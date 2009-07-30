/*
 * $Id: folder.h 767 2007-10-24 00:03:59Z hubert@u.washington.edu $
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

#ifndef PINE_FOLDER_INCLUDED
#define PINE_FOLDER_INCLUDED


#include "context.h"
#include "../pith/folder.h"
#include "../pith/state.h"
#include "../pith/conf.h"


/* exported protoypes */
void	    folder_screen(struct pine *);
void	    folder_config_screen(struct pine *, int);
int	    folders_for_goto(struct pine *, CONTEXT_S **, char *, int);
int	    folders_for_save(struct pine *, CONTEXT_S **, char *, int);
char	   *folders_for_fcc(char **);
char	   *folder_for_config(int);
char	   *context_edit_screen(struct pine *, char *, char *, char *, char *, char *);
int	    add_new_folder(CONTEXT_S *, EditWhich, int, char *, size_t, MAILSTREAM *, char *);
char	   *next_folder(MAILSTREAM **, char *, size_t, char *, CONTEXT_S *, long *, int *);
char	   *news_group_selector(char **);


#endif /* PINE_FOLDER_INCLUDED */
