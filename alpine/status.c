#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: status.c 840 2007-12-01 01:34:49Z hubert@u.washington.edu $";
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
     status.c
     Functions that manage the status line (third from the bottom)
       - put messages on the queue to be displayed
       - display messages on the queue with timers 
       - check queue to figure out next timeout
       - prompt for yes/no type of questions
  ====*/

#include "headers.h"
#include "status.h"
#include "keymenu.h"
#include "mailview.h"
#include "mailcmd.h"
#include "busy.h"
#include "after.h"

#include "../pith/charconv/utf8.h"

#include "../pith/conf.h"
#include "../pith/bitmap.h"


/*
 * Internal queue of messages.  The circular, double-linked list's 
 * allocated on demand, and cleared as each message is displayed.
 */
typedef struct message {
    char	   *text;
    unsigned	    flags:8;
    unsigned	    shown:1;
    unsigned	    saw_it:1;
    unsigned	    pending_removal:1;
    int		    min_display_time, max_display_time;
    struct message *next, *prev;
} SMQ_T;


/*
 * Internal prototypes
 */
void      pause_for_and_dq_cur_msg(void);
SMQ_T    *top_of_queue(void);
int       new_info_msg_need_not_be_queued(void);
int       is_last_message(SMQ_T *);
void      d_q_status_message(void);
int       output_message(SMQ_T *, int);
int       output_message_modal(SMQ_T *, int);
void      delay_cmd_cue(int);
int       modal_bogus_input(UCS);
int	  messages_in_queue(void);
int       status_message_remaining_nolock(void);
void      set_saw_it_to_zero();
void      mark_modals_done();
SMQ_T    *copy_status_queue(SMQ_T *);



/*----------------------------------------------------------------------
     Manage the second line from the bottom where status and error messages
are displayed. A small queue is set up and messages are put on the queue
by calling one of the q_status_message routines. Even though this is a queue
most of the time message will go right on through. The messages are 
displayed just before the read for the next command, or when a read times
out. Read timeouts occur every minute or so for new mail checking and every
few seconds when there are still messages on the queue. Hopefully, this scheme 
will not let messages fly past that the user can't see.
  ----------------------------------------------------------------------*/


static SMQ_T *message_queue = NULL;
static short  needs_clearing = 0, /* Flag set by want_to()
                                              and optionally_enter() */
	      prevstartcol, prevendcol;
static char   prevstatusbuf[6*MAX_SCREEN_COLS+1];
static time_t displayed_time;


/*----------------------------------------------------------------------
        Put a message for the status line on the queue

  Args: time    -- the min time in seconds to display the message
        message -- message string

  Result: queues message on queue represented by static variables

    This puts a single message on the queue to be shown.
  ----------*/
void
q_status_message(int flags, int min_time, int max_time, char *message)
{
    SMQ_T *new, *q;
    char  *clean_msg;
    size_t mlen;

    status_message_lock();

    /*
     * If there is already a message queued and
     * new message is just informational, discard it.
     */
    if(flags & SM_INFO && new_info_msg_need_not_be_queued()){
	status_message_unlock();
	return;
    }

    /*
     * By convention, we have min_time equal to zero in messages which we
     * think are not as important, so-called comfort messages. We have
     * min_time >= 3 for messages which we think the user should see for
     * sure. Some users don't like to wait so we've provided a way for them
     * to live on the edge.
     *    status_msg_delay == -1  => min time == MIN(0, min_time)
     *    status_msg_delay == -2  => min time == MIN(1, min_time)
     *    status_msg_delay == -3  => min time == MIN(2, min_time)
     *    ...
     */
    if(ps_global->status_msg_delay < 0)
      min_time = MIN(-1 - ps_global->status_msg_delay, min_time);

    /* The 40 is room for 40 escaped control characters */
    mlen = strlen(message) + 40;
    clean_msg = (char *)fs_get(mlen + 1);
    iutf8ncpy(clean_msg, message, mlen);		/* does the cleaning */

    clean_msg[mlen] = '\0';

    if((q = message_queue) != NULL){	/* the queue exists */

	/*
	 * Scan through all of the messages currently in the queue.
	 * If the new message is already queued, don't add it again.
	 */
	do {
	    if(!q->pending_removal && q->text && !strcmp(q->text, clean_msg)){
		q->shown = 0;
		if(q->min_display_time < min_time)
		  q->min_display_time = min_time;

		if(q->max_display_time < max_time)
		  q->max_display_time = max_time;

		dprint((9, "q_status_message(%s): skipping duplicate msg\n",
		       clean_msg ? clean_msg : "?"));

		if(clean_msg)
		  fs_give((void **)&clean_msg);

		status_message_unlock();
		return;
	    }

	    q = q->next;

	} while(q != message_queue);
    }

    new = (SMQ_T *)fs_get(sizeof(SMQ_T));
    memset(new, 0, sizeof(SMQ_T));
    new->text = clean_msg;
    new->min_display_time = min_time;
    new->max_display_time = max_time;
    new->flags            = flags;
    if(message_queue){
	new->next = message_queue;
	new->prev = message_queue->prev;
	new->prev->next = message_queue->prev = new;
    }
    else
      message_queue = new->next = new->prev = new;

    status_message_unlock();

    dprint((9, "q_status_message(%s)\n",
	   clean_msg ? clean_msg : "?"));
}


