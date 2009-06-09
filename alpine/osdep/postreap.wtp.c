#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: postreap.wtp.c 136 2006-09-22 20:06:05Z hubert@u.washington.edu $";
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

#include "../headers.h"
#include "../osdep.h"


/*======================================================================
    post_reap
    
    Manage exit status collection of a child spawned to handle posting
 ====*/



#if	defined(BACKGROUND_POST) && defined(SIGCHLD)
/*----------------------------------------------------------------------
    Check to see if we have any posting processes to clean up after

  Args: none
  Returns: any finished posting process reaped
 ----*/
post_reap()
{
    WaitType stat;
    int	     r;

    if(ps_global->post && ps_global->post->pid){
	r = waitpid(ps_global->post->pid, &stat, WNOHANG);
	if(r == ps_global->post->pid){
	    ps_global->post->status = WIFEXITED(stat) ? WEXITSTATUS(stat) : -1;
	    ps_global->post->pid = 0;
	    return(1);
	}
	else if(r < 0 && errno != EINTR){ /* pid's become bogus?? */
	    fs_give((void **) &ps_global->post);
	}
    }

    return(0);
}
#endif
