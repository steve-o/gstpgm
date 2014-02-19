/* vim:ts=8:sts=4:sw=4:noai:noexpandtab
 * 
 * PGM GStreamer-source GObject interface (GstPgmSrc)
 *
 * Copyright (c) 2008 Miru Limited.
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

#include "gstpgmsrc.h"
#include "config.h"


/* global locals */
enum
{
    PROP_NETWORK = 1,
    PROP_PORT,
    PROP_URI,
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
    PROP_NAK_NCF_RETRIES
};

static const GstElementDetails gst_pgm_src_details =
GST_ELEMENT_DETAILS(
	"PGM GStreamer-source",		    /* long name */
	"Source/Network/Protocol/Device",   /* element class (klass) */
	"PGM connectivity",		    /* description */
	"Miru Limited <mail@miru.hk>"	    /* author */
);

/* supported media types of this pad */
static GstStaticPadTemplate gst_pgm_src_src_template =
GST_STATIC_PAD_TEMPLATE(
	"src",			/* name */
	GST_PAD_SRC,		/* direction */
	GST_PAD_ALWAYS,		/* presence */
	GST_STATIC_CAPS_ANY	/* capabilities */
);

GST_BOILERPLATE (GstPgmSrc, gst_pgm_src, GstPushSrc, GST_TYPE_PUSH_SRC)

static gboolean gst_pgm_src_set_uri (GstPgmSrc*, const gchar*);
static void gst_pgm_src_uri_handler_init (gpointer, gpointer);
static GstFlowReturn gst_pgm_src_create (GstPushSrc*, GstBuffer**);
static gboolean gst_pgm_client_src_stop (GstBaseSrc*);
static gboolean gst_pgm_client_src_start (GstBaseSrc*);
static void gst_pgm_src_finalize (GObject*);
static void gst_pgm_src_set_property (GObject*, guint, const GValue*, GParamSpec*);
static void gst_pgm_src_get_property (GObject*, guint, GValue*, GParamSpec*);

/* uri handlers
 */
static GstURIType
gst_pgm_src_uri_get_type (void)
{
    return GST_URI_SRC;
}

static gchar**
gst_pgm_src_uri_get_protocols (void)
{
    static gchar* protocols[] = { "pgm", NULL };
    return protocols;
}

static const gchar*
gst_pgm_src_uri_get_uri (
	GstURIHandler*	    handler
	)
{
    GstPgmSrc* src = GST_PGM_SRC (handler);
    return src->uri;
}

static gboolean
gst_pgm_src_uri_set_uri (
	GstURIHandler*	    handler,
	const gchar*	    uri
	)
{
    GstPgmSrc* src = GST_PGM_SRC (handler);
    return gst_pgm_src_set_uri (src, uri);
}

static void
gst_pgm_src_uri_handler_init (
	gpointer	    g_iface,
	gpointer	    iface_data
	)
{
    GstURIHandlerInterface* iface = (GstURIHandlerInterface*)g_iface;
    iface->get_type	    = gst_pgm_src_uri_get_type;
    iface->get_protocols    = gst_pgm_src_uri_get_protocols;
    iface->get_uri	    = gst_pgm_src_uri_get_uri;
    iface->set_uri	    = gst_pgm_src_uri_set_uri;
}

/* recalculate src uri
 */
static void
gst_pgm_src_update_uri (
	GstPgmSrc*	    src
	)
{
    g_free (src->uri);
    src->uri = g_strdup_printf ("pgm://%s:%i:%i", src->network, src->port, src->udp_encap_port);
}

/* decode src uri
 */
static gboolean
gst_pgm_src_set_uri (
	GstPgmSrc*	    src,
	const gchar*	    uri
	)
{
    gchar* protocol = gst_uri_get_protocol (uri);
    if (strncmp (protocol, "pgm", strlen("pgm")) != 0)
	goto wrong_protocol;
    g_free (protocol);

    gchar* location = gst_uri_get_location (uri);
    if (location == NULL)
	return FALSE;
    gchar* port = strstr (location, ":");
    g_free (src->network);
    if (port == NULL) {
	src->network	    	= g_strdup (location);
	src->port		= PGM_PORT;
	src->udp_encap_port	= PGM_UDP_ENCAP_PORT;
    } else {
	src->network		= g_strndup (location, port - location);
	src->port		= atoi (port + 1);
	gchar* udp_encap_port	= strstr (port + 1, ":");
	if (udp_encap_port == NULL) {
	    src->udp_encap_port    = PGM_UDP_ENCAP_PORT;
	} else {
	    src->udp_encap_port    = atoi (udp_encap_port + 1);
	}
    }
    g_free (location);

    gst_pgm_src_update_uri (src);
    return TRUE;

wrong_protocol:
    g_free (protocol);
    GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
	("error parsing uri %s: wrong protocol (%s != udp)", uri, protocol));
    return FALSE;
}

