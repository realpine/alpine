#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: display.c 1025 2008-04-08 22:59:38Z hubert@u.washington.edu $";
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
 *
 * Program:	Display functions
 *
 */

/*
 * The functions in this file handle redisplay. There are two halves, the
 * ones that update the virtual display screen, and the ones that make the
 * physical display screen the same as the virtual display screen. These
 * functions use hints that are left in the windows by the commands.
 *
 */

#include	"../c-client/mail.h"
#include	"../c-client/utf8.h"

#ifdef _WINDOWS
/* wingdi.h uses ERROR (!) and we aren't using the c-client ERROR so... */
#undef ERROR
#endif

#include	"headers.h"
#include	"../pith/charconv/filesys.h"
#include	"../pith/charconv/utf8.h"


void     vtmove(int, int);
void     vtputc(CELL);
void     vteeol(void);
void     updateline(int, CELL *, CELL *, short *);
void     updext(void);
void     mlputi(int, int);
void     pprints(int, int);
void     mlputli(long, int);
void     showCompTitle(void);
int      nlforw(void);
int      dumbroot(int, int);
int      dumblroot(long, int);
unsigned cellwidth_ptr_to_ptr(CELL *pstart, CELL *pend);
unsigned vcellwidth_a_to_b(int row, int a, int b);
#ifdef _WINDOWS
void    pico_config_menu_items (KEYMENU *);
int     update_scroll (void);
#endif /* _WINDOWS */


/*
 * Standard pico keymenus...
 */
static KEYMENU menu_pico[] = {
    {"^G", N_("Get Help"), KS_SCREENHELP},	{"^O", N_("WriteOut"), KS_SAVEFILE},
    {"^R", N_("Read File"), KS_READFILE},	{"^Y", N_("Prev Pg"), KS_PREVPAGE},
    {"^K", N_("Cut Text"), KS_NONE},	{"^C", N_("Cur Pos"), KS_CURPOSITION},
    {"^X", N_("Exit"), KS_EXIT},		{"^J", N_("Justify"), KS_JUSTIFY},
    {"^W", N_("Where is"), KS_WHEREIS},	{"^V", N_("Next Pg"), KS_NEXTPAGE},
    {"^U", NULL, KS_NONE},
#ifdef	SPELLER
    {"^T", N_("To Spell"), KS_SPELLCHK}
#else
    {"^D", N_("Del Char"), KS_NONE}
#endif
};
#define	UNCUT_KEY	10


static KEYMENU menu_compose[] = {
    {"^G", N_("Get Help"), KS_SCREENHELP},	{"^X", NULL, KS_SEND},
    {"^R", N_("Read File"), KS_READFILE},	{"^Y", N_("Prev Pg"), KS_PREVPAGE},
    {"^K", N_("Cut Text"), KS_NONE},	{"^O", N_("Postpone"), KS_POSTPONE},
    /* TRANSLATORS: Justify is to reformat a paragraph automatically */
    {"^C", N_("Cancel"), KS_CANCEL},	{"^J", N_("Justify"), KS_JUSTIFY},
    {NULL, NULL, KS_NONE},		{"^V", N_("Next Pg"), KS_NEXTPAGE},
    {"^U", NULL, KS_NONE},
#ifdef	SPELLER
    {"^T", N_("To Spell"), KS_SPELLCHK}
#else
    {"^D", N_("Del Char"), KS_NONE}
#endif
};
#define	EXIT_KEY	1
#define	PSTPN_KEY	5
#define	WHERE_KEY	8


/*
 * Definition's for pico's modeline
 */
#define	PICO_TITLE	"  UW PICO %s"
#define	PICO_MOD_MSG	"Modified"
#define	PICO_NEWBUF_MSG	"New Buffer"

#define WFDEBUG 0                       /* Window flag debug. */

#define VFCHG   0x0001                  /* Changed flag			*/
#define	VFEXT	0x0002			/* extended (beyond column 80)	*/
#define	VFREV	0x0004			/* reverse video status		*/
#define	VFREQ	0x0008			/* reverse video request	*/

int     vtrow   = 0;                    /* Row location of SW cursor */
int     vtcol   = 0;                    /* Column location of SW cursor */
int     vtind   = 0;                    /* Index into row array of SW cursor */
int     ttrow   = FARAWAY;              /* Row location of HW cursor */
int     ttcol   = FARAWAY;              /* Column location of HW cursor */
int	lbound	= 0;			/* leftmost column of current line
					   being displayed */

VIDEO   **vscreen;                      /* Virtual screen. */
VIDEO   **pscreen;                      /* Physical screen. */


/*
 * Initialize the data structures used by the display code. The edge vectors
 * used to access the screens are set up. The operating system's terminal I/O
 * channel is set up. All the other things get initialized at compile time.
 * The original window has "WFCHG" set, so that it will get completely
 * redrawn on the first call to "update".
 */
int
vtinit(void)
{
    int i, j;
    VIDEO *vp;
    CELL   ac;

    ac.c = ' ';
    ac.a = 0;

    if(Pmaster == NULL)
      vtterminalinfo(gmode & MDTCAPWINS);

    (*term.t_open)();

    (*term.t_rev)(FALSE);
    vscreen = (VIDEO **) malloc((term.t_nrow+1)*sizeof(VIDEO *));
    memset(vscreen, 0, (term.t_nrow+1)*sizeof(VIDEO *));
    if (vscreen == NULL){
	emlwrite("Allocating memory for virtual display failed.", NULL);
        return(FALSE);
    }

    pscreen = (VIDEO **) malloc((term.t_nrow+1)*sizeof(VIDEO *));
    memset(pscreen, 0, (term.t_nrow+1)*sizeof(VIDEO *));
    if (pscreen == NULL){
	free((void *)vscreen);
	emlwrite("Allocating memory for physical display failed.", NULL);
        return(FALSE);
    }


    for (i = 0; i <= term.t_nrow; ++i) {
        vp = (VIDEO *) malloc(sizeof(VIDEO)+(term.t_ncol*sizeof(CELL)));

        if (vp == NULL){
	    free((void *)vscreen);
	    free((void *)pscreen);
	    emlwrite("Allocating memory for virtual display lines failed.",
		     NULL);
            return(FALSE);
	}
	else
	  for(j = 0; j < term.t_ncol; j++)
	    vp->v_text[j] = ac;

	vp->v_flag = 0;
        vscreen[i] = vp;

        vp = (VIDEO *) malloc(sizeof(VIDEO)+(term.t_ncol*sizeof(CELL)));

        if (vp == NULL){
            free((void *)vscreen[i]);
	    while(--i >= 0){
		free((void *)vscreen[i]);
		free((void *)pscreen[i]);
	    }

	    free((void *)vscreen);
	    free((void *)pscreen);
	    emlwrite("Allocating memory for physical display lines failed.",
		     NULL);
            return(FALSE);
	}
	else
	  for(j = 0; j < term.t_ncol; j++)
	    vp->v_text[j] = ac;

	vp->v_flag = 0;
        pscreen[i] = vp;
    }

    return(TRUE);
}

int
vtterminalinfo(int termcap_wins)
{
    return((term.t_terminalinfo) ? (*term.t_terminalinfo)(termcap_wins)
				 : (Pmaster ? 0 : TRUE));
}


/*
 * Clean up the virtual terminal system, in anticipation for a return to the
 * operating system. Move down to the last line and clear it out (the next
 * system prompt will be written in the line). Shut down the channel to the
 * terminal.
 */
void
vttidy(void)
{
    movecursor(term.t_nrow-1, 0);
    peeol();
    movecursor(term.t_nrow, 0);
    peeol();
    (*term.t_close)();
}


/*
 * Set the virtual cursor to the specified row and column on the virtual
 * screen. There is no checking for nonsense values; this might be a good
 * idea during the early stages.
 */
void
vtmove(int row, int col)
{
    vtrow = row;
    vtcol = col;

    if(vtcol < 0)
      vtind = -1;
    else if(vtcol == 0)
      vtind = vtcol;
    else{
	/*
	 * This is unused so don't worry about it.
	 */
	assert(0);
    }
}


/*
 * Write a character to the virtual screen. The virtual row and column are
 * updated. If the line is too long put a "$" in the last column. This routine
 * only puts printing characters into the virtual terminal buffers. Only
 * column overflow is checked.
 */
void
vtputc(CELL c)
{
    VIDEO   *vp;
    CELL     ac;
    int      w;

    vp = vscreen[vtrow];
    ac.c = ' ';
    ac.a = c.a;

    if (vtcol >= term.t_ncol) {
	/*
	 * What's this supposed to be doing? This sets vtcol
	 * to the even multiple of 8 >= vtcol. Why are we doing that?
	 * Must make tab work correctly.
	 *               24 -> 24
	 *               25 -> 32
	 *               ...
	 *               31 -> 32
	 *               32 -> 32
	 *               33 -> 40
	 */
        vtcol = (vtcol + 0x07) & ~0x07;
	ac.c = '$';

	/*
	 * If we get to here that means that there must be characters
	 * past the right hand edge, so we want to put a $ character
	 * in the last visible character. It would be nice to replace
	 * the last visible character by a double-$ if it is double-width
	 * but we aren't doing that because we'd have to add up the widths
	 * starting at the left hand margin each time through.
	 */
	if(vtind > 0 && vtind <= term.t_ncol)
          vp->v_text[vtind-1] = ac;
    }
    else if (c.c == '\t') {
        do {
            vtputc(ac);
	}
        while (((vtcol + (vtrow==currow ? lbound : 0)) & 0x07) != 0 && vtcol < term.t_ncol);
    }
    else if (ISCONTROL(c.c)){
	ac.c = '^';
        vtputc(ac);
	ac.c = ((c.c & 0x7f) | 0x40);
        vtputc(ac);
    }
    else{

	/*
	 * Have to worry about what happens if we skip over 0
	 * with a double-width character. There may be a better
	 * place to be setting vtind, or maybe we could make do
	 * without it.
	 */

	w = wcellwidth((UCS) c.c);
	w = (w >= 0 ? w : 1);

	if(vtcol == 0 || (vtcol < 0 && vtcol + w == 1)){
	    vtind = 0;
	    if(vtcol < 0)
	      vtcol = 0;
	}

	/*
	 * Double-width character overlaps right edge.
	 * Replace it with a $.
	 */
	if(vtcol + w > term.t_ncol){
	    ac.c = '$';
	    c = ac;
	    w = 1;
	}

	if(vtind >= 0 && vtind < term.t_ncol)
	  vp->v_text[vtind++] = c;

	vtcol += w;
    }
}


/*
 * Erase from the end of the software cursor to the end of the line on which
 * the software cursor is located.
 */
void
vteeol(void)
{
    register VIDEO      *vp;
    CELL     c;

    c.c = ' ';
    c.a = 0;
    vp = vscreen[vtrow];

    if(vtind >= 0)
      while (vtind < term.t_ncol)
        vp->v_text[vtind++] = c;

    vtcol = term.t_ncol;
}


/*
 * Make sure that the display is right. This is a three part process. First,
 * scan through all of the windows looking for dirty ones. Check the framing,
 * and refresh the screen. Second, make sure that "currow" and "curcol" are
 * correct for the current window. Third, make the virtual and physical
 * screens the same.
 */
