#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: reply.c 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $";
#endif

/*
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

#include "../pith/headers.h"
#include "../pith/reply.h"
#include "../pith/send.h"
#include "../pith/init.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/remote.h"
#include "../pith/status.h"
#include "../pith/mailview.h"
#include "../pith/filter.h"
#include "../pith/newmail.h"
#include "../pith/bldaddr.h"
#include "../pith/mailindx.h"
#include "../pith/mimedesc.h"
#include "../pith/detach.h"
#include "../pith/help.h"
#include "../pith/pipe.h"
#include "../pith/addrstring.h"
#include "../pith/news.h"
#include "../pith/util.h"
#include "../pith/pattern.h"
#include "../pith/detoken.h"
#include "../pith/stream.h"
#include "../pith/busy.h"
#include "../pith/readfile.h"
#include "../pith/text.h"
#include "../pith/list.h"
#include "../pith/ablookup.h"
#include "../pith/mailcmd.h"
#include "../pith/margin.h"


/*
 * Internal prototypes
 */
void	 bounce_mask_header(char **, char *);


int	(*pith_opt_replyto_prompt)(void);
int	(*pith_opt_reply_to_all_prompt)(int *);


/*
 * standard type of storage object used for body parts...
 */
#define		  PART_SO_TYPE	CharStar


char *(*pith_opt_user_agent_prefix)(void);


/*
 * reply_harvest - 
 *
 *  Returns: 1 if addresses successfully copied
 *	     0 on user cancel or error
 *
 *  Input flags: 
 *		 RSF_FORCE_REPLY_TO
 *		 RSF_QUERY_REPLY_ALL
 *		 RSF_FORCE_REPLY_ALL
 *
 *  Output flags:
 *		 RSF_FORCE_REPLY_ALL
 * 
 */
int
reply_harvest(struct pine *ps, long int msgno, char *section, ENVELOPE *env,
	      struct mail_address **saved_from, struct mail_address **saved_to,
	      struct mail_address **saved_cc, struct mail_address **saved_resent,
	      int *flags)
{
    ADDRESS *ap, *ap2, *rep_address;
    int	     ret = 0, sniff_resent = 0;
    char    *rep_field;

      /*
       * If Reply-To is same as From just treat it like it was From.
       * Otherwise, always use the reply-to if we're replying to more
       * than one msg or say ok to using it, even if it's us.
       * If there's no reply-to or it's the same as the from, assume
       * that the user doesn't want to reply to himself, unless there's
       * nobody else.
       */
    if(env->reply_to && !addr_lists_same(env->reply_to, env->from)
       && (F_ON(F_AUTO_REPLY_TO, ps_global)
	   || ((*flags) & RSF_FORCE_REPLY_TO)
	   || (pith_opt_replyto_prompt && (*pith_opt_replyto_prompt)() == 'y'))){
	rep_field   = "reply-to";
	rep_address = env->reply_to;
    }
    else{
	rep_field   = "From";
	rep_address = env->from;
    }

    ap = reply_cp_addr(ps, msgno, section, rep_field, *saved_from,
		       (ADDRESS *) NULL, rep_address, RCA_NOT_US);

    if(ret == 'x') {
	cmd_cancelled("Reply");
	return(0);
    }

    reply_append_addr(saved_from, ap);

    /*--------- check for other recipients ---------*/
    if(((*flags) & (RSF_FORCE_REPLY_ALL | RSF_QUERY_REPLY_ALL))){

	if((ap = reply_cp_addr(ps, msgno, section, "To", *saved_to,
			      *saved_from, env->to, RCA_NOT_US)) != NULL)
	  reply_append_addr(saved_to, ap);

	if((ap = reply_cp_addr(ps, msgno, section, "Cc", *saved_cc,
			      *saved_from, env->cc, RCA_NOT_US)) != NULL)
	  reply_append_addr(saved_cc, ap);

	/*
	 * In these cases, we either need to look at the resent headers
	 * to include in the reply-to-all, or to decide whether or not
	 * we need to ask the reply-to-all question.
	 */
	if(((*flags) & RSF_FORCE_REPLY_ALL)
	   || (((*flags) & RSF_QUERY_REPLY_ALL)
	       && ((!(*saved_from) && !(*saved_cc))
		   || (*saved_from && !(*saved_to) && !(*saved_cc))))){

	    sniff_resent++;
	    if((ap2 = reply_resent(ps, msgno, section)) != NULL){
		/*
		 * look for bogus addr entries and replace
		 */
		if((ap = reply_cp_addr(ps, 0, NULL, NULL, *saved_resent,
				      *saved_from, ap2, RCA_NOT_US)) != NULL)

		  reply_append_addr(saved_resent, ap);

		mail_free_address(&ap2);
	    }
	}

	/*
	 * It makes sense to ask reply-to-all now.
	 */
	if(((*flags) & RSF_QUERY_REPLY_ALL)
	   && ((*saved_from && (*saved_to || *saved_cc || *saved_resent))
	       || (*saved_cc || *saved_resent))){
	    *flags &= ~RSF_QUERY_REPLY_ALL;
	    if(pith_opt_reply_to_all_prompt
	       && (*pith_opt_reply_to_all_prompt)(flags) < 0){
		cmd_cancelled("Reply");
		return(0);
	    }
	}

	/*
	 * If we just answered yes to the reply-to-all question and
	 * we still haven't collected the resent headers, do so now.
	 */
	if(((*flags) & RSF_FORCE_REPLY_ALL) && !sniff_resent
	   && (ap2 = reply_resent(ps, msgno, section))){
	    /*
	     * look for bogus addr entries and replace
	     */
	    if((ap = reply_cp_addr(ps, 0, NULL, NULL, *saved_resent,
				  *saved_from, ap2, RCA_NOT_US)) != NULL)
	      reply_append_addr(saved_resent, ap);

	    mail_free_address(&ap2);
	}
    }

    return(1);
}


/*----------------------------------------------------------------------
    Return a pointer to a copy of the given address list
  filtering out those already in the "mask" lists and ourself.

Args:  mask1  -- Don't copy if in this list
       mask2  --  or if in this list
       source -- List to be copied
       us_too -- Don't filter out ourself.
       flags  -- RCA_NOT_US   copy all addrs except for our own
                 RCA_ALL      copy all addrs, including our own
                 RCA_ONLY_US  copy only addrs that are our own

  ---*/
ADDRESS *
reply_cp_addr(struct pine *ps, long int msgno, char *section, char *field,
	      struct mail_address *mask1, struct mail_address *mask2,
	      struct mail_address *source, int flags)
{
    ADDRESS *tmp1, *tmp2, *ret = NULL, **ret_tail;

    /* can only choose one of these flags values */
    assert(!((flags & RCA_ALL && flags & RCA_ONLY_US)
	     || (flags & RCA_ALL && flags & RCA_NOT_US)
	     || (flags & RCA_ONLY_US && flags & RCA_NOT_US)));

    for(tmp1 = source; msgno && tmp1; tmp1 = tmp1->next)
      if(tmp1->host && tmp1->host[0] == '.'){
	  char *h, *fields[2];

	  fields[0] = field;
	  fields[1] = NULL;
	  if((h = pine_fetchheader_lines(ps ? ps->mail_stream : NULL,
				        msgno, section, fields)) != NULL){
	      char *p, fname[32];
	      int q;

	      q = strlen(h);

	      strncpy(fname, field, sizeof(fname)-2);
	      fname[sizeof(fname)-2] = '\0';
	      strncat(fname, ":", sizeof(fname)-strlen(fname)-1);
	      fname[sizeof(fname)-1] = '\0';

	      for(p = h; (p = strstr(p, fname)) != NULL; )
		rplstr(p, q-(p-h), strlen(fname), "");	/* strip field strings */

	      sqznewlines(h);			/* blat out CR's & LF's */
	      for(p = h; (p = strchr(p, TAB)) != NULL; )
		*p++ = ' ';			/* turn TABs to whitespace */

	      if(*h){
		  long l;
		  size_t ll;

		  ret = (ADDRESS *) fs_get(sizeof(ADDRESS));
		  memset(ret, 0, sizeof(ADDRESS));

		  /* get rid of leading white space */
		  for(p = h; *p == SPACE; p++)
		    ;
		  
		  if(p != h){
		      memmove(h, p, l = strlen(p));
		      h[l] = '\0';
		  }

		  /* base64 armor plate the gunk to protect against
		   * c-client quoting in output.
		   */
		  p = (char *) rfc822_binary(h, strlen(h),
					     (unsigned long *) &l);
		  sqznewlines(p);
		  fs_give((void **) &h);
		  /*
		   * Seems like the 4 ought to be a 2, but I'll leave it
		   * to be safe, in case something else adds 2 chars later.
		   */
		  ll = strlen(p) + 4;
		  ret->mailbox = (char *) fs_get(ll * sizeof(char));
		  snprintf(ret->mailbox, ll, "&%s", p);
		  ret->mailbox[ll-1] = '\0';
		  fs_give((void **) &p);
		  ret->host = cpystr(RAWFIELD);
	      }
	  }

	  return(ret);
      }

    ret_tail = &ret;
    for(source = first_addr(source); source; source = source->next){
	for(tmp1 = first_addr(mask1); tmp1; tmp1 = tmp1->next)
	  if(address_is_same(source, tmp1))	/* it is in mask1, skip it */
	    break;

	for(tmp2 = first_addr(mask2); !tmp1 && tmp2; tmp2 = tmp2->next)
	  if(address_is_same(source, tmp2))	/* it is in mask2, skip it */
	    break;

	/*
	 * If there's no match in masks and this address satisfies the
	 * flags requirement, copy it.
	 */
	if(!tmp1 && !tmp2		/* no mask match */
	   && ((flags & RCA_ALL)	/* including everybody */
	       || (flags & RCA_ONLY_US && address_is_us(source, ps))
	       || (flags & RCA_NOT_US && !address_is_us(source, ps)))){
	    tmp1         = source->next;
	    source->next = NULL;	/* only copy one addr! */
	    *ret_tail    = rfc822_cpy_adr(source);
	    ret_tail     = &(*ret_tail)->next;
	    source->next = tmp1;	/* restore rest of list */
	}
    }

    return(ret);
}


ACTION_S *
set_role_from_msg(struct pine *ps, long int rflags, long int msgno, char *section)
{
    ACTION_S      *role = NULL;
    PAT_S         *pat = NULL;
    SEARCHSET     *ss = NULL;
    PAT_STATE      pstate;

    if(!nonempty_patterns(rflags, &pstate))
      return(role);

    if(msgno > 0L){
	ss = mail_newsearchset();
	ss->first = ss->last = (unsigned long)msgno;
    }

    /* Go through the possible roles one at a time until we get a match. */
    pat = first_pattern(&pstate);

    /* calculate this message's score if needed */
    if(ss && pat && scores_are_used(SCOREUSE_GET) & SCOREUSE_ROLES &&
       get_msg_score(ps->mail_stream, msgno) == SCORE_UNDEF)
      (void)calculate_some_scores(ps->mail_stream, ss, 0);

    while(!role && pat){
	if(match_pattern(pat->patgrp, ps->mail_stream, ss, section,
			 get_msg_score, SE_NOSERVER|SE_NOPREFETCH)){
	    if(!pat->action || pat->action->bogus)
	      break;

	    role = pat->action;
	}
	else
	  pat = next_pattern(&pstate);
    }

    if(ss)
      mail_free_searchset(&ss);

    return(role);
}


/*
 * reply_seed - fill in reply header
 * 
 */
void
reply_seed(struct pine *ps, ENVELOPE *outgoing, ENVELOPE *env,
	   struct mail_address *saved_from, struct mail_address *saved_to,
	   struct mail_address *saved_cc, struct mail_address *saved_resent,
	   char **fcc, int replytoall, char **errmsg)
{
    ADDRESS **to_tail, **cc_tail;
    
    to_tail = &outgoing->to;
    cc_tail = &outgoing->cc;

    if(saved_from){
	/* Put Reply-To or From in To. */
	*to_tail = reply_cp_addr(ps, 0, NULL, NULL, outgoing->to,
				 (ADDRESS *) NULL, saved_from, RCA_ALL);
	/* and the rest in cc */
	if(replytoall){
	    *cc_tail = reply_cp_addr(ps, 0, NULL, NULL, outgoing->cc,
				     outgoing->to, saved_to, RCA_ALL);
	    while(*cc_tail)		/* stay on last address */
	      cc_tail = &(*cc_tail)->next;

	    *cc_tail = reply_cp_addr(ps, 0, NULL, NULL, outgoing->cc,
				     outgoing->to, saved_cc, RCA_ALL);
	    while(*cc_tail)
	      cc_tail = &(*cc_tail)->next;

	    *cc_tail = reply_cp_addr(ps, 0, NULL, NULL, outgoing->cc,
				     outgoing->to, saved_resent, RCA_ALL);
	}
    }
    else if(saved_to){
	/* No From (maybe from us), put To in To. */
	*to_tail = reply_cp_addr(ps, 0, NULL, NULL, outgoing->to,
				 (ADDRESS *)NULL, saved_to, RCA_ALL);
	/* and the rest in cc */
	if(replytoall){
	    *cc_tail = reply_cp_addr(ps, 0, NULL, NULL, outgoing->cc,
				     outgoing->to, saved_cc, RCA_ALL);
	    while(*cc_tail)
	      cc_tail = &(*cc_tail)->next;

	    *cc_tail = reply_cp_addr(ps, 0, NULL, NULL, outgoing->cc,
				     outgoing->to, saved_resent, RCA_ALL);
	}
    }
    else{
	/* No From or To, put everything else in To if replytoall, */
	if(replytoall){
	    *to_tail = reply_cp_addr(ps, 0, NULL, NULL, outgoing->to,
				     (ADDRESS *) NULL, saved_cc, RCA_ALL);
	    while(*to_tail)
	      to_tail = &(*to_tail)->next;

	    *to_tail = reply_cp_addr(ps, 0, NULL, NULL, outgoing->to,
				     (ADDRESS *) NULL, saved_resent, RCA_ALL);
	}
	/* else, reply to original From which must be us */
	else{
	    /*
	     * Put self in To if in original From.
	     */
	    if(!outgoing->newsgroups)
	      *to_tail = reply_cp_addr(ps, 0, NULL, NULL, outgoing->to,
				       (ADDRESS *) NULL, env->from, RCA_ALL);
	}
    }

    /* add any missing personal data */
    reply_fish_personal(outgoing, env);

    /* get fcc */
    if(fcc && outgoing->to && outgoing->to->host[0] != '.'){
	*fcc = get_fcc_based_on_to(outgoing->to);
    }
    else if(fcc && outgoing->newsgroups){
	char *newsgroups_returned = NULL;
	int   rv;

	rv = news_grouper(outgoing->newsgroups, &newsgroups_returned, errmsg, fcc, NULL);
	if(rv != -1 &&
	    strcmp(outgoing->newsgroups, newsgroups_returned)){
	    fs_give((void **)&outgoing->newsgroups);
	    outgoing->newsgroups = newsgroups_returned;
	}
	else
	  fs_give((void **) &newsgroups_returned);
    }
}


