#!/bin/sh

GST_PLUGIN_PATH=. gst-launch-1.0 \
	pgmsrc network=";239.192.0.2" dport=7502 udp-encap-port=3057 caps="application/x-rtp" ! \
	rtph263depay ! \
	avdec_h263 ! \
	videoscale ! \
	video/x-raw,width=640,height=480 ! \
	timeoverlay ! \
	videoconvert ! \
	autovideosink
