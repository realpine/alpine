#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: send.c 1142 2008-08-13 17:22:21Z hubert@u.washington.edu $";
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
       Functions for composing and sending mail

 ====*/


#include "headers.h"
#include "send.h"
#include "status.h"
#include "mailview.h"
#include "mailindx.h"
#include "dispfilt.h"
#include "keymenu.h"
#include "folder.h"
#include "radio.h"
#include "addrbook.h"
#include "reply.h"
#include "titlebar.h"
#include "signal.h"
#include "mailcmd.h"
#include "roleconf.h"
#include "adrbkcmd.h"
#include "busy.h"
#include "../pith/debug.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/flag.h"
#include "../pith/bldaddr.h"
#include "../pith/copyaddr.h"
#include "../pith/detach.h"
#include "../pith/mimedesc.h"
#include "../pith/pipe.h"
#include "../pith/addrstring.h"
#include "../pith/news.h"
#include "../pith/detoken.h"
#include "../pith/util.h"
#include "../pith/init.h"
#include "../pith/mailcmd.h"
#include "../pith/ablookup.h"
#include "../pith/reply.h"
#include "../pith/hist.h"
#include "../pith/list.h"
#include "../pith/icache.h"
#include "../pith/busy.h"
#include "../pith/mimetype.h"
#include "../pith/send.h"
#include "../pith/smime.h"


typedef struct body_particulars {
    unsigned short     type, encoding, had_csp;
    char              *subtype, *charset;
    PARAMETER         *parameter;
} BODY_PARTICULARS_S;

#define	PHONE_HOME_VERSION	"-count"
#define	PHONE_HOME_HOST		"docserver.cac.washington.edu"

/*
 * macro to bind pico's headerentry pointer to PINEFIELD "extdata" hook
 */
#define	HE(PF)	((struct headerentry *)((PF)->extdata))


/*
 * Internal Prototypes
 */
int	   redraft(MAILSTREAM **, ENVELOPE **, BODY **, char **, char **, REPLY_S **,
		   REDRAFT_POS_S **, PINEFIELD **, ACTION_S **, int);
int	   redraft_prompt(char *, char *, int);
int	   check_for_subject(METAENV *);
int	   check_for_fcc(char *);
void	   free_prompts(PINEFIELD *);
int	   postpone_prompt(void);
METAENV	  *pine_simple_send_header(ENVELOPE *, char **, char ***);
void	   call_mailer_file_result(char *, int);
void	   mark_address_failure_for_pico(METAENV *);
BODY_PARTICULARS_S
	  *save_body_particulars(BODY *);
void       reset_body_particulars(BODY_PARTICULARS_S *, BODY *);
void       free_body_particulars(BODY_PARTICULARS_S *);
long	   message_format_for_pico(long, int (*)(int));
int	   send_exit_for_pico(struct headerentry *, void (*)(void), int, char **);
char      *choose_a_priority(char *);
int        dont_flow_this_time(void);
int	   mime_type_for_pico(char *);
char      *cancel_for_pico(void (*)(void));
int	   filter_message_text(char *, ENVELOPE *, BODY *, STORE_S **, METAENV *);
void	   pine_send_newsgroup_name(char *, char*, size_t);
void       outgoing2strings(METAENV *, BODY *, void **, PATMT **, int);
void       strings2outgoing(METAENV *, BODY **, PATMT *, int);
void	   create_message_body_text(BODY *, int);
void	   set_body_size(BODY *);
int        view_as_rich(char *, int);
int	   background_posting(int);
int	   valid_subject(char *, char **, char **,BUILDER_ARG *,int *);
int	   build_addr_lcc(char *, char **, char **, BUILDER_ARG *, int *);
int	   news_build(char *, char **, char **, BUILDER_ARG *, int *);
void	   news_build_busy(void);
#if defined(DOS) || defined(OS2)
int	   dos_valid_from(void);
#endif /* defined(DOS) || defined(OS2) */


/*
 * Pointer to buffer to hold pointers into pine data that's needed by pico. 
 */
static	PICO	  *pbf;


static char	  *g_rolenick = NULL;


static char	  *sending_filter_requested;
static char	   background_requested, flowing_requested;
static unsigned	   call_mailer_flags;
static char	  *priority_requested;

/* local global to save busy_cue state */
static int	   news_busy_cue = 0;


/*
 * Various useful strings
 */
#define	INTRPT_PMT \
	    _("Continue INTERRUPTED composition (answering \"n\" won't erase it)")
#define	PSTPND_PMT \
	    _("Continue postponed composition (answering \"No\" won't erase it)")
#define	FORM_PMT \
	    _("Start composition from Form Letter Folder")
#define	PSTPN_FORM_PMT	\
	    _("Save to Postponed or Form letter folder? ")
#define	POST_PMT   \
	    _("Posted message may go to thousands of readers. Really post")
#define	INTR_DEL_PMT \
	    _("Deleted messages will be removed from folder after use.  Proceed")


/*
 * Macros to help sort out posting results
 */
#define	P_MAIL_WIN	0x01
#define	P_MAIL_LOSE	0x02
#define	P_MAIL_BITS	0x03
#define	P_NEWS_WIN	0x04
#define	P_NEWS_LOSE	0x08
#define	P_NEWS_BITS	0x0C
#define	P_FCC_WIN	0x10
#define	P_FCC_LOSE	0x20
#define	P_FCC_BITS	0x30


#define COMPOSE_MAIL_TITLE "COMPOSE MESSAGE"


/*
 * For check_for_subject and check_for_fcc
 */
#define CF_OK		0x1
#define CF_MISSING	0x2


/*----------------------------------------------------------------------
    Compose screen (not forward or reply). Set up envelope, call composer
  
   Args: pine_state -- The usual pine structure
 
  Little front end for the compose screen
  ---*/
void
compose_screen(struct pine *pine_state)
{
    void (*prev_screen)(struct pine *) = pine_state->prev_screen,
	 (*redraw)(void) = pine_state->redrawer;

    pine_state->redrawer = NULL;
    ps_global->next_screen = SCREEN_FUN_NULL;
    mailcap_free(); /* free resources we won't be using for a while */
    compose_mail(NULL, NULL, NULL, NULL, NULL);
    pine_state->next_screen = prev_screen;
    pine_state->redrawer = redraw;
}


/*----------------------------------------------------------------------
    Alternate compose screen. Set up role and call regular compose.
  
   Args: pine_state -- The usual pine structure
  ---*/
void
alt_compose_screen(struct pine *pine_state)
{
    ACTION_S *role = NULL;
    void (*prev_screen)(struct pine *) = pine_state->prev_screen,
	 (*redraw)(void) = pine_state->redrawer;

    pine_state->redrawer = NULL;
    ps_global->next_screen = SCREEN_FUN_NULL;
    mailcap_free(); /* free resources we won't be using for a while */

    /* Setup role */
    if(role_select_screen(pine_state, &role, MC_COMPOSE) < 0){
	cmd_cancelled("Composition");
	pine_state->next_screen = prev_screen;
	pine_state->redrawer = redraw;
	return;
    }

    /*
     * If default role was selected (NULL) we need to make up a role which
     * won't do anything, but will cause compose_mail to think there's
     * already a role so that it won't try to confirm the default.
     */
    if(role)
      role = combine_inherited_role(role);
    else{
	role = (ACTION_S *)fs_get(sizeof(*role));
	memset((void *)role, 0, sizeof(*role));
	role->nick = cpystr("Default Role");
    }

    pine_state->redrawer = NULL;
    compose_mail(NULL, NULL, role, NULL, NULL);
    free_action(&role);
    pine_state->next_screen = prev_screen;
    pine_state->redrawer = redraw;
}


/*----------------------------------------------------------------------
     Format envelope for outgoing message and call editor

 Args:  given_to -- An address to send mail to (usually from command line 
                       invocation)
        fcc_arg  -- The fcc that goes with this address.
 
 If a "To" line is given format that into the envelope and get ready to call
           the composer
 If there's a message postponed, offer to continue it, and set it up,
 otherwise just fill in the outgoing envelope as blank.

 NOTE: we ignore postponed and interrupted messages in nr mode
 ----*/
