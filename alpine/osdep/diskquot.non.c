#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: diskquot.non.c 761 2007-10-23 22:35:18Z hubert@u.washington.edu $";
#endif

/*
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

#include <system.h>

#ifdef	USE_QUOTAS

/*----------------------------------------------------------------------
   This system doesn't have disk quotas.
   Return space left in disk quota on file system which given path is in.

    Args: path - Path name of file or directory on file system of concern
          over - pointer to flag that is set if the user is over quota

 Returns: If *over = 0, the number of bytes free in disk quota as per
          the soft limit.
	  If *over = 1, the number of bytes *over* quota.
          -1 is returned on an error looking up quota
           0 is returned if there is no quota

BUG:  If there's more than 2.1Gb free this function will break
  ----*/
long
disk_quota(path, over)
    char *path;
    int  *over;
{
    return(0L);
}
#endif /* USE_QUOTAS */


