#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: abdlc.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
#endif

/*
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

#include "../pith/headers.h"	/* for os-dep and pith defs/includes */
#include "../pith/abdlc.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/status.h"
#include "../pith/ldap.h"
#include "../pith/tempfile.h"


/*
 * Internal prototypes
 */
void           initialize_dlc_cache(void);
int            dlc_siblings(DL_CACHE_S *, DL_CACHE_S *);
DL_CACHE_S    *dlc_mgr(long, DlMgrOps, DL_CACHE_S *);
void           free_cache_array(DL_CACHE_S **, int);
DL_CACHE_S    *dlc_prev(DL_CACHE_S *, DL_CACHE_S *);
DL_CACHE_S    *dlc_next(DL_CACHE_S *, DL_CACHE_S *);
DL_CACHE_S    *get_global_top_dlc(DL_CACHE_S *);
DL_CACHE_S    *get_global_bottom_dlc(DL_CACHE_S *);
DL_CACHE_S    *get_top_dl_of_adrbk(int, DL_CACHE_S *);
DL_CACHE_S    *get_bottom_dl_of_adrbk(int, DL_CACHE_S *);


#define NO_PERMISSION	_("[ Permission Denied ]")
/* TRANSLATORS: This is a heading referring to something that is readable
   but not writeable. */
#define READONLY	_(" (ReadOnly)")
/* TRANSLATORS: Not readable */
#define NOACCESS	_(" (Un-readable)")
/* TRANSLATORS: Directories, as in LDAP Directories. A heading. */
#define OLDSTYLE_DIR_TITLE	_("Directories")


/* data for the display list cache */
static DL_CACHE_S *cache_array = (DL_CACHE_S *)NULL;
static long        valid_low,
		   valid_high;
static int         index_of_low,
		   size_of_cache,
		   n_cached;


void
initialize_dlc_cache(void)
{
    dprint((11, "- initialize_dlc_cache -\n"));

    (void)dlc_mgr(NO_LINE, Initialize, (DL_CACHE_S *)NULL);
}


void
done_with_dlc_cache(void)
{
    dprint((11, "- done_with_dlc_cache -\n"));

    (void)dlc_mgr(NO_LINE, DoneWithCache, (DL_CACHE_S *)NULL);
}


/*
 * Returns 1 if the dlc's are related to each other, 0 otherwise.
 *
 * The idea is that if you are going to flush one of these dlcs from the
 * cache you should also flush its partners. For example, if you flush one
 * Listent from a list you should flush the entire entry including all the
 * Listents and the ListHead. If you flush a DlcTitle, you should also
 * flush the SubTitle and the TitleBlankTop.
 */
int
dlc_siblings(DL_CACHE_S *dlc1, DL_CACHE_S *dlc2)
{
    if(!dlc1 || !dlc2 || dlc1->adrbk_num != dlc2->adrbk_num)
      return 0;

    switch(dlc1->type){

      case DlcSimple:
      case DlcListHead:
      case DlcListBlankTop:
      case DlcListBlankBottom:
      case DlcListEnt:
      case DlcListClickHere:
      case DlcListEmpty:
	switch(dlc2->type){
	  case DlcSimple:
	  case DlcListHead:
	  case DlcListBlankTop:
	  case DlcListBlankBottom:
	  case DlcListEnt:
	  case DlcListClickHere:
	  case DlcListEmpty:
	    return(dlc1->dlcelnum == dlc2->dlcelnum);

	  default:
	    return 0;
	}

	break;

      case DlcDirDelim1:
      case DlcDirDelim2:
	switch(dlc2->type){
	  case DlcDirDelim1:
	  case DlcDirDelim2:
	    return 1;

	  default:
	    return 0;
	}
	break;

      case DlcGlobDelim1:
      case DlcGlobDelim2:
	switch(dlc2->type){
	  case DlcGlobDelim1:
	  case DlcGlobDelim2:
	    return 1;

	  default:
	    return 0;
	}
	break;

      case DlcTitle:
      case DlcTitleNoPerm:
      case DlcSubTitle:
      case DlcTitleBlankTop:
	switch(dlc2->type){
	  case DlcTitle:
	  case DlcTitleNoPerm:
	  case DlcSubTitle:
	  case DlcTitleBlankTop:
	    return 1;

	  default:
	    return 0;
	}
	break;

      case DlcDirAccess:
      case DlcDirSubTitle:
      case DlcDirBlankTop:
	switch(dlc2->type){
	  case DlcDirAccess:
	  case DlcDirSubTitle:
	  case DlcDirBlankTop:
	    return 1;

	  default:
	    return 0;
	}
	break;

      case DlcTitleDashTopCmb:
      case DlcTitleCmb:
      case DlcTitleDashBottomCmb:
      case DlcTitleBlankBottomCmb:
      case DlcClickHereCmb:
      case DlcTitleBlankTopCmb:
	switch(dlc2->type){
	  case DlcTitleDashTopCmb:
	  case DlcTitleCmb:
	  case DlcTitleDashBottomCmb:
	  case DlcTitleBlankBottomCmb:
	  case DlcClickHereCmb:
	  case DlcTitleBlankTopCmb:
	    return 1;

	  default:
	    return 0;
	}
	break;

      default:
	return 0;
    }
    /*NOTREACHED*/
}


/*
 * Manage the display list cache.
 *
 * The cache is a circular array of DL_CACHE_S elements.  It always
 * contains a contiguous set of display lines.
 * The lowest numbered line in the cache is
 * valid_low, and the highest is valid_high.  Everything in between is
 * also valid.  Index_of_low is where to look
 * for the valid_low element in the circular array.
 *
 * We make calls to dlc_prev and dlc_next to get new entries for the cache.
 * We need a starting value before we can do that.
 *
 * This returns a pointer to a dlc for the desired row.  If you want the
 * actual display list line you call dlist(row) instead of dlc_mgr.
 */
