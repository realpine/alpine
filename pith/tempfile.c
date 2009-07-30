#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: tempfile.c 770 2007-10-24 00:23:09Z hubert@u.washington.edu $";
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

#include "../pith/headers.h"
#include "../pith/tempfile.h"


/*
 * Return the name of a file in the same directory as filename.
 * Same as temp_nam except it figures out a name in the same directory.
 * It also returns the name of the directory in ret_dir if ret_dir is
 * not NULL. That has to be freed by caller. If return is not NULL the
 * empty file has been created.
 */
char *
tempfile_in_same_dir(char *filename, char *prefix, char **ret_dir)
{
#ifndef MAXPATH
#define MAXPATH 1000    /* Longest file path we can deal with */
#endif
    char  dir[MAXPATH+1];
    char *dirp = NULL;
    char *ret_file = NULL;

    if(filename){
	char *lc;

	if((lc = last_cmpnt(filename)) != NULL){
	    int to_copy;

	    to_copy = (lc - filename > 1) ? (lc - filename - 1) : 1;
	    strncpy(dir, filename, MIN(to_copy, sizeof(dir)-1));
	    dir[MIN(to_copy, sizeof(dir)-1)] = '\0';
	}
	else{
	    dir[0] = '.';
	    dir[1] = '\0';
	}

	dirp = dir;
    }


    /* temp_nam creates ret_file */
    ret_file = temp_nam(dirp, prefix);

    /*
     * If temp_nam can't write in dirp it puts the file in a temp directory
     * anyway. We don't want that to happen to us.
     */
    if(dirp && ret_file && !in_dir(dirp, ret_file)){
	our_unlink(ret_file);
	fs_give((void **)&ret_file);  /* sets it to NULL */
    }

    if(ret_file && ret_dir && dirp)
      *ret_dir = cpystr(dirp);
      

    return(ret_file);
}


/*
 * Returns non-zero if dir is a prefix of path.
 *         zero     if dir is not a prefix of path, or if dir is empty.
 */
int
in_dir(char *dir, char *path)
{
    return(*dir ? !strncmp(dir, path, strlen(dir)) : 0);
}
