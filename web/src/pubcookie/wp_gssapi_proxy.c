/* ========================================================================
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


/* #define PROTOTYPE(x) x */

#include <system.h>
#include <general.h>

#include "wp_uidmapper_lib.h"

#include <gssapi/gssapi_generic.h>
#include <gssapi/gssapi_krb5.h>

#define AUTH_GSS_PROXY_MESSAGE 1
#define AUTH_GSS_PROXY_READ 2
#define AUTH_GSS_PROXY_SUCCESS 3 
#define AUTH_GSS_PROXY_WRITE 4
#define AUTH_GSS_PROXY_WRITE_NIL 5

#define AUTH_GSSAPI_P_NONE 1
#define AUTH_GSSAPI_P_INTEGRITY 2
#define AUTH_GSSAPI_P_PRIVACY 4

#ifdef NO_UIDMAPPER
int get_calling_username(int uid,char *name,int namelen) {
  struct passwd *pw;
  unsigned long len;

  pw = getpwuid(uid);
  if(!pw) return -1;
  len = strlen(pw->pw_name);
  if(len >= namelen) len = namelen - 1;  
  memcpy(name,pw->pw_name,len);
  name[len] = 0;
  return len;
}
#else
#define get_calling_username wp_uidmapper_getname
#endif

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

int cmd_message(char *str1, ...) {
  va_list list;
  unsigned long cmd[2],size;
  char *str;
  
  for(size = 0,str = str1,va_start(list,str1); str; str = va_arg(list,char*))
    size += strlen(str);
  va_end(list);

  cmd[0] = AUTH_GSS_PROXY_MESSAGE;
  cmd[1] = size;
  if(write_full(1,cmd,sizeof(cmd)) == -1) return -1;
  for(str = str1, va_start(list,str1); str; str = va_arg(list,char*))
    if(size = strlen(str)) if(write_full(1,str,size) == -1) {
      va_end(list);
      return -1;
    }
  va_end(list);
  return 0;
}

int cmd_read(gss_buffer_desc *pbuf) {
  unsigned long cmd[2],len,size;
  void *buf;
  
  cmd[0] = AUTH_GSS_PROXY_READ;
  cmd[1] = 0;
  if(write_full(1,cmd,sizeof(cmd)) == -1) return -1;
  
  len = read_full(0,&size,sizeof(size));
  if(len != sizeof(size)) return -1;
  if(size == 0) {
    pbuf->value = 0;
    pbuf->length = 0;
    return 0;
  }   

  buf = malloc(size);
  len = read_full(0,buf,size);
  if(len != size) {
    free(buf);
    return -1;
  }
  pbuf->value = buf;
  pbuf->length = size;
  return 0;
}

int cmd_success() {
  unsigned long cmd[2];  
  cmd[0] = AUTH_GSS_PROXY_SUCCESS;
  cmd[1] = 0;
  if(write_full(1,cmd,sizeof(cmd)) == -1) return -1;
  return 0;
}  

int cmd_write(gss_buffer_desc *buf) {
  unsigned long cmd[2];  
  cmd[0] = AUTH_GSS_PROXY_WRITE;
  cmd[1] = buf->length;
  if(write_full(1,cmd,sizeof(cmd)) == -1) return -1;
  if(buf->length) if(write_full(1,buf->value,buf->length) == -1) return -1;
  return 0;
}

int cmd_write_nil() {
  unsigned long cmd[2];  
  cmd[0] = AUTH_GSS_PROXY_WRITE_NIL;
  cmd[1] = 0;
  if(write_full(1,cmd,sizeof(cmd)) == -1) return -1;
  return 0;
}  

/*
 * service principal in argv[1]
 * reqested username in argv[2]
 */
  
