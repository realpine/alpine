#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: color.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
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

#include "terminal.h"
#include "color.h"
#include "../../pith/osdep/color.h"
#include "../../pith/osdep/collate.h"

#ifndef	_WINDOWS

struct color_name_list {
    char                  *name;
    int                    namelen;
    struct color_name_list *next;
};

struct color_table {
    struct color_name_list *names;
    char *rgb;
    int   red, green, blue;
    int   val;
};


/* useful definitions */
#define	ANSI8_COLOR()	(color_options & COLOR_ANSI8_OPT)
#define	ANSI16_COLOR()	(color_options & COLOR_ANSI16_OPT)
#define	ANSI256_COLOR()	(color_options & COLOR_ANSI256_OPT)
#define	ANSI_COLOR()	(color_options & (COLOR_ANSI8_OPT | COLOR_ANSI16_OPT | COLOR_ANSI256_OPT))
/* transparent (default) color is possible */
#define	COL_TRANS()	((color_options & COLOR_TRANS_OPT) ? 1 : 0)	/* transparent */
#define	END_PSEUDO_REVERSE	"EndInverse"
#define A_UNKNOWN	-1


/* local globals */
static unsigned color_options;
static COLOR_PAIR *the_rev_color, *the_normal_color;
static COLOR_PAIR *color_blasted_by_attrs;
static int pinvstate;	/* physical state of inverse (standout) attribute */
static int pboldstate;	/* physical state of bold attribute */
static int pulstate;	/* physical state of underline attribute */
static int rev_color_state;

static int boldstate;	/* should-be state of bold attribute */
static int ulstate;	/* should-be state of underline attribute */
static int invstate;	/* should-be state of Inverse, which could be a color
			   change or an actual setinverse */
static int     _color_inited, _using_color;
static char   *_nfcolor, *_nbcolor, *_rfcolor, *_rbcolor;
static char   *_last_fg_color, *_last_bg_color;
static int     _force_fg_color_change;
static int     _force_bg_color_change;

static struct color_table *color_tbl;

/* external references */
extern char   *_op, *_oc, *_setaf, *_setab, *_setf, *_setb, *_scp;
extern int    _bce, _colors;


/* internal prototypes */
void     flip_rev_color(int);
void     flip_bold(int);
void     flip_ul(int);
void     reset_attr_state(void);
void     add_to_color_name_list(struct color_table *t, char *name);
void     free_color_name_list(struct color_name_list **nl);


#if	HAS_TERMINFO
extern char    *tparm(char *, ...);
#endif
extern char    *tgoto();
void	tinitcolor(void);
int	tfgcolor(int);
int	tbgcolor(int);
struct color_table *init_color_table(void);
void	free_color_table(struct color_table **);
int	color_to_val(char *);



/*
 *     Start or end bold mode
 *
 * Result: escape sequence to go into or out of reverse color is output
 *
 *     (This is only called when there is a reverse color.)
 *
 * Arg   state = ON   set bold
 *               OFF  set normal
 */
void
flip_rev_color(int state)
{
    if((rev_color_state = state) == TRUE)
      (void)pico_set_colorp(the_rev_color, PSC_NONE);
    else
      pico_set_normal_color();
}


/*
 *     Start or end bold mode
 *
 * Result: escape sequence to go into or out of bold is output
 *
 * Arg   state = ON   set bold
 *               OFF  set normal
 */
void
flip_bold(int state)
{
    extern char *_setbold, *_clearallattr;

    if((pboldstate = state) == TRUE){
	if(_setbold != NULL)
	  putpad(_setbold);
    }
    else{
	if(_clearallattr != NULL){
	    if(!color_blasted_by_attrs)
	      color_blasted_by_attrs = pico_get_cur_color();

	    _force_fg_color_change = _force_bg_color_change = 1;
	    putpad(_clearallattr);
	    pinvstate = state;
	    pulstate = state;
	    rev_color_state = state;
	}
    }
}


/*
 *     Start or end inverse mode
 *
 * Result: escape sequence to go into or out of inverse is output
 *
 * Arg   state = ON   set inverse
 *               OFF  set normal
 */
void
flip_inv(int state)
{
    extern char *_setinverse, *_clearinverse;

    if((pinvstate = state) == TRUE){
	if(_setinverse != NULL)
	  putpad(_setinverse);
    }
    else{
	/*
	 * Unfortunately, some termcap entries configure end standout to
	 * be clear all attributes.
	 */
	if(_clearinverse != NULL){
	    if(!color_blasted_by_attrs)
	      color_blasted_by_attrs = pico_get_cur_color();

	    _force_fg_color_change = _force_bg_color_change = 1;
	    putpad(_clearinverse);
	    pboldstate = (pboldstate == FALSE) ?  pboldstate : A_UNKNOWN;
	    pulstate = (pulstate == FALSE) ?  pulstate : A_UNKNOWN;
	    rev_color_state = A_UNKNOWN;
	}
    }
}


/*
 *     Start or end underline mode
 *
 * Result: escape sequence to go into or out of underline is output
 *
 * Arg   state = ON   set underline
 *               OFF  set normal
 */
void
flip_ul(int state)
{
    extern char *_setunderline, *_clearunderline;

    if((pulstate = state) == TRUE){
	if(_setunderline != NULL)
	  putpad(_setunderline);
    }
    else{
	/*
	 * Unfortunately, some termcap entries configure end underline to
	 * be clear all attributes.
	 */
	if(_clearunderline != NULL){
	    if(!color_blasted_by_attrs)
	      color_blasted_by_attrs = pico_get_cur_color();

	    _force_fg_color_change = _force_bg_color_change = 1;
	    putpad(_clearunderline);
	    pboldstate = (pboldstate == FALSE) ?  pboldstate : A_UNKNOWN;
	    pinvstate = (pinvstate == FALSE) ?  pinvstate : A_UNKNOWN;
	    rev_color_state = A_UNKNOWN;
	}
    }
}


