#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: getkey.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
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

#include "../estruct.h"
#include "../mode.h"
#include "../pico.h"
#include "../edef.h"
#include "../efunc.h"
#include "../keydefs.h"

#include "tty.h"
#include "getkey.h"
#include "read.h"
#include "mouse.h"

#ifdef _WINDOWS
#include "mswin.h"
static int	MapMSKEYtoPK(int c);
#endif /* _WINDOWS */


/* internal declarations */
static int timeo = 0;

/* these next two are declared in pith/conf.h */
int
set_input_timeout(int t)
{
    int oldtimeo = timeo;

    timeo = t;
    return(oldtimeo);
}

int
get_input_timeout(void)
{
    return(timeo);
}


#ifndef _WINDOWS


/* internal prototypes */
void		bail(void);
int		ReadyForKey(int);



void
bail(void)
{
    sleep(30);				/* see if os receives SIGHUP */
    kill(getpid(), SIGHUP);			/* eof or bad error */
}


#if	TYPEAH
/* 
 * typahead - Check to see if any characters are already in the
 *	      keyboard buffer
 */
int
typahead(void)
{
    int x;	/* holds # of pending chars */

    return((ioctl(0,FIONREAD,&x) < 0) ? 0 : x);
}
#endif /* TYPEAH */



/*
 * ReadyForKey - return true if there's no timeout or we're told input
 *		 is available...
 */
int
ReadyForKey(int timeout)
{
    switch(input_ready(timeout)){
      case READY_TO_READ:
	return(1);
	break;

      case NO_OP_COMMAND:
      case NO_OP_IDLE:
      case READ_INTR:
	return(0);

      case BAIL_OUT:
      case PANIC_NOW:
	emlwrite("\007Problem reading from keyboard!", NULL);
	kill(getpid(), SIGHUP);	/* Bomb out (saving our work)! */
	/* no return */
    }

    /* can't happen */
    return(0);
}



/*
 * GetKey - Read in a key.
 * Do the standard keyboard preprocessing. Convert the keys to the internal
 * character set.  Resolves escape sequences and returns no-op if global
 * timeout value exceeded.
 */
