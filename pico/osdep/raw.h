/*
 * $Id: raw.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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


#ifndef PICO_OSDEP_RAW_INCLUDED
#define PICO_OSDEP_RAW_INCLUDED


/* useful definitions */
#define STDIN_FD	0
#define STDOUT_FD	1
#define STDERR_FD	2


/* exported prototypes */
int	Raw(int);
void	bit_strip_off(void);
void	quit_char_off(void);
int	ttisslow(void);
void	xonxoff_proc(int);
void	flush_input(void);
void	crlf_proc(int);
void	intr_proc(int);


#endif /* PICO_OSDEP_RAW_INCLUDED */
