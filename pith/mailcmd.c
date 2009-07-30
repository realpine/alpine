#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: mailcmd.c 1142 2008-08-13 17:22:21Z hubert@u.washington.edu $";
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
#include "../pith/mailcmd.h"
#include "../pith/conf.h"
#include "../pith/status.h"
#include "../pith/flag.h"
#include "../pith/thread.h"
#include "../pith/util.h"
#include "../pith/folder.h"
#include "../pith/sort.h"
#include "../pith/newmail.h"
#include "../pith/mailview.h"
#include "../pith/mailindx.h"
#include "../pith/save.h"
#include "../pith/news.h"
#include "../pith/sequence.h"
#include "../pith/stream.h"
#include "../pith/ldap.h"
#include "../pith/options.h"
#include "../pith/busy.h"
#include "../pith/icache.h"
#include "../pith/ablookup.h"
#include "../pith/search.h"
#include "../pith/charconv/utf8.h"

#ifdef _WINDOWS
#include "../pico/osdep/mswin.h"
#endif


/*
 * Internal prototypes
 */


/*
 * optional function hooks
 */
int	(*pith_opt_read_msg_prompt)(long, char *);
int	(*pith_opt_reopen_folder)(struct pine *, int *);
int	(*pith_opt_expunge_prompt)(MAILSTREAM *, char *, long);
void	(*pith_opt_begin_closing)(int, char *);
void	  get_new_message_count(MAILSTREAM *, int, long *, long *);
char	 *new_messages_string(MAILSTREAM *);
void      search_for_our_regex_addresses(MAILSTREAM *stream, char type,
					 int not, SEARCHSET *searchset);



/*----------------------------------------------------------------------
   Complain about command on empty folder

  Args: map -- msgmap 
	type -- type of message that's missing
	cmd -- string explaining command attempted

 ----*/
int
any_messages(MSGNO_S *map, char *type, char *cmd)
{
    if(mn_get_total(map) <= 0L){
	q_status_message5(SM_ORDER, 0, 2, "No %s%s%s%s%s",
			  type ? type : "",
			  type ? " " : "",
			  THRD_INDX() ? "threads" : "messages",
			  (!cmd || *cmd != '.') ? " " : "",
			  cmd ? cmd : "in folder");
	return(FALSE);
    }

    return(TRUE);
}


/*----------------------------------------------------------------------
   test whether or not we have a valid stream to set flags on

  Args: state -- pine state containing vital signs
	cmd -- string explaining command attempted
	permflag -- associated permanent flag state

  Result: returns 1 if we can set flags, otw 0 and complains

 ----*/
int
can_set_flag(struct pine *state, char *cmd, int permflag)
{
    if((!permflag && READONLY_FOLDER(state->mail_stream))
       || sp_dead_stream(state->mail_stream)){
	q_status_message2(SM_ORDER | (sp_dead_stream(state->mail_stream)
				        ? SM_DING : 0),
			  0, 3,
			  "Can't %s message.  Folder is %s.", cmd,
			  (sp_dead_stream(state->mail_stream)) ? "closed" : "read-only");
	return(FALSE);
    }

    return(TRUE);
}


/*----------------------------------------------------------------------
   Complain about command on empty folder

  Args: type -- type of message that's missing
	cmd -- string explaining command attempted

 ----*/
void
cmd_cancelled(char *cmd)
{
    /* TRANSLATORS: Arg is replaced with the command name or the word Command */
    q_status_message1(SM_INFO, 0, 2, _("%s cancelled"), cmd ? cmd : _("Command"));
}


/*----------------------------------------------------------------------
   Execute DELETE message command

  Args: state --  Various satate info
        msgmap --  map of c-client to local message numbers

 Result: with side effect of "current" message delete flag set

 ----*/
int
cmd_delete(struct pine *state, MSGNO_S *msgmap, int copts,
	   char *(*cmd_action_f)(struct pine *, MSGNO_S *))
{
    int	  lastmsg, rv = 0;
    long  msgno, del_count = 0L, new;
    char *sequence = NULL, prompt[128];

    dprint((4, "\n - delete message -\n"));
    if(!(any_messages(msgmap, NULL, "to Delete")
	 && can_set_flag(state, "delete", state->mail_stream->perm_deleted)))
      return rv;

    rv++;

    if(sp_io_error_on_stream(state->mail_stream)){
	sp_set_io_error_on_stream(state->mail_stream, 0);
	pine_mail_check(state->mail_stream);		/* forces write */
    }

    if(MCMD_ISAGG(copts)){
	sequence = selected_sequence(state->mail_stream, msgmap, &del_count, 0);
	snprintf(prompt, sizeof(prompt), "%ld%s message%s marked for deletion",
		del_count, (copts  & MCMD_AGG_2) ? "" : " selected", plural(del_count));
    }
    else{
	long rawno;

	msgno	  = mn_get_cur(msgmap);
	rawno     = mn_m2raw(msgmap, msgno);
	del_count = 1L;				/* return current */
	sequence  = cpystr(long2string(rawno));
	lastmsg	  = (msgno >= mn_get_total(msgmap));
	snprintf(prompt, sizeof(prompt), "%s%s marked for deletion",
		lastmsg ? "Last message" : "Message ",
		lastmsg ? "" : long2string(msgno));
    }

    dprint((3, "DELETE: msg %s\n", sequence ? sequence : "?"));
    new = sp_new_mail_count(state->mail_stream);
    mail_flag(state->mail_stream, sequence, "\\DELETED", ST_SET);
    fs_give((void **) &sequence);
    if(new != sp_new_mail_count(state->mail_stream))
      process_filter_patterns(state->mail_stream, state->msgmap,
			      sp_new_mail_count(state->mail_stream));

    if(cmd_action_f){
	char *rv;

	if((rv = (*cmd_action_f)(state, msgmap)) != NULL)
	  strncat(prompt, rv, sizeof(prompt) - strlen(prompt)- 1);
    }

    if(!(copts & MCMD_SILENT))
      q_status_message(SM_ORDER, 0, 3, prompt);

    return rv;
}


/*----------------------------------------------------------------------
   Execute UNDELETE message command

  Args: state --  Various satate info
        msgmap --  map of c-client to local message numbers

 Result: with side effect of "current" message delete flag UNset

 ----*/
int
cmd_undelete(struct pine *state, MSGNO_S *msgmap, int copts)
{
    long	  del_count;
    char	 *sequence;
    int		  wasdeleted = FALSE, rv = 0;
    MESSAGECACHE *mc;

    dprint((4, "\n - undelete -\n"));
    if(!(any_messages(msgmap, NULL, "to Undelete")
	 && can_set_flag(state, "undelete", state->mail_stream->perm_deleted)))
      return rv;

    rv++;

    if(MCMD_ISAGG(copts)){
	del_count = 0L;				/* return current */
	sequence = selected_sequence(state->mail_stream, msgmap, &del_count, 1);
	wasdeleted = TRUE;
    }
    else{
	long rawno;
	int  exbits = 0;

	del_count = 1L;				/* return current */
	rawno = mn_m2raw(msgmap, mn_get_cur(msgmap));
	sequence  = cpystr(long2string(rawno));
	wasdeleted = (state->mail_stream
	              && rawno > 0L && rawno <= state->mail_stream->nmsgs
		      && (mc = mail_elt(state->mail_stream, rawno))
		       && mc->valid
		       && mc->deleted);
	/*
	 * Mark this message manually flagged so we don't re-filter it
	 * with a filter which only sets flags.
	 */
	if(msgno_exceptions(state->mail_stream, rawno, "0", &exbits, FALSE))
	  exbits |= MSG_EX_MANUNDEL;
	else
	  exbits = MSG_EX_MANUNDEL;

	msgno_exceptions(state->mail_stream, rawno, "0", &exbits, TRUE);
    }

    dprint((3, "UNDELETE: msg %s\n", sequence ? sequence : "?"));

    mail_flag(state->mail_stream, sequence, "\\DELETED", 0L);
    fs_give((void **) &sequence);

    if((copts & MCMD_SILENT) == 0){
	if(del_count == 1L && MCMD_ISAGG(copts) == 0){
	    q_status_message(SM_ORDER, 0, 3,
			     wasdeleted
			       ? _("Deletion mark removed, message won't be deleted")
			       : _("Message not marked for deletion; no action taken"));
	}
	else
	  q_status_message2(SM_ORDER, 0, 3,
			    _("Deletion mark removed from %s message%s"),
			    comatose(del_count), plural(del_count));
    }

    if(sp_io_error_on_stream(state->mail_stream)){
	sp_set_io_error_on_stream(state->mail_stream, 0);
	pine_mail_check(state->mail_stream);		/* forces write */
    }

    return rv;
}


int
cmd_expunge_work(MAILSTREAM *stream, MSGNO_S *msgmap)
{
    long  old_max_msgno;
    int	  rv = 0;

    old_max_msgno = mn_get_total(msgmap);
    delete_filtered_msgs(stream);
    ps_global->expunge_in_progress = 1;
    mail_expunge(stream);
    ps_global->expunge_in_progress = 0;

    dprint((2,"expunge complete cur:%ld max:%ld\n",
	      mn_get_cur(msgmap), mn_get_total(msgmap)));
    /*
     * This is only actually necessary if this causes the width of the
     * message number field to change.  That is, it depends on the
     * format the user is using as well as on the max_msgno.  Since it
     * should be rare, we'll just do it whenever it happens.
     * Also have to check for an increase in max_msgno on new mail.
     */
    if((old_max_msgno >= 1000L && mn_get_total(msgmap) < 1000L)
       || (old_max_msgno >= 10000L && mn_get_total(msgmap) < 10000L)
       || (old_max_msgno >= 100000L && mn_get_total(msgmap) < 100000L)){
	clear_index_cache(stream, 0);
	rv = 1;
    }

    /*
     * mm_exists and mm_expunge take care of updating max_msgno
     * and selecting a new message should the selected get removed
     */


    reset_check_point(stream);

    return(rv);
}


CONTEXT_S *
broach_get_folder(CONTEXT_S *context, int *inbox, char **folder)
{
    CONTEXT_S *tc;

    if(ps_global->goto_default_rule == GOTO_LAST_FLDR){
	tc = context ? context : ps_global->context_current;
	*inbox = 1;		/* fill in last_folder below */
    }
    else if(ps_global->goto_default_rule == GOTO_FIRST_CLCTN){
	tc = (ps_global->context_list->use & CNTXT_INCMNG)
	  ? ps_global->context_list->next : ps_global->context_list;
	ps_global->last_unambig_folder[0] = '\0';
	*inbox = 1;		/* fill in last_folder below */
    }
    else if(ps_global->goto_default_rule == GOTO_FIRST_CLCTN_DEF_INBOX){
	tc = (ps_global->context_list->use & CNTXT_INCMNG)
	  ? ps_global->context_list->next : ps_global->context_list;
	tc->last_folder[0] = '\0';
	*inbox = 0;
	ps_global->last_unambig_folder[0] = '\0';
    }
    else{
	*inbox = (ps_global->cur_folder
	          && ps_global->inbox_name
		  && strucmp(ps_global->cur_folder,ps_global->inbox_name) == 0
		  && (!ps_global->context_current
		      || ps_global->context_current->use & CNTXT_INCMNG
		      || (!(ps_global->context_list->use & CNTXT_INCMNG)
		          && ps_global->context_current == ps_global->context_list)));
	if(!*inbox)
	  tc = ps_global->context_list;		/* inbox's context */
	else if(ps_global->goto_default_rule == GOTO_INBOX_FIRST_CLCTN){
	    tc = (ps_global->context_list->use & CNTXT_INCMNG)
		  ? ps_global->context_list->next : ps_global->context_list;
	    ps_global->last_unambig_folder[0] = '\0';
	}
	else
	  tc = context ? context : ps_global->context_current;
    }

    if(folder){
	if(!*inbox){
	    *folder = ps_global->inbox_name;
	}
	else
	  *folder = (ps_global->last_unambig_folder[0])
			  ? ps_global->last_unambig_folder
			  : ((tc->last_folder[0]) ? tc->last_folder : NULL);
    }

    return(tc);
}


