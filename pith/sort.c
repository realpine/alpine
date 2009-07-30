#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: sort.c 1142 2008-08-13 17:22:21Z hubert@u.washington.edu $";
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
#include "../pith/sort.h"
#include "../pith/state.h"
#include "../pith/status.h"
#include "../pith/msgno.h"
#include "../pith/flag.h"
#include "../pith/pineelt.h"
#include "../pith/thread.h"
#include "../pith/search.h"
#include "../pith/pattern.h"
#include "../pith/util.h"
#include "../pith/signal.h"
#include "../pith/busy.h"
#include "../pith/icache.h"


/*
 * global place to store mail_sort and mail_thread results
 */
struct global_sort_data g_sort;


/*
 * Internal prototypes
 */
void	sort_sort_callback(MAILSTREAM *, unsigned long *, unsigned long);
int     percent_sorted(void);
int	pine_compare_long(const qsort_t *, const qsort_t *);
int	pine_compare_long_rev(const qsort_t *, const qsort_t *);
int	pine_compare_scores(const qsort_t *, const qsort_t *);
void	build_score_array(MAILSTREAM *, MSGNO_S *);
void	free_score_array(void);


/*----------------------------------------------------------------------
    Map sort types to names
  ----*/
char *    
sort_name(SortOrder so)
{
    /*
     * Make sure the first upper case letter of any new sort name is
     * unique.  The command char and label for sort selection is 
     * derived from this name and its first upper case character.
     * See mailcmd.c:select_sort().
     */
    return((so == SortArrival)  ? "Arrival" :
	    (so == SortDate)	 ? "Date" :
	     (so == SortSubject)  ? "Subject" :
	      (so == SortCc)	   ? "Cc" :
	       (so == SortFrom)	    ? "From" :
		(so == SortTo)	     ? "To" :
		 (so == SortSize)     ? "siZe" :
		  (so == SortSubject2) ? "OrderedSubj" :
		   (so == SortScore)    ? "scorE" :
		     (so == SortThread)  ? "tHread" :
					  "BOTCH");
}


/*----------------------------------------------------------------------
    Sort the current folder into the order set in the msgmap

Args: msgmap --
      new_sort --
      new_rev -- 

    The idea of the deferred sort is to let the user interrupt a long sort
    and have a chance to do a different command, such as a sort by arrival
    or a Goto.  The next newmail call will increment the deferred variable,
    then the user may do a command, then the newmail call after that
    causes the sort to happen if it is still needed.
  ----*/
