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

noinst_LIBRARIES = libpicoosd.a

libpicoosd_a_SOURCES = altedit.c chkpoint.c color.c filesys.c fsync.c getkey.c mouse.c \
	newmail.c popen.c raw.c read.c shell.c signals.c spell.c terminal.c truncate.c tty.c

AM_CPPFLAGS = -I@top_builddir@/include -I@top_srcdir@/include