/*----------------------------------------------------------------------
    Test the given address lists for equivalence

Args:  x -- First address list for comparison
       y -- Second address for comparison

  ---*/
int
addr_lists_same(struct mail_address *x, struct mail_address *y)
{
    for(x = first_addr(x), y = first_addr(y);
	x && y;
	x = first_addr(x->next), y = first_addr(y->next)){
	if(!address_is_same(x, y))
	  return(0);
    }

    return(!x && !y);			/* true if ran off both lists */
}


/*----------------------------------------------------------------------
    Test the given address against those in the given envelope's to, cc

Args:  addr -- address for comparison
       env  -- envelope to compare against

  ---*/
int
addr_in_env(struct mail_address *addr, ENVELOPE *env)
{
    ADDRESS *ap;

    for(ap = env ? env->to : NULL; ap; ap = ap->next)
      if(address_is_same(addr, ap))
	return(1);

    for(ap = env ? env->cc : NULL; ap; ap = ap->next)
      if(address_is_same(addr, ap))
	return(1);

    return(0);				/* not found! */
}


/*----------------------------------------------------------------------
    Add missing personal info dest from src envelope

Args:  dest -- envelope to add personal info to
       src  -- envelope to get personal info from

NOTE: This is just kind of a courtesy function.  It's really not adding
      anything needed to get the mail thru, but it is nice for the user
      under some odd circumstances.
  ---*/
void
reply_fish_personal(ENVELOPE *dest, ENVELOPE *src)
{
    ADDRESS *da, *sa;

    for(da = dest ? dest->to : NULL; da; da = da->next){
	if(da->personal && !da->personal[0])
	  fs_give((void **)&da->personal);

	for(sa = src ? src->to : NULL; sa && !da->personal ; sa = sa->next)
	  if(address_is_same(da, sa) && sa->personal && sa->personal[0])
	    da->personal = cpystr(sa->personal);

	for(sa = src ? src->cc : NULL; sa && !da->personal; sa = sa->next)
	  if(address_is_same(da, sa) && sa->personal && sa->personal[0])
	    da->personal = cpystr(sa->personal);
    }

    for(da = dest ? dest->cc : NULL; da; da = da->next){
	if(da->personal && !da->personal[0])
	  fs_give((void **)&da->personal);

	for(sa = src ? src->to : NULL; sa && !da->personal; sa = sa->next)
	  if(address_is_same(da, sa) && sa->personal && sa->personal[0])
	    da->personal = cpystr(sa->personal);

	for(sa = src ? src->cc : NULL; sa && !da->personal; sa = sa->next)
	  if(address_is_same(da, sa) && sa->personal && sa->personal[0])
	    da->personal = cpystr(sa->personal);
    }
}


/*----------------------------------------------------------------------
   Given a header field and envelope, build "References: " header data

Args:  

Returns: 
  ---*/
char *
reply_build_refs(ENVELOPE *env)
{
    int   len, id_len, first_ref_len = 0, foldslop;
    char *p, *refs = NULL, *h = env->references;
    char *first_ref = NULL, *tail_refs = NULL;


    if(!(env->message_id && (id_len = strlen(env->message_id))))
      return(NULL);

    if(h){
	/*
	 * The length we have to work with doesn't seem to appear in any
	 * standards. Steve Jones says that in comp.news discussions he
	 * has seen 1024 as the longest length of a header value.
	 * In the inn news source we find MAXHEADERSIZE = 1024. It appears
	 * that is the maximum length of the header value, including 
	 * newlines for folded lines (that is, the newlines are counted).
	 * We'll be conservative and figure every reference will take up a
	 * line of its own when we fold. We'll also count 2 for CRLF instead
	 * of just one for LF just to be safe.       hubert 2001-jan
	 * J.B. Moreno <planb@newsreaders.com> says "The server limit is
	 * more commonly encountered at 999/1000 bytes [...]". So we'll
	 * back off to 999 instead of 1024.
	 */
#define MAXHEADERSIZE (999)

	/* count the total number of potential folds, max of 2 bytes each */
	for(foldslop = 2, p = h; (p = strstr(p+1, "> <")); )
	  foldslop += 2;
	
	if((len=strlen(h)) + 1+id_len + foldslop >= MAXHEADERSIZE
	   && (p = strstr(h, "> <"))){
	    /*
	     * If the references line is so long that we are going to have
	     * to delete some of the references, delete the 2nd, 3rd, ...
	     * We don't want to delete the first message in the thread.
	     */
	    p[1] = '\0';
	    first_ref = cpystr(h);
	    first_ref_len = strlen(first_ref)+1;	/* len includes space */
	    p[1] = ' ';
	    tail_refs = p+2;
	    /* get rid of 2nd, 3rd, ... until it fits */
	    while((len=strlen(tail_refs)) + first_ref_len + 1+id_len +
						    foldslop >= MAXHEADERSIZE
		  && (p = strstr(tail_refs, "> <"))){
		tail_refs = p + 2;
		foldslop -= 2;
	    }

	    /*
	     * If the individual references are seriously long, somebody
	     * is messing with us and we don't care if it works right.
	     */
	    if((len=strlen(tail_refs)) + first_ref_len + 1+id_len +
						    foldslop >= MAXHEADERSIZE){
		first_ref_len = len = 0;
		tail_refs = NULL;
		if(first_ref)
		  fs_give((void **)&first_ref);
	    }
	}
	else
	  tail_refs = h;

	refs = (char *)fs_get((first_ref_len + 1+id_len + len + 1) *
							sizeof(char));
	snprintf(refs, first_ref_len + 1+id_len + len + 1, "%s%s%s%s%s",
		first_ref ? first_ref : "",
		first_ref ? " " : "",
		tail_refs ? tail_refs : "",
		tail_refs ? " " : "",
		env->message_id);
	refs[first_ref_len + 1+id_len + len] = '\0';
    }

    if(!refs && id_len)
      refs = cpystr(env->message_id);

    if(first_ref)
      fs_give((void **)&first_ref);

    return(refs);
}



/*----------------------------------------------------------------------
   Snoop for any Resent-* headers, and return an ADDRESS list

Args:  stream -- 
       msgno -- 

Returns: either NULL if no Resent-* or parsed ADDRESS struct list
  ---*/
ADDRESS *
reply_resent(struct pine *pine_state, long int msgno, char *section)
{
#define RESENTFROM 0
#define RESENTTO   1
#define RESENTCC   2
    ADDRESS	*rlist = NULL, **a, **b;
    char	*hdrs, *values[RESENTCC+1];
    int		 i;
    static char *fields[] = {"Resent-From", "Resent-To", "Resent-Cc", NULL};
    static char *fakedomain = "@";

    if((hdrs = pine_fetchheader_lines(pine_state->mail_stream,
				     msgno, section, fields)) != NULL){
	memset(values, 0, (RESENTCC+1) * sizeof(char *));
	simple_header_parse(hdrs, fields, values);
	for(i = RESENTFROM; i <= RESENTCC; i++)
	  rfc822_parse_adrlist(&rlist, values[i],
			       (F_ON(F_COMPOSE_REJECTS_UNQUAL, pine_state))
				 ? fakedomain : pine_state->maildomain);

	/* pare dup's ... */
	for(a = &rlist; *a; a = &(*a)->next)	/* compare every address */
	  for(b = &(*a)->next; *b; )		/* to the others	 */
	    if(address_is_same(*a, *b)){
		ADDRESS *t = *b;

		if(!(*a)->personal){		/* preserve personal name */
		    (*a)->personal = (*b)->personal;
		    (*b)->personal = NULL;
		}

		*b	= t->next;
		t->next = NULL;
		mail_free_address(&t);
	    }
	    else
	      b = &(*b)->next;
    }

    if(hdrs)
      fs_give((void **) &hdrs);
    
    return(rlist);
}


/*----------------------------------------------------------------------
    Format and return subject suitable for the reply command

Args:  subject -- subject to build reply subject for
       buf -- buffer to use for writing.  If non supplied, alloc one.
       buflen -- length of buf if supplied, else ignored

Returns: with either "Re:" prepended or not, if already there.
         returned string is allocated.
  ---*/
char *
reply_subject(char *subject, char *buf, size_t buflen)
{
    size_t  l   = (subject && *subject) ? 4*strlen(subject) : 10;
    char   *tmp = fs_get(l + 1), *decoded, *p;

    if(!buf){
	buflen = l + 5;
	buf = fs_get(buflen);
    }

    /* decode any 8bit into tmp buffer */
    decoded = (char *) rfc1522_decode_to_utf8((unsigned char *)tmp, l+1, subject);

    buf[0] = '\0';
    if(decoded					/* already "re:" ? */
       && (decoded[0] == 'R' || decoded[0] == 'r')
       && (decoded[1] == 'E' || decoded[1] == 'e')){

        if(decoded[2] == ':')
	  snprintf(buf, buflen, "%.*s", buflen-1, subject);
	else if((decoded[2] == '[') && (p = strchr(decoded, ']'))){
	    p++;
	    while(*p && isspace((unsigned char)*p)) p++;
	    if(p[0] == ':')
	      snprintf(buf, buflen, "%.*s", buflen-1, subject);
	}
    }

    if(!buf[0])
      snprintf(buf, buflen, "Re: %.*s", buflen-1,
	      (subject && *subject) ? subject : "your mail");

    buf[buflen-1] = '\0';

    fs_give((void **) &tmp);
    return(buf);
}


/*----------------------------------------------------------------------
    return initials for the given personal name

Args:  name -- Personal name to extract initials from

Returns: pointer to name overwritten with initials
  ---*/
char *
reply_quote_initials(char *name)
{
    char *s = name,
         *w = name;
    
    /* while there are still characters to look at */
    while(s && *s){
	/* skip to next initial */
	while(*s && isspace((unsigned char) *s))
	  s++;
	
	/* skip over cruft like single quotes */
	while(*s && !isalnum((unsigned char) *s))
	  s++;

	/* copy initial */
	if(*s)
	  *w++ = *s++;
	
	/* skip to end of this piece of name */
	while(*s && !isspace((unsigned char) *s))
	  s++;
    }

    if(w)
      *w = '\0';

    return(name);
}

/*
 * There is an assumption that MAX_SUBSTITUTION is <= the size of the
 * tokens being substituted for (only in the size of buf below).
 */
#define MAX_SUBSTITUTION 6
#define MAX_PREFIX 63
static char *from_token = "_FROM_";
static char *nick_token = "_NICK_";
static char *init_token = "_INIT_";

/*----------------------------------------------------------------------
    return a quoting string, "> " by default, for replied text

Args:  env -- envelope of message being replied to

Returns: malloc'd array containing quoting string, freed by caller
  ---*/
