#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: composer.c 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $";
#endif

/*
 * ========================================================================
 * Copyright 2006-2009 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 *
 * Program:	Pine composer routines
 *
 * NOTES:
 *
 *  - composer.c is the composer for the PINE mail system
 *
 *  - tabled 01/19/90
 *
 *  Notes: These routines aren't incorporated yet, because the composer as
 *         a whole still needs development.  These are ideas that should
 *         be implemented in later releases of PINE.  See the notes in 
 *         pico.c concerning what needs to be done ....
 *
 *  - untabled 01/30/90
 *
 *  Notes: Installed header line editing, with wrapping and unwrapping, 
 *         context sensitive help, and other mail header editing features.
 *
 *  - finalish code cleanup 07/15/91
 * 
 *  Notes: Killed/yanked header lines use emacs kill buffer.
 *         Arbitrarily large headers are handled gracefully.
 *         All formatting handled by FormatLines.
 *
 *  - Work done to optimize display painting 06/26/92
 *         Not as clean as it should be, needs more thought 
 *
 */
#include "headers.h"

#include "osdep/terminal.h"
#include "../pith/string.h"

int              InitEntryText(char *, struct headerentry *);
int              HeaderOffset(int);
int              HeaderFocus(int, int);
UCS              LineEdit(int, UCS *);
int              header_downline(int, int);
int              header_upline(int);
int              FormatLines(struct hdr_line *, char *, int, int, int);
int              FormatSyncAttach(void);
UCS             *ucs4_strqchr(UCS *, UCS, int *, int);
int              ComposerHelp(int);
void             NewTop(int);
void             display_delimiter(int);
void             zotentry(struct hdr_line *);
int              InvertPrompt(int, int);
int              partial_entries(void);
int              physical_line(struct hdr_line *);
int              strend(UCS *, UCS);
int              KillHeaderLine(struct hdr_line *, int);
int              SaveHeaderLines(void);
UCS             *break_point(UCS *, int, UCS, int *);
int              hldelete(struct hdr_line *);
int              is_blank(int, int, int);
int              zotcomma(UCS *);
struct hdr_line *first_hline(int *);
struct hdr_line *first_sel_hline(int *);
struct hdr_line *next_hline(int *, struct hdr_line *);
struct hdr_line *next_sel_hline(int *, struct hdr_line *);
struct hdr_line *prev_hline(int *, struct hdr_line *);
struct hdr_line *prev_sel_hline(int *, struct hdr_line *);
struct hdr_line *first_requested_hline(int *);
void             fix_mangle_and_err(int *, char **, char *);
void             mark_sticky(struct headerentry *);


/*
 * definition header field array, structures defined in pico.h
 */
struct headerentry *headents;


/*
 * structure that keeps track of the range of header lines that are
 * to be displayed and other fun stuff about the header
 */
struct on_display ods;				/* global on_display struct */


/*
 * useful macros
 */
#define	HALLOC()	(struct hdr_line *)malloc(sizeof(struct hdr_line))
#define	LINEWID()	(((term.t_ncol - headents[ods.cur_e].prwid) > 0) ? ((unsigned) (term.t_ncol - headents[ods.cur_e].prwid)) : 0)
#define	BOTTOM()	(term.t_nrow - term.t_mrow)
#define	FULL_SCR()	(BOTTOM() - 3)
#define	HALF_SCR()	(FULL_SCR()/2)

#ifdef	MOUSE
/*
 * Redefine HeaderEditor to install wrapper required for mouse event
 * handling...
 */
#define	HeaderEditor	HeaderEditorWork
#endif /* MOUSE */

#define	HDR_DELIM	"----- Message Text -----"

/*
 * useful declarations
 */
static short delim_ps  = 0;		/* previous state */
static short invert_ps = 0;		/* previous state */


static KEYMENU menu_header[] = {
    /* TRANSLATORS: command key labels, Send means send the message
       we are currently working on. */
    {"^G", N_("Get Help"), KS_SCREENHELP},	{"^X", N_("Send"), KS_SEND},
    /* TRANSLATORS: Rich Headers is a command to display more headers. It
       is triggered with the ^R key. PrvPg stands for Previous Page. */
    {"^R", N_("Rich Hdr"), KS_RICHHDR},	{"^Y", N_("PrvPg/Top"), KS_PREVPAGE},
    /* TRANSLATORS: Cut Line means remove a line. Postpone means to save
       a message being composed so that it can be worked on later. */
    {"^K", N_("Cut Line"), KS_CURPOSITION},	{"^O", N_("Postpone"), KS_POSTPONE},
    /* TRANSLATORS: Del Char is Delete Character */
    {"^C", N_("Cancel"), KS_CANCEL},	{"^D", N_("Del Char"), KS_NONE},
    /* TRANSLATORS: Next Page */
    {"^J", N_("Attach"), KS_ATTACH},	{"^V", N_("NxtPg/End"), KS_NEXTPAGE},
    /* TRANSLATORS: Undelete a line that was just deleted */
    {"^U", N_("UnDel Line"), KS_NONE},	{NULL, NULL}
};
#define	SEND_KEY	1
#define	RICH_KEY	2
#define	CUT_KEY		4
#define	PONE_KEY	5
#define	DEL_KEY		7
#define	ATT_KEY		8
#define	UDEL_KEY	10
#define	TO_KEY		11


/*
 * function key mappings for header editor
 */
static UCS ckm[12][2] = {
    { F1,  (CTRL|'G')},
    { F2,  (CTRL|'C')},
    { F3,  (CTRL|'X')},
    { F4,  (CTRL|'D')},
    { F5,  (CTRL|'R')},
    { F6,  (CTRL|'J')},
    { F7,  0 },
    { F8,  0 },
    { F9,  (CTRL|'K')},
    { F10, (CTRL|'U')},
    { F11, (CTRL|'O')},
    { F12, (CTRL|'T')}
};


/*
 * InitMailHeader - initialize header array, and set beginning editor row 
 *                  range.  The header entry structure should look just like 
 *                  what is written on the screen, the vector 
 *                  (entry, line, offset) will describe the current cursor 
 *                  position in the header.
 *
 *	   Returns: TRUE if special header handling was requested,
 *		    FALSE under standard default behavior.
 */
int
InitMailHeader(PICO *mp)
{
    char	       *addrbuf;
    struct headerentry *he;
    int			rv;

    if(!mp->headents){
	headents = NULL;
	return(FALSE);
    }

    /*
     * initialize some of on_display structure, others below...
     */
    ods.p_ind  = 0;
    ods.p_line = COMPOSER_TOP_LINE;
    ods.top_l = ods.cur_l = NULL;

    headents = mp->headents;
    /*--- initialize the fields in the headerent structure ----*/
    for(he = headents; he->name != NULL; he++){
	he->hd_text    = NULL;
	he->display_it = he->display_it ? he->display_it : !he->rich_header;
        if(he->is_attach) {
            /*--- A lot of work to do since attachments are special ---*/
            he->maxlen = 0;
	    if(mp->attachments != NULL){
		char   buf[NLINE];
                int    attno = 0;
		int    l1, ofp, ofp1, ofp2;  /* OverFlowProtection */
		size_t addrbuflen = 4 * NLINE; /* malloc()ed size of addrbuf */
                PATMT *ap = mp->attachments;
 
		ofp = NLINE - 35;

                addrbuf = (char *)malloc(addrbuflen);
                addrbuf[0] = '\0';
                buf[0] = '\0';
                while(ap){
		  if((l1 = strlen(ap->filename)) <= ofp){
		      ofp1 = l1;
		      ofp2 = ofp - l1;
		  }
		  else{
		      ofp1 = ofp;
		      ofp2 = 0;
		  }

		  if(ap->filename){
                     snprintf(buf, sizeof(buf), "%d. %.*s%s %s%s%s\"",
                             ++attno,
			     ofp1,
                             ap->filename,
			     (l1 > ofp) ? "...]" : "",
                             ap->size ? "(" : "",
                             ap->size ? ap->size : "",
                             ap->size ? ") " : "");

		     /* append description, escaping quotes */
		     if(ap->description){
			 char *dp = ap->description, *bufp = &buf[strlen(buf)];
			 int   escaped = 0;

			 do
			   if(*dp == '\"' && !escaped){
			       *bufp++ = '\\';
			       ofp2--;
			   }
			   else if(escaped){
			       *bufp++ = '\\';
			       escaped = 0;
			   }
			 while(--ofp2 > 0 && (*bufp++ = *dp++));

			 *bufp = '\0';
		     }

		     snprintf(buf + strlen(buf), sizeof(buf)-strlen(buf), "\"%s", ap->next ? "," : "");

		     if(strlen(addrbuf) + strlen(buf) >= addrbuflen){
			 addrbuflen += NLINE * 4;
			 if(!(addrbuf = (char *)realloc(addrbuf, addrbuflen))){
			     emlwrite("\007Can't realloc addrbuf to %d bytes",
				      (void *) addrbuflen);
			     return(ABORT);
			 }
		     }

                     strncat(addrbuf, buf, addrbuflen-strlen(addrbuf)-1);
		     addrbuf[addrbuflen-1] = '\0';
                 }
                 ap = ap->next;
                }
                InitEntryText(addrbuf, he);
                free((char *)addrbuf);
            } else {
                InitEntryText("", he);
            }
            he->realaddr = NULL;
        } else {
	    if(!he->blank)
              addrbuf = *(he->realaddr);
	    else
              addrbuf = "";

            InitEntryText(addrbuf, he);
	}
    }

    /*
     * finish initialization and then figure out display layout.
     * first, look for any field the caller requested we start in,
     * and set the offset into that field.
     */
    if((ods.cur_l = first_requested_hline(&ods.cur_e)) != NULL){
	ods.top_e = 0;				/* init top_e */
	ods.top_l = first_hline(&ods.top_e);
	if(!HeaderFocus(ods.cur_e, Pmaster ? Pmaster->edit_offset : 0))
	  HeaderFocus(ods.cur_e, 0);

	rv = TRUE;
    }
    else{
	ods.top_l = first_hline(&ods.cur_e);
	ods.cur_l = first_sel_hline(&ods.cur_e);
	ods.top_e = ods.cur_e;
	rv = 0;
    }

    UpdateHeader(0);
    return(rv);
}



/*
 * InitEntryText - Add the given header text into the header entry 
 *		   line structure.
 */
int
InitEntryText(char *utf8text, struct headerentry *e)
{
    struct  hdr_line	*curline;
    register  int	longest;

    /*
     * get first chunk of memory, and tie it to structure...
     */
    if((curline = HALLOC()) == NULL){
        emlwrite("Unable to make room for full Header.", NULL);
        return(FALSE);
    }

    longest = term.t_ncol - e->prwid - 1;
    curline->text[0] = '\0';
    curline->next = NULL;
    curline->prev = NULL;
    e->hd_text = curline;		/* tie it into the list */

    if(FormatLines(curline, utf8text, longest, e->break_on_comma, 0) == -1)
      return(FALSE);
    else
      return(TRUE);
}



/*
 *  ResizeHeader - Handle resizing display when SIGWINCH received.
 *
 *	notes:
 *		works OK, but needs thorough testing
 *		  
 */
int
ResizeHeader(void)
{
    register struct headerentry *i;
    register int offset;

    if(!headents)
      return(TRUE);

    offset = (ComposerEditing) ? HeaderOffset(ods.cur_e) : 0;

    for(i=headents; i->name; i++){		/* format each entry */
	if(FormatLines(i->hd_text, "", (term.t_ncol - i->prwid),
		       i->break_on_comma, 0) == -1){
	    return(-1);
	}
    }

    if(ComposerEditing)				/* restart at the top */
      HeaderFocus(ods.cur_e, offset);		/* fix cur_l and p_ind */
    else {
      struct hdr_line *l;
      int              e;

      for(i = headents; i->name != NULL; i++);	/* Find last line */
      i--;
      e = i - headents;
      l = headents[e].hd_text;
      if(!headents[e].display_it || headents[e].blank)
        l = prev_sel_hline(&e, l);		/* last selectable line */

      if(!l){
	  e = i - headents;
	  l = headents[e].hd_text;
      }

      HeaderFocus(e, -1);		/* put focus on last line */
    }

    if(ComposerTopLine != COMPOSER_TOP_LINE)
      UpdateHeader(0);

    PaintBody(0);

    if(ComposerEditing)
      movecursor(ods.p_line, ods.p_ind+headents[ods.cur_e].prwid);

    (*term.t_flush)();
    return(TRUE);
}



/*
 * HeaderOffset - return the character offset into the given header
 */
int
HeaderOffset(int h)
{
    register struct hdr_line *l;
    int	     i = 0;

    l = headents[h].hd_text;

    while(l != ods.cur_l){
	i += ucs4_strlen(l->text);
	l = l->next;
    }
    return(i+ods.p_ind);
}



/*
 * HeaderFocus - put the dot at the given offset into the given header
 */
int
HeaderFocus(int h, int offset)
{
    register struct hdr_line *l;
    register int    i;
    int	     last = 0;

    if(offset == -1)				/* focus on last line */
      last = 1;

    l = headents[h].hd_text;
    while(1){
	if(last && l->next == NULL){
	    break;
	}
	else{
	    if((i=ucs4_strlen(l->text)) >= offset)
	      break;
	    else
	      offset -= i;
	}
	if((l = l->next) == NULL)
	  return(FALSE);
    }

    ods.cur_l = l;
    ods.p_len = ucs4_strlen(l->text);
    ods.p_ind = (last) ? 0 : offset;

    return(TRUE);
}



/*
 * HeaderEditor() - edit the mail header field by field, trapping 
 *                  important key sequences, hand the hard work off
 *                  to LineEdit().  
 *	returns:
 *              -3    if we drop out bottom *and* want to process mouse event
 *		-1    if we drop out the bottom 
 *		FALSE if editing is cancelled
 *		TRUE  if editing is finished
 */
