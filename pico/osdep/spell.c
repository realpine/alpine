#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: spell.c 854 2007-12-07 17:44:43Z hubert@u.washington.edu $";
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
 * Program:	spell.c
 */


#include <system.h>
#include <general.h>

#include "../headers.h"
/*
#include "../estruct.h"
#include "../mode.h"
#include "../pico.h"
#include "../edef.h"
#include "../efunc.h"
#include "../keydefs.h"
*/

#include "../../pith/charconv/filesys.h"

#include "popen.h"
#include "altedit.h"
#include "spell.h"


#ifdef	SPELLER

int  movetoword(UCS *);


static char *spellhelp[] = {
/* TRANSLATORS: Some help text. The ~ characters cause
   the characters they are in front of to be bold. */
  N_("Spell Check Help"),
  " ",
  N_("        The spell checker examines all words in the text.  It then"),
  N_("        offers each misspelled word for correction while simultaneously"),
  N_("        highlighting it in the text.  To leave a word unchanged simply"),
  N_("~        hit ~R~e~t~u~r~n at the edit prompt.  If a word has been corrected,"),
  N_("        each occurrence of the incorrect word is offered for replacement."),
  " ",
  N_("~        Spell checking can be cancelled at any time by typing ~^~C (~F~3)"),
  N_("        after exiting help."),
  " ",
  N_("End of Spell Check Help"),
  " ",
  NULL
};


static char *pinespellhelp[] = {
  N_("Spell Check Help"),
  " ",
  N_("\tThe spell checker examines all words in the text.  It then"),
  N_("\toffers each misspelled word for correction while simultaneously"),
  N_("\thighlighting it in the text.  To leave a word unchanged simply"),
  N_("\thit Return at the edit prompt.  If a word has been corrected,"),
  N_("\teach occurrence of the incorrect word is offered for replacement."),
  " ",
  N_("\tSpell checking can be cancelled at any time by typing ^C (F3)"),
  N_("\tafter exiting help."),
  " ",
  N_("End of Spell Check Help"),
  " ",
  NULL
};

#ifndef	_WINDOWS
/*
 * spell() - check for potentially missspelled words and offer them for
 *           correction
 */
