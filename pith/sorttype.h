/*
 * $Id: sorttype.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PITH_SORTTYPE_INCLUDED
#define PITH_SORTTYPE_INCLUDED


/*
 * Code exists that is sensitive to this order.  Don't change
 * it unless you know what you're doing.
 */
typedef enum {SortSubject = 0, SortArrival, SortFrom, SortTo, 
              SortCc, SortDate, SortSize,
	      SortSubject2, SortScore, SortThread, EndofList}   SortOrder;


/* exported protoypes */


#endif /* PITH_SORTTYPE_INCLUDED */
