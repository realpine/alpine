/*
 * $Id: print.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PINE_OSDEP_PRINT_INCLUDED
#define PINE_OSDEP_PRINT_INCLUDED


/* exported prototypes */
int	open_printer(char *);
void	close_printer(void);
int	print_char(int);
void	print_text(char *);
void	print_text1(char *, char *);


#endif /* PINE_OSDEP_PRINT_INCLUDED */
