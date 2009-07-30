/* ========================================================================
 * Copyright 2006-2008 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */

#include <system.h>
#include "../../../c-client/mail.h"
#include "../../../c-client/misc.h"
#include "../../../c-client/osdep.h"

#define PROTOTYPE(x) x
#include <gssapi/gssapi_generic.h>
#include <gssapi/gssapi_krb5.h>

long auth_gssapi_proxy_valid (void);
long auth_gssapi_proxy_client (authchallenge_t challenger,
			       authrespond_t responder,
			       char *service,
			       NETMBX *mb,void *stream,unsigned long *trial,
			       char *user);
char *auth_gssapi_proxy_server (authresponse_t responder,int argc,char *argv[]);

AUTHENTICATOR auth_gss_proxy = {
  AU_SECURE | AU_AUTHUSER,	/* secure authenticator */
  "GSSAPI",			/* authenticator name */
  auth_gssapi_proxy_valid,	/* check if valid */
  auth_gssapi_proxy_client,     /* client method */
  auth_gssapi_proxy_server,	/* server method */
  NIL				/* next authenticator */
};

#define AUTH_GSSAPI_P_NONE 1
#define AUTH_GSSAPI_P_INTEGRITY 2
#define AUTH_GSSAPI_P_PRIVACY 4

#define AUTH_GSSAPI_C_MAXSIZE 8192

#define SERVER_LOG(x,y) syslog (LOG_ALERT,x,y)

extern char *krb5_defkeyname;	/* sneaky way to get this name */


/* Placate const declarations */

static gss_OID auth_gss_proxy_mech;
static gss_OID_set auth_gss_proxy_mech_set;

/* Check if GSSAPI valid on this system
 * Returns: T if valid, NIL otherwise
 */

long auth_gssapi_proxy_valid (void)
{
  char *s,tmp[MAILTMPLEN];
  OM_uint32 smn;
  gss_buffer_desc buf;
  gss_name_t name;
  struct stat sbuf;
  sprintf (tmp,"host@%s",mylocalhost ());
  buf.length = strlen (buf.value = tmp) + 1;
  memcpy (&auth_gss_proxy_mech,&gss_mech_krb5,sizeof (gss_OID));
  memcpy (&auth_gss_proxy_mech_set,&gss_mech_set_krb5,sizeof (gss_OID_set));
				/* see if can build a name */
  if (gss_import_name (&smn,&buf,gss_nt_service_name,&name) != GSS_S_COMPLETE)
    return NIL;			/* failed */
  if ((s = strchr (krb5_defkeyname,':')) && stat (++s,&sbuf))
    auth_gss_proxy.server = NIL;	/* can't do server if no keytab */
  gss_release_name (&smn,&name);/* finished with name */
  return LONGT;
}

/* Proxy client authenticator (mihodge)
 * Accepts: challenger function
 *	    responder function
 *	    parsed network mailbox structure
 *	    stream argument for functions
 *	    pointer to current trial count
 *	    returned user name
 * Returns: T if success, NIL otherwise, number of trials incremented if retry
 */

#ifndef AUTH_GSS_PROXY_PATH
#define AUTH_GSS_PROXY_PATH "/usr/local/libexec/alpine/bin/wp_gssapi_proxy"
#endif
#define AUTH_GSS_PROXY_MESSAGE 1
#define AUTH_GSS_PROXY_READ 2
#define AUTH_GSS_PROXY_SUCCESS 3 
#define AUTH_GSS_PROXY_WRITE 4
#define AUTH_GSS_PROXY_WRITE_NIL 5
#define AUTH_GSS_PROXY_EXEC_FAILED 6

static unsigned long read_full(int fd,void *buf,unsigned long size) {
  unsigned long total,s;
  for(total = 0; total < size; total += s) {
    s = read(fd,(char*)buf + total,size - total);
    if(s == -1) {
      if((errno == EAGAIN) || (errno == EINTR)) s = 0;
      else return -1;
    } else if(s == 0) break;
  }
  return total; 
}

static unsigned long write_full(int fd,void *buf,unsigned long size) {
  unsigned long total,s;
  for(total = 0; total < size; total += s) {
    s = write(fd,(char*)buf + total,size - total);
    if(s == -1) {
      if((errno == EAGAIN) || (errno == EINTR)) s = 0;
      else return -1;
    }
  }
  return total; 
}

