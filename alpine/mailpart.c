#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: mailpart.c 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $";
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
     mailpart.c
     The meat and pototoes of attachment processing here.

  ====*/

#include "headers.h"
#include "mailpart.h"
#include "status.h"
#include "context.h"
#include "keymenu.h"
#include "alpine.h"
#include "reply.h"
#include "radio.h"
#include "takeaddr.h"
#include "mailview.h"
#include "mailindx.h"
#include "mailcmd.h"
#include "help.h"
#include "titlebar.h"
#include "signal.h"
#include "send.h"
#include "busy.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/store.h"
#include "../pith/msgno.h"
#include "../pith/detach.h"
#include "../pith/handle.h"
#include "../pith/filter.h"
#include "../pith/bitmap.h"
#include "../pith/charset.h"
#include "../pith/mimedesc.h"
#include "../pith/mailcap.h"
#include "../pith/newmail.h"
#include "../pith/rfc2231.h"
#include "../pith/flag.h"
#include "../pith/text.h"
#include "../pith/editorial.h"
#include "../pith/save.h"
#include "../pith/pipe.h"
#include "../pith/util.h"
#include "../pith/detoken.h"
#include "../pith/busy.h"
#include "../pith/mimetype.h"
#include "../pith/icache.h"
#include "../pith/list.h"
#include "../pith/ablookup.h"
#include "../pith/options.h"
#include "../pith/smime.h"


/*
 * Information used to paint and maintain a line on the attachment
 * screen.
 */
typedef struct atdisp_line {
    char	     *dstring;			/* alloc'd var value string  */
    ATTACH_S	     *attp;			/* actual attachment pointer */
    struct atdisp_line *next, *prev;
} ATDISP_S;


/*
 * struct defining attachment screen's current state
 */
typedef struct att_screen {
    ATDISP_S   *current,
	       *top_line;
    COLOR_PAIR *titlecolor;
} ATT_SCREEN_S;

static ATT_SCREEN_S *att_screen;


#define	next_attline(p)	((p) ? (p)->next : NULL)
#define	prev_attline(p)	((p) ? (p)->prev : NULL)


#ifdef	_WINDOWS
/*
 * local global pointer to scroll attachment popup menu
 */
static ATTACH_S *scrat_attachp;
#endif


#define	FETCH_READC   g_fr_desc->readc


/* used to keep track of detached MIME segments total length */
static long	save_att_length;

/*
 * Internal Prototypes
 */
ATDISP_S   *new_attline(ATDISP_S **);
void	    free_attline(ATDISP_S **);
int	    attachment_screen_updater(struct pine *, ATDISP_S *, ATT_SCREEN_S *);
void	    attachment_screen_redrawer(void);
void        update_att_screen_titlebar(void);
ATDISP_S   *first_attline(ATDISP_S *);
int	    init_att_progress(char *, MAILSTREAM *, BODY *);
long	    save_att_piped(int);
int	    save_att_percent(void);
void	    save_attachment(int, long, ATTACH_S *);
void	    export_attachment(int, long, ATTACH_S *);
char	   *write_attached_msg(long, ATTACH_S **, STORE_S *, int);
void	    save_msg_att(long, ATTACH_S *);
void	    save_digest_att(long, ATTACH_S *);
int         save_msg_att_work(long int, ATTACH_S *, MAILSTREAM *, char *,
			      CONTEXT_S *, char *);
void	    export_msg_att(long, ATTACH_S *);
void	    export_digest_att(long, ATTACH_S *);
void	    print_attachment(int, long, ATTACH_S *);
int	    print_msg_att(long, ATTACH_S *, int);
void	    print_digest_att(long, ATTACH_S *);
void	    run_viewer(char *, BODY *, int);
STORE_S	   *format_text_att(long, ATTACH_S *, HANDLE_S **);
int	    display_text_att(long, ATTACH_S *, int);
int	    display_msg_att(long, ATTACH_S *, int);
void	    display_digest_att(long, ATTACH_S *, int);
int	    scroll_attachment(char *, STORE_S *, SourceType, HANDLE_S *, ATTACH_S *, int);
int	    process_attachment_cmd(int, MSGNO_S *, SCROLL_S *);
int	    format_msg_att(long, ATTACH_S **, HANDLE_S **, gf_io_t, int);
void	    display_vcard_att(long, ATTACH_S *, int);
void	    display_attach_info(long, ATTACH_S *);
void	    forward_attachment(MAILSTREAM *, long, ATTACH_S *);
void	    forward_msg_att(MAILSTREAM *, long, ATTACH_S *);
void	    reply_msg_att(MAILSTREAM *, long, ATTACH_S *);
void	    bounce_msg_att(MAILSTREAM *, long, char *, char *);
void	    pipe_attachment(long, ATTACH_S *);
int	    delete_attachment(long, ATTACH_S *);
int	    undelete_attachment(long, ATTACH_S *, int *);
#ifdef	_WINDOWS
int	    scroll_att_popup(SCROLL_S *, int);
void	    display_text_att_window(ATTACH_S *);
void	    display_msg_att_window(ATTACH_S *);
#endif


/*----------------------------------------------------------------------
   Provide attachments in browser for selected action

  Args: ps -- pointer to pine structure
	msgmap -- struct containing current message to display

  Result: 

  Handle painting and navigation of attachment index.  It would be nice
  to someday handle message/rfc822 segments in a neat way (i.e., enable
  forwarding, take address, etc.).
 ----*/
