#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: bldaddr.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
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

/*====================================================================== 
    buildaddr.c
    Support for build_address function and low-level display cache.
 ====*/


#include "../pith/headers.h"
#include "../pith/bldaddr.h"
#include "../pith/ldap.h"
#include "../pith/conf.h"
#include "../pith/adrbklib.h"
#include "../pith/copyaddr.h"
#include "../pith/remote.h"
#include "../pith/status.h"
#include "../pith/search.h"
#include "../pith/addrstring.h"
#include "../pith/util.h"
#include "../pith/ablookup.h"
#include "../pith/news.h"
#include "../pith/stream.h"
#include "../pith/mailcmd.h"
#include "../pith/osdep/pw_stuff.h"


/*
 * Jump back to this location if we discover that one of the open addrbooks
 * has been changed by some other process.
 */
jmp_buf addrbook_changed_unexpectedly;


/*
 * Sometimes the address book code calls something outside of the address
 * book and that calls back to the address book. For example, you could be
 * in the address book mgmt screen, call ab_compose, and then a ^T could
 * call back to an address book select screen. Or moving out of a line
 * calls back to build_address. This keeps track of the nesting level so
 * that we can know when it is safe to update an out of date address book.
 * For example, if we know an address book is open and displayed, it isn't
 * safe to update it because that would pull the cache out from under the
 * displayed lines. If we don't have it displayed, then it is ok to close
 * it and re-open it (adrbk_check_and_fix()).
 */
int         ab_nesting_level = 0;



/*
 * This is like build_address() only it doesn't close
 * everything down when it is done, and it doesn't open addrbooks that
 * are already open.  Other than that, it has the same functionality.
 * It opens addrbooks that haven't been opened and saves and restores the
 * addrbooks open states (if save_and_restore is set).
 *
 * Args: to                    -- the address to attempt expanding (see the
 *				   description in expand_address)
 *       full_to               -- a pointer to result
 *		    This will be allocated here and freed by the caller.
 *       error                 -- a pointer to an error message, if non-null
 *       fcc                   -- a pointer to returned fcc, if non-null
 *		    This will be allocated here and freed by the caller.
 *		    *fcc should be null on entry.
 *       save_and_restore      -- restore addrbook states when finished
 *
 * Results:    0 -- address is ok
 *            -1 -- address is not ok
 * full_to contains the expanded address on success, or a copy of to
 *         on failure
 * *error  will point to an error message on failure it it is non-null
 *
 * Side effect: Can flush addrbook entry cache entries so they need to be
 * re-fetched afterwords.
 */
int
our_build_address(BuildTo to, char **full_to, char **error, char **fcc,
		  void (*save_and_restore_f)(int, SAVE_STATE_S *))
{
    int ret;

    dprint((7, "- our_build_address -  (%s)\n",
	(to.type == Str) ? (to.arg.str ? to.arg.str : "nul")
			 : (to.arg.abe->nickname ? to.arg.abe->nickname
						: "no nick")));

    if((to.type == Str && !to.arg.str) || (to.type == Abe && !to.arg.abe)){
	if(full_to)
	  *full_to = cpystr("");
	ret = 0;
    }
    else
      ret = build_address_internal(to, full_to, error, fcc, NULL, NULL,
				   save_and_restore_f, 0, NULL);

    dprint((8, "   our_build_address says %s address\n",
	ret ? "BAD" : "GOOD"));

    return(ret);
}



#define FCC_SET    1	/* Fcc was set */
#define LCC_SET    2	/* Lcc was set */
#define FCC_NOREPO 4	/* Fcc was set in a non-reproducible way */
#define LCC_NOREPO 8	/* Lcc was set in a non-reproducible way */
/*
 * Given an address, expand it based on address books, local domain, etc.
 * This will open addrbooks if needed before checking (actually one of
 * its children will open them).
 *
 * Args: to       -- The given address to expand (see the description
 *			in expand_address)
 *       full_to  -- Returned value after parsing to.
 *       error    -- This gets pointed at error message, if any
 *       fcc      -- Returned value of fcc for first addr in to
 *       no_repo  -- Returned value, set to 1 if the fcc or lcc we're
 *			returning is not reproducible from the expanded
 *			address.  That is, if we were to run
 *			build_address_internal again on the resulting full_to,
 *			we wouldn't get back the fcc again.  For example,
 *			if we expand a list and use the list fcc from the
 *			addrbook, the full_to no longer contains the
 *			information that this was originally list foo.
 *       save_and_restore -- restore addrbook state when done
 *
 * Result:  0 is returned if address was OK, 
 *         -1 if address wasn't OK.
 * The address is expanded, fully-qualified, and personal name added.
 *
 * Input may have more than one address separated by commas.
 *
 * Side effect: Can flush addrbook entry cache entries so they need to be
 * re-fetched afterwords.
 */
