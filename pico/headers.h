/*
 * $Id: headers.h 764 2007-10-23 23:44:49Z hubert@u.washington.edu $
 *
 * ========================================================================
 * Copyright 2006-2007 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 * 
 *
 *
 *     headers.h
 *
 *  The include file to always include that includes a few other things
 *    -  includes the most general system files and other pine include files
 *    -  declares the global variables
 */
         

#ifndef _PICO_HEADERS_INCLUDED
#define _PICO_HEADERS_INCLUDED


#include <system.h>
#include <general.h>

/*----------------------------------------------------------------------
           Include files
 
 ----*/

#ifdef _WINDOWS
#include "osdep/msmenu.h"
#include "osdep/mswin.h"
#endif /* _WINDOWS */
#include "estruct.h"
#include "mode.h"
#include "pico.h"
#include "keydefs.h"
#include "edef.h"
#include "efunc.h"
#include "utf8stub.h"

/* include osdep proto's defn's */
#include "osdep/altedit.h"
#include "osdep/chkpoint.h"
#include "osdep/color.h"
#include "osdep/filesys.h"
#include "osdep/getkey.h"
#include "osdep/mouse.h"
#include "osdep/newmail.h"
#include "osdep/popen.h"
#include "osdep/raw.h"
#include "osdep/read.h"
#include "osdep/shell.h"
#include "osdep/signals.h"
#include "osdep/spell.h"
#include "osdep/terminal.h"
#include "osdep/tty.h"


#endif /* _PICO_HEADERS_INCLUDED */
