/*
 * $Id: context.h 764 2007-10-23 23:44:49Z hubert@u.washington.edu $
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

#ifndef PITH_CONTEXT_INCLUDED
#define PITH_CONTEXT_INCLUDED


#include "../pith/foldertype.h"
#include "../pith/savetype.h"


/* exported prototypes */
char	   *context_digest(char *, char *, char *, char *, char *, size_t);
char	   *context_apply(char *, CONTEXT_S *, char *, size_t);
int	    context_isambig(char *);
int	    context_allowed(char *);
long	    context_create(CONTEXT_S *, MAILSTREAM *, char *);
MAILSTREAM *context_open(CONTEXT_S *, MAILSTREAM *, char *, long, long *);
long	    context_status(CONTEXT_S *, MAILSTREAM *, char *, long);
long	    context_status_full(CONTEXT_S *, MAILSTREAM *, char *,
				long, imapuid_t *, imapuid_t *);				
long	    context_status_streamp(CONTEXT_S *, MAILSTREAM **,
				   char *, long);
long	    context_status_streamp_full(CONTEXT_S *, MAILSTREAM **, char *, long,
					imapuid_t *, imapuid_t *);
long	    context_rename(CONTEXT_S *, MAILSTREAM *, char *, char *);
MAILSTREAM *context_already_open_stream(CONTEXT_S *, char *, int);
long	    context_delete(CONTEXT_S *, MAILSTREAM *, char *);
long	    context_append(CONTEXT_S *, MAILSTREAM *, char *, STRING *);
long	    context_append_full(CONTEXT_S *, MAILSTREAM *, char *, char *, char *, STRING *);
long	    context_append_multiple(CONTEXT_S *, MAILSTREAM *, char *, append_t,
				    APPENDPACKAGE *, MAILSTREAM *);
long	    context_copy(CONTEXT_S *, MAILSTREAM *, char *, char *);
MAILSTREAM *context_same_stream(CONTEXT_S *, char *, MAILSTREAM *);
CONTEXT_S  *new_context(char *, int *);
void	    free_contexts(CONTEXT_S **);
void	    free_context(CONTEXT_S **);


#endif /* PITH_CONTEXT_INCLUDED */