char *
reply_quote_str(ENVELOPE *env)
{
    char *prefix, *repl, *p, buf[MAX_PREFIX+1], pbf[MAX_SUBSTITUTION+1];

    strncpy(buf, ps_global->VAR_REPLY_STRING, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';

    /* set up the prefix to quote included text */
    if((p = strstr(buf, from_token)) != NULL){
	repl = (env && env->from && env->from->mailbox) ? env->from->mailbox
							: "";
	strncpy(pbf, repl, sizeof(pbf)-1);
	pbf[sizeof(pbf)-1] = '\0';;
	rplstr(p, sizeof(buf)-(p-buf), strlen(from_token), pbf);
    }
    
    if((p = strstr(buf, nick_token)) != NULL){
	repl = (env &&
		env->from &&
		env->from &&
		get_nickname_from_addr(env->from, tmp_20k_buf, 1000))
		 ? tmp_20k_buf : "";
	strncpy(pbf, repl, sizeof(pbf)-1);
	pbf[sizeof(pbf)-1] = '\0';;
	rplstr(p, sizeof(buf)-(p-buf), strlen(nick_token), pbf);
    }

    if((p = strstr(buf, init_token)) != NULL){
	char *q = NULL;
	char  buftmp[MAILTMPLEN];

	snprintf(buftmp, sizeof(buftmp), "%.200s",
	 (env && env->from && env->from->personal) ? env->from->personal : "");
	buftmp[sizeof(buftmp)-1] = '\0';

	repl = (env && env->from && env->from->personal)
		 ? reply_quote_initials(q = cpystr((char *)rfc1522_decode_to_utf8(
						(unsigned char *)tmp_20k_buf, 
						SIZEOF_20KBUF, buftmp)))
		 : "";

	istrncpy(pbf, repl, sizeof(pbf)-1);
	pbf[sizeof(pbf)-1] = '\0';;
	rplstr(p, sizeof(buf)-(p-buf), strlen(init_token), pbf);
	if(q)
	  fs_give((void **)&q);
    }
    
    prefix = removing_quotes(cpystr(buf));

    return(prefix);
}

int
reply_quote_str_contains_tokens(void)
{
    return(ps_global->VAR_REPLY_STRING && ps_global->VAR_REPLY_STRING[0] &&
	   (strstr(ps_global->VAR_REPLY_STRING, from_token) ||
	    strstr(ps_global->VAR_REPLY_STRING, nick_token) ||
	    strstr(ps_global->VAR_REPLY_STRING, init_token)));
}


/*----------------------------------------------------------------------
  Build the body for the message number/part being replied to

    Args: 

  Result: BODY structure suitable for sending

  ----------------------------------------------------------------------*/
BODY *
reply_body(MAILSTREAM *stream, ENVELOPE *env, struct mail_bodystruct *orig_body,
	   long int msgno, char *sect_prefix, void *msgtext, char *prefix,
	   int plustext, ACTION_S *role, int toplevel, REDRAFT_POS_S **redraft_pos)
{
    char     *p, *sig = NULL, *section, sect_buf[256];
    BODY     *body = NULL, *tmp_body = NULL;
    PART     *part;
    gf_io_t   pc;
    int       impl, template_len = 0, leave_cursor_at_top = 0, reply_raw_body = 0;

    if(sect_prefix)
      snprintf(section = sect_buf, sizeof(sect_buf), "%.*s.1", sizeof(sect_buf)-1, sect_prefix);
    else
      section = "1";

    sect_buf[sizeof(sect_buf)-1] = '\0';

    if(ps_global->full_header == 2
       && F_ON(F_ENABLE_FULL_HDR_AND_TEXT, ps_global))
      reply_raw_body = 1;

    gf_set_so_writec(&pc, (STORE_S *) msgtext);

    if(toplevel){
	char *filtered;

	impl = 0;
	filtered = detoken(role, env, 0,
			   F_ON(F_SIG_AT_BOTTOM, ps_global) ? 1 : 0,
			   0, redraft_pos, &impl);
	if(filtered){
	    if(*filtered){
		so_puts((STORE_S *)msgtext, filtered);
		if(impl == 1)
		  template_len = strlen(filtered);
		else if(impl == 2)
		  leave_cursor_at_top++;
	    }
	    
	    fs_give((void **)&filtered);
	}
	else
	  impl = 1;
    }
    else
      impl = 1;

    if(toplevel &&
       (sig = reply_signature(role, env, redraft_pos, &impl)) &&
       F_OFF(F_SIG_AT_BOTTOM, ps_global)){

	/*
	 * If CURSORPOS was set explicitly in sig_file, and there was a
	 * template file before that, we need to adjust the offset by the
	 * length of the template file. However, if the template had
	 * a set CURSORPOS in it then impl was 2 before getting to the
	 * signature, so offset wouldn't have been reset by the signature
	 * CURSORPOS and offset would already be correct. That case will
	 * be ok here because template_len will be 0 and adding it does
	 * nothing. If template
	 * didn't have CURSORPOS in it, then impl was 1 and got set to 2
	 * by the CURSORPOS in the sig. In that case we have to adjust the
	 * offset. That's what the next line does. It adjusts it if
	 * template_len is nonzero and if CURSORPOS was set in sig_file.
	 */
	if(impl == 2)
	  (*redraft_pos)->offset += template_len;
	
	if(*sig)
	  so_puts((STORE_S *)msgtext, sig);
	
	/*
	 * Set sig to NULL because we've already used it. If SIG_AT_BOTTOM
	 * is set, we won't have used it yet and want it to be non-NULL.
	 */
	fs_give((void **)&sig);
    }

    /*
     * Only put cursor in sig if there is a cursorpos there but not
     * one in the template, and sig-at-bottom.
     */
    if(!(sig && impl == 2 && !leave_cursor_at_top))
      leave_cursor_at_top++;

    if(plustext){
	if(!orig_body
	   || orig_body->type == TYPETEXT
	   || reply_raw_body
	   || F_OFF(F_ATTACHMENTS_IN_REPLY, ps_global)){
	    char *charset = NULL;

	    /*------ Simple text-only message ----*/
	    body		     = mail_newbody();
	    body->type		     = TYPETEXT;
	    body->contents.text.data = msgtext;
	    reply_delimiter(env, role, pc);
	    if(F_ON(F_INCLUDE_HEADER, ps_global))
	      reply_forward_header(stream, msgno, sect_prefix,
				   env, pc, prefix);

	    if(!orig_body || reply_raw_body || reply_body_text(orig_body, &tmp_body)){
		BODY *bodyp = NULL;

		bodyp = reply_raw_body ? NULL : tmp_body;

		/*
		 * We set the charset in the outgoing message to the same
		 * as the one in the message we're replying to unless it
		 * is the unknown charset. We do that in order to attempt
		 * to preserve the same charset in the reply if possible.
		 * It may be safer to just set it to whatever we want instead
		 * but then the receiver may not be able to read it.
		 */
		if(bodyp
		   && (charset = parameter_val(bodyp->parameter, "charset"))
		   && strucmp(charset, UNKNOWN_CHARSET))
		  set_parameter(&body->parameter, "charset", charset);

		if(charset)
		  fs_give((void **) &charset);

		get_body_part_text(stream, bodyp, msgno,
				   bodyp ? (p = body_partno(stream, msgno, bodyp))
					 : sect_prefix,
				   0L, pc, prefix, NULL, GBPT_NONE);
		if(bodyp && p)
		  fs_give((void **) &p);
	    }
	    else{
		gf_puts(NEWLINE, pc);
		gf_puts("  [NON-Text Body part not included]", pc);
		gf_puts(NEWLINE, pc);
	    }
	}
	else if(orig_body->type == TYPEMULTIPART){
	    /*------ Message is Multipart ------*/
	    if(orig_body->subtype
	       && !strucmp(orig_body->subtype, "signed")
	       && orig_body->nested.part){
		/* operate only on the signed part */
		body = reply_body(stream, env,
				  &orig_body->nested.part->body,
				  msgno, section, msgtext, prefix,
				  plustext, role, 0, redraft_pos);
	    }
	    else if(orig_body->subtype
		    && !strucmp(orig_body->subtype, "alternative")){
		/* Set up the simple text reply */
		body			 = mail_newbody();
		body->type		 = TYPETEXT;
		body->contents.text.data = msgtext;

		if(reply_body_text(orig_body, &tmp_body)){
		    reply_delimiter(env, role, pc);
		    if(F_ON(F_INCLUDE_HEADER, ps_global))
		      reply_forward_header(stream, msgno, sect_prefix,
					   env, pc, prefix);

		    get_body_part_text(stream, tmp_body, msgno,
				       p = body_partno(stream,msgno,tmp_body),
				       0L, pc, prefix, NULL, GBPT_NONE);
		    if(p)
		      fs_give((void **) &p);
		}
		else
		  q_status_message(SM_ORDER | SM_DING, 3, 3,
			    "No suitable multipart text found for inclusion!");
	    }
	    else{
		body = copy_body(NULL, orig_body);

		/*
		 * whatever subtype it is, demote it
		 * to plain old MIXED.
		 */
		if(body->subtype)
		  fs_give((void **) &body->subtype);

		body->subtype = cpystr("Mixed");

		if(body->nested.part &&
		   body->nested.part->body.type == TYPETEXT) {
		    char *new_charset = NULL;

		    /*---- First part of the message is text -----*/
		    body->nested.part->body.contents.text.data = msgtext;
		    if(body->nested.part->body.subtype &&
		       strucmp(body->nested.part->body.subtype, "Plain")){
		        fs_give((void **)&body->nested.part->body.subtype);
		        body->nested.part->body.subtype = cpystr("Plain");
		    }
		    reply_delimiter(env, role, pc);
		    if(F_ON(F_INCLUDE_HEADER, ps_global))
		      reply_forward_header(stream, msgno, sect_prefix,
					   env, pc, prefix);

		    if(!(get_body_part_text(stream,
					    &orig_body->nested.part->body,
					    msgno, section, 0L, pc, prefix,
					    &new_charset, GBPT_NONE)
			 && fetch_contents(stream, msgno, sect_prefix, body)))
		      q_status_message(SM_ORDER | SM_DING, 3, 4,
				       _("Error including all message parts"));
		    else if(new_charset)
		      set_parameter(&body->nested.part->body.parameter, "charset", new_charset);
		}
		else if(orig_body->nested.part->body.type == TYPEMULTIPART
			&& orig_body->nested.part->body.subtype
			&& !strucmp(orig_body->nested.part->body.subtype,
				    "alternative")
			&& reply_body_text(&orig_body->nested.part->body,
					   &tmp_body)){
		    int partnum;

		    reply_delimiter(env, role, pc);
		    if(F_ON(F_INCLUDE_HEADER, ps_global))
		      reply_forward_header(stream, msgno, sect_prefix,
					   env, pc, prefix);

		    snprintf(sect_buf, sizeof(sect_buf), "%.*s%s%.*s",
			    sizeof(sect_buf)/2-2,
			    sect_prefix ? sect_prefix : "",
			    sect_prefix ? "." : "",
			    sizeof(sect_buf)/2-2,
			    p = partno(orig_body, tmp_body));
		    sect_buf[sizeof(sect_buf)-1] = '\0';
		    fs_give((void **) &p);
		    get_body_part_text(stream, tmp_body, msgno,
				       sect_buf, 0L, pc, prefix,
				       NULL, GBPT_NONE);

		    part = body->nested.part->next;
		    body->nested.part->next = NULL;
		    mail_free_body_part(&body->nested.part);
		    body->nested.part = mail_newbody_part();
		    body->nested.part->body.type = TYPETEXT;
		    body->nested.part->body.subtype = cpystr("Plain");
		    body->nested.part->body.contents.text.data = msgtext;
		    body->nested.part->next = part;

		    for(partnum = 2; part != NULL; part = part->next){
			snprintf(sect_buf, sizeof(sect_buf), "%.*s%s%d",
				sizeof(sect_buf)/2,
				sect_prefix ? sect_prefix : "",
				sect_prefix ? "." : "", partnum++);
			sect_buf[sizeof(sect_buf)-1] = '\0';

			if(!fetch_contents(stream, msgno,
					   sect_buf, &part->body)){
			    break;
			}
		    }
		}
		else {
		    /*--- Fetch the original pieces ---*/
		    if(!fetch_contents(stream, msgno, sect_prefix, body))
		      q_status_message(SM_ORDER | SM_DING, 3, 4,
				       _("Error including all message parts"));

		    /*--- No text part, create a blank one ---*/
		    part			  = mail_newbody_part();
		    part->next			  = body->nested.part;
		    body->nested.part		  = part;
		    part->body.contents.text.data = msgtext;
		}
	    }
	}
	else{
	    /*---- Single non-text message of some sort ----*/
	    body              = mail_newbody();
	    body->type        = TYPEMULTIPART;
	    part              = mail_newbody_part();
	    body->nested.part = part;
    
	    /*--- The first part, a blank text part to be edited ---*/
	    part->body.type		  = TYPETEXT;
	    part->body.contents.text.data = msgtext;

	    /*--- The second part, what ever it is ---*/
	    part->next    = mail_newbody_part();
	    part          = part->next;
	    part->body.id = generate_message_id();
	    copy_body(&(part->body), orig_body);

	    /*
	     * the idea here is to fetch part into storage object
	     */
	    if((part->body.contents.text.data = (void *) so_get(PART_SO_TYPE,
							    NULL,EDIT_ACCESS)) != NULL){
		if((p = pine_mail_fetch_body(stream, msgno, section,
					    &part->body.size.bytes, NIL)) != NULL){
		    so_nputs((STORE_S *)part->body.contents.text.data,
			     p, part->body.size.bytes);
		}
		else
		  mail_free_body(&body);
	    }
	    else
	      mail_free_body(&body);
	}
    }
    else{
	/*--------- No text included --------*/
	body			 = mail_newbody();
	body->type		 = TYPETEXT;
	body->contents.text.data = msgtext;
    }

    if(!leave_cursor_at_top){
	long  cnt = 0L;
	unsigned char c;

	/* rewind and count chars to start of sig file */
	so_seek((STORE_S *)msgtext, 0L, 0);
	while(so_readc(&c, (STORE_S *)msgtext))
	  cnt++;

	if(!*redraft_pos){
	    *redraft_pos = (REDRAFT_POS_S *)fs_get(sizeof(**redraft_pos));
	    memset((void *)*redraft_pos, 0,sizeof(**redraft_pos));
	    (*redraft_pos)->hdrname = cpystr(":");
	}

	/*
	 * If explicit cursor positioning in sig file,
	 * add offset to start of sig file plus offset into sig file.
	 * Else, just offset to start of sig file.
	 */
	(*redraft_pos)->offset += cnt;
    }

    if(sig){
	if(*sig)
	  so_puts((STORE_S *)msgtext, sig);
	
	fs_give((void **)&sig);
    }

    gf_clear_so_writec((STORE_S *) msgtext);

    return(body);
}


/*
 * reply_part - first replyable multipart of a multipart.
 */
int
reply_body_text(struct mail_bodystruct *body, struct mail_bodystruct **new_body)
{
    if(body){
	switch(body->type){
	  case TYPETEXT :
	    *new_body = body;
	    return(1);

	  case TYPEMULTIPART :
	    if(body->subtype && !strucmp(body->subtype, "alternative")){
		PART *part;
		int got_one = 0;

		if(ps_global->force_prefer_plain
		   || (!ps_global->force_no_prefer_plain
		       && F_ON(F_PREFER_PLAIN_TEXT, ps_global))){
		    for(part = body->nested.part; part; part = part->next)
		      if((!part->body.type || part->body.type == TYPETEXT)
			 && (!part->body.subtype
			     || !strucmp(part->body.subtype, "plain"))){
			  *new_body = &part->body;
			  return(1);
		      }
		}

		/*
		 * Else choose last alternative among plain or html parts.
		 * Perhaps we should really be using mime_show() to make this
		 * potentially more general than just plain or html.
		 */
		for(part = body->nested.part; part; part = part->next){
		    if((!part->body.type || part->body.type == TYPETEXT)
		        && ((!part->body.subtype || !strucmp(part->body.subtype, "plain"))
			    ||
			    (part->body.subtype && !strucmp(part->body.subtype, "html")))){
			got_one++;
			*new_body = &part->body;
		    }
		}

		if(got_one)
		  return(1);
	    }
	    else if(body->nested.part)
	      /* NOTE: we're only interested in "first" part of mixed */
	      return(reply_body_text(&body->nested.part->body, new_body));

	    break;

	  default:
	    break;
	}
    }

    return(0);
}


char *
reply_signature(ACTION_S *role, ENVELOPE *env, REDRAFT_POS_S **redraft_pos, int *impl)
{
    char *sig;
    size_t l;

    sig = detoken(role, env,
		  2, F_ON(F_SIG_AT_BOTTOM, ps_global) ? 0 : 1, 1,
		  redraft_pos, impl);

    if(F_OFF(F_SIG_AT_BOTTOM, ps_global) && (!sig || !*sig)){
	if(sig)
	  fs_give((void **)&sig);

	l = 2 * strlen(NEWLINE);
	sig = (char *)fs_get((l+1) * sizeof(char));
	strncpy(sig, NEWLINE, l);
	sig[l] = '\0';
	strncat(sig, NEWLINE, l+1-1-strlen(sig));
	sig[l] = '\0';
	return(sig);
    }

    return(sig);
}


/*
 * Buf is at least size maxlen+1
 */
void
get_addr_data(ENVELOPE *env, IndexColType type, char *buf, size_t maxlen)
{
    ADDRESS *addr = NULL;
    ADDRESS *last_to = NULL;
    ADDRESS *first_addr = NULL, *second_addr = NULL;
    ADDRESS *third_addr = NULL, *fourth_addr = NULL;
    int      cntaddr, l;
    size_t   orig_maxlen;
    char    *p;

    buf[0] = '\0';

    switch(type){
      case iFrom:
	addr = env ? env->from : NULL;
	break;

      case iTo:
	addr = env ? env->to : NULL;
	break;

      case iCc:
	addr = env ? env->cc : NULL;
	break;

      case iSender:
	addr = env ? env->sender : NULL;
	break;

      /*
       * Recips is To and Cc togeter. We hook the two adrlists together
       * temporarily.
       */
      case iRecips:
	addr = env ? env->to : NULL;
	/* Find end of To list */
	for(last_to = addr; last_to && last_to->next; last_to = last_to->next)
	  ;
	
	/* Make the end of To list point to cc list */
	if(last_to)
	  last_to->next = (env ? env->cc : NULL);
	  
	break;

      /*
       * Initials.
       */
      case iInit:
	if(env && env->from && env->from->personal){
	    char *name, *initials = NULL;

	    name = (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf,
						  SIZEOF_20KBUF, env->from->personal);
	    if(name == env->from->personal){
		strncpy(tmp_20k_buf, name, SIZEOF_20KBUF-1);
		tmp_20k_buf[SIZEOF_20KBUF - 1] = '\0';
		name = tmp_20k_buf;
	    }

	    if(name && *name){
		initials = reply_quote_initials(name);
		iutf8ncpy(buf, initials, maxlen);
		buf[maxlen] = '\0';
	    }
	}

	return;

      default:
	break;
    }

    orig_maxlen = maxlen;

    first_addr = addr;
    /* skip over rest of c-client group addr */
    if(first_addr && first_addr->mailbox && !first_addr->host){
	for(second_addr = first_addr->next;
	    second_addr && second_addr->host;
	    second_addr = second_addr->next)
	  ;
	
	if(second_addr && !second_addr->host)
	  second_addr = second_addr->next;
    }
    else if(!(first_addr && first_addr->host && first_addr->host[0] == '.'))
      second_addr = first_addr ? first_addr->next : NULL;

    if(second_addr && second_addr->mailbox && !second_addr->host){
	for(third_addr = second_addr->next;
	    third_addr && third_addr->host;
	    third_addr = third_addr->next)
	  ;
	
	if(third_addr && !third_addr->host)
	  third_addr = third_addr->next;
    }
    else if(!(second_addr && second_addr->host && second_addr->host[0] == '.'))
      third_addr = second_addr ? second_addr->next : NULL;

    if(third_addr && third_addr->mailbox && !third_addr->host){
	for(fourth_addr = third_addr->next;
	    fourth_addr && fourth_addr->host;
	    fourth_addr = fourth_addr->next)
	  ;
	
	if(fourth_addr && !fourth_addr->host)
	  fourth_addr = fourth_addr->next;
    }
    else if(!(third_addr && third_addr->host && third_addr->host[0] == '.'))
      fourth_addr = third_addr ? third_addr->next : NULL;

    /* Just attempting to make a nice display */
    if(first_addr && ((first_addr->personal && first_addr->personal[0]) ||
		      (first_addr->mailbox && first_addr->mailbox[0]))){
      if(second_addr){
	if((second_addr->personal && second_addr->personal[0]) ||
	   (second_addr->mailbox && second_addr->mailbox[0])){
	  if(third_addr){
	    if((third_addr->personal && third_addr->personal[0]) ||
	       (third_addr->mailbox && third_addr->mailbox[0])){
	      if(fourth_addr)
	        cntaddr = 4;
	      else
		cntaddr = 3;
	    }
	    else
	      cntaddr = -1;
	  }
	  else
	    cntaddr = 2;
	}
	else
	  cntaddr = -1;
      }
      else
	cntaddr = 1;
    }
    else
      cntaddr = -1;

    p = buf;
    if(cntaddr == 1)
      a_little_addr_string(first_addr, p, maxlen);
    else if(cntaddr == 2){
	a_little_addr_string(first_addr, p, maxlen);
	maxlen -= (l=strlen(p));
	p += l;
	if(maxlen > 7){
	    strncpy(p, " and ", maxlen);
	    maxlen -= 5;
	    p += 5;
	    a_little_addr_string(second_addr, p, maxlen);
	}
    }
    else if(cntaddr == 3){
	a_little_addr_string(first_addr, p, maxlen);
	maxlen -= (l=strlen(p));
	p += l;
	if(maxlen > 7){
	    strncpy(p, ", ", maxlen);
	    maxlen -= 2;
	    p += 2;
	    a_little_addr_string(second_addr, p, maxlen);
	    maxlen -= (l=strlen(p));
	    p += l;
	    if(maxlen > 7){
		strncpy(p, ", and ", maxlen);
		maxlen -= 6;
		p += 6;
		a_little_addr_string(third_addr, p, maxlen);
	    }
	}
    }
    else if(cntaddr > 3){
	a_little_addr_string(first_addr, p, maxlen);
	maxlen -= (l=strlen(p));
	p += l;
	if(maxlen > 7){
	    strncpy(p, ", ", maxlen);
	    maxlen -= 2;
	    p += 2;
	    a_little_addr_string(second_addr, p, maxlen);
	    maxlen -= (l=strlen(p));
	    p += l;
	    if(maxlen > 7){
		strncpy(p, ", ", maxlen);
		maxlen -= 2;
		p += 2;
		a_little_addr_string(third_addr, p, maxlen);
		maxlen -= (l=strlen(p));
		p += l;
		if(maxlen >= 12)
		  strncpy(p, ", and others", maxlen);
		else if(maxlen >= 3)
		  strncpy(p, "...", maxlen);
	    }
	}
    }
    else if(addr){
	char *a_string;

	a_string = addr_list_string(addr, NULL, 0);
	iutf8ncpy(buf, a_string, maxlen);

	fs_give((void **)&a_string);
    }

    if(last_to)
      last_to->next = NULL;

    buf[orig_maxlen] = '\0';
}


/*
 * Buf is at least size maxlen+1
 */
void
get_news_data(ENVELOPE *env, IndexColType type, char *buf, size_t maxlen)
{
    int      cntnews = 0, orig_maxlen;
    char    *news = NULL, *p, *q;

    switch(type){
      case iNews:
      case iNewsAndTo:
      case iToAndNews:
      case iNewsAndRecips:
      case iRecipsAndNews:
	news = env ? env->newsgroups : NULL;
	break;

      case iCurNews:
	if(ps_global->mail_stream && IS_NEWS(ps_global->mail_stream))
	  news = ps_global->cur_folder;

	break;

      default:
	break;
    }

    orig_maxlen = maxlen;

    if(news){
	q = news;
	while(isspace((unsigned char)*q))
	  q++;

	if(*q)
	  cntnews++;
	
	while((q = strindex(q, ',')) != NULL){
	    q++;
	    while(isspace((unsigned char)*q))
	      q++;
	    
	    if(*q)
	      cntnews++;
	    else
	      break;
	}
    }

    if(cntnews == 1){
	istrncpy(buf, news, maxlen);
	buf[maxlen] = '\0';
	removing_leading_and_trailing_white_space(buf);
    }
    else if(cntnews == 2){
	p = buf;
	q = news;
	while(isspace((unsigned char)*q))
	  q++;
	
	while(maxlen > 0 && *q && !isspace((unsigned char)*q) && *q != ','){
	    *p++ = *q++;
	    maxlen--;
	}
	
	if(maxlen > 7){
	    strncpy(p, " and ", maxlen);
	    p += 5;
	    maxlen -= 5;
	}

	while(isspace((unsigned char)*q) || *q == ',')
	  q++;

	while(maxlen > 0 && *q && !isspace((unsigned char)*q) && *q != ','){
	    *p++ = *q++;
	    maxlen--;
	}

	*p = '\0';

	istrncpy(tmp_20k_buf, buf, 10000);
	strncpy(buf, tmp_20k_buf, orig_maxlen);
    }
    else if(cntnews > 2){
	char b[100];

	p = buf;
	q = news;
	while(isspace((unsigned char)*q))
	  q++;
	
	while(maxlen > 0 && *q && !isspace((unsigned char)*q) && *q != ','){
	    *p++ = *q++;
	    maxlen--;
	}
	
	*p = '\0';
	snprintf(b, sizeof(b), " and %d other newsgroups", cntnews-1);
	b[sizeof(b)-1] = '\0';
	if(maxlen >= strlen(b))
	  strncpy(p, b, maxlen);
	else if(maxlen >= 3)
	  strncpy(p, "...", maxlen);

	buf[orig_maxlen] = '\0';

	istrncpy(tmp_20k_buf, buf, 10000);
	tmp_20k_buf[10000-1] = '\0';
	strncpy(buf, tmp_20k_buf, orig_maxlen);
    }

    buf[orig_maxlen] = '\0';
}


/*
 * Buf is at least size maxlen+1
 */
char *
get_reply_data(ENVELOPE *env, ACTION_S *role, IndexColType type, char *buf, size_t maxlen)
{
    char        *space = NULL;
    IndexColType addrtype;

    buf[0] = '\0';

    switch(type){
      case iRDate: case iSDate: case iSTime:
      case iS1Date: case iS2Date: case iS3Date: case iS4Date:
      case iSDateIso: case iSDateIsoS:
      case iSDateS1: case iSDateS2: case iSDateS3: case iSDateS4: 
      case iSDateTime:
      case iSDateTimeIso: case iSDateTimeIsoS:
      case iSDateTimeS1: case iSDateTimeS2: case iSDateTimeS3: case iSDateTimeS4: 
      case iSDateTime24:
      case iSDateTimeIso24: case iSDateTimeIsoS24:
      case iSDateTimeS124: case iSDateTimeS224: case iSDateTimeS324: case iSDateTimeS424: 
      case iDateIso: case iDateIsoS: case iTime24: case iTime12:
      case iDay: case iDayOrdinal: case iDay2Digit:
      case iMonAbb: case iMonLong: case iMon: case iMon2Digit:
      case iYear: case iYear2Digit:
      case iDate: case iLDate:
      case iTimezone: case iDayOfWeekAbb: case iDayOfWeek:
      case iPrefDate: case iPrefTime: case iPrefDateTime:
	if(env && env->date && env->date[0] && maxlen >= 20)
	  date_str((char *) env->date, type, 1, buf, maxlen+1, 0);

	break;

      case iCurDate:
      case iCurDateIso:
      case iCurDateIsoS:
      case iCurTime24:
      case iCurTime12:
      case iCurDay:
      case iCurDay2Digit:
      case iCurDayOfWeek:
      case iCurDayOfWeekAbb:
      case iCurMon:
      case iCurMon2Digit:
      case iCurMonLong:
      case iCurMonAbb:
      case iCurYear:
      case iCurYear2Digit:
      case iCurPrefDate:
      case iCurPrefDateTime:
      case iCurPrefTime:
      case iLstMon:
      case iLstMon2Digit:
      case iLstMonLong:
      case iLstMonAbb:
      case iLstMonYear:
      case iLstMonYear2Digit:
      case iLstYear:
      case iLstYear2Digit:
	if(maxlen >= 20)
	  date_str(NULL, type, 1, buf, maxlen+1, 0);

	break;

      case iFrom:
      case iTo:
      case iCc:
      case iSender:
      case iRecips:
      case iInit:
	get_addr_data(env, type, buf, maxlen);
	break;

      case iRoleNick:
	if(role && role->nick){
	    strncpy(buf, role->nick, maxlen);
	    buf[maxlen] = '\0';
	}
	break;

      case iNewLine:
	if(maxlen >= strlen(NEWLINE)){
	    strncpy(buf, NEWLINE, maxlen);
	    buf[maxlen] = '\0';
	}
	break;

      case iAddress:
      case iMailbox:
	if(env && env->from && env->from->mailbox && env->from->mailbox[0] &&
	   strlen(env->from->mailbox) <= maxlen){
	    strncpy(buf, env->from->mailbox, maxlen);
	    buf[maxlen] = '\0';
	    if(type == iAddress &&
	       env->from->host &&
	       env->from->host[0] &&
	       env->from->host[0] != '.' &&
	       strlen(buf) + strlen(env->from->host) + 1 <= maxlen){
		strncat(buf, "@", maxlen+1-1-strlen(buf));
		buf[maxlen] = '\0';
		strncat(buf, env->from->host, maxlen+1-1-strlen(buf));
		buf[maxlen] = '\0';
	    }
	}

	break;

      case iNews:
      case iCurNews:
	get_news_data(env, type, buf, maxlen);
	break;

      case iToAndNews:
      case iNewsAndTo:
      case iRecipsAndNews:
      case iNewsAndRecips:
	if(type == iToAndNews || type == iNewsAndTo)
	  addrtype = iTo;
	else
	  addrtype = iRecips;

	if(env && env->newsgroups){
	    space = (char *)fs_get((maxlen+1) * sizeof(char));
	    get_news_data(env, type, space, maxlen);
	}

	get_addr_data(env, addrtype, buf, maxlen);

	if(space && *space && *buf){
	    if(strlen(space) + strlen(buf) + 5 > maxlen){
		if(strlen(space) > maxlen/2)
		  get_news_data(env, type, space, maxlen - strlen(buf) - 5);
		else
		  get_addr_data(env, addrtype, buf, maxlen - strlen(space) - 5);
	    }

	    if(type == iToAndNews || type == iRecipsAndNews)
	      snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%s and %s", buf, space);
	    else
	      snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%s and %s", space, buf);

	    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';

	    strncpy(buf, tmp_20k_buf, maxlen);
	    buf[maxlen] = '\0';
	}
	else if(space && *space){
	    strncpy(buf, space, maxlen);
	    buf[maxlen] = '\0';
	}
	
	if(space)
	  fs_give((void **)&space);

	break;

      case iSubject:
	if(env && env->subject){
	    size_t n, len;
	    unsigned char *p, *tmp = NULL;

	    if((n = 4*strlen(env->subject)) > SIZEOF_20KBUF-1){
		len = n+1;
		p = tmp = (unsigned char *)fs_get(len * sizeof(char));
	    }
	    else{
		len = SIZEOF_20KBUF;
		p = (unsigned char *)tmp_20k_buf;
	    }
	  
	    istrncpy(buf, (char *)rfc1522_decode_to_utf8(p, len, env->subject), maxlen);

	    buf[maxlen] = '\0';

	    if(tmp)
	      fs_give((void **)&tmp);
	}

	break;
    
      case iMsgID:
	if(env && env->message_id){
	    strncpy(buf, env->message_id, maxlen);
	    buf[maxlen] = '\0';
	}

	break;
    
      default:
	break;
    }

    buf[maxlen] = '\0';
    return(buf);
}


/*
 * reply_delimiter - output formatted reply delimiter for given envelope
 *		     with supplied character writing function.
 */
void
reply_delimiter(ENVELOPE *env, ACTION_S *role, gf_io_t pc)
{
#define MAX_DELIM 2000
    char           buf[MAX_DELIM+1];
    char          *p;
    char          *filtered = NULL;
    int            contains_newline_token = 0;


    if(!env)
      return;

    strncpy(buf, ps_global->VAR_REPLY_INTRO, MAX_DELIM);
    buf[MAX_DELIM] = '\0';
    /* preserve exact default behavior from before */
    if(!strcmp(buf, DEFAULT_REPLY_INTRO)){
	struct date d;
	int include_date;

	parse_date((char *) env->date, &d);
	include_date = !(d.day == -1 || d.month == -1 || d.year == -1);
	if(include_date){
	    gf_puts("On ", pc);			/* All delims have... */
	    if(d.wkday != -1){			/* "On day, date month year" */
		gf_puts(day_abbrev(d.wkday), pc);	/* in common */
		gf_puts(", ", pc);
	    }

	    gf_puts(int2string(d.day), pc);
	    (*pc)(' ');
	    gf_puts(month_abbrev(d.month), pc);
	    (*pc)(' ');
	    gf_puts(int2string(d.year), pc);
	}

	if(env->from
	   && ((env->from->personal && env->from->personal[0])
	       || (env->from->mailbox && env->from->mailbox[0]))){
	    char buftmp[MAILTMPLEN];

	    a_little_addr_string(env->from, buftmp, sizeof(buftmp)-1);
	    if(include_date)
	      gf_puts(", ", pc);

	    gf_puts(buftmp, pc);
	    gf_puts(" wrote:", pc);
	}
	else{
	    if(include_date)
	      gf_puts(", it was written", pc);
	    else
	      gf_puts("It was written", pc);
	}

    }
    else{
	/*
	 * This is here for backwards compatibility. There didn't used
	 * to be a _NEWLINE_ token. The user would enter text that should
	 * all fit on one line and then that was followed by two newlines.
	 * Also, truncation occurs if it is long.
	 * Now, if _NEWLINE_ is not in the text, same thing still works
	 * the same. However, if _NEWLINE_ is in there, then all bets are
	 * off and the user is on his or her own. No automatic newlines
	 * are added, only those that come from the tokens. No truncation
	 * is done, the user is trusted to get it right. Newlines may be
	 * embedded so that the leadin is multi-line.
	 */
	contains_newline_token = (strstr(buf, "_NEWLINE_") != NULL);
	filtered = detoken_src(buf, FOR_REPLY_INTRO, env, role,
			       NULL, NULL);

	/* try to truncate if too long */
	if(!contains_newline_token && filtered && utf8_width(filtered) > 80){
	    int ended_with_colon = 0;
	    int ended_with_quote = 0;
	    int ended_with_quote_colon = 0;
	    int l;

	    l = strlen(filtered);

	    if(filtered[l-1] == ':'){
		ended_with_colon = ':';
		if(filtered[l-2] == QUOTE || filtered[l-2] == '\'')
		  ended_with_quote_colon = filtered[l-2];
	    }
	    else if(filtered[l-1] == QUOTE || filtered[l-1] == '\'')
	      ended_with_quote = filtered[l-1];

	    /* try to find space to break at */
	    for(p = &filtered[75]; p > &filtered[60] && 
		  !isspace((unsigned char)*p); p--)
	      ;
	    
	    if(!isspace((unsigned char)*p))
	      p = &filtered[75];

	    *p++ = '.';
	    *p++ = '.';
	    *p++ = '.';
	    if(ended_with_quote_colon){
		*p++ = ended_with_quote_colon;
		*p++ = ':';
	    }
	    else if(ended_with_colon)
	      *p++ = ended_with_colon;
	    else if(ended_with_quote)
	      *p++ = ended_with_quote;

	    *p = '\0';
	}

	if(filtered && *filtered)
	  gf_puts(filtered, pc);
    }

    /* and end with two newlines unless no leadin at all */
    if(!contains_newline_token){
	if(!strcmp(buf, DEFAULT_REPLY_INTRO) || (filtered && *filtered)){
	    gf_puts(NEWLINE, pc);
	    gf_puts(NEWLINE, pc);
	}
    }

    if(filtered)
      fs_give((void **)&filtered);
}


void
free_redraft_pos(REDRAFT_POS_S **redraft_pos)
{
    if(redraft_pos && *redraft_pos){
	if((*redraft_pos)->hdrname)
	  fs_give((void **)&(*redraft_pos)->hdrname);
	
	fs_give((void **)redraft_pos);
    }
}


/*----------------------------------------------------------------------
  Build the body for the message number/part being forwarded as ATTACHMENT

    Args: 

  Result: PARTS suitably attached to body

  ----------------------------------------------------------------------*/
int
forward_mime_msg(MAILSTREAM *stream, long int msgno, char *section, ENVELOPE *env, struct mail_body_part **partp, void *msgtext)
{
    char	   *tmp_text;
    unsigned long   len;
    BODY	   *b;

    *partp		= mail_newbody_part();
    b			= &(*partp)->body;
    b->type		= TYPEMESSAGE;
    b->id		= generate_message_id();
    b->description	= cpystr("Forwarded Message");
    b->nested.msg	= mail_newmsg();
    b->disposition.type = cpystr("inline");

    /*---- Package each message in a storage object ----*/
    if((b->contents.text.data = (void *) so_get(PART_SO_TYPE,NULL,EDIT_ACCESS))
       && (tmp_text = mail_fetch_header(stream,msgno,section,NIL,NIL,FT_PEEK))
       && *tmp_text){
	so_puts((STORE_S *) b->contents.text.data, tmp_text);

	b->size.bytes = strlen(tmp_text);
	so_puts((STORE_S *) b->contents.text.data, "\015\012");
	if((tmp_text = pine_mail_fetch_text (stream,msgno,section,&len,NIL)) != NULL){
	    so_nputs((STORE_S *)b->contents.text.data,tmp_text,(long) len);
	    b->size.bytes += len;
	    return(1);
	}
    }

    return(0);
}


/*
 * forward_delimiter - return delimiter for forwarded text
 */
void
forward_delimiter(gf_io_t pc)
{
    gf_puts(NEWLINE, pc);
    /* TRANSLATORS: When a message is forwarded by the user this is the
       text that shows where the forwarded part of the message begins. */
    gf_puts(_("---------- Forwarded message ----------"), pc);
    gf_puts(NEWLINE, pc);
}


/*----------------------------------------------------------------------
    Wrapper for header formatting tool

    Args: stream --
	  msgno --
	  env --
	  pc --
	  prefix --

  Result: header suitable for reply/forward text written using "pc"

  ----------------------------------------------------------------------*/
void
reply_forward_header(MAILSTREAM *stream, long int msgno, char *part, ENVELOPE *env,
		     gf_io_t pc, char *prefix)
{
    int      rv;
    HEADER_S h;
    char   **list, **new_list = NULL;

    list = ps_global->VAR_VIEW_HEADERS;

    /*
     * If VIEW_HEADERS is set, we should remove BCC from the list so that
     * the user doesn't inadvertently forward the BCC header.
     */
    if(list && list[0]){
	int    i, cnt = 0;
	char **p;

	while(list[cnt++])
	  ;

	p = new_list = (char **) fs_get((cnt+1) * sizeof(char *));

	for(i=0; list[i]; i++)
	  if(strucmp(list[i], "bcc"))
	    *p++ = cpystr(list[i]);
	
	*p = NULL;

	if(new_list && new_list[0])
	  list = new_list;

    }

    HD_INIT(&h, list, ps_global->view_all_except, FE_DEFAULT & ~FE_BCC);
    if((rv = format_header(stream, msgno, part, env, &h,
			   prefix, NULL, FM_NOINDENT, NULL, pc)) != 0){
	if(rv == 1)
	  gf_puts("  [Error fetching message header data]", pc);
    }
    else
      gf_puts(NEWLINE, pc);		/* write header delimiter */
    
    if(new_list)
      free_list_array(&new_list);
}


/*----------------------------------------------------------------------
  Build the subject for the message number being forwarded

    Args: pine_state -- The usual pine structure
          msgno      -- The message number to build subject for

  Result: malloc'd string containing new subject or NULL on error

  ----------------------------------------------------------------------*/
char *
forward_subject(ENVELOPE *env, int flags)
{
    size_t l;
    char  *p, buftmp[MAILTMPLEN];
    
    if(!env)
      return(NULL);

    dprint((9, "checking subject: \"%s\"\n",
	       env->subject ? env->subject : "NULL"));

    if(env->subject && env->subject[0]){		/* add (fwd)? */
	snprintf(buftmp, sizeof(buftmp), "%s", env->subject);
	buftmp[sizeof(buftmp)-1] = '\0';
	/* decode any 8bit (copy to the temp buffer if decoding doesn't) */
	if(rfc1522_decode_to_utf8((unsigned char *) tmp_20k_buf,
				  SIZEOF_20KBUF, buftmp) == (unsigned char *) buftmp)
	  strncpy(tmp_20k_buf, buftmp, SIZEOF_20KBUF);

	tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';

	removing_trailing_white_space(tmp_20k_buf);
	if((l = strlen(tmp_20k_buf)) < 1000 &&
	   (l < 5 || strcmp(tmp_20k_buf+l-5,"(fwd)"))){
	    snprintf(tmp_20k_buf+2000, SIZEOF_20KBUF-2000, "%s (fwd)", tmp_20k_buf);
	    tmp_20k_buf[SIZEOF_20KBUF-2000-1] = '\0';
	    strncpy(tmp_20k_buf, tmp_20k_buf+2000, SIZEOF_20KBUF);
	    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	}

	/*
	 * HACK:  composer can't handle embedded double quotes in attachment
	 * comments so we substitute two single quotes.
	 */
	if(flags & FS_CONVERT_QUOTES)
	  while((p = strchr(tmp_20k_buf, QUOTE)) != NULL)
	    (void)rplstr(p, SIZEOF_20KBUF-(p-tmp_20k_buf), 1, "''");

	return(cpystr(tmp_20k_buf));

    }

    return(cpystr("Forwarded mail...."));
}


/*----------------------------------------------------------------------
  Build the body for the message number/part being forwarded

    Args: 

  Result: BODY structure suitable for sending

  ----------------------------------------------------------------------*/
BODY *
forward_body(MAILSTREAM *stream, ENVELOPE *env, struct mail_bodystruct *orig_body,
	     long int msgno, char *sect_prefix, void *msgtext, int flags)
{
    BODY    *body = NULL, *text_body, *tmp_body;
    PART    *part;
    gf_io_t  pc;
    char    *tmp_text, *section, sect_buf[256];
    int      forward_raw_body = 0;

    /*
     * Check to see if messages got expunged out from underneath us. This
     * could have happened during the prompt to the user asking whether to
     * include the message as an attachment. Either the message is gone or
     * it might be at a different sequence number. We'd better bail.
     */
    if(ps_global->full_header == 2
       && F_ON(F_ENABLE_FULL_HDR_AND_TEXT, ps_global))
      forward_raw_body = 1;
    if(sp_expunge_count(stream))
      return(NULL);

    if(sect_prefix && forward_raw_body == 0)
      snprintf(section = sect_buf, sizeof(sect_buf), "%s.1", sect_prefix);
    else if(sect_prefix && forward_raw_body)
      section = sect_prefix;
    else if(!sect_prefix && forward_raw_body)
      section = NULL;
    else
      section = "1";

    sect_buf[sizeof(sect_buf)-1] = '\0';

    gf_set_so_writec(&pc, (STORE_S *) msgtext);
    if(!orig_body || orig_body->type == TYPETEXT || forward_raw_body) {
	char *charset = NULL;

	/*---- Message has a single text part -----*/
	body			 = mail_newbody();
	body->type		 = TYPETEXT;
	body->contents.text.data = msgtext;
	if(orig_body
	   && (charset = parameter_val(orig_body->parameter, "charset")))
	  set_parameter(&body->parameter, "charset", charset);

	if(charset)
	  fs_give((void **) &charset);

	if(!(flags & FWD_ANON)){
	    forward_delimiter(pc);
	    reply_forward_header(stream, msgno, sect_prefix, env, pc, "");
	}

	if(!get_body_part_text(stream, forward_raw_body ? NULL : orig_body,
			       msgno, section, 0L, pc, NULL, NULL, GBPT_NONE)){
	    mail_free_body(&body);
	    return(NULL);
	}
    }
    else if(orig_body->type == TYPEMULTIPART) {
	if(orig_body->subtype && !strucmp(orig_body->subtype, "signed")
	   && orig_body->nested.part){
	    /* only operate on the signed data (not the signature) */
	    body = forward_body(stream, env, &orig_body->nested.part->body,
				msgno, sect_prefix, msgtext, flags);
	}
	/*---- Message is multipart ----*/
	else if(!(orig_body->subtype && !strucmp(orig_body->subtype,
						 "alternative")
		  && (body = forward_multi_alt(stream, env, orig_body, msgno,
					       sect_prefix, msgtext,
					       pc, flags)))){
	    /*--- Copy the body and entire structure  ---*/
	    body = copy_body(NULL, orig_body);

	    /*
	     * whatever subtype it is, demote it
	     * to plain old MIXED.
	     */
	    if(body->subtype)
	      fs_give((void **) &body->subtype);

	    body->subtype = cpystr("Mixed");

	    /*--- The text part of the message ---*/
	    if(!body->nested.part){
		q_status_message(SM_ORDER | SM_DING, 3, 6,
				 "Error referencing body part 1");
		mail_free_body(&body);
	    }
	    else if(body->nested.part->body.type == TYPETEXT) {
		char *new_charset = NULL;

		/*--- The first part is text ----*/
		text_body		      = &body->nested.part->body;
		text_body->contents.text.data = msgtext;
		if(text_body->subtype && strucmp(text_body->subtype, "Plain")){
		    /* this text is going to the composer, it should be Plain */
		    fs_give((void **)&text_body->subtype);
		    text_body->subtype = cpystr("PLAIN");
		}
		if(!(flags & FWD_ANON)){
		    forward_delimiter(pc);
		    reply_forward_header(stream, msgno,
					 sect_prefix, env, pc, "");
		}

		if(!(get_body_part_text(stream, &orig_body->nested.part->body,
					msgno, section, 0L, pc,
					NULL, &new_charset, GBPT_NONE)
		     && fetch_contents(stream, msgno, sect_prefix, body)))
		  mail_free_body(&body);
		else if(new_charset)
		  set_parameter(&text_body->parameter, "charset", new_charset);

/* BUG: ? matter that we're not setting body.size.bytes */
	    }
	    else if(body->nested.part->body.type == TYPEMULTIPART
		    && body->nested.part->body.subtype
		    && !strucmp(body->nested.part->body.subtype, "alternative")
		    && (tmp_body = forward_multi_alt(stream, env,
						     &body->nested.part->body,
						     msgno, sect_prefix,
						     msgtext, pc,
						     flags | FWD_NESTED))){
	      /* for the forward_multi_alt call above, we want to pass
	       * sect_prefix instead of section so we can obtain the header.
	       * Set the FWD_NESTED flag so we fetch the right body_part.
	       */ 
		int partnum;

		part = body->nested.part->next;
		body->nested.part->next = NULL;
		mail_free_body_part(&body->nested.part);
		body->nested.part = mail_newbody_part();
		body->nested.part->body = *tmp_body;
		body->nested.part->next = part;

		for(partnum = 2; part != NULL; part = part->next){
		    snprintf(sect_buf, sizeof(sect_buf), "%s%s%d",
			    sect_prefix ? sect_prefix : "",
			    sect_prefix ? "." : "", partnum++);
		    sect_buf[sizeof(sect_buf)-1] = '\0';

		    if(!fetch_contents(stream, msgno, sect_buf, &part->body)){
			mail_free_body(&body);
			break;
		    }
		}
	    }
	    else {
		if(fetch_contents(stream, msgno, sect_prefix, body)){
		    /*--- Create a new blank text part ---*/
		    part			  = mail_newbody_part();
		    part->next			  = body->nested.part;
		    body->nested.part		  = part;
		    part->body.contents.text.data = msgtext;
		}
		else
		  mail_free_body(&body);
	    }
	}
    }
    else {
	/*---- A single part message, not of type text ----*/
	body                     = mail_newbody();
	body->type               = TYPEMULTIPART;
	part                     = mail_newbody_part();
	body->nested.part      = part;

	/*--- The first part, a blank text part to be edited ---*/
	part->body.type            = TYPETEXT;
	part->body.contents.text.data = msgtext;

	/*--- The second part, what ever it is ---*/
	part->next               = mail_newbody_part();
	part                     = part->next;
	part->body.id            = generate_message_id();
	copy_body(&(part->body), orig_body);

	/*
	 * the idea here is to fetch part into storage object
	 */
	if((part->body.contents.text.data = (void *) so_get(PART_SO_TYPE, NULL,
							   EDIT_ACCESS)) != NULL){
	    if((tmp_text = pine_mail_fetch_body(stream, msgno, section,
					 &part->body.size.bytes, NIL)) != NULL)
	      so_nputs((STORE_S *)part->body.contents.text.data, tmp_text,
		       part->body.size.bytes);
	    else
	      mail_free_body(&body);
	}
	else
	  mail_free_body(&body);
    }

    gf_clear_so_writec((STORE_S *) msgtext);

    return(body);
}



/*
 * bounce_msg_body - build body from specified message suitable
 *		     for sending as bounced message
 */
char *
bounce_msg_body(MAILSTREAM *stream,
		long int rawno,
		char *part,
		char **to,
		char *subject,
		ENVELOPE **outgoingp,
		BODY **bodyp,
		int *seenp)
{
    char     *h, *p, *errstr = NULL;
    int	      i;
    STORE_S  *msgtext;
    gf_io_t   pc;

    *outgoingp		 = mail_newenvelope();
    (*outgoingp)->message_id = generate_message_id();
    (*outgoingp)->subject    = cpystr(subject ? subject : "Resent mail....");

    /*
     * Fill in destination if we were given one.  If so, note that we
     * call p_s_s() below such that it won't prompt...
     */
    if(to && *to){
	static char *fakedomain = "@";
	char	    *tmp_a_string;

	/* rfc822_parse_adrlist feels free to destroy input so copy */
	tmp_a_string = cpystr(*to);
	rfc822_parse_adrlist(&(*outgoingp)->to, tmp_a_string,
			     (F_ON(F_COMPOSE_REJECTS_UNQUAL, ps_global))
			     ? fakedomain : ps_global->maildomain);
	fs_give((void **) &tmp_a_string);
    }

    /* build remail'd header */
    if((h = mail_fetch_header(stream, rawno, part, NULL, 0, FT_PEEK)) != NULL){
	for(p = h, i = 0; (p = strchr(p, ':')) != NULL; p++)
	  i++;

	/* allocate it */
	(*outgoingp)->remail = (char *) fs_get(strlen(h) + (2 * i) + 1);

	/*
	 * copy it, "X-"ing out transport headers bothersome to
	 * software but potentially useful to the human recipient...
	 */
	p = (*outgoingp)->remail;
	bounce_mask_header(&p, h);
	do
	  if(*h == '\015' && *(h+1) == '\012'){
	      *p++ = *h++;		/* copy CR LF */
	      *p++ = *h++;
	      bounce_mask_header(&p, h);
	  }
	while((*p++ = *h++) != '\0');
    }
    /* BUG: else complain? */

    /* NOT bound for the composer, so no need for PicoText */
    if(!(msgtext = so_get(CharStar, NULL, EDIT_ACCESS))){
	mail_free_envelope(outgoingp);
	return(_("Error allocating message text"));
    }

    /* mark object for special handling */
    so_attr(msgtext, "rawbody", "1");

    /*
     * Build a fake body description.  It's ignored by pine_rfc822_header,
     * but we need to set it to something that makes set_mime_types
     * not sniff it and pine_rfc822_output_body not re-encode it.
     * Setting the encoding to (ENCMAX + 1) will work and shouldn't cause
     * problems unless something tries to access body_encodings[] using
     * it without proper precautions.  We don't want to use ENCOTHER
     * cause that tells set_mime_types to sniff it, and we don't want to
     * use ENC8BIT since that tells pine_rfc822_output_body to qp-encode
     * it.  When there's time, it'd be nice to clean this interaction
     * up...
     */
    *bodyp			 = mail_newbody();
    (*bodyp)->type		 = TYPETEXT;
    (*bodyp)->encoding		 = ENCMAX + 1;
    (*bodyp)->subtype		 = cpystr("Plain");
    (*bodyp)->contents.text.data = (void *) msgtext;
    gf_set_so_writec(&pc, msgtext);

    if(seenp && rawno > 0L && stream && rawno <= stream->nmsgs){
	MESSAGECACHE *mc;

	if((mc = mail_elt(stream,  rawno)) != NULL)
	  *seenp = mc->seen;
    }

    /* pass NULL body to force mail_fetchtext */
    if(!get_body_part_text(stream, NULL, rawno, part, 0L, pc, NULL, NULL, GBPT_NONE))
      errstr = _("Error fetching message contents. Can't Bounce message");

    gf_clear_so_writec(msgtext);

    return(errstr);
}



/*----------------------------------------------------------------------
    Mask off any header entries we don't want xport software to see

Args:  d -- destination string pointer pointer
       s -- source string pointer pointer

       Postfix uses Delivered-To to detect loops.
       Received line counting is also used to detect loops in places.

  ----*/
void
bounce_mask_header(char **d, char *s)
{
    if(((*s == 'R' || *s == 'r')
        && (!struncmp(s+1, "esent-", 6) || !struncmp(s+1, "eceived:", 8)
	    || !struncmp(s+1, "eturn-Path", 10)))
       || !struncmp(s, "Delivered-To:", 13)){
	*(*d)++ = 'X';				/* write mask */
	*(*d)++ = '-';
    }
}

        
/*----------------------------------------------------------------------
    Fetch and format text for forwarding

Args:  stream  -- Mail stream to fetch text from
       body    -- Body structure of message being forwarded
       msg_no  -- Message number of text for forward
       part_no -- Part number of text to forward
       partial -- If this is > 0 a partial fetch will be done and it will
                    be done using FT_PEEK so the message will remain unseen.
       pc      -- Function to write to
       prefix  -- Prefix for each line
       ret_charset -- If we translate to another charset return that
                      new charset here

Returns:  true if OK, false if problem occured while filtering

If the text is richtext, it will be converted to plain text, since there's
no rich text editing capabilities in Pine (yet).

It's up to calling routines to plug in signature appropriately

As with all internal text, NVT end-of-line conventions are observed.
DOESN'T sanity check the prefix given!!!
  ----*/
int
get_body_part_text(MAILSTREAM *stream, struct mail_bodystruct *body,
		   long int msg_no, char *part_no, long partial, gf_io_t pc,
		   char *prefix, char **ret_charset, unsigned flags)
{
    int		we_cancel = 0, dashdata, wrapflags = GFW_FORCOMPOSE, flow_res = 0;
    FILTLIST_S  filters[12];
    long	len;
    char       *err, *charset, *prefix_p = NULL;
    int		filtcnt = 0;
    char       *free_this = NULL;
    DELQ_S      dq;

    memset(filters, 0, sizeof(filters));
    if(ret_charset)
      *ret_charset = NULL;

    if(!pc_is_picotext(pc))
      we_cancel = busy_cue(NULL, NULL, 1);

    /* if null body, we must be talking to a non-IMAP2bis server.
     * No MIME parsing provided, so we just grab the message text...
     */
    if(body == NULL){
	char         *text, *decode_error;
	gf_io_t       gc;
	SourceType    src = CharStar;
	int           rv = 0;

	(void) pine_mail_fetchstructure(stream, msg_no, NULL);

	if((text = pine_mail_fetch_text(stream, msg_no, part_no, NULL, 0)) != NULL){
	    gf_set_readc(&gc, text, (unsigned long)strlen(text), src, 0);

	    gf_filter_init();		/* no filters needed */
	    if(prefix)
	      gf_link_filter(gf_prefix, gf_prefix_opt(prefix));
	    if((decode_error = gf_pipe(gc, pc)) != NULL){
		snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%s%s    [Formatting error: %s]%s",
			NEWLINE, NEWLINE,
			decode_error, NEWLINE);
		tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		gf_puts(tmp_20k_buf, pc);
		rv++;
	    }
	}
	else{
	    gf_puts(NEWLINE, pc);
	    gf_puts(_("    [ERROR fetching text of message]"), pc);
	    gf_puts(NEWLINE, pc);
	    gf_puts(NEWLINE, pc);
	    rv++;
	}

	if(we_cancel)
	  cancel_busy_cue(-1);

	return(rv == 0);
    }

    charset = parameter_val(body->parameter, "charset");

    if(charset && strucmp(charset, "utf-8") && strucmp(charset, "us-ascii")){
	if(ret_charset)
	  *ret_charset = "UTF-8";
    }

    /*
     * just use detach, but add an auxiliary filter to insert prefix,
     * and, perhaps, digest richtext
     */
    if(ps_global->full_header != 2
       && !ps_global->postpone_no_flow
       && (!body->subtype || !strucmp(body->subtype, "plain"))){
	char *parmval;

	flow_res = (F_OFF(F_QUELL_FLOWED_TEXT, ps_global)
		     && F_OFF(F_STRIP_WS_BEFORE_SEND, ps_global)
		     && (!prefix || (strucmp(prefix,"> ") == 0)
			 || strucmp(prefix, ">") == 0));
	if((parmval = parameter_val(body->parameter,
				       "format")) != NULL){
	    if(!strucmp(parmval, "flowed")){
		wrapflags |= GFW_FLOWED;

		fs_give((void **) &parmval);
		if((parmval = parameter_val(body->parameter, "delsp")) != NULL){
		    if(!strucmp(parmval, "yes")){
			filters[filtcnt++].filter = gf_preflow;
			wrapflags |= GFW_DELSP;
		    }

		    fs_give((void **) &parmval);
		}

 		/*
 		 * if there's no prefix we're forwarding text
 		 * otherwise it's reply text.  unless the user's
 		 * tied our hands, alter the prefix to continue flowed
 		 * formatting...
 		 */
 		if(flow_res)
		  wrapflags |= GFW_FLOW_RESULT;

		filters[filtcnt].filter = gf_wrap;
		/* 
		 * The 80 will cause longer lines than what is likely
		 * set by composer_fillcol, so we'll have longer
		 * quoted and forwarded lines than the lines we type.
		 * We're fine with that since the alternative is the
		 * possible wrapping of lines we shouldn't have, which
		 * is mitigated by this higher 80 limit.
		 *
		 * If we were to go back to using composer_fillcol,
		 * the correct value is composer_fillcol + 1, pine
		 * is off-by-one from pico.
		 */
		filters[filtcnt++].data = gf_wrap_filter_opt(
		    MAX(MIN(ps_global->ttyo->screen_cols
			    - (prefix ? strlen(prefix) : 0),
			    80 - (prefix ? strlen(prefix) : 0)),
			30), /* doesn't have to be 30 */
		    990, /* 998 is the SMTP limit */
		    NULL, 0, wrapflags);
	    }
	}

 	/*
 	 * if not flowed, remove trailing whitespace to reduce
 	 * confusion, since we're sending out as flowed if we
	 * can.  At some future point, we might try 
 	 * plugging in a user-option-controlled heuristic
 	 * flowing filter 
	 *
	 * We also want to fold "> " quotes so we get the
	 * attributions correct.
 	 */
	if(flow_res && prefix && !strucmp(prefix, "> "))
	  *(prefix_p = prefix + 1) = '\0';

	if(!(wrapflags & GFW_FLOWED)
	   && flow_res){
	    filters[filtcnt].filter = gf_line_test;
	    filters[filtcnt++].data = gf_line_test_opt(twsp_strip, NULL);

	    filters[filtcnt].filter = gf_line_test;
	    filters[filtcnt++].data = gf_line_test_opt(quote_fold, NULL);
	}
    }
    else if(body->subtype){
	int plain_opt = 1;

	if(strucmp(body->subtype,"richtext") == 0){
	    filters[filtcnt].filter = gf_rich2plain;
	    filters[filtcnt++].data = gf_rich2plain_opt(&plain_opt);
	}
	else if(strucmp(body->subtype,"enriched") == 0){
	    filters[filtcnt].filter = gf_enriched2plain;
	    filters[filtcnt++].data = gf_enriched2plain_opt(&plain_opt);
	}
	else if(strucmp(body->subtype,"html") == 0){
	    if((flags & GBPT_HTML_OK) != GBPT_HTML_OK){
		filters[filtcnt].filter = gf_html2plain;
		filters[filtcnt++].data = gf_html2plain_opt(NULL,
							    ps_global->ttyo->screen_cols,
							    non_messageview_margin(),
							    NULL, NULL, GFHP_STRIPPED);
	    }
	}
    }

    if(prefix){
	if(ps_global->full_header != 2
	   && (F_ON(F_ENABLE_SIGDASHES, ps_global)
	       || F_ON(F_ENABLE_STRIP_SIGDASHES, ps_global))){
	    dashdata = 0;
	    filters[filtcnt].filter = gf_line_test;
	    filters[filtcnt++].data = gf_line_test_opt(sigdash_strip, &dashdata);
	}

	filters[filtcnt].filter = gf_prefix;
	filters[filtcnt++].data = gf_prefix_opt(prefix);

	if(wrapflags & GFW_FLOWED || flow_res){
	    filters[filtcnt].filter = gf_line_test;
	    filters[filtcnt++].data = gf_line_test_opt(post_quote_space, NULL);
	}
    }

    if(flags & GBPT_DELQUOTES){
	memset(&dq, 0, sizeof(dq));
	dq.lines = Q_DEL_ALL;
	dq.is_flowed = 0;
	dq.indent_length = 0;
	dq.saved_line = &free_this;
	dq.handlesp   = NULL;
	dq.do_color   = 0;
	dq.delete_all = 1;

	filters[filtcnt].filter = gf_line_test;
	filters[filtcnt++].data = gf_line_test_opt(delete_quotes, &dq);
    }

    err = detach(stream, msg_no, part_no, partial, &len, pc,
		 filters[0].filter ? filters : NULL,
		 ((flags & GBPT_PEEK) ? FT_PEEK : 0)
		 | ((flags & GBPT_NOINTR) ? DT_NOINTR : 0));

    if(free_this)
      fs_give((void **) &free_this);

    if(prefix_p)
      *prefix_p = ' ';

    if (err != (char *) NULL)
       /* TRANSLATORS: The first arg is error text, the %ld is the message number */
       q_status_message2(SM_ORDER, 3, 4, "%s: message number %ld",
			 err, (void *) msg_no);

    if(we_cancel)
      cancel_busy_cue(-1);

    return((int) len);
}


