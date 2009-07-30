#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: browse.c 942 2008-03-04 18:21:33Z hubert@u.washington.edu $";
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
 * Program:	Routines to support file browser in pico and Pine composer
 *
 *
 * NOTES:
 *
 *   Misc. thoughts (mss, 5 Apr 92)
 * 
 *      This is supposed to be just a general purpose browser, equally
 *	callable from either pico or the pine composer.  Someday, it could
 * 	even be used to "wrap" the unix file business for really novice 
 *      users.  The stubs are here for renaming, copying, creating directories,
 *      deleting, undeleting (thought is delete just moves files to 
 *      ~/.pico.deleted directory or something and undelete offers the 
 *      files in there for undeletion: kind of like the mac trashcan).
 *
 *   Nice side effects
 *
 *      Since the full path name is always maintained and referencing ".." 
 *      stats the path stripped of its trailing name, the unpleasantness of 
 *      symbolic links is hidden.  
 *
 *   Fleshed out the file managements stuff (mss, 24 June 92)
 *
 *
 */
#include "headers.h"
#include "../c-client/mail.h"
#include "../c-client/rfc822.h"
#include "../pith/osdep/collate.h"
#include "../pith/charconv/filesys.h"
#include "../pith/conf.h"

#ifndef	_WINDOWS

#if	defined(bsd) || defined(lnx)
extern int errno;
#endif


/*
 * directory cell structure
 */
struct fcell {
    char *fname;				/* file name 	       */
    unsigned mode;				/* file's mode	       */
    char size[16];				/* file's size in s    */
    struct fcell *next;
    struct fcell *prev;
};


/*
 * master browser structure
 */
static struct bmaster {
    struct fcell *head;				/* first cell in list  */
    struct fcell *top;				/* cell in top left    */
    struct fcell *current;			/* currently selected  */
    int    longest;				/* longest file name (in screen width) */
    int	   fpl;					/* file names per line */
    int    cpf;					/* chars / file / line */
    int    flags;
    char   dname[NLINE];			/* this dir's name (UTF-8) */
    char   *names;				/* malloc'd name array (UTF-8) */
    LMLIST *lm;
} *gmp;						/* global master ptr   */


/*
 * title for exported browser display
 */
static	char	*browser_title = NULL;


struct bmaster *getfcells(char *, int);
void   PaintCell(int, int, int, struct fcell *, int);
void   PaintBrowser(struct bmaster *, int, int *, int *);
void   BrowserKeys(void);
void   layoutcells(struct bmaster *);
void   percdircells(struct bmaster *);
int    PlaceCell(struct bmaster *, struct fcell *, int *, int *);
void   zotfcells(struct fcell *);
void   zotmaster(struct bmaster **);
struct fcell *FindCell(struct bmaster *, char *);
int    sisin(char *, char *);
void   p_chdir(struct bmaster *);
void   BrowserAnchor(char *);
void   ClearBrowserScreen(void);
void   BrowserRunChild(char *, char *);
int    fcell_is_selected(struct fcell *, struct bmaster *);
void   add_cell_to_lmlist(struct fcell *, struct bmaster *);
void   del_cell_from_lmlist(struct fcell *, struct bmaster *);


static	KEYMENU menu_browse[] = {
    {"?", N_("Get Help"), KS_SCREENHELP},	{NULL, NULL, KS_NONE},
    {NULL, NULL, KS_NONE},		{"-", N_("Prev Pg"), KS_PREVPAGE},
    {"D", N_("Delete"), KS_NONE},		{"C",N_("Copy"), KS_NONE},
    {NULL, NULL, KS_NONE},		{NULL, NULL, KS_NONE},
    {"W", N_("Where is"), KS_NONE},		{"Spc", N_("Next Pg"), KS_NEXTPAGE},
    {"R", N_("Rename"), KS_NONE},		{NULL, NULL, KS_NONE}
};
#define	QUIT_KEY	1
#define	EXEC_KEY	2
#define	GOTO_KEY	6
#define	SELECT_KEY	7
#define	PICO_KEY	11


#define DIRWORD "dir"
#define PARENTDIR "parent"
#define SELECTWORD "SELECT"


/*
 * Default pager used by the stand-alone file browser.
 */
#define	BROWSER_PAGER	((gmode & MDFKEY) ? "pine -k -F" : "pine -F")


/*
 * function key mappings for callable browser
 */
static UCS  bfmappings[2][12][2] = { { { F1,  '?'},	/* stand alone */
				       { F2,  NODATA },	/* browser function */
				       { F3,  'q'},	/* key mappings... */
				       { F4,  'v'},
				       { F5,  'l'},
				       { F6,  'w'},
				       { F7,  '-'},
				       { F8,  ' '},
				       { F9,  'd'},
				       { F10, 'r'},
				       { F11, 'c'},
				       { F12, 'e'} },
				     { { F1,  '?'},	/* callable browser */
				       { F2,  NODATA },	/* function key */
				       { F3,  'e'},	/* mappings... */
				       { F4,  's'},
				       { F5,  NODATA },
				       { F6,  'w'},
				       { F7,  '-'},
				       { F8,  ' '},
				       { F9,  'd'},
				       { F10, 'r'},
				       { F11, 'c'},
				       { F12, 'a'} } };


/*
 * Browser help for pico (pine composer help handed to us by pine)
 */
static char *BrowseHelpText[] = {
/* TRANSLATORS: The next several lines go together. The ~ characters
   should be left in front of the characters they cause to be bold. */
N_("Help for Browse Command"),
"  ",
N_("        Pico's file browser is used to select a file from the"),
N_("        file system for inclusion in the edited text."),
"  ",
N_("~        Both directories and files are displayed.  Press ~S"),
N_("~        or ~R~e~t~u~r~n to select a file or directory.  When a file"),
N_("        is selected during the \"Read File\" command, it is"),
N_("        inserted into edited text.  Answering \"yes\" to the"),
N_("        verification question after a directory is selected causes"),
N_("        the contents of that directory to be displayed for selection."),
"  ",
N_("        The file named \"..\" is special, and means the \"parent\""),
N_("        of the directory being displayed.  Select this directory"),
N_("        to move upward in the directory tree."),
"  ",
N_("End of Browser Help."),
"  ",
NULL
};

/*
 * Help for standalone browser (pilot)
 */
static char *sa_BrowseHelpText[] = {
/* TRANSLATORS: Some more help text */
N_("Help for Pilot (PIne's Looker-upper Of Things"),
" ",
N_("        Pilot is a simple, display-oriented file system browser based on the"),
N_("~        Alpine message system composer.  As with Alpine, commands are displayed at"),
N_("~        the bottom of the screen, and context-sensitive help is provided."),
" ",
N_("~        Pilot displays the current working directory at the top of the screen."),
N_("~        The directory's contents are displayed in columns of file name, file"),
N_("~        size pairs.  Names that are directories are indicated by the name"),
N_("~        ~(~d~i~r~) in place of the file size.  The parent of the current working"),
N_("~        directory is indicated by the file name ~.~. and size of ~(~p~a~r~e~n~t~ ~d~i~r~)."),
N_("~        File names that are symbolic links to other files are displayed with a"),
N_("~        file size of ~-~-."),
" ",
N_("        The following function keys are available in Pilot:"),
" ",
N_("~        ~?        Display this help text."),
N_("~        ~Q        Quit Pilot."),
" ",
N_("~        ~V        View the currently selected file or open the selected directory."),
N_("~                Note: Pilot invokes ~p~i~n~e ~-~F, or the program defined by the ~P~A~G~E~R"),
N_("~                      environment variable, to view the file."),
N_("~        ~L        Launch an external application program."),
" ",
N_("~        ~W        Search for a file by name."),
N_("~        ~-        Scroll up one page."),
N_("~        ~S~p~a~c~e        Scroll down one page."),
N_("~        ~N,~^~F        Move forward (right) one column."),
N_("~        ~P,~^~B        Move back (left) one column."),
N_("~        ~^~N        Move down one row."),
N_("~        ~^~P        Move up one row."),
" ",
N_("~        ~D        Delete the selected file."),
N_("~        ~R        Rename the selected file or directory."),
N_("~        ~C        Copy the selected file."),
N_("~        ~E        Edit the selected file."),
N_("~                Note: Pilot invokes ~p~i~c~o, or the program defined by the ~E~D~I~T~O~R"),
N_("~                      environment variable, to edit the file."),
" ",
N_("End of Pilot Help."),
NULL
};



/*
 * pico_file_browse - Exported version of FileBrowse below.
 */
int
pico_file_browse(PICO *pdata, char *dir, size_t dirlen, char *fn, size_t fnlen,
		 char *sz, size_t szlen, int flags)
{
    int  rv;
    char title_buf[64];

    Pmaster = pdata;
    gmode = pdata->pine_flags | MDEXTFB;
    km_popped = 0;

    /* only init screen bufs for display and winch handler */
    if(!vtinit())
      return(-1);

    if(Pmaster){
	term.t_mrow = Pmaster->menu_rows;
	if(Pmaster->oper_dir)
	  strncpy(opertree, Pmaster->oper_dir, NLINE);
	
	if(*opertree)
	  fixpath(opertree, sizeof(opertree));
    }

    /* install any necessary winch signal handler */
    ttresize();

    snprintf(title_buf, sizeof(title_buf), "%s FILE", pdata->pine_anchor);
    set_browser_title(title_buf);
    rv = FileBrowse(dir, dirlen, fn, fnlen, sz, szlen, flags, NULL);
    set_browser_title(NULL);
    vttidy();			/* clean up tty handling */
    zotdisplay();		/* and display structures */
    Pmaster = NULL;		/* and global config structure */
    return(rv);
}



/*
 * FileBrowse - display contents of given directory dir
 *
 *	    intput:  
 *		     dir points to initial dir to browse.
 *                   dirlen- buffer size of dir
 *		     fn  initial file name.
 *                   fnlen- buffer size of fn 
 *
 *         returns:
 *                   dir points to currently selected directory (without
 *			trailing file system delimiter)
 *                   fn  points to currently selected file
 *                   sz  points to size of file if ptr passed was non-NULL
 *		     flags
 *
 *                 Special dispensation for FB_LMODE. If the caller sets
 *                 FB_LMODEPOS, and the caller passes a non-null lmreturn,
 *                 then the return values will be in lmreturn instead of
 *                 in dir and fn. The caller is responsible for freeing
 *                 the contents of lmreturn.
 *
 *                   1 if a file's been selected
 *                   0 if no files selected
 *                  -1 if there were problems
 */
