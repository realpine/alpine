/*
 * $Id: mimetype.h 769 2007-10-24 00:15:40Z hubert@u.washington.edu $
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
 */

#ifndef PITH_MIMETYPE_INCLUDED
#define PITH_MIMETYPE_INCLUDED


/*
 * Struct passed mime.types search functions
 */
typedef struct {
    union {
	char	 *ext;
	char	 *mime_type;
    } from;
    union {
	struct {
	    int	  type;
	    char *subtype;
	} mime;
	char	 *ext;
    } to;
} MT_MAP_T;


typedef int (* MT_OPERATORPROC)(MT_MAP_T *, FILE *);


/* exported protoypes */
int	    set_mime_type_by_extension(BODY *, char *);
int	    set_mime_extension_by_type(char *, char *);
int         mt_srch_by_ext(MT_MAP_T *, FILE *);
int         mt_get_file_ext(char *, char **);
int         mt_srch_mime_type(MT_OPERATORPROC, MT_MAP_T *);
int         mt_translate_type(char *);


#endif /* PITH_MIMETYPE_INCLUDED */
