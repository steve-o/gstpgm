/* vim:ts=8:sts=4:sw=4:noai:noexpandtab
 * 
 * PGM GStreamer-sink GObject interface (GstPgmSink)
 *
 * Copyright (c) 2008 Miru Limited.
 * Copyright (c) 2014 Tim Aerts.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <string.h>
#include <netinet/ip.h>
#include <pgm/packet.h>

#include "gstpgmsink.h"
#include "config.h"

enum
{
  PROP_0,
  PROP_NETWORK = 1,
  PROP_PORT,
  PROP_URI,
  PROP_UDP_ENCAP_PORT,
  PROP_MAX_TPDU,
  PROP_HOPS,
  PROP_TXW_SQNS,
  PROP_SPM_AMBIENT,
  PROP_IHB_MIN,
  PROP_IHB_MAX,
  PROP_LAST
};

/* supported media types of this pad */
static GstStaticPadTemplate gst_pgm_sink_sink_template =
  GST_STATIC_PAD_TEMPLATE ( "sink"
                          , GST_PAD_SINK
                          , GST_PAD_ALWAYS
                          , GST_STATIC_CAPS_ANY
                          );

static gboolean       gst_pgm_sink_set_uri (GstPgmSink*, const gchar*);
static void           gst_pgm_sink_uri_handler_init (gpointer, gpointer);
static GstFlowReturn  gst_pgm_sink_render (GstBaseSink*, GstBuffer*);
static gboolean       gst_pgm_client_sink_stop (GstBaseSink*);
static gboolean       gst_pgm_client_sink_start (GstBaseSink*);
static void           gst_pgm_sink_finalize (GObject*);
static void           gst_pgm_sink_set_property (GObject*, guint, const GValue*, GParamSpec*);
static void           gst_pgm_sink_get_property (GObject*, guint, GValue*, GParamSpec*);

G_DEFINE_TYPE (GstPgmSink, gst_pgm_sink, GST_TYPE_BASE_SINK)

static GstURIType gst_pgm_sink_uri_get_type (GType dummy) 
{
  return GST_URI_SRC;
}

static const gchar* const* gst_pgm_sink_uri_get_protocols (GType dummy)
{
  static const gchar* protocols[] = { "pgm", NULL };
  return protocols;
}

static gchar* gst_pgm_sink_uri_get_uri (GstURIHandler* i_handler)
{
  GstPgmSink* sink = GST_PGM_SINK(i_handler);
  return sink->uri;
}

static gboolean gst_pgm_sink_uri_set_uri (GstURIHandler* i_handler, const gchar* i_uri, GError** o_err)
{
  GstPgmSink* sink = GST_PGM_SINK(i_handler);
  return gst_pgm_sink_set_uri (sink, i_uri);
}

static void gst_pgm_sink_uri_handler_init (gpointer o_iface, gpointer dummy)
{
  GstURIHandlerInterface* iface = (GstURIHandlerInterface*)o_iface;
  iface->get_type      = GST_DEBUG_FUNCPTR(gst_pgm_sink_uri_get_type);
  iface->get_protocols = GST_DEBUG_FUNCPTR(gst_pgm_sink_uri_get_protocols);
  iface->get_uri       = GST_DEBUG_FUNCPTR(gst_pgm_sink_uri_get_uri);
  iface->set_uri       = GST_DEBUG_FUNCPTR(gst_pgm_sink_uri_set_uri);
}

static void gst_pgm_sink_update_uri (GstPgmSink* io_sink)
{
  g_free (io_sink->uri);
  io_sink->uri = g_strdup_printf ("pgm://%s:%i:%i", io_sink->network, io_sink->port, io_sink->udp_encap_port);
}

static gboolean gst_pgm_sink_set_uri (GstPgmSink* io_sink, const gchar* i_uri)
{
  gchar* protocol = gst_uri_get_protocol (i_uri);

  if (strncmp (protocol, "pgm", strlen("pgm")) != 0)  goto wrong_protocol;

  g_free (protocol);
  gchar* location = gst_uri_get_location (i_uri);

  if (location == NULL) return FALSE;

  gchar* port = strstr (location, ":");
  g_free (io_sink->network);

  if (port == NULL) 
  {
    io_sink->network = g_strdup (location);
    io_sink->port = PGM_PORT;
    io_sink->udp_encap_port = PGM_UDP_ENCAP_PORT;
  } 
  else 
  {
    io_sink->network = g_strndup (location, port - location);
    io_sink->port = atoi (port + 1);
    gchar* udp_encap_port = strstr (port + 1, ":");
    if (udp_encap_port == NULL) 
    {
      io_sink->udp_encap_port = PGM_UDP_ENCAP_PORT;
    } 
    else 
    {
      io_sink->udp_encap_port = atoi (udp_encap_port + 1);
    }
  }
  g_free (location);

  gst_pgm_sink_update_uri (io_sink);
  return TRUE;

wrong_protocol:
  g_free (protocol);

  GST_ELEMENT_ERROR ( io_sink
                    , RESOURCE
                    , READ
                    , (NULL)
                    , ("error parsing uri %s: wrong protocol (%s != udp)", i_uri, protocol)
                    );
  
  return FALSE;
}

