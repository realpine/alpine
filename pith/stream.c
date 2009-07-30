#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: stream.c 1012 2008-03-26 00:44:22Z hubert@u.washington.edu $";
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

/*======================================================================
     stream.c
     Implements the Pine mail stream management routines
     and c-client wrapper functions
  ====*/


#include "../pith/headers.h"
#include "../pith/stream.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/flag.h"
#include "../pith/msgno.h"
#include "../pith/adrbklib.h"
#include "../pith/status.h"
#include "../pith/newmail.h"
#include "../pith/detach.h"
#include "../pith/folder.h"
#include "../pith/mailcmd.h"
#include "../pith/util.h"
#include "../pith/news.h"
#include "../pith/sequence.h"
#include "../pith/options.h"
#include "../pith/mimedesc.h"


void     (*pith_opt_closing_stream)(MAILSTREAM *);


/*
 * Internal prototypes
 */
void	 reset_stream_view_state(MAILSTREAM *);
void	 carefully_reset_sp_flags(MAILSTREAM *, unsigned long);
char	*partial_text_gets(readfn_t, void *, unsigned long, GETS_DATA *);
void	 mail_list_internal(MAILSTREAM *, char *, char *);
int	 recent_activity(MAILSTREAM *);
int	 hibit_in_searchpgm(SEARCHPGM *);
int	 hibit_in_strlist(STRINGLIST *);
int	 hibit_in_header(SEARCHHEADER *);
int	 hibit_in_sizedtext(SIZEDTEXT *);
int      sp_nusepool_notperm(void);
int      sp_add(MAILSTREAM *, int);
void     sp_delete(MAILSTREAM *);
void     sp_free(PER_STREAM_S **);


static FETCH_READC_S *g_pft_desc;


MAILSTATUS *pine_cached_status;  /* implement status for #move folder */



/*
 * Pine wrapper around mail_open. If we have the PREFER_ALT_AUTH flag turned
 * on, we need to set the TRYALT flag before trying the open.
 * This routine manages the stream pool, too. It tries to re-use existing
 * streams instead of opening new ones, or maybe it will leave one open and
 * use a new one if that seems to make more sense. Pine_mail_close leaves
 * streams open so that they may be re-used. Each pine_mail_open should have
 * a matching pine_mail_close (or possible pine_mail_actually_close) somewhere
 * that goes with it.
 *
 * Args:
 *      stream -- A possible stream for recycling. This isn't usually the
 *                way recycling happens. Usually it is automatic.
 *     mailbox -- The mailbox to be opened.
 *   openflags -- Flags passed here to modify the behavior.
 *    retflags -- Flags returned from here. SP_MATCH will be lit if that is
 *                what happened. If SP_MATCH is lit then SP_LOCKED may also
 *                be lit if the matched stream was already locked when
 *                we got here.
 */