void
attachment_screen(struct pine *ps)
{
    UCS           ch = 'x';
    int		  n, cmd, dline,
		  maxnumwid = 0, maxsizewid = 0, old_cols = -1, km_popped = 0, expbits,
		  last_type = TYPEOTHER;
    long	  msgno;
    char	 *q, *last_subtype = NULL, backtag[64], *utf8str;
    OtherMenu     what;
    ATTACH_S	 *a;
    ATDISP_S	 *current = NULL, *ctmp = NULL;
    ATT_SCREEN_S  screen;

    ps->prev_screen = attachment_screen;
    ps->next_screen = SCREEN_FUN_NULL;

    ps->some_quoting_was_suppressed = 0;

    if(ps->ttyo->screen_rows - HEADER_ROWS(ps) - FOOTER_ROWS(ps) < 1){
	q_status_message(SM_ORDER | SM_DING, 0, 3,
			 _("Screen too small to view attachment"));
	ps->next_screen = mail_view_screen;
	return;
    }

    if(mn_total_cur(ps->msgmap) > 1L){
	q_status_message(SM_ORDER | SM_DING, 0, 3,
			 _("Can only view one message's attachments at a time."));
	return;
    }
    else if(ps->atmts && ps->atmts->description && !(ps->atmts + 1)->description)
      q_status_message1(SM_ASYNC, 0, 3,
    _("Message %s has only one part (the message body), and no attachments."),
	long2string(mn_get_cur(ps->msgmap)));

    /*----- Figure max display widths -----*/
    for(a = ps->atmts; a && a->description != NULL; a++){
	if((n = utf8_width(a->number)) > maxnumwid)
	  maxnumwid = n;

	if((n = utf8_width(a->size)) > maxsizewid)
	  maxsizewid = n;
    }

    /*
     * Then, allocate and initialize attachment line list...
     */
    for(a = ps->atmts; a && a->description; a++)
      new_attline(&current)->attp = a;

    memset(&screen, 0, sizeof(screen));
    msgno	       = mn_m2raw(ps->msgmap, mn_get_cur(ps->msgmap));
    ps->mangled_screen = 1;			/* build display */
    ps->redrawer       = attachment_screen_redrawer;
    att_screen	       = &screen;
    current	       = first_attline(current);
    what	       = FirstMenu;

    /*
     * Advance to next attachment if it's likely that we've already
     * shown it to the user...
     */

    if (current == NULL){
      q_status_message1(SM_ORDER | SM_DING, 0, 3,      
                       _("Malformed message: %s"),
			ps->c_client_error ? ps->c_client_error : "?");
      ps->next_screen = mail_view_screen;
    }
    else if(current->attp->shown && (ctmp = next_attline(current)))
      current = ctmp;

    while(ps->next_screen == SCREEN_FUN_NULL){
	ps->user_says_cancel = 0;
	if(km_popped){
	    km_popped--;
	    if(km_popped == 0){
		clearfooter(ps);
		ps->mangled_body = 1;
	    }
	}

	if(ps->mangled_screen){
	    /*
	     * build/rebuild display lines
	     */
	    if(old_cols != ps->ttyo->screen_cols){
		int avail, s1, s2, s4, s5, dwid, sizewid, descwid;

		avail = ps_global->ttyo->screen_cols;

		s1 = MAX(MIN(1, avail), 0);
		avail -= s1;

		dwid = MAX(MIN(1, avail), 0);
		avail -= dwid;

		s2 = MAX(MIN(1, avail), 0);
		avail -= s2;

		/* Only give up a third of the display to part numbers */
		maxnumwid = MIN(maxnumwid, (ps_global->ttyo->screen_cols/3));
		maxnumwid = MAX(MIN(maxnumwid, avail), 0);
		avail -= maxnumwid;

		s4 = MAX(MIN(3, avail), 0);
		avail -= s4;

		sizewid = MAX(MIN(maxsizewid, avail), 0);
		avail -= sizewid;

		s5 = MAX(MIN(3, avail), 0);
		avail -= s5;

		descwid = MAX(0, avail);

		old_cols = ps->ttyo->screen_cols;

		for(ctmp = first_attline(current);
		    ctmp && (a = ctmp->attp) && a->description;
		    ctmp = next_attline(ctmp)){
		    size_t len;
		    char numbuf[50];
		    char description[1000];

		    if(ctmp->dstring)
		      fs_give((void **)&ctmp->dstring);

		    len = 6 * MAX(80, ps->ttyo->screen_cols) * sizeof(char);
		    ctmp->dstring = (char *)fs_get(len + 1);

		    /*
		     * description
		     */
		    q = a->body->description;
		    if(!(q && q[0]) && (MIME_MSG_A(a) && a->body->nested.msg->env))
		      q = a->body->nested.msg->env->subject;

		    if(q && q[0]){
			char buftmp[1000];

			strncpy(buftmp, q, sizeof(buftmp));
			buftmp[sizeof(buftmp)-1] = '\0';

			q = (char *) rfc1522_decode_to_utf8((unsigned char *) tmp_20k_buf,
							    SIZEOF_20KBUF, buftmp);
		    }

		    if(q && !q[0])
		      q = NULL;

		    snprintf(description, sizeof(description), "%s%s%s%s", type_desc(a->body->type, a->body->subtype, a->body->parameter, a->body->disposition.type ? a->body->disposition.parameter : NULL, 1), q ? ", \"" : "", q ? q : "", q ? "\"" : "");
		    description[sizeof(description)-1] = '\0';

		    utf8_snprintf(ctmp->dstring, len+1,
			"%*.*s%*.*w%*.*s%-*.*w%*.*s%*.*w%*.*s%-*.*w",
			s1, s1, "",
			dwid, dwid,
			msgno_part_deleted(ps->mail_stream, msgno, a->number) ? "D" : "",
			s2, s2, "",
			maxnumwid, maxnumwid,
			a->number
			    ? short_str(a->number, numbuf, sizeof(numbuf), maxnumwid, FrontDots)
			    : "",
			s4, s4, "",
			sizewid, sizewid,
			a->size ? a->size : "",
			s5, s5, "",
			descwid, descwid, description);

		    ctmp->dstring[len] = '\0';
		}
	    }

	    ps->mangled_header = 1;
	    ps->mangled_footer = 1;
	    ps->mangled_body   = 1;
	}

	/*----------- Check for new mail -----------*/
        if(new_mail(0, NM_TIMING(ch), NM_STATUS_MSG | NM_DEFER_SORT) >= 0)
          ps->mangled_header = 1;

	/*
	 * If an expunge of the current message happened during the
	 * new mail check we want to bail out of here. See mm_expunged.
	 */
	if(ps->next_screen != SCREEN_FUN_NULL)
	  break;

	if(ps->mangled_header){
	    update_att_screen_titlebar();
	    ps->mangled_header = 0;
	}

	if(ps->mangled_screen){
	    ClearLine(1);
	    ps->mangled_screen = 0;
	}

	dline = attachment_screen_updater(ps, current, &screen);

	/*---- This displays new mail notification, or errors ---*/
	if(km_popped){
	    FOOTER_ROWS(ps) = 3;
	    mark_status_unknown();
	}

        display_message(ch);
	if(km_popped){
	    FOOTER_ROWS(ps) = 1;
	    mark_status_unknown();
	}

	if(ps->mangled_footer
	   || current->attp->body->type != last_type
	   || !(last_subtype && current->attp->body->subtype)
	   || strucmp(last_subtype, current->attp->body->subtype)){

	    struct key_menu *km = &att_index_keymenu;
	    bitmap_t	     bitmap;

	    setbitmap(bitmap);
	    ps->mangled_footer = 0;
	    last_type	       = current->attp->body->type;
	    last_subtype       = current->attp->body->subtype;

	    snprintf(backtag, sizeof(backtag), "Msg #%ld", mn_get_cur(ps->msgmap));
	    backtag[sizeof(backtag)-1] = '\0';
	    km->keys[ATT_PARENT_KEY].label = backtag;

	    if(F_OFF(F_ENABLE_PIPE, ps))
	      clrbitn(ATT_PIPE_KEY, bitmap);

	    /*
	     * If message or digest, leave Reply and Save and,
	     * conditionally, Bounce on...
	     */
	    if(MIME_MSG(last_type, last_subtype)){
		if(F_OFF(F_ENABLE_BOUNCE, ps))
		  clrbitn(ATT_BOUNCE_KEY, bitmap);

		km->keys[ATT_EXPORT_KEY].name  = "";
		km->keys[ATT_EXPORT_KEY].label = "";
	    }
	    else if(MIME_DGST(last_type, last_subtype)){
		clrbitn(ATT_BOUNCE_KEY, bitmap);
		clrbitn(ATT_REPLY_KEY, bitmap);

		km->keys[ATT_EXPORT_KEY].name  = "";
		km->keys[ATT_EXPORT_KEY].label = "";
	    }
	    else{
		clrbitn(ATT_BOUNCE_KEY, bitmap);
		clrbitn(ATT_REPLY_KEY, bitmap);

		if(last_type != TYPETEXT)
		  clrbitn(ATT_PRINT_KEY, bitmap);

		km->keys[ATT_EXPORT_KEY].name  = "E";
		km->keys[ATT_EXPORT_KEY].label = N_("Export");
	    }

	    if(km_popped){
		FOOTER_ROWS(ps) = 3;
		clearfooter(ps);
	    }

	    if(F_ON(F_ARROW_NAV, ps)){
		menu_add_binding(km, KEY_LEFT, MC_EXIT);
		menu_add_binding(km, KEY_RIGHT, MC_VIEW_ATCH);
	    }
	    else{
		menu_clear_binding(km, KEY_LEFT);
		menu_clear_binding(km, KEY_RIGHT);
	    }

	    draw_keymenu(km, bitmap, ps->ttyo->screen_cols, 1-FOOTER_ROWS(ps),
			 0, what);
	    what = SameMenu;
	    if(km_popped){
		FOOTER_ROWS(ps) = 1;
		mark_keymenu_dirty();
	    }
	}

	if(F_ON(F_SHOW_CURSOR, ps))
	  MoveCursor(dline, 0);
	else
	  MoveCursor(MAX(0, ps->ttyo->screen_rows - FOOTER_ROWS(ps)), 0);

        /*------ Prepare to read the command from the keyboard ----*/
#ifdef	MOUSE
	mouse_in_content(KEY_MOUSE, -1, -1, 0, 0); /* prime the handler */
	register_mfunc(mouse_in_content, HEADER_ROWS(ps_global), 0,
		       ps_global->ttyo->screen_rows -(FOOTER_ROWS(ps_global)+1),
		       ps_global->ttyo->screen_cols);
#endif
	ch = READ_COMMAND(&utf8str);
#ifdef	MOUSE
	clear_mfunc(mouse_in_content);
#endif

	cmd = menu_command(ch, &att_index_keymenu);

	if(km_popped)
	  switch(cmd){
	    case MC_NONE :
	    case MC_OTHER :
	    case MC_RESIZE :
	    case MC_REPAINT :
	      km_popped++;
	      break;
	    
	    default:
	      clearfooter(ps);
	      break;
	  }

	switch(cmd){
	  case MC_HELP :
	    if(FOOTER_ROWS(ps) == 1 && km_popped == 0){
		km_popped = 2;
		ps->mangled_footer = 1;
		break;
	    }

	    helper(h_attachment_screen, _("HELP FOR ATTACHMENT INDEX"), 0);
	    ps->mangled_screen = 1;
	    break;

	  case MC_OTHER :
	    what = NextMenu;
	    ps->mangled_footer = 1;
	    break;

	  case MC_FULLHDR :
	    ps->full_header++;
	    if(ps->full_header == 1){
		if(!(ps->quote_suppression_threshold
		     && (ps->some_quoting_was_suppressed /* || in_index != View */)))
		  ps->full_header++;
	    }
	    else if(ps->full_header > 2)
	      ps->full_header = 0;

	    switch(ps->full_header){
	      case 0:
		q_status_message(SM_ORDER, 0, 3,
				 _("Display of full headers is now off."));
		break;

	      case 1:
		q_status_message1(SM_ORDER, 0, 3,
			      _("Quotes displayed, use %s to see full headers"),
			      F_ON(F_USE_FK, ps) ? "F9" : "H");
		break;

	      case 2:
		q_status_message(SM_ORDER, 0, 3,
				 _("Display of full headers is now on."));
		break;

	    }

	    break;

	  case MC_EXIT :			/* exit attachment screen */
	    ps->next_screen = mail_view_screen;
	    break;

	  case MC_QUIT :
	    ps->next_screen = quit_screen;
	    break;

	  case MC_MAIN :
	    ps->next_screen = main_menu_screen;
	    break;

	  case MC_INDEX :
	    ps->next_screen = mail_index_screen;
	    break;

	  case MC_NEXTITEM :
	    if((ctmp = next_attline(current)) != NULL)
	      current = ctmp;
	    else
	      q_status_message(SM_ORDER, 0, 1, _("Already on last attachment"));

	    break;

	  case MC_PREVITEM :
	    if((ctmp = prev_attline(current)) != NULL)
	      current = ctmp;
	    else
	      q_status_message(SM_ORDER, 0, 1, _("Already on first attachment"));

	    break;

	  case MC_PAGEDN :				/* page forward */
	    if(next_attline(current)){
		while(dline++ < ps->ttyo->screen_rows - FOOTER_ROWS(ps))
		  if((ctmp = next_attline(current)) != NULL)
		    current = ctmp;
		  else
		    break;
	    }
	    else
	      q_status_message(SM_ORDER, 0, 1,
			       _("Already on last page of attachments"));

	    break;

	  case MC_PAGEUP :			/* page backward */
	    if(prev_attline(current)){
		while(dline-- > HEADER_ROWS(ps))
		  if((ctmp = prev_attline(current)) != NULL)
		    current = ctmp;
		  else
		    break;

		while(++dline < ps->ttyo->screen_rows - FOOTER_ROWS(ps))
		  if((ctmp = prev_attline(current)) != NULL)
		    current = ctmp;
		  else
		    break;
	    }
	    else
	      q_status_message(SM_ORDER, 0, 1,
			       _("Already on first page of attachments"));

	    break;

#ifdef MOUSE	    
	  case MC_MOUSE:
	    {
		MOUSEPRESS mp;

		mouse_get_last (NULL, &mp);
		mp.row -= HEADER_ROWS(ps);
		ctmp = screen.top_line;
		while (mp.row && ctmp != NULL) {
		    --mp.row;
		    ctmp = ctmp->next;
		}

		if (ctmp != NULL){
		    current = ctmp;

		    if (mp.doubleclick){
			if(mp.button == M_BUTTON_LEFT)
			  display_attachment(msgno, current->attp, DA_SAVE);
		    }
#ifdef	_WINDOWS
		    else if(mp.button == M_BUTTON_RIGHT){
			MPopup atmt_popup[20];
			int    i = -1, exoffer = 0;

			dline = attachment_screen_updater(ps,current,&screen);

			if(dispatch_attachment(current->attp) != MCD_NONE){
			    atmt_popup[++i].type   = tQueue;
			    atmt_popup[i].data.val   = 'V';
			    atmt_popup[i].label.style    = lNormal;
			    atmt_popup[i].label.string    = "&View";

			    if(!(current->attp->can_display & MCD_EXTERNAL)
				&& (current->attp->body->type == TYPETEXT
				    || MIME_MSG_A(current->attp)
				    || MIME_DGST_A(current->attp))){
				exoffer++;
				atmt_popup[++i].type	   = tIndex;
				atmt_popup[i].label.style  = lNormal;
				atmt_popup[i].label.string =
							  "View in New Window";
			    }
			}

			atmt_popup[++i].type	   = tQueue;
			atmt_popup[i].data.val	   = 'S';
			atmt_popup[i].label.style  = lNormal;
			atmt_popup[i].label.string = "&Save";

			atmt_popup[++i].type	  = tQueue;
			atmt_popup[i].label.style = lNormal;
			if(current->dstring[1] == 'D'){
			    atmt_popup[i].data.val     = 'U';
			    atmt_popup[i].label.string = "&Undelete";
			}
			else{
			    atmt_popup[i].data.val     = 'D';
			    atmt_popup[i].label.string = "&Delete";
			}

			if(MIME_MSG_A(current->attp)){
			    atmt_popup[++i].type       = tQueue;
			    atmt_popup[i].label.style  = lNormal;
			    atmt_popup[i].label.string = "&Reply...";
			    atmt_popup[i].data.val     = 'R';

			    atmt_popup[++i].type       = tQueue;
			    atmt_popup[i].label.style  = lNormal;
			    atmt_popup[i].label.string = "&Forward...";
			    atmt_popup[i].data.val   = 'F';
			}

			atmt_popup[++i].type	   = tQueue;
			atmt_popup[i].label.style  = lNormal;
			atmt_popup[i].label.string = "&About...";
			atmt_popup[i].data.val	   = 'A';

			atmt_popup[++i].type = tSeparator;

			atmt_popup[++i].type	   = tQueue;
			snprintf(backtag, sizeof(backtag), "View Message #%ld",
				mn_get_cur(ps->msgmap));
			backtag[sizeof(backtag)-1] = '\0';
			atmt_popup[i].label.string = backtag;
			atmt_popup[i].label.style  = lNormal;
			atmt_popup[i].data.val	   = '<';

			atmt_popup[++i].type = tTail;

			if(mswin_popup(atmt_popup) == 1 && exoffer)
			  display_att_window(current->attp);
		    }
		}
		else if(mp.button == M_BUTTON_RIGHT){
		    MPopup atmt_popup[2];

		    atmt_popup[0].type = tQueue;
		    snprintf(backtag, sizeof(backtag), "View Message #%ld",
			    mn_get_cur(ps->msgmap));
		    backtag[sizeof(backtag)-1] = '\0';
		    atmt_popup[0].label.string = backtag;
		    atmt_popup[0].label.style  = lNormal;
		    atmt_popup[0].data.val     = '<';

		    atmt_popup[1].type = tTail;

		    mswin_popup(atmt_popup);
#endif
		}
	    }
	    break;
#endif

	  case MC_WHEREIS :			/* whereis */
	    /*--- get string  ---*/
	    {int   rc, found = 0;
	     char *result = NULL, buf[64];
	     static char last[64], tmp[64];
	     HelpType help;

	     ps->mangled_footer = 1;
	     buf[0] = '\0';
	     snprintf(tmp, sizeof(tmp), "Word to find %s%s%s: ",
		     (last[0]) ? "[" : "",
		     (last[0]) ? last : "",
		     (last[0]) ? "]" : "");
	     tmp[sizeof(tmp)-1] = '\0';
	     help = NO_HELP;
	     while(1){
		 int flags = OE_APPEND_CURRENT | OE_SEQ_SENSITIVE;

		 rc = optionally_enter(buf,-FOOTER_ROWS(ps),0,sizeof(buf),
					 tmp,NULL,help,&flags);
		 if(rc == 3)
		   help = help == NO_HELP ? h_attach_index_whereis : NO_HELP;
		 else if(rc == 0 || rc == 1 || !buf[0]){
		     if(rc == 0 && !buf[0] && last[0]){
			 strncpy(buf, last, sizeof(buf));
			 buf[sizeof(buf)-1] = '\0';
		     }

		     break;
		 }
	     }

	     if(rc == 0 && buf[0]){
		 ctmp = current;
		 while((ctmp = next_attline(ctmp)) != NULL)
		   if(srchstr(ctmp->dstring, buf)){
		       found++;
		       break;
		   }

		 if(!found){
		     ctmp = first_attline(current);

		     while(ctmp != current)
		       if(srchstr(ctmp->dstring, buf)){
			   found++;
			   break;
		       }
		       else
			 ctmp = next_attline(ctmp);
		 }
	     }
	     else
	       result = _("WhereIs cancelled");

	     if(found && ctmp){
		 strncpy(last, buf, sizeof(last));
		 last[sizeof(last)-1] = '\0';
		 result  = "Word found";
		 current = ctmp;
	     }

	     q_status_message(SM_ORDER, 0, 3,
			      result ? result : _("Word not found"));
	    }

	    break;

	  case MC_DELETE :
	    if(delete_attachment(msgno, current->attp)){
		int l = current ? strlen(current->attp->number) : 0;

		current->dstring[1] = 'D';

		/* Also indicate any children that will be deleted */

		for(ctmp = current; ctmp; ctmp = next_attline(ctmp))
		  if(!strncmp(ctmp->attp->number, current->attp->number, l)
		     && ctmp->attp->number[l] == '.'){
		      ctmp->dstring[1] = 'D';
		      ps->mangled_screen = 1;
		  }
	    }

	    break;

	  case MC_UNDELETE :
	    if(undelete_attachment(msgno, current->attp, &expbits)){
		int l = current ? strlen(current->attp->number) : 0;

		current->dstring[1] = ' ';

		/* And unflag anything implicitly undeleted */
		for(ctmp = current; ctmp; ctmp = next_attline(ctmp))
		  if(!strncmp(ctmp->attp->number, current->attp->number, l)
		     && ctmp->attp->number[l] == '.'
		     && (!msgno_exceptions(ps->mail_stream, msgno,
					   ctmp->attp->number, &expbits, FALSE)
			 || !(expbits & MSG_EX_DELETE))){
		      ctmp->dstring[1] = ' ';
		      ps->mangled_screen = 1;
		  }
	    }

	    break;

	  case MC_REPLY :
	    reply_msg_att(ps->mail_stream, msgno, current->attp);
	    break;

	  case MC_FORWARD :
	    forward_attachment(ps->mail_stream, msgno, current->attp);
	    break;

	  case MC_BOUNCE :
	    bounce_msg_att(ps->mail_stream, msgno, current->attp->number,
			   current->attp->body->nested.msg->env->subject);
	    ps->mangled_footer = 1;
	    break;

	  case MC_ABOUTATCH :
	    display_attach_info(msgno, current->attp);
	    break;

	  case MC_VIEW_ATCH :			/* View command */
	    display_attachment(msgno, current->attp, DA_SAVE);
	    old_cols = -1;
	    /* fall thru to repaint */

	  case MC_REPAINT :			/* redraw */
          case MC_RESIZE :
	    ps->mangled_screen = 1;
	    break;

	  case MC_EXPORT :
	    export_attachment(-FOOTER_ROWS(ps), msgno, current->attp);
	    ps->mangled_footer = 1;
	    break;

	  case MC_SAVETEXT :			/* Save command */
	    save_attachment(-FOOTER_ROWS(ps), msgno, current->attp);
	    ps->mangled_footer = 1;
	    break;

	  case MC_PRINTMSG :			/* Save command */
	    print_attachment(-FOOTER_ROWS(ps), msgno, current->attp);
	    ps->mangled_footer = 1;
	    break;

	  case MC_PIPE :			/* Pipe command */
	    if(F_ON(F_ENABLE_PIPE, ps)){
		pipe_attachment(msgno, current->attp);
		ps->mangled_footer = 1;
		break;
	    }					/* fall thru to complain */

	  case MC_NONE:				/* simple timeout */
	    break;

	  case MC_UTF8:
	    bogus_utf8_command(utf8str, F_ON(F_USE_FK, ps) ? "F1" : "?");
	    break;

	  case MC_CHARUP :
	  case MC_CHARDOWN :
	  case MC_CHARRIGHT :
	  case MC_CHARLEFT :
	  case MC_GOTOBOL :
	  case MC_GOTOEOL :
	  case MC_UNKNOWN :
	  default:
	    bogus_command(ch, F_ON(F_USE_FK, ps) ? "F1" : "?");

	}
    }

    for(current = first_attline(current); current;){	/* clean up */
	ctmp = current->next;
	free_attline(&current);
	current = ctmp;
    }

    if(screen.titlecolor)
      free_color_pair(&screen.titlecolor);
}


/*----------------------------------------------------------------------
  allocate and attach a fresh attachment line struct

  Args: current -- display line to attache new struct to

  Returns: newly alloc'd attachment display line
  ----*/
ATDISP_S *
new_attline(ATDISP_S **current)
{
    ATDISP_S *p;

    p = (ATDISP_S *)fs_get(sizeof(ATDISP_S));
    memset((void *)p, 0, sizeof(ATDISP_S));
    if(current){
	if(*current){
	    p->next	     = (*current)->next;
	    (*current)->next = p;
	    p->prev	     = *current;
	    if(p->next)
	      p->next->prev = p;
	}

	*current = p;
    }

    return(p);
}


/*----------------------------------------------------------------------
  Release system resources associated with attachment display line

  Args: p -- line to free

  Result: 
  ----*/
void
free_attline(ATDISP_S **p)
{
    if(p){
	if((*p)->dstring)
	  fs_give((void **)&(*p)->dstring);

	fs_give((void **)p);
    }
}


/*----------------------------------------------------------------------
  Manage display of the attachment screen menu body.

  Args: ps -- pine struct pointer
	current -- currently selected display line
	screen -- reference points for display painting

  Result: 
 */
int
attachment_screen_updater(struct pine *ps, ATDISP_S *current, ATT_SCREEN_S *screen)
{
    int	      dline, return_line = HEADER_ROWS(ps);
    ATDISP_S *top_line, *ctmp;

    /* calculate top line of display */
    dline = 0;
    ctmp = top_line = first_attline(current);
    do
      if(((dline++)%(ps->ttyo->screen_rows-HEADER_ROWS(ps)-FOOTER_ROWS(ps)))==0)
	top_line = ctmp;
    while(ctmp != current && (ctmp = next_attline(ctmp)));

#ifdef _WINDOWS
    /* Don't know how to manage scroll bar for attachment screen yet. */
    scroll_setrange (0L, 0L);
#endif

    /* mangled body or new page, force redraw */
    if(ps->mangled_body || screen->top_line != top_line)
      screen->current = NULL;

    /* loop thru painting what's needed */
    for(dline = 0, ctmp = top_line;
	dline < ps->ttyo->screen_rows - FOOTER_ROWS(ps) - HEADER_ROWS(ps);
	dline++, ctmp = next_attline(ctmp)){

	/*
	 * only fall thru painting if something needs painting...
	 */
	if(!(!screen->current || ctmp == screen->current || ctmp == current))
	  continue;

	if(ctmp && ctmp->dstring){
	    char *p = tmp_20k_buf;
	    int   i, col, x = 0, totlen;

	    if(F_ON(F_FORCE_LOW_SPEED,ps) || ps->low_speed){
		x = 2;
		if(ctmp == current){
		    return_line = dline + HEADER_ROWS(ps);
		    PutLine0(dline + HEADER_ROWS(ps), 0, "->");
		}
		else
		  PutLine0(dline + HEADER_ROWS(ps), 0, "  ");

		/*
		 * Only paint lines if we have to...
		 */
		if(screen->current)
		  continue;
	    }
	    else if(ctmp == current){
		return_line = dline + HEADER_ROWS(ps);
		StartInverse();
	    }

	    totlen = strlen(ctmp->dstring);

	    /*
	     * Copy the value to a temp buffer expanding tabs.
	     * Assume the caller set the widths so as not to overflow the
	     * right margin.
	     */
	    for(i=0,col=x; ctmp->dstring[i]; i++){
		if(ctmp->dstring[i] == TAB){
		    if((p-tmp_20k_buf) < SIZEOF_20KBUF-8)
		      do
		        *p++ = ' ';
		      while((++col)&0x07);
		}
		else{
		    col += width_at_this_position((unsigned char *) &ctmp->dstring[i], totlen-i);
		    if((p-tmp_20k_buf) < SIZEOF_20KBUF)
		      *p++ = ctmp->dstring[i];

		}
	    }

	    if((p-tmp_20k_buf) < SIZEOF_20KBUF)
	      *p = '\0';

	    PutLine0(dline + HEADER_ROWS(ps), x, tmp_20k_buf + x);

	    if(ctmp == current
	       && !(F_ON(F_FORCE_LOW_SPEED,ps) || ps->low_speed))
	      EndInverse();
	}
	else
	  ClearLine(dline + HEADER_ROWS(ps));
    }

    ps->mangled_body = 0;
    screen->top_line = top_line;
    screen->current  = current;
    return(return_line);
}