int
FileBrowse(char *dir, size_t dirlen, char *fn, size_t fnlen,
	   char *sz, size_t szlen, int fb_flags, LMLIST **lmreturn)
{
    UCS c, new_c;
    int status, i, j;
    int row, col, crow, ccol;
    char *p, *envp, child[NLINE], tmp[NLINE];
    struct bmaster *mp;
    struct fcell *tp;
    EML eml;
#ifdef MOUSE
    MOUSEPRESS mousep;
#endif

    child[0] = '\0';

    if((gmode&MDTREE) && !in_oper_tree(dir)){
	eml.s = opertree;
	emlwrite(_("\007Can't read outside of %s in restricted mode"), &eml);
	sleep(2);
	return(0);
    }

    if(gmode&MDGOTO){
	/* fix up function key mapping table */
	/* fix up key menu labels */
    }

    /* build contents of cell structures */
    if((gmp = getfcells(dir, fb_flags)) == NULL)
      return(-1);
    
    tp = NULL;
    if(fn && *fn){
	if((tp = FindCell(gmp, fn)) != NULL){
	    gmp->current = tp;
	    PlaceCell(gmp, gmp->current, &row, &col);
	}
    }

    /* paint screen */
    PaintBrowser(gmp, 0, &crow, &ccol);

    while(1){						/* the big loop */
	if(!(gmode&MDSHOCUR)){
	    crow = term.t_nrow-term.t_mrow;
	    ccol = 0;
	}

	if(!(gmode&MDSHOCUR))
	  movecursor(crow, ccol);
	else if(gmp->flags & FB_LMODEPOS){
	    if(gmp->flags & FB_LMODE && gmp->current->mode != FIODIR)
	      movecursor(crow, ccol+1);
	    else
	      movecursor(crow, ccol+4);
	}
	else
	  movecursor(crow, ccol);

	if(km_popped){
	    km_popped--;
	    if(km_popped == 0)
	      /* cause bottom three lines to repaint */
	      PaintBrowser(gmp, 0, &crow, &ccol);
	}

	if(km_popped){  /* temporarily change to cause menu to paint */
	    term.t_mrow = 2;
	    movecursor(term.t_nrow-2, 0);  /* clear status line */
	    peeol();
	    BrowserKeys();
	    term.t_mrow = 0;
	}

	(*term.t_flush)();

#ifdef MOUSE
	mouse_in_content(KEY_MOUSE, -1, -1, 0, 0);
	register_mfunc(mouse_in_content,2,0,term.t_nrow-(term.t_mrow+1),
	    term.t_ncol);
#endif
	c = GetKey();
#ifdef	MOUSE
	clear_mfunc(mouse_in_content);
#endif

	if(Pmaster){
	    if(Pmaster->newmail && (c == NODATA || time_to_check())){
		if((*Pmaster->newmail)(c == NODATA ? 0 : 2, 1) >= 0){
		    int rv;
		    
		    if(km_popped){		/* restore display */
			km_popped = 0;
			PaintBrowser(gmp, 0, &crow, &ccol);
		    }

		    clearcursor();
		    mlerase();
		    rv = (*Pmaster->showmsg)(c);
		    ttresize();
		    picosigs();
		    if(rv)	/* Did showmsg corrupt the display? */
		      PaintBrowser(gmp, 0, &crow, &ccol); /* Yes, repaint */

		    mpresf = 1;
		}

		clearcursor();
		if(gmp->flags & FB_LMODEPOS){
		    if(gmp->flags & FB_LMODE && gmp->current->mode != FIODIR)
		      movecursor(crow, ccol+1);
		    else
		      movecursor(crow, ccol+4);
		}
		else
		  movecursor(crow, ccol);
	    }
	}
	else{
	    if(get_input_timeout() && (c == NODATA || time_to_check()))
	      if(pico_new_mail())
		emlwrite(_("You may possibly have new mail."), NULL);
	}

	if(km_popped)
	  switch(c){
	    case NODATA:
	    case (CTRL|'L'):
	      km_popped++;
	      break;
	    
	    default:
	      /* clear bottom three lines */
	      movecursor(term.t_nrow-2, 0);
	      peeol();
	      movecursor(term.t_nrow-1, 0);
	      peeol();
	      movecursor(term.t_nrow, 0);
	      peeol();
	      break;
	  }

	if(c == NODATA)				/* GetKey timed out */
	  continue;
	else if(Pmaster)
	  (*Pmaster->keybinput)();
	  

	if(mpresf){				/* blast old messages */
	    if(mpresf++ > MESSDELAY){		/* every few keystrokes */
		mlerase();
	    }
        }

						/* process commands */
	switch(new_c = normalize_cmd(c,bfmappings[(gmode&MDBRONLY)?0:1],2)){

	  case KEY_RIGHT:			/* move right */
	  case (CTRL|'@'):
	  case (CTRL|'F'):			/* forward  */
	  case 'n' :
	  case 'N' :
	    if(gmp->current->next == NULL){
		(*term.t_beep)();
		break;
	    }

	    PlaceCell(gmp, gmp->current, &row, &col);
	    PaintCell(row, col, gmp->cpf, gmp->current, 0);
	    gmp->current = gmp->current->next;
	    if(PlaceCell(gmp, gmp->current, &row, &col)){
		PaintBrowser(gmp, 1, &crow, &ccol);
	    }
	    else{
		PaintCell(row, col, gmp->cpf, gmp->current, 1);
		crow = row;
		ccol = col;
	    }
	    break;

	  case KEY_LEFT:				/* move left */
	  case (CTRL|'B'):				/* back */
	  case 'p' :
	  case 'P' :
	    if(gmp->current->prev == NULL){
		(*term.t_beep)();
		break;
	    }

	    PlaceCell(gmp, gmp->current, &row, &col);
	    PaintCell(row, col, gmp->cpf, gmp->current, 0);
	    gmp->current = gmp->current->prev;
	    if(PlaceCell(gmp, gmp->current, &row, &col)){
		PaintBrowser(gmp, 1, &crow, &ccol);
	    }
	    else{
		PaintCell(row, col, gmp->cpf, gmp->current, 1);
		crow = row;
		ccol = col;
	    }
	    break;

	  case (CTRL|'A'):				/* beginning of line */
	    tp = gmp->current;
	    i = col;
	    while(i > 0){
		i -= gmp->cpf;
		if(tp->prev != NULL)
		  tp = tp->prev;
	    }
	    PlaceCell(gmp, gmp->current, &row, &col);
	    PaintCell(row, col, gmp->cpf, gmp->current, 0);
	    gmp->current = tp;
	    if(PlaceCell(gmp, tp, &row, &col)){
		PaintBrowser(gmp, 1, &crow, &ccol);
	    }
	    else{
		PaintCell(row, col, gmp->cpf, gmp->current, 1);
		crow = row;
		ccol = col;
	    }
	    break;

	  case (CTRL|'E'):				/* end of line */
	    tp = gmp->current;
	    i = col + gmp->cpf;
	    while(i+gmp->cpf <= gmp->cpf * gmp->fpl){
		i += gmp->cpf;
		if(tp->next != NULL)
		  tp = tp->next;
	    }

	    PlaceCell(gmp, gmp->current, &row, &col);
	    PaintCell(row, col, gmp->cpf, gmp->current, 0);
	    gmp->current = tp;
	    if(PlaceCell(gmp, tp, &row, &col)){
		PaintBrowser(gmp, 1, &crow, &ccol);
	    }
	    else{
		PaintCell(row, col, gmp->cpf, gmp->current, 1);
		crow = row;
		ccol = col;
	    }
	    break;

	  case (CTRL|'V'):				/* page forward */
	  case ' ':
	  case KEY_PGDN :
	  case KEY_END :
	    tp = gmp->top;
	    i = term.t_nrow - term.t_mrow - 2;

	    while(i-- && tp->next != NULL){
		j = 0;
		while(++j <= gmp->fpl  && tp->next != NULL)
		  tp = tp->next;
	    }

	    if(tp == NULL)
	      continue;

	    PlaceCell(gmp, gmp->current, &row, &col);
	    PaintCell(row, col, gmp->cpf, gmp->current, 0);
	    gmp->current = tp;
	    if(PlaceCell(gmp, tp, &row, &col)){
		PaintBrowser(gmp, 1, &crow, &ccol);
	    }
	    else{
		PaintCell(row, col, gmp->cpf, gmp->current, 1);
		crow = row;
		ccol = col;
	    }
	    break;

	  case '-' :
	  case (CTRL|'Y'):				/* page backward */
	  case KEY_PGUP :
	  case KEY_HOME :
	    tp = gmp->top;
	    i = term.t_nrow - term.t_mrow - 4;
	    while(i-- && tp != NULL){
		j = gmp->fpl;
		while(j-- && tp != NULL)
		  tp = tp->prev;
	    }

	    if(tp || (gmp->current != gmp->top)){	/* clear old hilite */
		PlaceCell(gmp, gmp->current, &row, &col);
		PaintCell(row, col, gmp->cpf, gmp->current, 0);
	    }

	    if(tp)					/* new page ! */
		gmp->current = tp;
	    else if(gmp->current != gmp->top)		/* goto top of page */
		gmp->current = gmp->top;
	    else					/* do nothing */
	      continue;

	    if(PlaceCell(gmp, gmp->current, &row, &col)){
		PaintBrowser(gmp, 1, &crow, &ccol);
	    }
	    else{
		PaintCell(row, col, gmp->cpf, gmp->current, 1);
		crow = row;
		ccol = col;
	    }

	    break;

	  case KEY_DOWN :
	  case (CTRL|'N'):				/* next */
	    tp = gmp->current;
	    i = gmp->fpl;
	    while(i--){
		if(tp->next == NULL){
		    (*term.t_beep)();
		    break;
		}
		else
		  tp = tp->next;
	    }
	    if(i != -1)					/* can't go down */
	      break;

	    PlaceCell(gmp, gmp->current, &row, &col);
	    PaintCell(row, col, gmp->cpf, gmp->current, 0);
	    gmp->current = tp;
	    if(PlaceCell(gmp, tp, &row, &col)){
		PaintBrowser(gmp, 1, &crow, &ccol);
	    }
	    else{
		PaintCell(row, col, gmp->cpf, gmp->current, 1);
		crow = row;
		ccol = col;
	    }
	    break;

	  case KEY_UP :
	  case (CTRL|'P'):				/* previous */
	    tp = gmp->current;
	    i = gmp->fpl;
	    while(i-- && tp)
	      tp = tp->prev;

	    if(tp == NULL)
	      break;

	    PlaceCell(gmp, gmp->current, &row, &col);
	    PaintCell(row, col, gmp->cpf, gmp->current, 0);
	    gmp->current = tp;
	    if(PlaceCell(gmp, tp, &row, &col)){
		PaintBrowser(gmp, 1, &crow, &ccol);
	    }
	    else{
		PaintCell(row, col, gmp->cpf, gmp->current, 1);
		crow = row;
		ccol = col;
	    }
	    break;

#ifdef MOUSE	    
	  case KEY_MOUSE:
	    mouse_get_last (NULL, &mousep);
	    if (mousep.doubleclick) {
		goto Selected;
	    }
	    else {
		row = mousep.row -= 2;		/* Adjust for header*/
		col = mousep.col;
		i = row * gmp->fpl + (col / gmp->cpf);	/* Count from top */
		tp = gmp->top;				/* start at top. */
		for (; i > 0 && tp != NULL; --i)	/* Count cells. */
		  tp = tp->next;
		if (tp != NULL) {			/* Valid cell? */
		    PlaceCell(gmp, gmp->current, &row, &col);
		    PaintCell(row, col, gmp->cpf, gmp->current, 0);
		    gmp->current = tp;
		    if(PlaceCell(gmp, tp, &row, &col)){
			PaintBrowser(gmp, 1, &crow, &ccol);
		    }
		    else{
			PaintCell(row, col, gmp->cpf, gmp->current, 1);
			crow = row;
			ccol = col;
		    }
		}
            }
	    break;
#endif

	  case 'e':					/* exit or edit */
	  case 'E':
	    if(gmode&MDBRONLY){				/* run "pico" */
		snprintf(child, sizeof(child), "%s%c%s", gmp->dname, C_FILESEP,
			gmp->current->fname);
		/* make sure selected isn't a directory or executable */
		if(!LikelyASCII(child)){
		    emlwrite(_("Can't edit non-text file.  Try Launch."), NULL);
		    break;
		}

		if((envp = (char *) getenv("EDITOR")) != NULL)
		  snprintf(tmp, sizeof(tmp), "%s \'%s\'", envp, child);
		else
		  snprintf(tmp, sizeof(tmp), "pico%s%s%s \'%s\'",
			  (gmode & MDFKEY) ? " -f" : "",
			  (gmode & MDSHOCUR) ? " -g" : "",
			  (gmode & MDMOUSE) ? " -m" : "",
			  child);

		BrowserRunChild(tmp, gmp->dname);	/* spawn pico */
		PaintBrowser(gmp, 0, &crow, &ccol);	/* redraw browser */
	    }
	    else{
		zotmaster(&gmp);
		return(0);
	    }

	    break;

	  case 'q':			            /* user exits wrong */
	  case 'Q':
	    if(gmode&MDBRONLY){
		zotmaster(&gmp);
		return(0);
	    }

	    unknown_command(c);
	    break;

	  case 'x':				    /* toggle selection */
	  case 'X':
	    if(!(gmp->flags & FB_LMODE)){
		if(gmp->flags & FB_LMODEPOS)
		  emlwrite(_("\007Type L command to use ListMode"), NULL);
		else
		  unknown_command(c);
		
		break;
	    }

	    if(gmp->current->mode == FIODIR){
		emlwrite(_("\007Can't Set directories"), NULL);
		break;
	    }

	    if(fcell_is_selected(gmp->current, gmp))
	      del_cell_from_lmlist(gmp->current, gmp);
	    else
	      add_cell_to_lmlist(gmp->current, gmp);

	    PlaceCell(gmp, gmp->current, &row, &col);
	    PaintCell(row, col, gmp->cpf, gmp->current, 1);
	    break;

	  case 'l':			            /* run Command */
	  case 'L':				    /* or ListMode */
	    if(gmp->flags & FB_LMODEPOS){
		if(gmp->flags & FB_LMODE){
		    /*
		     * Unless we make it so you can get out of ListMode
		     * once you're in ListMode, this must be an error.
		     */
		    emlwrite(_("\007Already in ListMode"), NULL);
		    break;
		}
		else{
		    gmp->flags |= FB_LMODE;
		    PaintBrowser(gmp, 0, &crow, &ccol);
		}

		break;
	    }

	    if(!(gmode&MDBRONLY)){
		unknown_command(c);
		break;
	    }

/* add subcommands to invoke pico and insert selected filename */
/* perhaps: add subcmd to scroll command history */

	    tmp[0] = '\0';
	    i = 0;
	    snprintf(child, sizeof(child), "%s%c%s", gmp->dname, C_FILESEP,
		    gmp->current->fname);
	    while(!i){
		static EXTRAKEYS opts[] = {
		    {"^X", N_("Add Name"), CTRL|'X', KS_NONE},
		    {NULL, NULL, 0, KS_NONE},
		};

		status = mlreply_utf8(_("Command to execute: "),
				 tmp, NLINE, QNORML, opts);
		switch(status){
		  case HELPCH:
		    emlwrite(_("\007No help yet!"), NULL);
/* remove break and sleep after help text is installed */
		    sleep(3);
		    break;
		  case (CTRL|'X'):
		    strncat(tmp, child, sizeof(tmp)-strlen(tmp)-1);
		    tmp[sizeof(tmp)-1] = '\0';
		    break;
		  case (CTRL|'L'):
		    PaintBrowser(gmp, 0, &crow, &ccol);
		    break;
		  case ABORT:
		    emlwrite(_("Command cancelled"), NULL);
		    i++;
		    break;
		  case FALSE:
		  case TRUE:
		    i++;

		    if(tmp[0] == '\0'){
			emlwrite(_("No command specified"), NULL);
			break;
		    }

		    BrowserRunChild(tmp, gmp->dname);
		    PaintBrowser(gmp, 0, &crow, &ccol);
		    break;
		  default:
		    break;
		}
	    }

	    BrowserKeys();
	    break;

	  case 'd':					/* delete */
	  case 'D':
	    if(gmp->current->mode == FIODIR){
/* BUG: if dir is empty it should be deleted */
		emlwrite(_("\007Can't delete a directory"), NULL);
		break;
	    }

	    if(gmode&MDSCUR){				/* not allowed! */
		emlwrite(_("Delete not allowed in restricted mode"),NULL);
		break;
	    }

	    snprintf(child, sizeof(child), "%s%c%s", gmp->dname, C_FILESEP, 
		    gmp->current->fname);

	    i = 0;
	    while(i++ < 2){		/* verify twice!! */
		if(i == 1){
		    if(fexist(child, "w", (off_t *)NULL) != FIOSUC){
		      strncpy(tmp, _("File is write protected! OVERRIDE"), sizeof(tmp));
		      tmp[sizeof(tmp)-1] = '\0';
		    }
		    else
		      /* TRANSLATORS: This is a question, Delete file <filename> */
		      snprintf(tmp, sizeof(tmp), _("Delete file \"%.*s\""), NLINE - 20, child);
		}
		else{
		  strncpy(tmp, _("File CANNOT be UNdeleted!  Really delete"), sizeof(tmp));
		  tmp[sizeof(tmp)-1] = '\0';
		}

		if((status = mlyesno_utf8(tmp, FALSE)) != TRUE){
		    emlwrite((status ==  ABORT)
			       ? _("Delete Cancelled")
			       : _("File Not Deleted"),
			     NULL);
		    break;
		}
	    }

	    if(status == TRUE){
		if(our_unlink(child) < 0){
		    eml.s = errstr(errno);
		    emlwrite(_("Delete Failed: %s"), &eml);
		}
		else{			/* fix up pointers and redraw */
		    tp = gmp->current;
		    if(tp->next){
			gmp->current = tp->next;
			if((tp->next->prev = tp->prev) != NULL)
			  tp->prev->next = tp->next;
		    }
		    else if(tp->prev) {
			gmp->current = tp->prev;
			if((tp->prev->next = tp->next) != NULL)
			  tp->next->prev = tp->prev;
		    }

		    if(tp == gmp->head)
		      gmp->head = tp->next;

		    tp->fname = NULL;
		    tp->next = tp->prev = NULL;
		    if(tp != gmp->current)
		      free((char *) tp);

		    if((tp = FindCell(gmp, gmp->current->fname)) != NULL){
			gmp->current = tp;
			PlaceCell(gmp, gmp->current, &row, &col);
		    }
			    
		    PaintBrowser(gmp, 1, &crow, &ccol);
		    mlerase();
		}
	    }

	    BrowserKeys();
	    break;

	  case '?':					/* HELP! */
	  case (CTRL|'G'):
	    if(term.t_mrow == 0){
		if(km_popped == 0){
		    km_popped = 2;
		    break;
		}
	    }

	    if(Pmaster){
		VARS_TO_SAVE *saved_state;

		saved_state = save_pico_state();
		(*Pmaster->helper)(Pmaster->browse_help,
				   _("Help for Browsing"), 1);
		if(saved_state){
		    restore_pico_state(saved_state);
		    free_pico_state(saved_state);
		}
	    }
	    else if(gmode&MDBRONLY)
	      pico_help(sa_BrowseHelpText, _("Browser Help"), 1);
	    else
	      pico_help(BrowseHelpText, _("Help for Browsing"), 1);
	    /* fall thru to repaint everything */

	  case (CTRL|'L'):
	    PaintBrowser(gmp, 0, &crow, &ccol);
	    break;

	  case 'g':                             /* jump to a directory */
	  case 'G':

	    if(!(gmode&MDGOTO))
	      goto Default;

	    i = 0;
	    child[0] = '\0';
 
	    while(!i){
 
		/* TRANSLATORS: A prompt asking for a directory to
		   move into */
		status = mlreply_utf8(_("Directory to go to: "), child, NLINE, QNORML,
				 NULL);
 
		switch(status){
		  case HELPCH:
		    emlwrite(_("\007No help yet!"), NULL);
		    /* remove break and sleep after help text is installed */
		    sleep(3);
		    break;
		  case (CTRL|'L'):
		    PaintBrowser(gmp, 0, &crow, &ccol);
		  break;
		  case ABORT:
		    emlwrite(_("Goto cancelled"), NULL);
		    i++;
		    break;
		  case FALSE:
		  case TRUE:
		    i++;
 
		    if(*child == '\0'){
		      strncpy(child, gethomedir(NULL), sizeof(child));
		      child[sizeof(child)-1] = '\0';
		    }
 
		    if(!compresspath(gmp->dname, child, sizeof(child))){
			eml.s = child;
			emlwrite(_("Invalid Directory: %s"), &eml);
			break;
		    }

		    if((gmode&MDSCUR) && homeless(child)){
			emlwrite(_("Restricted mode browsing limited to home directory"),NULL);
			break;
		    }
 
		    if((gmode&MDTREE) && !in_oper_tree(child)){
		      eml.s = opertree;
		      emlwrite(_("\007 Can't go outside of %s in restricted mode"),
			       &eml);
			break;
		    }
 
		    if(isdir(child, (long *) NULL, NULL)){
			if((mp = getfcells(child, fb_flags)) == NULL){
			    /* getfcells should explain what happened */
			    i++;
			    break;
			}
 
			zotmaster(&gmp);
			gmp = mp;
			PaintBrowser(gmp, 0, &crow, &ccol);
		    }
		    else{
		      eml.s = child;
		      emlwrite(_("\007Not a directory: \"%s\""), &eml);
		    }
 
		    break;
		  default:
		    break;
		}
	    }
	    BrowserKeys();
	    break;

	  case 'a':					/* Add */
	  case 'A':
	    if(gmode&MDSCUR){				/* not allowed! */
		emlwrite(_("Add not allowed in restricted mode"),NULL);
		break;
	    }

	    i = 0;
	    child[0] = '\0';
	    /* pass in default filename */
	    if(fn && *fn){
		strncpy(child, fn, sizeof(child) - 1);
		child[sizeof(child) - 1] = '\0';
	    }

	    while(!i){

		switch(status=mlreply_utf8(_("Name of file to add: "), child, NLINE,
				      QFFILE, NULL)){
		  case HELPCH:
		    emlwrite(_("\007No help yet!"), NULL);
/* remove break and sleep after help text is installed */
		    sleep(3);
		    break;
		  case (CTRL|'L'):
		    PaintBrowser(gmp, 0, &crow, &ccol);
		    break;
		  case ABORT:
		    emlwrite(_("Add File Cancelled"), NULL);
		    i++;
		    break;
		  case FALSE:
		    /*
		     * Support default filename. A return of 'FALSE' means
		     * 'No change in filename' which is fine if we have
		     * provided a default.
		     * John Berthels <john.berthels@nexor.co.uk>
		     */
		    /* FALLTHROUGH */
		  case TRUE:
		    i++;

		    if(child[0] == '\0'){
			emlwrite(_("No file named.  Add Cancelled."), NULL);
			break;
		    }

		    if(!compresspath(gmp->dname, child, sizeof(child))){
			emlwrite(_("Too many ..'s in name"), NULL);
			break;
		    }

		    if((gmode&MDTREE) && !in_oper_tree(child)){
		       eml.s = opertree;
		       emlwrite(_("\007Restricted mode allows Add in %s only"),
				&eml);
			break;
		    }

		    if((status = fexist(child, "w", (off_t *)NULL)) == FIOSUC){
			snprintf(tmp, sizeof(tmp), _("File \"%.*s\" already exists!"),
				NLINE - 20, child);
			emlwrite(tmp, NULL);
			break;
		    }
		    else if(status != FIOFNF){
			fioperr(status, child);
			break;
		    }

		    if(ffwopen(child, FALSE) != FIOSUC){
			/* ffwopen should've complained */
			break;
		    }
		    else{			/* highlight new file */
			ffclose();
		        eml.s = child;
			emlwrite(_("Added File \"%s\""), &eml);

			if((p = strrchr(child, C_FILESEP)) == NULL){
			    emlwrite(_("Problems refiguring browser"), NULL);
			    break;
			}

			*p = '\0';
			if(p != child){
			    strncpy(tmp, child, sizeof(tmp));
			    tmp[sizeof(tmp)-1] = '\0';
			    j = 0;
			    while((child[j++] = *++p) != '\0')
			      ;
			}
			else{
			  strncpy(tmp, S_FILESEP, sizeof(tmp));
			  tmp[sizeof(tmp)-1] = '\0';
			}

			/*
			 * new file in same dir? if so, refigure files
			 * and redraw...
			 */
			if(!strcmp(tmp, gmp->dname)){ 
			    if((mp = getfcells(gmp->dname, fb_flags)) == NULL)
			      /* getfcells should explain what happened */
			      break;

			    zotmaster(&gmp);
			    gmp = mp;
			    if((tp = FindCell(gmp, child)) != NULL){
				gmp->current = tp;
				PlaceCell(gmp, gmp->current, &row, &col);
			    }

			    PaintBrowser(gmp, 1, &crow, &ccol);
			}
		    }
		    break;
		  default:
		    break;
		}
	    }

	    BrowserKeys();
	    break;

	  case 'c':					/* copy */
	  case 'C':
	    if(gmp->current->mode == FIODIR){
		emlwrite(_("\007Can't copy a directory"), NULL);
		break;
	    }

	    if(gmode&MDSCUR){				/* not allowed! */
		emlwrite(_("Copy not allowed in restricted mode"),NULL);
		break;
	    }

	    i = 0;
	    child[0] = '\0';

	    while(!i){

		switch(status=mlreply_utf8(_("Name of new copy: "), child, NLINE,
				      QFFILE, NULL)){
		  case HELPCH:
		    emlwrite(_("\007No help yet!"), NULL);
/* remove break and sleep after help text is installed */
		    sleep(3);
		    break;
		  case (CTRL|'L'):
		    PaintBrowser(gmp, 0, &crow, &ccol);
		    break;
		  case ABORT:
		    emlwrite(_("Make Copy Cancelled"), NULL);
		    i++;
		    break;
		  case FALSE:
		    i++;
		    mlerase();
		    break;
		  case TRUE:
		    i++;

		    if(child[0] == '\0'){
			emlwrite(_("No destination, file not copied"), NULL);
			break;
		    }

		    if(!strcmp(gmp->current->fname, child)){
			emlwrite(_("\007Can't copy file on to itself!"), NULL);
			break;
		    }

		    if(!compresspath(gmp->dname, child, sizeof(child))){
			emlwrite(_("Too many ..'s in name"), NULL);
			break;
		    }

		    if((gmode&MDTREE) && !in_oper_tree(child)){
		       eml.s = opertree;
		       emlwrite(_("\007Restricted mode allows Copy in %s only"),
				&eml);
			break;
		    }

		    if((status = fexist(child, "w", (off_t *)NULL)) == FIOSUC){
			/* TRANSLATORS: A question: File <filename> exists! Replace? */
			snprintf(tmp, sizeof(tmp), _("File \"%.*s\" exists! OVERWRITE"),
				NLINE - 20, child);
			if((status = mlyesno_utf8(tmp, 0)) != TRUE){
			    emlwrite((status == ABORT)
				      ? _("Make copy cancelled") 
				      : _("File Not Renamed"),
				     NULL);
			    break;
			}
		    }
		    else if(status != FIOFNF){
			fioperr(status, child);
			break;
		    }

		    snprintf(tmp, sizeof(tmp), "%s%c%s", gmp->dname, C_FILESEP, 
			    gmp->current->fname);

		    if(copy(tmp, child) < 0){
			/* copy()  will report any error messages */
			break;
		    }
		    else{			/* highlight new file */
		        eml.s = child;
			emlwrite(_("File copied to %s"), &eml);

			if((p = strrchr(child, C_FILESEP)) == NULL){
			    emlwrite(_("Problems refiguring browser"), NULL);
			    break;
			}

			*p = '\0';
			strncpy(tmp, (p == child) ? S_FILESEP: child, sizeof(tmp));
			tmp[sizeof(tmp)-1] = '\0';

			/*
			 * new file in same dir? if so, refigure files
			 * and redraw...
			 */
			if(!strcmp(tmp, gmp->dname)){ 
			    strncpy(child, gmp->current->fname, sizeof(child));
			    child[sizeof(child)-1] = '\0';
			    if((mp = getfcells(gmp->dname, fb_flags)) == NULL)
			      /* getfcells should explain what happened */
			      break;

			    zotmaster(&gmp);
			    gmp = mp;
			    if((tp = FindCell(gmp, child)) != NULL){
				gmp->current = tp;
				PlaceCell(gmp, gmp->current, &row, &col);
			    }

			    PaintBrowser(gmp, 1, &crow, &ccol);
			}
		    }
		    break;
		  default:
		    break;
		}
	    }
	    BrowserKeys();
	    break;

	  case 'r':					/* rename */
	  case 'R':
	    i = 0;

	    if(!strcmp(gmp->current->fname, "..")){
		emlwrite(_("\007Can't rename \"..\""), NULL);
		break;
	    }

	    if(gmode&MDSCUR){				/* not allowed! */
		emlwrite(_("Rename not allowed in restricted mode"),NULL);
		break;
	    }

	    strncpy(child, gmp->current->fname, sizeof(child));
	    child[sizeof(child)-1] = '\0';

	    while(!i){

		switch(status=mlreply_utf8(_("Rename file to: "), child, NLINE, QFFILE,
				      NULL)){
		  case HELPCH:
		    emlwrite(_("\007No help yet!"), NULL);
/* remove break and sleep after help text is installed */
		    sleep(3);
		    break;
		  case (CTRL|'L'):
		    PaintBrowser(gmp, 0, &crow, &ccol);
		    break;
		  case ABORT:
		    emlwrite(_("Rename cancelled"), NULL);
		    i++;
		    break;
		  case FALSE:
		  case TRUE:
		    i++;

		    if(child[0] == '\0' || status == FALSE){
			mlerase();
			break;
		    }

		    if(!compresspath(gmp->dname, child, sizeof(child))){
			emlwrite(_("Too many ..'s in name"), NULL);
			break;
		    }

		    if((gmode&MDTREE) && !in_oper_tree(child)){
		       eml.s = opertree;
		       emlwrite(_("\007Restricted mode allows Rename in %s only"),
				&eml);
			break;
		    }

		    status = fexist(child, "w", (off_t *)NULL);
		    if(status == FIOSUC || status == FIOFNF){
			if(status == FIOSUC){
			    snprintf(tmp, sizeof(tmp), _("File \"%.*s\" exists! OVERWRITE"),
				    NLINE - 20, child);

			    if((status = mlyesno_utf8(tmp, FALSE)) != TRUE){
				emlwrite((status ==  ABORT)
					  ? _("Rename cancelled")
					  : _("Not Renamed"),
					 NULL);
				break;
			    }
			}

			snprintf(tmp, sizeof(tmp), "%s%c%s", gmp->dname, C_FILESEP, 
				gmp->current->fname);

			if(our_rename(tmp, child) < 0){
			    eml.s = errstr(errno);
			    emlwrite(_("Rename Failed: %s"), &eml);
			}
			else{
			    if((p = strrchr(child, C_FILESEP)) == NULL){
				emlwrite(_("Problems refiguring browser"), NULL);
				break;
			    }
			    
			    *p = '\0';
			    strncpy(tmp, (p == child) ? S_FILESEP: child, sizeof(tmp));
			    tmp[sizeof(tmp)-1] = '\0';

			    if((mp = getfcells(tmp, fb_flags)) == NULL)
			      /* getfcells should explain what happened */
			      break;

			    zotmaster(&gmp);
			    gmp = mp;

			    if((tp = FindCell(gmp, ++p)) != NULL){
				gmp->current = tp;
				PlaceCell(gmp, gmp->current, &row, &col);
			    }

			    PaintBrowser(gmp, 1, &crow, &ccol);
			    mlerase();
			}
		    }
		    else{
			fioperr(status, child);
		    }
		    break;
		  default:
		    break;
		}
	    }
	    BrowserKeys();
	    break;

	  case 'v':					/* stand-alone */
	  case 'V':					/* browser "view" */
	  case 's':					/* user "select" */
	  case 'S':
	  case (CTRL|'M'):
	  Selected:

	    if(((new_c == 'S' || new_c == 's') && (gmode&MDBRONLY))
	       || ((new_c == 'V' || new_c == 'v') && !(gmode&MDBRONLY)))
	      goto Default;

	    if(gmp->current->mode == FIODIR){
		*child = '\0';
		strncpy(tmp, gmp->dname, sizeof(tmp));
		tmp[sizeof(tmp)-1] = '\0';
		p = gmp->current->fname;
		if(p[0] == '.' && p[1] == '.' && p[2] == '\0'){
		    if((p=strrchr(tmp, C_FILESEP)) != NULL){
			*p = '\0';

			if((gmode&MDTREE) && !in_oper_tree(tmp)){
			    eml.s = PARENTDIR;
			    emlwrite(
				   _("\007Can't visit %s in restricted mode"),
				   &eml);
			    break;
			}

			strncpy(child, &p[1], sizeof(child));
			child[sizeof(child)-1] = '\0';

			if
#if defined(DOS) || defined(OS2)
			  (p == tmp || (tmp[1] == ':' && tmp[2] == '\0'))
#else
			  (p == tmp)
#endif
			  {	/* is it root? */
#if defined(DOS) || defined(OS2)
			      if(*child){
				strncat(tmp, S_FILESEP, sizeof(tmp)-strlen(tmp)-1);
				tmp[sizeof(tmp)-1] = '\0';
			      }
#else
			      if(*child){
			        strncpy(tmp, S_FILESEP, sizeof(tmp));
				tmp[sizeof(tmp)-1] = '\0';
			      }
#endif
			      else{
				  emlwrite(_("\007Can't move up a directory"),
					   NULL);
				  break;
			      }
			  }
		    }
		}
		else if((fb_flags&FB_SAVE) && p[0] == '.' && p[1] == '\0'){
		    if ((strlen(gmp->dname) < dirlen) && 
			(strlen(gmp->current->fname) < fnlen)){
			strncpy(dir, gmp->dname, dirlen);
			dir[dirlen-1] = '\0';
		    }

		    zotmaster(&gmp);
		    return(0);  /* just change the directory, still return no selection */
		}
		else{
		    if(tmp[1] != '\0'){		/* were in root? */
		      strncat(tmp, S_FILESEP, sizeof(tmp)-strlen(tmp)-1);
		      tmp[sizeof(tmp)-1] = '\0';
		    }

		    strncat(tmp, gmp->current->fname, sizeof(tmp)-strlen(tmp));
		    tmp[sizeof(tmp)-1] = '\0';
		}

		if((mp = getfcells(tmp, fb_flags)) == NULL)
		  /* getfcells should explain what happened */
		  break;

		if(gmp->flags & FB_LMODE){
		    mp->flags |= FB_LMODE;
		    mp->lm = gmp->lm;
		    gmp->lm = NULL;
		}

		zotmaster(&gmp);
		gmp = mp;
		tp  = NULL;

		if(*child){
		    if((tp = FindCell(gmp, child)) != NULL){
			gmp->current = tp;
			PlaceCell(gmp, gmp->current, &row, &col);
		    }
		    else{
			eml.s = child;
			emlwrite(_("\007Problem finding dir \"%s\""), &eml);
		    }
		}

		PaintBrowser(gmp, 0, &crow, &ccol);
		if(!*child){
		  char b[100];

		  snprintf(b, sizeof(b), "Select/View \" .. %s %s\" to return to previous directory.", PARENTDIR, DIRWORD);
		  emlwrite(b, NULL);
		}

		break;
	    }
	    else if(gmode&MDBRONLY){
		snprintf(child, sizeof(child), "%s%c%s", gmp->dname, C_FILESEP, 
			gmp->current->fname);

		if(LikelyASCII(child)){
		    snprintf(tmp, sizeof(tmp), "%s \'%s\'",
			    (envp = (char *) getenv("PAGER"))
			      ? envp : BROWSER_PAGER, child);
		    BrowserRunChild(tmp, gmp->dname);
		    PaintBrowser(gmp, 0, &crow, &ccol);
		}

		break;
	    }
	    else{				/* just return */
	      if(gmp->flags & FB_LMODEPOS){

		  if(!lmreturn){
		      emlwrite("Programming error, called FileBrowse with LMODEPOS but no lmreturn", NULL);
		      zotmaster(&gmp);
		      return(-1);
		  }

		  /* user actually used ListMode, the list is finished */
		  if(gmp->flags & FB_LMODE){
		      if(!gmp->lm){
			  (*term.t_beep)();
			  emlwrite(_("No files are selected, use \"X\" to mark files for selection"), NULL);
			  break;
		      }

		      *lmreturn = gmp->lm;
		      gmp->lm = NULL;
		  }
		  else{		/* construct an lmreturn for user */
		      LMLIST *new;
		      size_t flen, dlen;

		      if((new=(LMLIST *)malloc(sizeof(*new))) == NULL
			 || (new->fname=malloc(gmp->current->fname ? (flen=strlen(gmp->current->fname))+1 : 1)) == NULL
			 || (new->dir=malloc((dlen=strlen(gmp->dname))+1)) == NULL){
			emlwrite("\007Can't malloc space for filename", NULL);
			return(-1);
		      }

		      strncpy(new->fname,
			      gmp->current->fname ? gmp->current->fname : "", flen);
		      new->fname[flen] = '\0';
		      strncpy(new->dir, gmp->dname, dlen);
		      new->dir[dlen] = '\0';
		      strncpy(new->size, gmp->current->size, sizeof(new->size));
		      new->size[sizeof(new->size)-1] = '\0';
		      new->next = NULL;
		      *lmreturn = new;
		  }

		  zotmaster(&gmp);
		  return(1);
	      }

	      if ((strlen(gmp->dname) < dirlen) && 
		  (strlen(gmp->current->fname) < fnlen)){
		  strncpy(dir, gmp->dname, dirlen);
		  dir[dirlen-1] = '\0';
		  strncpy(fn, gmp->current->fname, fnlen);
		  fn[fnlen-1] = '\0';
	      }
	      else {
		zotmaster(&gmp);
		return(-1);
	      }
	      if(sz != NULL){			/* size uninteresting */
		strncpy(sz, gmp->current->size, szlen);
		sz[szlen-1] = '\0';
	      }
	      
	      zotmaster (&gmp);
	      return (1);
	    }
	    break;

	  case 'w':				/* Where is */
	  case 'W':
	  case (CTRL|'W'):
	    i = 0;

	    while(!i){

		switch(readpattern(_("File name to find"), FALSE)){
		  case HELPCH:
		    emlwrite(_("\007No help yet!"), NULL);
/* remove break and sleep after help text is installed */
		    sleep(3);
		    break;
		  case (CTRL|'L'):
		    PaintBrowser(gmp, 0, &crow, &ccol);
		    break;
		  case (CTRL|'Y'):		/* first first cell */
		    for(tp = gmp->top; tp->prev; tp = tp->prev)
		      ;

		    i++;
		    /* fall thru to repaint */
		  case (CTRL|'V'):
		    if(!i){
			do{
			    tp = gmp->top;
			    if((i = term.t_nrow - term.t_mrow - 2) <= 0)
			      break;

			    while(i-- && tp->next){
				j = 0;
				while(++j <= gmp->fpl  && tp->next)
				  tp = tp->next;
			    }

			    if(i < 0)
			      gmp->top = tp;
			}
			while(tp->next);

			emlwrite(_("Searched to end of directory"), NULL);
		    }
		    else
		      emlwrite(_("Searched to start of directory"), NULL);

		    if(tp){
			PlaceCell(gmp, gmp->current, &row, &col);
			PaintCell(row, col, gmp->cpf, gmp->current, 0);
			gmp->current = tp;
			if(PlaceCell(gmp, gmp->current, &row, &col)){
			    PaintBrowser(gmp, 1, &crow, &ccol);
			}
			else{
			    PaintCell(row, col, gmp->cpf, gmp->current, 1);
			    crow = row;
			    ccol = col;
			}
		    }

		    i++;			/* make sure we jump out */
		    break;
		  case ABORT:
		    emlwrite(_("Whereis cancelled"), NULL);
		    i++;
		    break;
		  case FALSE:
		    mlerase();
		    i++;
		    break;
		  case TRUE:
		    {
		    char *utf8 = NULL;

		    if(pat && pat[0])
		      utf8 = ucs4_to_utf8_cpystr(pat);

		    if(utf8 && (tp = FindCell(gmp, utf8)) != NULL){
			PlaceCell(gmp, gmp->current, &row, &col);
			PaintCell(row, col, gmp->cpf, gmp->current, 0);
			gmp->current = tp;

			if(PlaceCell(gmp, tp, &row, &col)){ /* top changed */
			    PaintBrowser(gmp, 1, &crow, &ccol);
			}
			else{
			    PaintCell(row, col, gmp->cpf, gmp->current, 1);
			    crow = row;
			    ccol = col;
			}
			mlerase();
		    }
		    else{
			eml.s = utf8;
			emlwrite(_("\"%s\" not found"), &eml);
		    }

		    if(utf8)
		      fs_give((void **) &utf8);
		    }

		    i++;
		    break;
		  default:
		    break;
		}
	    }

	    BrowserKeys();
	    break;

	  case (CTRL|'\\') :
#if defined MOUSE && !defined(_WINDOWS)
	    toggle_xterm_mouse(0,1);
#else
	    unknown_command(c);
#endif
	    break;

	  case (CTRL|'Z'):
	    if(gmode&MDSSPD){
		bktoshell(0, 1);
		PaintBrowser(gmp, 0, &crow, &ccol);
		break;
	    }					/* fall thru with error! */

	  default:				/* what? */
	  Default:
	    unknown_command(c);

	  case NODATA:				/* no op */
	    break;
	}
    }
}


