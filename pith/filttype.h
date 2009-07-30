/*
 * $Id: filttype.h 767 2007-10-24 00:03:59Z hubert@u.washington.edu $
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

#ifndef PITH_FILTTYPE_INCLUDED
#define PITH_FILTTYPE_INCLUDED


/*
 * Size of generic filter's input/output queue
 */
#define	GF_MAXBUF		256


/*
 * typedefs of generalized filters used by gf_pipe
 */
typedef struct filter_s {	/* type to hold data for filter function */
    void (*f)(struct filter_s *, int);
    struct filter_s *next;	/* next filter to call                   */
    long     n;			/* number of chars seen                  */
    short    f1;		/* flags                                 */
    int	     f2;		/* second place for flags                */
    unsigned char t;		/* temporary char                        */
    char     *line;		/* place for temporary storage           */
    char     *linep;		/* pointer into storage space            */
    void     *opt;		/* optional per instance data		 */
    void     *data;		/* misc internal data pointer		 */
    unsigned char queue[1 + GF_MAXBUF];
    short	  queuein, queueout;
} FILTER_S;


typedef struct filter_insert_s {
    char *where;
    char *text;
    int   len;
    struct filter_insert_s *next;
} LT_INS_S;


typedef int  (*gf_io_t)();	/* type of get and put char function     */
typedef void (*filter_t)(FILTER_S *, int);
typedef	int  (*linetest_t)(long, char *, LT_INS_S **, void *);
typedef void (*htmlrisk_t)(void);
typedef int  (*prepedtest_t)(void);

typedef	struct filtlist_s {
    filter_t  filter;
    void     *data;
} FILTLIST_S;


typedef struct cbuf_s {
    unsigned char  cbuf[6];	/* used for converting to or from	*/
    unsigned char *cbufp;	/*   locale-specific charset		*/
    unsigned char *cbufend;
} CBUF_S;


/* exported protoypes */


#endif /* PITH_FILTTYPE_INCLUDED */
