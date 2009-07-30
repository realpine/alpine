/*
 * $Id: debugtime.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PITH_OSDEP_DEBUGTIME_INCLUDED
#define PITH_OSDEP_DEBUGTIME_INCLUDED

#include "../adjtime.h"

/*
 * Exported Prototypes
 */
#ifdef DEBUG
char	*debug_time(int, int);
#endif
int	 get_time(TIMEVAL_S *);
long	 time_diff(TIMEVAL_S *, TIMEVAL_S *);


#endif /* PITH_OSDEP_DEBUGTIME_INCLUDED */
