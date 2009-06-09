#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: termout.unx.c 159 2006-10-02 22:00:13Z hubert@u.washington.edu $";
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
#include <system.h>
#include <general.h>

#include "../../c-client/mail.h"	/* for MAILSTREAM and friends */
#include "../../c-client/osdep.h"
#include "../../c-client/rfc822.h"	/* for soutr_t and such */
#include "../../c-client/misc.h"	/* for cpystr proto */
#include "../../c-client/utf8.h"	/* for CHARSET and such*/
#include "../../c-client/imap4r1.h"

#include "../../pith/osdep/color.h"

#include "../../pith/charconv/filesys.h"

#include "../../pith/debug.h"
#include "../../pith/conf.h"
#include "../../pith/newmail.h"

#include "../../pico/estruct.h"
#include "../../pico/pico.h"
#include "../../pico/keydefs.h"

#include "../../pico/osdep/mswin.h"
#include "../../pico/osdep/color.h"

#include "../flagmaint.h"
#include "../status.h"
#include "../keymenu.h"
#include "../titlebar.h"

#include "termout.gen.h"
#include "termout.wnt.h"

/*======================================================================
       Routines for painting the screen
          - figure out what the terminal type is
          - deal with screen size changes
          - save special output sequences
          - the usual screen clearing, cursor addressing and scrolling


     This library gives programs the ability to easily access the
     termcap information and write screen oriented and raw input
     programs.  The routines can be called as needed, except that
     to use the cursor / screen routines there must be a call to
     InitScreen() first.  The 'Raw' input routine can be used
     independently, however. (Elm comment)

     Not sure what the original source of this code was. It got to be
     here as part of ELM. It has been changed significantly from the
     ELM version to be more robust in the face of inconsistent terminal
     autowrap behaviour. Also, the unused functions were removed, it was
     made to pay attention to the window size, and some code was made nicer
     (in my opinion anyways). It also outputs the terminal initialization
     strings and provides for minimal scrolling and detects terminals
     with out enough capabilities. (Pine comment, 1990)

*/

#define	PUTLINE_BUFLEN	256

static int   _lines, _columns;

BOOL CALLBACK  __export 
args_dialog_proc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

BOOL CALLBACK  __export 
login_dialog_proc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

BOOL CALLBACK  __export 
flag_dialog_proc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

BOOL CALLBACK  __export 
sort_dialog_proc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

BOOL CALLBACK  __export 
config_vars_dialog_proc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

BOOL CALLBACK  __export 
config_dialog_proc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

void
init_screen(void)
{
    /*
     * If we want to show it, turn it on. If we don't care, put it back
     * to the way it was originally.
     */
    mswin_showcaret(F_ON(F_SHOW_CURSOR, ps_global));
    mswin_trayicon(F_ON(F_ENABLE_TRAYICON, ps_global));
    return;					/* NO OP */
}

void
end_screen(char *message, int exit_val)
{
    int footer_rows_was_one = 0;

    if(panicking())
      return;

    if(FOOTER_ROWS(ps_global) == 1){
	footer_rows_was_one++;
	FOOTER_ROWS(ps_global) = 3;
	mark_status_unknown();
    }

    flush_status_messages(exit_val ? 0 : 1);

    blank_keymenu(_lines - 2, 0);

    if(message){
	StartInverse();
	PutLine0(_lines - 2, 0, message);
    }
    
    EndInverse();

    MoveCursor(_lines - 1, 0);

    mswin_showcaret(F_ON(F_SHOW_CURSOR, ps_global));

    if(footer_rows_was_one){
	FOOTER_ROWS(ps_global) = 1;
	mark_status_unknown();
    }
}

/*----------------------------------------------------------------------
      Initialize the screen for output, set terminal type, etc

   Args: tt -- Pointer to variable to store the tty output structure.

 Result:  terminal size is discovered and set pine state
          termcap entry is fetched and stored in local variables
          make sure terminal has adequate capabilites
          evaluate scrolling situation
          returns status of indicating the state of the screen/termcap entry

      Returns:
        -1 indicating no terminal name associated with this shell,
        -2..-n  No termcap for this terminal type known
	-3 Can't open termcap file 
        -4 Terminal not powerful enough - missing clear to eoln or screen
	                                       or cursor motion

  ----*/
int
config_screen(struct ttyo **tt)
{
    struct ttyo *ttyo;

    _line  =  0;		/* where are we right now?? */
    _col   =  0;		/* assume zero, zero...     */

    mswin_getscreensize(&_lines, &_columns);
    if (_lines > MAX_SCREEN_ROWS)
	_lines = MAX_SCREEN_ROWS;
    if (_columns > MAX_SCREEN_COLS)
	_columns = MAX_SCREEN_COLS;

    ttyo = (struct ttyo *)fs_get(sizeof(struct ttyo));
    ttyo->screen_cols = _columns;
    ttyo->screen_rows = _lines ;
    ttyo->header_rows = 2;
    ttyo->footer_rows = 3;

    *tt = ttyo;

    return(0);
}

/*----------------------------------------------------------------------
       Get the current window size
  
   Args: ttyo -- pointer to structure to store window size in

  NOTE: we don't override the given values unless we know better
 ----*/
int
get_windsize(struct ttyo *ttyo)
{
    char	fontName[LF_FACESIZE+1];
    char	fontSize[12];
    char	fontStyle[64];
    char        fontCharSet[256];
    char	windowPosition[32], windowPositionReg[32];
    char	foreColor[64], backColor[64];
    int		newRows, newCols;
    char	cursorStyle[32];

    cursorStyle[0] = '\0';

    /* Get the new window parameters and update the 'pinerc' variables. */
    mswin_getwindow(fontName, sizeof(fontName), fontSize, sizeof(fontSize),
		    fontStyle, sizeof(fontStyle),
		    windowPosition, sizeof(windowPosition),
		    foreColor, sizeof(foreColor),
		    backColor, sizeof(backColor),
		    cursorStyle, sizeof(cursorStyle),
		    fontCharSet, sizeof(fontCharSet));

    if(!ps_global->VAR_FONT_NAME
       || strucmp(ps_global->VAR_FONT_NAME, fontName))
      set_variable(V_FONT_NAME, fontName, 1, 0,
		   ps_global->ew_for_except_vars);

    if(!ps_global->VAR_FONT_SIZE
       || strucmp(ps_global->VAR_FONT_SIZE, fontSize))
      set_variable(V_FONT_SIZE, fontSize, 1, 0,
		   ps_global->ew_for_except_vars);

    if(!ps_global->VAR_FONT_STYLE
       || strucmp(ps_global->VAR_FONT_STYLE, fontStyle))
      set_variable(V_FONT_STYLE, fontStyle, 1, 0,
		   ps_global->ew_for_except_vars);

    if(!ps_global->VAR_FONT_CHAR_SET
       || strucmp(ps_global->VAR_FONT_CHAR_SET, fontCharSet))
      set_variable(V_FONT_CHAR_SET, fontCharSet, 1, 0,
		   ps_global->ew_for_except_vars);

    if(strnicmp(windowPosition, "MIN0", 4) && !strchr(windowPosition, '!')){
	   if(F_ON(F_STORE_WINPOS_IN_CONFIG, ps_global)){
	       if(!ps_global->VAR_WINDOW_POSITION
		  || strucmp(ps_global->VAR_WINDOW_POSITION, windowPosition))
		 set_variable(V_WINDOW_POSITION, windowPosition, 1, 0,
			      ps_global->ew_for_except_vars);
	   }
	   else{
	       /*
		* Get the window position stored in the registry.
		*
		* If current window position is not in the registry or is
		* different from what is there, save it.
		*/
	       if((!mswin_reg(MSWR_OP_GET, MSWR_PINE_POS, windowPositionReg,
			      sizeof(windowPositionReg)) ||
		   strucmp(windowPositionReg, windowPosition))
		  && (ps_global->update_registry != UREG_NEVER_SET))
		 mswin_reg(MSWR_OP_SET | MSWR_OP_FORCE, MSWR_PINE_POS, windowPosition,
			   (size_t)NULL);
	   }
    }
    
    mswin_getprintfont(fontName, sizeof(fontName),
		       fontSize, sizeof(fontSize),
		       fontStyle, sizeof(fontStyle),
		       fontCharSet, sizeof(fontCharSet));
    if(!ps_global->VAR_PRINT_FONT_NAME
       || strucmp(ps_global->VAR_PRINT_FONT_NAME, fontName))
      set_variable(V_PRINT_FONT_NAME, fontName, 1, 0,
		   ps_global->ew_for_except_vars);

    if(!ps_global->VAR_PRINT_FONT_SIZE
       || strucmp(ps_global->VAR_PRINT_FONT_SIZE, fontSize))
      set_variable(V_PRINT_FONT_SIZE, fontSize, 1, 0,
		   ps_global->ew_for_except_vars);

    if(!ps_global->VAR_PRINT_FONT_STYLE
       || strucmp(ps_global->VAR_PRINT_FONT_STYLE, fontStyle))
      set_variable(V_PRINT_FONT_STYLE, fontStyle, 1, 0,
		   ps_global->ew_for_except_vars);

    if(!ps_global->VAR_PRINT_FONT_CHAR_SET
       || strucmp(ps_global->VAR_PRINT_FONT_CHAR_SET, fontCharSet))
      set_variable(V_PRINT_FONT_CHAR_SET, fontCharSet, 1, 0,
		   ps_global->ew_for_except_vars);

    if(!ps_global->VAR_NORM_FORE_COLOR
       || strucmp(ps_global->VAR_NORM_FORE_COLOR, foreColor))
      set_variable(V_NORM_FORE_COLOR, foreColor, 1, 0,
		   ps_global->ew_for_except_vars);

    if(!ps_global->VAR_NORM_BACK_COLOR
       || strucmp(ps_global->VAR_NORM_BACK_COLOR, backColor))
      set_variable(V_NORM_BACK_COLOR, backColor, 1, 0,
		   ps_global->ew_for_except_vars);

    if(cursorStyle[0] && !ps_global->VAR_CURSOR_STYLE
       || strucmp(ps_global->VAR_CURSOR_STYLE, cursorStyle))
      set_variable(V_CURSOR_STYLE, cursorStyle, 1, 0,
		   ps_global->ew_for_except_vars);

    /* Get new window size.  Compare to old.  The window may have just
     * moved, in which case we don't bother updating the size. */
    mswin_getscreensize(&newRows, &newCols);
    if (newRows == ttyo->screen_rows && newCols == ttyo->screen_cols)
	    return (NO_OP_COMMAND);

    /* True resize. */
    ttyo->screen_rows = newRows;
    ttyo->screen_cols = newCols;

    if (ttyo->screen_rows > MAX_SCREEN_ROWS)
	ttyo->screen_rows = MAX_SCREEN_ROWS;
    if (ttyo->screen_cols > MAX_SCREEN_COLS)
	ttyo->screen_cols = MAX_SCREEN_COLS;

    return(KEY_RESIZE);
}

