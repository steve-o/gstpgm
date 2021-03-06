/* vim:ts=8:sts=4:sw=4:noai:noexpandtab
 * 
 * PGM GStreamer-source GObject interface (GstPgmSrc)
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
#include <stdio.h>  // for debug puts() & printf() only
#include <netinet/ip.h>
#include <pgm/packet.h>

#include "GstPGMSrc.h"
#include "GstPGMConfig.h"

enum
{
  PROP_0,
  PROP_NETWORK = 1,
  PROP_PORT,
  PROP_URI,
  PROP_CAPS,
  PROP_UDP_ENCAP_PORT,
  PROP_MAX_TPDU,
  PROP_HOPS,
  PROP_RXW_SQNS,
  PROP_PEER_EXPIRY,
  PROP_SPMR_EXPIRY,
  PROP_NAK_BO_IVL,
  PROP_NAK_RPT_IVL,
  PROP_NAK_RDATA_IVL,
  PROP_NAK_DATA_RETRIES,
  PROP_NAK_NCF_RETRIES,
  PROP_LAST
};

static GstStaticPadTemplate gst_pgm_src_src_template =
  GST_STATIC_PAD_TEMPLATE ( "src"
                          , GST_PAD_SRC
                          , GST_PAD_ALWAYS
                          , GST_STATIC_CAPS_ANY
                          );

static void          gst_pgm_src_finalize (GObject*);
static void          gst_pgm_src_set_property (GObject*, guint, const GValue*, GParamSpec*);
static void          gst_pgm_src_get_property (GObject*, guint, GValue*, GParamSpec*);
static GstCaps*      gst_pgm_src_get_caps (GstBaseSrc*, GstCaps*);
static gboolean      gst_pgm_src_set_uri (GstPgmSrc*, const gchar*);
static void          gst_pgm_src_uri_handler_init (gpointer, gpointer);
static GstFlowReturn gst_pgm_src_create (GstPushSrc*, GstBuffer**);
static gboolean      gst_pgm_client_src_stop (GstBaseSrc*);
static gboolean      gst_pgm_client_src_start (GstBaseSrc*);

G_DEFINE_TYPE (GstPgmSrc, gst_pgm_src, GST_TYPE_PUSH_SRC)

static GstURIType gst_pgm_src_uri_get_type (GType dummy)
{
  return GST_URI_SRC;
}

static const gchar* const* gst_pgm_src_uri_get_protocols (GType dummy)
{
  static const gchar* protocols[] = { "pgm", NULL };
  return protocols;
}

static gchar* gst_pgm_src_uri_get_uri (GstURIHandler* i_handler)
{
  GstPgmSrc* src = GST_PGM_SRC(i_handler);
  return src->uri;
}

static gboolean gst_pgm_src_uri_set_uri (GstURIHandler* i_handler, const gchar* i_uri, GError** o_err)
{
  GstPgmSrc* src = GST_PGM_SRC (i_handler);
  return gst_pgm_src_set_uri (src, i_uri);
}

static void gst_pgm_src_uri_handler_init (gpointer o_iface, gpointer dummy)
{
  GstURIHandlerInterface* iface = (GstURIHandlerInterface*)o_iface;
  iface->get_type       = GST_DEBUG_FUNCPTR(gst_pgm_src_uri_get_type);
  iface->get_protocols  = GST_DEBUG_FUNCPTR(gst_pgm_src_uri_get_protocols);
  iface->get_uri        = GST_DEBUG_FUNCPTR(gst_pgm_src_uri_get_uri);
  iface->set_uri        = GST_DEBUG_FUNCPTR(gst_pgm_src_uri_set_uri);
}

static void gst_pgm_src_update_uri (GstPgmSrc* io_src)
{
  g_free (io_src->uri);
  io_src->uri = g_strdup_printf ("pgm://%s:%i:%i", io_src->network, io_src->port, io_src->udp_encap_port);
}

static gboolean gst_pgm_src_set_uri (GstPgmSrc* io_src, const gchar* i_uri)
{
  if (!gst_uri_is_valid(i_uri)) return FALSE;

  gchar* protocol = gst_uri_get_protocol (i_uri);
  if (strncmp (protocol, "pgm", strlen("pgm")) != 0) goto wrong_protocol;

  {

  g_free (protocol);
  gchar* location = gst_uri_get_location (i_uri);

  if (location == NULL) return FALSE;

  gchar* port = strstr (location, ":");
  g_free (io_src->network);

  if (port == NULL) 
  {
    io_src->network  = g_strdup (location);
    io_src->port = PGM_DEFAULT_PORT;
    io_src->udp_encap_port = PGM_DEFAULT_UDP_ENCAP_PORT;
  } 
  else 
  {
    io_src->network  = g_strndup (location, port - location);
    io_src->port = atoi (port + 1);
    gchar* udp_encap_port = strstr (port + 1, ":");
    if (udp_encap_port == NULL) 
    {
      io_src->udp_encap_port = PGM_DEFAULT_UDP_ENCAP_PORT;
    } 
    else 
    {
      io_src->udp_encap_port = atoi (udp_encap_port + 1);
    }
  }
  g_free (location);
  
  gst_pgm_src_update_uri (io_src);
  return TRUE;

  }

wrong_protocol:
  g_free (protocol);
  GST_ELEMENT_ERROR ( io_src
                    , RESOURCE
                    , READ
                    , (NULL)
                    , ("error parsing uri %s: wrong protocol (%s != udp)", i_uri, protocol)
                    );
  return FALSE;
}

static void gst_pgm_src_base_init (gpointer klass)
{
}

static void gst_pgm_src_class_init (GstPgmSrcClass* klass)
{
  GstBaseSrcClass* gstbasesrcClass = (GstBaseSrcClass*)klass;
  gstbasesrcClass->start     = GST_DEBUG_FUNCPTR(gst_pgm_client_src_start);
  gstbasesrcClass->stop      = GST_DEBUG_FUNCPTR(gst_pgm_client_src_stop);
  gstbasesrcClass->get_caps  = GST_DEBUG_FUNCPTR(gst_pgm_src_get_caps);

  GstPushSrcClass* gstpushsrcClass = (GstPushSrcClass*)klass;
  gstpushsrcClass->create  = GST_DEBUG_FUNCPTR(gst_pgm_src_create);

  GObjectClass* gobjectClass = (GObjectClass*)klass;
  gobjectClass->finalize     = GST_DEBUG_FUNCPTR(gst_pgm_src_finalize);
  gobjectClass->set_property = GST_DEBUG_FUNCPTR(gst_pgm_src_set_property);
  gobjectClass->get_property = GST_DEBUG_FUNCPTR(gst_pgm_src_get_property);

  GstElementClass* elementClass = GST_ELEMENT_CLASS (klass);

  gst_element_class_add_pad_template 
    ( elementClass
    , gst_static_pad_template_get (&gst_pgm_src_src_template)
    );

  gst_element_class_set_static_metadata
    ( elementClass
    , "PGM Source"
    , "Source/Network"
    , "Feeds a GStreamer pipeline with content streamed from PGM."
    , "Tim Aerts <jobs@timaerts.be>"
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_NETWORK
    , g_param_spec_string
        ( "network"
        , "Network"
        , "Rendezvous-style multicast network definition."
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
    , PROP_CAPS
    , g_param_spec_boxed 
        ( "caps"
        , "CAPS"
        , "The caps of the source pad"
        , GST_TYPE_CAPS
        , (GParamFlags) G_PARAM_READWRITE
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
      , (sizeof (struct iphdr) + sizeof (struct pgm_header)) // minimum
      , UINT16_MAX
      , PGM_DEFAULT_MAX_TPDU
      , (GParamFlags) G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_HOPS
    , g_param_spec_int 
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
    , PROP_RXW_SQNS
    , g_param_spec_uint 
      ( "rxw-sqns"
      , "RXW_SQNS"
      , "Size of the receive window in sequence numbers."
      , 1 // minimum
      , UINT16_MAX
      , PGM_DEFAULT_RXW_SQNS
      , (GParamFlags) G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_PEER_EXPIRY
    , g_param_spec_uint 
      ( "peer-expiry"
      , "Peer expiry"
      , "Expiration timeout for peers."
      , 1 // minimum
      , UINT_MAX
      , PGM_DEFAULT_PEER_EXPIRY
      , (GParamFlags) G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_SPMR_EXPIRY
    , g_param_spec_uint 
      ( "spmr-expiry"
      , "SPMR expiry"
      , "Maximum back-off range before sending a SPM-Request."
      , 1 // minimum
      , UINT_MAX
      , PGM_DEFAULT_SPMR_EXPIRY
      , (GParamFlags) G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_NAK_BO_IVL
    , g_param_spec_uint 
      ( "nak-bo-ivl"
      , "NAK_BO_IVL"
      , "Back-off interval before sending a NAK."
      , 1 // minimum
      , UINT_MAX
      , PGM_DEFAULT_NAK_BO_IVL
      , (GParamFlags) G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_NAK_RPT_IVL
    , g_param_spec_uint 
      ( "nak-rpt-ivl"
      , "NAK_RPT_IVL"
      , "Repeat interval before re-sending a NAK."
      , 1 // minimum
      , UINT_MAX
      , PGM_DEFAULT_NAK_RPT_IVL
      , (GParamFlags) G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_NAK_RDATA_IVL
    , g_param_spec_uint 
      ( "nak-data-ivl"
      , "NAK_RDATA_IVL"
      , "Timeout waiting for data."
      , 1 // minimum
      , UINT_MAX
      , PGM_DEFAULT_NAK_RDATA_IVL
      , (GParamFlags) G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_NAK_DATA_RETRIES
    , g_param_spec_uint 
      ( "nak-data-retries"
      , "NAK_DATA_RETRIES"
      , "Maximum number of retries."
      , 1 // minimum
      , UINT_MAX
      , PGM_DEFAULT_NAK_DATA_RETRIES
      , (GParamFlags) G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );

  g_object_class_install_property 
    ( gobjectClass
    , PROP_NAK_NCF_RETRIES
    , g_param_spec_uint 
      ( "nak-ncf-retries"
      , "NAK_NCF_RETRIES"
      , "Maximum number of retries."
      , 1 // minimum
      , UINT_MAX
      , PGM_DEFAULT_NAK_NCF_RETRIES
      , (GParamFlags) G_PARAM_READWRITE /*| G_PARAM_CONSTRUCT_ONLY*/
      )
    );
}

