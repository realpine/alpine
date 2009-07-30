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

EXTRA_DIST = config.rpath  VERSION README LICENSE

SUBDIRS = m4 po $(REGEX_BUILD) pith pico alpine $(WEB_PUBCOOKIE_BUILD) $(WEB_BUILD)

BUILT_SOURCES = c-client.d c-client $(WEB_PUBCOOKIE_LINK)

CLEANFILES = c-client

.PHONY: c-client.d c-client gssapi_proxy.l

c-client.d:
	if test ! -L c-client ; then $(LN_S) imap/c-client c-client ; fi

c-client:
	$(C_CLIENT_WITH_IPV6)
	cd imap && $(MAKE) $(C_CLIENT_TARGET) $(C_CLIENT_CFLAGS) $(C_CLIENT_LDFLAGS) $(C_CLIENT_GCCOPTLEVEL) EXTRASPECIALS="$(C_CLIENT_SPECIALS)"

gssapi_proxy.l:
	if test ! -L c-client/auth_gss_proxy.c ; then $(LN_S) @abs_top_srcdir@/web/src/pubcookie/auth_gss_proxy.c c-client/auth_gss_proxy.c ; fi

clean-local:
	cd imap && $(MAKE) clean

man_MANS = doc/alpine.1 doc/pico.1 doc/pilot.1 doc/rpdump.1 doc/rpload.1

ACLOCAL_AMFLAGS = -I m4
