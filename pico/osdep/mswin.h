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


#ifndef MSWIN_H
#define MSWIN_H


#define	__far
#define	_far
#define	__export

/*
 * Equivalent to a windows handle.
 */
typedef void	       *WINHAND;

/*
 * Mouse input events. 
 */
typedef struct {
	int		event;
	int		button;
	int		nRow;
	int		nColumn;
	int		keys;
	int		flags;
} MEvent;


/*
 * These define how mouse events get queued.
 */
#define MSWIN_MF_REPLACING	0x1000
#define MSWIN_MF_REPEATING	0x2000


typedef struct {
	int		ch;
	int		rval;
	LPTSTR		name;
	LPTSTR		label;
	int		id;
} MDlgButton;



/*
 * Struct to tell pico how to build and what to return when
 * it's asked to build a popup menu.
 */
typedef struct _popup {
	enum {tIndex, tQueue, tMessage, tSubMenu,
	      tSeparator, tTail} type;
	struct {			/* menu's label */
	    char	  *string;
	    enum	   {lNormal = 0, lChecked, lDisabled} style;
	} label;
	union {
	    UCS		   val;		/* Queue: inserted into input queue */
	    UCS 	   msg;		/* Message: gets posted to ghTTYWnd */
	    struct _popup *submenu;	/* Submenu: array of submenu entries */
	} data;
	struct {			/* Internal data */
	    int	       id;
	} internal;
} MPopup;



/*
 * Type of function expected by mswin_allowcopy and mswin_allowcopycut
 * and used in EditDoCopyData
 */
typedef	int (*getc_t)(int pos);

/*
 * Callback used to fetch text for display in alternate window.  Text
 * can be returned as either a pointer to a null terminated block of
 * text (a string), with CRLF deliminating lines.  OR as a pointer to
 * an array of pointers to lines, each line being a null terminated
 * string. 
 */
typedef int (*gettext_t)(char *title, void **text, long *len, int *format);

/*
 * Type used by line up/down event handlers to move the body of the
 * displayed text, by sort callback to get/set sort order, by
 * header mode to get/set header state...
 */
typedef int (*cbarg_t)(int action, long args);


/*
 * Callback used for periodic callback.
 */
typedef void  (*cbvoid_t)(void);
typedef char *(*cbstr_t)(char *);


#define GETTEXT_TITLELEN	128
#define GETTEXT_TEXT		1	/* 'text' is pointer to text. */
#define GETTEXT_LINES		2	/* 'text' is pointer to array of
					 *   pointers to lines. */


#ifndef PATH_MAX
#define PATH_MAX		128	/* Max size of a directory path. */
#endif

/*
 * Scroll callback values. Used to be mapped in the MSWIN_RANGE_START..END
 *  range, but now are mapped directly to keydefs.h values. Except two of
 *  them which don't have map values in keydefs.h so they're still in the
 *  MSWIN_RANGE_START..END 0x7000 range.
 */
#define MSWIN_KEY_SCROLLUPPAGE		0x7000
#define MSWIN_KEY_SCROLLDOWNPAGE	0x7001
#define MSWIN_KEY_SCROLLUPLINE		KEY_SCRLUPL
#define MSWIN_KEY_SCROLLDOWNLINE	KEY_SCRLDNL
#define MSWIN_KEY_SCROLLTO		    KEY_SCRLTO

#define	MSWIN_PASTE_DISABLE	0
#define	MSWIN_PASTE_FULL	1
#define	MSWIN_PASTE_LINE	2


#define MSWIN_CURSOR_ARROW	0
#define MSWIN_CURSOR_BUSY	1
#define MSWIN_CURSOR_IBEAM	2
#define MSWIN_CURSOR_HAND	3

/*
 * Flags for mswin_displaytext
 */
#define MSWIN_DT_NODELETE	0x0001	/* Don't delete text when 
					 * window closes. */
#define MSWIN_DT_USEALTWINDOW   0x0002	/* Put text in alt window if already
					 * open. Open if not. */
#define MSWIN_DT_FILLFROMFILE   0x0004	/* pText_utf8 is a filename. */

/*
 * functions from mswin.c
 */


WINHAND		mswin_gethinstance();
WINHAND		mswin_gethwnd ();
void		mswin_killsplash();
void		mswin_settitle(char *);
int		mswin_getmouseevent (MEvent * pMouse);
void		mswin_mousetrackcallback (cbarg_t cbfunc);
int		mswin_popup(MPopup *members);
void		mswin_keymenu_popup ();
void		mswin_registericon(int row, int id, char *file);
void		mswin_destroyicons();
void		mswin_finishicons();
void		mswin_show_icon (int x, int y, char *file);
void		mswin_setdebug (int debug, FILE *debugfile);
void		mswin_setnewmailwidth (int width);
int		mswin_setdndcallback (int (*cb)());
int		mswin_cleardndcallback (void);
int		mswin_setresizecallback (int (*cb)());
int		mswin_clearresizecallback (int (*cb)());
void		mswin_setdebugoncallback (cbvoid_t cbfunc);
void		mswin_setdebugoffcallback (cbvoid_t cbfunc);
int		mswin_setconfigcallback (cbvoid_t cffunc);
int		mswin_sethelpcallback (cbstr_t cbfunc);
int		mswin_setgenhelpcallback (cbstr_t cbfunc);
void		mswin_setclosetext (char *pCloseText);
int		mswin_setwindow (char *fontName, char *fontSize, 
				 char *fontStyle, char *windowPosition,
				 char *cursorStyle, char *fontCharSet);
