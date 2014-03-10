/* vim:ts=8:sts=4:sw=4:noai:noexpandtab
 * 
 * PGM GStreamer plugin main
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

#include "gstpgm.h"
#include "config.h"

#include <pgm/pgm.h>


static gboolean plugin_init (GstPlugin*);

GST_PLUGIN_DEFINE 
  ( GST_VERSION_MAJOR   // major
  , GST_VERSION_MINOR   // minor
  , pgm                 // short unique name
  , "PGM connectivity"  // info
  , plugin_init         // GstPlugin::plugin_init
  , VERSION             // version
  , "GPL"               // license
  , GST_PACKAGE_NAME    // package name, usually the file archive name
  , GST_PACKAGE_ORIGIN  // origin
  )


static gboolean plugin_init (GstPlugin* plugin)
{
  if (!pgm_init (NULL)) return FALSE;

  if (!gst_element_register (plugin, "pgmsrc", GST_RANK_NONE, GST_TYPE_PGM_SRC)) return FALSE;
  if (!gst_element_register (plugin, "pgmsink", GST_RANK_NONE, GST_TYPE_PGM_SINK)) return FALSE;

  return TRUE;
}