/*----------------------------------------------------------------------
        Move the cursor to the row and column number
 Input:  row number
         column number

 Result: Cursor moves
         internal position updated
  ----------------------------------------------------------------------*/
void
MoveCursor(int row, int col)
{
    /** move cursor to the specified row column on the screen.
        0,0 is the top left! **/

    if(ps_global->in_init_seq)
      return;

    /*
     * This little hack is here for screen readers, for example, a screen
     * reader associated with a braille display. If we flash the cursor off
     * and on in the same position, the reader will be drawn to it. Since we
     * are calling MoveCursor every time through the message viewing loops we
     * need to suppress the calls if they aren't really doing anything.
     * However, mswin_move actually does more than just move the cursor.
     * It also call FlushAccum(), which is necessary in some cases. For
     * example, in optionally_enter the user is typing characters in and
     * the FlushAccum() allows them to show up on the screen. The putblock
     * call is a quick hack to get the Flush_Accum() call, which is all
     * the putblock does when the arg is NULL.
     */
    if(row == _line && col == _col){
	mswin_putblock(NULL, 0);
	return;
    }

    mswin_move(row, col);
    _line = row;
    _col = col;
}


/*----------------------------------------------------------------------
    Write a character to the screen, keeping track of cursor position

 Input:  charater to write

 Result: character output
         cursor position variables updated
  ----------------------------------------------------------------------*/
void
Writewchar(UCS ucs)
{
    int width;

    if(ps_global->in_init_seq)
      return;

    switch(ucs){
      case LINE_FEED :
	_line = min(_line+1,ps_global->ttyo->screen_rows);
        _col =0;
        mswin_move(_line, _col);
	break;

      case RETURN :		/* move to column 0 */
	_col = 0;
        mswin_move(_line, _col);

      case BACKSPACE :		/* move back a space if not in column 0 */
	if(_col > 0)
          mswin_move(_line, --_col);

	break;
	
      case BELL :		/* ring the bell but don't advance _col */
	mswin_beep();		/* libpico call */
	break;

      case TAB:			/* if a tab, output it */
	do
	  mswin_putc(' ');
	while(((++_col)&0x07) != 0);
	break;

      default:
	/* pass_ctrl_chars is always 1 for Windows */
	width = wcellwidth(ucs);
	if(width < 0){
	    mswin_putc('?');
	    _col++;
	}
	else if(_col + width > ps_global->ttyo->screen_cols){
	    int i;

	    i = ps_global->ttyo->screen_cols - _col - 1;
	    while(i-- > 0)
	      mswin_putc(' ');

	    _col = ps_global->ttyo->screen_cols - 1;
	    mswin_move(_line, _col);
	    mswin_putc('>');
	    _col++;
	}
	else{
	    mswin_putc(ucs);
	    _col += width;
	}
    }

    if(_col >= ps_global->ttyo->screen_cols){
	_col = ps_global->ttyo->screen_cols;
	mswin_move(_line, _col);
    }

    return;
}


/*----------------------------------------------------------------------
      Printf style write directly to the terminal at current position

 Input: printf style control string
        number of printf style arguments
        up to three printf style arguments

 Result: Line written to the screen
  ----------------------------------------------------------------------*/
void
Write_to_screen(char *string)
{
    if(ps_global->in_init_seq)
      return;

    mswin_puts (string);
    
    /*
     * mswin_puts does not keep track of where we are. Simple fix is to
     * reset _line and _col to unknown so that the next MoveCursor will
     * do a real move. We could check for a simple string and actually
     * keep track. Is that useful? I don't think so.
     *
     * There are other places where we lose track, but by comparing with
     * the termout.unx code where we also lose track it would seem we don't
     * really need to keep track in those other cases.
     */
    _line  = FARAWAY;
    _col   = FARAWAY;
}

void
Write_to_screen_n(char *string, int n)
{
    if(ps_global->in_init_seq)
      return;

    mswin_puts_n (string, n);
    _line  = FARAWAY;
    _col   = FARAWAY;
}


/*----------------------------------------------------------------------
     Clear the terminal screen
  ----------------------------------------------------------------------*/
void
ClearScreen(void)
{
    _line = 0;			/* clear leaves us at top... */
    _col  = 0;

    if(ps_global->in_init_seq)
      return;

    mark_status_unknown();
    mark_keymenu_dirty();
    mark_titlebar_dirty();

    mswin_move(0, 0);
    mswin_eeop();
}

/*----------------------------------------------------------------------
    Clear screen to end of line on current line
  ----------------------------------------------------------------------*/
void
CleartoEOLN(void)
{
    mswin_eeol();
}


/*----------------------------------------------------------------------
     function to output string such that it becomes icon text

   Args: s -- string to write

 Result: string indicated become our "icon" text
  ----*/
void
icon_text(char *s, int type)
{
    if(type == IT_MCLOSED)
      mswin_mclosedtext (s);
    else    /* IT_NEWMAIL */
      mswin_newmailtext (s);
}

/* Scroll stuff */

/*
 * Set the current position of the scroll bar.
 */
void
scroll_setpos(long newpos)
{
    mswin_setscrollpos (newpos);
}

/*
 * Set the range of the scroll bar.  
 * zero disables the scroll bar.
 */
void
scroll_setrange (long pagelen, long rangeend)
{
    if(rangeend < pagelen)
      mswin_setscrollrange (0L, 0L);
    else
      mswin_setscrollrange (pagelen, rangeend + pagelen - 1L);
}

void
EndScroll(void)
{
}

int
ScrollRegion(int lines)
{
    return(-1);
}

int
BeginScroll(int top, int bottom)
{
    static int __t, __b;

    __t = top;
    __b = bottom;
    return(-1);
}


/* Dialog stuff */
#define WINDOW_USER_DATA GWL_USERDATA

