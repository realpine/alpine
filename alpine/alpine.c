#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: alpine.c 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $";
#endif

/*
 * ========================================================================
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

#include "headers.h"

#include "../pith/newmail.h"
#include "../pith/init.h"
#include "../pith/sort.h"
#include "../pith/options.h"
#include "../pith/list.h"
#include "../pith/conf.h"

#include "osdep/debuging.h"
#include "osdep/termout.gen.h"
#include "osdep/chnge_pw.h"

#include "alpine.h"
#include "mailindx.h"
#include "mailcmd.h"
#include "addrbook.h"
#include "reply.h"
#include "arg.h"
#include "keymenu.h"
#include "status.h"
#include "context.h"
#include "mailview.h"
#include "imap.h"
#include "radio.h"
#include "folder.h"
#include "send.h"
#include "help.h"
#include "titlebar.h"
#include "takeaddr.h"
#include "dispfilt.h"
#include "init.h"
#include "remote.h"
#include "pattern.h"
#include "newuser.h"
#include "setup.h"
#include "adrbkcmd.h"
#include "signal.h"
#include "kblock.h"
#include "ldapconf.h"
#include "roleconf.h"
#include "colorconf.h"
#include "print.h"
#include "after.h"
#include "smime.h"
#include "newmail.h"
#ifndef _WINDOWS
#include "../pico/osdep/raw.h"	/* for STD*_FD */
#endif


#define	PIPED_FD	5			/* Some innocuous desc	    */


/* look for my_timer_period in pico directory for an explanation */
int my_timer_period = ((IDLE_TIMEOUT + 1)*1000);

/* byte count used by our gets routine to keep track */
static unsigned long gets_bytes;


/*
 * Internal prototypes
 */
void     convert_args_to_utf8(struct pine *, ARGDATA_S *);
void	 preopen_stayopen_folders(void);
int	 read_stdin_char(char *);
void	 main_redrawer(void);
void     show_main_screen(struct pine *, int, OtherMenu, struct key_menu *, int, Pos *);
void	 do_menu(int, Pos *, struct key_menu *);
int	 choose_setup_cmd(int, MSGNO_S *, SCROLL_S *);
int	 setup_menu(struct pine *);
void	 do_setup_task(int);
void	 queue_init_errors(struct pine *);
void	 process_init_cmds(struct pine *, char **);
void	 goodnight_gracey(struct pine *, int);
void	 pine_read_progress(GETS_DATA *, unsigned long);
int	 remote_pinerc_failure(void);
void	 dump_supported_options(void);
int      prune_folders_ok(void);
#ifdef	WIN32
char	*pine_user_callback(void);
#endif
#ifdef	_WINDOWS
int	 fkey_mode_callback(int, long);
void	 imap_telemetry_on(void);
void	 imap_telemetry_off(void);
char	*pcpine_help_main(char *);
int	 pcpine_main_cursor(int, long);
#define  main app_main
#endif


typedef struct setup_return_val {
    int cmd;
    int exc;
}SRV_S;


/*
 * strlen of longest label from keymenu, of labels corresponding to
 * commands in the middle of the screen.  9 is length of ListFldrs
 */
#define LONGEST_LABEL 9  /* length of longest label from keymenu */

#define EDIT_EXCEPTION (0x100)


static int   in_panic   = 0;


/*----------------------------------------------------------------------
     main routine -- entry point

  Args: argv, argc -- The command line arguments


 Initialize pine, parse arguments and so on

 If there is a user address on the command line go into send mode and exit,
 otherwise loop executing the various screens in Alpine.

 NOTE: The Windows port def's this to "app_main"
  ----*/

