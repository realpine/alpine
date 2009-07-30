#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: ablookup.c 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $";
#endif

/*
 * ========================================================================
 * Copyright 2006-2009 University of Washington
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
#include "../pith/debug.h"
#include "../pith/util.h"
#include "../pith/adrbklib.h"
#include "../pith/copyaddr.h"
#include "../pith/status.h"
#include "../pith/conf.h"
#include "../pith/search.h"
#include "../pith/abdlc.h"
#include "../pith/addrstring.h"
#include "../pith/ablookup.h"
#include "../pith/options.h"
#include "../pith/takeaddr.h"
#ifdef	ENABLE_LDAP
#include "../pith/ldap.h"
#endif	/* ENABLE_LDAP */


/*
 * Internal prototypes
 */
int   addr_is_in_addrbook(PerAddrBook *, ADDRESS *);
ABOOK_ENTRY_S *adrbk_list_of_possible_trie_completions(AdrBk_Trie *, AdrBk *, char *, unsigned);
void  gather_abook_entry_list(AdrBk *, AdrBk_Trie *, char *, ABOOK_ENTRY_S **, unsigned);
void  add_addr_to_return_list(ADDRESS *, unsigned, char *, int, COMPLETE_S **);


/*
 * Given an address, try to find the first nickname that goes with it.
 * Copies that nickname into the passed in buffer, which is assumed to
 * be at least MAX_NICKNAME+1 in length.  Returns NULL if it can't be found,
 * else it returns a pointer to the buffer.
 */
char *
get_nickname_from_addr(struct mail_address *adr, char *buffer, size_t buflen)
{
    AdrBk_Entry *abe;
    char        *ret = NULL;
    SAVE_STATE_S state;
    jmp_buf	 save_jmp_buf;
    int         *save_nesting_level;
    ADDRESS     *copied_adr;

    state.savep = NULL;
    state.stp = NULL;
    state.dlc_to_warp_to = NULL;
    copied_adr = copyaddr(adr);

    if(ps_global->remote_abook_validity > 0)
      (void)adrbk_check_and_fix_all(ab_nesting_level == 0, 0, 0);

    save_nesting_level = cpyint(ab_nesting_level);
    memcpy(save_jmp_buf, addrbook_changed_unexpectedly, sizeof(jmp_buf));
    if(setjmp(addrbook_changed_unexpectedly)){
	ret = NULL;
	if(state.savep)
	  fs_give((void **)&(state.savep));
	if(state.stp)
	  fs_give((void **)&(state.stp));
	if(state.dlc_to_warp_to)
	  fs_give((void **)&(state.dlc_to_warp_to));

	q_status_message(SM_ORDER, 3, 5, _("Resetting address book..."));
	dprint((1,
	    "RESETTING address book... get_nickname_from_addr()!\n"));
	addrbook_reset();
	ab_nesting_level = *save_nesting_level;
    }

    ab_nesting_level++;
    init_ab_if_needed();

    if(pith_opt_save_and_restore)
      (*pith_opt_save_and_restore)(SAR_SAVE, &state);

    abe = address_to_abe(copied_adr);

    if(copied_adr)
      mail_free_address(&copied_adr);

    if(abe && abe->nickname && abe->nickname[0]){
	strncpy(buffer, abe->nickname, buflen-1);
	buffer[buflen-1] = '\0';
	ret = buffer;
    }

    if(pith_opt_save_and_restore)
      (*pith_opt_save_and_restore)(SAR_RESTORE, &state);

    memcpy(addrbook_changed_unexpectedly, save_jmp_buf, sizeof(jmp_buf));
    ab_nesting_level--;

    if(save_nesting_level)
      fs_give((void **)&save_nesting_level);

    return(ret);
}


/*
 * Given an address, try to find the first fcc that goes with it.
 * Copies that fcc into the passed in buffer.
 * Returns NULL if it can't be found, else it returns a pointer to the buffer.
 */
char *
get_fcc_from_addr(struct mail_address *adr, char *buffer, size_t buflen)
{
    AdrBk_Entry *abe;
    char        *ret = NULL;
    SAVE_STATE_S state;
    jmp_buf	 save_jmp_buf;
    int         *save_nesting_level;
    ADDRESS     *copied_adr;

    state.savep = NULL;
    state.stp = NULL;
    state.dlc_to_warp_to = NULL;
    copied_adr = copyaddr(adr);

    if(ps_global->remote_abook_validity > 0)
      (void)adrbk_check_and_fix_all(ab_nesting_level == 0, 0, 0);

    save_nesting_level = cpyint(ab_nesting_level);
    memcpy(save_jmp_buf, addrbook_changed_unexpectedly, sizeof(jmp_buf));
    if(setjmp(addrbook_changed_unexpectedly)){
	ret = NULL;
	if(state.savep)
	  fs_give((void **)&(state.savep));
	if(state.stp)
	  fs_give((void **)&(state.stp));
	if(state.dlc_to_warp_to)
	  fs_give((void **)&(state.dlc_to_warp_to));

	q_status_message(SM_ORDER, 3, 5, _("Resetting address book..."));
	dprint((1,
	    "RESETTING address book... get_fcc_from_addr()!\n"));
	addrbook_reset();
	ab_nesting_level = *save_nesting_level;
    }

    ab_nesting_level++;
    init_ab_if_needed();

    if(pith_opt_save_and_restore)
      (*pith_opt_save_and_restore)(SAR_SAVE, &state);

    abe = address_to_abe(copied_adr);

    if(copied_adr)
      mail_free_address(&copied_adr);

    if(abe && abe->fcc && abe->fcc[0]){
	if(!strcmp(abe->fcc, "\"\""))
	  buffer[0] = '\0';
	else{
	    strncpy(buffer, abe->fcc, buflen-1);
	    buffer[buflen-1] = '\0';
	}

	ret = buffer;
    }

    if(pith_opt_save_and_restore)
      (*pith_opt_save_and_restore)(SAR_RESTORE, &state);

    memcpy(addrbook_changed_unexpectedly, save_jmp_buf, sizeof(jmp_buf));
    ab_nesting_level--;

    if(save_nesting_level)
      fs_give((void **)&save_nesting_level);

    return(ret);
}


/*
 * Given an address, try to find the first address book entry that
 * matches it and return all the other fields in the passed in pointers.
 * Caller needs to free the four fields.
 * Returns -1 if it can't be found, 0 if it is found.
 */