/*===========================================================================
 *
 * Dialog Data
 *
 * The following structures hold the state data for dialogs.
 */
typedef struct DLG_TYPEMAP {
    int		pineid;
    int		rsrcid;
} DLG_TYPEMAP;

typedef struct DLG_SORTDATA {
    DLG_SORTPARAM	*sortsel;	/* parameter structure. */
    int			sortcount;	/* Number of different sorts. */
    DLG_TYPEMAP		types[20];	/* Map pine sort types to ctrl ids. */
} DLG_SORTDATA;

typedef struct DLG_LOGINDATA {
    NETMBX              *mb;
    LPTSTR               user;
    int                  userlen;
    LPTSTR               pwd;
    int                  pwdlen;
    int                  pwc;  /* set if we're using passfiles and we don't know user yet */
    int                  fixuser;
    int                  prespass;  /* if user wants to preserve the password */
    int                  rv;
} DLG_LOGINDATA;

typedef struct DLG_CONFIGDATA {
    char                *confpath;  /* UTF-8 path */
    int                  confpathlen;
    int                  setreg;
    int                  nopinerc;
    int                  rv;
} DLG_CONFIGDATA;

typedef struct installvars {
    char *pname;
    char *userid;
    char *domain;
    char *inboxpath;
    char *smtpserver;
    unsigned defmailclient:1;
    unsigned defnewsclient:1;
} INSTALLVARS_S;

typedef struct DLG_CONGIGVARSDATA {
    INSTALLVARS_S *ivars;
    int            rv;
} DLG_CONFIGVARSDATA;

typedef struct DLG_FLAGDATA {
    struct flag_table	*ftbl;
    int			flagcount;
    HINSTANCE		hInstance;
    HWND		hTextWnd;
} DLG_FLAGDATA;

int  os_config_vars_dialog(INSTALLVARS_S *);
static void GetBtnPos (HWND, HWND, RECT *);

int
init_install_get_vars(void)
{
    INSTALLVARS_S ivars;
    struct pine *ps = ps_global;
    char *p;

    memset((void *)&ivars, 0, sizeof(INSTALLVARS_S));
    if(ps->vars[V_PERSONAL_NAME].main_user_val.p)
      ivars.pname = cpystr(ps->vars[V_PERSONAL_NAME].main_user_val.p);
    if(ps->vars[V_USER_ID].main_user_val.p)
      ivars.userid = cpystr(ps->vars[V_USER_ID].main_user_val.p);
    if(ps->vars[V_USER_DOMAIN].main_user_val.p)
      ivars.domain = cpystr(ps->vars[V_USER_DOMAIN].main_user_val.p);
    if(ps->vars[V_INBOX_PATH].main_user_val.p)
      ivars.inboxpath = cpystr(ps->vars[V_INBOX_PATH].main_user_val.p);
    else if(ps->prc && ps->prc->name && (*ps->prc->name == '{')
	    && (p = strindex(ps->prc->name, '}'))){
	ivars.inboxpath = cpystr(ps->prc->name);
	if(p = strindex(ivars.inboxpath, '}'))
	  *(p+1) = '\0';
    }
    if(ps->vars[V_SMTP_SERVER].main_user_val.l
	&& *ps->vars[V_SMTP_SERVER].main_user_val.l)
      ivars.smtpserver = cpystr(ps->vars[V_SMTP_SERVER].main_user_val.l[0]);
    ivars.defmailclient = mswin_is_def_client(MSWR_SDC_MAIL);
    ivars.defnewsclient = mswin_is_def_client(MSWR_SDC_NEWS);

    if(os_config_vars_dialog(&ivars) == 0){
	set_variable(V_PERSONAL_NAME, ivars.pname, 0, 0, Main);
	set_variable(V_USER_ID, ivars.userid, 0, 0, Main);
	set_variable(V_USER_DOMAIN, ivars.domain, 0, 0, Main);
	set_variable(V_INBOX_PATH, ivars.inboxpath, 0, 0, Main);
	if(ivars.smtpserver){
	    if(ps->vars[V_SMTP_SERVER].main_user_val.l){
		fs_give((void **)&ps->vars[V_SMTP_SERVER].main_user_val.l[0]);
		ps->vars[V_SMTP_SERVER].main_user_val.l[0] 
		  = cpystr(ivars.smtpserver);
		set_current_val(&ps->vars[V_SMTP_SERVER], TRUE, FALSE);
	    }
	    else {
		char *tstrlist[2];

		tstrlist[0] = ivars.smtpserver;
		tstrlist[1] = NULL;
		set_variable_list(V_SMTP_SERVER, tstrlist, 0, Main);
	    }
	}
	write_pinerc(ps_global, Main, WRP_NONE);
	if(ivars.defmailclient)
	  mswin_set_def_client(MSWR_SDC_MAIL);
	if(ivars.defnewsclient)
	  mswin_set_def_client(MSWR_SDC_NEWS);

	/* Tell Windows that stuff has changed */
	if(ivars.defmailclient || ivars.defnewsclient)
	  SendMessageTimeout(HWND_BROADCAST, WM_SETTINGCHANGE,
			     (WPARAM)NULL, (LPARAM)NULL, 
			     SMTO_NORMAL, 1000, NULL);
    }

    if(ivars.pname)
      fs_give((void **)&ivars.pname);
    if(ivars.userid)
      fs_give((void **)&ivars.userid);
    if(ivars.domain)
      fs_give((void **)&ivars.domain);
    if(ivars.inboxpath)
      fs_give((void **)&ivars.inboxpath);
    if(ivars.smtpserver)
      fs_give((void **)&ivars.smtpserver);
    return 0;
}

/*
 * Show args dialog
 */
int
os_argsdialog (char **arg_text)
{
    DLGPROC	dlgprc;
    HINSTANCE	hInst;
    HWND	hWnd;

    hInst = (HINSTANCE) mswin_gethinstance ();	
    hWnd = (HWND) mswin_gethwnd ();

    mswin_killsplash();

    dlgprc = args_dialog_proc;

    DialogBoxParam (hInst, MAKEINTRESOURCE (IDD_ARGLIST), hWnd,
		    dlgprc, (LPARAM) arg_text);

    return(1);
}

/*
 * Prompt for username and password
 */
int
os_login_dialog (NETMBX *mb, char *user_utf8, int userlen,
		 char *pwd_utf8, int pwdlen, int pwc, int fixuser, int *prespass)
{
    DLGPROC	dlgprc;
    HINSTANCE	hInst;
    HWND	hWnd;
    DLG_LOGINDATA  dlgpw;
    LPTSTR user_lptstr, pwd_lptstr;
    char *tuser_utf8, *tpwd_utf8;

    mswin_killsplash();
    hInst = (HINSTANCE) mswin_gethinstance ();	
    hWnd = (HWND) mswin_gethwnd ();

    dlgpw.mb = mb;

    dlgpw.user = (LPTSTR)fs_get(userlen*sizeof(TCHAR));
    user_lptstr = utf8_to_lptstr(user_utf8);
    _tcsncpy(dlgpw.user, user_lptstr, userlen - 1);
    dlgpw.user[userlen - 1] = '\0';
    fs_give((void **) &user_lptstr);
    dlgpw.userlen = userlen;

    dlgpw.pwd = (LPTSTR)fs_get(pwdlen*sizeof(TCHAR));
    pwd_lptstr = utf8_to_lptstr(pwd_utf8);
    _tcsncpy(dlgpw.pwd, pwd_lptstr, pwdlen - 1);
    dlgpw.pwd[pwdlen - 1] = '\0';
    fs_give((void **) &pwd_lptstr);
    dlgpw.pwdlen = pwdlen;

    dlgpw.fixuser = fixuser;
    dlgpw.pwc = pwc;
    dlgpw.rv = 0;

    dlgprc = login_dialog_proc;

    DialogBoxParam (hInst, MAKEINTRESOURCE (ps_global->install_flag
					    ? IDD_LOGINDLG2 : IDD_LOGINDLG), 
		    NULL, dlgprc, (LPARAM)&dlgpw);

    if(dlgpw.rv == 0){
	tuser_utf8 = lptstr_to_utf8(dlgpw.user);
	if(tuser_utf8){
	    strncpy(user_utf8, tuser_utf8, userlen - 1);
	    user_utf8[userlen - 1] = '\0';
	    fs_give((void **) &tuser_utf8);
	}

	tpwd_utf8 = lptstr_to_utf8(dlgpw.pwd);
	if(tpwd_utf8){
	    strncpy(pwd_utf8, tpwd_utf8, pwdlen - 1);
	    pwd_utf8[pwdlen - 1] = '\0';
	    fs_give((void **) &tpwd_utf8);
	}
	if(prespass)
	  (*prespass) = dlgpw.prespass;
    }

    return(dlgpw.rv);
}