/*----------------------------------------------------------------------
    Actually attempt to open given folder 

  Args: newfolder -- The folder name to open
        streamp   -- Candidate stream for recycling. This stream will either
	             be re-used, or it will be closed.

 Result:  1 if the folder was successfully opened
          0 if the folder open failed and went back to old folder
         -1 if open failed and no folder is left open
      
  Attempt to open the folder name given. If the open of the new folder
  fails then the previously open folder will remain open, unless
  something really bad has happened. The designate inbox will always be
  kept open, and when a request to open it is made the already open
  stream will be used.
  ----*/
int
do_broach_folder(char *newfolder, CONTEXT_S *new_context, MAILSTREAM **streamp,
		 long unsigned int flags)
{
    MAILSTREAM *m, *strm, *stream = streamp ? *streamp : NULL;
    int         open_inbox, rv, old_tros, we_cancel = 0,
                do_reopen = 0, n, was_dead = 0, cur_already_set = 0;
    char        expanded_file[MAX(MAXPATH,MAILTMPLEN)+1],
	       *old_folder, *old_path, *p, *report;
    long        openmode, rflags = 0L, pc = 0L, cur, raw;
    ENVELOPE   *env = NULL;
    char        status_msg[81];
    SortOrder	old_sort;
    unsigned    perfolder_startup_rule;
    char        tmp1[MAILTMPLEN], tmp2[MAILTMPLEN], *lname, *mname;

    openmode = SP_USERFLDR;

    dprint((1, "About to open folder \"%s\"    inbox is: \"%s\"\n",
	       newfolder ? newfolder : "?",
	       ps_global->inbox_name ? ps_global->inbox_name : "?"));

    /*
     *--- Set flag that we're opening the inbox, a special case.
     *
     * We want to know if inbox is being opened either by name OR
     * fully qualified path...
     */
    if(strucmp(newfolder, ps_global->inbox_name) == 0)
      open_inbox = (flags & DB_INBOXWOCNTXT || new_context == ps_global->context_list);
    else{
	open_inbox = (strcmp(newfolder, ps_global->VAR_INBOX_PATH) == 0
		      || same_remote_mailboxes(newfolder, ps_global->VAR_INBOX_PATH)
		      || (!IS_REMOTE(newfolder)
			  && (lname=mailboxfile(tmp1,newfolder))
			  && (mname=mailboxfile(tmp2,ps_global->VAR_INBOX_PATH))
			  && !strcmp(lname,mname)));

	/* further checking for inbox open */
	if(!open_inbox && new_context && context_isambig(newfolder)){
	    if((p = folder_is_nick(newfolder, FOLDERS(new_context), FN_WHOLE_NAME)) != NULL){
		/*
		 * Check for an incoming folder other
		 * than INBOX that also point to INBOX.
		 */
		open_inbox = (strucmp(p, ps_global->inbox_name) == 0
			      || strcmp(p, ps_global->VAR_INBOX_PATH) == 0
			      || same_remote_mailboxes(p, ps_global->VAR_INBOX_PATH)
			      || (!IS_REMOTE(p)
				  && (lname=mailboxfile(tmp1,p))
				  && (mname=mailboxfile(tmp2,ps_global->VAR_INBOX_PATH))
				  && !strcmp(lname,mname)));
	    }
	    else if(!(new_context->use & CNTXT_INCMNG)){
		char tmp3[MAILTMPLEN];

		/*
		 * Check to see if we are opening INBOX using the folder name
		 * and a context. We won't have recognized this is the
		 * same as INBOX without applying the context first.
		 */
		context_apply(tmp3, new_context, newfolder, sizeof(tmp3));
		open_inbox = (strucmp(tmp3, ps_global->inbox_name) == 0
			      || strcmp(tmp3, ps_global->VAR_INBOX_PATH) == 0
			      || same_remote_mailboxes(tmp3, ps_global->VAR_INBOX_PATH)
			      || (!IS_REMOTE(tmp3)
				  && (lname=mailboxfile(tmp1,tmp3))
				  && (mname=mailboxfile(tmp2,ps_global->VAR_INBOX_PATH))
				  && !strcmp(lname,mname)));
	    }
	}
    }

    if(open_inbox)
      new_context = ps_global->context_list; /* restore first context */

    was_dead = sp_a_locked_stream_is_dead();

    /*----- Little to do to if reopening same folder -----*/
    if(new_context == ps_global->context_current && ps_global->mail_stream
       && (strcmp(newfolder, ps_global->cur_folder) == 0
           || (open_inbox && sp_flagged(ps_global->mail_stream, SP_INBOX)))){
	if(stream){
	    pine_mail_close(stream);	/* don't need it */
	    stream = NULL;
	}

	if(sp_dead_stream(ps_global->mail_stream))
	  do_reopen++;
	
	/*
	 * If it is a stream which could probably discover newmail by
	 * reopening and user has YES set for those streams, or it
	 * is a stream which may discover newmail by reopening and
	 * user has YES set for those stream, then do_reopen.
	 */
	if(!do_reopen
	   &&
	   (((ps_global->mail_stream->dtb
	      && ((ps_global->mail_stream->dtb->flags & DR_NONEWMAIL)
		  || (ps_global->mail_stream->rdonly
		      && ps_global->mail_stream->dtb->flags
					      & DR_NONEWMAILRONLY)))
	     && (ps_global->reopen_rule == REOPEN_YES_YES
	         || ps_global->reopen_rule == REOPEN_YES_ASK_Y
	         || ps_global->reopen_rule == REOPEN_YES_ASK_N
	         || ps_global->reopen_rule == REOPEN_YES_NO))
	    ||
	    ((ps_global->mail_stream->dtb
	      && ps_global->mail_stream->rdonly
	      && !(ps_global->mail_stream->dtb->flags & DR_LOCAL))
	     && (ps_global->reopen_rule == REOPEN_YES_YES))))
	  do_reopen++;

	/*
	 * If it is a stream which could probably discover newmail by
	 * reopening and user has ASK set for those streams, or it
	 * is a stream which may discover newmail by reopening and
	 * user has ASK set for those stream, then ask.
	 */
	if(!do_reopen
	   && pith_opt_reopen_folder
	   && (*pith_opt_reopen_folder)(ps_global, &do_reopen) < 0){
	    cmd_cancelled(NULL);
	    return(0);
	}

	if(do_reopen){
	    /*
	     * If it's not healthy or if the user explicitly wants to
	     * do a reopen, we reset things and fall thru
	     * to actually reopen it.
	     */
	    if(sp_dead_stream(ps_global->mail_stream)){
		dprint((2, "Stream was dead, reopening \"%s\"\n",
				      newfolder ? newfolder : "?"));
	    }

	    /* clean up */
	    pine_mail_actually_close(ps_global->mail_stream);
	    ps_global->mangled_header = 1;
	    clear_index_cache(ps_global->mail_stream, 0);
	}
	else{
	    if(!(flags & DB_NOVISIT))
	      sp_set_recent_since_visited(ps_global->mail_stream, 0L);

	    return(1);			/* successful open of same folder! */
	}
    }

    /*
     * If ambiguous foldername (not fully qualified), make sure it's
     * not a nickname for a folder in the given context...
     */

    /* might get reset below */
    strncpy(expanded_file, newfolder, sizeof(expanded_file));
    expanded_file[sizeof(expanded_file)-1] = '\0';

    if(!open_inbox && new_context && context_isambig(newfolder)){
	if((p = folder_is_nick(newfolder, FOLDERS(new_context), FN_WHOLE_NAME)) != NULL){
	    strncpy(expanded_file, p, sizeof(expanded_file));
	    expanded_file[sizeof(expanded_file)-1] = '\0';
	    dprint((2, "broach_folder: nickname for %s is %s\n",
		       expanded_file ? expanded_file : "?",
		       newfolder ? newfolder : "?"));
	}
	else if((new_context->use & CNTXT_INCMNG)
		&& (folder_index(newfolder, new_context, FI_FOLDER) < 0)
		&& !is_absolute_path(newfolder)){
	    q_status_message1(SM_ORDER, 3, 4,
			    _("Can't find Incoming Folder %s."), newfolder);
	    if(stream)
	      pine_mail_close(stream);

	    return(0);
	}
    }

    /*--- Opening inbox, inbox has been already opened, the easy case ---*/
    /*
     * [ It is probably true that we could eliminate most of this special ]
     * [ inbox stuff and just get the inbox stream back when we do the    ]
     * [ context_open below, but figuring that out hasn't been done.      ]
     */
    if(open_inbox && (strm=sp_inbox_stream())){
      if(sp_dead_stream(strm)){
	/* 
	 * if dead INBOX, just close it and let it be reopened.
	 * This is different from the do_reopen case above,
	 * because we're going from another open mail folder to the
	 * dead INBOX.
	 */
	dprint((2, "INBOX was dead, closing before reopening\n"));
	pine_mail_actually_close(strm);
      }
      else{
	/*
	 * Clean up the mail_stream we're leaving.
	 */
	if(ps_global->mail_stream
	   && (!sp_flagged(ps_global->mail_stream, SP_PERMLOCKED)
	       || (sp_flagged(ps_global->mail_stream, SP_INBOX)
	           && F_ON(F_EXPUNGE_INBOX, ps_global))
	       || (!sp_flagged(ps_global->mail_stream, SP_INBOX)
		   && sp_flagged(ps_global->mail_stream, SP_PERMLOCKED)
	           && F_ON(F_EXPUNGE_STAYOPENS, ps_global))))
          expunge_and_close(ps_global->mail_stream, NULL,
			    sp_flagged(ps_global->mail_stream, SP_PERMLOCKED)
			      ? EC_NO_CLOSE : EC_NONE);
	else if(!sp_flagged(ps_global->mail_stream, SP_INBOX)
	        && sp_flagged(ps_global->mail_stream, SP_PERMLOCKED)){
	    /*
	     * We want to save our position in the folder so that when we
	     * come back to this folder again, we can place the cursor on
	     * a reasonable message number.
	     */
	    sp_set_saved_cur_msg_id(ps_global->mail_stream, NULL);

	    if(ps_global->mail_stream->nmsgs > 0L){
		cur = mn_get_cur(sp_msgmap(ps_global->mail_stream));
		raw = mn_m2raw(sp_msgmap(ps_global->mail_stream), cur);
		if(raw > 0L && raw <= ps_global->mail_stream->nmsgs)
		  env = pine_mail_fetchstructure(ps_global->mail_stream,
						 raw, NULL);
		
		if(env && env->message_id && env->message_id[0])
		  sp_set_saved_cur_msg_id(ps_global->mail_stream,
					  env->message_id);
	    }
	}

	/*
	 * Make the already open inbox the current mailbox.
	 */
	ps_global->mail_stream = strm;
	ps_global->msgmap      = sp_msgmap(strm);

	if(was_dead && pith_opt_icon_text)
	  (*pith_opt_icon_text)(NULL, IT_MCLOSED);

	dprint((7, "%ld %ld %x\n",
		   mn_get_cur(ps_global->msgmap),
                   mn_get_total(ps_global->msgmap),
		   ps_global->mail_stream));
	/*
	 * remember last context and folder
	 */
	if(context_isambig(ps_global->cur_folder)){
	    ps_global->context_last = ps_global->context_current;
	    snprintf(ps_global->context_current->last_folder,
		     sizeof(ps_global->context_current->last_folder),
		     "%s", ps_global->cur_folder);
	    ps_global->last_unambig_folder[0] = '\0';
	}
	else{
	    ps_global->context_last = NULL;
	    snprintf(ps_global->last_unambig_folder,
		     sizeof(ps_global->last_unambig_folder),
		     "%s", ps_global->cur_folder);
	}

	p = sp_fldr(ps_global->mail_stream) ? sp_fldr(ps_global->mail_stream)
					    : ps_global->inbox_name;
	strncpy(ps_global->cur_folder, p, sizeof(ps_global->cur_folder)-1);
	ps_global->cur_folder[sizeof(ps_global->cur_folder)-1] = '\0';
	ps_global->context_current = ps_global->context_list;
	reset_index_format();
	clear_index_cache(ps_global->mail_stream, 0);
        /* MUST sort before restoring msgno! */
	refresh_sort(ps_global->mail_stream, ps_global->msgmap, SRT_NON);
	report = new_messages_string(ps_global->mail_stream);
	q_status_message3(SM_ORDER, 0, 3,
			  (mn_get_total(ps_global->msgmap) > 1)
			    ? _("Opened folder \"%s\" with %s messages%s")
			    : _("Opened folder \"%s\" with %s message%s"),
			  ps_global->inbox_name, 
                          long2string(mn_get_total(ps_global->msgmap)),
			  report ? report : "");
	if(report)
	   fs_give((void **)&report);

#ifdef	_WINDOWS
	mswin_settitle(ps_global->inbox_name);
#endif
	if(stream)
	  pine_mail_close(stream);

	if(!(flags & DB_NOVISIT))
	  sp_set_recent_since_visited(ps_global->mail_stream, 0L);

	return(1);
      }
    }

    if(!new_context && !expand_foldername(expanded_file,sizeof(expanded_file))){
	if(stream)
	  pine_mail_close(stream);

	return(0);
    }

    /*
     * This is a safe time to clean up dead streams because nothing should
     * be referencing them right now.
     */
    sp_cleanup_dead_streams();

    old_folder = NULL;
    old_path   = NULL;
    old_sort   = SortArrival;			/* old sort */
    old_tros   = 0;				/* old reverse sort ? */
    /*---- now close the old one we had open if there was one ----*/
    if(ps_global->mail_stream != NULL){
        old_folder   = cpystr(ps_global->cur_folder);
        old_path     = cpystr(ps_global->mail_stream->original_mailbox
	                        ? ps_global->mail_stream->original_mailbox
				: ps_global->mail_stream->mailbox);
	old_sort     = mn_get_sort(ps_global->msgmap);
	old_tros     = mn_get_revsort(ps_global->msgmap);
	if(!sp_flagged(ps_global->mail_stream, SP_PERMLOCKED)
	    || (sp_flagged(ps_global->mail_stream, SP_INBOX)
	        && F_ON(F_EXPUNGE_INBOX, ps_global))
	    || (!sp_flagged(ps_global->mail_stream, SP_INBOX)
	        && sp_flagged(ps_global->mail_stream, SP_PERMLOCKED)
	        && F_ON(F_EXPUNGE_STAYOPENS, ps_global)))
          expunge_and_close(ps_global->mail_stream, NULL,
			    sp_flagged(ps_global->mail_stream, SP_PERMLOCKED)
			      ? EC_NO_CLOSE : EC_NONE);
	else if(!sp_flagged(ps_global->mail_stream, SP_INBOX)
	        && sp_flagged(ps_global->mail_stream, SP_PERMLOCKED)){
	    /*
	     * We want to save our position in the folder so that when we
	     * come back to this folder again, we can place the cursor on
	     * a reasonable message number.
	     */
	    
	    sp_set_saved_cur_msg_id(ps_global->mail_stream, NULL);

	    if(ps_global->mail_stream->nmsgs > 0L){
		cur = mn_get_cur(sp_msgmap(ps_global->mail_stream));
		raw = mn_m2raw(sp_msgmap(ps_global->mail_stream), cur);
		if(raw > 0L && raw <= ps_global->mail_stream->nmsgs)
		  env = pine_mail_fetchstructure(ps_global->mail_stream,
						 raw, NULL);
		
		if(env && env->message_id && env->message_id[0])
		  sp_set_saved_cur_msg_id(ps_global->mail_stream,
					  env->message_id);
	    }
	}

	ps_global->mail_stream = NULL;
    }

    snprintf(status_msg, sizeof(status_msg), "%sOpening \"", do_reopen ? "Re-" : "");
    strncat(status_msg, pretty_fn(newfolder),
	    sizeof(status_msg)-strlen(status_msg) - 2);
    status_msg[sizeof(status_msg)-2] = '\0';
    strncat(status_msg, "\"", sizeof(status_msg)-strlen(status_msg) - 1);
    status_msg[sizeof(status_msg)-1] = '\0';
    we_cancel = busy_cue(status_msg, NULL, 0);

    /* 
     * if requested, make access to folder readonly (only once)
     */
    if(ps_global->open_readonly_on_startup){
	openmode |= OP_READONLY;
	ps_global->open_readonly_on_startup = 0;
    }

    if(!(flags & DB_NOVISIT))
      ps_global->first_open_was_attempted = 1;

    openmode |= SP_USEPOOL;

    if(stream)
      sp_set_first_unseen(stream, 0L);

    m = context_open((new_context && !open_inbox) ? new_context : NULL,
		     stream, 
		     open_inbox ? ps_global->VAR_INBOX_PATH : expanded_file,
		     openmode | (open_inbox ? SP_INBOX : 0),
		     &rflags);

    /*
     * We aren't in a situation where we want a single cancel to
     * apply to multiple opens.
     */
    ps_global->user_says_cancel = 0;

    if(streamp)
      *streamp = m;


    dprint((8, "Opened folder %p \"%s\" (context: \"%s\")\n",
               m, (m && m->mailbox) ? m->mailbox : "nil",
	       (new_context && new_context->context)
	         ? new_context->context : "nil"));


    /* Can get m != NULL if correct passwd for remote, but wrong name */
    if(m == NULL || m->halfopen){
	/*-- non-existent local mailbox, or wrong passwd for remote mailbox--*/
        /* fall back to currently open mailbox */
	if(we_cancel)
	  cancel_busy_cue(-1);

	ps_global->mail_stream = NULL;
	if(m)
	  pine_mail_actually_close(m);

        rv = 0;
        dprint((8, "Old folder: \"%s\"\n",
               old_folder == NULL ? "" : old_folder));
        if(old_folder != NULL){
            if(strcmp(old_folder, ps_global->inbox_name) == 0){
                ps_global->mail_stream = sp_inbox_stream();
		ps_global->msgmap      = sp_msgmap(ps_global->mail_stream);

                dprint((8, "Reactivate inbox %ld %ld %p\n",
                           mn_get_cur(ps_global->msgmap),
                           mn_get_total(ps_global->msgmap),
                           ps_global->mail_stream));
		p = sp_fldr(ps_global->mail_stream)
			  ? sp_fldr(ps_global->mail_stream)
			  : ps_global->inbox_name;
		strncpy(ps_global->cur_folder, p,
			sizeof(ps_global->cur_folder)-1);
		ps_global->cur_folder[sizeof(ps_global->cur_folder)-1] = '\0';
            }
	    else{
                ps_global->mail_stream = pine_mail_open(NULL, old_path,
							openmode, &rflags);
                /* mm_log will take care of error message here */
                if(ps_global->mail_stream == NULL){
                    rv = -1;
                }
		else{
		    ps_global->msgmap = sp_msgmap(ps_global->mail_stream);
		    mn_set_sort(ps_global->msgmap, old_sort);
		    mn_set_revsort(ps_global->msgmap, old_tros);
		    ps_global->mangled_header = 1;
		    reset_index_format();
		    clear_index_cache(ps_global->mail_stream, 0);

		    if(!(rflags & SP_MATCH)){
			sp_set_expunge_count(ps_global->mail_stream, 0L);
			sp_set_new_mail_count(ps_global->mail_stream, 0L);
			sp_set_dead_stream(ps_global->mail_stream, 0);
			sp_set_noticed_dead_stream(ps_global->mail_stream, 0);

			reset_check_point(ps_global->mail_stream);
			if(IS_NEWS(ps_global->mail_stream)
			   && ps_global->mail_stream->rdonly)
			  msgno_exclude_deleted(ps_global->mail_stream,
					    sp_msgmap(ps_global->mail_stream));

			if(mn_get_total(ps_global->msgmap) > 0)
			  mn_set_cur(ps_global->msgmap,
				     first_sorted_flagged(F_NONE,
						      ps_global->mail_stream,
						      0L,
						      THREADING()
							  ? 0 : FSF_SKIP_CHID));

			if(!(mn_get_sort(ps_global->msgmap) == SortArrival
			     && !mn_get_revsort(ps_global->msgmap)))
			  refresh_sort(ps_global->mail_stream,
				       ps_global->msgmap, SRT_NON);
		    }

                    q_status_message1(SM_ORDER, 0, 3,
				      "Folder \"%s\" reopened", old_folder);
                }
            }

	    if(rv == 0)
	      mn_set_cur(ps_global->msgmap,
			 MIN(mn_get_cur(ps_global->msgmap), 
			     mn_get_total(ps_global->msgmap)));

            fs_give((void **)&old_folder);
            fs_give((void **)&old_path);
        }
	else
	  rv = -1;

        if(rv == -1){
            q_status_message(SM_ORDER | SM_DING, 0, 4, _("No folder opened"));
	    mn_set_total(ps_global->msgmap, 0L);
	    mn_set_nmsgs(ps_global->msgmap, 0L);
	    mn_set_cur(ps_global->msgmap, -1L);
	    ps_global->cur_folder[0] = '\0';
        }

	if(was_dead && !sp_a_locked_stream_is_dead() && pith_opt_icon_text)
	  (*pith_opt_icon_text)(NULL, IT_MCLOSED);

	if(ps_global->mail_stream && !(flags & DB_NOVISIT))
	  sp_set_recent_since_visited(ps_global->mail_stream, 0L);

	if(ps_global->mail_stream)
	  sp_set_first_unseen(ps_global->mail_stream, 0L);

        return(rv);
    }
    else{
        if(old_folder != NULL){
            fs_give((void **)&old_folder);
            fs_give((void **)&old_path);
        }
    }

    update_folder_unseen_by_stream(m, UFU_NONE);

    /*----- success in opening the new folder ----*/
    dprint((2, "Opened folder \"%s\" with %ld messages\n",
	       m->mailbox ? m->mailbox : "?", m->nmsgs));


    /*--- A Little house keeping ---*/

    ps_global->mail_stream = m;
    if(!(flags & DB_NOVISIT))
      sp_set_recent_since_visited(ps_global->mail_stream, 0L);

    ps_global->msgmap = sp_msgmap(m);
    if(!(rflags & SP_MATCH)){
	sp_set_expunge_count(m, 0L);
	sp_set_new_mail_count(m, 0L);
	sp_set_dead_stream(m, 0);
	sp_set_noticed_dead_stream(m, 0);
	sp_set_mail_box_changed(m, 0);
	reset_check_point(m);
    }

    if(was_dead && !sp_a_locked_stream_is_dead() && pith_opt_icon_text)
      (*pith_opt_icon_text)(NULL, IT_MCLOSED);

    ps_global->last_unambig_folder[0] = '\0';

    /*
     * remember old folder and context...
     */
    if(context_isambig(ps_global->cur_folder)){
	ps_global->context_last = ps_global->context_current;
	snprintf(ps_global->context_current->last_folder,
		 sizeof(ps_global->context_current->last_folder),
		 "%s", ps_global->cur_folder);
	ps_global->last_unambig_folder[0] = '\0';
    }
    else{
	ps_global->context_last = NULL;
	snprintf(ps_global->last_unambig_folder,
		 sizeof(ps_global->last_unambig_folder),
		 "%s", ps_global->cur_folder);
    }

    /* folder in a subdir of context? */
    if(ps_global->context_current->dir->prev)
      snprintf(ps_global->cur_folder, sizeof(ps_global->cur_folder), "%s%s",
		ps_global->context_current->dir->ref, newfolder);
    else{
	strncpy(ps_global->cur_folder,
		(open_inbox) ? ps_global->inbox_name : newfolder,
		sizeof(ps_global->cur_folder)-1);
	ps_global->cur_folder[sizeof(ps_global->cur_folder)-1] = '\0';
    }

    sp_set_fldr(ps_global->mail_stream, ps_global->cur_folder);

    if(new_context){
	ps_global->context_last    = ps_global->context_current;
	ps_global->context_current = new_context;

	if(!open_inbox)
	  sp_set_context(ps_global->mail_stream, ps_global->context_current);
    }

    clear_index_cache(ps_global->mail_stream, 0);
    reset_index_format();

    /*
     * Start news reading with messages the user's marked deleted
     * hidden from view...
     */
    if(IS_NEWS(ps_global->mail_stream) && ps_global->mail_stream->rdonly)
      msgno_exclude_deleted(ps_global->mail_stream, ps_global->msgmap);

    if(we_cancel && F_OFF(F_QUELL_FILTER_MSGS, ps_global))
      cancel_busy_cue(0);

    /*
     * If the stream we got from the open above was already opened earlier
     * for some temporary use, then it wouldn't have been filtered. That's
     * why we need this flag, so that we will filter if needed.
     */
    if(!sp_flagged(ps_global->mail_stream, SP_FILTERED))
      process_filter_patterns(ps_global->mail_stream, ps_global->msgmap, 0L);

    /*
     * If no filtering messages wait until here to cancel the busy cue
     * because the user will be waiting for that filtering with nothing
     * showing the activity otherwise.
     */
    if(we_cancel && F_ON(F_QUELL_FILTER_MSGS, ps_global))
      cancel_busy_cue(0);

    if(!(rflags & SP_MATCH) || !(rflags & SP_LOCKED))
      reset_sort_order(SRT_VRB);
    else if(sp_new_mail_count(ps_global->mail_stream) > 0L
          || sp_unsorted_newmail(ps_global->mail_stream)
          || sp_need_to_rethread(ps_global->mail_stream))
      refresh_sort(ps_global->mail_stream, ps_global->msgmap, SRT_NON);

    report = new_messages_string(ps_global->mail_stream);
    q_status_message7(SM_ORDER, 0, 4,
		    "%s \"%s\" opened with %s message%s%s%s%s",
			IS_NEWS(ps_global->mail_stream)
			  ? "News group" : "Folder",
			open_inbox ? pretty_fn(newfolder) : newfolder,
			comatose(mn_get_total(ps_global->msgmap)),
			plural(mn_get_total(ps_global->msgmap)),
			(!open_inbox
			 && sp_flagged(ps_global->mail_stream, SP_PERMLOCKED))
			    ? " (StayOpen)" : "",
			READONLY_FOLDER(ps_global->mail_stream)
						? " READONLY" : "",
			report ? report : "");

   if(report)
     fs_give((void **)&report);

#ifdef	_WINDOWS
    mswin_settitle(pretty_fn(newfolder));
#endif
    /*
     * Set current message number when re-opening Stay-Open or
     * cached folders.
     */
    if(rflags & SP_MATCH){
	if(rflags & SP_LOCKED){
	    if(F_OFF(F_STARTUP_STAYOPEN, ps_global)
	       && (cur = get_msgno_by_msg_id(ps_global->mail_stream,
			     sp_saved_cur_msg_id(ps_global->mail_stream),
			     ps_global->msgmap)) >= 1L
	       && cur <= mn_get_total(ps_global->msgmap)){
	      cur_already_set++;
	      mn_set_cur(ps_global->msgmap, (MsgNo) cur);
	      if(flags & DB_FROMTAB){
		/*
		 * When we TAB to a folder that is a StayOpen folder we try
		 * to increment the current message # by one instead of doing
		 * some search again. Some people probably won't like this
		 * behavior, especially if the new message that has arrived
		 * comes before where we are in the index. That's why we have
		 * the F_STARTUP_STAYOPEN feature above.
		 */
		mn_inc_cur(m, ps_global->msgmap, MH_NONE);
	      }
	      /* else leave it where it is */

	      adjust_cur_to_visible(ps_global->mail_stream, ps_global->msgmap);
	    }
	}
	else{
	    /*
	     * If we're reopening a cached open stream that wasn't explicitly
	     * kept open by the user, then the user expects it to act pretty
	     * much like we are re-opening the stream. A problem is that the
	     * recent messages are still recent because we haven't closed the
	     * stream, so we fake a quasi-recentness by remembering the last
	     * uid assigned on the stream when we pine_mail_close. Then when
	     * we come back messages with uids higher than that are recent.
	     *
	     * If uid_validity has changed, then we don't use any special
	     * treatment, but just do the regular search.
	     */
	    if(m->uid_validity == sp_saved_uid_validity(m)){
		long i;

		/*
		 * Because first_sorted_flagged uses sequence numbers, find the
		 * sequence number of the first message after the old last
		 * uid assigned. I.e., the first recent message.
		 */
		for(i = m->nmsgs; i > 0L; i--)
		  if(mail_uid(m, i) <= sp_saved_uid_last(m))
		    break;
		
		if(i > 0L && i < m->nmsgs)
		  pc = i+1L;
	    }
	}
    }


    if(!cur_already_set && mn_get_total(ps_global->msgmap) > 0L){

	perfolder_startup_rule = reset_startup_rule(ps_global->mail_stream);

	if(ps_global->start_entry > 0){
	    mn_set_cur(ps_global->msgmap, mn_get_revsort(ps_global->msgmap)
		       ? first_sorted_flagged(F_NONE, m,
					      ps_global->start_entry,
					      THREADING() ? 0 : FSF_SKIP_CHID)
		       : first_sorted_flagged(F_SRCHBACK, m,
					      ps_global->start_entry,
					      THREADING() ? 0 : FSF_SKIP_CHID));
	    ps_global->start_entry = 0;
        }
	else if(perfolder_startup_rule != IS_NOTSET ||
	        open_inbox ||
		ps_global->context_current->use & CNTXT_INCMNG){
	    unsigned use_this_startup_rule;

	    if(perfolder_startup_rule != IS_NOTSET)
	      use_this_startup_rule = perfolder_startup_rule;
	    else
	      use_this_startup_rule = ps_global->inc_startup_rule;

	    switch(use_this_startup_rule){
	      /*
	       * For news in incoming collection we're doing the same thing
	       * for first-unseen and first-recent. In both those cases you
	       * get first-unseen if FAKE_NEW is off and first-recent if
	       * FAKE_NEW is on. If FAKE_NEW is on, first unseen is the
	       * same as first recent because all recent msgs are unseen
	       * and all unrecent msgs are seen (see pine_mail_open).
	       */
	      case IS_FIRST_UNSEEN:
first_unseen:
		mn_set_cur(ps_global->msgmap,
			(sp_first_unseen(m)
			 && mn_get_sort(ps_global->msgmap) == SortArrival
			 && !mn_get_revsort(ps_global->msgmap)
			 && !get_lflag(ps_global->mail_stream, NULL,
				       sp_first_unseen(m), MN_EXLD)
			 && (n = mn_raw2m(ps_global->msgmap, 
					  sp_first_unseen(m))))
			   ? n
			   : first_sorted_flagged(F_UNSEEN | F_UNDEL, m, pc,
					      THREADING() ? 0 : FSF_SKIP_CHID));
		break;

	      case IS_FIRST_RECENT:
first_recent:
		/*
		 * We could really use recent for news but this is the way
		 * it has always worked, so we'll leave it. That is, if
		 * the FAKE_NEW feature is on, recent and unseen are
		 * equivalent, so it doesn't matter. If the feature isn't
		 * on, all the undeleted messages are unseen and we start
		 * at the first one. User controls with the FAKE_NEW feature.
		 */
		if(IS_NEWS(ps_global->mail_stream)){
		    mn_set_cur(ps_global->msgmap,
			       first_sorted_flagged(F_UNSEEN|F_UNDEL, m, pc,
					       THREADING() ? 0 : FSF_SKIP_CHID));
		}
		else{
		    mn_set_cur(ps_global->msgmap,
			       first_sorted_flagged(F_RECENT | F_UNSEEN
						    | F_UNDEL,
						    m, pc,
					      THREADING() ? 0 : FSF_SKIP_CHID));
		}
		break;

	      case IS_FIRST_IMPORTANT:
		mn_set_cur(ps_global->msgmap,
			   first_sorted_flagged(F_FLAG|F_UNDEL, m, pc,
					      THREADING() ? 0 : FSF_SKIP_CHID));
		break;

	      case IS_FIRST_IMPORTANT_OR_UNSEEN:

		if(IS_NEWS(ps_global->mail_stream))
		  goto first_unseen;

		{
		    MsgNo flagged, first_unseen;

		    flagged = first_sorted_flagged(F_FLAG|F_UNDEL, m, pc,
					       THREADING() ? 0 : FSF_SKIP_CHID);
		    first_unseen = (sp_first_unseen(m)
			     && mn_get_sort(ps_global->msgmap) == SortArrival
			     && !mn_get_revsort(ps_global->msgmap)
			     && !get_lflag(ps_global->mail_stream, NULL,
					   sp_first_unseen(m), MN_EXLD)
			     && (n = mn_raw2m(ps_global->msgmap, 
					      sp_first_unseen(m))))
				? n
				: first_sorted_flagged(F_UNSEEN|F_UNDEL, m, pc,
					       THREADING() ? 0 : FSF_SKIP_CHID);
		    mn_set_cur(ps_global->msgmap,
			  (MsgNo) MIN((int) flagged, (int) first_unseen));

		}

		break;

	      case IS_FIRST_IMPORTANT_OR_RECENT:

		if(IS_NEWS(ps_global->mail_stream))
		  goto first_recent;

		{
		    MsgNo flagged, first_recent;

		    flagged = first_sorted_flagged(F_FLAG|F_UNDEL, m, pc,
					       THREADING() ? 0 : FSF_SKIP_CHID);
		    first_recent = first_sorted_flagged(F_RECENT | F_UNSEEN
							| F_UNDEL,
							m, pc,
					       THREADING() ? 0 : FSF_SKIP_CHID);
		    mn_set_cur(ps_global->msgmap,
			      (MsgNo) MIN((int) flagged, (int) first_recent));
		}

		break;

	      case IS_FIRST:
		mn_set_cur(ps_global->msgmap,
			   first_sorted_flagged(F_UNDEL, m, pc,
					      THREADING() ? 0 : FSF_SKIP_CHID));
		break;

	      case IS_LAST:
		mn_set_cur(ps_global->msgmap,
			   first_sorted_flagged(F_UNDEL, m, pc,
			         FSF_LAST | (THREADING() ? 0 : FSF_SKIP_CHID)));
		break;

	      default:
		panic("Unexpected incoming startup case");
		break;

	    }
	}
	else if(IS_NEWS(ps_global->mail_stream)){
	    /*
	     * This will go to two different places depending on the FAKE_NEW
	     * feature (see pine_mail_open).
	     */
	    mn_set_cur(ps_global->msgmap,
		       first_sorted_flagged(F_UNSEEN|F_UNDEL, m, pc,
					      THREADING() ? 0 : FSF_SKIP_CHID));
	}
        else{
	    mn_set_cur(ps_global->msgmap,
		       mn_get_revsort(ps_global->msgmap)
		         ? 1L
			 : mn_get_total(ps_global->msgmap));
	}

	adjust_cur_to_visible(ps_global->mail_stream, ps_global->msgmap);
    }
    else if(!(rflags & SP_MATCH)){
	mn_set_cur(ps_global->msgmap, -1L);
    }

    if(ps_global->mail_stream)
      sp_set_first_unseen(ps_global->mail_stream, 0L);

    return(1);
}


