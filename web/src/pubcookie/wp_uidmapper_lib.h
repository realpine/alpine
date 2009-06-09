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

#ifndef _WP_UIDMAPPER_LIB_H_
#define _WP_UIDMAPPER_LIB_H_

int wp_uidmapper_getuid(char *name,unsigned int *key,int *puid);
int wp_uidmapper_getname(int uid,char *name,int namelen);
int wp_uidmapper_clear();
int wp_uidmapper_quit();
int wp_sessid2key(char *ids,unsigned int *ida);

#define	WP_BUF_SIZE	1024
#define	WP_KEY_LEN	6

#endif
