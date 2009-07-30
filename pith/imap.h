/*
 * $Id: imap.h 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $
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

#ifndef PITH_IMAP_INCLUDED
#define PITH_IMAP_INCLUDED


#include "../pith/string.h"


#define NETMAXPASSWD 100


/*
 * struct used to keep track of password/host/user triples.
 * The problem is we only want to try user names and passwords if
 * we've already tried talking to this host before.
 * 
 */
typedef struct _mmlogin_s {
    char	   *user,
		   *passwd;
    unsigned	    altflag:1;
    unsigned	    ok_novalidate:1;
    unsigned	    warned:1;
    STRLIST_S	   *hosts;
    struct _mmlogin_s *next;
} MMLOGIN_S;


typedef	struct _se_app_s {
    char *folder;
    long  flags;
} SE_APP_S;


/*
 * struct to help manage mail_list calls/callbacks
 */
typedef	struct mm_list_s {
    MAILSTREAM	*stream;
    unsigned     options;
    void       (*filter)(MAILSTREAM *, char *, int, long, void *, unsigned);
    void	*data;
} MM_LIST_S;


/*
 * return values for IMAP URL parser
 */
#define	URL_IMAP_MASK		0x0007
#define	URL_IMAP_ERROR		0
#define	URL_IMAP_IMAILBOXLIST	0x0001
#define	URL_IMAP_IMESSAGELIST	0x0002
#define	URL_IMAP_IMESSAGEPART	0x0004
#define	URL_IMAP_IMBXLSTLSUB	0x0010
#define	URL_IMAP_ISERVERONLY	0x0020


/*
 * Exported globals setup by searching functions to tell mm_searched
 * where to put message numbers that matched the search criteria,
 * and to allow mm_searched to return number of matches.
 */
extern MAILSTREAM *mm_search_stream;
extern long        mm_search_count;
extern MAILSTATUS  mm_status_result;

extern MM_LIST_S  *mm_list_info;


extern MMLOGIN_S  *mm_login_list;
extern MMLOGIN_S  *cert_failure_list;


/*
 * These are declared in c-client and implemented in ../pith/imap.c
 */
#if 0
 void mm_searched (MAILSTREAM *stream,unsigned long number);
 void mm_exists (MAILSTREAM *stream,unsigned long number);
 void mm_expunged (MAILSTREAM *stream,unsigned long number);
 void mm_flags (MAILSTREAM *stream,unsigned long number);
 void mm_list (MAILSTREAM *stream,int delimiter,char *name,long attributes);
 void mm_lsub (MAILSTREAM *stream,int delimiter,char *name,long attributes);
 void mm_status (MAILSTREAM *stream,char *mailbox,MAILSTATUS *status);
 void mm_dlog (char *string);
 void mm_critical (MAILSTREAM *stream);
 void mm_nocritical (MAILSTREAM *stream);
 void mm_fatal (char *string);
#endif

/*
 * These are declared in c-client and must be implemented in application
 */
#if 0
 void mm_notify (MAILSTREAM *stream,char *string,long errflg);
 void mm_log (char *string,long errflg);
 void mm_login (NETMBX *mb,char *user,char *pwd,long trial);
 long mm_diskerror (MAILSTREAM *stream,long errcode,long serious);
#endif


/* exported protoypes */
char   *imap_referral(MAILSTREAM *, char *, long);
long    imap_proxycopy(MAILSTREAM *, char *, char *, long);
char   *cached_user_name(char *);
int     imap_same_host(STRLIST_S *, STRLIST_S *);
int     imap_get_ssl(MMLOGIN_S *, STRLIST_S *, int *, int *);
char   *imap_get_user(MMLOGIN_S *, STRLIST_S *);
int     imap_get_passwd(MMLOGIN_S *, char *, char *, STRLIST_S *, int);
void    imap_set_passwd(MMLOGIN_S **, char *, char *, STRLIST_S *, int, int, int);
void    imap_flush_passwd_cache(int);


/* currently mandatory to implement stubs */

/* called by build_folder_list(), ok if it does nothing */
void    set_read_predicted(int);
void    mm_login_work (NETMBX *mb,char *user,char *pwd,long trial,char *usethisprompt, char *altuserforcache);


#endif /* PITH_IMAP_INCLUDED */