int
build_address_internal(BuildTo to, char **full_to, char **error, char **fcc,
		       int *no_repo, char **lcc,
		       void (*save_and_restore_f)(int, SAVE_STATE_S *),
		       int simple_verify, int *mangled)
{
    ADDRESS *a;
    int      loop, i;
    int      tried_route_addr_hack = 0;
    int      did_set = 0;
    char    *tmp = NULL;
    SAVE_STATE_S  state;
    PerAddrBook  *pab;

    dprint((8, "- build_address_internal -  (%s)\n",
	(to.type == Str) ? (to.arg.str ? to.arg.str : "nul")
			 : (to.arg.abe->nickname ? to.arg.abe->nickname
						: "no nick")));

    init_ab_if_needed();
    if(save_and_restore_f)
      (*save_and_restore_f)(SAR_SAVE, &state);

start:
    loop = 0;
    ps_global->c_client_error[0] = '\0';
    wp_exit = wp_nobail = 0;

    a = expand_address(to, ps_global->maildomain,
		       F_OFF(F_QUELL_LOCAL_LOOKUP, ps_global)
			 ? ps_global->maildomain : NULL,
		       &loop, fcc, &did_set, lcc, error,
		       0, simple_verify, mangled);

    /*
     * If the address is a route-addr, expand_address() will have rejected
     * it unless it was enclosed in brackets, since route-addrs can't stand
     * alone.  Try it again with brackets.  We should really be checking
     * each address in the list of addresses instead of assuming there is
     * only one address, but we don't want to have this function know
     * all about parsing rfc822 addrs.
     */
    if(!tried_route_addr_hack &&
        ps_global->c_client_error[0] != '\0' &&
	((to.type == Str && to.arg.str && to.arg.str[0] == '@') ||
	 (to.type == Abe && to.arg.abe->tag == Single &&
				 to.arg.abe->addr.addr[0] == '@'))){
	BuildTo      bldto;

	tried_route_addr_hack++;

	tmp = (char *)fs_get((size_t)(MAX_ADDR_FIELD + 3));

	/* add brackets to whole thing */
	strncpy(tmp, "<", MAX_ADDR_FIELD+3);
	tmp[MAX_ADDR_FIELD+3-1] = '\0';
	if(to.type == Str)
	  strncat(tmp, to.arg.str, MAX_ADDR_FIELD+3-strlen(tmp)-1);
	else
	  strncat(tmp, to.arg.abe->addr.addr, MAX_ADDR_FIELD+3-strlen(tmp)-1);

	tmp[MAX_ADDR_FIELD+3-1] = '\0';
	strncat(tmp, ">", MAX_ADDR_FIELD+3-strlen(tmp)-1);
	tmp[MAX_ADDR_FIELD+3-1] = '\0';

	loop = 0;
	ps_global->c_client_error[0] = '\0';

	bldto.type    = Str;
	bldto.arg.str = tmp;

	if(a)
	  mail_free_address(&a);

	/* try it */
	a = expand_address(bldto, ps_global->maildomain,
			   F_OFF(F_QUELL_LOCAL_LOOKUP, ps_global)
			     ? ps_global->maildomain : NULL,
		       &loop, fcc, &did_set, lcc, error,
		       0, simple_verify, mangled);

	/* if no error this time, use it */
	if(ps_global->c_client_error[0] == '\0'){
	    if(save_and_restore_f)
	      (*save_and_restore_f)(SAR_RESTORE, &state);

	    /*
	     * Clear references so that Addrbook Entry caching in adrbklib.c
	     * is allowed to throw them out of cache.
	     */
	    for(i = 0; i < as.n_addrbk; i++){
		pab = &as.adrbks[i];
		if(pab->ostatus == Open || pab->ostatus == NoDisplay)
		  adrbk_clearrefs(pab->address_book);
	    }

	    goto ok;
	}
	else{  /* go back and use what we had before, so we get the error */
	    if(tmp)
	      fs_give((void **)&tmp);

	    tmp = NULL;
	    goto start;
	}
    }

    if(save_and_restore_f)
      (*save_and_restore_f)(SAR_RESTORE, &state);

    /*
     * Clear references so that Addrbook Entry caching in adrbklib.c
     * is allowed to throw them out of cache.
     */
    for(i = 0; i < as.n_addrbk; i++){
	pab = &as.adrbks[i];
	if(pab->ostatus == Open || pab->ostatus == NoDisplay)
	  adrbk_clearrefs(pab->address_book);
    }

    if(ps_global->c_client_error[0] != '\0'){
        /* Parse error.  Return string as is and error message */
	if(full_to){
	    if(to.type == Str)
	      *full_to = cpystr(to.arg.str);
	    else{
	        if(to.arg.abe->nickname && to.arg.abe->nickname[0])
	          *full_to = cpystr(to.arg.abe->nickname);
	        else if(to.arg.abe->tag == Single)
	          *full_to = cpystr(to.arg.abe->addr.addr);
	        else
	          *full_to = cpystr("");
	    }
	}

	if(error != NULL){
	    /* display previous error and add new one */
	    if(*error){
		q_status_message(SM_ORDER, 3, 5, *error);
		display_message('x');
		fs_give((void **)error);
	    }

            *error = cpystr(ps_global->c_client_error);
	}

        dprint((2,
	    "build_address_internal returning parse error: %s\n",
                  ps_global->c_client_error ? ps_global->c_client_error : "?"));
	if(a)
	  mail_free_address(&a);

	if(tmp)
	  fs_give((void **)&tmp);

	if(mangled){
	    if(ps_global->mangled_screen)
	      *mangled |= BUILDER_SCREEN_MANGLED;
	    else if(ps_global->mangled_footer)
	      *mangled |= BUILDER_FOOTER_MANGLED;
	}
	  
        return -1;
    }

    if(mangled){
	if(ps_global->mangled_screen)
	  *mangled |= BUILDER_SCREEN_MANGLED;
	else if(ps_global->mangled_footer)
	  *mangled |= BUILDER_FOOTER_MANGLED;
    }

    /*
     * If there's a loop in the addressbook, we modify the address and
     * send an error back, but we still return 0.
     */
ok:
    if(loop && error != NULL){
	/* display previous error and add new one */
	if(*error){
	    q_status_message(SM_ORDER, 3, 5, *error);
	    display_message('x');
	    fs_give((void **)error);
	}

	*error = cpystr(_("Loop or Duplicate detected in addressbook!"));
    }


    if(full_to){
	if(simple_verify){
	    if(tmp){
		*full_to = tmp;  /* add the brackets to route addr */
		tmp = NULL;
	    }
	    else{
		/* try to return what they sent us */
		if(to.type == Str)
		  *full_to = cpystr(to.arg.str);
		else{
		    if(to.arg.abe->nickname && to.arg.abe->nickname[0])
		      *full_to = cpystr(to.arg.abe->nickname);
		    else if(to.arg.abe->tag == Single)
		      *full_to = cpystr(to.arg.abe->addr.addr);
		    else
		      *full_to = cpystr("");
		}
	    }
	}
	else{
	    RFC822BUFFER rbuf;
	    size_t len;

	    len = est_size(a);
	    *full_to = (char *) fs_get(len * sizeof(char));
	    (*full_to)[0] = '\0';
	    /*
	     * Assume that quotes surrounding the whole personal name are
	     * not meant to be literal quotes.  That is, the name
	     * "Joe College, PhD." is quoted so that we won't do the
	     * switcheroo of Last, First, not so that the quotes will be
	     * literal.  Rfc822_write_address will put the quotes back if they
	     * are needed, so Joe College would end up looking like
	     * "Joe College, PhD." <joe@somewhere.edu> but not like
	     * "\"Joe College, PhD.\"" <joe@somewhere.edu>.
	     */
	    strip_personal_quotes(a);
	    rbuf.f   = dummy_soutr;
	    rbuf.s   = NULL;
	    rbuf.beg = *full_to;
	    rbuf.cur = *full_to;
	    rbuf.end = (*full_to)+len-1;
	    rfc822_output_address_list(&rbuf, a, 0L, NULL);
	    *rbuf.cur = '\0';
	}
    }

    if(no_repo && (did_set & FCC_NOREPO || did_set & LCC_NOREPO))
      *no_repo = 1;

    /*
     * The condition in the leading if means that addressbook fcc's
     * override the fcc-rule (because did_set will be set).
     */
    if(fcc && !(did_set & FCC_SET)){
	char *fcc_got = NULL;

	if((ps_global->fcc_rule == FCC_RULE_LAST
	    || ps_global->fcc_rule == FCC_RULE_CURRENT)
	   && strcmp(fcc_got = get_fcc(NULL), ps_global->VAR_DEFAULT_FCC)){
	    if(*fcc)
	      fs_give((void **)fcc);
	    
	    *fcc = cpystr(fcc_got);
	}
	else if(a && a->host){ /* not group syntax */
	    if(*fcc)
	      fs_give((void **)fcc);

	    if(!tmp)
	      tmp = (char *)fs_get((size_t)200);

	    if((ps_global->fcc_rule == FCC_RULE_RECIP ||
		ps_global->fcc_rule == FCC_RULE_NICK_RECIP) &&
		  get_uname(a ? a->mailbox : NULL, tmp, 200))
	      *fcc = cpystr(tmp);
	    else
	      *fcc = cpystr(ps_global->VAR_DEFAULT_FCC);
	}
	else{ /* first addr is group syntax */
	    if(!*fcc)
	      *fcc = cpystr(ps_global->VAR_DEFAULT_FCC);
	    /* else, leave it alone */
	}

	if(fcc_got)
	  fs_give((void **)&fcc_got);
    }

    if(a)
      mail_free_address(&a);

    if(tmp)
      fs_give((void **)&tmp);

    if(wp_exit)
      return -1;

    return 0;
}