UCS
GetKey(void)
{
    UCS ch, status, cc;

    if(!ReadyForKey(FUDGE-5))
      return(NODATA);

    switch(status = kbseq(simple_ttgetc, NULL, bail, input_cs, &ch)){
      case 0: 	/* regular character */
	break;

      case KEY_DOUBLE_ESC:
	/*
	 * Special hack to get around comm devices eating control characters.
	 */
	if(!ReadyForKey(5))
	  return(BADESC);		/* user typed ESC ESC, then stopped */
	else
	    switch(status = kbseq(simple_ttgetc, NULL, bail, input_cs, &ch)){
	      case  KEY_UP		:
	      case  KEY_DOWN		:
	      case  KEY_RIGHT		:
	      case  KEY_LEFT		:
	      case  KEY_PGUP		:
	      case  KEY_PGDN		:
	      case  KEY_HOME		:
	      case  KEY_END		:
	      case  KEY_DEL		:
	      case F1  :
	      case F2  :
	      case F3  :
	      case F4  :
	      case F5  :
	      case F6  :
	      case F7  :
	      case F8  :
	      case F9  :
	      case F10 :
	      case F11 :
	      case F12 :
		return(CTRL | status);
		break;

	      case 0: 	/* regular character */
		break;

	      default:				/* punt the whole thing	*/
		(*term.t_beep)();
		return(BADESC);
		break;
	    }

	ch &= 0x7f;
	if(isdigit((unsigned char)ch)){
	    int n = 0, i = ch - '0';

	    if(i < 0 || i > 2)
	      return(BADESC);		/* bogus literal char value */

	    while(n++ < 2){
		if(!ReadyForKey(5)
		   || (!isdigit((unsigned char) (ch =
				   (*term.t_getchar)(NODATA, NULL, bail)))
		       || (n == 1 && i == 2 && ch > '5')
		       || (n == 2 && i == 25 && ch > '5'))){
		    return(BADESC);
		}

		i = (i * 10) + (ch - '0');
	    }

	    ch = i;
	}
	else{
	    if(islower((unsigned char)ch))	/* canonicalize if alpha */
	      ch = toupper((unsigned char)ch);

	    return((isalpha((unsigned char)ch) || ch == '@'
		    || (ch >= '[' && ch <= '_'))
		    ? (CTRL | ch) : ((ch == ' ') ? (CTRL | '@') : ch));
	}

	break;

#ifdef MOUSE
      case KEY_XTERM_MOUSE:
	{
	    /*
	     * Special hack to get mouse events from an xterm.
	     * Get the details, then pass it past the keymenu event
	     * handler, and then to the installed handler if there
	     * is one...
	     */
	    static int    down = 0;
	    int           x, y, button;
	    unsigned long cmd;

	    button = (*term.t_getchar)(NODATA, NULL, bail) & 0x03;

	    x = (*term.t_getchar)(NODATA, NULL, bail) - '!';
	    y = (*term.t_getchar)(NODATA, NULL, bail) - '!';

	    if(button == 0){
		down = 1;
		if(checkmouse(&cmd, 1, x, y))
		  return((UCS) cmd);
	    }
	    else if(down && button == 3){
		down = 0;
		if(checkmouse(&cmd, 0, x, y))
		  return((UCS) cmd);
	    }

	    return(NODATA);
	}

	break;
#endif /* MOUSE */

      case  KEY_UP		:
      case  KEY_DOWN		:
      case  KEY_RIGHT		:
      case  KEY_LEFT		:
      case  KEY_PGUP		:
      case  KEY_PGDN		:
      case  KEY_HOME		:
      case  KEY_END		:
      case  KEY_DEL		:
      case F1  :
      case F2  :
      case F3  :
      case F4  :
      case F5  :
      case F6  :
      case F7  :
      case F8  :
      case F9  :
      case F10 :
      case F11 :
      case F12 :
	return(status);

      case  CTRL_KEY_UP		:
	return(CTRL | KEY_UP);
      case  CTRL_KEY_DOWN	:
	return(CTRL | KEY_DOWN);
      case  CTRL_KEY_RIGHT	:
	return(CTRL | KEY_RIGHT);
      case  CTRL_KEY_LEFT	:
	return(CTRL | KEY_LEFT);

      case KEY_SWALLOW_Z:
	status = BADESC;
      case KEY_SWAL_UP:
      case KEY_SWAL_DOWN:
      case KEY_SWAL_LEFT:
      case KEY_SWAL_RIGHT:
	do
	  if(!ReadyForKey(2)){
	      status = BADESC;
	      break;
	  }
	while(!strchr("~qz", (*term.t_getchar)(NODATA, NULL, bail)));

	return((status == BADESC)
		? status
		: status - (KEY_SWAL_UP - KEY_UP));
	break;

      case KEY_KERMIT:
	do{
	    cc = ch;
	    if(!ReadyForKey(2))
	      return(BADESC);
	    else
	      ch = (*term.t_getchar)(NODATA, NULL, bail) & 0x7f;
	}while(cc != '\033' && ch != '\\');

	ch = NODATA;
	break;

      case BADESC:
	(*term.t_beep)();
	return(status);

      default:				/* punt the whole thing	*/
	(*term.t_beep)();
	break;
    }

    if (ch >= 0x00 && ch <= 0x1F)       /* C0 control -> C-     */
      ch = CTRL | (ch+'@');

    return(ch);
}


/* 
 * kbseq - looks at an escape sequence coming from the keyboard and 
 *         compares it to a trie of known keyboard escape sequences, and
 *         returns the function bound to the escape sequence.
 * 
 *     Args: getcfunc     -- Function to get a single character from stdin,
 *                           called with the next two arguments to this
 *                           function as its arguments.
 *           recorder     -- If non-NULL, function used to record keystroke.
 *           bail_handler -- Function used to bail out on read error.
 *           c            -- Pointer to returned character.
 *
 *  Returns: BADESC
 *           The escaped function.
 *           0 if a regular char with char stuffed in location c.
 */