DL_CACHE_S *
dlc_mgr(long int row, DlMgrOps op, DL_CACHE_S *dlc_start)
{
    int                new_index, known_index, next_index;
    DL_CACHE_S        *dlc = (DL_CACHE_S *)NULL;
    long               next_row;
    long               prev_row;


    if(op == Lookup){

	if(row >= valid_low && row <= valid_high){  /* already cached */

	    new_index = ((row - valid_low) + index_of_low) % size_of_cache;
	    dlc = &cache_array[new_index];

	}
	else if(row > valid_high){  /* row is past where we've looked */

	    known_index =
	      ((valid_high - valid_low) + index_of_low) % size_of_cache;
	    next_row    = valid_high + 1L;

	    /* we'll usually be looking for row = valid_high + 1 */
	    while(next_row <= row){

		new_index = (known_index + 1) % size_of_cache;

		dlc =
		  dlc_next(&cache_array[known_index], &cache_array[new_index]);

		/*
		 * This means somebody changed the file out from underneath
		 * us.  This would happen if dlc_next needed to ask for an
		 * abe to figure out what the type of the next row is, but
		 * adrbk_get_ae returned a NULL.  I don't think that can
		 * happen, but if it does...
		 */
		if(dlc->type == DlcNotSet){
		    dprint((1, "dlc_next returned DlcNotSet\n"));
		    goto panic_abook_corrupt;
		}

		if(n_cached == size_of_cache){ /* replaced low cache entry */
		    valid_low++;
		    index_of_low = (index_of_low + 1) % size_of_cache;
		}
		else
		  n_cached++;

		valid_high++;

		next_row++;
		known_index = new_index; /* for next time through loop */
	    }
	}
	else if(row < valid_low){  /* row is back up the screen */

	    known_index = index_of_low;
	    prev_row = valid_low - 1L;

	    while(prev_row >= row){

		new_index = (known_index - 1 + size_of_cache) % size_of_cache;
		dlc =
		  dlc_prev(&cache_array[known_index], &cache_array[new_index]);

		if(dlc->type == DlcNotSet){
		    dprint((1, "dlc_prev returned DlcNotSet (1)\n"));
		    goto panic_abook_corrupt;
		}

		if(n_cached == size_of_cache) /* replaced high cache entry */
		  valid_high--;
		else
		  n_cached++;

		valid_low--;
		index_of_low =
			    (index_of_low - 1 + size_of_cache) % size_of_cache;

		prev_row--;
		known_index = new_index;
	    }
	}
    }
    else if(op == Initialize){

	n_cached = 0;

	if(!cache_array || size_of_cache != 3 * MAX(as.l_p_page,1)){
	    if(cache_array)
	      free_cache_array(&cache_array, size_of_cache);

	    size_of_cache = 3 * MAX(as.l_p_page,1);
	    cache_array =
		(DL_CACHE_S *)fs_get(size_of_cache * sizeof(DL_CACHE_S));
	    memset((void *)cache_array, 0, size_of_cache * sizeof(DL_CACHE_S));
	}

	/* this will return NULL below and the caller should ignore that */
    }
    /*
     * Flush all rows for a particular addrbook entry from the cache, but
     * keep the cache alive and anchored in the same place.  The particular
     * entry is the one that dlc_start is one of the rows of.
     */
    else if(op == FlushDlcFromCache){
	long low_entry;

	next_row = dlc_start->global_row - 1;
	for(; next_row >= valid_low; next_row--){
	    next_index = ((next_row - valid_low) + index_of_low) %
		size_of_cache;
	    if(!dlc_siblings(dlc_start, &cache_array[next_index]))
		break;
	}

	low_entry = next_row + 1L;

	/*
	 * If low_entry now points one past a ListBlankBottom, delete that,
	 * too, since it may not make sense anymore.
	 */
	if(low_entry > valid_low){
	    next_index = ((low_entry -1L - valid_low) + index_of_low) %
		size_of_cache;
	    if(cache_array[next_index].type == DlcListBlankBottom)
		low_entry--;
	}

	if(low_entry > valid_low){ /* invalidate everything >= this */
	    n_cached -= (valid_high - (low_entry - 1L));
	    valid_high = low_entry - 1L;
	}
	else{
	    /*
	     * This is the tough case.  That entry was the first thing cached,
	     * so we need to invalidate the whole cache.  However, we also
	     * need to keep at least one thing cached for an anchor, so
	     * we need to get the dlc before this one and it should be a
	     * dlc not related to this same addrbook entry.
	     */
	    known_index = index_of_low;
	    prev_row = valid_low - 1L;

	    for(;;){

		new_index = (known_index - 1 + size_of_cache) % size_of_cache;
		dlc =
		  dlc_prev(&cache_array[known_index], &cache_array[new_index]);

		if(dlc->type == DlcNotSet){
		    dprint((1, "dlc_prev returned DlcNotSet (2)\n"));
		    goto panic_abook_corrupt;
		}

		valid_low--;
		index_of_low =
			    (index_of_low - 1 + size_of_cache) % size_of_cache;

		if(!dlc_siblings(dlc_start, dlc))
		  break;

		known_index = new_index;
	    }

	    n_cached = 1;
	    valid_high = valid_low;
	}
    }
    /*
     * We have to anchor ourselves at a first element.
     * Here's how we start at the top.
     */
    else if(op == FirstEntry){
	initialize_dlc_cache();
	n_cached++;
	dlc = &cache_array[0];
	dlc = get_global_top_dlc(dlc);
	dlc->global_row = row;
	index_of_low = 0;
	valid_low    = row;
	valid_high   = row;
    }
    /* And here's how we start from the bottom. */
    else if(op == LastEntry){
	initialize_dlc_cache();
	n_cached++;
	dlc = &cache_array[0];
	dlc = get_global_bottom_dlc(dlc);
	dlc->global_row = row;
	index_of_low = 0;
	valid_low    = row;
	valid_high   = row;
    }
    /*
     * And here's how we start from an arbitrary position in the middle.
     * We root the cache at display line row, so it helps if row is close
     * to where we're going to be starting so that things are easy to find.
     * The dl that goes with line row is dl_start from addrbook number
     * adrbk_num_start.
     */
    else if(op == ArbitraryStartingPoint){
	AddrScrn_Disp      dl;

	initialize_dlc_cache();
	n_cached++;
	dlc = &cache_array[0];
	/*
	 * Save this in case fill_in_dl_field needs to free the text
	 * it points to.
	 */
	dl = dlc->dl;
	*dlc = *dlc_start;
	dlc->dl = dl;
	dlc->global_row = row;

	index_of_low = 0;
	valid_low    = row;
	valid_high   = row;
    }
    else if(op == DoneWithCache){

	n_cached = 0;
	if(cache_array)
	  free_cache_array(&cache_array, size_of_cache);
    }

    return(dlc);

panic_abook_corrupt:
    q_status_message(SM_ORDER | SM_DING, 5, 10,
	_("Addrbook changed unexpectedly, re-syncing..."));
    dprint((1,
	_("addrbook changed while we had it open?, re-sync\n")));
    dprint((2,
	"valid_low=%ld valid_high=%ld index_of_low=%d size_of_cache=%d\n",
	valid_low, valid_high, index_of_low, size_of_cache));
    dprint((2,
	"n_cached=%d new_index=%d known_index=%d next_index=%d\n",
	n_cached, new_index, known_index, next_index));
    dprint((2,
	"next_row=%ld prev_row=%ld row=%ld\n", next_row, prev_row, row));
    /* jump back to a safe starting point */
    longjmp(addrbook_changed_unexpectedly, 1);
    /*NOTREACHED*/
}


void
free_cache_array(DL_CACHE_S **c_array, int size)
{
    DL_CACHE_S *dlc;
    int i;

    for(i = 0; i < size; i++){
	dlc = &(*c_array)[i];
	/* free any allocated space */
	switch(dlc->dl.type){
	  case Text:
	  case Title:
	  case TitleCmb:
	  case AskServer:
	    if(dlc->dl.usst)
	      fs_give((void **)&dlc->dl.usst);

	    break;
	  default:
	    break;
	}
    }

    fs_give((void **)c_array);
}


/*
 * Get the dlc element that comes before "old".  The function that calls this
 * function is the one that keeps a cache and checks in the cache before
 * calling here.  New is a passed in pointer to a buffer where we fill in
 * the answer.
 */