void
StartInverse(void)
{
    invstate = TRUE;
    reset_attr_state();
}


void
EndInverse(void)
{
    invstate = FALSE;
    reset_attr_state();
}


int
InverseState(void)
{
    return(invstate);
}


int
StartBold(void)
{
    extern char *_setbold;

    boldstate = TRUE;
    reset_attr_state();
    return(_setbold != NULL);
}


void
EndBold(void)
{
    boldstate = FALSE;
    reset_attr_state();
}


void
StartUnderline(void)
{
    ulstate = TRUE;
    reset_attr_state();
}


void
EndUnderline(void)
{
    ulstate = FALSE;
    reset_attr_state();
}


void
reset_attr_state(void)
{
    /*
     * If we have to turn some attributes off, do that first since that
     * may turn off other attributes as a side effect.
     */
    if(boldstate == FALSE && pboldstate != boldstate)
      flip_bold(boldstate);

    if(ulstate == FALSE && pulstate != ulstate)
      flip_ul(ulstate);

    if(invstate == FALSE){
	if(pico_get_rev_color()){
	    if(rev_color_state != invstate)
	      flip_rev_color(invstate);
	}
	else{
	    if(pinvstate != invstate)
	      flip_inv(invstate);
	}
    }
    
    /*
     * Now turn everything on that needs turning on.
     */
    if(boldstate == TRUE && pboldstate != boldstate)
      flip_bold(boldstate);

    if(ulstate == TRUE && pulstate != ulstate)
      flip_ul(ulstate);

    if(invstate == TRUE){
	if(pico_get_rev_color()){
	    if(rev_color_state != invstate)
	      flip_rev_color(invstate);
	}
	else{
	    if(pinvstate != invstate)
	      flip_inv(invstate);
	}
    }

    if(color_blasted_by_attrs){
	(void)pico_set_colorp(color_blasted_by_attrs, PSC_NONE);
	free_color_pair(&color_blasted_by_attrs);
    }

}


void
tinitcolor(void)
{
    if(_color_inited || panicking())
      return;

    if(ANSI_COLOR() || (_colors > 0 && ((_setaf && _setab) || (_setf && _setb)
/**** not sure how to do this yet
       || _scp
****/
	))){
	_color_inited = 1;
	color_tbl = init_color_table();

	if(ANSI_COLOR())
	  putpad("\033[39;49m");
	else{
	    if(_op)
	      putpad(_op);
	    if(_oc)
	      putpad(_oc);
	}
    }
}


#if	HAS_TERMCAP
/*
 * Treading on thin ice here. Tgoto wasn't designed for this. It takes
 * arguments column and row. We only use one of them, so we put it in
 * the row argument. The 0 is just a placeholder.
 */
#define tparm(s, c) tgoto(s, 0, c)
#endif

int
tfgcolor(int color)
{
    if(!_color_inited)
      tinitcolor();

    if(!_color_inited)
      return(-1);
    
    if((ANSI8_COLOR()  && (color < 0 || color >= 8+COL_TRANS())) ||
       (ANSI16_COLOR() && (color < 0 || color >= 16+COL_TRANS())) ||
       (ANSI256_COLOR() && (color < 0 || color >= 256+COL_TRANS())) ||
       (!ANSI_COLOR()  && (color < 0 || color >= _colors+COL_TRANS())))
      return(-1);

    if(ANSI_COLOR()){
	char buf[20];

	if(COL_TRANS() && color == pico_count_in_color_table()-1)
	  snprintf(buf, sizeof(buf), "\033[39m");
	else if(ANSI256_COLOR())
	  snprintf(buf, sizeof(buf), "\033[38;5;%dm", color);
	else{
	    if(color < 8)
	      snprintf(buf, sizeof(buf), "\033[3%cm", color + '0');
	    else
	      snprintf(buf, sizeof(buf), "\033[9%cm", (color-8) + '0');
	}
	
	putpad(buf);
    }
    else if(COL_TRANS() && color == pico_count_in_color_table()-1 && _op){
	char bg_color_was[MAXCOLORLEN+1];

	bg_color_was[0] = '\0';

	/*
	 * Setting transparent (default) foreground color.
	 * We don't know how to set only the default foreground color,
	 * _op sets both foreground and background. So we need to
	 *
	 * get current background color
	 * set default colors
	 * if (current background was not default) reset it
	 */
	if(_last_bg_color && strncmp(_last_bg_color, MATCH_TRAN_COLOR, RGBLEN)){
	    strncpy(bg_color_was, _last_bg_color, sizeof(bg_color_was));
	    bg_color_was[sizeof(bg_color_was)-1] = '\0';
	}

	putpad(_op);
	if(bg_color_was[0]){
	    _force_bg_color_change = 1;
	    pico_set_bg_color(bg_color_was);
	}
    }
    else if(_setaf)
      putpad(tparm(_setaf, color));
    else if(_setf)
      putpad(tparm(_setf, color));
    else if(_scp){
	/* set color pair method */
    }

    return(0);
}