int             mswin_showwindow();
int		mswin_getwindow (char *fontName, size_t nfontName,
				 char *fontSize, size_t nfontSize,
				 char *fontStyle, size_t nfontStyle,
				 char *windowPosition, size_t nwindowPosition,
				 char *foreColor, size_t nforeColor,
				 char *backColor, size_t nbackColor,
				 char *cursorStyle, size_t ncursorStyle,
				 char *fontCharSet, size_t nfontCharSet);
void            mswin_noscrollupdate (int flag);
void		mswin_setscrollrange (long page, long max);
void		mswin_setscrollpos (long pos);
long		mswin_getscrollpos (void);
long		mswin_getscrollto (void);
void		mswin_setscrollcallback (cbarg_t cbfunc);
void		mswin_setsortcallback (cbarg_t cbfunc);
void		mswin_setflagcallback (cbarg_t cbfunc);
void		mswin_sethdrmodecallback (cbarg_t cbfunc);
void		mswin_setselectedcallback (cbarg_t cbfunc);
void		mswin_setzoomodecallback (cbarg_t cbfunc);
void		mswin_setfkeymodecallback (cbarg_t cbfunc);
void		mswin_setprintfont (char *fontName, char *fontSize, 
			char *fontStyle, char *fontCharSet);
void		mswin_getprintfont (char *, size_t, char *, size_t,
				    char *, size_t, char *, size_t);
int		mswin_yield (void);
int		mswin_charavail (void);
UCS		mswin_getc_fast (void);
void		mswin_flush_input (void);
int		mswin_showcursor (int show);
int		mswin_showcaret (int show);
void		mswin_trayicon (int show);
int		mswin_move (int row, int column);
int		mswin_getpos (int *row, int *column);
int		mswin_getscreensize (int *row, int *column);
void		mswin_minimize (void);
int		mswin_putblock (char *utf8_str, int strLen);
int		mswin_puts (char *utf8_str);
int		mswin_puts_n (char *utf8_str, int n);
int		mswin_putc (UCS c);
int		mswin_outc (char c);
int		mswin_rev (int state);
int		mswin_getrevstate (void);
int		mswin_bold (int state);
int		mswin_uline (int state);
int		mswin_eeol (void);
int		mswin_eeop (void);
int		mswin_beep (void);
void		mswin_pause (int);
int		mswin_flush (void);
void		mswin_setcursor (int);
void		mswin_messagebox (char *msg, int);
void		mswin_allowpaste (int);
void		mswin_allowcopycut (getc_t);
void		mswin_allowcopy (getc_t);
void		mswin_addclipboard (char *s);
void		mswin_allowmousetrack (int);
int		mswin_newmailicon (void);
void		mswin_newmailtext (char *t);
void		mswin_newmaildone (void);
void		mswin_mclosedtext (char *t);
void		mswin_menuitemclear (void);
void		mswin_menuitemadd (UCS key, char *label, int menuitem, 
			int flags);
int		mswin_setwindowmenu (int menu);
int		mswin_print_ready (WINHAND hWnd, LPTSTR docDesc);
int		mswin_print_done (void);
char *		mswin_print_error (int errorcode);
int		mswin_print_char (TCHAR c);
int		mswin_print_char_utf8 (int c);
int		mswin_print_text (LPTSTR text);
int		mswin_print_text_utf8 (char *text);
int		mswin_savefile (char *dir, int dirlen, char *fName, int nMaxFName);
int		mswin_openfile (char *dir, int nMaxDName, char *fName, int nMaxFName, char *extlist);
int		mswin_multopenfile (char *dir, int nMaxDName, char *fName, int nMaxFName, char *extlist);
char	       *mswin_rgbchoice(char *pOldRGB);
void		mswin_killbuftoclip (getc_t copyfunc);
void		pico_popup();
void		mswin_paste_popup();
int		mswin_fflush (FILE *f);
void		mswin_setperiodiccallback (cbvoid_t periodiccb, long period);
WINHAND		mswin_inst2task (WINHAND hInst);
int		mswin_ontask_del (WINHAND hTask, char *path);
int		mswin_exec_and_wait (char *whatsit, char *command,
				     char *infile, char *outfile,
				     int *exit_val,
				     unsigned mseaw_flags);
int		mswin_shell_exec (char *command, WINHAND *childproc);
void		mswin_exec_err_msg (char *what, int status, char *buf, 
			size_t buflen);
int		mswin_onexit_del (char *path);
int		mswin_set_quit_confirm (int);
void		mswin_showhelpmsg (WINHAND hWnd, char **helplines);

