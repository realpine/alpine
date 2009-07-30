/*
 * $Id: filesize.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PITH_OSDEP_FILESIZE_INCLUDED
#define PITH_OSDEP_FILESIZE_INCLUDED


/*
 * Exported Prototypes
 */
long	name_file_size(char *);
long	fp_file_size(FILE *);
time_t	name_file_mtime(char *);
time_t	fp_file_mtime(FILE *);
void	file_attrib_copy(char *, char *);



#endif /* PITH_OSDEP_FILESIZE_INCLUDED */
