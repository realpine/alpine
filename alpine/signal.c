#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: signal.c 1025 2008-04-08 22:59:38Z hubert@u.washington.edu $";
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

/*======================================================================
     Different signal handlers for different signals
       - Catches all the abort signals, cleans up tty modes and then coredumps
       - Not much to do for SIGHUP
       - Not much to do for SIGTERM
       - turn SIGWINCH into a KEY_RESIZE command
       - No signals for ^Z/suspend, but do it here anyway
       - Also set up the signal handlers, and hold signals for
         critical imap sections of code.
 ====*/


#include "headers.h"
#include "alpine.h"
#include "signal.h"
#include "status.h"
#include "dispfilt.h"
#include "keymenu.h"
#include "titlebar.h"
#include "mailcmd.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/detach.h"
#include "../pith/pipe.h"
#include "../pith/util.h"
#include "../pith/icache.h"
#include "../pith/newmail.h"
#include "../pith/imap.h"
#include "../pith/adrbklib.h"
#include "../pith/remote.h"
#include "../pith/osdep/pipe.h"


/*
 * Internal Prototypes
 */
static RETSIGTYPE auger_in_signal(int);
void		  init_sighup(void);
void		  end_sighup(void);
RETSIGTYPE	  term_signal(void);
void		  fast_clean_up(void);
static RETSIGTYPE usr2_signal(int);
static RETSIGTYPE winch_signal(int);
static RETSIGTYPE intr_signal(int);
void		  suspend_notice(char *);
void		  suspend_warning(void);



static  int cleanup_called_from_sig_handler;



/*----------------------------------------------------------------------
    Install handlers for all the signals we care to catch
  ----------------------------------------------------------------------*/
void
init_signals(void)
{
    dprint((9, "init_signals()\n"));
    init_sighup();

#ifdef	_WINDOWS
#else
# ifdef DEBUG
#  define CUSHION_SIG	(debug < 7)
# else
#  define CUSHION_SIG	(1)
# endif

    if(CUSHION_SIG){
	signal(SIGILL,  auger_in_signal); 
	signal(SIGTRAP, auger_in_signal);
#   ifdef	SIGEMT
	signal(SIGEMT,  auger_in_signal);
#   endif
	signal(SIGBUS,  auger_in_signal);
	signal(SIGSEGV, auger_in_signal);
	signal(SIGSYS,  auger_in_signal);
	signal(SIGQUIT, auger_in_signal);
	/* Don't catch SIGFPE cause it's rare and we use it in a hack below*/
    }
    
    init_sigwinch();

    /*
     * Set up SIGUSR2 to catch signal from other software using the 
     * c-client to tell us that other access to the folder is being 
     * attempted.  THIS IS A TEST: if it turns out that simply
     * going R/O when another pine is started or the same folder is opened,
     * then we may want to install a smarter handler that uses idle time
     * or even prompts the user to see if it's ok to give up R/O access...
     */
    signal(SIGUSR2, usr2_signal);
    
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, SIG_IGN);

#   ifdef SIGTSTP
    /* Some unexplained behaviour on Ultrix 4.2 (Hardy) seems to be
       resulting in Pine getting sent a SIGTSTP. Ignore it here.
       probably better to ignore it than let it happen in any case
     */
    signal(SIGTSTP, SIG_IGN); 
#   endif /* SIGTSTP */

#   ifdef	SIGCHLD
    signal(SIGCHLD,  child_signal);
#   endif
#endif	/* !_WINDOWS */
}


/*----------------------------------------------------------------------
    Return all signal handling back to normal
  ----------------------------------------------------------------------*/
