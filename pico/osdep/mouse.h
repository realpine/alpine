/*
 * $Id: mouse.h 193 2006-10-20 17:09:26Z mikes@u.washington.edu $
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


#ifndef PICO_OSDEP_MOUSE_INCLUDED
#define PICO_OSDEP_MOUSE_INCLUDED

#ifdef	MOUSE


/* exported prototypes */
int   init_mouse(void);
void  end_mouse(void);
int   mouseexist(void);
int   checkmouse(unsigned long *, int, int, int);
void  invert_label(int, MENUITEM *);


#endif /* MOUSE */

#endif /* PICO_OSDEP_MOUSE_INCLUDED */