/*----------------------------------------------------------------------
      Expand a folder name, taking account of the folders_dir and 
      any home directory reference

  Args: filename -- The name of the file that is the folder
 
 Result: The folder name is expanded in place.  
         Returns 0 and queues status message if unsuccessful.
         Input string is overwritten with expanded name.
         Returns 1 if successful.

  ----*/
int
expand_foldername(char *filename, size_t len)
{
    char temp_filename[MAXPATH+1];

    dprint((5, "=== expand_foldername called (%s) ===\n",
	   filename ? filename : "?"));

    /*
     * We used to check for valid filename chars here if "filename"
     * didn't refer to a remote mailbox.  This has been rethought
     */

    strncpy(temp_filename, filename, sizeof(temp_filename)-1);
    temp_filename[sizeof(temp_filename)-1] = '\0';
    if(strucmp(temp_filename, "inbox") == 0) {
        strncpy(filename, ps_global->VAR_INBOX_PATH == NULL ? "inbox" :
                ps_global->VAR_INBOX_PATH, len-1);
	filename[len-1] = '\0';
    } else if(temp_filename[0] == '{') {
        strncpy(filename, temp_filename, len-1);
	filename[len-1] = '\0';
    } else if(ps_global->restricted && filename_is_restricted(temp_filename)){
	q_status_message(SM_ORDER, 0, 3, "Can only open local folders");
	return(0);
    } else if(temp_filename[0] == '*') {
        strncpy(filename, temp_filename, len-1);
	filename[len-1] = '\0';
    } else if(ps_global->VAR_OPER_DIR && filename_parent_ref(temp_filename)){
	q_status_message(SM_ORDER, 0, 3,
			 "\"..\" not allowed in folder name");
	return(0);
    } else if (is_homedir_path(temp_filename)){
        if(fnexpand(temp_filename, sizeof(temp_filename)) == NULL) {
    	    q_status_message1(SM_ORDER, 3, 3, "Error expanding folder \"%s\"", temp_filename);
    	    return(0);
        }
        strncpy(filename, temp_filename, len-1);
	filename[len-1] = '\0';
    } else if(F_ON(F_USE_CURRENT_DIR, ps_global) || is_absolute_path(temp_filename)){
        strncpy(filename, temp_filename, len-1);
	filename[len-1] = '\0';
    } else if(ps_global->VAR_OPER_DIR){
	build_path(filename, ps_global->VAR_OPER_DIR, temp_filename, len);
    } else {
	build_path(filename,
#ifdef	IS_WINDOWS
		   ps_global->folders_dir,
#else	/* UNIX */
		   ps_global->home_dir,
#endif	/* UNIX */
		   temp_filename, len);
    }

    dprint((5, "returning \"%s\"\n", filename ? filename : "?"));    
    return(1);
}



