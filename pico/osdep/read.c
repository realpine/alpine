#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: read.c 763 2007-10-23 23:37:34Z hubert@u.washington.edu $";
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
 * Keyboard input test and read functions
 */

#include <system.h>		/* os-dep defs/includes */
#include <general.h>		/* generally useful definitions */

#include "../keydefs.h"

#ifndef _WINDOWS
#include "raw.h"
#endif /* !_WINDOWS */

#include "read.h"


static time_t _time_of_last_input;

time_t
time_of_last_input(void)
{
    if(_time_of_last_input == 0)
      _time_of_last_input = time((time_t *) 0);

    return(_time_of_last_input);
}

#ifndef _WINDOWS
#if	HAVE_SELECT

#ifdef	HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif


/*
 *  Check whether or not a character is ready to be read, or time out.
 *  This version uses a select call.
 *
 *   Args: time_out -- number of seconds before it will time out.
 *
 * Result: NO_OP_IDLE:	  timed out before any chars ready, time_out > 25
 *         NO_OP_COMMAND: timed out before any chars ready, time_out <= 25
 *         READ_INTR:	  read was interrupted
 *         READY_TO_READ: input is available
 *         BAIL_OUT:	  reading input failed, time to bail gracefully
 *         PANIC_NOW:	  system call error, die now
 */
UCS
input_ready(int time_out)
{
     struct timeval tmo;
     fd_set         readfds, errfds;
     int	    res;

     fflush(stdout);

     if(time_out > 0){
         /* Check to see if there are bytes to read with a timeout */
	 FD_ZERO(&readfds);
	 FD_ZERO(&errfds);
	 FD_SET(STDIN_FD, &readfds);
	 FD_SET(STDIN_FD, &errfds);
	 tmo.tv_sec  = time_out;
         tmo.tv_usec = 0; 
         res = select(STDIN_FD+1, &readfds, 0, &errfds, &tmo);
         if(res < 0){
             if(errno == EINTR || errno == EAGAIN)
               return(READ_INTR);

	     return(BAIL_OUT);
         }

         if(res == 0){ /* the select timed out */
	     if(getppid() == 1){
		 /* Parent is init! */
	         return(BAIL_OUT);
	     }
	       
	     /*
	      * "15" is the minimum allowed mail check interval.
	      * Anything less, and we're being told to cycle thru
	      * the command loop because some task is pending...
	      */
             return(time_out < IDLE_TIMEOUT ? NO_OP_COMMAND : NO_OP_IDLE);
         }
     }

     _time_of_last_input = time((time_t *) 0);
     return(READY_TO_READ);
}


#elif	HAVE_POLL


#ifdef	HAVE_STROPTS_H
# include <stropts.h>
#endif

#ifdef	HAVE_SYS_POLL_H
# include <poll.h>
#endif

/*
 *  Check whether or not a character is ready to be read, or time out.
 *  This version uses a poll call.
 *
 *   Args: time_out -- number of seconds before it will time out.
 *
 * Result: NO_OP_IDLE:	  timed out before any chars ready, time_out > 25
 *         NO_OP_COMMAND: timed out before any chars ready, time_out <= 25
 *         READ_INTR:	  read was interrupted
 *         READY_TO_READ: input is available
 *         BAIL_OUT:	  reading input failed, time to bail gracefully
 *         PANIC_NOW:	  system call error, die now
 */
UCS
input_ready(int time_out)
{
     struct pollfd pollfd;
     int	   res;

     fflush(stdout);

     if(time_out > 0){
         /* Check to see if there are bytes to read with a timeout */
	 pollfd.fd = STDIN_FD;
	 pollfd.events = POLLIN;
	 res = poll (&pollfd, 1, time_out * 1000);
	 if(res >= 0){				/* status bits OK? */
	     if(pollfd.revents & (POLLERR | POLLNVAL))
	       res = -1;			/* bad news, exit below! */
	     else if(pollfd.revents & POLLHUP)
	       return(BAIL_OUT);
	 }

         if(res < 0){
             if(errno == EINTR || errno == EAGAIN)
               return(READ_INTR);

	     return(PANIC_NOW);
         }

         if(res == 0){ /* the select timed out */
	     if(getppid() == 1){
		 /* Parent is init! */
	         return(BAIL_OUT);
	     }

	     /*
	      * "15" is the minimum allowed mail check interval.
	      * Anything less, and we're being told to cycle thru
	      * the command loop because some task is pending...
	      */
             return(time_out < IDLE_TIMEOUT ? NO_OP_COMMAND : NO_OP_IDLE);
         }
     }

     _time_of_last_input = time((time_t *) 0);
     return(READY_TO_READ);
}

#endif /* HAVE_POLL */


/*
 * Read one character from STDIN.
 *
 * Result:           -- the single character read
 *         READ_INTR -- read was interrupted
 *         BAIL_OUT  -- read error of some sort
 */
int
read_one_char(void)
{
     int	    res;
     unsigned char  c;

     res = read(STDIN_FD, &c, 1);

     if(res <= 0){
	 /*
	  * Error reading from terminal!
	  * The only acceptable failure is being interrupted.  If so,
	  * return a value indicating such...
	  */
	 if(res < 0 && errno == EINTR)
	   return(READ_INTR);
	 else
	   return(BAIL_OUT);
     }

     return((int)c);
}

#else /* _WINDOWS */
int
set_time_of_last_input(void)
{
    _time_of_last_input = time(0L);
    return(0);
}
#endif /* _WINDOWS */
