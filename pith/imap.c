#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: imap.c 1142 2008-08-13 17:22:21Z hubert@u.washington.edu $";
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
#include "../pith/imap.h"
#include "../pith/msgno.h"
#include "../pith/state.h"
#include "../pith/flag.h"
#include "../pith/pineelt.h"
#include "../pith/status.h"
#include "../pith/conftype.h"
#include "../pith/context.h"
#include "../pith/thread.h"
#include "../pith/mailview.h"
#include "../pith/mailpart.h"
#include "../pith/mailindx.h"
#include "../pith/mailcmd.h"
#include "../pith/save.h"
#include "../pith/util.h"
#include "../pith/stream.h"
#include "../pith/newmail.h"
#include "../pith/icache.h"
#include "../pith/options.h"

#ifdef _WINDOWS
#include "../pico/osdep/mswin.h"
#endif


/*
 * Internal prototypes
 */
long  imap_seq_exec(MAILSTREAM *, char *, long (*)(MAILSTREAM *, long, void *), void *);
long  imap_seq_exec_append(MAILSTREAM *, long, void *);
char *ps_get(size_t);


/*
 * Exported globals setup by searching functions to tell mm_searched
 * where to put message numbers that matched the search criteria,
 * and to allow mm_searched to return number of matches.
 */
MAILSTREAM *mm_search_stream;
long	    mm_search_count  = 0L;
MAILSTATUS  mm_status_result;

MM_LIST_S  *mm_list_info;

MMLOGIN_S  *mm_login_list     = NULL;
MMLOGIN_S  *cert_failure_list = NULL;

/*
 * Instead of storing cached passwords in free storage, store them in this
 * private space. This makes it easier and more reliable when we want
 * to zero this space out. We only store passwords here (char *) so we
 * don't need to worry about alignment.
 */
static	volatile char private_store[1024];

static	int        critical_depth = 0;

/* hook to hang callback on "current" message expunge */
void (*pith_opt_current_expunged)(long unsigned int);

#ifdef	SIGINT
RETSIGTYPE (*hold_int)(int);
#endif

#ifdef	SIGTERM
RETSIGTYPE (*hold_term)(int);
#endif

#ifdef	SIGHUP
RETSIGTYPE (*hold_hup)(int);
#endif

#ifdef	SIGUSR2
RETSIGTYPE (*hold_usr2)(int);
#endif



/*---------------------------------------------------------------------- 
        receive notification that search found something           

 Input:  mail stream and message number of located item

 Result: nothing, not used by pine
  ----*/
void
mm_searched(MAILSTREAM *stream, long unsigned int rawno)
{
    MESSAGECACHE *mc;

    if(rawno > 0L && stream && rawno <= stream->nmsgs
       && (mc = mail_elt(stream, rawno))){
	mc->searched = 1;
	if(stream == mm_search_stream)
	  mm_search_count++;
    }
}


/*----------------------------------------------------------------------
       receive notification of new mail from imap daemon

   Args: stream -- The stream the message count report is for.
         number -- The number of messages now in folder.
 
  Result: Sets value in pine state indicating new mailbox size

     Called when the number of messages in the mailbox goes up.  This
 may also be called as a result of an expunge. It increments the
 new_mail_count based on a the difference between the current idea of
 the maximum number of messages and what mm_exists claims. The new mail
 notification is done in newmail.c

 Only worry about the cases when the number grows, as mm_expunged
 handles shrinkage...

 ----*/
