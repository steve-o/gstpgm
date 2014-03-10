# -*- mode: python -*-
# OpenPGM build script
# $Id$

EnsureSConsVersion( 0, 96 )
import os
env = Environment(ENV = os.environ,
        CCFLAGS = ['-pipe', '-pedantic', '-std=gnu99', '-D_REENTRANT'],
        LINKFLAGS = ['-pipe'],
	LIBS = ['libpgm'],
	CPPPATH = ['../openpgm/pgm/include'],
	LIBPATH = ['../openpgm/pgm/ref/release']
)
env.ParseConfig('pkg-config --cflags --libs glib-2.0 gthread-2.0 gstreamer-base-1.0');
env.SharedLibrary('libgstpgm', ['GstPGM.c', 'GstPGMSrc.c', 'GstPGMSink.c']);
