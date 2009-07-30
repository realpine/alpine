#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: termin.unx.c 1025 2008-04-08 22:59:38Z hubert@u.washington.edu $";
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
#include <system.h>
#include <general.h>

#include "../../c-client/mail.h"	/* for MAILSTREAM and friends */
#include "../../c-client/osdep.h"
#include "../../c-client/rfc822.h"	/* for soutr_t and such */
#include "../../c-client/misc.h"	/* for cpystr proto */
#include "../../c-client/utf8.h"	/* for CHARSET and such*/
#include "../../c-client/imap4r1.h"

#include "../../pith/charconv/utf8.h"
#include "../../pith/charconv/filesys.h"

#include "../../pith/osdep/color.h"
#include "../../pith/osdep/collate.h"
#include "../../pith/osdep/err_desc.h"

#include "../../pith/debug.h"
#include "../../pith/state.h"
#include "../../pith/conf.h"
#include "../../pith/detach.h"
#include "../../pith/adrbklib.h"
#include "../../pith/remote.h"
#include "../../pith/imap.h"
#include "../../pith/status.h"

#include "../pico/estruct.h"

#include "../../pico/estruct.h"
#include "../../pico/pico.h"
#include "../../pico/osdep/raw.h"
#include "../../pico/osdep/signals.h"
#include "../../pico/osdep/mouse.h"
#include "../../pico/osdep/read.h"
#include "../../pico/osdep/getkey.h"
#include "../../pico/osdep/tty.h"
#include "../../pico/keydefs.h"

#include "../talk.h"
#include "../radio.h"
#include "../dispfilt.h"
#include "../signal.h"
#include "../mailcmd.h"
#include "../setup.h"

#include "termin.gen.h"
#include "termout.gen.h"
#include "termin.unx.h"



/*======================================================================
       Things having to do with reading from the tty driver and keyboard
          - initialize tty driver and reset tty driver
          - read a character from terminal with keyboard escape seqence mapping
          - initialize keyboard (keypad or such) and reset keyboard
          - prompt user for a line of input
          - read a command from keyboard with timeouts.

 ====*/


/*
 * Helpful definitions
 */
/*
 * Should really be using pico's TERM's t_getchar to read a character but
 * we're just calling ttgetc directly for now. Ttgetc is the same as
 * t_getchar whenever we use it so we're avoiding the trouble of initializing
 * the TERM struct and calling ttgetc directly.
 */
#define READ_A_CHAR()	ttgetc(NO_OP_COMMAND, key_rec, read_bail)


/*
 * Internal prototypes
 */
int           pine_simple_ttgetc(int (*recorder)(int), void (*bail_handler)(void));
UCS           check_for_timeout(int);
void	      read_bail(void);


/*----------------------------------------------------------------------
    Initialize the tty driver to do single char I/O and whatever else  (UNIX)

   Args:  struct pine

 Result: tty driver is put in raw mode so characters can be read one
         at a time. Returns -1 if unsuccessful, 0 if successful.

Some file descriptor voodoo to allow for pipes across vforks. See 
open_mailer for details.
  ----------------------------------------------------------------------*/
int
init_tty_driver(struct pine *ps)
{
#ifdef	MOUSE
    if(F_ON(F_ENABLE_MOUSE, ps_global))
      init_mouse();
#endif	/* MOUSE */

    /* turn off talk permission by default */
    
    if(F_ON(F_ALLOW_TALK, ps))
      allow_talk(ps);
    else
      disallow_talk(ps);

    return(PineRaw(1));
}



/*----------------------------------------------------------------------
   Set or clear the specified tty mode

   Args: ps --  struct pine
	 mode -- mode bits to modify
	 clear -- whether or not to clear or set

 Result: tty driver mode change. 
  ----------------------------------------------------------------------*/
void
tty_chmod(struct pine *ps, int mode, int func)
{
    char	*tty_name;
    int		 new_mode;
    struct stat  sbuf;
    static int   saved_mode = -1;

    /* if no problem figuring out tty's name & mode? */
    if((((tty_name = (char *) ttyname(STDIN_FD)) != NULL
	 && fstat(STDIN_FD, &sbuf) == 0)
	|| ((tty_name = (char *) ttyname(STDOUT_FD)) != NULL
	    && fstat(STDOUT_FD, &sbuf) == 0))
       && !(func == TMD_RESET && saved_mode < 0)){
	new_mode = (func == TMD_RESET)
		     ? saved_mode
		     : (func == TMD_CLEAR)
			? (sbuf.st_mode & ~mode)
			: (sbuf.st_mode | mode);
	/* assign tty new mode */
	if(our_chmod(tty_name, new_mode) == 0){
	    if(func == TMD_RESET)		/* forget we knew */
	      saved_mode = -1;
	    else if(saved_mode < 0)
	      saved_mode = sbuf.st_mode;	/* remember original */
	}
    }
}



