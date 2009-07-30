/*
 * $Id: arg.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PINE_ARG_INCLUDED
#define PINE_ARG_INCLUDED


#include "../pith/state.h"
#include "../pith/string.h"


/*
 * Used by pine_args to tell caller what was found;
 */
typedef struct argdata {
    enum	{aaFolder = 0, aaMore, aaURL, aaMail,
		 aaPrcCopy, aaAbookCopy} action;
    union {
	char	  *folder;
	char	  *file;
	struct {
	    STRLIST_S *addrlist;
	    PATMT     *attachlist;
	} mail;
	struct {
	    char      *local;
	    char      *remote;
	} copy;
    } data;
    char      *url;
} ARGDATA_S;


/* exported protoypes */
void	    pine_args(struct pine *, int, char **, ARGDATA_S *);
void        display_args_err(char *, char **, int);
void        args_help(void);


#endif /* PINE_ARG_INCLUDED */
