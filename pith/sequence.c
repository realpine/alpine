#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: sequence.c 1012 2008-03-26 00:44:22Z hubert@u.washington.edu $";
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
#include "../pith/sequence.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/msgno.h"
#include "../pith/flag.h"
#include "../pith/thread.h"
#include "../pith/icache.h"


/*
 * Internal prototypes
 */


/*----------------------------------------------------------------------
  Build comma delimited list of selected messages

  Args: stream -- mail stream to use for flag testing
	msgmap -- message number struct of to build selected messages in
	count -- pointer to place to write number of comma delimited
	mark -- mark manually undeleted so we don't refilter right away

  Returns: malloc'd string containing sequence, else NULL if
	   no messages in msgmap with local "selected" flag.
  ----*/
char *
selected_sequence(MAILSTREAM *stream, MSGNO_S *msgmap, long int *count, int mark)
{
    long  i;
    MESSAGECACHE *mc;

    if(!stream)
      return(NULL);

    /*
     * The plan here is to use the c-client elt's "sequence" bit
     * to work around any orderings or exclusions in pine's internal
     * mapping that might cause the sequence to be artificially
     * lengthy.  It's probably cheaper to run down the elt list
     * twice rather than call nm_raw2m() for each message as
     * we run down the elt list once...
     */
    for(i = 1L; i <= stream->nmsgs; i++)
      if((mc = mail_elt(stream, i)) != NULL)
        mc->sequence = 0;

    for(i = 1L; i <= mn_get_total(msgmap); i++)
      if(get_lflag(stream, msgmap, i, MN_SLCT)){
	  long rawno;
	  int  exbits = 0;

	  /*
	   * Forget we knew about it, and set "add to sequence"
	   * bit...
	   */
	  clear_index_cache_ent(stream, i, 0);
	  rawno = mn_m2raw(msgmap, i);
	  if(rawno > 0L && rawno <= stream->nmsgs
	     && (mc = mail_elt(stream, rawno)))
	    mc->sequence = 1;

	  /*
	   * Mark this message manually flagged so we don't re-filter it
	   * with a filter which only sets flags.
	   */
	  if(mark){
	      if(msgno_exceptions(stream, rawno, "0", &exbits, FALSE))
		exbits |= MSG_EX_MANUNDEL;
	      else
		exbits = MSG_EX_MANUNDEL;

	      msgno_exceptions(stream, rawno, "0", &exbits, TRUE);
	  }
      }

    return(build_sequence(stream, NULL, count));
}


/*----------------------------------------------------------------------
  Build comma delimited list of messages current in msgmap which have all
  flags matching the arguments

  Args: stream -- mail stream to use for flag testing
	msgmap -- only consider messages selected in this msgmap
	  flag -- flags to match against
	 count -- pointer to place to return number of comma delimited
	  mark -- mark index cache entry changed, and count state change
	 kw_on -- if flag contains F_KEYWORD, this is
	            the array of keywords to be checked
	kw_off -- if flag contains F_UNKEYWORD, this is
	            the array of keywords to be checked

  Returns: malloc'd string containing sequence, else NULL if
	   no messages in msgmap with local "selected" flag (a flag
	   of zero means all current msgs).
  ----*/
