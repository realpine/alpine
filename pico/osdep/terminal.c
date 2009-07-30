#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: terminal.c 921 2008-01-31 02:09:25Z hubert@u.washington.edu $";
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
 * Program:	Display routines
 */

#include <system.h>
#include <general.h>


#include "../estruct.h"
#include "../keydefs.h"
#include "../pico.h"
#include "../mode.h"

#include "raw.h"
#include "color.h"
#include "tty.h"
#include "terminal.h"
#include "getkey.h"

#ifndef _WINDOWS
extern long gmode;


/*
 * Useful Defaults
 */
#define	MARGIN	8			/* size of minimim margin and	*/
#define	MROW	2			/* rows in menu                 */


/* any special properties of the current terminal */
unsigned term_capabilities = (TT_EOLEXIST | TT_SCROLLEXIST | TT_INSCHAR | TT_DELCHAR);


/*
 *
 */
unsigned
tthascap(void)
{
    return(term_capabilities);
}


#if	HAS_TERMINFO

/*
 * terminfo-based terminal i/o and control routines
 */


/* internal prototypes */
static int      tinfomove(int, int);
static int      tinfoeeol(void);
static int      tinfoeeop(void);
static int      tinfobeep(void);
static int      tinforev(int);
static int      tinfoopen(void);
static int      tinfoterminalinfo(int);
static int      tinfoclose(void);
static void     setup_dflt_esc_seq(void);
static void     tinfoinsert(UCS);
static void     tinfodelete(void);

extern int      tput();
extern int      tputs(char *, int, int (*)(int));
extern char    *tgoto(char *, int, int);
extern char    *tigetstr ();
extern int      setupterm(char *, int, int *);
extern int      tigetnum(char *);
extern int      tigetflag(char *);

/**
 ** Note: The tgoto calls should really be replaced by tparm calls for
 ** modern terminfo. tgoto(s, x, y) == tparm(s, y, x).
 **/

int	_tlines, _tcolumns;
char	*_clearscreen, *_moveto, *_up, *_down, *_right, *_left,
	*_setinverse, *_clearinverse,
	*_setunderline, *_clearunderline,
	*_setbold, *_clearallattr,	/* there is no clear only bold! */
	*_cleartoeoln, *_cleartoeos,
	*_deleteline,		/* delete line */
	*_insertline,		/* insert line */
	*_scrollregion,		/* define a scrolling region, vt100 */
	*_insertchar,		/* insert character, preferable to : */
	*_startinsert,		/* set insert mode and, */
	*_endinsert,		/* end insert mode */
	*_deletechar,		/* delete character */
	*_startdelete,		/* set delete mode and, */
	*_enddelete,		/* end delete mode */
	*_scrolldown,		/* scroll down */
	*_scrollup,		/* scroll up */
	*_termcap_init,		/* string to start termcap */
        *_termcap_end,		/* string to end termcap */
	*_op, *_oc, *_setaf, *_setab, *_setf, *_setb, *_scp;
int	_colors, _pairs, _bce;
char     term_name[40];

TERM term = {
        NROW-1,
        NCOL,
	MARGIN,
	MROW,
        tinfoopen,
        tinfoterminalinfo,
        tinfoclose,
        ttgetc,
        ttputc,
        ttflush,
        tinfomove,
        tinfoeeol,
        tinfoeeop,
        tinfobeep,
        tinforev
};


/*
 * Add default keypad sequences to the trie.
 */
static void
setup_dflt_esc_seq(void)
{
    /*
     * this is sort of a hack [no kidding], but it allows us to use
     * the function keys on pc's running telnet
     */

    /* 
     * UW-NDC/UCS vt10[02] application mode.
     */
    kpinsert("\033OP", F1, 1);
    kpinsert("\033OQ", F2, 1);
    kpinsert("\033OR", F3, 1);
    kpinsert("\033OS", F4, 1);
    kpinsert("\033Op", F5, 1);
    kpinsert("\033Oq", F6, 1);
    kpinsert("\033Or", F7, 1);
    kpinsert("\033Os", F8, 1);
    kpinsert("\033Ot", F9, 1);
    kpinsert("\033Ou", F10, 1);
    kpinsert("\033Ov", F11, 1);
    kpinsert("\033Ow", F12, 1);

    /*
     * DEC vt100, ANSI and cursor key mode.
     */
    kpinsert("\033OA", KEY_UP, 1);
    kpinsert("\033OB", KEY_DOWN, 1);
    kpinsert("\033OC", KEY_RIGHT, 1);
    kpinsert("\033OD", KEY_LEFT, 1);

    /*
     * special keypad functions
     */
    kpinsert("\033[4J", KEY_PGUP, 1);
    kpinsert("\033[3J", KEY_PGDN, 1);
    kpinsert("\033[2J", KEY_HOME, 1);
    kpinsert("\033[N",  KEY_END, 1);

    /*
     * vt220?
     */
    kpinsert("\033[5~", KEY_PGUP, 1);
    kpinsert("\033[6~", KEY_PGDN, 1);
    kpinsert("\033[1~", KEY_HOME, 1);
    kpinsert("\033[4~", KEY_END, 1);

    /*
     * konsole, XTerm (XFree 4.x.x) keyboard setting
     */
    kpinsert("\033[H", KEY_HOME, 1);
    kpinsert("\033[F", KEY_END, 1);

    /* 
     * gnome-terminal 2.6.0, don't know why it
     * changed from 2.2.1
     */
    kpinsert("\033OH", KEY_HOME, 1);
    kpinsert("\033OF", KEY_END, 1);

    /*
     * "\033[2~" was common for KEY_HOME in a quick survey
     *  of terminals (though typically the Insert key).
     *  Teraterm 2.33 sends the following escape sequences,
     *  which is quite incompatible with everything
     *  else:
     *    Home: "\033[2~" End: "\033[5~" PgUp: "\033[3~"
     *    PgDn: "\033[6~"
     *  The best thing to do would be to fix TeraTerm
     *  keymappings or to tweak terminfo.
     */

    /* 
     * ANSI mode.
     */
    kpinsert("\033[=a", F1, 1);
    kpinsert("\033[=b", F2, 1);
    kpinsert("\033[=c", F3, 1);
    kpinsert("\033[=d", F4, 1);
    kpinsert("\033[=e", F5, 1);
    kpinsert("\033[=f", F6, 1);
    kpinsert("\033[=g", F7, 1);
    kpinsert("\033[=h", F8, 1);
    kpinsert("\033[=i", F9, 1);
    kpinsert("\033[=j", F10, 1);
    kpinsert("\033[=k", F11, 1);
    kpinsert("\033[=l", F12, 1);

    /*
     * DEC vt100, ANSI and cursor key mode reset.
     */
    kpinsert("\033[A", KEY_UP, 1);
    kpinsert("\033[B", KEY_DOWN, 1);
    kpinsert("\033[C", KEY_RIGHT, 1);
    kpinsert("\033[D", KEY_LEFT, 1);

    /*
     * DEC vt52 mode.
     */
    kpinsert("\033A", KEY_UP, 1);
    kpinsert("\033B", KEY_DOWN, 1);
    kpinsert("\033C", KEY_RIGHT, 1);
    kpinsert("\033D", KEY_LEFT, 1);

    /*
     * DEC vt52 application keys, and some Zenith 19.
     */
    kpinsert("\033?r", KEY_DOWN, 1);
    kpinsert("\033?t", KEY_LEFT, 1);
    kpinsert("\033?v", KEY_RIGHT, 1);
    kpinsert("\033?x", KEY_UP, 1);

    /*
     * Some Ctrl-Arrow keys
     */
    kpinsert("\033[5A", CTRL_KEY_UP, 1);
    kpinsert("\033[5B", CTRL_KEY_DOWN, 1);
    kpinsert("\033[5C", CTRL_KEY_RIGHT, 1);
    kpinsert("\033[5D", CTRL_KEY_LEFT, 1);

    kpinsert("\033[1;5A", CTRL_KEY_UP, 1);
    kpinsert("\033[1;5B", CTRL_KEY_DOWN, 1);
    kpinsert("\033[1;5C", CTRL_KEY_RIGHT, 1);
    kpinsert("\033[1;5D", CTRL_KEY_LEFT, 1);

    /*
     * Map some shift+up/down/left/right to their shiftless counterparts
     */
    kpinsert("\033[1;2A", KEY_UP, 1);
    kpinsert("\033[1;2B", KEY_DOWN, 1);
    kpinsert("\033[1;2C", KEY_RIGHT, 1);
    kpinsert("\033[1;2D", KEY_LEFT, 1);
    kpinsert("\033[a", KEY_UP, 1);
    kpinsert("\033[b", KEY_DOWN, 1);
    kpinsert("\033[c", KEY_RIGHT, 1);
    kpinsert("\033[d", KEY_LEFT, 1);

    /*
     * Sun Console sequences.
     */
    kpinsert("\033[1",   KEY_SWALLOW_Z, 1);
    kpinsert("\033[215", KEY_SWAL_UP, 1);
    kpinsert("\033[217", KEY_SWAL_LEFT, 1);
    kpinsert("\033[219", KEY_SWAL_RIGHT, 1);
    kpinsert("\033[221", KEY_SWAL_DOWN, 1);

    /*
     * Kermit App Prog Cmd, gobble until ESC \ (kermit should intercept this)
     */
    kpinsert("\033_", KEY_KERMIT, 1);

    /*
     * Fake a control character.
     */
    kpinsert("\033\033", KEY_DOUBLE_ESC, 1);
}