void
sort_folder(MAILSTREAM *stream, MSGNO_S *msgmap, SortOrder new_sort,
	    int new_rev, unsigned int flags)
{
    long	   raw_current, i, j;
    unsigned long *sort = NULL;
    int		   we_cancel = 0;
    char	   sort_msg[MAX_SCREEN_COLS+1];
    SortOrder      current_sort;
    int	           current_rev;
    MESSAGECACHE  *mc;

    dprint((2, "Sorting by %s%s\n",
	       sort_name(new_sort), new_rev ? "/reverse" : ""));

    if(!msgmap)
      return;

    current_sort = mn_get_sort(msgmap);
    current_rev = mn_get_revsort(msgmap);
    /*
     * If we were previously threading (spare == 1) and now we're switching
     * sorts (other than just a rev switch) then erase the information
     * about the threaded state (collapsed and so forth).
     */
    if(stream && stream->spare && (current_sort != new_sort))
      erase_threading_info(stream, msgmap);

    if(mn_get_total(msgmap) <= 1L
       && !(mn_get_total(msgmap) == 1L
            && (new_sort == SortThread || new_sort == SortSubject2))){
	mn_set_sort(msgmap, new_sort);
	mn_set_revsort(msgmap, new_rev);
	if(!mn_get_mansort(msgmap))
	  mn_set_mansort(msgmap, (flags & SRT_MAN) ? 1 : 0);
	  
	return;
    }

    raw_current = mn_m2raw(msgmap, mn_get_cur(msgmap));

    if(new_sort == SortArrival){
	/*
	 * NOTE: RE c-client sorting, our idea of arrival is really
	 *	 just the natural sequence order.  C-client, and probably
	 *	 rightly so, considers "arrival" the order based on the
	 *	 message's internal date.  This is more costly to compute
	 *	 since it means touching the message store (say "nntp"?),
	 *	 so we just preempt it here.
	 *
	 *	 Someday c-client will support "unsorted" and do what
	 *	 we're doing here.  That day this gets scrapped.
	 */

	mn_set_sort(msgmap, new_sort);
	mn_set_revsort(msgmap, new_rev);

	if(current_sort != new_sort || current_rev != new_rev ||
	   any_lflagged(msgmap, MN_EXLD))
	  clear_index_cache(stream, 0);

	if(any_lflagged(msgmap, MN_EXLD)){
	    /*
	     * BEWARE: "exclusion" may leave holes in the unsorted sort order
	     * so we have to do a real sort if that is the case.
	     */
	    qsort(msgmap->sort+1, (size_t) mn_get_total(msgmap),
		  sizeof(long),
		  new_rev ? pine_compare_long_rev : pine_compare_long);
	}
	else if(mn_get_total(msgmap) > 0L){
	    if(new_rev){
		clear_index_cache(stream, 0);
		for(i = 1L, j = mn_get_total(msgmap); j >= 1; i++, j--)
		  msgmap->sort[i] = j;
	    }
	    else
	      for(i = 1L; i <= mn_get_total(msgmap); i++)
		msgmap->sort[i] = i;
	}

	/* reset the inverse array */
	msgno_reset_isort(msgmap);
    }
    else if(new_sort == SortScore){

	/*
	 * We have to build a temporary array which maps raw msgno to
	 * score. We use the index cache machinery to build the array.
	 */

	mn_set_sort(msgmap, new_sort);
	mn_set_revsort(msgmap, new_rev);
	
	if(flags & SRT_VRB){
	    /* TRANSLATORS: tell user they are waiting for Sorting of %s, the foldername */
	    snprintf(sort_msg, sizeof(sort_msg), _("Sorting \"%s\""),
		    strsquish(tmp_20k_buf + 500, 500, ps_global->cur_folder,
			      ps_global->ttyo->screen_cols - 20));
	    we_cancel = busy_cue(sort_msg, NULL, 0);
	}

	/*
	 * We do this so that we don't have to lookup the scores with function
	 * calls for each qsort compare.
	 */
	build_score_array(stream, msgmap);

	qsort(msgmap->sort+1, (size_t) mn_get_total(msgmap),
	      sizeof(long), pine_compare_scores);
	free_score_array();
	clear_index_cache(stream, 0);

	if(we_cancel)
	  cancel_busy_cue(1);

	/*
	 * Flip the sort if necessary (cheaper to do it once than for
	 * every comparison in pine_compare_scores.
	 */
	if(new_rev && mn_get_total(msgmap) > 1L){
	    long *ep = &msgmap->sort[mn_get_total(msgmap)],
		 *sp = &msgmap->sort[1], tmp;

	    do{
		tmp   = *sp;
		*sp++ = *ep;
		*ep-- = tmp;
	    }
	    while(ep > sp);
	}

	/* reset the inverse array */
	msgno_reset_isort(msgmap);
    }
    else{

	mn_set_sort(msgmap, new_sort);
	mn_set_revsort(msgmap, new_rev);
	clear_index_cache(stream, 0);

	if(flags & SRT_VRB){
	    int (*sort_func)() = NULL;

	    /*
	     * IMAP sort doesn't give us any way to get progress,
	     * so just spin the bar rather than show zero percent
	     * forever while a slow sort's going on...
	     */
	    if(!(stream && stream->dtb && stream->dtb->name
		 && !strucmp(stream->dtb->name, "imap")
		 && LEVELSORT(stream)))
	      sort_func = percent_sorted;

	    snprintf(sort_msg, sizeof(sort_msg), _("Sorting \"%s\""),
		    strsquish(tmp_20k_buf + 500, 500, ps_global->cur_folder,
			      ps_global->ttyo->screen_cols - 20));
	    we_cancel = busy_cue(sort_msg, sort_func, 0);
	}

	/*
	 * Limit the sort/thread if messages are hidden from view 
	 * by lighting searched bit of every interesting msg in 
	 * the folder and call c-client thread/sort to do the dirty work.
	 *
	 * Unfortunately it isn't that easy. IMAP servers are not able to
	 * handle medium to large sized sequence sets (more than 1000
	 * characters in the command line breaks some) so we have to try
	 * to handle it locally. By lighting the searched bits and
	 * providing a NULL search program we get a special c-client
	 * interface. This means that c-client will attempt to send the
	 * sequence set with the SORT or THREAD but it may get back
	 * a BAD response because of long command lines. In that case,
	 * if it is a SORT call, c-client will issue the full SORT
	 * without the sequence sets and will then filter the results
	 * locally. So sort_sort_callback will see the correctly
	 * filtered results. If it is a mail_thread call, a similar thing
	 * will be done. If a BAD is received, then there is no way to
	 * easily filter the results. C-client (in this special case where
	 * we provide a NULL search program) will set tree->num to zero
	 * for nodes of the thread tree which were supposed to be
	 * filtered out of the thread. Then pine, in sort_thread_callback,
	 * will treat those as dummy nodes (nodes which are part of the
	 * tree logically but where we don't have access to the messages).
	 * This will give us a different answer than we would have gotten
	 * if the restricted thread would have worked, but it's something.
	 *
	 * It isn't in general possible to give some shorter search set
	 * in place of the long sequence set because the long sequence set
	 * may be the result of several filter rules or of excluded
	 * messages in news (in this 2nd case maybe we could give
	 * a shorter search set).
	 *
	 * We note also that the too-long commands in imap is a general
	 * imap deficiency. It comes up in particular also in SEARCH
	 * commands. Pine likes to exclude the hidden messages from the
	 * SEARCH. SEARCH will be handled transparently by the local
	 * c-client by first issuing the full SEARCH command, if that
	 * comes back with a BAD and there is a pgm->msgno at the top
	 * level of pgm, then c-client will re-issue the SEARCH command
	 * but without the msgno sequence set in hopes that the resulting
	 * command will now be short enough, and then it will filter out
	 * the sequence set locally. If that doesn't work, it will
	 * download the messages and do the SEARCH locally. That is
	 * controllable by a flag bit.
	 */
	for(i = 1L; i <= stream->nmsgs; i++)
	  if((mc = mail_elt(stream, i)) != NULL)
	    mc->searched = !get_lflag(stream, NULL, i, MN_EXLD);
	
	g_sort.msgmap = msgmap;
	if(new_sort == SortThread || new_sort == SortSubject2){
	    THREADNODE *thread;

	    /*
	     * Install callback to collect thread results
	     * and update sort mapping.  Problem this solves
	     * is that of receiving exists/expunged events
	     * within sort/thread response.  Since we update
	     * the sorted table within those handlers, we
	     * can get out of sync when we replace possibly
	     * stale sort/thread results once the function
	     * call's returned.  Make sense?  Thought so.
	     */
	    mail_parameters(NULL, SET_THREADRESULTS,
			    (void *) sort_thread_callback);

	    thread = mail_thread(stream,
				 (new_sort == SortThread)
				   ? "REFERENCES" : "ORDEREDSUBJECT",
				 NULL, NULL, 0L);

	    mail_parameters(NULL, SET_THREADRESULTS, (void *) NULL);

	    if(!thread){
		new_sort = current_sort;
		new_rev  = current_rev;
		q_status_message1(SM_ORDER, 3, 3,
				  "Sort Failed!  Restored %.200s sort.",
				  sort_name(new_sort));
	    }

	    if(thread)
	      mail_free_threadnode(&thread);
	}
	else{
	    /*
	     * Set up the sort program.
	     * NOTE: we deal with reverse bit below.
	     */
	    g_sort.prog = mail_newsortpgm();
	    g_sort.prog->function = (new_sort == SortSubject)
				     ? SORTSUBJECT
				     : (new_sort == SortFrom)
					? SORTFROM
					: (new_sort == SortTo)
					   ? SORTTO
					   : (new_sort == SortCc)
					      ? SORTCC
					      : (new_sort == SortDate)
						 ? SORTDATE
						 : (new_sort == SortSize)
						    ? SORTSIZE
						    : SORTARRIVAL;

	    mail_parameters(NULL, SET_SORTRESULTS,
			    (void *) sort_sort_callback);

	    /* Where the rubber meets the road. */
	    sort = mail_sort(stream, NULL, NULL, g_sort.prog, 0L);

	    mail_parameters(NULL, SET_SORTRESULTS, (void *) NULL);

	    if(!sort){
		new_sort = current_sort;
		new_rev  = current_rev;
		q_status_message1(SM_ORDER, 3, 3,
				  "Sort Failed!  Restored %s sort.",
				  sort_name(new_sort));
	    }

	    if(sort)
	      fs_give((void **) &sort);

	    mail_free_sortpgm(&g_sort.prog);
	}

	if(we_cancel)
	  cancel_busy_cue(1);

	/*
	 * Flip the sort if necessary (cheaper to do it once than for
	 * every comparison underneath mail_sort()
	 */
	if(new_rev && mn_get_total(msgmap) > 1L){
	    long *ep = &msgmap->sort[mn_get_total(msgmap)],
		 *sp = &msgmap->sort[1], tmp;

	    do{
		tmp   = *sp;
		*sp++ = *ep;
		*ep-- = tmp;
	    }
	    while(ep > sp);

	    /* reset the inverse array */
	    msgno_reset_isort(msgmap);

	    /*
	     * Flip the thread numbers around.
	     * This puts us in a weird state that requires keeping track
	     * of. The direction of the thread list hasn't changed, but the
	     * thrdnos have and the display direction has.
	     *
	     * For Sort   thrdno 1       thread list head
	     *            thrdno 2              |
	     *            thrdno .              v  nextthd this direction
	     *            thrdno .
	     *            thrdno n       thread list tail
	     *
	     * Rev Sort   thrdno 1       thread list tail
	     *            thrdno 2
	     *            thrdno .              ^  nextthd this direction
	     *            thrdno .              |
	     *            thrdno n       thread list head
	     */
	    if(new_sort == SortThread || new_sort == SortSubject2){
		PINETHRD_S *thrd;

		thrd = fetch_head_thread(stream);
		for(j = msgmap->max_thrdno; thrd && j >= 1L; j--){
		    thrd->thrdno = j;

		    if(thrd->nextthd)
		      thrd = fetch_thread(stream,
					  thrd->nextthd);
		    else
		      thrd = NULL;
		}
	    }
	}
    }

    /* Fix up sort structure */
    mn_set_sort(msgmap, new_sort);
    mn_set_revsort(msgmap, new_rev);
    /*
     * Once a folder has been sorted manually, we continue treating it
     * as manually sorted until it is closed.
     */
    if(!mn_get_mansort(msgmap))
      mn_set_mansort(msgmap, (flags & SRT_MAN) ? 1 : 0);
    
    if(!msgmap->hilited){
	/*
	 * If current is hidden, change current to visible parent.
	 * It can only be hidden if we are threading.
	 *
	 * Don't do this if hilited is set, because it means we're in the
	 * middle of an aggregate op, and this will mess up our selection.
	 * "hilited" means we've done a pseudo_selected, which we'll later
	 * fix with restore_selected.
	 */
	if(THREADING())
	  mn_reset_cur(msgmap, first_sorted_flagged(new_rev ? F_NONE : F_SRCHBACK,
						    stream,
						    mn_raw2m(msgmap, raw_current),
						    FSF_SKIP_CHID));
	else
	  mn_reset_cur(msgmap, mn_raw2m(msgmap, raw_current));
    }

    msgmap->top = -1L;

    if(!sp_mail_box_changed(stream))
      sp_set_unsorted_newmail(stream, 0);

    /*
     * Turn off the MN_USOR flag. Don't bother going through the
     * function call and the message number mappings.
     */
    if(THREADING()){
	PINELT_S *pelt;

	for(i = 1L; i <= stream->nmsgs; i++)
	  if((mc = mail_elt(stream, i)) != NULL && (pelt = mc->sparep) != NULL)
	    pelt->unsorted = 0;
    }
}


