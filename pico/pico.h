/*
 * $Id: pico.h 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $
 *
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
 * Program:	pico.h - definitions for Pine's composer library
 */

#ifndef	PICO_H
#define	PICO_H

#include "mode.h"

#include "../pith/osdep/color.h"
#include "../pith/osdep/err_desc.h"
#include "../pith/charconv/utf8.h"

#define	errstr(n)	error_description(n)

/*
 * Defined for attachment support
 */
#define	ATTACHMENTS	1


/*
 * defs of return codes from pine mailer composer.
 */
#define	BUF_CHANGED	0x01
#define	COMP_CANCEL	0x02
#define	COMP_EXIT	0x04
#define	COMP_FAILED	0x08
#define	COMP_SUSPEND	0x10
#define	COMP_GOTHUP	0x20


/*
 * top line from the top of the screen for the editor to do 
 * its stuff
 */
#define	COMPOSER_TOP_LINE	2
#define	COMPOSER_TITLE_LINE	0



#define HLSZ NLINE

/*
 * definitions of Mail header structures 
 */
struct hdr_line {
        UCS     text[HLSZ];
        struct  hdr_line        *next;
        struct  hdr_line        *prev;
};
 

/* must be the same as HelpType in pith/helptext.h */
#define	HELP_T	char **

 
/* 
 *  This structure controls the header line items on the screen.  An
 * instance of this should be created and passed in as an argument when
 * pico is called. The list is terminated by an entry with the name
 * element NULL.
 */
 
struct headerentry {
        char     *prompt;
	char	 *name;
	HELP_T	  help;
        int	  prwid;              /* prompt width on screen             */
        int	  maxlen;
        char    **realaddr;
        int     (*builder)();        /* Function to verify/canonicalize val */
	struct headerentry        *affected_entry, *next_affected;
				     /* entry builder's 4th arg affects     */
        char   *(*selector)();       /* Browser for possible values         */
        char     *key_label;         /* Key label for key to call browser   */
        char   *(*fileedit)();       /* Editor for file named in header     */
        int     (*nickcmpl)();       /* routine that helps with nickname completion */
        unsigned  display_it:1;	     /* field is to be displayed by default */
        unsigned  break_on_comma:1;  /* Field breaks on commas              */
        unsigned  is_attach:1;       /* Special case field for attachments  */
        unsigned  rich_header:1;     /* Field is part of rich header        */
        unsigned  only_file_chars:1; /* Field is a file name                */
        unsigned  single_space:1;    /* Crush multiple spaces into one      */
        unsigned  sticky:1;          /* Can't change this via affected_entry*/
        unsigned  dirty:1;           /* We've changed this entry            */
	unsigned  start_here:1;	     /* begin composer on first on lit      */
	unsigned  blank:1;	     /* blank line separator                */
	unsigned  sticky_special:1;  /* special treatment                   */
#ifdef	KS_OSDATAVAR
	KS_OSDATAVAR		     /* Port-Specific keymenu data */
#endif
	void     *bldr_private;      /* Data managed by builders            */
        struct    hdr_line        *hd_text;
};


/*
 * Structure to pass as arg to builders.
 *
 *    me -- A pointer to the bldr_private data for the entry currently
 *            being edited.
 *  tptr -- A malloc'd copy of the displayed text for the affected_entry
 *            pointed to by the  aff argument.
 *   aff -- A pointer to the bldr_private data for the affected_entry (the
 *            entry that this entry affects).
 *  next -- The next affected_entry in the list. For example, the Lcc entry
 *            affects the To entry which affects the Fcc entry.
 */
typedef struct bld_arg {
    void          **me;
    char           *tptr;
    void          **aff;
    struct bld_arg *next;
} BUILDER_ARG;


/*
 * structure to keep track of header display
 */
struct on_display {
    int			 p_ind;			/* index into line */
    int			 p_len;			/* length of line   */
    int			 p_line;		/* physical line on screen */
    int			 top_e;			/* topline's header entry */
    struct hdr_line	*top_l;			/* top line on display */
    int			 cur_e;			/* current header entry */
    struct hdr_line	*cur_l;			/* current hd_line */
};						/* global on_display struct */


