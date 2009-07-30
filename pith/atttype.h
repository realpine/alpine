/*
 * $Id: atttype.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PITH_ATTTYPE_INCLUDED
#define PITH_ATTTYPE_INCLUDED


typedef struct attachment {
    char       *description;
    BODY       *body;
    unsigned	test_deferred:1;
    unsigned	can_display:4;
    unsigned	shown:1;
    unsigned	suppress_editorial:1;
    char       *number;
    char	size[25];
} ATTACH_S;


/*
 * struct to help peruse a, possibly fragmented ala RFC 2231, parm list
 */
typedef	struct parmlist {
    PARAMETER *list,
	      *seen;
    char       attrib[32],
	      *value;
} PARMLIST_S;


/* exported protoypes */


#endif /* PITH_ATTTYPE_INCLUDED */