/*----------------------------------------------------------------------
  Redraw the attachment screen based on the global "att_screen" struct

  Args: none

  Result: 
  ----*/
void
attachment_screen_redrawer(void)
{
    bitmap_t	 bitmap;

    update_att_screen_titlebar();
    ps_global->mangled_header = 0;
    ClearLine(1);

    ps_global->mangled_body = 1;
    (void)attachment_screen_updater(ps_global,att_screen->current,att_screen);

    setbitmap(bitmap);
    draw_keymenu(&att_index_keymenu, bitmap, ps_global->ttyo->screen_cols,
		 1-FOOTER_ROWS(ps_global), 0, SameMenu);
}


void
update_att_screen_titlebar(void)
{
    long        raw_msgno;
    COLOR_PAIR *returned_color = NULL;
    COLOR_PAIR *titlecolor = NULL;
    int         colormatch;
    SEARCHSET  *ss = NULL;
    PAT_STATE  *pstate = NULL;

    if(ps_global->titlebar_color_style != TBAR_COLOR_DEFAULT){
	raw_msgno = mn_m2raw(ps_global->msgmap, mn_get_cur(ps_global->msgmap));
	ss = mail_newsearchset();
	ss->first = ss->last = (unsigned long) raw_msgno;

	if(ss){
	    colormatch = get_index_line_color(ps_global->mail_stream,
					      ss, &pstate, &returned_color);
	    mail_free_searchset(&ss);

	    /*
	     * This is a bit tricky. If there is a colormatch but
	     * returned_color
	     * is NULL, that means that the user explicitly wanted the
	     * Normal color used in this index line, so that is what we
	     * use. If no colormatch then we will use the TITLE color
	     * instead of Normal.
	     */
	    if(colormatch){
		if(returned_color)
		  titlecolor = returned_color;
		else
		  titlecolor = new_color_pair(ps_global->VAR_NORM_FORE_COLOR,
					      ps_global->VAR_NORM_BACK_COLOR);
	    }

	    if(titlecolor
	       && ps_global->titlebar_color_style == TBAR_COLOR_REV_INDEXLINE){
		char cbuf[MAXCOLORLEN+1];

		strncpy(cbuf, titlecolor->fg, sizeof(cbuf));
		cbuf[sizeof(cbuf)-1] = '\0';
		strncpy(titlecolor->fg, titlecolor->bg, MAXCOLORLEN);
		titlecolor->fg[MAXCOLORLEN] = '\0';
		strncpy(titlecolor->bg, cbuf, MAXCOLORLEN);
		titlecolor->bg[MAXCOLORLEN] = '\0';
	    }
	}
	
	/* Did the color change? */
	if((!titlecolor && att_screen->titlecolor)
	   ||
	   (titlecolor && !att_screen->titlecolor)
	   ||
	   (titlecolor && att_screen->titlecolor
	    && (strcmp(titlecolor->fg, att_screen->titlecolor->fg)
		|| strcmp(titlecolor->bg, att_screen->titlecolor->bg)))){

	    if(att_screen->titlecolor)
	      free_color_pair(&att_screen->titlecolor);

	    att_screen->titlecolor = titlecolor;
	    titlecolor = NULL;
	}

	if(titlecolor)
	  free_color_pair(&titlecolor);
    }

    set_titlebar(_("ATTACHMENT INDEX"), ps_global->mail_stream,
		 ps_global->context_current, ps_global->cur_folder,
		 ps_global->msgmap, 1, MessageNumber, 0, 0,
		 att_screen->titlecolor);
}


/*----------------------------------------------------------------------
  Seek back from the given display line to the beginning of the list

  Args: p -- display linked list

  Result: 
  ----*/
ATDISP_S *
first_attline(ATDISP_S *p)
{
    while(p && p->prev)
      p = p->prev;

    return(p);
}


int
init_att_progress(char *msg, MAILSTREAM *stream, struct mail_bodystruct *body)
{
    if((save_att_length = body->size.bytes) != 0){
	/* if there are display filters, factor in extra copy */
	if(body->type == TYPETEXT && ps_global->VAR_DISPLAY_FILTERS)
	  save_att_length += body->size.bytes;

	/* if remote folder and segment not cached, factor in IMAP fetch */
	if(stream && stream->mailbox && IS_REMOTE(stream->mailbox)
	   && !((body->type == TYPETEXT && body->contents.text.data)
		|| ((body->type == TYPEMESSAGE)
		    && body->nested.msg && body->nested.msg->text.text.data)
		|| body->contents.text.data))
	  save_att_length += body->size.bytes;

	gf_filter_init();			/* reset counters */
	pine_gets_bytes(1);
	save_att_piped(1);
	return(busy_cue(msg, save_att_percent, 0));
    }

    return(0);
}


long
save_att_piped(int reset)
{
    static long x;
    long	y;

    if(reset){
	x = y = 0L;
    }
    else if((y = gf_bytes_piped()) >= x){
	x = y;
	y = 0;
    }

    return(x + y);
}


int
save_att_percent(void)
{
    int i = (int) (((pine_gets_bytes(0) + save_att_piped(0)) * 100)
							   / save_att_length);
    return(MIN(i, 100));
}


/*----------------------------------------------------------------------
  Save the given attachment associated with the given message no

  Args: ps

  Result: 
  ----*/
void
save_attachment(int qline, long int msgno, ATTACH_S *a)
{
    if(ps_global->restricted){
        q_status_message(SM_ORDER | SM_DING, 0, 4,
			 "Alpine demo can't save attachments");
        return;
    }

    if(MIME_MSG_A(a))
      save_msg_att(msgno, a);
    else if(MIME_DGST_A(a))
      save_digest_att(msgno, a);
    else if(MIME_VCARD_A(a))
      save_vcard_att(ps_global, qline, msgno, a);
    else
      write_attachment(qline, msgno, a, "SAVE");
}


/*----------------------------------------------------------------------
  Save the given attachment associated with the given message no

  Args: ps

  Result: 
  ----*/
void
export_attachment(int qline, long int msgno, ATTACH_S *a)
{
    if(ps_global->restricted){
        q_status_message(SM_ORDER | SM_DING, 0, 4,
			 "Alpine demo can't export attachments");
        return;
    }

    if(MIME_MSG_A(a))
      export_msg_att(msgno, a);
    else if(MIME_DGST_A(a))
      export_digest_att(msgno, a);
    else
      q_status_message1(SM_ORDER, 0, 3,
     _("Can't Export %s. Use \"Save\" to write file, \"<\" to leave index."),
	 body_type_names(a->body->type));
}


void
write_attachment(int qline, long int msgno, ATTACH_S *a, char *method)
{
    char	filename[MAXPATH+1], full_filename[MAXPATH+1],
	        title_buf[64], *err;
    int         r, rflags = GER_NONE, we_cancel = 0;
    static HISTORY_S *history = NULL;
    static ESCKEY_S att_save_opts[] = {
	{ctrl('T'), 10, "^T", N_("To Files")},
	{-1, 0, NULL, NULL},
	{-1, 0, NULL, NULL},
	{-1, 0, NULL, NULL}};

    /*-------  Figure out suggested file name ----*/
    filename[0] = full_filename[0] = '\0';
    (void) get_filename_parameter(filename, sizeof(filename), a->body, NULL);

    dprint((9, "export_attachment(name: %s)\n",
	   filename ? filename : "?"));

    r = 0;
#if	!defined(DOS) && !defined(MAC) && !defined(OS2)
    if(ps_global->VAR_DOWNLOAD_CMD && ps_global->VAR_DOWNLOAD_CMD[0]){
	att_save_opts[++r].ch  = ctrl('V');
	att_save_opts[r].rval  = 12;
	att_save_opts[r].name  = "^V";
	att_save_opts[r].label = N_("Downld Msg");
    }
#endif	/* !(DOS || MAC) */

    if(F_ON(F_ENABLE_TAB_COMPLETE,ps_global)){
	att_save_opts[++r].ch  =  ctrl('I');
	att_save_opts[r].rval  = 11;
	att_save_opts[r].name  = "TAB";
	att_save_opts[r].label = N_("Complete");
    }

    att_save_opts[++r].ch = -1;

    snprintf(title_buf, sizeof(title_buf), "%s ATTACHMENT", method);
    title_buf[sizeof(title_buf)-1] = '\0';

    r = get_export_filename(ps_global, filename, NULL, full_filename,
			    sizeof(filename), "attachment", title_buf,
			    att_save_opts, &rflags, qline, GE_SEQ_SENSITIVE, &history);

    if(r < 0){
	switch(r){
	  case -1:
	    cmd_cancelled((char *) lcase((unsigned char *) title_buf + 1) - 1);
	    break;

	  case -2:
	    q_status_message1(SM_ORDER, 0, 2,
			      _("Can't save to file outside of %s"),
			      ps_global->VAR_OPER_DIR);
	    break;
	}

	return;
    }
#if	!defined(DOS) && !defined(MAC) && !defined(OS2)
    else if(r == 12){			/* Download */
	char     cmd[MAXPATH], *tfp = NULL;
	PIPE_S  *syspipe;
	gf_io_t  pc;
	long     len;
	STORE_S *store;
	char     prompt_buf[256];

	if(ps_global->restricted){
	    q_status_message(SM_ORDER | SM_DING, 3, 3,
			     "Download disallowed in restricted mode");
	    return;
	}

	err = NULL;
	tfp = temp_nam(NULL, "pd");
	dprint((1, "Download attachment called!\n"));
	if((store = so_get(FileStar, tfp, WRITE_ACCESS|OWNER_ONLY|WRITE_TO_LOCALE)) != NULL){

	    snprintf(prompt_buf, sizeof(prompt_buf), "Saving to \"%s\"", tfp);
	    prompt_buf[sizeof(prompt_buf)-1] = '\0';
	    we_cancel = init_att_progress(prompt_buf,
					  ps_global->mail_stream,
					  a->body);

	    gf_set_so_writec(&pc, store);
	    if((err = detach(ps_global->mail_stream, msgno,
			    a->number, 0L, &len, pc, NULL, 0)) != NULL)
	      q_status_message2(SM_ORDER | SM_DING, 3, 5,
			       "%s: Error writing attachment to \"%s\"",
				err, tfp);

	    /* cancel regardless, so it doesn't get in way of xfer */
	    cancel_busy_cue(0);

	    gf_clear_so_writec(store);
	    if(so_give(&store))		/* close file */
	      err = "Error writing tempfile for download";

	    if(!err){
		build_updown_cmd(cmd, sizeof(cmd), ps_global->VAR_DOWNLOAD_CMD_PREFIX,
				 ps_global->VAR_DOWNLOAD_CMD, tfp);
		if((syspipe = open_system_pipe(cmd, NULL, NULL,
					      PIPE_USER | PIPE_RESET,
					      0, pipe_callback, pipe_report_error)) != NULL)
		  (void)close_system_pipe(&syspipe, NULL, pipe_callback);
		else
		  q_status_message(SM_ORDER | SM_DING, 3, 3,
				   err = "Error running download command");
	    }
	}
	else
	  q_status_message(SM_ORDER | SM_DING, 3, 3,
			   err = "Error building temp file for download");

	if(tfp){
	    our_unlink(tfp);
	    fs_give((void **)&tfp);
	}

	if(!err)
	  q_status_message1(SM_ORDER, 0, 4, "Part %s downloaded",
			    a->number);
			    
	return;
    }
#endif	/* !(DOS || MAC) */

    (void) write_attachment_to_file(ps_global->mail_stream, msgno, a,
				    rflags, full_filename);
}


/*
 * Args  stream --
 *       msgno  -- raw message number
 *       a      -- attachment struct
 *       flags  -- comes from get_export_filename
 *             GER_OVER -- the file was truncated before we wrote
 *           GER_APPEND -- the file was not truncated prior to our writing,
 *                         so we were appending
 *                 else -- the file didn't previously exist
 *       file   -- the full pathname of the file
 *
 * Returns  1 for successful write, 0 for nothing to write, -1 for error
 */
int
write_attachment_to_file(MAILSTREAM *stream, long int msgno, ATTACH_S *a, int flags, char *file)
{
    char       *l_string, sbuf[256], *err;
    int         is_text, we_cancel = 0;
    long        len, orig_size;
    gf_io_t     pc;
    STORE_S    *store;

    if(!(a && a->body && a->number && a->number[0] && file && file[0]
	 && stream))
      return 0;

    is_text = (a && a->body && a->body->type == TYPETEXT);

    if(flags & GER_APPEND)
      orig_size = name_file_size(file);

    store = so_get(FileStar, file, WRITE_ACCESS | (is_text ? WRITE_TO_LOCALE : 0));
    if(store == NULL){
	q_status_message2(SM_ORDER | SM_DING, 3, 5,
			  /* TRANSLATORS: Error opening destination <filename>: <error text> */
			  _("Error opening destination %s: %s"),
			  file, error_description(errno));
	return -1;
    }

    snprintf(sbuf, sizeof(sbuf), "Saving to \"%s\"", file);
    sbuf[sizeof(sbuf)-1] = '\0';
    we_cancel = init_att_progress(sbuf, stream, a->body);

    gf_set_so_writec(&pc, store);
    err = detach(stream, msgno, a->number, 0L, &len, pc, NULL, 0);
    gf_clear_so_writec(store);

    if(we_cancel)
      cancel_busy_cue(0);

    if(so_give(&store))			/* close file */
      err = error_description(errno);

    if(err){
	if(!(flags & (GER_APPEND | GER_OVER)))
	  our_unlink(file);
	else
	  our_truncate(file, (flags & GER_APPEND) ? (off_t) orig_size : (off_t) 0);

	q_status_message2(SM_ORDER | SM_DING, 3, 5,
			  /* TRANSLATORS: <error text>: Error writing attachment to <filename> */
			  _("%s: Error writing attachment to \"%s\""),
			  err, file);
	return -1;
    }
    else{
        l_string = cpystr(byte_string(len));
        q_status_message8(SM_ORDER, 0, 4,
	     "Part %s, %s%s %s to \"%s\"%s%s%s",
			  a->number, 
			  is_text
			    ? comatose(a->body->size.lines) : l_string,
			  is_text ? " lines" : "",
			  flags & GER_OVER
			      ? "overwritten"
			      : flags & GER_APPEND ? "appended" : "written",
			  file,
			  (is_text || len == a->body->size.bytes)
			    ? "" : "(decoded from ",
                          (is_text || len == a->body->size.bytes)
			    ? "" : byte_string(a->body->size.bytes),
			  is_text || len == a->body->size.bytes
			    ? "" : ")");
        fs_give((void **)&l_string);
	return 1;
    }
}


char *
write_attached_msg(long int msgno, ATTACH_S **ap, STORE_S *store, int newfile)
{
    char      *err = NULL;
    long      start_of_append;
    gf_io_t   pc;
    MESSAGECACHE *mc;

    if(ap && *ap && (*ap)->body && (*ap)->body->nested.msg
       && (*ap)->body->nested.msg->env){
	start_of_append = so_tell(store);

	gf_set_so_writec(&pc, store);
	if(!(ps_global->mail_stream && msgno > 0L
	   && msgno <= ps_global->mail_stream->nmsgs
	   && (mc = mail_elt(ps_global->mail_stream, msgno)) && mc->valid))
	  mc = NULL;
	
	if(!bezerk_delimiter((*ap)->body->nested.msg->env, mc, pc, newfile)
	   || !format_msg_att(msgno, ap, NULL, pc, FM_NOINDENT))
	  err = error_description(errno);

	gf_clear_so_writec(store);

	if(err)
	  ftruncate(fileno((FILE *)store->txt), (off_t) start_of_append);
    }
    else
      err = "Can't export message. Missing Envelope data";

    return(err);
}


