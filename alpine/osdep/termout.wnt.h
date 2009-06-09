/*
 * $Id: termout.unx.h 159 2006-10-02 22:00:13Z hubert@u.washington.edu $
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

#ifndef PINE_OSDEP_TERMOUT_WNT_INCLUDED
#define PINE_OSDEP_TERMOUT_WNT_INCLUDED

#include "resource.h"

typedef	struct DLG_SORTPARAM {
    int		rval;		/* Return value. */
    int		reverse;	/* Indicates a reversed sort. */
    int		cursort;	/* Current sort (pineid). */
    char	**helptext;	/* Pointer to help text. */
    int		sortcount;	/* Number of different sorts. */
    struct sorttypemap *types;  /* Pointer to array of conversion between
				 * the pine sort types and the radio button
				 * ids. */
} DLG_SORTPARAM;


/* exported prototypes */

void scroll_setpos(long);
void scroll_setrange(long, long);

/* dialog stuff */
int init_install_get_vars(void);
int os_argsdialog(char **);
int os_login_dialog(NETMBX *, char *, int, char *, int, int, int, int *);
int os_flagmsgdialog(struct flag_table *);
int os_sortdialog(DLG_SORTPARAM *);
int os_config_dialog(char *, int, int *, int);

#endif /* PINE_OSDEP_TERMOUT_WNT_INCLUDED */
