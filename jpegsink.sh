#!/bin/sh

GST_PLUGIN_PATH=. gst-launch-0.10 \
	pgmsrc ! \
	multipartdemux ! \
	jpegdec ! \
	videoscale ! \
	video/x-raw-yuv,width=640,height=480 ! \
	autovideosink