static int
tinfoterminalinfo(int termcap_wins)
{
    char   *_ku, *_kd, *_kl, *_kr,
	   *_kppu, *_kppd, *_kphome, *_kpend, *_kpdel,
	   *_kf1, *_kf2, *_kf3, *_kf4, *_kf5, *_kf6,
	   *_kf7, *_kf8, *_kf9, *_kf10, *_kf11, *_kf12;
    char   *ttnm;

    if (Pmaster) {
	/*
	 *		setupterm() automatically retrieves the value
	 *		of the TERM variable.
	 */
	int err;
	ttnm = getenv("TERM");
	if(!ttnm)
	  return(-1);

	strncpy(term_name, ttnm, sizeof(term_name));
	term_name[sizeof(term_name)-1] = '\0';
	setupterm ((char *) 0, 1, &err);
	if (err != 1) return(err-2);
    }
    else {
	/*
	 *		setupterm() issues a message and exits, if the
	 *		terminfo data base is gone or the term type is
	 *		unknown, if arg2 is 0.
	 */
	setupterm ((char *) 0, 1, (int *) 0);
    }

    _clearscreen	= tigetstr("clear");
    _moveto		= tigetstr("cup");
    _up			= tigetstr("cuu1");
    _down		= tigetstr("cud1");
    _right		= tigetstr("cuf1");
    _left		= tigetstr("cub1");
    _setinverse		= tigetstr("smso");
    _clearinverse	= tigetstr("rmso");
    _setunderline	= tigetstr("smul");
    _clearunderline	= tigetstr("rmul");
    _setbold		= tigetstr("bold");
    _clearallattr	= tigetstr("sgr0");
    _cleartoeoln	= tigetstr("el");
    _cleartoeos		= tigetstr("ed");
    _deletechar		= tigetstr("dch1");
    _insertchar		= tigetstr("ich1");
    _startinsert	= tigetstr("smir");
    _endinsert		= tigetstr("rmir");
    _deleteline		= tigetstr("dl1");
    _insertline		= tigetstr("il1");
    _scrollregion	= tigetstr("csr");
    _scrolldown		= tigetstr("ind");
    _scrollup		= tigetstr("ri");
    _termcap_init	= tigetstr("smcup");
    _termcap_end	= tigetstr("rmcup");
    _startdelete	= tigetstr("smdc");
    _enddelete		= tigetstr("rmdc");
    _ku			= tigetstr("kcuu1");
    _kd			= tigetstr("kcud1");
    _kl			= tigetstr("kcub1");
    _kr			= tigetstr("kcuf1");
    _kppu		= tigetstr("kpp");
    _kppd		= tigetstr("knp");
    _kphome		= tigetstr("khome");
    _kpend		= tigetstr("kend");
    _kpdel		= tigetstr("kdch1");
    _kf1		= tigetstr("kf1");
    _kf2		= tigetstr("kf2");
    _kf3		= tigetstr("kf3");
    _kf4		= tigetstr("kf4");
    _kf5		= tigetstr("kf5");
    _kf6		= tigetstr("kf6");
    _kf7		= tigetstr("kf7");
    _kf8		= tigetstr("kf8");
    _kf9		= tigetstr("kf9");
    _kf10		= tigetstr("kf10");
    _kf11		= tigetstr("kf11");
    _kf12		= tigetstr("kf12");

    _colors		= tigetnum("colors");
    _pairs		= tigetnum("pairs");
    _setaf		= tigetstr("setaf");
    _setab		= tigetstr("setab");
    _setf		= tigetstr("setf");
    _setb		= tigetstr("setb");
    _scp		= tigetstr("scp");
    _op			= tigetstr("op");
    _oc			= tigetstr("oc");
    _bce		= tigetflag("bce");

    _tlines = tigetnum("lines");
    if(_tlines == -1){
	char *er;
	int   rr;

	/* tigetnum failed, try $LINES */
	er = getenv("LINES");
	if(er && (rr = atoi(er)) > 0)
	  _tlines = rr;
    }

    _tcolumns = tigetnum("cols");
    if(_tcolumns == -1){
	char *ec;
	int   cc;

	/* tigetnum failed, try $COLUMNS */
	ec = getenv("COLUMNS");
	if(ec && (cc = atoi(ec)) > 0)
	  _tcolumns = cc;
    }
    
    /*
     * Add default keypad sequences to the trie.
     * Since these come first, they will override any conflicting termcap
     * or terminfo escape sequences defined below.  An escape sequence is
     * considered conflicting if one is a prefix of the other.
     * So, without TERMCAP_WINS, there will likely be some termcap/terminfo
     * escape sequences that don't work, because they conflict with default
     * sequences defined here.
     */
    if(!termcap_wins)
      setup_dflt_esc_seq();

    /*
     * add termcap/info escape sequences to the trie...
     */

    if(_ku != NULL && _kd != NULL && _kl != NULL && _kr != NULL){
	kpinsert(_ku, KEY_UP, termcap_wins);
	kpinsert(_kd, KEY_DOWN, termcap_wins);
	kpinsert(_kl, KEY_LEFT, termcap_wins);
	kpinsert(_kr, KEY_RIGHT, termcap_wins);
    }

    if(_kppu != NULL && _kppd != NULL){
	kpinsert(_kppu, KEY_PGUP, termcap_wins);
	kpinsert(_kppd, KEY_PGDN, termcap_wins);
    }

    kpinsert(_kphome, KEY_HOME, termcap_wins);
    kpinsert(_kpend,  KEY_END, termcap_wins);
    kpinsert(_kpdel,  KEY_DEL, termcap_wins);

    kpinsert(_kf1,  F1, termcap_wins);
    kpinsert(_kf2,  F2, termcap_wins);
    kpinsert(_kf3,  F3, termcap_wins);
    kpinsert(_kf4,  F4, termcap_wins);
    kpinsert(_kf5,  F5, termcap_wins);
    kpinsert(_kf6,  F6, termcap_wins);
    kpinsert(_kf7,  F7, termcap_wins);
    kpinsert(_kf8,  F8, termcap_wins);
    kpinsert(_kf9,  F9, termcap_wins);
    kpinsert(_kf10, F10, termcap_wins);
    kpinsert(_kf11, F11, termcap_wins);
    kpinsert(_kf12, F12, termcap_wins);

    /*
     * Add default keypad sequences to the trie.
     * Since these come after the termcap/terminfo escape sequences above,
     * the termcap/info sequences will override any conflicting default
     * escape sequences defined here.
     * So, with TERMCAP_WINS, some of the default sequences will be missing.
     * This means that you'd better get all of your termcap/terminfo entries
     * correct if you define TERMCAP_WINS.
     */
    if(termcap_wins)
      setup_dflt_esc_seq();

    if(Pmaster)
      return(0);
    else
      return(TRUE);
}

