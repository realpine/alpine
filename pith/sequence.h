/*
 * $Id: sequence.h 1012 2008-03-26 00:44:22Z hubert@u.washington.edu $
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

#ifndef PITH_SEQUENCE_INCLUDED
#define PITH_SEQUENCE_INCLUDED


#include "../pith/msgno.h"


/* exported protoypes */
char	  *selected_sequence(MAILSTREAM *, MSGNO_S *, long *, int);
char	  *currentf_sequence(MAILSTREAM *, MSGNO_S *, long, long *, int, char **, char **);
char	  *invalid_elt_sequence(MAILSTREAM *, MSGNO_S *);
char	  *build_sequence(MAILSTREAM *, MSGNO_S *, long *);
int	   pseudo_selected(MAILSTREAM *, MSGNO_S *);
void	   restore_selected(MSGNO_S *);
SEARCHSET *limiting_searchset(MAILSTREAM *, int);


#endif /* PITH_SEQUENCE_INCLUDED */
