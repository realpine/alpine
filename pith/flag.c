#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: flag.c 1142 2008-08-13 17:22:21Z hubert@u.washington.edu $";
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
     flag.c
     Implements Pine message flag management routines
  ====*/


#include "../pith/headers.h"
#include "../pith/flag.h"
#include "../pith/pineelt.h"
#include "../pith/icache.h"
#include "../pith/mailindx.h"
#include "../pith/mailcmd.h"
#include "../pith/msgno.h"
#include "../pith/thread.h"
#include "../pith/sort.h"
#include "../pith/news.h"
#include "../pith/sequence.h"


/*
 * Internal prototypes
 */
void	flag_search(MAILSTREAM *, int, MsgNo, MSGNO_S *, long (*)(MAILSTREAM *));
long    flag_search_sequence(MAILSTREAM *, MSGNO_S *, long, int);



/*----------------------------------------------------------------------
  Return sequence number based on given index that are search-worthy

    Args: stream --
	  msgmap --
	  msgno --
	  flags -- flags for msgline_hidden

  Result:  0		     : index not search-worthy
	  -1		     : index out of bounds
	   1 - stream->nmsgs : sequence number to flag search

  ----*/
long
flag_search_sequence(MAILSTREAM *stream, MSGNO_S *msgmap, long int msgno, int flags)
{
    long rawno = msgno;

    return((msgno > stream->nmsgs
	    || msgno <= 0
            || (msgmap && !(rawno = mn_m2raw(msgmap, msgno))))
	      ? -1L		/* out of range! */
	      : ((get_lflag(stream, NULL, rawno, MN_EXLD)
		  || (msgmap && msgline_hidden(stream, msgmap, msgno, flags)))
		    ? 0L		/* NOT interesting! */
		    : rawno));
}



/*----------------------------------------------------------------------
  Perform mail_search based on flag bits

    Args: stream --
	  flags --
	  start --
	  msgmap --

  Result: if no set_* specified, call mail_search to light the searched
	  bit for all the messages matching the given flags.  If set_start
	  specified, it is an index (possibly into set_msgmap) telling
	  us where to search for invalid flag state hence when we
	  return everything with the searched bit is interesting and
	  everything with the valid bit lit is believably valid.

  ----*/