int
get_contactinfo_from_addr(struct mail_address *adr, char **nick, char **full, char **fcc, char **comment)
{
    AdrBk_Entry *abe;
    int          ret = -1;
    SAVE_STATE_S state;
    jmp_buf	 save_jmp_buf;
    int         *save_nesting_level;
    ADDRESS     *copied_adr;

    state.savep = NULL;
    state.stp = NULL;
    state.dlc_to_warp_to = NULL;
    copied_adr = copyaddr(adr);

    if(ps_global->remote_abook_validity > 0)
      (void)adrbk_check_and_fix_all(ab_nesting_level == 0, 0, 0);

    save_nesting_level = cpyint(ab_nesting_level);
    memcpy(save_jmp_buf, addrbook_changed_unexpectedly, sizeof(jmp_buf));
    if(setjmp(addrbook_changed_unexpectedly)){
	ret = -1;
	if(state.savep)
	  fs_give((void **)&(state.savep));
	if(state.stp)
	  fs_give((void **)&(state.stp));
	if(state.dlc_to_warp_to)
	  fs_give((void **)&(state.dlc_to_warp_to));

	q_status_message(SM_ORDER, 3, 5, _("Resetting address book..."));
	dprint((1,
	    "RESETTING address book... get_contactinfo_from_addr()!\n"));
	addrbook_reset();
	ab_nesting_level = *save_nesting_level;
    }

    ab_nesting_level++;
    init_ab_if_needed();

    if(pith_opt_save_and_restore)
      (*pith_opt_save_and_restore)(SAR_SAVE, &state);

    abe = address_to_abe(copied_adr);

    if(copied_adr)
      mail_free_address(&copied_adr);

    if(abe){
	if(nick && abe->nickname && abe->nickname[0])
	  *nick = cpystr(abe->nickname);

	if(full && abe->fullname && abe->fullname[0])
	  *full = cpystr(abe->fullname);

	if(fcc && abe->fcc && abe->fcc[0])
	  *fcc = cpystr(abe->fcc);

	if(comment && abe->extra && abe->extra[0])
	  *comment = cpystr(abe->extra);

	ret = 0;
    }

    if(pith_opt_save_and_restore)
      (*pith_opt_save_and_restore)(SAR_RESTORE, &state);

    memcpy(addrbook_changed_unexpectedly, save_jmp_buf, sizeof(jmp_buf));
    ab_nesting_level--;

    if(save_nesting_level)
      fs_give((void **)&save_nesting_level);

    return(ret);
}


/*
 * This is a very special-purpose routine.
 * It implements the From or Reply-To address is in the Address Book
 * part of Pattern matching.
 */
void
address_in_abook(MAILSTREAM *stream, SEARCHSET *searchset,
			 int inabook, PATTERN_S *abooks)
{
    char         *savebits;
    MESSAGECACHE *mc;
    long          i, count = 0L;
    SEARCHSET    *s, *ss;
    ADDRESS      *from, *reply_to, *sender, *to, *cc;
    int           is_there, adrbknum, *abooklist = NULL, positive_match;
    PATTERN_S    *pat;
    PerAddrBook  *pab;
    ENVELOPE     *e;
    SAVE_STATE_S  state;
    jmp_buf	  save_jmp_buf;
    int          *save_nesting_level;

    if(!stream)
      return;

    /* everything that matches remains a match */
    if(inabook == IAB_EITHER)
      return;

    state.savep          = NULL;
    state.stp            = NULL;
    state.dlc_to_warp_to = NULL;

    /*
     * This may call build_header_line recursively because we may be in
     * build_header_line now. So we have to preserve and restore the
     * sequence bits since we want to use them here.
     */
    savebits = (char *) fs_get((stream->nmsgs+1) * sizeof(char));

    for(i = 1L; i <= stream->nmsgs; i++){
	if((mc = mail_elt(stream, i)) != NULL){
	    savebits[i] = mc->sequence;
	    mc->sequence = 0;
	}
    }

    /*
     * Build a searchset so we can look at all the envelopes
     * we need to look at but only those we need to look at.
     * Everything with the searched bit set is still a
     * possibility, so restrict to that set.
     */

    for(s = searchset; s; s = s->next)
      for(i = s->first; i <= s->last; i++)
	if(i > 0L && i <= stream->nmsgs
	   && (mc=mail_elt(stream, i)) && mc->searched){
	    mc->sequence = 1;
	    count++;
	}

    ss = build_searchset(stream);

    /*
     * We save the address book state here so we don't have to do it
     * each time through the loop below.
     */
    if(ss){
	if(ps_global->remote_abook_validity > 0)
	  (void)adrbk_check_and_fix_all(ab_nesting_level == 0, 0, 0);

	save_nesting_level = cpyint(ab_nesting_level);
	memcpy(save_jmp_buf, addrbook_changed_unexpectedly, sizeof(jmp_buf));
	if(setjmp(addrbook_changed_unexpectedly)){
	    if(state.savep)
	      fs_give((void **)&(state.savep));
	    if(state.stp)
	      fs_give((void **)&(state.stp));
	    if(state.dlc_to_warp_to)
	      fs_give((void **)&(state.dlc_to_warp_to));

	    q_status_message(SM_ORDER, 3, 5, _("Resetting address book..."));
	    dprint((1,
		"RESETTING address book... address_in_abook()!\n"));
	    addrbook_reset();
	    ab_nesting_level = *save_nesting_level;
	}

	ab_nesting_level++;
	init_ab_if_needed();

	if(pith_opt_save_and_restore)
	  (*pith_opt_save_and_restore)(SAR_SAVE, &state);

	if(as.n_addrbk > 0){
	    abooklist = (int *) fs_get(as.n_addrbk * sizeof(*abooklist));
	    memset((void *) abooklist, 0, as.n_addrbk * sizeof(*abooklist));
	}

	if(abooklist)
	  switch(inabook & IAB_TYPE_MASK){
	    case IAB_YES:
	    case IAB_NO:
	      for(adrbknum = 0; adrbknum < as.n_addrbk; adrbknum++)
	        abooklist[adrbknum] = 1;

	      break;

	    case IAB_SPEC_YES:
	    case IAB_SPEC_NO:
	      /* figure out which address books we're going to look in */
	      for(adrbknum = 0; adrbknum < as.n_addrbk; adrbknum++){
		  pab = &as.adrbks[adrbknum];
		  /*
		   * For each address book, check all of the address books
		   * in the pattern's list to see if they are it.
		   */
		  for(pat = abooks; pat; pat = pat->next){
		      if(!strcmp(pab->abnick, pat->substring)
			 || !strcmp(pab->filename, pat->substring)){
			  abooklist[adrbknum] = 1;
			  break;
		      }
		  }
	      }

	      break;
	   }

	switch(inabook & IAB_TYPE_MASK){
	  case IAB_YES:
	  case IAB_SPEC_YES:
	    positive_match = 1;
	    break;

	  case IAB_NO:
	  case IAB_SPEC_NO:
	    positive_match = 0;
	    break;
	}
    }

    if(count){
	SEARCHSET    **sset;

	mail_parameters(NULL, SET_FETCHLOOKAHEADLIMIT, (void *) count);

	/*
	 * This causes the lookahead to fetch precisely
	 * the messages we want (in the searchset) instead
	 * of just fetching the next 20 sequential
	 * messages. If the searching so far has caused
	 * a sparse searchset in a large mailbox, the
	 * difference can be substantial.
	 * This resets automatically after the first fetch.
	 */
	sset = (SEARCHSET **) mail_parameters(stream,
					      GET_FETCHLOOKAHEAD,
					      (void *) stream);
	if(sset)
	  *sset = ss;
    }

    for(s = ss; s; s = s->next){
	for(i = s->first; i <= s->last; i++){
	    if(i <= 0L || i > stream->nmsgs)
	      continue;

	    e = pine_mail_fetchenvelope(stream, i);

	    from = e ? e->from : NULL;
	    reply_to = e ? e->reply_to : NULL;
	    sender = e ? e->sender : NULL;
	    to = e ? e->to : NULL;
	    cc = e ? e->cc : NULL;

	    is_there = 0;
	    for(adrbknum = 0; !is_there && adrbknum < as.n_addrbk; adrbknum++){
		if(!abooklist[adrbknum])
		  continue;
		
		pab = &as.adrbks[adrbknum];
		is_there = ((inabook & IAB_FROM) && addr_is_in_addrbook(pab, from))
			   || ((inabook & IAB_REPLYTO) && addr_is_in_addrbook(pab, reply_to))
			   || ((inabook & IAB_SENDER) && addr_is_in_addrbook(pab, sender))
			   || ((inabook & IAB_TO) && addr_is_in_addrbook(pab, to))
			   || ((inabook & IAB_CC) && addr_is_in_addrbook(pab, cc));
	    }

	    if(positive_match){
		/*
		 * We matched up until now. If it isn't there, then it
		 * isn't a match. If it is there, leave the searched bit
		 * set.
		 */
		if(!is_there && i > 0L && i <= stream->nmsgs && (mc = mail_elt(stream, i)))
		  mc->searched = NIL;
	    }
	    else{
		if(is_there && i > 0L && i <= stream->nmsgs && (mc = mail_elt(stream, i)))
		  mc->searched = NIL;
	    }
	}
    }

    if(ss){
	if(pith_opt_save_and_restore)
	  (*pith_opt_save_and_restore)(SAR_RESTORE, &state);

	memcpy(addrbook_changed_unexpectedly, save_jmp_buf, sizeof(jmp_buf));
	ab_nesting_level--;

	if(save_nesting_level)
	  fs_give((void **)&save_nesting_level);
    }

    /* restore sequence bits */
    for(i = 1L; i <= stream->nmsgs; i++)
      if((mc = mail_elt(stream, i)) != NULL)
        mc->sequence = savebits[i];

    fs_give((void **) &savebits);

    if(ss)
      mail_free_searchset(&ss);

    if(abooklist)
      fs_give((void **) &abooklist);
}


