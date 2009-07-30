#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: imap.c 1142 2008-08-13 17:22:21Z hubert@u.washington.edu $";
#endif

/* ========================================================================
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

/*======================================================================
    imap.c
    The call back routines for the c-client/imap
       - handles error messages and other notification
       - handles prelimirary notification of new mail and expunged mail
       - prompting for imap server login and password 

 ====*/

#include <system.h>
#include <general.h>

#include "../../../c-client/c-client.h"

#include "../../../pith/state.h"
#include "../../../pith/debug.h"
#include "../../../pith/string.h"
#include "../../../pith/flag.h"
#include "../../../pith/imap.h"
#include "../../../pith/status.h"
#include "../../../pith/osdep/collate.h"

#include "debug.h"
#include "alpined.h"



/*
 * Internal prototypes
 */
long  imap_seq_exec(MAILSTREAM *, char *,long (*)(MAILSTREAM *, long, void *), void *);
long  imap_seq_exec_append(MAILSTREAM *, long, void *);


/*
 * Exported globals setup by searching functions to tell mm_searched
 * where to put message numbers that matched the search criteria,
 * and to allow mm_searched to return number of matches.
 */
MAILSTREAM *mm_search_stream;

MM_LIST_S  *mm_list_info;



/*----------------------------------------------------------------------
      Queue imap log message for display in the message line

   Args: string -- The message 
         errflg -- flag set to 1 if pertains to an error

 Result: Message queued for display

 The c-client/imap reports most of it's status and errors here
  ---*/
void
mm_log(char *string, long errflg)
{
    char        message[300];
    char       *occurance;
    int         was_capitalized;
    time_t      now;
    struct tm  *tm_now;

    if(errflg == ERROR){
	dprint((SYSDBG_ERR, "%.*s (%ld)", 128, string, errflg));
    }

    now = time((time_t *)0);
    tm_now = localtime(&now);

    dprint((ps_global->debug_imap ? 0 : (errflg == ERROR ? 1 : 2),
	    "IMAP %2.2d:%2.2d:%2.2d %d/%d mm_log %s: %s\n",
	    tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec, tm_now->tm_mon+1,
	    tm_now->tm_mday,
	    (errflg == ERROR)
	      ? "ERROR"
	      : (errflg == WARN)
		  ? "warn"
		  : (errflg == PARSE)
		      ? "parse"
		      : "babble",
	    string));

    if(errflg == ERROR && !strncmp(string, "[TRYCREATE]", 11)){
	ps_global->try_to_create = 1;
	return;
    }
    else if(ps_global->try_to_create
	    || (sp_dead_stream(ps_global->mail_stream)
	   && (!strncmp(string, "[CLOSED]", 8) || strstr(string, "No-op"))))
      /*
       * Don't display if creating new folder OR
       * warning about a dead stream ...
       */
      return;

    /*---- replace all "mailbox" with "folder" ------*/
    strncpy(message, string, sizeof(message));
    message[sizeof(message) - 1] = '\0';
    occurance = srchstr(message, "mailbox");
    while(occurance) {
	if(!*(occurance+7) || isspace((unsigned char)*(occurance+7))){
	    was_capitalized = isupper((unsigned char)*occurance);
	    rplstr(occurance, 7, 7, (errflg == PARSE ? "address" : "folder"));
	    if(was_capitalized)
	      *occurance = (errflg == PARSE ? 'A' : 'F');
	}
	else
	  occurance += 7;

        occurance = srchstr(occurance, "mailbox");
    }

    if(errflg == ERROR)
      ps_global->mm_log_error = 1;

    if(errflg == PARSE || (errflg == ERROR && ps_global->noshow_error)){
      strncpy(ps_global->c_client_error, message, sizeof(ps_global->c_client_error));
      ps_global->c_client_error[sizeof(ps_global->c_client_error)-1] = '\0';
    }

    if(ps_global->noshow_error
       || (ps_global->noshow_warn && errflg == WARN)
       || !(errflg == ERROR || errflg == WARN))
      return; /* Only care about errors; don't print when asked not to */

    /*---- Display the message ------*/
    q_status_message((errflg == ERROR) ? (SM_ORDER | SM_DING) : SM_ORDER,
		     3, 5, message);
    if(errflg == ERROR){
      strncpy(ps_global->last_error, message, sizeof(ps_global->last_error));
      ps_global->last_error[sizeof(ps_global->last_error)-1] = '\0';
    }
}



