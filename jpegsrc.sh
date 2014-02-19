#!/bin/sh
# framerate=1/5 is equivalent to one frame every 5 seconds
# framerate=5/1 means 5 frames every second.

GST_PLUGIN_PATH=. gst-launch-0.10 \
	v4l2src ! \
	videorate ! \
	video/x-raw-yuv,width=320,height=240,framerate=15/1 ! \
	jpegenc quality=40 ! \
	multipartmux ! \
	pgmsink