/*
 * Given two addresses, check to see if either is in the address book.
 * Returns 1 if yes, 0 if not found.
 */
int
addr_is_in_addrbook(PerAddrBook *pab, struct mail_address *addr)
{
    AdrBk_Entry *abe = NULL;
    int          ret = 0;
    char         abuf[MAX_ADDR_FIELD+1];

    if(!(pab && addr && addr->mailbox))
      return(ret);

    if(addr){
	strncpy(abuf, addr->mailbox, sizeof(abuf)-1);
	abuf[sizeof(abuf)-1] = '\0';
	if(addr->host && addr->host[0]){
	    strncat(abuf, "@", sizeof(abuf)-strlen(abuf)-1);
	    strncat(abuf, addr->host, sizeof(abuf)-strlen(abuf)-1);
	}

	if(pab->ostatus != Open && pab->ostatus != NoDisplay)
	  init_abook(pab, NoDisplay);
	
	abe = adrbk_lookup_by_addr(pab->address_book, abuf,
				   (adrbk_cntr_t *) NULL);
    }

    ret = abe ? 1 : 0;

    return(ret);
}


/*
 * Look through addrbooks for nickname, opening addrbooks first
 * if necessary.  It is assumed that the caller will restore the
 * state of the addrbooks if desired.
 *
 * Args: nickname       -- the nickname to lookup
 *       clearrefs      -- clear reference bits before lookup
 *       which_addrbook -- If matched, addrbook number it was found in.
 *       not_here       -- If non-negative, skip looking in this abook.
 *
 * Results: A pointer to an AdrBk_Entry is returned, or NULL if not found.
 * Stop at first match (so order of addrbooks is important).
 */
AdrBk_Entry *
adrbk_lookup_with_opens_by_nick(char *nickname, int clearrefs, int *which_addrbook, int not_here)
{
    AdrBk_Entry *abe = (AdrBk_Entry *)NULL;
    int i;
    PerAddrBook *pab;

    dprint((5, "- adrbk_lookup_with_opens_by_nick(%s) -\n",
	nickname ? nickname : "?"));

    for(i = 0; i < as.n_addrbk; i++){

	if(i == not_here)
	  continue;

	pab = &as.adrbks[i];

	if(pab->ostatus != Open && pab->ostatus != NoDisplay)
	  init_abook(pab, NoDisplay);

	if(clearrefs)
	  adrbk_clearrefs(pab->address_book);

	abe = adrbk_lookup_by_nick(pab->address_book,
		    nickname,
		    (adrbk_cntr_t *)NULL);
	if(abe)
	  break;
    }

    if(abe && which_addrbook)
      *which_addrbook = i;

    return(abe);
}


/*
 * Find the addressbook entry that matches the argument address.
 * Searches through all addressbooks looking for the match.
 * Opens addressbooks if necessary.  It is assumed that the caller
 * will restore the state of the addrbooks if desired.
 *
 * Args: addr -- the address we're trying to match
 *
 * Returns:  NULL -- no match found
 *           abe  -- a pointer to the addrbook entry that matches
 */
