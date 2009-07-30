#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: bind.c 857 2007-12-08 00:49:45Z hubert@u.washington.edu $";
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
 *
 * Program:	Key binding routines
 */

/*	This file is for functions having to do with key bindings,
	descriptions, help commands, and command line execution.

	written 11-feb-86 by Daniel Lawrence
								*/

#include	"headers.h"

int arraylen(char **);

/* 
 * help - help function for pico (UW pared down version of uemacs).
 *	  this function will intentionally garbage with helpful 
 *	  tips and then wait for a ' ' to be hit to then update the 
 *        screen.
 */


static char *helptext[] = {
    /* TRANSLATORS: The next several lines go together as help text.
       Leave the ~ characters where they are, they cause the following
       character to be printed in boldface as long as the first character
       of the line is also a ~. */
    N_("        Pico Help Text"),
    " ",
    N_("        Pico is designed to be a simple, easy-to-use text editor with a"),
    N_("        layout very similar to the Alpine mailer.  The status line at the"),
    N_("        top of the display shows pico's version, the current file being"),
    N_("        edited and whether or not there are outstanding modifications"),
    N_("        that have not been saved.  The third line from the bottom is used"),
    N_("        to report informational messages and for additional command input."),
    N_("        The bottom two lines list the available editing commands."),
    " ",
    N_("        Each character typed is automatically inserted into the buffer"),
    N_("        at the current cursor position.  Editing commands and cursor"),
    N_("        movement (besides arrow keys) are given to pico by typing"),
    N_("        special control-key sequences.  A caret, '^', is used to denote"),
    N_("~        the control key, sometimes marked \"CTRL\", so the ~C~T~R~L~-~q key"),
    N_("~        combination is written as ~^~Q."),
    " ",
    N_("        The following functions are available in pico (where applicable,"),
    N_("        corresponding function key commands are in parentheses)."),
    " ",
    N_("~        ~^~G (~F~1)   Display this help text."),
    " ",
    N_("~        ~^~F        move Forward a character."),
    N_("~        ~^~B        move Backward a character."),
    N_("~        ~^~P        move to the Previous line."),
    N_("~        ~^~N        move to the Next line."),
    N_("~        ~^~A        move to the beginning of the current line."),
    N_("~        ~^~E        move to the End of the current line."),
    N_("~        ~^~V (~F~8)   move forward a page of text."),
    N_("~        ~^~Y (~F~7)   move backward a page of text."),
    " ",
    N_("~        ~^~W (~F~6)   Search for (where is) text, neglecting case."),
    N_("~        ~^~L        Refresh the display."),
    " ",
    N_("~        ~^~D        Delete the character at the cursor position."),
    N_("~        ~^~^        Mark cursor position as beginning of selected text."),
    N_("                  Note: Setting mark when already set unselects text."),
    N_("~        ~^~K (~F~9)   Cut selected text (displayed in inverse characters)."),
    N_("                  Note: The selected text's boundary on the cursor side"),
    N_("                        ends at the left edge of the cursor.  So, with "),
    N_("                        selected text to the left of the cursor, the "),
    N_("                        character under the cursor is not selected."),
    N_("~        ~^~U (~F~1~0)  Uncut (paste) last cut text inserting it at the"),
    N_("                  current cursor position."),
    N_("~        ~^~I        Insert a tab at the current cursor position."),
    " ",
    N_("~        ~^~J (~F~4)   Format (justify) the current paragraph."),
    N_("                  Note: paragraphs delimited by blank lines or indentation."),
    N_("~        ~^~T (~F~1~2)  To invoke the spelling checker"),
    N_("~        ~^~C (~F~1~1)  Report current cursor position"),
    " ",
    N_("~        ~^~R (~F~5)   Insert an external file at the current cursor position."),
    N_("~        ~^~O (~F~3)   Output the current buffer to a file, saving it."),
    N_("~        ~^~X (~F~2)   Exit pico, saving buffer."),
    "    ",
    N_("    End of Help."),
    " ",
    NULL
};


/*
 * arraylen - return the number of bytes in an array of char
 */
int
arraylen(char **array)
{
    register int i=0;

    while(array[i++] != NULL) ;
    return(i);
}