int
quote_fold(long int linenum, char *line, LT_INS_S **ins, void *local)
{
    char *p;

    if(*line == '>'){
	for(p = line; *p; p++){
	    if(isspace((unsigned char) *p)){
		if(*(p+1) == '>')
		  ins = gf_line_test_new_ins(ins, p, "", -1);
	    }
	    else if(*p != '>')
	      break;
	}
    }

    return(0);
}


int
twsp_strip(long int linenum, char *line, LT_INS_S **ins, void *local)
{
    char *p, *ws = NULL;

    for(p = line; *p; p++){
      /* don't strip trailing space on signature line */
      if(*line == '-' && *(line+1) == '-' && *(line+2) == ' ' && !*(line+3))
        break;

      if(isspace((unsigned char) *p)){
	  if(!ws)
	    ws = p;
      }
      else
	ws = NULL;
    }

    if(ws)
      ins = gf_line_test_new_ins(ins, ws, "", -(p - ws));

    return(0);
}

int
post_quote_space(long int linenum, char *line, LT_INS_S **ins, void *local)
{
    char *p;

    for(p = line; *p; p++)
      if(*p != '>'){
	  if(p != line && *p != ' ')
	    ins = gf_line_test_new_ins(ins, p, " ", 1);

	  break;
      }

    return(0);
}


