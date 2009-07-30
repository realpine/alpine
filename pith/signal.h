/*
 * $Id: signal.h 1025 2008-04-08 22:59:38Z hubert@u.washington.edu $
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

#ifndef PITH_SIGNAL_INCLUDED
#define PITH_SIGNAL_INCLUDED


/* exported protoypes */

/* currently mandatory to implement stubs */

int	    intr_handling_on(void);
void	    intr_handling_off(void);


#endif /* PITH_SIGNAL_INCLUDED */
