/*
 * $Id: mimedesc.h 1012 2008-03-26 00:44:22Z hubert@u.washington.edu $
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

#ifndef PITH_MIMEDESC_INCLUDED
#define PITH_MIMEDESC_INCLUDED


#include "../pith/atttype.h"
#include "../pith/filttype.h"


/* exported prototypes */
void	    describe_mime(BODY *, char *, int, int, int, int);
void	    zero_atmts(ATTACH_S *);
char	   *body_type_names(int);
char	   *type_desc(int, char *, PARAMETER *, PARAMETER *, int);
long        fcc_size_guess(BODY *);
char	   *part_desc(char *, BODY *, int, int, int, gf_io_t);
char	   *parameter_val(PARAMETER *, char *);
int	    mime_can_display(int, char *, BODY *);
char	   *get_filename_parameter(char *, size_t, BODY *, char **);


#endif /* PITH_MIMEDESC_INCLUDED */