static gpointer gst_pgm_sink_nak_thread (gpointer io_sink)
{
  GstPgmSink* sink = io_sink;
  pgm_msgv_t msgv;

  do 
  {
    pgm_recvmsg (sink->transport, &msgv, 0, NULL, NULL);
  } 
  while (!sink->nak_quit);

  return NULL;
}

static void gst_pgm_sink_base_init (gpointer klass)
{
}

static void gst_pgm_sink_class_init (GstPgmSinkClass* klass)
{
  GstBaseSinkClass* gstbasesink_class = (GstBaseSinkClass*)klass;
  gstbasesink_class->start   = GST_DEBUG_FUNCPTR(gst_pgm_client_sink_start);
  gstbasesink_class->stop    = GST_DEBUG_FUNCPTR(gst_pgm_client_sink_stop);
  gstbasesink_class->render  = GST_DEBUG_FUNCPTR(gst_pgm_sink_render);

  GObjectClass* gobjectClass = (GObjectClass*)klass;
  gobjectClass->finalize      = GST_DEBUG_FUNCPTR(gst_pgm_sink_finalize);
  gobjectClass->set_property  = GST_DEBUG_FUNCPTR(gst_pgm_sink_set_property);
  gobjectClass->get_property  = GST_DEBUG_FUNCPTR(gst_pgm_sink_get_property);

  GstElementClass* elementClass = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template
    ( elementClass 
    , gst_static_pad_template_get (&gst_pgm_sink_sink_template)
    );

  gst_element_class_set_static_metadata
    ( elementClass
    , "PGM Sink"
    , "Source/Network"
    , "Sends data from a GStreamer pipeline over PGM."
    , "Tim Aerts <jobs@timaerts.be>"
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_NETWORK
    , g_param_spec_string 
      ( "network"
      , "Network"
      , "Rendezvous style multicast network definition."
      , PGM_NETWORK
      , G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_PORT
    , g_param_spec_uint 
      ( "dport"
      , "DPORT"
      , "Data-destination port."
      , 0
      , /* minimum */ UINT16_MAX
      , /* maximum */ PGM_PORT
      , /* default */ G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_URI
    , g_param_spec_string 
      ( "uri"
      , "URI"
      , "URI in the form of pgm://adapter;receive-multicast-groups;send-multicast-group:destination-port:udp-encapsulation-port"
      , PGM_URI
      , G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    ); 

  g_object_class_install_property 
    ( gobjectClass
    , PROP_UDP_ENCAP_PORT
    , g_param_spec_uint 
      ( "udp-encap-port"
      , "UDP encapsulation port"
      , "UDP port for encapsulation of PGM protocol."
      , 0
      , /* minimum */ UINT16_MAX
      , /* maximum */ PGM_UDP_ENCAP_PORT
      , /* default */ G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    ); 

  g_object_class_install_property 
    ( gobjectClass
    , PROP_MAX_TPDU
    , g_param_spec_uint 
      ( "max-tpdu"
      , "Maximum TPDU"
      , "Largest supported Transport Protocol Data Unit."
      , (sizeof(struct iphdr) + sizeof(struct pgm_header))
      , /* minimum */ UINT16_MAX
      , /* maximum */ PGM_MAX_TPDU
      , /* default */ G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_HOPS
    , g_param_spec_uint 
      ( "hops"
      , "Hops"
      , "Multicast packet hop limit."
      , 1
      , /* minimum */ UINT8_MAX
      , /* maximum */ PGM_HOPS
      , /* default */ G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_TXW_SQNS
    , g_param_spec_uint 
      ( "txw-sqns"
      , "TXW_SQNS"
      , "Size of the transmit window in sequence numbers."
      , 1
      , /* minimum */ UINT16_MAX
      , /* maximum */ PGM_TXW_SQNS
      , /* default */ G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_SPM_AMBIENT
    , g_param_spec_uint 
      ( "spm-ambient"
      , "SPM ambient"
      , "Ambient SPM broadcast interval."
      , 1
      , /* minimum */ UINT_MAX
      , /* maximum */ PGM_SPM_AMBIENT
      , /* default */ G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_IHB_MIN
    , g_param_spec_uint 
      ( "ihb-min"
      , "IHB_MIN"
      , "Minimum SPM inter-heartbeat timer interval."
      , 1
      , /* minimum */ UINT_MAX
      , /* maximum */ PGM_IHB_MIN
      , /* default */ G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_IHB_MAX
    , g_param_spec_uint 
      ( "ihb-max"
      , "IHB_MAX"
      , "Maximum SPM inter-heartbeat timer interval."
      , 1
      , /* minimum */ UINT_MAX
      , /* maximum */ PGM_IHB_MAX
      , /* default */ G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );
}

static void gst_pgm_sink_init ( GstPgmSink* io_sink)
{
  io_sink->transport = NULL;

  io_sink->network         = g_strdup (PGM_NETWORK);
  io_sink->port            = PGM_PORT;
  io_sink->uri             = g_strdup (PGM_URI);
  io_sink->udp_encap_port  = PGM_UDP_ENCAP_PORT;
  io_sink->max_tpdu        = PGM_MAX_TPDU;
  io_sink->hops            = PGM_HOPS;
  io_sink->txw_sqns        = PGM_TXW_SQNS;
  io_sink->spm_ambient     = PGM_SPM_AMBIENT;
  io_sink->ihb_min         = PGM_IHB_MIN;
  io_sink->ihb_max         = PGM_IHB_MAX;
}

static void gst_pgm_sink_finalize ( GObject* io_obj)
{
  GstPgmSink* sink = GST_PGM_SINK (io_obj);

  g_free (sink->network);
  g_free (sink->uri);

  G_OBJECT_CLASS(gst_pgm_sink_parent_class)->finalize(io_obj);
}

static void gst_pgm_sink_set_property (GObject* io_obj, guint i_propId, const GValue* i_value, GParamSpec* pspec)
{
  GstPgmSink* sink = GST_PGM_SINK (io_obj);

  switch (i_propId) 
  {
  case PROP_NETWORK:
    g_free (sink->network);
    if (g_value_get_string (i_value) == NULL)
    {
      sink->network = g_strdup (PGM_NETWORK);
    }
    else
    {
      sink->network = g_value_dup_string (i_value);
    }
    gst_pgm_sink_update_uri (sink);
    break;

  case PROP_PORT:
    sink->port = g_value_get_uint (i_value);
    gst_pgm_sink_update_uri (sink);
    break;

  case PROP_UDP_ENCAP_PORT:
    sink->udp_encap_port = g_value_get_uint (i_value);
    gst_pgm_sink_update_uri (sink);
    break;

  case PROP_URI:
    gst_pgm_sink_set_uri (sink, g_value_get_string (i_value));
    break;

  case PROP_MAX_TPDU:
    sink->max_tpdu = g_value_get_uint (i_value);
    break;

  case PROP_HOPS:
    sink->hops = g_value_get_uint (i_value);
    break;

  case PROP_TXW_SQNS:
    sink->txw_sqns = g_value_get_uint (i_value);
    break;

  case PROP_SPM_AMBIENT:
    sink->spm_ambient = g_value_get_uint (i_value);
    break;

  case PROP_IHB_MIN:
    sink->ihb_min = g_value_get_uint (i_value);
    break;

  case PROP_IHB_MAX:
    sink->ihb_max = g_value_get_uint (i_value);
    break;
  }
}

static void gst_pgm_sink_get_property (GObject* io_obj, guint i_propId, GValue* o_value, GParamSpec* pspec)
{
  GstPgmSink* sink = GST_PGM_SINK (io_obj);

  switch (i_propId) 
  {
  case PROP_NETWORK:
    g_value_set_string (o_value, sink->network);
    break;
  case PROP_PORT:
    g_value_set_uint (o_value, sink->port);
    break;
  case PROP_UDP_ENCAP_PORT:
    g_value_set_uint (o_value, sink->udp_encap_port);
    break;
  case PROP_URI:
    g_value_set_string (o_value, sink->uri);
    break;
  case PROP_MAX_TPDU:
    g_value_set_uint (o_value, sink->max_tpdu);
    break;
  case PROP_HOPS:
    g_value_set_uint (o_value, sink->hops);
    break;
  case PROP_TXW_SQNS:
    g_value_set_uint (o_value, sink->txw_sqns);
    break;
  case PROP_SPM_AMBIENT:
    g_value_set_uint (o_value, sink->spm_ambient);
    break;
  case PROP_IHB_MIN:
    g_value_set_uint (o_value, sink->ihb_min);
    break;
  case PROP_IHB_MAX:
    g_value_set_uint (o_value, sink->ihb_max);
    break;
  }
}

/* GstBaseSinkClass::render
 *
 * As a GStreamer source, create data, so recv on PGM transport.
 */
static GstFlowReturn gst_pgm_sink_render (GstBaseSink* io_basesink, GstBuffer* i_buffer)
{
  GstPgmSink* sink = GST_PGM_SINK (io_basesink);

  GstMapInfo map;
  gst_buffer_map (i_buffer, &map, (GstMapFlags)GST_MAP_READ);
  const PGMIOStatus status = pgm_send ( sink->transport
                                      , map.data
                                      , map.size
                                      , NULL
                                      );
  gst_buffer_unmap (i_buffer, &map);                                      

  if (PGM_IO_STATUS_NORMAL != status) return GST_FLOW_ERROR;

  return GST_FLOW_OK;
}

/* create PGM transport 
 */
static gboolean gst_pgm_client_sink_start (GstBaseSink* basesink)
{
  GstPgmSink* sink = GST_PGM_SINK (basesink);

  struct pgm_transport_info_t* res = NULL;
  GError* err = NULL;

  if (!pgm_if_get_transport_info (sink->network, NULL, &res, &err)) 
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("Parsing network parameter: %s", err->message));
    g_error_free (err);
    return FALSE;
  }

  if (!pgm_gsi_create_from_hostname (&res->ti_gsi, &err)) 
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("Creating GSI: %s", err->message));
    g_error_free (err);
    pgm_if_free_transport_info (res);
    return FALSE;
  }

  res->ti_udp_encap_ucast_port = sink->udp_encap_port;
  res->ti_udp_encap_mcast_port = sink->udp_encap_port;

  if (!pgm_transport_create (&sink->transport, res, &err)) 
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("Creating transport: %s", err->message));
    pgm_if_free_transport_info (res);
    return FALSE;
  }

  pgm_if_free_transport_info (res);

  if (!pgm_transport_set_send_only (sink->transport, TRUE)) 
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("cannot set send-only mode"));
    goto destroy_transport;
  }

  if (!pgm_transport_set_max_tpdu (sink->transport, sink->max_tpdu)) 
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("cannot set maximum TPDU size"));
    goto destroy_transport;
  }
  
  if (!pgm_transport_set_multicast_loop (sink->transport, TRUE)) 
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("cannot set multicast loop"));
    goto destroy_transport;
  }
  
  if (!pgm_transport_set_hops (sink->transport, sink->hops)) 
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("cannot set IP hop limit"));
    goto destroy_transport;
  }
  
  if (!pgm_transport_set_txw_sqns (sink->transport, sink->txw_sqns)) 
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("cannot set TXW_SQNS"));
    goto destroy_transport;
  }
  
  if (!pgm_transport_set_ambient_spm (sink->transport, sink->spm_ambient)) 
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("cannot set SPM ambient interval"));
    goto destroy_transport;
  }

  {
    guint array_len = 0;
    for (guint i = sink->ihb_min; i < sink->ihb_max; i *= 2) 
    {
      array_len++;
    }
    guint spm_heartbeat[array_len];
    for (guint i = 0, j = sink->ihb_min; j < sink->ihb_max; j *= 2) 
    {
      spm_heartbeat[i++] = j;
    }
  
    if (!pgm_transport_set_heartbeat_spm (sink->transport, spm_heartbeat, array_len)) 
    {
      GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("cannot set SPM heartbeat intervals"));
      goto destroy_transport;
    }
  }
  
  if (!pgm_transport_bind (sink->transport, &err)) 
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("Binding transport: %s", err->message));
    g_error_free (err);
    goto destroy_transport;
  }

  /* create NAK thread */
  sink->nak_thread = g_thread_create_full 
    ( gst_pgm_sink_nak_thread
    , sink
    , 0     // closure
    , TRUE  // stack size
    , TRUE  // joinable
    , G_THREAD_PRIORITY_HIGH  // native thread
    , &err  // priority
    );

  if (sink->nak_thread == NULL) 
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("Creating NAK processing thread:/%s", err->message));
    g_error_free (err);
    goto destroy_transport;
  }

  return TRUE;

destroy_transport:

  pgm_transport_destroy (sink->transport, TRUE);
  sink->transport = NULL;
  return FALSE;
}

static gboolean gst_pgm_client_sink_stop (GstBaseSink* io_basesink)
{
  GstPgmSink* sink = GST_PGM_SINK (io_basesink);

  /* stop nak thread */
  if (sink->nak_thread) 
  {
    sink->nak_quit = TRUE;
    g_thread_join (sink->nak_thread);
  }

  GST_DEBUG_OBJECT (sink, "destroying transport");
  
  if (sink->transport) 
  {
    pgm_transport_destroy (sink->transport, TRUE);
    sink->transport = NULL;
  }

  return TRUE;
}