DL_CACHE_S *
dlc_prev(DL_CACHE_S *old, DL_CACHE_S *new)
{
    PerAddrBook  *pab;
    AdrBk_Entry  *abe;
    adrbk_cntr_t  list_count;

    new->adrbk_num  = -2;
    new->dlcelnum   = NO_NEXT;
    new->dlcoffset  = NO_NEXT;
    new->type       = DlcNotSet;
    pab = &as.adrbks[old->adrbk_num];

    switch(old->type){
      case DlcTitle:
      case DlcTitleNoPerm:
	if(old->adrbk_num == 0 && as.config && as.how_many_personals == 0)
	  new->type = DlcGlobDelim2;
	else if(old->adrbk_num == 0)
	  new->type = DlcOneBeforeBeginning;
	else if(old->adrbk_num == as.how_many_personals)
	  new->type = DlcGlobDelim2;
	else
	  new->type = DlcTitleBlankTop;

	break;

      case DlcSubTitle:
	if(pab->access == NoAccess)
	  new->type = DlcTitleNoPerm;
	else
	  new->type = DlcTitle;

	break;

      case DlcTitleBlankTop:
	new->adrbk_num = old->adrbk_num - 1;
	new->type = DlcSubTitle;
	break;

      case DlcEmpty:
      case DlcZoomEmpty:
      case DlcNoPermission:
      case DlcNoAbooks:
	if(F_ON(F_CMBND_ABOOK_DISP,ps_global)){
	    if(as.n_addrbk == 1 && as.n_serv == 0)
	      new->type = DlcOneBeforeBeginning;
	    else
	      new->type = DlcTitleBlankBottomCmb;
	}
	else
	  new->type = DlcOneBeforeBeginning;

	break;

      case DlcSimple:
	{
	adrbk_cntr_t el;
	long i;

	i = old->dlcelnum;
	i--;
	el = old->dlcelnum - 1;
	while(i >= 0L){
	    if(!as.zoomed || entry_is_selected(pab->address_book->selects,
					       (a_c_arg_t)el))
	      break;
	    
	    el--;
	    i--;
	}

	if(i >= 0){
	    new->dlcelnum = el;
	    abe = adrbk_get_ae(pab->address_book, (a_c_arg_t) new->dlcelnum);
	    if(abe && abe->tag == Single)
	      new->type = DlcSimple;
	    else if(abe && abe->tag == List)
	      new->type = DlcListBlankBottom;
	}
	else if(F_ON(F_CMBND_ABOOK_DISP,ps_global)){
	    if(as.n_addrbk == 1 && as.n_serv == 0)
	      new->type = DlcOneBeforeBeginning;
	    else
	      new->type = DlcTitleBlankBottomCmb;
	}
	else
	  new->type = DlcOneBeforeBeginning;
	}

	break;

      case DlcListHead:
	{
	adrbk_cntr_t el;
	long i;

	i = old->dlcelnum;
	i--;
	el = old->dlcelnum - 1;
	while(i >= 0L){
	    if(!as.zoomed || entry_is_selected(pab->address_book->selects,
					       (a_c_arg_t)el))
	      break;
	    
	    el--;
	    i--;
	}

	if(i >= 0){
	    new->dlcelnum = el;
	    abe = adrbk_get_ae(pab->address_book, (a_c_arg_t) new->dlcelnum);
	    if(abe && abe->tag == Single){
		new->type  = DlcListBlankTop;
		new->dlcelnum = old->dlcelnum;
	    }
	    else if(abe && abe->tag == List)
	      new->type  = DlcListBlankBottom;
	}
	else if(F_ON(F_CMBND_ABOOK_DISP,ps_global)){
	    if(as.n_addrbk == 1 && as.n_serv == 0)
	      new->type = DlcOneBeforeBeginning;
	    else
	      new->type = DlcTitleBlankBottomCmb;
	}
	else
	  new->type = DlcOneBeforeBeginning;
	}

	break;

      case DlcListEnt:
	if(old->dlcoffset > 0){
	    new->type      = DlcListEnt;
	    new->dlcelnum  = old->dlcelnum;
	    new->dlcoffset = old->dlcoffset - 1;
	}
	else{
	    new->type     = DlcListHead;
	    new->dlcelnum = old->dlcelnum;
	}

	break;

      case DlcListClickHere:
      case DlcListEmpty:
	new->type     = DlcListHead;
	new->dlcelnum = old->dlcelnum;
	break;

      case DlcListBlankTop:  /* can only occur between a Simple and a List */
	new->type   = DlcSimple;
	{
	adrbk_cntr_t el;
	long i;

	i = old->dlcelnum;
	i--;
	el = old->dlcelnum - 1;
	while(i >= 0L){
	    if(!as.zoomed || entry_is_selected(pab->address_book->selects,
					       (a_c_arg_t)el))
	      break;
	    
	    el--;
	    i--;
	}

	if(i >= 0)
	  new->dlcelnum = el;
	else{
	    dprint((1, "Bug in addrbook: case ListBlankTop with no selected entry\n"));
	    goto oops;
	}
	}

	break;

      case DlcListBlankBottom:
	new->dlcelnum = old->dlcelnum;
	abe = adrbk_get_ae(pab->address_book, (a_c_arg_t) new->dlcelnum);
	if(F_ON(F_EXPANDED_DISTLISTS,ps_global)
	  || exp_is_expanded(pab->address_book->exp, (a_c_arg_t)new->dlcelnum)){
	    list_count = listmem_count_from_abe(abe);
	    if(list_count == 0)
	      new->type = DlcListEmpty;
	    else{
		new->type      = DlcListEnt;
		new->dlcoffset = list_count - 1;
	    }
	}
	else
	  new->type = DlcListClickHere;

	break;

      case DlcGlobDelim1:
	if(as.how_many_personals == 0)
	  new->type = DlcPersAdd;
	else{
	    new->adrbk_num = as.how_many_personals - 1;
	    new->type = DlcSubTitle;
	}
	break;

      case DlcGlobDelim2:
	new->type = DlcGlobDelim1;
	break;

      case DlcPersAdd:
	new->type = DlcOneBeforeBeginning;
	break;

      case DlcGlobAdd:
	new->type = DlcGlobDelim2;
	break;

      case DlcDirAccess:
	if(old->adrbk_num == 0 && as.n_addrbk == 0)
	  new->type = DlcOneBeforeBeginning;
	else if(old->adrbk_num == 0)
	  new->type = DlcDirDelim2;
	else
	  new->type = DlcDirBlankTop;

	break;
	
      case DlcDirSubTitle:
	new->type = DlcDirAccess;
	break;

      case DlcDirBlankTop:
	new->adrbk_num = old->adrbk_num -1;
	new->type = DlcDirSubTitle;
	break;

      case DlcDirDelim1:
        new->adrbk_num = as.n_addrbk - 1;
	if(as.n_addrbk == 0)
	  new->type = DlcNoAbooks;
	else
	  new = get_bottom_dl_of_adrbk(new->adrbk_num, new);

	break;

      case DlcDirDelim1a:
        new->type = DlcDirDelim1;
	break;

      case DlcDirDelim1b:
        new->type = DlcDirDelim1a;
	break;

      case DlcDirDelim1c:
        new->type = DlcDirDelim1b;
	break;

      case DlcDirDelim2:
	if(F_ON(F_CMBND_ABOOK_DISP,ps_global))
          new->type = DlcDirDelim1c;
	else
          new->type = DlcDirDelim1;

	break;

      case DlcTitleDashTopCmb:
	if(old->adrbk_num == 0)
	  new->type = DlcOneBeforeBeginning;
	else
	  new->type = DlcTitleBlankTopCmb;

	break;

      case DlcTitleCmb:
	new->type = DlcTitleDashTopCmb;
	break;

      case DlcTitleDashBottomCmb:
	new->type = DlcTitleCmb;
	break;

      case DlcTitleBlankBottomCmb:
	new->type = DlcTitleDashBottomCmb;
	break;

      case DlcClickHereCmb:
	if(as.n_addrbk == 1 && as.n_serv == 0)
	  new->type = DlcOneBeforeBeginning;
	else
	  new->type = DlcTitleBlankBottomCmb;

	break;

      case DlcTitleBlankTopCmb:
	new->adrbk_num = old->adrbk_num - 1;
	new = get_bottom_dl_of_adrbk(new->adrbk_num, new);
	break;

      case DlcBeginning:
      case DlcTwoBeforeBeginning:
	new->type = DlcBeginning;
	break;

      case DlcOneBeforeBeginning:
	new->type = DlcTwoBeforeBeginning;
	break;

oops:
      default:
	q_status_message(SM_ORDER | SM_DING, 5, 10,
	    _("Bug in addrbook, not supposed to happen, re-syncing..."));
	dprint((1, "Bug in addrbook, impossible case (%d) in dlc_prev, re-sync\n",
	    old->type));
	/* jump back to a safe starting point */
	longjmp(addrbook_changed_unexpectedly, 1);
	/*NOTREACHED*/
    }

    new->global_row = old->global_row - 1L;
    if(new->adrbk_num == -2)
      new->adrbk_num = old->adrbk_num;

    return(new);
}


