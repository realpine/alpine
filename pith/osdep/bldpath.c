#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: bldpath.c 934 2008-02-23 00:44:29Z hubert@u.washington.edu $";
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

#if	STDC_HEADERS
#include <ctype.h>
#endif

#include "bldpath.h"

/*
 * Useful definitions
 */
#ifdef	_WINDOWS
#define	ROOTED(S)	(*(S) == '\\' || (isalpha((unsigned char) (S)[0]) && (S)[1] == ':'))
#define	HOMERELATIVE(S)	(FALSE)
#else	/* UNIX */
#define	ROOTED(S)	(*(S) == '/')
#define	HOMERELATIVE(S)	(*(S) == '~')
#endif




/*----------------------------------------------------------------------
      Paste together two pieces of a file name path

  Args: pathbuf      -- Put the result here
        first_part   -- of path name
        second_part  -- of path name
	len          -- Length of pathbuf
 
 Result: New path is in pathbuf.  Note that
	 we don't have to check for /'s at end of first_part and beginning
	 of second_part since multiple slashes are ok.

BUGS:  This is a first stab at dealing with fs naming dependencies, and others 
still exist.
  ----*/
void
build_path(char *pathbuf, char *first_part, char *second_part, size_t len)
{
    if(!(pathbuf && len > 0))
      return;

    pathbuf[0] = '\0';

    if(!first_part || is_rooted_path(second_part)){
	if(second_part)
	  strncpy(pathbuf, second_part, len-1);

	pathbuf[len-1] = '\0';
    }
    else{
#ifdef	_WINDOWS
	int i;
	char *orig_pathbuf = pathbuf;

	for(i = 0; i < len-2 && first_part[i]; i++)
	  *pathbuf++ = first_part[i];

	if(second_part){
	    if(i && first_part[i-1] == '\\'){	/* first part ended with \  */
		if(*second_part == '\\')		/* and second starts with \ */
		  second_part++;			/* else just append second  */
	    }
	    else if(*second_part != '\\')		/* no slash at all, so      */
	      *pathbuf++ = '\\';			/* insert one...	    */

	    while(pathbuf-orig_pathbuf < len-1 && *second_part)
	      *pathbuf++ = *second_part++;
	}

	*pathbuf = '\0';

#else  /* UNIX */

	size_t fpl = 0;

	strncpy(pathbuf, first_part, len-2);
	pathbuf[len-2] = '\0';
	if(second_part){
	    if(*pathbuf && pathbuf[(fpl=strlen(pathbuf))-1] != '/'){
		pathbuf[fpl++] = '/';
		pathbuf[fpl] = '\0';
	    }

	    strncat(pathbuf, second_part, len-1-strlen(pathbuf));
	}

	pathbuf[len-1] = '\0';

#endif
    }
}


/*----------------------------------------------------------------------
  Test to see if the given file path is absolute

  Args: file -- file path to test

 Result: TRUE if absolute, FALSE otw

  ----*/
int
is_absolute_path(char *path)
{
    return(path && (ROOTED(path) || HOMERELATIVE(path)));
}


/*
 * homedir_in_path - return pointer to point in path string where home dir
 *                   reference starts
 */
int
is_rooted_path(char *path)
{
    return(path && ROOTED(path));
}


/*
 * homedir_in_path - return pointer to point in path string where home dir
 *                   reference starts
 */
int
is_homedir_path(char *path)
{
    return(path && HOMERELATIVE(path));
}


/*
 * homedir_in_path - return pointer to point in path string where home dir
 *                   reference starts
 */
char *
homedir_in_path(char *path)
{
#ifdef	_WINDOWS
    return(NULL);
#else	/* UNIX */
    char *p; 

    return((p = strchr(path, '~')) ? p : NULL);
#endif
}


/*
 *
 */
char *
filename_parent_ref(char *s)
{
#ifdef	_WINDOWS
    return(strstr(s, "\\..\\"));
#else	/* UNIX */
    return(strstr(s, "/../"));
#endif
}


int
filename_is_restricted(char *s)
{
#ifdef	_WINDOWS
    return(filename_parent_ref(s) != NULL);
#else	/* UNIX */
    return(strchr("./~", s[0]) != NULL || filename_parent_ref(s) != NULL);
#endif
}
