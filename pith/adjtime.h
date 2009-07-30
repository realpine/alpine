/*
 * $Id: adjtime.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PITH_TIME_INCLUDED
#define PITH_TIME_INCLUDED


/*
 * This is just like a struct timeval. We need it for portability to systems
 * that don't have a struct timeval.
 */
typedef struct our_time_val {
    long sec;
    long usec;
} TIMEVAL_S;


/* exported protoypes */
time_t        get_adj_time(void);
time_t        get_adj_name_file_mtime(char *);


#endif /* PITH_TIME_INCLUDED */
