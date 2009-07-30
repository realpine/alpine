/*
 * $Id: bldpath.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PITH_OSDEP_BLDPATH_INCLUDED
#define PITH_OSDEP_BLDPATH_INCLUDED


void	 build_path(char *, char *, char *, size_t);
int	 is_absolute_path(char *);
int	 is_rooted_path(char *);
int	 is_homedir_path(char *);
char	*homedir_in_path(char *);
char	*filename_parent_ref(char *);
int	 filename_is_restricted(char *);


#endif /* PITH_OSDEP_BLDPATH_INCLUDED */