int
sigdash_strip(long int linenum, char *line, LT_INS_S **ins, void *local)
{
    if(*((int *)local)
       || (*line == '-' && *(line+1) == '-'
	   && *(line+2) == ' ' && !*(line+3))){
	*((int *) local) = 1;
	return(2);		/* skip this line! */
    }

    return(0);
}


/*----------------------------------------------------------------------
  return the c-client reference name for the given end_body part
  ----*/
char *
body_partno(MAILSTREAM *stream, long int msgno, struct mail_bodystruct *end_body)
{
    BODY *body;

    (void) pine_mail_fetchstructure(stream, msgno, &body);
    return(partno(body, end_body));
}


/*----------------------------------------------------------------------
  return the c-client reference name for the given end_body part
  ----*/
char *
partno(struct mail_bodystruct *body, struct mail_bodystruct *end_body)
{
    PART *part;
    int   num = 0;
    char  tmp[64], *p = NULL;

    if(body && body->type == TYPEMULTIPART) {
	part = body->nested.part;	/* first body part */

	do {				/* for each part */
	    num++;
	    if(&part->body == end_body || (p = partno(&part->body, end_body))){
		snprintf(tmp, sizeof(tmp), "%d%s%.*s", num, (p) ? "." : "",
			sizeof(tmp)-10, (p) ? p : "");
		tmp[sizeof(tmp)-1] = '\0';
		if(p)
		  fs_give((void **)&p);

		return(cpystr(tmp));
	    }
	} while ((part = part->next) != NULL);	/* until done */

	return(NULL);
    }
    else if(body && body->type == TYPEMESSAGE && body->subtype 
	    && !strucmp(body->subtype, "rfc822")){
	return(partno(body->nested.msg->body, end_body));
    }

    return((body == end_body) ? cpystr("1") : NULL);
}


