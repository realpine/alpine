#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: termin.gen.c 1025 2008-04-08 22:59:38Z hubert@u.washington.edu $";
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

#include "../../pith/osdep/color.h"

#include "../../pith/charconv/utf8.h"

#include "../../pith/debug.h"
#include "../../pith/newmail.h"
#include "../../pith/conf.h"
#include "../../pith/busy.h"

#include "../../pico/estruct.h"
#include "../../pico/pico.h"
#include "../../pico/keydefs.h"

#include "../../pico/osdep/color.h"

#include "../status.h"
#include "../folder.h"
#include "../keymenu.h"
#include "../send.h"
#include "../radio.h"
#include "../busy.h"

#ifdef _WINDOWS
#include "../../pico/osdep/mswin.h"
#include "termin.wnt.h"
#include "termout.wnt.h"
#else
#include "termout.unx.h"
#endif

#include "termin.gen.h"
#include "termout.gen.h"

#include "../mailcmd.h"


#ifdef	_WINDOWS
static int g_mc_row, g_mc_col;

int	pcpine_oe_cursor(int, long);
#endif


/*
 *     Generic tty input routines
 */


/*----------------------------------------------------------------------
        Read a character from keyboard with timeout
 Input:  none

 Result: Returns command read via read_char
         Times out and returns a null command every so often

  Calculates the timeout for the read, and does a few other house keeping 
things.  The duration of the timeout is set in pine.c.
  ----------------------------------------------------------------------*/
UCS
read_command(char **utf8str)
{
    int tm = 0, more_freq_timeo;
    UCS ucs;
    long dtime; 
    static unsigned char utf8buf[7];
    unsigned char *newdestp;

    /*
     * timeo is the mail-check-interval. What we want to do (ignoring the
     * messages_queued part) is timeout more often than timeo but only
     * check for new mail every timeo or so seconds. The reason we want to
     * timeout more often is so that we will have a chance to catch the user
     * in an idle period where we can do a check_point(). Otherwise, with
     * a default mail-check-interval, we are almost always calling newmail
     * right after the user presses a key, making it the worst possible
     * time to do a checkpoint.
     */

    more_freq_timeo = MIN(get_input_timeout(), IDLE_TIMEOUT);
    if(more_freq_timeo == 0)
      more_freq_timeo = IDLE_TIMEOUT;

    cancel_busy_cue(-1);
    tm = (messages_queued(&dtime) > 1) ? (int)dtime : more_freq_timeo;

    if(utf8str)
      *utf8str = NULL;

    ucs = read_char(tm);
    if(ucs != NO_OP_COMMAND && ucs != NO_OP_IDLE && ucs != KEY_RESIZE)
      zero_new_mail_count();

#ifdef	BACKGROUND_POST
    /*
     * Any expired children to report on?
     */
    if(ps_global->post && ps_global->post->pid == 0){
	int   winner = 0;

	if(ps_global->post->status < 0){
	    q_status_message(SM_ORDER | SM_DING, 3, 3, "Abysmal failure!");
	}
	else{
	    (void) pine_send_status(ps_global->post->status,
				    ps_global->post->fcc, tmp_20k_buf, SIZEOF_20KBUF,
				    &winner);
	    q_status_message(SM_ORDER | (winner ? 0 : SM_DING), 3, 3,
			     tmp_20k_buf);

	}

	if(!winner)
	  q_status_message(SM_ORDER, 0, 3,
	  "Re-send via \"Compose\" then \"Yes\" to \"Continue INTERRUPTED?\"");

	if(ps_global->post->fcc)
	  fs_give((void **) &ps_global->post->fcc);

	fs_give((void **) &ps_global->post);
    }
#endif

    /*
     * The character we get from read_char() is a UCS-4 char. Or it could be a special
     * value like KEY_UP or NO_OP_IDLE or something similar. From here on out
     * we're going to operate with UTF-8 internally. This is the point where we
     * convert the UCS-4 input (actually whatever sort of input the user is typing
     * was converted to UCS-4 first) to UTF-8. It's easy in this read_command
     * case because if user types a non-ascii character as a command it's going to be
     * an error. All commands are ascii. In order to present a reasonable error
     * message we pass back the UTF-8 string to the caller.
     */
    if(ucs >= 0x80 && ucs < KEY_BASE){
	/*
	 * User typed a character that is non-ascii. Convert it to
	 * UTF-8.
	 */
	memset(utf8buf, 0, sizeof(utf8buf));
	newdestp = utf8_put(utf8buf, (unsigned long) ucs);
	if(newdestp - utf8buf > 1){	/* this should happen */
	    if(utf8str)
	      *utf8str = (char *) utf8buf;

	    dprint((9, "Read command: looks like user typed non-ascii command 0x%x %s: returning KEY_UTF8\n", ucs, pretty_command(ucs)));
	    ucs = KEY_UTF8;
	}
	else{
	    dprint((9, "Read command: looks like user typed unknown, non-ascii command 0x%x %s: returning KEY_UNKNOWN\n", ucs, pretty_command(ucs)));
	    ucs = KEY_UNKNOWN;	/* best we can do, shouldn't happen */
	}
    }
    else{
	dprint((9, "Read command returning: 0x%x %s\n", ucs, pretty_command(ucs)));
    }

    return(ucs);
}