int
HeaderEditor(int f, int n)
{
    register  int	i;
    UCS                 ch = 0, lastch;
    register  char	*bufp;
    struct headerentry *h;
    int                 cur_e, mangled, retval = -1,
		        hdr_only = (gmode & MDHDRONLY) ? 1 : 0;
    char               *errmss, *err;
#ifdef MOUSE
    MOUSEPRESS		mp;
#endif

    if(!headents)
      return(TRUE);				/* nothing to edit! */

    ComposerEditing = TRUE;
    display_delimiter(0);			/* provide feedback */

#ifdef	_WINDOWS
    mswin_setscrollrange (0, 0);
#endif /* _WINDOWS */

    /* 
     * Decide where to begin editing.  if f == TRUE begin editing
     * at the bottom.  this case results from the cursor backing
     * into the editor from the bottom.  otherwise, the user explicitly
     * requested editing the header, and we begin at the top.
     * 
     * further, if f == 1, we moved into the header by hitting the up arrow
     * in the message text, else if f == 2, we moved into the header by
     * moving past the left edge of the top line in the message.  so, make 
     * the end of the last line of the last entry the current cursor position
     * lastly, if f == 3, we moved into the header by hitting backpage() on
     * the top line of the message, so scroll a page back.  
     */
    if(f){
	if(f == 2){				/* 2 leaves cursor at end  */
	    struct hdr_line *l = ods.cur_l;
	    int              e = ods.cur_e;

	    /*--- make sure on last field ---*/
	    while((l = next_sel_hline(&e, l)) != NULL)
	      if(headents[ods.cur_e].display_it){
		  ods.cur_l = l;
		  ods.cur_e = e;
	      }

	    ods.p_ind = 1000;			/* and make sure at EOL    */
	}
	else{
	    /*
	     * note: assumes that ods.cur_e and ods.cur_l haven't changed
	     *       since we left...
	     */

	    /* fix postition */
	    if(curwp->w_doto < headents[ods.cur_e].prwid)
	      ods.p_ind = 0;
	    else if(curwp->w_doto < ods.p_ind + headents[ods.cur_e].prwid)
	      ods.p_ind = curwp->w_doto - headents[ods.cur_e].prwid;
	    else
	      ods.p_ind = 1000;

	    /* and scroll back if needed */
	    if(f == 3)
	      for(i = 0; header_upline(0) && i <= FULL_SCR(); i++)
		;
	}

	ods.p_line = ComposerTopLine - 2;
    }
    /* else just trust what ods contains */

    InvertPrompt(ods.cur_e, TRUE);		/* highlight header field */
    sgarbk = 1;

    do{
	lastch = ch;
	if(km_popped){
	    km_popped--;
	    if(km_popped == 0)
	      sgarbk = 1;
	}

	if(sgarbk){
	    if(km_popped){  /* temporarily change to cause menu to paint */
		term.t_mrow = 2;
		curwp->w_ntrows -= 2;
		movecursor(term.t_nrow-2, 0); /* clear status line, too */
		peeol();
	    }
	    else if(term.t_mrow == 0)
	      PaintBody(1);

	    ShowPrompt();			/* display correct options */
	    sgarbk = 0;
	    if(km_popped){
		term.t_mrow = 0;
		curwp->w_ntrows += 2;
	    }
	}

	ch = LineEdit(!(gmode&MDVIEW), &lastch);	/* work on the current line */

	if(km_popped)
	  switch(ch){
	    case NODATA:
	    case (CTRL|'L'):
	      km_popped++;
	      break;
	    
	    default:
	      movecursor(term.t_nrow-2, 0);
	      peeol();
	      movecursor(term.t_nrow-1, 0);
	      peeol();
	      movecursor(term.t_nrow, 0);
	      peeol();
	      break;
	  }

        switch (ch){
	  case (CTRL|'R') :			/* Toggle header display */
	    if(Pmaster->pine_flags & MDHDRONLY){
		if(Pmaster->expander){
		    packheader();
		    call_expander();
		    PaintBody(0);
		    break;
		}
		else
		  goto bleep;
	    }

            /*---- Are there any headers to expand above us? ---*/
            for(h = headents; h != &headents[ods.cur_e]; h++)
              if(h->rich_header)
                break;
            if(h->rich_header)
	      InvertPrompt(ods.cur_e, FALSE);	/* Yes, don't leave inverted */

	    mangled = 0;
	    err = NULL;
	    if(partial_entries()){
                /*--- Just turned off all rich headers --*/
		if(headents[ods.cur_e].rich_header){
                    /*-- current header got turned off too --*/
#ifdef	ATTACHMENTS
		    if(headents[ods.cur_e].is_attach){
			/* verify the attachments */
			if((i = FormatSyncAttach()) != 0){
			    FormatLines(headents[ods.cur_e].hd_text, "",
					term.t_ncol - headents[ods.cur_e].prwid,
					headents[ods.cur_e].break_on_comma, 0);
			}
		    }
#endif
		    if(headents[ods.cur_e].builder)	/* verify text */
		      i = call_builder(&headents[ods.cur_e], &mangled, &err)>0;
                    /* Check below */
                    for(cur_e =ods.cur_e; headents[cur_e].name!=NULL; cur_e++)
                      if(!headents[cur_e].rich_header)
                        break;
                    if(headents[cur_e].name == NULL) {
                        /* didn't find one, check above */
                        for(cur_e =ods.cur_e; headents[cur_e].name!=NULL;
                            cur_e--)
                          if(!headents[cur_e].rich_header)
                            break;

                    }
		    ods.p_ind = 0;
		    ods.cur_e = cur_e;
		    ods.cur_l = headents[ods.cur_e].hd_text;
		}
	    }

	    ods.p_line = 0;			/* force update */
	    UpdateHeader(0);
	    PaintHeader(COMPOSER_TOP_LINE, FALSE);
	    PaintBody(1);
	    fix_mangle_and_err(&mangled, &err, headents[ods.cur_e].name);
	    break;

	  case (CTRL|'C') :			/* bag whole thing ?*/
	    if(abort_composer(1, 0) == TRUE)
	      return(FALSE);

	    break;

	  case (CTRL|'X') :			/* Done. Send it. */
	    i = 0;
#ifdef	ATTACHMENTS
	    if(headents[ods.cur_e].is_attach){
		/* verify the attachments, and pretty things up in case
		 * we come back to the composer due to error...
		 */
		if((i = FormatSyncAttach()) != 0){
		    sleep(2);		/* give time for error to absorb */
		    FormatLines(headents[ods.cur_e].hd_text, "",
				term.t_ncol - headents[ods.cur_e].prwid,
				headents[ods.cur_e].break_on_comma, 0);
		}
	    }
	    else
#endif
	    mangled = 0;
	    err = NULL;
	    if(headents[ods.cur_e].builder)	/* verify text? */
	      i = call_builder(&headents[ods.cur_e], &mangled, &err);

	    if(i < 0){			/* don't leave without a valid addr */
		fix_mangle_and_err(&mangled, &err, headents[ods.cur_e].name);
		break;
	    }
	    else if(i > 0){
		ods.cur_l = headents[ods.cur_e].hd_text; /* attach cur_l */
		ods.p_ind = 0;
		ods.p_line = 0;			/* force realignment */
	        fix_mangle_and_err(&mangled, &err, headents[ods.cur_e].name);
		NewTop(0);
	    }

	    fix_mangle_and_err(&mangled, &err, headents[ods.cur_e].name);

	    if(wquit(1,0) == TRUE)
	      return(TRUE);

	    if(i > 0){
		/*
		 * need to be careful here because pointers might be messed up.
		 * also, could do a better job of finding the right place to
		 * put the dot back (i.e., the addr/list that was expanded).
		 */
		UpdateHeader(0);
		PaintHeader(COMPOSER_TOP_LINE, FALSE);
		PaintBody(1);
	    }
	    break;

	  case (CTRL|'Z') :			/* Suspend compose */
	    if(gmode&MDSSPD){			/* is it allowed? */
		bktoshell(0, 1);
		PaintBody(0);
	    }
	    else
	      unknown_command(ch);

	    break;

	  case (CTRL|'\\') :
#if defined MOUSE && !defined(_WINDOWS)
	    toggle_xterm_mouse(0,1);
#else
	    unknown_command(ch);
#endif
	    break;

	  case (CTRL|'O') :			/* Suspend message */
	    if(Pmaster->pine_flags & MDHDRONLY)
	      goto bleep;

	    i = 0;
	    mangled = 0;
	    err = NULL;
	    if(headents[ods.cur_e].is_attach){
		if(FormatSyncAttach() < 0){
		    if(mlyesno_utf8(_("Problem with attachments. Postpone anyway?"),
			       FALSE) != TRUE){
			if(FormatLines(headents[ods.cur_e].hd_text, "",
				       term.t_ncol - headents[ods.cur_e].prwid,
				       headents[ods.cur_e].break_on_comma, 0) == -1)
			  emlwrite("\007Format lines failed!", NULL);
			UpdateHeader(0);
			PaintHeader(COMPOSER_TOP_LINE, FALSE);
			PaintBody(1);
			continue;
		    }
		}
	    }
	    else if(headents[ods.cur_e].builder)
	      i = call_builder(&headents[ods.cur_e], &mangled, &err);

	    fix_mangle_and_err(&mangled, &err, headents[ods.cur_e].name);

	    if(i < 0)			/* don't leave without a valid addr */
	      break;

	    suspend_composer(1, 0);
	    return(TRUE);

#ifdef	ATTACHMENTS
	  case (CTRL|'J') :			/* handle attachments */
	    if(Pmaster->pine_flags & MDHDRONLY)
	      goto bleep;

	    { char    cmt[NLINE];
	      LMLIST *lm = NULL, *lmp;
	      char buf[NLINE], *bfp;
	      int saved_km_popped;
	      size_t len;

	      /*
	       * Attachment questions mess with km_popped and assume
	       * it is zero to start with.  If we don't set it to zero
	       * on entry, the message about mime type will be erased
	       * by PaintBody.  If we don't reset it when we come back,
	       * the bottom three lines may be messed up.
	       */
	      saved_km_popped = km_popped;
	      km_popped = 0;

	      if(AskAttach(cmt, sizeof(cmt), &lm)){

		for(lmp = lm; lmp; lmp = lmp->next){
		    size_t space;

		    len = lmp->dir ? strlen(lmp->dir)+1 : 0;
		    len += lmp->fname ? strlen(lmp->fname) : 0;

		    if(len+3 > sizeof(buf)){
			bfp = malloc(len+3);
			space = len+3;
			if((bfp=malloc(len+3)) == NULL){
			    emlwrite("\007Can't malloc space for filename",
				     NULL);
			    continue;
			}
		    }
		    else{
			bfp = buf;
			space = sizeof(buf);
		    }

		    if(lmp->dir && lmp->dir[0])
		      snprintf(bfp, space, "%s%c%s", lmp->dir, C_FILESEP,
			      lmp->fname ? lmp->fname : "");
		    else
		      snprintf(bfp, space, "%s", lmp->fname ? lmp->fname : "");

		    (void) QuoteAttach(bfp, space);

		    (void) AppendAttachment(bfp, lm->size, cmt);

		    if(bfp != buf)
		      free(bfp);
		}

		zotlmlist(lm);
	      }

	      km_popped = saved_km_popped;
	      sgarbk = 1;			/* clean up prompt */
	    }
	    break;
#endif

	  case (CTRL|'I') :			/* tab */
	    if(headents[ods.cur_e].nickcmpl != NULL){
		char *new_nickname = NULL;
		UCS *strng;
		UCS *start = NULL, *end = NULL, *before_start = NULL;
		UCS *uprefix = NULL, *up1, *up2;
		char *prefix = NULL, *saveprefix = NULL, *insert = NULL;
		char *ambig = NULL;
		int offset, prefixlen, add_a_comma = 0;
		size_t l, l1, l2;
		int ambiguity, fallthru = 1;

		strng = ods.cur_l->text;
		offset = HeaderOffset(ods.cur_e);

		if(ods.p_ind > 0
		   && (start = &strng[ods.p_ind-1])
		   && (!*(start+1)
		       || ucs4_isspace(*(start+1))
		       || *(start+1) == ',')
		   && (*start
		       && !ucs4_isspace(*start)
		       && *start != ',')){

		    end = start+1;

		    while(start > strng
			  && *(start-1)
		          && *(start-1) != ',')
		      start--;

		    while(ucs4_isspace(*start))
		      start++;

		    if(*end != ',' && ods.cur_l->next)
		      add_a_comma++;

		    /*
		     * Nickname prefix begins with start and ends
		     * with end-1. Replace those characters with
		     * completed nickname.
		     */
		    prefixlen = end-start;
		    uprefix = (UCS *) fs_get((prefixlen+1) * sizeof(UCS));
		    ucs4_strncpy(uprefix, start, prefixlen);
		    uprefix[prefixlen] = '\0';
		    prefix = ucs4_to_utf8_cpystr(uprefix);
		    fs_give((void **) &uprefix);

		    /*
		     * Ambiguity == 0 means no matches exist.
		     *              1 means it is ambiguous and the longest
		     *                unambiguous prefix beginning with prefix
		     *                is in new_nickname.
		     *              2 means it is unambiguous and the answer
		     *                is in new_nickname
		     *
		     *  The expansion from == 2 doesn't have to have prefix
		     *  as a prefix. For example, if the prefix is a prefix
		     *  of the full name or a prefix of a nickname the answer
		     *  might be the full address instead of the expanded
		     *  prefix. In fact, that's probably the expected thing.
		     *
		     *  Due to the way the build_abook_tries sets up the tries
		     *  it is unfortunately the case that the expanded address
		     *  is not entered in any trie, so after you get an
		     *  unambiguous expansion if you try TAB again at that
		     *  point you'll probably get a no match returned instead
		     *  of an unambiguous. So be aware of that and handle
		     *  that case ok by falling through to header_downline.
		     */
		    ambiguity = (*(headents[ods.cur_e].nickcmpl))(prefix,
					    &new_nickname, (lastch == ch), 0);

		    /*
		     * Don't fall through on no-match TAB unless
		     * it is the second TAB.
		     */
		    if(ambiguity == 0 && lastch != ch){
			lastch = 0;
			break;
		    }

		    if(ambiguity != 1)
		      lastch = 0;

		    if(ambiguity == 0)
		      goto nomore_to_complete;

		    if(new_nickname){
			if(strcmp(new_nickname, prefix)){
			    /*
			     * We're trying to work with the way
			     * FormatLines works. It inserts text at the
			     * beginning of the line we pass in.
			     * So, remove the beginning of the line and
			     * have FormatLines put it back.
			     */

			    /* save part before start */
			    fallthru = 0;
			    before_start = strng;
			    uprefix = (UCS *) fs_get((start-before_start+1) * sizeof(UCS));
			    ucs4_strncpy(uprefix, before_start, start-before_start);
			    uprefix[start-before_start] = '\0';
			    saveprefix = ucs4_to_utf8_cpystr(uprefix);

			    /* figure out new cursor offset */
			    up1 = utf8_to_ucs4_cpystr(new_nickname);
			    if(up1){
				offset += (ucs4_strlen(up1) - prefixlen);
				fs_give((void **) &up1);
			    }

			    fs_give((void **) &uprefix);

			    /*
			     * Delete everything up to end by
			     * copying characters to start of buffer.
			     */
			    up1 = before_start;
			    up2 = end;
			    for(i = ods.p_len - (end - before_start) + 1; i > 0; i--)
			      *up1++ = *up2++;

			    ods.p_len -= (end - before_start);

			    if(saveprefix){
				l1 = strlen(saveprefix);
				l2 = strlen(new_nickname);
				l = l1 + l2;

				/* add a comma? */
				if(add_a_comma && ambiguity == 2){
				    l++;
				    offset++;
				}
				else
				  add_a_comma = 0;

				insert = (char *) fs_get((l+1) * sizeof(char));

				/*
				 * Insert is the before start stuff plus the
				 * new nickname, and we're going to let
				 * FormatLines put it together for us.
				 */
				if(insert){
				    strncpy(insert, saveprefix, l);
				    strncpy(insert+l1, new_nickname, l-l1);
				    if(add_a_comma)
				      insert[l-1] = ',';

				    insert[l] = '\0';
				}

				fs_give((void **) &saveprefix);
			    }


			    if(insert && FormatLines(ods.cur_l, insert,
					 term.t_ncol - headents[ods.cur_e].prwid,
					 headents[ods.cur_e].break_on_comma,0)==-1){
				emlwrite("\007Format lines failed!", NULL);
			    }

			    if(insert)
			      fs_give((void **) &insert);

			    HeaderFocus(ods.cur_e, offset);
			}
			else{
                            (*term.t_beep)();
			}

			ambig = new_nickname;
			new_nickname = NULL;
		    }

		    if(!ambig && prefix){
			ambig = prefix;
			prefix = NULL;
		    }


		    if(ambiguity == 2 && fallthru){
			if(prefix)
			  fs_give((void **) &prefix);

			if(new_nickname)
			  fs_give((void **) &new_nickname);

			if(ambig)
			  fs_give((void **) &ambig);

			UpdateHeader(0);
			PaintBody(0);
			goto nomore_to_complete;
		    }

		    UpdateHeader(0);
		    PaintBody(0);

		    if(prefix)
		      fs_give((void **) &prefix);

		    if(new_nickname)
		      fs_give((void **) &new_nickname);

		    if(ambig)
		      fs_give((void **) &ambig);
		}
		else{
		    goto nomore_to_complete;
		}

		break;
	    }
	    else{
nomore_to_complete:
		ods.p_ind = 0;			/* fall through... */
	    }

	  case (CTRL|'N') :
	  case KEY_DOWN :
	    header_downline(!hdr_only, hdr_only);
	    break;

	  case (CTRL|'P') :
	  case KEY_UP :
	    header_upline(1);
	    break;

	  case (CTRL|'V') :			/* down a page */
	  case KEY_PGDN :
	    cur_e = ods.cur_e;
	    if(!next_sel_hline(&cur_e, ods.cur_l)){
		header_downline(!hdr_only, hdr_only);
		if(!(gmode & MDHDRONLY))
		  retval = -1;			/* tell caller we fell out */
	    }
	    else{
		int move_down, bot_pline;
		struct hdr_line *new_cur_l, *line, *next_line, *prev_line;

		move_down = BOTTOM() - 2 - ods.p_line;
		if(move_down < 0)
		  move_down = 0;

		/*
		 * Count down move_down lines to find the pointer to the line
		 * that we want to become the current line.
		 */
		new_cur_l = ods.cur_l;
		cur_e = ods.cur_e;
		for(i = 0; i < move_down; i++){
		    next_line = next_hline(&cur_e, new_cur_l);
		    if(!next_line)
		      break;

		    new_cur_l = next_line;
		}

		if(headents[cur_e].blank){
		    next_line = next_sel_hline(&cur_e, new_cur_l);
		    if(!next_line)
		      break;

		    new_cur_l = next_line;
		}

		/*
		 * Now call header_downline until we get down to the
		 * new current line, so that the builders all get called.
		 * New_cur_l will remain valid since we won't be calling
		 * a builder for it during this loop.
		 */
		while(ods.cur_l != new_cur_l && header_downline(0, 0))
		  ;
		
		/*
		 * Count back up, if we're at the bottom, to find the new
		 * top line.
		 */
		cur_e = ods.cur_e;
		if(next_hline(&cur_e, ods.cur_l) == NULL){
		    /*
		     * Cursor stops at bottom of headers, which is where
		     * we are right now.  Put as much of headers on
		     * screen as will fit.  Count up to figure
		     * out which line is top_l and which p_line cursor is on.
		     */
		    cur_e = ods.cur_e;
		    line = ods.cur_l;
		    /* leave delimiter on screen, too */
		    bot_pline = BOTTOM() - 1 - ((gmode & MDHDRONLY) ? 0 : 1);
		    for(i = COMPOSER_TOP_LINE; i < bot_pline; i++){
			prev_line = prev_hline(&cur_e, line);
			if(!prev_line)
			  break;
			
			line = prev_line;
		    }

		    ods.top_l = line;
		    ods.top_e = cur_e;
		    ods.p_line = i;
		      
		}
		else{
		    ods.top_l = ods.cur_l;
		    ods.top_e = ods.cur_e;
		    /*
		     * We don't want to scroll down further than the
		     * delimiter, so check to see if that is the case.
		     * If it is, we move the p_line down the screen
		     * until the bottom line is where we want it.
		     */
		    bot_pline = BOTTOM() - 1 - ((gmode & MDHDRONLY) ? 0 : 1);
		    cur_e = ods.cur_e;
		    line = ods.cur_l;
		    for(i = bot_pline; i > COMPOSER_TOP_LINE; i--){
			next_line = next_hline(&cur_e, line);
			if(!next_line)
			  break;

			line = next_line;
		    }

		    /*
		     * i is the desired value of p_line.
		     * If it is greater than COMPOSER_TOP_LINE, then
		     * we need to adjust top_l.
		     */
		    ods.p_line = i;
		    line = ods.top_l;
		    cur_e = ods.top_e;
		    for(; i > COMPOSER_TOP_LINE; i--){
			prev_line = prev_hline(&cur_e, line);
			if(!prev_line)
			  break;
			
			line = prev_line;
		    }

		    ods.top_l = line;
		    ods.top_e = cur_e;

		    /*
		     * Special case.  If p_line is within one of the bottom,
		     * move it to the bottom.
		     */
		    if(ods.p_line == bot_pline - 1){
			header_downline(0, 0);
			/* but put these back where we want them */
			ods.p_line = bot_pline;
			ods.top_l = line;
			ods.top_e = cur_e;
		    }
		}

		UpdateHeader(0);
		PaintHeader(COMPOSER_TOP_LINE, FALSE);
		PaintBody(1);
	    }

	    break;

	  case (CTRL|'Y') :			/* up a page */
	  case KEY_PGUP :
	    for(i = 0; header_upline(0) && i <= FULL_SCR(); i++)
	      if(i < 0)
		break;

	    break;

#ifdef	MOUSE
	  case KEY_MOUSE:
	    mouse_get_last (NULL, &mp);
	    switch(mp.button){
	      case M_BUTTON_LEFT :
		if (!mp.doubleclick) {
		    if (mp.row < ods.p_line) {
			for (i = ods.p_line - mp.row;
			     i > 0 && header_upline(0); 
			     --i)
			  ;
		    }
		    else {
			for (i = mp.row-ods.p_line;
			     i > 0 && header_downline(!hdr_only, 0);
			     --i)
			  ;
		    }

		    if((ods.p_ind = mp.col - headents[ods.cur_e].prwid) <= 0)
		      ods.p_ind = 0;

		    /* -3 is returned  if we drop out bottom
		     * *and* want to process a mousepress.  The Headereditor
		     * wrapper should make sense of this return code.
		     */
		    if (ods.p_line >= ComposerTopLine)
		      retval = -3;
		}

		break;

	      case M_BUTTON_RIGHT :
#ifdef	_WINDOWS
		pico_popup();
#endif
	      case M_BUTTON_MIDDLE :
	      default :	/* NOOP */
		break;
	    }

	    break;
#endif /* MOUSE */

	  case (CTRL|'T') :			/* Call field selector */
	    errmss = NULL;
            if(headents[ods.cur_e].is_attach) {
                /*--- selector for attachments ----*/
		char dir[NLINE], fn[NLINE], sz[NLINE];
		LMLIST *lm = NULL, *lmp;

		strncpy(dir, (gmode & MDCURDIR)
		       ? (browse_dir[0] ? browse_dir : ".")
		       : ((gmode & MDTREE) || opertree[0])
		       ? opertree
		       : (browse_dir[0] ? browse_dir
			  : gethomedir(NULL)), sizeof(dir));
		dir[sizeof(dir)-1] = '\0';
		fn[0] = sz[0] = '\0';
		if(FileBrowse(dir, sizeof(dir), fn, sizeof(fn), sz, sizeof(sz),
			      FB_READ | FB_ATTACH | FB_LMODEPOS, &lm) == 1){
		    char buf[NLINE], *bfp;
		    size_t len;

		    for(lmp = lm; lmp; lmp = lmp->next){
			size_t space;

			len = lmp->dir ? strlen(lmp->dir)+1 : 0;
			len += lmp->fname ? strlen(lmp->fname) : 0;
			len += 7;
			len += strlen(lmp->size);

			if(len+3 > sizeof(buf)){
			    bfp = malloc(len+3);
			    space = len+3;
			    if((bfp=malloc(len+3)) == NULL){
				emlwrite("\007Can't malloc space for filename",
					 NULL);
				continue;
			    }
			}
			else{
			    bfp = buf;
			    space = sizeof(buf);
			}

			if(lmp->dir && lmp->dir[0])
			  snprintf(bfp, space, "%s%c%s", lmp->dir, C_FILESEP,
				  lmp->fname ? lmp->fname : "");
			else
			  snprintf(bfp, space, "%s", lmp->fname ? lmp->fname : "");

			(void) QuoteAttach(bfp, space);

			snprintf(bfp + strlen(bfp), space-strlen(bfp), " (%s) \"\"%s", lmp->size,
			    (!headents[ods.cur_e].hd_text->text[0]) ? "":",");

			if(FormatLines(headents[ods.cur_e].hd_text, bfp,
				     term.t_ncol - headents[ods.cur_e].prwid,
				     headents[ods.cur_e].break_on_comma,0)==-1){
			    emlwrite("\007Format lines failed!", NULL);
			}

			if(bfp != buf)
			  free(bfp);

			UpdateHeader(0);
		    }

		    zotlmlist(lm);
		}				/* else, nothing of interest */
            } else if (headents[ods.cur_e].selector != NULL) {
		VARS_TO_SAVE *saved_state;

                /*---- General selector for non-attachments -----*/

		/*
		 * Since the selector may make a new call back to pico()
		 * we need to save and restore the pico state here.
		 */
		if((saved_state = save_pico_state()) != NULL){
		    bufp = (*(headents[ods.cur_e].selector))(&errmss);
		    restore_pico_state(saved_state);
		    free_pico_state(saved_state);
		    ttresize();			/* fixup screen bufs */
		    picosigs();			/* restore altered signals */
		}
		else{
		    char *s = "Not enough memory";
		    size_t len;

		    len = strlen(s);
		    errmss = (char *)malloc((len+1) * sizeof(char));
		    strncpy(errmss, s, len+1);
		    errmss[len] = '\0';
		    bufp = NULL;
		}

                if(bufp != NULL) {
		    mangled = 0;
		    err = NULL;
                    if(headents[ods.cur_e].break_on_comma) {
                        /*--- Must be an address ---*/
			
			/*
			 * If current line is empty and there are more
			 * lines that follow, delete the empty lines
			 * before adding the new address.
			 */
                        if(ods.cur_l->text[0] == '\0' && ods.cur_l->next){
			    do {
				KillHeaderLine(ods.cur_l, 1);
				ods.p_len = ucs4_strlen(ods.cur_l->text);
			    } while(ods.cur_l->text[0] == '\0' && ods.cur_l->next);
			}

			ods.p_ind = 0;

                        if(ods.cur_l->text[0] != '\0'){
			    struct hdr_line *h, *start_of_addr;
			    int              q = 0;
			    
			    /* cur is not first line */
			    if(ods.cur_l != headents[ods.cur_e].hd_text){
				/*
				 * Protect against adding a new entry into
				 * the middle of a long, continued entry.
				 */
				start_of_addr = NULL;	/* cur_l is a good place to be */
				q = 0;
				for(h = headents[ods.cur_e].hd_text; h; h = h->next){
				    if(ucs4_strqchr(h->text, ',', &q, -1)){
					start_of_addr = NULL;
					q = 0;
				    }
				    else if(start_of_addr == NULL)
				      start_of_addr = h;

				    if(h->next == ods.cur_l)
				      break;
				}

				if(start_of_addr){
				    ods.cur_l = start_of_addr;
				    ods.p_len = ucs4_strlen(ods.cur_l->text);
				}
			    }

			    for(i = ++ods.p_len; i; i--)
			      ods.cur_l->text[i] = ods.cur_l->text[i-1];

			    ods.cur_l->text[0] = ',';
			}

                        if(FormatLines(ods.cur_l, bufp,
				      (term.t_ncol-headents[ods.cur_e].prwid), 
                                      headents[ods.cur_e].break_on_comma, 0) == -1){
                            emlwrite("Problem adding address to header !",
                                     NULL);
                            (*term.t_beep)();
                            break;
                        }

			/*
			 * If the "selector" has a "builder" as well, pass
			 * what was just selected thru the builder...
			 */
			if(headents[ods.cur_e].builder){
			    struct hdr_line *l;
			    int		     cur_row, top_too = 0;

			    for(l = headents[ods.cur_e].hd_text, cur_row = 0;
				l && l != ods.cur_l;
				l = l->next, cur_row++)
			      ;

			    top_too = headents[ods.cur_e].hd_text == ods.top_l;

			    if(call_builder(&headents[ods.cur_e], &mangled,
					    &err) < 0){
			        fix_mangle_and_err(&mangled, &err,
						   headents[ods.cur_e].name);
			    }

			    for(ods.cur_l = headents[ods.cur_e].hd_text;
				ods.cur_l->next && cur_row;
				ods.cur_l = ods.cur_l->next, cur_row--)
			      ;

			    if(top_too)
			      ods.top_l = headents[ods.cur_e].hd_text;
			}

    		        UpdateHeader(0);
                    } else {
			UCS *u;

			u = utf8_to_ucs4_cpystr(bufp);
			if(u){
			  ucs4_strncpy(headents[ods.cur_e].hd_text->text, u, HLSZ);
			  headents[ods.cur_e].hd_text->text[HLSZ-1] = '\0';
			  fs_give((void **) &u);
			}
                    }

		    free(bufp);
		    /* mark this entry dirty */
		    mark_sticky(&headents[ods.cur_e]);
		    headents[ods.cur_e].dirty  = 1;
		    fix_mangle_and_err(&mangled,&err,headents[ods.cur_e].name);
		}
	    } else {
                /*----- No selector -----*/
		(*term.t_beep)();
		continue;
	    }

	    PaintBody(0);
	    if(errmss != NULL) {
		(*term.t_beep)();
		emlwrite(errmss, NULL);
		free(errmss);
		errmss = NULL;
	    }
	    continue;

	  case (CTRL|'G'):			/* HELP */
	    if(term.t_mrow == 0){
		if(km_popped == 0){
		    km_popped = 2;
		    sgarbk = 1;			/* bring up menu */
		    break;
		}
	    }

	    if(!ComposerHelp(ods.cur_e))
	      break;				/* else, fall through... */

	  case (CTRL|'L'):			/* redraw requested */
	    PaintBody(0);
	    break;

	  case (CTRL|'_'):			/* file editor */
            if(headents[ods.cur_e].fileedit != NULL){
		struct headerentry *e;
		struct hdr_line    *line;
		int                 sz = 0;
		char               *filename = NULL;
		VARS_TO_SAVE       *saved_state;

		/*
		 * Since the fileedit will make a new call back to pico()
		 * we need to save and restore the pico state here.
		 */
		if((saved_state = save_pico_state()) != NULL){
		    UCS *u;

		    e = &headents[ods.cur_e];

		    for(line = e->hd_text; line != NULL; line = line->next)
		      sz += ucs4_strlen(line->text);

		    u = (UCS *)malloc((sz+1) * sizeof(*u));
		    if(u){
			u[0] = '\0';
			for(line = e->hd_text; line != NULL; line = line->next)
			  ucs4_strncat(u, line->text, sz+1-ucs4_strlen(u)-1);
		    
			filename = ucs4_to_utf8_cpystr(u);
			free(u);
		    }

		    errmss = (*(headents[ods.cur_e].fileedit))(filename);

		    if(filename)
		      free(filename);

		    restore_pico_state(saved_state);
		    free_pico_state(saved_state);
		    ttresize();			/* fixup screen bufs */
		    picosigs();			/* restore altered signals */
		}
		else{
		    char *s = "Not enough memory";
		    size_t len;

		    len = strlen(s);
		    errmss = (char *)malloc((len+1) * sizeof(char));
		    strncpy(errmss, s, len+1);
		    errmss[len] = '\0';
		}

		PaintBody(0);

		if(errmss != NULL) {
		    (*term.t_beep)();
		    emlwrite(errmss, NULL);
		    free(errmss);
		    errmss = NULL;
		}

		continue;
	    }
	    else
	      goto bleep;

	    break;

	  default :				/* huh? */
bleep:
	    unknown_command(ch);

	  case NODATA:
	    break;
	}
    }
    while (ods.p_line < ComposerTopLine);

    display_delimiter(1);
    curwp->w_flag |= WFMODE;
    movecursor(currow, curcol);
    ComposerEditing = FALSE;
    if (ComposerTopLine == BOTTOM()){
	UpdateHeader(0);
	PaintHeader(COMPOSER_TOP_LINE, FALSE);
	PaintBody(1);
    }
    
    return(retval);
}


