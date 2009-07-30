#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: search.c 854 2007-12-07 17:44:43Z hubert@u.washington.edu $";
#endif

/*
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

#include "../pith/headers.h"
#include "../pith/search.h"


SEARCHSET *
build_searchset(MAILSTREAM *stream)
{
    long          i, run;
    SEARCHSET    *ret_s = NULL, **set;
    MESSAGECACHE *mc;

    if(!stream)
      return(NULL);

    for(i = 1L, set = &ret_s, run = 0L; i <= stream->nmsgs; i++){
	if(!((mc = mail_elt(stream, i)) && mc->sequence)){  /* end of run */
	    if(run){			/* run in progress */
		set = &(*set)->next;
		run = 0L;
	    }
	}
	else if(run++){				/* next in run */
	    (*set)->last = i;
	}
	else{					/* start of new run */
	    *set = mail_newsearchset();
	    /*
	     * Leave off (*set)->last until we get more than one msg
	     * in the run, to avoid 607:607 in SEARCH.
	     */
	    (*set)->first = (*set)->last = i;
	}
    }

    return(ret_s);
}


int
in_searchset(SEARCHSET *srch, long unsigned int num)
{
    SEARCHSET *s;
    unsigned long i;

    if(srch)
      for(s = srch; s; s = s->next)
	for(i = s->first; i <= s->last; i++){
	    if(i == num)
	      return 1;
	}

    return 0;
}