static int
tinfoopen(void)
{
    int     row, col;

    /*
     * determine the terminal's communication speed and decide
     * if we need to do optimization ...
     */
    if(ttisslow())
      term_capabilities |= TT_OPTIMIZE;
    
    col = _tcolumns;
    row = _tlines;
    if(row >= 0)
      row--;

    ttgetwinsz(&row, &col);
    term.t_nrow = (short) row;
    term.t_ncol = (short) col;

    if(_cleartoeoln != NULL)	/* able to use clear to EOL? */
      term_capabilities |= TT_EOLEXIST;
    else
      term_capabilities &= ~TT_EOLEXIST;

    if(_setinverse != NULL)
      term_capabilities |= TT_REVEXIST;
    else
      term_capabilities &= ~TT_REVEXIST;

    if(_deletechar == NULL && (_startdelete == NULL || _enddelete == NULL))
      term_capabilities &= ~TT_DELCHAR;

    if(_insertchar == NULL && (_startinsert == NULL || _endinsert == NULL))
      term_capabilities &= ~TT_INSCHAR;

    if((_scrollregion == NULL || _scrolldown == NULL || _scrollup == NULL)
       && (_deleteline == NULL || _insertline == NULL))
      term_capabilities &= ~TT_SCROLLEXIST;

    if(_clearscreen == NULL || _moveto == NULL || _up == NULL){
	if(Pmaster == NULL){
	    puts("Incomplete terminfo entry\n");
	    exit(1);
	}
    }

    ttopen();

    if(_termcap_init && !Pmaster) {
	putpad(_termcap_init);		/* any init terminfo requires */
	if (_scrollregion)
	  putpad(tgoto(_scrollregion, term.t_nrow, 0)) ;
    }

    /*
     * Initialize UW-modified NCSA telnet to use its functionkeys
     */
    if((gmode & MDFKEY) && Pmaster == NULL)
      puts("\033[99h");

    /* return ignored */
    return(0);
}


static int
tinfoclose(void)
{
    if(!Pmaster){
	if(gmode&MDFKEY)
	  puts("\033[99l");		/* reset UW-NCSA telnet keys */

	if(_termcap_end)		/* any clean up terminfo requires */
	  putpad(_termcap_end);
    }

    ttclose();
    
    /* return ignored */
    return(0);
}


/*
 * tinfoinsert - insert a character at the current character position.
 *               _insertchar takes precedence.
 */
static void
tinfoinsert(UCS ch)
{
    if(_insertchar != NULL){
	putpad(_insertchar);
	ttputc(ch);
    }
    else{
	putpad(_startinsert);
	ttputc(ch);
	putpad(_endinsert);
    }
}


/*
 * tinfodelete - delete a character at the current character position.
 */
static void
tinfodelete(void)
{
    if(_startdelete == NULL && _enddelete == NULL)
      putpad(_deletechar);
    else{
	putpad(_startdelete);
	putpad(_deletechar);
	putpad(_enddelete);
    }
}


/*
 * o_scrolldown() - open a line at the given row position.
 *               use either region scrolling or deleteline/insertline
 *               to open a new line.
 */
