#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: newmail.c 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $";
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
#include "../pith/newmail.h"
#include "../pith/conf.h"
#include "../pith/flag.h"
#include "../pith/mailindx.h"
#include "../pith/msgno.h"
#include "../pith/bldaddr.h"
#include "../pith/stream.h"
#include "../pith/sort.h"
#include "../pith/status.h"
#include "../pith/util.h"
#include "../pith/thread.h"
#include "../pith/options.h"
#include "../pith/folder.h"
#include "../pith/ablookup.h"

#ifdef _WINDOWS
#include "../pico/osdep/mswin.h"
#endif


/*
 * Hooks for various stuff
 */
void	(*pith_opt_newmail_announce)(MAILSTREAM *, long, long);
void	(*pith_opt_newmail_check_cue)(int);
void	(*pith_opt_checkpoint_cue)(int);
void	(*pith_opt_icon_text)(char *, int);


void fixup_flags(MAILSTREAM *, MSGNO_S *);
int  check_point(MAILSTREAM *, CheckPointTime, int);


/*----------------------------------------------------------------------
     new_mail() - check for new mail in the inbox
 
  Input:  force       -- flag indicating we should check for new mail no matter
          time_for_check_point -- 0: GoodTime, 1: BadTime, 2: VeryBadTime
	  flags -- whether to q a new mail status message or defer the sort

  Result: returns -1 if there was no new mail. Otherwise returns the
          sorted message number of the smallest message number that
          was changed. That is the screen needs to be repainted from
          that message down.

  Limit frequency of checks because checks use some resources. That is
  they generate an IMAP packet or require locking the local mailbox.
  (Acutally the lock isn't required, a stat will do, but the current
   c-client mail code locks before it stats.)

  Returns >= 0 only if there is a change in the given mail stream. Otherwise
  this returns -1.  On return the message counts in the pine
  structure are updated to reflect the current number of messages including
  any new mail and any expunging.
 
 --- */
