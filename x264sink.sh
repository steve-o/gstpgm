#!/bin/sh

GST_PLUGIN_PATH=. gst-launch-0.10 \
	pgmsrc caps="application/x-rtp" ! \
	rtph264depay ! \
	ffdec_h264 ! \
	videoscale ! \
	video/x-raw-yuv,width=640,height=480 ! \
	timeoverlay ! \
	autovideosink