/*
 * Select message flags.
 */
int
os_flagmsgdialog (struct flag_table *ftbl)
{
    DLGPROC	dlgprc;
    HINSTANCE	hInst;
    HWND	hWnd;
    DLG_FLAGDATA  dlgflag;
    int		rval;

    hInst = (HINSTANCE) mswin_gethinstance ();	
    hWnd = (HWND) mswin_gethwnd ();

    dlgflag.ftbl = ftbl;
    dlgflag.hInstance = hInst;
    dlgflag.hTextWnd  = hWnd;

    dlgprc = flag_dialog_proc;				   
	
    rval = DialogBoxParam (hInst, MAKEINTRESOURCE (IDD_SELECTFLAG), hWnd,
    		dlgprc, (LPARAM)&dlgflag);

    return (rval);
}

/*
 * Select a sort type.
 */

int
os_sortdialog (DLG_SORTPARAM *sortsel)
{
    DLGPROC	dlgprc;
    HINSTANCE	hInst;
    HWND	hWnd;
    int		i;
    DLG_SORTDATA  dlgsort;

    hInst = (HINSTANCE) mswin_gethinstance ();	
    hWnd = (HWND) mswin_gethwnd ();


    /* Build a map of pine sort types to the resource types. */
    i = 0;
    dlgsort.types[i].pineid   = SortArrival;
    dlgsort.types[i++].rsrcid = IDC_SORTARRIVAL;
    dlgsort.types[i].pineid   = SortDate;
    dlgsort.types[i++].rsrcid = IDC_SORTDATE;
    dlgsort.types[i].pineid   = SortFrom;
    dlgsort.types[i++].rsrcid = IDC_SORTFROM;
    dlgsort.types[i].pineid   = SortSubject;
    dlgsort.types[i++].rsrcid = IDC_SORTSUBJECT;
    dlgsort.types[i].pineid   = SortSubject2;
    dlgsort.types[i++].rsrcid = IDC_SORTORDERSUB;
    dlgsort.types[i].pineid   = SortTo;
    dlgsort.types[i++].rsrcid = IDC_SORTTO;
    dlgsort.types[i].pineid   = SortCc;
    dlgsort.types[i++].rsrcid = IDC_SORTCC;
    dlgsort.types[i].pineid   = SortSize;
    dlgsort.types[i++].rsrcid = IDC_SORTSIZE;
    dlgsort.types[i].pineid   = SortThread;
    dlgsort.types[i++].rsrcid = IDC_SORTTHREAD;
    dlgsort.types[i].pineid   = SortScore;
    dlgsort.types[i++].rsrcid = IDC_SORTSCORE;
    dlgsort.sortcount = i;
    dlgsort.sortsel = sortsel;

    dlgprc = sort_dialog_proc;

    DialogBoxParam (hInst, MAKEINTRESOURCE (IDD_SELECTSORT), hWnd,
    		dlgprc, (LPARAM)&dlgsort);

    return(1);
}


/*
 * Dialog proc to handle index sort selection.
 * 
 * Configures the dialog box on init and retrieves the settings on exit.
 */    
BOOL CALLBACK  __export 
args_dialog_proc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    BOOL ret = FALSE;
    long i, j, block_size;
    char **args_text;
    LPTSTR args_text_lptstr, args_block_lptstr;

    
    switch (uMsg) {
    case WM_INITDIALOG:

	args_text = (char **)lParam;

	/*
	 * First convert char *'s over to one big block of
	 * Unicode
	 */
	i = 0;
	while(args_text && *args_text){
	    i += strlen(*args_text++);
	    i += 2;
	}
	block_size = i;

	args_block_lptstr = (LPTSTR)fs_get((block_size+1)*sizeof(TCHAR));

	args_text = (char **)lParam;
	i = 0;
	j = 0;
	while(args_text && *args_text){
	    args_text_lptstr = utf8_to_lptstr(*args_text++);
	    while(args_text_lptstr[i] && j < block_size){
		args_block_lptstr[j++] = args_text_lptstr[i++];
	    }
	    args_block_lptstr[j++] = '\r';
	    args_block_lptstr[j++] = '\n';
	    fs_give((void **) &args_text_lptstr);
	    i = 0;
	}
	args_block_lptstr[j] = '\0';

	/* and replace everything selected with args_text */
	SendDlgItemMessage(hDlg, IDC_ARGTEXT, WM_SETTEXT, (WPARAM) 0,
			   (LPARAM) args_block_lptstr);
	fs_give((void **)&args_block_lptstr);

 	return (1);

    case WM_CLOSE :
        ret = TRUE;
        EndDialog (hDlg, TRUE);
        break;


    case WM_COMMAND:
	switch (wParam) {
	case IDOK:
	    ret = TRUE;
	    EndDialog (hDlg, TRUE);
	    break;
        }
	break;
	

    }
    return (ret);
}

/*
 * Dialog proc to handle login
 * 
 * Configures the dialog box on init and retrieves the settings on exit.
 */    
BOOL CALLBACK  __export 
login_dialog_proc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    DLG_LOGINDATA *dlglogin;
    BOOL ret = FALSE;
    TCHAR tcbuf[1024];
    NETMBX *mb;
    LPTSTR  user, pwd;
    LPTSTR  host_lptstr;

    switch (uMsg) {
    case WM_INITDIALOG:

	dlglogin = (DLG_LOGINDATA *)lParam;
	mb = dlglogin->mb;
	user = dlglogin->user;
	pwd = dlglogin->pwd;
	SetWindowLong (hDlg, WINDOW_USER_DATA, (LONG) dlglogin);

	host_lptstr = utf8_to_lptstr(mb->host);
	_sntprintf(tcbuf, sizeof(tcbuf), TEXT("Host: %.100s%s"), host_lptstr,
		   !mb->sslflag && !mb->tlsflag ? TEXT(" (INSECURE)") : TEXT(""));
	fs_give((void **) &host_lptstr);

	if(mb->sslflag || mb->tlsflag)
	  SetWindowText(hDlg, TEXT("Alpine Login  +"));
	else
	  SetWindowText(hDlg, TEXT("Alpine Insecure Login"));

	if(dlglogin->pwc){
	    EnableWindow(GetDlgItem(hDlg, IDC_RPASSWORD),0);
	    EnableWindow(GetDlgItem(hDlg, IDC_RPWTEXT),0);
	    EnableWindow(GetDlgItem(hDlg, IDC_PRESPASS),0);
	}
	if(mb->user && *mb->user){
	    LPTSTR user_lptstr;

	    user_lptstr = utf8_to_lptstr(mb->user);
	    SetDlgItemText(hDlg, IDC_RLOGINE, user_lptstr);
	    if((mb->user && *mb->user) || dlglogin->fixuser)
	      EnableWindow(GetDlgItem(hDlg, IDC_RLOGINE), 0);
	    fs_give((void **) &user_lptstr);
	}
	else if(user){
	    SetDlgItemText(hDlg, IDC_RLOGINE, user);
	    if(dlglogin->fixuser)
	      EnableWindow(GetDlgItem(hDlg, IDC_RLOGINE), 0);
	}
	SetDlgItemText(hDlg, IDC_PROMPT, tcbuf);
 	return (1);


    case WM_COMMAND:
	dlglogin = (DLG_LOGINDATA *) GetWindowLong (hDlg, WINDOW_USER_DATA);
	switch (wParam) {
	case IDOK:
	    /* Retrieve the new username/passwd. */
	    user = dlglogin->user;
	    pwd = dlglogin->pwd;
	    GetDlgItemText(hDlg, IDC_RLOGINE, user, dlglogin->userlen - 1);
	    GetDlgItemText(hDlg, IDC_RPASSWORD, pwd, dlglogin->pwdlen - 1);
	    user[dlglogin->userlen - 1] = '\0';
	    pwd[dlglogin->pwdlen - 1] = '\0';
	    dlglogin->prespass = (IsDlgButtonChecked(hDlg, IDC_PRESPASS) == BST_CHECKED);

	    EndDialog (hDlg, LOWORD(wParam));
	    dlglogin->rv = 0;
	    ret = TRUE;
	    break;

	case IDCANCEL:
	    dlglogin->rv = 1;
	    ret = TRUE;
	    EndDialog (hDlg, LOWORD(wParam));
	    break;
        }
	break;
    }
    return (ret);
}