void
mm_exists(MAILSTREAM *stream, long unsigned int number)
{
    long     new_this_call, n;
    int	     exbits = 0, lflags = 0;
    MSGNO_S *msgmap;

#ifdef DEBUG
    if(ps_global->debug_imap > 1 || ps_global->debugmem)
      dprint((3, "=== mm_exists(%lu,%s) called ===\n", number,
     !stream ? "(no stream)" : !stream->mailbox ? "(null)" : stream->mailbox));
#endif

    msgmap = sp_msgmap(stream);
    if(!msgmap)
      return;

    if(mn_get_nmsgs(msgmap) != (long) number){
	sp_set_mail_box_changed(stream, 1);
	/* titlebar will be affected */
	if(ps_global->mail_stream == stream)
	  ps_global->mangled_header = 1;
    }

    if(mn_get_nmsgs(msgmap) < (long) number){
	new_this_call = (long) number - mn_get_nmsgs(msgmap);
	sp_set_new_mail_count(stream,
			      sp_new_mail_count(stream) + new_this_call);
	sp_set_recent_since_visited(stream,
			      sp_recent_since_visited(stream) + new_this_call);

	mn_add_raw(msgmap, new_this_call);

	/*
	 * Set local "recent" and "hidden" bits...
	 */
	for(n = 0; n < new_this_call; n++, number--){
	    if(msgno_exceptions(stream, number, "0", &exbits, FALSE))
	      exbits |= MSG_EX_RECENT;
	    else
	      exbits = MSG_EX_RECENT;

	    msgno_exceptions(stream, number, "0", &exbits, TRUE);

	    if(SORT_IS_THREADED(msgmap))
	      lflags |= MN_USOR;

	    /*
	     * If we're zoomed, then hide this message too since
	     * it couldn't have possibly been selected yet...
	     */
	    lflags |= (any_lflagged(msgmap, MN_HIDE) ? MN_HIDE : 0);
	    if(lflags)
	      set_lflag(stream, msgmap, mn_get_total(msgmap) - n, lflags, 1);
	}
    }
}


/*----------------------------------------------------------------------
    Receive notification from IMAP that a single message has been expunged

   Args: stream -- The stream/folder the message is expunged from
         rawno  -- The raw message number that was expunged

mm_expunged is always called on an expunge.  Simply remove all 
reference to the expunged message, shifting internal mappings as
necessary.
  ----*/
