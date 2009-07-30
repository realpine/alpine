#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: word.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
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

/*
 * Program:	Word at a time routines
 *
 * The routines in this file implement commands that work word at a time.
 * There are all sorts of word mode commands. If I do any sentence and/or
 * paragraph mode commands, they are likely to be put in this file.
 */

#include	"headers.h"


int fpnewline(UCS *quote);
int fillregion(UCS *qstr, REGION *addedregion);
int setquotelevelinregion(int quotelevel, REGION *addedregion);
int is_user_separator(UCS c);


/* Word wrap on n-spaces. Back-over whatever precedes the point on the current
 * line and stop on the first word-break or the beginning of the line. If we
 * reach the beginning of the line, jump back to the end of the word and start
 * a new line.  Otherwise, break the line at the word-break, eat it, and jump
 * back to the end of the word.
 * Returns TRUE on success, FALSE on errors.
 */
int
wrapword(void)
{
    register int cnt;			/* size of word wrapped to next line */
    register int bp;			/* index to wrap on */
    register int first = -1;
    int wid, ww;

    if(curwp->w_doto <= 0)		/* no line to wrap? */
      return(FALSE);

    wid = 0;
    for(bp = cnt = 0; cnt < llength(curwp->w_dotp) && !bp; cnt++){
	if(ucs4_isspace(lgetc(curwp->w_dotp, cnt).c)){
	    first = 0;
	    if(lgetc(curwp->w_dotp, cnt).c == TAB){
	      ++wid;
	      while(wid & 0x07)
		++wid;
	    }
	    else
	      ++wid;
	}
	else{
	    ww = wcellwidth((UCS) lgetc(curwp->w_dotp, cnt).c);
	    wid += (ww >= 0 ? ww : 1);
	    if(!first)
	      first = cnt;
	}

	if(first > 0 && wid > fillcol)
	  bp = first;
    }

    if(!bp)
      return(FALSE);

    /* bp now points to the first character of the next line */
    cnt = curwp->w_doto - bp;
    curwp->w_doto = bp;

    if(!lnewline())			/* break the line */
      return(FALSE);

    /*
     * if there's a line below, it doesn't start with whitespace 
     * and there's room for this line...
     */
    if(!(curbp->b_flag & BFWRAPOPEN)
       && lforw(curwp->w_dotp) != curbp->b_linep 
       && llength(lforw(curwp->w_dotp)) 
       && !ucs4_isspace(lgetc(lforw(curwp->w_dotp), 0).c)
       && (llength(curwp->w_dotp) + llength(lforw(curwp->w_dotp)) < fillcol)){
	gotoeol(0, 1);			/* then pull text up from below */
	if(lgetc(curwp->w_dotp, curwp->w_doto - 1).c != ' ')
	  linsert(1, ' ');

	forwdel(0, 1);
	gotobol(0, 1);
    }

    curbp->b_flag &= ~BFWRAPOPEN;	/* don't open new line next wrap */
					/* restore dot (account for NL)  */
    if(cnt && !forwchar(0, cnt < 0 ? cnt-1 : cnt))
      return(FALSE);

    return(TRUE);
}


/*
 * Move the cursor backward by "n" words. All of the details of motion are
 * performed by the "backchar" and "forwchar" routines. Error if you try to
 * move beyond the buffers.
 */
int
backword(int f, int n)
{
        if (n < 0)
                return (forwword(f, -n));
        if (backchar_no_header_editor(FALSE, 1) == FALSE)
                return (FALSE);
        while (n--) {
                while (inword() == FALSE) {
                        if (backchar_no_header_editor(FALSE, 1) == FALSE)
                                return (FALSE);
                }
                while (inword() != FALSE) {
                        if (backchar_no_header_editor(FALSE, 1) == FALSE)
                                return (FALSE);
                }
        }
        return (forwchar(FALSE, 1));
}

/*
 * Move the cursor forward by the specified number of words. All of the motion
 * is done by "forwchar". Error if you try and move beyond the buffer's end.
 */
