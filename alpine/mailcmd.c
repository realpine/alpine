#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: mailcmd.c 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $";
#endif

/*
 * ========================================================================
 * Copyright 2006-2009 University of Washington
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
     mailcmd.c
     The meat and pototoes of mail processing here:
       - initial command processing and dispatch
       - save message
       - capture address off incoming mail
       - jump to specific numbered message
       - open (broach) a new folder
       - search message headers (where is) command
  ====*/


#include "headers.h"
#include "mailcmd.h"
#include "status.h"
#include "mailview.h"
#include "flagmaint.h"
#include "listsel.h"
#include "keymenu.h"
#include "alpine.h"
#include "mailpart.h"
#include "mailindx.h"
#include "folder.h"
#include "reply.h"
#include "help.h"
#include "titlebar.h"
#include "signal.h"
#include "radio.h"
#include "pipe.h"
#include "send.h"
#include "takeaddr.h"
#include "roleconf.h"
#include "smime.h"
#include "../pith/state.h"
#include "../pith/msgno.h"
#include "../pith/store.h"
#include "../pith/thread.h"
#include "../pith/flag.h"
#include "../pith/sort.h"
#include "../pith/maillist.h"
#include "../pith/save.h"
#include "../pith/pipe.h"
#include "../pith/news.h"
#include "../pith/util.h"
#include "../pith/sequence.h"
#include "../pith/keyword.h"
#include "../pith/stream.h"
#include "../pith/mailcmd.h"
#include "../pith/hist.h"
#include "../pith/list.h"
#include "../pith/icache.h"
#include "../pith/busy.h"
#include "../pith/mimedesc.h"
#include "../pith/pattern.h"
#include "../pith/tempfile.h"
#include "../pith/search.h"
#include "../pith/margin.h"
#ifdef _WINDOWS
#include "../pico/osdep/mswin.h"
#endif

/*
 * Internal Prototypes
 */
int       cmd_flag(struct pine *, MSGNO_S *, int);
int	  cmd_flag_prompt(struct pine *, struct flag_screen *, int);
void      free_flag_table(struct flag_table **);
int       cmd_reply(struct pine *, MSGNO_S *, int);
int       cmd_forward(struct pine *, MSGNO_S *, int);
int       cmd_bounce(struct pine *, MSGNO_S *, int);
int       cmd_save(struct pine *, MAILSTREAM *, MSGNO_S *, int, CmdWhere);
void      role_compose(struct pine *);
void	  cmd_expunge(struct pine *, MAILSTREAM *, MSGNO_S *);
int       cmd_export(struct pine *, MSGNO_S *, int, int);
char	 *cmd_delete_action(struct pine *, MSGNO_S *, CmdWhere);
char	 *cmd_delete_view(struct pine *, MSGNO_S *);
char	 *cmd_delete_index(struct pine *, MSGNO_S *);
long      get_level(int, UCS, SCROLL_S *);
long      closest_jump_target(long, MAILSTREAM *, MSGNO_S *, int, CmdWhere, char *, size_t);
int	  update_folder_spec(char *, size_t, char *);
int       cmd_print(struct pine *, MSGNO_S *, int, CmdWhere);
int       cmd_pipe(struct pine *, MSGNO_S *, int);
STORE_S	 *list_mgmt_text(RFC2369_S *, long);
void	  list_mgmt_screen(STORE_S *);
int	  aggregate_select(struct pine *, MSGNO_S *, int, CmdWhere);
int	  select_by_number(MAILSTREAM *, MSGNO_S *, SEARCHSET **);
int	  select_by_thrd_number(MAILSTREAM *, MSGNO_S *, SEARCHSET **);
int	  select_by_date(MAILSTREAM *, MSGNO_S *, long, SEARCHSET **);
int	  select_by_text(MAILSTREAM *, MSGNO_S *, long, SEARCHSET **);
int	  select_by_size(MAILSTREAM *, SEARCHSET **);
SEARCHSET *visible_searchset(MAILSTREAM *, MSGNO_S *);
int	  select_by_status(MAILSTREAM *, SEARCHSET **);
int	  select_by_rule(MAILSTREAM *, SEARCHSET **);
int	  select_by_thread(MAILSTREAM *, MSGNO_S *, SEARCHSET **);
char     *choose_a_rule(int);
int	  select_by_keyword(MAILSTREAM *, SEARCHSET **);
char     *choose_a_keyword(void);
int	  select_sort(struct pine *, int, SortOrder *, int *);
int       print_index(struct pine *, MSGNO_S *, int);



/*
 * List of Select options used by apply_* functions...
 */
static char *sel_pmt1 = N_("ALTER message selection : ");
ESCKEY_S sel_opts1[] = {
    /* TRANSLATORS: these are keymenu names for selecting. Broaden selection means
       we will add more messages to the selection, Narrow selection means we will
       remove some selections (like a logical AND instead of logical OR), and Flip
       Selected means that all the messages that are currently selected become unselected,
       and all the unselected messages become selected. */
    {'a', 'a', "A", N_("unselect All")},
    {'c', 'c', "C", NULL},
    {'b', 'b', "B", N_("Broaden selctn")},
    {'n', 'n', "N", N_("Narrow selctn")},
    {'f', 'f', "F", N_("Flip selected")},
    {-1, 0, NULL, NULL}
};


#define SEL_OPTS_THREAD 9	/* index number of "tHread" */
#define SEL_OPTS_THREAD_CH 'h'

char *sel_pmt2 = "SELECT criteria : ";
static ESCKEY_S sel_opts2[] = {
    /* TRANSLATORS: very short descriptions of message selection criteria. Select Cur
       means select the currently highlighted message; select by Number is by message
       number; Status is by status of the message, for example the message might be
       New or it might be Unseen or marked Important; Size has the Z upper case because
       it is a Z command; Keyword is an alpine keyword that has been set by the user;
       and Rule is an alpine rule */
    {'a', 'a', "A", N_("select All")},
    {'c', 'c', "C", N_("select Cur")},
    {'n', 'n', "N", N_("Number")},
    {'d', 'd', "D", N_("Date")},
    {'t', 't', "T", N_("Text")},
    {'s', 's', "S", N_("Status")},
    {'z', 'z', "Z", N_("siZe")},
    {'k', 'k', "K", N_("Keyword")},
    {'r', 'r', "R", N_("Rule")},
    {SEL_OPTS_THREAD_CH, 'h', "H", N_("tHread")},
    {-1, 0, NULL, NULL}
};


static ESCKEY_S sel_opts3[] = {
    /* TRANSLATORS: these are operations we can do on a set of selected messages.
       Del is Delete; Undel is Undelete; TakeAddr means to Take some Addresses into
       the address book; Save means to save the messages into another alpine folder;
       Export means to copy the messages to a file outside of alpine, external to
       alpine's world. */
    {'d', 'd',  "D", N_("Del")},
    {'u', 'u',  "U", N_("Undel")},
    {'r', 'r',  "R", N_("Reply")},
    {'f', 'f',  "F", N_("Forward")},
    {'%', '%',  "%", N_("Print")},
    {'t', 't',  "T", N_("TakeAddr")},
    {'s', 's',  "S", N_("Save")},
    {'e', 'e',  "E", N_("Export")},
    { -1,   0, NULL, NULL},
    { -1,   0, NULL, NULL},
    { -1,   0, NULL, NULL},
    { -1,   0, NULL, NULL},
    { -1,   0, NULL, NULL},
    { -1,   0, NULL, NULL},
    { -1,   0, NULL, NULL},
    {-1,    0, NULL, NULL}
};

static ESCKEY_S sel_opts4[] = {
    {'a', 'a', "A", N_("select All")},
    /* TRANSLATORS: select currrently highlighted message Thread */
    {'c', 'c', "C", N_("select Curthrd")},
    {'n', 'n', "N", N_("Number")},
    {'d', 'd', "D", N_("Date")},
    {'t', 't', "T", N_("Text")},
    {'s', 's', "S", N_("Status")},
    {'z', 'z', "Z", N_("siZe")},
    {'k', 'k', "K", N_("Keyword")},
    {'r', 'r', "R", N_("Rule")},
    {SEL_OPTS_THREAD_CH, 'h', "H", N_("tHread")},
    {-1, 0, NULL, NULL}
};


static char *sel_flag = 
    N_("Select New, Deleted, Answered, Forwarded, or Important messages ? ");
static char *sel_flag_not = 
    N_("Select NOT New, NOT Deleted, NOT Answered, NOT Forwarded or NOT Important msgs ? ");
static ESCKEY_S sel_flag_opt[] = {
    /* TRANSLATORS: When selecting messages by message Status these are the
       different types of Status you can select on. Is the message New, Recent,
       and so on. Not means flip the meaning of the selection to the opposite
       thing, so message is not New or not Important. */
    {'n', 'n', "N", N_("New")},
    {'*', '*', "*", N_("Important")},
    {'d', 'd', "D", N_("Deleted")},
    {'a', 'a', "A", N_("Answered")},
    {'f', 'f', "F", N_("Forwarded")},
    {-2, 0, NULL, NULL},
    {'!', '!', "!", N_("Not")},
    {-2, 0, NULL, NULL},
    {'r', 'r', "R", N_("Recent")},
    {'u', 'u', "U", N_("Unseen")},
    {-1, 0, NULL, NULL}
};


static ESCKEY_S sel_date_opt[] = {
    {0, 0, NULL, NULL},
    /* TRANSLATORS: options when selecting messages by Date */
    {ctrl('P'), 12, "^P", N_("Prev Day")},
    {ctrl('N'), 13, "^N", N_("Next Day")},
    {ctrl('X'), 11, "^X", N_("Cur Msg")},
    {ctrl('W'), 14, "^W", N_("Toggle When")},
    {KEY_UP,    12, "", ""},
    {KEY_DOWN,  13, "", ""},
    {-1, 0, NULL, NULL}
};


static char *sel_text =
    N_("Select based on To, From, Cc, Recip, Partic, Subject fields or All msg text ? ");
static char *sel_text_not =
    N_("Select based on NOT To, From, Cc, Recip, Partic, Subject or All msg text ? ");
static ESCKEY_S sel_text_opt[] = {
    /* TRANSLATORS: Select messages based on the text contained in the From line, or
       the Subject line, and so on. */
    {'f', 'f', "F", N_("From")},
    {'s', 's', "S", N_("Subject")},
    {'t', 't', "T", N_("To")},
    {'a', 'a', "A", N_("All Text")},
    {'c', 'c', "C", N_("Cc")},
    {'!', '!', "!", N_("Not")},
    {'r', 'r', "R", N_("Recipient")},
    {'p', 'p', "P", N_("Participant")},
    {'b', 'b', "B", N_("Body")},
    {-1, 0, NULL, NULL}
};

static ESCKEY_S choose_action[] = {
    {'c', 'c', "C", N_("Compose")},
    {'r', 'r', "R", N_("Reply")},
    {'f', 'f', "F", N_("Forward")},
    {'b', 'b', "B", N_("Bounce")},
    {-1, 0, NULL, NULL}
};

static char *select_num =
  N_("Enter comma-delimited list of numbers (dash between ranges): ");

static char *select_size_larger_msg =
  N_("Select messages with size larger than: ");

static char *select_size_smaller_msg =
  N_("Select messages with size smaller than: ");

static char *sel_size_larger  = N_("Larger");
static char *sel_size_smaller = N_("Smaller");
static ESCKEY_S sel_size_opt[] = {
    {0, 0, NULL, NULL},
    {ctrl('W'), 14, "^W", NULL},
    {-1, 0, NULL, NULL}
};

static ESCKEY_S sel_key_opt[] = {
    {0, 0, NULL, NULL},
    {ctrl('T'), 14, "^T", N_("To List")},
    {0, 0, NULL, NULL},
    {'!', '!', "!", N_("Not")},
    {-1, 0, NULL, NULL}
};

static ESCKEY_S flag_text_opt[] = {
    /* TRANSLATORS: these are types of flags (markers) that the user can
       set. For example, they can flag the message as an important message. */
    {'n', 'n', "N", N_("New")},
    {'*', '*', "*", N_("Important")},
    {'d', 'd', "D", N_("Deleted")},
    {'a', 'a', "A", N_("Answered")},
    {'f', 'f', "F", N_("Forwarded")},
    {'!', '!', "!", N_("Not")},
    {ctrl('T'), 10, "^T", N_("To Flag Details")},
    {-1, 0, NULL, NULL}
};


/*----------------------------------------------------------------------
         The giant switch on the commands for index and viewing

  Input:  command  -- The command char/code
          in_index -- flag indicating command is from index
          orig_command -- The original command typed before pre-processing
  Output: force_mailchk -- Set to tell caller to force call to new_mail().

  Result: Manifold

          Returns 1 if the message number or attachment to show changed 
 ---*/
int
process_cmd(struct pine *state, MAILSTREAM *stream, MSGNO_S *msgmap,
	    int command, CmdWhere in_index, int *force_mailchk)
{
    int           question_line, a_changed, flags = 0, ret, j;
    int           notrealinbox;
    long          new_msgno, del_count, old_msgno, i;
    long          start;
    char         *newfolder, prompt[MAX_SCREEN_COLS+1];
    CONTEXT_S    *tc;
    MESSAGECACHE *mc;
#if	defined(DOS) && !defined(_WINDOWS)
    extern long coreleft();
#endif

    dprint((4, "\n - process_cmd(cmd=%d) -\n", command));

    question_line         = -FOOTER_ROWS(state);
    state->mangled_screen = 0;
    state->mangled_footer = 0;
    state->mangled_header = 0;
    state->next_screen    = SCREEN_FUN_NULL;
    old_msgno             = mn_get_cur(msgmap);
    a_changed             = FALSE;
    *force_mailchk        = 0;

    switch (command) {
	/*------------- Help --------*/
      case MC_HELP :
	/*
	 * We're not using the h_mail_view portion of this right now because
	 * that call is being handled in scrolltool() before it gets
	 * here.  Leave it in case we change how it works.
	 */
	helper((in_index == MsgIndx)
		 ? h_mail_index
		 : (in_index == View)
		   ? h_mail_view
		   : h_mail_thread_index,
	       (in_index == MsgIndx)
	         ? _("HELP FOR MESSAGE INDEX")
		 : (in_index == View)
		   ? _("HELP FOR MESSAGE TEXT")
		   : _("HELP FOR THREAD INDEX"),
	       HLPD_NONE);
	dprint((4,"MAIL_CMD: did help command\n"));
	state->mangled_screen = 1;
	break;


          /*--------- Return to main menu ------------*/
      case MC_MAIN :
	state->next_screen = main_menu_screen;
	dprint((2,"MAIL_CMD: going back to main menu\n"));
	break;


          /*------- View message text --------*/
      case MC_VIEW_TEXT :
view_text:
	if(any_messages(msgmap, NULL, "to View")){
	    state->next_screen = mail_view_screen;
	}

	break;


          /*------- View attachment --------*/
      case MC_VIEW_ATCH :
	state->next_screen = attachment_screen;
	dprint((2,"MAIL_CMD: going to attachment screen\n"));
	break;


          /*---------- Previous message ----------*/
      case MC_PREVITEM :
	if(any_messages(msgmap, NULL, NULL)){
	    if((i = mn_get_cur(msgmap)) > 1L){
		mn_dec_cur(stream, msgmap,
			   (in_index == View && THREADING()
			    && sp_viewing_a_thread(stream))
			     ? MH_THISTHD
			     : (in_index == View)
			       ? MH_ANYTHD : MH_NONE);
		if(i == mn_get_cur(msgmap)){
		    PINETHRD_S *thrd, *topthrd;

		    if(THRD_INDX_ENABLED()){
			mn_dec_cur(stream, msgmap, MH_ANYTHD);
			if(i == mn_get_cur(msgmap))
			  q_status_message1(SM_ORDER, 0, 2,
				      _("Already on first %s in Zoomed Index"),
				      THRD_INDX() ? _("thread") : _("message"));
			else{
			    if(in_index == View
			       || F_ON(F_NEXT_THRD_WO_CONFIRM, state))
			      ret = 'y';
			    else
			      ret = want_to(_("View previous thread"), 'y', 'x',
					    NO_HELP, WT_NORM);

			    if(ret == 'y'){
				q_status_message(SM_ORDER, 0, 2,
						 _("Viewing previous thread"));
				new_msgno = mn_get_cur(msgmap);
				mn_set_cur(msgmap, i);
				if(unview_thread(state, stream, msgmap)){
				    state->next_screen = mail_index_screen;
				    state->view_skipped_index = 0;
				    state->mangled_screen = 1;
				}

				mn_set_cur(msgmap, new_msgno);
				if(THRD_AUTO_VIEW() && in_index == View){

				    thrd = fetch_thread(stream,
							mn_m2raw(msgmap,
								 new_msgno));
				    if(count_lflags_in_thread(stream, thrd,
							      msgmap,
							      MN_NONE) == 1){
					if(view_thread(state, stream, msgmap, 1)){
					    if(current_index_state)
					      msgmap->top_after_thrd = current_index_state->msg_at_top;

					    state->view_skipped_index = 1;
					    command = MC_VIEW_TEXT;
					    goto view_text;
					}
				    }
				}

				j = 0;
				if(THRD_AUTO_VIEW() && in_index != View){
				    thrd = fetch_thread(stream, mn_m2raw(msgmap, new_msgno));
				    if(thrd && thrd->top)
				      topthrd = fetch_thread(stream, thrd->top);

				    if(topthrd)
				      j = count_lflags_in_thread(stream, topthrd, msgmap, MN_NONE);
				}

				if(!THRD_AUTO_VIEW() || in_index == View || j != 1){
				    if(view_thread(state, stream, msgmap, 1)
				       && current_index_state)
				      msgmap->top_after_thrd = current_index_state->msg_at_top;

				}

				state->next_screen = SCREEN_FUN_NULL;
			    }
			    else
			      mn_set_cur(msgmap, i);	/* put it back */
			}
		    }
		    else
		      q_status_message1(SM_ORDER, 0, 2,
				  _("Already on first %s in Zoomed Index"),
				  THRD_INDX() ? _("thread") : _("message"));
		}
	    }
	    else{
		time_t now;

		if(!IS_NEWS(stream)
		   && ((now = time(0)) > state->last_nextitem_forcechk)){
		    *force_mailchk = 1;
		    /* check at most once a second */
		    state->last_nextitem_forcechk = now;
		}

		q_status_message1(SM_ORDER, 0, 1, _("Already on first %s"),
				  THRD_INDX() ? _("thread") : _("message"));
	    }
	}

	break;


          /*---------- Next Message ----------*/
      case MC_NEXTITEM :
	if(mn_get_total(msgmap) > 0L
	   && ((i = mn_get_cur(msgmap)) < mn_get_total(msgmap))){
	    mn_inc_cur(stream, msgmap,
		       (in_index == View && THREADING()
		        && sp_viewing_a_thread(stream))
			 ? MH_THISTHD
			 : (in_index == View)
			   ? MH_ANYTHD : MH_NONE);
	    if(i == mn_get_cur(msgmap)){
		PINETHRD_S *thrd, *topthrd;

		if(THRD_INDX_ENABLED()){
		    if(!THRD_INDX())
		      mn_inc_cur(stream, msgmap, MH_ANYTHD);

		    if(i == mn_get_cur(msgmap)){
			if(any_lflagged(msgmap, MN_HIDE))
			  any_messages(NULL, "more", "in Zoomed Index");
			else
			  goto nfolder;
		    }
		    else{
			if(in_index == View
			   || F_ON(F_NEXT_THRD_WO_CONFIRM, state))
			  ret = 'y';
			else
			  ret = want_to(_("View next thread"), 'y', 'x',
					NO_HELP, WT_NORM);

			if(ret == 'y'){
			    q_status_message(SM_ORDER, 0, 2,
					     _("Viewing next thread"));
			    new_msgno = mn_get_cur(msgmap);
			    mn_set_cur(msgmap, i);
			    if(unview_thread(state, stream, msgmap)){
				state->next_screen = mail_index_screen;
				state->view_skipped_index = 0;
				state->mangled_screen = 1;
			    }

			    mn_set_cur(msgmap, new_msgno);
			    if(THRD_AUTO_VIEW() && in_index == View){

				thrd = fetch_thread(stream,
						    mn_m2raw(msgmap,
							     new_msgno));
				if(count_lflags_in_thread(stream, thrd,
							  msgmap,
							  MN_NONE) == 1){
				    if(view_thread(state, stream, msgmap, 1)){
					if(current_index_state)
					  msgmap->top_after_thrd = current_index_state->msg_at_top;

					state->view_skipped_index = 1;
					command = MC_VIEW_TEXT;
					goto view_text;
				    }
				}
			    }

			    j = 0;
			    if(THRD_AUTO_VIEW() && in_index != View){
				thrd = fetch_thread(stream, mn_m2raw(msgmap, new_msgno));
				if(thrd && thrd->top)
				  topthrd = fetch_thread(stream, thrd->top);

				if(topthrd)
				j = count_lflags_in_thread(stream, topthrd, msgmap, MN_NONE);
			    }

			    if(!THRD_AUTO_VIEW() || in_index == View || j != 1){
				if(view_thread(state, stream, msgmap, 1)
				   && current_index_state)
				  msgmap->top_after_thrd = current_index_state->msg_at_top;

			    }

			    state->next_screen = SCREEN_FUN_NULL;
			}
			else
			  mn_set_cur(msgmap, i);	/* put it back */
		    }
		}
		else if(THREADING()
			&& (thrd = fetch_thread(stream, mn_m2raw(msgmap, i)))
			&& thrd->next
			&& get_lflag(stream, NULL, thrd->rawno, MN_COLL)){
		       q_status_message(SM_ORDER, 0, 2,
			       _("Expand collapsed thread to see more messages"));
		}
		else
		  any_messages(NULL, "more", "in Zoomed Index");
	    }
	}
	else{
	    time_t now;
nfolder:
	    prompt[0] = '\0';
	    if(IS_NEWS(stream)
	       || (state->context_current->use & CNTXT_INCMNG)){
		char nextfolder[MAXPATH];

		strncpy(nextfolder, state->cur_folder, sizeof(nextfolder));
		nextfolder[sizeof(nextfolder)-1] = '\0';
		if(next_folder(NULL, nextfolder, sizeof(nextfolder), nextfolder,
			       state->context_current, NULL, NULL))
		  strncpy(prompt, _(".  Press TAB for next folder."),
			  sizeof(prompt));
		else
		  strncpy(prompt, _(".  No more folders to TAB to."),
			  sizeof(prompt));

		prompt[sizeof(prompt)-1] = '\0';
	    }

	    any_messages(NULL, (mn_get_total(msgmap) > 0L) ? "more" : NULL,
			 prompt[0] ? prompt : NULL);

	    if(!IS_NEWS(stream)
	       && ((now = time(0)) > state->last_nextitem_forcechk)){
		*force_mailchk = 1;
		/* check at most once a second */
		state->last_nextitem_forcechk = now;
	    }
	}

	break;


          /*---------- Delete message ----------*/
      case MC_DELETE :
	(void) cmd_delete(state, msgmap, MCMD_NONE,
			  (in_index == View) ? cmd_delete_view : cmd_delete_index);
	break;
          

          /*---------- Undelete message ----------*/
      case MC_UNDELETE :
	(void) cmd_undelete(state, msgmap, MCMD_NONE);
	update_titlebar_status();
	break;


          /*---------- Reply to message ----------*/
      case MC_REPLY :
	(void) cmd_reply(state, msgmap, MCMD_NONE);
	break;


          /*---------- Forward message ----------*/
      case MC_FORWARD :
	(void) cmd_forward(state, msgmap, MCMD_NONE);
	break;


          /*---------- Quit pine ------------*/
      case MC_QUIT :
	state->next_screen = quit_screen;
	dprint((1,"MAIL_CMD: quit\n"));		    
	break;


          /*---------- Compose message ----------*/
      case MC_COMPOSE :
	state->prev_screen = (in_index == View) ? mail_view_screen
						: mail_index_screen;
	compose_screen(state);
	state->mangled_screen = 1;
	if (state->next_screen)
	  a_changed = TRUE;
	break;


          /*---------- Alt Compose message ----------*/
      case MC_ROLE :
	state->prev_screen = (in_index == View) ? mail_view_screen
						: mail_index_screen;
	role_compose(state);
	if(state->next_screen)
	  a_changed = TRUE;

	break;


          /*--------- Folders menu ------------*/
      case MC_FOLDERS :
	state->start_in_context = 1;

          /*--------- Top of Folders list menu ------------*/
      case MC_COLLECTIONS :
	state->next_screen = folder_screen;
	dprint((2,"MAIL_CMD: going to folder/collection menu\n"));
	break;


          /*---------- Open specific new folder ----------*/
      case MC_GOTO :
	tc = (state->context_last && !NEWS_TEST(state->context_current)) 
	       ? state->context_last : state->context_current;

	newfolder = broach_folder(question_line, 1, &notrealinbox, &tc);
	if(newfolder){
	    visit_folder(state, newfolder, tc, NULL, notrealinbox ? 0L : DB_INBOXWOCNTXT);
	    a_changed = TRUE;
	}

	break;
    	  
    	    
          /*------- Go to Index Screen ----------*/
      case MC_INDEX :
	state->next_screen = mail_index_screen;
	break;

          /*------- Skip to next interesting message -----------*/
      case MC_TAB :
	if(THRD_INDX()){
	    PINETHRD_S *thrd;

	    /*
	     * If we're in the thread index, start looking after this
	     * thread. We don't want to match something in the current
	     * thread.
	     */
	    start = mn_get_cur(msgmap);
	    thrd = fetch_thread(stream, mn_m2raw(msgmap, mn_get_cur(msgmap)));
	    if(mn_get_revsort(msgmap)){
		/* if reversed, top of thread is last one before next thread */
		if(thrd && thrd->top)
		  start = mn_raw2m(msgmap, thrd->top);
	    }
	    else{
		/* last msg of thread is at the ends of the branches/nexts */
		while(thrd){
		    start = mn_raw2m(msgmap, thrd->rawno);
		    if(thrd->branch)
		      thrd = fetch_thread(stream, thrd->branch);
		    else if(thrd->next)
		      thrd = fetch_thread(stream, thrd->next);
		    else
		      thrd = NULL;
		}
	    }

	    /*
	     * Flags is 0 in this case because we want to not skip
	     * messages inside of threads so that we can find threads
	     * which have some unseen messages even though the top-level
	     * of the thread is already seen.
	     * If new_msgno ends up being a message which is not visible
	     * because it isn't at the top-level, the current message #
	     * will be adjusted below in adjust_cur.
	     */
	    flags = 0;
	    new_msgno = next_sorted_flagged((F_UNDEL 
					     | F_UNSEEN
					     | ((F_ON(F_TAB_TO_NEW,state))
						 ? 0 : F_OR_FLAG)),
					    stream, start, &flags);
	}
	else if(THREADING() && sp_viewing_a_thread(stream)){
	    PINETHRD_S *thrd, *topthrd = NULL;

	    start = mn_get_cur(msgmap);

	    /*
	     * Things are especially complicated when we're viewing_a_thread
	     * from the thread index. First we have to check within the
	     * current thread for a new message. If none is found, then
	     * we search in the next threads and offer to continue in
	     * them. Then we offer to go to the next folder.
	     */
	    flags = NSF_SKIP_CHID;
	    new_msgno = next_sorted_flagged((F_UNDEL 
					     | F_UNSEEN
					     | ((F_ON(F_TAB_TO_NEW,state))
					       ? 0 : F_OR_FLAG)),
					    stream, start, &flags);
	    /*
	     * If we found a match then we are done, that is another message
	     * in the current thread index. Otherwise, we have to look
	     * further.
	     */
	    if(!(flags & NSF_FLAG_MATCH)){
		ret = 'n';
		while(1){

		    flags = 0;
		    new_msgno = next_sorted_flagged((F_UNDEL 
						     | F_UNSEEN
						     | ((F_ON(F_TAB_TO_NEW,
							      state))
							 ? 0 : F_OR_FLAG)),
						    stream, start, &flags);
		    /*
		     * If we got a match, new_msgno is a message in
		     * a different thread from the one we are viewing.
		     */
		    if(flags & NSF_FLAG_MATCH){
			thrd = fetch_thread(stream, mn_m2raw(msgmap,new_msgno));
			if(thrd && thrd->top)
			  topthrd = fetch_thread(stream, thrd->top);

			if(F_OFF(F_AUTO_OPEN_NEXT_UNREAD, state)){
			    static ESCKEY_S next_opt[] = {
				{'y', 'y', "Y", N_("Yes")},
				{'n', 'n', "N", N_("No")},
				{TAB, 'n', "Tab", N_("NextNew")},
				{-1, 0, NULL, NULL}
			    };

			    if(in_index)
			      snprintf(prompt, sizeof(prompt), _("View thread number %s? "),
				     topthrd ? comatose(topthrd->thrdno) : "?");
			    else
			      snprintf(prompt, sizeof(prompt),
				     _("View message in thread number %s? "),
				     topthrd ? comatose(topthrd->thrdno) : "?");

			    prompt[sizeof(prompt)-1] = '\0';
				    
			    ret = radio_buttons(prompt, -FOOTER_ROWS(state),
						next_opt, 'y', 'x', NO_HELP,
						RB_NORM);
			    if(ret == 'x'){
				cmd_cancelled(NULL);
				goto get_out;
			    }
			}
			else
			  ret = 'y';

			if(ret == 'y'){
			    if(unview_thread(state, stream, msgmap)){
				state->next_screen = mail_index_screen;
				state->view_skipped_index = 0;
				state->mangled_screen = 1;
			    }

			    mn_set_cur(msgmap, new_msgno);
			    if(THRD_AUTO_VIEW()){

				if(count_lflags_in_thread(stream, topthrd,
				                         msgmap, MN_NONE) == 1){
				    if(view_thread(state, stream, msgmap, 1)){
					if(current_index_state)
					  msgmap->top_after_thrd = current_index_state->msg_at_top;

					state->view_skipped_index = 1;
					command = MC_VIEW_TEXT;
					goto view_text;
				    }
				}
			    }

			    if(view_thread(state, stream, msgmap, 1) && current_index_state)
			      msgmap->top_after_thrd = current_index_state->msg_at_top;

			    state->next_screen = SCREEN_FUN_NULL;
			    break;
			}
			else if(ret == 'n' && topthrd){
			    /*
			     * skip to end of this thread and look starting
			     * in the next thread.
			     */
			    if(mn_get_revsort(msgmap)){
				/*
				 * if reversed, top of thread is last one
				 * before next thread
				 */
				start = mn_raw2m(msgmap, topthrd->rawno);
			    }
			    else{
				/*
				 * last msg of thread is at the ends of
				 * the branches/nexts
				 */
				thrd = topthrd;
				while(thrd){
				    start = mn_raw2m(msgmap, thrd->rawno);
				    if(thrd->branch)
				      thrd = fetch_thread(stream, thrd->branch);
				    else if(thrd->next)
				      thrd = fetch_thread(stream, thrd->next);
				    else
				      thrd = NULL;
				}
			    }
			}
			else if(ret == 'n')
			  break;
		    }
		    else
		      break;
		}
	    }
	}
	else{

	    start = mn_get_cur(msgmap);

	    /*
	     * If we are on a collapsed thread, start looking after the
	     * collapsed part, unless we are viewing the message.
	     */
	    if(THREADING() && in_index != View){
		PINETHRD_S *thrd;
		long        rawno;
		int         collapsed;

		rawno = mn_m2raw(msgmap, start);
		thrd = fetch_thread(stream, rawno);
		collapsed = thrd && thrd->next
			    && get_lflag(stream, NULL, rawno, MN_COLL);

		if(collapsed){
		    if(mn_get_revsort(msgmap)){
			if(thrd && thrd->top)
			  start = mn_raw2m(msgmap, thrd->top);
		    }
		    else{
			while(thrd){
			    start = mn_raw2m(msgmap, thrd->rawno);
			    if(thrd->branch)
			      thrd = fetch_thread(stream, thrd->branch);
			    else if(thrd->next)
			      thrd = fetch_thread(stream, thrd->next);
			    else
			      thrd = NULL;
			}
		    }

		}
	    }

	    new_msgno = next_sorted_flagged((F_UNDEL 
					     | F_UNSEEN
					     | ((F_ON(F_TAB_TO_NEW,state))
						 ? 0 : F_OR_FLAG)),
					    stream, start, &flags);
	}

	/*
	 * If there weren't any unread messages left, OR there
	 * aren't any messages at all, we may want to offer to
	 * go on to the next folder...
	 */
	if(flags & NSF_FLAG_MATCH){
	    mn_set_cur(msgmap, new_msgno);
	    if(in_index != View)
	      adjust_cur_to_visible(stream, msgmap);
	}
	else{
	    int in_inbox = sp_flagged(stream, SP_INBOX);

	    if(state->context_current
	       && ((NEWS_TEST(state->context_current)
		    && context_isambig(state->cur_folder))
		   || ((state->context_current->use & CNTXT_INCMNG)
		       && (in_inbox
			   || folder_index(state->cur_folder,
					   state->context_current,
					   FI_FOLDER) >= 0)))){
		char	    nextfolder[MAXPATH];
		MAILSTREAM *nextstream = NULL;
		long	    recent_cnt;
		int         did_cancel = 0;

		strncpy(nextfolder, state->cur_folder, sizeof(nextfolder));
		nextfolder[sizeof(nextfolder)-1] = '\0';
		while(1){
		    if(!(next_folder(&nextstream, nextfolder, sizeof(nextfolder), nextfolder,
				     state->context_current, &recent_cnt,
				     F_ON(F_TAB_NO_CONFIRM,state)
				       ? NULL : &did_cancel))){
			if(!in_inbox){
			    static ESCKEY_S inbox_opt[] = {
				{'y', 'y', "Y", N_("Yes")},
				{'n', 'n', "N", N_("No")},
				{TAB, 'z', "Tab", N_("To Inbox")},
				{-1, 0, NULL, NULL}
			    };

			    if(F_ON(F_RET_INBOX_NO_CONFIRM,state))
			      ret = 'y';
			    else{
				/* TRANSLATORS: this is a question, with some information followed
				   by Return to INBOX? */
				if(state->context_current->use&CNTXT_INCMNG)
				  snprintf(prompt, sizeof(prompt), _("No more incoming folders. Return to \"%s\"? "), state->inbox_name);
				else
				  snprintf(prompt, sizeof(prompt), _("No more news groups. Return to \"%s\"? "), state->inbox_name);

				ret = radio_buttons(prompt, -FOOTER_ROWS(state),
						    inbox_opt, 'y', 'x',
						    NO_HELP, RB_NORM);
			    }

			    /*
			     * 'z' is a synonym for 'y'.  It is not 'y'
			     * so that it isn't displayed as a default
			     * action with square-brackets around it
			     * in the keymenu...
			     */
			    if(ret == 'y' || ret == 'z'){
				visit_folder(state, state->inbox_name,
					     state->context_current,
					     NULL, DB_INBOXWOCNTXT);
				a_changed = TRUE;
			    }
			}
			else if (did_cancel)
			  cmd_cancelled(NULL);			
			else{
			  if(state->context_current->use&CNTXT_INCMNG)
			    q_status_message(SM_ORDER, 0, 2, _("No more incoming folders"));
			  else
			    q_status_message(SM_ORDER, 0, 2, _("No more news groups"));
			}

			break;
		    }

		    {char *front, type[80], cnt[80], fbuf[MAX_SCREEN_COLS/2+1];
		     int rbspace, avail, need, take_back;

			/*
			 * View_next_
			 * Incoming_folder_ or news_group_ or folder_ or group_
			 * "foldername"
			 * _(13 recent) or _(some recent) or nothing
			 * ?_
			 */
			front = "View next";
			strncpy(type,
				(state->context_current->use & CNTXT_INCMNG)
				    ? "Incoming folder" : "news group",
				sizeof(type));
			type[sizeof(type)-1] = '\0';
			snprintf(cnt, sizeof(cnt), " (%.*s %s)", sizeof(cnt)-20,
				recent_cnt ? long2string(recent_cnt) : "some",
				F_ON(F_TAB_USES_UNSEEN, ps_global)
				    ? "unseen" : "recent");
			cnt[sizeof(cnt)-1] = '\0';

			/*
			 * Space reserved for radio_buttons call.
			 * If we make this 3 then radio_buttons won't mess
			 * with the prompt. If we make it 2, then we get
			 * one more character to use but radio_buttons will
			 * cut off the last character of our prompt, which is
			 * ok because it is a space.
			 */
			rbspace = 2;
			avail = ps_global->ttyo ? ps_global->ttyo->screen_cols
						: 80;
			need = strlen(front)+1 + strlen(type)+1 +
			       + strlen(nextfolder)+2 + strlen(cnt) +
			       2 + rbspace;
			if(avail < need){
			    take_back = strlen(type);
			    strncpy(type,
				    (state->context_current->use & CNTXT_INCMNG)
					? "folder" : "group", sizeof(type));
			    take_back -= strlen(type);
			    need -= take_back;
			    if(avail < need){
				need -= strlen(cnt);
				cnt[0] = '\0';
			    }
			}

			snprintf(prompt, sizeof(prompt), "%.*s %.*s \"%.*s\"%.*s? ",
				sizeof(prompt)/8, front,
				sizeof(prompt)/8, type,
				sizeof(prompt)/2,
				short_str(nextfolder, fbuf, sizeof(fbuf),
					  strlen(nextfolder) -
					    ((need>avail) ? (need-avail) : 0),
					  MidDots),
				sizeof(prompt)/8, cnt);
			prompt[sizeof(prompt)-1] = '\0';
		    }

		    /*
		     * When help gets added, this'll have to become
		     * a loop like the rest...
		     */
		    if(F_OFF(F_AUTO_OPEN_NEXT_UNREAD, state)){
			static ESCKEY_S next_opt[] = {
			    {'y', 'y', "Y", N_("Yes")},
			    {'n', 'n', "N", N_("No")},
			    {TAB, 'n', "Tab", N_("NextNew")},
			    {-1, 0, NULL, NULL}
			};

			ret = radio_buttons(prompt, -FOOTER_ROWS(state),
					    next_opt, 'y', 'x', NO_HELP,
					    RB_NORM);
			if(ret == 'x'){
			    cmd_cancelled(NULL);
			    break;
			}
		    }
		    else
		      ret = 'y';

		    if(ret == 'y'){
			if(nextstream && sp_dead_stream(nextstream))
			  nextstream = NULL;

			visit_folder(state, nextfolder,
				     state->context_current, nextstream,
				     DB_FROMTAB);
			/* visit_folder takes care of nextstream */
			nextstream = NULL;
			a_changed = TRUE;
			break;
		    }
		}

		if(nextstream)
		  pine_mail_close(nextstream);
	    }
	    else
	      any_messages(NULL,
			   (mn_get_total(msgmap) > 0L)
			     ? IS_NEWS(stream) ? "more undeleted" : "more new"
			     : NULL,
			   NULL);
	}

get_out:

	break;


          /*------- Zoom -----------*/
      case MC_ZOOM :
	/*
	 * Right now the way zoom is implemented is sort of silly.
	 * There are two per-message flags where just one and a 
	 * global "zoom mode" flag to suppress messags from the index
	 * should suffice.
	 */
	if(any_messages(msgmap, NULL, "to Zoom on")){
	    if(unzoom_index(state, stream, msgmap)){
		dprint((4, "\n\n ---- Exiting ZOOM mode ----\n"));
		q_status_message(SM_ORDER,0,2, _("Index Zoom Mode is now off"));
	    }
	    else if((i = zoom_index(state, stream, msgmap, MN_SLCT)) != 0){
		if(any_lflagged(msgmap, MN_HIDE)){
		    dprint((4,"\n\n ---- Entering ZOOM mode ----\n"));
		    q_status_message4(SM_ORDER, 0, 2,
				      _("In Zoomed Index of %s%s%s%s.  Use \"Z\" to restore regular Index"),
				      THRD_INDX() ? "" : comatose(i),
				      THRD_INDX() ? "" : " ",
				      THRD_INDX() ? _("threads") : _("message"),
				      THRD_INDX() ? "" : plural(i));
		}
		else
		  q_status_message(SM_ORDER, 0, 2,
		     _("All messages selected, so not entering Index Zoom Mode"));
	    }
	    else
	      any_messages(NULL, "selected", "to Zoom on");
	}

	break;


          /*---------- print message on paper ----------*/
      case MC_PRINTMSG :
	if(any_messages(msgmap, NULL, "to print"))
	  (void) cmd_print(state, msgmap, MCMD_NONE, in_index);

	break;


          /*---------- Take Address ----------*/
      case MC_TAKE :
	if(F_ON(F_ENABLE_ROLE_TAKE, state) ||
	   any_messages(msgmap, NULL, "to Take address from"))
	  (void) cmd_take_addr(state, msgmap, MCMD_NONE);

	break;


          /*---------- Save Message ----------*/
      case MC_SAVE :
	if(any_messages(msgmap, NULL, "to Save"))
	  (void) cmd_save(state, stream, msgmap, MCMD_NONE, in_index);

	break;


          /*---------- Export message ----------*/
      case MC_EXPORT :
	if(any_messages(msgmap, NULL, "to Export")){
	    (void) cmd_export(state, msgmap, question_line, MCMD_NONE);
	    state->mangled_footer = 1;
	}

	break;


          /*---------- Expunge ----------*/
      case MC_EXPUNGE :
	cmd_expunge(state, stream, msgmap);
	break;


          /*------- Unexclude -----------*/
      case MC_UNEXCLUDE :
	if(!(IS_NEWS(stream) && stream->rdonly)){
	    q_status_message(SM_ORDER, 0, 3,
			     _("Unexclude not available for mail folders"));
	}
	else if(any_lflagged(msgmap, MN_EXLD)){
	    SEARCHPGM *pgm;
	    long       i;
	    int	       exbits;

	    /*
	     * Since excluded means "hidden deleted" and "killed",
	     * the count should reflect the former.
	     */
	    pgm = mail_newsearchpgm();
	    pgm->deleted = 1;
	    pine_mail_search_full(stream, NULL, pgm, SE_NOPREFETCH | SE_FREE);
	    for(i = 1L, del_count = 0L; i <= stream->nmsgs; i++)
	      if((mc = mail_elt(stream, i)) && mc->searched
		 && get_lflag(stream, NULL, i, MN_EXLD)
		 && !(msgno_exceptions(stream, i, "0", &exbits, FALSE)
		      && (exbits & MSG_EX_FILTERED)))
		del_count++;

	    if(del_count > 0L){
		state->mangled_footer = 1;
		snprintf(prompt, sizeof(prompt), "UNexclude %ld message%s in %.*s", del_count,
			plural(del_count), sizeof(prompt)-40,
			pretty_fn(state->cur_folder));
		prompt[sizeof(prompt)-1] = '\0';
		if(F_ON(F_FULL_AUTO_EXPUNGE, state)
		   || (F_ON(F_AUTO_EXPUNGE, state)
		       && (state->context_current
			   && (state->context_current->use & CNTXT_INCMNG))
		       && context_isambig(state->cur_folder))
		   || want_to(prompt, 'y', 0, NO_HELP, WT_NORM) == 'y'){
		    long save_cur_rawno;
		    int  were_viewing_a_thread;

		    save_cur_rawno = mn_m2raw(msgmap, mn_get_cur(msgmap));
		    were_viewing_a_thread = (THREADING()
					     && sp_viewing_a_thread(stream));

		    if(msgno_include(stream, msgmap, MI_NONE)){
			clear_index_cache(stream, 0);

			if(stream && stream->spare)
			  erase_threading_info(stream, msgmap);

			refresh_sort(stream, msgmap, SRT_NON);
		    }

		    if(were_viewing_a_thread){
			if(save_cur_rawno > 0L)
			  mn_set_cur(msgmap, mn_raw2m(msgmap,save_cur_rawno));

			if(view_thread(state, stream, msgmap, 1) && current_index_state)
			  msgmap->top_after_thrd = current_index_state->msg_at_top;
		    }

		    if(save_cur_rawno > 0L)
		      mn_set_cur(msgmap, mn_raw2m(msgmap,save_cur_rawno));

		    state->mangled_screen = 1;
		    q_status_message2(SM_ORDER, 0, 4,
				      "%s message%s UNexcluded",
				      long2string(del_count),
				      plural(del_count));

		    if(in_index != View)
		      adjust_cur_to_visible(stream, msgmap);
		}
		else
		  any_messages(NULL, NULL, "UNexcluded");
	    }
	    else
	      any_messages(NULL, "excluded", "to UNexclude");
	}
	else
	  any_messages(NULL, "excluded", "to UNexclude");

	break;


          /*------- Make Selection -----------*/
      case MC_SELECT :
	if(any_messages(msgmap, NULL, "to Select")){
	    if(aggregate_select(state, msgmap, question_line, in_index) == 0
	       && (in_index == MsgIndx || in_index == ThrdIndx)
	       && F_ON(F_AUTO_ZOOM, state)
	       && any_lflagged(msgmap, MN_SLCT) > 0L
	       && !any_lflagged(msgmap, MN_HIDE))
	      (void) zoom_index(state, stream, msgmap, MN_SLCT);
	}

	break;


          /*------- Toggle Current Message Selection State -----------*/
      case MC_SELCUR :
	if(any_messages(msgmap, NULL, NULL)){
	   if((select_by_current(state, msgmap, in_index)
	       || (F_OFF(F_UNSELECT_WONT_ADVANCE, state)
	           && !any_lflagged(msgmap, MN_HIDE)))
	       && (i = mn_get_cur(msgmap)) < mn_get_total(msgmap)){
		/* advance current */
		mn_inc_cur(stream, msgmap,
			   (in_index == View && THREADING()
			    && sp_viewing_a_thread(stream))
			     ? MH_THISTHD
			     : (in_index == View)
			       ? MH_ANYTHD : MH_NONE);
	   }
	}

	break;


          /*------- Apply command -----------*/
      case MC_APPLY :
	if(any_messages(msgmap, NULL, NULL)){
	    if(any_lflagged(msgmap, MN_SLCT) > 0L){
		if(apply_command(state, stream, msgmap, 0,
				 AC_NONE, question_line)){
		    if(F_ON(F_AUTO_UNSELECT, state)){
			agg_select_all(stream, msgmap, NULL, 0);
			unzoom_index(state, stream, msgmap);
		    }
		    else if(F_ON(F_AUTO_UNZOOM, state))
		      unzoom_index(state, stream, msgmap);
		}
	    }
	    else
	      any_messages(NULL, NULL, "to Apply command to.  Try \"Select\"");
	}

	break;


          /*-------- Sort command -------*/
      case MC_SORT :
	{
	    int were_threading = THREADING();
	    SortOrder sort = mn_get_sort(msgmap);
	    int	      rev  = mn_get_revsort(msgmap);

	    dprint((1,"MAIL_CMD: sort\n"));		    
	    if(select_sort(state, question_line, &sort, &rev)){
		/* $ command reinitializes threading collapsed/expanded info */
		if(SORT_IS_THREADED(msgmap) && !SEP_THRDINDX())
		  erase_threading_info(stream, msgmap);

		if(ps_global && ps_global->ttyo){
		    blank_keymenu(ps_global->ttyo->screen_rows - 2, 0);
		    ps_global->mangled_footer = 1;
		}

		sort_folder(stream, msgmap, sort, rev, SRT_VRB|SRT_MAN);
	    }

	    state->mangled_footer = 1;

	    /*
	     * We've changed whether we are threading or not so we need to
	     * exit the index and come back in so that we switch between the
	     * thread index and the regular index. Sort_folder will have
	     * reset viewing_a_thread if necessary.
	     */
	    if(SEP_THRDINDX()
	       && ((!were_threading && THREADING())
	            || (were_threading && !THREADING()))){
		state->next_screen = mail_index_screen;
		state->mangled_screen = 1;
	    }
	}

	break;


          /*------- Toggle Full Headers -----------*/
      case MC_FULLHDR :
	state->full_header++;
	if(state->full_header == 1){
	    if(!(state->quote_suppression_threshold
	         && (state->some_quoting_was_suppressed || in_index != View)))
	      state->full_header++;
	}
	else if(state->full_header > 2)
	  state->full_header = 0;

	switch(state->full_header){
	  case 0:
	    q_status_message(SM_ORDER, 0, 3,
			     _("Display of full headers is now off."));
	    break;

	  case 1:
	    q_status_message1(SM_ORDER, 0, 3,
			  _("Quotes displayed, use %s to see full headers"),
			  F_ON(F_USE_FK, state) ? "F9" : "H");
	    break;

	  case 2:
	    q_status_message(SM_ORDER, 0, 3,
			     _("Display of full headers is now on."));
	    break;

	}

	a_changed = TRUE;
	break;


      case MC_TOGGLE :
	a_changed = TRUE;
	break;


#ifdef SMIME
          /*------- Try to decrypt message -----------*/
      case MC_DECRYPT:
	if(state->smime && state->smime->need_passphrase)
	  smime_get_passphrase();

	a_changed = TRUE;
	break;

      case MC_SECURITY:
	state->next_screen = smime_info_screen;
	break;
#endif


          /*------- Bounce -----------*/
      case MC_BOUNCE :
	(void) cmd_bounce(state, msgmap, MCMD_NONE);
	break;


          /*------- Flag -----------*/
      case MC_FLAG :
	dprint((4, "\n - flag message -\n"));
	(void) cmd_flag(state, msgmap, MCMD_NONE);
	break;


          /*------- Pipe message -----------*/
      case MC_PIPE :
	(void) cmd_pipe(state, msgmap, MCMD_NONE);
	break;


          /*--------- Default, unknown command ----------*/
      default:
	panic("Unexpected command case");
	break;
    }

    return((a_changed || mn_get_cur(msgmap) != old_msgno) ? 1 : 0);
}



