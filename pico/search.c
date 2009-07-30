#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: search.c 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $";
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

/*
 * Program:	Searching routines
 *
 * The functions in this file implement commands that search in the forward
 * and backward directions. There are no special characters in the search
 * strings. Probably should have a regular expression search, or something
 * like that.
 *
 */

#include	"headers.h"

int	eq(UCS, UCS);
int	expandp(UCS *, UCS *, int);
int	readnumpat(char *);
void	get_pat_cases(UCS *, UCS *);
int     srpat(char *, UCS *, size_t, int);
int     readpattern(char *, int);
int     replace_pat(UCS *, int *);
int     replace_all(UCS *, UCS *);


#define	FWS_RETURN(RV)	{				\
			    thisflag |= CFSRCH;		\
			    curwp->w_flag |= WFMODE;	\
			    sgarbk = TRUE;		\
			    return(RV);			\
			}


/*
 * Search forward. Get a search string from the user, and search, beginning at
 * ".", for the string. If found, reset the "." to be just after the match
 * string, and [perhaps] repaint the display. Bound to "C-S".
 */

/*	string search input parameters	*/

#define	PTBEG	1	/* leave the point at the begining on search */
#define	PTEND	2	/* leave the point at the end on search */

#define NPMT (2*NLINE+32)


static char *SearchHelpText[] = {
/* TRANSLATORS: Some help text that goes together in a group. */
N_("Help for Search Command"),
" ",
N_("        Enter the words or characters you would like to search"),
N_("~        for, then press ~R~e~t~u~r~n.  The search then takes place."),
N_("        When the characters or words that you entered "),
N_("        are found, the buffer will be redisplayed with the cursor "),
N_("        at the beginning of the selected text."),
" ",
N_("        The most recent string for which a search was made is"),
N_("        displayed in the \"Search\" prompt between the square"),
N_("        brackets.  This string is the default search prompt."),
N_("~        Hitting only ~R~e~t~u~r~n or at the prompt will cause the"),
N_("        search to be made with the default value."),
"  ",
N_("        The text search is not case sensitive, and will examine the"),
N_("        entire message."),
"  ",
N_("        Should the search fail, a message will be displayed."),
"  ",
N_("End of Search Help."),
"  ",
NULL
};


/*
 * Compare two characters. The "bc" comes from the buffer. It has it's case
 * folded out. The "pc" is from the pattern.
 */
int
eq(UCS bc, UCS pc)
{
    if ((curwp->w_bufp->b_mode & MDEXACT) == 0){
	if (bc>='a' && bc<='z')
	  bc -= 0x20;

	if (pc>='a' && pc<='z')
	  pc -= 0x20;
    }

    return(bc == pc);
}


