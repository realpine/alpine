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

#include "../pith/headers.h"
#include "../pith/color.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/filter.h"


char *
color_embed(char *fg, char *bg)
{
    static char buf[(2 * RGBLEN) + 5], *p;

    p = buf;
    if(fg){
	if(sizeof(buf)-(p-buf) > 1){
	  *p++ = TAG_EMBED;
	  *p++ = TAG_FGCOLOR;
	}

	sstrncpy(&p, color_to_asciirgb(fg), sizeof(buf)-(p-buf));
    }

    if(bg){
	if(sizeof(buf)-(p-buf) > 1){
	  *p++ = TAG_EMBED;
	  *p++ = TAG_BGCOLOR;
	}

	sstrncpy(&p, color_to_asciirgb(bg), sizeof(buf)-(p-buf));
    }

    buf[sizeof(buf)-1] = '\0';

    return(buf);
}


int
colorcmp(char *color1, char *color2)
{
    if(color1 && color2)
      return(strcmp(color_to_asciirgb(color1), color_to_asciirgb(color2)));

    /* if both NULL they're the same? */
    return(!(color1 || color2));
}



struct quote_colors {
    COLOR_PAIR          *color;
    struct quote_colors *next;
};


int
color_a_quote(long int linenum, char *line, LT_INS_S **ins, void *is_flowed_msg)
{
    int countem = 0;
    struct variable *vars = ps_global->vars;
    char *p;
    struct quote_colors *colors = NULL, *cp, *next;
    COLOR_PAIR *col = NULL;
    int is_flowed = is_flowed_msg ? *((int *)is_flowed_msg) : 0;

    p = line;
    if(!is_flowed)
      while(isspace((unsigned char)*p))
	p++;

    if(p[0] == '>'){
	struct quote_colors *c;

	/*
	 * We have a fixed number of quote level colors (3). If there are
	 * more levels of quoting than are defined, they repeat.
	 */
	if(VAR_QUOTE1_FORE_COLOR && VAR_QUOTE1_BACK_COLOR &&
	   (col = new_color_pair(VAR_QUOTE1_FORE_COLOR,
				 VAR_QUOTE1_BACK_COLOR)) &&
	   pico_is_good_colorpair(col)){
	    c = (struct quote_colors *)fs_get(sizeof(*c));
	    memset(c, 0, sizeof(*c));
	    c->color = col;
	    col = NULL;
	    colors = c;
	    cp = c;
	    countem++;
	    if(VAR_QUOTE2_FORE_COLOR && VAR_QUOTE2_BACK_COLOR &&
	       (col = new_color_pair(VAR_QUOTE2_FORE_COLOR,
				     VAR_QUOTE2_BACK_COLOR)) &&
	       pico_is_good_colorpair(col)){
		c = (struct quote_colors *)fs_get(sizeof(*c));
		memset(c, 0, sizeof(*c));
		c->color = col;
		col = NULL;
		cp->next = c;
		cp = c;
		countem++;
		if(VAR_QUOTE3_FORE_COLOR && VAR_QUOTE3_BACK_COLOR &&
		   (col = new_color_pair(VAR_QUOTE3_FORE_COLOR,
					 VAR_QUOTE3_BACK_COLOR)) &&
		   pico_is_good_colorpair(col)){
		    c = (struct quote_colors *)fs_get(sizeof(*cp));
		    memset(c, 0, sizeof(*c));
		    c->color = col;
		    col = NULL;
		    cp->next = c;
		    cp = c;
		    countem++;
		}
	    }
	}
    }

    if(col)
      free_color_pair(&col);

    cp = NULL;
    while(*p == '>'){
	cp = (cp && cp->next) ? cp->next : colors;

	if(countem > 0)
	  ins = gf_line_test_new_ins(ins, p,
				     color_embed(cp->color->fg, cp->color->bg),
				     (2 * RGBLEN) + 4);

	countem = (countem == 1) ? 0 : countem;

	p++;
	if(!is_flowed)
	  for(; isspace((unsigned char)*p); p++)
	    ;
    }

    if(colors){
	char fg[RGBLEN + 1], bg[RGBLEN + 1], rgbbuf[RGBLEN + 1];

	strncpy(fg, color_to_asciirgb(VAR_NORM_FORE_COLOR), sizeof(fg));
	strncpy(bg, color_to_asciirgb(VAR_NORM_BACK_COLOR), sizeof(bg));
	fg[sizeof(fg)-1] = '\0';
	bg[sizeof(bg)-1] = '\0';

	/*
	 * Loop watching colors, and override with most recent
	 * quote color whenever the normal foreground and background
	 * colors are in force.
	 */
	while(*p)
	  if(*p++ == TAG_EMBED){

	      switch(*p++){
		case TAG_HANDLE :
		  p += *p + 1;	/* skip handle key */
		  break;

		case TAG_FGCOLOR :
		  snprintf(rgbbuf, sizeof(rgbbuf), "%s", p);
		  p += RGBLEN;	/* advance past color value */
		  
		  if(!colorcmp(rgbbuf, VAR_NORM_FORE_COLOR)
		     && !colorcmp(bg, VAR_NORM_BACK_COLOR))
		    ins = gf_line_test_new_ins(ins, p,
					       color_embed(cp->color->fg,NULL),
					       RGBLEN + 2);
		  break;

		case TAG_BGCOLOR :
		  snprintf(rgbbuf, sizeof(rgbbuf), "%s", p);
		  p += RGBLEN;	/* advance past color value */
		  
		  if(!colorcmp(rgbbuf, VAR_NORM_BACK_COLOR)
		     && !colorcmp(fg, VAR_NORM_FORE_COLOR))
		    ins = gf_line_test_new_ins(ins, p,
					       color_embed(NULL,cp->color->bg),
					       RGBLEN + 2);

		  break;

		default :
		  break;
	      }
	  }

	ins = gf_line_test_new_ins(ins, line + strlen(line),
				   color_embed(VAR_NORM_FORE_COLOR,
					       VAR_NORM_BACK_COLOR),
				   (2 * RGBLEN) + 4);
	for(cp = colors; cp && cp->color; cp = next){
	    free_color_pair(&cp->color);
	    next = cp->next;
	    fs_give((void **)&cp);
	}
    }

    return(0);
}


void
free_spec_colors(SPEC_COLOR_S **colors)
{
    if(colors && *colors){
	free_spec_colors(&(*colors)->next);
	if((*colors)->spec)
	  fs_give((void **)&(*colors)->spec);
	if((*colors)->fg)
	  fs_give((void **)&(*colors)->fg);
	if((*colors)->bg)
	  fs_give((void **)&(*colors)->bg);
	if((*colors)->val)
	  free_pattern(&(*colors)->val);
	
	fs_give((void **)colors);
    }
}