static void gst_pgm_src_init (GstPgmSrc* io_src)
{
    io_src->sock = NULL;

    io_src->network          = g_strdup (PGM_DEFAULT_NETWORK);
    io_src->port             = PGM_DEFAULT_PORT;
    io_src->uri              = g_strdup (PGM_DEFAULT_URI);
    io_src->udp_encap_port   = PGM_DEFAULT_UDP_ENCAP_PORT;
    io_src->max_tpdu         = PGM_DEFAULT_MAX_TPDU;
    io_src->hops             = PGM_DEFAULT_HOPS;
    io_src->rxw_sqns         = PGM_DEFAULT_RXW_SQNS;
    io_src->peer_expiry      = PGM_DEFAULT_PEER_EXPIRY;
    io_src->spmr_expiry      = PGM_DEFAULT_SPMR_EXPIRY;
    io_src->nak_bo_ivl       = PGM_DEFAULT_NAK_BO_IVL;
    io_src->nak_rpt_ivl      = PGM_DEFAULT_NAK_RPT_IVL;
    io_src->nak_rdata_ivl    = PGM_DEFAULT_NAK_RDATA_IVL;
    io_src->nak_data_retries = PGM_DEFAULT_NAK_DATA_RETRIES;
    io_src->nak_ncf_retries  = PGM_DEFAULT_NAK_NCF_RETRIES;

/* ensure source provides live, time based output, with timestamps */
    gst_base_src_set_live (GST_BASE_SRC (io_src), TRUE);
    gst_base_src_set_format (GST_BASE_SRC (io_src), GST_FORMAT_TIME);
    gst_base_src_set_do_timestamp (GST_BASE_SRC (io_src), TRUE);
}

