/*
 * $Id: getkey.h 166 2006-10-04 01:42:12Z jpf@u.washington.edu $
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


#ifndef PICO_OSDEP_GETKEY_INCLUDED
#define PICO_OSDEP_GETKEY_INCLUDED


#include <general.h>


/* exported prototypes */
UCS	      GetKey(void);
#if	TYPEAH
int	      typahead(void);
#endif /* TYPEAH */
#ifndef _WINDOWS
UCS           kbseq(int (*getcfunc)(int (*recorder)(int ), void (*bail_handler)(void )),
		    int (*recorder)(int), void (*bail_handler)(void), void *, UCS *);
void          kbdestroy(KBESC_T *);
#endif

#endif /* PICO_OSDEP_GETKEY_INCLUDED */