/*----------------------------------------------------------------------
         recieve notification from IMAP

  Args: stream  --  Mail stream message is relavant to 
        string  --  The message text
        errflag --  Set if it is a serious error

 Result: message displayed in status line

 The facility is for general notices, such as connection to server;
 server shutting down etc... It is used infrequently.
  ----------------------------------------------------------------------*/
void
mm_notify(MAILSTREAM *stream, char *string, long errflag)
{
    if(errflag == ERROR){
	dprint((SYSDBG_ERR, "mm_notify: %s (%ld)", string, errflag));
    }

    /* be sure to log the message... */
#ifdef DEBUG
    if(ps_global->debug_imap)
      dprint((0, "IMAP mm_notify %s : %s (%s) : %s\n",
               (!errflag) ? "NIL" : 
		 (errflag == ERROR) ? "error" :
		   (errflag == WARN) ? "warning" :
		     (errflag == BYE) ? "bye" : "unknown",
	       (stream && stream->mailbox) ? stream->mailbox : "-no folder-",
	      (stream && stream == sp_inbox_stream()) ? "inboxstream" :
		 (stream && stream == ps_global->mail_stream) ? "mailstream" :
		   (stream) ? "abookstream?" : "nostream",
	       string));
#endif

    strncpy(ps_global->last_error, string, 500);
    ps_global->last_error[499] = '\0';

    /*
     * Then either set special bits in the pine struct or
     * display the message if it's tagged as an "ALERT" or
     * its errflag > NIL (i.e., WARN, or ERROR)
     */
    if(errflag == BYE){
	if(stream == ps_global->mail_stream){
	    if(sp_dead_stream(ps_global->mail_stream))
	      return;
	    else
	      sp_set_dead_stream(ps_global->mail_stream, 1);
	}
	else if(stream && stream == sp_inbox_stream()){
	    if(sp_dead_stream(stream))
	      return;
	    else
	      sp_set_dead_stream(stream, 1);
	}
    }
    else if(!strncmp(string, "[TRYCREATE]", 11))
      ps_global->try_to_create = 1;
    else if(!strncmp(string, "[ALERT]", 7))
      q_status_message2(SM_MODAL, 3, 3, "Alert received while accessing \"%s\":  %s",
			(stream && stream->mailbox)
			  ? stream->mailbox : "-no folder-",
			rfc1522_decode_to_utf8((unsigned char *) tmp_20k_buf, SIZEOF_20KBUF, string));
    else if(!strncmp(string, "[UNSEEN ", 8)){
	char *p;
	long  n = 0;

	for(p = string + 8; isdigit(*p); p++)
	  n = (n * 10) + (*p - '0');

	sp_set_first_unseen(ps_global->mail_stream, n);
    }
    else if(!strncmp(string, "[READ-ONLY]", 11)
	    && !(stream && stream->mailbox && IS_NEWS(stream)))
      q_status_message2(SM_ORDER | SM_DING, 3, 3, "%s : %s",
			(stream && stream->mailbox)
			  ? stream->mailbox : "-no folder-",
			string + 11);
    else if(errflag && (errflag == WARN || errflag == ERROR))
      q_status_message(SM_ORDER | ((errflag == ERROR) ? SM_DING : 0),
		       3, 6, ps_global->last_error);
}



