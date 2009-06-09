/*
 * $Id: detach.h 211 2006-11-01 00:34:10Z hubert@u.washington.edu $
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

#ifndef PITH_DETACH_INCLUDED
#define PITH_DETACH_INCLUDED


#include "../pith/filttype.h"
#include "../pith/store.h"


/*
 * Data used to keep track of partial fetches...
 */
typedef struct _fetch_read {
    unsigned	   free_me:1;
    MAILSTREAM	  *stream;		/* stream of open mailbox */
    unsigned long  msgno;		/* message number within mailbox */
    char	  *section,		/* MIME section within message */
		  *chunk,		/* block of partial fetched data */
		  *chunkp,		/* pointer to next char in block */
		  *endp,		/* cell past last char in block */
		  *error;		/* Error message to report */
    unsigned long  read,		/* bytes read so far */
		   size,		/* total bytes to read */
		   chunksize,		/* size of chunk block */
		   allocsize;		/* allocated size of chunk block */
    long	   flags,		/* flags to use fetching block */
		   fetchtime;		/* usecs avg per chunk fetch */
    gf_io_t	   readc;
    STORE_S	  *cache;
} FETCH_READC_S;


extern FETCH_READC_S *g_fr_desc;

#define	AVOID_MICROSOFT_SSL_CHUNKING_BUG ((unsigned long)(12 * 1024L))

/* exported protoypes */
char	   *detach_raw(MAILSTREAM *, long, char *, gf_io_t, int);
char	   *detach(MAILSTREAM *, long, char *, long, long *, gf_io_t, FILTLIST_S *, int);
int	    valid_filter_command(char **);

#endif /* PITH_DETACH_INCLUDED */
