#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: msgno.c 854 2007-12-07 17:44:43Z hubert@u.washington.edu $";
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

#include "../pith/headers.h"
#include "../pith/msgno.h"
#include "../pith/flag.h"
#include "../pith/mailindx.h"
#include "../pith/pineelt.h"
#include "../pith/icache.h"


/* internal prototypes */
void    set_msg_score(MAILSTREAM *, long, long);


/*
 *           * * *  Message number management functions  * * *
 */


/*----------------------------------------------------------------------
  Initialize a message manipulation structure for the given total

   Accepts: msgs - pointer to pointer to message manipulation struct
	    tot - number of messages to initialize with
  ----*/
void
msgno_init(MSGNO_S **msgs, long int tot, SortOrder def_sort, int def_sort_rev)
{
    long   slop = (tot + 1L) % 64;
    size_t len;

    if(!msgs)
      return;

    if(!(*msgs)){
	(*msgs) = (MSGNO_S *)fs_get(sizeof(MSGNO_S));
	memset((void *)(*msgs), 0, sizeof(MSGNO_S));
    }

    (*msgs)->sel_cur  = 0L;
    (*msgs)->sel_cnt  = 1L;
    (*msgs)->sel_size = 8L;
    len		      = (size_t)(*msgs)->sel_size * sizeof(long);
    if((*msgs)->select)
      fs_resize((void **)&((*msgs)->select), len);
    else
      (*msgs)->select = (long *)fs_get(len);

    (*msgs)->select[0] = (tot) ? 1L : 0L;

    (*msgs)->sort_size = (tot + 1L) + (64 - slop);
    len		       = (size_t)(*msgs)->sort_size * sizeof(long);
    if((*msgs)->sort)
      fs_resize((void **)&((*msgs)->sort), len);
    else
      (*msgs)->sort = (long *)fs_get(len);

    memset((void *)(*msgs)->sort, 0, len);
    for(slop = 1L ; slop <= tot; slop++)	/* reusing "slop" */
      (*msgs)->sort[slop] = slop;

    /*
     * If there is filtering happening, isort will become larger than sort.
     * Sort is a list of raw message numbers in their sorted order. There
     * are missing raw numbers because some of the messages are excluded
     * (MN_EXLD) from the view. Isort has one entry for every raw message
     * number, which maps to the corresponding msgno (the row in the sort
     * array). Some of the entries in isort are not used because those
     * messages are excluded, but the entry is still there because we want
     * to map from rawno to message number and the row number is the rawno.
     */
    (*msgs)->isort_size = (*msgs)->sort_size;
    if((*msgs)->isort)
      fs_resize((void **)&((*msgs)->isort), len);
    else
      (*msgs)->isort = (long *)fs_get(len);

    (*msgs)->max_msgno    = tot;
    (*msgs)->nmsgs	  = tot;

    /* set the inverse array */
    msgno_reset_isort(*msgs);

    (*msgs)->sort_order   = def_sort;
    (*msgs)->reverse_sort = def_sort_rev;
    (*msgs)->flagged_hid  = 0L;
    (*msgs)->flagged_exld = 0L;
    (*msgs)->flagged_chid = 0L;
    (*msgs)->flagged_chid2= 0L;
    (*msgs)->flagged_coll = 0L;
    (*msgs)->flagged_usor = 0L;
    (*msgs)->flagged_tmp  = 0L;
    (*msgs)->flagged_stmp = 0L;

    /*
     * This one is the total number of messages which are flagged
     * hid OR chid. It isn't the sum of those two because a
     * message may be flagged both at the same time.
     */
    (*msgs)->flagged_invisible = 0L;

    /*
     * And this keeps track of visible threads in the THRD_INDX. This is
     * weird because a thread is visible if any of its messages are
     * not hidden, including those that are CHID hidden. You can't just
     * count up all the messages that are hid or chid because you would
     * miss a thread that has its top-level message hidden but some chid
     * message not hidden.
     */
    (*msgs)->visible_threads = -1L;
}


/*
 * Isort makes mn_raw2m fast. Alternatively, we could look through
 * the sort array to do mn_raw2m.
 */