/*----------------------------------------------------------------------
   Fill in the contents of each body part

Args: stream      -- Stream the message is on
      msgno       -- Message number the body structure is for
      section	  -- body section associated with body pointer
      body        -- Body pointer to fill in

Result: 1 if all went OK, 0 if there was a problem

This function copies the contents from an original message/body to
a new message/body.  It recurses down all multipart levels.

If one or more part (but not all) can't be fetched, a status message
will be queued.
 ----*/
int
fetch_contents(MAILSTREAM *stream, long int msgno, char *section, struct mail_bodystruct *body)
{
    char *tp;
    int   got_one = 0;

    if(!body->id)
      body->id = generate_message_id();
          
    if(body->type == TYPEMULTIPART){
	char  subsection[256], *subp;
	int   n, last_one = 10;		/* remember worst case */
	PART *part     = body->nested.part;

	if(!(part = body->nested.part))
	  return(0);

	subp = subsection;
	if(section && *section){
	    for(n = 0;
		n < sizeof(subsection)-20 && (*subp = section[n]); n++, subp++)
	      ;

	    *subp++ = '.';
	}

	n = 1;
	do {
	    snprintf(subp, sizeof(subsection)-(subp-subsection), "%d", n++);
	    subsection[sizeof(subsection)-1] = '\0';
	    got_one  = fetch_contents(stream, msgno, subsection, &part->body);
	    last_one = MIN(last_one, got_one);
	}
	while((part = part->next) != NULL);

	return(last_one);
    }

    if(body->contents.text.data)
      return(1);			/* already taken care of... */

    if(body->type == TYPEMESSAGE){
	if(body->subtype && strucmp(body->subtype,"external-body")){
	    /*
	     * the idea here is to fetch everything into storage objects
	     */
	    body->contents.text.data = (void *) so_get(PART_SO_TYPE, NULL,
						    EDIT_ACCESS);
	    if(body->contents.text.data
	       && (tp = pine_mail_fetch_body(stream, msgno, section,
				       &body->size.bytes, NIL))){
	        so_truncate((STORE_S *)body->contents.text.data, 
			    body->size.bytes + 2048);
		so_nputs((STORE_S *)body->contents.text.data, tp,
			 body->size.bytes);
		got_one = 1;
	    }
	    else
	      q_status_message1(SM_ORDER | SM_DING, 3, 3,
				_("Error fetching part %s"), section);
	} else {
	    got_one = 1;
	}
    } else {
	/*
	 * the idea here is to fetch everything into storage objects
	 * so, grab one, then fetch the body part
	 */
	body->contents.text.data = (void *)so_get(PART_SO_TYPE,NULL,EDIT_ACCESS);
	if(body->contents.text.data
	   && (tp=pine_mail_fetch_body(stream, msgno, section,
				       &body->size.bytes, NIL))){
	    so_truncate((STORE_S *)body->contents.text.data, 
			    body->size.bytes + 2048);
	    so_nputs((STORE_S *)body->contents.text.data, tp,
		     body->size.bytes);
	    got_one = 1;
	}
	else
	  q_status_message1(SM_ORDER | SM_DING, 3, 3,
			    _("Error fetching part %s"), section);
    }

    return(got_one);
}