/*
 * Expand an address string against the address books, local names, and domain.
 *
 * Args:  to      -- this is either an address string to parse (one or more
 *		      address strings separated by commas) or it is an
 *		      AdrBk_Entry, in which case it refers to a single addrbook
 *		      entry.  If it is an abe, then it is treated the same as
 *		      if the nickname of this entry was passed in and we
 *		      looked it up in the addrbook, except that it doesn't
 *		      actually have to have a non-null nickname.
 *     userdomain -- domain the user is in
 *    localdomain -- domain of the password file (usually same as userdomain)
 *  loop_detected -- pointer to an int we set if we detect a loop in the
 *		     address books (or a duplicate in a list)
 *       fcc      -- Returned value of fcc for first addr in a_string
 *       did_set  -- expand_address set the fcc (need this in case somebody
 *                     sets fcc explicitly to a value equal to default-fcc)
 *  simple_verify -- don't add list full names or expand 2nd level lists
 *
 * Result: An adrlist of expanded addresses is returned
 *
 * If the localdomain is NULL, then no lookup against the password file will
 * be done.
 */
ADDRESS *
expand_address(BuildTo to, char *userdomain, char *localdomain, int *loop_detected,
	       char **fcc, int *did_set, char **lcc, char **error, int recursing,
	       int simple_verify, int *mangled)
{
    size_t       domain_length, length;
    ADDRESS     *adr, *a, *a_tail, *adr2, *a2, *a_temp, *wp_a;
    AdrBk_Entry *abe, *abe2;
    char        *list, *l1, **l2;
    char        *tmp_a_string, *q;
    BuildTo      bldto;
    static char *fakedomain;

    dprint((9, "- expand_address -  (%s)\n",
	(to.type == Str) ? (to.arg.str ? to.arg.str : "nul")
			 : (to.arg.abe->nickname ? to.arg.abe->nickname
						: "no nick")));
    /*
     * We use the domain "@" to detect an unqualified address.  If it comes
     * back from rfc822_parse_adrlist with the host part set to "@", then
     * we know it must have been unqualified (so we should look it up in the
     * addressbook).  Later, we also use a c-client hack.  If an ADDRESS has
     * a host part that begins with @ then rfc822_write_address()
     * will write only the local part and leave off the @domain part.
     *
     * We also malloc enough space here so that we can strcpy over host below.
     */
    domain_length = MAX(localdomain!=NULL ? strlen(localdomain) : (size_t)0,
			userdomain!=NULL ? strlen(userdomain) : (size_t)0);
    if(!recursing){
	fakedomain = (char *)fs_get(domain_length + 1);
	memset((void *)fakedomain, '@', domain_length);
	fakedomain[domain_length] = '\0';
    }

    adr = NULL;

    if(to.type == Str){
	/* rfc822_parse_adrlist feels free to destroy input so send copy */
	tmp_a_string = cpystr(to.arg.str);
	/* remove trailing comma */
	for(q = tmp_a_string + strlen(tmp_a_string) - 1;
	    q >= tmp_a_string && (*q == SPACE || *q == ',');
	    q--)
	  *q = '\0';

	if(as.n_impl)
	  mail_parameters(NIL, SET_PARSEPHRASE, (void *)massage_phrase_addr);

	rfc822_parse_adrlist(&adr, tmp_a_string, fakedomain);

	if(as.n_impl)
	  mail_parameters(NIL, SET_PARSEPHRASE, NULL);

	/*
	 * Short circuit the process if there was a parsing error.
	 */
	if(!recursing && ps_global->c_client_error[0] != '\0')
	  mail_free_address(&adr);

	fs_give((void **)&tmp_a_string);
    }
    else{
	if(!to.arg.abe ||
	   (to.arg.abe->tag == Single &&
	     (!to.arg.abe->addr.addr || to.arg.abe->addr.addr[0] == '\0')) ||
	   (to.arg.abe->tag == List &&
	     (!to.arg.abe->addr.list || !to.arg.abe->addr.list[0] ||
	      to.arg.abe->addr.list[0][0] == '\0'))){
	    adr = NULL;
	}
	else{
	    /* if we've already looked it up, fake an adr */
	    adr = mail_newaddr();
	    adr->mailbox = cpystr(to.arg.abe->nickname);
	    adr->host    = cpystr(fakedomain);
	}
    }

    for(a = adr, a_tail = adr; a;){

	/* start or end of c-client group syntax */
	if(!a->host){
            a_tail = a;
            a      = a->next;
	    continue;
	}
        else if(a->host[0] != '@'){
            /* Already fully qualified hostname */
            a_tail = a;
            a      = a->next;
        }
        else{
            /*
             * Hostname is "@" indicating name wasn't qualified.
             * Need to look up in address book, and the password file.
             * If no match then fill in the local domain for host.
             */
	    if(to.type == Str)
              abe = adrbk_lookup_with_opens_by_nick(a->mailbox,
						    !recursing,
						    (int *)NULL, -1);
	    else
              abe = to.arg.abe;

            if(simple_verify && abe == NULL){
                /*--- Move to next address in list -----*/
                a_tail = a;
                a = a->next;
            }
            else if(abe == NULL){
		WP_ERR_S wp_err;

	        if(F_OFF(F_COMPOSE_REJECTS_UNQUAL, ps_global)){
		    if(localdomain != NULL && a->personal == NULL){
			/* lookup in passwd file for local full name */
			a->personal = local_name_lookup(a->mailbox); 
			/* we know a->host is long enough for localdomain */
			if(a->personal){
			    /* we know a->host is long enough for userdomain */
			    strncpy(a->host, localdomain, domain_length+1);
			    a->host[domain_length] = '\0';
			}
		    }
		}

		/*
		 * Didn't find it in address book or password
		 * file, try white pages.
		 */
		memset(&wp_err, 0, sizeof(wp_err));
		wp_err.mangled = mangled;
		if(!wp_exit && a->personal == NULL &&
		   (wp_a = wp_lookups(a->mailbox, &wp_err, recursing))){
		    if(wp_a->mailbox && wp_a->mailbox[0] &&
		       wp_a->host && wp_a->host[0]){
			a->personal = wp_a->personal;
			if(a->adl)fs_give((void **)&a->adl);
			a->adl = wp_a->adl;
			if(a->mailbox)fs_give((void **)&a->mailbox);
			a->mailbox = wp_a->mailbox;
			if(a->host)fs_give((void **)&a->host);
			a->host = wp_a->host;
		    }

		    fs_give((void **)&wp_a);
		}

		if(wp_err.error){
		    /*
		     * If wp_err has already been displayed long enough
		     * just get rid of it. Otherwise, try to fit it in
		     * with any other error messages we have had.
		     * In that case we may display error messages in a
		     * weird order. We'll have to see if this is a problem
		     * in real life.
		     */
		    if(status_message_remaining() && error && !wp_exit){
			if(*error){
			    q_status_message(SM_ORDER, 3, 5, *error);
			    display_message('x');
			    fs_give((void **)error);
			}

			*error = wp_err.error;
		    }
		    else
		      fs_give((void **)&wp_err.error);
		}
		  
		/* still haven't found it */
		if(a->host[0] == '@' && !wp_nobail){
		    int space_phrase;

		    /*
		     * Figure out if there is a space in the mailbox so
		     * that user probably meant it to resolve on the
		     * directory server.
		     */
		    space_phrase = (a->mailbox && strindex(a->mailbox, SPACE));

		    if(!wp_nobail && (F_ON(F_COMPOSE_REJECTS_UNQUAL, ps_global) ||
		       (space_phrase && as.n_impl))){
			char ebuf[200];

			/* TRANSLATORS: The first %s is a mailbox, the second is either
			   directory or addressbook */
			snprintf(ebuf, sizeof(ebuf), _("Address for \"%s\" not in %s"),
				a->mailbox,
				(space_phrase && as.n_impl) ? _("directory")
							    : _("addressbook"));

			if(error){
			    /* display previous error and add new one */
			    if(*error){
				if(!wp_exit){
				    q_status_message(SM_ORDER, 3, 5, *error);
				    display_message('x');
				}

				fs_give((void **)error);
			    }

			    if(!wp_exit)
			      *error = cpystr(ebuf);
			}

			if(!wp_exit)
			  strncpy(ps_global->c_client_error, ebuf, 200);

			if(!recursing)
			  fs_give((void **)&fakedomain);

			if(adr)
			  mail_free_address(&adr);
			    
			return(adr);
		    }
		    else if(wp_err.wp_err_occurred){
			if(!recursing){
			    if(error && *error && !wp_exit)
			      strncpy(ps_global->c_client_error, *error, sizeof(ps_global->c_client_error));
			    else
			      strncpy(ps_global->c_client_error, " ", sizeof(ps_global->c_client_error));

			    ps_global->c_client_error[sizeof(ps_global->c_client_error)-1] = '\0';

			    fs_give((void **)&fakedomain);
			    if(adr)
			      mail_free_address(&adr);
			    
			    return(adr);
			}
		    }
		    else{
			/* we know a->host is long enough for userdomain */
			strncpy(a->host, userdomain, domain_length+1);
			a->host[domain_length] = '\0';
		    }
		}

                /*--- Move to next address in list -----*/
                a_tail = a;
                a = a->next;

            }
	    /* expand first list, but not others if simple_verify */
	    else if(abe->tag == List && simple_verify && recursing){
                /*--- Move to next address in list -----*/
                a_tail = a;
                a = a->next;
	    }
	    else{
                /*
                 * There was a match in the address book.  We have to do a lot
                 * here because the item from the address book might be a 
                 * distribution list.  Treat the string just like an address
                 * passed in to parse and recurse on it.  Then combine
                 * the personal names from address book.  Lastly splice
                 * result into address list being processed
                 */

		/* first addr in list and fcc needs to be filled in */
		if(!recursing && a == adr && fcc && !(*did_set & FCC_SET)){
		    /*
		     * Easy case for fcc.  This is a nickname that has
		     * an fcc associated with it.
		     */
		    if(abe->fcc && abe->fcc[0]){
			if(*fcc)
			  fs_give((void **)fcc);

			if(!strcmp(abe->fcc, "\"\""))
			  *fcc = cpystr("");
			else
			  *fcc = cpystr(abe->fcc);

			/*
			 * After we expand the list, we no longer remember
			 * that it came from this address book entry, so
			 * we wouldn't be able to set the fcc again based
			 * on the result.  This tells our caller to remember
			 * that for us.
			 */
			*did_set |= (FCC_SET | FCC_NOREPO);
		    }
		    /*
		     * else if fcc-rule=fcc-by-nickname, use that
		     */
		    else if(abe->nickname && abe->nickname[0] &&
			    (ps_global->fcc_rule == FCC_RULE_NICK ||
			     ps_global->fcc_rule == FCC_RULE_NICK_RECIP)){
			if(*fcc)
			  fs_give((void **)fcc);

			*fcc = cpystr(abe->nickname);
			/*
			 * After we expand the list, we no longer remember
			 * that it came from this address book entry, so
			 * we wouldn't be able to set the fcc again based
			 * on the result.  This tells our caller to remember
			 * that for us.
			 */
			*did_set |= (FCC_SET | FCC_NOREPO);
		    }
		}

		/* lcc needs to be filled in */
		if(a == adr  &&
		    lcc      &&
		   (!*lcc || !**lcc)){
		    ADDRESS *atmp;
		    char    *tmp;

		    /* return fullname for To line */
		    if(abe->fullname && *abe->fullname){
			RFC822BUFFER rbuf;
			size_t l, len;

			if(*lcc)
			  fs_give((void **)lcc);

			atmp = mail_newaddr();
			atmp->mailbox = cpystr(abe->fullname);
			len = est_size(atmp);
			tmp = (char *) fs_get(len * sizeof(char));
			tmp[0] = '\0';
			/* write the phrase with quoting */
			rbuf.f   = dummy_soutr;
			rbuf.s   = NULL;
			rbuf.beg = tmp;
			rbuf.cur = tmp;
			rbuf.end = tmp+len-1;
			rfc822_output_address_list(&rbuf, atmp, 0L, NULL);
			*rbuf.cur = '\0';
			l = strlen(tmp)+1;
			*lcc = (char *) fs_get((l+1) * sizeof(char));
			strncpy(*lcc, tmp, l);
			(*lcc)[l] = '\0';
			strncat(*lcc, ";", l+1-strlen(*lcc)-1);
			(*lcc)[l] = '\0';
			mail_free_address(&atmp);
			fs_give((void **)&tmp);
			*did_set |= (LCC_SET | LCC_NOREPO);
		    }
		}

                if(recursing && abe->referenced){
                     /*---- caught an address loop! ----*/
                    fs_give(((void **)&a->host));
		    (*loop_detected)++;
                    a->host = cpystr("***address-loop-in-addressbooks***");
                    continue;
                }

                abe->referenced++;   /* For address loop detection */
                if(abe->tag == List){
                    length = 0;
                    for(l2 = abe->addr.list; *l2; l2++)
                        length += (strlen(*l2) + 1);

                    list = (char *)fs_get(length + 1);
		    list[0] = '\0';
                    l1 = list;
                    for(l2 = abe->addr.list; *l2; l2++){
                        if(l1 != list && length+1-(l1-list) > 0)
                          *l1++ = ',';

                        strncpy(l1, *l2, length+1-(l1-list));
			if(l1 > &list[length])
			  l1 = &list[length];

			list[length] = '\0';

                        l1 += strlen(l1);
                    }

		    bldto.type    = Str;
		    bldto.arg.str = list;
                    adr2 = expand_address(bldto, userdomain, localdomain,
					  loop_detected, fcc, did_set,
					  lcc, error, 1, simple_verify,
					  mangled);
                    fs_give((void **)&list);
                }
                else if(abe->tag == Single){
                    if(strucmp(abe->addr.addr, a->mailbox)){
			bldto.type    = Str;
			bldto.arg.str = abe->addr.addr;
                        adr2 = expand_address(bldto, userdomain,
					      localdomain, loop_detected,
					      fcc, did_set, lcc,
					      error, 1, simple_verify,
					      mangled);
                    }
		    else{
                        /*
			 * A loop within plain single entry is ignored.
			 * Set up so later code thinks we expanded.
			 */
                        adr2          = mail_newaddr();
                        adr2->mailbox = cpystr(abe->addr.addr);
                        adr2->host    = cpystr(userdomain);
                        adr2->adl     = cpystr(a->adl);
                    }
                }

                abe->referenced--;  /* Janet Jackson <janet@dialix.oz.au> */
                if(adr2 == NULL){
                    /* expanded to nothing, hack out of list */
                    a_temp = a;
                    if(a == adr){
                        adr    = a->next;
                        a      = adr;
                        a_tail = adr;
                    }
		    else{
                        a_tail->next = a->next;
                        a            = a->next;
                    }

		    a_temp->next = NULL;  /* So free won't do whole list */
                    mail_free_address(&a_temp);
                    continue;
                }

                /*
                 * Personal names:  If the expanded address has a personal
                 * name and the address book entry is a list with a fullname,
		 * tack the full name from the address book on in front.
                 * This mainly occurs with a distribution list where the
                 * list has a full name, and the first person in the list also
                 * has a full name.
		 *
		 * This algorithm doesn't work very well if lists are
		 * included within lists, but it's not clear what would
		 * be better.
                 */
		if(abe->fullname && abe->fullname[0]){
		    if(adr2->personal && adr2->personal[0]){
			if(abe->tag == List){
			    /* combine list name and existing name */
			    char *name;

			    if(!simple_verify){
				size_t l;

				l = strlen(adr2->personal) + strlen(abe->fullname) + 4;
				name = (char *) fs_get((l+1) * sizeof(char));
				snprintf(name, l+1, "%s -- %s", abe->fullname,
					adr2->personal);
				fs_give((void **)&adr2->personal);
				adr2->personal = name;
			    }
			}
			else{
			    /* replace with nickname fullname */
			    fs_give((void **)&adr2->personal);
			    adr2->personal = adrbk_formatname(abe->fullname,
							      NULL, NULL);
			}
		    }
		    else{
			if(abe->tag != List || !simple_verify){
			    if(adr2->personal)
			      fs_give((void **)&adr2->personal);

			    adr2->personal = adrbk_formatname(abe->fullname,
							      NULL, NULL);
			}
		    }
		}

                /* splice new list into old list and remove replaced addr */
                for(a2 = adr2; a2->next != NULL; a2 = a2->next)
		  ;/* do nothing */

                a2->next = a->next;
                if(a == adr)
                  adr    = adr2;
		else
                  a_tail->next = adr2;

                /* advance to next item, and free replaced ADDRESS */
                a_tail       = a2;
                a_temp       = a;
                a            = a->next;
                a_temp->next = NULL;  /* So free won't do whole list */
                mail_free_address(&a_temp);
            }
        }

	if((a_tail == adr && fcc && !(*did_set & FCC_SET))
	   || !a_tail->personal)
	  /*
	   * This looks for the addressbook entry that matches the given
	   * address.  It looks through all the addressbooks
	   * looking for an exact match and then returns that entry.
	   */
	  abe2 = address_to_abe(a_tail);
	else
	  abe2 = NULL;

	/*
	 * If there is no personal name yet but we found the address in
	 * an address book, then we take the fullname from that address
	 * book entry and use it.  One consequence of this is that if I
	 * have an address book entry with address hubert@cac.washington.edu
	 * and a fullname of Steve Hubert, then there is no way I can
	 * send mail to hubert@cac.washington.edu without having the
	 * personal name filled in for me.
	 */
	if(!a_tail->personal && abe2 && abe2->fullname && abe2->fullname[0])
	  a_tail->personal = adrbk_formatname(abe2->fullname, NULL, NULL);

	/* if it's first addr in list and fcc hasn't been set yet */
	if(!recursing && a_tail == adr && fcc && !(*did_set & FCC_SET)){
	    if(abe2 && abe2->fcc && abe2->fcc[0]){
		if(*fcc)
		  fs_give((void **)fcc);

		if(!strcmp(abe2->fcc, "\"\""))
		  *fcc = cpystr("");
		else
		  *fcc = cpystr(abe2->fcc);

		*did_set |= FCC_SET;
	    }
	    else if(abe2 && abe2->nickname && abe2->nickname[0] &&
		    (ps_global->fcc_rule == FCC_RULE_NICK ||
		     ps_global->fcc_rule == FCC_RULE_NICK_RECIP)){
		if(*fcc)
		  fs_give((void **)fcc);

		*fcc = cpystr(abe2->nickname);
		*did_set |= FCC_SET;
	    }
	}

	/*
	 * Lcc needs to be filled in.
	 * Bug: if ^T select was used to put the list in the lcc field, then
	 * the list will have been expanded already and the fullname for
	 * the list will be mixed with the initial fullname in the list,
	 * and we don't have anyway to tell them apart.  We could look for
	 * the --.  We could change expand_address so it doesn't combine
	 * those two addresses.
	 */
	if(adr &&
	    a_tail == adr  &&
	    lcc      &&
	   (!*lcc || !**lcc)){
	    if(adr->personal){
		ADDRESS *atmp;
		char    *tmp;
		RFC822BUFFER rbuf;
		size_t l, len;

		if(*lcc)
		  fs_give((void **)lcc);

		atmp = mail_newaddr();
		atmp->mailbox = cpystr(adr->personal);
		len = est_size(atmp);
		tmp = (char *) fs_get(len * sizeof(char));
		tmp[0] = '\0';
		/* write the phrase with quoting */
		rbuf.f   = dummy_soutr;
		rbuf.s   = NULL;
		rbuf.beg = tmp;
		rbuf.cur = tmp;
		rbuf.end = tmp+len-1;
		rfc822_output_address_list(&rbuf, atmp, 0L, NULL);
		*rbuf.cur = '\0';
		l = strlen(tmp)+1;
		*lcc = (char *) fs_get((l+1) * sizeof(char));
		strncpy(*lcc, tmp, l);
		(*lcc)[l] = '\0';
		strncat(*lcc, ";", l+1-strlen(*lcc)-1);
		(*lcc)[l] = '\0';
		mail_free_address(&atmp);
		fs_give((void **)&tmp);
		*did_set |= (LCC_SET | LCC_NOREPO);
	    }
	}
    }

    if(!recursing)
      fs_give((void **)&fakedomain);

    return(adr);
}