int
o_scrolldown(int row, int n)
{
    register int i;

    if(_scrollregion != NULL){
	putpad(tgoto(_scrollregion, term.t_nrow - (term.t_mrow+1), row));
	tinfomove(row, 0);
	for(i = 0; i < n; i++)
	  putpad((_scrollup != NULL && *_scrollup != '\0') ? _scrollup : "\n" );
	putpad(tgoto(_scrollregion, term.t_nrow, 0));
	tinfomove(row, 0);
    }
    else{
	/*
	 * this code causes a jiggly motion of the keymenu when scrolling
	 */
	for(i = 0; i < n; i++){
	    tinfomove(term.t_nrow - (term.t_mrow+1), 0);
	    putpad(_deleteline);
	    tinfomove(row, 0);
	    putpad(_insertline);
	}
#ifdef	NOWIGGLYLINES
	/*
	 * this code causes a sweeping motion up and down the display
	 */
	tinfomove(term.t_nrow - term.t_mrow - n, 0);
	for(i = 0; i < n; i++)
	  putpad(_deleteline);
	tinfomove(row, 0);
	for(i = 0; i < n; i++)
	  putpad(_insertline);
#endif
    }

    /* return ignored */
    return(0);
}


/*
 * o_scrollup() - open a line at the given row position.
 *               use either region scrolling or deleteline/insertline
 *               to open a new line.
 */
int
o_scrollup(int row, int n)
{
    register int i;

    if(_scrollregion != NULL){
	putpad(tgoto(_scrollregion, term.t_nrow - (term.t_mrow+1), row));
	/* setting scrolling region moves cursor to home */
	tinfomove(term.t_nrow-(term.t_mrow+1), 0);
	for(i = 0;i < n; i++)
	  putpad((_scrolldown == NULL || _scrolldown[0] == '\0') ? "\n"
								 : _scrolldown);
	putpad(tgoto(_scrollregion, term.t_nrow, 0));
	tinfomove(2, 0);
    }
    else{
	for(i = 0; i < n; i++){
	    tinfomove(row, 0);
	    putpad(_deleteline);
	    tinfomove(term.t_nrow - (term.t_mrow+1), 0);
	    putpad(_insertline);
	}
#ifdef  NOWIGGLYLINES
	/* see note above */
	tinfomove(row, 0);
	for(i = 0; i < n; i++)
	  putpad(_deleteline);
	tinfomove(term.t_nrow - term.t_mrow - n, 0);
	for(i = 0;i < n; i++)
	  putpad(_insertline);
#endif
    }

    /* return ignored */
    return(0);
}



/*
 * o_insert - use terminfo to optimized character insert
 *            returns: true if it optimized output, false otherwise
 */
int
o_insert(UCS c)
{
    if(term_capabilities & TT_INSCHAR){
	tinfoinsert(c);
	return(1);			/* no problems! */
    }

    return(0);				/* can't do it. */
}


/*
 * o_delete - use terminfo to optimized character insert
 *            returns true if it optimized output, false otherwise
 */
int
o_delete(void)
{
    if(term_capabilities & TT_DELCHAR){
	tinfodelete();
	return(1);			/* deleted, no problem! */
    }

    return(0);				/* no dice. */
}


static int
tinfomove(int row, int col)
{
    putpad(tgoto(_moveto, col, row));

    /* return ignored */
    return(0);
}


static int
tinfoeeol(void)
{
    int   c, starting_col, starting_line;
    char *last_bg_color;

    /*
     * If the terminal doesn't have back color erase, then we have to
     * erase manually to preserve the background color.
     */
    if(pico_usingcolor() && (!_bce || !_cleartoeoln)){
	extern int ttcol, ttrow;

	starting_col  = ttcol;
	starting_line = ttrow;
	last_bg_color = pico_get_last_bg_color();
	pico_set_nbg_color();
	for(c = ttcol; c < term.t_ncol; c++)
	  ttputc(' ');
	
        tinfomove(starting_line, starting_col);
	if(last_bg_color){
	    pico_set_bg_color(last_bg_color);
	    free(last_bg_color);
	}
    }
    else if(_cleartoeoln)
      putpad(_cleartoeoln);

    /* return ignored */
    return(0);
}


static int
tinfoeeop(void)
{
    int i, starting_col, starting_row;

    /*
     * If the terminal doesn't have back color erase, then we have to
     * erase manually to preserve the background color.
     */
    if(pico_usingcolor() && (!_bce || !_cleartoeos)){
	extern int ttcol, ttrow;

	starting_col = ttcol;
	starting_row = ttrow;
	tinfoeeol();				  /* rest of this line */
	for(i = ttrow+1; i <= term.t_nrow; i++){  /* the remaining lines */
	    tinfomove(i, 0);
	    tinfoeeol();
	}

	tinfomove(starting_row, starting_col);
    }
    else if(_cleartoeos)
      putpad(_cleartoeos);

    /* return ignored */
    return(0);
}


static int
tinforev(int state)		/* change reverse video status */
{				/* FALSE = normal video, TRUE = rev video */
    if(state)
      StartInverse();
    else
      EndInverse();

    return(1);
}


static int
tinfobeep(void)
{
    ttputc(BELL);

    /* return ignored */
    return(0);
}


void
putpad(char *str)
{
    tputs(str, 1, putchar);
}

#elif	HAS_TERMCAP

/*
 * termcap-based terminal i/o and control routines
 */


/* internal prototypes */
static int	tcapmove(int, int);
static int      tcapeeol(void);
static int      tcapeeop(void);
static int      tcapbeep(void);
static int      tcaprev(int);
static int      tcapopen(void);
static int      tcapterminalinfo(int);
static int      tcapclose(void);
static void     setup_dflt_esc_seq(void);
static void     tcapinsert(UCS);
static void     tcapdelete(void);

extern int      tput();
extern char     *tgoto(char *, int, int);

/*
 * This number used to be 315. No doubt there was a reason for that but we
 * don't know what it was. It's a bit of a hassle to make it dynamic, and most
 * modern systems seem to be using terminfo, so we'll just change it to 800.
 * We weren't stopping on overflow before, so we'll do that, too. 
 */