/*----------------------------------------------------------------------
      Expunge (if confirmed) and close a mail stream

    Args: stream   -- The MAILSTREAM * to close
        final_msg  -- If non-null, this should be set to point to a
		      message to print out in the caller, it is allocated
		      here and freed by the caller.

   Result:  Mail box is expunged and closed. A message is displayed to
             say what happened
 ----*/
void
expunge_and_close(MAILSTREAM *stream, char **final_msg, long unsigned int flags)
{
    long  i, delete_count, seen_not_del;
    char  buff1[MAX_SCREEN_COLS+1], *moved_msg = NULL,
	  buff2[MAX_SCREEN_COLS+1], *folder;
    CONTEXT_S *context;
    struct variable *vars = ps_global->vars;
    int ret, expunge = FALSE, no_close = 0;
    char ing[4];

    no_close = (flags & EC_NO_CLOSE);

    if(!(stream && sp_flagged(stream, SP_LOCKED)))
      stream = NULL;

    /* check for dead stream */
    if(stream && sp_dead_stream(stream)){
	pine_mail_actually_close(stream);
	stream = NULL;
    }

    if(stream != NULL){
	context = sp_context(stream);
	folder  = STREAMNAME(stream);

        dprint((2, "expunge_and_close: \"%s\"%s\n",
                   folder, no_close ? " (NO_CLOSE bit set)" : ""));

	update_folder_unseen_by_stream(stream, UFU_NONE);

	if(final_msg)
	  strncpy(ing, "ed", sizeof(ing));
	else
	  strncpy(ing, "ing", sizeof(ing));

	ing[sizeof(ing)-1] = '\0';

	buff1[0] = '\0';
	buff2[0] = '\0';

        if(!stream->rdonly){

	    if(pith_opt_begin_closing)
	      (*pith_opt_begin_closing)(flags, folder);

	    mail_expunge_prefilter(stream, MI_CLOSING);

	    /*
	     * Be sure to expunge any excluded (filtered) msgs
	     * Do it here so they're not copied into read/archived
	     * folders *AND* to be sure we don't refilter them
	     * next time the folder's opened.
	     */
	    for(i = 1L; i <= stream->nmsgs; i++)
	      if(get_lflag(stream, NULL, i, MN_EXLD)){	/* if there are any */
		  delete_filtered_msgs(stream);		/* delete them all */
		  expunge = TRUE;
		  break;
	      }

	    /* Save read messages? */
	    if(VAR_READ_MESSAGE_FOLDER && VAR_READ_MESSAGE_FOLDER[0]
	       && sp_flagged(stream, SP_INBOX)
	       && (seen_not_del = count_flagged(stream, F_SEEN | F_UNDEL))){

		if(F_ON(F_AUTO_READ_MSGS,ps_global)
		   || (pith_opt_read_msg_prompt
		       && (*pith_opt_read_msg_prompt)(seen_not_del, VAR_READ_MESSAGE_FOLDER)))
		  /* move inbox's read messages */
		  moved_msg = move_read_msgs(stream, VAR_READ_MESSAGE_FOLDER,
					     buff1, sizeof(buff1), -1L);
	    }
	    else if(VAR_ARCHIVED_FOLDERS)
	      moved_msg = move_read_incoming(stream, context, folder,
					     VAR_ARCHIVED_FOLDERS,
					     buff1, sizeof(buff1));

	    /*
	     * We need the count_flagged to be executed not only to set
	     * delete_count, but also to set the searched bits in all of
	     * the deleted messages. The searched bit is used in the monkey
	     * business section below which undeletes deleted messages
	     * before expunging. It determines which messages are deleted
	     * by examining the searched bit, which had better be set or not
	     * based on this count_flagged call rather than some random
	     * search that happened earlier.
	     */
            delete_count = count_flagged(stream, F_DEL);
	    if(F_ON(F_EXPUNGE_MANUALLY,ps_global))
              delete_count = 0L;

	    ret = 'n';
	    if(!ps_global->noexpunge_on_close && delete_count){

		if(F_ON(F_FULL_AUTO_EXPUNGE,ps_global)
		   || (F_ON(F_AUTO_EXPUNGE, ps_global)
		       && ((!strucmp(folder,ps_global->inbox_name))
			   || (context && (context->use & CNTXT_INCMNG)))
		       && context_isambig(folder))){
		    ret = 'y';
		}
		else if(pith_opt_expunge_prompt)
		  ret = (*pith_opt_expunge_prompt)(stream, pretty_fn(folder), delete_count);

		/* get this message back in queue */
		if(moved_msg)
		  q_status_message(SM_ORDER,
		      F_ON(F_AUTO_READ_MSGS,ps_global) ? 0 : 3, 5, moved_msg);

		if(ret == 'y'){
		    long filtered;

		    filtered = any_lflagged(sp_msgmap(stream), MN_EXLD);

		    snprintf(buff2, sizeof(buff2),
		      "%s%s%s%.30s%s%s %s message%s and remov%s %s.",
			no_close ? "" : "Clos",
			no_close ? "" : ing,
			no_close ? "" : " \"",
	 		no_close ? "" : pretty_fn(folder),
			no_close ? "" : "\". ",
			final_msg ? "Kept" : "Keeping",
			comatose(stream->nmsgs - filtered - delete_count),
			plural(stream->nmsgs - filtered - delete_count),
			ing,
			long2string(delete_count));
		    if(final_msg)
		      *final_msg = cpystr(buff2);
		    else
		      q_status_message(SM_ORDER,
				       no_close ? 1 :
				        (F_ON(F_AUTO_EXPUNGE,ps_global)
					 || F_ON(F_FULL_AUTO_EXPUNGE,ps_global))
					         ? 0 : 3,
				       5, buff2);
		      
		    flush_status_messages(1);
		    ps_global->mm_log_error = 0;
		    ps_global->expunge_in_progress = 1;
		    mail_expunge(stream);
		    ps_global->expunge_in_progress = 0;
		    if(ps_global->mm_log_error && final_msg && *final_msg){
			fs_give((void **)final_msg);
			*final_msg = NULL;
		    }
		}
	    }

	    if(ret != 'y'){
		if(!ps_global->noexpunge_on_close && expunge){
		    MESSAGECACHE *mc;
		    char	 *seq;
		    int		  expbits;

		    /*
		     * filtered message monkey business.
		     * The Plan:
		     *   1) light sequence bits for legit deleted msgs
		     *      and store marker in local extension
		     *   2) clear their deleted flag
		     *   3) perform expunge to removed filtered msgs
		     *   4) restore deleted flags for legit msgs
		     *      based on local extension bit
		     */
		    for(i = 1L; i <= stream->nmsgs; i++)
		      if(!get_lflag(stream, NULL, i, MN_EXLD)
			 && (((mc = mail_elt(stream, i)) && mc->valid && mc->deleted)
			     || (mc && !mc->valid && mc->searched))){
			  mc->sequence = 1;
			  expbits      = MSG_EX_DELETE;
			  msgno_exceptions(stream, i, "0", &expbits, TRUE);
		      }
		      else if((mc = mail_elt(stream, i)) != NULL)
			mc->sequence = 0;

		    if((seq = build_sequence(stream, NULL, NULL)) != NULL){
			mail_flag(stream, seq, "\\DELETED", ST_SILENT);
			fs_give((void **) &seq);
		    }

		    ps_global->mm_log_error = 0;
		    ps_global->expunge_in_progress = 1;
		    mail_expunge(stream);
		    ps_global->expunge_in_progress = 0;

		    for(i = 1L; i <= stream->nmsgs; i++)
		      if((mc = mail_elt(stream, i)) != NULL)
		        mc->sequence 
			   = (msgno_exceptions(stream, i, "0", &expbits, FALSE)
			      && (expbits & MSG_EX_DELETE));

		    if((seq = build_sequence(stream, NULL, NULL)) != NULL){
			mail_flag(stream, seq, "\\DELETED", ST_SET|ST_SILENT);
			fs_give((void **) &seq);
		    }
		}

		if(!no_close){
		    if(stream->nmsgs){
			snprintf(buff2, sizeof(buff2),
			    "Clos%s folder \"%.*s\". %s%s%s message%s.",
			    ing,
			    sizeof(buff2)-50, pretty_fn(folder), 
			    final_msg ? "Kept" : "Keeping",
			    (stream->nmsgs == 1L) ? " single" : " all ",
			    (stream->nmsgs > 1L)
			      ? comatose(stream->nmsgs) : "",
			    plural(stream->nmsgs));
		    }
		    else{
			snprintf(buff2, sizeof(buff2), "Clos%s empty folder \"%.*s\"",
			    ing, sizeof(buff2)-50, pretty_fn(folder));
		    }

		    if(final_msg)
		      *final_msg = cpystr(buff2);
		    else
		      q_status_message(SM_ORDER, 0, 3, buff2);
		}
	    }
        }
	else{
            if(IS_NEWS(stream)){
		/*
		 * Mark the filtered messages deleted so they aren't
		 * filtered next time.
		 */
	        for(i = 1L; i <= stream->nmsgs; i++){
		  int exbits;
		  if(msgno_exceptions(stream, i, "0" , &exbits, FALSE)
		     && (exbits & MSG_EX_FILTERED)){
		    delete_filtered_msgs(stream);
		    break;
		  }
		}
		/* first, look to archive read messages */
		if((moved_msg = move_read_incoming(stream, context, folder,
						  VAR_ARCHIVED_FOLDERS,
						  buff1, sizeof(buff1))) != NULL)
		  q_status_message(SM_ORDER,
		      F_ON(F_AUTO_READ_MSGS,ps_global) ? 0 : 3, 5, moved_msg);

		snprintf(buff2, sizeof(buff2), "Clos%s news group \"%.*s\"",
			ing, sizeof(buff2)-50, pretty_fn(folder));

		if(F_ON(F_NEWS_CATCHUP, ps_global)){
		    MESSAGECACHE *mc;

		    /* count visible messages */
		    (void) count_flagged(stream, F_DEL);
		    for(i = 1L, delete_count = 0L; i <= stream->nmsgs; i++)
		      if(!(get_lflag(stream, NULL, i, MN_EXLD)
			   || ((mc = mail_elt(stream, i)) && mc->valid
				&& mc->deleted)
			   || (mc && !mc->valid && mc->searched)))
			delete_count++;

		    if(delete_count && pith_opt_expunge_prompt){
			ret = (*pith_opt_expunge_prompt)(stream, pretty_fn(folder), delete_count);
			if(ret == 'y'){
			    char seq[64];

			    snprintf(seq, sizeof(seq), "1:%ld", stream->nmsgs);
			    mail_flag(stream, seq, "\\DELETED", ST_SET|ST_SILENT);
			}
		    }
		}

		if(F_ON(F_NEWS_CROSS_DELETE, ps_global))
		  cross_delete_crossposts(stream);
	    }
            else
	      snprintf(buff2, sizeof(buff2),
			"Clos%s read-only folder \"%.*s\". No changes to save",
			ing, sizeof(buff2)-60, pretty_fn(folder));

	    if(final_msg)
	      *final_msg = cpystr(buff2);
	    else
	      q_status_message(SM_ORDER, 0, 2, buff2);
        }

	/*
	 * Make darn sure any mm_log fallout caused above get's seen...
	 */
	if(!no_close){
	    flush_status_messages(1);
	    pine_mail_close(stream);
	}
    }
}