void
end_signals(int blockem)
{
#ifdef	_WINDOWS

    signal(SIGHUP,  blockem ? SIG_IGN : SIG_DFL);

#else
#ifndef	SIG_ERR
#define	SIG_ERR	(RETSIGTYPE (*)())-1
#endif

    dprint((5, "end_signals(%d)\n", blockem));
    if(signal(SIGILL,  blockem ? SIG_IGN : SIG_DFL) == SIG_ERR){
        fprintf(stderr, "Error resetting signals: %s\n",
                error_description(errno));
        exit(-1);
    }

    signal(SIGTRAP, blockem ? SIG_IGN : SIG_DFL);
#ifdef	SIGEMT
    signal(SIGEMT,  blockem ? SIG_IGN : SIG_DFL);
#endif
    signal(SIGBUS,  blockem ? SIG_IGN : SIG_DFL);
    signal(SIGSEGV, blockem ? SIG_IGN : SIG_DFL);
    signal(SIGSYS,  blockem ? SIG_IGN : SIG_DFL);
#if	defined(SIGWINCH) && defined(TIOCGWINSZ)
    signal(SIGWINCH, blockem ? SIG_IGN : SIG_DFL);
#endif
    signal(SIGQUIT, blockem ? SIG_IGN : SIG_DFL);
#ifdef SIGTSTP
    signal(SIGTSTP, blockem ? SIG_IGN : SIG_DFL);
#endif /* SIGTSTP */
    signal(SIGHUP,  blockem ? SIG_IGN : SIG_DFL);
    signal(SIGTERM, blockem ? SIG_IGN : SIG_DFL);
    signal(SIGINT,  blockem ? SIG_IGN : SIG_DFL);

#endif	/* !_WINDOWS */
}


/*----------------------------------------------------------------------
     Handle signals caused by aborts -- SIGSEGV, SIGILL, etc

Call panic which cleans up tty modes and then core dumps
  ----------------------------------------------------------------------*/
static RETSIGTYPE
auger_in_signal(int sig)
{
    char buf[100];

    end_signals(1);			/* don't catch any more signals */
    imap_flush_passwd_cache(FALSE);

    snprintf(buf, sizeof(buf), "Received abort signal(sig=%d)", sig);
    buf[sizeof(buf)-1] = '\0';

    panic(buf);				/* clean up and get out */

    exit(-1);				/* in case panic doesn't kill us */
}


/*----------------------------------------------------------------------
   Install signal handler to deal with hang up signals -- SIGHUP, SIGTERM
  
  ----------------------------------------------------------------------*/
void
init_sighup(void)
{
#if	!(defined(DOS) && !defined(_WINDOWS))
#if	defined(_WINDOWS) || defined(OS2)
    signal(SIGHUP, (void *) hup_signal);
#else
    signal(SIGHUP, hup_signal);
#endif
#endif
#if	!(defined(DOS) || defined(OS2))
    signal(SIGTERM, term_signal);
#endif
}


/*----------------------------------------------------------------------
   De-Install signal handler to deal with hang up signals -- SIGHUP, SIGTERM
  
  ----------------------------------------------------------------------*/
void
end_sighup(void)
{
#if	!(defined(DOS) && !defined(_WINDOWS))
    signal(SIGHUP, SIG_IGN);
#endif
#if	!(defined(DOS) || defined(OS2))
    signal(SIGTERM, SIG_IGN);
#endif
}


/*----------------------------------------------------------------------
      handle hang up signal -- SIGHUP

Not much to do. Rely on periodic mail file check pointing.
  ----------------------------------------------------------------------*/
RETSIGTYPE
hup_signal(void)
{
    end_signals(1);			/* don't catch any more signals */
    dprint((1, "\n\n** Received SIGHUP **\n\n\n\n"));
    cleanup_called_from_sig_handler = 1;
    fast_clean_up();
#if	defined(DEBUG)
    if(debugfile)
      fclose(debugfile);
#endif 

    _exit(0);				/* cleaning up can crash */
}


/*----------------------------------------------------------------------
      Timeout when no user input for a long, long time.
      Treat it pretty much the same as if we got a HUP.
      Only difference is we sometimes turns the timeout off (when composing).
  ----------------------------------------------------------------------*/
void
user_input_timeout_exit(int to_hours)
{
    char msg[80];

    dprint((1,
	   "\n\n** Exiting: user input timeout (%d hours) **\n\n\n\n",
	   to_hours));
    snprintf(msg, sizeof(msg), _("\n\nAlpine timed out (No user input for %d %s)\n"), to_hours,
	    to_hours > 1 ? "hours" : "hour");
    msg[sizeof(msg)-1] = '\0';
    fast_clean_up();
    end_screen(msg, 0);
    end_titlebar();
    end_keymenu();
    end_keyboard(F_ON(F_USE_FK,ps_global));
    end_tty_driver(ps_global);
    end_signals(0);
#if	defined(DEBUG) && (!defined(DOS) || defined(_WINDOWS))
    if(debugfile)
      fclose(debugfile);
#endif 
    exit(0);
}


/*----------------------------------------------------------------------
      handle terminate signal -- SIGTERM

Not much to do. Rely on periodic mail file check pointing.
  ----------------------------------------------------------------------*/
