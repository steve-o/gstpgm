/* vim:ts=8:sts=4:sw=4:noai:noexpandtab
 * 
 * PGM GStreamer plugin main
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

#include "gstpgm.h"
#include "config.h"

#include <pgm/pgm.h>


/* global locals */
static gboolean plugin_init (GstPlugin*);

GST_PLUGIN_DEFINE (
	GST_VERSION_MAJOR,	/* major */
	GST_VERSION_MINOR,	/* minor */
	"pgm",			/* short unique name */
	"PGM connectivity",	/* info */
	plugin_init,		/* GstPlugin::plugin_init */
	VERSION,		/* version */
	"GPL",			/* license */
	PACKAGE_NAME,		/* package-name, usually the file archive name */
	"http://miru.hk/"	/* origin */
	)


/* register gstreamer sink and source
 */
static gboolean
plugin_init (
	GstPlugin*	plugin
	)
{
/* non-superuser friendly configuration */
    setenv ("PGM_TIMER", "GETTIMEOFDAY", TRUE);
    setenv ("PGM_SLEEP", "USLEEP", TRUE);

/* startup pgm library */
    pgm_init ();

/* register GStreamer elements */
    if (!gst_element_register (plugin, "pgmsrc", GST_RANK_NONE, GST_TYPE_PGM_SRC))
	return FALSE;

    if (!gst_element_register (plugin, "pgmsink", GST_RANK_NONE, GST_TYPE_PGM_SINK))
	return FALSE;

    return TRUE;
}

/* eof */
