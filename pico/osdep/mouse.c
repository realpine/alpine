#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: mouse.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
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

#include "../headers.h"

#include "getkey.h"

#ifdef _WINDOWS
#include "mswin.h"
#endif

#include "mouse.h"

#ifdef	MOUSE


#ifndef _WINDOWS

/* useful definitions */
#define	XTERM_MOUSE_ON	"\033[?1000h"	/* DECSET with parm 1000 */
#define	XTERM_MOUSE_OFF	"\033[?1000l"	/* DECRST with parm 1000  */


/* useful declarations */
static int mexist = 0;		/* is the mouse driver installed? */
static unsigned mnoop;



/* internal prototypes */
void	mouseon(void);
void	mouseoff(void);



/* 
 * init_mouse - check for xterm and initialize mouse tracking if present...
 */
int
init_mouse(void)
{
    if(mexist)
      return(TRUE);

    if(getenv("DISPLAY")){
	mouseon();
        kpinsert("\033[M", KEY_XTERM_MOUSE, 1);
	return(mexist = TRUE);
    }
    else
      return(FALSE);
}


/* 
 * end_mouse - clear xterm mouse tracking if present...
 */
void
end_mouse(void)
{
    if(mexist){
	mexist = 0;			/* just see if it exists here. */
	mouseoff();
    }
}


/*
 * mouseexist - function to let outsiders know if mouse is turned on
 *              or not.
 */
int
mouseexist(void)
{
    return(mexist);
}


/*
 * mouseon - call made available for programs calling pico to turn ON the
 *           mouse cursor.
 */
void
mouseon(void)
{
    fputs(XTERM_MOUSE_ON, stdout);
}


/*
 * mouseon - call made available for programs calling pico to turn OFF the
 *           mouse cursor.
 */
void
mouseoff(void)
{
    fputs(XTERM_MOUSE_OFF, stdout);
}


/* 
 * checkmouse - look for mouse events in key menu and return 
 *              appropriate value.
 */
int
checkmouse(unsigned long *ch, int down, int mcol, int mrow)
{
    static int oindex;
    int i = 0, rv = 0;
    MENUITEM *mp;

    if(!mexist || mcol < 0 || mrow < 0)
      return(FALSE);

    if(down)			/* button down */
      oindex = -1;

    for(mp = mfunc; mp; mp = mp->next)
      if(mp->action && M_ACTIVE(mrow, mcol, mp))
	break;

    if(mp){
	unsigned long r;

	r = (*mp->action)(down ? M_EVENT_DOWN : M_EVENT_UP,
			  mrow, mcol, M_BUTTON_LEFT, 0);
	if(r){
	    *ch = r;
	    rv  = TRUE;
	}
    }
    else{
	while(1){			/* see if we understand event */
	    if(i >= 12){
		i = -1;
		break;
	    }

	    if(M_ACTIVE(mrow, mcol, &menuitems[i]))
	      break;

	    i++;
	}

	if(down){			/* button down */
	    oindex = i;			/* remember where */
	    if(i != -1
	       && menuitems[i].label_hiliter != NULL
	       && menuitems[i].val != mnoop)  /* invert label */
	      (*menuitems[i].label_hiliter)(1, &menuitems[i]);
	}
	else{				/* button up */
	    if(oindex != -1){
		if(i == oindex){
		    *ch = menuitems[i].val;
		    rv = TRUE;
		}
	    }
	}
    }

    /* restore label */
    if(!down
       && oindex != -1
       && menuitems[oindex].label_hiliter != NULL
       && menuitems[oindex].val != mnoop)
      (*menuitems[oindex].label_hiliter)(0, &menuitems[oindex]);

    return(rv);
}


/*
 * invert_label - highlight the label of the given menu item.
 */
void
invert_label(int state, MENUITEM *m)
{
    unsigned i, j;
    int   col_offset, savettrow, savettcol;
    char *lp;

    get_cursor(&savettrow, &savettcol);

    /*
     * Leave the command name bold
     */
    col_offset = (state || !(lp=strchr(m->label, ' '))) ? 0 : (lp - m->label);
    movecursor((int)m->tl.r, (int)m->tl.c + col_offset);
    flip_inv(state);

    for(i = m->tl.r; i <= m->br.r; i++)
      for(j = m->tl.c + col_offset; j <= m->br.c; j++)
	if(i == m->lbl.r && j == m->lbl.c + col_offset && m->label){
	    lp = m->label + col_offset;		/* show label?? */
	    while(*lp && j++ < m->br.c)
	      putc(*lp++, stdout);

	    continue;
	}
	else
	  putc(' ', stdout);

    if(state)
      flip_inv(FALSE);

    movecursor(savettrow, savettcol);
}

#else /* _WINDOWS */

#define MOUSE_BUTTONS		3

static int mexist = 0;			/* is the mouse driver installed? */
static int nbuttons;			/* number of buttons on the mouse */
static unsigned mnoop;

/* 
 * init_mouse - check for and initialize mouse driver...
 */

int
init_mouse(void)
{
    nbuttons = MOUSE_BUTTONS;
    return (mexist = TRUE);		/* Mouse always exists under windows */
}

/*
 * end_mouse - a no-op on Windows
 */
void
end_mouse(void)
{
}

/*
 * mouseexist - function to let outsiders know if mouse is turned on
 *              or not.
 */
