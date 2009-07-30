/*
 * $Id: dispfilt.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PINE_DISPFILT_INCLUDED
#define PINE_DISPFILT_INCLUDED



/* exported protoypes */
char	*dfilter(char *, STORE_S *, gf_io_t, FILTLIST_S *);
char	*dfilter_trigger(BODY *, char *, size_t);
char	*expand_filter_tokens(char *, ENVELOPE *, char **, char **, char **, int *, int *);
char	*filter_session_key(void);
char	*filter_data_file(int);



#endif /* PINE_DISPFILT_INCLUDED */