void
sort_sort_callback(MAILSTREAM *stream, long unsigned int *list, long unsigned int nmsgs)
{
    long i;

    dprint((2, "sort_sort_callback\n"));

    if(mn_get_total(g_sort.msgmap) < nmsgs)
      panic("Message count shrank after sort!");

    /* copy ulongs to array of longs */
    for(i = nmsgs; i > 0; i--)
      g_sort.msgmap->sort[i] = list ? (long) list[i-1] : i-1;

    /* reset the inverse array */
    msgno_reset_isort(g_sort.msgmap);

    dprint((2, "sort_sort_callback done\n"));
}


/*
 * Return value for use by progress bar.
 */
int
percent_sorted(void)
{
    /*
     * C-client's sort routine exports two types of status
     * indicators.  One's the progress thru loading the cache (typically
     * the elephantine bulk of the delay, and the progress thru the
     * actual sort (typically qsort).  Our job is to balance the two
     */
    
    return(g_sort.prog && g_sort.prog->nmsgs
      ? (((((g_sort.prog->progress.cached * 100)
					 / g_sort.prog->nmsgs) * 9) / 10)
	 + (((g_sort.prog->progress.sorted) * 10)
					 / g_sort.prog->nmsgs))
      : 0);
}


/*
 * This is only used at startup time.  It sets ps->def_sort and
 * ps->def_sort_rev.  The syntax of the sort_spec is type[/reverse].
 * A reverse without a type is the same as arrival/reverse.  A blank
 * argument also means arrival/reverse.
 */