/*----------------------------------------------------------------------
     Mark the status line as dirty so it gets cleared next chance
 ----*/
void
mark_status_dirty(void)
{
    mark_status_unknown();
    needs_clearing++;
}


/*----------------------------------------------------------------------
    Cause status line drawing optimization to be turned off, because we
    don't know what the status line looks like.
 ----*/
void
mark_status_unknown(void)
{
    prevstartcol = -1;
    prevendcol = -1;
    prevstatusbuf[0]  = '\0';
}


/*----------------------------------------------------------------------
     Wait a suitable amount of time for the currently displayed message
 ----*/
void
pause_for_and_dq_cur_msg(void)
{
    if(top_of_queue()){
	int w;

	if((w = status_message_remaining_nolock()) != 0){
	    delay_cmd_cue(1);
	    sleep(w);
	    delay_cmd_cue(0);
	}

	d_q_status_message();
    }
}


/*
 * This relies on the global displayed_time being properly set
 * for this message.
 */
void
pause_for_and_mark_specific_msg(SMQ_T *msg)
{
    if(msg){
	int w;

	w = (int) (displayed_time - time(0)) + msg->min_display_time;
	w = (w > 0) ? w : 0;
	if(w){
	    delay_cmd_cue(1);
	    sleep(w);
	    delay_cmd_cue(0);
	}

	msg->pending_removal = 1;
    }
}


/*----------------------------------------------------------------------
    Time remaining for current message's minimum display
 ----*/
int
status_message_remaining(void)
{
    int ret;

    status_message_lock();
    ret = status_message_remaining_nolock();
    status_message_unlock();

    return(ret);
}


int
status_message_remaining_nolock(void)
{
    SMQ_T *q;
    int d = 0;

    if((q = top_of_queue()) != NULL)
      d = (int) (displayed_time - time(0)) + q->min_display_time;

    return((d > 0) ? d : 0);
}


/*
 * Return first message in queue that isn't pending_removal.
 */
SMQ_T *
top_of_queue(void)
{
    SMQ_T *p;

    if((p = message_queue) != NULL){
	do
	  if(!p->pending_removal)
	    return(p);
	while((p = p->next) != message_queue);
    }

    return(NULL);
}


int
new_info_msg_need_not_be_queued(void)
{
    SMQ_T *q;

    if(status_message_remaining_nolock() > 0)
      return 1;

    if((q = top_of_queue()) != NULL && (q = q->next) != message_queue){
	do
	  if(!q->pending_removal && !(q->flags & SM_INFO))
	    return 1;
	while((q = q->next) != message_queue);
    }

    return 0;
}


/*----------------------------------------------------------------------
        Find out how many messages are queued for display

  Args:   dtime -- will get set to minimum display time for current message

  Result: number of messages in the queue.

  ---------*/
int
messages_queued(long int *dtime)
{
    SMQ_T *q;
    int ret;

    status_message_lock();
    if(dtime && (q = top_of_queue()) != NULL)
      *dtime = (long) MAX(q->min_display_time, 1L);

    ret = ps_global->in_init_seq ? 0 : messages_in_queue();

    status_message_unlock();

    return(ret);
}


/*----------------------------------------------------------------------
       Return number of messages in queue
  ---------*/
int
messages_in_queue(void)
{
    int	   n = 0;
    SMQ_T *p;

    if((p = message_queue) != NULL){
	do
	  if(!p->pending_removal)
	    n++;
	while((p = p->next) != message_queue);
    }

    return(n);
}


/*----------------------------------------------------------------------
     Return last message queued
  ---------*/