/*
 * Get the dlc element that comes after "old".  The function that calls this
 * function is the one that keeps a cache and checks in the cache before
 * calling here.
 */
DL_CACHE_S *
dlc_next(DL_CACHE_S *old, DL_CACHE_S *new)
{
    PerAddrBook  *pab;
    AdrBk_Entry  *abe;
    adrbk_cntr_t  ab_count;
    adrbk_cntr_t  list_count;

    new->adrbk_num  = -2;
    new->dlcelnum   = NO_NEXT;
    new->dlcoffset  = NO_NEXT;
    new->type       = DlcNotSet;
    pab = &as.adrbks[old->adrbk_num];

    switch(old->type){
      case DlcTitle:
      case DlcTitleNoPerm:
	new->type = DlcSubTitle;
	break;

      case DlcSubTitle:
	if((old->adrbk_num == as.how_many_personals - 1) &&
	   (as.config || as.n_addrbk > as.how_many_personals))
	  new->type = DlcGlobDelim1;
	else if(as.n_serv && !as.config &&
		(old->adrbk_num == as.n_addrbk - 1))
	  new->type = DlcDirDelim1;
	else if(old->adrbk_num == as.n_addrbk - 1)
	  new->type = DlcEnd;
	else{
	    new->adrbk_num = old->adrbk_num + 1;
	    new->type = DlcTitleBlankTop;
	}

	break;

      case DlcTitleBlankTop:
	if(pab->access == NoAccess)
	  new->type = DlcTitleNoPerm;
	else
	  new->type = DlcTitle;

	break;

      case DlcEmpty:
      case DlcZoomEmpty:
      case DlcNoPermission:
      case DlcClickHereCmb:
	if(F_ON(F_CMBND_ABOOK_DISP,ps_global)){
	    if(old->adrbk_num < as.n_addrbk - 1){
		new->adrbk_num = old->adrbk_num + 1;
		new->type = DlcTitleBlankTopCmb;
	    }
	    else if(as.n_serv)
	      new->type = DlcDirDelim1;
	    else
	      new->type = DlcEnd;
	}
	else
          new->type = DlcEnd;

	break;

      case DlcNoAbooks:
      case DlcGlobAdd:
      case DlcEnd:
        new->type = DlcEnd;
	break;

      case DlcSimple:
	{
	adrbk_cntr_t el;
	long i;

	ab_count = adrbk_count(pab->address_book);
	i = old->dlcelnum;
	i++;
	el = old->dlcelnum + 1;
	while(i < ab_count){
	    if(!as.zoomed || entry_is_selected(pab->address_book->selects,
					       (a_c_arg_t)el))
	      break;
	    
	    el++;
	    i++;
	}

	if(i < ab_count){
	    new->dlcelnum = el;
	    abe = adrbk_get_ae(pab->address_book, (a_c_arg_t) new->dlcelnum);
	    if(abe && abe->tag == Single)
	      new->type = DlcSimple;
	    else if(abe && abe->tag == List)
	      new->type = DlcListBlankTop;
	}
	else if(F_ON(F_CMBND_ABOOK_DISP,ps_global)){
	    if(old->adrbk_num < as.n_addrbk - 1){
		new->adrbk_num = old->adrbk_num + 1;
		new->type = DlcTitleBlankTopCmb;
	    }
	    else if(as.n_serv)
	      new->type = DlcDirDelim1;
	    else
	      new->type = DlcEnd;
	}
	else
	  new->type = DlcEnd;
	}

	break;

      case DlcListHead:
	new->dlcelnum = old->dlcelnum;
	abe = adrbk_get_ae(pab->address_book, (a_c_arg_t) new->dlcelnum);
	if(F_ON(F_EXPANDED_DISTLISTS,ps_global)
	  || exp_is_expanded(pab->address_book->exp, (a_c_arg_t)new->dlcelnum)){
	    list_count = listmem_count_from_abe(abe);
	    if(list_count == 0)
	      new->type = DlcListEmpty;
	    else{
		new->type      = DlcListEnt;
		new->dlcoffset = 0;
	    }
	}
	else
	  new->type = DlcListClickHere;

	break;

      case DlcListEnt:
	new->dlcelnum = old->dlcelnum;
	abe = adrbk_get_ae(pab->address_book, (a_c_arg_t) new->dlcelnum);
	list_count = listmem_count_from_abe(abe);
	if(old->dlcoffset == list_count - 1){  /* last member of list */
	    adrbk_cntr_t el;
	    long i;

	    ab_count = adrbk_count(pab->address_book);
	    i = old->dlcelnum;
	    i++;
	    el = old->dlcelnum + 1;
	    while(i < ab_count){
		if(!as.zoomed || entry_is_selected(pab->address_book->selects,
						   (a_c_arg_t)el))
		  break;
		
		el++;
		i++;
	    }

	    if(i < ab_count)
	      new->type = DlcListBlankBottom;
	    else if(F_ON(F_CMBND_ABOOK_DISP,ps_global)){
		if(old->adrbk_num < as.n_addrbk - 1){
		    new->adrbk_num = old->adrbk_num + 1;
		    new->type = DlcTitleBlankTopCmb;
		}
		else if(as.n_serv)
		  new->type = DlcDirDelim1;
		else
		  new->type = DlcEnd;
	    }
	    else
	      new->type = DlcEnd;
	}
	else{
	    new->type      = DlcListEnt;
	    new->dlcoffset = old->dlcoffset + 1;
	}

	break;

      case DlcListClickHere:
      case DlcListEmpty:
	{
	adrbk_cntr_t el;
	long i;

	new->dlcelnum = old->dlcelnum;
	ab_count = adrbk_count(pab->address_book);
	i = old->dlcelnum;
	i++;
	el = old->dlcelnum + 1;
	while(i < ab_count){
	    if(!as.zoomed || entry_is_selected(pab->address_book->selects,
					       (a_c_arg_t)el))
	      break;
	    
	    el++;
	    i++;
	}

	if(i < ab_count)
	  new->type = DlcListBlankBottom;
	else if(F_ON(F_CMBND_ABOOK_DISP,ps_global)){
	    if(old->adrbk_num < as.n_addrbk - 1){
		new->adrbk_num = old->adrbk_num + 1;
		new->type = DlcTitleBlankTopCmb;
	    }
	    else if(as.n_serv)
	      new->type = DlcDirDelim1;
	    else
	      new->type = DlcEnd;
	}
	else
	  new->type = DlcEnd;
	}

	break;

      case DlcListBlankTop:
	new->type   = DlcListHead;
	new->dlcelnum  = old->dlcelnum;
	break;

      case DlcListBlankBottom:
	{
	adrbk_cntr_t el;
	long i;

	ab_count = adrbk_count(pab->address_book);
	i = old->dlcelnum;
	i++;
	el = old->dlcelnum + 1;
	while(i < ab_count){
	    if(!as.zoomed || entry_is_selected(pab->address_book->selects,
					       (a_c_arg_t)el))
	      break;
	    
	    el++;
	    i++;
	}

	if(i < ab_count){
	    new->dlcelnum = el;
	    abe = adrbk_get_ae(pab->address_book, (a_c_arg_t) new->dlcelnum);
	    if(abe && abe->tag == Single)
	      new->type = DlcSimple;
	    else if(abe && abe->tag == List)
	      new->type = DlcListHead;
	}
	else
	  /* can't happen */
	  new->type = DlcEnd;
	}

	break;

      case DlcGlobDelim1:
	new->type = DlcGlobDelim2;
	break;

      case DlcGlobDelim2:
	if(as.config && as.how_many_personals == as.n_addrbk)
	  new->type = DlcGlobAdd;
	else{
	    new->adrbk_num = as.how_many_personals;
	    pab = &as.adrbks[new->adrbk_num];
	    if(pab->access == NoAccess)
	      new->type = DlcTitleNoPerm;
	    else
	      new->type = DlcTitle;
	}

	break;

      case DlcDirDelim1:
	if(F_ON(F_CMBND_ABOOK_DISP,ps_global))
	  new->type = DlcDirDelim1a;
	else
	  new->type = DlcDirDelim2;

	new->adrbk_num = 0;
	break;

      case DlcDirDelim1a:
	new->type = DlcDirDelim1b;
	break;

      case DlcDirDelim1b:
	new->type = DlcDirDelim1c;
	break;

      case DlcDirDelim1c:
	new->type = DlcDirDelim2;
	new->adrbk_num = 0;
	break;

      case DlcDirDelim2:
	new->type = DlcDirAccess;
	new->adrbk_num = 0;
	break;

      case DlcPersAdd:
	new->type = DlcGlobDelim1;
	break;

      case DlcDirAccess:
	new->type = DlcDirSubTitle;
	break;

      case DlcDirSubTitle:
	if(old->adrbk_num == as.n_serv - 1)
	  new->type = DlcEnd;
	else{
	    new->type = DlcDirBlankTop;
	    new->adrbk_num = old->adrbk_num + 1;
	}

	break;

      case DlcDirBlankTop:
	new->type = DlcDirAccess;
	break;

      case DlcTitleDashTopCmb:
	new->type = DlcTitleCmb;
	break;

      case DlcTitleCmb:
	new->type = DlcTitleDashBottomCmb;
	break;

      case DlcTitleDashBottomCmb:
        new->type = DlcTitleBlankBottomCmb;
	break;

      case DlcTitleBlankBottomCmb:
	pab = &as.adrbks[old->adrbk_num];
	if(pab->ostatus != Open && pab->access != NoAccess)
	  new->type = DlcClickHereCmb;
	else
	  new = get_top_dl_of_adrbk(old->adrbk_num, new);

	break;

      case DlcTitleBlankTopCmb:
	new->type = DlcTitleDashTopCmb;
	break;

      case DlcOneBeforeBeginning:
	new = get_global_top_dlc(new);
	break;

      case DlcTwoBeforeBeginning:
	new->type = DlcOneBeforeBeginning;
	break;

      default:
	q_status_message(SM_ORDER | SM_DING, 5, 10,
	    _("Bug in addrbook, not supposed to happen, re-syncing..."));
	dprint((1, "Bug in addrbook, impossible case (%d) in dlc_next, re-sync\n",
		old->type));
	/* jump back to a safe starting point */
	longjmp(addrbook_changed_unexpectedly, 1);
	/*NOTREACHED*/
    }

    new->global_row = old->global_row + 1L;
    if(new->adrbk_num == -2)
      new->adrbk_num = old->adrbk_num;

    return(new);
}