int
decode_sort(char *sort_spec, SortOrder *def_sort, int *def_sort_rev)
{
    char *sep;
    char *fix_this = NULL;
    int   x, reverse;

    if(!sort_spec || !*sort_spec){
	*def_sort = SortArrival;
	*def_sort_rev = 0;
        return(0);
    }

    if(struncmp(sort_spec, "reverse", strlen(sort_spec)) == 0){
	*def_sort = SortArrival;
	*def_sort_rev = 1;
        return(0);
    }
     
    reverse = 0;
    if((sep = strindex(sort_spec, '/')) != NULL){
        *sep = '\0';
	fix_this = sep;
        sep++;
        if(struncmp(sep, "reverse", strlen(sep)) == 0)
          reverse = 1;
        else{
	    *fix_this = '/';
	    return(-1);
	}
    }

    for(x = 0; ps_global->sort_types[x] != EndofList; x++)
      if(struncmp(sort_name(ps_global->sort_types[x]),
                  sort_spec, strlen(sort_spec)) == 0)
        break;

    if(fix_this)
      *fix_this = '/';

    if(ps_global->sort_types[x] == EndofList)
      return(-1);

    *def_sort     = ps_global->sort_types[x];
    *def_sort_rev = reverse;
    return(0);
}


/*----------------------------------------------------------------------
  Compare raw message numbers 
 ----*/
