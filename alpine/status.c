#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: status.c 508 2007-04-03 22:14:39Z hubert@u.washington.edu $";
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
    int		    min_display_time, max_display_time;
    struct message *next, *prev;
} SMQ_T;

#define	LAST_MESSAGE(X)	((X) == (X)->next)


/*
 * Internal prototypes
 */
void      pause_for_current_message(void);
void      d_q_status_message(void);
int       output_message(SMQ_T *);
void      delay_cmd_cue(int);
int       modal_bogus_input(UCS);
int	  messages_in_queue(void);



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
    SMQ_T *new;
    char  *clean_msg;
    size_t mlen;

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

    /* Hunt to last message -- if same already queued, move on... */
    if(new = message_queue){
	while(new->next != message_queue)
	  new = new->next;

	if(!strcmp(new->text, message)){
	    new->shown = 0;

	    if(new->min_display_time < min_time)
	      new->min_display_time = min_time;

	    if(new->max_display_time < max_time)
	      new->max_display_time = max_time;

	    if(clean_msg)
	      fs_give((void **)&clean_msg);

	    return;
	}
	else if(flags & SM_INFO){
	    if(clean_msg)
	      fs_give((void **)&clean_msg);

	    return;
	}
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
pause_for_current_message(void)
{
    if(message_queue){
	int w;

	if(w = status_message_remaining()){
	    delay_cmd_cue(1);
	    sleep(w);
	    delay_cmd_cue(0);
	}

	d_q_status_message();
    }
}


/*----------------------------------------------------------------------
    Time remaining for current message's minimum display
 ----*/
int
status_message_remaining(void)
{
    if(message_queue){
	int d = (int)(displayed_time - time(0))
					  + message_queue->min_display_time;
	return((d > 0) ? d : 0);
    }

    return(0);
}


/*----------------------------------------------------------------------
        Find out how many messages are queued for display

  Args:   dtime -- will get set to minimum display time for current message

  Result: number of messages in the queue.

  ---------*/
int
messages_queued(long int *dtime)
{
    if(message_queue && dtime)
      *dtime = (long)MAX(message_queue->min_display_time, 1L);

    return((ps_global->in_init_seq) ? 0 : messages_in_queue());
}



/*----------------------------------------------------------------------
       Return number of messages in queue
  ---------*/
int
messages_in_queue(void)
{
    int	   n = message_queue ? 1 : 0;
    SMQ_T *p = message_queue;

    while(n && (p = p->next) != message_queue)
      n++;

    return(n);
}



/*----------------------------------------------------------------------
     Return last message queued
  ---------*/
char *
last_message_queued(void)
{
    SMQ_T *p, *r = NULL;

    if(p = message_queue){
	do
	  if(p->flags & SM_ORDER)
	    r = p;
	while((p = p->next) != message_queue);
    }

    return(r ? r->text : NULL);
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

At slow terminal output speeds all of this can be for naught, the amount
of time it takes to paint the screen when the whole screen is being painted
is greater than the second or two delay so the time stamps set here have
nothing to do with when the user actually sees the message.
----------------------------------------------------------------------*/
int
display_message(UCS command)
{
    if(ps_global == NULL || ps_global->ttyo == NULL
       || ps_global->ttyo->screen_rows < 1 || ps_global->in_init_seq)
      return(0);

    /*---- Deal with any previously displayed messages ----*/
    if(message_queue && message_queue->shown) {
	int rv = -1;

	if(command == ctrl('L')) {	/* just repaint it, and go on */
	    mark_status_unknown();
	    mark_keymenu_dirty();
	    mark_titlebar_dirty();
	    rv = 0;
	}
	else {				/* ensure sufficient time's passed */
	    time_t now;
	    int    diff;

	    now  = time(0);
	    diff = (int)(displayed_time - now)
			+ ((command == NO_OP_COMMAND || command == NO_OP_IDLE)
			    ? message_queue->max_display_time
			    : message_queue->min_display_time);
            dprint((9,
		       "STATUS: diff:%d, displayed: %ld, now: %ld\n",
		       diff, (long) displayed_time, (long) now));
            if(diff > 0)
	      rv = diff;			/* check again next time  */
	    else if(LAST_MESSAGE(message_queue)
		    && (command == NO_OP_COMMAND || command == NO_OP_IDLE)
		    && message_queue->max_display_time)
	      rv = 0;				/* last msg, no cmd, has max */
	}

	if(rv >= 0){				/* leave message displayed? */
	    if(prevstartcol < 0)		/* need to redisplay it? */
	      output_message(message_queue);

	    return(rv);
	}
	  
	d_q_status_message();			/* remove it from queue and */
	needs_clearing++;			/* clear the line if needed */
    }

    if(!message_queue && (command == ctrl('L') || needs_clearing)) {
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

    /*---- Display any queued messages, weeding 0 disp times ----*/
    while(message_queue && !message_queue->shown)
      if(message_queue->min_display_time || LAST_MESSAGE(message_queue)){
	  displayed_time = time(0);
	  output_message(message_queue);
      }
      else{
	  if(message_queue->text){
	      char   buf[1000];
	      char  *append = " [not actually shown]";
	      char  *ptr;
	      size_t len;

	      len = strlen(message_queue->text) + strlen(append);
	      if(len < sizeof(buf))
		ptr = buf;
	      else
		ptr = (char *) fs_get((len+1) * sizeof(char));

	      strncpy(ptr, message_queue->text, len);
	      ptr[len] = '\0';
	      strncat(ptr, append, len-strlen(ptr));
	      ptr[len] = '\0';
	      add_review_message(ptr, -1);
	      if(ptr != buf)
		fs_give((void **) &ptr);
	  }

	  d_q_status_message();
      }

    needs_clearing = 0;				/* always cleared or written */
    dprint((9,
               "STATUS cmd:%d, max:%d, min%d\n", command, 
	       (message_queue) ? message_queue->max_display_time : -1,
	       (message_queue) ? message_queue->min_display_time : -1));
    fflush(stdout);
    return(0);
}



/*----------------------------------------------------------------------
     Display all the messages on the queue as quickly as possible
  ----*/
void
flush_status_messages(int skip_last_pause)
{
    while(message_queue){
	if(LAST_MESSAGE(message_queue)
	   && skip_last_pause
	   && message_queue->shown)
	  break;

	if(message_queue->shown)
	  pause_for_current_message();

	while(message_queue && !message_queue->shown)
	  if(message_queue->min_display_time
	     || LAST_MESSAGE(message_queue)){
	      displayed_time = time(0);
	      output_message(message_queue);
	  }
	  else
	    d_q_status_message();
    }
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
    SMQ_T *start = NULL;

    while(message_queue && message_queue != start){
	if(message_queue->shown)
	  pause_for_current_message(); /* changes "message_queue" */

	while(message_queue && !message_queue->shown
	      && message_queue != start)
	  if(message_queue->flags & SM_ORDER){
	      if(message_queue->min_display_time){
		  displayed_time = time(0);
		  output_message(message_queue);
	      }
	      else
		d_q_status_message();
	  }
	  else if(!start)
	    start = message_queue;
    }
}


     
/*----------------------------------------------------------------------
      Remove a message from the message queue.
  ----*/
void
d_q_status_message(void)
{
    if(message_queue){
	dprint((9, "d_q_status_message(%s)\n",
	       message_queue->text ? message_queue->text : "?"));
	if(!LAST_MESSAGE(message_queue)){
	    SMQ_T *p = message_queue;
	    p->next->prev = p->prev;
	    message_queue = p->prev->next = p->next;
	    if(p->text)
	      fs_give((void **)&p->text);

	    fs_give((void **)&p);
	}
	else{
	    if(message_queue->text)
	      fs_give((void **)&message_queue->text);
	    fs_give((void **)&message_queue);
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

	    /* move cursor to a consistent position */
	    MoveCursor(row, 0);
	    fflush(stdout);
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

	    MoveCursor(row, 0);
	    fflush(stdout);
	    strncpy(prevstatusbuf, newstatusbuf, sizeof(prevstatusbuf));
	    prevstatusbuf[sizeof(prevstatusbuf)-1] = '\0';
	    prevstartcol = col;
	    prevendcol = col + utf8_width(prevstatusbuf) - 1;
	}

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
output_message(SMQ_T *mq_entry)
{
    int rv = 0;

    dprint((9, "output_message(%s)\n",
	   (mq_entry && mq_entry->text) ? mq_entry->text : "?"));

    if((mq_entry->flags & SM_DING) && F_OFF(F_QUELL_BEEPS, ps_global))
      Writechar(BELL, 0);			/* ring bell */
      /* flush() handled below */

    if(!(mq_entry->flags & SM_MODAL)){
	rv = status_message_write(mq_entry->text, 0);
    	if(ps_global->status_msg_delay > 0){
	    MoveCursor(ps_global->ttyo->screen_rows-FOOTER_ROWS(ps_global), 0);
	    fflush(stdout);
	    sleep(ps_global->status_msg_delay);
	}

	mq_entry->shown = 1;
    }
    else if (!mq_entry->shown){
	int	  i      = 0,
		  pad    = MAX(0, (ps_global->ttyo->screen_cols - 59) / 2);
	char	 *p, *q, *s, *t;
	SMQ_T	 *m;
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
		
		if (p = strstr(m->text, "[ALERT]")){
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

		m->shown = 1;
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

	MoveCursor(ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global), 0);
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