void
flag_search(MAILSTREAM *stream, int flags, MsgNo set_start, MSGNO_S *set_msgmap,
	    long int (*ping)(MAILSTREAM *))
{
    long	        n, i, new;
    char	       *seq;
    SEARCHPGM	       *pgm;
    SEARCHSET	       *full_set = NULL, **set;
    MESSAGECACHE       *mc;
    extern  MAILSTREAM *mm_search_stream;

    if(!stream)
      return;

    new = sp_new_mail_count(stream);

    /* Anything we don't already have flags for? */
    if(set_start){
	/*
	 * Use elt's sequence bit to coalesce runs in ascending
	 * sequence order...
	 */
	for(i = 1L; i <= stream->nmsgs; i++)
	  if((mc = mail_elt(stream, i)) != NULL)
	    mc->sequence = 0;

	for(i = set_start;
	    (n = flag_search_sequence(stream, set_msgmap, i, MH_ANYTHD)) >= 0L;
	    (flags & F_SRCHBACK) ? i-- : i++)
	  if(n > 0L && n <= stream->nmsgs
	     && (mc = mail_elt(stream, n)) && !mc->valid)
	    mc->sequence = 1;

	/* Unroll searchset in ascending sequence order */
	set = &full_set;
	for(i = 1L; i <= stream->nmsgs; i++)
	  if((mc = mail_elt(stream, i)) && mc->sequence){
	      if(*set){
		  if(((*set)->last ? (*set)->last : (*set)->first)+1L == i)
		    (*set)->last = i;
		  else
		    set = &(*set)->next;
	      }

	      if(!*set){
		  *set = mail_newsearchset();
		  (*set)->first = i;
	      }
	  }

	/*
	 * No search-worthy messsages?, prod the server for
	 * any flag updates and clear the searched bits...
	 */
	if(full_set){
	    if(full_set->first == 1
	       && full_set->last == stream->nmsgs
	       && full_set->next == NULL)
	      mail_free_searchset(&full_set);
	}
	else{
	    for(i = 1L; i <= stream->nmsgs; i++)
	      if((mc = mail_elt(stream, i)) != NULL)
	        mc->searched = 0;

	    if(ping)
	      (*ping)(stream);	/* prod server for any flag updates */

	    if(!(flags & F_NOFILT) && new != sp_new_mail_count(stream)){
		process_filter_patterns(stream, sp_msgmap(stream),
					sp_new_mail_count(stream));

		refresh_sort(stream, sp_msgmap(stream), SRT_NON);
		flag_search(stream, flags, set_start, set_msgmap, ping);
	    }

	    return;
	}
    }

    if((!is_imap_stream(stream) || modern_imap_stream(stream))
       && !(IS_NEWS(stream))){
	pgm = mail_newsearchpgm();

	if(flags & F_SEEN)
	  pgm->seen = 1;

	if(flags & F_UNSEEN)
	  pgm->unseen = 1;

	if(flags & F_DEL)
	  pgm->deleted = 1;

	if(flags & F_UNDEL)
	  pgm->undeleted = 1;

	if(flags & F_ANS)
	  pgm->answered = 1;

	if(flags & F_UNANS)
	  pgm->unanswered = 1;

	if(flags & F_FLAG)
	  pgm->flagged = 1;

	if(flags & F_UNFLAG)
	  pgm->unflagged = 1;

	if(flags & (F_FWD | F_UNFWD)){
	    STRINGLIST **slpp;

	    for(slpp = (flags & F_FWD) ? &pgm->keyword : &pgm->unkeyword;
		*slpp;
		slpp = &(*slpp)->next)
	      ;

	    *slpp = mail_newstringlist();
	    (*slpp)->text.data = (unsigned char *) cpystr(FORWARDED_FLAG);
	    (*slpp)->text.size = (unsigned long) strlen(FORWARDED_FLAG);
	}

	if(flags & F_RECENT)
	  pgm->recent = 1;

	if(flags & F_OR_SEEN){
	    SEARCHPGM *pgm2 = mail_newsearchpgm();
	    pgm2->or = mail_newsearchor();
	    pgm2->or->first = pgm;
	    pgm2->or->second = mail_newsearchpgm();
	    pgm2->or->second->seen = 1;
	    pgm = pgm2;
	}

	if(flags & F_OR_UNSEEN){
	    SEARCHPGM *pgm2 = mail_newsearchpgm();
	    pgm2->or = mail_newsearchor();
	    pgm2->or->first = pgm;
	    pgm2->or->second = mail_newsearchpgm();
	    pgm2->or->second->unseen = 1;
	    pgm = pgm2;
	}

	if(flags & F_OR_DEL){
	    SEARCHPGM *pgm2 = mail_newsearchpgm();
	    pgm2->or = mail_newsearchor();
	    pgm2->or->first = pgm;
	    pgm2->or->second = mail_newsearchpgm();
	    pgm2->or->second->deleted = 1;
	    pgm = pgm2;
	}

	if(flags & F_OR_UNDEL){
	    SEARCHPGM *pgm2 = mail_newsearchpgm();
	    pgm2->or = mail_newsearchor();
	    pgm2->or->first = pgm;
	    pgm2->or->second = mail_newsearchpgm();
	    pgm2->or->second->undeleted = 1;
	    pgm = pgm2;
	}

	if(flags & F_OR_FLAG){
	    SEARCHPGM *pgm2 = mail_newsearchpgm();
	    pgm2->or = mail_newsearchor();
	    pgm2->or->first = pgm;
	    pgm2->or->second = mail_newsearchpgm();
	    pgm2->or->second->flagged = 1;
	    pgm = pgm2;
	}

	if(flags & F_OR_UNFLAG){
	    SEARCHPGM *pgm2 = mail_newsearchpgm();
	    pgm2->or = mail_newsearchor();
	    pgm2->or->first = pgm;
	    pgm2->or->second = mail_newsearchpgm();
	    pgm2->or->second->unflagged = 1;
	    pgm = pgm2;
	}

	if(flags & F_OR_ANS){
	    SEARCHPGM *pgm2 = mail_newsearchpgm();
	    pgm2->or = mail_newsearchor();
	    pgm2->or->first = pgm;
	    pgm2->or->second = mail_newsearchpgm();
	    pgm2->or->second->answered = 1;
	    pgm = pgm2;
	}

	if(flags & F_OR_UNANS){
	    SEARCHPGM *pgm2 = mail_newsearchpgm();
	    pgm2->or = mail_newsearchor();
	    pgm2->or->first = pgm;
	    pgm2->or->second = mail_newsearchpgm();
	    pgm2->or->second->unanswered = 1;
	    pgm = pgm2;
	}

	if(flags & (F_OR_FWD | F_OR_UNFWD)){
	    STRINGLIST **slpp;
	    SEARCHPGM *pgm2 = mail_newsearchpgm();
	    pgm2->or = mail_newsearchor();
	    pgm2->or->first = pgm;
	    pgm2->or->second = mail_newsearchpgm();

	    for(slpp = (flags & F_OR_FWD)
			 ? &pgm2->or->second->keyword
			 : &pgm2->or->second->unkeyword;
		*slpp;
		slpp = &(*slpp)->next)
	      ;

	    *slpp = mail_newstringlist();
	    (*slpp)->text.data = (unsigned char *) cpystr(FORWARDED_FLAG);
	    (*slpp)->text.size = (unsigned long) strlen(FORWARDED_FLAG);

	    pgm = pgm2;
	}

	if(flags & F_OR_RECENT){
	    SEARCHPGM *pgm2 = mail_newsearchpgm();
	    pgm2->or = mail_newsearchor();
	    pgm2->or->first = pgm;
	    pgm2->or->second = mail_newsearchpgm();
	    pgm2->or->second->recent = 1;
	    pgm = pgm2;
	}

	pgm->msgno = full_set;

	pine_mail_search_full(mm_search_stream = stream, NULL,
			      pgm, SE_NOPREFETCH | SE_FREE);

	if(!(flags & F_NOFILT) && new != sp_new_mail_count(stream)){
	    process_filter_patterns(stream, sp_msgmap(stream),
				    sp_new_mail_count(stream));

	    flag_search(stream, flags, set_start, set_msgmap, ping);
	}
    }
    else{
	if(full_set){
	    /* sequence bits of interesting msgs set */
	    mail_free_searchset(&full_set);
	}
	else{
	    /* light sequence bits of interesting msgs */
	    for(i = 1L;
		(n = flag_search_sequence(stream, set_msgmap, i, MH_ANYTHD)) >= 0L;
		i++)
	      if(n > 0L && n <= stream->nmsgs
		 && (mc = mail_elt(stream, n)) && !mc->valid)
		mc->sequence = 1;
	      else
		mc->sequence = 0;
	}

	for(i = 1L; i <= stream->nmsgs; i++)
	  if((mc = mail_elt(stream, i)) != NULL)
	    mc->searched = 0;

	if((seq = build_sequence(stream, NULL, NULL)) != NULL){
	    pine_mail_fetch_flags(stream, seq, 0L);
	    fs_give((void **) &seq);
	}
    }
}