RETSIGTYPE
term_signal(void)
{
#if !defined(DOS) && !defined(OS2)
    end_signals(1);			/* don't catch any more signals */
    dprint((1, "\n\n** Received SIGTERM **\n\n\n\n"));
    cleanup_called_from_sig_handler = 1;
    fast_clean_up();
#if	defined(DEBUG) && (!defined(DOS) || defined(_WINDOWS))
    if(debugfile)
      fclose(debugfile);
#endif 
    printf(_("\n\nAlpine finished. Received terminate signal\n\n"));
#endif	/* !DOS */
    exit(0);
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
#if !defined(DOS) && !defined(OS2)
    int         i;
    MAILSTREAM *m;
#endif

    dprint((1, "fast_clean_up()\n"));

    if(ps_global->expunge_in_progress){
	PineRaw(0);
	return;
    }

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

    PineRaw(0);

#endif	/* !DOS */

    imap_flush_passwd_cache(TRUE);

    dprint((1, "done with fast_clean_up\n"));
}


#if !defined(DOS) && !defined(OS2)
/*----------------------------------------------------------------------
      handle hang up signal -- SIGUSR2

Not much to do. Rely on periodic mail file check pointing.
  ----------------------------------------------------------------------*/
static RETSIGTYPE
usr2_signal(int sig)
{
    char        c, *mbox, mboxbuf[20];
    int         i;
    MAILSTREAM *stream;
    NETMBX      mb;

    for(i = 0; i < ps_global->s_pool.nstream; i++){
	stream = ps_global->s_pool.streams[i];
	if(stream
	   && sp_flagged(stream, SP_LOCKED)
	   && !sp_dead_stream(stream)
	   && !stream->lock
	   && !stream->rdonly
	   && stream->mailbox
	   && (c = *stream->mailbox) != '{' && c != '*'){
	    pine_mail_check(stream);		/* write latest state   */
	    stream->rdonly = 1;			/* and become read-only */
	    (void) pine_mail_ping(stream);
	    mbox = stream->mailbox;
	    if(!strucmp(stream->mailbox, ps_global->inbox_name)
	       || !strcmp(stream->mailbox, ps_global->VAR_INBOX_PATH)
	       || !strucmp(stream->original_mailbox, ps_global->inbox_name)
	       || !strcmp(stream->original_mailbox, ps_global->VAR_INBOX_PATH))
	      mbox = "INBOX";
	    else if(mail_valid_net_parse(stream->mailbox, &mb) && mb.mailbox)
	      mbox = mb.mailbox;
	    
	    q_status_message1(SM_ASYNC, 3, 7,
	     _("Another email program is accessing %s. Session now Read-Only."),
	     short_str((mbox && *mbox) ? mbox : "folder",
		       mboxbuf, sizeof(mboxbuf), 19, FrontDots));
	    dprint((1, "** folder %s went read-only **\n\n",
			stream->mailbox));
	}
    }
}
#endif


/*----------------------------------------------------------------------
   Install signal handler to deal with window resize signal -- SIGWINCH
  
  ----------------------------------------------------------------------*/
void
init_sigwinch (void)
{
#if	defined(SIGWINCH) && defined(TIOCGWINSZ)
    signal(SIGWINCH, winch_signal);
#endif
}


#if	defined(SIGWINCH) && defined(TIOCGWINSZ)
/*----------------------------------------------------------------------
   Handle window resize signal -- SIGWINCH
  
   The planned strategy is just force a redraw command. This is similar
  to new mail handling which forces a noop command. The signals are
  help until pine reads input. Then a KEY_RESIZE is forced into the command
  stream .
   Note that ready_for_winch is only non-zero inside the read_char function,
  so the longjmp only ever happens there, and it is really just a jump
  from lower down in the function up to the top of that function.  Its
  purpose is to return a KEY_RESIZE from read_char when interrupted
  out of the select lower down in read_char.
  ----------------------------------------------------------------------*/
extern jmp_buf  winch_state;
extern int      ready_for_winch, winch_occured;

static RETSIGTYPE
winch_signal(int sig)
{
    clear_cursor_pos();
    init_sigwinch();
    winch_cleanup();
}
#endif


/*
 * winch_cleanup - 
 */
void
winch_cleanup(void)
{
#if	defined(SIGWINCH) && defined(TIOCGWINSZ)
    if(ready_for_winch)
      longjmp(winch_state, 1);
    else
      winch_occured = 1;
#endif
}


