/*
 * $Id: store.h 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $
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

#ifndef PITH_STORE_INCLUDED
#define PITH_STORE_INCLUDED


#include "../pith/filttype.h"
#ifdef SMIME
#include <openssl/bio.h>
#endif /* SMIME */


typedef enum {CharStarStar, CharStar, FileStar,
#ifdef SMIME
		BioType,
#endif /* SMIME */
		TmpFileStar, PipeStar, ExternalText} SourceType;


/*
 * typedef used by storage object routines
 */

typedef struct store_object {
    unsigned char *dp;		/* current position in data		*/
    unsigned char *eod;		/* end of current data			*/
    void	  *txt;		/* container's data			*/
    unsigned char *eot;		/* end of space alloc'd for data	*/
    int  (*writec)(int, struct store_object *);
    int  (*readc)(unsigned char *, struct store_object *);
    int  (*puts)(struct store_object *, char *);
    fpos_t	   used;	/* amount of object in use		*/
    char          *name;	/* optional object name			*/
    SourceType     src;		/* what we're copying into		*/
    short          flags;	/* flags relating to object use		*/
    CBUF_S         cb;
    PARAMETER	   *attr;	/* attribute list			*/
} STORE_S;

#define	so_writec(c, so)	((*(so)->writec)((c), (so)))
#define	so_readc(c, so)		((*(so)->readc)((c), (so)))
#define	so_puts(so, s)		((*(so)->puts)((so), (s)))


/* exported protoypes */
STORE_S	*so_get(SourceType, char *, int);
int	 so_give(STORE_S **);
int	 so_nputs(STORE_S *, char *, long);
int	 so_seek(STORE_S *, long, int);
int	 so_truncate(STORE_S *, long);
long	 so_tell(STORE_S *);
char	*so_attr(STORE_S *, char *, char *);
int	 so_release(STORE_S *);
void	*so_text(STORE_S *);
char	*so_fgets(STORE_S *, char *, size_t);
void	 so_register_external_driver(STORE_S *(*)(void),
				     int      (*)(STORE_S **),
				     int      (*)(int, STORE_S *),
				     int      (*)(unsigned char *, STORE_S *),
				     int      (*)(STORE_S *, char *),
				     int      (*)(STORE_S *, long, int),
				     int      (*)(STORE_S *, off_t),
				     int      (*)(STORE_S *));


#endif /* PITH_STORE_INCLUDED */
