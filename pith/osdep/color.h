/*
 * $Id: color.h 1012 2008-03-26 00:44:22Z hubert@u.washington.edu $
 *
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

#ifndef PITH_OSDEP_COLOR_INCLUDED
#define PITH_OSDEP_COLOR_INCLUDED


#define RGBLEN 11
#define MAXCOLORLEN 11			/* longest string a color can be */

typedef struct COLOR_PAIR {
	char fg[MAXCOLORLEN+1];
	char bg[MAXCOLORLEN+1];
} COLOR_PAIR;

#define COL_BLACK	0
#define COL_RED		1
#define COL_GREEN	2
#define COL_YELLOW	3
#define COL_BLUE	4
#define COL_MAGENTA	5
#define COL_CYAN	6
#define COL_WHITE	7

#define DEFAULT_NORM_FORE_RGB	"000,000,000"
#define DEFAULT_NORM_BACK_RGB	"255,255,255"

/* flags for pico_set_color() */
#define PSC_NONE	0x0
#define PSC_NORM	0x1
#define PSC_REV		0x2
#define PSC_RET		0x4	/* return an allocated copy of previous color */


/*
 * MATCH_NORM_COLOR means that the color that is set to this value
 * will actually use the corresponding fg or bg color from the
 * so called Normal Color. A MATCH_NONE_COLOR means that the
 * corresponding fg or bg color will just be left alone, so that
 * it will stay the same as it was. This is useful when you want
 * to change the foreground color but let the background match
 * whatever it was before, for example in colored index lines.
 *
 * Note: these need to be RGBLEN in length because they are sometimes
 * used in places where an RGB value is expected.
 */
#define MATCH_NORM_COLOR	"norm_padded"
#define MATCH_NONE_COLOR	"none_padded"

#define MATCH_TRAN_COLOR	"transparent"


/* exported prototypes */
COLOR_PAIR *new_color_pair(char *, char *);
void	    free_color_pair(COLOR_PAIR **cp);
int	    pico_is_good_colorpair(COLOR_PAIR *);
COLOR_PAIR *pico_set_colorp(COLOR_PAIR *, int);


/* required prototypes (os/app dependent ) */
int         pico_usingcolor(void);
int	    pico_hascolor(void);
char	   *colorx(int);
char	   *color_to_asciirgb(char *);
int	    pico_is_good_color(char *);
COLOR_PAIR *pico_set_colors(char *, char *, int);
int	    pico_set_fg_color(char *);
int	    pico_set_bg_color(char *);
void	    pico_nfcolor(char *);
void	    pico_nbcolor(char *);
void	    pico_rfcolor(char *);
void	    pico_rbcolor(char *);
COLOR_PAIR *pico_get_cur_color(void);
COLOR_PAIR *pico_get_rev_color(void);
void	    pico_set_normal_color(void);
void	    pico_set_color_options(unsigned);
unsigned    pico_get_color_options(void);
int         pico_trans_is_on(void);
char	   *pico_get_last_fg_color(void);
char	   *pico_get_last_bg_color(void);
char	   *color_to_canonical_name(char *);
int	    pico_count_in_color_table(void);


#endif /* PITH_OSDEP_COLOR_INCLUDED */
