#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: alpineldap.c 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $";
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

#include <system.h>
#include <general.h>

#include "../../../c-client/c-client.h"
#include "../../../c-client/imap4r1.h"

#include "../../../pith/osdep/color.h"	/* color support library */
#include "../../../pith/osdep/canaccess.h"
#include "../../../pith/osdep/temp_nam.h"

#include "../../../pith/stream.h"
#include "../../../pith/context.h"
#include "../../../pith/state.h"
#include "../../../pith/msgno.h"
#include "../../../pith/debug.h"
#include "../../../pith/init.h"
#include "../../../pith/conf.h"
#include "../../../pith/conftype.h"
#include "../../../pith/detoken.h"
#include "../../../pith/flag.h"
#include "../../../pith/help.h"
#include "../../../pith/remote.h"
#include "../../../pith/status.h"
#include "../../../pith/mailcmd.h"
#include "../../../pith/savetype.h"
#include "../../../pith/save.h"
#include "../../../pith/reply.h"
#include "../../../pith/sort.h"
#include "../../../pith/ldap.h"
#include "../../../pith/addrbook.h"
#include "../../../pith/takeaddr.h"
#include "../../../pith/bldaddr.h"
#include "../../../pith/copyaddr.h"
#include "../../../pith/thread.h"
#include "../../../pith/folder.h"
#include "../../../pith/mailview.h"
#include "../../../pith/indxtype.h"
#include "../../../pith/mailindx.h"
#include "../../../pith/mailpart.h"
#include "../../../pith/mimedesc.h"
#include "../../../pith/detach.h"
#include "../../../pith/newmail.h"
#include "../../../pith/charset.h"
#include "../../../pith/util.h"
#include "../../../pith/rfc2231.h"
#include "../../../pith/string.h"
#include "../../../pith/send.h"

#include "alpined.h"
#include "ldap.h"

struct pine *ps_global;                         /* THE global variable! */
char         tmp_20k_buf[20480];

char	  *peSocketName;

#ifdef ENABLE_LDAP
WPLDAP_S *wpldap_global;
#endif

int	peNoPassword, peCredentialError;
int	peCertQuery, peCertFailure;
char	peCredentialRequestor[CRED_REQ_SIZE];
STRLIST_S *peCertHosts;

void
sml_addmsg(priority, text)
     int   priority;
     char *text;
{
}

void
peDestroyUserContext(pps)
     struct pine **pps;
{
}

int
main(argc, argv)
     int   argc;
     char *argv[];
{
#ifdef ENABLE_LDAP
    struct pine *pine_state;
    char *p = NULL, *userid = NULL, *domain = NULL, *pname;
    struct variable *vars;
    int i, usage = 0, rv = 0;

    pine_state = new_pine_struct();
    ps_global  = pine_state;
    vars = ps_global->vars;
    debug = 0;

    for(i = 1 ; i < argc; i++){
        if(*argv[i] == '-'){
	    switch (argv[i++][1]) {
	        case 'p':
		  p = argv[i];
		  break;
	        case 'u':
		  userid = argv[i];
		  break;
	        case 'd':
		  domain = argv[i];
		  break;
	        default:
		  usage = rv = 1;
		  break;
	    }
	}
	else
	  usage = rv = 1;
	if(usage == 1) break;
    }
    if(argc == 1) usage = rv = 1;
    if (usage == 1 || !p || !userid){
      usage = rv = 1;
      goto done;
    }
    wpldap_global = (WPLDAP_S *)fs_get(sizeof(WPLDAP_S));
    wpldap_global->query_no = 0;
    wpldap_global->ldap_search_list = NULL;

    ps_global->pconf = new_pinerc_s(p);
    if(ps_global->pconf)
      read_pinerc(ps_global->pconf, vars, ParseGlobal);
    else {
        fprintf(stderr, "Failed to read pineconf\n");
	rv = 1;
	goto done;
    }
    set_current_val(&ps_global->vars[V_LDAP_SERVERS], FALSE, FALSE);
    set_current_val(&ps_global->vars[V_USER_DOMAIN], FALSE, FALSE);
    if(!ps_global->VAR_USER_DOMAIN && !domain){
      fprintf(stderr, "No domain set in pineconf\n");
      usage = 1;
      goto done;
    }
    if((pname = peLdapPname(userid, domain ? domain : ps_global->VAR_USER_DOMAIN)) != NULL){
      fprintf(stdout, "%s\n", pname);
      fs_give((void **)&pname);
    }
    else 
      fprintf(stdout, "\n");

done:
    if(usage)
      fprintf(stderr, "usage: pineldap -u userid -p pineconf [-d domain]\n");
    if(wpldap_global){
        if(wpldap_global->ldap_search_list)
	  free_wpldapres(wpldap_global->ldap_search_list);
	fs_give((void **)&wpldap_global);
    }
    if(ps_global->pconf)
      free_pinerc_s(&ps_global->pconf);
    free_pine_struct(&pine_state);

    exit(rv);
#else
    fprintf(stderr, "%s: Not built with LDAP support\n", argv[0]);
    exit(-1);
#endif
}

