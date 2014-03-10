#!/bin/sh

GST_PLUGIN_PATH=. gst-launch-1.0 \
	pgmsrc caps="application/x-rtp" ! \
	rtph263depay ! \
	avdec_h263 ! \
	videoscale ! \
	video/x-raw,width=640,height=480 ! \
	timeoverlay ! \
	autovideosink
