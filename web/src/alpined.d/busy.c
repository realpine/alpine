#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: signal.c 91 2006-07-28 19:02:07Z mikes@u.washington.edu $";
#endif

/* ========================================================================
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
#include <general.h>

#include "../../../pith/status.h"
#include "../../../pith/busy.h"


/*
 * Turn on a busy alarm.
 */
int
busy_cue(char *msg, percent_done_t pc_func, int init_msg)
{
    if(msg && !strncmp("Moving", msg, 6)){
	strncpy(msg+1, "Moved", 5);
	q_status_message(SM_ORDER, 3, 3, msg+1);
    }

    return(0);
}


/*
 * If final_message was set when busy_cue was called:
 *   and message_pri = -1 -- no final message queued
 *                 else final message queued with min equal to message_pri
 */
void
cancel_busy_cue(int message_pri)
{
}