/*----------------------------------------------------------------------
    count messages on stream with specified system flag attributes

  Args: stream -- The stream/folder to look at message status
        flags -- flags on folder/stream to examine

 Result: count of messages flagged as requested

 Task: return count of message flagged as requested while being
       as server/network friendly as possible.

 Strategy: run thru flags to make sure they're all valid.  If any
	   invalid, do a search starting with the invalid message.
	   If all valid, ping the server to let it know we're 
	   receptive to flag updates.  At this 

  ----------------------------------------------------------------------*/
long
count_flagged(MAILSTREAM *stream, long int flags)
{
    long		n, count;
    MESSAGECACHE       *mc;

    if(!stream)
      return(0L);

    flag_search(stream, flags, 1, NULL, pine_mail_ping);

    /* Paw thru once more since all should be updated */
    for(n = 1L, count = 0L; n <= stream->nmsgs; n++)
      if((((mc = mail_elt(stream, n)) && mc->searched)
	  || (mc && mc->valid && FLAG_MATCH(flags, mc, stream)))
	 && !get_lflag(stream, NULL, n, MN_EXLD)){	  
	  mc->searched = 1;	/* caller may be interested! */
	  count++;
      }

    return(count);
}



/*----------------------------------------------------------------------
     Find the first message with the specified flags set

  Args: flags -- Flags in messagecache to match on
        stream -- The stream/folder to look at message status

 Result: Message number of first message with specified flags set or the
	 number of the last message if none found.
  ----------------------------------------------------------------------*/