AdrBk_Entry *
address_to_abe(struct mail_address *addr)
{
    register PerAddrBook *pab;
    int adrbk_number;
    AdrBk_Entry *abe = NULL;
    char *abuf = NULL;

    if(!(addr && addr->mailbox))
      return (AdrBk_Entry *)NULL;

    abuf = (char *)fs_get((MAX_ADDR_FIELD + 1) * sizeof(char));

    strncpy(abuf, addr->mailbox, MAX_ADDR_FIELD);
    abuf[MAX_ADDR_FIELD] = '\0';
    if(addr->host && addr->host[0]){
	strncat(abuf, "@", MAX_ADDR_FIELD+1-1-strlen(abuf));
	strncat(abuf, addr->host, MAX_ADDR_FIELD+1-1-strlen(abuf));
    }

    /* for each addressbook */
    for(adrbk_number = 0; adrbk_number < as.n_addrbk; adrbk_number++){

	pab = &as.adrbks[adrbk_number];

	if(pab->ostatus != Open && pab->ostatus != NoDisplay)
	  init_abook(pab, NoDisplay);

	abe = adrbk_lookup_by_addr(pab->address_book,
				   abuf,
				   (adrbk_cntr_t *)NULL);
	if(abe)
	  break;
    }

    if(abuf)
      fs_give((void **)&abuf);

    return(abe);
}


/*
 * Turn an AdrBk_Entry into an address list
 *
 * Args: abe      -- the AdrBk_Entry
 *       dl       -- the corresponding dl
 *       abook    -- which addrbook the abe is in (only used for type ListEnt)
 *       how_many -- The number of addresses is returned here
 *
 * Result:  allocated address list or NULL
 */
ADDRESS *
abe_to_address(AdrBk_Entry *abe, AddrScrn_Disp *dl, AdrBk *abook, int *how_many)
{
    char          *fullname, *tmp_a_string;
    char          *list, *l1, **l2;
    char          *fakedomain = "@";
    ADDRESS       *addr = NULL;
    size_t         length;
    int            count = 0;

    if(!dl || !abe)
      return(NULL);

    fullname = (abe->fullname && abe->fullname[0]) ? abe->fullname : NULL;

    switch(dl->type){
      case Simple:
	/* rfc822_parse_adrlist feels free to destroy input so send copy */
	tmp_a_string = cpystr(abe->addr.addr);
	rfc822_parse_adrlist(&addr, tmp_a_string, fakedomain);

	if(tmp_a_string)
	  fs_give((void **)&tmp_a_string);

	if(addr && fullname){
#ifdef	ENABLE_LDAP
	    if(strncmp(addr->mailbox, RUN_LDAP, LEN_RL) != 0)
#endif	/* ENABLE_LDAP */
	    {
		if(addr->personal)
		  fs_give((void **)&addr->personal);

		addr->personal = adrbk_formatname(fullname, NULL, NULL);
	    }
	}

	if(addr)
	  count++;

        break;

      case ListEnt:
	/* rfc822_parse_adrlist feels free to destroy input so send copy */
	tmp_a_string = cpystr(listmem_from_dl(abook, dl));
	rfc822_parse_adrlist(&addr, tmp_a_string, fakedomain);
	if(tmp_a_string)
	  fs_give((void **)&tmp_a_string);

	if(addr)
	  count++;

        break;

      case ListHead:
	length = 0;
	for(l2 = abe->addr.list; *l2; l2++)
	  length += (strlen(*l2) + 1);

	list = (char *)fs_get(length + 1);
	l1 = list;
	for(l2 = abe->addr.list; *l2; l2++){
	    if(l1 != list && length-(l1-list) > 0)
	      *l1++ = ',';

	    strncpy(l1, *l2, length-(l1-list));
	    list[length] = '\0';
	    l1 += strlen(l1);
	    count++;
	}

	rfc822_parse_adrlist(&addr, list, fakedomain);
	if(list)
	  fs_give((void **)&list);

        break;
      
      default:
	dprint((1, "default case in abe_to_address, shouldn't happen\n"));
	break;
    } 

    if(how_many)
      *how_many = count;

    return(addr);
}


/*
 * Turn an AdrBk_Entry into a nickname (if it has a nickname) or a
 * formatted addr_string which has been rfc1522 decoded.
 *
 * Args: abe      -- the AdrBk_Entry
 *       dl       -- the corresponding dl
 *       abook    -- which addrbook the abe is in (only used for type ListEnt)
 *
 * Result:  allocated string is returned
 */
char *
abe_to_nick_or_addr_string(AdrBk_Entry *abe, AddrScrn_Disp *dl, int abook_num)
{
    ADDRESS       *addr;
    char          *a_string;

    if(!dl || !abe)
      return(cpystr(""));

    if((dl->type == Simple || dl->type == ListHead)
       && abe->nickname && abe->nickname[0]){
	char *fname;
	int   which_addrbook;

	/*
	 * We prefer to pass back the nickname since that allows the
	 * caller to keep track of which entry the address came from.
	 * This is useful in build_address so that the fcc line can
	 * be kept correct. However, if the nickname is also present in
	 * another addressbook then we have to be careful. If that other
	 * addressbook comes before this one then passing back the nickname
	 * will cause the wrong entry to get used. So check for that
	 * and pass back the addr_string in that case.
	 */
	if((fname = addr_lookup(abe->nickname, &which_addrbook, abook_num)) != NULL){
	    fs_give((void **)&fname);
	    if(which_addrbook >= abook_num)
	      return(cpystr(abe->nickname));
	}
	else
	  return(cpystr(abe->nickname));
    }

    addr = abe_to_address(abe, dl, as.adrbks[abook_num].address_book, NULL);
    a_string = addr_list_string(addr, NULL, 0); /* always returns a string */
    if(addr)
      mail_free_address(&addr);
    
    return(a_string);
}


/*
 * Check to see if address is that of the current user running pine
 *
 * Args: a  -- Address to check
 *       ps -- The pine_state structure
 *
 * Result: returns 1 if it matches, 0 otherwise.
 *
 * The mailbox must match the user name and the hostname must match.
 * In matching the hostname, matches occur if the hostname in the address
 * is blank, or if it matches the local hostname, or the full hostname
 * with qualifying domain, or the qualifying domain without a specific host.
 * Note, there is a very small chance that we will err on the
 * non-conservative side here.  That is, we may decide two addresses are
 * the same even though they are different (because we do case-insensitive
 * compares on the mailbox).  That might cause a reply not to be sent to
 * somebody because they look like they are us.  This should be very,
 * very rare.
 *
 * It is also considered a match if any of the addresses in alt-addresses
 * matches a.  The check there is simpler.  It parses each address in
 * the list, adding maildomain if there wasn't a domain, and compares
 * mailbox and host in the ADDRESS's for equality.
 */