static void gst_pgm_src_finalize (GObject* io_obj)
{
  GstPgmSrc* src = GST_PGM_SRC(io_obj);

  if (src->caps) 
  {
    gst_caps_unref (src->caps);
  }

  g_free (src->network);
  g_free (src->uri);

  G_OBJECT_CLASS(gst_pgm_src_parent_class)->finalize(io_obj);
}

static GstCaps* gst_pgm_src_get_caps (GstBaseSrc* i_basesrc, GstCaps* i_filter)
{
  GstPgmSrc* src = GST_PGM_SRC(i_basesrc);

  if (src->caps == NULL)
  {
    return gst_caps_new_any();
  }

  return gst_caps_ref(src->caps);
}

static void gst_pgm_src_set_property (GObject* io_obj, guint i_propId, const GValue* i_value, GParamSpec* i_pspec)
{
  GstPgmSrc* src = GST_PGM_SRC (io_obj);

  switch (i_propId) 
  {

  case PROP_NETWORK:
    g_free (src->network);
    if (g_value_get_string (i_value) == NULL)
    {
      src->network = g_strdup (PGM_DEFAULT_NETWORK);
    }
    else
    {
      src->network = g_value_dup_string (i_value);
    }
    gst_pgm_src_update_uri (src);
    break;

  case PROP_PORT:
    src->port = g_value_get_uint (i_value);
    gst_pgm_src_update_uri (src);
    break;

  case PROP_UDP_ENCAP_PORT:
    src->udp_encap_port = g_value_get_uint (i_value);
    gst_pgm_src_update_uri (src);
    break;
  
  case PROP_URI:
    gst_pgm_src_set_uri (src, g_value_get_string (i_value));
    break;
  
  case PROP_CAPS:
  {
    const GstCaps* new_caps_value = gst_value_get_caps (i_value);
    GstCaps* new_caps = new_caps_value ? gst_caps_copy (new_caps_value) : gst_caps_new_any();
    GstCaps* old_caps = src->caps;
    src->caps = new_caps; 

    if (old_caps) 
    {
      gst_caps_unref (old_caps);
    }
    
    gst_pad_set_caps (GST_BASE_SRC (src)->srcpad, new_caps);
    break;
  }
  
  case PROP_MAX_TPDU:
    src->max_tpdu = g_value_get_uint (i_value);
    break;
  
  case PROP_HOPS:
    src->hops = g_value_get_uint (i_value);
    break;
  
  case PROP_RXW_SQNS:
    src->rxw_sqns = g_value_get_uint (i_value);
    break;
  
  case PROP_PEER_EXPIRY:
    src->peer_expiry = g_value_get_uint (i_value);
    break;
  
  case PROP_SPMR_EXPIRY:
    src->spmr_expiry = g_value_get_uint (i_value);
    break;
  
  case PROP_NAK_BO_IVL:
    src->nak_bo_ivl = g_value_get_uint (i_value);
    break;
  
  case PROP_NAK_RPT_IVL:
    src->nak_rpt_ivl = g_value_get_uint (i_value);
    break;
  
  case PROP_NAK_RDATA_IVL:
    src->nak_rdata_ivl = g_value_get_uint (i_value);
    break;
  
  case PROP_NAK_DATA_RETRIES:
    src->nak_data_retries = g_value_get_uint (i_value);
    break;
  
  case PROP_NAK_NCF_RETRIES:
    src->nak_ncf_retries = g_value_get_uint (i_value);
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (io_obj, i_propId, i_pspec);
    break;
  }
}

