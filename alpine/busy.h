/*
 * $Id: busy.h 137 2006-09-22 21:34:06Z mikes@u.washington.edu $
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

#ifndef PINE_BUSY_INCLUDED
#define PINE_BUSY_INCLUDED


#define MAX_BM			150  /* max length of busy message */


/* exported prototypes */

void	    suspend_busy_cue(void);
void	    resume_busy_cue(unsigned);


#endif	/* PINE_BUSY_INCLUDED */
