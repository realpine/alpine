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

noinst_LIBRARIES = libpithosd.a

libpithosd_a_SOURCES = bldpath.c canaccess.c canonicl.c collate.c color.c coredump.c \
	creatdir.c debugtime.c domnames.c err_desc.c fgetpos.c filesize.c \
	fnexpand.c hostname.c lstcmpnt.c mimedisp.c pipe.c pw_stuff.c \
	rename.c tempfile.c temp_nam.c writ_dir.c

AM_CPPFLAGS = -I@top_builddir@/include -I@top_srcdir@/include
