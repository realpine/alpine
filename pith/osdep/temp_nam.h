/*
 * $Id: temp_nam.h 229 2006-11-13 23:14:48Z hubert@u.washington.edu $
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

#ifndef PITH_OSDEP_TEMP_NAM_INCLUDED
#define PITH_OSDEP_TEMP_NAM_INCLUDED


/* flags for temp_nam() */
#define TN_BINARY	0x0	/* default */
#define TN_TEXT		0x1


/*
 * Exported Prototypes
 */
char	*temp_nam(char *, char *, int);
char	*temp_nam_ext(char *, char *, int, char *);


#endif /* PITH_OSDEP_TEMP_NAM_INCLUDED */