void
update(void)
{
    LINE   *lp;
    WINDOW *wp;
    VIDEO  *vp1;
    VIDEO  *vp2;
    int     i;
    int     j;
    int     scroll = 0;
    CELL	     c;

#if	TYPEAH
    if (typahead())
	return;
#endif

#ifdef _WINDOWS
    /* This tells our MS Windows module to not bother updating the
     * cursor position while a massive screen update is in progress.
     */
    mswin_beginupdate ();
#endif

/*
 * BUG: setting and unsetting whole region at a time is dumb.  fix this.
 */
    if(curwp->w_markp){
	unmarkbuffer();
	markregion(1);
    }

    wp = wheadp;

    while (wp != NULL){
        /* Look at any window with update flags set on. */

        if (wp->w_flag != 0){
            /* If not force reframe, check the framing. */

            if ((wp->w_flag & WFFORCE) == 0){
                lp = wp->w_linep;

                for (i = 0; i < wp->w_ntrows; ++i){
                    if (lp == wp->w_dotp)
		      goto out;

                    if (lp == wp->w_bufp->b_linep)
		      break;

                    lp = lforw(lp);
		}
	    }

            /* Not acceptable, better compute a new value for the line at the
             * top of the window. Then set the "WFHARD" flag to force full
             * redraw.
             */
            i = wp->w_force;

            if (i > 0){
                --i;

                if (i >= wp->w_ntrows)
                  i = wp->w_ntrows-1;
	    }
            else if (i < 0){
                i += wp->w_ntrows;

                if (i < 0)
		  i = 0;
	    }
            else if(TERM_OPTIMIZE){
		/* 
		 * find dotp, if its been moved just above or below the 
		 * window, use scrollxxx() to facilitate quick redisplay...
		 */
		lp = lforw(wp->w_dotp);
		if(lp != wp->w_dotp){
		    if(lp == wp->w_linep && lp != wp->w_bufp->b_linep){
			scroll = 1;
		    }
		    else {
			lp = wp->w_linep;
			for(j=0;j < wp->w_ntrows; ++j){
			    if(lp != wp->w_bufp->b_linep)
			      lp = lforw(lp);
			    else
			      break;
			}
			if(lp == wp->w_dotp && j == wp->w_ntrows)
			  scroll = 2;
		    }
		}
		j = i = wp->w_ntrows/2;
	    }
	    else
	      i = wp->w_ntrows/2;

            lp = wp->w_dotp;

            while (i != 0 && lback(lp) != wp->w_bufp->b_linep){
                --i;
                lp = lback(lp);
	    }

	    /*
	     * this is supposed to speed things up by using tcap sequences
	     * to efficiently scroll the terminal screen.  the thinking here
	     * is that its much faster to update pscreen[] than to actually
	     * write the stuff to the screen...
	     */
	    if(TERM_OPTIMIZE){
		switch(scroll){
		  case 1:			/* scroll text down */
		    j = j-i+1;			/* add one for dot line */
			/* 
			 * do we scroll down the header as well?  Well, only 
			 * if we're not editing the header, we've backed up 
			 * to the top, and the composer is not being 
			 * displayed...
			 */
		    if(Pmaster && Pmaster->headents && !ComposerEditing 
		       && (lback(lp) == wp->w_bufp->b_linep)
		       && (ComposerTopLine == COMPOSER_TOP_LINE))
		      j += entry_line(1000, TRUE); /* Never > 1000 headers */

		    scrolldown(wp, -1, j);
		    break;
		  case 2:			/* scroll text up */
		    j = wp->w_ntrows - (j-i);	/* we chose new top line! */
		    if(Pmaster && j){
			/* 
			 * do we scroll down the header as well?  Well, only 
			 * if we're not editing the header, we've backed up 
			 * to the top, and the composer is not being 
			 * displayed...
			 */
			if(!ComposerEditing 
			   && (ComposerTopLine != COMPOSER_TOP_LINE))
			  scrollup(wp, COMPOSER_TOP_LINE, 
				   j+entry_line(1000, TRUE));
			else
			  scrollup(wp, -1, j);
		    }
		    else
		      scrollup(wp, -1, j);
		    break;
		    default :
		      break;
		}
	    }

            wp->w_linep = lp;
            wp->w_flag |= WFHARD;       /* Force full. */
out:
	    /*
	     * if the line at the top of the page is the top line
	     * in the body, show the header...
	     */
	    if(Pmaster && Pmaster->headents && !ComposerEditing){
		if(lback(wp->w_linep) == wp->w_bufp->b_linep){
		    if(ComposerTopLine == COMPOSER_TOP_LINE){
			i = term.t_nrow - 2 - term.t_mrow - HeaderLen();
			if(i > 0 && nlforw() >= i) {	/* room for header ? */
			    if((i = nlforw()/2) == 0 && term.t_nrow&1)
			      i = 1;
			    while(wp->w_linep != wp->w_bufp->b_linep && i--)
			      wp->w_linep = lforw(wp->w_linep);
			    
			}
			else
			  ToggleHeader(1);
		    }
		}
		else{
		    if(ComposerTopLine != COMPOSER_TOP_LINE)
		      ToggleHeader(0);		/* hide it ! */
		}
	    }

            /* Try to use reduced update. Mode line update has its own special
             * flag. The fast update is used if the only thing to do is within
             * the line editing.
             */
            lp = wp->w_linep;
            i = wp->w_toprow;

            if ((wp->w_flag & ~WFMODE) == WFEDIT){
                while (lp != wp->w_dotp){
                    ++i;
                    lp = lforw(lp);
		}

                vscreen[i]->v_flag |= VFCHG;
                vtmove(i, 0);

                for (j = 0; j < llength(lp); ++j)
                    vtputc(lgetc(lp, j));

                vteeol();
	    }
	    else if ((wp->w_flag & (WFEDIT | WFHARD)) != 0){
                while (i < wp->w_toprow+wp->w_ntrows){
                    vscreen[i]->v_flag |= VFCHG;
                    vtmove(i, 0);

		    /* if line has been changed */
                    if (lp != wp->w_bufp->b_linep){
                        for (j = 0; j < llength(lp); ++j)
                            vtputc(lgetc(lp, j));

                        lp = lforw(lp);
		    }

                    vteeol();
                    ++i;
		}
	    }
#if ~WFDEBUG
            if ((wp->w_flag&WFMODE) != 0)
                modeline(wp);

            wp->w_flag  = 0;
            wp->w_force = 0;
#endif
	}
#if WFDEBUG
        modeline(wp);
        wp->w_flag =  0;
        wp->w_force = 0;
#endif

	/* and onward to the next window */
        wp = wp->w_wndp;
    }

    /* Always recompute the row and column number of the hardware cursor. This
     * is the only update for simple moves.
     */
    lp = curwp->w_linep;
    currow = curwp->w_toprow;

    while (lp != curwp->w_dotp){
        ++currow;
        lp = lforw(lp);
    }

    curcol = 0;
    i = 0;

    while (i < curwp->w_doto){
	c = lgetc(lp, i++);

        if(c.c == '\t'){
            curcol |= 0x07;
	    ++curcol;
	}
        else if(ISCONTROL(c.c)){
            curcol += 2;
	}
	else{
	    int w;

	    w = wcellwidth((UCS) c.c);
	    curcol += (w >= 0 ? w : 1);
	}
    }

    if (curcol >= term.t_ncol) { 		/* extended line. */
	/* flag we are extended and changed */
	vscreen[currow]->v_flag |= VFEXT | VFCHG;
	updext();				/* and output extended line */
    } else
      lbound = 0;				/* not extended line */

    /* make sure no lines need to be de-extended because the cursor is
     * no longer on them 
     */

    wp = wheadp;

    while (wp != NULL) {
	lp = wp->w_linep;
	i = wp->w_toprow;

	while (i < wp->w_toprow + wp->w_ntrows) {
	    if (vscreen[i]->v_flag & VFEXT) {
		/* always flag extended lines as changed */
		vscreen[i]->v_flag |= VFCHG;
		if ((wp != curwp) || (lp != wp->w_dotp) ||
		    (curcol < term.t_ncol)) {
		    vtmove(i, 0);
		    for (j = 0; j < llength(lp); ++j)
		      vtputc(lgetc(lp, j));
		    vteeol();

		    /* this line no longer is extended */
		    vscreen[i]->v_flag &= ~VFEXT;
		}
	    }
	    lp = lforw(lp);
	    ++i;
	}
	/* and onward to the next window */
        wp = wp->w_wndp;
    }

    /* Special hacking if the screen is garbage. Clear the hardware screen,
     * and update your copy to agree with it. Set all the virtual screen
     * change bits, to force a full update.
     */

    if (sgarbf != FALSE){
	if(Pmaster){
	    int rv;
       
	    showCompTitle();

	    if(ComposerTopLine != COMPOSER_TOP_LINE){
		UpdateHeader(0);		/* arrange things */
		PaintHeader(COMPOSER_TOP_LINE, TRUE);
	    }

	    /*
	     * since we're using only a portion of the screen and only 
	     * one buffer, only clear enough screen for the current window
	     * which is to say the *only* window.
	     */
	    for(i=wheadp->w_toprow;i<=term.t_nrow; i++){
		movecursor(i, 0);
		peeol();
		vscreen[i]->v_flag |= VFCHG;
	    }
	    rv = (*Pmaster->showmsg)('X' & 0x1f);	/* ctrl-L */
	    ttresize();
	    picosigs();		/* restore altered handlers */
	    if(rv)		/* Did showmsg corrupt the display? */
	      PaintBody(0);	/* Yes, repaint */
	    movecursor(wheadp->w_toprow, 0);
	}
	else{
	    for (i = 0; i < term.t_nrow-term.t_mrow; ++i){
		vscreen[i]->v_flag |= VFCHG;
		vp1 = pscreen[i];
		c.c = ' ';
		c.a = 0;
		for (j = 0; j < term.t_ncol; ++j)
		  vp1->v_text[j] = c;
	    }

	    movecursor(0, 0);	               /* Erase the screen. */
	    (*term.t_eeop)();

	}

        sgarbf = FALSE;				/* Erase-page clears */
        mpresf = FALSE;				/* the message area. */

	if(Pmaster)
	  modeline(curwp);
	else
	  sgarbk = TRUE;			/* fix the keyhelp as well...*/
    }

    /* Make sure that the physical and virtual displays agree. Unlike before,
     * the "updateline" code is only called with a line that has been updated
     * for sure.
     */
    if(Pmaster)
      i = curwp->w_toprow;
    else
      i = 0;

    if (term.t_nrow > term.t_mrow)
       c.c = term.t_nrow - term.t_mrow;
    else
       c.c = 0;

    for (; i < (int)c.c; ++i){

        vp1 = vscreen[i];

	/* for each line that needs to be updated, or that needs its
	   reverse video status changed, call the line updater	*/
	j = vp1->v_flag;
        if (j & VFCHG){

#if	TYPEAH
	    if (typahead()){
#ifdef _WINDOWS
		mswin_endupdate ();
#endif
	        return;
	    }
#endif
            vp2 = pscreen[i];

            updateline(i, &vp1->v_text[0], &vp2->v_text[0], &vp1->v_flag);
	}
    }

    if(Pmaster == NULL){

	if(sgarbk != FALSE){
	    if(term.t_mrow > 0){
		movecursor(term.t_nrow-1, 0);
		peeol();
		movecursor(term.t_nrow, 0);
		peeol();
	    }

	    if(lastflag&CFFILL){
		/* TRANSLATORS: UnJustify means undo the previous
		   Justify command. */
		menu_pico[UNCUT_KEY].label = N_("UnJustify");
		if(!(lastflag&CFFLBF)){
		    emlwrite(_("Can now UnJustify!"), NULL);
		    mpresf = FARAWAY;	/* remove this after next keystroke! */
		}
	    }
	    else
	      menu_pico[UNCUT_KEY].label = N_("UnCut Text");

	    wkeyhelp(menu_pico);
	    sgarbk = FALSE;
        }
    }

    if(lastflag&CFFLBF){
	emlwrite(_("Can now UnJustify!"), NULL);
	mpresf = FARAWAY;  /* remove this after next keystroke! */
    }

    /* Finally, update the hardware cursor and flush out buffers. */

    movecursor(currow, curcol - lbound);
#ifdef _WINDOWS
    mswin_endupdate ();

    /* 
     * Update the scroll bars.  This function is where curbp->b_linecnt
     * is really managed.  See update_scroll.
     */
    update_scroll ();
#endif
    (*term.t_flush)();
}


