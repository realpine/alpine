#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: reply.c 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $";
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
   Code here for forward and reply to mail
   A few support routines as well

  This code will forward and reply to MIME messages. The Alpine composer
at this time will only support non-text segments at the end of a
message so, things don't always come out as one would like. If you
always forward a message in MIME format, all will be correct.  Forwarding
of nested MULTIPART messages will work.  There's still a problem with
MULTIPART/ALTERNATIVE as the "first text part" rule doesn't allow modifying
the equivalent parts.  Ideally, we should probably such segments as a 
single attachment when forwarding/replying.  It would also be real nice to
flatten out the nesting in the composer so pieces inside can get snipped.

The evolution continues...
  =====*/


#include "headers.h"
#include "reply.h"
#include "status.h"
#include "radio.h"
#include "send.h"
#include "titlebar.h"
#include "mailindx.h"
#include "help.h"
#include "signal.h"
#include "mailcmd.h"
#include "alpine.h"
#include "roleconf.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/init.h"
#include "../pith/filter.h"
#include "../pith/pattern.h"
#include "../pith/charset.h"
#include "../pith/mimedesc.h"
#include "../pith/remote.h"
#include "../pith/news.h"
#include "../pith/util.h"
#include "../pith/detoken.h"
#include "../pith/newmail.h"
#include "../pith/readfile.h"
#include "../pith/tempfile.h"
#include "../pith/busy.h"
#include "../pith/ablookup.h"


/*
 * Internal Prototypes
 */
int	 reply_poster_followup(ENVELOPE *);
int	 sigedit_exit_for_pico(struct headerentry *, void (*)(void), int, char **);
long	 new_mail_for_pico(int, int);
void	 cmd_input_for_pico(void);
int	 display_message_for_pico(int);
char	*checkpoint_dir_for_pico(char *, size_t);
void	 resize_for_pico(void);
PCOLORS *colors_for_pico(void);
void	 free_pcolors(PCOLORS **);


/*----------------------------------------------------------------------
        Fill in an outgoing message for reply and pass off to send

    Args: pine_state -- The usual pine structure

  Result: Reply is formatted and passed off to composer/mailer

Reply

   - put senders address in To field
   - search to and cc fields to see if we aren't the only recipients
   - if other than us, ask if we should reply to all.
   - if answer yes, fill out the To and Cc fields
   - fill in the fcc argument
   - fill in the subject field
   - fill out the body and the attachments
   - pass off to pine_send()
  ---*/
