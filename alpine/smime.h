/*
 * $Id: smime.h 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $
 *
 * ========================================================================
 * Copyright 2008 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */

#ifdef SMIME
#ifndef PINE_SMIME_INCLUDED
#define PINE_SMIME_INCLUDED


#include "../pith/state.h"
#include "../pith/send.h"
#include "../pith/smime.h"


/* exported protoypes */
int    smime_get_passphrase(void);
void   smime_info_screen(struct pine *ps);
void   smime_config_screen(struct pine *, int edit_exceptions);
int    smime_related_var(struct pine *, struct variable *);


#endif /* PINE_SMIME_INCLUDED */
#endif /* SMIME */
