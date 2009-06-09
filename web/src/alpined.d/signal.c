#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: signal.c 91 2006-07-28 19:02:07Z mikes@u.washington.edu $";
#endif

/* ========================================================================
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
#include <general.h>

#include "../../../c-client/c-client.h"

#include "../../../pith/conf.h"
#include "../../../pith/status.h"
#include "../../../pith/signal.h"
#include "../../../pith/debug.h"

#include "debug.h"


static  int cleanup_called_from_sig_handler;

void	fast_clean_up();
void	init_sighup(void);
static	RETSIGTYPE	auger_in_signal(int);
static	RETSIGTYPE	hup_signal(int);
static	RETSIGTYPE	term_signal(int);
#if	defined(DEBUG) && defined(SIGUSR1) && defined(SIGUSR2)
static	RETSIGTYPE	usr1_signal(int);
static	RETSIGTYPE	usr2_signal(int);
#endif


/*----------------------------------------------------------------------
    Install handlers for all the signals we care to catch
  ----------------------------------------------------------------------*/
void
init_signals(void)
{
    dprint((1, "init_signals()\n"));

    init_sighup();

#if	defined(DEBUG) && defined(SIGUSR1) && defined(SIGUSR2)
    /*
     * Set up SIGUSR2 to {in,de}crement debug level
     */
    signal(SIGUSR1, usr1_signal);
    signal(SIGUSR2, usr2_signal);
#endif
    
}



/*----------------------------------------------------------------------
    Return all signal handling back to normal
  ----------------------------------------------------------------------*/
void
end_signals(int blockem)
{
}


/*----------------------------------------------------------------------
     Handle signals caused by aborts -- SIGSEGV, SIGILL, etc

Call panic which cleans up tty modes and then core dumps
  ----------------------------------------------------------------------*/
static RETSIGTYPE
auger_in_signal(int sig)
{
    dprint((1, "auger_in_signal()\n"));
    panic("Received abort signal");	/* clean up and get out */
    exit(-1);				/* in case panic doesn't kill us */
}


/*----------------------------------------------------------------------
   Install signal handler to deal with hang up signals -- SIGHUP, SIGTERM
  
  ----------------------------------------------------------------------*/
void
init_sighup(void)
{
    signal(SIGHUP, hup_signal);
    signal(SIGTERM, term_signal);
}


/*----------------------------------------------------------------------
   De-Install signal handler to deal with hang up signals -- SIGHUP, SIGTERM
  
  ----------------------------------------------------------------------*/
void
end_sighup(void)
{
    signal(SIGHUP, SIG_IGN);
    signal(SIGTERM, SIG_IGN);
}


/*----------------------------------------------------------------------
      handle hang up signal -- SIGHUP

Not much to do. Rely on periodic mail file check pointing.
  ----------------------------------------------------------------------*/
static RETSIGTYPE
hup_signal(int sig)
{
    end_signals(1);			/* don't catch any more signals */
    cleanup_called_from_sig_handler = 1;
    fast_clean_up();
    exceptional_exit("SIGHUP received", 0);
}


/*----------------------------------------------------------------------
      Timeout when no user input for a long, long time.
      Treat it pretty much the same as if we got a HUP.
      Only difference is we sometimes turns the timeout off (when composing).
  ----------------------------------------------------------------------*/
void
user_input_timeout_exit(int to_hours)
{
}


/*----------------------------------------------------------------------
      handle terminate signal -- SIGTERM

Not much to do. Rely on periodic mail file check pointing.
  ----------------------------------------------------------------------*/
static RETSIGTYPE
term_signal(int sig)
{
    end_signals(1);			/* don't catch any more signals */
    cleanup_called_from_sig_handler = 1;
    fast_clean_up();
    exceptional_exit("SIGTERM received", 0);
}


/*----------------------------------------------------------------------
     Handle cleaning up mail streams and tty modes...
Not much to do. Rely on periodic mail file check pointing.  Don't try
cleaning up screen or flushing output since stdout is likely already
gone.  To be safe, though, we'll at least restore the original tty mode.
Also delete any remnant _DATAFILE_ from sending-filters.
  ----------------------------------------------------------------------*/
void
fast_clean_up(void)
{
    int         i;
    MAILSTREAM *m;
    extern char *peSockName;

    if(peSockName)
      (void) unlink(peSockName);

    if(ps_global->expunge_in_progress)
      return;

    /*
     * This gets rid of temporary cache files for remote addrbooks.
     */
    completely_done_with_adrbks();

    /*
     * This flushes out deferred changes and gets rid of temporary cache
     * files for remote config files.
     */
    if(ps_global->prc){
	if(ps_global->prc->outstanding_pinerc_changes)
	  write_pinerc(ps_global, Main,
		       cleanup_called_from_sig_handler ? WRP_NOUSER : WRP_NONE);

	if(ps_global->prc->rd)
	  rd_close_remdata(&ps_global->prc->rd);
	
	free_pinerc_s(&ps_global->prc);
    }

    /* as does this */
    if(ps_global->post_prc){
	if(ps_global->post_prc->outstanding_pinerc_changes)
	  write_pinerc(ps_global, Post,
		       cleanup_called_from_sig_handler ? WRP_NOUSER : WRP_NONE);

	if(ps_global->post_prc->rd)
	  rd_close_remdata(&ps_global->post_prc->rd);
	
	free_pinerc_s(&ps_global->post_prc);
    }

    /*
     * Can't figure out why this section is inside the ifdef, but no
     * harm leaving it that way, I guess.
     */
#if !defined(DOS) && !defined(OS2)
    for(i = 0; i < ps_global->s_pool.nstream; i++){
	m = ps_global->s_pool.streams[i];
	if(m && !m->lock)
	  pine_mail_actually_close(m);
    }
#endif	/* !DOS */

    imap_flush_passwd_cache(TRUE);

    dprint((1, "done with fast_clean_up\n"));
}



#if	defined(DEBUG) && defined(SIGUSR1) && defined(SIGUSR2)
/*----------------------------------------------------------------------
      handle -- SIGUSR1

  Increment debug level
  ----------------------------------------------------------------------*/
static RETSIGTYPE
usr1_signal(int sig)
{
    if(debug < 11)
      debug++;

    dprint((SYSDBG_INFO, "Debug level now %d", debug));
}

/*----------------------------------------------------------------------
      handle -- SIGUSR2

  Decrement debug level
  ----------------------------------------------------------------------*/
static RETSIGTYPE
usr2_signal(int sig)
{
    if(debug > 0)
      debug--;

    dprint((SYSDBG_INFO, "Debug level now %d", debug));

}
#endif


/*
 * Command interrupt support.
 */
void
intr_handling_on(void)
{
}


void
intr_handling_off(void)
{
}
