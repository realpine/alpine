#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: radio.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
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

#include "headers.h"
#include "radio.h"
#include "keymenu.h"
#include "busy.h"
#include "status.h"
#include "mailcmd.h"
#include "titlebar.h"
#include "roleconf.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/newmail.h"
#include "../pith/util.h"


/*
 * Internal prototypes
 */
int       pre_screen_config_want_to(char *, int, int);
ESCKEY_S *construct_combined_esclist(ESCKEY_S *, ESCKEY_S *);
void      radio_help(int, int, HelpType);
void      draw_radio_prompt(int, unsigned, unsigned, char *);


#define	RAD_BUT_COL	0
#define WANT_TO_BUF     2500


/*
 * want_to's array passed to radio_buttions...
 */
static ESCKEY_S yorn[] = {
    {'y', 'y', "Y", N_("Yes")},
    {'n', 'n', "N", N_("No")},
    {-1, 0, NULL, NULL}
};

int
pre_screen_config_want_to(char *question, int dflt, int on_ctrl_C)
{
    int ret = 0;
    char rep[WANT_TO_BUF], *p;
#ifdef _WINDOWS
    rep[0] = '\0';
    mswin_flush();
    if(strlen(question) + 3 < WANT_TO_BUF){
      snprintf(rep, sizeof(rep), "%s ?", question);
      rep[sizeof(rep)-1] = '\0';
    }

    switch (mswin_yesno_utf8 (*rep ? rep : question)) {
      default:
      case 0:   return (on_ctrl_C);
      case 1:   return ('y');
      case 2:   return ('n');
    }
#endif
    while(!ret){
      fprintf(stdout, "%s? [%c]:", question, dflt);
      fgets(rep, WANT_TO_BUF, stdin);
      if((p = strpbrk(rep, "\r\n")) != NULL)
	*p = '\0';
      switch(*rep){
        case 'Y':
        case 'y':
	  ret = (int)'y';
	  break;
        case 'N':
        case 'n':
	  ret = (int)'n';
	  break;
        case '\0':
	  ret = dflt;
	  break;
        default:
	  break;
      }
    }
    return ret;
}


/*----------------------------------------------------------------------
     Ask a yes/no question in the status line

   Args: question     -- string to prompt user with
         dflt         -- The default answer to the question (should probably
			 be y or n)
         on_ctrl_C    -- Answer returned on ^C
	 help         -- Two line help text
	 flags        -- Flags to modify behavior
			 WT_FLUSH_IN      - Discard pending input.
			 WT_SEQ_SENSITIVE - Caller is sensitive to sequence
			                    number changes caused by
					    unsolicited expunges while we're
					    viewing a message.

 Result: Messes up the status line,
         returns y, n, dflt, on_ctrl_C, or SEQ_EXCEPTION
  ---*/
int
want_to(char *question, int dflt, int on_ctrl_C, HelpType help, int flags)
{
    char *free_this = NULL, *free_this2 = NULL, *prompt;
    int	  rv, width;
    size_t len;

    if(!ps_global->ttyo)
      return(pre_screen_config_want_to(question, dflt, on_ctrl_C));
#ifdef _WINDOWS
    if (mswin_usedialog ()) {
	mswin_flush ();
	switch (mswin_yesno_utf8 (question)) {
	default:
	case 0:		return (on_ctrl_C);
	case 1:		return ('y');
	case 2:		return ('n');
        }
    }
#endif

    /*----
       One problem with adding the (y/n) here is that shrinking the 
       screen while in radio_buttons() will cause it to get chopped
       off. It would be better to truncate the question passed in
       here and leave the full "(y/n) [x] : " on.
      ----*/

    len = strlen(question) + 4;
    free_this = (char *) fs_get(len);
    width = utf8_width(question);

    if(width + 2 < ps_global->ttyo->screen_cols){
	snprintf(free_this, len, "%s? ", question);
	free_this[len-1] = '\0';
	prompt = free_this;
    }
    else if(width + 1 < ps_global->ttyo->screen_cols){
	snprintf(free_this, len, "%s?", question);
	free_this[len-1] = '\0';
	prompt = free_this;
    }
    else if(width < ps_global->ttyo->screen_cols){
	snprintf(free_this, len, "%s", question);
	free_this[len-1] = '\0';
	prompt = free_this;
    }
    else{
	free_this2 = (char *) fs_get(len);
	snprintf(free_this2, len, "%s? ", question);
	prompt = short_str(free_this2, free_this, len, ps_global->ttyo->screen_cols-1, MidDots);
    }

    if(on_ctrl_C == 'n')	/* don't ever let cancel == 'n' */
      on_ctrl_C = 0;

    rv = radio_buttons(prompt,
	(ps_global->ttyo->screen_rows > 4) ? - FOOTER_ROWS(ps_global) : -1,
	yorn, dflt, on_ctrl_C, help, flags);

    if(free_this)
      fs_give((void **) &free_this);

    if(free_this2)
      fs_give((void **) &free_this2);

    return(rv);
}


