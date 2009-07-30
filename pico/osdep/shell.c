#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: shell.c 807 2007-11-09 01:21:33Z hubert@u.washington.edu $";
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

#include "../estruct.h"
#include "../mode.h"
#include "../pico.h"
#include "../edef.h"
#include "../efunc.h"
#include "../keydefs.h"

#include "tty.h"
#include "signals.h"

#ifdef	_WINDOWS
#include "mswin.h"
#endif

#include "shell.h"



/* internal prototypes */
#ifdef	SIGCONT
RETSIGTYPE rtfrmshell(int);
#endif



#ifdef	SIGTSTP
/*
 *  bktoshell - suspend and wait to be woken up
 */
int
bktoshell(int f, int n)
{
    UCS z = CTRL | 'Z';

    if(!(gmode&MDSSPD)){
	unknown_command(z);
	return(FALSE);
    }

    if(Pmaster){
	if(!Pmaster->suspend){
	    unknown_command(z);
	    return(FALSE);
	}

	if((*Pmaster->suspend)() == NO_OP_COMMAND){
	    int rv;
	
	    if(km_popped){
		term.t_mrow = 2;
		curwp->w_ntrows -= 2;
	    }

	    clearcursor();
	    mlerase();
	    rv = (*Pmaster->showmsg)('x');
	    ttresize();
	    picosigs();
	    if(rv)		/* Did showmsg corrupt the display? */
	      pico_refresh(0, 1);	/* Yes, repaint */

	    mpresf = 1;
	    if(km_popped){
		term.t_mrow = 0;
		curwp->w_ntrows += 2;
	    }
	}
	else{
	    ttresize();
	    pclear(0, term.t_nrow);
	    pico_refresh(0, 1);
	}

	return(TRUE);
    }

    if(gmode&MDSPWN){
	char *shell;
	int   dummy;

	vttidy();
	movecursor(0, 0);
	(*term.t_eeop)();
	printf("\n\n\nUse \"exit\" to return to Pi%s\n",
	       (gmode & MDBRONLY) ? "lot" : "co");
	system((shell = (char *)getenv("SHELL")) ? shell : "/bin/csh");
	rtfrmshell(dummy);	/* fixup tty */
    }
    else {
	movecursor(term.t_nrow-1, 0);
	peeol();
	movecursor(term.t_nrow, 0);
	peeol();
	movecursor(term.t_nrow, 0);
	printf("\n\n\nUse \"fg\" to return to Pi%s\n",
	       (gmode & MDBRONLY) ? "lot" : "co");
	ttclose();
	movecursor(term.t_nrow, 0);
	peeol();
	(*term.t_flush)();

	signal(SIGCONT, rtfrmshell);	/* prepare to restart */
	signal(SIGTSTP, SIG_DFL);			/* prepare to stop */
	kill(0, SIGTSTP);
    }

    return(TRUE);
}
#else
#ifdef _WINDOWS
int
bktoshell(int f, int n)
{
    UCS z = CTRL | 'Z';

    if(!(gmode&MDSSPD)){
	unknown_command(z);
	return(FALSE);
    }
    if(Pmaster){
        if(!Pmaster->suspend){
	    unknown_command(z);
	    return(FALSE);
	}
	(*Pmaster->suspend)();
	return(TRUE);
    }
			 
    mswin_minimize();
    return(TRUE);
}
#endif /* _WINDOWS */

#endif /* SIGTSTP */

#ifdef	SIGCONT
/* 
 * rtfrmshell - back from shell, fix modes and return
 */
RETSIGTYPE
rtfrmshell(int sig)
{
    signal(SIGCONT, SIG_DFL);
    ttopen();
    ttresize();
    pclear(0, term.t_nrow);
    pico_refresh(0, 1);
}
#endif /* SIGCONT */