long auth_gssapi_proxy_client (authchallenge_t challenger,
			       authrespond_t responder,
			       char *service,
			       NETMBX *mb,void *stream,unsigned long *trial,
			       char *user)
{
  char err[MAILTMPLEN];
  int status, ipipe[2], opipe[2];
  unsigned long len,cmd[2],maxlogintrials;
  char *buf;
  pid_t pid;
  long ret = NIL;
  
  imap_parameters(GET_MAXLOGINTRIALS, (void *) &maxlogintrials);
  *trial = maxlogintrials + 1;
  err[0] = 0;
  strcpy (user,mb->user[0] ? mb->user : myusername ());
  
  /* open pipe to gss proxy process */  
  if(pipe(ipipe)) {
    sprintf (err,"auth_gss_proxy: create pipe error: %s",strerror(errno));
  } else if(pipe(opipe)) {
    sprintf (err,"auth_gss_proxy: create pipe error: %s",strerror(errno));
    close(ipipe[0]);
    close(ipipe[1]);
  } else if((pid = fork()) == -1) {
    sprintf (err,"auth_gss_proxy: fork error: %s",strerror(errno));
    close(ipipe[0]);
    close(ipipe[1]);
    close(opipe[0]);
    close(opipe[1]);

  } else if(pid == 0) { /* child process */
    close(ipipe[0]);
    close(opipe[1]);
    dup2(opipe[0],0);
    dup2(ipipe[1],1);
    close(opipe[0]);    
    close(ipipe[1]);
    
    /* a little overloading of the "err" field here */
    sprintf (err,"%s@%s",service,mb->host);
    execlp(AUTH_GSS_PROXY_PATH,AUTH_GSS_PROXY_PATH,err,user,0);
    
    /* tell parent that exec failed and exit */
    cmd[0] = AUTH_GSS_PROXY_EXEC_FAILED;
    cmd[1] = 0;
    write_full(1,cmd,sizeof(cmd));
    exit(-1);
    
  } else { /* parent process */
    close(ipipe[1]);
    close(opipe[0]);
	
    while(len = read_full(ipipe[0],cmd,sizeof(cmd))) {
      if (len == -1) {
	sprintf (err,"auth_gss_proxy: read error: %s",strerror(errno));
	break;
      } else if (len != sizeof(cmd)) {
	sprintf (err,"auth_gss_proxy: read error: %lu out of %lu",
		 len,sizeof(cmd));
	break;

      } else if(cmd[0] == AUTH_GSS_PROXY_EXEC_FAILED) {
	sprintf (err,"auth_gss_proxy: could not spawn proxy process");
	break;
	
      } else if(cmd[0] == AUTH_GSS_PROXY_MESSAGE) {
	if(cmd[1]) {
	  buf = fs_get(cmd[1]);
	  len = read_full(ipipe[0],buf,cmd[1]);
	  if(len == -1) {
	    sprintf (err,"auth_gss_proxy: read error: %s",strerror(errno));
	    break;
	  } else if(len != cmd[1]) {
	    sprintf (err,"auth_gss_proxy: read error: %lu out of %lu",
		     len,cmd[1]);
	    break;
	  } else {
	    mm_log(buf,WARN);
	  }
	  fs_give ((void **) &buf);
	}
	
      } else if(cmd[0] == AUTH_GSS_PROXY_READ) {
	buf = (*challenger) (stream,&len);
	if(!buf) len = 0;
	if(write_full(opipe[1],&len,sizeof(len)) == -1) {
	  sprintf (err,"auth_gss_proxy: write error: %s",strerror(errno));
	  break;
	} else if (buf && (write_full(opipe[1],buf,len) == -1)) {
	  sprintf (err,"auth_gss_proxy: write error: %s",strerror(errno));
	  break;
	}
	if(buf) fs_give ((void **) &buf);

      } else if(cmd[0] == AUTH_GSS_PROXY_SUCCESS) {
   	ret = T;
	
      } else if(cmd[0] == AUTH_GSS_PROXY_WRITE) {
	if(cmd[1]) {
	  buf = fs_get(cmd[1]);
	  len = read_full(ipipe[0],buf,cmd[1]);
	  if(len == -1) {
	    sprintf (err,"auth_gss_proxy: read error: %s",strerror(errno));
	    break;
	  } else if(len != cmd[1]) {
	    sprintf (err,"auth_gss_proxy: read error: %lu out of %lu",
		     len,cmd[1]);
	    break;
	  } else {
	    (*responder) (stream,buf,cmd[1]);
	  }
	  fs_give ((void **) &buf);
	} else {
	  (*responder) (stream,"",0);
	}
	
      } else if(cmd[0] == AUTH_GSS_PROXY_WRITE_NIL) {
	(*responder) (stream,NIL,0);
	
      } else {
	sprintf (err,"auth_gss_proxy: unknown command: %lu",cmd[0]);
	break;
      }
    }
    
    /* close pipes and wait for process to die */
    close(ipipe[0]);
    waitpid(pid,&status,0);
    close(opipe[1]);
  }
  
  if(err[0]) mm_log(err,WARN);
  return ret;
}

/* Server authenticator
 * Accepts: responder function
 *	    argument count
 *	    argument vector
 * Returns: authenticated user name or NIL
 */

