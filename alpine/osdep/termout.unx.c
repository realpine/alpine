#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: termout.unx.c 955 2008-03-06 23:52:36Z hubert@u.washington.edu $";
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

#include "../../pith/debug.h"
#include "../../pith/conf.h"
#include "../../pith/newmail.h"
#include "../../pith/charconv/utf8.h"

#include "../../pico/estruct.h"
#include "../../pico/pico.h"
#include "../../pico/edef.h"
#include "../../pico/efunc.h"
#include "../../pico/osdep/color.h"

#include "../status.h"
#include "../keymenu.h"
#include "../titlebar.h"

#include "termout.gen.h"
#include "termout.unx.h"


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


This code used to pay attention to the "am" auto margin and "xn"
new line glitch fields, but they were so often incorrect because many
terminals can be configured to do either that we've taken it out. It
now assumes it dosn't know where the cursor is after outputing in the
80th column.
*/


static int   _lines, _columns;


/*
 * Internal prototypes
 */
static int outchar(int);
static void moveabsolute(int, int);
static void CursorUp(int);
static void CursorDown(int);
static void CursorLeft(int);
static void CursorRight(int);


extern char *_clearscreen, *_moveto, *_up, *_down, *_right, *_left,
            *_setinverse, *_clearinverse,
            *_cleartoeoln, *_cleartoeos,
            *_startinsert, *_endinsert, *_insertchar, *_deletechar,
            *_deleteline, *_insertline,
            *_scrollregion, *_scrollup, *_scrolldown,
            *_termcap_init, *_termcap_end;
extern char term_name[];
extern int  _tlines, _tcolumns, _bce;

static enum  {NoScroll,UseScrollRegion,InsertDelete} _scrollmode;

extern int      tputs(char *, int, int (*)(int));
extern char    *tgoto(char *, int, int);



/*----------------------------------------------------------------------
      Initialize the screen for output, set terminal type, etc

   Args: tt -- Pointer to variable to store the tty output structure.

 Result:  terminal size is discovered and set in pine state
          termcap entry is fetched and stored
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
    int          err;

    ttyo = (struct ttyo *)fs_get(sizeof (struct ttyo));

    _line  =  0;		/* where are we right now?? */
    _col   =  0;		/* assume zero, zero...     */

    /*
     * This is an ugly hack to let vtterminalinfo know it's being called
     * from pine.
     */
    Pmaster = (PICO *)1;
    if((err = vtterminalinfo(F_ON(F_TCAP_WINS, ps_global))) != 0)
      return(err);

    Pmaster = NULL;

    if(_tlines <= 0)
      _lines = DEFAULT_LINES_ON_TERMINAL;
    else
      _lines = _tlines;

    if(_tcolumns <= 0)
      _columns = DEFAULT_COLUMNS_ON_TERMINAL;
    else
      _columns = _tcolumns;

    get_windsize(ttyo);

    ttyo->header_rows = 2;
    ttyo->footer_rows = 3;

    /*---- Make sure this terminal has the capability.
        All we need is cursor address, clear line, and 
        reverse video.
      ---*/
    if(_moveto == NULL || _cleartoeoln == NULL ||
       _setinverse == NULL || _clearinverse == NULL) {
          return(-4);
    }

    dprint((1, "Terminal type: %s\n", term_name ? term_name : "?"));

    /*------ Figure out scrolling mode -----*/
    if(_scrollregion != NULL && _scrollregion[0] != '\0' &&
    	  _scrollup != NULL && _scrollup[0] != '\0'){
        _scrollmode = UseScrollRegion;
    } else if(_insertline != NULL && _insertline[0] != '\0' &&
       _deleteline != NULL && _deleteline[0] != '\0') {
        _scrollmode = InsertDelete;
    } else {
        _scrollmode = NoScroll;
    }
    dprint((7, "Scroll mode: %s\n",
               _scrollmode==NoScroll ? "No Scroll" :
               _scrollmode==InsertDelete ? "InsertDelete" : "Scroll Regions"));

    if (!_left) {
    	_left = "\b";
    }

    *tt = ttyo;

    return(0);
}



/*----------------------------------------------------------------------
   Initialize the screen with the termcap string 
  ----*/