/*----------------------------------------------------------------------
       End use of the tty, put it back into it's normal mode     (UNIX)

   Args: ps --  struct pine

 Result: tty driver mode change. 
  ----------------------------------------------------------------------*/
void
end_tty_driver(struct pine *ps)
{
    ps = ps; /* get rid of unused parameter warning */

#ifdef	MOUSE
    end_mouse();
#endif	/* MOUSE */
    fflush(stdout);
    dprint((2, "about to end_tty_driver\n"));

    tty_chmod(ps, 0, TMD_RESET);
    PineRaw(0);
}



/*----------------------------------------------------------------------
    Actually set up the tty driver                             (UNIX)

   Args: state -- which state to put it in. 1 means go into raw, 0 out of

  Result: returns 0 if successful and < 0 if not.
  ----*/
int
PineRaw(int state)
{
    int result;

    result = Raw(state);
    
    if(result == 0 && state == 1){
	/*
	 * Only go into 8 bit mode if we are doing something other
	 * than plain ASCII. This will save the folks that have
	 * their parity on their serial lines wrong the trouble of
	 * getting it right
	 */
        if((ps_global->keyboard_charmap && ps_global->keyboard_charmap[0] &&
	   strucmp(ps_global->keyboard_charmap, "us-ascii"))
	   || (ps_global->display_charmap && ps_global->display_charmap[0] &&
	       strucmp(ps_global->display_charmap, "us-ascii")))
	  bit_strip_off();

#ifdef	DEBUG
	if(debug < 9)			/* only on if full debugging set */
#endif
	quit_char_off();
	ps_global->low_speed = ttisslow();
	crlf_proc(0);
	xonxoff_proc(F_ON(F_PRESERVE_START_STOP, ps_global));
    }

    return(result);
}


#if	defined(SIGWINCH) && defined(TIOCGWINSZ)
jmp_buf winch_state;
int     winch_occured = 0;
int     ready_for_winch = 0;
#endif

/*----------------------------------------------------------------------
     This checks whether or not a character			(UNIX)
     is ready to be read, or it times out.

    Args:  time_out --  number of seconds before it will timeout

  Result: Returns a NO_OP_IDLE or a NO_OP_COMMAND if the timeout expires
	  before input is available, or a KEY_RESIZE if a resize event
	  occurs, or READY_TO_READ if input is available before the timeout.
  ----*/
UCS
check_for_timeout(int time_out)
{
    UCS res = NO_OP_COMMAND;

    fflush(stdout);

#if	defined(SIGWINCH) && defined(TIOCGWINSZ)
    if(!winch_occured){
	if(setjmp(winch_state) != 0){
	    winch_occured = 1;
	    ready_for_winch = 0;

	    /*
	     * Need to unblock signal after longjmp from handler, because
	     * signal is normally unblocked upon routine exit from the handler.
	     */
	    our_sigunblock(SIGWINCH);
	}
	else
	  ready_for_winch = 1;
    }

    if(winch_occured){
	winch_occured = ready_for_winch = 0;
	fix_windsize(ps_global);
	return(KEY_RESIZE);
    }
#endif /* SIGWINCH */

    switch(res = input_ready(time_out)){
      case BAIL_OUT:
	read_bail();			/* non-tragic exit */
	/* NO RETURN */

      case PANIC_NOW:
	panic1("Select error: %s\n", error_description(errno));
	/* NO RETURN */

      case READ_INTR:
	res = NO_OP_COMMAND;
	/* fall through */

      case NO_OP_IDLE:
      case NO_OP_COMMAND:
      case READY_TO_READ:
#if	defined(SIGWINCH) && defined(TIOCGWINSZ)
	ready_for_winch = 0;
#endif
	return(res);
    }

    /* not reachable */
    return(res);
}



