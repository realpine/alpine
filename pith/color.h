/*
 * $Id: color.h 136 2006-09-22 20:06:05Z hubert@u.washington.edu $
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



typedef struct spec_color_s {
    int   inherit;	/* this isn't a color, it is INHERIT */
    char *spec;
    char *fg;
    char *bg;
    PATTERN_S *val;
    struct spec_color_s *next;
} SPEC_COLOR_S;


/* exported protoypes */
char	*color_embed(char *, char *);
int	 colorcmp(char *, char *);
int	 color_a_quote(long, char *, LT_INS_S **, void *);
void	 free_spec_colors(SPEC_COLOR_S **);


#endif /* PITH_COLOR_INCLUDED */