char *
last_message_queued(void)
{
    SMQ_T *p, *r = NULL;
    char  *ret = NULL;

    status_message_lock();
    if((p = message_queue) != NULL){
	do
	  if(p->flags & SM_ORDER && !p->pending_removal)
	    r = p;
	while((p = p->next) != message_queue);
    }

    ret = (r && r->text) ? cpystr(r->text) : NULL;

    status_message_unlock();

    return(ret);
}


int
is_last_message(SMQ_T *msg)
{
    SMQ_T *p, *r = NULL;

    if(msg && !msg->pending_removal){
	if((p = message_queue) != NULL){
	    do
	      if(!p->pending_removal)
		r = p;
	    while((p = p->next) != message_queue);
	}
    }

    return(r && msg && (r == msg));
}


/*----------------------------------------------------------------------
       Update status line, clearing or displaying a message

   Arg: command -- The command that is about to be executed

  Result: status line cleared or
             next message queued is displayed or
             current message is redisplayed.
	     if next message displayed, it's min display time
	     is returned else if message already displayed, it's
	     time remaining on the display is returned, else 0.

   This is called when ready to display the next message, usually just
before reading the next command from the user. We pass in the nature
of the command because it affects what we do here. If the command just
executed by the user is a redraw screen, we don't want to reset or go to 
next message because it might not have been seen.  Also if the command
is just a noop, which are usually executed when checking for new mail 
and happen every few minutes, we don't clear the message.

If it was really a command and there's nothing more to show, then we
clear, because we know the user has seen the message. In this case the
user might be typing commands very quickly and miss a message, so
there is a time stamp and time check that each message has been on the
screen for a few seconds.  If it hasn't we just return and let it be
taken care of next time.
----------------------------------------------------------------------*/
int
display_message(UCS command)
{
    SMQ_T *q, *copy_of_q;
    int need_to_unlock;
    int ding;

    if(ps_global == NULL || ps_global->ttyo == NULL
       || ps_global->ttyo->screen_rows < 1 || ps_global->in_init_seq)
      return(0);

    status_message_lock();

    /*---- Deal with any previously displayed message ----*/
    if((q = top_of_queue()) != NULL && q->shown){
	int rv = -1;

	if(command == ctrl('L')){	/* just repaint it, and go on */
	    mark_status_unknown();
	    mark_keymenu_dirty();
	    mark_titlebar_dirty();
	    rv = 0;
	}
	else{				/* ensure sufficient time's passed */
	    time_t now;
	    int    diff;

	    now  = time(0);
	    diff = (int)(displayed_time - now)
			+ ((command == NO_OP_COMMAND || command == NO_OP_IDLE)
			    ? q->max_display_time
			    : q->min_display_time);
            dprint((9,
		       "STATUS: diff:%d, displayed: %ld, now: %ld\n",
		       diff, (long) displayed_time, (long) now));
            if(diff > 0)
	      rv = diff;			/* check again next time  */
	    else if(is_last_message(q)
		    && (command == NO_OP_COMMAND || command == NO_OP_IDLE)
		    && q->max_display_time)
	      rv = 0;				/* last msg, no cmd, has max */
	}

	if(rv >= 0){				/* leave message displayed? */
	    if(prevstartcol < 0){		/* need to redisplay it? */
		ding = q->flags & SM_DING;
		q->flags &= ~SM_DING;
		if(q->flags & SM_MODAL && !q->shown){
		    copy_of_q = copy_status_queue(q);
		    mark_modals_done();
		    status_message_unlock();
		    output_message_modal(copy_of_q, ding);
		}
		else{
		    output_message(q, ding);
		    status_message_unlock();
		}
	    }
	    else
	      status_message_unlock();

	    return(rv);
	}
	  
	d_q_status_message();		/* remove it from queue and */
	needs_clearing++;		/* clear the line if needed */
    }

    if(!top_of_queue() && (command == ctrl('L') || needs_clearing)){
	int inverse;
	struct variable *vars = ps_global->vars;
	char *last_bg = NULL;

	dprint((9, "Clearing status line\n"));
	inverse = InverseState();	/* already in inverse? */
	if(inverse && pico_usingcolor() && VAR_STATUS_FORE_COLOR &&
	   VAR_STATUS_BACK_COLOR){
	    last_bg = pico_get_last_bg_color();
	    pico_set_nbg_color();   /* so ClearLine will clear in bg color */
	}

	ClearLine(ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global));
	if(last_bg){
	    (void)pico_set_bg_color(last_bg);
	    if(last_bg)
	      fs_give((void **)&last_bg);
	}

	mark_status_unknown();
	if(command == ctrl('L')){
	    mark_keymenu_dirty();
	    mark_titlebar_dirty();
	}
    }

    need_to_unlock = 1;

    /* display next message, weeding out 0 display times */
    for(q = top_of_queue(); q && !q->shown; q = top_of_queue()){
	if(q->min_display_time || is_last_message(q)){
	    displayed_time = time(0);
	    ding = q->flags & SM_DING;
	    q->flags &= ~SM_DING;
	    if(q->flags & SM_MODAL && !q->shown){
		copy_of_q = copy_status_queue(q);
		mark_modals_done();
		status_message_unlock();
		output_message_modal(copy_of_q, ding);
		need_to_unlock = 0;
	    }
	    else
	      output_message(q, ding);

	    break;
	}
	/* zero display time message, log it, delete it */
	else{
	    if(q->text){
		char   buf[1000];
		char  *append = " [not actually shown]";
		char  *ptr;
		size_t len;

		len = strlen(q->text) + strlen(append);
		if(len < sizeof(buf))
		  ptr = buf;
		else
		  ptr = (char *) fs_get((len+1) * sizeof(char));

		strncpy(ptr, q->text, len);
		ptr[len] = '\0';
		strncat(ptr, append, len+1-1-strlen(ptr));
		ptr[len] = '\0';
		add_review_message(ptr, -1);
		if(ptr != buf)
		  fs_give((void **) &ptr);
	    }

	    d_q_status_message();
	}
    }

    needs_clearing = 0;				/* always cleared or written */
    fflush(stdout);

    if(need_to_unlock)
      status_message_unlock();

    return(0);
}