/*----------------------------------------------------------------------
      Do work of getting login and password from user for IMAP login
  
  Args:  mb -- The mail box property struct
         user   -- Buffer to return the user name in 
         passwd -- Buffer to return the passwd in
         trial  -- The trial number or number of attempts to login

 Result: username and password passed back to imap
  ----*/
void
mm_login_work(NETMBX *mb, char *user, char *pwd, long trial, char *usethisprompt, char *altuserforcache)
{
    STRLIST_S hostlist[2];
    NETMBX    cmb;
    int	      l;

    pwd[0] = '\0';

    if((l = strlen(mb->orighost)) > 0 && l < CRED_REQ_SIZE)
      strcpy(peCredentialRequestor, mb->orighost);

    if(trial){			/* one shot only! */
	user[0] = '\0';
	peCredentialError = 1;
	return;
    }

#if	0
    if(ps_global && ps_global->anonymous) {
        /*------ Anonymous login mode --------*/
        if(trial < 1) {
            strcpy(user, "anonymous");
            sprintf(pwd, "%s@%s", ps_global->VAR_USER_ID,
		    ps_global->hostname);
	}
	else
	  user[0] = pwd[0] = '\0';

        return;
    }
#endif

#if	WEB_REQUIRE_SECURE_IMAP
    /* we *require* secure authentication */
    if(!(mb->sslflag || mb->tlsflag) && strcmp("localhost",mb->host)){
	user[0] = pwd[0] = '\0';
        return;
    }
#endif

    /*
     * heavily paranoid about offering password to server
     * that the users hasn't also indicated the remote user
     * name
     */
    if(*mb->user){
	strcpy(user, mb->user);
    }
    else if(ps_global->prc
	    && ps_global->prc->name
	    && mail_valid_net_parse(ps_global->prc->name,&cmb)
	    && cmb.user){
	strcpy(user, cmb.user);
    }
    else{
	/*
	 * don't blindly offer user/pass
	 */
	user[0] = pwd[0] = '\0';
        return;
    }

    /*
     * set up host list for sybil servers...
     */
    hostlist[0].name = mb->host;
    if(mb->orighost[0] && strucmp(mb->host, mb->orighost)){
	hostlist[0].next = &hostlist[1];
	hostlist[1].name = mb->orighost;
	hostlist[1].next = NULL;
    }
    else
      hostlist[0].next = NULL;

    /* try last working password associated with this host. */
    if(!imap_get_passwd(mm_login_list, pwd, user, hostlist, (mb->sslflag || mb->tlsflag))){
	peNoPassword = 1;
	user[0] = pwd[0]  = '\0';
    }
}



/*----------------------------------------------------------------------
       Receive notification of an error writing to disk
      
  Args: stream  -- The stream the error occured on
        errcode -- The system error code (errno)
        serious -- Flag indicating error is serious (mail may be lost)

Result: If error is non serious, the stream is marked as having an error
        and deletes are disallowed until error clears
        If error is serious this returns with syslogging if possible
  ----*/
long
mm_diskerror (MAILSTREAM *stream, long errcode, long serious)
{
    if(!serious && stream == ps_global->mail_stream) {
	sp_set_io_error_on_stream(ps_global->mail_stream, 1);
    }

    dprint((SYSDBG_ERR, "mm_diskerror: mailbox: %s, errcode: %ld, serious: %ld\n",
	    (stream && stream->mailbox) ? stream->mailbox : "", errcode, serious));

    return(1);
}


/*
 * alpine_tcptimeout - C-client callback to handle tcp-related timeouts.
 */
long
alpine_tcptimeout(long elapsed, long sincelast)
{
    long rv = 1L;			/* keep trying by default */

    dprint((SYSDBG_INFO, "tcptimeout: waited %s seconds\n", long2string(elapsed)));

    if(elapsed > WP_TCP_TIMEOUT){
	dprint((SYSDBG_ERR, "tcptimeout: BAIL after %s seconds\n", long2string(elapsed)));
	rv = 0L;
    }

    return(rv);
}


