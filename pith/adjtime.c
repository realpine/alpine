#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: adjtime.c 761 2007-10-23 22:35:18Z hubert@u.washington.edu $";
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

#include "../pith/headers.h"
#include "../pith/adjtime.h"


/*
 * MSC ver 7.0 and less times are since 1900, everybody else's time so far
 * is since 1970. sheesh.
 */
#if	defined(DOS) && (_MSC_VER == 700)
#define EPOCH_ADJ	((time_t)((time_t)(70*365 + 18) * (time_t)86400))
#endif

/*
 * Adjust the mtime to return time since Unix epoch.  DOS is off by 70 years.
 */
time_t
get_adj_time(void)
{
    time_t tt;

    tt = time((time_t *)0);

#ifdef EPOCH_ADJ
    tt -= EPOCH_ADJ;
#endif

    return(tt);
}


time_t
get_adj_name_file_mtime(char *name)
{
    time_t mtime;

    mtime = name_file_mtime(name);

#ifdef EPOCH_ADJ
    if(mtime != (time_t)(-1))
      mtime -= EPOCH_ADJ;
#endif

    return(mtime);
}