/* updext - update the extended line which the cursor is currently
 *	    on at a column greater than the terminal width. The line
 *	    will be scrolled right or left to let the user see where
 *	    the cursor is
 */
void
updext(void)
{
    int   rcursor;		/* real cursor location */
    LINE *lp;			/* pointer to current line */
    int   j;			/* index into line */
    int   w = 0;
    int   ww;

    /*
     * Calculate what column the real cursor will end up in.
     * The cursor will be in the rcursor'th column. So if we're
     * counting columns 0 1 2 3 and rcursor is 8, then rcursor
     * will be over cell 7.
     *
     * What this effectively does is to scroll the screen as we're
     * moving to the right when the cursor first passes off the
     * screen's right edge. It would be nice if it did the same
     * thing coming back to the left. Instead, in order that the
     * screen's display depends only on the curcol and not on
     * how we got there, the screen scrolls when we pass the
     * t_margin column. It's also kind of funky that you can't
     * see the character under the $ but you can delete it.
     */
    rcursor = ((curcol - term.t_ncol) % (term.t_ncol - term.t_margin + 1)) + term.t_margin;
    lbound = curcol - rcursor + 1;

    /*
     * Make sure lbound is set so that a double-width character does
     * not straddle the boundary. If it does, move over one cell.
     */
    lp = curwp->w_dotp;			/* line to output */
    for (j=0; j<llength(lp) && w < lbound; ++j){
	ww = wcellwidth((UCS) lgetc(lp, j).c);
	w += (ww >= 0 ? ww : 1);
    }

    if(w > lbound)
      lbound = w;


    /* scan through the line outputing characters to the virtual screen
     * once we reach the left edge
     */
    vtmove(currow, -lbound);		/* start scanning offscreen */
    for (j=0; j<llength(lp); ++j)	/* until the end-of-line */
      vtputc(lgetc(lp, j));

    /* truncate the virtual line */
    vteeol();

    /* and put a '$' in column 1, may have to adjust curcol */
    w = wcellwidth((UCS) vscreen[currow]->v_text[0].c);
    vscreen[currow]->v_text[0].c = '$';
    vscreen[currow]->v_text[0].a = 0;
    if(w == 2){
	/*
	 * We want to put $ in the first two columns so that it
	 * takes up the right amount of space, but that means we
	 * have to scoot the real characters over one slot.
	 */
	for (j = term.t_ncol-1; j >= 2; --j)
	  vscreen[currow]->v_text[j] = vscreen[currow]->v_text[j-1];

	vscreen[currow]->v_text[1].c = '$';
	vscreen[currow]->v_text[1].a = 0;
    }
}


/*
 * Update a single line. This does not know how to use insert or delete
 * character sequences; we are using VT52 functionality. Update the physical
 * row and column variables.
 */
void
updateline(int row,			/* row on screen */
	   CELL vline[],		/* what we want it to end up as */
	   CELL pline[],		/* what it looks like now       */
	   short *flags)
{
    CELL *cp1, *cp2, *cp3, *cp4, *cp5, *cp6, *cp7;
    int   display = TRUE;
    int   nbflag;		/* non-blanks to the right flag? */
    int   cleartoeol = 0;

    if(row < 0 || row > term.t_nrow)
      return;

    /* set up pointers to virtual and physical lines */
    cp1 = &vline[0];
    cp2 = &pline[0];
    cp3 = &vline[term.t_ncol];

    /* advance past any common chars at the left */
    while (cp1 != cp3 && cp1[0].c == cp2[0].c && cp1[0].a == cp2[0].a) {
	++cp1;
	++cp2;
    }

/* This can still happen, even though we only call this routine on changed
 * lines. A hard update is always done when a line splits, a massive
 * change is done, or a buffer is displayed twice. This optimizes out most
 * of the excess updating. A lot of computes are used, but these tend to
 * be hard operations that do a lot of update, so I don't really care.
 */
    /* if both lines are the same, no update needs to be done */
    if (cp1 == cp3){
	*flags &= ~VFCHG;			/* mark it clean */
	return;
    }

    /* find out if there is a match on the right */
    nbflag = FALSE;
    cp3 = &vline[term.t_ncol];
    cp4 = &pline[term.t_ncol];

    if(cellwidth_ptr_to_ptr(cp1, cp3) == cellwidth_ptr_to_ptr(cp2, cp4))
      while (cp3[-1].c == cp4[-1].c && cp3[-1].a == cp4[-1].a) {
	--cp3;
	--cp4;
	if (cp3[0].c != ' ' || cp3[0].a != 0)	/* Note if any nonblank */
	  nbflag = TRUE;			/* in right match. */
      }

    cp5 = cp3;

    if (nbflag == FALSE && TERM_EOLEXIST) {	/* Erase to EOL ? */
	while (cp5 != cp1 && cp5[-1].c == ' ' && cp5[-1].a == 0)
	  --cp5;

	if (cp3-cp5 <= 3)		/* Use only if erase is */
	  cp5 = cp3;			/* fewer characters. */
    }

    /* go to start of differences */
    movecursor(row, cellwidth_ptr_to_ptr(&vline[0], cp1));

    if (!nbflag) {				/* use insert or del char? */
	cp6 = cp3;
	cp7 = cp4;

	if(TERM_INSCHAR
	   &&(cp7!=cp2 && cp6[0].c==cp7[-1].c && cp6[0].a==cp7[-1].a)){
	    while (cp7 != cp2 && cp6[0].c==cp7[-1].c && cp6[0].a==cp7[-1].a){
		--cp7;
		--cp6;
	    }

	    if (cp7==cp2 && cp4-cp2 > 3){
		int ww;

		(*term.t_rev)(cp1->a);	/* set inverse for this char */
		o_insert((UCS) cp1->c);  /* insert the char */
		ww = wcellwidth((UCS) cp1->c);
		ttcol += (ww >= 0 ? ww : 1);
		display = FALSE;        /* only do it once!! */
	    }
	}
	else if(TERM_DELCHAR && cp3 != cp1 && cp7[0].c == cp6[-1].c
		&& cp7[0].a == cp6[-1].a){
	    while (cp6 != cp1 && cp7[0].c==cp6[-1].c && cp7[0].a==cp6[-1].a){
		--cp7;
		--cp6;
	    }

	    if (cp6==cp1 && cp5-cp6 > 3){
		int w;

		w = wcellwidth((UCS) cp7[0].c);
		w = (w >= 0 ? w : 1);
		while(w-- > 0)		/* in case double-width char */
		  o_delete();		/* delete the char */
		display = FALSE;        /* only do it once!! */
	    }
	}
    }

    if(cp1 != cp5 && display){
	int w1, w2;

	/*
	 * If we need to copy characters from cp1 to cp2 and
	 * we need to display them, then we have to worry about
	 * the characters that we are replacing being of a different
	 * width than the new characters, else the display may be
	 * messed up.
	 *
	 * If the new width (w1) is less than the old width, that means
	 * we will leave behind some old remnants if we aren't careful.
	 * If the new width is larger than the old width, we have to
	 * make sure we draw the characters all the way to the end
	 * in order to get it right. Take advantage of clear to end
	 * of line if we have it.
	 */
	w1 = cellwidth_ptr_to_ptr(cp1, cp3);
	w2 = cellwidth_ptr_to_ptr(cp2, cp2 + (cp3-cp1));

	if(w1 < w2 || (nbflag && w1 != w2)){
	    if(TERM_EOLEXIST){
		if(nbflag){
		    /*
		     * Draw all of the characters starting with cp1
		     * until we get to all spaces, then clear to the end of
		     * line from there. Watch out we don't run over the
		     * right hand edge, which shouldn't happen.
		     *
		     * Set cp5 to the first of the repeating spaces.
		     */
		    cp5 = &vline[term.t_ncol];
		    while (cp5 != cp1 && cp5[-1].c == ' ' && cp5[-1].a == 0)
		      --cp5;
		}

		/*
		 * In the !nbflag case we want spaces from cp5 on.
		 * Setting cp3 to something different from cp5 triggers
		 * the clear to end of line below.
		 */
		if(cellwidth_ptr_to_ptr(&vline[0], cp5) < term.t_ncol)
		  cleartoeol++;
	    }
	    else{
		int w;

		/*
		 * No peeol so draw all the way to the edge whether they
		 * are spaces or not.
		 */
		cp3 = &vline[0];
		for(w = 0; w < term.t_ncol; cp3++){
		    int ww;

		    ww = wcellwidth((UCS) cp3->c);
		    w += (ww >= 0 ? ww : 1);
		}

		cp5 = cp3;
	    }
	}
    }

    while (cp1 != cp5) {		/* Ordinary. */
	int ww;

	if(display){
	    (*term.t_rev)(cp1->a);	/* set inverse for this char */
	    (*term.t_putchar)(cp1->c);
	}

	ww = wcellwidth((UCS) cp1->c);
	ttcol += (ww >= 0 ? ww : 1);

	*cp2++ = *cp1++;
    }

    (*term.t_rev)(0);			/* turn off inverse anyway! */

    if (cp5 != cp3 || cleartoeol) {	/* Erase. */
	if(display)
	  peeol();
	else
	  while (cp1 != cp3)
	    *cp2++ = *cp1++;
    }

    *flags &= ~VFCHG;			/* flag this line is changed */
}


/*
 * Redisplay the mode line for the window pointed to by the "wp". This is the
 * only routine that has any idea of how the modeline is formatted. You can
 * change the modeline format by hacking at this routine. Called by "update"
 * any time there is a dirty window.
 */