void
msgno_reset_isort(MSGNO_S *msgs)
{
    long i;

    if(msgs){
	/*
	 * Zero isort so raw messages numbers which don't appear in the
	 * sort array show up as undefined.
	 */
	memset((void *) msgs->isort, 0,
	       (size_t) msgs->isort_size * sizeof(long));

	/* fill in all the defined entries */
	for(i = 1L; i <= mn_get_total(msgs); i++)
	  msgs->isort[msgs->sort[i]] = i;
    }
}



/*----------------------------------------------------------------------
  Release resources of a message manipulation structure

   Accepts: msgs - pointer to message manipulation struct
	    n - number to test
   Returns: with specified structure and its members free'd
  ----*/
void
msgno_give(MSGNO_S **msgs)
{
    if(msgs && *msgs){
	if((*msgs)->sort)
	  fs_give((void **) &((*msgs)->sort));

	if((*msgs)->isort)
	  fs_give((void **) &((*msgs)->isort));

	if((*msgs)->select)
	  fs_give((void **) &((*msgs)->select));

	fs_give((void **) msgs);
    }
}



/*----------------------------------------------------------------------
  Release resources of a message part exception list

   Accepts: parts -- list of parts to free
   Returns: with specified structure and its members free'd
  ----*/
void
msgno_free_exceptions(PARTEX_S **parts)
{
    if(parts && *parts){
	if((*parts)->next)
	  msgno_free_exceptions(&(*parts)->next);

	fs_give((void **) &(*parts)->partno);
	fs_give((void **) parts);
    }
}



/*----------------------------------------------------------------------
 Increment the current message number

   Accepts: msgs - pointer to message manipulation struct
  ----*/
void
msgno_inc(MAILSTREAM *stream, MSGNO_S *msgs, int flags)
{
    long i;

    if(!msgs || mn_get_total(msgs) < 1L)
      return;
    
    for(i = msgs->select[msgs->sel_cur] + 1; i <= mn_get_total(msgs); i++){
        if(!msgline_hidden(stream, msgs, i, flags)){
	    (msgs)->select[((msgs)->sel_cur)] = i;
	    break;
	}
    }
}
 


/*----------------------------------------------------------------------
  Decrement the current message number

   Accepts: msgs - pointer to message manipulation struct
  ----*/
void
msgno_dec(MAILSTREAM *stream, MSGNO_S *msgs, int flags)
{
    long i;

    if(!msgs || mn_get_total(msgs) < 1L)
      return;

    for(i = (msgs)->select[((msgs)->sel_cur)] - 1L; i >= 1L; i--){
        if(!msgline_hidden(stream, msgs, i, flags)){
	    (msgs)->select[((msgs)->sel_cur)] = i;
	    break;
	}
    }
}



/*----------------------------------------------------------------------
  Got thru the message mapping table, and remove messages with DELETED flag

   Accepts: stream -- mail stream to removed message references from
	    msgs -- pointer to message manipulation struct
	    f -- flags to use a purge criteria
  ----*/
void
msgno_exclude_deleted(MAILSTREAM *stream, MSGNO_S *msgs)
{
    long	  i, rawno;
    MESSAGECACHE *mc;
    int           need_isort_reset = 0;

    if(!msgs || msgs->max_msgno < 1L)
      return;

    /*
     * With 3.91 we're using a new strategy for finding and operating
     * on all the messages with deleted status.  The idea is to do a
     * mail_search for deleted messages so the elt's "searched" bit gets
     * set, and then to scan the elt's for them and set our local bit
     * to indicate they're excluded...
     */
    (void) count_flagged(stream, F_DEL);

    /*
     * Start with the end of the folder and work backwards so that
     * msgno_exclude doesn't have to shift the entire array each time when
     * there are lots of deleteds. In fact, if everything is deleted (like
     * might be the case in a huge newsgroup) then it never has to shift
     * anything. It is always at the end of the array just eliminating the
     * last one instead. So instead of an n**2 operation, it is n.
     */
    for(i = msgs->max_msgno; i >= 1L; i--)
      if((rawno = mn_m2raw(msgs, i)) > 0L && stream && rawno <= stream->nmsgs
	 && (mc = mail_elt(stream, rawno))
	 && ((mc->valid && mc->deleted) || (!mc->valid && mc->searched))){
	  msgno_exclude(stream, msgs, i, 0);
	  need_isort_reset++;
      }
    
    if(need_isort_reset)
      msgno_reset_isort(msgs);

    /*
     * If we excluded away a zoomed display, unhide everything...
     */
    if(msgs->max_msgno > 0L && any_lflagged(msgs, MN_HIDE) >= msgs->max_msgno)
      for(i = 1L; i <= msgs->max_msgno; i++)
	set_lflag(stream, msgs, i, MN_HIDE, 0);
}



