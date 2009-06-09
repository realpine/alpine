#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: signal.c 138 2006-09-22 22:12:03Z mikes@u.washington.edu $";
#endif

/* ========================================================================
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
     Implement busy_cue spinner
 ====*/


#include <system.h>
#include <general.h>

#include "../c-client/c-client.h"

#include "../pith/conf.h"
#include "../pith/state.h"
#include "../pith/status.h"
#include "../pith/busy.h"
#include "../pith/debug.h"
#include "../pith/help.h"

#include "../pith/charconv/utf8.h"

#ifdef _WINDOWS
#include "../pico/osdep/mswin.h"
#endif

#include "status.h"
#include "after.h"
#include "busy.h"


static int       dotcount;
static char      busy_message[MAX_BM + 1];
static int       busy_cue_pause;
static int       busy_width;
static int       final_message;
static int       final_message_pri;

static percent_done_t percent_done_ptr;

#define	MAX_SPINNER_WIDTH	32
#define	MAX_SPINNER_ELEMENTS	28

static int spinner = 0;
static struct _spinner {
	int    width, 
	       elements;
	char *bars[MAX_SPINNER_ELEMENTS];
	unsigned used_this_round:1;
} spinners[] = {
    {4, 4, {"<|> ", "</> ", "<-> ", "<\\> "}},
    {11, 4, {"--|-(o)-|--", "--/-(o)-\\--", "----(o)----", "--\\-(o)-/--"}},
    {6, 7, {"\\____/", "_\\__/_", "__\\/__", "__/\\__",
	    "_/__\\_", "/____\\", "|____|"}},
    {4, 4, {"<|> ", "<\\> ", "<-> ", "</> "}},
    {4, 10,{"|   ", " /  ", " _  ", "  \\ ", "  | ", "  | ", "  \\ ",
	    " _  ", " /  ", "|   "}},
    {4, 8, {"_ _ ", "\\ \\ ", " | |", " / /", " _ _", " / /", " | |", "\\ \\ "}},
    {4, 8, {"_   ", "\\   ", " |  ", "  / ", "  _ ", "  \\ ", " |  ", "/   "}},
    {4, 8, {"_   ", "\\   ", " |  ", "  / ", "  _ ", "  / ", " |  ", "\\   "}},
    {4, 4, {" .  ", " o  ", " O  ", " o  "}},
    {4, 5, {"....", " ...", ". ..", ".. .", "... "}},
    {4, 5, {"    ", ".   ", " .  ", "  . ", "   ."}},
    {4, 4, {".oOo", "oOo.", "Oo.o", "o.oO"}},
    {4, 11,{"____", "\\___", "|\\__", "||\\_", "|||\\", "||||", "/|||",
	    "_/||", "__/|", "___/", "____"}},
    {7, 9, {".     .", " .   . ", "  . .  ",
            "   .   ", "   +   ", "   *   ", "   X   ",
	    "   #   ", "       "}},
    {4, 4, {". O ", "o o ", "O . ", "o o "}},
    {4, 26,{"|   ", "/   ", "_   ", "\\   ", " |  ", " /  ", " _  ", " \\  ",
            "  | ", "  / ", "  _ ", "  \\ ", "   |", "   |", "  \\ ", "  _ ", "  / ",
	    "  | ", " \\  ", " _  ", " /  ", " |  ", "\\   ", "_   ", "/   ", "|   "}},
    {4, 8, {"*   ", "-*  ", "--* ", " --*", "  --", "   -", "    ", "    "}},
    {4, 2, {"\\/\\/", "/\\/\\"}},
    {4, 4, {"\\|/|", "|\\|/", "/|\\|", "|/|\\"}}
};



/*
 * various pauses in 100th's of second
 */
#define	BUSY_PERIOD_PERCENT	25	/* percent done update */
#define	BUSY_MSG_DONE		0	/* no more updates */
#define	BUSY_MSG_RETRY		25	/* message line conflict */
#define	BUSY_DELAY_PERCENT	33	/* pause before showing % */
#define	BUSY_DELAY_SPINNER	100	/* second pause before spinner */


/* internal prototypes */
int	do_busy_cue(void *);
void	done_busy_cue(void *);


/*
 * Turn on a busy cue.
 *
 *    msg   -- the busy message to print in status line
 *    pc_f  -- if non-null, call this function to get the percent done,
 *	       (an integer between 0 and 100).  If null, append dots.
 *   delay  -- seconds to delay output of delay notification
 *
 *   Returns:  0 If busy cue was already set up before we got here
 *             1 If busy cue was not already set up.
 *
 *   NOTE: busy_cue and cancel_busy_cue MUST be called symetrically in the
 *         same lexical scope.
 */
