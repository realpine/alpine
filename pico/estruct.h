/*
 * $Id: estruct.h 767 2007-10-24 00:03:59Z hubert@u.washington.edu $
 *
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
 * Program:	Struct and preprocessor definitions
 *
 */

/*	ESTRUCT:	Structure and preprocesser defined for
			MicroEMACS 3.6

			written by Dave G. Conroy
			modified by Steve Wilhite, George Jones
			greatly modified by Daniel Lawrence
*/

#ifndef	ESTRUCT_H
#define	ESTRUCT_H

/*	Configuration options	*/

#define CVMVAS  1	/* arguments to page forward/back in pages	*/
#define	NFWORD	1	/* forward word jumps to begining of word	*/
#define	TYPEAH	0	/* type ahead causes update to be skipped	*/
#define	REVSTA	1	/* Status line appears in reverse video		*/


/*	internal constants	*/

#define	NBINDS	50			/* max # of bound keys		*/
#ifdef	MAXPATH
#define NFILEN  MAXPATH                 /* # of bytes, file name        */
#else
#define NFILEN  200                     /* # of bytes, file name        */
#endif
#define NBUFN   16                      /* # of bytes, buffer name      */
#define NLINE   500                     /* # of bytes, line             */
#define	NSTRING	256			/* # of bytes, string buffers	*/
#define NPAT    80                      /* # of bytes, pattern          */
#define FARAWAY 1000                    /* Huge number                  */
#define	NLOCKS	100			/* max # of file locks active	*/

#define AGRAVE  0x60                    /* M- prefix,   Grave (LK201)   */
#define METACH  0x1B                    /* M- prefix,   Control-[, ESC  */
#define CTMECH  0x1C                    /* C-M- prefix, Control-\       */
#define EXITCH  0x1D                    /* Exit level,  Control-]       */
#define CTRLCH  0x1E                    /* C- prefix,   Control-^       */
#define HELPCH  0x1F                    /* Help key,    Control-_       */

#define	QNORML	0x0000			/* Flag meaning no flag ;)	*/
#define	QFFILE	0x0001			/* Flag buffer for file neme	*/
#define	QDEFLT	0x0002			/* Flag to use default answer	*/
#define	QBOBUF	0x0004			/* Start with cursor at BOL	*/
#define QNODQT  0x0008		        /* Don't accept double quotes   */

#define FIOSUC  0                       /* File I/O, success.           */
#define FIOFNF  1                       /* File I/O, file not found.    */
#define FIOEOF  2                       /* File I/O, end of file.       */
#define FIOERR  3                       /* File I/O, error.             */
#define	FIOLNG	4			/*line longer than allowed len	*/
#define	FIODIR	5			/* File is a directory		*/
#define	FIONWT	6			/* File lacks write permission	*/
#define	FIONRD	7			/* File lacks read permission	*/
#define	FIONEX	8			/* File lacks exec permission	*/
#define	FIOSYM	9			/* File is a symbolic link	*/
#define	FIOPER	10			/* Generic permission denied    */


#define CFCPCN  0x0001                  /* Last command was C-P, C-N    */
#define CFKILL  0x0002                  /* Last command was a kill      */
#define CFFILL  0x0004                  /* Last command was a fill      */
#define	CFSRCH	0x0008			/* Last command was WhereIs	*/
#define CFFLBF  0x0010                  /* Last cmd was full buf fill   */
#define CFFLPA  0x0020                  /* Last cmd was paragraph fill  */

/*
 * macros to help filter character input
 */
#define CELLMASK 0xffffff
#define	LOBIT_CHAR(C)	((C) > 0x1f && (C) < 0x7f)
#define	HIBIT_CHAR(C)	((C) > 0x7f && (C) <= CELLMASK)
#define	HIBIT_OK(C)	(!(gmode & MDHBTIGN))
#define	VALID_KEY(C)	(LOBIT_CHAR(C) || (HIBIT_OK(C) && HIBIT_CHAR(C)))
#define ctrl(c)         ((c) & 0x1f)    /* control character mapping */


/*
 * Placeholders for keymenu tags used in some ports  (well, only in the
 * windows port for now) to turn on commands in the Menu Bar.
 */
#ifndef	KS_OSDATAVAR
#define	KS_OSDATAVAR
#define	KS_OSDATAGET(X)
#define	KS_OSDATASET(X, Y)

