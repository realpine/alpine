#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: color.c 270 2006-11-27 21:18:06Z mikes@u.washington.edu $";
#endif

/* ========================================================================
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

#include "../../../pith/osdep/color.h"


static COLOR_PAIR *the_rev_color, *the_normal_color;
static char   *_nfcolor, *_nbcolor, *_rfcolor, *_rbcolor;
static char   *_last_fg_color, *_last_bg_color;
static int     _force_fg_color_change, _force_bg_color_change;


/* * * * * * * PITH-REQUIRED COLOR ROUTINES * * * * * * */

/* internal prototypes */
char	*alpine_color_name(char *);
int	 alpine_valid_rgb(char *s);

int
pico_usingcolor(void)
{
    return(TRUE);
}


int
pico_hascolor(void)
{
    return(TRUE);
}


/*
 * Web Alpine Color Table
 */
static struct color_table {
    int		 number;
    char	*rgb;
    struct {
	char	*s,
		 l;
    } name;
} webcoltab[] = {
    {COL_BLACK,		"  0,  0,  0",	{"black",	5}},
    {COL_RED,		"255,  0,  0",	{"red",		3}},
    {COL_GREEN,		"  0,255,  0",	{"green",	5}},
    {COL_YELLOW,	"255,255,  0",	{"yellow",	6}},
    {COL_BLUE,		"  0,  0,255",	{"blue",	4}},
    {COL_MAGENTA,	"255,  0,255",	{"magenta",	7}},
    {COL_CYAN,		"  0,255,255",	{"cyan",	4}},
    {COL_WHITE,		"255,255,255",	{"white",	5}},
    {8,			"192,192,192",	{"color008",	8}},	/* light gray */
    {9,			"128,128,128",	{"color009",	8}},	/* gray */
    {10,		" 64, 64, 64",	{"color010",	8}},	/* dark gray */
    {8,			"192,192,192",	{"colorlgr",	8}},	/* light gray */
    {9,			"128,128,128",	{"colormgr",	8}},	/* gray */
    {10,		" 64, 64, 64",	{"colordgr",	8}},	/* dark gray */
};


char *
colorx(int color)
{
    int i;
    static char cbuf[12];

    for(i = 0; i < sizeof(webcoltab) / sizeof(struct color_table); i++)
      if(color == webcoltab[i].number)
	return(webcoltab[i].rgb);

    sprintf(cbuf, "color%03.3d", color);
    return(cbuf);
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
    int		i;
    static char c_to_a_buf[RGBLEN+1];

    for(i = 0; i < sizeof(webcoltab) / sizeof(struct color_table); i++)
      if(!strucmp(webcoltab[i].name.s, colorName))
	return(webcoltab[i].rgb);
	  
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
    i = strlen(colorName);
    strncpy(c_to_a_buf, colorName, (i < RGBLEN) ? i : RGBLEN);
    c_to_a_buf[RGBLEN] = '\0';
    return(c_to_a_buf);
}



int
pico_is_good_color(char *s)
{
    return(alpine_color_name(s) != NULL || alpine_valid_rgb(s));
}


int
alpine_valid_rgb(char *s)
{
    int i, j;

    /* has to be three spaces or decimal digits followed by a dot.*/

    for(i = 0; i < 3; i++){
	int w = 0, n = 0;

	for(j = 0; j < 3; j++, s++) {
	    if(*s == ' '){
		if(n)
		  return(FALSE);
	    }
	    else if(isdigit((unsigned char) *s)){
		n = (n * 10) + (*s - '0');
	    }
	    else
	      return(FALSE);
	}

	if (i < 2 && *s++ != ',')
	  return(FALSE);
    }

    return (TRUE);
}



