#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: tty.c 672 2007-08-15 23:07:18Z hubert@u.washington.edu $";
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
 *
 * Program:	tty routines
 */

#include <system.h>
#include <general.h>

#include "../estruct.h"
#include "../mode.h"
#include "../pico.h"
#include "../edef.h"
#include "../efunc.h"
#include "../keydefs.h"

#include "signals.h"
#ifndef _WINDOWS
#include "terminal.h"
#include "raw.h"
#include "read.h"
#else
#include "mswin.h"
#endif /* _WINDOWS */

#ifdef MOUSE
#include "mouse.h"
#endif /* MOUSE */

#include "tty.h"


#ifndef _WINDOWS
/*
 * ttopen - this function is called once to set up the terminal device 
 *          streams.  if called as pine composer, don't mess with
 *          tty modes, but set signal handlers.
 */
int
ttopen(void)
{
    if(Pmaster == NULL){
	Raw(1);
#ifdef	MOUSE
	if(gmode & MDMOUSE)
	  init_mouse();
#endif	/* MOUSE */
	xonxoff_proc(preserve_start_stop);
    }

    picosigs();

    return(1);
}


/*
 * ttclose - this function gets called just before we go back home to 
 *           the command interpreter.  If called as pine composer, don't
 *           worry about modes, but set signals to default, pine will 
 *           rewire things as needed.
 */
int
ttclose(void)
{
    if(Pmaster){
	signal(SIGHUP, SIG_DFL);
#ifdef  SIGCONT
	signal(SIGCONT, SIG_DFL);
#endif
#if	defined(SIGWINCH) && defined(TIOCGWINSZ)
	signal(SIGWINCH, SIG_DFL);
#endif
    }
    else{
	Raw(0);
#ifdef  MOUSE
	end_mouse();
#endif
    }

    return(1);
}


/*
 * ttgetc - Read a character from the terminal, performing no editing 
 *          and doing no echo at all.
 *
 * Args: return_on_intr     -- Function to get a single character from stdin,
 *             recorder     -- If non-NULL, function used to record keystroke.
 *             bail_handler -- Function used to bail out on read error.
 *
 *  Returns: The character read from stdin.
 *           Return_on_intr is returned if read is interrupted.
 *           If read error, BAIL_OUT is returned unless bail_handler is
 *             non-NULL, in which case it is called (and usually it exits).
 *
 *      If recorder is non-null, it is used to record the keystroke.
 */
int
ttgetc(int return_on_intr, int (*recorder)(int), void (*bail_handler)(void))
{
    int c;

    switch(c = read_one_char()){
      case READ_INTR:
	return(return_on_intr);

      case BAIL_OUT:
	if(bail_handler)
	  (*bail_handler)();
	else
	  return(BAIL_OUT);

      default:
        return(recorder ? (*recorder)(c) : c);
    }
}


/*
 * Simple version of ttgetc with simple error handling
 *
 *      Args:  recorder     -- If non-NULL, function used to record keystroke.
 *             bail_handler -- Function used to bail out on read error.
 *
 *  Returns: The character read from stdin.
 *           If read error, BAIL_OUT is returned unless bail_handler is
 *             non-NULL, in which case it is called (and usually it exits).
 *
 *      If recorder is non-null, it is used to record the keystroke.
 *      Retries if interrupted.
 */
int
simple_ttgetc(int (*recorder)(int), void (*bail_handler)(void))
{
    int		  res;
    unsigned char c;

    while((res = read(STDIN_FD, &c, 1)) <= 0)
      if(!(res < 0 && errno == EINTR))
	(*bail_handler)();

    return(recorder ? (*recorder)((int)c) : (int)c);
}


/*
 * ttputc - Write a character to the display. 
 */
int
ttputc(UCS ucs)
{
    unsigned char obuf[MAX(MB_LEN_MAX,32)];
    int  r, i, width = 0, outchars = 0;
    int  ret = 0;

    if(ucs < 0x80)
      return(putchar((unsigned char) ucs));

    width = wcellwidth(ucs);

    if(width < 0){
	width = 1;
	obuf[outchars++] = '?';
    }
    else{
	/*
	 * Convert the ucs into the multibyte
	 * character that corresponds to the
	 * ucs in the users locale.
	 */
	outchars = wtomb((char *) obuf, ucs);
	if(outchars < 0){
	    width = 1;
	    obuf[0] = '?';
	    outchars = 1;
	}
    }

    for(i = 0; i < outchars; i++){
	r = putchar(obuf[i]);
	ret = (ret == EOF) ? EOF : r;
    }

    return(ret);
}