/*
 * whelp - display help text for the composer and pico
 */
int
whelp(int f, int n)
{
    if(term.t_mrow == 0){  /* blank keymenu in effect */
	if(km_popped == 0){
	    /* cause keymenu display */
	    km_popped = 2;
	    if(!Pmaster)
	      sgarbf = TRUE;

	    return(TRUE);
	}
    }

    if(Pmaster){
	VARS_TO_SAVE *saved_state;

	saved_state = save_pico_state();
	(*Pmaster->helper)(Pmaster->composer_help,
			   Pmaster->headents
			     ? _("Help for the Alpine Composer")
			     : _("Help for Signature Editor"),
			   1);
	if(saved_state){
	    restore_pico_state(saved_state);
	    free_pico_state(saved_state);
	}

	ttresize();
	picosigs();			/* restore any altered handlers */
	curwp->w_flag |= WFMODE;
	if(km_popped)  /* this will unpop us */
	  curwp->w_flag |= WFHARD;
    }
    else{
	int mrow_was_zero = 0;

	/* always want keyhelp showing during help */
	if(term.t_mrow == 0){
	    mrow_was_zero++;
	    term.t_mrow = 2;
	}

	/* TRANSLATORS: Pico is the name of a program */
	pico_help(helptext, _("Help for Pico"), 1);
	/* put it back the way it was */
	if(mrow_was_zero)
	  term.t_mrow = 0;
    }

    sgarbf = TRUE;
    return(FALSE);
}

static KEYMENU menu_scroll[] = {
    {NULL, NULL, KS_NONE},		{NULL, NULL, KS_NONE},
    {NULL, NULL, KS_NONE},		{NULL, NULL, KS_NONE},
    {NULL, NULL, KS_NONE},		{NULL, NULL, KS_NONE},
    {"^X", N_("Exit Help"), KS_NONE},	{NULL, NULL, KS_NONE},
    {NULL, NULL, KS_NONE},		{NULL, NULL, KS_NONE},
    {NULL, NULL, KS_NONE},		{NULL, NULL, KS_NONE}
};
#define	PREV_KEY	3
#define	NEXT_KEY	9


#define	OVERLAP	2		/* displayed page overlap */

/*
 * scrollw - takes beginning row and ending row to diplay an array
 *           of text lines.  returns either 0 if scrolling terminated
 *           normally or the value of a ctrl character typed to end it.
 *
 * updates - 
 *     01/11/89 - added stripe call if 1st char is tilde - '~'
 *
 */