int
one_try_want_to(char *question, int dflt, int on_ctrl_C, HelpType help, int flags)
{
    char     *q2;
    int	      rv;
    size_t    l;

    l = strlen(question) + 5;
    q2 = fs_get((l+1) * sizeof(char));
    strncpy(q2, question, l);
    q2[l] = '\0';
    (void) utf8_truncate(q2, ps_global->ttyo->screen_cols - 6);
    strncat(q2, "? ", l+1 - strlen(q2) - 1);
    q2[l] = '\0';
    rv = radio_buttons(q2,
	(ps_global->ttyo->screen_rows > 4) ? - FOOTER_ROWS(ps_global) : -1,
	yorn, dflt, on_ctrl_C, help, flags | RB_ONE_TRY);
    fs_give((void **) &q2);

    return(rv);
}


/*----------------------------------------------------------------------
    Prompt user for a choice among alternatives

Args --  utf8prompt:    The prompt for the question/selection
         line:      The line to prompt on, if negative then relative to bottom
         esc_list:  ESC_KEY_S list of keys
         dflt:	    The selection when the <CR> is pressed (should probably
		      be one of the chars in esc_list)
         on_ctrl_C: The selection when ^C is pressed
         help_text: Text to be displayed on bottom two lines
	 flags:     Logically OR'd flags modifying our behavior to:
		RB_FLUSH_IN    - Discard any pending input chars.
		RB_ONE_TRY     - Only give one chance to answer.  Returns
				 on_ctrl_C value if not answered acceptably
				 on first try.
		RB_NO_NEWMAIL  - Quell the usual newmail check.
		RB_SEQ_SENSITIVE - The caller is sensitive to sequence number
				   changes so return on_ctrl_C if an
				   unsolicited expunge happens while we're
				   viewing a message.
		RB_RET_HELP    - Instead of the regular internal handling
				 way of handling help_text, this just causes
				 radio_buttons to return 3 when help is
				 asked for, so that the caller handles it
				 instead.
	
	 Note: If there are enough keys in the esc_list to need a second
	       screen, and there is no help, then the 13th key will be
	       put in the help position.

Result -- Returns the letter pressed. Will be one of the characters in the
          esc_list argument, or dflt, or on_ctrl_C, or SEQ_EXCEPTION.

This will pause for any new status message to be seen and then prompt the user.
The prompt will be truncated to fit on the screen. Redraw and resize are
handled along with ^Z suspension. Typing ^G will toggle the help text on and
off. Character types that are not buttons will result in a beep (unless one_try
is set).
  ----*/