int
main(int argc, char **argv)
{
    ARGDATA_S	 args;
    int		 rv;
    long	 rvl;
    struct pine *pine_state;
    gf_io_t	 stdin_getc = NULL;
    char        *args_for_debug = NULL, *init_pinerc_debugging = NULL;

    /*----------------------------------------------------------------------
          Set up buffering and some data structures
      ----------------------------------------------------------------------*/

    pine_state = new_pine_struct();
    ps_global  = pine_state;

    /*
     * fill in optional pith-offered behavior hooks
     */
    pith_opt_read_msg_prompt	   = read_msg_prompt;
    pith_opt_paint_index_hline	   = paint_index_hline;
    pith_opt_rfc2369_editorial	   = rfc2369_editorial;
    pith_opt_condense_thread_cue   = condensed_thread_cue;
    pith_opt_truncate_sfstr        = truncate_subj_and_from_strings;
    pith_opt_save_and_restore	   = save_and_restore;
    pith_opt_newmail_announce	   = newmail_status_message;
    pith_opt_newmail_check_cue	   = newmail_check_cue;
    pith_opt_checkpoint_cue	   = newmail_check_point_cue;
    pith_opt_icon_text		   = icon_text;
    pith_opt_rd_metadata_name	   = rd_metadata_name;
    pith_opt_remote_pinerc_failure = remote_pinerc_failure;
    pith_opt_reopen_folder	   = ask_mailbox_reopen;
    pith_opt_expunge_prompt	   = expunge_prompt;
    pith_opt_begin_closing	   = expunge_and_close_begins;
    pith_opt_replyto_prompt	   = reply_using_replyto_query;
    pith_opt_reply_to_all_prompt   = reply_to_all_query;
    pith_opt_save_create_prompt	   = create_for_save_prompt;
    pith_opt_daemon_confirm	   = confirm_daemon_send;
    pith_opt_save_size_changed_prompt = save_size_changed_prompt;
    pith_opt_save_index_state	   = setup_index_state;
    pith_opt_filter_pattern_cmd	   = pattern_filter_command;
    pith_opt_get_signature_file	   = get_signature_file;
    pith_opt_pretty_var_name	   = pretty_var_name;
    pith_opt_pretty_feature_name   = pretty_feature_name;
    pith_opt_closing_stream        = titlebar_stream_closing;
    pith_opt_current_expunged	   = mm_expunged_current;
#ifdef	SMIME
    pith_opt_smime_get_passphrase  = smime_get_passphrase;
#endif
#ifdef	ENABLE_LDAP
    pith_opt_save_ldap_entry       = save_ldap_entry;
#endif

    status_message_lock_init();

#if	HAVE_SRANDOM
    /*
     * Seed the random number generator with the date & pid.  Random 
     * numbers are used for new mail notification and bug report id's
     */
    srandom(getpid() + time(0));
#endif

    /* need home directory early */
    get_user_info(&ps_global->ui);

    if(!(pine_state->home_dir = our_getenv("HOME")))
      pine_state->home_dir = cpystr(ps_global->ui.homedir);

#ifdef _WINDOWS
    {
	char *p;

	/* normalize path delimiters */
	for(p = pine_state->home_dir; p = strchr(p, '/'); p++)
	  *p='\\';
    }
#endif /* _WINDOWS */

#ifdef DEBUG
    {   size_t len = 0;
	int   i;
	char *p;
	char *no_args = " <no args>";

	for(i = 0; i < argc; i++)
	  len += (strlen(argv[i] ? argv[i] : "")+3);
	
	if(argc == 1)
	  len += strlen(no_args);
	
	p = args_for_debug = (char *)fs_get((len+2) * sizeof(char));
	*p++ = '\n';
	*p = '\0';

	for(i = 0; i < argc; i++){
	    snprintf(p, len+2-(p-args_for_debug), "%s\"%s\"", i ? " " : "", argv[i] ? argv[i] : "");
	    args_for_debug[len+2-1] = '\0';
	    p += strlen(p);
	}
	
	if(argc == 1){
	    strncat(args_for_debug, no_args, len+2-strlen(args_for_debug)-1);
	    args_for_debug[len+2-1] = '\0';
	}
    }
#endif

    /*----------------------------------------------------------------------
           Parse arguments and initialize debugging
      ----------------------------------------------------------------------*/
    pine_args(pine_state, argc, argv, &args);

#ifndef	_WINDOWS
    if(!isatty(0)){
	/*
	 * monkey with descriptors so our normal tty i/o routines don't
	 * choke...
	 */
	dup2(STDIN_FD, PIPED_FD);	/* redirected stdin to new desc */
	dup2(STDERR_FD, STDIN_FD);	/* rebind stdin to the tty	*/
	stdin_getc = read_stdin_char;
	if(stdin_getc && args.action == aaURL){
	  display_args_err(
  "Cannot read stdin when using -url\nFor mailto URLs, use \'body=\' instead", 
	   NULL, 1);
	  args_help();
	  exit(-1);
	}
    }

#else /* _WINDOWS */
    /*
     * We now have enough information to do some of the basic registry settings.
     */
    if(ps_global->update_registry != UREG_NEVER_SET){
	mswin_reg(MSWR_OP_SET
		  | ((ps_global->update_registry == UREG_ALWAYS_SET)
		     ? MSWR_OP_FORCE : 0),
		  MSWR_PINE_DIR, ps_global->pine_dir, (size_t)NULL);
	mswin_reg(MSWR_OP_SET
		  | ((ps_global->update_registry == UREG_ALWAYS_SET)
		     ? MSWR_OP_FORCE : 0),
		  MSWR_PINE_EXE, ps_global->pine_name, (size_t)NULL);
    }

#endif /* _WINDOWS */

    if(ps_global->convert_sigs &&
       (!ps_global->pinerc || !ps_global->pinerc[0])){
	fprintf(stderr, "Use -p <pinerc> with -convert_sigs\n");
	exit(-1);
    }

    /* set some default timeouts in case pinerc is remote */
    mail_parameters(NULL, SET_OPENTIMEOUT, (void *)(long)30);
    mail_parameters(NULL, SET_READTIMEOUT, (void *)(long)15);
    mail_parameters(NULL, SET_TIMEOUT, (void *) pine_tcptimeout);
    /* could be TO_BAIL_THRESHOLD, 15 seems more appropriate for now */
    pine_state->tcp_query_timeout = 15;

    mail_parameters(NULL, SET_SENDCOMMAND, (void *) pine_imap_cmd_happened);
    mail_parameters(NULL, SET_FREESTREAMSPAREP, (void *) sp_free_callback);
    mail_parameters(NULL, SET_FREEELTSPAREP,    (void *) free_pine_elt);
#ifdef	SMIME
    mail_parameters(NULL, SET_FREEBODYSPAREP,   (void *) free_smime_body_sparep);
#endif

    init_pinerc(pine_state, &init_pinerc_debugging);

#ifdef DEBUG
    /* Since this is specific debugging we don't mind if the
       ifdef is the type of system.
     */
#if defined(HAVE_SMALLOC) || defined(NXT)
    if(ps_global->debug_malloc)
      malloc_debug(ps_global->debug_malloc);
#endif
#ifdef	CSRIMALLOC
    if(ps_global->debug_malloc)
      mal_debug(ps_global->debug_malloc);
#endif

    if(!ps_global->convert_sigs
#ifdef _WINDOWS
       && !ps_global->install_flag
#endif /* _WINDOWS */
	)
      init_debug();

    if(args_for_debug){
	dprint((0, " %s (PID=%ld)\n\n", args_for_debug,
	       (long) getpid()));
	fs_give((void **)&args_for_debug);
    }

    {
	char *env_to_free;
	if((env_to_free = our_getenv("HOME")) != NULL){
	    dprint((2, "Setting home dir from $HOME: \"%s\"\n",
		    env_to_free));
	    fs_give((void **)&env_to_free);
	}
	else{
	    dprint((2, "Setting home dir: \"%s\"\n",
		    pine_state->home_dir ? pine_state->home_dir : "<?>"));
	}
    }

    /* Watch out. Sensitive information in debug file. */
    if(ps_global->debug_imap > 4)
      mail_parameters(NULL, SET_DEBUGSENSITIVE, (void *) TRUE);

#ifndef DEBUGJOURNAL
    if(ps_global->debug_tcp)
#endif
      mail_parameters(NULL, SET_TCPDEBUG, (void *) TRUE);

#ifdef	_WINDOWS
    mswin_setdebug(debug, debugfile);
    mswin_setdebugoncallback (imap_telemetry_on);
    mswin_setdebugoffcallback (imap_telemetry_off);
    mswin_enableimaptelemetry(ps_global->debug_imap != 0);
#endif
#endif  /* DEBUG */

#ifdef	_WINDOWS
    mswin_setsortcallback(index_sort_callback);
    mswin_setflagcallback(flag_callback);
    mswin_sethdrmodecallback(header_mode_callback);
    mswin_setselectedcallback(any_selected_callback);
    mswin_setzoomodecallback(zoom_mode_callback);
    mswin_setfkeymodecallback(fkey_mode_callback);
#endif

    /*------- Set up c-client drivers -------*/ 
#include "../c-client/linkage.c"

    /*------- ... then tune the drivers just installed -------*/ 
#ifdef	_WINDOWS
    if(_tgetenv(TEXT("HOME")))
      mail_parameters(NULL, SET_HOMEDIR, (void *) pine_state->home_dir);

    mail_parameters(NULL, SET_USERPROMPT, (void *) pine_user_callback);

    /*
     * Sniff the environment for timezone offset.  We need to do this
     * here since Windows needs help figuring out UTC, and will adjust
     * what time() returns based on TZ.  THIS WILL SCREW US because
     * we use time() differences to manage status messages.  So, if 
     * rfc822_date, which calls localtime() and thus needs tzset(),
     * is called while a status message is displayed, it's possible
     * for time() to return a time *before* what we remember as the
     * time we put the status message on the display.  Sheesh.
     */
    tzset();
#else /* !_WINDOWS */
    /*
     * We used to let c-client do this for us automatically, but it declines
     * to do so for root. This forces c-client to establish an environment,
     * even if the uid is 0.
     */
    env_init(ps_global->ui.login, ps_global->ui.homedir);

    /*
     * Install callback to let us know the progress of network reads...
     */
    (void) mail_parameters(NULL, SET_READPROGRESS, (void *)pine_read_progress);
#endif /* !_WINDOWS */

    /*
     * Install callback to handle certificate validation failures,
     * allowing the user to continue if they wish.
     */
    mail_parameters(NULL, SET_SSLCERTIFICATEQUERY, (void *) pine_sslcertquery);
    mail_parameters(NULL, SET_SSLFAILURE, (void *) pine_sslfailure);

    if(init_pinerc_debugging){
	dprint((2, init_pinerc_debugging));
	fs_give((void **)&init_pinerc_debugging);
    }

    /*
     * Initial allocation of array of stream pool pointers.
     * We do this before init_vars so that we can re-use streams used for
     * remote config files. These sizes may get changed later.
     */
    ps_global->s_pool.max_remstream  = 2;
    dprint((9,
	"Setting initial max_remstream to %d for remote config re-use\n",
	ps_global->s_pool.max_remstream));

    init_vars(pine_state, process_init_cmds);

#ifdef SMIME
    if(F_ON(F_DONT_DO_SMIME, ps_global))
      smime_deinit();
#endif /* SMIME */

#ifdef	ENABLE_NLS
    /*
     * LC_CTYPE is already set from the set_collation call above.
     *
     * We can't use gettext calls before we do this stuff so it doesn't
     * help to translate strings that come before this in the program.
     * Maybe we could rearrange things to accomodate that.
     */
    setlocale(LC_MESSAGES, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(PACKAGE, "UTF-8");
    textdomain(PACKAGE);
#endif	/* ENABLE_NLS */

    convert_args_to_utf8(pine_state, &args);

    if(args.action == aaFolder){
	pine_state->beginning_of_month = first_run_of_month();
	pine_state->beginning_of_year = first_run_of_year();
    }

    /* Set up optional for user-defined display filtering */
    pine_state->tools.display_filter	     = dfilter;
    pine_state->tools.display_filter_trigger = dfilter_trigger;

#ifdef _WINDOWS
    if(ps_global->install_flag){
	init_install_get_vars();

	if(ps_global->prc)
	  free_pinerc_s(&ps_global->prc);

	exit(0);
    }
#endif

    if(ps_global->convert_sigs){
	if(convert_sigs_to_literal(ps_global, 0) == -1){
	    /* TRANSLATORS: sigs refers to signatures, which the user was trying to convert */
	    fprintf(stderr, _("trouble converting sigs\n"));
	    exit(-1);
	}

	if(ps_global->prc){
	    if(ps_global->prc->outstanding_pinerc_changes)
	      write_pinerc(ps_global, Main, WRP_NONE);

	    free_pinerc_s(&pine_state->prc);
	}

	exit(0);
    }

    /*
     * Set up a c-client read timeout and timeout handler.  In general,
     * it shouldn't happen, but a server crash or dead link can cause
     * pine to appear wedged if we don't set this up...
     */
    rv = 30;
    if(pine_state->VAR_TCPOPENTIMEO)
      (void)SVAR_TCP_OPEN(pine_state, rv, tmp_20k_buf, SIZEOF_20KBUF);
    mail_parameters(NULL, SET_OPENTIMEOUT, (void *)(long)rv);

    rv = 15;
    if(pine_state->VAR_TCPREADWARNTIMEO)
      (void)SVAR_TCP_READWARN(pine_state, rv, tmp_20k_buf, SIZEOF_20KBUF);
    mail_parameters(NULL, SET_READTIMEOUT, (void *)(long)rv);

    rv = 0;
    if(pine_state->VAR_TCPWRITEWARNTIMEO){
	if(!SVAR_TCP_WRITEWARN(pine_state, rv, tmp_20k_buf, SIZEOF_20KBUF))
	  if(rv == 0 || rv > 4)				/* making sure */
	    mail_parameters(NULL, SET_WRITETIMEOUT, (void *)(long)rv);
    }

    mail_parameters(NULL, SET_TIMEOUT, (void *) pine_tcptimeout);

    rv = 15;
    if(pine_state->VAR_RSHOPENTIMEO){
	if(!SVAR_RSH_OPEN(pine_state, rv, tmp_20k_buf, SIZEOF_20KBUF))
	  if(rv == 0 || rv > 4)				/* making sure */
	    mail_parameters(NULL, SET_RSHTIMEOUT, (void *)(long)rv);
    }

    rv = 15;
    if(pine_state->VAR_SSHOPENTIMEO){
	if(!SVAR_SSH_OPEN(pine_state, rv, tmp_20k_buf, SIZEOF_20KBUF))
	  if(rv == 0 || rv > 4)				/* making sure */
	    mail_parameters(NULL, SET_SSHTIMEOUT, (void *)(long)rv);
    }

    rvl = 60L;
    if(pine_state->VAR_MAILDROPCHECK){
	if(!SVAR_MAILDCHK(pine_state, rvl, tmp_20k_buf, SIZEOF_20KBUF)){
	    if(rvl == 0L)
	      rvl = (60L * 60L * 24L * 100L);	/* 100 days */

	    if(rvl >= 60L)			/* making sure */
	      mail_parameters(NULL, SET_SNARFINTERVAL, (void *) rvl);
	}
    }

    /*
     * Lookups of long login names which don't exist are very slow in aix.
     * This would normally get set in system-wide config if not needed.
     */
    if(F_ON(F_DISABLE_SHARED_NAMESPACES, ps_global))
      mail_parameters(NULL, SET_DISABLEAUTOSHAREDNS, (void *) TRUE);

    if(F_ON(F_HIDE_NNTP_PATH, ps_global))
      mail_parameters(NULL, SET_NNTPHIDEPATH, (void *) TRUE);

    if(F_ON(F_MAILDROPS_PRESERVE_STATE, ps_global))
      mail_parameters(NULL, SET_SNARFPRESERVE, (void *) TRUE);

    rvl = 0L;
    if(pine_state->VAR_NNTPRANGE){
	if(!SVAR_NNTPRANGE(pine_state, rvl, tmp_20k_buf, SIZEOF_20KBUF))
	  if(rvl > 0L)
	    mail_parameters(NULL, SET_NNTPRANGE, (void *) rvl);
    }

    /*
     * Tell c-client not to be so aggressive about uid mappings
     */
    mail_parameters(NULL, SET_UIDLOOKAHEAD, (void *) 20);

    /*
     * Setup referral handling
     */
    mail_parameters(NULL, SET_IMAPREFERRAL, (void *) imap_referral);
    mail_parameters(NULL, SET_MAILPROXYCOPY, (void *) imap_proxycopy);

    /*
     * Setup multiple newsrc transition
     */
    mail_parameters(NULL, SET_NEWSRCQUERY, (void *) pine_newsrcquery);

    /*
     * Disable some drivers if requested.
     */
    if(ps_global->VAR_DISABLE_DRIVERS &&
       ps_global->VAR_DISABLE_DRIVERS[0] &&
       ps_global->VAR_DISABLE_DRIVERS[0][0]){
	char **t;

	for(t = ps_global->VAR_DISABLE_DRIVERS; t[0] && t[0][0]; t++)
	  if(mail_parameters(NULL, DISABLE_DRIVER, (void *)(*t))){
	      dprint((2, "Disabled mail driver \"%s\"\n", *t));
	  }
	  else{
	      snprintf(tmp_20k_buf, SIZEOF_20KBUF,
		     _("Failed to disable mail driver \"%s\": name not found"),
		      *t);
	      tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	      init_error(ps_global, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
	  }
    }

    /*
     * Disable some authenticators if requested.
     */
    if(ps_global->VAR_DISABLE_AUTHS &&
       ps_global->VAR_DISABLE_AUTHS[0] &&
       ps_global->VAR_DISABLE_AUTHS[0][0]){
	char **t;

	for(t = ps_global->VAR_DISABLE_AUTHS; t[0] && t[0][0]; t++)
	  if(mail_parameters(NULL, DISABLE_AUTHENTICATOR, (void *)(*t))){
	      dprint((2,"Disabled SASL authenticator \"%s\"\n", *t));
	  }
	  else{
	      snprintf(tmp_20k_buf, SIZEOF_20KBUF,
	      _("Failed to disable SASL authenticator \"%s\": name not found"),
		      *t);
	      tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	      init_error(ps_global, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
	  }
    }

    /*
     * setup alternative authentication driver preference for IMAP opens
     */
    if(F_ON(F_PREFER_ALT_AUTH, ps_global))
      mail_parameters(NULL, SET_IMAPTRYALT, (void *) TRUE);

    /*
     * Install handler to let us know about potential delays
     */
    (void) mail_parameters(NULL, SET_BLOCKNOTIFY, (void *) pine_block_notify);

    if(ps_global->dump_supported_options){
	dump_supported_options();
	exit(0);
    }

    /*
     * Install extra headers to fetch along with all the other stuff
     * mail_fetch_structure and mail_fetch_overview requests.
     */
    calc_extra_hdrs();
    if(get_extra_hdrs())
      (void) mail_parameters(NULL, SET_IMAPEXTRAHEADERS,
			     (void *) get_extra_hdrs());

    if(init_username(pine_state) < 0){
        fprintf(stderr, _("Who are you? (Unable to look up login name)\n"));
	exit(-1);
    }

    if(init_userdir(pine_state) < 0)
      exit(-1);

    if(init_hostname(pine_state) < 0)
      exit(-1);
    
    /*
     * Verify mail dir if we're not in send only mode...
     */
    if(args.action == aaFolder && init_mail_dir(pine_state) < 0)
      exit(-1);

    init_signals();

    /*--- input side ---*/
    if(init_tty_driver(pine_state)){
#ifndef _WINDOWS	/* always succeeds under _WINDOWS */
        fprintf(stderr, _("Can't access terminal or input is not a terminal. Redirection of\nstandard input is not allowed. For example \"pine < file\" doesn't work.\n%c"), BELL);
        exit(-1);
#endif /* !_WINDOWS */
    }
        

    /*--- output side ---*/
    rv = config_screen(&(pine_state->ttyo));
#ifndef _WINDOWS	/* always succeeds under _WINDOWS */
    if(rv){
        switch(rv){
          case -1:
	    printf(_("Terminal type (environment variable TERM) not set.\n"));
            break;
          case -2:
	    printf(_("Terminal type \"%s\" is unknown.\n"), getenv("TERM"));
            break;
          case -3:
            printf(_("Can't open terminal capabilities database.\n"));
            break;
          case -4:
            printf(_("Your terminal, of type \"%s\", is lacking functions needed to run alpine.\n"), getenv("TERM"));
            break;
        }

        printf("\r");
        end_tty_driver(pine_state);
        exit(-1);
    }
#endif /* !_WINDOWS */

    if(F_ON(F_BLANK_KEYMENU,ps_global))
      FOOTER_ROWS(ps_global) = 1;

    init_screen();
    init_keyboard(pine_state->orig_use_fkeys);
    strncpy(pine_state->inbox_name, INBOX_NAME,
	    sizeof(pine_state->inbox_name)-1);
    init_folders(pine_state);		/* digest folder spec's */

    pine_state->in_init_seq = 0;	/* so output (& ClearScreen) show up */
    pine_state->dont_use_init_cmds = 1;	/* don't use up initial_commands yet */
    ClearScreen();

    /* initialize titlebar in case we use it */
    set_titlebar("", NULL, NULL, NULL, NULL, 0, FolderName, 0, 0, NULL);

    /*
     * Prep storage object driver for PicoText 
     */
    so_register_external_driver(pine_pico_get, pine_pico_give, pine_pico_writec, pine_pico_readc, 
				pine_pico_puts, pine_pico_seek, NULL, NULL);

#ifdef	DEBUG
    if(ps_global->debug_imap > 4 || debug > 9){
	q_status_message(SM_ORDER | SM_DING, 5, 9,
	      _("Warning: sensitive authentication data included in debug file"));
	flush_status_messages(0);
    }
#endif

    if(args.action == aaPrcCopy || args.action == aaAbookCopy){
	int   exit_val = -1;
	char *err_msg = NULL;

	/*
	 * Don't translate these into UTF-8 because we'll be using them
	 * before we translate next time. User should use ascii.
	 */
	if(args.data.copy.local && args.data.copy.remote){
	    switch(args.action){
	      case aaPrcCopy:
		exit_val = copy_pinerc(args.data.copy.local,
				       args.data.copy.remote, &err_msg);
		break;

	      case aaAbookCopy:
		exit_val = copy_abook(args.data.copy.local,
				      args.data.copy.remote, &err_msg);
		break;

	      default:
		break;
	    }
	}
	if(err_msg){
	  q_status_message(SM_ORDER | SM_DING, 3, 4, err_msg);
	  fs_give((void **)&err_msg);
	}
	goodnight_gracey(pine_state, exit_val);
    }

    if(args.action == aaFolder
       && (pine_state->first_time_user || pine_state->show_new_version)){
	pine_state->mangled_header = 1;
	show_main_screen(pine_state, 0, FirstMenu, &main_keymenu, 0,
			 (Pos *) NULL);
	new_user_or_version(pine_state);
	ClearScreen();
    }
    
    /* put back in case we need to suppress output */
    pine_state->in_init_seq = pine_state->save_in_init_seq;

    /* queue any init errors so they get displayed in a screen below */
    queue_init_errors(ps_global);

    /* "Page" the given file? */
    if(args.action == aaMore){
	int dice = 1, redir = 0;

	if(pine_state->in_init_seq){
	    pine_state->in_init_seq = pine_state->save_in_init_seq = 0;
	    clear_cursor_pos();
	    if(pine_state->free_initial_cmds)
	      fs_give((void **)&(pine_state->free_initial_cmds));

	    pine_state->initial_cmds = NULL;
	}

	/*======= Requested that we simply page the given file =======*/
	if(args.data.file){		/* Open the requested file... */
	    SourceType  src;
	    STORE_S    *store = NULL;
	    char       *decode_error = NULL;
	    char       filename[MAILTMPLEN];

	    if(args.data.file[0] == '\0'){
		HelpType help = NO_HELP;

		pine_state->mangled_footer = 1;
		filename[0] = '\0';
    		while(1){
		    int flags = OE_APPEND_CURRENT;

        	    rv = optionally_enter(filename, -FOOTER_ROWS(pine_state),
					  0, sizeof(filename),
					  /* TRANSLATORS: file is computer data */
					  _("File to open : "),
					  NULL, help, &flags);
        	    if(rv == 3){
			help = (help == NO_HELP) ? h_no_F_arg : NO_HELP;
			continue;
		    }

        	    if(rv != 4)
		      break;
    		}

    		if(rv == 1){
		    q_status_message(SM_ORDER, 0, 2, _("Cancelled"));
		    goodnight_gracey(pine_state, -1);
		} 

		if(*filename){
		    removing_trailing_white_space(filename);
		    removing_leading_white_space(filename);
		    if(is_absolute_path(filename))
		      fnexpand(filename, sizeof(filename));

		    args.data.file = filename;
    		}

		if(!*filename){
					  /* TRANSLATORS: file is computer data */
		    q_status_message(SM_ORDER, 0, 2 ,_("No file to open"));
		    goodnight_gracey(pine_state, -1);
		} 
	    }

	    if(stdin_getc){
		redir++;
		src = CharStar;
		if(isatty(0) && (store = so_get(src, NULL, EDIT_ACCESS))){
		    gf_io_t pc;

		    gf_set_so_writec(&pc, store);
		    gf_filter_init();
		    if((decode_error = gf_pipe(stdin_getc, pc)) != NULL){
			dice = 0;
			q_status_message1(SM_ORDER, 3, 4,
					  _("Problem reading standard input: %s"),
					  decode_error);
		    }

		    gf_clear_so_writec(store);
		}
		else
		  dice = 0;
	    }
	    else{
		src = FileStar;
		strncpy(ps_global->cur_folder, args.data.file,
			sizeof(ps_global->cur_folder)-1);
		ps_global->cur_folder[sizeof(ps_global->cur_folder)-1] = '\0';
		if(!(store = so_get(src, args.data.file, READ_ACCESS|READ_FROM_LOCALE)))
		  dice = 0;
	    }

	    if(dice){
		SCROLL_S sargs;

		memset(&sargs, 0, sizeof(SCROLL_S));
		sargs.text.text = so_text(store);
		sargs.text.src  = src;
		/* TRANSLATORS: file is computer file being read by user */
		sargs.text.desc = _("file");
		/* TRANSLATORS: this is in the title bar at top of screen */
		sargs.bar.title = _("FILE VIEW");
		sargs.bar.style = FileTextPercent;
		sargs.keys.menu = &simple_file_keymenu;
		setbitmap(sargs.keys.bitmap);
		scrolltool(&sargs);

		printf("\n\n");
		so_give(&store);
	    }
	}

	if(!dice){
	    q_status_message2(SM_ORDER, 3, 4,
		_("Can't display \"%s\": %s"),
		 (redir) ? _("Standard Input") 
			 : args.data.file ? args.data.file : "NULL",
		 error_description(errno));
	}

	goodnight_gracey(pine_state, 0);
    }
    else if(args.action == aaMail || (stdin_getc && (args.action != aaURL))){
        /*======= address on command line/send one message mode ============*/
        char	   *to = NULL, *error = NULL, *addr = NULL;
        int	    len, good_addr = 1;
	int	    exit_val = 0;
	BUILDER_ARG fcc;

	if(pine_state->in_init_seq){
	    pine_state->in_init_seq = pine_state->save_in_init_seq = 0;
	    clear_cursor_pos();
	    if(pine_state->free_initial_cmds)
	      fs_give((void **) &(pine_state->free_initial_cmds));

	    pine_state->initial_cmds = NULL;
	}

        /*----- Format the To: line with commas for the composer ---*/
	if(args.data.mail.addrlist){
	    STRLIST_S *p;

	    for(p = args.data.mail.addrlist, len = 0; p; p = p->next)
	      len += strlen(p->name) + 2;

	    to = (char *) fs_get((len + 5) * sizeof(char));
	    for(p = args.data.mail.addrlist, *to = '\0'; p; p = p->next){
		if(*to){
		    strncat(to, ", ", len+5-strlen(to)-1);
		    to[len+5-1] = '\0';
		}

		strncat(to, p->name, len+5-strlen(to)-1);
		to[len+5-1] = '\0';
	    }

	    memset((void *)&fcc, 0, sizeof(BUILDER_ARG));
	    dprint((2, "building addr: -->%s<--\n", to ? to : "?"));
	    good_addr = (build_address(to, &addr, &error, &fcc, NULL) >= 0);
	    dprint((2, "mailing to: -->%s<--\n", addr ? addr : "?"));
	    free_strlist(&args.data.mail.addrlist);
	}
	else
	  memset(&fcc, 0, sizeof(fcc));

	if(good_addr){
	    compose_mail(addr, fcc.tptr, NULL,
			 args.data.mail.attachlist, stdin_getc);
	}
	else{
	    /* TRANSLATORS: refers to bad email address */
	    q_status_message1(SM_ORDER, 3, 4, _("Bad address: %s"), error);
	    exit_val = -1;
	}

	if(addr)
	  fs_give((void **) &addr);

	if(fcc.tptr)
	  fs_give((void **) &fcc.tptr);

	if(args.data.mail.attachlist)
	  free_attachment_list(&args.data.mail.attachlist);

	if(to)
	  fs_give((void **) &to);

	if(error)
	  fs_give((void **) &error);

	goodnight_gracey(pine_state, exit_val);
    }
    else{
	char             int_mail[MAXPATH+1];
        struct key_menu *km = &main_keymenu;

        /*========== Normal pine mail reading mode ==========*/
            
        pine_state->mail_stream    = NULL;
        pine_state->mangled_screen = 1;

	if(args.action == aaURL){
	    url_tool_t f;

	    if(pine_state->in_init_seq){
		pine_state->in_init_seq = pine_state->save_in_init_seq = 0;
		clear_cursor_pos();
		if(pine_state->free_initial_cmds)
		  fs_give((void **) &(pine_state->free_initial_cmds));
		pine_state->initial_cmds = NULL;
	    }
	    if((f = url_local_handler(args.url)) != NULL){
		if(args.data.mail.attachlist){
		    if(f == url_local_mailto){
			if(!(url_local_mailto_and_atts(args.url,
					args.data.mail.attachlist)
			     && pine_state->next_screen))
			  free_attachment_list(&args.data.mail.attachlist);
			  goodnight_gracey(pine_state, 0);
		    }
		    else {
			q_status_message(SM_ORDER | SM_DING, 3, 4,
			 _("Only mailto URLs are allowed with file attachments"));
			goodnight_gracey(pine_state, -1);	/* no return */
		    }
		}
		else if(!((*f)(args.url) && pine_state->next_screen))
		  goodnight_gracey(pine_state, 0);	/* no return */
	    }
	    else{
		q_status_message1(SM_ORDER | SM_DING, 3, 4,
				  _("Unrecognized URL \"%s\""), args.url);
		goodnight_gracey(pine_state, -1);	/* no return */
	    }
	}
	else if(!pine_state->start_in_index){
	    /* flash message about executing initial commands */
	    if(pine_state->in_init_seq){
	        pine_state->in_init_seq    = 0;
		clear_cursor_pos();
		pine_state->mangled_header = 1;
		pine_state->mangled_footer = 1;
		pine_state->mangled_screen = 0;
		/* show that this is Alpine */
		show_main_screen(pine_state, 0, FirstMenu, km, 0, (Pos *)NULL);
		pine_state->mangled_screen = 1;
		pine_state->painted_footer_on_startup = 1;
		if(MIN(4, pine_state->ttyo->screen_rows - 4) > 1){
		  char buf1[6*MAX_SCREEN_COLS+1];
		  char buf2[6*MAX_SCREEN_COLS+1];
		  int  wid;

		  /* TRANSLATORS: Initial Keystroke List is the literal name of an option */
		  strncpy(buf1, _("Executing Initial Keystroke List......"), sizeof(buf1));
		  buf1[sizeof(buf1)-1] = '\0';
		  wid = utf8_width(buf1);
		  if(wid > ps_global->ttyo->screen_cols){
		    utf8_pad_to_width(buf2, buf1, sizeof(buf2), ps_global->ttyo->screen_cols, 1);
	            PutLine0(MIN(4, pine_state->ttyo->screen_rows - 4), 0, buf2);
		  }
		  else{
	            PutLine0(MIN(4, pine_state->ttyo->screen_rows - 4),
		      MAX(MIN(11, pine_state->ttyo->screen_cols - wid), 0), buf1);
		  }
		}

	        pine_state->in_init_seq = 1;
	    }
	    else{
                show_main_screen(pine_state, 0, FirstMenu, km, 0, (Pos *)NULL);
		pine_state->painted_body_on_startup   = 1;
		pine_state->painted_footer_on_startup = 1;
	    }
        }
	else{
	    /* cancel any initial commands, overridden by cmd line */
	    if(pine_state->in_init_seq){
		pine_state->in_init_seq      = 0;
		pine_state->save_in_init_seq = 0;
		clear_cursor_pos();
		if(pine_state->initial_cmds){
		    if(pine_state->free_initial_cmds)
		      fs_give((void **)&(pine_state->free_initial_cmds));

		    pine_state->initial_cmds = NULL;
		}

		F_SET(F_USE_FK,pine_state, pine_state->orig_use_fkeys);
	    }

            (void) do_index_border(pine_state->context_current,
				   pine_state->cur_folder,
				   pine_state->mail_stream,
				   pine_state->msgmap, MsgIndex, NULL,
				   INDX_CLEAR|INDX_HEADER|INDX_FOOTER);
	    pine_state->painted_footer_on_startup = 1;
	    if(MIN(4, pine_state->ttyo->screen_rows - 4) > 1){
	      char buf1[6*MAX_SCREEN_COLS+1];
	      char buf2[6*MAX_SCREEN_COLS+1];
	      int  wid;

	      strncpy(buf1, _("Please wait, opening mail folder......"), sizeof(buf1));
	      buf1[sizeof(buf1)-1] = '\0';
	      wid = utf8_width(buf1);
	      if(wid > ps_global->ttyo->screen_cols){
		utf8_pad_to_width(buf2, buf1, sizeof(buf2), ps_global->ttyo->screen_cols, 1);
		PutLine0(MIN(4, pine_state->ttyo->screen_rows - 4), 0, buf2);
	      }
	      else{
		PutLine0(MIN(4, pine_state->ttyo->screen_rows - 4),
		  MAX(MIN(11, pine_state->ttyo->screen_cols - wid), 0), buf1);
	      }
	    }
        }

        fflush(stdout);

#if !defined(_WINDOWS) && !defined(LEAVEOUTFIFO)
	if(ps_global->VAR_FIFOPATH && ps_global->VAR_FIFOPATH[0])
	  init_newmailfifo(ps_global->VAR_FIFOPATH);
#endif

	if(pine_state->in_init_seq){
	    pine_state->in_init_seq = 0;
	    clear_cursor_pos();
	}

        if(args.action == aaFolder && args.data.folder){
	    CONTEXT_S *cntxt = NULL, *tc = NULL;
	    char       foldername[MAILTMPLEN];
	    int        notrealinbox = 0;

	    if(args.data.folder[0] == '\0'){
		char *fldr;
		unsigned save_def_goto_rule;

		foldername[0] = '\0';
		save_def_goto_rule = pine_state->goto_default_rule;
		pine_state->goto_default_rule = GOTO_FIRST_CLCTN;
		tc = default_save_context(pine_state->context_list);
		fldr = broach_folder(-FOOTER_ROWS(pine_state), 1, &notrealinbox, &tc);
		pine_state->goto_default_rule = save_def_goto_rule;
		if(fldr){
		    strncpy(foldername, fldr, sizeof(foldername)-1);
		    foldername[sizeof(foldername)-1] = '\0';
		}

		if(*foldername){
		    removing_trailing_white_space(foldername);
		    removing_leading_white_space(foldername);
		    args.data.folder = cpystr(foldername);
    		}

		if(!*foldername){
		    q_status_message(SM_ORDER, 0, 2 ,_("No folder to open"));
		    goodnight_gracey(pine_state, -1);
		} 
	    }

	    if(tc)
	      cntxt = tc;
	    else if((rv = pine_state->init_context) < 0)
	      /*
	       * As with almost all the folder vars in the pinerc,
	       * we subvert the collection "breakout" here if the
	       * folder name given looks like an asolute path on
	       * this system...
	       */
	      cntxt = (is_absolute_path(args.data.folder))
			? NULL : pine_state->context_current;
	    else if(rv == 0)
	      cntxt = NULL;
	    else
	      for(cntxt = pine_state->context_list;
		  rv > 1 && cntxt->next;
		  rv--, cntxt = cntxt->next)
		;

	    if(pine_state && pine_state->ttyo){
		blank_keymenu(pine_state->ttyo->screen_rows - 2, 0);
		pine_state->painted_footer_on_startup = 0;
		pine_state->mangled_footer = 1;
	    }

            if(do_broach_folder(args.data.folder, cntxt, NULL, notrealinbox ? 0L : DB_INBOXWOCNTXT) <= 0){
		q_status_message1(SM_ORDER, 3, 4,
		    _("Unable to open folder \"%s\""), args.data.folder);

		goodnight_gracey(pine_state, -1);
	    }
	}
	else if(args.action == aaFolder){
#ifdef _WINDOWS
            /*
	     * need to ask for the inbox name if no default under DOS
	     * since there is no "inbox"
	     */

	    if(!pine_state->VAR_INBOX_PATH || !pine_state->VAR_INBOX_PATH[0]
	       || strucmp(pine_state->VAR_INBOX_PATH, "inbox") == 0){
		HelpType help = NO_HELP;
		static   ESCKEY_S ekey[] = {{ctrl(T), 2, "^T", "To Fldrs"},
					  {-1, 0, NULL, NULL}};

		pine_state->mangled_footer = 1;
		int_mail[0] = '\0';
    		while(1){
		    int flags = OE_APPEND_CURRENT;

        	    rv = optionally_enter(int_mail, -FOOTER_ROWS(pine_state),
				      0, sizeof(int_mail),
				      _("No inbox!  Folder to open as inbox : "),
				      /* ekey */ NULL, help, &flags);
        	    if(rv == 3){
			help = (help == NO_HELP) ? h_sticky_inbox : NO_HELP;
			continue;
		    }

        	    if(rv != 4)
		      break;
    		}

    		if(rv == 1){
		    q_status_message(SM_ORDER, 0, 2 ,_("Folder open cancelled"));
		    rv = 0;		/* reset rv */
		} 
		else if(rv == 2){
                    show_main_screen(pine_state,0,FirstMenu,km,0,(Pos *)NULL);
		}

		if(*int_mail){
		    removing_trailing_white_space(int_mail);
		    removing_leading_white_space(int_mail);
		    if((!pine_state->VAR_INBOX_PATH 
			|| strucmp(pine_state->VAR_INBOX_PATH, "inbox") == 0)
		     /* TRANSLATORS: Inbox-Path and PINERC are literal, not to be translated */
		     && want_to(_("Preserve folder as \"Inbox-Path\" in PINERC"), 
				'y', 'n', NO_HELP, WT_NORM) == 'y'){
			set_variable(V_INBOX_PATH, int_mail, 1, 1, Main);
		    }
		    else{
			if(pine_state->VAR_INBOX_PATH)
			  fs_give((void **)&pine_state->VAR_INBOX_PATH);

			pine_state->VAR_INBOX_PATH = cpystr(int_mail);
		    }

		    if(pine_state && pine_state->ttyo){
			blank_keymenu(pine_state->ttyo->screen_rows - 2, 0);
			pine_state->painted_footer_on_startup = 0;
			pine_state->mangled_footer = 1;
		    }

		    do_broach_folder(pine_state->inbox_name, 
				     pine_state->context_list, NULL, DB_INBOXWOCNTXT);
    		}
		else
		  q_status_message(SM_ORDER, 0, 2 ,_("No folder opened"));

	    }
	    else

#endif /* _WINDOWS */
	    if(F_ON(F_PREOPEN_STAYOPENS, ps_global))
	      preopen_stayopen_folders();

	    if(pine_state && pine_state->ttyo){
		blank_keymenu(pine_state->ttyo->screen_rows - 2, 0);
		pine_state->painted_footer_on_startup = 0;
		pine_state->mangled_footer = 1;
	    }

	    /* open inbox */
            do_broach_folder(pine_state->inbox_name,
			     pine_state->context_list, NULL, DB_INBOXWOCNTXT);
        }

        if(pine_state->mangled_footer)
	  pine_state->painted_footer_on_startup = 0;

        if(args.action == aaFolder
	   && pine_state->mail_stream
	   && expire_sent_mail())
	  pine_state->painted_footer_on_startup = 0;

	/*
	 * Initialize the defaults.  Initializing here means that
	 * if they're remote, the user isn't prompted for an imap login
	 * before the display's drawn, AND there's the chance that
	 * we can climb onto the already opened folder's stream...
	 */
	if(ps_global->first_time_user)
	  init_save_defaults();	/* initialize default save folders */

	build_path(int_mail,
		   ps_global->VAR_OPER_DIR ? ps_global->VAR_OPER_DIR
					   : pine_state->home_dir,
		   INTERRUPTED_MAIL, sizeof(int_mail));
	if(args.action == aaFolder
	   && (folder_exists(NULL, int_mail) & FEX_ISFILE))
	  q_status_message(SM_ORDER | SM_DING, 4, 5, 
		       _("Use Compose command to continue interrupted message."));

#if defined(USE_QUOTAS)
	{
	    long q;
	    int  over;
	    q = disk_quota(pine_state->home_dir, &over);
	    if(q > 0 && over){
		q_status_message2(SM_ASYNC | SM_DING, 4, 5,
		      _("WARNING! Over your disk quota by %s bytes (%s)"),
			      comatose(q),byte_string(q));
	    }
	}
#endif

	pine_state->in_init_seq = pine_state->save_in_init_seq;
	pine_state->dont_use_init_cmds = 0;
	clear_cursor_pos();

	if(pine_state->give_fixed_warning)
	  q_status_message(SM_ASYNC, 0, 10,
/* TRANSLATORS: config is an abbreviation for configuration */
_("Note: some of your config options conflict with site policy and are ignored"));

	if(!prune_folders_ok())
	  q_status_message(SM_ASYNC, 0, 10, 
			   /* TRANSLATORS: Pruned-Folders is literal */
			   _("Note: ignoring Pruned-Folders outside of default collection for saves"));
	
	if(get_input_timeout() == 0 &&
	   ps_global->VAR_INBOX_PATH &&
	   ps_global->VAR_INBOX_PATH[0] == '{')
	  q_status_message(SM_ASYNC, 0, 10,
_("Note: Mail-Check-Interval=0 may cause IMAP server connection to time out"));

#ifdef _WINDOWS
	mswin_setnewmailwidth(ps_global->nmw_width);
#endif


        /*-------------------------------------------------------------------
                         Loop executing the commands
    
            This is done like this so that one command screen can cause
            another one to execute it with out going through the main menu. 
          ------------------------------------------------------------------*/
	if(!pine_state->next_screen)
	  pine_state->next_screen = pine_state->start_in_index
				      ? mail_index_screen : main_menu_screen;
        while(1){
            if(pine_state->next_screen == SCREEN_FUN_NULL) 
              pine_state->next_screen = main_menu_screen;

            (*(pine_state->next_screen))(pine_state);
        }
    }

    exit(0);
}


/*
 * The arguments need to be converted to UTF-8 for our internal use.
 * Not all arguments are converted because some are used before we
 * are able to do the conversion, like the pinerc name.
 */
void
convert_args_to_utf8(struct pine *ps, ARGDATA_S *args)
{
    char  *fromcharset = NULL;
    char  *conv;

    if(args){
	if(ps->keyboard_charmap && strucmp(ps->keyboard_charmap, "UTF-8")
	   &&  strucmp(ps->keyboard_charmap, "US-ASCII"))
	  fromcharset = ps->keyboard_charmap;
	else if(ps->display_charmap && strucmp(ps->display_charmap, "UTF-8")
	   &&  strucmp(ps->display_charmap, "US-ASCII"))
	  fromcharset = ps->display_charmap;
#ifndef	_WINDOWS
	else if(ps->VAR_OLD_CHAR_SET && strucmp(ps->VAR_OLD_CHAR_SET, "UTF-8")
	   &&  strucmp(ps->VAR_OLD_CHAR_SET, "US-ASCII"))
	  fromcharset = ps->VAR_OLD_CHAR_SET;
#endif	/* ! _WINDOWS */

	if(args->action == aaURL && args->url){
	    conv = convert_to_utf8(args->url, fromcharset, 0);
	    if(conv){
		fs_give((void **) &args->url);
		args->url = conv;
	    }
	}

	if(args->action == aaFolder && args->data.folder){
	    conv = convert_to_utf8(args->data.folder, fromcharset, 0);
	    if(conv){
		fs_give((void **) &args->data.folder);
		args->data.folder = conv;
	    }
	}

	if(args->action == aaMore && args->data.file){
	    conv = convert_to_utf8(args->data.file, fromcharset, 0);
	    if(conv){
		fs_give((void **) &args->data.file);
		args->data.file = conv;
	    }
	}

	if(args->action == aaURL || args->action == aaMail){
	    if(args->data.mail.addrlist){
		STRLIST_S *p;

		for(p = args->data.mail.addrlist; p; p=p->next){
		    if(p->name){
			conv = convert_to_utf8(p->name, fromcharset, 0);
			if(conv){
			    fs_give((void **) &p->name);
			    p->name = conv;
			}
		    }
		}
	    }

	    if(args->data.mail.attachlist){
		PATMT *p;

		for(p = args->data.mail.attachlist; p; p=p->next){
		    if(p->filename){
			conv = convert_to_utf8(p->filename, fromcharset, 0);
			if(conv){
			    fs_give((void **) &p->filename);
			    p->filename = conv;
			}
		    }
		}
	    }
	}
    }
}


void
preopen_stayopen_folders(void)
{
    char  **open_these;

    for(open_these = ps_global->VAR_PERMLOCKED;
	open_these && *open_these; open_these++)
      (void) do_broach_folder(*open_these, ps_global->context_list,
			      NULL, DB_NOVISIT);
}


/*
 * read_stdin_char - simple function to return a character from
 *		     redirected stdin
 */
int
read_stdin_char(char *c)
{
    int rv;
    
    /* it'd probably be a good idea to fix this to pre-read blocks */
    while(1){
	rv = read(PIPED_FD, c, 1);
	if(rv < 0){
	    if(errno == EINTR){
		dprint((2, "read_stdin_char: read interrupted, restarting\n"));
		continue;
	    }
	    else
	      dprint((1, "read_stdin_char: read FAILED: %s\n",
			 error_description(errno)));
	}
	break;
    }
    return(rv);
}


/* this default is from the array of structs below */
#define DEFAULT_MENU_ITEM ((unsigned) 3)	/* LIST FOLDERS */
#define ABOOK_MENU_ITEM ((unsigned) 4)		/* ADDRESS BOOK */
#define MAX_MENU_ITEM ((unsigned) 6)
/*
 * Skip this many spaces between rows of main menu screen.
 * We have    MAX_MENU_ITEM+1 = # of commands in menu
 *            1               = copyright line
 *            MAX_MENU_ITEM   = rows between commands
 *            1               = extra row above commands
 *            1               = row between commands and copyright
 *
 * To make it simple, if there is enough room for all of that include all the
 * extra space, if not, cut it all out.
 */
#define MNSKIP(X) (((HEADER_ROWS(X)+FOOTER_ROWS(X)+(MAX_MENU_ITEM+1)+1+MAX_MENU_ITEM+1+1) <= (X)->ttyo->screen_rows) ? 1 : 0)

static unsigned menu_index = DEFAULT_MENU_ITEM;

/*
 * One of these for each line that gets printed in the middle of the
 * screen in the main menu.
 */
static struct menu_key {
    char         *key_and_name,
		 *news_addition;
    int		  key_index;	  /* index into keymenu array for this cmd */
} mkeys[] = {
    /*
     * TRANSLATORS: These next few are headings on the Main alpine menu.
     * It's nice if the dashes can be made to line up vertically.
     */
    {N_(" %s     HELP               -  Get help using Alpine"),
     NULL, MAIN_HELP_KEY},
    {N_(" %s     COMPOSE MESSAGE    -  Compose and send%s a message"),
     /* TRANSLATORS: We think of sending an email message or posting a news message.
        The message is shown as Compose and send/post a message */
     N_("/post"), MAIN_COMPOSE_KEY},
    {N_(" %s     MESSAGE INDEX      -  View messages in current folder"),
     NULL, MAIN_INDEX_KEY},
    {N_(" %s     FOLDER LIST        -  Select a folder%s to view"),
     /* TRANSLATORS: When news is supported the message above becomes
        Select a folder OR news group to view */
     N_(" OR news group"), MAIN_FOLDER_KEY},
    {N_(" %s     ADDRESS BOOK       -  Update address book"),
     NULL, MAIN_ADDRESS_KEY},
    {N_(" %s     SETUP              -  Configure Alpine Options"),
     NULL, MAIN_SETUP_KEY},
    /* TRANSLATORS: final Main menu line */
    {N_(" %s     QUIT               -  Leave the Alpine program"),
     NULL, MAIN_QUIT_KEY}
};



/*----------------------------------------------------------------------
      display main menu and execute main menu commands

    Args: The usual pine structure

  Result: main menu commands are executed


              M A I N   M E N U    S C R E E N

   Paint the main menu on the screen, get the commands and either execute
the function or pass back the name of the function to execute for the menu
selection. Only simple functions that always return here can be executed
here.

This functions handling of new mail, redrawing, errors and such can 
serve as a template for the other screen that do much the same thing.

There is a loop that fetchs and executes commands until a command to leave
this screen is given. Then the name of the next screen to display is
stored in next_screen member of the structure and this function is exited
with a return.

First a check for new mail is performed. This might involve reading the new
mail into the inbox which might then cause the screen to be repainted.

Then the general screen painting is done. This is usually controlled
by a few flags and some other position variables. If they change they
tell this part of the code what to repaint. This will include cursor
motion and so on.
  ----*/
void
main_menu_screen(struct pine *pine_state)
{
    UCS             ch;
    int		    cmd, just_a_navigate_cmd, setup_command, km_popped;
    int             notrealinbox;
    char            *new_folder, *utf8str;
    CONTEXT_S       *tc;
    struct key_menu *km;
    OtherMenu        what;
    Pos              curs_pos;

    ps_global                 = pine_state;
    just_a_navigate_cmd       = 0;
    km_popped		      = 0;
    menu_index = DEFAULT_MENU_ITEM;
    what                      = FirstMenu;  /* which keymenu to display */
    ch                        = 'x'; /* For display_message 1st time through */
    pine_state->next_screen   = SCREEN_FUN_NULL;
    pine_state->prev_screen   = main_menu_screen;
    curs_pos.row = pine_state->ttyo->screen_rows-FOOTER_ROWS(pine_state);
    curs_pos.col = 0;
    km		 = &main_keymenu;

    mailcap_free(); /* free resources we won't be using for a while */

    if(!pine_state->painted_body_on_startup 
       && !pine_state->painted_footer_on_startup){
	pine_state->mangled_screen = 1;
    }

    dprint((1, "\n\n    ---- MAIN_MENU_SCREEN ----\n"));

    while(1){
	if(km_popped){
	    km_popped--;
	    if(km_popped == 0){
		clearfooter(pine_state);
		pine_state->mangled_body = 1;
	    }
	}

	/*
	 * fix up redrawer just in case some submenu caused it to get
	 * reassigned...
	 */
	pine_state->redrawer = main_redrawer;

	/*----------- Check for new mail -----------*/
        if(new_mail(0, NM_TIMING(ch), NM_STATUS_MSG | NM_DEFER_SORT) >= 0)
          pine_state->mangled_header = 1;

        if(streams_died())
          pine_state->mangled_header = 1;

        show_main_screen(pine_state, just_a_navigate_cmd, what, km,
			 km_popped, &curs_pos);
        just_a_navigate_cmd = 0;
	what = SameMenu;

	/*---- This displays new mail notification, or errors ---*/
	if(km_popped){
	    FOOTER_ROWS(pine_state) = 3;
	    mark_status_dirty();
	}

        display_message(ch);
	if(km_popped){
	    FOOTER_ROWS(pine_state) = 1;
	    mark_status_dirty();
	}

	if(F_OFF(F_SHOW_CURSOR, ps_global)){
	    curs_pos.row =pine_state->ttyo->screen_rows-FOOTER_ROWS(pine_state);
	    curs_pos.col =0;
	}

        MoveCursor(curs_pos.row, curs_pos.col);

        /*------ Read the command from the keyboard ----*/      
#ifdef	MOUSE
	mouse_in_content(KEY_MOUSE, -1, -1, 0, 0);
	register_mfunc(mouse_in_content, HEADER_ROWS(pine_state), 0,
		    pine_state->ttyo->screen_rows-(FOOTER_ROWS(pine_state)+1),
		       pine_state->ttyo->screen_cols);
#endif
#if defined(DOS) || defined(OS2)
	/*
	 * AND pre-build header lines.  This works just fine under
	 * DOS since we wait for characters in a loop. Something will
         * will have to change under UNIX if we want to do the same.
	 */
	/* while_waiting = build_header_cache; */
#ifdef	_WINDOWS
	mswin_sethelptextcallback(pcpine_help_main);
	mswin_mousetrackcallback(pcpine_main_cursor);
#endif
#endif
	ch = READ_COMMAND(&utf8str);
#ifdef	MOUSE
	clear_mfunc(mouse_in_content);
#endif
#if defined(DOS) || defined(OS2)
/*	while_waiting = NULL; */
#ifdef	_WINDOWS
	mswin_sethelptextcallback(NULL);
	mswin_mousetrackcallback(NULL);
#endif
#endif

	/* No matter what, Quit here always works */
	if(ch == 'q' || ch == 'Q'){
	    cmd = MC_QUIT;
	}
#ifdef	DEBUG
	else if(debug && ch && ch < 0x80 && strchr("123456789", ch)){
	    int olddebug;

	    olddebug = debug;
	    debug = ch - '0';
	    if(debug > 7)
	      ps_global->debug_timestamp = 1;
	    else
	      ps_global->debug_timestamp = 0;

	    if(debug > 7)
	      ps_global->debug_imap = 4;
	    else if(debug > 6)
	      ps_global->debug_imap = 3;
	    else if(debug > 4)
	      ps_global->debug_imap = 2;
	    else if(debug > 2)
	      ps_global->debug_imap = 1;
	    else
	      ps_global->debug_imap = 0;

	    if(ps_global->mail_stream){
		if(ps_global->debug_imap > 0){
		    mail_debug(ps_global->mail_stream);
#ifdef	_WINDOWS
		    mswin_enableimaptelemetry(TRUE);
#endif
		}
		else{
		    mail_nodebug(ps_global->mail_stream);
#ifdef	_WINDOWS
		    mswin_enableimaptelemetry(FALSE);
#endif
		}
	    }

	    if(debug > 7 && olddebug <= 7)
	      mail_parameters(NULL, SET_TCPDEBUG, (void *) TRUE);
	    else if(debug <= 7 && olddebug > 7 && !ps_global->debugmem)
	      mail_parameters(NULL, SET_TCPDEBUG, (void *) FALSE);

	    dprint((1, "*** Debug level set to %d ***\n", debug));
	    if(debugfile)
	      fflush(debugfile);

	    q_status_message1(SM_ORDER, 0, 1, _("Debug level set to %s"),
			      int2string(debug));
	    continue;
	}
#endif	/* DEBUG */
	else{
	    cmd = menu_command(ch, km);

	    if(km_popped)
	      switch(cmd){
		case MC_NONE :
		case MC_OTHER :
		case MC_RESIZE :
		case MC_REPAINT :
		  km_popped++;
		  break;

		default:
		  clearfooter(pine_state);
		  break;
	      }
	}

	/*------ Execute the command ------*/
	switch (cmd){
help_case :
	    /*------ HELP ------*/
	  case MC_HELP :

	    if(FOOTER_ROWS(pine_state) == 1 && km_popped == 0){
		km_popped = 2;
		pine_state->mangled_footer = 1;
	    }
	    else{
		/* TRANSLATORS: This is a screen title */
		helper(main_menu_tx, _("HELP FOR MAIN MENU"), 0);
		pine_state->mangled_screen = 1;
	    }

	    break;


	    /*---------- display other key bindings ------*/
	  case MC_OTHER :
	    if(ch == 'o')
	      warn_other_cmds();

	    what = NextMenu;
	    pine_state->mangled_footer = 1;
	    break;


	    /*---------- Previous item in menu ----------*/
	  case MC_PREVITEM :
	    if(menu_index > 0) {
		menu_index--;
		pine_state->mangled_body = 1;
		if(km->which == 0)
		  pine_state->mangled_footer = 1;

		just_a_navigate_cmd++;
	    }
	    else
	      /* TRANSLATORS: list refers to list of commands in main menu */
	      q_status_message(SM_ORDER, 0, 2, _("Already at top of list"));

	    break;


	    /*---------- Next item in menu ----------*/
	  case MC_NEXTITEM :
	    if(menu_index < MAX_MENU_ITEM){
		menu_index++;
		pine_state->mangled_body = 1;
		if(km->which == 0)
		  pine_state->mangled_footer = 1;

		just_a_navigate_cmd++;
	    }
	    else
	      q_status_message(SM_ORDER, 0, 2, _("Already at bottom of list"));

	    break;


	    /*---------- Release Notes ----------*/
	  case MC_RELNOTES :
	    /* TRANSLATORS: This is a screen title */
	    helper(h_news, _("ALPINE RELEASE NOTES"), 0);
	    pine_state->mangled_screen = 1;
	    break;


#ifdef KEYBOARD_LOCK
	    /*---------- Keyboard lock ----------*/
	  case MC_KBLOCK :
	    (void) lock_keyboard();
	    pine_state->mangled_screen = 1;
	    break;
#endif /* KEYBOARD_LOCK */


	    /*---------- Quit pine ----------*/
	  case MC_QUIT :
	    pine_state->next_screen = quit_screen;
	    return;

  
	    /*---------- Go to composer ----------*/
	  case MC_COMPOSE :
	    pine_state->next_screen = compose_screen;
	    return;

  
	    /*---- Go to alternate composer ------*/
	  case MC_ROLE :
	    pine_state->next_screen = alt_compose_screen;
	    return;

  
	    /*---------- Top of Folder list ----------*/
	  case MC_COLLECTIONS : 
	    pine_state->next_screen = folder_screen;
	    return;


	    /*---------- Goto new folder ----------*/
	  case MC_GOTO :
	    tc = ps_global->context_current;
	    new_folder = broach_folder(-FOOTER_ROWS(pine_state), 1, &notrealinbox, &tc);
	    if(new_folder)
	      visit_folder(ps_global, new_folder, tc, NULL, notrealinbox ? 0L : DB_INBOXWOCNTXT);

	    return;


	    /*---------- Go to index ----------*/
	  case MC_INDEX :
	    if(THREADING()
	       && sp_viewing_a_thread(pine_state->mail_stream)
	       && unview_thread(pine_state, pine_state->mail_stream,
				pine_state->msgmap)){
		pine_state->view_skipped_index = 0;
		pine_state->mangled_screen = 1;
	    }

	    pine_state->next_screen = mail_index_screen;
	    return;


	    /*---------- Review Status Messages ----------*/
	  case MC_JOURNAL :
	    review_messages();
	    pine_state->mangled_screen = 1;
	    break;


	    /*---------- Setup mini menu ----------*/
	  case MC_SETUP :
setup_case :
	    setup_command = setup_menu(pine_state);
	    pine_state->mangled_footer = 1;
	    do_setup_task(setup_command);
	    if(ps_global->next_screen != main_menu_screen)
	      return;

	    break;


	    /*---------- Go to address book ----------*/
	  case MC_ADDRBOOK :
	    pine_state->next_screen = addr_book_screen;
	    return;


	    /*------ Repaint the works -------*/
	  case MC_RESIZE :
          case MC_REPAINT :
	    ClearScreen();
	    pine_state->mangled_screen = 1;
	    break;

  
#ifdef	MOUSE
	    /*------- Mouse event ------*/
	  case MC_MOUSE :
	    {   
		MOUSEPRESS mp;
		unsigned ndmi;
		struct pine *ps = pine_state;

		mouse_get_last (NULL, &mp);

#ifdef	_WINDOWS
		if(mp.button == M_BUTTON_RIGHT){
		    if(!mp.doubleclick){
			static MPopup main_popup[] = {
			    {tQueue, {"Folder List", lNormal}, {'L'}},
			    {tQueue, {"Message Index", lNormal}, {'I'}},
			    {tSeparator},
			    {tQueue, {"Address Book", lNormal}, {'A'}},
			    {tQueue, {"Setup Options", lNormal}, {'S'}},
			    {tTail}
			};

			mswin_popup(main_popup);
		    }
		}
		else {
#endif
		    if (mp.row >= (HEADER_ROWS(ps) + MNSKIP(ps)))
		      ndmi = (mp.row+1 - HEADER_ROWS(ps) - (MNSKIP(ps)+1))/(MNSKIP(ps)+1);

		    if (mp.row >= (HEADER_ROWS(ps) + MNSKIP(ps))
			&& !(MNSKIP(ps) && (mp.row+1) & 0x01)
			&& ndmi <= MAX_MENU_ITEM
			&& FOOTER_ROWS(ps) + (ndmi+1)*(MNSKIP(ps)+1)
			    + MNSKIP(ps) + FOOTER_ROWS(ps) <= ps->ttyo->screen_rows){
			if(mp.doubleclick){
			    switch(ndmi){	/* fake main_screen request */
			      case 0 :
				goto help_case;

			      case 1 :
				pine_state->next_screen = compose_screen;
				return;

			      case 2 :
				pine_state->next_screen = mail_index_screen;
				return;

			      case 3 :
				pine_state->next_screen = folder_screen;
				return;

			      case 4 :
				pine_state->next_screen = addr_book_screen;
				return;

			      case 5 :
				goto setup_case;

			      case 6 :
				pine_state->next_screen = quit_screen;
				return;

			      default:			/* no op */
				break;
			    }
			}
			else{
			    menu_index = ndmi;
			    pine_state->mangled_body = 1;
			    if(km->which == 0)
			      pine_state->mangled_footer = 1;

			    just_a_navigate_cmd++;
			}
		    }
#ifdef	_WINDOWS
		}
#endif
	    }

	    break;
#endif


	    /*------ Input timeout ------*/
	  case MC_NONE :
            break;	/* noop for timeout loop mail check */


	    /*------ Bogus Input ------*/
          case MC_UNKNOWN :
	    if(ch == 'm' || ch == 'M'){
		q_status_message(SM_ORDER, 0, 1, "Already in Main Menu");
		break;
	    }

	  default:
	    bogus_command(ch, F_ON(F_USE_FK,pine_state) ? "F1" : "?");
	    break;

	  case MC_UTF8:
	    bogus_utf8_command(utf8str, F_ON(F_USE_FK, pine_state) ? "F1" : "?");
	    break;
	 } /* the switch */
    } /* the BIG while loop! */
}


/*----------------------------------------------------------------------
    Re-Draw the main menu

    Args: none.

  Result: main menu is re-displayed
  ----*/
void
main_redrawer(void)
{
    struct key_menu *km = &main_keymenu;

    ps_global->mangled_screen = 1;
    show_main_screen(ps_global, 0, FirstMenu, km, 0, (Pos *)NULL);
}

	
/*----------------------------------------------------------------------
         Draw the main menu

    Args: pine_state - the usual struct
	  quick_draw - tells do_menu() it can skip some drawing
	  what       - tells which section of keymenu to draw
	  km         - the keymenu
	  cursor_pos - returns a good position for the cursor to be located

  Result: main menu is displayed
  ----*/
void
show_main_screen(struct pine *ps, int quick_draw, OtherMenu what,
		 struct key_menu *km, int km_popped, Pos *cursor_pos)
{
    if(ps->painted_body_on_startup || ps->painted_footer_on_startup){
	ps->mangled_screen = 0;		/* only worry about it here */
	ps->mangled_header = 1;		/* we have to redo header */
	if(!ps->painted_body_on_startup)
	  ps->mangled_body = 1;		/* make sure to paint body*/

	if(!ps->painted_footer_on_startup)
	  ps->mangled_footer = 1;	/* make sure to paint footer*/

	ps->painted_body_on_startup   = 0;
        ps->painted_footer_on_startup = 0;
    }

    if(ps->mangled_screen){
	ps->mangled_header = 1;
	ps->mangled_body   = 1;
	ps->mangled_footer = 1;
	ps->mangled_screen = 0;
    }

#ifdef _WINDOWS
    /* Reset the scroll range.  Main screen never scrolls. */
    scroll_setrange (0L, 0L);
    mswin_beginupdate();
#endif

    /* paint the titlebar if needed */
    if(ps->mangled_header){
	/* TRANSLATORS: screen title */
	set_titlebar(_("MAIN MENU"), ps->mail_stream, ps->context_current,
		     ps->cur_folder, ps->msgmap, 1, FolderName, 0, 0, NULL);
	ps->mangled_header = 0;
    }

    /* paint the body if needed */
    if(ps->mangled_body){
	if(!quick_draw)
	  ClearBody();

	do_menu(quick_draw, cursor_pos, km);
	ps->mangled_body = 0;
    }

    /* paint the keymenu if needed */
    if(km && ps->mangled_footer){
	static char label[LONGEST_LABEL + 2 + 1], /* label + brackets + \0 */
		    name[8];
	bitmap_t    bitmap;

	setbitmap(bitmap);

#ifdef KEYBOARD_LOCK
	if(ps_global->restricted || F_ON(F_DISABLE_KBLOCK_CMD,ps_global))
#endif
	  clrbitn(MAIN_KBLOCK_KEY, bitmap);

	menu_clear_binding(km, '>');
	menu_clear_binding(km, '.');
	menu_clear_binding(km, KEY_RIGHT);
	menu_clear_binding(km, ctrl('M'));
	menu_clear_binding(km, ctrl('J'));
	km->keys[MAIN_DEFAULT_KEY].bind
				 = km->keys[mkeys[menu_index].key_index].bind;
	km->keys[MAIN_DEFAULT_KEY].label
				 = km->keys[mkeys[menu_index].key_index].label;

	/* put brackets around the default action */
	snprintf(label, sizeof(label), "[%s]", km->keys[mkeys[menu_index].key_index].label);
	label[sizeof(label)-1] = '\0';
	strncpy(name, ">", sizeof(name));
	name[sizeof(name)-1] = '\0';
	km->keys[MAIN_DEFAULT_KEY].label = label;
	km->keys[MAIN_DEFAULT_KEY].name = name;
	menu_add_binding(km, '>', km->keys[MAIN_DEFAULT_KEY].bind.cmd);
	menu_add_binding(km, '.', km->keys[MAIN_DEFAULT_KEY].bind.cmd);
	menu_add_binding(km, ctrl('M'), km->keys[MAIN_DEFAULT_KEY].bind.cmd);
	menu_add_binding(km, ctrl('J'), km->keys[MAIN_DEFAULT_KEY].bind.cmd);

	if(F_ON(F_ARROW_NAV,ps_global))
	  menu_add_binding(km, KEY_RIGHT, km->keys[MAIN_DEFAULT_KEY].bind.cmd);

	if(km_popped){
	    FOOTER_ROWS(ps) = 3;
	    clearfooter(ps);
	}

	draw_keymenu(km, bitmap, ps_global->ttyo->screen_cols,
		     1-FOOTER_ROWS(ps_global), 0, what);
	ps->mangled_footer = 0;
	if(km_popped){
	    FOOTER_ROWS(ps) = 1;
	    mark_keymenu_dirty();
	}
    }

#ifdef _WINDOWS
    mswin_endupdate();
#endif
}


/*----------------------------------------------------------------------
         Actually display the main menu

    Args: quick_draw - just a next or prev command was typed so we only have
		       to redraw the highlighting
          cursor_pos - a place to return a good value for cursor location

  Result: Main menu is displayed
  ---*/
void
do_menu(int quick_draw, Pos *cursor_pos, struct key_menu *km)
{
    struct pine *ps = ps_global;
    int  dline, indent, longest = 0, cmd;
    char buf[4*MAX_SCREEN_COLS+1];
    char buf2[4*MAX_SCREEN_COLS+1];
    static int last_inverse = -1;
    Pos pos;

    /* find the longest command */
    for(cmd = 0; cmd < sizeof(mkeys)/(sizeof(mkeys[1])); cmd++){
	memset((void *) buf, ' ', sizeof(buf));
        snprintf(buf, sizeof(buf), mkeys[cmd].key_and_name[0] ? _(mkeys[cmd].key_and_name) : "",
		(F_OFF(F_USE_FK,ps)
		 && km->keys[mkeys[cmd].key_index].name)
		   ? km->keys[mkeys[cmd].key_index].name : "",
		(ps->VAR_NEWS_SPEC && mkeys[cmd].news_addition && mkeys[cmd].news_addition[0])
		  ? _(mkeys[cmd].news_addition) : "");
	buf[sizeof(buf)-1] = '\0';

	if(longest < (indent = utf8_width(buf)))
	  longest = indent;
    }

    indent = MAX(((ps->ttyo->screen_cols - longest)/2) - 1, 0);

    dline = HEADER_ROWS(ps) + MNSKIP(ps);
    for(cmd = 0; cmd < sizeof(mkeys)/(sizeof(mkeys[1])); cmd++){
	/* leave room for copyright and footer */
	if(dline + MNSKIP(ps) + 1 + FOOTER_ROWS(ps) >= ps->ttyo->screen_rows)
	  break;

	if(quick_draw && !(cmd == last_inverse || cmd == menu_index)){
	    dline += (1 + MNSKIP(ps));
	    continue;
	}

	if(cmd == menu_index)
	  StartInverse();

	memset((void *) buf, ' ', sizeof(buf));
        snprintf(buf, sizeof(buf), mkeys[cmd].key_and_name[0] ? _(mkeys[cmd].key_and_name) : "",
		(F_OFF(F_USE_FK,ps)
		 && km->keys[mkeys[cmd].key_index].name)
		   ? km->keys[mkeys[cmd].key_index].name : "",
		(ps->VAR_NEWS_SPEC && mkeys[cmd].news_addition && mkeys[cmd].news_addition[0])
		  ? _(mkeys[cmd].news_addition) : "");
	buf[sizeof(buf)-1] = '\0';

	utf8_pad_to_width(buf2, buf, sizeof(buf2),
			  MIN(ps->ttyo->screen_cols-indent,longest+1), 1);
	pos.row = dline++;
	pos.col = indent;
        PutLine0(pos.row, pos.col, buf2);

	if(MNSKIP(ps))
	  dline++;

	if(cmd == menu_index){
	    if(cursor_pos){
		cursor_pos->row = pos.row;
		/* 6 is 1 for the letter plus 5 spaces */
		cursor_pos->col = pos.col + 6;
		if(F_OFF(F_USE_FK,ps))
		  cursor_pos->col++;

		cursor_pos->col = MIN(cursor_pos->col, ps->ttyo->screen_cols);
	    }

	    EndInverse();
	}
    }


    last_inverse = menu_index;

    if(!quick_draw && FOOTER_ROWS(ps)+1 < ps->ttyo->screen_rows){
	utf8_to_width(buf2, LEGAL_NOTICE, sizeof(buf2),
		      ps->ttyo->screen_cols-3, NULL);
	PutLine0(ps->ttyo->screen_rows - (FOOTER_ROWS(ps)+2),
		 MAX(0, ((ps->ttyo->screen_cols-utf8_width(buf2))/2)),
		 buf2);
    }
    if(!quick_draw && FOOTER_ROWS(ps)+1 < ps->ttyo->screen_rows){
	utf8_to_width(buf2, LEGAL_NOTICE2, sizeof(buf2),
		      ps->ttyo->screen_cols-3, NULL);
	PutLine0(ps->ttyo->screen_rows - (FOOTER_ROWS(ps)+1),
		 MAX(0, ((ps->ttyo->screen_cols-utf8_width(buf2))/2)),
		 buf2);
    }

    fflush(stdout);
}


int
choose_setup_cmd(int cmd, MSGNO_S *msgmap, SCROLL_S *sparms)
{
    int rv = 1;
    SRV_S *srv;

    if(!(srv = (SRV_S *)sparms->proc.data.p)){
	sparms->proc.data.p = (SRV_S *)fs_get(sizeof(*srv));
	srv = (SRV_S *)sparms->proc.data.p;
	memset(srv, 0, sizeof(*srv));
    }

    ps_global->next_screen = SCREEN_FUN_NULL;

    switch(cmd){
      case MC_PRINTER :
	srv->cmd = 'p';
	break;

      case MC_PASSWD :
	srv->cmd = 'n';
	break;

      case MC_CONFIG :
	srv->cmd = 'c';
	break;

      case MC_SIG :
	srv->cmd = 's';
	break;

      case MC_ABOOKS :
	srv->cmd = 'a';
	break;

      case MC_CLISTS :
	srv->cmd = 'l';
	break;

      case MC_RULES :
	srv->cmd = 'r';
	break;

      case MC_DIRECTORY :
	srv->cmd = 'd';
	break;

      case MC_KOLOR :
	srv->cmd = 'k';
	break;

      case MC_REMOTE :
	srv->cmd = 'z';
	break;

      case MC_SECURITY :	/* S/MIME setup screen */
	srv->cmd = 'm';
	break;

      case MC_EXCEPT :
	srv->exc = !srv->exc;
	menu_clear_binding(sparms->keys.menu, 'x');
	if(srv->exc){
	  if(sparms->bar.title) fs_give((void **)&sparms->bar.title);
	  /* TRANSLATORS: screen title */
	  sparms->bar.title = cpystr(_("SETUP EXCEPTIONS"));
	  ps_global->mangled_header = 1;
	  /* TRANSLATORS: The reason the X is upper case in eXceptions
	     is because the command key is X. It isn't necessary, just
	     nice if it works. */
	  menu_init_binding(sparms->keys.menu, 'x', MC_EXCEPT, "X",
			    N_("not eXceptions"), SETUP_EXCEPT);
	}
	else{
	  if(sparms->bar.title) fs_give((void **)&sparms->bar.title);
	  /* TRANSLATORS: screen title */
	  sparms->bar.title = cpystr(_("SETUP"));
	  ps_global->mangled_header = 1;
	  menu_init_binding(sparms->keys.menu, 'x', MC_EXCEPT, "X",
			    N_("eXceptions"), SETUP_EXCEPT);
	}

	if(sparms->keys.menu->which == 1)
	  ps_global->mangled_footer = 1;

	rv = 0;
	break;

      case MC_NO_EXCEPT :
#if defined(DOS) || defined(OS2)
        q_status_message(SM_ORDER, 0, 2, _("Need argument \"-x <except_config>\" or \"PINERCEX\" file to use eXceptions"));
#else
        q_status_message(SM_ORDER, 0, 2, _("Need argument \"-x <except_config>\" or \".pinercex\" file to use eXceptions"));
#endif
	rv = 0;
	break;

      default:
	panic("Unexpected command in choose_setup_cmd");
	break;
    }

    return(rv);
}


int
setup_menu(struct pine *ps)
{
    int         ret = 0, exceptions = 0;
    int         printer = 0, passwd = 0, config = 0, sig = 0, dir = 0, smime = 0, exc = 0;
    SCROLL_S	sargs;
    SRV_S      *srv;
    STORE_S    *store;

    if(!(store = so_get(CharStar, NULL, EDIT_ACCESS))){
	q_status_message(SM_ORDER | SM_DING, 3, 3, _("Error allocating space."));
	return(ret);
    }

#if	!defined(DOS)
    if(!ps_global->vars[V_PRINTER].is_fixed)	/* printer can be changed */
      printer++;
#endif

#ifdef	PASSWD_PROG
    if(F_OFF(F_DISABLE_PASSWORD_CMD,ps_global))	/* password is allowed */
      passwd++;
#endif

    if(F_OFF(F_DISABLE_CONFIG_SCREEN,ps_global))	/* config allowed */
      config++;

    if(F_OFF(F_DISABLE_SIGEDIT_CMD,ps_global))	/* .sig editing is allowed */
      sig++;

#ifdef	ENABLE_LDAP
    dir++;
#endif

#ifdef	SMIME
    smime++;
#endif

    if(ps_global->post_prc)
      exc++;

    /* TRANSLATORS: starting here we have a whole screen of help text */
    so_puts(store, _("This is the Setup screen for Alpine. Choose from the following commands:\n"));

    so_puts(store, "\n");
    so_puts(store, _("(E) Exit Setup:\n"));
    so_puts(store, _("    This puts you back at the Main Menu.\n"));

    if(exc){
	so_puts(store, "\n");
	so_puts(store, _("(X) eXceptions:\n"));
	so_puts(store, _("    This command is different from the rest. It is not actually a command\n"));
	so_puts(store, _("    itself. Instead, it is a toggle which modifies the behavior of the\n"));
	so_puts(store, _("    other commands. You toggle Exceptions editing on and off with this\n"));
	so_puts(store, _("    command. When it is off you will be editing (changing) your regular\n"));
	so_puts(store, _("    configuration file. When it is on you will be editing your exceptions\n"));
	so_puts(store, _("    configuration file. For example, you might want to type the command \n"));
	so_puts(store, _("    \"eXceptions\" followed by \"Kolor\" to setup different screen colors\n"));
	so_puts(store, _("    on a particular platform.\n"));
	so_puts(store, _("    (Note: this command does not show up on the keymenu at the bottom of\n"));
	so_puts(store, _("    the screen unless you press \"O\" for \"Other Commands\" --but you don't\n"));
	so_puts(store, _("    need to press the \"O\" in order to invoke the command.)\n"));
    }

    if(printer){
	so_puts(store, "\n");
	so_puts(store, _("(P) Printer:\n"));
	so_puts(store, _("    Allows you to set a default printer and to define custom\n"));
	so_puts(store, _("    print commands.\n"));
    }

    if(passwd){
	so_puts(store, "\n");
	so_puts(store, _("(N) Newpassword:\n"));
	so_puts(store, _("    Change your password.\n"));
    }

    if(config){
	so_puts(store, "\n");
	so_puts(store, _("(C) Config:\n"));
	so_puts(store, _("    Allows you to set or unset many features of Alpine.\n"));
	so_puts(store, _("    You may also set the values of many options with this command.\n"));
    }

    if(sig){
	so_puts(store, "\n");
	so_puts(store, _("(S) Signature:\n"));
	so_puts(store, _("    Enter or edit a custom signature which will\n"));
	so_puts(store, _("    be included with each new message you send.\n"));
    }

    so_puts(store, "\n");
    so_puts(store, _("(A) AddressBooks:\n"));
    so_puts(store, _("    Define a non-default address book.\n"));

    so_puts(store, "\n");
    so_puts(store, _("(L) collectionLists:\n"));
    so_puts(store, _("    You may define groups of folders to help you better organize your mail.\n"));

    so_puts(store, "\n");
    so_puts(store, _("(R) Rules:\n"));
    so_puts(store, _("    This has up to six sub-categories: Roles, Index Colors, Filters,\n"));
    so_puts(store, _("    SetScores, Search, and Other. If the Index Colors option is\n"));
    so_puts(store, _("    missing you may turn it on (if possible) with Setup/Kolor.\n"));
    so_puts(store, _("    If Roles is missing it has probably been administratively disabled.\n"));

    if(dir){
	so_puts(store, "\n");
	so_puts(store, _("(D) Directory:\n"));
	so_puts(store, _("    Define an LDAP Directory server for Alpine's use. A directory server is\n"));
	so_puts(store, _("    similar to an address book, but it is usually maintained by an\n"));
	so_puts(store, _("    organization. It is similar to a telephone directory.\n"));
    }

    so_puts(store, "\n");
    so_puts(store, _("(K) Kolor:\n"));
    so_puts(store, _("    Set custom colors for various parts of the Alpine screens. For example, the\n"));
    so_puts(store, _("    command key labels, the titlebar at the top of each page, and quoted\n"));
    so_puts(store, _("    sections of messages you are viewing.\n"));

    if(smime){
	so_puts(store, "\n");
	so_puts(store, _("(M) S/MIME:\n"));
	so_puts(store, _("    Setup for using S/MIME to verify signed messages, decrypt\n"));
	so_puts(store, _("    encrypted messages, and to sign or encrypt outgoing messages.\n"));
    }

    so_puts(store, "\n");
    so_puts(store, _("(Z) RemoteConfigSetup:\n"));
    so_puts(store, _("    This is a command you will probably only want to use once, if at all.\n"));
    so_puts(store, _("    It helps you transfer your Alpine configuration data to an IMAP server,\n"));
    so_puts(store, _("    where it will be accessible from any of the computers you read mail\n"));
    so_puts(store, _("    from (using Alpine). The idea behind a remote configuration is that you\n"));
    so_puts(store, _("    can change your configuration in one place and have that change show\n"));
    so_puts(store, _("    up on all of the computers you use.\n"));
    so_puts(store, _("    (Note: this command does not show up on the keymenu at the bottom of\n"));
    so_puts(store, _("    the screen unless you press \"O\" for \"Other Commands\" --but you don't\n"));
    so_puts(store, _("    need to press the \"O\" in order to invoke the command.)\n"));

    /* put this down here for people who don't have exceptions */
    if(!exc){
	so_puts(store, "\n");
	so_puts(store, _("(X) eXceptions:\n"));
	so_puts(store, _("    This command is different from the rest. It is not actually a command\n"));
	so_puts(store, _("    itself. Instead, it is a toggle which modifies the behavior of the\n"));
	so_puts(store, _("    other commands. You toggle Exceptions editing on and off with this\n"));
	so_puts(store, _("    command. When it is off you will be editing (changing) your regular\n"));
	so_puts(store, _("    configuration file. When it is on you will be editing your exceptions\n"));
	so_puts(store, _("    configuration file. For example, you might want to type the command \n"));
	so_puts(store, _("    \"eXceptions\" followed by \"Kolor\" to setup different screen colors\n"));
	so_puts(store, _("    on a particular platform.\n"));
	so_puts(store, _("    (Note: this command does not do anything unless you have a configuration\n"));
	so_puts(store, _("    with exceptions enabled (you don't have that). Common ways to enable an\n"));
	so_puts(store, _("    exceptions config are the command line argument \"-x <exception_config>\";\n"));
	so_puts(store, _("    or the existence of the file \".pinercex\" for Unix Alpine, or \"PINERCEX\")\n"));
	so_puts(store, _("    for PC-Alpine.)\n"));
	so_puts(store, _("    (Another note: this command does not show up on the keymenu at the bottom\n"));
	so_puts(store, _("    of the screen unless you press \"O\" for \"Other Commands\" --but you\n"));
	so_puts(store, _("    don't need to press the \"O\" in order to invoke the command.)\n"));
    }

    memset(&sargs, 0, sizeof(SCROLL_S));
    sargs.text.text   = so_text(store);
    sargs.text.src    = CharStar;
    sargs.text.desc   = _("Information About Setup Command");
    sargs.bar.title   = cpystr(_("SETUP"));
    sargs.proc.tool   = choose_setup_cmd;
    sargs.help.text   = NO_HELP;
    sargs.help.title  = NULL;
    sargs.keys.menu   = &choose_setup_keymenu;
    sargs.keys.menu->how_many = 2;

    setbitmap(sargs.keys.bitmap);
    if(!printer)
      clrbitn(SETUP_PRINTER, sargs.keys.bitmap);

    if(!passwd)
      clrbitn(SETUP_PASSWD, sargs.keys.bitmap);

    if(!config)
      clrbitn(SETUP_CONFIG, sargs.keys.bitmap);

    if(!sig)
      clrbitn(SETUP_SIG, sargs.keys.bitmap);

    if(!dir)
      clrbitn(SETUP_DIRECTORY, sargs.keys.bitmap);

    if(!smime)
      clrbitn(SETUP_SMIME, sargs.keys.bitmap);

    if(exc)
      menu_init_binding(sargs.keys.menu, 'x', MC_EXCEPT, "X",
			N_("eXceptions"), SETUP_EXCEPT);
    else
      menu_init_binding(sargs.keys.menu, 'x', MC_NO_EXCEPT, "X",
			N_("eXceptions"), SETUP_EXCEPT);


    scrolltool(&sargs);

    ps->mangled_screen = 1;

    srv = (SRV_S *)sargs.proc.data.p;

    exceptions = srv ? srv->exc : 0;

    so_give(&store);

    if(sargs.bar.title) fs_give((void**)&sargs.bar.title);
    if(srv){
	ret = srv->cmd;
	fs_give((void **)&sargs.proc.data.p);
    }
    else
      ret = 'e';

    return(ret | (exceptions ? EDIT_EXCEPTION : 0));
}


/*----------------------------------------------------------------------

Args: command -- command char to perform

  ----*/
void
do_setup_task(int command)
{
    char *err = NULL;
    int   rtype;
    int   edit_exceptions = 0;
    int   do_lit_sig = 0;

    if(command & EDIT_EXCEPTION){
	edit_exceptions = 1;
	command &= ~EDIT_EXCEPTION;
    }

    switch(command) {
        /*----- EDIT SIGNATURE -----*/
      case 's':
	if(ps_global->VAR_LITERAL_SIG)
	  do_lit_sig = 1;
	else {
	    char sig_path[MAXPATH+1];

	    if(!signature_path(ps_global->VAR_SIGNATURE_FILE, sig_path, MAXPATH))
	      do_lit_sig = 1;
	    else if((!IS_REMOTE(ps_global->VAR_SIGNATURE_FILE)
		     && can_access(sig_path, READ_ACCESS) == 0)
		    ||(IS_REMOTE(ps_global->VAR_SIGNATURE_FILE)
		       && (folder_exists(NULL, sig_path) & FEX_ISFILE)))
	      do_lit_sig = 0;
	    else if(!ps_global->vars[V_SIGNATURE_FILE].main_user_val.p
		    && !ps_global->vars[V_SIGNATURE_FILE].cmdline_val.p
		    && !ps_global->vars[V_SIGNATURE_FILE].fixed_val.p)
	      do_lit_sig = 1;
	    else
	      do_lit_sig = 0;
	}

	if(do_lit_sig){
	    char     *result = NULL;
	    char    **apval;
	    EditWhich ew;
	    int       readonly = 0;

	    ew = edit_exceptions ? ps_global->ew_for_except_vars : Main;

	    if(ps_global->restricted)
	      readonly = 1;
	    else switch(ew){
		   case Main:
		     readonly = ps_global->prc->readonly;
		     break;
		   case Post:
		     readonly = ps_global->post_prc->readonly;
		     break;
		   default:
		     break;
	    }

	    if(readonly)
	      err = cpystr(ps_global->restricted
				     ? "Alpine demo can't change config file"
				     : _("Config file not changeable"));

	    if(!err){
		apval = APVAL(&ps_global->vars[V_LITERAL_SIG], ew);
		if(!apval)
		  err = cpystr(_("Problem accessing configuration"));
		else{
		    char *input;

		    input = (char *)fs_get((strlen(*apval ? *apval : "")+1) *
								sizeof(char));
		    input[0] = '\0';
		    cstring_to_string(*apval, input);
		    err = signature_edit_lit(input, &result,
					     _("SIGNATURE EDITOR"),
					     h_composer_sigedit);
		    fs_give((void **)&input);
		}
	    }

	    if(!err){
		char *cstring_version;

		cstring_version = string_to_cstring(result);

		set_variable(V_LITERAL_SIG, cstring_version, 0, 0, ew);

		if(cstring_version)
		  fs_give((void **)&cstring_version);
	    }

	    if(result)
	      fs_give((void **)&result);
	}
	else
	    err = signature_edit(ps_global->VAR_SIGNATURE_FILE,
				 _("SIGNATURE EDITOR"));

	if(err){
	    q_status_message(SM_ORDER, 3, 4, err);
	    fs_give((void **)&err);
	}

	ps_global->mangled_screen = 1;
	break;

        /*----- ADD ADDRESSBOOK ----*/
      case 'a':
	addr_book_config(ps_global, edit_exceptions);
	menu_index = ABOOK_MENU_ITEM;
	ps_global->mangled_screen = 1;
	break;

#ifdef	ENABLE_LDAP
        /*--- ADD DIRECTORY SERVER --*/
      case 'd':
	directory_config(ps_global, edit_exceptions);
	ps_global->mangled_screen = 1;
	break;
#endif

#ifdef	SMIME
        /*--- S/MIME --*/
      case 'm':
	smime_config_screen(ps_global, edit_exceptions);
	ps_global->mangled_screen = 1;
	break;
#endif

        /*----- CONFIGURE OPTIONS -----*/
      case 'c':
	option_screen(ps_global, edit_exceptions);
	ps_global->mangled_screen = 1;
	break;

        /*----- COLLECTION LIST -----*/
      case 'l':
	folder_config_screen(ps_global, edit_exceptions);
	ps_global->mangled_screen = 1;
	break;

        /*----- RULES -----*/
      case 'r':
	rtype = rule_setup_type(ps_global, RS_RULES | RS_INCFILTNOW,
				_("Type of rule setup : "));
	switch(rtype){
	  case 'r':
	  case 's':
	  case 'i':
	  case 'f':
	  case 'o':
	  case 'c':
	    role_config_screen(ps_global, (rtype == 'r') ? ROLE_DO_ROLES :
					   (rtype == 's') ? ROLE_DO_SCORES :
					    (rtype == 'o') ? ROLE_DO_OTHER :
					     (rtype == 'f') ? ROLE_DO_FILTER :
					      (rtype == 'c') ? ROLE_DO_SRCH :
							        ROLE_DO_INCOLS,
			       edit_exceptions);
	    break;

	  case 'Z':
	    q_status_message(SM_ORDER | SM_DING, 3, 5,
			_("Try turning on color with the Setup/Kolor command."));
	    break;

	  case 'n':
	    role_process_filters();
	    break;

	  default:
	    cmd_cancelled(NULL);
	    break;
	}

	ps_global->mangled_screen = 1;
	break;

        /*----- COLOR -----*/
      case 'k':
	color_config_screen(ps_global, edit_exceptions);
	ps_global->mangled_screen = 1;
	break;

      case 'z':
	convert_to_remote_config(ps_global, edit_exceptions);
	ps_global->mangled_screen = 1;
	break;

        /*----- EXIT -----*/
      case 'e':
        break;

        /*----- NEW PASSWORD -----*/
      case 'n':
#ifdef	PASSWD_PROG
        if(ps_global->restricted){
	    q_status_message(SM_ORDER, 3, 5,
	    "Password change unavailable in restricted demo version of Alpine.");
        }else {
	    change_passwd();
	    ClearScreen();
	    ps_global->mangled_screen = 1;
	}
#else
        q_status_message(SM_ORDER, 0, 5,
		 _("Password changing not configured for this version of Alpine."));
	display_message('x');
#endif	/* DOS */
        break;

#if	!defined(DOS)
        /*----- CHOOSE PRINTER ------*/
      case 'p':
        select_printer(ps_global, edit_exceptions); 
	ps_global->mangled_screen = 1;
        break;
#endif
    }
}


int
rule_setup_type(struct pine *ps, int flags, char *prompt)
{
    ESCKEY_S opts[9];
    int ekey_num = 0, deefault = 0;

    if(flags & RS_INCADDR){
	deefault = 'a';
	opts[ekey_num].ch      = 'a';
	opts[ekey_num].rval    = 'a';
	opts[ekey_num].name    = "A";
	opts[ekey_num++].label = "Addrbook";
    }

  if(flags & RS_RULES){

    if(F_OFF(F_DISABLE_ROLES_SETUP,ps)){ /* roles are allowed */
	if(deefault != 'a')
	  deefault = 'r';

	opts[ekey_num].ch      = 'r';
	opts[ekey_num].rval    = 'r';
	opts[ekey_num].name    = "R";
	opts[ekey_num++].label = "Roles";
    }
    else if(deefault != 'a')
      deefault = 's';

    opts[ekey_num].ch      = 's';
    opts[ekey_num].rval    = 's';
    opts[ekey_num].name    = "S";
    opts[ekey_num++].label = "SetScores";

#ifndef	_WINDOWS
    if(ps->color_style != COL_NONE && pico_hascolor()){
#endif
	if(deefault != 'a')
	  deefault = 'i';

	opts[ekey_num].ch      = 'i';
	opts[ekey_num].rval    = 'i';
	opts[ekey_num].name    = "I";
	opts[ekey_num++].label = "Indexcolor";
#ifndef	_WINDOWS
    }
    else{
	opts[ekey_num].ch      = 'i';
	opts[ekey_num].rval    = 'Z';		/* notice this rval! */
	opts[ekey_num].name    = "I";
	opts[ekey_num++].label = "Indexcolor";
    }
#endif

    opts[ekey_num].ch      = 'f';
    opts[ekey_num].rval    = 'f';
    opts[ekey_num].name    = "F";
    opts[ekey_num++].label = "Filters";

    opts[ekey_num].ch      = 'o';
    opts[ekey_num].rval    = 'o';
    opts[ekey_num].name    = "O";
    opts[ekey_num++].label = "Other";

    opts[ekey_num].ch      = 'c';
    opts[ekey_num].rval    = 'c';
    opts[ekey_num].name    = "C";
    opts[ekey_num++].label = "searCh";

  }

    if(flags & RS_INCEXP){
	opts[ekey_num].ch      = 'e';
	opts[ekey_num].rval    = 'e';
	opts[ekey_num].name    = "E";
	opts[ekey_num++].label = "Export";
    }

    if(flags & RS_INCFILTNOW){
	opts[ekey_num].ch      = 'n';
	opts[ekey_num].rval    = 'n';
	opts[ekey_num].name    = "N";
	opts[ekey_num++].label = "filterNow";
    }

    opts[ekey_num].ch    = -1;

    return(radio_buttons(prompt, -FOOTER_ROWS(ps), opts,
			 deefault, 'x', NO_HELP, RB_NORM));
}



/*
 * Process the command list, changing function key notation into
 * lexical equivalents.
 */
void
process_init_cmds(struct pine *ps, char **list)
{
    char **p;
    int i = 0;
    int j;
    int lpm1;
#define MAX_INIT_CMDS 500
    /* this is just a temporary stack array, the real one is allocated below */
    int i_cmds[MAX_INIT_CMDS];
    int fkeys = 0;
    int not_fkeys = 0;
  
    if(list){
      for(p = list; *p; p++){
	  if(i >= MAX_INIT_CMDS){
	      snprintf(tmp_20k_buf, SIZEOF_20KBUF,
		      "Initial keystroke list too long at \"%s\"", *p);
	      init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
	      break;
	  }
	  

	/* regular character commands */
	if(strlen(*p) == 1){
	  i_cmds[i++] = **p;
	  not_fkeys++;
	}

	/* special commands */
	else if(strucmp(*p, "SPACE") == 0)
	  i_cmds[i++] = ' ';
	else if(strucmp(*p, "CR") == 0)
	  i_cmds[i++] = '\n';
	else if(strucmp(*p, "TAB") == 0)
	  i_cmds[i++] = '\t';
	else if(strucmp(*p, "UP") == 0)
	  i_cmds[i++] = KEY_UP;
	else if(strucmp(*p, "DOWN") == 0)
	  i_cmds[i++] = KEY_DOWN;
	else if(strucmp(*p, "LEFT") == 0)
	  i_cmds[i++] = KEY_LEFT;
	else if(strucmp(*p, "RIGHT") == 0)
	  i_cmds[i++] = KEY_RIGHT;

	/* control chars */
	else if(strlen(*p) == 2 && **p == '^')
	  i_cmds[i++] = ctrl(*((*p)+1));

	/* function keys */
	else if(**p == 'F' || **p == 'f'){
	    int v;

	    fkeys++;
	    v = atoi((*p)+1);
	    if(v >= 1 && v <= 12)
	      i_cmds[i++] = PF1 + v - 1;
	    else
	      i_cmds[i++] = KEY_JUNK;
	}

	/* literal string */
	else if(**p == '"' && (*p)[lpm1 = strlen(*p) - 1] == '"'){
	    if(lpm1 + i - 1 > MAX_INIT_CMDS){
		snprintf(tmp_20k_buf, SIZEOF_20KBUF,
			"Initial keystroke list too long, truncated at %s\n", *p);
		init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
		break;                   /* Bail out of this loop! */
	    } else
	      for(j = 1; j < lpm1; j++)
		i_cmds[i++] = (*p)[j];
	}
	else {
	    snprintf(tmp_20k_buf,SIZEOF_20KBUF,
		    "Bad initial keystroke \"%.500s\" (missing comma?)", *p);
	    init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
	    break;
	}
      }
    }

    /*
     * We don't handle the case where function keys are used to specify the
     * commands but some non-function key input is also required.  For example,
     * you might want to jump to a specific message number and view it
     * on start up.  To do that, you need to use character commands instead
     * of function key commands in the initial-keystroke-list.
     */
    if(fkeys && not_fkeys){
	init_error(ps, SM_ORDER | SM_DING, 3, 5,
"Mixed characters and function keys in \"initial-keystroke-list\", skipping.");
	i = 0;
    }

    if(fkeys && !not_fkeys)
      F_TURN_ON(F_USE_FK,ps);
    if(!fkeys && not_fkeys)
      F_TURN_OFF(F_USE_FK,ps);

    if(i > 0){
	ps->initial_cmds = (int *)fs_get((i+1) * sizeof(int));
	ps->free_initial_cmds = ps->initial_cmds;
	for(j = 0; j < i; j++)
	  ps->initial_cmds[j] = i_cmds[j];

	ps->initial_cmds[i] = 0;
	ps->in_init_seq = ps->save_in_init_seq = 1;
    }
}


UCS *
user_wordseps(char **list)
 {
    char **p;
    int i = 0;
    int j;
#define MAX_SEPARATORS 500
    /*
     * This is just a temporary stack array, the real one is allocated below.
     * This is supposed to be way large enough.
     */
    UCS seps[MAX_SEPARATORS+1];
    UCS *u;
    UCS *return_array = NULL;
    size_t l;
  
    seps[0] = '\0';

    if(list){
	for(p = list; *p; p++){
	    if(i >= MAX_SEPARATORS){
		q_status_message(SM_ORDER | SM_DING, 3, 3,
			  "Warning: composer-word-separators list is too long");
		break;
	    }

	    u = utf8_to_ucs4_cpystr(*p);

	    if(u){
		if(ucs4_strlen(u) == 1)
		  seps[i++] = *u;
		else if(*u == '"' && u[l = ucs4_strlen(u) - 1] == '"'){
		    if(l + i - 1 > MAX_SEPARATORS){
			q_status_message(SM_ORDER | SM_DING, 3, 3,
				  "Warning: composer-word-separators list is too long");
			break;                   /* Bail out of this loop! */
		    }
		    else{
			for(j = 1; j < l; j++)
			  seps[i++] = u[j];
		    }
		}
		else{
		    l = ucs4_strlen(u);
		    if(l + i > MAX_SEPARATORS){
			q_status_message(SM_ORDER | SM_DING, 3, 3,
				  "Warning: composer-word-separators list is too long");
			break;                   /* Bail out of this loop! */
		    }
		    else{
			for(j = 0; j < l; j++)
			  seps[i++] = u[j];
		    }
		}

		fs_give((void **) &u);
	    }
	}
    }

    seps[i] = '\0';

    if(i > 0)
      return_array = ucs4_cpystr(seps);

    return(return_array);
}


/*
 * Make sure any errors during initialization get queued for display
 */
void
queue_init_errors(struct pine *ps)
{
    int i;

    if(ps->init_errs){
	for(i = 0; (ps->init_errs)[i].message; i++){
	    q_status_message((ps->init_errs)[i].flags,
			     (ps->init_errs)[i].min_time,
			     (ps->init_errs)[i].max_time,
			     (ps->init_errs)[i].message);
	    fs_give((void **)&(ps->init_errs)[i].message);
	}

	fs_give((void **)&ps->init_errs);
    }
}


/*----------------------------------------------------------------------
          Quit pine if the user wants to 

    Args: The usual pine structure

  Result: User is asked if she wants to quit, if yes then execute quit.

       Q U I T    S C R E E N

Not really a full screen. Just count up deletions and ask if we really
want to quit.
  ----*/
void
quit_screen(struct pine *pine_state)
{
    int quit = 0;

    dprint((1, "\n\n    ---- QUIT SCREEN ----\n"));    

    if(F_ON(F_CHECK_MAIL_ONQUIT,ps_global)
       && new_mail(1, VeryBadTime, NM_STATUS_MSG | NM_DEFER_SORT) > 0
       && (quit = want_to(_("Quit even though new mail just arrived"), 'y', 0,
			  NO_HELP, WT_NORM)) != 'y'){
	refresh_sort(pine_state->mail_stream, pine_state->msgmap, SRT_VRB);
        pine_state->next_screen = pine_state->prev_screen;
        return;
    }

    if(quit != 'y'
       && F_OFF(F_QUIT_WO_CONFIRM,pine_state)
       && want_to(_("Really quit Alpine"), 'y', 0, NO_HELP, WT_NORM) != 'y'){
        pine_state->next_screen = pine_state->prev_screen;
        return;
    }

    goodnight_gracey(pine_state, 0);
}


/*----------------------------------------------------------------------
    The nuts and bolts of actually cleaning up and exitting pine

    Args: ps -- the usual pine structure, 
	  exit_val -- what to tell our parent

  Result: This never returns

  ----*/
void
goodnight_gracey(struct pine *pine_state, int exit_val)
{
    int   i, cnt_user_streams = 0;
    char *final_msg = NULL;
    char  msg[MAX_SCREEN_COLS+1];
    char *pf = _("Alpine finished");
    MAILSTREAM *m;
    extern KBESC_T *kbesc;

    dprint((2, "goodnight_gracey:\n"));    

    /* We want to do this here before we close up the streams */
    trim_remote_adrbks();

    for(i = 0; i < ps_global->s_pool.nstream; i++){
	m = ps_global->s_pool.streams[i];
	if(m && sp_flagged(m, SP_LOCKED) && sp_flagged(m, SP_USERFLDR))
	  cnt_user_streams++;
    }

    /* clean up open streams */

    if(pine_state->mail_stream
       && sp_flagged(pine_state->mail_stream, SP_LOCKED)
       && sp_flagged(pine_state->mail_stream, SP_USERFLDR)){
	dprint((5, "goodnight_gracey: close current stream\n"));    
	expunge_and_close(pine_state->mail_stream,
			  (cnt_user_streams <= 1) ? &final_msg : NULL, EC_NONE);
	cnt_user_streams--;
    }

    pine_state->mail_stream = NULL;
    pine_state->redrawer = (void (*)(void))NULL;

    dprint((5,
	    "goodnight_gracey: close other stream pool streams\n"));    
    for(i = 0; i < ps_global->s_pool.nstream; i++){
	m = ps_global->s_pool.streams[i];
        /* 
	 * fix global for functions that depend(ed) on it sort_folder.
	 * Hopefully those will get phased out.
	 */
	ps_global->mail_stream = m;
	if(m && sp_flagged(m, SP_LOCKED) && sp_flagged(m, SP_USERFLDR)
	   && !sp_flagged(m, SP_INBOX)){
	    sp_set_expunge_count(m, 0L);
	    expunge_and_close(m, (cnt_user_streams <= 1) ? &final_msg : NULL,
			      EC_NONE);
	    cnt_user_streams--;
	}
    }

    for(i = 0; i < ps_global->s_pool.nstream; i++){
	m = ps_global->s_pool.streams[i];
        /* 
	 * fix global for functions that depend(ed) on it (sort_folder).
	 * Hopefully those will get phased out.
	 */
	ps_global->mail_stream = m;
	if(m && sp_flagged(m, SP_LOCKED) && sp_flagged(m, SP_USERFLDR)
	   && sp_flagged(m, SP_INBOX)){
	    dprint((5,
		    "goodnight_gracey: close inbox stream stream\n"));    
	    sp_set_expunge_count(m, 0L);
	    expunge_and_close(m, (cnt_user_streams <= 1) ? &final_msg : NULL,
			      EC_NONE);
	    cnt_user_streams--;
	}
    }

#ifdef _WINDOWS
    if(ps_global->ttyo)
      (void)get_windsize(ps_global->ttyo);
#endif

    dprint((7, "goodnight_gracey: close config files\n"));    

#ifdef SMIME
    smime_deinit();
#endif

    free_pinerc_strings(&pine_state);

    strncpy(msg, pf, sizeof(msg));
    msg[sizeof(msg)-1] = '\0';
    if(final_msg){
	strncat(msg, " -- ", sizeof(msg)-strlen(msg)-1);
	msg[sizeof(msg)-1] = '\0';
	strncat(msg, final_msg, sizeof(msg)-strlen(msg)-1);
	msg[sizeof(msg)-1] = '\0';
	fs_give((void **)&final_msg);
    }

    dprint((7, "goodnight_gracey: sp_end\n"));
    ps_global->noshow_error = 1;
    sp_end();

    /* after sp_end, which might call a filter */
    completely_done_with_adrbks();

    dprint((7, "goodnight_gracey: end_screen\n"));
    end_screen(msg, exit_val);
    dprint((7, "goodnight_gracey: end_titlebar\n"));
    end_titlebar();
    dprint((7, "goodnight_gracey: end_keymenu\n"));
    end_keymenu();

    dprint((7, "goodnight_gracey: end_keyboard\n"));
    end_keyboard(F_ON(F_USE_FK,pine_state));
    dprint((7, "goodnight_gracey: end_ttydriver\n"));
    end_tty_driver(pine_state);
#if !defined(DOS) && !defined(OS2)
    kbdestroy(kbesc);
#if !defined(LEAVEOUTFIFO)
    close_newmailfifo();
#endif
#endif
    end_signals(0);
    if(filter_data_file(0))
      our_unlink(filter_data_file(0));

    imap_flush_passwd_cache(TRUE);
    free_newsgrp_cache();
    mailcap_free();
    close_every_pattern();
    free_extra_hdrs();
    free_contexts(&ps_global->context_list);
    free_charsetchecker();
    dprint((7, "goodnight_gracey: free more memory\n"));    
#ifdef	ENABLE_LDAP
    free_saved_query_parameters();
#endif

    free_pine_struct(&pine_state);

    free_histlist();

#ifdef DEBUG
    if(debugfile){
	if(debug >= 2)
	  fputs("goodnight_gracey finished\n", debugfile);

	fclose(debugfile);
    }
#endif    

    exit(exit_val);
}


/*----------------------------------------------------------------------
  Call back for c-client to feed us back the progress of network reads

  Input: 

 Result: 
  ----*/
void
pine_read_progress(GETS_DATA *md, long unsigned int count)
{
    gets_bytes += count;			/* update counter */
}


/*----------------------------------------------------------------------
  Function to fish the current byte count from a c-client fetch.

  Input: reset -- flag telling us to reset the count

 Result: Returns the number of bytes read by the c-client so far
  ----*/
unsigned long
pine_gets_bytes(int reset)
{
    if(reset)
      gets_bytes = 0L;

    return(gets_bytes);
}


/*----------------------------------------------------------------------
    Panic pine - call on detected programmatic errors to exit pine

   Args: message -- message to record in debug file and to be printed for user

 Result: The various tty modes are restored
         If debugging is active a core dump will be generated
         Exits Alpine

  This is also called from imap routines and fs_get and fs_resize.
  ----*/
void
panic(char *message)
{
    char buf[256];

    /* global variable in .../pico/edef.h */
    in_panic = 1;

    if(ps_global->ttyo){
	end_screen(NULL, -1);
	end_keyboard(ps_global != NULL ? F_ON(F_USE_FK,ps_global) : 0);
	end_tty_driver(ps_global);
	end_signals(1);
    }
    if(filter_data_file(0))
      our_unlink(filter_data_file(0));

    dprint((1, "\n===========================================\n\n"));
    dprint((1, "   Alpine Panic: %s\n\n", message ? message : "?"));
    dprint((1, "===========================================\n\n"));

    /* intercept c-client "free storage" errors */
    if(strstr(message, "free storage"))
      snprintf(buf, sizeof(buf), _("No more available memory.\nAlpine Exiting"));
    else
      snprintf(buf, sizeof(buf), _("Problem detected: \"%s\".\nAlpine Exiting."), message);

    buf[sizeof(buf)-1] = '\0';

#ifdef _WINDOWS
    /* Put up a message box. */
    mswin_messagebox (buf, 1);
#else
    fprintf(stderr, "\n\n%s\n", buf);
#endif

#ifdef DEBUG
    if(debugfile){
	save_debug_on_crash(debugfile, recent_keystroke);
    }

    coredump();   /*--- If we're debugging get a core dump --*/
#endif

    exit(-1);
    fatal("ffo"); /* BUG -- hack to get fatal out of library in right order*/
}


/*
 * panicking - function to test whether or not we're exiting under stress.
 *             
 */
int
panicking(void)
{
    return(in_panic);
}


/*----------------------------------------------------------------------
    exceptional_exit - called to exit under unusual conditions (with no core)

   Args: message -- message to record in debug file and to be printed for user
	 ev -- exit value

  ----*/
void
exceptional_exit(char *message, int ev)
{
    fprintf(stderr, "%s\n", message);
    exit(ev);
}


/*
 *  PicoText Storage Object Support Routines
 */

STORE_S *
pine_pico_get(void)
{
    return((STORE_S *)pico_get());
}

int
pine_pico_give(STORE_S **sop)
{
    pico_give((void *)sop);
    return(1);
}

int
pine_pico_writec(int c, STORE_S *so)
{
    unsigned char ch = (unsigned char) c;

    return(pico_writec(so->txt, ch, PICOREADC_NONE));
}

int
pine_pico_writec_noucs(int c, STORE_S *so)
{
    unsigned char ch = (unsigned char) c;

    return(pico_writec(so->txt, ch, PICOREADC_NOUCS));
}

int
pine_pico_readc(unsigned char *c, STORE_S *so)
{
    return(pico_readc(so->txt, c, PICOREADC_NONE));
}

int
pine_pico_readc_noucs(unsigned char *c, STORE_S *so)
{
    return(pico_readc(so->txt, c, PICOREADC_NOUCS));
}

int
pine_pico_puts(STORE_S *so, char *s)
{
    return(pico_puts(so->txt, s, PICOREADC_NONE));
}

int
pine_pico_puts_noucs(STORE_S *so, char *s)
{
    return(pico_puts(so->txt, s, PICOREADC_NOUCS));
}

int
pine_pico_seek(STORE_S *so, long pos, int orig)
{
    return(pico_seek((void *)so, pos, orig));
}


int
remote_pinerc_failure(void)
{
#ifdef _WINDOWS
    if(ps_global->install_flag)  /* just exit silently */
      exit(0);
#endif /* _WINDOWS */

    if(ps_global->exit_if_no_pinerc){
	exceptional_exit("Exiting because -bail option is set and config file not readable.", -1);
    }

    if(want_to("Trouble reading remote configuration! Continue anyway ",
	       'n', 'n', NO_HELP, WT_FLUSH_IN) != 'y'){
	return(0);
    }

    return(1);
}


void
dump_supported_options(void)
{
    char **config;

    config = get_supported_options();
    if(config){
	display_args_err(NULL, config, 0);
	free_list_array(&config);
    }
}


/*----------------------------------------------------------------------
     Check pruned-folders for validity, making sure they are in the 
     same context as sent-mail.

  ----*/
int
prune_folders_ok(void)
{
    char **p;

    for(p = ps_global->VAR_PRUNED_FOLDERS; p && *p && **p; p++)
      if(!context_isambig(*p))
	return(0);

    return(1);
}


#ifdef	WIN32
char *
pine_user_callback()
{
    if(ps_global->VAR_USER_ID && ps_global->VAR_USER_ID[0]){
	return(ps_global->VAR_USER_ID);
    }
    else{
	/* SHOULD PROMPT HERE! */
      return(NULL);
    }
}
#endif

#ifdef	_WINDOWS
/*
 * windows callback to get/set function keys mode state
 */
int
fkey_mode_callback(set, args)
    int  set;
    long args;
{
    return(F_ON(F_USE_FK, ps_global) != 0);
}


void
imap_telemetry_on()
{
    if(ps_global->mail_stream)
      mail_debug(ps_global->mail_stream);
}


void
imap_telemetry_off()
{
    if(ps_global->mail_stream)
      mail_nodebug(ps_global->mail_stream);
}


char *
pcpine_help_main(title)
    char *title;
{
    if(title)
      strncpy(title, _("PC-Alpine MAIN MENU Help"), 256);

    return(pcpine_help(main_menu_tx));
}


int
pcpine_main_cursor(col, row)
    int  col;
    long row;
{
    unsigned ndmi;

    if (row >= (HEADER_ROWS(ps_global) + MNSKIP(ps_global)))
      ndmi = (row+1 - HEADER_ROWS(ps_global) - (MNSKIP(ps_global)+1))/(MNSKIP(ps_global)+1);

    if (row >= (HEADER_ROWS(ps_global) + MNSKIP(ps_global))
	&& !(MNSKIP(ps_global) && (row+1) & 0x01)
	&& ndmi <= MAX_MENU_ITEM
	&& FOOTER_ROWS(ps_global) + (ndmi+1)*(MNSKIP(ps_global)+1)
	    + MNSKIP(ps_global) + FOOTER_ROWS(ps_global) <= ps_global->ttyo->screen_rows)
      return(MSWIN_CURSOR_HAND);
    else
      return(MSWIN_CURSOR_ARROW);
}
#endif /* _WINDOWS */
