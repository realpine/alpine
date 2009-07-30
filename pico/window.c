#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: window.c 761 2007-10-23 22:35:18Z hubert@u.washington.edu $";
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

/*
 * Window management. Some of the functions are internal, and some are
 * attached to keys that the user actually types.
 */

#include	"headers.h"


/*
 * Refresh the screen. With no argument, it just does the refresh. With an
 * argument it recenters "." in the current window. Bound to "C-L".
 */
int
pico_refresh(int f, int n)
{
    /*
     * since pine mode isn't using the traditional mode line, sgarbf isn't
     * enough.
     */
    if(Pmaster && curwp)
        curwp->w_flag |= WFMODE;

    if (f == FALSE)
        sgarbf = TRUE;
    else if(curwp){
        curwp->w_force = 0;             /* Center dot. */
        curwp->w_flag |= WFFORCE;
    }

    return (TRUE);
}