int
radio_buttons(char *utf8prompt, int line, ESCKEY_S *esc_list, int dflt,
	      int on_ctrl_C, HelpType help_text, int flags)
{
    UCS              ucs;
    register int     ch, real_line;
    char            *q, *ds = NULL;
    unsigned         maxcol;
    int              max_label, i, start, fkey_table[12];
    int		     km_popped = 0;
    struct key	     rb_keys[12];
    struct key_menu  rb_keymenu;
    bitmap_t	     bitmap;
    struct variable *vars = ps_global->vars;
    COLOR_PAIR      *lastc = NULL, *promptc = NULL;

#ifdef	_WINDOWS
    int		     cursor_shown;

    if (mswin_usedialog()){
	MDlgButton button_list[25];
	LPTSTR     free_names[25];
	LPTSTR     free_labels[25];
	int        b, i, ret;
	char     **help;

	memset(&free_names, 0, sizeof(LPTSTR) * 25);
	memset(&free_labels, 0, sizeof(LPTSTR) * 25);
	memset(&button_list, 0, sizeof (MDlgButton) * 25);
	b = 0;

	if(flags & RB_RET_HELP){
	    if(help_text != NO_HELP)
	      panic("RET_HELP and help in radio_buttons!");

	    button_list[b].ch = '?';
	    button_list[b].rval = 3;
	    button_list[b].name = TEXT("?");
	    free_labels[b] = utf8_to_lptstr(N_("Help"));
	    button_list[b].label = free_labels[b];
	    ++b;
	}

	for(i = 0; esc_list && esc_list[i].ch != -1 && i < 23; ++i){
	  if(esc_list[i].ch != -2){
	    button_list[b].ch = esc_list[i].ch;
	    button_list[b].rval = esc_list[i].rval;
	    free_names[b] = utf8_to_lptstr(esc_list[i].name);
	    button_list[b].name = free_names[b];
	    free_labels[b] = utf8_to_lptstr(esc_list[i].label);
	    button_list[b].label = free_labels[b];
	    ++b;
	  }
	}

	button_list[b].ch = -1;
	
	/* assumption here is that HelpType is char **  */
	help = help_text;

	ret = mswin_select(utf8prompt, button_list, dflt, on_ctrl_C, help, flags);
	for(i = 0; i < 25; i++){
	    if(free_names[i])
	      fs_give((void **) &free_names[i]);
	    if(free_labels[i])
	      fs_give((void **) &free_labels[i]);
	}

	return (ret);
    }

#endif /* _WINDOWS */

    suspend_busy_cue();
    flush_ordered_messages();		/* show user previous status msgs */
    mark_status_dirty();		/* clear message next display call */
    real_line = line > 0 ? line : ps_global->ttyo->screen_rows + line;
    MoveCursor(real_line, RAD_BUT_COL);
    CleartoEOLN();

    /*---- Find widest label ----*/
    max_label = 0;
    for(i = 0; esc_list && esc_list[i].ch != -1 && i < 11; i++){
      if(esc_list[i].ch == -2) /* -2 means to skip this key and leave blank */
	continue;
      if(esc_list[i].name)
        max_label = MAX(max_label, utf8_width(esc_list[i].name));
    }

    if(ps_global->ttyo->screen_cols - max_label - 1 > 0)
      maxcol = ps_global->ttyo->screen_cols - max_label - 1;
    else
      maxcol = 0;

    /*
     * We need to be able to truncate q, so copy it in case it is
     * a readonly string.
     */
    q = cpystr(utf8prompt);

    /*---- Init structs for keymenu ----*/
    for(i = 0; i < 12; i++)
      memset((void *)&rb_keys[i], 0, sizeof(struct key));

    memset((void *)&rb_keymenu, 0, sizeof(struct key_menu));
    rb_keymenu.how_many = 1;
    rb_keymenu.keys     = rb_keys;

    /*---- Setup key menu ----*/
    start = 0;
    clrbitmap(bitmap);
    memset(fkey_table, NO_OP_COMMAND, 12 * sizeof(int));
    if(flags & RB_RET_HELP && help_text != NO_HELP)
      panic("RET_HELP and help in radio_buttons!");

    /* if shown, always at position 0 */
    if(help_text != NO_HELP || flags & RB_RET_HELP){
	rb_keymenu.keys[0].name  = "?";
	rb_keymenu.keys[0].label = N_("Help");
	setbitn(0, bitmap);
	fkey_table[0] = ctrl('G');
	start++;
    }

    if(on_ctrl_C){
	rb_keymenu.keys[1].name  = "^C";
	rb_keymenu.keys[1].label = N_("Cancel");
	setbitn(1, bitmap);
	fkey_table[1] = ctrl('C');
	start++;
    }

    start = start ? 2 : 0;
    /*---- Show the usual possible keys ----*/
    for(i=start; esc_list && esc_list[i-start].ch != -1; i++){
	/*
	 * If we have an esc_list item we'd like to put in the non-existent
	 * 13th slot, and there is no help, we put it in the help slot
	 * instead.  We're hacking now...!
	 *
	 * We may also have invisible esc_list items that don't show up
	 * on the screen.  We use this when we have two different keys
	 * which are synonyms, like ^P and KEY_UP.  If all the slots are
	 * already full we can still fit invisible keys off the screen to
	 * the right.  A key is invisible if it's label is "".
	 */
	if(i >= 12){
	    if(esc_list[i-start].label
	       && esc_list[i-start].label[0] != '\0'){  /* visible */
		if(i == 12){  /* special case where we put it in help slot */
		    if(help_text != NO_HELP)
		  panic("Programming botch in radio_buttons(): too many keys");

		    if(esc_list[i-start].ch != -2)
		      setbitn(0, bitmap); /* the help slot */

		    fkey_table[0] = esc_list[i-start].ch;
		    rb_keymenu.keys[0].name  = esc_list[i-start].name;
		    if(esc_list[i-start].ch != -2
		       && esc_list[i-start].rval == dflt
		       && esc_list[i-start].label){
		        size_t l;

			l = strlen(esc_list[i-start].label) + 2;
			ds = (char *)fs_get((l+1) * sizeof(char));
			snprintf(ds, l+1, "[%s]", esc_list[i-start].label);
			ds[l] = '\0';
			rb_keymenu.keys[0].label = ds;
		    }
		    else
		      rb_keymenu.keys[0].label = esc_list[i-start].label;
		}
		else
		  panic("Botch in radio_buttons(): too many keys");
	    }
	}
	else{
	    if(esc_list[i-start].ch != -2)
	      setbitn(i, bitmap);

	    fkey_table[i] = esc_list[i-start].ch;
	    rb_keymenu.keys[i].name  = esc_list[i-start].name;
	    if(esc_list[i-start].ch != -2
	       && esc_list[i-start].rval == dflt
	       && esc_list[i-start].label){
	        size_t l;

		l = strlen(esc_list[i-start].label) + 2;
		ds = (char *)fs_get((l+1) * sizeof(char));
		snprintf(ds, l+1, "[%s]", esc_list[i-start].label);
		ds[l] = '\0';
		rb_keymenu.keys[i].label = ds;
	    }
	    else
	      rb_keymenu.keys[i].label = esc_list[i-start].label;
	}
    }

    for(; i < 12; i++)
      rb_keymenu.keys[i].name = NULL;

    ps_global->mangled_footer = 1;

#ifdef	_WINDOWS
    cursor_shown = mswin_showcaret(1);
#endif

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

    draw_radio_prompt(real_line, RAD_BUT_COL, maxcol, q);

    while(1){
        fflush(stdout);

	/*---- Paint the keymenu ----*/
	if(lastc)
	  (void)pico_set_colorp(lastc, PSC_NONE);
	else
	  EndInverse();

	draw_keymenu(&rb_keymenu, bitmap, ps_global->ttyo->screen_cols,
		     1 - FOOTER_ROWS(ps_global), 0, FirstMenu);
	if(promptc)
	  (void)pico_set_colorp(promptc, PSC_NONE);
	else
	  StartInverse();

	MoveCursor(real_line, MIN(RAD_BUT_COL+utf8_width(q), maxcol+1));

	if(flags & RB_FLUSH_IN)
	  flush_input();

  newcmd:
	/* Timeout 5 min to keep imap mail stream alive */
        ucs = read_char(600);
        dprint((2,
                   "Want_to read: %s (0x%x)\n", pretty_command(ucs), ucs));
	if((ucs < 0x80) && isupper((unsigned char) ucs))
	  ucs = tolower((unsigned char) ucs);

	if(F_ON(F_USE_FK,ps_global)
	   && (((ucs < 0x80) && isalpha((unsigned char) ucs) && !strchr("YyNn",(int) ucs))
	       || ((ucs >= PF1 && ucs <= PF12)
		   && (ucs = fkey_table[ucs - PF1]) == NO_OP_COMMAND))){
	    /*
	     * The funky test above does two things.  It maps
	     * esc_list character commands to function keys, *and* prevents
	     * character commands from input while in function key mode.
	     * NOTE: this breaks if we ever need more than the first
	     * twelve function keys...
	     */
	    if(flags & RB_ONE_TRY){
		ch = ucs = on_ctrl_C;
	        goto out_of_loop;
	    }

	    Writechar(BELL, 0);
	    continue;
	}

        switch(ucs){

          default:
	    for(i = 0; esc_list && esc_list[i].ch != -1; i++)
	      if(ucs == esc_list[i].ch){
		  int len, n;

		  MoveCursor(real_line,len=MIN(RAD_BUT_COL+utf8_width(q),maxcol+1));
		  for(n = 0, len = ps_global->ttyo->screen_cols - len;
		      esc_list[i].label && esc_list[i].label[n] && len > 0;
		      n++, len--)
		    Writechar(esc_list[i].label[n], 0);

		  ch = esc_list[i].rval;
		  goto out_of_loop;
	      }

	    if(flags & RB_ONE_TRY){
		ch = on_ctrl_C;
	        goto out_of_loop;
	    }

	    Writechar(BELL, 0);
	    break;

          case ctrl('M'):
          case ctrl('J'):
            ch = dflt;
	    for(i = 0; esc_list && esc_list[i].ch != -1; i++)
	      if(ch == esc_list[i].rval){
		  int len, n;

		  MoveCursor(real_line,len=MIN(RAD_BUT_COL+utf8_width(q),maxcol+1));
		  for(n = 0, len = ps_global->ttyo->screen_cols - len;
		      esc_list[i].label && esc_list[i].label[n] && len > 0;
		      n++, len--)
		    Writechar(esc_list[i].label[n], 0);
		  break;
	      }

            goto out_of_loop;

          case ctrl('C'):
	    if(on_ctrl_C || (flags & RB_ONE_TRY)){
		ch = on_ctrl_C;
		goto out_of_loop;
	    }

	    Writechar(BELL, 0);
	    break;


          case '?':
          case ctrl('G'):
	    if(FOOTER_ROWS(ps_global) == 1 && km_popped == 0){
		km_popped++;
		FOOTER_ROWS(ps_global) = 3;
		line = -3;
		real_line = ps_global->ttyo->screen_rows + line;
		if(lastc)
		  (void)pico_set_colorp(lastc, PSC_NONE);
		else
		  EndInverse();

		clearfooter(ps_global);
		if(promptc)
		  (void)pico_set_colorp(promptc, PSC_NONE);
		else
		  StartInverse();

		draw_radio_prompt(real_line, RAD_BUT_COL, maxcol, q);
		break;
	    }

	    if(flags & RB_RET_HELP){
		ch = 3;
		goto out_of_loop;
	    }
	    else if(help_text != NO_HELP && FOOTER_ROWS(ps_global) > 1){
		mark_keymenu_dirty();
		if(lastc)
		  (void)pico_set_colorp(lastc, PSC_NONE);
		else
		  EndInverse();

		MoveCursor(real_line + 1, RAD_BUT_COL);
		CleartoEOLN();
		MoveCursor(real_line + 2, RAD_BUT_COL);
		CleartoEOLN();
		radio_help(real_line, RAD_BUT_COL, help_text);
		sleep(5);
		MoveCursor(real_line, MIN(RAD_BUT_COL+utf8_width(q), maxcol+1));
		if(promptc)
		  (void)pico_set_colorp(promptc, PSC_NONE);
		else
		  StartInverse();
	    }
	    else
	      Writechar(BELL, 0);

            break;
            

          case NO_OP_COMMAND:
	    goto newcmd;		/* misunderstood escape? */

          case NO_OP_IDLE:		/* UNODIR, keep the stream alive */
	    if(flags & RB_NO_NEWMAIL)
	      goto newcmd;

	    i = new_mail(0, VeryBadTime, NM_DEFER_SORT);
	    if(sp_expunge_count(ps_global->mail_stream)
	       && flags & RB_SEQ_SENSITIVE){
		if(on_ctrl_C)
		  ch = on_ctrl_C;
		else
		  ch = SEQ_EXCEPTION;

		goto out_of_loop;
	    }

	    if(i < 0)
	      break;		/* no changes, get on with life */
            /* Else fall into redraw to adjust displayed numbers and such */


          case KEY_RESIZE:
          case ctrl('L'):
            real_line = line > 0 ? line : ps_global->ttyo->screen_rows + line;
	    if(lastc)
	      (void)pico_set_colorp(lastc, PSC_NONE);
	    else
	      EndInverse();

            ClearScreen();
            redraw_titlebar();
            if(ps_global->redrawer != NULL)
              (*ps_global->redrawer)();
	    if(FOOTER_ROWS(ps_global) == 3 || km_popped)
              redraw_keymenu();

	    if(ps_global->ttyo->screen_cols - max_label - 1 > 0)
	      maxcol = ps_global->ttyo->screen_cols - max_label - 1;
	    else
	      maxcol = 0;

	    if(promptc)
	      (void)pico_set_colorp(promptc, PSC_NONE);
	    else
	      StartInverse();

	    draw_radio_prompt(real_line, RAD_BUT_COL, maxcol, q);
            break;

        } /* switch */
    }

  out_of_loop:

#ifdef	_WINDOWS
    if(!cursor_shown)
      mswin_showcaret(0);
#endif

    fs_give((void **) &q);
    if(ds)
      fs_give((void **) &ds);

    if(lastc){
	(void) pico_set_colorp(lastc, PSC_NONE);
	free_color_pair(&lastc);
	if(promptc)
	  free_color_pair(&promptc);
    }
    else
      EndInverse();

    fflush(stdout);
    resume_busy_cue(0);
    if(km_popped){
	FOOTER_ROWS(ps_global) = 1;
	clearfooter(ps_global);
	ps_global->mangled_body = 1;
    }

    return(ch);
}