/*
 * Dialog proc to handle flag selection.
 * 
 * Configures the dialog box on init, adding buttons as needed for
 * an unknown number of flags.
 * Retrieves the settings on exit.
 */    
BOOL CALLBACK  __export 
flag_dialog_proc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    DLG_FLAGDATA	*dlgflag;
    BOOL		ret = FALSE;
    int			i;
    struct flag_table	*fp;
    HWND		hRB[2], hBtn;
    RECT		rb[2];
    UINT		bheight, bwidth, bvertSpace;
    UINT		btnOKHeight;
    int			base, line;
    int			bstate;
    HFONT		btnFont;
    
    

    
    switch (uMsg) {
    case WM_INITDIALOG:
 	dlgflag = (DLG_FLAGDATA *)lParam;	    
	SetWindowLong (hDlg, WINDOW_USER_DATA, (LONG) dlgflag);

	/* Count buttons */
	dlgflag->flagcount = 0;
	for (fp = dlgflag->ftbl; fp && fp->name; ++fp)
		++dlgflag->flagcount;

	/* Get the positions of the current buttons. */
	for (i = 0; i < 2; ++i) {
	    hRB[i] = GetDlgItem (hDlg, IDC_FLAGCOL1 + i);
	    GetBtnPos (hDlg, hRB[i], &rb[i]);
	}
	bheight = rb[0].bottom - rb[0].top;
	bwidth  = rb[0].right  - rb[0].left;
	bvertSpace = bheight + 5;
	btnFont = (HFONT) SendMessage (hRB[0], WM_GETFONT, 0, 0);

	for (i = 0; i < dlgflag->flagcount; ++i) {
	    LPTSTR fp_name_lptstr;

	    fp = &dlgflag->ftbl[i];
	    
	    fp_name_lptstr = utf8_to_lptstr(fp->name);
	    if (i < 2) {
	    	hBtn = hRB[i];
		SetWindowText (hBtn, fp_name_lptstr);
	    }
	    else {
	    	base = i % 2;
		line = i / 2;
	    	hBtn = CreateWindow (TEXT("BUTTON"), fp_name_lptstr,
			WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
			rb[base].left, rb[base].top + bvertSpace * line,
			bwidth, bheight, 
			hDlg, (HMENU)NULL, dlgflag->hInstance, NULL);
		SetWindowLong (hBtn, GWL_ID, IDC_FLAGCOL1 + i);
	    	SendMessage (hBtn, WM_SETFONT, (WPARAM)btnFont, 
				MAKELPARAM (0, 0));
	    }

	    fs_give((void **) &fp_name_lptstr);
	    if (fp->ukn) 
	    	SendMessage (hBtn, BM_SETSTYLE, 
			(WPARAM)(BS_CHECKBOX | BS_AUTO3STATE), 0);
	    SendMessage (hBtn, BM_SETCHECK, 
		(WPARAM) fp->set == CMD_FLAG_UNKN ? 2 :	fp->set ? 1 : 0,
		0);
	    ShowWindow (hBtn, SW_SHOW);
	    EnableWindow (hBtn, TRUE);
	}
	
	/* Position the OK and Cancel buttons. */
	line = (dlgflag->flagcount + 1) / 2;
	for (i = 0; i < 2; ++i) {
	    hRB[1] = GetDlgItem (hDlg, i == 0 ? IDOK : IDCANCEL);
	    GetBtnPos (hDlg, hRB[1], &rb[1]);
	    MoveWindow (hRB[1], rb[1].left, rb[0].top + bvertSpace * line,
		    rb[1].right - rb[1].left, rb[1].bottom - rb[1].top, 
		    FALSE);
	    btnOKHeight = rb[1].bottom - rb[1].top;
        }
	
	
	/* Resize whole dialog window. */
	GetWindowRect (hDlg, &rb[1]);
	rb[1].right -= rb[1].left;
	rb[1].bottom = rb[0].top + bvertSpace * line + btnOKHeight + 10 + 
				GetSystemMetrics (SM_CYCAPTION);
	MoveWindow (hDlg, rb[1].left, rb[1].top, rb[1].right,
		rb[1].bottom, TRUE);
 	return (1);


    case WM_COMMAND:
	dlgflag = (DLG_FLAGDATA *) GetWindowLong (hDlg, WINDOW_USER_DATA);
	switch (wParam) {

	case IDOK:
	    /* Retrieve the button states. */
	    for (i = 0; i < dlgflag->flagcount; ++i) {
		fp = &dlgflag->ftbl[i];
		bstate = SendMessage (GetDlgItem (hDlg, IDC_FLAGCOL1 + i),
					BM_GETCHECK, 0, 0);
		switch (bstate) {
		case 2:	 fp->set = CMD_FLAG_UNKN;	break;
		case 1:  fp->set = CMD_FLAG_SET;	break;
		case 0:  fp->set = CMD_FLAG_CLEAR;	break;
	        }
	    }
	    EndDialog (hDlg, TRUE);
	    ret = TRUE;
	    break;

	case IDCANCEL:
	    EndDialog (hDlg, FALSE);
	    ret = TRUE;
	    break;
        }
	break;
	

    }
    return (ret);
}

/*
 * Dialog proc to handle index sort selection.
 * 
 * Configures the dialog box on init and retrieves the settings on exit.
 */    
BOOL CALLBACK  __export 
sort_dialog_proc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    DLG_SORTDATA *dlgsort;
    BOOL ret = FALSE;
    int	cursort;
    int	i;

    
    switch (uMsg) {
    case WM_INITDIALOG:
	dlgsort = (DLG_SORTDATA *)lParam;	    
	SetWindowLong (hDlg, WINDOW_USER_DATA, (LONG) dlgsort);
	
    	/* Set the reversed button state. */
	CheckDlgButton (hDlg, IDC_SORTREVERSE, dlgsort->sortsel->reverse);

	/* Set the current sort type radio button.*/
	cursort = IDC_SORTARRIVAL;
	for (i = 0; i < dlgsort->sortcount; ++i) {
	    if (dlgsort->types[i].pineid == dlgsort->sortsel->cursort) {
	    	cursort = dlgsort->types[i].rsrcid;
		break;
	    }
	}
	CheckRadioButton (hDlg, IDC_SORTFIRSTBUTTON, IDC_SORTLASTBUTTON,
			  cursort);

	EnableWindow (GetDlgItem (hDlg, IDC_GETHELP), 
				(dlgsort->sortsel->helptext != NULL));
 	return (1);


    case WM_COMMAND:
	dlgsort = (DLG_SORTDATA *) GetWindowLong (hDlg, WINDOW_USER_DATA);
	switch (wParam) {
	case IDC_GETHELP:
	    if (dlgsort->sortsel->helptext)
		mswin_showhelpmsg ((WINHAND)hDlg, dlgsort->sortsel->helptext);
	    ret = TRUE;
	    break;

	case IDOK:
	    dlgsort->sortsel->rval = 1;

	    /* Retrieve the reverse sort state. */
	    dlgsort->sortsel->reverse = (IsDlgButtonChecked (hDlg, IDC_SORTREVERSE) == 1);

	    /* Retrieve the new sort type. */
	    for (i = 0; i < dlgsort->sortcount; ++i) {
		if (IsDlgButtonChecked (hDlg, dlgsort->types[i].rsrcid)) {
		    dlgsort->sortsel->cursort = dlgsort->types[i].pineid;
		    break;
		}
	    }
	    EndDialog (hDlg, dlgsort->sortsel->rval);
	    ret = TRUE;
	    break;

	case IDCANCEL:
	    dlgsort->sortsel->rval = 0;
	    ret = TRUE;
	    EndDialog (hDlg, dlgsort->sortsel->rval);
	    break;
        }
	break;
	

    }
    return (ret);
}