/*----------------------------------------------------------------------
  Save the attachment message/rfc822 to specified folder

  Args: 

  Result: 
  ----*/
void
save_msg_att(long int msgno, ATTACH_S *a)
{
    char	  newfolder[MAILTMPLEN], *save_folder, *flags = NULL;
    char          date[64], nmsgs[80];
    CONTEXT_S    *cntxt = NULL;
    int		  our_stream = 0, rv;
    MAILSTREAM   *save_stream;
    MESSAGECACHE *mc;

    snprintf(nmsgs, sizeof(nmsgs), _("Attached Msg (part %s) "), a->number);
    nmsgs[sizeof(nmsgs)-1] = '\0';
    if(save_prompt(ps_global, &cntxt, newfolder, sizeof(newfolder), nmsgs,
		   a->body->nested.msg->env, msgno, a->number, NULL, NULL)){
	if(strucmp(newfolder, ps_global->inbox_name) == 0){
	    save_folder = ps_global->VAR_INBOX_PATH;
	    cntxt = NULL;
	}
	else
	  save_folder = newfolder;

	save_stream = save_msg_stream(cntxt, save_folder, &our_stream);

	mc = (msgno > 0L && ps_global->mail_stream
	      && msgno <= ps_global->mail_stream->nmsgs)
	      ? mail_elt(ps_global->mail_stream, msgno) : NULL;
	flags = flag_string(ps_global->mail_stream, msgno, F_ANS|F_FLAG|F_SEEN|F_KEYWORD);
	if(mc && mc->day)
	  mail_date(date, mc);
	else
	  *date = '\0';

	if(pith_opt_save_size_changed_prompt)
	  (*pith_opt_save_size_changed_prompt)(0L, SSCP_INIT);

	rv = save_msg_att_work(msgno, a, save_stream, save_folder, cntxt, date);

	if(pith_opt_save_size_changed_prompt)
	  (*pith_opt_save_size_changed_prompt)(0L, SSCP_END);

	if(flags)
	  fs_give((void **) &flags);

	if(rv == 1)
	  q_status_message2(SM_ORDER, 0, 4,
		       _("Attached message (part %s) saved to \"%s\""),
			    a->number, 
			    save_folder);
	else if(rv == -1)
	  cmd_cancelled("Attached message Save");
	/* else whatever broke in save_fetch_append shoulda bitched */

	if(our_stream)
	  mail_close(save_stream);
    }
}


/*----------------------------------------------------------------------
  Save the message/rfc822 in the given digest to the specified folder

  Args: 

  Result: 
  ----*/
void
save_digest_att(long int msgno, ATTACH_S *a)
{
    char	 newfolder[MAILTMPLEN], *save_folder,
		 date[64], nmsgs[80];
    CONTEXT_S   *cntxt = NULL;
    int		 our_stream = 0, rv, cnt = 0;
    MAILSTREAM  *save_stream;
    ATTACH_S    *ap;

    for(ap = a + 1;
	ap->description
	  && !strncmp(a->number, ap->number, strlen(a->number));
	ap++)
      if(MIME_MSG(ap->body->type, ap->body->subtype))
	cnt++;
    
    snprintf(nmsgs, sizeof(nmsgs), "%d Msg Digest (part %s) ", cnt, a->number);
    nmsgs[sizeof(nmsgs)-1] = '\0';

    if(save_prompt(ps_global, &cntxt, newfolder, sizeof(newfolder),
		   nmsgs, NULL, 0, NULL, NULL, NULL)){
	save_folder = (strucmp(newfolder, ps_global->inbox_name) == 0)
			? ps_global->VAR_INBOX_PATH : newfolder;

	save_stream = save_msg_stream(cntxt, save_folder, &our_stream);

	if(pith_opt_save_size_changed_prompt)
	  (*pith_opt_save_size_changed_prompt)(0L, SSCP_INIT);

	for(ap = a + 1;
	    ap->description
	      && !strncmp(a->number, ap->number, strlen(a->number));
	    ap++)
	  if(MIME_MSG(ap->body->type, ap->body->subtype)){
	      *date = '\0';
	      rv = save_msg_att_work(msgno, ap, save_stream, save_folder, cntxt, date);
	      if(rv != 1)
		break;
	  }

	if(pith_opt_save_size_changed_prompt)
	  (*pith_opt_save_size_changed_prompt)(0L, SSCP_END);

	if(rv == 1)
	  q_status_message2(SM_ORDER, 0, 4,
			    _("Attached digest (part %s) saved to \"%s\""),
			    a->number, 
			    save_folder);
	else if(rv == -1)
	  cmd_cancelled("Attached digest Save");
	/* else whatever broke in save_fetch_append shoulda bitched */

	if(our_stream)
	  mail_close(save_stream);
    }
}


int
save_msg_att_work(long int msgno, ATTACH_S *a, MAILSTREAM *save_stream,
		  char *save_folder, CONTEXT_S *cntxt, char *date)
{
    STORE_S      *so;
    int           rv = 0;

    if(a && a->body && MIME_MSG(a->body->type, a->body->subtype)){
	if((so = so_get(CharStar, NULL, WRITE_ACCESS)) != NULL){
	    *date = '\0';
	    rv = save_fetch_append(ps_global->mail_stream, msgno,
				   a->number,
				   save_stream, save_folder, cntxt,
				   a->body->size.bytes,
				   NULL, date, so);
	}
	else{
	    dprint((1, "Can't allocate store for save: %s\n",
		     error_description(errno)));
	    q_status_message(SM_ORDER | SM_DING, 3, 4,
			   _("Problem creating space for message text."));
	    rv = 0;
	}
    }

    return(rv);
}


/*----------------------------------------------------------------------
  Export the attachment message/rfc822 to specified file

  Args: 

  Result: 
  ----*/
void
export_msg_att(long int msgno, ATTACH_S *a)
{
    char      filename[MAXPATH+1], full_filename[MAXPATH+1], *err;
    int	      rv, rflags = GER_NONE, i = 1;
    ATTACH_S *ap = a;
    STORE_S  *store;
    static HISTORY_S *history = NULL;
    static ESCKEY_S opts[] = {
	{ctrl('T'), 10, "^T", N_("To Files")},
	{-1, 0, NULL, NULL},
	{-1, 0, NULL, NULL}};

    if(F_ON(F_ENABLE_TAB_COMPLETE,ps_global)){
	opts[i].ch    =  ctrl('I');
	opts[i].rval  = 11;
	opts[i].name  = "TAB";
	opts[i].label = N_("Complete");
    }

    filename[0] = full_filename[0] = '\0';

    rv = get_export_filename(ps_global, filename, NULL, full_filename,
			     sizeof(filename), "msg attachment",
			     /* TRANSLATORS: Message Attachment (a screen title) */
			     _("MSG ATTACHMENT"), opts,
			     &rflags, -FOOTER_ROWS(ps_global),
			     GE_IS_EXPORT | GE_SEQ_SENSITIVE, &history);

    if(rv < 0){
	switch(rv){
	  case -1:
	    cmd_cancelled("Export");
	    break;

	  case -2:
	    q_status_message1(SM_ORDER, 0, 2,
			      _("Can't export to file outside of %s"),
			      ps_global->VAR_OPER_DIR);
	    break;
	}

	return;
    }

    /* With name in hand, allocate storage object and save away... */
    if((store = so_get(FileStar, full_filename, WRITE_ACCESS)) != NULL){
	if((err = write_attached_msg(msgno, &ap, store, !(rflags & GER_APPEND))) != NULL)
	  q_status_message(SM_ORDER | SM_DING, 3, 4, err);
	else
          q_status_message3(SM_ORDER, 0, 4,
			  _("Attached message (part %s) %s to \"%s\""),
			    a->number, 
			    rflags & GER_OVER
			      ? _("overwritten")
			      : rflags & GER_APPEND ? _("appended") : _("written"),
			    full_filename);

	if(so_give(&store))
	  q_status_message2(SM_ORDER | SM_DING, 3, 4,
			   _("Error writing %s: %s"),
			   full_filename, error_description(errno));
    }
    else
      q_status_message2(SM_ORDER | SM_DING, 3, 4,
		    /* TRANSLATORS: Error opening file <filename> to export message: <error text> */
		    _("Error opening file \"%s\" to export message: %s"),
			full_filename, error_description(errno));
}


/*----------------------------------------------------------------------
  Export the message/rfc822 in the given digest to the specified file

  Args: 

  Result: 
  ----*/
void
export_digest_att(long int msgno, ATTACH_S *a)
{
    char      filename[MAXPATH+1], full_filename[MAXPATH+1], *err = NULL;
    int	      rv, rflags = GER_NONE, i = 1;
    long      count = 0L;
    ATTACH_S *ap;
    static HISTORY_S *history = NULL;
    STORE_S  *store;
    static ESCKEY_S opts[] = {
	{ctrl('T'), 10, "^T", N_("To Files")},
	{-1, 0, NULL, NULL},
	{-1, 0, NULL, NULL}};

    if(F_ON(F_ENABLE_TAB_COMPLETE,ps_global)){
	opts[i].ch    =  ctrl('I');
	opts[i].rval  = 11;
	opts[i].name  = "TAB";
	opts[i].label = N_("Complete");
    }

    filename[0] = full_filename[0] = '\0';

    rv = get_export_filename(ps_global, filename, NULL, full_filename,
			     sizeof(filename), "digest", _("DIGEST ATTACHMENT"),
			     opts, &rflags, -FOOTER_ROWS(ps_global),
			     GE_IS_EXPORT | GE_SEQ_SENSITIVE, &history);

    if(rv < 0){
	switch(rv){
	  case -1:
	    cmd_cancelled("Export");
	    break;

	  case -2:
	    q_status_message1(SM_ORDER, 0, 2,
			      _("Can't export to file outside of %s"),
			      ps_global->VAR_OPER_DIR);
	    break;
	}

	return;
    }

    /* With name in hand, allocate storage object and save away... */
    if((store = so_get(FileStar, full_filename, WRITE_ACCESS)) != NULL){
	count = 0;

	for(ap = a + 1;
	    ap->description
	      && !strncmp(a->number, ap->number, strlen(a->number))
	      && !err;
	    ap++){
	    if(MIME_MSG(ap->body->type, ap->body->subtype)){
		count++;
		err = write_attached_msg(msgno, &ap, store,
					 !count && !(rflags & GER_APPEND));
	    }
	}

	if(so_give(&store))
	  err = error_description(errno);

	if(err){
	    q_status_message1(SM_ORDER | SM_DING, 3, 3,
			      _("Error exporting: %s"), err);
	    q_status_message1(SM_ORDER | SM_DING, 3, 3,
			_("%s messages exported before error occurred"), err);
	}
	else
          q_status_message4(SM_ORDER, 0, 4,
		"%s messages in digest (part %s) %s to \"%s\"",
			    long2string(count),
			    a->number, 
			    rflags & GER_OVER
			      ? "overwritten"
			      : rflags & GER_APPEND ? "appended" : "written",
			    full_filename);
    }
    else
      q_status_message2(SM_ORDER | SM_DING, 3, 4,
		    _("Error opening file \"%s\" to export digest: %s"),
			full_filename, error_description(errno));
}


/*----------------------------------------------------------------------
  Print the given attachment associated with the given message no

  Args: ps

  Result: 
  ----*/
void
print_attachment(int qline, long int msgno, ATTACH_S *a)
{
    char prompt[250];

    if(ps_global->restricted){
        q_status_message(SM_ORDER | SM_DING, 0, 4,
			 "Alpine demo can't Print attachments");
        return;
    }

    snprintf(prompt, sizeof(prompt), "attach%s %s",
	    (a->body->type == TYPETEXT) ? "ment" : "ed message",
	    MIME_DGST_A(a) ? "digest" : a->number);
    prompt[sizeof(prompt)-1] = '\0';
    if(open_printer(prompt) >= 0){
	if(MIME_MSG_A(a))
	  (void) print_msg_att(msgno, a, 1);
	else if(MIME_DGST_A(a))
	  print_digest_att(msgno, a);
	else
	  (void) decode_text(a, msgno, print_char, NULL, QStatus, FM_NOINDENT);

	close_printer();
    }
}


/*
 * Print the attachment message/rfc822 to specified file
 *
 * Returns 1 on success, 0 on failure.
 */
int
print_msg_att(long int msgno, ATTACH_S *a, int first)
{
    ATTACH_S *ap = a;
    MESSAGECACHE *mc;

    if(!(ps_global->mail_stream && msgno > 0L
       && msgno <= ps_global->mail_stream->nmsgs
       && (mc = mail_elt(ps_global->mail_stream, msgno)) && mc->valid))
      mc = NULL;

    if(((!first && F_ON(F_AGG_PRINT_FF, ps_global)) ? print_char(FORMFEED) : 1)
       && pine_mail_fetchstructure(ps_global->mail_stream, msgno, NULL)
       && (F_ON(F_FROM_DELIM_IN_PRINT, ps_global)
	     ? bezerk_delimiter(a->body->nested.msg->env, mc, print_char, !first)
	     : 1)
       && format_msg_att(msgno, &ap, NULL, print_char, FM_NOINDENT))
      return(1);


    q_status_message2(SM_ORDER | SM_DING, 3, 3,
		      _("Error printing message %s, part %s"),
		      long2string(msgno), a->number);
    return(0);
}


/*----------------------------------------------------------------------
  Print the attachment message/rfc822 to specified file

  Args: 

  Result: 
  ----*/
void
print_digest_att(long int msgno, ATTACH_S *a)
{
    ATTACH_S *ap;
    int	      next = 0;

    for(ap = a + 1;
	ap->description
	  && !strncmp(a->number, ap->number, strlen(a->number));
	ap++){
	if(MIME_MSG(ap->body->type, ap->body->subtype)){
	    char *p = part_desc(ap->number, ap->body->nested.msg->body,
				0, 80, FM_NOINDENT, print_char);
	    if(p){
		q_status_message1(SM_ORDER | SM_DING, 3, 3,
				  _("Can't print digest: %s"), p);
		break;
	    }
	    else if(!print_msg_att(msgno, ap, !next))
	      break;

	    next++;
	}
    }
}


/*----------------------------------------------------------------------
  Unpack and display the given attachment associated with given message no.

  Args: msgno -- message no attachment is part of
	a -- attachment to display

  Returns: 0 on success, non-zero (and error message queued) otherwise
  ----*/        