int
tbgcolor(int color)
{
    if(!_color_inited)
      tinitcolor();

    if(!_color_inited)
      return(-1);
    
    if((ANSI8_COLOR()  && (color < 0 || color >= 8+COL_TRANS())) ||
       (ANSI16_COLOR() && (color < 0 || color >= 16+COL_TRANS())) ||
       (ANSI256_COLOR() && (color < 0 || color >= 256+COL_TRANS())) ||
       (!ANSI_COLOR()  && (color < 0 || color >= _colors+COL_TRANS())))
      return(-1);

    if(ANSI_COLOR()){
	char buf[20];

	if(COL_TRANS() && color == pico_count_in_color_table()-1)
	  snprintf(buf, sizeof(buf), "\033[49m");
	else if(ANSI256_COLOR())
	  snprintf(buf, sizeof(buf), "\033[48;5;%dm", color);
	else{
	    if(color < 8)
	      snprintf(buf, sizeof(buf), "\033[4%cm",  color + '0');
	    else
	      snprintf(buf, sizeof(buf), "\033[10%cm", (color-8) + '0');
	}
	
	putpad(buf);
    }
    else if(COL_TRANS() && color == pico_count_in_color_table()-1 && _op){
	char fg_color_was[MAXCOLORLEN+1];

	fg_color_was[0] = '\0';

	/*
	 * Setting transparent (default) background color.
	 * We don't know how to set only the default background color,
	 * _op sets both foreground and background. So we need to
	 *
	 * get current foreground color
	 * set default colors
	 * if (current foreground was not default) reset it
	 */
	if(_last_fg_color && strncmp(_last_fg_color, MATCH_TRAN_COLOR, RGBLEN)){
	    strncpy(fg_color_was, _last_fg_color, sizeof(fg_color_was));
	    fg_color_was[sizeof(fg_color_was)-1] = '\0';
	}

	putpad(_op);
	if(fg_color_was[0]){
	    _force_fg_color_change = 1;
	    pico_set_fg_color(fg_color_was);
	}
    }
    else if(_setab)
      putpad(tparm(_setab, color));
    else if(_setb)
      putpad(tparm(_setb, color));
    else if(_scp){
	/* set color pair method */
    }

    return(0);
}



/*
 * We're not actually using the RGB value other than as a string which
 * maps into the color.
 * In fact, on some systems color 1 and color 4 are swapped, and color 3
 * and color 6 are swapped. So don't believe the RGB values.
 * Still, it feels right to have them be the canonical values, so we do that.
 *
 * Actually we are using them more now. In color_to_val we map to the closest
 * color if we don't get an exact match. We ignore values over 255.
 *
 * More than one "name" can map to the same "rgb".
 * More than one "name" can map to the same "val".
 * The "val" for a "name" and for its "rgb" are the same.
 */
