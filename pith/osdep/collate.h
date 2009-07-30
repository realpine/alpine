/*
 * $Id: collate.h 925 2008-02-06 02:03:01Z hubert@u.washington.edu $
 *
 * ========================================================================
 * Copyright 2006-2008 University of Washington
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
int       sstrcasecmp(const qsort_t *, const qsort_t *);

extern    int	(*pcollator)();


#endif /* PITH_OSDEP_COLLATE_INCLUDED */
