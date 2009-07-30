#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: filesize.c 761 2007-10-23 22:35:18Z hubert@u.washington.edu $";
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
#include "filesize.h"


/*----------------------------------------------------------------------
      Return the number of bytes in given file

    Args: file -- file name

  Result: the number of bytes in the file is returned or
          -1 on error, in which case errno is valid
 ----*/
long
name_file_size(char *file)
{
    struct stat buffer;

    if(our_stat(file, &buffer) != 0)
      return(-1L);

    return((long)buffer.st_size);
}


/*----------------------------------------------------------------------
      Return the number of bytes in given file

    Args: fp -- FILE * for open file

  Result: the number of bytes in the file is returned or
          -1 on error, in which case errno is valid
 ----*/
long
fp_file_size(FILE *fp)
{
    struct stat buffer;

    if(fstat(fileno(fp), &buffer) != 0)
      return(-1L);

    return((long)buffer.st_size);
}


/*----------------------------------------------------------------------
      Return the modification time of given file

    Args: file -- file name

  Result: the time of last modification (mtime) of the file is returned or
          -1 on error, in which case errno is valid
 ----*/
time_t
name_file_mtime(char *file)
{
    struct stat buffer;

    if(our_stat(file, &buffer) != 0)
      return((time_t)(-1));

    return(buffer.st_mtime);
}


/*----------------------------------------------------------------------
      Return the modification time of given file

    Args: fp -- FILE * for open file

  Result: the time of last modification (mtime) of the file is returned or
          -1 on error, in which case errno is valid
 ----*/
time_t
fp_file_mtime(FILE *fp)
{
    struct stat buffer;

    if(fstat(fileno(fp), &buffer) != 0)
      return((time_t)(-1));

    return(buffer.st_mtime);
}


/*----------------------------------------------------------------------
      Copy the mode, owner, and group of sourcefile to targetfile.

    Args: targetfile -- 
	  sourcefile --
    
    We don't bother keeping track of success or failure because we don't care.
 ----*/
void
file_attrib_copy(char *targetfile, char *sourcefile)
{
    struct stat buffer;

    if(our_stat(sourcefile, &buffer) == 0){
	our_chmod(targetfile, buffer.st_mode);
#if	HAVE_CHOWN
	our_chown(targetfile, buffer.st_uid, buffer.st_gid);
#endif
    }
}