/*----------------------------------------------------------------------
     Display all the messages on the queue as quickly as possible
  ----*/
void
flush_status_messages(int skip_last_pause)
{
    SMQ_T *q, *copy_of_q;
    int ding;

start_over:
    status_message_lock();

    for(q = top_of_queue(); q; q = top_of_queue()){
	/* don't have to wait for this one */
	if(is_last_message(q) && skip_last_pause && q->shown)
	  break;

	if(q->shown)
	  pause_for_and_dq_cur_msg();

	/* find next one we need to show and show it */
	for(q = top_of_queue(); q && !q->shown; q = top_of_queue()){
	    if((q->min_display_time || is_last_message(q))){
		displayed_time = time(0);
		ding = q->flags & SM_DING;
		q->flags &= ~SM_DING;
		if(q->flags & SM_MODAL){
		    copy_of_q = copy_status_queue(q);
		    mark_modals_done();
		    status_message_unlock();
		    output_message_modal(copy_of_q, ding);

		    /*
		     * Because we unlock the message queue in order
		     * to display modal messages we have to worry
		     * about the queue being changed while we have
		     * it unlocked. So start the whole process over.
		     */
		    goto start_over;
		}
		else
		  output_message(q, ding);
	    }
	    else{
		d_q_status_message();
	    }
	}
    }

    status_message_unlock();
}


/*----------------------------------------------------------------------
     Make sure any and all SM_ORDER messages get displayed.

     Note: This flags the message line as having nothing displayed.
           The idea is that it's a function called by routines that want
	   the message line for a prompt or something, and that they're
	   going to obliterate the message anyway.
 ----*/
void
flush_ordered_messages(void)
{
    SMQ_T *q, *copy_of_q;
    int firsttime = 1;
    int ding;

    status_message_lock();

    set_saw_it_to_zero();

start_over2:
    if(!firsttime)
      status_message_lock();
    else
      firsttime = 0;

    if((q = message_queue) != NULL){
	do{
	    if(q->pending_removal || q->saw_it || q->shown){
		if(!q->pending_removal && q->shown)
		  pause_for_and_mark_specific_msg(q);

		q->saw_it = 1;
		q = (q->next != message_queue) ? q->next : NULL;
	    }
	    else{
		/* find next one we need to show and show it */
		do{
		    if(q->pending_removal || q->saw_it || q->shown){
			q->saw_it = 1;
			q = (q->next != message_queue) ? q->next : NULL;
		    }
		    else{
			if((q->flags & (SM_ORDER | SM_MODAL))
			   && q->min_display_time){
			    displayed_time = time(0);
			    ding = q->flags & SM_DING;
			    q->flags &= ~SM_DING;
			    if(q->flags & SM_MODAL){
				copy_of_q = copy_status_queue(q);
				mark_modals_done();
				q->saw_it = 1;
				status_message_unlock();
				output_message_modal(copy_of_q, ding);
				goto start_over2;
			    }
			    else{
				output_message(q, ding);
			    }
			}
			else{
			    q->saw_it = 1;
			    if(!(q->flags & SM_ASYNC))
			      q->pending_removal = 1;

			    q = (q->next != message_queue) ? q->next : NULL;
			}
		    }
		}while(q && !q->shown);
	    }
	}while(q);
    }

    status_message_unlock();
}