/*----------------------------------------------------------------------
     Map some of the special characters into sensible strings for human
   consumption.
   c is a UCS-4 character!
  ----*/
char *
pretty_command(UCS c)
{
    static char  buf[10];
    char	*s;

    buf[0] = '\0';
    s = buf;

    switch(c){
      case ' '       : s = "SPACE";		break;
      case '\033'    : s = "ESC";		break;
      case '\177'    : s = "DEL";		break;
      case ctrl('I') : s = "TAB";		break;
      case ctrl('J') : s = "LINEFEED";		break;
      case ctrl('M') : s = "RETURN";		break;
      case ctrl('Q') : s = "XON";		break;
      case ctrl('S') : s = "XOFF";		break;
      case KEY_UP    : s = "Up Arrow";		break;
      case KEY_DOWN  : s = "Down Arrow";	break;
      case KEY_RIGHT : s = "Right Arrow";	break;
      case KEY_LEFT  : s = "Left Arrow";	break;
      case KEY_PGUP  : s = "Prev Page";		break;
      case KEY_PGDN  : s = "Next Page";		break;
      case KEY_HOME  : s = "Home";		break;
      case KEY_END   : s = "End";		break;
      case KEY_DEL   : s = "Delete";		break; /* Not necessary DEL! */
      case KEY_JUNK  : s = "Junk!";		break;
      case BADESC    : s = "Bad Esc";		break;
      case NO_OP_IDLE      : s = "NO_OP_IDLE";		break;
      case NO_OP_COMMAND   : s = "NO_OP_COMMAND";	break;
      case KEY_RESIZE      : s = "KEY_RESIZE";		break;
      case KEY_UTF8        : s = "KEY_UTF8";		break;
      case KEY_MOUSE       : s = "KEY_MOUSE";		break;
      case KEY_SCRLUPL     : s = "KEY_SCRLUPL";		break;
      case KEY_SCRLDNL     : s = "KEY_SCRLDNL";		break;
      case KEY_SCRLTO      : s = "KEY_SCRLTO";		break;
      case KEY_XTERM_MOUSE : s = "KEY_XTERM_MOUSE";	break;
      case KEY_DOUBLE_ESC  : s = "KEY_DOUBLE_ESC";	break;
      case CTRL_KEY_UP     : s = "Ctrl Up Arrow";	break;
      case CTRL_KEY_DOWN   : s = "Ctrl Down Arrow";	break;
      case CTRL_KEY_RIGHT  : s = "Ctrl Right Arrow";	break;
      case CTRL_KEY_LEFT   : s = "Ctrl Left Arrow";	break;
      case PF1	     :
      case PF2	     :
      case PF3	     :
      case PF4	     :
      case PF5	     :
      case PF6	     :
      case PF7	     :
      case PF8	     :
      case PF9	     :
      case PF10	     :
      case PF11	     :
      case PF12	     :
        snprintf(s = buf, sizeof(buf), "F%ld", (long) (c - PF1 + 1));
	break;

      default:
	if(c < ' ' || (c >= 0x80 && c < 0xA0)){
	  char d;
	  int  c1;

	  c1 = (c >= 0x80);
	  d = (c & 0x1f) + 'A' - 1;
	  snprintf(s = buf, sizeof(buf), "%c%c", c1 ? '~' : '^', d);
	}
	else{
	  memset(buf, 0, sizeof(buf));
	  utf8_put((unsigned char *) buf, (unsigned long) c);
	}

	break;
    }

    return(s);
}


/*----------------------------------------------------------------------
   Complain about bogus input

  Args: ch -- input command to complain about
	help -- string indicating where to get help

 ----*/
void
bogus_command(UCS cmd, char *help)
{
    if(cmd == ctrl('Q') || cmd == ctrl('S'))
      q_status_message1(SM_ASYNC, 0, 2,
 "%s char received.  Set \"preserve-start-stop\" feature in Setup/Config.",
			pretty_command(cmd));
    else if(cmd == KEY_JUNK)
      q_status_message3(SM_ORDER, 0, 2,
		      "Invalid key pressed.%s%s%s",
		      (help) ? " Use " : "",
		      (help) ?  help   : "",
		      (help) ? " for help" : "");
    else
      q_status_message4(SM_ORDER, 0, 2,
	  "Command \"%s\" not defined for this screen.%s%s%s",
		      pretty_command(cmd),
		      (help) ? " Use " : "",
		      (help) ?  help   : "",
		      (help) ? " for help" : "");
}


void
bogus_utf8_command(char *cmd, char *help)
{
    q_status_message4(SM_ORDER, 0, 2,
	  "Command \"%s\" not defined for this screen.%s%s%s",
		      cmd ? cmd : "?",
		      (help) ? " Use " : "",
		      (help) ?  help   : "",
		      (help) ? " for help" : "");
}


/*----------------------------------------------------------------------
   Execute FLAG message command

  Args: state --  Various satate info
        msgmap --  map of c-client to local message numbers

 Result: with side effect of "current" message FLAG flag set or UNset

 ----*/
int
cmd_flag(struct pine *state, MSGNO_S *msgmap, int aopt)
{
    char	  *flagit, *seq, *screen_text[20], **exp, **p, *answer = NULL;
    char          *keyword_array[2];
    int            user_defined_flags = 0, mailbox_flags = 0;
    int            directly_to_maint_screen = 0;
    long	   unflagged, flagged, flags, rawno;
    MESSAGECACHE  *mc = NULL;
    KEYWORD_S     *kw;
    int            i, cnt, is_set, trouble = 0, rv = 0;
    size_t         len;
    struct flag_table *fp, *ftbl = NULL;
    struct flag_screen flag_screen;
    static char   *flag_screen_text1[] = {
	N_("    Set desired flags for current message below.  An 'X' means set"),
	N_("    it, and a ' ' means to unset it.  Choose \"Exit\" when finished."),
	NULL
    };

    static char   *flag_screen_text2[] = {
	N_("    Set desired flags below for selected messages.  A '?' means to"),
	N_("    leave the flag unchanged, 'X' means to set it, and a ' ' means"),
	N_("    to unset it.  Use the \"Return\" key to toggle, and choose"),
	N_("    \"Exit\" when finished."),
	NULL
    };

    static struct  flag_table default_ftbl[] = {
	{N_("Important"), h_flag_important, F_FLAG, 0, 0, NULL, NULL},
	{N_("New"),	  h_flag_new, F_SEEN, 0, 0, NULL, NULL},
	{N_("Answered"),  h_flag_answered, F_ANS, 0, 0, NULL, NULL},
	{N_("Forwarded"),  h_flag_forwarded, F_FWD, 0, 0, NULL, NULL},
	{N_("Deleted"),   h_flag_deleted, F_DEL, 0, 0, NULL, NULL},
	{NULL, NO_HELP, 0, 0, 0, NULL, NULL}
    };

    /* Only check for dead stream for now.  Should check permanent flags
     * eventually
     */
    if(!(any_messages(msgmap, NULL, "to Flag") && can_set_flag(state, "flag", 1)))
      return rv;

    if(sp_io_error_on_stream(state->mail_stream)){
	sp_set_io_error_on_stream(state->mail_stream, 0);
	pine_mail_check(state->mail_stream);		/* forces write */
	return rv;
    }

go_again:
    answer = NULL;
    user_defined_flags = 0;
    mailbox_flags = 0;
    mc = NULL;
    trouble = 0;
    ftbl = NULL;

    /* count how large ftbl will be */
    for(cnt = 0; default_ftbl[cnt].name; cnt++)
      ;
    
    /* add user flags */
    for(kw = ps_global->keywords; kw; kw = kw->next){
	if(!((kw->nick && !strucmp(FORWARDED_FLAG, kw->nick)) || (kw->kw && !strucmp(FORWARDED_FLAG, kw->kw)))){
	    user_defined_flags++;
	    cnt++;
	}
    }

    /*
     * Add mailbox flags that aren't user-defined flags.
     * Don't consider it if it matches either one of our defined
     * keywords or one of our defined nicknames for a keyword.
     */
    for(i = 0; stream_to_user_flag_name(state->mail_stream, i); i++){
	char *q;

	q = stream_to_user_flag_name(state->mail_stream, i);
	if(q && q[0]){
	    for(kw = ps_global->keywords; kw; kw = kw->next){
		if((kw->nick && !strucmp(kw->nick, q)) || (kw->kw && !strucmp(kw->kw, q)))
		  break;
	    }
	}

	if(!kw && !(q && !strucmp(FORWARDED_FLAG, q))){
	    mailbox_flags++;
	    cnt++;
	}
    }
    
    cnt += (user_defined_flags ? 2 : 0) + (mailbox_flags ? 2 : 0);

    /* set up ftbl, first the system flags */
    ftbl = (struct flag_table *) fs_get((cnt+1) * sizeof(*ftbl));
    memset(ftbl, 0, (cnt+1) * sizeof(*ftbl));
    for(i = 0, fp = ftbl; default_ftbl[i].name; i++, fp++){
	fp->name = cpystr(default_ftbl[i].name);
	fp->help = default_ftbl[i].help;
	fp->flag = default_ftbl[i].flag;
	fp->set  = default_ftbl[i].set;
	fp->ukn  = default_ftbl[i].ukn;
    }

    if(user_defined_flags){
	fp->flag = F_COMMENT;
	fp->name = cpystr("");
	fp++;
	fp->flag = F_COMMENT;
	len = strlen(_("User-defined Keywords from Setup/Config"));
	fp->name = (char *) fs_get((len+6+6+1) * sizeof(char));
	snprintf(fp->name, len+6+6+1, "----- %s -----", _("User-defined Keywords from Setup/Config"));
	fp++;
    }

    /* then the user-defined keywords */
    if(user_defined_flags)
      for(kw = ps_global->keywords; kw; kw = kw->next){
	if(!((kw->nick && !strucmp(FORWARDED_FLAG, kw->nick))
	     || (kw->kw && !strucmp(FORWARDED_FLAG, kw->kw)))){
	    fp->name = cpystr(kw->nick ? kw->nick : kw->kw ? kw->kw : "");
	    fp->keyword = cpystr(kw->kw ? kw->kw : "");
	    if(kw->nick && kw->kw){
		size_t l;

		l = strlen(kw->kw)+2;
		fp->comment = (char *) fs_get((l+1) * sizeof(char));
		snprintf(fp->comment, l+1, "(%.*s)", strlen(kw->kw), kw->kw);
		fp->comment[l] = '\0';
	    }

	    fp->help = h_flag_user_flag;
	    fp->flag = F_KEYWORD;
	    fp->set  = 0;
	    fp->ukn  = 0;
	    fp++;
	}
      }

    if(mailbox_flags){
	fp->flag = F_COMMENT;
	fp->name = cpystr("");
	fp++;
	fp->flag = F_COMMENT;
	len = strlen(_("Other keywords in the mailbox that are not user-defined"));
	fp->name = (char *) fs_get((len+6+6+1) * sizeof(char));
	snprintf(fp->name, len+6+6+1, "----- %s -----", _("Other keywords in the mailbox that are not user-defined"));
	fp++;
    }

    /* then the extra mailbox-defined keywords */
    if(mailbox_flags)
      for(i = 0; stream_to_user_flag_name(state->mail_stream, i); i++){
	char *q;

	q = stream_to_user_flag_name(state->mail_stream, i);
	if(q && q[0]){
	    for(kw = ps_global->keywords; kw; kw = kw->next){
		if((kw->nick && !strucmp(kw->nick, q)) || (kw->kw && !strucmp(kw->kw, q)))
		  break;
	    }
	}

	if(!kw && !(q && !strucmp(FORWARDED_FLAG, q))){
	    fp->name = cpystr(q);
	    fp->keyword = cpystr(q);
	    fp->help = h_flag_user_flag;
	    fp->flag = F_KEYWORD;
	    fp->set  = 0;
	    fp->ukn  = 0;
	    fp++;
	}
      }

    flag_screen.flag_table  = &ftbl;
    flag_screen.explanation = screen_text;

    if(MCMD_ISAGG(aopt)){
	if(!pseudo_selected(ps_global->mail_stream, msgmap)){
	    free_flag_table(&ftbl);
	    return rv;
	}

	exp = flag_screen_text2;
	for(fp = ftbl; fp->name; fp++){
	    fp->set = CMD_FLAG_UNKN;		/* set to unknown */
	    fp->ukn = TRUE;
	}
    }
    else if(state->mail_stream
	    && (rawno = mn_m2raw(msgmap, mn_get_cur(msgmap))) > 0L
	    && rawno <= state->mail_stream->nmsgs
	    && (mc = mail_elt(state->mail_stream, rawno))){
	exp = flag_screen_text1;
	for(fp = &ftbl[0]; fp->name; fp++){
	    fp->ukn = 0;
	    if(fp->flag == F_KEYWORD){
		/* see if this keyword is defined for this message */
		fp->set = CMD_FLAG_CLEAR;
		if(user_flag_is_set(state->mail_stream,
				    rawno, fp->keyword))
		  fp->set = CMD_FLAG_SET;
	    }
	    else if(fp->flag == F_FWD){
		/* see if forwarded keyword is defined for this message */
		fp->set = CMD_FLAG_CLEAR;
		if(user_flag_is_set(state->mail_stream,
				    rawno, FORWARDED_FLAG))
		  fp->set = CMD_FLAG_SET;
	    }
	    else if(fp->flag != F_COMMENT)
	      fp->set = ((fp->flag == F_SEEN && !mc->seen)
		         || (fp->flag == F_DEL && mc->deleted)
		         || (fp->flag == F_FLAG && mc->flagged)
		         || (fp->flag == F_ANS && mc->answered))
			  ? CMD_FLAG_SET : CMD_FLAG_CLEAR;
	}
    }
    else{
	q_status_message(SM_ORDER | SM_DING, 3, 4,
			 _("Error accessing message data"));
	free_flag_table(&ftbl);
	return rv;
    }

    if(directly_to_maint_screen)
      goto the_maint_screen;

#ifdef _WINDOWS
    if (mswin_usedialog ()) {
	if (!os_flagmsgdialog (&ftbl[0])){
	    free_flag_table(&ftbl);
	    return rv;
	}
    }
    else
#endif	    
    {
	int use_maint_screen;
	int keyword_shortcut = 0;

	use_maint_screen = F_ON(F_FLAG_SCREEN_DFLT, ps_global);

	if(!use_maint_screen){
	    /*
	     * We're going to call cmd_flag_prompt(). We need
	     * to decide whether or not to offer the keyword setting
	     * shortcut. We'll offer it if the user has the feature
	     * enabled AND there are some possible keywords that could
	     * be set.
	     */
	    if(F_ON(F_FLAG_SCREEN_KW_SHORTCUT, ps_global)){
		for(fp = &ftbl[0]; fp->name && !keyword_shortcut; fp++){
		    if(fp->flag == F_KEYWORD){
			int first_char;
			ESCKEY_S *tp;

			first_char = (fp->name && fp->name[0])
					? fp->name[0] : -2;
			if(isascii(first_char) && isupper(first_char))
			  first_char = tolower((unsigned char) first_char);

			for(tp=flag_text_opt; tp->ch != -1; tp++)
			  if(tp->ch == first_char)
			    break;

			if(tp->ch == -1)
			  keyword_shortcut++;
		    }
		}
	    }

	    use_maint_screen = !cmd_flag_prompt(state, &flag_screen,
						keyword_shortcut);
	}

the_maint_screen:
	if(use_maint_screen){
	    for(p = &screen_text[0]; *exp; p++, exp++)
	      *p = *exp;

	    *p = NULL;

	    directly_to_maint_screen = flag_maintenance_screen(state, &flag_screen);
	}
    }

    /* reaquire the elt pointer */
    mc = (state->mail_stream
	  && (rawno = mn_m2raw(msgmap, mn_get_cur(msgmap))) > 0L
	  && rawno <= state->mail_stream->nmsgs)
	  ? mail_elt(state->mail_stream, rawno) : NULL;

    for(fp = ftbl; mc && fp->name; fp++){
	flags = -1;
	switch(fp->flag){
	  case F_SEEN:
	    if((!MCMD_ISAGG(aopt) && fp->set != !mc->seen)
	       || (MCMD_ISAGG(aopt) && fp->set != CMD_FLAG_UNKN)){
		flagit = "\\SEEN";
		if(fp->set){
		    flags     = 0L;
		    unflagged = F_SEEN;
		}
		else{
		    flags     = ST_SET;
		    unflagged = F_UNSEEN;
		}
	    }

	    break;

	  case F_ANS:
	    if((!MCMD_ISAGG(aopt) && fp->set != mc->answered)
	       || (MCMD_ISAGG(aopt) && fp->set != CMD_FLAG_UNKN)){
		flagit = "\\ANSWERED";
		if(fp->set){
		    flags     = ST_SET;
		    unflagged = F_UNANS;
		}
		else{
		    flags     = 0L;
		    unflagged = F_ANS;
		}
	    }

	    break;

	  case F_DEL:
	    if((!MCMD_ISAGG(aopt) && fp->set != mc->deleted)
	       || (MCMD_ISAGG(aopt) && fp->set != CMD_FLAG_UNKN)){
		flagit = "\\DELETED";
		if(fp->set){
		    flags     = ST_SET;
		    unflagged = F_UNDEL;
		}
		else{
		    flags     = 0L;
		    unflagged = F_DEL;
		}
	    }

	    break;

	  case F_FLAG:
	    if((!MCMD_ISAGG(aopt) && fp->set != mc->flagged)
	       || (MCMD_ISAGG(aopt) && fp->set != CMD_FLAG_UNKN)){
		flagit = "\\FLAGGED";
		if(fp->set){
		    flags     = ST_SET;
		    unflagged = F_UNFLAG;
		}
		else{
		    flags     = 0L;
		    unflagged = F_FLAG;
		}
	    }

	    break;

	  case F_FWD :
	    if(!MCMD_ISAGG(aopt)){
		/* see if forwarded is defined for this message */
		is_set = CMD_FLAG_CLEAR;
		if(user_flag_is_set(state->mail_stream,
				    mn_m2raw(msgmap, mn_get_cur(msgmap)),
				    FORWARDED_FLAG))
		  is_set = CMD_FLAG_SET;
	    }

	    if((!MCMD_ISAGG(aopt) && fp->set != is_set)
	       || (MCMD_ISAGG(aopt) && fp->set != CMD_FLAG_UNKN)){
		flagit = FORWARDED_FLAG;
		if(fp->set){
		    flags     = ST_SET;
		    unflagged = F_UNFWD;
		}
		else{
		    flags     = 0L;
		    unflagged = F_FWD;
		}
	    }

	    break;

	  case F_KEYWORD:
	    if(!MCMD_ISAGG(aopt)){
		/* see if this keyword is defined for this message */
		is_set = CMD_FLAG_CLEAR;
		if(user_flag_is_set(state->mail_stream,
				    mn_m2raw(msgmap, mn_get_cur(msgmap)),
				    fp->keyword))
		  is_set = CMD_FLAG_SET;
	    }

	    if((!MCMD_ISAGG(aopt) && fp->set != is_set)
	       || (MCMD_ISAGG(aopt) && fp->set != CMD_FLAG_UNKN)){
		flagit = fp->keyword;
		keyword_array[0] = fp->keyword;
		keyword_array[1] = NULL;
		if(fp->set){
		    flags     = ST_SET;
		    unflagged = F_UNKEYWORD;
		}
		else{
		    flags     = 0L;
		    unflagged = F_KEYWORD;
		}
	    }

	    break;

	  default:
	    break;
	}

	flagged = 0L;
	if(flags >= 0L
	   && (seq = currentf_sequence(state->mail_stream, msgmap,
				       unflagged, &flagged, unflagged & F_DEL,
				       (fp->flag == F_KEYWORD
				        && unflagged == F_KEYWORD)
					 ? keyword_array : NULL,
				       (fp->flag == F_KEYWORD
				        && unflagged == F_UNKEYWORD)
					 ? keyword_array : NULL))){
	    /*
	     * For user keywords, we may have to create the flag in
	     * the folder if it doesn't already exist and we are setting
	     * it (as opposed to clearing it). Mail_flag will
	     * do that for us, but it's failure isn't very friendly
	     * error-wise. So we try to make it a little smoother.
	     */
	    if(!(fp->flag == F_KEYWORD || fp->flag == F_FWD) || !fp->set
	       || ((i=user_flag_index(state->mail_stream, flagit)) >= 0
	           && i < NUSERFLAGS))
	      mail_flag(state->mail_stream, seq, flagit, flags);
	    else{
		/* trouble, see if we can add the user flag */
		if(state->mail_stream->kwd_create)
		  mail_flag(state->mail_stream, seq, flagit, flags);
		else{
		    trouble++;
		    
		    if(some_user_flags_defined(state->mail_stream))
		      q_status_message(SM_ORDER, 3, 4,
			       _("No more keywords allowed in this folder!"));
		    else if(fp->flag == F_FWD)
		      q_status_message(SM_ORDER, 3, 4,
				   _("Cannot add keywords for this folder so cannot set Forwarded flag"));
		    else
		      q_status_message(SM_ORDER, 3, 4,
				   _("Cannot add keywords for this folder"));
		}
	    }

	    fs_give((void **) &seq);
	    if(flagged && !trouble){
		snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%slagged%s%s%s%s%s message%s%s \"%s\"",
			(fp->set) ? "F" : "Unf",
			MCMD_ISAGG(aopt) ? " " : "",
			MCMD_ISAGG(aopt) ? long2string(flagged) : "",
			(MCMD_ISAGG(aopt) && flagged != mn_total_cur(msgmap))
			  ? " (of " : "",
			(MCMD_ISAGG(aopt) && flagged != mn_total_cur(msgmap))
			  ? comatose(mn_total_cur(msgmap)) : "",
			(MCMD_ISAGG(aopt) && flagged != mn_total_cur(msgmap))
			  ? ")" : "",
			MCMD_ISAGG(aopt) ? plural(flagged) : " ",
			MCMD_ISAGG(aopt) ? "" : long2string(mn_get_cur(msgmap)),
			fp->name);
		tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		q_status_message(SM_ORDER, 0, 2, answer = tmp_20k_buf);
		rv++;
	    }
	}
    }

    free_flag_table(&ftbl);

    if(directly_to_maint_screen)
      goto go_again;

    if(MCMD_ISAGG(aopt))
      restore_selected(msgmap);

    if(!answer)
      q_status_message(SM_ORDER, 0, 2, _("No flags changed."));

    return rv;
}


/*----------------------------------------------------------------------
   Offer concise status line flag prompt 

  Args: state --  Various satate info
        flags -- flags to offer setting

 Result: TRUE if flag to set specified in flags struct or FALSE otw

 ----*/
int
cmd_flag_prompt(struct pine *state, struct flag_screen *flags, int allow_keyword_shortcuts)
{
    int			r, setflag = 1, first_char;
    struct flag_table  *fp;
    ESCKEY_S           *ek;
    char               *ftext, *ftext_not;
    static char *flag_text =
  N_("Flag New, Deleted, Answered, Forwarded or Important ? ");
    static char *flag_text_ak =
  N_("Flag New, Deleted, Answered, Forwarded, Important or Keyword initial ? ");
    static char *flag_text_not =
  N_("Flag !New, !Deleted, !Answered, !Forwarded, or !Important ? ");
    static char *flag_text_ak_not =
  N_("Flag !New, !Deleted, !Answered, !Forwarded, !Important or !Keyword initial ? ");

    if(allow_keyword_shortcuts){
	int       cnt = 0;
	ESCKEY_S *dp, *sp, *tp;

	for(sp=flag_text_opt; sp->ch != -1; sp++)
	  cnt++;

	for(fp=(flags->flag_table ? *flags->flag_table : NULL); fp->name; fp++)
	  if(fp->flag == F_KEYWORD)
	    cnt++;

	/* set up an ESCKEY_S list which includes invisible keys for keywords */
	ek = (ESCKEY_S *) fs_get((cnt + 1) * sizeof(*ek));
	memset(ek, 0, (cnt+1) * sizeof(*ek));
	for(dp=ek, sp=flag_text_opt; sp->ch != -1; sp++, dp++)
	  *dp = *sp;

	for(fp=(flags->flag_table ? *flags->flag_table : NULL); fp->name; fp++){
	    if(fp->flag == F_KEYWORD){
		first_char = (fp->name && fp->name[0]) ? fp->name[0] : -2;
		if(isascii(first_char) && isupper(first_char))
		  first_char = tolower((unsigned char) first_char);

		/*
		 * Check to see if an earlier keyword in the list, or one of
		 * the builtin system letters already uses this character.
		 * If so, the first one wins.
		 */
		for(tp=ek; tp->ch != 0; tp++)
		  if(tp->ch == first_char)
		    break;

		if(tp->ch != 0)
		  continue;		/* skip it, already used that char */

		dp->ch    = first_char;
		dp->rval  = first_char;
		dp->name  = "";
		dp->label = "";
		dp++;
	    }
	}

	dp->ch = -1;
	ftext = _(flag_text_ak);
	ftext_not = _(flag_text_ak_not);
    }
    else{
	ek = flag_text_opt;
	ftext = _(flag_text);
	ftext_not = _(flag_text_not);
    }

    while(1){
	r = radio_buttons(setflag ? ftext : ftext_not,
			  -FOOTER_ROWS(state), ek, '*', SEQ_EXCEPTION-1,
			  NO_HELP, RB_NORM | RB_SEQ_SENSITIVE);
	/*
	 * It is SEQ_EXCEPTION-1 just so that it is some value that isn't
	 * being used otherwise. The keywords use up all the possible
	 * letters, so a negative number is good, but it has to be different
	 * from other negative return values.
	 */
	if(r == SEQ_EXCEPTION-1)	/* ol'cancelrooney */
	  return(TRUE);
	else if(r == 10)		/* return and goto flag screen */
	  return(FALSE);
	else if(r == '!')		/* flip intention */
	  setflag = !setflag;
	else
	  break;
    }

    for(fp = (flags->flag_table ? *flags->flag_table : NULL); fp->name; fp++){
	if(r == 'n' || r == '*' || r == 'd' || r == 'a' || r == 'f'){
	    if((r == 'n' && fp->flag == F_SEEN)
	       || (r == '*' && fp->flag == F_FLAG)
	       || (r == 'd' && fp->flag == F_DEL)
	       || (r == 'f' && fp->flag == F_FWD)
	       || (r == 'a' && fp->flag == F_ANS)){
		fp->set = setflag ? CMD_FLAG_SET : CMD_FLAG_CLEAR;
		break;
	    }
	}
	else if(allow_keyword_shortcuts && fp->flag == F_KEYWORD){
	    first_char = (fp->name && fp->name[0]) ? fp->name[0] : -2;
	    if(isascii(first_char) && isupper(first_char))
	      first_char = tolower((unsigned char) first_char);

	    if(r == first_char){
		fp->set = setflag ? CMD_FLAG_SET : CMD_FLAG_CLEAR;
		break;
	    }
	}
    }

    if(ek != flag_text_opt)
      fs_give((void **) &ek);

    return(TRUE);
}


