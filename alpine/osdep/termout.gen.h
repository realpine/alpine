/*
 * $Id: termout.gen.h 221 2006-11-06 22:17:52Z jpf@u.washington.edu $
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

#ifndef PINE_OSDEP_TERMOUT_GEN_INCLUDED
#define PINE_OSDEP_TERMOUT_GEN_INCLUDED


/* exported prototypes */
void	PutLine0(int, int, register char *);
void	PutLine0n8b(int, int, register char *, int, HANDLE_S *);
void	PutLine1(int, int, char *, void *);
void	PutLine2(int, int, char *, void *, void *);
void	PutLine3(int, int, char *, void *, void *, void *);
void	PutLine4(int, int, char *, void *, void *, void *, void *);
void	PutLine5(int, int, char *, void *, void *, void *, void *, void *);
int	Centerline(int, char *);
void	ClearLine(int);
void	ClearLines(int, int);
void	clear_cursor_pos(void);
void	Writechar(register unsigned int, int);


#endif /* PINE_OSDEP_TERMOUT_GEN_INCLUDED */
