/*
 * $Id: pineelt.h 136 2006-09-22 20:06:05Z hubert@u.washington.edu $
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

#ifndef PITH_PINEELT_INCLUDED
#define PITH_PINEELT_INCLUDED


#include "../pith/thread.h"
#include "../pith/indxtype.h"


/*
 * The two structs below hold all knowledge regarding
 * messages requiring special handling
 */
typedef	struct part_exception_s {
    char		     *partno;
    int			      handling;
    struct part_exception_s  *next;
} PARTEX_S;


/*
 * Pine's private per-message data stored on the c-client's elt
 * spare pointer.
 */
typedef struct pine_elt {
    PINETHRD_S  *pthrd;
    PARTEX_S    *exceptions;
    ICE_S       *ice;
} PINELT_S;


/* exported protoypes */
void	    msgno_free_exceptions(PARTEX_S **);


#endif /* PITH_PINEELT_INCLUDED */