/*
 *
 */
int
header_downline(int beyond, int gripe)
{
    struct hdr_line *new_l, *l;
    int    new_e, status, fullpaint, len, e, incr = 0;

    /* calculate the next line: physical *and* logical */
    status    = 0;
    new_e     = ods.cur_e;
    if((new_l = next_sel_hline(&new_e, ods.cur_l)) == NULL && !beyond){

	if(gripe){
	    char xx[81];

	    strncpy(xx, "Can't move down. Use ^X to ", sizeof(xx));
	    xx[sizeof(xx)-1] = '\0';
	    strncat(xx, (Pmaster && Pmaster->exit_label)
			    ? Pmaster->exit_label
			    : (gmode & MDHDRONLY)
			      ? "eXit/Save"
			      : (gmode & MDVIEW)
				? "eXit"
				: "Send", sizeof(xx)-strlen(xx)-1);
	    xx[sizeof(xx)-1] = '\0';
	    strncat(xx, ".", sizeof(xx)-strlen(xx)-1);
	    xx[sizeof(xx)-1] = '\0';
	    emlwrite(xx, NULL);
	}

        return(0);
    }

    /*
     * Because of blank header lines the cursor may need to move down
     * more than one line. Figure out how far.
     */
    e = ods.cur_e;
    l = ods.cur_l;
    while(l != new_l){
	if((l = next_hline(&e, l)) != NULL)
	  incr++;
	else
	  break;  /* can't happen */
    }

    ods.p_line += incr;
    fullpaint = ods.p_line >= BOTTOM();	/* force full redraw?       */

    /* expand what needs expanding */
    if(new_e != ods.cur_e || !new_l){		/* new (or last) field !    */
	if(new_l)
	  InvertPrompt(ods.cur_e, FALSE);	/* turn off current entry   */

	if(headents[ods.cur_e].is_attach) {	/* verify data ?	    */
	    if((status = FormatSyncAttach()) != 0){	/* fixup if 1 or -1	    */
		headents[ods.cur_e].rich_header = 0;
		if(FormatLines(headents[ods.cur_e].hd_text, "",
			       term.t_ncol-headents[new_e].prwid,
			       headents[ods.cur_e].break_on_comma, 0) == -1)
		  emlwrite("\007Format lines failed!", NULL);
	    }
	} else if(headents[ods.cur_e].builder) { /* expand addresses	    */
	    int mangled = 0;
	    char *err = NULL;

	    if((status = call_builder(&headents[ods.cur_e], &mangled, &err))>0){
		struct hdr_line *l;		/* fixup ods.cur_l */
		ods.p_line = 0;			/* force top line recalc */
		for(l = headents[ods.cur_e].hd_text; l; l = l->next)
		  ods.cur_l = l;

		if(new_l)			/* if new_l, force validity */
		  new_l = headents[new_e].hd_text;

		NewTop(0);			/* get new top_l */
	    }
	    else if(status < 0){		/* bad addr? no leave! */
		--ods.p_line;
		fix_mangle_and_err(&mangled, &err, headents[ods.cur_e].name);
		InvertPrompt(ods.cur_e, TRUE);
		return(0);
	    }

	    fix_mangle_and_err(&mangled, &err, headents[ods.cur_e].name);
	}

	if(new_l){				/* if one below, turn it on */
	    InvertPrompt(new_e, TRUE);
	    sgarbk = 1;				/* paint keymenu too	    */
	}
    }

    if(new_l){					/* fixup new pointers	    */
	ods.cur_l = (ods.cur_e != new_e) ? headents[new_e].hd_text : new_l;
	ods.cur_e = new_e;
	if(ods.p_ind > (len = ucs4_strlen(ods.cur_l->text)))
	  ods.p_ind = len;
    }

    if(!new_l || status || fullpaint){		/* handle big screen paint  */
	UpdateHeader(0);
	PaintHeader(COMPOSER_TOP_LINE, FALSE);
	PaintBody(1);

	if(!new_l){				/* make sure we're done     */
	    ods.p_line = ComposerTopLine;
	    InvertPrompt(ods.cur_e, FALSE);	/* turn off current entry   */
	}
    }

    return(new_l ? 1 : 0);
}


/*
 *
 */
