#!/bin/sh

GST_PLUGIN_PATH=. gst-launch-1.0 \
	pgmsrc caps="application/x-rtp" ! \
	rtph264depay ! \
	avdec_h264 ! \
	videoscale ! \
	video/x-raw,width=640,height=480 ! \
	timeoverlay ! \
	autovideosink