void
init_screen(void)
{
    if(_termcap_init)			/* init using termcap's rule */
      tputs(_termcap_init, 1, outchar);

    /* and make sure there are no scrolling surprises! */
    BeginScroll(0, ps_global->ttyo->screen_rows - 1);

    pico_toggle_color(0);
    switch(ps_global->color_style){
      case COL_NONE:
      case COL_TERMDEF:
	pico_set_color_options(pico_trans_color() ? COLOR_TRANS_OPT : 0);
	break;
      case COL_ANSI8:
	pico_set_color_options(COLOR_ANSI8_OPT|COLOR_TRANS_OPT);
	break;
      case COL_ANSI16:
	pico_set_color_options(COLOR_ANSI16_OPT|COLOR_TRANS_OPT);
	break;
      case COL_ANSI256:
	pico_set_color_options(COLOR_ANSI256_OPT|COLOR_TRANS_OPT);
	break;
    }

    if(ps_global->color_style != COL_NONE)
      pico_toggle_color(1);

    /* set colors */
    if(pico_usingcolor()){
	if(ps_global->VAR_NORM_FORE_COLOR)
	  pico_nfcolor(ps_global->VAR_NORM_FORE_COLOR);

	if(ps_global->VAR_NORM_BACK_COLOR)
	  pico_nbcolor(ps_global->VAR_NORM_BACK_COLOR);

	if(ps_global->VAR_REV_FORE_COLOR)
	  pico_rfcolor(ps_global->VAR_REV_FORE_COLOR);

	if(ps_global->VAR_REV_BACK_COLOR)
	  pico_rbcolor(ps_global->VAR_REV_BACK_COLOR);

	pico_set_normal_color();
    }

    /* and make sure icon text starts out consistent */
    icon_text(NULL, IT_NEWMAIL);
    fflush(stdout);
}
        



/*----------------------------------------------------------------------
       Get the current window size
  
   Args: ttyo -- pointer to structure to store window size in

  NOTE: we don't override the given values unless we know better
 ----*/
int
get_windsize(struct ttyo *ttyo)
{
#if	defined(SIGWINCH) && defined(TIOCGWINSZ)
    struct winsize win;

    /*
     * Get the window size from the tty driver.  If we can't fish it from
     * stdout (pine's output is directed someplace else), try stdin (which
     * *must* be associated with the terminal; see init_tty_driver)...
     */
    if(ioctl(1, TIOCGWINSZ, &win) >= 0			/* 1 is stdout */
	|| ioctl(0, TIOCGWINSZ, &win) >= 0){		/* 0 is stdin */
	if(win.ws_row)
	  _lines = MIN(win.ws_row, MAX_SCREEN_ROWS);

	if(win.ws_col)
	  _columns  = MIN(win.ws_col, MAX_SCREEN_COLS);

        dprint((2, "new win size -----<%d %d>------\n",
                   _lines, _columns));
    }
    else{
      /* Depending on the OS, the ioctl() may have failed because
	 of a 0 rows, 0 columns setting.  That happens on DYNIX/ptx 1.3
	 (with a kernel patch that happens to involve the negotiation
	 of window size in the telnet streams module.)  In this case
	 the error is EINVARG.  Leave the default settings. */
      dprint((1, "ioctl(TIOCWINSZ) failed :%s\n",
		 error_description(errno)));
    }
#endif

    ttyo->screen_cols = MIN(_columns, MAX_SCREEN_COLS);
    ttyo->screen_rows = MIN(_lines, MAX_SCREEN_ROWS);
    return(0);
}


/*----------------------------------------------------------------------
      End use of the screen.
      Print status message, if any.
      Flush status messages.
  ----*/
void
end_screen(char *message, int exit_val)
{
    int footer_rows_was_one = 0;

    if(!panicking()){

	dprint((9, "end_screen called\n"));

	if(FOOTER_ROWS(ps_global) == 1){
	    footer_rows_was_one++;
	    FOOTER_ROWS(ps_global) = 3;
	    mark_status_unknown();
	}

	flush_status_messages(exit_val ? 0 : 1);
	blank_keymenu(_lines - 2, 0);
	MoveCursor(_lines - 2, 0);
    }

    /* unset colors */
    if(pico_usingcolor())
      pico_endcolor();

    if(_termcap_end != NULL)
      tputs(_termcap_end, 1, outchar);

    if(!panicking()){

	if(message){
	    printf("%s\r\n", message);
	}

	if(F_ON(F_ENABLE_XTERM_NEWMAIL, ps_global) && getenv("DISPLAY"))
	  icon_text("xterm", IT_NEWMAIL);

	fflush(stdout);

	if(footer_rows_was_one){
	    FOOTER_ROWS(ps_global) = 1;
	    mark_status_unknown();
	}
    }
}
    


