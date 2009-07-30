/*
 * $Id: abdlc.h 769 2007-10-24 00:15:40Z hubert@u.washington.edu $
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

#ifndef PITH_ABDLC_INCLUDED
#define PITH_ABDLC_INCLUDED


#include "../pith/adrbklib.h"


/* exported protoypes */
DL_CACHE_S    *get_dlc(long);
void           warp_to_dlc(DL_CACHE_S *, long);
void           warp_to_beginning(void);
void           warp_to_top_of_abook(int);
void           warp_to_end(void);
void           flush_dlc_from_cache(DL_CACHE_S *);
int            matching_dlcs(DL_CACHE_S *, DL_CACHE_S *);
void           fill_in_dl_field(DL_CACHE_S *);
char          *listmem(long);
char          *listmem_from_dl(AdrBk *, AddrScrn_Disp *);
adrbk_cntr_t   listmem_count_from_abe(AdrBk_Entry *);
AddrScrn_Disp *dlist(long);
int            adrbk_num_from_lineno(long);
void           done_with_dlc_cache(void);


#endif /* PITH_ABDLC_INCLUDED */
