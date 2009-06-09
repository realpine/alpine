/*
 * $Id: strlst.h 136 2006-09-22 20:06:05Z hubert@u.washington.edu $
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

#ifndef PITH_STRLST_INCLUDED
#define PITH_STRLST_INCLUDED


/* exported protoypes */
STRINGLIST *new_strlst(char **);
void	    free_strlst(STRINGLIST **);


#endif /* PITH_STRLST_INCLUDED */