int
reply(struct pine *pine_state, ACTION_S *role_arg)
{
    ADDRESS    *saved_from, *saved_to, *saved_cc, *saved_resent;
    ADDRESS    *us_in_to_and_cc, *ap;
    ENVELOPE   *env, *outgoing;
    BODY       *body, *orig_body = NULL;
    REPLY_S     reply;
    void       *msgtext = NULL;
    char       *tmpfix = NULL, *prefix = NULL, *fcc = NULL, *errmsg = NULL;
    long        msgno, j, totalm, rflags, *seq = NULL;
    int         i, include_text = 0, times = -1, warned = 0, rv = 0,
		flags = RSF_QUERY_REPLY_ALL, reply_raw_body = 0;
    int         rolemsg = 0, copytomsg = 0;
    gf_io_t     pc;
    PAT_STATE   dummy;
    REDRAFT_POS_S *redraft_pos = NULL;
    ACTION_S   *role = NULL, *nrole;
#if	defined(DOS) && !defined(_WINDOWS)
    char *reserve;
#endif

    outgoing = mail_newenvelope();
    totalm   = mn_total_cur(pine_state->msgmap);
    seq	     = (long *)fs_get(((size_t)totalm + 1) * sizeof(long));

    dprint((4,"\n - reply (%s msgs) -\n", comatose(totalm)));

    saved_from		  = (ADDRESS *) NULL;
    saved_to		  = (ADDRESS *) NULL;
    saved_cc		  = (ADDRESS *) NULL;
    saved_resent	  = (ADDRESS *) NULL;

    us_in_to_and_cc	  = (ADDRESS *) NULL;

    outgoing->subject	  = NULL;

    memset((void *)&reply, 0, sizeof(reply));

    if(ps_global->full_header == 2
       && F_ON(F_ENABLE_FULL_HDR_AND_TEXT, ps_global))
      reply_raw_body = 1;

    /*
     * We may have to loop through first to figure out what default
     * reply-indent-string to offer...
     */
    if(mn_total_cur(pine_state->msgmap) > 1 &&
       F_ON(F_ENABLE_EDIT_REPLY_INDENT, pine_state) &&
       reply_quote_str_contains_tokens()){
	for(msgno = mn_first_cur(pine_state->msgmap);
	    msgno > 0L && !tmpfix;
	    msgno = mn_next_cur(pine_state->msgmap)){

	    env = pine_mail_fetchstructure(pine_state->mail_stream,
					   mn_m2raw(pine_state->msgmap, msgno),
					   NULL);
	    if(!env) {
		q_status_message1(SM_ORDER,3,4,
			    _("Error fetching message %s. Can't reply to it."),
				long2string(msgno));
		goto done_early;
	    }

	    if(!tmpfix){			/* look for prefix? */
		tmpfix = reply_quote_str(env);
		if(prefix){
		    i = strcmp(tmpfix, prefix);
		    fs_give((void **) &tmpfix);
		    if(i){		/* don't check back if dissimilar */
			fs_give((void **) &prefix);
			/*
			 * We free prefix, not tmpfix. We set tmpfix to prefix
			 * so that we won't come check again.
			 */
			tmpfix = prefix = cpystr("> ");
		    }
		}
		else{
		    prefix = tmpfix;
		    tmpfix = NULL;		/* check back later? */
		}
	    }
	}

	tmpfix = prefix;
    }

    /*
     * Loop thru the selected messages building the
     * outgoing envelope's destinations...
     */
    for(msgno = mn_first_cur(pine_state->msgmap);
	msgno > 0L;
	msgno = mn_next_cur(pine_state->msgmap)){

	/*--- Grab current envelope ---*/
	env = pine_mail_fetchstructure(pine_state->mail_stream,
			    seq[++times] = mn_m2raw(pine_state->msgmap, msgno),
			    NULL);
	if(!env) {
	    q_status_message1(SM_ORDER,3,4,
			  _("Error fetching message %s. Can't reply to it."),
			      long2string(msgno));
	    goto done_early;
	}

	/*
	 * We check for the prefix here if we didn't do it in the first
	 * loop above. This is just to save having to go through the loop
	 * twice in the cases where we don't need to.
	 */
	if(!tmpfix){
	    tmpfix = reply_quote_str(env);
	    if(prefix){
		i = strcmp(tmpfix, prefix);
		fs_give((void **) &tmpfix);
		if(i){			/* don't check back if dissimilar */
		    fs_give((void **) &prefix);
		    tmpfix = prefix = cpystr("> ");
		}
	    }
	    else{
		prefix = tmpfix;
		tmpfix = NULL;		/* check back later? */
	    }
	}

	/*
	 * For consistency, the first question is always "include text?"
	 */
	if(!times){		/* only first time */
	    char *p = cpystr(prefix);

	    if((include_text=reply_text_query(pine_state,totalm,&prefix)) < 0)
	      goto done_early;
	    
	    /* edited prefix? */
	    if(strcmp(p, prefix))
	      tmpfix = prefix;		/* stop looking */
	    
	    fs_give((void **)&p);
	}
	
	/*
	 * If we're agg-replying or there's a newsgroup and the user want's
	 * to post to news *and* via email, add relevant addresses to the
	 * outgoing envelope...
	 *
	 * The single message case gets us around the aggregate reply
	 * to messages in a mixed mail-news archive where some might
	 * have newsgroups and others not or whatever.
	 */
	if(totalm > 1L || ((i = reply_news_test(env, outgoing)) & 1)){
	    if(totalm > 1)
	      flags |= RSF_FORCE_REPLY_TO;

	    if(!reply_harvest(pine_state, seq[times], NULL, env,
			      &saved_from, &saved_to, &saved_cc,
			      &saved_resent, &flags))
	      goto done_early;
	}
	else if(i == 0)
	  goto done_early;

	/* collect a list of addresses that are us in to and cc fields */
	if(env->to)
	  if((ap=reply_cp_addr(pine_state, 0L, NULL,
			       NULL, us_in_to_and_cc, NULL,
			       env->to, RCA_ONLY_US)) != NULL)
	    reply_append_addr(&us_in_to_and_cc, ap);

	if(env->cc)
	  if((ap=reply_cp_addr(pine_state, 0L, NULL,
			       NULL, us_in_to_and_cc, NULL,
			       env->cc, RCA_ONLY_US)) != NULL)
	    reply_append_addr(&us_in_to_and_cc, ap);

	/*------------ Format the subject line ---------------*/
	if(outgoing->subject){
	    /*
	     * if reply to more than one message, and all subjects
	     * match, so be it.  otherwise set it to something generic...
	     */
	    if(strucmp(outgoing->subject,
		       reply_subject(env->subject,tmp_20k_buf,SIZEOF_20KBUF))){
		fs_give((void **)&outgoing->subject);
		outgoing->subject = cpystr("Re: several messages");
	    }
	}
	else
	  outgoing->subject = reply_subject(env->subject, NULL, 0);
    }

    /* fill reply header */
    reply_seed(pine_state, outgoing, env, saved_from,
	       saved_to, saved_cc, saved_resent,
	       &fcc, flags & RSF_FORCE_REPLY_ALL, &errmsg);
    if(errmsg){
	if(*errmsg){
	    q_status_message1(SM_ORDER, 3, 3, "%.200s", errmsg);
	    display_message(NO_OP_COMMAND);
	}

	fs_give((void **)&errmsg);
    }

    if(sp_expunge_count(pine_state->mail_stream))	/* cur msg expunged */
      goto done_early;

    /* Setup possible role */
    if(role_arg)
      role = copy_action(role_arg);

    if(!role){
	rflags = ROLE_REPLY;
	if(nonempty_patterns(rflags, &dummy)){
	    /* setup default role */
	    nrole = NULL;
	    j = mn_first_cur(pine_state->msgmap);
	    do {
		role = nrole;
		nrole = set_role_from_msg(pine_state, rflags,
					  mn_m2raw(pine_state->msgmap, j),
					  NULL);
	    } while(nrole && (!role || nrole == role)
		    && (j=mn_next_cur(pine_state->msgmap)) > 0L);

	    if(!role || nrole == role)
	      role = nrole;
	    else
	      role = NULL;

	    if(confirm_role(rflags, &role))
	      role = combine_inherited_role(role);
	    else{				/* cancel reply */
		role = NULL;
		cmd_cancelled("Reply");
		goto done_early;
	    }
	}
    }

    /*
     * Reply_seed may call c-client in get_fcc_based_on_to, so env may
     * no longer be valid. Get it again.
     * Similarly for set_role_from_message.
     */
    env = pine_mail_fetchstructure(pine_state->mail_stream, seq[times], NULL);

    if(role){
	rolemsg++;
	/* override fcc gotten in reply_seed */
	if(role->fcc && fcc)
	  fs_give((void **) &fcc);
    }

    if(F_ON(F_COPY_TO_TO_FROM, pine_state) && !(role && role->from)){
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

    if(us_in_to_and_cc)
      mail_free_address(&us_in_to_and_cc);


    seq[++times] = -1L;		/* mark end of sequence list */

    /*==========  Other miscelaneous fields ===================*/   
    outgoing->in_reply_to = reply_in_reply_to(env);
    outgoing->references = reply_build_refs(env);
    outgoing->message_id = generate_message_id();

    if(!outgoing->to &&
       !outgoing->cc &&
       !outgoing->bcc &&
       !outgoing->newsgroups)
      q_status_message(SM_ORDER | SM_DING, 3, 6,
		       _("Warning: no valid addresses to reply to!"));


   /*==================== Now fix up the message body ====================*/

    /* 
     * create storage object to be used for message text
     */
    if((msgtext = (void *)so_get(PicoText, NULL, EDIT_ACCESS)) == NULL){
        q_status_message(SM_ORDER | SM_DING, 3, 4,
			 _("Error allocating message text"));
        goto done_early;
    }

    gf_set_so_writec(&pc, (STORE_S *) msgtext);

    /*---- Include the original text if requested ----*/
    if(include_text && totalm > 1L){
	char *sig;
	int   impl, template_len = 0, leave_cursor_at_top = 0;


	env = NULL;
        if(role && role->template){
	    char *filtered;

	    impl = 0;
	    filtered = detoken(role, env, 0,
			       F_ON(F_SIG_AT_BOTTOM, ps_global) ? 1 : 0,
			       0, &redraft_pos, &impl);
	    if(filtered){
		if(*filtered){
		    so_puts((STORE_S *)msgtext, filtered);
		    if(impl == 1)
		      template_len = strlen(filtered);
		    else if(impl == 2)
		      leave_cursor_at_top++;
		}

		fs_give((void **)&filtered);
	    }
	    else
	      impl = 1;
	}
	else
	  impl = 1;

	if((sig = reply_signature(role, env, &redraft_pos, &impl)) &&
	   F_OFF(F_SIG_AT_BOTTOM, ps_global)){

	    /*
	     * If CURSORPOS was set explicitly in sig_file, and there was a
	     * template file before that, we need to adjust the offset by the
	     * length of the template file. However, if the template had
	     * a set CURSORPOS in it then impl was 2 before getting to the
	     * signature, so offset wouldn't have been reset by the signature
	     * CURSORPOS and offset would already be correct. That case will
	     * be ok here because template_len will be 0 and adding it does
	     * nothing. If template
	     * didn't have CURSORPOS in it, then impl was 1 and got set to 2
	     * by the CURSORPOS in the sig. In that case we have to adjust the
	     * offset. That's what the next line does. It adjusts it if
	     * template_len is nonzero and if CURSORPOS was set in sig_file.
	     */
	    if(impl == 2)
	      redraft_pos->offset += template_len;
	    
	    if(*sig)
	      so_puts((STORE_S *)msgtext, sig);
	    
	    /*
	     * Set sig to NULL because we've already used it. If SIG_AT_BOTTOM
	     * is set, we won't have used it yet and want it to be non-NULL.
	     */
	    fs_give((void **)&sig);
	}

	/*
	 * Only put cursor in sig if there is a cursorpos there but not
	 * one in the template, and sig-at-bottom.
	 */
	 if(!(sig && impl == 2 && !leave_cursor_at_top))
	   leave_cursor_at_top++;

	body                  = mail_newbody();
	body->type            = TYPETEXT;
	body->contents.text.data = msgtext;

	for(msgno = mn_first_cur(pine_state->msgmap);
	    msgno > 0L;
	    msgno = mn_next_cur(pine_state->msgmap)){

	    if(env){			/* put 2 between messages */
		gf_puts(NEWLINE, pc);
		gf_puts(NEWLINE, pc);
	    }

	    /*--- Grab current envelope ---*/
	    env = pine_mail_fetchstructure(pine_state->mail_stream,
					   mn_m2raw(pine_state->msgmap, msgno),
					   &orig_body);
	    if(!env){
		q_status_message1(SM_ORDER,3,4,
			  _("Error fetching message %s. Can't reply to it."),
			      long2string(mn_get_cur(pine_state->msgmap)));
		goto done_early;
	    }

	    if(orig_body == NULL || orig_body->type == TYPETEXT || reply_raw_body) {
		reply_delimiter(env, role, pc);
		if(F_ON(F_INCLUDE_HEADER, pine_state))
		  reply_forward_header(pine_state->mail_stream,
				       mn_m2raw(pine_state->msgmap,msgno),
				       NULL, env, pc, prefix);

		get_body_part_text(pine_state->mail_stream, reply_raw_body ? NULL : orig_body,
				   mn_m2raw(pine_state->msgmap, msgno),
				   reply_raw_body ? NULL : "1", 0L, pc, prefix,
				   NULL, GBPT_NONE);
	    }
	    else if(orig_body->type == TYPEMULTIPART) {
		if(!warned++)
		  q_status_message(SM_ORDER,3,7,
		      _("WARNING!  Attachments not included in multiple reply."));

		if(orig_body->nested.part
		   && orig_body->nested.part->body.type == TYPETEXT) {
		    /*---- First part of the message is text -----*/
		    reply_delimiter(env, role, pc);
		    if(F_ON(F_INCLUDE_HEADER, pine_state))
		      reply_forward_header(pine_state->mail_stream,
					   mn_m2raw(pine_state->msgmap,
						    msgno),
					   NULL, env, pc, prefix);

		    get_body_part_text(pine_state->mail_stream,
				       &orig_body->nested.part->body,
				       mn_m2raw(pine_state->msgmap, msgno),
				       "1", 0L, pc, prefix, NULL, GBPT_NONE);
		}
		else{
		    q_status_message(SM_ORDER,0,3,
				     _("Multipart with no leading text part."));
		}
	    }
	    else{
		/*---- Single non-text message of some sort ----*/
		q_status_message(SM_ORDER,3,3,
				 _("Non-text message not included."));
	    }
	}

	if(!leave_cursor_at_top){
	    long  cnt = 0L;
	    unsigned char c;

	    /* rewind and count chars to start of sig file */
	    so_seek((STORE_S *)msgtext, 0L, 0);
	    while(so_readc(&c, (STORE_S *)msgtext))
	      cnt++;

	    if(!redraft_pos){
		redraft_pos = (REDRAFT_POS_S *)fs_get(sizeof(*redraft_pos));
		memset((void *)redraft_pos, 0,sizeof(*redraft_pos));
		redraft_pos->hdrname = cpystr(":");
	    }

	    /*
	     * If explicit cursor positioning in sig file,
	     * add offset to start of sig file plus offset into sig file.
	     * Else, just offset to start of sig file.
	     */
	    redraft_pos->offset += cnt;
	}

	if(sig){
	    if(*sig)
	      so_puts((STORE_S *)msgtext, sig);
	    
	    fs_give((void **)&sig);
	}
    }
    else{
	msgno = mn_m2raw(pine_state->msgmap,
			 mn_get_cur(pine_state->msgmap));

	/*--- Grab current envelope ---*/
	env = pine_mail_fetchstructure(pine_state->mail_stream, msgno,
				       &orig_body);

	/*
	 * If the charset of the body part is different from ascii and
	 * charset conversion is _not_ happening, then preserve the original
	 * charset from the message so that if we don't enter any new
	 * chars with the hibit set we can use the original charset.
	 * If not all those things, then don't try to preserve it.
	 */
	if(orig_body){
	    char *charset;

	    charset = parameter_val(orig_body->parameter, "charset");
	    if(charset && strucmp(charset, "us-ascii") != 0){
		CONV_TABLE *ct;

		/*
		 * There is a non-ascii charset, is there conversion happening?
		 */
		if(!(ct=conversion_table(charset, ps_global->posting_charmap)) || !ct->table){
		    reply.orig_charset = charset;
		    charset = NULL;
		}
	    }

	    if(charset)
	      fs_give((void **) &charset);
	}

	if(env) {
	    if(!(body = reply_body(pine_state->mail_stream, env, orig_body,
				   msgno, NULL, msgtext, prefix,
				   include_text, role, 1, &redraft_pos)))
	       goto done_early;
	}
	else{
	    q_status_message1(SM_ORDER,3,4,
			  _("Error fetching message %s. Can't reply to it."),
			      long2string(mn_get_cur(pine_state->msgmap)));
	    goto done_early;
	}
    }

    /* fill in reply structure */
    reply.prefix	= prefix;
    reply.mailbox	= cpystr(pine_state->mail_stream->mailbox);
    reply.origmbox	= cpystr(pine_state->mail_stream->original_mailbox
				    ? pine_state->mail_stream->original_mailbox
				    : pine_state->mail_stream->mailbox);
    reply.data.uid.msgs = (imapuid_t *) fs_get((times + 1) * sizeof(imapuid_t));
    if((reply.data.uid.validity = pine_state->mail_stream->uid_validity) != 0){
	reply.uid = 1;
	for(i = 0; i < times ; i++)
	  reply.data.uid.msgs[i] = mail_uid(pine_state->mail_stream, seq[i]);
    }
    else{
	reply.msgno = 1;
	for(i = 0; i < times ; i++)
	  reply.data.uid.msgs[i] = seq[i];
    }

    reply.data.uid.msgs[i] = 0;			/* tie off list */

#if	defined(DOS) && !defined(_WINDOWS)
    free((void *)reserve);
#endif

    /* partially formatted outgoing message */
    pine_send(outgoing, &body, _("COMPOSE MESSAGE REPLY"),
	      role, fcc, &reply, redraft_pos, NULL, NULL, 0);

    rv++;
    pine_free_body(&body);
    if(reply.mailbox)
      fs_give((void **) &reply.mailbox);
    if(reply.origmbox)
      fs_give((void **) &reply.origmbox);
    if(reply.orig_charset)
      fs_give((void **) &reply.orig_charset);
    fs_give((void **) &reply.data.uid.msgs);
  done_early:
    if((STORE_S *) msgtext)
      gf_clear_so_writec((STORE_S *) msgtext);

    mail_free_envelope(&outgoing);
    mail_free_address(&saved_from);
    mail_free_address(&saved_to);
    mail_free_address(&saved_cc);
    mail_free_address(&saved_resent);

    fs_give((void **)&seq);

    if(prefix)
      fs_give((void **)&prefix);

    if(fcc)
      fs_give((void **) &fcc);

    free_redraft_pos(&redraft_pos);
    free_action(&role);
    return rv;
}


/*
 * Ask user to confirm role choice, or choose another role.
 *
 * Args    role -- A pointer into the pattern_h space at the default
 *                    role to use. This can't be a copy, the comparison
 *                    relies on it pointing at the actual role.
 *                    This arg is also used to return a pointer to the
 *                    chosen role.
 *
 * Returns   1 -- Yes, use role which is now in *role. This may not be
 *                the same as the role passed in and it may even be NULL.
 *           0 -- Cancel reply.
 */
int
confirm_role(long int rflags, ACTION_S **role)
{
    ACTION_S       *role_p = NULL;
    ACTION_S       *default_role = NULL;
    char            prompt[80], *prompt_fodder;
    int             cmd, done, ret = 1;
    void (*prev_screen)(struct pine *) = ps_global->prev_screen,
	 (*redraw)(void) = ps_global->redrawer;
    PAT_S          *curpat, *pat;
    PAT_STATE       pstate;
    HelpType        help;
    ESCKEY_S        ekey[4];

    if(!nonempty_patterns(ROLE_DO_ROLES, &pstate) || !role)
      return(ret);
    
    /*
     * If this is a reply or forward and the role doesn't require confirmation,
     * then we just return with what was passed in.
     */
    if(((rflags & ROLE_REPLY) &&
	*role && (*role)->repl_type == ROLE_REPL_NOCONF) ||
       ((rflags & ROLE_FORWARD) &&
	*role && (*role)->forw_type == ROLE_FORW_NOCONF) ||
       ((rflags & ROLE_COMPOSE) &&
	*role && (*role)->comp_type == ROLE_COMP_NOCONF) ||
       (!*role && F_OFF(F_ROLE_CONFIRM_DEFAULT, ps_global)
        && !ps_global->default_role))
      return(ret);

    /*
     * Check that there is at least one role available. This is among all
     * roles, not just the reply roles or just the forward roles. That's
     * because we have ^T take us to all the roles, not the category-specific
     * roles.
     */
    if(!(pat = last_pattern(&pstate)))
      return(ret);
    
    ekey[0].ch    = 'y';
    ekey[0].rval  = 'y';

    ekey[1].ch    = 'n';
    ekey[1].rval  = 'n';

    ekey[2].ch    = ctrl('T');
    ekey[2].rval  = 2;
    ekey[2].name  = "^T";

    ekey[3].ch    = -1;

    /* check for more than one role available (or no role set) */
    if(pat == first_pattern(&pstate) && *role)	/* no ^T */
      ekey[2].ch    = -1;
    
    /*
     * Setup default role
     * Go through the loop just in case default_role doesn't point
     * to a real current role.
     */
    if(ps_global->default_role){
	for(pat = first_pattern(&pstate);
	    pat;
	    pat = next_pattern(&pstate)){
	    if(pat->action == ps_global->default_role){
		default_role = ps_global->default_role;
		break;
	    }
	}
    }

    curpat = NULL;

    /* override default */
    if(*role){
	for(pat = first_pattern(&pstate);
	    pat;
	    pat = next_pattern(&pstate)){
	    if(pat->action == *role){
		curpat = pat;
		break;
	    }
	}
    }

    if(rflags & ROLE_REPLY)
      prompt_fodder = _("Reply");
    else if(rflags & ROLE_FORWARD)
      prompt_fodder = _("Forward");
    else
      prompt_fodder = _("Compose");

    done = 0;
    while(!done){
	if(curpat){
	    char buf[100];

	    help = h_role_confirm;
	    ekey[0].name  = "Y";
	    ekey[0].label = N_("Yes");
	    ekey[1].name  = "N";
	    if(default_role)
	      ekey[1].label = N_("No, use default role");
	    else
	      ekey[1].label = N_("No, use default settings");

	    ekey[2].label = N_("To Select Alternate Role");

	    if(curpat->patgrp && curpat->patgrp->nick)
	      /* TRANSLATORS: This is something like Use role <nickname of role> for Reply? */
	      snprintf(prompt, sizeof(prompt), _("Use role \"%s\" for %s? "),
		      short_str(curpat->patgrp->nick, buf, sizeof(buf), 50, MidDots),
		      prompt_fodder);
	    else
	      snprintf(prompt, sizeof(prompt),
		      _("Use role \"<a role without a nickname>\" for %s? "),
		      prompt_fodder);
	}
	else{
	    help = h_norole_confirm;
	    ekey[0].name  = "Ret";
	    ekey[0].label = prompt_fodder;
	    ekey[1].name  = "";
	    ekey[1].label = "";
	    ekey[2].label = N_("To Select Role");
	    snprintf(prompt, sizeof(prompt),
		    _("Press Return to %s using %s role, or ^T to select a role "),
		    prompt_fodder, default_role ? _("default") : _("no"));
	}

	prompt[sizeof(prompt)-1] = '\0';

	cmd = radio_buttons(prompt, -FOOTER_ROWS(ps_global), ekey,
			    'y', 'x', help, RB_NORM);

	switch(cmd){
	  case 'y':					/* Accept */
	    done++;
	    *role = curpat ? curpat->action : default_role;
	    break;

	  case 'x':					/* Cancel */
	    ret = 0;
	    /* fall through */

	  case 'n':					/* NoRole */
	    done++;
	    *role = default_role;
	    break;

	  case 2:					/* ^T */
	    if(role_select_screen(ps_global, &role_p, 0) >= 0){
		if(role_p){
		    for(pat = first_pattern(&pstate);
			pat;
			pat = next_pattern(&pstate)){
			if(pat->action == role_p){
			    curpat = pat;
			    break;
			}
		    }
		}
		else
		  curpat = NULL;
	    }

	    ClearBody();
	    ps_global->mangled_body = 1;
	    ps_global->prev_screen = prev_screen;
	    ps_global->redrawer = redraw;
	    break;
	}
    }

    return(ret);
}


/*
 * reply_to_all_query - Ask user about replying to all recipients
 *
 * Returns:  -1 if cancel, 0 otherwise
 *	     by reference: flagp
 */
int
reply_to_all_query(int *flagp)
{
    switch(want_to("Reply to all recipients",
		   'n', 'x', NO_HELP, WT_SEQ_SENSITIVE)){
      case 'x' :
	return(-1);

      case 'y' :		/* set reply-all bit */
	(*flagp) |= RSF_FORCE_REPLY_ALL;
	break;

      case 'n' :		/* clear reply-all bit */
	(*flagp) &= ~RSF_FORCE_REPLY_ALL;
	break;
    }

    return(0);
}


/*
 * reply_using_replyto_query - Ask user about replying with reply-to value
 *
 * Returns:  'y' if yes
 *	     'x' if cancel
 */
int
reply_using_replyto_query(void)
{
    return(want_to("Use \"Reply-To:\" address instead of \"From:\" address",
		   'y', 'x', NO_HELP,WT_SEQ_SENSITIVE));
}


/*
 * reply_text_query - Ask user about replying with text...
 *
 * Returns:  1 if include the text
 *	     0 if we're NOT to include the text
 *	    -1 on cancel or error
 */
int
reply_text_query(struct pine *ps, long int many, char **prefix)
{
    int ret, edited = 0;
    static ESCKEY_S rtq_opts[] = {
	{'y', 'y', "Y", N_("Yes")},
	{'n', 'n', "N", N_("No")},
	{-1, 0, NULL, NULL},	                  /* may be overridden below */
	{-1, 0, NULL, NULL}
    };

    if(F_ON(F_AUTO_INCLUDE_IN_REPLY, ps)
       && F_OFF(F_ENABLE_EDIT_REPLY_INDENT, ps))
      return(1);

    while(1){
	if(many > 1L)
	  /* TRANSLATORS: The final three %s's can probably be safely ignored */
	  snprintf(tmp_20k_buf, SIZEOF_20KBUF, _("Include %s original messages in Reply%s%s%s? "),
		comatose(many),
		F_ON(F_ENABLE_EDIT_REPLY_INDENT, ps) ? " (using \"" : "",
		F_ON(F_ENABLE_EDIT_REPLY_INDENT, ps) ? *prefix : "",
		F_ON(F_ENABLE_EDIT_REPLY_INDENT, ps) ? "\")" : "");
	else
	  snprintf(tmp_20k_buf, SIZEOF_20KBUF, _("Include original message in Reply%s%s%s? "),
		F_ON(F_ENABLE_EDIT_REPLY_INDENT, ps) ? " (using \"" : "",
		F_ON(F_ENABLE_EDIT_REPLY_INDENT, ps) ? *prefix : "",
		F_ON(F_ENABLE_EDIT_REPLY_INDENT, ps) ? "\")" : "");

	if(F_ON(F_ENABLE_EDIT_REPLY_INDENT, ps)){
	    rtq_opts[2].ch    = ctrl('R');
	    rtq_opts[2].rval  = 'r';
	    rtq_opts[2].name  = "^R";
	    rtq_opts[2].label = N_("Edit Indent String");
	}
	else
	  rtq_opts[2].ch    = -1;

	switch(ret = radio_buttons(tmp_20k_buf, 
				   ps->ttyo->screen_rows > 4
				     ? -FOOTER_ROWS(ps_global) : -1,
				   rtq_opts,
				   (edited || F_ON(F_AUTO_INCLUDE_IN_REPLY, ps))
				       ? 'y' : 'n',
				   'x', NO_HELP, RB_SEQ_SENSITIVE)){
	  case 'x':
	    cmd_cancelled("Reply");
	    return(-1);

	  case 'r':
	    if(prefix && *prefix){
		int  done = 0;
		char buf[64];
		int  flags;

		while(!done){
		    strncpy(buf, *prefix, sizeof(buf)-1);
		    buf[sizeof(buf)-1] = '\0';

		    flags = OE_APPEND_CURRENT |
			    OE_KEEP_TRAILING_SPACE |
			    OE_DISALLOW_HELP |
			    OE_SEQ_SENSITIVE;

		    switch(optionally_enter(buf, ps->ttyo->screen_rows > 4
					    ? -FOOTER_ROWS(ps_global) : -1,
					    0, sizeof(buf), "Reply prefix : ", 
					    NULL, NO_HELP, &flags)){
		      case 0:		/* entry successful, continue */
			if(flags & OE_USER_MODIFIED){
			    fs_give((void **)prefix);
			    *prefix = removing_quotes(cpystr(buf));
			    edited = 1;
			}

			done++;
			break;

		      case 1:
			cmd_cancelled("Reply");

		      case -1:
			return(-1);

		      case 4:
			EndInverse();
			ClearScreen();
			redraw_titlebar();
			if(ps_global->redrawer != NULL)
			  (*ps_global->redrawer)();

			redraw_keymenu();
			break;

		      case 3:
			break;

		      case 2:
		      default:
			q_status_message(SM_ORDER, 3, 4,
				 "Programmer botch in reply_text_query()");
			return(-1);
		    }
		}
	    }

	    break;

	  case 'y':
	    return(1);

	  case 'n':
	    return(0);

	  default:
	    q_status_message1(SM_ORDER, 3, 4,
			      "Invalid rval \'%s\'", pretty_command(ret));
	    return(-1);
	}
    }
}


/*
 * reply_poster_followup - return TRUE if "followup-to" set to "poster"
 *
 * NOTE: queues status message indicating such
 */
int
reply_poster_followup(ENVELOPE *e)
{
    if(e && e->followup_to && !strucmp(e->followup_to, "poster")){
	q_status_message(SM_ORDER, 2, 3,
			 _("Replying to Poster as specified in \"Followup-To\""));
	return(1);
    }

    return(0);
}


/*
 * reply_news_test - Test given envelope for newsgroup data and copy
 *		     it at the users request
 *	RETURNS:
 *	     0  if error or cancel
 *	     1     reply via email
 *           2     follow-up via news
 *	     3     do both
 */
int
reply_news_test(ENVELOPE *env, ENVELOPE *outgoing)
{
    int    ret = 1;
    static ESCKEY_S news_opt[] = { {'f', 'f', "F", N_("Follow-up")},
				   {'r', 'r', "R", N_("Reply")},
				   {'b', 'b', "B", N_("Both")},
				   {-1, 0, NULL, NULL} };

    if(env->newsgroups && *env->newsgroups && !reply_poster_followup(env))
      /*
       * Now that we know a newsgroups field is present, 
       * ask if the user is posting a follow-up article...
       */
      switch(radio_buttons(
	     _("Follow-up to news group(s), Reply via email to author or Both? "),
			   -FOOTER_ROWS(ps_global), news_opt, 'r', 'x',
			   NO_HELP, RB_NORM)){
	case 'r' :		/* Reply */
	  ret = 1;
	  break;

	case 'f' :		/* Follow-Up via news ONLY! */
	  ret = 2;
	  break;

	case 'b' :		/* BOTH */
	  ret = 3;
	  break;

	case 'x' :		/* cancel or unknown response */
	default  :
	  cmd_cancelled("Reply");
	  ret = 0;
	  break;
      }

    if(ret > 1){
	if(env->followup_to){
	    q_status_message(SM_ORDER, 2, 3,
			     _("Posting to specified Followup-To groups"));
	    outgoing->newsgroups = cpystr(env->followup_to);
	}
	else if(!outgoing->newsgroups)
	  outgoing->newsgroups = cpystr(env->newsgroups);
	if(!IS_NEWS(ps_global->mail_stream))
	  q_status_message(SM_ORDER, 2, 3,
 _("Replying to message that MAY or MAY NOT have been posted to newsgroup"));
    }

    return(ret);
}


/*----------------------------------------------------------------------
  Acquire the pinerc defined signature file
  It is allocated here and freed by the caller.

          file -- use this file
   prenewlines -- prefix the file contents with this many newlines
  postnewlines -- postfix the file contents with this many newlines
        is_sig -- this is a signature (not a template)
  ----*/
char *
get_signature_file(char *file, int prenewlines, int postnewlines, int is_sig)
{
    char     *sig, *tmp_sig = NULL, sig_path[MAXPATH+1];
    int       len, do_the_pipe_thang = 0;
    long      sigsize = 0L, cntdown;

    sig_path[0] = '\0';
    if(!signature_path(file, sig_path, MAXPATH))
      return(NULL);

    dprint((5, "get_signature(%s)\n", sig_path));

    if(sig_path[(len=strlen(sig_path))-1] == '|'){
	if(is_sig && F_ON(F_DISABLE_PIPES_IN_SIGS, ps_global)){
	    q_status_message(SM_ORDER | SM_DING, 3, 4,
			 _("Pipes for signatures are administratively disabled"));
	    return(NULL);
	}
	else if(!is_sig && F_ON(F_DISABLE_PIPES_IN_TEMPLATES, ps_global)){
	    q_status_message(SM_ORDER | SM_DING, 3, 4,
			 _("Pipes for templates are administratively disabled"));
	    return(NULL);
	}
	    
	sig_path[len-1] = '\0';
	removing_trailing_white_space(sig_path);
	do_the_pipe_thang++;
    }

    if(!IS_REMOTE(sig_path) && ps_global->VAR_OPER_DIR &&
       !in_dir(ps_global->VAR_OPER_DIR, sig_path)){
	q_status_message2(SM_ORDER | SM_DING, 3, 4,
			  /* TRANSLATORS: First arg is the directory name, second is
			     the file user wants to read but can't. */
			  _("Can't read file outside %s: %s"),
			  ps_global->VAR_OPER_DIR, file);
	
	return(NULL);
    }

    if(IS_REMOTE(sig_path) || can_access(sig_path, ACCESS_EXISTS) == 0){
	if(do_the_pipe_thang){
	    if(can_access(sig_path, EXECUTE_ACCESS) == 0){
		STORE_S  *store;
		int       flags;
		PIPE_S   *syspipe;
		gf_io_t   pc, gc;
		long      start;

		if((store = so_get(CharStar, NULL, EDIT_ACCESS)) != NULL){

		    flags = PIPE_READ | PIPE_STDERR | PIPE_NOSHELL;

		    start = time(0);

		    if((syspipe = open_system_pipe(sig_path, NULL, NULL, flags, 5,
						  pipe_callback, pipe_report_error)) != NULL){
			unsigned char c;
			char         *error, *q;

			gf_set_so_writec(&pc, store);
			gf_set_readc(&gc, (void *)syspipe, 0, PipeStar, READ_FROM_LOCALE);
			gf_filter_init();

			if((error = gf_pipe(gc, pc)) != NULL){
			    (void)close_system_pipe(&syspipe, NULL, pipe_callback);
			    gf_clear_so_writec(store);
			    so_give(&store);
			    q_status_message1(SM_ORDER | SM_DING, 3, 4,
					      _("Can't get file: %s"), error);
			    return(NULL);
			}

			if(close_system_pipe(&syspipe, NULL, pipe_callback)){
			    long now;

			    now = time(0);
			    q_status_message2(SM_ORDER, 3, 4,
				    _("Error running program \"%s\"%s"),
				    file,
				    (now - start > 4) ? ": timed out" : "");
			}

			gf_clear_so_writec(store);

			/* rewind and count chars */
			so_seek(store, 0L, 0);
			while(so_readc(&c, store) && sigsize < 100000L)
			  sigsize++;

			/* allocate space */
			tmp_sig = fs_get((sigsize + 1) * sizeof(char));
			tmp_sig[0] = '\0';
			q = tmp_sig;

			/* rewind and copy chars, no prenewlines... */
			so_seek(store, 0L, 0);
			cntdown = sigsize;
			while(so_readc(&c, store) && cntdown-- > 0L)
			  *q++ = c;
			
			*q = '\0';
			so_give(&store);
		    }
		    else{
			so_give(&store);
			q_status_message1(SM_ORDER | SM_DING, 3, 4,
				     _("Error running program \"%s\""),
				     file);
		    }
		}
		else
		  q_status_message(SM_ORDER | SM_DING, 3, 4,
			  "Error allocating space for sig or template program");
	    }
	    else
	      q_status_message1(SM_ORDER | SM_DING, 3, 4,
				/* TRANSLATORS: Arg is a program name */
				_("Can't execute \"%s\": Permission denied"),
				sig_path);
	}
	else if((IS_REMOTE(sig_path) &&
		 (tmp_sig = simple_read_remote_file(sig_path, REMOTE_SIG_SUBTYPE))) ||
		(tmp_sig = read_file(sig_path, READ_FROM_LOCALE)))
	  sigsize = strlen(tmp_sig);
	else
	  q_status_message2(SM_ORDER | SM_DING, 3, 4,
			    /* TRANSLATORS: First arg is error description, 2nd is
			       filename */
			    _("Error \"%s\" reading file \"%s\""),
			    error_description(errno), sig_path);
    }

    sig = get_signature_lit(tmp_sig, prenewlines, postnewlines, is_sig, 0);
    if(tmp_sig)
      fs_give((void **)&tmp_sig);

    return(sig);
}



/*----------------------------------------------------------------------
       Partially set up message to forward and pass off to composer/mailer

    Args: pine_state -- The usual pine structure

  Result: outgoing envelope and body created and passed off to composer/mailer

   Create the outgoing envelope for the mail being forwarded, which is 
not much more than filling in the subject, and create the message body
of the outgoing message which requires formatting the header from the
envelope of the original messasge.
  ----------------------------------------------------------------------*/
int
forward(struct pine *ps, ACTION_S *role_arg)
{
    char	  *sig;
    int		   ret, forward_raw_body = 0, rv = 0, i;
    long	   msgno, j, totalmsgs, rflags;
    ENVELOPE	  *env, *outgoing;
    BODY	  *orig_body, *body = NULL;
    REPLY_S        reply;
    void	  *msgtext = NULL;
    gf_io_t	   pc;
    int            impl, template_len = 0;
    PAT_STATE      dummy;
    REDRAFT_POS_S *redraft_pos = NULL;
    ACTION_S      *role = NULL, *nrole;
#if	defined(DOS) && !defined(_WINDOWS)
    char	  *reserve;
#endif

    dprint((4, "\n - forward -\n"));

    memset((void *)&reply, 0, sizeof(reply));
    outgoing              = mail_newenvelope();
    outgoing->message_id  = generate_message_id();

    if(ps_global->full_header == 2
       && F_ON(F_ENABLE_FULL_HDR_AND_TEXT, ps_global))
      forward_raw_body = 1;

    if((totalmsgs = mn_total_cur(ps->msgmap)) > 1L){
	snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%s forwarded messages...", comatose(totalmsgs));
	tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	outgoing->subject = cpystr(tmp_20k_buf);
    }
    else{
	/*---------- Get the envelope of message we're forwarding ------*/
	msgno = mn_m2raw(ps->msgmap, mn_get_cur(ps->msgmap));
	if(!((env = pine_mail_fetchstructure(ps->mail_stream, msgno, NULL))
	     && (outgoing->subject = forward_subject(env, 0)))){
	    q_status_message1(SM_ORDER,3,4,
			  _("Error fetching message %s. Can't forward it."),
			      long2string(msgno));
	    goto clean;
	}
    }

    /*
     * as with all text bound for the composer, build it in 
     * a storage object of the type it understands...
     */
    if((msgtext = (void *)so_get(PicoText, NULL, EDIT_ACCESS)) == NULL){
	q_status_message(SM_ORDER | SM_DING, 3, 4,
			 _("Error allocating message text"));
	goto clean;
    }

    ret = (F_ON(F_FORWARD_AS_ATTACHMENT, ps_global))
	   ? 'y'
	   : (totalmsgs > 1L)
	      ? want_to(_("Forward messages as a MIME digest"), 'y', 'x', NO_HELP, WT_SEQ_SENSITIVE)
	      : (ps->full_header == 2)
		 ? want_to(_("Forward message as an attachment"), 'n', 'x', NO_HELP, WT_SEQ_SENSITIVE)
		 : 0;

    if(ret == 'x'){
	cmd_cancelled("Forward");
	so_give((STORE_S **)&msgtext);
	goto clean;
    }

    /* Setup possible role */
    if(role_arg)
      role = copy_action(role_arg);

    if(!role){
	rflags = ROLE_FORWARD;
	if(nonempty_patterns(rflags, &dummy)){
	    /* setup default role */
	    nrole = NULL;
	    j = mn_first_cur(ps->msgmap);
	    do {
		role = nrole;
		nrole = set_role_from_msg(ps, rflags,
					  mn_m2raw(ps->msgmap, j), NULL);
	    } while(nrole && (!role || nrole == role)
		    && (j=mn_next_cur(ps->msgmap)) > 0L);

	    if(!role || nrole == role)
	      role = nrole;
	    else
	      role = NULL;

	    if(confirm_role(rflags, &role))
	      role = combine_inherited_role(role);
	    else{				/* cancel reply */
		role = NULL;
		cmd_cancelled("Forward");
		so_give((STORE_S **)&msgtext);
		goto clean;
	    }
	}
    }

    if(role)
      q_status_message1(SM_ORDER, 3, 4,
			_("Forwarding using role \"%s\""), role->nick);

    if(role && role->template){
	char *filtered;

	impl = 1;
	filtered = detoken(role, (totalmsgs == 1L) ? env : NULL,
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
     
    if((sig = detoken(role, NULL, 2, 0, 1, &redraft_pos, &impl)) != NULL){
	if(impl == 2)
	  redraft_pos->offset += template_len;

	so_puts((STORE_S *)msgtext, *sig ? sig : NEWLINE);

	fs_give((void **)&sig);
    }
    else
      so_puts((STORE_S *)msgtext, NEWLINE);

    gf_set_so_writec(&pc, (STORE_S *)msgtext);

#if	defined(DOS) && !defined(_WINDOWS)
#if	defined(LWP) || defined(PCTCP) || defined(PCNFS)
#define	IN_RESERVE	8192
#else
#define	IN_RESERVE	16384
#endif
    if((reserve=(char *)malloc(IN_RESERVE)) == NULL){
	gf_clear_so_writec((STORE_S *) msgtext);
	so_give((STORE_S **)&msgtext);
        q_status_message(SM_ORDER | SM_DING, 3, 4,
			 _("Insufficient memory for message text"));
	goto clean;
    }
#endif

    /*
     * If we're forwarding multiple messages *or* the forward-as-mime
     * is turned on and the users wants it done that way, package things
     * up...
     */
    if(ret == 'y'){			/* attach message[s]!!! */
	PART **pp;
	long   totalsize = 0L;

	/*---- New Body to start with ----*/
        body	   = mail_newbody();
        body->type = TYPEMULTIPART;

        /*---- The TEXT part/body ----*/
        body->nested.part                       = mail_newbody_part();
        body->nested.part->body.type            = TYPETEXT;
        body->nested.part->body.contents.text.data = msgtext;

	if(totalmsgs > 1L){
	    /*---- The MULTIPART/DIGEST part ----*/
	    body->nested.part->next		  = mail_newbody_part();
	    body->nested.part->next->body.type	  = TYPEMULTIPART;
	    body->nested.part->next->body.subtype = cpystr("Digest");
	    snprintf(tmp_20k_buf, SIZEOF_20KBUF, "Digest of %s messages", comatose(totalmsgs));
	    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	    body->nested.part->next->body.description = cpystr(tmp_20k_buf);
	    pp = &(body->nested.part->next->body.nested.part);
	}
	else
	  pp = &(body->nested.part->next);

	/*---- The Message body subparts ----*/
	for(msgno = mn_first_cur(ps->msgmap);
	    msgno > 0L;
	    msgno = mn_next_cur(ps->msgmap)){

	    msgno = mn_m2raw(ps->msgmap, msgno);
	    env   = pine_mail_fetchstructure(ps->mail_stream, msgno, NULL);

	    if(forward_mime_msg(ps->mail_stream,msgno,NULL,env,pp,msgtext)){
		totalsize += (*pp)->body.size.bytes;
		pp = &((*pp)->next);
	    }
	    else
	    goto bomb;
	}

	if(totalmsgs > 1L)
	  body->nested.part->next->body.size.bytes = totalsize;
    }
    else if(totalmsgs > 1L){
	int		        warned = 0;
	body                  = mail_newbody();
	body->type            = TYPETEXT;
	body->contents.text.data = msgtext;
	env		      = NULL;

	for(msgno = mn_first_cur(ps->msgmap);
	    msgno > 0L;
	    msgno = mn_next_cur(ps->msgmap)){

	    if(env){			/* put 2 between messages */
		gf_puts(NEWLINE, pc);
		gf_puts(NEWLINE, pc);
	    }

	    /*--- Grab current envelope ---*/
	    env = pine_mail_fetchstructure(ps->mail_stream,
					   mn_m2raw(ps->msgmap, msgno),
					   &orig_body);
	    if(!env || !orig_body){
		q_status_message1(SM_ORDER,3,4,
			   _("Error fetching message %s. Can't forward it."),
			       long2string(msgno));
		goto bomb;
	    }

	    if(orig_body == NULL || orig_body->type == TYPETEXT || forward_raw_body) {
		forward_delimiter(pc);
		reply_forward_header(ps->mail_stream,
				     mn_m2raw(ps->msgmap, msgno),
				     NULL, env, pc, "");

		if(!get_body_part_text(ps->mail_stream, forward_raw_body ? NULL : orig_body,
				       mn_m2raw(ps->msgmap, msgno),
				       forward_raw_body ? NULL : "1", 0L, pc,
				       NULL, NULL, GBPT_NONE))
		  goto bomb;
	    } else if(orig_body->type == TYPEMULTIPART) {
		if(!warned++)
		  q_status_message(SM_ORDER,3,7,
		    _("WARNING!  Attachments not included in multiple forward."));

		if(orig_body->nested.part &&
		   orig_body->nested.part->body.type == TYPETEXT) {
		    /*---- First part of the message is text -----*/
		    forward_delimiter(pc);
		    reply_forward_header(ps->mail_stream,
					 mn_m2raw(ps->msgmap,msgno),
					 NULL, env, pc, "");

		    if(!get_body_part_text(ps->mail_stream,
					   &orig_body->nested.part->body,
					   mn_m2raw(ps->msgmap, msgno),
					   "1", 0L, pc,
					   NULL, NULL, GBPT_NONE))
		      goto bomb;
		} else {
		    q_status_message(SM_ORDER,0,3,
				     _("Multipart with no leading text part!"));
		}
	    } else {
		/*---- Single non-text message of some sort ----*/
		q_status_message(SM_ORDER,0,3,
				 _("Non-text message not included!"));
	    }
	}
    }
    else if(!((env = pine_mail_fetchstructure(ps->mail_stream, msgno,
					      &orig_body))
	      && (body = forward_body(ps->mail_stream, env, orig_body, msgno,
				      NULL, msgtext,
				      FWD_NONE)))){
	q_status_message1(SM_ORDER,3,4,
		      _("Error fetching message %s. Can't forward it."),
			  long2string(msgno));
	goto clean;
    }

    if(ret != 'y' && totalmsgs == 1L && orig_body){
	char *charset;

	charset = parameter_val(orig_body->parameter, "charset");
	if(charset && strucmp(charset, "us-ascii") != 0){
	    CONV_TABLE *ct;

	    /*
	     * There is a non-ascii charset, is there conversion happening?
	     */
	    if(!(ct=conversion_table(charset, ps_global->posting_charmap)) || !ct->table){
		reply.orig_charset = charset;
		charset = NULL;
	    }
	}

	if(charset)
	  fs_give((void **) &charset);

	/*
	 * I don't think orig_charset is ever used except possibly
	 * right here. Hubert 2008-01-15.
	 */
	if(reply.orig_charset)
	  reply.forw = 1;
    }

    /* fill in reply structure */
    reply.forwarded	= 1;
    reply.mailbox	= cpystr(ps->mail_stream->mailbox);
    reply.origmbox	= cpystr(ps->mail_stream->original_mailbox
				    ? ps->mail_stream->original_mailbox
				    : ps->mail_stream->mailbox);
    reply.data.uid.msgs = (imapuid_t *) fs_get((totalmsgs + 1) * sizeof(imapuid_t));
    if((reply.data.uid.validity = ps->mail_stream->uid_validity) != 0){
	reply.uid = 1;
	for(msgno = mn_first_cur(ps->msgmap), i = 0;
	    msgno > 0L;
	    msgno = mn_next_cur(ps->msgmap), i++)
	  reply.data.uid.msgs[i] = mail_uid(ps->mail_stream, mn_m2raw(ps->msgmap, msgno));
    }
    else{
	reply.msgno = 1;
	for(msgno = mn_first_cur(ps->msgmap), i = 0;
	    msgno > 0L;
	    msgno = mn_next_cur(ps->msgmap), i++)
	  reply.data.uid.msgs[i] = mn_m2raw(ps->msgmap, msgno);
    }

    reply.data.uid.msgs[i] = 0;			/* tie off list */

#if	defined(DOS) && !defined(_WINDOWS)
    free((void *)reserve);
#endif
    pine_send(outgoing, &body, "FORWARD MESSAGE",
	      role, NULL, &reply, redraft_pos,
	      NULL, NULL, 0);
    rv++;

  clean:
    if(body)
      pine_free_body(&body);

    if((STORE_S *) msgtext)
      gf_clear_so_writec((STORE_S *) msgtext);

    mail_free_envelope(&outgoing);
    free_redraft_pos(&redraft_pos);
    free_action(&role);

    if(reply.orig_charset)
      fs_give((void **)&reply.orig_charset);

    return rv;

  bomb:
    q_status_message(SM_ORDER | SM_DING, 4, 5,
		   _("Error fetching message contents.  Can't forward message."));
    goto clean;
}


/*----------------------------------------------------------------------
       Partially set up message to forward and pass off to composer/mailer

    Args: pine_state -- The usual pine structure
          message    -- The MESSAGECACHE of entry to reply to 

  Result: outgoing envelope and body created and passed off to composer/mailer

   Create the outgoing envelope for the mail being forwarded, which is 
not much more than filling in the subject, and create the message body
of the outgoing message which requires formatting the header from the
envelope of the original messasge.
  ----------------------------------------------------------------------*/
void
forward_text(struct pine *pine_state, void *text, SourceType source)
{
    ENVELOPE *env;
    BODY     *body;
    gf_io_t   pc, gc;
    STORE_S  *msgtext;
    char     *enc_error, *sig;
    ACTION_S *role = NULL;
    PAT_STATE dummy;
    long      rflags = ROLE_COMPOSE;

    if((msgtext = so_get(PicoText, NULL, EDIT_ACCESS)) != NULL){
	env                   = mail_newenvelope();
	env->message_id       = generate_message_id();
	body                  = mail_newbody();
	body->type            = TYPETEXT;
	body->contents.text.data = (void *) msgtext;

	if(nonempty_patterns(rflags, &dummy)){
	    /*
	     * This is really more like Compose, even though it
	     * is called Forward.
	     */
	    if(confirm_role(rflags, &role))
	      role = combine_inherited_role(role);
	    else{			/* cancel */
		cmd_cancelled("Composition");
		display_message('x');
		mail_free_envelope(&env);
		pine_free_body(&body);
		return;
	    }
	}

	if(role)
	  q_status_message1(SM_ORDER, 3, 4, _("Composing using role \"%s\""),
			    role->nick);

	sig = detoken(role, NULL, 2, 0, 1, NULL, NULL);
	so_puts(msgtext, (sig && *sig) ? sig : NEWLINE);
	so_puts(msgtext, NEWLINE);
	so_puts(msgtext, "----- Included text -----");
	so_puts(msgtext, NEWLINE);
	if(sig)
	  fs_give((void **)&sig);

	gf_filter_init();
	gf_set_so_writec(&pc, msgtext);
	gf_set_readc(&gc,text,(source == CharStar) ? strlen((char *)text) : 0L,
		     source, 0);

	if((enc_error = gf_pipe(gc, pc)) == NULL){
	    pine_send(env, &body, "SEND MESSAGE", role, NULL, NULL, NULL,
		      NULL, NULL, 0);
	    pine_state->mangled_screen = 1;
	}
	else{
	    q_status_message1(SM_ORDER | SM_DING, 3, 5,
			      _("Error reading text \"%s\""),enc_error);
	    display_message('x');
	}

	gf_clear_so_writec(msgtext);
	mail_free_envelope(&env);
	pine_free_body(&body);
    }
    else {
	q_status_message(SM_ORDER | SM_DING, 3, 4,
			 _("Error allocating message text"));
	display_message('x');
    }

    free_action(&role);
}


/*----------------------------------------------------------------------
       Partially set up message to resend and pass off to mailer

    Args: pine_state -- The usual pine structure

  Result: outgoing envelope and body created and passed off to mailer

   Create the outgoing envelope for the mail being resent, which is 
not much more than filling in the subject, and create the message body
of the outgoing message which requires formatting the header from the
envelope of the original messasge.
  ----------------------------------------------------------------------*/
int
bounce(struct pine *pine_state, ACTION_S *role)
{
    ENVELOPE	  *env;
    long           msgno, rawno;
    char          *save_to = NULL, **save_toptr = NULL, *errstr = NULL,
		  *prmpt_who = NULL, *prmpt_cnf = NULL;

    dprint((4, "\n - bounce -\n"));

    if(mn_total_cur(pine_state->msgmap) > 1L){
	save_toptr = &save_to;
	snprintf(tmp_20k_buf, SIZEOF_20KBUF, _("BOUNCE (redirect) %ld messages to : "),
		mn_total_cur(pine_state->msgmap));
	tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	prmpt_who = cpystr(tmp_20k_buf);
	snprintf(tmp_20k_buf, SIZEOF_20KBUF, _("Send %ld messages "),
		mn_total_cur(pine_state->msgmap));
	tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	prmpt_cnf = cpystr(tmp_20k_buf);
    }

    for(msgno = mn_first_cur(pine_state->msgmap);
	msgno > 0L;
	msgno = mn_next_cur(pine_state->msgmap)){

	rawno = mn_m2raw(pine_state->msgmap, msgno);
	if((env = pine_mail_fetchstructure(pine_state->mail_stream, rawno, NULL)) != NULL)
	  errstr = bounce_msg(pine_state->mail_stream, rawno, NULL, role,
			      save_toptr, env->subject, prmpt_who, prmpt_cnf);
	else
	  errstr = _("Can't fetch Subject for Bounce");


	if(errstr){
	    if(*errstr)
	      q_status_message(SM_ORDER | SM_DING, 4, 7, errstr);

	    break;
	}
    }

    if(save_to)
      fs_give((void **)&save_to);

    if(prmpt_who)
      fs_give((void **) &prmpt_who);

    if(prmpt_cnf)
      fs_give((void **) &prmpt_cnf);

    return(errstr ? 0 : 1);
}



char *
bounce_msg(MAILSTREAM *stream,
	   long int rawno,
	   char *part,
	   ACTION_S *role,
	   char **to,
	   char *subject,
	   char *pmt_who,
	   char *pmt_cnf)
{
    char     *errstr = NULL;
    int	      was_seen = -1;
    ENVELOPE *outgoing;
    BODY     *body = NULL;
    MESSAGECACHE *mc;

    if((errstr = bounce_msg_body(stream, rawno, part, to, subject, &outgoing, &body, &was_seen)) == NULL){
	if(pine_simple_send(outgoing, &body, role, pmt_who, pmt_cnf, to,
			    !(to && *to) ? SS_PROMPTFORTO : 0) < 0){
	    errstr = "";		/* p_s_s() better have explained! */
	    /* clear seen flag */
	    if(was_seen == 0 && rawno > 0L
	       && stream && rawno <= stream->nmsgs
	       && (mc = mail_elt(stream,  rawno)) && mc->seen)
	      mail_flag(stream, long2string(rawno), "\\SEEN", 0);
	}
    }

    /* Just for good measure... */
    mail_free_envelope(&outgoing);
    pine_free_body(&body);

    return(errstr);		/* no problem-o */
}


/*----------------------------------------------------------------------
    Serve up the current signature within pico for editing

    Args: file to edit

  Result: signature changed or not.
  ---*/
char *
signature_edit(char *sigfile, char *title)
{
    int	     editor_result;
    char     sig_path[MAXPATH+1], errbuf[2000], *errstr = NULL;
    char    *ret = NULL;
    STORE_S *msgso, *tmpso = NULL;
    gf_io_t  gc, pc;
    PICO     pbf;
    struct variable *vars = ps_global->vars;
    REMDATA_S *rd = NULL;

    if(!signature_path(sigfile, sig_path, MAXPATH))
      return(cpystr(_("No signature file defined.")));
    
    if(IS_REMOTE(sigfile)){
	rd = rd_create_remote(RemImap, sig_path, REMOTE_SIG_SUBTYPE,
			      NULL, "Error: ",
			      _("Can't access remote configuration."));
	if(!rd)
	  return(cpystr(_("Error attempting to edit remote configuration")));
	
	(void)rd_read_metadata(rd);

	if(rd->access == MaybeRorW){
	    if(rd->read_status == 'R')
	      rd->access = ReadOnly;
	    else
	      rd->access = ReadWrite;
	}

	if(rd->access != NoExists){

	    rd_check_remvalid(rd, 1L);

	    /*
	     * If the cached info says it is readonly but
	     * it looks like it's been fixed now, change it to readwrite.
	     */
	    if(rd->read_status == 'R'){
		rd_check_readonly_access(rd);
		if(rd->read_status == 'W'){
		    rd->access = ReadWrite;
		    rd->flags |= REM_OUTOFDATE;
		}
		else
		  rd->access = ReadOnly;
	    }
	}

	if(rd->flags & REM_OUTOFDATE){
	    if(rd_update_local(rd) != 0){

		dprint((1,
		       "signature_edit: rd_update_local failed\n"));
		rd_close_remdata(&rd);
		return(cpystr(_("Can't access remote sig")));
	    }
	}
	else
	  rd_open_remote(rd);

	if(rd->access != ReadWrite || rd_remote_is_readonly(rd)){
	    rd_close_remdata(&rd);
	    return(cpystr(_("Can't get write permission for remote sig")));
	}

	rd->flags |= DO_REMTRIM;

	strncpy(sig_path, rd->lf, sizeof(sig_path)-1);
	sig_path[sizeof(sig_path)-1] = '\0';
    }

    standard_picobuf_setup(&pbf);
    pbf.tty_fix       = PineRaw;
    pbf.composer_help = h_composer_sigedit;
    pbf.exittest      = sigedit_exit_for_pico;
    pbf.upload	       = (VAR_UPLOAD_CMD && VAR_UPLOAD_CMD[0])
			   ? upload_msg_to_pico : NULL;
    pbf.alt_ed        = (VAR_EDITOR && VAR_EDITOR[0] && VAR_EDITOR[0][0])
			    ? VAR_EDITOR : NULL;
    pbf.alt_spell     = (VAR_SPELLER && VAR_SPELLER[0]) ? VAR_SPELLER : NULL;
    pbf.always_spell_check = F_ON(F_ALWAYS_SPELL_CHECK, ps_global);
    pbf.strip_ws_before_send = F_ON(F_STRIP_WS_BEFORE_SEND, ps_global);
    pbf.allow_flowed_text = 0;

    pbf.pine_anchor   = set_titlebar(title,
				      ps_global->mail_stream,
				      ps_global->context_current,
				      ps_global->cur_folder,
				      ps_global->msgmap,
				      0, FolderName, 0, 0, NULL);

    /* NOTE: at this point, alot of pico struct fields are null'd out
     * thanks to the leading memset; in particular "headents" which tells
     * pico to behave like a normal editor (though modified slightly to
     * let the caller dictate the file to edit and such)...
     */

    if(VAR_OPER_DIR && !in_dir(VAR_OPER_DIR, sig_path)){
	size_t l;

	l = strlen(VAR_OPER_DIR) + 100;
	ret = (char *) fs_get((l+1) * sizeof(char));
	snprintf(ret, l+1, _("Can't edit file outside of %s"), VAR_OPER_DIR);
	ret[l] = '\0';
	return(ret);
    }

    /*
     * Now alloc and init the text to pass pico
     */
    if(!(msgso = so_get(PicoText, NULL, EDIT_ACCESS))){
	ret = cpystr(_("Error allocating space for file"));
	dprint((1, "Can't alloc space for signature_edit"));
	return(ret);
    }
    else
      pbf.msgtext = so_text(msgso);

    if(can_access(sig_path, READ_ACCESS) == 0
       && !(tmpso = so_get(FileStar, sig_path, READ_ACCESS|READ_FROM_LOCALE))){
	char *problem = error_description(errno);

	snprintf(errbuf, sizeof(errbuf), _("Error editing \"%s\": %s"),
		sig_path, problem ? problem : "<NULL>");
	errbuf[sizeof(errbuf)-1] = '\0';
	ret = cpystr(errbuf);

	dprint((1, "signature_edit: can't open %s: %s", sig_path,
		   problem ? problem : "<NULL>"));
	return(ret);
    }
    else if(tmpso){			/* else, fill pico's edit buffer */
	gf_set_so_readc(&gc, tmpso);	/* read from file, write pico buf */
	gf_set_so_writec(&pc, msgso);
	gf_filter_init();		/* no filters needed */
	if((errstr = gf_pipe(gc, pc)) != NULL){
	    snprintf(errbuf, sizeof(errbuf), _("Error reading file: \"%s\""), errstr);
	    errbuf[sizeof(errbuf)-1] = '\0';
	    ret = cpystr(errbuf);
	}

	gf_clear_so_readc(tmpso);
	gf_clear_so_writec(msgso);
	so_give(&tmpso);
    }

    if(!errstr){
#ifdef _WINDOWS
	mswin_setwindowmenu (MENU_COMPOSER);
#endif

	/*------ OK, Go edit the signature ------*/
	editor_result = pico(&pbf);

#ifdef _WINDOWS
	mswin_setwindowmenu (MENU_DEFAULT);
#endif
	if(editor_result & COMP_GOTHUP){
	    hup_signal();		/* do what's normal for a hup */
	}
	else{
	    fix_windsize(ps_global);
	    init_signals();
	}

	if(editor_result & (COMP_SUSPEND | COMP_GOTHUP | COMP_CANCEL)){
	}
	else{
            /*------ Must have an edited buffer, write it to .sig -----*/
	    our_unlink(sig_path);	/* blast old copy */
	    if((tmpso = so_get(FileStar, sig_path, WRITE_ACCESS|WRITE_TO_LOCALE)) != NULL){
		so_seek(msgso, 0L, 0);
		gf_set_so_readc(&gc, msgso);	/* read from pico buf */
		gf_set_so_writec(&pc, tmpso);	/* write sig file */
		gf_filter_init();		/* no filters needed */
		if((errstr = gf_pipe(gc, pc)) != NULL){
		    snprintf(errbuf, sizeof(errbuf), _("Error writing file: \"%s\""),
				      errstr);
		    errbuf[sizeof(errbuf)-1] = '\0';
		    ret = cpystr(errbuf);
		}

		gf_clear_so_readc(msgso);
		gf_clear_so_writec(tmpso);
		if(so_give(&tmpso)){
		    errstr = error_description(errno);
		    snprintf(errbuf, sizeof(errbuf), _("Error writing file: \"%s\""),
				      errstr);
		    errbuf[sizeof(errbuf)-1] = '\0';
		    ret = cpystr(errbuf);
		}

		if(IS_REMOTE(sigfile)){
		    int   e, we_cancel;
		    char datebuf[200];

		    datebuf[0] = '\0';

		    we_cancel = busy_cue("Copying to remote sig", NULL, 1);
		    if((e = rd_update_remote(rd, datebuf)) != 0){
			if(e == -1){
			    q_status_message2(SM_ORDER | SM_DING, 3, 5,
			      _("Error opening temporary sig file %s: %s"),
				rd->lf, error_description(errno));
			    dprint((1,
			       "write_remote_sig: error opening temp file %s\n",
			       rd->lf ? rd->lf : "?"));
			}
			else{
			    q_status_message2(SM_ORDER | SM_DING, 3, 5,
					    _("Error copying to %s: %s"),
					    rd->rn, error_description(errno));
			    dprint((1,
			      "write_remote_sig: error copying from %s to %s\n",
			      rd->lf ? rd->lf : "?", rd->rn ? rd->rn : "?"));
			}
			
			q_status_message(SM_ORDER | SM_DING, 5, 5,
	   _("Copy of sig to remote folder failed, changes NOT saved remotely"));
		    }
		    else{
			rd_update_metadata(rd, datebuf);
			rd->read_status = 'W';
		    }

		    rd_close_remdata(&rd);

		    if(we_cancel)
		      cancel_busy_cue(-1);
		}
	    }
	    else{
		snprintf(errbuf, sizeof(errbuf), _("Error writing \"%s\""), sig_path);
		errbuf[sizeof(errbuf)-1] = '\0';
		ret = cpystr(errbuf);
		dprint((1, "signature_edit: can't write %s",
			   sig_path));
	    }
	}
    }
    
    standard_picobuf_teardown(&pbf);
    so_give(&msgso);
    return(ret);
}


/*----------------------------------------------------------------------
    Serve up the current signature within pico for editing

    Args: literal signature to edit

  Result: raw edited signature is returned in result arg
  ---*/
char *
signature_edit_lit(char *litsig, char **result, char *title, HelpType composer_help)
{
    int	     editor_result;
    char    *errstr = NULL;
    char    *ret = NULL;
    STORE_S *msgso;
    PICO     pbf;
    struct variable *vars = ps_global->vars;

    standard_picobuf_setup(&pbf);
    pbf.tty_fix       = PineRaw;
    pbf.search_help   = h_sigedit_search;
    pbf.composer_help = composer_help;
    pbf.exittest      = sigedit_exit_for_pico;
    pbf.upload	       = (VAR_UPLOAD_CMD && VAR_UPLOAD_CMD[0])
			   ? upload_msg_to_pico : NULL;
    pbf.alt_ed        = (VAR_EDITOR && VAR_EDITOR[0] && VAR_EDITOR[0][0])
			    ? VAR_EDITOR : NULL;
    pbf.alt_spell     = (VAR_SPELLER && VAR_SPELLER[0]) ? VAR_SPELLER : NULL;
    pbf.always_spell_check = F_ON(F_ALWAYS_SPELL_CHECK, ps_global);
    pbf.strip_ws_before_send = F_ON(F_STRIP_WS_BEFORE_SEND, ps_global);
    pbf.allow_flowed_text = 0;

    pbf.pine_anchor   = set_titlebar(title,
				      ps_global->mail_stream,
				      ps_global->context_current,
				      ps_global->cur_folder,
				      ps_global->msgmap,
				      0, FolderName, 0, 0, NULL);

    /* NOTE: at this point, alot of pico struct fields are null'd out
     * thanks to the leading memset; in particular "headents" which tells
     * pico to behave like a normal editor (though modified slightly to
     * let the caller dictate the file to edit and such)...
     */

    /*
     * Now alloc and init the text to pass pico
     */
    if(!(msgso = so_get(PicoText, NULL, EDIT_ACCESS))){
	ret = cpystr(_("Error allocating space"));
	dprint((1, "Can't alloc space for signature_edit_lit"));
	return(ret);
    }
    else
      pbf.msgtext = so_text(msgso);
    
    so_puts(msgso, litsig ? litsig : "");


    if(!errstr){
#ifdef _WINDOWS
	mswin_setwindowmenu (MENU_COMPOSER);
#endif

	/*------ OK, Go edit the signature ------*/
	editor_result = pico(&pbf);

#ifdef _WINDOWS
	mswin_setwindowmenu (MENU_DEFAULT);
#endif
	if(editor_result & COMP_GOTHUP){
	    hup_signal();		/* do what's normal for a hup */
	}
	else{
	    fix_windsize(ps_global);
	    init_signals();
	}

	if(editor_result & (COMP_SUSPEND | COMP_GOTHUP | COMP_CANCEL)){
	    ret = cpystr(_("Edit Cancelled"));
	}
	else{
            /*------ Must have an edited buffer, write it to .sig -----*/
	    unsigned char c;
	    int cnt = 0;
	    char *p;

	    so_seek(msgso, 0L, 0);
	    while(so_readc(&c, msgso))
	      cnt++;
	    
	    *result = (char *)fs_get((cnt+1) * sizeof(char));
	    p = *result;
	    so_seek(msgso, 0L, 0);
	    while(so_readc(&c, msgso))
	      *p++ = c;
	    
	    *p = '\0';
	}
    }
    
    standard_picobuf_teardown(&pbf);
    so_give(&msgso);
    return(ret);
}


/*
 * Returns  0  for Save Changes and exit
 *          1  for Cancel Exit
 *         -1  exit but Dont Save Changes
 */
int
sigedit_exit_for_pico(struct headerentry *he, void (*redraw_pico)(void), int allow_flowed,
		      char **result)
{
    int	      rv;
    char     *rstr = NULL;
    void    (*redraw)(void) = ps_global->redrawer;
    static ESCKEY_S opts[] = {
	{'s', 's', "S", N_("Save changes")},
	{'d', 'd', "D", N_("Don't save changes")},
	{-1, 0, NULL, NULL}
    };

    ps_global->redrawer = redraw_pico;
    fix_windsize(ps_global);

    while(1){
	rv = radio_buttons(_("Exit editor? "),
			   -FOOTER_ROWS(ps_global), opts,
			   's', 'x', h_exit_editor, RB_NORM);
	if(rv == 's'){				/* user ACCEPTS! */
	    break;
	}
	else if(rv == 'd'){			/* Declined! */
	    rstr = _("No Changes Saved");
	    break;
	}
	else if(rv == 'x'){			/* Cancelled! */
	    rstr = _("Exit Cancelled");
	    break;
	}
    }

    if(result)
      *result = rstr;

    ps_global->redrawer = redraw;
    return((rv == 's') ? 0 : (rv == 'd') ? -1 : 1);
}


/*
 * Common stuff we almost always want to set when calling pico.
 */
void
standard_picobuf_setup(PICO *pbf)
{
    memset(pbf, 0, sizeof(*pbf));

    pbf->pine_version	= ALPINE_VERSION;
    pbf->fillcolumn	= ps_global->composer_fillcol;
    pbf->menu_rows	= FOOTER_ROWS(ps_global) - 1;
    pbf->colors		= colors_for_pico();
    pbf->wordseps	= user_wordseps(ps_global->VAR_WORDSEPS);
    pbf->helper		= helper;
    pbf->showmsg	= display_message_for_pico;
    pbf->suspend	= do_suspend;
    pbf->keybinput	= cmd_input_for_pico;
    pbf->tty_fix	= ttyfix;		/* watch out for this one */
    pbf->newmail	= new_mail_for_pico;
    pbf->ckptdir	= checkpoint_dir_for_pico;
    pbf->resize		= resize_for_pico;
    pbf->input_cs	= ps_global->input_cs;
    pbf->winch_cleanup	= winch_cleanup;
    pbf->search_help	= h_composer_search;
    pbf->ins_help	= h_composer_ins;
    pbf->ins_m_help	= h_composer_ins_m;
    pbf->composer_help	= h_composer;
    pbf->browse_help	= h_composer_browse;
    pbf->attach_help	= h_composer_ctrl_j;

    pbf->pine_flags =
       ( (F_ON(F_CAN_SUSPEND,ps_global)			? P_SUSPEND	: 0L)
       | (F_ON(F_USE_FK,ps_global)			? P_FKEYS	: 0L)
       | (ps_global->restricted				? P_SECURE	: 0L)
       | (F_ON(F_ALT_ED_NOW,ps_global)			? P_ALTNOW	: 0L)
       | (F_ON(F_USE_CURRENT_DIR,ps_global)		? P_CURDIR	: 0L)
       | (F_ON(F_SUSPEND_SPAWNS,ps_global)		? P_SUBSHELL	: 0L)
       | (F_ON(F_COMPOSE_MAPS_DEL,ps_global)		? P_DELRUBS	: 0L)
       | (F_ON(F_ENABLE_TAB_COMPLETE,ps_global)		? P_COMPLETE	: 0L)
       | (F_ON(F_SHOW_CURSOR,ps_global)			? P_SHOCUR	: 0L)
       | (F_ON(F_DEL_FROM_DOT,ps_global)		? P_DOTKILL	: 0L)
       | (F_ON(F_ENABLE_DOT_FILES,ps_global)		? P_DOTFILES	: 0L)
       | (F_ON(F_ALLOW_GOTO,ps_global)			? P_ALLOW_GOTO	: 0L)
       | (F_ON(F_ENABLE_SEARCH_AND_REPL,ps_global)	? P_REPLACE	: 0L)
       | (!ps_global->pass_ctrl_chars
          && !ps_global->pass_c1_ctrl_chars		? P_HICTRL	: 0L)
       | ((F_ON(F_ENABLE_ALT_ED,ps_global)
           || F_ON(F_ALT_ED_NOW,ps_global)
	   || (ps_global->VAR_EDITOR
	       && ps_global->VAR_EDITOR[0]
	       && ps_global->VAR_EDITOR[0][0]))
							? P_ADVANCED	: 0L)
       | ((!ps_global->keyboard_charmap
           || !strucmp(ps_global->keyboard_charmap, "US-ASCII"))
							? P_HIBITIGN	: 0L));

    if(ps_global->VAR_OPER_DIR){
	pbf->oper_dir    = ps_global->VAR_OPER_DIR;
	pbf->pine_flags |= P_TREE;
    }

    pbf->home_dir = ps_global->home_dir;
}


void
standard_picobuf_teardown(PICO *pbf)
{
    if(pbf){
	if(pbf->colors)
	  free_pcolors(&pbf->colors);

	if(pbf->wordseps)
	  fs_give((void **) &pbf->wordseps);
    }
}


/*----------------------------------------------------------------------
    Call back for pico to use to check for new mail.
     
Args: cursor -- pointer to in to tell caller if cursor location changed
                if NULL, turn off cursor positioning.
      timing -- whether or not it's a good time to check 
 

Returns: returns 1 on success, zero on error.
----*/      
long
new_mail_for_pico(int timing, int status)
{
    /*
     * If we're not interested in the status, don't display the busy
     * cue either...
     */
    /* don't know where the cursor's been, reset it */
    clear_cursor_pos();
    return(new_mail(0, timing,
		    (status ? NM_STATUS_MSG : NM_NONE) | NM_DEFER_SORT
						       | NM_FROM_COMPOSER));
}


void
cmd_input_for_pico(void)
{
    zero_new_mail_count();
}


/*----------------------------------------------------------------------
    Call back for pico to get newmail status messages displayed

Args: x -- char processed

Returns: 
----*/      
int
display_message_for_pico(int x)
{
    int rv;
    
    clear_cursor_pos();			/* can't know where cursor is */
    mark_status_dirty();		/* don't count on cached text */
    fix_windsize(ps_global);
    init_sigwinch();
    display_message(x);
    rv = ps_global->mangled_screen;
    ps_global->mangled_screen = 0;
    return(rv);
}


/*----------------------------------------------------------------------
    Call back for pico to get desired directory for its check point file
     
  Args: s -- buffer to write directory name
	n -- length of that buffer

  Returns: pointer to static buffer
----*/      
char *
checkpoint_dir_for_pico(char *s, size_t n)
{
#if defined(DOS) || defined(OS2)
    /*
     * we can't assume anything about root or home dirs, so
     * just plunk it down in the same place as the pinerc
     */
    if(!getenv("HOME")){
	char *lc = last_cmpnt(ps_global->pinerc);

	if(lc != NULL){
	    strncpy(s, ps_global->pinerc, MIN(n-1,lc-ps_global->pinerc));
	    s[MIN(n-1,lc-ps_global->pinerc)] = '\0';
	}
	else{
	    strncpy(s, ".\\", n-1);
	    s[n-1] = '\0';
	}
    }
    else
#endif
    strncpy(s, ps_global->home_dir, n-1);
    s[n-1] = '\0';

    return(s);
}


/*----------------------------------------------------------------------
  Call back for pico to tell us the window size's changed

  Args: none

  Returns: none (but pine's ttyo structure may have been updated)
----*/
void
resize_for_pico(void)
{
    fix_windsize(ps_global);
}


PCOLORS *
colors_for_pico(void)
{
    PCOLORS *colors = NULL;
    struct variable *vars = ps_global->vars;

    if (pico_usingcolor()){
      colors = (PCOLORS *)fs_get(sizeof(PCOLORS));

      colors->tbcp = current_titlebar_color();

      if (VAR_KEYLABEL_FORE_COLOR && VAR_KEYLABEL_BACK_COLOR){
	colors->klcp = new_color_pair(VAR_KEYLABEL_FORE_COLOR,
				      VAR_KEYLABEL_BACK_COLOR);
	if (!pico_is_good_colorpair(colors->klcp))
	  free_color_pair(&colors->klcp);
      }
      else colors->klcp = NULL;

      if (colors->klcp && VAR_KEYNAME_FORE_COLOR && VAR_KEYNAME_BACK_COLOR){
	colors->kncp = new_color_pair(VAR_KEYNAME_FORE_COLOR, 
				      VAR_KEYNAME_BACK_COLOR);
      }
      else colors->kncp = NULL;

      if (VAR_STATUS_FORE_COLOR && VAR_STATUS_BACK_COLOR){
	colors->stcp = new_color_pair(VAR_STATUS_FORE_COLOR, 
				      VAR_STATUS_BACK_COLOR);
      }
      else colors->stcp = NULL;

      if (VAR_PROMPT_FORE_COLOR && VAR_PROMPT_BACK_COLOR){
	colors->prcp = new_color_pair(VAR_PROMPT_FORE_COLOR, 
				      VAR_PROMPT_BACK_COLOR);
      }
      else colors->prcp = NULL;
    }
    
    return colors;
}


void
free_pcolors(PCOLORS **colors)
{
    if (*colors){
	  if ((*colors)->tbcp)
		free_color_pair(&(*colors)->tbcp);
	  if ((*colors)->kncp)
		free_color_pair(&(*colors)->kncp);
	  if ((*colors)->klcp)
		free_color_pair(&(*colors)->klcp);
	  if ((*colors)->stcp)
		free_color_pair(&(*colors)->stcp);
	  if ((*colors)->prcp)
		free_color_pair(&(*colors)->prcp);
	  fs_give((void **)colors);
	  fs_give((void **)colors);
	  *colors = NULL;
	}
}