#define TCAPSLEN 800
char tcapbuf[TCAPSLEN];
int	_tlines, _tcolumns;
char	*_clearscreen, *_moveto, *_up, *_down, *_right, *_left,
	*_setinverse, *_clearinverse,
	*_setunderline, *_clearunderline,
	*_setbold, *_clearallattr,	/* there is no clear only bold! */
	*_cleartoeoln, *_cleartoeos,
	*_deleteline,		/* delete line */
	*_insertline,		/* insert line */
	*_scrollregion,		/* define a scrolling region, vt100 */
	*_insertchar,		/* insert character, preferable to : */
	*_startinsert,		/* set insert mode and, */
	*_endinsert,		/* end insert mode */
	*_deletechar,		/* delete character */
	*_startdelete,		/* set delete mode and, */
	*_enddelete,		/* end delete mode */
	*_scrolldown,		/* scroll down */
	*_scrollup,		/* scroll up */
	*_termcap_init,		/* string to start termcap */
        *_termcap_end,		/* string to end termcap */
	*_op, *_oc, *_setaf, *_setab, *_setf, *_setb, *_scp;
int	_colors, _pairs, _bce;
char     term_name[40];

TERM term = {
        NROW-1,
        NCOL,
	MARGIN,
	MROW,
        tcapopen,
        tcapterminalinfo,
        tcapclose,
        ttgetc,
        ttputc,
        ttflush,
        tcapmove,
        tcapeeol,
        tcapeeop,
        tcapbeep,
        tcaprev
};


/*
 * Add default keypad sequences to the trie.
 */
static void
setup_dflt_esc_seq(void)
{
    /*
     * this is sort of a hack, but it allows us to use
     * the function keys on pc's running telnet
     */

    /* 
     * UW-NDC/UCS vt10[02] application mode.
     */
    kpinsert("\033OP", F1, 1);
    kpinsert("\033OQ", F2, 1);
    kpinsert("\033OR", F3, 1);
    kpinsert("\033OS", F4, 1);
    kpinsert("\033Op", F5, 1);
    kpinsert("\033Oq", F6, 1);
    kpinsert("\033Or", F7, 1);
    kpinsert("\033Os", F8, 1);
    kpinsert("\033Ot", F9, 1);
    kpinsert("\033Ou", F10, 1);
    kpinsert("\033Ov", F11, 1);
    kpinsert("\033Ow", F12, 1);

    /*
     * DEC vt100, ANSI and cursor key mode.
     */
    kpinsert("\033OA", KEY_UP, 1);
    kpinsert("\033OB", KEY_DOWN, 1);
    kpinsert("\033OC", KEY_RIGHT, 1);
    kpinsert("\033OD", KEY_LEFT, 1);

    /*
     * special keypad functions
     */
    kpinsert("\033[4J", KEY_PGUP, 1);
    kpinsert("\033[3J", KEY_PGDN, 1);
    kpinsert("\033[2J", KEY_HOME, 1);
    kpinsert("\033[N",  KEY_END, 1);

    /*
     * vt220?
     */
    kpinsert("\033[5~", KEY_PGUP, 1);
    kpinsert("\033[6~", KEY_PGDN, 1);
    kpinsert("\033[1~", KEY_HOME, 1);
    kpinsert("\033[4~", KEY_END, 1);

    /*
     * konsole, XTerm (XFree 4.x.x) keyboard setting
     */
    kpinsert("\033[H", KEY_HOME, 1);
    kpinsert("\033[F", KEY_END, 1);

    /* 
     * gnome-terminal 2.6.0, don't know why it
     * changed from 2.2.1
     */
    kpinsert("\033OH", KEY_HOME, 1);
    kpinsert("\033OF", KEY_END, 1);

    /*
     * "\033[2~" was common for KEY_HOME in a quick survey
     *  of terminals (though typically the Insert key).
     *  Teraterm 2.33 sends the following escape sequences,
     *  which is quite incompatible with everything
     *  else:
     *    Home: "\033[2~" End: "\033[5~" PgUp: "\033[3~"
     *    PgDn: "\033[6~"
     *  The best thing to do would be to fix TeraTerm
     *  keymappings or to tweak terminfo.
     */

    /* 
     * ANSI mode.
     */
    kpinsert("\033[=a", F1, 1);
    kpinsert("\033[=b", F2, 1);
    kpinsert("\033[=c", F3, 1);
    kpinsert("\033[=d", F4, 1);
    kpinsert("\033[=e", F5, 1);
    kpinsert("\033[=f", F6, 1);
    kpinsert("\033[=g", F7, 1);
    kpinsert("\033[=h", F8, 1);
    kpinsert("\033[=i", F9, 1);
    kpinsert("\033[=j", F10, 1);
    kpinsert("\033[=k", F11, 1);
    kpinsert("\033[=l", F12, 1);

    /*
     * DEC vt100, ANSI and cursor key mode reset.
     */
    kpinsert("\033[A", KEY_UP, 1);
    kpinsert("\033[B", KEY_DOWN, 1);
    kpinsert("\033[C", KEY_RIGHT, 1);
    kpinsert("\033[D", KEY_LEFT, 1);

    /*
     * DEC vt52 mode.
     */
    kpinsert("\033A", KEY_UP, 1);
    kpinsert("\033B", KEY_DOWN, 1);
    kpinsert("\033C", KEY_RIGHT, 1);
    kpinsert("\033D", KEY_LEFT, 1);

    /*
     * DEC vt52 application keys, and some Zenith 19.
     */
    kpinsert("\033?r", KEY_DOWN, 1);
    kpinsert("\033?t", KEY_LEFT, 1);
    kpinsert("\033?v", KEY_RIGHT, 1);
    kpinsert("\033?x", KEY_UP, 1);

    /*
     * Some Ctrl-Arrow keys
     */
    kpinsert("\033[5A", CTRL_KEY_UP, 1);
    kpinsert("\033[5B", CTRL_KEY_DOWN, 1);
    kpinsert("\033[5C", CTRL_KEY_RIGHT, 1);
    kpinsert("\033[5D", CTRL_KEY_LEFT, 1);

    kpinsert("\033[1;5A", CTRL_KEY_UP, 1);
    kpinsert("\033[1;5B", CTRL_KEY_DOWN, 1);
    kpinsert("\033[1;5C", CTRL_KEY_RIGHT, 1);
    kpinsert("\033[1;5D", CTRL_KEY_LEFT, 1);

    /*
     * Map some shift+up/down/left/right to their shiftless counterparts
     */
    kpinsert("\033[1;2A", KEY_UP, 1);
    kpinsert("\033[1;2B", KEY_DOWN, 1);
    kpinsert("\033[1;2C", KEY_RIGHT, 1);
    kpinsert("\033[1;2D", KEY_LEFT, 1);
    kpinsert("\033[a", KEY_UP, 1);
    kpinsert("\033[b", KEY_DOWN, 1);
    kpinsert("\033[c", KEY_RIGHT, 1);
    kpinsert("\033[d", KEY_LEFT, 1);

    /*
     * Sun Console sequences.
     */
    kpinsert("\033[1",   KEY_SWALLOW_Z, 1);
    kpinsert("\033[215", KEY_SWAL_UP, 1);
    kpinsert("\033[217", KEY_SWAL_LEFT, 1);
    kpinsert("\033[219", KEY_SWAL_RIGHT, 1);
    kpinsert("\033[221", KEY_SWAL_DOWN, 1);

    /*
     * Kermit App Prog Cmd, gobble until ESC \ (kermit should intercept this)
     */
    kpinsert("\033_", KEY_KERMIT, 1);

    /*
     * Fake a control character.
     */
    kpinsert("\033\033", KEY_DOUBLE_ESC, 1);
}