void
mm_expunged(MAILSTREAM *stream, long unsigned int rawno)
{
    MESSAGECACHE *mc;
    long          i;
    int           is_current = 0;
    MSGNO_S      *msgmap;

#ifdef DEBUG
    if(ps_global->debug_imap > 1 || ps_global->debugmem)
      dprint((3, "mm_expunged(%s,%lu)\n",
	       stream
		? (stream->mailbox
		    ? stream->mailbox
		    : "(no stream)")
		: "(null)", rawno));
#endif

    msgmap = sp_msgmap(stream);
    if(!msgmap)
      return;

    if(ps_global->mail_stream == stream)
      is_current++;

    if((i = mn_raw2m(msgmap, (long) rawno)) != 0L){
	dprint((7, "mm_expunged: rawno=%lu msgno=%ld nmsgs=%ld max_msgno=%ld flagged_exld=%ld\n", rawno, i, mn_get_nmsgs(msgmap), mn_get_total(msgmap), msgmap->flagged_exld));

	sp_set_mail_box_changed(stream, 1);
	sp_set_expunge_count(stream, sp_expunge_count(stream) + 1);

	if(is_current){
	    reset_check_point(stream);
	    ps_global->mangled_header = 1;

	    /* flush invalid cache entries */
	    while(i <= mn_get_total(msgmap))
	      clear_index_cache_ent(stream, i++, 0);

	    /* let app know what happened */
	    if(pith_opt_current_expunged)
	      (*pith_opt_current_expunged)(rawno);
	}
    }
    else{
	dprint((7,
	       "mm_expunged: rawno=%lu was excluded, flagged_exld was %d\n",
	       rawno, msgmap->flagged_exld));
	dprint((7, "             nmsgs=%ld max_msgno=%ld\n",
	       mn_get_nmsgs(msgmap), mn_get_total(msgmap)));
	if(rawno > 0L && rawno <= stream->nmsgs)
	  mc = mail_elt(stream, rawno);

	if(!mc){
	    dprint((7, "             cannot get mail_elt(%lu)\n",
		   rawno));
	}
	else if(!mc->sparep){
	    dprint((7, "             mail_elt(%lu)->sparep is NULL\n",
		   rawno));
	}
	else{
	    dprint((7, "             mail_elt(%lu)->sparep->excluded=%d\n",
		    rawno, (int) (((PINELT_S *) mc->sparep)->excluded)));
	}
    }

    if(SORT_IS_THREADED(msgmap)
       && (SEP_THRDINDX()
	   || ps_global->thread_disp_style != THREAD_NONE)){
	long cur;

	/*
	 * When we're sorting with a threaded method an expunged
	 * message may cause the rest of the sort to be wrong. This
	 * isn't so bad if we're just looking at the index. However,
	 * it also causes the thread tree (PINETHRD_S) to become
	 * invalid, so if we're using a threading view we need to
	 * sort in order to fix the tree and to protect fetch_thread().
	 */
	sp_set_need_to_rethread(stream, 1);

	/*
	 * If we expunged the current message which was a member of the
	 * viewed thread, and the adjustment to current will take us
	 * out of that thread, fix it if we can, by backing current up
	 * into the thread. We'd like to just check after mn_flush_raw
	 * below but the problem is that the elts won't change until
	 * after we return from mm_expunged. So we have to manually
	 * check the other messages for CHID2 flags instead of thinking
	 * that we can expunge the current message and then check. It won't
	 * work because the elt will still refer to the expunged message.
	 */
	if(sp_viewing_a_thread(stream)
	   && get_lflag(stream, NULL, rawno, MN_CHID2)
	   && mn_total_cur(msgmap) == 1
	   && mn_is_cur(msgmap, mn_raw2m(msgmap, (long) rawno))
	   && (cur = mn_get_cur(msgmap)) > 1L
	   && cur < mn_get_total(msgmap)
	   && !get_lflag(stream, msgmap, cur + 1L, MN_CHID2)
	   && get_lflag(stream, msgmap, cur - 1L, MN_CHID2))
	  mn_set_cur(msgmap, cur - 1L);
    }

    /*
     * Keep on top of our special flag counts.
     * 
     * NOTE: This is allowed since mail_expunged releases
     * data for this message after the callback.
     */
    if(rawno > 0L && rawno <= stream->nmsgs && (mc = mail_elt(stream, rawno))){
      PINELT_S *pelt = (PINELT_S *) mc->sparep;

      if(pelt){
	if(pelt->hidden)
	  msgmap->flagged_hid--;

	if(pelt->excluded)
	  msgmap->flagged_exld--;

	if(pelt->selected)
	  msgmap->flagged_tmp--;

	if(pelt->colhid)
	  msgmap->flagged_chid--;

	if(pelt->colhid2)
	  msgmap->flagged_chid2--;

	if(pelt->collapsed)
	  msgmap->flagged_coll--;

	if(pelt->tmp)
	  msgmap->flagged_stmp--;

	if(pelt->unsorted)
	  msgmap->flagged_usor--;

	if(pelt->searched)
	  msgmap->flagged_srch--;

	if(pelt->hidden || pelt->colhid)
	  msgmap->flagged_invisible--;

	free_pine_elt(&mc->sparep);
      }
    }

    /*
     * if it's in the sort array, flush it, otherwise
     * decrement raw sequence numbers greater than "rawno"
     */
    mn_flush_raw(msgmap, (long) rawno);
}


void
mm_flags(MAILSTREAM *stream, long unsigned int rawno)
{
    /*
     * The idea here is to clean up any data pine might have cached
     * that has anything to do with the indicated message number.
     */
    if(stream == ps_global->mail_stream){
	long        msgno, t;
	PINETHRD_S *thrd;

	if(scores_are_used(SCOREUSE_GET) & SCOREUSE_STATEDEP)
	  clear_msg_score(stream, rawno);

	msgno = mn_raw2m(sp_msgmap(stream), (long) rawno);

	/* if in thread index */
	if(THRD_INDX()){
	    if((thrd = fetch_thread(stream, rawno))
	       && thrd->top
	       && (thrd = fetch_thread(stream, thrd->top))
	       && thrd->rawno
	       && (t = mn_raw2m(sp_msgmap(stream), thrd->rawno)))
	      clear_index_cache_ent(stream, t, 0);
	}
	else if(THREADING()){
	    if(msgno > 0L)
	      clear_index_cache_ent(stream, msgno, 0);

	    /*
	     * If a parent is collapsed, clear that parent's
	     * index cache entry.
	     */
	    if((thrd = fetch_thread(stream, rawno)) && thrd->parent){
		thrd = fetch_thread(stream, thrd->parent);
		while(thrd){
		    if(get_lflag(stream, NULL, thrd->rawno, MN_COLL)
		       && (t = mn_raw2m(sp_msgmap(stream), (long) thrd->rawno)))
		      clear_index_cache_ent(stream, t, 0);

		    if(thrd->parent)
		      thrd = fetch_thread(stream, thrd->parent);
		    else
		      thrd = NULL;
		}
	    }
	}
	else if(msgno > 0L)
	  clear_index_cache_ent(stream, msgno, 0);

	if(msgno && mn_is_cur(sp_msgmap(stream), msgno))
	  ps_global->mangled_header = 1;
    }
	    
    /*
     * We count up flag changes here. The
     * dont_count_flagchanges variable tries to prevent us from
     * counting when we're just fetching flags.
     */
    if(!(ps_global->dont_count_flagchanges
	 && stream == ps_global->mail_stream)){
	int exbits;

	check_point_change(stream);

	/* we also note flag changes for filtering purposes */
	if(msgno_exceptions(stream, rawno, "0", &exbits, FALSE))
	  exbits |= MSG_EX_STATECHG;
	else
	  exbits = MSG_EX_STATECHG;

	msgno_exceptions(stream, rawno, "0", &exbits, TRUE);
    }
}