void
msgno_exclude(MAILSTREAM *stream, MSGNO_S *msgmap, long int msgno, int reset_isort)
{
    long i;

    /*--- clear all flags to keep our counts consistent  ---*/
    set_lflag(stream, msgmap, msgno, MN_HIDE | MN_CHID | MN_CHID2 | MN_SLCT, 0);
    set_lflag(stream, msgmap, msgno, MN_EXLD, 1); /* mark excluded */

    /* erase knowledge in sort array (shift array down) */
    for(i = msgno + 1L; i <= msgmap->max_msgno; i++)
      msgmap->sort[i-1L] = msgmap->sort[i];

    msgmap->max_msgno = MAX(0L, msgmap->max_msgno - 1L);
    if(reset_isort)
      msgno_reset_isort(msgmap);

    msgno_flush_selected(msgmap, msgno);
}



/*----------------------------------------------------------------------
   Accepts: stream -- mail stream to removed message references from
	    msgs -- pointer to message manipulation struct
	    flags
	      MI_REFILTERING  -- do includes appropriate for refiltering
	      MI_STATECHGONLY -- when refiltering, maybe only re-include
	                         messages which have had state changes
				 since they were originally filtered
   Returns 1 if any new messages are included (indicating that we need
              to re-sort)
	   0 if no new messages are included
  ----*/
int
msgno_include(MAILSTREAM *stream, MSGNO_S *msgs, int flags)
{
    long   i, slop, old_total, old_size;
    int    exbits, ret = 0;
    size_t len;
    MESSAGECACHE *mc;

    if(!msgs)
      return(ret);

    for(i = 1L; i <= stream->nmsgs; i++){
	if(!msgno_exceptions(stream, i, "0", &exbits, FALSE))
	  exbits = 0;

	if((((flags & MI_REFILTERING) && (exbits & MSG_EX_FILTERED)
	     && !(exbits & MSG_EX_FILED)
	     && (!(flags & MI_STATECHGONLY) || (exbits & MSG_EX_STATECHG)))
	    || (!(flags & MI_REFILTERING) && !(exbits & MSG_EX_FILTERED)))
	   && get_lflag(stream, NULL, i, MN_EXLD)){
	    old_total	     = msgs->max_msgno;
	    old_size	     = msgs->sort_size;
	    slop	     = (msgs->max_msgno + 1L) % 64;
	    msgs->sort_size  = (msgs->max_msgno + 1L) + (64 - slop);
	    len		     = (size_t) msgs->sort_size * sizeof(long);
	    if(msgs->sort){
		if(old_size != msgs->sort_size)
		  fs_resize((void **)&(msgs->sort), len);
	    }
	    else
	      msgs->sort = (long *)fs_get(len);

	    ret = 1;
	    msgs->sort[++msgs->max_msgno] = i;
	    msgs->isort[i] = msgs->max_msgno;
	    set_lflag(stream, msgs, msgs->max_msgno, MN_EXLD, 0);
	    if(flags & MI_REFILTERING){
		exbits &= ~(MSG_EX_FILTERED | MSG_EX_TESTED);
		msgno_exceptions(stream, i, "0", &exbits, TRUE);
	    }

	    if(old_total <= 0L){	/* if no previous messages, */
		if(!msgs->select){	/* select the new message   */
		    msgs->sel_size = 8L;
		    len		   = (size_t)msgs->sel_size * sizeof(long);
		    msgs->select   = (long *)fs_get(len);
		}

		msgs->sel_cnt   = 1L;
		msgs->sel_cur   = 0L;
		msgs->select[0] = 1L;
	    }
	}
	else if((flags & MI_REFILTERING)
		&& (exbits & (MSG_EX_FILTERED | MSG_EX_TESTED))
		&& !(exbits & MSG_EX_FILED)
		&& (!(exbits & MSG_EX_MANUNDEL)
		    || ((mc = mail_elt(stream, i)) && mc->deleted))
	        && (!(flags & MI_STATECHGONLY) || (exbits & MSG_EX_STATECHG))){
	    /*
	     * We get here if the message was filtered by a filter that
	     * just changes status bits (it wasn't excluded), and now also
	     * if the message was merely tested for filtering. It has also
	     * not been manually undeleted. If it was manually undeleted, we
	     * don't want to reprocess the filter, undoing the user's
	     * manual undeleting. Of course, a new pine will re check this
	     * message anyway, so the user had better be using this
	     * manual undeleting only to temporarily save him or herself
	     * from an expunge before Saving or printing or something.
	     * Also, we want to still try filtering if the message has at
	     * all been marked deleted, even if the there was any manual
	     * undeleting, since this directly precedes an expunge, we want
	     * to make sure the filter does the right thing before getting
	     * rid of the message forever.
	     */
	    exbits &= ~(MSG_EX_FILTERED | MSG_EX_TESTED);
	    msgno_exceptions(stream, i, "0", &exbits, TRUE);
	}
    }

    return(ret);
}