int
display_attachment(long int msgno, ATTACH_S *a, int flags)
{
    char    *filename = NULL;
    char     sender_filename[1000];
    char    *extp = NULL;
    STORE_S *store;
    gf_io_t  pc;
    char    *err;
    int      we_cancel = 0, rv;
    char     prefix[70];
    char     ext[32];
    char     mtype[128];

    /*------- Display the attachment -------*/
    if(dispatch_attachment(a) == MCD_NONE){
        /*----- Can't display this type ------*/
	if(a->body->encoding < ENCOTHER)
	  q_status_message4(SM_ORDER | SM_DING, 3, 5,
	     /* TRANSLATORS: Don't know how to display <certain type> attachments. <might say Try Save.> */
	     _("Don't know how to display %s%s%s attachments.%s"),
			    body_type_names(a->body->type),
			    a->body->subtype ? "/" : "",
			    a->body->subtype ? a->body->subtype :"",
			    (flags & DA_SAVE) ? _(" Try Save.") : "");
	else
	  q_status_message1(SM_ORDER | SM_DING, 3, 5,
			    _("Don't know how to unpack \"%s\" encoding"),
			    body_encodings[(a->body->encoding <= ENCMAX)
					     ? a->body->encoding : ENCOTHER]);

        return(1);
    }
    else if(!(a->can_display & MCD_EXTERNAL)){
	if(a->body->type == TYPEMULTIPART){
	    if(a->body->subtype){
		if(!strucmp(a->body->subtype, "digest"))
		  display_digest_att(msgno, a, flags);
		else
		  q_status_message1(SM_ORDER, 3, 5,
				   _("Can't display Multipart/%s"),
				   a->body->subtype);
	    }
	    else
	      q_status_message(SM_ORDER, 3, 5,
			       _("Can't display unknown Multipart Subtype"));
	}
	else if(MIME_VCARD_A(a))
	  display_vcard_att(msgno, a, flags);
	else if(a->body->type == TYPETEXT){
	  do{
	    rv = display_text_att(msgno, a, flags);
	  } while(rv == MC_FULLHDR);
	}
	else if(a->body->type == TYPEMESSAGE){
	  do{
	    rv = display_msg_att(msgno, a, flags);
	  } while(rv == MC_FULLHDR);
	}

	ps_global->mangled_screen = 1;
	return(0);
    }

    /* arrive here if MCD_EXTERNAL */

    if(F_OFF(F_QUELL_ATTACH_EXTRA_PROMPT, ps_global)
       && (!(flags & DA_DIDPROMPT)))
      if(want_to(_("View selected Attachment"), 'y',
		 0, NO_HELP, WT_NORM) == 'n'){
	  cmd_cancelled(NULL);
	  return(1);
      }

    sender_filename[0] = '\0';
    ext[0] = '\0';
    prefix[0] = '\0';

    if(F_OFF(F_QUELL_ATTACH_EXT_WARN, ps_global)
       && (a->can_display & MCD_EXT_PROMPT)){
	char prompt[256];

	(void) get_filename_parameter(sender_filename, sizeof(sender_filename),
				      a->body, &extp);
	snprintf(prompt, sizeof(prompt),
		"Attachment %s%s unrecognized. %s%s%s", 
		a->body->subtype, 
		strlen(a->body->subtype) > 12 ? "..." : "", 
		(extp && extp[0]) ? "Try open by file extension (." : "Try opening anyway",
		(extp && extp[0]) ? extp : "",
		(extp && extp[0]) ? ")" : "");

	if(want_to(prompt, 'n', 0, NO_HELP, WT_NORM) == 'n'){
	    cmd_cancelled(NULL);
	    return(1);
	}
    }

    /*------ Write the image to a temporary file ------*/

    /* create type/subtype in mtype */
    strncpy(mtype, body_type_names(a->body->type), sizeof(mtype));
    mtype[sizeof(mtype)-1] = '\0';
    if(a->body->subtype){
	strncat(mtype, "/", sizeof(mtype)-strlen(mtype)-1);
	mtype[sizeof(mtype)-1] = '\0';
	strncat(mtype, a->body->subtype, sizeof(mtype)-strlen(mtype)-1);
	mtype[sizeof(mtype)-1] = '\0';
    }

    /*
     * If we haven't already gotten the filename parameter, get it
     * now. It may be used in the temporary filename and possibly
     * for its extension.
     */
    if(!sender_filename[0])
      (void) get_filename_parameter(sender_filename, sizeof(sender_filename),
				    a->body, &extp);

    if(!set_mime_extension_by_type(ext, mtype)){	/* extension from type */
	if(extp && extp[0]){				/* extension from filename */
	    strncpy(ext, extp, sizeof(ext));
	    ext[sizeof(ext)-1] = '\0';
	}
    }

    /* create a temp file */
    if(sender_filename){
	char *p, *q = NULL;

	/* get rid of any extension */
	if(mt_get_file_ext(sender_filename, &q) && q && q > sender_filename)
	  *(q-1) = '\0';

	/* be careful about what is allowed in the filename */
	for(p = sender_filename; *p; p++)
	  if(!(isascii((unsigned char) *p)
	       && (isalnum((unsigned char) *p)
		   || *p == '-' || *p == '_' || *p == '+' || *p == '.' || *p == '=')))
	    break;

	if(!*p)			/* filename was ok to use */
	  snprintf(prefix, sizeof(prefix), "img-%s-", sender_filename);
    }

    /* didn't get it yet */
    if(!prefix[0]){
	snprintf(prefix, sizeof(prefix), "img-%s-", (a->body->subtype)
		 ? a->body->subtype : "unk");
    }

    filename = temp_nam_ext(NULL, prefix, ext);

    if(!filename){
        q_status_message1(SM_ORDER | SM_DING, 3, 5,
                          _("Error \"%s\", Can't create temporary file"),
                          error_description(errno));
        return(1);
    }

    if((store = so_get(FileStar, filename, WRITE_ACCESS|OWNER_ONLY)) == NULL){
        q_status_message2(SM_ORDER | SM_DING, 3, 5,
                          _("Error \"%s\", Can't write file %s"),
                          error_description(errno), filename);
	if(filename){
	    our_unlink(filename);
	    fs_give((void **)&filename);
	}

        return(1);
    }


    if(a->body->size.bytes){
	char msg_buf[128];

	snprintf(msg_buf, sizeof(msg_buf), "Decoding %s%s%s%s",
		a->description ? "\"" : "",
		a->description ? a->description : "attachment number ",
		a->description ? "" : a->number,
		a->description ? "\"" : "");
	msg_buf[sizeof(msg_buf)-1] = '\0';
	we_cancel = init_att_progress(msg_buf, ps_global->mail_stream,
				      a->body);
    }

    if(a->body->type == TYPEMULTIPART){
	char *h = mail_fetch_mime(ps_global->mail_stream, msgno, a->number,
				  NULL, 0L);

	/* Write to store while converting newlines */
	while(h && *h)
	  if(*h == '\015' && *(h+1) == '\012'){
	      so_puts(store, NEWLINE);
	      h += 2;
	  }
	  else
	    so_writec(*h++, store);
    }

    gf_set_so_writec(&pc, store);

    err = detach(ps_global->mail_stream, msgno, a->number, 0L, NULL, pc, NULL, 0);

    gf_clear_so_writec(store);

    if(we_cancel)
      cancel_busy_cue(0);

    so_give(&store);

    if(err){
	q_status_message2(SM_ORDER | SM_DING, 3, 5,
		     "%s: Error saving image to temp file %s",
		     err, filename);
	if(filename){
	    our_unlink(filename);
	    fs_give((void **)&filename);
	}

	return(1);
    }

    /*----- Run the viewer process ----*/
    run_viewer(filename, a->body, a->can_display & MCD_EXT_PROMPT);
    if(filename)
      fs_give((void **)&filename);

    return(0);
}


/*----------------------------------------------------------------------
   Fish the required command from mailcap and run it

  Args: image_file -- The name of the file to pass viewer
	body -- body struct containing type/subtype of part

A side effect may be that scrolltool is called as well if
exec_mailcap_cmd has any substantial output...
 ----*/
void
run_viewer(char *image_file, struct mail_bodystruct *body, int chk_extension)
{
    MCAP_CMD_S *mc_cmd   = NULL;
    int   needs_terminal = 0, we_cancel = 0;

    we_cancel = busy_cue("Displaying attachment", NULL, 0);

    if((mc_cmd = mailcap_build_command(body->type, body->subtype,
				      body, image_file,
				      &needs_terminal, chk_extension)) != NULL){
	if(we_cancel)
	  cancel_busy_cue(-1);

	exec_mailcap_cmd(mc_cmd, image_file, needs_terminal);
	if(mc_cmd->command)
	  fs_give((void **)&mc_cmd->command);
	fs_give((void **)&mc_cmd);
    }
    else{
	if(we_cancel)
	  cancel_busy_cue(-1);

	q_status_message1(SM_ORDER, 3, 4, _("Cannot display %s attachment"),
			  type_desc(body->type, body->subtype,
				    body->parameter, NULL, 1));
    }
}


/*----------------------------------------------------------------------
  Detach and provide for browsing a text body part

  Args: msgno -- raw message number to get part from
	 a -- attachment struct for the desired part

  Result: 
 ----*/
STORE_S *
format_text_att(long int msgno, ATTACH_S *a, HANDLE_S **handlesp)
{
    STORE_S	*store;
    gf_io_t	 pc;

    if((store = so_get(CharStar, NULL, EDIT_ACCESS)) != NULL){
	if(handlesp)
	  init_handles(handlesp);

	gf_set_so_writec(&pc, store);
	(void) decode_text(a, msgno, pc, handlesp, QStatus, FM_DISPLAY);
	gf_clear_so_writec(store);
    }

    return(store);
}


/*----------------------------------------------------------------------
  Detach and provide for browsing a text body part

  Args: msgno -- raw message number to get part from
	 a -- attachment struct for the desired part

  Result: 
 ----*/
int
display_text_att(long int msgno, ATTACH_S *a, int flags)
{
    STORE_S  *store;
    HANDLE_S *handles = NULL;
    int       rv = 0;

    if(msgno > 0L)
      clear_index_cache_ent(ps_global->mail_stream, msgno, 0);

    if((store = format_text_att(msgno, a, &handles)) != NULL){
	rv = scroll_attachment("ATTACHED TEXT", store, CharStar, handles, a, flags);
	free_handles(&handles);
	so_give(&store);	/* free resources associated with store */
    }
    else
      q_status_message(SM_ORDER | SM_DING, 3, 3,
		       _("Error allocating space for attachment."));

    return(rv);
}


/*----------------------------------------------------------------------
  Detach and provide for browsing a body part of type "MESSAGE"

  Args: msgno -- message number to get partrom
	 a -- attachment struct for the desired part

  Result: 
 ----*/
int
display_msg_att(long int msgno, ATTACH_S *a, int flags)
{
    STORE_S	*store;
    gf_io_t	 pc;
    ATTACH_S	*ap = a;
    HANDLE_S *handles = NULL;
    int          rv = 0;

    if(msgno > 0L)
      clear_index_cache_ent(ps_global->mail_stream, msgno, 0);

    /* BUG, should check this return code */
    (void) pine_mail_fetchstructure(ps_global->mail_stream, msgno, NULL);

    /* initialize a storage object */
    if(!(store = so_get(CharStar, NULL, EDIT_ACCESS))){
	q_status_message(SM_ORDER | SM_DING, 3, 3,
			 _("Error allocating space for message."));
	return(rv);
    }

    gf_set_so_writec(&pc, store);

    if(format_msg_att(msgno, &ap, &handles, pc, FM_DISPLAY)){
	if(ps_global->full_header == 2)
	  q_status_message(SM_INFO, 0, 3,
		      _("Full header mode ON.  All header text being included"));

	rv = scroll_attachment((a->body->subtype
			   && !strucmp(a->body->subtype, "delivery-status"))
			     ? "DELIVERY STATUS REPORT" : "ATTACHED MESSAGE",
			  store, CharStar, handles, a, flags);
	free_handles(&handles);
    }

    gf_clear_so_writec(store);

    so_give(&store);	/* free resources associated with store */
    return(rv);
}


/*----------------------------------------------------------------------
  Detach and provide for browsing a multipart body part of type "DIGEST"

  Args: msgno -- message number to get partrom
	 a -- attachment struct for the desired part

  Result: 
 ----*/
void
display_digest_att(long int msgno, ATTACH_S *a, int flags)
{
    STORE_S     *store;
    ATTACH_S	*ap;
    HANDLE_S	*handles = NULL;
    gf_io_t      pc;
    SourceType	 src = CharStar;
    int		 bad_news = 0;

    if(msgno > 0L)
      clear_index_cache_ent(ps_global->mail_stream, msgno, 0);

    /* BUG, should check this return code */
    (void) pine_mail_fetchstructure(ps_global->mail_stream, msgno, NULL);

    if(!(store = so_get(src, NULL, EDIT_ACCESS))){
	q_status_message(SM_ORDER | SM_DING, 3, 3,
			 _("Error allocating space for message."));
	return;
    }

    gf_set_so_writec(&pc, store);

    /*
     * While in a subtype of this message
     */
    for(ap = a + 1;
	ap->description
	  && !strncmp(a->number, ap->number, strlen(a->number))
	  && !bad_news;
	ap++){
	if(ap->body->type == TYPEMESSAGE){
	    char *errstr;

	    if(ap->body->subtype && strucmp(ap->body->subtype, "rfc822")){
		char *tsub;

		tsub = cpystr(ap->body->subtype);
		convert_possibly_encoded_str_to_utf8((char **) &tsub);
		snprintf(tmp_20k_buf, SIZEOF_20KBUF, "Unknown Message subtype: %s", tsub);
		tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';

		if((errstr = format_editorial(tmp_20k_buf,
					     ps_global->ttyo->screen_cols, 0,
					     NULL, pc)) != NULL){
		    q_status_message1(SM_ORDER | SM_DING, 3, 3,
				      _("Can't format digest: %s"), errstr);
		    bad_news++;
		}
		else if(!gf_puts(NEWLINE, pc))
		  bad_news++;

		fs_give((void **) &tsub);
	    }
	    else{
		if((errstr = part_desc(ap->number, ap->body->nested.msg->body,
				      0, ps_global->ttyo->screen_cols, FM_DISPLAY, pc)) != NULL){
		    q_status_message1(SM_ORDER | SM_DING, 3, 3,
				      _("Can't format digest: %s"), errstr);
		    bad_news++;
		}
		else if(!format_msg_att(msgno, &ap, &handles, pc, FM_DISPLAY))
		  bad_news++;
	    }
	}
	else if(ap->body->type == TYPETEXT
		&& decode_text(ap, msgno, pc, NULL, QStatus, FM_DISPLAY))
	  bad_news++;
	else if(!gf_puts("Unknown type in Digest", pc))
	  bad_news++;
    }

    if(!bad_news){
	if(ps_global->full_header == 2)
	  q_status_message(SM_INFO, 0, 3,
		      _("Full header mode ON.  All header text being included"));

	scroll_attachment(_("ATTACHED MESSAGES"), store, src, handles, a, flags);
    }

    free_handles(&handles);

    gf_clear_so_writec(store);
    so_give(&store);	/* free resources associated with store */
}


int
scroll_attachment(char *title, STORE_S *store, SourceType src, HANDLE_S *handles, ATTACH_S *a, int flags)
{
    SCROLL_S  sargs;

    memset(&sargs, 0, sizeof(SCROLL_S));
    sargs.text.text    = so_text(store);
    sargs.text.src     = src;
    sargs.text.desc    = "attachment";
    sargs.text.handles = handles;
    sargs.bar.title    = title;
    sargs.proc.tool    = process_attachment_cmd;
    sargs.proc.data.p  = (void *) a;
    sargs.help.text    = h_mail_text_att_view;
    sargs.help.title   = _("HELP FOR ATTACHED TEXT VIEW");
    sargs.keys.menu    = &att_view_keymenu;
    setbitmap(sargs.keys.bitmap);

    /* First, fix up "back" key */
    if(flags & DA_FROM_VIEW){
	att_view_keymenu.keys[ATV_BACK_KEY].label = N_("MsgText");
    }
    else{
	att_view_keymenu.keys[ATV_BACK_KEY].label = N_("AttchIndex");
    }

    if(!handles){
	clrbitn(ATV_VIEW_HILITE, sargs.keys.bitmap);
	clrbitn(ATV_PREV_URL, sargs.keys.bitmap);
	clrbitn(ATV_NEXT_URL, sargs.keys.bitmap);
    }

    if(F_OFF(F_ENABLE_PIPE, ps_global))
      clrbitn(ATV_PIPE_KEY, sargs.keys.bitmap);

    if(!(a->body->type == TYPETEXT || MIME_MSG_A(a) || MIME_DGST_A(a)))
      clrbitn(ATV_PRINT_KEY, sargs.keys.bitmap);

    /*
     * If message or digest, leave Reply and Save and,
     * conditionally, Bounce on...
     */
    if(MIME_MSG_A(a)){
	if(F_OFF(F_ENABLE_BOUNCE, ps_global))
	  clrbitn(ATV_BOUNCE_KEY, sargs.keys.bitmap);
    }
    else{
	clrbitn(ATV_BOUNCE_KEY, sargs.keys.bitmap);
	clrbitn(ATV_REPLY_KEY, sargs.keys.bitmap);
	clrbitn(ATV_EXPORT_KEY, sargs.keys.bitmap);
    }

    sargs.use_indexline_color = 1;

    if(DA_RESIZE & flags)
      sargs.resize_exit = 1;

#ifdef	_WINDOWS
    scrat_attachp    = a;
    sargs.mouse.popup = scroll_att_popup;
#endif

    return(scrolltool(&sargs));
}