int
read_command_prep()
{
    int		i;
    char       *fname;
    MAILSTREAM *m;

    /*
     * Before we sniff at the input queue, make sure no external event's
     * changed our picture of the message sequence mapping.  If so,
     * recalculate the dang thing and run thru whatever processing loop
     * we're in again...
     */
    for(i = 0; i < ps_global->s_pool.nstream; i++){
	m = ps_global->s_pool.streams[i];
	if(m && sp_flagged(m, SP_LOCKED) && sp_flagged(m, SP_USERFLDR)
	   && sp_expunge_count(m)){
	    fname = STREAMNAME(m);
	    q_status_message3(SM_ORDER, 3, 3,
			      "%s message%s expunged from folder \"%s\"",
			      long2string(sp_expunge_count(m)),
			      plural(sp_expunge_count(m)),
			      pretty_fn(fname));
	    sp_set_expunge_count(m, 0L);
	    display_message('x');
	}
    }

    if(sp_mail_box_changed(ps_global->mail_stream)
       && sp_new_mail_count(ps_global->mail_stream)){
        dprint((2, "Noticed %ld new msgs! \n",
		   sp_new_mail_count(ps_global->mail_stream)));
	return(FALSE);		/* cycle thru so caller can update */
    }

    return(TRUE);
}


/*---------------------------------------------------------------------- 
       Prompt user for a string in status line with various options

  Args: utf8string -- the buffer result is returned in, and original string (if 
                   any) is passed in.
        y_base -- y position on screen to start on. 0,0 is upper left
                    negative numbers start from bottom
        x_base -- column position on screen to start on. 0,0 is upper left
        utf8string_size -- Length of utf8string buffer
        utf8prompt -- The string to prompt with
	escape_list -- pointer to array of ESCKEY_S's.  input chars matching
                       those in list return value from list.
        help   -- Array of strings for help text in bottom screen lines
        flags  -- pointer (because some are return values) to flags
		  OE_USER_MODIFIED       - Set on return if user modified buffer
		  OE_DISALLOW_CANCEL     - No cancel in menu.
		  OE_DISALLOW_HELP       - No help in menu.
		  OE_KEEP_TRAILING_SPACE - Allow trailing space.
		  OE_SEQ_SENSITIVE       - Caller is sensitive to sequence
					   number changes.
		  OE_APPEND_CURRENT      - String should not be truncated
					   before accepting user input.
		  OE_PASSWD              - Don't echo on screen.

  Result:  editing input string
            returns -1 unexpected errors
            returns 0  normal entry typed (editing and return or PF2)
            returns 1  typed ^C or PF2 (cancel)
            returns 3  typed ^G or PF1 (help)
            returns 4  typed ^L for a screen redraw

  WARNING: Care is required with regard to the escape_list processing.
           The passed array is terminated with an entry that has ch = -1.
           Function key labels and key strokes need to be setup externally!
	   Traditionally, a return value of 2 is used for ^T escapes.

   Unless in escape_list, tabs are trapped by isprint().
This allows near full weemacs style editing in the line
   ^A beginning of line
   ^E End of line
   ^R Redraw line
   ^G Help
   ^F forward
   ^B backward
   ^D delete
----------------------------------------------------------------------*/

