#!/bin/sh

GST_PLUGIN_PATH=. gst-launch-1.0 \
	v4l2src ! \
	videorate ! \
	video/x-raw,framerate=15/1 ! \
	videoscale ! \
	video/x-raw,width=352,height=288 ! \
	avenc_h263 ! \
	rtph263pay ! \
	pgmsink network=";239.192.0.2" dport=7502 udp-encap-port=3057

