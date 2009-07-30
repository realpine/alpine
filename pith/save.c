#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: save.c 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $";
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

#include "../pith/headers.h"
#include "../pith/save.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/mimedesc.h"
#include "../pith/filter.h"
#include "../pith/context.h"
#include "../pith/folder.h"
#include "../pith/copyaddr.h"
#include "../pith/mailview.h"
#include "../pith/mailcmd.h"
#include "../pith/bldaddr.h"
#include "../pith/flag.h"
#include "../pith/status.h"
#include "../pith/ablookup.h"
#include "../pith/news.h"
#include "../pith/util.h"
#include "../pith/reply.h"
#include "../pith/sequence.h"
#include "../pith/stream.h"
#include "../pith/options.h"


/*
 * Internal prototypes
 */
int	  save_ex_replace_body(char *, unsigned long *,BODY *,gf_io_t);
int	  save_ex_output_body(MAILSTREAM *, long, char *, BODY *, unsigned long *, gf_io_t);
int	  save_ex_mask_types(char *, unsigned long *, gf_io_t);
int	  save_ex_explain_body(BODY *, unsigned long *, gf_io_t);
int	  save_ex_explain_parts(BODY *, int, unsigned long *, gf_io_t);
int	  save_ex_output_line(char *, unsigned long *, gf_io_t);


/*
 * pith hook
 */
int (*pith_opt_save_create_prompt)(CONTEXT_S *, char *, int);
int (*pith_opt_save_size_changed_prompt)(long, int);



/*----------------------------------------------------------------------
  save_get_default - return default folder name for saving
 ----*/
char *
save_get_default(struct pine *state, ENVELOPE *e, long int rawno,
		 char *section, CONTEXT_S **cntxt)
{
    int               context_was_set;

    if(!cntxt)
      return("");

    context_was_set = ((*cntxt) != NULL);

    /* start with the default save context */
    if(!(*cntxt)
       && ((*cntxt) = default_save_context(state->context_list)) == NULL)
       (*cntxt) = state->context_list;

    if(!e || ps_global->save_msg_rule == SAV_RULE_LAST
	  || ps_global->save_msg_rule == SAV_RULE_DEFLT){
	if(ps_global->save_msg_rule == SAV_RULE_LAST && ps_global->last_save_context){
	    if(!context_was_set)
	      (*cntxt) = ps_global->last_save_context;
	}
	else{
	    strncpy(ps_global->last_save_folder,
		    ps_global->VAR_DEFAULT_SAVE_FOLDER,
		    sizeof(ps_global->last_save_folder)-1);
	    ps_global->last_save_folder[sizeof(ps_global->last_save_folder)-1] = '\0';

	    /*
	     * If the user entered "inbox" as their default save folder it is very
	     * likely they meant the real inbox, not the inbox in the primary collection.
	     */
	    if(!context_was_set
	      && !strucmp(ps_global->inbox_name, ps_global->last_save_folder))
	      (*cntxt) = state->context_list;
	}
    }
    else{
	save_get_fldr_from_env(ps_global->last_save_folder,
			       sizeof(ps_global->last_save_folder),
			       e, state, rawno, section);
	/* somebody expunged current message */
	if(sp_expunge_count(ps_global->mail_stream))
	  return(NULL);
    }

    return(ps_global->last_save_folder);
}



/*----------------------------------------------------------------------
   Grope through envelope to find default folder name to save to

  Args: fbuf     --  Buffer to return result in
        nfbuf    --  Size of fbuf
        e        --  The envelope to look in
        state    --  Usual pine state
        rawmsgno --  Raw c-client sequence number of message
	section  --  Mime section of header data (for message/rfc822)

 Result: The appropriate default folder name is copied into fbuf.
 ----*/