int
address_is_us(struct mail_address *a, struct pine *ps)
{
    char **t;
    int ret;
    char addrstr[500];

    if(!a || a->mailbox == NULL || !ps)
      ret = 0;

    /* at least LHS must match, but case-independent */
    else if(strucmp(a->mailbox, ps->VAR_USER_ID) == 0

                      && /* and hostname matches */

    /* hostname matches if it's not there, */
    (a->host == NULL ||
       /* or if hostname and userdomain (the one user sets) match exactly, */
      ((ps->userdomain && a->host && strucmp(a->host,ps->userdomain) == 0) ||

              /*
               * or if(userdomain is either not set or it is set to be
               * the same as the localdomain or hostname) and (the hostname
               * of the address matches either localdomain or hostname)
               */
             ((ps->userdomain == NULL ||
               strucmp(ps->userdomain, ps->localdomain) == 0 ||
               strucmp(ps->userdomain, ps->hostname) == 0) &&
              (strucmp(a->host, ps->hostname) == 0 ||
               strucmp(a->host, ps->localdomain) == 0)))))
      ret = 1;

    /*
     * If no match yet, check to see if it matches any of the alternate
     * addresses the user has specified.
     */
    else if(!ps_global->VAR_ALT_ADDRS ||
	    !ps_global->VAR_ALT_ADDRS[0] ||
	    !ps_global->VAR_ALT_ADDRS[0][0])
	ret = 0;  /* none defined */

    else{
	ret = 0;
	if(a && a->host && a->mailbox)
	  snprintf(addrstr, sizeof(addrstr), "%s@%s", a->mailbox, a->host);

	for(t = ps_global->VAR_ALT_ADDRS; !ret && t[0] && t[0][0]; t++){
	    char    *alt;
	    regex_t reg;
	    int err;
	    char ebuf[200];

	    alt  = (*t);

	    if(F_ON(F_DISABLE_REGEX, ps_global) || !contains_regex_special_chars(alt)){
		ADDRESS *alt_addr;
		char    *alt2;

		alt2 = cpystr(alt);
		alt_addr = NULL;
		rfc822_parse_adrlist(&alt_addr, alt2, ps_global->maildomain);
		if(alt2)
		  fs_give((void **) &alt2);

		if(address_is_same(a, alt_addr))
		  ret = 1;

		if(alt_addr)
		  mail_free_address(&alt_addr);
	    }
	    else{
		/* treat alt as a regular expression */
		ebuf[0] = '\0';
		if(!(err=regcomp(&reg, alt, REG_ICASE | REG_NOSUB | REG_EXTENDED))){
		    err = regexec(&reg, addrstr, 0, NULL, 0);
		    if(err == 0)
		      ret = 1;
		    else if(err != REG_NOMATCH){
			regerror(err, &reg, ebuf, sizeof(ebuf));
			if(ebuf[0])
			  dprint((2, "- address_is_us regexec error: %s (%s)", ebuf, alt));
		    }

		    regfree(&reg);
		}
		else{
		    regerror(err, &reg, ebuf, sizeof(ebuf));
		    if(ebuf[0])
		      dprint((2, "- address_is_us regcomp error: %s (%s)", ebuf, alt));
		}
	    }
	}
    }

    return(ret);
}


/*
 * In an ad hoc way try to decide if str is meant to be a regular
 * expression or not. Dot doesn't count * as regex stuff because
 * we're worried about addresses.
 *
 * Returns 0 or 1
 */
int
contains_regex_special_chars(char *str)
{
    char special_chars[] = {'*', '|',  '+', '?', '{', '[', '^', '$', '\\',  '\0'};
    char *c;

    if(!str)
      return 0;

    /*
     * If any of special_chars are in str consider it a regex expression.
     */
    for(c = special_chars; *c; c++)
      if(strindex(str, *c))
        return 1;

    return 0;
}


/*
 * Compare the two addresses, and return true if they're the same,
 * false otherwise
 *
 * Args: a -- First address for comparison
 *       b -- Second address for comparison
 *
 * Result: returns 1 if it matches, 0 otherwise.
 */
int
address_is_same(struct mail_address *a, struct mail_address *b)
{
    return(a && b && a->mailbox && b->mailbox && a->host && b->host
	   && strucmp(a->mailbox, b->mailbox) == 0
	   && strucmp(a->host, b->host) == 0);
}


/*
 * Returns nonzero if the two address book entries are equal.
 * Returns zero if not equal.
 */
int
abes_are_equal(AdrBk_Entry *a, AdrBk_Entry *b)
{
    int    result = 0;
    char **alist, **blist;
    char  *addra, *addrb;

    if(a && b &&
       a->tag == b->tag &&
       ((!a->nickname && !b->nickname) ||
	(a->nickname && b->nickname && strcmp(a->nickname,b->nickname) == 0)) &&
       ((!a->fullname && !b->fullname) ||
	(a->fullname && b->fullname && strcmp(a->fullname,b->fullname) == 0)) &&
       ((!a->fcc && !b->fcc) ||
	(a->fcc && b->fcc && strcmp(a->fcc,b->fcc) == 0)) &&
       ((!a->extra && !b->extra) ||
	(a->extra && b->extra && strcmp(a->extra,b->extra) == 0))){

	/* If we made it in here, they still might be equal. */
	if(a->tag == Single)
	  result = strcmp(a->addr.addr,b->addr.addr) == 0;
	else{
	    alist = a->addr.list;
	    blist = b->addr.list;
	    if(!alist && !blist)
	      result = 1;
	    else{
		/* compare the whole lists */
		addra = *alist;
		addrb = *blist;
		while(addra && addrb && strcmp(addra,addrb) == 0){
		    alist++;
		    blist++;
		    addra = *alist;
		    addrb = *blist;
		}

		if(!addra && !addrb)
		  result = 1;
	    }
	}
    }

    return(result);
}


/*
 *  Interface to address book lookups for callers outside or inside this file.
 *
 * Args: nickname       -- The nickname to look up
 *       which_addrbook -- If matched, addrbook number it was found in.
 *       not_here       -- If non-negative, skip looking in this abook.
 *
 * Result: returns NULL or the corresponding fullname.  The fullname is
 * allocated here so the caller must free it.
 *
 * This opens the address books if they haven't been opened and restores
 * them to the state they were in upon entry.
 */
