#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: lstcmpnt.c 897 2008-01-04 22:49:15Z hubert@u.washington.edu $";
#endif

/*
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

#include <system.h>
#include <general.h>

#include <string.h>
#include "lstcmpnt.h"


#ifdef	_WINDOWS

#define	FILE_SEP		'\\'

#else  /* UNIX */

#define	FILE_SEP		'/'

#endif /* UNIX */



/*----------------------------------------------------------------------
      Return pointer to last component of pathname.

  Args: filename     -- The pathname.
 
 Result: Returned pointer points to last component in the input argument.
  ----*/
char *
last_cmpnt(char *filename)
{
    char *p = NULL, *q = filename;

    if(filename == NULL)
      return(filename);

    while((q = strchr(q, FILE_SEP)) != NULL)
      if(*++q)
	p = q;

#ifdef	_WINDOWS

    if(!p && isalpha((unsigned char) *filename) && *(filename+1) == ':' && *(filename+2))
      p = filename + 2;

#endif

    return(p);
}


