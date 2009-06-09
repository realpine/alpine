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

#include <system.h>
#include <general.h>

#include "wp_uidmapper_lib.h"


static char *xdigits = "0123456789ABCDEF";

static int do_handshake(char *sockname,
			struct iovec *out,int outlen,int *routbytes,
			struct iovec *in,int inlen,int *rinbytes) {
  int sock,i;
  struct msghdr mh;
  struct sockaddr_un sun;
  
#ifdef DGRAM_MODE
  sock = socket(AF_UNIX,SOCK_DGRAM,0);
#else
  sock = socket(AF_UNIX,SOCK_STREAM,0);
#endif
  if(sock < 0) return -1;
  
  sun.sun_family = AF_UNIX;
  strcpy(sun.sun_path,sockname);
  if(connect(sock,(struct sockaddr*)&sun,sizeof(sun))) return -1;
  
  mh.msg_name = NULL;
  mh.msg_namelen = 0;
  mh.msg_iov = out;
  mh.msg_iovlen = outlen;
  mh.msg_control = NULL;
  mh.msg_controllen = 0;
  mh.msg_flags = 0;
  
  if((i = sendmsg(sock,&mh,0)) == -1) {
    close(sock);
    return -1;
  }  
  if(routbytes) *routbytes = i;

  if(in) {
    mh.msg_iov = in;
    mh.msg_iovlen = inlen;
    mh.msg_flags = 0;
    if((i = recvmsg(sock,&mh,0)) == -1) {
      close(sock);
      return -1;
    }
    if(rinbytes) *rinbytes = i;
  }
  close(sock);
  return 0;
}

int wp_uidmapper_getuid(char *name,unsigned int *key,int *puid) {
  int uid,outbytes,inbytes;
  char cmd;
  struct iovec out[3],in[1];
  
  cmd = 'u';
  out[0].iov_base = &cmd;
  out[0].iov_len = 1;
  out[1].iov_base = key;
  out[1].iov_len = WP_KEY_LEN * sizeof(unsigned int);
  out[2].iov_base = name ? name : "";
  out[2].iov_len = name ? strlen(name) : 0;
  in[0].iov_base = &uid;
  in[0].iov_len = sizeof(uid);

  if(do_handshake(WP_UIDMAPPER_SOCKET,out,3,&outbytes,in,1,&inbytes))
    return -1;
  if((outbytes != (1 + out[1].iov_len) + out[2].iov_len) || (inbytes != in[0].iov_len))
    return -1;
  *puid = uid;
  return 0;
}

int wp_uidmapper_getname(int uid,char *name,int namelen) {
  int outbytes,inbytes;
  char cmd;
  struct iovec out[2],in[1];

  cmd = 'n';
  out[0].iov_base = &cmd;
  out[0].iov_len = 1;
  out[1].iov_base = &uid;
  out[1].iov_len = sizeof(uid);
  in[0].iov_base = name;
  in[0].iov_len = namelen - 1;
  
  if(do_handshake(WP_UIDMAPPER_SOCKET,out,2,&outbytes,in,1,&inbytes))
    return -1;
  if(outbytes != (1 + out[1].iov_len)) return -1;
  name[inbytes] = 0;
  return 0;
}

int wp_uidmapper_clear() {
  int outbytes;
  char cmd;
  struct iovec out[1];

  cmd = 'c';
  out[0].iov_base = &cmd;
  out[0].iov_len = 1;
  
  if(do_handshake(WP_UIDMAPPER_SOCKET,out,1,&outbytes,NULL,0,NULL)) return -1;
  if(outbytes != 1) return -1;
  return 0;
}

int wp_uidmapper_quit() {
  int outbytes;
  char cmd;
  struct iovec out[1];

  cmd = 'q';
  out[0].iov_base = &cmd;
  out[0].iov_len = 1;
  
  if(do_handshake(WP_UIDMAPPER_SOCKET,out,1,&outbytes,NULL,0,NULL)) return -1;
  if(outbytes != 1) return -1;
  return 0;
}

int wp_sessid2key(char *ids,unsigned int *ida) {
    int   i, j, k;
    unsigned int n;
    char *p;

    i = j = 0;
    while(1){
	n = 0;
	for(k = 0; k < 8; k++)
	  if((p = strchr(xdigits, toupper((int) ids[j++]))) != NULL)
	    n = (n << 4) | (p - xdigits);
	  else
	    return(0);

	ida[i] = n;
	if(++i == WP_KEY_LEN)
	  return(ids[j] == '\0');
	else if(ids[j++] != '.')
	  return(0);
    }
}

int wp_parse_cookie (char *cookie,char *cname,char *terms,char *cvalue,int vmax) {
    char *p, *q;
    int   i;

    if((p = strstr(cookie, cname)) != NULL){
	p += strlen(cname) + 1;			/* skip cname and equals */
	q = cvalue;
	while(*p && !strchr(terms, *p)){
	    *q++ = *p++;
	    if((q - cvalue) >= vmax)
	      return(0);
	}

	*q = '\0';
	return(1);
    }

    return(0);
}