/*----------------------------------------------------------------------
 Add the given number of raw message numbers to the end of the
 current list...

   Accepts: msgs - pointer to message manipulation struct
	    n - number to add
   Returns: with fixed up msgno struct

   Only have to adjust the sort array, as since new mail can't cause
   selection!
  ----*/
void
msgno_add_raw(MSGNO_S *msgs, long int n)
{
    long   slop, islop, old_total, old_size, old_isize;
    size_t len, ilen;

    if(!msgs || n <= 0L)
      return;

    old_total        = msgs->max_msgno;
    old_size         = msgs->sort_size;
    old_isize        = msgs->isort_size;
    slop             = (msgs->max_msgno + n + 1L) % 64;
    islop            = (msgs->nmsgs + n + 1L) % 64;
    msgs->sort_size  = (msgs->max_msgno + n + 1L) + (64 - slop);
    msgs->isort_size = (msgs->nmsgs + n + 1L) + (64 - islop);
    len		     = (size_t) msgs->sort_size * sizeof(long);
    ilen	     = (size_t) msgs->isort_size * sizeof(long);
    if(msgs->sort){
	if(old_size != msgs->sort_size)
	  fs_resize((void **) &(msgs->sort), len);
    }
    else
      msgs->sort = (long *) fs_get(len);

    if(msgs->isort){
	if(old_isize != msgs->isort_size)
	  fs_resize((void **) &(msgs->isort), ilen);
    }
    else
      msgs->isort = (long *) fs_get(ilen);

    while(n-- > 0){
	msgs->sort[++msgs->max_msgno] = ++msgs->nmsgs;
	msgs->isort[msgs->nmsgs] = msgs->max_msgno;
    }

    if(old_total <= 0L){			/* if no previous messages, */
	if(!msgs->select){			/* select the new message   */
	    msgs->sel_size = 8L;
	    len		   = (size_t) msgs->sel_size * sizeof(long);
	    msgs->select   = (long *) fs_get(len);
	}

	msgs->sel_cnt   = 1L;
	msgs->sel_cur   = 0L;
	msgs->select[0] = 1L;
    }
}


/*----------------------------------------------------------------------
  Remove all knowledge of the given raw message number

   Accepts: msgs - pointer to message manipulation struct
	    rawno - number to remove
   Returns: with fixed up msgno struct

   After removing *all* references, adjust the sort array and
   various pointers accordingly...
  ----*/
