#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: signal.c 91 2006-07-28 19:02:07Z mikes@u.washington.edu $";
#endif

/* ========================================================================
 * Copyright 2006-2008 University of Washington
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
#include "../../../pith/adrbklib.h"
#include "../../../pith/remote.h"
#include "../../../pith/imap.h"

#include "alpined.h"
#include "debug.h"


static  int cleanup_called_from_sig_handler;

static	RETSIGTYPE	hup_signal(int);
static	RETSIGTYPE	term_signal(int);
static	RETSIGTYPE	auger_in_signal(int);
int			fast_clean_up();
void			end_signals(int);
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
    /* prepare for unexpected exit */
    signal(SIGHUP,  hup_signal);
    signal(SIGTERM, term_signal);

    /* prepare for unforseen problems */
    signal(SIGILL,  auger_in_signal);
    signal(SIGTRAP, auger_in_signal);
#ifdef	SIGEMT
    signal(SIGEMT,  auger_in_signal);
#endif
    signal(SIGBUS,  auger_in_signal);
    signal(SIGSEGV, auger_in_signal);
    signal(SIGSYS,  auger_in_signal);

#if	defined(DEBUG) && defined(SIGUSR1) && defined(SIGUSR2)
    /* Set up SIGUSR2 to {in,de}crement debug level */
    signal(SIGUSR1, usr1_signal);
    signal(SIGUSR2, usr2_signal);
#endif
}


/*----------------------------------------------------------------------
      handle hang up signal -- SIGHUP

Not much to do. Rely on periodic mail file check pointing.
  ----------------------------------------------------------------------*/
static RETSIGTYPE
hup_signal(int sig)
{
    end_signals(1);
    cleanup_called_from_sig_handler = 1;

    while(!fast_clean_up())
      sleep(1);

    if(peSocketName)		/* clean up unix domain socket */
      (void) unlink(peSocketName);

    exceptional_exit("SIGHUP received", 0);
}


/*----------------------------------------------------------------------
      handle terminate signal -- SIGTERM

Not much to do. Rely on periodic mail file check pointing.
  ----------------------------------------------------------------------*/
static RETSIGTYPE
term_signal(int sig)
{
    end_signals(1);
    cleanup_called_from_sig_handler = 1;

    while(!fast_clean_up())
      sleep(1);

    if(peSocketName)		/* clean up unix domain socket */
      (void) unlink(peSocketName);

    exceptional_exit("SIGTERM received", 0);
}


/*----------------------------------------------------------------------
     Handle signals caused by aborts -- SIGSEGV, SIGILL, etc

Call panic which cleans up tty modes and then core dumps
  ----------------------------------------------------------------------*/
static RETSIGTYPE
auger_in_signal(int sig)
{
    end_signals(1);

    if(peSocketName)		/* clean up unix domain socket */
      (void) unlink(peSocketName);

    snprintf(tmp_20k_buf, SIZEOF_20KBUF, "Abort: signal %d", sig);
    panic(tmp_20k_buf);		/* clean up and get out */
    exit(-1);			/* in case panic doesn't kill us */
}


/*----------------------------------------------------------------------
     Handle cleaning up mail streams and tty modes...
Not much to do. Rely on periodic mail file check pointing.  Don't try
cleaning up screen or flushing output since stdout is likely already
gone.  To be safe, though, we'll at least restore the original tty mode.
Also delete any remnant _DATAFILE_ from sending-filters.
  ----------------------------------------------------------------------*/
int
fast_clean_up(void)
{
    int         i;
    MAILSTREAM *m;

    if(ps_global->expunge_in_progress)
      return(0);

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

    if(peSocketName)		/* clean up unix domain socket */
      (void) unlink(peSocketName);

    dprint((1, "done with fast_clean_up\n"));
    return(1);
}



/*----------------------------------------------------------------------
    Return all signal handling back to normal
  ----------------------------------------------------------------------*/
void
end_signals(int blockem)
{
#ifndef	SIG_ERR
#define	SIG_ERR	(RETSIGTYPE (*)())-1
#endif
    if(signal(SIGILL,  blockem ? SIG_IGN : SIG_DFL) != SIG_ERR){
	signal(SIGTRAP, blockem ? SIG_IGN : SIG_DFL);
#ifdef	SIGEMT
	signal(SIGEMT,  blockem ? SIG_IGN : SIG_DFL);
#endif
	signal(SIGBUS,  blockem ? SIG_IGN : SIG_DFL);
	signal(SIGSEGV, blockem ? SIG_IGN : SIG_DFL);
	signal(SIGSYS,  blockem ? SIG_IGN : SIG_DFL);
	signal(SIGHUP,  blockem ? SIG_IGN : SIG_DFL);
	signal(SIGTERM, blockem ? SIG_IGN : SIG_DFL);
	signal(SIGINT,  blockem ? SIG_IGN : SIG_DFL);
    }
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

    setup_imap_debug();

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

    setup_imap_debug();

    dprint((SYSDBG_INFO, "Debug level now %d", debug));

}
#endif


/*
 * Command interrupt support.
 */
int
intr_handling_on(void)
{
    return 0;
}


void
intr_handling_off(void)
{
}