void
agg_select_all(MAILSTREAM *stream, MSGNO_S *msgmap, long int *diff, int on)
{ 
    long i;
    int  hidden = any_lflagged(msgmap, MN_HIDE) > 0L;

    for(i = 1L; i <= mn_get_total(msgmap); i++){
	if(on){		/* mark 'em all */
	  set_lflag(stream, msgmap, i, MN_SLCT, 1);
	}
	else {		/* unmark 'em all */
	    if(get_lflag(stream, msgmap, i, MN_SLCT)){
		if(diff)
		  (*diff)++;

		set_lflag(stream, msgmap, i, MN_SLCT, 0);
	    }
	    else if(hidden)
	      set_lflag(stream, msgmap, i, MN_HIDE, 0);
	}
    }
}


/*----------------------------------------------------------------------
  Move all read messages from srcfldr to dstfldr
 
  Args: stream -- stream to usr
	dstfldr -- folder to receive moved messages
	buf -- place to write success message

  Returns: success message or NULL for failure
  ----*/
char *
move_read_msgs(MAILSTREAM *stream, char *dstfldr, char *buf, size_t buflen, long int searched)
{
    long	  i, raw;
    int           we_cancel = 0;
    MSGNO_S	 *msgmap = NULL;
    CONTEXT_S	 *save_context = NULL;
    char	  *bufp = NULL;
    MESSAGECACHE *mc;

    if(!is_absolute_path(dstfldr)
       && !(save_context = default_save_context(ps_global->context_list)))
      save_context = ps_global->context_list;

    /*
     * Use the "searched" bit to select the set of messages
     * we want to save.  If searched is non-neg, the message
     * cache already has the necessary "searched" bits set.
     */
    if(searched < 0L)
      searched = count_flagged(stream, F_SEEN | F_UNDEL);

    if(searched){
	/*
	 * We're going to be messing with SLCT flags in order
	 * to do our work. If this stream is a StayOpen stream
	 * we want to restore those flags after we're done
	 * using them. So copy them into STMP so we can put them
	 * back below.
	 */
	msgmap = sp_msgmap(stream);
	if(sp_flagged(stream, SP_PERMLOCKED))
	  copy_lflags(stream, msgmap, MN_SLCT, MN_STMP);

	set_lflags(stream, msgmap, MN_SLCT, 0);

	/* select search results */
	for(i = 1L; i <= mn_get_total(msgmap); i++)
	  if((raw = mn_m2raw(msgmap, i)) > 0L && stream
	     && raw <= stream->nmsgs
	     && (mc = mail_elt(stream,raw))
	     && ((mc->valid && mc->seen && !mc->deleted)
	         || (!mc->valid && mc->searched)))
	    set_lflag(stream, msgmap, i, MN_SLCT, 1);

	pseudo_selected(stream, msgmap);
	snprintf(buf, buflen, "Moving %s read message%s to \"%s\"",
		comatose(searched), plural(searched), dstfldr);
	we_cancel = busy_cue(buf, NULL, 0);
	if(save(ps_global, stream, save_context, dstfldr, msgmap,
		SV_DELETE | SV_FIX_DELS | SV_INBOXWOCNTXT) == searched)
	  strncpy(bufp = buf + 1, "Moved", MIN(5,buflen)); /* change Moving to Moved */

	buf[buflen-1] = '\0';
	if(we_cancel)
	  cancel_busy_cue(bufp ? 0 : -1);

	if(sp_flagged(stream, SP_PERMLOCKED)){
	    restore_selected(msgmap);
	    copy_lflags(stream, msgmap, MN_STMP, MN_SLCT);
	}
    }

    return(bufp);
}


