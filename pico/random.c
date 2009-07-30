#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: random.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
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
 * Program:	Random routines
 *
 * This file contains the command processing functions for a number of random
 * commands. There is no functional grouping here, for sure.
 */

#include	"headers.h"

#include "osdep/terminal.h"

int	worthit(int *);

int     tabsize;                        /* Tab size (0: use real tabs)  */


/*
 * Display the current position of the cursor, in origin 1 X-Y coordinates,
 * the character that is under the cursor (in octal), and the fraction of the
 * text that is before the cursor. The displayed column is not the current
 * column, but the column that would be used on an infinite width display.
 * Normally this is bound to "C-X =".
 */
int
showcpos(int f, int n)
{
    register LINE   *clp;
    register long   nch;
    register int    cbo;
    register long   nbc;
    register int    lines;
    register int    thisline;
    char     buffer[80];

    clp = lforw(curbp->b_linep);            /* Grovel the data.     */
    cbo = 0;
    nch = 0L;
    lines = 0;
    for (;;) {
	if (clp==curwp->w_dotp && cbo==curwp->w_doto) {
	    thisline = lines;
	    nbc = nch;
	}
	if (cbo == llength(clp)) {
	    if (clp == curbp->b_linep)
	      break;
	    clp = lforw(clp);
	    cbo = 0;
	    lines++;
	} else
	  ++cbo;
	++nch;
    }

    snprintf(buffer,sizeof(buffer),"line %d of %d (%d%%%%), character %ld of %ld (%d%%%%)",
	    thisline+1, lines+1, (int)((100L*(thisline+1))/(lines+1)),
	    nbc, nch, (nch) ? (int)((100L*nbc)/nch) : 0);

    emlwrite(buffer, NULL);
    return (TRUE);
}


/*
 * Return current column.  Stop at first non-blank given TRUE argument.
 */
int
getccol(int bflg)
{
    UCS c;
    int i, col;

    col = 0;
    for (i=0; i<curwp->w_doto; ++i) {
	c = lgetc(curwp->w_dotp, i).c;
	if (c!=' ' && c!='\t' && bflg)
	  break;

	if (c == '\t'){
	    col |= 0x07;
	    ++col;
	}
	else if (ISCONTROL(c)){
	    col += 2;
	}
	else{
	    int ww;

	    ww = wcellwidth(c);
	    col += (ww >= 0 ? ww : 1);
	}
    }

    return(col);
}



/*
 * Set tab size if given non-default argument (n <> 1).  Otherwise, insert a
 * tab into file.  If given argument, n, of zero, change to true tabs.
 * If n > 1, simulate tab stop every n-characters using spaces. This has to be
 * done in this slightly funny way because the tab (in ASCII) has been turned
 * into "C-I" (in 10 bit code) already. Bound to "C-I".
 */
int
tab(int f, int n)
{
    if (n < 0)
      return (FALSE);

    if (n == 0 || n > 1) {
	tabsize = n;
	return(TRUE);
    }

    if (! tabsize)
      return(linsert(1, '\t'));

    return(linsert(tabsize - (getccol(FALSE) % tabsize), ' '));
}


/*
 * Insert a newline. Bound to "C-M".
 */
int
newline(int f, int n)
{
    register int    s;

    if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
      return(rdonly());	/* we are in read only mode	*/

    if (n < 0)
      return (FALSE);

    if(TERM_OPTIMIZE && (curwp->w_dotp != curwp->w_bufp->b_linep)){
	int l;

	if(worthit(&l)){
	    if(curwp->w_doto != 0)
	      l++;
	    scrolldown(curwp, l, n);
	}
    }

    /* if we are in C mode and this is a default <NL> */
    /* pico's never in C mode */

    if(Pmaster && Pmaster->allow_flowed_text && curwp->w_doto
       && ucs4_isspace(lgetc(curwp->w_dotp, curwp->w_doto - 1).c)
       && !(curwp->w_doto == 3
	    && lgetc(curwp->w_dotp, 0).c == '-'
	    && lgetc(curwp->w_dotp, 1).c == '-'
	    && lgetc(curwp->w_dotp, 2).c == ' ')){
	/*
	 * flowed mode, make the newline a hard one by
	 * stripping trailing space.
	 */
	int i, dellen;
	for(i = curwp->w_doto - 1;
	    i && ucs4_isspace(lgetc(curwp->w_dotp, i - 1).c);
	    i--);
	dellen = curwp->w_doto - i;
	curwp->w_doto = i;
	ldelete(dellen, NULL);
    }
    /* insert some lines */
    while (n--) {
	if ((s=lnewline()) != TRUE)
	  return (s);
    }
    return (TRUE);
}



/*
 * Delete forward. This is real easy, because the basic delete routine does
 * all of the work. Watches for negative arguments, and does the right thing.
 * If any argument is present, it kills rather than deletes, to prevent loss
 * of text if typed with a big argument. Normally bound to "C-D".
 */
int
forwdel(int f, int n)
{
    if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
      return(rdonly());	/* we are in read only mode	*/

    if (n < 0)
      return (backdel(f, -n));

    if(TERM_OPTIMIZE && (curwp->w_dotp != curwp->w_bufp->b_linep)){
	int l;

	if(worthit(&l) && curwp->w_doto == llength(curwp->w_dotp))
	  scrollup(curwp, l+1, 1);
    }

    if (f != FALSE) {                       /* Really a kill.       */
	if ((lastflag&CFKILL) == 0)
	  kdelete();
	thisflag |= CFKILL;
    }

    return (ldelete((long) n, f ? kinsert : NULL));
}



