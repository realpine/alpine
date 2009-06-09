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

#include "windows.h"

#include "mapi.h"
#include "io.h"
#include "fcntl.h"
#include "sys/stat.h"
#include "string.h"
#include "stdio.h"
#include "errno.h"
#include "process.h"
#include "resource.h"
#include "time.h"
#undef   ERROR
#include "../c-client-dll/mail.h"
#include "../c-client-dll/rfc822.h"
#include "../c-client-dll/osdep.h"
#include "../c-client-dll/smtp.h"
#include "../c-client-dll/misc.h"
#include "ctype.h"

#define USER_ID                0
#define PERSONAL_NAME          1
#define USER_DOMAIN            2
#define SMTP_SERVER            3
#define INBOX_PATH             4
#define FEATURE_LIST           5
#define CHARACTER_SET          6
#define FOLDER_COLLECTIONS     7
#define PMAPI_SEND_BEHAVIOR    8
#define DEFAULT_FCC            9
#define PMAPI_SUPPRESS_DIALOGS 10
#define PMAPI_STRICT_NO_DIALOG 11
#define NUMPRCVARS             12
#define ENABLE8BIT          0
#define NUMPRCFEATS         1
#define PMSB_ALWAYS_PROMPT  0
#define PMSB_ALWAYS_SEND    1
#define PMSB_NEVER_SEND     2
#define PMSD_NO             0
#define PMSD_YES            1
#define PMSD_PROMPT         2
#define errBufSize 300	/* for buffers to sprintf %.200s error messages into */
#define BUFLEN 1024
#define EDITLEN 128
#define MSDEBUG  ms_global && ms_global->debug
#define NOT_PINERC 0
#define IS_PINERC 1
#define PINERC_FILE "tmpmapiuwpinerc"
#define ErrorBox(msg,parm){					\
    char buf[errBufSize];					\
    sprintf(buf, msg, parm);					\
    ErrorBoxFunc(buf);	\
    }	/* a macro so parm can be a pointer or a value as per % format */
#define DEBUG_WRITE(msg, parm) { if(MSDEBUG) fprintf(ms_global->dfd,msg,parm);}

#ifdef	ANSI
#define	PROTO(args)	args
#else
#define	PROTO(args)	()
#endif

/* used for pine pwd file */
#define	FIRSTCH		0x20
#define	LASTCH		0x7e
#define	TABSZ		(LASTCH - FIRSTCH + 1)


typedef struct rc_entry{
    char *var;
    union {
	char  *p;
	char **l;
    } val;
    int   islist;
    int   ispmapivar;
} rc_entry_s;

typedef struct rc_feat {
    char *var;
    int   is_set;
} rc_feat_s;

typedef struct STRLIST{
    char *str;
    struct STRLIST *next;
} STRLIST_S;

typedef struct file_struct{
  char *filename;
  struct file_struct *next;
} file_s;

typedef enum {RecipDesc, Message} BufType;

typedef struct mbuffer_list{
    void *buf;
    int   arraysize;   /* should always be 1 unless it's one of those silly arrays */
    BufType type;
    struct mbuffer_list *next;
} MBUFFER_LIST_S;

typedef struct strbuffer{
    char *buf;
    unsigned long cur_bytes;
    unsigned long increment;
    unsigned long bufsize;
} STRBUFFER_S;

typedef struct dlg_edits{
  char edit1[EDITLEN];
  char edit2[EDITLEN];
} dlg_edits_s;

typedef struct pw_cache {
    char user[EDITLEN];
    char pwd[EDITLEN];
    char host[EDITLEN];
    int  validpw;
    struct pw_cache *next;
} pw_cache_s;

typedef struct sessionl{
    NETMBX *mb;
    dlg_edits_s dlge;
    pw_cache_s *pwc;
    MAILSTREAM *open_stream;
    char *currently_open;
    struct {
	unsigned dlg_cancel:1;
	unsigned int mapi_logon_ui:1;
	unsigned int check_stream:1;
	/*  int passfile_checked; */
    } flags;
    file_s *fs;
    lpMapiMessage FAR lpm;
    HWND mhwnd;
    unsigned long session_number;
    struct sessionl *next;
} sessionlist_s;

typedef struct mapi_global{
    char *debugFile;
    int   debug;
    FILE *dfd;
    char *pineExe;
    char *pineExeAlt;
    char *pinerc;
    char *pineconf;
    char *pinercex;
    char *fccfolder;
    int inited;
    char *tmpmbxptr;
    rc_entry_s *prcvars[NUMPRCVARS];
    rc_feat_s  *prcfeats[NUMPRCFEATS];
    int pmapi_send_behavior;
    int pmapi_suppress_dialogs;
    int pmapi_strict_no_dialog;
    sessionlist_s *sessionlist;
    sessionlist_s *cs;   /* the current session, used for logins */
    unsigned long next_session;
    unsigned long attach_no;
    unsigned long attached;
    MBUFFER_LIST_S *mapi_bufs;
    char *attachDir;
    HINSTANCE mhinst;
} mapi_global_s;