int
forwsearch(int f, int n)
{
  int              status;
  int              wrapt = FALSE, wrapt2 = FALSE;
  int              repl_mode = FALSE;
  UCS              defpat[NPAT];
  int              search = FALSE;
  EML              eml;

    /* resolve the repeat count */
    if (n == 0)
      n = 1;

    if (n < 1)			/* search backwards */
      FWS_RETURN(0);

    defpat[0] = '\0';

    /* ask the user for the text of a pattern */
    while(1){

	if (gmode & MDREPLACE)
	  status = srpat("Search", defpat, NPAT, repl_mode);
	else
	  status = readpattern("Search", TRUE);

	switch(status){
	  case TRUE:                         /* user typed something */
	    search = TRUE;
	    break;

	  case HELPCH:			/* help requested */
	    if(Pmaster){
		VARS_TO_SAVE *saved_state;

		saved_state = save_pico_state();
		(*Pmaster->helper)(Pmaster->search_help,
				   _("Help for Searching"), 1);
		if(saved_state){
		    restore_pico_state(saved_state);
		    free_pico_state(saved_state);
		}
	    }
	    else
	      pico_help(SearchHelpText, _("Help for Searching"), 1);

	  case (CTRL|'L'):			/* redraw requested */
	    pico_refresh(FALSE, 1);
	    update();
	    break;

	  case  (CTRL|'V'):
	    gotoeob(0, 1);
	    mlerase();
	    FWS_RETURN(TRUE);

	  case (CTRL|'Y'):
	    gotobob(0, 1);
	    mlerase();
	    FWS_RETURN(TRUE); 

	  case (CTRL|'T') :
	    switch(status = readnumpat(_("Search to Line Number : "))){
	      case -1 :
		emlwrite(_("Search to Line Number Cancelled"), NULL);
		FWS_RETURN(FALSE);

	      case  0 :
		emlwrite(_("Line number must be greater than zero"), NULL);
		FWS_RETURN(FALSE);

	      case -2 :
		emlwrite(_("Line number must contain only digits"), NULL);
		FWS_RETURN(FALSE);
		
	      case -3 :
		continue;

	      default :
		gotoline(0, status);
		mlerase();
		FWS_RETURN(TRUE);
	    }

	    break;

	  case  (CTRL|'W'):
	    {
		LINE *linep = curwp->w_dotp;
		int   offset = curwp->w_doto;

		gotobop(0, 1);
		gotobol(0, 1);

		/*
		 * if we're asked to backup and we're already
		 *
		 */
		if((lastflag & CFSRCH)
		   && linep == curwp->w_dotp
		   && offset == curwp->w_doto
		   && !(offset == 0 && lback(linep) == curbp->b_linep)){
		    backchar(0, 1);
		    gotobop(0, 1);
		    gotobol(0, 1);
		}
	    }

	    mlerase();
	    FWS_RETURN(TRUE);

	  case  (CTRL|'O'):
	    if(curwp->w_dotp != curbp->b_linep){
		gotoeop(0, 1);
		forwchar(0, 1);
	    }

	    mlerase();
	    FWS_RETURN(TRUE);

	  case (CTRL|'U'):
	    fillbuf(0, 1);
	    mlerase();
	    FWS_RETURN(TRUE);

	  case  (CTRL|'R'):        /* toggle replacement option */
	    repl_mode = !repl_mode;
	    break;

	  default:
	    if(status == ABORT)
	      emlwrite(_("Search Cancelled"), NULL);
	    else
	      mlerase();

	    FWS_RETURN(FALSE);
	}

	/* replace option is disabled */
	if (!(gmode & MDREPLACE)){
	    ucs4_strncpy(defpat, pat, NPAT);
	    defpat[NPAT-1] = '\0';
	    break;
	}
	else if (search){  /* search now */
	    ucs4_strncpy(pat, defpat, NPAT);	/* remember this search for the future */
	    pat[NPAT-1] = '\0';
	    break;
	}
    }

    /*
     * This code is kind of dumb.  What I want is successive C-W 's to 
     * move dot to successive occurences of the pattern.  So, if dot is
     * already sitting at the beginning of the pattern, then we'll move
     * forward a char before beginning the search.  We'll let the
     * automatic wrapping handle putting the dot back in the right 
     * place...
     */
    status = 0;		/* using "status" as int temporarily! */
    while(1){
	if(defpat[status] == '\0'){
	    forwchar(0, 1);
	    break;		/* find next occurence! */
	}

	if(status + curwp->w_doto >= llength(curwp->w_dotp) ||
	   !eq(defpat[status],lgetc(curwp->w_dotp, curwp->w_doto + status).c))
	  break;		/* do nothing! */
	status++;
    }

    /* search for the pattern */
    
    while (n-- > 0) {
	if((status = forscan(&wrapt,defpat,NULL,0,PTBEG)) == FALSE)
	  break;
    }

    /* and complain if not there */
    if (status == FALSE){
      char *utf8;
      UCS x[1];

      x[0] = '\0';

      utf8 = ucs4_to_utf8_cpystr(defpat ? defpat : x); 
      /* TRANSLATORS: reporting the result of a failed search */
      eml.s = utf8;
      emlwrite(_("\"%s\" not found"), &eml);
      if(utf8)
	fs_give((void **) &utf8);
    }
    else if((gmode & MDREPLACE) && repl_mode == TRUE){
        status = replace_pat(defpat, &wrapt2);    /* replace pattern */
	if (wrapt == TRUE || wrapt2 == TRUE){
	    eml.s = (status == ABORT) ? "cancelled but wrapped" : "Wrapped";
	    emlwrite("Replacement %s", &eml);
	}
    }
    else if(wrapt == TRUE){
	emlwrite("Search Wrapped", NULL);
    }
    else if(status == TRUE){
	emlwrite("", NULL);
    }

    FWS_RETURN(status);
}


