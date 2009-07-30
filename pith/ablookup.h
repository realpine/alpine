/*
 * $Id: ablookup.h 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $
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


/*
 * Flags to adrbk_list_of_completions().
 * ALC_INCLUDE_ADDRS means that the "addr" element
 * should be included in the COMPLETE_S list. When
 * this is not set the entries that match because
 * of an addr match are still included in the list
 * but the actual "addr" element itself is not filled
 * in. That isn't needed by Web Alpine and it does
 * have a cost, though rather small, to fill it in.
 */
#define ALC_INCLUDE_ADDRS   0x1
#define ALC_INCLUDE_LDAP    0x2

/*
 * Values OR'd together in matches_bitmaps.
 * These are not in the same namespace as the flags
 * above, these are just bits for the matches_bitmaps member.
 *
 * If ALC_NICK is set that means there was a prefix match on the nickname.
 * ALC_FULL is a match on the full address.
 * ALC_ADDR is a match on the mailbox@host part of the address.
 * ALC_REVFULL is a match on the Last, First Fullname if the user uses that.
 * ALC_ABOOK means the entry came from an address book entry
 * ALC_LDAP means the entry came from an LDAP lookup
 * ALC_CURR means the entry came from the currently viewed message
 * This is used in both ABOOK_ENTRY_S and COMPLETE_S structures.
 */
#define ALC_NICK    0x01
#define ALC_FULL    0x02
#define ALC_REVFULL 0x04
#define ALC_ADDR    0x08
#define ALC_ABOOK   0x10
#define ALC_LDAP    0x20
#define ALC_CURR    0x40
#define ALC_FCC     0x80


typedef struct abook_entry_list {
    AdrBk                   *ab;
    adrbk_cntr_t             entrynum;
    unsigned                 matches_bitmap;
    struct abook_entry_list *next;
} ABOOK_ENTRY_S;


typedef struct completelist {
    char                *nickname;	/* the nickname */
    char                *full_address;	/* Some Body <someb@there.org> */
    char                *addr;		/* someb@there.org (costs extra) */
    char                *rev_fullname;	/* optional Last, First version of fullname */
    char		*fcc;		/* optional fcc associated with address */
    unsigned             matches_bitmap;
    struct completelist *next;
} COMPLETE_S;


/* exported protoypes */
char          *get_nickname_from_addr(ADDRESS *, char *, size_t);
char          *get_fcc_from_addr(ADDRESS *, char *, size_t);
int            get_contactinfo_from_addr(ADDRESS *, char **, char **, char **, char **);
void           address_in_abook(MAILSTREAM *, SEARCHSET *, int, PATTERN_S *);
ADDRESS       *abe_to_address(AdrBk_Entry *, AddrScrn_Disp *, AdrBk *, int *);
char          *abe_to_nick_or_addr_string(AdrBk_Entry *, AddrScrn_Disp *, int);
int            address_is_us(ADDRESS *, struct pine *);
int            address_is_same(ADDRESS *, ADDRESS *);
int            abes_are_equal(AdrBk_Entry *, AdrBk_Entry *);
char          *addr_lookup(char *, int *, int);
AdrBk_Entry   *adrbk_lookup_with_opens_by_nick(char *, int, int *, int);
AdrBk_Entry   *address_to_abe(ADDRESS *);
int            contains_regex_special_chars(char *str);
COMPLETE_S    *adrbk_list_of_completions(char *, MAILSTREAM *, imapuid_t, int);
COMPLETE_S    *new_complete_s(char *, char *, char *, char *, char *, unsigned);
void           free_complete_s(COMPLETE_S **);
ABOOK_ENTRY_S *new_abook_entry_s(AdrBk *, a_c_arg_t, unsigned);
void           free_abook_entry_s(ABOOK_ENTRY_S **);
void           combine_abook_entry_lists(ABOOK_ENTRY_S **, ABOOK_ENTRY_S *);
ABOOK_ENTRY_S *adrbk_list_of_possible_completions(AdrBk *, char *);


#endif /* PITH_ABLOOKUP_INCLUDED */
