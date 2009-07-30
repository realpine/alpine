#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: jobcntrl.c 765 2007-10-23 23:51:37Z hubert@u.washington.edu $";
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

#include "jobcntrl.h"


/*----------------------------------------------------------------------
     This routine returns 1 if job control is available.  Note, thiis
     could be some type of fake job control.  It doesn't have to be
     real BSD-style job control.
  ----*/
int
have_job_control(void)
{
    return 1;
}


/*----------------------------------------------------------------------
    If we don't have job control, this routine is never called.
  ----*/
void
stop_process(void)
{
#ifndef _WINDOWS
    RETSIGTYPE (*save_usr2)(int);
    
    /*
     * Since we can't respond to KOD while stopped, the process that sent 
     * the KOD is going to go read-only.  Therefore, we can safely ignore
     * any KODs that come in before we are ready to respond...
     */
    save_usr2 = signal(SIGUSR2, SIG_IGN);
    kill(0, SIGSTOP); 
    (void)signal(SIGUSR2, save_usr2);
#endif /* !_WINDOWS */
}