int
header_upline(int gripe)
{
    struct hdr_line *new_l, *l;
    int    new_e, status, fullpaint, len, e, incr = 0;
    EML    eml;

    /* calculate the next line: physical *and* logical */
    status    = 0;
    new_e     = ods.cur_e;
    if(!(new_l = prev_sel_hline(&new_e, ods.cur_l))){	/* all the way up! */
	ods.p_line = COMPOSER_TOP_LINE;
	if(gripe){
	    eml.s = (Pmaster->pine_flags & MDHDRONLY) ? "entry" : "header";
	    emlwrite(_("Can't move beyond top of %s"), &eml);
	}

	return(0);
    }
    
    /*
     * Because of blank header lines the cursor may need to move up
     * more than one line. Figure out how far.
     */
    e = ods.cur_e;
    l = ods.cur_l;
    while(l != new_l){
	if((l = prev_hline(&e, l)) != NULL)
	  incr++;
	else
	  break;  /* can't happen */
    }

    ods.p_line -= incr;
    fullpaint = ods.p_line <= COMPOSER_TOP_LINE;

    if(new_e != ods.cur_e){			/* new field ! */
	InvertPrompt(ods.cur_e, FALSE);
	if(headents[ods.cur_e].is_attach){
	    if((status = FormatSyncAttach()) != 0){   /* non-zero ? reformat field */
		headents[ods.cur_e].rich_header = 0;
		if(FormatLines(headents[ods.cur_e].hd_text, "",
			       term.t_ncol - headents[ods.cur_e].prwid,
			       headents[ods.cur_e].break_on_comma,0) == -1)
		  emlwrite("\007Format lines failed!", NULL);
	    }
	}
	else if(headents[ods.cur_e].builder){
	    int mangled = 0;
	    char *err = NULL;

	    if((status = call_builder(&headents[ods.cur_e], &mangled,
				      &err)) >= 0){
		/* repair new_l */
		for(new_l = headents[new_e].hd_text;
		    new_l->next;
		    new_l=new_l->next)
		  ;

		/* and cur_l (required in fix_and... */
		ods.cur_l = new_l;
	    }
	    else{
		++ods.p_line;
		fix_mangle_and_err(&mangled, &err, headents[ods.cur_e].name);
		InvertPrompt(ods.cur_e, TRUE);
		return(0);
	    }

	    fix_mangle_and_err(&mangled, &err, headents[ods.cur_e].name);
	}

	InvertPrompt(new_e, TRUE);
	sgarbk = 1;
    }

    ods.cur_e = new_e;				/* update pointers */
    ods.cur_l = new_l;
    if(ods.p_ind > (len = ucs4_strlen(ods.cur_l->text)))
      ods.p_ind = len;

    if(status > 0 || fullpaint){
	UpdateHeader(0);
	PaintHeader(COMPOSER_TOP_LINE, FALSE);
	PaintBody(1);
    }

    return(1);
}


/*
 * 
 */
int
AppendAttachment(char *fn, char *sz, char *cmt)
{
    int	 a_e, status, spaces;
    struct hdr_line *lp;
    char b[256];
    UCS *u;

    /*--- Find headerentry that is attachments (only first) --*/
    for(a_e = 0; headents[a_e].name != NULL; a_e++ )
      if(headents[a_e].is_attach){
	  /* make sure field stays displayed */
	  headents[a_e].rich_header = 0;
	  headents[a_e].display_it = 1;
	  break;
      }

    /* append new attachment line */
    for(lp = headents[a_e].hd_text; lp->next; lp=lp->next)
      ;

    /* build new attachment line */
    if(lp->text[0]){		/* adding a line? */
	UCS comma[2];

	comma[0] = ',';
	comma[1] = '\0';
	ucs4_strncat(lp->text, comma, HLSZ-ucs4_strlen(lp->text)-1);	/* append delimiter */
	if((lp->next = HALLOC()) != NULL){	/* allocate new line */
	    lp->next->prev = lp;
	    lp->next->next = NULL;
	    lp = lp->next;
	}
	else{
	    emlwrite("\007Can't allocate line for new attachment!", NULL);
	    return(0);
	}
    }

    
    spaces = (*fn == '\"') ? 0 : (strpbrk(fn, "(), \t") != NULL);
    snprintf(b, sizeof(b), "%s%s%s (%s) \"%.*s\"",
	    spaces ? "\"" : "", fn, spaces ? "\"" : "",
	    sz ? sz : "", 80, cmt ? cmt : "");
    u = utf8_to_ucs4_cpystr(b);
    if(u){
	ucs4_strncpy(lp->text, u, HLSZ); 
	lp->text[HLSZ-1] = '\0';
	fs_give((void **) &u);
    }

    /* validate the new attachment, and reformat if needed */
    if((status = SyncAttach()) != 0){
	EML eml;

	if(status < 0){
	    eml.s = fn;
	    emlwrite("\007Problem attaching: %s", &eml);
	}

	if(FormatLines(headents[a_e].hd_text, "",
		       term.t_ncol - headents[a_e].prwid,
		       headents[a_e].break_on_comma, 0) == -1){
	    emlwrite("\007Format lines failed!", NULL);
	    return(0);
	}
    }

    UpdateHeader(0);
    PaintHeader(COMPOSER_TOP_LINE, status != 0);
    PaintBody(1);
    return(status != 0);
}


/*
 * LineEdit - Always use insert mode and handle line wrapping
 *
 *	returns:
 *		Any characters typed in that aren't printable 
 *		(i.e. commands)
 *
 *	notes: 
 *		Assume we are guaranteed that there is sufficiently 
 *		more buffer space in a line than screen width (just one 
 *		less thing to worry about).  If you want to change this,
 *		then pputc will have to be taught to check the line buffer
 *		length, and HALLOC() will probably have to become a func.
 */
UCS
LineEdit(int allowedit, UCS *lastch)
{
    register struct	hdr_line   *lp;		/* temporary line pointer    */
    register int	i;
    UCS                 ch = 0;
    register int	status;			/* various func's return val */
    UCS	               *tbufp;			/* temporary buffer pointers */
    int	                skipmove = 0;
    UCS	               *strng;
    UCS                 last_key;		/* last keystroke  */

    strng   = ods.cur_l->text;			/* initialize offsets */
    ods.p_len = MIN(ucs4_strlen(strng), HLSZ);
    if(ods.p_ind < 0)				/* offset within range? */
      ods.p_ind = 0;
    else if(ods.p_ind > ods.p_len)
      ods.p_ind = ods.p_len;
    else if(ucs4_str_width_ptr_to_ptr(&strng[0], &strng[ods.p_ind]) > LINEWID()){
	UCS *u;

	u = ucs4_particular_width(strng, LINEWID());
	ods.p_ind = u - strng;
    }

    while(1){					/* edit the line... */

	if(skipmove)
	  skipmove = 0;
	else
	  HeaderPaintCursor();

	last_key = ch;
	if(ch && lastch)
	  *lastch = ch;

	(*term.t_flush)();			/* get everything out */

#ifdef MOUSE
	mouse_in_content(KEY_MOUSE, -1, -1, 0, 0);
	register_mfunc(mouse_in_content,2,0,term.t_nrow-(term.t_mrow+1),
		       term.t_ncol);
#endif
#ifdef	_WINDOWS
	mswin_setdndcallback (composer_file_drop);
	mswin_mousetrackcallback(pico_cursor);
#endif

	ch = GetKey();
	
	if (term.t_nrow < 6 && ch != NODATA){
	    (*term.t_beep)();
	    emlwrite(_("Please make the screen larger."), NULL);
	    continue;
	}

#ifdef	MOUSE
	clear_mfunc(mouse_in_content);
#endif
#ifdef	_WINDOWS
	mswin_cleardndcallback ();
	mswin_mousetrackcallback(NULL);
#endif

	switch(ch){
	  case DEL :
	    if(gmode & P_DELRUBS)
	      ch = KEY_DEL;

	  default :
	    (*Pmaster->keybinput)();
	    if(!time_to_check())
	      break;

	  case NODATA :			/* new mail ? */
	    if((*Pmaster->newmail)(ch == NODATA ? 0 : 2, 1) >= 0){
		int rv;
		
		if(km_popped){
		    term.t_mrow = 2;
		    curwp->w_ntrows -= 2;
		}

		clearcursor();
		mlerase();
		rv = (*Pmaster->showmsg)(ch);
		ttresize();
		picosigs();
		if(rv)		/* Did showmsg corrupt the display? */
		  PaintBody(0);	/* Yes, repaint */

		mpresf = 1;
		if(km_popped){
		    term.t_mrow = 0;
		    curwp->w_ntrows += 2;
		}
	    }

	    clearcursor();
	    movecursor(ods.p_line,
		ucs4_str_width_ptr_to_ptr(strng, &strng[ods.p_ind])+headents[ods.cur_e].prwid);
	    if(ch == NODATA)			/* GetKey timed out */
	      continue;

	    break;
	}

        if(mpresf){				/* blast old messages */
	    if(mpresf++ > NMMESSDELAY){		/* every few keystrokes */
		mlerase();
		movecursor(ods.p_line,
		    ucs4_str_width_ptr_to_ptr(strng, &strng[ods.p_ind])+headents[ods.cur_e].prwid);
	    }
        }

	tbufp = &strng[ods.p_len];

	if(VALID_KEY(ch)){			/* char input */
            /*
             * if we are allowing editing, insert the new char
             * end up leaving tbufp pointing to newly
             * inserted character in string, and offset to the
             * index of the character after the inserted ch ...
             */
            if(allowedit){
		if(headents[ods.cur_e].is_attach && intag(strng,ods.p_ind)){
		    emlwrite(_("\007Can't edit attachment number!"), NULL);
		    continue;
		}

		if(headents[ods.cur_e].single_space){
		    if(ch == ' ' 
		       && (strng[ods.p_ind]==' ' || strng[ods.p_ind-1]==' '))
		      continue;
		}

		/*
		 * go ahead and add the character...
		 */
		if(ods.p_len < HLSZ){
		    tbufp = &strng[++ods.p_len];	/* find the end */
		    do{
			*tbufp = tbufp[-1];
		    } while(--tbufp > &strng[ods.p_ind]);	/* shift right */
		    strng[ods.p_ind++] = ch;	/* add char to str */
		}

		/* mark this entry dirty */
		mark_sticky(&headents[ods.cur_e]);
		headents[ods.cur_e].dirty  = 1;

		/*
		 * then find out where things fit...
		 */
		if(ucs4_str_width_ptr_to_ptr(&strng[0], &strng[ods.p_len]) < LINEWID()){
		    CELL c;

		    c.c = ch & CELLMASK;
		    c.a = 0;
		    if(pinsert(c)){		/* add char to str */
			skipmove++;		/* must'a been optimal */
			continue; 		/* on to the next! */
		    }
		}
		else{
                    if((status = FormatLines(ods.cur_l, "", LINEWID(), 
    			        headents[ods.cur_e].break_on_comma,0)) == -1){
                        (*term.t_beep)();
                        continue;
                    }
                    else{
			/*
			 * during the format, the dot may have moved
			 * down to the next line...
			 */
			if(ods.p_ind >= ucs4_strlen(strng)){
			    ods.p_line++;
			    ods.p_ind -= ucs4_strlen(strng);
			    ods.cur_l = ods.cur_l->next;
			    strng = ods.cur_l->text;
			}

			ods.p_len = ucs4_strlen(strng);
		    }

		    UpdateHeader(0);
		    PaintHeader(COMPOSER_TOP_LINE, FALSE);
		    PaintBody(1);
                    continue;
		}
            }
            else{  
                rdonly();
                continue;
            } 
        }
        else {					/* interpret ch as a command */
            switch (ch = normalize_cmd(ch, ckm, 2)) {
              case (CTRL|KEY_LEFT):     /* word skip left */
                if(ods.p_ind > 0)       /* Scoot one char left if possible */
                  ods.p_ind--;

                if(ods.p_ind == 0)
                {
	          if(ods.p_line != COMPOSER_TOP_LINE)
		    ods.p_ind = 1000;		/* put cursor at end of line */
		  return(KEY_UP);
                }

		while(ods.p_ind > 0 && !ucs4_isalnum(strng[ods.p_ind]))
		  ods.p_ind--;		/* skip any whitespace we're in */

		while(ods.p_ind > 0) {
                  /* Bail if the character right before this one is whitespace */
                  if(ods.p_ind > 1 && !ucs4_isalnum(strng[ods.p_ind - 1]))
                    break;
		  ods.p_ind--;
                }
                continue;

	      case (CTRL|'@') :		/* word skip */
              case (CTRL|KEY_RIGHT):
		while(ucs4_isalnum(strng[ods.p_ind]))
		  ods.p_ind++;		/* skip any text we're in */

		while(strng[ods.p_ind] && !ucs4_isalnum(strng[ods.p_ind]))
		  ods.p_ind++;		/* skip any whitespace after it */

		if(strng[ods.p_ind] == '\0'){
		    ods.p_ind = 0;	/* end of line, let caller handle it */
		    return(KEY_DOWN);
		}

		continue;

	      case (CTRL|'K') :			/* kill line cursor's on */
		if(!allowedit){
		    rdonly();
		    continue;
		}

		lp = ods.cur_l;
		if (!(gmode & MDDTKILL))
		  ods.p_ind = 0;

		if(KillHeaderLine(lp, (last_key == (CTRL|'K')))){
		    if(TERM_OPTIMIZE && 
		       !(ods.cur_l->prev==NULL && ods.cur_l->next==NULL)) 
		      scrollup(wheadp, ods.p_line, 1);

		    if(ods.cur_l->next == NULL)
		      if(zotcomma(ods.cur_l->text)){
			  if(ods.p_ind > 0)
			    ods.p_ind = ucs4_strlen(ods.cur_l->text);
		      }
		    
		    i = (ods.p_line == COMPOSER_TOP_LINE);
		    UpdateHeader(0);
		    PaintHeader(COMPOSER_TOP_LINE, TRUE);

		    if(km_popped){
			km_popped--;
			movecursor(term.t_nrow, 0);
			peeol();
		    }

		    PaintBody(1);

		}
		strng = ods.cur_l->text;
		ods.p_len = ucs4_strlen(strng);
		headents[ods.cur_e].sticky = 0;
		headents[ods.cur_e].dirty  = 1;
		continue;

	      case (CTRL|'U') :			/* un-delete deleted lines */
		if(!allowedit){
		    rdonly();
		    continue;
		}

		if(SaveHeaderLines()){
		    UpdateHeader(0);
		    PaintHeader(COMPOSER_TOP_LINE, FALSE);
		    if(km_popped){
			km_popped--;
			movecursor(term.t_nrow, 0);
			peeol();
		    }

		    PaintBody(1);
		    strng = ods.cur_l->text;
		    ods.p_len = ucs4_strlen(strng);
		    mark_sticky(&headents[ods.cur_e]);
		    headents[ods.cur_e].dirty  = 1;
		}
		else
		  /* TRANSLATORS: Killing text is deleting it and
		     Unkilling text is undeleting killed text. */
		  emlwrite(_("Problem Unkilling text"), NULL);
		continue;

	      case (CTRL|'F') :
	      case KEY_RIGHT:			/* move character right */
		if(ods.p_ind < ods.p_len){
		    ods.p_ind++;
		    continue;
		}
		else if(gmode & MDHDRONLY)
		  continue;

		ods.p_ind = 0;
		return(KEY_DOWN);

	      case (CTRL|'B') :
	      case KEY_LEFT	:		/* move character left */
		if(ods.p_ind > 0){
		    ods.p_ind--;
		    continue;
		}
		if(ods.p_line != COMPOSER_TOP_LINE)
		  ods.p_ind = 1000;		/* put cursor at end of line */
		return(KEY_UP);

	      case (CTRL|'M') :			/* goto next field */
		ods.p_ind = 0;
		return(KEY_DOWN);

	      case KEY_HOME :
	      case (CTRL|'A') :			/* goto beginning of line */
		ods.p_ind = 0;
		continue;

	      case KEY_END  :
	      case (CTRL|'E') :			/* goto end of line */
		ods.p_ind = ods.p_len;
		continue;

	      case (CTRL|'D')   :		/* blast this char */
	      case KEY_DEL :
		if(!allowedit){
		    rdonly();
		    continue;
		}
		else if(ods.p_ind >= ucs4_strlen(strng))
		  continue;

		if(headents[ods.cur_e].is_attach && intag(strng, ods.p_ind)){
		    emlwrite(_("\007Can't edit attachment number!"), NULL);
		    continue;
		}

		pputc(strng[ods.p_ind++], 0); 	/* drop through and rubout */

	      case DEL        :			/* blast previous char */
	      case (CTRL|'H') :
		if(!allowedit){
		    rdonly();
		    continue;
		}

		if(headents[ods.cur_e].is_attach && intag(strng, ods.p_ind-1)){
		    emlwrite(_("\007Can't edit attachment number!"), NULL);
		    continue;
		}

		if(ods.p_ind > 0){		/* just shift left one char */
		    ods.p_len--;
		    headents[ods.cur_e].dirty  = 1;
		    if(ods.p_len == 0)
		      headents[ods.cur_e].sticky = 0;
		    else
		      mark_sticky(&headents[ods.cur_e]);

		    tbufp = &strng[--ods.p_ind];
		    while(*tbufp++ != '\0')
		      tbufp[-1] = *tbufp;
		    tbufp = &strng[ods.p_ind];
		    if(pdel())			/* physical screen delete */
		      skipmove++;		/* must'a been optimal */
		}
		else{				/* may have work to do */
		    if(ods.cur_l->prev == NULL){  
			(*term.t_beep)();	/* no erase into next field */
			continue;
		    }

		    ods.p_line--;
		    ods.cur_l = ods.cur_l->prev;
		    strng = ods.cur_l->text;
		    if((i=ucs4_strlen(strng)) > 0){
			strng[i-1] = '\0';	/* erase the character */
			ods.p_ind = i-1;
		    }
		    else{
			headents[ods.cur_e].sticky = 0;
			ods.p_ind = 0;
		    }
		    
		    tbufp = &strng[ods.p_ind];
		}

		if((status = FormatLines(ods.cur_l, "", LINEWID(), 
				   headents[ods.cur_e].break_on_comma,0))==-1){
		    (*term.t_beep)();
		    continue;
		}
		else{
		    /*
		     * beware, the dot may have moved...
		     */
		    while((ods.p_len=ucs4_strlen(strng)) < ods.p_ind){
			ods.p_line++;
			ods.p_ind -= ucs4_strlen(strng);
			ods.cur_l = ods.cur_l->next;
			strng = ods.cur_l->text;
			ods.p_len = ucs4_strlen(strng);
			tbufp = &strng[ods.p_ind];
			status = TRUE;
		    }

		    if(UpdateHeader(0))
		      status = TRUE;

		    PaintHeader(COMPOSER_TOP_LINE, FALSE);
		    if(status == TRUE)
		      PaintBody(1);
		}

		movecursor(ods.p_line,
		    ucs4_str_width_ptr_to_ptr(strng, &strng[ods.p_ind])+headents[ods.cur_e].prwid);

		if(skipmove)
		  continue;

		break;

              default   :
		return(ch);
            }
        }

	while ((tbufp-strng) < HLSZ && *tbufp != '\0')		/* synchronizing loop */
	  pputc(*tbufp++, 0);

	if(ucs4_str_width_ptr_to_ptr(&strng[0], &strng[ods.p_len]) < LINEWID())
	  peeol();
    }
}