/*
 * Read termcap and set some global variables. Initialize input trie to
 * decode escape sequences.
 */
static int
tcapterminalinfo(int termcap_wins)
{
    char   *p, *tgetstr();
    char    tcbuf[2*1024];
    char   *tv_stype;
    char    err_str[72];
    int     err;
    char   *_ku, *_kd, *_kl, *_kr,
	   *_kppu, *_kppd, *_kphome, *_kpend, *_kpdel,
	   *_kf1, *_kf2, *_kf3, *_kf4, *_kf5, *_kf6,
	   *_kf7, *_kf8, *_kf9, *_kf10, *_kf11, *_kf12;

    if (!(tv_stype = getenv("TERM")) || !strncpy(term_name, tv_stype, sizeof(term_name))){
	if(Pmaster){
	    return(-1);
	}
	else{
	    puts("Environment variable TERM not defined!");
	    exit(1);
	}
    }

    term_name[sizeof(term_name)-1] = '\0';

    if((err = tgetent(tcbuf, tv_stype)) != 1){
	if(Pmaster){
	    return(err - 2);
	}
	else{
	    snprintf(err_str, sizeof(err_str), "Unknown terminal type %s!", tv_stype);
	    puts(err_str);
	    exit(1);
	}
    }

    p = tcapbuf;

    _clearscreen	= tgetstr("cl", &p);
    _moveto		= tgetstr("cm", &p);
    _up			= tgetstr("up", &p);
    _down		= tgetstr("do", &p);
    _right		= tgetstr("nd", &p);
    _left		= tgetstr("bs", &p);
    _setinverse		= tgetstr("so", &p);
    _clearinverse	= tgetstr("se", &p);
    _setunderline	= tgetstr("us", &p);
    _clearunderline	= tgetstr("ue", &p);
    _setbold		= tgetstr("md", &p);
    _clearallattr	= tgetstr("me", &p);
    _cleartoeoln	= tgetstr("ce", &p);
    _cleartoeos		= tgetstr("cd", &p);
    _deletechar		= tgetstr("dc", &p);
    _insertchar		= tgetstr("ic", &p);
    _startinsert	= tgetstr("im", &p);
    _endinsert		= tgetstr("ei", &p);
    _deleteline		= tgetstr("dl", &p);
    _insertline		= tgetstr("al", &p);
    _scrollregion	= tgetstr("cs", &p);
    _scrolldown		= tgetstr("sf", &p);
    _scrollup		= tgetstr("sr", &p);
    _termcap_init	= tgetstr("ti", &p);
    _termcap_end	= tgetstr("te", &p);
    _startdelete	= tgetstr("dm", &p);
    _enddelete		= tgetstr("ed", &p);
    _ku			= tgetstr("ku", &p);
    _kd			= tgetstr("kd", &p);
    _kl			= tgetstr("kl", &p);
    _kr			= tgetstr("kr", &p);
    _kppu		= tgetstr("kP", &p);
    _kppd		= tgetstr("kN", &p);
    _kphome		= tgetstr("kh", &p);
    _kpend		= tgetstr("kH", &p);
    _kpdel		= tgetstr("kD", &p);
    _kf1		= tgetstr("k1", &p);
    _kf2		= tgetstr("k2", &p);
    _kf3		= tgetstr("k3", &p);
    _kf4		= tgetstr("k4", &p);
    _kf5		= tgetstr("k5", &p);
    _kf6		= tgetstr("k6", &p);
    _kf7		= tgetstr("k7", &p);
    _kf8		= tgetstr("k8", &p);
    _kf9		= tgetstr("k9", &p);
    if((_kf10		= tgetstr("k;", &p)) == NULL)
      _kf10		= tgetstr("k0", &p);
    _kf11		= tgetstr("F1", &p);
    _kf12		= tgetstr("F2", &p);

    _colors		= tgetnum("Co");
    _pairs		= tgetnum("pa");
    _setaf		= tgetstr("AF", &p);
    _setab		= tgetstr("AB", &p);
    _setf		= tgetstr("Sf", &p);
    _setb		= tgetstr("Sb", &p);
    _scp		= tgetstr("sp", &p);
    _op			= tgetstr("op", &p);
    _oc			= tgetstr("oc", &p);
    _bce		= tgetflag("ut");

    if (p >= &tcapbuf[TCAPSLEN]){
	puts("Terminal description too big!\n");
	if(Pmaster)
	  return(-3);
	else
	  exit(1);
    }

    _tlines = tgetnum("li");
    if(_tlines == -1){
	char *er;
	int   rr;

	/* tgetnum failed, try $LINES */
	er = getenv("LINES");
	if(er && (rr = atoi(er)) > 0)
	  _tlines = rr;
    }

    _tcolumns = tgetnum("co");
    if(_tcolumns == -1){
	char *ec;
	int   cc;

	/* tgetnum failed, try $COLUMNS */
	ec = getenv("COLUMNS");
	if(ec && (cc = atoi(ec)) > 0)
	  _tcolumns = cc;
    }

    /*
     * Add default keypad sequences to the trie.
     * Since these come first, they will override any conflicting termcap
     * or terminfo escape sequences defined below.  An escape sequence is
     * considered conflicting if one is a prefix of the other.
     * So, without TERMCAP_WINS, there will likely be some termcap/terminfo
     * escape sequences that don't work, because they conflict with default
     * sequences defined here.
     */
    if(!termcap_wins)
      setup_dflt_esc_seq();

    /*
     * add termcap/info escape sequences to the trie...
     */

    if(_ku != NULL && _kd != NULL && _kl != NULL && _kr != NULL){
	kpinsert(_ku, KEY_UP, termcap_wins);
	kpinsert(_kd, KEY_DOWN, termcap_wins);
	kpinsert(_kl, KEY_LEFT, termcap_wins);
	kpinsert(_kr, KEY_RIGHT, termcap_wins);
    }

    if(_kppu != NULL && _kppd != NULL){
	kpinsert(_kppu, KEY_PGUP, termcap_wins);
	kpinsert(_kppd, KEY_PGDN, termcap_wins);
    }

    kpinsert(_kphome, KEY_HOME, termcap_wins);
    kpinsert(_kpend,  KEY_END, termcap_wins);
    kpinsert(_kpdel,  KEY_DEL, termcap_wins);

    kpinsert(_kf1,  F1, termcap_wins);
    kpinsert(_kf2,  F2, termcap_wins);
    kpinsert(_kf3,  F3, termcap_wins);
    kpinsert(_kf4,  F4, termcap_wins);
    kpinsert(_kf5,  F5, termcap_wins);
    kpinsert(_kf6,  F6, termcap_wins);
    kpinsert(_kf7,  F7, termcap_wins);
    kpinsert(_kf8,  F8, termcap_wins);
    kpinsert(_kf9,  F9, termcap_wins);
    kpinsert(_kf10, F10, termcap_wins);
    kpinsert(_kf11, F11, termcap_wins);
    kpinsert(_kf12, F12, termcap_wins);

    /*
     * Add default keypad sequences to the trie.
     * Since these come after the termcap/terminfo escape sequences above,
     * the termcap/info sequences will override any conflicting default
     * escape sequences defined here.
     * So, with TERMCAP_WINS, some of the default sequences will be missing.
     * This means that you'd better get all of your termcap/terminfo entries
     * correct if you define TERMCAP_WINS.
     */
    if(termcap_wins)
      setup_dflt_esc_seq();

    if(Pmaster)
      return(0);
    else
      return(TRUE);
}