int
mouseexist(void)
{
    return(mexist);
}

/* 
 * checkmouse - Check mouse and return maped command.
 *
 *	EXPORTED to pico.
 *      NOTE: "down", "xxx", and "yyy" aren't used under windows.
 */
int	
checkmouse (unsigned long *ch, int ddd, int xxx, int yyy)
{
    static int	oindex;		/* Index of previous mouse down. */
    int		mcol;		/* current mouse column */
    int		mrow;		/* current mouse row */
    unsigned long r;
    int		rv = 0;		/* TRUE when we have something to return. */
    MEvent      mouse;
    int		i = 0;
    MENUITEM	*mp;

    
    *ch = 0;
    
    /* Mouse installed? */
    if (!mexist)
	return (FALSE);

    if (!mswin_getmouseevent (&mouse)) 
	return (FALSE);


    /* Location of mouse event. */
    mcol = mouse.nColumn;
    mrow = mouse.nRow;
    
    
    
    /* 
     * If there is a tracking function it gets all the mouse events
     * reguardless of where they occur.
     */
    if (mtrack != NULL) {
	r = mtrack (mouse.event, mrow, mcol, mouse.button, mouse.keys);
	if (r & 0xffff){
	    *ch = r;
	    rv  = TRUE;
	}
	return (rv);
    }
    



    /* Mouse down or up? */
    if (mouse.event == M_EVENT_DOWN) {	/* button down */
	oindex = -1;	/* No Previous mouse down. */
    }


    /* In special screen region? */
    for(mp = mfunc; mp; mp = mp->next)
      if(mp->action && M_ACTIVE(mrow, mcol, mp))
	break;

    if(mp){

	r = (*mp->action)(mouse.event, mrow, mcol, mouse.button, mouse.keys);
	if (r){
	    *ch = r;
	    rv  = TRUE;
	}
    }
    else if(mouse.button == M_BUTTON_LEFT){

	/* In any of the menuitems? */
	while(1){	/* see if we understand event */
	    if(i >= 12){
		i = -1;	/* Not Found. */
		break;
	    }

	    if(M_ACTIVE(mrow, mcol, &menuitems[i]))
		break;	/* Found. */

	    i++;		/* Next. */
	}

	/* Now, was that a mouse down or mouse up? */
	if (mouse.event == M_EVENT_DOWN) {	/* button down */
	    oindex = i;			/* remember where */
	    if(i != -1){		/* invert label */
		if(menuitems[i].label_hiliter != NULL)
		  (*menuitems[i].label_hiliter)(1, &menuitems[i]);
		else
		    invert_label(1, &menuitems[i]);
	    }
	}
	else if (mouse.event == M_EVENT_UP) {/* button up */
	    if (oindex != -1) {		  /* If up in menu item. */
		if (i == oindex){	  /* And same item down in. */
		    *ch = menuitems[i].val; /* Return menu character. */
		    rv = 1;
		}
	    }
	}
    }
    else if(mouse.button == M_BUTTON_RIGHT){
	if (mouse.event == M_EVENT_UP) {
	    mswin_keymenu_popup();
	}
    }

    /* If this is mouse up AND there was a mouse down in a menu item
     * then uninvert that menu item */
    if(mouse.event == M_EVENT_UP && oindex != -1){
	if(menuitems[oindex].label_hiliter != NULL)
	  (*menuitems[oindex].label_hiliter)(0, &menuitems[oindex]);
	else
	  invert_label(0, &menuitems[oindex]);
    }

    return(rv);
}

/*
 * invert_label - highlight the label of the given menu item.
 */
void
invert_label(int state, MENUITEM *m)
{
    int			r, c;
    unsigned            i, j;
    char		*lp;
    int			old_state;
    int			wasShown;
    int			col_offset;
    COLOR_PAIR          *lastc = NULL;
    if(m->val == mnoop)
      return;

  
    mswin_getpos (&r, &c);			/* get cursor position */
    wasShown = mswin_showcaret (0);
    old_state = mswin_getrevstate ();
    /*
     * Leave the command name bold
     */
    col_offset = (state || !(lp=strchr(m->label, ' '))) ? 0 : (lp - m->label);
    (*term.t_move)(m->tl.r, m->tl.c + col_offset);
    if(state && m->kncp)
      lastc = pico_set_colorp(m->kncp, PSC_REV|PSC_RET);
    else if(!state && m->klcp)
      lastc = pico_set_colorp(m->klcp, PSC_NORM|PSC_RET);
    else
      (*term.t_rev)(state);

    for(i = m->tl.r; i <= m->br.r; i++) {
      for(j = m->tl.c + col_offset; j <= m->br.c; j++) {
        if(i == m->lbl.r && j == m->lbl.c + col_offset){ /* show label?? */
	      lp = m->label + col_offset;
	      while(*lp && j++ < m->br.c)
	        (*term.t_putchar)(*lp++);

	      continue;
	    }
	    else
	      (*term.t_putchar)(' ');
      }
    }

    if(lastc){
	(void)pico_set_colorp(lastc, PSC_NONE);
	free_color_pair(&lastc);
    }
    else
        (*term.t_rev)(old_state);
    mswin_move (r, c);
    mswin_showcaret (wasShown);
}


#endif /* _WINDOWS */

#endif /* MOUSE */
