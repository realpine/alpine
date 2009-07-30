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


WP_UIDMAPPER_SOCKET = "/tmp/wp_uidmapper"
WEBSERVER_UID = 65534

CFLAGS = '-DWP_UIDMAPPER_SOCKET=$(WP_UIDMAPPER_SOCKET)' \
         '-DWEBSERVER_UID=$(WEBSERVER_UID)' \
         '-DAUTH_GSS_PROXY_PATH="$(WEB_BINDIR)/wp_gssapi_proxy"'

bin_PROGRAMS = wp_uidmapper wp_tclsh wp_gssapi_proxy wp_umc

noinst_LIBRARIES = libauthgssproxy.a

libauthgssproxy_a_SOURCES = auth_gss_proxy.c

wp_uidmapper_SOURCES = wp_uidmapper.c id_table.c id_table.h 

wp_tclsh_SOURCES = wp_tclsh.c wp_uidmapper_lib.c

wp_gssapi_proxy_SOURCES = wp_gssapi_proxy.c wp_uidmapper_lib.c

wp_umc_SOURCES = wp_umc.c wp_uidmapper_lib.c

AM_CPPFLAGS = -I@top_builddir@/include -I@top_srcdir@/include
