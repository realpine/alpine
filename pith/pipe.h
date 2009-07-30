/*
 * $Id: pipe.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PITH_PIPE_INCLUDED
#define PITH_PIPE_INCLUDED


#include "../pith/filter.h"
#include "../pith/osdep/pipe.h"


/* exported protoypes */
int	 pipe_putc(int, PIPE_S *);
int	 pipe_puts(char *, PIPE_S *);
char	*pipe_gets(char *, int, PIPE_S *);
int	 pipe_readc(unsigned char *, PIPE_S *);
int	 pipe_writec(int, PIPE_S *);
void	 pipe_report_error(char *);


#endif /* PITH_PIPE_INCLUDED */
