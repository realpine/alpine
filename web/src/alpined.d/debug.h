/*-----------------------------------------------------------------------
 $Id: debug.h 130 2006-09-22 04:39:36Z mikes@u.washington.edu $
  -----------------------------------------------------------------------*/

/* ========================================================================
 * Copyright 2006-2007 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */

#ifndef _WEB_ALPINE_DEBUG_INCLUDED
#define _WEB_ALPINE_DEBUG_INCLUDED


#ifndef DEBUG
/*
 * support dprint regardless so we leave at least a few
 * footsteps in syslog
 */
#undef    dprint
#define   dprint(x)	{ output_debug_msg x ; }

/* alpined-scoped debugging level */
extern int debug;

void    output_debug_msg(int, char *fmt, ...);
#endif


/*
 * Use these to for dprint() debug level arg to force
 * debug output (typically to syslog())
 */
#define SYSDBG		0x8000
#define SYSDBG_ALERT	SYSDBG+1
#define SYSDBG_ERR	SYSDBG+2
#define SYSDBG_INFO	SYSDBG+3
#define SYSDBG_DEBUG	SYSDBG+4


/* exported prototypes */
void	debug_init(void);
void    setup_imap_debug(void);


#endif /* _WEB_ALPINE_DEBUG_INCLUDED */
