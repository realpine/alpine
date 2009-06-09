/*
 * $Id: termin.unx.h 136 2006-09-22 20:06:05Z hubert@u.washington.edu $
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

#ifndef PINE_OSDEP_TERMIN_WNT_INCLUDED
#define PINE_OSDEP_TERMIN_WNT_INCLUDED


#include <general.h>
#include "../radio.h"
#include "../../pith/helptext.h"


/* exported prototypes */
void            flush_input(void);
int             win_dialog_opt_enter(char *, int, char *, ESCKEY_S *, HelpType, int *);
void            intr_proc(int);


#endif /* PINE_OSDEP_TERMIN_WNT_INCLUDED */