void
set_saw_it_to_zero()
{
    SMQ_T *q;

    /* set saw_it to zero */
    if((q = message_queue) != NULL){
	do{
	    q->saw_it = 0;
	    q = (q->next != message_queue) ? q->next : NULL;
	}while(q);
    }
}


void
mark_modals_done()
{
    SMQ_T *q;

    /* set shown to one */
    if((q = message_queue) != NULL){
	do{
	    if(q->flags & SM_MODAL){
		q->shown = 1;
		q->pending_removal = 1;
	    }

	    q = (q->next != message_queue) ? q->next : NULL;
	}while(q);
    }
}


/*
 * Caller needs to free the memory.
 */
SMQ_T *
copy_status_queue(SMQ_T *start)
{
    SMQ_T *q, *new, *head = NULL;

    if((q = start) != NULL){
	do{
	    new = (SMQ_T *) fs_get(sizeof(SMQ_T));
	    *new = *q;

	    if(q->text)
	      new->text = cpystr(q->text);
	    else
	      new->text = NULL;

	    if(head){
		new->next = head;
		new->prev = head->prev;
		new->prev->next = head->prev = new;
	    }
	    else
	      head = new->next = new->prev = new;

	    q = (q->next != start) ? q->next : NULL;
	}while(q);
    }

    return(head);
}


/*----------------------------------------------------------------------
      Removes the first message from the message queue.
      Returns 0 if all was well, -1 if had to skip the removal and
      message_queue is unchanged.
  ----*/
void
d_q_status_message(void)
{
    int the_last_one = 0;
    SMQ_T *q, *p, *last_one;

    /* mark the top one for removal */
    if((p = top_of_queue()) != NULL)
      p->pending_removal = 1;

    if(message_queue){

	/* flush out pending removals */
	the_last_one = 0;	/* loop control */
	q = message_queue;
	last_one = message_queue->prev;
	while(!the_last_one){
	    if(q == last_one)
	      the_last_one++;

	    if(q->pending_removal){
		p = q;
		if(q == q->next){	/* last item in queue */
		    q = message_queue = NULL;
		}
		else{
		    p->next->prev = p->prev;
		    q = p->prev->next = p->next;	/* here's the increment */
		    if(message_queue == p)
		      message_queue = q;
		}

		if(p){
		    if(p->text)
		      fs_give((void **) &p->text);

		    fs_give((void **) &p);
		}
	    }
	    else
	      q = q->next;
	}
    }
}


/*----------------------------------------------------------------------
    Actually output the message to the screen

  Args: message            -- The message to output
	from_alarm_handler -- Called from alarm signal handler.
			      We don't want to add this message to the review
			      message list since we may mess with the malloc
			      arena here, and the interrupt may be from
			      the middle of something malloc'ing.
 ----*/