MsgNo
first_sorted_flagged(long unsigned int flags, MAILSTREAM *stream, long int set_start, int opts)
{
    MsgNo	  i, n, start_with, winner = 0L;
    MESSAGECACHE *mc;
    int		  last;
    MSGNO_S      *msgmap;

    msgmap = sp_msgmap(stream);

    last = (opts & FSF_LAST);

    /* set_start only affects which search bits we light */
    start_with = set_start ? set_start
			   : (flags & F_SRCHBACK)
			       ? mn_get_total(msgmap) : 1L;
    flag_search(stream, flags, start_with, msgmap, NULL);

    for(i = start_with;
	(n = flag_search_sequence(stream, msgmap, i,
				 (opts & FSF_SKIP_CHID) ? 0 : MH_ANYTHD)) >= 0L;
	(flags & F_SRCHBACK) ? i-- : i++)
      if(n > 0L && n <= stream->nmsgs
	 && (((mc = mail_elt(stream, n)) && mc->searched)
	     || (mc && mc->valid && FLAG_MATCH(flags, mc, stream)))){
	  winner = i;
	  if(!last)
	    break;
      }

    if(winner == 0L && flags != F_UNDEL && flags != F_NONE){
	dprint((4,
	   "First_sorted_flagged didn't find a winner, look for undeleted\n"));
	winner = first_sorted_flagged(F_UNDEL, stream, 0L,
		opts | (mn_get_revsort(msgmap) ? 0 : FSF_LAST));
    }

    if(winner == 0L && flags != F_NONE){
	dprint((4,
          "First_sorted_flagged didn't find an undeleted, look for visible\n"));
	winner = first_sorted_flagged(F_NONE, stream, 0L,
		opts | (mn_get_revsort(msgmap) ? 0 : FSF_LAST));
    }

    dprint((4,
	       "First_sorted_flagged returning winner = %ld\n", winner));
    return(winner ? winner
		  : (mn_get_revsort(msgmap)
		      ? 1L : mn_get_total(msgmap)));
}



/*----------------------------------------------------------------------
     Find the next message with specified flags set

  Args: flags  -- Flags in messagecache to match on
        stream -- The stream/folder to look at message status
        start  -- Start looking after this message
        opts   -- These bits are both input and output. On input the bit
		  NSF_TRUST_FLAGS tells us whether we need to ping or not.
		  On input, the bit NSF_SEARCH_BACK tells us that we want to
		  know about matches <= start if we don't find any > start.
		  On output, NSF_FLAG_MATCH is set if we matched a message.
  Returns: Message number of the matched message, if any; else the start # or
	   the max_msgno if the mailbox changed dramatically.
  ----------------------------------------------------------------------*/
MsgNo
next_sorted_flagged(long unsigned int flags, MAILSTREAM *stream, long int start, int *opts)
{
    MsgNo	  i, n, dir;
    MESSAGECACHE *mc;
    int           rev, fss_flags = 0;
    MSGNO_S      *msgmap;

    msgmap = sp_msgmap(stream);

    /*
     * Search for the next thing the caller's interested in...
     */

    fss_flags = (opts && *opts & NSF_SKIP_CHID) ? 0 : MH_ANYTHD;
    rev = (opts && *opts & NSF_SEARCH_BACK);
    dir = (rev ? -1L : 1L);

    flag_search(stream, flags | (rev ? F_SRCHBACK : 0), start + dir,
		msgmap,
		(opts && ((*opts) & NSF_TRUST_FLAGS)) ? NULL : pine_mail_ping);

    for(i = start + dir;
	(n = flag_search_sequence(stream, msgmap,
				  i, fss_flags)) >= 0L;
	i += dir)
      if(n > 0L && n <= stream->nmsgs
	 && (((mc = mail_elt(stream, n)) && mc->searched)
	     || (mc && mc->valid && FLAG_MATCH(flags, mc, stream)))){
	  /* actually found a msg matching the flags */
	  if(opts)
	    (*opts) |= NSF_FLAG_MATCH;

	  return(i);
      }
    

    return(MIN(start, mn_get_total(msgmap)));
}



/*----------------------------------------------------------------------
  get the requested LOCAL flag bits for the given pine message number

   Accepts: msgs - pointer to message manipulation struct
            n - message number to get
	    f - bitmap of interesting flags
   Returns: non-zero if flag set, 0 if not set or no elt (error?)

   NOTE: this can be used to test system flags
  ----*/