void
save_get_fldr_from_env(char *fbuf, int nfbuf, ENVELOPE *e, struct pine *state,
		       long int rawmsgno, char *section)
{
    char     fakedomain[2];
    ADDRESS *tmp_adr = NULL;
    char     buf[MAX(MAXFOLDER,MAX_NICKNAME) + 1];
    char    *bufp;
    char    *folder_name = NULL;
    static char botch[] = "programmer botch, unknown message save rule";
    unsigned save_msg_rule;

    if(!e)
      return;

    /* copy this because we might change it below */
    save_msg_rule = state->save_msg_rule;

    /* first get the relevant address to base the folder name on */
    switch(save_msg_rule){
      case SAV_RULE_FROM:
      case SAV_RULE_NICK_FROM:
      case SAV_RULE_NICK_FROM_DEF:
      case SAV_RULE_FCC_FROM:
      case SAV_RULE_FCC_FROM_DEF:
      case SAV_RULE_RN_FROM:
      case SAV_RULE_RN_FROM_DEF:
        tmp_adr = e->from ? copyaddr(e->from)
			  : e->sender ? copyaddr(e->sender) : NULL;
	break;

      case SAV_RULE_SENDER:
      case SAV_RULE_NICK_SENDER:
      case SAV_RULE_NICK_SENDER_DEF:
      case SAV_RULE_FCC_SENDER:
      case SAV_RULE_FCC_SENDER_DEF:
      case SAV_RULE_RN_SENDER:
      case SAV_RULE_RN_SENDER_DEF:
        tmp_adr = e->sender ? copyaddr(e->sender)
			    : e->from ? copyaddr(e->from) : NULL;
	break;

      case SAV_RULE_REPLYTO:
      case SAV_RULE_NICK_REPLYTO:
      case SAV_RULE_NICK_REPLYTO_DEF:
      case SAV_RULE_FCC_REPLYTO:
      case SAV_RULE_FCC_REPLYTO_DEF:
      case SAV_RULE_RN_REPLYTO:
      case SAV_RULE_RN_REPLYTO_DEF:
        tmp_adr = e->reply_to ? copyaddr(e->reply_to)
			  : e->from ? copyaddr(e->from)
			  : e->sender ? copyaddr(e->sender) : NULL;
	break;

      case SAV_RULE_RECIP:
      case SAV_RULE_NICK_RECIP:
      case SAV_RULE_NICK_RECIP_DEF:
      case SAV_RULE_FCC_RECIP:
      case SAV_RULE_FCC_RECIP_DEF:
      case SAV_RULE_RN_RECIP:
      case SAV_RULE_RN_RECIP_DEF:
	/* news */
	if(state->mail_stream && IS_NEWS(state->mail_stream)){
	    char *tmp_a_string, *ng_name;

	    fakedomain[0] = '@';
	    fakedomain[1] = '\0';

	    /* find the news group name */
	    if((ng_name = strstr(state->mail_stream->mailbox,"#news")) != NULL)
	      ng_name += 6;
	    else
	      ng_name = state->mail_stream->mailbox; /* shouldn't happen */

	    /* copy this string so rfc822_parse_adrlist can't blast it */
	    tmp_a_string = cpystr(ng_name);
	    /* make an adr */
	    rfc822_parse_adrlist(&tmp_adr, tmp_a_string, fakedomain);
	    fs_give((void **)&tmp_a_string);
	    if(tmp_adr && tmp_adr->host && tmp_adr->host[0] == '@')
	      tmp_adr->host[0] = '\0';
	}
	/* not news */
	else{
	    static char *fields[] = {"Resent-To", NULL};
	    char *extras, *values[sizeof(fields)/sizeof(fields[0])];

	    extras = pine_fetchheader_lines(state->mail_stream, rawmsgno,
					    section, fields);
	    if(extras){
		long i;

		memset(values, 0, sizeof(fields));
		simple_header_parse(extras, fields, values);
		fs_give((void **)&extras);

		for(i = 0; i < sizeof(fields)/sizeof(fields[0]); i++)
		  if(values[i]){
		      if(tmp_adr)		/* take last matching value */
			mail_free_address(&tmp_adr);

		      /* build a temporary address list */
		      fakedomain[0] = '@';
		      fakedomain[1] = '\0';
		      rfc822_parse_adrlist(&tmp_adr, values[i], fakedomain);
		      fs_give((void **)&values[i]);
		  }
	    }

	    if(!tmp_adr)
	      tmp_adr = e->to ? copyaddr(e->to) : NULL;
	}

	break;
      
      default:
	panic(botch);
	break;
    }

    /* For that address, lookup the fcc or nickname from address book */
    switch(save_msg_rule){
      case SAV_RULE_NICK_FROM:
      case SAV_RULE_NICK_SENDER:
      case SAV_RULE_NICK_REPLYTO:
      case SAV_RULE_NICK_RECIP:
      case SAV_RULE_FCC_FROM:
      case SAV_RULE_FCC_SENDER:
      case SAV_RULE_FCC_REPLYTO:
      case SAV_RULE_FCC_RECIP:
      case SAV_RULE_NICK_FROM_DEF:
      case SAV_RULE_NICK_SENDER_DEF:
      case SAV_RULE_NICK_REPLYTO_DEF:
      case SAV_RULE_NICK_RECIP_DEF:
      case SAV_RULE_FCC_FROM_DEF:
      case SAV_RULE_FCC_SENDER_DEF:
      case SAV_RULE_FCC_REPLYTO_DEF:
      case SAV_RULE_FCC_RECIP_DEF:
	switch(save_msg_rule){
	  case SAV_RULE_NICK_FROM:
	  case SAV_RULE_NICK_SENDER:
	  case SAV_RULE_NICK_REPLYTO:
	  case SAV_RULE_NICK_RECIP:
	  case SAV_RULE_NICK_FROM_DEF:
	  case SAV_RULE_NICK_SENDER_DEF:
	  case SAV_RULE_NICK_REPLYTO_DEF:
	  case SAV_RULE_NICK_RECIP_DEF:
	    bufp = get_nickname_from_addr(tmp_adr, buf, sizeof(buf));
	    break;

	  case SAV_RULE_FCC_FROM:
	  case SAV_RULE_FCC_SENDER:
	  case SAV_RULE_FCC_REPLYTO:
	  case SAV_RULE_FCC_RECIP:
	  case SAV_RULE_FCC_FROM_DEF:
	  case SAV_RULE_FCC_SENDER_DEF:
	  case SAV_RULE_FCC_REPLYTO_DEF:
	  case SAV_RULE_FCC_RECIP_DEF:
	    bufp = get_fcc_from_addr(tmp_adr, buf, sizeof(buf));
	    break;
	}

	if(bufp && *bufp){
	    istrncpy(fbuf, bufp, nfbuf - 1);
	    fbuf[nfbuf - 1] = '\0';
	}
	else
	  /* fall back to non-nick/non-fcc version of rule */
	  switch(save_msg_rule){
	    case SAV_RULE_NICK_FROM:
	    case SAV_RULE_FCC_FROM:
	      save_msg_rule = SAV_RULE_FROM;
	      break;

	    case SAV_RULE_NICK_SENDER:
	    case SAV_RULE_FCC_SENDER:
	      save_msg_rule = SAV_RULE_SENDER;
	      break;

	    case SAV_RULE_NICK_REPLYTO:
	    case SAV_RULE_FCC_REPLYTO:
	      save_msg_rule = SAV_RULE_REPLYTO;
	      break;

	    case SAV_RULE_NICK_RECIP:
	    case SAV_RULE_FCC_RECIP:
	      save_msg_rule = SAV_RULE_RECIP;
	      break;
	    
	    default:
	      istrncpy(fbuf, ps_global->VAR_DEFAULT_SAVE_FOLDER, nfbuf - 1);
	      fbuf[nfbuf - 1] = '\0';
	      break;
	  }

	break;
    }

    /* Realname */
    switch(save_msg_rule){
      case SAV_RULE_RN_FROM_DEF:
      case SAV_RULE_RN_FROM:
      case SAV_RULE_RN_SENDER_DEF:
      case SAV_RULE_RN_SENDER:
      case SAV_RULE_RN_RECIP_DEF:
      case SAV_RULE_RN_RECIP:
      case SAV_RULE_RN_REPLYTO_DEF:
      case SAV_RULE_RN_REPLYTO:
        /* Fish out the realname */
	if(tmp_adr && tmp_adr->personal && tmp_adr->personal[0])
	  folder_name = (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf,
						       SIZEOF_20KBUF, tmp_adr->personal);

	if(folder_name && folder_name[0]){
	    istrncpy(fbuf, folder_name, nfbuf - 1);
	    fbuf[nfbuf - 1] = '\0';
	}
	else{	/* fall back to other behaviors */
	    switch(save_msg_rule){
	      case SAV_RULE_RN_FROM:
	        save_msg_rule = SAV_RULE_FROM;
		break;

	      case SAV_RULE_RN_SENDER:
	        save_msg_rule = SAV_RULE_SENDER;
		break;

	      case SAV_RULE_RN_RECIP:
	        save_msg_rule = SAV_RULE_RECIP;
		break;

	      case SAV_RULE_RN_REPLYTO:
	        save_msg_rule = SAV_RULE_REPLYTO;
		break;

	      default:
		istrncpy(fbuf, ps_global->VAR_DEFAULT_SAVE_FOLDER, nfbuf - 1);
		fbuf[nfbuf - 1] = '\0';
		break;
	    }
	}

	break;
    }

    /* get the username out of the mailbox for this address */
    switch(save_msg_rule){
      case SAV_RULE_FROM:
      case SAV_RULE_SENDER:
      case SAV_RULE_REPLYTO:
      case SAV_RULE_RECIP:
	/*
	 * Fish out the user's name from the mailbox portion of
	 * the address and put it in folder.
	 */
	folder_name = (tmp_adr && tmp_adr->mailbox && tmp_adr->mailbox[0])
		      ? tmp_adr->mailbox : NULL;
	if(!get_uname(folder_name, fbuf, nfbuf)){
	    istrncpy(fbuf, ps_global->VAR_DEFAULT_SAVE_FOLDER, nfbuf - 1);
	    fbuf[nfbuf - 1] = '\0';
	}

	break;
    }

    if(tmp_adr)
      mail_free_address(&tmp_adr);
}


/*----------------------------------------------------------------------
        Do the work of actually saving messages to a folder

    Args: state -- pine state struct (for stream pointers)
	  stream  -- source stream, which msgmap refers to
	  context -- context to interpret name in if not fully qualified
	  folder  -- The folder to save the message in
          msgmap -- message map of currently selected messages
	  flgs -- Possible bits are
		    SV_DELETE   - delete after saving
		    SV_FOR_FILT - called from filtering function, not save
		    SV_FIX_DELS - remove Del mark before saving
		    SV_INBOXWOCNTXT - "inbox" is interpreted without context making
				      it the one-true inbox instead

  Result: Returns number of messages saved

  Note: There's a bit going on here; temporary clearing of deleted flags
	since they are *not* preserved, picking or creating the stream for
	copy or append, and dealing with errors...
	We try to preserve user keywords by setting them in the destination.
 ----*/
