/*
 * $Id: edef.h 900 2008-01-05 01:13:26Z hubert@u.washington.edu $
 *
 * ========================================================================
 * Copyright 2006 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 *
 * Program:	Global definitions and initializations
 */

/*	EDEF:		Global variable definitions for
			MicroEMACS 3.2

			written by Dave G. Conroy
			modified by Steve Wilhite, George Jones
			greatly modified by Daniel Lawrence
*/

#ifndef	EDEF_H
#define	EDEF_H

#ifdef	maindef

/* for MAIN.C */

/* initialized global definitions */

int     fillcol = 72;                   /* Current fill column          */
int     userfillcol = -1;               /* Fillcol set from cmd line    */
UCS     pat[NPAT];                      /* Search pattern		*/
UCS     rpat[NPAT];                     /* Replace pattern		*/
int     sgarbk = TRUE;                  /* TRUE if keyhelp garbaged     */
int     sup_keyhelp = FALSE;            /* TRUE if keyhelp is suppressed*/
int     mline_open = FALSE;             /* TRUE if message line is open */
int	ComposerTopLine = 2;		/* TRUE if message line is open */
int	ComposerEditing = FALSE;	/* TRUE if editing the headers  */
char	modecode[] = "WCSEVO";		/* letters to represent modes	*/
long	gmode = MDWRAP|MDREPLACE;	/* global editor mode		*/
int     sgarbf  = TRUE;                 /* TRUE if screen is garbage	*/
int     mpresf  = FALSE;                /* TRUE if message in last line */
int	clexec	= FALSE;		/* command line execution flag	*/
char   *alt_speller = NULL;		/* alt spell checking command   */
int     preserve_start_stop = FALSE;    /* TRUE if pass ^S/^Q to term   */
UCS    *glo_quote_str = NULL;           /* points to quote string if set*/
char   *glo_quote_str_orig = NULL;
int     use_system_translation = FALSE;
char   *display_character_set = NULL;
char   *keyboard_character_set = NULL;
UCS    *glo_wordseps = NULL;            /* points to word separators if set */
char   *glo_wordseps_orig = NULL;

/* uninitialized global definitions */
int     currow;                 /* Cursor row                   */
int     curcol;                 /* Cursor column                */
int     thisflag;               /* Flags, this command          */
int     lastflag;               /* Flags, last command          */
int     curgoal;                /* Goal for C-P, C-N            */
char	opertree[NLINE+1];	/* operate within this tree     */
char    browse_dir[NLINE+1];    /* directory of last browse (cwd) */
WINDOW  *curwp;                 /* Current window               */
BUFFER  *curbp;                 /* Current buffer               */
WINDOW  *wheadp;                /* Head of list of windows      */
BUFFER  *bheadp;                /* Head of list of buffers      */
BUFFER  *blistp;                /* Buffer for C-X C-B           */

BUFFER  *bfind(char *, int, int); /* Lookup a buffer by name */
LINE    *lalloc(int used);   	/* Allocate a line              */
int	 km_popped;		/* menu popped up               */
#if	defined(HAS_TERMCAP) || defined(HAS_TERMINFO) || defined(VMS)
KBESC_T *kbesc;			/* keyboard esc sequence trie   */
#endif /* HAS_TERMCAP/HAS_TERMINFO/VMS */
void    *input_cs;		/* passed to mbtow() via kbseq() */

#else  /* maindef */

/* for all the other .C files */

/* initialized global external declarations */

extern  int     fillcol;                /* Fill column                  */
extern  int     userfillcol;            /* Fillcol set from cmd line    */
extern  UCS     pat[];                  /* Search pattern               */
extern	UCS     rpat[];                 /* Replace pattern		*/
extern  int     sgarbk;
extern  int     sup_keyhelp;
extern  int     mline_open;             /* Message line is open         */
extern	int	ComposerTopLine;	/* TRUE if message line is open */
extern	int	ComposerEditing;	/* TRUE if message line is open */
extern	char	modecode[];		/* letters to represent modes	*/
extern	KEYTAB	keytab[];		/* key bind to functions table	*/
extern	KEYTAB	pkeytab[];		/* pico's function table	*/
extern	long	gmode;			/* global editor mode		*/
extern  int     sgarbf;                 /* State of screen unknown      */
extern  int     mpresf;                 /* Stuff in message line        */
extern	int	clexec;			/* command line execution flag	*/
extern	char   *alt_speller;		/* alt spell checking command   */
extern  int     preserve_start_stop;    /* TRUE if pass ^S/^Q to term   */
extern  UCS    *glo_quote_str;          /* points to quote string if set*/
extern  char   *glo_quote_str_orig;
extern  int     use_system_translation;
extern  char   *display_character_set;
extern  char   *keyboard_character_set;
extern  UCS    *glo_wordseps;
extern  char   *glo_wordseps_orig;
/* initialized global external declarations */
extern  int     currow;                 /* Cursor row                   */
extern  int     curcol;                 /* Cursor column                */
extern  int     thisflag;               /* Flags, this command          */
extern  int     lastflag;               /* Flags, last command          */
extern  int     curgoal;                /* Goal for C-P, C-N            */
extern  char	opertree[NLINE+1];	/* operate within this tree     */
extern  char	browse_dir[NLINE+1];	/* directory of last browse (cwd) */
extern  WINDOW  *curwp;                 /* Current window               */
extern  BUFFER  *curbp;                 /* Current buffer               */
extern  WINDOW  *wheadp;                /* Head of list of windows      */
extern  BUFFER  *bheadp;                /* Head of list of buffers      */
extern  BUFFER  *blistp;                /* Buffer for C-X C-B           */

extern  BUFFER  *bfind(char *, int, int); /* Lookup a buffer by name */
extern  LINE    *lalloc(int used);   	/* Allocate a line              */
extern  int	 km_popped;		/* menu popped up               */
/*
 * This is a weird one. It has to be defined differently for pico and for
 * pine. It seems to need to be defined at startup as opposed to set later.
 * It doesn't work to set it later in pico. When pico is used with a
 * screen reader it seems to jump to the cursor every time through the
 * mswin_charavail() loop in GetKey, and the timeout is this long. So we
 * just need to set it higher than we do in pine. If we understood this
 * we would probably see that we don't need any timer at all in pico, but
 * we don't remember why it is here so we'd better leave it.
 *
 * This is defined in .../pico/main.c and in .../alpine/alpine.c.
 */
extern  int	 my_timer_period;	/* here so can be set           */
#ifdef	MOUSE
extern	MENUITEM menuitems[];		/* key labels and functions */
extern	MENUITEM *mfunc;		/* single generic function  */
extern	mousehandler_t mtrack;		/* func used to track the mouse */
#endif	/* MOUSE */

#if	defined(HAS_TERMCAP) || defined(HAS_TERMINFO) || defined(VMS)
extern	KBESC_T *kbesc;			/* keyboard esc sequence trie   */
#endif /* HAS_TERMCAP/HAS_TERMINFO/VMS */
extern void *input_cs;			/* passed to mbtow() via kbseq() */

#endif /* maindef */

/* terminal table defined only in TERM.C */

#ifndef	termdef
#if	defined(VMS) && !defined(__ALPHA)
globalref
#else
extern
#endif /* VMS */
       TERM    term;                   /* Terminal information.        */
#endif /* termdef */

#endif	/* EDEF_H */
