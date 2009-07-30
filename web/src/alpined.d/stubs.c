#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: stubs.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
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

#include "../../../c-client/c-client.h"

#include "../../../pith/takeaddr.h"
#include "../../../pith/ldap.h"
#include "../../../pith/debug.h"
#include "../../../pith/osdep/coredump.h"

#include "alpined.h"


/* let cleanup calls know we're screwed */
static int in_panic = 0;

/* input timeout */
static int input_timeout = 0;

/* time of last user-initiated newmail check */
static time_t time_of_input;


void
peMarkInputTime(void)
{
    time_of_input = time((time_t *)0);
}


/********* ../../../pith/newmail.c stub **********/
time_t
time_of_last_input()
{
    return(time_of_input);
}


/********* ../../../pith/conf.c stub **********/
int
set_input_timeout(t)
    int t;
{
    int	old_t = input_timeout;

    input_timeout = t;
    return(old_t);
}


int
get_input_timeout()
{
    return(input_timeout);
}


int
unexpected_pinerc_change()
{
    dprint((1, "Unexpected pinerc change"));
    return(0);			/* always overwrite */
}

/********* ../../../pith/mailcap.c stub **********/
int
exec_mailcap_test_cmd(cmd)
    char *cmd;
{
    return(-1);			/* never succeeds on web server */
}


/******  various other stuff ******/
/*----------------------------------------------------------------------
    panic - call on detected programmatic errors to exit pine

   Args: message -- message to record in debug file and to be printed for user

 Result: The various tty modes are restored
         If debugging is active a core dump will be generated
         Exits Pine

  This is also called from imap routines and fs_get and fs_resize.
  ----*/
void
panic(message)
    char *message;
{
    in_panic = 1;

    syslog(LOG_ERR, message);	/* may not work, but try */

#if	0
    if(ps_global)
      peDestroyUserContext(&ps_global);
#endif

#ifdef	DEBUG
    if(debug > 1)
      coredump();   /*--- If we're debugging get a core dump --*/
#endif

    exit(-1);
    fatal("ffo"); /* BUG -- hack to get fatal out of library in right order*/
}


/*----------------------------------------------------------------------
    panicking - called to test whether we're sunk

   Args: none

  ----*/
int
panicking()
{
    return(in_panic);
}


/*----------------------------------------------------------------------
    exceptional_exit - called to exit under unusual conditions (with no core)

   Args: message -- message to record in debug file and to be printed for user
	 ev -- exit value

  ----*/
void
exceptional_exit(message, ev)
    char *message;
    int   ev;
{
    syslog(LOG_ALERT, message);
    exit(ev);
}


/*----------------------------------------------------------------------
   write argument error to the display...

  Args:  none

 Result: prints help messages
  ----------------------------------------------------------------------*/
void
display_args_err(s, a, err)
    char  *s;
    char **a;
    int    err;
{
    syslog(LOG_INFO, "Arg Error: %s", s);
}