extern struct mapi_global *ms_global;

void ErrorBoxFunc(char *msg);
char *quote(char *old);
char *mstrdup(char *old);
int   msprint(char *str);
int   msprint1(char *str, void *arg1);
int   msprint_message_structure(lpMapiMessage lpm);
int   msprint_recipient_structure(lpMapiRecipDesc lmrd, int mapi_orig_is_unexpected);
int   msprint_file_structure(lpMapiFileDesc lmfd);

int   est_size(ADDRESS *a);
int send_documents(char *files, char sep);
unsigned long send_msg_nodlg(LHANDLE lhSession, ULONG ulUIParam, 
			     lpMapiMessage lpMessage, FLAGS flFlags, ULONG ulReserved);
ADDRESS *mapirecip2address(lpMapiRecipDesc lpmrd);
long pmapi_soutr(STRBUFFER_S *s, char *str);
char *TmpCopy(char *srcFile, int is_pinerc);
sessionlist_s *new_sessionlist();
sessionlist_s *free_sessionlist_node(sessionlist_s *cs);
sessionlist_s *get_session(unsigned long num);
lpMapiRecipDesc new_MapiRecipDesc(int arraysize);
void free_MapiRecipDesc(lpMapiRecipDesc buf, int arraysize);
lpMapiMessage new_MapiMessage(int arraysize);
void free_MapiMessage(lpMapiMessage buf, int arraysize);
int new_mbuffer(void *buf, int arraysize, BufType type);
int free_mbuffer(void *buf);
int InitPineSpecific(sessionlist_s *cs);
MAILSTREAM *mapi_mail_open(sessionlist_s *cs, MAILSTREAM *stream, char *name, long options);
MAILSTREAM *check_mailstream(sessionlist_s *cs);
unsigned long convert_to_msgno(char *msgid);
int fetch_structure_and_attachments(long msgno, long flags, 
				    FLAGS MAPIflags, sessionlist_s *cs);
char *message_structure_to_mailto_url(lpMapiMessage lpm);
ULONG FAR PASCAL MAPISendMail(LHANDLE lhSession, ULONG ulUIParam, lpMapiMessage lpMessage,
			      FLAGS flFlags, ULONG ulReserved);
ULONG FAR PASCAL MAPILogon(ULONG ulUIParam, LPTSTR lpszProfileName, LPTSTR lpszPassword,
			   FLAGS flFlags, ULONG ulReserved, LPLHANDLE lplhSession);
ULONG FAR PASCAL MAPILogoff (LHANDLE lhSession, ULONG ulUIParam, FLAGS flFlags, ULONG ulReserved);
ULONG FAR PASCAL MAPIFindNext (LHANDLE lhSession, ULONG ulUIParam, LPSTR lpszMessageType,
			       LPSTR lpszSeedMessageID, FLAGS flFlags, ULONG ulReserved,
			       LPSTR lpszMessageID);
ULONG FAR PASCAL MAPIReadMail(LHANDLE lhSession, ULONG ulUIParam, LPSTR lpszMessageID,
			      FLAGS flFlags, ULONG ulReserved, lpMapiMessage FAR *lppMessage);
ULONG FAR PASCAL MAPIAddress(LHANDLE lhSession, ULONG ulUIParam, LPTSTR lpszCaption,                 
			     ULONG nEditFields, LPTSTR lpszLabels, ULONG nRecips, 
			     lpMapiRecipDesc lpRecips, FLAGS flFlags, ULONG ulReserved,
			     LPULONG lpnNewRecips, lpMapiRecipDesc FAR * lppNewRecips);
ULONG FAR PASCAL MAPIDeleteMail(LHANDLE lhSession, ULONG ulUIParam, LPTSTR lpszMessageID,   
				FLAGS flFlags, ULONG ulReserved);
ULONG FAR PASCAL MAPIDetails(LHANDLE lhSession, ULONG ulUIParam, lpMapiRecipDesc lpRecip,   
			     FLAGS flFlags, ULONG ulReserved);
ULONG FAR PASCAL MAPIFreeBuffer(LPVOID pv);
ULONG FAR PASCAL MAPIResolveName(LHANDLE lhSession, ULONG ulUIParam, LPTSTR lpszName,
				 FLAGS flFlags, ULONG ulReserved, lpMapiRecipDesc FAR * lppRecip);
ULONG FAR PASCAL MAPISaveMail(LHANDLE lhSession, ULONG ulUIParam, lpMapiMessage lpMessage,
			      FLAGS flFlags, ULONG ulReserved, LPTSTR lpszMessageID);
ULONG FAR PASCAL MAPISendDocuments(ULONG ulUIParam, LPTSTR lpszDelimChar, LPTSTR lpszFullPaths,   
				   LPTSTR lpszFileNames, ULONG ulReserved);

/* rfc1522.c */
char	   *rfc1522_encode PROTO((char *, size_t, unsigned char *, char *));
void	    get_pair PROTO((char *, char **, char **, int, int));
void	    removing_leading_and_trailing_white_space PROTO((char *));