char *auth_gssapi_proxy_server (authresponse_t responder,int argc,char *argv[])
{
  char *ret = NIL;
  char *s,tmp[MAILTMPLEN];
  unsigned long maxsize = htonl (AUTH_GSSAPI_C_MAXSIZE);
  int conf;
  OM_uint32 smj,smn,dsmj,dsmn,flags;
  OM_uint32 mctx = 0;
  gss_name_t crname,name;
  gss_OID mech;
  gss_buffer_desc chal,resp,buf;
  gss_cred_id_t crd;
  gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;
  gss_qop_t qop = GSS_C_QOP_DEFAULT;
				/* make service name */
  sprintf (tmp,"%s@%s",(char *) mail_parameters (NIL,GET_SERVICENAME,NIL),
	   tcp_serverhost ());
  buf.length = strlen (buf.value = tmp) + 1;
				/* acquire credentials */
  if ((gss_import_name (&smn,&buf,gss_nt_service_name,&crname)) ==
      GSS_S_COMPLETE) {
    if ((smj = gss_acquire_cred (&smn,crname,0,auth_gss_proxy_mech_set,GSS_C_ACCEPT,
				 &crd,NIL,NIL)) == GSS_S_COMPLETE) {
      if (resp.value = (*responder) ("",0,(unsigned long *) &resp.length)) {
	do {			/* negotiate authentication */
	  smj = gss_accept_sec_context (&smn,&ctx,crd,&resp,
					GSS_C_NO_CHANNEL_BINDINGS,&name,&mech,
					&chal,&flags,NIL,NIL);
				/* don't need response any more */
	  fs_give ((void **) &resp.value);
	  switch (smj) {	/* how did it go? */
	  case GSS_S_COMPLETE:	/* successful */
				/* paranoia */
	    if (memcmp (mech->elements,auth_gss_proxy_mech->elements,mech->length))
	      fatal ("GSSAPI is bogus");
	  case GSS_S_CONTINUE_NEEDED:
	    if (chal.value) {	/* send challenge, get next response */
	      resp.value = (*responder) (chal.value,chal.length,
					 (unsigned long *) &resp.length);
	      gss_release_buffer (&smn,&chal);
	    }
	    break;
	  }
	}
	while (resp.value && resp.length && (smj == GSS_S_CONTINUE_NEEDED));

				/* successful exchange? */
	if ((smj == GSS_S_COMPLETE) &&
	    (gss_display_name (&smn,name,&buf,&mech) == GSS_S_COMPLETE)) {
				/* extract authentication ID from principal */
	  if (s = strchr ((char *) buf.value,'@')) *s = '\0';
				/* send security and size */
	  memcpy (resp.value = tmp,(void *) &maxsize,resp.length = 4);
	  tmp[0] = AUTH_GSSAPI_P_NONE;
	  if (gss_wrap (&smn,ctx,NIL,qop,&resp,&conf,&chal) == GSS_S_COMPLETE){
	    resp.value = (*responder) (chal.value,chal.length,
				       (unsigned long *) &resp.length);
	    gss_release_buffer (&smn,&chal);
	    if (gss_unwrap (&smn,ctx,&resp,&chal,&conf,&qop)==GSS_S_COMPLETE) {
				/* client request valid */
	      if (chal.value && (chal.length > 4) && (chal.length < MAILTMPLEN)
		  && memcpy (tmp,chal.value,chal.length) &&
		  (tmp[0] & AUTH_GSSAPI_P_NONE)) {
				/* tie off authorization ID */
		tmp[chal.length] = '\0';
		if (authserver_login (tmp+4,buf.value,argc,argv) ||
		    authserver_login (lcase (tmp+4),buf.value,argc,argv))
		  ret = myusername ();
	      }
				/* done with user name */
	      gss_release_buffer (&smn,&chal);
	    }
				/* finished with response */
	    fs_give ((void **) &resp.value);
	  }
				/* don't need name buffer any more */
	  gss_release_buffer (&smn,&buf);
	}
				/* don't need client name any more */
	gss_release_name (&smn,&name);
				/* don't need context any more */
	if (ctx != GSS_C_NO_CONTEXT) gss_delete_sec_context (&smn,&ctx,NIL);
      }
				/* finished with credentials */
      gss_release_cred (&smn,&crd);
    }

    else {			/* can't acquire credentials! */
      if (gss_display_name (&dsmn,crname,&buf,&mech) == GSS_S_COMPLETE)
	SERVER_LOG ("Failed to acquire credentials for %s",buf.value);
      if (smj != GSS_S_FAILURE) do
	switch (dsmj = gss_display_status (&dsmn,smj,GSS_C_GSS_CODE,
					   GSS_C_NO_OID,&mctx,&resp)) {
	case GSS_S_COMPLETE:
	  mctx = 0;
	case GSS_S_CONTINUE_NEEDED:
	  SERVER_LOG ("Unknown GSSAPI failure: %s",resp.value);
	  gss_release_buffer (&dsmn,&resp);
	}
      while (dsmj == GSS_S_CONTINUE_NEEDED);
      do switch (dsmj = gss_display_status (&dsmn,smn,GSS_C_MECH_CODE,
					    GSS_C_NO_OID,&mctx,&resp)) {
      case GSS_S_COMPLETE:
      case GSS_S_CONTINUE_NEEDED:
	SERVER_LOG ("GSSAPI mechanism status: %s",resp.value);
	gss_release_buffer (&dsmn,&resp);
      }
      while (dsmj == GSS_S_CONTINUE_NEEDED);
    }
				/* finished with credentials name */
    gss_release_name (&smn,&crname);
  }
  return ret;			/* return status */
}
