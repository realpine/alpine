/*
 * $Id: user.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PITH_USER_INCLUDED
#define PITH_USER_INCLUDED


/*
 * used to store system derived user info
 */
typedef struct user_info {
    char *homedir;
    char *login;
    char *fullname;
} USER_S;


#endif /* PITH_USER_INCLUDED */
