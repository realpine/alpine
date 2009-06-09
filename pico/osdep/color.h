/*
 * $Id: color.h 673 2007-08-16 22:25:10Z hubert@u.washington.edu $
 *
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


#endif /* PICO_OSDEP_COLOR_INCLUDED */