int
optionally_enter(char *utf8string, int y_base, int x_base, int utf8string_size,
		 char *utf8prompt, ESCKEY_S *escape_list, HelpType help, int *flags)
{
    UCS           *string = NULL, ucs;
    size_t         string_size;
    UCS           *s2;
    UCS           *saved_original = NULL;
    char          *candidate;
    UCS           *kill_buffer = NULL;
    UCS           *k, *kb;
    int            field_pos;		/* offset into array dline.vl */
    int            i, j, return_v, cols, prompt_width, too_thin,
                   real_y_base, km_popped, passwd;
    char         **help_text;
    long	   fkey_table[12];
    struct	   key_menu *km;
    bitmap_t	   bitmap;
    COLOR_PAIR    *lastc = NULL, *promptc = NULL;
    struct variable *vars = ps_global->vars;
    struct display_line dline;
#ifdef	_WINDOWS
    int		   cursor_shown;
#endif

    dprint((5, "=== optionally_enter called ===\n"));
    dprint((9, "utf8string:\"%s\"  y:%d  x:%d  length: %d append: %d\n",
               utf8string ? utf8string : "",
	       x_base, y_base, utf8string_size,
	       (flags && *flags & OE_APPEND_CURRENT)));
    dprint((9, "passwd:%d   utf8prompt:\"%s\"   label:\"%s\"\n",
	       (flags && *flags & OE_PASSWD_NOAST) ? 10 :
		   (flags && *flags & OE_PASSWD) ? 1 : 0,
	       utf8prompt ? utf8prompt : "",
	       (escape_list && escape_list[0].ch != -1 && escape_list[0].label)
		 ? escape_list[0].label: ""));

    if(!ps_global->ttyo)
      return(pre_screen_config_opt_enter(utf8string, utf8string_size, utf8prompt,
					 escape_list, help, flags));

#ifdef _WINDOWS
    if (mswin_usedialog ())
      return(win_dialog_opt_enter(utf8string, utf8string_size, utf8prompt,
				  escape_list, help, flags));
#endif


    /*
     * Utf8string comes in as UTF-8. We'll convert it to a UCS-4 array and operate on
     * that array, then convert it back before returning. Utf8string_size is the size
     * of the utf8string array but that doesn't help us much for the array we need to
     * operate on here. We'll just allocate a big array and then cut it off when
     * sending it back.
     *
     * This should come before the specialized calls above but those aren't
     * converted to use UCS-4 yet.
     */
    string = utf8_to_ucs4_cpystr(utf8string);
    dline.vused = ucs4_strlen(string);

    string_size = (2 * MAX(utf8string_size,dline.vused) + 100);
    fs_resize((void **) &string, string_size * sizeof(UCS));

    suspend_busy_cue();
    cols         = ps_global->ttyo->screen_cols;
    prompt_width = utf8_width(utf8prompt);
    too_thin   = 0;
    km_popped  = 0;
    if(y_base > 0)
      real_y_base = y_base;
    else{
        real_y_base = y_base + ps_global->ttyo->screen_rows;
	real_y_base = MAX(real_y_base, 0);
    }

    flush_ordered_messages();
    mark_status_dirty();

    if(flags && *flags & OE_APPEND_CURRENT) /* save a copy in case of cancel */
      saved_original = ucs4_cpystr(string);

    /*
     * build the function key mapping table, skipping predefined keys...
     */
    memset(fkey_table, NO_OP_COMMAND, 12 * sizeof(long));
    for(i = 0, j = 0; escape_list && escape_list[i].ch != -1 && i+j < 12; i++){
	if(i+j == OE_HELP_KEY)
	  j++;

	if(i+j == OE_CANCEL_KEY)
	  j++;

	if(i+j == OE_ENTER_KEY)
	  j++;

	fkey_table[i+j] = escape_list[i].ch;
    }

    /* assumption that HelpType is char **  */
    help_text = help;
    if(help_text){			/*---- Show help text -----*/
	int width = ps_global->ttyo->screen_cols - x_base;

	if(FOOTER_ROWS(ps_global) == 1){
	    km_popped++;
	    FOOTER_ROWS(ps_global) = 3;
	    clearfooter(ps_global);

	    y_base = -3;
	    real_y_base = y_base + ps_global->ttyo->screen_rows;
	}

	for(j = 0; j < 2 && help_text[j]; j++){
	    MoveCursor(real_y_base + 1 + j, x_base);
	    CleartoEOLN();

	    if(width < utf8_width(help_text[j])){
		char *tmp = cpystr(help_text[j]);
		(void) utf8_truncate(tmp, width);
		PutLine0(real_y_base + 1 + j, x_base, tmp);
		fs_give((void **) &tmp);
	    }
	    else
	      PutLine0(real_y_base + 1 + j, x_base, help_text[j]);
	}
    }
    else{
	clrbitmap(bitmap);
	clrbitmap((km = &oe_keymenu)->bitmap);		/* force formatting */
	if(!(flags && (*flags) & OE_DISALLOW_HELP))
	  setbitn(OE_HELP_KEY, bitmap);

	setbitn(OE_ENTER_KEY, bitmap);
	if(!(flags && (*flags) & OE_DISALLOW_CANCEL))
	  setbitn(OE_CANCEL_KEY, bitmap);

	setbitn(OE_CTRL_T_KEY, bitmap);

        /*---- Show the usual possible keys ----*/
	for(i=0,j=0; escape_list && escape_list[i].ch != -1 && i+j < 12; i++){
	    if(i+j == OE_HELP_KEY)
	      j++;

	    if(i+j == OE_CANCEL_KEY)
	      j++;

	    if(i+j == OE_ENTER_KEY)
	      j++;

	    oe_keymenu.keys[i+j].label = escape_list[i].label;
	    oe_keymenu.keys[i+j].name = escape_list[i].name;
	    setbitn(i+j, bitmap);
	}

	for(i = i+j; i < 12; i++)
	  if(!(i == OE_HELP_KEY || i == OE_ENTER_KEY || i == OE_CANCEL_KEY))
	    oe_keymenu.keys[i].name = NULL;

	draw_keymenu(km, bitmap, cols, 1-FOOTER_ROWS(ps_global), 0, FirstMenu);
    }
    
    if(pico_usingcolor() && VAR_PROMPT_FORE_COLOR &&
       VAR_PROMPT_BACK_COLOR &&
       pico_is_good_color(VAR_PROMPT_FORE_COLOR) &&
       pico_is_good_color(VAR_PROMPT_BACK_COLOR)){
	lastc = pico_get_cur_color();
	if(lastc){
	    promptc = new_color_pair(VAR_PROMPT_FORE_COLOR,
				     VAR_PROMPT_BACK_COLOR);
	    (void)pico_set_colorp(promptc, PSC_NONE);
	}
    }
    else
      StartInverse();

    /*
     * if display length isn't wide enough to support input,
     * shorten up the prompt...
     */
    if((dline.dwid = cols - (x_base + prompt_width)) < MIN_OPT_ENT_WIDTH){
	char *p;
	unsigned got_width;

	/*
	 * Scoot prompt pointer forward at least (MIN_OPT_ENT_WIDTH - dline.dwid) screencells.
	 */
	p = utf8_count_forw_width(utf8prompt, MIN_OPT_ENT_WIDTH-dline.dwid, &got_width);
	if(got_width < MIN_OPT_ENT_WIDTH-dline.dwid)
	  p = utf8_count_forw_width(utf8prompt, MIN_OPT_ENT_WIDTH+1-dline.dwid, &got_width);

	if(p){
	    prompt_width = utf8_width(p);
	    dline.dwid =  cols - (x_base + prompt_width);
	    utf8prompt = p;
	}
    }

    /*
     * How many UCS-4 characters will we need to make up the width dwid? It could be
     * unlimited because of zero-width characters, I suppose, but realistically it
     * isn't going to be much more than dwid.
     */
    dline.dlen = 2 * dline.dwid + 100;

    dline.dl    = (UCS *) fs_get(dline.dlen * sizeof(UCS));
    dline.olddl = (UCS *) fs_get(dline.dlen * sizeof(UCS));
    memset(dline.dl,    0, dline.dlen * sizeof(UCS));
    memset(dline.olddl, 0, dline.dlen * sizeof(UCS));

    dline.movecursor = MoveCursor;
    dline.writechar  = Writewchar;

    dline.row   = real_y_base;
    dline.col   = x_base + prompt_width;

    dline.vl    = string;
    dline.vlen  = --string_size;		/* -1 for terminating zero */
    dline.vbase = field_pos = 0;

#ifdef	_WINDOWS
    cursor_shown = mswin_showcaret(1);
#endif
    
    PutLine0(real_y_base, x_base, utf8prompt);

    /*
     * If appending, position field_pos at end of input.
     */
    if(flags && *flags & OE_APPEND_CURRENT)
      while(string[field_pos])
	field_pos++;

    passwd = (flags && *flags & OE_PASSWD_NOAST) ? 10 :
              (flags && *flags & OE_PASSWD)       ?  1 : 0;
    line_paint(field_pos, &dline, &passwd);

    /*----------------------------------------------------------------------
      The main loop
	loops until someone sets the return_v.
      ----------------------------------------------------------------------*/
    return_v = -10;

    while(return_v == -10) {

#ifdef	MOUSE
	mouse_in_content(KEY_MOUSE, -1, -1, 0x5, 0);
	register_mfunc(mouse_in_content, 
		       real_y_base, x_base + prompt_width,
		       real_y_base, ps_global->ttyo->screen_cols);
#endif
#ifdef	_WINDOWS
	mswin_allowpaste(MSWIN_PASTE_LINE);
	g_mc_row = real_y_base;
	g_mc_col = x_base + prompt_width;
	mswin_mousetrackcallback(pcpine_oe_cursor);
#endif

	/* Timeout 10 min to keep imap mail stream alive */
	ps_global->conceal_sensitive_debugging = passwd ? 1 : 0;
        ucs = read_char(600);
	ps_global->conceal_sensitive_debugging = 0;

#ifdef	MOUSE
	clear_mfunc(mouse_in_content);
#endif
#ifdef	_WINDOWS
	mswin_allowpaste(MSWIN_PASTE_DISABLE);
	mswin_mousetrackcallback(NULL);
#endif

	/*
	 * Don't want to intercept all characters if typing in passwd.
	 * We select an ad hoc set that we will catch and let the rest
	 * through.  We would have caught the set below in the big switch
	 * but we skip the switch instead.  Still catch things like ^K,
	 * DELETE, ^C, RETURN.
	 */
	if(passwd)
	  switch(ucs){
            case ctrl('F'):  
	    case KEY_RIGHT:
            case ctrl('B'):
	    case KEY_LEFT:
            case ctrl('U'):
            case ctrl('A'):
	    case KEY_HOME:
            case ctrl('E'):
	    case KEY_END:
	    case TAB:
	      goto ok_for_passwd;
	  }

        if(too_thin && ucs != KEY_RESIZE && ucs != ctrl('Z') && ucs != ctrl('C'))
          goto bleep;

	switch(ucs){

	    /*--------------- KEY RIGHT ---------------*/
          case ctrl('F'):  
	  case KEY_RIGHT:
	    if(field_pos >= string_size || string[field_pos] == '\0')
              goto bleep;

	    line_paint(++field_pos, &dline, &passwd);
	    break;

	    /*--------------- KEY LEFT ---------------*/
          case ctrl('B'):
	  case KEY_LEFT:
	    if(field_pos <= 0)
	      goto bleep;

	    line_paint(--field_pos, &dline, &passwd);
	    break;

          /*-------------------- WORD SKIP --------------------*/
	  case ctrl('@'):
	    /*
	     * Note: read_char *can* return NO_OP_COMMAND which is
	     * the def'd with the same value as ^@ (NULL), BUT since
	     * read_char has a big timeout (>25 secs) it won't.
	     */

	    /* skip thru current word */
	    while(string[field_pos]
		  && isalnum((unsigned char) string[field_pos]))
	      field_pos++;

	    /* skip thru current white space to next word */
	    while(string[field_pos]
		  && !isalnum((unsigned char) string[field_pos]))
	      field_pos++;

	    line_paint(field_pos, &dline, &passwd);
	    break;

          /*--------------------  RETURN --------------------*/
	  case PF4:
	    if(F_OFF(F_USE_FK,ps_global)) goto bleep;
	  case ctrl('J'): 
	  case ctrl('M'): 
	    return_v = 0;
	    break;

          /*-------------------- Destructive backspace --------------------*/
	  case '\177': /* DEL */
	  case ctrl('H'):
            /*   Try and do this with by telling the terminal to delete a
                 a character. If that fails, then repaint the rest of the
                 line, acheiving the same much less efficiently
             */
	    if(field_pos <= 0)
	      goto bleep;

	    field_pos--;
	    /* drop thru to pull line back ... */

          /*-------------------- Delete char --------------------*/
	  case ctrl('D'): 
	  case KEY_DEL: 
            if(field_pos >= string_size || !string[field_pos])
	      goto bleep;

	    dline.vused--;
	    for(s2 = &string[field_pos]; *s2 != 0; s2++)
	      *s2 = s2[1];

	    *s2 = 0;			/* Copy last NULL */
	    line_paint(field_pos, &dline, &passwd);
	    if(flags)		/* record change if requested  */
	      *flags |= OE_USER_MODIFIED;

	    break;

            /*--------------- Kill line -----------------*/
          case ctrl('K'):
            if(kill_buffer != NULL)
              fs_give((void **) &kill_buffer);

	    if(field_pos != 0 || string[0]){
		if(!passwd && F_ON(F_DEL_FROM_DOT, ps_global))
		  dline.vused -= ucs4_strlen(&string[i = field_pos]);
		else
		  dline.vused = i = 0;

		kill_buffer = ucs4_cpystr(&string[field_pos = i]);
		string[field_pos] = '\0';
		line_paint(field_pos, &dline, &passwd);
		if(flags)		/* record change if requested  */
		  *flags |= OE_USER_MODIFIED;
	    }

            break;

            /*------------------- Undelete line --------------------*/
          case ctrl('U'):
            if(kill_buffer == NULL)
              goto bleep;

            /* Make string so it will fit */
            kb = ucs4_cpystr(kill_buffer);
            if(ucs4_strlen(kb) + ucs4_strlen(string) > string_size) 
                kb[string_size - ucs4_strlen(string)] = '\0';
                       
            if(string[field_pos] == '\0') {
                /*--- adding to the end of the string ----*/
                for(k = kb; *k; k++)
		  string[field_pos++] = *k;

                string[field_pos] = '\0';
            }
	    else{
		int shift;

		shift = ucs4_strlen(kb);

		/* shift field_pos ... end to right */
		for(k = &string[field_pos] + ucs4_strlen(&string[field_pos]);
		    k >= &string[field_pos]; k--)
		  *(k+shift) = *k;

                for(k = kb; *k; k++)
		  string[field_pos++] = *k;
            }

	    if(*kb && flags)		/* record change if requested  */
	      *flags |= OE_USER_MODIFIED;

	    dline.vused = ucs4_strlen(string);
            fs_give((void **) &kb);
	    line_paint(field_pos, &dline, &passwd);
            break;
            
	    /*-------------------- Interrupt --------------------*/
	  case ctrl('C'): /* ^C */ 
	    if(F_ON(F_USE_FK,ps_global) || (flags && ((*flags) & OE_DISALLOW_CANCEL)))
	      goto bleep;

	    goto cancel;

	  case PF2:
	    if(F_OFF(F_USE_FK,ps_global) || (flags && ((*flags) & OE_DISALLOW_CANCEL)))
	      goto bleep;

	  cancel:
	    return_v = 1;
	    if(saved_original){
		for(i = 0; saved_original[i]; i++)
		  string[i] = saved_original[i];

		string[i] = 0;
	    }

	    break;

          case ctrl('A'):
	  case KEY_HOME:
            /*-------------------- Start of line -------------*/
	    line_paint(field_pos = 0, &dline, &passwd);
            break;

          case ctrl('E'):
	  case KEY_END:
            /*-------------------- End of line ---------------*/
	    line_paint(field_pos = dline.vused, &dline, &passwd);
            break;

	    /*-------------------- Help --------------------*/
	  case ctrl('G') : 
	  case PF1:
	    if(flags && ((*flags) & OE_DISALLOW_HELP))
	      goto bleep;
	    else if(FOOTER_ROWS(ps_global) == 1 && km_popped == 0){
		km_popped++;
		FOOTER_ROWS(ps_global) = 3;
		clearfooter(ps_global);
		if(lastc)
		  (void)pico_set_colorp(lastc, PSC_NONE);
		else
		  EndInverse();

		draw_keymenu(km, bitmap, cols, 1-FOOTER_ROWS(ps_global),
			     0, FirstMenu);

		if(promptc)
		  (void)pico_set_colorp(promptc, PSC_NONE);
		else
		  StartInverse();

		mark_keymenu_dirty();
		y_base = -3;
		dline.row = real_y_base = y_base + ps_global->ttyo->screen_rows;
		PutLine0(real_y_base, x_base, utf8prompt);
		memset(dline.dl,    0, dline.dlen * sizeof(UCS));
		memset(dline.olddl, 0, dline.dlen * sizeof(UCS));
		line_paint(field_pos, &dline, &passwd);
		break;
	    }

	    if(FOOTER_ROWS(ps_global) > 1){
		mark_keymenu_dirty();
		return_v = 3;
	    }
	    else
	      goto bleep;

	    break;


#ifdef	MOUSE
			    /* Mouse support untested in pine 5.00 */
	  case KEY_MOUSE :
	    {
	      MOUSEPRESS mp;
	      int w;

	      mouse_get_last (NULL, &mp);

	      switch(mp.button){
		case M_BUTTON_LEFT :			/* position cursor */
		  mp.col -= dline.col;

		  /*
		   * We have to figure out which character is under the cursor.
		   * This is complicated by the fact that characters may
		   * be other than one cell wide.
		   */

		  /* the -1 is for the '<' when text is offscreen left */
		  w = (dline.vbase > 0) ? mp.col-1 : mp.col;

		  if(mp.col <= 0)
		    field_pos = dline.vbase - 1;
		  else{
		    if(dline.vused <= dline.vbase
		       || ucs4_str_width_a_to_b(dline.vl,dline.vbase,dline.vused-1) <= w)
		      field_pos = dline.vused;
		    else{
		      /*
		       * Find index of 1st character that causes the
		       * width to be > w.
		       */
		      for(i = 0;
			  ucs4_str_width_a_to_b(dline.vl,dline.vbase,dline.vbase+i) <= w;
			  i++)
			;

		      field_pos = dline.vbase + i;
		    }
		  }
		  
		  field_pos = MIN(MAX(field_pos, 0), dline.vused);

		  /* just allow line_paint to choose vbase */
		  line_paint(field_pos, &dline, &passwd);
		  break;

		case M_BUTTON_RIGHT :
#ifdef	_WINDOWS

		  /*
		   * Same as M_BUTTON_LEFT except we paste in text after
		   * moving the cursor.
		   */

		  mp.col -= dline.col;

		  /* the -1 is for the '<' when text is offscreen left */
		  w = (dline.vbase > 0) ? mp.col-1 : mp.col;

		  if(mp.col <= 0)
		    field_pos = dline.vbase - 1;
		  else{
		    if(dline.vused <= dline.vbase
		       || ucs4_str_width_a_to_b(dline.vl,dline.vbase,dline.vused-1) <= w)
		      field_pos = dline.vused;
		    else{
		      /*
		       * Find index of 1st character that causes the
		       * width to be > w.
		       */
		      for(i = 0;
			  ucs4_str_width_a_to_b(dline.vl,dline.vbase,dline.vbase+i) <= w;
			  i++)
			;

		      field_pos = dline.vbase + i;
		    }
		  }
		  
		  field_pos = MIN(MAX(field_pos, 0), dline.vused);

		  line_paint(field_pos, &dline, &passwd);

		  mswin_allowpaste(MSWIN_PASTE_LINE);
		  mswin_paste_popup();
		  mswin_allowpaste(MSWIN_PASTE_DISABLE);
		  break;
#endif

		case M_BUTTON_MIDDLE :			/* NO-OP for now */
		default:				/* just ignore */
		  break;
	      }
	    }

	    break;
#endif


          case NO_OP_IDLE:
	    /*
	     * Keep mail stream alive by checking for new mail.
	     * If we're asking for a password in a login prompt
	     * we don't want to check for new_mail because the
	     * new mail check might be what got us here in the first
	     * place (because of a filter trying to save a message).
	     * If we need to wait for the user to come back then
	     * the caller will just have to deal with the failure
	     * to login.
	     */
	    i = -1;
	    if(!ps_global->no_newmail_check_from_optionally_enter)
	      i = new_mail(0, 2, NM_DEFER_SORT);

	    if(sp_expunge_count(ps_global->mail_stream) &&
	       flags && ((*flags) & OE_SEQ_SENSITIVE))
	      goto cancel;

	    if(i < 0){
	      line_paint(field_pos, &dline, &passwd);
	      break;			/* no changes, get on with life */
	    }
	    /* Else fall into redraw */

	    /*-------------------- Redraw --------------------*/
	  case ctrl('L'):
            /*---------------- re size ----------------*/
          case KEY_RESIZE:
            
	    dline.row = real_y_base = y_base > 0 ? y_base :
					 y_base + ps_global->ttyo->screen_rows;
	    if(lastc)
	      (void)pico_set_colorp(lastc, PSC_NONE);
	    else
	      EndInverse();

            ClearScreen();
            redraw_titlebar();
            if(ps_global->redrawer != (void (*)(void))NULL)
              (*ps_global->redrawer)();

            redraw_keymenu();
	    if(promptc)
	      (void)pico_set_colorp(promptc, PSC_NONE);
	    else
	      StartInverse();
            
            PutLine0(real_y_base, x_base, utf8prompt);
            cols     =  ps_global->ttyo->screen_cols;
            too_thin = 0;
            if(cols < x_base + prompt_width + 4){
		Writechar(BELL, 0);
                PutLine0(real_y_base, 0, "Screen's too thin. Ouch!");
                too_thin = 1;
            }
	    else{
		dline.col  = x_base + prompt_width;
		dline.dwid = cols - (x_base + prompt_width);
		dline.dlen = 2 * dline.dwid + 100;
		fs_resize((void **) &dline.dl, (size_t) dline.dlen * sizeof(UCS));
		fs_resize((void **) &dline.olddl, (size_t) dline.dlen * sizeof(UCS));
		memset(dline.dl,    0, dline.dlen * sizeof(UCS));
		memset(dline.olddl, 0, dline.dlen * sizeof(UCS));
		line_paint(field_pos, &dline, &passwd);
            }

            fflush(stdout);

            dprint((9,
                    "optionally_enter  RESIZE new_cols:%d  too_thin: %d\n",
                       cols, too_thin));
            break;

	  case PF3 :		/* input to potentially remap */
	  case PF5 :
	  case PF6 :
	  case PF7 :
	  case PF8 :
	  case PF9 :
	  case PF10 :
	  case PF11 :
	  case PF12 :
	      if(F_ON(F_USE_FK,ps_global)
		 && fkey_table[ucs - PF1] != NO_OP_COMMAND)
		ucs = fkey_table[ucs - PF1]; /* remap function key input */
  
          default:
	    if(escape_list){		/* in the escape key list? */
		for(j=0; escape_list[j].ch != -1; j++){
		    if(escape_list[j].ch == ucs){
			return_v = escape_list[j].rval;
			break;
		    }
		}

		if(return_v != -10)
		  break;
	    }

	    if(ucs < 0x80 && FILTER_THIS((unsigned char) ucs)){
       bleep:
		putc(BELL, stdout);
		continue;
	    }

       ok_for_passwd:
	    /*--- Insert a character -----*/
	    if(dline.vused >= string_size)
	      goto bleep;

	    /*---- extending the length of the string ---*/
	    for(s2 = &string[++dline.vused]; s2 - string > field_pos; s2--)
	      *s2 = *(s2-1);

	    string[field_pos++] = ucs;
	    line_paint(field_pos, &dline, &passwd);
	    if(flags)		/* record change if requested  */
	      *flags |= OE_USER_MODIFIED;
		    
	}   /*---- End of switch on char ----*/
    }

#ifdef	_WINDOWS
    if(!cursor_shown)
      mswin_showcaret(0);
#endif

    if(dline.dl)
      fs_give((void **) &dline.dl);

    if(dline.olddl)
      fs_give((void **) &dline.olddl);

    if(saved_original) 
      fs_give((void **) &saved_original);

    if(kill_buffer)
      fs_give((void **) &kill_buffer);

    /*
     * Change string back into UTF-8.
     */
    candidate = ucs4_to_utf8_cpystr(string);

    if(string) 
      fs_give((void **) &string);

    if(candidate){
	strncpy(utf8string, candidate, utf8string_size);
	utf8string[utf8string_size-1] = '\0';
	fs_give((void **) &candidate);
    }

    if (!(flags && (*flags) & OE_KEEP_TRAILING_SPACE))
      removing_trailing_white_space(utf8string);

    if(lastc){
	(void)pico_set_colorp(lastc, PSC_NONE);
	free_color_pair(&lastc);
	if(promptc)
	  free_color_pair(&promptc);
    }
    else
      EndInverse();

    MoveCursor(real_y_base, x_base); /* Move the cursor to show we're done */
    fflush(stdout);
    resume_busy_cue(0);
    if(km_popped){
	FOOTER_ROWS(ps_global) = 1;
	clearfooter(ps_global);
	ps_global->mangled_body = 1;
    }

    return(return_v);
}