static int
tcapopen(void)
{
    int     row, col;

    /*
     * determine the terminal's communication speed and decide
     * if we need to do optimization ...
     */
    if(ttisslow())
      term_capabilities |= TT_OPTIMIZE;

    col = _tcolumns;
    row = _tlines;
    if(row >= 0)
      row--;

    ttgetwinsz(&row, &col);
    term.t_nrow = (short) row;
    term.t_ncol = (short) col;

    if(_cleartoeoln != NULL)	/* able to use clear to EOL? */
      term_capabilities |= TT_EOLEXIST;
    else
      term_capabilities &= ~TT_EOLEXIST;

    if(_setinverse != NULL)
      term_capabilities |= TT_REVEXIST;
    else
      term_capabilities &= ~TT_REVEXIST;

    if(_deletechar == NULL && (_startdelete == NULL || _enddelete == NULL))
      term_capabilities &= ~TT_DELCHAR;

    if(_insertchar == NULL && (_startinsert == NULL || _endinsert == NULL))
      term_capabilities &= ~TT_INSCHAR;

    if((_scrollregion == NULL || _scrolldown == NULL || _scrollup == NULL)
       && (_deleteline == NULL || _insertline == NULL))
      term_capabilities &= ~TT_SCROLLEXIST;

    if(_clearscreen == NULL || _moveto == NULL || _up == NULL){
	if(Pmaster == NULL){
	    puts("Incomplete termcap entry\n");
	    exit(1);
	}
    }

    ttopen();

    if(_termcap_init && !Pmaster) {
	putpad(_termcap_init);		/* any init termcap requires */
	if (_scrollregion)
	  putpad(tgoto(_scrollregion, term.t_nrow, 0)) ;
    }

    /*
     * Initialize UW-modified NCSA telnet to use it's functionkeys
     */
    if(gmode&MDFKEY && Pmaster == NULL)
      puts("\033[99h");
    
    /* return ignored */
    return(0);
}


static int
tcapclose(void)
{
    if(!Pmaster){
	if(gmode&MDFKEY)
	  puts("\033[99l");		/* reset UW-NCSA telnet keys */

	if(_termcap_end)		/* any cleanup termcap requires */
	  putpad(_termcap_end);
    }

    ttclose();
    
    /* return ignored */
    return(0);
}



/*
 * tcapinsert - insert a character at the current character position.
 *              _insertchar takes precedence.
 */
static void
tcapinsert(UCS ch)
{
    if(_insertchar != NULL){
	putpad(_insertchar);
	ttputc(ch);
    }
    else{
	putpad(_startinsert);
	ttputc(ch);
	putpad(_endinsert);
    }
}


/*
 * tcapdelete - delete a character at the current character position.
 */
static void
tcapdelete(void)
{
    if(_startdelete == NULL && _enddelete == NULL)
      putpad(_deletechar);
    else{
	putpad(_startdelete);
	putpad(_deletechar);
	putpad(_enddelete);
    }
}


/*
 * o_scrolldown - open a line at the given row position.
 *                use either region scrolling or deleteline/insertline
 *                to open a new line.
 */
int
o_scrolldown(int row, int n)
{
    register int i;

    if(_scrollregion != NULL){
	putpad(tgoto(_scrollregion, term.t_nrow - (term.t_mrow+1), row));
	tcapmove(row, 0);
	for(i = 0; i < n; i++)
	  putpad( (_scrollup != NULL && *_scrollup != '\0')
	          ? _scrollup : "\n" );
	putpad(tgoto(_scrollregion, term.t_nrow, 0));
	tcapmove(row, 0);
    }
    else{
	/*
	 * this code causes a jiggly motion of the keymenu when scrolling
	 */
	for(i = 0; i < n; i++){
	    tcapmove(term.t_nrow - (term.t_mrow+1), 0);
	    putpad(_deleteline);
	    tcapmove(row, 0);
	    putpad(_insertline);
	}
#ifdef	NOWIGGLYLINES
	/*
	 * this code causes a sweeping motion up and down the display
	 */
	tcapmove(term.t_nrow - term.t_mrow - n, 0);
	for(i = 0; i < n; i++)
	  putpad(_deleteline);
	tcapmove(row, 0);
	for(i = 0; i < n; i++)
	  putpad(_insertline);
#endif
    }
    
    /* return ignored */
    return(0);
}


/*
 * o_scrollup - open a line at the given row position.
 *              use either region scrolling or deleteline/insertline
 *              to open a new line.
 */