/*
 * getfcells - make a master browser struct and fill it in
 *             return NULL if there's a problem.
 */
struct bmaster *
getfcells(char *dname, int fb_flags)
{
    int  i, 					/* various return codes */
         flength,
         nentries = 0;				/* number of dir ents */
    off_t attsz;
    char *np,					/* names of files in dir */
         *dcp,                                  /* to add file to path */
         *tmpstr,
         **filtnames,				/* array filtered names */
	 errbuf[NLINE];
    struct fcell *ncp,				/* new cell pointer */
                 *tcp;				/* trailing cell ptr */
    struct bmaster *mp;
    EML  eml;

    errbuf[0] = '\0';
    if((mp=(struct bmaster *)malloc(sizeof(struct bmaster))) == NULL){
	emlwrite("\007Can't malloc space for master filename cell", NULL);
	return(NULL);
    }

    memset(mp, 0, sizeof(*mp));

    if(dname[0] == '.' && dname[1] == '\0'){		/* remember this dir */
	if(!getcwd(mp->dname, 256))
	  mp->dname[0] = '\0';
    }
    else if(dname[0] == '.' && dname[1] == '.' && dname[2] == '\0'){
	if(!getcwd(mp->dname, 256))
	  mp->dname[0] = '\0';
	else{
	    if((np = (char *)strrchr(mp->dname, C_FILESEP)) != NULL)
	      if(np != mp->dname)
		*np = '\0';
	}
    }
    else{
	strncpy(mp->dname, dname, sizeof(mp->dname));
	mp->dname[sizeof(mp->dname)-1] = '\0';
    }

    mp->head = mp->top = NULL;
    mp->cpf = mp->fpl = 0;
    mp->longest = 5;				/* .. must be labeled! */
    mp->flags = fb_flags;

    eml.s = mp->dname;
    emlwrite("Building file list of %s...", &eml);

    if((mp->names = getfnames(mp->dname, NULL, &nentries, errbuf, sizeof(errbuf))) == NULL){
	free((char *) mp);
	if(*errbuf)
	  emlwrite(errbuf, NULL);

	return(NULL);
    }

    /*
     * this is the fun part.  build an array of pointers to the fnames we're
     * interested in (i.e., do any filtering), then pass that off to be
     * sorted before building list of cells...
     *
     * right now default filtering on ".*" except "..", but this could
     * easily be made a user option later on...
     */
    if((filtnames=(char **)malloc((nentries+1) * sizeof(char *))) == NULL){
	emlwrite("\007Can't malloc space for name array", NULL);
	zotmaster(&mp);
	return(NULL);
    }

    i = 0;					/* index of filt'd array */
    np = mp->names;
    while(nentries--){
	int ii;
	int width;

	/*
	 * Filter dot files?  Always filter ".", never filter "..",
	 * and sometimes fitler ".*"...
	 */
	if(*np == '.' && (!(*(np+1) == '.' && *(np+2) == '\0')
			  && !(*(np+1) == '\0' && (fb_flags&FB_SAVE)))
	   && (*(np+1) == '\0' || !(gmode & MDDOTSOK))){
	    np += strlen(np) + 1;
	    continue;
	}

	filtnames[i++] = np;

	ii = (int) strlen(np);
	if((width = (int) utf8_width(np)) > mp->longest)
	  mp->longest = width;			/* remember longest */

	np += ii + 1;				/* advance name pointer */
    }

    nentries = i;				/* new # of entries */

    /* 
     * sort files case independently
     */
    qsort((qsort_t *)filtnames, (size_t)nentries, sizeof(char *), sstrcasecmp);

    /* 
     * this is so we use absolute path names for stats.
     * remember: be careful using dname as directory name, and fix mp->dname
     * when we're done
     */
    dcp = (char *) strchr(mp->dname, '\0');
    if(dcp == mp->dname || dcp[-1] != C_FILESEP){
	dcp[0] = C_FILESEP;
	dcp[1] = '\0';
    }
    else
      dcp--;

    i = 0;
    while(nentries--){				/* stat filtered files */
	/* get a new cell */
	if((ncp=(struct fcell *)malloc(sizeof(struct fcell))) == NULL){
	    emlwrite("\007Can't malloc cells for browser!", NULL);
	    zotfcells(mp->head);		/* clean up cells */
	    free((char *) filtnames);
	    free((char *) mp);
	    return(NULL);			/* bummer. */
	}

	ncp->next = ncp->prev = NULL;

	if(mp->head == NULL){			/* tie it onto the list */
	    mp->head = mp->top = mp->current = ncp;
	}
	else{
	    tcp->next = ncp;
	    ncp->prev = tcp;
	}

	tcp = ncp;

	/* fill in the new cell */
	ncp->fname = filtnames[i++];

	/* fill in file's mode */
	if ((flength = strlen(ncp->fname) + 1 + strlen(dname)) < sizeof(mp->dname)){
	  strncpy(&dcp[1], ncp->fname, sizeof(mp->dname)-(dcp+1-mp->dname)); /* use absolute path! */
	  mp->dname[sizeof(mp->dname)-1] = '\0';
	  tmpstr = mp->dname;
	}
	else{
	  if((tmpstr = (char *)malloc((flength+1)*sizeof(char))) == NULL){
	    emlwrite("\007Can't malloc cells for temp buffer!", NULL);
            zotfcells(mp->head);                /* clean up cells */
            free((char *) filtnames);
            free((char *) mp);
            return(NULL);                       /* bummer. */
	  }

	  strncpy(tmpstr, dname, flength);
	  tmpstr[flength] = '\0';
	  tmpstr = strncat(tmpstr, S_FILESEP, flength+1-1-strlen(tmpstr));
	  tmpstr[flength] = '\0';
	  tmpstr = strncat(tmpstr, ncp->fname, flength+1-1-strlen(tmpstr));
	  tmpstr[flength] = '\0';
	}

	switch(fexist(tmpstr, "t", &attsz)){
	  case FIODIR :
	    ncp->mode = FIODIR;
	    snprintf(ncp->size, sizeof(ncp->size), "(%s%s%s)",
		    (ncp->fname[0] == '.' && ncp->fname[1] == '.'
		     && ncp->fname[2] == '\0') ? PARENTDIR : 
		    ((fb_flags&FB_SAVE) && ncp->fname[0] == '.' && ncp->fname[1] == '\0'
		     ? SELECTWORD : ""),
		    (ncp->fname[0] == '.' && ncp->fname[1] == '.'
		     && ncp->fname[2] == '\0') ? " " : 
		    ((fb_flags&FB_SAVE) && ncp->fname[0] == '.' && ncp->fname[1] == '\0'
		     ? " " : ""),
		     DIRWORD);
	    break;

	  case FIOSYM :
	    ncp->mode = FIOSYM;
	    strncpy(ncp->size, "--", sizeof(ncp->size));
	    ncp->size[sizeof(ncp->size)-1] = '\0';
	    break;

	  default :
	    ncp->mode = FIOSUC;			/* regular file */
	    strncpy(ncp->size, prettysz(attsz), sizeof(ncp->size));
	    ncp->size[sizeof(ncp->size)-1] = '\0';
	    break;
	}

	if (flength >= NLINE)
	  free((char *) tmpstr);
    }

    dcp[(dcp == mp->dname) ? 1 : 0] = '\0';	/* remember to cap dname */
    free((char *) filtnames);			/* 'n blast filt'd array*/

    percdircells(mp);
    layoutcells(mp);
    if(strlen(mp->dname) < sizeof(browse_dir)){
      strncpy(browse_dir, mp->dname, sizeof(browse_dir));
      browse_dir[sizeof(browse_dir)-1] = '\0';
    }
    else
      browse_dir[0] = '\0';

    return(mp);
}