void
HeaderPaintCursor(void)
{
    movecursor(ods.p_line, ucs4_str_width_ptr_to_ptr(&ods.cur_l->text[0], &ods.cur_l->text[ods.p_ind])+headents[ods.cur_e].prwid);
}



/*
 * FormatLines - Place the given text at the front of the given line->text
 *               making sure to properly format the line, then check
 *               all lines below for proper format.
 *
 *	notes:
 *		Not much optimization at all.  Right now, it recursively
 *		fixes all remaining lines in the entry.  Some speed might
 *		gained if this was built to iteratively scan the lines.
 *
 *	returns:
 *		-1 on error
 *		FALSE if only this line is changed
 *		TRUE  if text below the first line is changed
 */
int
FormatLines(struct hdr_line *h,			/* where to begin formatting */
	    char *utf8_instr,			/* input string */
	    int maxwid,				/* max width of a line */
	    int break_on_comma,			/* break lines on commas */
	    int quoted)				/* this line inside quotes */
{
    int		 retval = FALSE;
    int		 i, l, len;
    char        *utf8;
    UCS		*ostr;			/* pointer to output string */
    UCS         *free_istr = NULL;
    UCS         *istr = NULL;
    UCS 	*breakp;		/* pointer to line break */
    UCS		*bp, *tp;		/* temporary pointers */
    UCS		*buf;			/* string to add later */
    struct hdr_line	*nlp, *lp;

    ostr = h->text;
    nlp = h->next;
    if(utf8_instr)
      free_istr = istr = utf8_to_ucs4_cpystr(utf8_instr);

    len = ucs4_strlen(istr) + ucs4_strlen(ostr);
    if((buf = (UCS *) malloc((len+10) * sizeof(*buf))) == NULL){
	if(free_istr)
	  fs_give((void **) &free_istr);

	return(-1);
    }

    if(ucs4_str_width(istr) + ucs4_str_width(ostr) >= maxwid){	/* break then fixup below */

	if((l=ucs4_str_width(istr)) < maxwid){		/* room for more */

	    if(break_on_comma && (bp = ucs4_strqchr(istr, ',', &quoted, -1))){
		bp += (bp[1] == ' ') ? 2 : 1;
		for(tp = bp; *tp && *tp == ' '; tp++)
		  ;

		ucs4_strncpy(buf, tp, len+10);
		buf[len+10-1] = '\0';
		ucs4_strncat(buf, ostr, len+10-ucs4_strlen(buf)-1);
		buf[len+10-1] = '\0';

		for(i = 0; &istr[i] < bp; i++)
		  ostr[i] = istr[i];

		ostr[i] = '\0';
		retval = TRUE;
	    }
	    else{
		breakp = break_point(ostr, maxwid-ucs4_str_width(istr),
				     break_on_comma ? ',' : ' ',
				     break_on_comma ? &quoted : NULL);

		if(breakp == ostr){	/* no good breakpoint */
		    if(break_on_comma && *breakp == ','){
			breakp = ostr + 1;
			retval = TRUE;
		    }
		    else if(ucs4_strchr(istr,(break_on_comma && !quoted)?',':' ')){
			ucs4_strncpy(buf, ostr, len+10);
			buf[len+10-1] = '\0';
			ucs4_strncpy(ostr, istr, HLSZ);
			ostr[HLSZ-1] = '\0';
		    }
		    else{		/* istr's broken as we can get it */
			/*
			 * Break at maxwid - width of istr
			 */
			breakp = ucs4_particular_width(ostr, maxwid - ucs4_str_width(istr)-1);
			retval = TRUE;
		    }
		}
		else
		  retval = TRUE;
	    
		if(retval){
		    ucs4_strncpy(buf, breakp, len+10);	/* save broken line */
		    buf[len+10-1] = '\0';
		    if(breakp == ostr){
			ucs4_strncpy(ostr, istr, HLSZ);	/* simple if no break */
			ostr[HLSZ-1] = '\0';
		    }
		    else{
			*breakp = '\0';		/* more work to break it */
			i = ucs4_strlen(istr);
			/*
			 * shift ostr i chars
			 */
			for(bp=breakp; bp >= ostr && i; bp--)
			  *(bp+i) = *bp;
			for(tp=ostr, bp=istr; *bp != '\0'; tp++, bp++)
			  *tp = *bp;		/* then add istr */
		    }
		}
	    }
	}
	/*
	 * Short-circuit the recursion in this case.
	 * No time right now to figure out how to do it better.
	 */
	else if(l > 2*maxwid){
	    UCS *istrp, *saveostr = NULL;

	    retval = TRUE;
	    if(ostr && *ostr)
	      saveostr = ucs4_cpystr(ostr);

	    istrp = istr;
	    while(l > 2*maxwid){
		if(break_on_comma || maxwid == 1){
		    breakp = (!(bp = ucs4_strqchr(istrp, ',', &quoted, maxwid))
			      || ucs4_str_width_ptr_to_ptr(istrp, bp) >= maxwid || maxwid == 1)
			       ? ucs4_particular_width(istrp, maxwid)
			       : bp + ((bp[1] == ' ') ? 2 : 1);
		}
		else{
		    breakp = break_point(istrp, maxwid, ' ', NULL);

		    if(breakp == istrp)	/* no good break point */
		      breakp =  ucs4_particular_width(istrp, maxwid-1);
		}
		
		for(tp=ostr,bp=istrp; bp < breakp; tp++, bp++)
		  *tp = *bp;

		*tp = '\0';
		l -= ucs4_str_width_ptr_to_ptr(istrp, breakp);
		istrp = breakp;

		if((lp = HALLOC()) == NULL){
		    emlwrite("Can't allocate any more lines for header!", NULL);
		    free(buf);
		    if(free_istr)
		      fs_give((void **) &free_istr);

		    return(-1);
		}

		lp->next = h->next;
		if(h->next)
		  h->next->prev = lp;

		h->next = lp;
		lp->prev = h;
		lp->text[0] = '\0';
		h = h->next;
		ostr = h->text;
	    }

	    /*
	     * Now l is still > maxwid. Do it the recursive way,
	     * like the else clause below. Surely we could fix up the
	     * flow control some here, but this works for now.
	     */

	    nlp = h->next;
	    istr = istrp;
	    if(saveostr){
		ucs4_strncpy(ostr, saveostr, HLSZ);
		ostr[HLSZ-1] = '\0';
		fs_give((void **) &saveostr);
	    }

	    if(break_on_comma || maxwid == 1){
		breakp = (!(bp = ucs4_strqchr(istrp, ',', &quoted, maxwid))
			  || ucs4_str_width_ptr_to_ptr(istrp, bp) >= maxwid || maxwid == 1)
			   ? ucs4_particular_width(istrp, maxwid)
			   : bp + ((bp[1] == ' ') ? 2 : 1);
	    }
	    else{
		breakp = break_point(istrp, maxwid, ' ', NULL);

		if(breakp == istrp)	/* no good break point */
		  breakp =  ucs4_particular_width(istrp, maxwid-1);
	    }
	    
	    ucs4_strncpy(buf, breakp, len+10);	/* save broken line */
	    buf[len+10-1] = '\0';
	    ucs4_strncat(buf, ostr, len+10-ucs4_strlen(buf)-1);	/* add line that was there */
	    buf[len+10-1] = '\0';

	    for(tp=ostr,bp=istr; bp < breakp; tp++, bp++)
	      *tp = *bp;

	    *tp = '\0';
	}
	else{					/* utf8_instr > maxwid ! */
	    if(break_on_comma || maxwid == 1){
		breakp = (!(bp = ucs4_strqchr(istr, ',', &quoted, maxwid))
			  || ucs4_str_width_ptr_to_ptr(istr, bp) >= maxwid || maxwid == 1)
			   ? ucs4_particular_width(istr, maxwid)
			   : bp + ((bp[1] == ' ') ? 2 : 1);
	    }
	    else{
		breakp = break_point(istr, maxwid, ' ', NULL);

		if(breakp == istr)	/* no good break point */
		  breakp =  ucs4_particular_width(istr, maxwid-1);
	    }
	    
	    ucs4_strncpy(buf, breakp, len+10);	/* save broken line */
	    buf[len+10-1] = '\0';
	    ucs4_strncat(buf, ostr, len+10-ucs4_strlen(buf)-1);	/* add line that was there */
	    buf[len+10-1] = '\0';

	    for(tp=ostr,bp=istr; bp < breakp; tp++, bp++)
	      *tp = *bp;

	    *tp = '\0';
	}

	if(nlp == NULL){			/* no place to add below? */
	    if((lp = HALLOC()) == NULL){
		emlwrite("Can't allocate any more lines for header!", NULL);
		free(buf);
		if(free_istr)
		  fs_give((void **) &free_istr);

		return(-1);
	    }

	    if(TERM_OPTIMIZE && (i = physical_line(h)) != -1)
	      scrolldown(wheadp, i - 1, 1);

	    h->next = lp;			/* fix up links */
	    lp->prev = h;
	    lp->next = NULL;
	    lp->text[0] = '\0';
	    nlp = lp;
	    retval = TRUE;
	}
    }
    else{					/* combined width < max */
	buf[0] = '\0';
	if(istr && *istr){
	    ucs4_strncpy(buf, istr, len+10);	/* insert istr before ostr */
	    buf[len+10-1] = '\0';

	    ucs4_strncat(buf, ostr, len+10-ucs4_strlen(buf)-1);
	    buf[len+10-1] = '\0';

	    ucs4_strncpy(ostr, buf, HLSZ);		/* copy back to ostr */
	    ostr[HLSZ-1] = '\0';
	}

	*buf = '\0';
	breakp = NULL;

	if(break_on_comma && (breakp = ucs4_strqchr(ostr, ',', &quoted, -1))){
	    breakp += (breakp[1] == ' ') ? 2 : 1;
	    ucs4_strncpy(buf, breakp, len+10);
	    buf[len+10-1] = '\0';
	    *breakp = '\0';

	    if(*buf && !nlp){
		if((lp = HALLOC()) == NULL){
		    emlwrite("Can't allocate any more lines for header!",NULL);
		    free(buf);
		    if(free_istr)
		      fs_give((void **) &free_istr);

		    return(-1);
		}

		if(TERM_OPTIMIZE && (i = physical_line(h)) != -1)
		  scrolldown(wheadp, i - 1, 1);

		h->next = lp;		/* fix up links */
		lp->prev = h;
		lp->next = NULL;
		lp->text[0] = '\0';
		nlp = lp;
		retval = TRUE;
	    }
	}

	if(nlp){
	    if(!*buf && !breakp){
		if(ucs4_str_width(ostr) + ucs4_str_width(nlp->text) >= maxwid){	
		    breakp = break_point(nlp->text, maxwid-ucs4_str_width(ostr), 
					 break_on_comma ? ',' : ' ',
					 break_on_comma ? &quoted : NULL);
		    
		    if(breakp == nlp->text){	/* commas this line? */
			for(tp=ostr; *tp  && *tp != ' '; tp++)
			  ;

			if(!*tp){		/* no commas, get next best */
			    breakp += maxwid - ucs4_str_width(ostr) - 1;
			    retval = TRUE;
			}
			else
			  retval = FALSE;
		    }
		    else
		      retval = TRUE;

		    if(retval){			/* only if something to do */
			for(tp = &ostr[ucs4_strlen(ostr)],bp=nlp->text; bp<breakp; 
			tp++, bp++)
			  *tp = *bp;		/* add breakp to this line */
			*tp = '\0';
			for(tp=nlp->text, bp=breakp; *bp != '\0'; tp++, bp++)
			  *tp = *bp;		/* shift next line to left */
			*tp = '\0';
		    }
		}
		else{
		    ucs4_strncat(ostr, nlp->text, HLSZ-ucs4_strlen(ostr)-1);
		    ostr[HLSZ-1] = '\0';

		    if(TERM_OPTIMIZE && (i = physical_line(nlp)) != -1)
		      scrollup(wheadp, i, 1);

		    hldelete(nlp);

		    if(!(nlp = h->next)){
			free(buf);
			if(free_istr)
			  fs_give((void **) &free_istr);

			return(TRUE);		/* can't go further */
		    }
		    else
		      retval = TRUE;		/* more work to do? */
		}
	    }
	}
	else{
	    free(buf);
	    if(free_istr)
	      fs_give((void **) &free_istr);

	    return(FALSE);
	}

    }

    utf8 = ucs4_to_utf8_cpystr(buf);
    free(buf);
    if(free_istr)
      fs_give((void **) &free_istr);

    if(utf8){
	int rv;

	i = FormatLines(nlp, utf8, maxwid, break_on_comma, quoted);
	fs_give((void **) &utf8);
	switch(i){
	  case -1:					/* bubble up worst case */
	    rv = -1;
	    break;
	  case FALSE:
	    if(retval == FALSE){
		rv = FALSE;
		break;
	    }
	  default:
	    rv = TRUE;
	}

	return(rv);
    }
    else
      return(-1);
}

/*
 * Format the lines before parsing attachments so we
 * don't expand a bunch of attachments that we don't
 * have the buffer space for.
 */
int
FormatSyncAttach(void)
{
    FormatLines(headents[ods.cur_e].hd_text, "",
		term.t_ncol - headents[ods.cur_e].prwid,
		headents[ods.cur_e].break_on_comma, 0);
    return(SyncAttach());
}


/*
 * PaintHeader - do the work of displaying the header from the given 
 *               physical screen line the end of the header.
 *
 *       17 July 91 - fixed reshow to deal with arbitrarily large headers.
 */
void
PaintHeader(int line,		/* physical line on screen */
	    int clear)		/* clear before painting */
{
    struct hdr_line *lp;
    int	             curline;
    int	             curindex;
    int	             curoffset;
    UCS              buf[NLINE];
    UCS	            *bufp;
    int              i, e, w;

    if(clear)
      pclear(COMPOSER_TOP_LINE, ComposerTopLine-1);

    curline  = COMPOSER_TOP_LINE;
    curindex = curoffset = 0;

    for(lp = ods.top_l, e = ods.top_e; ; curline++){
	if((curline == line) || ((lp = next_hline(&e, lp)) == NULL))
	  break;
    }

    while(headents[e].name != NULL){			/* begin to redraw */
	while(lp != NULL){
	    buf[0] = '\0';
            if((!lp->prev || curline == COMPOSER_TOP_LINE) && !curoffset){
	        if(InvertPrompt(e, (e == ods.cur_e && ComposerEditing)) == -1
		   && !is_blank(curline, 0, headents[e].prwid)){
		    for(i = 0; i < headents[e].prwid; i++)
		      buf[i] = ' ';

		    buf[i] = '\0';
		}
	    }
	    else if(!is_blank(curline, 0, headents[e].prwid)){
		for(i = 0; i < headents[e].prwid; i++)
		  buf[i] = ' ';

		buf[i] = '\0';
	    }

	    if(*(bufp = buf) != '\0'){		/* need to paint? */
		movecursor(curline, 0);		/* paint the line... */
		while(*bufp != '\0')
		  pputc(*bufp++, 0);
	    }

	    bufp = &(lp->text[curindex]);	/* skip chars already there */
	    curoffset += headents[e].prwid;
	    curindex = index_from_col(curline, curoffset);
	    while(pscr(curline, curindex) != NULL &&
		  *bufp == pscr(curline, curindex)->c && *bufp != '\0'){
		w = wcellwidth(*bufp);
		curoffset += (w >= 0 ? w : 1);
		++bufp;
		++curindex;
		if(curoffset >= term.t_ncol)
		  break;
	    }

	    if(*bufp != '\0'){			/* need to move? */
		movecursor(curline, curoffset);
		while(*bufp != '\0'){		/* display what's not there */
		    pputc(*bufp, 0);
		    w = wcellwidth(*bufp);
		    curoffset += (w >= 0 ? w : 1);
		    ++bufp;
		    ++curindex;
		}
	    }

	    if(curoffset < term.t_ncol){
		     movecursor(curline, curoffset);
		     peeol();
	    }
	    curline++;
	    curindex = curoffset = 0;
	    if(curline >= BOTTOM())
	      break;

	    lp = lp->next;
        }

	if(curline >= BOTTOM())
	  return;				/* don't paint delimiter */

	while(headents[++e].name != NULL)
	  if(headents[e].display_it){
	      lp = headents[e].hd_text;
	      break;
	  }
    }

    display_delimiter(ComposerEditing ? 0 : 1);
}



/*
 * PaintBody() - generic call to handle repainting everything BUT the 
 *		 header
 *
 *	notes:
 *		The header redrawing in a level 0 body paint gets done
 *		in update()
 */
void
PaintBody(int level)
{
    curwp->w_flag |= WFHARD;			/* make sure framing's right */
    if(level == 0)				/* specify what to update */
        sgarbf = TRUE;

    update();					/* display message body */

    if(level == 0 && ComposerEditing){
	mlerase();				/* clear the error line */
	ShowPrompt();
    }
}