void
modeline(WINDOW *wp)
{
    if(Pmaster){
        if(ComposerEditing)
	  ShowPrompt();
	else{
	    menu_compose[EXIT_KEY].label  = (Pmaster->headents)
					      ? N_("Send") :N_("Exit");
	    menu_compose[PSTPN_KEY].name  = (Pmaster->headents)
					      ? "^O" : NULL;
	    menu_compose[PSTPN_KEY].label = (Pmaster->headents)
					      ? N_("Postpone") : NULL;
	    menu_compose[WHERE_KEY].name  = (Pmaster->alt_ed) ? "^_" : "^W";
	    menu_compose[WHERE_KEY].label = (Pmaster->alt_ed) ? N_("Alt Edit") 
							      : N_("Where is");
	    KS_OSDATASET(&menu_compose[WHERE_KEY],
			 (Pmaster->alt_ed) ? KS_ALTEDITOR : KS_WHEREIS);
	    menu_compose[UNCUT_KEY].label = (thisflag&CFFILL) ? N_("UnJustify")
							      : N_("UnCut Text");
	    wkeyhelp(menu_compose);
	}
    }
    else{
	BUFFER  *bp;
	char     t1[NLINE], t2[NLINE], t3[NLINE], tline[NLINE];
	int      w1, w2, w3, w1_to_2, w2_to_3, w3_to_r;
	UCS     *ucs;

	vtmove(1, 0);
	vteeol();
	vscreen[0]->v_flag |= VFCHG; /* Redraw next time. */
	vtmove(0, 0);		/* Seek to right line. */

	snprintf(t1, sizeof(t1), PICO_TITLE, version);	/* write version */

	bp = wp->w_bufp;
	if(bp->b_fname[0])				/* File name? */
	  snprintf(t2, sizeof(t2), "File: %s", bp->b_fname);
        else{
	    strncpy(t2, PICO_NEWBUF_MSG, sizeof(t2));
	    t2[sizeof(t2)-1] = '\0';
	}

	if(bp->b_flag&BFCHG){				/* "MOD" if changed. */
	    strncpy(t3, PICO_MOD_MSG, sizeof(t3));
	    t3[sizeof(t3)-1] = '\0';
	}
	else
	  t3[0] = '\0';

#define ALLOFTHEM (w1+w1_to_2+w2+w2_to_3+w3+w3_to_r)
#define ALLBUTSPACE (w1+w2+w3+w3_to_r)

	w1 = utf8_width(t1);
	w2 = utf8_width(t2);
	w3 = utf8_width(t3);
	w1_to_2 = w2_to_3 = 1;		/* min values for separation */
	w3_to_r = 2;

	if(ALLOFTHEM <= term.t_ncol){	/* everything fits */
	  w1_to_2 = (term.t_ncol - ALLBUTSPACE)/2;
	  w2_to_3 = term.t_ncol - (ALLBUTSPACE + w1_to_2);
	}
	else{
	  w1 = 2;
	  w1_to_2 = 0;
	  if(ALLOFTHEM <= term.t_ncol)
	    w2_to_3 = term.t_ncol - (ALLBUTSPACE + w1_to_2);
	  else{
	    w1 = w1_to_2 = w3_to_r = 0;
	    if(ALLOFTHEM <= term.t_ncol)
	      w2_to_3 = term.t_ncol - (ALLBUTSPACE + w1_to_2);
	    else{
	      if(bp->b_fname[0]){
	        snprintf(t2, sizeof(t2), "%s", bp->b_fname);
	        w2 = utf8_width(t2);
	      }

	      if(ALLOFTHEM <= term.t_ncol)
	        w2_to_3 = term.t_ncol - (ALLBUTSPACE + w1_to_2);
	      else{
	        w2 = 8;
	        if(bp->b_fname[0] && ALLOFTHEM <= term.t_ncol){
	          /* reduce size of file */
		  w2 = term.t_ncol - (ALLOFTHEM - w2);
		  t2[0] = t2[1] = t2[2] = '.';
		  utf8_to_width_rhs(t2+3, bp->b_fname, sizeof(t2)-3, w2-3);
		  w2 = utf8_width(t2);
	        }
	        else
	          w2 = utf8_width(t2);

	        if(ALLOFTHEM <= term.t_ncol)
	          w2_to_3 = term.t_ncol - (ALLBUTSPACE + w1_to_2);
	        else{
	          w1 = w1_to_2 = w2 = w2_to_3 = w3_to_r = 0;
	          if(ALLOFTHEM <= term.t_ncol)
	            w2_to_3 = term.t_ncol - ALLBUTSPACE;
		  else
		    w3 = 0;
	        }
	      }
	    }
	  }
	}

	utf8_snprintf(tline, sizeof(tline),
		      "%*.*w%*.*w%*.*w%*.*w%*.*w%*.*w",
		      w1, w1, t1,
		      w1_to_2, w1_to_2, "",
		      w2, w2, t2,
		      w2_to_3, w2_to_3, "",
		      w3, w3, t3,
		      w3_to_r, w3_to_r, "");

	ucs = NULL;
	if(utf8_width(tline) <= term.t_ncol)
	  ucs = utf8_to_ucs4_cpystr(tline);

	if(ucs){
	    UCS *ucsp;
	    CELL     c;

	    c.a = 1;
	    ucsp = ucs;
	    while((c.c = CELLMASK & *ucsp++))
	      vtputc(c);

	    fs_give((void **) &ucs);
	}
    }
}



/*
 * Send a command to the terminal to move the hardware cursor to row "row"
 * and column "col". The row and column arguments are origin 0. Optimize out
 * random calls. Update "ttrow" and "ttcol".
 */
void
movecursor(int row, int col)
{
    if (row!=ttrow || col!=ttcol) {
        ttrow = row;
        ttcol = col;
        (*term.t_move)(MIN(MAX(row,0),term.t_nrow), MIN(MAX(col,0),term.t_ncol-1));
    }
}


/*
 * Erase any sense we have of the cursor's HW location...
 */
void
clearcursor(void)
{
    ttrow = ttcol = FARAWAY;
}

void
get_cursor(int *row, int *col)
{
    if(row)
      *row = ttrow;
    if(col)
      *col = ttcol;
}


/*
 * Erase the message line. This is a special routine because the message line
 * is not considered to be part of the virtual screen. It always works
 * immediately; the terminal buffer is flushed via a call to the flusher.
 */
void
mlerase(void)
{
    if (term.t_nrow < term.t_mrow)
      return;

    movecursor(term.t_nrow - term.t_mrow, 0);
    (*term.t_rev)(0);
    if (TERM_EOLEXIST == TRUE)
      peeol();
    else{
	if(ttrow == term.t_nrow){
	  while(ttcol++ < term.t_ncol-1)
	    (*term.t_putchar)(' ');
	}
	else{
	  while(ttcol++ < term.t_ncol)		/* track's ttcol */
	    (*term.t_putchar)(' ');
	}
    }

    (*term.t_flush)();
    mpresf = FALSE;
}


int
mlyesno_utf8(char *utf8prompt, int dflt)
{
    int  ret;
    UCS *prompt;

    prompt = utf8_to_ucs4_cpystr(utf8prompt ? utf8prompt : "");

    ret = mlyesno(prompt, dflt);

    if(prompt)
      fs_give((void **) &prompt);

    return(ret);
}


/*
 * Ask a yes or no question in the message line. Return either TRUE, FALSE, or
 * ABORT. The ABORT status is returned if the user bumps out of the question
 * with a ^G. if d >= 0, d is the default answer returned. Otherwise there
 * is no default.
 */