int
forwword(int f, int n)
{
        if (n < 0)
                return (backword(f, -n));
        while (n--) {
#if	NFWORD
                while (inword() != FALSE) {
                        if (forwchar(FALSE, 1) == FALSE)
                                return (FALSE);
                }
#endif
                while (inword() == FALSE) {
                        if (forwchar(FALSE, 1) == FALSE)
                                return (FALSE);
                }
#if	NFWORD == 0
                while (inword() != FALSE) {
                        if (forwchar(FALSE, 1) == FALSE)
                                return (FALSE);
                }
#endif
        }
	return(TRUE);
}

int
ucs4_isalnum(UCS c)
{
    return((c && c <= 0x7f && isalnum((unsigned char) c))
	   || (c >= 0xA0 && !SPECIAL_SPACE(c)));
}

int
ucs4_isalpha(UCS c)
{
    return((c && c <= 0x7f && isalpha((unsigned char) c))
	   || (c >= 0xA0 && !SPECIAL_SPACE(c)));
}

int
ucs4_isspace(UCS c)
{
    return((c < 0xff && isspace((unsigned char) c)) || SPECIAL_SPACE(c));
}

int
ucs4_ispunct(UCS c)
{
     return !ucs4_isalnum(c) && !ucs4_isspace(c);
}

#ifdef	MAYBELATER
/*
 * Move the cursor forward by the specified number of words. As you move,
 * convert any characters to upper case. Error if you try and move beyond the
 * end of the buffer. Bound to "M-U".
 */
int
upperword(int f, int n)
{
        register int    c;
	CELL            ac;

	ac.a = 0;
	if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
		return(rdonly());	/* we are in read only mode	*/
        if (n < 0)
                return (FALSE);
        while (n--) {
                while (inword() == FALSE) {
                        if (forwchar(FALSE, 1) == FALSE)
                                return (FALSE);
                }
                while (inword() != FALSE) {
                        c = lgetc(curwp->w_dotp, curwp->w_doto).c;
                        if (c>='a' && c<='z') {
                                ac.c = (c -= 'a'-'A');
                                lputc(curwp->w_dotp, curwp->w_doto, ac);
                                lchange(WFHARD);
                        }
                        if (forwchar(FALSE, 1) == FALSE)
                                return (FALSE);
                }
        }
        return (TRUE);
}

/*
 * Move the cursor forward by the specified number of words. As you move
 * convert characters to lower case. Error if you try and move over the end of
 * the buffer. Bound to "M-L".
 */
int
lowerword(int f, int n)
{
        register int    c;
	CELL            ac;

	ac.a = 0;
	if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
		return(rdonly());	/* we are in read only mode	*/
        if (n < 0)
                return (FALSE);
        while (n--) {
                while (inword() == FALSE) {
                        if (forwchar(FALSE, 1) == FALSE)
                                return (FALSE);
                }
                while (inword() != FALSE) {
                        c = lgetc(curwp->w_dotp, curwp->w_doto).c;
                        if (c>='A' && c<='Z') {
                                ac.c (c += 'a'-'A');
                                lputc(curwp->w_dotp, curwp->w_doto, ac);
                                lchange(WFHARD);
                        }
                        if (forwchar(FALSE, 1) == FALSE)
                                return (FALSE);
                }
        }
        return (TRUE);
}

/*
 * Move the cursor forward by the specified number of words. As you move
 * convert the first character of the word to upper case, and subsequent
 * characters to lower case. Error if you try and move past the end of the
 * buffer. Bound to "M-C".
 */
int
capword(int f, int n)
{
        register int    c;
	CELL	        ac;

	ac.a = 0;
	if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
		return(rdonly());	/* we are in read only mode	*/
        if (n < 0)
                return (FALSE);
        while (n--) {
                while (inword() == FALSE) {
                        if (forwchar(FALSE, 1) == FALSE)
                                return (FALSE);
                }
                if (inword() != FALSE) {
                        c = lgetc(curwp->w_dotp, curwp->w_doto).c;
                        if (c>='a' && c<='z') {
			    ac.c = (c -= 'a'-'A');
			    lputc(curwp->w_dotp, curwp->w_doto, ac);
			    lchange(WFHARD);
                        }
                        if (forwchar(FALSE, 1) == FALSE)
                                return (FALSE);
                        while (inword() != FALSE) {
                                c = lgetc(curwp->w_dotp, curwp->w_doto).c;
                                if (c>='A' && c<='Z') {
				    ac.c = (c += 'a'-'A');
				    lputc(curwp->w_dotp, curwp->w_doto, ac);
				    lchange(WFHARD);
                                }
                                if (forwchar(FALSE, 1) == FALSE)
                                        return (FALSE);
                        }
                }
        }
        return (TRUE);
}

