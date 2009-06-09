/*
 * $Id: ablookup.h 947 2008-03-05 23:25:51Z hubert@u.washington.edu $
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

#ifndef PITH_ABLOOKUP_INCLUDED
#define PITH_ABLOOKUP_INCLUDED


#include "../pith/state.h"
#include "../pith/pattern.h"
#include "../pith/adrbklib.h"
#include "../pith/bldaddr.h"


#define ANC_AFTERCOMMA   0x1


typedef struct abook_entry_list {
    AdrBk                   *ab;
    adrbk_cntr_t             entrynum;
    struct abook_entry_list *next;
} ABOOK_ENTRY_S;


typedef struct completelist {
    char                *nickname;
    char                *full_address;
    struct completelist *next;
} COMPLETE_S;


/* exported protoypes */
char          *get_nickname_from_addr(ADDRESS *, char *, size_t);
char          *get_fcc_from_addr(ADDRESS *, char *, size_t);
void           address_in_abook(MAILSTREAM *, SEARCHSET *, int, PATTERN_S *);
ADDRESS       *abe_to_address(AdrBk_Entry *, AddrScrn_Disp *, AdrBk *, int *);
char          *abe_to_nick_or_addr_string(AdrBk_Entry *, AddrScrn_Disp *, int);
int            address_is_us(ADDRESS *, struct pine *);
int            address_is_same(ADDRESS *, ADDRESS *);
int            abes_are_equal(AdrBk_Entry *, AdrBk_Entry *);
char          *addr_lookup(char *, int *, int);
int            adrbk_nick_complete(char *, char **, unsigned);
AdrBk_Entry   *adrbk_lookup_with_opens_by_nick(char *, int, int *, int);
AdrBk_Entry   *address_to_abe(ADDRESS *);
int            contains_regex_special_chars(char *str);
COMPLETE_S    *adrbk_list_of_completions(char *);
COMPLETE_S    *new_complete_s(char *, char *);
void           free_complete_s(COMPLETE_S **);
ABOOK_ENTRY_S *new_abook_entry_s(AdrBk *, a_c_arg_t);
void           free_abook_entry_s(ABOOK_ENTRY_S **);
void           combine_abook_entry_lists(ABOOK_ENTRY_S **, ABOOK_ENTRY_S *);
ABOOK_ENTRY_S *adrbk_list_of_possible_completions(AdrBk *, char *);


#endif /* PITH_ABLOOKUP_INCLUDED */