#ifdef	SIGCHLD
/*----------------------------------------------------------------------
   Handle child status change -- SIGCHLD
  
   The strategy here is to install the handler when we spawn a child, and
   to let the system tell us when the child's state has changed.  In the
   mean time, we can do whatever.  Typically, "whatever" is spinning in a
   loop alternating between sleep and new_mail calls intended to keep the
   IMAP stream alive.

  ----------------------------------------------------------------------*/
static short    child_signalled, child_jump;
static jmp_buf  child_state;

RETSIGTYPE
child_signal(int sig)
{
#ifdef	BACKGROUND_POST
    /*
     * reap background posting process
     */
    if(ps_global->post && ps_global->post->pid){
	pid_t pid;
	int   es;

	pid = process_reap(ps_global->post->pid, &es, PR_NOHANG);
	if(pid == ps_global->post->pid){
	    ps_global->post->status = es;
	    ps_global->post->pid = 0;
	    return;
	}
	else if(pid < 0 && errno != EINTR){
	    fs_give((void **) &ps_global->post);
	}
    }
#endif /* BACKGROUND_POST */

    child_signalled = 1;
    if(child_jump)
      longjmp(child_state, 1);
}
#endif /* SIGCHLD */


/*
 * pipe_callback - handle pine-specific pipe setup like child
 *		   signal handler and possibly any display stuff
 * BUG: this function should probably be in a "alpine/pipe.c"
 */
void
pipe_callback(PIPE_S *syspipe, int flags, void *data)
{
#ifdef _WINDOWS
    bitmap_t bitmap;
#endif
    if(flags & OSB_PRE_OPEN){
	dprint((5, "Opening pipe: (%s%s%s%s%s%s)\n",
		   (syspipe->mode & PIPE_WRITE)   ? "W":"", (syspipe->mode & PIPE_READ)  ? "R":"",
		   (syspipe->mode & PIPE_NOSHELL) ? "N":"", (syspipe->mode & PIPE_PROT)  ? "P":"",
		   (syspipe->mode & PIPE_USER)    ? "U":"", (syspipe->mode & PIPE_RESET) ? "T":""));

#ifdef	_WINDOWS
	q_status_message(SM_ORDER, 0, 0,
			 "Waiting for called program to finish...");

	flush_status_messages(1);
	setbitmap(bitmap);
	draw_keymenu(&pipe_cancel_keymenu, bitmap, ps_global->ttyo->screen_cols,
		     1-FOOTER_ROWS(ps_global), 0, FirstMenu);
#else  /* UNIX */

	if(!(syspipe->mode & (PIPE_WRITE | PIPE_READ)) && !(syspipe->mode & PIPE_SILENT)){
	    flush_status_messages(0);		/* just clean up display */
	    ClearScreen();
	    fflush(stdout);
	}

	if(syspipe->mode & PIPE_RESET)
	  ttyfix(0);

#ifdef	SIGCHLD
	/*
	 * Prepare for demise of child.  Use SIGCHLD if it's available so
	 * we can do useful things, like keep the IMAP stream alive, while
	 * we're waiting on the child. The handler may have been changed by
	 * something in the composer, in particular, by an alt_editor call.
	 * So we need to re-set it to child_signal and then set it back
	 * when we're done.
	 */
	child_signalled = child_jump = 0;
	syspipe->chld   = signal(SIGCHLD, child_signal);
#endif
#endif /* UNIX */
    }
    else if(flags & OSB_POST_OPEN){
#ifdef	_WINDOWS

	clearfooter(ps_global);
	ps_global->mangled_footer = 1;

	if((int) data == -2)
	  q_status_message1(SM_ORDER, 2, 3, "Ignoring completion of %s", syspipe->command);

#else  /* UNIX */
	if(!(syspipe->mode & (PIPE_WRITE | PIPE_READ)) && !(syspipe->mode & PIPE_SILENT)){
	    ClearScreen();
	    ps_global->mangled_screen = 1;
	}

	if(syspipe->mode & PIPE_RESET)
	  ttyfix(1);

#ifdef	SIGCHLD
	(void) signal(SIGCHLD,  SIG_DFL);
#endif
#endif /* UNIX */
    }
    else if(flags & OSB_PRE_CLOSE){
#ifdef	SIGCHLD
	/*
	 * this is here so close_system_pipe so it has something
	 * to do while we're waiting on the other end of the
	 * pipe to complete.  When we're in the background for
	 * a shell, the the side effect is pinging
	 */
	RETSIGTYPE (*alarm_sig)();
	int	old_cue = F_ON(F_SHOW_DELAY_CUE, ps_global);

	/*
	 * remember the current SIGALRM handler, and make sure it's
	 * installed when we're finished just in case the longjmp
	 * out of the SIGCHLD handler caused sleep() to lose it.
	 * Don't pay any attention to that man behind the curtain.
	 */
	alarm_sig = signal(SIGALRM, SIG_IGN);
	(void) F_SET(F_SHOW_DELAY_CUE, ps_global, 0);
	ps_global->noshow_timeout = 1;
	while(!child_signalled){
	    /* wake up and prod server */
	    if(!(syspipe->mode & PIPE_NONEWMAIL))
	      new_mail(0, 2,
		       (syspipe->mode & PIPE_RESET) ? NM_NONE : NM_DEFER_SORT);

	    if(!child_signalled){
		if(setjmp(child_state) == 0){
		    child_jump = 1;	/* prepare to wake up */
		    sleep(600);		/* give it 5mins to happend */
		}
		else
		  our_sigunblock(SIGCHLD);
	    }

	    child_jump = 0;
	}

	ps_global->noshow_timeout = 0;
	F_SET(F_SHOW_DELAY_CUE, ps_global, old_cue);
	(void) signal(SIGALRM, alarm_sig);
	(void) signal(SIGCHLD, syspipe->chld);
#endif
    }
    else if(flags & OSB_POST_CLOSE){
	if(syspipe->mode & PIPE_RESET)		/* restore our tty modes */
	  ttyfix(1);

	if(!(syspipe->mode & (PIPE_WRITE | PIPE_READ | PIPE_SILENT))){
	    ClearScreen();			/* No I/O to forked child */
	    ps_global->mangled_screen = 1;
	}
    }
}


