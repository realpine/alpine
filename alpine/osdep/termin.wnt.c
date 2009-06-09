#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: termin.unx.c 193 2006-10-20 17:09:26Z mikes@u.washington.edu $";
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

#include "../../pith/debug.h"
#include "../../pith/state.h"
#include "../../pith/conf.h"
#include "../../pith/detach.h"

#include "../pico/estruct.h"
#include "../pico/pico.h"

#include "../../pico/osdep/raw.h"
#include "../../pico/osdep/signals.h"
#include "../../pico/osdep/mouse.h"
#include "../../pico/keydefs.h"

#include "../talk.h"
#include "../radio.h"
#include "../dispfilt.h"
#include "../signal.h"
#include "../status.h"
#include "../titlebar.h"

#include "../../pico/osdep/mswin.h"

#include "termin.gen.h"
#include "termin.wnt.h"
#include "termout.gen.h"

#define	RETURN_CH(X)	return(key_rec ? ((*key_rec)(X)) : (int)(X))

/* global to tell us if the window was resized. */
static int DidResize = FALSE;

int pine_window_resize_callback(void);

/*----------------------------------------------------------------------
    Initialize the tty driver to do single char I/O and whatever else

 Input:  struct pine

 Result: tty driver is put in raw mode
  ----------------------------------------------------------------------*/
int
init_tty_driver(struct pine *pine)
{
    mswin_showwindow();
    mswin_setresizecallback (pine_window_resize_callback);
    init_mouse ();			/* always a mouse under windows? */
    return(PineRaw(1));
}


/*----------------------------------------------------------------------
       End use of the tty, put it back into it's normal mode

 Input:  struct pine

 Result: tty driver mode change
  ----------------------------------------------------------------------*/
void
end_tty_driver(struct pine *pine)
{
    dprint((2, "about to end_tty_driver\n"));
    mswin_clearresizecallback (pine_window_resize_callback);
}

/*----------------------------------------------------------------------
    Actually set up the tty driver

   Args: state -- which state to put it in. 1 means go into raw, 0 out of

  Result: returns 0 if successful and -1 if not.
  ----*/

int
PineRaw(int state)
{
    ps_global->low_speed = 0;
    return(0);
}

/*----------------------------------------------------------------------
   Read input characters with lots of processing for arrow keys and such

 Input:  none

 Result: returns the character read. Possible special chars defined h file


    This deals with function and arrow keys as well. 
  It returns ^T for up , ^U for down, ^V for forward and ^W for back.
  These are just sort of arbitrarily picked and might be changed.
  They are defined in defs.h. Didn't want to use 8 bit chars because
  the values are signed chars, though it ought to work with negative 
  values. 

  The idea is that this routine handles all escape codes so it done in
  only one place. Especially so the back arrow key can work when entering
  things on a line. Also so all function keys can be broken and not
  cause weird things to happen.
----------------------------------------------------------------------*/

UCS
read_char(int tm)
{
    unsigned   ch = 0;
    time_t     timein;
    int (*key_rec)(int);

    key_rec = key_recorder;
    if(ps_global->conceal_sensitive_debugging && debug < 10)
      key_rec = NULL;

    if(process_config_input((int *) &ch))
      RETURN_CH(ch);

    if (DidResize) {
	 DidResize = FALSE;
	 fix_windsize(ps_global);
	 return(KEY_RESIZE);
    }

    mswin_setcursor(MSWIN_CURSOR_ARROW);

    if(tm){
	timein = time(0L);
	/* mswin_charavail() Yields control to other window apps. */
	while (!mswin_charavail()) {
	    if(time(0L) >= timein + (time_t) tm){
		ch = (tm < IDLE_TIMEOUT) ? NO_OP_COMMAND : NO_OP_IDLE;
		goto gotone;
	    }
	    if (DidResize) {
		DidResize = FALSE;
		fix_windsize(ps_global);
		return(KEY_RESIZE);
	    }
 	    if(checkmouse(&ch,0,0,0))
	      goto gotone;
	}
    }
    else
      while(!mswin_charavail())
	if(checkmouse(&ch,0,0,0))
	  goto gotone;

    ch = mswin_getc_fast();

    /*
     * mswin_getc_fast() returns UCS type codes (see keydefs.h). Map
     *  those values to what is expected from read_char(): Ctrl keys being
     *  in the 0..0x1f range, NO_OP_COMMAND, etc.
     */
    if(ch == NODATA)
        ch = NO_OP_COMMAND;
    else if(ch & CTRL)
    {
        ch &= ~CTRL;
        if(ch >= '@' && ch < '@' + 32) {
            ch -= '@';
        }
        else {
            /*
             * This could be a ctrl key this part of Pine doesn't understand.
             *  For example, CTRL|KEY_LEFT. Map these to nops.
             */
            ch = NO_OP_COMMAND;
        }
    }

gotone:
    /* More obtuse key mapping.  If it is a mouse event, the return
     * may be KEY_MOUSE, which indicates to the upper layer that it
     * is a mouse event.  Return it here to avoid the code that
     * follows which would do a (ch & 0xff).
     */
    if (ch == KEY_MOUSE)
      RETURN_CH(ch);

    /*
     * WARNING: Hack notice.
     * the mouse interaction complicates this expression a bit as 
     * if function key mode is set, PFn values are setup for return
     * by the mouse event catcher.  For now, just special case them
     * since they don't conflict with any of the DOS special keys.
     */
    if((ch & 0xff) == ctrl('Z'))
      RETURN_CH(do_suspend());

    RETURN_CH (ch);
}

