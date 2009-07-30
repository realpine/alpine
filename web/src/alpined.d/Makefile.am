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

# This is because alpined, libwpcomm and friends are
# not intended for system-wide consumption
locbindir = @abs_srcdir@/../../bin
loclibdir = @abs_srcdir@/../../lib

locbin_PROGRAMS = alpined alpineldap

alpined_SOURCES = alpined.c busy.c color.c imap.c ldap.c remote.c \
		  signal.c debug.c status.c stubs.c \
		  alpined.h color.h ldap.h

alpineldap_SOURCES = alpineldap.c busy.c color.c imap.c ldap.c remote.c \
		   signal.c debug.c status.c stubs.c \
		   alpined.h color.h ldap.h

LDADD = local.o \
	@top_srcdir@/pith/libpith.a \
	@top_srcdir@/pith/osdep/libpithosd.a \
	@top_srcdir@/pith/charconv/libpithcc.a \
	@top_srcdir@/c-client/c-client.a \
	$(WEB_PUBCOOKIE_LIB)

loclib_LTLIBRARIES = libwpcomm.la

libwpcomm_la_SOURCES = wpcomm.c

libwpcomm_la_LDFLAGS = -rpath '$(loclibdir)' -version-info 1:0:0

AM_CPPFLAGS = -I@top_builddir@/include -I@top_srcdir@/include

AM_LDFLAGS = `cat @top_srcdir@/c-client/LDFLAGS`

CLEANFILES = local.c

local.c: alpineldap.c color.c imap.c ldap.c remote.c signal.c \
	 debug.c status.c stubs.c alpined.h color.h ldap.h
	echo "char datestamp[]="\"`date`\"";" > local.c
	echo "char hoststamp[]="\"`hostname`\"";" >> local.c