static void gst_pgm_src_get_property (GObject* io_obj, guint i_propId, GValue* o_value, GParamSpec* i_pspec)
{
  GstPgmSrc* src = GST_PGM_SRC (io_obj);

  switch (i_propId) 
  {
  case PROP_NETWORK:
    g_value_set_string (o_value, src->network);
    break;
  case PROP_PORT:
    g_value_set_uint (o_value, src->port);
    break;
  case PROP_UDP_ENCAP_PORT:
    g_value_set_uint (o_value, src->udp_encap_port);
    break;
  case PROP_URI:
    g_value_set_string (o_value, src->uri);
    break;
  case PROP_CAPS:
    gst_value_set_caps (o_value, src->caps);
    break;
  case PROP_MAX_TPDU:
    g_value_set_uint (o_value, src->max_tpdu);
    break;
  case PROP_HOPS:
    g_value_set_uint (o_value, src->hops);
    break;
  case PROP_RXW_SQNS:
    g_value_set_uint (o_value, src->rxw_sqns);
    break;
  case PROP_PEER_EXPIRY:
    g_value_set_uint (o_value, src->peer_expiry);
    break;
  case PROP_SPMR_EXPIRY:
    g_value_set_uint (o_value, src->spmr_expiry);
    break;
  case PROP_NAK_BO_IVL:
    g_value_set_uint (o_value, src->nak_bo_ivl);
    break;
  case PROP_NAK_RPT_IVL:
    g_value_set_uint (o_value, src->nak_rpt_ivl);
    break;
  case PROP_NAK_RDATA_IVL:
    g_value_set_uint (o_value, src->nak_rdata_ivl);
    break;
  case PROP_NAK_DATA_RETRIES:
    g_value_set_uint (o_value, src->nak_data_retries);
    break;
  case PROP_NAK_NCF_RETRIES:
    g_value_set_uint (o_value, src->nak_ncf_retries);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID (io_obj, i_propId, i_pspec);
    break;
  }
}

