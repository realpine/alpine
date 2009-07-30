/*
 * $Id: imap.h 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $
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
void	mm_expunged_current(long unsigned int);

#ifdef	LOCAL_PASSWD_CACHE
int     get_passfile_passwd(char *, char *, char *, STRLIST_S *, int);
int     is_using_passfile();
void    set_passfile_passwd(char *, char *, char *, STRLIST_S *, int, int);
char   *get_passfile_user(char *, STRLIST_S *);
#endif	/* LOCAL_PASSWD_CACHE */

#if	(WINCRED > 0)
void    erase_windows_credentials(void);
#endif

#ifdef	APPLEKEYCHAIN
void    macos_erase_keychain(void);
#endif


#endif /* PINE_IMAP_INCLUDED */
