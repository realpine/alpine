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

EXTRA_DIST = config.rpath  VERSION README LICENSE

SUBDIRS = m4 po pith pico alpine $(WEB_BUILD) $(WEB_PUBCOOKIE_BUILD)

DIST_SUBDIRS = pith pico alpine web packages po doc include mapi

BUILT_SOURCES = c-client.d c-client

CLEANFILES = c-client

.PHONY: c-client.d c-client

c-client.d:
	if test ! -L c-client ; then $(LN_S) imap/c-client c-client ; fi

c-client:
	cd imap && $(MAKE) @C_CLIENT_TARGET@ @C_CLIENT_SPECIALS@

clean-local:
	cd imap && $(MAKE) clean

man_MANS = doc/alpine.1 doc/pico.1 doc/pilot.1

ACLOCAL_AMFLAGS = -I m4