int
status_message_write(char *message, int from_alarm_handler)
{
    int  col, row, max_width, invert;
    int bytes;
    char newstatusbuf[6*MAX_SCREEN_COLS + 1];
    struct variable *vars = ps_global->vars;
    COLOR_PAIR *lastc = NULL, *newc;

    if(!from_alarm_handler)
      add_review_message(message, -1);

    invert = !InverseState();	/* already in inverse? */
    row = MAX(0, ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global));

    /* Put [] around message and truncate to screen width */
    max_width = ps_global->ttyo != NULL ? ps_global->ttyo->screen_cols : 80;
    max_width = MIN(max_width, MAX_SCREEN_COLS);
    newstatusbuf[0] = '[';
    newstatusbuf[1] = '\0';

    bytes = utf8_to_width(newstatusbuf+1, message, sizeof(newstatusbuf)-1, max_width-2, NULL);
    newstatusbuf[1+bytes] = ']';
    newstatusbuf[1+bytes+1] = '\0';

    if(prevstartcol == -1 || strcmp(newstatusbuf, prevstatusbuf)){
	UCS *prevbuf = NULL, *newbuf = NULL;
	size_t plen;

	if(prevstartcol != -1){
	    prevbuf = utf8_to_ucs4_cpystr(prevstatusbuf);
	    newbuf  = utf8_to_ucs4_cpystr(newstatusbuf);
	}

	/*
	 * Simple optimization.  If the strings are the same length
	 * and width just skip leading and trailing strings of common
	 * characters and only write whatever's in between. Otherwise,
	 * write out the whole thing.
	 * We could do something more complicated but the odds of
	 * getting any optimization goes way down if they aren't the
	 * same length and width and the complexity goes way up.
	 */
	if(prevbuf && newbuf
	   && (plen=ucs4_strlen(prevbuf)) == ucs4_strlen(newbuf)
	   && ucs4_str_width(prevbuf) == ucs4_str_width(newbuf)){
	    UCS *start_of_unequal, *end_of_unequal;
	    UCS *pprev, *endnewbuf;
	    char *to_screen = NULL;
	    int column;

	    pprev = prevbuf;
	    start_of_unequal = newbuf;
	    endnewbuf = newbuf + plen;
	    col = column = prevstartcol;

	    while(start_of_unequal < endnewbuf && (*start_of_unequal) == (*pprev)){
		int w;

		w = wcellwidth(*start_of_unequal);
		if(w >= 0)
		  column += w;

		pprev++;
		start_of_unequal++;
	    }

	    end_of_unequal = endnewbuf-1;
	    pprev = prevbuf + plen - 1;

	    while(end_of_unequal > start_of_unequal && (*end_of_unequal) == (*pprev)){
		*end_of_unequal = '\0';
		pprev--;
		end_of_unequal--;
	    }

	    if(pico_usingcolor() && VAR_STATUS_FORE_COLOR &&
	       VAR_STATUS_BACK_COLOR &&
	       pico_is_good_color(VAR_STATUS_FORE_COLOR) &&
	       pico_is_good_color(VAR_STATUS_BACK_COLOR)){
		lastc = pico_get_cur_color();
		if(lastc){
		    newc = new_color_pair(VAR_STATUS_FORE_COLOR,
					  VAR_STATUS_BACK_COLOR);
		    (void)pico_set_colorp(newc, PSC_NONE);
		    free_color_pair(&newc);
		}
	    }
	    else if(invert)
	      StartInverse();

	    /* PutLine wants UTF-8 string */
	    if(start_of_unequal && (*start_of_unequal))
	      to_screen = ucs4_to_utf8_cpystr(start_of_unequal);

	    if(to_screen){
		PutLine0(row, column, to_screen);
		fs_give((void **) &to_screen);

		strncpy(prevstatusbuf, newstatusbuf, sizeof(prevstatusbuf));
		prevstatusbuf[sizeof(prevstatusbuf)-1] = '\0';
	    }

	    if(lastc){
		(void)pico_set_colorp(lastc, PSC_NONE);
		free_color_pair(&lastc);
	    }
	    else if(invert)
	      EndInverse();
	}
	else{
	    if(pico_usingcolor())
	      lastc = pico_get_cur_color();

	    if(!invert && pico_usingcolor() && VAR_STATUS_FORE_COLOR &&
	       VAR_STATUS_BACK_COLOR &&
	       pico_is_good_color(VAR_STATUS_FORE_COLOR) &&
	       pico_is_good_color(VAR_STATUS_BACK_COLOR))
	      pico_set_nbg_color();	/* so ClearLine uses bg color */

	    ClearLine(row);

	    if(pico_usingcolor() && VAR_STATUS_FORE_COLOR &&
	       VAR_STATUS_BACK_COLOR &&
	       pico_is_good_color(VAR_STATUS_FORE_COLOR) &&
	       pico_is_good_color(VAR_STATUS_BACK_COLOR)){
		if(lastc){
		    newc = new_color_pair(VAR_STATUS_FORE_COLOR,
					  VAR_STATUS_BACK_COLOR);
		    (void)pico_set_colorp(newc, PSC_NONE);
		    free_color_pair(&newc);
		}
	    }
	    else if(invert){
		if(lastc)
		  free_color_pair(&lastc);

		StartInverse();
	    }

	    col = Centerline(row, newstatusbuf);

	    if(lastc){
		(void)pico_set_colorp(lastc, PSC_NONE);
		free_color_pair(&lastc);
	    }
	    else if(invert)
	      EndInverse();

	    strncpy(prevstatusbuf, newstatusbuf, sizeof(prevstatusbuf));
	    prevstatusbuf[sizeof(prevstatusbuf)-1] = '\0';
	    prevstartcol = col;
	    prevendcol = col + utf8_width(prevstatusbuf) - 1;
	}

	/* move cursor to a consistent position */
	if(F_ON(F_SHOW_CURSOR, ps_global))
	  MoveCursor(row, MIN(MAX(0,col+1),ps_global->ttyo->screen_cols-1));
	else
	  MoveCursor(row, 0);

	fflush(stdout);

	if(prevbuf)
	  fs_give((void **) &prevbuf);

	if(newbuf)
	  fs_give((void **) &newbuf);
    }
    else
      col = prevstartcol;

    return(col);
}


