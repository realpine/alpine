/*
 * $Id: bitmap.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PITH_BITMAP_INCLUDED
#define PITH_BITMAP_INCLUDED


#include "../pith/conftype.h"


/*
 * Max size of a bitmap based on largest customer: feature list count
 * Problems if a screen's keymenu bitmap ever gets wider than feature list.
 */
#define BM_SIZE			((F_FEATURE_LIST_COUNT / 8)		   \
				      + ((F_FEATURE_LIST_COUNT % 8) ? 1 : 0))

typedef unsigned char bitmap_t[BM_SIZE];

/* clear entire bitmap */
#define clrbitmap(map)		memset((void *)(map), 0, (size_t)BM_SIZE)

/* set entire bitmap */
#define setbitmap(map)		memset((void *)(map), 0xff, (size_t)BM_SIZE)


#endif /* PITH_BITMAP_INCLUDED */
