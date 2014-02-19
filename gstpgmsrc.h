/* vim:ts=8:sts=4:sw=4:noai:noexpandtab
 * 
 * PGM GStreamer-source GObject interface
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

#ifndef __GSTPGMSRC_H__
#define __GSTPGMSRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

#include <pgm/pgm.h>

G_BEGIN_DECLS

#define GST_TYPE_PGM_SRC \
  (gst_pgm_src_get_type())
#define GST_PGM_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PGM_SRC,GstPgmSrc))
#define GST_PGM_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PGM_SRC,GstPgmSrcClass))
#define GST_IS_PGM_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PGM_SRC))
#define GST_IS_PGM_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PGM_SRC))

typedef struct _GstPgmSrc GstPgmSrc;

struct _GstPgmSrc
{
    GstPushSrc		parent;
    GstPad*		srcpad;

    pgm_transport_t*	transport;
    pgm_msgv_t*		msgv;

    gchar*		network;
    guint		port;
    gchar*		uri;
    guint16		udp_encap_port;
    guint		max_tpdu;
    guint		hops;
    guint		rxw_sqns;
    guint		peer_expiry;
    guint		spmr_expiry;
    guint		nak_bo_ivl;
    guint		nak_rpt_ivl;
    guint		nak_rdata_ivl;
    guint		nak_data_retries;
    guint		nak_ncf_retries;
};

typedef struct _GstPgmSrcClass GstPgmSrcClass;

struct _GstPgmSrcClass
{
    GstPushSrcClass parent_class;
};

GType gst_pgm_src_get_type (void);

G_END_DECLS

#endif /* __GSTPGMSRC_H__ */
