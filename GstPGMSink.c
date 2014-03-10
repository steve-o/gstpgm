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

#include "GstPGMSink.h"
#include "GstPGMConfig.h"

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

  {

  g_free (protocol);
  gchar* location = gst_uri_get_location (i_uri);

  if (location == NULL) return FALSE;

  gchar* port = strstr (location, ":");
  g_free (io_sink->network);

  if (port == NULL) 
  {
    io_sink->network = g_strdup (location);
    io_sink->port = PGM_DEFAULT_PORT;
    io_sink->udp_encap_port = PGM_DEFAULT_UDP_ENCAP_PORT;
  } 
  else 
  {
    io_sink->network = g_strndup (location, port - location);
    io_sink->port = atoi (port + 1);
    gchar* udp_encap_port = strstr (port + 1, ":");
    if (udp_encap_port == NULL) 
    {
      io_sink->udp_encap_port = PGM_DEFAULT_UDP_ENCAP_PORT;
    } 
    else 
    {
      io_sink->udp_encap_port = atoi (udp_encap_port + 1);
    }
  }
  g_free (location);

  gst_pgm_sink_update_uri (io_sink);
  return TRUE;

  }

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
  GstPgmSink* sink = (GstPgmSink*) io_sink;
  struct pgm_msgv_t msgv;

  int result;
  do 
  {
    result = pgm_recvmsg (sink->sock, &msgv, 0, NULL, NULL);
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
      , PGM_DEFAULT_NETWORK
      , (GParamFlags) G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_PORT
    , g_param_spec_uint 
      ( "dport"
      , "DPORT"
      , "Data-destination port."
      , 0 // minimum
      , UINT16_MAX
      , PGM_DEFAULT_PORT
      , (GParamFlags) G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_URI
    , g_param_spec_string 
      ( "uri"
      , "URI"
      , "URI in the form of pgm://adapter;receive-multicast-groups;send-multicast-group:destination-port:udp-encapsulation-port"
      , PGM_DEFAULT_URI
      , (GParamFlags) G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    ); 

  g_object_class_install_property 
    ( gobjectClass
    , PROP_UDP_ENCAP_PORT
    , g_param_spec_uint 
      ( "udp-encap-port"
      , "UDP encapsulation port"
      , "UDP port for encapsulation of PGM protocol."
      , 0 // minimum
      , UINT16_MAX
      , PGM_DEFAULT_UDP_ENCAP_PORT
      , (GParamFlags) G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
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
      , UINT16_MAX
      , PGM_DEFAULT_MAX_TPDU
      , (GParamFlags) G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_HOPS
    , g_param_spec_uint 
      ( "hops"
      , "Hops"
      , "Multicast packet hop limit."
      , 1 // minimum
      , UINT8_MAX
      , PGM_DEFAULT_HOPS
      , (GParamFlags) G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_TXW_SQNS
    , g_param_spec_uint 
      ( "txw-sqns"
      , "TXW_SQNS"
      , "Size of the transmit window in sequence numbers."
      , 1 // minimum
      , UINT16_MAX
      , PGM_DEFAULT_TXW_SQNS
      , (GParamFlags) G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_SPM_AMBIENT
    , g_param_spec_uint 
      ( "spm-ambient"
      , "SPM ambient"
      , "Ambient SPM broadcast interval."
      , 1 // minimum
      , UINT_MAX
      , PGM_DEFAULT_SPM_AMBIENT
      , (GParamFlags) G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_IHB_MIN
    , g_param_spec_uint 
      ( "ihb-min"
      , "IHB_MIN"
      , "Minimum SPM inter-heartbeat timer interval."
      , 1 // minimum
      , UINT_MAX
      , PGM_DEFAULT_IHB_MIN
      , (GParamFlags) G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_IHB_MAX
    , g_param_spec_uint 
      ( "ihb-max"
      , "IHB_MAX"
      , "Maximum SPM inter-heartbeat timer interval."
      , 1 // minimum
      , UINT_MAX
      , PGM_DEFAULT_IHB_MAX
      , (GParamFlags) G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );
}

static void gst_pgm_sink_init ( GstPgmSink* io_sink)
{
  io_sink->sock = NULL;

  io_sink->network         = g_strdup (PGM_DEFAULT_NETWORK);
  io_sink->port            = PGM_DEFAULT_PORT;
  io_sink->uri             = g_strdup (PGM_DEFAULT_URI);
  io_sink->udp_encap_port  = PGM_DEFAULT_UDP_ENCAP_PORT;
  io_sink->max_tpdu        = PGM_DEFAULT_MAX_TPDU;
  io_sink->hops            = PGM_DEFAULT_HOPS;
  io_sink->txw_sqns        = PGM_DEFAULT_TXW_SQNS;
  io_sink->spm_ambient     = PGM_DEFAULT_SPM_AMBIENT;
  io_sink->ihb_min         = PGM_DEFAULT_IHB_MIN;
  io_sink->ihb_max         = PGM_DEFAULT_IHB_MAX;
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
      sink->network = g_strdup (PGM_DEFAULT_NETWORK);
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
  size_t written = 0u;
  const int status = pgm_send ( sink->sock
                              , map.data
                              , map.size
                              , &written
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

  const int valTrue  = 1;
  const int valFalse = 0;

	sa_family_t sa_family = AF_UNSPEC;

  struct pgm_addrinfo_t* res = NULL;
  pgm_error_t* pErr = NULL;
  GError* gErr = NULL;

	if (!pgm_init (&pErr)) {
		g_error ("Unable to start PGM engine: %s", pErr->message);
		pgm_error_free (pErr);
		goto destroy_transport;
	}
  if (!pgm_getaddrinfo (sink->network, NULL, &res, &pErr)) 
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("Parsing network parameter: %s", pErr->message));
    pgm_error_free (pErr);
    return FALSE;
  }

	sa_family = res->ai_send_addrs[0].gsr_group.ss_family;

  if (!pgm_socket (&sink->sock, sa_family, SOCK_SEQPACKET, IPPROTO_UDP, &pErr)) 
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("Creating transport: %s", pErr->message));
    pgm_freeaddrinfo (res);
    return FALSE;
  }

  if (!pgm_setsockopt (sink->sock, IPPROTO_PGM, PGM_IP_ROUTER_ALERT, &valFalse, sizeof(valFalse))) 
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("cannot unset the router assist"));
    goto destroy_transport;
  }

  if (!pgm_setsockopt (sink->sock, IPPROTO_PGM, PGM_SEND_ONLY, &valTrue, sizeof(valTrue))) 
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("cannot set send-only mode"));
    goto destroy_transport;
  }

  if (!pgm_setsockopt (sink->sock, IPPROTO_PGM, PGM_MTU, &sink->max_tpdu, sizeof(sink->max_tpdu))) 
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("cannot set maximum TPDU size"));
    goto destroy_transport;
  }
  
  if (!pgm_setsockopt (sink->sock, IPPROTO_PGM, PGM_MULTICAST_LOOP, &valTrue, sizeof(valTrue)))
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("cannot set multicast loop"));
    goto destroy_transport;
  }
  
  if (!pgm_setsockopt (sink->sock, IPPROTO_PGM, PGM_MULTICAST_HOPS, &sink->hops, sizeof(sink->hops))) 
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("cannot set IP hop limit"));
    goto destroy_transport;
  }
  
  if (!pgm_setsockopt (sink->sock, IPPROTO_PGM, PGM_TXW_SQNS, &sink->txw_sqns, sizeof(sink->txw_sqns))) 
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("cannot set TXW_SQNS"));
    goto destroy_transport;
  }
  
  if (!pgm_setsockopt (sink->sock, IPPROTO_PGM, PGM_AMBIENT_SPM, &sink->spm_ambient, sizeof(sink->spm_ambient))) 
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("cannot set SPM ambient interval"));
    goto destroy_transport;
  }

  if (!pgm_setsockopt (sink->sock, IPPROTO_PGM, PGM_UDP_ENCAP_UCAST_PORT, &sink->udp_encap_port, sizeof(sink->udp_encap_port)))
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("cannot set UDP encap ucast port"));
    goto destroy_transport;
  }
  
  if (!pgm_setsockopt (sink->sock, IPPROTO_PGM, PGM_UDP_ENCAP_MCAST_PORT, &sink->udp_encap_port, sizeof(sink->udp_encap_port)))
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("cannot set UDP encap mcast port"));
    goto destroy_transport;
  }

  if (!pgm_setsockopt (sink->sock, IPPROTO_PGM, PGM_NOBLOCK, &valFalse, sizeof(valFalse)))
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("cannot unset no-block"));
    goto destroy_transport;
  }

  {
    const int heartbeat_spm[] = { pgm_msecs (100)
                                , pgm_msecs (100)
                                , pgm_msecs (100)
                                , pgm_msecs (100)
                                , pgm_msecs (1300)
                                , pgm_secs  (7)
                                , pgm_secs  (16)
                                , pgm_secs  (25)
                                , pgm_secs  (30)
                                };

    if (!pgm_setsockopt (sink->sock, IPPROTO_PGM, PGM_HEARTBEAT_SPM, &heartbeat_spm, sizeof(heartbeat_spm))) 
    {
      GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("cannot set SPM heartbeat intervals"));
      goto destroy_transport;
    }
  }

  struct pgm_sockaddr_t addr;
  memset (&addr, '\0', sizeof(addr));
  addr.sa_port = sink->port;
  addr.sa_addr.sport = DEFAULT_DATA_SOURCE_PORT;

  if (!pgm_gsi_create_from_hostname (&addr.sa_addr.gsi, &pErr))
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("Creating GSI: %s", pErr->message));
    pgm_error_free (pErr);
    pgm_freeaddrinfo (res);
    return FALSE;
  }

  struct pgm_interface_req_t ifReq;
  memset (&ifReq, '\0', sizeof(ifReq));
  ifReq.ir_interface = res->ai_recv_addrs[0].gsr_interface;
  ifReq.ir_scope_id  = 0;

	if (AF_INET6 == sa_family) 
  {
		struct sockaddr_in6 sa6;
		memcpy (&sa6, &res->ai_recv_addrs[0].gsr_group, sizeof(sa6));
		ifReq.ir_scope_id = sa6.sin6_scope_id;
	}

  if (!pgm_bind3 ( sink->sock
                 , &addr, sizeof(addr)
                 , &ifReq, sizeof(ifReq)  // tx interface
                 , &ifReq, sizeof(ifReq)  // rx interface
                 , &pErr
                 )
     )
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("Binding transport: %s", pErr->message));
    pgm_error_free (pErr);
    goto destroy_transport;
  }

	for (unsigned i = 0; i < res->ai_recv_addrs_len; ++i)
  {
		pgm_setsockopt (sink->sock, IPPROTO_PGM, PGM_JOIN_GROUP, &res->ai_recv_addrs[i], sizeof(struct group_req));
  }
	pgm_setsockopt (sink->sock, IPPROTO_PGM, PGM_SEND_GROUP, &res->ai_send_addrs[0], sizeof(struct group_req));
	pgm_freeaddrinfo (res);

	if (!pgm_connect (sink->sock, &pErr))
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("Connecting socket: %s", pErr->message));
    pgm_error_free (pErr);
    goto destroy_transport;
	}


  /* create NAK thread */
  sink->nak_thread = g_thread_new 
    ( "nak_thread"
    , gst_pgm_sink_nak_thread
    , sink
    );

  if (sink->nak_thread == NULL) 
  {
    GST_ELEMENT_ERROR (sink, RESOURCE, OPEN_WRITE, (NULL), ("Creating NAK processing thread:/%s", gErr->message));
    g_error_free (gErr);
    goto destroy_transport;
  }
  return TRUE;

destroy_transport:
  pgm_close (sink->sock, TRUE);
  sink->sock = NULL;
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
  
  if (sink->sock) 
  {
    pgm_close (sink->sock, TRUE);
    sink->sock = NULL;
  }

  return TRUE;
}

