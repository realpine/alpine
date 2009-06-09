#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: wimap.c 73 2006-06-13 16:46:59Z hubert@u.washington.edu $";
#endif

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

/*======================================================================
    debug.c
    Provide debug support routines
 ====*/

#include <system.h>
#include <general.h>

#include "../../../c-client/c-client.h"

#include "../../../pith/state.h"
#include "../../../pith/debug.h"

#include "debug.h"


#define	MAX_DEBUG_FMT	1024


#ifndef	DEBUG
/*
 * Preserve debug for syslog trace
 */
int	debug;
#endif


void
debug_init(void)
{
#if	HAVE_SYSLOG
    openlog("alpined", LOG_PID, LOG_MAIL);
#endif
}


void
output_debug_msg(int dlevel, char *fmt, ...)
{
    /* always write SYSDBG */
    if((dlevel & SYSDBG) || dlevel <= debug){
#if	HAVE_SYSLOG
	va_list args;
	char    fmt2[MAX_DEBUG_FMT], *p, *q, *trailing = NULL;
	int	priority = LOG_DEBUG, leading = 1;

	/* whack nl's */
	for(p = fmt, q = fmt2; *p && p - fmt < MAX_DEBUG_FMT - 2; p++){
	    if(*p == '\n'){
		if(!leading && !trailing)
		  trailing = q;
	    }
	    else{
		leading = 0;
		if(trailing){
		    *q++ = '_';
		    trailing = NULL;
		}

		*q++ = *p;
	    }
	}

	*q = '\0';
	if(trailing)
	  *trailing = '\0';

	if(dlevel & SYSDBG)
	  switch(dlevel){
	    case SYSDBG_ALERT : priority = LOG_ALERT; break;
	    case SYSDBG_ERR   : priority = LOG_ERR;   break;
	    case SYSDBG_INFO  : priority = LOG_INFO;  break;
	    default	      : priority = LOG_DEBUG; break;
	  }


	va_start(args, fmt);
	vsyslog(priority, fmt2, args);
	va_end(args);
#else
# error Write something to record error/debugging output
#endif
    }
}

#ifdef DEBUG

void
dump_configuration(int brief)
{
    dprint((8, "asked to dump_configuration"));
}


void
dump_contexts(void)
{
    dprint((8, "asked to dump_contexts"));
}


void
setup_imap_debug(void)
{
    int olddebug;

    olddebug = debug;

    if(debug > 7)
      ps_global->debug_imap = 4;
    else if(debug > 6)
      ps_global->debug_imap = 3;
    else if(debug > 4)
      ps_global->debug_imap = 2;
    else if(debug > 2)
      ps_global->debug_imap = 1;
    else
      ps_global->debug_imap = 0;

    if(ps_global->mail_stream){
	if(ps_global->debug_imap > 0){
	    mail_debug(ps_global->mail_stream);
	}
	else{
	    mail_nodebug(ps_global->mail_stream);
	}
    }

    if(debug > 7 && olddebug <= 7)
      mail_parameters(NULL, SET_TCPDEBUG, (void *) TRUE);
    else if(debug <= 7 && olddebug > 7 && !ps_global->debugmem)
      mail_parameters(NULL, SET_TCPDEBUG, (void *) FALSE);

}
#endif	/* DEBUG */
