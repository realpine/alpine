#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: creatdir.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
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
#include "../charconv/utf8.h"
#include "../charconv/filesys.h"
#include "creatdir.h"


#ifdef	S_IRWXU
#define	MAILDIR_MODE	S_IRWXU
#else
#define	MAILDIR_MODE	0700
#endif



/*----------------------------------------------------------------------
      Create the mail subdirectory.

  Args: dir -- Name of the directory to create
 
 Result: Directory is created.  Returns 0 on success, else -1 on error
	 and errno is valid.
  ----*/
int
create_mail_dir(char *dir)
{
    if(our_mkdir(dir, MAILDIR_MODE) < 0)
      return(-1);

#ifndef _WINDOWS
    our_chmod(dir, MAILDIR_MODE);

    /* Some systems need this, on others we don't care if it fails */
    our_chown(dir, getuid(), getgid());
#endif /* !_WINDOWS */

    return(0);
}