int
process_attachment_cmd(int cmd, MSGNO_S *msgmap, SCROLL_S *sparms)
{
    int rv = 0, n;
    long rawno = mn_m2raw(msgmap, mn_get_cur(msgmap));
#define	AD(X)	((ATTACH_S *) (X)->proc.data.p)

    switch(cmd){
      case MC_EXIT :
	rv = 1;
	break;

      case MC_QUIT :
	ps_global->next_screen = quit_screen;
	break;

      case MC_MAIN :
	ps_global->next_screen = main_menu_screen;
	break;

      case MC_REPLY :
	reply_msg_att(ps_global->mail_stream, rawno, sparms->proc.data.p);
	break;

      case MC_FORWARD :
	forward_attachment(ps_global->mail_stream, rawno, sparms->proc.data.p);
	break;

      case MC_BOUNCE :
	bounce_msg_att(ps_global->mail_stream, rawno, AD(sparms)->number,
		       AD(sparms)->body->nested.msg->env->subject);
	ps_global->mangled_footer = 1;
	break;

      case MC_DELETE :
	delete_attachment(rawno, sparms->proc.data.p);
	break;

      case MC_UNDELETE :
	if(undelete_attachment(rawno, sparms->proc.data.p, &n))
	  q_status_message1(SM_ORDER, 0, 3,
			    "Part %s UNdeleted", AD(sparms)->number);

	break;

      case MC_SAVE :
	save_attachment(-FOOTER_ROWS(ps_global), rawno, sparms->proc.data.p);
	ps_global->mangled_footer = 1;
	break;

      case MC_EXPORT :
	export_attachment(-FOOTER_ROWS(ps_global), rawno, sparms->proc.data.p);
	ps_global->mangled_footer = 1;
	break;

      case MC_PRINTMSG :
	print_attachment(-FOOTER_ROWS(ps_global), rawno, sparms->proc.data.p);
	ps_global->mangled_footer = 1;
	break;

      case MC_PIPE :
	pipe_attachment(rawno, sparms->proc.data.p);
	ps_global->mangled_footer = 1;
	break;

      case MC_FULLHDR :
	ps_global->full_header++;
	if(ps_global->full_header == 1){
	    if(!(ps_global->quote_suppression_threshold
	         && (ps_global->some_quoting_was_suppressed /* || in_index != View*/)))
	      ps_global->full_header++;
	}
	else if(ps_global->full_header > 2)
	  ps_global->full_header = 0;

	switch(ps_global->full_header){
	  case 0:
	    q_status_message(SM_ORDER, 0, 3,
			     _("Display of full headers is now off."));
	    break;

	  case 1:
	    q_status_message1(SM_ORDER, 0, 3,
			  _("Quotes displayed, use %s to see full headers"),
			  F_ON(F_USE_FK, ps_global) ? "F9" : "H");
	    break;

	  case 2:
	    q_status_message(SM_ORDER, 0, 3,
			     _("Display of full headers is now on."));
	    break;

	}

	rv = 1;
	break;

      default:
	panic("Unexpected command case");
	break;
    }

    return(rv);
}


/*
 * Returns 1 on success, 0 on error.
 */
int
format_msg_att(long int msgno, ATTACH_S **a, HANDLE_S **handlesp, gf_io_t pc, int flags)
{
    int rv = 1;

    if((*a)->body->type != TYPEMESSAGE)
      return(gf_puts("[ Undisplayed Attachment of Type ", pc)
	     && gf_puts(body_type_names((*a)->body->type), pc)
	     && gf_puts(" ]", pc) && gf_puts(NEWLINE, pc));

    if((*a)->body->subtype && strucmp((*a)->body->subtype, "rfc822") == 0){
	HEADER_S h;

	HD_INIT(&h, ps_global->VAR_VIEW_HEADERS, ps_global->view_all_except,
		FE_DEFAULT);
	switch(format_header(ps_global->mail_stream, msgno, (*a)->number,
			     (*a)->body->nested.msg->env, &h, NULL, NULL,
			     flags, NULL, pc)){
	  case -1 :			/* write error */
	    return(0);

	  case 1 :			/* fetch error */
	    if(!(gf_puts("[ Error fetching header ]",  pc)
		 && !gf_puts(NEWLINE, pc)))
	      return(0);

	    break;
	}

	gf_puts(NEWLINE, pc);

	++(*a);

#ifdef SMIME
	if((*a)->body && (*a)->body->subtype && (strucmp((*a)->body->subtype, OUR_PKCS7_ENCLOSURE_SUBTYPE)==0)){
	    if((*a)->description){
		if(!(!format_editorial((*a)->description,
				       ps_global->ttyo->screen_cols,
				       flags, NULL, pc)
		     && gf_puts(NEWLINE, pc) && gf_puts(NEWLINE, pc)))
		  rv = 0;
	    }

	    ++(*a);
	}
#endif /* SMIME */

	if(((*a))->description
	   && (*a)->body && (*a)->body->type == TYPETEXT){
	    if(decode_text(*a, msgno, pc, NULL, QStatus, flags))
	      rv = 0;
	}
	else if(!(gf_puts("[Can't display ", pc)
		  && gf_puts(((*a)->description && (*a)->body)
			       ? "first non-" : "missing ", pc)
		  && gf_puts("text segment]", pc)
		  && gf_puts(NEWLINE, pc)))
	  rv = 0;
    }
    else if((*a)->body->subtype 
	    && strucmp((*a)->body->subtype, "external-body") == 0) {
	if(format_editorial("This part is not included and can be fetched as follows:",
			    ps_global->ttyo->screen_cols, flags, NULL, pc)
	   || !gf_puts(NEWLINE, pc)
	   || format_editorial(display_parameters((*a)->body->parameter),
			       ps_global->ttyo->screen_cols, flags, handlesp, pc))
	  rv = 0;
    }
    else if(decode_text(*a, msgno, pc, NULL, QStatus, flags))
      rv = 0;

    return(rv);
}


void
display_vcard_att(long int msgno, ATTACH_S *a, int flags)
{
    STORE_S   *in_store, *out_store = NULL;
    HANDLE_S  *handles = NULL;
    URL_HILITE_S uh;
    gf_io_t    gc, pc;
    char     **lines, **ll, *errstr = NULL, tmp[MAILTMPLEN], *p;
    int	       cmd, indent, begins = 0;

    lines = detach_vcard_att(ps_global->mail_stream,
			     msgno, a->body, a->number);
    if(!lines){
	q_status_message(SM_ORDER | SM_DING, 3, 3,
			 _("Error accessing attachment.")); 
	return;
    }

    if(!(in_store = so_get(CharStar, NULL, EDIT_ACCESS))){
	free_list_array(&lines);
	q_status_message(SM_ORDER | SM_DING, 3, 3,
			 _("Error allocating space for attachment.")); 
	return;
    }

    for(ll = lines, indent = 0; ll && *ll; ll++)
      if((p = strindex(*ll, ':')) && p - *ll > indent)
	indent = p - *ll;

    indent += 5;
    for(ll = lines; ll && *ll; ll++){
	if((p = strindex(*ll, ':')) != NULL){
	    if(begins < 2 && struncmp(*ll, "begin:", 6) == 0)
	      begins++;

	    snprintf(tmp, sizeof(tmp), "  %-*.*s : ", indent - 5,
		    MIN(p - *ll, sizeof(tmp)-5), *ll);
	    tmp[sizeof(tmp)-1] = '\0';
	    so_puts(in_store, tmp);
	    p++;
	}
	else{
	    p = *ll;
	    so_puts(in_store, repeat_char(indent, SPACE));
	}

	snprintf(tmp, sizeof(tmp), "%.200s", p);
	tmp[sizeof(tmp)-1] = '\0';
	so_puts(in_store,
		(char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf,
					       SIZEOF_20KBUF, tmp));
	so_puts(in_store, "\015\012");
    }

    free_list_array(&lines);

    so_puts(in_store, "\015\012\015\012");

    do{
	if((out_store = so_get(CharStar, NULL, EDIT_ACCESS)) != NULL){
	    so_seek(in_store, 0L, 0);

	    init_handles(&handles);
	    gf_filter_init();

	    if(F_ON(F_VIEW_SEL_URL, ps_global)
	       || F_ON(F_VIEW_SEL_URL_HOST, ps_global)
	       || F_ON(F_SCAN_ADDR, ps_global))
	      gf_link_filter(gf_line_test,
			     gf_line_test_opt(url_hilite,
					      gf_url_hilite_opt(&uh,&handles,0)));

	    gf_link_filter(gf_wrap,
			   gf_wrap_filter_opt(ps_global->ttyo->screen_cols - 4,
					      ps_global->ttyo->screen_cols,
					      NULL, indent, GFW_HANDLES));
	    gf_link_filter(gf_nvtnl_local, NULL);

	    gf_set_so_readc(&gc, in_store);
	    gf_set_so_writec(&pc, out_store);

	    errstr = gf_pipe(gc, pc);

	    gf_clear_so_readc(in_store);

	    if(!errstr){
#define	VCARD_TEXT_ONE	_("This is a vCard which has been forwarded to you. You may add parts of it to your address book with the Save command. You will have a chance to edit it before committing it to your address book.")
#define	VCARD_TEXT_MORE	_("This is a vCard which has been forwarded to you. You may add the entries to your address book with the Save command.")
		errstr = format_editorial((begins > 1)
					    ? VCARD_TEXT_MORE : VCARD_TEXT_ONE,
					  ps_global->ttyo->screen_cols, 0, NULL, pc);
	    }

	    gf_clear_so_writec(out_store);

	    if(!errstr)
	      cmd = scroll_attachment(_("ADDRESS BOOK ATTACHMENT"), out_store,
				      CharStar, handles, a, flags | DA_RESIZE);

	    free_handles(&handles);
	    so_give(&out_store);
	}
	else
	  errstr = _("Error allocating space");
    }
    while(!errstr && (cmd == MC_RESIZE || cmd == MC_FULLHDR));

    if(errstr)
      q_status_message1(SM_ORDER | SM_DING, 3, 3,
			_("Can't format entry : %s"), errstr);

    so_give(&in_store);
}


/*----------------------------------------------------------------------
  Display attachment information

  Args: msgno -- message number to get partrom
	 a -- attachment struct for the desired part

  Result: a screen containing information about attachment:
 ----*/
void
display_attach_info(long int msgno, ATTACH_S *a)
{
    int		i, indent, cols;
    char        buf1[100], *folded;
    STORE_S    *store;
    PARMLIST_S *plist;
    SCROLL_S	sargs;

    (void) dispatch_attachment(a);

    if(!(store = so_get(CharStar, NULL, EDIT_ACCESS))){
	q_status_message(SM_ORDER | SM_DING, 3, 3,
			 _("Error allocating space for message."));
	return;
    }

    cols = ps_global->ttyo->screen_cols;

    /*
     * 2 spaces on left
     * 16 for text (longest is Display Method == 14)
     * 2 for ": "
     */
    indent = 20;

    /* don't try stupid folding */
    cols = MAX(indent+10, cols);

    so_puts(store, "Details about Attachment #");
    so_puts(store, a->number);
    so_puts(store, " :\n\n");
    utf8_snprintf(buf1, sizeof(buf1), "  %-*.*w: ", indent-4, indent-4, "Type");
    so_puts(store, buf1);
    so_puts(store, body_type_names(a->body->type));
    so_puts(store, "\n");
    utf8_snprintf(buf1, sizeof(buf1), "  %-*.*w: ", indent-4, indent-4, "Subtype");
    so_puts(store, buf1);
    so_puts(store, a->body->subtype ? a->body->subtype : "Unknown");
    so_puts(store, "\n");
    utf8_snprintf(buf1, sizeof(buf1), "  %-*.*w: ", indent-4, indent-4, "Encoding");
    so_puts(store, buf1);
    so_puts(store, a->body->encoding < ENCMAX
			 ? body_encodings[a->body->encoding]
			 : "Unknown");
    so_puts(store, "\n");
    if((plist = rfc2231_newparmlist(a->body->parameter)) != NULL){
	utf8_snprintf(buf1, sizeof(buf1), "  %-*.*w: ", indent-4, indent-4, "Parameters");
	so_puts(store, buf1);
	i = 0;
	while(rfc2231_list_params(plist)){
	    if(i++)
	      so_puts(store, repeat_char(indent, ' '));

	    so_puts(store, plist->attrib);
	    so_puts(store, " = ");
	    so_puts(store, plist->value ? plist->value : "");
	    so_puts(store, "\n");
	}

	rfc2231_free_parmlist(&plist);
    }

    if(a->body->description && a->body->description[0]){
	char buftmp[MAILTMPLEN];
	unsigned char *q;

	utf8_snprintf(buf1, sizeof(buf1), "  %-*.*w: ", indent-4, indent-4, "Description");

	snprintf(buftmp, sizeof(buftmp), "%s", a->body->description);
	buftmp[sizeof(buftmp)-1] = '\0';
	q = rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf, SIZEOF_20KBUF, buftmp);
	folded = fold((char *) q, cols, cols, buf1, repeat_char(indent+1, ' '), FLD_NONE);

	if(folded){
	  so_puts(store, folded);
	  fs_give((void **) &folded);
	}
    }

    /* BUG: no a->body->language support */

    if(a->body->disposition.type){
	utf8_snprintf(buf1, sizeof(buf1), "  %-*.*w: ", indent-4, indent-4, "Disposition");
	so_puts(store, buf1);
	so_puts(store, a->body->disposition.type);
	so_puts(store, "\n");
	if((plist = rfc2231_newparmlist(a->body->disposition.parameter)) != NULL){
	    while(rfc2231_list_params(plist)){
	        so_puts(store, repeat_char(indent, ' '));
		so_puts(store, plist->attrib);
		so_puts(store, " = ");
		so_puts(store, plist->value ? plist->value : "");
		so_puts(store, "\n");
	    }

	    rfc2231_free_parmlist(&plist);
	}
    }

    utf8_snprintf(buf1, sizeof(buf1), "  %-*.*w: ", indent-4, indent-4, "Approx. Size");
    so_puts(store, buf1);
    so_puts(store, comatose((a->body->encoding == ENCBASE64)
			      ? ((a->body->size.bytes * 3)/4)
			      : a->body->size.bytes));
    so_puts(store, " bytes\n");
    utf8_snprintf(buf1, sizeof(buf1), "  %-*.*w: ", indent-4, indent-4, "Display Method");
    so_puts(store, buf1);
    if(a->can_display == MCD_NONE) {
	so_puts(store, "Can't, ");
	so_puts(store, (a->body->encoding < ENCOTHER)
			 ? "Unknown Attachment Format"
			 : "Unknown Encoding");
    }
    else if(!(a->can_display & MCD_EXTERNAL)){
	so_puts(store, "Alpine's Internal Pager");
    }
    else{
	int   nt, free_pretty_cmd;
	MCAP_CMD_S *mc_cmd;
	char *pretty_cmd;

	if((mc_cmd = mailcap_build_command(a->body->type, a->body->subtype,
				       a->body, "<datafile>", &nt,
				       a->can_display & MCD_EXT_PROMPT)) != NULL){
	    so_puts(store, "\"");
	    if((pretty_cmd = execview_pretty_command(mc_cmd, &free_pretty_cmd)) != NULL)
	      so_puts(store, pretty_cmd);
	    so_puts(store, "\"");
	    if(free_pretty_cmd && pretty_cmd)
	      fs_give((void **)&pretty_cmd);
	    if(mc_cmd->command)
	      fs_give((void **)&mc_cmd->command);
	    fs_give((void **)&mc_cmd);
	}
    }

    so_puts(store, "\n");

    memset(&sargs, 0, sizeof(SCROLL_S));
    sargs.text.text  = so_text(store);
    sargs.text.src   = CharStar;
    sargs.text.desc  = "attachment info";
    sargs.bar.title  = _("ABOUT ATTACHMENT");
    sargs.help.text  = h_simple_text_view;
    sargs.help.title = _("HELP FOR \"ABOUT ATTACHMENT\"");

    sargs.use_indexline_color = 1;

    scrolltool(&sargs);

    so_give(&store);	/* free resources associated with store */
    ps_global->mangled_screen = 1;
}


/*----------------------------------------------------------------------

 ----*/
