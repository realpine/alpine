/*
 * $Id: busy.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PITH_BUSY_INCLUDED
#define PITH_BUSY_INCLUDED


typedef int (*percent_done_t)();    /* returns %done for progress status msg */


/* used to tweak busy without it */
#ifndef	ALARM_BLIP
#define ALARM_BLIP()
#endif


/* exported protoypes */


/* currently mandatory to implement stubs */

int	    busy_cue(char *, percent_done_t, int);
void	    cancel_busy_cue(int); 


#endif /* PITH_BUSY_INCLUDED */