/*
 * Delete backwards. This is quite easy too, because it's all done with other
 * functions. Just move the cursor back, and delete forwards. Like delete
 * forward, this actually does a kill if presented with an argument. Bound to
 * both "RUBOUT" and "C-H".
 */
int
backdel(int f, int n)
{
    register int    s;

    if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
      return(rdonly());	/* we are in read only mode	*/

    if (n < 0)
      return (forwdel(f, -n));

    if(TERM_OPTIMIZE && curwp->w_dotp != curwp->w_bufp->b_linep){
	int l;
	
	if(worthit(&l) && curwp->w_doto == 0 &&
	   lback(curwp->w_dotp) != curwp->w_bufp->b_linep){
	    if(l == curwp->w_toprow)
	      scrollup(curwp, l+1, 1);
	    else if(llength(lback(curwp->w_dotp)) == 0)
	      scrollup(curwp, l-1, 1);
	    else
	      scrollup(curwp, l, 1);
	}
    }

    if (f != FALSE) {                       /* Really a kill.       */
	if ((lastflag&CFKILL) == 0)
	  kdelete();

	thisflag |= CFKILL;
    }

    if ((s=backchar(f, n)) == TRUE)
      s = ldelete((long) n, f ? kinsert : NULL);

    return (s);
}



/*
 * killtext - delete the line that the cursor is currently in.
 *	      a greatly pared down version of its former self.
 */
int
killtext(int f, int n)
{
    register int chunk;
    int		 opt_scroll = 0;

    if (curbp->b_mode&MDVIEW)		/* don't allow this command if  */
      return(rdonly());			/* we are in read only mode     */

    if ((lastflag&CFKILL) == 0)		/* Clear kill buffer if */
      kdelete();			/* last wasn't a kill.  */

    if(gmode & MDDTKILL){		/*  */
	if((chunk = llength(curwp->w_dotp) - curwp->w_doto) == 0){
	    chunk = 1;
	    if(TERM_OPTIMIZE)
	      opt_scroll = 1;
	}
    }
    else{
	gotobol(FALSE, 1);		/* wack from bol past newline */
	chunk = llength(curwp->w_dotp) + 1;
	if(TERM_OPTIMIZE)
	  opt_scroll = 1;
    }

    /* optimize what motion we can */
    if(opt_scroll && (curwp->w_dotp != curwp->w_bufp->b_linep)){
	int l;

	if(worthit(&l))
	  scrollup(curwp, l, 1);
    }

    thisflag |= CFKILL;
    return(ldelete((long) chunk, kinsert));
}


/*
 * Yank text back from the kill buffer. This is really easy. All of the work
 * is done by the standard insert routines. All you do is run the loop, and
 * check for errors. Bound to "C-Y".
 */
int
yank(int f, int n)
{
    int    c, i;
    REGION region, *added_region;
    LINE  *dotp;

    if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
      return(rdonly());	/* we are in read only mode	*/

    if (n < 0)
      return (FALSE);

    if(TERM_OPTIMIZE && (curwp->w_dotp != curwp->w_bufp->b_linep)){
	int l;

	if(worthit(&l) && !(lastflag&CFFILL)){
	    register int  t = 0; 
	    register int  i = 0;
	    register int  ch;

	    while((ch=fremove(i++)) >= 0)
	      if(ch == '\n')
		t++;
	    if(t+l < curwp->w_toprow+curwp->w_ntrows)
	      scrolldown(curwp, l, t);
	}
    }

    if(lastflag & CFFILL){		/* if last command was fillpara() */
	if(lastflag & CFFLBF){
	    gotoeob(FALSE, 1);
	    dotp = curwp->w_dotp;
	    gotobob(FALSE, 1);
	    curwp->w_doto = 0;
	    getregion(&region, dotp, llength(dotp));
	}
	else{
	    added_region = get_last_region_added();
	    if(added_region){
		curwp->w_dotp = added_region->r_linep;
		curwp->w_doto = added_region->r_offset;
		region = (*added_region);
	    }
	    else
	      return(FALSE);
	}

	if(!ldelete(region.r_size, NULL))
	  return(FALSE);
    }					/* then splat out the saved buffer */

    while (n--) {
	i = 0;
	while ((c = ((lastflag&CFFILL)
		     ? ((lastflag & CFFLBF) ? kremove(i) : fremove(i))
		     : kremove(i))) >= 0) {
	    if (c == '\n') {
		if (lnewline() == FALSE)
		  return (FALSE);
	    } else {
		if (linsert(1, c) == FALSE)
		  return (FALSE);
	    }

	    ++i;
	}
    }

    if(lastflag&CFFLPA){            /* if last command was fill paragraph */
	curwp->w_dotp = lforw(curwp->w_dotp);
	curwp->w_doto = 0;

	curwp->w_flag |= WFMODE;
	
	if(!Pmaster){
	    sgarbk = TRUE;
	    emlwrite("", NULL);
	}
    }

    return (TRUE);
}



/*
 * worthit - generic sort of test to roughly gage usefulness of using 
 *           optimized scrolling.
 *
 * note:
 *	returns the line on the screen, l, that the dot is currently on
 */
int
worthit(int *l)
{
    int i;			/* l is current line */
    unsigned below;		/* below is avg # of ch/line under . */

    *l = doton(&i, &below);
    below = (i > 0) ? below/(unsigned)i : 0;

    return(below > 3);
}
