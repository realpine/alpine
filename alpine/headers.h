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

#ifndef ALPINE_HEADERS_INCLUDED
#define ALPINE_HEADERS_INCLUDED


/*----------------------------------------------------------------------
           Include files
 ----*/

#include "../pith/headers.h"

#include "../pico/headers.h"


/*
 * Redefinition to help pico storage object use more clear
 */
#define	PicoText ExternalText


#if !defined(DOS) && !defined(OS2) && !defined(LEAVEOUTFIFO)
    /*
     * If LEAVEOUTFIFO is set in os.h, then we leave it out.
     * If it isn't set, we still might leave it out. We'll decide
     * based on whether or not O_NONBLOCK is defined or not.
     * It's just a guess. Safer would be to change the polarity of the
     * test and define something like INCLUDEFIFO instead of LEAVEOUTFIFO
     * and only define it where we know. Since we don't really know
     * we'd rather run the risk of being wrong and finding out that
     * way instead of just having people not know about it.
     */
#if !defined(O_NONBLOCK)
#define LEAVEOUTFIFO 1
#endif
#endif

/* include osdep protos and def'ns */
#include "osdep/debuging.h"
#include "osdep/execview.h"
#include "osdep/fltrname.h"
#include "osdep/jobcntrl.h"
#include "osdep/print.h"
#include "osdep/termin.gen.h"
#include "osdep/termout.gen.h"
#ifndef _WINDOWS
#include "osdep/termin.unx.h"
#include "osdep/termout.unx.h"
#else /* _WINDOWS */
#include "osdep/termin.wnt.h"
#include "osdep/termout.wnt.h"
#endif /* _WINDOWS */


#endif /* ALPINE_HEADERS_INCLUDED */