/*----------------------------------------------------------------------
     Clear the terminal screen

 Result: The screen is cleared
         internal cursor position set to 0,0
  ----*/
void
ClearScreen(void)
{
    _line = 0;	/* clear leaves us at top... */
    _col  = 0;

    if(ps_global->in_init_seq)
      return;

    mark_status_unknown();
    mark_keymenu_dirty();
    mark_titlebar_dirty();

    /*
     * If the terminal doesn't have back color erase, then we have to
     * erase manually to preserve the background color.
     */
    if(pico_usingcolor() && (!_bce || !_clearscreen)){
	ClearLines(0, _lines-1);
        MoveCursor(0, 0);
    }
    else if(_clearscreen){
	tputs(_clearscreen, 1, outchar);
        moveabsolute(0, 0);	/* some clearscreens don't move correctly */
    }
}


/*----------------------------------------------------------------------
            Internal move cursor to absolute position

  Args: col -- column to move cursor to
        row -- row to move cursor to

 Result: cursor is moved (variables, not updates)
  ----*/

static void
moveabsolute(int col, int row)
{

	char *stuff, *tgoto();

	stuff = tgoto(_moveto, col, row);
	tputs(stuff, 1, outchar);
}


/*----------------------------------------------------------------------
        Move the cursor to the row and column number
  Args:  row number
         column number

 Result: Cursor moves
         internal position updated
  ----*/
void
MoveCursor(int row, int col)
{
    /** move cursor to the specified row column on the screen.
        0,0 is the top left! **/

    int scrollafter = 0;

    /* we don't want to change "rows" or we'll mangle scrolling... */

    if(ps_global->in_init_seq)
      return;

    if (col < 0)
      col = 0;
    if (col >= ps_global->ttyo->screen_cols)
      col = ps_global->ttyo->screen_cols - 1;
    if (row < 0)
      row = 0;
    if (row > ps_global->ttyo->screen_rows) {
      if (col == 0)
        scrollafter = row - ps_global->ttyo->screen_rows;
      row = ps_global->ttyo->screen_rows;
    }

    if (!_moveto)
    	return;

    if (_col >= ps_global->ttyo->screen_cols)
      _col = ps_global->ttyo->screen_cols - 1;

    if (row == _line) {
      if (col == _col)
        return;				/* already there! */

      else if (abs(col - _col) < 5) {	/* within 5 spaces... */
        if (col > _col && _right)
          CursorRight(col - _col);
        else if (col < _col &&  _left)
          CursorLeft(_col - col);
        else
          moveabsolute(col, row);
      }
      else 		/* move along to the new x,y loc */
        moveabsolute(col, row);
    }
    else if (col == _col && abs(row - _line) < 5) {
      if (row < _line && _up)
        CursorUp(_line - row);
      else if (_line > row && _down)
        CursorDown(row - _line);
      else
        moveabsolute(col, row);
    }
    else if (_line == row-1 && col == 0) {
      putchar('\n');	/* that's */
      putchar('\r');	/*  easy! */
    }
    else 
      moveabsolute(col, row);

    _line = row;	/* to ensure we're really there... */
    _col  = col;

    if (scrollafter) {
      while (scrollafter--) {
        putchar('\n');
        putchar('\r');

      }
    }

    return;
}



/*----------------------------------------------------------------------
         Newline, move the cursor to the start of next line

 Result: Cursor moves
  ----*/
void
NewLine(void)
{
   /** move the cursor to the beginning of the next line **/

    Writechar('\n', 0);
    Writechar('\r', 0);
}



/*----------------------------------------------------------------------
        Move cursor up n lines with terminal escape sequence
 
   Args:  n -- number of lines to go up

 Result: cursor moves, 
         internal position updated

 Only for ttyout use; not outside callers
  ----*/
