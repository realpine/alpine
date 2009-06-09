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

#include "id_table.h"
#include "wp_uidmapper_lib.h"


/* Makefile should #define:
 * 
 * WP_UIDMAPPER_SOCKET
 */

unsigned long send_full(int fd,void *buf,unsigned long len,int flags) {
  unsigned long total,i;  
  for(total = 0; total < len; total += i) {
    i = send(fd,(char*)buf + total, len - total,flags);
    if(i == -1) return -1;
  }
  return total;
}

static char *socketname = WP_UIDMAPPER_SOCKET;
static int   socketname_remove = 0;

void socketname_cleanup(void) {
  unlink(socketname);
}

void quit_handler(int signal) {
  exit(signal);
}

int main(int argc, char *argv[]) {
  extern char *optarg;
  extern int optind, opterr, optopt;

  int debug,log_opt;
  mode_t sockmode;
  
  id_table_range *range;
  id_table table;
  struct sockaddr_un sun,rsun;
  struct sigaction sa;
  int is_err,i,ssock,uid;
  unsigned int kbuf[WP_KEY_LEN];
  char rbuf[WP_BUF_SIZE],cbuf[WP_BUF_SIZE],rcmd;
  struct msghdr rmh,smh;
  struct iovec riov[3],siov[1];
#ifndef DGRAM_MODE
  int csock,rsun_len;
#endif
  struct ucred cred;
  
  /*
   * process command line arguments
   */

  debug = 0;
  log_opt = 0;
  sockmode = 0600;

  for(is_err = 0; !is_err && ((i = getopt(argc,argv,"dlrm:s:u:")) != -1); ) {
    switch(i) {
    case 'd': debug++; break;
    case 'l': log_opt |= LOG_PERROR; break;
    case 'm': sockmode = strtol(optarg,NULL,0); break;
    case 'r': socketname_remove = 1; break;
    case 's': socketname = optarg; break;
    case 'u': umask(strtol(optarg,NULL,0)); break;
    case '?': is_err = 1; break;
    }
  }
  if((optind + 1) == argc) {
    range = id_table_range_new(argv[optind]);
    if(!range) is_err = 1;
  } else {
    is_err = 1;
  }
  if(is_err) {
    fprintf(stderr,"Usage: uidmapper [-d] [-l] [-r] [-m mode] [-s socketname] [-u umask] uidranges\n");
    exit(1);
  }

  /*
   * main initialization
   */

  openlog(argv[0],log_opt,LOG_MAIL);
  
  if(id_table_init(&table,range)) {
    syslog(LOG_ERR,"could not initialize tables: %s\n",strerror(errno));
    exit(1);
  }
  id_table_range_delete(range);
  
  sa.sa_handler = quit_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGHUP,&sa,0);
  sigaction(SIGINT,&sa,0);
  sigaction(SIGQUIT,&sa,0);  
  sigaction(SIGTERM,&sa,0);

  if(socketname_remove)
    socketname_cleanup();

  /*
   * open socket
   */
  
#ifdef DGRAM_MODE
  ssock = socket(AF_UNIX,SOCK_DGRAM,0);
#else
  ssock = socket(AF_UNIX,SOCK_STREAM,0);
#endif
  if(ssock < 0) {
    syslog(LOG_ERR,"%s: socket: %s\n",socketname,strerror(errno));
    exit(1);
  }
  /* sun.sun_len = strlen(socketname) + 1; */
  sun.sun_family = AF_UNIX;
  strcpy(sun.sun_path,socketname);  
  if(bind(ssock,(struct sockaddr*)&sun,sizeof(sun))) {
    syslog(LOG_ERR,"%s: bind: %s\n",socketname,strerror(errno));
    exit(1);
  }
  atexit(socketname_cleanup);
  chmod(ssock,sockmode);

#ifndef DGRAM_MODE
  if(listen(ssock,8)) {
    syslog(LOG_ERR,"%s: listen: %s\n",socketname,strerror(errno));
    exit(1);
  }
#endif

#ifdef DGRAM_MODE 
  if(debug >= 1) syslog(LOG_INFO,"SOCK_DGRAM socket opened");
#else
  if(debug >= 1) syslog(LOG_INFO,"SOCK_STREAM socket opened");
#endif

  /*
   * accept commands
   */
  
  riov[0].iov_base = &rcmd;
  riov[0].iov_len = 1;
  riov[1].iov_base = kbuf;
  riov[1].iov_len = WP_KEY_LEN * sizeof(unsigned int);
  riov[2].iov_base = rbuf;
  riov[2].iov_len = WP_BUF_SIZE - 1;
  rmh.msg_name = &rsun;
  rmh.msg_namelen = sizeof(rsun);
  rmh.msg_iov = riov;  
  rmh.msg_iovlen = 3;
  rmh.msg_control = cbuf;
  rmh.msg_controllen = riov[0].iov_len + riov[1].iov_len + riov[2].iov_len;
  rmh.msg_flags = 0;

  /* siov[0].iov_base */
  /* siov[0].iov_len */
  smh.msg_name = NULL;
  smh.msg_namelen = 0;
  smh.msg_iov = siov;
  /* smh.msg_iovlen */
  smh.msg_control = NULL;
  smh.msg_controllen = 0;
  smh.msg_flags = 0;

