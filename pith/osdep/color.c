#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: color.c 761 2007-10-23 22:35:18Z hubert@u.washington.edu $";
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
    *
    *  These routines themselves aren't necessarily OS-specific, they
    *  are all called from within pico, pine and webpine.
    * 
    *  They used to be in pico source (osdep/unix, mswin.c), but considering
    *  webpine uses color as well and it should *not* have to be linked
    *  against libpico and considering pico uses these routines but should
    *  not have to link against libpith (and in turn c-client) we put them
    *  in pith/osdep which should only have to link against system libraries
    *  and thus be include freely in all of pine, pico and webpine.
    */  


#include <system.h>
#include "./color.h"



/*
 * new_color_pair - allocate a new color pair structure assigning 
 *                  given foreground and background color strings
 */
COLOR_PAIR *
new_color_pair(char *fg, char *bg)
{
    COLOR_PAIR *ret;

    if((ret = (COLOR_PAIR *) malloc(sizeof(*ret))) != NULL){
	memset(ret, 0, sizeof(*ret));
	if(fg){
	    strncpy(ret->fg, fg, MAXCOLORLEN);
	    ret->fg[MAXCOLORLEN] = '\0';
	}

	if(bg){
	    strncpy(ret->bg, bg, MAXCOLORLEN);
	    ret->bg[MAXCOLORLEN] = '\0';
	}
    }

    return(ret);
}


/*
 * free_color_pair - release resources associated with given
 *                   color pair structure
 */
void
free_color_pair(COLOR_PAIR **cp)
{
    if(cp && *cp){
	free(*cp);
	*cp = NULL;
    }
}


/*
 * Just like pico_set_color except it doesn't set the color, it just
 * returns the value. Assumes def of PSC_NONE, since otherwise we always
 * succeed and don't need to call this.
 */
int
pico_is_good_colorpair(COLOR_PAIR *cp)
{
    return(cp && pico_is_good_color(cp->fg) && pico_is_good_color(cp->bg));
}


COLOR_PAIR *
pico_set_colorp(COLOR_PAIR *col, int flags)
{
    return(pico_set_colors(col ? col->fg : NULL, col ? col->bg : NULL, flags));
}
