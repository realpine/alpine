/*
 * $Id: imap.h 232 2006-11-15 23:14:41Z hubert@u.washington.edu $
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

#ifndef PINE_IMAP_INCLUDED
#define PINE_IMAP_INCLUDED


#include "../pith/imap.h"


/* exported protoypes */
void   *pine_block_notify(int, void *);
long    pine_tcptimeout(long, long);
long    pine_sslcertquery(char *, char *, char *);
char   *pine_newsrcquery(MAILSTREAM *, char *, char *);
int     url_local_certdetails(char *);
void    pine_sslfailure(char *, char *, unsigned long);
#ifdef	PASSFILE
int     get_passfile_passwd(char *, char *, char *, STRLIST_S *, int);
int     is_using_passfile();
void    set_passfile_passwd(char *, char *, char *, STRLIST_S *, int);
char   *get_passfile_user(char *, STRLIST_S *);
void    update_passfile_hostlist(char *, char *, STRLIST_S *, int);
#ifdef	_WINDOWS
void    erase_windows_credentials(void);
#endif
#endif


#endif /* PINE_IMAP_INCLUDED */