int
pine_compare_long(const qsort_t *a, const qsort_t *b)
{
    long *mess_a = (long *)a, *mess_b = (long *)b, mdiff;

    return((mdiff = *mess_a - *mess_b) ? ((mdiff > 0L) ? 1 : -1) : 0);
}

/*
 * reverse version of pine_compare_long
 */
int
pine_compare_long_rev(const qsort_t *a, const qsort_t *b)
{
    long *mess_a = (long *)a, *mess_b = (long *)b, mdiff;

    return((mdiff = *mess_a - *mess_b) ? ((mdiff < 0L) ? 1 : -1) : 0);
}


long *g_score_arr;

/*
 * This calculate all of the scores and also puts them into a temporary array
 * for speed when sorting.
 */
void
build_score_array(MAILSTREAM *stream, MSGNO_S *msgmap)
{
    SEARCHSET *searchset;
    long       msgno, cnt, nmsgs, rawmsgno;
    long       score;
    MESSAGECACHE *mc;

    nmsgs = mn_get_total(msgmap);
    g_score_arr = (long *) fs_get((nmsgs+1) * sizeof(score));
    memset(g_score_arr, 0, (nmsgs+1) * sizeof(score));

    /*
     * Build a searchset that contains everything except those that have
     * already been looked up.
     */

    for(msgno=1L; msgno <= stream->nmsgs; msgno++)
      if((mc = mail_elt(stream, msgno)) != NULL)
	mc->sequence = 0;

    for(cnt=0L, msgno=1L; msgno <= nmsgs; msgno++){
	rawmsgno = mn_m2raw(msgmap, msgno);
	if(get_msg_score(stream, rawmsgno) == SCORE_UNDEF
	   && rawmsgno > 0L && stream && rawmsgno <= stream->nmsgs
	   && (mc = mail_elt(stream, rawmsgno))){
	    mc->sequence = 1;
	    cnt++;
	}
    }

    if(cnt){
	searchset = build_searchset(stream);
	(void)calculate_some_scores(stream, searchset, 0);
	mail_free_searchset(&searchset);
    }

    /*
     * Copy scores to g_score_arr. They should all be defined now but if
     * they aren't assign score zero.
     */
    for(rawmsgno = 1L; rawmsgno <= nmsgs; rawmsgno++){
	score = get_msg_score(stream, rawmsgno);
	g_score_arr[rawmsgno] = (score == SCORE_UNDEF) ? 0L : score;
    }
}