/*
 * Get the display line at the very top of whole addrbook screen display.
 */
DL_CACHE_S *
get_global_top_dlc(DL_CACHE_S *new)
                      /* fill in answer here */
{
    PerAddrBook  *pab;

    new->dlcelnum   = NO_NEXT;
    new->dlcoffset  = NO_NEXT;
    new->type       = DlcNotSet;

    if(F_ON(F_CMBND_ABOOK_DISP,ps_global) && !as.config){
	new->adrbk_num = 0;
	if(as.n_addrbk > 1 || (as.n_serv && as.n_addrbk == 1))
	  new->type = DlcTitleDashTopCmb;
	else if(as.n_addrbk == 1)
	  new = get_top_dl_of_adrbk(new->adrbk_num, new);
	else if(as.n_serv > 0){				/* 1st directory */
	    new->type = DlcDirAccess;
	    new->adrbk_num = 0;
	}
	else
	  new->type = DlcNoAbooks;
    }
    else{
	new->adrbk_num = 0;

	if(as.config){
	    if(as.how_many_personals == 0)		/* no personals */
	      new->type = DlcPersAdd;
	    else{
		pab = &as.adrbks[new->adrbk_num];	/* 1st personal */
		if(pab->access == NoAccess)
		  new->type = DlcTitleNoPerm;
		else
		  new->type = DlcTitle;
	    }
	}
	else if(any_ab_open()){
	    new->adrbk_num = as.cur;
	    new = get_top_dl_of_adrbk(new->adrbk_num, new);
	}
	else if(as.n_addrbk > 0){			/* 1st addrbook */
	    pab = &as.adrbks[new->adrbk_num];
	    if(pab->access == NoAccess)
	      new->type = DlcTitleNoPerm;
	    else
	      new->type = DlcTitle;
	}
	else if(as.n_serv > 0){				/* 1st directory */
	    new->type = DlcDirAccess;
	    new->adrbk_num = 0;
	}
	else
	  new->type = DlcNoAbooks;
    }

    return(new);
}


/*
 * Get the last display line for the whole address book screen.
 * This gives us a way to start at the end and move back up.
 */
DL_CACHE_S *
get_global_bottom_dlc(DL_CACHE_S *new)
                      /* fill in answer here */
{
    new->dlcelnum   = NO_NEXT;
    new->dlcoffset  = NO_NEXT;
    new->type       = DlcNotSet;

    if(F_ON(F_CMBND_ABOOK_DISP,ps_global) && !as.config){
	if(as.n_serv){
	    new->type = DlcDirSubTitle;
	    new->adrbk_num = as.n_serv - 1;
	}
	else if(as.n_addrbk > 0){
	    new->adrbk_num = as.n_addrbk - 1;
	    new = get_bottom_dl_of_adrbk(new->adrbk_num, new);
	}
	else
	  new->type = DlcNoAbooks;
    }
    else{
	new->adrbk_num = MAX(as.n_addrbk - 1, 0);

	if(as.config){
	    if(as.how_many_personals == as.n_addrbk)	/* no globals */
	      new->type = DlcGlobAdd;
	    else
	      new->type = DlcSubTitle;
	}
	else if(any_ab_open()){
	    new->adrbk_num = as.cur;
	    new = get_bottom_dl_of_adrbk(new->adrbk_num, new);
	}
	else if(as.n_serv){
	    new->type = DlcDirSubTitle;
	    new->adrbk_num = as.n_serv - 1;
	}
	else{				/* !config && !opened && !n_serv */
	    if(as.n_addrbk)
	      new->type = DlcSubTitle;
	    else
	      new->type = DlcNoAbooks;
	}
    }

    return(new);
}