/*
 * Kill forward by "n" words. Remember the location of dot. Move forward by
 * the right number of words. Put dot back where it was and issue the kill
 * command for the right number of characters. Bound to "M-D".
 */
int
delfword(int f, int n)
{
        register long   size;
        register LINE   *dotp;
        register int    doto;

	if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
		return(rdonly());	/* we are in read only mode	*/
        if (n < 0)
                return (FALSE);
        dotp = curwp->w_dotp;
        doto = curwp->w_doto;
        size = 0L;
        while (n--) {
#if	NFWORD
		while (inword() != FALSE) {
			if (forwchar(FALSE,1) == FALSE)
				return(FALSE);
			++size;
		}
#endif
                while (inword() == FALSE) {
                        if (forwchar(FALSE, 1) == FALSE)
                                return (FALSE);
                        ++size;
                }
#if	NFWORD == 0
                while (inword() != FALSE) {
                        if (forwchar(FALSE, 1) == FALSE)
                                return (FALSE);
                        ++size;
                }
#endif
        }
        curwp->w_dotp = dotp;
        curwp->w_doto = doto;
        return (ldelete(size, kinsert));
}

/*
 * Kill backwards by "n" words. Move backwards by the desired number of words,
 * counting the characters. When dot is finally moved to its resting place,
 * fire off the kill command. Bound to "M-Rubout" and to "M-Backspace".
 */
int
delbword(int f, int n)
{
        register long   size;

	if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
		return(rdonly());	/* we are in read only mode	*/
        if (n < 0)
                return (FALSE);
        if (backchar(FALSE, 1) == FALSE)
                return (FALSE);
        size = 0L;
        while (n--) {
                while (inword() == FALSE) {
                        if (backchar(FALSE, 1) == FALSE)
                                return (FALSE);
                        ++size;
                }
                while (inword() != FALSE) {
                        if (backchar(FALSE, 1) == FALSE)
                                return (FALSE);
                        ++size;
                }
        }
        if (forwchar(FALSE, 1) == FALSE)
                return (FALSE);
        return (ldelete(size, kinsert));
}
#endif	/* MAYBELATER */

/*
 * Return TRUE if the character at dot is a character that is considered to be
 * part of a word.
 */
int
inword(void)
{
     if(curwp->w_doto < llength(curwp->w_dotp))
     {
         if(ucs4_isalnum(lgetc(curwp->w_dotp, curwp->w_doto).c))
         {
             return(TRUE);
         }
         else if(ucs4_ispunct(lgetc(curwp->w_dotp, curwp->w_doto).c)
	         && !is_user_separator(lgetc(curwp->w_dotp, curwp->w_doto).c))
         {
             if((curwp->w_doto > 0) &&
                 ucs4_isalnum(lgetc(curwp->w_dotp, curwp->w_doto - 1).c) &&
                 (curwp->w_doto + 1 < llength(curwp->w_dotp)) &&
                 ucs4_isalnum(lgetc(curwp->w_dotp, curwp->w_doto + 1).c))
             {
                 return(TRUE);
             }
         }
     }

     return(FALSE);
}


int
is_user_separator(UCS c)
{
    UCS *u;

    if(glo_wordseps)
      for(u = glo_wordseps; *u; u++)
	if(*u == c)
	  return 1;

    return 0;
}


/*
 * Return number of quotes if whatever starts the line matches the quote string
 */