int
busy_cue(char *msg, percent_done_t pc_f, int delay)
{
    AFTER_S *a = NULL, **ap;

    dprint((9, "busy_cue(%s, %p, %d)\n", msg ? msg : "Busy", pc_f, delay));

    if(!(ps_global && ps_global->ttyo)){
	dprint((9, "busy_cue returns No (ttyo)"));
	return(0);
    }

    /*
     * If we're already busy'ing, but a cue is invoked that
     * supplies more useful info, use it...
     */
    if(after_active){
	if(msg || pc_f)
	  stop_after(1);	/* uninstall old handler */
	else
	  return(0);		/* nothing to add, return */
    }

    /* get ready to set up list of AFTER_S's */
    ap = &a;

    dotcount = 0;
    percent_done_ptr = pc_f;

    if(msg){
	strncpy(busy_message, msg, sizeof(busy_message));
	final_message = 1;
    }
    else{
	strncpy(busy_message, "Busy", sizeof(busy_message));
	final_message = 0;
    }

    busy_message[sizeof(busy_message)-1] = '\0';
    busy_width = utf8_width(busy_message);

    if(!delay){
	char progress[MAX_SCREEN_COLS+1];
	int space_left, slots_used;

	final_message = 1;
	space_left = (ps_global->ttyo ? ps_global->ttyo->screen_cols
		      : 80) - busy_width - 2;  /* 2 is for [] */
	slots_used = MAX(0, MIN(space_left-3, 10));

	if(percent_done_ptr && slots_used >= 4){
	    snprintf(progress, sizeof(progress), "%s |%*s|", busy_message, slots_used, "");
	    progress[sizeof(progress)-1] = '\0';
	}
	else{
	    dotcount++;
	    snprintf(progress, sizeof(progress), "%s%*s", busy_message,
		     spinners[spinner].width + 1, "");
	    progress[sizeof(progress)-1] = '\0';
	}


	if(status_message_remaining()){
	    char buf[sizeof(progress) + 30];
	    char  *append = " [not actually shown]";

	    strncpy(buf, progress, sizeof(buf)-1);
	    buf[sizeof(buf)-1] = '\0';

	    strncat(buf, append, sizeof(buf) - strlen(buf) - 1);
	    buf[sizeof(buf)-1] = '\0';

	    add_review_message(buf, -1);
	}
	else{
	    q_status_message(SM_ORDER, 0, 1, progress);

	    /*
	     * We use display_message so that the initial message will
	     * be forced out only if there is not a previous message
	     * currently being displayed that hasn't been displayed for
	     * its min display time yet.  In that case, we don't want
	     * to force out the initial message.
	     */
	    display_message('x');
	}
	
	fflush(stdout);
    }

    /*
     * Randomly select one of the animations, even taking care
     * to run through all of them before starting over!
     * The user won't actually see them all because most of them
     * will never show up before they are canceled, but it
     * should still help.
     */
    spinner = -1;
    if(F_OFF(F_USE_BORING_SPINNER,ps_global) && ps_global->active_status_interval > 0){
	int arrsize, eligible, pick_this_one, i, j;

	arrsize = sizeof(spinners)/sizeof(struct _spinner);

	/* how many of them are eligible to be used this round? */
	for(eligible = i = 0; i < arrsize; i++)
	  if(!spinners[i].used_this_round)
	    eligible++;

	if(eligible == 0)				/* reset */
	  for(eligible = i = 0; i < arrsize; i++){
	      spinners[i].used_this_round = 0;
	      eligible++;
	  }

	if(eligible > 0){	/* it will be */
	    pick_this_one = random() % eligible;
	    for(j = i = 0; i < arrsize && spinner < 0; i++)
	      if(!spinners[i].used_this_round){
		  if(j == pick_this_one)
		    spinner = i;
		  else
		    j++;
	      }
	}
    }

    if(spinner < 0 || spinner > sizeof(spinners)/sizeof(struct _spinner) -1)
      spinner = 0;

    *ap		 = new_afterstruct();
    (*ap)->delay = (pc_f) ? BUSY_DELAY_PERCENT : BUSY_DELAY_SPINNER;
    (*ap)->f	 = do_busy_cue;
    (*ap)->cf	 = done_busy_cue;
    ap		 = &(*ap)->next;

    start_after(a);		/* launch cue handler */

#ifdef _WINDOWS
    mswin_setcursor(MSWIN_CURSOR_BUSY);
#endif

    return(1);
}


/*
 * If final_message was set when busy_cue was called:
 *   and message_pri = -1 -- no final message queued
 *                 else final message queued with min equal to message_pri
 */
void
cancel_busy_cue(int message_pri)
{
    dprint((9, "cancel_busy_cue(%d)\n", message_pri));

    final_message_pri = message_pri;

    stop_after(0);
}