int
fcell_is_selected(struct fcell *cell, struct bmaster *mp)
{
    LMLIST *lm;

    if(cell && cell->fname){
	for(lm = mp ? mp->lm : NULL; lm; lm = lm->next){
	    /* directory has to match */
	    if(!((mp->dname[0] == '\0' && (!lm->dir || lm->dir[0] =='\0'))
	         || (mp->dname[0] != '\0' && lm->dir && lm->dir[0] !='\0'
	             && !strcmp(mp->dname, lm->dir))))
	      continue;

	    if(lm->fname && !strcmp(cell->fname, lm->fname))
	      return(1);
	}
    }
    
    return(0);
}


/*
 * Adds a new name to the head of the lmlist
 */
void
add_cell_to_lmlist(struct fcell *cell, struct bmaster *mp)
{
    LMLIST *new;
    size_t flen, dlen;

    if(mp && cell && cell->fname && cell->fname[0]){
	if((new=(LMLIST *)malloc(sizeof(*new))) == NULL ||
	   (new->fname=malloc(sizeof(char)*((flen=strlen(cell->fname))+1))) == NULL ||
	   (new->dir=malloc(sizeof(char)*((dlen=strlen(mp->dname))+1))) == NULL){
	    emlwrite("\007Can't malloc space for filename", NULL);
	    return;
	}

	strncpy(new->fname, cell->fname, flen);
	new->fname[flen] = '\0';
	strncpy(new->dir, mp->dname, dlen);
	new->dir[dlen] = '\0';
	new->size[0] = '\0';
	if(cell->size[0]){
	    strncpy(new->size, cell->size, sizeof(new->size));
	    new->size[sizeof(new->size)-1] = '\0';
	}

	new->next = mp->lm;
	mp->lm = new;
    }
}


