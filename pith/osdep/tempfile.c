#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: tempfile.c 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $";
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

#if	HAVE_TMPFILE
#else
#include "temp_nam.h"
#endif

#include "../charconv/filesys.h"

#include "tempfile.h"


/*----------------------------------------------------------------------
   Create a temporary file, the name of which we don't care about 
and that goes away when it is closed.  Just like ANSI C tmpfile.
  ----*/
FILE  *
create_tmpfile(void)
{
#if	HAVE_TMPFILE
    return(tmpfile());
#else
    char *file_name;
    FILE *stream = NULL;

    file_name = temp_nam(NULL, "pine-tmp");
    if(file_name){
	stream = our_fopen(file_name, "w+b");
	our_unlink(file_name);
	fs_give((void **) &file_name);
    }

    return(stream);
#endif
}


