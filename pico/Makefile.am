## Process this file with automake to produce Makefile.in
## Use aclocal -I m4; automake

# ========================================================================
# Copyright 2006 University of Washington
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# ========================================================================

SUBDIRS = osdep

bin_PROGRAMS = pico pilot

pico_SOURCES = main.c utf8stub.c

pilot_SOURCES = pilot.c utf8stub.c

pico_LDADD = $(LDADD) $(INTLLIBS)

pilot_LDADD = $(LDADD) $(INTLLIBS)

LDADD = ../c-client/utf8.o libpico.a osdep/libpicoosd.a \
	../pith/osdep/libpithosd.a ../pith/charconv/libpithcc.a

noinst_LIBRARIES = libpico.a

libpico_a_SOURCES = attach.c basic.c bind.c browse.c buffer.c composer.c display.c file.c \
	fileio.c line.c pico.c random.c region.c search.c window.c word.c \
	ebind.h edef.h efunc.h estruct.h headers.h keydefs.h mode.h pico.h

AM_CPPFLAGS = -I@top_srcdir@/include