int
quote_match(UCS *q, LINE *l, UCS *buf, size_t buflen)
{
    register int i, n, j, qb;

    *buf = '\0';
    if(*q == '\0')
      return(1);

    qb = (ucs4_strlen(q) > 1 && q[ucs4_strlen(q)-1] == ' ') ? 1 : 0;
    for(n = 0, j = 0; ;){
	for(i = 0; j <= llength(l) && qb ? q[i+1] : q[i]; i++, j++)
	  if(q[i] != lgetc(l, j).c)
	    return(n);

	n++;
	if((!qb && q[i] == '\0') || (qb && q[i+1] == '\0')){
	    if(ucs4_strlen(buf) + ucs4_strlen(q) + 1 < buflen){
		ucs4_strncat(buf, q, buflen-ucs4_strlen(q)-1);
		buf[buflen-1] = '\0';
		if(qb && (j > llength(l) || lgetc(l, j).c != ' '))
		  buf[ucs4_strlen(buf)-1] = '\0';
	    }
	}
	if(j > llength(l))
	  return(n);
	else if(qb && lgetc(l, j).c == ' ')
	  j++;
    }
    return(n);  /* never reached */
}


/* Justify the entire buffer instead of just a paragraph */
int
fillbuf(int f, int n)
{
    LINE *eobline;
    REGION region;

    if(curbp->b_mode&MDVIEW){		/* don't allow this command if	*/
	return(rdonly());		/* we are in read only mode	*/
    }
    else if (fillcol == 0) {		/* no fill column set */
	mlwrite_utf8("No fill column set", NULL);
	return(FALSE);
    }

    if((lastflag & CFFILL) && (lastflag & CFFLBF)){
	/* no use doing a full justify twice */
	thisflag |= (CFFLBF | CFFILL);
	return(TRUE);
    }

    /* record the pointer of the last line */
    if(gotoeob(FALSE, 1) == FALSE)
      return(FALSE);

    eobline = curwp->w_dotp;		/* last line of buffer */
    if(!llength(eobline))
      eobline = lback(eobline);

    /* and back to the beginning of the buffer */
    gotobob(FALSE, 1);

    thisflag |= CFFLBF; /* CFFILL also gets set in fillpara */

    if(!Pmaster)
      sgarbk = TRUE;
    
    curwp->w_flag |= WFMODE;

    /*
     * clear the kill buffer, that's where we'll store undo
     * information, we can't do the fill buffer because
     * fillpara relies on its contents
     */
    kdelete();
    curwp->w_doto = 0;
    getregion(&region, eobline, llength(eobline));

    /* Put full message in the kill buffer for undo */
    if(!ldelete(region.r_size, kinsert))
      return(FALSE);

    /* before yank'ing, clear lastflag so we don't just unjustify */
    lastflag &= ~(CFFLBF | CFFILL);

    /* Now in kill buffer, bring back text to use in fillpara */
    yank(FALSE, 1);

    gotobob(FALSE, 1);

    /* call fillpara until we're at the end of the buffer */
    while(curwp->w_dotp != curbp->b_linep)
      if(!(fillpara(FALSE, 1)))
	return(FALSE);
    
    return(TRUE);
}


/*
 * Fill the current paragraph according to the current fill column
 */
