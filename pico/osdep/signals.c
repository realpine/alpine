#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: signals.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
#endif

/*
 * ========================================================================
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

#include "../headers.h"

#include "../../pith/charconv/filesys.h"

#include "tty.h"

#include "signals.h"


/* internal prototypes */
#ifndef _WINDOWS
RETSIGTYPE	do_hup_signal(int);
#endif


/*
 * picosigs - Install any handlers for the signals we're interested
 *	      in catching.
 */
void
picosigs(void)
{
#ifndef _WINDOWS
    signal(SIGHUP,  do_hup_signal);	/* deal with SIGHUP */
    signal(SIGTERM, do_hup_signal);	/* deal with SIGTERM */
#ifdef	SIGTSTP
    signal(SIGTSTP, SIG_DFL);
#endif
#if	defined(SIGWINCH) && defined(TIOCGWINSZ)
    signal(SIGWINCH, winch_handler); /* window size changes */
#endif
#endif /* !_WINDOWS */
}




#ifndef _WINDOWS
/*
 * do_hup_signal - jump back in the stack to where we can handle this
 */
RETSIGTYPE
do_hup_signal(int sig)
{
    signal(SIGHUP,  SIG_IGN);			/* ignore further SIGHUP's */
    signal(SIGTERM, SIG_IGN);			/* ignore further SIGTERM's */
    if(Pmaster){
	extern jmp_buf finstate;

	longjmp(finstate, COMP_GOTHUP);
    }
    else{
	/*
	 * if we've been interrupted and the buffer is changed,
	 * save it...
	 */
	if(anycb() == TRUE){			/* time to save */
	    if(curbp->b_fname[0] == '\0'){	/* name it */
		strncpy(curbp->b_fname, "pico.save", sizeof(curbp->b_fname));
		curbp->b_fname[sizeof(curbp->b_fname)-1] = '\0';
	    }
	    else{
		strncat(curbp->b_fname, ".save", sizeof(curbp->b_fname)-strlen(curbp->b_fname)-1);
		curbp->b_fname[sizeof(curbp->b_fname)-1] = '\0';
	    }

	    our_unlink(curbp->b_fname);
	    writeout(curbp->b_fname, TRUE);
	}

	vttidy();
	exit(1);
    }
}
#endif /* !_WINDOWS */


#if	defined(SIGWINCH) && defined(TIOCGWINSZ)
/*
 * winch_handler - handle window change signal
 */
RETSIGTYPE
winch_handler(int sig)
{
    signal(SIGWINCH, winch_handler);
    ttresize();
    if(Pmaster && Pmaster->winch_cleanup && Pmaster->arm_winch_cleanup)
      (*Pmaster->winch_cleanup)();
}
#endif	/* SIGWINCH and friends */



#ifdef	POSIX_SIGNALS
/*----------------------------------------------------------------------
   Reset signals after critical imap code
 ----*/
void
(*posix_signal(int sig_num, RETSIGTYPE (*action)(int)))(int)
{
    struct sigaction new_action, old_action;

    memset((void *)&new_action, 0, sizeof(struct sigaction));
    sigemptyset (&new_action.sa_mask);
    new_action.sa_handler = action;
#ifdef	SA_RESTART
    new_action.sa_flags = SA_RESTART;
#else
    new_action.sa_flags = 0;
#endif
    sigaction(sig_num, &new_action, &old_action);
    return(old_action.sa_handler);
}

int
posix_sigunblock(int mask)
{
    sigset_t sig_mask;

    sigemptyset(&sig_mask);
    sigaddset(&sig_mask, mask);
#if	HAVE_PTHREAD
# define sigprocmask	pthread_sigmask
#endif
    return(sigprocmask(SIG_UNBLOCK, &sig_mask, NULL));
}
#endif /* POSIX_SIGNALS */


#if	defined(sv3) || defined(ct)
/* Placed by rll to handle the rename function not found in AT&T */
int
rename(char *oldname, char *newname)
{
    int rtn;
    char b[NLINE];

    strncpy(b, fname_to_locale(oldname), sizeof(b));
    b[sizeof(b)-1] = '\0';

    if ((rtn = link(b, fname_to_locale(newname))) != 0) {
	perror("Was not able to rename file.");
	return(rtn);
    }

    if ((rtn = our_unlink(b)) != 0)
      perror("Was not able to unlink file.");

    return(rtn);
}
#endif