void
mm_list(MAILSTREAM *stream, int delimiter, char *mailbox, long int attributes)
{
#ifdef DEBUG
    if(ps_global->debug_imap > 2 || ps_global->debugmem)
      dprint((5, "mm_list \"%s\": delim: '%c', %s%s%s%s%s%s\n",
	       mailbox ? mailbox : "?", delimiter ? delimiter : 'X',
	       (attributes & LATT_NOINFERIORS) ? ", no inferiors" : "",
	       (attributes & LATT_NOSELECT) ? ", no select" : "",
	       (attributes & LATT_MARKED) ? ", marked" : "",
	       (attributes & LATT_UNMARKED) ? ", unmarked" : "",
	       (attributes & LATT_HASCHILDREN) ? ", has children" : "",
	       (attributes & LATT_HASNOCHILDREN) ? ", has no children" : ""));
#endif

    if(!mm_list_info->stream || stream == mm_list_info->stream)
      (*mm_list_info->filter)(stream, mailbox, delimiter,
			      attributes, mm_list_info->data,
			      mm_list_info->options);
}


void
mm_lsub(MAILSTREAM *stream, int delimiter, char *mailbox, long int attributes)
{
#ifdef DEBUG
    if(ps_global->debug_imap > 2 || ps_global->debugmem)
      dprint((5, "LSUB \"%s\": delim: '%c', %s%s%s%s%s%s\n",
	       mailbox ? mailbox : "?", delimiter ? delimiter : 'X',
	       (attributes & LATT_NOINFERIORS) ? ", no inferiors" : "",
	       (attributes & LATT_NOSELECT) ? ", no select" : "",
	       (attributes & LATT_MARKED) ? ", marked" : "",
	       (attributes & LATT_UNMARKED) ? ", unmarked" : "",
	       (attributes & LATT_HASCHILDREN) ? ", has children" : "",
	       (attributes & LATT_HASNOCHILDREN) ? ", has no children" : ""));
#endif

    if(!mm_list_info->stream || stream == mm_list_info->stream)
      (*mm_list_info->filter)(stream, mailbox, delimiter,
			      attributes, mm_list_info->data,
			      mm_list_info->options);
}


void
mm_status(MAILSTREAM *stream, char *mailbox, MAILSTATUS *status)
{
    /*
     * We implement mail_status for the #move namespace by adding a wrapper
     * routine, pine_mail_status. It may have to call the real mail_status
     * twice for #move folders and combine the results. It sets
     * pine_cached_status to point to a local status variable to store the
     * intermediate results.
     */
    if(status){
	if(pine_cached_status != NULL)
	  *pine_cached_status = *status;
	else
	  mm_status_result = *status;
    }

#ifdef DEBUG
    if(status){
	if(pine_cached_status)
	  dprint((2,
		 "mm_status: Preliminary pass for #move\n"));

	dprint((2, "mm_status: Mailbox \"%s\"",
	       mailbox ? mailbox : "?"));
	if(status->flags & SA_MESSAGES)
	  dprint((2, ", %lu messages", status->messages));

	if(status->flags & SA_RECENT)
	  dprint((2, ", %lu recent", status->recent));

	if(status->flags & SA_UNSEEN)
	  dprint((2, ", %lu unseen", status->unseen));

	if(status->flags & SA_UIDVALIDITY)
	  dprint((2, ", %lu UID validity", status->uidvalidity));

	if(status->flags & SA_UIDNEXT)
	  dprint((2, ", %lu next UID", status->uidnext));

	dprint((2, "\n"));
    }
#endif
}