/*
 * Structure to handle attachments
 */
typedef struct pico_atmt {
    char *description;			/* attachment description */
    char *filename;			/* file/pseudonym for attachment */
    char *size;				/* size of attachment */
    char *id;				/* attachment id */
    unsigned short flags;
    struct pico_atmt *next;
} PATMT;

/*
 * Structure to contain color options
 */
typedef struct pico_colors {
    COLOR_PAIR *tbcp;                   /* title bar color pair */
    COLOR_PAIR *klcp;                   /* key label color pair */
    COLOR_PAIR *kncp;                   /* key name color pair  */
    COLOR_PAIR *stcp;                   /* status color pair    */
    COLOR_PAIR *prcp;                   /* prompt color pair    */
} PCOLORS;

/*
 * Flags for attachment handling
 */
#define	A_FLIT	0x0001			/* Accept literal file and size      */
#define	A_ERR	0x0002			/* Problem with specified attachment */
#define	A_TMP	0x0004			/* filename is temporary, delete it  */


/*
 * Master pine composer structure.  Right now there's not much checking
 * that any of these are pointing to something, so pine must have them pointing
 * somewhere.
 */
typedef struct pico_struct {
    void  *msgtext;			/* ptrs to malloc'd arrays of char */
    char  *pine_anchor;			/* ptr to pine anchor line */
    char  *pine_version;		/* string containing Pine's version */
    char  *oper_dir;			/* Operating dir (confine to tree) */
    char  *home_dir;                    /* Home directory that should be used (WINDOWS) */
    char  *quote_str;			/* prepended to lines of quoted text */
    char  *exit_label;			/* Label for ^X in keymenu */
    char  *ctrlr_label;			/* Label for ^R in keymenu */
    char  *alt_spell;			/* Checker to use other than "spell" */
    char **alt_ed;			/* name of alternate editor or NULL */
    UCS   *wordseps;			/* word separator characters other than space */
    int    fillcolumn;			/* where to wrap */
    int    menu_rows;			/* number of rows in menu (0 or 2) */
    int    space_stuffed;		/* space-stuffed for flowed, in case needs undoing */
    long   edit_offset;			/* offset into hdr line or body */
    PATMT *attachments;			/* linked list of attachments */
    PCOLORS *colors;                    /* colors for titlebar and keymenu */
    void  *input_cs;			/* passed to mbtow() via kbseq() */
    long   pine_flags;			/* entry mode flags */
    /* The next few bits are features that don't fit in pine_flags      */
    /* If we had this to do over, it would probably be one giant bitmap */
    unsigned always_spell_check:1;      /* always spell-checking upon quit */
    unsigned strip_ws_before_send:1;    /* don't default strip bc of flowed */
    unsigned allow_flowed_text:1;    /* clean text when done to keep flowed */
    int   (*helper)();			/* Pine's help function  */
    int   (*showmsg)();			/* Pine's display_message */
    UCS   (*suspend)();			/* Pine's suspend */
    void  (*keybinput)();		/* Pine's keyboard input indicator */
    int   (*tty_fix)();			/* Let Pine fix tty state */
    long  (*newmail)();			/* Pine's report_new_mail */
    long  (*msgntext)();		/* callback to get msg n's text */
    int   (*upload)();			/* callback to rcv uplaoded text */
    char *(*ckptdir)();			/* callback for checkpoint file dir */
    int   (*exittest)();		/* callback to verify exit request */
    char *(*canceltest)();		/* callback to verify cancel request */
    int   (*mimetype)();		/* callback to display mime type */
    int   (*expander)();		/* callback to expand address lists */
    int   (*user_says_noflow)();	/* callback to tell us we're not flowing */
    void  (*resize)();			/* callback handling screen resize */
    void  (*winch_cleanup)();		/* callback handling screen resize */
    int    arm_winch_cleanup;		/* do the winch_cleanup if resized */
    HELP_T search_help;
    HELP_T ins_help;
    HELP_T ins_m_help;
    HELP_T composer_help;
    HELP_T browse_help;
    HELP_T attach_help;
    struct headerentry *headents;
} PICO;