/*
 * This is a call back from rfc822_parse_adrlist. If we return NULL then
 * it does its regular parsing. However, if we return and ADDRESS then it
 * returns that address to us.
 *
 * We want a phrase with no address to be parsed just like a nickname would
 * be in expand_address. That is, the phrase gets put in the mailbox name
 * and the host is set to the fakedomain. Then expand_address will try to
 * look that up using wp_lookup().
 *
 * Args     phrase -- The start of the phrase of the address
 *             end -- The first character after the phrase
 *            host -- The defaulthost that was passed to rfc822_parse_adrlist
 *
 * Returns  An allocated address, or NULL.
 */
ADDRESS *
massage_phrase_addr(char *phrase, char *end, char *host)
{
    ADDRESS *adr = NULL;
    size_t   size;

    if((size = end - phrase) > 0){
	char *mycopy;

	mycopy = (char *)fs_get((size+1) * sizeof(char));
	strncpy(mycopy, phrase, size);
	mycopy[size] = '\0';
	removing_trailing_white_space(mycopy);

	/*
	 * If it is quoted we want to leave it alone. It will be treated
	 * like an atom in parse_adrlist anyway, which is what we want to
	 * have happen, and we'd have to remove the quotes ourselves here
	 * if we did it. The problem then is that we don't know if we
	 * removed the quotes here or they just weren't there in the
	 * first place.
	 */
	if(*mycopy == '"' && mycopy[strlen(mycopy)-1] == '"')
	  fs_give((void **)&mycopy);
	else{
	    adr = mail_newaddr();
	    adr->mailbox = mycopy;
	    adr->host    = cpystr(host);
	}
    }

    return(adr);
}