int
os_config_vars_dialog (INSTALLVARS_S *ivars)
{
    DLGPROC	dlgprc;
    HINSTANCE	hInst;
    HWND	hWnd;
    DLG_CONFIGVARSDATA  dlgcv;

    mswin_killsplash();
    hInst = (HINSTANCE) mswin_gethinstance ();	
    hWnd = (HWND) mswin_gethwnd ();

    dlgcv.ivars = ivars;
    dlgcv.rv = 0;

    dlgprc = config_vars_dialog_proc;

    DialogBoxParam (hInst, MAKEINTRESOURCE (IDD_CFVARSDLG), NULL,
		    dlgprc, (LPARAM)&dlgcv);

    return(dlgcv.rv);
}

/*
 * Dialog proc to handle login
 * 
 * Configures the dialog box on init and retrieves the settings on exit.
 */    
BOOL CALLBACK  __export 
config_vars_dialog_proc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    DLG_CONFIGVARSDATA *dlgcv;
    BOOL ret = FALSE;
    int usepop = 0;
    char buf[2*MAXPATH+1], *p;
    int buflen = 2*MAXPATH;
#define TCBUFLEN (2*MAXPATH)
    TCHAR tcbuf[TCBUFLEN+1];
    LPTSTR tmp_lptstr;
    char *u;

    switch (uMsg) {
      case WM_INITDIALOG:
  	dlgcv = (DLG_CONFIGVARSDATA *)lParam;
  	SetWindowLong (hDlg, WINDOW_USER_DATA, (LONG) dlgcv);
	if(dlgcv->ivars->pname){
	    tmp_lptstr = utf8_to_lptstr(dlgcv->ivars->pname);
	    if(tmp_lptstr){
		SetDlgItemText(hDlg, IDC_CFV_PNAME, tmp_lptstr);
		fs_give((void **) &tmp_lptstr);
	    }
	}

	if(dlgcv->ivars->userid && dlgcv->ivars->domain){
	    snprintf(buf, sizeof(buf), "%.*s@%.*s", MAXPATH, dlgcv->ivars->userid,
		    MAXPATH, dlgcv->ivars->domain);
	    buf[buflen-1] = '\0';
	    tmp_lptstr = utf8_to_lptstr(buf);
	    if(tmp_lptstr){
		SetDlgItemText(hDlg, IDC_CFV_EMAILADR, tmp_lptstr);
		fs_give((void **) &tmp_lptstr);
	    }
	}

	if(dlgcv->ivars->inboxpath){
	    char *mbx;

	    mbx = dlgcv->ivars->inboxpath;
	    if(*mbx == '{' && (p = strindex(mbx, '}'))
	       && ((*(p+1) == '\0') || !strucmp(p+1, "inbox"))){
		char srvbuf[MAXPATH+1], tuser[MAXPATH+1];
		int i, j, k;

		srvbuf[0] = '\0';
		tuser[0] = '\0';
		strncpy(buf, mbx+1, min(buflen, (int)(p - (mbx+1))));
		buf[min(buflen-1, (int)(p - (mbx+1)))] = '\0';
		for(i = 0, j = 0; buf[i] && j < MAXPATH; i++){
		    if(buf[i] == '/'){
			if(!struncmp("/user=", buf+i, 6)){
			    i += 6;
			    for(k = 0; k < MAXPATH && buf[i]
				  && buf[i] != '/'; i++)
			      tuser[k++] = buf[i];
			    tuser[k] = '\0';
			    i--;
			}
			else if(!struncmp("/pop3", buf+i, 5)
				&& (!*(buf+5) || (*(buf+5) == '/'))){
			    usepop = 1;
			    i += 4;
			}
			else
			  srvbuf[j++] = buf[i];
		    }
		    else
		      srvbuf[j++] = buf[i];
		}
		srvbuf[j] = '\0';
		if(*srvbuf){
		    tmp_lptstr = utf8_to_lptstr(srvbuf);
		    if(tmp_lptstr){
			SetDlgItemText(hDlg, IDC_CFV_MSERVER, tmp_lptstr);
			fs_give((void **) &tmp_lptstr);
		    }
		}

		if(*tuser){
		    tmp_lptstr = utf8_to_lptstr(tuser);
		    if(tmp_lptstr){
			SetDlgItemText(hDlg, IDC_CFV_LOGIN, tmp_lptstr);
			fs_give((void **) &tmp_lptstr);
		    }
		}
	    }
	    else {
		tmp_lptstr = utf8_to_lptstr(mbx);
		if(tmp_lptstr){
		    SetDlgItemText(hDlg, IDC_CFV_MSERVER, tmp_lptstr);
		    fs_give((void **) &tmp_lptstr);
		}

		if(*mbx != '{' && *mbx != '#')
		  EnableWindow(GetDlgItem(hDlg, IDC_CFV_MSERVER), 0);
	    }
	}
	CheckRadioButton (hDlg, IDC_CFV_IMAP, IDC_CFV_POP3,
			  usepop ? IDC_CFV_POP3 : IDC_CFV_IMAP);
	if(dlgcv->ivars->smtpserver){
	    tmp_lptstr = utf8_to_lptstr(dlgcv->ivars->smtpserver);
	    if(tmp_lptstr){
		SetDlgItemText(hDlg, IDC_CFV_SMTPSERVER, tmp_lptstr);
		fs_give((void **) &tmp_lptstr);
	    }
	}

	if(dlgcv->ivars->defmailclient)
	  CheckDlgButton(hDlg, IDC_CFV_DEFMAILER, BST_CHECKED);
	if(dlgcv->ivars->defnewsclient)
	  CheckDlgButton(hDlg, IDC_CFV_DEFNEWSRDR, BST_CHECKED);
	
 	return (1);
	break;

      case WM_COMMAND:
	dlgcv = (DLG_CONFIGVARSDATA *) GetWindowLong (hDlg, WINDOW_USER_DATA);
	switch (wParam) {
	  case IDOK:
	    /* personal name */
	    GetDlgItemText(hDlg, IDC_CFV_PNAME, tcbuf, TCBUFLEN);
	    tcbuf[TCBUFLEN] = '\0';
	    u = lptstr_to_utf8(tcbuf);
	    if(u){
		removing_leading_and_trailing_white_space(u);
		if(dlgcv->ivars->pname)
		  fs_give((void **)&dlgcv->ivars->pname);

		if(*u){
		    dlgcv->ivars->pname = u;
		    u = NULL;
		}

		if(u)
		  fs_give((void **) &u);
	    }
	    
	    /* user-id and domain */
	    GetDlgItemText(hDlg, IDC_CFV_EMAILADR, tcbuf, TCBUFLEN);
	    tcbuf[TCBUFLEN] = '\0';
	    u = lptstr_to_utf8(tcbuf);
	    if(u){
		removing_leading_and_trailing_white_space(u);
		if(p = strindex(u, '@')){
		    *(p++) = '\0';
		    if(dlgcv->ivars->userid)
		      fs_give((void **)&dlgcv->ivars->userid);

		    if(dlgcv->ivars->domain)
		      fs_give((void **)&dlgcv->ivars->domain);

		    if(*u){
			dlgcv->ivars->userid = u;
			u = NULL;
		    }

		    if(*p)
		      dlgcv->ivars->domain = cpystr(p);
		}
		else{
		    MessageBox(hDlg, TEXT("Invalid email address.  Should be username@domain"),
			       TEXT("Alpine"), MB_ICONWARNING | MB_OK);
		    return(TRUE);
		}

		if(u)
		  fs_give((void **) &u);
	    }

	    /* inbox-path */
	    GetDlgItemText(hDlg, IDC_CFV_MSERVER, tcbuf, TCBUFLEN);
	    tcbuf[TCBUFLEN] = '\0';
	    u = lptstr_to_utf8(tcbuf);
	    if(u){
		removing_leading_and_trailing_white_space(u);
		if(*u == '{' || *u == '#'
		   || !IsWindowEnabled(GetDlgItem(hDlg, IDC_CFV_MSERVER))){
		    if(dlgcv->ivars->inboxpath)
		      fs_give((void **)&dlgcv->ivars->inboxpath);

		    dlgcv->ivars->inboxpath = u;
		    u = NULL;
		}
		else if(*u){
		    char tsrvr[4*MAXPATH+1];
		    int tsrvrlen = 4*MAXPATH;

		    if(dlgcv->ivars->inboxpath)
		      fs_give((void **)&dlgcv->ivars->inboxpath);

		    snprintf(tsrvr, sizeof(tsrvr), "{%s%s", u,
			    IsDlgButtonChecked(hDlg, IDC_CFV_POP3) 
			    == BST_CHECKED ? "/pop3" : "");

		    if(u)
		      fs_give((void **) &u);

		    GetDlgItemText(hDlg, IDC_CFV_LOGIN, tcbuf, TCBUFLEN);
		    tcbuf[TCBUFLEN] = '\0';
		    u = lptstr_to_utf8(tcbuf);
		    if(u){
			removing_leading_and_trailing_white_space(u);
			if(*u){
			    strncat(tsrvr, "/user=", sizeof(tsrvr)-strlen(tsrvr)-1);
			    strncat(tsrvr, u, sizeof(tsrvr)-strlen(tsrvr)-1);
			}

			strncat(tsrvr, "}inbox", sizeof(tsrvr)-strlen(tsrvr)-1);
			tsrvr[sizeof(tsrvr)-1] = '\0';
			dlgcv->ivars->inboxpath = cpystr(tsrvr);
		    }
		}

		if(u)
		  fs_give((void **) &u);
	    }

	    /* smtp-server */
	    GetDlgItemText(hDlg, IDC_CFV_SMTPSERVER, tcbuf, TCBUFLEN);
	    tcbuf[TCBUFLEN] = '\0';
	    u = lptstr_to_utf8(tcbuf);
	    if(u){
		removing_leading_and_trailing_white_space(u);
		if(dlgcv->ivars->smtpserver)
		  fs_give((void **)&dlgcv->ivars->smtpserver);

		if(*u){
		    dlgcv->ivars->smtpserver = u;
		    u = NULL;
		}

		if(u)
		  fs_give((void **) &u);
	    }

	    dlgcv->ivars->defmailclient =
	      (IsDlgButtonChecked(hDlg, IDC_CFV_DEFMAILER)
	       == BST_CHECKED);
	    dlgcv->ivars->defnewsclient =
	      (IsDlgButtonChecked(hDlg, IDC_CFV_DEFNEWSRDR)
	       == BST_CHECKED);

	    EndDialog (hDlg, LOWORD(wParam));
	    dlgcv->rv = 0;
	    ret = TRUE;
	    break;

	case IDCANCEL:
	    dlgcv->rv = 1;
	    ret = TRUE;
	    EndDialog (hDlg, LOWORD(wParam));
	    break;
        }
	break;
	

    }
    return (ret);
}

