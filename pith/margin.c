#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: margin.c 1032 2008-04-11 00:30:04Z hubert@u.washington.edu $";
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

/*======================================================================

     margin.c
     Implements message data gathering and formatting

  ====*/


#include "../pith/headers.h"
#include "../pith/string.h"
#include "../pith/state.h"
#include "../pith/conf.h"


/*
 * Internal prototypes
 */


/*
 * format_view_margin - return sane value for vertical margins
 */
int *
format_view_margin(void)
{
    static int margin[2];
    char tmp[100], e[200], *err, lastchar = 0;
    int left = 0, right = 0, leftm = 0, rightm = 0;
    size_t l;

    memset(margin, 0, sizeof(margin));

    /*
     * We initially tried to make sure that the user didn't shoot themselves
     * in the foot by setting this too small. People seem to want to do some
     * strange stuff, so we're going to relax the wild shot detection and
     * let people set what they want, until we get to the point of totally
     * absurd. We've also added the possibility of appending the letter c
     * onto the width. That means treat the value as an absolute column
     * instead of a width. That is, a right margin of 76c means wrap at
     * column 76, whereas right margin of 4 means to wrap at column
     * screen width - 4.
     */
    if(ps_global->VAR_VIEW_MARGIN_LEFT){
	strncpy(tmp, ps_global->VAR_VIEW_MARGIN_LEFT, sizeof(tmp)-1);
	tmp[sizeof(tmp)-1] = '\0';
	removing_leading_and_trailing_white_space(tmp);
	if(tmp[0]){
	    l = strlen(tmp);
	    if(l > 0)
	      lastchar = tmp[l-1];

	    if(lastchar == 'c')
	      tmp[l-1] = '\0';
	    
	    if((err = strtoval(tmp, &left, 0, 0, 0, e, sizeof(e), "Viewer-margin-left")) != NULL){
		leftm = 0;
		dprint((2, "%s\n", err));
	    }
	    else{
		if(lastchar == 'c')
		  leftm = left-1;
		else
		  leftm = left;
		
		leftm = MIN(MAX(0, leftm), ps_global->ttyo->screen_cols);
	    }
	}
    }

    if(ps_global->VAR_VIEW_MARGIN_RIGHT){
	strncpy(tmp, ps_global->VAR_VIEW_MARGIN_RIGHT, sizeof(tmp)-1);
	tmp[sizeof(tmp)-1] = '\0';
	removing_leading_and_trailing_white_space(tmp);
	if(tmp[0]){
	    l = strlen(tmp);
	    if(l > 0)
	      lastchar = tmp[l-1];

	    if(lastchar == 'c')
	      tmp[l-1] = '\0';
	    
	    if((err = strtoval(tmp, &right, 0, 0, 0, e, sizeof(e), "Viewer-margin-right")) != NULL){
		rightm = 0;
		dprint((2, "%s\n", err));
	    }
	    else{
		if(lastchar == 'c')
		  rightm = ps_global->ttyo->screen_cols - right;
		else
		  rightm = right;
		
		rightm = MIN(MAX(0, rightm), ps_global->ttyo->screen_cols);
	    }
	}
    }

    if((rightm > 0 || leftm > 0) && rightm >= 0 && leftm >= 0
       && ps_global->ttyo->screen_cols - rightm - leftm >= 8){
	margin[0] = leftm;
	margin[1] = rightm;
    }

    return(margin);
}


/*
 * Give a margin for help and such
 */
int *
non_messageview_margin(void)
{
    static int margin[2];

    margin[0] = 0;
    margin[1] = 4;

    return(margin);
}