/*
 * First dl in a particular addrbook, not counting title lines.
 * Assumes as.opened.
 */
DL_CACHE_S *
get_top_dl_of_adrbk(int adrbk_num, DL_CACHE_S *new)
                          
                      /* fill in answer here */
{
    PerAddrBook  *pab;
    AdrBk_Entry  *abe;
    adrbk_cntr_t  ab_count;

    pab = &as.adrbks[adrbk_num];
    new->adrbk_num = adrbk_num;

    if(pab->access == NoAccess)
      new->type = DlcNoPermission;
    else{
	adrbk_cntr_t el;
	long i;

	ab_count = adrbk_count(pab->address_book);

	i = 0L;
	el = 0;
	/* find first displayed entry */
	while(i < ab_count){
	    if(!as.zoomed || entry_is_selected(pab->address_book->selects,
					       (a_c_arg_t)el))
	      break;
	    
	    el++;
	    i++;
	}

	if(i < ab_count){
	    new->dlcelnum = el;
	    abe = adrbk_get_ae(pab->address_book, (a_c_arg_t) new->dlcelnum);
	    if(abe && abe->tag == Single)
	      new->type = DlcSimple;
	    else if(abe && abe->tag == List)
	      new->type = DlcListHead;
	}
	else if(as.zoomed)
	  new->type = DlcZoomEmpty;
	else
	  new->type = DlcEmpty;
    }

    return(new);
}


/*
 * Find the last display line for addrbook number adrbk_num.
 * Assumes as.opened (unless OLD_ABOOK_DISP).
 */
DL_CACHE_S *
get_bottom_dl_of_adrbk(int adrbk_num, DL_CACHE_S *new)
                          
                      /* fill in answer here */
{
    PerAddrBook  *pab;
    AdrBk_Entry  *abe;
    adrbk_cntr_t  ab_count;
    adrbk_cntr_t  list_count;

    pab = &as.adrbks[adrbk_num];
    new->adrbk_num = adrbk_num;

    if(F_ON(F_CMBND_ABOOK_DISP,ps_global) && pab->ostatus != Open){
	if(pab->access == NoAccess)
	  new->type = DlcNoPermission;
	else
	  new->type = DlcClickHereCmb;
    }
    else if(F_OFF(F_CMBND_ABOOK_DISP,ps_global) && pab->ostatus != Open){
	new->type = DlcSubTitle;
    }
    else{
	if(pab->access == NoAccess)
	  new->type = DlcNoPermission;
	else{
	    adrbk_cntr_t el;
	    long i;

	    ab_count = adrbk_count(pab->address_book);
	    i = ab_count - 1;
	    el = ab_count - 1;
	    /* find last displayed entry */
	    while(i >= 0L){
		if(!as.zoomed || entry_is_selected(pab->address_book->selects,
						   (a_c_arg_t)el))
		  break;
		
		el--;
		i--;
	    }

	    if(i >= 0){
		new->dlcelnum = el;
		abe = adrbk_get_ae(pab->address_book, (a_c_arg_t)new->dlcelnum);
		if(abe && abe->tag == Single)
		  new->type = DlcSimple;
		else if(abe && abe->tag == List){
		    if(F_ON(F_EXPANDED_DISTLISTS,ps_global)
		       || exp_is_expanded(pab->address_book->exp,
					  (a_c_arg_t)new->dlcelnum)){
			list_count = listmem_count_from_abe(abe);
			if(list_count == 0)
			  new->type = DlcListEmpty;
			else{
			    new->type      = DlcListEnt;
			    new->dlcoffset = list_count - 1;
			}
		    }
		    else
		      new->type = DlcListClickHere;
		}
	    }
	    else if(as.zoomed)
	      new->type = DlcZoomEmpty;
	    else
	      new->type = DlcEmpty;
	}

    }

    return(new);
}


/*
 * This returns the actual dlc instead of the dl within the dlc.
 */
DL_CACHE_S *
get_dlc(long int row)
{
    dprint((11, "- get_dlc(%ld) -\n", row));

    return(dlc_mgr(row, Lookup, (DL_CACHE_S *)NULL));
}


/*
 * Move to new_dlc and give it row number row_number_to_assign_it.
 * We copy the passed in dlc in case the caller passed us a pointer into
 * the cache.
 */
void
warp_to_dlc(DL_CACHE_S *new_dlc, long int row_number_to_assign_it)
{
    DL_CACHE_S dlc;

    dprint((9, "- warp_to_dlc(%ld) -\n", row_number_to_assign_it));

    dlc = *new_dlc;

    /*
     * Make sure we can move forward and backward from these
     * types that we may wish to warp to. The caller may not have known
     * to set adrbk_num for these types.
     */
    switch(dlc.type){
      case DlcPersAdd:
	dlc.adrbk_num = 0;
	break;
      case DlcGlobAdd:
	dlc.adrbk_num = as.n_addrbk;
	break;
      default:
	break;
    }

    (void)dlc_mgr(row_number_to_assign_it, ArbitraryStartingPoint, &dlc);
}


/*
 * Move to first dlc and give it row number 0.
 */
void
warp_to_beginning(void)
{
    dprint((9, "- warp_to_beginning -\n"));

    (void)dlc_mgr(0L, FirstEntry, (DL_CACHE_S *)NULL);
}


/*
 * Move to first dlc in abook_num and give it row number 0.
 */
void
warp_to_top_of_abook(int abook_num)
{
    DL_CACHE_S dlc;

    dprint((9, "- warp_to_top_of_abook(%d) -\n", abook_num));

    (void)get_top_dl_of_adrbk(abook_num, &dlc);
    warp_to_dlc(&dlc, 0L);
}


/*
 * Move to last dlc and give it row number 0.
 */
void
warp_to_end(void)
{
    dprint((9, "- warp_to_end -\n"));

    (void)dlc_mgr(0L, LastEntry, (DL_CACHE_S *)NULL);
}


/*
 * This flushes all of the cache that is related to this dlc.
 */
void
flush_dlc_from_cache(DL_CACHE_S *dlc_to_flush)
{
    dprint((11, "- flush_dlc_from_cache -\n"));

    (void)dlc_mgr(NO_LINE, FlushDlcFromCache, dlc_to_flush);
}


/*
 * Returns 1 if the dlc's match, 0 otherwise.
 */