#ifndef DGRAM_MODE
  csock = -1;
#endif

  while(1) {
#ifdef DGRAM_MODE
    i = recvmsg(ssock,&rmh,0);
#else
    if(csock >= 0) close(csock);
    rsun.sun_family = AF_UNIX;
    rsun_len = sizeof(rsun);
    csock = accept(ssock,(struct sockaddr*)&rsun,&rsun_len);
    if(csock == -1) {
      syslog(LOG_ERR,"accept: %s\n",strerror(errno));
      break;
    }
    if(debug >= 2) {
      i = sizeof(cred);
      if(getsockopt(csock,SOL_SOCKET,SO_PEERCRED,&cred,&i) == -1) {
	syslog(LOG_INFO,"getsockopt(SO_PEERCRED) failed: %s",strerror(errno));
      } else {
	syslog(LOG_INFO,"connection from pid=%i uid=%i gid=%i\n",
	       cred.pid,cred.uid,cred.gid);
      }
    }
    i = recvmsg(csock,&rmh,0);
#endif

    if(i == -1) {
      syslog(LOG_ERR,"recvmsg: %s\n",strerror(errno));
      break;
    }
    if(debug >= 2) 
      syslog(LOG_INFO,"recvd datagram [size=%i] from %s [size=%i]\n",
	     i,rsun.sun_path,rmh.msg_namelen);
    
    /* check that datagram is well formed */
    if(i < 1) {
      syslog(LOG_WARNING,"recv: recvd datagram that is too small\n");
      continue;
    }
    i--;
#ifdef DGRAM_MODE
    smh.msg_name = rmh.msg_name;
    smh.msg_namelen = rmh.msg_namelen;
#endif
    
    if(rcmd == 'u') {
      if(!i) {
	syslog(LOG_WARNING,"recv: recvd 'u' datagram with no payload\n");
	continue;
      }
      rbuf[i - (WP_KEY_LEN * sizeof(unsigned int))] = 0;
      uid = id_table_create_id(&table,rbuf,kbuf);
      if(debug >= 1) syslog(LOG_INFO,"request uid(%s) = %i\n",rbuf,uid);
      if(uid == -1) {
	char sbuf[2 * WP_BUF_SIZE],*sep = strerror(errno);
	sprintf(sbuf,"id_table_create_id(%s,[",rbuf);
	for(i = 0; i < WP_KEY_LEN; i++)
	  sprintf(sbuf + strlen(sbuf), "%u,", kbuf[i]);

	sprintf(sbuf + strlen(sbuf) - 1, "]): %s\n",sep);
	syslog(LOG_ERR,sbuf);
	/* break; hobble along rather than die */
      }
      
      siov[0].iov_base = &uid;
      siov[0].iov_len = sizeof(uid);
      smh.msg_iovlen = 1;
#ifdef DGRAM_MODE
      if(sendmsg(ssock,&smh,0) == -1)
#else
      if(sendmsg(csock,&smh,0) == -1)
#endif
	syslog(LOG_WARNING,"sendmsg: %s\n",strerror(errno));

    } else if(rcmd == 'n') {
      if(i != sizeof(uid)) {
	syslog(LOG_WARNING,"recv: recvd 'n' datagram with invalid payload\n");
	continue;
      }
      
      memcpy(&uid,kbuf,sizeof(uid));
      siov[0].iov_base = id_table_get_name(&table,uid);
      if(debug >= 1)
	syslog(LOG_INFO,"request name(%d) = %s\n",uid,siov[0].iov_base);
      
      if(siov[0].iov_base) {
	siov[0].iov_len = strlen(siov[0].iov_base);
	smh.msg_iovlen = 1;
      } else {
	smh.msg_iovlen = 0;
      } 
#ifdef DGRAM_MODE
      if(sendmsg(ssock,&smh,0) == -1) 
#else
      if(sendmsg(csock,&smh,0) == -1)
#endif
	syslog(LOG_WARNING,"sendmsg: %s\n",strerror(errno));
      
    } else if(rcmd == 'c') {
      if(debug >= 1) syslog(LOG_INFO,"request clear()");
      if(id_table_remove_stale(&table)) {
	syslog(LOG_WARNING,"id_table_remove_stale: %s\n",strerror(errno));
	break;
      }	
    }
#ifdef HONOR_QUIT
    else if(rcmd == 'q') {
      if(debug >= 1) syslog(LOG_INFO,"quit requested");
      break;
    }
#endif
    else {
      syslog(LOG_WARNING,"invalid request received");
    }
  }
#ifndef DGRAM_MODE
  if(csock >= 0) close(csock);
#endif

  /*
   * clean up (never really get this far)
   */

  id_table_destroy(&table);
  close(ssock);
  exit(0);
  return 0;
}