int
fillpara(int f, int n)
{
    UCS    *qstr, qstr2[NSTRING], c;
    int     quotelevel = -1;
    REGION  addedregion;
    char    action = 'P';

    if(curbp->b_mode&MDVIEW){		/* don't allow this command if	*/
	return(rdonly());		/* we are in read only mode	*/
    }
    else if (fillcol == 0) {		/* no fill column set */
	mlwrite_utf8("No fill column set", NULL);
	return(FALSE);
    }
    else if(curwp->w_dotp == curbp->b_linep && !curwp->w_markp) /* don't wrap! */
      return(FALSE);

    /*
     * If there is already a region set, then we may use it
     * instead of the current paragraph.
     */

    if(curwp->w_markp){
	int k, rv;
	KEYMENU menu_justify[12];
	char prompt[100];

	for(k = 0; k < 12; k++){
	    menu_justify[k].name = NULL;
	    KS_OSDATASET(&menu_justify[k], KS_NONE);
	}

	menu_justify[1].name  = "R";
	menu_justify[1].label = "[" N_("Region") "]";
	menu_justify[6].name  = "^C";
	menu_justify[6].label = N_("Cancel");
	menu_justify[7].name  = "P";
	menu_justify[7].label = N_("Paragraph");
	menu_justify[2].name  = "Q";
	menu_justify[2].label = N_("Quotelevel");

	wkeyhelp(menu_justify);		/* paint menu */
	sgarbk = TRUE;
	if(Pmaster && curwp)
	  curwp->w_flag |= WFMODE;

	strncpy(prompt, "justify Region, Paragraph; or fix Quotelevel ? ", sizeof(prompt));
	prompt[sizeof(prompt)-1] = '\0';
	mlwrite_utf8(prompt, NULL);
	(*term.t_rev)(1);
	rv = -1;
	while(1){
	    switch(c = GetKey()){

	      case (CTRL|'C') :		/* Bail out! */
	      case F2         :
		pputs_utf8(_("ABORT"), 1);
		rv = ABORT;
		emlwrite("", NULL);
		break;

	      case (CTRL|'M') :		/* default */
	      case 'r' :
	      case 'R' :
	      case F3  :
		pputs_utf8(_("Region"), 1);
		rv = 'R';
		break;

	      case 'p' :
	      case 'P' :
	      case F7  :
		pputs_utf8(_("Paragraph"), 1);
		rv = 'P';
		break;

	      case 'q' :
	      case 'Q' :
	      case F8  :
	      case '0' : case '1' : case '2' : case '3' : case '4' :
	      case '5' : case '6' : case '7' : case '8' : case '9' :
		pputs_utf8(_("Quotelevel"), 1);
		while(rv == -1){
		  switch(c){
		    case 'q' :
		    case 'Q' :
		    case F8  :
		     {char num[20];

		      num[0] = '\0';
		      switch(mlreplyd_utf8("Quote Level ? ", num, sizeof(num), QNORML, NULL)){
		        case TRUE:
			  if(isdigit(num[0])){
			      quotelevel = atoi(num);
			      if(quotelevel < 0){
				  emlwrite("Quote Level cannot be negative", NULL);
				  sleep(3);
			      }
			      else if(quotelevel > 20){
				  emlwrite("Quote Level should be less than 20", NULL);
				  rv = ABORT;
			      }
			      else{
				  rv = 'Q';
			      }
			  }
			  else if(num[0]){
			      emlwrite("Quote Level should be a number", NULL);
			      sleep(3);
			  }

			  break;

		        case HELPCH:
			  emlwrite("Enter the number of quotes you want before the text", NULL);
			  sleep(3);
			  break;

		        default:
			  emlwrite("Quote Level is a number", NULL);
			  rv = ABORT;
			  break;
		      }
		     }

		      break;

		    case '0' : case '1' : case '2' : case '3' : case '4' :
		    case '5' : case '6' : case '7' : case '8' : case '9' :
		      rv = 'Q';
		      quotelevel = (int) (c - '0');
		      break;
		  }
		}

		break;

	      case (CTRL|'G') :
		if(term.t_mrow == 0 && km_popped == 0){
		    movecursor(term.t_nrow-2, 0);
		    peeol();
		    term.t_mrow = 2;
		    (*term.t_rev)(0);
		    wkeyhelp(menu_justify);
		    mlwrite_utf8(prompt, NULL);
		    (*term.t_rev)(1);
		    sgarbk = TRUE;			/* mark menu dirty */
		    km_popped++;
		    break;
		}
		/* else fall through */

	      default:
		(*term.t_beep)();

	      case NODATA :
		break;
	    }

	    (*term.t_flush)();
	    if(rv != -1){
		(*term.t_rev)(0);
		if(km_popped){
		    term.t_mrow = 0;
		    movecursor(term.t_nrow, 0);
		    peeol();
		    sgarbf = 1;
		    km_popped = 0;
		}

		action = rv;
		break;
	    }
	}

	if(action != ABORT)
	  emlwrite("", NULL);
    }

    if(action == 'R' && curwp->w_markp){
	/* let yank() know that it may be restoring a paragraph */
	thisflag |= CFFILL;

	if(!Pmaster)
	  sgarbk = TRUE;
	
	curwp->w_flag |= WFMODE;

	swap_mark_and_dot_if_mark_comes_first();

	/* determine if we're justifying quoted text or not */
	qstr = (glo_quote_str
		&& quote_match(glo_quote_str, 
			       curwp->w_doto > 0 ? curwp->w_dotp->l_fp : curwp->w_dotp,
			       qstr2, NSTRING)
		&& *qstr2) ? qstr2 : NULL;


	/*
	 * Fillregion moves dot to the end of the filled region.
	 */
	if(!fillregion(qstr, &addedregion))
	  return(FALSE);

	set_last_region_added(&addedregion);
    }
    else if(action == 'P'){

	/*
	 * Justfiy the current paragraph.
	 */

	if(curwp->w_markp)		/* clear mark if already set */
	  setmark(0,0);

	if(gotoeop(FALSE, 1) == FALSE)
	  return(FALSE);

	/* determine if we're justifying quoted text or not */
	qstr = (glo_quote_str
		&& quote_match(glo_quote_str, 
			       curwp->w_dotp, qstr2, NSTRING)
		&& *qstr2) ? qstr2 : NULL;

	setmark(0,0);			/* mark last line of para */

	/* jump back to the beginning of the paragraph */
	gotobop(FALSE, 1);

	/* let yank() know that it may be restoring a paragraph */
	thisflag |= (CFFILL | CFFLPA);

	if(!Pmaster)
	  sgarbk = TRUE;
	
	curwp->w_flag |= WFMODE;

	curwp->w_doto = 0;		/* start region at beginning of line */

	/*
	 * Fillregion moves dot to the end of the filled region.
	 */
	if(!fillregion(qstr, &addedregion))
	  return(FALSE);

	set_last_region_added(&addedregion);

	/* Leave cursor on first char of first line after justified region */
	curwp->w_dotp = lforw(curwp->w_dotp);
	curwp->w_doto = 0;

	if(curwp->w_markp)
	  setmark(0,0);			/* clear mark */
    }
    else if(action == 'Q'){
	/* let yank() know that it may be restoring a paragraph */
	thisflag |= CFFILL;

	if(!Pmaster)
	  sgarbk = TRUE;
	
	curwp->w_flag |= WFHARD;

	swap_mark_and_dot_if_mark_comes_first();

	if(!setquotelevelinregion(quotelevel, &addedregion))
	  return(FALSE);

	set_last_region_added(&addedregion);
    }
    else{
	/* abort */
    }

    return(TRUE);
}