/*
 * Command interrupt support.
 */

static RETSIGTYPE
intr_signal(int sig)
{
    ps_global->intr_pending = 1;
}


int
intr_handling_on(void)
{
#ifdef	_WINDOWS
    return 0;				/* No interrupts in Windows */
#else  /* UNIX */
    if(signal(SIGINT, intr_signal) == intr_signal)
      return 0;				/* already installed */

    intr_proc(1);
    if(ps_global && ps_global->ttyo)
      draw_cancel_keymenu();

    return 1;
#endif /* UNIX */
}


void
intr_handling_off(void)
{
#ifdef	_WINDOWS
#else  /* UNIX */
    if(signal(SIGINT, SIG_IGN) == SIG_IGN)	/* already off! */
      return;

    ps_global->intr_pending = 0;
    intr_proc(0);
    if(ps_global && ps_global->ttyo)
      blank_keymenu(ps_global->ttyo->screen_rows - 2, 0);

    ps_global->mangled_footer = 1;
#endif /* UNIX */
}


/*----------------------------------------------------------------------
     Set or reset what needs to be set when coming out of pico to run
     an alternate editor.

   Args:   come_back -- If come_back is 0 we're going out of our environment
		          to set up for an external editor.
		        If come_back is 1 we're coming back into pine.
  ----------------------------------------------------------------------*/
int
ttyfix(int come_back)
{
#if	defined(DEBUG) && (!defined(DOS) || defined(_WINDOWS))
    if(debugfile)
      fflush(debugfile);
#endif

    if(come_back){
#ifdef OS2
	enter_text_mode(NULL);
#endif
	init_screen();
	init_tty_driver(ps_global);
	init_keyboard(F_ON(F_USE_FK,ps_global));
#ifdef OS2
	dont_interrupt();
#endif
	fix_windsize(ps_global);
    }
    else{
	EndInverse();
	end_keyboard(F_ON(F_USE_FK,ps_global));
	end_tty_driver(ps_global);
	end_screen(NULL, 0);
#ifdef OS2
	interrupt_ok();
#endif
    }

    return(0);
}


/*----------------------------------------------------------------------
     Suspend Pine. Reset tty and suspend. Suspend is finished when this returns

   Args:  The pine structure

 Result:  Execution suspended for a while. Screen will need redrawing 
          after this is done.

 Instead of the usual handling of ^Z by catching a signal, we actually read
the ^Z and then clean up the tty driver, then kill ourself to stop, and 
pick up where we left off when execution resumes.
  ----------------------------------------------------------------------*/