/*
 * (*ft) is an array of flag_table entries.
 */
void
free_flag_table(struct flag_table **ft)
{
    struct flag_table *fp;

    if(ft && *ft){
	for(fp = (*ft); fp->name; fp++){
	    if(fp->name)
	      fs_give((void **) &fp->name);
	    
	    if(fp->keyword)
	      fs_give((void **) &fp->keyword);
	    
	    if(fp->comment)
	      fs_give((void **) &fp->comment);
	}
	
	fs_give((void **) ft);
    }
}


/*----------------------------------------------------------------------
   Execute REPLY message command

  Args: state --  Various satate info
        msgmap --  map of c-client to local message numbers

 Result: reply sent or not

 ----*/
int
cmd_reply(struct pine *state, MSGNO_S *msgmap, int aopt)
{
    int rv = 0;

    if(any_messages(msgmap, NULL, "to Reply to")){
	if(MCMD_ISAGG(aopt) && !pseudo_selected(state->mail_stream, msgmap))
	  return rv;

	rv = reply(state, NULL);

	if(MCMD_ISAGG(aopt))
	  restore_selected(msgmap);

	state->mangled_screen = 1;
    }

    return rv;
}


/*----------------------------------------------------------------------
   Execute FORWARD message command

  Args: state --  Various satate info
        msgmap --  map of c-client to local message numbers

 Result: selected message[s] forwarded or not

 ----*/
int
cmd_forward(struct pine *state, MSGNO_S *msgmap, int aopt)
{
    int rv = 0;

    if(any_messages(msgmap, NULL, "to Forward")){
	if(MCMD_ISAGG(aopt) && !pseudo_selected(state->mail_stream, msgmap))
	  return rv;

	rv = forward(state, NULL);

	if(MCMD_ISAGG(aopt))
	  restore_selected(msgmap);

	state->mangled_screen = 1;
    }

    return rv;
}


/*----------------------------------------------------------------------
   Execute BOUNCE message command

  Args:  state --  Various satate info
        msgmap --  map of c-client to local message numbers
	  aopt --  aggregate options

 Result: selected message[s] bounced or not

 ----*/
int
cmd_bounce(struct pine *state, MSGNO_S *msgmap, int aopt)
{
    int rv = 0;

    if(any_messages(msgmap, NULL, "to Bounce")){
	if(MCMD_ISAGG(aopt) && !pseudo_selected(state->mail_stream, msgmap))
	  return rv;

	rv = bounce(state, NULL);
	if(MCMD_ISAGG(aopt))
	  restore_selected(msgmap);

	state->mangled_footer = 1;
    }

    return rv;
}


/*----------------------------------------------------------------------
   Execute save message command: prompt for folder and call function to save

  Args: screen_line    --  Line on the screen to prompt on
        message        --  The MESSAGECACHE entry of message to save

 Result: The folder lister can be called to make selection; mangled screen set

   This does the prompting for the folder name to save to, possibly calling 
 up the folder display for selection of folder by user.                 
 ----*/
int
cmd_save(struct pine *state, MAILSTREAM *stream, MSGNO_S *msgmap, int aopt, CmdWhere in_index)
{
    char	      newfolder[MAILTMPLEN], nmsgs[32], *nick;
    int		      we_cancel = 0, rv = 0, save_flags;
    long	      i, raw;
    CONTEXT_S	     *cntxt = NULL;
    ENVELOPE	     *e = NULL;
    SaveDel           del = DontAsk;
    SavePreserveOrder pre = DontAskPreserve;

    dprint((4, "\n - saving message -\n"));

    state->ugly_consider_advancing_bit = 0;
    if(F_OFF(F_SAVE_PARTIAL_WO_CONFIRM, state)
       && msgno_any_deletedparts(stream, msgmap)
       && want_to(_("Saved copy will NOT include entire message!  Continue"),
		  'y', 'n', NO_HELP, WT_FLUSH_IN | WT_SEQ_SENSITIVE) != 'y'){
	cmd_cancelled("Save message");
	return rv;
    }

    if(MCMD_ISAGG(aopt) && !pseudo_selected(stream, msgmap))
      return rv;

    raw = mn_m2raw(msgmap, mn_get_cur(msgmap));

    if(mn_total_cur(msgmap) <= 1L){
	snprintf(nmsgs, sizeof(nmsgs), "Msg #%ld ", mn_get_cur(msgmap));
	nmsgs[sizeof(nmsgs)-1] = '\0';
	e = pine_mail_fetchstructure(stream, raw, NULL);
	if(!e) {
	    q_status_message(SM_ORDER | SM_DING, 3, 4,
			     _("Can't save message.  Error accessing folder"));
	    restore_selected(msgmap);
	    return rv;
	}
    }
    else{
	snprintf(nmsgs, sizeof(nmsgs), "%s msgs ", comatose(mn_total_cur(msgmap)));
	nmsgs[sizeof(nmsgs)-1] = '\0';

	/* e is just used to get a default save folder from the first msg */
	e = pine_mail_fetchstructure(stream,
				     mn_m2raw(msgmap, mn_first_cur(msgmap)),
				     NULL);
    }

    del = (!READONLY_FOLDER(stream) && F_OFF(F_SAVE_WONT_DELETE, ps_global))
	     ? Del : NoDel;
    if(mn_total_cur(msgmap) > 1L)
      pre = F_OFF(F_AGG_SEQ_COPY, ps_global) ? Preserve : NoPreserve;
    else
      pre = DontAskPreserve;

    if(save_prompt(state, &cntxt, newfolder, sizeof(newfolder), nmsgs, e,
		   raw, NULL, &del, &pre)){

	if(ps_global && ps_global->ttyo){
	    blank_keymenu(ps_global->ttyo->screen_rows - 2, 0);
	    ps_global->mangled_footer = 1;
	}

	save_flags = SV_FIX_DELS;
	if(pre == RetPreserve)
	  save_flags |= SV_PRESERVE;
	if(del == RetDel)
	  save_flags |= SV_DELETE;
	if(ps_global->context_list == cntxt && !strucmp(newfolder, ps_global->inbox_name))
	  save_flags |= SV_INBOXWOCNTXT;

	we_cancel = busy_cue(_("Saving"), NULL, 1);
	i = save(state, stream, cntxt, newfolder, msgmap, save_flags);
	if(we_cancel)
	  cancel_busy_cue(0);

	if(i == mn_total_cur(msgmap)){
	    rv++;
	    if(mn_total_cur(msgmap) <= 1L){
		int need, avail = ps_global->ttyo->screen_cols - 2;
		int lennick, lenfldr;

		if(cntxt
		   && ps_global->context_list->next
		   && context_isambig(newfolder)){
		    lennick = MIN(strlen(cntxt->nickname), 500);
		    lenfldr = MIN(strlen(newfolder), 500);
		    need = 27 + strlen(long2string(mn_get_cur(msgmap))) +
			   lenfldr + lennick;
		    if(need > avail){
			if(lennick > 10){
			    need -= MIN(lennick-10, need-avail);
			    lennick -= MIN(lennick-10, need-avail);
			}

			if(need > avail && lenfldr > 10)
			  lenfldr -= MIN(lenfldr-10, need-avail);
		    }

		    snprintf(tmp_20k_buf, SIZEOF_20KBUF,
			    "Message %s copied to \"%s\" in <%s>",
			    long2string(mn_get_cur(msgmap)),
			    short_str(newfolder, (char *)(tmp_20k_buf+1000), 1000,
				      lenfldr, MidDots),
			    short_str(cntxt->nickname,
				      (char *)(tmp_20k_buf+2000), 1000,
				      lennick, EndDots));
		    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		}
		else if((nick=folder_is_target_of_nick(newfolder, cntxt)) != NULL){
		    snprintf(tmp_20k_buf, SIZEOF_20KBUF,
			    "Message %s copied to \"%s\"",
			    long2string(mn_get_cur(msgmap)),
			    nick);
		    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		}
		else{
		    char *f = " folder";

		    lenfldr = MIN(strlen(newfolder), 500);
		    need = 28 + strlen(long2string(mn_get_cur(msgmap))) +
			   lenfldr;
		    if(need > avail){
			need -= strlen(f);
			f = "";
			if(need > avail && lenfldr > 10)
			  lenfldr -= MIN(lenfldr-10, need-avail);
		    }

		    snprintf(tmp_20k_buf,SIZEOF_20KBUF,
			    "Message %s copied to%s \"%s\"",
			    long2string(mn_get_cur(msgmap)), f,
			    short_str(newfolder, (char *)(tmp_20k_buf+1000), 1000,
				      lenfldr, MidDots));
		    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		}
	    }
	    else{
		snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%s messages saved",
		      comatose(mn_total_cur(msgmap)));
		tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	    }

	    if(del == RetDel){
		strncat(tmp_20k_buf, " and deleted", SIZEOF_20KBUF-strlen(tmp_20k_buf)-1);
		tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	    }

	    q_status_message(SM_ORDER, 0, 3, tmp_20k_buf);

	    if(!MCMD_ISAGG(aopt) && F_ON(F_SAVE_ADVANCES, state)){
		if(sp_new_mail_count(stream))
		  process_filter_patterns(stream, msgmap,
					  sp_new_mail_count(stream));

		mn_inc_cur(stream, msgmap,
			   (in_index == View && THREADING()
			    && sp_viewing_a_thread(stream))
			     ? MH_THISTHD
			     : (in_index == View)
			       ? MH_ANYTHD : MH_NONE);
	    }

	    state->ugly_consider_advancing_bit = 1;
	}
    }

    if(MCMD_ISAGG(aopt))			/* straighten out fakes */
      restore_selected(msgmap);

    if(del == RetDel)
      update_titlebar_status();			/* make sure they see change */

    return rv;
}


void
role_compose(struct pine *state)
{
    int action;

    if(F_ON(F_ALT_ROLE_MENU, state) && mn_get_total(state->msgmap) > 0L){
	PAT_STATE  pstate;

	if(nonempty_patterns(ROLE_DO_ROLES, &pstate) && first_pattern(&pstate)){
	    action = radio_buttons(_("Compose, Forward, Reply, or Bounce? "),
				   -FOOTER_ROWS(state), choose_action,
				   'c', 'x', h_role_compose, RB_NORM);
	}
	else{
	    q_status_message(SM_ORDER, 0, 3,
			 _("No roles available. Use Setup/Rules to add roles."));
	    return;
	}
    }
    else
      action = 'c';

    if(action == 'c' || action == 'r' || action == 'f' || action == 'b'){
	ACTION_S *role = NULL;
	void    (*prev_screen)(struct pine *) = NULL, (*redraw)(void) = NULL;

	redraw = state->redrawer;
	state->redrawer = NULL;
	prev_screen = state->prev_screen;
	role = NULL;
	state->next_screen = SCREEN_FUN_NULL;

	/* Setup role */
	if(role_select_screen(state, &role,
			      action == 'f' ? MC_FORWARD :
			       action == 'r' ? MC_REPLY :
			        action == 'b' ? MC_BOUNCE :
				 action == 'c' ? MC_COMPOSE : 0) < 0){
	    cmd_cancelled(action == 'f' ? _("Forward") :
			  action == 'r' ? _("Reply") :
			   action == 'c' ? _("Composition") : _("Bounce"));
	    state->next_screen = prev_screen;
	    state->redrawer = redraw;
	    state->mangled_screen = 1;
	}
	else{
	    /*
	     * If default role was selected (NULL) we need to make
	     * up a role which won't do anything, but will cause
	     * compose_mail to think there's already a role so that
	     * it won't try to confirm the default.
	     */
	    if(role)
	      role = combine_inherited_role(role);
	    else{
		role = (ACTION_S *) fs_get(sizeof(*role));
		memset((void *) role, 0, sizeof(*role));
		role->nick = cpystr("Default Role");
	    }

	    state->redrawer = NULL;
	    switch(action){
	      case 'c':
		compose_mail(NULL, NULL, role, NULL, NULL);
		break;

	      case 'r':
		(void) reply(state, role);
		break;

	      case 'f':
		(void) forward(state, role);
		break;

	      case 'b':
		(void) bounce(state, role);
		break;
	    }

	    if(role)
	      free_action(&role);
	    
	    state->next_screen = prev_screen;
	    state->redrawer = redraw;
	    state->mangled_screen = 1;
	}
    }
}


/*----------------------------------------------------------------------
   Do the dirty work of prompting the user for a folder name

  Args: 
        nfldr should be a buffer at least MAILTMPLEN long
	dela -- a pointer to a SaveDel. If it is
	  DontAsk on input, don't offer Delete prompt
	  Del     on input, offer Delete command with default of Delete
	  NoDel                                                NoDelete
	  RetDel and RetNoDel are return values
        

 Result: 

 ----*/
int
save_prompt(struct pine *state, CONTEXT_S **cntxt, char *nfldr, size_t len_nfldr,
	    char *nmsgs, ENVELOPE *env, long int rawmsgno, char *section,
	    SaveDel *dela, SavePreserveOrder *prea)
{
    int		      rc, ku = -1, n, flags, last_rc = 0, saveable_count = 0, done = 0;
    int		      delindex, preindex, r;
    char	      prompt[6*MAX_SCREEN_COLS+1], *p, expanded[MAILTMPLEN];
    char              *buf = tmp_20k_buf;
    char              shortbuf[200];
    char              *folder;
    HelpType	      help;
    SaveDel           del = DontAsk;
    SavePreserveOrder pre = DontAskPreserve;
    char             *deltext = NULL;
    static HISTORY_S *history = NULL;
    CONTEXT_S	     *tc;
    ESCKEY_S	      ekey[10];

    if(!cntxt)
      panic("no context ptr in save_prompt");

    init_hist(&history, HISTSIZE);

    if(!(folder = save_get_default(state, env, rawmsgno, section, cntxt)))
      return(0);		/* message expunged! */

    /* how many context's can be saved to... */
    for(tc = state->context_list; tc; tc = tc->next)
      if(!NEWS_TEST(tc))
        saveable_count++;

    /* set up extra command option keys */
    rc = 0;
    ekey[rc].ch      = ctrl('T');
    ekey[rc].rval    = 2;
    ekey[rc].name    = "^T";
    /* TRANSLATORS: command means go to Folders list */
    ekey[rc++].label = N_("To Fldrs");

    if(saveable_count > 1){
	ekey[rc].ch      = ctrl('P');
	ekey[rc].rval    = 10;
	ekey[rc].name    = "^P";
	ekey[rc++].label = N_("Prev Collection");

	ekey[rc].ch      = ctrl('N');
	ekey[rc].rval    = 11;
	ekey[rc].name    = "^N";
	ekey[rc++].label = N_("Next Collection");
    }

    if(F_ON(F_ENABLE_TAB_COMPLETE, ps_global)){
	ekey[rc].ch      = TAB;
	ekey[rc].rval    = 12;
	ekey[rc].name    = "TAB";
	/* TRANSLATORS: command asks alpine to complete the name when tab is typed */
	ekey[rc++].label = N_("Complete");
    }

    if(F_ON(F_ENABLE_SUB_LISTS, ps_global)){
	ekey[rc].ch      = ctrl('X');
	ekey[rc].rval    = 14;
	ekey[rc].name    = "^X";
	/* TRANSLATORS: list all the matches */
	ekey[rc++].label = N_("ListMatches");
    }

    if(dela && (*dela == NoDel || *dela == Del)){
	ekey[rc].ch      = ctrl('R');
	ekey[rc].rval    = 15;
	ekey[rc].name    = "^R";
	delindex = rc++;
	del = *dela;
    }

    if(prea && (*prea == NoPreserve || *prea == Preserve)){
	ekey[rc].ch      = ctrl('W');
	ekey[rc].rval    = 16;
	ekey[rc].name    = "^W";
	preindex = rc++;
	pre = *prea;
    }

    if(saveable_count > 1 && F_ON(F_DISABLE_SAVE_INPUT_HISTORY, ps_global)){
	ekey[rc].ch      = KEY_UP;
	ekey[rc].rval    = 10;
	ekey[rc].name    = "";
	ekey[rc++].label = "";

	ekey[rc].ch      = KEY_DOWN;
	ekey[rc].rval    = 11;
	ekey[rc].name    = "";
	ekey[rc++].label = "";
    }
    else if(F_OFF(F_DISABLE_SAVE_INPUT_HISTORY, ps_global)){
	ekey[rc].ch      = KEY_UP;
	ekey[rc].rval    = 30;
	ekey[rc].name    = "";
	ku = rc;
	ekey[rc++].label = "";

	ekey[rc].ch      = KEY_DOWN;
	ekey[rc].rval    = 31;
	ekey[rc].name    = "";
	ekey[rc++].label = "";
    }

    ekey[rc].ch = -1;

    *nfldr = '\0';
    help = NO_HELP;
    while(!done){
	/* only show collection number if more than one available */
	if(ps_global->context_list->next)
	  snprintf(prompt, sizeof(prompt), "SAVE%s %sto folder in <%s> [%s] : ",
		  deltext ? deltext : "",
		  nmsgs,
		  short_str((*cntxt)->nickname, shortbuf, sizeof(shortbuf), 16, EndDots),
		  strsquish(buf, SIZEOF_20KBUF, folder, 25));
	else
	  snprintf(prompt, sizeof(prompt), "SAVE%s %sto folder [%s] : ",
		  deltext ? deltext : "",
		  nmsgs, strsquish(buf, SIZEOF_20KBUF, folder, 40));

	prompt[sizeof(prompt)-1] = '\0';

	/*
	 * If the prompt won't fit, try removing deltext.
	 */
	if(state->ttyo->screen_cols < strlen(prompt) + MIN_OPT_ENT_WIDTH && deltext){
	    if(ps_global->context_list->next)
	      snprintf(prompt, sizeof(prompt), "SAVE %sto folder in <%s> [%s] : ",
		      nmsgs,
		      short_str((*cntxt)->nickname, shortbuf, sizeof(shortbuf), 16, EndDots),
		      strsquish(buf, SIZEOF_20KBUF, folder, 25));
	    else
	      snprintf(prompt, sizeof(prompt), "SAVE %sto folder [%s] : ",
		      nmsgs, strsquish(buf, SIZEOF_20KBUF, folder, 40));

	    prompt[sizeof(prompt)-1] = '\0';
	}

	/*
	 * If the prompt still won't fit, remove the extra info contained
	 * in nmsgs.
	 */
	if(state->ttyo->screen_cols < strlen(prompt) + MIN_OPT_ENT_WIDTH && *nmsgs){
	    if(ps_global->context_list->next)
	      snprintf(prompt, sizeof(prompt), "SAVE to folder in <%s> [%s] : ",
		      short_str((*cntxt)->nickname, shortbuf, sizeof(shortbuf), 16, EndDots),
		      strsquish(buf, SIZEOF_20KBUF, folder, 25));
	    else
	      snprintf(prompt, sizeof(prompt), "SAVE to folder [%s] : ", 
		      strsquish(buf, SIZEOF_20KBUF, folder, 25));

	    prompt[sizeof(prompt)-1] = '\0';
	}
	
	if(del != DontAsk)
	  ekey[delindex].label = (del == NoDel) ? "Delete" : "No Delete";

	if(pre != DontAskPreserve)
	  ekey[preindex].label = (pre == NoPreserve) ? "Preserve Order" : "Any Order";

	if(ku >= 0){
	    if(items_in_hist(history) > 1){
		ekey[ku].name  = HISTORY_UP_KEYNAME;
		ekey[ku].label = HISTORY_KEYLABEL;
		ekey[ku+1].name  = HISTORY_DOWN_KEYNAME;
		ekey[ku+1].label = HISTORY_KEYLABEL;
	    }
	    else{
		ekey[ku].name  = "";
		ekey[ku].label = "";
		ekey[ku+1].name  = "";
		ekey[ku+1].label = "";
	    }
	}

	flags = OE_APPEND_CURRENT | OE_SEQ_SENSITIVE;
	rc = optionally_enter(nfldr, -FOOTER_ROWS(state), 0, len_nfldr,
			      prompt, ekey, help, &flags);

	switch(rc){
	  case -1 :
	    q_status_message(SM_ORDER | SM_DING, 3, 3,
			     _("Error reading folder name"));
	    done--;
	    break;

	  case 0 :
	    removing_trailing_white_space(nfldr);
	    removing_leading_white_space(nfldr);

	    if(*nfldr || *folder){
		char *p, *name, *fullname = NULL;
		int   exists, breakout = FALSE;

		if(!*nfldr){
		    strncpy(nfldr, folder, len_nfldr-1);
		    nfldr[len_nfldr-1] = '\0';
		}

		save_hist(history, nfldr, 0, (void *) *cntxt);

		if(!(name = folder_is_nick(nfldr, FOLDERS(*cntxt), 0)))
		    name = nfldr;

		if(update_folder_spec(expanded, sizeof(expanded), name)){
		    strncpy(name = nfldr, expanded, len_nfldr-1);
		    nfldr[len_nfldr-1] = '\0';
		}

		exists = folder_name_exists(*cntxt, name, &fullname);

		if(exists == FEX_ERROR){
		    q_status_message1(SM_ORDER, 0, 3,
				      _("Problem accessing folder \"%s\""),
				      nfldr);
		    done--;
		}
		else{
		    if(fullname){
			strncpy(name = nfldr, fullname, len_nfldr-1);
			nfldr[len_nfldr-1] = '\0';
			fs_give((void **) &fullname);
			breakout = TRUE;
		    }

		    if(exists & FEX_ISFILE){
			done++;
		    }
		    else if((exists & FEX_ISDIR)){
			char	   tmp[MAILTMPLEN];

			tc = *cntxt;
			if(breakout){
			    CONTEXT_S *fake_context;
			    size_t	   l;

			    strncpy(tmp, name, sizeof(tmp));
			    tmp[sizeof(tmp)-2-1] = '\0';
			    if(tmp[(l = strlen(tmp)) - 1] != tc->dir->delim){
				if(l < sizeof(tmp)){
				    tmp[l] = tc->dir->delim;
				    strncpy(&tmp[l+1], "[]", sizeof(tmp)-(l+1));
				}
			    }
			    else
			      strncat(tmp, "[]", sizeof(tmp)-strlen(tmp)-1);

			    tmp[sizeof(tmp)-1] = '\0';

			    fake_context = new_context(tmp, 0);
			    nfldr[0] = '\0';
			    done = display_folder_list(&fake_context, nfldr,
						       1, folders_for_save);
			    free_context(&fake_context);
			}
			else if(tc->dir->delim
				&& (p = strrindex(name, tc->dir->delim))
				&& *(p+1) == '\0')
			  done = display_folder_list(cntxt, nfldr,
						     1, folders_for_save);
			else{
			    q_status_message1(SM_ORDER, 3, 3,
				      _("\"%s\" is a directory"), name);
			    if(tc->dir->delim
			       && !((p=strrindex(name, tc->dir->delim)) && *(p+1) == '\0')){
				strncpy(tmp, name, sizeof(tmp));
				tmp[sizeof(tmp)-1] = '\0';
				snprintf(nfldr, len_nfldr, "%s%c", tmp, tc->dir->delim);
			    }
			}
		    }
		    else{			/* Doesn't exist, create! */
			if((fullname = folder_as_breakout(*cntxt, name)) != NULL){
			    strncpy(name = nfldr, fullname, len_nfldr-1);
			    nfldr[len_nfldr-1] = '\0';
			    fs_give((void **) &fullname);
			}

			switch(create_for_save(*cntxt, name)){
			  case 1 :		/* success */
			    done++;
			    break;
			  case 0 :		/* error */
			  case -1 :		/* declined */
			    done--;
			    break;
			}
		    }
		}

		break;
	    }
	    /* else fall thru like they cancelled */

	  case 1 :
	    cmd_cancelled("Save message");
	    done--;
	    break;

	  case 2 :
	    r = display_folder_list(cntxt, nfldr, 0, folders_for_save);

	    if(r)
	      done++;

	    break;

	  case 3 :
	    helper(h_save, _("HELP FOR SAVE"), HLPD_SIMPLE);
	    ps_global->mangled_screen = 1;
	    break;

	  case 4 :				/* redraw */
	    break;

	  case 10 :				/* previous collection */
	    for(tc = (*cntxt)->prev; tc; tc = tc->prev)
	      if(!NEWS_TEST(tc))
		break;

	    if(!tc){
		CONTEXT_S *tc2;

		for(tc2 = (tc = (*cntxt))->next; tc2; tc2 = tc2->next)
		  if(!NEWS_TEST(tc2))
		    tc = tc2;
	    }

	    *cntxt = tc;
	    break;

	  case 11 :				/* next collection */
	    tc = (*cntxt);

	    do
	      if(((*cntxt) = (*cntxt)->next) == NULL)
		(*cntxt) = ps_global->context_list;
	    while(NEWS_TEST(*cntxt) && (*cntxt) != tc);
	    break;

	  case 12 :				/* file name completion */
	    if(!folder_complete(*cntxt, nfldr, len_nfldr, &n)){
		if(n && last_rc == 12 && !(flags & OE_USER_MODIFIED)){
		    r = display_folder_list(cntxt, nfldr, 1, folders_for_save);
		    if(r)
		      done++;			/* bingo! */
		    else
		      rc = 0;			/* burn last_rc */
		}
		else
		  Writechar(BELL, 0);
	    }

	    break;

	  case 14 :				/* file name completion */
	    r = display_folder_list(cntxt, nfldr, 2, folders_for_save);
	    if(r)
	      done++;			/* bingo! */
	    else
	      rc = 0;			/* burn last_rc */

	    break;

	  case 15 :			/* Delete / No Delete */
	    del = (del == NoDel) ? Del : NoDel;
	    deltext = (del == NoDel) ? " (no delete)" : " (and delete)";
	    break;

	  case 16 :			/* Preserve Order or not */
	    pre = (pre == NoPreserve) ? Preserve : NoPreserve;
	    break;

	  case 30 :
	    if((p = get_prev_hist(history, nfldr, 0, (void *) *cntxt)) != NULL){
		strncpy(nfldr, p, len_nfldr);
		nfldr[len_nfldr-1] = '\0';
		if(history->hist[history->curindex])
		  *cntxt = (CONTEXT_S *) history->hist[history->curindex]->cntxt;
	    }
	    else
	      Writechar(BELL, 0);

	    break;

	  case 31 :
	    if((p = get_next_hist(history, nfldr, 0, (void *) *cntxt)) != NULL){
		strncpy(nfldr, p, len_nfldr);
		nfldr[len_nfldr-1] = '\0';
		if(history->hist[history->curindex])
		  *cntxt = (CONTEXT_S *) history->hist[history->curindex]->cntxt;
	    }
	    else
	      Writechar(BELL, 0);

	    break;

	  default :
	    panic("Unhandled case");
	    break;
	}

	last_rc = rc;
    }

    ps_global->mangled_footer = 1;

    if(done < 0)
      return(0);

    if(*nfldr){
	strncpy(ps_global->last_save_folder, nfldr, sizeof(ps_global->last_save_folder)-1);
	ps_global->last_save_folder[sizeof(ps_global->last_save_folder)-1] = '\0';
	if(*cntxt)
	  ps_global->last_save_context = *cntxt;
    }
    else{
	strncpy(nfldr, folder, len_nfldr-1);
	nfldr[len_nfldr-1] = '\0';
    }

    /* nickname?  Copy real name to nfldr */
    if(*cntxt
       && context_isambig(nfldr)
       && (p = folder_is_nick(nfldr, FOLDERS(*cntxt), 0))){
	strncpy(nfldr, p, len_nfldr-1);
	nfldr[len_nfldr-1] = '\0';
    }

    if(dela && (*dela == NoDel || *dela == Del))
      *dela = (del == NoDel) ? RetNoDel : RetDel;

    if(prea && (*prea == NoPreserve || *prea == Preserve))
      *prea = (pre == NoPreserve) ? RetNoPreserve : RetPreserve;

    return(1);
}


/*----------------------------------------------------------------------
   Prompt user before implicitly creating a folder for saving

  Args: context - context to create folder in
	folder  - folder name to create

 Result: 1 on proceed, -1 on decline, 0 on error

 ----*/
int
create_for_save_prompt(CONTEXT_S *context, char *folder, int sequence_sensitive)
{
    if(context && ps_global->context_list->next && context_isambig(folder)){
	if(context->use & CNTXT_INCMNG){
	    snprintf(tmp_20k_buf,SIZEOF_20KBUF,
		     _("\"%.15s%s\" doesn't exist - Add it in FOLDER LIST screen"),
		     folder, (strlen(folder) > 15) ? "..." : "");
	    q_status_message(SM_ORDER, 3, 3, tmp_20k_buf);
	    return(0);		/* error */
	}

	snprintf(tmp_20k_buf,SIZEOF_20KBUF,
		 _("Folder \"%.15s%s\" in <%.15s%s> doesn't exist. Create"),
		 folder, (strlen(folder) > 15) ? "..." : "",
		 context->nickname,
		 (strlen(context->nickname) > 15) ? "..." : "");
    }
    else
      snprintf(tmp_20k_buf, SIZEOF_20KBUF,
	       _("Folder \"%.40s%s\" doesn't exist.  Create"),
	       folder, strlen(folder) > 40 ? "..." : "");

    if(want_to(tmp_20k_buf, 'y', 'n',
	       NO_HELP, (sequence_sensitive) ? WT_SEQ_SENSITIVE : WT_NORM) != 'y'){
	cmd_cancelled("Save message");
	return(-1);
    }

    return(1);
}



/*----------------------------------------------------------------------
    Expunge messages from current folder

    Args: state -- pointer to struct holding a bunch of pine state
	 msgmap -- table mapping msg nums to c-client sequence nums
	  qline -- screen line to ask questions on
	    agg -- boolean indicating we're to operate on aggregate set

 Result: 
 ----*/
void
cmd_expunge(struct pine *state, MAILSTREAM *stream, MSGNO_S *msgmap)
{
    long del_count, prefilter_del_count;
    int we_cancel = 0;
    char prompt[MAX_SCREEN_COLS+1];
    COLOR_PAIR   *lastc = NULL;

    dprint((2, "\n - expunge -\n"));

    if(IS_NEWS(stream) && stream->rdonly){
	if((del_count = count_flagged(stream, F_DEL)) > 0L){
	    state->mangled_footer = 1;
	    snprintf(prompt, sizeof(prompt), "Exclude %ld message%s from %.*s", del_count,
		    plural(del_count), sizeof(prompt)-40,
		    pretty_fn(state->cur_folder));
	    prompt[sizeof(prompt)-1] = '\0';
	    if(F_ON(F_FULL_AUTO_EXPUNGE, state)
	       || (F_ON(F_AUTO_EXPUNGE, state)
		   && (state->context_current
		       && (state->context_current->use & CNTXT_INCMNG))
		   && context_isambig(state->cur_folder))
	       || want_to(prompt, 'y', 0, NO_HELP, WT_NORM) == 'y'){

		if(F_ON(F_NEWS_CROSS_DELETE, state))
		  cross_delete_crossposts(stream);

		msgno_exclude_deleted(stream, msgmap);
		clear_index_cache(stream, 0);

		/*
		 * This is kind of surprising at first. For most sort
		 * orders, if the whole set is sorted, then any subset
		 * is also sorted. Not so for threaded sorts.
		 */
		if(SORT_IS_THREADED(msgmap))
		  refresh_sort(stream, msgmap, SRT_NON);

		state->mangled_body = 1;
		state->mangled_header = 1;
		q_status_message2(SM_ORDER, 0, 4,
				  "%s message%s excluded",
				  long2string(del_count),
				  plural(del_count));
	    }
	    else
	      any_messages(NULL, NULL, "Excluded");
	}
	else
	  any_messages(NULL, "deleted", "to Exclude");

	return;
    }
    else if(READONLY_FOLDER(stream)){
	q_status_message(SM_ORDER, 0, 4,
			 _("Can't expunge. Folder is read-only"));
	return;
    }

    prefilter_del_count = count_flagged(stream, F_DEL|F_NOFILT);

    mail_expunge_prefilter(stream, MI_NONE);

    if((del_count = count_flagged(stream, F_DEL|F_NOFILT)) != 0){
	int ret;

	snprintf(prompt, sizeof(prompt), "Expunge %ld message%s from %.*s", del_count,
		plural(del_count), sizeof(prompt)-40,
		pretty_fn(state->cur_folder));
	prompt[sizeof(prompt)-1] = '\0';
	state->mangled_footer = 1;

	if(F_ON(F_FULL_AUTO_EXPUNGE, state)
	   || (F_ON(F_AUTO_EXPUNGE, state)
	       && ((!strucmp(state->cur_folder,state->inbox_name))
		   || (state->context_current->use & CNTXT_INCMNG))
	       && context_isambig(state->cur_folder))
	   || (ret=want_to(prompt, 'y', 0, NO_HELP, WT_NORM)) == 'y')
	  ret = 'y';

	if(ret == 'x')
	  cmd_cancelled("Expunge");

	if(ret != 'y')
	  return;
    }

    dprint((8, "Expunge max:%ld cur:%ld kill:%d\n",
	      mn_get_total(msgmap), mn_get_cur(msgmap), del_count));

    lastc = pico_set_colors(state->VAR_TITLE_FORE_COLOR,
			    state->VAR_TITLE_BACK_COLOR,
			    PSC_REV|PSC_RET);

    PutLine0(0, 0, "**");			/* indicate delay */

    if(lastc){
	(void)pico_set_colorp(lastc, PSC_NONE);
	free_color_pair(&lastc);
    }

    MoveCursor(state->ttyo->screen_rows -FOOTER_ROWS(state), 0);
    fflush(stdout);

    we_cancel = busy_cue(_("Expunging"), NULL, 1);

    if(cmd_expunge_work(stream, msgmap))
      state->mangled_body = 1;

    if(we_cancel)
      cancel_busy_cue((sp_expunge_count(stream) > 0) ? 0 : -1);

    lastc = pico_set_colors(state->VAR_TITLE_FORE_COLOR,
			    state->VAR_TITLE_BACK_COLOR,
			    PSC_REV|PSC_RET);
    PutLine0(0, 0, "  ");			/* indicate delay's over */

    if(lastc){
	(void)pico_set_colorp(lastc, PSC_NONE);
	free_color_pair(&lastc);
    }

    fflush(stdout);

    if(sp_expunge_count(stream) > 0){
	/*
	 * This is kind of surprising at first. For most sort
	 * orders, if the whole set is sorted, then any subset
	 * is also sorted. Not so for threaded sorts.
	 */
	if(SORT_IS_THREADED(msgmap))
	  refresh_sort(stream, msgmap, SRT_NON);
    }
    else{
	if(del_count)
	  q_status_message1(SM_ORDER, 0, 3,
			    _("No messages expunged from folder \"%s\""),
			    pretty_fn(state->cur_folder));
	else if(!prefilter_del_count)
	  q_status_message(SM_ORDER, 0, 3,
		     _("No messages marked deleted.  No messages expunged."));
    }
}


/*----------------------------------------------------------------------
    Expunge_and_close callback to prompt user for confirmation

    Args: stream -- folder's stream
	  folder -- name of folder containing folders
	 deleted -- number of del'd msgs

 Result: 'y' to continue with expunge
 ----*/
int
expunge_prompt(MAILSTREAM *stream, char *folder, long int deleted)
{
    long  max_folder;
    int	  charcnt = 0;
    char  prompt_b[MAX_SCREEN_COLS+1], temp[MAILTMPLEN+1], buff[MAX_SCREEN_COLS+1];
    char *short_folder_name;

    if(deleted == 1)
      charcnt = 1;
    else{
	snprintf(temp, sizeof(temp), "%ld", deleted);
	charcnt = strlen(temp)+1;
    }

    max_folder = MAX(1,MAXPROMPT - (36+charcnt));
    strncpy(temp, folder, sizeof(temp));
    temp[sizeof(temp)-1] = '\0';
    short_folder_name = short_str(temp,buff,sizeof(buff),max_folder,FrontDots);

    if(IS_NEWS(stream))
      snprintf(prompt_b, sizeof(prompt_b),
	       "Delete %s%ld message%s from \"%s\"",
	       (deleted > 1L) ? "all " : "", deleted,
	       plural(deleted), short_folder_name);
    else
      snprintf(prompt_b, sizeof(prompt_b),
	       "Expunge the %ld deleted message%s from \"%s\"",
	       deleted, deleted == 1 ? "" : "s",
	       short_folder_name);

    return(want_to(prompt_b, 'y', 0, NO_HELP, WT_NORM));
}


/*
 * This is used with multiple append saves. Call it once before
 * the series of appends with SSCP_INIT and once after all are
 * done with SSCP_END. In between, it is called automatically
 * from save_fetch_append or save_fetch_append_cb when we need
 * to ask the user if he or she wants to continue even though
 * announced message size doesn't match the actual message size.
 * As of 2008-02-29 the gmail IMAP server has these size mismatches
 * on a regular basis even though the data is ok.
 */
int
save_size_changed_prompt(long msgno, int flags)
{
    int ret;
    char prompt[100];
    static int remember_the_yes = 0;
    static int possible_corruption = 0;
    static ESCKEY_S save_size_opts[] = {
	{'y', 'y', "Y", "Yes"},
	{'n', 'n', "N", "No"},
	{'a', 'a', "A", "yes to All"},
	{-1, 0, NULL, NULL}
    };

    if(flags & SSCP_INIT || flags & SSCP_END){
	if(flags & SSCP_END && possible_corruption)
	  q_status_message(SM_ORDER, 3, 3, "There is possible data corruption, check the results");

	remember_the_yes = 0;
	possible_corruption = 0;
	ps_global->noshow_error = 0;
	ps_global->noshow_warn = 0;
	return(0);
    }

    if(remember_the_yes){
	snprintf(prompt, sizeof(prompt),
		 "Message to save shrank! (msg # %ld): Continuing", msgno);
	q_status_message(SM_ORDER, 0, 3, prompt);
	display_message('x');
	return(remember_the_yes);
    }

    snprintf(prompt, sizeof(prompt),
	     "Message to save shrank! (msg # %ld): Continue anyway ? ", msgno);
    ret = radio_buttons(prompt, -FOOTER_ROWS(ps_global), save_size_opts,
			'n', 0, h_save_size_changed, RB_NORM);

    switch(ret){
      case 'a':
	remember_the_yes = 'y';
	possible_corruption++;
	return(remember_the_yes);

      case 'y':
	possible_corruption++;
	return('y');

      default:
	possible_corruption = 0;
	ps_global->noshow_error = 1;
	ps_global->noshow_warn = 1;
	break;
    }

    return('n');
}