/*
 * Deletes a name from the lmlist
 */
void
del_cell_from_lmlist(struct fcell *cell, struct bmaster *mp)
{
    LMLIST *lm, *lmprev = NULL;

    if(mp && cell && cell->fname && cell->fname[0])
      for(lm = mp ? mp->lm : NULL; lm; lm = lm->next){
	  if(lm->fname && strcmp(cell->fname, lm->fname) == 0){
	      free((char *) lm->fname);
	      if(lm->dir)
	        free((char *) lm->dir);

	      if(lmprev)
		lmprev->next = lm->next;
	      else
		mp->lm = lm->next;
	    
	      free((char *) lm);

	      break;
	  }

	  lmprev = lm;
      }
}


/*
 * PaintCell - print the given cell at the given location on the display
 *             the format of a printed cell is:
 *
 *                       "<fname>       <size>  "
 */
void
PaintCell(int row, int col,
	  int sc,	/* screen columns available for this cell */
	  struct fcell *cell, int inverted)
{
    char  buf1[NLINE], buf2[NLINE];
    char  lbuf[5];
    int   need, l_wid, f_wid, f_to_s_wid, s_wid;
    UCS  *ucs;

    if(cell == NULL)
	return;

    l_wid = (gmp && ((gmp->flags & FB_LMODEPOS) > 4)) ? 4 : 0;
    f_wid = utf8_width(cell->fname ? cell->fname : "");
    f_to_s_wid = 1;
    s_wid = utf8_width(cell->size ? cell->size : "");