/* Replace a pattern with the pattern the user types in one or more times. */
int
replace_pat(UCS *defpat, int *wrapt)
{
  register         int status;
  UCS              lpat[NPAT], origpat[NPAT];	/* case sensitive pattern */
  EXTRAKEYS        menu_pat[2];
  int              repl_all = FALSE;
  UCS             *b;
  char             utf8tmp[NPMT];
  UCS              prompt[NPMT];
  UCS             *promptp;

    forscan(wrapt, defpat, NULL, 0, PTBEG);    /* go to word to be replaced */

    lpat[0] = '\0';

    /* additional 'replace all' menu option */
    menu_pat[0].name  = "^X";
    menu_pat[0].key   = (CTRL|'X');
    menu_pat[0].label = N_("Repl All");
    KS_OSDATASET(&menu_pat[0], KS_NONE);
    menu_pat[1].name  = NULL;

    while(1) {

	update();
	(*term.t_rev)(1);
	get_pat_cases(origpat, defpat);
	pputs(origpat, 1);                       /* highlight word */
	(*term.t_rev)(0);

	snprintf(utf8tmp, NPMT, "Replace%s \"", repl_all ? " every" : "");
	b = utf8_to_ucs4_cpystr(utf8tmp);
	if(b){
	    ucs4_strncpy(prompt, b, NPMT);
	    prompt[NPMT-1] = '\0';
	    fs_give((void **) &b);
	}

	promptp = &prompt[ucs4_strlen(prompt)];

	expandp(defpat, promptp, NPMT-(promptp-prompt));
	prompt[NPMT-1] = '\0';
	promptp += ucs4_strlen(promptp);

	b = utf8_to_ucs4_cpystr("\" with");
	if(b){
	    ucs4_strncpy(promptp, b, NPMT-(promptp-prompt));
	    promptp += ucs4_strlen(promptp);
	    prompt[NPMT-1] = '\0';
	    fs_give((void **) &b);
	}

	if(rpat[0] != '\0'){
	    if((promptp-prompt) < NPMT-2){
		*promptp++ = ' ';
		*promptp++ = '[';
		*promptp = '\0';
	    }

	    expandp(rpat, promptp, NPMT-(promptp-prompt));
	    prompt[NPMT-1] = '\0';
	    promptp += ucs4_strlen(promptp);

	    if((promptp-prompt) < NPMT-1){
		*promptp++ = ']';
		*promptp = '\0';
	    }
	}

	if((promptp-prompt) < NPMT-3){
	    *promptp++ = ' ';
	    *promptp++ = ':';
	    *promptp++ = ' ';
	    *promptp = '\0';
	}

	prompt[NPMT-1] = '\0';

	status = mlreplyd(prompt, lpat, NPAT, QDEFLT, menu_pat);

	curwp->w_flag |= WFMOVE;

	switch(status){

	  case TRUE :
	  case FALSE :
	    if(lpat[0]){
	      ucs4_strncpy(rpat, lpat, NPAT); /* remember default */
	      rpat[NPAT-1] = '\0';
	    }
	    else{
	      ucs4_strncpy(lpat, rpat, NPAT); /* use default */
	      lpat[NPAT-1] = '\0';
	    }

	    if (repl_all){
		status = replace_all(defpat, lpat);
	    }
	    else{
		chword(defpat, lpat);	/* replace word    */
		update();
		status = TRUE;
	    }

	    if(status == TRUE)
	      emlwrite("", NULL);

	    return(status);

	  case HELPCH:                      /* help requested */
	    if(Pmaster){
		VARS_TO_SAVE *saved_state;

		saved_state = save_pico_state();
		(*Pmaster->helper)(Pmaster->search_help,
				   _("Help for Searching"), 1);
		if(saved_state){
		    restore_pico_state(saved_state);
		    free_pico_state(saved_state);
		}
	    }
	    else
	      pico_help(SearchHelpText, _("Help for Searching"), 1);

	  case (CTRL|'L'):			/* redraw requested */
	    pico_refresh(FALSE, 1);
	    update();
	    break;

	  case (CTRL|'X'):        /* toggle replace all option */
	    if (repl_all){
		repl_all = FALSE;
		/* TRANSLATORS: abbreviation for Replace All occurences */
		menu_pat[0].label = N_("Repl All");
	    }
	    else{
		repl_all = TRUE;
		/* TRANSLATORS: Replace just one occurence */
		menu_pat[0].label = N_("Repl One");
	    }

	    break;

	  default:
	    if(status == ABORT){
	      emlwrite(_("Replacement Cancelled"), NULL);
	      pico_refresh(FALSE, 1);
	    }
	    else{
		mlerase();
		chword(defpat, origpat);
	    }

	    update();
	    return(FALSE);
	}
    }
}