/*----------------------------------------------------------------------
    Copy the body structure

Args: new_body -- Pointer to already allocated body, or NULL, if none
      old_body -- The Body to copy


 This is traverses the body structure recursively copying all elements.
The new_body parameter can be NULL in which case a new body is
allocated. Alternatively it can point to an already allocated body
structure. This is used when copying body parts since a PART includes a 
BODY. The contents fields are *not* filled in.
  ----*/

BODY *
copy_body(struct mail_bodystruct *new_body, struct mail_bodystruct *old_body)
{
    if(old_body == NULL)
      return(NULL);

    if(new_body == NULL)
      new_body = mail_newbody();

    new_body->type = old_body->type;
    new_body->encoding = old_body->encoding;

    if(old_body->subtype)
      new_body->subtype = cpystr(old_body->subtype);

    new_body->parameter = copy_parameters(old_body->parameter);

    if(old_body->id)
      new_body->id = cpystr(old_body->id);

    if(old_body->description)
      new_body->description = cpystr(old_body->description);

    if(old_body->disposition.type)
      new_body->disposition.type = cpystr(old_body->disposition.type);

    new_body->disposition.parameter
			    = copy_parameters(old_body->disposition.parameter);
    
    new_body->size = old_body->size;

    if(new_body->type == TYPEMESSAGE
       && new_body->subtype && !strucmp(new_body->subtype, "rfc822")){
	new_body->nested.msg = mail_newmsg();
	new_body->nested.msg->body
				 = copy_body(NULL, old_body->nested.msg->body);
    }
    else if(new_body->type == TYPEMULTIPART) {
	PART **new_partp, *old_part;

	new_partp = &new_body->nested.part;
	for(old_part = old_body->nested.part;
	    old_part != NULL;
	    old_part = old_part->next){
	    *new_partp = mail_newbody_part();
            copy_body(&(*new_partp)->body, &old_part->body);
	    new_partp = &(*new_partp)->next;
        }
    }

    return(new_body);
}


/*----------------------------------------------------------------------
    Copy the MIME parameter list
 
 Allocates storage for new part, and returns pointer to new paramter
list. If old_p is NULL, NULL is returned.
 ----*/
PARAMETER *
copy_parameters(PARAMETER *old_p)
{
    PARAMETER *new_p, *p1, *p2;

    if(old_p == NULL)
      return((PARAMETER *)NULL);

    new_p = p2 = NULL;
    for(p1 = old_p; p1 != NULL; p1 = p1->next){
	set_parameter(&p2, p1->attribute, p1->value);
	if(new_p == NULL)
	  new_p = p2;
    }

    return(new_p);
}
    

/*----------------------------------------------------------------------
    Make a complete copy of an envelope and all it's fields

Args:    e -- the envelope to copy

Result:  returns the new envelope, or NULL, if the given envelope was NULL

  ----*/