struct color_table *
init_color_table(void)
{
    struct color_table *ct = NULL, *t;
    int                 i, count;
    char                colorname[12];

    count = pico_count_in_color_table();

    if(count > 0 && count <= 256+COL_TRANS()){
	int    ind, graylevel;
	struct {
	    char rgb[RGBLEN+1];
	    int red, green, blue;
	} cube[256];

	ct = (struct color_table *) fs_get((count+1) * sizeof(struct color_table));
	if(ct)
	  memset(ct, 0, (count+1) * sizeof(struct color_table));

	/*
	 * We boldly assume that 256 colors means xterm 256-color
	 * color cube and grayscale.
	 */
	if(ANSI_COLOR() && (count == 256+COL_TRANS())){
	    int r, g, b, gray;

	    for(r = 0; r < 6; r++)
	      for(g = 0; g < 6; g++)
		for(b = 0; b < 6; b++){
		    ind = 16 + 36*r + 6*g + b;
		    cube[ind].red   = r ? (40*r + 55) : 0;
		    cube[ind].green = g ? (40*g + 55) : 0;
		    cube[ind].blue  = b ? (40*b + 55) : 0;
		    snprintf(cube[ind].rgb, sizeof(cube[ind].rgb), "%3.3d,%3.3d,%3.3d",
			     cube[ind].red, cube[ind].green, cube[ind].blue);
		}

	    for(gray = 0; gray < 24; gray++){
		ind = gray + 232;
		graylevel = 10*gray + 8;
		cube[ind].red   = graylevel;
		cube[ind].green = graylevel;
		cube[ind].blue  = graylevel;
		snprintf(cube[ind].rgb, sizeof(cube[ind].rgb), "%3.3d,%3.3d,%3.3d",
			 graylevel, graylevel, graylevel);
	    }
	}

	for(i = 0, t = ct; t && i < count; i++, t++){
	    t->val = i;

	    switch(i){
	      case COL_BLACK:
		strncpy(colorname, "black", sizeof(colorname));
		colorname[sizeof(colorname)-1] = '\0';
		break;
	      case COL_RED:
		strncpy(colorname, "red", sizeof(colorname));
		colorname[sizeof(colorname)-1] = '\0';
		break;
	      case COL_GREEN:
		strncpy(colorname, "green", sizeof(colorname));
		colorname[sizeof(colorname)-1] = '\0';
		break;
	      case COL_YELLOW:
		strncpy(colorname, "yellow", sizeof(colorname));
		colorname[sizeof(colorname)-1] = '\0';
		break;
	      case COL_BLUE:
		strncpy(colorname, "blue", sizeof(colorname));
		colorname[sizeof(colorname)-1] = '\0';
		break;
	      case COL_MAGENTA:
		strncpy(colorname, "magenta", sizeof(colorname));
		colorname[sizeof(colorname)-1] = '\0';
		break;
	      case COL_CYAN:
		strncpy(colorname, "cyan", sizeof(colorname));
		colorname[sizeof(colorname)-1] = '\0';
		break;
	      case COL_WHITE:
		strncpy(colorname, "white", sizeof(colorname));
		colorname[sizeof(colorname)-1] = '\0';
		break;
	      default:
		snprintf(colorname, sizeof(colorname), "color%3.3d", i);
		break;
	    }

	    if(COL_TRANS() && i == count-1){
		strncpy(colorname, MATCH_TRAN_COLOR, sizeof(colorname));
		colorname[sizeof(colorname)-1] = '\0';
	    }

	    add_to_color_name_list(t, colorname);

	    if(count == 8+COL_TRANS()){
	      if(COL_TRANS() && i == count-1){
		  t->red = t->green = t->blue = -1;
	      }
	      else
	       switch(i){
		case COL_BLACK:
		  t->red = t->green = t->blue = 0;
		  add_to_color_name_list(t, "color008");
		  add_to_color_name_list(t, "colordgr");
		  add_to_color_name_list(t, "colormgr");
		  break;
		case COL_RED:
		  t->red = 255;
		  t->green = t->blue = 0;
		  add_to_color_name_list(t, "color009");
		  break;
		case COL_GREEN:
		  t->green = 255;
		  t->red = t->blue = 0;
		  add_to_color_name_list(t, "color010");
		  break;
		case COL_YELLOW:
		  t->red = t->green = 255;
		  t->blue = 0;
		  add_to_color_name_list(t, "color011");
		  break;
		case COL_BLUE:
		  t->red = t->green = 0;
		  t->blue = 255;
		  add_to_color_name_list(t, "color012");
		  break;
		case COL_MAGENTA:
		  t->red = t->blue = 255;
		  t->green = 0;
		  add_to_color_name_list(t, "color013");
		  break;
		case COL_CYAN:
		  t->red = 0;
		  t->green = t->blue = 255;
		  add_to_color_name_list(t, "color014");
		  break;
		case COL_WHITE:
		  t->red = t->green = t->blue = 255;
		  add_to_color_name_list(t, "color015");
		  add_to_color_name_list(t, "colorlgr");
		  break;
	       }
	    }
	    else if(count == 16+COL_TRANS() || count == 256+COL_TRANS()){
	      if(COL_TRANS() && i == count-1){
		  t->red = t->green = t->blue = -1;
	      }
	      else
	       /*
	        * This set of RGB values seems to come close to describing
	        * what a 16-color xterm gives you.
	        */
	       switch(i){
		case COL_BLACK:
		  t->red = t->green = t->blue = 0;
		  break;
		case COL_RED:			/* actually dark red */
		  t->red = 174;
		  t->green = t->blue = 0;
		  break;
		case COL_GREEN:			/* actually dark green */
		  t->green = 174;
		  t->red = t->blue = 0;
		  break;
		case COL_YELLOW:		/* actually dark yellow */
		  t->blue = 0;
		  t->red = t->green = 174;
		  break;
		case COL_BLUE:			/* actually dark blue */
		  t->blue = 174;
		  t->red = t->green = 0;
		  break;
		case COL_MAGENTA:		/* actually dark magenta */
		  t->green = 0;
		  t->red = t->blue = 174;
		  break;
		case COL_CYAN:			/* actually dark cyan */
		  t->red = 0;
		  t->green = t->blue = 174;
		  break;
		case COL_WHITE:			/* actually light gray */
		  t->red = t->green = t->blue = 174;
		  if(count == 16)
		    add_to_color_name_list(t, "colorlgr");

		  break;
		case 8:				/* dark gray */
		  t->red = t->green = t->blue = 64;
		  if(count == 16){
		      add_to_color_name_list(t, "colordgr");
		      add_to_color_name_list(t, "colormgr");
		  }

		  break;
		case 9:				/* red */
		  t->red = 255;
		  t->green = t->blue = 0;
		  break;
		case 10:			/* green */
		  t->green = 255;
		  t->red = t->blue = 0;
		  break;
		case 11:			/* yellow */
		  t->blue = 0;
		  t->red = t->green = 255;
		  break;
		case 12:			/* blue */
		  t->blue = 255;
		  t->red = t->green = 0;
		  break;
		case 13:			/* magenta */
		  t->green = 0;
		  t->red = t->blue = 255;
		  break;
		case 14:			/* cyan */
		  t->red = 0;
		  t->green = t->blue = 255;
		  break;
		case 15:			/* white */
		  t->red = t->green = t->blue = 255;
		  break;
		default:
		  t->red   = cube[i].red;
		  t->green = cube[i].green;
		  t->blue  = cube[i].blue;
		  switch(i){
		    case 238:
		      add_to_color_name_list(t, "colordgr");
		      break;

		    case 244:
		      add_to_color_name_list(t, "colormgr");
		      break;

		    case 250:
		      add_to_color_name_list(t, "colorlgr");
		      break;
		  }

		  break;
	       }
	    }
	    else{
	      if(COL_TRANS() && i == count-1){
		  t->red = t->green = t->blue = -1;
	      }
	      else
	       switch(i){
		case COL_BLACK:
		  t->red = t->green = t->blue = 0;
		  break;
		case COL_RED:			/* actually dark red */
		  t->red = 255;
		  t->green = t->blue = 0;
		  break;
		case COL_GREEN:			/* actually dark green */
		  t->green = 255;
		  t->red = t->blue = 0;
		  break;
		case COL_YELLOW:		/* actually dark yellow */
		  t->blue = 0;
		  t->red = t->green = 255;
		  break;
		case COL_BLUE:			/* actually dark blue */
		  t->blue = 255;
		  t->red = t->green = 0;
		  break;
		case COL_MAGENTA:		/* actually dark magenta */
		  t->green = 0;
		  t->red = t->blue = 255;
		  break;
		case COL_CYAN:			/* actually dark cyan */
		  t->red = 0;
		  t->green = t->blue = 255;
		  break;
		case COL_WHITE:			/* actually light gray */
		  t->red = t->green = t->blue = 255;
		  break;
		default:
		  /* this will just be a string match */
		  t->red = t->green = t->blue = 256+i;
		  break;
	       }
	    }
	}

	for(i = 0, t = ct; t && i < count; i++, t++){
	    t->rgb = (char *)fs_get((RGBLEN+1) * sizeof(char));
	    if(COL_TRANS() && i == count-1)
	      snprintf(t->rgb, RGBLEN+1, MATCH_TRAN_COLOR);
	    else
	      snprintf(t->rgb, RGBLEN+1, "%3.3d,%3.3d,%3.3d", t->red, t->green, t->blue);
	}
    }

    return(ct);
}


void
add_to_color_name_list(struct color_table *t, char *name)
{
    if(t && name && *name){
	struct color_name_list *new_name;

	new_name = (struct color_name_list *) fs_get(sizeof(struct color_name_list));
	if(new_name){
	    memset(new_name, 0, sizeof(*new_name));
	    new_name->namelen = strlen(name);

	    new_name->name = (char *) fs_get((new_name->namelen+1) * sizeof(char));
	    if(new_name->name){
		strncpy(new_name->name, name, new_name->namelen+1);
		new_name->name[new_name->namelen] = '\0';

		if(t->names){
		    struct color_name_list *nl;
		    for(nl = t->names; nl->next; nl = nl->next)
		      ;

		    nl->next = new_name;
		}
		else
		  t->names = new_name;
	    }
	}
    }
}