MAILSTREAM *
pine_mail_open(MAILSTREAM *stream, char *mailbox, long int openflags, long int *retflags)
{
    MAILSTREAM *retstream = NULL;
    DRIVER     *d;
    int         permlocked = 0, is_inbox = 0, usepool = 0, tempuse = 0, uf = 0;
    unsigned long flags;
    char      **lock_these, *target = NULL;
    static unsigned long streamcounter = 0;

    dprint((7,
    "pine_mail_open: opening \"%s\"%s openflags=0x%x %s%s%s%s%s%s%s%s%s (%s)\n",
	   mailbox ? mailbox : "(NULL)",
	   stream ? "" : " (stream was NULL)",
	   openflags,
	   openflags & OP_HALFOPEN   ? " OP_HALFOPEN"   : "",
	   openflags & OP_READONLY   ? " OP_READONLY"   : "",
	   openflags & OP_SILENT     ? " OP_SILENT"     : "",
	   openflags & OP_DEBUG      ? " OP_DEBUG"      : "",
	   openflags & SP_PERMLOCKED ? " SP_PERMLOCKED" : "",
	   openflags & SP_INBOX      ? " SP_INBOX"      : "",
	   openflags & SP_USERFLDR   ? " SP_USERFLDR"   : "",
	   openflags & SP_USEPOOL    ? " SP_USEPOOL"    : "",
	   openflags & SP_TEMPUSE    ? " SP_TEMPUSE"    : "",
	   debug_time(1, ps_global->debug_timestamp)));
    
    if(retflags)
      *retflags = 0L;

    if(ps_global->user_says_cancel){
	dprint((7, "pine_mail_open: cancelled by user\n"));
	return(retstream);
    }

    is_inbox    = openflags & SP_INBOX;
    uf          = openflags & SP_USERFLDR;

    /* inbox is still special, assume that we want to permlock it */
    permlocked  = (is_inbox || openflags & SP_PERMLOCKED) ? 1 : 0;

    /* check to see if user wants this folder permlocked */
    for(lock_these = ps_global->VAR_PERMLOCKED;
	uf && !permlocked && lock_these && *lock_these; lock_these++){
	char *p = NULL, *dummy = NULL, *lt, *lname, *mname;
	char  tmp1[MAILTMPLEN], tmp2[MAILTMPLEN];

	/* there isn't really a pair, it just dequotes for us */
	get_pair(*lock_these, &dummy, &p, 0, 0);

	/*
	 * Check to see if this is an incoming nickname and replace it
	 * with the full name.
	 */
	if(!(p && ps_global->context_list
	     && ps_global->context_list->use & CNTXT_INCMNG
	     && (lt=folder_is_nick(p, FOLDERS(ps_global->context_list), 0))))
	  lt = p;

	if(dummy)
	  fs_give((void **) &dummy);
	
	if(lt && mailbox
	   && (same_remote_mailboxes(mailbox, lt)
	       ||
	       (!IS_REMOTE(mailbox)
	        && (lname=mailboxfile(tmp1, lt))
	        && (mname=mailboxfile(tmp2, mailbox))
	        && !strcmp(lname, mname))))
	  permlocked++;

	if(p)
	  fs_give((void **) &p);
    }

    /*
     * Only cache if remote, not nntp, not pop, and caller asked us to.
     * It might make sense to do some caching for nntp and pop, as well, but
     * we aren't doing it right now. For example, an nntp stream open to
     * one group could be reused for another group. An open pop stream could
     * be used for mail_copy_full.
     *
     * An implication of doing only imap here is that sp_stream_get will only
     * be concerned with imap streams.
     */
    if((d = mail_valid (NIL, mailbox, (char *) NIL)) && !strcmp(d->name, "imap")){
	usepool = openflags & SP_USEPOOL;
	tempuse = openflags & SP_TEMPUSE;
    }
    else{
	if(IS_REMOTE(mailbox)){
	    dprint((9, "pine_mail_open: not cacheable: %s\n", !d ? "no driver?" : d->name ? d->name : "?" ));
	}
	else{
	    if(permlocked || (openflags & OP_READONLY)){
		/*
		 * This is a strange case. We want to allow stay-open local
		 * folders, but they don't fit into the rest of the framework
		 * well. So we'll look for it being already open in this case
		 * and special-case it (the already_open_stream() case
		 * below).
		 */
		dprint((9,
   "pine_mail_open: not cacheable: not remote, but check for local stream\n"));
	    }
	    else{
		dprint((9,
		       "pine_mail_open: not cacheable: not remote\n"));
	    }
	}
    }

    /* If driver doesn't support halfopen, just open it. */
    if(d && (openflags & OP_HALFOPEN) && !(d->flags & DR_HALFOPEN)){
	openflags &= ~OP_HALFOPEN;
	dprint((9,
	       "pine_mail_open: turning off OP_HALFOPEN flag\n"));
    }

    /*
     * Some of the flags are pine's, the rest are meant for mail_open.
     * We've noted the pine flags, now remove them before we call mail_open.
     */
    openflags &= ~(SP_USEPOOL | SP_TEMPUSE | SP_INBOX
		   | SP_PERMLOCKED | SP_USERFLDR);

#ifdef	DEBUG
    if(ps_global->debug_imap > 3 || ps_global->debugmem)
      openflags |= OP_DEBUG;
#endif
    
    if(F_ON(F_PREFER_ALT_AUTH, ps_global)){
	if((d = mail_valid (NIL, mailbox, (char *) NIL))
	   && !strcmp(d->name, "imap"))
	  openflags |= OP_TRYALT;
    }

    if(F_ON(F_ENABLE_MULNEWSRCS, ps_global)){
	char source[MAILTMPLEN];
	if(check_for_move_mbox(mailbox, source, sizeof(source), &target)){
	    DRIVER *d;
	    if((d = mail_valid(NIL, source, (char *) NIL))
	       && (!strcmp(d->name, "news")
		   || !strcmp(d->name, "nntp")))
	      openflags |= OP_MULNEWSRC;
	}
	else if((d = mail_valid(NIL, mailbox, (char *) NIL))
	   && !strcmp(d->name, "nntp"))
	  openflags |= OP_MULNEWSRC;
    }

    /*    
     * One of the problems is that the new-style stream caching (the
     * sp_stream_get stuff) may conflict with some of the old-style caching
     * (the passed in argument stream) that is still in the code. We should
     * probably eliminate the old-style caching, but some of it is still useful,
     * especially if it deals with something other than IMAP. We want to prevent
     * mistakes caused by conflicts between the two styles. In particular, we
     * don't want to have a new-style cached stream re-opened because of the
     * old-style caching code. This can happen if a stream is passed in that
     * is not useable, and then a new stream is opened because the passed in
     * stream causes us to bypass the new caching code. Play it safe. If it
     * is an IMAP stream, just close it. This should leave it in the new-style
     * cache anyway, causing no loss. Maybe not if the cache wasn't large
     * enough to have it in there in the first place, in which case we get
     * a possibly unnecessary close and open. If it isn't IMAP we still have
     * to worry about it because it will cause us to bypass the caching code.
     * So if the stream isn't IMAP but the mailbox we're opening is, close it.
     * The immediate alternative would be to try to emulate the code in
     * mail_open that checks whether it is re-usable or not, but that is
     * dangerous if that code changes on us.
     */
    if(stream){
	if(is_imap_stream(stream)
	   || ((d = mail_valid (NIL, mailbox, (char *) NIL))
	       && !strcmp(d->name, "imap"))){
	    if(is_imap_stream(stream)){
		dprint((7,
		       "pine_mail_open: closing passed in IMAP stream %s\n",
		       stream->mailbox ? stream->mailbox : "?"));
	    }
	    else{
		dprint((7,
		       "pine_mail_open: closing passed in non-IMAP stream %s\n",
		       stream->mailbox ? stream->mailbox : "?"));
	    }

	    pine_mail_close(stream);
	    stream = NULL;
	}
    }

    /*
     * Maildrops are special. The mailbox name will be a #move name. If the
     * target of the maildrop is an IMAP folder we want to be sure it isn't
     * already open in another cached stream, to avoid double opens. This
     * could have happened if the user opened it manually as the target
     * instead of as a maildrop. It could also be a side-effect of marking
     * an answered flag after a reply.
     */
    target = NULL;
    if(check_for_move_mbox(mailbox, NULL, 0, &target)){
	MAILSTREAM *targetstream = NULL;

	if((d = mail_valid (NIL, target, (char *) NIL)) && !strcmp(d->name, "imap")){
	    targetstream = sp_stream_get(target, SP_MATCH | SP_RO_OK);
	    if(targetstream){
		dprint((9, "pine_mail_open: close previously opened target of maildrop\n"));
		pine_mail_actually_close(targetstream);
	    }
	}
    }

    if((usepool && !stream && permlocked)
       || (!usepool && (permlocked || (openflags & OP_READONLY))
	   && (retstream = already_open_stream(mailbox, AOS_NONE)))){
	if(retstream)
	  stream = retstream;
	else
	  stream = sp_stream_get(mailbox,
			SP_MATCH | ((openflags & OP_READONLY) ? SP_RO_OK : 0));
	if(stream){
	    flags = SP_LOCKED
		    | (usepool    ? SP_USEPOOL    : 0)
		    | (permlocked ? SP_PERMLOCKED : 0)
		    | (is_inbox   ? SP_INBOX      : 0)
		    | (uf         ? SP_USERFLDR   : 0)
		    | (tempuse    ? SP_TEMPUSE    : 0);

	    /*
	     * If the stream wasn't already locked, then we reset it so it
	     * looks like we are reopening it. We have to worry about recent
	     * messages since they will still be recent, if that affects us.
	     */
	    if(!(sp_flags(stream) & SP_LOCKED))
	      reset_stream_view_state(stream);

	    if(retflags){
		*retflags |= SP_MATCH;
		if(sp_flags(stream) & SP_LOCKED)
		  *retflags |= SP_LOCKED;
	    }

	    if(sp_flags(stream) & SP_LOCKED
	       && sp_flags(stream) & SP_USERFLDR
	       && !(flags & SP_USERFLDR)){
		sp_set_ref_cnt(stream, sp_ref_cnt(stream)+1);
		dprint((7,
		       "pine_mail_open: permlocked: ref cnt up to %d\n",
		       sp_ref_cnt(stream)));
	    }
	    else if(sp_ref_cnt(stream) <= 0){
		sp_set_ref_cnt(stream, 1);
		dprint((7,
		       "pine_mail_open: permexact: ref cnt set to %d\n",
		       sp_ref_cnt(stream)));
	    }

	    carefully_reset_sp_flags(stream, flags);

	    if(stream->silent && !(openflags & OP_SILENT))
	      stream->silent = NIL;

	    dprint((9, "pine_mail_open: stream was already open\n"));
	    if(stream && stream->dtb && stream->dtb->name
	       && !strcmp(stream->dtb->name, "imap")){
		dprint((7, "pine_mail_open: next TAG %08lx\n",
		       stream->gensym));
	    }

	    return(stream);
	}
    }

    if(usepool && !stream){
	/*
	 * First, we look for an exact match, a stream which is already
	 * open to the mailbox we are trying to re-open, and we use that.
	 * Skip permlocked only because we did it above already.
	 */
	if(!permlocked)
	  stream = sp_stream_get(mailbox,
			SP_MATCH | ((openflags & OP_READONLY) ? SP_RO_OK : 0));

	if(stream){
	    flags = SP_LOCKED
		    | (usepool    ? SP_USEPOOL    : 0)
		    | (permlocked ? SP_PERMLOCKED : 0)
		    | (is_inbox   ? SP_INBOX      : 0)
		    | (uf         ? SP_USERFLDR   : 0)
		    | (tempuse    ? SP_TEMPUSE    : 0);

	    /*
	     * If the stream wasn't already locked, then we reset it so it
	     * looks like we are reopening it. We have to worry about recent
	     * messages since they will still be recent, if that affects us.
	     */
	    if(!(sp_flags(stream) & SP_LOCKED))
	      reset_stream_view_state(stream);

	    if(retflags){
		*retflags |= SP_MATCH;
		if(sp_flags(stream) & SP_LOCKED)
		  *retflags |= SP_LOCKED;
	    }

	    if(sp_flags(stream) & SP_LOCKED
	       && sp_flags(stream) & SP_USERFLDR
	       && !(flags & SP_USERFLDR)){
		sp_set_ref_cnt(stream, sp_ref_cnt(stream)+1);
		dprint((7,
		       "pine_mail_open: matched: ref cnt up to %d\n",
		       sp_ref_cnt(stream)));
	    }
	    else if(sp_ref_cnt(stream) <= 0){
		sp_set_ref_cnt(stream, 1);
		dprint((7,
		       "pine_mail_open: exact: ref cnt set to %d\n",
		       sp_ref_cnt(stream)));
	    }

	    carefully_reset_sp_flags(stream, flags);

	    /*
	     * We may be re-using a stream that was previously open
	     * with OP_SILENT and now we don't want OP_SILENT.
	     * We don't turn !silent into silent because the !silentness
	     * could be important in the original context (for example,
	     * silent suppresses mm_expunged calls).
	     *
	     * WARNING: we're messing with c-client internals.
	     */
	    if(stream->silent && !(openflags & OP_SILENT))
	      stream->silent = NIL;

	    dprint((9, "pine_mail_open: stream already open\n"));
	    if(stream && stream->dtb && stream->dtb->name
	       && !strcmp(stream->dtb->name, "imap")){
		dprint((7, "pine_mail_open: next TAG %08lx\n",
		       stream->gensym));
	    }

	    return(stream);
	}

	/*
	 * No exact match, look for a stream which is open to the same
	 * server and marked for TEMPUSE.
	 */
	stream = sp_stream_get(mailbox, SP_SAME | SP_TEMPUSE);
	if(stream){
	    dprint((9,
		    "pine_mail_open: attempting to re-use TEMP stream\n"));
	}
	/*
	 * No SAME/TEMPUSE stream so we look to see if there is an
	 * open slot available (we're not yet at max_remstream). If there
	 * is an open slot, we'll just open a new stream and put it there.
	 * If not, we'll go inside this conditional.
	 */
	else if(!permlocked
		&& sp_nusepool_notperm() >= ps_global->s_pool.max_remstream){
	    dprint((9,
		    "pine_mail_open: no empty slots\n"));
	    /*
	     * No empty remote slots available. See if there is a
	     * TEMPUSE stream that is open but to the wrong server.
	     */
	    stream = sp_stream_get(mailbox, SP_TEMPUSE);
	    if(stream){
		/*
		 * We will close this stream and use the empty slot
		 * that that creates.
		 */
		dprint((9,
	    "pine_mail_open: close a TEMPUSE stream and re-use that slot\n"));
		pine_mail_actually_close(stream);
		stream = NULL;
	    }
	    else{

		/*
		 * Still no luck. Look for a stream open to the same
		 * server that is just not locked. This would be a
		 * stream that might be reusable in the future, but we
		 * need it now instead.
		 */
		stream = sp_stream_get(mailbox, SP_SAME | SP_UNLOCKED);
		if(stream){
		    dprint((9,
			    "pine_mail_open: attempting to re-use stream\n"));
		}
		else{
		    /*
		     * We'll take any UNLOCKED stream and re-use it.
		     */
		    stream = sp_stream_get(mailbox, 0);
		    if(stream){
			/*
			 * We will close this stream and use the empty slot
			 * that that creates.
			 */
			dprint((9,
	    "pine_mail_open: close an UNLOCKED stream and re-use the slot\n"));
			pine_mail_actually_close(stream);
			stream = NULL;
		    }
		    else{
			if(ps_global->s_pool.max_remstream){
			  dprint((9, "pine_mail_open: all USEPOOL slots full of LOCKED streams, nothing to use\n"));
			}
			else{
			  dprint((9, "pine_mail_open: no caching, max_remstream == 0\n"));
			}

			usepool = 0;
			tempuse = 0;
			if(permlocked){
			    permlocked = 0;
			    dprint((2,
		    "pine_mail_open5: Can't mark folder Stay-Open: at max-remote limit\n"));
			    q_status_message1(SM_ORDER, 3, 5,
		  "Can't mark folder Stay-Open: reached max-remote limit (%s)",
		  comatose((long) ps_global->s_pool.max_remstream));
			}
		    }
		}
	    }
	}
	else{
	    dprint((9,
		    "pine_mail_open: there is an empty slot to use\n"));
	}

	/*
	 * We'll make an assumption here. If we were asked to halfopen a
	 * stream then we'll assume that the caller doesn't really care if
	 * the stream is halfopen or if it happens to be open to some mailbox
	 * already. They are just saying halfopen because they don't need to
	 * SELECT a mailbox. That's the assumption, anyway.
	 */
	if(openflags & OP_HALFOPEN && stream){
	    dprint((9,
	     "pine_mail_open: asked for HALFOPEN so returning stream as is\n"));
	    sp_set_ref_cnt(stream, sp_ref_cnt(stream)+1);
	    dprint((7,
		   "pine_mail_open: halfopen: ref cnt up to %d\n",
		   sp_ref_cnt(stream)));
	    if(stream && stream->dtb && stream->dtb->name
	       && !strcmp(stream->dtb->name, "imap")){
		dprint((7, "pine_mail_open: next TAG %08lx\n",
		       stream->gensym));
	    }

	    if(stream->silent && !(openflags & OP_SILENT))
	      stream->silent = NIL;

	    return(stream);
	}

	/*
	 * We're going to SELECT another folder with this open stream.
	 */
	if(stream){
	    /*
	     * We will have just pinged the stream to make sure it is
	     * still alive. That ping may have discovered some new mail.
	     * Before unselecting the folder, we should process the filters
	     * for that new mail.
	     */
	    if(!sp_flagged(stream, SP_LOCKED)
	       && !sp_flagged(stream, SP_USERFLDR)
	       && sp_new_mail_count(stream))
	      process_filter_patterns(stream, sp_msgmap(stream),
				      sp_new_mail_count(stream));

	    if(stream && stream->dtb && stream->dtb->name
	       && !strcmp(stream->dtb->name, "imap")){
		dprint((7,
		   "pine_mail_open: cancel idle timer: TAG %08lx (%s)\n",
		   stream->gensym, debug_time(1, ps_global->debug_timestamp)));
	    }

	    /*
	     * We need to reset the counters and everything.
	     * The easiest way to do that is just to delete all of the
	     * sp_s data and let the open fill it in correctly for
	     * the new folder.
	     */
	    sp_free((PER_STREAM_S **) &stream->sparep);
	}
    }

    /*
     * When we pass a stream to mail_open, it will either re-use it or
     * close it.
     */
    retstream = mail_open(stream, mailbox, openflags);

    if(retstream){

	dprint((7, "pine_mail_open: mail_open returns stream:\n  original_mailbox=%s\n  mailbox=%s\n  driver=%s rdonly=%d halfopen=%d secure=%d nmsgs=%ld recent=%ld\n", retstream->original_mailbox ? retstream->original_mailbox : "?", retstream->mailbox ? retstream->mailbox : "?", (retstream->dtb && retstream->dtb->name) ? retstream->dtb->name : "?", retstream->rdonly, retstream->halfopen, retstream->secure, retstream->nmsgs, retstream->recent));

	/*
	 * So it is easier to figure out which command goes with which
	 * stream when debugging, change the tag associated with each stream.
	 * Of course, this will come after the SELECT, so the startup IMAP
	 * commands will have confusing tags.
	 */
	if(retstream != stream && retstream->dtb && retstream->dtb->name
	   && !strcmp(retstream->dtb->name, "imap")){
	    retstream->gensym += (streamcounter * 0x1000000);
	    streamcounter = (streamcounter + 1) % (8 * 16);
	}

	if(retstream && retstream->dtb && retstream->dtb->name
	   && !strcmp(retstream->dtb->name, "imap")){
	    dprint((7, "pine_mail_open: next TAG %08lx\n",
		   retstream->gensym));
	}

	/*
	 * Catch the case where our test up above (where usepool was set)
	 * did not notice that this was news, but that we can tell once
	 * we've opened the stream. (One such case would be an imap proxy
	 * to an nntp server.)  Remove it from being cached here. There was
	 * a possible penalty for not noticing sooner. If all the usepool
	 * slots were full, we will have closed one of the UNLOCKED streams
	 * above, preventing us from future re-use of that stream.
	 * We could figure out how to do the test better with just the
	 * name. We could open the stream and then close the other one
	 * after the fact. Or we could just not worry about it since it is
	 * rare.
	 */
	if(IS_NEWS(retstream)){
	    usepool = 0;
	    tempuse = 0;
	}

	/* make sure the message map has been instantiated */
	(void) sp_msgmap(retstream);

	/*
	 * If a referral during the mail_open above causes a stream
	 * re-use which involves a BYE on an IMAP stream, then that
	 * BYE will have caused an mm_notify which will have
	 * instantiated the sp_data and set dead_stream! Trust that
	 * it is alive up to here and reset it to zero.
	 */
	sp_set_dead_stream(retstream, 0);

	flags = SP_LOCKED
		| (usepool    ? SP_USEPOOL    : 0)
		| (permlocked ? SP_PERMLOCKED : 0)
		| (is_inbox   ? SP_INBOX      : 0)
		| (uf         ? SP_USERFLDR   : 0)
		| (tempuse    ? SP_TEMPUSE    : 0);

	sp_flag(retstream, flags);
	sp_set_recent_since_visited(retstream, retstream->recent);

	/* initialize the reference count */
	sp_set_ref_cnt(retstream, 1);
	dprint((7, "pine_mail_open: reset: ref cnt set to %d\n",
		   sp_ref_cnt(retstream)));

	if(sp_add(retstream, usepool) != 0 && usepool){
	    usepool = 0;
	    tempuse = 0;
	    flags = SP_LOCKED
		    | (usepool    ? SP_USEPOOL    : 0)
		    | (permlocked ? SP_PERMLOCKED : 0)
		    | (is_inbox   ? SP_INBOX      : 0)
		    | (uf         ? SP_USERFLDR   : 0)
		    | (tempuse    ? SP_TEMPUSE    : 0);

	    sp_flag(retstream, flags);
	    (void) sp_add(retstream, usepool);
	}


	/*
	 * When opening a newsgroup, c-client marks the messages up to the
	 * last Deleted as Unseen. If the feature news-approximates-new-status
	 * is on, we'd rather they be treated as Seen. That way, selecting New
	 * messages will give us the ones past the last Deleted. So we're going
	 * to change them to Seen. Since Seen is a session flag for news, making
	 * this change won't have any permanent effect. C-client also marks the
	 * messages after the last deleted Recent, which is the bit of
	 * information we'll use to find the  messages we want to change.
	 */
	if(F_ON(F_FAKE_NEW_IN_NEWS, ps_global) &&
	   retstream->nmsgs > 0 && IS_NEWS(retstream)){
	    char         *seq;
	    long          i, mflags = ST_SET;
	    MESSAGECACHE *mc;

	    /*
	     * Search for !recent messages to set the searched bit for
	     * those messages we want to change. Then we'll flip the bits.
	     */
	    (void)count_flagged(retstream, F_UNRECENT);

	    for(i = 1L; i <= retstream->nmsgs; i++)
	      if((mc = mail_elt(retstream,i)) && mc->searched)
		mc->sequence = 1;
	      else
		mc->sequence = 0;

	    if(!is_imap_stream(retstream))
		mflags |= ST_SILENT;
	    if((seq = build_sequence(retstream, NULL, NULL)) != NULL){
		mail_flag(retstream, seq, "\\SEEN", mflags);
		fs_give((void **)&seq);
	    }
	}
    }

    return(retstream);
}


void
reset_stream_view_state(MAILSTREAM *stream)
{
    MSGNO_S *mm;

    if(!stream)
      return;

    mm = sp_msgmap(stream);

    if(!mm)
      return;

    sp_set_viewing_a_thread(stream, 0);
    sp_set_need_to_rethread(stream, 0);

    mn_reset_cur(mm, stream->nmsgs > 0L ? stream->nmsgs : 0L);  /* default */

    mm->visible_threads = -1L;
    mm->top             = 0L;
    mm->max_thrdno      = 0L;
    mm->top_after_thrd  = 0L;

    mn_set_mansort(mm, 0);

    /*
     * Get rid of zooming and selections, but leave filtering flags. All the
     * flag counts and everything should still be correct because set_lflag
     * preserves them correctly.
     */
    if(any_lflagged(mm, MN_SLCT | MN_HIDE)){
	long i;

	for(i = 1L; i <= mn_get_total(mm); i++)
	  set_lflag(stream, mm, i, MN_SLCT | MN_HIDE, 0);
    }

    /*
     * We could try to set up a default sort order, but the caller is going
     * to re-sort anyway if they are interested in sorting. So we won't do
     * it here.
     */
}


/*
 * We have to be careful when we change the flags of an already
 * open stream, because there may be more than one section of
 * code actively using the stream.
 * We allow turning on (but not off) SP_LOCKED
 *                                   SP_PERMLOCKED
 *                                   SP_USERFLDR
 *                                   SP_INBOX
 * We allow turning off (but not on) SP_TEMPUSE
 */
