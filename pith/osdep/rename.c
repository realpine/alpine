#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: rename.c 761 2007-10-23 22:35:18Z hubert@u.washington.edu $";
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
#include "err_desc.h"
#include "../charconv/utf8.h"
#include "../charconv/filesys.h"
#include "rename.h"


/*----------------------------------------------------------------------
      Rename a file

  Args: tmpfname -- Old name of file
        fname    -- New name of file
 
 Result: File is renamed.  Returns 0 on success, else -1 on error
	 and errno is valid.
  ----*/
int
rename_file(char *tmpfname, char *fname)
{
#if	HAVE_RENAME
    return(our_rename(tmpfname, fname));
#else
# if	defined(_WINDOWS)
    int ret;

    /*
     * DOS rename doesn't unlink destination for us...
     */
    if((ret = our_unlink(fname)) && (errno == EPERM)){
	ret = -5;
    }
    else{
	ret = our_rename(tmpfname, fname);
	if(ret)
	  ret = -1;
    }

    return(ret);
# else
    int status;

    our_unlink(fname);
    if ((status = link(tmpfname, fname)) != 0)
        return(status);

    our_unlink(tmpfname);
    return(0);
# endif
#endif
}