/*----------------------------------------------------------------------
    Check to see if the given command is reasonably valid
  
  Args:  ch -- the character to check

 Result:  A valid command is returned, or a well know bad command is returned.
 
 ---*/
UCS
validatekeys(UCS ch)
{
    if(F_ON(F_USE_FK,ps_global)){
	if(ch >= 'a' && ch <= 'z')
	  return(KEY_JUNK);
    }
    else{
	if(ch >= PF1 && ch <= PF12)
	  return(KEY_JUNK);
    }

    return(ch);
}



/*----------------------------------------------------------------------
  Prepend config'd commands to keyboard input
  
  Args:  ch -- pointer to storage for returned command

 Returns: TRUE if we're passing back a useful command, FALSE otherwise
 
 ---*/
int
process_config_input(UCS *ch)
{
    static char firsttime = (char) 1;
    int c;
    unsigned long octets_so_far, remaining_octets, ret = 0;
    unsigned char *inputp;
    UCS ucs;
    unsigned char inputbuf[20];

    /* commands in config file */
    if(ps_global->initial_cmds && *ps_global->initial_cmds) {
	/*
	 * There are a few commands that may require keyboard input before
	 * we enter the main command loop.  That input should be interactive,
	 * not from our list of initial keystrokes.
	 */
	if(ps_global->dont_use_init_cmds)
	  return(ret);

	c = *ps_global->initial_cmds++;

	/*
	 * Use enough bytes to make up a character and convert it to UCS-4.
	 */
	if(c < 0x80 || c > KEY_BASE){
	    *ch = (UCS) c;
	    ret = 1;
	}
	else{
	    memset(inputbuf, 0, sizeof(inputbuf));
	    inputbuf[0] = (0xff & c);
	    octets_so_far = 1;

	    while(!ret){
		remaining_octets = octets_so_far;
		inputp = inputbuf;
		ucs = (UCS) utf8_get(&inputp, &remaining_octets);
		switch(ucs){
		  case U8G_ENDSTRG:	/* incomplete character, wait */
		  case U8G_ENDSTRI:	/* incomplete character, wait */
		    if(!*ps_global->initial_cmds || octets_so_far >= sizeof(inputbuf)){
			*ch = BADESC;
			ret = 1;
		    }
		    else
		      inputbuf[octets_so_far++] = (0xff & *ps_global->initial_cmds++);

		    break;

		  default:
		    if(ucs & U8G_ERROR || ucs == UBOGON)
		      *ch = BADESC;
		    else
		      *ch = ucs;

		    ret = 1;
		    break;
		}
	    }
	}

	if(!*ps_global->initial_cmds && ps_global->free_initial_cmds){
	    fs_give((void **) &ps_global->free_initial_cmds);
	    ps_global->initial_cmds = NULL;
	}

	return(ret);
    }

    if(firsttime) {
	firsttime = 0;
	if(ps_global->in_init_seq) {
	    ps_global->in_init_seq = 0;
	    ps_global->save_in_init_seq = 0;
	    clear_cursor_pos();
	    F_SET(F_USE_FK,ps_global,ps_global->orig_use_fkeys);
	    /* draw screen */
	    *ch = (UCS) ctrl('L');
	    return(1);
	}
    }

    return(0);
}