long
new_mail(int force_arg, CheckPointTime time_for_check_point, int flags)
{
    static time_t last_check_point_call = 0;
    long          since_last_input;
    time_t        expunged_reaper_to, adj_idle_timeout, interval, adj;
    static int    nexttime = 0;
    time_t        now;
    long          n, rv = 0, t_nm_count = 0, exp_count;
    MAILSTREAM   *m;
    int           force, i, started_on;
    int           new_mail_was_announced = 0;
    int           have_pinged_non_special = 0;
    int		  timeo;

    dprint((9, "new mail called (force=%d %s flags=0x%x)\n",
	       force_arg,
	       time_for_check_point == GoodTime    ? "GoodTime" :
	        time_for_check_point == BadTime     ? "BadTime"  :
		 time_for_check_point == VeryBadTime ? "VeryBad"  :
		  time_for_check_point == DoItNow     ? "DoItNow"  : "?",
	       flags));

    force = force_arg;

    now = time(0);

    timeo = get_input_timeout();

    if(time_for_check_point == GoodTime)
      adrbk_maintenance();

    if(time_for_check_point == GoodTime || force_arg)
      folder_unseen_count_updater(UFU_ANNOUNCE | (force_arg ? UFU_FORCE : 0));

    if(sp_need_to_rethread(ps_global->mail_stream))
      force = 1;

    if(!force && sp_unsorted_newmail(ps_global->mail_stream))
      force = !(flags & NM_DEFER_SORT);

    if(!ps_global->mail_stream
       || !(timeo || force || sp_a_locked_stream_changed()))
      return(-1);

    last_check_point_call = now;
    since_last_input = (long) now - (long) time_of_last_input();

    /*
     * We have this for loop followed by the do-while so that we will prefer
     * to ping the active streams before we ping the inactive ones, in cases
     * where the pings or checks are taking a long time.
     */
    for(i = 0; i < ps_global->s_pool.nstream; i++){
	m = ps_global->s_pool.streams[i];
	if(!m || m->halfopen ||
	   (m != ps_global->mail_stream &&
	    !(force_arg && sp_flagged(m, SP_LOCKED))))
	  continue;
	
	/*
	 * This for() loop is only the current folder, unless the user
	 * has forced the check, in which case it is all locked folders.
	 */
	
	/*
	 * After some amount of inactivity on a stream, the server may
	 * close the stream. Each protocol has its own idea of how much
	 * inactivity should be allowed before the stream is closed. For
	 * example, IMAP specifies that the server should not close the
	 * stream unilaterally until at least 30 minutes of inactivity.
	 * The GET_IDLETIMEOUT call gives us that time in minutes. We
	 * want to be sure to keep the stream alive if it is about to die
	 * due to inactivity.
	 */
	adj_idle_timeout = 60 * (long) mail_parameters(m,GET_IDLETIMEOUT,NULL);
	if(adj_idle_timeout <= 0)
	  adj_idle_timeout = 600;

	adj = (adj_idle_timeout >= 50 * FUDGE) ? 5 * FUDGE :
	        (adj_idle_timeout >= 30 * FUDGE) ? 3 * FUDGE :
	          (adj_idle_timeout >= 20 * FUDGE) ? 2 * FUDGE : FUDGE;
	adj_idle_timeout = MAX(adj_idle_timeout - adj, 120);

	/*
	 * Set interval to mail-check-interval unless
	 * mail-check-interval-noncurrent is nonzero and this is not inbox
	 * or current stream.
	 */
	if(ps_global->check_interval_for_noncurr > 0
	   && m != ps_global->mail_stream
	   && !sp_flagged(m, SP_INBOX))
	  interval = ps_global->check_interval_for_noncurr;
	else
	  interval = timeo;

	/*
	 * We want to make sure that we notice expunges, but we don't have
	 * to be fanatical about it. Every once in a while we'll do a ping
	 * because we haven't had a command that notices expunges for a
	 * while. It's also a larger number than interval so it gives us a
	 * convenient interval to do slower pinging than interval if we
	 * are busy.
	 */
	if(interval <= adj_idle_timeout)
	  expunged_reaper_to = MIN(MAX(2*interval,180), adj_idle_timeout);
	else
	  expunged_reaper_to = interval;

	/*
	 * User may want to avoid new mail checking while composing.
	 * In this case we will only do the keepalives.
	 */
	if(timeo == 0
	   || (flags & NM_FROM_COMPOSER
	       && ((F_ON(F_QUELL_PINGS_COMPOSING, ps_global)
	            && !sp_flagged(m, SP_INBOX))
	           ||
	           (F_ON(F_QUELL_PINGS_COMPOSING_INBOX, ps_global)
	            && sp_flagged(m, SP_INBOX))))){
	    interval = expunged_reaper_to = 0;
	}

	dprint((9,
	       "%s: force=%d interval=%ld exp_reap_to=%ld adj_idle_to=%ld\n",
	       STREAMNAME(m), force_arg, (long) interval,
	       (long) expunged_reaper_to, (long) adj_idle_timeout));
	dprint((9,
	       "   since_last_ping=%ld since_last_reap=%ld\n",
	       (long) (now - sp_last_ping(m)),
	       (long) (now - sp_last_expunged_reaper(m))));

	/* if check_point does a check it resets last_ping time */
	if(force_arg || (timeo && expunged_reaper_to > 0))
	  (void) check_point(m, time_for_check_point, flags);

	/*
	 * Remember that unless force_arg is set, this check is really
	 * only for the current folder. This is usually going to fire
	 * on the first test, which is the interval the user set.
	 */
	if(force_arg
	   ||
	   (timeo
	    &&
	    ((interval && (now - sp_last_ping(m) >= interval-1))
	     ||
	     (expunged_reaper_to
	      && (now - sp_last_expunged_reaper(m)) >= expunged_reaper_to)
	     ||
	     (now - sp_last_ping(m) >= adj_idle_timeout)))){

	    if((flags & NM_STATUS_MSG) && pith_opt_newmail_check_cue)
	      (*pith_opt_newmail_check_cue)(TRUE);

	    dprint((7, "Mail_Ping(%s): lastping=%ld er=%ld%s%s %s\n",
		   STREAMNAME(m),
		   (long) (now - sp_last_ping(m)),
		   (long) (now - sp_last_expunged_reaper(m)),
		   force_arg                      ? " [forced]" :
		    interval && (now-sp_last_ping(m) >= interval-1)
						   ? " [it's time]" :
		    expunged_reaper_to
		     && (now - sp_last_expunged_reaper(m) >= expunged_reaper_to)
						    ? " [expunged reaper]"
						    : " [keepalive]",
		   m == ps_global->mail_stream ? " [current]" : "",
		   debug_time(0,1)));

	    /*
	     * We're about to ping the stream.
	     * If the stream is a #move Mail Drop there is a minimum time
	     * between re-opens of the mail drop to check for new mail.
	     * If the check is forced by the user, they want it to
	     * happen now. We use knowledge of c-client internals to
	     * make this happen.
	     */
	    if(force_arg && m && m->snarf.name)
	      m->snarf.time = 0;

	    /*-- Ping the stream to check for new mail --*/
	    if(sp_dead_stream(m)){
		dprint((6, "no check: stream is dead\n"));
	    }
	    else if(!pine_mail_ping(m)){
		dprint((6, "ping failed: stream is dead\n"));
		sp_set_dead_stream(m, 1);
	    }

	    dprint((7, "Ping complete: %s\n", debug_time(0,1)));

	    if((flags & NM_STATUS_MSG) && pith_opt_newmail_check_cue)
	      (*pith_opt_newmail_check_cue)(FALSE);
	}
    }

    if(nexttime < 0 || nexttime >= ps_global->s_pool.nstream)
      nexttime = 0;

    i = started_on = nexttime;
    do {
	m = ps_global->s_pool.streams[i];

	nexttime = i;
	if(ps_global->s_pool.nstream > 0)
	  i = (i + 1) % ps_global->s_pool.nstream;

	/*
	 * This do() loop handles all the other streams that weren't covered
	 * in the for() loop above.
	 */
	if((!m || m->halfopen) ||
	   (m == ps_global->mail_stream ||
	    (force_arg && sp_flagged(m, SP_LOCKED)))){
	    nexttime = i;
	    continue;
	}

	/*
	 * If it is taking an extra long time to do the pings and checks,
	 * think about skipping some of them. Always do at least one of
	 * these non-special streams (if they are due to be pinged).
	 * The nexttime variable keeps track of where we were so that we
	 * don't always ping the first one in the list and then skip out.
	 */
	if((time(0) - now >= 5) && have_pinged_non_special){
	    dprint((7, "skipping pings due to delay: %s\n",
			debug_time(0,1)));
	    break;
	}

	nexttime = i;
	have_pinged_non_special++;

	adj_idle_timeout = 60 * (long) mail_parameters(m,GET_IDLETIMEOUT,NULL);
	if(adj_idle_timeout <= 0)
	  adj_idle_timeout = 600;

	adj = (adj_idle_timeout >= 50 * FUDGE) ? 5 * FUDGE :
	        (adj_idle_timeout >= 30 * FUDGE) ? 3 * FUDGE :
	          (adj_idle_timeout >= 20 * FUDGE) ? 2 * FUDGE : FUDGE;
	adj_idle_timeout = MAX(adj_idle_timeout - adj, 120);

	if(ps_global->check_interval_for_noncurr > 0
	   && m != ps_global->mail_stream
	   && !sp_flagged(m, SP_INBOX))
	  interval = ps_global->check_interval_for_noncurr;
	else
	  interval = timeo;

	if(interval <= adj_idle_timeout)
	  expunged_reaper_to = MIN(MAX(2*interval,180), adj_idle_timeout);
	else
	  expunged_reaper_to = interval;

	if(timeo == 0
	   || (flags & NM_FROM_COMPOSER
	       && ((F_ON(F_QUELL_PINGS_COMPOSING, ps_global)
	            && !sp_flagged(m, SP_INBOX))
	           ||
	           (F_ON(F_QUELL_PINGS_COMPOSING_INBOX, ps_global)
	            && sp_flagged(m, SP_INBOX))))){
	    interval = expunged_reaper_to = 0;
	}

	dprint((9,
	       "%s: force=%d interval=%ld exp_reap_to=%ld adj_idle_to=%ld\n",
	       STREAMNAME(m), force_arg, (long) interval,
	       (long) expunged_reaper_to, (long) adj_idle_timeout));
	dprint((9,
	    "   since_last_ping=%ld since_last_reap=%ld since_last_input=%ld\n",
	    (long) (now - sp_last_ping(m)),
	    (long) (now - sp_last_expunged_reaper(m)),
	    (long) since_last_input));

	/* if check_point does a check it resets last_ping time */
	if(force_arg || (timeo && expunged_reaper_to > 0))
	  (void) check_point(m, time_for_check_point, flags);


	/*
	 * The check here is a little bit different from the current folder
	 * check in the for() loop above. In particular, we defer our
	 * pinging for awhile if the last input was recent (except for the
	 * inbox!). We ping streams which are cached but not actively being
	 * used (that is, the non-locked streams) at a slower rate.
	 * If we don't use them for a long time we will eventually close them
	 * (in maybe_kill_old_stream()) but we do it when we want to instead
	 * of when the server wants us to by attempting to keep it alive here.
	 * The other reason to ping the cached streams is that we only get
	 * told the new mail in those streams is recent one time, the messages
	 * that weren't handled here will no longer be recent next time
	 * we open the folder.
	 */
	if((force_arg && sp_flagged(m, SP_LOCKED))
	   ||
	   (timeo
	    &&
	    ((interval
	      && sp_flagged(m, SP_LOCKED)
	      && ((since_last_input >= 3
		   && (now-sp_last_ping(m) >= interval-1))
		  || (sp_flagged(m, SP_INBOX)
		      && (now-sp_last_ping(m) >= interval-1))))
	     ||
	     (expunged_reaper_to
	      && sp_flagged(m, SP_LOCKED)
	      && (now-sp_last_expunged_reaper(m) >= expunged_reaper_to))
	     ||
	     (expunged_reaper_to
	      && !sp_flagged(m, SP_LOCKED)
	      && since_last_input >= 3
	      && (now-sp_last_ping(m) >= expunged_reaper_to))
	     ||
	     (now - sp_last_ping(m) >= adj_idle_timeout)))){

	    if((flags & NM_STATUS_MSG) && pith_opt_newmail_check_cue)
	      (*pith_opt_newmail_check_cue)(TRUE);

	    dprint((7,
		   "Mail_Ping(%s): lastping=%ld er=%ld%s idle: %ld %s\n",
		   STREAMNAME(m),
		   (long) (now - sp_last_ping(m)),
		   (long) (now - sp_last_expunged_reaper(m)),
		   (force_arg && sp_flagged(m, SP_LOCKED)) ? " [forced]" :
		   (interval
		    && sp_flagged(m, SP_LOCKED)
		    && ((since_last_input >= 3
		         && (now-sp_last_ping(m) >= interval-1))
		        || (sp_flagged(m, SP_INBOX)
			    && (now-sp_last_ping(m) >= interval-1))))
				    ? " [it's time]" :
		   (expunged_reaper_to
		    && sp_flagged(m, SP_LOCKED)
		    && (now-sp_last_expunged_reaper(m) >= expunged_reaper_to))
				    ? " [expunged reaper]" :
		   (expunged_reaper_to
		    && !sp_flagged(m, SP_LOCKED)
		    && since_last_input >= 3
		    && (now-sp_last_ping(m) >= expunged_reaper_to))
				     ? " [slow ping]" : " [keepalive]",
		   since_last_input,
		   debug_time(0,1)));

	    /*
	     * We're about to ping the stream.
	     * If the stream is a #move Mail Drop there is a minimum time
	     * between re-opens of the mail drop to check for new mail.
	     * If the check is forced by the user, they want it to
	     * happen now. We use knowledge of c-client internals to
	     * make this happen.
	     */
	    if(force_arg && m && m->snarf.name)
	      m->snarf.time = 0;

	    /*-- Ping the stream to check for new mail --*/
	    if(sp_dead_stream(m)){
		dprint((6, "no check: stream is dead\n"));
	    }
	    else if(!pine_mail_ping(m)){
		dprint((6, "ping failed: stream is dead\n"));
		sp_set_dead_stream(m, 1);
	    }

	    dprint((7, "Ping complete: %s\n", debug_time(0,1)));

	    if((flags & NM_STATUS_MSG) && pith_opt_newmail_check_cue)
	      (*pith_opt_newmail_check_cue)(FALSE);
	}
    } while(i != started_on);

    /*
     * Current mail box state changed, could be additions or deletions.
     * Also check if we need to do sorting that has been deferred.
     * We handle the current stream separately from the rest in the
     * similar loop that follows this paragraph.
     */
    m = ps_global->mail_stream;
    if(sp_mail_box_changed(m) || sp_unsorted_newmail(m)
       || sp_need_to_rethread(m)){
        dprint((7,
	 "Cur new mail, %s, new_mail_count: %ld expunge: %ld, max_msgno: %ld\n",
		   (m && m->mailbox) ? m->mailbox : "?",
		   sp_new_mail_count(m),
		   sp_expunge_count(m),
		   mn_get_total(sp_msgmap(m))));

	new_mail_was_announced = 0;
	if(sp_mail_box_changed(m))
	  fixup_flags(m, sp_msgmap(m));

        if(sp_new_mail_count(m))
	  process_filter_patterns(m, sp_msgmap(m), sp_new_mail_count(m));

	/* worry about sorting */
        if((sp_new_mail_count(m) > 0L
	    || sp_unsorted_newmail(m)
	    || sp_need_to_rethread(m))
	   && !((flags & NM_DEFER_SORT)
		|| any_lflagged(sp_msgmap(m), MN_HIDE)))
	  refresh_sort(m, sp_msgmap(m),
		       (flags & NM_STATUS_MSG) ? SRT_VRB : SRT_NON);
        else if(sp_new_mail_count(m) > 0L)
	  sp_set_unsorted_newmail(m, 1);

        if(sp_new_mail_count(m) > 0L){
            sp_set_mail_since_cmd(m, sp_mail_since_cmd(m)+sp_new_mail_count(m));
	    rv += (t_nm_count = sp_new_mail_count(m));
	    sp_set_new_mail_count(m, 0L);

	    if((flags & NM_STATUS_MSG) && pith_opt_newmail_announce){
		for(n = m->nmsgs; n > 1L; n--)
		  if(!get_lflag(m, NULL, n, MN_EXLD))
		    break;

		(*pith_opt_newmail_announce)(m, n, t_nm_count);

		if(n)
		  new_mail_was_announced++;
	    }
        }

	update_folder_unseen_by_stream(m, new_mail_was_announced ? UFU_NONE : UFU_ANNOUNCE);

	if(flags & NM_STATUS_MSG)
	  sp_set_mail_box_changed(m, 0);
    }

    /* the rest of the streams */
    for(i = 0; i < ps_global->s_pool.nstream; i++){
	m = ps_global->s_pool.streams[i];
	if(!m || m == ps_global->mail_stream)
	  continue;

	if(sp_mail_box_changed(m)){
	    /*-- New mail for this stream, queue up the notification --*/
	    dprint((7,
	     "New mail, %s, new_mail_count: %ld expunge: %ld, max_msgno: %ld\n",
		       (m && m->mailbox) ? m->mailbox : "?",
		       sp_new_mail_count(m),
		       sp_expunge_count(m),
		       mn_get_total(sp_msgmap(m))));

	    new_mail_was_announced = 0;
	    fixup_flags(m, sp_msgmap(m));

	    if(sp_new_mail_count(m))
	      process_filter_patterns(m, sp_msgmap(m), sp_new_mail_count(m));

	    if(sp_new_mail_count(m) > 0){
		sp_set_unsorted_newmail(m, 1);
		sp_set_mail_since_cmd(m, sp_mail_since_cmd(m) +
					 sp_new_mail_count(m));
		if(sp_flagged(m, SP_LOCKED) && sp_flagged(m, SP_USERFLDR))
		  rv += (t_nm_count = sp_new_mail_count(m));

		sp_set_new_mail_count(m, 0L);

		/* messages only for user's streams */
		if(flags & NM_STATUS_MSG
		   && pith_opt_newmail_announce
		   && sp_flagged(m, SP_LOCKED)
		   && sp_flagged(m, SP_USERFLDR)){
		    for(n = m->nmsgs; n > 1L; n--)
		      if(!get_lflag(m, NULL, n, MN_EXLD))
			break;

		    (*pith_opt_newmail_announce)(m, n, t_nm_count);

		    if(n)
		      new_mail_was_announced++;
		}
	    }

	    update_folder_unseen_by_stream(m, new_mail_was_announced ? UFU_NONE : UFU_ANNOUNCE);

	    if(flags & NM_STATUS_MSG)
	      sp_set_mail_box_changed(m, 0);
	}
    }

    /* so quit_screen can tell new mail from expunged mail */
    exp_count = sp_expunge_count(ps_global->mail_stream);

    /* see if we want to kill any cached streams */
    for(i = 0; i < ps_global->s_pool.nstream; i++){
	m = ps_global->s_pool.streams[i];
	if(sp_last_ping(m) >= now)
          maybe_kill_old_stream(m);
    }

    /*
     * This is here to prevent banging on the down arrow key (or holding
     * it down and repeating) at the end of the index from causing
     * a whole bunch of new mail checks. Last_nextitem_forcechk does get
     * set at the place it is tested in mailcmd.c, but this is here to
     * reset it to a little bit later in case it takes a second or more
     * to check for the mail. If we didn't do this, we'd just check every
     * keystroke as long as the checks took more than a second.
     */
    if(force_arg)
      ps_global->last_nextitem_forcechk = time(0);

    dprint((6, "******** new mail returning %ld  ********\n", 
	       rv ? rv : (exp_count ? 0 : -1)));
    return(rv ? rv : (exp_count ? 0 : -1));
}