void
free_color_name_list(struct color_name_list **nl)
{
    if(nl && *nl){
	if((*nl)->next)
	  free_color_name_list(&(*nl)->next);

	if((*nl)->name)
	  fs_give((void **) &(*nl)->name);

	fs_give((void **) nl);
    }
}


/*
 * Map from integer color value to canonical color name.
 */
char *
colorx(int color)
{
    struct color_table *ct;
    static char         cbuf[12];

    /* before we get set up, we still need to use this a bit */
    if(!color_tbl){
	switch(color){
	  case COL_BLACK:
	    return("black");
	  case COL_RED:
	    return("red");
	  case COL_GREEN:
	    return("green");
	  case COL_YELLOW:
	    return("yellow");
	  case COL_BLUE:
	    return("blue");
	  case COL_MAGENTA:
	    return("magenta");
	  case COL_CYAN:
	    return("cyan");
	  case COL_WHITE:
	    return("white");
	  default:
	    snprintf(cbuf, sizeof(cbuf), "color%3.3d", color);
	    return(cbuf);
	}
    }
    
    for(ct = color_tbl; ct->names; ct++)
      if(ct->val == color)
        break;

    /* rgb _is_ the canonical name */
    if(ct->names)
      return(ct->rgb);

    /* not supposed to get here */
    snprintf(cbuf, sizeof(cbuf), "color%3.3d", color);
    return(cbuf);
}


/*
 * Argument is a color name which could be an RGB string, a name like "blue",
 * or a name like "color011".
 *
 * Returns a pointer to the canonical name of the color.
 */
char *
color_to_canonical_name(char *s)
{
    struct color_table *ct;
    struct color_name_list *nl;
    int done;

    if(!s || !color_tbl)
      return(NULL);
    
    if(*s == ' ' || isdigit(*s)){
	/* check for rgb string instead of name */
	for(ct = color_tbl; ct->rgb; ct++)
	  if(!strncmp(ct->rgb, s, RGBLEN))
	    break;
    }
    else{
	for(done=0, ct = color_tbl; !done && ct->names; ct++){
	  for(nl = ct->names; !done && nl; nl = nl->next)
	    if(nl->name && !struncmp(nl->name, s, nl->namelen))
	      done++;
	  
	  if(done)
	    break;
	}
    }
    
    /* rgb is the canonical name */
    if(ct->names)
      return(ct->rgb);
    else if(!struncmp(s, MATCH_NORM_COLOR, RGBLEN) || !struncmp(s, MATCH_NONE_COLOR, RGBLEN))
      return(s);
    
    return("");
}


/*
 * Argument is the name of a color or an RGB value that we recognize.
 * The table should be set up so that the val returned is the same whether
 * or not we choose the canonical name.
 *
 * Returns the integer value for the color.
 */
int
color_to_val(char *s)
{
    struct color_table *ct;
    struct color_name_list *nl;
    int done;

    if(!s || !color_tbl)
      return(-1);
    
    if(*s == ' ' || isdigit(*s)){
	/* check for rgb string instead of name */
	for(ct = color_tbl; ct->rgb; ct++)
	  if(!strncmp(ct->rgb, s, RGBLEN))
	    break;

	/*
	 * Didn't match any. Find "closest" to match.
	 */
	if(!ct->rgb){
	    int r = -1, g = -1, b = -1;
	    char *p, *comma, scopy[RGBLEN+1];

	    strncpy(scopy, s, sizeof(scopy));
	    scopy[sizeof(scopy)-1] = '\0';

	    p = scopy;
	    comma = strchr(p, ',');
	    if(comma){
	      *comma = '\0';
	      r = atoi(p);
	      p = comma+1;
	      if(r >= 0 && r <= 255 && *p){
		comma = strchr(p, ',');
		if(comma){
		  *comma = '\0';
		  g = atoi(p);
		  p = comma+1;
		  if(g >= 0 && g <= 255 && *p){
		    b = atoi(p);
		  }
		}
	      }
	    }

	    if(r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255){
		struct color_table *closest = NULL;
		int closest_value = 1000000;
		int cv;

		for(ct = color_tbl; ct->rgb; ct++){

		    if(ct->red >= 0 && ct->red <= 255
		       && ct->green >= 0 && ct->green <= 255
		       && ct->blue >= 0 && ct->blue <= 255){
			cv = (ct->red - r) * (ct->red - r) +
			     (ct->green - g) * (ct->green - g) +
			     (ct->blue - b) * (ct->blue - b);
			if(cv < closest_value){
			    closest_value = cv;
			    closest = ct;
			}
		    }
		}

		if(closest)
		  ct = closest;
	    }
	}
    }
    else{
	for(done=0, ct = color_tbl; !done && ct->names; ct++){
	  for(nl = ct->names; !done && nl; nl = nl->next)
	    if(nl->name && !struncmp(nl->name, s, nl->namelen))
	      done++;
	  
	  if(done)
	    break;
	}
    }
    
    if(ct->names)
      return(ct->val);
    else
      return(-1);
}


void
free_color_table(struct color_table **ctbl)
{
    struct color_table *t;

    if(ctbl && *ctbl){
	for(t = *ctbl; t->names; t++){
	    free_color_name_list(&t->names);

	    if(t->rgb)
	      fs_give((void **) &t->rgb);
	}
	
	fs_give((void **) ctbl);
    }
}


int
pico_count_in_color_table(void)
{
    return(
      (ANSI_COLOR()
        ? (ANSI8_COLOR() ? 8 : ANSI16_COLOR() ? 16 : 256)
        : _colors)
      + COL_TRANS());
}


