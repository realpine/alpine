#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: ablookup.c 406 2007-01-31 00:36:05Z hubert@u.washington.edu $";
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


/*
 * Internal prototypes
 */
int	       addr_is_in_addrbook(PerAddrBook *, ADDRESS *);


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
 * This is a very special-purpose routine.
 * It implements the From or Reply-To address is in the Address Book
 * part of Pattern matching.
 */
void
address_in_abook(MAILSTREAM *stream, struct search_set *searchset,
			 int inabook, PATTERN_S *abooks)
{
    char         *savebits;
    MESSAGECACHE *mc;
    long          i;
    SEARCHSET    *s, *ss, **sset;
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
	   && (mc=mail_elt(stream, i)) && mc->searched)
	  mc->sequence = 1;

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

    for(s = ss; s; s = s->next){
	for(i = s->first; i <= s->last; i++){
	    if(i <= 0L || i > stream->nmsgs)
	      continue;

	    /*
	     * This causes the lookahead to fetch precisely
	     * the messages we want (in the searchset) instead
	     * of just fetching the next 20 sequential
	     * messages. If the searching so far has caused
	     * a sparse searchset in a large mailbox, the
	     * difference can be substantial.
	     */
	    sset = (SEARCHSET **) mail_parameters(stream,
						  GET_FETCHLOOKAHEAD,
						  (void *) stream);
	    if(sset)
	      *sset = s;

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
	strncpy(abuf, addr->mailbox, MAX_ADDR_FIELD);
	abuf[MAX_ADDR_FIELD] = '\0';
	if(addr->host && addr->host[0]){
	    strncat(abuf, "@", MAX_ADDR_FIELD-strlen(abuf));
	    strncat(abuf, addr->host, MAX_ADDR_FIELD-strlen(abuf));
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

    abuf = (char *)fs_get((size_t)(MAX_ADDR_FIELD + 1));

    strncpy(abuf, addr->mailbox, MAX_ADDR_FIELD);
    abuf[MAX_ADDR_FIELD] = '\0';
    if(addr->host && addr->host[0]){
	strncat(abuf, "@", MAX_ADDR_FIELD-strlen(abuf));
	strncat(abuf, addr->host, MAX_ADDR_FIELD-strlen(abuf));
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
	if(fname = addr_lookup(abe->nickname, &which_addrbook, abook_num)){
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

    if(!a || a->mailbox == NULL)
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
	for(t = ps_global->VAR_ALT_ADDRS; t[0] && t[0][0]; t++){
	    ADDRESS *alt_addr;
	    char    *alt;

	    alt  = cpystr(*t);
	    alt_addr = NULL;
	    rfc822_parse_adrlist(&alt_addr, alt, ps_global->maildomain);
	    if(alt)
	      fs_give((void **)&alt);

	    if(address_is_same(a, alt_addr)){
		if(alt_addr)
		  mail_free_address(&alt_addr);

		ret = 1;
		break;
	    }

	    if(alt_addr)
	      mail_free_address(&alt_addr);
	}
    }

    return(ret);
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
       (!a->nickname && !b->nickname ||
	a->nickname && b->nickname && strcmp(a->nickname,b->nickname) == 0) &&
       (!a->fullname && !b->fullname ||
	a->fullname && b->fullname && strcmp(a->fullname,b->fullname) == 0) &&
       (!a->fcc && !b->fcc ||
	a->fcc && b->fcc && strcmp(a->fcc,b->fcc) == 0) &&
       (!a->extra && !b->extra ||
	a->extra && b->extra && strcmp(a->extra,b->extra) == 0)){

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
 * Look in all of the address books for the longest unambiguous prefix
 * of a nickname which begins with the characters in "prefix".
 *
 * Args    prefix  -- The part of the nickname that has been typed so far
 *         answer  -- The answer is returned here.
 *         flags   -- ANC_AFTERCOMMA -- This means that the passed in
 *                                      prefix may be a list of comma
 *                                      separated addresses and we're only
 *                                      completing the last one.
 *
 * Returns    0 -- no nickname has prefix as a prefix
 *            1  -- more than one nickname begins with
 *                              the answer being returned
 *            2 -- the returned answer is a complete
 *                              nickname and there are no longer nicknames
 *                              which begin with the same characters
 *
 * Allocated answer is returned in answer argument.
 * Caller needs to free the answer.
 */
int
adrbk_nick_complete(char *prefix, char **answer, unsigned flags)
{
    int i, j, k, longest_match, prefixlen, done;
    int answerlen, candidate_kth_char, l;
    int ambiguity = 0;
    char *saved_beginning = NULL;
    char *ans = NULL;
    size_t answer_allocated;
    SAVE_STATE_S  state;
    PerAddrBook  *pab;
    struct unambig_s {
	char *name;
	int   unambig;
    };
    struct unambig_s *unambig;

    if(answer)
      *answer = NULL;

    if(flags & ANC_AFTERCOMMA){
	char *lastnick;

	/*
	 * Find last comma, save the part before that, operate
	 * only on the last address.
	 */
	if((lastnick = strrchr(prefix ? prefix : "", ',')) != NULL){
	    lastnick++;
	    while(!(*lastnick & 0x80) && isspace((unsigned char) (*lastnick)))
	      lastnick++;

	    saved_beginning = cpystr(prefix);
	    saved_beginning[lastnick-prefix] = '\0';
	    prefix = lastnick;
	}
    }

    /*
     * Loop through the abooks looking for the longest unambiguous match.
     */

    init_ab_if_needed();

    unambig = (struct unambig_s *) fs_get((as.n_addrbk + 1) * (sizeof(struct unambig_s)));
    memset(unambig, 0, (as.n_addrbk + 1) * (sizeof(struct unambig_s)));

    if(pith_opt_save_and_restore)
      (*pith_opt_save_and_restore)(SAR_SAVE, &state);

    for(i = 0; i < as.n_addrbk; i++){

	pab = &as.adrbks[i];

	if(pab->ostatus != Open && pab->ostatus != NoDisplay)
	  init_abook(pab, NoDisplay);

	unambig[i].unambig = adrbk_longest_unambig_nick(pab->address_book,
							prefix, &unambig[i].name);
    }

    unambig[i].name = NULL;

    if(pith_opt_save_and_restore)
      (*pith_opt_save_and_restore)(SAR_RESTORE, &state);

    prefixlen = strlen(prefix ? prefix : "");

    /*
     * Find the longest unambiguous of all of
     * the longest unambiguous nicknames, if you know
     * what I mean.
     */
    longest_match = 0;
    for(i = 0; i < as.n_addrbk; i++)
      if(unambig[i].name)
	longest_match = MAX(longest_match, strlen(unambig[i].name));

    if(longest_match < prefixlen){
	for(i = 0; i < as.n_addrbk; i++)
	  if(unambig[i].name)
	    fs_give((void **) &unambig[i].name);
	
	fs_give((void **) &unambig);
	return(ambiguity);
    }
    else if(longest_match == prefixlen){
	ans = cpystr(prefix);
    }
    else{
	answer_allocated = prefixlen + 100;
	ans = (char *) fs_get(answer_allocated * sizeof(char));
	strncpy(ans, prefix, prefixlen);
	ans[prefixlen] = '\0';

	/*
	 * If we get here we know that at least one of the matches
	 * is longer than the prefix, so we need to check to see what
	 * the longest unambiguous match is.
	 */

	for(k = prefixlen, done = 0; !done; k++){
	    candidate_kth_char = -1;
	    for(i = 0; !done && i < as.n_addrbk; i++){
		if(k > 0 && unambig[i].name && unambig[i].name[k-1] == '\0')
		  unambig[i].name[0] = '\0';			/* mark this one done */

		if(unambig[i].name && unambig[i].name[0] != '\0'){
		    if(candidate_kth_char == -1){		/* no candidate yet */
			candidate_kth_char = unambig[i].name[k];
		    }
		    else{
			if(unambig[i].name[k] != candidate_kth_char){
			    done++;
			}
		    }
		}
	    }

	    if(!done){
		if(candidate_kth_char == '\0')
		  done++;
		else{
		    if(answer_allocated < k+2){
			answer_allocated += 100;
			fs_resize((void **) &ans, answer_allocated);
		    }

		    ans[k] = candidate_kth_char;
		    ans[k+1] = '\0';
		}
	    }
	}
    }

    k = 0;
    if(ans)
      k = strlen(ans);

    ambiguity = 2;	/* start off with unambiguous */

    /* determine if answer is ambiguous or not */
    for(i = 0; ambiguity == 2 && i < as.n_addrbk; i++){
	if(unambig[i].name
	   && (l=strlen(unambig[i].name)) >= k
	   && (l > k || unambig[i].unambig == 1))
	  ambiguity = 1;
    }

    for(i = 0; i < as.n_addrbk; i++)
      if(unambig[i].name)
	fs_give((void **) &unambig[i].name);
    
    fs_give((void **) &unambig);

    if(answer){
	size_t l1, l2;

	if(saved_beginning){
	    l1 = strlen(saved_beginning);
	    l2 = strlen(ans ? ans : "");
	    *answer = (char *) fs_get((l1+l2+1) * sizeof(char));
	    strncpy(*answer, saved_beginning, l1+l2);
	    strncpy(*answer+l1, ans ? ans : "", l2);
	    (*answer)[l1+l2] = '\0';
	    fs_give((void **) &saved_beginning);
	    if(ans)
	      fs_give((void **) &ans);
	}
	else{
	    fs_resize((void **) &ans, strlen(ans)+1);
	    *answer = ans;
	}
    }

    return(ambiguity);
}