/*----------------------------------------------------------------------
  Read input characters with lots of processing for arrow keys and such  (UNIX)

 Args:  time_out -- The timeout to for the reads 

 Result: returns the character read. Possible special chars.

    This deals with function and arrow keys as well. 

  The idea is that this routine handles all escape codes so it done in
  only one place. Especially so the back arrow key can work when entering
  things on a line. Also so all function keys can be disabled and not
  cause weird things to happen.
  ---*/
UCS
read_char(int time_out)
{
    UCS status, cc, ch;
    int (*key_rec)(int);

    key_rec = key_recorder;
    if(ps_global->conceal_sensitive_debugging)
      key_rec = NULL;

    /* Get input from initial-keystrokes */
    if(process_config_input(&cc)){
	ch = cc;
	return(ch);
    }

    if((ch = check_for_timeout(time_out)) != READY_TO_READ)
      goto done;
    
    switch(status = kbseq(pine_simple_ttgetc, key_rec, read_bail,
			  ps_global->input_cs, &ch)){
      case KEY_DOUBLE_ESC:
	/*
	 * Special hack to get around comm devices eating control characters.
	 */
	if(check_for_timeout(5) != READY_TO_READ){
	    dprint((9, "Read char: incomplete double escape timed out...\n"));
	    ch = KEY_JUNK;		/* user typed ESC ESC, then stopped */
	    goto done;
	}
	else
	  ch = READ_A_CHAR();

	ch &= 0x7f;

	/* We allow a 3-digit number between 001 and 255 */
	if(isdigit((unsigned char) ch)){
	    int n = 0, i = ch - '0';

	    if(i < 0 || i > 2){
		dprint((9, "Read char: double escape followed by 1st digit not 0, 1, or 2... (%d)\n", i));
		ch = KEY_JUNK;
		goto done;		/* bogus literal char value */
	    }

	    while(n++ < 2){
		if(check_for_timeout(5) != READY_TO_READ
		   || (!isdigit((unsigned char) (ch = READ_A_CHAR()))
		       || (n == 1 && i == 2 && ch > '5')
		       || (n == 2 && i == 25 && ch > '5'))){
		    dprint((9, "Read char: bad double escape, either timed out or too large 3-digit num...\n"));
		    ch = KEY_JUNK;	/* user typed ESC ESC #, stopped */
		    goto done;
		}

		i = (i * 10) + (ch - '0');
	    }

	    ch = i;
	}
	else{	/* or, normal case, ESC ESC c  means ^c */
	    if(islower((unsigned char) ch))	/* canonicalize if alpha */
	      ch = toupper((unsigned char) ch);

	    ch = (isalpha((unsigned char)ch) || ch == '@'
		  || (ch >= '[' && ch <= '_'))
		   ? ctrl(ch) : ((ch == SPACE) ? ctrl('@'): ch);
	    dprint((9, "Read char: this is a successful double escape...\n"));
	}

	goto done;

#ifdef MOUSE
      case KEY_XTERM_MOUSE:
	if(mouseexist()){
	    /*
	     * Special hack to get mouse events from an xterm.
	     * Get the details, then pass it past the keymenu event
	     * handler, and then to the installed handler if there
	     * is one...
	     */
	    static int    down = 0;
	    int           x, y, button;
	    unsigned long cmd;

	    clear_cursor_pos();
	    button = READ_A_CHAR() & 0x03;

	    x = READ_A_CHAR() - '!';
	    y = READ_A_CHAR() - '!';

	    ch = NO_OP_COMMAND;
	    if(button == 0){		/* xterm button 1 down */
		down = 1;
		if(checkmouse(&cmd, 1, x, y))
		  ch = cmd;
	    }
	    else if (down && button == 3){
		down = 0;
		if(checkmouse(&cmd, 0, x, y))
		  ch = cmd;
	    }

	    goto done;
	}

	break;
#endif /* MOUSE */

      case  KEY_UP	:
      case  KEY_DOWN	:
      case  KEY_RIGHT	:
      case  KEY_LEFT	:
      case  KEY_PGUP	:
      case  KEY_PGDN	:
      case  KEY_HOME	:
      case  KEY_END	:
      case  KEY_DEL	:
      case  PF1		:
      case  PF2		:
      case  PF3		:
      case  PF4		:
      case  PF5		:
      case  PF6		:
      case  PF7		:
      case  PF8		:
      case  PF9		:
      case  PF10	:
      case  PF11	:
      case  PF12	:
        dprint((9, "Read char returning: 0x%x %s\n", status, pretty_command(status)));
	return(status);

      case  CTRL_KEY_UP	:
	status = KEY_UP;
        dprint((9, "Read char returning: 0x%x %s (for CTRL_KEY_UP)\n", status, pretty_command(status)));
	return(status);

      case  CTRL_KEY_DOWN	:
	status = KEY_DOWN;
        dprint((9, "Read char returning: 0x%x %s (for CTRL_KEY_DOWN)\n", status, pretty_command(status)));
	return(status);

      case  CTRL_KEY_RIGHT	:
	status = KEY_RIGHT;
        dprint((9, "Read char returning: 0x%x %s (for CTRL_KEY_RIGHT)\n", status, pretty_command(status)));
	return(status);

      case  CTRL_KEY_LEFT	:
	status = KEY_LEFT;
        dprint((9, "Read char returning: 0x%x %s (for CTRL_KEY_LEFT)\n", status, pretty_command(status)));
	return(status);

      case KEY_SWALLOW_Z:
	status = KEY_JUNK;
      case KEY_SWAL_UP:
      case KEY_SWAL_DOWN:
      case KEY_SWAL_LEFT:
      case KEY_SWAL_RIGHT:
	do
	  if(check_for_timeout(2) != READY_TO_READ){
	      status = KEY_JUNK;
	      break;
	  }
	while(!strchr("~qz", READ_A_CHAR()));
	ch = (status == KEY_JUNK) ? status : status - (KEY_SWAL_UP - KEY_UP);
	goto done;

      case KEY_KERMIT:
	do{
	    cc = ch;
	    if(check_for_timeout(2) != READY_TO_READ){
		status = KEY_JUNK;
		break;
	    }
	    else
	      ch = READ_A_CHAR();
	}while(cc != '\033' && ch != '\\');

	ch = KEY_JUNK;
	goto done;

      case BADESC:
	ch = KEY_JUNK;
	goto done;

      case 0: 	/* regular character */
      default:
	/*
	 * we used to strip (ch &= 0x7f;), but this seems much cleaner
	 * in the face of line noise and has the benefit of making it
	 * tougher to emit mistakenly labeled MIME...
	 */
	if((ch & ~0x7f)
	   && ((!ps_global->keyboard_charmap || !strucmp(ps_global->keyboard_charmap, "US-ASCII"))
	       && (!ps_global->display_charmap || !strucmp(ps_global->display_charmap, "US-ASCII")))){
	    dprint((9, "Read char sees ch = 0x%x status=0x%x, returns KEY_JUNK\n", ch, status));
	    return(KEY_JUNK);
	}
	else if(ch == ctrl('Z')){
	    dprint((9, "Read char got ^Z, calling do_suspend\n"));
	    ch = do_suspend();
	    dprint((9, "After do_suspend Read char returns 0x%x %s\n", ch, pretty_command(ch)));
	    return(ch);
	}
#ifdef MOUSE
	else if(ch == ctrl('\\')){
	    int e;

	    dprint((9, "Read char got ^\\, toggle xterm mouse\n"));
	    if(F_ON(F_ENABLE_MOUSE, ps_global)){
		(e=mouseexist()) ? end_mouse() : (void) init_mouse();
		if(e != mouseexist())
		  q_status_message1(SM_ASYNC, 0, 2, "Xterm mouse tracking %s!",
						     mouseexist() ? "on" : "off");
		else if(!e)
		  q_status_message1(SM_ASYNC, 0, 2, "See help for feature \"%s\" ($DISPLAY variable set?)", pretty_feature_name(feature_list_name(F_ENABLE_MOUSE), -1));
	    }
	    else
	      q_status_message1(SM_ASYNC, 0, 2, "Feature \"%s\" not enabled",
				pretty_feature_name(feature_list_name(F_ENABLE_MOUSE), -1));

	    return(NO_OP_COMMAND);
	}
#endif /* MOUSE */


      done:
#ifdef	DEBUG
	if(ps_global->conceal_sensitive_debugging && debug < 10){
	    dprint((9, "Read char returning: <hidden char>\n"));
	}
	else{
	    dprint((9, "Read char returning: 0x%x %s\n", ch, pretty_command(ch)));
	}
#endif

        return(ch);
    }

    /* not reachable */
    return(KEY_JUNK);
}