/*
 * C-client callback to handle SSL/TLS certificate validation failures
 *
 * Returning 0 means error becomes fatal
 *           Non-zero means certificate problem is ignored and SSL session is
 *             established
 *
 *  We remember the answer and won't re-ask for subsequent open attempts to
 *  the same hostname.
 */
long
alpine_sslcertquery(char *reason, char *host, char *cert)
{
    static char buf[256];
    STRLIST_S *p;

    for(p = peCertHosts; p; p = p->next)
      if(!strucmp(p->name, host))
	return(1);

    peCertQuery = 1;
    snprintf(peCredentialRequestor, CRED_REQ_SIZE, "%s++%s", host ? host : "?", reason ? reason : "UNKNOWN");
    q_status_message(SM_ORDER, 0, 3, "SSL Certificate Problem");
    dprint((SYSDBG_INFO, "sslcertificatequery: host=%s reason=%s cert=%s\n",
	    host ? host : "?", reason ? reason : "?",
	    cert ? cert : "?"));
    return(0);
}


/*
 * C-client callback to handle SSL/TLS certificate validation failures
 */
void
alpine_sslfailure(char *host, char *reason, unsigned long flags)
{
    peCertFailure = 1;
    snprintf(peCredentialRequestor, CRED_REQ_SIZE, "%s++%s", host ? host : "?", reason ? reason : "UNKNOWN");
    q_status_message1(SM_ORDER, 0, 3, "SSL Certificate Failure: %s", reason ? reason : "?");
    dprint((SYSDBG_INFO, "SSL Invalid Cert (%s) : %s", host, reason));
}



/*----------------------------------------------------------------------
  This can be used to prevent the flickering of the check_cue char
  caused by numerous (5000+) fetches by c-client.  Right now, the only
  practical use found is newsgroup subsciption.

  check_cue_display will check if this global is set, and won't clear
  the check_cue_char if set.
  ----*/
void
set_read_predicted(int i)
{
}

/*----------------------------------------------------------------------
   Exported method to display status of mail check

   Args: putstr -- should be NO LONGER THAN 2 bytes

 Result: putstr displayed at upper-left-hand corner of screen
  ----*/
void
check_cue_display(char *putstr)
{
}



void
alpine_set_passwd(char *user, char *passwd, char *host, int altflag)
{
    STRLIST_S hostlist[1];

    hostlist[0].name = host;
    hostlist[0].next = NULL;

    imap_set_passwd(&mm_login_list, passwd, user, hostlist, altflag, 1, 0);
}    


void
alpine_clear_passwd(char *user, char *host)
{
    MMLOGIN_S **lp, *l;
    STRLIST_S  hostlist[1];

    hostlist[0].name = host;
    hostlist[0].next = NULL;

    for(lp = &mm_login_list; *lp; lp = &(*lp)->next)
      if(imap_same_host((*lp)->hosts, hostlist)
	 && (!*user || !strcmp(user, (*lp)->user))){
	  l = *lp;
	  *lp = (*lp)->next;

	  if(l->user)
	    fs_give((void **) &l->user);

	  free_strlist(&l->hosts);

	  if(l->passwd){
	      char *p = l->passwd;

	      while(*p)
		*p++ = '\0';
	  }

	  fs_give((void **) &l);

	  break;
      }
}    


int
alpine_have_passwd(char *user, char *host, int altflag)
{
    STRLIST_S hostlist[1];

    hostlist[0].name = host;
    hostlist[0].next = NULL;

    return(imap_get_passwd(mm_login_list, NULL, user, hostlist, altflag));
}    


char *
alpine_get_user(char *host)
{
    STRLIST_S hostlist[1];

    hostlist[0].name = host;
    hostlist[0].next = NULL;

    return(imap_get_user(mm_login_list, hostlist));
}    