void
msgno_flush_raw(MSGNO_S *msgs, long int rawno)
{
    long i, old_sorted = 0L;
    int  shift = 0;

    if(!msgs)
      return;

    /* blast rawno from sort array */
    for(i = 1L; i <= msgs->max_msgno; i++){
	if(msgs->sort[i] == rawno){
	    old_sorted = i;
	    shift++;
	}

	if(shift && i < msgs->max_msgno)
	  msgs->sort[i] = msgs->sort[i + 1L];

	if(msgs->sort[i] > rawno)
	  msgs->sort[i] -= 1L;
    }

    /*---- now, fixup counts and select array ----*/
    if(--msgs->nmsgs < 0)
      msgs->nmsgs = 0L;

    if(old_sorted){
	if(--msgs->max_msgno < 0)
	  msgs->max_msgno = 0L;

	msgno_flush_selected(msgs, old_sorted);
    }

    msgno_reset_isort(msgs);

    {	char b[100];
	snprintf(b, sizeof(b),
		"isort validity: end of msgno_flush_raw: rawno=%ld\n", rawno);
    }
}


/*----------------------------------------------------------------------
  Remove all knowledge of the given selected message number

   Accepts: msgs - pointer to message manipulation struct
	    n - number to remove
   Returns: with fixed up selec members in msgno struct

   Remove reference and fix up selected message numbers beyond
   the specified number
  ----*/
void
msgno_flush_selected(MSGNO_S *msgs, long int n)
{
    long i;
    int  shift = 0;

    for(i = 0L; i < msgs->sel_cnt; i++){
	if(!shift && (msgs->select[i] == n))
	  shift++;

	if(shift && i + 1L < msgs->sel_cnt)
	  msgs->select[i] = msgs->select[i + 1L];

	if(n < msgs->select[i] || msgs->select[i] > msgs->max_msgno)
	  msgs->select[i] -= 1L;
    }

    if(shift && msgs->sel_cnt > 1L)
      msgs->sel_cnt -= 1L;
}


void
msgno_set_sort(MSGNO_S *msgs, SortOrder sort)
{
    if(msgs){
	if(sort == SortScore)
	  scores_are_used(SCOREUSE_INVALID);

	msgs->sort_order = sort;
    }
}


/*----------------------------------------------------------------------
  Test to see if the given message number is in the selected message
  list...

   Accepts: msgs - pointer to message manipulation struct
	    n - number to test
   Returns: true if n is in selected array, false otherwise

  ----*/
int
msgno_in_select(MSGNO_S *msgs, long int n)
{
    long i;

    if(msgs)
      for(i = 0L; i < msgs->sel_cnt; i++)
	if(msgs->select[i] == n)
	  return(1);

    return(0);
}



/*----------------------------------------------------------------------
  return our index number for the given raw message number

   Accepts: msgs - pointer to message manipulation struct
	    msgno - number that's important
	    part
   Returns: our index number of given raw message

  ----*/
int
msgno_exceptions(MAILSTREAM *stream, long int rawno, char *part, int *bits, int set)
{
    PINELT_S **peltp;
    PARTEX_S **partp;
    MESSAGECACHE *mc;

    if(!stream || rawno < 1L || rawno > stream->nmsgs)
      return FALSE;

    /*
     * Get pointer to exceptional part list, and scan down it
     * for the requested part...
     */
    if((mc = mail_elt(stream, rawno)) && (*(peltp = (PINELT_S **) &mc->sparep)))
      for(partp = &(*peltp)->exceptions; *partp; partp = &(*partp)->next){
	  if(part){
	      if(!strcmp(part, (*partp)->partno)){
		  if(bits){
		      if(set)
			(*partp)->handling = *bits;
		      else
			*bits = (*partp)->handling;
		  }

		  return(TRUE);		/* bingo! */
	      }
	  }
	  else if(bits){
	      /*
	       * The caller provided flags, but no part.
	       * We are looking to see if the bits are set in any of the
	       * parts. This doesn't count parts with non-digit partno's (like
	       * scores) because those are used differently.
	       * any of the flags...
	       */
	      if((*partp)->partno && *(*partp)->partno &&
		 isdigit((unsigned char) *(*partp)->partno) &&
		 (*bits & (*partp)->handling) == *bits)
		return(TRUE);
	  }
	  else
	    /*
	     * The caller didn't specify a part, so
	     * they must just be interested in whether
	     * the msg had any exceptions at all...
	     */
	    return(TRUE);
      }

    if(set && part){
	if(!*peltp){
	    *peltp = (PINELT_S *) fs_get(sizeof(PINELT_S));
	    memset(*peltp, 0, sizeof(PINELT_S));
	    partp = &(*peltp)->exceptions;
	}

	(*partp)	   = (PARTEX_S *) fs_get(sizeof(PARTEX_S));
	(*partp)->partno   = cpystr(part);
	(*partp)->next	   = NULL;
	(*partp)->handling = *bits;
	return(TRUE);
    }

    if(bits)			/* init bits */
      *bits = 0;

    return(FALSE);
}


