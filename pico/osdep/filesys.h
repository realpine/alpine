/*
 * $Id: filesys.h 764 2007-10-23 23:44:49Z hubert@u.washington.edu $
 *
 * ========================================================================
 * Copyright 2006-2007 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */


#ifndef PICO_OSDEP_FILESYS_INCLUDED
#define PICO_OSDEP_FILESYS_INCLUDED


#include "../../pith/osdep/canaccess.h" /* for *_ACCESS */


/* exported prototypes */
int	 fexist(char *, char *, off_t *);
int	 isdir(char *, long *, time_t *);
char	*gethomedir(int *);
int	 homeless(char *);
char	*getfnames(char *, char *, int *, char *, size_t);
void	 fioperr(int, char *);
char	*pfnexpand(char *, size_t);
void	 fixpath(char *, size_t);
int	 compresspath(char *, char *, size_t);
void	 tmpname(char *, char *);
void	 makename(char *, char *);
int	 copy(char *, char *);
int	 ffwopen(char *, int);
int	 ffclose(void);
int	 ffelbowroom(void);


#endif /* PICO_OSDEP_FILESYS_INCLUDED */