/* GstPushSrcClass::create
 *
 * As a GStreamer source, create data, so recv on PGM transport.
 */
static GstFlowReturn gst_pgm_src_create ( GstPushSrc* pushsrc, GstBuffer** buffer)
{
  GstPgmSrc* src = GST_PGM_SRC(pushsrc);

  /* read in waiting data */
  struct pgm_msgv_t msgv;
  size_t len;
  struct pgm_error_t* pErr = NULL;
  int status = pgm_recvmsg (src->sock, &msgv, 0, &len, &pErr);

  if (PGM_IO_STATUS_NORMAL != status) 
  {
    puts ("read not normal");
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL), ("Receive error: %s)", pErr->message));
    pgm_error_free (pErr);
    return GST_FLOW_ERROR;
  }

  /* try to allocate memory from GStreamer pool */
  *buffer = gst_buffer_new_and_alloc (len);
  if (NULL == *buffer)
  {
    puts ("Could not allocate a buffer?!");
    return GST_FLOW_ERROR;
  }

  /* return contiguous copy */
  GstMapInfo map;
  gst_buffer_map (*buffer, &map, (GstMapFlags)GST_MAP_READWRITE);
  guint8* dst = map.data;
  for (unsigned j = 0; j < msgv.msgv_len; j++)
  {
    memcpy (dst, msgv.msgv_skb[j]->data, msgv.msgv_skb[j]->len);
    dst += msgv.msgv_skb[j]->len;
    len -= msgv.msgv_skb[j]->len;
  }
  gst_buffer_unmap (*buffer, &map);

  //?gst_buffer_set_caps (GST_BUFFER_CAST (*buffer), src->caps);

  return GST_FLOW_OK;
}