char *
currentf_sequence(MAILSTREAM *stream, MSGNO_S *msgmap, long int flag,
		  long int *count, int mark, char **kw_on, char **kw_off)
{
    char	 *seq, **t;
    long	  i, rawno;
    int           exbits;
    MESSAGECACHE *mc;

    if(!stream)
      return(NULL);

    /* First, make sure elts are valid for all the interesting messages */
    if((seq = invalid_elt_sequence(stream, msgmap)) != NULL){
	pine_mail_fetch_flags(stream, seq, NIL);
	fs_give((void **) &seq);
    }

    for(i = 1L; i <= stream->nmsgs; i++)
      if((mc = mail_elt(stream, i)) != NULL)
        mc->sequence = 0;			/* clear "sequence" bits */

    for(i = mn_first_cur(msgmap); i > 0L; i = mn_next_cur(msgmap)){
	/* if not already set, go on... */
	rawno = mn_m2raw(msgmap, i);
	mc = (rawno > 0L && rawno <= stream->nmsgs)
		? mail_elt(stream, rawno) : NULL;
	if(!mc)
	  continue;

	if((flag == 0)
	   || ((flag & F_DEL) && mc->deleted)
	   || ((flag & F_UNDEL) && !mc->deleted)
	   || ((flag & F_SEEN) && mc->seen)
	   || ((flag & F_UNSEEN) && !mc->seen)
	   || ((flag & F_ANS) && mc->answered)
	   || ((flag & F_UNANS) && !mc->answered)
	   || ((flag & F_FLAG) && mc->flagged)
	   || ((flag & F_UNFLAG) && !mc->flagged)){
	    mc->sequence = 1;			/* set "sequence" flag */
	}

	/* check for forwarded status */
	if(!mc->sequence
	   && (flag & F_FWD)
	   && user_flag_is_set(stream, rawno, FORWARDED_FLAG)){
	    mc->sequence = 1;
	}

	if(!mc->sequence
	   && (flag & F_UNFWD)
	   && !user_flag_is_set(stream, rawno, FORWARDED_FLAG)){
	    mc->sequence = 1;
	}

	/* check for user keywords or not */
	if(!mc->sequence && flag & F_KEYWORD && kw_on){
	    for(t = kw_on; !mc->sequence && *t; t++)
	      if(user_flag_is_set(stream, rawno, *t))
		mc->sequence = 1;
	}
	else if(!mc->sequence && flag & F_UNKEYWORD && kw_off){
	    for(t = kw_off; !mc->sequence && *t; t++)
	      if(!user_flag_is_set(stream, rawno, *t))
		mc->sequence = 1;
	}
	
	if(mc->sequence){
	    if(mark){
		if(THRD_INDX()){
		    PINETHRD_S *thrd;
		    long        t;

		    /* clear thread index line instead of index index line */
		    thrd = fetch_thread(stream, mn_m2raw(msgmap, i));
		    if(thrd && thrd->top
		       && (thrd=fetch_thread(stream,thrd->top))
		       && (t = mn_raw2m(msgmap, thrd->rawno)))
		      clear_index_cache_ent(stream, t, 0);
		}
		else
		  clear_index_cache_ent(stream, i, 0);/* force new index line */

		/*
		 * Mark this message manually flagged so we don't re-filter it
		 * with a filter which only sets flags.
		 */
		exbits = 0;
		if(msgno_exceptions(stream, rawno, "0", &exbits, FALSE))
		  exbits |= MSG_EX_MANUNDEL;
		else
		  exbits = MSG_EX_MANUNDEL;

		msgno_exceptions(stream, rawno, "0", &exbits, TRUE);
	    }
	}
    }

    return(build_sequence(stream, NULL, count));
}


/*----------------------------------------------------------------------
  Return sequence numbers of messages with invalid MESSAGECACHEs

  Args: stream -- mail stream to use for flag testing
	msgmap -- message number struct of to build selected messages in

  Returns: malloc'd string containing sequence, else NULL if
	   no messages in msgmap with local "selected" flag (a flag
	   of zero means all current msgs).
  ----*/
char *
invalid_elt_sequence(MAILSTREAM *stream, MSGNO_S *msgmap)
{
    long	  i, rawno;
    MESSAGECACHE *mc;

    if(!stream)
      return(NULL);

    for(i = 1L; i <= stream->nmsgs; i++)
      if((mc = mail_elt(stream, i)) != NULL)
        mc->sequence = 0;			/* clear "sequence" bits */

    for(i = mn_first_cur(msgmap); i > 0L; i = mn_next_cur(msgmap))
      if((rawno = mn_m2raw(msgmap, i)) > 0L && rawno <= stream->nmsgs
	 && (mc = mail_elt(stream, rawno)) && !mc->valid)
	mc->sequence = 1;

    return(build_sequence(stream, NULL, NULL));
}


/*----------------------------------------------------------------------
  Build comma delimited list of messages with elt "sequence" bit set

  Args: stream -- mail stream to use for flag testing
	msgmap -- struct containing sort to build sequence in
	count -- pointer to place to write number of comma delimited
		 NOTE: if non-zero, it's a clue as to how many messages
		       have the sequence bit lit.

  Returns: malloc'd string containing sequence, else NULL if
	   no messages in msgmap with elt's "sequence" bit set
  ----*/