int main(int argc,char *argv[])
{
  char *user = 0;
  char userbuf[WP_BUF_SIZE];
  char *prog;
  OM_uint32 smj,smn,dsmn,mctx;
  gss_name_t crname = GSS_C_NO_NAME;
  gss_ctx_id_t ctx = GSS_C_NO_CONTEXT;
  gss_buffer_desc chal,resp,buf;
  int conf,i;
  gss_qop_t qop;
  gss_OID oid;
  int calling_uid,eff_uid;

  if((prog = strrchr(argv[0], '/')) == NULL)
    prog = argv[0];
  else
    prog++;

  openlog(prog,LOG_PID,LOG_MAIL);

  calling_uid = getuid();
  eff_uid = geteuid();
#ifdef DEBUG
  syslog(LOG_INFO,"uid = %d, euid=%d\n",calling_uid,eff_uid);
#endif

#ifdef WEBSERVER_UID
  /* if euid != uid, change to web server user */
  if(calling_uid != eff_uid) if(setuid(WEBSERVER_UID)) {
    syslog(LOG_ERR,"setuid ((%d != %d) -> %d) failed: %s",
	   calling_uid,eff_uid,WEBSERVER_UID,strerror(errno));
    cmd_write_nil();
    goto cleanup;
  }
#endif

  if(argc < 2) {
    syslog(LOG_WARNING,"not enough arguments");
    cmd_write_nil();
    goto cleanup;
  } 
  if(get_calling_username(calling_uid,userbuf,WP_BUF_SIZE) == -1) {
    syslog(LOG_WARNING,"cannot determine calling username");
    cmd_write_nil();
    goto cleanup;
  }
  if(argc == 2) {
    user = userbuf;
#ifdef DEBUG
    syslog(LOG_INFO,"calling=%s\n",user);
#endif
  } else if(argc > 2) {
    user = argv[2];
#ifdef DEBUG
    syslog(LOG_INFO,"requested=%s calling=%s\n",user,userbuf);
#endif
#ifndef NO_NAME_CHECK
    if(strcmp(user,userbuf)) {
      syslog(LOG_WARNING,"cannot act on behalf of user %s (%s)",user,userbuf);
      cmd_write_nil();
      goto cleanup;
    }
#endif
  }
  
  /* expect empty challenge from server */
  if(cmd_read(&chal)) {
    syslog(LOG_WARNING,"cmd_read[initial] failed");
    goto cleanup;
  } else if(chal.length) {
    free(chal.value);
    syslog(LOG_WARNING,"cmd_read[initial] not empty");
    goto cleanup;
  }  

  /*
   * obtain credentials for requested service
   */
  
  buf.value = argv[1];
  buf.length = strlen(argv[1]);
  if(gss_import_name (&smn,&buf,gss_nt_service_name,&crname) != 
     GSS_S_COMPLETE) {
    syslog(LOG_WARNING,"gss_import_name(%s) failed",buf.value);
    cmd_write_nil();
    goto cleanup;
  }
  
  /* initial init_sec_context call, and send data */
  memcpy(&oid,&gss_mech_krb5,sizeof(oid));  
  smj = gss_init_sec_context
    (&smn,GSS_C_NO_CREDENTIAL,&ctx,crname,oid,
     GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG,0,GSS_C_NO_CHANNEL_BINDINGS,
     GSS_C_NO_BUFFER,0,&resp,0,0);
  if((smj == GSS_S_COMPLETE) || (smj == GSS_S_CONTINUE_NEEDED)) {   
    i = cmd_write(&resp);
    gss_release_buffer (&smn,&resp);
    if(i) {
      syslog(LOG_WARNING,"cmd_write[init_sec_context] failed");
      goto cleanup;
    }    
  }

  /* loop until init_sec_context is done */
  while(smj == GSS_S_CONTINUE_NEEDED) {
    if(cmd_read(&chal)) {
      syslog(LOG_WARNING,"cmd_read[init_sec_context] failed");
      goto cleanup;
    } else if(!chal.length) {
      syslog(LOG_WARNING,"cmd_read[init_sec_context] empty");
      goto cleanup;
    } else {
      smj = gss_init_sec_context (&smn,GSS_C_NO_CREDENTIAL,&ctx,crname,
				  GSS_C_NO_OID,
				  GSS_C_MUTUAL_FLAG|GSS_C_REPLAY_FLAG,0,
				  GSS_C_NO_CHANNEL_BINDINGS,&chal,0,
				  &resp,0,0);
      if(chal.value) free(chal.value);
      if((smj == GSS_S_COMPLETE) || (smj == GSS_S_CONTINUE_NEEDED)) {
	i = cmd_write(&resp);
	gss_release_buffer (&smn,&resp);
	if(i) {
	  syslog(LOG_WARNING,"cmd_write[init_sec_context] failed");	
	  goto cleanup;
	}
      }
    }
  }

  switch(smj) {
  case GSS_S_COMPLETE:
    /* get challenge and unwrap it */
    if(cmd_read(&chal)) {
      syslog(LOG_WARNING,"cmd_read[gss_unwrap] failed");
      goto cleanup;
    }
    smj = gss_unwrap (&smn,ctx,&chal,&resp,&conf,&qop);
    if(chal.value) free(chal.value);
    if(smj != GSS_S_COMPLETE) {
      syslog(LOG_WARNING,"gss_unwrap failed");
      cmd_write_nil();
      goto cleanup;
    } else if(resp.length < 4) {
      syslog(LOG_WARNING,"challenge too short");
      gss_release_buffer (&smn,&resp);
      cmd_write_nil();
      goto cleanup;
    } else if(!( ((char*)resp.value)[0] & AUTH_GSSAPI_P_NONE)) {
      syslog(LOG_WARNING,"invalid challenge");
      gss_release_buffer (&smn,&resp);
      cmd_write_nil();
      goto cleanup;
    }
    
    /* prepare response to challenge */ 
    buf.length = 4 + (user ? strlen(user) : 0);
    buf.value = malloc(buf.length);    
    memcpy (buf.value,resp.value,4);
    gss_release_buffer (&smn,&resp);
    *(char*)buf.value = AUTH_GSSAPI_P_NONE;
    if(user) memcpy((char*)buf.value + 4, user, buf.length - 4);
    
    /* wrap response and send */
    smj = gss_wrap (&smn,ctx,0,qop,&buf,&conf,&resp);
    free(buf.value);
    if(smj != GSS_S_COMPLETE) {
      syslog(LOG_WARNING,"gss_unwrap failed");
      cmd_write_nil();
      goto cleanup;
    }
    i = cmd_write(&resp);
    gss_release_buffer (&smn,&resp);
    if(i) {
      syslog(LOG_WARNING,"cmd_write[gss_wrap] failed");
      goto cleanup;
    }
    
    /* success! */
    if(cmd_success()) syslog(LOG_WARNING,"cmd_success failed");
    goto cleanup;
    
  case GSS_S_CREDENTIALS_EXPIRED:
#ifdef DEBUG
    syslog(LOG_INFO,"Kerberos credentials expired (try running kinit)");
#endif
    if(cmd_message("Kerberos credentials expired (try running kinit)",0)) {
      syslog(LOG_WARNING,"cmd_message[credentials expired] failed");
      goto cleanup;
    }      
    cmd_write_nil();     
    goto cleanup;
    
  case GSS_S_FAILURE:
    if (smn == (OM_uint32) KRB5_FCC_NOFILE) {
#ifdef DEBUG
      syslog(LOG_INFO,"No credentials cache found (try running kinit)");
#endif
      if(cmd_message("No credentials cache found (try running kinit)",0)) {
	syslog(LOG_WARNING,"cmd_message[no cache file found] failed");
	goto cleanup;
      }
    } else {
      mctx = 0;
      do {
	gss_display_status (&dsmn,smn,GSS_C_MECH_CODE,
			    GSS_C_NO_OID,&mctx,&resp);
#ifdef DEBUG
	syslog(LOG_INFO,"GSSAPI failure: %s",resp.value);
#endif
	i = cmd_message("GSSAPI failure: ",resp.value,0);
	gss_release_buffer (&dsmn,&resp);
	if(i) {
	  syslog(LOG_WARNING,"cmd_message[failure] failed");
	  goto cleanup;
	}
      } while(mctx);
    }
    cmd_write_nil();     
    goto cleanup;
    
  default:
    mctx = 0;
    do {
      gss_display_status (&dsmn,smn,GSS_C_GSS_CODE,
			  GSS_C_NO_OID,&mctx,&resp);
#ifdef DEBUG
      syslog(LOG_INFO,"GSSAPI failure: %s",resp.value);
#endif
      i = cmd_message("Unknown GSSAPI failure: ",resp.value,0);
      gss_release_buffer (&dsmn,&resp);
      if(i) {
	syslog(LOG_WARNING,"cmd_message[unknown failure] failed");
	goto cleanup;
      }
    } while(mctx);
    if(!mctx) do {
      gss_display_status (&dsmn,smn,GSS_C_MECH_CODE,
			  GSS_C_NO_OID,&mctx,&resp);
#ifdef DEBUG
      syslog(LOG_INFO,"GSSAPI mechanism status: %s",resp.value);
#endif
      i = cmd_message("GSSAPI mechanism status: ",resp.value,0);
      gss_release_buffer (&dsmn,&resp);
      if(i) {
	syslog(LOG_WARNING,"cmd_message[unknown failure 2] failed");
	goto cleanup;
      }
    } while(mctx);
    cmd_write_nil();     
    goto cleanup;
  }  
  
 cleanup:
  if(ctx != GSS_C_NO_CONTEXT) gss_delete_sec_context (&smn,&ctx,0);
  if(crname != GSS_C_NO_NAME) gss_release_name (&smn,&crname);
  closelog();
  exit(0);
  return 0;
}