/*
 * display_for_send - paint the composer from the top line and return.
 */
void
display_for_send(void)
{
    int		     i = 0;
    struct hdr_line *l;

    /* if first header line isn't displayed, there's work to do */
    if(headents && ((l = first_hline(&i)) != ods.top_l
		    || ComposerTopLine == COMPOSER_TOP_LINE
		    || !ods.p_line)){
	struct on_display orig_ods;
	int		  orig_edit    = ComposerEditing,
			  orig_ct_line = ComposerTopLine;

	/*
	 * fake that the cursor's in the first header line
	 * and force repaint...
	 */
	orig_ods	= ods;
	ods.cur_e	= i;
	ods.top_l	= ods.cur_l = l;
	ods.top_e	= ods.cur_e;
	ods.p_line	= COMPOSER_TOP_LINE;
	ComposerEditing = TRUE;			/* to fool update() */
	setimark(FALSE, 1);			/* remember where we were */
	gotobob(FALSE, 1);

	UpdateHeader(0);			/* redraw whole enchilada */
	PaintHeader(COMPOSER_TOP_LINE, TRUE);
	PaintBody(0);

	ods = orig_ods;				/* restore original state */
	ComposerEditing = orig_edit;
	ComposerTopLine = curwp->w_toprow = orig_ct_line;
        curwp->w_ntrows = BOTTOM() - ComposerTopLine;
	swapimark(FALSE, 1);

	/* in case we don't exit, set up restoring the screen */
	sgarbf = TRUE;				/* force redraw */
    }
}


/*
 * ArrangeHeader - set up display parm such that header is reasonably 
 *                 displayed
 */
void
ArrangeHeader(void)
{
    int      e;
    register struct hdr_line *l;

    ods.p_line = ods.p_ind = 0;
    e = ods.top_e = 0;
    l = ods.top_l = headents[e].hd_text;
    while(headents[e+1].name || (l && l->next))
      if((l = next_sel_hline(&e, l)) != NULL){
	  ods.cur_l = l;
	  ods.cur_e = e;
      }

    UpdateHeader(1);
}


/*
 * ComposerHelp() - display mail help in a context sensitive way
 *                  based on the level passed ...
 */
int
ComposerHelp(int level)
{
    char buf[80];
    VARS_TO_SAVE *saved_state;

    curwp->w_flag |= WFMODE;
    sgarbf = TRUE;

    if(level < 0 || !headents[level].name){
	(*term.t_beep)();
	emlwrite("Sorry, I can't help you with that.", NULL);
	sleep(2);
	return(FALSE);
    }

    snprintf(buf, sizeof(buf), "Help for %s %.40s Field",
		 (Pmaster->pine_flags & MDHDRONLY) ? "Address Book"
						 : "Composer",
		 headents[level].name);
    saved_state = save_pico_state();
    (*Pmaster->helper)(headents[level].help, buf, 1);
    if(saved_state){
	restore_pico_state(saved_state);
	free_pico_state(saved_state);
    }

    ttresize();
    picosigs();					/* restore altered handlers */
    return(TRUE);
}



/*
 * ToggleHeader() - set or unset pico values to the full screen size
 *                  painting header if need be.
 */
int
ToggleHeader(int show)
{
    /*
     * check to see if we need to display the header... 
     */
    if(show){
	UpdateHeader(0);				/* figure bounds  */
	PaintHeader(COMPOSER_TOP_LINE, FALSE);	/* draw it */
    }
    else{
        /*
         * set bounds for no header display
         */
        curwp->w_toprow = ComposerTopLine = COMPOSER_TOP_LINE;
        curwp->w_ntrows = BOTTOM() - ComposerTopLine;
    }
    return(TRUE);
}



/*
 * HeaderLen() - return the length in lines of the exposed portion of the
 *               header
 */
int
HeaderLen(void)
{
    register struct hdr_line *lp;
    int      e;
    int      i;
    
    i = 1;
    lp = ods.top_l;
    e  = ods.top_e;
    while(lp != NULL){
	lp = next_hline(&e, lp);
	i++;
    }
    return(i);
}



/*
 * first_hline() - return a pointer to the first displayable header line
 * 
 *	returns:
 *		1) pointer to first displayable line in header and header
 *                 entry, via side effect, that the first line is a part of
 *              2) NULL if no next line, leaving entry at LASTHDR
 */
struct hdr_line *
first_hline(int *entry)
{
    /* init *entry so we're sure to start from the top */
    for(*entry = 0; headents[*entry].name; (*entry)++)
      if(headents[*entry].display_it)
	return(headents[*entry].hd_text);

    *entry = 0;
    return(NULL);		/* this shouldn't happen */
}


/*
 * first_sel_hline() - return a pointer to the first selectable header line
 * 
 *	returns:
 *		1) pointer to first selectable line in header and header
 *                 entry, via side effect, that the first line is a part of
 *              2) NULL if no next line, leaving entry at LASTHDR
 */
struct hdr_line *
first_sel_hline(int *entry)
{
    /* init *entry so we're sure to start from the top */
    for(*entry = 0; headents[*entry].name; (*entry)++)
      if(headents[*entry].display_it && !headents[*entry].blank)
	return(headents[*entry].hd_text);

    *entry = 0;
    return(NULL);		/* this shouldn't happen */
}



/*
 * next_hline() - return a pointer to the next line structure
 * 
 *	returns:
 *		1) pointer to next displayable line in header and header
 *                 entry, via side effect, that the next line is a part of
 *              2) NULL if no next line, leaving entry at LASTHDR
 */
struct hdr_line *
next_hline(int *entry, struct hdr_line *line)
{
    if(line == NULL)
      return(NULL);

    if(line->next == NULL){
	while(headents[++(*entry)].name != NULL){
	    if(headents[*entry].display_it)
	      return(headents[*entry].hd_text);
	}
	--(*entry);
	return(NULL);
    }
    else
      return(line->next);
}


/*
 * next_sel_hline() - return a pointer to the next selectable line structure
 * 
 *	returns:
 *		1) pointer to next selectable line in header and header
 *                 entry, via side effect, that the next line is a part of
 *              2) NULL if no next line, leaving entry at LASTHDR
 */
struct hdr_line *
next_sel_hline(int *entry, struct hdr_line *line)
{
    if(line == NULL)
      return(NULL);

    if(line->next == NULL){
	while(headents[++(*entry)].name != NULL){
	    if(headents[*entry].display_it && !headents[*entry].blank)
	      return(headents[*entry].hd_text);
	}
	--(*entry);
	return(NULL);
    }
    else
      return(line->next);
}



/*
 * prev_hline() - return a pointer to the next line structure back
 * 
 *	returns:
 *              1) pointer to previous displayable line in header and 
 *                 the header entry that the next line is a part of 
 *                 via side effect
 *              2) NULL if no next line, leaving entry unchanged from
 *                 the value it had on entry.
 */
struct hdr_line *
prev_hline(int *entry, struct hdr_line *line)
{
    if(line == NULL)
      return(NULL);

    if(line->prev == NULL){
	int orig_entry;

	orig_entry = *entry;
	while(--(*entry) >= 0){
	    if(headents[*entry].display_it){
		line = headents[*entry].hd_text;
		while(line->next != NULL)
		  line = line->next;
		return(line);
	    }
	}

	*entry = orig_entry;
	return(NULL);
    }
    else
      return(line->prev);
}


/*
 * prev_sel_hline() - return a pointer to the previous selectable line
 * 
 *	returns:
 *              1) pointer to previous selectable line in header and 
 *                 the header entry that the next line is a part of 
 *                 via side effect
 *              2) NULL if no next line, leaving entry unchanged from
 *                 the value it had on entry.
 */
struct hdr_line *
prev_sel_hline(int *entry, struct hdr_line *line)
{
    if(line == NULL)
      return(NULL);

    if(line->prev == NULL){
	int orig_entry;

	orig_entry = *entry;
	while(--(*entry) >= 0){
	    if(headents[*entry].display_it && !headents[*entry].blank){
		line = headents[*entry].hd_text;
		while(line->next != NULL)
		  line = line->next;
		return(line);
	    }
	}

	*entry = orig_entry;
	return(NULL);
    }
    else
      return(line->prev);
}



/*
 * first_requested_hline() - return pointer to first line that pico's caller
 *			     asked that we start on.
 */
struct hdr_line *
first_requested_hline(int *ent)
{
    int		     i, reqfield;
    struct hdr_line *rv = NULL;

    for(reqfield = -1, i = 0; headents[i].name;  i++)
      if(headents[i].start_here){
	  headents[i].start_here = 0;		/* clear old setting */
	  if(reqfield < 0){			/* if not already, set up */
	      headents[i].display_it = 1;	/* make sure it's shown */
	      *ent = reqfield = i;
	      rv = headents[i].hd_text;
	  }
      }

    return(rv);
}



/*
 * UpdateHeader() - determines the best range of lines to be displayed 
 *                  using the global ods value for the current line and the
 *		    top line, also sets ComposerTopLine and pico limits
 *
 *	showtop -- Attempt to show all header lines if they'll fit.
 *                    
 *      notes:
 *	        This is pretty ugly because it has to keep the current line
 *		on the screen in a reasonable location no matter what.
 *		There are also a couple of rules to follow:
 *                 1) follow paging conventions of pico (ie, half page 
 *		      scroll)
 *                 2) if more than one page, always display last half when 
 *                    pline is toward the end of the header
 * 
 *      returns:
 *             TRUE  if anything changed (side effects: new p_line, top_l
 *		     top_e, and pico parms)
 *             FALSE if nothing changed 
 *             
 */
int
UpdateHeader(int showtop)
{
    register struct	hdr_line	*lp;
    int	     i, le;
    int      ret = FALSE;
    int      old_top = ComposerTopLine;
    int      old_p = ods.p_line;

    if(ods.p_line < COMPOSER_TOP_LINE ||
       ((ods.p_line == ComposerTopLine-2) ? 2: 0) + ods.p_line >= BOTTOM()){
        /* NewTop if cur header line is at bottom of screen or two from */
        /* the bottom of the screen if cur line is bottom header line */
	NewTop(showtop);			/* get new top_l */
	ret = TRUE;
    }
    else{					/* make sure p_line's OK */
	i = COMPOSER_TOP_LINE;
	lp = ods.top_l;
	le = ods.top_e;
	while(lp != ods.cur_l){
	    /*
	     * this checks to make sure cur_l is below top_l and that
	     * cur_l is on the screen...
	     */
	    if((lp = next_hline(&le, lp)) == NULL || ++i >= BOTTOM()){
		NewTop(0);
		ret = TRUE;
		break;
	    }
	}
    }

    ods.p_line = COMPOSER_TOP_LINE;		/* find  p_line... */
    lp = ods.top_l;
    le = ods.top_e;
    while(lp && lp != ods.cur_l){
	lp = next_hline(&le, lp);
	ods.p_line++;
    }

    if(!ret)
      ret = !(ods.p_line == old_p);

    ComposerTopLine = ods.p_line;		/* figure top composer line */
    while(lp && ComposerTopLine <= BOTTOM()){
	lp = next_hline(&le, lp);
	ComposerTopLine += (lp) ? 1 : 2;	/* allow for delim at end   */
    }

    if(!ret)
      ret = !(ComposerTopLine == old_top);

    if(wheadp->w_toprow != ComposerTopLine){	/* update pico params... */
        wheadp->w_toprow = ComposerTopLine;
        wheadp->w_ntrows = ((i = BOTTOM() - ComposerTopLine) > 0) ? i : 0;
	ret = TRUE;
    }
    return(ret);
}



/*
 * NewTop() - calculate a new top_l based on the cur_l
 *
 *	showtop -- Attempt to show all the header lines if they'll fit
 *
 *	returns:
 *		with ods.top_l and top_e pointing at a reasonable line
 *		entry
 */
void
NewTop(int showtop)
{
    register struct hdr_line *lp;
    register int i;
    int      e;

    lp = ods.cur_l;
    e  = ods.cur_e;
    i  = showtop ? FULL_SCR() : HALF_SCR();

    while(lp != NULL && (--i > 0)){
	ods.top_l = lp;
	ods.top_e = e;
	lp = prev_hline(&e, lp);
    }
}



/*
 * display_delimiter() - just paint the header/message body delimiter with
 *                       inverse value specified by state.
 */
void
display_delimiter(int state)
{
    UCS    *bufp, *buf;

    if(ComposerTopLine - 1 >= BOTTOM())		/* silently forget it */
      return;

    buf = utf8_to_ucs4_cpystr((gmode & MDHDRONLY) ? "" : HDR_DELIM);
    if(!buf)
      return;

    bufp = buf;

    if(state == delim_ps){			/* optimize ? */
	for(delim_ps = 0; bufp[delim_ps] && pscr(ComposerTopLine-1,delim_ps) != NULL && pscr(ComposerTopLine-1,delim_ps)->c == bufp[delim_ps];delim_ps++)
	  ;

	if(bufp[delim_ps] == '\0' && !(gmode & MDHDRONLY)){
	    delim_ps = state;
	    fs_give((void **) &buf);
	    return;				/* already displayed! */
	}
    }

    delim_ps = state;

    movecursor(ComposerTopLine - 1, 0);
    if(state)
      (*term.t_rev)(1);

    while(*bufp != '\0')
      pputc(*bufp++, state ? 1 : 0);

    if(state)
      (*term.t_rev)(0);

    peeol();
    fs_give((void **) &buf);
}



/*
 * InvertPrompt() - invert the prompt associated with header entry to state
 *                  state (true if invert, false otherwise).
 *	returns:
 *		non-zero if nothing done
 *		0 if prompt inverted successfully
 *
 *	notes:
 *		come to think of it, this func and the one above could
 *		easily be combined
 */
int
InvertPrompt(int entry, int state)
{
    UCS *buf, *bufp;
    UCS *end;
    int  i;

    buf = utf8_to_ucs4_cpystr(headents[entry].prompt);	/* fresh prompt paint */
    if(!buf)
      return(-1);

    bufp = buf;
    if((i = entry_line(entry, FALSE)) == -1){
      fs_give((void **) &buf);
      return(-1);				/* silently forget it */
    }

    end = buf + ucs4_strlen(buf);

    /*
     * Makes sure that the prompt doesn't take up more than prwid of screen space.
     * The caller should do that, too, in order to make it look right so
     * this should most likely be a no-op
     */
    if(ucs4_str_width_ptr_to_ptr(buf, end) > headents[entry].prwid){
	end = ucs4_particular_width(buf, headents[entry].prwid);
	*end = '\0';
    }

    if(entry < 16 && (invert_ps&(1<<entry)) 
       == (state ? 1<<entry : 0)){	/* optimize ? */
	int j;

	for(j = 0; bufp[j] && pscr(i, j)->c == bufp[j]; j++)
	  ;

	if(bufp[j] == '\0'){
	    if(state)
	      invert_ps |= 1<<entry;
	    else
	      invert_ps &= ~(1<<entry);

	    fs_give((void **) &buf);
	    return(0);				/* already displayed! */
	}
    }

    if(entry < 16){  /* if > 16, cannot be stored in invert_ps */
      if(state)
	invert_ps |= 1<<entry;
      else
	invert_ps &= ~(1<<entry);
    }

    movecursor(i, 0);
    if(state)
      (*term.t_rev)(1);

    while(*bufp && *(bufp + 1))
      pputc(*bufp++, 1);			/* putc upto last char */

    if(state)
      (*term.t_rev)(0);

    pputc(*bufp, 0);				/* last char not inverted */

    fs_give((void **) &buf);

    return(TRUE);
}



/*
 * partial_entries() - toggle display of the bcc and fcc fields.
 *
 *	returns:
 *		TRUE if there are partial entries on the display
 *		FALSE otherwise.
 */
int
partial_entries(void)
{
    register struct headerentry *h;
    int                          is_on;
  
    /*---- find out status of first rich header ---*/
    for(h = headents; !h->rich_header && h->name != NULL; h++)
      ;

    is_on = h->display_it;
    for(h = headents; h->name != NULL; h++) 
      if(h->rich_header) 
        h->display_it = ! is_on;

    return(is_on);
}



/*
 * entry_line() - return the physical line on the screen associated
 *                with the given header entry field.  Note: the field
 *                may span lines, so if the last char is set, return
 *                the appropriate value.
 *
 *	returns:
 *             1) physical line number of entry
 *             2) -1 if entry currently not on display
 */
int
entry_line(int entry, int lastchar)
{
    register int    p_line = COMPOSER_TOP_LINE;
    int    i;
    register struct hdr_line    *line;

    for(line = ods.top_l, i = ods.top_e;
	headents && headents[i].name && i <= entry;
	p_line++){
	if(p_line >= BOTTOM())
	  break;
	if(i == entry){
	    if(lastchar){
		if(line->next == NULL)
		  return(p_line);
	    }
	    else if(line->prev == NULL)
	      return(p_line);
	    else
	      return(-1);
	}
	line = next_hline(&i, line);
    }
    return(-1);
}



/*
 * physical_line() - return the physical line on the screen associated
 *                   with the given header line pointer.
 *
 *	returns:
 *             1) physical line number of entry
 *             2) -1 if entry currently not on display
 */