char *
build_sequence(MAILSTREAM *stream, MSGNO_S *msgmap, long int *count)
{
#define	SEQ_INCREMENT	128
    long    n = 0L, i, x, lastn = 0L, runstart = 0L;
    size_t  size = SEQ_INCREMENT;
    char   *seq = NULL, *p;
    MESSAGECACHE *mc;

    if(!stream)
      return(NULL);

    if(count){
	if(*count > 0L)
	  size = MAX(size, MIN((*count) * 4, 16384));

	*count = 0L;
    }

    for(x = 1L; x <= stream->nmsgs; x++){
	if(msgmap){
	    if((i = mn_m2raw(msgmap, x)) == 0L)
	      continue;
	}
	else
	  i = x;

	if(i > 0L && i <= stream->nmsgs
	   && (mc = mail_elt(stream, i)) && mc->sequence){
	    n++;
	    if(!seq)				/* initialize if needed */
	      seq = p = fs_get(size);

	    /*
	     * This code will coalesce the ascending runs of
	     * sequence numbers, but fails to break sequences
	     * into a reasonably sensible length for imapd's to
	     * swallow (reasonable addtition to c-client?)...
	     */
	    if(lastn){				/* if may be in a run */
		if(lastn + 1L == i){		/* and its the next raw num */
		    lastn = i;			/* skip writing anything... */
		    continue;
		}
		else if(runstart != lastn){
		    *p++ = (runstart + 1L == lastn) ? ',' : ':';
		    sstrncpy(&p, long2string(lastn), size-(p-seq));
		}				/* wrote end of run */
	    }

	    runstart = lastn = i;		/* remember last raw num */

	    if(n > 1L && (p-seq) < size)			/* !first num, write delim */
	      *p++ = ',';

	    if(size - (p - seq) < 16){	/* room for two more nums? */
		size_t offset = p - seq;	/* grow the sequence array */
		size += SEQ_INCREMENT;
		fs_resize((void **)&seq, size);
		p = seq + offset;
	    }

	    sstrncpy(&p, long2string(i), size-(p-seq));	/* write raw number */
	}
    }

    if(lastn && runstart != lastn){		/* were in a run? */
	if((p-seq) < size)			/* !first num, write delim */
	  *p++ = (runstart + 1L == lastn) ? ',' : ':';

	sstrncpy(&p, long2string(lastn), size-(p-seq));	/* write the trailing num */
    }

    if(seq && (p-seq) < size)				/* if sequence, tie it off */
      *p  = '\0';

    if(seq)
      seq[size-1] = '\0';

    if(count)
      *count = n;

    return(seq);
}


/*----------------------------------------------------------------------
  If any messages flagged "selected", fake the "currently selected" array

  Args: map -- message number struct of to build selected messages in

  OK folks, here's the tradeoff: either all the functions have to
  know if the user want's to deal with the "current" hilited message
  or the list of currently "selected" messages, *or* we just
  wrap the call to these functions with some glue that tweeks
  what these functions see as the "current" message list, and let them
  do their thing.
  ----*/
int
pseudo_selected(MAILSTREAM *stream, MSGNO_S *map)
{
    long i, later = 0L;

    if(any_lflagged(map, MN_SLCT)){
	map->hilited = mn_m2raw(map, mn_get_cur(map));

	for(i = 1L; i <= mn_get_total(map); i++)
	  if(get_lflag(stream, map, i, MN_SLCT)){
	      if(!later++){
		  mn_set_cur(map, i);
	      }
	      else{
		  mn_add_cur(map, i);
	      }
	  }

	return(1);
    }

    return(0);
}


/*----------------------------------------------------------------------
  Antidote for the monkey business committed above

  Args: map -- message number struct of to build selected messages in

  ----*/
void
restore_selected(MSGNO_S *map)
{
    if(map->hilited){
	mn_reset_cur(map, mn_raw2m(map, map->hilited));
	map->hilited = 0L;
    }
}


/*
 * Return a search set which can be used to limit the search to a smaller set,
 * for performance reasons. The caller must still work correctly if we return
 * the whole set (or NULL) here, because we may not be able to send the full
 * search set over IMAP. In cases where the search set description is getting
 * too large we send a search set which contains all of the relevant messages.
 * It may contain more.
 *
 * Args    stream
 *         narrow  -- If set, we are narrowing our selection (restrict to
 *                      messages with MN_SLCT already set) or if not set,
 *                      we are broadening (so we may look only at messages
 *                      with MN_SLCT not set)
 *
 * Returns - allocated search set or NULL. Caller is responsible for freeing it.
 */
SEARCHSET *
limiting_searchset(MAILSTREAM *stream, int narrow)
{
    long       n, run;
    int        cnt = 0;
    SEARCHSET *full_set = NULL, **set;

    /*
     * If we're talking to anything other than a server older than
     * imap 4rev1, build a searchset otherwise it'll choke.
     */
    if(!(is_imap_stream(stream) && !modern_imap_stream(stream))){
	for(n = 1L, set = &full_set, run = 0L; n <= stream->nmsgs; n++)
	  /* test for end of run */
	  if(get_lflag(stream, NULL, n, MN_EXLD)
	     || (narrow && !get_lflag(stream, NULL, n, MN_SLCT))
	     || (!narrow && get_lflag(stream, NULL, n, MN_SLCT))){
	      if(run){		/* previous selected? */
		  set = &(*set)->next;
		  run = 0L;
	      }
	  }
	  else if(run++){		/* next in run */
	      (*set)->last = n;
	  }
	  else{				/* start of run */
	      *set = mail_newsearchset();
	      (*set)->first = (*set)->last = n;

	      /*
	       * Make this last set cover the rest of the messages.
	       * We could be fancier about this but it probably isn't
	       * worth the trouble.
	       */
	      if(++cnt > 100){
		  (*set)->last = stream->nmsgs;
		  break;
	      }
	  }
    }

    return(full_set);
}
