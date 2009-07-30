/*
 * $Id: news.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PITH_NEWS_INCLUDED
#define PITH_NEWS_INCLUDED


#include "../pith/conftype.h"


/*
 * Useful macro to test if current folder is a bboard type (meaning
 * netnews for now) collection...
 */
#define	IS_NEWS(S)	((S) ? ns_test((S)->mailbox, "news") : 0)


/* exported protoypes */
int         ns_test(char *, char *);
int         news_in_folders(struct variable *);
int	    news_grouper(char *, char **, char **, char **, void (*)(void));
void	    free_newsgrp_cache(void);


#endif /* PITH_NEWS_INCLUDED */