int
matching_dlcs(DL_CACHE_S *dlc1, DL_CACHE_S *dlc2)
{
    if(!dlc1 || !dlc2 ||
        dlc1->type != dlc2->type ||
	dlc1->adrbk_num != dlc2->adrbk_num)
	return 0;

    switch(dlc1->type){

      case DlcSimple:
      case DlcListHead:
      case DlcListBlankTop:
      case DlcListBlankBottom:
      case DlcListClickHere:
      case DlcListEmpty:
	return(dlc1->dlcelnum == dlc2->dlcelnum);

      case DlcListEnt:
	return(dlc1->dlcelnum  == dlc2->dlcelnum &&
	       dlc1->dlcoffset == dlc2->dlcoffset);

      case DlcTitle:
      case DlcSubTitle:
      case DlcTitleNoPerm:
      case DlcTitleBlankTop:
      case DlcEmpty:
      case DlcZoomEmpty:
      case DlcNoPermission:
      case DlcPersAdd:
      case DlcGlobAdd:
      case DlcGlobDelim1:
      case DlcGlobDelim2:
      case DlcDirAccess:
      case DlcDirDelim1:
      case DlcDirDelim2:
      case DlcTitleDashTopCmb:
      case DlcTitleCmb:
      case DlcTitleDashBottomCmb:
      case DlcTitleBlankBottomCmb:
      case DlcClickHereCmb:
      case DlcTitleBlankTopCmb:
	return 1;

      case DlcNotSet:
      case DlcBeginning:
      case DlcOneBeforeBeginning:
      case DlcTwoBeforeBeginning:
      case DlcEnd:
      case DlcNoAbooks:
	return 0;

      default:
	break;
    }
    /*NOTREACHED*/

    return 0;
}


/*
 * Uses information in new to fill in new->dl.
 */
void
fill_in_dl_field(DL_CACHE_S *new)
{
    AddrScrn_Disp *dl;
    PerAddrBook   *pab;
    char buf[6*MAX_SCREEN_COLS + 1];
    char buf2[6*1024];
    char hostbuf[128];
    char *folder;
    char *q;
    unsigned screen_width = ps_global->ttyo->screen_cols;
    unsigned got_width, need_width, cellwidth;

    screen_width = MIN(MAX_SCREEN_COLS, screen_width);

    dl = &(new->dl);

    /* free any previously allocated space */
    switch(dl->type){
      case Text:
      case Title:
      case TitleCmb:
      case AskServer:
	if(dl->usst)
	  fs_give((void **)&dl->usst);
      default:
        break;
    }

    /* set up new dl */
    switch(new->type){
      case DlcListBlankTop:
      case DlcListBlankBottom:
      case DlcGlobDelim1:
      case DlcGlobDelim2:
      case DlcDirDelim1:
      case DlcDirDelim2:
      case DlcTitleBlankTop:
      case DlcDirBlankTop:
      case DlcTitleBlankBottomCmb:
      case DlcTitleBlankTopCmb:
	dl->type = Text;
	dl->usst = cpystr("");
	break;

      case DlcTitleDashTopCmb:
      case DlcTitleDashBottomCmb:
      case DlcDirDelim1a:
      case DlcDirDelim1c:
	/* line of dashes in txt field */
	dl->type = Text;
	memset((void *)buf, '-', screen_width * sizeof(char));
	buf[screen_width] = '\0';
	dl->usst = cpystr(buf);
	break;

      case DlcNoPermission:
	dl->type = Text;
	dl->usst = cpystr(NO_PERMISSION);
	break;

      case DlcTitle:
      case DlcTitleNoPerm:
	dl->type = Title;
	pab = &as.adrbks[new->adrbk_num];
        /* title for this addrbook */
        snprintf(buf, sizeof(buf), "    %s", pab->abnick ? pab->abnick : pab->filename);
	cellwidth = utf8_width(buf);
	if(cellwidth > screen_width)
	  cellwidth = utf8_truncate(buf, screen_width);

	/* add space padding */
	if(cellwidth < screen_width)
	  snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%*.*s",
		   screen_width - cellwidth, screen_width - cellwidth, "");

	buf[sizeof(buf)-1] = '\0';

        if(as.ro_warning && (pab->access == NoAccess || pab->access == ReadOnly)){
	    char *str;

	    if(pab->access == NoAccess)
	      str = NOACCESS;
	    else
	      str = READONLY;

	    need_width = utf8_width(str);

	    if(screen_width > need_width && (q = utf8_count_back_width(buf, buf+strlen(buf), need_width, &got_width)) != NULL)

	      (void) utf8_pad_to_width(q, str, sizeof(buf)-(q-buf), need_width, 0);
	}

	buf[sizeof(buf)-1] = '\0';
	dl->usst = cpystr(buf);
	break;

      case DlcSubTitle:
	dl->type = Text;
	pab = &as.adrbks[new->adrbk_num];
	/* Find a hostname to put in title */
	hostbuf[0] = '\0';
	folder = NULL;
	if(pab->type & REMOTE_VIA_IMAP && pab->filename){
	    char *start, *end;

	    start = strindex(pab->filename, '{');
	    if(start)
	      end = strindex(start+1, '}');

	    if(start && end){
		strncpy(hostbuf, start + 1,
			MIN(end - start - 1, sizeof(hostbuf)-1));
		hostbuf[MIN(end - start - 1, sizeof(hostbuf)-1)] = '\0';
		if(*(end+1))
		  folder = end+1;
	    }
	}

	if(!folder)
	  folder = pab->filename;

	/*
	 * Just trying to find the name relative to the home directory
	 * to display in an OS-independent way.
	 */
	if(folder && in_dir(ps_global->home_dir, folder)){
	    char *p, *new_folder = NULL, *savep = NULL;
	    int   l, saveval = 0;

	    l = strlen(ps_global->home_dir);

	    while(!new_folder && (p = last_cmpnt(folder)) != NULL){
		if(savep){
		    *savep = saveval;
		    savep = NULL;
		}

		if(folder + l == p || folder + l + 1 == p)
		  new_folder = p;
		else{
		    savep = --p;
		    saveval = *savep;
		    *savep = '\0';
		}
	    }

	    if(savep)
	      *savep = saveval;

	    if(new_folder)
	      folder = new_folder;
	}

        snprintf(buf, sizeof(buf), "        %s AddressBook%s%s in %s",
		(pab->type & GLOBAL) ? "Global" : "Personal",
		(pab->type & REMOTE_VIA_IMAP && *hostbuf) ? " on " : "",
		(pab->type & REMOTE_VIA_IMAP && *hostbuf) ? hostbuf : "",
		(folder && *folder) ? folder : "<?>");
	cellwidth = utf8_width(buf);
	if(cellwidth > screen_width)
	  cellwidth = utf8_truncate(buf, screen_width);

	/* add space padding */
	if(cellwidth < screen_width)
	  snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%*.*s",
		   screen_width - cellwidth, screen_width - cellwidth, "");

	buf[sizeof(buf)-1] = '\0';
	dl->usst = cpystr(buf);
	break;

      case DlcTitleCmb:
	dl->type = TitleCmb;
	pab = &as.adrbks[new->adrbk_num];
        /* title for this addrbook */
        snprintf(buf, sizeof(buf), "%s AddressBook <%s>",
		    (new->adrbk_num < as.how_many_personals) ?
			"Personal" :
			"Global",
                    pab->abnick ? pab->abnick : pab->filename);
	cellwidth = utf8_width(buf);
	if(cellwidth > screen_width)
	  cellwidth = utf8_truncate(buf, screen_width);

	/* add space padding */
	if(cellwidth < screen_width)
	  snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%*.*s",
		   screen_width - cellwidth, screen_width - cellwidth, "");

	buf[sizeof(buf)-1] = '\0';

        if(as.ro_warning && (pab->access == NoAccess || pab->access == ReadOnly)){
	    char *str;

	    if(pab->access == NoAccess)
	      str = NOACCESS;
	    else
	      str = READONLY;

	    need_width = utf8_width(str);

	    if(screen_width > need_width && (q = utf8_count_back_width(buf, buf+strlen(buf), need_width, &got_width)) != NULL)

	      (void) utf8_pad_to_width(q, str, sizeof(buf)-(q-buf), need_width, 0);
	}

	buf[sizeof(buf)-1] = '\0';
	dl->usst = cpystr(buf);
	break;

      case DlcDirDelim1b:
	dl->type = Text;
	utf8_snprintf(buf, sizeof(buf), "%-*.*w", screen_width, screen_width, OLDSTYLE_DIR_TITLE);
	dl->usst = cpystr(buf);
	break;

      case DlcClickHereCmb:
	dl->type  = ClickHereCmb;
	break;

      case DlcListClickHere:
	dl->type  = ListClickHere;
	dl->elnum = new->dlcelnum;
	break;

      case DlcListEmpty:
	dl->type  = ListEmpty;
	dl->elnum = new->dlcelnum;
	break;

      case DlcEmpty:
	dl->type = Empty;
	break;

      case DlcZoomEmpty:
	dl->type = ZoomEmpty;
	break;

      case DlcNoAbooks:
	dl->type = NoAbooks;
	break;

      case DlcPersAdd:
	dl->type = AddFirstPers;
	break;

      case DlcGlobAdd:
	dl->type = AddFirstGlob;
	break;

      case DlcDirAccess:
	dl->type = AskServer;