/* Since the search is not case sensitive, we must obtain the actual pattern 
   that appears in the text, so that we can highlight (and unhighlight) it
   without using the wrong cases  */
void
get_pat_cases(UCS *realpat, UCS *searchpat)
{
  int i, searchpatlen, curoff;

  curoff = curwp->w_doto;
  searchpatlen = ucs4_strlen(searchpat);

  for (i = 0; i < searchpatlen; i++)
    realpat[i] = lgetc(curwp->w_dotp, curoff++).c;

  realpat[searchpatlen] = '\0';
}
    

/* Ask the user about every occurence of orig pattern and replace it with a 
   repl pattern if the response is affirmative. */   
int
replace_all(UCS *orig, UCS *repl)
{
  register         int status = 0;
  UCS             *b;
  UCS              realpat[NPAT];
  char             utf8tmp[NPMT];
  UCS             *promptp;
  UCS              prompt[NPMT];
  int              wrapt, n = 0;
  LINE		  *stop_line   = curwp->w_dotp;
  int		   stop_offset = curwp->w_doto;
  EML              eml;

  while (1)
    if (forscan(&wrapt, orig, stop_line, stop_offset, PTBEG)){
        curwp->w_flag |= WFMOVE;            /* put cursor back */

        update();
	(*term.t_rev)(1);
	get_pat_cases(realpat, orig);
	pputs(realpat, 1);                       /* highlight word */
	(*term.t_rev)(0);
	fflush(stdout);

	snprintf(utf8tmp, NPMT, "Replace \"");
	b = utf8_to_ucs4_cpystr(utf8tmp);
	if(b){
	    ucs4_strncpy(prompt, b, NPMT);
	    prompt[NPMT-1] = '\0';
	    fs_give((void **) &b);
	}

	promptp = &prompt[ucs4_strlen(prompt)];

	expandp(orig, promptp, NPMT-(promptp-prompt));
	prompt[NPMT-1] = '\0';
	promptp += ucs4_strlen(promptp);

	b = utf8_to_ucs4_cpystr("\" with \"");
	if(b){
	    ucs4_strncpy(promptp, b, NPMT-(promptp-prompt));
	    promptp += ucs4_strlen(promptp);
	    prompt[NPMT-1] = '\0';
	    fs_give((void **) &b);
	}

	expandp(repl, promptp, NPMT-(promptp-prompt));
	prompt[NPMT-1] = '\0';
	promptp += ucs4_strlen(promptp);

	if((promptp-prompt) < NPMT-1){
	    *promptp++ = '\"';
	    *promptp = '\0';
	}

	prompt[NPMT-1] = '\0';

	status = mlyesno(prompt, TRUE);		/* ask user */

	if (status == TRUE){
	    n++;
	    chword(realpat, repl);		     /* replace word    */
	    update();
	}else{
	    chword(realpat, realpat);	       /* replace word by itself */
	    update();
	    if(status == ABORT){		/* if cancelled return */
		eml.s = comatose(n);
		emlwrite("Replace All cancelled after %s changes", &eml);
		return (ABORT);			/* ... else keep looking */
	    }
	}
    }
    else{
	char *utf8;

	utf8 = ucs4_to_utf8_cpystr(orig);
	if(utf8){
	  eml.s = utf8;
	  emlwrite(_("No more matches for \"%s\""), &eml);
	  fs_give((void **) &utf8);
	}
	else
	  emlwrite(_("No more matches"), NULL);

	return (FALSE);
    }
}


