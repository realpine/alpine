/*
 * $Id: detach.h 1025 2008-04-08 22:59:38Z hubert@u.washington.edu $
 *
 * ========================================================================
 * Copyright 2006-2008 University of Washington
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
    unsigned	   we_turned_on:1;
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


/*
 * This lazily gets combined with FT_ flags from c-client so make
 * it different from all those possible values.
 */
#define DT_NODFILTER   (long) 0x10000
#define DT_NOINTR      (long) 0x20000


/* exported protoypes */
char	   *detach_raw(MAILSTREAM *, long, char *, gf_io_t, int);
char	   *detach(MAILSTREAM *, long, char *, long, long *, gf_io_t, FILTLIST_S *, long);
int	    valid_filter_command(char **);
void	    fetch_readc_init(FETCH_READC_S *, MAILSTREAM *, long, char *,
			     unsigned long, long, long);

#endif /* PITH_DETACH_INCLUDED */