#define OTHER_RETURN_VAL 1300
#define KEYS_PER_LIST 8

/*
 * This should really be part of radio_buttons itself, I suppose. It was
 * easier to do it this way. This is for when there are more than 12
 * possible commands. We could have all the radio_buttons calls call this
 * instead of radio_buttons, or rename this to radio_buttons.
 *
 * Radio_buttons is limited to 10 visible commands unless there is no Help,
 * in which case it is 11 visible commands.
 * Double_radio_buttons is limited to 16 visible commands because it uses
 * slots 3 and 4 for readability and the OTHER CMD.
 */
int
double_radio_buttons(char *prompt, int line, ESCKEY_S *esc_list, int dflt, int on_ctrl_C, HelpType help_text, int flags)
{
    ESCKEY_S *list = NULL, *list1 = NULL, *list2 = NULL;
    int       i = 0, j;
    int       v = OTHER_RETURN_VAL, listnum = 0;

#ifdef _WINDOWS
    if(mswin_usedialog())
      return(radio_buttons(prompt, line, esc_list, dflt, on_ctrl_C,
			   help_text, flags));
#endif

    /* check to see if it will all fit in one */
    while(esc_list && esc_list[i].ch != -1)
      i++;

    i++;		/* for ^C */
    if(help_text != NO_HELP)
      i++;
    
    if(i <= 12)
      return(radio_buttons(prompt, line, esc_list, dflt, on_ctrl_C,
			   help_text, flags));

    /*
     * Won't fit, split it into two lists.
     *
     * We can fit at most 8 items in the visible list. The rest of
     * the commands have to be invisible. Each of list1 and list2 should
     * have no more than 8 visible (name != "" || label != "") items.
     */
    list1 = (ESCKEY_S *)fs_get((KEYS_PER_LIST+1) * sizeof(*list1));
    memset(list1, 0, (KEYS_PER_LIST+1) * sizeof(*list1));
    list2 = (ESCKEY_S *)fs_get((KEYS_PER_LIST+1) * sizeof(*list2));
    memset(list2, 0, (KEYS_PER_LIST+1) * sizeof(*list2));

    for(j=0,i=0; esc_list[i].ch != -1 && j < KEYS_PER_LIST; j++,i++)
      list1[j] = esc_list[i];
    
    list1[j].ch = -1;

    for(j=0; esc_list[i].ch != -1 && j < KEYS_PER_LIST; j++,i++)
      list2[j] = esc_list[i];

    list2[j].ch = -1;

    list = construct_combined_esclist(list1, list2);

    while(v == OTHER_RETURN_VAL){
	v = radio_buttons(prompt,line,list,dflt,on_ctrl_C,help_text,flags);
	if(v == OTHER_RETURN_VAL){
	    fs_give((void **) &list);
	    listnum = 1 - listnum;
	    list = construct_combined_esclist(listnum ? list2 : list1,
					      listnum ? list1 : list2);
	}
    }

    if(list)
      fs_give((void **) &list);
    if(list1)
      fs_give((void **) &list1);
    if(list2)
      fs_give((void **) &list2);

    return(v);
}


