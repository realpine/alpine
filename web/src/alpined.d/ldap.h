/*-----------------------------------------------------------------------
 $Id: ldap.h 5 2006-01-04 17:53:54Z hubert $
  -----------------------------------------------------------------------*/

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

#ifndef _WEB_ALPINE_LDAP_INCLUDED
#define _WEB_ALPINE_LDAP_INCLUDED


#ifdef ENABLE_LDAP

#include "../../../pith/ldap.h"

typedef struct wpldapres {
  char             *str;
  LDAP_SERV_RES_S  *reslist;
  struct wpldapres *next;
} WPLDAPRES_S;

typedef struct wpldap {
  int          query_no;
  WPLDAPRES_S *ldap_search_list;
} WPLDAP_S;


extern WPLDAP_S *wpldap_global;


char	    *peLdapPname(char *, char *);
int          peLdapEntryParse(LDAP_SERV_RES_S *, LDAPMessage *,
			      char ***, char ***, char ***, char ***,
			      char ***, char ***);
WPLDAPRES_S *free_wpldapres(WPLDAPRES_S *);

#endif  /* ENABLE_LDAP */

#endif /* _WEB_ALPINE_LDAP_INCLUDED */