UCS
kbseq(int (*getcfunc)(int (*recorder)(int ), void (*bail_handler)(void )),
      int (*recorder)(int),
      void (*bail_handler)(void),
      void *data,
      UCS *ch)
{
    unsigned char c;
    int      first = 1;
    KBESC_T *current;

    current = kbesc;
    if(current == NULL)				/* bag it */
      return(BADESC);

    while(1){
	c = (*getcfunc)(recorder, bail_handler);

	while(current->value != c){
	    if(current->left == NULL){		/* NO MATCH */
		if(first){
		    unsigned long octets_so_far, remaining_octets;
		    unsigned char *inputp;
		    UCS ucs;
		    unsigned char inputbuf[20];

		    /*
		     * Regular character.
		     * Read enough bytes to make up a character and convert it to UCS-4.
		     */
		    memset(inputbuf, 0, sizeof(inputbuf));
		    inputbuf[0] = c;
		    octets_so_far = 1;
		    for(;;){
			remaining_octets = octets_so_far;
			inputp = inputbuf;
			ucs = mbtow(data, &inputp, &remaining_octets);
			switch(ucs){
			  case CCONV_BADCHAR:
			    /*
			     * Not really a BADESC but that ought to
			     * be sufficient. We can add another type if
			     * we need to.
			     */
			    return(BADESC);
			  
			  case CCONV_NEEDMORE:
			    if(octets_so_far >= sizeof(inputbuf))
			      return(BADESC);

			    c = (*getcfunc)(recorder, bail_handler);
			    inputbuf[octets_so_far++] = c;
			    break;

			  default:
			    /* got a good UCS-4 character */
			    *ch = ucs;
			    return(0);
			}
		    }

		    /* NOTREACHED */
		    return(0);
		}
		else
		  return(BADESC);
	    }
	    current = current->left;
	}

	if(current->down == NULL)		/* match!!!*/
	  return(current->func);
	else
	  current = current->down;

	first = 0;
    }
}


#define	newnode()	(KBESC_T *)malloc(sizeof(KBESC_T))

/*
 * kpinsert - insert a keystroke escape sequence into the global search
 *	      structure.
 */
void
kpinsert(char *kstr, int kval, int termcap_wins)
{
    register	char	*buf;
    register	KBESC_T *temp;
    register	KBESC_T *trail;

    if(kstr == NULL)
      return;

    /*
     * Don't allow escape sequences that don't start with ESC unless
     * termcap_wins.  This is to protect against mistakes in termcap files.
     */
    if(!termcap_wins && *kstr != '\033')
      return;

    temp = trail = kbesc;
    buf = kstr;

    for(;;){
	if(temp == NULL){
	    temp = newnode();
	    temp->value = *buf;
	    temp->func = 0;
	    temp->left = NULL;
	    temp->down = NULL;
	    if(kbesc == NULL)
	      kbesc = temp;
	    else
	      trail->down = temp;
	}
	else{				/* first entry */
	    while((temp != NULL) && (temp->value != *buf)){
		trail = temp;
		temp = temp->left;
	    }

	    if(temp == NULL){   /* add new val */
		temp = newnode();
		temp->value = *buf;
		temp->func = 0;
		temp->left = NULL;
		temp->down = NULL;
		trail->left = temp;
	    }
	}

	if(*(++buf) == '\0')
	  break;
	else{
	    /*
	     * Ignore attempt to overwrite shorter existing escape sequence.
	     * That means that sequences with higher priority should be
	     * set up first, so if we want termcap sequences to override
	     * hardwired sequences, put the kpinsert calls for the
	     * termcap sequences first.  (That's what you get if you define
	     * TERMCAP_WINS.)
	     */
	    if(temp->func != 0)
	      return;

	    trail = temp;
	    temp = temp->down;
	}
    }
    
    /*
     * Ignore attempt to overwrite longer sequences we are a prefix
     * of (down != NULL) and exact same sequence (func != 0).
     */
    if(temp != NULL && temp->down == NULL && temp->func == 0)
      temp->func = kval;
}



/*
 * kbdestroy() - kills the key pad function key search tree
 *		 and frees all lines associated with it
 *
 *               Should be called with arg kbesc, the top of the tree.
 */
void
kbdestroy(KBESC_T *kb)
{
    if(kb){
	kbdestroy(kb->left);
	kbdestroy(kb->down);
	free((char *)kb);
	kb = NULL;
    }
}

#else /* _WINDOWS */

/*
 * Read in a key.
 * Do the standard keyboard preprocessing. Convert the keys to the internal
 * character set.  Resolves escape sequences and returns no-op if global
 * timeout value exceeded.
 */
UCS
GetKey(void)
{
    UCS			ch = 0;
    long		timein;
    

    ch = NODATA;
    timein = time(0L);

    /*
     * Main character processing loop.
     */
    while(!mswin_charavail()) {

#ifdef MOUSE
	/* Check Mouse.  If we get a mouse event, convert to char
	 * event and return that. */
	if (checkmouse (&ch,0,0,0)) {
	    curwp->w_flag |= WFHARD;
	    return (ch);
	}
#endif /* MOUSE */


	/* Check Timeout. */
	if(time(0L) >= timein+(FUDGE-10)) 
	    return(NODATA);
    }

    
    return (mswin_getc_fast());
}

#endif /* _WINDOWS */