static void
CursorUp(int n)
{
	/** move the cursor up 'n' lines **/
	/** Calling function must check that _up is not null before calling **/

    _line = (_line-n > 0? _line - n: 0);	/* up 'n' lines... */

    while (n-- > 0)
      tputs(_up, 1, outchar);
}



/*----------------------------------------------------------------------
        Move cursor down n lines with terminal escape sequence
 
    Arg: n -- number of lines to go down

 Result: cursor moves, 
         internal position updated

 Only for ttyout use; not outside callers
  ----*/
static void
CursorDown(int n)
{
    /** move the cursor down 'n' lines **/
    /** Caller must check that _down is not null before calling **/

    _line = (_line+n < ps_global->ttyo->screen_rows ? _line + n
             : ps_global->ttyo->screen_rows);
                                               /* down 'n' lines... */

    while (n-- > 0)
    	tputs(_down, 1, outchar);
}



/*----------------------------------------------------------------------
        Move cursor left n lines with terminal escape sequence
 
   Args:  n -- number of lines to go left

 Result: cursor moves, 
         internal position updated

 Only for ttyout use; not outside callers
  ----*/
static void 
CursorLeft(int n)
{
    /** move the cursor 'n' characters to the left **/
    /** Caller must check that _left is not null before calling **/

    _col = (_col - n> 0? _col - n: 0);	/* left 'n' chars... */

    while (n-- > 0)
      tputs(_left, 1, outchar);
}


/*----------------------------------------------------------------------
        Move cursor right n lines with terminal escape sequence
 
   Args:  number of lines to go right

 Result: cursor moves, 
         internal position updated

 Only for ttyout use; not outside callers
  ----*/
static void 
CursorRight(int n)
{
    /** move the cursor 'n' characters to the right (nondestructive) **/
    /** Caller must check that _right is not null before calling **/

    _col = (_col+n < ps_global->ttyo->screen_cols? _col + n :
             ps_global->ttyo->screen_cols);	/* right 'n' chars... */

    while (n-- > 0)
      tputs(_right, 1, outchar);

}


/*----------------------------------------------------------------------
  Go into scrolling mode, that is set scrolling region if applicable

   Args: top    -- top line of region to scroll
         bottom -- bottom line of region to scroll
	 (These are zero-origin numbers)

 Result: either set scrolling region or
         save values for later scrolling
         returns -1 if we can't scroll

 Unfortunately this seems to leave the cursor in an unpredictable place
 at least the manuals don't say where, so we force it here.
-----*/
static int __t, __b;

int
BeginScroll(int top, int bottom)
{
    char *stuff;

    if(_scrollmode == NoScroll)
      return(-1);

    __t = top;
    __b = bottom;
    if(_scrollmode == UseScrollRegion){
        stuff = tgoto(_scrollregion, bottom, top);
        tputs(stuff, 1, outchar);
        /*-- a location  very far away to force a cursor address --*/
        _line = FARAWAY;
        _col  = FARAWAY;
    }
    return(0);
}



/*----------------------------------------------------------------------
   End scrolling -- clear scrolling regions if necessary

 Result: Clear scrolling region on terminal
  -----*/
void
EndScroll(void)
{
    if(_scrollmode == UseScrollRegion && _scrollregion != NULL){
	/* Use tgoto even though we're not cursor addressing because
           the format of the capability is the same.
         */
        char *stuff = tgoto(_scrollregion, ps_global->ttyo->screen_rows -1, 0);
	tputs(stuff, 1, outchar);
        /*-- a location  very far away to force a cursor address --*/
        _line = FARAWAY;
        _col  = FARAWAY;
    }
}


/* ----------------------------------------------------------------------
    Scroll the screen using insert/delete or scrolling regions

   Args:  lines -- number of lines to scroll, positive forward

 Result: Screen scrolls
         returns 0 if scroll succesful, -1 if not

 positive lines goes foward (new lines come in at bottom
 Leaves cursor at the place to insert put new text

 0,0 is the upper left
 -----*/