/*----------------------------------------------------------------------
  Reading input somehow failed and we need to shutdown now

 Args:  none

 Result: pine exits

  ---*/
void
read_bail(void)
{
    dprint((1, "read_bail: cleaning up\n"));
    end_signals(1);

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
	  write_pinerc(ps_global, Main, WRP_NOUSER);

	if(ps_global->prc->rd)
	  rd_close_remdata(&ps_global->prc->rd);
	
	free_pinerc_s(&ps_global->prc);
    }

    /* as does this */
    if(ps_global->post_prc){
	if(ps_global->post_prc->outstanding_pinerc_changes)
	  write_pinerc(ps_global, Post, WRP_NOUSER);

	if(ps_global->post_prc->rd)
	  rd_close_remdata(&ps_global->post_prc->rd);
	
	free_pinerc_s(&ps_global->post_prc);
    }

    sp_end();

    dprint((1, "done with read_bail clean up\n"));

    imap_flush_passwd_cache(TRUE);
    end_keyboard(F_ON(F_USE_FK,ps_global));
    end_tty_driver(ps_global);
    if(filter_data_file(0))
      our_unlink(filter_data_file(0));

    exit(0);
}


int
pine_simple_ttgetc(int (*fi)(int), void (*fv)(void))
{
    int ch;

#if	defined(SIGWINCH) && defined(TIOCGWINSZ)
    if(!winch_occured){
	if(setjmp(winch_state) != 0){
	    winch_occured = 1;
	    ready_for_winch = 0;

	    /*
	     * Need to unblock signal after longjmp from handler, because
	     * signal is normally unblocked upon routine exit from the handler.
	     */
	    our_sigunblock(SIGWINCH);
	}
	else
	  ready_for_winch = 1;
    }

    if(winch_occured){
	winch_occured = ready_for_winch = 0;
	fix_windsize(ps_global);
	return(KEY_RESIZE);
    }
#endif /* SIGWINCH */

    ch = simple_ttgetc(fi, fv);

#if	defined(SIGWINCH) && defined(TIOCGWINSZ)
    ready_for_winch = 0;
#endif

    return(ch);
}