void
carefully_reset_sp_flags(MAILSTREAM *stream, long unsigned int flags)
{
    if(sp_flags(stream) != flags){
	/* allow turning on but not off */
	if(sp_flags(stream) & SP_LOCKED && !(flags & SP_LOCKED))
	  flags |= SP_LOCKED;

	if(sp_flags(stream) & SP_PERMLOCKED && !(flags & SP_PERMLOCKED))
	  flags |= SP_PERMLOCKED;

	if(sp_flags(stream) & SP_USERFLDR && !(flags & SP_USERFLDR))
	  flags |= SP_USERFLDR;

	if(sp_flags(stream) & SP_INBOX && !(flags & SP_INBOX))
	  flags |= SP_INBOX;


	/* allow turning off but not on */
	if(!(sp_flags(stream) & SP_TEMPUSE) && flags & SP_TEMPUSE)
	  flags &= ~SP_TEMPUSE;
	

	/* leave the way they already are */
	if((sp_flags(stream) & SP_FILTERED) != (flags & SP_FILTERED))
	  flags = (flags & ~SP_FILTERED) | (sp_flags(stream) & SP_FILTERED);


	if(sp_flags(stream) != flags)
	  sp_flag(stream, flags);
    }
}


/*
 * Pine wrapper around mail_create. If we have the PREFER_ALT_AUTH flag turned
 * on we don't want to pass a NULL stream to c-client because it will open
 * a non-ssl connection when we want it to be ssl.
 */
long
pine_mail_create(MAILSTREAM *stream, char *mailbox)
{
    MAILSTREAM *ourstream = NULL;
    long        return_val;
    long openflags = (OP_HALFOPEN | OP_SILENT | SP_USEPOOL | SP_TEMPUSE);
    char        source[MAILTMPLEN], *target = NULL;
    DRIVER     *d;

    dprint((7, "pine_mail_create: creating \"%s\"%s\n", 
	       mailbox ? mailbox : "(NULL)",
	       stream ? "" : " (stream was NULL)"));

    if(check_for_move_mbox(mailbox, source, sizeof(source), &target)){
	mailbox = target;
	dprint((7,
		   "pine_mail_create: #move special case, creating \"%s\"\n", 
		   mailbox ? mailbox : "(NULL)"));
    }

    /*
     * We don't really need this anymore, since we are now using IMAPTRYALT.
     * We'll leave it since it works.
     */
    if((F_ON(F_PREFER_ALT_AUTH, ps_global)
       || (ps_global->debug_imap > 3 || ps_global->debugmem))){

	if((d = mail_valid (NIL, mailbox, (char *) NIL))
	   && !strcmp(d->name, "imap")){

	    if(F_ON(F_PREFER_ALT_AUTH, ps_global))
	      openflags |= OP_TRYALT;
	}
    }

    if(!stream)
      stream = sp_stream_get(mailbox, SP_MATCH);
    if(!stream)
      stream = sp_stream_get(mailbox, SP_SAME);

    if(!stream){
	/*
	 * It is only useful to open a stream in the imap case.
	 */
	if((d = mail_valid (NIL, mailbox, (char *) NIL))
	   && !strcmp(d->name, "imap")){

	    stream = pine_mail_open(NULL, mailbox, openflags, NULL);
	    ourstream = stream;
	}
    }

    return_val = mail_create(stream, mailbox);

    if(ourstream)
      pine_mail_close(ourstream);

    return(return_val);
}


/*
 * Pine wrapper around mail_delete.
 */
long
pine_mail_delete(MAILSTREAM *stream, char *mailbox)
{
    MAILSTREAM *ourstream = NULL;
    long        return_val;
    long openflags = (OP_HALFOPEN | OP_SILENT | SP_USEPOOL | SP_TEMPUSE);
    char        source[MAILTMPLEN], *target = NULL;
    DRIVER     *d;

    dprint((7, "pine_mail_delete: deleting \"%s\"%s\n", 
	       mailbox ? mailbox : "(NULL)",
	       stream ? "" : " (stream was NULL)"));

    if(check_for_move_mbox(mailbox, source, sizeof(source), &target)){
	mailbox = target;
	dprint((7,
		   "pine_mail_delete: #move special case, deleting \"%s\"\n", 
		   mailbox ? mailbox : "(NULL)"));
    }

    /*
     * We don't really need this anymore, since we are now using IMAPTRYALT.
     */
    if((F_ON(F_PREFER_ALT_AUTH, ps_global)
       || (ps_global->debug_imap > 3 || ps_global->debugmem))){

	if((d = mail_valid (NIL, mailbox, (char *) NIL))
	   && !strcmp(d->name, "imap")){

	    if(F_ON(F_PREFER_ALT_AUTH, ps_global))
	      openflags |= OP_TRYALT;
	}
    }

    /* oops, we seem to be deleting a selected stream */
    if(!stream && (stream = sp_stream_get(mailbox, SP_MATCH))){
	pine_mail_actually_close(stream);
	stream = NULL;
    }

    if(!stream)
      stream = sp_stream_get(mailbox, SP_SAME);

    if(!stream){
	/*
	 * It is only useful to open a stream in the imap case.
	 */
	if((d = mail_valid (NIL, mailbox, (char *) NIL))
	   && !strcmp(d->name, "imap")){

	    stream = pine_mail_open(NULL, mailbox, openflags, NULL);
	    ourstream = stream;
	}
    }

    return_val = mail_delete(stream, mailbox);

    if(ourstream)
      pine_mail_close(ourstream);

    return(return_val);
}


/*
 * Pine wrapper around mail_append.
 */
long
pine_mail_append_full(MAILSTREAM *stream, char *mailbox, char *flags, char *date, STRING *message)
{
    MAILSTREAM *ourstream = NULL;
    long        return_val;
    long openflags = (OP_HALFOPEN | OP_SILENT | SP_USEPOOL | SP_TEMPUSE);
    char        source[MAILTMPLEN], *target = NULL;
    char        mailbox_nodelim[MAILTMPLEN];
    int         delim;
    DRIVER     *d;

    dprint((7, "pine_mail_append_full: appending to \"%s\"%s\n", 
	       mailbox ? mailbox : "(NULL)",
	       stream ? "" : " (stream was NULL)"));

    /* strip delimiter, it has no meaning in an APPEND and could cause trouble */
    if(mailbox && (delim = get_folder_delimiter(mailbox)) != '\0'){
	size_t len;

	len = strlen(mailbox);
	if(mailbox[len-1] == delim){
	    strncpy(mailbox_nodelim, mailbox, MIN(len-1,sizeof(mailbox_nodelim)-1));
	    mailbox_nodelim[MIN(len-1,sizeof(mailbox_nodelim)-1)] = '\0';
	    mailbox = mailbox_nodelim;
	}
    }

    if(check_for_move_mbox(mailbox, source, sizeof(source), &target)){
	mailbox = target;
	dprint((7,
	   "pine_mail_append_full: #move special case, appending to \"%s\"\n", 
		   mailbox ? mailbox : "(NULL)"));
    }

    /*
     * We don't really need this anymore, since we are now using IMAPTRYALT.
     */
    if((F_ON(F_PREFER_ALT_AUTH, ps_global)
       || (ps_global->debug_imap > 3 || ps_global->debugmem))){

	if((d = mail_valid (NIL, mailbox, (char *) NIL))
	   && !strcmp(d->name, "imap")){

	    if(F_ON(F_PREFER_ALT_AUTH, ps_global))
	      openflags |= OP_TRYALT;
	}
    }

    if(!stream)
      stream = sp_stream_get(mailbox, SP_MATCH);
    if(!stream)
      stream = sp_stream_get(mailbox, SP_SAME);

    if(!stream){
	/*
	 * It is only useful to open a stream in the imap case.
	 */
	if((d = mail_valid (NIL, mailbox, (char *) NIL))
	   && !strcmp(d->name, "imap")){

	    stream = pine_mail_open(NULL, mailbox, openflags, NULL);
	    ourstream = stream;
	}
    }

    return_val = mail_append_full(stream, mailbox, flags, date, message);

    if(ourstream)
      pine_mail_close(ourstream);

    return(return_val);
}


/*
 * Pine wrapper around mail_append.
 */
long
pine_mail_append_multiple(MAILSTREAM *stream, char *mailbox, append_t af,
			  APPENDPACKAGE *data, MAILSTREAM *not_this_stream)
{
    MAILSTREAM *ourstream = NULL;
    long        return_val;
    long openflags = (OP_HALFOPEN | OP_SILENT | SP_USEPOOL | SP_TEMPUSE);
    char        source[MAILTMPLEN], *target = NULL;
    DRIVER     *d;
    int         we_blocked_reuse = 0;

    dprint((7, "pine_mail_append_multiple: appending to \"%s\"%s\n", 
	       mailbox ? mailbox : "(NULL)",
	       stream ? "" : " (stream was NULL)"));

    if(check_for_move_mbox(mailbox, source, sizeof(source), &target)){
	mailbox = target;
	dprint((7,
         "pine_mail_append_multiple: #move special case, appending to \"%s\"\n",
		   mailbox ? mailbox : "(NULL)"));
    }

    if((F_ON(F_PREFER_ALT_AUTH, ps_global)
       || (ps_global->debug_imap > 3 || ps_global->debugmem))){

	if((d = mail_valid (NIL, mailbox, (char *) NIL))
	   && !strcmp(d->name, "imap")){

	    if(F_ON(F_PREFER_ALT_AUTH, ps_global))
	      openflags |= OP_TRYALT;
	}
    }

    /*
     * We have to be careful re-using streams for multiappend, because of
     * the way it works. We call into c-client below but part of the call
     * is data containing a callback function to us to supply the data to
     * be appended. That function may need to get the data from the server.
     * If that uses the same stream as we're trying to append on, we're
     * in trouble. We can't call back into c-client from c-client on the same
     * stream. (Just think about it, we're in the middle of an APPEND command.
     * We can't issue a FETCH before the APPEND completes in order to complete
     * the APPEND.) We can re-use a stream if it is a different stream from
     * the one we are reading from, so that's what the not_this_stream
     * stuff is for. If we mark it !SP_USEPOOL, it won't get reused.
     */
    if(sp_flagged(not_this_stream, SP_USEPOOL)){
	we_blocked_reuse++;
	sp_unflag(not_this_stream, SP_USEPOOL);
    }

    if(!stream)
      stream = sp_stream_get(mailbox, SP_MATCH);
    if(!stream)
      stream = sp_stream_get(mailbox, SP_SAME);

    if(!stream){
	/*
	 * It is only useful to open a stream in the imap case.
	 */
	if((d = mail_valid (NIL, mailbox, (char *) NIL))
	   && !strcmp(d->name, "imap")){

	    stream = pine_mail_open(NULL, mailbox, openflags, NULL);
	    ourstream = stream;
	}
    }

    if(we_blocked_reuse)
      sp_set_flags(not_this_stream, sp_flags(not_this_stream) | SP_USEPOOL);

    return_val = mail_append_multiple(stream, mailbox, af, (void *) data);

    if(ourstream)
      pine_mail_close(ourstream);

    return(return_val);
}


/*
 * Pine wrapper around mail_copy.
 */
long
pine_mail_copy_full(MAILSTREAM *stream, char *sequence, char *mailbox, long int options)
{
    MAILSTREAM *ourstream = NULL;
    long        return_val;
    long openflags = (OP_HALFOPEN | OP_SILENT | SP_USEPOOL | SP_TEMPUSE);
    char        source[MAILTMPLEN], *target = NULL;
    char        mailbox_nodelim[MAILTMPLEN];
    int         delim;
    DRIVER     *d;

    dprint((7, "pine_mail_copy_full: copying to \"%s\"%s\n", 
	       mailbox ? mailbox : "(NULL)",
	       stream ? "" : " (stream was NULL)"));

    /* strip delimiter, it has no meaning in a COPY and could cause trouble */
    if(mailbox && (delim = get_folder_delimiter(mailbox)) != '\0'){
	size_t len;

	len = strlen(mailbox);
	if(mailbox[len-1] == delim){
	    strncpy(mailbox_nodelim, mailbox, MIN(len-1,sizeof(mailbox_nodelim)-1));
	    mailbox_nodelim[MIN(len-1,sizeof(mailbox_nodelim)-1)] = '\0';
	    mailbox = mailbox_nodelim;
	}
    }

    if(check_for_move_mbox(mailbox, source, sizeof(source), &target)){
	mailbox = target;
	dprint((7,
	   "pine_mail_copy_full: #move special case, copying to \"%s\"\n", 
		   mailbox ? mailbox : "(NULL)"));
    }

    /*
     * We don't really need this anymore, since we are now using IMAPTRYALT.
     */
    if((F_ON(F_PREFER_ALT_AUTH, ps_global)
       || (ps_global->debug_imap > 3 || ps_global->debugmem))){

	if((d = mail_valid (NIL, mailbox, (char *) NIL))
	   && !strcmp(d->name, "imap")){

	    if(F_ON(F_PREFER_ALT_AUTH, ps_global))
	      openflags |= OP_TRYALT;
	}
    }

    if(!stream)
      stream = sp_stream_get(mailbox, SP_MATCH);
    if(!stream)
      stream = sp_stream_get(mailbox, SP_SAME);

    if(!stream){
	/*
	 * It is only useful to open a stream in the imap case.
	 * Actually, mail_copy_full is the case where it might also be
	 * useful to provide a stream in the nntp and pop3 cases. If we
	 * cache such streams, then we will probably want to open one
	 * here so that it gets cached.
	 */
	if((d = mail_valid (NIL, mailbox, (char *) NIL))
	   && !strcmp(d->name, "imap")){

	    stream = pine_mail_open(NULL, mailbox, openflags, NULL);
	    ourstream = stream;
	}
    }

    return_val = mail_copy_full(stream, sequence, mailbox, options);

    if(ourstream)
      pine_mail_close(ourstream);

    return(return_val);
}


/*
 * Pine wrapper around mail_rename.
 */
long
pine_mail_rename(MAILSTREAM *stream, char *old, char *new)
{
    MAILSTREAM *ourstream = NULL;
    long        return_val;
    long        openflags = (OP_HALFOPEN | OP_SILENT | SP_USEPOOL | SP_TEMPUSE);
    DRIVER     *d;

    dprint((7, "pine_mail_rename(%s,%s)\n", old ? old : "",
		new ? new : ""));

    /*
     * We don't really need this anymore, since we are now using IMAPTRYALT.
     */
    if((F_ON(F_PREFER_ALT_AUTH, ps_global)
       || (ps_global->debug_imap > 3 || ps_global->debugmem))){

	if((d = mail_valid (NIL, old, (char *) NIL))
	   && !strcmp(d->name, "imap")){

	    if(F_ON(F_PREFER_ALT_AUTH, ps_global))
	      openflags |= OP_TRYALT;
	}
    }

    /* oops, we seem to be renaming a selected stream */
    if(!stream && (stream = sp_stream_get(old, SP_MATCH))){
	pine_mail_actually_close(stream);
	stream = NULL;
    }

    if(!stream)
      stream = sp_stream_get(old, SP_SAME);

    if(!stream){
	/*
	 * It is only useful to open a stream in the imap case.
	 */
	if((d = mail_valid (NIL, old, (char *) NIL))
	   && !strcmp(d->name, "imap")){

	    stream = pine_mail_open(NULL, old, openflags, NULL);
	    ourstream = stream;
	}
    }

    return_val = mail_rename(stream, old, new);

    if(ourstream)
      pine_mail_close(ourstream);

    return(return_val);
}