#define KS_NONE
#define KS_VIEW
#define KS_EXPUNGE
#define KS_ZOOM
#define KS_SORT
#define KS_HDRMODE
#define KS_MAINMENU
#define KS_FLDRLIST
#define KS_FLDRINDEX
#define KS_COMPOSER
#define KS_PREVPAGE
#define KS_PREVMSG
#define KS_NEXTMSG
#define KS_ADDRBOOK
#define KS_WHEREIS
#define KS_PRINT
#define KS_REPLY
#define KS_FORWARD
#define KS_BOUNCE
#define KS_DELETE
#define KS_UNDELETE
#define KS_FLAG
#define KS_SAVE
#define KS_EXPORT
#define KS_TAKEADDR
#define KS_SELECT
#define KS_APPLY
#define KS_POSTPONE
#define KS_SEND
#define KS_CANCEL
#define KS_ATTACH
#define KS_TOADDRBOOK
#define KS_READFILE
#define KS_JUSTIFY
#define KS_ALTEDITOR
#define KS_GENERALHELP
#define KS_SCREENHELP
#define KS_EXIT
#define KS_NEXTPAGE
#define KS_SAVEFILE
#define KS_CURPOSITION
#define KS_GOTOFLDR
#define KS_JUMPTOMSG
#define KS_RICHHDR
#define KS_EXITMODE
#define KS_REVIEW
#define KS_KEYMENU
#define KS_SELECTCUR
#define KS_UNDO
#define KS_SPELLCHK
#endif /* !KS_OSDATAVAR */


/*
 * There is a window structure allocated for every active display window. The
 * windows are kept in a big list, in top to bottom screen order, with the
 * listhead at "wheadp". Each window contains its own values of dot and mark.
 * The flag field contains some bits that are set by commands to guide
 * redisplay; although this is a bit of a compromise in terms of decoupling,
 * the full blown redisplay is just too expensive to run for every input
 * character.
 */
typedef struct  WINDOW {
        struct  WINDOW *w_wndp;         /* Next window                  */
        struct  BUFFER *w_bufp;         /* Buffer displayed in window   */
        struct  LINE *w_linep;          /* Top line in the window       */
        struct  LINE *w_dotp;           /* Line containing "."          */
        int     w_doto;                 /* Byte offset for "."          */
        struct  LINE *w_markp;          /* Line containing "mark"       */
        int	w_marko;                /* Byte offset for "mark"       */
        struct  LINE *w_imarkp;         /* INTERNAL Line with "mark"    */
        int	w_imarko;               /* INTERNAL "mark" byte offset  */
        signed char w_toprow;           /* Origin 0 top row of window   */
        signed char w_ntrows;           /* # of rows of text in window  */
        char    w_force;                /* If NZ, forcing row.          */
        char    w_flag;                 /* Flags.                       */
}       WINDOW;

#define WFFORCE 0x01                    /* Window needs forced reframe  */
#define WFMOVE  0x02                    /* Movement from line to line   */
#define WFEDIT  0x04                    /* Editing within a line        */
#define WFHARD  0x08                    /* Better to a full display     */
#define WFMODE  0x10                    /* Update mode line.            */

/*
 * Text is kept in buffers. A buffer header, described below, exists for every
 * buffer in the system. The buffers are kept in a big list, so that commands
 * that search for a buffer by name can find the buffer header. There is a
 * safe store for the dot and mark in the header, but this is only valid if
 * the buffer is not being displayed (that is, if "b_nwnd" is 0). The text for
 * the buffer is kept in a circularly linked list of lines, with a pointer to
 * the header line in "b_linep".
 * 	Buffers may be "Inactive" which means the files accosiated with them
 * have not been read in yet. These get read in at "use buffer" time.
 */
typedef struct  BUFFER {
	struct  BUFFER *b_bufp;         /* Link to next BUFFER          */
	struct  LINE *b_dotp;           /* Link to "." LINE structure   */
	int	b_doto;                 /* Offset of "." in above LINE  */
	struct  LINE *b_markp;          /* The same as the above two,   */
        int	b_marko;                /* but for the "mark"           */
	struct  LINE *b_linep;          /* Link to the header LINE      */
	long	b_linecnt;		/* Lines in buffer (mswin only)	*/
	long	b_mode;			/* editor mode of this buffer	*/
	char	b_active;		/* window activated flag	*/
	char    b_nwnd;                 /* Count of windows on buffer   */
	char    b_flag;                 /* Flags                        */
	char    b_fname[NFILEN];        /* File name                    */
	char    b_bname[NBUFN];         /* Buffer name                  */
}	BUFFER;

#define BFTEMP     0x01                    /* Internal temporary buffer     */
#define BFCHG      0x02                    /* Changed since last write      */
#define BFWRAPOPEN 0x04                    /* Wordwrap should open new line */


/*
 * The starting position of a region, and the size of the region in
 * characters, is kept in a region structure.  Used by the region commands.
 */
typedef struct  {
        struct  LINE *r_linep;          /* Origin LINE address.         */
        int	r_offset;               /* Origin LINE offset.          */
        long    r_size;                 /* Length in characters.        */
}       REGION;


/*
 * character and attribute pair.  The basic building block
 * of the editor.  The bitfields may have to be changed to a char
 * and short if there are problems...
 */
typedef	struct CELL {
	unsigned int c : 24;		/* Character value in cell      */
	unsigned int a : 8;		/* Its attributes               */
} CELL;