/* Read a replacement pattern.  Modeled after readpattern(). */
int
srpat(char *utf8prompt, UCS *defpat, size_t defpatlen, int repl_mode)
{
	register int s;
	int	     i = 0;
	UCS         *b;
	UCS	     prompt[NPMT];
	UCS         *promptp;
	EXTRAKEYS    menu_pat[8];

	menu_pat[i = 0].name = "^Y";
	menu_pat[i].label    = N_("FirstLine");
	menu_pat[i].key	     = (CTRL|'Y');
	KS_OSDATASET(&menu_pat[i], KS_NONE);

	menu_pat[++i].name = "^V";
	menu_pat[i].label  = N_("LastLine");
	menu_pat[i].key	   = (CTRL|'V');
	KS_OSDATASET(&menu_pat[i], KS_NONE);

	menu_pat[++i].name = "^R";
	menu_pat[i].label  = repl_mode ? N_("No Replace") : N_("Replace");
	menu_pat[i].key	   = (CTRL|'R');
	KS_OSDATASET(&menu_pat[i], KS_NONE);

	if(!repl_mode){
	    menu_pat[++i].name = "^T";
	    menu_pat[i].label  = N_("LineNumber");
	    menu_pat[i].key    = (CTRL|'T');
	    KS_OSDATASET(&menu_pat[i], KS_NONE);

	    menu_pat[++i].name = "^W";
	    /* TRANSLATORS: Start of paragraph */
	    menu_pat[i].label  = N_("Start of Para");
	    menu_pat[i].key    = (CTRL|'W');
	    KS_OSDATASET(&menu_pat[i], KS_NONE);

	    menu_pat[++i].name = "^O";
	    menu_pat[i].label  = N_("End of Para");
	    menu_pat[i].key    = (CTRL|'O');
	    KS_OSDATASET(&menu_pat[i], KS_NONE);

	    menu_pat[++i].name = "^U";
	    /* TRANSLATORS: Instead of justifying (formatting) just a
	       single paragraph, Full Justify justifies the entire
	       message. */
	    menu_pat[i].label  = N_("FullJustify");
	    menu_pat[i].key    = (CTRL|'U');
	    KS_OSDATASET(&menu_pat[i], KS_NONE);
	}

	menu_pat[++i].name = NULL;

	b = utf8_to_ucs4_cpystr(utf8prompt);
	if(b){
	    ucs4_strncpy(prompt, b, NPMT);
	    prompt[NPMT-1] = '\0';
	    fs_give((void **) &b);
	}

	promptp = &prompt[ucs4_strlen(prompt)];

	if(repl_mode){
	    b = utf8_to_ucs4_cpystr(" (to replace)");
	    if(b){
		ucs4_strncpy(promptp, b, NPMT-(promptp-prompt));
		promptp += ucs4_strlen(promptp);
		prompt[NPMT-1] = '\0';
		fs_give((void **) &b);
	    }
	}

        if(pat[0] != '\0'){
	    if((promptp-prompt) < NPMT-2){
		*promptp++ = ' ';
		*promptp++ = '[';
		*promptp = '\0';
	    }

	    expandp(pat, promptp, NPMT-(promptp-prompt));
	    prompt[NPMT-1] = '\0';
	    promptp += ucs4_strlen(promptp);

	    if((promptp-prompt) < NPMT-1){
		*promptp++ = ']';
		*promptp = '\0';
	    }
	}

	if((promptp-prompt) < NPMT-2){
	    *promptp++ = ':';
	    *promptp++ = ' ';
	    *promptp = '\0';
	}

	prompt[NPMT-1] = '\0';

	s = mlreplyd(prompt, defpat, defpatlen, QDEFLT, menu_pat);

	if (s == TRUE || s == FALSE){	/* changed or not, they're done */
	    if(!defpat[0]){		/* use default */
		ucs4_strncpy(defpat, pat, defpatlen);
		defpat[defpatlen-1] = '\0';
	    }
	    else if(ucs4_strcmp(pat, defpat)){   	      /* Specified */
		ucs4_strncpy(pat, defpat, NPAT);
		pat[NPAT-1] = '\0';
		rpat[0] = '\0';
	    }

	    s = TRUE;			/* let caller know to proceed */
	}

	return(s);
}