int
ScrollRegion(int lines)
{
    int l;

    if(lines == 0)
      return(0);

    if(_scrollmode == UseScrollRegion) {
	if(lines > 0) {
	    MoveCursor(__b, 0);
	    for(l = lines ; l > 0 ; l--)
	      tputs((_scrolldown == NULL || _scrolldown[0] =='\0') ? "\n" :
		    _scrolldown, 1, outchar);
	} else {
	    MoveCursor(__t, 0);
	    for(l = -lines; l > 0; l--)
	      tputs(_scrollup, 1, outchar);
	}
    } else if(_scrollmode == InsertDelete) {
	if(lines > 0) {
	    MoveCursor(__t, 0);
	    for(l = lines; l > 0; l--) 
	      tputs(_deleteline, 1, outchar);
	    MoveCursor(__b, 0);
	    for(l = lines; l > 0; l--) 
	      tputs(_insertline, 1, outchar);
	} else {
	    for(l = -lines; l > 0; l--) {
	        MoveCursor(__b, 0);
	        tputs(_deleteline, 1, outchar);
		MoveCursor(__t, 0);
		tputs(_insertline, 1, outchar);
	    }
	}
    } else {
	return(-1);
    }
    fflush(stdout);
    return(0);
}


void
Writewchar(UCS ucs)
{
    if(ps_global->in_init_seq				/* silent */
       || (F_ON(F_BLANK_KEYMENU, ps_global)		/* or bottom, */
	         					/* right cell */
	   && _line + 1 == ps_global->ttyo->screen_rows
	   && _col + 1 == ps_global->ttyo->screen_cols))
      return;


    if(ucs == LINE_FEED || ucs == RETURN || ucs == BACKSPACE || ucs == BELL || ucs == TAB){
	switch(ucs){
	  case LINE_FEED:
	    /*-- Don't have to watch out for auto wrap or newline glitch
	      because we never let it happen. See below
	      ---*/
	    putchar('\n');
	    _line = MIN(_line+1,ps_global->ttyo->screen_rows);
	    break;

	  case RETURN :		/* move to column 0 */
	    putchar('\r');
	    _col = 0;
	    break;

	  case BACKSPACE :	/* move back a space if not in column 0 */
	    if(_col != 0) {
		putchar('\b');
		_col--;
	    }			/* else BACKSPACE does nothing */

	    break;

	  case BELL :		/* ring the bell but don't advance _col */
	    putchar((int) ucs);
	    break;

	  case TAB :		/* if a tab, output it */
	    do			/* BUG? ignores tty driver's spacing */
	      putchar(' ');
	    /* fix from Eduardo Chappa, have to increment _col once more */
	    while(_col < ps_global->ttyo->screen_cols
		  && ((++_col)&0x07) != 0);
	    break;
	}
    }
    else{
	unsigned char obuf[MAX(MB_LEN_MAX,32)];
	int  i, width = 0, outchars = 0, printable_ascii = 0;

	if(ucs < 0x80 && isprint((unsigned char) ucs)){
	    printable_ascii++;		/* efficiency shortcut */
	    width = 1;
	}
	else
	  width = wcellwidth(ucs);

	if(width < 0){
	    /*
	     * This happens when we have a Unicode character that
	     * we aren't able to print in our locale. For example,
	     * if the locale is setup with the terminal
	     * expecting ISO-8859-1 characters then there are
	     * lots of Unicode characters that can't be printed.
	     * Print a '?' instead.
	     */
	    width = 1;
	    obuf[outchars++] = '?';
	}
	else if(_col + width > ps_global->ttyo->screen_cols){
	    /*
	     * Hopefully this won't happen. The
	     * character overflows the right edge because it
	     * is a double wide and we're in last column.
	     * Fill with spaces instead and toss the
	     * character.
	     *
	     * Put a > in the right column to show we tried to write
	     * past the end.
	     */
	    while(outchars < ps_global->ttyo->screen_cols - _col - 1)
	      obuf[outchars++] = ' ';

	    obuf[outchars++] = '>';

	    /*
	     * In case last time through wrote in the last column and
	     * caused an autowrap, reposition cursor.
	     */
	    if(_col >= ps_global->ttyo->screen_cols)
	      moveabsolute(ps_global->ttyo->screen_cols-1, _line);
	}
	else{
	    /*
	     * Convert the ucs into the multibyte
	     * character that corresponds to the
	     * ucs in the users locale.
	     */
	    if(printable_ascii)
	      obuf[outchars++] = (unsigned char) ucs;
	    else{
		outchars = wtomb((char *) obuf, ucs);
		if(outchars < 0){
		    width = 1;
		    obuf[0] = '?';
		    outchars = 1;
		}
	    }
	}

	_col += width;
	for(i = 0; i < outchars; i++)
	  putchar(obuf[i]);
    }

    /*
     * We used to wrap by moving to the next line and making _col = 0
     * when we went past the end. We don't believe that this is useful
     * anymore. If we wrap it is unintentional. Instead, stay at the
     * end of the line. We need to be a little careful because we're
     * moving to a different place than _col is set to, but since _col
     * is off the right edge, it should be ok.
     */
    if(_col >= ps_global->ttyo->screen_cols) {
	_col = ps_global->ttyo->screen_cols;
	moveabsolute(ps_global->ttyo->screen_cols-1, _line);
    }
}