/*----------------------------------------------------------------------
  Move read messages from folder if listed in archive
 
  Args: 

  ----*/
char *
move_read_incoming(MAILSTREAM *stream, CONTEXT_S *context, char *folder,
		   char **archive, char *buf, size_t buflen)
{
    char *s, *d, *f = folder;
    long  seen_undel;

    if(buf && buflen > 0)
      buf[0] = '\0';

    if(archive && !sp_flagged(stream, SP_INBOX)
       && context && (context->use & CNTXT_INCMNG)
       && ((context_isambig(folder)
	    && folder_is_nick(folder, FOLDERS(context), 0))
	   || folder_index(folder, context, FI_FOLDER) > 0)
       && (seen_undel = count_flagged(stream, F_SEEN | F_UNDEL))){

	for(; f && *archive; archive++){
	    char *p;

	    get_pair(*archive, &s, &d, 1, 0);
	    if(s && d
	       && (!strcmp(s, folder)
		   || (context_isambig(folder)
		       && (p = folder_is_nick(folder, FOLDERS(context), 0))
		       && !strcmp(s, p)))){
		if(F_ON(F_AUTO_READ_MSGS,ps_global)
		   || (pith_opt_read_msg_prompt
		       && (*pith_opt_read_msg_prompt)(seen_undel, d)))
		  buf = move_read_msgs(stream, d, buf, buflen, seen_undel);

		f = NULL;		/* bust out after cleaning up */
	    }

	    fs_give((void **)&s);
	    fs_give((void **)&d);
	}
    }

    return((buf && *buf) ? buf : NULL);
}


/*----------------------------------------------------------------------
    Delete all references to a deleted news posting

 
  ---*/
void
cross_delete_crossposts(MAILSTREAM *stream)
{
    if(count_flagged(stream, F_DEL)){
	static char *fields[] = {"Xref", NULL};
	MAILSTREAM  *tstream;
	CONTEXT_S   *fake_context;
	char	    *xref, *p, *group, *uidp,
		    *newgrp, newfolder[MAILTMPLEN];
	long	     i, hostlatch = 0L;
	imapuid_t    uid;
	int	     we_cancel = 0;
	MESSAGECACHE *mc;

	strncpy(newfolder, stream->mailbox, sizeof(newfolder));
	newfolder[sizeof(newfolder)-1] = '\0';
	if(!(newgrp = strstr(newfolder, "#news.")))
	  return;				/* weird mailbox */

	newgrp += 6;

	we_cancel = busy_cue("Busy deleting crosspostings", NULL, 1);

	/* build subscribed list */
	strncpy(newgrp, "[]", sizeof(newfolder)-(newgrp-newfolder));
	newfolder[sizeof(newfolder)-1] = '\0';
	fake_context = new_context(newfolder, 0);
	build_folder_list(NULL, fake_context, "*", NULL, BFL_LSUB);

	for(i = 1L; i <= stream->nmsgs; i++)
	  if(!get_lflag(stream, NULL, i, MN_EXLD)
	     && (mc = mail_elt(stream, i)) && mc->deleted){

	      if((xref = pine_fetchheader_lines(stream, i, NULL, fields)) != NULL){
		  if((p = strstr(xref, ": ")) != NULL){
		      p	     += 2;
		      hostlatch = 0L;
		      while(*p){
			  group = p;
			  uidp  = NULL;

			  /* get server */
			  while(*++p && !isspace((unsigned char) *p))
			    if(*p == ':'){
				*p   = '\0';
				uidp = p + 1;
			    }

			  /* tie off uid/host */
			  if(*p)
			    *p++ = '\0';

			  if(uidp){
			      /*
			       * For the nonce, we're only deleting valid
			       * uid's from outside the current newsgroup
			       * and inside only subscribed newsgroups
			       */
			      if(strcmp(group, stream->mailbox
							+ (newgrp - newfolder))
				 && folder_index(group, fake_context,
						 FI_FOLDER) >= 0){
				  if((uid = strtoul(uidp, NULL, 10)) != 0L){
				      strncpy(newgrp, group, sizeof(newfolder)-(newgrp-newfolder));
				      newfolder[sizeof(newfolder)-1] = '\0';
				      if((tstream = pine_mail_open(NULL,
								  newfolder,
								  SP_USEPOOL,
								  NULL)) != NULL){
					  mail_flag(tstream, ulong2string(uid),
						    "\\DELETED",
						    ST_SET | ST_UID);
					  pine_mail_close(tstream);
				      }
				  }
				  else
				    break;		/* bogus uid */
			      }
			  }
			  else if(!hostlatch++){
			      char *p, *q;

			      if(stream->mailbox[0] == '{'
				 && !((p = strpbrk(stream->mailbox+1, "}:/"))
				      && !struncmp(stream->mailbox + 1,
						   q = canonical_name(group),
						   p - (stream->mailbox + 1))
				      && q[p - (stream->mailbox + 1)] == '\0'))
				break;		/* different server? */
			  }
			  else
			    break;		/* bogus field! */
		      }
		  }
		  
		  fs_give((void **) &xref);
	      }
	  }

	free_context(&fake_context);

	if(we_cancel)
	  cancel_busy_cue(0);
    }
}


/*
 * Original version from Eduardo Chappa.
 *
 * Returns a string describing the number of new/unseen messages
 * for use in the status line. Can return NULL. Caller must free the memory.
 */
char *
new_messages_string(MAILSTREAM *stream)
{
    char message[80] = {'\0'};
    long new = 0L, uns = 0L;
    int i, imapstatus = 0;

    for (i = 0; ps_global->index_disp_format[i].ctype != iNothing
		&& ps_global->index_disp_format[i].ctype != iIStatus
		&& ps_global->index_disp_format[i].ctype != iSIStatus; i++)
      ;

    imapstatus = ps_global->index_disp_format[i].ctype == iIStatus
		 || ps_global->index_disp_format[i].ctype == iSIStatus;

    get_new_message_count(stream, imapstatus, &new, &uns);

    if(imapstatus)
      snprintf(message, sizeof(message), " - %s%s%s%s%s%s%s",
	       uns != 0L ? comatose((long) new) : "",
	       uns != 0L ? " " : "",
	       uns != 0L ? _("recent") : "",
	       uns > 0L  ? ", " : "",
	       uns != -1L ? comatose((long) uns) : "",
	       uns != -1L ? " " : "",
	       uns != -1L ? _("unseen") : "");
    else if(!imapstatus && new > 0L)
      snprintf(message, sizeof(message), " - %s %s",
	       comatose((long) new), _("new"));

    return(*message ? cpystr(message) : NULL);
}


void
get_new_message_count(MAILSTREAM *stream, int imapstatus,
		      long *new, long *unseen)
{
    if(new)
      *new = 0L;

    if(unseen)
      *unseen = 0L;

    if(imapstatus){
	if(new)
	  *new = count_flagged(stream, F_RECENT | F_UNSEEN | F_UNDEL);

	if(!IS_NEWS(stream)){
	    if(unseen)
	      *unseen = count_flagged(stream, F_UNSEEN | F_UNDEL);
	}
	else if(unseen)
	  *unseen = -1L;
    }
    else{
	if(IS_NEWS(stream)){
	    if(F_ON(F_FAKE_NEW_IN_NEWS, ps_global)){
		if(new)
		  *new = count_flagged(stream, F_RECENT | F_UNSEEN | F_UNDEL);
	    }
	}
	else{
	    if(new)
	      *new = count_flagged(stream, F_UNSEEN | F_UNDEL | F_UNANS);
	}
    }
}