/*
 * Read a pattern. Stash it in the external variable "pat". The "pat" is not
 * updated if the user types in an empty line. If the user typed an empty line,
 * and there is no old pattern, it is an error. Display the old pattern, in the
 * style of Jeff Lomicka. There is some do-it-yourself control expansion.
 * change to using <ESC> to delemit the end-of-pattern to allow <NL>s in
 * the search string.
 */

int
readnumpat(char *utf8prompt)
{
    int		 i, n;
    char	 numpat[NPMT];
    EXTRAKEYS    menu_pat[2];

    menu_pat[i = 0].name  = "^T";
    menu_pat[i].label	  = N_("No Line Number");
    menu_pat[i].key	  = (CTRL|'T');
    KS_OSDATASET(&menu_pat[i++], KS_NONE);

    menu_pat[i].name  = NULL;

    numpat[0] = '\0';
    while(1)
      switch(mlreplyd_utf8(utf8prompt, numpat, NPMT, QNORML, menu_pat)){
	case TRUE :
	  if(*numpat){
	      for(i = n = 0; numpat[i]; i++)
		if(strchr("0123456789", numpat[i])){
		    n = (n * 10) + (numpat[i] - '0');
		}
		else
		  return(-2);

	      return(n);
	  }

	case FALSE :
	default :
	  return(-1);

	case (CTRL|'T') :
	  return(-3);

	case (CTRL|'L') :
	case HELPCH :
	  break;
      }
}	    