int
get_lflag(MAILSTREAM *stream, MSGNO_S *msgs, long int n, int f)
{
    MESSAGECACHE *mc;
    PINELT_S	 *pelt;
    unsigned long rawno;

    rawno = msgs ? mn_m2raw(msgs, n) : n;
    if(!stream || rawno < 1L || rawno > stream->nmsgs)
      return(0);

    mc = mail_elt(stream, rawno);
    if(!mc || (pelt = (PINELT_S *) mc->sparep) == NULL)
      return(f ? 0 : 1);

    return((!f)
	     ? !(pelt->hidden || pelt->excluded || pelt->selected ||
		 pelt->colhid || pelt->collapsed || pelt->searched)
	     : (((f & MN_HIDE) ? pelt->hidden : 0)
		|| ((f & MN_EXLD) ? pelt->excluded : 0)
		|| ((f & MN_SLCT) ? pelt->selected : 0)
		|| ((f & MN_STMP) ? pelt->tmp : 0)
		|| ((f & MN_USOR) ? pelt->unsorted : 0)
		|| ((f & MN_COLL) ? pelt->collapsed : 0)
		|| ((f & MN_CHID) ? pelt->colhid : 0)
		|| ((f & MN_CHID2) ? pelt->colhid2 : 0)
		|| ((f & MN_SRCH) ? pelt->searched : 0)));
}



/*----------------------------------------------------------------------
  set the requested LOCAL flag bits for the given pine message number

   Accepts: msgs - pointer to message manipulation struct
            n - message number to set
	    f - bitmap of interesting flags
	    v - value (on or off) flag should get
   Returns: our index number of first

   NOTE: this isn't to be used for setting IMAP system flags
  ----*/
int
set_lflag(MAILSTREAM *stream, MSGNO_S *msgs, long int n, int f, int v)
{
    MESSAGECACHE *mc;
    long          rawno = 0L;
    PINETHRD_S   *thrd, *topthrd = NULL;
    PINELT_S    **peltp, *pelt;

    if(n < 1L || n > mn_get_total(msgs))
      return(0L);

    if((rawno=mn_m2raw(msgs, n)) > 0L && stream && rawno <= stream->nmsgs
       && (mc = mail_elt(stream, rawno))){
	int was_invisible, is_invisible;
	int chk_thrd_cnt = 0, thrd_was_visible, was_hidden, is_hidden;

	if((*(peltp = (PINELT_S **) &mc->sparep) == NULL)){
	    *peltp = (PINELT_S *) fs_get(sizeof(PINELT_S));
	    memset(*peltp, 0, sizeof(PINELT_S));
	}

	pelt = (*peltp);

	was_invisible = (pelt->hidden || pelt->colhid) ? 1 : 0;

	if((chk_thrd_cnt = ((msgs->visible_threads >= 0L)
	   && THRD_INDX_ENABLED() && (f & MN_HIDE) && (pelt->hidden != v))) != 0){
	    thrd = fetch_thread(stream, rawno);
	    if(thrd && thrd->top){
		if(thrd->top == thrd->rawno)
		  topthrd = thrd;
		else
		  topthrd = fetch_thread(stream, thrd->top);
	    }

	    if(topthrd){
		thrd_was_visible = thread_has_some_visible(stream, topthrd);
		was_hidden = pelt->hidden ? 1 : 0;
	    }
	}

	if((f & MN_HIDE) && pelt->hidden != v){
	    pelt->hidden = v;
	    msgs->flagged_hid += (v) ? 1L : -1L;

	    if(pelt->hidden && THREADING() && !THRD_INDX()
	       && stream == ps_global->mail_stream
	       && ps_global->thread_disp_style == THREAD_MUTTLIKE)
	      clear_index_cache_for_thread(stream, fetch_thread(stream, rawno),
					   sp_msgmap(stream));
	}

	if((f & MN_CHID) && pelt->colhid != v){
	    pelt->colhid = v;
	    msgs->flagged_chid += (v) ? 1L : -1L;
	}

	if((f & MN_CHID2) && pelt->colhid2 != v){
	    pelt->colhid2 = v;
	    msgs->flagged_chid2 += (v) ? 1L : -1L;
	}

	if((f & MN_COLL) && pelt->collapsed != v){
	    pelt->collapsed = v;
	    msgs->flagged_coll += (v) ? 1L : -1L;
	}

	if((f & MN_USOR) && pelt->unsorted != v){
	    pelt->unsorted = v;
	    msgs->flagged_usor += (v) ? 1L : -1L;
	}

	if((f & MN_EXLD) && pelt->excluded != v){
	    pelt->excluded = v;
	    msgs->flagged_exld += (v) ? 1L : -1L;
	}

	if((f & MN_SLCT) && pelt->selected != v){
	    pelt->selected = v;
	    msgs->flagged_tmp += (v) ? 1L : -1L;
	}

	if((f & MN_SRCH) && pelt->searched != v){
	    pelt->searched = v;
	    msgs->flagged_srch += (v) ? 1L : -1L;
	}

	if((f & MN_STMP) && pelt->tmp != v){
	    pelt->tmp = v;
	    msgs->flagged_stmp += (v) ? 1L : -1L;
	}

	is_invisible = (pelt->hidden || pelt->colhid) ? 1 : 0;

	if(was_invisible != is_invisible)
	  msgs->flagged_invisible += (v) ? 1L : -1L;
	
	/*
	 * visible_threads keeps track of how many of the max_thrdno threads
	 * are visible and how many are MN_HIDE-hidden.
	 */
	if(chk_thrd_cnt && topthrd
	   && (was_hidden != (is_hidden = pelt->hidden ? 1 : 0))){
	    if(!thrd_was_visible && !is_hidden){
		/* it is visible now, increase count by one */
		msgs->visible_threads++;
	    }
	    else if(thrd_was_visible && is_hidden){
		/* thread may have been hidden, check */
		if(!thread_has_some_visible(stream, topthrd))
		  msgs->visible_threads--;
	    }
	    /* else no change */
	}
    }

    return(1);
}