/* flags for color_options */
#define COLOR_ANSI8_OPT		0x01
#define COLOR_ANSI16_OPT	0x02
#define COLOR_ANSI256_OPT	0x04
#define COLOR_TRANS_OPT		0x08

/*
 * All text is kept in circularly linked lists of "LINE" structures. These
 * begin at the header line (which is the blank line beyond the end of the
 * buffer). This line is pointed to by the "BUFFER". Each line contains the
 * number of bytes in the line (the "used" size), the size of the text array,
 * and the text. The end of line is not stored as a byte; it's implied. Future
 * additions will include update hints, and a list of marks into the line.
 */
typedef struct  LINE {
        struct  LINE *l_fp;             /* Link to the next line        */
        struct  LINE *l_bp;             /* Link to the previous line    */
        int     l_size;                 /* Allocated size               */
        int     l_used;                 /* Used size                    */
        CELL    l_text[1];              /* A bunch of characters.       */
}       LINE;

#define lforw(lp)       ((lp)->l_fp)
#define lback(lp)       ((lp)->l_bp)
#define lgetc(lp, n)    ((lp)->l_text[(n)])
#define lputc(lp, n, c) ((lp)->l_text[(n)]=(c))
#define llength(lp)     ((lp)->l_used)

/*
 * The editor communicates with the display using a high level interface. A
 * "TERM" structure holds useful variables, and indirect pointers to routines
 * that do useful operations. The low level get and put routines are here too.
 * This lets a terminal, in addition to having non standard commands, have
 * funny get and put character code too. The calls might get changed to
 * "termp->t_field" style in the future, to make it possible to run more than
 * one terminal type.
 */
typedef struct  {
        short   t_nrow;                 /* Number of rows - 1.          */
        short   t_ncol;                 /* Number of columns.           */
	short	t_margin;		/* min margin for extended lines*/
        short   t_mrow;                 /* Number of rows in menu       */
        int     (*t_open)(void);        /* Open terminal at the start.  */
        int	(*t_terminalinfo)(int); /* Set up terminal info         */
        int     (*t_close)(void);       /* Close terminal at end.       */
        int     (*t_getchar)(int return_on_intr,
			     int (*recorder)(int),
			     void (*bail_handler)(void));
					/* Get character from keyboard. */
        int     (*t_putchar)(UCS);      /* Put character to display.    */
        int     (*t_flush)(void);       /* Flush output buffers.        */
        int     (*t_move)(int, int);    /* Move the cursor, origin 0.   */
        int     (*t_eeol)(void);        /* Erase to end of line.        */
        int     (*t_eeop)(void);        /* Erase to end of page.        */
        int     (*t_beep)(void);        /* Beep.                        */
	int	(*t_rev)(int);		/* set reverse video state	*/
}       TERM;

/*	structure for the table of initial key bindings		*/

typedef struct  {
        UCS	k_code;                 /* Key code                     */
        int     (*k_fp)();              /* Routine to handle it         */
}       KEYTAB;

/*      sturcture used for key menu painting         */

typedef struct {
	char	*name;			/* key to display (UTF-8)		*/
	char	*label;			/* function name key invokes (UTF-8)	*/
	KS_OSDATAVAR			/* port-specific data */
}	KEYMENU;

typedef struct {
	char	*name;			/* key to display  		*/
	char	*label;			/* function name key invokes	*/
	UCS	key;			/* what to watch for and return	*/
	KS_OSDATAVAR			/* port-specific data */
}	EXTRAKEYS;

typedef struct lmlist {
	char *dir;			/* these 3 are all UTF-8 strings */
	char *fname;
	char  size[32];
	struct lmlist *next;
}	LMLIST;


typedef struct	VIDEO {
	short	v_flag;			/* Flags */
	CELL	v_text[1];		/* Screen data. */
}	VIDEO;


typedef union eml {
	char *s;
	int   d;
	long  l;
	UCS   c;
}	EML;

typedef	struct _file_io_data {
	FILE *fp;			/* stdio stream into file */
        long  flags;			/* state flags */
	char *name;			/* pointer to file name */
}	FIOINFO;

#define	FIOINFO_READ	0x01
#define	FIOINFO_WRITE	0x02

#define	ISCONTROL(C)	((C) < 0x20 || (C) == 0x7F \
			 || ((gmode & P_HICTRL) && ((C) > 0x7F && (C) < 0xA0)))

#ifdef	MOUSE

/* Test if mouse position (R, C) is in menu item (X) */
#define	M_ACTIVE(R, C, X)	(((unsigned)(R)) >= (X)->tl.r       \
				    && ((unsigned)(R)) <= (X)->br.r \
				    && ((unsigned)(C)) >= (X)->tl.c \
				    && ((unsigned)(C)) < (X)->br.c)
#endif	/* MOUSE */

#endif	/* ESTRUCT_H */
