## Process this file with automake to produce Makefile.in
## Use aclocal -I m4; automake

# ========================================================================
# Copyright 2006-2007 University of Washington
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# ========================================================================

noinst_LIBRARIES = libregex.a

libregex_a_SOURCES = regcomp.c regerror.c regexec.c regfree.c

AM_CPPFLAGS = -I@top_builddir@/include -I@top_srcdir@/include
