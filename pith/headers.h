/*
 * $Id: headers.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
 *
 * ========================================================================
 * Copyright 2006 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */


#ifndef PITH_HEADERS_INCLUDED
#define PITH_HEADERS_INCLUDED


/*----------------------------------------------------------------------
           Include files
 ----*/
#include <system.h>		/* os-dep defs/includes */
#include <general.h>		/* generally useful definitions */

#include "../c-client/mail.h"	/* for MAILSTREAM and friends */
#include "../c-client/osdep.h"
#include "../c-client/rfc822.h"	/* for soutr_t and such */
#include "../c-client/misc.h"	/* for cpystr proto */
#include "../c-client/utf8.h"	/* for CHARSET and such*/
#include "../c-client/imap4r1.h"

/* include osdep protos and def'ns */
#include "osdep/bldpath.h"
#include "osdep/canaccess.h"
#include "osdep/canonicl.h"
#include "osdep/collate.h"
#include "osdep/color.h"
#include "osdep/coredump.h"
#include "osdep/creatdir.h"
#include "osdep/debugtime.h"
#include "osdep/domnames.h"
#include "osdep/err_desc.h"
#include "osdep/fgetpos.h"
#include "osdep/filesize.h"
#include "osdep/fnexpand.h"
#include "osdep/hostname.h"
#include "osdep/lstcmpnt.h"
#include "osdep/mimedisp.h"
#include "osdep/pipe.h"
#include "osdep/pithosd.h"
#include "osdep/pw_stuff.h"
#include "osdep/rename.h"
#include "osdep/tempfile.h"
#include "osdep/temp_nam.h"
#include "osdep/writ_dir.h"
#include "charconv/utf8.h"
#include "charconv/filesys.h"

#include "debug.h"

#endif /* PITH_HEADERS_INCLUDED */
