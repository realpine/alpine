/*
 * $Id: color.h 408 2007-02-01 00:14:18Z hubert@u.washington.edu $
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


#ifndef PICO_OSDEP_COLOR_INCLUDED
#define PICO_OSDEP_COLOR_INCLUDED


/* exported prototypes */
void	 StartInverse(void);
void	 EndInverse(void);
int	 InverseState(void);
int	 StartBold(void);
void	 EndBold(void);
void	 StartUnderline(void);
void	 EndUnderline(void);
int      pico_trans_color(void);


#endif /* PICO_OSDEP_COLOR_INCLUDED */