/*----------------------------------------------------------------------
  ZOOM the message index (set any and all necessary hidden flag bits)

   Args: state -- usual pine state
	 msgmap -- usual message mapping
   Returns: number of messages zoomed in on

  ----*/
long
zoom_index(struct pine *state, MAILSTREAM *stream, MSGNO_S *msgmap, int onflag)
{
    long        i, count = 0L, first = 0L, msgno;
    PINETHRD_S *thrd = NULL, *topthrd = NULL, *nthrd;

    if(any_lflagged(msgmap, onflag)){

	if(THREADING() && sp_viewing_a_thread(stream)){
	    /* get top of current thread */
	    thrd = fetch_thread(stream, mn_m2raw(msgmap, mn_get_cur(msgmap)));
	    if(thrd && thrd->top)
	      topthrd = fetch_thread(stream, thrd->top);
	}

	for(i = 1L; i <= mn_get_total(msgmap); i++){
	    if(!get_lflag(stream, msgmap, i, onflag)){
		set_lflag(stream, msgmap, i, MN_HIDE, 1);
	    }
	    else{

		/*
		 * Because subject lines depend on whether or not
		 * other parts of the thread above us are visible or not.
		 */
		if(THREADING() && !THRD_INDX()
		   && ps_global->thread_disp_style == THREAD_MUTTLIKE)
		  clear_index_cache_ent(stream, i, 0);

		/*
		 * If a selected message is hidden beneath a collapsed
		 * thread (not beneath a thread index line, but a collapsed
		 * thread or subthread) then we make it visible. The user
		 * should be able to see the selected messages when they
		 * Zoom. We could get a bit fancier and re-collapse the
		 * thread when the user unzooms, but we don't do that
		 * for now.
		 */
		if(THREADING() && !THRD_INDX()
		   && get_lflag(stream, msgmap, i, MN_CHID)){

		    /*
		     * What we need to do is to unhide this message and
		     * uncollapse any parent above us.
		     * Also, when we uncollapse a parent, we need to
		     * trace back down the tree and unhide until we get
		     * to a collapse point or the end. That's what
		     * set_thread_subtree does.
		     */

		    thrd = fetch_thread(stream, mn_m2raw(msgmap, i));

		    if(thrd && thrd->parent)
		      thrd = fetch_thread(stream, thrd->parent);
		    else
		      thrd = NULL;

		    /* unhide and uncollapse its parents */
		    while(thrd){
			/* if this parent is collapsed */
			if(get_lflag(stream, NULL, thrd->rawno, MN_COLL)){
			    /* uncollapse this parent and unhide its subtree */
			    msgno = mn_raw2m(msgmap, thrd->rawno);
			    if(msgno > 0L && msgno <= mn_get_total(msgmap)){
				set_lflag(stream, msgmap, msgno,
					  MN_COLL | MN_CHID, 0);
				if(thrd->next &&
				   (nthrd = fetch_thread(stream, thrd->next)))
				  set_thread_subtree(stream, nthrd, msgmap,
						     0, MN_CHID);
			    }

			    /* collapse symbol will be wrong */
			    clear_index_cache_ent(stream, msgno, 0);
			}

			/*
			 * Continue up tree to next parent looking for
			 * more collapse points.
			 */
			if(thrd->parent)
			  thrd = fetch_thread(stream, thrd->parent);
			else
			  thrd = NULL;
		    }
		}

		count++;
		if(!first){
		    if(THRD_INDX()){
			/* find msgno of top of thread for msg i */
			if((thrd=fetch_thread(stream, mn_m2raw(msgmap, i)))
			    && thrd->top)
			  first = mn_raw2m(msgmap, thrd->top);
		    }
		    else if(THREADING() && sp_viewing_a_thread(stream)){
			/* want first selected message in this thread */
			if(topthrd
			   && (thrd=fetch_thread(stream, mn_m2raw(msgmap, i)))
			   && thrd->top
			   && topthrd->rawno == thrd->top)
			  first = i;
		    }
		    else
		      first = i;
		}
	    }
	}

	if(THRD_INDX()){
	    thrd = fetch_thread(stream, mn_m2raw(msgmap, mn_get_cur(msgmap)));
	    if(count_lflags_in_thread(stream, thrd, msgmap, onflag) == 0)
	      mn_set_cur(msgmap, first);
	}
	else if((THREADING() && sp_viewing_a_thread(stream))
	        || !get_lflag(stream, msgmap, mn_get_cur(msgmap), onflag)){
	    if(!first){
		int flags = 0;

		/*
		 * Nothing was selected in the thread we were in, so
		 * drop back to the Thread Index instead. Set the current
		 * thread to the first one that has a selection in it.
		 */

		unview_thread(state, stream, msgmap);

		i = next_sorted_flagged(F_UNDEL, stream, 1L, &flags);
		
		if(flags & NSF_FLAG_MATCH
		   && (thrd=fetch_thread(stream, mn_m2raw(msgmap, i)))
		    && thrd->top)
		  first = mn_raw2m(msgmap, thrd->top);
		else
		  first = 1L;	/* can't happen */

		mn_set_cur(msgmap, first);
	    }
	    else{
		if(msgline_hidden(stream, msgmap, mn_get_cur(msgmap), 0))
		  mn_set_cur(msgmap, first);
	    }
	}
    }

    return(count);
}



/*----------------------------------------------------------------------
  UnZOOM the message index (clear any and all hidden flag bits)

   Args: state -- usual pine state
	 msgmap -- usual message mapping
   Returns: 1 if hidden bits to clear and they were, 0 if none to clear

  ----*/
int
unzoom_index(struct pine *state, MAILSTREAM *stream, MSGNO_S *msgmap)
{
    register long i;

    if(!any_lflagged(msgmap, MN_HIDE))
      return(0);

    for(i = 1L; i <= mn_get_total(msgmap); i++)
      set_lflag(stream, msgmap, i, MN_HIDE, 0);

    return(1);
}


int
agg_text_select(MAILSTREAM *stream, MSGNO_S *msgmap, char type, int not,
		int check_for_my_addresses,
		char *sstring, char *charset, SEARCHSET **limitsrch)
{
    int		 old_imap, we_cancel;
    int          me_with_regex = 0;
    long         searchflags;
    SEARCHPGM   *srchpgm, *pgm, *secondpgm = NULL, *thirdpgm = NULL;
    SEARCHPGM   *mepgm = NULL;

    if(!stream)
      return(1);

    old_imap = (is_imap_stream(stream) && !modern_imap_stream(stream));

    /*
     * Special case code for matching one of the user's addresses.
     */
    if(check_for_my_addresses){
	char **t, *alt;

	if(F_OFF(F_DISABLE_REGEX, ps_global)){
	    for(t = ps_global->VAR_ALT_ADDRS; !me_with_regex && t && t[0] && t[0][0]; t++){
		alt = (*t);
		if(contains_regex_special_chars(alt))
		  me_with_regex++;
	    }
	}

	/*
	 * In this case we can't use search because it doesn't support
	 * regex. So we have to manually do the whole thing ourselves.
	 * The searching is done in the subroutine and the searched bits
	 * will be set on return.
	 */
	if(me_with_regex){
	    search_for_our_regex_addresses(stream, type, not, limitsrch ? *limitsrch : NULL);
	    return(0);
	}
	else{
	    PATGRP_S *patgrp = NULL;
	    PATTERN_S *p = NULL;
	    PATTERN_S *pattern = NULL, **nextp;
	    char buf[1000];

	    /*
	     * We're going to use the pattern matching machinery to generate
	     * a search program. We build a pattern whose only purpose is
	     * to generate the program.
	     */
	    nextp = &pattern;

	    /* add standard me addresses to list */
	    if(ps_global->VAR_USER_ID){
		if(ps_global->userdomain && ps_global->userdomain[0]){
		    p = (PATTERN_S *) fs_get(sizeof(*p));
		    memset((void *) p, 0, sizeof(*p));
		    snprintf(buf, sizeof(buf), "%s@%s", ps_global->VAR_USER_ID,
			     ps_global->userdomain);
		    p->substring = cpystr(buf);
		    *nextp = p;
		    nextp = &p->next;
		}

		if(!ps_global->userdomain && ps_global->localdomain && ps_global->localdomain[0]){
		    p = (PATTERN_S *) fs_get(sizeof(*p));
		    memset((void *) p, 0, sizeof(*p));
		    snprintf(buf, sizeof(buf), "%s@%s", ps_global->VAR_USER_ID,
			     ps_global->localdomain);
		    p->substring = cpystr(buf);
		    *nextp = p;
		    nextp = &p->next;
		}

		if(!ps_global->userdomain && ps_global->hostname && ps_global->hostname[0]){
		    p = (PATTERN_S *) fs_get(sizeof(*p));
		    memset((void *) p, 0, sizeof(*p));
		    snprintf(buf, sizeof(buf), "%s@%s", ps_global->VAR_USER_ID,
			     ps_global->hostname);
		    p->substring = cpystr(buf);
		    *nextp = p;
		    nextp = &p->next;
		}
	    }

	    /* add user's alternate addresses */
	    for(t = ps_global->VAR_ALT_ADDRS; t && t[0] && t[0][0]; t++){
		alt = (*t);
		if(alt && alt[0]){
		    p = (PATTERN_S *) fs_get(sizeof(*p));
		    memset((void *) p, 0, sizeof(*p));
		    p->substring = cpystr(alt);
		    *nextp = p;
		    nextp = &p->next;
		}
	    }

	    patgrp = (PATGRP_S *) fs_get(sizeof(*patgrp));
	    memset((void *) patgrp, 0, sizeof(*patgrp));

	    switch(type){
	      case 'r' :
		patgrp->recip = pattern;
		break;
	      case 'p' :
		patgrp->partic = pattern;
		break;
	      case 'f' :
		patgrp->from = pattern;
		break;
	      case 'c' :
		patgrp->cc = pattern;
		break;
	      case 't' :
		patgrp->to = pattern;
	        break;
	      default :
		q_status_message(SM_ORDER, 3, 3, "Unhandled case in agg_text_select");
	        break;
	    }

	    mepgm = match_pattern_srchpgm(patgrp, stream, NULL);

	    free_patgrp(&patgrp);
	}
    }

    if(mepgm){
	if(not && !old_imap){
	    srchpgm = mail_newsearchpgm();
	    srchpgm->not = mail_newsearchpgmlist();
	    srchpgm->not->pgm = mepgm;
	}
	else{
	    srchpgm = mepgm;
	}

    }
    else{
	/* create a search program and fill it in */
	srchpgm = pgm = mail_newsearchpgm();
	if(not && !old_imap){
	    srchpgm->not = mail_newsearchpgmlist();
	    srchpgm->not->pgm = mail_newsearchpgm();
	    pgm = srchpgm->not->pgm;
	}
    }

  if(!mepgm)
    switch(type){
      case 'r' :				/* TO or CC */
	if(old_imap){
	    /* No OR on old servers */
	    pgm->to = mail_newstringlist();
	    pgm->to->text.data = (unsigned char *) cpystr(sstring);
	    pgm->to->text.size = strlen(sstring);
	    secondpgm = mail_newsearchpgm();
	    secondpgm->cc = mail_newstringlist();
	    secondpgm->cc->text.data = (unsigned char *) cpystr(sstring);
	    secondpgm->cc->text.size = strlen(sstring);
	}
	else{
	    pgm->or = mail_newsearchor();
	    pgm->or->first->to = mail_newstringlist();
	    pgm->or->first->to->text.data = (unsigned char *) cpystr(sstring);
	    pgm->or->first->to->text.size = strlen(sstring);
	    pgm->or->second->cc = mail_newstringlist();
	    pgm->or->second->cc->text.data = (unsigned char *) cpystr(sstring);
	    pgm->or->second->cc->text.size = strlen(sstring);
	}

	break;

      case 'p' :				/* TO or CC or FROM */
	if(old_imap){
	    /* No OR on old servers */
	    pgm->to = mail_newstringlist();
	    pgm->to->text.data = (unsigned char *) cpystr(sstring);
	    pgm->to->text.size = strlen(sstring);
	    secondpgm = mail_newsearchpgm();
	    secondpgm->cc = mail_newstringlist();
	    secondpgm->cc->text.data = (unsigned char *) cpystr(sstring);
	    secondpgm->cc->text.size = strlen(sstring);
	    thirdpgm = mail_newsearchpgm();
	    thirdpgm->from = mail_newstringlist();
	    thirdpgm->from->text.data = (unsigned char *) cpystr(sstring);
	    thirdpgm->from->text.size = strlen(sstring);
	}
	else{
	    pgm->or = mail_newsearchor();
	    pgm->or->first->to = mail_newstringlist();
	    pgm->or->first->to->text.data = (unsigned char *) cpystr(sstring);
	    pgm->or->first->to->text.size = strlen(sstring);

	    pgm->or->second->or = mail_newsearchor();
	    pgm->or->second->or->first->cc = mail_newstringlist();
	    pgm->or->second->or->first->cc->text.data =
					    (unsigned char *) cpystr(sstring);
	    pgm->or->second->or->first->cc->text.size = strlen(sstring);
	    pgm->or->second->or->second->from = mail_newstringlist();
	    pgm->or->second->or->second->from->text.data =
					    (unsigned char *) cpystr(sstring);
	    pgm->or->second->or->second->from->text.size = strlen(sstring);
	}

	break;

      case 'f' :				/* FROM */
	pgm->from = mail_newstringlist();
	pgm->from->text.data = (unsigned char *) cpystr(sstring);
	pgm->from->text.size = strlen(sstring);
	break;

      case 'c' :				/* CC */
	pgm->cc = mail_newstringlist();
	pgm->cc->text.data = (unsigned char *) cpystr(sstring);
	pgm->cc->text.size = strlen(sstring);
	break;

      case 't' :				/* TO */
	pgm->to = mail_newstringlist();
	pgm->to->text.data = (unsigned char *) cpystr(sstring);
	pgm->to->text.size = strlen(sstring);
	break;

      case 's' :				/* SUBJECT */
	pgm->subject = mail_newstringlist();
	pgm->subject->text.data = (unsigned char *) cpystr(sstring);
	pgm->subject->text.size = strlen(sstring);
	break;

      case 'a' :				/* ALL TEXT */
	pgm->text = mail_newstringlist();
	pgm->text->text.data = (unsigned char *) cpystr(sstring);
	pgm->text->text.size = strlen(sstring);
	break;

      case 'b' :				/* ALL BODY TEXT */
	pgm->body = mail_newstringlist();
	pgm->body->text.data = (unsigned char *) cpystr(sstring);
	pgm->body->text.size = strlen(sstring);
	break;

      default :
	dprint((1,"\n - BOTCH: select_text unrecognized type\n"));
	return(1);
    }

    /*
     * If we happen to have any messages excluded, make sure we
     * don't waste time searching their text...
     */
    srchpgm->msgno = (limitsrch ? *limitsrch : NULL);

    /* TRANSLATORS: warning to user that we're busy selecting messages */
    we_cancel = busy_cue(_("Busy Selecting"), NULL, 1);

    searchflags = SE_NOPREFETCH | (secondpgm ? 0 : SE_FREE);

    pine_mail_search_full(stream, !old_imap ? charset : NULL, srchpgm,
			  searchflags);
    
    /* search for To or Cc; or To or Cc or From on old imap server */
    if(secondpgm){
	if(srchpgm){
	    srchpgm->msgno = NULL;
	    mail_free_searchpgm(&srchpgm);
	}

	secondpgm->msgno = (limitsrch ? *limitsrch : NULL);
	searchflags |= (SE_RETAIN | (thirdpgm ? 0 : SE_FREE));

	pine_mail_search_full(stream, NULL, secondpgm, searchflags);

	if(thirdpgm){
	    if(secondpgm){
		secondpgm->msgno = NULL;
		mail_free_searchpgm(&secondpgm);
	    }

	    thirdpgm->msgno = (limitsrch ? *limitsrch : NULL);
	    searchflags |= SE_FREE;
	    pine_mail_search_full(stream, NULL, thirdpgm, searchflags);
	}
    }

    /* we know this was freed in mail_search, let caller know */
    if(limitsrch)
      *limitsrch = NULL;

    if(old_imap && not){
	MESSAGECACHE *mc;
	long	      msgno;

	/* 
	 * Old imap server doesn't have a NOT, so we actually searched for
	 * the subject (or whatever) instead of !subject. Flip the searched
	 * bits.
	 */
	for(msgno = 1L; msgno <= mn_get_total(msgmap); msgno++)
	    if(stream && msgno <= stream->nmsgs
	       && (mc=mail_elt(stream, msgno)) && mc->searched)
	      mc->searched = NIL;
	    else
	      mc->searched = T;
    }

    if(we_cancel)
      cancel_busy_cue(0);

    return(0);
}


