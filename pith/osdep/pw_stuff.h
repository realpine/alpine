/*
 * $Id: pw_stuff.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PITH_OSDEP_PW_STUFF_INCLUDED
#define PITH_OSDEP_PW_STUFF_INCLUDED

#include "../user.h"


/*
 * Exported Prototypes
 */
void	 get_user_info(USER_S *);
char	*local_name_lookup(char *);


#endif /* PITH_OSDEP_PW_STUFF_INCLUDED */