/*
 * Copy value of flag from to flag to.
 */
void
copy_lflags(MAILSTREAM *stream, MSGNO_S *msgmap, int from, int to)
{
    unsigned long i;
    int           hide;

    hide = ((to == MN_SLCT) && (any_lflagged(msgmap, MN_HIDE) > 0L));

    set_lflags(stream, msgmap, to, 0);

    if(any_lflagged(msgmap, from))
      for(i = 1L; i <= mn_get_total(msgmap); i++)
	if(get_lflag(stream, msgmap, i, from))
	  set_lflag(stream, msgmap, i, to, 1);
	else if(hide)
	  set_lflag(stream, msgmap, i, MN_HIDE, 1);
}


/*
 * Set flag f to value v in all message.
 */
void
set_lflags(MAILSTREAM *stream, MSGNO_S *msgmap, int f, int v)
{
    unsigned long i;

    if((v == 0 && any_lflagged(msgmap, f)) || v )
      for(i = 1L; i <= mn_get_total(msgmap); i++)
	set_lflag(stream, msgmap, i, f, v);
}



/*----------------------------------------------------------------------
  return whether the given flag is set somewhere in the folder

   Accepts: msgs - pointer to message manipulation struct
	    f - flag bitmap to act on
   Returns: number of messages with the given flag set.
	    NOTE: the sum, if multiple flags tested, is bogus
  ----*/
long
any_lflagged(MSGNO_S *msgs, int f)
{
    if(!msgs)
      return(0L);

    if(f == MN_NONE)
      return(!(msgs->flagged_hid || msgs->flagged_exld || msgs->flagged_tmp ||
	       msgs->flagged_coll || msgs->flagged_chid || msgs->flagged_srch));
    else if(f == (MN_HIDE | MN_CHID))
      return(msgs->flagged_invisible);		/* special non-bogus case */
    else
      return(((f & MN_HIDE)   ? msgs->flagged_hid  : 0L)
	     + ((f & MN_EXLD) ? msgs->flagged_exld : 0L)
	     + ((f & MN_SLCT) ? msgs->flagged_tmp  : 0L)
	     + ((f & MN_SRCH) ? msgs->flagged_srch  : 0L)
	     + ((f & MN_STMP) ? msgs->flagged_stmp  : 0L)
	     + ((f & MN_COLL) ? msgs->flagged_coll  : 0L)
	     + ((f & MN_USOR) ? msgs->flagged_usor  : 0L)
	     + ((f & MN_CHID) ? msgs->flagged_chid : 0L)
	     + ((f & MN_CHID2) ? msgs->flagged_chid2 : 0L));
}