void 
compose_mail(char *given_to, char *fcc_arg, ACTION_S *role_arg,
	     PATMT *attach, gf_io_t inc_text_getc)
{
    BODY	  *body = NULL;
    ENVELOPE	  *outgoing = NULL;
    PINEFIELD	  *custom   = NULL;
    REPLY_S	  *reply	= NULL;
    REDRAFT_POS_S *redraft_pos = NULL;
    ACTION_S      *role = NULL;
    MAILSTREAM	  *stream;
    char	  *fcc_to_free,
		  *fcc      = NULL,
		  *lcc      = NULL,
		  *sig      = NULL;
    int		   fcc_is_sticky = 0,
		   to_is_sticky = 0,
                   intrptd = 0,
                   postponed = 0,
		   form = 0;

    dprint((1,
                 "\n\n    ---- COMPOSE SCREEN (not in pico yet) ----\n"));

    /*-- Check for INTERRUPTED mail  --*/
    if(!role_arg && !(given_to || attach)){
	char	     file_path[MAXPATH+1];

	/* build filename and see if it exists.  build_path creates
	 * an explicit local path name, so all c-client access is thru
	 * local drivers.
	 */
	file_path[0] = '\0';
	build_path(file_path,
		   ps_global->VAR_OPER_DIR ? ps_global->VAR_OPER_DIR
					   : ps_global->home_dir,
		   INTERRUPTED_MAIL, sizeof(file_path));

	/* check to see if the folder exists, the user wants to continue
	 * and that we can actually read something in...
	 */
	if(folder_exists(NULL, file_path) & FEX_ISFILE)
	  intrptd = 1;
    }

    /*-- Check for postponed mail --*/
    if(!role_arg
       && !outgoing				/* not replying/forwarding */
       && !(given_to || attach)			/* not command line send */
       && ps_global->VAR_POSTPONED_FOLDER	/* folder to look in */
       && ps_global->VAR_POSTPONED_FOLDER[0])
      postponed = 1;

    /*-- Check for form letter folder --*/
    if(!role_arg
       && !outgoing				/* not replying/forwarding */
       && !(given_to || attach)			/* not command line send */
       && ps_global->VAR_FORM_FOLDER		/* folder to look in */
       && ps_global->VAR_FORM_FOLDER[0])
      form = 1;

    if(!outgoing && !(given_to || attach)
       && !role_arg && F_ON(F_ALT_COMPOSE_MENU, ps_global)){
        char     prompt[80];
 	char     letters[30];
	char     chosen_task;
 	char    *new      = "New";
 	char    *intrpt   = "Interrupted";
 	char    *postpnd  = "Postponed";
 	char    *formltr  = "FormLetter";
 	char    *roles    = "setRole";
	HelpType help     = h_composer_browse;
	ESCKEY_S compose_style[6];
	unsigned which_help;
	int      ekey_num;
	
	ekey_num = 0;
	compose_style[ekey_num].ch      = 'n';
 	compose_style[ekey_num].rval    = 'n';
 	compose_style[ekey_num].name    = "N";
 	compose_style[ekey_num++].label = new;

	if(intrptd){
 	    compose_style[ekey_num].ch      = 'i';
 	    compose_style[ekey_num].rval    = 'i';
 	    compose_style[ekey_num].name    = "I";
 	    compose_style[ekey_num++].label = intrpt;
 	}
 
 	if(postponed){
 	    compose_style[ekey_num].ch      = 'p';
 	    compose_style[ekey_num].rval    = 'p';
 	    compose_style[ekey_num].name    = "P";
 	    compose_style[ekey_num++].label = postpnd;
 	}
 
 	if(form){
 	    compose_style[ekey_num].ch      = 'f';
 	    compose_style[ekey_num].rval    = 'f';
 	    compose_style[ekey_num].name    = "F";
 	    compose_style[ekey_num++].label = formltr;
 	}

	compose_style[ekey_num].ch      = 'r';
	compose_style[ekey_num].rval    = 'r';
	compose_style[ekey_num].name    = "R";
	compose_style[ekey_num++].label = roles;

 	compose_style[ekey_num].ch    = -1;

 	if(F_ON(F_BLANK_KEYMENU,ps_global)){
 	    char *p;
 
 	    p = letters;
 	    *p = '\0';
 	    for(ekey_num = 0; compose_style[ekey_num].ch != -1; ekey_num++){
		if(p - letters < sizeof(letters))
 		  *p++ = (char) compose_style[ekey_num].ch;
		  
 		if(compose_style[ekey_num + 1].ch != -1 && p - letters < sizeof(letters))
 		  *p++ = ',';
 	    }
 
	    if(p - letters < sizeof(letters))
 	      *p = '\0';
 	}
 
	which_help = intrptd + 2 * postponed + 4 * form;
	switch(which_help){
	  case 1:
	    help = h_compose_intrptd;
	    break;
	  case 2:
	    help = h_compose_postponed;
	    break;
	  case 3:
	    help = h_compose_intrptd_postponed;
	    break;
	  case 4:
	    help = h_compose_form;
	    break;
	  case 5:
	    help = h_compose_intrptd_form;
	    break;
	  case 6:
	    help = h_compose_postponed_form;
	    break;
	  case 7:
	    help = h_compose_intrptd_postponed_form;
	    break;
	  default:
	    help = h_compose_default;
	    break;
	}

 	snprintf(prompt, sizeof(prompt),
 		"Choose a compose method from %s : ",
 		F_ON(F_BLANK_KEYMENU,ps_global) ? letters : "the menu below");
	prompt[sizeof(prompt)-1] = '\0';

 	chosen_task = radio_buttons(prompt, -FOOTER_ROWS(ps_global),
 				    compose_style, 'n', 'x', help, RB_NORM);
	intrptd = postponed = form = 0;

	switch(chosen_task){
	  case 'i':
	    intrptd = 1;
	    break;
	  case 'p':
	    postponed = 1;
	    break;
	  case 'r':
	    { 
	      void (*prev_screen)(struct pine *) = ps_global->prev_screen,
		   (*redraw)(void) = ps_global->redrawer;

	      ps_global->redrawer = NULL;
	      ps_global->next_screen = SCREEN_FUN_NULL;	      
	      if(role_select_screen(ps_global, &role, MC_COMPOSE) < 0){
		cmd_cancelled("Composition");
		ps_global->next_screen = prev_screen;
		ps_global->redrawer = redraw;
		return;
	      }

	      ps_global->next_screen = prev_screen;
	      ps_global->redrawer = redraw;
	      if(role)
		role = combine_inherited_role(role);
	    }
	    break;
	  
	  case 'f':
	    form = 1;
	    break;

	  case 'x':
	    q_status_message(SM_ORDER, 0, 3,
			     "Composition cancelled");
	    return;
	    break;

	  default:
	    break;
	}
    }

    if(intrptd && !outgoing){
	char	     file_path[MAXPATH+1];
	int	     ret = 'n';

	file_path[0] = '\0';
	build_path(file_path,
		   ps_global->VAR_OPER_DIR ? ps_global->VAR_OPER_DIR
					   : ps_global->home_dir,
		   INTERRUPTED_MAIL, sizeof(file_path));
	if(folder_exists(NULL, file_path) & FEX_ISFILE){
	    if((stream = pine_mail_open(NULL, file_path,
					SP_USEPOOL|SP_TEMPUSE, NULL))
	       && !stream->halfopen){

		if(F_ON(F_ALT_COMPOSE_MENU, ps_global) || 
		   (ret = redraft_prompt("Interrupted",INTRPT_PMT,'n')) =='y'){
		    if(!redraft(&stream, &outgoing, &body, &fcc, &lcc, &reply,
				&redraft_pos, &custom, &role, REDRAFT_DEL)){
			if(stream)
			  pine_mail_close(stream);

			return;
		    }

		    to_is_sticky++;

		    /* redraft() may or may not have closed stream */
		    if(stream)
		      pine_mail_close(stream);

		    postponed = form = 0;
		}
		else{
		    pine_mail_close(stream);
		    if(ret == 'x'){
			q_status_message(SM_ORDER, 0, 3,
					 _("Composition cancelled"));
			return;
		    }
		}
	    }
	    else{
		q_status_message1(SM_ORDER | SM_DING, 3, 3,
				  _("Can't open Interrupted mailbox: %s"),
				  file_path);
		if(stream)
		  pine_mail_close(stream);
	    }
	}
    }

    if(postponed && !outgoing){
	int ret = 'n', done = 0;
	int exists;

	if((exists=postponed_stream(&stream,
				    ps_global->VAR_POSTPONED_FOLDER,
				    "Postponed", 0)) & FEX_ISFILE){
	    if(F_ON(F_ALT_COMPOSE_MENU, ps_global) || 
	       (ret = redraft_prompt("Postponed",PSTPND_PMT,'n')) == 'y'){
		if(!redraft(&stream, &outgoing, &body, &fcc, &lcc, &reply,
			    &redraft_pos, &custom, &role,
			    REDRAFT_DEL | REDRAFT_PPND))
		  done++;

		/* stream may or may not be closed in redraft() */
		if(stream && (stream != ps_global->mail_stream))
		  pine_mail_close(stream);

		to_is_sticky++;
		intrptd = form = 0;
	    }
	    else{
		if(stream != ps_global->mail_stream)
		  pine_mail_close(stream);

		if(ret == 'x'){
		    q_status_message(SM_ORDER, 0, 3,
				     _("Composition cancelled"));
		    done++;
		}
	    }
	}
	else if(F_ON(F_ALT_COMPOSE_MENU, ps_global))
	  done++;

	if(done)
	  return;
    }

    if(form && !outgoing){
	int ret = 'n', done = 0;
	int exists;

	if((exists=postponed_stream(&stream,
				    ps_global->VAR_FORM_FOLDER,
				    "Form letter", 1)) & FEX_ISFILE){
	    if(F_ON(F_ALT_COMPOSE_MENU, ps_global) ||
	       (ret = want_to(FORM_PMT,'y','x',NO_HELP,WT_NORM))=='y'){
		if(!redraft(&stream, &outgoing, &body, &fcc, &lcc, &reply,
			    &redraft_pos, &custom, NULL, REDRAFT_NONE))
		    done++;

		/* stream may or may not be closed in redraft() */
		if(stream && (stream != ps_global->mail_stream))
		  pine_mail_close(stream);

		to_is_sticky++;
		intrptd = postponed = 0;
	    }
	    else{
		if(stream != ps_global->mail_stream)
		  pine_mail_close(stream);

		if(ret == 'x'){
		    q_status_message(SM_ORDER, 0, 3,
				     _("Composition cancelled"));
		    done++;
		}
	    }
	}
	else{
	  if(F_ON(F_ALT_COMPOSE_MENU, ps_global)){
	    q_status_message(SM_ORDER | SM_DING, 3, 3,
			      _("Form letter folder doesn't exist!"));
	    return;
	  }
	}

	if(done)
	  return;
    }

    /*-- normal composition --*/
    if(!outgoing){
	int impl, template_len = 0;
	long rflags = ROLE_COMPOSE;
	PAT_STATE dummy;

        /*=================  Compose new message ===============*/
        body         = mail_newbody();
        outgoing     = mail_newenvelope();

        if(given_to)
	  rfc822_parse_adrlist(&outgoing->to, given_to, ps_global->maildomain);

        outgoing->message_id = generate_message_id();

	/*
	 * Setup possible role
	 */
	if(role_arg)
	  role = copy_action(role_arg);

	if(!role){
	    /* Setup possible compose role */
	    if(nonempty_patterns(rflags, &dummy)){
		/*
		 * setup default role
		 * Msgno = -1 means there is no msg.
		 * This will match roles which have the Compose Use turned
		 * on, and have no patterns set, and match the Current
		 * Folder Type.
		 */
		role = set_role_from_msg(ps_global, rflags, -1L, NULL);

		if(confirm_role(rflags, &role))
		  role = combine_inherited_role(role);
		else{				/* cancel reply */
		    role = NULL;
		    cmd_cancelled("Composition");
		    return;
		}
	    }
	}

	if(role)
	  q_status_message1(SM_ORDER, 3, 4, _("Composing using role \"%s\""),
			    role->nick);

	/*
	 * The type of storage object allocated below is vitally
	 * important.  See SIMPLIFYING ASSUMPTION #37
	 */
	if((body->contents.text.data = (void *) so_get(PicoText,
						      NULL, EDIT_ACCESS)) != NULL){
	    char ch;

	    if(inc_text_getc){
		while((*inc_text_getc)(&ch))
		  if(!so_writec(ch, (STORE_S *)body->contents.text.data)){
		      break;
		  }
	    }
	}
	else{
	    q_status_message(SM_ORDER | SM_DING, 3, 4,
			     _("Problem creating space for message text."));
	    return;
	}

	if(role && role->template){
	    char *filtered;

	    impl = 1;	/* leave cursor in header if not explicit */
	    filtered = detoken(role, NULL, 0, 0, 0, &redraft_pos, &impl);
	    if(filtered){
		if(*filtered){
		    so_puts((STORE_S *)body->contents.text.data, filtered);
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

	    if(*sig)
	      so_puts((STORE_S *)body->contents.text.data, sig);

	    fs_give((void **)&sig);
	}

	body->type = TYPETEXT;

	if(attach)
	  create_message_body(&body, attach, 0);
    }

    ps_global->prev_screen = compose_screen;
    if(!(fcc_to_free = fcc) && !(role && role->fcc))
      fcc = fcc_arg;			/* Didn't pick up fcc, use given  */

    /*
     * check whether a build_address-produced fcc is different from
     * fcc.  If same, do nothing, if different, set sticky bit in pine_send.
     */
    if(fcc){
	char *tmp_fcc = NULL;
  
	if(outgoing->to){
  	    tmp_fcc = get_fcc_based_on_to(outgoing->to);
  	    if(strcmp(fcc, tmp_fcc ? tmp_fcc : ""))
  	      fcc_is_sticky++;  /* cause sticky bit to get set */
  
	}
	else if((tmp_fcc = get_fcc(NULL)) != NULL &&
	        !strcmp(fcc, tmp_fcc)){
	    /* not sticky */
  	}
  	else
  	  fcc_is_sticky++;

	if(tmp_fcc)
	  fs_give((void **)&tmp_fcc);
    }

    pine_send(outgoing, &body, COMPOSE_MAIL_TITLE, role, fcc,
	      reply, redraft_pos, lcc, custom,
	      (fcc_is_sticky ? PS_STICKY_FCC : 0) | (to_is_sticky ? PS_STICKY_TO : 0));

    if(reply){
	if(reply->mailbox)
	  fs_give((void **) &reply->mailbox);
	if(reply->origmbox)
	  fs_give((void **) &reply->origmbox);
	if(reply->prefix)
	  fs_give((void **) &reply->prefix);
	if(reply->data.uid.msgs)
	  fs_give((void **) &reply->data.uid.msgs);
  	fs_give((void **) &reply);
    }

    if(fcc_to_free)
      fs_give((void **)&fcc_to_free);

    if(lcc)
      fs_give((void **)&lcc);

    mail_free_envelope(&outgoing);
    pine_free_body(&body);
    free_redraft_pos(&redraft_pos);
    free_action(&role);
}


/*----------------------------------------------------------------------
 Args:	stream -- This is where we get the postponed messages from
		  We'll expunge and close it here unless it is mail_stream.

    These are all return values:
    ================
	outgoing -- 
	body -- 
	fcc -- 
	lcc -- 
	reply -- 
	redraft_pos -- 
	custom -- 
	role -- 
    ================

	flags --
 
 ----*/
int
redraft(MAILSTREAM **streamp, ENVELOPE **outgoing, struct mail_bodystruct **body,
	char **fcc, char **lcc, REPLY_S **reply, REDRAFT_POS_S **redraft_pos,
	PINEFIELD **custom, ACTION_S **role, int flags)
{
    MAILSTREAM *stream;
    long	cont_msg = 1L;
    STORE_S    *so;

    if(!(streamp && *streamp))
      return(0);
    
    stream = *streamp;

    /*
     * If we're manipulating the current folder, don't bother
     * with index
     */
    if(!stream->nmsgs){
	if(REDRAFT_PPND&flags)
	  q_status_message(SM_ORDER | SM_DING, 3, 5, _("Empty folder! No messages really postponed!"));
	else
	  q_status_message(SM_ORDER | SM_DING, 3, 5, _("Empty folder! No messages really interrupted!"));

	return(redraft_cleanup(streamp, FALSE, flags));
    }
    else if(stream == ps_global->mail_stream
	    && ps_global->prev_screen == mail_index_screen){
	/*
	 * Since the user's got this folder already opened and they're
	 * on a selected message, pick that one rather than rebuild
	 * another index screen...
	 */
	cont_msg = mn_m2raw(ps_global->msgmap, mn_get_cur(ps_global->msgmap));
    }
    else if(stream->nmsgs > 1L){		/* offer browser ? */
	int rv;

	if(REDRAFT_PPND&flags){			/* set to last message postponed */
	    mn_set_cur(sp_msgmap(stream),
		       mn_get_revsort(sp_msgmap(stream))
			   ? 1L : mn_get_total(sp_msgmap(stream)));
	}
	else{					/* set to top form letter */
	    mn_set_cur(sp_msgmap(stream), 1L);
	}

	clear_index_cache(stream, 0);
	while(1){
	    void *ti;

	    ti = stop_threading_temporarily();
	    rv = index_lister(ps_global, NULL, stream->mailbox,
			      stream, sp_msgmap(stream));
	    restore_threading(&ti);

	    cont_msg = mn_m2raw(sp_msgmap(stream), mn_get_cur(sp_msgmap(stream)));
	    if(count_flagged(stream, F_DEL)
	       && want_to(INTR_DEL_PMT, 'n', 0, NO_HELP, WT_NORM) == 'n'){
		if(REDRAFT_PPND&flags)
		  q_status_message(SM_ORDER, 3, 3, _("Undelete messages to remain postponed, and then continue message"));
		else
		  q_status_message(SM_ORDER, 3, 3, _("Undelete form letters you want to keep, and then continue message"));

		continue;
	    }

	    break;
	}

	clear_index_cache(stream, 0);

	if(rv){
	    q_status_message(SM_ORDER, 0, 3, _("Composition cancelled"));
	    (void) redraft_cleanup(streamp, FALSE, flags);

	    if(!*streamp && !ps_global->mail_stream){
		q_status_message2(SM_ORDER, 3, 7,
			     "No more %.200s, returning to \"%.200s\"",
			     (REDRAFT_PPND&flags) ? "postponed messages"
						  : "form letters",
			     ps_global->inbox_name);
		if(ps_global && ps_global->ttyo){
		    blank_keymenu(ps_global->ttyo->screen_rows - 2, 0);
		    ps_global->mangled_footer = 1;
		}

		do_broach_folder(ps_global->inbox_name,
				 ps_global->context_list, NULL, DB_INBOXWOCNTXT);

		ps_global->next_screen = mail_index_screen;
	    }

	    return(0);				/* special case */
	}
    }

    if((so = (void *) so_get(PicoText, NULL, EDIT_ACCESS)) != NULL)
      return(redraft_work(streamp, cont_msg, outgoing, body,
			  fcc, lcc, reply, redraft_pos, custom,
			  role, flags, so));
    else
      return(0);
}


int
redraft_prompt(char *type, char *prompt, int failure)
{
    if(background_posting(FALSE)){
	q_status_message1(SM_ORDER, 0, 3,
			  _("%s folder unavailable while background posting"),
			  type);
	return(failure);
    }

    return(want_to(prompt, 'y', 'x', NO_HELP, WT_NORM));
}


/* this is for initializing the fixed header elements in pine_send() */
/*
prompt::name::help::prwid::maxlen::realaddr::
builder::affected_entry::next_affected::selector::key_label::fileedit::
display_it::break_on_comma::is_attach::rich_header::only_file_chars::
single_space::sticky::dirty::start_here::blank::sticky_special::KS_ODATAVAR
*/
static struct headerentry he_template[]={
  {"",            "X-Auth-Received",  NO_HELP,          10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"From    : ",  "From",        h_composer_from,       10, 0, NULL,
   build_address, NULL, NULL, addr_book_compose,    "To AddrBk", NULL, abook_nickname_complete,
   0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, KS_TOADDRBOOK},
  {"Reply-To: ",  "Reply To",    h_composer_reply_to,   10, 0, NULL,
   build_address, NULL, NULL, addr_book_compose,    "To AddrBk", NULL, abook_nickname_complete,
   0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, KS_TOADDRBOOK},
  {"To      : ",  "To",          h_composer_to,         10, 0, NULL,
   build_address, NULL, NULL, addr_book_compose,    "To AddrBk", NULL, abook_nickname_complete,
   0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, KS_TOADDRBOOK},
  {"Cc      : ",  "Cc",          h_composer_cc,         10, 0, NULL,
   build_address, NULL, NULL, addr_book_compose,    "To AddrBk", NULL, abook_nickname_complete,
   0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_TOADDRBOOK},
  {"Bcc     : ",  "Bcc",         h_composer_bcc,        10, 0, NULL,
   build_address, NULL, NULL, addr_book_compose,    "To AddrBk", NULL, abook_nickname_complete,
   0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, KS_TOADDRBOOK},
  {"Newsgrps: ",  "Newsgroups",  h_composer_news,        10, 0, NULL,
   news_build,    NULL, NULL, news_group_selector,  "To NwsGrps", NULL, NULL,
   0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Fcc     : ",  "Fcc",         h_composer_fcc,        10, 0, NULL,
   NULL,          NULL, NULL, folders_for_fcc,      "To Fldrs", NULL, NULL,
   0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Lcc     : ",  "Lcc",         h_composer_lcc,        10, 0, NULL,
   build_addr_lcc, NULL, NULL, addr_book_compose_lcc,"To AddrBk", NULL, abook_nickname_complete,
   0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Attchmnt: ",  "Attchmnt",    h_composer_attachment, 10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 "To Files", NULL, NULL,
   0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, KS_NONE},
  {"Subject : ",  "Subject",     h_composer_subject,    10, 0, NULL,
   valid_subject, NULL, NULL, NULL,                 NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "References",  NO_HELP,               10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "Date",        NO_HELP,               10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "In-Reply-To", NO_HELP,               10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "Message-ID",  NO_HELP,               10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "X-Priority",  NO_HELP,               10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "User-Agent",  NO_HELP,               10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "To",          NO_HELP,               10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "X-Post-Error",NO_HELP,               10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "X-Reply-UID", NO_HELP,               10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "X-Reply-Mbox", NO_HELP,              10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "X-SMTP-Server", NO_HELP,             10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "X-Cursor-Pos", NO_HELP,              10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "X-Our-ReplyTo", NO_HELP,             10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            OUR_HDRS_LIST, NO_HELP,               10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"",            "X-Auth-Received", NO_HELP,           10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
#if	!(defined(DOS) || defined(OS2)) || defined(NOAUTH)
  {"",            "Sender",      NO_HELP,               10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE}
#endif
};


static struct headerentry he_custom_addr_templ={
   NULL,          NULL,          h_composer_custom_addr,10, 0, NULL,
   build_address, NULL, NULL, addr_book_compose,    "To AddrBk", NULL, abook_nickname_complete,
   0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, KS_TOADDRBOOK};

static struct headerentry he_custom_free_templ={
   NULL,          NULL,          h_composer_custom_free,10, 0, NULL,
   NULL,          NULL, NULL, NULL,                 NULL, NULL, NULL,
   0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, KS_NONE};


/*----------------------------------------------------------------------
     Get addressee for message, then post message

  Args:  outgoing -- Partially formatted outgoing ENVELOPE
         body     -- Body of outgoing message
        prmpt_who -- Optional prompt for optionally_enter call
        prmpt_cnf -- Optional prompt for confirmation call
    used_tobufval -- The string that the to was eventually set equal to.
		      This gets passed back if non-NULL on entry.
        flagsarg  --     SS_PROMPTFORTO - Allow user to change recipient
                         SS_NULLRP - Use null return-path so we'll send an
			             SMTP MAIL FROM: <>

  Result: message "To: " field is provided and message is sent or cancelled.

  Fields:
     remail                -
     return_path           -
     date                 added here
     from                 added here
     sender                -
     reply_to              -
     subject              passed in, NOT edited but maybe canonized here
     to                   possibly passed in, edited and canonized here
     cc                    -
     bcc                   -
     in_reply_to           -
     message_id            -
  
Can only deal with message bodies that are either TYPETEXT or TYPEMULTIPART
with the first part TYPETEXT! All newlines in the text here also end with
CRLF.

Returns 0 on success, -1 on failure.
  ----*/
int
pine_simple_send(ENVELOPE *outgoing,	/* envelope for outgoing message */
		 struct mail_bodystruct **body,
		 ACTION_S *role,
		 char *prmpt_who,
		 char *prmpt_cnf,
		 char **used_tobufval,
		 int flagsarg)
{
    char     **tobufp, *p;
    void      *messagebuf;
    int        done = 0, retval = 0, x;
    int	       lastrc, rc = 0, ku, i, resize_len, result, fcc_result;
    int        og2s_done = 0;
    HelpType   help;
    static HISTORY_S *history = NULL;
    ESCKEY_S   ekey[5];
    BUILDER_ARG ba_fcc;
    METAENV   *header;

    dprint((1,"\n === simple send called === \n"));

    memset(&ba_fcc, 0, sizeof(BUILDER_ARG));

    init_hist(&history, HISTSIZE);

    header = pine_simple_send_header(outgoing, &ba_fcc.tptr, &tobufp);

    /*----- Fill in a few general parts of the envelope ----*/
    if(!outgoing->date){
	if(F_ON(F_QUELL_TIMEZONE, ps_global))
	  mail_parameters(NULL, SET_DISABLE822TZTEXT, (void *) TRUE);

	rfc822_date(tmp_20k_buf);		/* format and copy new date */
	if(F_ON(F_QUELL_TIMEZONE, ps_global))
	  mail_parameters(NULL, SET_DISABLE822TZTEXT, (void *) FALSE);

	outgoing->date = (unsigned char *) cpystr(tmp_20k_buf);
    }

    if(!outgoing->from){
	if(role && role->from){
	    if(ps_global->never_allow_changing_from)
	      q_status_message(SM_ORDER, 3, 3, _("Site policy doesn't allow changing From address so role's From has no effect"));
	    else
	      outgoing->from = copyaddrlist(role->from);
	}
	else
	  outgoing->from = generate_from();
    }

    if(!(flagsarg & SS_NULLRP))
      outgoing->return_path = rfc822_cpy_adr(outgoing->from);

    ekey[i = 0].ch    = ctrl('T');
    ekey[i].rval  = 2;
    ekey[i].name  = "^T";
    ekey[i++].label = N_("To AddrBk");

    if(F_ON(F_ENABLE_TAB_COMPLETE,ps_global)){
	ekey[i].ch      =  ctrl('I');
	ekey[i].rval    = 11;
	ekey[i].name    = "TAB";
	ekey[i++].label = N_("Complete");
    }

    ekey[i].ch      = KEY_UP;
    ekey[i].rval    = 30;
    ekey[i].name    = "";
    ku = i;
    ekey[i++].label = "";

    ekey[i].ch      = KEY_DOWN;
    ekey[i].rval    = 31;
    ekey[i].name    = "";
    ekey[i++].label = "";

    ekey[i].ch    = -1;

    /*----------------------------------------------------------------------
       Loop editing the "To: " field until everything goes well
     ----*/
    help = NO_HELP;

    while(!done){
	int flags;

	if(!og2s_done){
	    og2s_done++;
	    outgoing2strings(header, *body, &messagebuf, NULL, 1);
	}

	lastrc = rc;
	if(flagsarg & SS_PROMPTFORTO){
	    if(!*tobufp)
	      *tobufp = cpystr("");

	    resize_len = MAX(MAXPATH, strlen(*tobufp));
	    fs_resize((void **) tobufp, resize_len+1);

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

	    flags = OE_APPEND_CURRENT;

	    rc = optionally_enter(*tobufp, -FOOTER_ROWS(ps_global),
				 0, resize_len,
				 prmpt_who
				   ? prmpt_who
				   : outgoing->remail == NULL 
				     ? _("FORWARD (as e-mail) to : ")
				     : _("BOUNCE (redirect) message to : "),
				 ekey, help, &flags);
	}
	else
	  rc = 0;

	switch(rc){
	  case -1:
	    q_status_message(SM_ORDER | SM_DING, 3, 4,
			     "Internal problem encountered");
	    retval = -1;
	    done++;
	    break;

	  case 30 :
	    if((p = get_prev_hist(history, *tobufp, 0, NULL)) != NULL){
		strncpy(*tobufp, p, resize_len);
		(*tobufp)[resize_len-1] = '\0';
	    }
	    else
	      Writechar(BELL, 0);

	    break;

	  case 31 :
	    if((p = get_next_hist(history, *tobufp, 0, NULL)) != NULL){
		strncpy(*tobufp, p, resize_len);
		(*tobufp)[resize_len-1] = '\0';
	    }
	    else
	      Writechar(BELL, 0);

	    break;

	  case 2: /* ^T */
	  case 0:
	   {void (*redraw) (void) = ps_global->redrawer;
	    char  *returned_addr = NULL;
	    int    len, l;

	    if(rc == 2){
		int got_something = 0;

		push_titlebar_state();
		returned_addr = addr_book_bounce();

		/*
		 * Just make it look like user typed this list in.
		 */
		if(returned_addr){
		    got_something++;
		    if((l=resize_len) < (len = strlen(returned_addr)) + 1){
			l = len;
			fs_resize((void **) tobufp, (size_t) (l+1));
		    }

		    strncpy(*tobufp, returned_addr, l);
		    (*tobufp)[l] = '\0';
		    fs_give((void **)&returned_addr);
		}

		ClearScreen();
		pop_titlebar_state();
		redraw_titlebar();
		if((ps_global->redrawer = redraw) != NULL) /* reset old value, and test */
		  (*ps_global->redrawer)();

		if(!got_something)
		  continue;
	    }

	    if(*tobufp && **tobufp != '\0'){
		char *errbuf, *addr;
		int   tolen;

		save_hist(history, *tobufp, 0, NULL);

		errbuf = NULL;

		/*
		 * If role has an fcc, use it instead of what build_address
		 * tells us.
		 */
		if(role && role->fcc){
		    if(ba_fcc.tptr)
		      fs_give((void **) &ba_fcc.tptr);
		    
		    ba_fcc.tptr = cpystr(role->fcc);
		}

		if(build_address(*tobufp, &addr, &errbuf,
		    (role && role->fcc) ? NULL : &ba_fcc, NULL) >= 0){
		    int sendit = 0;

		    if(errbuf)
		      fs_give((void **)&errbuf);

		    if((l=strlen(*tobufp)) < (tolen = strlen(addr)) + 1){
			l = tolen;
			fs_resize((void **) tobufp, (size_t) (l+1));
		    }

		    strncpy(*tobufp, addr, l);
		    (*tobufp)[l] = '\0';
		    if(used_tobufval)
		      *used_tobufval = cpystr(addr);

		    /* confirm address */
		    if(flagsarg & SS_PROMPTFORTO){
			char dsn_string[30];
			int  dsn_label = 0, dsn_show, i;
			int  verbose_label = 0;
			ESCKEY_S opts[13];

			strings2outgoing(header, body, NULL, 0);

			if((flagsarg & SS_PROMPTFORTO)
			   && ((x = check_addresses(header)) == CA_BAD 
			       || (x == CA_EMPTY && F_OFF(F_FCC_ON_BOUNCE,
							  ps_global))))
			  /*--- Addresses didn't check out---*/
			  continue;
			
			i = 0;
			opts[i].ch      = 'y';
			opts[i].rval    = 'y';
			opts[i].name    = "Y";
			opts[i++].label = N_("Yes");

			opts[i].ch      = 'n';
			opts[i].rval    = 'n';
			opts[i].name    = "N";
			opts[i++].label = N_("No");

			call_mailer_flags &= ~CM_VERBOSE;	/* clear verbose */
			if(F_ON(F_VERBOSE_POST, ps_global)){
			    /* setup keymenu slot to toggle verbose mode */
			    opts[i].ch    = ctrl('W');
			    opts[i].rval  = 12;
			    opts[i].name  = "^W";
			    verbose_label = i++;
			    if(F_ON(F_DSN, ps_global)){
				opts[i].ch      = 0;
				opts[i].rval    = 0;
				opts[i].name    = "";
				opts[i++].label = "";
			    }
			}

			/* clear DSN flags */
			call_mailer_flags &= ~(CM_DSN_NEVER | CM_DSN_DELAY | CM_DSN_SUCCESS | CM_DSN_FULL);
			if(F_ON(F_DSN, ps_global)){
			    /* setup keymenu slots to toggle dsn bits */
			    opts[i].ch      = 'd';
			    opts[i].rval    = 'd';
			    opts[i].name    = "D";
			    opts[i].label   = "DSNOpts";
			    dsn_label       = i++;
			    opts[i].ch      = -2;
			    opts[i].rval    = 's';
			    opts[i].name    = "S";
			    opts[i++].label = "";
			    opts[i].ch      = -2;
			    opts[i].rval    = 'x';
			    opts[i].name    = "X";
			    opts[i++].label = "";
			    opts[i].ch      = -2;
			    opts[i].rval    = 'h';
			    opts[i].name    = "H";
			    opts[i++].label = "";
			}

			opts[i].ch = -1;

			while(1){
			    int rv;

			    dsn_show = (call_mailer_flags & CM_DSN_SHOW);
			    snprintf(tmp_20k_buf, SIZEOF_20KBUF,
				    "%s%s%s%s%s%sto \"%s\" ? ",
				    prmpt_cnf ? prmpt_cnf : "Send message ",
				    ((call_mailer_flags & CM_VERBOSE)
				      || (dsn_show))
				      ? "(" : "",
				    (call_mailer_flags & CM_VERBOSE)
				      ? "in verbose mode" : "",
				    (dsn_show && (call_mailer_flags & CM_VERBOSE))
				      ? ", " : "",
				    (dsn_show) ? dsn_string : "",
				    ((call_mailer_flags & CM_VERBOSE) || dsn_show)
				      ? ") " : "",
				    (addr && *addr)
					? addr
					: (F_ON(F_FCC_ON_BOUNCE, ps_global)
					   && ba_fcc.tptr && ba_fcc.tptr[0])
					    ? ba_fcc.tptr
					    : "");
			    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';

			    if((strlen(tmp_20k_buf) >
					ps_global->ttyo->screen_cols - 2) &&
			       ps_global->ttyo->screen_cols >= 7)
			      strncpy(tmp_20k_buf+ps_global->ttyo->screen_cols-7,
				     "...? ", SIZEOF_20KBUF-ps_global->ttyo->screen_cols-7);

			    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';

			    if(verbose_label)
			      opts[verbose_label].label =
				      /* TRANSLATORS: several possible key labels follow */
				      (call_mailer_flags & CM_VERBOSE) ? N_("Normal") : N_("Verbose");
			    
			    if(F_ON(F_DSN, ps_global)){
				if(call_mailer_flags & CM_DSN_SHOW){
				    opts[dsn_label].label   =
						(call_mailer_flags & CM_DSN_DELAY)
						   ? N_("NoDelay") : N_("Delay");
				    opts[dsn_label+1].ch    = 's';
				    opts[dsn_label+1].label =
						(call_mailer_flags & CM_DSN_SUCCESS)
						   ? N_("NoSuccess") : N_("Success");
				    opts[dsn_label+2].ch    = 'x';
				    opts[dsn_label+2].label =
						(call_mailer_flags & CM_DSN_NEVER)
						   ? N_("ErrRets") : N_("NoErrRets");
				    opts[dsn_label+3].ch    = 'h';
				    opts[dsn_label+3].label =
						(call_mailer_flags & CM_DSN_FULL)
						   ? N_("RetHdrs") : N_("RetFull");
				}
			    }

			    rv = radio_buttons(tmp_20k_buf,
					       -FOOTER_ROWS(ps_global), opts,
					       'y', 'z', NO_HELP, RB_NORM);
			    if(rv == 'y'){		/* user ACCEPTS! */
				sendit = 1;
				break;
			    }
			    else if(rv == 'n'){		/* Declined! */
				break;
			    }
			    else if(rv == 'z'){		/* Cancelled! */
				break;
			    }
			    else if(rv == 12){		/* flip verbose bit */
				if(call_mailer_flags & CM_VERBOSE)
				  call_mailer_flags &= ~CM_VERBOSE;
				else
				  call_mailer_flags |= CM_VERBOSE;
			    }
			    else if(call_mailer_flags & CM_DSN_SHOW){
				if(rv == 's'){		/* flip success bit */
				    call_mailer_flags ^= CM_DSN_SUCCESS;
				    /* turn off related bits */
				    if(call_mailer_flags & CM_DSN_SUCCESS)
				      call_mailer_flags &= ~(CM_DSN_NEVER);
				}
				else if(rv == 'd'){	/* flip delay bit */
				    call_mailer_flags ^= CM_DSN_DELAY;
				    /* turn off related bits */
				    if(call_mailer_flags & CM_DSN_DELAY)
				      call_mailer_flags &= ~(CM_DSN_NEVER);
				}
				else if(rv == 'x'){	/* flip never bit */
				    call_mailer_flags ^= CM_DSN_NEVER;
				    /* turn off related bits */
				    if(call_mailer_flags & CM_DSN_NEVER)
				      call_mailer_flags &= ~(CM_DSN_SUCCESS | CM_DSN_DELAY);
				}
				else if(rv == 'h'){	/* flip full bit */
				    call_mailer_flags ^= CM_DSN_FULL;
				}
			    }
			    else if(rv == 'd'){		/* show dsn options */
				call_mailer_flags |= (CM_DSN_SHOW | CM_DSN_SUCCESS | CM_DSN_DELAY | CM_DSN_FULL);
			    }

			    snprintf(dsn_string, sizeof(dsn_string), _("DSN requested[%s%s%s%s]"),
				    (call_mailer_flags & CM_DSN_NEVER)
				      ? _("Never") : "F",
				    (call_mailer_flags & CM_DSN_DELAY)
				      ? "D" : "",
				    (call_mailer_flags & CM_DSN_SUCCESS)
				      ? "S" : "",
				    (call_mailer_flags & CM_DSN_NEVER)
				      ? ""
				      : (call_mailer_flags & CM_DSN_FULL) ? "-Full"
								   : "-Hdrs");
			    dsn_string[sizeof(dsn_string)-1] = '\0';
			}
		    }

		    if(addr)
		      fs_give((void **)&addr);

		    if(!(flagsarg & SS_PROMPTFORTO) || sendit){
			char *fcc = NULL;
			CONTEXT_S *fcc_cntxt = NULL;

			if(F_ON(F_FCC_ON_BOUNCE, ps_global)){
			    if(ba_fcc.tptr)
			      fcc = cpystr(ba_fcc.tptr);

			    set_last_fcc(fcc);

			    /*
			     * If special name "inbox" then replace it with the
			     * real inbox path.
			     */
			    if(ps_global->VAR_INBOX_PATH
			       && strucmp(fcc, ps_global->inbox_name) == 0){
				char *replace_fcc;

				replace_fcc = cpystr(ps_global->VAR_INBOX_PATH);
				fs_give((void **) &fcc);
				fcc = replace_fcc;
			    }
			}

			/*---- Check out fcc -----*/
			if(fcc && *fcc){
			    (void) commence_fcc(fcc, &fcc_cntxt, FALSE);
			    if(!lmc.so){
				dprint((4,"can't open fcc, cont\n"));
				if(!(flagsarg & SS_PROMPTFORTO)){
				    retval = -1;
				    fs_give((void **)&fcc);
				    fcc = NULL;
				    goto finish;
				}
				else
				  continue;
			    }
			    else
			      so_truncate(lmc.so, fcc_size_guess(*body) + 2048);
			}
			else
			  lmc.so = NULL;

			if(!(outgoing->to || outgoing->cc || outgoing->bcc
			     || lmc.so)){
			    q_status_message(SM_ORDER, 3, 5, _("No recipients specified!"));
			    continue;
			}

			if(outgoing->to || outgoing->cc || outgoing->bcc){
			    char **alt_smtp = NULL;

			    if(role && role->smtp){
				if(ps_global->FIX_SMTP_SERVER
				   && ps_global->FIX_SMTP_SERVER[0])
				  q_status_message(SM_ORDER | SM_DING, 5, 5, _("Use of a role-defined smtp-server is administratively prohibited"));
				else
				  alt_smtp = role->smtp;
			    }

			    result = call_mailer(header, *body, alt_smtp,
						 call_mailer_flags,
						 call_mailer_file_result,
						 pipe_callback);
			    mark_address_failure_for_pico(header);
			}
			else
			  result = 0;

			if(result == 1 && !lmc.so)
			  q_status_message(SM_ORDER, 0, 3, _("Message sent"));

			/*----- Was there an fcc involved? -----*/
			if(lmc.so){
			    if(result == 1
			       || (result == 0
			   && pine_rfc822_output(header, *body, NULL, NULL))){
				char label[50];

				strncpy(label, "Fcc", sizeof(label));
				label[sizeof(label)-1] = '\0';
				if(strcmp(fcc, ps_global->VAR_DEFAULT_FCC)){
				    snprintf(label + 3, sizeof(label)-3, " to %s", fcc);
				    label[sizeof(label)-1] = '\0';
				}

				/* Now actually copy to fcc folder and close */
				fcc_result = 
				  write_fcc(fcc, fcc_cntxt, lmc.so, NULL,
					    label,
					    F_ON(F_MARK_FCC_SEEN, ps_global)
						? "\\SEEN" : NULL);
			    }
			    else if(result == 0){
				q_status_message(SM_ORDER,3,5,
				    _("Fcc Failed!.  No message saved."));
				retval = -1;
				dprint((1, "explicit fcc write failed!\n"));
			    }

			    so_give(&lmc.so);
			}

			if(result < 0){
			    dprint((1, "Bounce failed\n"));
			    if(!(flagsarg & SS_PROMPTFORTO))
			      retval = -1;
			    else
			      continue;
			}
			else if(result == 1){
			  if(!fcc)
			    q_status_message(SM_ORDER, 0, 3, 
					     _("Message sent"));
			  else{
			      int avail = ps_global->ttyo->screen_cols-2;
			      int need, fcclen;
			      char *part1 = "Message sent and ";
			      char *part2 = fcc_result ? "" : "NOT ";
			      char *part3 = "copied to ";
			      fcclen = strlen(fcc);

			      need = 2 + strlen(part1) + strlen(part2) +
				     strlen(part3) + fcclen;

			      if(need > avail && fcclen > 6)
				fcclen -= MIN(fcclen-6, need-avail);

			      q_status_message4(SM_ORDER, 0, 3,
						"%s%s%s\"%s\"",
						part1, part2, part3,
						short_str(fcc,
							  (char *)tmp_20k_buf,
							  SIZEOF_20KBUF,
							  fcclen, FrontDots));
			  }
			} 

			if(fcc)
			  fs_give((void **)&fcc);
		    }
		    else{
			q_status_message(SM_ORDER, 0, 3, _("Send cancelled"));
			retval = -1;
		    }
		}
		else{
		    q_status_message1(SM_ORDER | SM_DING, 3, 5,
				      _("Error in address: %s"), errbuf);
		    if(errbuf)
		      fs_give((void **)&errbuf);

		    if(!(flagsarg & SS_PROMPTFORTO))
		      retval = -1;
		    else
		      continue;
		}

	    }
	    else{
		q_status_message(SM_ORDER | SM_DING, 3, 5,
				 _("No addressee!  No e-mail sent."));
		retval = -1;
	    }
	   }

	    done++;
	    break;

	  case 1:
	    q_status_message(SM_ORDER, 0, 3, _("Send cancelled"));
	    done++;
	    retval = -1;
	    break;

	  case 3:
	    help = (help == NO_HELP)
			    ? (outgoing->remail == NULL
				? h_anon_forward
				: h_bounce)
			    : NO_HELP;
	    break;

	  case 11:
	    if(**tobufp){
	      char *new_nickname = NULL;
	      int l;
	      int ambiguity;

	      ambiguity = abook_nickname_complete(*tobufp, &new_nickname,
			      (lastrc==rc && !(flags & OE_USER_MODIFIED)), ANC_AFTERCOMMA);
	      if(new_nickname){
		if(*new_nickname){
		  if((l=strlen(new_nickname)) > resize_len){
		    resize_len = l;
		    fs_resize((void **) tobufp, resize_len+1);
		  }
		    
		  strncpy(*tobufp, new_nickname, l);
		  (*tobufp)[l] = '\0';
	        }

		fs_give((void **) &new_nickname);
	      }

	      if(ambiguity != 2)
		Writechar(BELL, 0);
	    }

	    break;

	  case 4:				/* can't suspend */
	  default:
	    break;
	}
    }

finish:
    if(ba_fcc.tptr)
      fs_give((void **)&ba_fcc.tptr);

    pine_free_env(&header);

    return(retval);
}


/*
 * pine_simple_send_header - generate header suitable for simple_sending
 */
METAENV *
pine_simple_send_header(ENVELOPE *outgoing, char **fccp, char ***tobufpp)
{
    METAENV   *header;
    PINEFIELD *pf;
    static struct headerentry he_dummy;
    
    header = pine_new_env(outgoing, fccp, tobufpp, NULL);

    /* assign he_dummy to "To:" field "he" for strings2outgoing */
    for(pf = header->local; pf && pf->name; pf = pf->next)
      if(pf->type == Address && !strucmp(pf->name, "to")){
	  memset((void *) &he_dummy, 0, sizeof(he_dummy));
	  pf->extdata   = (void *) &he_dummy;
	  HE(pf)->dirty	= 1;
	  break;
      }
 
    return(header);
}



/*----------------------------------------------------------------------
     Prepare data structures for pico, call pico, then post message

  Args:  outgoing     -- Partially formatted outgoing ENVELOPE
         body         -- Body of outgoing message
         editor_title -- Title for anchor line in composer
         fcc_arg      -- The file carbon copy field
	 reply        -- Struct describing set of msgs being replied-to
	 lcc_arg      --
	 custom       -- custom header list.
	 sticky_fcc   --

  Result: message is edited, then postponed, cancelled or sent.

  Fields:
     remail                -
     return_path           -
     date                 added here
     from                 added here
     sender                -
     reply_to              -
     subject              passed in, edited and cannonized here
     to                   possibly passed in, edited and cannonized here
     cc                   possibly passed in, edited and cannonized here
     bcc                  edited and cannonized here
     in_reply_to          generated in reply() and passed in
     message_id            -
  
 Storage for these fields comes from anywhere outside. It is remalloced
 here so the composer can realloc them if needed. The copies here are also 
 freed here.

Can only deal with message bodies that are either TYPETEXT or TYPEMULTIPART
with the first part TYPETEXT! All newlines in the text here also end with
CRLF.

There's a further assumption that the text in the TYPETEXT part is 
stored in a storage object (see filter.c).
  ----*/
void
pine_send(ENVELOPE *outgoing, struct mail_bodystruct **body,
	  char *editor_title, ACTION_S *role, char *fcc_arg,
	  REPLY_S *reply, REDRAFT_POS_S *redraft_pos, char *lcc_arg,
	  PINEFIELD *custom, int flags)
{
    int			i, fixed_cnt, total_cnt, index,
			editor_result = 0, body_start = 0, use_news_order = 0;
    char	       *p, *addr, *fcc, *fcc_to_free = NULL;
    char	       *start_here_name = NULL;
    char               *suggested_nntp_server = NULL;
    char	       *title = NULL;
    struct headerentry *he, *headents, *he_to, *he_fcc, *he_news = NULL, *he_lcc = NULL,
		       *he_from = NULL;
    PINEFIELD          *pfields, *pf, *pf_nobody = NULL, *pf_to = NULL,
                       *pf_smtp_server, *pf_nntp_server,
		       *pf_fcc = NULL, *pf_err, *pf_uid, *pf_mbox, *pf_curpos,
		       *pf_ourrep, *pf_ourhdrs, **sending_order;
    METAENV             header;
    ADDRESS            *lcc_addr = NULL;
    ADDRESS            *nobody_addr = NULL;
    BODY_PARTICULARS_S *bp;
    STORE_S	       *orig_so = NULL;
    PICO	        pbuf1, *save_previous_pbuf;
    CustomType          ct;
    REDRAFT_POS_S      *local_redraft_pos = NULL;

    dprint((1,"\n=== send called ===\n"));

    save_previous_pbuf = pbf;
    pbf = &pbuf1;
    standard_picobuf_setup(pbf);

    /*
     * Cancel any pending initial commands since pico uses a different
     * input routine.  If we didn't cancel them, they would happen after
     * we returned from the editor, which would be confusing.
     */
    if(ps_global->in_init_seq){
	ps_global->in_init_seq = 0;
	ps_global->save_in_init_seq = 0;
	clear_cursor_pos();
	if(ps_global->initial_cmds){
	    if(ps_global->free_initial_cmds)
	      fs_give((void **)&(ps_global->free_initial_cmds));

	    ps_global->initial_cmds = 0;
	}

	F_SET(F_USE_FK,ps_global,ps_global->orig_use_fkeys);
    }

#if defined(DOS) || defined(OS2)
    if(!dos_valid_from()){
	pbf = save_previous_pbuf;
	return;
    }

    pbf->upload        = NULL;
#else
    pbf->upload        = (ps_global->VAR_UPLOAD_CMD
			  && ps_global->VAR_UPLOAD_CMD[0])
			  ? upload_msg_to_pico : NULL;
#endif

    pbf->msgntext      = message_format_for_pico;
    pbf->mimetype      = mime_type_for_pico;
    pbf->exittest      = send_exit_for_pico;
    pbf->user_says_noflow = dont_flow_this_time;
    if(F_OFF(F_CANCEL_CONFIRM, ps_global))
      pbf->canceltest    = cancel_for_pico;

    pbf->alt_ed        = (ps_global->VAR_EDITOR && ps_global->VAR_EDITOR[0] &&
			    ps_global->VAR_EDITOR[0][0])
				? ps_global->VAR_EDITOR : NULL;
    pbf->alt_spell     = (ps_global->VAR_SPELLER && ps_global->VAR_SPELLER[0])
				? ps_global->VAR_SPELLER : NULL;
    pbf->always_spell_check = F_ON(F_ALWAYS_SPELL_CHECK, ps_global);
    pbf->quote_str     = reply && reply->prefix ? reply->prefix : "> ";
    /* We actually want to set this only if message we're sending is flowed */
    pbf->strip_ws_before_send = F_ON(F_STRIP_WS_BEFORE_SEND, ps_global);
    pbf->allow_flowed_text = (F_OFF(F_QUELL_FLOWED_TEXT, ps_global)
			      && F_OFF(F_STRIP_WS_BEFORE_SEND, ps_global)
			      && (strcmp(pbf->quote_str, "> ") == 0
				  || strcmp(pbf->quote_str, ">") == 0));
    pbf->edit_offset   = 0;
    title               = cpystr(set_titlebar(editor_title,
				    ps_global->mail_stream,
				    ps_global->context_current,
				    ps_global->cur_folder,ps_global->msgmap, 
				    0, FolderName, 0, 0, NULL));
    pbf->pine_anchor   = title;

#if	defined(DOS) || defined(OS2)
    if(!pbf->oper_dir && ps_global->VAR_FILE_DIR){
	pbf->oper_dir    = ps_global->VAR_FILE_DIR;
    }
#endif

    if(redraft_pos && editor_title && !strcmp(editor_title, COMPOSE_MAIL_TITLE))
      pbf->pine_flags |= P_CHKPTNOW;

    /* NOTE: initial cursor position set below */

    dprint((9, "flags: %x\n", pbf->pine_flags));

    /*
     * When user runs compose and the current folder is a newsgroup,
     * offer to post to the current newsgroup.
     */
    if(!(outgoing->to || (outgoing->newsgroups && *outgoing->newsgroups))
       && IS_NEWS(ps_global->mail_stream)){
	char prompt[200], news_group[MAILTMPLEN];

	pine_send_newsgroup_name(ps_global->mail_stream->mailbox, news_group,
				 sizeof(news_group));

	/*
	 * Replies don't get this far because To or Newsgroups will already
	 * be filled in.  So must be either ordinary compose or forward.
	 * Forward sets subject, so use that to tell the difference.
	 */
	if(news_group[0] && !outgoing->subject){
	    int ch = 'y';
	    int ret_val;
	    char *errmsg = NULL;
	    BUILDER_ARG	 *fcc_build = NULL;

	    if(F_OFF(F_COMPOSE_TO_NEWSGRP,ps_global)){
		snprintf(prompt, sizeof(prompt),
		    _("Post to current newsgroup (%s)"), news_group);
		prompt[sizeof(prompt)-1] = '\0';
		ch = want_to(prompt, 'y', 'x', NO_HELP, WT_NORM);
	    }

	    switch(ch){
	      case 'y':
		if(outgoing->newsgroups)
		  fs_give((void **)&outgoing->newsgroups); 

		if(!fcc_arg && !(role && role->fcc)){
		    fcc_build = (BUILDER_ARG *)fs_get(sizeof(BUILDER_ARG));
		    memset((void *)fcc_build, 0, sizeof(BUILDER_ARG));
		    fcc_build->tptr = fcc_to_free;
		}

		ret_val = news_build(news_group, &outgoing->newsgroups,
				     &errmsg, fcc_build, NULL);

		if(ret_val == -1){
		    if(outgoing->newsgroups)
		      fs_give((void **)&outgoing->newsgroups); 

		    outgoing->newsgroups = cpystr(news_group);
		}

		if(!fcc_arg && !(role && role->fcc)){
		    fcc_arg = fcc_to_free = fcc_build->tptr;
		    fs_give((void **)&fcc_build);
		}

		if(errmsg){
		    if(*errmsg){
			q_status_message(SM_ORDER, 3, 3, errmsg);
			display_message(NO_OP_COMMAND);
		    }

		    fs_give((void **)&errmsg);
		}

		break;

	      case 'x': /* ^C */
		q_status_message(SM_ORDER, 0, 3, _("Message cancelled"));
		dprint((4, "=== send: cancelled\n"));
		pbf = save_previous_pbuf;
		return;

	      case 'n':
		break;

	      default:
		break;
	    }
	}
    }
    if(F_ON(F_PREDICT_NNTP_SERVER, ps_global)
       && outgoing->newsgroups && *outgoing->newsgroups
       && IS_NEWS(ps_global->mail_stream)){
	NETMBX news_mb;

	if(mail_valid_net_parse(ps_global->mail_stream->original_mailbox,
				&news_mb))
	  if(!strucmp(news_mb.service, "nntp")){
	      if(*ps_global->mail_stream->original_mailbox == '{'){
		  char *svcp = NULL, *psvcp;

		  suggested_nntp_server =
		    cpystr(ps_global->mail_stream->original_mailbox + 1);
		  if((p = strindex(suggested_nntp_server, '}')) != NULL)
		    *p = '\0';
		  for(p = strindex(suggested_nntp_server, '/'); p && *p;
		      p = strindex(p, '/')){
		      /* take out /nntp, which gets added in nntp_open */
		      if(!struncmp(p, "/nntp", 5))
			svcp = p + 5;
		      else if(!struncmp(p, "/service=nntp", 13))
			svcp = p + 13;
		      else if(!struncmp(p, "/service=\"nntp\"", 15))
			svcp = p + 15;
		      else
			p++;
		      if(svcp){
			  if(*svcp == '\0')
			    *p = '\0';
			  else if(*svcp == '/' || *svcp == ':'){
			      for(psvcp = p; *svcp; svcp++, psvcp++)
				*psvcp = *svcp;
			      *psvcp = '\0';
			  }
			  svcp = NULL;
		      }
		  }
	      }
	      else
		suggested_nntp_server = cpystr(news_mb.orighost);
	  }
    }

    /*
     * If we don't already have custom headers set and the role has custom
     * headers, then incorporate those custom headers into "custom".
     */
    if(!custom){
	PINEFIELD *dflthdrs = NULL, *rolehdrs = NULL;

	dflthdrs = parse_custom_hdrs(ps_global->VAR_CUSTOM_HDRS, UseAsDef);
/*
 * If we allow the Combine argument here, we're saying that we want to
 * combine the values from the envelope and the role for the fields To,
 * Cc, Bcc, and Newsgroups. For example, if we are replying to a message
 * we'll have a To in the envelope because we're replying. If our role also
 * has a To action, then Combine would combine those two and offer both
 * to the user. We've decided against doing this. Instead, we always use
 * Replace, and the role's header value replaces the value from the
 * envelope. It might also make sense in some cases to do the opposite,
 * which would be treating the role headers as defaults, just like
 * customized-hdrs.
 */
#ifdef WANT_TO_COMBINE_ADDRESSES
	if(role && role->cstm)
	  rolehdrs = parse_custom_hdrs(role->cstm, Combine);
#else
	if(role && role->cstm)
	  rolehdrs = parse_custom_hdrs(role->cstm, Replace);
#endif

	if(rolehdrs){
	    custom = combine_custom_headers(dflthdrs, rolehdrs);
	    if(dflthdrs){
		free_prompts(dflthdrs);
		free_customs(dflthdrs);
	    }

	    if(rolehdrs){
		free_prompts(rolehdrs);
		free_customs(rolehdrs);
	    }
	}
        else
	  custom = dflthdrs;
    }

    g_rolenick = role ? role->nick : NULL;

    /* how many fixed fields are there? */
    for(fixed_cnt = 0; pf_template && pf_template[fixed_cnt].name; fixed_cnt++)
      ;

    total_cnt = fixed_cnt + count_custom_hdrs_pf(custom,1);

    /* the fixed part of the PINEFIELDs */
    i       = fixed_cnt * sizeof(PINEFIELD);
    pfields = (PINEFIELD *)fs_get((size_t) i);
    memset(pfields, 0, (size_t) i);

    /* temporary headerentry array for pico */
    i        = (total_cnt + 1) * sizeof(struct headerentry);
    headents = (struct headerentry *)fs_get((size_t) i);
    memset(headents, 0, (size_t) i);

    i             = total_cnt * sizeof(PINEFIELD *);
    sending_order = (PINEFIELD **)fs_get((size_t) i);
    memset(sending_order, 0, (size_t) i);

    pbf->headents        = headents;
    header.env           = outgoing;
    header.local         = pfields;
    header.sending_order = sending_order;

    /* custom part of PINEFIELDs */
    header.custom = custom;

    he = headents;
    pf = pfields;

    /*
     * For Address types, pf->addr points to an ADDRESS *.
     * If that address is in the "outgoing" envelope, it will
     * be freed by the caller, otherwise, it should be freed here.
     * Pf->textbuf for an Address is used a little to set up a default,
     * but then is freed right away below.  Pf->scratch is used for a
     * pointer to some alloced space for pico to edit in.  Addresses in
     * the custom area are freed by free_customs().
     *
     * For FreeText types, pf->addr is not used.  Pf->text points to a
     * pointer that points to the text.  Pf->textbuf points to a copy of
     * the text that must be freed before we leave, otherwise, it is
     * probably a pointer into the envelope and that gets freed by the
     * caller.
     *
     * He->realaddr is the pointer to the text that pico actually edits.
     */

#if	!(defined(DOS) || defined(OS2)) || defined(NOAUTH)
# define NN 4
#else
# define NN 3
#endif

    if(outgoing->newsgroups && *outgoing->newsgroups)
      use_news_order++;

    /* initialize the fixed header elements of the two temp arrays */
    for(i=0; i < fixed_cnt; i++, pf++){
	static int news_order[] = {
	    N_AUTHRCVD,N_FROM, N_REPLYTO, N_NEWS, N_TO, N_CC, N_BCC,
	    N_FCC, N_LCC, N_ATTCH, N_SUBJ, N_REF, N_DATE, N_INREPLY,
	    N_MSGID, N_PRIORITY, N_USERAGENT, N_NOBODY, N_POSTERR, N_RPLUID, N_RPLMBOX,
	    N_SMTP, N_NNTP, N_CURPOS, N_OURREPLYTO, N_OURHDRS
#if	!(defined(DOS) || defined(OS2)) || defined(NOAUTH)
	    , N_SENDER
#endif
	};

	index = i;
	/* slightly different editing order if sending to news */
	if(use_news_order &&
	   index >= 0 && index < sizeof(news_order)/sizeof(news_order[0]))
	  index = news_order[i];

	/* copy the templates */
	*he             = he_template[index];

	pf->name        = cpystr(pf_template[index].name);
	if(index == N_SENDER && F_ON(F_USE_SENDER_NOT_X, ps_global))
	  /* slide string over so it is Sender instead of X-X-Sender */
	  for(p=pf->name; *(p+1); p++)
	    *p = *(p+4);

	pf->type        = pf_template[index].type;
	pf->canedit     = pf_template[index].canedit;
	pf->rcptto      = pf_template[index].rcptto;
	pf->writehdr    = pf_template[index].writehdr;
	pf->localcopy   = pf_template[index].localcopy;
	pf->extdata     = he;
	pf->next        = pf + 1;

	he->rich_header = view_as_rich(pf->name, he->rich_header);
	if(F_OFF(F_ENABLE_TAB_COMPLETE,ps_global))
	  he->nickcmpl = NULL;

	switch(pf->type){
	  case FreeText:   		/* realaddr points to c-client env */
	    if(index == N_NEWS){
		sending_order[1]	= pf;
		he->realaddr		= &outgoing->newsgroups;
	        he_news			= he;

		switch(set_default_hdrval(pf, custom)){
		  case Replace:
		    if(*he->realaddr)
		      fs_give((void **)he->realaddr);

		    *he->realaddr = pf->textbuf;
		    pf->textbuf = NULL;
		    he->sticky = 1;
		    break;

		  case Combine:
		    if(*he->realaddr){		/* combine values */
			if(pf->textbuf && *pf->textbuf){
			    char *combined_hdr;
			    size_t l;

			    l = strlen(*he->realaddr) + strlen(pf->textbuf) + 1;
			    combined_hdr = (char *) fs_get((l+1) * sizeof(char));
			    strncpy(combined_hdr, *he->realaddr, l);
			    combined_hdr[l] = '\0';
			    strncat(combined_hdr, ",", l+1-1-strlen(combined_hdr));
			    combined_hdr[l] = '\0';
			    strncat(combined_hdr, pf->textbuf, l+1-1-strlen(combined_hdr));
			    combined_hdr[l] = '\0';

			    fs_give((void **)he->realaddr);
			    *he->realaddr = combined_hdr;
			    q_status_message(SM_ORDER, 3, 3,
					     "Adding newsgroup from role");
			    he->sticky = 1;
			}
		    }
		    else{
			*he->realaddr = pf->textbuf;
			pf->textbuf   = NULL;
		    }

		    break;

		  case UseAsDef:
		    /* if no value, use default */
		    if(!*he->realaddr){
			*he->realaddr = pf->textbuf;
			pf->textbuf   = NULL;
		    }

		    break;

		  case NoMatch:
		    break;
		}

		/* If there is a newsgroup, we'd better show it */
		if(outgoing->newsgroups && *outgoing->newsgroups)
		  he->rich_header = 0; /* force on by default */

		if(pf->textbuf)
		  fs_give((void **)&pf->textbuf);

		pf->text = he->realaddr;
	    }
	    else if(index == N_DATE){
		sending_order[2]	= pf;
		pf->text		= (char **) &outgoing->date;
		pf->extdata			= NULL;
	    }
	    else if(index == N_INREPLY){
		sending_order[NN+9]	= pf;
		pf->text		= &outgoing->in_reply_to;
		pf->extdata			= NULL;
	    }
	    else if(index == N_MSGID){
		sending_order[NN+10]	= pf;
		pf->text		= &outgoing->message_id;
		pf->extdata			= NULL;
	    }
	    else if(index == N_REF){
		sending_order[NN+11]	= pf;
		pf->text		= &outgoing->references;
		pf->extdata			= NULL;
	    }
	    else if(index == N_PRIORITY){
		sending_order[NN+12]	= pf;
		pf->text		= &pf->textbuf;
		pf->extdata			= NULL;
	    }
	    else if(index == N_USERAGENT){
		sending_order[NN+13]	= pf;
		pf->text		= &pf->textbuf;
		pf->textbuf		= generate_user_agent();
		pf->extdata			= NULL;
	    }
	    else if(index == N_POSTERR){
		sending_order[NN+14]	= pf;
		pf_err			= pf;
		pf->text		= &pf->textbuf;
		pf->extdata			= NULL;
	    }
	    else if(index == N_RPLUID){
		sending_order[NN+15]	= pf;
		pf_uid			= pf;
		pf->text		= &pf->textbuf;
		pf->extdata			= NULL;
	    }
	    else if(index == N_RPLMBOX){
  		sending_order[NN+16]	= pf;
		pf_mbox			= pf;
		pf->text		= &pf->textbuf;
		pf->extdata			= NULL;
	    }
	    else if(index == N_SMTP){
		sending_order[NN+17]	= pf;
		pf_smtp_server		= pf;
		pf->text		= &pf->textbuf;
		pf->extdata			= NULL;
	    }
	    else if(index == N_NNTP){
		sending_order[NN+18]	= pf;
		pf_nntp_server		= pf;
		pf->text		= &pf->textbuf;
		pf->extdata			= NULL;
	    }
	    else if(index == N_CURPOS){
		sending_order[NN+19]	= pf;
		pf_curpos		= pf;
		pf->text		= &pf->textbuf;
		pf->extdata			= NULL;
	    }
	    else if(index == N_OURREPLYTO){
		sending_order[NN+20]	= pf;
		pf_ourrep		= pf;
		pf->text		= &pf->textbuf;
		pf->extdata			= NULL;
	    }
	    else if(index == N_OURHDRS){
		sending_order[NN+21]	= pf;
		pf_ourhdrs		= pf;
		pf->text		= &pf->textbuf;
		pf->extdata			= NULL;
	    }
	    else if(index == N_AUTHRCVD){
		sending_order[0]	= pf;
		pf_ourhdrs		= pf;
		pf->text		= &pf->textbuf;
		pf->extdata			= NULL;
	    }
	    else{
		q_status_message(SM_ORDER | SM_DING, 3, 7,
			    "Botched: Unmatched FreeText header in pine_send");
	    }

	    break;

	  /* can't do a default for this one */
	  case Attachment:
	    /* If there is an attachment already, we'd better show them */
            if(body && *body && (*body)->type != TYPETEXT)
	      he->rich_header = 0; /* force on by default */

	    break;

	  case Address:
	    switch(index){
	      case N_FROM:
		sending_order[3]	= pf;
		pf->addr		= &outgoing->from;
		if(role && role->from){
		    if(ps_global->never_allow_changing_from)
		      q_status_message(SM_ORDER, 3, 3, _("Site policy doesn't allow changing From address so role's From has no effect"));
		    else{
			outgoing->from = copyaddrlist(role->from);
			he->display_it  = 1;  /* show it */
			he->rich_header = 0;
		    }
		}

		he_from			= he;
		break;

	      case N_TO:
		sending_order[NN+2]	= pf;
		pf->addr		= &outgoing->to;
		/* If already set, make it act like we typed it in */
		if(outgoing->to
		   && outgoing->to->mailbox
		   && outgoing->to->mailbox[0]
		   && flags & PS_STICKY_TO)
		  he->sticky = 1;

		he_to			= he;
		pf_to			= pf;
		break;

	      case N_NOBODY:
		sending_order[NN+5]	= pf;
		pf_nobody		= pf;
		if(ps_global->VAR_EMPTY_HDR_MSG
		   && !ps_global->VAR_EMPTY_HDR_MSG[0]){
		    pf->addr		= NULL;
		}
		else{
		    nobody_addr          = mail_newaddr();
		    nobody_addr->next    = mail_newaddr();
		    nobody_addr->mailbox = cpystr(rfc1522_encode(tmp_20k_buf,
			    SIZEOF_20KBUF,
			    (unsigned char *)(ps_global->VAR_EMPTY_HDR_MSG
						? ps_global->VAR_EMPTY_HDR_MSG
						: "undisclosed-recipients"),
			    ps_global->posting_charmap));
		    pf->addr		= &nobody_addr;
		}

		break;

	      case N_CC:
		sending_order[NN+3]	= pf;
		pf->addr		= &outgoing->cc;
		break;

	      case N_BCC:
		sending_order[NN+4]	= pf;
		pf->addr		= &outgoing->bcc;
		/* if bcc exists, make sure it's exposed so nothing's
		 * sent by mistake...
		 */
		if(outgoing->bcc)
		  he->display_it = 1;

		break;

	      case N_REPLYTO:
		sending_order[NN+1]	= pf;
		pf->addr		= &outgoing->reply_to;
		if(role && role->replyto){
		    if(outgoing->reply_to)
		      mail_free_address(&outgoing->reply_to);

		    outgoing->reply_to = copyaddrlist(role->replyto);
		    he->display_it  = 1;  /* show it */
		    he->rich_header = 0;
		}

		break;

	      case N_LCC:
		sending_order[NN+7]	= pf;
		pf->addr		= &lcc_addr;
		he_lcc			= he;
		if(lcc_arg){
		    build_address(lcc_arg, &addr, NULL, NULL, NULL);
		    rfc822_parse_adrlist(&lcc_addr, addr,
					 ps_global->maildomain);
		    fs_give((void **)&addr);
		    he->display_it = 1;
		}

		break;

#if	!(defined(DOS) || defined(OS2)) || defined(NOAUTH)
              case N_SENDER:
		sending_order[4]	= pf;
		pf->addr		= &outgoing->sender;
                break;
#endif

	      default:
		q_status_message1(SM_ORDER,3,7,
		    "Internal error: Address header %s", comatose(index));
		break;
	    }

	    /*
	     * If this is a reply to news, don't show the regular email
	     * recipient headers (unless they are non-empty).
	     */
	    if((outgoing->newsgroups && *outgoing->newsgroups)
	       && (index == N_TO || index == N_CC
		   || index == N_BCC || index == N_LCC)
	       && (pf->addr && !*pf->addr)){
		if((ct=set_default_hdrval(pf, custom)) >= UseAsDef &&
		   pf->textbuf && *pf->textbuf){
		    removing_trailing_white_space(pf->textbuf);
		    (void)removing_double_quotes(pf->textbuf);
		    build_address(pf->textbuf, &addr, NULL, NULL, NULL);
		    rfc822_parse_adrlist(pf->addr, addr,
					 ps_global->maildomain);
		    fs_give((void **)&addr);
		    if(ct > UseAsDef)
		      he->sticky = 1;
		}
		else
		  he->rich_header = 1; /* hide */
	    }

	    /*
	     * If this address doesn't already have a value, then we check
	     * for a default value assigned by the user.
	     */
	    else if(pf->addr && !*pf->addr){
		if((ct=set_default_hdrval(pf, custom)) >= UseAsDef &&
		   (index != N_FROM ||
		    (!ps_global->never_allow_changing_from &&
		     F_ON(F_ALLOW_CHANGING_FROM, ps_global))) &&
		   pf->textbuf && *pf->textbuf){

		    removing_trailing_white_space(pf->textbuf);
		    (void)removing_double_quotes(pf->textbuf);

		    /*
		     * Try to set To based on Lcc. Don't attempt Fcc.
		     */
		    if(index == N_LCC && !he_to->sticky && pf_to && pf_to->addr){
			BUILDER_ARG *barg = NULL;
			char *ppp = NULL;

			if(*pf_to->addr)
			  ppp = addr_list_string(*pf_to->addr, NULL, 1);

			if(!ppp)
			  ppp = cpystr("");

			barg = (BUILDER_ARG *) fs_get(sizeof(*barg));
			memset(barg, 0, sizeof(*barg));
			barg->me = &(he->bldr_private);
			barg->aff = &(he_to->bldr_private);
			barg->tptr = cpystr(ppp);

			build_addr_lcc(pf->textbuf, &addr, NULL, barg, NULL);
			he->display_it = 1;

			rfc822_parse_adrlist(pf->addr, addr,
					     ps_global->maildomain);
			if(addr)
			  fs_give((void **) &addr);

			if(ct > UseAsDef)
			  he->sticky = 1;

			if(barg && barg->tptr && strcmp(ppp, barg->tptr)){
			    ADDRESS *a = NULL;

			    rfc822_parse_adrlist(&a, barg->tptr,
						 ps_global->maildomain);
			    if(a){
				if(pf_to->addr)
				  mail_free_address(pf_to->addr);

				*pf_to->addr = a;
			    }
			}

			if(barg){
			    if(barg->tptr)
			      fs_give((void **) &barg->tptr);

			    fs_give((void **) &barg);
			}

			if(ppp)
			  fs_give((void **) &ppp);
		    }
		    else{
			build_address(pf->textbuf, &addr, NULL, NULL, NULL);
			rfc822_parse_adrlist(pf->addr, addr,
					     ps_global->maildomain);
			if(addr)
			  fs_give((void **) &addr);

			if(ct > UseAsDef)
			  he->sticky = 1;
		    }
		}

		/* if we still don't have a from */
		if(index == N_FROM && !*pf->addr)
		  *pf->addr = generate_from();
	    }

	    /*
	     * Addr is already set in the rest of the cases.
	     */
	    else if((index == N_FROM || index == N_REPLYTO) && pf->addr){
		ADDRESS *adr = NULL;

		/*
		 * We get to this case of the ifelse if the from or reply-to
		 * addr was set by a role above.
		 */

		/* figure out the default value */
		(void)set_default_hdrval(pf, custom);
		if(pf->textbuf && *pf->textbuf){
		    removing_trailing_white_space(pf->textbuf);
		    (void)removing_double_quotes(pf->textbuf);
		    build_address(pf->textbuf, &addr, NULL, NULL, NULL);
		    rfc822_parse_adrlist(&adr, addr,
					 ps_global->maildomain);
		    fs_give((void **)&addr);
		}

		/* if value set by role is different from default, show it */
		if(adr && !address_is_same(*pf->addr, adr))
		  he->display_it = 1;  /* start this off showing */

		/* malformed */
		if(!(*pf->addr)->mailbox){
		    fs_give((void **)pf->addr);
		    he->display_it = 1;
		}

		if(adr)
		  mail_free_address(&adr);
	    }
	    else if((index == N_TO || index == N_CC || index == N_BCC)
		    && pf->addr){
		ADDRESS *a = NULL, **tail;

		/*
		 * These three are different from the others because we
		 * might add the addresses to what is already there instead
		 * of replacing.
		 */

		switch(set_default_hdrval(pf, custom)){
		  case Replace:
		    if(*pf->addr)
		      mail_free_address(pf->addr);
		    
		    removing_trailing_white_space(pf->textbuf);
		    (void)removing_double_quotes(pf->textbuf);
		    build_address(pf->textbuf, &addr, NULL, NULL, NULL);
		    rfc822_parse_adrlist(pf->addr, addr, ps_global->maildomain);
		    fs_give((void **)&addr);
		    he->sticky = 1;
		    break;

		  case Combine:
		    removing_trailing_white_space(pf->textbuf);
		    (void)removing_double_quotes(pf->textbuf);
		    build_address(pf->textbuf, &addr, NULL, NULL, NULL);
		    rfc822_parse_adrlist(&a, addr, ps_global->maildomain);
		    fs_give((void **)&addr);
		    he->sticky = 1;
		    if(a){
			for(tail = pf->addr; *tail; tail = &(*tail)->next)
			  ;
			*tail = reply_cp_addr(ps_global, 0, NULL, NULL,
					      *pf->addr, NULL, a, RCA_ALL);
			q_status_message(SM_ORDER, 3, 3,
					 "Adding addresses from role");
			mail_free_address(&a);
		    }

		    break;

		  case UseAsDef:
		  case NoMatch:
		    break;
		}

		he->display_it = 1;  /* start this off showing */
	    }
	    else if(pf->addr){
		switch(set_default_hdrval(pf, custom)){
		  case Replace:
		  case Combine:
		    if(*pf->addr)
		      mail_free_address(pf->addr);
		    
		    removing_trailing_white_space(pf->textbuf);
		    (void)removing_double_quotes(pf->textbuf);
		    build_address(pf->textbuf, &addr, NULL, NULL, NULL);
		    rfc822_parse_adrlist(pf->addr, addr, ps_global->maildomain);
		    fs_give((void **)&addr);
		    he->sticky = 1;
		    break;

		  case UseAsDef:
		  case NoMatch:
		    break;
		}

		he->display_it = 1;
	    }

	    if(pf->addr && *pf->addr && !(*pf->addr)->mailbox){
		mail_free_address(pf->addr);
		he->display_it = 1;  /* start this off showing */
	    }

	    if(pf->textbuf)		/* free default value in any case */
	      fs_give((void **)&pf->textbuf);

	    /* outgoing2strings will alloc the string pf->scratch below */
	    he->realaddr = &pf->scratch;
	    break;

	  case Fcc:
	    sending_order[NN+8] = pf;
	    pf_fcc              = pf;
	    if(role && role->fcc)
	      fcc = role->fcc;
	    else
	      fcc = get_fcc(fcc_arg);

	    if(fcc_to_free){
		fs_give((void **)&fcc_to_free);
		fcc_arg = NULL;
	    }

	    if(((flags & PS_STICKY_FCC) && fcc[0]) || (role && role->fcc))
	      he->sticky = 1;

	    if(role)
	      role->fcc = NULL;

	    if(strcmp(fcc, ps_global->VAR_DEFAULT_FCC) != 0)
	      he->display_it = 1;  /* start this off showing */

	    he->realaddr  = &fcc;
	    pf->text      = &fcc;
	    he_fcc        = he;
	    break;

	  case Subject :
	    sending_order[NN+6]	= pf;
	    
	    switch(set_default_hdrval(pf, custom)){
	      case Replace:
	      case Combine:
		pf->scratch = pf->textbuf;
		pf->textbuf = NULL;
		he->sticky = 1;
		if(outgoing->subject)
		  fs_give((void **)&outgoing->subject);

		break;

	      case UseAsDef:
	      case NoMatch:
		/* if no value, use default */
		if(outgoing->subject){
		    pf->scratch = cpystr(outgoing->subject);
		}
		else{
		    pf->scratch = pf->textbuf;
		    pf->textbuf   = NULL;
		}

		break;
	    }

	    he->realaddr = &pf->scratch;
	    pf->text	 = &outgoing->subject;
	    break;

	  default:
	    q_status_message1(SM_ORDER,3,7,
			      "Unknown header type %d in pine_send",
			      (void *)pf->type);
	    break;
	}

	/*
	 * We may or may not want to give the user the chance to edit
	 * the From and Reply-To lines.  If they are listed in either
	 * Default-composer-hdrs or Customized-hdrs, then they can edit
	 * them, else no.
	 * If canedit is not set, that means that this header is not in
	 * the user's customized-hdrs.  If rich_header is set, that
	 * means that this header is not in the user's
	 * default-composer-hdrs (since From and Reply-To are rich
	 * by default).  So, don't give it an he to edit with in that case.
	 *
	 * For other types, just not setting canedit will cause it to be
	 * uneditable, regardless of what the user does.
	 */
	switch(index){
	  case N_FROM:
    /* to allow it, we let this fall through to the reply-to case below */
	    if(ps_global->never_allow_changing_from ||
	       (F_OFF(F_ALLOW_CHANGING_FROM, ps_global) &&
	        !(role && role->from))){
		if(pf->canedit || !he->rich_header)
		  q_status_message(SM_ORDER, 3, 3,
			_("Not allowed to change header \"From\""));

		memset(he, 0, (size_t)sizeof(*he));
		pf->extdata = NULL;
		break;
	    }

	  case N_REPLYTO:
	    if(!pf->canedit && he->rich_header){
	        memset(he, 0, (size_t)sizeof(*he));
		pf->extdata = NULL;
	    }
	    else{
		pf->canedit = 1;
		he++;
	    }

	    break;

	  default:
	    if(!pf->canedit){
	        memset(he, 0, (size_t)sizeof(*he));
		pf->extdata = NULL;
	    }
	    else
	      he++;

	    break;
	}
    }

    /*
     * This is so the builder can tell the composer to fill the affected
     * field based on the value in the field on the left.
     *
     * Note that this mechanism isn't completely general.  Each entry has
     * only a single next_affected, so if some other entry points an
     * affected entry at an entry with a next_affected, they all inherit
     * that next_affected.  Since this isn't used much a careful ordering
     * of the affected fields should make it a sufficient mechanism.
     */
    he_to->affected_entry   = he_fcc;
    he_news->affected_entry = he_fcc;
    he_lcc->affected_entry  = he_to;
    he_to->next_affected    = he_fcc;

    (--pf)->next = (total_cnt != fixed_cnt) ? header.custom : NULL;

    i--;  /* subtract one because N_ATTCH doesn't get a sending_order slot */
    /*
     * Set up headerentries for custom fields.
     * NOTE: "i" is assumed to now index first custom field in sending
     *       order.
     */
    for(pf = pf->next; pf && pf->name; pf = pf->next){
	char *addr;

	if(pf->standard)
	  continue;

	pf->extdata          = he;
	pf->canedit     = 1;
	pf->rcptto      = 0;
	pf->writehdr    = 1;
	pf->localcopy   = 1;
	
	switch(pf->type){
	  case Address:
	    if(pf->addr){				/* better be set */
		sending_order[i++] = pf;
		*he = he_custom_addr_templ;
		/* change default text into an ADDRESS */
		/* strip quotes around whole default */
		removing_trailing_white_space(pf->textbuf);
		(void)removing_double_quotes(pf->textbuf);
		build_address(pf->textbuf, &addr, NULL, NULL, NULL);
		rfc822_parse_adrlist(pf->addr, addr, ps_global->maildomain);
		fs_give((void **)&addr);
		if(pf->textbuf)
		  fs_give((void **)&pf->textbuf);

		he->realaddr = &pf->scratch;
		if(F_OFF(F_ENABLE_TAB_COMPLETE,ps_global))
		  he->nickcmpl = NULL;
	    }

	    break;

	  case FreeText:
	    sending_order[i++] = pf;
	    *he                = he_custom_free_templ;
	    he->realaddr       = &pf->textbuf;
	    pf->text           = &pf->textbuf;
	    if(((!pf->val || !pf->val[0]) && pf->textbuf && pf->textbuf[0]) ||
	       (pf->val && (!pf->textbuf || strcmp(pf->textbuf, pf->val))))
	      he->display_it  = 1;  /* show it */

	    break;

	  default:
	    q_status_message1(SM_ORDER,0,7,"Unknown custom header type %d",
							(void *)pf->type);
	    break;
	}

	he->name = pf->name;

	/* use first 8 characters for prompt */
	he->prompt = cpystr("        : ");
	strncpy(he->prompt, he->name, MIN(strlen(he->name), he->prwid - 2));

	he->rich_header = view_as_rich(he->name, he->rich_header);
	he++;
    }

    /*
     * Make sure at least *one* field is displayable...
     */
    for(index = -1, i=0, pf=header.local; pf && pf->name; pf=pf->next, i++)
      if(HE(pf) && !HE(pf)->rich_header){
	  index = i;
	  break;
      }

    /*
     * None displayable!!!  Warn and display defaults.
     */
    if(index == -1){
	q_status_message(SM_ORDER,0,5,
		     "No default-composer-hdrs matched, displaying defaults");
	for(i = 0, pf = header.local; pf; pf = pf->next, i++)
	  if((i == N_TO || i == N_CC || i == N_SUBJ || i == N_ATTCH)
	      && HE(pf))
	    HE(pf)->rich_header = 0;
    }

    /*
     * Save information about body which set_mime_type_by_grope might change.
     * Then, if we get an error sending, we reset these things so that
     * grope can do it's thing again after we edit some more.
     */
    if ((*body)->type == TYPEMULTIPART)
	bp = save_body_particulars(&(*body)->nested.part->body);
    else
        bp = save_body_particulars(*body);


    local_redraft_pos = redraft_pos;

    /*----------------------------------------------------------------------
       Loop calling the editor until everything goes well
     ----*/
    while(1){
	int saved_user_timeout;

	/* Reset body to what it was when we started. */
	if ((*body)->type == TYPEMULTIPART)
	    reset_body_particulars(bp, &(*body)->nested.part->body);
	else
	    reset_body_particulars(bp,*body);
	/*
	 * set initial cursor position based on how many times we've been
	 * thru the loop...
	 */
	if(reply && reply->pseudo){
	    pbf->pine_flags |= reply->data.pico_flags;
	}
	else if(body_start){
	    pbf->pine_flags |= P_BODY;
	    body_start = 0;		/* maybe not next time */
	}
	else if(local_redraft_pos){
	    pbf->edit_offset = local_redraft_pos->offset;
	    /* set the start_here bit in correct header */
	    for(pf = header.local; pf && pf->name; pf = pf->next)
	      if(strcmp(pf->name, local_redraft_pos->hdrname) == 0
		  && HE(pf)){
		  HE(pf)->start_here = 1;
		  break;
	      }
	    
	    /* If didn't find it, we start in body. */
	    if(!pf || !pf->name)
	      pbf->pine_flags |= P_BODY;
	}
	else if(reply && (!reply->forw && !reply->forwarded)){
	    pbf->pine_flags |= P_BODY;
	}

	/* in case these were turned on in previous pass through loop */
	if(pf_nobody){
	    pf_nobody->writehdr  = 0;
	    pf_nobody->localcopy = 0;
	}

	if(pf_fcc)
	  pf_fcc->localcopy = 0;

	/*
	 * If a sending attempt failed after we passed the message text
	 * thru a user-defined filter, "orig_so" points to the original
	 * text.  Replace the body's encoded data with the original...
	 */
	if(orig_so){
	    STORE_S **so = (STORE_S **)(((*body)->type == TYPEMULTIPART)
				? &(*body)->nested.part->body.contents.text.data
				: &(*body)->contents.text.data);
	    so_give(so);
	    *so     = orig_so;
	    orig_so = NULL;
	}

        /*
         * Convert the envelope and body to the string format that
         * pico can edit
         */
        outgoing2strings(&header, *body, &pbf->msgtext, &pbf->attachments, 0);

	for(pf = header.local; pf && pf->name; pf = pf->next){
	    /*
	     * If this isn't the first time through this loop, we may have
	     * freed some of the FreeText headers below so that they wouldn't
	     * show up as empty headers in the finished message.  Need to
	     * alloc them again here so they can be edited.
	     */
	    if(pf->type == FreeText && HE(pf) && !*HE(pf)->realaddr)
	      *HE(pf)->realaddr = cpystr("");

	    if(pf->type != Attachment && HE(pf) && *HE(pf)->realaddr)
	      HE(pf)->maxlen = strlen(*HE(pf)->realaddr);
	}

	/*
	 * If From is exposed, probably by a role, then start the cursor
	 * on the first line which isn't filled in. If it isn't, then we
	 * don't move the cursor, mostly for back-compat.
	 */
	if((!reply || reply->forw || reply->forwarded) &&
	   !local_redraft_pos && !(pbf->pine_flags & P_BODY) && he_from &&
	   (he_from->display_it || !he_from->rich_header)){
	    for(pf = header.local; pf && pf->name; pf = pf->next)
	      if(HE(pf) &&
		 (HE(pf)->display_it || !HE(pf)->rich_header) &&
		 HE(pf)->realaddr &&
		 (!*HE(pf)->realaddr || !**HE(pf)->realaddr)){
		  HE(pf)->start_here = 1;
		  break;
	      }
	}

#ifdef _WINDOWS
	mswin_setwindowmenu (MENU_COMPOSER);
#endif

	cancel_busy_cue(-1);
        flush_status_messages(1);

	/* turn off user input timeout when in composer */
	saved_user_timeout = ps_global->hours_to_timeout;
	ps_global->hours_to_timeout = 0;
	dprint((1, "\n  ---- COMPOSER ----\n"));
	editor_result = pico(pbf);
	dprint((4, "... composer returns (0x%x)\n", editor_result));
	ps_global->hours_to_timeout = saved_user_timeout;

#ifdef _WINDOWS
	mswin_setwindowmenu (MENU_DEFAULT);
#endif
	fix_windsize(ps_global);

	/*
	 * Only reinitialize signals if we didn't receive an interesting
	 * one while in pico, since pico's return is part of processing that
	 * signal and it should continue to be ignored.
	 */
	if(!(editor_result & COMP_GOTHUP))
	  init_signals();        /* Pico has it's own signal stuff */

	/*
	 * We're going to save in DEADLETTER.  Dump attachments first.
	 */
	if(editor_result & COMP_CANCEL)
	  free_attachment_list(&pbf->attachments);

        /* Turn strings back into structures */
        strings2outgoing(&header, body, pbf->attachments, flowing_requested);

        /* Make newsgroups NULL if it is "" (so won't show up in headers) */
	if(outgoing->newsgroups){
	    sqzspaces(outgoing->newsgroups);
	    if(!outgoing->newsgroups[0])
	      fs_give((void **)&(outgoing->newsgroups));
	}

        /* Make subject NULL if it is "" (so won't show up in headers) */
        if(outgoing->subject && !outgoing->subject[0])
          fs_give((void **)&(outgoing->subject)); 
	
	/* remove custom fields that are empty */
	for(pf = header.local; pf && pf->name; pf = pf->next){
	  if(pf->type == FreeText && pf->textbuf){
	    if(pf->textbuf[0] == '\0'){
		fs_give((void **)&pf->textbuf); 
		pf->text = NULL;
	    }
	  }
	}

        removing_trailing_white_space(fcc);

	/*-------- Stamp it with a current date -------*/
	if(outgoing->date)			/* update old date */
	  fs_give((void **)&(outgoing->date));

	if(F_ON(F_QUELL_TIMEZONE, ps_global))
	  mail_parameters(NULL, SET_DISABLE822TZTEXT, (void *) TRUE);

	rfc822_date(tmp_20k_buf);		/* format and copy new date */
	if(F_ON(F_QUELL_TIMEZONE, ps_global))
	  mail_parameters(NULL, SET_DISABLE822TZTEXT, (void *) FALSE);

	outgoing->date = (unsigned char *) cpystr(tmp_20k_buf);

	/* Set return_path based on From which is going to be used */
	if(outgoing->return_path)
	  mail_free_address(&outgoing->return_path);

	outgoing->return_path = rfc822_cpy_adr(outgoing->from);

	/*
	 * Don't ever believe the sender that is there.
	 * If From doesn't look quite right, generate our own sender.
	 */
	if(outgoing->sender)
	  mail_free_address(&outgoing->sender);

	/*
	 * If the LHS of the address doesn't match, or the RHS
	 * doesn't match one of localdomain or hostname,
	 * then add a sender line (really X-X-Sender).
	 *
	 * Don't add a personal_name since the user can change that.
	 */
	if(F_OFF(F_DISABLE_SENDER, ps_global)
	   &&
	   (!outgoing->from
	    || !outgoing->from->mailbox
	    || strucmp(outgoing->from->mailbox, ps_global->VAR_USER_ID) != 0
	    || !outgoing->from->host
	    || !(strucmp(outgoing->from->host, ps_global->localdomain) == 0
	    || strucmp(outgoing->from->host, ps_global->hostname) == 0))){

	    outgoing->sender	      = mail_newaddr();
	    outgoing->sender->mailbox = cpystr(ps_global->VAR_USER_ID);
	    outgoing->sender->host    = cpystr(ps_global->hostname);
	}

        /*----- Message is edited, now decide what to do with it ----*/
	if(editor_result & (COMP_SUSPEND | COMP_GOTHUP | COMP_CANCEL)){
            /*=========== Postpone or Interrupted message ============*/
	    CONTEXT_S *fcc_cntxt = NULL;
	    char       folder[MAXPATH+1];
	    int	       fcc_result = 0;
	    char       label[50];

	    dprint((4, "pine_send:%s handling\n",
		       (editor_result & COMP_SUSPEND)
			   ? "SUSPEND"
			   : (editor_result & COMP_GOTHUP)
			       ? "HUP"
			       : (editor_result & COMP_CANCEL)
				   ? "CANCEL" : "HUH?"));
	    if((editor_result & COMP_CANCEL)
	       && (F_ON(F_QUELL_DEAD_LETTER, ps_global)
	           || ps_global->deadlets == 0)){
		q_status_message(SM_ORDER, 0, 3, "Message cancelled");
		break;
	    }

	    /*
	     * The idea here is to use the Fcc: writing facility
	     * to append to the special postponed message folder...
	     *
	     * NOTE: the strategy now is to write the message and
	     * all attachments as they exist at composition time.
	     * In other words, attachments are postponed by value
	     * and not reference.  This may change later, but we'll
	     * need a local "message/external-body" type that
	     * outgoing2strings knows how to properly set up for
	     * the composer.  Maybe later...
	     */

	    label[0] = '\0';

	    if(F_ON(F_COMPOSE_REJECTS_UNQUAL, ps_global)
	       && (editor_result & COMP_SUSPEND)
	       && (check_addresses(&header) == CA_BAD)){
		/*--- Addresses didn't check out---*/
		q_status_message(SM_ORDER, 7, 7,
	      _("Not allowed to postpone message until addresses are qualified"));
		continue;
            }

	    /*
	     * Build the local message copy so.
	     * 
	     * In the HUP case, we'll write the bezerk delimiter by
	     * hand and output the message directly into the folder.
	     * It's not only faster, we don't have to worry about
	     * c-client reentrance and less hands paw over the data so
	     * there's less chance of a problem.
	     *
	     * In the Postpone case, just create it if the user wants to
	     * and create a temporary storage object to write into.  */
  fake_hup:
	    lmc.all_written = lmc.text_only = lmc.text_written = 0;
	    if(editor_result & (COMP_GOTHUP | COMP_CANCEL)){
		int    newfile = 1;
		time_t now = time((time_t *)0);

#if defined(DOS) || defined(OS2)
		/*
		 * we can't assume anything about root or home dirs, so
		 * just plunk it down in the same place as the pinerc
		 */
		if(!getenv("HOME")){
		    char *lc = last_cmpnt(ps_global->pinerc);
		    folder[0] = '\0';
		    if(lc != NULL){
			strncpy(folder,ps_global->pinerc,
				MIN(lc-ps_global->pinerc,sizeof(folder)-1));
			folder[MIN(lc-ps_global->pinerc,sizeof(folder)-1)]='\0';
		    }

		    strncat(folder, (editor_result & COMP_GOTHUP)
				     ? INTERRUPTED_MAIL : DEADLETTER,
			   sizeof(folder)-strlen(folder)-1);
		}
		else
#endif
		build_path(folder,
			   ps_global->VAR_OPER_DIR
			     ? ps_global->VAR_OPER_DIR : ps_global->home_dir,
			   (editor_result & COMP_GOTHUP)
			     ? INTERRUPTED_MAIL : DEADLETTER,
			   sizeof(folder));

		if(editor_result & COMP_CANCEL){
		  char filename[MAXPATH+1], newfname[MAXPATH+1], nbuf[5];

		  if(strlen(folder) + 1 < sizeof(filename))
		    for(i = ps_global->deadlets - 1; i > 0 && i < 9; i--){
			strncpy(filename, folder, sizeof(filename));
			filename[sizeof(filename)-1] = '\0';
			strncpy(newfname, filename, sizeof(newfname));
			newfname[sizeof(newfname)-1] = '\0';

			if(i > 1){
			    snprintf(nbuf, sizeof(nbuf), "%d", i);
			    nbuf[sizeof(nbuf)-1] = '\0';
			    strncat(filename, nbuf,
				    sizeof(filename)-strlen(filename)-1);
			    filename[sizeof(filename)-1] = '\0';
			}

			snprintf(nbuf,  sizeof(nbuf), "%d", i+1);
			nbuf[sizeof(nbuf)-1] = '\0';
			strncat(newfname, nbuf,
				sizeof(newfname)-strlen(newfname)-1);
			newfname[sizeof(newfname)-1] = '\0';
			(void) rename_file(filename, newfname);
		    }

		  our_unlink(folder);
		}
		else
		  newfile = can_access(folder, ACCESS_EXISTS);

		if((lmc.so = so_get(FCC_SOURCE, NULL, WRITE_ACCESS)) != NULL){
		  if (outgoing->from){
		    snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%sFrom %s@%s %.24s\015\012",
			      newfile ? "" : "\015\012",
			      outgoing->from->mailbox,
			      outgoing->from->host, ctime(&now));
		    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		    if(!so_puts(lmc.so, tmp_20k_buf)){
		      if(editor_result & COMP_CANCEL)
			q_status_message2(SM_ORDER | SM_DING, 3, 3,
					  "Can't write \"%s\": %s",
					  folder, error_description(errno));
		      else
			dprint((1, "* * * CAN'T WRITE %s: %s\n",
			       folder ? folder : "?",
			       error_description(errno)));
		    }
		  }
		}
	    }
	    else{			/* Must be COMP_SUSPEND */
		if(!ps_global->VAR_POSTPONED_FOLDER
		   || !ps_global->VAR_POSTPONED_FOLDER[0]){
		    q_status_message(SM_ORDER | SM_DING, 3, 3,
				     _("No postponed file defined"));
		    continue;
		}

		/*
		 * Store the cursor position
		 *
		 * First find the header entry with the start_here
		 * bit set, if any. This means the editor is telling
		 * us to start on this header field next time.
		 */
		start_here_name = NULL;
		for(pf = header.local; pf && pf->name; pf = pf->next)
		  if(HE(pf) && HE(pf)->start_here){
		      start_here_name = pf->name;
		      break;
		  }
		
		/* If there wasn't one, ":" means we start in the body. */
		if(!start_here_name || !*start_here_name)
		  start_here_name = ":";

		if(ps_global->VAR_FORM_FOLDER
		   && ps_global->VAR_FORM_FOLDER[0]
		   && postpone_prompt() == 'f'){
		    strncpy(folder, ps_global->VAR_FORM_FOLDER,
			    sizeof(folder)-1);
		    folder[sizeof(folder)-1] = '\0';
		    strncpy(label, "form letter", sizeof(label));
		    label[sizeof(label)-1] = '\0';
		}
		else{
		    strncpy(folder, ps_global->VAR_POSTPONED_FOLDER,
			    sizeof(folder)-1);
		    folder[sizeof(folder)-1] = '\0';
		    strncpy(label, "postponed message", sizeof(label));
		    label[sizeof(label)-1] = '\0';
		}

		lmc.so = open_fcc(folder,&fcc_cntxt, 1, NULL, NULL);
	    }

	    if(lmc.so){
		size_t sz;
		char *lmq = NULL;

		/* copy fcc line to postponed or interrupted folder */
	        if(pf_fcc)
		  pf_fcc->localcopy = 1;

		/* plug error into header for later display to user */
		if((editor_result & ~0xff) && (lmq = last_message_queued()) != NULL){
		    pf_err->writehdr  = 1;
		    pf_err->localcopy = 1;
		    pf_err->textbuf   = lmq;
		}

		/*
		 * if reply, write (UID)folder header field so we can
		 * later flag the replied-to message \\ANSWERED
		 * DON'T save MSGNO's.
		 */
		if(reply && reply->uid){
		    char uidbuf[MAILTMPLEN], *p;
		    long i;

		    for(i = 0L, p = tmp_20k_buf; reply->data.uid.msgs[i]; i++){
			if(i)
			  sstrncpy(&p, ",", SIZEOF_20KBUF-(p-tmp_20k_buf));

			sstrncpy(&p,ulong2string(reply->data.uid.msgs[i]),SIZEOF_20KBUF-(p-tmp_20k_buf));
		    }

		    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';

		    pf_uid->writehdr  = 1;
		    pf_uid->localcopy = 1;
		    snprintf(uidbuf, sizeof(uidbuf), "(%s%s%s)(%ld %lu %s)%s",
			     reply->prefix ? int2string(strlen(reply->prefix))
					   : (reply->forwarded) ? "": "0 ",
			     reply->prefix ? " " : "",
			     reply->prefix ? reply->prefix : "",
			     i, reply->data.uid.validity,
			     tmp_20k_buf, reply->mailbox);
		    uidbuf[sizeof(uidbuf)-1] = '\0';
		    pf_uid->textbuf   = cpystr(uidbuf);

		    /*
		     * Logically, this ought to be part of pf_uid, but this
		     * was added later and so had to be in a separate header
		     * for backwards compatibility.
		     */
		    pf_mbox->writehdr  = 1;
		    pf_mbox->localcopy = 1;
		    pf_mbox->textbuf   = cpystr(reply->origmbox
						  ? reply->origmbox
						  : reply->mailbox);
		}

		/* Save cursor position */
		if(start_here_name && *start_here_name){
		    char curposbuf[MAILTMPLEN];

		    pf_curpos->writehdr  = 1;
		    pf_curpos->localcopy = 1;
		    snprintf(curposbuf, sizeof(curposbuf), "%s %ld", start_here_name,
						 pbf->edit_offset);
		    curposbuf[sizeof(curposbuf)-1] = '\0';
		    pf_curpos->textbuf   = cpystr(curposbuf);
		}

		/*
		 * Work around c-client reply-to bug. C-client will
		 * return a reply_to in an envelope even if there is
		 * no reply-to header field. We want to note here whether
		 * the reply-to is real or not.
		 */
		if(outgoing->reply_to || hdr_is_in_list("reply-to", custom)){
		    pf_ourrep->writehdr  = 1;
		    pf_ourrep->localcopy = 1;
		    if(outgoing->reply_to)
		      pf_ourrep->textbuf   = cpystr("Full");
		    else
		      pf_ourrep->textbuf   = cpystr("Empty");
		}

		/* Save the role-specific smtp server */
		if(role && role->smtp && role->smtp[0]){
		    char  *q, *smtp = NULL;
		    char **lp;
		    size_t len = 0;

		    /*
		     * Turn the list of smtp servers into a space-
		     * delimited list in a single string.
		     */
		    for(lp = role->smtp; (q = *lp) != NULL; lp++)
		      len += (strlen(q) + 1);

		    if(len){
			smtp = (char *) fs_get(len * sizeof(char));
			smtp[0] = '\0';
			for(lp = role->smtp; (q = *lp) != NULL; lp++){
			    if(lp != role->smtp)
			      strncat(smtp, " ", len-strlen(smtp)-1);

			    strncat(smtp, q, len-strlen(smtp)-1);
			}
			
			smtp[len-1] = '\0';
		    }
		    
		    pf_smtp_server->writehdr  = 1;
		    pf_smtp_server->localcopy = 1;
		    if(smtp)
		      pf_smtp_server->textbuf = smtp;
		    else
		      pf_smtp_server->textbuf = cpystr("");
		}

		/* Save the role-specific nntp server */
		if(suggested_nntp_server || 
		   (role && role->nntp && role->nntp[0])){
		    char  *q, *nntp = NULL;
		    char **lp;
		    size_t len = 0;

		    if(role && role->nntp && role->nntp[0]){
			/*
			 * Turn the list of nntp servers into a space-
			 * delimited list in a single string.
			 */
			for(lp = role->nntp; (q = *lp) != NULL; lp++)
			  len += (strlen(q) + 1);

			if(len){
			    nntp = (char *) fs_get(len * sizeof(char));
			    nntp[0] = '\0';
			    for(lp = role->nntp; (q = *lp) != NULL; lp++){
				if(lp != role->nntp)
				  strncat(nntp, " ", len-strlen(nntp)-1);

				strncat(nntp, q, len-strlen(nntp)-1);
			    }
			
			    nntp[len-1] = '\0';
			}
		    }
		    else
		      nntp = cpystr(suggested_nntp_server);
		    
		    pf_nntp_server->writehdr  = 1;
		    pf_nntp_server->localcopy = 1;
		    if(nntp)
		      pf_nntp_server->textbuf = nntp;
		    else
		      pf_nntp_server->textbuf = cpystr("");
		}

		/*
		 * Write the list of custom headers to the
		 * X-Our-Headers header so that we can recover the
		 * list in redraft.
		 */
		sz = 0;
		for(pf = header.custom; pf && pf->name; pf = pf->next)
		  sz += strlen(pf->name) + 1;

		if(sz){
		    char *q;

		    pf_ourhdrs->writehdr  = 1;
		    pf_ourhdrs->localcopy = 1;
		    pf_ourhdrs->textbuf = (char *)fs_get(sz);
		    memset(pf_ourhdrs->textbuf, 0, sz);
		    q = pf_ourhdrs->textbuf;
		    for(pf = header.custom; pf && pf->name; pf = pf->next){
			if(pf != header.custom)
			  sstrncpy(&q, ",", sz-(q-pf_ourhdrs->textbuf));

			sstrncpy(&q, pf->name, sz-(q-pf_ourhdrs->textbuf));
		    }

		    pf_ourhdrs->textbuf[sz-1] = '\0';;
		}

		/*
		 * We need to make sure any header values that got cleared
		 * get written to the postponed message (they won't if
		 * pf->text is NULL).  Otherwise, we can't tell previously
		 * non-existent custom headers or default values from 
		 * custom (or other) headers that got blanked in the
		 * composer...
		 */
		for(pf = header.local; pf && pf->name; pf = pf->next)
		  if(pf->type == FreeText && HE(pf) && !*(HE(pf)->realaddr))
		    *(HE(pf)->realaddr) = cpystr("");

		/*
		 * We're saving the message for use later. It may be that the
		 * characters in the message are not all convertible to the
		 * user's posting_charmap. We'll save it as UTF-8 instead
		 * and worry about that the next time they try to send it.
		 * Use a different save pointer just to be sure we don't
		 * mess up the other stuff. We should probably make the
		 * charset an argument.
		 *
		 * We also need to fix the charset of the body part
		 * the user is editing so that we can read it back
		 * successfully when we resume the composition.
		 */
		ps_global->post_utf8 = 1;

		{
		    PARAMETER *pm;
		    BODY *bp;
		    if((*body)->type == TYPEMULTIPART)
		      bp = &(*body)->nested.part->body;
		    else
		      bp = *body;

		    for(pm = bp->parameter;
			pm && strucmp(pm->attribute, "charset") != 0;
			pm = pm->next)
		      ;

		    if(pm){
			if(pm->value)
			  fs_give((void **) &pm->value);

			pm->value = cpystr("UTF-8");
		    }
		}

		if(pine_rfc822_output(&header,*body,NULL,NULL) >= 0L){
		    if(editor_result & (COMP_GOTHUP | COMP_CANCEL)){
			char	*err;
			STORE_S *hup_so;
			gf_io_t	 gc, pc;
			int      we_cancel = 0;

			if(editor_result & COMP_CANCEL){
			    snprintf(tmp_20k_buf, SIZEOF_20KBUF,
				    "Saving to \"%s\"", folder);
			    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
			    we_cancel = busy_cue((char *)tmp_20k_buf, NULL, 1);
			}

			if((hup_so =
			    so_get(FileStar, folder, WRITE_ACCESS|OWNER_ONLY)) != NULL){
			    gf_set_so_readc(&gc, lmc.so);
			    gf_set_so_writec(&pc, hup_so);
			    so_seek(lmc.so, 0L, 0); 	/* read msg copy and */
			    so_seek(hup_so, 0L, 2);	/* append to folder  */
			    gf_filter_init();
			    gf_link_filter(gf_nvtnl_local, NULL);
			    if(!(fcc_result = !(err = gf_pipe(gc, pc))))
			      dprint((1, "*** PIPE FAILED: %s\n",
					 err ? err : "?"));

			    gf_clear_so_readc(lmc.so);
			    gf_clear_so_writec(hup_so);
			    so_give(&hup_so);
			}
			else
			  dprint((1, "*** CAN'T CREATE %s: %s\n",
				     folder ? folder : "?",
				     error_description(errno)));
			
			if(we_cancel)
			  cancel_busy_cue(-1);
		    }
		    else
		      fcc_result = write_fcc(folder, fcc_cntxt,
					     lmc.so, NULL, label, NULL);
		}

		/* discontinue coerced UTF-8 posting */
		ps_global->post_utf8 = 0;

		so_give(&lmc.so);
	    }
	    else
	      dprint((1, "***CAN'T ALLOCATE temp store: %s ",
			 error_description(errno)));

	    if(editor_result & COMP_GOTHUP){
		/*
		 * Special Hack #291: if any hi-byte bits are set in
		 *		      editor's result, we put them there.
		 */
		if(editor_result & 0xff00)
		  exit(editor_result >> 8);

		dprint((1, "Save composition on HUP %sED\n",
			   fcc_result ? "SUCCEED" : "FAIL"));
		hup_signal();		/* Do what we normally do on SIGHUP */
	    }
	    else if((editor_result & COMP_SUSPEND) && fcc_result){
		if(ps_global->VAR_FORM_FOLDER
		   && ps_global->VAR_FORM_FOLDER[0]
		   && !strcmp(folder, ps_global->VAR_FORM_FOLDER))
		  q_status_message(SM_ORDER, 0, 3,
	   _("Composition saved to Form Letter Folder. Select Compose to send."));
		else
		  q_status_message(SM_ORDER, 0, 3,
			 _("Composition postponed. Select Compose to resume."));

                break; /* postpone went OK, get out of here */
	    }
	    else if(editor_result & COMP_CANCEL){
		char *lc = NULL;

		if(fcc_result && folder)
		  lc = last_cmpnt(folder);

		q_status_message3(SM_ORDER, 0, 3,
				  _("Message cancelled%s%s%s"),
				  (lc && *lc) ? " and copied to \"" : "",
				  (lc && *lc) ? lc : "",
				  (lc && *lc) ? "\" file" : "");
		break;
            }
	    else{
		q_status_message(SM_ORDER, 0, 4,
		    _("Continuing composition.  Message not postponed or sent"));
		body_start = 1;
		continue; /* postpone failed, jump back in to composer */
            }
	}
	else{
	    /*------ Must be sending mail or posting ! -----*/
	    int	       result, valid_addr, continue_with_only_fcc = 0;
	    CONTEXT_S *fcc_cntxt = NULL;

	    result = 0;
	    dprint((4, "=== sending: "));

            /* --- If posting, confirm with user ----*/
	    if(outgoing->newsgroups && *outgoing->newsgroups
	       && F_OFF(F_QUELL_EXTRA_POST_PROMPT, ps_global)
	       && want_to(POST_PMT, 'n', 'n', NO_HELP, WT_NORM) == 'n'){
		q_status_message(SM_ORDER, 0, 3, _("Message not posted"));
		dprint((4, "no post, continuing\n"));
		continue;
	    }

	    if(!(outgoing->to || outgoing->cc || outgoing->bcc || lcc_addr
		 || outgoing->newsgroups)){
		if(fcc && fcc[0]){
		    if(F_OFF(F_AUTO_FCC_ONLY, ps_global) &&
		       want_to(_("No recipients, really copy only to Fcc "),
			       'n', 'n', h_send_fcc_only, WT_NORM) != 'y')
		      continue;
		    
		    continue_with_only_fcc++;
		}
		else{
		    q_status_message(SM_ORDER, 3, 4,
				     _("No recipients specified!"));
		    dprint((4, "no recip, continuing\n"));
		    continue;
		}
	    }

	    if((valid_addr = check_addresses(&header)) == CA_BAD){
		/*--- Addresses didn't check out---*/
		dprint((4, "addrs failed, continuing\n"));
		continue;
	    }

	    if(F_ON(F_WARN_ABOUT_NO_TO_OR_CC, ps_global)
	       && !continue_with_only_fcc
	       && !(outgoing->to || outgoing->cc || lcc_addr
		    || outgoing->newsgroups)
	       && (want_to(_("No To, Cc, or Newsgroup specified, send anyway "),
			   'n', 'n', h_send_check_to_cc, WT_NORM) != 'y')){
		dprint((4, "No To or CC or Newsgroup, continuing\n"));
		if(local_redraft_pos && local_redraft_pos != redraft_pos)
		  free_redraft_pos(&local_redraft_pos);
		
		local_redraft_pos
			= (REDRAFT_POS_S *) fs_get(sizeof(*local_redraft_pos));
		memset((void *) local_redraft_pos,0,sizeof(*local_redraft_pos));
		local_redraft_pos->hdrname = cpystr(TONAME);
		continue;
	    }

	    if(F_ON(F_WARN_ABOUT_NO_SUBJECT, ps_global)
	       && check_for_subject(&header) == CF_MISSING){
		dprint((4, "No subject, continuing\n"));
		if(local_redraft_pos && local_redraft_pos != redraft_pos)
		  free_redraft_pos(&local_redraft_pos);
		
		local_redraft_pos
			= (REDRAFT_POS_S *) fs_get(sizeof(*local_redraft_pos));
		memset((void *) local_redraft_pos,0,sizeof(*local_redraft_pos));
		local_redraft_pos->hdrname = cpystr(SUBJNAME);
		continue;
	    }

	    if(F_ON(F_WARN_ABOUT_NO_FCC, ps_global)
	       && check_for_fcc(fcc) == CF_MISSING){
		dprint((4, "No fcc, continuing\n"));
		if(local_redraft_pos && local_redraft_pos != redraft_pos)
		  free_redraft_pos(&local_redraft_pos);
		
		local_redraft_pos
			= (REDRAFT_POS_S *) fs_get(sizeof(*local_redraft_pos));
		memset((void *) local_redraft_pos,0,sizeof(*local_redraft_pos));
		local_redraft_pos->hdrname = cpystr("Fcc");
		continue;
	    }

	    set_last_fcc(fcc);

            /*---- Check out fcc -----*/
            if(fcc && *fcc){
		/*
		 * If special name "inbox" then replace it with the
		 * real inbox path.
		 */
		if(ps_global->VAR_INBOX_PATH
		   && strucmp(fcc, ps_global->inbox_name) == 0){
		    char *replace_fcc;

		    replace_fcc = cpystr(ps_global->VAR_INBOX_PATH);
		    fs_give((void **)&fcc);
		    fcc = replace_fcc;
		}

		lmc.all_written = lmc.text_written = 0;
		/* lmc.text_only set on command line */
	        if(!(lmc.so = open_fcc(fcc, &fcc_cntxt, 0, NULL, NULL))){
		    /* ---- Open or allocation of fcc failed ----- */
		    dprint((4,"can't open/allocate fcc, cont'g\n"));

		    /*
		     * Find field entry associated with fcc, and start
		     * composer on it...
		     */
		    for(pf = header.local; pf && pf->name; pf = pf->next)
		      if(pf->type == Fcc && HE(pf))
			HE(pf)->start_here = 1;

		    continue;
		}
		else
		  so_truncate(lmc.so, fcc_size_guess(*body) + 2048);
            }
	    else
	      lmc.so = NULL;

            /*---- Take care of any requested prefiltering ----*/
	    if(sending_filter_requested
	       && !filter_message_text(sending_filter_requested, outgoing,
				       *body, &orig_so, &header)){
		q_status_message1(SM_ORDER, 3, 3,
				 _("Problem filtering!  Nothing sent%s."),
				 fcc ? " or saved to fcc" : "");
		continue;
	    }

            /*------ Actually post  -------*/
            if(outgoing->newsgroups){
		char **alt_nntp = NULL, *alt_nntp_p[2];
		if(((role && role->nntp)
		    || suggested_nntp_server)){
		    if(ps_global->FIX_NNTP_SERVER
		       && ps_global->FIX_NNTP_SERVER[0])
		      q_status_message(SM_ORDER | SM_DING, 5, 5,
				       "Using nntp-server that is administratively fixed");
		    else if(role && role->nntp)
		      alt_nntp = role->nntp;
		    else{
			alt_nntp_p[0] = suggested_nntp_server;
			alt_nntp_p[1] = NULL;
			alt_nntp = alt_nntp_p;
		    }
		}
		if(news_poster(&header, *body, alt_nntp, pipe_callback) < 0){
		    dprint((1, "Post failed, continuing\n"));
		    if(outgoing->message_id)
		      fs_give((void **) &outgoing->message_id);

		    outgoing->message_id = generate_message_id();

		    continue;
		}
		else
		  result |= P_NEWS_WIN;
	    }

	    /*
	     * BUG: IF we've posted the message *and* an fcc was specified
	     * then we've already got a neatly formatted message in the
	     * lmc.so.  It'd be nice not to have to re-encode everything
	     * to insert it into the smtp slot...
	     */

	    /*
	     * Turn on "undisclosed recipients" header if no To or cc.
	     */
            if(!(outgoing->to || outgoing->cc)
	      && (outgoing->bcc || lcc_addr) && pf_nobody && pf_nobody->addr){
		pf_nobody->writehdr  = 1;
		pf_nobody->localcopy = 1;
	    }

	    if(priority_requested){
		(void) set_priority_header(&header, priority_requested);
		fs_give((void **) &priority_requested);
	    }

#if	defined(BACKGROUND_POST) && defined(SIGCHLD)
	    /*
	     * If requested, launch backgroud posting...
	     */
	    if(background_requested && !(call_mailer_flags & CM_VERBOSE)){
		ps_global->post = (POST_S *)fs_get(sizeof(POST_S));
		memset(ps_global->post, 0, sizeof(POST_S));
		if(fcc)
		  ps_global->post->fcc = cpystr(fcc);

		if((ps_global->post->pid = fork()) == 0){
		    /*
		     * Put us in new process group...
		     */
		    setpgrp(0, ps_global->post->pid);

		    /* BUG: should fix argv[0] to indicate what we're up to */

		    /*
		     * If there are any live streams, pretend we never
		     * knew them.  Problem is two processes writing
		     * same server process.
		     * This is not clean but we're just going to exit
		     * right away anyway. We just want to be sure to leave
		     * the stuff that the parent is going to use alone.
		     * The next three lines will disable the re-use of the
		     * existing streams and cause us to open a new one if
		     * needed.
		     */
		    ps_global->mail_stream = NULL;
		    ps_global->s_pool.streams = NULL;
		    ps_global->s_pool.nstream = 0;

		    /* quell any display output */
		    ps_global->in_init_seq = 1;

		    /*------- Actually mail the message ------*/
		    if(valid_addr == CA_OK 
		       && (outgoing->to || outgoing->cc
			   || outgoing->bcc || lcc_addr)){
			char **alt_smtp = NULL;

			if(role && role->smtp){
			    if(ps_global->FIX_SMTP_SERVER
			       && ps_global->FIX_SMTP_SERVER[0])
			      q_status_message(SM_ORDER | SM_DING, 5, 5, "Use of a role-defined smtp-server is administratively prohibited");
			    else
			      alt_smtp = role->smtp;
			}

		        result |= (call_mailer(&header, *body, alt_smtp,
					       call_mailer_flags,
					       call_mailer_file_result,
					       pipe_callback) > 0)
				    ? P_MAIL_WIN : P_MAIL_LOSE;

			if(result & P_MAIL_LOSE)
			  mark_address_failure_for_pico(&header);
		    }

		    /*----- Was there an fcc involved? -----*/
		    if(lmc.so){
			/*------ Write it if at least something worked ------*/
			if((result & (P_MAIL_WIN | P_NEWS_WIN))
			   || (!(result & (P_MAIL_BITS | P_NEWS_BITS))
			       && pine_rfc822_output(&header, *body,
						     NULL, NULL))){
			    char label[50];

			    strncpy(label, "Fcc", sizeof(label));
			    label[sizeof(label)-1] = '\0';
			    if(strcmp(fcc, ps_global->VAR_DEFAULT_FCC)){
				snprintf(label + 3, sizeof(label)-3, " to %s", fcc);
				label[sizeof(label)-1] = '\0';
			    }

			    /*-- Now actually copy to fcc folder and close --*/
			    result |= (write_fcc(fcc, fcc_cntxt, lmc.so,
						 NULL, label,
					      F_ON(F_MARK_FCC_SEEN, ps_global)
						  ? "\\SEEN" : NULL))
					? P_FCC_WIN : P_FCC_LOSE;
			}
			else if(!(result & (P_MAIL_BITS | P_NEWS_BITS))){
			    q_status_message(SM_ORDER, 3, 5,
					    _("Fcc Failed!.  No message saved."));
			    dprint((1,
				       "explicit fcc write failed!\n"));
			    result |= P_FCC_LOSE;
			}

			so_give(&lmc.so);
		    }

		    if(result & (P_MAIL_LOSE | P_NEWS_LOSE | P_FCC_LOSE)){
			/*
			 * Encode child's result in hi-byte of
			 * editor's result
			 */
			editor_result = ((result << 8) | COMP_GOTHUP);
			goto fake_hup;
		    }

		    exit(result);
		}

		if(ps_global->post->pid > 0){
		    q_status_message(SM_ORDER, 3, 3,
				     _("Message handed off for posting"));
		    break;		/* up to our child now */
		}
		else{
		    q_status_message1(SM_ORDER | SM_DING, 3, 3,
				      "Can't fork for send: %s",
				      error_description(errno));
		    if(ps_global->post->fcc)
		      fs_give((void **) &ps_global->post->fcc);

		    fs_give((void **) &ps_global->post);
		}

		if(lmc.so)	/* throw away unused store obj */
		  so_give(&lmc.so);

		if(outgoing->message_id)
		  fs_give((void **) &outgoing->message_id);

		outgoing->message_id = generate_message_id();

		continue;		/* if we got here, there was a prob */
	    }
#endif /* BACKGROUND_POST */

            /*------- Actually mail the message ------*/
            if(valid_addr == CA_OK
	       && (outgoing->to || outgoing->cc || outgoing->bcc || lcc_addr)){
		char **alt_smtp = NULL;

		if(role && role->smtp){
		    if(ps_global->FIX_SMTP_SERVER
		       && ps_global->FIX_SMTP_SERVER[0])
		      q_status_message(SM_ORDER | SM_DING, 5, 5, "Use of a role-defined smtp-server is administratively prohibited");
		    else
		      alt_smtp = role->smtp;
		}

		result |= (call_mailer(&header, *body, alt_smtp,
				       call_mailer_flags,
				       call_mailer_file_result,
				       pipe_callback) > 0)
			    ? P_MAIL_WIN : P_MAIL_LOSE;

		if(result & P_MAIL_LOSE)
		  mark_address_failure_for_pico(&header);
	    }

	    /*----- Was there an fcc involved? -----*/
            if(lmc.so){
		/*------ Write it if at least something worked ------*/
		if((result & (P_MAIL_WIN | P_NEWS_WIN))
		   || (!(result & (P_MAIL_BITS | P_NEWS_BITS))
		       && pine_rfc822_output(&header, *body, NULL, NULL))){
		    char label[50];

		    strncpy(label, "Fcc", sizeof(label));
		    label[sizeof(label)-1] = '\0';
		    if(strcmp(fcc, ps_global->VAR_DEFAULT_FCC)){
			snprintf(label + 3, sizeof(label)-3, " to %s", fcc);
			label[sizeof(label)-1] = '\0';
		    }

		    /*-- Now actually copy to fcc folder and close --*/
		    result |= (write_fcc(fcc, fcc_cntxt, lmc.so, NULL, label,
					 F_ON(F_MARK_FCC_SEEN, ps_global)
					    ? "\\SEEN" : NULL))
				? P_FCC_WIN : P_FCC_LOSE;
		}
		else if(!(result & (P_MAIL_BITS | P_NEWS_BITS))){
		    q_status_message(SM_ORDER,3,5,
			_("Fcc Failed!.  No message saved."));
		    dprint((1, "explicit fcc write failed!\n"));
		    result |= P_FCC_LOSE;
		}

		so_give(&lmc.so);
	    }

            /*----- Mail Post FAILED, back to composer -----*/
            if(result & (P_MAIL_LOSE | P_FCC_LOSE)){
		dprint((1, "Send failed, continuing\n"));

		if(result & P_FCC_LOSE){
		    /*
		     * Find field entry associated with fcc, and start
		     * composer on it...
		     */
		    for(pf = header.local; pf && pf->name; pf = pf->next)
		      if(pf->type == Fcc && HE(pf))
			HE(pf)->start_here = 1;

		    q_status_message(SM_ORDER | SM_DING, 3, 3,
				     pine_send_status(result, fcc,
						      tmp_20k_buf, SIZEOF_20KBUF, NULL));
		}

		if(outgoing->message_id)
		  fs_give((void **) &outgoing->message_id);

		outgoing->message_id = generate_message_id();

		continue;
	    }

	    /*
	     * If message sent *completely* successfully, there's a
	     * reply struct AND we're allowed to write back state, do it.
	     * But also protect against shifted message numbers due 
	     * to new mail arrival.  Since the number passed is based
	     * on the real imap msg no, AND we're sure no expunge has 
	     * been done, just fix up the sorted number...
	     */
	    update_answered_flags(reply);

            /*----- Signed, sealed, delivered! ------*/
	    q_status_message(SM_ORDER, 0, 3,
			     pine_send_status(result, fcc, tmp_20k_buf, SIZEOF_20KBUF, NULL));

            break; /* All's well, pop out of here */
        }
    }

    if(orig_so)
      so_give(&orig_so);

    if(fcc)
      fs_give((void **)&fcc);
    
    free_body_particulars(bp);

    free_attachment_list(&pbf->attachments);

    standard_picobuf_teardown(pbf);

    for(i=0; i < fixed_cnt; i++){
	if(pfields[i].textbuf)
	  fs_give((void **)&pfields[i].textbuf);

	fs_give((void **)&pfields[i].name);
    }

    if(lcc_addr)
      mail_free_address(&lcc_addr);
    
    if(nobody_addr)
      mail_free_address(&nobody_addr);

    free_prompts(header.custom);
    free_customs(header.custom);
    fs_give((void **)&pfields);
    free_headents(&headents);
    fs_give((void **)&sending_order);
    if(suggested_nntp_server)
      fs_give((void **)&suggested_nntp_server);
    if(title)
      fs_give((void **)&title);
    
    if(local_redraft_pos && local_redraft_pos != redraft_pos)
      free_redraft_pos(&local_redraft_pos);

    pbf = save_previous_pbuf;
    g_rolenick = NULL;

    dprint((4, "=== send returning ===\n"));
}


/*
 * Check for subject in outgoing message.
 * 
 * Asks user whether to proceed with no subject.
 */
int
check_for_subject(METAENV *header)
{
    PINEFIELD *pf;
    int        rv = CF_OK;

    for(pf = header->local; pf && pf->name; pf = pf->next)
      if(pf->type == Subject){
	  if(pf->text && *pf->text && **pf->text)
	    rv = CF_OK;
	  else{
	      if(want_to("No Subject, send anyway ",
		         'n', 'n', h_send_check_subj, WT_NORM) == 'y') 
		rv = CF_OK;
	      else
		rv = CF_MISSING;
	  }

	  break;
      }
	      

    return(rv);
}


/*
 * Check for fcc in outgoing message.
 * 
 * Asks user whether to proceed with no fcc.
 */
int
check_for_fcc(char *fcc)
{
    int        rv = CF_OK;

    if(fcc && *fcc)
      rv = CF_OK;
    else{
	if(want_to("No Fcc, send anyway ", 'n', 'n', h_send_check_fcc, WT_NORM) == 'y') 
	  rv = CF_OK;
	else
	  rv = CF_MISSING;
    }

    return(rv);
}


/*
 * Confirm that the user wants to send to MAILER-DAEMON
 */
int
confirm_daemon_send(void)
{
    return(want_to("Really send this message to the MAILER-DAEMON",
		   'n', 'n', NO_HELP, WT_NORM) == 'y');
}


void
free_prompts(PINEFIELD *head)
{
    PINEFIELD *pf;

    for(pf = head; pf && pf->name; pf = pf->next){
	if(HE(pf) && HE(pf)->prompt)
	  fs_give((void **)& HE(pf)->prompt);
    }
}


int
postpone_prompt(void)
{
    static ESCKEY_S pstpn_form_opt[] = { {'p', 'p', "P", N_("Postponed Folder")},
					 {'f', 'f', "F", N_("Form Letter Folder")},
					 {-1, 0, NULL, NULL} };

    return(radio_buttons(PSTPN_FORM_PMT, -FOOTER_ROWS(ps_global),
			 pstpn_form_opt, 'p', 0, NO_HELP, RB_FLUSH_IN));
}


/*
 * call__mailer_file_result - some results from call_mailer might be in a file.
 *			      dislplay that file.
 */
void
call_mailer_file_result(char *filename, int style)
{
    if(filename){
	if(style & CM_BR_VERBOSE){
	    display_output_file(filename, "Verbose SMTP Interaction", NULL, DOF_BRIEF);
	}
	else{
	    display_output_file(filename, "POSTING ERRORS", "Posting Error", DOF_EMPTY);
	}
    }
}

void
mark_address_failure_for_pico(METAENV *header)
{
    PINEFIELD	       *pf;
    ADDRESS	       *a;
    int			error_count = 0;
    struct headerentry *last_he = NULL;

    for(pf = header->local; pf && pf->name; pf = pf->next)
      if(pf->type == Address && pf->rcptto && pf->addr && *pf->addr)
	for(a = *pf->addr; a != NULL; a = a->next)
	  if(a->error != NULL
	     && error_count++ < MAX_ADDR_ERROR
	     && (HE(pf))){
	      if(last_he)	/* start last reported err */
		last_he->start_here = 0;

	      (last_he = HE(pf))->start_here = 1;
	  }
}



/*
 * This is specialized routine. It assumes that the only things that we
 * care about restoring are the body type, subtype, encoding and the
 * state of the charset parameter. It also assumes that if the charset
 * parameter exists when we save it, it won't be removed later.
 */
BODY_PARTICULARS_S *
save_body_particulars(struct mail_bodystruct *body)
{
    BODY_PARTICULARS_S *bp;
    PARAMETER *pm;

    bp = (BODY_PARTICULARS_S *)fs_get(sizeof(BODY_PARTICULARS_S));

    bp->type      = body->type;
    bp->encoding  = body->encoding;
    bp->subtype   = body->subtype ? cpystr(body->subtype) : NULL;
    bp->parameter = body->parameter;
    for(pm = bp->parameter;
	pm && strucmp(pm->attribute, "charset") != 0;
	pm = pm->next)
      ;/* searching for possible charset parameter */
    
    if(pm){					/* found one */
	bp->had_csp = 1;		/* saved body had charset parameter */
	bp->charset = pm->value ? cpystr(pm->value) : NULL;
    }
    else{
	bp->had_csp = 0;
	bp->charset = NULL;
    }

    return(bp);
}


void
reset_body_particulars(BODY_PARTICULARS_S *bp, struct mail_bodystruct *body)
{
    body->type     = bp->type;
    body->encoding = bp->encoding;
    if(body->subtype)
      fs_give((void **)&body->subtype);

    body->subtype  = bp->subtype ? cpystr(bp->subtype) : NULL;

    if(bp->parameter){
	PARAMETER *pm, *pm_prev = NULL;

	for(pm = body->parameter;
	    pm && strucmp(pm->attribute, "charset") != 0;
	    pm = pm->next)
	  pm_prev = pm;

	if(pm){				/* body has charset parameter */
	    if(bp->had_csp){		/* reset to what it used to be */
		if(pm->value)
		  fs_give((void **)&pm->value);

		pm->value = bp->charset ? cpystr(bp->charset) : NULL;
	    }
	    else{			/* remove charset parameter */
		if(pm_prev)
		  pm_prev->next = pm->next;
		else
		  body->parameter = pm->next;

		mail_free_body_parameter(&pm);
	    }
	}
	else{
	    if(bp->had_csp){
		/*
		 * This can't happen because grope never removes
		 * the charset parameter.
		 */
		q_status_message(SM_ORDER | SM_DING, 5, 5,
"Programmer error: saved charset but no current charset param in pine_send");
	    }
	    /*
	    else{
		ok, still no parameter 
	    }
	    */
	}
    }
    else{
	if(body->parameter)
	  mail_free_body_parameter(&body->parameter);
	
	body->parameter = NULL;
    }
}


void
free_body_particulars(BODY_PARTICULARS_S *bp)
{
    if(bp){
	if(bp->subtype)
	  fs_give((void **)&bp->subtype);

	if(bp->charset)
	  fs_give((void **)&bp->charset);

	fs_give((void **)&bp);
    }
}


/*----------------------------------------------------------------------
   Build a status message suitable for framing

Returns: pointer to resulting buffer
  ---*/
char *
pine_send_status(int result, char *fcc_name, char *buf, size_t buflen, int *goodorbad)
{
    int   avail = ps_global->ttyo->screen_cols - 2;
    int   fixedneed, need, lenfcc;
    char *part1, *part2, *part3, *part4, *part5;
    char  fbuf[MAILTMPLEN+1];

    part1 = (result & P_NEWS_WIN)
	     ? "posted"
	     : (result & P_NEWS_LOSE)
	      ? "NOT POSTED"
	      : "";
    part2 = ((result & P_NEWS_BITS) && (result & P_MAIL_BITS)
	     && (result & P_FCC_BITS))
	     ? ", "
	     : ((result & P_NEWS_BITS) && (result & (P_MAIL_BITS | P_FCC_BITS)))
	      ? " and "
	      : "";
    part3 = (result & P_MAIL_WIN)
	     ? "sent"
	     : (result & P_MAIL_LOSE)
	      ? "NOT SENT"
	      : "";
    part4 = ((result & P_MAIL_BITS) && (result & P_FCC_BITS))
	     ? " and "
	     : "";
    part5 = ((result & P_FCC_WIN) && !(result & (P_MAIL_WIN | P_NEWS_WIN)))
	     ? "ONLY copied to " 
	     : (result & P_FCC_WIN)
	      ? "copied to "
	      : (result & P_FCC_LOSE)
	       ? "NOT copied to "
	       : "";
    lenfcc = MIN(sizeof(fbuf)-1, (result & P_FCC_BITS) ? strlen(fcc_name) : 0);
    
    fixedneed = 9 + strlen(part1) + strlen(part2) + strlen(part3) +
	strlen(part4) + strlen(part5);
    need = fixedneed + ((result & P_FCC_BITS) ? 2 : 0) + lenfcc;

    if(need > avail && fixedneed + 3 >= avail){
	/* dots on end of fixed, no fcc */
	snprintf(fbuf, sizeof(fbuf), "Message %s%s%s%s%s     ",
		part1, part2, part3, part4, part5);
	short_str(fbuf, buf, buflen, avail, EndDots);
    }
    else if(need > avail){
	/* include whole fixed part, quotes and dots at end of fcc name */
	if(lenfcc > 0)
	  lenfcc = MAX(1, lenfcc-(need-avail));

	snprintf(buf, buflen, "Message %s%s%s%s%s%s%s%s.",
		part1, part2, part3, part4, part5,
		(result & P_FCC_BITS) ? "\"" : "",
		short_str((result & P_FCC_BITS) ? fcc_name : "",
			  fbuf, sizeof(fbuf), lenfcc, FrontDots), 
		(result & P_FCC_BITS) ? "\"" : "");
    }
    else{
	/* whole thing */
	snprintf(buf, buflen, "Message %s%s%s%s%s%s%s%s.",
		part1, part2, part3, part4, part5,
		(result & P_FCC_BITS) ? "\"" : "",
		(result & P_FCC_BITS) ? fcc_name : "",
		(result & P_FCC_BITS) ? "\"" : "");
    }

    if(goodorbad)
      *goodorbad = (result & (P_MAIL_LOSE | P_NEWS_LOSE | P_FCC_LOSE)) == 0;

    return(buf);
}


/*----------------------------------------------------------------------
    Call back for pico to insert the specified message's text
     
Args: n -- message number to format
      f -- function to use to output the formatted message
 

Returns: returns msg number formatted on success, zero on error.
----*/      
long
message_format_for_pico(long int n, int (*f) (int))
{
    ENVELOPE *e;
    BODY     *b;
    char     *old_quote = NULL;
    long      rv = n;

    if(!(n > 0L && n <= mn_get_total(ps_global->msgmap)
       && (e = pine_mail_fetchstructure(ps_global->mail_stream,
				        mn_m2raw(ps_global->msgmap, n), &b)))){
	q_status_message(SM_ORDER|SM_DING,3,3,"Error inserting Message");
	flush_status_messages(0);
	return(0L);
    }

    /* temporarily assign a new quote string */
    old_quote = pbf->quote_str;
    pbf->quote_str = reply_quote_str(e);

    /* build separator line */
    reply_delimiter(e, NULL, f);

    /* actually write message text */
    if(!format_message(mn_m2raw(ps_global->msgmap, n), e, b, NULL,
		       FM_NEW_MESS | FM_DISPLAY | FM_NOCOLOR | FM_NOINDENT, f)){
	q_status_message(SM_ORDER|SM_DING,3,3,"Error inserting Message");
	flush_status_messages(0);
	rv = 0L;
    }

    fs_give((void **)&pbf->quote_str);
    pbf->quote_str = old_quote;
    return(rv);
}


/*----------------------------------------------------------------------
    Call back for pico to prompt the user for exit confirmation

Args: dflt -- default answer for confirmation prompt

Returns: either NULL if the user accepts exit, or string containing
	 reason why the user declined.
----*/      
int
send_exit_for_pico(struct headerentry *he, void (*redraw_pico)(void), int allow_flowed,
		   char **result)
{
    int	       i, rv, c, verbose_label = 0, bg_label = 0, old_suspend;
    int        dsn_label = 0, fcc_label = 0, lparen;
    int        flowing_label = 0, double_rad;
    char      *rstr = NULL, *p, *lc, *optp;
    char       dsn_string[30];
    void     (*redraw)(void) = ps_global->redrawer;
    ESCKEY_S   opts[18];
    struct filters {
	char  *filter;
	int    index;
	struct filters *prev, *next;
    } *filters = NULL, *fp;

    sending_filter_requested = NULL;
    call_mailer_flags	     = 0;
    background_requested     = 0;
    flowing_requested        = allow_flowed ? 1 : 0;
    lmc.text_only            = F_ON(F_NO_FCC_ATTACH, ps_global) != 0;
    if(priority_requested)
      fs_give((void **) &priority_requested);

    if(background_posting(FALSE)){
	if(result)
	  *result = "Can't send while background posting. Use postpone.";

	return(1);
    }

    if(F_ON(F_SEND_WO_CONFIRM, ps_global)){
	if(result)
	  *result = NULL;

	return(0);
    }

    ps_global->redrawer = redraw_pico;

    if((old_suspend = F_ON(F_CAN_SUSPEND, ps_global)) != 0)
      (void) F_SET(F_CAN_SUSPEND, ps_global, 0);

    /*
     * Build list of available filters...
     */
    for(i=0; ps_global->VAR_SEND_FILTER && ps_global->VAR_SEND_FILTER[i]; i++){
	for(p = ps_global->VAR_SEND_FILTER[i];
	    *p && !isspace((unsigned char)*p); p++)
	  ;

	c  = *p;
	*p = '\0';
	if(!(is_absolute_path(ps_global->VAR_SEND_FILTER[i])
	     && can_access(ps_global->VAR_SEND_FILTER[i],EXECUTE_ACCESS) ==0)){
	    *p = c;
	    continue;
	}

	fp	   = (struct filters *)fs_get(sizeof(struct filters));
	fp->index  = i;
	if((lc = last_cmpnt(ps_global->VAR_SEND_FILTER[i])) != NULL){
	    fp->filter = cpystr(lc);
	}
	else if((p - ps_global->VAR_SEND_FILTER[i]) > 20){
	    snprintf(tmp_20k_buf, SIZEOF_20KBUF, "...%s", p - 17);
	    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	    fp->filter = cpystr(tmp_20k_buf);
	}
	else
	  fp->filter = cpystr(ps_global->VAR_SEND_FILTER[i]);

	*p = c;

	if(filters){
	    fp->next	   = filters;
	    fp->prev	   = filters->prev;
	    fp->prev->next = filters->prev = fp;
	}
	else{
	    filters = (struct filters *)fs_get(sizeof(struct filters));
	    filters->index  = -1;
	    filters->filter = NULL;
	    filters->next = filters->prev = fp;
	    fp->next = fp->prev = filters;
	}
    }

    i = 0;
    opts[i].ch      = 'y';
    opts[i].rval    = 'y';
    opts[i].name    = "Y";
    opts[i++].label = N_("Yes");

    opts[i].ch      = 'n';
    opts[i].rval    = 'n';
    opts[i].name    = "N";
    opts[i++].label = N_("No");

    if(filters){
	/* set global_filter_pointer to desired filter or NULL if none */
	/* prepare two keymenu slots for selecting filter */
	opts[i].ch      = ctrl('P');
	opts[i].rval    = 10;
	opts[i].name    = "^P";
	opts[i++].label = N_("Prev Filter");

	opts[i].ch      = ctrl('N');
	opts[i].rval    = 11;
	opts[i].name    = "^N";
	opts[i++].label = N_("Next Filter");

	if(F_ON(F_FIRST_SEND_FILTER_DFLT, ps_global))
	  filters = filters->next;
    }

    if(F_ON(F_VERBOSE_POST, ps_global)){
	/* setup keymenu slot to toggle verbose mode */
	opts[i].ch    = ctrl('W');
	opts[i].rval  = 12;
	opts[i].name  = "^W";
	verbose_label = i++;
    }

    if(allow_flowed){
	/* setup keymenu slot to toggle flowed mode */
	opts[i].ch    = ctrl('V');
	opts[i].rval  = 22;
	opts[i].name  = "^V";
	flowing_label = i++;
	flowing_requested = 1;
    }

    if(F_ON(F_NO_FCC_ATTACH, ps_global)){
	/* setup keymenu slot to toggle attacment on fcc */
	opts[i].ch      = ctrl('F');
	opts[i].rval    = 21;
	opts[i].name    = "^F";
	fcc_label       = i++;
    }

#if	defined(BACKGROUND_POST) && defined(SIGCHLD)
    if(F_ON(F_BACKGROUND_POST, ps_global)){
	opts[i].ch    = ctrl('R');
	opts[i].rval  = 15;
	opts[i].name  = "^R";
	bg_label = i++;
    }
#endif

    opts[i].ch      = 'p';
    opts[i].rval    = 'p';
    opts[i].name    = "P";
    opts[i++].label = N_("Priority");

#ifdef SMIME
    if(F_OFF(F_DONT_DO_SMIME, ps_global)){
    	opts[i].ch  	= 'e';
	opts[i].rval	= 'e';
	opts[i].name	= "E";
	opts[i++].label	= "Encrypt";
    
    	opts[i].ch  	= 'g';
	opts[i].rval	= 'g';
	opts[i].name	= "G";
	opts[i++].label	= "Sign";

	if(ps_global->smime){
	    ps_global->smime->do_encrypt = F_ON(F_ENCRYPT_DEFAULT_ON, ps_global);
	    ps_global->smime->do_sign = F_ON(F_SIGN_DEFAULT_ON, ps_global);
	}
    }		
#endif

    double_rad = i;

    if(F_ON(F_DSN, ps_global)){
	/* setup keymenu slots to toggle dsn bits */
	opts[i].ch      = 'd';
	opts[i].rval    = 'd';
	opts[i].name    = "D";
	opts[i].label   = N_("DSNOpts");
	dsn_label       = i++;
	opts[i].ch      = -2;
	opts[i].rval    = 's';
	opts[i].name    = "S";
	opts[i++].label = "";
	opts[i].ch      = -2;
	opts[i].rval    = 'x';
	opts[i].name    = "X";
	opts[i++].label = "";
	opts[i].ch      = -2;
	opts[i].rval    = 'h';
	opts[i].name    = "H";
	opts[i++].label = "";
    }

    if(filters){
	opts[i].ch      = KEY_UP;
	opts[i].rval    = 10;
	opts[i].name    = "";
	opts[i++].label = "";

	opts[i].ch      = KEY_DOWN;
	opts[i].rval    = 11;
	opts[i].name    = "";
	opts[i++].label = "";
    }

    opts[i].ch = -1;

    fix_windsize(ps_global);

    while(1){
	if(filters && filters->filter && (p = strindex(filters->filter, ' ')))
	  *p = '\0';
	else
	  p = NULL;

	lparen = 0;
	strncpy(tmp_20k_buf, "Send message", SIZEOF_20KBUF);
	tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	optp = tmp_20k_buf + strlen(tmp_20k_buf);

	if(F_ON(F_NO_FCC_ATTACH, ps_global) && !lmc.text_only)
	  sstrncpy(&optp, " and Fcc Atmts", SIZEOF_20KBUF-(optp-tmp_20k_buf));

	if(allow_flowed && !flowing_requested){
	    if((optp-tmp_20k_buf)+2 < SIZEOF_20KBUF){
		if(!lparen){
		  *optp++ = ' ';
		  *optp++ = '(';
		  lparen++;
		}
		else 
		  *optp++ = ' ';
	    }

	    sstrncpy(&optp, "not flowed", SIZEOF_20KBUF-(optp-tmp_20k_buf));
	}

	if(filters){
	    if((optp-tmp_20k_buf)+2 < SIZEOF_20KBUF){
		if(!lparen){
		  *optp++ = ' ';
		  *optp++ = '(';
		  lparen++;
		}
		else{
		    *optp++ = ',';
		    *optp++ = ' ';
		}
	    }

	    if(filters->filter){
		sstrncpy(&optp, "filtered thru \"", SIZEOF_20KBUF-(optp-tmp_20k_buf));
		sstrncpy(&optp, filters->filter, SIZEOF_20KBUF-(optp-tmp_20k_buf));
		if((optp-tmp_20k_buf) < SIZEOF_20KBUF)
		  *optp++ = '\"';
	    }
	    else
	      sstrncpy(&optp, "unfiltered", SIZEOF_20KBUF-(optp-tmp_20k_buf));
	}

	if((call_mailer_flags & CM_VERBOSE) || background_requested){
	    if((optp-tmp_20k_buf)+2 < SIZEOF_20KBUF){
		if(!lparen){
		  *optp++ = ' ';
		  *optp++ = '(';
		  lparen++;
		}
		else 
		  *optp++ = ' ';
	    }

	    sstrncpy(&optp, "in ", SIZEOF_20KBUF-(optp-tmp_20k_buf));
	    if(call_mailer_flags & CM_VERBOSE)
	      sstrncpy(&optp, "verbose ", SIZEOF_20KBUF-(optp-tmp_20k_buf));

	    if(background_requested)
	      sstrncpy(&optp, "background ", SIZEOF_20KBUF-(optp-tmp_20k_buf));

	    sstrncpy(&optp, "mode", SIZEOF_20KBUF-(optp-tmp_20k_buf));
	}

	if(g_rolenick && !(he && he[N_FROM].dirty)){
	    if((optp-tmp_20k_buf)+2 < SIZEOF_20KBUF){
		if(!lparen){
		  *optp++ = ' ';
		  *optp++ = '(';
		  lparen++;
		}
		else 
		  *optp++ = ' ';
	    }

	    sstrncpy(&optp, "as \"", SIZEOF_20KBUF-(optp-tmp_20k_buf));
	    sstrncpy(&optp, g_rolenick, SIZEOF_20KBUF-(optp-tmp_20k_buf));
	    sstrncpy(&optp, "\"", SIZEOF_20KBUF-(optp-tmp_20k_buf));
	}

	if(call_mailer_flags & CM_DSN_SHOW){
	    if((optp-tmp_20k_buf)+2 < SIZEOF_20KBUF){
		if(!lparen){
		  *optp++ = ' ';
		  *optp++ = '(';
		  lparen++;
		}
		else{
		    *optp++ = ',';
		    *optp++ = ' ';
		}
	    }

	    sstrncpy(&optp, dsn_string, SIZEOF_20KBUF-(optp-tmp_20k_buf));
	}

#ifdef SMIME
    	if(ps_global->smime && ps_global->smime->do_encrypt){
	    if((optp-tmp_20k_buf)+2 < SIZEOF_20KBUF){
		if(!lparen){
		    *optp++ = ' ';
		    *optp++ = '(';
		    lparen++;
		}
		else{
		    *optp++ = ',';
		    *optp++ = ' ';
		}
	    }

	    sstrncpy(&optp, "Encrypted", SIZEOF_20KBUF-(optp-tmp_20k_buf));
	}
	
    	if(ps_global->smime && ps_global->smime->do_sign){
	    if((optp-tmp_20k_buf)+2 < SIZEOF_20KBUF){
		if(!lparen){
		    *optp++ = ' ';
		    *optp++ = '(';
		    lparen++;
		}
		else{
		    *optp++ = ',';
		    *optp++ = ' ';
		}
	    }

	    sstrncpy(&optp, "Signed", SIZEOF_20KBUF-(optp-tmp_20k_buf));
	}
#endif

	if(lparen && (optp-tmp_20k_buf) < SIZEOF_20KBUF)
	  *optp++ = ')';

	sstrncpy(&optp, "? ", SIZEOF_20KBUF-(optp-tmp_20k_buf));
	tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';

	if(p)
	  *p = ' ';

	if(flowing_label)
	  opts[flowing_label].label = flowing_requested ? N_("NoFlow") : N_("Flow");

	if(verbose_label)
	  opts[verbose_label].label = (call_mailer_flags & CM_VERBOSE) ? N_("Normal") : N_("Verbose");

	if(bg_label)
	  opts[bg_label].label = background_requested
				   ? N_("Foreground") : N_("Background");

	if(fcc_label)
	  opts[fcc_label].label = lmc.text_only ? N_("Fcc Attchmnts")
						: N_("No Fcc Atmts ");

	if(F_ON(F_DSN, ps_global)){
	    if(call_mailer_flags & CM_DSN_SHOW){
		opts[dsn_label].label   = (call_mailer_flags & CM_DSN_DELAY)
					   ? N_("NoDelay") : N_("Delay");
		opts[dsn_label+1].ch    = 's';
		opts[dsn_label+1].label = (call_mailer_flags & CM_DSN_SUCCESS)
					   ? N_("NoSuccess") : N_("Success");
		opts[dsn_label+2].ch    = 'x';
		opts[dsn_label+2].label = (call_mailer_flags & CM_DSN_NEVER)
					   ? N_("ErrRets") : N_("NoErrRets");
		opts[dsn_label+3].ch    = 'h';
		opts[dsn_label+3].label = (call_mailer_flags & CM_DSN_FULL)
					   ? N_("RetHdrs") : N_("RetFull");
	    }
	}

	if(double_rad +
	   ((call_mailer_flags & CM_DSN_SHOW)
	       ? 4 : F_ON(F_DSN, ps_global) ? 1 : 0) > 11)
	  rv = double_radio_buttons(tmp_20k_buf, -FOOTER_ROWS(ps_global), opts,
			   'y', 'z',
			   (F_ON(F_DSN, ps_global) && allow_flowed)
					          ? h_send_prompt_dsn_flowed :
			    F_ON(F_DSN, ps_global) ? h_send_prompt_dsn       :
			     allow_flowed           ? h_send_prompt_flowed   :
						       h_send_prompt,
			   RB_NORM);
	else
	  rv = radio_buttons(tmp_20k_buf, -FOOTER_ROWS(ps_global), opts,
			   'y', 'z',
			   (double_rad +
			    ((call_mailer_flags & CM_DSN_SHOW)
				? 4 : F_ON(F_DSN, ps_global) ? 1 : 0) == 11)
				   ? NO_HELP : 
			   (F_ON(F_DSN, ps_global) && allow_flowed)
					          ? h_send_prompt_dsn_flowed :
			    F_ON(F_DSN, ps_global) ? h_send_prompt_dsn       :
			     allow_flowed           ? h_send_prompt_flowed   :
						       h_send_prompt,
			   RB_NORM);

	if(rv == 'y'){				/* user ACCEPTS! */
	    break;
	}
	else if(rv == 'n'){			/* Declined! */
	    rstr = _("No Message Sent");
	    break;
	}
	else if(rv == 'z'){			/* Cancelled! */
	    rstr = _("Send Cancelled");
	    break;
	}
	else if(rv == 10){			/* PREVIOUS filter */
	    filters = filters->prev;
	}
	else if(rv == 11){			/* NEXT filter */
	    filters = filters->next;
	}
	else if(rv == 21){
	    lmc.text_only = !lmc.text_only;
	}
	else if(rv == 12){			/* flip verbose bit */
	    if(call_mailer_flags & CM_VERBOSE)
	      call_mailer_flags &= ~CM_VERBOSE;
	    else
	      call_mailer_flags |= CM_VERBOSE;

	    if((call_mailer_flags & CM_VERBOSE) && background_requested)
	      background_requested = 0;
	}
	else if(rv == 22){			/* flip flowing bit */
	    flowing_requested = !flowing_requested;
	}
	else if(rv == 15){
	    if((background_requested = !background_requested)
	       && (call_mailer_flags & CM_VERBOSE))
	      call_mailer_flags &= ~CM_VERBOSE;	/* clear verbose */
	}
	else if(call_mailer_flags & CM_DSN_SHOW){
	    if(rv == 's'){			/* flip success bit */
		call_mailer_flags ^= CM_DSN_SUCCESS;
		/* turn off related bits */
		if(call_mailer_flags & CM_DSN_SUCCESS)
		  call_mailer_flags &= ~(CM_DSN_NEVER);
	    }
	    else if(rv == 'd'){			/* flip delay bit */
		call_mailer_flags ^= CM_DSN_DELAY;
		/* turn off related bits */
		if(call_mailer_flags & CM_DSN_DELAY)
		  call_mailer_flags &= ~(CM_DSN_NEVER);
	    }
	    else if(rv == 'x'){			/* flip never bit */
		call_mailer_flags ^= CM_DSN_NEVER;
		/* turn off related bits */
		if(call_mailer_flags & CM_DSN_NEVER)
		  call_mailer_flags &= ~(CM_DSN_SUCCESS | CM_DSN_DELAY);
	    }
	    else if(rv == 'h'){			/* flip full bit */
		call_mailer_flags ^= CM_DSN_FULL;
	    }
	}
	else if(rv == 'd'){			/* show dsn options */
	    /*
	     * When you turn on DSN, the default is to notify on
	     * failure, success, or delay; and to return the whole
	     * body on failure.
	     */
	    call_mailer_flags |= (CM_DSN_SHOW | CM_DSN_SUCCESS | CM_DSN_DELAY | CM_DSN_FULL);
	}
	else if(rv == 'p'){			/* choose X-Priority */
	    char *prio;

	    prio = choose_a_priority(priority_requested);
	    if((ps_global->redrawer = redraw_pico) != NULL){
		(*ps_global->redrawer)();
		fix_windsize(ps_global);
	    }

	    if(prio){
		if(priority_requested)
		  fs_give((void **) &priority_requested);

		if(prio[0])
		  priority_requested = prio;
		else
		  fs_give((void **) &prio);
	    }
	}
#ifdef SMIME
	else if(rv=='e'){
	    if(ps_global->smime)
	      ps_global->smime->do_encrypt = !ps_global->smime->do_encrypt;
	}
	else if(rv=='g'){
	    if(ps_global->smime)
	      ps_global->smime->do_sign = !ps_global->smime->do_sign;
	}
#endif

	snprintf(dsn_string, sizeof(dsn_string), "DSN requested[%s%s%s%s]",
		(call_mailer_flags & CM_DSN_NEVER)
		  ? "Never" : "F",
		(call_mailer_flags & CM_DSN_DELAY)
		  ? "D" : "",
		(call_mailer_flags & CM_DSN_SUCCESS)
		  ? "S" : "",
		(call_mailer_flags & CM_DSN_NEVER)
		  ? ""
		  : (call_mailer_flags & CM_DSN_FULL) ? "-Full"
					       : "-Hdrs");
	dsn_string[sizeof(dsn_string)-1] = '\0';
    }

    /* remember selection */
    if(filters && filters->index > -1)
      sending_filter_requested = ps_global->VAR_SEND_FILTER[filters->index];

    if(filters){
	filters->prev->next = NULL;			/* tie off list */
	while(filters){				/* then free it */
	    fp = filters->next;
	    if(filters->filter)
	      fs_give((void **)&filters->filter);

	    fs_give((void **)&filters);
	    filters = fp;
	}
    }

    if(old_suspend)
      (void) F_SET(F_CAN_SUSPEND, ps_global, 1);

    if(result)
      *result = rstr;

    ps_global->redrawer = redraw;

    return((rstr == NULL) ? 0 : 1);
}


/*
 * Allow user to choose a priority for sending.
 *
 * Returns an allocated priority on success, NULL otherwise.
 */
char *
choose_a_priority(char *default_val)
{
    char      *choice = NULL;
    char     **priority_list, **lp;
    char      *starting_val = NULL;
    char      *none;
    int        i, cnt;
    PRIORITY_S *p;

    for(cnt = 0, p = priorities; p && p->desc; p++)
      cnt++;

    cnt++;	/* for NONE entry */
    lp = priority_list = (char **) fs_get((cnt + 1) * sizeof(*priority_list));
    memset(priority_list, 0, (cnt+1) * sizeof(*priority_list));

    for(i = 0, p = priorities; p && p->desc; p++){
	*lp = cpystr(p->desc);
	if(default_val && !strcmp(default_val, p->desc))
	  starting_val = (*lp);

	lp++;
    }

    none = _("NONE - No X-Priority header included");
    *lp = cpystr(none);
    if(!starting_val)
      starting_val = (*lp);

    /* TRANSLATORS: SELECT A PRIORITY is a screen title
       TRANSLATORS: Print something1 using something2.
       "priorities" is something1 */
    choice = choose_item_from_list(priority_list, NULL, _("SELECT A PRIORITY"),
				   _("priorities"), h_select_priority_screen,
				   _("HELP FOR SELECTING A PRIORITY"),
				   starting_val);

    if(!choice)
      q_status_message(SM_ORDER, 1, 4, _("No change"));
    else if(!strcmp(choice, none))
      choice[0] = '\0';

    free_list_array(&priority_list);

    return(choice);
}


int
dont_flow_this_time(void)
{
    return(flowing_requested ? 0 : 1);
}


/*----------------------------------------------------------------------
    Call back for pico to display mime type of attachment
     
Args: file -- filename being attached

Returns: returns 1 on success (message queued), zero otherwise (don't know
	  type so nothing queued).
----*/      
int
mime_type_for_pico(char *file)
{
    BODY *body;
    int   rv;
    void *file_contents;

    body           = mail_newbody();
    body->type     = TYPEOTHER;
    body->encoding = ENCOTHER;

    /* don't know where the cursor's been, reset it */
    clear_cursor_pos();
    if(!set_mime_type_by_extension(body, file)){
	if((file_contents=(void *)so_get(FileStar,file,READ_ACCESS|READ_FROM_LOCALE)) != NULL){
	    body->contents.text.data = file_contents;
	    set_mime_type_by_grope(body);
	}
    }

    if(body->type != TYPEOTHER){
	rv = 1;
	q_status_message3(SM_ORDER, 0, 3,
	    _("File %s attached as type %s/%s"), file,
	    body_types[body->type],
	    body->subtype ? body->subtype : rfc822_default_subtype(body->type));
    }
    else
      rv = 0;

    pine_free_body(&body);
    return(rv);
}


/*----------------------------------------------------------------------
  Call back for pico to receive an uploaded message

  Args: fname -- name for uploaded file (empty if they want us to assign it)
	size -- pointer to long to hold the attachment's size

  Notes: the attachment is uploaded to a temp file, and 

  Returns: TRUE on success, FALSE otherwise
----*/
int
upload_msg_to_pico(char *fname, size_t fnlen, long int *size)
{
    char     cmd[MAXPATH+1], *fnp = NULL;
    char    *locale_name = NULL;
    long     l;
    PIPE_S  *syspipe;

    dprint((1, "Upload cmd called to xfer \"%s\"\n",
	       fname ? fname : "<NO FILE>"));

    if(!fname)				/* no place for file name */
      return(0);

    if(!*fname){			/* caller wants temp file */
	if((fnp = temp_nam(NULL, "pu")) != NULL){
	    strncpy(fname, fnp, fnlen);
	    fname[fnlen-1] = '\0';
	    our_unlink(fnp);
	    fs_give((void **)&fnp);
	}
    }
    else
      locale_name = convert_to_locale(fname);

    build_updown_cmd(cmd, sizeof(cmd), ps_global->VAR_UPLOAD_CMD_PREFIX,
		     ps_global->VAR_UPLOAD_CMD, locale_name ? locale_name : fname);
    if((syspipe = open_system_pipe(cmd, NULL, NULL, PIPE_USER | PIPE_RESET,
				  0, pipe_callback, pipe_report_error)) != NULL){
	(void) close_system_pipe(&syspipe, NULL, pipe_callback);
	if((l = name_file_size(locale_name ? locale_name : fname)) < 0L){
	    q_status_message2(SM_ORDER | SM_DING, 3, 4,
			      "Error determining size of %s: %s", fname,
			      fnp = error_description(errno));
	    dprint((1,
		   "!!! Upload cmd \"%s\" failed for \"%s\": %s\n",
		   cmd ? cmd : "?",
		   fname ? fname : "?",
		   fnp ? fnp : "?"));
	}
	else if(size)
	  *size = l;

	if(locale_name)
	  fs_give((void **) &locale_name);

	return(l >= 0);
    }
    else
      q_status_message(SM_ORDER | SM_DING, 3, 4, _("Error opening pipe"));

    if(locale_name)
      fs_give((void **) &locale_name);

    return(0);
}


char *
cancel_for_pico(void (*redraw_pico)(void))
{
    int	  rv;
    char *rstr = NULL;
    char *prompt =
     _("Cancel message (answering \"Confirm\" will abandon your mail message) ? ");
    void (*redraw)(void) = ps_global->redrawer;
    static ESCKEY_S opts[] = {
	{'c', 'c', "C", N_("Confirm")},
	{'n', 'n', "N", N_("No")},
	{'y', 'y', "", ""},
	{-1, 0, NULL, NULL}
    };

    ps_global->redrawer = redraw_pico;
    fix_windsize(ps_global);
    
    while(1){
	rv = radio_buttons(prompt, -FOOTER_ROWS(ps_global), opts,
			   'n', 'x', h_confirm_cancel, RB_NORM);
	if(rv == 'c'){				/* user ACCEPTS! */
	    rstr = "";
	    break;
	}
	else if(rv == 'y'){
	    q_status_message(SM_INFO, 1, 3, _(" Type \"C\" to cancel message "));
	    display_message('x');
	}
	else
	  break;
    }

    ps_global->redrawer = redraw;
    return(rstr);
}


/*----------------------------------------------------------------------
    Pass the first text segment of the message thru the "send filter"
     
Args: body pointer and address for storage object of old data

Returns: returns 1 on success, zero on error.
----*/      
int
filter_message_text(char *fcmd, ENVELOPE *outgoing, struct mail_bodystruct *body,
		    STORE_S **old, METAENV *header)
{
    char     *cmd, *tmpf = NULL, *resultf = NULL, *errstr = NULL, *mtf = NULL;
    int	      key = 0, include_hdrs = 0;
    gf_io_t   gc, pc;
    STORE_S **so = (STORE_S **)((body->type == TYPEMULTIPART)
				? &body->nested.part->body.contents.text.data
				: &body->contents.text.data),
	     *tmp_so = NULL, *tmpf_so,
	     *save_local_so, *readthis_so, *our_tmpf_so = NULL;
#define DO_HEADERS 1

    if(fcmd
       && (cmd=expand_filter_tokens(fcmd, outgoing, &tmpf, &resultf, &mtf,
				    &key, &include_hdrs))){
	if(tmpf){
	    /*
	     * We need WRITE_TO_LOCALE here because the user is going to
	     * be operating on tmpf. We need to change it back after they
	     * are done.
	     */
	    if((tmpf_so = so_get(FileStar, tmpf, EDIT_ACCESS|OWNER_ONLY|WRITE_TO_LOCALE)) != NULL){
		if(key){
		    so_puts(tmpf_so, filter_session_key());
		    so_puts(tmpf_so, NEWLINE);
		}

		/*
		 * If the headers are wanted for filtering, we can just
		 * stick them in the tmpf file that is already there before
		 * putting the body in.
		 */
		if(include_hdrs){
		    save_local_so = lmc.so;
		    lmc.so = tmpf_so;		/* write it to tmpf_so */
		    lmc.all_written = lmc.text_written = lmc.text_only = 0;
		    pine_rfc822_header(header, body, NULL, NULL);
		    lmc.so = save_local_so;
		}

		so_seek(*so, 0L, 0);
		gf_set_so_readc(&gc, *so);
		gf_set_so_writec(&pc, tmpf_so);
		gf_filter_init();
		errstr = gf_pipe(gc, pc);
		gf_clear_so_readc(*so);
		gf_clear_so_writec(tmpf_so);
		so_give(&tmpf_so);
	    }
	    else
	      errstr = "Can't create space for filter temporary file.";
	}
	else if(include_hdrs){
	    /*
	     * Gf_filter wants a single storage object to read from.
	     * If headers are wanted for filtering we'll have to put them
	     * and the body into a temp file first and then use that
	     * as the storage object for gf_filter.
	     * We don't use WRITE_TO_LOCALE in this case because gf_filter
	     * takes care of that.
	     */
	    if((our_tmpf_so = so_get(TmpFileStar, NULL, EDIT_ACCESS|OWNER_ONLY)) != NULL){
		/* put headers in our_tmpf_so */
		save_local_so = lmc.so;
		lmc.so = our_tmpf_so;		/* write it to our_tmpf_so */
		lmc.all_written = lmc.text_written = lmc.text_only = 0;
		pine_rfc822_header(header, body, NULL, NULL);
		lmc.so = save_local_so;

		/* put body in our_tmpf_so */
		so_seek(*so, 0L, 0);
		gf_set_so_readc(&gc, *so);
		gf_set_so_writec(&pc, our_tmpf_so);
		gf_filter_init();
		errstr = gf_pipe(gc, pc);
		gf_clear_so_readc(*so);
		gf_clear_so_writec(our_tmpf_so);

		/* tell gf_filter to read from our_tmpf_so instead of *so */
		readthis_so = our_tmpf_so;
	    }
	    else
	      errstr = "Can't create space for temporary file.";
	}
	else
	  readthis_so = *so;

	if(!errstr){
	    if((tmp_so = so_get(PicoText, NULL, EDIT_ACCESS)) != NULL){
		gf_set_so_writec(&pc, tmp_so);
		ps_global->mangled_screen = 1;
		suspend_busy_cue();
		ClearScreen();
		fflush(stdout);
		if(tmpf){
		    PIPE_S *fpipe;

		    if((fpipe = open_system_pipe(cmd, NULL, NULL,
						PIPE_NOSHELL | PIPE_RESET,
						0, pipe_callback, pipe_report_error)) != NULL){
			if(close_system_pipe(&fpipe, NULL, pipe_callback) == 0){

			    /* now we undo the WRITE_FROM_LOCALE change in tmpf */
			    if((tmpf_so = so_get(FileStar, tmpf, READ_ACCESS|READ_FROM_LOCALE)) != NULL){
				gf_set_so_readc(&gc, tmpf_so);
				gf_filter_init();
				errstr = gf_pipe(gc, pc);
				gf_clear_so_readc(tmpf_so);
				so_give(&tmpf_so);
			    }
			    else
			      errstr = "Can't open temp file filter wrote.";
			}
			else
			  errstr = "Filter command returned error.";
		    }
		    else
		      errstr = "Can't exec filter text.";
		}
		else
		  errstr = gf_filter(cmd, key ? filter_session_key() : NULL,
				     readthis_so, pc, NULL, 0,
				     pipe_callback);

		if(our_tmpf_so)
		  so_give(&our_tmpf_so);

		gf_clear_so_writec(tmp_so);

#ifdef _WINDOWS
		if(!errstr){
		    /*
		     * Can't really be using stdout, so don't print message that
		     * gets printed in the else.  Ideally the program being called
		     * will wait after showing the message, we might want to look
		     * into doing the waiting in console based apps... or not.
		     */
#else
		if(errstr){
		    unsigned long ch;

		    fprintf(stdout, "\r\n%s  Hit return to continue.", errstr);
		    fflush(stdout);
		    while((ch = read_char(300)) != ctrl('M')
			  && ch != NO_OP_IDLE)
		      putchar(BELL);
		}
		else{
#endif /* _WINDOWS */
		    BODY *b = (body->type == TYPEMULTIPART)
					   ? &body->nested.part->body : body;

		    *old = *so;			/* save old so */
		    *so = tmp_so;		/* return new one */
		    (*so)->attr = copy_parameters((*old)->attr);

		    /*
		     * If the command said it would return new MIME
		     * mime type data, check it out...
		     */
		    if(mtf){
			char  buf[MAILTMPLEN], *s;
			FILE *fp;

			if((fp = our_fopen(mtf, "rb")) != NULL){
			    if(fgets(buf, sizeof(buf), fp)
			       && !struncmp(buf, "content-", 8)
			       && (s = strchr(buf+8, ':'))){
				BODY *nb = mail_newbody();

				for(*s++ = '\0'; *s == ' '; s++)
				  ;

				rfc822_parse_content_header(nb,
				    (char *) ucase((unsigned char *) buf+8),s);
				if(nb->type == TYPETEXT
				   && nb->subtype
				   && (!b->subtype 
				       || strucmp(b->subtype, nb->subtype))){
				    if(b->subtype)
				      fs_give((void **) &b->subtype);

				    b->subtype  = nb->subtype;
				    nb->subtype = NULL;
				      
				    mail_free_body_parameter(&b->parameter);
				    b->parameter = nb->parameter;
				    nb->parameter = NULL;
				    mail_free_body_parameter(&nb->parameter);
				}

				mail_free_body(&nb);
			    }

			    fclose(fp);
			}
		    }

		    /*
		     * Reevaluate the encoding in case form's changed...
		     */
		    b->encoding = ENCOTHER;
		    set_mime_type_by_grope(b);
		}

		ClearScreen();
		resume_busy_cue(0);
	    }
	    else
	      errstr = "Can't create space for filtered text.";
	}

	fs_give((void **)&cmd);
    }
    else
      return(0);

    if(tmpf){
	our_unlink(tmpf);
	fs_give((void **)&tmpf);
    }

    if(mtf){
	our_unlink(mtf);
	fs_give((void **) &mtf);
    }

    if(resultf){
	if(name_file_size(resultf) > 0L)
	  display_output_file(resultf, "Filter", NULL, DOF_BRIEF);

	fs_give((void **)&resultf);
    }
    else if(errstr){
	if(tmp_so)
	  so_give(&tmp_so);

	q_status_message1(SM_ORDER | SM_DING, 3, 6, _("Problem filtering: %s"),
			  errstr);
	dprint((1, "Filter FAILED: %s\n",
	       errstr ? errstr : "?"));
    }

    return(errstr == NULL);
}


/*----------------------------------------------------------------------
    Copy the newsgroup name of the given mailbox into the given buffer
     
Args: 

Returns: 
----*/      
void
pine_send_newsgroup_name(char *mailbox, char *group_name, size_t len)
{
    NETMBX  mb;

    if(*mailbox == '#'){		/* Strip the leading "#news." */
	strncpy(group_name, mailbox + 6, len-1);
	group_name[len-1] = '\0';
    }
    else if(mail_valid_net_parse(mailbox, &mb)){
	pine_send_newsgroup_name(mb.mailbox, group_name, len);
    }
    else
      *group_name = '\0';
}


/*----------------------------------------------------------------------
     Generate and send a message back to the pine development team
     
Args: none

Returns: none
----*/      
void
phone_home(char *addr)
{
    char      tmp[MAX_ADDRESS];
    ENVELOPE *outgoing;
    BODY     *body;

    outgoing = mail_newenvelope();
    if(!addr || !strindex(addr, '@')){
	snprintf(addr = tmp, sizeof(tmp), "alpine%s@%s", PHONE_HOME_VERSION, PHONE_HOME_HOST);
	tmp[sizeof(tmp)-1] = '\0';
    }

    rfc822_parse_adrlist(&outgoing->to, addr, ps_global->maildomain);

    outgoing->message_id  = generate_message_id();
    outgoing->subject	  = cpystr("Document Request");
    outgoing->from	  = phone_home_from();

    body       = mail_newbody();
    body->type = TYPETEXT;

    if((body->contents.text.data = (void *)so_get(PicoText,NULL,EDIT_ACCESS)) != NULL){
	so_puts((STORE_S *)body->contents.text.data, "Document request: ");
	so_puts((STORE_S *)body->contents.text.data, "Alpine-");
	so_puts((STORE_S *)body->contents.text.data, ALPINE_VERSION);
	if(ps_global->first_time_user)
	  so_puts((STORE_S *)body->contents.text.data, " for New Users");

	if(ps_global->VAR_INBOX_PATH && ps_global->VAR_INBOX_PATH[0] == '{')
	  so_puts((STORE_S *)body->contents.text.data, " and IMAP");

	if(ps_global->VAR_NNTP_SERVER && ps_global->VAR_NNTP_SERVER[0]
	      && ps_global->VAR_NNTP_SERVER[0][0])
	  so_puts((STORE_S *)body->contents.text.data, " and NNTP");

	(void)pine_simple_send(outgoing, &body, NULL,NULL,NULL,NULL, SS_NULLRP);

	q_status_message(SM_ORDER, 1, 3, "Thanks for being counted!");
    }
    else
      q_status_message(SM_ORDER | SM_DING, 3, 4,
		       "Problem creating space for message text.");

    mail_free_envelope(&outgoing);
    pine_free_body(&body);

}


/*----------------------------------------------------------------------
    Set up fields for passing to pico.  Assumes first text part is
    intended to be passed along for editing, and is in the form of
    of a storage object brought into existence sometime before pico_send().
 -----*/
void
outgoing2strings(METAENV *header, struct mail_bodystruct *bod, void **text,
		 PATMT **pico_a, int from_bounce)
{
    PINEFIELD  *pf;

    /*
     * SIMPLIFYING ASSUMPTION #37: the first TEXT part's storage object
     * is guaranteed to be of type PicoText!
     */
    if(bod->type == TYPETEXT){
	*text = so_text((STORE_S *) bod->contents.text.data);

	/* mark storage object as user edited */
	if(!from_bounce)
	  (void) so_attr((STORE_S *) bod->contents.text.data, "edited", "1");
    }
    else if(bod->type == TYPEMULTIPART){
	PART       *part;
	PATMT     **ppa;
	char       *type, *name, *p;
	int		name_l;

	/*
	 * We used to jump out the window if the first part wasn't text,
	 * but that may not be the case when bouncing a message with
	 * a leading non-text segment.  So, IT'S UNDERSTOOD that the 
	 * contents of the first part to send is still ALWAYS in a 
	 * PicoText storage object, *AND* if that object doesn't contain
	 * data of type text, then it must contain THE ENCODED NON-TEXT
	 * DATA of the piece being sent.
	 *
	 * It's up to the programmer to make sure that such a message is
	 * sent via pine_simple_send and never get to the composer via
	 * pine_send.
	 *
	 * Make sense?
	 */
	*text = so_text((STORE_S *) bod->nested.part->body.contents.text.data);

	/* mark storage object as user edited */
	if(!from_bounce)
	  (void) so_attr((STORE_S *) bod->nested.part->body.contents.text.data, "edited", "1");

	/*
	 * If we already had a list, blast it now, so we can build a new
	 * attachment list that reflects what's really there...
	 */
	if(pico_a){
	    free_attachment_list(pico_a);


	    /* Simplifyihg assumption #28e. (see cross reference) 
	       All parts in the body passed in here that are not already
	       in the attachments list are added to the end of the attachments
	       list. Attachment items not in the body list will be taken care
	       of in strings2outgoing, but they are unlikey to occur
	    */

	    for(part = bod->nested.part->next; part != NULL; part = part->next) {
		/* Already in list? */
		for(ppa = pico_a;
		    *ppa && strcmp((*ppa)->id, part->body.id);
		    ppa = &(*ppa)->next)
		  ;

		if(!*ppa){		/* Not in the list! append it... */
		    *ppa = (PATMT *)fs_get(sizeof(PATMT));

		    if(part->body.description){
			char *p;
			size_t len;

			len = 4*strlen(part->body.description)+1;
			p = (char *)fs_get(len*sizeof(char));
			if(rfc1522_decode_to_utf8((unsigned char *)p,
						  len, part->body.description) == (unsigned char *) p){
			    (*ppa)->description = p;
			}
			else{
			    fs_give((void **)&p);
			    (*ppa)->description = cpystr(part->body.description);
			}
		    }
		    else
		      (*ppa)->description = cpystr("");
            
		    type = type_desc(part->body.type, part->body.subtype,
				     part->body.parameter, NULL, 0);

		    /*
		     * If we can find a "name" parm, display that too...
		     */
		    if((name = parameter_val(part->body.parameter, "name")) != NULL){
			/* Convert any [ or ]'s the name contained */
			for(p = name; *p ; p++)
			  if(*p == '[')
			    *p = '(';
			  else if(*p == ']')
			    *p = ')';

			name_l = p - name;
		    }
		    else
		      name_l = 0;

		    (*ppa)->filename = fs_get(strlen(type) + name_l + 5);

		    snprintf((*ppa)->filename, strlen(type) + name_l + 5, "[%s%s%s]", type,
			    name ? ": " : "", name ? name : "");
		    (*ppa)->filename[strlen(type) + name_l + 5 - 1] = '\0';

		    if(name)
		      fs_give((void **) &name);

		    (*ppa)->flags = A_FLIT;
		    (*ppa)->size  = cpystr(byte_string(
						   send_body_size(&part->body)));
		    if(!part->body.id)
		      part->body.id = generate_message_id();

		    (*ppa)->id   = cpystr(part->body.id);
		    (*ppa)->next = NULL;
		}
	    }
	}
    }
        

    /*------------------------------------------------------------------
       Malloc strings to pass to composer editor because it expects
       such strings so it can realloc them
      -----------------------------------------------------------------*/
    /*
     * turn any address fields into text strings
     */
    /*
     * SIMPLIFYING ASSUMPTION #116: all header strings are understood
     * NOT to be RFC1522 decoded.  Said differently, they're understood
     * to be RFC1522 ENCODED as necessary.  The intent is to preserve
     * original charset tagging as far into the compose/send pipe as
     * we can.
     */
    for(pf = header->local; pf && pf->name; pf = pf->next)
      if(pf->canedit)
	switch(pf->type){
	  case Address :
	    if(pf->addr){
		char *p, *t, *u;
		long  l;

		pf->scratch = addr_list_string(*pf->addr, NULL, 1);

		/*
		 * Scan for and fix-up patently bogus fields.
		 *
		 * NOTE: collaboration with this code and what's done in
		 * reply.c:reply_cp_addr to package up the bogus stuff
		 * is required.
		 */
		for(p = pf->scratch; (p = strstr(p, "@" RAWFIELD)); )
		  for(t = p; ; t--)
		    if(*t == '&'){		/* find "leading" token */
			int replacelen;

			/*
			 * Rfc822_cat has been changed so that it now quotes
			 * this sometimes. So we have to look out for quotes
			 * which confuse the decoder. It was only quoting
			 * because we were putting \r \n in the input, I think.
			 */
			if(t > pf->scratch && t[-1] == '\"' && p[-1] == '\"')
			  t[-1] = p[-1] = ' ';

			*t++ = ' ';		/* replace token */
			*p = '\0';		/* tie off string */
			u = rfc822_base64((unsigned char *) t,
					  (unsigned long) strlen(t),
					  (unsigned long *) &l);
			if(!u)
			  u = "";

			replacelen = strlen(t);
			*p = '@';		/* restore 'p' */
			rplstr(p, strlen(p), 12, "");	/* clear special token */
			rplstr(t, strlen(u)-replacelen+1, replacelen, u);
			if(u)
			  fs_give((void **) &u);

			if(HE(pf))
			  HE(pf)->start_here = 1;

			break;
		    }
		    else if(t == pf->scratch)
		      break;

		removing_leading_white_space(pf->scratch);
		if(pf->scratch){
		    size_t l;

		    /*
		     * Replace control characters with ^C notation, unless
		     * some conditions are met (see istrncpy).
		     * If user doesn't edit, then we switch back to the
		     * original version. If user does edit, then all bets
		     * are off.
		     */
		    iutf8ncpy((char *)tmp_20k_buf, pf->scratch, SIZEOF_20KBUF-1);
		    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		    if((l=strlen((char *)tmp_20k_buf)) > strlen(pf->scratch)){
			fs_give((void **)&pf->scratch);
			pf->scratch = (char *)fs_get((l+1) * sizeof(char));
		    }

		    strncpy(pf->scratch, (char *)tmp_20k_buf, l+1);
		}
	    }

	    break;

	  case Subject :
	    if(pf->text){
		char *p, *src;
		size_t len;

		src = pf->scratch ? pf->scratch
				  : (*pf->text) ? *pf->text : "";

		len = 4*strlen(src)+1;
		p = (char *)fs_get(len * sizeof(char));
		if(rfc1522_decode_to_utf8((unsigned char *)p, len, src) == (unsigned char *) p){
		    if(pf->scratch)
		      fs_give((void **)&pf->scratch);

		    pf->scratch = p;
		}
		else{
		    fs_give((void **)&p);
		    if(!pf->scratch)
		      pf->scratch = cpystr(src);
		}

		if(pf->scratch){
		    size_t l;

		    /*
		     * Replace control characters with ^C notation, unless
		     * some conditions are met (see istrncpy).
		     * If user doesn't edit, then we switch back to the
		     * original version. If user does edit, then all bets
		     * are off.
		     */
		    iutf8ncpy((char *)tmp_20k_buf, pf->scratch, SIZEOF_20KBUF-1);
		    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		    if((l=strlen((char *)tmp_20k_buf)) > strlen(pf->scratch)){
			fs_give((void **)&pf->scratch);
			pf->scratch = (char *)fs_get((l+1) * sizeof(char));
		    }

		    strncpy(pf->scratch, (char *)tmp_20k_buf, l+1);
		}
	    }

	    break;

	  default :
	    break;
	}
}


/*----------------------------------------------------------------------
    Restore fields returned from pico to form useful to sending
    routines.
 -----*/
void
strings2outgoing(METAENV *header, struct mail_bodystruct **bod, PATMT *attach, int flow_it)
{
    PINEFIELD *pf;
    int we_cancel = 0;

    we_cancel = busy_cue(NULL, NULL, 1);

    /*
     * turn any local address strings into address lists
     */
    for(pf = header->local; pf && pf->name; pf = pf->next)
	if(pf->scratch){
	    char *the_address = NULL;

	      switch(pf->type){
		case Address :
		  removing_trailing_white_space(pf->scratch);

		  if((the_address || *pf->scratch) && pf->addr){
		      ADDRESS     *new_addr = NULL;
		      static char *fakedomain = "@";

		      if(!the_address)
			the_address = pf->scratch;

		      rfc822_parse_adrlist(&new_addr, the_address,
				   (F_ON(F_COMPOSE_REJECTS_UNQUAL, ps_global))
				     ? fakedomain : ps_global->maildomain);
		      mail_free_address(pf->addr);	/* free old addrs */
		      *pf->addr = new_addr;		/* assign new addr */
		  }
		  else if(pf->addr)
		    mail_free_address(pf->addr);	/* free old addrs */

		  break;

		case Subject :
		  if(*pf->text)
		    fs_give((void **)pf->text);

		  if(*pf->scratch){
		        *pf->text = cpystr(pf->scratch);
		  }

		  break;

		default :
		  break;
	      }

	    fs_give((void **)&pf->scratch);	/* free now useless text */
	}

    create_message_body(bod, attach, flow_it);

    if(we_cancel)
      cancel_busy_cue(-1);
}


/*----------------------------------------------------------------------

 The head of the body list here is always either TEXT or MULTIPART. It may be
changed from TEXT to MULTIPART if there are attachments to be added
and it is not already multipart. 
  ----*/
void
create_message_body(struct mail_bodystruct **b, PATMT *attach, int flow_it)
{
    PART       *p, **pp;
    PATMT      *pa;
    BODY       *tmp_body, *text_body = NULL;
    void       *file_contents;
    PARAMETER **parmp;
    char       *lc;

    TIME_STAMP("create_body start.", 1);
    /*
     * if conditions are met short circuit MIME wrapping
     */
    if((*b)->type != TYPEMULTIPART && !attach){

	/* only override assigned encoding if it might need upgrading */
	if((*b)->type == TYPETEXT && (*b)->encoding == ENC7BIT)
	  (*b)->encoding = ENCOTHER;

	create_message_body_text(*b, flow_it);

	if(F_ON(F_COMPOSE_ALWAYS_DOWNGRADE, ps_global)
	   || !((*b)->encoding == ENC8BIT
		|| (*b)->encoding == ENCBINARY)){
	    TIME_STAMP("create_body end.", 1);
	    return;
	}
	else			/* protect 8bit in multipart */
	  text_body = *b;
    }

    if((*b)->type == TYPETEXT) {
        /*-- Current type is text, but there are attachments to add --*/
        /*-- Upgrade to a TYPEMULTIPART --*/
        tmp_body			= (BODY *)mail_newbody();
        tmp_body->type			= TYPEMULTIPART;
        tmp_body->nested.part		= mail_newbody_part();
	if(text_body){
	    /*
	     * Why do we do this?
	     * The problem is that base64 or quoted-printable encoding is
	     * sensitive to having random data appended to it's end. If
	     * we use a single part TEXT message and something in between
	     * us and the end appends advertising without adjusting for
	     * the encoding, the message is screwed up. So we wrap the
	     * text part inside a multipart and then the appended data
	     * will come after the boundary.
	     *
	     * We wish we could do this on the way out the door in a
	     * child of post_rfc822_output because at that point we know
	     * the character set and the encoding being used. For example,
	     * iso-2022-jp is an encoding that is not sensitive to data
	     * appended to the end, so it wouldn't need to be wrapped.
	     * We could conceivably have post_rfc822_body inspect the
	     * body and change it before doing the output. It would work
	     * but would be very fragile. We'd be passed a body from
	     * c-client to output and instead of just doing the output
	     * we'd change the body and then output it. Not worth it
	     * since the multipart wrapping is completely correct for
	     * MIME-aware mailers.
	     */
	    (void) copy_body(&(tmp_body->nested.part->body), *b);
	    /* move contents which were NOT copied */
	    tmp_body->nested.part->body.contents.text.data = (*b)->contents.text.data;
	    (*b)->contents.text.data = NULL;
	}
	else{
	    tmp_body->nested.part->body	= **b;

	    (*b)->subtype = (*b)->id = (*b)->description = NULL;
	    (*b)->parameter				 = NULL;
	    (*b)->contents.text.data			 = NULL;
	}

	pine_free_body(b);
        *b = tmp_body;
    }

    if(!text_body){
	/*-- Now type must be MULTIPART with first part text --*/
	(*b)->nested.part->body.encoding = ENCOTHER;
	create_message_body_text(&((*b)->nested.part->body), flow_it);
    }

    /*------ Go through the parts list remove those to be deleted -----*/
    for(pp = &(*b)->nested.part->next; *pp;){
	for(pa = attach; pa && (*pp)->body.id; pa = pa->next)
	  /* already existed? */
	  if(pa->id && strcmp(pa->id, (*pp)->body.id) == 0){
	      char *orig_descp = NULL, *cs = NULL;

	      /* 
	       * decode original to see if it matches what was decoded
	       * when we sent it in.
	       */

	      if((*pp)->body.description)
		orig_descp = (char *) rfc1522_decode_to_utf8((unsigned char *) tmp_20k_buf,
							     SIZEOF_20KBUF, (*pp)->body.description);

	      if(!(*pp)->body.description	/* update description? */
		 || (pa->description && strcmp(pa->description, orig_descp))){
		  if((*pp)->body.description)
		    fs_give((void **) &(*pp)->body.description);

		  /* encoding happens as msg text is written */
		  (*pp)->body.description = cpystr(pa->description);
	      }

	      if(cs)
		fs_give((void **) &cs);

	      break;
	  }

        if(pa == NULL){
	    p = *pp;				/* prepare to zap *pp */
	    *pp = p->next;			/* pull next one in list up */
	    p->next = NULL;			/* tie off removed node */

	    pine_free_body_data(&p->body);	/* clean up contained data */
	    mail_free_body_part(&p);		/* free up the part */
	}
	else
	  pp = &(*pp)->next;
    }

    /*---------- Now add any new attachments ---------*/
    for(p = (*b)->nested.part ; p->next != NULL; p = p->next);
    for(pa = attach; pa != NULL; pa = pa->next) {
        if(pa->id != NULL)
	  continue;			/* Has an ID, it's old */

	/*
	 * the idea is handle ALL attachments as open FILE *'s.  Actual
         * encoding and such is handled at the time the message
         * is shoved into the mail slot or written to disk...
	 *
         * Also, we never unlink a file, so it's up to whoever opens
         * it to deal with tmpfile issues.
	 */
	if((file_contents = (void *)so_get(FileStar, pa->filename,
					   READ_ACCESS)) == NULL){
            q_status_message2(SM_ORDER | SM_DING, 3, 4,
			  _("Error \"%s\", couldn't attach file \"%s\""),
                              error_description(errno), pa->filename);
            display_message('x');
            continue;
        }
        
        p->next			   = mail_newbody_part();
        p			   = p->next;
        p->body.id		   = generate_message_id();
        p->body.contents.text.data = file_contents;

	/*
	 * Set type to unknown and let set_mime_type_by_* figure it out.
	 * Always encode attachments we add as BINARY.
	 */
	p->body.type		     = TYPEOTHER;
	p->body.encoding	     = ENCBINARY;
	p->body.size.bytes           = name_file_size(pa->filename);
	if(!set_mime_type_by_extension(&p->body, pa->filename)){
	    set_mime_type_by_grope(&p->body);
	    set_charset_possibly_to_ascii(&p->body,  ps_global->keyboard_charmap);
	}

	so_release((STORE_S *)p->body.contents.text.data);

	if(pa->description)	/* encoding happens when msg written */
          p->body.description = cpystr(pa->description);

	/* Add name attribute for backward compatibility */
	for(parmp = &p->body.parameter; *parmp; )
	  if(!struncmp((*parmp)->attribute, "name", 4)
	     && (!*((*parmp)->attribute + 4)
		 || *((*parmp)->attribute + 4) == '*')){
	      PARAMETER *free_me = *parmp;
	      *parmp = (*parmp)->next;
	      free_me->next = NULL;
	      mail_free_body_parameter(&free_me);
	  }
	  else
	    parmp = &(*parmp)->next;

	set_parameter(parmp, "name",
		      pa->filename
		        ? ((lc = last_cmpnt(pa->filename)) ? lc : pa->filename)
			: NULL);

	/* Then set the Content-Disposition ala RFC1806 */
	if(!p->body.disposition.type){
	    p->body.disposition.type = cpystr("attachment");
            for(parmp = &p->body.disposition.parameter; *parmp; )
	      if(!struncmp((*parmp)->attribute, "filename", 4)
		 && (!*((*parmp)->attribute + 4)
		     || *((*parmp)->attribute + 4) == '*')){
		  PARAMETER *free_me = *parmp;
		  *parmp = (*parmp)->next;
		  free_me->next = NULL;
		  mail_free_body_parameter(&free_me);
	      }
	      else
		parmp = &(*parmp)->next;

	    set_parameter(parmp, "filename",
			  pa->filename
			    ? ((lc = last_cmpnt(pa->filename)) ? lc : pa->filename)
			    : NULL);
	}

        p->next = NULL;
        pa->id = cpystr(p->body.id);
    }

    /*
     * Now, if this multipart has but one text piece (that is, no
     * attachments), then downgrade from a composite type to a discrete
     * text/plain message if CTE is not 8bit.
     */
    if(!(*b)->nested.part->next
       && (*b)->nested.part->body.type == TYPETEXT
       && (F_ON(F_COMPOSE_ALWAYS_DOWNGRADE, ps_global)
	   || !((*b)->nested.part->body.encoding == ENC8BIT
		|| (*b)->nested.part->body.encoding == ENCBINARY))){
	/* Clone the interesting body part */
	tmp_body  = mail_newbody();
	*tmp_body = (*b)->nested.part->body;
	/* and rub out what we don't want cleaned up when it's free'd */
	mail_initbody(&(*b)->nested.part->body);
	mail_free_body(b);
	*b = tmp_body;
    }


    TIME_STAMP("create_body end.", 1);
}


/*
 * Fill in text BODY part's structure
 * 
 */
void
create_message_body_text(struct mail_bodystruct *b, int flow_it)
{
    set_mime_type_by_grope(b);
    if(F_OFF(F_QUELL_FLOWED_TEXT, ps_global)
       && F_OFF(F_STRIP_WS_BEFORE_SEND, ps_global)
       && flow_it)
      set_parameter(b ? &b->parameter : NULL, "format", "flowed");

    set_body_size(b);
}


/*
 * free_attachment_list - free attachments in given list
 */
void
free_attachment_list(PATMT **alist)
{
    PATMT  *leading;

    while(alist && *alist){		/* pointer pointing to something */
	leading = (*alist)->next;
	if((*alist)->description)
          fs_give((void **)&(*alist)->description);

	if((*alist)->filename){
	    if((*alist)->flags & A_TMP)
	      if(our_unlink((*alist)->filename) < 0)
		dprint((1, "-- Can't unlink(%s): %s\n",
		       (*alist)->filename ? (*alist)->filename : "?",
		       error_description(errno)));

	    fs_give((void **)&(*alist)->filename);
	}

	if((*alist)->size)
          fs_give((void **)&(*alist)->size);

	if((*alist)->id)
          fs_give((void **)&(*alist)->id);

	fs_give((void **)alist);

	*alist = leading;
    }
}
    

void
set_body_size(struct mail_bodystruct *b)
{
    unsigned char c;
    int we_cancel = 0;

    we_cancel = busy_cue(NULL, NULL, 1);
    so_seek((STORE_S *)b->contents.text.data, 0L, 0);
    b->size.bytes = 0L;
    while(so_readc(&c, (STORE_S *)b->contents.text.data))
      b->size.bytes++;

    if(we_cancel)
      cancel_busy_cue(-1);
}


/*
 * view_as_rich - set the rich_header flag
 *
 *         name  - name of the header field
 *         deflt - default value to return if user didn't set it
 *
 *         Note: if the user tries to turn them all off with "", then
 *		 we take that to mean default, since otherwise there is no
 *		 way to get to the headers.
 */
int
view_as_rich(char *name, int deflt)
{
    char **p;
    char *q;

    p = ps_global->VAR_COMP_HDRS;

    if(p && *p && **p){
        for(; (q = *p) != NULL; p++){
	    if(!struncmp(q, name, strlen(name)))
	      return 0; /* 0 means we *do* view it by default */
	}

        return 1; /* 1 means it starts out hidden */
    }
    return(deflt);
}


/*
 * background_posting - return whether or not we're already in the process
 *			of posting
 */
int
background_posting(int gripe)
{
    if(ps_global->post){
	if(gripe)
	  q_status_message(SM_ORDER|SM_DING, 3, 3,
			   _("Can't post while posting!"));
	return(1);
    }

    return(0);
}


/*----------------------------------------------------------------------
    Validate the given subject relative to any news groups.
     
Args: none

Returns: always returns 1, but also returns error if
----*/      
int
valid_subject(char *given, char **expanded, char **error, BUILDER_ARG *fcc, int *mangled)
{
    struct headerentry *hp;

    if(expanded)
      *expanded = cpystr(given);

    if(error){
	/*
	 * Now look for any header entry we passed to pico that has to do
	 * with news.  If there's no subject, gripe.
	 */
	for(hp = pbf->headents; hp->prompt; hp++)
	  if(hp->help == h_composer_news){
	      if(hp->hd_text->text[0] && !*given)
		*error = cpystr(
			_("News postings MUST have a subject!  Please add one!"));

	      break;
	  }
    }

    return(0);
}


/*
 * This is the build_address used by the composer to check for an address
 * in the addrbook.
 *
 * Args: to      -- the passed in line to parse
 *       full_to -- Address of a pointer to return the full address in.
 *		    This will be allocated here and freed by the caller.
 *       error   -- Address of a pointer to return an error message in.
 *		    This will be allocated here and freed by the caller.
 *        barg   -- Address of a pointer to return the fcc in is in
 *		    fcc->tptr.  It will have already been allocated by the
 *		    caller but we may free it and reallocate if we wish.
 *		    Caller will free it.
 *
 * Result:  0 is returned if address was OK, 
 *         -1 if address wasn't OK.
 *
 * Side effect: Can flush addrbook entry cache entries so they need to be
 * re-fetched afterwords.
 */
int
build_address(char *to, char **full_to, char **error, BUILDER_ARG *barg, int *mangled)
{
    char   *p;
    int     ret_val, no_repo = 0, *save_nesting_level;
    BuildTo bldto;
    PrivateTop *pt = NULL;
    PrivateAffector *af = NULL;
    char   *fcc_local = NULL;
    jmp_buf save_jmp_buf;

    dprint((5, "- build_address - (%s)\n", to ? to : "nul"));

    /* check to see if to string is empty to avoid work */
    for(p = to; p && *p && isspace((unsigned char)*p); p++)
      ;/* do nothing */

    if(!p || !*p){
	if(full_to)
	  *full_to = cpystr(to ? to : "");  /* because pico does a strcmp() */

	return 0;
    }

    if(full_to != NULL)
      *full_to = (char *)NULL;

    if(error != NULL)
      *error = (char *)NULL;

    /* No guarantee cursor or status line is how we saved it */
    clear_cursor_pos();
    mark_status_unknown();

    if(ps_global->remote_abook_validity > 0 &&
       adrbk_check_and_fix_all(ab_nesting_level == 0, 0, 0) && mangled)
      *mangled |= BUILDER_SCREEN_MANGLED;

    /*
     * If we end up jumping back here because somebody else changed one of
     * our addrbooks out from underneath us, we may well leak some memory.
     * That's probably ok since this will be very rare.
     *
     * The reason for the memcpy of the jmp_buf is that we may actually
     * be indirectly calling this function from within the address book.
     * For example, we may be in the address book screen and then run
     * the ComposeTo command which puts us in the composer, then we call
     * build_address from there which resets addrbook_changed_unexpectedly.
     * Once we leave build_address we need to reset addrbook_changed_un...
     * because this position on the stack will no longer be valid.
     * Same is true of the other setjmp's in this file which are wrapped
     * in memcpy calls.
     */
    save_nesting_level = cpyint(ab_nesting_level);
    memcpy(save_jmp_buf, addrbook_changed_unexpectedly, sizeof(jmp_buf));
    if(setjmp(addrbook_changed_unexpectedly)){
	no_repo = 0;
	pt = NULL;
	af = NULL;
	fcc_local = NULL;
	if(error != NULL)
	  *error = (char *)NULL;

	if(full_to && *full_to)
	  fs_give((void **)full_to);

	q_status_message(SM_ORDER, 3, 5, "Resetting address book...");
	dprint((1,
	    "RESETTING address book... build_address(%s)!\n", to ? to : "?"));
	addrbook_reset();
        ab_nesting_level = *save_nesting_level;
    }

    ab_nesting_level++;
    bldto.type    = Str;
    bldto.arg.str = to;
    ret_val = build_address_internal(bldto, full_to, error,
				     barg ? &fcc_local : NULL,
				     &no_repo, NULL, save_and_restore,
				     0, mangled);
    ab_nesting_level--;
    if(save_nesting_level)
      fs_give((void **)&save_nesting_level);

    /*
     * Have to rfc1522_decode the full_to string before sending it back.
     */
    if(full_to && *full_to ){
	char   *q;
	size_t  len;

	len = 4*strlen(*full_to)+1;
	q = (char *)fs_get(len * sizeof(char));
	p = (char *)rfc1522_decode_to_utf8((unsigned char *)q, len, *full_to);

	/* p == q means that decoding happened, p is decoded *full_to */
	if(p == q){
	    fs_give((void **)full_to);
	    *full_to = p;
	}
	else
	  fs_give((void **)&q);
    }

    if(fcc_local){
	unsigned long csum;

	/* Pt will point to headents[Fcc].bldr_private */
	pt = NULL;
	if(barg && barg->aff)
	  pt = (PrivateTop *)(*barg->aff);

	/*
	 * If *barg->aff is set, that means fcc was set from a list
	 * during some previous builder call.
	 * If the current To line contains the old expansion as a prefix, then
	 * we should leave things as they are. In order to decide that,
	 * we look at a hash value computed from the strings.
	 */
	if(pt && (af=pt->affector) && af->who == BP_To){
	    int len;

	    len = strlen(to);
	    if(len >= af->cksumlen){
		int save;

		save = to[af->cksumlen];
		to[af->cksumlen] = '\0';
		csum = line_hash(to);
		to[af->cksumlen] = save;
	    }
	    else
	      csum = af->cksumval + 1;  /* something not equal to cksumval */
	}

	if(!pt ||
	   !pt->affector ||
	   (pt->affector->who == BP_To && csum != pt->affector->cksumval)){

	    /* replace fcc value */
	    if(barg->tptr)
	      fs_give((void **)&barg->tptr);

	    barg->tptr = fcc_local;

	    if(barg->aff){
		if(!pt){
		    *barg->aff = (void *)fs_get(sizeof(PrivateTop));
		    pt = (PrivateTop *)(*barg->aff);
		    memset((void *)pt, 0, sizeof(PrivateTop));
		}

		if(no_repo){
		    if(!pt->affector)
		      pt->affector =
			    (PrivateAffector *)fs_get(sizeof(PrivateAffector));
		    
		    af = pt->affector;
		    af->who = BP_To;
		    af->cksumlen = strlen(((full_to && *full_to)
							    ? *full_to : ""));
		    af->cksumval = line_hash(((full_to && *full_to)
							    ? *full_to : ""));
		}
		else{
		    /*
		     * If result is reproducible, we don't keep track here.
		     */
		    if(pt->affector)
		      fs_give((void **)&pt->affector);
		}
	    }
	}
	else
	  fs_give((void **)&fcc_local);  /* unused in this case */
    }

    /* This is so pico will erase the old message */
    if(error != NULL && *error == NULL)
      *error = cpystr("");

    memcpy(addrbook_changed_unexpectedly, save_jmp_buf, sizeof(jmp_buf));
    flush_status_messages(1);
    return(ret_val);
}


/*
 * This is the builder used by the composer for the Lcc line.
 *
 * Args:     lcc -- the passed in Lcc line to parse
 *      full_lcc -- Address of a pointer to return the full address in.
 *		    This will be allocated here and freed by the caller.
 *         error -- Address of a pointer to return an error message in.
 *                  This is not allocated so should not be freed by the caller.
 *          barg -- This is a pointer to text for affected entries which
 *		    we may be changing.  The first one in the list is the
 *		    To entry.  We may put the name of the list in empty
 *		    group syntax form there (like  List Name: ;).
 *		    The second one in the list is the fcc field.
 *		    The tptr members already point to text allocated in the
 *		    caller. We may free and reallocate here, caller will
 *		    free the result in any case.
 *
 * Result:  0 is returned if address was OK, 
 *         -1 if address wasn't OK.
 *
 * Side effect: Can flush addrbook entry cache entries so they need to be
 * re-fetched afterwords.
 */
int
build_addr_lcc(char *lcc, char **full_lcc, char **error, BUILDER_ARG *barg, int *mangled)
{
    int		     ret_val,
		     no_repo = 0;	/* fcc or lcc not reproducible */
    int		    *save_nesting_level;
    BuildTo	     bldlcc;
    PrivateTop      *pt = NULL;
    PrivateAffector *af = NULL;
    char	    *p,
		    *fcc_local = NULL,
		    *to = NULL,
		    *dummy;
    jmp_buf          save_jmp_buf;

    dprint((5, "- build_addr_lcc - (%s)\n", lcc ? lcc : "nul"));

    /* check to see if to string is empty to avoid work */
    for(p = lcc; p && *p && isspace((unsigned char)*p); p++)
      ;/* do nothing */

    if(!p || !*p){
	if(full_lcc)
	  *full_lcc = cpystr(lcc ? lcc : ""); /* because pico does a strcmp() */

	return 0;
    }

    if(error != NULL)
      *error = (char *)NULL;

    if(ps_global->remote_abook_validity > 0 &&
       adrbk_check_and_fix_all(ab_nesting_level == 0, 0, 0) && mangled)
      *mangled |= BUILDER_SCREEN_MANGLED;

    /*
     * If we end up jumping back here because somebody else changed one of
     * our addrbooks out from underneath us, we may well leak some memory.
     * That's probably ok since this will be very rare.
     */
    save_nesting_level = cpyint(ab_nesting_level);
    memcpy(save_jmp_buf, addrbook_changed_unexpectedly, sizeof(jmp_buf));
    if(setjmp(addrbook_changed_unexpectedly)){
	no_repo = 0;
	pt = NULL;
	af = NULL;
	fcc_local = NULL;
	to = NULL;
	if(error != NULL)
	  *error = (char *)NULL;

	if(full_lcc && *full_lcc)
	  fs_give((void **)full_lcc);

	q_status_message(SM_ORDER, 3, 5, "Resetting address book...");
	dprint((1,
	    "RESETTING address book... build_address(%s)!\n", lcc ? lcc : "?"));
	addrbook_reset();
	ab_nesting_level = *save_nesting_level;
    }

    ab_nesting_level++;
    bldlcc.type    = Str;
    bldlcc.arg.str = lcc;

    /*
     * To is first affected_entry and Fcc is second.
     * The conditional stuff for the fcc argument says to only change the
     * fcc if the fcc pointer is passed in non-null, and the To pointer
     * is also non-null.  If they are null, that means they've already been
     * entered (are sticky).  We don't affect fcc if either fcc or To has
     * been typed in.
     */
    ret_val = build_address_internal(bldlcc,
		full_lcc,
		error,
		(barg && barg->next && barg->next->tptr && barg->tptr)
		  ? &fcc_local : NULL,
		&no_repo,
		(barg && barg->tptr) ? &to : NULL,
		save_and_restore, 0, mangled);

    ab_nesting_level--;
    if(save_nesting_level)
      fs_give((void **)&save_nesting_level);

    /* full_lcc is what ends up in the Lcc: line */
    if(full_lcc && *full_lcc){
	size_t len;

	/*
	 * Have to rfc1522_decode the full_lcc string before sending it back.
	 */
	len = 4*strlen(*full_lcc)+1;
	p = (char *)fs_get(len * sizeof(char));
	if(rfc1522_decode_to_utf8((unsigned char *)p, len, *full_lcc) == (unsigned char *)p){
	    fs_give((void **)full_lcc);
	    *full_lcc = p;
	}
	else
	  fs_give((void **)&p);
    }

    /* to is what ends up in the To: line */
    if(to && *to){
	unsigned long csum;
	size_t len;

	/*
	 * Have to rfc1522_decode the full_to string before sending it back.
	 */
	len = 4*strlen(to)+1;
	p = (char *)fs_get(len * sizeof(char));
	dummy = NULL;
	if(rfc1522_decode_to_utf8((unsigned char *)p, len, to) == (unsigned char *)p){
	    /*
	     * If the caller wants us to try to preserve the charset
	     * information (they set aff) we copy it into encoded->etext.
	     * We don't have to worry about pasting together pieces of
	     * etext like we do in build_address because whenever the
	     * Lcc line is setting the To line it will be setting the
	     * whole line, not modifying it.
	     * Pt will point to headents[To].bldr_private.
	     */
	    if(barg && barg->aff){
		pt = (PrivateTop *)(*barg->aff);

		if(!pt){
		    *barg->aff = (void *)fs_get(sizeof(PrivateTop));
		    pt = (PrivateTop *)(*barg->aff);
		    memset((void *)pt, 0, sizeof(PrivateTop));
		}
	    }

	    fs_give((void **)&to);
	    to = p;
	}
	else
	  fs_give((void **)&p);

	if(dummy)
	  fs_give((void **)&dummy);


	/*
	 * This part is recording the fact that the To line was set to
	 * what it is by entering something on the Lcc line. In particular,
	 * if a list alias was entered here then the fullname of the list
	 * goes in the To line. We save this affector information so that
	 * we can tell it shouldn't be modified if we call build_addr_lcc
	 * again unless we actually modified what's in the Lcc line so that
	 * it doesn't start with the same thing. The problem we're solving
	 * is that the contents of the Lcc line no longer look like the
	 * list they were derived from.
	 * Pt will point to headents[To].bldr_private.
	 */
	if(barg && barg->aff)
	  pt = (PrivateTop *)(*barg->aff);

	if(pt && (af=pt->affector) && af->who == BP_Lcc){
	    int len;

	    len = strlen(lcc);
	    if(len >= af->cksumlen){
		int save;

		save = lcc[af->cksumlen];
		lcc[af->cksumlen] = '\0';
		csum = line_hash(lcc);
		lcc[af->cksumlen] = save;
	    }
	    else
	      csum = af->cksumval + 1;		/* so they aren't equal */
	}

	if(!pt ||
	   !pt->affector ||
	   pt->affector->who != BP_Lcc ||
	   (pt->affector->who == BP_Lcc && csum != pt->affector->cksumval)){

	    /* replace to value */
	    if(barg->tptr && barg->tptr[0]){
		size_t l;
		char *t;

		l = strlen(barg->tptr) + strlen(to ? to : "") + 2;
		t = (char *)fs_get((l+1) * sizeof(char));
		snprintf(t, l+1, "%s%s%s",
			 barg->tptr,
			 (to && *to) ? ", " : "",
			 (to && *to) ? to : "");
		fs_give((void **)&barg->tptr);
		if(to)
		  fs_give((void **)&to);

		barg->tptr = t;
	    }
	    else{
		if(barg->tptr)
		  fs_give((void **)&barg->tptr);

		barg->tptr = to;
	    }

	    if(barg->aff){
		if(!pt){
		    *barg->aff = (void *)fs_get(sizeof(PrivateTop));
		    pt = (PrivateTop *)(*barg->aff);
		    memset((void *)pt, 0, sizeof(PrivateTop));
		}

		if(no_repo){
		    if(!pt->affector)
		      pt->affector =
			    (PrivateAffector *)fs_get(sizeof(PrivateAffector));
		    
		    af = pt->affector;
		    af->who = BP_Lcc;
		    af->cksumlen = strlen(((full_lcc && *full_lcc)
							    ? *full_lcc : ""));
		    af->cksumval = line_hash(((full_lcc && *full_lcc)
							    ? *full_lcc : ""));
		}
		else{
		    /*
		     * If result is reproducible, we don't keep track here.
		     */
		    if(pt->affector)
		      fs_give((void **)&pt->affector);
		}
	    }
	}
	else
	  fs_give((void **)&to);  /* unused in this case */
    }

    if(fcc_local){
	unsigned long csum;

	/*
	 * If *barg->next->aff is set, that means fcc was set from a list
	 * during some previous builder call. If the current Lcc line
	 * contains the old expansion as a prefix, then we should leave
	 * things as they are. In order to decide that we look at a hash
	 * value computed from the strings.
	 * Pt will point to headents[Fcc].bldr_private
	 */
	pt = NULL;
	if(barg && barg->next && barg->next->aff)
	  pt = (PrivateTop *)(*barg->next->aff);

	if(pt && (af=pt->affector) && af->who == BP_Lcc){
	    int len;

	    len = strlen(lcc);
	    if(len >= af->cksumlen){
		int save;

		save = lcc[af->cksumlen];
		lcc[af->cksumlen] = '\0';
		csum = line_hash(lcc);
		lcc[af->cksumlen] = save;
	    }
	    else
	      csum = af->cksumval + 1;  /* something not equal to cksumval */
	}

	if(!pt ||
	   !pt->affector ||
	   pt->affector->who != BP_Lcc ||
	   (pt->affector->who == BP_Lcc && csum != pt->affector->cksumval)){

	    /* replace fcc value */
	    if(barg->next->tptr)
	      fs_give((void **)&barg->next->tptr);

	    barg->next->tptr = fcc_local;

	    if(barg->next->aff){
		if(!pt){
		    *barg->next->aff = (void *)fs_get(sizeof(PrivateTop));
		    pt = (PrivateTop *)(*barg->next->aff);
		    memset((void *)pt, 0, sizeof(PrivateTop));
		}

		if(no_repo){
		    if(!pt->affector)
		      pt->affector =
			    (PrivateAffector *)fs_get(sizeof(PrivateAffector));
		    
		    af = pt->affector;
		    af->who = BP_Lcc;
		    af->cksumlen = strlen(((full_lcc && *full_lcc)
							    ? *full_lcc : ""));
		    af->cksumval = line_hash(((full_lcc && *full_lcc)
							    ? *full_lcc : ""));
		}
		else{
		    /*
		     * If result is reproducible, we don't keep track here.
		     */
		    if(pt->affector)
		      fs_give((void **)&pt->affector);
		}
	    }
	}
	else
	  fs_give((void **)&fcc_local);  /* unused in this case */
    }


    if(error != NULL && *error == NULL)
      *error = cpystr("");

    memcpy(addrbook_changed_unexpectedly, save_jmp_buf, sizeof(jmp_buf));
    flush_status_messages(0);
    return(ret_val);
}


/*----------------------------------------------------------------------
    Verify and canonicalize news groups names. 
    Called from the message composer

Args:  given_group    -- List of groups typed by user
       expanded_group -- pointer to point to expanded list, which will be
			 allocated here and freed in caller.  If this is
			 NULL, don't attempt to validate.
       error          -- pointer to store error message
       fcc            -- pointer to point to fcc, which will be
			 allocated here and freed in caller

Returns:  0 if all is OK
         -1 if addresses weren't valid

Test the given list of newstroups against those recognized by our nntp
servers.  Testing by actually trying to open the list is much cheaper, both
in bandwidth and memory, than yanking the whole list across the wire.
  ----*/
int
news_build(char *given_group, char **expanded_group, char **error, BUILDER_ARG *fcc, int *mangled)
{
    int	  rv;
    char *fccptr = NULL;

    if(fcc && fcc->tptr)
      fccptr = cpystr(fcc->tptr);

    clear_cursor_pos();

    rv = news_grouper(given_group, expanded_group, error, &fccptr, news_build_busy);

    /* assign any new fcc to the BUILDER_ARG */
    if(fccptr){
	if(fcc){
	    /* it changed */
	    if(fcc->tptr && strcmp(fcc->tptr, fccptr)){
		fs_give((void **) &fcc->tptr);
		fcc->tptr = fccptr;
		fccptr = NULL;
	    }
	}

	if(fccptr)
	  fs_give((void **) &fccptr);
    }

    /* deal with any busy indicator */
    if(news_busy_cue){
	news_busy_cue = 0;
	cancel_busy_cue(0);
	mark_status_dirty();
	display_message('x');
	if(mangled)
	  *mangled |= BUILDER_MESSAGE_DISPLAYED;
    }


    return(rv);
}


void
news_build_busy(void)
{
    news_busy_cue = busy_cue("Validating newsgroup(s)", NULL, 0);
}


#if defined(DOS) || defined(OS2)

/*----------------------------------------------------------------------
    Verify that the necessary pieces are around to allow for
    message sending under DOS

Args:  strict -- tells us if a remote stream is required before
		 sending is permitted.

The idea is to make sure pine knows enough to put together a valid 
from line.  The things we MUST know are a user-id, user-domain and
smtp server to dump the message off on.  Typically these are 
provided in pine's configuration file, but if not, the user is
queried here.
 ----*/
int
dos_valid_from()
{
    char        prompt[100], answer[80];
    int         rc, i, flags;
    HelpType    help;

    /*
     * query for user name portion of address, use IMAP login
     * name as default
     */
    if(!ps_global->VAR_USER_ID || ps_global->VAR_USER_ID[0] == '\0'){
	NETMBX mb;
	int no_prompt_user_id = 0;

	if(ps_global->mail_stream && ps_global->mail_stream->mailbox
	   && mail_valid_net_parse(ps_global->mail_stream->mailbox, &mb)
	   && *mb.user){
	    strncpy(answer, mb.user, sizeof(answer)-1);
	    answer[sizeof(answer)-1] = '\0';
	}
	else if(F_ON(F_QUELL_USER_ID_PROMPT, ps_global)){
	    /* no user-id prompting if set */
	    no_prompt_user_id = 1;
	    rc = 0;
	    if(!ps_global->mail_stream)
	      do_broach_folder(ps_global->inbox_name,
			       ps_global->context_list, NULL, DB_INBOXWOCNTXT);
	    if(ps_global->mail_stream && ps_global->mail_stream->mailbox
	       && mail_valid_net_parse(ps_global->mail_stream->mailbox, &mb)
	       && *mb.user){
		strncpy(answer, mb.user, sizeof(answer)-1);
		answer[sizeof(answer)-1] = '\0';
	    }
	    else
	      answer[0] = '\0';
	}
	else
	  answer[0] = '\0';

	if(F_ON(F_QUELL_USER_ID_PROMPT, ps_global) && answer[0]){
	    /* No prompt, just assume mailbox login is user-id */
	    no_prompt_user_id = 1; 
	    rc = 0;
	}

	snprintf(prompt,sizeof(prompt),_("User-id for From address : ")); 
	prompt[sizeof(prompt)-1] = '\0';

	help = NO_HELP;
	while(!no_prompt_user_id) {
	    flags = OE_APPEND_CURRENT;
	    rc = optionally_enter(answer,-FOOTER_ROWS(ps_global),0,
				  sizeof(answer),prompt,NULL,help,&flags);
	    if(rc == 2)
	      continue;

	    if(rc == 3){
		help = (help == NO_HELP) ? h_sticky_user_id : NO_HELP;
		continue;
	    }

	    if(rc != 4)
	      break;
	}

	if(rc == 1 || (rc == 0 && !answer[0])) {
	    q_status_message(SM_ORDER, 3, 4,
		   _("Send cancelled (User-id must be provided before sending)"));
	    return(0);
	}

	/* save the name */
	snprintf(prompt, sizeof(prompt), _("Preserve %.*s as \"user-id\" in PINERC"),
		sizeof(prompt)-50, answer);
	prompt[sizeof(prompt)-1] = '\0';
	if(ps_global->blank_user_id
	   && !no_prompt_user_id
	   && want_to(prompt, 'y', 'n', NO_HELP, WT_NORM) == 'y'){
	    set_variable(V_USER_ID, answer, 1, 1, Main);
	}
	else{
            fs_give((void **)&(ps_global->VAR_USER_ID));
	    ps_global->VAR_USER_ID = cpystr(answer);
	}
    }

    /* query for personal name */
    if(!ps_global->VAR_PERSONAL_NAME || ps_global->VAR_PERSONAL_NAME[0]=='\0'
	&& F_OFF(F_QUELL_PERSONAL_NAME_PROMPT, ps_global)){
	answer[0] = '\0';
	snprintf(prompt, sizeof(prompt), _("Personal name for From address : ")); 
	prompt[sizeof(prompt)-1] = '\0';

	help = NO_HELP;
	while(1) {
	    flags = OE_APPEND_CURRENT;
	    rc = optionally_enter(answer,-FOOTER_ROWS(ps_global),0,
				  sizeof(answer),prompt,NULL,help,&flags);
	    if(rc == 2)
	      continue;

	    if(rc == 3){
		help = (help == NO_HELP) ? h_sticky_personal_name : NO_HELP;
		continue;
	    }

	    if(rc != 4)
	      break;
    	}

	if(rc == 0 && answer){		/* save the name */
	    snprintf(prompt, sizeof(prompt), _("Preserve %.*s as \"personal-name\" in PINERC"),
		    sizeof(prompt)-50, answer);
	    prompt[sizeof(prompt)-1] = '\0';
	    if(ps_global->blank_personal_name
	       && want_to(prompt, 'y', 'n', NO_HELP, WT_NORM) == 'y'){
		set_variable(V_PERSONAL_NAME, answer, 1, 1, Main);
	    }
	    else{
        	fs_give((void **)&(ps_global->VAR_PERSONAL_NAME));
		ps_global->VAR_PERSONAL_NAME = cpystr(answer);
	    }
	}
    }

    /* 
     * query for host/domain portion of address, using IMAP
     * host as default
     */
    if(ps_global->blank_user_domain 
       || ps_global->maildomain == ps_global->localdomain
       || ps_global->maildomain == ps_global->hostname){
	if(ps_global->inbox_name[0] == '{'){
	    for(i=0;
		i < sizeof(answer)-1 && ps_global->inbox_name[i+1] != '}'; i++)
		answer[i] = ps_global->inbox_name[i+1];

	    answer[i] = '\0';
	}
	else
	  answer[0] = '\0';

	snprintf(prompt,sizeof(prompt),_("Host/domain for From address : ")); 
	prompt[sizeof(prompt)-1] = '\0';

	help = NO_HELP;
	while(1) {
	    flags = OE_APPEND_CURRENT;
	    rc = optionally_enter(answer,-FOOTER_ROWS(ps_global),0,
				  sizeof(answer),prompt,NULL,help,&flags);
	    if(rc == 2)
	      continue;

	    if(rc == 3){
		help = (help == NO_HELP) ? h_sticky_domain : NO_HELP;
		continue;
	    }

	    if(rc != 4)
	      break;
	}

	if(rc == 1 || (rc == 0 && !answer[0])) {
	    q_status_message(SM_ORDER, 3, 4,
	  _("Send cancelled (Host/domain name must be provided before sending)"));
	    return(0);
	}

	/* save the name */
	snprintf(prompt, sizeof(prompt), _("Preserve %.*s as \"user-domain\" in PINERC"),
		sizeof(prompt)-50, answer);
	prompt[sizeof(prompt)-1] = '\0';
	if(!ps_global->userdomain && !ps_global->blank_user_domain
	   && want_to(prompt, 'y', 'n', NO_HELP, WT_NORM) == 'y'){
	    set_variable(V_USER_DOMAIN, answer, 1, 1, Main);
	    fs_give((void **)&(ps_global->maildomain));	/* blast old val */
	    ps_global->userdomain      = cpystr(answer);
	    ps_global->maildomain      = ps_global->userdomain;
	}
	else{
            fs_give((void **)&(ps_global->maildomain));
            ps_global->userdomain = cpystr(answer);
	    ps_global->maildomain = ps_global->userdomain;
	}
    }

    /* check for smtp server */
    if(!ps_global->VAR_SMTP_SERVER ||
       !ps_global->VAR_SMTP_SERVER[0] ||
       !ps_global->VAR_SMTP_SERVER[0][0]){
	char **list;

	if(ps_global->inbox_name[0] == '{'){
	    for(i=0;
		i < sizeof(answer)-1 && ps_global->inbox_name[i+1] != '}'; i++)
	      answer[i] = ps_global->inbox_name[i+1];

	    answer[i] = '\0';
	}
	else
          answer[0] = '\0';

        snprintf(prompt,sizeof(prompt),_("SMTP server to forward message : ")); 
	prompt[sizeof(prompt)-1] = '\0';

	help = NO_HELP;
        while(1) {
	    flags = OE_APPEND_CURRENT;
            rc = optionally_enter(answer,-FOOTER_ROWS(ps_global),0,
				  sizeof(answer),prompt,NULL,help,&flags);
            if(rc == 2)
                  continue;

	    if(rc == 3){
		help = (help == NO_HELP) ? h_sticky_smtp : NO_HELP;
		continue;
	    }

            if(rc != 4)
                  break;
        }

        if(rc == 1 || (rc == 0 && answer[0] == '\0')) {
            q_status_message(SM_ORDER, 3, 4,
	       _("Send cancelled (SMTP server must be provided before sending)"));
            return(0);
        }

	/* save the name */
        list    = (char **) fs_get(2 * sizeof(char *));
	list[0] = cpystr(answer);
	list[1] = NULL;
	set_variable_list(V_SMTP_SERVER, list, TRUE, Main);
	fs_give((void *)&list[0]);
	fs_give((void *)list);
    }

    return(1);
}

#endif /* defined(DOS) || defined(OS2) */
