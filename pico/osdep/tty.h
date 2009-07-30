/*
 * $Id: tty.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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


#ifndef PICO_OSDEP_TTY_INCLUDED
#define PICO_OSDEP_TTY_INCLUDED


#include <general.h>


/* exported prototypes */
int	ttopen(void);
int	ttclose(void);
int	ttflush(void);
void	ttresize(void);
#ifndef _WINDOWS
int	ttgetc(int, int (*recorder)(int), void (*bail_handler)(void));
int	simple_ttgetc(int (*recorder)(int), void (*bail_handler)(void));
int	ttputc(UCS);
void	ttgetwinsz(int *, int *);
#endif /* !_WINDOWS */

#endif /* PICO_OSDEP_TTY_INCLUDED */
