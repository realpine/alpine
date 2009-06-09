/*
 * $Id: pipe.h 136 2006-09-22 20:06:05Z hubert@u.washington.edu $
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

#ifndef ALPINE_PIPE_INCLUDED
#define ALPINE_PIPE_INCLUDED


#include "../pith/filter.h"
#include "../pith/osdep/pipe.h"


/* exported protoypes */
int	 raw_pipe_getc(unsigned char *);
void	 prime_raw_pipe_getc(MAILSTREAM *, long, long, long);
PIPE_S	*cmd_pipe_open(char *, char **, int, gf_io_t *);

#endif /* ALPINE_PIPE_INCLUDED */