/*----------------------------------------------------------------------
      Write imap debugging information into log file

   Args: strings -- the string for the debug file

 Result: message written to the debug log file
  ----*/
void
mm_dlog(char *string)
{
    char *p, *q = NULL, save, *continued;
    int   more = 1;

#ifdef	_WINDOWS
    mswin_imaptelemetry(string);
#endif
#ifdef	DEBUG
    continued = "";
    p = string;
#ifdef DEBUGJOURNAL
    /* because string can be really long and we don't want to lose any of it */
    if(p)
      more = 1;

    while(more){
	if(q){
	    *q = save;
	    p = q;
	    continued = "(Continuation line) ";
	}

	if(strlen(p) > 63000){
	    q = p + 60000;
	    save = *q;
	    *q = '\0';
	}
	else
	  more = 0;
#endif
	dprint(((ps_global->debug_imap >= 4 && debug < 4) ? debug : 4,
		"IMAP DEBUG %s%s: %s\n",
		continued ? continued : "",
		debug_time(1, ps_global->debug_timestamp), p ? p : "?"));
#ifdef DEBUGJOURNAL
    }
#endif
#endif
}


/*----------------------------------------------------------------------
     Ignore signals when imap is running through critical code

 Args: stream -- The stream on which critical operation is proceeding
 ----*/

void 
mm_critical(MAILSTREAM *stream)
{
    stream = stream; /* For compiler complaints that this isn't used */

    if(++critical_depth == 1){

#ifdef	SIGHUP
	hold_hup  = signal(SIGHUP, SIG_IGN);
#endif
#ifdef	SIGUSR2
	hold_usr2 = signal(SIGUSR2, SIG_IGN);
#endif
#ifdef	SIGINT
	hold_int  = signal(SIGINT, SIG_IGN);
#endif
#ifdef	SIGTERM
	hold_term = signal(SIGTERM, SIG_IGN);
#endif
    }

    dprint((9, "IMAP critical (depth now %d) on %s\n",
              critical_depth,
	      (stream && stream->mailbox) ? stream->mailbox : "<no folder>" ));
}


/*----------------------------------------------------------------------
   Reset signals after critical imap code
 ----*/
void
mm_nocritical(MAILSTREAM *stream)
{ 
    stream = stream; /* For compiler complaints that this isn't used */

    if(--critical_depth == 0){

#ifdef	SIGHUP
	(void)signal(SIGHUP, hold_hup);
#endif
#ifdef	SIGUSR2
	(void)signal(SIGUSR2, hold_usr2);
#endif
#ifdef	SIGINT
	(void)signal(SIGINT, hold_int);
#endif
#ifdef	SIGTERM
	(void)signal(SIGTERM, hold_term);
#endif
    }

    critical_depth = MAX(critical_depth, 0);

    dprint((9, "Done with IMAP critical (depth now %d) on %s\n",
              critical_depth,
	      (stream && stream->mailbox) ? stream->mailbox : "<no folder>" ));
}


void
mm_fatal(char *message)
{
    panic(message);
}


char *
imap_referral(MAILSTREAM *stream, char *ref, long int code)
{
    char *buf = NULL;

    if(ref && !struncmp(ref, "imap://", 7)){
	char *folder = NULL;
	imapuid_t uid_val, uid;
	int rv;

	rv = url_imap_folder(ref, &folder, &uid, &uid_val, NULL, 1);
	switch(code){
	  case REFAUTHFAILED :
	  case REFAUTH :
	    if((rv & URL_IMAP_IMAILBOXLIST) && (rv & URL_IMAP_ISERVERONLY))
	      buf = cpystr(folder);

	    break;

	  case REFSELECT :
	  case REFCREATE :
	  case REFDELETE :
	  case REFRENAME :
	  case REFSUBSCRIBE :
	  case REFUNSUBSCRIBE :
	  case REFSTATUS :
	  case REFCOPY :
	  case REFAPPEND :
	    if(rv & URL_IMAP_IMESSAGELIST)
	      buf = cpystr(folder);

	    break;

	  default :
	    break;
	}

	if(folder)
	  fs_give((void **) &folder);
    }

    return(buf);
}


