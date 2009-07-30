/*
 * $Id: listsel.h 918 2008-01-23 19:39:38Z hubert@u.washington.edu $
 *
 * ========================================================================
 * Copyright 2006-2008 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */

#ifndef PINE_LISTSEL_INCLUDED
#define PINE_LISTSEL_INCLUDED


#include "help.h"


/* for select_from_list_screen */
#define	SFL_NONE		0x000
#define	SFL_ALLOW_LISTMODE	0x001
#define	SFL_STARTIN_LISTMODE	0x002	/* should also light ALLOW_LISTMODE */
#define	SFL_ONLY_LISTMODE	0x004	/* don't allow switching back out of ListMode */
#define	SFL_NOSELECT		0x008	/* per item flag, line not selectable */
#define	SFL_CTRLC		0x010	/* use ^C instead of Exit for Exit and
					   allow zero selections */


typedef struct list_selection {
    char                  *display_item;	/* use item if this is NULL */
    char                  *item;		/* selected value for item  */
    int                    selected;		/* is item selected or not */
    int                    flags;
    struct list_selection *next;
} LIST_SEL_S;


/* exported protoypes */
int         select_from_list_screen(LIST_SEL_S *, unsigned long, char *, char *, HelpType, char *, LIST_SEL_S *);


#endif /* PINE_LISTSEL_INCLUDED */