extern char term_name[];
/* -------------------------------------------------------------------
     Set up the keyboard -- usually enable some function keys  (UNIX)

    Args: struct pine 

So far all we do here is turn on keypad mode for certain terminals

Hack for NCSA telnet on an IBM PC to put the keypad in the right mode.
This is the same for a vtXXX terminal or [zh][12]9's which we have 
a lot of at UW
  ----*/
void
init_keyboard(int use_fkeys)
{
    if(use_fkeys && (!strucmp(term_name,"vt102")
		     || !strucmp(term_name,"vt100")))
      printf("\033\133\071\071\150");
}



/*----------------------------------------------------------------------
     Clear keyboard, usually disable some function keys           (UNIX)

   Args:  pine state (terminal type)

 Result: keyboard state reset
  ----*/
void
end_keyboard(int use_fkeys)
{
    if(use_fkeys && (!strcmp(term_name, "vt102")
		     || !strcmp(term_name, "vt100"))){
	printf("\033\133\071\071\154");
	fflush(stdout);
    }
}


/*
 * This is a bare-bones implementation which is missing most of the
 * features of the real optionally_enter. The initial value of string is
 * isgnored. The escape_list is ignored. The help is not implemented. The
 * only flag implemented is OE_PASSWD. We don't go into raw mode so the
 * only input possible is a line (the EOL is stripped before returning).
 */
int
pre_screen_config_opt_enter(char *string, int string_size, char *prompt,
			    ESCKEY_S *escape_list, HelpType help, int *flags)
{
    char *pw;
    int   return_v = -10;

    while(return_v == -10) {

	if(flags && (*flags & (OE_PASSWD | OE_PASSWD_NOAST))){
	    if((pw = getpass(prompt)) != NULL){
		if(strlen(pw) < string_size){
		    strncpy(string, pw, string_size);
		    string[string_size-1] = '\0';
		    return_v = 0;
		}
		else{
		    fputs("Password too long\n", stderr);
		    return_v = -1;
		}
	    }
	    else
	      return_v = 1;	/* cancel? */
	}
	else{
	    char *p;

	    fputs(prompt, stdout);
	    fgets(string, string_size, stdin);
	    string[string_size-1] = '\0';
	    if((p = strpbrk(string, "\r\n")) != NULL)
	      *p = '\0';

	    return_v = 0;
	}
    }

    return(return_v);
}