/*
 * suspend_busy_cue - continue previously installed busy_cue.
 */
void
suspend_busy_cue(void)
{
    dprint((9, "suspend_busy_cue\n"));

    if(after_active)
      busy_cue_pause = 1;
}


/*
 * resume_busy_cue - continue previously installed busy_cue.
 */
void
resume_busy_cue(unsigned int pause)
{
    dprint((9, "resume_busy_cue\n"));

    if(after_active)
      busy_cue_pause = 0;
}


/*
 * do_busy_cue - paint the busy cue and return how long caller
 *               should pause before calling us again
 */
int
do_busy_cue(void *data)
{
    int space_left, slots_used, period;
    char dbuf[MAX_SCREEN_COLS+1];

    /* Don't wipe out any displayed status message prematurely */
    if(status_message_remaining() || busy_cue_pause)
      return(BUSY_MSG_RETRY);

    space_left = (ps_global->ttyo ? ps_global->ttyo->screen_cols : 80) -
	busy_width - 2;  /* 2 is for [] */
    slots_used = MAX(0, MIN(space_left-3, 10));

    if(percent_done_ptr && slots_used >= 4){
	int completed, pd;
	char *s;

	pd = (*percent_done_ptr)();
	pd = MIN(MAX(0, pd), 100);

	completed = (pd * slots_used) / 100;
	snprintf(dbuf, sizeof(dbuf), "%s |%s%s%*s|", busy_message,
	    completed > 1 ? repeat_char(completed-1, pd==100 ? ' ' : '-') : "",
	    (completed > 0 && pd != 100) ? ">" : "",
	    slots_used - completed, "");
	dbuf[sizeof(dbuf)-1] = '\0';

	if(slots_used == 10){
	    s = dbuf + strlen(dbuf) - 8;
	    if(pd < 10){
		s++; s++;
		*s++ = '0' + pd;
	    }
	    else if(pd < 100){
		s++;
		*s++ = '0' + pd / 10;
		*s++ = '0' + pd % 10;
	    }
	    else{
		*s++ = '1';
		*s++ = '0';
		*s++ = '0';
	    }

	    *s   = '%';
	}

	period = BUSY_PERIOD_PERCENT;
    }
    else{
	char b[MAX_SPINNER_WIDTH + 2];
	int ind;

	ind = (dotcount % spinners[spinner].elements);

	spinners[spinner].used_this_round = 1;
	if(space_left >= spinners[spinner].width + 1){
	    b[0] = SPACE;
	    strncpy(b+1,
		    (ps_global->active_status_interval > 0)
		      ? spinners[spinner].bars[ind] : "... ", sizeof(b)-1);
	    b[sizeof(b)-1] = '\0';
	}
	else if(space_left >= 2 && space_left < sizeof(b)){
	    b[0] = '.';
	    b[1] = '.';
	    b[2] = '.';
	    b[space_left] = '\0';
	}
	else
	  b[0] = '\0';

	snprintf(dbuf, sizeof(dbuf), "%s%s", busy_message, b);
	dbuf[sizeof(dbuf)-1] = '\0';

	/* convert interval to delay in 100ths of second */
	period = (ps_global->active_status_interval > 0)
		  ? (100 / MIN(10, ps_global->active_status_interval)) : BUSY_MSG_DONE;
    }

    status_message_write(dbuf, 1);
    dotcount++;
    fflush(stdout);

    return(period);
}


void
done_busy_cue(void *data)
{
    int space_left, slots_used;

    if(final_message && final_message_pri >= 0){
	char progress[MAX_SCREEN_COLS+1];

	/* 2 is for [] */
	space_left = (ps_global->ttyo ? ps_global->ttyo->screen_cols : 80) - busy_width - 2;
	slots_used = MAX(0, MIN(space_left-3, 10));

	if(percent_done_ptr && slots_used >= 4){
	    int left, right;

	    right = (slots_used - 4)/2;
	    left  = slots_used - 4 - right;
	    snprintf(progress, sizeof(progress), "%s |%*s100%%%*s|",
		     busy_message, left, "", right, "");
	    progress[sizeof(progress)-1] = '\0';
	    q_status_message(SM_ORDER,
			     final_message_pri>=2 ? MAX(final_message_pri,3) : 0,
			     final_message_pri+2, progress);
	}
	else{
	    int padding;

	    padding = MAX(0, MIN(space_left-5, spinners[spinner].width-4));

	    snprintf(progress, sizeof(progress), "%s %*sDONE", busy_message,
		     padding, "");
	    progress[sizeof(progress)-1] = '\0';
	    q_status_message(SM_ORDER,
			     final_message_pri>=2 ? MAX(final_message_pri,3) : 0,
			     final_message_pri+2, progress);
	}
    }

    mark_status_dirty();
}
