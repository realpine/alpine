/*
 * $Id: color.h 768 2007-10-24 00:10:03Z hubert@u.washington.edu $
 *
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

#ifndef PITH_COLOR_INCLUDED
#define PITH_COLOR_INCLUDED


#include "../pith/filttype.h"
#include "../pith/pattern.h"
#include "../pith/osdep/color.h"


typedef struct spec_color_s {
    int   inherit;	/* this isn't a color, it is INHERIT */
    char *spec;
    char *fg;
    char *bg;
    PATTERN_S *val;
    struct spec_color_s *next;
} SPEC_COLOR_S;


/*
 * These are default colors that you'll get when you turn
 * color on. The way color works is that the closest possible
 * RGB value is chosen so these colors will produce different
 * results in the different color models (the 8-color, 16-color,
 * xterm 256-color, and PC-Alpine color).
 * See init_color_table() for the RGB values we use.
 * The 8-color model uses the 8 0 or 255 possibilities. So,
 * for example, if the default color is "000,217,217" the
 * closes 8-color version of that is going to be "000,255,255".
 * In the 16-color terminal we use 000, 174, and 255 as the
 * possible values. That means that a default value
 * of "000,214,000" maps to "000,174,000" (a dull green)
 * but "000,215,000" maps to "000,255,000" (a bright green).
 *
 * The colors which don't have defaults map to either the current
 * setting of the Normal color or the current setting of the
 * Reverse color, depending on what we thought was right long ago.
 */
#define DEFAULT_TITLE_FORE_RGB		"000,000,000"
#define DEFAULT_TITLE_BACK_RGB		"255,255,000"
#define DEFAULT_TITLECLOSED_FORE_RGB	"255,255,255"
#define DEFAULT_TITLECLOSED_BACK_RGB	"255,000,000"
#define DEFAULT_METAMSG_FORE_RGB	"000,000,000"
#define DEFAULT_METAMSG_BACK_RGB	"255,255,000"
#define DEFAULT_QUOTE1_FORE_RGB		"000,000,000"
#define DEFAULT_QUOTE1_BACK_RGB		"000,217,217"
#define DEFAULT_QUOTE2_FORE_RGB		"000,000,000"
#define DEFAULT_QUOTE2_BACK_RGB		"204,214,000"
#define DEFAULT_QUOTE3_FORE_RGB		"000,000,000"
#define DEFAULT_QUOTE3_BACK_RGB		"000,214,000"
#define DEFAULT_SIGNATURE_FORE_RGB	"000,000,255"
#define DEFAULT_SIGNATURE_BACK_RGB	"255,255,255"
#define DEFAULT_IND_PLUS_FORE_RGB	"000,000,000"
#define DEFAULT_IND_PLUS_BACK_RGB	"000,174,174"
#define DEFAULT_IND_IMP_FORE_RGB	"240,240,240"
#define DEFAULT_IND_IMP_BACK_RGB	"174,000,000"
#define DEFAULT_IND_ANS_FORE_RGB	"255,000,000"
#define DEFAULT_IND_ANS_BACK_RGB	"174,174,000"
#define DEFAULT_IND_NEW_FORE_RGB	"240,240,240"
#define DEFAULT_IND_NEW_BACK_RGB	"174,000,174"
#define DEFAULT_IND_OP_FORE_RGB		"192,192,192"
#define DEFAULT_IND_OP_BACK_RGB		"255,255,255"


/* exported protoypes */
char	*color_embed(char *, char *);
int	 colorcmp(char *, char *);
int	 color_a_quote(long, char *, LT_INS_S **, void *);
void	 free_spec_colors(SPEC_COLOR_S **);


#endif /* PITH_COLOR_INCLUDED */
