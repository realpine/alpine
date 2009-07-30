#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: newmail.c 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $";
#endif

/*
 * ========================================================================
 * Copyright 2009 University of Washington
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
 * Internal prototypes
 */
void new_mail_win_mess(MAILSTREAM *, long);
void new_mail_mess(MAILSTREAM *, long, long, int);
void newmailfifo(int, char *, char *, char *);


/*----------------------------------------------------------------------
     pith optional function to queue a newmail announcement
     

  ----*/
void
newmail_status_message(MAILSTREAM *stream, long n, long t_nm_count)
{
#ifdef _WINDOWS
    if(mswin_newmailwinon())
      new_mail_win_mess(stream, t_nm_count);
#elif !defined(DOS) && !defined(OS2) && !defined(LEAVEOUTFIFO)
    if(ps_global->VAR_FIFOPATH)
      new_mail_win_mess(stream, t_nm_count);
#endif
    if(n){
      new_mail_mess(stream, sp_mail_since_cmd(stream), n, 0);
    }
}


/*
 * alert for each new message individually.  new_mail_mess lumps 
 * messages together, we call new_mail_mess with 1 message at a time.
 * This is currently for PC-Pine new mail window, but could probably
 * be used more generally.
 *      stream - new mail stream
 *      number - number of new messages to alert for
 */
void
new_mail_win_mess(MAILSTREAM *stream, long int number)
{
    int n, i;
    MESSAGECACHE *mc;

    if(!stream)
      return;

    /* 
     * spare6, or MN_STMP, should be safe to use for now, we
     * just want to set which messages to alert about before
     * going to c-client.
     */
    for(n = stream->nmsgs, i = 0; n > 1L && i < number; n--){
	if(!get_lflag(stream, NULL, n, MN_EXLD)){
	    mc = mail_elt(stream, n);
	    if(mc)
	      mc->spare6 = 1;

	    if(++i == number)
	      break;
	}
	else{
	    mc = mail_elt(stream, n);
	    if(mc)
	      mc->spare6 = 0;
	}
    }
    /* 
     * Here n is the first new message we want to notify about.
     * spare6 will tell us which ones to use.  We set spare6
     * in case of new mail or expunge that could happen when
     * we mail_fetchstructure in new_mail_mess.
     */
    for(; n <= stream->nmsgs; n++)
      if(n > 0L && (mc = mail_elt(stream, n)) && mc->spare6){
	  mc->spare6 = 0;
	  new_mail_mess(stream, 1, n, 1);
      }
}


/*----------------------------------------------------------------------
     Format and queue a "new mail" message

  Args: stream     -- mailstream on which a mail has arrived
        number     -- number of new messages since last command
        max_num    -- The number of messages now on stream
	for_new_mail_win -- for separate new mail window (curr. PC-Pine)

 Not too much worry here about the length of the message because the
status_message code will fit what it can on the screen and truncation on
the right is about what we want which is what will happen.
  ----*/
void
new_mail_mess(MAILSTREAM *stream, long int number, long int max_num, int for_new_mail_win)
{
    char      subject[MAILTMPLEN+1], subjtext[MAILTMPLEN+1], from[MAILTMPLEN+1],
	     *folder = NULL, intro[MAILTMPLEN+1];
    ENVELOPE *e = NULL;

    if(stream)
      e = pine_mail_fetchstructure(stream, max_num, NULL);

    if(stream){
	if(sp_flagged(stream, SP_INBOX))
	  folder = NULL;
	else{
	    folder = STREAMNAME(stream);
	    if(folder[0] == '?' && folder[1] == '\0')
	      folder = NULL;
	}
    }

    format_new_mail_msg(folder, number, e, intro, from, subject, subjtext, sizeof(subject));

    if(!for_new_mail_win)
      q_status_message5(SM_ASYNC | SM_DING, 0, 60,
		      "%s%s%s%.80s%.80s", intro,
 		      from ? ((number > 1L) ? " Most recent f" : " F") : "",
 		      from ? "rom " : "",
 		      from ? from : "",
		      subjtext);
#if (!defined(DOS) && !defined(OS2) && !defined(LEAVEOUTFIFO)) || defined(_WINDOWS)
    else {
	int is_us = 0;
	ADDRESS *tadr;

	if(e)
	  for(tadr = e->to; tadr; tadr = tadr->next)
	    if(address_is_us(tadr, ps_global)){
		is_us = 1;
		break;
	    }
#ifdef _WINDOWS
	mswin_newmailwin(is_us, from, subject, folder);
#else
	newmailfifo(is_us, from, subject, folder);
#endif
    }
#endif

    if(pith_opt_icon_text){
	if(F_ON(F_ENABLE_XTERM_NEWMAIL, ps_global)
	   && F_ON(F_ENABLE_NEWMAIL_SHORT_TEXT, ps_global)){
	    long inbox_nm;
	    if(!sp_flagged(stream, SP_INBOX) 
	       && (inbox_nm = sp_mail_since_cmd(sp_inbox_stream()))){
		snprintf(tmp_20k_buf, SIZEOF_20KBUF, "[%ld, %ld] %s",
			 inbox_nm > 1L ? inbox_nm : 1L,
			 number > 1L ? number: 1L,
			 ps_global->pine_name);
	    }
	    else
	      snprintf(tmp_20k_buf, SIZEOF_20KBUF, "[%ld] %s", number > 1L ? number: 1L,
		       ps_global->pine_name);
	}
	else
	  snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%s%s%s%.80s", intro,
		   from ? ((number > 1L) ? " Most recent f" : " F") : "",
		   from ? "rom " : "",
		   from ? from : "");

	(*pith_opt_icon_text)(tmp_20k_buf, IT_NEWMAIL);
    }
}


