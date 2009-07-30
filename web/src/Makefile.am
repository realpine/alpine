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


all:
	cd @abs_top_srcdir@/web/src/cgi.tcl-1.10 && ./configure --prefix=@abs_top_srcdir@/web/lib

install:
	cd @abs_top_srcdir@/web/src/alpined.d && make install
	cd @abs_top_srcdir@/web/src/cgi.tcl-1.10 && make install SCRIPTDIR=@abs_top_srcdir@/web/lib
	cd @abs_top_srcdir@/web/lib && tclsh ./pkgcreate
	if test -x pubcookie/wp_uidmapper ; then $(LN) -f pubcookie/wp_uidmapper @abs_top_srcdir@/web/bin ; fi
	if test -x pubcookie/wp_tclsh ; then $(LN) -f pubcookie/wp_tclsh @abs_top_srcdir@/web/bin ; fi
	if test -x pubcookie/wp_gssapi_proxy ; then $(LN) -f pubcookie/wp_gssapi_proxy @abs_top_srcdir@/web/bin ; fi