void
flush_input(void)
{
    mswin_flush_input();
}

void
init_keyboard(int use_fkeys)
{
}

void
end_keyboard(int use_fkeys)
{
}

int
pre_screen_config_opt_enter(char *utf8string, int utf8string_size, 
			    char *utf8prompt, ESCKEY_S *escape_list, 
			    HelpType help, int *flags)
{
    mswin_setwindow(NULL, NULL, NULL, NULL, NULL, NULL);
    return(win_dialog_opt_enter(utf8string, utf8string_size, utf8prompt, escape_list,
				help, flags));
}

int
win_dialog_opt_enter(char *utf8string, int utf8string_size, char *utf8prompt, 
		     ESCKEY_S *escape_list, HelpType help, int *flags)
{
    MDlgButton button_list[12];
    LPTSTR     free_names[12];
    LPTSTR     free_labels[12];
    int        i, b, return_v;
    char     **help_text;
    char      *utf8, *saved_string = NULL;
    UCS       *string, *s, *prompt;
    int        n;

    memset(&free_names, 0, sizeof(LPTSTR) * 12);
    memset(&free_labels, 0, sizeof(LPTSTR) * 12);
    memset (&button_list, 0, sizeof (MDlgButton) * 12);
    b = 0;
    for (i = 0; escape_list && escape_list[i].ch != -1 && i < 11; ++i) {
	if (escape_list[i].name != NULL
	    && escape_list[i].ch > 0 && escape_list[i].ch < 256) {
	    button_list[b].ch = escape_list[i].ch;
	    button_list[b].rval = escape_list[i].rval;
	    free_names[b] = utf8_to_lptstr(escape_list[i].name);
	    button_list[b].name = free_names[b];
	    free_labels[b] = utf8_to_lptstr(escape_list[i].label);
	    button_list[b].label = free_labels[b];
	    b++;
	}
    }

    button_list[b].ch = -1;

    if(utf8string)
      saved_string = cpystr(utf8string);

    /* assumption here is that HelpType is char **  */
    help_text = help;

    n = utf8string_size;
    string = (UCS *) fs_get(n * sizeof(UCS));
    s = utf8_to_ucs4_cpystr(utf8string);
    if(s){
	ucs4_strncpy(string, s, n);
	string[n-1] = '\0';
	fs_give((void **) &s);
    }

    prompt = utf8_to_ucs4_cpystr(utf8prompt ? utf8prompt : "");

    return_v = mswin_dialog(prompt, string, n, 
			    (flags && *flags & OE_APPEND_CURRENT),
			    (flags && *flags & OE_PASSWD_NOAST) ? 10 :
			     (flags && *flags & OE_PASSWD)       ?  1 : 0,
			    button_list,
			    help_text, flags ? *flags : OE_NONE);

    for(i = 0; i < 12; i++){
	if(free_names[i])
	  fs_give((void **) &free_names[i]);
	if(free_labels[i])
	  fs_give((void **) &free_labels[i]);
    }

    utf8 = ucs4_to_utf8_cpystr(string);
    if(utf8){
	strncpy(utf8string, utf8, utf8string_size);
	utf8string[utf8string_size-1] = '\0';
	fs_give((void **) &utf8);
    }

    if(string)
      fs_give((void **) &string);

    if(prompt)
      fs_give((void **) &prompt);

    if(flags && (saved_string && !utf8string || !saved_string && utf8string ||
       (saved_string && string && strcmp(saved_string, utf8string))))
      *flags |= OE_USER_MODIFIED;

    if(saved_string)
      fs_give((void **) &saved_string);

    return(return_v);
}


/*----------------------------------------------------------------------
    Flag the fact the window has resized.
*/
int
pine_window_resize_callback (void)
{
    DidResize = TRUE;
    return(0);
}

void
intr_proc(int state)
{
    return;	/* no op */
}

