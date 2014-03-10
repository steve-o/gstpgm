#!/bin/sh

GST_PLUGIN_PATH=. gst-launch-1.0 \
	v4lsrc ! \
	videorate ! \
	video/x-raw-yuv,width=320,height=240,framerate=15/1 ! \
	theoraenc quality=6 ! \
	oggmux ! \
	pgmsink

