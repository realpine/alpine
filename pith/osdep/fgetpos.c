#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: fgetpos.c 761 2007-10-23 22:35:18Z hubert@u.washington.edu $";
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

#include <stdio.h>
#include "fgetpos.h"


/*----------------------------------------------------------------------
   This is just a call to the ANSI C fgetpos function.
  ----*/
int
fget_pos(FILE *stream, fpos_t *ptr)
{
#ifdef	IS_NON_ANSI
    *ptr = (fpos_t)ftell(stream);
    return (*ptr == -1L ? -1 : 0);
#else	/* ANSI */
    return(fgetpos(stream, ptr));
#endif	/* ANSI */
}


/*----------------------------------------------------------------------
   This is just a call to the ANSI C fsetpos function.
  ----*/
int
fset_pos(FILE *stream, fpos_t *ptr)
{
#ifdef	IS_NON_ANSI
    return fseek(stream, *ptr, 0);
#else	/* ANSI */
    return(fsetpos(stream, ptr));
#endif	/* ANSI */
}


