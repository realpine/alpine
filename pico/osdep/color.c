#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: color.c 320 2006-12-12 22:40:05Z hubert@u.washington.edu $";
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


#include <system.h>
#include <general.h>

#include "../../pith/osdep/color.h"

#include "../estruct.h"

#include "terminal.h"
#include "color.h"

#ifndef	_WINDOWS

struct color_table {
    char *name;
    char *canonical_name;
    int   namelen;
    char *rgb;
    int   val;
};


/* useful definitions */
#define	ANSI8_COLOR()	(color_options & COLOR_ANSI8_OPT)
#define	ANSI16_COLOR()	(color_options & COLOR_ANSI16_OPT)
#define	ANSI256_COLOR()	(color_options & COLOR_ANSI256_OPT)
#define	ANSI_COLOR()	(color_options & (COLOR_ANSI8_OPT | COLOR_ANSI16_OPT | COLOR_ANSI256_OPT))
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
extern int	_colors;


/* internal prototypes */
void     flip_rev_color(int);
void     flip_bold(int);
void     flip_inv(int);
void     flip_ul(int);
void     reset_attr_state(void);


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
    
    if((ANSI8_COLOR()  && (color < 0 || color >= 8)) ||
       (ANSI16_COLOR() && (color < 0 || color >= 16)) ||
       (ANSI256_COLOR() && (color < 0 || color >= 256)) ||
       (!ANSI_COLOR()  && (color < 0 || color >= _colors)))
      return(-1);

    if(ANSI_COLOR()){
	char buf[20];

	if(ANSI256_COLOR())
	  snprintf(buf, sizeof(buf), "\033[38;5;%dm", color);
	else{
	    if(color < 8)
	      snprintf(buf, sizeof(buf), "\033[3%cm", color + '0');
	    else
	      snprintf(buf, sizeof(buf), "\033[9%cm", (color-8) + '0');
	}
	
	putpad(buf);
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
    
    if((ANSI8_COLOR()  && (color < 0 || color >= 8)) ||
       (ANSI16_COLOR() && (color < 0 || color >= 16)) ||
       (ANSI256_COLOR() && (color < 0 || color >= 256)) ||
       (!ANSI_COLOR()  && (color < 0 || color >= _colors)))
      return(-1);

    if(ANSI_COLOR()){
	char buf[20];

	if(ANSI256_COLOR())
	  snprintf(buf, sizeof(buf), "\033[48;5;%dm", color);
	else{
	    if(color < 8)
	      snprintf(buf, sizeof(buf), "\033[4%cm",  color + '0');
	    else
	      snprintf(buf, sizeof(buf), "\033[10%cm", (color-8) + '0');
	}
	
	putpad(buf);
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
 * More than one "name" can map to the same "canonical_name".
 * More than one "name" can map to the same "val".
 * The "val" for a "name" and for its "canonical_name" are the same.
 */
struct color_table *
init_color_table(void)
{
    struct color_table *ct = NULL, *t;
    int                 i, size, count;
    char                colorname[12];

    count = pico_count_in_color_table();

    /*
     * Special case: If table is of size 8 we use a table of size 16 instead
     * and map the 2nd eight into the first 8 so that both 8 and 16 color
     * terminals can be used with the same pinerc and colors in the 2nd
     * 8 may be useful. We could make this more general and always map
     * colors larger than our current # of colors by mapping it modulo
     * the color table size. The only problem with this is that it would
     * take a boatload of programming to convert what we have now into that,
     * and it is highly likely that no one would ever use anything other
     * than the 8/16 case. So we'll stick with that special case for now.
     */
    if(count == 8)
      size = 16;
    else
      size = count;

    if(size > 0 && size <= 256){
	char   rgb[256][RGBLEN+1];
	int    ind, graylevel;

	ct = (struct color_table *)malloc((size+1)*sizeof(struct color_table));
	if(ct)
	  memset(ct, 0, (size+1) * sizeof(struct color_table));

	if(size == 256){
	    int red, green, blue, gray;

	    for(red = 0; red < 6; red++)
	      for(green = 0; green < 6; green++)
		for(blue = 0; blue < 6; blue++){
		    ind = 16 + 36*red + 6*green + blue;
		    snprintf(rgb[ind], sizeof(rgb[ind]), "%3.3d,%3.3d,%3.3d",
			     red ? (40*red + 55) : 0,
			     green ? (40*green + 55) : 0,
			     blue ? (40*blue + 55) : 0);
		}

	    for(gray = 0; gray < 24; gray++){
		ind = gray + 232;
		graylevel = 10*gray + 8;
		snprintf(rgb[ind], sizeof(rgb[ind]), "%3.3d,%3.3d,%3.3d",
			 graylevel, graylevel, graylevel);
	    }
	}

	for(i = 0, t = ct; t && i < size; i++, t++){
	    t->val = i % count;

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
		snprintf(colorname, sizeof(colorname), "color%03.3d", i);
		break;
	    }

	    t->namelen = strlen(colorname);
	    t->name = (char *)malloc((t->namelen+1) * sizeof(char));
	    if(t->name){
		strncpy(t->name, colorname, t->namelen+1);
		t->name[t->namelen] = '\0';
	    }

	    t->rgb = (char *)malloc((RGBLEN+1) * sizeof(char));
	    if(t->rgb){
		if(count == 8){
		  switch(i){
		    case COL_BLACK:
		    case 8:
		      strncpy(t->rgb, "000,000,000", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case COL_RED:
		    case 9:
		      strncpy(t->rgb, "255,000,000", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case COL_GREEN:
		    case 10:
		      strncpy(t->rgb, "000,255,000", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case COL_YELLOW:
		    case 11:
		      strncpy(t->rgb, "255,255,000", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case COL_BLUE:
		    case 12:
		      strncpy(t->rgb, "000,000,255", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case COL_MAGENTA:
		    case 13:
		      strncpy(t->rgb, "255,000,255", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case COL_CYAN:
		    case 14:
		      strncpy(t->rgb, "000,255,255", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case COL_WHITE:
		    case 15:
		      strncpy(t->rgb, "255,255,255", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    default:
		      /*
		       * These aren't really used as RGB values, just as
		       * strings for the lookup. We don't know how to
		       * convert to any RGB, we just know how to output
		       * the escape sequences. So it doesn't matter that
		       * the numbers in the snprintf below are too big.
		       * We are using the fact that all rgb values start with
		       * a digit or space and color names don't in the lookup
		       * routines below.
		       */
		      snprintf(t->rgb, RGBLEN+1, "%d,%d,%d", 256+i, 256+i, 256+i);
		      break;
		  }
		}
		else if(count == 16 || count == 256){
		  /*
		   * This set of RGB values seems to come close to describing
		   * what a 16-color xterm gives you.
		   */
		  switch(i){
		    case COL_BLACK:
		      strncpy(t->rgb, "000,000,000", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case COL_RED:		/* actually dark red */
		      strncpy(t->rgb, "174,000,000", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case COL_GREEN:		/* actually dark green */
		      strncpy(t->rgb, "000,174,000", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case COL_YELLOW:		/* actually dark yellow */
		      strncpy(t->rgb, "174,174,000", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case COL_BLUE:		/* actually dark blue */
		      strncpy(t->rgb, "000,000,174", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case COL_MAGENTA:		/* actually dark magenta */
		      strncpy(t->rgb, "174,000,174", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case COL_CYAN:		/* actually dark cyan */
		      strncpy(t->rgb, "000,174,174", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case COL_WHITE:		/* actually light gray */
		      strncpy(t->rgb, "174,174,174", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case 8:			/* dark gray */
		      strncpy(t->rgb, "064,064,064", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case 9:			/* red */
		      strncpy(t->rgb, "255,000,000", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case 10:			/* green */
		      strncpy(t->rgb, "000,255,000", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case 11:			/* yellow */
		      strncpy(t->rgb, "255,255,000", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case 12:			/* blue */
		      strncpy(t->rgb, "000,000,255", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case 13:			/* magenta */
		      strncpy(t->rgb, "255,000,255", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case 14:			/* cyan */
		      strncpy(t->rgb, "000,255,255", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case 15:			/* white */
		      strncpy(t->rgb, "255,255,255", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    default:
		      strncpy(t->rgb, rgb[i], RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		  }
		}
		else{
		  switch(i){
		    case COL_BLACK:
		      strncpy(t->rgb, "000,000,000", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case COL_RED:
		      strncpy(t->rgb, "255,000,000", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case COL_GREEN:
		      strncpy(t->rgb, "000,255,000", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case COL_YELLOW:
		      strncpy(t->rgb, "255,255,000", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case COL_BLUE:
		      strncpy(t->rgb, "000,000,255", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case COL_MAGENTA:
		      strncpy(t->rgb, "255,000,255", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case COL_CYAN:
		      strncpy(t->rgb, "000,255,255", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    case COL_WHITE:
		      strncpy(t->rgb, "255,255,255", RGBLEN+1);
		      t->rgb[RGBLEN] = '\0';
		      break;
		    default:
		      snprintf(t->rgb, RGBLEN+1, "%d,%d,%d", 256+i, 256+i, 256+i);
		      break;
		  }
		}
	    }

	    /*
	     * For an 8 color terminal, canonical black == black, but
	     * canonical red == color009, etc.
	     *
	     * This is weird. What we have is 16-color xterms where color 0
	     * is black (fine, that matches) colors 1 - 7 are called
	     * red, ..., white but they are set at 2/3rds brightness so that
	     * bold can be brighter. Those 7 colors suck. Color 8 is some
	     * sort of gray, colors 9 - 15 are real red, ..., white. On other
	     * 8 color terminals and in PC-Pine, we want to equate color 0
	     * with color 0 on 16-color, but color 1 (red) is really the
	     * same as the 16-color color 9. So color 9 - 15 are the real
	     * colors on a 16-color terminal and colors 1 - 7 are the real
	     * colors on an 8-color terminal. We make that work by mapping
	     * the 2nd eight into the first eight when displaying on an
	     * 8-color terminal, and by using the canonical_name when
	     * we modify and write out a color. The canonical name is set
	     * to the "real* color (0, 9, 10, ..., 15 on 16-color terminal).
	     */
	    if(size == 256){
		/*
		 * Is this a good idea?
		 * We use RGB to try to interoperate with PC Alpine.
		 */
		t->canonical_name = (char *)malloc((RGBLEN+1)*sizeof(char));
		strncpy(t->canonical_name, t->rgb, RGBLEN+1);
		t->canonical_name[RGBLEN] = '\0';

		/*
		 * This is an ugly hack to recognize PC-Alpine's
		 * three gray colors and display them.
		 */
		if(i == 250 || i == 244 || i == 238){
		    switch(i){
		      case 250:
			strncpy(t->name, "colorlgr", t->namelen+1);
			t->name[t->namelen] = '\0';
			break;
		      case 244:
			strncpy(t->name, "colormgr", t->namelen+1);
			t->name[t->namelen] = '\0';
			break;
		      case 238:
			strncpy(t->name, "colordgr", t->namelen+1);
			t->name[t->namelen] = '\0';
			break;
		    }
		}
	    }
	    else if(size == count || i == 0 || i > 7){
		t->canonical_name = (char *)malloc((t->namelen+1)*sizeof(char));
		strncpy(t->canonical_name, colorname, t->namelen+1);
		t->canonical_name[t->namelen] = '\0';
	    }
	    else{
		t->canonical_name = (char *)malloc(9*sizeof(char));
		snprintf(t->canonical_name, 9, "color%03.3d", i+8);
	    }

	}
    }

    return(ct);
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
	    snprintf(cbuf, sizeof(cbuf), "color%03.3d", color);
	    return(cbuf);
	}
    }
    
    for(ct = color_tbl; ct->name; ct++)
      if(ct->val == color)
        break;

    if(ct->name)
      return(ct->canonical_name);

    /* not supposed to get here */
    snprintf(cbuf, sizeof(cbuf), "color%03.3d", color);
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

    if(!s || !color_tbl)
      return(NULL);
    
    if(*s == ' ' || isdigit(*s)){
	/* check for rgb string instead of name */
	for(ct = color_tbl; ct->rgb; ct++)
	  if(!strncmp(ct->rgb, s, RGBLEN))
	    break;
    }
    else{
	for(ct = color_tbl; ct->name; ct++)
	  if(!struncmp(ct->name, s, ct->namelen))
	    break;
    }
    
    if(ct->name)
      return(ct->canonical_name);
    
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
	    char *p, *comma, scopy[2*RGBLEN];

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
		int tr, tg, tb;
		int cv;

		for(ct = color_tbl; ct->rgb; ct++){

		    strncpy(scopy, ct->rgb, sizeof(scopy));
		    scopy[sizeof(scopy)-1] = '\0';

		    p = scopy;
		    comma = strchr(p, ',');
		    if(comma){
		      *comma = '\0';
		      tr = atoi(p);
		      p = comma+1;
		      if(tr >= 0 && tr <= 255 && *p){
			comma = strchr(p, ',');
			if(comma){
			  *comma = '\0';
			  tg = atoi(p);
			  p = comma+1;
			  if(tg >= 0 && tg <= 255 && *p){
			    tb = atoi(p);
			  }
			}
		      }
		    }

		    if(tr >= 0 && tr <= 255 && tg >= 0 && tg <= 255 && tb >= 0 && tb <= 255){
			cv = (tr - r) * (tr - r) + (tg - g) * (tg - g) + (tb - b) * (tb - b);
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
	for(ct = color_tbl; ct->name; ct++)
	  if(!struncmp(ct->name, s, ct->namelen))
	    break;
    }
    
    if(ct->name)
      return(ct->val);
    else
      return(-1);
}


void
free_color_table(struct color_table **ctbl)
{
    struct color_table *t;

    if(ctbl && *ctbl){
	for(t = *ctbl; t->name; t++){
	    if(t->name){
		free(t->name);
		t->name = NULL;
	    }

	    if(t->canonical_name){
		free(t->canonical_name);
		t->canonical_name = NULL;
	    }

	    if(t->rgb){
		free(t->rgb);
		t->rgb = NULL;
	    }
	}
	
	free(*ctbl);
	*ctbl = NULL;
    }
}


int
pico_count_in_color_table(void)
{
    return(ANSI8_COLOR() ? 8 : ANSI16_COLOR() ? 16 : ANSI256_COLOR() ? 256 : _colors);
}


void
pico_nfcolor(char *s)
{
    if(_nfcolor){
	free(_nfcolor);
	_nfcolor = NULL;
    }

    if(s){
	size_t len;

	len = strlen(s);
	_nfcolor = (char *)malloc((len+1) * sizeof(char));
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
    if(_nbcolor){
	free(_nbcolor);
	_nbcolor = NULL;
    }

    if(s){
	size_t len;

	len = strlen(s);
	_nbcolor = (char *)malloc((len+1) * sizeof(char));
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
    if(_rfcolor){
	free(_rfcolor);
	_rfcolor = NULL;
    }

    if(s){
	size_t len;

	len = strlen(s);
	_rfcolor = (char *)malloc((len+1) * sizeof(char));
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
    if(_rbcolor){
	free(_rbcolor);
	_rbcolor = NULL;
    }

    if(s){
	size_t len;

	len = strlen(s);
	_rbcolor = (char *)malloc((len+1) * sizeof(char));
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

    if(_nfcolor){
	free(_nfcolor);
	_nfcolor = NULL;
    }

    if(_nbcolor){
	free(_nbcolor);
	_nbcolor = NULL;
    }

    if(_rfcolor){
	free(_rfcolor);
	_rfcolor = NULL;
    }

    if(_rbcolor){
	free(_rbcolor);
	_rbcolor = NULL;
    }

    if(_last_fg_color){
	free(_last_fg_color);
	_last_fg_color = NULL;
    }

    if(_last_bg_color){
	free(_last_bg_color);
	_last_bg_color = NULL;
    }

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
	(void)pico_set_fg_color(colorx(DEFAULT_NORM_FORE));
	(void)pico_set_bg_color(colorx(DEFAULT_NORM_BACK));
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
	    if(rev = pico_get_rev_color()){
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

    if(!s || !color_tbl)
      return(FALSE);

    if(!strcmp(s, END_PSEUDO_REVERSE))
      return(TRUE);
    else if(*s == ' ' || isdigit(*s)){
	/* check for rgb string instead of name */
	for(ct = color_tbl; ct->rgb; ct++)
	  if(!strncmp(ct->rgb, s, RGBLEN))
	    break;
    }
    else{
	for(ct = color_tbl; ct->name; ct++)
	  if(!struncmp(ct->name, s, ct->namelen))
	    break;
    }
    
    return(ct->name ? TRUE : FALSE);
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

    if((val = color_to_val(s)) >= 0){
	size_t len;

	/* already set correctly */
	if(!_force_fg_color_change && _last_fg_color &&
	   !strcmp(_last_fg_color,colorx(val)))
	  return(TRUE);

	_force_fg_color_change = 0;
	if(_last_fg_color){
	    free(_last_fg_color);
	    _last_fg_color = NULL;
	}
	
	len = strlen(colorx(val));
	if(_last_fg_color = (char *)malloc((len+1) * sizeof(char))){
	  strncpy(_last_fg_color, colorx(val), len+1);
	  _last_fg_color[len] = '\0';
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

    if((val = color_to_val(s)) >= 0){
	size_t len;

	/* already set correctly */
	if(!_force_bg_color_change && _last_bg_color &&
	   !strcmp(_last_bg_color,colorx(val)))
	  return(TRUE);

	_force_bg_color_change = 0;
	if(_last_bg_color){
	    free(_last_bg_color);
	    _last_bg_color = NULL;
	}
	
	len = strlen(colorx(val));
	if(_last_bg_color = (char *)malloc((len+1) * sizeof(char))){
	  strncpy(_last_bg_color, colorx(val), len+1);
	  _last_bg_color[len] = '\0';
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
 * Returns  Pointer to a static buffer containing the rgb string.
 */
char *
color_to_asciirgb(char *colorName)
{
    static char c_to_a_buf[RGBLEN+1];

    struct color_table *ct;

    for(ct = color_tbl; ct && ct->name; ct++)
      if(!strucmp(ct->name, colorName))
	break;
    
    if(ct && ct->name){
	strncpy(c_to_a_buf, ct->rgb, sizeof(c_to_a_buf));
	c_to_a_buf[sizeof(c_to_a_buf)-1] = '\0';
    }
    else{
	int l;

	/*
	 * If we didn't find the color we're in a bit of trouble. This
	 * most likely means that the user is using the same pinerc on
	 * two terminals, one with more colors than the other. We didn't
	 * find a match because this color isn't present on this terminal.
	 * Since the return value of this function is assumed to be
	 * RGBLEN long, we'd better make it that long.
	 * It still won't work correctly because colors will be screwed up,
	 * but at least the embedded colors in filter.c will get properly
	 * sucked up when they're encountered.
	 */
	strncpy(c_to_a_buf, "xxxxxxxxxxx", RGBLEN);  /* RGBLEN is 11 */
	l = strlen(colorName);
	strncpy(c_to_a_buf, colorName, (l < RGBLEN) ? l : RGBLEN);
	c_to_a_buf[RGBLEN] = '\0';
    }
    
    return(c_to_a_buf);
}


char *
pico_get_last_fg_color(void)
{
    char *ret = NULL;

    if(_last_fg_color){
      size_t len;

      len = strlen(_last_fg_color);
      if(ret = (char *)malloc((len+1) * sizeof(char))){
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
      if(ret = (char *)malloc((len+1) * sizeof(char))){
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
