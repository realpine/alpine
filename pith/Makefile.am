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

SUBDIRS = osdep charconv

noinst_LIBRARIES = libpith.a

BUILT_SOURCES = helptext.h

noinst_SCRIPTS = cmplhelp.sh cmplhlp2.sh

libpith_a_SOURCES = ablookup.c abdlc.c addrbook.c addrstring.c adrbklib.c bldaddr.c charset.c \
	color.c conf.c context.c copyaddr.c detoken.c detach.c editorial.c escapes.c \
	filter.c flag.c folder.c handle.c help.c helpindx.c hist.c icache.c imap.c init.c \
	keyword.c ldap.c list.c mailcap.c mailcmd.c mailindx.c maillist.c mailview.c \
	margin.c mimedesc.c mimetype.c msgno.c newmail.c news.c pattern.c pipe.c \
	readfile.c remote.c reply.c rfc2231.c save.c search.c sequence.c send.c sort.c \
	state.c status.c store.c stream.c string.c strlst.c takeaddr.c tempfile.c text.c \
	thread.c adjtime.c url.c util.c helptext.c

helptext.c: cmplhelp.sh pine.hlp
	$(SHELL) cmplhelp.sh < pine.hlp > $@

helptext.h: cmplhlp2.sh pine.hlp
	$(SHELL) cmplhlp2.sh < pine.hlp > $@

AM_CPPFLAGS = -I$(top_builddir)/include -I$(top_srcdir)/include

CLEANFILES = helptext.c helptext.h cmplhelp.sh cmplhlp2.sh