char *
addr_lookup(char *nickname, int *which_addrbook, int not_here)
{
    AdrBk_Entry  *abe;
    SAVE_STATE_S  state;
    char         *fullname;

    dprint((9, "- addr_lookup(%s) -\n",nickname ? nickname : "nil"));

    init_ab_if_needed();

    if(pith_opt_save_and_restore)
      (*pith_opt_save_and_restore)(SAR_SAVE, &state);

    abe = adrbk_lookup_with_opens_by_nick(nickname,0,which_addrbook,not_here);

    fullname = (abe && abe->fullname) ? cpystr(abe->fullname) : NULL;

    if(pith_opt_save_and_restore)
      (*pith_opt_save_and_restore)(SAR_RESTORE, &state);

    return(fullname);
}


/*
 * Look in all of the address books for all of the possible entries
 * that match the query string. The matches can be for the nickname,
 * for the fullname, or for the address@host part of the address.
 * All of the matches are at the starts of the strings, not a general
 * substring match. This is not true anymore. Fullname matches can be
 * at the start of the fullname or starting after a space in the fullname.
 * If flags has ALC_INCLUDE_LDAP defined then LDAP
 * entries are added to the end of the list. The LDAP queries are done
 * only for those servers that have the 'impl' feature turned on, which
 * means that lookups should be done implicitly. This feature also
 * controls whether or not lookups should be done when typing carriage
 * return (instead of this which is TAB).
 *
 * Args     query  -- What the user has typed so far
 *
 * Returns a list of possibilities for the given query string.
 *
 * Caller needs to free the answer.
 */
COMPLETE_S *
adrbk_list_of_completions(char *query, MAILSTREAM *stream, imapuid_t uid, int flags)
{
    int i;
    SAVE_STATE_S  state;
    PerAddrBook  *pab;
    ABOOK_ENTRY_S *list, *list2, *biglist = NULL;
    COMPLETE_S *return_list = NULL, *last_one_added = NULL, *new, *cp, *dp, *dprev;
    BuildTo toaddr;
    ADDRESS *addr;
    char buf[1000];
    char *newaddr = NULL, *simple_addr = NULL, *fcc = NULL;
    ENVELOPE *env = NULL;
    BODY *body = NULL;

    init_ab_if_needed();

    if(pith_opt_save_and_restore)
      (*pith_opt_save_and_restore)(SAR_SAVE, &state);

    for(i = 0; i < as.n_addrbk; i++){

	pab = &as.adrbks[i];

	if(pab->ostatus != Open && pab->ostatus != NoDisplay)
	  init_abook(pab, NoDisplay);

	list = adrbk_list_of_possible_completions(pab ? pab->address_book : NULL, query);
	combine_abook_entry_lists(&biglist, list);
    }

    /*
     * Eliminate duplicates by NO_NEXTing the entrynums.
     */
    for(list = biglist; list; list = list->next)
      /* eliminate any dups further along in the list */
      if(list->entrynum != NO_NEXT)
	for(list2 = list->next; list2; list2 = list2->next)
	  if(list2->entrynum ==  list->entrynum){
	      list2->entrynum = NO_NEXT;
	      list->matches_bitmap |= list2->matches_bitmap;
	  }

    /* build the return list */
    for(list = biglist; list; list = list->next)
      if(list->entrynum != NO_NEXT){
	  fcc = NULL;
	  toaddr.type = Abe;
	  toaddr.arg.abe = adrbk_get_ae(list->ab, list->entrynum);
	  if(our_build_address(toaddr, &newaddr, NULL, &fcc, NULL) == 0){
	      char    *reverse_fullname = NULL;

	      /*
	       * ALC_FULL is a regular FullName match and that will be
	       * captured in the full_address field. If there was also
	       * an ALC_REVFULL match that means that the user has the
	       * FullName entered in their addrbook as Last, First and
	       * that is where the match was. We want to put that in
	       * the completions structure in the rev_fullname field.
	       */
	      if(list->matches_bitmap & ALC_REVFULL
	         && toaddr.arg.abe
		 && toaddr.arg.abe->fullname && toaddr.arg.abe->fullname[0]
		 && toaddr.arg.abe->fullname[0] != '"'
		 && strindex(toaddr.arg.abe->fullname, ',') != NULL){

		  reverse_fullname = toaddr.arg.abe->fullname;
	      }

	      if(flags & ALC_INCLUDE_ADDRS){
		  if(toaddr.arg.abe && toaddr.arg.abe->tag == Single
		     && toaddr.arg.abe->addr.addr && toaddr.arg.abe->addr.addr[0]){
		      char    *tmp_a_string;
		      char    *fakedomain = "@";

		      tmp_a_string = cpystr(toaddr.arg.abe->addr.addr);
		      addr = NULL;
		      rfc822_parse_adrlist(&addr, tmp_a_string, fakedomain);
		      if(tmp_a_string)
			fs_give((void **) &tmp_a_string);
		     
		      if(addr){
			  if(addr->mailbox && addr->host
			     && !(addr->host[0] == '@' && addr->host[1] == '\0'))
			    simple_addr = simple_addr_string(addr, buf, sizeof(buf));

			  mail_free_address(&addr);
		      }
		  }
	      }

	      new = new_complete_s(toaddr.arg.abe ? toaddr.arg.abe->nickname : NULL,
				   newaddr, simple_addr, reverse_fullname, fcc,
				   list->matches_bitmap | ALC_ABOOK);

	      /* add to end of list */
	      if(return_list == NULL){
		  return_list = new;
		  last_one_added = new;
	      }
	      else{
		  last_one_added->next = new;
		  last_one_added = new;
	      }
	  }

	  if(newaddr)
	    fs_give((void **) &newaddr);
      }

    if(pith_opt_save_and_restore)
      (*pith_opt_save_and_restore)(SAR_RESTORE, &state);

    free_abook_entry_s(&biglist);

#ifdef	ENABLE_LDAP
    if(flags & ALC_INCLUDE_LDAP){
	LDAP_SERV_RES_S *head_of_result_list = NULL, *res;
	LDAP_CHOOSE_S cs;
	WP_ERR_S wp_err;
	LDAPMessage *e;

	memset(&wp_err, 0, sizeof(wp_err));

	/*
	 * This lookup covers all servers with the impl bit set.
	 * It uses the regular LDAP search parameters that the
	 * user has set, not necessarily just a prefix match
	 * like the rest of the address completion above.
	 */
	head_of_result_list = ldap_lookup_all_work(query, as.n_serv, 0, NULL, &wp_err);
	for(res = head_of_result_list; res; res = res->next){
	  for(e = ldap_first_entry(res->ld, res->res);
	      e != NULL;
	      e = ldap_next_entry(res->ld, e)){
	    simple_addr = newaddr = NULL;
	    cs.ld = res->ld;
	    cs.selected_entry = e;
	    cs.info_used = res->info_used;
	    cs.serv = res->serv;
	    addr = address_from_ldap(&cs);
	    if(addr){
		add_addr_to_return_list(addr, ALC_LDAP, query, flags, &return_list);
		mail_free_address(&addr);
	    }
	  }
	}

	if(wp_err.error)
	  fs_give((void **) &wp_err.error);

	if(head_of_result_list)
	  free_ldap_result_list(&head_of_result_list);
    }
#endif	/* ENABLE_LDAP */

    /* add from current message */
    if(uid > 0 && stream)
      env = pine_mail_fetch_structure(stream, uid, &body, FT_UID);

    /* from the envelope addresses */
    if(env){
	for(addr = env->from; addr; addr = addr->next)
	  add_addr_to_return_list(addr, ALC_CURR, query, flags, &return_list);

	for(addr = env->reply_to; addr; addr = addr->next)
	  add_addr_to_return_list(addr, ALC_CURR, query, flags, &return_list);

	for(addr = env->sender; addr; addr = addr->next)
	  add_addr_to_return_list(addr, ALC_CURR, query, flags, &return_list);

	for(addr = env->to; addr; addr = addr->next)
	  add_addr_to_return_list(addr, ALC_CURR, query, flags, &return_list);

	for(addr = env->cc; addr; addr = addr->next)
	  add_addr_to_return_list(addr, ALC_CURR, query, flags, &return_list);

	/*
	 * May as well search the body for addresses.
	 * Use this function written for TakeAddr.
	 */
	if(body){
	    TA_S *talist = NULL, *tp;

	    if(grab_addrs_from_body(stream, mail_msgno(stream,uid), body, &talist) > 0){
		if(talist){

		    /* rewind to start */
		    while(talist->prev)
		      talist = talist->prev;

		    for(tp = talist; tp; tp = tp->next){
			addr = tp->addr;
			if(addr)
			  add_addr_to_return_list(addr, ALC_CURR, query, flags, &return_list);
		    }

		    free_talines(&talist);
		}
	    }
	}
    }

    
    /*
     * Check for and eliminate some duplicates.
     * The criteria for deciding what is a duplicate is
     * kind of ad hoc.
     */
    for(cp = return_list; cp; cp = cp->next)
      for(dprev = cp, dp = cp->next; dp; ){
        if(cp->full_address && dp->full_address
	   && !strucmp(dp->full_address, cp->full_address)
	   && (((cp->matches_bitmap & ALC_ABOOK)
	        && (dp->matches_bitmap & ALC_ABOOK)
	        && (!(dp->matches_bitmap & ALC_NICK && dp->nickname && dp->nickname[0])
		    || ((!cp->addr && !dp->addr) || (cp->addr && dp->addr && !strucmp(cp->addr, dp->addr)))))
	       ||
	       (dp->matches_bitmap & ALC_CURR)
	       ||
	       (dp->matches_bitmap & ALC_LDAP
	        && dp->matches_bitmap & (ALC_FULL | ALC_ADDR))
	       ||
	       (cp->matches_bitmap == dp->matches_bitmap
	        && (!(dp->matches_bitmap & ALC_NICK && dp->nickname && dp->nickname[0])
		    || (dp->nickname && cp->nickname
		        && !strucmp(cp->nickname, dp->nickname)))))){
	    /*
	     * dp is equivalent to cp so eliminate dp
	     */
	    dprev->next = dp->next;
	    dp->next = NULL;
	    free_complete_s(&dp);
	    dp = dprev;
	}

	dprev = dp;
	dp = dp->next;
      }

    return(return_list);
}