/*
 * Run through the adrlist "adr" and strip off any enclosing quotes
 * around personal names.  That is, change "Joe L. Normal" to
 * Joe L. Normal.
 */
void
strip_personal_quotes(struct mail_address *adr)
{
    int   len;
    register char *p, *q;

    while(adr){
	if(adr->personal){
	    len = strlen(adr->personal);
	    if(len > 1
	       && adr->personal[0] == '"'
	       && adr->personal[len-1] == '"'){
		adr->personal[len-1] = '\0';
		p = adr->personal;
		q = p + 1;
		while((*p++ = *q++) != '\0')
		  ;
	    }
	}

	adr = adr->next;
    }
}


static char *last_fcc_used;
/*
 * Returns alloc'd fcc.
 */
char *
get_fcc(char *fcc_arg)
{
    char *fcc;

    /*
     * Use passed in arg unless it is the same as default (and then
     * may use that anyway below).
     */
    if(fcc_arg && strcmp(fcc_arg, ps_global->VAR_DEFAULT_FCC))
      fcc = cpystr(fcc_arg);
    else{
	if(ps_global->fcc_rule == FCC_RULE_LAST && last_fcc_used)
	  fcc = cpystr(last_fcc_used);
	else if(ps_global->fcc_rule == FCC_RULE_CURRENT
		&& ps_global->mail_stream
		&& !sp_flagged(ps_global->mail_stream, SP_INBOX)
		&& !IS_NEWS(ps_global->mail_stream)
		&& ps_global->cur_folder
		&& ps_global->cur_folder[0]){
	    CONTEXT_S *cntxt = ps_global->context_current;
	    char *rs = NULL;

	    if(((cntxt->use) & CNTXT_SAVEDFLT))
	      rs = ps_global->cur_folder;
	    else
	      rs = ps_global->mail_stream->mailbox;

	    fcc = cpystr((rs&&*rs) ? rs : ps_global->VAR_DEFAULT_FCC);
	}
	else
	  fcc = cpystr(ps_global->VAR_DEFAULT_FCC);
    }
    
    return(fcc);
}