void
forward_attachment(MAILSTREAM *stream, long int msgno, ATTACH_S *a)
{
    char     *sig;
    void     *msgtext;
    ENVELOPE *outgoing;
    BODY     *body;

    if(MIME_MSG_A(a)){
	forward_msg_att(stream, msgno, a);
    }
    else{
	ACTION_S      *role = NULL;
	REDRAFT_POS_S *redraft_pos = NULL;
	long           rflags = ROLE_FORWARD;
	PAT_STATE      dummy;

	outgoing              = mail_newenvelope();
	outgoing->message_id  = generate_message_id();
	outgoing->subject     = cpystr("Forwarded attachment...");

	if(nonempty_patterns(rflags, &dummy)){
	    /*
	     * There is no message, but a Current Folder Type might match.
	     *
	     * This has been changed to check against the message 
	     * containing the attachment.
	     */
	    role = set_role_from_msg(ps_global, ROLE_FORWARD, msgno, NULL);
	    if(confirm_role(rflags, &role))
	      role = combine_inherited_role(role);
	    else{
		role = NULL;
		cmd_cancelled("Forward");
		mail_free_envelope(&outgoing);
		return;
	    }
	}

	if(role)
	  q_status_message1(SM_ORDER, 3, 4,
			    _("Forwarding using role \"%s\""), role->nick);

	/*
	 * as with all text bound for the composer, build it in 
	 * a storage object of the type it understands...
	 */
	if((msgtext = (void *) so_get(PicoText, NULL, EDIT_ACCESS)) != NULL){
	    int   impl, template_len = 0;

	    if(role && role->template){
		char *filtered;

		impl = 1;
		filtered = detoken(role, NULL, 0, 0, 0, &redraft_pos, &impl);
		if(filtered){
		    if(*filtered){
			so_puts((STORE_S *)msgtext, filtered);
			if(impl == 1)
			  template_len = strlen(filtered);
		    }
		    
		    fs_give((void **)&filtered);
		}
	    }
	    else
	      impl = 1;

	    if((sig = detoken(role, NULL, 2, 0, 1, &redraft_pos, &impl)) != NULL){
		if(impl == 2)
		  redraft_pos->offset += template_len;

		so_puts((STORE_S *)msgtext, *sig ? sig : NEWLINE);

		fs_give((void **)&sig);
	    }
	    else
	      so_puts((STORE_S *)msgtext, NEWLINE);

	    /*---- New Body to start with ----*/
	    body       = mail_newbody();
	    body->type = TYPEMULTIPART;

	    /*---- The TEXT part/body ----*/
	    body->nested.part			       = mail_newbody_part();
	    body->nested.part->body.type	       = TYPETEXT;
	    body->nested.part->body.contents.text.data = msgtext;

	    /*---- The corresponding things we're attaching ----*/
	    body->nested.part->next  = mail_newbody_part();
	    body->nested.part->next->body.id = generate_message_id();
	    copy_body(&body->nested.part->next->body, a->body);

	    if(fetch_contents(stream, msgno, a->number,
			      &body->nested.part->next->body)){
		pine_send(outgoing, &body, "FORWARD MESSAGE",
			  role, NULL, NULL, redraft_pos, NULL, NULL, 0);

		ps_global->mangled_screen = 1;
		pine_free_body(&body);
		free_redraft_pos(&redraft_pos);
	    }
	    else{
		mail_free_body(&body);
		so_give((STORE_S **) &msgtext);
		free_redraft_pos(&redraft_pos);
		q_status_message(SM_ORDER | SM_DING, 4, 5,
		   _("Error fetching message contents.  Can't forward message."));
	    }
	}
	else
	  q_status_message(SM_ORDER | SM_DING, 3, 4,
			   _("Error allocating message text"));

	mail_free_envelope(&outgoing);
	free_action(&role);
    }
}


void
forward_msg_att(MAILSTREAM *stream, long int msgno, ATTACH_S *a)
{
    char          *p, *sig = NULL;
    int            ret;
    void          *msgtext;
    ENVELOPE      *outgoing;
    BODY          *body;
    ACTION_S      *role = NULL;
    REPLY_S        reply;
    REDRAFT_POS_S *redraft_pos = NULL;

    outgoing             = mail_newenvelope();
    outgoing->message_id = generate_message_id();

    memset((void *)&reply, 0, sizeof(reply));

    if((outgoing->subject = forward_subject(a->body->nested.msg->env, 0)) != NULL){
	/*
	 * as with all text bound for the composer, build it in 
	 * a storage object of the type it understands...
	 */
	if((msgtext = (void *) so_get(PicoText, NULL, EDIT_ACCESS)) != NULL){
	    int impl, template_len = 0;
	    long rflags = ROLE_FORWARD;
	    PAT_STATE dummy;

	    ret = 'n';
	    if(ps_global->full_header == 2)
	       ret = want_to(_("Forward message as an attachment"), 'n', 0,
			     NO_HELP, WT_SEQ_SENSITIVE);
	    /* Setup possible role */
	    if(nonempty_patterns(rflags, &dummy)){
		role = set_role_from_msg(ps_global, rflags, msgno, a->number);
		if(confirm_role(rflags, &role))
		  role = combine_inherited_role(role);
		else{				/* cancel reply */
		    role = NULL;
		    cmd_cancelled("Forward");
		    mail_free_envelope(&outgoing);
		    so_give((STORE_S **) &msgtext);
		    return;
		}
	    }

	    if(role)
	      q_status_message1(SM_ORDER, 3, 4,
				_("Forwarding using role \"%s\""), role->nick);

	    if(role && role->template){
		char *filtered;

		impl = 1;
		filtered = detoken(role, a->body->nested.msg->env,
				   0, 0, 0, &redraft_pos, &impl);
		if(filtered){
		    if(*filtered){
			so_puts((STORE_S *)msgtext, filtered);
			if(impl == 1)
			  template_len = strlen(filtered);
		    }
		    
		    fs_give((void **)&filtered);
		}
	    }
	    else
	      impl = 1;

	    if((sig = detoken(role, a->body->nested.msg->env,
			     2, 0, 1, &redraft_pos, &impl)) != NULL){
		if(impl == 2)
		  redraft_pos->offset += template_len;

		so_puts((STORE_S *)msgtext, *sig ? sig : NEWLINE);

		fs_give((void **)&sig);
	    }
	    else
	      so_puts((STORE_S *)msgtext, NEWLINE);

	    if(ret == 'y'){
		/*---- New Body to start with ----*/
		body	   = mail_newbody();
		body->type = TYPEMULTIPART;

		/*---- The TEXT part/body ----*/
		body->nested.part = mail_newbody_part();
		body->nested.part->body.type = TYPETEXT;
		body->nested.part->body.contents.text.data = msgtext;

		if(!forward_mime_msg(stream, msgno,
				     p = body_partno(stream, msgno, a->body),
				     a->body->nested.msg->env,
				     &body->nested.part->next, msgtext))
		  mail_free_body(&body);
	    }
	    else{
		reply.forw = 1;
		if(a->body->nested.msg->body){
		    char *charset;

		    charset
		      = parameter_val(a->body->nested.msg->body->parameter,
				          "charset");
		    
		    if(charset && strucmp(charset, "us-ascii") != 0){
			CONV_TABLE *ct;

			/*
			 * There is a non-ascii charset,
			 * is there conversion happening?
			 */
			if(!(ct=conversion_table(charset, ps_global->posting_charmap))
			   || !ct->table){
			    reply.orig_charset = charset;
			    charset = NULL;
			}
		    }

		    if(charset)
		      fs_give((void **) &charset);
		}

		body = forward_body(stream, a->body->nested.msg->env,
				    a->body->nested.msg->body, msgno,
				    p = body_partno(stream, msgno, a->body),
				    msgtext, FWD_NONE);
	    }

	    fs_give((void **) &p);

	    if(body){
		pine_send(outgoing, &body,
			  "FORWARD MESSAGE",
			  role, NULL,
			  reply.forw ? &reply : NULL,
			  redraft_pos, NULL, NULL, 0);

		ps_global->mangled_screen = 1;
		pine_free_body(&body);
		free_redraft_pos(&redraft_pos);
		free_action(&role);
	    }
	    else{
		so_give((STORE_S **) &msgtext);
		q_status_message(SM_ORDER | SM_DING, 4, 5,
		   _("Error fetching message contents.  Can't forward message."));
	    }
	}
	else
	  q_status_message(SM_ORDER | SM_DING, 3, 4,
			   _("Error allocating message text"));
    }
    else
      q_status_message1(SM_ORDER,3,4,
			_("Error fetching message %s. Can't forward it."),
			long2string(msgno));

    if(reply.orig_charset)
      fs_give((void **)&reply.orig_charset);

    mail_free_envelope(&outgoing);
}


void
reply_msg_att(MAILSTREAM *stream, long int msgno, ATTACH_S *a)
{
    ADDRESS       *saved_from, *saved_to, *saved_cc, *saved_resent;
    ENVELOPE      *outgoing;
    BODY          *body;
    void          *msgtext;
    char          *tp, *prefix = NULL, *fcc = NULL, *errmsg = NULL;
    int            include_text = 0, flags = RSF_QUERY_REPLY_ALL;
    int            rolemsg = 0, copytomsg = 0;
    long           rflags;
    PAT_STATE      dummy;
    REDRAFT_POS_S *redraft_pos = NULL;
    ACTION_S      *role = NULL;

    outgoing = mail_newenvelope();

    dprint((4,"\n - attachment reply \n"));

    saved_from		  = (ADDRESS *) NULL;
    saved_to		  = (ADDRESS *) NULL;
    saved_cc		  = (ADDRESS *) NULL;
    saved_resent	  = (ADDRESS *) NULL;
    outgoing->subject	  = NULL;

    prefix = reply_quote_str(a->body->nested.msg->env);
    /*
     * For consistency, the first question is always "include text?"
     */
    if((include_text = reply_text_query(ps_global, 1, &prefix)) >= 0
       && reply_news_test(a->body->nested.msg->env, outgoing) > 0
       && reply_harvest(ps_global, msgno, a->number,
			a->body->nested.msg->env, &saved_from,
			&saved_to, &saved_cc, &saved_resent, &flags)){
	outgoing->subject = reply_subject(a->body->nested.msg->env->subject,
					  NULL, 0);
	clear_cursor_pos();
	reply_seed(ps_global, outgoing, a->body->nested.msg->env,
		   saved_from, saved_to, saved_cc, saved_resent,
		   &fcc, flags & RSF_FORCE_REPLY_ALL, &errmsg);
	if(errmsg){
	    if(*errmsg){
		q_status_message1(SM_ORDER, 3, 3, "%.200s", errmsg);
		display_message(NO_OP_COMMAND);
	    }

	    fs_give((void **)&errmsg);
	}

	if(sp_expunge_count(stream))	/* current msg was expunged */
	  goto seeyalater;

	/* Setup possible role */
	rflags = ROLE_REPLY;
	if(nonempty_patterns(rflags, &dummy)){
	    role = set_role_from_msg(ps_global, rflags, msgno, a->number);
	    if(confirm_role(rflags, &role))
	      role = combine_inherited_role(role);
	    else{				/* cancel reply */
		role = NULL;
		cmd_cancelled("Reply");
		goto seeyalater;
	    }
	}

	if(role){
	    rolemsg++;

	    /* override fcc gotten in reply_seed */
	    if(role->fcc && fcc)
	      fs_give((void **) &fcc);
	}

	if(F_ON(F_COPY_TO_TO_FROM, ps_global) && !(role && role->from)){
	    ADDRESS *us_in_to_and_cc, *ap;

	    us_in_to_and_cc = (ADDRESS *) NULL;
	    if(a->body->nested.msg->env && a->body->nested.msg->env->to)
	      if((ap=reply_cp_addr(ps_global, 0L, NULL,
				   NULL, us_in_to_and_cc, NULL,
				   a->body->nested.msg->env->to, RCA_ONLY_US)) != NULL)
		reply_append_addr(&us_in_to_and_cc, ap);

	    if(a->body->nested.msg->env && a->body->nested.msg->env->cc)
	      if((ap=reply_cp_addr(ps_global, 0L, NULL,
				   NULL, us_in_to_and_cc, NULL,
				   a->body->nested.msg->env->cc, RCA_ONLY_US)) != NULL)
		reply_append_addr(&us_in_to_and_cc, ap);

	    /*
	     * A list of all of our addresses that appear in the To
	     * and cc fields is in us_in_to_and_cc.
	     * If there is exactly one address in that list then
	     * use it for the outgoing From.
	     */
	    if(us_in_to_and_cc && !us_in_to_and_cc->next){
		PINEFIELD *custom, *pf;
		ADDRESS *a = NULL;
		char *addr = NULL;

		/*
		 * Check to see if this address is different from what
		 * we would have used anyway. If it is, notify the user
		 * with a status message. This is pretty hokey because we're
		 * mimicking how pine_send would set the From address and
		 * there is no coordination between the two.
		 */

		/* in case user has a custom From value */
		custom = parse_custom_hdrs(ps_global->VAR_CUSTOM_HDRS, UseAsDef);

		pf = (PINEFIELD *) fs_get(sizeof(*pf));
		memset((void *) pf, 0, sizeof(*pf));
		pf->name = cpystr("From");
		pf->addr = &a;
		if(set_default_hdrval(pf, custom) >= UseAsDef
		   && pf->textbuf && pf->textbuf[0]){
		    removing_trailing_white_space(pf->textbuf);
		    (void)removing_double_quotes(pf->textbuf);
		    build_address(pf->textbuf, &addr, NULL, NULL, NULL);
		    rfc822_parse_adrlist(pf->addr, addr, ps_global->maildomain);
		    if(addr)
		      fs_give((void **) &addr);
		}

		if(!*pf->addr)
		  *pf->addr = generate_from();

		if(*pf->addr && !address_is_same(*pf->addr, us_in_to_and_cc)){
		    copytomsg++;
		    if(!role){
			role = (ACTION_S *) fs_get(sizeof(*role));
			memset((void *) role, 0, sizeof(*role));
			role->is_a_role = 1;
		    }

		    role->from = us_in_to_and_cc;
		    us_in_to_and_cc = NULL;
		}

		free_customs(custom);
		free_customs(pf);
	    }

	    if(us_in_to_and_cc)
	      mail_free_address(&us_in_to_and_cc);

	}

	if(role){
	    if(rolemsg && copytomsg)
	      q_status_message1(SM_ORDER, 3, 4,
				_("Replying using role \"%s\" and To as From"), role->nick);
	    else if(rolemsg)
	      q_status_message1(SM_ORDER, 3, 4,
				_("Replying using role \"%s\""), role->nick);
	    else if(copytomsg)
	      q_status_message(SM_ORDER, 3, 4,
			       _("Replying using incoming To as outgoing From"));
	}

	outgoing->in_reply_to = reply_in_reply_to(a->body->nested.msg->env);
	outgoing->references = reply_build_refs(a->body->nested.msg->env);
	outgoing->message_id = generate_message_id();

	if(!outgoing->to && !outgoing->cc
	   && !outgoing->bcc && !outgoing->newsgroups)
	  q_status_message(SM_ORDER | SM_DING, 3, 6,
			   _("Warning: no valid addresses to reply to!"));

	/*
	 * Now fix up the body...
	 */
	if((msgtext = (void *) so_get(PicoText, NULL, EDIT_ACCESS)) != NULL){
	    REPLY_S reply;

	    memset((void *)&reply, 0, sizeof(reply));
	    reply.forw = 1;
	    if(a->body->nested.msg->body){
		char *charset;

		charset
		  = parameter_val(a->body->nested.msg->body->parameter,
				      "charset");
		
		if(charset && strucmp(charset, "us-ascii") != 0){
		    CONV_TABLE *ct;

		    /*
		     * There is a non-ascii charset,
		     * is there conversion happening?
		     */
		    if(!(ct=conversion_table(charset, ps_global->posting_charmap))
		       || !ct->table){
			reply.orig_charset = charset;
			charset = NULL;
		    }
		}

		if(charset)
		  fs_give((void **) &charset);
	    }

	    if((body = reply_body(stream, a->body->nested.msg->env,
				 a->body->nested.msg->body, msgno,
				 tp = body_partno(stream, msgno, a->body),
				 msgtext, prefix, include_text, role,
				 1, &redraft_pos)) != NULL){
		/* partially formatted outgoing message */
		pine_send(outgoing, &body, "COMPOSE MESSAGE REPLY",
			  role, fcc, &reply, redraft_pos, NULL, NULL, 0);

		pine_free_body(&body);
		ps_global->mangled_screen = 1;
	    }
	    else
	      q_status_message(SM_ORDER | SM_DING, 3, 4,
			       _("Error building message body"));

	    fs_give((void **) &tp);
	    if(reply.orig_charset)
	      fs_give((void **)&reply.orig_charset);
	}
	else
	  q_status_message(SM_ORDER | SM_DING, 3, 4,
			   _("Error allocating message text"));
    }

seeyalater:
    mail_free_envelope(&outgoing);
    mail_free_address(&saved_from);
    mail_free_address(&saved_to);
    mail_free_address(&saved_cc);
    mail_free_address(&saved_resent);

    if(prefix)
      fs_give((void **) &prefix);

    if(fcc)
      fs_give((void **) &fcc);
    
    free_redraft_pos(&redraft_pos);
    free_action(&role);
}