int
readpattern(char *utf8prompt, int text_mode)
{
	register int s;
	int	     i;
	UCS         *b;
	UCS	     tpat[NPAT+20];
	UCS         *tpatp;
	EXTRAKEYS    menu_pat[7];

	menu_pat[i = 0].name = "^Y";
	menu_pat[i].label    = N_("FirstLine");
	menu_pat[i].key	     = (CTRL|'Y');
	KS_OSDATASET(&menu_pat[i], KS_NONE);

	menu_pat[++i].name = "^V";
	menu_pat[i].label  = N_("LastLine");
	menu_pat[i].key	   = (CTRL|'V');
	KS_OSDATASET(&menu_pat[i], KS_NONE);

	if(text_mode){
	    menu_pat[++i].name = "^T";
	    menu_pat[i].label  = N_("LineNumber");
	    menu_pat[i].key    = (CTRL|'T');
	    KS_OSDATASET(&menu_pat[i], KS_NONE);

	    menu_pat[++i].name = "^W";
	    menu_pat[i].label  = N_("Start of Para");
	    menu_pat[i].key    = (CTRL|'W');
	    KS_OSDATASET(&menu_pat[i], KS_NONE);

	    menu_pat[++i].name = "^O";
	    menu_pat[i].label  = N_("End of Para");
	    menu_pat[i].key    = (CTRL|'O');
	    KS_OSDATASET(&menu_pat[i], KS_NONE);

	    menu_pat[++i].name = "^U";
	    menu_pat[i].label  = N_("FullJustify");
	    menu_pat[i].key    = (CTRL|'U');
	    KS_OSDATASET(&menu_pat[i], KS_NONE);
	}

	menu_pat[++i].name = NULL;

	b = utf8_to_ucs4_cpystr(utf8prompt);
	if(b){
	    ucs4_strncpy(tpat, b, NPAT+20);
	    tpat[NPAT+20-1] = '\0';
	    fs_give((void **) &b);
	}

	tpatp = &tpat[ucs4_strlen(tpat)];

        if(pat[0] != '\0'){
	    if((tpatp-tpat) < NPAT+20-2){
		*tpatp++ = ' ';
		*tpatp++ = '[';
		*tpatp = '\0';
	    }

	    expandp(pat, tpatp, NPAT+20-(tpatp-tpat));
	    tpat[NPAT+20-1] = '\0';
	    tpatp += ucs4_strlen(tpatp);

	    if((tpatp-tpat) < NPAT+20-1){
		*tpatp++ = ']';
		*tpatp = '\0';
	    }
	}

	if((tpatp-tpat) < NPAT+20-3){
	    *tpatp++ = ' ';
	    *tpatp++ = ':';
	    *tpatp++ = ' ';
	    *tpatp = '\0';
	}

	tpat[NPAT+20-1] = '\0';

	s = mlreplyd(tpat, tpat, NPAT, QNORML, menu_pat);

	if ((s == TRUE) && ucs4_strcmp(pat,tpat)){			/* Specified */
	  ucs4_strncpy(pat, tpat, NPAT);
	  pat[NPAT-1] = '\0';
	  rpat[0] = '\0';
	}
	else if (s == FALSE && pat[0] != '\0')	/* CR, but old one */
		s = TRUE;

	return(s);
}


/* search forward for a <patrn>	*/
int
forscan(int *wrapt,	/* boolean indicating search wrapped */
	UCS *patrn,	/* string to scan for */
	LINE *limitp,	/* stop searching if reached */
	int limito,	/* stop searching if reached */
	int leavep)	/* place to leave point
				PTBEG = begining of match
				PTEND = at end of match		*/

