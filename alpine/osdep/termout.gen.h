/*
 * $Id: termout.gen.h 769 2007-10-24 00:15:40Z hubert@u.washington.edu $
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

#ifndef PINE_OSDEP_TERMOUT_GEN_INCLUDED
#define PINE_OSDEP_TERMOUT_GEN_INCLUDED


extern int   _line;
extern int   _col;


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
void    CleartoEOLN(void);
void    ClearScreen(void);
void	clear_cursor_pos(void);
void	Writechar(unsigned int, int);
void    Write_to_screen(char *);
void    Write_to_screen_n(char *, int);
void    init_screen(void);
void	end_screen(char *, int);
int     config_screen(struct ttyo **);
void	MoveCursor(int, int);
void	Writewchar(UCS);
void	icon_text(char *, int);
int     get_windsize(struct ttyo *);
int     BeginScroll(int, int);
void    EndScroll(void);
int     ScrollRegion(int);


#endif /* PINE_OSDEP_TERMOUT_GEN_INCLUDED */
