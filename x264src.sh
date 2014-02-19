#!/bin/sh

GST_PLUGIN_PATH=. gst-launch-0.10 \
	v4lsrc ! \
	videorate ! \
	video/x-raw-yuv,width=320,height=240,framerate=15/1 ! \
	x264enc byte-stream=true bitrate=128 bframes=4 b_pyramid=true me=dia trellis=false key_int_max=4000 threads=1 ! \
	rtph264pay ! \
	pgmsink