char *
alpine_color_name(char *s)
{
    if(s){
	int i;

	if(*s == ' ' || isdigit(*s)){
	    /* check for rgb string instead of name */
	    for(i = 0; i < sizeof(webcoltab) / sizeof(struct color_table); i++)
	      if(!strncmp(webcoltab[i].rgb, s, RGBLEN))
		return(webcoltab[i].name.s);
	}
	else{
	    for(i = 0; i < sizeof(webcoltab) / sizeof(struct color_table); i++)
	      if(!struncmp(webcoltab[i].name.s, s, webcoltab[i].name.l))
		return(webcoltab[i].name.s);
	}
    }

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

    if(!((uc = pico_usingcolor())
	 && fg && bg
	 && pico_set_fg_color(fg) && pico_set_bg_color(bg))){

	if(uc && flags & PSC_NORM){
	    pico_set_normal_color();
	}
	else if(flags & PSC_REV){
	    if(rev = pico_get_rev_color()){
		pico_set_fg_color(rev->fg);	/* these will succeed */
		pico_set_bg_color(rev->bg);
	    }
	}
    }

    return(cp);
}



void
pico_nfcolor(char *s)
{
    if(_nfcolor){
	free(_nfcolor);
	_nfcolor = NULL;
    }

    if(s){
	_nfcolor = (char *)malloc(strlen(s)+1);
	if(_nfcolor)
	  strcpy(_nfcolor, s);
    }
}


void
pico_nbcolor(char *s)
{
    if(_nbcolor){
	free(_nbcolor);
	_nbcolor = NULL;
    }

    if(s){
	_nbcolor = (char *)malloc(strlen(s)+1);
	if(_nbcolor)
	  strcpy(_nbcolor, s);
    }
}

void
pico_rfcolor(char *s)
{
    if(_rfcolor){
	free(_rfcolor);
	_rfcolor = NULL;
    }

    if(s){
	_rfcolor = (char *)malloc(strlen(s)+1);
	if(_rfcolor)
	  strcpy(_rfcolor, s);
	
	if(the_rev_color)
	  strcpy(the_rev_color->fg, _rfcolor);
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
	_rbcolor = (char *)malloc(strlen(s)+1);
	if(_rbcolor)
	  strcpy(_rbcolor, s);
	
	if(the_rev_color)
	  strcpy(the_rev_color->bg, _rbcolor);
    }
    else if(the_rev_color)
      free_color_pair(&the_rev_color);
}


void
pico_endcolor(void)
{
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

    if(the_rev_color)
      free_color_pair(&the_rev_color);
}


COLOR_PAIR *
pico_get_cur_color(void)
{
    return(new_color_pair(_last_fg_color, _last_bg_color));
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


int
pico_set_fg_color(char *s)
{
    if(pico_is_good_color(s)){
	/* already set correctly */
	if(!_force_fg_color_change
	   && _last_fg_color
	   && !strcmp(_last_fg_color, s))
	  return(TRUE);

	_force_fg_color_change = 0;
	if(_last_fg_color)
	  free(_last_fg_color);

	if(_last_fg_color = (char *) malloc(strlen(s) + 1))
	  strcpy(_last_fg_color, s);

	return(TRUE);
    }

    return(FALSE);
}


int
pico_set_bg_color(char *s)
{
    if(pico_is_good_color(s)){
	/* already set correctly */
	if(!_force_bg_color_change
	   && _last_bg_color
	   && !strcmp(_last_bg_color, s))
	  return(TRUE);

	_force_bg_color_change = 0;
	if(_last_bg_color)
	  free(_last_bg_color);

	if(_last_bg_color = (char *) malloc(strlen(s) + 1))
	  strcpy(_last_bg_color, s);

	return(TRUE);
    }

    return(FALSE);
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


char *
pico_get_last_fg_color(void)
{
    char *ret = NULL;

    if(_last_fg_color)
      if(ret = (char *)malloc(strlen(_last_fg_color)+1))
	strcpy(ret, _last_fg_color);

    return(ret);
}

char *
pico_get_last_bg_color(void)
{
    char *ret = NULL;

    if(_last_bg_color)
      if(ret = (char *)malloc(strlen(_last_bg_color)+1))
	strcpy(ret, _last_bg_color);

    return(ret);
}
