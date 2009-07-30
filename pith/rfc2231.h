/*
 * $Id: rfc2231.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PITH_RFC2231_INCLUDED
#define PITH_RFC2231_INCLUDED


#include "../pith/atttype.h"
#include "../pith/store.h"


/* exported protoypes */
char	   *rfc2231_get_param(PARAMETER *, char *, char **, char **);
int	    rfc2231_output(STORE_S *, char *, char *, char *, char *);
PARMLIST_S *rfc2231_newparmlist(PARAMETER *);
void	    rfc2231_free_parmlist(PARMLIST_S **);
int	    rfc2231_list_params(PARMLIST_S *);


#endif /* PITH_RFC2231_INCLUDED */
