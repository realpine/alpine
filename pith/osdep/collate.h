/*
 * $Id: collate.h 136 2006-09-22 20:06:05Z hubert@u.washington.edu $
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

#ifndef PITH_OSDEP_COLLATE_INCLUDED
#define PITH_OSDEP_COLLATE_INCLUDED


/*
 * Exported Prototypes
 */
void	  set_collation(int, int);
int	  strucmp(char *, char *);
int	  struncmp(char *, char *, int);
int	(*pcollator)();


#endif /* PITH_OSDEP_COLLATE_INCLUDED */
