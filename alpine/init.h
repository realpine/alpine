/*
 * $Id: init.h 82 2006-07-12 23:36:59Z mikes@u.washington.edu $
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

#ifndef PINE_INIT_INCLUDED
#define PINE_INIT_INCLUDED


/* exported protoypes */
int	init_mail_dir(struct pine *);
int	expire_sent_mail(void);


#endif /* PINE_INIT_INCLUDED */