/*
 * Used to save and restore global pico variables that are destroyed by
 * calling pico a second time. This happens when pico calls a selector
 * in the HeaderEditor and that selector calls pico again.
 */
typedef struct save_stuff {
    int                 vtrow,
	                vtcol,
	                lbound;
    VIDEO             **vscreen,
	              **pscreen;	/* save pointers */
    struct on_display   ods;		/* save whole struct */
    short               delim_ps,
	                invert_ps;
    int                 pico_all_done;
    jmp_buf             finstate;
    UCS                *pico_anchor;	/* save pointer */
    PICO               *Pmaster;	/* save pointer */
    int                 fillcol;
    UCS                *pat;		/* save array */
    int                 ComposerTopLine,
                        ComposerEditing;
    long                gmode;
    char               *alt_speller;	/* save pointer */
    UCS                *quote_str;      /* save pointer */
    UCS                *wordseps;       /* save pointer */
    int                 currow,
	                curcol,
	                thisflag,
	                lastflag,
	                curgoal;
    char               *opertree;	/* save array */
    WINDOW             *curwp;		/* save pointer */
    WINDOW             *wheadp;		/* save pointer */
    BUFFER             *curbp;		/* save pointer */
    BUFFER             *bheadp;		/* save pointer */
    int                 km_popped;
    int                 mrow;
} VARS_TO_SAVE;


#ifdef	MOUSE
/*
 * Mouse buttons.
 */
#define M_BUTTON_LEFT		0
#define M_BUTTON_MIDDLE		1
#define M_BUTTON_RIGHT		2


/*
 * Flags.  (modifier keys)
 */
#define M_KEY_CONTROL		0x01	/* Control key was down. */
#define M_KEY_SHIFT		0x02	/* Shift key was down. */


/*
 * Mouse Events
 */
#define M_EVENT_DOWN		0x01	/* Mouse went down. */
#define M_EVENT_UP		0x02	/* Mouse went up. */
#define M_EVENT_TRACK		0x04	/* Mouse tracking */

/*
 * Mouse event information.
 */
typedef struct mouse_struct {
	unsigned long mevent;	/* Indicates type of event:  Down, Up or Track */
	char	down;		/* TRUE when mouse down event */
	char	doubleclick;	/* TRUE when double click. */
	int	button;		/* button pressed. */
	int	flags;		/* What other keys pressed. */
	int	row;
	int	col;
} MOUSEPRESS;



typedef	unsigned long (*mousehandler_t)(unsigned long, int, int, int, int);

typedef struct point {
    unsigned	r:8;		/* row value				*/
    unsigned	c:8;		/* column value				*/
} MPOINT;


typedef struct menuitem {
    unsigned	     val;	/* return value				*/
    mousehandler_t    action;	/* action to perform			*/
    MPOINT	     tl;	/* top-left corner of active area	*/
    MPOINT	     br;	/* bottom-right corner of active area	*/
    MPOINT	     lbl;	/* where the label starts		*/
    char	    *label;
    void            (*label_hiliter)();
    COLOR_PAIR      *kncp;      /* key name color pair                  */
    COLOR_PAIR      *klcp;      /* key label color pair                 */ 
    struct menuitem *next;
} MENUITEM;
#endif


/*
 * Structure used to manage keyboard input that comes as escape
 * sequences (arrow keys, function keys, etc.)
 */
typedef struct  KBSTREE {
	char	value;
        int     func;              /* Routine to handle it         */
	struct	KBSTREE *down; 
	struct	KBSTREE	*left;
} KBESC_T;


/*
 * various flags that they may passed to PICO
 */