#ifdef	ENABLE_LDAP
	{
	LDAP_SERV_S *info;

	info = break_up_ldap_server(ps_global->VAR_LDAP_SERVERS[new->adrbk_num]);
	snprintf(buf2, sizeof(buf2), "    %s",
		(info && info->nick && *info->nick) ? info->nick :
		  (info && info->serv && *info->serv) ? info->serv :
		    comatose(new->adrbk_num + 1));
	if(info)
	  free_ldap_server_info(&info);
	}
#else
	snprintf(buf2, sizeof(buf2), "    %s", comatose(new->adrbk_num + 1));
#endif

	utf8_snprintf(buf, sizeof(buf), "%-*.*w", screen_width, screen_width, buf2);
	dl->usst = cpystr(buf);
	break;

      case DlcDirSubTitle:
	dl->type = Text;
#ifdef	ENABLE_LDAP
	{
	LDAP_SERV_S *info;

	info = break_up_ldap_server(ps_global->VAR_LDAP_SERVERS[new->adrbk_num]);
	if(info && info->port >= 0)
	  snprintf(buf2, sizeof(buf2), "        Directory Server on %s:%d",
		  (info && info->serv && *info->serv) ? info->serv : "<?>",
		  info->port);
	else
	  snprintf(buf2, sizeof(buf2), "        Directory Server on %s",
		  (info && info->serv && *info->serv) ? info->serv : "<?>");

	if(info)
	  free_ldap_server_info(&info);
	}
#else
	snprintf(buf2, sizeof(buf2), "        Directory Server %s",
		comatose(new->adrbk_num + 1));
#endif

	utf8_snprintf(buf, sizeof(buf), "%-*.*w", screen_width, screen_width, buf2);
	dl->usst = cpystr(buf);
	break;

      case DlcSimple:
	dl->type  = Simple;
	dl->elnum = new->dlcelnum;
	break;

      case DlcListHead:
	dl->type  = ListHead;
	dl->elnum = new->dlcelnum;
	break;

      case DlcListEnt:
	dl->type     = ListEnt;
	dl->elnum    = new->dlcelnum;
	dl->l_offset = new->dlcoffset;
	break;

      case DlcBeginning:
      case DlcOneBeforeBeginning:
      case DlcTwoBeforeBeginning:
	dl->type = Beginning;
	break;

      case DlcEnd:
	dl->type = End;
	break;

      default:
	q_status_message(SM_ORDER | SM_DING, 5, 10,
	    _("Bug in addrbook, not supposed to happen, re-syncing..."));
	dprint((1, "Bug in addrbook, impossible dflt in fill_in_dl (%d)\n",
		new->type));
	/* jump back to a safe starting point */
	longjmp(addrbook_changed_unexpectedly, 1);
	/*NOTREACHED*/
    }
}


/*
 * Returns a pointer to the member_number'th list member of the list
 * associated with this display line.
 */
char *
listmem(long int row)
{
    PerAddrBook *pab;
    AddrScrn_Disp *dl;

    dl = dlist(row);
    if(dl->type != ListEnt)
      return((char *)NULL);

    pab = &as.adrbks[adrbk_num_from_lineno(row)];

    return(listmem_from_dl(pab->address_book, dl));
}


/*
 * Returns a pointer to the list member
 * associated with this display line.
 */
char *
listmem_from_dl(AdrBk *address_book, AddrScrn_Disp *dl)
{
    AdrBk_Entry *abe;
    char **p = (char **)NULL;

    /* This shouldn't happen */
    if(dl->type != ListEnt)
      return((char *)NULL);

    abe = adrbk_get_ae(address_book, (a_c_arg_t) dl->elnum);

    /*
     * If we wanted to be more careful, We'd go through the list making sure
     * we don't pass the end.  We'll count on the caller being careful
     * instead.
     */
    if(abe && abe->tag == List){
	p =  abe->addr.list;
	p += dl->l_offset;
    }

    return((p && *p) ? *p : (char *)NULL);
}


/*
 * How many members in list?
 */
adrbk_cntr_t
listmem_count_from_abe(AdrBk_Entry *abe)
{
    char **p;

    if(abe->tag != List)
      return 0;

    for(p = abe->addr.list; p != NULL && *p != NULL; p++)
      ;/* do nothing */
    
    return((adrbk_cntr_t)(p - abe->addr.list));
}


/*
 * Return a ptr to the row'th line of the global disp_list.
 * Line numbers count up but you can't count on knowing which line number
 * goes with the first or the last row.  That is, row 0 is not necessarily
 * special.  It could be before the rows that make up the display list, after
 * them, or anywhere in between.  You can't tell what the last row is
 * numbered, but a dl with type End is returned when you get past the end.
 * You can't tell what the number of the first row is, but if you go past
 * the first row a dl of type Beginning will be returned.  Row numbers can
 * be positive or negative.  Their values have no meaning other than how
 * they line up relative to other row numbers.
 */
AddrScrn_Disp *
dlist(long int row)
{
    DL_CACHE_S *dlc = (DL_CACHE_S *)NULL;

    dlc = get_dlc(row);

    if(dlc){
	fill_in_dl_field(dlc);
	return(&dlc->dl);
    }
    else{
	q_status_message(SM_ORDER | SM_DING, 5, 10,
		     _("Bug in addrbook, not supposed to happen, re-syncing..."));
	dprint((1, "Bug in addrbook (null dlc in dlist(%ld), not supposed to happen\n", row));
	/* jump back to a safe starting point */

	dprint((1, "dlist: panic: initialized %d, n_addrbk %d, cur_row %d, top_ent %ld, ro_warning %d, no_op_possbl %d\n",
		as.initialized, as.n_addrbk, as.cur_row, as.top_ent, as.ro_warning, as.no_op_possbl));

	longjmp(addrbook_changed_unexpectedly, 1);
	/*NOTREACHED*/
    }
}


/*
 * Returns the index of the current address book.
 */
int
adrbk_num_from_lineno(long int lineno)
{
    DL_CACHE_S *dlc;

    dlc = get_dlc(lineno);

    return(dlc->adrbk_num);
}