void
search_for_our_regex_addresses(MAILSTREAM *stream, char type, int not,
			       SEARCHSET *searchset)
{
    long rawno, count = 0L;
    MESSAGECACHE *mc;
    ADDRESS *addr1 = NULL, *addr2 = NULL, *addr3 = NULL;
    ENVELOPE *env;
    SEARCHSET *s, *ss = NULL;
    extern MAILSTREAM *mm_search_stream;
    extern long        mm_search_count;

    mm_search_count = 0L;
    mm_search_stream = stream;

    if(!stream)
      return;

    /* set searched bits to zero */
    for(rawno = 1L; rawno <= stream->nmsgs; rawno++)
      if((mc=mail_elt(stream, rawno)) != NULL)
	mc->searched = NIL;

    /* set sequence bits for envelopes we need */
    for(rawno = 1L; rawno <= stream->nmsgs; rawno++){
	if((mc = mail_elt(stream, rawno)) != NULL){
	    if((!searchset || in_searchset(searchset, (unsigned long) rawno))
	       && !mc->private.msg.env){
		mc->sequence = 1;
		count++;
	    }
	    else
	      mc->sequence = 0;
	}
    }

    /*
     * Set up a searchset that will control the fetch ahead.
     */
    if(count){
	ss = build_searchset(stream);
	if(ss){
	    SEARCHSET **sset = NULL;

	    mail_parameters(NULL, SET_FETCHLOOKAHEADLIMIT, (void *) count);

	    /* this resets automatically after the first fetch */
	    sset = (SEARCHSET **) mail_parameters(stream,
						  GET_FETCHLOOKAHEAD,
						  (void *) stream);
	    if(sset)
	      *sset = ss;
	}
    }

    for(s = searchset; s; s = s->next){
      for(rawno = s->first; rawno <= s->last; rawno++){
	env = pine_mail_fetchenvelope(stream, rawno);
	addr1 = addr2 = addr3 = NULL;
	switch(type){
	  case 'r' :
	    addr1 = env ? env->to : NULL;
	    addr2 = env ? env->cc : NULL;
	    break;
	  case 'p' :
	    addr1 = env ? env->to : NULL;
	    addr2 = env ? env->cc : NULL;
	    addr3 = env ? env->from : NULL;
	    break;
	  case 'f' :
	    addr1 = env ? env->from : NULL;
	    break;
	  case 'c' :
	    addr1 = env ? env->cc : NULL;
	    break;
	    break;
	  case 't' :
	    addr1 = env ? env->to : NULL;
	    break;
	  default :
	    q_status_message(SM_ORDER, 3, 3, "Unhandled case2 in agg_text_select");
	    break;
	}

	if(addr1 && address_is_us(addr1, ps_global)){
	  if((mc=mail_elt(stream, rawno)) != NULL)
	    mm_searched(stream, rawno);
	}
	else if(addr2 && address_is_us(addr2, ps_global)){
	  if((mc=mail_elt(stream, rawno)) != NULL)
	    mm_searched(stream, rawno);
	}
	else if(addr3 && address_is_us(addr3, ps_global)){
	  if((mc=mail_elt(stream, rawno)) != NULL)
	    mm_searched(stream, rawno);
	}
      }
    }

    if(ss)
      mail_free_searchset(&ss);

    if(not){
	for(rawno = 1L; rawno <= stream->nmsgs; rawno++){
	    if((mc=mail_elt(stream, rawno)) && mc->searched)
	      mc->searched = NIL;
	    else
	      mc->searched = T;
	}
    }
}


int
agg_flag_select(MAILSTREAM *stream, int not, int crit, SEARCHSET **limitsrch)
{
    SEARCHPGM *pgm;

    pgm = mail_newsearchpgm();
    switch(crit){
      case 'n' :
	if(not){
	    SEARCHPGM *notpgm;

	    /* this is the same as seen or deleted or answered */
	    pgm->not = mail_newsearchpgmlist();
	    notpgm = pgm->not->pgm = mail_newsearchpgm();
	    notpgm->unseen = notpgm->undeleted = notpgm->unanswered = 1;
	}
	else
	  pgm->unseen = pgm->undeleted = pgm->unanswered = 1;
	  
	break;

      case 'd' :
	if(not)
	  pgm->undeleted = 1;
	else
	  pgm->deleted = 1;

	break;

      case 'r' :
	if(not)
	  pgm->old = 1;
	else
	  pgm->recent = 1;

	break;

      case 'u' :
	if(not)
	  pgm->seen = 1;
	else
	  pgm->unseen = 1;

	break;

      case 'a':
	/*
	 * Not a true "not", we are implicitly only interested in undeleted.
	 */
	if(not)
	  pgm->unanswered = pgm->undeleted = 1;
	else
	  pgm->answered = pgm->undeleted = 1;
	break;

      case 'f':
        {
	    STRINGLIST **slpp;

	    for(slpp = (not) ? &pgm->unkeyword : &pgm->keyword;
		*slpp;
		slpp = &(*slpp)->next)
	      ;

	    *slpp = mail_newstringlist();
	    (*slpp)->text.data = (unsigned char *) cpystr(FORWARDED_FLAG);
	    (*slpp)->text.size = (unsigned long) strlen(FORWARDED_FLAG);
	}

	break;

      case '*' :
	if(not)
	  pgm->unflagged = 1;
	else
	  pgm->flagged = 1;
	  
	break;

      default :
	return(1);
	break;
    }

    pgm->msgno = (limitsrch ? *limitsrch : NULL);
    pine_mail_search_full(stream, NULL, pgm, SE_NOPREFETCH | SE_FREE);
    /* we know this was freed in mail_search, let caller know */
    if(limitsrch)
      *limitsrch = NULL;

    return(0);
}


/*
 * Get the user name from the mailbox portion of an address.
 *
 * Args: mailbox -- the mailbox portion of an address (lhs of address)
 *       target  -- a buffer to put the result in
 *       len     -- length of the target buffer
 *
 * Returns the left most portion up to the first '%', ':' or '@',
 * and to the right of any '!' (as if c-client would give us such a mailbox).
 * Returns NULL if it can't find a username to point to.
 */
char *
get_uname(char *mailbox, char *target, int len)
{
    int i, start, end;

    if(!mailbox || !*mailbox)
      return(NULL);

    end = strlen(mailbox) - 1;
    for(start = end; start > -1 && mailbox[start] != '!'; start--)
        if(strindex("%:@", mailbox[start]))
	    end = start - 1;

    start++;			/* compensate for either case above */

    for(i = start; i <= end && (i-start) < (len-1); i++) /* copy name */
      target[i-start] = isupper((unsigned char)mailbox[i])
					  ? tolower((unsigned char)mailbox[i])
					  : mailbox[i];

    target[i-start] = '\0';	/* tie it off */

    return(*target ? target : NULL);
}
