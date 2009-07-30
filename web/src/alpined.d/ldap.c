#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: ldap.c 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $";
#endif

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
#include <general.h>

#include "../../../c-client/c-client.h"

#include "../../../pith/state.h"
#include "../../../pith/debug.h"
#include "../../../pith/adrbklib.h"
#include "../../../pith/ldap.h"

#include "ldap.h"


#ifdef ENABLE_LDAP

int
ldap_addr_select(ps, ac, result, style, wp_err, srchstr)
    struct pine           *ps;
    ADDR_CHOOSE_S         *ac;
    LDAP_CHOOSE_S        **result;
    LDAPLookupStyle        style;
    WP_ERR_S              *wp_err;
    char                  *srchstr;
{
    LDAP_SERV_RES_S *res_list, *tmp_rl;
    LDAPMessage     *e, *tmp_e;
    char           **mail = NULL, *a;
    int              got_n_entries = 0, retval = -5;
    BerElement      *ber;
    
    dprint((7, "ldap_addr_select, srchstr: %s", srchstr));
    for(res_list = ac->res_head; res_list; res_list = res_list->next){
        tmp_rl = res_list;
        for(e = ldap_first_entry(res_list->ld, res_list->res);
	    e != NULL;
	    e = ldap_next_entry(res_list->ld, e)){
	    tmp_e = e;
	    got_n_entries++;
	}
    }
    if(got_n_entries == 1){
	for(a = ldap_first_attribute(tmp_rl->ld, tmp_e, &ber);
	    a != NULL;
	    a = ldap_next_attribute(tmp_rl->ld, tmp_e, ber)){
	    if(strcmp(a, tmp_rl->info_used->mailattr) == 0){
		mail = ldap_get_values(tmp_rl->ld, tmp_e, a);
		break;
	    }
	}
	if(mail && mail[0] && mail[0][0]){
	    retval = 0;
	    if(result){
		(*result) =
		  (LDAP_CHOOSE_S *)fs_get(sizeof(LDAP_CHOOSE_S));
		(*result)->ld    = tmp_rl->ld;
		(*result)->selected_entry   = tmp_e;
		(*result)->info_used = tmp_rl->info_used;
		(*result)->serv  = tmp_rl->serv;
	    }
	}
	else{
	    retval = -2;
	}
    }
    else
      retval = -3;

    return(retval);
}


char *
peLdapPname(mailbox, host)
     char *mailbox;
     char *host;
{
    char *retstr = NULL;
    char              adrstr[1024], **cn;
    int               ecnt;
    CUSTOM_FILT_S    *filter;
    WP_ERR_S          wp_err;
    LDAP_CHOOSE_S    *winning_e = NULL;
    LDAP_SERV_RES_S  *results = NULL;
    LDAP_SERV_RES_S  *trl;
    LDAPMessage      *e;

    sprintf(adrstr, "(mail=%.500s@%.500s)", mailbox, host);
    filter = (CUSTOM_FILT_S *)fs_get(sizeof(CUSTOM_FILT_S));
    filter->filt = cpystr(adrstr);
    filter->combine = 0;
    memset(&wp_err, 0, sizeof(wp_err));
    wpldap_global->query_no++;
    if(wpldap_global->ldap_search_list){
        wpldap_global->ldap_search_list =
	  free_wpldapres(wpldap_global->ldap_search_list);
    }
    ldap_lookup_all("", 0, 0, AlwaysDisplay, filter, &winning_e,
		    &wp_err, &results);
    if(filter){
        fs_give((void **)&filter->filt);
	fs_give((void **)&filter);
    }
    if(wpldap_global->ldap_search_list){
        trl = wpldap_global->ldap_search_list->reslist;
	for(ecnt = 0, e = ldap_first_entry(trl->ld, trl->res);
	    e != NULL; e = ldap_next_entry(trl->ld, e), ecnt++);
	if(ecnt == 1) { /* found the one true name */
	    e = ldap_first_entry(trl->ld, trl->res);
	    peLdapEntryParse(trl, e, &cn, NULL, NULL, NULL,
			     NULL, NULL);
	    if(cn){
	      retstr = cpystr(cn[0]);
	      ldap_value_free(cn);
	    }
	}
    }
    return(retstr);
}

int
peLdapEntryParse(trl, e, ret_cn, ret_org, ret_unit, 
		 ret_title, ret_mail, ret_sn)
     LDAP_SERV_RES_S *trl;
     LDAPMessage     *e;
     char          ***ret_cn;
     char          ***ret_org;
     char          ***ret_unit;
     char          ***ret_title;
     char          ***ret_mail;
     char          ***ret_sn;
{
    char       *a, **cn, **org, **unit, **title, **mail, **sn;
    BerElement *ber;
    
    cn = org = title = unit = mail = sn = NULL;
    
    for(a = ldap_first_attribute(trl->ld, e, &ber);
	a != NULL;
	a = ldap_next_attribute(trl->ld, e, ber)){
        dprint((9, " %s", a));
	if(strcmp(a, trl->info_used->cnattr) == 0){
	    if(!cn)
	      cn = ldap_get_values(trl->ld, e, a);
	    
	    if(cn && !(cn[0] && cn[0][0])){
	        ldap_value_free(cn);
		cn = NULL;
	    }
	}
	else if(strcmp(a, trl->info_used->mailattr) == 0){
	    if(!mail)
	      mail = ldap_get_values(trl->ld, e, a);
	}
	else if(strcmp(a, "o") == 0){
	    if(!org)
	      org = ldap_get_values(trl->ld, e, a);
	}
	else if(strcmp(a, "ou") == 0){
	    if(!unit)
	      unit = ldap_get_values(trl->ld, e, a);
	}
	else if(strcmp(a, "title") == 0){
	    if(!title)
	      title = ldap_get_values(trl->ld, e, a);
	}
	
	our_ldap_memfree(a);
    }

    if(!cn){
        for(a = ldap_first_attribute(trl->ld, e, &ber);
	    a != NULL;
	    a = ldap_next_attribute(trl->ld, e, ber)){
	  
	    if(strcmp(a,  trl->info_used->snattr) == 0){
	        if(!sn)
		  sn = ldap_get_values(trl->ld, e, a);
		
		if(sn && !(sn[0] && sn[0][0])){
		  ldap_value_free(sn);
		  sn = NULL;
		}
	    }
	    our_ldap_memfree(a);
	}
    }
    if(ret_cn)
      (*ret_cn)    = cn;
    else if(cn) ldap_value_free(cn);
    if(ret_org)
      (*ret_org)   = org;
    else if(org) ldap_value_free(org);
    if(ret_unit)
      (*ret_unit)  = unit;
    else if(unit) ldap_value_free(unit);
    if(ret_title)
      (*ret_title) = title;
    else if(title) ldap_value_free(title);
    if(ret_mail)
      (*ret_mail)  = mail;
    else if(mail) ldap_value_free(mail);
    if(ret_sn)
      (*ret_sn)    = sn;
    else if(sn) ldap_value_free(sn);

    return 0;
}

WPLDAPRES_S *
free_wpldapres(wpldapr)
     WPLDAPRES_S *wpldapr;
{
    WPLDAPRES_S *tmp1, *tmp2;

    for(tmp1 = wpldapr; tmp1; tmp1 = tmp2){
        tmp2 = tmp1->next;
	if(tmp1->str)
	  fs_give((void **)&tmp1->str);
	if(tmp1->reslist)
	  free_ldap_result_list(&tmp1->reslist);
    }
    fs_give((void **)&wpldapr);
    return(NULL);
}
#endif /* ENABLE_LDAP */