/*
 * Save the fcc for use with next composition.
 */
void
set_last_fcc(char *fcc)
{
    size_t l;

    if(fcc){
	if(!last_fcc_used)
	  last_fcc_used = cpystr(fcc);
	else if(strcmp(last_fcc_used, fcc)){
	    if((l=strlen(last_fcc_used)) >= strlen(fcc)){
		strncpy(last_fcc_used, fcc, l+1);
		last_fcc_used[l] = '\0';
	    }
	    else{
		fs_give((void **)&last_fcc_used);
		last_fcc_used = cpystr(fcc);
	    }
	}
    }
}


/*
 * Figure out what the fcc is based on the given to address.
 *
 * Returns an allocated copy of the fcc, to be freed by the caller, or NULL.
 */
char *
get_fcc_based_on_to(struct mail_address *to)
{
    ADDRESS    *next_addr;
    char       *bufp, *fcc;
    BuildTo	bldto;
    size_t      len;

    if(!to || !to->host || to->host[0] == '.')
      return(NULL);

    /* pick off first address */
    next_addr = to->next;
    to->next = NULL;
    len = est_size(to);
    bufp = (char *) fs_get(len * sizeof(char));
    bufp[0] = '\0';

    bldto.type    = Str;
    bldto.arg.str = cpystr(addr_string(to, bufp, len));

    fs_give((void **)&bufp);
    to->next = next_addr;

    fcc = NULL;

    (void) build_address_internal(bldto, NULL, NULL, &fcc, NULL, NULL, NULL, 0, NULL);

    if(bldto.arg.str)
      fs_give((void **) &bldto.arg.str);
    
    return(fcc);
}


/*
 * Free storage in headerentry.bldr_private.
 */
void
free_privatetop(PrivateTop **pt)
{
    if(pt && *pt){
	if((*pt)->affector)
	  fs_give((void **)&(*pt)->affector);
	
	fs_give((void **)pt);
    }
}
