/*
 * $Id: altedit.h 769 2007-10-24 00:15:40Z hubert@u.washington.edu $
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

#ifndef PICO_OSDEP_ALTEDIT_INCLUDED
#define PICO_OSDEP_ALTEDIT_INCLUDED



/* exported prototypes */
int	alt_editor(int, int);
#ifndef _WINDOWS
int	   pathcat(char *, char **, char *);
#endif


#endif /* PICO_OSDEP_ALTEDIT_INCLUDED */
