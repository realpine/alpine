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


#ifndef PICO_OSDEP_COLOR_INCLUDED
#define PICO_OSDEP_COLOR_INCLUDED


/* exported prototypes */
void	 StartInverse(void);
void     flip_inv(int);
void	 EndInverse(void);
int	 InverseState(void);
int	 StartBold(void);
void	 EndBold(void);
void	 StartUnderline(void);
void	 EndUnderline(void);
int      pico_trans_color(void);
void	 pico_endcolor(void);
void	 pico_toggle_color(int);
void	 pico_set_nfg_color(void);
void	 pico_set_nbg_color(void);


#endif /* PICO_OSDEP_COLOR_INCLUDED */
