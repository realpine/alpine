/*
 * $Id: init.h 900 2008-01-05 01:13:26Z hubert@u.washington.edu $
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

#ifndef PITH_INIT_INCLUDED
#define PITH_INIT_INCLUDED

#include "../pith/state.h"
#include "../pith/conftype.h"
#include "../pith/context.h"


#define ALPINE_VERSION		PACKAGE_VERSION

#define	LEGAL_NOTICE \
   "Copyright 2006-2008 University of Washington"

#define	LEGAL_NOTICE2 \
   "Copyright 2009-2010 Re-Alpine Project"


/* exported protoypes */
int               init_username(struct pine *);
int               init_userdir(struct pine *);
int               init_hostname(struct pine *);  
void              init_save_defaults(void);
int               check_prune_time(time_t *, struct tm **);
int               prune_move_folder(char *, char *, CONTEXT_S *);
int               first_run_of_month(void);
int               first_run_of_year(void);
struct sm_folder *get_mail_list(CONTEXT_S *, char *);


#endif /* PITH_INIT_INCLUDED */
