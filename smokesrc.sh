#!/bin/sh

GST_PLUGIN_PATH=. gst-launch-1.0 \
	v4l2src ! \
	videorate ! \
	video/x-raw-yuv,width=320,height=240,framerate=15/1 ! \
	smokeenc qmax=40 keyframe=8 ! \
	pgmsink

