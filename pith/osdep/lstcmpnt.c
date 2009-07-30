#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: lstcmpnt.c 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $";
#endif

/*
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

#include <system.h>
#include <general.h>

#include <string.h>
#include "../../pith/charconv/filesys.h"
#include "canaccess.h"
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


/*
 * Like our_mkdir but it makes subdirs as well as the final dir
 */
int
our_mkpath(char *path, mode_t mode)
{
    char save, *q = path;

#ifdef	_WINDOWS
    if(isalpha((unsigned char) q[0]) && q[1] == ':' && q[2])
      q = path + 3;
#endif

    if(q == path && q[0] == FILE_SEP)
      q = path + 1;

    while((q = strchr(q, FILE_SEP)) != NULL){
	save = *q;
	*q = '\0';
	if(can_access(path, ACCESS_EXISTS) != 0)
	  if(our_mkdir(path, mode) != 0){
	      *q = save;
	      return -1;
	  }

	*q = save;
	q++;
    }

    if(can_access(path, ACCESS_EXISTS) != 0 && our_mkdir(path, mode) != 0)
      return -1;

    return 0;
}