int
mlyesno(UCS *prompt, int dflt)
{
    int     rv;
    UCS     buf[NLINE], lbuf[10];
    KEYMENU menu_yesno[12];
    COLOR_PAIR *lastc = NULL;

#ifdef _WINDOWS
    if (mswin_usedialog ()) 
      switch (mswin_yesno (prompt)) {
	default:
	case 0:		return (ABORT);
	case 1:		return (TRUE);
	case 2:		return (FALSE);
      }
#endif  

    for(rv = 0; rv < 12; rv++){
	menu_yesno[rv].name = NULL;
	KS_OSDATASET(&menu_yesno[rv], KS_NONE);
    }

    menu_yesno[1].name  = "Y";
    menu_yesno[1].label = (dflt == TRUE) ? "[" N_("Yes") "]" : N_("Yes");
    menu_yesno[6].name  = "^C";
    menu_yesno[6].label = N_("Cancel");
    menu_yesno[7].name  = "N";
    menu_yesno[7].label = (dflt == FALSE) ? "[" N_("No") "]" : N_("No");
    wkeyhelp(menu_yesno);		/* paint generic menu */
    sgarbk = TRUE;			/* mark menu dirty */
    if(Pmaster && curwp)
      curwp->w_flag |= WFMODE;

    ucs4_strncpy(buf, prompt, NLINE);
    buf[NLINE-1] = '\0';
    lbuf[0] = ' '; lbuf[1] = '?'; lbuf[2] = ' '; lbuf[3] = '\0';
    ucs4_strncat(buf, lbuf, NLINE - ucs4_strlen(buf) - 1);
    buf[NLINE-1] = '\0';
    mlwrite(buf, NULL);
    if(Pmaster && Pmaster->colors && Pmaster->colors->prcp
       && pico_is_good_colorpair(Pmaster->colors->prcp)){
	lastc = pico_get_cur_color();
	(void) pico_set_colorp(Pmaster->colors->prcp, PSC_NONE);
    }
    else
      (*term.t_rev)(1);

    rv = -1;
    while(1){
	switch(GetKey()){
	  case (CTRL|'M') :		/* default */
	    if(dflt >= 0){
		pputs_utf8((dflt) ? _("Yes") : _("No"), 1);
		rv = dflt;
	    }
	    else
	      (*term.t_beep)();

	    break;

	  case (CTRL|'C') :		/* Bail out! */
	  case F2         :
	    pputs_utf8(_("ABORT"), 1);
	    rv = ABORT;
	    break;

	  case 'y' :
	  case 'Y' :
	  case F3  :
	    pputs_utf8(_("Yes"), 1);
	    rv = TRUE;
	    break;

	  case 'n' :
	  case 'N' :
	  case F4  :
	    pputs_utf8(_("No"), 1);
	    rv = FALSE;
	    break;

	  case (CTRL|'G') :
	    if(term.t_mrow == 0 && km_popped == 0){
		movecursor(term.t_nrow-2, 0);
		peeol();
		term.t_mrow = 2;
		if(lastc){
		    (void) pico_set_colorp(lastc, PSC_NONE);
		    free_color_pair(&lastc);
		}
		else
		  (*term.t_rev)(0);

		wkeyhelp(menu_yesno);		/* paint generic menu */
		mlwrite(buf, NULL);
		if(Pmaster && Pmaster->colors && Pmaster->colors->prcp
		   && pico_is_good_colorpair(Pmaster->colors->prcp)){
		    lastc = pico_get_cur_color();
		    (void) pico_set_colorp(Pmaster->colors->prcp, PSC_NONE);
		}
		else
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
	    if(lastc){
		(void) pico_set_colorp(lastc, PSC_NONE);
		free_color_pair(&lastc);
	    }
	    else
	      (*term.t_rev)(0);

	    if(km_popped){
		term.t_mrow = 0;
		movecursor(term.t_nrow, 0);
		peeol();
		sgarbf = 1;
		km_popped = 0;
	    }

	    return(rv);
	}
    }
}


/*
 * Write a prompt into the message line, then read back a response. Keep
 * track of the physical position of the cursor. If we are in a keyboard
 * macro throw the prompt away, and return the remembered response. This
 * lets macros run at full speed. The reply is always terminated by a carriage
 * return. Handle erase, kill, and abort keys.
 */
int
mlreply_utf8(char *utf8prompt, char *utf8buf, int nbuf, int flg, EXTRAKEYS *extras)
{
    return(mlreplyd_utf8(utf8prompt, utf8buf, nbuf, flg|QDEFLT, extras));
}


int
mlreply(UCS *prompt, UCS *buf, int nbuf, int flg, EXTRAKEYS *extras)
{
    return(mlreplyd(prompt, buf, nbuf, flg|QDEFLT, extras));
}


/*
 * function key mappings
 */
static UCS rfkm[12][2] = {
    { F1,  (CTRL|'G')},
    { F2,  (CTRL|'C')},
    { F3,  0 },
    { F4,  0 },
    { F5,  0 },
    { F6,  0 },
    { F7,  0 },
    { F8,  0 },
    { F9,  0 },
    { F10, 0 },
    { F11, 0 },
    { F12, 0 }
};


int
mlreplyd_utf8(char *utf8prompt, char *utf8buf, int nbuf, int flg, EXTRAKEYS *extras)
{
    int  ret;
    UCS   *b, *buf;
    char  *utf8;
    UCS   *prompt;

    buf = (UCS *) fs_get(nbuf * sizeof(*b));
    b = utf8_to_ucs4_cpystr(utf8buf);
    if(b){
	ucs4_strncpy(buf, b, nbuf);
	buf[nbuf-1] = '\0';
	fs_give((void **) &b);
    }

    prompt = utf8_to_ucs4_cpystr(utf8prompt ? utf8prompt : "");

    ret = mlreplyd(prompt, buf, nbuf, flg, extras);

    utf8 = ucs4_to_utf8_cpystr(buf);
    if(utf8){
	strncpy(utf8buf, utf8, nbuf);
	utf8buf[nbuf-1] = '\0';
	fs_give((void **) &utf8);
    }

    if(buf)
      fs_give((void **) &buf);

    if(prompt)
      fs_give((void **) &prompt);

    return(ret);
}


void
writeachar(UCS ucs)
{
    pputc(ucs, 0);
}


/*
 * mlreplyd - write the prompt to the message line along with a default
 *	      answer already typed in.  Carriage return accepts the
 *	      default.  answer returned in buf which also holds the initial
 *            default, nbuf is its length, def set means use default value.
 */
int
mlreplyd(UCS *prompt, UCS *buf, int nbuf, int flg, EXTRAKEYS *extras)
{
    UCS      c;				/* current char       */
    UCS     *b;				/* pointer in buf     */
    int      i, j;
    int      plen;
    int      changed = FALSE;
    int      return_val = 0;
    KEYMENU  menu_mlreply[12];
    UCS	     extra_v[12];
    struct   display_line dline;
    COLOR_PAIR *lastc = NULL;

#ifdef _WINDOWS
    if(mswin_usedialog()){
	MDlgButton		btn_list[12];
	LPTSTR                  free_names[12];
	LPTSTR                  free_labels[12];
	int			i, j;

	memset(&free_names, 0, sizeof(LPTSTR) * 12);
	memset(&free_labels, 0, sizeof(LPTSTR) * 12);
	memset(&btn_list, 0, sizeof (MDlgButton) * 12);
	j = 0;
	for(i = 0; extras && extras[i].name != NULL; ++i) {
	    if(extras[i].label[0] != '\0') {
		if((extras[i].key & CTRL) == CTRL) 
		  btn_list[j].ch = (extras[i].key & ~CTRL) - '@';
		else
		  btn_list[j].ch = extras[i].key;

		btn_list[j].rval = extras[i].key;
		free_names[j] = utf8_to_lptstr(extras[i].name);
		btn_list[j].name = free_names[j];
		free_labels[j] = utf8_to_lptstr(extras[i].label);
		btn_list[j].label = free_labels[j];
		j++;
	    }
	}

	btn_list[j].ch = -1;

	return_val = mswin_dialog(prompt, buf, nbuf, ((flg&QDEFLT) > 0), 
			    FALSE, btn_list, NULL, 0);

	if(return_val == 3)
	  return_val = HELPCH;

	for(i = 0; i < 12; i++){
	    if(free_names[i])
	      fs_give((void **) &free_names[i]);
	    if(free_labels[i])
	      fs_give((void **) &free_labels[i]);
	}

	return(return_val);
    }
#endif

    menu_mlreply[0].name = "^G";
    menu_mlreply[0].label = N_("Get Help");
    KS_OSDATASET(&menu_mlreply[0], KS_SCREENHELP);
    for(j = 0, i = 1; i < 6; i++){	/* insert odd extras */
	menu_mlreply[i].name = NULL;
	KS_OSDATASET(&menu_mlreply[i], KS_NONE);
	rfkm[2*i][1] = 0;
	if(extras){
	    for(; extras[j].name && j != 2*(i-1); j++)
	      ;

	    if(extras[j].name){
		rfkm[2*i][1]	      = extras[j].key;
		menu_mlreply[i].name  = extras[j].name;
		menu_mlreply[i].label = extras[j].label;
		KS_OSDATASET(&menu_mlreply[i], KS_OSDATAGET(&extras[j]));
	    }
	}
    }

    menu_mlreply[6].name = "^C";
    menu_mlreply[6].label = N_("Cancel");
    KS_OSDATASET(&menu_mlreply[6], KS_NONE);
    for(j = 0, i = 7; i < 12; i++){	/* insert even extras */
	menu_mlreply[i].name = NULL;
	rfkm[2*(i-6)+1][1] = 0;
	if(extras){
	    for(; extras[j].name && j != (2*(i-6)) - 1; j++)
	      ;

	    if(extras[j].name){
		rfkm[2*(i-6)+1][1]    = extras[j].key;
		menu_mlreply[i].name  = extras[j].name;
		menu_mlreply[i].label = extras[j].label;
		KS_OSDATASET(&menu_mlreply[i], KS_OSDATAGET(&extras[j]));
	    }
	}
    }

    /* set up what to watch for and return values */
    memset(extra_v, 0, sizeof(extra_v));
    for(i = 0, j = 0; i < 12 && extras && extras[i].name; i++)
      extra_v[j++] = extras[i].key;

    plen = mlwrite(prompt, NULL);		/* paint prompt */

    if(!(flg&QDEFLT))
      *buf = '\0';

    dline.vused = ucs4_strlen(buf);
    dline.dwid  = term.t_ncol - plen;
    dline.row   = term.t_nrow - term.t_mrow;
    dline.col   = plen;

    dline.dlen  = 2 * dline.dwid + 100;

    dline.dl    = (UCS *) fs_get(dline.dlen * sizeof(UCS));
    dline.olddl = (UCS *) fs_get(dline.dlen * sizeof(UCS));
    memset(dline.dl,    0, dline.dlen * sizeof(UCS));
    memset(dline.olddl, 0, dline.dlen * sizeof(UCS));

    dline.movecursor = movecursor;
    dline.writechar  = writeachar;

    dline.vl    = buf;
    dline.vlen  = nbuf-1;
    dline.vbase = 0;

    b = &buf[(flg & QBOBUF) ? 0 : ucs4_strlen(buf)];
    
    wkeyhelp(menu_mlreply);		/* paint generic menu */

    sgarbk = 1;				/* mark menu dirty */

    if(Pmaster && Pmaster->colors && Pmaster->colors->prcp
       && pico_is_good_colorpair(Pmaster->colors->prcp)){
	lastc = pico_get_cur_color();
	(void) pico_set_colorp(Pmaster->colors->prcp, PSC_NONE);
    }
    else
      (*term.t_rev)(1);

    for(;;){

	line_paint(b-buf, &dline, NULL);
	(*term.t_flush)();

#ifdef	MOUSE
	mouse_in_content(KEY_MOUSE, -1, -1, 0x5, 0);
	register_mfunc(mouse_in_content, 
		       term.t_nrow - term.t_mrow, plen,
		       term.t_nrow - term.t_mrow, term.t_ncol-1);
#endif
#ifdef	_WINDOWS
	mswin_allowpaste(MSWIN_PASTE_LINE);
#endif
	while((c = GetKey()) == NODATA)
	  ;

#ifdef	MOUSE
	clear_mfunc(mouse_in_content);
#endif
#ifdef	_WINDOWS
	mswin_allowpaste(MSWIN_PASTE_DISABLE);
#endif

	switch(c = normalize_cmd(c, rfkm, 1)){
	  case (CTRL|'A') :			/* CTRL-A beginning     */
	  case KEY_HOME :
	    b = buf;
	    continue;

	  case (CTRL|'B') :			/* CTRL-B back a char   */
	  case KEY_LEFT:
	    if(b <= buf)
	      (*term.t_beep)();
	    else
	      b--;

	    continue;

	  case (CTRL|'C') :			/* CTRL-C abort		*/
	    pputs_utf8(_("ABORT"), 1);
	    ctrlg(FALSE, 0);
	    return_val = ABORT;
	    goto ret;

	  case (CTRL|'E') :			/* CTRL-E end of line   */
	  case KEY_END :
	    b = &buf[ucs4_strlen(buf)];
	    continue;

	  case (CTRL|'F') :			/* CTRL-F forward a char*/
	  case KEY_RIGHT :
	    if(*b == '\0')
	      (*term.t_beep)();
	    else
	      b++;

	    continue;

	  case (CTRL|'G') :			/* CTRL-G help		*/
	    if(term.t_mrow == 0 && km_popped == 0){
		movecursor(term.t_nrow-2, 0);
		peeol();
		sgarbk = 1;			/* mark menu dirty */
		km_popped++;
		term.t_mrow = 2;
		if(lastc){
		    (void) pico_set_colorp(lastc, PSC_NONE);
		    free_color_pair(&lastc);
		}
		else
		  (*term.t_rev)(0);

		wkeyhelp(menu_mlreply);		/* paint generic menu */
		plen = mlwrite(prompt, NULL);		/* paint prompt */
		if(Pmaster && Pmaster->colors && Pmaster->colors->prcp
		   && pico_is_good_colorpair(Pmaster->colors->prcp)){
		    lastc = pico_get_cur_color();
		    (void) pico_set_colorp(Pmaster->colors->prcp, PSC_NONE);
		}
		else
		  (*term.t_rev)(1);

		pputs(buf, 1);
		break;
	    }

	    pputs_utf8(_("HELP"), 1);
	    return_val = HELPCH;
	    goto ret;

	  case (CTRL|'H') :			/* CTRL-H backspace	*/
	  case 0x7f :				/*        rubout	*/
	    if (b <= buf){
	      (*term.t_beep)();
	      break;
	    }
	    else
	      b--;

	  case (CTRL|'D') :			/* CTRL-D delete char   */
	  case KEY_DEL :
	    if (!*b){
	      (*term.t_beep)();
	      break;
	    }

	    changed=TRUE;
	    i = 0;
	    dline.vused--;
	    do					/* blat out left char   */
	      b[i] = b[i+1];
	    while(b[i++] != '\0');
	    break;

	  case (CTRL|'L') :			/* CTRL-L redraw	*/
	    return_val = (CTRL|'L');
	    goto ret;

	  case (CTRL|'K') :			/* CTRL-K kill line	*/
	    changed=TRUE;
	    buf[0] = '\0';
	    dline.vused = 0;
	    b = buf;
	    break;

	  case F1 :				/* sort of same thing */
	    return_val = HELPCH;
	    goto ret;

	  case (CTRL|'M') :			/*        newline       */
	    return_val = changed;
	    goto ret;

#ifdef	MOUSE
	  case KEY_MOUSE :
	    {
	      MOUSEPRESS mp;

	      mouse_get_last (NULL, &mp);

	      /* The clicked line have anything special on it? */
	      switch(mp.button){
		case M_BUTTON_LEFT :			/* position cursor */
		  mp.col -= plen;			/* normalize column */
		  if(mp.col >= 0 && mp.col <= ucs4_strlen(buf))
		    b = buf + mp.col;

		  break;

		case M_BUTTON_RIGHT :
#ifdef	_WINDOWS
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

	    continue;
#endif

	  default : 

	    /* look for match in extra_v */
	    for(i = 0; i < 12; i++)
	      if(c && c == extra_v[i]){
		  return_val = c;
		  goto ret;
	      }

	    changed=TRUE;

	    if(c & (CTRL | FUNC)){		/* bag ctrl_special chars */
		(*term.t_beep)();
	    }
	    else{
		i = ucs4_strlen(b);
		if(flg&QNODQT){	                /* reject double quotes? */
		    if(c == '"'){
			(*term.t_beep)();
			continue;
		    }
		}

		if(dline.vused >= nbuf-1){
		    (*term.t_beep)();
		    continue;
		}

		do				/* blat out left char   */
		  b[i+1] = b[i];
		while(i-- > 0);

		dline.vused++;
		*b++ = c;
	    }
	}
    }

ret:
    if(lastc){
	(void) pico_set_colorp(lastc, PSC_NONE);
	free_color_pair(&lastc);
    }
    else
      (*term.t_rev)(0);

    (*term.t_flush)();

    if(km_popped){
	term.t_mrow = 0;
	movecursor(term.t_nrow, 0);
	peeol();
	sgarbf = 1;
	km_popped = 0;
    }

    if(dline.dl)
      fs_give((void **) &dline.dl);

    if(dline.olddl)
      fs_give((void **) &dline.olddl);

    return(return_val);
}


void
emlwrite(char *utf8message, EML *eml)
{
    UCS *message;

    message = utf8_to_ucs4_cpystr(utf8message ? utf8message : "");

    emlwrite_ucs4(message, eml);

    if(message)
      fs_give((void **) &message);
}


/*
 * emlwrite() - write the message string to the error half of the screen
 *              center justified.  much like mlwrite (which is still used
 *              to paint the line for prompts and such), except it center
 *              the text.
 */
void
emlwrite_ucs4(UCS *message, EML *eml) 
{
    UCS  *bufp, *ap;
    int   width;
    COLOR_PAIR *lastc = NULL;

    mlerase();

    if(!(message && *message) || term.t_nrow < 2)	
      return;    /* nothing to write or no space to write, bag it */

    bufp = message;

    width = ucs4_str_width(message);

    /*
     * next, figure out where the to move the cursor so the message 
     * comes out centered
     */
    if((ap=ucs4_strchr(message, '%')) != NULL){
	width -= 2;
	switch(ap[1]){
	  case '%':
	  case 'c':
	    width += (eml && eml->c) ? wcellwidth(eml->c) : 1;
	    break;
	  case 'd':
	    width += dumbroot(eml ? eml->d : 0, 10);
	    break;
	  case 'D':
	    width += dumblroot(eml ? eml->l : 0L, 10);
	    break;
	  case 'o':
	    width += dumbroot(eml ? eml->d : 0, 8);
	    break;
	  case 'x':
	    width += dumbroot(eml ? eml->d : 0, 16);
	    break;
	  case 's':				/* string arg is UTF-8 */
            width += (eml && eml->s) ? utf8_width(eml->s) : 2;
	    break;
	}
    }

    if(width+4 <= term.t_ncol)
      movecursor(term.t_nrow-term.t_mrow, (term.t_ncol - (width + 4))/2);
    else
      movecursor(term.t_nrow-term.t_mrow, 0);

    if(Pmaster && Pmaster->colors && Pmaster->colors->stcp
       && pico_is_good_colorpair(Pmaster->colors->stcp)){
	lastc = pico_get_cur_color();
	(void) pico_set_colorp(Pmaster->colors->stcp, PSC_NONE);
    }
    else
      (*term.t_rev)(1);

    pputs_utf8("[ ", 1);
    while (*bufp != '\0' && ttcol < term.t_ncol-2){
	if(*bufp == '\007')
	  (*term.t_beep)();
	else if(*bufp == '%'){
	    switch(*++bufp){
	      case 'c':
		if(eml && eml->c)
		  pputc(eml->c, 0);
		else {
		    pputs_utf8("%c", 0);
		}
		break;
	      case 'd':
		mlputi(eml ? eml->d : 0, 10);
		break;
	      case 'D':
		mlputli(eml ? eml->l : 0L, 10);
		break;
	      case 'o':
		mlputi(eml ? eml->d : 0, 16);
		break;
	      case 'x':
		mlputi(eml ? eml->d : 0, 8);
		break;
	      case 's':
		pputs_utf8((eml && eml->s) ? eml->s : "%s", 0);
		break;
	      case '%':
	      default:
		pputc(*bufp, 0);
		break;
	    }
	}
	else
	  pputc(*bufp, 0);
	bufp++;
    }

    pputs_utf8(" ]", 1);

    if(lastc){
	(void) pico_set_colorp(lastc, PSC_NONE);
	free_color_pair(&lastc);
    }
    else
      (*term.t_rev)(0);

    (*term.t_flush)();

    mpresf = TRUE;
}


int
mlwrite_utf8(char *utf8fmt, void *arg)
{
    UCS  *fmt;
    int   ret;

    fmt = utf8_to_ucs4_cpystr(utf8fmt ? utf8fmt : "");
    ret = mlwrite(fmt, arg);
    if(fmt)
      fs_give((void **) &fmt);

    return(ret);
}


/*
 * Write a message into the message line. Keep track of the physical cursor
 * position. A small class of printf like format items is handled. Assumes the
 * stack grows down; this assumption is made by the "++" in the argument scan
 * loop. Set the "message line" flag TRUE.
 */
int
mlwrite(UCS *fmt, void *arg)
{
    int   ret, ww;
    UCS   c;
    char *ap;
    COLOR_PAIR *lastc = NULL;

    /*
     * the idea is to only highlight if there is something to show
     */
    mlerase();
    movecursor(ttrow, 0);

    if(Pmaster && Pmaster->colors && Pmaster->colors->prcp
       && pico_is_good_colorpair(Pmaster->colors->prcp)){
	lastc = pico_get_cur_color();
	(void) pico_set_colorp(Pmaster->colors->prcp, PSC_NONE);
    }
    else
      (*term.t_rev)(1);

    ap = (char *) &arg;

    while ((c = *fmt++) != 0) {
        if (c != '%') {
	    pputc(c, 1);
	}
        else {
            c = *fmt++;
            switch (c){
	      case 'd':
		mlputi(*(int *)ap, 10);
		ap += sizeof(int);
		break;

	      case 'o':
		mlputi(*(int *)ap,  8);
		ap += sizeof(int);
		break;

	      case 'x':
		mlputi(*(int *)ap, 16);
		ap += sizeof(int);
		break;

	      case 'D':
		mlputli(*(long *)ap, 10);
		ap += sizeof(long);
		break;

	      case 's':
		pputs_utf8(*(char **)ap, 1);
		ap += sizeof(char *);
		break;

              default:
		pputc(c, 1);
		ww = wcellwidth(c);
		ttcol += (ww >= 0 ? ww : 1);
	    }
	}
    }

    ret = ttcol;
    while(ttcol < term.t_ncol)
      pputc(' ', 0);

    movecursor(term.t_nrow - term.t_mrow, ret);

    if(lastc){
	(void) pico_set_colorp(lastc, PSC_NONE);
	free_color_pair(&lastc);
    }
    else
      (*term.t_rev)(0);

    (*term.t_flush)();
    mpresf = TRUE;

    return(ret);
}


/*
 * Write out an integer, in the specified radix. Update the physical cursor
 * position. This will not handle any negative numbers; maybe it should.
 */
void
mlputi(int i, int r)
{
    register int q;
    static char hexdigits[] = "0123456789ABCDEF";

    if (i < 0){
        i = -i;
	pputc('-', 1);
    }

    q = i/r;

    if (q != 0)
      mlputi(q, r);

    pputc(hexdigits[i%r], 1);
}


/*
 * do the same except as a long integer.
 */
void
mlputli(long l, int r)
{
    register long q;

    if (l < 0){
        l = -l;
        pputc('-', 1);
    }

    q = l/r;

    if (q != 0)
      mlputli(q, r);

    pputc((int)(l%r)+'0', 1);
}


void
unknown_command(UCS c)
{
    char  buf[10], ch, *s;
    EML   eml;

    buf[0] = '\0';
    s = buf;

    if(!c){
	/* fall through */
    }
    else if(c & CTRL && c >= (CTRL|'@') && c <= (CTRL|'_')){
	ch = c - (CTRL|'@') + '@';
	snprintf(s, sizeof(buf), "^%c", ch);
    }
    else
     switch(c){
      case ' '       : s = "SPACE";		break;
      case '\033'    : s = "ESC";		break;
      case '\177'    : s = "DEL";		break;
      case ctrl('I') : s = "TAB";		break;
      case ctrl('J') : s = "LINEFEED";		break;
      case ctrl('M') : s = "RETURN";		break;
      case ctrl('Q') : s = "XON";		break;
      case ctrl('S') : s = "XOFF";		break;
      case KEY_UP    : s = "Up Arrow";		break;
      case KEY_DOWN  : s = "Down Arrow";	break;
      case KEY_RIGHT : s = "Right Arrow";	break;
      case KEY_LEFT  : s = "Left Arrow";	break;
      case CTRL|KEY_UP    : s = "Ctrl-Up Arrow";	break;
      case CTRL|KEY_DOWN  : s = "Ctrl-Down Arrow";	break;
      case CTRL|KEY_RIGHT : s = "Ctrl-Right Arrow";	break;
      case CTRL|KEY_LEFT  : s = "Ctrl-Left Arrow";	break;
      case KEY_PGUP  : s = "Prev Page";		break;
      case KEY_PGDN  : s = "Next Page";		break;
      case KEY_HOME  : s = "Home";		break;
      case KEY_END   : s = "End";		break;
      case KEY_DEL   : s = "Delete";		break; /* Not necessary DEL! */
      case F1	     :
      case F2	     :
      case F3	     :
      case F4	     :
      case F5	     :
      case F6	     :
      case F7	     :
      case F8	     :
      case F9	     :
      case F10	     :
      case F11	     :
      case F12	     :
        snprintf(s, sizeof(buf), "F%ld", (long) (c - PF1 + 1));
	break;

      default:
	if(c < CTRL)
	  utf8_put((unsigned char *) s, (unsigned long) c);

	break;
     }

    eml.s = s;
    emlwrite("Unknown Command: %s", &eml);
    (*term.t_beep)();
}


/*
 * scrolldown - use stuff to efficiently move blocks of text on the
 *              display, and update the pscreen array to reflect those
 *              moves...
 *
 *        wp is the window to move in
 *        r  is the row at which to begin scrolling
 *        n  is the number of lines to scrol
 */
void
scrolldown(WINDOW *wp, int r, int n)
{
#ifdef	TERMCAP
    register int i;
    register int l;
    register VIDEO *vp1;
    register VIDEO *vp2;

    if(!n)
      return;

    if(r < 0){
	r = wp->w_toprow;
	l = wp->w_ntrows;
    }
    else{
	if(r > wp->w_toprow)
	    vscreen[r-1]->v_flag |= VFCHG;
	l = wp->w_toprow+wp->w_ntrows-r;
    }

    o_scrolldown(r, n);

    for(i=l-n-1; i >=  0; i--){
	vp1 = pscreen[r+i]; 
	vp2 = pscreen[r+i+n];
	memcpy(vp2, vp1, term.t_ncol * sizeof(CELL));
    }
    pprints(r+n-1, r);
    ttrow = FARAWAY;
    ttcol = FARAWAY;
#endif /* TERMCAP */
}


/*
 * scrollup - use tcap stuff to efficiently move blocks of text on the
 *            display, and update the pscreen array to reflect those
 *            moves...
 */
void
scrollup(WINDOW *wp, int r, int n)
{
#ifdef	TERMCAP
    register int i;
    register VIDEO *vp1;
    register VIDEO *vp2;

    if(!n)
      return;

    if(r < 0)
      r = wp->w_toprow;

    o_scrollup(r, n);

    i = 0;
    while(1){
	if(Pmaster){
	    if(!(r+i+n < wp->w_toprow+wp->w_ntrows))
	      break;
	}
	else{
	    if(!((i < wp->w_ntrows-n)&&(r+i+n < wp->w_toprow+wp->w_ntrows)))
	      break;
	}
	vp1 = pscreen[r+i+n]; 
	vp2 = pscreen[r+i];
	memcpy(vp2, vp1, term.t_ncol * sizeof(CELL));
	i++;
    }
    pprints(wp->w_toprow+wp->w_ntrows-n, wp->w_toprow+wp->w_ntrows-1);
    ttrow = FARAWAY;
    ttcol = FARAWAY;
#endif /* TERMCAP */
}


/*
 * print spaces in the physical screen starting from row abs(n) working in
 * either the positive or negative direction (depending on sign of n).
 */
void
pprints(int x, int y)
{
    register int i;
    register int j;

    if(x < y){
	for(i = x;i <= y; ++i){
	    for(j = 0; j < term.t_ncol; j++){
		pscreen[i]->v_text[j].c = ' ';
		pscreen[i]->v_text[j].a = 0;
	    }
        }
    }
    else{
	for(i = x;i >= y; --i){
	    for(j = 0; j < term.t_ncol; j++){
		pscreen[i]->v_text[j].c = ' ';
		pscreen[i]->v_text[j].a = 0;
	    }
        }
    }
    ttrow = y;
    ttcol = 0;
}


/*
 * doton - return the physical line number that the dot is on in the
 *         current window, and by side effect the number of lines remaining
 */
int
doton(int *r, unsigned *chs)
{
    register int  i = 0;
    register LINE *lp = curwp->w_linep;
    int      l = -1;

    assert(r != NULL && chs != NULL);

    *chs = 0;
    while(i++ < curwp->w_ntrows){
	if(lp == curwp->w_dotp)
	  l = i-1;
	lp = lforw(lp);
	if(lp == curwp->w_bufp->b_linep){
	    i++;
	    break;
	}
	if(l >= 0)
	  (*chs) += llength(lp);
    }
    *r = i - l - term.t_mrow;
    return(l+curwp->w_toprow);
}



/*
 * resize_pico - given new window dimensions, allocate new resources
 */
int
resize_pico(int row, int col)
{
    int old_nrow, old_ncol;
    register int i;
    register VIDEO *vp;

    old_nrow = term.t_nrow;
    old_ncol = term.t_ncol;

    term.t_nrow = row;
    term.t_ncol = col;

    if (old_ncol == term.t_ncol && old_nrow == term.t_nrow)
      return(TRUE);

    if(curwp){
	curwp->w_toprow = 2;
	curwp->w_ntrows = term.t_nrow - curwp->w_toprow - term.t_mrow;
    }

    if(Pmaster){
	fillcol = Pmaster->fillcolumn;
	(*Pmaster->resize)();
    }
    else if(userfillcol > 0)
      fillcol = userfillcol;
    else
      fillcol = term.t_ncol - 6;	       /* we control the fill column */

    /* 
     * free unused screen space ...
     */
    for(i=term.t_nrow+1; i <= old_nrow; ++i){
	free((char *) vscreen[i]);
	free((char *) pscreen[i]);
    }

    /* 
     * realloc new space for screen ...
     */
    if((vscreen=(VIDEO **)realloc(vscreen,(term.t_nrow+1)*sizeof(VIDEO *))) == NULL){
	if(Pmaster)
	  return(-1);
	else
	  exit(1);
    }

    if((pscreen=(VIDEO **)realloc(pscreen,(term.t_nrow+1)*sizeof(VIDEO *))) == NULL){
	if(Pmaster)
	  return(-1);
	else
	  exit(1);
    }

    for (i = 0; i <= term.t_nrow; ++i) {
	if(i <= old_nrow)
	  vp = (VIDEO *) realloc(vscreen[i], sizeof(VIDEO)+(term.t_ncol*sizeof(CELL)));
	else
	  vp = (VIDEO *) malloc(sizeof(VIDEO)+(term.t_ncol*sizeof(CELL)));

	if (vp == NULL)
	  exit(1);
	vp->v_flag = VFCHG;
	vscreen[i] = vp;
	if(old_ncol < term.t_ncol){  /* don't let any garbage in */
	    vtrow = i;
	    vtcol = (i < old_nrow) ? old_ncol : 0;
	    vteeol();
	}

	if(i <= old_nrow)
	  vp = (VIDEO *) realloc(pscreen[i], sizeof(VIDEO)+(term.t_ncol*sizeof(CELL)));
	else
	  vp = (VIDEO *) malloc(sizeof(VIDEO)+(term.t_ncol*sizeof(CELL)));

	if (vp == NULL)
	  exit(1);

	vp->v_flag = VFCHG;
	pscreen[i] = vp;
    }

    if(!ResizeBrowser()){
	if(Pmaster && Pmaster->headents){
	    ResizeHeader();
	}
	else{
	    curwp->w_flag |= (WFHARD | WFMODE);
	    pico_refresh(0, 1);                /* redraw whole enchilada. */
	    update();                          /* do it */
	}
    }

    return(TRUE);
}

void
redraw_pico_for_callback(void)
{
    pico_refresh(0, 1);
    update();
}


/*
 * showCompTitle - display the anchor line passed in from pine
 */
void
showCompTitle(void)
{
    if(Pmaster){
	UCS *bufp;
	extern   UCS *pico_anchor;
	COLOR_PAIR *lastc = NULL;

	if((bufp = pico_anchor) == NULL)
	  return;
	
	movecursor(COMPOSER_TITLE_LINE, 0);
	if (Pmaster->colors && Pmaster->colors->tbcp &&
	    pico_is_good_colorpair(Pmaster->colors->tbcp)){
	  lastc = pico_get_cur_color();
	  (void)pico_set_colorp(Pmaster->colors->tbcp, PSC_NONE);
	}
	else
	  (*term.t_rev)(1);   

	while (ttcol < term.t_ncol)
	  if(*bufp != '\0')
	    pputc(*bufp++, 1);
          else
	    pputc(' ', 1);

	if (lastc){
	  (void)pico_set_colorp(lastc, PSC_NONE);
	  free_color_pair(&lastc);
	}
	else
	  (*term.t_rev)(0);

	movecursor(COMPOSER_TITLE_LINE + 1, 0);
	peeol();
    }
}



/*
 * zotdisplay - blast malloc'd space created for display maps
 */
void
zotdisplay(void)
{
    register int i;

    for (i = 0; i <= term.t_nrow; ++i){		/* free screens */
	free((char *) vscreen[i]);
	free((char *) pscreen[i]);
    }

    free((char *) vscreen);
    free((char *) pscreen);
}



/*
 * nlforw() - returns the number of lines from the top to the dot
 */
int
nlforw(void)
{
    register int  i = 0;
    register LINE *lp = curwp->w_linep;
    
    while(lp != curwp->w_dotp){
	lp = lforw(lp);
	i++;
    }
    return(i);
}



/*
 * pputc - output the given char, keep track of it on the physical screen
 *	   array, and keep track of the cursor
 */
void
pputc(UCS c,				/* char to write */
      int a)				/* and its attribute */
{
    int ind, width, printable_ascii = 0;

    /*
     * This is necessary but not sufficient to allow us to draw. Note that
     * ttrow runs from 0 to t_nrow (so total number of rows is t_nrow+1)
     * ttcol runs from 0 to t_ncol-1 (so total number of cols is t_ncol)
     */
    if((ttcol >= 0 && ttcol < term.t_ncol) && (ttrow >= 0 && ttrow <= term.t_nrow)){

	/*
	 * Width is the number of screen columns a character will occupy.
	 */
	if(c < 0x80 && isprint(c)){
	    printable_ascii++;
	    width = 1;
	}
	else
	  width = wcellwidth(c);

	if(width < 0)
	  width = 1;		/* will be a '?' */

	if(ttcol + width <= term.t_ncol){	/* it fits */
	    /*
	     * Some terminals scroll when you write in the lower right corner
	     * of the screen, so don't write there.
	     */
	    if(!(ttrow == term.t_nrow && ttcol+width == term.t_ncol)){
		(*term.t_putchar)(c);			/* write it */
		ind = index_from_col(ttrow, ttcol);
		pscreen[ttrow]->v_text[ind].c = c;	/* keep track of it */
		pscreen[ttrow]->v_text[ind].a = a;	/* keep track of it */
	    }
	}
	else{
	    /*
	     * Character overlaps right edge of screen. Hopefully the higher
	     * layers will prevent this but we're making sure.
	     *
	     * We may want to do something like writing a space character
	     * into the cells that are on the screen. We'll see.
	     */
	}

	ttcol = MIN(term.t_ncol, ttcol+width);
    }
}


/*
 * pputs - print a string and keep track of the cursor
 */
void
pputs(UCS *s,				/* string to write */
      int a)				/* and its attribute */
{
    while (*s != '\0')
      pputc(*s++, a);
}


void
pputs_utf8(char *s, int a)
{
    UCS *ucsstr = NULL;

    if(s && *s){
	ucsstr = utf8_to_ucs4_cpystr(s);
	if(ucsstr){
	    pputs(ucsstr, a);
	    fs_give((void **) &ucsstr);
	}
    }
}


/*
 * peeol - physical screen array erase to end of the line.  remember to
 *	   track the cursor.
 */
void
peeol(void)
{
    int  i, width = 0, ww;
    CELL cl;

    if(ttrow < 0 || ttrow > term.t_nrow)
      return;

    cl.c = ' ';
    cl.a = 0;

    /*
     * Don't clear if we think we are sitting past the last column,
     * that erases the last column if we just wrote it.
     */
    if(ttcol < term.t_ncol)
      (*term.t_eeol)();

    /*
     * Because the characters are variable width it's a little tricky
     * to erase the rest of the line. What we do is add up the
     * widths of the characters until we reach ttcol
     * then set the rest to the space character.
     */
    for(i = 0; i < term.t_ncol && width < ttcol; i++){
	ww = wcellwidth((UCS) pscreen[ttrow]->v_text[i].c);
	width += (ww >= 0 ? ww : 1);
    }

    while(i < term.t_ncol)
      pscreen[ttrow]->v_text[i++] = cl;
}


/*
 * pscr - return the character cell on the physical screen map on the 
 *        given line, l, and offset, o.
 */
CELL *
pscr(int l, int o)
{
    if((l >= 0 && l <= term.t_nrow) && (o >= 0 && o < term.t_ncol))
      return(&(pscreen[l]->v_text[o]));
    else
      return(NULL);
}


/*
 * pclear() - clear the physical screen from row x through row y (inclusive)
 *            row is zero origin, min row = 0 max row = t_nrow
 *            Clear whole screen      -- pclear(0, term.t_nrow)
 *            Clear bottom two rows   -- pclear(term.t_nrow-1, term.t_nrow)
 *            Clear bottom three rows -- pclear(term.t_nrow-2, term.t_nrow)
 */
void
pclear(int x, int y)
{
    register int i;

    x = MIN(MAX(0, x), term.t_nrow);
    y = MIN(MAX(0, y), term.t_nrow);

    for(i=x; i <= y; i++){
	movecursor(i, 0);
	peeol();
    }
}


/*
 * dumbroot - just get close 
 */
int
dumbroot(int x, int b)
{
    if(x < b)
      return(1);
    else
      return(dumbroot(x/b, b) + 1);
}


/*
 * dumblroot - just get close 
 */
int
dumblroot(long x, int b)
{
    if(x < b)
      return(1);
    else
      return(dumblroot(x/b, b) + 1);
}


/*
 * pinsertc - use optimized insert, fixing physical screen map.
 *            returns true if char written, false otherwise
 */
int
pinsert(CELL c)
{
    int   i, ind = 0, ww;
    CELL *p;

    if(ttrow < 0 || ttrow > term.t_nrow)
      return(0);

    if(o_insert((UCS) c.c)){		/* if we've got it, use it! */
	p = pscreen[ttrow]->v_text;	/* then clean up physical screen */

	ind = index_from_col(ttrow, ttcol);

	for(i = term.t_ncol-1; i > ind; i--)
	  p[i] = p[i-1];		/* shift right */

	p[ind] = c;			/* insert new char */

	ww = wcellwidth((UCS) c.c);
	ttcol += (ww >= 0 ? ww : 1);
	
	return(1);
    }

    return(0);
}


/*
 * pdel - use optimized delete to rub out the current char and
 *        fix the physical screen array.
 *        returns true if optimized the delete, false otherwise
 */
int
pdel(void)
{
    int   i, ind = 0, w;
    CELL *p;

    if(ttrow < 0 || ttrow > term.t_nrow)
      return(0);

    if(TERM_DELCHAR){			/* if we've got it, use it! */
	p = pscreen[ttrow]->v_text;
	ind = index_from_col(ttrow, ttcol);

	if(ind > 0){
	    --ind;
	    w = wcellwidth((UCS) p[ind].c);
	    w = (w >= 0 ? w : 1);
	    ttcol -= w;

	    for(i = 0; i < w; i++){
		(*term.t_putchar)('\b'); 	/* move left a char */
		o_delete();			/* and delete it */
	    }

	    /* then clean up physical screen */
	    for(i=ind; i < term.t_ncol-1; i++)
	      p[i] = p[i+1];

	    p[i].c = ' ';
	    p[i].a = 0;
	}
	
	return(1);
    }

    return(0);
}



/*
 * wstripe - write out the given string at the given location, and reverse
 *           video on flagged characters.  Does the same thing as pine's
 *           stripe.
 *
 * I believe this needs to be fixed to work with non-ascii utf8pmt, but maybe
 * only if you want to put the tildes before multi-byte chars.
 */
void
wstripe(int line, int column, char *utf8pmt, int key)
{
    UCS  *ucs4pmt, *u;
    int  i = 0, col = 0;
    int  j = 0;
    int  l, ww;
    COLOR_PAIR *lastc = NULL;
    COLOR_PAIR *kncp = NULL;
    COLOR_PAIR *klcp = NULL;

    if(line < 0 || line > term.t_nrow)
      return;

    if (Pmaster && Pmaster->colors){
      if(pico_is_good_colorpair(Pmaster->colors->klcp))
        klcp = Pmaster->colors->klcp;

      if(klcp && pico_is_good_colorpair(Pmaster->colors->kncp))
        kncp = Pmaster->colors->kncp;

      lastc = pico_get_cur_color();
    }

    ucs4pmt = utf8_to_ucs4_cpystr(utf8pmt);
    l = ucs4_strlen(ucs4pmt);
    while(1){
	if(i >= term.t_ncol || col >= term.t_ncol || j >= l)
	  return;				/* equal strings */

	if(ucs4pmt[j] == (UCS) key)
	  j++;

	if (pscr(line, i) == NULL)
	  return;
	
	if(pscr(line, i)->c != ucs4pmt[j]){
	    if(j >= 1 && ucs4pmt[j-1] == (UCS) key)
 	      j--;
	    break;
	}

	ww = wcellwidth((UCS) pscr(line, i)->c);
	col += (ww >= 0 ? ww : 1);
	j++;
	i++;
    }

    movecursor(line, column+col);
    if(klcp) (void)pico_set_colorp(klcp, PSC_NONE);
    u = &ucs4pmt[j];
    do{
	if(*u == (UCS) key){
	    u++;
	    if(kncp)
	      (void)pico_set_colorp(kncp, PSC_NONE);
	    else
	      (void)(*term.t_rev)(1);

	    pputc(*u, 1);
	    if(kncp)
	      (void)pico_set_colorp(klcp, PSC_NONE);
	    else
	      (void)(*term.t_rev)(0);
	}
	else{
	    pputc(*u, 0);
	}
    }    
    while(*++u != '\0');

    if(ucs4pmt)
      fs_give((void **) &ucs4pmt);

    peeol();
    if (lastc){
      (void)pico_set_colorp(lastc, PSC_NONE);
      free_color_pair(&lastc);
    }
    (*term.t_flush)();
}



/*
 *  wkeyhelp - paint list of possible commands on the bottom
 *             of the display (yet another pine clone)
 *  NOTE: function key mode is handled here since all the labels
 *        are the same...
 *
 *    The KEYMENU definitions have names and labels defined as UTF-8 strings,
 *    and wstripe expects UTF-8.
 */
void
wkeyhelp(KEYMENU *keymenu)
{
    char *obufp, *p, fkey[4];
    char  linebuf[2*NLINE];	/* "2" is for space for invert tokens */
    int   row, slot, tspace, adjusted_tspace, nspace[6], index, n;
#ifdef	MOUSE
    char  nbuf[NLINE];
#endif

#ifdef _WINDOWS
    pico_config_menu_items (keymenu);
#endif

    if(term.t_mrow == 0)
      return;

    if(term.t_nrow < 1)
      return;

    /*
     * Calculate amount of space for the names column by column...
     */
    for(index = 0; index < 6; index++)
      if(!(gmode&MDFKEY)){
	  nspace[index] = (keymenu[index].name)
			    ? utf8_width(keymenu[index].name) : 0;
	  if(keymenu[index+6].name 
	     && (n = utf8_width(keymenu[index+6].name)) > nspace[index])
	    nspace[index] = n;

	  nspace[index]++;
      }
      else
	nspace[index] = (index < 4) ? 3 : 4;

    tspace = term.t_ncol/6;		/* total space for each item */

    /*
     * Avoid writing in bottom right corner so we won't scroll screens that
     * scroll when you do that. The way this is setup, we won't do that
     * unless the number of columns is evenly divisible by 6.
     */
    adjusted_tspace = (6 * tspace == term.t_ncol) ? tspace - 1 : tspace;

    index  = 0;
    for(row = 0; row <= 1; row++){
	linebuf[0] = '\0';
	obufp = &linebuf[0];
	for(slot = 0; slot < 6; slot++){
	    if(keymenu[index].name && keymenu[index].label){
		size_t l;
		char this_label[200], tmp_label[200];

		if(keymenu[index].label[0] == '[' && keymenu[index].label[(l=strlen(keymenu[index].label))-1] == ']' && l > 2){
		    strncpy(tmp_label, &keymenu[index].label[1], MIN(sizeof(tmp_label),l-2));
		    tmp_label[MIN(sizeof(tmp_label)-1,l-2)] = '\0';
		    snprintf(this_label, sizeof(this_label), "[%s]", _(tmp_label));
	        }
		else
		  strncpy(this_label, _(keymenu[index].label), sizeof(this_label));

		this_label[sizeof(this_label)-1] = '\0';

		if(gmode&MDFKEY){
		    p = fkey;
		    snprintf(fkey, sizeof(fkey), "F%d", (2 * slot) + row + 1);
		}
		else
		  p = keymenu[index].name;
#ifdef	MOUSE
		snprintf(nbuf, sizeof(nbuf), "%.*s %s", nspace[slot], p, this_label);
		register_key(index,
			     (gmode&MDFKEY) ? F1 + (2 * slot) + row:
			     (keymenu[index].name[0] == '^')
			       ? (CTRL | keymenu[index].name[1])
			       : (keymenu[index].name[0] == 'S'
				  && !strcmp(keymenu[index].name, "Spc"))
				   ? ' '
				   : keymenu[index].name[0],
			     nbuf, invert_label,
			     term.t_nrow - 1 + row, (slot * tspace),
			     strlen(nbuf),
			     (Pmaster && Pmaster->colors) 
			       ? Pmaster->colors->kncp: NULL,
			     (Pmaster && Pmaster->colors) 
			       ? Pmaster->colors->klcp: NULL);
#endif

		n = nspace[slot];
		while(p && *p && n--){
		    *obufp++ = '~';	/* insert "invert" token */
		    *obufp++ = *p++;
		}

		while(n-- > 0)
		  *obufp++ = ' ';

		p = this_label;
		n = ((slot == 5 && row == 1) ? adjusted_tspace
					     : tspace) - nspace[slot];
		while(p && *p && n-- > 0)
		  *obufp++ = *p++;

		while(n-- > 0)
		  *obufp++ = ' ';
	    }
	    else{
		n = (slot == 5 && row == 1) ? adjusted_tspace : tspace;
		while(n--)
		  *obufp++ = ' ';

#ifdef	MOUSE
		register_key(index, NODATA, "", NULL, 0, 0, 0, NULL, NULL);
#endif
	    }

	    *obufp = '\0';
	    index++;
	}

	wstripe(term.t_nrow - 1 + row, 0, linebuf, '~');
    }
}


/*
 * This returns the screen width between pstart (inclusive) and
 * pend (exclusive) where the pointers point into an array of CELLs.
 */
unsigned
cellwidth_ptr_to_ptr(CELL *pstart, CELL *pend)
{
    CELL *p;
    unsigned width = 0;
    int ww;

    if(pstart)
      for(p = pstart; p < pend; p++){
	  ww = wcellwidth((UCS) p->c);
	  width += (ww >= 0 ? ww : 1);
      }

    return(width);
}


/*
 * This returns the virtual screen width in row from index a to b (exclusive).
 */
unsigned
vcellwidth_a_to_b(int row, int a, int b)
{
    CELL *pstart, *pend;
    VIDEO *vp;
  
    if(row < 0 || row > term.t_nrow)
      return 0;

    if(a >= b)
      return 0;

    a = MIN(MAX(0, a), term.t_ncol-1);
    b = MIN(MAX(0, a), term.t_ncol);	/* b is past where we stop */

    vp = vscreen[row];
    pstart = &vp->v_text[a];
    pend   = &vp->v_text[b];

    return(cellwidth_ptr_to_ptr(pstart, pend));
}


/*
 * This returns the physical screen width in row from index a to b (exclusive).
 */
unsigned
pcellwidth_a_to_b(int row, int a, int b)
{
    CELL *pstart, *pend;
    VIDEO *vp;
  
    if(row < 0 || row > term.t_nrow)
      return 0;

    if(a >= b)
      return 0;

    a = MIN(MAX(0, a), term.t_ncol-1);
    b = MIN(MAX(0, a), term.t_ncol);	/* b is past where we stop */

    vp = pscreen[row];
    pstart = &vp->v_text[a];
    pend   = &vp->v_text[b];

    return(cellwidth_ptr_to_ptr(pstart, pend));
}


int
index_from_col(int row, int col)
{
    CELL *p_start, *p_end, *p_limit;
    int   w_consumed = 0, w, done = 0;

    if(row < 0 || row > term.t_nrow)
      return 0;

    p_end = p_start = pscreen[row]->v_text;
    p_limit = p_start + term.t_ncol;

    if(p_start)
      while(!done && p_end < p_limit && p_end->c && w_consumed <= col){
	w = wcellwidth((UCS) p_end->c);
	w = (w >= 0 ? w : 1);
	if(w_consumed + w <= col){
	    w_consumed += w;
	    ++p_end;
	}
	else
	  ++done;
      }

    /* MIN and MAX just to be sure */
    return(MIN(MAX(0, p_end - p_start), term.t_ncol-1));
}

#ifdef _WINDOWS

void
pico_config_menu_items (KEYMENU *keymenu)
{
    int		i;
    KEYMENU	*k;
    UCS		key;

    mswin_menuitemclear ();

    /* keymenu's seem to be hardcoded at 12 entries. */
    for (i = 0, k = keymenu; i < 12; ++i, ++k) {
	if (k->name != NULL && k->label != NULL && 
		k->menuitem != KS_NONE) {

	    if (k->name[0] == '^')
		key = CTRL | k->name[1];
	    else if (strcmp(k->name, "Ret") == 0) 
		key = '\r';
	    else
		key = k->name[0];

	    mswin_menuitemadd (key, k->label, k->menuitem, 0);
	}
    }
}

/*
 * Update the scroll range and position. (exported)
 *
 * This is where curbp->b_linecnt is really managed.  With out this function
 * to count the number of lines when needed curbp->b_linecnt will never
 * really be correct.  BUT, this function is only compiled into the 
 * windows version, so b_linecnt will only ever be right in the windows
 * version.  OK for now because that is the only version that
 * looks at b_linecnt.
 */
int
update_scroll (void)
{
    long	scr_pos;
    long	scr_range;
    LINE	*lp;
    static LINE *last_top_line = NULL;
    static long last_scroll_pos = -1;
    
    
    if (ComposerEditing) {
	/* Editing header - don't allow scroll bars. */
	mswin_setscrollrange (0, 0);
	return(0);
    }
	   
	
    /*
     * Count the number of lines in the current bufer.  Done when:
     *
     *      when told to recount:           curbp->b_linecnt == -1
     *      when the top line changed:      curwp->w_linep != last_top_line
     *  when we don't know the scroll pos:  last_scroll_pos == -1
     *
     * The first line in the list is a "place holder" line and is not
     * counted.  The list is circular, when we return the to place
     * holder we have reached the end.
     */
    if(curbp->b_linecnt == -1 || curwp->w_linep != last_top_line
       || last_scroll_pos == -1) {
	scr_range = 0;
	scr_pos = 0;
	for (lp = lforw (curbp->b_linep); lp != curbp->b_linep; 
	     lp = lforw (lp)) {
	    if (lp == curwp->w_linep)
              scr_pos = scr_range;

	    ++scr_range;
	}

	curbp->b_linecnt = scr_range;
	last_scroll_pos = scr_pos;
	last_top_line = curwp->w_linep;
    }

    /*
     * Set new scroll range and position.
     */
    mswin_setscrollrange (curwp->w_ntrows - 2, curbp->b_linecnt - 1);
    mswin_setscrollpos (last_scroll_pos);
    return (0);
}
#endif /* _WINDOWS */
