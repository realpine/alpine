/*
 * $Id: addrbook.h 82 2006-07-12 23:36:59Z mikes@u.washington.edu $
 *
 * ========================================================================
 * Copyright 2006-2007 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */

#ifndef PITH_ADDRBOOK_INCLUDED
#define PITH_ADDRBOOK_INCLUDED


#include "adrbklib.h"


/* exported protoypes */
void	addrbook_new_disp_form(PerAddrBook *, char **, int, int (*)(PerAddrBook *, int *));
long	first_selectable_line(long);
int	line_is_selectable(long);


#endif /* PITH_ADDRBOOK_INCLUDED */