/*
 * format_new_mail_msg - actual work of generating intro,
 *			 from, subject and expanded subject
 */
void
format_new_mail_msg(char *folder, long int number, ENVELOPE *e,
		    char *intro, char *from, char *subj, char *subjex,
		    size_t buflen) /* min length of each of the 4 above if they're non-null */
{
    char *p, tmp[MAILTMPLEN+1], subj_leadin[MAILTMPLEN];
    static char *carray[] = { "regarding",
				"concerning",
				"about",
				"as to",
				"as regards",
				"as respects",
				"in re",
				"re",
				"respecting",
				"in point of",
				"with regard to",
				"subject:"
    };

    if(buflen > 0){
	if(intro)
	  intro[0] = '\0';

	if(from)
	  from[0] = '\0';

	if(subj)
	  subj[0] = '\0';

	if(subjex)
	  subjex[0] = '\0';
    }

    if(from && e && e->from){
        if(e->from->personal && e->from->personal[0]){
	    /*
	     * The reason we use so many characters for tmp is because we
	     * may have multiple MIME3 chunks and we don't want to truncate
	     * in the middle of one of them before decoding.
	     */
	    snprintf(tmp, sizeof(tmp), "%s", e->from->personal);
 	    p = (char *) rfc1522_decode_to_utf8((unsigned char *) tmp_20k_buf, SIZEOF_20KBUF, tmp);
	    removing_leading_and_trailing_white_space(p);
	    if(*p)
 	      snprintf(from, buflen, "%.200s", p);
 	}

 	if(!from[0]){
 	    snprintf(tmp, sizeof(tmp), "%s%s%s", 
 		    e->from->mailbox,
 		    e->from->host ? "@" : "",
 		    e->from->host ? e->from->host : "");
 	    snprintf(from, buflen, "%.200s", tmp);
 	}
    }

    if(number <= 1L){
        if(e && e->subject && e->subject[0]){
 	    snprintf(tmp, sizeof(tmp), "%s", e->subject);
	    p = (char *) rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf, SIZEOF_20KBUF, tmp);
	    if(subj)
 	      snprintf(subj, buflen, "%.200s", p);
	}

	snprintf(subj_leadin, sizeof(subj_leadin), " %s ", carray[(unsigned)random()%12]);
        if(!(from && from[0]))
          subj_leadin[1] = toupper((unsigned char)subj_leadin[1]);
    }

    if(subj && subjex){
	if(subj[0]){
	    snprintf(subjex, buflen, "%s%.200s%s",
		    (number <= 1L) ? (subj[0] ? subj_leadin : "")
				   : "",
		    (number <= 1L) ? (subj[0] ? subj
						 : from ? " w" : " W")
				   : "",
		    (number <= 1L) ? (subj[0] ? "" : "ith no subject")
				   : "");
	}
	else
	    subj[0] = subjex[0] = '\0';
    }
      
    if(intro){
	if(!folder){
	    if(number > 1)
	      /* TRANSLATORS: The argument is the number of new messages */
	      snprintf(intro, buflen, _("%ld new messages!"), number);
	    else
	      /* TRANSLATORS: The argument is either " to you" or nothing */
	      snprintf(intro, buflen, _("New mail%s!"),
		      (e && address_is_us(e->to, ps_global)) ? _(" to you") : "");
	}
	else {
	    long fl, tot, newfl;

	    if(number > 1)
	      snprintf(intro, buflen, _("%ld messages saved to folder \"%.80s\""),
		      number, folder);
	    else
	      snprintf(intro, buflen, _("Mail saved to folder \"%.80s\""), folder);
	    
	    if((fl=utf8_width(folder)) > 10 &&
	       (tot=utf8_width(intro) + utf8_width(from ? from : "") + utf8_width(subj ? subj : "")) >
					       ps_global->ttyo->screen_cols - 2){
		newfl = MAX(10, fl-(tot-(ps_global->ttyo->screen_cols - 2)));
		if(number > 1)
		  snprintf(intro, buflen, _("%ld messages saved to folder \"...%.80s\""),
			  number, folder+(fl-(newfl-3)));
		else
		  snprintf(intro, buflen, _("Mail saved to folder \"...%.80s\""),
			  folder+(fl-(newfl-3)));
	    }
	}
    }
}