int
wscrollw(int begrow, int endrow, char *utf8textp[], int textlen)
{
    register int	loffset = 0; 
    register int	prevoffset = -1; 
    register int	dlines;
    register int	i;
    register int	cont;
    register int	done = 0;
    register char	*buf;
    UCS	 c;
     
    dlines = endrow - begrow - 1;
    while(!done) {
        /*
         * diplay a page loop ...
         */
	if(prevoffset != loffset){
       	    for(i = 0; i < dlines; i++){
                movecursor(i + begrow, 0);
                peeol();
                if((loffset+i) < textlen){
		    buf = _(&(utf8textp[loffset+i][0]));
		    if(*buf == '~'){
			buf++;
			wstripe(begrow+i, 0, buf, '~');
		    }
		    else{
			pputs_utf8(buf, 0);
		    }
                }
            }
	    /*
	     * put up the options prompt
	     */
            movecursor(begrow + dlines, 0);
            cont = (loffset+dlines < textlen);
            if(cont){                               /* continue ? */
		menu_scroll[NEXT_KEY].name  = "^V";
		/* TRANSLATORS: Next Page, a command key label */
		menu_scroll[NEXT_KEY].label = N_("Next Pg");
	    }
	    else
	      menu_scroll[NEXT_KEY].name = NULL;

	    if(loffset){
		menu_scroll[PREV_KEY].name  = "^Y";
		/* TRANSLATORS: Previous Page */
		menu_scroll[PREV_KEY].label = N_("Prev Pg");
	    }
	    else
	      menu_scroll[PREV_KEY].name = NULL;

	    wkeyhelp(menu_scroll);
	}

	(*term.t_flush)();

        c = GetKey();

	prevoffset = loffset;
	switch(c){
	    case  (CTRL|'X') :		/* quit */
 	    case  F2  :
		done = 1;
		break;
	    case  (CTRL|'Y') :		/* prev page */
	    case  F7  :			/* prev page */
		if((loffset-dlines-OVERLAP) > 0){
               	    loffset -= (dlines-OVERLAP);
		}
	        else{
		    if(loffset != 0){
			prevoffset = -1;
		    }
		    else{
		    	(*term.t_beep)();
		    }
		    loffset = 0;
	        }
		break;
	    case  (CTRL|'V') :			/* next page */
 	    case  F8  :
		if(cont){
               	   loffset += (dlines-OVERLAP);
		}
		else{
		   (*term.t_beep)();
		}
		break;
	    case  '\016' :		/* prev-line */
	    case  (CTRL|'N') :
	      if(cont)
		loffset++;
	      else
		(*term.t_beep)();
	      break;
	    case  '\020' :		/* prev-line */
	    case  (CTRL|'P') :
	      if(loffset > 0)
		loffset--;
	      else
		(*term.t_beep)();
	      break;
	    case  '\014' :		/* refresh */
	    case  (CTRL|'L') :		/* refresh */
	        modeline(curwp);
		update();
		prevoffset = -1;
		break;
	    case  NODATA :
	        break;
#ifdef notdef
    /*
     * We don't handle window resize events correctly when in pico help.
     * resize_pico() redraws the edit window instead of the help window.
     * A ^L will redraw the help text. What we'd like is something like
     * a KEY_RESIZE return from GetKey. If we had that we could exit
     * wscrollw with a FALSE return value and have that cause us to loop
     * back into wscrollw with the adjusted size. That would still mean
     * the edit text would be redrawn first...
     */
#endif /* notdef */
	    default :
		unknown_command(c);
		break;
	}
    }

    return(TRUE);
}


/*
 * normalize_cmd - given a char and list of function key to command key
 *		   mappings, return, depending on gmode, the right command.
 *		   The list is an array of (Fkey, command-key) pairs.
 *		   sc is the index in the array that means to ignore fkey
 *		   vs. command key mapping
 *
 *          rules:  1. if c not in table (either fkey or command), let it thru
 *                  2. if c matches, but in other mode, punt it
 */
UCS
normalize_cmd(UCS c, UCS list[][2], int sc)
{
    int i;

    for(i=0; i < 12; i++){
	if(c == list[i][(FUNC&c) ? 0 : 1]){	/* in table? */
	    if(i == sc)				/* SPECIAL CASE! */
	      return(list[i][1]);

	    if(list[i][1] == NODATA)		/* no mapping ! */
	      return(c);

	    if(((FUNC&c) == FUNC) && !((gmode&MDFKEY) == MDFKEY))
	      return(c);	/* not allowed, let caller handle it */
	    else
	      return(list[i][1]);		/* never return func keys */
	}
    }

    return(c);
}


/*
 * rebind - replace the first function with the second
 */
void
rebindfunc(int (*a)(int, int), int (*b)(int, int))
{
    KEYTAB *kp;

    kp = (Pmaster) ? &keytab[0] : &pkeytab[0];

    while(kp->k_fp != NULL){		/* go thru whole list, and */
	if(kp->k_fp == a)
	  kp->k_fp = b;			/* replace all occurances */
	kp++;
    }
}


/*
 * bindtokey - bind function f to command c
 */
int
bindtokey(UCS c, int (*f)(int, int))
{
    KEYTAB *kp, *ktab = (Pmaster) ? &keytab[0] : &pkeytab[0];

    for(kp = ktab; kp->k_fp; kp++)
      if(kp->k_code == c){
	  kp->k_fp = f;			/* set to new function */
	  break;
      }

    /* not found? create new binding */
    if(!kp->k_fp && kp < &ktab[NBINDS]){
	kp->k_code     = c;		/* assign new code and function */
	kp->k_fp       = f;
	(++kp)->k_code = 0;		/* and null out next slot */
	kp->k_fp       = NULL;
    }

    return(TRUE);
}