/*
 * ttflush - flush terminal buffer. Does real work where the terminal 
 *           output is buffered up. A no-operation on systems where byte 
 *           at a time terminal I/O is done.
 */
int
ttflush(void)
{
    return(fflush(stdout));
}


/*
 * ttresize - recompute the screen dimensions if necessary, and then
 *	      adjust pico's internal buffers accordingly.
 */
void
ttresize(void)
{
    int row = -1, col = -1;

    ttgetwinsz(&row, &col);
    resize_pico(row, col);
}



/*
 * ttgetwinsz - set global row and column values (if we can get them)
 *		and return.
 */
void
ttgetwinsz(int *row, int *col)
{
    extern int _tlines, _tcolumns;

    if(*row < 0)
      *row = (_tlines > 0) ? _tlines - 1 : NROW - 1;
    if(*col <= 0)
      *col = (_tcolumns > 0) ? _tcolumns : NCOL;
#if	defined(SIGWINCH) && defined(TIOCGWINSZ)
    {
	struct winsize win;

	if (ioctl(0, TIOCGWINSZ, &win) == 0) {	/* set to anything useful.. */
	    if(win.ws_row)			/* ... the tty drivers says */
	      *row = win.ws_row - 1;

	    if(win.ws_col)
	      *col = win.ws_col;
	}

	signal(SIGWINCH, winch_handler);	/* window size changes */
    }
#endif

    if(*col > NLINE-1)
      *col = NLINE-1;
}

#else /* _WINDOWS */

#define	MARGIN			8	/* size of minimim margin and	*/
#define	SCRSIZ			64	/* scroll size for extended lines */
#define	MROW			2	/* rows in menu */

/* internal prototypes */
int mswin_resize (int, int);

/*
 * Standard terminal interface dispatch table. Fields point to functions
 * that operate the terminal.  All these functions live in mswin.c, but
 * this structure is defined here because it is specific to pico.
 */
TERM    term    = {
        0,
        0,
	MARGIN,
	MROW,
        ttopen,
        NULL,
        ttclose,
        NULL,		/* was mswin_getc, but not used? */
	mswin_putc,
        mswin_flush,
        mswin_move,
        mswin_eeol,
        mswin_eeop,
        mswin_beep,
	mswin_rev
};

/*
 * This function is called once to set up the terminal device streams.
 */
int
ttopen(void)
{
    int rows, columns;

    
    mswin_getscreensize (&rows, &columns);
    term.t_nrow = rows - 1;
    term.t_ncol = columns;
    /* term.t_scrsiz = (columns * 2) / 3; */
    
    /*
     * Do we implement optimized character insertion and deletion?
     * o_insert() and o_delete()
     */
    /* inschar  = delchar = FALSE; */
    /* revexist = TRUE; dead code? */
    
    mswin_setresizecallback (mswin_resize);

    init_mouse();

    return(1);
}

/*
 * This function gets called just before we go back home to the command
 * interpreter.
 */
int
ttclose(void)
{
    mswin_clearresizecallback (mswin_resize);
    return(1);
}

/*
 * Flush terminal buffer. Does real work where the terminal output is buffered
 * up. A no-operation on systems where byte at a time terminal I/O is done.
 */
int
ttflush(void)
{
    return(1);
}

/*
 * ttresize - recompute the screen dimensions if necessary, and then
 *	      adjust pico's internal buffers accordingly.
 */
void
ttresize(void)
{
    int row, col;

    mswin_getscreensize(&row, &col);
    resize_pico (row-1, col);
}


/*
 * mswin_resize - windows specific callback to set pico's internal tables
 *		  to new screen dimensions.
 */
int
mswin_resize(int row, int col)
{
    if (wheadp)
       resize_pico (row-1, col);
    return (0);
}

#endif /* _WINDOWS */