/*
 * The region we're filling is the region from dot to mark.
 * We cut out that region and then put it back in filled.
 * The cut out part is saved in the ldelete call and the
 * reinstalled region is noted in addedregion, so that yank()
 * can delete it and restore the saved part.
 */
int
fillregion(UCS *qstr, REGION *addedregion)
{
    long    c, sz, last_char = 0;
    int	    i, j, qlen, same_word,
	    spaces, word_len, word_ind, line_len, ww;
    int     starts_midline = 0;
    int     ends_midline = 0;
    int     offset_into_start;
    LINE   *line_before_start, *lp;
    UCS     line_last, word[NSTRING];
    REGION  region;

    /* if region starts midline insert a newline */
    if(curwp->w_doto > 0 && curwp->w_doto < llength(curwp->w_dotp))
      starts_midline++;

    /* if region ends midline insert a newline at end */
    if(curwp->w_marko > 0 && curwp->w_marko < llength(curwp->w_markp))
      ends_midline++;

    /* cut the paragraph into our fill buffer */
    fdelete();
    if(!getregion(&region, curwp->w_markp, curwp->w_marko))
      return(FALSE);

    if(!ldelete(region.r_size, finsert))
      return(FALSE);

    line_before_start = lback(curwp->w_dotp);
    offset_into_start = curwp->w_doto;

    if(starts_midline)
      lnewline();

    /* Now insert it back wrapped */
    spaces = word_len = word_ind = line_len = same_word = 0;
    qlen = qstr ? ucs4_strlen(qstr) : 0;

    /* Beginning with leading quoting... */
    if(qstr){
	i = 0;
	while(qstr[i]){
	  ww = wcellwidth(qstr[i]);
	  line_len += (ww >= 0 ? ww : 1);
	  linsert(1, qstr[i++]);
	}

	line_last = ' ';			/* no word-flush space! */
    }

    /* remove first leading quotes if any */
    if(starts_midline)
      i = 0;
    else
      for(i = qlen; (c = fremove(i)) == ' ' || c == TAB; i++){
	  linsert(1, line_last = (UCS) c);
	  line_len += ((c == TAB) ? (~line_len & 0x07) + 1 : 1);
      }

    /* then digest the rest... */
    while((c = fremove(i++)) >= 0){
	last_char = c;
	switch(c){
	  case '\n' :
	    /* skip next quote string */
	    j = 0;
	    while(j < qlen && ((c = fremove(i+j)) == qstr[j] || c == ' '))
	      j++;

	    i += j;


	    if(!spaces)
	      spaces++;
	    same_word = 0;
	    break;

	  case TAB :
	  case ' ' :
	    spaces++;
	    same_word = 0;
	    break;

	  default :
	    if(spaces){				/* flush word? */
		if((line_len - qlen > 0)
		   && line_len + word_len + 1 > fillcol
		   && ((ucs4_isspace(line_last))
		       || (linsert(1, ' ')))
		   && (line_len = fpnewline(qstr)))
		  line_last = ' ';	/* no word-flush space! */

		if(word_len){			/* word to write? */
		    if(line_len && !ucs4_isspace(line_last)){
			linsert(1, ' ');	/* need padding? */
			line_len++;
		    }

		    line_len += word_len;
		    for(j = 0; j < word_ind; j++)
		      linsert(1, line_last = word[j]);

		    if(spaces > 1 && strchr(".?!:;\")", line_last)){
			linsert(2, line_last = ' ');
			line_len += 2;
		    }

		    word_len = word_ind = 0;
		}

		spaces = 0;
	    }

	    if(word_ind + 1 >= NSTRING){
		/* Magic!  Fake that we output a wrapped word */
		if((line_len - qlen > 0) && !same_word++){
		    if(!ucs4_isspace(line_last))
		      linsert(1, ' ');
		    line_len = fpnewline(qstr);
		}

		line_len += word_len;
		for(j = 0; j < word_ind; j++)
		  linsert(1, word[j]);

		word_len = word_ind = 0;
		line_last = ' ';
	    }

	    word[word_ind++] = (UCS) c;
	    ww = wcellwidth((UCS) c);
	    word_len += (ww >= 0 ? ww : 1);

	    break;
	}
    }

    if(word_len){
	if((line_len - qlen > 0) && (line_len + word_len + 1 > fillcol)){
	    if(!ucs4_isspace(line_last))
	      linsert(1, ' ');
	    (void) fpnewline(qstr);
	}
	else if(line_len && !ucs4_isspace(line_last))
	  linsert(1, ' ');

	for(j = 0; j < word_ind; j++)
	  linsert(1, word[j]);
    }

    if(last_char == '\n')
      lnewline();

    if(ends_midline)
      (void) fpnewline(qstr);

    /*
     * Calculate the size of the region that was added.
     */
    swapmark(0,1);	/* mark current location after adds */
    addedregion->r_linep = lforw(line_before_start);
    addedregion->r_offset = offset_into_start;
    lp = addedregion->r_linep;
    sz = llength(lp) - addedregion->r_offset;
    if(lforw(lp) != curwp->w_markp->l_fp){
	lp = lforw(lp);
	while(lp != curwp->w_markp->l_fp){
	    sz += llength(lp) + 1;
	    lp = lforw(lp);
	}
    }

    sz -= llength(curwp->w_markp) - curwp->w_marko;
    addedregion->r_size = sz;

    swapmark(0,1);

    if(ends_midline){
	/*
	 * We want to back up to the end of the original
	 * region instead of being here after the added newline.
	 */
	curwp->w_doto = 0;
	backchar(0, 1);
	unmarkbuffer();
	markregion(1);
    }

    return(TRUE);
}