long
save(struct pine *state, MAILSTREAM *stream, CONTEXT_S *context, char *folder,
     MSGNO_S *msgmap, int flgs)
{
    int		  rv, rc, j, our_stream = 0, cancelled = 0;
    int           delete, filter, k, worry_about_keywords = 0;
    char	 *save_folder, *seq, *flags = NULL, date[64], tmp[MAILTMPLEN];
    long	  i, nmsgs, rawno;
    size_t        len;
    STORE_S	 *so = NULL;
    MAILSTREAM	 *save_stream = NULL;
    MESSAGECACHE *mc;

    delete = flgs & SV_DELETE;
    filter = flgs & SV_FOR_FILT;

    if(strucmp(folder, state->inbox_name) == 0 && flgs & SV_INBOXWOCNTXT){
	save_folder = state->VAR_INBOX_PATH;
	context = NULL;
    }
    else
      save_folder = folder;

    /*
     * Because the COPY/APPEND command doesn't always create keywords when they
     * aren't already defined in a mailbox, we need to ensure that the keywords
     * exist in the destination (are defined and settable) before we do the copies.
     * Here's what the code is doing
     *
     * If we have keywords set in the source messages
     *   Add a dummy message to destination mailbox
     *
     *   for each keyword that is set in the set of messages we're saving
     *     set the keyword in that message (thus creating it)
     *
     *   remember deleted messages
     *   undelete them
     *   delete dummy message
     *   expunge
     *   delete remembered messages
     *
     * After that the assumption is that the keywords will be saved by a
     * COPY command. We need to set the flags string ourself for appends.
     */

    /* are any keywords set in the source messages? */
    for(i = mn_first_cur(msgmap); !worry_about_keywords && i > 0L; i = mn_next_cur(msgmap)){
	rawno = mn_m2raw(msgmap, i);
	mc = (rawno > 0L && stream && rawno <= stream->nmsgs)
		? mail_elt(stream, rawno) : NULL;
	if(mc && mc->user_flags)
	  worry_about_keywords++;
    }

    if(worry_about_keywords){
	MAILSTREAM *dstn_stream = NULL;
	int         already_open = 0;
	int         we_blocked_reuse = 0;

	/*
	 * Possible problem created by our stream re-use
	 * strategy. If we are going to open a new stream
	 * here, we want to be sure not to re-use the
	 * stream we are saving _from_, so take it out of the
	 * re-use pool before we call open.
	 */
	if(sp_flagged(stream, SP_USEPOOL)){
	    we_blocked_reuse++;
	    sp_unflag(stream, SP_USEPOOL);
	}

	/* see if there is a stream open already */
	if(!dstn_stream){
	    dstn_stream = context_already_open_stream(context,
						      save_folder,
						      AOS_RW_ONLY);
	    already_open = dstn_stream ? 1 : 0;
	}

	if(!dstn_stream)
	  dstn_stream = context_open(context, NULL,
				     save_folder,
				     SP_USEPOOL | SP_TEMPUSE,
				     NULL);

	if(dstn_stream && dstn_stream->kwd_create){
	    imapuid_t dummy_uid = 0L;
	    long dummy_msgno, delete_count;
	    int expbits, set;
	    char *flags = NULL;
	    char *user_flag_name, *duser_flag_name;

	    /* find keywords that need to be defined */
	    for(k = 0; stream_to_user_flag_name(stream, k); k++){
	      user_flag_name = stream_to_user_flag_name(stream, k);
	      if(user_flag_name && user_flag_name[0]){
		  /* is this flag set in any of the save set? */
		  for(set = 0, i = mn_first_cur(msgmap);
		      !set && i > 0L;
		      i = mn_next_cur(msgmap)){
		      rawno = mn_m2raw(msgmap, i);
		      if(user_flag_is_set(stream, rawno, user_flag_name))
			set++;
		  }

		  if(set){
		      /*
		       * The flag may already be defined in this
		       * mailbox. Check for that first.
		       */
		      for(j = 0; stream_to_user_flag_name(dstn_stream, j); j++){
			duser_flag_name = stream_to_user_flag_name(dstn_stream, j);
		        if(duser_flag_name && duser_flag_name[0]
			   && !strucmp(duser_flag_name, user_flag_name)){
			    set = 0;
			    break;
			}
		      }
		  }

		  if(set){
		      if(flags == NULL){
		          len = strlen(user_flag_name) + 1;
		          flags = (char *) fs_get((len+1) * sizeof(char));
			  snprintf(flags, len+1, "%s ", user_flag_name);
		      }
		      else{
			  char *p;
			  size_t newlen;

		          newlen = strlen(user_flag_name) + 1;
		          len += newlen;
			  fs_resize((void **) &flags, (len+1) * sizeof(char));
			  p = flags + strlen(flags);
			  snprintf(p, newlen+1, "%s ", user_flag_name);
		      }
		  }
	      }
	    }

	    if(flags){
		char *p;
		size_t newlen;
		STRING msg;
		char dummymsg[1000];
		char *id = NULL;
		char *idused;
		appenduid_t *au;

		newlen = strlen("\\DELETED");
		len += newlen;
		fs_resize((void **) &flags, (len+1) * sizeof(char));
		p = flags + strlen(flags);
		snprintf(p, newlen+1, "%s", "\\DELETED");

		id = generate_message_id();
		idused = id ? id : "<xyz>";
		snprintf(dummymsg, sizeof(dummymsg), "Date: Thu, 18 May 2006 00:00 -0700\r\nFrom: dummy@example.com\r\nSubject: dummy\r\nMessage-ID: %s\r\n\r\ndummy\r\n", idused);

		/*
		 * We need to get the uid of the message we are about to
		 * append so that we can delete it when we're done and
		 * so we don't affect other messages.
		 */

		if(is_imap_stream(dstn_stream) && LEVELUIDPLUS (dstn_stream)){
		    au = mail_parameters(NIL, GET_APPENDUID, NIL);
		    mail_parameters(NIL, SET_APPENDUID, (void *) appenduid_cb);
		}

		INIT(&msg, mail_string, (void *) dummymsg, strlen(dummymsg));
		if(pine_mail_append(dstn_stream, dstn_stream->mailbox, &msg)){

		    (void) pine_mail_ping(dstn_stream);

		    if(is_imap_stream(dstn_stream) && LEVELUIDPLUS (dstn_stream))
		      dummy_uid = get_last_append_uid();

		    if(dummy_uid == 0L){
			dummy_msgno = get_msgno_by_msg_id(dstn_stream, idused,
							  sp_msgmap(dstn_stream));
			if(dummy_msgno <= 0L || dummy_msgno > dstn_stream->nmsgs)
			  dummy_msgno = dstn_stream->nmsgs;

			rawno = mn_m2raw(sp_msgmap(dstn_stream), dummy_msgno);
			if(rawno > 0L && rawno <= dstn_stream->nmsgs)
			  dummy_uid = mail_uid(dstn_stream, rawno);

			if(dummy_uid == 0L)
			  dummy_msgno = dstn_stream->nmsgs;
		    }

		    /*
		     * We need to remember which messages are deleted,
		     * undelete them, do the expunge, then delete them again.
		     */
		    delete_count = count_flagged(dstn_stream, F_DEL);
		    if(delete_count){
			for(i = 1L; i <= dstn_stream->nmsgs; i++)
			  if(((mc = mail_elt(dstn_stream, i)) && mc->valid && mc->deleted)
				 || (mc && !mc->valid && mc->searched)){
			      mc->sequence = 1;
			      expbits      = MSG_EX_DELETE;
			      msgno_exceptions(dstn_stream, i, "0", &expbits, TRUE);
			  }
			  else if((mc = mail_elt(dstn_stream, i)) != NULL)
			    mc->sequence = 0;

			if((seq = build_sequence(dstn_stream, NULL, NULL)) != NULL){
			    mail_flag(dstn_stream, seq, "\\DELETED", ST_SILENT);
			    fs_give((void **) &seq);
			}
		    }

		    if(dummy_uid > 0L)
		      mail_flag(dstn_stream, ulong2string(dummy_uid),
			        flags, ST_SET | ST_UID | ST_SILENT);
		    else
		      mail_flag(dstn_stream, ulong2string(dummy_msgno),
			        flags, ST_SET | ST_SILENT);

		    ps_global->mm_log_error = 0;
		    ps_global->expunge_in_progress = 1;
		    mail_expunge(dstn_stream);
		    ps_global->expunge_in_progress = 0;

		    if(delete_count){
			for(i = 1L; i <= dstn_stream->nmsgs; i++)
			  if((mc = mail_elt(dstn_stream, i)) != NULL){
			    mc->sequence 
			       = (msgno_exceptions(dstn_stream, i, "0", &expbits, FALSE)
				  && (expbits & MSG_EX_DELETE));

			    /*
			     * Remove the EX_DELETE bit in case we're still using
			     * this stream.
			     */
			    if(mc->sequence){
			      expbits      &= ~MSG_EX_DELETE;
			      msgno_exceptions(dstn_stream, i, "0", &expbits, TRUE);
			    }
			  }

			if((seq = build_sequence(dstn_stream, NULL, NULL)) != NULL){
			    mail_flag(dstn_stream, seq, "\\DELETED", ST_SET | ST_SILENT);
			    fs_give((void **) &seq);
			}
		    }
		}

		if(is_imap_stream(dstn_stream) && LEVELUIDPLUS (dstn_stream))
		  mail_parameters(NIL, SET_APPENDUID, (void *) au);

		if(id)
		  fs_give((void **) &id);

		fs_give((void **) &flags);
	    }
	}

	if(dstn_stream && !already_open)
	  pine_mail_close(dstn_stream);

	if(we_blocked_reuse)
	  sp_set_flags(stream, sp_flags(stream) | SP_USEPOOL);
    }

    /*
     * If any of the messages have exceptional attachment handling
     * we have to fall thru below to do the APPEND by hand...
     */
    if(!msgno_any_deletedparts(stream, msgmap)){
	int loc_to_loc = 0;

	/*
	 * Compare the current stream (the save's source) and the stream
	 * the destination folder will need...
	 */
	context_apply(tmp, context, save_folder, sizeof(tmp));

	/*
	 * If we're going to be doing a cross-format copy we're better off
	 * using the else code below that knows how to do multi-append.
	 * The part in the if is leaving save_stream set to NULL in the
	 * case that the stream is local and the folder is local and they
	 * are different formats (like unix and tenex). That will cause us
	 * to fall thru to the APPEND case which is faster than using
	 * copy which will use our imap_proxycopy which doesn't know
	 * about multiappend.
	 */
	loc_to_loc = stream && stream->dtb
			&& stream->dtb->flags & DR_LOCAL && !IS_REMOTE(tmp);
	if(!loc_to_loc || (stream->dtb->valid && (*stream->dtb->valid)(tmp)))
          save_stream = loc_to_loc ? stream
				   : context_same_stream(context, save_folder, stream);
    }

    /* if needed, this'll get set in mm_notify */
    ps_global->try_to_create = 0;
    rv = rc = 0;
    nmsgs = 0L;

    /*
     * At this point, if we have a save_stream, then none of the messages
     * being saved involve special handling that would require our use
     * of mail_append, so go with mail_copy since in the IMAP case it
     * means no data on the wire...
     */
    if(save_stream){
	char *dseq = NULL, *oseq;

	if((flgs & SV_FIX_DELS) &&
	   (dseq = currentf_sequence(stream, msgmap, F_DEL, NULL,
				     0, NULL, NULL)))
	  mail_flag(stream, dseq, "\\DELETED", 0L);

	seq = currentf_sequence(stream, msgmap, 0L, &nmsgs, 0, NULL, NULL);
	if(!(flgs & SV_PRESERVE)
	   && (F_ON(F_AGG_SEQ_COPY, ps_global)
	       || (mn_get_sort(msgmap) == SortArrival && !mn_get_revsort(msgmap)))){
	    
	    /*
	     * currentf_sequence() above lit all the elt "sequence"
	     * bits of the interesting messages.  Now, build a sequence
	     * that preserves sort order...
	     */
	    oseq = build_sequence(stream, msgmap, &nmsgs);
	}
	else{
	    oseq  = NULL;			/* no single sequence! */
	    nmsgs = 0L;
	    i = mn_first_cur(msgmap);		/* set first to copy */
	}

	do{
	    while(!(rv = (int) context_copy(context, save_stream,
				oseq ? oseq : long2string(mn_m2raw(msgmap, i)),
				save_folder))){
		if(rc++ || !ps_global->try_to_create)   /* abysmal failure! */
		  break;			/* c-client returned error? */

		if((context && context->use & CNTXT_INCMNG)
		   && context_isambig(save_folder)){
		    q_status_message(SM_ORDER, 3, 5,
		   _("Can only save to existing folders in Incoming Collection"));
		    break;
		}

		ps_global->try_to_create = 0;	/* reset for next time */
		if((j = create_for_save(context, save_folder)) < 1){
		    if(j < 0)
		      cancelled = 1;		/* user cancels */

		    break;
		}
	    }

	    if(rv){				/* failure or finished? */
		if(oseq)			/* all done? */
		  break;
		else
		  nmsgs++;
	    }
	    else{				/* failure! */
		if(oseq)
		  nmsgs = 0L;			/* nothing copy'd */

		break;
	    }
	}
	while((i = mn_next_cur(msgmap)) > 0L);

	if(rv && delete)			/* delete those saved */
	  mail_flag(stream, seq, "\\DELETED", ST_SET);
	else if(dseq)				/* or restore previous state */
	  mail_flag(stream, dseq, "\\DELETED", ST_SET);

	if(dseq)				/* clean up */
	  fs_give((void **)&dseq);

	if(oseq)
	  fs_give((void **)&oseq);

	fs_give((void **)&seq);
    }
    else{
	/*
	 * Special handling requires mail_append.  See if we can
	 * re-use stream source messages are on...
	 */
	save_stream = context_same_stream(context, save_folder, stream);

	/*
	 * IF the destination's REMOTE, open a stream here so c-client
	 * doesn't have to open it for each aggregate save...
	 */
	if(!save_stream){
	    if(context_apply(tmp, context, save_folder, sizeof(tmp))[0] == '{'
	       && (save_stream = context_open(context, NULL,
					      save_folder,
				      OP_HALFOPEN | SP_USEPOOL | SP_TEMPUSE,
					      NULL)))
	      our_stream = 1;
	}

	/*
	 * Allocate a storage object to temporarily store the message
	 * object in.  Below it'll get mapped into a c-client STRING struct
	 * in preparation for handing off to context_append...
	 */
	if(!(so = so_get(CharStar, NULL, WRITE_ACCESS))){
	    dprint((1, "Can't allocate store for save: %s\n",
		       error_description(errno)));
	    q_status_message(SM_ORDER | SM_DING, 3, 4,
			     "Problem creating space for message text.");
	}

	/*
	 * get a sequence of invalid elt's so we can get their flags...
	 */
	if((seq = invalid_elt_sequence(stream, msgmap)) != NULL){
	    mail_fetch_fast(stream, seq, 0L);
	    fs_give((void **) &seq);
	}

	/*
	 * If we're supposed set the deleted flag, clear the elt bit
	 * we'll use to build the sequence later...
	 */
	if(delete)
	  for(i = 1L; i <= stream->nmsgs; i++)
	    if((mc = mail_elt(stream, i)) != NULL)
	      mc->sequence = 0;

	nmsgs = 0L;

	if(pith_opt_save_size_changed_prompt)
	  (*pith_opt_save_size_changed_prompt)(0L, SSCP_INIT);

	/* 
	 * if there is more than one message, do multiappend.
	 * otherwise, we can use our already open stream.
	 */
	if(!save_stream || !is_imap_stream(save_stream) ||
	   (LEVELMULTIAPPEND(save_stream) && mn_total_cur(msgmap) > 1)){
	    APPENDPACKAGE pkg;
	    STRING msg;

	    pkg.stream = stream;
	    /* tell save_fetch_append_cb whether or not to leave deleted flag */
	    pkg.flags = flgs & SV_FIX_DELS ? NULL : cpystr("\\DELETED");
	    pkg.date = date;
	    pkg.msg = &msg;
	    pkg.msgmap = msgmap;

	    if ((pkg.so = so) && ((pkg.msgno = mn_first_cur(msgmap)) > 0L)) {
	        so_truncate(so, 0L);

		/* 
		 * we've gotta make sure this is a stream that we've
		 * opened ourselves.
		 */
		rc = 0;
		while(!(rv = context_append_multiple(context, 
						     our_stream ? save_stream
						     : NULL, save_folder,
						     save_fetch_append_cb,
						     &pkg,
						     stream))) {

		  if(rc++ || !ps_global->try_to_create)
		    break;
		  if((context && context->use & CNTXT_INCMNG)
		     && context_isambig(save_folder)){
		    q_status_message(SM_ORDER, 3, 5,
		   _("Can only save to existing folders in Incoming Collection"));
		    break;
		  }

		  ps_global->try_to_create = 0;
		  if((j = create_for_save(context, save_folder)) < 1){
		    if(j < 0)
		      cancelled = 1;
		    break;
		  }
		}

		if(pkg.flags)
		  fs_give((void **) &pkg.flags);

		ps_global->noshow_error = 0;

		if(rv){
		    /*
		     * Success!  Count it, and if it's not already deleted and 
		     * it's supposed to be, mark it to get deleted later...
		     */
		  for(i = mn_first_cur(msgmap); so && i > 0L;
		      i = mn_next_cur(msgmap)){
		    nmsgs++;
		    if(delete){
		      rawno = mn_m2raw(msgmap, i);
		      mc = (rawno > 0L && stream && rawno <= stream->nmsgs)
			    ? mail_elt(stream, rawno) : NULL;
		      if(mc && !mc->deleted)
			mc->sequence = 1;	/* mark for later deletion */
		    }
		  }
		}
	    }
	    else
	      cancelled = 1;		/* No messages to append! */

	    if(sp_expunge_count(stream))
	      cancelled = 1;		/* All bets are off! */
	}
	else 
	  for(i = mn_first_cur(msgmap); so && i > 0L; i = mn_next_cur(msgmap)){
	    int preserve_these_flags;

	    so_truncate(so, 0L);

	    rawno = mn_m2raw(msgmap, i);
	    mc = (rawno > 0L && stream && rawno <= stream->nmsgs)
		    ? mail_elt(stream, rawno) : NULL;

	    /* always preserve these flags */
	    preserve_these_flags = F_ANS|F_FWD|F_FLAG|F_SEEN|F_KEYWORD;
	    /* maybe preserve deleted flag */
	    preserve_these_flags |= flgs & SV_FIX_DELS ? 0 : F_DEL;
	    flags = flag_string(stream, rawno, preserve_these_flags);
	      
	    if(mc && mc->day)
	      mail_date(date, mc);
	    else
	      *date = '\0';

	    rv = save_fetch_append(stream, mn_m2raw(msgmap, i),
				   NULL, save_stream, save_folder, context,
				   mc ? mc->rfc822_size : 0L, flags, date, so);

	    if(flags)
	      fs_give((void **) &flags);

	    if(sp_expunge_count(stream))
	      rv = -1;			/* All bets are off! */

	    if(rv == 1){
		/*
		 * Success!  Count it, and if it's not already deleted and 
		 * it's supposed to be, mark it to get deleted later...
		 */
		nmsgs++;
		if(delete){
		    rawno = mn_m2raw(msgmap, i);
		    mc = (rawno > 0L && stream && rawno <= stream->nmsgs)
			    ? mail_elt(stream, rawno) : NULL;
		    if(mc && !mc->deleted)
		      mc->sequence = 1;		/* mark for later deletion */
		}
	    }
	    else{
		if(rv == -1)
		  cancelled = 1;		/* else horrendous failure */

		break;
	    }
	}

	if(pith_opt_save_size_changed_prompt)
	  (*pith_opt_save_size_changed_prompt)(0L, SSCP_END);

	if(our_stream)
	  pine_mail_close(save_stream);

	if(so)
	  so_give(&so);

	if(delete && (seq = build_sequence(stream, NULL, NULL))){
	    mail_flag(stream, seq, "\\DELETED", ST_SET);
	    fs_give((void **)&seq);
	}
    }

    ps_global->try_to_create = 0;		/* reset for next time */
    if(!cancelled && nmsgs < mn_total_cur(msgmap)){
	dprint((1, "FAILED save of msg %s (c-client sequence #)\n",
		   long2string(mn_m2raw(msgmap, mn_get_cur(msgmap)))));
	if((mn_total_cur(msgmap) > 1L) && nmsgs != 0){
	  /* this shouldn't happen cause it should be all or nothing */
	    snprintf(tmp_20k_buf, SIZEOF_20KBUF,
		    "%ld of %ld messages saved before error occurred",
		    nmsgs, mn_total_cur(msgmap));
	    dprint((1, "\t%s\n", tmp_20k_buf));
	    q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
	else if(mn_total_cur(msgmap) == 1){
	  snprintf(tmp_20k_buf, SIZEOF_20KBUF,
		  "%s to folder \"%s\" FAILED",
		  filter ? "Filter" : "Save", 
		  strsquish(tmp_20k_buf+500, 500, folder, 35));
	  dprint((1, "\t%s\n", tmp_20k_buf));
	  q_status_message(SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
	}
	else{
	  snprintf(tmp_20k_buf, SIZEOF_20KBUF,
		  "%s of %s messages to folder \"%s\" FAILED",
		  filter ? "Filter" : "Save", comatose(mn_total_cur(msgmap)),
		  strsquish(tmp_20k_buf+500, 500, folder, 35));
	  dprint((1, "\t%s\n", tmp_20k_buf));
	  q_status_message(SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
	}
    }

    return(nmsgs);
}


/* Append message callback
 * Accepts: MAIL stream
 *	    append data package
 *	    pointer to return initial flags
 *	    pointer to return message internal date
 *	    pointer to return stringstruct of message or NIL to stop
 * Returns: T if success (have message or stop), NIL if error
 */

long save_fetch_append_cb(MAILSTREAM *stream, void *data, char **flags,
			  char **date, STRING **message)
{
    unsigned long size = 0;
    APPENDPACKAGE *pkg = (APPENDPACKAGE *) data;
    MESSAGECACHE *mc;
    char *fetch;
    int rc;
    unsigned long raw, hlen, tlen, mlen;

    if(pkg->so && (pkg->msgno > 0L)) {
	raw = mn_m2raw(pkg->msgmap, pkg->msgno);
	mc = (raw > 0L && pkg->stream && raw <= pkg->stream->nmsgs)
		? mail_elt(pkg->stream, raw) : NULL;
	if(mc){
	    int preserve_these_flags = F_ANS|F_FWD|F_FLAG|F_SEEN|F_KEYWORD;

	    size = mc->rfc822_size;

	    /*
	     * If the caller wants us to preserve the state of the
	     * \DELETED flag then pkg->flags will be \DELETED, otherwise
	     * let it be undeleted. Fix from Eduardo Chappa.
	     */
	    if(pkg->flags){
		if(strstr(pkg->flags,"\\DELETED"))
		  preserve_these_flags |= F_DEL;

		/* not used anymore */
		fs_give((void **) &pkg->flags);
	    }

	    pkg->flags = flag_string(pkg->stream, raw, preserve_these_flags);
	}

	if(mc && mc->day)
	  mail_date(pkg->date, mc);
	else
	  *pkg->date = '\0';
	if((fetch = mail_fetch_header(pkg->stream, raw, NULL, NULL, &hlen,
				     FT_PEEK)) != NULL){
	    if(!*pkg->date)
	      saved_date(pkg->date, fetch);
	}
	else
	  return(0);			/* fetchtext writes error */

	rc = MSG_EX_DELETE;		/* "rc" overloaded */
	if(msgno_exceptions(pkg->stream, raw, NULL, &rc, 0)){
	    char  section[64];
	    int	 failure = 0;
	    BODY *body;
	    gf_io_t  pc;

	    size = 0;			/* all bets off, abort sanity test  */
	    gf_set_so_writec(&pc, pkg->so);

	    section[0] = '\0';
	    if(!pine_mail_fetch_structure(pkg->stream, raw, &body, 0))
	      return(0);
	    
	    if(msgno_part_deleted(pkg->stream, raw, "")){
	       tlen = 0;
	       failure = !save_ex_replace_body(fetch, &hlen, body, pc);
	     }
	    else
	      failure = !(so_nputs(pkg->so, fetch, (long) hlen)
			  && save_ex_output_body(pkg->stream, raw, section,
						 body, &tlen, pc));

	    gf_clear_so_writec(pkg->so);

	    if(failure)
	      return(0);

	    q_status_message(SM_ORDER, 3, 3,
			     /* TRANSLATORS: A warning to user that the message parts
			        they deleted will not be included in the copy they
				are now saving to. */
			     _("NOTE: Deleted message parts NOT included in saved copy!"));

	}
	else{
	    if(!so_nputs(pkg->so, fetch, (long) hlen))
	      return(0);

	    fetch = pine_mail_fetch_text(pkg->stream, raw, NULL, &tlen, FT_PEEK);

	    if(!(fetch && so_nputs(pkg->so, fetch, tlen)))
	      return(0);
	}

	so_seek(pkg->so, 0L, 0);	/* rewind just in case */
	mlen = hlen + tlen;

	if(size && mlen < size){
	    char buf[128];

	    if(sp_dead_stream(pkg->stream)){
		snprintf(buf, sizeof(buf), _("Cannot save because current folder is Closed"));
		q_status_message(SM_ORDER | SM_DING, 3, 4, buf);
		dprint((1, "save_fetch_append_cb: %s\n", buf));
		return(0);
	    }
	    else{
		if(pith_opt_save_size_changed_prompt
		   && (*pith_opt_save_size_changed_prompt)(mn_raw2m(pkg->msgmap, raw), 0) == 'y'){
		    snprintf(buf, sizeof(buf),
			     "Message to save shrank! (raw msg# %ld: %ld -> %ld): User says continue",
			     raw, size, mlen);
		    dprint((1, "save_fetch_append_cb: %s", buf));
		    snprintf(buf, sizeof(buf),
	 "Message to save shrank: source msg # %ld may be saved incorrectly",
			     mn_raw2m(pkg->msgmap, raw));
		    q_status_message(SM_ORDER, 0, 3, buf);
		}
		else{
		    snprintf(buf, sizeof(buf),
	 "Message to save shrank: raw msg # %ld went from announced size %ld to actual size %ld",
			     raw, size, mlen);
		    dprint((1, "save_fetch_append_cb: %s", buf));
		    return(0);
		}
	    }
	}

	INIT(pkg->msg, mail_string, (void *)so_text(pkg->so), mlen);
      *message = pkg->msg;
					/* Next message */
      pkg->msgno = mn_next_cur(pkg->msgmap);
  }
  else					/* No more messages */
    *message = NIL;

  *flags = pkg->flags;
  *date = (pkg->date && *pkg->date) ? pkg->date : NIL;
  return LONGT;				/* Return success */
}


/*----------------------------------------------------------------------
   FETCH an rfc822 message header and body and APPEND to destination folder

  Args: 
        

 Result: 

 ----*/
int
save_fetch_append(MAILSTREAM *stream, long int raw, char *sect,
		  MAILSTREAM *save_stream, char *save_folder, CONTEXT_S *context,
		  long unsigned int size, char *flags, char *date, STORE_S *so)
{
    int		   rc, rv, old_imap_server = 0;
    long	   j;
    char	  *fetch;
    unsigned long  hlen, tlen, mlen;
    STRING	   msg;

    if((fetch = mail_fetch_header(stream, raw, sect, NULL, &hlen, FT_PEEK)) != NULL){
	/*
	 * If there's no date string, then caller found the
	 * MESSAGECACHE for this message element didn't already have it.
	 * So, parse the "internal date" by hand since fetchstructure
	 * hasn't been called yet for this particular message, and
	 * we don't want to call it now just to get the date since
	 * the full header has what we want.  Likewise, don't even
	 * think about calling mail_fetchfast either since it also
	 * wants to load mc->rfc822_size (which we could actually
	 * use but...), which under some drivers is *very*
	 * expensive to acquire (can you say NNTP?)...
	 */
	if(!*date)
	  saved_date(date, fetch);
    }
    else
      return(0);			/* fetchtext writes error */

    rc = MSG_EX_DELETE;			/* "rc" overloaded */
    if(msgno_exceptions(stream, raw, NULL, &rc, 0)){
	char	 section[64];
	int	 failure = 0;
	BODY	*body;
	gf_io_t  pc;

	size = 0;			/* all bets off, abort sanity test  */
	gf_set_so_writec(&pc, so);

	if(sect && *sect){
	    snprintf(section, sizeof(section), "%s.1", sect);
	    if(!(body = mail_body(stream, raw, (unsigned char *) section)))
	      return(0);
	}
	else{
	    section[0] = '\0';
	    if(!pine_mail_fetch_structure(stream, raw, &body, 0))
	      return(0);
	}

	/*
	 * Walk the MIME structure looking for exceptional segments,
	 * writing them in the requested fashion.
	 *
	 * First, though, check for the easy case...
	 */
	if(msgno_part_deleted(stream, raw, sect ? sect : "")){
	    tlen = 0;
	    failure = !save_ex_replace_body(fetch, &hlen, body, pc);
	}
	else{
	    /*
	     * Otherwise, roll up your sleeves and get to work...
	     * start by writing msg header and then the processed body
	     */
	    failure = !(so_nputs(so, fetch, (long) hlen)
			&& save_ex_output_body(stream, raw, section,
					       body, &tlen, pc));
	}

	gf_clear_so_writec(so);

	if(failure)
	  return(0);

	q_status_message(SM_ORDER, 3, 3,
		    _("NOTE: Deleted message parts NOT included in saved copy!"));

    }
    else{
	/* First, write the header we just fetched... */
	if(!so_nputs(so, fetch, (long) hlen))
	  return(0);

	old_imap_server = is_imap_stream(stream) && !modern_imap_stream(stream);

	/* Second, go fetch the corresponding text... */
	fetch = pine_mail_fetch_text(stream, raw, sect, &tlen,
				     !old_imap_server ? FT_PEEK : 0);

	/*
	 * Special handling in case we're fetching a Message/rfc822
	 * segment and we're talking to an old server...
	 */
	if(fetch && *fetch == '\0' && sect && (hlen + tlen) != size){
	    so_seek(so, 0L, 0);
	    fetch = pine_mail_fetch_body(stream, raw, sect, &tlen, 0L);
	}

	/*
	 * Pre IMAP4 servers can't do a non-peeking mail_fetch_text,
	 * so if the message we are saving from was originally unseen,
	 * we have to change it back to unseen. Flags contains the
	 * string "SEEN" if the original message was seen.
	 */
	if(old_imap_server && (!flags || !srchstr(flags,"SEEN"))){
	    char seq[20];

	    strncpy(seq, long2string(raw), sizeof(seq));
	    seq[sizeof(seq)-1] = '\0';
	    mail_flag(stream, seq, "\\SEEN", 0);
	}

	/* If fetch succeeded, write the result */
	if(!(fetch && so_nputs(so, fetch, tlen)))
	   return(0);
    }

    so_seek(so, 0L, 0);			/* rewind just in case */

    /*
     * Set up a c-client string driver so we can hand the
     * collected text down to mail_append.
     *
     * NOTE: We only test the size if and only if we already
     *	     have it.  See, in some drivers, especially under
     *	     dos, its too expensive to get the size (full
     *	     header and body text fetch plus MIME parse), so
     *	     we only verify the size if we already know it.
     */
    mlen = hlen + tlen;

    if(size && mlen < size){
	char buf[128];

	if(sp_dead_stream(stream)){
	    snprintf(buf, sizeof(buf), _("Cannot save because current folder is Closed"));
	    q_status_message(SM_ORDER | SM_DING, 3, 4, buf);
	    dprint((1, "save_fetch_append: %s", buf));
	    return(0);
	}
	else{
	    if(pith_opt_save_size_changed_prompt
	       && (*pith_opt_save_size_changed_prompt)(mn_raw2m(sp_msgmap(stream), raw), 0) == 'y'){
		snprintf(buf, sizeof(buf),
			 "Message to save shrank! (raw msg# %ld: %ld -> %ld): User says continue",
			 raw, size, mlen);
		dprint((1, "save_fetch_append: %s", buf));
		snprintf(buf, sizeof(buf),
     "Message to save shrank: source msg # %ld may be saved incorrectly",
			 mn_raw2m(sp_msgmap(stream), raw));
		q_status_message(SM_ORDER, 0, 3, buf);
	    }
	    else{
		snprintf(buf, sizeof(buf),
     "Message to save shrank: source raw msg # %ld went from announced size %ld to actual size %ld",
			 raw, size, mlen);
		dprint((1, "save_fetch_append: %s", buf));
		return(0);
	    }
	}
    }

    INIT(&msg, mail_string, (void *)so_text(so), mlen);

    rc = 0;
    while(!(rv = (int) context_append_full(context, save_stream,
					   save_folder, flags,
					   *date ? date : NULL,
					   &msg))){
	if(rc++ || !ps_global->try_to_create)	/* abysmal failure! */
	  break;				/* c-client returned error? */

	if(context && (context->use & CNTXT_INCMNG)
	   && context_isambig(save_folder)){
	    q_status_message(SM_ORDER, 3, 5,
	       _("Can only save to existing folders in Incoming Collection"));
	    break;
	}

	ps_global->try_to_create = 0;		/* reset for next time */
	if((j = create_for_save(context, save_folder)) < 1){
	    if(j < 0)
	      rv = -1;			/* user cancelled */

	    break;
	}

	SETPOS((&msg), 0L);			/* reset string driver */
    }

    return(rv);
}


/*
 * save_ex_replace_body -
 *
 * NOTE : hlen points to a cell that has the byte count of "hdr" on entry
 *	  *BUT* which is to contain the count of written bytes on exit
 */
int
save_ex_replace_body(char *hdr, long unsigned int *hlen, struct mail_bodystruct *body, gf_io_t pc)
{
    unsigned long len;

    /*
     * "X-" out the given MIME headers unless they're
     * the same as we're going to substitute...
     */
    if(body->type == TYPETEXT
       && (!body->subtype || !strucmp(body->subtype, "plain"))
       && body->encoding == ENC7BIT){
	if(!gf_nputs(hdr, *hlen, pc))	/* write out header */
	  return(0);
    }
    else{
	int bol = 1;

	/*
	 * write header, "X-"ing out transport headers bothersome to
	 * software but potentially useful to the human recipient...
	 */
	for(len = *hlen; len; len--){
	    if(bol){
		unsigned long n;

		bol = 0;
		if(save_ex_mask_types(hdr, &n, pc))
		  *hlen += n;		/* add what we inserted */
		else
		  break;
	    }

	    if(*hdr == '\015' && *(hdr+1) == '\012'){
		bol++;			/* remember beginning of line */
		len--;			/* account for LF */
		if(gf_nputs(hdr, 2, pc))
		  hdr += 2;
		else
		  break;
	    }
	    else if(!(*pc)(*hdr++))
	      break;
	}

	if(len)				/* bytes remain! */
	  return(0);
    }

    /* Now, blat out explanatory text as the body... */
    if(save_ex_explain_body(body, &len, pc)){
	*hlen += len;
	return(1);
    }
    else
      return(0);
}


int
save_ex_output_body(MAILSTREAM *stream, long int raw, char *section,
		    struct mail_bodystruct *body, long unsigned int *len, gf_io_t pc)
{
    char	  *txtp, newsect[128];
    unsigned long  ilen;

    txtp = mail_fetch_mime(stream, raw, section, len, FT_PEEK);

    if(msgno_part_deleted(stream, raw, section))
      return(save_ex_replace_body(txtp, len, body, pc));

    if(body->type == TYPEMULTIPART){
	char	  *subsect, boundary[128];
	int	   n, blen;
	PART	  *part = body->nested.part;
	PARAMETER *param;

	/* Locate supplied multipart boundary */
	for (param = body->parameter; param; param = param->next)
	  if (!strucmp(param->attribute, "boundary")){
	      snprintf(boundary, sizeof(boundary), "--%.*s\015\012", sizeof(boundary)-10,
		      param->value);
	      blen = strlen(boundary);
	      break;
	  }

	if(!param){
	    q_status_message(SM_ORDER|SM_DING, 3, 3, "Missing MIME boundary");
	    return(0);
	}

	/*
	 * BUG: if multi/digest and a message deleted (which we'll
	 * change to text/plain), we need to coerce this composite
	 * type to multi/mixed !!
	 */
	if(!gf_nputs(txtp, *len, pc))		/* write MIME header */
	  return(0);

	/* Prepare to specify sub-sections */
	strncpy(newsect, section, sizeof(newsect));
	newsect[sizeof(newsect)-1] = '\0';
	subsect = &newsect[n = strlen(newsect)];
	if(n > 2 && !strcmp(&newsect[n-2], ".0"))
	  subsect--;
	else if(n){
	  if((subsect-newsect) < sizeof(newsect))
	    *subsect++ = '.';
	}

	n = 1;
	do {				/* for each part */
	    strncpy(subsect, int2string(n++), sizeof(newsect)-(subsect-newsect));
	    newsect[sizeof(newsect)-1] = '\0';
	    if(gf_puts(boundary, pc)
		 && save_ex_output_body(stream, raw, newsect,
					&part->body, &ilen, pc))
	      *len += blen + ilen;
	    else
	      return(0);
	}
	while ((part = part->next) != NULL);	/* until done */

	snprintf(boundary, sizeof(boundary), "--%.*s--\015\012", sizeof(boundary)-10,param->value);
	*len += blen + 2;
	return(gf_puts(boundary, pc));
    }

    /* Start by writing the part's MIME header */
    if(!gf_nputs(txtp, *len, pc))
      return(0);
    
    if(body->type == TYPEMESSAGE
       && (!body->subtype || !strucmp(body->subtype, "rfc822"))){
	/* write RFC 822 message's header */
	if((txtp = mail_fetch_header(stream,raw,section,NULL,&ilen,FT_PEEK))
	     && gf_nputs(txtp, ilen, pc))
	  *len += ilen;
	else
	  return(0);

	/* then go deal with its body parts */
	snprintf(newsect, sizeof(newsect), "%.10s%s%s", section, section ? "." : "",
		(body->nested.msg->body->type == TYPEMULTIPART) ? "0" : "1");
	if(save_ex_output_body(stream, raw, newsect,
			       body->nested.msg->body, &ilen, pc)){
	    *len += ilen;
	    return(1);
	}

	return(0);
    }

    /* Write corresponding body part */
    if((txtp = pine_mail_fetch_body(stream, raw, section, &ilen, FT_PEEK))
       && gf_nputs(txtp, (long) ilen, pc) && gf_puts("\015\012", pc)){
	*len += ilen + 2;
	return(1);
    }

    return(0);
}


/*----------------------------------------------------------------------
    Mask off any header entries we don't want to show up in exceptional saves

Args:  hdr -- pointer to start of a header line
       pc -- function to write the prefix

  ----*/
int
save_ex_mask_types(char *hdr, long unsigned int *len, gf_io_t pc)
{
    char *s = NULL;

    if(!struncmp(hdr, "content-type:", 13))
      s = "Content-Type: Text/Plain; charset=US-ASCII\015\012X-";
    else if(!struncmp(hdr, "content-description:", 20))
      s = "Content-Description: Deleted Attachment\015\012X-";
    else if(!struncmp(hdr, "content-transfer-encoding:", 26)
	    || !struncmp(hdr, "content-disposition:", 20))
      s = "X-";

    return((*len = s ? strlen(s) : 0) ? gf_puts(s, pc) : 1);
}


int
save_ex_explain_body(struct mail_bodystruct *body, long unsigned int *len, gf_io_t pc)
{
    unsigned long   ilen;
    char	  **blurbp;
    static char    *blurb[] = {
	N_("The following attachment was DELETED when this message was saved:"),
	NULL
    };

    *len = 0;
    for(blurbp = blurb; *blurbp; blurbp++)
      if(save_ex_output_line(_(*blurbp), &ilen, pc))
	*len += ilen;
      else
	return(0);

    if(!save_ex_explain_parts(body, 0, &ilen, pc))
      return(0);

    *len += ilen;
    return(1);
}


int
save_ex_explain_parts(struct mail_bodystruct *body, int depth, long unsigned int *len, gf_io_t pc)
{
    char	  tmp[MAILTMPLEN], buftmp[MAILTMPLEN];
    unsigned long ilen;
    char *name = parameter_val(body->parameter, "name");

    if(body->type == TYPEMULTIPART) {   /* multipart gets special handling */
	PART *part = body->nested.part;	/* first body part */

	*len = 0;
	if(body->description && *body->description){
	    snprintf(tmp, sizeof(tmp), "%*.*sA %s/%.*s%.10s%.100s%.10s segment described",
		    depth, depth, " ", body_type_names(body->type),
		    sizeof(tmp)-300, body->subtype ? body->subtype : "Unknown",
		    name ? " (Name=\"" : "",
		    name ? name : "",
		    name ? "\")" : "");
	    if(!save_ex_output_line(tmp, len, pc))
	      return(0);

	    snprintf(buftmp, sizeof(buftmp), "%.75s", body->description);
	    snprintf(tmp, sizeof(tmp), "%*.*s  as \"%.50s\" containing:", depth, depth, " ",
		    (char *) rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf,
						    SIZEOF_20KBUF, buftmp));
	}
	else{
	    snprintf(tmp, sizeof(tmp), "%*.*sA %s/%.*s%.10s%.100s%.10s segment containing:",
		    depth, depth, " ",
		    body_type_names(body->type),
		    sizeof(tmp)-300, body->subtype ? body->subtype : "Unknown",
		    name ? " (Name=\"" : "",
		    name ? name : "",
		    name ? "\")" : "");
	}

	if(save_ex_output_line(tmp, &ilen, pc))
	  *len += ilen;
	else
	  return(0);

	depth++;
	do				/* for each part */
	  if(save_ex_explain_parts(&part->body, depth, &ilen, pc))
	    *len += ilen;
	  else
	    return(0);
	while ((part = part->next) != NULL);	/* until done */
    }
    else{
	snprintf(tmp, sizeof(tmp), "%*.*sA %s/%.*s%.10s%.100s%.10s segment of about %s bytes%s",
		depth, depth, " ",
		body_type_names(body->type), 
		sizeof(tmp)-300, body->subtype ? body->subtype : "Unknown",
		name ? " (Name=\"" : "",
		name ? name : "",
		name ? "\")" : "",
		comatose((body->encoding == ENCBASE64)
			   ? ((body->size.bytes * 3)/4)
			   : body->size.bytes),
		body->description ? "," : ".");
	if(!save_ex_output_line(tmp, len, pc))
	  return(0);

	if(body->description && *body->description){
	    snprintf(buftmp, sizeof(buftmp), "%.75s", body->description);
	    snprintf(tmp, sizeof(tmp), "%*.*s   described as \"%.*s\"", depth, depth, " ",
		    sizeof(tmp)-100,
		    (char *) rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf,
						    SIZEOF_20KBUF, buftmp));
	    if(save_ex_output_line(tmp, &ilen, pc))
	      *len += ilen;
	    else
	      return(0);
	}
    }

    return(1);
}


int
save_ex_output_line(char *line, long unsigned int *len, gf_io_t pc)
{
    snprintf(tmp_20k_buf, SIZEOF_20KBUF, "  [ %-*.*s ]\015\012", 68, 68, line);
    *len = strlen(tmp_20k_buf);
    return(gf_puts(tmp_20k_buf, pc));
}


/*----------------------------------------------------------------------
   Save() helper function to create canonical date string from given header

   Args: date -- buf to recieve canonical date string
	 header -- rfc822 header to fish date string from

 Result: date filled with canonicalized date in header, or null string
 ----*/
void
saved_date(char *date, char *header)
{
    char	 *d, *p, c;
    MESSAGECACHE  elt;

    *date = '\0';
    if((toupper((unsigned char)(*(d = header)))
	== 'D' && !strncmp(d, "date:", 5))
       || (d = srchstr(header, "\015\012date:"))){
	for(d += 7; *d == ' '; d++)
	  ;					/* skip white space */

	if((p = strstr(d, "\015\012")) != NULL){
	    for(; p > d && *p == ' '; p--)
	      ;					/* skip white space */

	    c  = *p;
	    *p = '\0';				/* tie off internal date */
	}

	if(mail_parse_date(&elt, (unsigned char *) d))  /* normalize it */
	  mail_date(date, &elt);

	if(p)					/* restore header */
	  *p = c;
    }
}


MAILSTREAM *
save_msg_stream(CONTEXT_S *context, char *folder, int *we_opened)
{
    char	tmp[MAILTMPLEN];
    MAILSTREAM *save_stream = NULL;

    if(IS_REMOTE(context_apply(tmp, context, folder, sizeof(tmp)))
       && !(save_stream = sp_stream_get(tmp, SP_MATCH))
       && !(save_stream = sp_stream_get(tmp, SP_SAME))){
	if((save_stream = context_open(context, NULL, folder,
				      OP_HALFOPEN | SP_USEPOOL | SP_TEMPUSE,
				      NULL)) != NULL)
	  *we_opened = 1;
    }

    return(save_stream);
}


/*----------------------------------------------------------------------
    Offer to create a non-existant folder to copy message[s] into

   Args: context -- context to create folder in
	 folder -- name of folder to create

 Result: 0 if create failed (c-client writes error)
	 1 if create successful
	-1 if user declines to create folder
 ----*/
int
create_for_save(CONTEXT_S *context, char *folder)
{
    int ret;

    if(pith_opt_save_create_prompt
       && (ret = (*pith_opt_save_create_prompt)(context, folder, 1)) != 1)
      return(ret);

    return(context_create(context, NULL, folder));
}