#define	TAPELEN	256
static int   tape[TAPELEN];
static long  recorded = 0L;
static short length  = 0;


/*
 * record user keystrokes
 *
 * Args:  ch -- the character to record
 *
 * Returns: character recorded
 */
int
key_recorder(int ch)
{
    tape[recorded++ % TAPELEN] = ch;
    if(length < TAPELEN)
      length++;

    return(ch);
}


/*
 * playback user keystrokes
 *
 * Args:  ch -- ignored
 *
 * Returns: character played back or -1 to indicate end of tape
 */
int
key_playback(int ch)
{
    ch = length ? tape[(recorded + TAPELEN - length--) % TAPELEN] : -1;
    return(ch);
}


/*
 * recent_keystroke - verbose version of key_playback
 */
int
recent_keystroke(int *cv, char *cs, size_t cslen)
{
    int c;

    if((c = key_playback(0)) != -1){
	*cv = c;
	snprintf(cs, cslen, "%.32s", pretty_command(c));
	return(0);
    }

    return(-1);
}


#ifdef	_WINDOWS
int
pcpine_oe_cursor(col, row)
    int  col;
    long row;
{
    return((row == g_mc_row
	    && col >= g_mc_col
	    && col < ps_global->ttyo->screen_cols)
	    ? MSWIN_CURSOR_IBEAM
	    : MSWIN_CURSOR_ARROW);
}
#endif