long
imap_proxycopy(MAILSTREAM *stream, char *sequence, char *mailbox, long int flags)
{
    SE_APP_S args;
    long ret;

    args.folder = mailbox;
    args.flags  = flags;

    if(pith_opt_save_size_changed_prompt)
      (*pith_opt_save_size_changed_prompt)(0L, SSCP_INIT);

    ret = imap_seq_exec(stream, sequence, imap_seq_exec_append, &args);

    if(pith_opt_save_size_changed_prompt)
      (*pith_opt_save_size_changed_prompt)(0L, SSCP_END);

    return(ret);
}


long
imap_seq_exec(MAILSTREAM *stream, char *sequence,
	      long int (*func)(MAILSTREAM *, long int, void *),
	      void *args)
{
    unsigned long i,j,x;

    while (*sequence) {		/* while there is something to parse */
	if (*sequence == '*') {	/* maximum message */
	    if(!(i = stream->nmsgs)){
		mm_log ("No messages, so no maximum message number",ERROR);
		return(0L);
	    }

	    sequence++;		/* skip past * */
	}
	else if (!(i = strtoul ((const char *) sequence,&sequence,10))
		 || (i > stream->nmsgs)){
	    mm_log ("Sequence invalid",ERROR);
	    return(0L);
	}

	switch (*sequence) {	/* see what the delimiter is */
	  case ':':			/* sequence range */
	    if (*++sequence == '*') {	/* maximum message */
		if (stream->nmsgs) j = stream->nmsgs;
		else {
		    mm_log ("No messages, so no maximum message number",ERROR);
		    return NIL;
		}

		sequence++;		/* skip past * */
	    }
				/* parse end of range */
	    else if (!(j = strtoul ((const char *) sequence,&sequence,10)) ||
		     (j > stream->nmsgs)) {
		mm_log ("Sequence range invalid",ERROR);
		return NIL;
	    }

	    if (*sequence && *sequence++ != ',') {
		mm_log ("Sequence range syntax error",ERROR);
		return NIL;
	    }

	    if (i > j) {		/* swap the range if backwards */
		x = i; i = j; j = x;
	    }

	    while (i <= j)
	      if(!(*func)(stream, i++, args))
		return(0L);

	    break;
	  case ',':			/* single message */
	    ++sequence;		/* skip the delimiter, fall into end case */
	  case '\0':		/* end of sequence */
	    if(!(*func)(stream, i, args))
	      return(0L);

	    break;
	  default:			/* anything else is a syntax error! */
	    mm_log ("Sequence syntax error",ERROR);
	    return NIL;
	}
    }

    return T;			/* successfully parsed sequence */
}


long
imap_seq_exec_append(MAILSTREAM *stream, long int msgno, void *args)
{
    char	 *save_folder, *flags = NULL, date[64];
    CONTEXT_S    *cntxt = NULL;
    int		  our_stream = 0;
    long	  rv = 0L;
    MAILSTREAM   *save_stream;
    SE_APP_S	 *sa = (SE_APP_S *) args;
    MESSAGECACHE *mc;
    STORE_S      *so;

    save_folder = (strucmp(sa->folder, ps_global->inbox_name) == 0)
		    ? ps_global->VAR_INBOX_PATH : sa->folder;

    save_stream = save_msg_stream(cntxt, save_folder, &our_stream);

    if((so = so_get(CharStar, NULL, WRITE_ACCESS)) != NULL){
	/* store flags before the fetch so UNSEEN bit isn't flipped */
	mc = (msgno > 0L && stream && msgno <= stream->nmsgs)
		? mail_elt(stream, msgno) : NULL;

	flags = flag_string(stream, msgno, F_ANS|F_FWD|F_FLAG|F_SEEN|F_KEYWORD);
	if(mc && mc->day)
	  mail_date(date, mc);
	else
	  *date = '\0';

	rv = save_fetch_append(stream, msgno, NULL,
			       save_stream, save_folder, NULL,
			       mc ? mc->rfc822_size : 0L, flags, date, so);
	if(flags)
	  fs_give((void **) &flags);

	if(rv < 0 || sp_expunge_count(stream)){
	    cmd_cancelled("Attached message Save");
	    rv = 0L;
	}
	/* else whatever broke in save_fetch_append shoulda bitched */

	so_give(&so);
    }
    else{
	dprint((1, "Can't allocate store for save: %s\n",
		   error_description(errno)));
	q_status_message(SM_ORDER | SM_DING, 3, 4,
			 "Problem creating space for message text.");
    }

    if(our_stream)
      mail_close(save_stream);

    return(rv);
}