UCS
do_suspend(void)
{
    struct pine *pine = ps_global;
    time_t now;
    UCS retval;
#if defined(DOS) || defined(OS2)
    int   result, orig_cols, orig_rows;
#else
    int   isremote;
#endif
#ifdef	DOS
    static char *shell = NULL;
#define	STD_SHELL	"COMMAND.COM"
#else
#ifdef	OS2
    static char *shell = NULL;
#define	STD_SHELL	"CMD.EXE"
#endif
#endif

    if(!have_job_control()){
	bogus_command(ctrl('Z'), F_ON(F_USE_FK, pine) ? "F1" : "?");
	return(NO_OP_COMMAND);
    }

    if(F_OFF(F_CAN_SUSPEND, pine)){
	q_status_message(SM_ORDER | SM_DING, 3, 3,
			 _("Alpine suspension not enabled - see help text"));
	return(NO_OP_COMMAND);
    }

#ifdef	_WINDOWS
    mswin_minimize();
    return(NO_OP_COMMAND);
#else

    isremote = (ps_global->mail_stream && ps_global->mail_stream->mailbox
		&& (*ps_global->mail_stream->mailbox == '{'
		    || (*ps_global->mail_stream->mailbox == '*'
			&& *(ps_global->mail_stream->mailbox + 1) == '{')));

    now = time((time_t *)0);
    dprint((1, "\n\n --- %s - SUSPEND ----  %s",
	       isremote ? "REMOTE" : "LOCAL", ctime(&now)));
    ttyfix(0);

#if defined(DOS) || defined(OS2)
    suspend_notice("exit");
    if (!shell){
	char *p;

	if (!((shell = getenv("SHELL")) || (shell = getenv("COMSPEC"))))
	  shell = STD_SHELL;

	shell = cpystr(shell);			/* copy in free storage */
	for(p = shell; (p = strchr(p, '/')); p++)
	  *p = '\\';
    }

    result = system(shell);
#else
    if(F_ON(F_SUSPEND_SPAWNS, ps_global)){
	PIPE_S *syspipe;

	if((syspipe = open_system_pipe(NULL, NULL, NULL, PIPE_USER|PIPE_RESET,
				      0, pipe_callback, pipe_report_error)) != NULL){
	    suspend_notice("exit");
#ifndef	SIGCHLD
	    if(isremote)
	      suspend_warning();
#endif
	    (void) close_system_pipe(&syspipe, NULL, pipe_callback);
	}
    }
    else{
	suspend_notice("fg");

	if(isremote)
	  suspend_warning();

	stop_process();
    }
#endif	/* DOS */

    now = time((time_t *)0);
    dprint((1, "\n\n ---- RETURN FROM SUSPEND ----  %s",
               ctime(&now)));
    
    ttyfix(1);

#if defined(DOS) || defined(OS2)
    orig_cols = pine->ttyo->screen_cols;
    orig_rows = pine->ttyo->screen_rows;
#endif

    fix_windsize(pine);

#if defined(DOS) || defined(OS2)
    if(orig_cols != pine->ttyo->screen_cols ||
       orig_rows != pine->ttyo->screen_rows)
	retval = KEY_RESIZE;
    else
#endif
      retval = ctrl('L');;
 
#if	defined(DOS) || defined(OS2)
    if(result == -1)
      q_status_message1(SM_ORDER | SM_DING, 3, 4,
			_("Error loading \"%s\""), shell);
#endif

    if(isremote && !pine_mail_ping(ps_global->mail_stream))
      q_status_message(SM_ORDER | SM_DING, 4, 9,
		       _("Suspended for too long, IMAP connection broken"));

    return(retval);
#endif	/* !_WINDOWS */
}



/*----------------------------------------------------------------------
 ----*/
void
suspend_notice(char *s)
{
    printf(_("\nAlpine suspended. Give the \"%s\" command to come back.\n"), s);
    fflush(stdout);
}


/*----------------------------------------------------------------------
 ----*/
void
suspend_warning(void)
{
    puts(_("Warning: Your IMAP connection will be closed if Alpine"));
    puts(_("is suspended for more than 30 minutes\n"));
    fflush(stdout);
}


/*----------------------------------------------------------------------
 ----*/
void
fix_windsize(struct pine *pine)
{
    int old_width = pine->ttyo->screen_cols;

    dprint((9, "fix_windsize()\n"));
    mark_keymenu_dirty();
    mark_status_dirty();
    mark_titlebar_dirty();
    clear_cursor_pos();
    get_windsize(pine->ttyo);

    if(old_width != pine->ttyo->screen_cols)
      clear_index_cache(pine->mail_stream, 0);
}