/*
 * Prompt for config location
 */

int
os_config_dialog (char *utf8_buf, int utf8_buflen,
		  int *regset, int nopinerc)
{
    DLGPROC	dlgprc;
    HINSTANCE	hInst;
    HWND	hWnd;
    DLG_CONFIGDATA  dlgcfg;

    mswin_killsplash();
    hInst = (HINSTANCE) mswin_gethinstance ();	
    hWnd = (HWND) mswin_gethwnd ();

    dlgcfg.confpath = utf8_buf;
    dlgcfg.confpathlen = utf8_buflen;
    dlgcfg.setreg = 0;
    dlgcfg.nopinerc = nopinerc;
    dlgcfg.rv = 0;

    dlgprc = config_dialog_proc;

    DialogBoxParam (hInst, MAKEINTRESOURCE (IDD_CONFIGDLG), NULL,
		    dlgprc, (LPARAM)&dlgcfg);

    *regset = dlgcfg.setreg;
    return(dlgcfg.rv);
}


/*
 * Dialog proc to handle login
 * 
 * Configures the dialog box on init and retrieves the settings on exit.
 */    
BOOL CALLBACK  __export 
config_dialog_proc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    DLG_CONFIGDATA *dlgcfg;
    BOOL ret = FALSE;
    int checked, def_rem_fldr = 1, def_diff_fldr = 0;

    switch (uMsg) {
    case WM_INITDIALOG:

  	dlgcfg = (DLG_CONFIGDATA *)lParam;
  	SetWindowLong (hDlg, WINDOW_USER_DATA, (LONG) dlgcfg);
	if(ps_global->install_flag)
	  SetDlgItemText(hDlg, IDC_CONFTEXT, TEXT("Please specify where Alpine should create (or look for) your personal configuration file."));
	SetDlgItemText(hDlg, IDC_CONFEFLDRNAME, TEXT("remote_pinerc"));
	if(*dlgcfg->confpath){
	    if(dlgcfg->confpath[0] == '{'){
		LPTSTR tfldr, p, tfldr2, p2, user;

		tfldr = utf8_to_lptstr((LPSTR)(dlgcfg->confpath+1));
		if(p = _tcschr(tfldr, '}')){
		    *p++ = '\0';
		    if(_tcscmp(TEXT("remote_pinerc"), p)){
			SetDlgItemText(hDlg, IDC_CONFEFLDRNAME, p);
			def_diff_fldr = 1;
		    }

		    tfldr2 = _tcsdup(tfldr);
		    for(p = tfldr, p2 = tfldr2; *p; p++)
		      if(*p == '/' && !_tcsnicmp(p, TEXT("/user="), 6)){
			  p += 6;
			  for(user = p; *p && *p != '/'; p++);
			  if(*p){
			      *p2++ = *p;
			      *p = '\0';
			  }
			  else
			    p--;
			  SetDlgItemText(hDlg, IDC_CONFEUSERNAME, user);
		      }
		      else
			*p2++ = *p;
		    *p2 = '\0';
		    SetDlgItemText(hDlg, IDC_CONFESERVER, tfldr2);
		    if(tfldr2)
		      MemFree((void *)tfldr2);
		}

		if(tfldr)
		  fs_give((void **) &tfldr);
	    }
	    else{
		LPTSTR local_file;

		local_file = utf8_to_lptstr((LPSTR)dlgcfg->confpath);
		SetDlgItemText(hDlg, IDC_CONFEFN, local_file);
		if(!dlgcfg->nopinerc)
		  def_rem_fldr = 0;

		fs_give((void **) &local_file);
	    }
	}
	CheckRadioButton (hDlg, IDC_CONFRRADIO, IDC_CONFLRADIO,
			  def_rem_fldr ? IDC_CONFRRADIO : IDC_CONFLRADIO);
	if(def_rem_fldr){
	    EnableWindow(GetDlgItem(hDlg, IDC_CONFFNTXT), 0);
	    EnableWindow(GetDlgItem(hDlg, IDC_CONFEFN), 0);
	    EnableWindow(GetDlgItem(hDlg, IDC_CONFBROWSE), 0);
	    if(!def_diff_fldr){
		CheckDlgButton(hDlg, IDC_CONFDFLTFLDR, BST_CHECKED);
		EnableWindow(GetDlgItem(hDlg, IDC_CONFEFLDRNAME), 0);
	    }
	    EnableWindow(GetDlgItem(hDlg, IDC_CONFFLDRTXT), 0);
	}
	else {
	    EnableWindow(GetDlgItem(hDlg, IDC_CONFSRVRTXT), 0);
	    EnableWindow(GetDlgItem(hDlg, IDC_CONFESERVER), 0);
	    EnableWindow(GetDlgItem(hDlg, IDC_CONFUNTXT), 0);
	    EnableWindow(GetDlgItem(hDlg, IDC_CONFEUSERNAME), 0);
	    CheckDlgButton(hDlg, IDC_CONFDFLTFLDR, BST_CHECKED);
	    EnableWindow(GetDlgItem(hDlg, IDC_CONFDFLTFLDR), 0);
	    EnableWindow(GetDlgItem(hDlg, IDC_CONFFLDRTXT), 0);
	    EnableWindow(GetDlgItem(hDlg, IDC_CONFEFLDRNAME), 0);
	}
	if(ps_global->install_flag)
	  CheckDlgButton(hDlg, IDC_CONFDFLTSET, BST_CHECKED);

 	return (1);


    case WM_COMMAND:
	dlgcfg = (DLG_CONFIGDATA *) GetWindowLong (hDlg, WINDOW_USER_DATA);
	switch (wParam) {
	case IDC_CONFDFLTFLDR:
	  checked = (IsDlgButtonChecked(hDlg, IDC_CONFDFLTFLDR) == BST_CHECKED);
	  EnableWindow(GetDlgItem(hDlg, IDC_CONFFLDRTXT), checked ? 0 : 1);
	  EnableWindow(GetDlgItem(hDlg, IDC_CONFEFLDRNAME), checked ? 0 : 1);
	  if(checked)
	    SetDlgItemText(hDlg, IDC_CONFEFLDRNAME, TEXT("remote_pinerc"));
	  break;
	case IDC_CONFRRADIO:
	case IDC_CONFLRADIO:
	    CheckRadioButton (hDlg, IDC_CONFRRADIO, IDC_CONFLRADIO, wParam);
	    EnableWindow(GetDlgItem(hDlg, IDC_CONFFNTXT), wParam == IDC_CONFRRADIO ? 0 : 1);
	    EnableWindow(GetDlgItem(hDlg, IDC_CONFEFN), wParam == IDC_CONFRRADIO ? 0 : 1);
	    EnableWindow(GetDlgItem(hDlg, IDC_CONFBROWSE), wParam == IDC_CONFRRADIO ? 0 : 1);
	    EnableWindow(GetDlgItem(hDlg, IDC_CONFSRVRTXT), wParam == IDC_CONFRRADIO ? 1 : 0);
	    EnableWindow(GetDlgItem(hDlg, IDC_CONFESERVER), wParam == IDC_CONFRRADIO ? 1 : 0);
	    EnableWindow(GetDlgItem(hDlg, IDC_CONFUNTXT), wParam == IDC_CONFRRADIO ? 1 : 0);
	    EnableWindow(GetDlgItem(hDlg, IDC_CONFEUSERNAME), wParam == IDC_CONFRRADIO ? 1 : 0);
	    EnableWindow(GetDlgItem(hDlg, IDC_CONFDFLTFLDR), wParam == IDC_CONFRRADIO ? 1 : 0);
	    if(wParam == IDC_CONFRRADIO && IsDlgButtonChecked(hDlg, IDC_CONFDFLTFLDR) == BST_UNCHECKED){
		EnableWindow(GetDlgItem(hDlg, IDC_CONFFLDRTXT), 1);
		EnableWindow(GetDlgItem(hDlg, IDC_CONFEFLDRNAME), 1);
	    }
	    else if(wParam == IDC_CONFLRADIO){
		EnableWindow(GetDlgItem(hDlg, IDC_CONFFLDRTXT), 0);
		EnableWindow(GetDlgItem(hDlg, IDC_CONFEFLDRNAME), 0);
	    }
	    break;
	case IDC_CONFBROWSE:
	    if(1){
		TCHAR fn[MAXPATH+1];
		OPENFILENAME ofn;

		fn[0] = '\0';

		/* Set up the BIG STRUCTURE. see mswin.c */
  		memset (&ofn, 0, sizeof(ofn));
  		ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;
  		ofn.hwndOwner = hDlg;
  		ofn.lpstrFilter = NULL;
  		ofn.lpstrCustomFilter = NULL;
  		ofn.nFilterIndex = 0;
  		ofn.lpstrFile = fn;
  		ofn.nMaxFile = MAXPATH;
  		ofn.lpstrFileTitle = NULL;
  		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = NULL;
  		ofn.lpstrTitle = TEXT("Select File");
  		ofn.Flags = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST;
  		ofn.lpstrDefExt = NULL;

  		if (GetOpenFileName (&ofn)) {
		    SetDlgItemText(hDlg, IDC_CONFEFN, fn);
		}
	    }
	    break;
	case IDOK:
	    /* Retrieve the new username/passwd. */
	    if(IsDlgButtonChecked(hDlg, IDC_CONFDFLTSET) == BST_CHECKED)
	      dlgcfg->setreg = 1;
	    else
	      dlgcfg->setreg = 0;
	    if(IsDlgButtonChecked(hDlg, IDC_CONFRRADIO) == BST_CHECKED){
		TCHAR lptstr_buf[MAXPATH+1];
		char *utf8_srvr, *utf8_username, *utf8_fldrname;

		GetDlgItemText(hDlg, IDC_CONFESERVER, lptstr_buf, MAXPATH);
		lptstr_buf[MAXPATH] = '\0';
		utf8_srvr = lptstr_to_utf8(lptstr_buf);
		removing_leading_and_trailing_white_space(utf8_srvr);
		if(!*utf8_srvr){
		    MessageBox(hDlg, TEXT("IMAP Server field empty"),
			       TEXT("Alpine"), MB_ICONWARNING | MB_OK);
		    if(utf8_srvr)
		      fs_give((void **) &utf8_srvr);

		    return(TRUE);
		}

		GetDlgItemText(hDlg, IDC_CONFEUSERNAME, lptstr_buf, MAXPATH);
		lptstr_buf[MAXPATH] = '\0';
		utf8_username = lptstr_to_utf8(lptstr_buf);
		removing_leading_and_trailing_white_space(utf8_username);
		if(IsDlgButtonChecked(hDlg, IDC_CONFDFLTFLDR) == BST_CHECKED){
		    utf8_fldrname = cpystr("remote_pinerc");
		}
		else{
		    GetDlgItemText(hDlg, IDC_CONFEFLDRNAME, lptstr_buf, MAXPATH);
		    lptstr_buf[MAXPATH] = '\0';
		    utf8_fldrname = lptstr_to_utf8(lptstr_buf);
		    removing_leading_and_trailing_white_space(utf8_fldrname);
		    if(!*utf8_fldrname){
			MessageBox(hDlg, TEXT("Configuration Folder Name field empty"),
				   TEXT("Alpine"), MB_ICONWARNING | MB_OK);
			if(utf8_srvr)
			  fs_give((void **) &utf8_srvr);

			if(utf8_username)
			  fs_give((void **) &utf8_username);

			if(utf8_fldrname)
			  fs_give((void **) &utf8_fldrname);

			return(TRUE);
		    }
		}
		if((strlen(utf8_srvr) 
		    + strlen(utf8_username)
		    + strlen(utf8_fldrname)
		    + 11) > dlgcfg->confpathlen){
		    MessageBox(hDlg, TEXT("Config path too long"),
			       TEXT("Alpine"), MB_ICONWARNING | MB_OK);
		    if(utf8_srvr)
		      fs_give((void **) &utf8_srvr);

		    if(utf8_username)
		      fs_give((void **) &utf8_username);

		    if(utf8_fldrname)
		      fs_give((void **) &utf8_fldrname);

		    return(TRUE);
		}

		snprintf(dlgcfg->confpath, dlgcfg->confpathlen, "{%s%s%s}%s", utf8_srvr,
			 *utf8_username ? "/user=" : "",
			utf8_username, utf8_fldrname);
		if(utf8_srvr)
		  fs_give((void **) &utf8_srvr);

		if(utf8_username)
		  fs_give((void **) &utf8_username);

		if(utf8_fldrname)
		  fs_give((void **) &utf8_fldrname);
	    }
	    else{
		TCHAR lptstr_fn[MAXPATH+1];
		char *utf8_fn;

		GetDlgItemText(hDlg, IDC_CONFEFN, lptstr_fn, MAXPATH);
		lptstr_fn[MAXPATH] = '\0';
		utf8_fn = lptstr_to_utf8(lptstr_fn);
		removing_leading_and_trailing_white_space(utf8_fn);
		if(!*utf8_fn){
		    MessageBox(hDlg, TEXT("Configuration File Name field empty"),
			       TEXT("Alpine"), MB_ICONWARNING | MB_OK);
		    if(utf8_fn)
		      fs_give((void **) &utf8_fn);

		    return(TRUE);
		}

		if(strlen(utf8_fn) >= dlgcfg->confpathlen){
		    MessageBox(hDlg, TEXT("Config path too long"),
			       TEXT("Alpine"), MB_ICONWARNING | MB_OK);
		    if(utf8_fn)
		      fs_give((void **) &utf8_fn);

		    return(TRUE);
		}

		strncpy(dlgcfg->confpath, utf8_fn, dlgcfg->confpathlen);
		dlgcfg->confpath[dlgcfg->confpathlen-1] = '\0';
		if(utf8_fn)
		  fs_give((void **) &utf8_fn);
	    }

	    EndDialog (hDlg, LOWORD(wParam));
	    dlgcfg->rv = 0;
	    ret = TRUE;
	    break;

	case IDCANCEL:
	    dlgcfg->rv = 1;
	    ret = TRUE;
	    EndDialog (hDlg, LOWORD(wParam));
	    break;
        }
	break;
	

    }
    return (ret);
}

/*
 * Get a button position in the parent's coordinate space.
 */
static void
GetBtnPos (HWND hPrnt, HWND hWnd, RECT *r)
{
    GetWindowRect (hWnd, r);
    ScreenToClient (hPrnt, (POINT *) r);
    ScreenToClient (hPrnt, (POINT *) &r->right);
}