    /* the two is the space between cell columns */
    sc = MIN(sc-2, term.t_ncol-col);

    if(l_wid){
	if(gmp && gmp->flags & FB_LMODE && cell->mode != FIODIR){
	    /*
	     * We have to figure out here if it is selected or not
	     * and use that to write the X or space.
	     */
	    lbuf[0] = '[';
	    if(fcell_is_selected(cell, gmp))
	      lbuf[1] = 'X';
	    else
	      lbuf[1] = ' ';

	    lbuf[2] = ']';
	    lbuf[3] = ' ';
	}
	else{
	    lbuf[0] = lbuf[1] = lbuf[2] = lbuf[3] = ' ';
	}

	lbuf[4] = '\0';
    }

    sc -= l_wid;

    need = f_wid+f_to_s_wid+s_wid;

    /* space between name and size */
    if(need < sc)
      f_to_s_wid += (sc-need);

    /*
     * If the width isn't enough to handle everything we're just putting a single
     * space between fname and size and truncating on the right. That means that
     * the sizes in the right hand column won't line up correctly when there is
     * a lack of space. Instead, we opt for displaying as much info as possible
     * in a slightly discordant way.
     */

    movecursor(row, col);
    if(l_wid){
	ucs = utf8_to_ucs4_cpystr(lbuf);
	if(ucs){
	    pputs(ucs, 0);
	    fs_give((void **) &ucs);
	}
    }

    utf8_snprintf(buf1, sizeof(buf1), "%*.*w%*.*w%*.*w",
		  f_wid, f_wid, cell->fname ? cell->fname : "",
		  f_to_s_wid, f_to_s_wid, "",
		  s_wid, s_wid, cell->size ? cell->size : "");

    utf8_snprintf(buf2, sizeof(buf2), "%*.*w", sc, sc, buf1);

    if(inverted)
      (*term.t_rev)(1);

    ucs = utf8_to_ucs4_cpystr(buf2);
    if(ucs){
	pputs(ucs, 0);
	fs_give((void **) &ucs);
    }

    if(inverted)
      (*term.t_rev)(0);
}


/*
 * PaintBrowse - with the current data, display the browser.  if level == 0 
 *               paint the whole thing, if level == 1 just paint the cells
 *               themselves
 */
void
PaintBrowser(struct bmaster *mp, int level, int *row, int *col)
{
    int i, cl;
    struct fcell *tp;

    if(!level){
	ClearBrowserScreen();
	BrowserAnchor(mp->dname);
    }

    i = 0;
    tp = mp->top;
    cl = COMPOSER_TOP_LINE;			/* current display line */
    while(tp){

	PaintCell(cl, mp->cpf * i, mp->cpf, tp, tp == mp->current);

	if(tp == mp->current){
	    if(row)
	      *row = cl;

	    if(col)
	      *col = mp->cpf * i;
	}

	if(++i >= mp->fpl){
	    i = 0;
	    if(++cl > term.t_nrow-(term.t_mrow+1))
	      break;
	}

	tp = tp->next;
    }

    if(level){
	while(cl <= term.t_nrow - (term.t_mrow+1)){
	    if(!i)
	      movecursor(cl, 0);
	    peeol();
	    movecursor(++cl, 0);
	}
    }
    else{
	BrowserKeys();
    }
}


