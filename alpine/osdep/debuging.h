/*
 * $Id: debuging.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PINE_OSDEP_DEBUGING_INCLUDED
#define PINE_OSDEP_DEBUGING_INCLUDED

/* stream to stuff debuging output */
extern FILE *debugfile;

/*
 * Exported Prototypes
 */
#ifdef DEBUG
void	      init_debug(void);
void	      save_debug_on_crash(FILE *, int (*)(int *, char *, size_t));
#endif

#endif /* PINE_OSDEP_DEBUGING_INCLUDED */
