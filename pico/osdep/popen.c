#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: popen.c 761 2007-10-23 22:35:18Z hubert@u.washington.edu $";
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
#include <general.h>
#include "../estruct.h"




/*
 * P_open - run the given command in a sub-shell returning a file pointer
 *	    from which to read the output
 *
 * note:
 *	For OS's other than unix, you will have to rewrite this function.
 *	Hopefully it'll be easy to exec the command into a temporary file, 
 *	and return a file pointer to that opened file or something.
 */
int
P_open(char *s)
{
#if	HAVE_POPEN
    extern FIOINFO g_pico_fio;

    g_pico_fio.flags = FIOINFO_READ;
    g_pico_fio.name = "pipe";

    if((g_pico_fio.fp = popen(s, "r")) != NULL)
      return(FIOSUC);

    return(FIOERR);
#else
    /* Windows never did this, but piping has been done elsewhere */
    return(0);
#endif
}



/*
 * P_close - close the given descriptor
 *
 */
void
P_close(void)
{
#if	HAVE_PCLOSE
    extern FIOINFO g_pico_fio;

    if(g_pico_fio.fp)
      (void) pclose(g_pico_fio.fp);
#endif
}
