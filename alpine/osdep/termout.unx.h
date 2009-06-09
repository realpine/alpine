/*
 * $Id: termout.unx.h 221 2006-11-06 22:17:52Z jpf@u.washington.edu $
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

#ifndef PINE_OSDEP_TERMOUT_UNX_INCLUDED
#define PINE_OSDEP_TERMOUT_UNX_INCLUDED



extern int   _line;
extern int   _col;


/* exported prototypes */
void	outchar(int);
void	end_screen(char *, int);
void	MoveCursor(int, int);
void	Writewchar(UCS);
void	icon_text(char *, int);


#endif /* PINE_OSDEP_TERMOUT_UNX_INCLUDED */