/*----------------------------------------------------------------------
    Write the given status message to the display.

  Args: mq_entry -- pointer to message queue entry to write.
 
 ----*/
int 
output_message(SMQ_T *mq_entry, int ding)
{
    int col = 0;

    dprint((9, "output_message(%s)\n",
	   (mq_entry && mq_entry->text) ? mq_entry->text : "?"));

    if(ding && F_OFF(F_QUELL_BEEPS, ps_global)){
	Writechar(BELL, 0);			/* ring bell */
	fflush(stdout);
    }

    if(!(mq_entry->flags & SM_MODAL)){
	col = status_message_write(mq_entry->text, 0);
    	if(ps_global->status_msg_delay > 0){
	    if(F_ON(F_SHOW_CURSOR, ps_global))
	      /* col+1 because col is "[" character */
	      MoveCursor(ps_global->ttyo->screen_rows-FOOTER_ROWS(ps_global),
			 MIN(MAX(0,col+1),ps_global->ttyo->screen_cols-1));
	    else
	      MoveCursor(ps_global->ttyo->screen_rows-FOOTER_ROWS(ps_global), 0);

	    fflush(stdout);
	    sleep(ps_global->status_msg_delay);
	}

	mq_entry->shown = 1;
    }

    return(col);
}


/*
 * This is split off from output_message due to the locking
 * on the status message queue data. This calls scrolltool
 * which can call back into q_status and so on. So instead of
 * keeping the queue locked for a long time we copy the
 * data, unlock the queue, and display the data.
 */
int 
output_message_modal(SMQ_T *mq_entry, int ding)
{
    int rv = 0;
    SMQ_T *m, *mnext;

    if(!mq_entry)
      return(rv);

    dprint((9, "output_message_modal(%s)\n",
	   (mq_entry && mq_entry->text) ? mq_entry->text : "?"));

    if(ding && F_OFF(F_QUELL_BEEPS, ps_global)){
	Writechar(BELL, 0);			/* ring bell */
	fflush(stdout);
    }

    if(!mq_entry->shown){
	int	  i      = 0,
		  pad    = MAX(0, (ps_global->ttyo->screen_cols - 59) / 2);
	char	 *p, *q, *s, *t;
	SCROLL_S  sargs;
	
	/* Count the number of modal messsages and add up their lengths. */
	for(m = mq_entry->next; m != mq_entry; m = m->next)
	  if((m->flags & SM_MODAL) && !m->shown){
	      i++;
	  }

	snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%*s%s\n%*s%s\n%*s%s\n%*s%s\n%*s%s\n%*s%s\n%*s%s\n\n", 
		/*        1         2         3         4         5         6*/
		/*23456789012345678901234567890123456789012345678901234567890*/
		pad, "",
		"***********************************************************",
		pad, "", i ? 
		"* What follows are advisory messages.  After reading them *" :
		"* What follows is an advisory message.  After reading it  *", 
		
		pad, "",
		"* simply hit \"Return\" to continue your Alpine session.    *", 
		pad, "",
		"*                                                         *",
		pad, "", i ?
		"* To review these messages later, press 'J' from the      *" :
		"* To review this message later, press 'J' from the        *",
		pad, "",
		"* MAIN MENU.                                              *",
		pad, "",
		"***********************************************************");
	tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	t = tmp_20k_buf + strlen(tmp_20k_buf);	

	m = mq_entry;
	do{
	    if((m->flags & SM_MODAL) && !m->shown){
		int   indent;

		indent = ps_global->ttyo->screen_cols > 80 
		         ? (ps_global->ttyo->screen_cols - 80) / 3 : 0;
		
		if(t - tmp_20k_buf > 19000){
		    snprintf(t, SIZEOF_20KBUF-(t-tmp_20k_buf), "\n%*s* * *  Running out of buffer space   * * *", indent, "");
		    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		    t += strlen(t);
		    snprintf(t, SIZEOF_20KBUF-(t-tmp_20k_buf), "\n%*s* * * Press RETURN for more messages * * *", indent, "");
		    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		    break;
		}
		
		add_review_message(m->text, -1);
		
		if((p = strstr(m->text, "[ALERT]")) != NULL){
		    snprintf(t, SIZEOF_20KBUF-(t-tmp_20k_buf), "%*.*s\n", indent + p - m->text, p - m->text, m->text);
		    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		    t += strlen(t);

		    for(p += 7; *p && isspace((unsigned char)*p); p++)
		      ;
		    indent += 8;
		}
		else{
		    p = m->text;
		}
		
		while(strlen(p) > ps_global->ttyo->screen_cols - 2 * indent){
		    for(q = p + ps_global->ttyo->screen_cols - 2 * indent; 
			q > p && !isspace((unsigned char)*q); q--)
		      ;

		    snprintf(t, SIZEOF_20KBUF-(t-tmp_20k_buf), "\n%*.*s", indent + q - p, q - p, p);
		    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		    t += strlen(t);
		    p = q + 1;
		}

		snprintf(t, SIZEOF_20KBUF-(t-tmp_20k_buf), "\n%*s%s", indent, "", p);
		tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		t += strlen(t);
		
		if(i--){
		    snprintf(t, SIZEOF_20KBUF-(t-tmp_20k_buf), "\n\n%*s\n", pad + 30, "-  -  -");
		    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		    t += strlen(t);
		}
	    }
	    m = m->next;
	} while(m != mq_entry);

	s = cpystr(tmp_20k_buf);
	ClearLine(ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global));

	memset(&sargs, 0, sizeof(SCROLL_S));
	sargs.text.text	  = s;
	sargs.text.src	  = CharStar;
	sargs.bar.title	  = "Status Message";
	sargs.bogus_input = modal_bogus_input;
	sargs.no_stat_msg = 1;
	sargs.keys.menu   = &modal_message_keymenu;
	setbitmap(sargs.keys.bitmap);

	scrolltool(&sargs);

	fs_give((void **)&s);
	ps_global->mangled_screen = 1;
    }

    /* free the passed in queue */
    m = mq_entry;
    do{
	if(m->text)
	  fs_give((void **) &m->text);

	mnext = m->next;
	fs_give((void **) &m);
	m = mnext;
    } while(m != mq_entry);

    return(rv);
}