/*
 * BrowserKeys - just paints the keyhelp at the bottom of the display
 */
void
BrowserKeys(void)
{
    menu_browse[QUIT_KEY].name  = (gmode&MDBRONLY) ? "Q" : "E";
    /* TRANSLATORS: Brwsr is an abbreviation for Browser, which is
       a command used to look through something */
    menu_browse[QUIT_KEY].label = (gmode&MDBRONLY) ? N_("Quit") : N_("Exit Brwsr");
    menu_browse[GOTO_KEY].name  = (gmode&MDGOTO) ? "G" : NULL;
    menu_browse[GOTO_KEY].label = (gmode&MDGOTO) ? N_("Goto") : NULL;
    if(gmode & MDBRONLY){
	menu_browse[EXEC_KEY].name  = "L";
	menu_browse[EXEC_KEY].label = N_("Launch");
	menu_browse[SELECT_KEY].name  = "V";
	menu_browse[SELECT_KEY].label = "[" N_("View") "]";
	menu_browse[PICO_KEY].name  = "E";
	menu_browse[PICO_KEY].label = N_("Edit");
    }
    else{
	menu_browse[SELECT_KEY].name  = "S";
	menu_browse[SELECT_KEY].label = "[" N_("Select") "]";
	menu_browse[PICO_KEY].name  = "A";
	menu_browse[PICO_KEY].label = N_("Add");

	if(gmp && gmp->flags & FB_LMODEPOS){
	    if(gmp && gmp->flags & FB_LMODE){	/* ListMode is already on */
		menu_browse[EXEC_KEY].name  = "X";
		menu_browse[EXEC_KEY].label = N_("Set/Unset");
	    }
	    else{				/* ListMode is possible   */
		menu_browse[EXEC_KEY].name  = "L";
		menu_browse[EXEC_KEY].label = N_("ListMode");
	    }
	}
	else{					/* No ListMode possible   */
	    menu_browse[EXEC_KEY].name  = NULL;
	    menu_browse[EXEC_KEY].label = NULL;
	}
    }

    wkeyhelp(menu_browse);
}


/*
 * layoutcells - figure out max length of cell and how many cells can 
 *               go on a line of the display
 */
void
layoutcells(struct bmaster *mp)
{
    static int wid = -1;
    /*
     * Max chars/file. Actually this is max screen cells/file
     * Longest name + "(parent dir)"
     */
    if(wid < 0)
      wid = MAX(utf8_width(PARENTDIR),utf8_width(SELECTWORD)) + utf8_width(DIRWORD);

    mp->cpf = mp->longest + wid + 3;

    if(mp->flags & FB_LMODEPOS)			/* "[X] " */
      mp->cpf += 4;

    if(gmode & MDONECOL){
	mp->fpl = 1;
    }
    else{
	int i = 1;

	while(i*mp->cpf - 2 <= term.t_ncol)		/* no force... */
	  i++;					/* like brute force! */

	mp->fpl = i - 1;			/* files per line */
    }

    if(mp->fpl == 0)
      mp->fpl = 1;
}


/*
 * percdircells - bubble all the directory cells to the top of the
 *                list.
 */
void
percdircells(struct bmaster *mp)
{
    struct fcell *dirlp,			/* dir cell list pointer */
                 *lp, *nlp;			/* cell list ptr and next */

    dirlp = NULL;
    for(lp = mp->head; lp; lp = nlp){
	nlp = lp->next;
	if(lp->mode == FIODIR){
	    if(lp->prev)			/* clip from list */
	      lp->prev->next = lp->next;

	    if(lp->next)
	      lp->next->prev = lp->prev;

	    if((lp->prev = dirlp) != NULL){	/* tie it into dir portion */
		if((lp->next = dirlp->next) != NULL)
		  lp->next->prev = lp;

		dirlp->next = lp;
		dirlp = lp;
	    }
	    else{
		if((dirlp = lp) != mp->head)
		  dirlp->next = mp->head;

		if(dirlp->next)
		  dirlp->next->prev = dirlp;

		mp->head = mp->top = mp->current = dirlp;
	    }
	}
    }
}


/*
 * PlaceCell - given a browser master and a cell, return row and col of the display that
 *             it should go on.  
 *
 *             return 1 if mp->top has changed, x,y relative to new page
 *             return 0 if otherwise (same page)
 *             return -1 on error
 */
int
PlaceCell(struct bmaster *mp, struct fcell *cp, int *x, int *y)
{
    int cl = COMPOSER_TOP_LINE;			/* current line */
    int ci = 0;					/* current index on line */
    int rv = 0;
    int secondtry = 0;
    struct fcell *tp;

    /* will cp fit on screen? */
    tp = mp->top;
    while(1){
	if(tp == cp){				/* bingo! */
	    *x = cl;
	    *y = ci * mp->cpf;
	    break;
	}

	if((tp = tp->next) == NULL){		/* above top? */
	    if(secondtry++){
		emlwrite("\007Internal error: can't find fname cell", NULL);
		return(-1);
	    }
	    else{
		tp = mp->top = mp->head;	/* try from the top! */
		cl = COMPOSER_TOP_LINE;
		ci = 0;
		rv = 1;
		continue;			/* start over! */
	    }
	}

	if(++ci >= mp->fpl){			/* next line? */
	    ci = 0;
	    if(++cl > term.t_nrow-(term.t_mrow+1)){ /* next page? */
		ci = mp->fpl;			/* tp is at bottom right */
		while(ci--)			/* find new top */
		  tp = tp->prev;
		mp->top = tp;
		ci = 0;
		cl = COMPOSER_TOP_LINE;		/* keep checking */
		rv = 1;
	    }
	}

    }

    /* not on display! */
    return(rv);
}


/*
 * zotfcells - clean up malloc'd cells of file names
 */
void
zotfcells(struct fcell *hp)
{
    struct fcell *tp;

    while(hp){
	tp = hp;
	hp = hp->next;
	tp->next = NULL;
	free((char *) tp);
    }
}


/*
 * zotmaster - blast the browser master struct
 */
void
zotmaster(struct bmaster **mp)
{
    if(mp && *mp){
	zotfcells((*mp)->head);				/* free cells       */
	zotlmlist((*mp)->lm);				/* free lmlist      */
	if((*mp)->names)
	  free((char *)(*mp)->names);			/* free file names  */

	free((char *)*mp);				/* free master      */
	*mp = NULL;					/* make double sure */
    }
}


/*
 * FindCell - starting from the current cell find the first occurance of 
 *            the given string wrapping around if necessary
 */
struct fcell *
FindCell(struct bmaster *mp, char *utf8string)
{
    struct fcell *tp, *fp;

    if(*utf8string == '\0')
      return(NULL);

    fp = NULL;
    tp = mp->current->next;
    
    while(tp && !fp){
	if(sisin(tp->fname, utf8string))
	  fp = tp;
	else
	  tp = tp->next;
    }

    tp = mp->head;
    while(tp != mp->current && !fp){
	if(sisin(tp->fname, utf8string))
	  fp = tp;
	else
	  tp = tp->next;
    }

    return(fp);
}


/*
 * sisin - case insensitive substring matching function
 *
 *  We can't really do case-insensitive for non-ascii, so restrict
 *  that to ascii characters. The strings will be utf8.
 */
int
sisin(char *bigstr, char *utf8substr)
{
    register int j;

    while(*bigstr){
	j = 0;
	while(bigstr[j] == utf8substr[j]
	      || ((unsigned char)bigstr[j] < 0x80 && (unsigned char)utf8substr[j] < 0x80
		  && toupper((unsigned char)bigstr[j]) == toupper((unsigned char)utf8substr[j])))
	  if(utf8substr[++j] == '\0')			/* bingo! */
	    return(1);

	bigstr++;
    }

    return(0);
}


/*
 * set_browser_title - 
 */
void
set_browser_title(char *s)
{
    browser_title = s;
}


/*
 * BrowserAnchor - draw the browser's anchor line.
 */
void
BrowserAnchor(char *utf8dir)
{
    char *pdir;
    char  titlebuf[NLINE];
    char  buf[NLINE];
    char  dirbuf[NLINE];
    char *dots = "...";
    char *br = "BROWSER";
    char *dir = "Dir: ";
    UCS  *ucs;
    int   need, extra, avail;
    int   title_wid, b_wid, t_to_b_wid, b_to_d_wid, d_wid, dot_wid, dir_wid, after_dir_wid;
    COLOR_PAIR *lastc = NULL;

    if(!utf8dir)
      utf8dir = "";

    pdir = utf8dir;

    if(browser_title)
      snprintf(titlebuf, sizeof(buf), "  %s", browser_title);
    else if(Pmaster)
      snprintf(titlebuf, sizeof(buf), "  ALPINE %s", Pmaster->pine_version);
    else
      snprintf(titlebuf, sizeof(buf), "  UW PICO %s", (gmode&MDBRONLY) ? "BROWSER" : version);

    title_wid = utf8_width(titlebuf);
    t_to_b_wid = 15;
    b_wid = utf8_width(br);
    b_to_d_wid = 4;
    d_wid = utf8_width(dir);
    dot_wid = 0;
    dir_wid = utf8_width(pdir);

    need = title_wid+t_to_b_wid+b_wid+b_to_d_wid+d_wid+dot_wid+dir_wid;

    if(need > term.t_ncol){
	extra = need - term.t_ncol;
	t_to_b_wid -= MIN(extra, 10);
	need -= MIN(extra, 10);

	if(need > term.t_ncol){
	    extra = need - term.t_ncol;
	    b_to_d_wid -= MIN(extra, 2);
	    need -= MIN(extra, 2);
	}

	if(need > term.t_ncol){
	    titlebuf[0] = titlebuf[1] = ' ';
	    titlebuf[2] = '\0';
	    title_wid = utf8_width(titlebuf);
	    t_to_b_wid = 0;
	    b_to_d_wid = 4;
	    need = title_wid+t_to_b_wid+b_wid+b_to_d_wid+d_wid+dot_wid+dir_wid;
	    if(need > term.t_ncol){
		extra = need - term.t_ncol;
		b_to_d_wid -= MIN(extra, 2);
		need -= MIN(extra, 2);
	    }

	    need = title_wid+t_to_b_wid+b_wid+b_to_d_wid+d_wid+dot_wid+dir_wid;
	    if(need > term.t_ncol){
		t_to_b_wid = 0;
		b_wid = 0;
		b_to_d_wid = 0;
	    }

	    need = title_wid+t_to_b_wid+b_wid+b_to_d_wid+d_wid+dot_wid+dir_wid;
	    if(need > term.t_ncol)
	      d_wid = 0;

	    need = title_wid+t_to_b_wid+b_wid+b_to_d_wid+d_wid+dot_wid+dir_wid;

	    if(need > term.t_ncol && dir_wid > 0){
		dot_wid = utf8_width(dots);
		need = title_wid+t_to_b_wid+b_wid+b_to_d_wid+d_wid+dot_wid+dir_wid;
		while(need > term.t_ncol && (pdir = strchr(pdir+1, C_FILESEP))){
		    dir_wid = utf8_width(pdir);
		    need = title_wid+t_to_b_wid+b_wid+b_to_d_wid+d_wid+dot_wid+dir_wid;
		}

		if(!pdir){		/* adjust other widths to fill up space */
		    avail = term.t_ncol - (title_wid+t_to_b_wid+b_wid+b_to_d_wid+d_wid+dot_wid);
		    if(avail > 2)
		      utf8_to_width_rhs(dirbuf, utf8dir, sizeof(dirbuf), avail);
		    else
		      dirbuf[0] = '\0';

		    pdir = dirbuf;
		}

		dir_wid = utf8_width(pdir);
	    }
	}
    }

    extra = term.t_ncol - (title_wid+t_to_b_wid+b_wid+b_to_d_wid+d_wid+dot_wid+dir_wid);

    if(extra >= 0)
      after_dir_wid = extra;
    else
      after_dir_wid = 0;

    utf8_snprintf(buf, sizeof(buf), "%*.*w%*.*w%*.*w%*.*w%*.*w%*.*w%*.*w%*.*w",
		  title_wid, title_wid, titlebuf,
		  t_to_b_wid, t_to_b_wid, "",
		  b_wid, b_wid, br,
		  b_to_d_wid, b_to_d_wid, "",
		  d_wid, d_wid, dir,
		  dot_wid, dot_wid, dots,
		  dir_wid, dir_wid, pdir,
		  after_dir_wid, after_dir_wid, "");

    /* just making sure */
    utf8_snprintf(titlebuf, sizeof(titlebuf), "%-*.*w", term.t_ncol, term.t_ncol, buf);

    movecursor(0, 0);
    if(Pmaster && Pmaster->colors && Pmaster->colors->tbcp
       && pico_is_good_colorpair(Pmaster->colors->tbcp)){
	lastc = pico_get_cur_color();
	(void) pico_set_colorp(Pmaster->colors->tbcp, PSC_NONE);
    }
    else
      (*term.t_rev)(1);

    ucs = utf8_to_ucs4_cpystr(titlebuf);
    if(ucs){
	pputs(ucs, 0);
	fs_give((void **) &ucs);
    }

    if(lastc){
	(void) pico_set_colorp(lastc, PSC_NONE);
	free_color_pair(&lastc);
    }
    else
      (*term.t_rev)(0);
}


