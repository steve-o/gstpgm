#!/bin/sh

GST_PLUGIN_PATH=. gst-launch-0.10 \
	pgmsrc network=";239.192.0.2" dport=7502 udp-encap-port=3057 caps="application/x-rtp" ! \
	rtph263depay ! \
	ffdec_h263 ! \
	videoscale ! \
	video/x-raw-yuv,width=640,height=480 ! \
	timeoverlay ! \
	ffmpegcolorspace ! \
	autovideosink