int
spell(int f, int n)
{
    int    status, next, ret;
    char   ccb[NLINE], *sp, *fn, *lp, *wsp, c, spc[NLINE];
    UCS   *b;
    UCS    wb[NLINE], cb[NLINE];
    EML    eml;

    setimark(0, 1);
    emlwrite(_("Checking spelling..."), NULL); 	/* greetings */

    if(alt_speller)
      return(alt_editor(1, 0));			/* f == 1 means fork speller */

    if((fn = writetmp(0, NULL)) == NULL){
	emlwrite(_("Can't write temp file for spell checker"), NULL);
	return(-1);
    }

    if((sp = (char *)getenv("SPELL")) == NULL)
      sp = SPELLER;

    /* exists? */
    ret = (strlen(sp) + 1);
    snprintf(spc, sizeof(spc), "%s", sp);

    for(lp = spc, ret = FIOERR; *lp; lp++){
	if((wsp = strpbrk(lp, " \t")) != NULL){
	    c = *wsp;
	    *wsp = '\0';
	}

	if(strchr(lp, '/')){
	    ret = fexist(lp, "x", (off_t *)NULL);
	}
	else{
	    char *path, fname[MAXPATH+1];

	    if(!(path = getenv("PATH")))
	      path = ":/bin:/usr/bin";

	    ret = ~FIOSUC;
	    while(ret != FIOSUC && *path && pathcat(fname, &path, lp))
	      ret = fexist(fname, "x", (off_t *)NULL);
	}

	if(wsp)
	  *wsp = c;

	if(ret == FIOSUC)
	  break;
    }

    if(ret != FIOSUC){
	eml.s = sp;
        emlwrite(_("\007Spell-checking file \"%s\" not found"), &eml);
	return(-1);
    }

    snprintf(ccb, sizeof(ccb), "( %s ) < %s", sp, fn);
    if(P_open(ccb) != FIOSUC){ 		/* read output from command */
	our_unlink(fn);
	emlwrite(_("Can't fork spell checker"), NULL);
	return(-1);
    }

    ret = 1;
    while(ffgetline(wb, NLINE, NULL, 0) == FIOSUC && ret){
	if((b = ucs4_strchr(wb, (UCS) '\n')) != NULL)
	  *b = '\0';

	ucs4_strncpy(cb, wb, NLINE);
	cb[NLINE-1] = '\0';

	gotobob(0, 1);

	status = TRUE;
	next = 1;

	while(status){
	    if(next++)
	      if(movetoword(wb) != TRUE)
		break;

	    update();
	    (*term.t_rev)(1);
	    pputs(wb, 1);			/* highlight word */
	    (*term.t_rev)(0);

	    if(ucs4_strcmp(cb, wb)){
		char prompt[2*NLINE + 32];
		char *wbu, *cbu;

		wbu = ucs4_to_utf8_cpystr(wb);
		cbu = ucs4_to_utf8_cpystr(cb);

		snprintf(prompt, sizeof(prompt), _("Replace \"%s\" with \"%s\""), wbu, cbu);
		status=mlyesno_utf8(prompt, TRUE);
		if(wbu)
		  fs_give((void **) &wbu);
		if(cbu)
		  fs_give((void **) &cbu);
	    }
	    else{
		UCS *p;

		p = utf8_to_ucs4_cpystr(_("Edit a replacement: "));
		status=mlreplyd(p, cb, NLINE, QDEFLT, NULL);
		if(p)
		  fs_give((void **) &p);
	    }


	    curwp->w_flag |= WFMOVE;		/* put cursor back */
	    sgarbk = 0;				/* fake no-keymenu-change! */
	    update();
	    pputs(wb, 0);			/* un-highlight */

	    switch(status){
	      case TRUE:
		chword(wb, cb);			/* correct word    */
	      case FALSE:
		update();			/* place cursor */
		break;
	      case ABORT:
		emlwrite(_("Spell Checking Cancelled"), NULL);
		ret = FALSE;
		status = FALSE;
		break;
	      case HELPCH:
		if(Pmaster){
		    VARS_TO_SAVE *saved_state;

		    saved_state = save_pico_state();
		    (*Pmaster->helper)(pinespellhelp, 
				       _("Help with Spelling Checker"), 1);
		    if(saved_state){
			restore_pico_state(saved_state);
			free_pico_state(saved_state);
		    }
		}
		else
		  pico_help(spellhelp, _("Help with Spelling Checker"), 1);

	      case (CTRL|'L'):
		next = 0;			/* don't get next word */
		sgarbf = TRUE;			/* repaint full screen */
		update();
		status = TRUE;
		continue;
	      default:
		emlwrite("Huh?", NULL);		/* shouldn't get here, but.. */
		status = TRUE;
		sleep(1);
		break;
	    }
	    
	    forwword(0, 1);			/* goto next word */
	}
    }

    P_close();					/* clean up */
    our_unlink(fn);
    swapimark(0, 1);
    curwp->w_flag |= WFHARD|WFMODE;
    sgarbk = TRUE;

    if(ret)
      emlwrite(_("Done checking spelling"), NULL);

    return(ret);
}


#endif /* UNIX */


/* 
 * movetoword() - move to the first occurance of the word w
 *
 *	returns:
 *		TRUE upon success
 *		FALSE otherwise
 */
int
movetoword(UCS *w)
{
    int      i;
    int      ret  = FALSE;
    int	     olddoto;
    LINE     *olddotp;
    register int   off;				/* curwp offset */
    register LINE *lp;				/* curwp line   */

    olddoto = curwp->w_doto;			/* save where we are */
    olddotp = curwp->w_dotp;

    curwp->w_bufp->b_mode |= MDEXACT;		/* case sensitive */
    while(forscan(&i, w, NULL, 0, 1) == TRUE){
	if(i)
	  break;				/* wrap NOT allowed! */

	lp  = curwp->w_dotp;			/* for convenience */
	off = curwp->w_doto;

	/*
	 * We want to minimize the number of substrings that we report
	 * as matching a misspelled word...
	 */
	if(off == 0 || !ucs4_isalpha(lgetc(lp, off - 1).c)){
	    off += ucs4_strlen(w);
	    if((!ucs4_isalpha(lgetc(lp, off).c) || off == llength(lp))
	       && lgetc(lp, 0).c != '>'){
		ret = TRUE;
		break;
	    }
	}

	forwchar(0, 1);				/* move on... */

    }
    curwp->w_bufp->b_mode ^= MDEXACT;		/* case insensitive */

    if(ret == FALSE){
	curwp->w_dotp = olddotp;
	curwp->w_doto = olddoto;
    }
    else
      curwp->w_flag |= WFHARD;

    return(ret);
}

#endif	/* SPELLER */
