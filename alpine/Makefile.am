## Process this file with automake to produce Makefile.in
## Use aclocal -I m4; automake

# ========================================================================
# Copyright 2006-2008 University of Washington
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# ========================================================================

SUBDIRS = osdep

bin_PROGRAMS = alpine rpdump rpload

alpine_SOURCES = addrbook.c adrbkcmd.c after.c alpine.c arg.c busy.c colorconf.c \
	confscroll.c context.c dispfilt.c flagmaint.c folder.c help.c imap.c \
	init.c kblock.c keymenu.c ldapconf.c listsel.c mailcmd.c mailindx.c \
	mailpart.c mailview.c newuser.c pattern.c pipe.c print.c radio.c remote.c reply.c \
	roleconf.c send.c setup.c signal.c status.c takeaddr.c titlebar.c smime.c newmail.c

alpine_LDADD = $(LDADD) $(INTLLIBS)

nodist_alpine_SOURCES = date.c

rpdump_SOURCES = rpdump.c

rpload_SOURCES = rpload.c

BUILT_SOURCES = date.c

LDADD = ../pico/libpico.a ../pico/osdep/libpicoosd.a \
	../pith/libpith.a ../pith/osdep/libpithosd.a  \
	../pith/charconv/libpithcc.a \
	osdep/libpineosd.a ../c-client/c-client.a

AM_CPPFLAGS = -I@top_builddir@/include -I@top_srcdir@/include -DLOCALEDIR=\"$(localedir)\"

AM_LDFLAGS = `cat @top_srcdir@/c-client/LDFLAGS`

CLEANFILES = date.c

date.c:
	echo "char datestamp[]="\"`date`\"";" > date.c
	echo "char hoststamp[]="\"`hostname`\"";" >> date.c