void
pico_nfcolor(char *s)
{
    if(_nfcolor)
      fs_give((void **) &_nfcolor);

    if(s){
	size_t len;

	len = strlen(s);
	_nfcolor = (char *) fs_get((len+1) * sizeof(char));
	if(_nfcolor){
	  strncpy(_nfcolor, s, len+1);
	  _nfcolor[len] = '\0';
	}

	if(the_normal_color){
	  strncpy(the_normal_color->fg, _nfcolor, MAXCOLORLEN+1);
	  the_normal_color->fg[MAXCOLORLEN] = '\0';
	}
    }
    else if(the_normal_color)
      free_color_pair(&the_normal_color);
}


void
pico_nbcolor(char *s)
{
    if(_nbcolor)
      fs_give((void **) &_nbcolor);

    if(s){
	size_t len;

	len = strlen(s);
	_nbcolor = (char *) fs_get((len+1) * sizeof(char));
	if(_nbcolor){
	  strncpy(_nbcolor, s, len+1);
	  _nbcolor[len] = '\0';
	}

	if(the_normal_color){
	  strncpy(the_normal_color->bg, _nbcolor, MAXCOLORLEN+1);
	  the_normal_color->bg[MAXCOLORLEN] = '\0';
	}
    }
    else if(the_normal_color)
      free_color_pair(&the_normal_color);
}

void
pico_rfcolor(char *s)
{
    if(_rfcolor)
      fs_give((void **) &_rfcolor);

    if(s){
	size_t len;

	len = strlen(s);
	_rfcolor = (char *) fs_get((len+1) * sizeof(char));
	if(_rfcolor){
	  strncpy(_rfcolor, s, len+1);
	  _rfcolor[len] = '\0';
	}

	if(the_rev_color){
	  strncpy(the_rev_color->fg, _rfcolor, MAXCOLORLEN+1);
	  the_rev_color->fg[MAXCOLORLEN] = '\0';
	}
    }
    else if(the_rev_color)
      free_color_pair(&the_rev_color);
}

void
pico_rbcolor(char *s)
{
    if(_rbcolor)
      fs_give((void **) &_rbcolor);

    if(s){
	size_t len;

	len = strlen(s);
	_rbcolor = (char *) fs_get((len+1) * sizeof(char));
	if(_rbcolor){
	  strncpy(_rbcolor, s, len+1);
	  _rbcolor[len] = '\0';
	}

	if(the_rev_color){
	  strncpy(the_rev_color->bg, _rbcolor, MAXCOLORLEN+1);
	  the_rev_color->bg[MAXCOLORLEN] = '\0';
	}
    }
    else if(the_rev_color)
      free_color_pair(&the_rev_color);
}


int
pico_hascolor(void)
{
    if(!_color_inited)
      tinitcolor();

    return(_color_inited);
}


int
pico_usingcolor(void)
{
    return(_using_color && pico_hascolor());
}


/*
 * This should only be called when we're using the
 * unix termdef color, as opposed to the ANSI defined
 * color stuff or the Windows stuff.
 */
int
pico_trans_color(void)
{
    return(_bce && _op);
}


void
pico_toggle_color(int on)
{
    if(on){
	if(pico_hascolor())
	  _using_color = 1;
    }
    else{
	_using_color = 0;
	if(_color_inited){
	    _color_inited = 0;
	    if(!panicking())
	      free_color_table(&color_tbl);

	    if(ANSI_COLOR())
	      putpad("\033[39;49m");
	    else{
		if(_op)
		  putpad(_op);
		if(_oc)
		  putpad(_oc);
	    }
	}
    }
}


unsigned
pico_get_color_options(void)
{
    return(color_options);
}


int
pico_trans_is_on(void)
{
    return(COL_TRANS());
}


/*
 * Absolute set of options. Caller has to OR things together and so forth.
 */
void
pico_set_color_options(unsigned flags)
{
    color_options = flags;
}

void
pico_endcolor(void)
{
    pico_toggle_color(0);
    if(panicking())
      return;

    if(_nfcolor)
      fs_give((void **) &_nfcolor);

    if(_nbcolor)
      fs_give((void **) &_nbcolor);

    if(_rfcolor)
      fs_give((void **) &_rfcolor);

    if(_rbcolor)
      fs_give((void **) &_rbcolor);

    if(_last_fg_color)
      fs_give((void **) &_last_fg_color);

    if(_last_bg_color)
      fs_give((void **) &_last_bg_color);

    if(the_rev_color)
      free_color_pair(&the_rev_color);

    if(the_normal_color)
      free_color_pair(&the_normal_color);
}


void
pico_set_nfg_color(void)
{
    if(_nfcolor)
      (void)pico_set_fg_color(_nfcolor);
}


void
pico_set_nbg_color(void)
{
    if(_nbcolor)
      (void)pico_set_bg_color(_nbcolor);
}


void
pico_set_normal_color(void)
{
    if(!_nfcolor || !_nbcolor ||
       !pico_set_fg_color(_nfcolor) || !pico_set_bg_color(_nbcolor)){
	(void)pico_set_fg_color(DEFAULT_NORM_FORE_RGB);
	(void)pico_set_bg_color(DEFAULT_NORM_BACK_RGB);
    }
}


/*
 * If inverse is a color, returns a pointer to that color.
 * If not, returns NULL.
 *
 * NOTE: Don't free this!
 */
COLOR_PAIR *
pico_get_rev_color(void)
{
    if(pico_usingcolor() && _rfcolor && _rbcolor &&
       pico_is_good_color(_rfcolor) && pico_is_good_color(_rbcolor)){
	if(!the_rev_color)
	  the_rev_color = new_color_pair(_rfcolor, _rbcolor);
	
	return(the_rev_color);
    }
    else
      return(NULL);
}


/*
 * Returns a pointer to the normal color.
 *
 * NOTE: Don't free this!
 */
COLOR_PAIR *
pico_get_normal_color(void)
{
    if(pico_usingcolor() && _nfcolor && _nbcolor &&
       pico_is_good_color(_nfcolor) && pico_is_good_color(_nbcolor)){
	if(!the_normal_color)
	  the_normal_color = new_color_pair(_nfcolor, _nbcolor);
	
	return(the_normal_color);
    }
    else
      return(NULL);
}