ESCKEY_S *
construct_combined_esclist(ESCKEY_S *list1, ESCKEY_S *list2)
{
    ESCKEY_S *list;
    int       i, j=0, count;
    
    count = 2;	/* for blank key and for OTHER key */
    for(i=0; list1 && list1[i].ch != -1; i++)
      count++;
    for(i=0; list2 && list2[i].ch != -1; i++)
      count++;
    
    list = (ESCKEY_S *) fs_get((count + 1) * sizeof(*list));
    memset(list, 0, (count + 1) * sizeof(*list));

    list[j].ch    = -2;			/* leave blank */
    list[j].rval  = 0;
    list[j].name  = NULL;
    list[j++].label = NULL;

    list[j].ch    = 'o';
    list[j].rval  = OTHER_RETURN_VAL;
    list[j].name  = "O";
    list[j].label = N_("OTHER CMDS");

    /*
     * Make sure that O for OTHER CMD or the return val for OTHER CMD
     * isn't used for something else.
     */
    for(i=0; list1 && list1[i].ch != -1; i++){
	if(list1[i].rval == list[j].rval)
	  panic("1bad rval in d_r");
	if(F_OFF(F_USE_FK,ps_global) && list1[i].ch == list[j].ch)
	  panic("1bad ch in ccl");
    }

    for(i=0; list2 && list2[i].ch != -1; i++){
	if(list2[i].rval == list[j].rval)
	  panic("2bad rval in d_r");
	if(F_OFF(F_USE_FK,ps_global) && list2[i].ch == list[j].ch)
	  panic("2bad ch in ccl");
    }

    j++;

    /* the visible set */
    for(i=0; list1 && list1[i].ch != -1; i++){
	if(i >= KEYS_PER_LIST && list1[i].label[0] != '\0')
	  panic("too many visible keys in ccl");

	list[j++] = list1[i];
    }

    /* the rest are invisible */
    for(i=0; list2 && list2[i].ch != -1; i++){
	list[j] = list2[i];
	list[j].label = "";
	list[j++].name  = "";
    }

    list[j].ch = -1;

    return(list);
}


