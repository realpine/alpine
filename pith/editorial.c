#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: editorial.c 768 2007-10-24 00:10:03Z hubert@u.washington.edu $";
#endif

/* ========================================================================
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

/*======================================================================

     editorial.c
     Implements editorial text insertion/formatting

  ====*/


#include "../pith/headers.h"
#include "../pith/conf.h"
#include "../pith/state.h"
#include "../pith/margin.h"
#include "../pith/filter.h"
#include "../pith/handle.h"
#include "../pith/mailview.h"
#include "../pith/editorial.h"

/*
 * Internal prototypes
 */
int	 quote_editorial(long, char *, LT_INS_S **, void *);


/*
 * Struct to help with editorial comment insertion
 */
#define	EDITORIAL_MAX	128
typedef struct _editorial_s {
    char prefix[EDITORIAL_MAX];
    int	 prelen;
    char postfix[EDITORIAL_MAX];
    int	 postlen;
    int  do_color;
} EDITORIAL_S;


char *
format_editorial(char *s, int width, int flags, HANDLE_S **handlesp, gf_io_t pc)
{
    gf_io_t	 gc;
    int		*margin;
    EDITORIAL_S  es;
    URL_HILITE_S uh;

    /* ASSUMPTION #2,341: All MIME-decoding is done by now */
    gf_set_readc(&gc, s, strlen(s), CharStar, 0);

    margin = format_view_margin();
    if(flags & FM_NOINDENT)
      margin[0] = margin[1] = 0;

    /* safety net */
    if(width - (margin[0] + margin[1]) < 5){
	margin[0] = margin[1] = 0;
	if(width < 5)
	  width = 80;
    }

    width -= (margin[0] + margin[1]);

    if(width > 40){
	width -= 12;

	es.prelen = MAX(2, MIN(margin[0] + 6, sizeof(es.prefix) - 3));
	snprintf(es.prefix, sizeof(es.prefix), "%s[ ", repeat_char(es.prelen - 2, ' '));
	es.postlen = 2;
	strncpy(es.postfix, " ]", sizeof(es.postfix));
	es.postfix[sizeof(es.postfix)-1] = '\0';
    }
    else if(width > 20){
	width -= 6;

	es.prelen = MAX(2, MIN(margin[0] + 3, sizeof(es.prefix) - 3));
	snprintf(es.prefix, sizeof(es.prefix), "%s[ ", repeat_char(es.prelen - 2, ' '));
	es.postlen = 2;
	strncpy(es.postfix, " ]", sizeof(es.postfix));
	es.postfix[sizeof(es.postfix)-1] = '\0';
    }
    else{
	width -= 2;
	strncpy(es.prefix, "[", sizeof(es.prefix));
	es.prefix[sizeof(es.prefix)-1] = '\0';
	strncpy(es.postfix, "]", sizeof(es.postfix));
	es.postfix[sizeof(es.postfix)-1] = '\0';
	es.prelen = 1;
	es.postlen = 1;
    }

    es.do_color = (!(flags & FM_NOCOLOR) && (flags & FM_DISPLAY) && pico_usingcolor());

    gf_filter_init();

    /* catch urls */
    if((F_ON(F_VIEW_SEL_URL, ps_global)
	|| F_ON(F_VIEW_SEL_URL_HOST, ps_global)
	|| F_ON(F_SCAN_ADDR, ps_global))
       && handlesp){
	gf_link_filter(gf_line_test,
		       gf_line_test_opt(url_hilite,
				        gf_url_hilite_opt(&uh,handlesp,0)));
    }
      
    gf_link_filter(gf_wrap, gf_wrap_filter_opt(width, width, NULL, 0,
					       (handlesp ? GFW_HANDLES : GFW_NONE)));
    gf_link_filter(gf_line_test, gf_line_test_opt(quote_editorial, &es));

    /* If not for display, change to local end of line */
    if(!(flags & FM_DISPLAY))
	gf_link_filter(gf_nvtnl_local, NULL);

    return(gf_pipe(gc, pc));
}


int
quote_editorial(long int linenum, char *line, LT_INS_S **ins, void *local)
{
    COLOR_PAIR *col = NULL;

    ins = gf_line_test_new_ins(ins, line,
			       ((EDITORIAL_S *)local)->prefix,
			       ((EDITORIAL_S *)local)->prelen);
    if(((EDITORIAL_S *)local)->do_color
       && ps_global->VAR_METAMSG_FORE_COLOR
       && ps_global->VAR_METAMSG_BACK_COLOR
       && (col = new_color_pair(ps_global->VAR_METAMSG_FORE_COLOR,
				  ps_global->VAR_METAMSG_BACK_COLOR))){
	if(!pico_is_good_colorpair(col))
	  free_color_pair(&col);

	if(col){
	    char *p;
	    char normal_embed[(2 * RGBLEN) + 5];
	    char quote_color_embed[(2 * RGBLEN) + 5];

	    strncpy(quote_color_embed,
		    color_embed(col->fg, col->bg),
		    sizeof(quote_color_embed));
	    quote_color_embed[sizeof(quote_color_embed)-1] = '\0';

	    ins = gf_line_test_new_ins(ins, line,
				       quote_color_embed, (2 * RGBLEN) + 4);

	    /*
	     * If there was already a color change back to normal color
	     * in the line that was passed in, then instead of allowing
	     * that color change back to normal we want to change that
	     * to a color change back to our METAMSG color instead.
	     * Search line for that and modify it.
	     */

	    strncpy(normal_embed,
		    color_embed(ps_global->VAR_NORM_FORE_COLOR,
			        ps_global->VAR_NORM_BACK_COLOR),
		    sizeof(normal_embed));
	    normal_embed[sizeof(normal_embed)-1] = '\0';

	    for(p = line; (p = strstr(p, normal_embed)); p++){

		/*
		 * Replace the normal color with our special quoting
		 * color. No need to change it if there are no
		 * characters after the color change because we're
		 * going to change the color to normal right below
		 * this anyway.
		 */
		if(strlen(p) > strlen(quote_color_embed))
		  rplstr(p, strlen(p)+1, strlen(quote_color_embed), quote_color_embed);
	    }

	    ins = gf_line_test_new_ins(ins, line+strlen(line),
				       normal_embed, (2 * RGBLEN) + 4);
	    free_color_pair(&col);
	}
    }

    ins = gf_line_test_new_ins(ins, line + strlen(line),
			       ((EDITORIAL_S *)local)->postfix,
			       ((EDITORIAL_S *)local)->postlen);
    return(0);
}
