/*
 * $Id: shell.h 807 2007-11-09 01:21:33Z hubert@u.washington.edu $
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


#ifndef PICO_OSDEP_SHELL_INCLUDED
#define PICO_OSDEP_SHELL_INCLUDED


/* exported prototypes */
#if defined(SIGTSTP) || defined(_WINDOWS)
int	bktoshell(int, int);
#endif


#endif /* PICO_OSDEP_SHELL_INCLUDED */