/*----------------------------------------------------------------------
      Get login and password from user for IMAP login
  
  Args:  mb -- The mail box property struct
         user   -- Buffer to return the user name in 
         passwd -- Buffer to return the passwd in
         trial  -- The trial number or number of attempts to login
    user is at least size NETMAXUSER
    passwd is apparently at least MAILTMPLEN, but mrc has asked us to
      use a max size of about 100 instead

 Result: username and password passed back to imap
  ----*/
void
mm_login(NETMBX *mb, char *user, char *pwd, long int trial)
{
    mm_login_work(mb, user, pwd, trial, NULL, NULL);
}


/*----------------------------------------------------------------------
   Exported method to retrieve logged in user name associated with stream

   Args: host -- host to find associated login name with.

 Result: 
  ----*/
char *
cached_user_name(char *host)
{
    MMLOGIN_S *l;
    STRLIST_S *h;

    if((l = mm_login_list) && host)
      do
	for(h = l->hosts; h; h = h->next)
	  if(!strucmp(host, h->name))
	    return(l->user);
      while((l = l->next) != NULL);

    return(NULL);
}


int
imap_same_host(STRLIST_S *hl1, STRLIST_S *hl2)
{
    STRLIST_S *lp;

    for( ; hl1; hl1 = hl1->next)
      for(lp = hl2; lp; lp = lp->next)
      if(!strucmp(hl1->name, lp->name))
	return(TRUE);

    return(FALSE);
}


/*
 * For convenience, we use the same m_list structure (but a different
 * instance) for storing a list of hosts we've asked the user about when
 * SSL validation fails. If this function returns TRUE, that means we
 * have previously asked the user about this host. Ok_novalidate == 1 means
 * the user said yes, it was ok. Ok_novalidate == 0 means the user
 * said no. Warned means we warned them already.
 */
int
imap_get_ssl(MMLOGIN_S *m_list, STRLIST_S *hostlist, int *ok_novalidate, int *warned)
{
    MMLOGIN_S *l;
    
    for(l = m_list; l; l = l->next)
      if(imap_same_host(l->hosts, hostlist)){
	  if(ok_novalidate)
	    *ok_novalidate = l->ok_novalidate;

	  if(warned)
	    *warned = l->warned;

	  return(TRUE);
      }

    return(FALSE);
}


/*
 * Just trying to guess the username the user might want to use on this
 * host, the user will confirm.
 */
char *
imap_get_user(MMLOGIN_S *m_list, STRLIST_S *hostlist)
{
    MMLOGIN_S *l;
    
    for(l = m_list; l; l = l->next)
      if(imap_same_host(l->hosts, hostlist))
	return(l->user);

    return(NULL);
}


/*
 * If we have a matching hostname, username, and altflag in our cache,
 * attempt to login with the password from the cache.
 */
