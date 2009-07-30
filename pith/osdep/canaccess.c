#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: canaccess.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
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
#include "bldpath.h"
#include "fnexpand.h"
#include "../charconv/utf8.h"
#include "../charconv/filesys.h"
#include "canaccess.h"


/*
 * Useful definitions
 */
#ifdef	_WINDOWS

#define ACCESS_IN_CWD(F,M)	(can_access((F), (M)))
#define	PATH_SEP		';'
#define	FILE_SEP		'\\'

#else  /* UNIX */

#define ACCESS_IN_CWD(F,M)	(-1)
#define	PATH_SEP		':'
#define	FILE_SEP		'/'

#endif /* UNIX */



/*
 *     Check if we can access a file in a given way
 *
 * Args: file      -- The file to check
 *       mode      -- The mode ala the access() system call, see ACCESS_EXISTS
 *                    and friends in alpine.h.
 *
 * Result: returns 0 if the user can access the file according to the mode,
 *         -1 if he can't (and errno is set).
 *
 *
 */
int
can_access(char *file, int mode)
{
#ifdef	_WINDOWS
    struct stat buf;

    /*
     * NOTE: The WinNT access call returns that every directory is readable and
     * writable. We actually want to know if the write is going to fail, so we
     * try it. We don't read directories in Windows so we skip implementing that.
     */
    if(mode & WRITE_ACCESS && file && !our_stat(file, &buf) && (buf.st_mode & S_IFMT) == S_IFDIR){
	char *testname;
	int   fd;
	size_t l = 0;

	/*
	 * We'd like to just call temp_nam here, since it creates a file
	 * and does what we want. However, temp_nam calls us!
	 */
	if((testname = malloc(MAXPATH * sizeof(char)))){
	    strncpy(testname, file, MAXPATH-1);
	    testname[MAXPATH-1] = '\0';
	    if(testname[0] && testname[(l=strlen(testname))-1] != '\\' &&
	       l+1 < MAXPATH){
		l++;
		strncat(testname, "\\", MAXPATH-strlen(testname)-1);
		testname[MAXPATH-1] = '\0';
	    }
	    
	    if(l+8 < MAXPATH &&
	       strncat(testname, "caXXXXXX", MAXPATH-strlen(testname)-1) && mktemp(testname)){
		if((fd = our_open(testname, O_CREAT|O_EXCL|O_WRONLY|O_BINARY, 0600)) >= 0){
		    (void)close(fd);
		    our_unlink(testname);
		    free(testname);
		    /* success, drop through to access call */
		}
		else{
		    free(testname);
		    /* can't write in the directory */
		    return(-1);
		}
	    }
	    else{
		free(testname);
		return(-1);
	    }
	}
    }
    if(mode & EXECUTE_ACCESS)    /* Windows access has no execute mode */
      mode &= ~EXECUTE_ACCESS;   /* and crashes because of it */
#endif /* WINDOWS */

    return(our_access(file, mode));
}


/*----------------------------------------------------------------------
       Check if we can access a file in a given way in the given path

   Args: path     -- The path to look for "file" in
	 file      -- The file to check
         mode      -- The mode ala the access() system call, see ACCESS_EXISTS
                      and friends in alpine.h.

 Result: returns 0 if the user can access the file according to the mode,
         -1 if he can't (and errno is set).
 ----*/
int
can_access_in_path(char *path, char *file, int mode)
{
    char tmp[MAXPATH];
    int  rv = -1;

    if(!path || !*path || is_rooted_path(file)){
	rv = can_access(file, mode);
    }
    else if(is_homedir_path(file)){
	strncpy(tmp, file, sizeof(tmp));
	tmp[sizeof(tmp)-1] = '\0';
	rv = fnexpand(tmp, sizeof(tmp)) ? can_access(tmp, mode) : -1;
    }
    else if((rv = ACCESS_IN_CWD(file,mode)) < 0){
	char path_copy[MAXPATH + 1], *p, *t;

	if(strlen(path) < MAXPATH){
	    strncpy(path_copy, path, sizeof(path_copy));
	    path_copy[sizeof(path_copy)-1] = '\0';

	    for(p = path_copy; p && *p; p = t){
		if((t = strchr(p, PATH_SEP)) != NULL)
		  *t++ = '\0';

		snprintf(tmp, sizeof(tmp), "%s%c%s", p, FILE_SEP, file);
		if((rv = can_access(tmp, mode)) == 0)
		  break;
	    }
	}
    }

    return(rv);
}