/*
 * Sets color to (fg,bg).
 * Flags == PSC_NONE  No alternate default if fg,bg fails.
 *       == PSC_NORM  Set it to Normal color on failure.
 *       == PSC_REV   Set it to Reverse color on failure.
 *
 * If flag PSC_RET is set, returns an allocated copy of the previous
 * color pair, otherwise returns NULL.
 */
COLOR_PAIR *
pico_set_colors(char *fg, char *bg, int flags)
{
    int         uc;
    COLOR_PAIR *cp = NULL, *rev = NULL;

    if(flags & PSC_RET)
      cp = pico_get_cur_color();

    if(fg && !strcmp(fg, END_PSEUDO_REVERSE)){
	EndInverse();
	if(cp)
	  free_color_pair(&cp);
    }
    else if(!((uc=pico_usingcolor()) && fg && bg &&
	      pico_set_fg_color(fg) && pico_set_bg_color(bg))){

	if(uc && flags & PSC_NORM)
	  pico_set_normal_color();
	else if(flags & PSC_REV){
	    if((rev = pico_get_rev_color()) != NULL){
		pico_set_fg_color(rev->fg);	/* these will succeed */
		pico_set_bg_color(rev->bg);
	    }
	    else{
		StartInverse();
		if(cp){
		    strncpy(cp->fg, END_PSEUDO_REVERSE, MAXCOLORLEN+1);
		    cp->fg[MAXCOLORLEN] = '\0';
		    strncpy(cp->bg, END_PSEUDO_REVERSE, MAXCOLORLEN+1);
		    cp->bg[MAXCOLORLEN] = '\0';
		}
	    }
	}
    }

    return(cp);
}


int
pico_is_good_color(char *s)
{
    struct color_table *ct;
    struct color_name_list *nl;
    int done;

    if(!s || !color_tbl)
      return(FALSE);

    if(!strcmp(s, END_PSEUDO_REVERSE))
      return(TRUE);
    else if(!struncmp(s, MATCH_NORM_COLOR, RGBLEN) || !struncmp(s, MATCH_NONE_COLOR, RGBLEN))
      return(TRUE);
    else if(*s == ' ' || isdigit(*s)){
	/* check for rgb string instead of name */
	for(ct = color_tbl; ct->rgb; ct++)
	  if(!strncmp(ct->rgb, s, RGBLEN))
	    break;

	/* if no match it's still ok if rgb */
	if(!ct->rgb){
	    int r = -1, g = -1, b = -1;
	    char *p, *comma, scopy[RGBLEN+1];

	    strncpy(scopy, s, sizeof(scopy));
	    scopy[sizeof(scopy)-1] = '\0';

	    p = scopy;
	    comma = strchr(p, ',');
	    if(comma){
	      *comma = '\0';
	      r = atoi(p);
	      p = comma+1;
	      if(r >= 0 && r <= 255 && *p){
		comma = strchr(p, ',');
		if(comma){
		  *comma = '\0';
		  g = atoi(p);
		  p = comma+1;
		  if(g >= 0 && g <= 255 && *p){
		    b = atoi(p);
		  }
		}
	      }
	    }

	    if(r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255)
	      ct = color_tbl;	/* to force TRUE */
	}
    }
    else{
	for(done=0, ct = color_tbl; !done && ct->names; ct++){
	  for(nl = ct->names; !done && nl; nl = nl->next)
	    if(nl->name && !struncmp(nl->name, s, nl->namelen))
	      done++;
	  
	  if(done)
	    break;
	}
    }
    
    return(ct->names ? TRUE : FALSE);
}


/*
 * Return TRUE on success.
 */
int
pico_set_fg_color(char *s)
{
    int val;

    if(!s || !color_tbl)
      return(FALSE);

    if(!strcmp(s, END_PSEUDO_REVERSE)){
	EndInverse();
	return(TRUE);
    }

    if(!struncmp(s, MATCH_NORM_COLOR, RGBLEN))
      s = _nfcolor;
    else if(!struncmp(s, MATCH_NONE_COLOR, RGBLEN))
      return(TRUE);

    if((val = color_to_val(s)) >= 0){
	size_t len;
	int changed;

	changed = !_last_fg_color || strcmp(_last_fg_color,colorx(val));

	/* already set correctly */
	if(!_force_fg_color_change && !changed)
	  return(TRUE);

	_force_fg_color_change = 0;

	if(changed){
	    if(_last_fg_color)
	      fs_give((void **) &_last_fg_color);
	    
	    len = strlen(colorx(val));
	    if((_last_fg_color = (char *) fs_get((len+1) * sizeof(char))) != NULL){
	      strncpy(_last_fg_color, colorx(val), len+1);
	      _last_fg_color[len] = '\0';
	    }
	}

	tfgcolor(val);
	return(TRUE);
    }
    else
      return(FALSE);
}


int
pico_set_bg_color(char *s)
{
    int val;

    if(!s || !color_tbl)
      return(FALSE);

    if(!strcmp(s, END_PSEUDO_REVERSE)){
	EndInverse();
	return(TRUE);
    }

    if(!struncmp(s, MATCH_NORM_COLOR, RGBLEN))
      s = _nbcolor;
    else if(!struncmp(s, MATCH_NONE_COLOR, RGBLEN))
      return(TRUE);

    if((val = color_to_val(s)) >= 0){
	size_t len;
	int changed;

	changed = !_last_bg_color || strcmp(_last_bg_color,colorx(val));

	/* already set correctly */
	if(!_force_bg_color_change && !changed)
	  return(TRUE);

	_force_bg_color_change = 0;

	if(changed){
	    if(_last_bg_color)
	      fs_give((void **) &_last_bg_color);
	    
	    len = strlen(colorx(val));
	    if((_last_bg_color = (char *) fs_get((len+1) * sizeof(char))) != NULL){
	      strncpy(_last_bg_color, colorx(val), len+1);
	      _last_bg_color[len] = '\0';
	    }
	}

	tbgcolor(val);
	return(TRUE);
    }
    else
      return(FALSE);
}


