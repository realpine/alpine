/*
 * $Id: ldap.h 1012 2008-03-26 00:44:22Z hubert@u.washington.edu $
 *
 * ========================================================================
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

#ifndef PITH_LDAP_INCLUDED
#define PITH_LDAP_INCLUDED


#include "../pith/state.h"
#include "../pith/adrbklib.h"


#ifdef	ENABLE_LDAP

/*
 * This is used to consolidate related information about a server. This
 * information is all stored in the ldap-servers variable, per server.
 */
typedef struct ldap_serv {
    char	*serv,		/* Server name			*/
		*base,		/* Search base			*/
		*binddn,	/* Bind DN if non-anonymous	*/
		*cust,		/* Custom search filter		*/
		*nick,		/* Nickname			*/
		*mail,		/* Backup email address		*/
		*mailattr,	/* "Mail" attribute name	*/
		*snattr,	/* "Surname" attribute name	*/
		*gnattr,	/* "Givenname" attribute name	*/
		*cnattr;	/* "CommonName" attribute name	*/
    int		 port,		/* Port number			*/
		 time,		/* Time limit			*/
		 size,		/* Size limit			*/
		 impl,		/* Use implicitly feature	*/
		 rhs,		/* Lookup contents feature	*/
		 ref,		/* Save by reference feature	*/
		 nosub,		/* Disable space sub feature	*/
		 tls,		/* Attempt TLS			*/
		 tlsmust,	/* Require TLS			*/
		 type,		/* Search type (surname...)	*/
		 srch,		/* Search rule (contains...)	*/
		 scope;		/* Scope of search (base...)	*/
} LDAP_SERV_S;


/*
 * Structures to control the LDAP address selection screen
 *
 * We may run into the problem of LDAP databases containing non-UTF-8 data
 * because they are old. They should have all UTF-8 data and that is what
 * we are assuming. If we wanted to accomodate these servers we could
 * translate the data when we use it. LDAP data is only used in a few
 * places so it might not be too hard to fix it. There are four calls
 * into the LDAP library that produce character strings which are
 * supposed to be UTF-8. They are
 *                                 ldap_get_dn
 *                                 ldap_first_attribute
 *                                 ldap_next_attribute
 *                                 ldap_get_values
 * We call those from a half dozen functions. We could fix it by
 * having a directory-character-set per server and passing that around
 * in the LDAP_SERV_RES_S structure, I think. For now, let's go with
 * the assumption that everything is already UTF-8.
 */
typedef struct ldap_serv_results {
    LDAP                      *ld;		/* LDAP handle */
    LDAPMessage               *res;		/* LDAP search result */
    LDAP_SERV_S               *info_used;
    char                      *serv;
    struct ldap_serv_results  *next;
} LDAP_SERV_RES_S;


typedef struct addr_choose {
    LDAP_SERV_RES_S *res_head;
    char            *title;
    LDAP            *selected_ld;	/* from which ld was entry selected */
    LDAPMessage     *selected_entry;	/* which entry was selected */
    LDAP_SERV_S     *info_used;
    char            *selected_serv;
} ADDR_CHOOSE_S;


/*
 * This is very similar to LDAP_SERV_RES_S, but selected_entry
 * is a single entry instead of a result list.
 */
typedef struct ldap_choose_results {
    LDAP                      *ld;		/* LDAP handle */
    LDAPMessage               *selected_entry;
    LDAP_SERV_S               *info_used;
    char                      *serv;
} LDAP_CHOOSE_S;


/*
 * How the LDAP lookup should work.
 */
typedef	enum {AlwaysDisplay,
	      AlwaysDisplayAndMailRequired,
    	      DisplayIfTwo,
	      DisplayIfOne,
	      DisplayForURL
	      } LDAPLookupStyle;


#define	LDAP_TYPE_CN		0
#define	LDAP_TYPE_SUR		1
#define	LDAP_TYPE_GIVEN		2
#define	LDAP_TYPE_EMAIL		3
#define	LDAP_TYPE_CN_EMAIL	4
#define	LDAP_TYPE_SUR_GIVEN	5
#define	LDAP_TYPE_SEVERAL	6

#define	LDAP_SRCH_CONTAINS	0
#define	LDAP_SRCH_EQUALS	1
#define	LDAP_SRCH_BEGINS	2
#define	LDAP_SRCH_ENDS		3

#define	DEF_LDAP_TYPE		6
#define	DEF_LDAP_SRCH		2
#define	DEF_LDAP_TIME		30
#define	DEF_LDAP_SIZE		0
#define	DEF_LDAP_SCOPE		LDAP_SCOPE_SUBTREE
#define	DEF_LDAP_MAILATTR	"mail"
#define	DEF_LDAP_SNATTR		"sn"
#define	DEF_LDAP_GNATTR		"givenname"
#define	DEF_LDAP_CNATTR		"cn"

#endif	/* ENABLE_LDAP */


/*
 * Error handling argument for white pages lookups.
 */
typedef struct wp_err {
    char	*error;
    int		 wp_err_occurred;
    int		*mangled;
    int		 ldap_errno;
} WP_ERR_S;


extern int wp_exit;
extern int wp_nobail;


/* exported protoypes */
ADDRESS       *wp_lookups(char *, WP_ERR_S *, int);
#ifdef	ENABLE_LDAP
int            ldap_lookup_all(char *, int, int, LDAPLookupStyle, CUSTOM_FILT_S *,
			       LDAP_CHOOSE_S **, WP_ERR_S *, LDAP_SERV_RES_S **);
char	      *ldap_translate(char *, LDAP_SERV_S *);
ADDRESS       *address_from_ldap(LDAP_CHOOSE_S *);
LDAP_SERV_S   *break_up_ldap_server(char *);
void           free_ldap_server_info(LDAP_SERV_S **);
void           free_ldap_result_list(LDAP_SERV_RES_S **);
void           our_ldap_memfree(void *);
void           our_ldap_dn_memfree(void *);
int            our_ldap_set_option(LDAP *, int, void *);
int            ldap_v3_is_supported(LDAP *);
int            ask_user_which_entry(LDAP_SERV_RES_S *, char *,
				    LDAP_CHOOSE_S **, WP_ERR_S *, LDAPLookupStyle);
LDAP_SERV_RES_S *ldap_lookup_all_work(char *, int, int, CUSTOM_FILT_S *, WP_ERR_S *);


/*
 * This must be defined in the application
 */
int         ldap_addr_select(struct pine *, ADDR_CHOOSE_S *, LDAP_CHOOSE_S **,
			     LDAPLookupStyle, WP_ERR_S *, char *);
#endif	/* ENABLE_LDAP */


#endif /* PITH_LDAP_INCLUDED */
