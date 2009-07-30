#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: raw.c 761 2007-10-23 22:35:18Z hubert@u.washington.edu $";
#endif

/*
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
 */

/*
 * TTY setup routines. 
 */

#include <system.h>
#include <general.h>

#include "../estruct.h"

#include "raw.h"


#if	HAS_TERMIOS
/*
 * These are the TERMIOS-style (POSIX) routines.
 */

static struct termios _raw_tty, _original_tty;

#elif	HAS_TERMIO
/*
 * These are the TERMIO-style (System V) routines.
 */

static struct termio _raw_tty, _original_tty;

#else	/* RAW_BSD */
/*
 * These are for the BSD-style routines.
 */

static struct sgttyb  _raw_tty,     _original_tty;
static struct ltchars _raw_ltchars, _original_ltchars;
static struct tchars  _raw_tchars,  _original_tchars;
static int            _raw_lmode,   _original_lmode;

#endif

/*
 * current raw state
 */
static short _inraw = 0;

/*
 *  Set up the tty driver
 *
 * Args: state -- which state to put it in. 1 means go into raw (cbreak),
 *                0 out of raw.
 *
 * Result: returns 0 if successful and -1 if not.
 */
int
Raw(int state)
{
    /** state is either ON or OFF, as indicated by call **/
    /* Check return code only on first call. If it fails we're done for and
       if it goes OK the others will probably go OK too. */

    if(state == 0 && _inraw){
        /*----- restore state to original -----*/

#if	HAS_TERMIOS

	if(tcsetattr(STDIN_FD, TCSADRAIN, &_original_tty) < 0)
	  return -1;

#elif	HAS_TERMIO

        if(ioctl(STDIN_FD, TCSETAW, &_original_tty) < 0)
          return(-1);

#else	/* RAW_BSD */

	if(ioctl(STDIN_FD, TIOCSETP, &_original_tty) < 0)
          return(-1);

	(void)ioctl(STDIN_FD, TIOCSLTC, &_original_ltchars);
	(void)ioctl(STDIN_FD, TIOCSETC, &_original_tchars);
        (void)ioctl(STDIN_FD, TIOCLSET, &_original_lmode);

#endif

        _inraw = 0;
    }
    else if(state == 1 && ! _inraw){
        /*----- Go into raw mode (cbreak actually) ----*/

#if	HAS_TERMIOS

	if(tcgetattr(STDIN_FD, &_original_tty) < 0)
	  return -1;

	tcgetattr(STDIN_FD, &_raw_tty);
	_raw_tty.c_lflag &= ~(ICANON | ECHO | IEXTEN); /* noecho raw mode    */
 	_raw_tty.c_lflag &= ~ISIG;		/* disable signals           */
 	_raw_tty.c_iflag &= ~ICRNL;		/* turn off CR->NL on input  */
 	_raw_tty.c_oflag &= ~ONLCR;		/* turn off NL->CR on output */

	_raw_tty.c_cc[VMIN]  = '\01';		/* min # of chars to queue  */
	_raw_tty.c_cc[VTIME] = '\0';		/* min time to wait for input*/
	_raw_tty.c_cc[VINTR] = ctrl('C');	/* make it our special char */
	_raw_tty.c_cc[VQUIT] = 0;
	_raw_tty.c_cc[VSUSP] = 0;
	tcsetattr(STDIN_FD, TCSADRAIN, &_raw_tty);

#elif	HAS_TERMIO

        if(ioctl(STDIN_FD, TCGETA, &_original_tty) < 0)
          return(-1);

	(void)ioctl(STDIN_FD, TCGETA, &_raw_tty);    /** again! **/
	_raw_tty.c_lflag &= ~(ICANON | ECHO);	/* noecho raw mode  */
 	_raw_tty.c_lflag &= ~ISIG;		/* disable signals */
 	_raw_tty.c_iflag &= ~ICRNL;		/* turn off CR->NL on input  */
 	_raw_tty.c_oflag &= ~ONLCR;		/* turn off NL->CR on output */
	_raw_tty.c_cc[VMIN]  = 1;		/* min # of chars to queue   */
	_raw_tty.c_cc[VTIME] = 0;		/* min time to wait for input*/
	_raw_tty.c_cc[VINTR] = ctrl('C');	/* make it our special char */
	_raw_tty.c_cc[VQUIT] = 0;
	(void)ioctl(STDIN_FD, TCSETAW, &_raw_tty);

#else	/* RAW_BSD */
        if(ioctl(STDIN_FD, TIOCGETP, &_original_tty) < 0)
          return(-1);

	(void)ioctl(STDIN_FD, TIOCGETP, &_raw_tty);   
        (void)ioctl(STDIN_FD, TIOCGETC, &_original_tchars);
	(void)ioctl(STDIN_FD, TIOCGETC, &_raw_tchars);
	(void)ioctl(STDIN_FD, TIOCGLTC, &_original_ltchars);
	(void)ioctl(STDIN_FD, TIOCGLTC, &_raw_ltchars);
        (void)ioctl(STDIN_FD, TIOCLGET, &_original_lmode);
        (void)ioctl(STDIN_FD, TIOCLGET, &_raw_lmode);

	_raw_tty.sg_flags &= ~(ECHO);	/* echo off */
	_raw_tty.sg_flags |= CBREAK;	/* raw on    */
        _raw_tty.sg_flags &= ~CRMOD;	/* Turn off CR -> LF mapping */

	_raw_tchars.t_intrc = -1;	/* Turn off ^C and ^D */
	_raw_tchars.t_eofc  = -1;

	_raw_ltchars.t_lnextc = -1;	/* Turn off ^V so we can use it */
	_raw_ltchars.t_dsuspc = -1;	/* Turn off ^Y so we can use it */
	_raw_ltchars.t_suspc  = -1;	/* Turn off ^Z; we just read 'em */
	_raw_ltchars.t_werasc = -1;	/* Turn off ^w word erase */
	_raw_ltchars.t_rprntc = -1;	/* Turn off ^R reprint line */
        _raw_ltchars.t_flushc = -1;	/* Turn off ^O output flush */
            
	(void)ioctl(STDIN_FD, TIOCSETP, &_raw_tty);
	(void)ioctl(STDIN_FD, TIOCSLTC, &_raw_ltchars);
        (void)ioctl(STDIN_FD, TIOCSETC, &_raw_tchars);
        (void)ioctl(STDIN_FD, TIOCLSET, &_raw_lmode);
#endif
	_inraw = 1;
    }

    return(0);
}