/*----------------------------------------------------------------------
  Our mail_close wrapper to clean up anything on the mailstream we may have
  added to it.  mostly in the unused bits of the elt's.
  ----*/
void
pine_mail_close(MAILSTREAM *stream)
{
    unsigned long uid_last, last_uid;
    int refcnt;

    if(!stream)
      return;

    dprint((7, "pine_mail_close: %s (%s)\n", 
	       stream && stream->mailbox ? stream->mailbox : "(NULL)",
	       debug_time(1, ps_global->debug_timestamp)));

    if(sp_flagged(stream, SP_USEPOOL) && !sp_dead_stream(stream)){

	refcnt = sp_ref_cnt(stream);
	dprint((7, "pine_mail_close: ref cnt is %d\n", refcnt));

	/*
	 * Instead of checkpointing here, which takes time that the user
	 * definitely notices, we checkpoint in new_mail at the next
	 * opportune time, hopefully when the user is idle.
	 */
#if 0
	if(sp_flagged(stream, SP_LOCKED) && sp_flagged(stream, SP_USERFLDR)
	   && !stream->halfopen && refcnt <= 1){
	    if(changes_to_checkpoint(stream))
	      pine_mail_check(stream);
	    else{
	      dprint((7,
		     "pine_mail_close: dont think we need to checkpoint\n"));
	    }
	}
#endif

	/*
	 * Uid_last is valid when we first open a stream, but not always
	 * valid after that. So if we know the last uid should be higher
	 * than uid_last (!#%) use that instead.
	 */
	uid_last = stream->uid_last;
	if(stream->nmsgs > 0L
	   && (last_uid=mail_uid(stream,stream->nmsgs)) > uid_last)
	  uid_last = last_uid;

	sp_set_saved_uid_validity(stream, stream->uid_validity);
	sp_set_saved_uid_last(stream, uid_last);

	/*
	 * If the reference count is down to 0, unlock it.
	 * In any case, don't actually do any real closing.
	 */
	if(refcnt > 0)
	  sp_set_ref_cnt(stream, refcnt-1);

	refcnt = sp_ref_cnt(stream);
	dprint((7, "pine_mail_close: ref cnt is %d\n", refcnt));
	if(refcnt <= 0){
	    dprint((7,
	       "pine_mail_close: unlocking: start idle timer: TAG %08lx (%s)\n",
	       stream->gensym, debug_time(1, ps_global->debug_timestamp)));
	    sp_set_last_use_time(stream, time(0));

	    /*
	     * Logically, we ought to be unflagging SP_INBOX, too. However,
	     * the filtering code uses SP_INBOX when deciding if it should
	     * filter some things, and we keep filtering after the mailbox
	     * is closed. So leave SP_INBOX alone. This (the closing of INBOX)
	     * usually only happens in goodnight_gracey when we're
	     * shutting everything down.
	     */
	    sp_unflag(stream, SP_LOCKED | SP_PERMLOCKED | SP_USERFLDR);
	}
	else{
	    dprint((7, "pine_mail_close: ref cnt is now %d\n", 
		       refcnt));
	}
    }
    else{
	dprint((7, "pine_mail_close: %s\n", 
		   sp_flagged(stream, SP_USEPOOL) ? "dead stream" : "no pool"));

	refcnt = sp_ref_cnt(stream);
	dprint((7, "pine_mail_close: ref cnt is %d\n", refcnt));

	/*
	 * If the reference count is down to 0, unlock it.
	 * In any case, don't actually do any real closing.
	 */
	if(refcnt > 0)
	  sp_set_ref_cnt(stream, refcnt-1);

	refcnt = sp_ref_cnt(stream);
	if(refcnt <= 0){
	    pine_mail_actually_close(stream);
	}
	else{
	    dprint((7, "pine_mail_close: ref cnt is now %d\n", 
		       refcnt));
	}

    }
}


void
pine_mail_actually_close(MAILSTREAM *stream)
{
    if(!stream)
      return;

    if(!sp_closing(stream)){
	dprint((7, "pine_mail_actually_close: %s (%s)\n", 
		   stream && stream->mailbox ? stream->mailbox : "(NULL)",
		   debug_time(1, ps_global->debug_timestamp)));

	sp_set_closing(stream, 1);
	
	if(!sp_flagged(stream, SP_LOCKED)
	   && !sp_flagged(stream, SP_USERFLDR)
	   && !sp_dead_stream(stream)
	   && sp_new_mail_count(stream))
	  process_filter_patterns(stream, sp_msgmap(stream),
				  sp_new_mail_count(stream));
	sp_delete(stream);

	/*
	 * let sp_free_callback() free the sp_s stuff and the callbacks to
	 * free_pine_elt free the per-elt pine stuff.
	 */
	mail_close(stream);
    }
}


/*
 * If we haven't used a stream for a while, we may want to logout.
 */
void
maybe_kill_old_stream(MAILSTREAM *stream)
{
#define KILL_IF_IDLE_TIME (25 * 60)
    if(stream
       && !sp_flagged(stream, SP_LOCKED)
       && !sp_flagged(stream, SP_USERFLDR)
       && time(0) - sp_last_use_time(stream) > KILL_IF_IDLE_TIME){

	dprint((7,
		"killing idle stream: %s (%s): idle timer = %ld secs\n", 
		stream && stream->mailbox ? stream->mailbox : "(NULL)",
		debug_time(1, ps_global->debug_timestamp),
		(long) (time(0)-sp_last_use_time(stream))));

	/*
	 * Another thing we could do here instead is to unselect the
	 * mailbox, leaving the stream open to the server.
	 */
	pine_mail_actually_close(stream);
    }
}


/*
 * Catch searches that don't need to go to the server.
 * (Not anymore, now c-client does this for us.)
 */
long
pine_mail_search_full(MAILSTREAM *stream, char *charset,
		      SEARCHPGM *pgm, long int flags)
{
    /*
     * The charset should either be UTF-8 or NULL for alpine.
     * If it is UTF-8 we may be able to change it to NULL if
     * everything in the search is actually ascii. We try to
     * do this because not all servers understand the CHARSET
     * parameter.
     */
    if(charset && !strucmp(charset, "utf-8")
       && is_imap_stream(stream) && !hibit_in_searchpgm(pgm))
      charset = NULL;

    if(F_ON(F_NNTP_SEARCH_USES_OVERVIEW, ps_global))
      flags |= SO_OVERVIEW;

    return(stream ? mail_search_full(stream, charset, pgm, flags) : NIL);
}


int
hibit_in_searchpgm(SEARCHPGM *pgm)
{
    SEARCHOR *or;
    SEARCHPGMLIST *not;

    if(!pgm)
      return 0;

    if((pgm->subject && hibit_in_strlist(pgm->subject))
       || (pgm->text && hibit_in_strlist(pgm->text))
       || (pgm->body && hibit_in_strlist(pgm->body))
       || (pgm->cc && hibit_in_strlist(pgm->cc))
       || (pgm->from && hibit_in_strlist(pgm->from))
       || (pgm->to && hibit_in_strlist(pgm->to))
       || (pgm->bcc && hibit_in_strlist(pgm->bcc))
       || (pgm->keyword && hibit_in_strlist(pgm->keyword))
       || (pgm->unkeyword && hibit_in_strlist(pgm->return_path))
       || (pgm->sender && hibit_in_strlist(pgm->sender))
       || (pgm->reply_to && hibit_in_strlist(pgm->reply_to))
       || (pgm->in_reply_to && hibit_in_strlist(pgm->in_reply_to))
       || (pgm->message_id && hibit_in_strlist(pgm->message_id))
       || (pgm->newsgroups && hibit_in_strlist(pgm->newsgroups))
       || (pgm->followup_to && hibit_in_strlist(pgm->followup_to))
       || (pgm->references && hibit_in_strlist(pgm->references))
       || (pgm->header && hibit_in_header(pgm->header)))
      return 1;

    for(or = pgm->or; or; or = or->next)
      if(hibit_in_searchpgm(or->first) || hibit_in_searchpgm(or->second))
        return 1;

    for(not = pgm->not; not; not = not->next)
      if(hibit_in_searchpgm(not->pgm))
        return 1;

    return 0;
}


int
hibit_in_strlist(STRINGLIST *sl)
{
    for(; sl; sl = sl->next)
      if(hibit_in_sizedtext(&sl->text))
        return 1;

    return 0;
}


int
hibit_in_header(SEARCHHEADER *header)
{
    SEARCHHEADER *hdr;

    for(hdr = header; hdr; hdr = hdr->next)
      if(hibit_in_sizedtext(&hdr->line) || hibit_in_sizedtext(&hdr->text))
        return 1;

    return 0;
}


int
hibit_in_sizedtext(SIZEDTEXT *st)
{
    unsigned char *p, *end;

    p = st ? st->data : NULL;
    if(p)
      for(end = p + st->size; p < end; p++)
        if(*p & 0x80)
	  return 1;

    return 0;
}


void
pine_mail_fetch_flags(MAILSTREAM *stream, char *sequence, long int flags)
{
    ps_global->dont_count_flagchanges = 1;
    mail_fetch_flags(stream, sequence, flags);
    ps_global->dont_count_flagchanges = 0;
}


ENVELOPE *
pine_mail_fetchenvelope(MAILSTREAM *stream, long unsigned int msgno)
{
    ENVELOPE *env = NULL;

    ps_global->dont_count_flagchanges = 1;
    if(stream && msgno > 0L && msgno <= stream->nmsgs)
      env = mail_fetchenvelope(stream, msgno);

    ps_global->dont_count_flagchanges = 0;
    return(env);
}


ENVELOPE *
pine_mail_fetch_structure(MAILSTREAM *stream, long unsigned int msgno,
			  struct mail_bodystruct **body, long int flags)
{
    ENVELOPE *env = NULL;

    ps_global->dont_count_flagchanges = 1;
    if(stream && (flags & FT_UID || (msgno > 0L && msgno <= stream->nmsgs)))
      env = mail_fetch_structure(stream, msgno, body, flags);

    ps_global->dont_count_flagchanges = 0;
    return(env);
}


ENVELOPE *
pine_mail_fetchstructure(MAILSTREAM *stream, long unsigned int msgno, struct mail_bodystruct **body)
{
    ENVELOPE *env = NULL;

    ps_global->dont_count_flagchanges = 1;
    if(stream && msgno > 0L && msgno <= stream->nmsgs)
      env = mail_fetchstructure(stream, msgno, body);

    ps_global->dont_count_flagchanges = 0;
    return(env);
}


/*
 * Wrapper around mail_fetch_body.
 * Currently only used to turn on partial fetching if trying
 * to work around the microsoft ssl bug.
 */
char *
pine_mail_fetch_body(MAILSTREAM *stream, long unsigned int msgno, char *section,
		     long unsigned int *len, long int flags)
{
#ifdef _WINDOWS
    if(F_ON(F_QUELL_SSL_LARGEBLOCKS, ps_global))
      return(pine_mail_partial_fetch_wrapper(stream, msgno, 
			     section, len, flags, 0, NULL, 0));
    else
#endif
    return(mail_fetch_body(stream, msgno, section, len, flags));
}

/*
 * Wrapper around mail_fetch_text.
 * Currently the only purpose this wrapper serves is to turn
 * on partial fetching for quell-ssl-largeblocks.
 */
char *
pine_mail_fetch_text(MAILSTREAM *stream, long unsigned int msgno, char *section,
		     long unsigned int *len, long int flags)
{
#ifdef _WINDOWS
    if(F_ON(F_QUELL_SSL_LARGEBLOCKS, ps_global))
      return(pine_mail_partial_fetch_wrapper(stream, msgno,
					     section, len, flags, 
					     0, NULL, 1));
    else
#endif
      return(mail_fetch_text(stream, msgno, section, len, flags));
}


/*
 * Determine whether to do partial-fetching or not, and do it
 *    args - same as c-client functions being wrapped around
 *    get_n_bytes - try to partial fetch for the first n bytes.
 *                  makes no guarantees, might wind up fetching
 *                  the entire text anyway.
 *    str_to_free - pointer to string to free if we only get
 *                  (non-cachable) partial text (required for
 *                  get_n_bytes).
 *    is_text_fetch - 
 *      set, tells us to do mail_fetch_text and mail_partial_text
 *      unset, tells us to do mail_fetch_body and mail_partial_body
 */
