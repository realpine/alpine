/*
 * $Id: ablookup.h 406 2007-01-31 00:36:05Z hubert@u.washington.edu $
 *
 * ========================================================================
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

#ifndef PITH_ABLOOKUP_INCLUDED
#define PITH_ABLOOKUP_INCLUDED


#include "../pith/state.h"
#include "../pith/pattern.h"
#include "../pith/adrbklib.h"
#include "../pith/bldaddr.h"


#define ANC_AFTERCOMMA   0x1


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


#endif /* PITH_ABLOOKUP_INCLUDED */
