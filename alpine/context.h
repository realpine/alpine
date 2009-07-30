/*
 * $Id: context.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PINE_CONTEXT_INCLUDED
#define PINE_CONTEXT_INCLUDED


#include "../pith/context.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "help.h"
#include "keymenu.h"


/*
 * Structures to control the collection list screen
 */
typedef struct context_screen {
    unsigned	      edit:1;
    char	     *title, *print_string;
    CONTEXT_S	     *start,		/* for context_select_screen */
		     *selected,
		    **contexts;
    struct variable  *starting_var;	/* another type of start for config */
    int               starting_varmem;
    struct {
	HelpType  text;
	char	 *title;
    } help;
    struct key_menu  *keymenu;
} CONT_SCR_S;


/* exported protoypes */
void        context_config_screen(struct pine *, CONT_SCR_S *, int);
CONTEXT_S  *context_select_screen(struct pine *, CONT_SCR_S *, int);


#endif /* PINE_CONTEXT_INCLUDED */
