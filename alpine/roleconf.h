/*
 * $Id: roleconf.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PINE_ROLECONF_INCLUDED
#define PINE_ROLECONF_INCLUDED


#include "conftype.h"
#include "../pith/state.h"
#include "../pith/pattern.h"
#include "../pith/msgno.h"


/* exported protoypes */
int         role_select_screen(struct pine *, ACTION_S **, int);
void        role_config_screen(struct pine *, long, int);
void	    role_take(struct pine *, MSGNO_S *, int);
int	    role_radiobutton_tool(struct pine *, int, CONF_S **, unsigned);


#endif /* PINE_ROLECONF_INCLUDED */