/*
 *  Set up the tty driver to use XON/XOFF flow control
 *
 * Args: state -- True to make sure XON/XOFF turned on, FALSE off.
 *
 * Result: none.
 */
void
xonxoff_proc(int state)
{
    if(_inraw){
	if(state){
#if	HAS_TERMIOS

	    if(!(_raw_tty.c_iflag & IXON)){
		_raw_tty.c_iflag |= IXON;
		tcsetattr (STDIN_FD, TCSADRAIN, &_raw_tty);
	    }

#elif	HAS_TERMIO

	    if(!(_raw_tty.c_iflag & IXON)){
		_raw_tty.c_iflag |= IXON;	/* turn ON ^S/^Q on input   */
		(void)ioctl(STDIN_FD, TCSETAW, &_raw_tty);

#else	/* RAW_BSD */

	    if(_raw_tchars.t_startc == -1 || _raw_tchars.t_stopc == -1){
		_raw_tchars.t_startc = 'Q' - '@'; /* Turn ON ^S/^Q */
		_raw_tchars.t_stopc  = 'S' - '@';
		(void)ioctl(STDIN_FD, TIOCSETC, &_raw_tchars);
	    }

#endif
	}
	else{
#if	HAS_TERMIOS

	    if(_raw_tty.c_iflag & IXON){
		_raw_tty.c_iflag &= ~IXON;	/* turn off ^S/^Q on input   */
		tcsetattr (STDIN_FD, TCSADRAIN, &_raw_tty);
	    }

#elif	HAS_TERMIO

	    if(_raw_tty.c_iflag & IXON){
		_raw_tty.c_iflag &= ~IXON;	/* turn off ^S/^Q on input   */
		(void)ioctl(STDIN_FD, TCSETAW, &_raw_tty);
	    }

#else	/* RAW_BSD */
	    if(!(_raw_tchars.t_startc == -1 && _raw_tchars.t_stopc == -1)){
		_raw_tchars.t_startc = -1;	/* Turn off ^S/^Q */
		_raw_tchars.t_stopc  = -1;
		(void)ioctl(STDIN_FD, TIOCSETC, &_raw_tchars);
	    }
#endif
	}
    }
}


/*
 *  Set up the tty driver to do LF->CR translation
 *
 * Args: state -- True to turn on translation, false to write raw LF's
 *
 * Result: none.
 */
void
crlf_proc(int state)
{
    if(_inraw){
	if(state){				/* turn ON NL->CR on output */
#if	HAS_TERMIOS

	    if(!(_raw_tty.c_oflag & ONLCR)){
		_raw_tty.c_oflag |= ONLCR;
		tcsetattr (STDIN_FD, TCSADRAIN, &_raw_tty);
	    }

#elif	HAS_TERMIO

	    if(!(_raw_tty.c_oflag & ONLCR)){
		_raw_tty.c_oflag |= ONLCR;
		(void)ioctl(STDIN_FD, TCSETAW, &_raw_tty);
	    }

#else	/* RAW_BSD */
	    if(!(_raw_tty.sg_flags & CRMOD)){
		_raw_tty.sg_flags |= CRMOD;
		(void)ioctl(STDIN_FD, TIOCSETP, &_raw_tty);
	    }
#endif
	}
	else{					/* turn OFF NL-CR on output */
#if	HAS_TERMIOS

	    if(_raw_tty.c_oflag & ONLCR){
		_raw_tty.c_oflag &= ~ONLCR;
		tcsetattr (STDIN_FD, TCSADRAIN, &_raw_tty);
	    }

#elif	HAS_TERMIO

	    if(_raw_tty.c_oflag & ONLCR){
		_raw_tty.c_oflag &= ~ONLCR;
		(void)ioctl(STDIN_FD, TCSETAW, &_raw_tty);
	    }

#else	/* RAW_BSD */

	    if(_raw_tty.sg_flags & CRMOD){
		_raw_tty.sg_flags &= ~CRMOD;
		(void)ioctl(STDIN_FD, TIOCSETP, &_raw_tty);
	    }

#endif
	}
    }
}


/*
 *  Set up the tty driver to hanle interrupt char
 *
 * Args: state -- True to turn on interrupt char, false to not
 *
 * Result: tty driver that'll send us SIGINT or not
 */
void
intr_proc(int state)
{
    if(_inraw){
	if(state){
#if	HAS_TERMIOS

	    _raw_tty.c_lflag |= ISIG;		/* enable signals */
	    tcsetattr(STDIN_FD, TCSADRAIN, &_raw_tty);

#elif	HAS_TERMIO

	    _raw_tty.c_lflag |= ISIG;		/* enable signals */
	    (void)ioctl(STDIN_FD, TCSETAW, &_raw_tty);

#else	/* RAW_BSD */

	    _raw_tchars.t_intrc = ctrl('C');
	    (void)ioctl(STDIN_FD, TIOCSETC, &_raw_tchars);

#endif
	}
	else{
#if	HAS_TERMIOS

	    _raw_tty.c_lflag &= ~ISIG;		/* disable signals */
	    tcsetattr(STDIN_FD, TCSADRAIN, &_raw_tty);

#elif	HAS_TERMIO

	    _raw_tty.c_lflag &= ~ISIG;		/* disable signals */
	    (void)ioctl(STDIN_FD, TCSETAW, &_raw_tty);

#else	/* RAW_BSD */
	    _raw_tchars.t_intrc = -1;
	    (void)ioctl(STDIN_FD, TIOCSETC, &_raw_tchars);
#endif
	}
    }
}


/*
 * Discard any pending input characters
 *
 * Args:  none
 *
 * Result: pending input buffer flushed
 */
void
flush_input(void)
{
#if	HAS_TERMIOS

    tcflush(STDIN_FD, TCIFLUSH);

#elif	HAS_TERMIO

    ioctl(STDIN_FD, TCFLSH, 0);

#else	/* RAW_BSD */

#ifdef	TIOCFLUSH
#ifdef	FREAD
    int i = FREAD;
#else
    int i = 1;
#endif
    ioctl(STDIN_FD, TIOCFLUSH, &i);
#endif	/* TIOCFLUSH */

#endif
}


/*
 * Turn off hi bit stripping
 */
void
bit_strip_off(void)
{
#if	HAS_TERMIOS

    _raw_tty.c_iflag &= ~ISTRIP;
    tcsetattr(STDIN_FD, TCSADRAIN, &_raw_tty);

#elif	HAS_TERMIO

    /* no op */

#else	/* RAW_BSD */

#ifdef	LPASS8    
    _raw_lmode |= LPASS8;
#endif

#ifdef	LPASS8OUT
    _raw_lmode |= LPASS8OUT;
#endif

    (void)ioctl(STDIN_FD, TIOCLSET, &_raw_lmode);

#endif
}


/*
 * Turn off quit character (^\) if possible
 */
void
quit_char_off(void)
{
#if	HAS_TERMIOS

#elif	HAS_TERMIO

#else	/* RAW_BSD */
    _raw_tchars.t_quitc = -1;
    (void)ioctl(STDIN_FD, TIOCSETC, &_raw_tchars);
#endif
}


/*
 * Returns TRUE if tty is < 4800, 0 otherwise.
 */
int
ttisslow(void)
{
#if	HAS_TERMIOS

    return(cfgetospeed(&_raw_tty) < B4800);

#elif	HAS_TERMIO

    return((_raw_tty.c_cflag&CBAUD) < (unsigned int)B4800);

#else	/* RAW_BSD */

    return((int)_raw_tty.sg_ispeed < B4800);

#endif
}
