/*
 * $Id: mimedisp.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PITH_OSDEP_MIMEDISP_INCLUDED
#define PITH_OSDEP_MIMEDISP_INCLUDED


/*
 * Exported Prototypes
 */
int	mime_os_specific_access(void);
int	mime_get_os_mimetype_command(char *, char *, char *, int, int, int *);
int	mime_get_os_mimetype_from_ext(char *, char *, int);
int	mime_get_os_ext_from_mimetype(char *, char *, int);

#endif /* PITH_OSDEP_MIMEDISP_INCLUDED */