/*----------------------------------------------------------------------
  Straighten out any local flag problems here.  We can't take care of
  them in the mm_exists or mm_expunged callbacks since the flags
  themselves are in an MESSAGECACHE and we're not allowed to reenter
  c-client from a callback...

 Args: stream -- mail stream to operate on
       msgmap -- messages in that stream to fix up

 Result: returns with local flags as they should be

  ----*/
void
fixup_flags(MAILSTREAM *stream, MSGNO_S *msgmap)
{
    /*
     * Deal with the case where expunged away all of the 
     * zoomed messages.  Unhide everything in that case...
     */
    if(mn_get_total(msgmap) > 0L){
	long i;

	if(any_lflagged(msgmap, MN_HIDE) >= mn_get_total(msgmap)){
	    for(i = 1L; i <= mn_get_total(msgmap); i++)
	      set_lflag(stream, msgmap, i, MN_HIDE, 0);

	    mn_set_cur(msgmap, THREADING()
				 ? first_sorted_flagged(F_NONE, stream, 0L,
				               (THREADING() ? 0 : FSF_SKIP_CHID)
					       | FSF_LAST)
				 : mn_get_total(msgmap));
	}
	else if(any_lflagged(msgmap, MN_HIDE)){
	    /*
	     * if we got here, there are some hidden messages and
	     * some not.  Make sure the current message is one
	     * that's not...
	     */
	    for(i = mn_get_cur(msgmap); i <= mn_get_total(msgmap); i++)
	      if(!msgline_hidden(stream, msgmap, i, 0)){
		  mn_set_cur(msgmap, i);
		  break;
	      }

	    for(i = mn_get_cur(msgmap); i > 0L; i--)
	      if(!msgline_hidden(stream, msgmap, i, 0)){
		  mn_set_cur(msgmap, i);
		  break;
	      }
	}
    }
}