int
physical_line(struct hdr_line *l)
{
    register int    p_line = COMPOSER_TOP_LINE;
    register struct hdr_line    *lp;
    int    i;

    for(lp=ods.top_l, i=ods.top_e; headents[i].name && lp != NULL; p_line++){
	if(p_line >= BOTTOM())
	  break;

	if(lp == l)
	  return(p_line);

	lp = next_hline(&i, lp);
    }
    return(-1);
}



/*
 * call_builder() - resolve any nicknames in the address book associated
 *                  with the given entry...
 *
 *    NOTES:
 * 
 *      BEWARE: this function can cause cur_l and top_l to get lost so BE 
 *              CAREFUL before and after you call this function!!!
 * 
 *      There could to be something here to resolve cur_l and top_l
 *      reasonably into the new linked list for this entry.  
 *
 *      The reason this would mostly work without it is resolve_niks gets
 *      called for the most part in between fields.  Since we're moving
 *      to the beginning or end (i.e. the next/prev pointer in the old 
 *      freed cur_l is NULL) of the next entry, we get a new cur_l
 *      pointing at a good line.  Then since top_l is based on cur_l in
 *      NewTop() we have pretty much lucked out.
 * 
 *      Where we could get burned is in a canceled exit (ctrl|x).  Here
 *      nicknames get resolved into addresses, which invalidates cur_l
 *      and top_l.  Since we don't actually leave, we could begin editing
 *      again with bad pointers.  This would usually results in a nice 
 *      core dump.
 *
 *      NOTE: The mangled argument is a little strange. It's used on both
 *      input and output. On input, if it is not set, then that tells the
 *      builder not to do anything that might take a long time, like a
 *      white pages lookup. On return, it tells the caller that the screen
 *      and signals may have been mangled so signals should be reset, window
 *      resized, and screen redrawn.
 *
 *	RETURNS:
 *              > 0 if any names where resolved, otherwise
 *                0 if not, or
 *		< 0 on error
 *                -1: move to next line
 *                -2: don't move off this line
 */
int
call_builder(struct headerentry *entry, int *mangled, char **err)
{
    register    int     retval = 0;
    register	int	i;
    register    struct  hdr_line  *line;
    int          quoted = 0;
    int          sbuflen;
    char	*sbuf;
    char	*s = NULL;
    char        *tmp;
    struct headerentry *e;
    BUILDER_ARG *nextarg, *arg = NULL, *headarg = NULL;
    VARS_TO_SAVE *saved_state;

    if(!entry->builder)
      return(0);

    line = entry->hd_text;
    sbuflen = 0;
    while(line != NULL){
	sbuflen += (6*term.t_ncol);
        line = line->next;
    }
    
    if((sbuf=(char *)malloc(sbuflen * sizeof(*sbuf))) == NULL){
	emlwrite("Can't malloc space to expand address", NULL);
	return(-1);
    }
    
    *sbuf = '\0';

    /*
     * cat the whole entry into one string...
     */
    line = entry->hd_text;
    while(line != NULL){
	i = ucs4_strlen(line->text);

	/*
	 * To keep pine address builder happy, addresses should be separated
	 * by ", ".  Add this space if needed, otherwise...
	 * (This is some ancient requirement that is no longer needed.)
	 *
	 * If this line is NOT a continuation of the previous line, add
	 * white space for pine's address builder if its not already there...
	 * (This is some ancient requirement that is no longer needed.)
	 *
	 * Also if it's not a continuation (i.e., there's already and addr on 
	 * the line), and there's another line below, treat the new line as
	 * an implied comma.
	 * (This should only be done for address-type lines, not for regular
	 * text lines like subjects. Key off of the break_on_comma bit which
	 * should only be set on those that won't mind a comma being added.)
	 */
	if(entry->break_on_comma){
	    UCS *space, commaspace[3];

	    commaspace[0] = ',';
	    commaspace[1] = ' ';
	    commaspace[2] = '\0';
	    space = commaspace+1;

	    if(i && line->text[i-1] == ','){
	      ucs4_strncat(line->text, space, HLSZ-i-1);	/* help address builder */
	      line->text[HLSZ-1] = '\0';
	    }
	    else if(line->next != NULL && !strend(line->text, ',')){
		if(ucs4_strqchr(line->text, ',', &quoted, -1)){
		  ucs4_strncat(line->text, commaspace, HLSZ-i-1);	/* implied comma */
		  line->text[HLSZ-1] = '\0';
		}
	    }
	    else if(line->prev != NULL && line->next != NULL){
		if(ucs4_strchr(line->prev->text, ' ') != NULL 
		   && line->text[i-1] != ' '){
		  ucs4_strncat(line->text, space, HLSZ-i-1);
		  line->text[HLSZ-1] = '\0';
		}
	    }
	}

	tmp = ucs4_to_utf8_cpystr(line->text);
	if(tmp){
	    strncat(sbuf, tmp, sbuflen-strlen(sbuf)-1);
	    sbuf[sbuflen-1] = '\0';
	    fs_give((void **) &tmp);
	}

        line = line->next;
    }

    if(entry->affected_entry){
	/* check if any non-sticky affected entries */
	for(e = entry->affected_entry; e; e = e->next_affected)
	  if(!e->sticky)
	    break;

	/* there is at least one non-sticky so make a list to pass */
	if(e){
	    for(e = entry->affected_entry; e; e = e->next_affected){
		if(!arg){
		    headarg = arg = (BUILDER_ARG *)malloc(sizeof(BUILDER_ARG));
		    if(!arg){
			emlwrite("Can't malloc space for fcc", NULL);
			return(-1);
		    }
		    else{
			arg->next = NULL;
			arg->tptr = NULL;
			arg->aff  = &(e->bldr_private);
			arg->me   = &(entry->bldr_private);
		    }
		}
		else{
		    nextarg = (BUILDER_ARG *)malloc(sizeof(BUILDER_ARG));
		    if(!nextarg){
			emlwrite("Can't malloc space for fcc", NULL);
			return(-1);
		    }
		    else{
			nextarg->next = NULL;
			nextarg->tptr = NULL;
			nextarg->aff  = &(e->bldr_private);
			nextarg->me   = &(entry->bldr_private);
			arg->next     = nextarg;
			arg           = arg->next;
		    }
		}

		if(!e->sticky){
		    line = e->hd_text;
		    arg->tptr = ucs4_to_utf8_cpystr(line->text);
		}
	    }
	}
    }

    /*
     * Even if there are no affected entries, we still need the arg
     * to pass the "me" pointer.
     */
    if(!headarg){
	headarg = (BUILDER_ARG *)malloc(sizeof(BUILDER_ARG));
	if(!headarg){
	    emlwrite("Can't malloc space", NULL);
	    return(-1);
	}
	else{
	    headarg->next = NULL;
	    headarg->tptr = NULL;
	    headarg->aff  = NULL;
	    headarg->me   = &(entry->bldr_private);
	}
    }

    /*
     * The builder may make a new call back to pico() so we save and
     * restore the pico state.
     */
    saved_state = save_pico_state();
    retval = (*entry->builder)(sbuf, &s, err, headarg, mangled);
    if(saved_state){
	restore_pico_state(saved_state);
	free_pico_state(saved_state);
    }

    if(mangled && *mangled & BUILDER_MESSAGE_DISPLAYED){
	*mangled &= ~ BUILDER_MESSAGE_DISPLAYED;
	if(mpresf == FALSE)
	  mpresf = TRUE;
    }

    if(mangled && *mangled & BUILDER_FOOTER_MANGLED){
	*mangled &= ~ BUILDER_FOOTER_MANGLED;
	sgarbk = TRUE;
	pclear(term.t_nrow-1, term.t_nrow);
    }

    if(retval >= 0){
	if(strcmp(sbuf, s)){
	    line = entry->hd_text;
	    InitEntryText(s, entry);		/* arrange new one */
	    zotentry(line); 			/* blast old list o'entries */
	    entry->dirty = 1;			/* mark it dirty */
	    retval = 1;
	}

	for(e = entry->affected_entry, arg = headarg;
	    e;
	    e = e->next_affected, arg = arg ? arg->next : NULL){
	    if(!e->sticky){
		line = e->hd_text;
		tmp = ucs4_to_utf8_cpystr(line->text);
		if(strcmp(tmp, arg ? arg->tptr : "")){ /* it changed */
		    /* make sure they see it if changed */
		    e->display_it = 1;
		    InitEntryText(arg ? arg->tptr : "", e);
		    if(line == ods.top_l)
		      ods.top_l = e->hd_text;

		    zotentry(line);	/* blast old list o'entries */
		    e->dirty = 1;	/* mark it dirty */
		    retval = 1;
		}

		if(tmp)
		  fs_give((void **) &tmp);
	    }
	}
    }

    if(s)
      free(s);

    for(arg = headarg; arg; arg = nextarg){
	/* Don't free xtra or me, they just point to headerentry data */
	nextarg = arg->next;
	if(arg->tptr)
	  free(arg->tptr);
	
	free(arg);
    }

    free(sbuf);
    return(retval);
}


void
call_expander(void)
{
    char        **s = NULL;
    VARS_TO_SAVE *saved_state;
    int           expret;

    if(!Pmaster->expander)
      return;

    /*
     * Since expander may make a call back to pico() we need to
     * save and restore pico state.
     */
    if((saved_state = save_pico_state()) != NULL){

	expret = (*Pmaster->expander)(headents, &s);

	restore_pico_state(saved_state);
	free_pico_state(saved_state);
	ttresize();
	picosigs();

	if(expret > 0 && s){
	    char               *tbuf, *p;
	    int                 i, biggest = 100;
	    struct headerentry *e;

	    /*
	     * Use tbuf to cat together multiple line entries before comparing.
	     */
	    tbuf = (char *)malloc((biggest+1) * sizeof(*tbuf));
	    for(e = headents, i=0; e->name != NULL; e++,i++){
		int sz = 0;
		struct hdr_line *line;

		while(e->name && e->blank)
		  e++;
		
		if(e->name == NULL)
		  continue;

		for(line = e->hd_text; line != NULL; line = line->next){
		  p = ucs4_to_utf8_cpystr(line->text);
		  if(p){
		    sz += strlen(p);
		    fs_give((void **) &p);
		  }
		}
		
		if(sz > biggest){
		    biggest = sz;
		    free(tbuf);
		    tbuf = (char *)malloc((biggest+1) * sizeof(*tbuf));
		}

		tbuf[0] = '\0';
		for(line = e->hd_text; line != NULL; line = line->next){
		  p = ucs4_to_utf8_cpystr(line->text);
		  if(p){
		    strncat(tbuf, p, biggest+1-strlen(tbuf)-1);
		    tbuf[biggest] = '\0';
		    fs_give((void **) &p);
		  }
		}
		  
		if(strcmp(tbuf, s[i])){ /* it changed */
		    struct hdr_line *zline;

		    line = zline = e->hd_text;
		    InitEntryText(s[i], e);

		    /*
		     * If any of the lines for this entry are current or
		     * top, fix that.
		     */
		    for(; line != NULL; line = line->next){
			if(line == ods.top_l)
			  ods.top_l = e->hd_text;

			if(line == ods.cur_l)
			  ods.cur_l = e->hd_text;
		    }

		    zotentry(zline);	/* blast old list o'entries */
		}
	    }

	    free(tbuf);
	}

	if(s){
	    char **p;

	    for(p = s; *p; p++)
	      free(*p);

	    free(s);
	}
    }

    return;
}


/*
 * strend - neglecting white space, returns TRUE if c is at the
 *          end of the given line.  otherwise FALSE.
 */
int
strend(UCS *s, UCS ch)
{
    UCS *b;

    if(s == NULL || *s == '\0')
      return(FALSE);

    for(b = &s[ucs4_strlen(s)] - 1; *b && ucs4_isspace(*b); b--){
	if(b == s)
	  return(FALSE);
    }

    return(*b == ch);
}


/*
 * ucs4_strqchr - returns pointer to first non-quote-enclosed occurance of ch in 
 *           the given string.  otherwise NULL.
 *      s -- the string
 *     ch -- the character we're looking for
 *      q -- q tells us if we start out inside quotes on entry and is set
 *           correctly on exit.
 *      m -- max characters we'll check for ch (set to -1 for no max)
 */
UCS *
ucs4_strqchr(UCS *s, UCS ch, int *q, int m)
{
    int	 quoted = (q) ? *q : 0;

    for(; s && *s && m != 0; s++, m--){
	if(*s == '"'){
	    quoted = !quoted;
	    if(q)
	      *q = quoted;
	}

	if(!quoted && *s == ch)
	  return(s);
    }

    return(NULL);
}


/*
 * KillHeaderLine() - kill a line in the header
 *
 *	notes:
 *		This is pretty simple.  Just using the emacs kill buffer
 *		and its accompanying functions to cut the text from lines.
 *
 *	returns:
 *		TRUE if hldelete worked
 *		FALSE otherwise
 */
int
KillHeaderLine(struct hdr_line *l, int append)
{
    UCS	*c;
    int i = ods.p_ind;
    int nl = TRUE;

    if(!append)
	kdelete();

    c = l->text;
    if (gmode & MDDTKILL){
	if (c[i] == '\0')  /* don't insert a new line after this line*/
	  nl = FALSE;
        /*put to be deleted part into kill buffer */
	for (i=ods.p_ind; c[i] != '\0'; i++) 
	  kinsert(c[i]);                       
    }else{
	while(*c != '\0')				/* splat out the line */
	  kinsert(*c++);
    }

    if (nl)
        kinsert('\n');				/* helpful to yank in body */

#ifdef _WINDOWS
    mswin_killbuftoclip (kremove);
#endif

    if (gmode & MDDTKILL){
	if (l->text[0]=='\0'){

	   if(l->next && l->prev)
	      ods.cur_l = next_hline(&ods.cur_e, l);
	   else if(l->prev)
	      ods.cur_l = prev_hline(&ods.cur_e, l);

	   if(l == ods.top_l)
	      ods.top_l = ods.cur_l;

	   return(hldelete(l));
	}
	else {
	  l->text[ods.p_ind]='\0';    /* delete part of the line from the cursor */
	  return(TRUE);
	}
    }else{
	if(l->next && l->prev)
	   ods.cur_l = next_hline(&ods.cur_e, l);
	else if(l->prev)
	   ods.cur_l = prev_hline(&ods.cur_e, l);

	if(l == ods.top_l)
	   ods.top_l = ods.cur_l;

        return(hldelete(l));			/* blast it  */
    }
}



/*
 * SaveHeaderLines() - insert the saved lines in the list before the 
 *                     current line in the header
 *
 *	notes:
 *		Once again, just using emacs' kill buffer and its 
 *              functions.
 *
 *	returns:
 *		TRUE if something good happend
 *		FALSE otherwise
 */
int
SaveHeaderLines(void)
{
    UCS     *buf;			/* malloc'd copy of buffer */
    UCS     *bp;			/* pointer to above buffer */
    register unsigned	i;			/* index */
    UCS     *work_buf, *work_buf_begin;
    char     empty[1];
    int      len, buf_len, work_buf_len, tentative_p_ind = 0;
    struct hdr_line *travel, *tentative_cur_l = NULL;

    if(ksize()){
	if((bp = buf = (UCS *) malloc((ksize()+5) * sizeof(*buf))) == NULL){
	    emlwrite("Can't malloc space for saved text", NULL);
	    return(FALSE);
	}
    }
    else
      return(FALSE);

    for(i=0; i < ksize(); i++)
      if(kremove(i) != '\n')			/* filter out newlines */
	*bp++ = (UCS) kremove(i);

    *bp = '\0';

    while(--bp >= buf)				/* kill trailing white space */
      if(*bp != ' '){
	  if(ods.cur_l->text[0] != '\0'){
	      if(*bp == '>'){			/* inserting an address */
		  *++bp = ',';			/* so add separator */
		  *++bp = '\0';
	      }
	  }
	  else{					/* nothing in field yet */
	      if(*bp == ','){			/* so blast any extra */
		  *bp = '\0';			/* separators */
	      }
	  }
	  break;
      }

    /* insert new text at the dot position */
    buf_len      = ucs4_strlen(buf);
    tentative_p_ind = ods.p_ind + buf_len;
    work_buf_len = ucs4_strlen(ods.cur_l->text) + buf_len;
    work_buf = (UCS *) malloc((work_buf_len + 1) * sizeof(UCS));
    if (work_buf == NULL) {
	emlwrite("Can't malloc space for saved text", NULL);
	return(FALSE);
    }

    work_buf[0] = '\0';
    work_buf_begin = work_buf;
    i = MIN(ods.p_ind, work_buf_len);
    ucs4_strncpy(work_buf, ods.cur_l->text, i);
    work_buf[i] = '\0';
    ucs4_strncat(work_buf, buf, work_buf_len+1-ucs4_strlen(work_buf)-1);
    work_buf[work_buf_len] = '\0';
    ucs4_strncat(work_buf, &ods.cur_l->text[ods.p_ind], work_buf_len+1-ucs4_strlen(work_buf)-1);
    work_buf[work_buf_len] = '\0';
    empty[0]='\0';
    ods.p_ind = 0;

    i = TRUE;

    /* insert text in HLSZ character chunks */
    while(work_buf_len + ods.p_ind > HLSZ) {
	ucs4_strncpy(&ods.cur_l->text[ods.p_ind], work_buf, HLSZ-ods.p_ind);
	work_buf += (HLSZ - ods.p_ind);
	work_buf_len -= (HLSZ - ods.p_ind);

	if(FormatLines(ods.cur_l, empty, LINEWID(),
		       headents[ods.cur_e].break_on_comma, 0) == -1) {
	   i = FALSE;
	   break;
	} else {
	   i = TRUE;
	  len = 0;
	  travel = ods.cur_l;
	  while (len < HLSZ){
	      len += ucs4_strlen(travel->text);
	      if (len >= HLSZ)
		break;

	      /*
	       * This comes after the break above because it will
	       * be accounted for in the while loop below.
	       */
	      if(!tentative_cur_l){
		  if(tentative_p_ind <= ucs4_strlen(travel->text))
		    tentative_cur_l = travel;
		  else
		    tentative_p_ind -= ucs4_strlen(travel->text);
	      }

	      travel = travel->next;
	  }

	  ods.cur_l = travel;
	  ods.p_ind = ucs4_strlen(travel->text) - len + HLSZ;
	}
    }

    /* insert the remainder of text */
    if (i != FALSE && work_buf_len > 0) {
	ucs4_strncpy(&ods.cur_l->text[ods.p_ind], work_buf, HLSZ-ods.p_ind);
	ods.cur_l->text[HLSZ-1] = '\0';
	work_buf = work_buf_begin;
	free(work_buf);

	if(FormatLines(ods.cur_l, empty, LINEWID(),
		       headents[ods.cur_e].break_on_comma, 0) == -1) {
	   i = FALSE;
	} else {  
	  len = 0;
	  travel = ods.cur_l;
	  while (len < work_buf_len + ods.p_ind){
	      if(!tentative_cur_l){
		  if(tentative_p_ind <= ucs4_strlen(travel->text))
		    tentative_cur_l = travel;
		  else
		    tentative_p_ind -= ucs4_strlen(travel->text);
	      }

	      len += ucs4_strlen(travel->text);
	      if (len >= work_buf_len + ods.p_ind)
		break;

	      travel = travel->next;
	  }

	  ods.cur_l = travel;
	  ods.p_ind = ucs4_strlen(travel->text) - len + work_buf_len + ods.p_ind;
	  if(tentative_cur_l
	     && tentative_p_ind >= 0
	     && tentative_p_ind <= ucs4_strlen(tentative_cur_l->text)){
	      ods.cur_l = tentative_cur_l;
	      ods.p_ind = tentative_p_ind;
	  }
	}
    }

    free(buf);
    return(i);
}