/* create PGM transport 
 */
static gboolean gst_pgm_client_src_start ( GstBaseSrc* basesrc)
{
  GstPgmSrc* src = GST_PGM_SRC (basesrc);

  const int valTrue  = 1;
  const int valFalse = 0;

	sa_family_t sa_family = AF_UNSPEC;
  struct pgm_addrinfo_t* res = NULL;
  struct pgm_error_t* pErr = NULL;

	if (!pgm_init (&pErr)) {
		g_error ("Unable to start PGM engine: %s", pErr->message);
		pgm_error_free (pErr);
		goto destroy_transport;
	}

  if (!pgm_getaddrinfo (src->network, NULL, &res, &pErr)) 
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL), ("Parsing network parameter: %s", pErr->message));
    pgm_error_free (pErr);
    return FALSE;
  }

	sa_family = res->ai_send_addrs[0].gsr_group.ss_family;

  if (!pgm_socket (&src->sock, sa_family, SOCK_SEQPACKET, IPPROTO_UDP, &pErr)) 
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL), ("Creating transport: %s", pErr->message));
    pgm_error_free (pErr);
    pgm_freeaddrinfo (res);
    return FALSE;
  }

  if (!pgm_setsockopt (src->sock, IPPROTO_PGM, PGM_RECV_ONLY, &valTrue, sizeof(valTrue))) 
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL), ("cannot set receive-only mode"));
    goto destroy_transport;
  }

  if (!pgm_setsockopt (src->sock, IPPROTO_PGM, PGM_MTU, &src->max_tpdu, sizeof(src->max_tpdu))) 
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL), ("cannot set maximum TPDU size"));
    goto destroy_transport;
  }
  
  if (!pgm_setsockopt (src->sock, IPPROTO_PGM, PGM_MULTICAST_LOOP, &valTrue, sizeof(valTrue))) 
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL), ("cannot set multicast loop"));
    goto destroy_transport;
  }
  
  if (!pgm_setsockopt (src->sock, IPPROTO_PGM, PGM_MULTICAST_HOPS, &src->hops, sizeof(src->hops))) 
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL), ("cannot set IP hop limit"));
    goto destroy_transport;
  }

  if (!pgm_setsockopt (src->sock, IPPROTO_PGM, PGM_RXW_SQNS, &src->rxw_sqns, sizeof(src->rxw_sqns))) 
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL), ("cannot set RXW_SQNS"));
    goto destroy_transport;
  }

  if (!pgm_setsockopt (src->sock, IPPROTO_PGM, PGM_PEER_EXPIRY, &src->peer_expiry, sizeof(src->peer_expiry))) 
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL), ("cannot set peer expiration timeout"));
    goto destroy_transport;
  }

  if (!pgm_setsockopt (src->sock, IPPROTO_PGM, PGM_SPMR_EXPIRY, &src->spmr_expiry, sizeof(src->spmr_expiry))) 
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL), ("cannot set SPMR timeout"));
    goto destroy_transport;
  }

  if (!pgm_setsockopt (src->sock, IPPROTO_PGM, PGM_NAK_BO_IVL, &src->nak_bo_ivl, sizeof(src->nak_bo_ivl))) 
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL), ("cannot set NAK_BO_IVL"));
    goto destroy_transport;
  }

  if (!pgm_setsockopt (src->sock, IPPROTO_PGM, PGM_NAK_RPT_IVL, &src->nak_rpt_ivl, sizeof(src->nak_rpt_ivl))) 
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL), ("cannot set NAK_RPT_IVL"));
    goto destroy_transport;
  }
  
  if (!pgm_setsockopt (src->sock, IPPROTO_PGM, PGM_NAK_RDATA_IVL, &src->nak_rdata_ivl, sizeof(src->nak_rdata_ivl))) 
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL), ("cannot set NAK_RDATA_IVL"));
    goto destroy_transport;
  }
  
  if (!pgm_setsockopt (src->sock, IPPROTO_PGM, PGM_NAK_DATA_RETRIES, &src->nak_data_retries, sizeof(src->nak_data_retries))) 
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL), ("cannot set NAK_DATA_RETRIES"));
    goto destroy_transport;
  }
  
  if (!pgm_setsockopt (src->sock, IPPROTO_PGM, PGM_NAK_NCF_RETRIES, &src->nak_ncf_retries, sizeof(src->nak_ncf_retries))) 
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL), ("cannot set NAK_NCF_RETRIES"));
    goto destroy_transport;
  }
  
  if (!pgm_setsockopt (src->sock, IPPROTO_PGM, PGM_UDP_ENCAP_UCAST_PORT, &src->udp_encap_port, sizeof(src->udp_encap_port)))
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_WRITE, (NULL), ("cannot set UDP encap ucast port"));
    goto destroy_transport;
  }
  
  if (!pgm_setsockopt (src->sock, IPPROTO_PGM, PGM_UDP_ENCAP_MCAST_PORT, &src->udp_encap_port, sizeof(src->udp_encap_port)))
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_WRITE, (NULL), ("cannot set UDP encap mcast port"));
    goto destroy_transport;
  }

  if (!pgm_setsockopt (src->sock, IPPROTO_PGM, PGM_PASSIVE, &valFalse, sizeof(valFalse))) 
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_WRITE, (NULL), ("cannot unset passive"));
    goto destroy_transport;
  }

  if (!pgm_setsockopt (src->sock, IPPROTO_PGM, PGM_IP_ROUTER_ALERT, &valFalse, sizeof(valFalse))) 
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_WRITE, (NULL), ("cannot unset the router assist"));
    goto destroy_transport;
  }


  struct pgm_sockaddr_t addr;
  memset (&addr, '\0', sizeof(addr));
  addr.sa_port = src->port;
  addr.sa_addr.sport = DEFAULT_DATA_SOURCE_PORT;

  if (!pgm_gsi_create_from_hostname (&addr.sa_addr.gsi, &pErr))
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_WRITE, (NULL), ("Creating GSI: %s", pErr->message));
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

  if (!pgm_bind3 ( src->sock
                 , &addr, sizeof(addr)
                 , &ifReq, sizeof(ifReq)  // tx interface
                 , &ifReq, sizeof(ifReq)  // rx interface
                 , &pErr
                 )
     )
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_WRITE, (NULL), ("Binding transport: %s", pErr->message));
    pgm_error_free (pErr);
    goto destroy_transport;
  }

	for (unsigned i = 0; i < res->ai_recv_addrs_len; ++i)
  {
		pgm_setsockopt (src->sock, IPPROTO_PGM, PGM_JOIN_GROUP, &res->ai_recv_addrs[i], sizeof(struct group_req));
  }
	pgm_setsockopt (src->sock, IPPROTO_PGM, PGM_SEND_GROUP, &res->ai_send_addrs[0], sizeof(struct group_req));
	pgm_freeaddrinfo (res);

	if (!pgm_connect (src->sock, &pErr))
  {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_WRITE, (NULL), ("Connecting socket: %s", pErr->message));
    pgm_error_free (pErr);
    goto destroy_transport;
	}

  return TRUE;

destroy_transport:

  pgm_close (src->sock, TRUE);
  src->sock = NULL;
  return FALSE;
}

/* destroy PGM transport 
 */
static gboolean gst_pgm_client_src_stop (GstBaseSrc* io_basesrc)
{
  GstPgmSrc* src = GST_PGM_SRC (io_basesrc);

  GST_DEBUG_OBJECT (src, "destroying transport");

  if (src->sock) 
  {
    pgm_close (src->sock, TRUE);
    src->sock = NULL;
  }

  return TRUE;
}

