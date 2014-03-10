#!/bin/sh

GST_PLUGIN_PATH=. gst-launch-1.0 \
	v4l2src ! \
	videorate ! \
	video/x-raw,width=320,height=240,framerate=15/1 ! \
	x264enc byte-stream=true bitrate=128 vbv-buf-capacity=300 bframes=0 b_pyramid=true weightb=true me=dia trellis=false key_int_max=4000 threads=1 ! \
	rtph264pay ! \
	pgmsink