void
add_addr_to_return_list(ADDRESS *addr, unsigned bitmap, char *query,
			int flags, COMPLETE_S **return_list)
{
    char buf[1000];
    char *newaddr = NULL;
    char *simple_addr = NULL;
    COMPLETE_S *new = NULL, *cp;
    ADDRESS *savenext;

    if(return_list && query && addr && addr->mailbox && addr->host){

	savenext = addr->next;
	addr->next = NULL;
	newaddr = addr_list_string(addr, NULL, 0);
	addr->next = savenext;

	/*
	 * If the start of the full_address actually matches the query
	 * string then mark this as ALC_FULL. This might be helpful
	 * when deciding on the longest unambiguous match.
	 */
	if(newaddr && newaddr[0] && !struncmp(newaddr, query, strlen(query)))
	  bitmap |= ALC_FULL;

	if(newaddr && newaddr[0] && flags & ALC_INCLUDE_ADDRS){
	    if(addr->mailbox && addr->host
	       && !(addr->host[0] == '@' && addr->host[1] == '\0'))
	      simple_addr = simple_addr_string(addr, buf, sizeof(buf));

	    if(simple_addr && !simple_addr[0])
	      simple_addr = NULL;

	    if(simple_addr && !struncmp(simple_addr, query, strlen(query)))
	      bitmap |= ALC_ADDR;
	}

	/*
	 * We used to require && bitmap & (ALC_FULL | ALC_ADDR) before
	 * we would add a match but we think that we should match
	 * other stuff that matches (like middle names), too, unless we
	 * are adding an address from the current message.
	 */
	if((newaddr && newaddr[0])
	   && (!(bitmap & ALC_CURR) || bitmap & (ALC_FULL | ALC_ADDR))){
	    new = new_complete_s(NULL, newaddr, simple_addr, NULL, NULL, bitmap);

	    /* add to end of list */
	    if(*return_list == NULL){
		*return_list = new;
	    }
	    else{
		for(cp = *return_list; cp->next; cp = cp->next)
		  ;

		cp->next = new;
	    }
	}

	if(newaddr)
	  fs_give((void **) &newaddr);
    }
}


/*
 * nick = nickname
 * full = whole thing, like Some Body <someb@there.org>
 * addr = address part, like someb@there.org
 */