/*----------------------------------------------------------------------
    Expunge_and_close callback that happens once the decision to expunge
     and close has been made and before expunging and closing begins
    

    Args: stream -- folder's stream
	  folder -- name of folder containing folders
	 deleted -- number of del'd msgs

 Result: 'y' to continue with expunge
 ----*/
void
expunge_and_close_begins(int flags, char *folder)
{
    if(!(flags & EC_NO_CLOSE)){
	q_status_message1(SM_INFO, 0, 1, "Closing \"%.200s\"...", folder);
	flush_status_messages(1);
    }
}


/*----------------------------------------------------------------------
    Export a message to a plain file in users home directory

    Args: state -- pointer to struct holding a bunch of pine state
	 msgmap -- table mapping msg nums to c-client sequence nums
	  qline -- screen line to ask questions on
	    agg -- boolean indicating we're to operate on aggregate set

 Result: 
 ----*/
int
cmd_export(struct pine *state, MSGNO_S *msgmap, int qline, int aopt)
{
    char      filename[MAXPATH+1], full_filename[MAXPATH+1], *err;
    char      nmsgs[80];
    int       r, leading_nl, failure = 0, orig_errno, rflags = GER_NONE;
    int       flags = GE_IS_EXPORT | GE_SEQ_SENSITIVE, rv = 0;
    ENVELOPE *env;
    MESSAGECACHE *mc;
    BODY     *b;
    long      i, count = 0L, start_of_append, rawno;
    gf_io_t   pc;
    STORE_S  *store;
    struct variable *vars = ps_global->vars;
    ESCKEY_S export_opts[5];
    static HISTORY_S *history = NULL;

    if(ps_global->restricted){
	q_status_message(SM_ORDER, 0, 3,
	    "Alpine demo can't export messages to files");
	return rv;
    }

    if(MCMD_ISAGG(aopt) && !pseudo_selected(state->mail_stream, msgmap))
      return rv;

    export_opts[i = 0].ch  = ctrl('T');
    export_opts[i].rval	   = 10;
    export_opts[i].name	   = "^T";
    export_opts[i++].label = N_("To Files");

#if	!defined(DOS) && !defined(MAC) && !defined(OS2)
    if(ps_global->VAR_DOWNLOAD_CMD && ps_global->VAR_DOWNLOAD_CMD[0]){
	export_opts[i].ch      = ctrl('V');
	export_opts[i].rval    = 12;
	export_opts[i].name    = "^V";
	/* TRANSLATORS: this is an abbreviation for Download Messages */
	export_opts[i++].label = N_("Downld Msg");
    }
#endif	/* !(DOS || MAC) */

    if(F_ON(F_ENABLE_TAB_COMPLETE,ps_global)){
	export_opts[i].ch      =  ctrl('I');
	export_opts[i].rval    = 11;
	export_opts[i].name    = "TAB";
	export_opts[i++].label = N_("Complete");
    }

#if	0
    /* Commented out since it's not yet support! */
    if(F_ON(F_ENABLE_SUB_LISTS,ps_global)){
	export_opts[i].ch      = ctrl('X');
	export_opts[i].rval    = 14;
	export_opts[i].name    = "^X";
	export_opts[i++].label = N_("ListMatches");
    }
#endif

    /*
     * If message has attachments, add a toggle that will allow the user
     * to save all of the attachments to a single directory, using the
     * names provided with the attachments or part names. What we'll do is
     * export the message as usual, and then export the attachments into
     * a subdirectory that did not exist before. The subdir will be named
     * something based on the name of the file being saved to, but a
     * unique, new name.
     */
    if(!MCMD_ISAGG(aopt)
       && state->mail_stream
       && (rawno = mn_m2raw(msgmap, mn_get_cur(msgmap))) > 0L
       && rawno <= state->mail_stream->nmsgs
       && (env = pine_mail_fetchstructure(state->mail_stream, rawno, &b))
       && b
       && b->type == TYPEMULTIPART
       && b->subtype
       && strucmp(b->subtype, "ALTERNATIVE") != 0){
	PART *part;

	part = b->nested.part;	/* 1st part */
	if(part && part->next)
	  flags |= GE_ALLPARTS;
    }

    export_opts[i].ch = -1;
    filename[0] = '\0';

    if(mn_total_cur(msgmap) <= 1L){
      snprintf(nmsgs, sizeof(nmsgs), "Msg #%ld", mn_get_cur(msgmap));
      nmsgs[sizeof(nmsgs)-1] = '\0';
    }
    else{
      snprintf(nmsgs, sizeof(nmsgs), "%s messages", comatose(mn_total_cur(msgmap)));
      nmsgs[sizeof(nmsgs)-1] = '\0';
    }

    r = get_export_filename(state, filename, NULL, full_filename,
			    sizeof(filename), nmsgs, "EXPORT",
			    export_opts, &rflags, qline, flags, &history);

    if(r < 0){
	switch(r){
	  case -1:
	    cmd_cancelled("Export message");
	    break;

	  case -2:
	    q_status_message1(SM_ORDER, 0, 2,
			      _("Can't export to file outside of %s"),
			      VAR_OPER_DIR);
	    break;
	}

	goto fini;
    }
#if	!defined(DOS) && !defined(MAC) && !defined(OS2)
    else if(r == 12){			/* Download */
	char     cmd[MAXPATH], *tfp = NULL;
	int	     next = 0;
	PIPE_S  *syspipe;
	STORE_S *so;
	gf_io_t  pc;

	if(ps_global->restricted){
	    q_status_message(SM_ORDER | SM_DING, 3, 3,
			     "Download disallowed in restricted mode");
	    goto fini;
	}

	err = NULL;
	tfp = temp_nam(NULL, "pd");
	build_updown_cmd(cmd, sizeof(cmd), ps_global->VAR_DOWNLOAD_CMD_PREFIX,
			 ps_global->VAR_DOWNLOAD_CMD, tfp);
	dprint((1, "Download cmd called: \"%s\"\n", cmd));
	if((so = so_get(FileStar, tfp, WRITE_ACCESS|OWNER_ONLY|WRITE_TO_LOCALE)) != NULL){
	    gf_set_so_writec(&pc, so);

	    for(i = mn_first_cur(msgmap); i > 0L; i = mn_next_cur(msgmap)){
	      if(!(state->mail_stream
		 && (rawno = mn_m2raw(msgmap, i)) > 0L
		 && rawno <= state->mail_stream->nmsgs
		 && (mc = mail_elt(state->mail_stream, rawno))
		 && mc->valid))
	        mc = NULL;

	      if(!(env = pine_mail_fetchstructure(state->mail_stream,
						  mn_m2raw(msgmap, i), &b))
		 || !bezerk_delimiter(env, mc, pc, next++)
		 || !format_message(mn_m2raw(msgmap, mn_get_cur(msgmap)),
				    env, b, NULL, FM_NEW_MESS | FM_NOWRAP, pc)){
		  q_status_message(SM_ORDER | SM_DING, 3, 3,
			   err = "Error writing tempfile for download");
		  break;
	      }
	    }

	    gf_clear_so_writec(so);
	    if(so_give(&so)){			/* close file */
		if(!err)
		  err = "Error writing tempfile for download";
	    }

	    if(!err){
		if((syspipe = open_system_pipe(cmd, NULL, NULL,
					      PIPE_USER | PIPE_RESET,
					      0, pipe_callback, pipe_report_error)) != NULL)
		  (void) close_system_pipe(&syspipe, NULL, pipe_callback);
		else
		  q_status_message(SM_ORDER | SM_DING, 3, 3,
				err = _("Error running download command"));
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
	  q_status_message(SM_ORDER, 0, 3, _("Download Command Completed"));

	goto fini;
    }
#endif	/* !(DOS || MAC) */


    if(rflags & GER_APPEND)
      leading_nl = 1;
    else
      leading_nl = 0;

    dprint((5, "Opening file \"%s\" for export\n",
	   full_filename ? full_filename : "?"));

    if(!(store = so_get(FileStar, full_filename, WRITE_ACCESS|WRITE_TO_LOCALE))){
        q_status_message2(SM_ORDER | SM_DING, 3, 4,
		      /* TRANSLATORS: error opening file "<filename>" to export message: <error text> */
		      _("Error opening file \"%s\" to export message: %s"),
                          full_filename, error_description(errno));
	goto fini;
    }
    else
      gf_set_so_writec(&pc, store);

    err = NULL;
    for(i = mn_first_cur(msgmap); i > 0L; i = mn_next_cur(msgmap), count++){
	env = pine_mail_fetchstructure(state->mail_stream, mn_m2raw(msgmap, i),
				       &b);
	if(!env) {
	    err = _("Can't export message. Error accessing mail folder");
	    failure = 1;
	    break;
	}

        if(!(state->mail_stream
	   && (rawno = mn_m2raw(msgmap, i)) > 0L
	   && rawno <= state->mail_stream->nmsgs
	   && (mc = mail_elt(state->mail_stream, rawno))
	   && mc->valid))
	  mc = NULL;

	start_of_append = so_tell(store);
	if(!bezerk_delimiter(env, mc, pc, leading_nl)
	   || !format_message(mn_m2raw(msgmap, i), env, b, NULL,
			      FM_NEW_MESS | FM_NOWRAP, pc)){
	    orig_errno = errno;		/* save incase things are really bad */
	    failure    = 1;		/* pop out of here */
	    break;
	}

	leading_nl = 1;
    }

    gf_clear_so_writec(store);
    if(so_give(&store))				/* release storage */
      failure++;

    if(failure){
	our_truncate(full_filename, (off_t)start_of_append);
	if(err){
	    dprint((1, "FAILED Export: fetch(%ld): %s\n",
		       i, err ? err : "?"));
	    q_status_message(SM_ORDER | SM_DING, 3, 4, err);
	}
	else{
	    dprint((1, "FAILED Export: file \"%s\" : %s\n",
		       full_filename ? full_filename : "?",
		       error_description(orig_errno)));
	    q_status_message2(SM_ORDER | SM_DING, 3, 4,
			      /* TRANSLATORS: Error exporting to <filename>: <error text> */
			      _("Error exporting to \"%s\" : %s"),
			      filename, error_description(orig_errno));
	}
    }
    else{
	if(rflags & GER_ALLPARTS && full_filename[0]){
	    char dir[MAXPATH+1];
	    char  lfile[MAXPATH+1];
	    int  ok = 0, tries = 0, saved = 0, errs = 0;
	    ATTACH_S *a;

	    /*
	     * Now we want to save all of the attachments to a subdirectory.
	     * To make it easier for us and probably easier for the user, and
	     * to prevent the user from shooting himself in the foot, we
	     * make a new subdirectory so that we can't possibly step on
	     * any existing files, and we don't need any interaction with the
	     * user while saving.
	     *
	     * We'll just use the directory name full_filename.d or if that
	     * already exists and isn't empty, we'll try adding a suffix to
	     * that until we get something to use.
	     */

	    if(strlen(full_filename) + strlen(".d") + 1 > sizeof(dir)){
		q_status_message1(SM_ORDER | SM_DING, 3, 4,
			  _("Can't save attachments, filename too long: %s"),
			  full_filename);
		goto fini;
	    }

	    ok = 0;
	    snprintf(dir, sizeof(dir), "%s.d", full_filename);
	    dir[sizeof(dir)-1] = '\0';

	    do {
		tries++;
		switch(r = is_writable_dir(dir)){
		  case 0:		/* exists and is a writable dir */
		    /*
		     * We could figure out if it is empty and use it in
		     * that case, but that sounds like a lot of work, so
		     * just fall through to default.
		     */

		  default:
		    if(strlen(full_filename) + strlen(".d") + 1 +
		       1 + strlen(long2string((long) tries)) > sizeof(dir)){
			q_status_message(SM_ORDER | SM_DING, 3, 4,
					      "Problem saving attachments");
			goto fini;
		    }

		    snprintf(dir, sizeof(dir), "%s.d_%s", full_filename,
			    long2string((long) tries));
		    dir[sizeof(dir)-1] = '\0';
		    break;

		  case 3:		/* doesn't exist, that's good! */
		    /* make new directory */
		    ok++;
		    break;
		}
	    } while(!ok && tries < 1000);
	    
	    if(tries >= 1000){
		q_status_message(SM_ORDER | SM_DING, 3, 4,
					      _("Problem saving attachments"));
		goto fini;
	    }

	    /* create the new directory */
	    if(our_mkdir(dir, 0700)){
		q_status_message2(SM_ORDER | SM_DING, 3, 4,
		      _("Problem saving attachments: %s: %s"), dir,
		      error_description(errno));
		goto fini;
	    }

	    if(!(state->mail_stream
	         && (rawno = mn_m2raw(msgmap, mn_get_cur(msgmap))) > 0L
	         && rawno <= state->mail_stream->nmsgs
	         && (env=pine_mail_fetchstructure(state->mail_stream,rawno,&b))
	         && b)){
		q_status_message(SM_ORDER | SM_DING, 3, 4,
					      _("Problem reading message"));
		goto fini;
	    }

	    zero_atmts(state->atmts);
	    describe_mime(b, "", 1, 1, 0, 0);

	    a = state->atmts;
	    if(a && a->description)		/* skip main body part */
	      a++;

	    for(; a->description != NULL; a++){
		/* skip over these parts of the message */
		if(MIME_MSG_A(a) || MIME_DGST_A(a) || MIME_VCARD_A(a))
		  continue;
		
		lfile[0] = '\0';
		(void) get_filename_parameter(lfile, sizeof(lfile), a->body, NULL);
		
		if(lfile[0] == '\0'){
		  snprintf(lfile, sizeof(lfile), "part_%.*s", sizeof(lfile)-6,
			  a->number ? a->number : "?");
		  lfile[sizeof(lfile)-1] = '\0';
		}

		if(strlen(dir) + strlen(S_FILESEP) + strlen(lfile) + 1
							    > sizeof(filename)){
		    dprint((2,
			   "FAILED Att Export: name too long: %s\n",
			   dir, S_FILESEP, lfile));
		    errs++;
		    continue;
		}

		snprintf(filename, sizeof(filename), "%s%s%s", dir, S_FILESEP, lfile);
		filename[sizeof(filename)-1] = '\0';

		if(write_attachment_to_file(state->mail_stream, rawno,
					    a, GER_NONE, filename) == 1)
		  saved++;
		else
		  errs++;
	    }

	    if(errs){
		if(saved)
		  q_status_message1(SM_ORDER, 3, 3,
			"Errors saving some attachments, %s attachments saved",
			long2string((long) saved));
		else
		  q_status_message(SM_ORDER, 3, 3,
			_("Problems saving attachments"));
	    }
	    else{
		if(saved)
		  q_status_message2(SM_ORDER, 0, 3,
			/* TRANSLATORS: Saved <how many> attachements to <directory name> */
			_("Saved %s attachments to %s"),
			long2string((long) saved), dir);
		else
		  q_status_message(SM_ORDER, 3, 3, _("No attachments to save"));
	    }
	}
	else if(mn_total_cur(msgmap) > 1L)
	  q_status_message4(SM_ORDER,0,3,
			    "%s message%s %s to file \"%s\"",
			    long2string(count), plural(count),
			    rflags & GER_OVER
			      ? "overwritten"
			      : rflags & GER_APPEND ? "appended" : "exported",
			    filename);
	else
	  q_status_message3(SM_ORDER,0,3,
			    "Message %s %s to file \"%s\"",
			    long2string(mn_get_cur(msgmap)),
			    rflags & GER_OVER
			      ? "overwritten"
			      : rflags & GER_APPEND ? "appended" : "exported",
			    filename);
	rv++;
    }

  fini:
    if(MCMD_ISAGG(aopt))
      restore_selected(msgmap);

    return rv;
}


/*
 * Ask user what file to export to. Export from srcstore to that file.
 *
 * Args     ps -- pine struct
 *     srctext -- pointer to source text
 *     srctype -- type of that source text
 *  prompt_msg -- see get_export_filename
 *  lister_msg --      "
 *
 * Returns: != 0 : error
 *             0 : ok
 */
int
simple_export(struct pine *ps, void *srctext, SourceType srctype, char *prompt_msg, char *lister_msg)
{
    int r = 1, rflags = GER_NONE;
    char     filename[MAXPATH+1], full_filename[MAXPATH+1];
    STORE_S *store = NULL;
    struct variable *vars = ps->vars;
    static HISTORY_S *history = NULL;
    static ESCKEY_S simple_export_opts[] = {
	{ctrl('T'), 10, "^T", N_("To Files")},
	{-1, 0, NULL, NULL},
	{-1, 0, NULL, NULL}};

    if(F_ON(F_ENABLE_TAB_COMPLETE,ps)){
	simple_export_opts[r].ch    =  ctrl('I');
	simple_export_opts[r].rval  = 11;
	simple_export_opts[r].name  = "TAB";
	simple_export_opts[r].label = N_("Complete");
    }

    if(!srctext){
	q_status_message(SM_ORDER, 0, 2, _("Error allocating space"));
	r = -3;
	goto fini;
    }

    simple_export_opts[++r].ch = -1;
    filename[0] = '\0';
    full_filename[0] = '\0';

    r = get_export_filename(ps, filename, NULL, full_filename, sizeof(filename),
			    prompt_msg, lister_msg, simple_export_opts, &rflags,
			    -FOOTER_ROWS(ps), GE_IS_EXPORT, &history);

    if(r < 0)
      goto fini;
    else if(!full_filename[0]){
	r = -1;
	goto fini;
    }

    dprint((5, "Opening file \"%s\" for export\n",
	   full_filename ? full_filename : "?"));

    if((store = so_get(FileStar, full_filename, WRITE_ACCESS|WRITE_TO_LOCALE)) != NULL){
	char *pipe_err;
	gf_io_t pc, gc;

	gf_set_so_writec(&pc, store);
	gf_set_readc(&gc, srctext, (srctype == CharStar)
					? strlen((char *)srctext)
					: 0L,
		     srctype, 0);
	gf_filter_init();
	if((pipe_err = gf_pipe(gc, pc)) != NULL){
	    q_status_message2(SM_ORDER | SM_DING, 3, 3,
			      /* TRANSLATORS: Problem saving to <filename>: <error text> */
			      _("Problem saving to \"%s\": %s"),
			      filename, pipe_err);
	    r = -3;
	}
	else
	  r = 0;

	gf_clear_so_writec(store);
	if(so_give(&store)){
	    q_status_message2(SM_ORDER | SM_DING, 3, 3,
			      _("Problem saving to \"%s\": %s"),
			      filename, error_description(errno));
	    r = -3;
	}
    }
    else{
	q_status_message2(SM_ORDER | SM_DING, 3, 4,
			  _("Error opening file \"%s\" for export: %s"),
			  full_filename, error_description(errno));
	r = -3;
    }

fini:
    switch(r){
      case  0:
	/* overloading full_filename */
	snprintf(full_filename, sizeof(full_filename), "%c%s",
		(prompt_msg && prompt_msg[0])
		  ? (islower((unsigned char)prompt_msg[0])
		    ? toupper((unsigned char)prompt_msg[0]) : prompt_msg[0])
		  : 'T',
	        (prompt_msg && prompt_msg[0]) ? prompt_msg+1 : "ext");
	full_filename[sizeof(full_filename)-1] = '\0';
	q_status_message3(SM_ORDER,0,2,"%s %s to \"%s\"",
			  full_filename,
			  rflags & GER_OVER
			      ? "overwritten"
			      : rflags & GER_APPEND ? "appended" : "exported",
			  filename);
	break;

      case -1:
	cmd_cancelled("Export");
	break;

      case -2:
	q_status_message1(SM_ORDER, 0, 2,
	    _("Can't export to file outside of %s"), VAR_OPER_DIR);
	break;
    }

    ps->mangled_footer = 1;
    return(r);
}


/*
 * Ask user what file to export to.
 *
 *       filename -- On input, this is the filename to start with. On exit,
 *                   this is the filename chosen. (but this isn't used)
 *       deefault -- This is the default value if user hits return. The
 *                   prompt will have [deefault] added to it automatically.
 *  full_filename -- This is the full filename on exit.
 *            len -- Minimum length of _both_ filename and full_filename.
 *     prompt_msg -- Message to insert in prompt.
 *     lister_msg -- Message to insert in file_lister.
 *           opts -- Key options.
 *                      There is a tangled relationship between the callers
 *                      and this routine as far as opts are concerned. Some
 *                      of the opts are handled here. In particular, r == 3,
 *                      r == 10, r == 11, and r == 13 are all handled here.
 *                      Don't use those values unless you want what happens
 *                      here. r == 12 and others are handled by the caller.
 *         rflags -- Return flags
 *                     GER_OVER      - overwrite of existing file
 *                     GER_APPEND    - append of existing file
 *                      else file did not exist before
 *
 *                     GER_ALLPARTS  - AllParts toggle was turned on
 *
 *          qline -- Command line to prompt on.
 *          flags -- Logically OR'd flags
 *                     GE_IS_EXPORT     - The command was an Export command
 *                                        so the prompt should include
 *                                        EXPORT:.
 *                     GE_SEQ_SENSITIVE - The command that got us here is
 *                                        sensitive to sequence number changes
 *                                        caused by unsolicited expunges.
 *                     GE_NO_APPEND     - We will not allow append to an
 *                                        existing file, only removal of the
 *                                        file if it exists.
 *                     GE_IS_IMPORT     - We are selecting for reading.
 *                                        No overwriting or checking for
 *                                        existence at all. Don't use this
 *                                        together with GE_NO_APPEND.
 *                     GE_ALLPARTS      - Turn on AllParts toggle.
 *
 *  Returns:  -1  cancelled
 *            -2  prohibited by VAR_OPER_DIR
 *            -3  other error, already reported here
 *             0  ok
 *            12  user chose 12 command from opts
 */
int
get_export_filename(struct pine *ps, char *filename, char *deefault,
		    char *full_filename, size_t len, char *prompt_msg,
		    char *lister_msg, ESCKEY_S *optsarg, int *rflags,
		    int qline, int flags, HISTORY_S **history)
{
    char      dir[MAXPATH+1], dir2[MAXPATH+1];
    char      precolon[MAXPATH+1], postcolon[MAXPATH+1];
    char      filename2[MAXPATH+1], tmp[MAXPATH+1], *fn, *ill;
    int       l, i, ku = -1, r, fatal, homedir = 0, was_abs_path=0, avail, ret = 0;
    int       allparts = 0;
    char      prompt_buf[400];
    char      def[500];
    ESCKEY_S *opts = NULL;
    struct variable *vars = ps->vars;

    if(flags & GE_ALLPARTS || history){
	/*
	 * Copy the opts and add one to the end of the list.
	 */
	for(i = 0; optsarg[i].ch != -1; i++)
	  ;

	if(history)
	  i += 2;
	
	if(flags & GE_ALLPARTS)
	  i++;

	opts = (ESCKEY_S *) fs_get((i+1) * sizeof(*opts));
	memset(opts, 0, (i+1) * sizeof(*opts));

	for(i = 0; optsarg[i].ch != -1; i++){
	    opts[i].ch = optsarg[i].ch;
	    opts[i].rval = optsarg[i].rval;
	    opts[i].name = optsarg[i].name;	/* no need to make a copy */
	    opts[i].label = optsarg[i].label;	/* " */
	}

	if(flags & GE_ALLPARTS){
	    allparts = i;
	    opts[i].ch      = ctrl('P');
	    opts[i].rval    = 13;
	    opts[i].name    = "^P";
	    /* TRANSLATORS: Export all attachment parts */
	    opts[i++].label = N_("AllParts");
	}

	if(history){
	    opts[i].ch      = KEY_UP;
	    opts[i].rval    = 30;
	    opts[i].name    = "";
	    ku = i;
	    opts[i++].label = "";

	    opts[i].ch      = KEY_DOWN;
	    opts[i].rval    = 31;
	    opts[i].name    = "";
	    opts[i++].label = "";
	}
	
	opts[i].ch = -1;

	if(history)
	  init_hist(history, HISTSIZE);
    }
    else
      opts = optsarg;

    if(rflags)
      *rflags = GER_NONE;

    if(F_ON(F_USE_CURRENT_DIR, ps))
      dir[0] = '\0';
    else if(VAR_OPER_DIR){
      strncpy(dir, VAR_OPER_DIR, sizeof(dir));
      dir[sizeof(dir)-1] = '\0';
    }
#if	defined(DOS) || defined(OS2)
    else if(VAR_FILE_DIR){
      strncpy(dir, VAR_FILE_DIR, sizeof(dir));
      dir[sizeof(dir)-1] = '\0';
    }
#endif
    else{
	dir[0] = '~';
	dir[1] = '\0';
	homedir=1;
    }

    postcolon[0] = '\0';
    strncpy(precolon, dir, sizeof(precolon));
    precolon[sizeof(precolon)-1] = '\0';
    if(deefault){
	strncpy(def, deefault, sizeof(def)-1);
	def[sizeof(def)-1] = '\0';
	removing_leading_and_trailing_white_space(def);
    }
    else
      def[0] = '\0';
    
    avail = MAX(20, ps_global->ttyo ? ps_global->ttyo->screen_cols : 80) - MIN_OPT_ENT_WIDTH;

    /*---------- Prompt the user for the file name -------------*/
    while(1){
	int  oeflags;
	char dirb[50], fileb[50];
	int  l1, l2, l3, l4, l5, needed;
	char *p, p1[100], p2[100], *p3, p4[100], p5[100];

	snprintf(p1, sizeof(p1), "%sCopy ", 
		(flags & GE_IS_EXPORT) ? "EXPORT: " :
		  (flags & GE_IS_IMPORT) ? "IMPORT: " : "SAVE: ");
	p1[sizeof(p1)-1] = '\0';
	l1 = strlen(p1);

	strncpy(p2, prompt_msg ? prompt_msg : "", sizeof(p2)-1);
	p2[sizeof(p2)-1] = '\0';
	l2 = strlen(p2);

	if(rflags && *rflags & GER_ALLPARTS)
	  p3 = " (and atts)";
	else
	  p3 = "";
	
	l3 = strlen(p3);

	snprintf(p4, sizeof(p4), " %s file%s%s",
		(flags & GE_IS_IMPORT) ? "from" : "to",
		is_absolute_path(filename) ? "" : " in ",
		is_absolute_path(filename) ? "" :
		  (!dir[0] ? "current directory"
			   : (dir[0] == '~' && !dir[1]) ? "home directory"
				     : short_str(dir,dirb,sizeof(dirb),30,FrontDots)));
	p4[sizeof(p4)-1] = '\0';
	l4 = strlen(p4);

	snprintf(p5, sizeof(p5), "%s%s%s: ",
		*def ? " [" : "",
		*def ? short_str(def,fileb,sizeof(fileb),40,EndDots) : "",
		*def ? "]" : "");
	p5[sizeof(p5)-1] = '\0';
	l5 = strlen(p5);

	if((needed = l1+l2+l3+l4+l5-avail) > 0){
	    snprintf(p4, sizeof(p4), " %s file%s%s",
		    (flags & GE_IS_IMPORT) ? "from" : "to",
		    is_absolute_path(filename) ? "" : " in ",
		    is_absolute_path(filename) ? "" :
		      (!dir[0] ? "current dir"
			       : (dir[0] == '~' && !dir[1]) ? "home dir"
					 : short_str(dir,dirb,sizeof(dirb),10,FrontDots)));
	    p4[sizeof(p4)-1] = '\0';
	    l4 = strlen(p4);
	}

	if((needed = l1+l2+l3+l4+l5-avail) > 0 && l5 > 0){
	    snprintf(p5, sizeof(p5), "%s%s%s: ",
		    *def ? " [" : "",
		    *def ? short_str(def,fileb,sizeof(fileb),
				     MAX(15,l5-5-needed),EndDots) : "",
		    *def ? "]" : "");
	    p5[sizeof(p5)-1] = '\0';
	    l5 = strlen(p5);
	}

	if((needed = l1+l2+l3+l4+l5-avail) > 0 && l2 > 0){

	    /*
	     * 14 is about the shortest we can make this, because there are
	     * fixed length strings of length 14 coming in here.
	     */
	    p = short_str(prompt_msg, p2, sizeof(p2), MAX(14,l2-needed), FrontDots);
	    if(p != p2){
		strncpy(p2, p, sizeof(p2)-1);
		p2[sizeof(p2)-1] = '\0';
	    }

	    l2 = strlen(p2);
	}

	if((needed = l1+l2+l3+l4+l5-avail) > 0){
	    strncpy(p1, "Copy ", sizeof(p1)-1);
	    p1[sizeof(p1)-1] = '\0';
	    l1 = strlen(p1);
	}

	if((needed = l1+l2+l3+l4+l5-avail) > 0 && l5 > 0){
	    snprintf(p5, sizeof(p5), "%s%s%s: ",
		    *def ? " [" : "",
		    *def ? short_str(def,fileb, sizeof(fileb),
				     MAX(10,l5-5-needed),EndDots) : "",
		    *def ? "]" : "");
	    p5[sizeof(p5)-1] = '\0';
	    l5 = strlen(p5);
	}

	if((needed = l1+l2+l3+l4+l5-avail) > 0 && l3 > 0){
	    if(needed <= l3 - strlen(" (+ atts)"))
	      p3 = " (+ atts)";
	    else if(needed <= l3 - strlen(" (atts)"))
	      p3 = " (atts)";
	    else if(needed <= l3 - strlen(" (+)"))
	      p3 = " (+)";
	    else if(needed <= l3 - strlen("+"))
	      p3 = "+";
	    else
	      p3 = "";

	    l3 = strlen(p3);
	}

	snprintf(prompt_buf, sizeof(prompt_buf), "%s%s%s%s%s", p1, p2, p3, p4, p5);
	prompt_buf[sizeof(prompt_buf)-1] = '\0';

	if(ku >= 0){
	    if(items_in_hist(*history) > 0){
		opts[ku].name  = HISTORY_UP_KEYNAME;
		opts[ku].label = HISTORY_KEYLABEL;
		opts[ku+1].name  = HISTORY_DOWN_KEYNAME;
		opts[ku+1].label = HISTORY_KEYLABEL;
	    }
	    else{
		opts[ku].name  = "";
		opts[ku].label = "";
		opts[ku+1].name  = "";
		opts[ku+1].label = "";
	    }
	}

	oeflags = OE_APPEND_CURRENT |
		  ((flags & GE_SEQ_SENSITIVE) ? OE_SEQ_SENSITIVE : 0);
	r = optionally_enter(filename, qline, 0, len, prompt_buf,
			     opts, NO_HELP, &oeflags);

        /*--- Help ----*/
	if(r == 3){
	    /*
	     * Helps may not be right if you add another caller or change
	     * things. Check it out.
	     */
	    if(flags & GE_IS_IMPORT)
	      helper(h_ge_import, _("HELP FOR IMPORT FILE SELECT"), HLPD_SIMPLE);
	    else if(flags & GE_ALLPARTS)
	      helper(h_ge_allparts, _("HELP FOR EXPORT FILE SELECT"), HLPD_SIMPLE);
	    else
	      helper(h_ge_export, _("HELP FOR EXPORT FILE SELECT"), HLPD_SIMPLE);

	    ps->mangled_screen = 1;

	    continue;
        }
	else if(r == 10 || r == 11){	/* Browser or File Completion */
	    if(filename[0]=='~'){
	      if(filename[1] == C_FILESEP && filename[2]!='\0'){
		precolon[0] = '~';
		precolon[1] = '\0';
		for(i=0; filename[i+2] != '\0' && i+2 < len-1; i++)
		  filename[i] = filename[i+2];
		filename[i] = '\0';
		strncpy(dir, precolon, sizeof(dir)-1);
		dir[sizeof(dir)-1] = '\0';
	      }
	      else if(filename[1]=='\0' || 
		 (filename[1] == C_FILESEP && filename[2] == '\0')){
		precolon[0] = '~';
		precolon[1] = '\0';
		filename[0] = '\0';
		strncpy(dir, precolon, sizeof(dir)-1);
		dir[sizeof(dir)-1] = '\0';
	      }
	    }
	    else if(!dir[0] && !is_absolute_path(filename) && was_abs_path){
	      if(homedir){
		precolon[0] = '~';
		precolon[1] = '\0';
		strncpy(dir, precolon, sizeof(dir)-1);
		dir[sizeof(dir)-1] = '\0';
	      }
	      else{
		precolon[0] = '\0';
		dir[0] = '\0';
	      }
	    }
	    l = MAXPATH;
	    dir2[0] = '\0';
	    strncpy(tmp, filename, sizeof(tmp)-1);
	    tmp[sizeof(tmp)-1] = '\0';
	    if(*tmp && is_absolute_path(tmp))
	      fnexpand(tmp, sizeof(tmp));
	    if(strncmp(tmp,postcolon, strlen(postcolon)))
	      postcolon[0] = '\0';

	    if(*tmp && (fn = last_cmpnt(tmp))){
	        l -= fn - tmp;
		strncpy(filename2, fn, sizeof(filename2)-1);
		filename2[sizeof(filename2)-1] = '\0';
		if(is_absolute_path(tmp)){
		    strncpy(dir2, tmp, MIN(fn - tmp, sizeof(dir2)-1));
		    dir2[MIN(fn - tmp, sizeof(dir2)-1)] = '\0';
#ifdef _WINDOWS
		    if(tmp[1]==':' && tmp[2]=='\\' && dir2[2]=='\0'){
		      dir2[2] = '\\';
		      dir2[3] = '\0';
		    }
#endif
		    strncpy(postcolon, dir2, sizeof(postcolon)-1);
		    postcolon[sizeof(postcolon)-1] = '\0';
		    precolon[0] = '\0';
		}
		else{
		    char *p = NULL;
		    /*
		     * Just building the directory name in dir2,
		     * full_filename is overloaded.
		     */
		    snprintf(full_filename, len, "%.*s", MIN(fn-tmp,len-1), tmp);
		    full_filename[len-1] = '\0';
		    strncpy(postcolon, full_filename, sizeof(postcolon)-1);
		    postcolon[sizeof(postcolon)-1] = '\0';
		    build_path(dir2, !dir[0] ? p = (char *)getcwd(NULL,MAXPATH)
					     : (dir[0] == '~' && !dir[1])
					       ? ps->home_dir
					       : dir,
			       full_filename, sizeof(dir2));
		    if(p)
		      free(p);
		}
	    }
	    else{
		if(is_absolute_path(tmp)){
		    strncpy(dir2, tmp, sizeof(dir2)-1);
		    dir2[sizeof(dir2)-1] = '\0';
#ifdef _WINDOWS
		    if(dir2[2]=='\0' && dir2[1]==':'){
		      dir2[2]='\\';
		      dir2[3]='\0';
		      strncpy(postcolon,dir2,sizeof(postcolon)-1);
		      postcolon[sizeof(postcolon)-1] = '\0';
		    }
#endif
		    filename2[0] = '\0';
		    precolon[0] = '\0';
		}
		else{
		    strncpy(filename2, tmp, sizeof(filename2)-1);
		    filename2[sizeof(filename2)-1] = '\0';
		    if(!dir[0])
		      (void)getcwd(dir2, sizeof(dir2));
		    else if(dir[0] == '~' && !dir[1]){
			strncpy(dir2, ps->home_dir, sizeof(dir2)-1);
			dir2[sizeof(dir2)-1] = '\0';
		    }
		    else{
			strncpy(dir2, dir, sizeof(dir2)-1);
			dir2[sizeof(dir2)-1] = '\0';
		    }

		    postcolon[0] = '\0';
		}
	    }

	    build_path(full_filename, dir2, filename2, len);
	    if(!strcmp(full_filename, dir2))
	      filename2[0] = '\0';
	    if(full_filename[strlen(full_filename)-1] == C_FILESEP 
	       && isdir(full_filename,NULL,NULL)){
	      if(strlen(full_filename) == 1)
		strncpy(postcolon, full_filename, sizeof(postcolon)-1);
	      else if(filename2[0])
		strncpy(postcolon, filename2, sizeof(postcolon)-1);
	      postcolon[sizeof(postcolon)-1] = '\0';
	      strncpy(dir2, full_filename, sizeof(dir2)-1);
	      dir2[sizeof(dir2)-1] = '\0';
	      filename2[0] = '\0';
	    }
#ifdef _WINDOWS  /* use full_filename even if not a valid directory */
	    else if(full_filename[strlen(full_filename)-1] == C_FILESEP){ 
	      strncpy(postcolon, filename2, sizeof(postcolon)-1);
	      postcolon[sizeof(postcolon)-1] = '\0';
	      strncpy(dir2, full_filename, sizeof(dir2)-1);
	      dir2[sizeof(dir2)-1] = '\0';
	      filename2[0] = '\0';
	    }
#endif
	    if(dir2[strlen(dir2)-1] == C_FILESEP && strlen(dir2)!=1
	       && strcmp(dir2+1, ":\\")) 
	      /* last condition to prevent stripping of '\\' 
		 in windows partition */
	      dir2[strlen(dir2)-1] = '\0';

	    if(r == 10){			/* File Browser */
		r = file_lister(lister_msg ? lister_msg : "EXPORT",
				dir2, sizeof(dir2), filename2, sizeof(filename2), 
                                TRUE,
				(flags & GE_IS_IMPORT) ? FB_READ : FB_SAVE);
#ifdef _WINDOWS
/* Windows has a special "feature" in which entering the file browser will
   change the working directory if the directory is changed at all (even
   clicking "Cancel" will change the working directory).
*/
		if(F_ON(F_USE_CURRENT_DIR, ps))
		  (void)getcwd(dir2,sizeof(dir2));
#endif
		if(isdir(dir2,NULL,NULL)){
		  strncpy(precolon, dir2, sizeof(precolon)-1);
		  precolon[sizeof(precolon)-1] = '\0';
		}
		strncpy(postcolon, filename2, sizeof(postcolon)-1);
		postcolon[sizeof(postcolon)-1] = '\0';
		if(r == 1){
		    build_path(full_filename, dir2, filename2, len);
		    if(isdir(full_filename, NULL, NULL)){
			strncpy(dir, full_filename, sizeof(dir)-1);
			dir[sizeof(dir)-1] = '\0';
			filename[0] = '\0';
		    }
		    else{
			fn = last_cmpnt(full_filename);
			strncpy(dir, full_filename,
				MIN(fn - full_filename, sizeof(dir)-1));
			dir[MIN(fn - full_filename, sizeof(dir)-1)] = '\0';
			if(fn - full_filename > 1)
			  dir[fn - full_filename - 1] = '\0';
		    }
		    
		    if(!strcmp(dir, ps->home_dir)){
			dir[0] = '~';
			dir[1] = '\0';
		    }

		    strncpy(filename, fn, len-1);
		    filename[len-1] = '\0';
		}
	    }
	    else{				/* File Completion */
	      if(!pico_fncomplete(dir2, filename2, sizeof(filename2)))
		  Writechar(BELL, 0);
	      strncat(postcolon, filename2,
		      sizeof(postcolon)-1-strlen(postcolon));
	      postcolon[sizeof(postcolon)-1] = '\0';
	      
	      was_abs_path = is_absolute_path(filename);

	      if(!strcmp(dir, ps->home_dir)){
		dir[0] = '~';
		dir[1] = '\0';
	      }
	    }
	    strncpy(filename, postcolon, len-1);
	    filename[len-1] = '\0';
	    strncpy(dir, precolon, sizeof(dir)-1);
	    dir[sizeof(dir)-1] = '\0';

	    if(filename[0] == '~' && !filename[1]){
		dir[0] = '~';
		dir[1] = '\0';
		filename[0] = '\0';
	    }

	    continue;
	}
	else if(r == 12){	/* Download, caller handles it */
	    ret = r;
	    goto done;
	}
	else if(r == 13){	/* toggle AllParts bit */
	    if(rflags){
		if(*rflags & GER_ALLPARTS){
		    *rflags &= ~GER_ALLPARTS;
		    opts[allparts].label = N_("AllParts");
		}
		else{
		    *rflags |=  GER_ALLPARTS;
		    /* opposite of All Parts, No All Parts */
		    opts[allparts].label = N_("NoAllParts");
		}
	    }

	    continue;
	}
#if	0
	else if(r == 14){	/* List file names matching partial? */
	    continue;
	}
#endif
        else if(r == 1){	/* Cancel */
	    ret = -1;
	    goto done;
        }
        else if(r == 4){
	    continue;
	}
	else if(r == 30 || r == 31){
	    char *p = NULL;

	    if(history){
		if(r == 30)
		  p = get_prev_hist(*history, filename, 0, NULL);
		else
		  p = get_next_hist(*history, filename, 0, NULL);
	    }

	    if(p != NULL){
		fn = last_cmpnt(p);
		strncpy(dir, p, MIN(fn - p, sizeof(dir)-1));
		dir[MIN(fn - p, sizeof(dir)-1)] = '\0';
		if(fn - p > 1)
		  dir[fn - p - 1] = '\0';
		
		if(!strcmp(dir, ps->home_dir)){
		    dir[0] = '~';
		    dir[1] = '\0';
		}

		strncpy(filename, fn, len-1);
		filename[len-1] = '\0';
	    }
	    else
	      Writechar(BELL, 0);

	    continue;
	}
	else if(r != 0){
	    Writechar(BELL, 0);
	    continue;
	}

        removing_leading_and_trailing_white_space(filename);

	if(!*filename){
	    if(!*def){		/* Cancel */
		ret = -1;
		goto done;
	    }
	    
	    strncpy(filename, def, len-1);
	    filename[len-1] = '\0';
	}

#if	defined(DOS) || defined(OS2)
	if(is_absolute_path(filename)){
	    fixpath(filename, len);
	}
#else
	if(filename[0] == '~'){
	    if(fnexpand(filename, len) == NULL){
		char *p = strindex(filename, '/');
		if(p != NULL)
		  *p = '\0';
		q_status_message1(SM_ORDER | SM_DING, 3, 3,
			  _("Error expanding file name: \"%s\" unknown user"),
			      filename);
		continue;
	    }
	}
#endif

	if(is_absolute_path(filename)){
	    strncpy(full_filename, filename, len-1);
	    full_filename[len-1] = '\0';
	}
	else{
	    if(!dir[0])
	      build_path(full_filename, (char *)getcwd(dir,sizeof(dir)),
			 filename, len);
	    else if(dir[0] == '~' && !dir[1])
	      build_path(full_filename, ps->home_dir, filename, len);
	    else
	      build_path(full_filename, dir, filename, len);
	}

        if((ill = filter_filename(full_filename, &fatal, 
				  ps_global->restricted || ps_global->VAR_OPER_DIR)) != NULL){
	    if(fatal){
		q_status_message1(SM_ORDER | SM_DING, 3, 3, "%s", ill);
		continue;
	    }
	    else{
/* BUG: we should beep when the key's pressed rather than bitch later */
		/* Warn and ask for confirmation. */
		snprintf(prompt_buf, sizeof(prompt_buf), "File name contains a '%s'.  %s anyway",
			ill, (flags & GE_IS_EXPORT) ? "Export" : "Save");
		prompt_buf[sizeof(prompt_buf)-1] = '\0';
		if(want_to(prompt_buf, 'n', 0, NO_HELP,
		  ((flags & GE_SEQ_SENSITIVE) ? RB_SEQ_SENSITIVE : 0)) != 'y')
		  continue;
	    }
	}

	break;		/* Must have got an OK file name */
    }

    if(VAR_OPER_DIR && !in_dir(VAR_OPER_DIR, full_filename)){
	ret = -2;
	goto done;
    }

    if(!can_access(full_filename, ACCESS_EXISTS)){
	int rbflags;
	static ESCKEY_S access_opts[] = {
	    /* TRANSLATORS: asking user if they want to overwrite (replace contents of)
	       a file or append to the end of the file */
	    {'o', 'o', "O", N_("Overwrite")},
	    {'a', 'a', "A", N_("Append")},
	    {-1, 0, NULL, NULL}};

	rbflags = RB_NORM | ((flags & GE_SEQ_SENSITIVE) ? RB_SEQ_SENSITIVE : 0);

	if(flags & GE_NO_APPEND){
	    r = strlen(filename);
	    snprintf(prompt_buf, sizeof(prompt_buf),
		   /* TRANSLATORS: asking user whether to overwrite a file or not,
		      File <filename> already exists. Overwrite it ? */
		   _("File \"%s%s\" already exists. Overwrite it "),
		   (r > 20) ? "..." : "",
		   filename + ((r > 20) ? r - 20 : 0));
	    prompt_buf[sizeof(prompt_buf)-1] = '\0';
	    if(want_to(prompt_buf, 'n', 'x', NO_HELP, rbflags) == 'y'){
		if(rflags)
		  *rflags |= GER_OVER;

		if(our_unlink(full_filename) < 0){
		    q_status_message2(SM_ORDER | SM_DING, 3, 5,
				  /* TRANSLATORS: Cannot remove old <filename>: <error text> */
				  _("Cannot remove old %s: %s"),
				  full_filename, error_description(errno));
		}
	    }
	    else{
		ret = -1;
		goto done;
	    }
	}
	else if(!(flags & GE_IS_IMPORT)){
	    r = strlen(filename);
	    snprintf(prompt_buf, sizeof(prompt_buf),
		   /* TRANSLATORS: File <filename> already exists. Overwrite or append to it ? */
		   _("File \"%s%s\" already exists. Overwrite or append to it ? "),
		   (r > 20) ? "..." : "",
		   filename + ((r > 20) ? r - 20 : 0));
	    prompt_buf[sizeof(prompt_buf)-1] = '\0';
	    switch(radio_buttons(prompt_buf, -FOOTER_ROWS(ps_global),
				 access_opts, 'a', 'x', NO_HELP, rbflags)){
	      case 'o' :
		if(rflags)
		  *rflags |= GER_OVER;

		if(our_truncate(full_filename, (off_t)0) < 0)
		  /* trouble truncating, but we'll give it a try anyway */
		  q_status_message2(SM_ORDER | SM_DING, 3, 5,
				  /* TRANSLATORS: Warning: Cannot truncate old <filename>: <error text> */
				  _("Warning: Cannot truncate old %s: %s"),
				  full_filename, error_description(errno));
		break;

	      case 'a' :
		if(rflags)
		  *rflags |= GER_APPEND;

		break;

	      case 'x' :
	      default :
		ret = -1;
		goto done;
	    }
	}
    }

done:
    if(history && ret == 0)
      save_hist(*history, full_filename, 0, NULL);

    if(opts && opts != optsarg)
      fs_give((void **) &opts);

    return(ret);
}


/*----------------------------------------------------------------------
  parse the config'd upload/download command

  Args: cmd -- buffer to return command fit for shellin'
	prefix --
	cfg_str --
	fname -- file name to build into the command

  Returns: pointer to cmd_str buffer or NULL on real bad error

  NOTE: One SIDE EFFECT is that any defined "prefix" string in the
	cfg_str is written to standard out right before a successful
	return of this function.  The call immediately following this
	function darn well better be the shell exec...
 ----*/
char *
build_updown_cmd(char *cmd, size_t cmdlen, char *prefix, char *cfg_str, char *fname)
{
    char *p;
    int   fname_found = 0;

    if(prefix && *prefix){
	/* loop thru replacing all occurances of _FILE_ */
	p = strncpy(cmd, prefix, cmdlen);
	cmd[cmdlen-1] = '\0';
	while((p = strstr(p, "_FILE_")))
	  rplstr(p, cmdlen-(p-cmd), 6, fname);

	fputs(cmd, stdout);
    }

    /* loop thru replacing all occurances of _FILE_ */
    p = strncpy(cmd, cfg_str, cmdlen);
    cmd[cmdlen-1] = '\0';
    while((p = strstr(p, "_FILE_"))){
	rplstr(p, cmdlen-(p-cmd), 6, fname);
	fname_found = 1;
    }

    if(!fname_found)
      snprintf(cmd+strlen(cmd), cmdlen-strlen(cmd), " %s", fname);

    cmd[cmdlen-1] = '\0';

    dprint((4, "\n - build_updown_cmd = \"%s\" -\n",
	   cmd ? cmd : "?"));
    return(cmd);
}


/*----------------------------------------------------------------------
  Write a berzerk format message delimiter using the given putc function

    Args: e -- envelope of message to write
	  pc -- function to use 

    Returns: TRUE if we could write it, FALSE if there was a problem

    NOTE: follows delimiter with OS-dependent newline
 ----*/
int
bezerk_delimiter(ENVELOPE *env, MESSAGECACHE *mc, gf_io_t pc, int leading_newline)
{
    MESSAGECACHE telt;
    time_t       when;
    char        *p;
    
    /* write "[\n]From mailbox[@host] " */
    if(!((leading_newline ? gf_puts(NEWLINE, pc) : 1)
	 && gf_puts("From ", pc)
	 && gf_puts((env && env->from) ? env->from->mailbox
				       : "the-concourse-on-high", pc)
	 && gf_puts((env && env->from && env->from->host) ? "@" : "", pc)
	 && gf_puts((env && env->from && env->from->host) ? env->from->host
							  : "", pc)
	 && (*pc)(' ')))
      return(0);

    if(mc && mc->valid)
      when = mail_longdate(mc);
    else if(env && env->date && env->date[0]
	    && mail_parse_date(&telt,env->date))
      when = mail_longdate(&telt);
    else
      when = time(0);

    p = ctime(&when);

    while(p && *p && *p != '\n')	/* write date */
      if(!(*pc)(*p++))
	return(0);

    if(!gf_puts(NEWLINE, pc))		/* write terminating newline */
      return(0);

    return(1);
}


/*----------------------------------------------------------------------
      Execute command to jump to a given message number

    Args: qline -- Line to ask question on

  Result: returns true if the use selected a new message, false otherwise

 ----*/
long
jump_to(MSGNO_S *msgmap, int qline, UCS first_num, SCROLL_S *sparms, CmdWhere in_index)
{
    char     jump_num_string[80], *j, prompt[70];
    HelpType help;
    int      rc;
    static ESCKEY_S jump_to_key[] = { {0, 0, NULL, NULL},
				      /* TRANSLATORS: go to First Message */
				      {ctrl('Y'), 10, "^Y", N_("First Msg")},
				      {ctrl('V'), 11, "^V", N_("Last Msg")},
				      {-1, 0, NULL, NULL} };

    dprint((4, "\n - jump_to -\n"));

#ifdef DEBUG
    if(sparms && sparms->jump_is_debug)
      return(get_level(qline, first_num, sparms));
#endif

    if(!any_messages(msgmap, NULL, "to Jump to"))
      return(0L);

    if(first_num && first_num < 0x80 && isdigit((unsigned char) first_num)){
	jump_num_string[0] = first_num;
	jump_num_string[1] = '\0';
    }
    else
      jump_num_string[0] = '\0';

    if(mn_total_cur(msgmap) > 1L){
	snprintf(prompt, sizeof(prompt), "Unselect %s msgs in favor of number to be entered", 
		comatose(mn_total_cur(msgmap)));
	prompt[sizeof(prompt)-1] = '\0';
	if((rc = want_to(prompt, 'n', 0, NO_HELP, WT_NORM)) == 'n')
	  return(0L);
    }

    snprintf(prompt, sizeof(prompt), "%s number to jump to : ", in_index == ThrdIndx
						    ? "Thread"
						    : "Message");
    prompt[sizeof(prompt)-1] = '\0';

    help = NO_HELP;
    while(1){
	int flags = OE_APPEND_CURRENT;

        rc = optionally_enter(jump_num_string, qline, 0,
                              sizeof(jump_num_string), prompt,
                              jump_to_key, help, &flags);
        if(rc == 3){
            help = help == NO_HELP
			? (in_index == ThrdIndx ? h_oe_jump_thd : h_oe_jump)
			: NO_HELP;
            continue;
        }
	else if(rc == 10 || rc == 11){
	    char warning[100];
	    long closest;

	    closest = closest_jump_target(rc == 10 ? 1L
					  : ((in_index == ThrdIndx)
					     ? msgmap->max_thrdno
					     : mn_get_total(msgmap)),
					  ps_global->mail_stream,
					  msgmap, 0,
					  in_index, warning, sizeof(warning));
	    /* ignore warning */
	    return(closest);
	}

	/*
	 * If we take out the *jump_num_string nonempty test in this if
	 * then the closest_jump_target routine will offer a jump to the
	 * last message. However, it is slow because you have to wait for
	 * the status message and it is annoying for people who hit J command
	 * by mistake and just want to hit return to do nothing, like has
	 * always worked. So the test is there for now. Hubert 2002-08-19
	 *
	 * Jumping to first/last message is now possible through ^Y/^V 
	 * commands above. jpf 2002-08-21
	 * (and through "end" hubert 2006-07-07)
	 */
        if(rc == 0 && *jump_num_string != '\0'){
	    removing_leading_and_trailing_white_space(jump_num_string);
            for(j=jump_num_string; isdigit((unsigned char)*j) || *j=='-'; j++)
	      ;

	    if(*j != '\0'){
		if(!strucmp("end", j))
		  return((in_index == ThrdIndx) ? msgmap->max_thrdno : mn_get_total(msgmap));

	        q_status_message(SM_ORDER | SM_DING, 2, 2,
                           _("Invalid number entered. Use only digits 0-9"));
		jump_num_string[0] = '\0';
	    }
	    else{
		char warning[100];
		long closest, jump_num;

		if(*jump_num_string)
		  jump_num = atol(jump_num_string);
		else
		  jump_num = -1L;

		warning[0] = '\0';
		closest = closest_jump_target(jump_num, ps_global->mail_stream,
					      msgmap,
					      *jump_num_string ? 0 : 1,
					      in_index, warning, sizeof(warning));
		if(warning[0])
		  q_status_message(SM_ORDER | SM_DING, 2, 2, warning);

		if(closest == jump_num)
		  return(jump_num);

		if(closest == 0L)
		  jump_num_string[0] = '\0';
		else
		  strncpy(jump_num_string, long2string(closest),
			  sizeof(jump_num_string));
            }

            continue;
	}

        if(rc != 4)
          break;
    }

    return(0L);
}


/*
 * cmd_delete_action - handle msgno advance and such after single message deletion
 */
char *
cmd_delete_action(struct pine *state, MSGNO_S *msgmap, CmdWhere in_index)
{
    int	  opts;
    long  msgno;
    char *rv = NULL;

    msgno = mn_get_cur(msgmap);
    advance_cur_after_delete(state, state->mail_stream, msgmap, in_index);

    if(IS_NEWS(state->mail_stream)
       || ((state->context_current->use & CNTXT_INCMNG)
	   && context_isambig(state->cur_folder))){

	opts = (NSF_TRUST_FLAGS | NSF_SKIP_CHID);
	if(in_index == View)
	  opts &= ~NSF_SKIP_CHID;

	(void)next_sorted_flagged(F_UNDEL|F_UNSEEN, state->mail_stream, msgno, &opts);
	if(!(opts & NSF_FLAG_MATCH)){
	    char nextfolder[MAXPATH];

	    strncpy(nextfolder, state->cur_folder, sizeof(nextfolder));
	    nextfolder[sizeof(nextfolder)-1] = '\0';
	    rv = next_folder(NULL, nextfolder, sizeof(nextfolder), nextfolder,
			     state->context_current, NULL, NULL)
		   ? ".  Press TAB for next folder."
		   : ".  No more folders to TAB to.";
	}
    }

    return(rv);
}


/*
 * cmd_delete_index - fixup msgmap or whatever after cmd_delete has done it's thing
 */
char *
cmd_delete_index(struct pine *state, MSGNO_S *msgmap)
{
    return(cmd_delete_action(state, msgmap,MsgIndx));
}

/*
 * cmd_delete_view - fixup msgmap or whatever after cmd_delete has done it's thing
 */
char *
cmd_delete_view(struct pine *state, MSGNO_S *msgmap)
{
    return(cmd_delete_action(state, msgmap, View));
}


void
advance_cur_after_delete(struct pine *state, MAILSTREAM *stream, MSGNO_S *msgmap, CmdWhere in_index)
{
    long new_msgno, msgno;
    int  opts;

    new_msgno = msgno = mn_get_cur(msgmap);
    opts = NSF_TRUST_FLAGS;

    if(F_ON(F_DEL_SKIPS_DEL, state)){

	if(THREADING() && sp_viewing_a_thread(stream))
	  opts |= NSF_SKIP_CHID;

	new_msgno = next_sorted_flagged(F_UNDEL, stream, msgno, &opts);
    }
    else{
	mn_inc_cur(stream, msgmap,
		   (in_index == View && THREADING()
		    && sp_viewing_a_thread(stream))
		     ? MH_THISTHD
		     : (in_index == View)
		       ? MH_ANYTHD : MH_NONE);
	new_msgno = mn_get_cur(msgmap);
	if(new_msgno != msgno)
	  opts |= NSF_FLAG_MATCH;
    }

    /*
     * Viewing_a_thread is the complicated case because we want to ignore
     * other threads at first and then look in other threads if we have to.
     * By ignoring other threads we also ignore collapsed partial threads
     * in our own thread.
     */
    if(THREADING() && sp_viewing_a_thread(stream) && !(opts & NSF_FLAG_MATCH)){
	long rawno, orig_thrdno;
	PINETHRD_S *thrd, *topthrd = NULL;

	rawno = mn_m2raw(msgmap, msgno);
	thrd  = fetch_thread(stream, rawno);
	if(thrd && thrd->top)
	  topthrd = fetch_thread(stream, thrd->top);

	orig_thrdno = topthrd ? topthrd->thrdno : -1L;

	opts = NSF_TRUST_FLAGS;
	new_msgno = next_sorted_flagged(F_UNDEL, stream, msgno, &opts);

	/*
	 * If we got a match, new_msgno may be a message in
	 * a different thread from the one we are viewing, or it could be
	 * in a collapsed part of this thread.
	 */
	if(opts & NSF_FLAG_MATCH){
	    int         ret;
	    char        pmt[128];

	    topthrd = NULL;
	    thrd = fetch_thread(stream, mn_m2raw(msgmap,new_msgno));
	    if(thrd && thrd->top)
	      topthrd = fetch_thread(stream, thrd->top);
	    
	    /*
	     * If this match is in the same thread we're already in
	     * then we're done, else we have to ask the user and maybe
	     * switch threads.
	     */
	    if(!(orig_thrdno > 0L && topthrd
		 && topthrd->thrdno == orig_thrdno)){

		if(F_OFF(F_AUTO_OPEN_NEXT_UNREAD, state)){
		    if(in_index == View)
		      snprintf(pmt, sizeof(pmt),
			     "View message in thread number %.10s",
			     topthrd ? comatose(topthrd->thrdno) : "?");
		    else
		      snprintf(pmt, sizeof(pmt), "View thread number %.10s",
			     topthrd ? comatose(topthrd->thrdno) : "?");
			    
		    ret = want_to(pmt, 'y', 'x', NO_HELP, WT_NORM);
		}
		else
		  ret = 'y';

		if(ret == 'y'){
		    unview_thread(state, stream, msgmap);
		    mn_set_cur(msgmap, new_msgno);
		    if(THRD_AUTO_VIEW()
		       && (count_lflags_in_thread(stream, topthrd, msgmap,
						  MN_NONE) == 1)
		       && view_thread(state, stream, msgmap, 1)){
			if(current_index_state)
			  msgmap->top_after_thrd = current_index_state->msg_at_top;

			state->view_skipped_index = 1;
			state->next_screen = mail_view_screen;
		    }
		    else{
			view_thread(state, stream, msgmap, 1);
			if(current_index_state)
			  msgmap->top_after_thrd = current_index_state->msg_at_top;

			state->next_screen = SCREEN_FUN_NULL;
		    }
		}
		else
		  new_msgno = msgno;	/* stick with original */
	    }
	}
    }

    mn_set_cur(msgmap, new_msgno);
    if(in_index != View)
      adjust_cur_to_visible(stream, msgmap);
}


#ifdef DEBUG
long
get_level(int qline, UCS first_num, SCROLL_S *sparms)
{
    char     debug_num_string[80], *j, prompt[70];
    HelpType help;
    int      rc;
    long     debug_num;

    if(first_num && first_num < 0x80 && isdigit((unsigned char)first_num)){
	debug_num_string[0] = first_num;
	debug_num_string[1] = '\0';
	debug_num = atol(debug_num_string);
	*(int *)(sparms->proc.data.p) = debug_num;
	q_status_message1(SM_ORDER, 0, 3, "Show debug <= level %s",
			  comatose(debug_num));
	return(1L);
    }
    else
      debug_num_string[0] = '\0';

    snprintf(prompt, sizeof(prompt), "Show debug <= this level (0-%d) : ", MAX(debug, 9));
    prompt[sizeof(prompt)-1] = '\0';

    help = NO_HELP;
    while(1){
	int flags = OE_APPEND_CURRENT;

        rc = optionally_enter(debug_num_string, qline, 0,
                              sizeof(debug_num_string), prompt,
                              NULL, help, &flags);
        if(rc == 3){
            help = help == NO_HELP ? h_oe_debuglevel : NO_HELP;
            continue;
        }

        if(rc == 0){
	    removing_leading_and_trailing_white_space(debug_num_string);
            for(j=debug_num_string; isdigit((unsigned char)*j); j++)
	      ;

	    if(*j != '\0'){
	        q_status_message(SM_ORDER | SM_DING, 2, 2,
                           _("Invalid number entered. Use only digits 0-9"));
		debug_num_string[0] = '\0';
	    }
	    else{
		debug_num = atol(debug_num_string);
		if(debug_num < 0)
	          q_status_message(SM_ORDER | SM_DING, 2, 2,
				   _("Number should be >= 0"));
		else if(debug_num > MAX(debug,9))
	          q_status_message1(SM_ORDER | SM_DING, 2, 2,
				   _("Maximum is %s"), comatose(MAX(debug,9)));
		else{
		    *(int *)(sparms->proc.data.p) = debug_num;
		    q_status_message1(SM_ORDER, 0, 3,
				      "Show debug <= level %s",
				      comatose(debug_num));
		    return(1L);
		}
            }

            continue;
	}

        if(rc != 4)
          break;
    }

    return(0L);
}
#endif /* DEBUG */


/*
 * Returns the message number closest to target that isn't hidden.
 * Make warning at least 100 chars.
 * A return of 0 means there is no message to jump to.
 */
long
closest_jump_target(long int target, MAILSTREAM *stream, MSGNO_S *msgmap, int no_target, CmdWhere in_index, char *warning, size_t warninglen)
{
    long i, start, closest = 0L;
    char buf[80];
    long maxnum;

    warning[0] = '\0';
    maxnum = (in_index == ThrdIndx) ? msgmap->max_thrdno : mn_get_total(msgmap);

    if(no_target){
	target = maxnum;
	start = 1L;
	snprintf(warning, warninglen,  "No %s number entered, jump to end? ",
		(in_index == ThrdIndx) ? "thread" : "message");
	warning[warninglen-1] = '\0';
    }
    else if(target < 1L)
      start = 1L - target;
    else if(target > maxnum)
      start = target - maxnum;
    else
      start = 1L;

    if(target > 0L && target <= maxnum)
      if(in_index == ThrdIndx
	 || !msgline_hidden(stream, msgmap, target, 0))
	return(target);

    for(i = start; target+i <= maxnum || target-i > 0L; i++){

	if(target+i > 0L && target+i <= maxnum &&
	   (in_index == ThrdIndx
	    || !msgline_hidden(stream, msgmap, target+i, 0))){
	    closest = target+i;
	    break;
	}

	if(target-i > 0L && target-i <= maxnum &&
	   (in_index == ThrdIndx
	    || !msgline_hidden(stream, msgmap, target-i, 0))){
	    closest = target-i;
	    break;
	}
    }

    strncpy(buf, long2string(closest), sizeof(buf));
    buf[sizeof(buf)-1] = '\0';

    if(closest == 0L)
      strncpy(warning, "Nothing to jump to", warninglen);
    else if(target < 1L)
      snprintf(warning, warninglen, "%s number (%s) must be at least %s",
	      (in_index == ThrdIndx) ? "Thread" : "Message",
	      long2string(target), buf);
    else if(target > maxnum)
      snprintf(warning, warninglen, "%s number (%s) may be no more than %s",
	      (in_index == ThrdIndx) ? "Thread" : "Message",
	      long2string(target), buf);
    else if(!no_target)
      snprintf(warning, warninglen,
	"Message number (%s) is not in \"Zoomed Index\" - Closest is(%s)",
	long2string(target), buf);

    warning[warninglen-1] = '\0';

    return(closest);
}


/*----------------------------------------------------------------------
     Prompt for folder name to open, expand the name and return it

   Args: qline      -- Screen line to prompt on
         allow_list -- if 1, allow ^T to bring up collection lister

 Result: returns the folder name or NULL
         pine structure mangled_footer flag is set
         may call the collection lister in which case mangled screen will be set

 This prompts the user for the folder to open, possibly calling up
the collection lister if the user types ^T.
----------------------------------------------------------------------*/
char *
broach_folder(int qline, int allow_list, int *notrealinbox, CONTEXT_S **context)
{
    HelpType	help;
    static char newfolder[MAILTMPLEN];
    char        expanded[MAXPATH+1],
                prompt[MAX_SCREEN_COLS+1],
               *last_folder, *p;
    static HISTORY_S *history = NULL;
    CONTEXT_S  *tc, *tc2;
    ESCKEY_S    ekey[9];
    int		rc, r, ku = -1, n, flags, last_rc = 0, inbox, done = 0;

    /*
     * the idea is to provide a clue for the context the file name
     * will be saved in (if a non-imap names is typed), and to
     * only show the previous if it was also in the same context
     */
    help	   = NO_HELP;
    *expanded	   = '\0';
    *newfolder	   = '\0';
    last_folder	   = NULL;
    if(notrealinbox)
      (*notrealinbox) = 1;

    init_hist(&history, HISTSIZE);

    tc = broach_get_folder(context ? *context : NULL, &inbox, NULL);

    /* set up extra command option keys */
    rc = 0;
    ekey[rc].ch	     = (allow_list) ? ctrl('T') : 0 ;
    ekey[rc].rval    = (allow_list) ? 2 : 0;
    ekey[rc].name    = (allow_list) ? "^T" : "";
    ekey[rc++].label = (allow_list) ? N_("ToFldrs") : "";

    if(ps_global->context_list->next){
	ekey[rc].ch      = ctrl('P');
	ekey[rc].rval    = 10;
	ekey[rc].name    = "^P";
	ekey[rc++].label = N_("Prev Collection");

	ekey[rc].ch      = ctrl('N');
	ekey[rc].rval    = 11;
	ekey[rc].name    = "^N";
	ekey[rc++].label = N_("Next Collection");
    }

    ekey[rc].ch      = ctrl('W');
    ekey[rc].rval    = 17;
    ekey[rc].name    = "^W";
    ekey[rc++].label = N_("INBOX");

    if(F_ON(F_ENABLE_TAB_COMPLETE,ps_global)){
	ekey[rc].ch      = TAB;
	ekey[rc].rval    = 12;
	ekey[rc].name    = "TAB";
	ekey[rc++].label = N_("Complete");
    }

    if(F_ON(F_ENABLE_SUB_LISTS, ps_global)){
	ekey[rc].ch      = ctrl('X');
	ekey[rc].rval    = 14;
	ekey[rc].name    = "^X";
	ekey[rc++].label = N_("ListMatches");
    }

    if(ps_global->context_list->next && F_ON(F_DISABLE_SAVE_INPUT_HISTORY, ps_global)){
	ekey[rc].ch      = KEY_UP;
	ekey[rc].rval    = 10;
	ekey[rc].name    = "";
	ekey[rc++].label = "";

	ekey[rc].ch      = KEY_DOWN;
	ekey[rc].rval    = 11;
	ekey[rc].name    = "";
	ekey[rc++].label = "";
    }
    else if(F_OFF(F_DISABLE_SAVE_INPUT_HISTORY, ps_global)){
	ekey[rc].ch      = KEY_UP;
	ekey[rc].rval    = 30;
	ekey[rc].name    = "";
	ku = rc;
	ekey[rc++].label = "";

	ekey[rc].ch      = KEY_DOWN;
	ekey[rc].rval    = 31;
	ekey[rc].name    = "";
	ekey[rc++].label = "";
    }

    ekey[rc].ch = -1;

    while(!done) {
	/*
	 * Figure out next default value for this context.  The idea
	 * is that in each context the last folder opened is cached.
	 * It's up to pick it out and display it.  This is fine
	 * and dandy if we've currently got the inbox open, BUT
	 * if not, make the inbox the default the first time thru.
	 */
	if(!inbox){
	    last_folder = ps_global->inbox_name;
	    inbox = 1;		/* pretend we're in inbox from here on out */
	}
	else
	  last_folder = (ps_global->last_unambig_folder[0])
			  ? ps_global->last_unambig_folder
			  : ((tc->last_folder[0]) ? tc->last_folder : NULL);

	if(last_folder)
	  snprintf(expanded, sizeof(expanded), " [%.*s]", sizeof(expanded)-5, last_folder);
	else
	  *expanded = '\0';

	expanded[sizeof(expanded)-1] = '\0';

	/* only show collection number if more than one available */
	if(ps_global->context_list->next)
	  snprintf(prompt, sizeof(prompt), "GOTO %s in <%s> %.*s%s: ",
		    NEWS_TEST(tc) ? "news group" : "folder",
		    tc->nickname, sizeof(prompt)-50, expanded,
		    *expanded ? " " : "");
	else
	  snprintf(prompt, sizeof(prompt), "GOTO folder %.*s%s: ", sizeof(prompt)-20, expanded,
		  *expanded ? " " : "");

	prompt[sizeof(prompt)-1] = '\0';

	if(utf8_width(prompt) > MAXPROMPT){
	    if(ps_global->context_list->next)
	      snprintf(prompt, sizeof(prompt), "GOTO <%s> %.*s%s: ",
			tc->nickname, sizeof(prompt)-50, expanded,
			*expanded ? " " : "");
	    else
	      snprintf(prompt, sizeof(prompt), "GOTO %.*s%s: ", sizeof(prompt)-20, expanded,
		      *expanded ? " " : "");

	    prompt[sizeof(prompt)-1] = '\0';

	    if(utf8_width(prompt) > MAXPROMPT){
		if(ps_global->context_list->next)
		  snprintf(prompt, sizeof(prompt), "<%s> %.*s%s: ",
			    tc->nickname, sizeof(prompt)-50, expanded,
			    *expanded ? " " : "");
		else
		  snprintf(prompt, sizeof(prompt), "%.*s%s: ", sizeof(prompt)-20, expanded,
			  *expanded ? " " : "");

		prompt[sizeof(prompt)-1] = '\0';
	    }
	}

	if(ku >= 0){
	    if(items_in_hist(history) > 1){
		ekey[ku].name  = HISTORY_UP_KEYNAME;
		ekey[ku].label = HISTORY_KEYLABEL;
		ekey[ku+1].name  = HISTORY_DOWN_KEYNAME;
		ekey[ku+1].label = HISTORY_KEYLABEL;
	    }
	    else{
		ekey[ku].name  = "";
		ekey[ku].label = "";
		ekey[ku+1].name  = "";
		ekey[ku+1].label = "";
	    }
	}

	flags = OE_APPEND_CURRENT;
        rc = optionally_enter(newfolder, qline, 0, sizeof(newfolder),
			      prompt, ekey, help, &flags);

	ps_global->mangled_footer = 1;

	switch(rc){
	  case -1 :				/* o_e says error! */
	    q_status_message(SM_ORDER | SM_DING, 3, 3,
			     _("Error reading folder name"));
	    return(NULL);

	  case 0 :				/* o_e says normal entry */
	    removing_trailing_white_space(newfolder);
	    removing_leading_white_space(newfolder);

	    if(*newfolder){
		char *name, *fullname = NULL;
		int   exists, breakout = 0;

		save_hist(history, newfolder, 0, tc);

		if(!(name = folder_is_nick(newfolder, FOLDERS(tc),
					   FN_WHOLE_NAME)))
		  name = newfolder;

		if(update_folder_spec(expanded, sizeof(expanded), name)){
		    strncpy(name = newfolder, expanded, sizeof(newfolder));
		    newfolder[sizeof(newfolder)-1] = '\0';
		}

		exists = folder_name_exists(tc, name, &fullname);

		if(fullname){
		    strncpy(name = newfolder, fullname, sizeof(newfolder));
		    newfolder[sizeof(newfolder)-1] = '\0';
		    fs_give((void **) &fullname);
		    breakout = TRUE;
		}

		/*
		 * if we know the things a folder, open it.
		 * else if we know its a directory, visit it.
		 * else we're not sure (it either doesn't really
		 * exist or its unLISTable) so try opening it anyway
		 */
		if(exists & FEX_ISFILE){
		    done++;
		    break;
		}
		else if((exists & FEX_ISDIR)){
		    if(breakout){
			CONTEXT_S *fake_context;
			char	   tmp[MAILTMPLEN];
			size_t	   l;

			strncpy(tmp, name, sizeof(tmp));
			tmp[sizeof(tmp)-2-1] = '\0';
			if(tmp[(l = strlen(tmp)) - 1] != tc->dir->delim){
			    if(l < sizeof(tmp)){
				tmp[l] = tc->dir->delim;
				strncpy(&tmp[l+1], "[]", sizeof(tmp)-(l+1));
			    }
			}
			else
			  strncat(tmp, "[]", sizeof(tmp)-strlen(tmp)-1);

			tmp[sizeof(tmp)-1] = '\0';

			fake_context = new_context(tmp, 0);
			newfolder[0] = '\0';
			done = display_folder_list(&fake_context, newfolder,
						   1, folders_for_goto);
			free_context(&fake_context);
			break;
		    }
		    else if(!(tc->use & CNTXT_INCMNG)){
			done = display_folder_list(&tc, newfolder,
						   1, folders_for_goto);
			break;
		    }
		}
		else if((exists & FEX_ERROR)){
		    q_status_message1(SM_ORDER, 0, 3,
				      _("Problem accessing folder \"%s\""),
				      newfolder);
		    return(NULL);
		}
		else{
		    done++;
		    break;
		}

		if(exists == FEX_ERROR)
		  q_status_message1(SM_ORDER, 0, 3,
				    _("Problem accessing folder \"%s\""),
				    newfolder);
		else if(tc->use & CNTXT_INCMNG)
		  q_status_message1(SM_ORDER, 0, 3,
				    _("Can't find Incoming Folder: %s"),
				    newfolder);
		else if(context_isambig(newfolder))
		  q_status_message2(SM_ORDER, 0, 3,
				    _("Can't find folder \"%s\" in %s"),
				    newfolder, (void *) tc->nickname);
		else
		  q_status_message1(SM_ORDER, 0, 3,
				    _("Can't find folder \"%s\""),
				    newfolder);

		return(NULL);
	    }
	    else if(last_folder){
		if(ps_global->goto_default_rule == GOTO_FIRST_CLCTN_DEF_INBOX
		   && !strucmp(last_folder, ps_global->inbox_name)
		   && tc == ((ps_global->context_list->use & CNTXT_INCMNG)
		              ? ps_global->context_list->next : ps_global->context_list)){
		    if(notrealinbox)
		      (*notrealinbox) = 0;

		    tc = ps_global->context_list;
		}

		strncpy(newfolder, last_folder, sizeof(newfolder));
		newfolder[sizeof(newfolder)-1] = '\0';
		save_hist(history, newfolder, 0, tc);
		done++;
		break;
	    }
	    /* fall thru like they cancelled */

	  case 1 :				/* o_e says user cancel */
	    cmd_cancelled("Open folder");
	    return(NULL);

	  case 2 :				/* o_e says user wants list */
	    r = display_folder_list(&tc, newfolder, 0, folders_for_goto);
	    if(r)
	      done++;

	    break;

	  case 3 :				/* o_e says user wants help */
	    help = help == NO_HELP ? h_oe_broach : NO_HELP;
	    break;

	  case 4 :				/* redraw */
	    break;
	    
	  case 10 :				/* Previous collection */
	    tc2 = ps_global->context_list;
	    while(tc2->next && tc2->next != tc)
	      tc2 = tc2->next;

	    tc = tc2;
	    break;

	  case 11 :				/* Next collection */
	    tc = (tc->next) ? tc->next : ps_global->context_list;
	    break;

	  case 12 :				/* file name completion */
	    if(!folder_complete(tc, newfolder, sizeof(newfolder), &n)){
		if(n && last_rc == 12 && !(flags & OE_USER_MODIFIED)){
		    r = display_folder_list(&tc, newfolder, 1,folders_for_goto);
		    if(r)
		      done++;			/* bingo! */
		    else
		      rc = 0;			/* burn last_rc */
		}
		else
		  Writechar(BELL, 0);
	    }

	    break;

	  case 14 :				/* file name completion */
	    r = display_folder_list(&tc, newfolder, 2, folders_for_goto);
	    if(r)
	      done++;			/* bingo! */
	    else
	      rc = 0;			/* burn last_rc */

	    break;

	  case 17 :				/* GoTo INBOX */
	    done++;
	    strncpy(newfolder, ps_global->inbox_name, sizeof(newfolder)-1);
	    newfolder[sizeof(newfolder)-1] = '\0';
	    if(notrealinbox)
	      (*notrealinbox) = 0;

	    tc = ps_global->context_list;
	    save_hist(history, newfolder, 0, tc);

	    break;

	  case 30 :
	    if((p = get_prev_hist(history, newfolder, 0, tc)) != NULL){
		strncpy(newfolder, p, sizeof(newfolder));
		newfolder[sizeof(newfolder)-1] = '\0';
		if(history->hist[history->curindex])
		  tc = history->hist[history->curindex]->cntxt;
	    }
	    else
	      Writechar(BELL, 0);

	    break;

	  case 31 :
	    if((p = get_next_hist(history, newfolder, 0, tc)) != NULL){
		strncpy(newfolder, p, sizeof(newfolder));
		newfolder[sizeof(newfolder)-1] = '\0';
		if(history->hist[history->curindex])
		  tc = history->hist[history->curindex]->cntxt;
	    }
	    else
	      Writechar(BELL, 0);

	    break;

	  default :
	    panic("Unhandled case");
	    break;
	}

	last_rc = rc;
    }

    dprint((2, "broach folder, name entered \"%s\"\n",
	   newfolder ? newfolder : "?"));

    /*-- Just check that we can expand this. It gets done for real later --*/
    strncpy(expanded, newfolder, sizeof(expanded));
    expanded[sizeof(expanded)-1] = '\0';

    if(!expand_foldername(expanded, sizeof(expanded))) {
        dprint((1, "Error: Failed on expansion of filename %s (do_broach)\n", 
    	  expanded ? expanded : "?"));
        return(NULL);
    }

    *context = tc;
    return(newfolder);
}


/*----------------------------------------------------------------------
    Check to see if user wants to reopen dead stream.

  Args: ps --
	reopenp -- 

 Result:  1 if the folder was successfully updatedn
          0 if not necessary
      
  ----*/
int
ask_mailbox_reopen(struct pine *ps, int *reopenp)
{
    if(((ps->mail_stream->dtb
	 && ((ps->mail_stream->dtb->flags & DR_NONEWMAIL)
	     || (ps->mail_stream->rdonly
		 && ps->mail_stream->dtb->flags & DR_NONEWMAILRONLY)))
	&& (ps->reopen_rule == REOPEN_ASK_ASK_Y
	    || ps->reopen_rule == REOPEN_ASK_ASK_N
	    || ps->reopen_rule == REOPEN_ASK_NO_Y
	    || ps->reopen_rule == REOPEN_ASK_NO_N))
       || ((ps->mail_stream->dtb
	    && ps->mail_stream->rdonly
	    && !(ps->mail_stream->dtb->flags & DR_LOCAL))
	   && (ps->reopen_rule == REOPEN_YES_ASK_Y
	       || ps->reopen_rule == REOPEN_YES_ASK_N
	       || ps->reopen_rule == REOPEN_ASK_ASK_Y
	       || ps->reopen_rule == REOPEN_ASK_ASK_N))){
	int deefault;

	switch(ps->reopen_rule){
	  case REOPEN_YES_ASK_Y:
	  case REOPEN_ASK_ASK_Y:
	  case REOPEN_ASK_NO_Y:
	    deefault = 'y';
	    break;

	  default:
	    deefault = 'n';
	    break;
	}

	switch(want_to("Re-open folder to check for new messages", deefault,
		       'x', h_reopen_folder, WT_NORM)){
	  case 'y':
	    (*reopenp)++;
	    break;
	    
	  case 'x':
	    return(-1);
	}
    }

    return(0);
}



/*----------------------------------------------------------------------
    Check to see if user input is in form of old c-client mailbox speck

  Args: old --
	new -- 

 Result:  1 if the folder was successfully updatedn
          0 if not necessary
      
  ----*/
int
update_folder_spec(char *new, size_t newlen, char *old)
{
    char *p, *orignew;
    int	  nntp = 0;

    orignew = new;
    if(*(p = old) == '*')		/* old form? */
      old++;

    if(*old == '{')			/* copy host spec */
      do
	switch(*new = *old++){
	  case '\0' :
	    return(FALSE);

	  case '/' :
	    if(!struncmp(old, "nntp", 4))
	      nntp++;

	    break;

	  default :
	    break;
	}
      while(*new++ != '}' && (new-orignew) < newlen-1);

    if((*p == '*' && *old) || ((*old == '*') ? *++old : 0)){
	/*
	 * OK, some heuristics here.  If it looks like a newsgroup
	 * then we plunk it into the #news namespace else we
	 * assume that they're trying to get at a #public folder...
	 */
	for(p = old;
	    *p && (isalnum((unsigned char) *p) || strindex(".-", *p));
	    p++)
	  ;

	sstrncpy(&new, (*p && !nntp) ? "#public/" : "#news.", newlen-(new-orignew));
	strncpy(new, old, newlen-(new-orignew));
	return(TRUE);
    }

    orignew[newlen-1] = '\0';

    return(FALSE);
}


/*----------------------------------------------------------------------
    Open the requested folder in the requested context

    Args: state -- usual pine state struct
	  newfolder -- folder to open
	  new_context -- folder context might live in
	  stream -- candidate for recycling

   Result: New folder open or not (if error), and we're set to
	   enter the index screen.
 ----*/
void
visit_folder(struct pine *state, char *newfolder, CONTEXT_S *new_context,
	     MAILSTREAM *stream, long unsigned int flags)
{
    dprint((9, "visit_folder(%s, %s)\n",
	       newfolder ? newfolder : "?",
	       (new_context && new_context->context)
	         ? new_context->context : "(NULL)"));

    if(ps_global && ps_global->ttyo){
	blank_keymenu(ps_global->ttyo->screen_rows - 2, 0);
	ps_global->mangled_footer = 1;
    }

    if(do_broach_folder(newfolder, new_context, stream ? &stream : NULL,
			flags) >= 0
       || !sp_flagged(state->mail_stream, SP_LOCKED))
      state->next_screen = mail_index_screen;
    else
      state->next_screen = folder_screen;
}


/*----------------------------------------------------------------------
  Move read messages from folder if listed in archive
 
  Args: 

  ----*/
int
read_msg_prompt(long int n, char *f)
{
    char buf[MAX_SCREEN_COLS+1];

    snprintf(buf, sizeof(buf), "Save the %ld read message%s in \"%s\"", n, plural(n), f);
    buf[sizeof(buf)-1] = '\0';
    return(want_to(buf, 'y', 0, NO_HELP, WT_NORM) == 'y');
}


/*----------------------------------------------------------------------
    Print current message[s] or folder index

    Args: state -- pointer to struct holding a bunch of pine state
	 msgmap -- table mapping msg nums to c-client sequence nums
	   aopt -- aggregate options
       in_index -- boolean indicating we're called from Index Screen

 Filters the original header and sends stuff to printer
  ---*/
int
cmd_print(struct pine *state, MSGNO_S *msgmap, int aopt, CmdWhere in_index)
{
    char      prompt[250];
    long      i, msgs, rawno;
    int	      next = 0, do_index = 0, rv = 0;
    ENVELOPE *e;
    BODY     *b;
    MESSAGECACHE *mc;

    if(MCMD_ISAGG(aopt) && !pseudo_selected(state->mail_stream, msgmap))
      return rv;

    msgs = mn_total_cur(msgmap);

    if((in_index != View) && F_ON(F_PRINT_INDEX, state)){
	char m[10];
	int  ans;
	static ESCKEY_S prt_opts[] = {
	    {'i', 'i', "I", N_("Index")},
	    {'m', 'm', "M", NULL},
	    {-1, 0, NULL, NULL}};

	if(in_index == ThrdIndx){
	    /* TRANSLATORS: This is a question, Print Index ? */
	    if(want_to(_("Print Index"), 'y', 'x', NO_HELP, WT_NORM) == 'y')
	      ans = 'i';
	    else
	      ans = 'x';
	}
	else{
	    snprintf(m, sizeof(m), "Message%s", (msgs>1L) ? "s" : "");
	    m[sizeof(m)-1] = '\0';
	    prt_opts[1].label = m;
	    snprintf(prompt, sizeof(prompt), "Print %sFolder Index or %s %s? ",
		(aopt & MCMD_AGG_2) ? "thread " : MCMD_ISAGG(aopt) ? "selected " : "",
		(aopt & MCMD_AGG_2) ? "thread" : MCMD_ISAGG(aopt) ? "selected" : "current", m);
	    prompt[sizeof(prompt)-1] = '\0';

	    ans = radio_buttons(prompt, -FOOTER_ROWS(state), prt_opts, 'm', 'x',
				NO_HELP, RB_NORM|RB_SEQ_SENSITIVE);
	}

	switch(ans){
	  case 'x' :
	    cmd_cancelled("Print");
	    if(MCMD_ISAGG(aopt))
	      restore_selected(msgmap);

	    return rv;

	  case 'i':
	    do_index = 1;
	    break;

	  default :
	  case 'm':
	    break;
	}
    }

    if(do_index)
      snprintf(prompt, sizeof(prompt), "%sFolder Index",
	      (aopt & MCMD_AGG_2) ? "Thread " : MCMD_ISAGG(aopt) ? "Selected " : "");
    else if(msgs > 1L)
      snprintf(prompt, sizeof(prompt), "%s messages", long2string(msgs));
    else
      snprintf(prompt, sizeof(prompt), "Message %s", long2string(mn_get_cur(msgmap)));

    prompt[sizeof(prompt)-1] = '\0';

    if(open_printer(prompt) < 0){
	if(MCMD_ISAGG(aopt))
	  restore_selected(msgmap);

	return rv;
    }

    if(do_index){
	TITLE_S *tc;

	tc = format_titlebar();

	/* Print titlebar... */
	print_text1("%s\n\n", tc ? tc->titlebar_line : "");
	/* then all the index members... */
	if(!print_index(state, msgmap, MCMD_ISAGG(aopt)))
	  q_status_message(SM_ORDER | SM_DING, 3, 3,
			   _("Error printing folder index"));
        else
	  rv++;
    }
    else{
	rv++;
        for(i = mn_first_cur(msgmap); i > 0L; i = mn_next_cur(msgmap), next++){
	    if(next && F_ON(F_AGG_PRINT_FF, state))
	      if(!print_char(FORMFEED)){
		rv = 0;
	        break;
	      }

	    if(!(state->mail_stream
	       && (rawno = mn_m2raw(msgmap, i)) > 0L
	       && rawno <= state->mail_stream->nmsgs
	       && (mc = mail_elt(state->mail_stream, rawno))
	       && mc->valid))
	      mc = NULL;

	    if(!(e=pine_mail_fetchstructure(state->mail_stream,
					    mn_m2raw(msgmap,i),
					    &b))
	       || (F_ON(F_FROM_DELIM_IN_PRINT, ps_global)
		   && !bezerk_delimiter(e, mc, print_char, next))
	       || !format_message(mn_m2raw(msgmap, mn_get_cur(msgmap)),
				  e, b, NULL, FM_NEW_MESS | FM_NOINDENT,
				  print_char)){
	        q_status_message(SM_ORDER | SM_DING, 3, 3,
			       _("Error printing message"));
		rv = 0;
	        break;
	    }
        }
    }

    close_printer();

    if(MCMD_ISAGG(aopt))
      restore_selected(msgmap);

    return rv;
}


/*----------------------------------------------------------------------
    Pipe message text

   Args: state -- various pine state bits
	 msgmap -- Message number mapping table
	 aopt -- option flags

   Filters the original header and sends stuff to specified command
  ---*/
int
cmd_pipe(struct pine *state, MSGNO_S *msgmap, int aopt)
{
    ENVELOPE      *e;
    MESSAGECACHE  *mc;
    BODY	  *b;
    PIPE_S	  *syspipe;
    char          *resultfilename = NULL, prompt[80], *p;
    int            done = 0, rv = 0;
    gf_io_t	   pc;
    int		   fourlabel = -1, j = 0, next = 0, ku;
    int            pipe_rv; /* rv of proc to separate from close_system_pipe rv */
    long           i, rawno;
    unsigned       flagsforhist = 1;	/* raw=8/delimit=4/newpipe=2/capture=1 */
    static HISTORY_S *history = NULL;
    int	           capture = 1, raw = 0, delimit = 0, newpipe = 0;
    char           pipe_command[MAXPATH];
    ESCKEY_S pipe_opt[8];

    if(ps_global->restricted){
	q_status_message(SM_ORDER | SM_DING, 0, 4,
			 "Alpine demo can't pipe messages");
	return rv;
    }
    else if(!any_messages(msgmap, NULL, "to Pipe"))
      return rv;

    pipe_command[0] = '\0';
    init_hist(&history, HISTSIZE);
    flagsforhist = (raw ? 0x8 : 0) +
		    (delimit ? 0x4 : 0) +
		     (newpipe ? 0x2 : 0) +
		      (capture ? 0x1 : 0);
    if((p = get_prev_hist(history, "", flagsforhist, NULL)) != NULL){
	strncpy(pipe_command, p, sizeof(pipe_command));
	pipe_command[sizeof(pipe_command)-1] = '\0';
	if(history->hist[history->curindex]){
	    flagsforhist = history->hist[history->curindex]->flags;
	    raw     = (flagsforhist & 0x8) ? 1 : 0;
	    delimit = (flagsforhist & 0x4) ? 1 : 0;
	    newpipe = (flagsforhist & 0x2) ? 1 : 0;
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

    pipe_opt[j].ch    = ctrl('R');
    pipe_opt[j].rval  = 12;
    pipe_opt[j].name  = "^R";
    pipe_opt[j++].label = NULL;

    if(MCMD_ISAGG(aopt)){
	if(!pseudo_selected(state->mail_stream, msgmap))
	  return rv;
	else{
	    fourlabel = j;
	    pipe_opt[j].ch    = ctrl('T');
	    pipe_opt[j].rval  = 13;
	    pipe_opt[j].name  = "^T";
	    pipe_opt[j++].label = NULL;
	}
    }

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

    while (!done) {
	int flags;

	snprintf(prompt, sizeof(prompt), "Pipe %smessage%s%s to %s%s%s%s%s%s%s: ",
		raw ? "RAW " : "",
		MCMD_ISAGG(aopt) ? "s" : " ",
		MCMD_ISAGG(aopt) ? "" : comatose(mn_get_cur(msgmap)),
		(!capture || delimit || (newpipe && MCMD_ISAGG(aopt))) ? "(" : "",
		capture ? "" : "uncaptured",
		(!capture && delimit) ? "," : "",
		delimit ? "delimited" : "",
		((!capture || delimit) && newpipe && MCMD_ISAGG(aopt)) ? "," : "",
		(newpipe && MCMD_ISAGG(aopt)) ? "new pipe" : "",
		(!capture || delimit || (newpipe && MCMD_ISAGG(aopt))) ? ") " : "");
	prompt[sizeof(prompt)-1] = '\0';
	pipe_opt[1].label = raw ? N_("Shown Text") : N_("Raw Text");
	pipe_opt[2].label = capture ? N_("Free Output") : N_("Capture Output");
	pipe_opt[3].label = delimit ? N_("No Delimiter") : N_("With Delimiter");
	if(fourlabel > 0)
	  pipe_opt[fourlabel].label = newpipe ? N_("To Same Pipe") : N_("To Individual Pipes");


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
	switch(optionally_enter(pipe_command, -FOOTER_ROWS(state), 0,
				sizeof(pipe_command), prompt,
				pipe_opt, NO_HELP, &flags)){
	  case -1 :
	    q_status_message(SM_ORDER | SM_DING, 3, 4,
			     _("Internal problem encountered"));
	    done++;
	    break;
      
	  case 10 :			/* flip raw bit */
	    raw = !raw;
	    break;

	  case 11 :			/* flip capture bit */
	    capture = !capture;
	    break;

	  case 12 :			/* flip delimit bit */
	    delimit = !delimit;
	    break;

	  case 13 :			/* flip newpipe bit */
	    newpipe = !newpipe;
	    break;

	  case 30 :
	    flagsforhist = (raw ? 0x8 : 0) +
	                    (delimit ? 0x4 : 0) +
			     (newpipe ? 0x2 : 0) +
			      (capture ? 0x1 : 0);
	    if((p = get_prev_hist(history, pipe_command, flagsforhist, NULL)) != NULL){
		strncpy(pipe_command, p, sizeof(pipe_command));
		pipe_command[sizeof(pipe_command)-1] = '\0';
		if(history->hist[history->curindex]){
		    flagsforhist = history->hist[history->curindex]->flags;
		    raw     = (flagsforhist & 0x8) ? 1 : 0;
		    delimit = (flagsforhist & 0x4) ? 1 : 0;
		    newpipe = (flagsforhist & 0x2) ? 1 : 0;
		    capture = (flagsforhist & 0x1) ? 1 : 0;
		}
	    }
	    else
	      Writechar(BELL, 0);

	    break;

	  case 31 :
	    flagsforhist = (raw ? 0x8 : 0) +
	                    (delimit ? 0x4 : 0) +
			     (newpipe ? 0x2 : 0) +
			      (capture ? 0x1 : 0);
	    if((p = get_next_hist(history, pipe_command, flagsforhist, NULL)) != NULL){
		strncpy(pipe_command, p, sizeof(pipe_command));
		pipe_command[sizeof(pipe_command)-1] = '\0';
		if(history->hist[history->curindex]){
		    flagsforhist = history->hist[history->curindex]->flags;
		    raw     = (flagsforhist & 0x8) ? 1 : 0;
		    delimit = (flagsforhist & 0x4) ? 1 : 0;
		    newpipe = (flagsforhist & 0x2) ? 1 : 0;
		    capture = (flagsforhist & 0x1) ? 1 : 0;
		}
	    }
	    else
	      Writechar(BELL, 0);

	    break;

	  case 0 :
	    if(pipe_command[0]){

		flagsforhist = (raw ? 0x8 : 0) +
				(delimit ? 0x4 : 0) +
				 (newpipe ? 0x2 : 0) +
				  (capture ? 0x1 : 0);
		save_hist(history, pipe_command, flagsforhist, NULL);

		flags = PIPE_USER | PIPE_WRITE | PIPE_STDERR;
		flags |= (raw ? PIPE_RAW : 0);
		if(!capture){
#ifndef	_WINDOWS
		    ClearScreen();
		    fflush(stdout);
		    clear_cursor_pos();
		    ps_global->mangled_screen = 1;
		    ps_global->in_init_seq = 1;
#endif
		    flags |= PIPE_RESET;
		}

		if(!newpipe && !(syspipe = cmd_pipe_open(pipe_command,
							 (flags & PIPE_RESET)
							   ? NULL
							   : &resultfilename,
							 flags, &pc)))
		  done++;

		for(i = mn_first_cur(msgmap);
		    i > 0L && !done;
		    i = mn_next_cur(msgmap)){
		    e = pine_mail_fetchstructure(ps_global->mail_stream,
						 mn_m2raw(msgmap, i), &b);
		    if(!(state->mail_stream
		       && (rawno = mn_m2raw(msgmap, i)) > 0L
		       && rawno <= state->mail_stream->nmsgs
		       && (mc = mail_elt(state->mail_stream, rawno))
		       && mc->valid))
		      mc = NULL;

		    if((newpipe
			&& !(syspipe = cmd_pipe_open(pipe_command,
						     (flags & PIPE_RESET)
						       ? NULL
						       : &resultfilename,
						     flags, &pc)))
		       || (delimit && !bezerk_delimiter(e, mc, pc, next++)))
		      done++;

		    if(!done){
			if(raw){
			    char    *pipe_err;

			    prime_raw_pipe_getc(ps_global->mail_stream,
						mn_m2raw(msgmap, i), -1L, 0L);
			    gf_filter_init();
			    gf_link_filter(gf_nvtnl_local, NULL);
			    if((pipe_err = gf_pipe(raw_pipe_getc, pc)) != NULL){
				q_status_message1(SM_ORDER|SM_DING,
						  3, 3,
						  _("Internal Error: %s"),
						  pipe_err);
				done++;
			    }
			}
			else if(!format_message(mn_m2raw(msgmap, i), e, b,
						NULL, FM_NEW_MESS | FM_NOWRAP, pc))
			  done++;
		    }

		    if(newpipe)
		      if(close_system_pipe(&syspipe, &pipe_rv, pipe_callback) == -1)
			done++;
		}

		if(!capture)
		  ps_global->in_init_seq = 0;

		if(!newpipe)
		  if(close_system_pipe(&syspipe, &pipe_rv, pipe_callback) == -1)
		    done++;
		if(done)		/* say we had a problem */
		  q_status_message(SM_ORDER | SM_DING, 3, 3,
				   _("Error piping message"));
		else if(resultfilename){
		    rv++;
		    /* only display if no error */
		    display_output_file(resultfilename, "PIPE MESSAGE",
					NULL, DOF_EMPTY);
		    fs_give((void **)&resultfilename);
		}
		else{
		  rv++;
		  q_status_message(SM_ORDER, 0, 2, _("Pipe command completed"));
		}

		done++;
		break;
	    }
	    /* else fall thru as if cancelled */

	  case 1 :
	    cmd_cancelled("Pipe command");
	    done++;
	    break;

	  case 3 :
	    helper(h_common_pipe, _("HELP FOR PIPE COMMAND"), HLPD_SIMPLE);
	    ps_global->mangled_screen = 1;
	    break;

	  case 2 :                              /* no place to escape to */
	  case 4 :                              /* can't suspend */
	  default :
	    break;   
	}
    }

    ps_global->mangled_footer = 1;
    if(MCMD_ISAGG(aopt))
      restore_selected(msgmap);

    return rv;
}


/*----------------------------------------------------------------------
  Screen to offer list management commands contained in message

    Args: state -- pointer to struct holding a bunch of pine state
	 msgmap -- table mapping msg nums to c-client sequence nums
	   aopt -- aggregate options

 Result: 

   NOTE: Inspired by contrib from Jeremy Blackman <loki@maison-otaku.net>
 ----*/
void
rfc2369_display(MAILSTREAM *stream, MSGNO_S *msgmap, long int msgno)
{
    int	       winner = 0;
    char      *h, *hdrs[MLCMD_COUNT + 1];
    long       index_no = mn_raw2m(msgmap, msgno);
    RFC2369_S  data[MLCMD_COUNT];

    /* for each header field */
    if((h = pine_fetchheader_lines(stream, msgno, NULL, rfc2369_hdrs(hdrs))) != NULL){
	memset(&data[0], 0, sizeof(RFC2369_S) * MLCMD_COUNT);
	if(rfc2369_parse_fields(h, &data[0])){
	    STORE_S *explain;

	    if((explain = list_mgmt_text(data, index_no)) != NULL){
		list_mgmt_screen(explain);
		ps_global->mangled_screen = 1;
		so_give(&explain);
		winner++;
	    }
	}

	fs_give((void **) &h);
    }

    if(!winner)
      q_status_message1(SM_ORDER, 0, 3,
		    "Message %s contains no list management information",
			comatose(index_no));
}


STORE_S *
list_mgmt_text(RFC2369_S *data, long int msgno)
{
    STORE_S	  *store;
    int		   i, j, n, fields = 0;
    static  char  *rfc2369_intro1 =
      "<HTML><HEAD></HEAD><BODY><H1>Mail List Commands</H1>Message ";
    static char *rfc2369_intro2[] = {
	N_(" has information associated with it "),
	N_("that explains how to participate in an email list.  An "),
	N_("email list is represented by a single email address that "),
	N_("users sharing a common interest can send messages to (known "),
	N_("as posting) which are then redistributed to all members "),
	N_("of the list (sometimes after review by a moderator)."),
	N_("<P>List participation commands in this message include:"),
	NULL
    };

    if((store = so_get(CharStar, NULL, EDIT_ACCESS)) != NULL){

	/* Insert introductory text */
	so_puts(store, rfc2369_intro1);

	so_puts(store, comatose(msgno));

	for(i = 0; rfc2369_intro2[i]; i++)
	  so_puts(store, _(rfc2369_intro2[i]));

	so_puts(store, "<P>");
	for(i = 0; i < MLCMD_COUNT; i++)
	  if(data[i].data[0].value
	     || data[i].data[0].comment
	     || data[i].data[0].error){
	      if(!fields++)
		so_puts(store, "<UL>");

	      so_puts(store, "<LI>");
	      so_puts(store,
		      (n = (data[i].data[1].value || data[i].data[1].comment))
			? "Methods to "
			: "A method to ");

	      so_puts(store, data[i].field.description);
	      so_puts(store, ". ");

	      if(n)
		so_puts(store, "<OL>");

	      for(j = 0;
		  j < MLCMD_MAXDATA
		  && (data[i].data[j].comment
		      || data[i].data[j].value
		      || data[i].data[j].error);
		  j++){

		  so_puts(store, n ? "<P><LI>" : "<P>");

		  if(data[i].data[j].comment){
		      so_puts(store,
			      _("With the provided comment:<P><BLOCKQUOTE>"));
		      so_puts(store, data[i].data[j].comment);
		      so_puts(store, "</BLOCKQUOTE><P>");
		  }

		  if(data[i].data[j].value){
		      if(i == MLCMD_POST
			 && !strucmp(data[i].data[j].value, "NO")){
			  so_puts(store,
			   _("Posting is <EM>not</EM> allowed on this list"));
		      }
		      else{
			  so_puts(store, "Select <A HREF=\"");
			  so_puts(store, data[i].data[j].value);
			  so_puts(store, "\">HERE</A> to ");
			  so_puts(store, (data[i].field.action)
					   ? data[i].field.action
					   : "try it");
		      }

		      so_puts(store, ".");
		  }

		  if(data[i].data[j].error){
		      so_puts(store, "<P>Unfortunately, Alpine can not offer");
		      so_puts(store, " to take direct action based upon it");
		      so_puts(store, " because it was improperly formatted.");
		      so_puts(store, " The unrecognized data associated with");
		      so_puts(store, " the \"");
		      so_puts(store, data[i].field.name);
		      so_puts(store, "\" header field was:<P><BLOCKQUOTE>");
		      so_puts(store, data[i].data[j].error);
		      so_puts(store, "</BLOCKQUOTE>");
		  }

		  so_puts(store, "<P>");
	      }

	      if(n)
		so_puts(store, "</OL>");
	  }

	if(fields)
	  so_puts(store, "</UL>");

	so_puts(store, "</BODY></HTML>");
    }

    return(store);
}


void
list_mgmt_screen(STORE_S *html)
{
    int		    cmd = MC_NONE;
    long	    offset = 0L;
    char	   *error = NULL;
    STORE_S	   *store;
    HANDLE_S	   *handles = NULL;
    gf_io_t	    gc, pc;

    do{
	so_seek(html, 0L, 0);
	gf_set_so_readc(&gc, html);

	init_handles(&handles);

	if((store = so_get(CharStar, NULL, EDIT_ACCESS)) != NULL){
	    gf_set_so_writec(&pc, store);
	    gf_filter_init();

	    gf_link_filter(gf_html2plain,
			   gf_html2plain_opt(NULL, ps_global->ttyo->screen_cols,
					     non_messageview_margin(), &handles, NULL, 0));

	    error = gf_pipe(gc, pc);

	    gf_clear_so_writec(store);

	    if(!error){
		SCROLL_S	sargs;

		memset(&sargs, 0, sizeof(SCROLL_S));
		sargs.text.text	   = so_text(store);
		sargs.text.src	   = CharStar;
		sargs.text.desc	   = "list commands";
		sargs.text.handles = handles;
		if(offset){
		    sargs.start.on	   = Offset;
		    sargs.start.loc.offset = offset;
		}

		sargs.bar.title	   = _("MAIL LIST COMMANDS");
		sargs.bar.style	   = MessageNumber;
		sargs.resize_exit  = 1;
		sargs.help.text	   = h_special_list_commands;
		sargs.help.title   = _("HELP FOR LIST COMMANDS");
		sargs.keys.menu	   = &listmgr_keymenu;
		setbitmap(sargs.keys.bitmap);
		if(!handles){
		    clrbitn(LM_TRY_KEY, sargs.keys.bitmap);
		    clrbitn(LM_PREV_KEY, sargs.keys.bitmap);
		    clrbitn(LM_NEXT_KEY, sargs.keys.bitmap);
		}

		cmd = scrolltool(&sargs);
		offset = sargs.start.loc.offset;
	    }

	    so_give(&store);
	}

	free_handles(&handles);
	gf_clear_so_readc(html);
    }
    while(cmd == MC_RESIZE);
}


/*----------------------------------------------------------------------
 Prompt the user for the type of select desired

   NOTE: any and all functions that successfully exit the second
	 switch() statement below (currently "select_*() functions"),
	 *MUST* update the folder's MESSAGECACHE element's "searched"
	 bits to reflect the search result.  Functions using
	 mail_search() get this for free, the others must update 'em
	 by hand.

    Returns -1 if canceled without changing selection
	     0 if selection may have changed
  ----*/
int
aggregate_select(struct pine *state, MSGNO_S *msgmap, int q_line, CmdWhere in_index)
{
    long          i, diff, old_tot, msgno, raw;
    int           q = 0, rv = 0, narrow = 0, hidden, ret = -1;
    ESCKEY_S     *sel_opts;
    MESSAGECACHE *mc;
    SEARCHSET    *limitsrch = NULL;
    PINETHRD_S   *thrd;
    extern     MAILSTREAM *mm_search_stream;
    extern     long	   mm_search_count;

    hidden           = any_lflagged(msgmap, MN_HIDE) > 0L;
    mm_search_stream = state->mail_stream;
    mm_search_count  = 0L;

    sel_opts = THRD_INDX() ? sel_opts4 : sel_opts2;
    if(THREADING()){
	sel_opts[SEL_OPTS_THREAD].ch = SEL_OPTS_THREAD_CH;
    }
    else{
	sel_opts[SEL_OPTS_THREAD].ch = -1;
    }

    if((old_tot = any_lflagged(msgmap, MN_SLCT)) != 0){
	if(THRD_INDX()){
	    i = 0;
	    thrd = fetch_thread(state->mail_stream,
				mn_m2raw(msgmap, mn_get_cur(msgmap)));
	    /* check if whole thread is selected or not */
	    if(thrd &&
	       count_lflags_in_thread(state->mail_stream,thrd,msgmap,MN_SLCT)
		   ==
	       count_lflags_in_thread(state->mail_stream,thrd,msgmap,MN_NONE))
	      i = 1;

	    sel_opts1[1].label = i ? N_("unselect Curthrd") : N_("select Curthrd");
	}
	else{
	    i = get_lflag(state->mail_stream, msgmap, mn_get_cur(msgmap),
			  MN_SLCT);
	    sel_opts1[1].label = i ? N_("unselect Cur") : N_("select Cur");
	}

	sel_opts += 2;			/* disable extra options */
	switch(q = radio_buttons(_(sel_pmt1), q_line, sel_opts1, 'c', 'x', NO_HELP,
				 RB_NORM)){
	  case 'f' :			/* flip selection */
	    msgno = 0L;
	    for(i = 1L; i <= mn_get_total(msgmap); i++){
		ret = 0;
		q = !get_lflag(state->mail_stream, msgmap, i, MN_SLCT);
		set_lflag(state->mail_stream, msgmap, i, MN_SLCT, q);
		if(hidden){
		    set_lflag(state->mail_stream, msgmap, i, MN_HIDE, !q);
		    if(!msgno && q)
		      mn_reset_cur(msgmap, msgno = i);
		}
	    }

	    return(ret);

	  case 'n' :			/* narrow selection */
	    narrow++;
	  case 'b' :			/* broaden selection */
	    q = 0;			/* offer criteria prompt */
	    break;

	  case 'c' :			/* Un/Select Current */
	  case 'a' :			/* Unselect All */
	  case 'x' :			/* cancel */
	    break;

	  default :
	    q_status_message(SM_ORDER | SM_DING, 3, 3,
			     "Unsupported Select option");
	    return(ret);
	}
    }

    if(!q){
	while(1){
	    q = radio_buttons(sel_pmt2, q_line, sel_opts, 'c', 'x',
			      NO_HELP, RB_NORM|RB_RET_HELP);

	    if(q == 3){
		helper(h_index_cmd_select, _("HELP FOR SELECT"), HLPD_SIMPLE);
		ps_global->mangled_screen = 1;
	    }
	    else
	      break;
	}
    }

    /*
     * The purpose of this is to add the appropriate searchset to the
     * search so that the search can be limited to only looking at what
     * it needs to look at. That is, if we are narrowing then we only need
     * to look at messages which are already selected, and if we are
     * broadening, then we only need to look at messages which are not
     * yet selected. This routine will work whether or not
     * limiting_searchset properly limits the search set. In particular,
     * the searchset returned by limiting_searchset may include messages
     * which really shouldn't be included. We do that because a too-large
     * searchset will break some IMAP servers. It is even possible that it
     * becomes inefficient to send the whole set. If the select function
     * frees limitsrch, it should be sure to set it to NULL so we won't
     * try freeing it again here.
     */
    limitsrch = limiting_searchset(state->mail_stream, narrow);

    /*
     * NOTE: See note about MESSAGECACHE "searched" bits above!
     */
    switch(q){
      case 'x':				/* cancel */
	cmd_cancelled("Select command");
	return(ret);

      case 'c' :			/* select/unselect current */
	(void) select_by_current(state, msgmap, in_index);
	ret = 0;
	return(ret);

      case 'a' :			/* select/unselect all */
	msgno = any_lflagged(msgmap, MN_SLCT);
	diff    = (!msgno) ? mn_get_total(msgmap) : 0L;
	ret = 0;
	agg_select_all(state->mail_stream, msgmap, &diff,
		       any_lflagged(msgmap, MN_SLCT) <= 0L);
	q_status_message4(SM_ORDER,0,2,
			  "%s%s message%s %sselected",
			  msgno ? "" : "All ", comatose(diff), 
			  plural(diff), msgno ? "UN" : "");
	return(ret);

      case 'n' :			/* Select by Number */
	ret = 0;
	if(THRD_INDX())
	  rv = select_by_thrd_number(state->mail_stream, msgmap, &limitsrch);
	else
	  rv = select_by_number(state->mail_stream, msgmap, &limitsrch);

	break;

      case 'd' :			/* Select by Date */
	ret = 0;
	rv = select_by_date(state->mail_stream, msgmap, mn_get_cur(msgmap),
			 &limitsrch);
	break;

      case 't' :			/* Text */
	ret = 0;
	rv = select_by_text(state->mail_stream, msgmap, mn_get_cur(msgmap),
			 &limitsrch);
	break;

      case 'z' :			/* Size */
	ret = 0;
	rv = select_by_size(state->mail_stream, &limitsrch);
	break;

      case 's' :			/* Status */
	ret = 0;
	rv = select_by_status(state->mail_stream, &limitsrch);
	break;

      case 'k' :			/* Keyword */
	ret = 0;
	rv = select_by_keyword(state->mail_stream, &limitsrch);
	break;

      case 'r' :			/* Rule */
	ret = 0;
	rv = select_by_rule(state->mail_stream, &limitsrch);
	break;

      case 'h' :			/* Thread */
	ret = 0;
	rv = select_by_thread(state->mail_stream, msgmap, &limitsrch);
	break;

      default :
	q_status_message(SM_ORDER | SM_DING, 3, 3,
			 "Unsupported Select option");
	return(ret);
    }

    if(limitsrch)
      mail_free_searchset(&limitsrch);

    if(rv)				/* bad return value.. */
      return(ret);			/* error already displayed */

    if(narrow)				/* make sure something was selected */
      for(i = 1L; i <= mn_get_total(msgmap); i++)
	if((raw = mn_m2raw(msgmap, i)) > 0L && state->mail_stream
	   && raw <= state->mail_stream->nmsgs
	   && (mc = mail_elt(state->mail_stream, raw)) && mc->searched){
	    if(get_lflag(state->mail_stream, msgmap, i, MN_SLCT))
	      break;
	    else
	      mm_search_count--;
	}

    diff = 0L;
    if(mm_search_count){
	/*
	 * loop thru all the messages, adjusting local flag bits
	 * based on their "searched" bit...
	 */
	for(i = 1L, msgno = 0L; i <= mn_get_total(msgmap); i++)
	  if(narrow){
	      /* turning OFF selectedness if the "searched" bit isn't lit. */
	      if(get_lflag(state->mail_stream, msgmap, i, MN_SLCT)){
		  if((raw = mn_m2raw(msgmap, i)) > 0L && state->mail_stream
		     && raw <= state->mail_stream->nmsgs
		     && (mc = mail_elt(state->mail_stream, raw))
		     && !mc->searched){
		      diff--;
		      set_lflag(state->mail_stream, msgmap, i, MN_SLCT, 0);
		      if(hidden)
			set_lflag(state->mail_stream, msgmap, i, MN_HIDE, 1);
		  }
		  /* adjust current message in case we unselect and hide it */
		  else if(msgno < mn_get_cur(msgmap)
			  && (!THRD_INDX()
			      || !get_lflag(state->mail_stream, msgmap,
					    i, MN_CHID)))
		    msgno = i;
	      }
	  }
	  else if((raw = mn_m2raw(msgmap, i)) > 0L && state->mail_stream
	          && raw <= state->mail_stream->nmsgs
	          && (mc = mail_elt(state->mail_stream, raw)) && mc->searched){
	      /* turn ON selectedness if "searched" bit is lit. */
	      if(!get_lflag(state->mail_stream, msgmap, i, MN_SLCT)){
		  diff++;
		  set_lflag(state->mail_stream, msgmap, i, MN_SLCT, 1);
		  if(hidden)
		    set_lflag(state->mail_stream, msgmap, i, MN_HIDE, 0);
	      }
	  }

	/* if we're zoomed and the current message was unselected */
	if(narrow && msgno
	   && get_lflag(state->mail_stream,msgmap,mn_get_cur(msgmap),MN_HIDE))
	  mn_reset_cur(msgmap, msgno);
    }

    if(!diff){
	if(narrow)
	  q_status_message4(SM_ORDER, 3, 3,
			"%s.  %s message%s remain%s selected.",
			mm_search_count
					? "No change resulted"
					: "No messages in intersection",
			comatose(old_tot), plural(old_tot),
			(old_tot == 1L) ? "s" : "");
	else if(old_tot)
	  q_status_message(SM_ORDER, 3, 3,
		   _("No change resulted.  Matching messages already selected."));
	else
	  q_status_message1(SM_ORDER | SM_DING, 3, 3,
			    _("Select failed.  No %smessages selected."),
			    old_tot ? _("additional ") : "");
    }
    else if(old_tot){
	snprintf(tmp_20k_buf, SIZEOF_20KBUF,
		"Select matched %ld message%s.  %s %smessage%s %sselected.",
		(diff > 0) ? diff : old_tot + diff,
		plural((diff > 0) ? diff : old_tot + diff),
		comatose((diff > 0) ? any_lflagged(msgmap, MN_SLCT) : -diff),
		(diff > 0) ? "total " : "",
		plural((diff > 0) ? any_lflagged(msgmap, MN_SLCT) : -diff),
		(diff > 0) ? "" : "UN");
	tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	q_status_message(SM_ORDER, 3, 3, tmp_20k_buf);
    }
    else
      q_status_message2(SM_ORDER, 3, 3, _("Select matched %s message%s!"),
			comatose(diff), plural(diff));

    return(ret);
}


/*----------------------------------------------------------------------
 Toggle the state of the current message

   Args: state -- pointer pine's state variables
	 msgmap -- message collection to operate on
	 in_index -- in the message index view
   Returns: TRUE if current marked selected, FALSE otw
  ----*/
int
select_by_current(struct pine *state, MSGNO_S *msgmap, CmdWhere in_index)
{
    long cur;
    int  all_selected = 0;
    unsigned long was, tot, rawno;
    PINETHRD_S *thrd;

    cur = mn_get_cur(msgmap);

    if(THRD_INDX()){
	thrd = fetch_thread(state->mail_stream, mn_m2raw(msgmap, cur));
	if(!thrd)
	  return 0;

	was = count_lflags_in_thread(state->mail_stream, thrd, msgmap, MN_SLCT);
	tot = count_lflags_in_thread(state->mail_stream, thrd, msgmap, MN_NONE);
	if(was == tot)
	  all_selected++;

	if(all_selected){
	    set_thread_lflags(state->mail_stream, thrd, msgmap, MN_SLCT, 0);
	    if(any_lflagged(msgmap, MN_HIDE) > 0L){
		set_thread_lflags(state->mail_stream, thrd, msgmap, MN_HIDE, 1);
		/*
		 * See if there's anything left to zoom on.  If so, 
		 * pick an adjacent one for highlighting, else make
		 * sure nothing is left hidden...
		 */
		if(any_lflagged(msgmap, MN_SLCT)){
		    mn_inc_cur(state->mail_stream, msgmap,
			       (in_index == View && THREADING()
				&& sp_viewing_a_thread(state->mail_stream))
				 ? MH_THISTHD
				 : (in_index == View)
				   ? MH_ANYTHD : MH_NONE);
		    if(mn_get_cur(msgmap) == cur)
		      mn_dec_cur(state->mail_stream, msgmap,
			         (in_index == View && THREADING()
				  && sp_viewing_a_thread(state->mail_stream))
				   ? MH_THISTHD
				   : (in_index == View)
				     ? MH_ANYTHD : MH_NONE);
		}
		else			/* clear all hidden flags */
		  (void) unzoom_index(state, state->mail_stream, msgmap);
	    }
	}
	else
	  set_thread_lflags(state->mail_stream, thrd, msgmap, MN_SLCT, 1);

	q_status_message3(SM_ORDER, 0, 2, "%s message%s %sselected",
			  comatose(all_selected ? was : tot-was),
			  plural(all_selected ? was : tot-was),
			  all_selected ? "UN" : "");
    }
    /* collapsed thread */
    else if(THREADING()
            && ((rawno = mn_m2raw(msgmap, cur)) != 0L)
            && ((thrd = fetch_thread(state->mail_stream, rawno)) != NULL)
            && (thrd && thrd->next && get_lflag(state->mail_stream, NULL, rawno, MN_COLL))){
	/*
	 * This doesn't work quite the same as the colon command works, but
	 * it is arguably doing the correct thing. The difference is
	 * that aggregate_select will zoom after selecting back where it
	 * was called from, but selecting a thread with colon won't zoom.
	 * Maybe it makes sense to zoom after a select but not after a colon
	 * command even though they are very similar.
	 */
	thread_command(state, state->mail_stream, msgmap, ':', -FOOTER_ROWS(state));
    }
    else{
	if((all_selected =
	    get_lflag(state->mail_stream, msgmap, cur, MN_SLCT)) != 0){ /* set? */
	    set_lflag(state->mail_stream, msgmap, cur, MN_SLCT, 0);
	    if(any_lflagged(msgmap, MN_HIDE) > 0L){
		set_lflag(state->mail_stream, msgmap, cur, MN_HIDE, 1);
		/*
		 * See if there's anything left to zoom on.  If so, 
		 * pick an adjacent one for highlighting, else make
		 * sure nothing is left hidden...
		 */
		if(any_lflagged(msgmap, MN_SLCT)){
		    mn_inc_cur(state->mail_stream, msgmap,
			       (in_index == View && THREADING()
				&& sp_viewing_a_thread(state->mail_stream))
				 ? MH_THISTHD
				 : (in_index == View)
				   ? MH_ANYTHD : MH_NONE);
		    if(mn_get_cur(msgmap) == cur)
		      mn_dec_cur(state->mail_stream, msgmap,
			         (in_index == View && THREADING()
				  && sp_viewing_a_thread(state->mail_stream))
				   ? MH_THISTHD
				   : (in_index == View)
				     ? MH_ANYTHD : MH_NONE);
		}
		else			/* clear all hidden flags */
		  (void) unzoom_index(state, state->mail_stream, msgmap);
	    }
	}
	else
	  set_lflag(state->mail_stream, msgmap, cur, MN_SLCT, 1);

	q_status_message2(SM_ORDER, 0, 2, "Message %s %sselected",
			  long2string(cur), all_selected ? "UN" : "");
    }


    return(!all_selected);
}


/*----------------------------------------------------------------------
 Prompt the user for the command to perform on selected messages

   Args: state -- pointer pine's state variables
	 msgmap -- message collection to operate on
	 q_line -- line on display to write prompts
   Returns: 1 if the selected messages are suitably commanded,
	    0 if the choice to pick the command was declined

  ----*/
int
apply_command(struct pine *state, MAILSTREAM *stream, MSGNO_S *msgmap,
	      UCS preloadkeystroke, int flags, int q_line)
{
    int i = 8,			/* number of static entries in sel_opts3 */
        rv = 0,
	cmd,
        we_cancel = 0,
	agg = (flags & AC_FROM_THREAD) ? MCMD_AGG_2 : MCMD_AGG;
    char prompt[80];

    /*
     * To do this "right", we really ought to have access to the keymenu
     * here and change the typed command into a real command by running
     * it through menu_command. Then the switch below would be against
     * results from menu_command. If we did that we'd also pass the
     * results of menu_command in as preloadkeystroke instead of passing
     * the keystroke itself. But we don't have the keymenu handy,
     * so we have to fake it. The only complication that we run into
     * is that KEY_DEL is an escape sequence so we change a typed
     * KEY_DEL esc seq into the letter D.
     */

    if(!preloadkeystroke){
	if(F_ON(F_ENABLE_FLAG,state)){ /* flag? */
	    sel_opts3[i].ch      = '*';
	    sel_opts3[i].rval    = '*';
	    sel_opts3[i].name    = "*";
	    sel_opts3[i++].label = N_("Flag");
	}

	if(F_ON(F_ENABLE_PIPE,state)){ /* pipe? */
	    sel_opts3[i].ch      = '|';
	    sel_opts3[i].rval    = '|';
	    sel_opts3[i].name    = "|";
	    sel_opts3[i++].label = N_("Pipe");
	}

	if(F_ON(F_ENABLE_BOUNCE,state)){ /* bounce? */
	    sel_opts3[i].ch      = 'b';
	    sel_opts3[i].rval    = 'b';
	    sel_opts3[i].name    = "B";
	    sel_opts3[i++].label = N_("Bounce");
	}

	if(flags & AC_FROM_THREAD){
	    if(flags & (AC_COLL | AC_EXPN)){
		sel_opts3[i].ch      = '/';
		sel_opts3[i].rval    = '/';
		sel_opts3[i].name    = "/";
		sel_opts3[i++].label = (flags & AC_COLL) ? N_("Collapse")
							 : N_("Expand");
	    }

	    sel_opts3[i].ch      = ';';
	    sel_opts3[i].rval    = ';';
	    sel_opts3[i].name    = ";";
	    if(flags & AC_UNSEL)
	      sel_opts3[i++].label = N_("UnSelect");
	    else
	      sel_opts3[i++].label = N_("Select");
	}

	if(F_ON(F_ENABLE_PRYNT, state)){	/* this one is invisible */
	    sel_opts3[i].ch      = 'y';
	    sel_opts3[i].rval    = '%';
	    sel_opts3[i].name    = "";
	    sel_opts3[i++].label = "";
	}

	sel_opts3[i].ch      = KEY_DEL;		/* also invisible */
	sel_opts3[i].rval    = 'd';
	sel_opts3[i].name    = "";
	sel_opts3[i++].label = "";

	sel_opts3[i].ch = -1;

	snprintf(prompt, sizeof(prompt), "%s command : ",
		(flags & AC_FROM_THREAD) ? "THREAD" : "APPLY");
	prompt[sizeof(prompt)-1] = '\0';
	cmd = double_radio_buttons(prompt, q_line, sel_opts3, 'z', 'x', NO_HELP,
				   RB_SEQ_SENSITIVE);
	if(isupper(cmd))
	  cmd = tolower(cmd);
    }
    else{
	if(preloadkeystroke == KEY_DEL)
	  cmd = 'd';
	else{
	    if(preloadkeystroke < 0x80 && isupper((int) preloadkeystroke))
	      cmd = tolower((int) preloadkeystroke);
	    else
	      cmd = (int) preloadkeystroke;	/* shouldn't happen */
	}
    }
    
    switch(cmd){
      case 'd' :			/* delete */
	we_cancel = busy_cue(NULL, NULL, 1);
	rv = cmd_delete(state, msgmap, agg, NULL); /* don't advance or offer "TAB" */
	if(we_cancel)
	  cancel_busy_cue(0);
	break;

      case 'u' :			/* undelete */
	we_cancel = busy_cue(NULL, NULL, 1);
	rv = cmd_undelete(state, msgmap, agg);
	if(we_cancel)
	  cancel_busy_cue(0);
	break;

      case 'r' :			/* reply */
	rv = cmd_reply(state, msgmap, agg);
	break;

      case 'f' :			/* Forward */
	rv = cmd_forward(state, msgmap, agg);
	break;

      case '%' :			/* print */
	rv = cmd_print(state, msgmap, agg, MsgIndx);
	break;

      case 't' :			/* take address */
	rv = cmd_take_addr(state, msgmap, agg);
	break;

      case 's' :			/* save */
	rv = cmd_save(state, stream, msgmap, agg, MsgIndx);
	break;

      case 'e' :			/* export */
	rv = cmd_export(state, msgmap, q_line, agg);
	break;

      case '|' :			/* pipe */
	rv = cmd_pipe(state, msgmap, agg);
	break;

      case '*' :			/* flag */
	we_cancel = busy_cue(NULL, NULL, 1);
	rv = cmd_flag(state, msgmap, agg);
	if(we_cancel)
	  cancel_busy_cue(0);
	break;

      case 'b' :			/* bounce */
	rv = cmd_bounce(state, msgmap, agg);
	break;

      case '/' :
	collapse_or_expand(state, stream, msgmap,
			   F_ON(F_SLASH_COLL_ENTIRE, ps_global)
			     ? 0L
			     : mn_get_cur(msgmap));
	break;

      case ':' :
	select_thread_stmp(state, stream, msgmap);
	break;

      case 'x' :			/* cancel */
	cmd_cancelled((flags & AC_FROM_THREAD) ? "Thread command"
					       : "Apply command");
	break;

      case 'z' :			/* default */
        q_status_message(SM_INFO, 0, 2,
			 "Cancelled, there is no default command");
	break;

      default:
	break;
    }

    return(rv);
}


/*
 * Select by message number ranges.
 * Sets searched bits in mail_elts
 * 
 * Args    limitsrch -- limit search to this searchset
 *
 * Returns 0 on success.
 */
int
select_by_number(MAILSTREAM *stream, MSGNO_S *msgmap, SEARCHSET **limitsrch)
{
    int r, end;
    long n1, n2, raw;
    char number1[16], number2[16], numbers[80], *p, *t;
    HelpType help;
    MESSAGECACHE *mc;

    numbers[0] = '\0';
    ps_global->mangled_footer = 1;
    help = NO_HELP;
    while(1){
	int flags = OE_APPEND_CURRENT;

        r = optionally_enter(numbers, -FOOTER_ROWS(ps_global), 0,
			     sizeof(numbers), _(select_num), NULL, help, &flags);
        if(r == 4)
	  continue;

        if(r == 3){
            help = (help == NO_HELP) ? h_select_by_num : NO_HELP;
	    continue;
	}

	for(t = p = numbers; *p ; p++)	/* strip whitespace */
	  if(!isspace((unsigned char)*p))
	    *t++ = *p;

	*t = '\0';

        if(r == 1 || numbers[0] == '\0'){
	    cmd_cancelled("Selection by number");
	    return(1);
        }
	else
	  break;
    }

    for(n1 = 1; n1 <= stream->nmsgs; n1++)
      if((mc = mail_elt(stream, n1)) != NULL)
        mc->searched = 0;			/* clear searched bits */

    for(p = numbers; *p ; p++){
	t = number1;
	while(*p && isdigit((unsigned char)*p))
	  *t++ = *p++;

	*t = '\0';

	end = 0;
	if(number1[0] == '\0'){
	    if(*p == '-'){
		q_status_message1(SM_ORDER | SM_DING, 0, 2,
		   _("Invalid number range, missing number before \"-\": %s"),
		       numbers);
		return(1);
	    }
	    else if(!strucmp("end", p)){
		end = 1;
		p += strlen("end");
	    }
	    else{
		q_status_message1(SM_ORDER | SM_DING, 0, 2,
			        _("Invalid message number: %s"), numbers);
		return(1);
	    }
	}

	if(end)
	  n1 = mn_get_total(msgmap);
	else if((n1 = atol(number1)) < 1L || n1 > mn_get_total(msgmap)){
	    q_status_message1(SM_ORDER | SM_DING, 0, 2,
			      _("\"%s\" out of message number range"),
			      long2string(n1));
	    return(1);
	}

	t = number2;
	if(*p == '-'){
	    while(*++p && isdigit((unsigned char)*p))
	      *t++ = *p;

	    *t = '\0';

	    end = 0;
	    if(number2[0] == '\0'){
		if(!strucmp("end", p)){
		    end = 1;
		    p += strlen("end");
		}
		else{
		    q_status_message1(SM_ORDER | SM_DING, 0, 2,
		 _("Invalid number range, missing number after \"-\": %s"),
		     numbers);
		    return(1);
		}
	    }

	    if(end)
	      n2 = mn_get_total(msgmap);
	    else if((n2 = atol(number2)) < 1L || n2 > mn_get_total(msgmap)){
		q_status_message1(SM_ORDER | SM_DING, 0, 2,
				  _("\"%s\" out of message number range"),
				  long2string(n2));
		return(1);
	    }

	    if(n2 <= n1){
		char t[20];

		strncpy(t, long2string(n1), sizeof(t));
		t[sizeof(t)-1] = '\0';
		q_status_message2(SM_ORDER | SM_DING, 0, 2,
			  _("Invalid reverse message number range: %s-%s"),
				  t, long2string(n2));
		return(1);
	    }

	    for(;n1 <= n2; n1++){
		raw = mn_m2raw(msgmap, n1);
		if(raw > 0L
		   && (!(limitsrch && *limitsrch)
		       || in_searchset(*limitsrch, (unsigned long) raw)))
		  mm_searched(stream, raw);
	    }
	}
	else{
	    raw = mn_m2raw(msgmap, n1);
	    if(raw > 0L
	       && (!(limitsrch && *limitsrch)
		   || in_searchset(*limitsrch, (unsigned long) raw)))
	      mm_searched(stream, raw);
	}

	if(*p == '\0')
	  break;
    }

    return(0);
}


/*
 * Select by thread number ranges.
 * Sets searched bits in mail_elts
 * 
 * Args    limitsrch -- limit search to this searchset
 *
 * Returns 0 on success.
 */
int
select_by_thrd_number(MAILSTREAM *stream, MSGNO_S *msgmap, SEARCHSET **msgset)
{
    int r, end;
    long n1, n2;
    char number1[16], number2[16], numbers[80], *p, *t;
    HelpType help;
    PINETHRD_S   *thrd = NULL;
    MESSAGECACHE *mc;

    numbers[0] = '\0';
    ps_global->mangled_footer = 1;
    help = NO_HELP;
    while(1){
	int flags = OE_APPEND_CURRENT;

        r = optionally_enter(numbers, -FOOTER_ROWS(ps_global), 0,
			     sizeof(numbers), _(select_num), NULL, help, &flags);
        if(r == 4)
	  continue;

        if(r == 3){
            help = (help == NO_HELP) ? h_select_by_thrdnum : NO_HELP;
	    continue;
	}

	for(t = p = numbers; *p ; p++)	/* strip whitespace */
	  if(!isspace((unsigned char)*p))
	    *t++ = *p;

	*t = '\0';

        if(r == 1 || numbers[0] == '\0'){
	    cmd_cancelled("Selection by number");
	    return(1);
        }
	else
	  break;
    }

    for(n1 = 1; n1 <= stream->nmsgs; n1++)
      if((mc = mail_elt(stream, n1)) != NULL)
        mc->searched = 0;			/* clear searched bits */

    for(p = numbers; *p ; p++){
	t = number1;
	while(*p && isdigit((unsigned char)*p))
	  *t++ = *p++;

	*t = '\0';

	end = 0;
	if(number1[0] == '\0'){
	    if(*p == '-'){
		q_status_message1(SM_ORDER | SM_DING, 0, 2,
		   _("Invalid number range, missing number before \"-\": %s"),
		       numbers);
		return(1);
	    }
	    else if(!strucmp("end", p)){
		end = 1;
		p += strlen("end");
	    }
	    else{
		q_status_message1(SM_ORDER | SM_DING, 0, 2,
			        _("Invalid thread number: %s"), numbers);
		return(1);
	    }
	}

	if(end)
	  n1 = msgmap->max_thrdno;
	else if((n1 = atol(number1)) < 1L || n1 > msgmap->max_thrdno){
	    q_status_message1(SM_ORDER | SM_DING, 0, 2,
			      _("\"%s\" out of thread number range"),
			      long2string(n1));
	    return(1);
	}

	t = number2;
	if(*p == '-'){

	    while(*++p && isdigit((unsigned char)*p))
	      *t++ = *p;

	    *t = '\0';

	    end = 0;
	    if(number2[0] == '\0'){
		if(!strucmp("end", p)){
		    end = 1;
		    p += strlen("end");
		}
		else{
		    q_status_message1(SM_ORDER | SM_DING, 0, 2,
		 _("Invalid number range, missing number after \"-\": %s"),
		     numbers);
		    return(1);
		}
	    }

	    if(end)
	      n2 = msgmap->max_thrdno;
	    else if((n2 = atol(number2)) < 1L || n2 > msgmap->max_thrdno){
		q_status_message1(SM_ORDER | SM_DING, 0, 2,
				  _("\"%s\" out of thread number range"),
				  long2string(n2));
		return(1);
	    }

	    if(n2 <= n1){
		char t[20];

		strncpy(t, long2string(n1), sizeof(t));
		t[sizeof(t)-1] = '\0';
		q_status_message2(SM_ORDER | SM_DING, 0, 2,
			  _("Invalid reverse message number range: %s-%s"),
				  t, long2string(n2));
		return(1);
	    }

	    for(;n1 <= n2; n1++){
		thrd = find_thread_by_number(stream, msgmap, n1, thrd);

		if(thrd)
		  set_search_bit_for_thread(stream, thrd, msgset);
	    }
	}
	else{
	    thrd = find_thread_by_number(stream, msgmap, n1, NULL);

	    if(thrd)
	      set_search_bit_for_thread(stream, thrd, msgset);
	}

	if(*p == '\0')
	  break;
    }
    
    return(0);
}


/*
 * Select by message dates.
 * Sets searched bits in mail_elts
 * 
 * Args    limitsrch -- limit search to this searchset
 *
 * Returns 0 on success.
 */
int
select_by_date(MAILSTREAM *stream, MSGNO_S *msgmap, long int msgno, SEARCHSET **limitsrch)
{
    int	       r, we_cancel = 0, when = 0;
    char       date[100], defdate[100], prompt[128];
    time_t     seldate = time(0);
    struct tm *seldate_tm;
    SEARCHPGM *pgm;
    HelpType   help;
    static struct _tense {
	char *preamble,
	     *range,
	     *scope;
    } tense[] = {
	{"were ", "SENT SINCE",     " (inclusive)"},
	{"were ", "SENT BEFORE",    " (exclusive)"},
	{"were ", "SENT ON",        ""            },
	{"",      "ARRIVED SINCE",  " (inclusive)"},
	{"",      "ARRIVED BEFORE", " (exclusive)"},
	{"",      "ARRIVED ON",     ""            }
    };

    date[0]		      = '\0';
    ps_global->mangled_footer = 1;
    help		      = NO_HELP;

    /*
     * If talking to an old server, default to SINCE instead of
     * SENTSINCE, which was added later.
     */
    if(is_imap_stream(stream) && !modern_imap_stream(stream))
      when = 3;

    while(1){
	int flags = OE_APPEND_CURRENT;

	seldate_tm = localtime(&seldate);
	snprintf(defdate, sizeof(defdate), "%.2d-%.4s-%.4d", seldate_tm->tm_mday,
		month_abbrev(seldate_tm->tm_mon + 1),
		seldate_tm->tm_year + 1900);
	defdate[sizeof(defdate)-1] = '\0';
	snprintf(prompt,sizeof(prompt),"Select messages which %s%s%s [%s]: ",
		tense[when].preamble, tense[when].range,
		tense[when].scope, defdate);
	prompt[sizeof(prompt)-1] = '\0';
	r = optionally_enter(date,-FOOTER_ROWS(ps_global), 0, sizeof(date),
			     prompt, sel_date_opt, help, &flags);
	switch (r){
	  case 1 :
	    cmd_cancelled("Selection by date");
	    return(1);

	  case 3 :
	    help = (help == NO_HELP) ? h_select_date : NO_HELP;
	    continue;

	  case 4 :
	    continue;

	  case 11 :
	    {
		MESSAGECACHE *mc;
		long rawno;

		if(stream && (rawno = mn_m2raw(msgmap, msgno)) > 0L
		   && rawno <= stream->nmsgs
		   && (mc = mail_elt(stream, rawno))){

		    /* cache not filled in yet? */
		    if(mc->day == 0){
			char seq[20];

			if(stream->dtb && stream->dtb->flags & DR_NEWS){
			    strncpy(seq,
				    ulong2string(mail_uid(stream, rawno)),
				    sizeof(seq));
			    seq[sizeof(seq)-1] = '\0';
			    mail_fetch_overview(stream, seq, NULL);
			}
			else{
			    strncpy(seq, long2string(rawno),
				    sizeof(seq));
			    seq[sizeof(seq)-1] = '\0';
			    mail_fetch_fast(stream, seq, 0L);
			}
		    }

		    /* mail_date returns fixed field width date */
		    mail_date(date, mc);
		    date[11] = '\0';
		}
	    }

	    continue;

	  case 12 :			/* set default to PREVIOUS day */
	    seldate -= 86400;
	    continue;

	  case 13 :			/* set default to NEXT day */
	    seldate += 86400;
	    continue;

	  case 14 :
	    when = (when+1) % (sizeof(tense) / sizeof(struct _tense));
	    continue;

	  default:
	    break;
	}

	removing_leading_white_space(date);
	removing_trailing_white_space(date);
	if(!*date){
	    strncpy(date, defdate, sizeof(date));
	    date[sizeof(date)-1] = '\0';
	}

	break;
    }

    if((pgm = mail_newsearchpgm()) != NULL){
	MESSAGECACHE elt;
	short          converted_date;

	if(mail_parse_date(&elt, (unsigned char *) date)){
	    converted_date = mail_shortdate(elt.year, elt.month, elt.day);

	    switch(when){
	      case 0:
		pgm->sentsince = converted_date;
		break;
	      case 1:
		pgm->sentbefore = converted_date;
		break;
	      case 2:
		pgm->senton = converted_date;
		break;
	      case 3:
		pgm->since = converted_date;
		break;
	      case 4:
		pgm->before = converted_date;
		break;
	      case 5:
		pgm->on = converted_date;
		break;
	    }

	    pgm->msgno = (limitsrch ? *limitsrch : NULL);

	    if(ps_global && ps_global->ttyo){
		blank_keymenu(ps_global->ttyo->screen_rows - 2, 0);
		ps_global->mangled_footer = 1;
	    }

	    we_cancel = busy_cue(_("Selecting"), NULL, 1);

	    pine_mail_search_full(stream, NULL, pgm, SE_NOPREFETCH | SE_FREE);

	    if(we_cancel)
	      cancel_busy_cue(0);

	    /* we know this was freed in mail_search, let caller know */
	    if(limitsrch)
	      *limitsrch = NULL;
	}
	else{
	    mail_free_searchpgm(&pgm);
	    q_status_message1(SM_ORDER, 3, 3,
			     _("Invalid date entered: %s"), date);
	    return(1);
	}
    }

    return(0);
}


/*
 * Select by searching in message headers or body.
 * Sets searched bits in mail_elts
 * 
 * Args    limitsrch -- limit search to this searchset
 *
 * Returns 0 on success.
 */
int
select_by_text(MAILSTREAM *stream, MSGNO_S *msgmap, long int msgno, SEARCHSET **limitsrch)
{
    int          r, ku, type, we_cancel = 0, flags, rv, ekeyi = 0;
    int          not = 0, me = 0;
    char         sstring[80], savedsstring[80], tmp[128];
    char        *p, *sval = NULL;
    char         buftmp[MAILTMPLEN];
    ESCKEY_S     ekey[8];
    ENVELOPE    *env = NULL;
    HelpType     help;
    unsigned     flagsforhist = 0;
    static HISTORY_S *history = NULL;
    static char *recip = "RECIPIENTS";
    static char *partic = "PARTICIPANTS";
    static char *match_me = N_("[Match_My_Addresses]");
    static char *dont_match_me = N_("[Don't_Match_My_Addresses]");

    ps_global->mangled_footer = 1;
    savedsstring[0] = '\0';
    ekey[0].ch = ekey[1].ch = ekey[2].ch = ekey[3].ch = -1;

    while(1){
	type = radio_buttons(not ? _(sel_text_not) : _(sel_text),
			     -FOOTER_ROWS(ps_global), sel_text_opt,
			     's', 'x', NO_HELP, RB_NORM|RB_RET_HELP);
	
	if(type == '!')
	  not = !not;
	else if(type == 3){
	    helper(h_select_text, "HELP FOR SELECT BASED ON CONTENTS",
		   HLPD_SIMPLE);
	    ps_global->mangled_screen = 1;
	}
	else
	  break;
    }

    /*
     * prepare some friendly defaults...
     */
    switch(type){
      case 't' :			/* address fields, offer To or From */
      case 'f' :
      case 'c' :
      case 'r' :
      case 'p' :
	sval          = (type == 't') ? "TO" :
			  (type == 'f') ? "FROM" :
			    (type == 'c') ? "CC" :
			      (type == 'r') ? recip : partic;
	ekey[ekeyi].ch    = ctrl('T');
	ekey[ekeyi].name  = "^T";
	ekey[ekeyi].rval  = 10;
	/* TRANSLATORS: use Current To Address */
	ekey[ekeyi++].label = N_("Cur To");
	ekey[ekeyi].ch    = ctrl('R');
	ekey[ekeyi].name  = "^R";
	ekey[ekeyi].rval  = 11;
	/* TRANSLATORS: use Current From Address */
	ekey[ekeyi++].label = N_("Cur From");
	ekey[ekeyi].ch    = ctrl('W');
	ekey[ekeyi].name  = "^W";
	ekey[ekeyi].rval  = 12;
	/* TRANSLATORS: use Current Cc Address */
	ekey[ekeyi++].label = N_("Cur Cc");
	ekey[ekeyi].ch    = ctrl('Y');
	ekey[ekeyi].name  = "^Y";
	ekey[ekeyi].rval  = 13;
	/* TRANSLATORS: Match Me means match my address */
	ekey[ekeyi++].label = N_("Match Me");
	ekey[ekeyi].ch    = 0;
	ekey[ekeyi].name  = "";
	ekey[ekeyi].rval  = 0;
	ekey[ekeyi++].label = "";
	break;

      case 's' :
	sval          = "SUBJECT";
	ekey[ekeyi].ch    = ctrl('X');
	ekey[ekeyi].name  = "^X";
	ekey[ekeyi].rval  = 14;
	/* TRANSLATORS: use Current Subject */
	ekey[ekeyi++].label = N_("Cur Subject");
	break;

      case 'a' :
	sval = "TEXT";
	break;

      case 'b' :
	sval = "BODYTEXT";
	break;

      case 'x':
	break;

      default:
	dprint((1,"\n - BOTCH: select_text unrecognized option\n"));
	return(1);
    }

    ekey[ekeyi].ch      = KEY_UP;
    ekey[ekeyi].rval    = 30;
    ekey[ekeyi].name    = "";
    ku = ekeyi;
    ekey[ekeyi++].label = "";

    ekey[ekeyi].ch      = KEY_DOWN;
    ekey[ekeyi].rval    = 31;
    ekey[ekeyi].name    = "";
    ekey[ekeyi++].label = "";

    ekey[ekeyi].ch = -1;

    if(type != 'x'){

	init_hist(&history, HISTSIZE);

	if(ekey[0].ch > -1 && msgno > 0L
	   && !(env=pine_mail_fetchstructure(stream,mn_m2raw(msgmap,msgno),
					     NULL)))
	  ekey[0].ch = -1;

	sstring[0] = '\0';
	help = NO_HELP;
	r = type;
	while(r != 'x'){
	    if(not)
	      /* TRANSLATORS: character String in message <message number> to NOT match : " */
	      snprintf(tmp, sizeof(tmp), "String in message %s to NOT match : ", sval);
	    else
	      snprintf(tmp, sizeof(tmp), "String in message %s to match : ", sval);

	    if(items_in_hist(history) > 0){
		ekey[ku].name  = HISTORY_UP_KEYNAME;
		ekey[ku].label = HISTORY_KEYLABEL;
		ekey[ku+1].name  = HISTORY_DOWN_KEYNAME;
		ekey[ku+1].label = HISTORY_KEYLABEL;
	    }
	    else{
		ekey[ku].name  = "";
		ekey[ku].label = "";
		ekey[ku+1].name  = "";
		ekey[ku+1].label = "";
	    }

	    flags = OE_APPEND_CURRENT | OE_KEEP_TRAILING_SPACE;
	    r = optionally_enter(sstring, -FOOTER_ROWS(ps_global), 0,
				 79, tmp, ekey, help, &flags);

	    if(me && r == 0 && ((!not && strcmp(sstring, _(match_me))) || (not && strcmp(sstring, _(dont_match_me)))))
	      me = 0;

	    switch(r){
	      case 3 :
		help = (help == NO_HELP)
			? (not
			    ? ((type == 'f') ? h_select_txt_not_from
			      : (type == 't') ? h_select_txt_not_to
			       : (type == 'c') ? h_select_txt_not_cc
				: (type == 's') ? h_select_txt_not_subj
				 : (type == 'a') ? h_select_txt_not_all
				  : (type == 'r') ? h_select_txt_not_recip
				   : (type == 'p') ? h_select_txt_not_partic
				    : (type == 'b') ? h_select_txt_not_body
				     :                 NO_HELP)
			    : ((type == 'f') ? h_select_txt_from
			      : (type == 't') ? h_select_txt_to
			       : (type == 'c') ? h_select_txt_cc
				: (type == 's') ? h_select_txt_subj
				 : (type == 'a') ? h_select_txt_all
				  : (type == 'r') ? h_select_txt_recip
				   : (type == 'p') ? h_select_txt_partic
				    : (type == 'b') ? h_select_txt_body
				     :                 NO_HELP))
			: NO_HELP;

	      case 4 :
		continue;

	      case 10 :			/* To: default */
		if(env && env->to && env->to->mailbox){
		    snprintf(sstring, sizeof(sstring), "%s%s%s", env->to->mailbox,
			  env->to->host ? "@" : "",
			  env->to->host ? env->to->host : "");
		    sstring[sizeof(sstring)-1] = '\0';
		}
		continue;

	      case 11 :			/* From: default */
		if(env && env->from && env->from->mailbox){
		    snprintf(sstring, sizeof(sstring), "%s%s%s", env->from->mailbox,
			  env->from->host ? "@" : "",
			  env->from->host ? env->from->host : "");
		    sstring[sizeof(sstring)-1] = '\0';
		}
		continue;

	      case 12 :			/* Cc: default */
		if(env && env->cc && env->cc->mailbox){
		    snprintf(sstring, sizeof(sstring), "%s%s%s", env->cc->mailbox,
			  env->cc->host ? "@" : "",
			  env->cc->host ? env->cc->host : "");
		    sstring[sizeof(sstring)-1] = '\0';
		}
		continue;

	      case 13 :			/* Match my addresses */
		me++;
		snprintf(sstring, sizeof(sstring), not ? _(dont_match_me) : _(match_me));
		continue;

	      case 14 :			/* Subject: default */
		if(env && env->subject && env->subject[0]){
		    char *q = NULL;

		    snprintf(buftmp, sizeof(buftmp), "%.75s", env->subject);
		    buftmp[sizeof(buftmp)-1] = '\0';
		    q = (char *) rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf,
							SIZEOF_20KBUF, buftmp);
		    if(q != env->subject){
			snprintf(savedsstring, sizeof(savedsstring), "%.70s", q);
			savedsstring[sizeof(savedsstring)-1] = '\0';
		    }

		    snprintf(sstring, sizeof(sstring), "%s", q);
		    sstring[sizeof(sstring)-1] = '\0';
		}

		continue;

	      case 30 :
		flagsforhist = (not ? 0x1 : 0) + (me ? 0x2 : 0);
		if((p = get_prev_hist(history, sstring, flagsforhist, NULL)) != NULL){
		    strncpy(sstring, p, sizeof(sstring));
		    sstring[sizeof(sstring)-1] = '\0';
		    if(history->hist[history->curindex]){
			flagsforhist = history->hist[history->curindex]->flags;
			not = (flagsforhist & 0x1) ? 1 : 0;
			me = (flagsforhist & 0x2) ? 1 : 0;
		    }
		}
		else
		  Writechar(BELL, 0);

		continue;

	      case 31 :
		flagsforhist = (not ? 0x1 : 0) + (me ? 0x2 : 0);
		if((p = get_next_hist(history, sstring, flagsforhist, NULL)) != NULL){
		    strncpy(sstring, p, sizeof(sstring));
		    sstring[sizeof(sstring)-1] = '\0';
		    if(history->hist[history->curindex]){
			flagsforhist = history->hist[history->curindex]->flags;
			not = (flagsforhist & 0x1) ? 1 : 0;
			me = (flagsforhist & 0x2) ? 1 : 0;
		    }
		}
		else
		  Writechar(BELL, 0);

		continue;

	      default :
		break;
	    }

	    if(r == 1 || sstring[0] == '\0')
	      r = 'x';

	    break;
	}
    }

    if(type == 'x' || r == 'x'){
	cmd_cancelled("Selection by text");
	return(1);
    }

    if(ps_global && ps_global->ttyo){
	blank_keymenu(ps_global->ttyo->screen_rows - 2, 0);
	ps_global->mangled_footer = 1;
    }

    we_cancel = busy_cue(_("Selecting"), NULL, 1);

    flagsforhist = (not ? 0x1 : 0) + (me ? 0x2 : 0);
    save_hist(history, sstring, flagsforhist, NULL);

    rv = agg_text_select(stream, msgmap, type, not, me, sstring, "utf-8", limitsrch);
    if(we_cancel)
      cancel_busy_cue(0);

    return(rv);
}


/*
 * Select by message size.
 * Sets searched bits in mail_elts
 * 
 * Args    limitsrch -- limit search to this searchset
 *
 * Returns 0 on success.
 */
int
select_by_size(MAILSTREAM *stream, SEARCHSET **limitsrch)
{
    int        r, large = 1, we_cancel = 0;
    unsigned long n, mult = 1L, numerator = 0L, divisor = 1L;
    char       size[16], numbers[80], *p, *t;
    HelpType   help;
    SEARCHPGM *pgm;
    long       flags = (SE_NOPREFETCH | SE_FREE);

    numbers[0] = '\0';
    ps_global->mangled_footer = 1;

    help = NO_HELP;
    while(1){
	int flgs = OE_APPEND_CURRENT;

	sel_size_opt[1].label = large ? sel_size_smaller : sel_size_larger;

        r = optionally_enter(numbers, -FOOTER_ROWS(ps_global), 0,
			     sizeof(numbers), large ? _(select_size_larger_msg)
						    : _(select_size_smaller_msg),
			     sel_size_opt, help, &flgs);
        if(r == 4)
	  continue;

        if(r == 14){
	    large = 1 - large;
	    continue;
	}

        if(r == 3){
            help = (help == NO_HELP) ? (large ? h_select_by_larger_size
					      : h_select_by_smaller_size)
				     : NO_HELP;
	    continue;
	}

	for(t = p = numbers; *p ; p++)	/* strip whitespace */
	  if(!isspace((unsigned char)*p))
	    *t++ = *p;

	*t = '\0';

        if(r == 1 || numbers[0] == '\0'){
	    cmd_cancelled("Selection by size");
	    return(1);
        }
	else
	  break;
    }

    if(numbers[0] == '-'){
	q_status_message1(SM_ORDER | SM_DING, 0, 2,
			  _("Invalid size entered: %s"), numbers);
	return(1);
    }

    t = size;
    p = numbers;

    while(*p && isdigit((unsigned char)*p))
      *t++ = *p++;

    *t = '\0';

    if(size[0] == '\0' && *p == '.' && isdigit(*(p+1))){
	size[0] = '0';
	size[1] = '\0';
    }

    if(size[0] == '\0'){
	q_status_message1(SM_ORDER | SM_DING, 0, 2,
			  _("Invalid size entered: %s"), numbers);
	return(1);
    }

    n = strtoul(size, (char **)NULL, 10); 

    size[0] = '\0';
    if(*p == '.'){
	/*
	 * We probably ought to just use atof() to convert 1.1 into a
	 * double, but since we haven't used atof() anywhere else I'm
	 * reluctant to use it because of portability concerns.
	 */
	p++;
	t = size;
	while(*p && isdigit((unsigned char)*p)){
	    *t++ = *p++;
	    divisor *= 10;
	}

	*t = '\0';

	if(size[0])
	  numerator = strtoul(size, (char **)NULL, 10); 
    }

    switch(*p){
      case 'g':
      case 'G':
        mult *= 1000;
	/* fall through */

      case 'm':
      case 'M':
        mult *= 1000;
	/* fall through */

      case 'k':
      case 'K':
        mult *= 1000;
	break;
    }

    n = n * mult + (numerator * mult) / divisor;

    pgm = mail_newsearchpgm();
    if(large)
	pgm->larger = n;
    else
	pgm->smaller = n;

    if(is_imap_stream(stream) && !modern_imap_stream(stream))
      flags |= SE_NOSERVER;

    if(ps_global && ps_global->ttyo){
	blank_keymenu(ps_global->ttyo->screen_rows - 2, 0);
	ps_global->mangled_footer = 1;
    }

    we_cancel = busy_cue(_("Selecting"), NULL, 1);

    pgm->msgno = (limitsrch ? *limitsrch : NULL);
    pine_mail_search_full(stream, NULL, pgm, SE_NOPREFETCH | SE_FREE);
    /* we know this was freed in mail_search, let caller know */
    if(limitsrch)
      *limitsrch = NULL;

    if(we_cancel)
      cancel_busy_cue(0);

    return(0);
}


/*
 * visible_searchset -- return c-client search set unEXLDed
 *			sequence numbers
 */
SEARCHSET *
visible_searchset(MAILSTREAM *stream, MSGNO_S *msgmap)
{
    long       n, run;
    SEARCHSET *full_set = NULL, **set;

    /*
     * If we're talking to anything other than a server older than
     * imap 4rev1, build a searchset otherwise it'll choke.
     */
    if(!(is_imap_stream(stream) && !modern_imap_stream(stream))){
	if(any_lflagged(msgmap, MN_EXLD)){
	    for(n = 1L, set = &full_set, run = 0L; n <= stream->nmsgs; n++)
	      if(get_lflag(stream, NULL, n, MN_EXLD)){
		  if(run){		/* previous NOT excluded? */
		      if(run > 1L)
			(*set)->last = n - 1L;

		      set = &(*set)->next;
		      run = 0L;
		  }
	      }
	      else if(run++){		/* next in run */
		  (*set)->last = n;
	      }
	      else{				/* start of run */
		  *set = mail_newsearchset();
		  (*set)->first = n;
	      }
	}
	else{
	    full_set = mail_newsearchset();
	    full_set->first = 1L;
	    full_set->last  = stream->nmsgs;
	}
    }

    return(full_set);
}


/*
 * Select by message status bits.
 * Sets searched bits in mail_elts
 * 
 * Args    limitsrch -- limit search to this searchset
 *
 * Returns 0 on success.
 */
int
select_by_status(MAILSTREAM *stream, SEARCHSET **limitsrch)
{
    int s, not = 0, we_cancel = 0, rv;

    while(1){
	s = radio_buttons((not) ? _(sel_flag_not) : _(sel_flag),
			  -FOOTER_ROWS(ps_global), sel_flag_opt, '*', 'x',
			  NO_HELP, RB_NORM|RB_RET_HELP);
			  
	if(s == 'x'){
	    cmd_cancelled("Selection by status");
	    return(1);
	}
	else if(s == 3){
	    helper(h_select_status, _("HELP FOR SELECT BASED ON STATUS"),
		   HLPD_SIMPLE);
	    ps_global->mangled_screen = 1;
	}
	else if(s == '!')
	  not = !not;
	else
	  break;
    }

    if(ps_global && ps_global->ttyo){
	blank_keymenu(ps_global->ttyo->screen_rows - 2, 0);
	ps_global->mangled_footer = 1;
    }

    we_cancel = busy_cue(_("Selecting"), NULL, 1);
    rv = agg_flag_select(stream, not, s, limitsrch);
    if(we_cancel)
      cancel_busy_cue(0);

    return(rv);
}


/*
 * Select by rule. Usually srch, indexcolor, and roles would be most useful.
 * Sets searched bits in mail_elts
 * 
 * Args    limitsrch -- limit search to this searchset
 *
 * Returns 0 on success.
 */
int
select_by_rule(MAILSTREAM *stream, SEARCHSET **limitsrch)
{
    char       rulenick[1000], *nick;
    PATGRP_S  *patgrp;
    int        r, not = 0, we_cancel = 0, rflags = ROLE_DO_SRCH
				    | ROLE_DO_INCOLS
				    | ROLE_DO_ROLES
				    | ROLE_DO_SCORES
				    | ROLE_DO_OTHER
				    | ROLE_DO_FILTER;

    rulenick[0] = '\0';
    ps_global->mangled_footer = 1;

    do{
	int oe_flags;

	oe_flags = OE_APPEND_CURRENT;
        r = optionally_enter(rulenick, -FOOTER_ROWS(ps_global), 0,
			     sizeof(rulenick),
			     not ? _("Rule to NOT match: ")
			         : _("Rule to match: "),
			     sel_key_opt, NO_HELP, &oe_flags);

	if(r == 14){
	    /* select rulenick from a list */
	    if((nick=choose_a_rule(rflags)) != NULL){
		strncpy(rulenick, nick, sizeof(rulenick)-1);
		rulenick[sizeof(rulenick)-1] = '\0';
		fs_give((void **) &nick);
	    }
	    else
	      r = 4;
	}
	else if(r == '!')
	  not = !not;

	if(r == 3){
	    helper(h_select_rule, _("HELP FOR SELECT BY RULE"), HLPD_SIMPLE);
	    ps_global->mangled_screen = 1;
	}
	else if(r == 1){
	    cmd_cancelled("Selection by Rule");
	    return(1);
	}

	removing_leading_and_trailing_white_space(rulenick);

    }while(r == 3 || r == 4 || r == '!');


    /*
     * The approach of requiring a nickname instead of just allowing the
     * user to select from the list of rules has the drawback that a rule
     * may not have a nickname, or there may be more than one rule with
     * the same nickname. However, it has the benefit of allowing the user
     * to type in the nickname and, most importantly, allows us to set
     * up the ! (not). We could incorporate the ! into the selection
     * screen, but this is easier and also allows the typing of nicks.
     * User can just set up nicknames if they want to use this feature.
     */
    patgrp = nick_to_patgrp(rulenick, rflags);

    if(patgrp){
	if(ps_global && ps_global->ttyo){
	    blank_keymenu(ps_global->ttyo->screen_rows - 2, 0);
	    ps_global->mangled_footer = 1;
	}

	we_cancel = busy_cue(_("Selecting"), NULL, 1);
	match_pattern(patgrp, stream, limitsrch ? *limitsrch : 0, NULL,
		      get_msg_score,
		      (not ? MP_NOT : 0) | SE_NOPREFETCH);
	free_patgrp(&patgrp);
	if(we_cancel)
	  cancel_busy_cue(0);
    }

    if(limitsrch && *limitsrch){
	mail_free_searchset(limitsrch);
	*limitsrch = NULL;
    }

    return(0);
}


/*
 * Allow user to choose a rule from their list of rules.
 *
 * Returns an allocated rule nickname on success, NULL otherwise.
 */
char *
choose_a_rule(int rflags)
{
    char      *choice = NULL;
    char     **rule_list, **lp;
    int        cnt = 0;
    PAT_S     *pat;
    PAT_STATE  pstate;

    if(!(nonempty_patterns(rflags, &pstate) && first_pattern(&pstate))){
	q_status_message(SM_ORDER, 3, 3,
			 _("No rules available. Use Setup/Rules to add some."));
	return(choice);
    }

    /*
     * Build a list of rules to choose from.
     */

    for(pat = first_pattern(&pstate); pat; pat = next_pattern(&pstate))
      cnt++;

    if(cnt <= 0){
	q_status_message(SM_ORDER, 3, 4, _("No rules defined, use Setup/Rules"));
	return(choice);
    }

    lp = rule_list = (char **) fs_get((cnt + 1) * sizeof(*rule_list));
    memset(rule_list, 0, (cnt+1) * sizeof(*rule_list));

    for(pat = first_pattern(&pstate); pat; pat = next_pattern(&pstate))
      *lp++ = cpystr((pat->patgrp && pat->patgrp->nick)
			  ? pat->patgrp->nick : "?");

    /* TRANSLATORS: SELECT A RULE is a screen title
       TRANSLATORS: Print something1 using something2.
       "rules" is something1 */
    choice = choose_item_from_list(rule_list, NULL, _("SELECT A RULE"),
				   _("rules"), h_select_rule_screen,
				   _("HELP FOR SELECTING A RULE NICKNAME"), NULL);

    if(!choice)
      q_status_message(SM_ORDER, 1, 4, "No choice");

    free_list_array(&rule_list);

    return(choice);
}


/*
 * Select by current thread.
 * Sets searched bits in mail_elts for this entire thread
 * 
 * Args    limitsrch -- limit search to this searchset
 *
 * Returns 0 on success.
 */
int
select_by_thread(MAILSTREAM *stream, MSGNO_S *msgmap, SEARCHSET **limitsrch)
{
    long n;
    PINETHRD_S *thrd = NULL;
    int ret = 1;
    MESSAGECACHE *mc;

    if(!stream)
      return(ret);

    for(n = 1L; n <= stream->nmsgs; n++)
      if((mc = mail_elt(stream, n)) != NULL)
        mc->searched = 0;			/* clear searched bits */

    thrd = fetch_thread(stream, mn_m2raw(msgmap, mn_get_cur(msgmap)));
    if(thrd && thrd->top && thrd->top != thrd->rawno)
      thrd = fetch_thread(stream, thrd->top);

    /*
     * This doesn't unselect if the thread is already selected
     * (like select current does), it always selects.
     * There is no way to select ! this thread.
     */
    if(thrd){
	set_search_bit_for_thread(stream, thrd, limitsrch);
	ret = 0;
    }

    return(ret);
}


/*
 * Select by message keywords.
 * Sets searched bits in mail_elts
 * 
 * Args    limitsrch -- limit search to this searchset
 *
 * Returns 0 on success.
 */
int
select_by_keyword(MAILSTREAM *stream, SEARCHSET **limitsrch)
{
    int        r, not = 0, we_cancel = 0;
    char       keyword[MAXUSERFLAG+1], *kword;
    char      *error = NULL, *p, *prompt;
    HelpType   help;
    SEARCHPGM *pgm;

    keyword[0] = '\0';
    ps_global->mangled_footer = 1;

    help = NO_HELP;
    do{
	int oe_flags;

	if(error){
	    q_status_message(SM_ORDER, 3, 4, error);
	    fs_give((void **) &error);
	}

	if(F_ON(F_FLAG_SCREEN_KW_SHORTCUT, ps_global) && ps_global->keywords){
	    if(not)
	      prompt = _("Keyword (or keyword initial) to NOT match: ");
	    else
	      prompt = _("Keyword (or keyword initial) to match: ");
	}
	else{
	    if(not)
	      prompt = _("Keyword to NOT match: ");
	    else
	      prompt = _("Keyword to match: ");
	}

	oe_flags = OE_APPEND_CURRENT;
        r = optionally_enter(keyword, -FOOTER_ROWS(ps_global), 0,
			     sizeof(keyword),
			     prompt, sel_key_opt, help, &oe_flags);

	if(r == 14){
	    /* select keyword from a list */
	    if((kword=choose_a_keyword()) != NULL){
		strncpy(keyword, kword, sizeof(keyword)-1);
		keyword[sizeof(keyword)-1] = '\0';
		fs_give((void **) &kword);
	    }
	    else
	      r = 4;
	}
	else if(r == '!')
	  not = !not;

	if(r == 3)
	  help = help == NO_HELP ? h_select_keyword : NO_HELP;
	else if(r == 1){
	    cmd_cancelled("Selection by keyword");
	    return(1);
	}

	removing_leading_and_trailing_white_space(keyword);

    }while(r == 3 || r == 4 || r == '!' || keyword_check(keyword, &error));


    if(F_ON(F_FLAG_SCREEN_KW_SHORTCUT, ps_global) && ps_global->keywords){
	p = initial_to_keyword(keyword);
	if(p != keyword){
	    strncpy(keyword, p, sizeof(keyword)-1);
	    keyword[sizeof(keyword)-1] = '\0';
	}
    }

    /*
     * We want to check the keyword, not the nickname of the keyword,
     * so convert it to the keyword if necessary.
     */
    p = nick_to_keyword(keyword);
    if(p != keyword){
	strncpy(keyword, p, sizeof(keyword)-1);
	keyword[sizeof(keyword)-1] = '\0';
    }

    pgm = mail_newsearchpgm();
    if(not){
	pgm->unkeyword = mail_newstringlist();
	pgm->unkeyword->text.data = (unsigned char *) cpystr(keyword);
	pgm->unkeyword->text.size = strlen(keyword);
    }
    else{
	pgm->keyword = mail_newstringlist();
	pgm->keyword->text.data = (unsigned char *) cpystr(keyword);
	pgm->keyword->text.size = strlen(keyword);
    }

    if(ps_global && ps_global->ttyo){
	blank_keymenu(ps_global->ttyo->screen_rows - 2, 0);
	ps_global->mangled_footer = 1;
    }

    we_cancel = busy_cue(_("Selecting"), NULL, 1);

    pgm->msgno = (limitsrch ? *limitsrch : NULL);
    pine_mail_search_full(stream, "UTF-8", pgm, SE_NOPREFETCH | SE_FREE);
    /* we know this was freed in mail_search, let caller know */
    if(limitsrch)
      *limitsrch = NULL;

    if(we_cancel)
      cancel_busy_cue(0);

    return(0);
}


/*
 * Allow user to choose a keyword from their list of keywords.
 *
 * Returns an allocated keyword on success, NULL otherwise.
 */
char *
choose_a_keyword(void)
{
    char      *choice = NULL;
    char     **keyword_list, **lp;
    int        cnt;
    KEYWORD_S *kw;

    /*
     * Build a list of keywords to choose from.
     */

    for(cnt = 0, kw = ps_global->keywords; kw; kw = kw->next)
      cnt++;

    if(cnt <= 0){
	q_status_message(SM_ORDER, 3, 4,
	    _("No keywords defined, use \"keywords\" option in Setup/Config"));
	return(choice);
    }

    lp = keyword_list = (char **) fs_get((cnt + 1) * sizeof(*keyword_list));
    memset(keyword_list, 0, (cnt+1) * sizeof(*keyword_list));

    for(kw = ps_global->keywords; kw; kw = kw->next)
      *lp++ = cpystr(kw->nick ? kw->nick : kw->kw ? kw->kw : "");

    /* TRANSLATORS: SELECT A KEYWORD is a screen title
       TRANSLATORS: Print something1 using something2.
       "keywords" is something1 */
    choice = choose_item_from_list(keyword_list, NULL, _("SELECT A KEYWORD"),
				   _("keywords"), h_select_keyword_screen,
				   _("HELP FOR SELECTING A KEYWORD"), NULL);

    if(!choice)
      q_status_message(SM_ORDER, 1, 4, "No choice");

    free_list_array(&keyword_list);

    return(choice);
}


/*
 * Allow user to choose a list of keywords from their list of keywords.
 *
 * Returns allocated list.
 */
char **
choose_list_of_keywords(void)
{
    LIST_SEL_S *listhead, *ls, *p;
    char      **ret = NULL;
    int         cnt, i;
    KEYWORD_S  *kw;

    /*
     * Build a list of keywords to choose from.
     */

    p = listhead = NULL;
    for(kw = ps_global->keywords; kw; kw = kw->next){

	ls = (LIST_SEL_S *) fs_get(sizeof(*ls));
	memset(ls, 0, sizeof(*ls));
	ls->item = cpystr(kw->nick ? kw->nick : kw->kw ? kw->kw : "");

	if(p){
	    p->next = ls;
	    p = p->next;
	}
	else
	  listhead = p = ls;
    }
    
    if(!listhead)
      return(ret);
    
    /* TRANSLATORS: SELECT KEYWORDS is a screen title
       Print something1 using something2.
       "keywords" is something1 */
    if(!select_from_list_screen(listhead, SFL_ALLOW_LISTMODE,
				_("SELECT KEYWORDS"), _("keywords"),
				h_select_multkeyword_screen,
			        _("HELP FOR SELECTING KEYWORDS"), NULL)){
	for(cnt = 0, p = listhead; p; p = p->next)
	  if(p->selected)
	    cnt++;

	ret = (char **) fs_get((cnt+1) * sizeof(*ret));
	memset(ret, 0, (cnt+1) * sizeof(*ret));
	for(i = 0, p = listhead; p; p = p->next)
	  if(p->selected)
	    ret[i++] = cpystr(p->item ? p->item : "");
    }

    free_list_sel(&listhead);

    return(ret);
}


/*
 * Allow user to choose a charset
 *
 * Returns an allocated charset on success, NULL otherwise.
 */
char *
choose_a_charset(int which_charsets)
{
    char      *choice = NULL;
    char     **charset_list, **lp;
    const CHARSET *cs;
    int        cnt;

    /*
     * Build a list of charsets to choose from.
     */

    for(cnt = 0, cs = utf8_charset(NIL); cs && cs->name; cs++){
	if(!(cs->flags & (CF_UNSUPRT|CF_NOEMAIL))
	   && ((which_charsets & CAC_ALL)
	       || (which_charsets & CAC_POSTING
		   && cs->flags & CF_POSTING)
	       || (which_charsets & CAC_DISPLAY
		   && cs->type != CT_2022
		   && (cs->flags & (CF_PRIMARY|CF_DISPLAY)) == (CF_PRIMARY|CF_DISPLAY))))
	  cnt++;
    }

    if(cnt <= 0){
	q_status_message(SM_ORDER, 3, 4,
	    _("No charsets found? Enter charset manually."));
	return(choice);
    }

    lp = charset_list = (char **) fs_get((cnt + 1) * sizeof(*charset_list));
    memset(charset_list, 0, (cnt+1) * sizeof(*charset_list));

    for(cs = utf8_charset(NIL); cs && cs->name; cs++){
	if(!(cs->flags & (CF_UNSUPRT|CF_NOEMAIL))
	   && ((which_charsets & CAC_ALL)
	       || (which_charsets & CAC_POSTING
		   && cs->flags & CF_POSTING)
	       || (which_charsets & CAC_DISPLAY
		   && cs->type != CT_2022
		   && (cs->flags & (CF_PRIMARY|CF_DISPLAY)) == (CF_PRIMARY|CF_DISPLAY))))
	  *lp++ = cpystr(cs->name);
    }

    /* TRANSLATORS: SELECT A CHARACTER SET is a screen title
       TRANSLATORS: Print something1 using something2.
       "character sets" is something1 */
    choice = choose_item_from_list(charset_list, NULL, _("SELECT A CHARACTER SET"),
				   _("character sets"), h_select_charset_screen,
				   _("HELP FOR SELECTING A CHARACTER SET"), NULL);

    if(!choice)
      q_status_message(SM_ORDER, 1, 4, "No choice");

    free_list_array(&charset_list);

    return(choice);
}


/*
 * Allow user to choose a list of character sets and/or scripts
 *
 * Returns allocated list.
 */
char **
choose_list_of_charsets(void)
{
    LIST_SEL_S *listhead, *ls, *p;
    char      **ret = NULL;
    int         cnt, i, got_one;
    const CHARSET *cs;
    SCRIPT     *s;
    char       *q, *t;
    long 	width, limit;
    char        buf[1024], *folded;

    /*
     * Build a list of charsets to choose from.
     */

    p = listhead = NULL;

    /* this width is determined by select_from_list_screen() */
    width = ps_global->ttyo->screen_cols - 4;

    /* first comes a list of scripts (sets of character sets) */
    for(s = utf8_script(NIL); s && s->name; s++){

	limit = sizeof(buf)-1;
	q = buf;
	memset(q, 0, limit+1);

	if(s->name)
	  sstrncpy(&q, s->name, limit);

	if(s->description){
	    sstrncpy(&q, " (", limit-(q-buf));
	    sstrncpy(&q, s->description, limit-(q-buf));
	    sstrncpy(&q, ")", limit-(q-buf));
	}

	/* add the list of charsets that are in this script */
	got_one = 0;
	for(cs = utf8_charset(NIL);
	    cs && cs->name && (q-buf) < limit; cs++){
	    if(cs->script & s->script){
		/*
		 * Filter out some un-useful members of the list.
		 * UTF-7 and UTF-8 weren't actually in the list at the
		 * time this was written. Just making sure.
		 */
		if(!strucmp(cs->name, "ISO-2022-JP-2")
		   || !strucmp(cs->name, "UTF-7")
		   || !strucmp(cs->name, "UTF-8"))
		  continue;

		if(got_one)
		  sstrncpy(&q, " ", limit-(q-buf));
		else{
		    got_one = 1;
		    sstrncpy(&q, " {", limit-(q-buf));
		}

		sstrncpy(&q, cs->name, limit-(q-buf));
	    }
	}

	if(got_one)
	  sstrncpy(&q, "}", limit-(q-buf));

	/* fold this line so that it can all be seen on the screen */
	folded = fold(buf, width, width, "", "    ", FLD_NONE);
	if(folded){
	    t = folded;
	    while(t && *t && (q = strindex(t, '\n')) != NULL){
		*q = '\0';

		ls = (LIST_SEL_S *) fs_get(sizeof(*ls));
		memset(ls, 0, sizeof(*ls));
		if(t == folded)
		  ls->item = cpystr(s->name);
		else
		  ls->flags = SFL_NOSELECT;

		ls->display_item = cpystr(t);

		t = q+1;

		if(p){
		    p->next = ls;
		    p = p->next;
		}
		else{
		    /* add a heading */
		    listhead = (LIST_SEL_S *) fs_get(sizeof(*ls));
		    memset(listhead, 0, sizeof(*listhead));
		    listhead->flags = SFL_NOSELECT;
		    listhead->display_item =
	   cpystr(_("Scripts representing groups of related character sets"));
		    listhead->next = (LIST_SEL_S *) fs_get(sizeof(*ls));
		    memset(listhead->next, 0, sizeof(*listhead));
		    listhead->next->flags = SFL_NOSELECT;
		    listhead->next->display_item =
	   cpystr(repeat_char(width, '-'));

		    listhead->next->next = ls;
		    p = ls;
		}
	    }

	    fs_give((void **) &folded);
	}
    }

    ls = (LIST_SEL_S *) fs_get(sizeof(*ls));
    memset(ls, 0, sizeof(*ls));
    ls->flags = SFL_NOSELECT;
    if(p){
	p->next = ls;
	p = p->next;
    }
    else
      listhead = p = ls;

    ls = (LIST_SEL_S *) fs_get(sizeof(*ls));
    memset(ls, 0, sizeof(*ls));
    ls->flags = SFL_NOSELECT;
    ls->display_item =
               cpystr(_("Individual character sets, may be mixed with scripts"));
    p->next = ls;
    p = p->next;

    ls = (LIST_SEL_S *) fs_get(sizeof(*ls));
    memset(ls, 0, sizeof(*ls));
    ls->flags = SFL_NOSELECT;
    ls->display_item =
	       cpystr(repeat_char(width, '-'));
    p->next = ls;
    p = p->next;

    /* then comes a list of individual character sets */
    for(cs = utf8_charset(NIL); cs && cs->name; cs++){
	ls = (LIST_SEL_S *) fs_get(sizeof(*ls));
	memset(ls, 0, sizeof(*ls));
	ls->item = cpystr(cs->name);

	if(p){
	    p->next = ls;
	    p = p->next;
	}
	else
	  listhead = p = ls;
    }
    
    if(!listhead)
      return(ret);
    
    /* TRANSLATORS: SELECT CHARACTER SETS is a screen title
       Print something1 using something2.
       "character sets" is something1 */
    if(!select_from_list_screen(listhead, SFL_ALLOW_LISTMODE,
				_("SELECT CHARACTER SETS"), _("character sets"),
				h_select_multcharsets_screen,
			        _("HELP FOR SELECTING CHARACTER SETS"), NULL)){
	for(cnt = 0, p = listhead; p; p = p->next)
	  if(p->selected)
	    cnt++;

	ret = (char **) fs_get((cnt+1) * sizeof(*ret));
	memset(ret, 0, (cnt+1) * sizeof(*ret));
	for(i = 0, p = listhead; p; p = p->next)
	  if(p->selected)
	    ret[i++] = cpystr(p->item ? p->item : "");
    }

    free_list_sel(&listhead);

    return(ret);
}


/*----------------------------------------------------------------------
   Prompt the user for the type of sort he desires

Args: state -- pine state pointer
      q1 -- Line to prompt on

      Returns 0 if it was cancelled, 1 otherwise.
  ----*/
int
select_sort(struct pine *state, int ql, SortOrder *sort, int *rev)
{
    char      prompt[200], tmp[3], *p;
    int       s, i;
    int       deefault = 'a', retval = 1;
    HelpType  help;
    ESCKEY_S  sorts[14];

#ifdef _WINDOWS
    DLG_SORTPARAM	sortsel;

    if (mswin_usedialog ()) {

	sortsel.reverse = mn_get_revsort (state->msgmap);
	sortsel.cursort = mn_get_sort (state->msgmap);
	/* assumption here that HelpType is char **  */
	sortsel.helptext = h_select_sort;
	sortsel.rval = 0;

	if ((retval = os_sortdialog (&sortsel))) {
	    *sort = sortsel.cursort;
	    *rev  = sortsel.reverse;
        }

	return (retval);
    }
#endif

    /*----- String together the prompt ------*/
    tmp[1] = '\0';
    if(F_ON(F_USE_FK,ps_global))
      strncpy(prompt, _("Choose type of sort : "), sizeof(prompt));
    else
      strncpy(prompt, _("Choose type of sort, or 'R' to reverse current sort : "),
	      sizeof(prompt));

    for(i = 0; state->sort_types[i] != EndofList; i++) {
	sorts[i].rval	   = i;
	p = sorts[i].label = sort_name(state->sort_types[i]);
	while(*(p+1) && islower((unsigned char)*p))
	  p++;

	sorts[i].ch   = tolower((unsigned char)(tmp[0] = *p));
	sorts[i].name = cpystr(tmp);

        if(mn_get_sort(state->msgmap) == state->sort_types[i])
	  deefault = sorts[i].rval;
    }

    sorts[i].ch     = 'r';
    sorts[i].rval   = 'r';
    sorts[i].name   = cpystr("R");
    if(F_ON(F_USE_FK,ps_global))
      sorts[i].label  = N_("Reverse");
    else
      sorts[i].label  = "";

    sorts[++i].ch   = -1;
    help = h_select_sort;

    if((F_ON(F_USE_FK,ps_global)
        && ((s = double_radio_buttons(prompt,ql,sorts,deefault,'x',
				      help,RB_NORM)) != 'x'))
       ||
       (F_OFF(F_USE_FK,ps_global)
        && ((s = radio_buttons(prompt,ql,sorts,deefault,'x',
			       help,RB_NORM)) != 'x'))){
	state->mangled_body = 1;		/* signal screen's changed */
	if(s == 'r')
	  *rev = !mn_get_revsort(state->msgmap);
	else
	  *sort = state->sort_types[s];

	if(F_ON(F_SHOW_SORT, ps_global))
	  ps_global->mangled_header = 1;
    }
    else{
	retval = 0;
	cmd_cancelled("Sort");
    }

    while(--i >= 0)
      fs_give((void **)&sorts[i].name);

    blank_keymenu(ps_global->ttyo->screen_rows - 2, 0);
    return(retval);
}


/*---------------------------------------------------------------------
  Build list of folders in the given context for user selection

  Args: c -- pointer to pointer to folder's context context 
	f -- folder prefix to display
	sublist -- whether or not to use 'f's contents as prefix
	lister -- function used to do the actual display

  Returns: malloc'd string containing sequence, else NULL if
	   no messages in msgmap with local "selected" flag.
  ----*/
int
display_folder_list(CONTEXT_S **c, char *f, int sublist, int (*lister) (struct pine *, CONTEXT_S **, char *, int))
{
    int	       rc;
    CONTEXT_S *tc;
    void (*redraw)(void) = ps_global->redrawer;

    push_titlebar_state();
    tc = *c;
    if((rc = (*lister)(ps_global, &tc, f, sublist)) != 0)
      *c = tc;

    ClearScreen();
    pop_titlebar_state();
    redraw_titlebar();
    if((ps_global->redrawer = redraw) != NULL) /* reset old value, and test */
      (*ps_global->redrawer)();

    if(rc == 1 && F_ON(F_SELECT_WO_CONFIRM, ps_global))
      return(1);

    return(0);
}


/*
 * Allow user to choose a single item from a list of strings.
 *
 * Args    list -- Array of strings to choose from, NULL terminated.
 *     displist -- Array of strings to display instead of displaying list.
 *                   Indices correspond to the list array. Display the displist
 *                   but return the item from list if displist non-NULL.
 *        title -- For conf_scroll_screen
 *        pdesc -- For conf_scroll_screen
 *         help -- For conf_scroll_screen
 *       htitle -- For conf_scroll_screen
 *
 * Returns an allocated copy of the chosen item or NULL.
 */
char *
choose_item_from_list(char **list, char **displist, char *title, char *pdesc, HelpType help,
		      char *htitle, char *cursor_location)
{
    LIST_SEL_S *listhead, *ls, *p, *starting_val = NULL;
    char      **t, **dl;
    char       *ret = NULL, *choice = NULL;

    /* build the LIST_SEL_S list */
    p = listhead = NULL;
    for(t = list, dl = displist; *t; t++, dl++){
	ls = (LIST_SEL_S *) fs_get(sizeof(*ls));
	memset(ls, 0, sizeof(*ls));
	ls->item = cpystr(*t);
	if(displist)
	  ls->display_item = cpystr(*dl);

	if(cursor_location && (cursor_location == (*t)))
	  starting_val = ls;
	
	if(p){
	    p->next = ls;
	    p = p->next;
	}
	else
	  listhead = p = ls;
    }

    if(!listhead)
      return(ret);

    if(!select_from_list_screen(listhead, SFL_NONE, title, pdesc,
				help, htitle, starting_val))
      for(p = listhead; !choice && p; p = p->next)
	if(p->selected)
	  choice = p->item;

    if(choice)
      ret = cpystr(choice);
      
    free_list_sel(&listhead);

    return(ret);
}


void
free_list_sel(LIST_SEL_S **lsel)
{
    if(lsel && *lsel){
	free_list_sel(&(*lsel)->next);
	if((*lsel)->item)
	  fs_give((void **) &(*lsel)->item);

	if((*lsel)->display_item)
	  fs_give((void **) &(*lsel)->display_item);
	
	fs_give((void **) lsel);
    }
}


/*
 * file_lister - call pico library's file lister
 */
int
file_lister(char *title, char *path, size_t pathlen, char *file, size_t filelen, int newmail, int flags)
{
    PICO   pbf;
    int	   rv;
    void (*redraw)(void) = ps_global->redrawer;

    standard_picobuf_setup(&pbf);
    push_titlebar_state();
    if(!newmail)
      pbf.newmail = NULL;

/* BUG: what about help command and text? */
    pbf.pine_anchor   = title;

    rv = pico_file_browse(&pbf, path, pathlen, file, filelen, NULL, 0, flags);
    standard_picobuf_teardown(&pbf);
    fix_windsize(ps_global);
    init_signals();		/* has it's own signal stuff */

    /* Restore display's titlebar and body */
    pop_titlebar_state();
    redraw_titlebar();
    if((ps_global->redrawer = redraw) != NULL)
      (*ps_global->redrawer)();

    return(rv);
}


/*----------------------------------------------------------------------
    Print current folder index

  ---*/
int
print_index(struct pine *state, MSGNO_S *msgmap, int agg)
{
    long     i;
    ICE_S   *ice;
    char     buf[MAX_SCREEN_COLS+1];

    for(i = 1L; i <= mn_get_total(msgmap); i++){
	if(agg && !get_lflag(state->mail_stream, msgmap, i, MN_SLCT))
	  continue;
	
	if(!agg && msgline_hidden(state->mail_stream, msgmap, i, 0))
	  continue;

	ice = build_header_line(state, state->mail_stream, msgmap, i, NULL);

	if(ice){
	  /*
	   * I don't understand why we'd want to mark the current message
	   * instead of printing out the first character of the status
	   * so I'm taking it out and including the first character of the
	   * line instead. Hubert 2006-02-09
	   *
	  if(!print_char((mn_is_cur(msgmap, i)) ? '>' : ' '))
	    return(0);
	   */

	  if(!gf_puts(simple_index_line(buf,sizeof(buf),ice,i),
				        print_char)
	     || !gf_puts(NEWLINE, print_char))
	    return(0);
	}
    }

    return(1);
}


#ifdef	_WINDOWS

/*
 * windows callback to get/set header mode state
 */
int
header_mode_callback(set, args)
    int  set;
    long args;
{
    return(ps_global->full_header);
}


/*
 * windows callback to get/set zoom mode state
 */
int
zoom_mode_callback(set, args)
    int  set;
    long args;
{
    return(any_lflagged(ps_global->msgmap, MN_HIDE) != 0);
}


/*
 * windows callback to get/set zoom mode state
 */
int
any_selected_callback(set, args)
    int  set;
    long args;
{
    return(any_lflagged(ps_global->msgmap, MN_SLCT) != 0);
}


/*
 *
 */
int
flag_callback(set, flags)
    int  set;
    long flags;
{
    MESSAGECACHE *mc;
    int		  newflags = 0;
    long	  msgno;
    int		  permflag = 0;

    switch (set) {
      case 1:			/* Important */
        permflag = ps_global->mail_stream->perm_flagged;
	break;

      case 2:			/* New */
        permflag = ps_global->mail_stream->perm_seen;
	break;

      case 3:			/* Answered */
        permflag = ps_global->mail_stream->perm_answered;
	break;

      case 4:			/* Deleted */
        permflag = ps_global->mail_stream->perm_deleted;
	break;

    }

    if(!(any_messages(ps_global->msgmap, NULL, "to Flag")
	 && can_set_flag(ps_global, "flag", permflag)))
      return(0);

    if(sp_io_error_on_stream(ps_global->mail_stream)){
	sp_set_io_error_on_stream(ps_global->mail_stream, 0);
	pine_mail_check(ps_global->mail_stream);	/* forces write */
	return(0);
    }

    msgno = mn_m2raw(ps_global->msgmap, mn_get_cur(ps_global->msgmap));
    if(msgno > 0L && ps_global->mail_stream
       && msgno <= ps_global->mail_stream->nmsgs
       && (mc = mail_elt(ps_global->mail_stream, msgno))
       && mc->valid){
	/*
	 * NOTE: code below is *VERY* sensitive to the order of
	 * the messages defined in resource.h for flag handling.
	 * Don't change it unless you know what you're doing.
	 */
	if(set){
	    char *flagstr;
	    long  mflag;

	    switch(set){
	      case 1 :			/* Important */
		flagstr = "\\FLAGGED";
		mflag   = (mc->flagged) ? 0L : ST_SET;
		break;

	      case 2 :			/* New */
		flagstr = "\\SEEN";
		mflag   = (mc->seen) ? 0L : ST_SET;
		break;

	      case 3 :			/* Answered */
		flagstr = "\\ANSWERED";
		mflag   = (mc->answered) ? 0L : ST_SET;
		break;

	      case 4 :		/* Deleted */
		flagstr = "\\DELETED";
		mflag   = (mc->deleted) ? 0L : ST_SET;
		break;

	      default :			/* bogus */
		return(0);
	    }

	    mail_flag(ps_global->mail_stream, long2string(msgno),
		      flagstr, mflag);

	    if(ps_global->redrawer)
	      (*ps_global->redrawer)();
	}
	else{
	    /* Important */
	    if(mc->flagged)
	      newflags |= 0x0001;

	    /* New */
	    if(!mc->seen)
	      newflags |= 0x0002;

	    /* Answered */
	    if(mc->answered)
	      newflags |= 0x0004;

	    /* Deleted */
	    if(mc->deleted)
	      newflags |= 0x0008;
	}
    }

    return(newflags);
}



/*
 * BUG:  Should teach this about keywords
 */
MPopup *
flag_submenu(mc)
    MESSAGECACHE *mc;
{
    static MPopup flag_submenu[] = {
	{tMessage, {N_("Important"), lNormal}, {IDM_MI_FLAGIMPORTANT}},
	{tMessage, {N_("New"), lNormal}, {IDM_MI_FLAGNEW}},
	{tMessage, {N_("Answered"), lNormal}, {IDM_MI_FLAGANSWERED}},
	{tMessage , {N_("Deleted"), lNormal}, {IDM_MI_FLAGDELETED}},
	{tTail}
    };

    /* Important */
    flag_submenu[0].label.style = (mc && mc->flagged) ? lChecked : lNormal;

    /* New */
    flag_submenu[1].label.style = (mc && mc->seen) ? lNormal : lChecked;

    /* Answered */
    flag_submenu[2].label.style = (mc && mc->answered) ? lChecked : lNormal;

    /* Deleted */
    flag_submenu[3].label.style = (mc && mc->deleted) ? lChecked : lNormal;

    return(flag_submenu);
}

#endif	/* _WINDOWS */