/* GObject::base_init
 *
 * Register PGM GStreamer-source GObject type (GstPgmSrc) with GLib.
 */
static void
gst_pgm_src_base_init (
	gpointer	    g_class
	)
{
    GstElementClass* element_class = GST_ELEMENT_CLASS (g_class);

    gst_element_class_add_pad_template (element_class,
					gst_static_pad_template_get (&gst_pgm_src_src_template));
    gst_element_class_set_details (element_class, &gst_pgm_src_details);
}

/* GObject::class_init
 */
static void
gst_pgm_src_class_init (
	GstPgmSrcClass*	    klass
	)
{
    GstBaseSrcClass* gstbasesrc_class = (GstBaseSrcClass*)klass;
    gstbasesrc_class->start	= gst_pgm_client_src_start;
    gstbasesrc_class->stop	= gst_pgm_client_src_stop;
    GstPushSrcClass* gstpushsrc_class = (GstPushSrcClass*)klass;
    gstpushsrc_class->create	= gst_pgm_src_create;

    GObjectClass* gobject_class	= (GObjectClass*)klass;
    gobject_class->finalize	= gst_pgm_src_finalize;
    gobject_class->set_property = gst_pgm_src_set_property;
    gobject_class->get_property = gst_pgm_src_get_property;

    g_object_class_install_property (gobject_class, PROP_NETWORK,
	g_param_spec_string (
	    "network",
	    "Network",
	    "Rendezvous style multicast network definition.",
	    PGM_NETWORK,
	    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (gobject_class, PROP_PORT,
	g_param_spec_uint (
	    "dport",
	    "DPORT",
	    "Data-destination port.",
	    0,				/* minimum */
	    UINT16_MAX,			/* maximum */
	    PGM_PORT,			/* default */
	    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (gobject_class, PROP_URI,
	g_param_spec_string (
	    "uri",
	    "URI",
	    "URI in the form of pgm://adapter;receive-multicast-groups;send-multicast-group:destination-port:udp-encapsulation-port",
	    PGM_URI,
	    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (gobject_class, PROP_UDP_ENCAP_PORT,
	g_param_spec_uint (
	    "udp-encap-port",
	    "UDP encapsulation port",
	    "UDP port for encapsulation of PGM protocol.",
	    0,				/* minimum */
	    UINT16_MAX,			/* maximum */
	    PGM_UDP_ENCAP_PORT,		/* default */
	    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (gobject_class, PROP_MAX_TPDU,
	g_param_spec_uint (
	    "max-tpdu",
	    "Maximum TPDU",
	    "Largest supported Transport Protocol Data Unit.",
	    (sizeof(struct iphdr) + sizeof(struct pgm_header)),	/* minimum */
	    UINT16_MAX,			/* maximum */
	    PGM_MAX_TPDU,		/* default */
	    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (gobject_class, PROP_HOPS,
	g_param_spec_uint (
	    "hops",
	    "Hops",
	    "Multicast packet hop limit.",
	    1,				/* minimum */
	    UINT8_MAX,			/* maximum */
	    PGM_HOPS,			/* default */
	    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (gobject_class, PROP_RXW_SQNS,
	g_param_spec_uint (
	    "rxw-sqns",
	    "RXW_SQNS",
	    "Size of the receive window in sequence numbers.",
	    1,				/* minimum */
	    UINT16_MAX,			/* maximum */
	    PGM_RXW_SQNS,		/* default */
	    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (gobject_class, PROP_PEER_EXPIRY,
	g_param_spec_uint (
	    "peer-expiry",
	    "Peer expiry",
	    "Expiration timeout for peers.",
	    1,				/* minimum */
	    UINT_MAX,			/* maximum */
	    PGM_PEER_EXPIRY,		/* default */
	    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (gobject_class, PROP_SPMR_EXPIRY,
	g_param_spec_uint (
	    "spmr-expiry",
	    "SPMR expiry",
	    "Maximum back-off range before sending a SPM-Request.",
	    1,				/* minimum */
	    UINT_MAX,			/* maximum */
	    PGM_SPMR_EXPIRY,		/* default */
	    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (gobject_class, PROP_NAK_BO_IVL,
	g_param_spec_uint (
	    "nak-bo-ivl",
	    "NAK_BO_IVL",
	    "Back-off interval before sending a NAK.",
	    1,				/* minimum */
	    UINT_MAX,			/* maximum */
	    PGM_NAK_BO_IVL,		/* default */
	    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (gobject_class, PROP_NAK_RPT_IVL,
	g_param_spec_uint (
	    "nak-rpt-ivl",
	    "NAK_RPT_IVL",
	    "Repeat interval before re-sending a NAK.",
	    1,				/* minimum */
	    UINT_MAX,			/* maximum */
	    PGM_NAK_RPT_IVL,		/* default */
	    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (gobject_class, PROP_NAK_RDATA_IVL,
	g_param_spec_uint (
	    "nak-data-ivl",
	    "NAK_RDATA_IVL",
	    "Timeout waiting for data.",
	    1,				/* minimum */
	    UINT_MAX,			/* maximum */
	    PGM_NAK_RDATA_IVL,		/* default */
	    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (gobject_class, PROP_NAK_DATA_RETRIES,
	g_param_spec_uint (
	    "nak-data-retries",
	    "NAK_DATA_RETRIES",
	    "Maximum number of retries.",
	    1,				/* minimum */
	    UINT_MAX,			/* maximum */
	    PGM_NAK_DATA_RETRIES,	/* default */
	    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
    g_object_class_install_property (gobject_class, PROP_NAK_NCF_RETRIES,
	g_param_spec_uint (
	    "nak-ncf-retries",
	    "NAK_NCF_RETRIES",
	    "Maximum number of retries.",
	    1,				/* minimum */
	    UINT_MAX,			/* maximum */
	    PGM_NAK_NCF_RETRIES,	/* default */
	    G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

/* GObject::instance_init ???
 */
static void
gst_pgm_src_init (
	GstPgmSrc*	    src,
	GstPgmSrcClass*	    g_class
	)
{
    src->transport = NULL;

    src->network	    = g_strdup (PGM_NETWORK);
    src->port		    = PGM_PORT;
    src->uri		    = g_strdup (PGM_URI);
    src->udp_encap_port     = PGM_UDP_ENCAP_PORT;
    src->max_tpdu	    = PGM_MAX_TPDU;
    src->hops		    = PGM_HOPS;
    src->rxw_sqns	    = PGM_RXW_SQNS;
    src->peer_expiry	    = PGM_PEER_EXPIRY;
    src->spmr_expiry	    = PGM_SPMR_EXPIRY;
    src->nak_bo_ivl	    = PGM_NAK_BO_IVL;
    src->nak_rpt_ivl	    = PGM_NAK_RPT_IVL;
    src->nak_rdata_ivl	    = PGM_NAK_RDATA_IVL;
    src->nak_data_retries   = PGM_NAK_DATA_RETRIES;
    src->nak_ncf_retries    = PGM_NAK_NCF_RETRIES;

    gst_base_src_set_live (GST_BASE_SRC (src), TRUE);
}

/* GObject::finalize
 */
static void
gst_pgm_src_finalize (
	GObject*	    object
	)
{
    GstPgmSrc* src = GST_PGM_SRC (object);

    g_free (src->network);
    g_free (src->uri);

    G_OBJECT_CLASS (parent_class)->finalize (object);
}

/* GObject::set_property
 *
 * Set runtime configuration property
 */
static void
gst_pgm_src_set_property (
	GObject*	    object,
	guint		    prop_id,
	const GValue*	    value,
	GParamSpec*	    pspec
	)
{
    GstPgmSrc* src = GST_PGM_SRC (object);

    switch (prop_id) {
    case PROP_NETWORK:
	g_free (src->network);
	if (g_value_get_string (value) == NULL)
	    src->network = g_strdup (PGM_NETWORK);
	else
	    src->network = g_value_dup_string (value);
	gst_pgm_src_update_uri (src);
	break;
    case PROP_PORT:
	src->port = g_value_get_uint (value);
	gst_pgm_src_update_uri (src);
	break;
    case PROP_UDP_ENCAP_PORT:
	src->udp_encap_port = g_value_get_uint (value);
	gst_pgm_src_update_uri (src);
	break;
    case PROP_URI:
	gst_pgm_src_set_uri (src, g_value_get_string (value));
	break;
    case PROP_MAX_TPDU:
	src->max_tpdu = g_value_get_uint (value);
	break;
    case PROP_HOPS:
	src->hops = g_value_get_uint (value);
	break;
    case PROP_RXW_SQNS:
	src->rxw_sqns = g_value_get_uint (value);
	break;
    case PROP_PEER_EXPIRY:
	src->peer_expiry = g_value_get_uint (value);
	break;
    case PROP_SPMR_EXPIRY:
	src->spmr_expiry = g_value_get_uint (value);
	break;
    case PROP_NAK_BO_IVL:
	src->nak_bo_ivl = g_value_get_uint (value);
	break;
    case PROP_NAK_RPT_IVL:
	src->nak_rpt_ivl = g_value_get_uint (value);
	break;
    case PROP_NAK_RDATA_IVL:
	src->nak_rdata_ivl = g_value_get_uint (value);
	break;
    case PROP_NAK_DATA_RETRIES:
	src->nak_data_retries = g_value_get_uint (value);
	break;
    case PROP_NAK_NCF_RETRIES:
	src->nak_ncf_retries = g_value_get_uint (value);
	break;
    }
}

/* GObject::get_property
 *
 * Get runtime configuration property
 */
static void
gst_pgm_src_get_property (
	GObject*	    object,
	guint		    prop_id,
	GValue*		    value,
	GParamSpec*	    pspec
	)
{
    GstPgmSrc* src = GST_PGM_SRC (object);

    switch (prop_id) {
    case PROP_NETWORK:
	g_value_set_string (value, src->network);
	break;
    case PROP_PORT:
	g_value_set_uint (value, src->port);
	break;
    case PROP_UDP_ENCAP_PORT:
	g_value_set_uint (value, src->udp_encap_port);
	break;
    case PROP_URI:
	g_value_set_string (value, src->uri);
	break;
    case PROP_MAX_TPDU:
	g_value_set_uint (value, src->max_tpdu);
	break;
    case PROP_HOPS:
	g_value_set_uint (value, src->hops);
	break;
    case PROP_RXW_SQNS:
	g_value_set_uint (value, src->rxw_sqns);
	break;
    case PROP_PEER_EXPIRY:
	g_value_set_uint (value, src->peer_expiry);
	break;
    case PROP_SPMR_EXPIRY:
	g_value_set_uint (value, src->spmr_expiry);
	break;
    case PROP_NAK_BO_IVL:
	g_value_set_uint (value, src->nak_bo_ivl);
	break;
    case PROP_NAK_RPT_IVL:
	g_value_set_uint (value, src->nak_rpt_ivl);
	break;
    case PROP_NAK_RDATA_IVL:
	g_value_set_uint (value, src->nak_rdata_ivl);
	break;
    case PROP_NAK_DATA_RETRIES:
	g_value_set_uint (value, src->nak_data_retries);
	break;
    case PROP_NAK_NCF_RETRIES:
	g_value_set_uint (value, src->nak_ncf_retries);
	break;
    }
}

/* GstPushSrcClass::create
 *
 * As a GStreamer source, create data, so recv on PGM transport.
 */
static GstFlowReturn
gst_pgm_src_create (
	GstPushSrc*	    pushsrc,
	GstBuffer**	    buffer
	)
{
    GstPgmSrc* src = GST_PGM_SRC (pushsrc);

/* read in waiting data */
    pgm_msgv_t msgv;
    gssize len = pgm_transport_recvmsg (src->transport, &msgv, 0);

g_message ("len = %i", (int)len);
    if (len <= 0) {
	GST_ELEMENT_ERROR (src, RESOURCE, READ, (NULL),
	    ("receive error %d: %s (%d)", len, g_strerror (errno), errno));
	return GST_FLOW_ERROR;
    }

/* try to allocate memory from GStreamer pool */
    *buffer = gst_buffer_new_and_alloc (len);

/* return contiguous copy */
    guint i = 0;
    guint8* dst = GST_BUFFER_DATA (*buffer);
    while (len)
    {
	memcpy (dst, msgv.msgv_iov[i].iov_base, msgv.msgv_iov[i].iov_len);
	dst += msgv.msgv_iov[i].iov_len;
	len -= msgv.msgv_iov[i].iov_len;
	i++;
    }

    return GST_FLOW_OK;
}

/* create PGM transport 
 */
static gboolean
gst_pgm_client_src_start (
	GstBaseSrc*	    basesrc
	)
{
    GstPgmSrc* src = GST_PGM_SRC (basesrc);
    pgm_gsi_t gsi;
    if (0 != pgm_create_md5_gsi (&gsi)) {
	GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
	    ("cannot resolve hostname"));
	return FALSE;
    }

    struct pgm_sock_mreq recv_smr, send_smr;
    int smr_len = 1;
    if (0 != pgm_if_parse_transport (src->network, AF_INET, &recv_smr, &send_smr, &smr_len)) {
	GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
	    ("cannot parse network parameter"));
	return FALSE;
    }
    if (1 != smr_len) {
	GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
	    ("parsing network parameter found more than one device"));
	return FALSE;
    }

    ((struct sockaddr_in*)&send_smr.smr_multiaddr)->sin_port = g_htons (src->udp_encap_port);
    ((struct sockaddr_in*)&recv_smr.smr_interface)->sin_port = g_htons (src->udp_encap_port);

    if (0 != pgm_transport_create (&src->transport, &gsi, src->port, &recv_smr, 1, &send_smr)) {
	GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
	    ("cannot create transport"));
	return FALSE;
    }
    if (0 != pgm_transport_set_recv_only (src->transport, FALSE)) {
	GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
	    ("cannot set receive-only mode"));
	goto destroy_transport;
    }
    if (0 != pgm_transport_set_max_tpdu (src->transport, src->max_tpdu)) {
	GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
	    ("cannot set maximum TPDU size"));
	goto destroy_transport;
    }
    if (0 != pgm_transport_set_hops (src->transport, src->hops)) {
	GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
	    ("cannot set IP hop limit"));
	goto destroy_transport;
    }
    if (0 != pgm_transport_set_rxw_sqns (src->transport, src->rxw_sqns)) {
	GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
	    ("cannot set RXW_SQNS"));
	goto destroy_transport;
    }
    if (0 != pgm_transport_set_peer_expiry (src->transport, src->peer_expiry)) {
	GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
	    ("cannot set peer expiration timeout"));
	goto destroy_transport;
    }
    if (0 != pgm_transport_set_spmr_expiry (src->transport, src->spmr_expiry)) {
	GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
	    ("cannot set SPMR timeout"));
	goto destroy_transport;
    }
    if (0 != pgm_transport_set_nak_bo_ivl (src->transport, src->nak_bo_ivl)) {
	GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
	    ("cannot set NAK_BO_IVL"));
	goto destroy_transport;
    }
    if (0 != pgm_transport_set_nak_rpt_ivl (src->transport, src->nak_rpt_ivl)) {
	GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
	    ("cannot set NAK_RPT_IVL"));
	goto destroy_transport;
    }
    if (0 != pgm_transport_set_nak_rdata_ivl (src->transport, src->nak_rdata_ivl)) {
	GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
	    ("cannot set NAK_RDATA_IVL"));
	goto destroy_transport;
    }
    if (0 != pgm_transport_set_nak_data_retries (src->transport, src->nak_data_retries)) {
	GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
	    ("cannot set NAK_DATA_RETRIES"));
	goto destroy_transport;
    }
    if (0 != pgm_transport_set_nak_ncf_retries (src->transport, src->nak_ncf_retries)) {
	GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
	    ("cannot set NAK_NCF_RETRIES"));
	goto destroy_transport;
    }
    if (0 != pgm_transport_bind (src->transport)) {
	GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ, (NULL),
	    ("cannot bind transport"));
	goto destroy_transport;
    }

    return TRUE;
destroy_transport:

    pgm_transport_destroy (src->transport, TRUE);
    src->transport = NULL;
    return FALSE;
}

/* destroy PGM transport 
 */
static gboolean
gst_pgm_client_src_stop (
	GstBaseSrc*	    basesrc
	)
{
    GstPgmSrc* src = GST_PGM_SRC (basesrc);

    GST_DEBUG_OBJECT (src, "destroying transport");
    if (src->transport) {
        pgm_transport_destroy (src->transport, TRUE);
	src->transport = NULL;
    }

    return TRUE;
}

/* eof */
