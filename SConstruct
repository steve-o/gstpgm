# -*- mode: python -*-
# OpenPGM build script
# $Id$

EnsureSConsVersion( 0, 96 )
import os
env = Environment(ENV = os.environ,
        CCFLAGS = ['-pipe', '-pedantic', '-std=gnu99', '-D_REENTRANT'],
        LINKFLAGS = ['-pipe'],
	LIBS = ['libpgmplus-pic'],
	CPPPATH = ['/miru/projects/openpgm/pgm/include'],
#	LIBPATH = ['/miru/projects/openpgm/pgm/ref/debug']
	LIBPATH = ['/miru/projects/openpgm/pgm/ref/thirtytwo']
)
#env.ParseConfig('pkg-config --cflags --libs glib-2.0 gthread-2.0 gstreamer-0.10');
env.ParseConfig('pkg-config --cflags --libs glib-2.0 gthread-2.0 gstreamer-base-0.10');
env.SharedLibrary('libgstpgm', ['gstpgm.c', 'gstpgmsrc.c', 'gstpgmsink.c']);