/*
 * fpnewline - output a fill paragraph newline mindful of quote string
 */
int
fpnewline(UCS *quote)
{
    int len;

    lnewline();
    for(len = 0; quote && *quote; quote++){
	int ww;

	ww = wcellwidth(*quote);
	len += (ww >= 0 ? ww : 1);
	linsert(1, *quote);
    }

    return(len);
}


int
setquotelevelinregion(int quotelevel, REGION *addedregion)
{
    int     i, standards_based = 0;
    int     quote_chars = 0, backuptoprevline = 0;
    int     starts_midline = 0, ends_midline = 0, offset_into_start;
    long    c, sz;
    UCS     qstr_def1[] = { '>', ' ', 0}, qstr_def2[] = { '>', 0};
    LINE   *lp, *line_before_start;
    REGION  region;

    if(curbp->b_mode&MDVIEW)		/* don't allow this command if	*/
      return(rdonly());			/* we are in read only mode	*/

    if(!glo_quote_str
       || !ucs4_strcmp(glo_quote_str, qstr_def1)
       || !ucs4_strcmp(glo_quote_str, qstr_def2))
      standards_based++;

    if(!standards_based){
	emlwrite("Quote level setting only works with standard \"> \" quotes", NULL);
	return(FALSE);
    }

    /* if region starts midline insert a newline */
    if(curwp->w_doto > 0 && curwp->w_doto < llength(curwp->w_dotp))
      starts_midline++;

    /* if region ends midline insert a newline at end */
    if(curwp->w_marko > 0 && curwp->w_marko < llength(curwp->w_markp)){
	ends_midline++;
	backuptoprevline++;
	/* count quote chars for re-insertion */
	for(i = 0; i < llength(curwp->w_markp); ++i)
	  if(lgetc(curwp->w_markp, i).c != '>')
	    break;

	quote_chars = i;
    }
    else if(curwp->w_marko == 0)
      backuptoprevline++;

    /* find the size of the region */
    getregion(&region, curwp->w_markp, curwp->w_marko);

    /* cut the paragraph into our fill buffer */
    fdelete();
    if(!ldelete(region.r_size, finsert))
      return(FALSE);

    line_before_start = lback(curwp->w_dotp);
    offset_into_start = curwp->w_doto;

    /* if region starts midline add a newline */
    if(starts_midline)
      lnewline();

    i = 0;
    while(fremove(i) >= 0){

	/* remove all quote strs from current line */
	if(standards_based){
	    while((c = fremove(i)) == '>')
	      i++;

	    if(c == ' ')
	      i++;
	}
	else{
	}

	/* insert quotelevel quote strs */
	if(standards_based){
            linsert(quotelevel, '>');
	    if(quotelevel > 0)
              linsert(1, ' ');
	}
	else{
	}

	/* put back the actual line */
	while((c = fremove(i++)) >= 0 && c != '\n')
	  linsert(1, (UCS) c);

	if(c == '\n')
	  lnewline();
    }

    /* if region ends midline add a newline */
    if(ends_midline){
	lnewline();
	if(quote_chars){
	    linsert(quote_chars, '>');
	    if(curwp->w_doto < llength(curwp->w_dotp)
	       && lgetc(curwp->w_dotp, curwp->w_doto).c != ' ')
	      linsert(1, ' ');
	}
    }

    /*
     * Calculate the size of the region that was added.
     */
    swapmark(0,1);	/* mark current location after adds */
    addedregion->r_linep = lforw(line_before_start);
    addedregion->r_offset = offset_into_start;
    lp = addedregion->r_linep;
    sz = llength(lp) - addedregion->r_offset;
    if(lforw(lp) != curwp->w_markp->l_fp){
	lp = lforw(lp);
	while(lp != curwp->w_markp->l_fp){
	    sz += llength(lp) + 1;
	    lp = lforw(lp);
	}
    }

    sz -= llength(curwp->w_markp) - curwp->w_marko;
    addedregion->r_size = sz;

    swapmark(0,1);

    /*
     * This puts us at the end of the quoted region instead
     * of on the following line. This makes it convenient
     * for the user to follow a quotelevel adjustment with
     * a Justify if desired.
     */
    if(backuptoprevline){
	curwp->w_doto = 0;
	backchar(0, 1);
    }

    if(ends_midline){	/* doesn't need fixing otherwise */
	unmarkbuffer();
	markregion(1);
    }

    return (TRUE);
}