/*----------------------------------------------------------------------

  ----*/
void
radio_help(int line, int column, HelpType help)
{
    char **text;

    /* assumption here is that HelpType is char **  */
    text = help;
    if(text == NULL)
      return;
    
    MoveCursor(line + 1, column);
    CleartoEOLN();
    if(text[0])
      PutLine0(line + 1, column, text[0]);

    MoveCursor(line + 2, column);
    CleartoEOLN();
    if(text[1])
      PutLine0(line + 2, column, text[1]);

    fflush(stdout);
}


/*----------------------------------------------------------------------
   Paint the screen with the radio buttons prompt
  ----*/
void
draw_radio_prompt(int line, unsigned start_c, unsigned max_c, char *q)
{
    size_t len;
    unsigned x, width, got_width;
    char  *tmpq = NULL, *useq;

    width = utf8_width(q);
    if(width > max_c - start_c + 1){
	tmpq = (char *) fs_get((len=(strlen(q)+1)) * sizeof(char));
	(void) utf8_to_width(tmpq, q, len, max_c - start_c + 1, &got_width);
	useq = tmpq;
	width = got_width;
    }
    else
      useq = q;

    PutLine0(line, start_c, useq);
    x = start_c + width;
    MoveCursor(line, x);
    while(x++ < ps_global->ttyo->screen_cols)
      Writechar(' ', 0);

    MoveCursor(line, start_c + width);
    fflush(stdout);
    if(tmpq)
      fs_give((void **) &tmpq);
}
