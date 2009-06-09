/*
 * $Id: pattern.h 137 2006-09-22 21:34:06Z mikes@u.washington.edu $
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

#ifndef ALPINE_PATTERN_INCLUDED
#define ALPINE_PATTERN_INCLUDED

#include "../pith/pattern.h"

/* exported protoypes */
void	pattern_filter_command(char **, SEARCHSET *, MAILSTREAM *, long, INTVL_S *);


#endif /* ALPINE_PATTERN_INCLUDED */