void
free_score_array(void)
{
    if(g_score_arr)
      fs_give((void **) &g_score_arr);
}


/*----------------------------------------------------------------------
  Compare scores
 ----*/
int
pine_compare_scores(const qsort_t *a, const qsort_t *b)
{
    long *mess_a = (long *)a, *mess_b = (long *)b, mdiff;
    long  sdiff;

    return((sdiff = g_score_arr[*mess_a] - g_score_arr[*mess_b])
	    ? ((sdiff > 0L) ? 1 : -1)
	    : ((mdiff = *mess_a - *mess_b) ? ((mdiff > 0) ? 1 : -1) : 0));
}


void
reset_sort_order(unsigned int flags)
{
    long rflags = ROLE_DO_OTHER;
    PAT_STATE     pstate;
    PAT_S        *pat;
    SortOrder	  the_sort_order;
    int           sort_is_rev;

    /* set default order */
    the_sort_order = ps_global->def_sort;
    sort_is_rev    = ps_global->def_sort_rev;

    if(ps_global->mail_stream && nonempty_patterns(rflags, &pstate)){
	for(pat = first_pattern(&pstate); pat; pat = next_pattern(&pstate)){
	    if(match_pattern(pat->patgrp, ps_global->mail_stream, NULL,
			     NULL, NULL, SE_NOSERVER|SE_NOPREFETCH))
	      break;
	}

	if(pat && pat->action && !pat->action->bogus
	   && pat->action->sort_is_set){
	    the_sort_order = pat->action->sortorder;
	    sort_is_rev    = pat->action->revsort;
	}
    }

    sort_folder(ps_global->mail_stream, ps_global->msgmap,
		the_sort_order, sort_is_rev, flags);
}
