/* vim:ts=8:sts=4:sw=4:noai:noexpandtab
 * 
 * PGM GStreamer-sink GObject interface
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

#ifndef GST_PGM_SINK_H
#define GST_PGM_SINK_H

#include <gst/gst.h>
#include <gst/base/gstbasesink.h>

#include <pgm/pgm.h>

G_BEGIN_DECLS

#define GST_TYPE_PGM_SINK             (gst_pgm_sink_get_type())
#define GST_PGM_SINK(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PGM_SINK,GstPgmSink))
#define GST_PGM_SINK_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PGM_SINK,GstPgmSinkClass))
#define GST_IS_PGM_SINK(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PGM_SINK))
#define GST_IS_PGM_SINK_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PGM_SINK))

typedef struct _GstPgmSink GstPgmSink;
typedef struct _GstPgmSinkClass GstPgmSinkClass;

struct _GstPgmSink
{
  GstBaseSink    parent;
  GstPad*        sinkpad;

  struct pgm_sock_t*  sock;

  GThread*    nak_thread;
  gboolean    nak_quit;

  gchar*  network;
  guint   port;
  gchar*  uri;
  guint   udp_encap_port;
  guint   max_tpdu;
  guint   txw_sqns;
  guint   hops;
  guint   spm_ambient;
  guint   ihb_min;
  guint   ihb_max;
};

struct _GstPgmSinkClass
{
  GstBaseSinkClass parent_class;
};

GType gst_pgm_sink_get_type (void);

G_END_DECLS

#endif // GST_PGM_SINK_H