/*----------------------------------------------------------------------
    Force write of the main file so user doesn't lose too much when
 something bad happens. The only thing that can get lost is flags, such 
 as when new mail arrives, is read, deleted or answered.

 Args: timing      -- indicates if it's a good time for to do a checkpoint

  Result: returns 1 if checkpoint was written, 
                  0 if not.

NOTE: mail_check will also notice new mail arrival, so it's imperative that
code exist after this function is called that can deal with updating the 
various pieces of pine's state associated with the message count and such.

Only need to checkpoint current stream because there are no changes going
on with other streams when we're not manipulating them.
  ----*/

int
check_point(MAILSTREAM *stream, CheckPointTime timing, int flags)
{
    int     freq, tm, check_count, tst1 = 0, tst2 = 0, accel;
    long    since_last_input, since_first_change;
    time_t  now, then;
#define AT_LEAST (180)
#define MIN_LONG_CHK_IDLE (10)

    if(!stream || stream->rdonly || stream->halfopen
       || (check_count = sp_check_cnt(stream)) == 0)
      return(0);

    since_last_input = (long) time(0) - (long) time_of_last_input();
    
    if(timing == GoodTime && since_last_input <= 0)
      timing = VeryBadTime;
    else if(timing == GoodTime && since_last_input <= 4)
      timing = BadTime;

    dprint((9, "check_point(%s: %s)\n", 
               timing == GoodTime ? "GoodTime" :
               timing == BadTime  ? "BadTime" :
               timing == VeryBadTime  ? "VeryBadTime" : "DoItNow",
	       STREAMNAME(stream)));

    freq = CHECK_POINT_FREQ * (timing==GoodTime ? 1 : timing==BadTime ? 3 : 4);
    tm   = CHECK_POINT_TIME * (timing==GoodTime ? 2 : timing==BadTime ? 4 : 6);
    tm   = MAX(100, tm);

    if(timing == DoItNow){
	dprint((9, "DoItNow\n"));
    }
    else{
	since_first_change = (long) (time(0) - sp_first_status_change(stream));
	accel = MIN(3, MAX(1, (4 * since_first_change)/tm));
	tst1 = ((check_count * since_last_input) >= (30/accel) * freq);
	tst2 = ((since_first_change >= tm)
                && (since_last_input >= MIN_LONG_CHK_IDLE));
	dprint((9,
		"Chk changes(%d) x since_last_input(%ld) (=%ld) >= freq(%d) x 30/%d (=%d) ? %s\n",
		check_count, since_last_input,
		check_count * since_last_input, freq, accel, (30/accel)*freq,
		tst1 ? "Yes" : "No"));

	dprint((9,
		"    or since_1st_change(%ld) >= tm(%d) && since_last_input >= %d ? %s\n",
		since_first_change, tm, MIN_LONG_CHK_IDLE,
		tst2 ? "Yes" : "No"));
    }

    if(timing == DoItNow || tst1 || tst2){

	if(timing == DoItNow
	   || time(0) - sp_last_chkpnt_done(stream) >= AT_LEAST){
	    then = time(0);
	    dprint((2, "Checkpoint: %s  Since 1st change: %ld secs  idle: %ld secs\n",
		   debug_time(0,1),
		   (long) (then - sp_first_status_change(stream)),
		   since_last_input))
;
	    if((flags & NM_STATUS_MSG) && pith_opt_checkpoint_cue)
	      (*pith_opt_checkpoint_cue)(TRUE);

	    pine_mail_check(stream);		/* write file state */

	    now = time(0);
	    dprint((2,
		    "Checkpoint complete: %s%s%s%s\n",
		    debug_time(0,1),
		    (now-then > 0) ? " (elapsed: " : "",
		    (now-then > 0) ? comatose((long)(now-then)) : "",
		    (now-then > 0) ? " secs)" : ""));

	    if((flags & NM_STATUS_MSG) && pith_opt_checkpoint_cue)
	      (*pith_opt_checkpoint_cue)(FALSE);

	    return(1);
	}
	else{
	    dprint((9,
	      "Skipping checkpoint since last was only %ld secs ago (< %d)\n",
	      (long) (time(0) - sp_last_chkpnt_done(stream)), AT_LEAST));
	}
    }

    return(0);
}


