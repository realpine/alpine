#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: status.c 1142 2008-08-13 17:22:21Z hubert@u.washington.edu $";
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

/*======================================================================
     status.c
     Functions that manage the status line (third from the bottom)
       - put messages on the queue to be displayed
       - display messages on the queue with timers 
       - check queue to figure out next timeout
       - prompt for yes/no type of questions
  ====*/

#include <system.h>
#include <general.h>

#include "../../../pith/status.h"
#include "../../../pith/helptext.h"
#include "../../../pith/debug.h"
#include "../../../pith/string.h"

#include "alpined.h"




/*----------------------------------------------------------------------
        Put a message for the status line on the queue
  ----------*/
void
q_status_message(int flags, int min_time, int max_time, char *message)
{
    if(!(flags & SM_INFO))
      sml_addmsg(0, message);
}


/*----------------------------------------------------------------------
    Time remaining for current message's minimum display
 ----*/
int
status_message_remaining(void)
{
    return(0);
}



/*----------------------------------------------------------------------
       Update status line, clearing or displaying a message
----------------------------------------------------------------------*/
int
display_message(UCS command)
{
    return(0);
}



/*----------------------------------------------------------------------
     Display all the messages on the queue as quickly as possible
  ----*/
void
flush_status_messages(int skip_last_pause)
{
}