char *
pine_mail_partial_fetch_wrapper(MAILSTREAM *stream, long unsigned int msgno,
				char *section, long unsigned int *len,
				long int flags, long unsigned int get_n_bytes,
				char **str_to_free, int is_text_fetch)
{
    BODY *body;
    unsigned long size, firstbyte, lastbyte;
    void *old_gets;
    FETCH_READC_S *pftc;
    char imap_cache_section[MAILTMPLEN];
    SIZEDTEXT new_text;
    MESSAGECACHE *mc;
    char *(*fetch_full)(MAILSTREAM *, unsigned long, char *,
		       unsigned long *, long);
    long (*fetch_partial)(MAILSTREAM *, unsigned long, char *,
			   unsigned long, unsigned long, long);

    fetch_full = is_text_fetch ? mail_fetch_text : mail_fetch_body;
    fetch_partial = is_text_fetch ? mail_partial_text : mail_partial_body;
    if(str_to_free)
      *str_to_free = NULL;
#ifdef _WINDOWS
    if(F_ON(F_QUELL_SSL_LARGEBLOCKS, ps_global) || get_n_bytes){
#else
    if(get_n_bytes){
#endif /* _WINDOWS */
	if(section && *section)
	  body = mail_body(stream, msgno, (unsigned char *) section);
	else
	  pine_mail_fetch_structure(stream, msgno, &body, flags);
	if(!body)
	  return NULL;
	if(body->type != TYPEMULTIPART)
	  size = body->size.bytes;
	else if((!section || !*section) && msgno > 0L
	        && stream && msgno <= stream->nmsgs
		&& (mc = mail_elt(stream, msgno)))
	  size = mc->rfc822_size; /* upper bound */
	else      /* just a guess, can't get actual size */
	  size = fcc_size_guess(body) + 2048;

	/* 
	 * imap_cache, originally intended for c-client internal use,
	 * takes a section argument that is different from one we
	 * would pass to mail_body.  Typically in this function
	 * section is NULL, which translates to "TEXT", but in other
	 * cases we would want to append ".TEXT" to the section
	 */
	if(is_text_fetch)
	  snprintf(imap_cache_section, sizeof(imap_cache_section), "%.*s%sTEXT", MAILTMPLEN - 10,
		  section && *section ? section : "",
		  section && *section ? "." : "");
	else
	  snprintf(imap_cache_section, sizeof(imap_cache_section), "%.*s", MAILTMPLEN - 10,
		  section && *section ? section : "");

	if(modern_imap_stream(stream)
#ifdef _WINDOWS
	   && ((size > AVOID_MICROSOFT_SSL_CHUNKING_BUG)
	       || (get_n_bytes && size > get_n_bytes))
#else
	   && (get_n_bytes && size > get_n_bytes)
#endif /* _WINDOWS */
	   && !imap_cache(stream, msgno, imap_cache_section,
			  NULL, NULL)){
	    if(get_n_bytes == 0){
	      dprint((8, 
    "fetch_wrapper: doing partial fetching to work around microsoft bug\n"));
	    }
	    else{
	      dprint((8,
    "fetch_wrapper: partial fetching due to %lu get_n_bytes\n", get_n_bytes));
	    }
	    pftc = (FETCH_READC_S *)fs_get(sizeof(FETCH_READC_S));
	    memset(g_pft_desc = pftc, 0, sizeof(FETCH_READC_S));
#ifdef _WINDOWS
	    if(F_ON(F_QUELL_SSL_LARGEBLOCKS, ps_global)){
		if(get_n_bytes)
		  pftc->chunksize = MIN(get_n_bytes,
					AVOID_MICROSOFT_SSL_CHUNKING_BUG);
		else
		  pftc->chunksize = AVOID_MICROSOFT_SSL_CHUNKING_BUG;
	    }
	    else
#endif /* _WINDOWS */
	    pftc->chunksize = get_n_bytes;

	    pftc->chunk = (char *) fs_get((pftc->chunksize+1)
					  * sizeof(char));
	    pftc->cache = so_get(CharStar, NULL, EDIT_ACCESS);
	    pftc->read = 0L;
	    so_truncate(pftc->cache, size + 1);
	    old_gets = mail_parameters(stream, GET_GETS, (void *)NULL);
	    mail_parameters(stream, SET_GETS, (void *) partial_text_gets);
	    /* start fetching */
	    do{
		firstbyte = pftc->read ;
		lastbyte = firstbyte + pftc->chunksize;
		if(get_n_bytes > firstbyte && get_n_bytes < lastbyte){
		    pftc->chunksize = get_n_bytes - firstbyte;
		    lastbyte = get_n_bytes;
		}
		(*fetch_partial)(stream, msgno, section, firstbyte,
				 pftc->chunksize, flags);

		if(pftc->read != lastbyte)
		  break;
	    } while((pftc->read ==  lastbyte)
		    && (!get_n_bytes || (pftc->read < get_n_bytes)));
	    dprint((8,
		       "fetch_wrapper: anticipated size=%lu read=%lu\n",
		       size, pftc->read));
	    mail_parameters(stream, SET_GETS, old_gets);
	    new_text.size = pftc->read;
	    new_text.data = (unsigned char *)so_text(pftc->cache);

	    if(get_n_bytes && pftc->read == get_n_bytes){
		/* 
		 * don't write to cache if we're only dealing with
		 * partial text.
		 */
		if(!str_to_free)
		  panic("Programmer botch: partial fetch attempt w/o string pointer");
		else
		  *str_to_free = (char *) new_text.data;
	    }
	    else {
		/* ugh, rewrite string in case it got stomped on last call */
		if(is_text_fetch)
		  snprintf(imap_cache_section, sizeof(imap_cache_section), "%.*s%sTEXT", MAILTMPLEN - 10,
			  section && *section ? section : "",
			  section && *section ? "." : "");
		else
		  snprintf(imap_cache_section, sizeof(imap_cache_section), "%.*s", MAILTMPLEN - 10,
			  section && *section ? section : "");

		imap_cache(stream, msgno, imap_cache_section, NULL, &new_text);
	    }

	    pftc->cache->txt = (void *)NULL;
	    so_give(&pftc->cache);
	    fs_give((void **)&pftc->chunk);
	    fs_give((void **)&pftc);
	    g_pft_desc = NULL;
	    if(len)
	      *len = new_text.size;
	    return ((char *)new_text.data);
	}
	else
	  return((*fetch_full)(stream, msgno, section, len, flags));
    }
    else
      return((*fetch_full)(stream, msgno, section, len, flags));
}

/*
 * c-client callback that handles getting the text
 */
char *
partial_text_gets(readfn_t f, void *stream, long unsigned int size, GETS_DATA *md)
{
    unsigned long n;

    n = MIN(g_pft_desc->chunksize, size);
    g_pft_desc->read +=n;

    (*f) (stream, n, g_pft_desc->chunk);

    if(g_pft_desc->cache)
      so_nputs(g_pft_desc->cache, g_pft_desc->chunk, (long) n);


    return(NULL);
}


/*
 * Pings the stream. Returns 0 if the stream is dead, non-zero otherwise.
 */
long
pine_mail_ping(MAILSTREAM *stream)
{
    time_t now;
    long   ret = 0L;

    if(!sp_dead_stream(stream)){
	ret = mail_ping(stream);
	if(ret && sp_dead_stream(stream))
	  ret = 0L;
    }

    if(ret){
	now = time(0);
	sp_set_last_ping(stream, now);
	sp_set_last_expunged_reaper(stream, now);
    }

    return(ret);
}


void
pine_mail_check(MAILSTREAM *stream)
{
    reset_check_point(stream);
    mail_check(stream);
}


/*
 * Unlike mail_list, this version returns a value. The returned value is
 * TRUE if the stream is opened ok, and FALSE if we failed opening the
 * stream. This allows us to differentiate between a LIST which returns
 * no matches and a failure opening the stream. We do this by pre-opening
 * the stream ourselves.
 */
int
pine_mail_list(MAILSTREAM *stream, char *ref, char *pat, unsigned int *options)
{
    int   we_open = 0;
    char *halfopen_target;
    char  source[MAILTMPLEN], *target = NULL;
    MAILSTREAM *ourstream = NULL;

    dprint((7, "pine_mail_list: ref=%s pat=%s%s\n", 
	       ref ? ref : "(NULL)",
	       pat ? pat : "(NULL)",
	       stream ? "" : " (stream was NULL)"));

    if((!ref && check_for_move_mbox(pat, source, sizeof(source), &target))
       ||
       check_for_move_mbox(ref, source, sizeof(source), &target)){
	ref = NIL;
	pat = target;
	if(options)
	  *options |= PML_IS_MOVE_MBOX;

	dprint((7,
		   "pine_mail_list: #move special case, listing \"%s\"%s\n", 
		   pat ? pat : "(NULL)",
		   stream ? "" : " (stream was NULL)"));
    }

    if(!stream && ((pat && *pat == '{') || (ref && *ref == '{'))){
	we_open++;
	if(pat && *pat == '{'){
	    ref = NIL;
	    halfopen_target = pat;
	}
	else
	  halfopen_target = ref;
    }

    if(we_open){
	long flags = (OP_HALFOPEN | OP_SILENT | SP_USEPOOL | SP_TEMPUSE);

	stream = sp_stream_get(halfopen_target, SP_MATCH);
	if(!stream)
	  stream = sp_stream_get(halfopen_target, SP_SAME);

	if(!stream){
	    DRIVER *d;

	    /*
	     * POP is a special case. We don't need to have a stream
	     * to call mail_list for a POP name. The else part is the
	     * POP part.
	     */
	    if((d = mail_valid(NIL, halfopen_target, (char *) NIL))
	       && (d->flags & DR_HALFOPEN)){
		stream = pine_mail_open(NIL, halfopen_target, flags, NULL);
		ourstream = stream;
		if(!stream)
		  return(FALSE);
	    }
	    else
	      stream = NULL;
	}

	mail_list_internal(stream, ref, pat);
    }
    else
      mail_list_internal(stream, ref, pat);
    
    if(ourstream)
      pine_mail_close(ourstream);

    return(TRUE);
}


/*
 * mail_list_internal -- A monument to software religion and those who
 *			 would force it upon us.
 */
void
mail_list_internal(MAILSTREAM *s, char *r, char *p)
{
    if(F_ON(F_FIX_BROKEN_LIST, ps_global)
       && ((s && s->mailbox && *s->mailbox == '{')
	   || (!s && ((r && *r == '{') || (p && *p == '{'))))){
	char tmp[2*MAILTMPLEN];

	snprintf(tmp, sizeof(tmp), "%.*s%.*s", sizeof(tmp)/2-1, r ? r : "",
		sizeof(tmp)/2-1, p);
	mail_list(s, "", tmp);
    }
    else
      mail_list(s, r, p);
}


long
pine_mail_status(MAILSTREAM *stream, char *mailbox, long int flags)
{
    return(pine_mail_status_full(stream, mailbox, flags, NULL, NULL));
}


long
pine_mail_status_full(MAILSTREAM *stream, char *mailbox, long int flags,
		      imapuid_t *uidvalidity, imapuid_t *uidnext)
{
    char        source[MAILTMPLEN], *target = NULL;
    long        ret = NIL;
    MAILSTATUS  cache, status;
    MAILSTREAM *ourstream = NULL;

    if(check_for_move_mbox(mailbox, source, sizeof(source), &target)){
	memset(&status, 0, sizeof(status));
	memset(&cache,  0, sizeof(cache));

	/* tell mm_status() to write partial return here */
	pine_cached_status = &status;

	flags |= (SA_UIDVALIDITY | SA_UIDNEXT | SA_MESSAGES);

	/* do status of destination */

	stream = sp_stream_get(target, SP_SAME);

	/* should never be news, don't worry about mulnewrsc flag*/
	if((ret = pine_mail_status_full(stream, target, flags, uidvalidity,
					uidnext))
	   && !status.recent){

	    /* do status of source */
	    pine_cached_status = &cache;
	    stream = sp_stream_get(source, SP_SAME);

	    if(!stream){
		DRIVER *d;

		if((d = mail_valid (NIL, source, (char *) NIL))
		   && !strcmp(d->name, "imap")){
		    long openflags =OP_HALFOPEN|OP_SILENT|SP_USEPOOL|SP_TEMPUSE;

		    if(F_ON(F_PREFER_ALT_AUTH, ps_global))
		      openflags |= OP_TRYALT;

		    stream = pine_mail_open(NULL, source, openflags, NULL);
		    ourstream = stream;
		}
		else if(F_ON(F_ENABLE_MULNEWSRCS, ps_global)
			&& d && (!strucmp(d->name, "news")
				 || !strucmp(d->name, "nntp")))
		  flags |= SA_MULNEWSRC;

	    }

	    if(!ps_global->user_says_cancel && mail_status(stream, source, flags)){
		DRIVER *d;
		int     is_news = 0;

		/*
		 * If the target has recent messages, then we don't come
		 * through here. We just use the answer from the target.
		 *
		 * If not, then we leave the target results in "status" and
		 * run a mail_status on the source that puts its results
		 * in "cache". Combine the results from cache with the
		 * results that were already in status.
		 *
		 * We count all messages in the source mailbox as recent and
		 * unseen, even if they are not recent or unseen in the source,
		 * because they will be recent and unseen in the target
		 * when we open it. (Not quite true. It is possible that some
		 * messages from a POP server will end up seen instead
		 * of unseen.
		 * It is also possible that it is correct. If we add unseen, or
		 * if we add messages, we could get it wrong. As far as I
		 * can tell, Pine doesn't ever even use status.unseen, so it
		 * is currently academic anyway.)  Hubert 2003-03-05
		 * (Does now 2004-06-02 in next_folder.)
		 *
		 * However, we don't want to count all messages as recent if
		 * the source mailbox is NNTP or NEWS, because we won't be
		 * deleting those messages from the source.
		 * We only count recent.
		 *
		 * There are other cases that are trouble. One in particular
		 * is an IMAP-to-NNTP proxy, where the messages can't be removed
		 * from the mailbox but they can be deleted. If we count
		 * messages in the source as being recent and it turns out they
		 * were all deleted already, then we incorrectly say the folder
		 * has recent messages when it doesn't. We can recover from that
		 * case at some cost by actually opening the folder the first
		 * time if there are not recents, and then checking to see if
		 * everything is deleted. Subsequently, we store the uid values
		 * (which are returned by status) so that we can know if the
		 * mailbox changed or not. The problem being solved is that
		 * the TAB command indicates new messages in a folder when there
		 * really aren't any. An alternative would be to use the is_news
		 * half of the if-else in all cases. A problem with that is
		 * that there could be non-recent messages sitting in the
		 * source mailbox that we never discover. Hubert 2003-03-28
		 */

		if((d = mail_valid (NIL, source, (char *) NIL))
		   && (!strcmp(d->name, "nntp") || !strcmp(d->name, "news")))
		  is_news++;

		if(is_news && cache.flags & SA_RECENT){
		    status.messages += cache.recent;
		    status.recent   += cache.recent;
		    status.unseen   += cache.recent;
		    status.uidnext  += cache.recent;
		}
		else{
		    if(uidvalidity && *uidvalidity
		       && uidnext && *uidnext
		       && cache.flags & SA_UIDVALIDITY
		       && cache.uidvalidity == *uidvalidity
		       && cache.flags & SA_UIDNEXT
		       && cache.uidnext == *uidnext){
			; /* nothing changed in source mailbox */
		    }
		    else if(cache.flags & SA_RECENT && cache.recent){
			status.messages += cache.recent;
			status.recent   += cache.recent;
			status.unseen   += cache.recent;
			status.uidnext  += cache.recent;
		    }
		    else if(!(cache.flags & SA_MESSAGES) || cache.messages){
			long openflags = OP_SILENT | SP_USEPOOL | SP_TEMPUSE;
			long delete_count, not_deleted = 0L;

			/* Actually open it up and check */
			if(F_ON(F_PREFER_ALT_AUTH, ps_global))
			  openflags |= OP_TRYALT;

			if(!ourstream)
			  stream = NULL;

			if(ourstream
			   && !same_stream_and_mailbox(source, ourstream)){
			    pine_mail_close(ourstream);
			    ourstream = stream = NULL;
			}

			if(!stream){
			    stream = pine_mail_open(stream, source, openflags,
						    NULL);
			    ourstream = stream;
			}

			if(stream){
			    delete_count = count_flagged(stream, F_DEL);
			    not_deleted = stream->nmsgs - delete_count;
			}

			status.messages += not_deleted;
			status.recent   += not_deleted;
			status.unseen   += not_deleted;
			status.uidnext  += not_deleted;
		    }

		    if(uidvalidity && cache.flags & SA_UIDVALIDITY)
		      *uidvalidity = cache.uidvalidity;

		    if(uidnext && cache.flags & SA_UIDNEXT)
		      *uidnext = cache.uidnext;
		}
	    }
	}

	/*
	 * Do the regular mm_status callback which we've been intercepting
	 * into different locations above.
	 */
	pine_cached_status = NIL;
	if(ret)
	  mm_status(NULL, mailbox, &status);
    }
    else{
	if(!stream){
	    DRIVER *d;

	    if((d = mail_valid (NIL, mailbox, (char *) NIL))
	       && !strcmp(d->name, "imap")){
		long openflags = OP_HALFOPEN|OP_SILENT|SP_USEPOOL|SP_TEMPUSE;

		if(F_ON(F_PREFER_ALT_AUTH, ps_global))
		  openflags |= OP_TRYALT;

		/*
		 * We just use this to find the answer.
		 * We're asking for trouble if we do a STATUS on a
		 * selected mailbox. I don't believe this happens in pine.
		 * It does now (2004-06-02) in next_folder if the
		 * F_TAB_USES_UNSEEN option is set and the folder was
		 * already opened.
		 */
		stream = sp_stream_get(mailbox, SP_MATCH);
		if(stream){ 
		    memset(&status, 0, sizeof(status));
		    if(flags & SA_MESSAGES){
			status.flags |= SA_MESSAGES;
			status.messages = stream->nmsgs;
		    }

		    if(flags & SA_RECENT){
			status.flags |= SA_RECENT;
			status.recent = stream->recent;
		    }

		    if(flags & SA_UNSEEN){
		        long i;
			SEARCHPGM *srchpgm;
			MESSAGECACHE *mc;

			srchpgm = mail_newsearchpgm();
			srchpgm->unseen = 1;
		      
			pine_mail_search_full(stream, NULL, srchpgm,
					      SE_NOPREFETCH | SE_FREE);
			status.flags |= SA_UNSEEN;
			status.unseen = 0L;
			for(i = 1L; i <= stream->nmsgs; i++)
			  if((mc = mail_elt(stream, i)) && mc->searched)
			    status.unseen++;
		    }

		    if(flags & SA_UIDVALIDITY){
			status.flags |= SA_UIDVALIDITY;
			status.uidvalidity = stream->uid_validity;
		    }

		    if(flags & SA_UIDNEXT){
			status.flags |= SA_UIDNEXT;
			status.uidnext = stream->uid_last + 1L;
		    }

		    mm_status(NULL, mailbox, &status);
		    return T;  /* that's what c-client returns when success */
		}

		if(!stream)
		  stream = sp_stream_get(mailbox, SP_SAME);

		if(!stream){
		    stream = pine_mail_open(NULL, mailbox, openflags, NULL);
		    ourstream = stream;
		}
	    }
	    else if(F_ON(F_ENABLE_MULNEWSRCS, ps_global)
		    && d && (!strucmp(d->name, "news")
			     || !strucmp(d->name, "nntp")))
	      flags |= SA_MULNEWSRC;
	}

	if(!ps_global->user_says_cancel)
	  ret = mail_status(stream, mailbox, flags);	/* non #move case */
    }

    if(ourstream)
      pine_mail_close(ourstream);

    return ret;
}


/*
 * Check for a mailbox name that is a legitimate #move mailbox.
 *
 * Args   mbox     -- The mailbox name to check
 *      sourcebuf  -- Copy the source mailbox name into this buffer
 *      sbuflen    -- Length of sourcebuf
 *      targetptr  -- Set the pointer this points to to point to the
 *                      target mailbox name in the original string
 *
 * Returns  1 - is a #move mailbox
 *          0 - not
 */
int
check_for_move_mbox(char *mbox, char *sourcebuf, size_t sbuflen, char **targetptr)
{
    char delim, *s;
    int  i;

    if(mbox && (mbox[0] == '#')
       && ((mbox[1] == 'M') || (mbox[1] == 'm'))
       && ((mbox[2] == 'O') || (mbox[2] == 'o'))
       && ((mbox[3] == 'V') || (mbox[3] == 'v'))
       && ((mbox[4] == 'E') || (mbox[4] == 'e'))
       && (delim = mbox[5])
       && (s = strchr(mbox+6, delim))
       && (i = s++ - (mbox + 6))
       && (!sourcebuf || i < sbuflen)){

	if(sourcebuf){
	    strncpy(sourcebuf, mbox+6, i);	/* copy source mailbox name */
	    sourcebuf[i] = '\0';
	}

	if(targetptr)
	  *targetptr = s;
	
	return 1;
    }

    return 0;
}


/*
 * Checks through stream cache for a stream pointer already open to
 * this mailbox, read/write. Very similar to sp_stream_get, but we want
 * to look at all streams, not just imap streams.
 * Right now it is very specialized. If we want to use it more generally,
 * generalize it or combine it with sp_stream_get somehow.
 */
MAILSTREAM *
already_open_stream(char *mailbox, int flags)
{
    int         i;
    MAILSTREAM *m;

    if(!mailbox)
      return(NULL);

    if(*mailbox == '{'){
	for(i = 0; i < ps_global->s_pool.nstream; i++){
	    m = ps_global->s_pool.streams[i];
	    if(m && !(flags & AOS_RW_ONLY && m->rdonly)
	       && (*m->mailbox == '{') && !sp_dead_stream(m)
	       && same_stream_and_mailbox(mailbox, m))
	      return(m);
	}
    }
    else{
	char *cn, tmp[MAILTMPLEN];

	cn = mailboxfile(tmp, mailbox);
	for(i = 0; i < ps_global->s_pool.nstream; i++){
	    m = ps_global->s_pool.streams[i];
	    if(m && !(flags & AOS_RW_ONLY && m->rdonly)
	       && (*m->mailbox != '{') && !sp_dead_stream(m)
	       && ((cn && *cn && !strcmp(cn, m->mailbox))
	           || !strcmp(mailbox, m->original_mailbox)
		   || !strcmp(mailbox, m->mailbox)))
	      return(m);
	}
    }
    
    return(NULL);
}


void
pine_imap_cmd_happened(MAILSTREAM *stream, char *cmd, long int flags)
{
    dprint((9, "imap_cmd(%s, %s, 0x%lx)\n",
	    STREAMNAME(stream), cmd ? cmd : "?", flags));

    if(cmd && !strucmp(cmd, "CHECK"))
      reset_check_point(stream);

    if(is_imap_stream(stream)){
	time_t now;

	now = time(0);
	sp_set_last_ping(stream, now);
	sp_set_last_activity(stream, now);
	if(!(flags & SC_EXPUNGEDEFERRED))
	  sp_set_last_expunged_reaper(stream, now);
    }
}


/*
 * Tells us whether we ought to check for a dead stream or not.
 * We assume that we ought to check if it is not IMAP and if it is IMAP we
 * don't need to check if the last activity was within the last 5 minutes.
 */
int
recent_activity(MAILSTREAM *stream)
{
    if(is_imap_stream(stream) && !sp_dead_stream(stream)
       && (time(0) - sp_last_activity(stream) < 5L * 60L))
      return 1;
    else
      return 0;
}

void
sp_cleanup_dead_streams(void)
{
    int         i;
    MAILSTREAM *m;

    (void) streams_died();	/* tell user in case they don't know yet */

    for(i = 0; i < ps_global->s_pool.nstream; i++){
	m = ps_global->s_pool.streams[i];
	if(m && sp_dead_stream(m))
	  pine_mail_close(m);
    }
}


/*
 * Returns 0 if stream flags not set, non-zero if they are.
 */
int
sp_flagged(MAILSTREAM *stream, long unsigned int flags)
{
    return(sp_flags(stream) & flags);
}


void
sp_set_fldr(MAILSTREAM *stream, char *folder)
{
    PER_STREAM_S **pss;

    pss = sp_data(stream);
    if(pss && *pss){
	if((*pss)->fldr)
	  fs_give((void **) &(*pss)->fldr);
	
	if(folder)
	  (*pss)->fldr = cpystr(folder);
    }
}


void
sp_set_saved_cur_msg_id(MAILSTREAM *stream, char *id)
{
    PER_STREAM_S **pss;

    pss = sp_data(stream);
    if(pss && *pss){
	if((*pss)->saved_cur_msg_id)
	  fs_give((void **) &(*pss)->saved_cur_msg_id);
	
	if(id)
	  (*pss)->saved_cur_msg_id = cpystr(id);
    }
}


/*
 * Sets flags absolutely, erasing old flags.
 */
void
sp_flag(MAILSTREAM *stream, long unsigned int flags)
{
    if(!stream)
      return;

    dprint((9, "sp_flag(%s, 0x%x): %s%s%s%s%s%s%s%s\n",
	    (stream && stream->mailbox) ? stream->mailbox : "?",
	    flags,
	    flags ? "set" : "clear",
	    (flags & SP_LOCKED)     ? " SP_LOCKED" : "",
	    (flags & SP_PERMLOCKED) ? " SP_PERMLOCKED" : "",
	    (flags & SP_INBOX)      ? " SP_INBOX"      : "",
	    (flags & SP_USERFLDR)   ? " SP_USERFLDR"   : "",
	    (flags & SP_USEPOOL)    ? " SP_USEPOOL"    : "",
	    (flags & SP_TEMPUSE)    ? " SP_TEMPUSE"    : "",
	    !flags                  ? " ALL" : ""));

    sp_set_flags(stream, flags);
}


/*
 * Clear individual stream flags.
 */
void
sp_unflag(MAILSTREAM *stream, long unsigned int flags)
{
    if(!stream || !flags)
      return;

    dprint((9, "sp_unflag(%s, 0x%x): unset%s%s%s%s%s%s\n",
	    (stream && stream->mailbox) ? stream->mailbox : "?",
	    flags,
	    (flags & SP_LOCKED)     ? " SP_LOCKED" : "",
	    (flags & SP_PERMLOCKED) ? " SP_PERMLOCKED" : "",
	    (flags & SP_INBOX)      ? " SP_INBOX"      : "",
	    (flags & SP_USERFLDR)   ? " SP_USERFLDR"   : "",
	    (flags & SP_USEPOOL)    ? " SP_USEPOOL"    : "",
	    (flags & SP_TEMPUSE)    ? " SP_TEMPUSE"    : ""));

    sp_set_flags(stream, sp_flags(stream) & ~flags);

    flags = sp_flags(stream);
    dprint((9, "sp_unflag(%s, 0x%x): result:%s%s%s%s%s%s\n",
	    (stream && stream->mailbox) ? stream->mailbox : "?",
	    flags,
	    (flags & SP_LOCKED)     ? " SP_LOCKED" : "",
	    (flags & SP_PERMLOCKED) ? " SP_PERMLOCKED" : "",
	    (flags & SP_INBOX)      ? " SP_INBOX"      : "",
	    (flags & SP_USERFLDR)   ? " SP_USERFLDR"   : "",
	    (flags & SP_USEPOOL)    ? " SP_USEPOOL"    : "",
	    (flags & SP_TEMPUSE)    ? " SP_TEMPUSE"    : ""));
}


/*
 * Set dead stream indicator and close if not locked.
 */
void
sp_mark_stream_dead(MAILSTREAM *stream)
{
    if(!stream)
      return;

    dprint((9, "sp_mark_stream_dead(%s)\n",
	    (stream && stream->mailbox) ? stream->mailbox : "?"));

    /*
     * If the stream isn't locked, it is no longer useful. Get rid of it.
     */
    if(!sp_flagged(stream, SP_LOCKED))
      pine_mail_actually_close(stream);
    else{
	/*
	 * If it is locked, then we have to worry about references to it
	 * that still exist. For example, it might be a permlocked stream
	 * or it might be the current stream. We need to let it be discovered
	 * by those referencers instead of just eliminating it, so that they
	 * can clean up the mess they need to clean up.
	 */
	sp_set_dead_stream(stream, 1);
    }
}


/*
 * Returns the number of streams in the stream pool which are
 * SP_USEPOOL but not SP_PERMLOCKED.
 */
int
sp_nusepool_notperm(void)
{
    int         i, cnt = 0;
    MAILSTREAM *m;

    for(i = 0; i < ps_global->s_pool.nstream; i++){
	m = ps_global->s_pool.streams[i];
	if(sp_flagged(m, SP_USEPOOL) && !sp_flagged(m, SP_PERMLOCKED))
	  cnt++;
    }

    return(cnt);
}


/*
 * Returns the number of folders that the user has marked to be PERMLOCKED
 * folders (plus INBOX) that are remote IMAP folders.
 *
 * This routine depends on the fact that VAR_INBOX_PATH, VAR_PERMLOCKED,
 * and the ps_global->context_list are correctly set.
 */
int
sp_nremote_permlocked(void)
{
    int         cnt = 0;
    char      **lock_these, *p = NULL, *dummy = NULL, *lt;
    DRIVER     *d;

    /* first check if INBOX is remote */
    lt = ps_global->VAR_INBOX_PATH;
    if(lt && (d=mail_valid(NIL, lt, (char *) NIL))
       && !strcmp(d->name, "imap"))
      cnt++;

    /* then count the user-specified permlocked folders */
    for(lock_these = ps_global->VAR_PERMLOCKED; lock_these && *lock_these;
	lock_these++){

	/*
	 * Skip inbox, already done above. Should do this better so that we 
	 * catch the case where the user puts the technical spec of the inbox
	 * in the list, or where the user lists one folder twice.
	 */
	if(*lock_these && !strucmp(*lock_these, ps_global->inbox_name))
	  continue;

	/* there isn't really a pair, it just dequotes for us */
	get_pair(*lock_these, &dummy, &p, 0, 0);

	/*
	 * Check to see if this is an incoming nickname and replace it
	 * with the full name.
	 */
	if(!(p && ps_global->context_list
	     && ps_global->context_list->use & CNTXT_INCMNG
	     && (lt=folder_is_nick(p, FOLDERS(ps_global->context_list),
				   FN_WHOLE_NAME))))
	  lt = p;

	if(dummy)
	  fs_give((void **) &dummy);

	if(lt && (d=mail_valid(NIL, lt, (char *) NIL))
	   && !strcmp(d->name, "imap"))
	  cnt++;

	if(p)
	  fs_give((void **) &p);
    }

    return(cnt);
}


/*
 * Look for an already open stream that can be used for a new purpose.
 * (Note that we only look through streams flagged SP_USEPOOL.)
 *
 * Args:   mailbox
 *           flags
 *
 * Flags is a set of values or'd together which tells us what the request
 * is looking for.
 *
 *  Returns: a live stream from the stream pool or NULL.
 */
MAILSTREAM *
sp_stream_get(char *mailbox, long unsigned int flags)
{
    int         i;
    MAILSTREAM *m;

    dprint((7, "sp_stream_get(%s):%s%s%s%s%s\n",
	    mailbox ? mailbox : "?",
	    (flags & SP_MATCH)    ? " SP_MATCH"    : "",
	    (flags & SP_RO_OK)    ? " SP_RO_OK"    : "",
	    (flags & SP_SAME)     ? " SP_SAME"     : "",
	    (flags & SP_UNLOCKED) ? " SP_UNLOCKED" : "",
	    (flags & SP_TEMPUSE)  ? " SP_TEMPUSE" : ""));

    /* look for stream already open to this mailbox */
    if(flags & SP_MATCH){
	for(i = 0; i < ps_global->s_pool.nstream; i++){
	    m = ps_global->s_pool.streams[i];
	    if(m && sp_flagged(m, SP_USEPOOL)
	       && (!m->rdonly || (flags & SP_RO_OK)) && !sp_dead_stream(m)
	       && same_stream_and_mailbox(mailbox, m)){
		if((sp_flagged(m, SP_LOCKED) && recent_activity(m))
		   || pine_mail_ping(m)){
		    dprint((7,
		       "sp_stream_get: found exact match, slot %d\n", i));
		    if(!sp_flagged(m, SP_LOCKED)){
			dprint((7,
			       "reset idle timer1: next TAG %08lx (%s)\n",
			       m->gensym,
			       debug_time(1, ps_global->debug_timestamp)));
			sp_set_last_use_time(m, time(0));
		    }

		    return(m);
		}

		sp_mark_stream_dead(m);
	    }
	}
    }

    /*
     * SP_SAME will not match if an SP_MATCH match would have worked.
     * If the caller is interested in SP_MATCH streams as well as SP_SAME
     * streams then the caller should make two separate calls to this
     * routine.
     */
    if(flags & SP_SAME){
	/*
	 * If the flags arg does not have either SP_TEMPUSE or SP_UNLOCKED
	 * set, then we'll accept any stream, even if locked.
	 * We want to prefer the LOCKED case so that we don't have to ping.
	 */
	if(!(flags & SP_UNLOCKED) && !(flags & SP_TEMPUSE)){
	    for(i = 0; i < ps_global->s_pool.nstream; i++){
		m = ps_global->s_pool.streams[i];
		if(m && sp_flagged(m, SP_USEPOOL)
		   && sp_flagged(m, SP_LOCKED) && !sp_dead_stream(m)
		   && same_stream(mailbox, m)
		   && !same_stream_and_mailbox(mailbox, m)){
		    if(recent_activity(m) || pine_mail_ping(m)){
			dprint((7,
			    "sp_stream_get: found SAME match, slot %d\n", i));
			return(m);
		    }

		    sp_mark_stream_dead(m);
		}
	    }

	    /* consider the unlocked streams */
	    for(i = 0; i < ps_global->s_pool.nstream; i++){
		m = ps_global->s_pool.streams[i];
		if(m && sp_flagged(m, SP_USEPOOL)
		   && !sp_flagged(m, SP_LOCKED) && !sp_dead_stream(m)
		   && same_stream(mailbox, m)
		   && !same_stream_and_mailbox(mailbox, m)){
		    /* always ping unlocked streams */
		    if(pine_mail_ping(m)){
			dprint((7,
			    "sp_stream_get: found SAME match, slot %d\n", i));
			dprint((7,
			   "reset idle timer4: next TAG %08lx (%s)\n",
			   m->gensym,
			   debug_time(1, ps_global->debug_timestamp)));
			sp_set_last_use_time(m, time(0));

			return(m);
		    }

		    sp_mark_stream_dead(m);
		}
	    }
	}

	/*
	 * Prefer streams marked SP_TEMPUSE and not LOCKED.
	 * If SP_TEMPUSE is set in the flags arg then this is the
	 * only loop we try.
	 */
	for(i = 0; i < ps_global->s_pool.nstream; i++){
	    m = ps_global->s_pool.streams[i];
	    if(m && sp_flagged(m, SP_USEPOOL) && sp_flagged(m, SP_TEMPUSE)
	       && !sp_flagged(m, SP_LOCKED) && !sp_dead_stream(m)
	       && same_stream(mailbox, m)
	       && !same_stream_and_mailbox(mailbox, m)){
		if(pine_mail_ping(m)){
		    dprint((7,
		      "sp_stream_get: found SAME/TEMPUSE match, slot %d\n", i));
		    dprint((7,
			   "reset idle timer2: next TAG %08lx (%s)\n",
			   m->gensym,
			   debug_time(1, ps_global->debug_timestamp)));
		    sp_set_last_use_time(m, time(0));
		    return(m);
		}

		sp_mark_stream_dead(m);
	    }
	}

	/*
	 * If SP_TEMPUSE is not set in the flags arg but SP_UNLOCKED is,
	 * then we will consider
	 * streams which are not marked SP_TEMPUSE (but are still not
	 * locked). We go through these in reverse order so that we'll get
	 * the last one added instead of the first one. It's not clear if
	 * that is a good idea or if a more complex search would somehow
	 * be better. Maybe we should use a round-robin sort of search
	 * here so that we don't leave behind unused streams. Or maybe
	 * we should keep track of when we used it and look for the LRU stream.
	 */
	if(!(flags & SP_TEMPUSE)){
	    for(i = ps_global->s_pool.nstream - 1; i >= 0; i--){
		m = ps_global->s_pool.streams[i];
		if(m && sp_flagged(m, SP_USEPOOL)
		   && !sp_flagged(m, SP_LOCKED) && !sp_dead_stream(m)
		   && same_stream(mailbox, m)
		   && !same_stream_and_mailbox(mailbox, m)){
		    if(pine_mail_ping(m)){
			dprint((7,
		    "sp_stream_get: found SAME/UNLOCKED match, slot %d\n", i));
			dprint((7,
			   "reset idle timer3: next TAG %08lx (%s)\n",
			   m->gensym,
			   debug_time(1, ps_global->debug_timestamp)));
			sp_set_last_use_time(m, time(0));
			return(m);
		    }

		    sp_mark_stream_dead(m);
		}
	    }
	}
    }

    /*
     * If we can't find a useful stream to use in pine_mail_open, we may
     * want to re-use one that is not actively being used even though it
     * is not on the same server. We'll have to close it and then re-use
     * the slot.
     */
    if(!(flags & (SP_SAME | SP_MATCH))){
	/*
	 * Prefer streams marked SP_TEMPUSE and not LOCKED.
	 * If SP_TEMPUSE is set in the flags arg then this is the
	 * only loop we try.
	 */
	for(i = 0; i < ps_global->s_pool.nstream; i++){
	    m = ps_global->s_pool.streams[i];
	    if(m && sp_flagged(m, SP_USEPOOL) && sp_flagged(m, SP_TEMPUSE)
	       && !sp_flagged(m, SP_LOCKED)){
		dprint((7,
    "sp_stream_get: found Not-SAME/TEMPUSE match, slot %d\n", i));
		/*
		 * We ping it in case there is new mail that we should
		 * pass through our filters. Pine_mail_actually_close will
		 * do that.
		 */
		(void) pine_mail_ping(m);
		return(m);
	    }
	}

	/*
	 * If SP_TEMPUSE is not set in the flags arg, then we will consider
	 * streams which are not marked SP_TEMPUSE (but are still not
	 * locked). Maybe we should use a round-robin sort of search
	 * here so that we don't leave behind unused streams. Or maybe
	 * we should keep track of when we used it and look for the LRU stream.
	 */
	if(!(flags & SP_TEMPUSE)){
	    for(i = ps_global->s_pool.nstream - 1; i >= 0; i--){
		m = ps_global->s_pool.streams[i];
		if(m && sp_flagged(m, SP_USEPOOL) && !sp_flagged(m, SP_LOCKED)){
		    dprint((7,
	"sp_stream_get: found Not-SAME/UNLOCKED match, slot %d\n", i));
		    /*
		     * We ping it in case there is new mail that we should
		     * pass through our filters. Pine_mail_actually_close will
		     * do that.
		     */
		    (void) pine_mail_ping(m);
		    return(m);
		}
	    }
	}
    }

    dprint((7, "sp_stream_get: no match found\n"));

    return(NULL);
}


void
sp_end(void)
{
    int         i;
    MAILSTREAM *m;

    dprint((7, "sp_end\n"));

    for(i = 0; i < ps_global->s_pool.nstream; i++){
	m = ps_global->s_pool.streams[i];
	if(m)
	  pine_mail_actually_close(m);
    }

    if(ps_global->s_pool.streams)
      fs_give((void **) &ps_global->s_pool.streams);

    ps_global->s_pool.nstream = 0;
}


/*
 * Find a vacant slot to put this new stream in.
 * We are willing to close and kick out another stream as long as it isn't
 * LOCKED. However, we may find that there is no place to put this one
 * because all the slots are used and locked. For now, we'll return -1
 * in that case and leave the new stream out of the pool.
 */
int
sp_add(MAILSTREAM *stream, int usepool)
{
    int         i, slot = -1;
    MAILSTREAM *m;

    dprint((7, "sp_add(%s, %d)\n",
	    (stream && stream->mailbox) ? stream->mailbox : "?", usepool));

    if(!stream){
	dprint((7, "sp_add: NULL stream\n"));
	return -1;
    }

    /* If this stream is already there, don't add it again */
    for(i = 0; i < ps_global->s_pool.nstream; i++){
	m = ps_global->s_pool.streams[i];
	if(m == stream){
	    slot = i;
	    dprint((7,
		    "sp_add: stream was already in slot %d\n", slot));
	    return 0;
	}
    }

    if(usepool && !sp_flagged(stream, SP_PERMLOCKED)
       && sp_nusepool_notperm() >= ps_global->s_pool.max_remstream){
	dprint((7,
		"sp_add: reached max implicit SP_USEPOOL of %d\n",
		ps_global->s_pool.max_remstream));
	return -1;
    }

    /* Look for an unused slot */
    for(i = 0; i < ps_global->s_pool.nstream; i++){
	m = ps_global->s_pool.streams[i];
	if(!m){
	    slot = i;
	    dprint((7,
		    "sp_add: using empty slot %d\n", slot));
	    break;
	}
    }

    /* else, allocate more space */
    if(slot < 0){
	ps_global->s_pool.nstream++;
	slot = ps_global->s_pool.nstream - 1;
	if(ps_global->s_pool.streams){
	    fs_resize((void **) &ps_global->s_pool.streams,
		      ps_global->s_pool.nstream *
		        sizeof(*ps_global->s_pool.streams));
	    ps_global->s_pool.streams[slot] = NULL;
	}
	else{
	    ps_global->s_pool.streams =
		(MAILSTREAM **) fs_get(ps_global->s_pool.nstream *
					sizeof(*ps_global->s_pool.streams));
	    memset(ps_global->s_pool.streams, 0,
		   ps_global->s_pool.nstream *
		    sizeof(*ps_global->s_pool.streams));
	}

	dprint((7,
	    "sp_add: allocate more space, using new slot %d\n", slot));
    }

    if(slot >= 0 && slot < ps_global->s_pool.nstream){
	ps_global->s_pool.streams[slot] = stream;
	return 0;
    }
    else{
	dprint((7, "sp_add: failed to find a slot!\n"));
	return -1;
    }
}


/*
 * Simply remove this stream from the stream pool.
 */
void
sp_delete(MAILSTREAM *stream)
{
    int         i;
    MAILSTREAM *m;

    if(!stream)
      return;

    dprint((7, "sp_delete(%s)\n",
	    (stream && stream->mailbox) ? stream->mailbox : "?"));

    /*
     * There are some global stream pointers that we have to worry
     * about before deleting the stream.
     */

    /* first, mail_stream is the global currently open folder */
    if(ps_global->mail_stream == stream)
      ps_global->mail_stream = NULL;
    
    /* remote address books may have open stream pointers */
    note_closed_adrbk_stream(stream);

    if(pith_opt_closing_stream)
      (*pith_opt_closing_stream)(stream);

    for(i = 0; i < ps_global->s_pool.nstream; i++){
	m = ps_global->s_pool.streams[i];
	if(m == stream){
	    ps_global->s_pool.streams[i] = NULL;
	    dprint((7,
		    "sp_delete: stream removed from slot %d\n", i));
	    return;
	}
    }
}


/*
 * Returns 1 if any locked userfldr is dead, 0 if all alive.
 */
int
sp_a_locked_stream_is_dead(void)
{
    int         i, ret = 0;
    MAILSTREAM *m;

    for(i = 0; !ret && i < ps_global->s_pool.nstream; i++){
	m = ps_global->s_pool.streams[i];
	if(m && sp_flagged(m, SP_LOCKED) && sp_flagged(m, SP_USERFLDR)
	   && sp_dead_stream(m))
	  ret++;
    }

    return(ret);
}


/*
 * Returns 1 if any locked stream is changed, 0 otherwise
 */
int
sp_a_locked_stream_changed(void)
{
    int         i, ret = 0;
    MAILSTREAM *m;

    for(i = 0; !ret && i < ps_global->s_pool.nstream; i++){
	m = ps_global->s_pool.streams[i];
	if(m && sp_flagged(m, SP_LOCKED) && sp_flagged(m, SP_USERFLDR)
	   && sp_mail_box_changed(m))
	  ret++;
    }

    return(ret);
}


/*
 * Returns the inbox stream or NULL.
 */
MAILSTREAM *
sp_inbox_stream(void)
{
    int         i;
    MAILSTREAM *m, *ret = NULL;

    for(i = 0; !ret && i < ps_global->s_pool.nstream; i++){
	m = ps_global->s_pool.streams[i];
	if(m && sp_flagged(m, SP_INBOX))
	  ret = m;
    }

    return(ret);
}


/*
 * Make sure that the sp_data per-stream data storage area exists.
 *
 * Returns a handle to the sp_data data unless stream is NULL,
 *         in which case NULL is returned
 */
PER_STREAM_S **
sp_data(MAILSTREAM *stream)
{
    PER_STREAM_S **pss = NULL;

    if(stream){
	if(*(pss = (PER_STREAM_S **) &stream->sparep) == NULL){
	    *pss = (PER_STREAM_S *) fs_get(sizeof(PER_STREAM_S));
	    memset(*pss, 0, sizeof(PER_STREAM_S));
	    reset_check_point(stream);
	}
    }

    return(pss);
}


/*
 * Returns a pointer to the msgmap associated with the argument stream.
 *
 * If the PER_STREAM_S data or the msgmap does not already exist, it will be
 * created.
 */
MSGNO_S *
sp_msgmap(MAILSTREAM *stream)
{
    MSGNO_S      **msgmap = NULL;
    PER_STREAM_S **pss    = NULL;

    pss = sp_data(stream);

    if(pss && *pss
       && (*(msgmap = (MSGNO_S **) &(*pss)->msgmap) == NULL))
      mn_init(msgmap, stream->nmsgs);

    return(msgmap ? *msgmap : NULL);
}


void
sp_free_callback(void **sparep)
{
    PER_STREAM_S **pss;
    MAILSTREAM    *stream = NULL, *m;
    int            i;

    pss = (PER_STREAM_S **) sparep;

    if(pss && *pss){
	/*
	 * It is possible that this has been called from c-client when
	 * we weren't expecting it. We need to clean up the stream pool
	 * entries if the stream that goes with this pointer is in the
	 * stream pool somewhere.
	 */
	for(i = 0; !stream && i < ps_global->s_pool.nstream; i++){
	    m = ps_global->s_pool.streams[i];
	    if(sparep && *sparep && m && m->sparep == *sparep)
	      stream = m;
	}

	if(stream){
	    if(ps_global->mail_stream == stream)
	      ps_global->mail_stream = NULL;

	    sp_delete(stream);
	}

	sp_free(pss);
    }
}


/*
 * Free the data but don't mess with the stream pool.
 */
void
sp_free(PER_STREAM_S **pss)
{
    if(pss && *pss){
	if((*pss)->msgmap){
	    if(ps_global->msgmap == (*pss)->msgmap)
	      ps_global->msgmap = NULL;

	    mn_give(&(*pss)->msgmap);
	}

	if((*pss)->fldr)
	  fs_give((void **) &(*pss)->fldr);
	
	fs_give((void **) pss);
    }
}



/*----------------------------------------------------------------------
  See if stream can be used for a mailbox name

   Accepts: mailbox name
            candidate stream
   Returns: stream if it can be used, else NIL

  This is called to weed out unnecessary use of c-client streams. In other
  words, to help facilitate re-use of streams.

  This code is very similar to the same_remote_mailboxes code below, which
  is used in pine_mail_open. That code compares two mailbox names. One is
  usually from the config file and the other is either from the config file
  or is typed in. Here and in same_stream_and_mailbox below, we're comparing
  an open stream to a name instead of two names. We could conceivably use
  same_remote_mailboxes to compare stream->mailbox to name, but it isn't
  exactly the same and the differences may be important. Some stuff that
  happens here seems wrong, but it isn't easy to fix.
  Having !mb_n.port count as a match to any mb_s.port isn't right. It should
  only match if mb_s.port is equal to the default, but the default isn't
  something that is available to us. The same thing is done in c-client in
  the mail_usable_network_stream() routine, and it isn't right there, either.
  The semantics of a missing user are also suspect, because just like with
  port, a default is used.
  ----*/
MAILSTREAM *
same_stream(char *name, MAILSTREAM *stream)
{
    NETMBX mb_s, mb_n, mb_o;

    if(stream && stream->mailbox && *stream->mailbox && name && *name
       && !(sp_dead_stream(stream))
       && mail_valid_net_parse(stream->mailbox, &mb_s)
       && mail_valid_net_parse(stream->original_mailbox, &mb_o)
       && mail_valid_net_parse(name, &mb_n)
       && !strucmp(mb_n.service, mb_s.service)
       && (!strucmp(mb_n.host, mb_o.host)	/* s is already canonical */
	   || !strucmp(canonical_name(mb_n.host), mb_s.host))
       && (!mb_n.port || mb_n.port == mb_s.port)
       && mb_n.anoflag == stream->anonymous
       && ((mb_n.user && *mb_n.user &&
	    mb_s.user && !strcmp(mb_n.user, mb_s.user))
	   ||
	   ((!mb_n.user || !*mb_n.user)
	    && mb_s.user
	    && ((ps_global->VAR_USER_ID
		 && !strcmp(ps_global->VAR_USER_ID, mb_s.user))
		||
		(!ps_global->VAR_USER_ID
		 && ps_global->ui.login[0]
		 && !strcmp(ps_global->ui.login, mb_s.user))))
	   ||
	   (!((mb_n.user && *mb_n.user) || (mb_s.user && *mb_s.user))
	    && stream->anonymous))
       && (struncmp(mb_n.service, "imap", 4) ? 1 : strcmp(imap_host(stream), ".NO-IMAP-CONNECTION."))){
	dprint((7, "same_stream: name->%s == stream->%s: yes\n",
	       name ? name : "?",
	       (stream && stream->mailbox) ? stream->mailbox : "NULL"));
	return(stream);
    }

    dprint((7, "same_stream: name->%s == stream->%s: no dice\n",
	   name ? name : "?",
	   (stream && stream->mailbox) ? stream->mailbox : "NULL"));
    return(NULL);
}



/*----------------------------------------------------------------------
  See if this stream has the named mailbox selected.

   Accepts: mailbox name
            candidate stream
   Returns: stream if it can be used, else NIL
  ----*/
MAILSTREAM *
same_stream_and_mailbox(char *name, MAILSTREAM *stream)
{
    NETMBX mb_s, mb_n;

    if(same_stream(name, stream)
       && mail_valid_net_parse(stream->mailbox, &mb_s)
       && mail_valid_net_parse(name, &mb_n)
       && (mb_n.mailbox && mb_s.mailbox
       &&  (!strcmp(mb_n.mailbox,mb_s.mailbox)  /* case depend except INBOX */
            || (!strucmp(mb_n.mailbox,"INBOX")
	        && !strucmp(mb_s.mailbox,"INBOX"))))){
	dprint((7,
	       "same_stream_and_mailbox: name->%s == stream->%s: yes\n",
	       name ? name : "?",
	       (stream && stream->mailbox) ? stream->mailbox : "NULL"));
	return(stream);
    }

    dprint((7,
	   "same_stream_and_mailbox: name->%s == stream->%s: no dice\n",
	   name ? name : "?",
	   (stream && stream->mailbox) ? stream->mailbox : "NULL"));
    return(NULL);
}

/*
 * Args -- name1 and name2 are remote mailbox names.
 *
 * Returns --  True  if names refer to same mailbox accessed in same way
 *             False if not
 *
 * This has some very similar code to same_stream_and_mailbox but we're not
 * quite ready to discard the differences.
 * The treatment of the port and the user is not quite the same.
 */
int
same_remote_mailboxes(char *name1, char *name2)
{
    NETMBX mb1, mb2;
    char *cn1;

    /*
     * Probably we should allow !port equal to default port, but we don't
     * know how to get the default port. To match what c-client does we
     * allow !port to be equal to anything.
     */
    return(name1 && IS_REMOTE(name1)
	   && name2 && IS_REMOTE(name2)
	   && mail_valid_net_parse(name1, &mb1)
	   && mail_valid_net_parse(name2, &mb2)
	   && !strucmp(mb1.service, mb2.service)
	   && (!strucmp(mb1.host, mb2.host)	/* just to save DNS lookups */
	       || !strucmp(cn1=canonical_name(mb1.host), mb2.host)
	       || !strucmp(cn1, canonical_name(mb2.host)))
	   && (!mb1.port || !mb2.port || mb1.port == mb2.port)
	   && mb1.anoflag == mb2.anoflag
	   && mb1.mailbox && mb2.mailbox
	   && (!strcmp(mb1.mailbox, mb2.mailbox)
	       || (!strucmp(mb1.mailbox,"INBOX")
		   && !strucmp(mb2.mailbox,"INBOX")))
	   && ((mb1.user && *mb1.user && mb2.user && *mb2.user
		&& !strcmp(mb1.user, mb2.user))
	       ||
	       (!(mb1.user && *mb1.user) && !(mb2.user && *mb2.user))
	       ||
	       (!(mb1.user && *mb1.user)
		&& ((ps_global->VAR_USER_ID
		     && !strcmp(ps_global->VAR_USER_ID, mb2.user))
		    ||
		    (!ps_global->VAR_USER_ID
		     && ps_global->ui.login[0]
		     && !strcmp(ps_global->ui.login, mb2.user))))
	       ||
	       (!(mb2.user && *mb2.user)
		&& ((ps_global->VAR_USER_ID
		     && !strcmp(ps_global->VAR_USER_ID, mb1.user))
		    ||
		    (!ps_global->VAR_USER_ID
		     && ps_global->ui.login[0]
		     && !strcmp(ps_global->ui.login, mb1.user))))));
}


int
is_imap_stream(MAILSTREAM *stream)
{
    return(stream && stream->dtb && stream->dtb->name
           && !strcmp(stream->dtb->name, "imap"));
}


int
modern_imap_stream(MAILSTREAM *stream)
{
    return(is_imap_stream(stream) && LEVELIMAP4rev1(stream));
}


/*----------------------------------------------------------------------
     Check and see if all the stream are alive

Returns:  0 if there was no change
         >0 if streams have died since last call

Also outputs a message that the streams have died
 ----*/
int
streams_died(void)
{
    int rv = 0;
    int         i;
    MAILSTREAM *m;
    char       *folder;

    for(i = 0; i < ps_global->s_pool.nstream; i++){
	m = ps_global->s_pool.streams[i];
	if(m && sp_dead_stream(m)){
	    if(sp_flagged(m, SP_LOCKED) && sp_flagged(m, SP_USERFLDR)){
		if(!sp_noticed_dead_stream(m)){
		    rv++;
		    sp_set_noticed_dead_stream(m, 1);
		    folder = STREAMNAME(m);
		    q_status_message1(SM_ORDER | SM_DING, 3, 3,
			  _("MAIL FOLDER \"%s\" CLOSED DUE TO ACCESS ERROR"),
			  short_str(pretty_fn(folder) ? pretty_fn(folder) : "?",
				    tmp_20k_buf+1000, SIZEOF_20KBUF-1000, 35, FrontDots));
		    dprint((6, "streams_died: locked: \"%s\"\n",
			    folder));
		    if(rv == 1){
			snprintf(tmp_20k_buf, SIZEOF_20KBUF, _("Folder \"%s\" is Closed"), 
			  short_str(pretty_fn(folder) ? pretty_fn(folder) : "?",
			  tmp_20k_buf+1000, SIZEOF_20KBUF-1000, 35, FrontDots));
			if(pith_opt_icon_text)
			  (*pith_opt_icon_text)(tmp_20k_buf, IT_MCLOSED);
		    }
		}
	    }
	    else{
		if(!sp_noticed_dead_stream(m)){
		    sp_set_noticed_dead_stream(m, 1);
		    folder = STREAMNAME(m);
		    /*
		     * If a cached stream died and then we tried to use it
		     * it could cause problems. We could warn about it here
		     * but it may be confusing because it might be
		     * unrelated to what the user is doing and not cause
		     * any problem at all.
		     */
#if 0
		    if(sp_flagged(m, SP_USEPOOL))
		      q_status_message(SM_ORDER, 3, 3,
	"Warning: Possible problem accessing remote data, connection died.");
#endif

		    dprint((6, "streams_died: not locked: \"%s\"\n",
			    folder));
		}

		pine_mail_actually_close(m);
	    }
	}
    }

    return(rv);
}


/*
 * Very simple version of appenduid_cb until we need something
 * more complex.
 */

static imapuid_t last_append_uid;

void
appenduid_cb(char *mailbox,unsigned long uidvalidity, SEARCHSET *set)
{
    last_append_uid = set ? set->first : 0L;
}


imapuid_t
get_last_append_uid(void)
{
    return last_append_uid;
}


/*
 * mail_cmd_stream - return a stream suitable for mail_lsub,
 *		      mail_subscribe, and mail_unsubscribe
 *
 */
MAILSTREAM *
mail_cmd_stream(CONTEXT_S *context, int *closeit)
{
    char	tmp[MAILTMPLEN];

    *closeit = 1;
    (void) context_apply(tmp, context, "x", sizeof(tmp));

    return(pine_mail_open(NULL, tmp,
			  OP_HALFOPEN | OP_SILENT | SP_USEPOOL | SP_TEMPUSE,
			  NULL));
}


/*
 * This is so we can replace the old rfc822_ routines like rfc822_header_line
 * with the new version that checks bounds, like rfc822_output_header_line.
 * This routine is called when would be a bounds overflow, which we simply log
 * and go on with the truncated data.
 */
long
dummy_soutr(void *stream, char *string)
{
    dprint((2, "dummy_soutr unexpected call, caught overflow\n"));
    return LONGT;
}
