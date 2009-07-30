#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: writ_dir.c 761 2007-10-23 22:35:18Z hubert@u.washington.edu $";
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
#include "../charconv/utf8.h"
#include "../charconv/filesys.h"
#include "canaccess.h"
#include "writ_dir.h"


/*----------------------------------------------------------------------
      Check to see if a directory exists and is writable by us

   Args: dir -- directory name

 Result:       returns 0 if it exists and is writable
                       1 if it is a directory, but is not writable
                       2 if it is not a directory
                       3 it doesn't exist.
  ----*/
int
is_writable_dir(char *dir)
{
    struct stat sb;

    if(our_stat(dir, &sb) < 0)
      /*--- It doesn't exist ---*/
      return(3);

    if(!(sb.st_mode & S_IFDIR))
      /*---- it's not a directory ---*/
      return(2);

    if(can_access(dir, 07))
      return(1);
    else
      return(0);
}