/*
 * ResizeBrowser - handle a resize event
 */
int
ResizeBrowser(void)
{
    if(gmp){
	layoutcells(gmp);
	PaintBrowser(gmp, 0, NULL, NULL);
	return(1);
    }
    else
      return(0);
}


void
ClearBrowserScreen(void)
{
    int i;

    for(i = 0; i <= term.t_nrow; i++){		/* clear screen */
	movecursor(i, 0);
	peeol();
    }
}


void
BrowserRunChild(char *child, char *dir)
{
    int    status;
    char   tmp[NLINE];
    time_t t_in, t_out;

    ClearBrowserScreen();
    movecursor(0, 0);
    (*term.t_close)();
    if(!isdir(dir, NULL, &t_in))
      t_in = 0;

    fflush(stdout);
    status = system(child);
    (*term.t_open)();
    if(t_in && isdir(dir, NULL, &t_out) && t_in < t_out){
	struct bmaster *mp;

	if((mp = getfcells(dir, 0)) != NULL){
	    zotmaster(&gmp);
	    gmp = mp;
	}
	/* else getfcells should explain what happened */
    }

    /* complain about non-zero exit status */
    if((status >> 8) & 0xff){

	movecursor(term.t_nrow - 1, 0);
	snprintf(tmp, sizeof(tmp), "[ \"%.30s\" exit with error value: %d ]",
		 child, (status >> 8) & 0xff);
	pputs_utf8(tmp, 1);
	movecursor(term.t_nrow, 0);
	pputs_utf8("[ Hit RETURN to continue ]", 1);
	fflush(stdout);

	while(GetKey() != (CTRL|'M')){
	    (*term.t_beep)();
	    fflush(stdout);
	}
    }
}


/*
 * imitate pc-pine memory for where we last called the file browser.
 */
void
p_chdir(struct bmaster *mp)
{
    if(mp && mp->dname)
      chdir(mp->dname);
}
#else /* _WINDOWS */


/*
 * pico_file_browse - Exported version of FileBrowse below.
 */
int
pico_file_browse(PICO *pdata, char *dir, size_t dirlen, char *fn, size_t fnlen,
		 char *sz, size_t szlen, int flags)
{
    return(FileBrowse(dir, dirlen, fn, fnlen, sz, szlen, flags, NULL));
}

int
ResizeBrowser (void)
{
    return (0);
}

/*
 * FileBrowse - Windows version of above function
 */
int 
FileBrowse(char *dir, size_t dirlen, char *fn, size_t fnlen,
	   char *sz, size_t szlen, int fb_flags, LMLIST **lmreturn)
{
    struct stat sbuf;
    int			rc;
    char		lfn[NLINE];
    
    if (fb_flags & FB_SAVE){
	rc = mswin_savefile(dir, dirlen, fn, MIN(dirlen,fnlen));
    }
    else{
	*fn = '\0';				/* No initial file names for 
						 * open. */
	if(fb_flags & FB_LMODEPOS){
	    /*
	     * We're going to allow multiple filenames to be returned so
	     * we don't want to use the passed in fn for that. Instead, make
	     * a bigger space and use that.
	     */
	    char f[20000];
	    size_t flen, dlen;

	    memset(f, 0, sizeof(f));

	    rc = mswin_multopenfile(dir, dirlen, f, sizeof(f),
		   (fb_flags & FB_ATTACH) ? NULL : "Text Files (*.txt)#*.txt");

	    if(rc == 1){
		LMLIST *lmhead = NULL, *new;
		char   *p;

		/*
		 * Build an LMLIST to return to the caller.
		 */
		for(p = f; *p; p += strlen(p)+1){
		    flen = strlen(p);
		    dlen = strlen(dir ? dir : "");
		    new = (LMLIST *) fs_get(sizeof(*new));
		    new->fname = (char *) fs_get((flen+1) * sizeof(char));
		    new->dir = (char *) fs_get((dlen+1) * sizeof(char));

		    strncpy(new->fname, p, flen);
		    new->fname[flen] = '\0';
		    strncpy(new->dir, dir ? dir : "", dlen);
		    new->dir[dlen] = '\0';

		    /* build full path to stat file. */
		    if((strlen(new->dir) + strlen(S_FILESEP) +
			strlen(new->fname) + 1) < sizeof(lfn)){
			strncpy(lfn, new->dir, sizeof(lfn));
			lfn[sizeof(lfn)-1] = '\0';
			strncat(lfn, S_FILESEP, sizeof(lfn)-strlen(lfn)-1);
			strncat(lfn, new->fname, sizeof(lfn)-strlen(lfn)-1);
			lfn[sizeof(lfn)-1] = '\0';
			if(our_stat(lfn, &sbuf) < 0)
			  strncpy(new->size, "0", 32);
			else
			  strncpy(new->size, prettysz((off_t)sbuf.st_size), 32);

			new->size[32-1] = '\0';
		    }

		    new->next = lmhead;
		    lmhead = new;
		}

		*lmreturn = lmhead;
		return(1);
	    }
	}
	else
	  rc = mswin_openfile (dir, dirlen, fn, fnlen,
		   (fb_flags & FB_ATTACH) ? NULL : "Text Files (*.txt)#*.txt");
    }

    if(rc == 1){
	if(sz != NULL){
	    /* build full path to stat file. */
	    if((strlen(dir) + strlen(S_FILESEP) + strlen(fn) + 1) > NLINE)
	      return -1;

	    strncpy(lfn, dir, sizeof(lfn));
	    lfn[sizeof(lfn)-1] = '\0';
	    strncat(lfn, S_FILESEP, sizeof(lfn)-strlen(lfn)-1);
	    lfn[sizeof(lfn)-1] = '\0';
	    strncat(lfn, fn, sizeof(lfn)-strlen(lfn)-1);
	    lfn[sizeof(lfn)-1] = '\0';
	    if(our_stat(lfn, &sbuf) < 0){
		strncpy(sz, "0", szlen);
		sz[szlen-1] = '\0';
	    }
	    else{
		strncpy(sz, prettysz ((off_t)sbuf.st_size), szlen);
		sz[szlen-1] = '\0';
	    }
	}

	return(1);
    }

    return(rc ? -1 : 0);
}

#endif	/* _WINDOWS */


/*
 * LikelyASCII - make a rough guess as to the displayability of the
 *		 given file.
 */
int
LikelyASCII(char *file)
{
#define	LA_TEST_BUF	1024
#define	LA_LINE_LIMIT	300
    int		   n, i, line, rv = FALSE;
    unsigned char  buf[LA_TEST_BUF];
    FILE	  *fp;
    EML            eml;

    if((fp = our_fopen(file, "rb")) != NULL){
	clearerr(fp);
	if((n = fread(buf, sizeof(char), LA_TEST_BUF * sizeof(char), fp)) > 0
	   || !ferror(fp)){
	    /*
	     * If we don't hit any newlines in a reasonable number,
	     * LA_LINE_LIMIT, of characters or the file contains NULLs,
	     * bag out...
	     */
	    rv = TRUE;
	    for(i = line = 0; i < n; i++)
	      if((line = (buf[i] == '\n') ? 0 : line + 1) >= LA_LINE_LIMIT
		 || !buf[i]){
		  rv = FALSE;
		  emlwrite(_("Can't display non-text file.  Try \"Launch\"."),
			   NULL);
		  break;
	      }
	}
	else{
	    eml.s = file;
	    emlwrite(_("Can't read file: %s"), &eml);
	}

	fclose(fp);
    }
    else{
	eml.s = file;
	emlwrite(_("Can't open file: %s"), &eml);
    }

    return(rv);
}


void
zotlmlist(LMLIST *lm)
{
    LMLIST *tp;

    while(lm){
	if(lm->fname)
	  free(lm->fname);

	if(lm->dir)
	  free(lm->dir);

	tp = lm;
	lm = lm->next;
	tp->next = NULL;
	free((char *) tp);
    }
}


/*
 * time_to_check - checks the current time against the last time called 
 *                 and returns true if the elapsed time is > below.
 *                 Newmail won't necessarily check, but we want to give it
 *                 a chance to check or do a keepalive.
 */
int
time_to_check(void)
{
    static time_t lasttime = 0L;

    if(!get_input_timeout())
      return(FALSE);

    if(time((time_t *) 0) - lasttime > (Pmaster ? (time_t)(FUDGE-10) : get_input_timeout())){
	lasttime = time((time_t *) 0);
	return(TRUE);
    }
    else
      return(FALSE);
}