/*
 * Return a pointer to an rgb string for the input color. The output is 11
 * characters long and looks like rrr,ggg,bbb.
 *
 * Args    colorName -- The color to convert to ascii rgb.
 *
 * Returns  Pointer to a static buffer containing the rgb string. Can use up
 * to three returned values at once before the first is overwritten.
 */
char *
color_to_asciirgb(char *colorName)
{
    static char c_to_a_buf[3][RGBLEN+1];
    static int whichbuf = 0;
    struct color_table *ct;
    struct color_name_list *nl;
    int done;

    whichbuf = (whichbuf + 1) % 3;

    c_to_a_buf[whichbuf][0] = '\0';

    for(done=0, ct = color_tbl; !done && ct->names; ct++){
      for(nl = ct->names; !done && nl; nl = nl->next)
	if(nl->name && !struncmp(nl->name, colorName, nl->namelen))
	  done++;
      
      if(done)
	break;
    }
    
    if(ct && ct->names){
	strncpy(c_to_a_buf[whichbuf], ct->rgb, sizeof(c_to_a_buf[0]));
	c_to_a_buf[whichbuf][sizeof(c_to_a_buf[0])-1] = '\0';
    }
    else if(*colorName == ' ' || isdigit(*colorName)){
	/* check for rgb string instead of name */
	for(ct = color_tbl; ct->rgb; ct++)
	  if(!strncmp(ct->rgb, colorName, RGBLEN))
	    break;

	/* if no match it's still ok if rgb */
	if(!ct->rgb){
	    int r = -1, g = -1, b = -1;
	    char *p, *comma, scopy[RGBLEN+1];

	    strncpy(scopy, colorName, sizeof(scopy));
	    scopy[sizeof(scopy)-1] = '\0';

	    p = scopy;
	    comma = strchr(p, ',');
	    if(comma){
	      *comma = '\0';
	      r = atoi(p);
	      p = comma+1;
	      if(r >= 0 && r <= 255 && *p){
		comma = strchr(p, ',');
		if(comma){
		  *comma = '\0';
		  g = atoi(p);
		  p = comma+1;
		  if(g >= 0 && g <= 255 && *p){
		    b = atoi(p);
		  }
		}
	      }
	    }

	    if(r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255){
		/* it was already RGB */
		snprintf(c_to_a_buf[whichbuf], sizeof(c_to_a_buf[0]), "%3.3d,%3.3d,%3.3d", r, g, b);
	    }
	}
	else{
	    strncpy(c_to_a_buf[whichbuf], ct->rgb, sizeof(c_to_a_buf[0]));
	    c_to_a_buf[whichbuf][sizeof(c_to_a_buf[0])-1] = '\0';
	}
    }

    if(!c_to_a_buf[whichbuf][0]){
	int l;

	/*
	 * If we didn't find the color it could be that it is the
	 * normal color (MATCH_NORM_COLOR) or the none color
	 * (MATCH_NONE_COLOR). If that is the case, this strncpy thing
	 * will work out correctly because those two strings are
	 * RGBLEN long. Otherwise we're in a bit of trouble. This
	 * most likely means that the user is using the same pinerc on
	 * two terminals, one with more colors than the other. We didn't
	 * find a match because this color isn't present on this terminal.
	 * Since the return value of this function is assumed to be
	 * RGBLEN long, we'd better make it that long.
	 * It still won't work correctly because colors will be screwed up,
	 * but at least the embedded colors in filter.c will get properly
	 * sucked up when they're encountered.
	 */
	strncpy(c_to_a_buf[whichbuf], "xxxxxxxxxxx", RGBLEN);  /* RGBLEN is 11 */
	l = strlen(colorName);
	strncpy(c_to_a_buf[whichbuf], colorName, (l < RGBLEN) ? l : RGBLEN);
	c_to_a_buf[whichbuf][RGBLEN] = '\0';
    }
    
    return(c_to_a_buf[whichbuf]);
}


char *
pico_get_last_fg_color(void)
{
    char *ret = NULL;

    if(_last_fg_color){
      size_t len;

      len = strlen(_last_fg_color);
      if((ret = (char *) fs_get((len+1) * sizeof(char))) != NULL){
	strncpy(ret, _last_fg_color, len+1);
	ret[len] = '\0';
      }
    }

    return(ret);
}


char *
pico_get_last_bg_color(void)
{
    char *ret = NULL;

    if(_last_bg_color){
      size_t len;

      len = strlen(_last_bg_color);
      if((ret = (char *) fs_get((len+1) * sizeof(char))) != NULL){
	strncpy(ret, _last_bg_color, len+1);
	ret[len] = '\0';
      }
    }

    return(ret);
}


COLOR_PAIR *
pico_get_cur_color(void)
{
    return(new_color_pair(_last_fg_color, _last_bg_color));
}

#else  /* _WINDOWS */
static short _in_inverse, _in_bold, _in_uline;

int
pico_trans_is_on(void)
{
    return(0);
}

void
StartInverse()
{
    if(!_in_inverse)
      mswin_rev(_in_inverse = 1);
}

void
EndInverse()
{
    if(_in_inverse)
      mswin_rev(_in_inverse = 0);
}

int
InverseState()
{
    return(_in_inverse);
}

void
StartUnderline()
{
    if(!_in_uline)
      mswin_uline(_in_uline = 1);
}

void
EndUnderline()
{
    if(_in_uline)
      mswin_uline(_in_uline = 0);
}

int
StartBold()
{
    if(!_in_bold)
      mswin_bold(_in_bold = 1);

    return(1);
}

void
EndBold()
{
    if(_in_bold)
      mswin_bold(_in_bold = 0);
}


#endif /* _WINDOWS */