void
Write_to_screen(char *string)				/* UNIX */
                             
{
    while(*string)
      Writechar((unsigned char) *string++, 0);
}


/*----------------------------------------------------------------------
       Write no more than n chars of string to screen at current cursor position

   Args: string -- string to be output
	      n -- number of chars to output

 Result: String written to the screen
  ----*/
void
Write_to_screen_n(char *string, int n)				/* UNIX */
                             
                       
{
    while(n-- && *string)
      Writechar((unsigned char) *string++, 0);
}


/*----------------------------------------------------------------------
    Clear screen to end of line on current line

 Result: Line is cleared
  ----*/
void
CleartoEOLN(void)
{
    int   c, starting_col, starting_line;
    char *last_bg_color;

    /*
     * If the terminal doesn't have back color erase, then we have to
     * erase manually to preserve the background color.
     */
    if(pico_usingcolor() && (!_bce || !_cleartoeoln)){
	starting_col  = _col;
	starting_line = _line;
	last_bg_color = pico_get_last_bg_color();
	pico_set_nbg_color();
	for(c = _col; c < _columns; c++)
	  Writechar(' ', 0);
	
        MoveCursor(starting_line, starting_col);
	if(last_bg_color){
	    (void)pico_set_bg_color(last_bg_color);
	    fs_give((void **)&last_bg_color);
	}
    }
    else if(_cleartoeoln)
      tputs(_cleartoeoln, 1, outchar);
}



/*----------------------------------------------------------------------
     Clear screen to end of screen from current point

 Result: screen is cleared
  ----*/
void
CleartoEOS(void)
{
    int starting_col, starting_line;

    /*
     * If the terminal doesn't have back color erase, then we have to
     * erase manually to preserve the background color.
     */
    if(pico_usingcolor() && (!_bce || !_cleartoeos)){
	starting_col  = _col;
	starting_line = _line;
        CleartoEOLN();
	ClearLines(_line+1, _lines-1);
        MoveCursor(starting_line, starting_col);
    }
    else if(_cleartoeos)
      tputs(_cleartoeos, 1, outchar);
}



/*----------------------------------------------------------------------
     function to output character used by termcap

   Args: c -- character to output

 Result: character output to screen via stdio
  ----*/
int
outchar(int c)
{
	/** output the given character.  From tputs... **/
	/** Note: this CANNOT be a macro!              **/

	return(putc((unsigned char)c, stdout));
}



/*----------------------------------------------------------------------
     function to output string such that it becomes icon text

   Args: s -- string to write

 Result: string indicated become our "icon" text
  ----*/
void
icon_text(char *s, int type)
{
    static enum {ukn, yes, no} xterm;
    char *str, *converted, *use_this;

    if(xterm == ukn)
      xterm = (getenv("DISPLAY") != NULL) ? yes : no;

    if(F_ON(F_ENABLE_XTERM_NEWMAIL,ps_global) && xterm == yes){
	fputs("\033]0;", stdout);

	str = s ? s : ps_global->pine_name;
	converted = convert_to_locale(str);
	use_this = converted ? converted : str;

	fputs(use_this, stdout);

	fputs("\007", stdout);
	fputs("\033]1;", stdout);

	fputs(use_this, stdout);

	fputs("\007", stdout);
	fflush(stdout);

	if(converted)
	  fs_give((void **) &converted);
    }
}


