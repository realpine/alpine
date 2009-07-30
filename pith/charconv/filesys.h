/*-----------------------------------------------------------------------
 $Id: filesys.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
  -----------------------------------------------------------------------*/

/*
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

#ifndef PITH_CHARCONV_FILESYS_INCLUDED
#define PITH_CHARCONV_FILESYS_INCLUDED


#include <general.h>


/*
 * Exported Prototypes
 */
char    *fname_to_locale(char *);
char    *fname_to_utf8(char *);
UCS      read_a_wide_char(FILE *fp, void *input_cs);
int      write_a_wide_char(UCS ucs, FILE *fp);
int      our_stat(char *, struct stat *);
FILE    *our_fopen(char *, char *);
int      our_open(char *, int, mode_t);
int      our_creat(char *, mode_t);
int      our_mkdir(char *, mode_t);
int      our_rename(char *, char *);
int      our_unlink(char *);
int      our_link(char *, char *);
int      our_lstat(char *, struct stat *);
int      our_chmod(char *, mode_t);
int      our_chown(char *, uid_t, gid_t);
int      our_truncate(char *, off_t);
int      our_utime(char *, struct utimbuf *);
int      our_access(char *, int);
char    *our_getenv(char *);
char    *fgets_binary(char *, int, FILE *);


#endif	/* PITH_CHARCONV_FILESYS_INCLUDED */