int
imap_get_passwd(MMLOGIN_S *m_list, char *passwd, char *user, STRLIST_S *hostlist, int altflag)
{
    MMLOGIN_S *l;
    
    dprint((9,
	       "imap_get_passwd: checking user=%s alt=%d host=%s%s%s\n",
	       user ? user : "(null)",
	       altflag,
	       hostlist->name ? hostlist->name : "",
	       (hostlist->next && hostlist->next->name) ? ", " : "",
	       (hostlist->next && hostlist->next->name) ? hostlist->next->name
						        : ""));
    for(l = m_list; l; l = l->next)
      if(imap_same_host(l->hosts, hostlist)
	 && *user
	 && !strcmp(user, l->user)
	 && l->altflag == altflag){
	  if(passwd){
	      strncpy(passwd, l->passwd, NETMAXPASSWD);
	      passwd[NETMAXPASSWD-1] = '\0';
	  }
	  dprint((9, "imap_get_passwd: match\n"));
	  dprint((10, "imap_get_passwd: trying passwd=\"%s\"\n",
		      passwd ? passwd : "?"));
	  return(TRUE);
      }

    dprint((9, "imap_get_passwd: no match\n"));
    return(FALSE);
}



void
imap_set_passwd(MMLOGIN_S **l, char *passwd, char *user, STRLIST_S *hostlist,
	        int altflag, int ok_novalidate, int warned)
{
    STRLIST_S **listp;
    size_t      len;

    dprint((9, "imap_set_passwd\n"));
    for(; *l; l = &(*l)->next)
      if(imap_same_host((*l)->hosts, hostlist)
	 && !strcmp(user, (*l)->user)
	 && altflag == (*l)->altflag){
	if(strcmp(passwd, (*l)->passwd) ||
	   (*l)->ok_novalidate != ok_novalidate ||
	   (*l)->warned != warned)
	  break;
	else
	  return;
      }

    if(!*l){
	*l = (MMLOGIN_S *)fs_get(sizeof(MMLOGIN_S));
	memset(*l, 0, sizeof(MMLOGIN_S));
    }

    len = strlen(passwd);
    if(!(*l)->passwd || strlen((*l)->passwd) < len)
      (*l)->passwd = ps_get(len+1);

    strncpy((*l)->passwd, passwd, len+1);

    (*l)->altflag = altflag;
    (*l)->ok_novalidate = ok_novalidate;
    (*l)->warned = warned;

    if(!(*l)->user)
      (*l)->user = cpystr(user);

    dprint((9, "imap_set_passwd: user=%s altflag=%d\n",
	   (*l)->user ? (*l)->user : "?",
	   (*l)->altflag));

    for( ; hostlist; hostlist = hostlist->next){
	for(listp = &(*l)->hosts;
	    *listp && strucmp((*listp)->name, hostlist->name);
	    listp = &(*listp)->next)
	  ;

	if(!*listp){
	    *listp = new_strlist(hostlist->name);
	    dprint((9, "imap_set_passwd: host=%s\n",
		       (*listp)->name ? (*listp)->name : "?"));
	}
    }

    dprint((10, "imap_set_passwd: passwd=\"%s\"\n",
	   passwd ? passwd : "?"));
}



void
imap_flush_passwd_cache(int dumpcache)
{
    size_t len;
    volatile char *p;

    /* equivalent of memset but can't be optimized away */
    len = sizeof(private_store);
    p = private_store;
    while(len-- > 0)
      *p++ = '\0';

    if(dumpcache){
	MMLOGIN_S *l;

	while((l = mm_login_list) != NULL){
	    mm_login_list = mm_login_list->next;
	    if(l->user)
	      fs_give((void **) &l->user);

	    free_strlist(&l->hosts);

	    fs_give((void **) &l);
	}

	while((l = cert_failure_list) != NULL){
	    cert_failure_list = cert_failure_list->next;
	    if(l->user)
	      fs_give((void **) &l->user);

	    free_strlist(&l->hosts);

	    fs_give((void **) &l);
	}
    }
}


/*
 * Mimics fs_get except it only works for char * (no alignment hacks), it
 * stores in a static array so it is easy to zero it out (that's the whole
 * purpose), allocations always happen at the end (no free).
 * If we go past array limit, we don't break, we just use free storage.
 * Should be awfully rare, though.
 */
char *
ps_get(size_t size)
{
    static char *last  = (char *) private_store;
    char        *block = NULL;

    /* there is enough space */
    if(size <= sizeof(private_store) - (last - (char *) private_store)){
	block = last;
	last += size;
    }
    else{
	dprint((2,
		   "Out of password caching space in private_store\n"));
	dprint((2,
		   "Using free storage instead\n"));
	block = fs_get(size);
    }

    return(block);
}