void
bounce_msg_att(MAILSTREAM *stream, long int msgno, char *part, char *subject)
{
    char *errstr;

    if((errstr = bounce_msg(stream, msgno, part, NULL, NULL, subject, NULL, NULL)) != NULL)
      q_status_message(SM_ORDER | SM_DING, 3, 3, errstr);
}


void
pipe_attachment(long int msgno, ATTACH_S *a)
{
    char    *err, *resultfilename = NULL, prompt[80], *p;
    int      rc, capture = 1, raw = 0, we_cancel = 0, j = 0;
    long     ku;
    PIPE_S  *syspipe;
    HelpType help;
    char     pipe_command[MAXPATH+1];
    unsigned          flagsforhist = 1;	/* raw=2 /capture=1 */
    static HISTORY_S *history = NULL;
    ESCKEY_S pipe_opt[6];
    
    if(ps_global->restricted){
	q_status_message(SM_ORDER | SM_DING, 0, 4,
			 "Alpine demo can't pipe attachments");
	return;
    }

    help = NO_HELP;
    pipe_command[0] = '\0';

    init_hist(&history, HISTSIZE);
    flagsforhist = (raw ? 0x2 : 0) + (capture ? 0x1 : 0);
    if((p = get_prev_hist(history, "", flagsforhist, NULL)) != NULL){
	strncpy(pipe_command, p, sizeof(pipe_command));
	pipe_command[sizeof(pipe_command)-1] = '\0';
	if(history->hist[history->curindex]){
	    flagsforhist = history->hist[history->curindex]->flags;
	    raw     = (flagsforhist & 0x2) ? 1 : 0;
	    capture = (flagsforhist & 0x1) ? 1 : 0;
	}
    }

    pipe_opt[j].ch    = 0;
    pipe_opt[j].rval  = 0;
    pipe_opt[j].name  = "";
    pipe_opt[j++].label = "";

    pipe_opt[j].ch    = ctrl('W');
    pipe_opt[j].rval  = 10;
    pipe_opt[j].name  = "^W";
    pipe_opt[j++].label = NULL;

    pipe_opt[j].ch    = ctrl('Y');
    pipe_opt[j].rval  = 11;
    pipe_opt[j].name  = "^Y";
    pipe_opt[j++].label = NULL;

    pipe_opt[j].ch      = KEY_UP;
    pipe_opt[j].rval    = 30;
    pipe_opt[j].name    = "";
    ku = j;
    pipe_opt[j++].label = "";

    pipe_opt[j].ch      = KEY_DOWN;
    pipe_opt[j].rval    = 31;
    pipe_opt[j].name    = "";
    pipe_opt[j++].label = "";
    
    pipe_opt[j].ch = -1;

    while(1){
	int flags;

	snprintf(prompt, sizeof(prompt), "Pipe %sattachment %s to %s: ", raw ? "RAW " : "",
		a->number, capture ? "" : "(Free Output) ");
	prompt[sizeof(prompt)-1] = '\0';
	pipe_opt[1].label = raw ? "DecodedData" : "Raw Data";
	pipe_opt[2].label = capture ? "Free Output" : "Capture Output";

	/*
	 * 2 is really 1 because there will be one real entry and
	 * one entry of "" because of the get_prev_hist above.
	 */
	if(items_in_hist(history) > 2){
	    pipe_opt[ku].name  = HISTORY_UP_KEYNAME;
	    pipe_opt[ku].label = HISTORY_KEYLABEL;
	    pipe_opt[ku+1].name  = HISTORY_DOWN_KEYNAME;
	    pipe_opt[ku+1].label = HISTORY_KEYLABEL;
	}
	else{
	    pipe_opt[ku].name  = "";
	    pipe_opt[ku].label = "";
	    pipe_opt[ku+1].name  = "";
	    pipe_opt[ku+1].label = "";
	}

	flags = OE_APPEND_CURRENT | OE_SEQ_SENSITIVE;
	rc = optionally_enter(pipe_command, -FOOTER_ROWS(ps_global), 0,
			      sizeof(pipe_command), prompt,
			      pipe_opt, help, &flags);
	if(rc == -1){
	    q_status_message(SM_ORDER | SM_DING, 3, 4,
			     "Internal problem encountered");
	    break;
	}
	else if(rc == 10){
	    raw = !raw;			/* flip raw text */
	}
	else if(rc == 11){
	    capture = !capture;		/* flip capture output */
	}
	else if(rc == 30){
	    flagsforhist = (raw ? 0x2 : 0) + (capture ? 0x1 : 0);
	    if((p = get_prev_hist(history, pipe_command, flagsforhist, NULL)) != NULL){
		strncpy(pipe_command, p, sizeof(pipe_command));
		pipe_command[sizeof(pipe_command)-1] = '\0';
		if(history->hist[history->curindex]){
		    flagsforhist = history->hist[history->curindex]->flags;
		    raw     = (flagsforhist & 0x2) ? 1 : 0;
		    capture = (flagsforhist & 0x1) ? 1 : 0;
		}
	    }
	    else
	      Writechar(BELL, 0);
	}
	else if(rc == 31){
	    flagsforhist = (raw ? 0x2 : 0) + (capture ? 0x1 : 0);
	    if((p = get_next_hist(history, pipe_command, flagsforhist, NULL)) != NULL){
		strncpy(pipe_command, p, sizeof(pipe_command));
		pipe_command[sizeof(pipe_command)-1] = '\0';
		if(history->hist[history->curindex]){
		    flagsforhist = history->hist[history->curindex]->flags;
		    raw     = (flagsforhist & 0x2) ? 1 : 0;
		    capture = (flagsforhist & 0x1) ? 1 : 0;
		}
	    }
	    else
	      Writechar(BELL, 0);
	}
	else if(rc == 0){
	    if(pipe_command[0] == '\0'){
		cmd_cancelled("Pipe command");
		break;
	    }

	    flagsforhist = (raw ? 0x2 : 0) + (capture ? 0x1 : 0);
	    save_hist(history, pipe_command, flagsforhist, NULL);

	    flags = PIPE_USER | PIPE_WRITE | PIPE_STDERR;
	    flags |= (raw ? PIPE_RAW : 0);
	    if(!capture){
#ifndef	_WINDOWS
		ClearScreen();
		fflush(stdout);
		clear_cursor_pos();
		ps_global->mangled_screen = 1;
#endif
		flags |= PIPE_RESET;
	    }

	    if((syspipe = open_system_pipe(pipe_command,
				   (flags&PIPE_RESET) ? NULL : &resultfilename,
				   NULL, flags, 0, pipe_callback, pipe_report_error)) != NULL){
		int is_text = 0;
		gf_io_t  pc;		/* wire up a generic putchar */

		is_text = (a && a->body && a->body->type == TYPETEXT);

		gf_set_writec(&pc, syspipe, 0L, PipeStar,
			      (is_text && !raw) ? WRITE_TO_LOCALE : 0);

		/*------ Write the image to a temporary file ------*/
		if(raw){		/* pipe raw text */
		    FETCH_READC_S fetch_part;

		    err = NULL;

		    if(capture)
		      we_cancel = busy_cue(NULL, NULL, 1);
		    else
		      suspend_busy_cue();

		    gf_filter_init();
		    fetch_readc_init(&fetch_part, ps_global->mail_stream,
				     msgno, a->number, a->body->size.bytes, 0, 0);
		    gf_link_filter(gf_nvtnl_local, NULL);
		    err = gf_pipe(FETCH_READC, pc);

		    if(capture){
			if(we_cancel)
			  cancel_busy_cue(0);
		    }
		    else
		      resume_busy_cue(0);
		}
		else{
		    /* BUG: there's got to be a better way */
		    if(!capture)
		      ps_global->print = (PRINT_S *) 1;

		    suspend_busy_cue();
		    err = detach(ps_global->mail_stream, msgno,
				 a->number, 0L, (long *)NULL, pc, NULL, 0);
		    ps_global->print = (PRINT_S *) NULL;
		    resume_busy_cue(0);
		}

		(void) close_system_pipe(&syspipe, NULL, pipe_callback);

		if(err)
		  q_status_message1(SM_ORDER | SM_DING, 3, 4,
				    _("Error detaching for pipe: %s"), err);

		display_output_file(resultfilename,
				    (err)
				      ? _("PIPE ATTACHMENT (ERROR)")
				      : _("PIPE ATTACHMENT"),
				    NULL, DOF_EMPTY);

		fs_give((void **) &resultfilename);
	    }
	    else
	      q_status_message(SM_ORDER | SM_DING, 3, 4,
			       _("Error opening pipe"));

	    break;
	}
	else if(rc == 1){
	    cmd_cancelled("Pipe");
	    break;
	}
	else if(rc == 3)
	  help = (help == NO_HELP) ? h_pipe_attach : NO_HELP;
    }
}


int
delete_attachment(long int msgno, ATTACH_S *a)
{
    int expbits, rv = 0;

    if(!msgno_exceptions(ps_global->mail_stream, msgno,
			 a->number, &expbits, FALSE)
       || !(expbits & MSG_EX_DELETE)){
	expbits |= MSG_EX_DELETE;
	msgno_exceptions(ps_global->mail_stream, msgno,
			 a->number, &expbits, TRUE);

	q_status_message1(SM_ORDER, 0, 3,
			_("Part %s will be omitted only if message is Saved"),
			  a->number);
	rv = 1;
    }
    else
      q_status_message1(SM_ORDER, 0, 3, _("Part %s already deleted"),
			a->number);

    return(rv);
}


int
undelete_attachment(long int msgno, ATTACH_S *a, int *expbitsp)
{
    int rv = 0;

    if(msgno_exceptions(ps_global->mail_stream, msgno,
			a->number, expbitsp, FALSE)
       && ((*expbitsp) & MSG_EX_DELETE)){
	(*expbitsp) ^= MSG_EX_DELETE;
	msgno_exceptions(ps_global->mail_stream, msgno,
			 a->number, expbitsp, TRUE);
	rv = 1;
    }
    else
      q_status_message1(SM_ORDER, 0, 3, _("Part %s already UNdeleted"),
			a->number);

    return(rv);
}


/*----------------------------------------------------------------------
  Resolve any deferred tests for attachment displayability

  Args: attachment structure

  Returns: undefer's attachment's displayability test
  ----*/
int
dispatch_attachment(ATTACH_S *a)
{
    if(a->test_deferred){
	a->test_deferred = 0;
	a->can_display = mime_can_display(a->body->type, a->body->subtype, a->body);
    }

    return(a->can_display);
}


#ifdef	_WINDOWS
int
scroll_att_popup(sparms, in_handle)
    SCROLL_S *sparms;
    int	      in_handle;
{
    MPopup scrat_popup[20];
    int	   i = -1, n;

    if(in_handle){
	scrat_popup[++i].type	    = tIndex;
	scrat_popup[i].label.style  = lNormal;
	scrat_popup[i].label.string = "View Selectable Item";
	scrat_popup[i].data.val	    = ctrl('L');
    }

    scrat_popup[++i].type	= tQueue;
    scrat_popup[i].label.style	= lNormal;
    scrat_popup[i].label.string = "&Save";
    scrat_popup[i].data.val	= 'S';

    scrat_popup[++i].type	= tQueue;
    scrat_popup[i].label.style	= lNormal;
    if(msgno_exceptions(ps_global->mail_stream,
			mn_m2raw(ps_global->msgmap,
				 mn_get_cur(ps_global->msgmap)),
			scrat_attachp->number, &n, FALSE)
       && (n & MSG_EX_DELETE)){
	scrat_popup[i].label.string = "&Undelete";
	scrat_popup[i].data.val	    = 'U';
    }
    else{
	scrat_popup[i].label.string = "&Delete";
	scrat_popup[i].data.val	    = 'D';
    }

    if(MIME_MSG_A(scrat_attachp) || MIME_DGST_A(scrat_attachp)){
	scrat_popup[++i].type	    = tQueue;
	scrat_popup[i].label.style  = lNormal;
	scrat_popup[i].label.string = "&Reply";
	scrat_popup[i].data.val	    = 'R';

	scrat_popup[++i].type	    = tQueue;
	scrat_popup[i].label.style  = lNormal;
	scrat_popup[i].label.string = "&Forward";
	scrat_popup[i].data.val	    = 'f';
    }

    scrat_popup[++i].type = tSeparator;

    scrat_popup[++i].type	    = tQueue;
    scrat_popup[i].label.style  = lNormal;
    scrat_popup[i].label.string = "Attachment Index";
    scrat_popup[i].data.val	    = '<';

    scrat_popup[++i].type = tTail;

    return(mswin_popup(scrat_popup) == 0 && in_handle);
}


void
display_att_window(a)
     ATTACH_S *a;
{
#if !defined(DOS) && !defined(OS2)
    char     prefix[8];
#endif

    if(a->body->type == TYPEMULTIPART){
	if(a->body->subtype){
/*	    if(!strucmp(a->body->subtype, "digest"))
	      display_digest_att(msgno, a, flags);
	    else */
	      q_status_message1(SM_ORDER, 3, 5,
				"Can't display Multipart/%s",
				a->body->subtype);
	}
	else
	  q_status_message(SM_ORDER, 3, 5,
			   "Can't display unknown Multipart Subtype");
    }
/*    else if(MIME_VCARD_A(a))
      display_vcard_att_window(msgno, a, flags);*/
    else if(a->body->type == TYPETEXT)
      display_text_att_window(a);
    else if(a->body->type == TYPEMESSAGE)
      display_msg_att_window(a);
}


void
display_text_att_window(a)
    ATTACH_S *a;
{
    STORE_S  *store;
    long      msgno;

    msgno = mn_m2raw(ps_global->msgmap, mn_get_cur(ps_global->msgmap));

    if(store = format_text_att(msgno, a, NULL)){
	if (mswin_displaytext("ATTACHED TEXT",
			      so_text(store),
			      strlen((char *) so_text(store)),
			      NULL, NULL, 0) >= 0)
	  store->txt = (void *) NULL;	/* free'd in mswin_displaytext */

	so_give(&store);	/* free resources associated with store */
    }
    else
      q_status_message(SM_ORDER | SM_DING, 3, 3,
		       "Error allocating space for attachment.");
}


void
display_msg_att_window(a)
    ATTACH_S *a;
{
    STORE_S  *store;
    gf_io_t   pc;
    ATTACH_S *ap = a;
    long      msgno;

    msgno = mn_m2raw(ps_global->msgmap, mn_get_cur(ps_global->msgmap));

    /* BUG, should check this return code */
    (void) pine_mail_fetchstructure(ps_global->mail_stream, msgno, NULL);

    /* initialize a storage object */
    if(store = so_get(CharStar, NULL, EDIT_ACCESS)){

	gf_set_so_writec(&pc, store);

	if(format_msg_att(msgno, &ap, NULL, pc, FM_DISPLAY)
	   && mswin_displaytext("ATTACHED MESSAGE", so_text(store),
				strlen((char *) so_text(store)),
				NULL, NULL, 0) >= 0)
	  /* free'd in mswin_displaytext */
	  store->txt = (void *) NULL;

	gf_clear_so_writec(store);

	so_give(&store);
    }
    else
      q_status_message(SM_ORDER | SM_DING, 3, 3,
		       "Error allocating space for message.");
}
#endif