ENVELOPE *    
copy_envelope(register ENVELOPE *e)
{
    register ENVELOPE *e2;

    if(!e)
      return(NULL);

    e2		    = mail_newenvelope();
    e2->remail      = e->remail	     ? cpystr(e->remail)	      : NULL;
    e2->return_path = e->return_path ? rfc822_cpy_adr(e->return_path) : NULL;
    e2->date        = e->date	     ? (unsigned char *)cpystr((char *) e->date)
								      : NULL;
    e2->from        = e->from	     ? rfc822_cpy_adr(e->from)	      : NULL;
    e2->sender      = e->sender	     ? rfc822_cpy_adr(e->sender)      : NULL;
    e2->reply_to    = e->reply_to    ? rfc822_cpy_adr(e->reply_to)    : NULL;
    e2->subject     = e->subject     ? cpystr(e->subject)	      : NULL;
    e2->to          = e->to          ? rfc822_cpy_adr(e->to)	      : NULL;
    e2->cc          = e->cc          ? rfc822_cpy_adr(e->cc)	      : NULL;
    e2->bcc         = e->bcc         ? rfc822_cpy_adr(e->bcc)	      : NULL;
    e2->in_reply_to = e->in_reply_to ? cpystr(e->in_reply_to)	      : NULL;
    e2->newsgroups  = e->newsgroups  ? cpystr(e->newsgroups)	      : NULL;
    e2->message_id  = e->message_id  ? cpystr(e->message_id)	      : NULL;
    e2->references  = e->references  ? cpystr(e->references)          : NULL;
    e2->followup_to = e->followup_to ? cpystr(e->references)          : NULL;
    return(e2);
}


/*----------------------------------------------------------------------
     Generate the "In-reply-to" text from message header

  Args: message -- Envelope of original message

  Result: returns an alloc'd string or NULL if there is a problem
 ----*/
char *
reply_in_reply_to(ENVELOPE *env)
{
    return((env && env->message_id) ? cpystr(env->message_id) : NULL);
}


/*----------------------------------------------------------------------
        Generate a unique message id string.

   Args: ps -- The usual pine structure

  Result: Alloc'd unique string is returned

Uniqueness is gaurenteed by using the host name, process id, date to the
second and a single unique character
*----------------------------------------------------------------------*/
char *
generate_message_id(void)
{
    static short osec = 0, cnt = 0;
    char         idbuf[128];
    char        *id;
    time_t       now;
    struct tm   *now_x;
    char        *hostpart = NULL;

    now   = time((time_t *)0);
    now_x = localtime(&now);

    if(now_x->tm_sec == osec)
      cnt++;
    else{
	cnt = 0;
	osec = now_x->tm_sec;
    }

    hostpart = F_ON(F_ROT13_MESSAGE_ID, ps_global)
		 ? rot13(ps_global->hostname)
		 : cpystr(ps_global->hostname);
    
    if(!hostpart)
      hostpart = cpystr("huh");

    snprintf(idbuf, sizeof(idbuf), "<alpine.%.4s.%.20s.%02d%02d%02d%02d%02d%02d%X.%d@%.50s>",
	    SYSTYPE, ALPINE_VERSION, (now_x->tm_year) % 100, now_x->tm_mon + 1,
	    now_x->tm_mday, now_x->tm_hour, now_x->tm_min, now_x->tm_sec, 
	    cnt, getpid(), hostpart);
    idbuf[sizeof(idbuf)-1] = '\0';

    id = cpystr(idbuf);

    if(hostpart)
      fs_give((void **) &hostpart);

    return(id);
}


char *
generate_user_agent(void)
{
    char         buf[128];
    char         rev[128];

    if(F_ON(F_QUELL_USERAGENT, ps_global))
      return(NULL);

    snprintf(buf, sizeof(buf),
	     "%sAlpine %s (%s %s)",
	     (pith_opt_user_agent_prefix) ? (*pith_opt_user_agent_prefix)() : "",
	     ALPINE_VERSION, SYSTYPE,
	     get_alpine_revision_string(rev, sizeof(rev)));

    return(cpystr(buf));
}


char *
rot13(char *src)
{
    char byte, cap, *p, *ret = NULL;

    if(src && *src){
	ret = (char *) fs_get((strlen(src)+1) * sizeof(char));
	p = ret;
	while((byte = *src++) != '\0'){
	    cap   = byte & 32;
	    byte &= ~cap;
	    *p++ = ((byte >= 'A') && (byte <= 'Z')
		    ? ((byte - 'A' + 13) % 26 + 'A') : byte) | cap;
	}

	*p = '\0';
    }

    return(ret);
}


/*----------------------------------------------------------------------
  Return the first true address pointer (modulo group syntax allowance)

  Args: addr  -- Address list

 Result: First real address pointer, or NULL
  ----------------------------------------------------------------------*/
ADDRESS *
first_addr(struct mail_address *addr)
{
    while(addr && !addr->host)
      addr = addr->next;

    return(addr);
}


/*----------------------------------------------------------------------
           lit -- this is the source
   prenewlines -- prefix the file contents with this many newlines
  postnewlines -- postfix the file contents with this many newlines
        is_sig -- this is a signature (not a template)
  decode_constants -- change C-style constants into their values
  ----*/
char *
get_signature_lit(char *lit, int prenewlines, int postnewlines, int is_sig, int decode_constants)
{
    char *sig = NULL;

    /*
     * Should make this smart enough not to do the copying and double
     * allocation of space.
     */
    if(lit){
	char  *tmplit = NULL, *p, *q, *d, save;
	size_t len;

	if(decode_constants){
	    tmplit = (char *) fs_get((strlen(lit)+1) * sizeof(char));
	    tmplit[0] = '\0';
	    cstring_to_string(lit, tmplit);
	}
	else
	  tmplit = cpystr(lit);

	len = strlen(tmplit) + 5 + (prenewlines+postnewlines) * strlen(NEWLINE);
	sig = (char *) fs_get((len+1) * sizeof(char));
	memset(sig, 0, len+1);
	d = sig;
	while(prenewlines--)
	  sstrncpy(&d, NEWLINE, len-(d-sig));

	if(is_sig && F_ON(F_ENABLE_SIGDASHES, ps_global) &&
	   !sigdashes_are_present(tmplit)){
	    sstrncpy(&d, SIGDASHES, len-(d-sig));
	    sstrncpy(&d, NEWLINE, len-(d-sig));
	}

	sig[len] = '\0';

	p = tmplit;
	while(*p){
	    /* get a line */
	    q = strpbrk(p, "\n\r");
	    if(q){
		save = *q;
		*q = '\0';
	    }

	    /*
	     * Strip trailing space if we are doing a signature and
	     * this line is not sigdashes.
	     */
	    if(is_sig && strcmp(p, SIGDASHES))
	      removing_trailing_white_space(p);

	    while((d-sig) <= len && (*d = *p++) != '\0')
	      d++;

	    if(q){
		if((d-sig) <= len)
		  *d++ = save;
		  
		p = q+1;
	    }
	    else
	      break;
	}

	while(postnewlines--)
	  sstrncpy(&d, NEWLINE, len-(d-sig));

	sig[len] = '\0';

	if((d-sig) <= len)
	  *d = '\0';

	if(tmplit)
	  fs_give((void **) &tmplit);
    }

    return(sig);
}


int
sigdashes_are_present(char *sig)
{
    char *p;

    p = srchstr(sig, SIGDASHES);
    while(p && !((p == sig || (p[-1] == '\n' || p[-1] == '\r')) &&
	         (p[3] == '\0' || p[3] == '\n' || p[3] == '\r')))
      p = srchstr(p+1, SIGDASHES);
    
    return(p ? 1 : 0);
}


/*----------------------------------------------------------------------
  Acquire the pinerc defined signature file pathname

  ----*/
char *
signature_path(char *sname, char *sbuf, size_t len)
{
    *sbuf = '\0';
    if(sname && *sname){
	size_t spl = strlen(sname);
	if(IS_REMOTE(sname)){
	    if(spl < len - 1)
	      strncpy(sbuf, sname, len-1);
	}
	else if(is_absolute_path(sname)){
	    strncpy(sbuf, sname, len-1);
	    sbuf[len-1] = '\0';
	    fnexpand(sbuf, len);
	}
	else if(ps_global->VAR_OPER_DIR){
	    if(strlen(ps_global->VAR_OPER_DIR) + spl < len - 1)
	      build_path(sbuf, ps_global->VAR_OPER_DIR, sname, len);
	}
	else{
	    char *lc = last_cmpnt(ps_global->pinerc);

	    sbuf[0] = '\0';
	    if(lc != NULL){
		strncpy(sbuf,ps_global->pinerc,MIN(len-1,lc-ps_global->pinerc));
		sbuf[MIN(len-1,lc-ps_global->pinerc)] = '\0';
	    }

	    strncat(sbuf, sname, MAX(len-1-strlen(sbuf), 0));
	    sbuf[len-1] = '\0';
	}
    }

    return(*sbuf ? sbuf : NULL);
}


char *
simple_read_remote_file(char *name, char *subtype)
{
    int        try_cache;
    REMDATA_S *rd;
    char      *file = NULL;


    dprint((7, "simple_read_remote_file(%s, %s)\n", name ? name : "?", subtype ? subtype : "?"));

    /*
     * We could parse the name here to find what type it is. So far we
     * only have type RemImap.
     */
    rd = rd_create_remote(RemImap, name, subtype,
			  NULL, _("Error: "), _("Can't fetch remote configuration."));
    if(!rd)
      goto bail_out;
    
    try_cache = rd_read_metadata(rd);

    if(rd->access == MaybeRorW){
	if(rd->read_status == 'R')
	  rd->access = ReadOnly;
	else
	  rd->access = ReadWrite;
    }

    if(rd->access != NoExists){

	rd_check_remvalid(rd, 1L);

	/*
	 * If the cached info says it is readonly but
	 * it looks like it's been fixed now, change it to readwrite.
	 */
        if(rd->read_status == 'R'){
	    /*
	     * We go to this trouble since readonly sigfiles
	     * are likely a mistake. They are usually supposed to be
	     * readwrite so we open it and check if it's been fixed.
	     */
	    rd_check_readonly_access(rd);
	    if(rd->read_status == 'W'){
		rd->access = ReadWrite;
		rd->flags |= REM_OUTOFDATE;
	    }
	    else
	      rd->access = ReadOnly;
	}

	if(rd->flags & REM_OUTOFDATE){
	    if(rd_update_local(rd) != 0){

		dprint((1,
		       "simple_read_remote_file: rd_update_local failed\n"));
		/*
		 * Don't give up altogether. We still may be
		 * able to use a cached copy.
		 */
	    }
	    else{
		dprint((7,
		       "%s: copied remote to local (%ld)\n",
		       rd->rn ? rd->rn : "?", (long)rd->last_use));
	    }
	}

	if(rd->access == ReadWrite)
	  rd->flags |= DO_REMTRIM;
    }

    /* If we couldn't get to remote folder, try using the cached copy */
    if(rd->access == NoExists || rd->flags & REM_OUTOFDATE){
	if(try_cache){
	    rd->access = ReadOnly;
	    rd->flags |= USE_OLD_CACHE;
	    q_status_message(SM_ORDER, 3, 4,
	     "Can't contact remote server, using cached copy");
	    dprint((2,
    "Can't open remote file %s, using local cached copy %s readonly\n",
		   rd->rn ? rd->rn : "?",
		   rd->lf ? rd->lf : "?"));
	}
	else{
	    rd->flags &= ~DO_REMTRIM;
	    goto bail_out;
	}
    }

    file = read_file(rd->lf, READ_FROM_LOCALE);

bail_out:
    if(rd)
      rd_close_remdata(&rd);

    return(file);
}


/*----------------------------------------------------------------------
  Build the body for the multipart/alternative part

    Args: 

  Result: 

  ----------------------------------------------------------------------*/
BODY *
forward_multi_alt(MAILSTREAM *stream, ENVELOPE *env, struct mail_bodystruct *orig_body,
		  long int msgno, char *sect_prefix, void *msgtext, gf_io_t pc, int flags)
{
    BODY *body = NULL;
    PART *part = NULL, *bestpart = NULL;
    char  tmp_buf[256];
    char *new_charset = NULL;
    int   partnum, bestpartnum;

    if(ps_global->force_prefer_plain
       || (!ps_global->force_no_prefer_plain
	   && F_ON(F_PREFER_PLAIN_TEXT, ps_global))){
	for(part = orig_body->nested.part, partnum = 1;
	    part;
	    part = part->next, partnum++)
	  if((!part->body.type || part->body.type == TYPETEXT)
	     && (!part->body.subtype
		 || !strucmp(part->body.subtype, "plain")))
	      break;
    }

    /*
     * Else choose last alternative among plain or html parts.
     * Perhaps we should really be using mime_show() to make this
     * potentially more general than just plain or html.
     */
    if(!part){
	for(part = orig_body->nested.part, partnum = 1;
	    part;
	    part = part->next, partnum++){
	    if((!part->body.type || part->body.type == TYPETEXT)
		&& ((!part->body.subtype || !strucmp(part->body.subtype, "plain"))
		    ||
		    (part->body.subtype && !strucmp(part->body.subtype, "html")))){
		bestpart = part;
		bestpartnum = partnum;
	    }
	}

	part = bestpart;
	partnum = bestpartnum;
    }

    /*
     * IF something's interesting insert it
     * AND forget the rest of the multipart
     */
    if(part){
	body			 =  mail_newbody();
	body->type		 = TYPETEXT;
	body->contents.text.data = msgtext;

	/* record character set, flowing, etc */
	body->parameter = copy_parameters(part->body.parameter);
	body->size.bytes = part->body.size.bytes;

	if(!(flags & FWD_ANON)){
	    forward_delimiter(pc);
	    reply_forward_header(stream, msgno, sect_prefix, env, pc, "");
	}

	snprintf(tmp_buf, sizeof(tmp_buf), "%.*s%s%s%d",
		sizeof(tmp_buf)/2, sect_prefix ? sect_prefix : "",
		sect_prefix ? "." : "", flags & FWD_NESTED ? "1." : "",
		partnum);
	tmp_buf[sizeof(tmp_buf)-1] = '\0';
	get_body_part_text(stream, &part->body, msgno, tmp_buf, 0L, pc,
			   NULL, &new_charset, GBPT_NONE);

	/*
	 * get_body_part_text translated the data to a new charset.
	 * We need to record that fact in body.
	 */
	if(new_charset)
	  set_parameter(&body->parameter, "charset", new_charset);
    }
    else
      q_status_message(SM_ORDER | SM_DING, 3, 3,
		       "No suitable part found.  Forwarding as attachment");

    return(body);
}


void
reply_append_addr(struct mail_address **dest, struct mail_address *src)
{
    for( ; *dest; dest = &(*dest)->next)
      ;

    *dest = src;
}