/*
 * Checks whether any parts of any of the messages in msgmap are marked
 * for deletion.
 */
int
msgno_any_deletedparts(MAILSTREAM *stream, MSGNO_S *msgmap)
{
    long n, rawno;
    PINELT_S  *pelt;
    PARTEX_S **partp;
    MESSAGECACHE *mc;

    for(n = mn_first_cur(msgmap); n > 0L; n = mn_next_cur(msgmap))
      if((rawno = mn_m2raw(msgmap, n)) > 0L
	 && stream && rawno <= stream->nmsgs
	 && (mc = mail_elt(stream, rawno))
	 && (pelt = (PINELT_S *) mc->sparep))
        for(partp = &pelt->exceptions; *partp; partp = &(*partp)->next)
	  if(((*partp)->handling & MSG_EX_DELETE)
	     && (*partp)->partno
	     && *(*partp)->partno != '0'
	     && isdigit((unsigned char) *(*partp)->partno))
	    return(1);

    return(0);
}


int
msgno_part_deleted(MAILSTREAM *stream, long int rawno, char *part)
{
    char *p;
    int   expbits;

    /*
     * Is this attachment or any of it's parents in the
     * MIME structure marked for deletion?
     */
    for(p = part; p && *p; p = strindex(++p, '.')){
	if(*p == '.')
	  *p = '\0';

	(void) msgno_exceptions(stream, rawno, part, &expbits, FALSE);
	if(!*p)
	  *p = '.';

	if(expbits & MSG_EX_DELETE)
	  return(TRUE);
    }

    /* Finally, check if the whole message body's deleted */
    return(msgno_exceptions(stream, rawno, "", &expbits, FALSE)
	   ? (expbits & MSG_EX_DELETE) : FALSE);
}



/* 
 * Set the score for a message to score, which can be anything including
 * SCORE_UNDEF.
 */
void
set_msg_score(MAILSTREAM *stream, long int rawmsgno, long int score)
{
    int intscore;

    /* scores are between SCORE_MIN and SCORE_MAX, so ok */
    intscore = (int) score;

    (void) msgno_exceptions(stream, rawmsgno, "S", &intscore, TRUE);
}


/*
 * Returns the score for a message. If that score is undefined the value
 * returned will be SCORE_UNDEF, so the caller has to be prepared for that.
 * The caller should calculate the undefined scores before calling this.
 */
long
get_msg_score(MAILSTREAM *stream, long int rawmsgno)
{
    int  s;
    long score;

    if(msgno_exceptions(stream, rawmsgno, "S", &s, FALSE))
      score = (long) s;
    else
      score = SCORE_UNDEF;

    return(score);
}


void
clear_msg_score(MAILSTREAM *stream, long int rawmsgno)
{
    if(!stream)
      return;

    set_msg_score(stream, rawmsgno, SCORE_UNDEF);
}


/*
 * Set all the score values to undefined.
 */
void
clear_folder_scores(MAILSTREAM *stream)
{
    long n;

    if(!stream)
      return;

    for(n = 1L; n <= stream->nmsgs; n++)
      clear_msg_score(stream, n);
}


