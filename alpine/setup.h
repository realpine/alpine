/*
 * $Id: setup.h 136 2006-09-22 20:06:05Z hubert@u.washington.edu $
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

#ifndef PINE_SETUP_INCLUDED
#define PINE_SETUP_INCLUDED


#include "confscroll.h"
#include "../pith/state.h"


/* exported protoypes */
void	    option_screen(struct pine *, int);
int         litsig_text_tool(struct pine *, int, CONF_S **, unsigned);


#endif /* PINE_SETUP_INCLUDED */