/*----------------------------------------------------------------------
    Call this when we need to tell the check pointing mechanism about
  mailbox state changes.
  ----------------------------------------------------------------------*/
void
check_point_change(MAILSTREAM *stream)
{
    if(!sp_check_cnt(stream))
      sp_set_first_status_change(stream, time(0));

    sp_set_check_cnt(stream, sp_check_cnt(stream)+1);
    dprint((9, "check_point_change(%s): increment to %d\n",
	   STREAMNAME(stream), sp_check_cnt(stream)));
}



/*----------------------------------------------------------------------
    Call this when a mail file is written to reset timer and counter
  for next check_point.
  ----------------------------------------------------------------------*/
void
reset_check_point(MAILSTREAM *stream)
{
    time_t now;

    now = time(0);

    sp_set_check_cnt(stream, 0);
    sp_set_first_status_change(stream, 0);
    sp_set_last_chkpnt_done(stream, now);
    sp_set_last_ping(stream, now);
    sp_set_last_expunged_reaper(stream, now);
}


int
changes_to_checkpoint(MAILSTREAM *stream)
{
    return(sp_check_cnt(stream) > 0);
}



/*----------------------------------------------------------------------
    Zero the counters that keep track of mail accumulated between
   commands.
 ----*/
void
zero_new_mail_count(void)
{
    int         i;
    MAILSTREAM *m;

    dprint((9, "New_mail_count zeroed\n"));

    for(i = 0; i < ps_global->s_pool.nstream; i++){
	m = ps_global->s_pool.streams[i];
	if(m && sp_flagged(m, SP_LOCKED) && sp_flagged(m, SP_USERFLDR)
	   && sp_mail_since_cmd(m)){
	    if(pith_opt_icon_text)
	      (*pith_opt_icon_text)(NULL, IT_NEWMAIL);

	    break;
	}
    }

    for(i = 0; i < ps_global->s_pool.nstream; i++){
	m = ps_global->s_pool.streams[i];
	if(m && sp_flagged(m, SP_LOCKED))
	  sp_set_mail_since_cmd(m, 0L);
    }
}