#define P_HICTRL	0x80000000	/* overwrite mode		*/
#define	P_CHKPTNOW	0x40000000	/* do the checkpoint on entry      */
#define	P_DELRUBS	0x20000000	/* map ^H to forwdel		   */
#define	P_LOCALLF	0x10000000	/* use local vs. NVT EOL	   */
#define	P_BODY		0x08000000	/* start composer in body	   */
#define	P_HEADEND	0x04000000	/* start composer at end of header */
#define	P_VIEW		MDVIEW		/* read-only			   */
#define	P_FKEYS		MDFKEY		/* run in function key mode 	   */
#define	P_SECURE	MDSCUR		/* run in restricted (demo) mode   */
#define	P_TREE		MDTREE		/* restrict to a subtree	   */
#define	P_SUSPEND	MDSSPD		/* allow ^Z suspension		   */
#define	P_ADVANCED	MDADVN		/* enable advanced features	   */
#define	P_CURDIR	MDCURDIR	/* use current dir for lookups	   */
#define	P_ALTNOW	MDALTNOW	/* enter alt ed sans hesitation	   */
#define	P_SUBSHELL	MDSPWN		/* spawn subshell for suspend	   */
#define	P_COMPLETE	MDCMPLT		/* enable file name completion     */
#define	P_DOTKILL	MDDTKILL	/* kill from dot to eol		   */
#define	P_SHOCUR	MDSHOCUR	/* cursor follows hilite in browser*/
#define	P_HIBITIGN	MDHBTIGN	/* ignore chars with hi bit set    */
#define	P_DOTFILES	MDDOTSOK	/* browser displays dot files	   */
#define	P_NOBODY	MDHDRONLY	/* Operate only on given headers   */
#define	P_ALLOW_GOTO	MDGOTO		/* support "Goto" in file browser  */
#define P_REPLACE       MDREPLACE       /* allow "Replace" in "Where is"   */


/*
 * Main defs 
 */
#ifdef	maindef
PICO	*Pmaster = NULL;		/* composer specific stuff */
char	*version = "5.05";		/* PICO version number */

#else
extern	PICO *Pmaster;			/* composer specific stuff */
extern	char *version;			/* pico version! */

#endif	/* maindef */


/*
 * Flags for FileBrowser call
 */
#define FB_READ		0x0001		/* Looking for a file to read.  */
#define FB_SAVE		0x0002		/* Looking for a file to save.  */
#define	FB_ATTACH	0x0004		/* Looking for a file to attach */
#define	FB_LMODEPOS	0x0008		/* ListMode is a possibility    */
#define	FB_LMODE	0x0010		/* Using ListMode now           */


/*
 * Flags for pico_readc/pico_writec.
 */
#define PICOREADC_NONE	0x00
#define PICOREADC_NOUCS	0x01


/*
 * number of keystrokes to delay removing an error message, or new mail
 * notification, or checkpointing
 */
#define	MESSDELAY	25
#define	NMMESSDELAY	60
#ifndef CHKPTDELAY
#define	CHKPTDELAY	100
#endif

#include "./keydefs.h"


/*
 * useful function definitions
 */
int   pico(PICO *pm);
int   pico_file_browse(PICO *, char *, size_t, char *, size_t, char *, size_t, int);
void *pico_get(void);
void  pico_give(void *w);
int   pico_readc(void *w, unsigned char *c, int flags);
int   pico_writec(void *w, int c, int flags);
int   pico_puts(void *w, char *s, int flags);
int   pico_seek(void *w, long offset, int orig);
int   pico_replace(void *, char *);
int   pico_fncomplete(char *, char *, size_t);
#if defined(DOS) || defined(OS2)
int   pico_nfsetcolor(char *);
int   pico_nbsetcolor(char *);
int   pico_rfsetcolor(char *);
int   pico_rbsetcolor(char *);
#endif
#ifdef	MOUSE
int   register_mfunc(mousehandler_t, int, int, int, int);
void  clear_mfunc(mousehandler_t);
unsigned long mouse_in_content(unsigned long, int, int, int, int);
unsigned long mouse_in_pico(unsigned long, int, int, int, int);
void  mouse_get_last(mousehandler_t *, MOUSEPRESS *);
void  register_key(int, unsigned, char *, void (*)(),
		   int, int, int, COLOR_PAIR *, COLOR_PAIR *);
int   mouse_on_key(int, int);
#endif	/* MOUSE */


#endif	/* PICO_H */