int
o_scrollup(int row, int n)
{
    register int i;

    if(_scrollregion != NULL){
	putpad(tgoto(_scrollregion, term.t_nrow - (term.t_mrow+1), row));
	/* setting scrolling region moves cursor to home */
	tcapmove(term.t_nrow-(term.t_mrow+1), 0);
	for(i = 0;i < n; i++)
	  putpad((_scrolldown == NULL || _scrolldown[0] == '\0')
		 ? "\n" : _scrolldown);
	putpad(tgoto(_scrollregion, term.t_nrow, 0));
	tcapmove(2, 0);
    }
    else{
	for(i = 0; i < n; i++){
	    tcapmove(row, 0);
	    putpad(_deleteline);
	    tcapmove(term.t_nrow - (term.t_mrow+1), 0);
	    putpad(_insertline);
	}
#ifdef  NOWIGGLYLINES
	/* see note above */
	tcapmove(row, 0);
	for(i = 0; i < n; i++)
	  putpad(_deleteline);
	tcapmove(term.t_nrow - term.t_mrow - n, 0);
	for(i = 0;i < n; i++)
	  putpad(_insertline);
#endif
    }

    /* return ignored */
    return(0);
}


/*
 * o_insert - use termcap info to optimized character insert
 *            returns: true if it optimized output, false otherwise
 */
int
o_insert(UCS c)
{
    if(term_capabilities & TT_INSCHAR){
	tcapinsert(c);
	return(1);			/* no problems! */
    }

    return(0);				/* can't do it. */
}


/*
 * o_delete - use termcap info to optimized character insert
 *            returns true if it optimized output, false otherwise
 */
int
o_delete(void)
{
    if(term_capabilities & TT_DELCHAR){
	tcapdelete();
	return(1);			/* deleted, no problem! */
    }

    return(0);				/* no dice. */
}


static int
tcapmove(int row, int col)
{
    putpad(tgoto(_moveto, col, row));

    /* return ignored */
    return(0);
}


static int
tcapeeol(void)
{
    int   c, starting_col, starting_line;
    char *last_bg_color;

    /*
     * If the terminal doesn't have back color erase, then we have to
     * erase manually to preserve the background color.
     */
    if(pico_usingcolor() && (!_bce || !_cleartoeoln)){
	extern int ttcol, ttrow;

	starting_col  = ttcol;
	starting_line = ttrow;
	last_bg_color = pico_get_last_bg_color();
	pico_set_nbg_color();
	for(c = ttcol; c < term.t_ncol; c++)
	  ttputc(' ');
	
        tcapmove(starting_line, starting_col);
	if(last_bg_color){
	    pico_set_bg_color(last_bg_color);
	    free(last_bg_color);
	}
    }
    else if(_cleartoeoln)
      putpad(_cleartoeoln);

    /* return ignored */
    return(0);
}


static int
tcapeeop(void)
{
    int i, starting_col, starting_row;

    /*
     * If the terminal doesn't have back color erase, then we have to
     * erase manually to preserve the background color.
     */
    if(pico_usingcolor() && (!_bce || !_cleartoeos)){
	extern int ttcol, ttrow;

	starting_col = ttcol;
	starting_row = ttrow;
	tcapeeol();				  /* rest of this line */
	for(i = ttrow+1; i <= term.t_nrow; i++){  /* the remaining lines */
	    tcapmove(i, 0);
	    tcapeeol();
	}

	tcapmove(starting_row, starting_col);
    }
    else if(_cleartoeos)
      putpad(_cleartoeos);

    /* return ignored */
    return(0);
}


static int
tcaprev(int state)		/* change reverse video status */
{				/* FALSE = normal video, TRUE = reverse video */
    if(state)
      StartInverse();
    else
      EndInverse();

    return(1);
}


static int
tcapbeep(void)
{
    ttputc(BELL);

    /* return ignored */
    return(0);
}


void
putpad(char *str)
{
    tputs(str, 1, putchar);
}

#else  /* HARD_CODED_ANSI_TERMINAL */

/*
 * ANSI-specific terminal i/o and control routines
 */


#define BEL     0x07                    /* BEL character.               */
#define ESC     0x1B                    /* ESC character.               */


extern  int     ttflush();

extern  int     ansimove(int, int);
extern  int     ansieeol(void);
extern  int     ansieeop(void);
extern  int     ansibeep(void);
extern  int     ansiparm(int);
extern  int     ansiopen(void);
extern  int     ansiterminalinfo(int);
extern	int	ansirev(int);

/*
 * Standard terminal interface dispatch table. Most of the fields point into
 * "termio" code.
 */
#if defined(VAX) && !defined(__ALPHA)
globaldef
#endif

TERM    term    = {
        NROW-1,
        NCOL,
	MARGIN,
	MROW,
        ansiopen,
        ansiterminalinfo,
        ttclose,
        ttgetc,
        ttputc,
        ttflush,
        ansimove,
        ansieeol,
        ansieeop,
        ansibeep,
	ansirev
};

int
ansimove(int row, int col)
{
        ttputc(ESC);
        ttputc('[');
        ansiparm(row+1);
        ttputc(';');
        ansiparm(col+1);
        ttputc('H');
}

int
ansieeol(void)
{
        ttputc(ESC);
        ttputc('[');
        ttputc('K');
}

int
ansieeop(void)
{
        ttputc(ESC);
        ttputc('[');
        ttputc('J');
}

int
ansirev(int state)	/* change reverse video state */
{			/* TRUE = reverse, FALSE = normal */
	static int PrevState = 0;

	if(state != PrevState) {
		PrevState = state ;
		ttputc(ESC);
		ttputc('[');
		ttputc(state ? '7': '0');
		ttputc('m');
	}
}

int
ansibeep(void)
{
        ttputc(BEL);
        ttflush();
}

int
ansiparm(int n)
{
        register int    q;

        q = n/10;
        if (q != 0)
                ansiparm(q);
        ttputc((n%10) + '0');
}


int
ansiterminalinfo(int termcap_wins)
{
#if     V7
        register char *cp;
        char *getenv();

        if ((cp = getenv("TERM")) == NULL) {
                puts("Shell variable TERM not defined!");
                exit(1);
        }
        if (strcmp(cp, "vt100") != 0) {
                puts("Terminal type not 'vt100'!");
                exit(1);
        }
#endif
	/* revexist = TRUE; dead code? */
}

int
ansiopen(void)
{
        ttopen();
}


#endif /* HARD_CODED_ANSI_TERMINAL */
#else /* _WINDOWS */

/* These are all just noops in Windows */

unsigned
tthascap(void)
{
    return(0);
}

/*
 * o_insert - optimize screen insert of char c
 */
int
o_insert(UCS c)
{
    return(0);
}


/*
 * o_delete - optimized character deletion
 */
int
o_delete(void)
{
    return(0);
}



#endif /* _WINDOWS */
