/*
 * $Id: read.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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


#ifndef PICO_OSDEP_READ_INCLUDED
#define PICO_OSDEP_READ_INCLUDED


#include <general.h>


/* exported prototypes */
#ifndef _WINDOWS
UCS	input_ready(int);
int	read_one_char(void);
#else /* _WINDOWS */
int set_time_of_last_input(void);
#endif /* _WINDOWS */

time_t	time_of_last_input(void);


#endif /* PICO_OSDEP_READ_INCLUDED */