{
    LINE *curline;	/* current line during scan */
    int curoff;		/* position within current line */
    LINE *lastline;	/* last line position during scan */
    int lastoff;	/* position within last line */
    UCS c;		/* character at current position */
    LINE *matchline;	/* current line during matching */
    int matchoff;	/* position in matching line */
    UCS *patptr;	/* pointer into pattern */
    int stopoff;	/* offset to stop search */
    LINE *stopline;	/* line to stop search */

    *wrapt = FALSE;

    /*
     * the idea is to set the character to end the search at the 
     * next character in the buffer.  thus, let the search wrap
     * completely around the buffer.
     * 
     * first, test to see if we are at the end of the line, 
     * otherwise start searching on the next character. 
     */
    if(curwp->w_doto == llength(curwp->w_dotp)){
	/*
	 * dot is not on end of a line
	 * start at 0 offset of the next line
	 */
	stopoff = curoff  = 0;
	stopline = curline = lforw(curwp->w_dotp);
	if (curwp->w_dotp == curbp->b_linep)
	  *wrapt = TRUE;
    }
    else{
	stopoff = curoff  = curwp->w_doto;
	stopline = curline = curwp->w_dotp;
    }

    /* scan each character until we hit the head link record */

    /*
     * maybe wrapping is a good idea
     */
    while (curline){

	if (curline == curbp->b_linep)
	  *wrapt = TRUE;

	/* save the current position in case we need to
	   restore it on a match			*/

	lastline = curline;
	lastoff = curoff;

	/* get the current character resolving EOLs */
	if (curoff == llength(curline)) {	/* if at EOL */
	    curline = lforw(curline);	/* skip to next line */
	    curoff = 0;
	    c = '\n';			/* and return a <NL> */
	}
	else
	  c = lgetc(curline, curoff++).c;	/* get the char */

	/* test it against first char in pattern */
	if (eq(c, patrn[0]) != FALSE) {	/* if we find it..*/
	    /* setup match pointers */
	    matchline = curline;
	    matchoff = curoff;
	    patptr = &patrn[0];

	    /* scan through patrn for a match */
	    while (*++patptr != '\0') {
		/* advance all the pointers */
		if (matchoff == llength(matchline)) {
		    /* advance past EOL */
		    matchline = lforw(matchline);
		    matchoff = 0;
		    c = '\n';
		} else
		  c = lgetc(matchline, matchoff++).c;

		if(matchline == limitp && matchoff == limito)
		  return(FALSE);

		/* and test it against the pattern */
		if (eq(*patptr, c) == FALSE)
		  goto fail;
	    }

	    /* A SUCCESSFULL MATCH!!! */
	    /* reset the global "." pointers */
	    if (leavep == PTEND) {	/* at end of string */
		curwp->w_dotp = matchline;
		curwp->w_doto = matchoff;
	    }
	    else {		/* at begining of string */
		curwp->w_dotp = lastline;
		curwp->w_doto = lastoff;
	    }

	    curwp->w_flag |= WFMOVE; /* flag that we have moved */
	    return(TRUE);

	}

fail:;			/* continue to search */
	if(((curline == stopline) && (curoff == stopoff))
	   || (curline == limitp && curoff == limito))
	  break;			/* searched everywhere... */
    }
    /* we could not find a match */

    return(FALSE);
}



/* 	expandp:	expand control key sequences for output		*/
int
expandp(UCS *srcstr,		/* string to expand */
	UCS *deststr,		/* destination of expanded string */
	int maxlength)		/* maximum chars in destination */
{
	UCS c;		/* current char to translate */

	/* scan through the string */
	while ((c = *srcstr++) != 0) {
		if (c == '\n') {		/* its an EOL */
			*deststr++ = '<';
			*deststr++ = 'N';
			*deststr++ = 'L';
			*deststr++ = '>';
			maxlength -= 4;
		} else if (c < 0x20 || c == 0x7f) {	/* control character */
			*deststr++ = '^';
			*deststr++ = c ^ 0x40;
			maxlength -= 2;
		} else if (c == '%') {
			*deststr++ = '%';
			*deststr++ = '%';
			maxlength -= 2;
		} else {			/* any other character */
			*deststr++ = c;
			maxlength--;
		}

		/* check for maxlength */
		if (maxlength < 4) {
			*deststr++ = '$';
			*deststr = '\0';
			return(FALSE);
		}
	}

	*deststr = '\0';
	return(TRUE);
}


/* 
 * chword() - change the given word, wp, pointed to by the curwp->w_dot 
 *            pointers to the word in cb
 */
void
chword(UCS *wb, UCS *cb)
{
    ldelete((long) ucs4_strlen(wb), NULL);		/* not saved in kill buffer */
    while(*cb != '\0')
      linsert(1, *cb++);

    curwp->w_flag |= WFEDIT;
}