typedef struct MSWIN_TEXTWINDOW MSWIN_TEXTWINDOW;
MSWIN_TEXTWINDOW *mswin_displaytext (char *title, char *pText, size_t textLen,
			             char **pLines, MSWIN_TEXTWINDOW *mswin_tw,
			             int flags);
int		mswin_imaptelemetry(char *msg);
void		mswin_enableimaptelemetry(int state);
int             mswin_newmailwin(int is_us, char *from,
				 char *subject, char *folder);
int             mswin_newmailwinon(void);

char	       *mswin_reg_default_browser(char *url);
int		mswin_reg(int op, int tree, char *data, size_t size);
int             mswin_is_def_client(int type);
int             mswin_set_def_client(int type);
char	      **mswin_reg_dump(void);
int		mswin_majorver();
int		mswin_minorver();
char	       *mswin_compilation_date();
char	       *mswin_compilation_remarks();
char	       *mswin_specific_winver();


int		mswin_usedialog (void);
int		mswin_dialog(UCS *prompt, UCS *string, int field_len, 
			int append_current, int passwd, 
			MDlgButton *button_list, char **help, unsigned flags);
int		mswin_select(char *utf8prompt, MDlgButton *button_list, 
			int dflt, int on_ctrl_C, char **help, unsigned flags);
int		mswin_yesno(UCS *);
int		mswin_yesno_utf8(char *);

BOOL		MSWRShellCanOpen(LPTSTR key, char *cmdbuf, int clen, int allow_noreg);
BOOL		MSWRPeek(HKEY hRootKey, LPTSTR subkey, LPTSTR valstr,
			 LPTSTR data, DWORD *dlen);
int             mswin_store_pass_prompt(void);
void            mswin_set_erasecreds_callback(cbvoid_t);
void		mswin_setviewinwindcallback (cbvoid_t);
int             mswin_setgenhelptextcallback(cbstr_t);
int             mswin_caninput(void);
void            mswin_beginupdate(void);
void            mswin_endupdate(void);
int             mswin_sethelptextcallback(cbstr_t);
int             strucmp(char *, char *);
int             struncmp(char *, char *, int);

#ifdef	MSC_MALLOC
/*
 * These definitions will disable the SEGHEAP allocation routines, in
 * favor of the compliler libraries usual allocators.  This is useful
 * when external debugging tools and the SEGHEAP debugging routines step
 * on each other...
 */
#define MemAlloc(X) malloc(X)
#define MemFree(X) free(X)
#define MemRealloc(X,Y) realloc(X,Y)
#define MemFreeAll()
#define	MemDebug(X,Y)
#else
/*
 * Memory management stuff, from msmem.c
 */
typedef unsigned long		MemSize;
typedef void __far	      *	MemPtr;

#define MemAlloc(s)		_MemAlloc (s, __FILE__, __LINE__)
#define malloc(s)		_MemAlloc (s, __FILE__, __LINE__)
#define MemFree(b)		_MemFree (b, __FILE__, __LINE__)
#define free(b)			_MemFree (b, __FILE__, __LINE__)
#define MemRealloc(b,s)		_MemRealloc (b, s, __FILE__, __LINE__)
#define realloc(b,s)		_MemRealloc (b, s, __FILE__, __LINE__)
#define MEM_BLOCK_SIZE_MAX	0xff00

void		MemDebug (int debug, FILE *debugFile);
void __far *	_MemAlloc (MemSize size, char __far * file, int line);
int		_MemFree (void __far *block, char __far *file, int line);
void __far *	_MemRealloc (void __far *block, MemSize size, 
					char __far * file, int line);
MemSize		MemBlkSize (MemPtr block);
void		MemFreeAll (void);
void            MemFailSoon (MemSize);
#endif

/* functions from win moved to mswin.c */
int	pico_scroll_callback (int, long);
#undef sleep
int     sleep (int);

/*
 * Signals that are not defined by MS C
 */
#define SIGHUP	    1	/* Terminal hangup. */
#define SIGINT      2   /* Ctrl-C sequence */
#define SIGILL      4   /* illegal instruction - invalid function image */
#define SIGSEGV     11  /* segment violation */
#define SIGALRM     14  /* alarm clock */
#define SIGTERM     15  /* Software termination signal from kill */
#define SIGABRT     22  /* abnormal termination triggered by abort call */
#define SIGWINCH    28	/* Change in window size. */


#define fflush	mswin_fflush

#define	alarm	mswin_alarm

/*
 * Registry setting constants
 */
#define	MSWR_PINE_RC	       1
#define	MSWR_PINE_DIR	       2
#define	MSWR_PINE_EXE	       3
#define MSWR_PINE_AUX          4
#define MSWR_PINE_POS          5
#define MSWR_PINE_CONF         6

#define MSWR_SDC_MAIL 1
#define MSWR_SDC_NEWS 2
#define MSWR_SDC_NNTP 3
#define MSWR_SDC_IMAP 4

#define	MSWR_NULL	0x00
#define	MSWR_OP_SET	0x01
#define	MSWR_OP_FORCE	0x02
#define	MSWR_OP_GET	0x04
#define	MSWR_OP_BLAST	0x08

#endif /* MSWIN_H */