/*----------------------------------------------------------------------
    Write or clear delay cue

  Args: on -- whether to turn it on or not
 
 ----*/
void
delay_cmd_cue(int on)
{
    COLOR_PAIR *lastc;
    struct variable *vars = ps_global->vars;

    if(prevstartcol >= 0 && prevendcol < ps_global->ttyo->screen_cols){
	MoveCursor(ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global),
		   MAX(prevstartcol - 1, 0));
	lastc = pico_set_colors(VAR_STATUS_FORE_COLOR, VAR_STATUS_BACK_COLOR,
				PSC_REV|PSC_RET);
	Write_to_screen(on ? (prevstartcol ? "[>" : ">")
			   : (prevstartcol ? " [" : "["));

	MoveCursor(ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global),
		   prevendcol);

	Write_to_screen(on ? (prevendcol < ps_global->ttyo->screen_cols-1 ? "<]" : "<")
			   : (prevendcol < ps_global->ttyo->screen_cols-1 ? "] " : "]"));

	if(lastc){
	    (void)pico_set_colorp(lastc, PSC_NONE);
	    free_color_pair(&lastc);
	}

	if(F_ON(F_SHOW_CURSOR, ps_global))
	  MoveCursor(ps_global->ttyo->screen_rows-FOOTER_ROWS(ps_global),
		     MIN(MAX(0,(prevstartcol + on ? 2 : 1)),ps_global->ttyo->screen_cols-1));
	else
	  MoveCursor(ps_global->ttyo->screen_rows-FOOTER_ROWS(ps_global), 0);
    }

    fflush(stdout);
#ifdef	_WINDOWS
    mswin_setcursor ((on) ? MSWIN_CURSOR_BUSY : MSWIN_CURSOR_ARROW);
#endif
}


/*
 * modal_bogus_input - used by scrolltool to complain about
 *		       invalid user input.
 */
int
modal_bogus_input(UCS ch)
{
    char s[MAX_SCREEN_COLS+1];
		
    snprintf(s, sizeof(s), _("Command \"%s\" not allowed.  Press RETURN to continue Alpine."),
	     pretty_command(ch));
    s[sizeof(s)-1] = '\0';
    status_message_write(s, 0);
    Writechar(BELL, 0);
    return(0);
}