#if !defined(DOS) && !defined(OS2) && !defined(LEAVEOUTFIFO)
static char *fifoname = NULL;
static int   fifoopenerrmsg = 0;
static int   fifofd = -1;
static int   fifoheader = 0;

void
init_newmailfifo(char *fname)
{
    if(fifoname)
      close_newmailfifo();

    if(!(fname && *fname))
      return;

    if(!fifoname){
	if(mkfifo(fname, 0600) == -1){
	    q_status_message2(SM_ORDER,3,3,
			      "Can't create NewMail FIFO \"%s\": %s",
			      fname, error_description(errno));
	    return;
	}
	else
	  q_status_message1(SM_ORDER,0,3, "NewMail FIFO: \"%s\"", fname);

	fifoname = cpystr(fname);
    }
}


void
close_newmailfifo(void)
{
    if(fifoname){
	if(fifofd >= 0)
	  (void) close(fifofd);

	if(*fifoname)
	  our_unlink(fifoname);

	fs_give((void **) &fifoname);
    }

    fifoheader = 0;
    fifoname = NULL;
    fifofd = -1;
    fifoopenerrmsg = 0;
}


void
newmailfifo(int is_us, char *from, char *subject, char *folder)
{
    char buf[MAX_SCREEN_COLS+1], buf2[MAX_SCREEN_COLS+1];
    char buf3[MAX_SCREEN_COLS+1], buf4[MAX_SCREEN_COLS+1];

    if(!(fifoname && *fifoname)){
	if(fifoname)
	  close_newmailfifo();

	return;
    }

    if(fifofd < 0){
	fifofd = our_open(fifoname, O_WRONLY | O_NONBLOCK | O_BINARY, 0600);
	if(fifofd < 0){
	    if(!fifoopenerrmsg){
		if(errno == ENXIO)
		  q_status_message2(SM_ORDER,0,3, "Nothing reading \"%s\": %s",
				    fifoname, error_description(errno));
		else
		  q_status_message2(SM_ORDER,0,3, "Can't open \"%s\": %s",
				    fifoname, error_description(errno));

		fifoopenerrmsg++;
	    }

	    return;
	}
    }

    if(fifofd >= 0){
	int width;
	int fromlen, subjlen, foldlen;

	width = MIN(MAX(20, ps_global->nmw_width), MAX_SCREEN_COLS);

	foldlen = .18 * width;
	foldlen = MAX(5, foldlen);
	fromlen = .28 * width;
	subjlen = width - 2 - foldlen - fromlen;

	if(!fifoheader){
	    time_t now;
	    char *tmtxt;

	    now = time((time_t *) 0);
	    tmtxt = ctime(&now);
	    if(!tmtxt)
	      tmtxt = "";

	    snprintf(buf, sizeof(buf), "New Mail window started at %.*s\n",
		    MIN(100, strlen(tmtxt)-1), tmtxt);
	    (void) write(fifofd, buf, strlen(buf));

	    snprintf(buf, sizeof(buf), "  %-*s%-*s%-*s\n",
		    fromlen, "From:",
		    subjlen, "Subject:",
		    foldlen, "Folder:");
	    (void) write(fifofd, buf, strlen(buf));

	    snprintf(buf, sizeof(buf), "%-*.*s\n", width, width, repeat_char(width, '-'));
	    (void) write(fifofd, buf, strlen(buf));

	    fifoheader++;
	}

	snprintf(buf, sizeof(buf), "%s %-*.*s %-*.*s %-*.*s\n", is_us ? "+" : " ",
		fromlen - 1, fromlen - 1,
		short_str(from ? from : "", buf2, sizeof(buf2), fromlen-1, EndDots),
		subjlen - 1, subjlen - 1,
		short_str(subject ? subject : "(no subject)",
			  buf3, sizeof(buf3), subjlen-1, EndDots),
		foldlen, foldlen,
		short_str(folder ? folder : "INBOX", buf4, sizeof(buf4), foldlen, FrontDots));
	(void) write(fifofd, buf, strlen(buf));
    }
}
#endif