/*
 * Calculates all of the scores for the searchset and stores them in the
 * mail elts. Careful, this function uses patterns so if the caller is using
 * patterns then the caller will probably have to reset the pattern functions.
 * That is, will have to call first_pattern again with the correct type.
 *
 * Args:     stream
 *        searchset -- calculate scores for this set of messages
 *         no_fetch -- we're in a callback from c-client, don't call c-client
 *
 * Returns   1 -- ok
 *           0 -- error, because of no_fetch
 */
int
calculate_some_scores(MAILSTREAM *stream, SEARCHSET *searchset, int no_fetch)
{
    PAT_S         *pat = NULL;
    PAT_STATE      pstate;
    char          *savebits;
    long           newscore, addtoscore, score;
    int            error = 0;
    long           rflags = ROLE_SCORE;
    long           n, i;
    SEARCHSET     *s;
    MESSAGECACHE  *mc;
    HEADER_TOK_S  *hdrtok;

    dprint((7, "calculate_some_scores\n"));

    if(nonempty_patterns(rflags, &pstate)){

	/* calculate scores */
	if(searchset){
	    
	    /* this calls match_pattern which messes up searched bits */
	    savebits = (char *)fs_get((stream->nmsgs+1) * sizeof(char));
	    for(i = 1L; i <= stream->nmsgs; i++)
	      savebits[i] = (mc = mail_elt(stream, i)) ? mc->searched : 0;

	    /*
	     * First set all the scores in the searchset to zero so that they
	     * will no longer be undefined.
	     */
	    score = 0L;
	    for(s = searchset; s; s = s->next)
	      for(n = s->first; n <= s->last; n++)
		set_msg_score(stream, n, score);

	    for(pat = first_pattern(&pstate);
		!error && pat;
		pat = next_pattern(&pstate)){

		newscore = pat->action->scoreval;
		hdrtok = pat->action->scorevalhdrtok;

		/*
		 * This no_fetch probably isn't necessary since
		 * we will actually have fetched this with
		 * the envelope. Just making sure.
		 */
		if(hdrtok && no_fetch){
		    error++;
		    break;
		}

		switch(match_pattern(pat->patgrp, stream, searchset, NULL, NULL,
				     (no_fetch ? MP_IN_CCLIENT_CB : 0)
				      | (SE_NOSERVER|SE_NOPREFETCH))){
		  case 1:
		    if(!pat->action || pat->action->bogus)
		      break;

		    for(s = searchset; s; s = s->next)
		      for(n = s->first; n <= s->last; n++)
			if(n > 0L && stream && n <= stream->nmsgs
			   && (mc = mail_elt(stream, n)) && mc->searched){
			    if((score = get_msg_score(stream,n)) == SCORE_UNDEF)
			      score = 0L;
			    
			    if(hdrtok)
			      addtoscore = scorevalfrommsg(stream, n, hdrtok, no_fetch);
			    else
			      addtoscore = newscore;

			    score += addtoscore;
			    set_msg_score(stream, n, score);
			}

		    break;
		
		  case 0:
		    break;

		  case -1:
		    error++;
		    break;
		}
	    }

	    for(i = 1L; i <= stream->nmsgs; i++)
	      if((mc = mail_elt(stream, i)) != NULL)
		mc->searched = savebits[i];

	    fs_give((void **)&savebits);

	    if(error){
		/*
		 * Revert to undefined scores.
		 */
		score = SCORE_UNDEF;
		for(s = searchset; s; s = s->next)
		  for(n = s->first; n <= s->last; n++)
		    set_msg_score(stream, n, score);
	    }
	}
    }

    return(error ? 0 : 1);
}


void
free_pine_elt(void **sparep)
{
    PINELT_S **peltp;

    peltp = (PINELT_S **) sparep;

    if(peltp && *peltp){
	msgno_free_exceptions(&(*peltp)->exceptions);
	if((*peltp)->pthrd)
	  fs_give((void **) &(*peltp)->pthrd);

	if((*peltp)->ice)
	  free_ice(&(*peltp)->ice);

	fs_give((void **) peltp);
    }
}