/*
 * break_point - Break the given line at the most reasonable character breakch
 *               within maxwid max characters.
 *
 *	returns:
 *		Pointer to the best break point in s, or
 *		Pointer to the beginning of s if no break point found
 */
UCS *
break_point(UCS *line, int maxwid, UCS breakch, int *quotedarg)
{
    UCS           *bp;		/* break point */
    int            quoted;

    /*
     * Start at maxwid and work back until first opportunity to break.
     */
    bp = ucs4_particular_width(line, maxwid);

    /*
     * Quoted should be set up for the start of line. Since we want
     * to move to bp and work our way back we need to scan through the
     * line up to bp setting quoted appropriately.
     */
    if(quotedarg)
      ucs4_strqchr(line, '\0', quotedarg, bp-line);

    quoted = quotedarg ? *quotedarg : 0;

    while(bp != line){
	if(breakch == ',' && *bp == '"')	/* don't break on quoted ',' */
	  quoted = !quoted;			/* toggle quoted state */

	if(*bp == breakch && !quoted){
	    if(breakch == ' '){
		if(ucs4_str_width_ptr_to_ptr(line, bp+1) < maxwid){
		    bp++;			/* leave the ' ' */
		    break;
		}
	    }
	    else{
		/*
		 * if break char isn't a space, leave a space after
		 * the break char.
		 */
		if(!(ucs4_str_width_ptr_to_ptr(line, bp+1) >= maxwid
		     || (bp[1] == ' ' && ucs4_str_width_ptr_to_ptr(line, bp+2) >= maxwid))){
		    bp += (bp[1] == ' ') ? 2 : 1;
		    break;
		}
	    }
	}

	bp--;
    }

    if(quotedarg)
      *quotedarg = quoted;

    return((quoted) ? line : bp);
}




/*
 * hldelete() - remove the header line pointed to by l from the linked list
 *              of lines.
 *
 *	notes:
 *		the case of first line in field is kind of bogus.  since
 *              the array of headers has a pointer to the first line, and 
 *		i don't want to worry about this too much, i just copied 
 *		the line below and removed it rather than the first one
 *		from the list.
 *
 *	returns:
 *		TRUE if it worked 
 *		FALSE otherwise
 */
int
hldelete(struct hdr_line *l)
{
    register struct hdr_line *lp;

    if(l == NULL)
      return(FALSE);

    if(l->next == NULL && l->prev == NULL){	/* only one line in field */
	l->text[0] = '\0';
	return(TRUE);				/* no free only line in list */
    }
    else if(l->next == NULL){			/* last line in field */
	l->prev->next = NULL;
    }
    else if(l->prev == NULL){			/* first line in field */
	ucs4_strncpy(l->text, l->next->text, HLSZ);
	l->text[HLSZ-1] = '\0';
	lp = l->next;
	if((l->next = lp->next) != NULL)
	  l->next->prev = l;
	l = lp;
    }
    else{					/* some where in field */
	l->prev->next = l->next;
	l->next->prev = l->prev;
    }

    l->next = NULL;
    l->prev = NULL;
    free((char *)l);
    return(TRUE);
}



/*
 * is_blank - returns true if the next n chars from coordinates row, col
 *           on display are spaces
 */
int
is_blank(int row, int col, int n)
{
    n += col;
    for( ;col < n; col++){
        if(pscr(row, col) == NULL || pscr(row, col)->c != ' ')
	  return(0);
    }
    return(1);
}


/*
 * ShowPrompt - display key help corresponding to the current header entry
 */
void
ShowPrompt(void)
{
    if(headents[ods.cur_e].key_label){
	menu_header[TO_KEY].name  = "^T";
	menu_header[TO_KEY].label = headents[ods.cur_e].key_label;
	KS_OSDATASET(&menu_header[TO_KEY], KS_OSDATAGET(&headents[ods.cur_e]));
    }
    else
      menu_header[TO_KEY].name  = NULL;
    
    if(Pmaster && Pmaster->exit_label)
      menu_header[SEND_KEY].label = Pmaster->exit_label;
    else if(gmode & (MDVIEW | MDHDRONLY))
      menu_header[SEND_KEY].label =  (gmode & MDHDRONLY) ? "eXit/Save" : "eXit";
    else
      menu_header[SEND_KEY].label = N_("Send");

    if(gmode & MDVIEW){
	menu_header[CUT_KEY].name  = NULL;
	menu_header[DEL_KEY].name  = NULL;
	menu_header[UDEL_KEY].name = NULL;
    }
    else{
	menu_header[CUT_KEY].name  = "^K";
	menu_header[DEL_KEY].name  = "^D";
	menu_header[UDEL_KEY].name = "^U";
    }

    if(Pmaster->ctrlr_label){
	menu_header[RICH_KEY].label = Pmaster->ctrlr_label;
	menu_header[RICH_KEY].name = "^R";
    }
    else if(gmode & MDHDRONLY){
	menu_header[RICH_KEY].name  = NULL;
    }
    else{
	menu_header[RICH_KEY].label = N_("Rich Hdr");
	menu_header[RICH_KEY].name  = "^R";
    }

    if(gmode & MDHDRONLY){
	if(headents[ods.cur_e].fileedit){
	    menu_header[PONE_KEY].name  = "^_";
	    menu_header[PONE_KEY].label   = N_("Edit File");
	}
	else
	  menu_header[PONE_KEY].name  = NULL;

	menu_header[ATT_KEY].name   = NULL;
    }
    else{
	menu_header[PONE_KEY].name  = "^O";
	menu_header[PONE_KEY].label = N_("Postpone");

	menu_header[ATT_KEY].name   = "^J";
    }

    wkeyhelp(menu_header);
}


/*
 * packheader - packup all of the header fields for return to caller. 
 *              NOTE: all of the header info passed in, including address
 *                    of the pointer to each string is contained in the
 *                    header entry array "headents".
 */
int
packheader(void)
{
    register int	i = 0;		/* array index */
    register int	count;		/* count of chars in a field */
    register int	retval = TRUE;
    register char	*bufp;
    register struct	hdr_line *line;
    char               *p;

    if(!headents)
      return(TRUE);

    while(headents[i].name != NULL){
#ifdef	ATTACHMENTS
	/*
	 * attachments are special case, already in struct we pass back
	 */
	if(headents[i].is_attach){
	    i++;
	    continue;
	}
#endif

	if(headents[i].blank){
	    i++;
	    continue;
	}

        /*
         * count chars to see if we need a new malloc'd space for our
         * array.
         */
        line = headents[i].hd_text;
        count = 0;
        while(line != NULL){
            /*
             * add one for possible concatination of a ' ' character ...
             */
	    p = ucs4_to_utf8_cpystr(line->text);
	    if(p){
		count += strlen(p);
		if(p[0] && p[strlen(p)-1] == ',')
		  count++;

		fs_give((void **) &p);
	    }

            line = line->next;
        }

        line = headents[i].hd_text;
        if(count <= headents[i].maxlen){		
            *headents[i].realaddr[0] = '\0';
        }
        else{
            /*
             * don't forget to include space for the null terminator!!!!
             */
            if((bufp = (char *)malloc((count+1) * sizeof(char))) != NULL){
                *bufp = '\0';

                free(*headents[i].realaddr);
                *headents[i].realaddr = bufp;
                headents[i].maxlen = count;
            }
            else{
                emlwrite("Can't make room to pack header field.", NULL);
                retval = FALSE;
            }
        }

        if(retval != FALSE){
	    int saw_current_line = 0;

	    while(line != NULL){

		/* pass the cursor offset back in Pmaster struct */
		if(headents[i].start_here && Pmaster && !saw_current_line){
		    if(ods.cur_l == line)
		      saw_current_line++;
		    else
		      Pmaster->edit_offset += ucs4_strlen(line->text);
		}

		p = ucs4_to_utf8_cpystr(line->text);
		if(p){
		    strncat(*headents[i].realaddr, p, headents[i].maxlen+1-strlen(*headents[i].realaddr)-1);
		    (*headents[i].realaddr)[headents[i].maxlen] = '\0';

		    if(p[0] && p[strlen(p)-1] == ','){
			strncat(*headents[i].realaddr, " ", headents[i].maxlen+1-strlen(*headents[i].realaddr)-1);
			(*headents[i].realaddr)[headents[i].maxlen] = '\0';
		    }

		    fs_give((void **) &p);
		}

                line = line->next;
            }
        }

        i++;
    }

    return(retval);    
}



/*
 * zotheader - free all malloc'd lines associated with the header structs
 */
void
zotheader(void)
{
    register struct headerentry *i;
  
    for(i = headents; headents && i->name; i++)
      zotentry(i->hd_text);
}


/*
 * zotentry - free malloc'd space associated with the given linked list
 */
void
zotentry(struct hdr_line *l)
{
    register struct hdr_line *ld, *lf = l;

    while((ld = lf) != NULL){
	lf = ld->next;
	ld->next = ld->prev = NULL;
	free((char *) ld);
    }
}



/*
 * zotcomma - blast any trailing commas and white space from the end 
 *	      of the given line
 */
int
zotcomma(UCS *s)
{
    UCS *p;
    int retval = FALSE;

    p = &s[ucs4_strlen(s)];
    while(--p >= s){
	if(*p != ' '){
	    if(*p == ','){
		*p = '\0';
		retval = TRUE;
	    }

	    return(retval);
	}
    }

    return(retval);
}


/*
 * Save the current state of global variables so that we can restore
 * them later. This is so we can call pico again.
 * Also have to initialize some variables that normally would be set to
 * zero on startup.
 */
VARS_TO_SAVE *
save_pico_state(void)
{
    VARS_TO_SAVE  *ret;
    extern int     vtrow;
    extern int     vtcol;
    extern int     lbound;
    extern VIDEO **vscreen;
    extern VIDEO **pscreen;
    extern int     pico_all_done;
    extern jmp_buf finstate;
    extern UCS    *pico_anchor;

    if((ret = (VARS_TO_SAVE *)malloc(sizeof(VARS_TO_SAVE))) == NULL)
      return(ret);

    ret->vtrow = vtrow;
    ret->vtcol = vtcol;
    ret->lbound = lbound;
    ret->vscreen = vscreen;
    ret->pscreen = pscreen;
    ret->ods = ods;
    ret->delim_ps = delim_ps;
    ret->invert_ps = invert_ps;
    ret->pico_all_done = pico_all_done;
    memcpy(ret->finstate, finstate, sizeof(jmp_buf));
    ret->pico_anchor = pico_anchor;
    ret->Pmaster = Pmaster;
    ret->fillcol = fillcol;
    if((ret->pat = (UCS *)malloc(sizeof(UCS) * (ucs4_strlen(pat)+1))) != NULL)
      ucs4_strncpy(ret->pat, pat, ucs4_strlen(pat)+1);

    ret->ComposerTopLine = ComposerTopLine;
    ret->ComposerEditing = ComposerEditing;
    ret->gmode = gmode;
    ret->alt_speller = alt_speller;
    ret->quote_str = glo_quote_str;
    ret->wordseps = glo_wordseps;
    ret->currow = currow;
    ret->curcol = curcol;
    ret->thisflag = thisflag;
    ret->lastflag = lastflag;
    ret->curgoal = curgoal;
    ret->opertree = (char *) malloc(sizeof(char) * (strlen(opertree) + 1));
    if(ret->opertree != NULL)
      strncpy(ret->opertree, opertree, strlen(opertree)+1);

    ret->curwp = curwp;
    ret->wheadp = wheadp;
    ret->curbp = curbp;
    ret->bheadp = bheadp;
    ret->km_popped = km_popped;
    ret->mrow = term.t_mrow;

    /* Initialize for next pico call */
    wheadp = NULL;
    curwp = NULL;
    bheadp = NULL;
    curbp = NULL;

    return(ret);
}


void
restore_pico_state(VARS_TO_SAVE *state)
{
    extern int     vtrow;
    extern int     vtcol;
    extern int     lbound;
    extern VIDEO **vscreen;
    extern VIDEO **pscreen;
    extern int     pico_all_done;
    extern jmp_buf finstate;
    extern UCS    *pico_anchor;

    clearcursor();
    vtrow = state->vtrow;
    vtcol = state->vtcol;
    lbound = state->lbound;
    vscreen = state->vscreen;
    pscreen = state->pscreen;
    ods = state->ods;
    delim_ps = state->delim_ps;
    invert_ps = state->invert_ps;
    pico_all_done = state->pico_all_done;
    memcpy(finstate, state->finstate, sizeof(jmp_buf));
    pico_anchor = state->pico_anchor;
    Pmaster = state->Pmaster;
    if(Pmaster)
      headents = Pmaster->headents;

    fillcol = state->fillcol;
    if(state->pat)
      ucs4_strncpy(pat, state->pat, NPAT);

    ComposerTopLine = state->ComposerTopLine;
    ComposerEditing = state->ComposerEditing;
    gmode = state->gmode;
    alt_speller = state->alt_speller;
    glo_quote_str = state->quote_str;
    glo_wordseps = state->wordseps;
    currow = state->currow;
    curcol = state->curcol;
    thisflag = state->thisflag;
    lastflag = state->lastflag;
    curgoal = state->curgoal;
    if(state->opertree){
      strncpy(opertree, state->opertree, sizeof(opertree));
      opertree[sizeof(opertree)-1] = '\0';
    }

    curwp = state->curwp;
    wheadp = state->wheadp;
    curbp = state->curbp;
    bheadp = state->bheadp;
    km_popped = state->km_popped;
    term.t_mrow = state->mrow;
}


void
free_pico_state(VARS_TO_SAVE *state)
{
    if(state->pat)
      free(state->pat);

    if(state->opertree)
      free(state->opertree);

    free(state);
}


/*
 * Ok to call this twice in a row because it won't do anything the second
 * time.
 */
void
fix_mangle_and_err(int *mangled, char **errmsg, char *name)
{
    if(mangled && *mangled){
	ttresize();
	picosigs();
	PaintBody(0);
	*mangled = 0;
    }

    if(errmsg && *errmsg){
	if(**errmsg){
	    char err[500];

	    snprintf(err, sizeof(err), "%s field: %s", name, *errmsg);
	    (*term.t_beep)();
	    emlwrite(err, NULL);
	}
	else
	    mlerase();

	free(*errmsg);
	*errmsg = NULL;
    }
}


/*
 * What is this for?
 * This is so that the To line will be appended to by an Lcc
 * entry unless the user types in the To line after the Lcc
 * has already been set.
 */
void
mark_sticky(struct headerentry *h)
{
    if(h && (!h->sticky_special || h->bldr_private))
      h->sticky = 1;
}


#ifdef	MOUSE
#undef	HeaderEditor

/*
 * Wraper function for the real header editor. 
 * Does the important tasks of:
 *	1) verifying that we _can_ edit the headers.
 *	2) acting on the result code from the header editor.
 */
int
HeaderEditor(int f, int n)
{
    int  retval;
    
    
#ifdef _WINDOWS
    /* Sometimes we get here from a scroll callback, which
     * is no good at all because mswin is not ready to process input and
     * this _headeredit() will never do anything.
     * Putting this test here was the most general solution I could think
     * of. */
    if (!mswin_caninput()) 
	return (-1);
#endif

    retval = HeaderEditorWork(f, n);
    if (retval == -3) {
	retval = mousepress(0,0);
    }
    return (retval);
}
#endif