COMPLETE_S *
new_complete_s(char *nick, char *full, char *addr,
	       char *rev_fullname, char *fcc, unsigned matches_bitmap)
{
    COMPLETE_S *new = NULL;

    new = (COMPLETE_S *) fs_get(sizeof(*new));
    memset((void *) new, 0, sizeof(*new));
    new->nickname = nick ? cpystr(nick) : NULL;
    new->full_address = full ? cpystr(full) : NULL;
    new->addr = addr ? cpystr(addr) : NULL;
    new->rev_fullname = rev_fullname ? cpystr(rev_fullname) : NULL;
    new->fcc = fcc ? cpystr(fcc) : NULL;
    new->matches_bitmap = matches_bitmap;

    return(new);
}


void
free_complete_s(COMPLETE_S **compptr)
{
    if(compptr && *compptr){
	if((*compptr)->next)
	  free_complete_s(&(*compptr)->next);

	if((*compptr)->nickname)
	  fs_give((void **) &(*compptr)->nickname);

	if((*compptr)->full_address)
	  fs_give((void **) &(*compptr)->full_address);

	if((*compptr)->addr)
	  fs_give((void **) &(*compptr)->addr);

	if((*compptr)->rev_fullname)
	  fs_give((void **) &(*compptr)->rev_fullname);

	if((*compptr)->fcc)
	  fs_give((void **) &(*compptr)->fcc);

	fs_give((void **) compptr);
    }
}


ABOOK_ENTRY_S *
new_abook_entry_s(AdrBk *ab, a_c_arg_t numarg, unsigned bit)
{
    ABOOK_ENTRY_S *new = NULL;
    adrbk_cntr_t entrynum;

    entrynum = (adrbk_cntr_t) numarg;

    new = (ABOOK_ENTRY_S *) fs_get(sizeof(*new));
    memset((void *) new, 0, sizeof(*new));
    new->ab = ab;
    new->entrynum = entrynum;
    new->matches_bitmap = bit;

    return(new);
}


void
free_abook_entry_s(ABOOK_ENTRY_S **aep)
{
    if(aep && *aep){
	if((*aep)->next)
	  free_abook_entry_s(&(*aep)->next);

	fs_give((void **) aep);
    }
}


/*
 * Add the second list to the end of the first.
 */
void
combine_abook_entry_lists(ABOOK_ENTRY_S **first, ABOOK_ENTRY_S *second)
{
    ABOOK_ENTRY_S *sl;

    if(!second)
      return;

    if(first){
	if(*first){
	    for(sl = *first; sl->next; sl = sl->next)
	      ;

	    sl->next = second;
	}
	else
	  *first = second;
    }
}


ABOOK_ENTRY_S *
adrbk_list_of_possible_completions(AdrBk *ab, char *prefix)
{
    ABOOK_ENTRY_S *list = NULL, *biglist = NULL;

    if(!ab || !prefix)
      return(biglist);

    if(ab->nick_trie){
	list = adrbk_list_of_possible_trie_completions(ab->nick_trie, ab, prefix, ALC_NICK);
	combine_abook_entry_lists(&biglist, list);
    }

    if(ab->full_trie){
	list = adrbk_list_of_possible_trie_completions(ab->full_trie, ab, prefix, ALC_FULL);
	combine_abook_entry_lists(&biglist, list);
    }

    if(ab->addr_trie){
	list = adrbk_list_of_possible_trie_completions(ab->addr_trie, ab, prefix, ALC_ADDR);
	combine_abook_entry_lists(&biglist, list);
    }

    if(ab->revfull_trie){
	list = adrbk_list_of_possible_trie_completions(ab->revfull_trie, ab, prefix, ALC_REVFULL);
	combine_abook_entry_lists(&biglist, list);
    }

    return(biglist);
}


/*
 * Look in this address book for all nicknames, addresses, or fullnames
 * which begin with the prefix prefix, and return an allocated
 * list of them.
 */
ABOOK_ENTRY_S *
adrbk_list_of_possible_trie_completions(AdrBk_Trie *trie, AdrBk *ab, char *prefix,
					unsigned bit)
{
    AdrBk_Trie *t;
    char        *p, *lookthisup;
    char         buf[1000];
    ABOOK_ENTRY_S *list = NULL;

    if(!ab || !prefix || !trie)
      return(list);

    t = trie;

    /* make lookup case independent */

    for(p = prefix; *p && !(*p & 0x80) && islower((unsigned char) *p); p++)
      ;

    if(*p){
	strncpy(buf, prefix, sizeof(buf));
	buf[sizeof(buf)-1] = '\0';
	for(p = buf; *p; p++)
	  if(!(*p & 0x80) && isupper((unsigned char) *p))
	    *p = tolower(*p);

	lookthisup = buf;
    }
    else
      lookthisup = prefix;

    p = lookthisup;

    while(*p){
	/* search for character at this level */
	while(t->value != *p){
	    if(t->right == NULL)
	      return(list);		/* no match */

	    t = t->right;
	}

	if(*++p == '\0')		/* matched through end of prefix */
	  break;

	/* need to go down to match next character */
	if(t->down == NULL)		/* no match */
	  return(list);

	t = t->down;
    }

    /*
     * If we get here that means we found at least
     * one entry that matches up through prefix.
     * Gather_abook_list recursively adds the nicknames starting at
     * this node.
     */
    if(t->entrynum != NO_NEXT){
	/*
	 * Add it to the list.
	 */
	list = new_abook_entry_s(ab, t->entrynum, bit);
    }

    gather_abook_entry_list(ab, t->down, prefix, &list, bit);

    return(list);
}


void
gather_abook_entry_list(AdrBk *ab, AdrBk_Trie *node, char *prefix, ABOOK_ENTRY_S **list, unsigned bit)
{
  char *next_prefix = NULL;
  size_t l;
  ABOOK_ENTRY_S *newlist = NULL;

  if(node){
    if(node->entrynum != NO_NEXT || node->down || node->right){
      l = strlen(prefix ? prefix : "");
      if(node->entrynum != NO_NEXT){
	/*
	 * Add it to the list.
	 */
	newlist = new_abook_entry_s(ab, node->entrynum, bit);
	combine_abook_entry_lists(list, newlist);
      }

      /* same prefix for node->right */
      if(node->right)
        gather_abook_entry_list(ab, node->right, prefix, list, bit);

      /* prefix is one longer for node->down */
      if(node->down){
	next_prefix = (char *) fs_get((l+2) * sizeof(char));
	strncpy(next_prefix, prefix ? prefix : "", l+2);
	next_prefix[l] = node->value;
	next_prefix[l+1] = '\0';
	gather_abook_entry_list(ab, node->down, next_prefix, list, bit);

	if(next_prefix)
	  fs_give((void **) &next_prefix);
      }
    }
  }
}
