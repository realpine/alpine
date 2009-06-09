/*
 * Program:	File sync emulator
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

#include <system.h>


#ifndef	HAVE_FSYNC
/* Emulator for BSD fsync() call
 * Accepts: file name
 * Returns: 0 if successful, -1 if failure
 */
int
our_fsync(int fd)
{
    sync();
    return 0;
}
#endif /* !HAVE_FSYNC */

