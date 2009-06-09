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


int main(int argc,char *argv[]) {
  char name[WP_BUF_SIZE],sessid[WP_BUF_SIZE];
  int uid,i,key[WP_KEY_LEN];
  
  if(argc >= 2) {
    if(*argv[1] == 'u') {
      if(argc >= 3) {
	  memset(key,0,WP_KEY_LEN * sizeof(int));
	  if(argc >= 4){
	      if(wp_parse_cookie(argv[3],"sessid",";@",sessid,WP_BUF_SIZE))
		(void)wp_sessid2key(sessid,key);
	  }

	  if(wp_uidmapper_getuid((argv[2][0] == '\0') ? NULL : argv[2],key,&uid) != -1) {
	      printf("uid = %i\n",uid);
	      return 0;
	  } else fprintf(stderr,"wp_uidmapper_getuid error: %s\n",
			 strerror(errno));
      }
    } else if(*argv[1] == 'n') {
      if(argc >= 3) {
	i = wp_uidmapper_getname(strtol(argv[2],NULL,0),name,WP_BUF_SIZE);
	if(i != -1) {
	  printf("name = %s\n",name);
	  return 0;
	} else fprintf(stderr,"wp_uidmapper_getname error: %s\n",
		       strerror(errno));
      } 
    } else if(*argv[1] == 'c') {
      if(wp_uidmapper_clear() != -1) {
	printf("clear command sent\n");
	return 0;
      } else fprintf(stderr,"wp_uidmapper_clear error: %s\n",
		     strerror(errno));
    } else if(*argv[1] == 'q') {
      if(wp_uidmapper_quit() != -1) {
	printf("quit command sent\n");
	return 0;
      } else fprintf(stderr,"wp_uidmapper_quit error: %s\n",
		     strerror(errno));
    } 
  }
  fprintf(stderr,"Usage: wp_umc [u name [key] ] [n uid] [c] [q]\n");
  return -1;
}
