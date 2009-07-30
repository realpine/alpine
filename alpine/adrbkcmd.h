/*
 * $Id: adrbkcmd.h 1012 2008-03-26 00:44:22Z hubert@u.washington.edu $
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

#ifndef PITH_ADRBKCMD_INCLUDED
#define PITH_ADRBKCMD_INCLUDED


#include "../pith/adrbklib.h"
#include "../pith/state.h"
#include "../pith/ldap.h"
#include "../pith/handle.h"
#include "../pith/store.h"


#define AB_COMMENT_STR			_("Comment")


/* exported protoypes */
void       view_abook_entry(struct pine *, long);
void       edit_entry(AdrBk *, AdrBk_Entry *, a_c_arg_t, Tag, int, int *, char *);
int        ab_add_abook(int, int);
int        ab_edit_abook(int, int, char *, char *, char *);
int        any_addrbooks_to_convert(struct pine *);
int        convert_addrbooks_to_remote(struct pine *, char *, size_t);
int        any_sigs_to_convert(struct pine *);
int        convert_sigs_to_literal(struct pine *, int);
void       warn_about_rule_files(struct pine *);
void       convert_to_remote_config(struct pine *, int);
int        ab_del_abook(long, int, char **);
int        ab_shuffle(PerAddrBook *, int *, int, char **);
int        ab_compose_to_addr(long, int, int);
int        ab_forward(struct pine *, long, int);
int        ab_save(struct pine *, AdrBk *, long, int, int);
int        ab_print(int);
int        ab_agg_delete(struct pine *, int);
int        single_entry_delete(AdrBk *, long, int *);
char      *query_server(struct pine *, int, int *, int, char **);
void	   free_headents(struct headerentry **);
void       write_single_vcard_entry(struct pine *, gf_io_t, VCARD_INFO_S *);
void       free_vcard_info(VCARD_INFO_S **);
#ifdef	ENABLE_LDAP
void       view_ldap_entry(struct pine *, LDAP_CHOOSE_S *);
void       compose_to_ldap_entry(struct pine *, LDAP_CHOOSE_S *,int);
void       forward_ldap_entry(struct pine *, LDAP_CHOOSE_S *);
STORE_S   *prep_ldap_for_viewing(struct pine *, LDAP_CHOOSE_S *, SourceType, HANDLE_S **);
void       free_saved_query_parameters(void);
int        url_local_ldap(char *);
#endif


#endif /* PITH_ADRBKCMD_INCLUDED */
