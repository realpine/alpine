#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: roleconf.c 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $";
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

#include "headers.h"
#include "roleconf.h"
#include "colorconf.h"
#include "conftype.h"
#include "confscroll.h"
#include "keymenu.h"
#include "status.h"
#include "radio.h"
#include "reply.h"
#include "folder.h"
#include "addrbook.h"
#include "mailcmd.h"
#include "setup.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/msgno.h"
#include "../pith/bitmap.h"
#include "../pith/sort.h"
#include "../pith/addrstring.h"
#include "../pith/list.h"
#include "../pith/flag.h"
#include "../pith/bldaddr.h"
#include "../pith/news.h"
#include "../pith/util.h"
#include "../pith/detoken.h"
#include "../pith/icache.h"
#include "../pith/ablookup.h"
#include "../pith/pattern.h"
#include "../pith/tempfile.h"


#define NOT		"! "
#define NOTLEN		2


#define ARB_HELP _("HELP FOR ARBITRARY HEADER PATTERNS")
#define ADDXHDRS _("Add Extra Headers")


/*
 * Internal prototypes
 */
int      role_select_tool(struct pine *, int, CONF_S **, unsigned);
PATTERN_S *addrlst_to_pattern(ADDRESS *);
void     role_config_init_disp(struct pine *, CONF_S **, long, PAT_STATE *);
void     add_patline_to_display(struct pine *, CONF_S **, int, CONF_S **, CONF_S **, PAT_LINE_S *, long);
void     add_role_to_display(CONF_S **, PAT_LINE_S *, PAT_S *, int, CONF_S **, int, long);
void     add_fake_first_role(CONF_S **, int, long);
int      role_config_tool(struct pine *, int, CONF_S **, unsigned);
int      role_config_add(struct pine *, CONF_S **, long);
int      role_config_replicate(struct pine *, CONF_S **, long);
int      role_config_edit(struct pine *, CONF_S **, long);
int      role_config_del(struct pine *, CONF_S **, long);
void     delete_a_role(CONF_S **, long);
int      role_config_shuffle(struct pine *, CONF_S **);
int      role_config_addfile(struct pine *, CONF_S **, long);
int      role_config_delfile(struct pine *, CONF_S **, long);
void     swap_literal_roles(CONF_S *, CONF_S *);
void     swap_file_roles(CONF_S *, CONF_S *);
void     move_role_into_file(CONF_S **, int);
void     move_role_outof_file(CONF_S **, int);
void     move_role_around_file(CONF_S **, int);
int      role_config_edit_screen(struct pine *, PAT_S *, char *, long, PAT_S **);
void     setup_dummy_pattern_var(struct variable *, char *, PATTERN_S *);
void     setup_role_pat(struct pine *, CONF_S **, struct variable *, HelpType, char *,
			struct key_menu *,
		        int (*tool)(struct pine *, int, CONF_S **, unsigned),
			EARB_S **, int);
void     setup_role_pat_alt(struct pine *, CONF_S **, struct variable *, HelpType, char *,
			    struct key_menu *,
			    int (*tool)(struct pine *, int, CONF_S **, unsigned),
			    int, int);
void     free_earb(EARB_S **);
void     calculate_inick_stuff(struct pine *);
int      check_role_folders(char **, unsigned);
void     maybe_add_to_incoming(CONTEXT_S *, char *);
int	 role_filt_exitcheck(CONF_S **, unsigned);
int	 role_filt_text_tool(struct pine *, int, CONF_S **, unsigned);
int	 role_filt_addhdr_tool(struct pine *, int, CONF_S **, unsigned);
int	 role_addhdr_tool(struct pine *, int, CONF_S **, unsigned);
int	 role_filt_radiobutton_tool(struct pine *, int, CONF_S **, unsigned);
int	 role_sort_tool(struct pine *, int, CONF_S **, unsigned);
char   **get_role_specific_folder(CONF_S **);
int      role_litsig_text_tool(struct pine *, int, CONF_S **, unsigned);
int      role_cstm_text_tool(struct pine *, int, CONF_S **, unsigned);
int      role_text_tool(struct pine *, int, CONF_S **, unsigned);
int      role_text_tool_inick(struct pine *, int, CONF_S **, unsigned);
int      role_text_tool_kword(struct pine *, int, CONF_S **, unsigned);
int      role_text_tool_charset(struct pine *, int, CONF_S **, unsigned);
int      role_text_tool_afrom(struct pine *, int, CONF_S **, unsigned);
char    *role_type_print(char *, size_t, char *, long);
int	 feat_checkbox_tool(struct pine *, int, CONF_S **, unsigned);
void	 toggle_feat_option_bit(struct pine *, int, struct variable *, char *);
NAMEVAL_S *feat_feature_list(int);
int	 inabook_checkbox_tool(struct pine *, int, CONF_S **, unsigned);
void	 toggle_inabook_type_bit(struct pine *, int, struct variable *, char *);
NAMEVAL_S *inabook_feature_list(int);


static char *set_choose = "---  --------------------";
static long role_global_flags;
static PAT_STATE *role_global_pstate;


int
role_select_screen(struct pine *ps, ACTION_S **role, int alt_compose)
{
    CONF_S        *ctmp = NULL, *first_line = NULL;
    OPT_SCREEN_S   screen;
    PAT_S         *pat, *sel_pat = NULL;
    int            ret = -1;
    int            change_default = 0;
    long           rflags = ROLE_DO_ROLES;
    char          *helptitle;
    HelpType       help;
    PAT_STATE      pstate;

    if(!role)
      return(ret);

    *role = NULL;

    if(!(nonempty_patterns(rflags, &pstate) &&
         first_pattern(&pstate))){
	q_status_message(SM_ORDER, 3, 3,
			 _("No roles available. Use Setup/Rules to add roles."));
	return(ret);
    }


    if(alt_compose){
	menu_init_binding(&role_select_km,
	  alt_compose == MC_FORWARD ? 'F' :
	   alt_compose == MC_REPLY ? 'R' :
	    alt_compose == MC_COMPOSE ? 'C' : 'B',
	  MC_CHOICE,
	  alt_compose == MC_FORWARD ? "F" :
	   alt_compose == MC_REPLY ? "R" :
	    alt_compose == MC_COMPOSE ? "C" : "B",
	  alt_compose == MC_FORWARD ? "[" N_("ForwardAs") "]" :
	   alt_compose == MC_REPLY ? "[" N_("ReplyAs") "]" :
	    alt_compose == MC_COMPOSE ? "[" N_("ComposeAs") "]" : "[" N_("BounceAs") "]",
	  DEFAULT_KEY);
	menu_add_binding(&role_select_km, ctrl('J'), MC_CHOICE);
	menu_add_binding(&role_select_km, ctrl('M'), MC_CHOICE);
    }
    else{
	menu_init_binding(&role_select_km, 'S', MC_CHOICE, "S", "[" N_("Select") "]",
			  DEFAULT_KEY);
	menu_add_binding(&role_select_km, ctrl('J'), MC_CHOICE);
	menu_add_binding(&role_select_km, ctrl('M'), MC_CHOICE);
    }

    help      = h_role_select;
    if(alt_compose == MC_BOUNCE)
      helptitle = _("HELP FOR SELECTING A ROLE TO BOUNCE AS");
    else if(alt_compose)
      helptitle = _("HELP FOR SELECTING A ROLE TO COMPOSE AS");
    else
      helptitle = _("HELP FOR SELECTING A ROLE");

    menu_init_binding(&role_select_km, 'D', MC_TOGGLE, "D", "changeDef", CHANGEDEF_KEY); 

    for(pat = first_pattern(&pstate);
	pat;
	pat = next_pattern(&pstate)){
	new_confline(&ctmp);
	if(!first_line)
	  first_line = ctmp;

	ctmp->value        = cpystr((pat->patgrp && pat->patgrp->nick)
					? pat->patgrp->nick : "?");
	ctmp->d.r.selected = &sel_pat;
	ctmp->d.r.pat      = pat;
	ctmp->d.r.change_def = &change_default;
	ctmp->keymenu      = &role_select_km;
	ctmp->help         = help;
	ctmp->help_title   = helptitle;
	ctmp->tool         = role_select_tool;
	ctmp->flags        = CF_STARTITEM;
	ctmp->valoffset    = 4;
    }

    memset(&screen, 0, sizeof(screen));
    /* TRANSLATORS: Print something1 using something2.
       "roles" is something1 */
    (void)conf_scroll_screen(ps, &screen, first_line, _("SELECT ROLE"),
			     _("roles"), 0);

    if(sel_pat){
	*role = sel_pat->action;
	if(change_default == 1)
	  ps_global->default_role = *role;
	else if(change_default == 2)
	  ps_global->default_role = NULL;

	ret = 0;
    }

    ps->mangled_screen = 1;
    return(ret);
}


int
role_select_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int retval = 0, newval;

    switch(cmd){
      case MC_CHOICE :
	*((*cl)->d.r.selected) = (*cl)->d.r.pat;
	retval = simple_exit_cmd(flags);
	break;

      case MC_TOGGLE :
	newval = (*((*cl)->d.r.change_def) + 1) % 3;
	*((*cl)->d.r.change_def) = newval;
	menu_init_binding((*cl)->keymenu, 'D', MC_TOGGLE, "D",
	    (newval == 0) ? "changeDef" : (newval == 1) ? "removeDef" : "leaveDef",
	    CHANGEDEF_KEY); 
	if(newval == 1){
	    if(ps_global->default_role)
	      q_status_message(SM_ORDER, 0, 3,
		  _("Default role will be changed to the role you Select"));
	    else
	      q_status_message(SM_ORDER, 0, 3,
		  _("Default role will be set to the role you Select"));
	}
	else if(newval == 2){
	    q_status_message(SM_ORDER, 0, 3, _("Default role will be unset"));
	}
	else{		/* newval == 0 */
	    if(ps_global->default_role)
	      q_status_message(SM_ORDER, 0, 3, _("Default role will remain unchanged"));
	    else
	      q_status_message(SM_ORDER, 0, 3, _("Default role will remain unset"));
	}

	ps->mangled_footer = 1;
	retval = 0;
	break;

      case MC_EXIT :
        retval = simple_exit_cmd(flags);
	break;

      default:
	retval = -1;
	break;
    }

    if(retval > 0)
      ps->mangled_body = 1;

    return(retval);
}


void
role_config_screen(struct pine *ps, long int rflags, int edit_exceptions)
{
    CONF_S      *first_line;
    OPT_SCREEN_S screen;
    char         title[100];
    int          readonly_warning = 0;
    PAT_STATE    pstate;
    struct variable *v = NULL;

    dprint((4, "role_config_screen()\n"));

    if(ps->fix_fixed_warning)
      offer_to_fix_pinerc(ps);

    ew = edit_exceptions ? ps_global->ew_for_except_vars : Main;

    if(ps->restricted)
      readonly_warning = 1;
    else{
	PINERC_S *prc = NULL;

	switch(ew){
	  case Main:
	    prc = ps->prc;
	    rflags |= PAT_USE_MAIN;
	    break;
	  case Post:
	    prc = ps->post_prc;
	    rflags |= PAT_USE_POST;
	    break;
	  default:
	    break;
	}

	readonly_warning = prc ? prc->readonly : 1;
	if(prc && prc->quit_to_edit){
	    quit_to_edit_msg(prc);
	    return;
	}
    }

    if(!any_patterns(rflags, &pstate))
      return;
    
    if(rflags & ROLE_DO_ROLES)
      v = &ps_global->vars[V_PAT_ROLES];
    else if(rflags & ROLE_DO_INCOLS)
      v = &ps_global->vars[V_PAT_INCOLS];
    else if(rflags & ROLE_DO_OTHER)
      v = &ps_global->vars[V_PAT_OTHER];
    else if(rflags & ROLE_DO_SCORES)
      v = &ps_global->vars[V_PAT_SCORES];
    else if(rflags & ROLE_DO_FILTER)
      v = &ps_global->vars[V_PAT_FILTS];
    else if(rflags & ROLE_DO_SRCH)
      v = &ps_global->vars[V_PAT_SRCH];

    if((ps_global->ew_for_except_vars != Main) && (ew == Main)){
	char           **lval;

	if((lval=LVAL(v, ps_global->ew_for_except_vars)) &&
	   lval[0] && strcmp(INHERIT, lval[0]) != 0){
	    role_type_print(title, sizeof(title), _("Warning: \"%sRules\" are overridden in your exceptions configuration"), rflags);
	    q_status_message(SM_ORDER, 7, 7, title);
	}
    }

    role_type_print(title, sizeof(title), "%sRules", rflags);
    if(fixed_var(v, "change", title))
      return;

uh_oh:
    first_line = NULL;

    snprintf(title, sizeof(title), "SETUP%s ", edit_exceptions ? " EXCEPTIONAL" : "");
    title[sizeof(title)-1] = '\0';
    role_type_print(title+strlen(title), sizeof(title)-strlen(title), "%sRULES", rflags);
    role_global_flags = rflags;
    role_global_pstate = &pstate;
    role_config_init_disp(ps, &first_line, rflags, &pstate);
    
    if(!first_line){
	role_global_flags = 0;
	ps->mangled_screen = 1;
	q_status_message(SM_ORDER,5,5,
		    _("Unexpected problem: config file modified externally?"));
	q_status_message1(SM_ORDER,5,5,
	 _("Perhaps a newer version of pine was used to set variable \"%s\"?"),
	    v ? v->name : "?");
	dprint((1, "Unexpected problem: config file modified externally?\nPerhaps by a newer pine? Variable \"%s\" has unexpected contents.\n",
	(v && v->name) ? v->name : "?"));
	return;
    }

    memset(&screen, 0, sizeof(screen));
    screen.deferred_ro_warning = readonly_warning;
    /* TRANSLATORS: Print something1 using something2.
       "rules" is something1 */
    switch(conf_scroll_screen(ps, &screen, first_line, title, _("rules"), 0)){
	case 0:
	  break;

	case 10:
	  /* flush changes and re-read orig */
	  close_patterns(rflags);
	  break;
	
	case 1:
	  if(write_patterns(rflags))
	    goto uh_oh;

	  /*
	   * Flush out current_vals of anything we've possibly changed.
	   */

	  if(ps_global->default_role){
	      q_status_message(SM_ORDER,0,3, "Default role is unset");
	      ps_global->default_role = NULL;
	  }

	  close_patterns((rflags & ROLE_MASK) | PAT_USE_CURRENT);

	  /* scores may have changed */
	  if(rflags & ROLE_DO_SCORES){
	      int         i;
	      MAILSTREAM *m;

	      for(i = 0; i < ps_global->s_pool.nstream; i++){
		  m = ps_global->s_pool.streams[i];
		  if(m){
		    clear_folder_scores(m);
		    clear_index_cache(m, 0);
		  }
	      }
	      
	      if(mn_get_sort(sp_msgmap(ps_global->mail_stream)) == SortScore)
	        refresh_sort(ps_global->mail_stream,
			     sp_msgmap(ps_global->mail_stream), SRT_VRB);
	  }

	  /* recalculate need for scores */
	  scores_are_used(SCOREUSE_INVALID);

	  /* we may want to fetch more or fewer headers each fetch */ 
	  calc_extra_hdrs();
	  if(get_extra_hdrs())
	    (void) mail_parameters(NULL, SET_IMAPEXTRAHEADERS,
				   (void *) get_extra_hdrs());

	  if(rflags & ROLE_DO_INCOLS && pico_usingcolor())
	    clear_index_cache(ps_global->mail_stream, 0);

	  if(rflags & ROLE_DO_FILTER)
	    role_process_filters();

	  /*
	   * ROLE_DO_OTHER is made up of a bunch of different variables
	   * that may have changed. Assume they all changed and fix them.
	   * Similarly for ROLE_DO_INCOLS.
	   */
	  if(rflags & (ROLE_DO_OTHER|ROLE_DO_INCOLS)){
	      reset_index_format();
	      clear_index_cache(ps_global->mail_stream, 0);
	      if(!mn_get_mansort(ps_global->msgmap))
		reset_sort_order(SRT_VRB);
	  }

	  break;
	
	default:
	  q_status_message(SM_ORDER,7,10, "conf_scroll_screen unexpected ret");
	  break;
    }

    role_global_flags = 0;
    ps->mangled_screen = 1;
}


/*
 * This is called from process_cmd to add a new pattern to the end of the
 * list of patterns. The pattern is seeded with values from the current
 * message.
 */
void
role_take(struct pine *ps, MSGNO_S *msgmap, int rtype)
{
    PAT_S       *defpat, *newpat = NULL;
    PAT_LINE_S  *new_patline, *patline;
    ENVELOPE    *env = NULL;
    long         rflags;
    char        *s, title[100], specific_fldr[MAXPATH+1];
    PAT_STATE    pstate;
    EditWhich    ew;

    dprint((4, "role_take()\n"));

    if(mn_get_cur(msgmap) > 0){
	env = pine_mail_fetchstructure(ps->mail_stream,
				       mn_m2raw(msgmap, mn_get_cur(msgmap)),
				       NULL);
    
	if(!env){
	    q_status_message(SM_ORDER, 3, 7,
			     _("problem getting addresses from message"));
	    return;
	}
    }

    switch(rtype){
      case 'r':
	rflags = ROLE_DO_ROLES;
	ew = ps_global->ew_for_role_take;
	break;
      case 's':
	rflags = ROLE_DO_SCORES;
	ew = ps_global->ew_for_score_take;
	break;
      case 'i':
	rflags = ROLE_DO_INCOLS;
	ew = ps_global->ew_for_incol_take;
	break;
      case 'f':
	rflags = ROLE_DO_FILTER;
	ew = ps_global->ew_for_filter_take;
	break;
      case 'o':
	rflags = ROLE_DO_OTHER;
	ew = ps_global->ew_for_other_take;
	break;
      case 'c':
	rflags = ROLE_DO_SRCH;
	ew = ps_global->ew_for_srch_take;
	break;

      default:
	cmd_cancelled(NULL);
	return;
    }

    switch(ew){
      case Main:
	rflags |= PAT_USE_MAIN;
	break;
      case Post:
	rflags |= PAT_USE_POST;
	break;
      default:
	break;
    }

    if(!any_patterns(rflags, &pstate)){
	q_status_message(SM_ORDER, 3, 7, _("problem accessing rules"));
	return;
    }

    /* set this so that even if we don't edit at all, we'll be asked */
    rflags |= ROLE_CHANGES;

    /*
     * Make a pattern out of the information in the envelope and
     * use that as the default pattern we give to the role editor.
     * It will have a pattern but no actions set.
     */
    defpat = (PAT_S *)fs_get(sizeof(*defpat));
    memset((void *)defpat, 0, sizeof(*defpat));

    defpat->patgrp = (PATGRP_S *)fs_get(sizeof(*defpat->patgrp));
    memset((void *)defpat->patgrp, 0, sizeof(*defpat->patgrp));

    if(env){
	if(env->to)
	  defpat->patgrp->to = addrlst_to_pattern(env->to);

	if(env->from)
	  defpat->patgrp->from = addrlst_to_pattern(env->from);

	if(env->cc)
	  defpat->patgrp->cc = addrlst_to_pattern(env->cc);

	if(env->sender &&
	   (!env->from || !address_is_same(env->sender, env->from)))
	  defpat->patgrp->sender = addrlst_to_pattern(env->sender);

	/*
	 * Env->newsgroups is already comma-separated and there shouldn't be
	 * any commas or backslashes in newsgroup names, so we don't add the
	 * roletake escapes.
	 */
	if(env->newsgroups)
	  defpat->patgrp->news = string_to_pattern(env->newsgroups);

	/*
	 * Subject may have commas or backslashes, so we add escapes.
	 */
	if(env->subject){
	    char *q, *t = NULL;

	    /*
	     * Mail_strip_subject not only strips the Re's and Fwd's but
	     * it also canonicalizes to UTF-8.
	     */
	    mail_strip_subject(env->subject, &q);
	    if(q != NULL){
		t = add_roletake_escapes(q);
		fs_give((void **)&q);
	    }

	    if(t){
		defpat->patgrp->subj = string_to_pattern(t);
		fs_give((void **)&t);
	    }
	}
    }
    
    if(IS_NEWS(ps->mail_stream))
      defpat->patgrp->fldr_type = FLDR_NEWS;
    else
      defpat->patgrp->fldr_type = FLDR_EMAIL;

    specific_fldr[0] = specific_fldr[sizeof(specific_fldr)-1] = '\0';
    if(sp_flagged(ps->mail_stream, SP_INBOX))
      strncpy(specific_fldr, ps_global->inbox_name, sizeof(specific_fldr)-1);
    else if(ps->context_current
	    && ps->context_current->use & CNTXT_INCMNG &&
	    folder_is_nick(ps->cur_folder, FOLDERS(ps->context_current), 0))
      strncpy(specific_fldr, ps->cur_folder, sizeof(specific_fldr)-1);
    else
      context_apply(specific_fldr, ps->context_current, ps->cur_folder,
		    sizeof(specific_fldr));
    
    if(specific_fldr[0]){
	s = add_comma_escapes(specific_fldr);
	if(s){
	    if(rtype == 'f')
	      defpat->patgrp->fldr_type = FLDR_SPECIFIC;

	    defpat->patgrp->folder = string_to_pattern(s);
	    fs_give((void **)&s);
	}
    }

    role_type_print(title, sizeof(title), "ADD NEW %sRULE", rflags);

    /*
     * Role_config_edit_screen is sometimes called as a tool or a sub
     * routine called from a tool within conf_scroll_screen, but here it
     * is going to be at the top-level (we're not inside conf_scroll_screen
     * right now). It uses opt_screen to set the ro_warning bit. We need
     * to let it know that we're at the top, which we do by setting
     * opt_screen to NULL. Otherwise, the thing that opt_screen is pointing
     * to is just random stack stuff from some previous conf_scroll_screen
     * call which has already exited.
     */
    opt_screen = NULL;

    if(role_config_edit_screen(ps, defpat, title, rflags,
			       &newpat) == 1 && newpat){

	if(ps->never_allow_changing_from && newpat->action &&
	   newpat->action->from)
	    q_status_message(SM_ORDER|SM_DING, 3, 7,
      _("Site policy doesn't allow changing From address so From is ignored"));

	if(rflags & ROLE_DO_ROLES && newpat->patgrp && newpat->patgrp->nick){
	    PAT_S *pat;
	    
	    for(pat = first_pattern(&pstate);
		pat;
		pat = next_pattern(&pstate)){
		if(pat->patgrp && pat->patgrp->nick &&
		   !strucmp(pat->patgrp->nick, newpat->patgrp->nick)){
		    q_status_message(SM_ORDER|SM_DING, 3, 7, _("Warning: The nickname of the new role is already in use."));
		    break;
		}
	    }
	}


	set_pathandle(rflags);

	/* need a new patline */
	new_patline = (PAT_LINE_S *)fs_get(sizeof(*new_patline));
	memset((void *)new_patline, 0, sizeof(*new_patline));
	new_patline->type = Literal;
	(*cur_pat_h)->dirtypinerc = 1;

	/* tie together with new pattern */
	new_patline->first = new_patline->last = newpat;
	newpat->patline = new_patline;

	/* find last current patline */
	for(patline = (*cur_pat_h)->patlinehead;
	    patline && patline->next;
	    patline = patline->next)
	  ;
	
	/* add new patline to end of list */
	if(patline){
	    patline->next = new_patline;
	    new_patline->prev = patline;
	}
	else
	  (*cur_pat_h)->patlinehead = new_patline;

	if(write_patterns(rflags) == 0){
	    char msg[60];

	    /*
	     * Flush out current_vals of anything we've possibly changed.
	     */

	    if(rflags & ROLE_DO_ROLES && ps_global->default_role){
		q_status_message(SM_ORDER,0,3, "Default role is unset");
		ps_global->default_role = NULL;
	    }

	    close_patterns(rflags | PAT_USE_CURRENT);

	    role_type_print(msg, sizeof(msg), "New %srule saved", rflags);
	    q_status_message(SM_ORDER, 0, 3, msg);

	    /* scores may have changed */
	    if(rflags & ROLE_DO_SCORES){
		int         i;
		MAILSTREAM *m;

		for(i = 0; i < ps_global->s_pool.nstream; i++){
		    m = ps_global->s_pool.streams[i];
		    if(m){
		      clear_folder_scores(m);
		      clear_index_cache(m, 0);
		    }
		}
	      
		/* We've already bound msgmap to global mail_stream
		 * at the start of this function, but if we wanted to
		 * we could clean this up.
		 */
		if(mn_get_sort(msgmap) == SortScore)
	          refresh_sort(ps_global->mail_stream, msgmap, SRT_VRB);
	    }

	    if(rflags & ROLE_DO_FILTER)
	      role_process_filters();

	    /* recalculate need for scores */
	    scores_are_used(SCOREUSE_INVALID);

	    /* we may want to fetch more or fewer headers each fetch */ 
	    calc_extra_hdrs();
	    if(get_extra_hdrs())
	      (void) mail_parameters(NULL, SET_IMAPEXTRAHEADERS,
				     (void *) get_extra_hdrs());

	    if(rflags & ROLE_DO_INCOLS && pico_usingcolor())
	      clear_index_cache(ps_global->mail_stream, 0);

	    /*
	     * ROLE_DO_OTHER is made up of a bunch of different variables
	     * that may have changed. Assume they all changed and fix them.
	     */
	    if(rflags & ROLE_DO_OTHER){
	        reset_index_format();
	        clear_index_cache(ps_global->mail_stream, 0);
		if(!mn_get_mansort(msgmap))
	          reset_sort_order(SRT_VRB);
	    }
	}
    }
    else
      cmd_cancelled(NULL);

    free_pat(&defpat);
    ps->mangled_screen = 1;
}


PATTERN_S *
addrlst_to_pattern(struct mail_address *addr)
{
    char      *s, *t, *u, *v;
    PATTERN_S *p = NULL;
    size_t l;

    if(addr){
	l = est_size(addr);
	t = s = (char *) fs_get((l+1) * sizeof(char));
	s[0] = '\0';
	while(addr){
	    u = simple_addr_string(addr, tmp_20k_buf, SIZEOF_20KBUF);
	    v = add_roletake_escapes(u);
	    if(v){
		if(*v && t != s)
		  sstrncpy(&t, ",", l-(t-s));

		sstrncpy(&t, v, l-(t-s));
		fs_give((void **)&v);
	    }

	    addr = addr->next;
	}

	s[l] = '\0';

	if(*s)
	  p = string_to_pattern(s);
	
	fs_give((void **) &s);
    }

    return(p);
}


void
role_config_init_disp(struct pine *ps, CONF_S **first_line, long int rflags, PAT_STATE *pstate)
{
    PAT_LINE_S    *patline;
    CONF_S        *ctmp = NULL;
    int            inherit = 0, added_fake = 0;

    if(first_line)
      *first_line = NULL;

    /*
     * Set cur_pat_h and manipulate directly.
     */
    set_pathandle(rflags);
    patline = *cur_pat_h ? (*cur_pat_h)->patlinehead : NULL;
    if(patline && patline->type == Inherit){
	add_patline_to_display(ps, &ctmp, 0, first_line, NULL, patline, rflags);
	patline = patline->next;
    }

    if(!patline){
	add_fake_first_role(&ctmp, 0, rflags);
	added_fake++;
	if(first_line && !*first_line)
	  (*first_line) = ctmp;
    }

    for(; patline; patline = patline->next)
      add_patline_to_display(ps, &ctmp, 0, first_line, NULL, patline, rflags);
    
    /*
     * If there are no actual patterns so far, we need to have an Add line
     * for the cursor to be on. This would happen if all of the patlines
     * were File includes and none of the files contained patterns.
     */
    if(!first_pattern(role_global_pstate) ||
       ((inherit=first_pattern(role_global_pstate)->inherit) &&
	 !next_pattern(role_global_pstate))){

	/*
	 * Find the start and prepend the fake first role.
	 */
	while(ctmp && ctmp->prev)
	  ctmp = ctmp->prev;

	if(!added_fake){
	    add_fake_first_role(&ctmp, inherit ? 0 : 1, rflags);
	    if(first_line && !*first_line)
	      (*first_line) = ctmp;
	}
    }
}


void
add_patline_to_display(struct pine *ps, CONF_S **ctmp, int before, CONF_S **first_line, CONF_S **top_line, PAT_LINE_S *patline, long int rflags)
{
    PAT_S *pat;
    int    len, firstitem, wid;
    char  *q;
    char   buf[6*MAX_SCREEN_COLS+1];

    /* put dashed line around file contents */
    if(patline->type == File){

	new_confline(ctmp);
	if(before){
	    /*
	     * New_confline appends ctmp after old current instead of inserting
	     * it, so we have to adjust. We have
	     *  <- a <-> b <-> p <-> c -> and want <- a <-> p <-> b <-> c ->
	     */

	    CONF_S *a, *b, *c, *p;

	    p = *ctmp;
	    b = (*ctmp)->prev;
	    c = (*ctmp)->next;
	    a = b ? b->prev : NULL;
	    if(a)
	      a->next = p;

	    if(b){
		b->prev = p;
		b->next = c;
	    }

	    if(c)
	      c->prev = b;

	    p->prev = a;
	    p->next = b;
	}

	if(top_line && *top_line == NULL)
	  *top_line = (*ctmp);

	len = strlen(patline->filename) + 100;

	q = (char *) fs_get((len + 1) * sizeof(char));
	snprintf(q, len+1, "From file %s%s", patline->filename,
		patline->readonly ? " (ReadOnly)" : ""); 
	q[len-1] = '\0';

	if((wid=utf8_width(q)) > ps->ttyo->screen_cols -2)
	  utf8_snprintf(buf, sizeof(buf), "--%.*w", ps->ttyo->screen_cols -2, q);
	else
	  snprintf(buf, sizeof(buf), "--%s%s", q, repeat_char(ps->ttyo->screen_cols -2-wid, '-'));

	(*ctmp)->value = cpystr(buf);

	fs_give((void **)&q);
	(*ctmp)->flags     |= (CF_NOSELECT | CF_STARTITEM);
	(*ctmp)->d.r.patline = patline;
	firstitem = 0;
    }
    else
      firstitem = 1;

    for(pat = patline->first; pat; pat = pat->next){
	
	/* Check that pattern has a role and is of right type */
	if(pat->inherit ||
	   (pat->action &&
	    (((rflags & ROLE_DO_ROLES)  && pat->action->is_a_role)  ||
	     ((rflags & ROLE_DO_INCOLS) && pat->action->is_a_incol) ||
	     ((rflags & ROLE_DO_SRCH)   && pat->action->is_a_srch)  ||
	     ((rflags & ROLE_DO_OTHER)  && pat->action->is_a_other) ||
	     ((rflags & ROLE_DO_SCORES) && pat->action->is_a_score) ||
	     ((rflags & ROLE_DO_FILTER) && pat->action->is_a_filter)))){
	    add_role_to_display(ctmp, patline, pat, 0,
				(first_line && *first_line == NULL)
				  ? first_line :
				    (top_line && *top_line == NULL)
				      ? top_line : NULL,
				firstitem, rflags);
	    firstitem = 1;
	    if(top_line && *top_line == NULL && first_line)
	      *top_line = *first_line;
	}

    }

    if(patline->type == File){
	new_confline(ctmp);
	len = strlen(patline->filename) + 100;

	q = (char *) fs_get((len + 1) * sizeof(char));
	snprintf(q, len+1, "End of Rules from %s", patline->filename); 
	q[len-1] = '\0';

	if((wid=utf8_width(q)) > ps->ttyo->screen_cols -2)
	  utf8_snprintf(buf, sizeof(buf), "--%.*w", ps->ttyo->screen_cols -2, q);
	else
	  snprintf(buf, sizeof(buf), "--%s%s", q, repeat_char(ps->ttyo->screen_cols -2-wid, '-'));

	(*ctmp)->value = cpystr(buf);

	fs_give((void **)&q);
	(*ctmp)->flags     |= CF_NOSELECT;
	(*ctmp)->d.r.patline = patline;
    }
}


void
add_role_to_display(CONF_S **ctmp, PAT_LINE_S *patline, PAT_S *pat, int before, CONF_S **first_line, int firstitem, long int rflags)
{
    char      title[80];

    if(!(pat && (pat->action || pat->inherit)))
      return;

    new_confline(ctmp);
    if(first_line && !pat->inherit)
      *first_line = *ctmp;

    if(before){
	/*
	 * New_confline appends ctmp after old current instead of inserting
	 * it, so we have to adjust. We have
	 *  <- a <-> b <-> p <-> c -> and want <- a <-> p <-> b <-> c ->
	 */

	CONF_S *a, *b, *c, *p;

	p = *ctmp;
	b = (*ctmp)->prev;
	c = (*ctmp)->next;
	a = b ? b->prev : NULL;
	if(a)
	  a->next = p;

	if(b){
	    b->prev = p;
	    b->next = c;
	}

	if(c)
	  c->prev = b;

	p->prev = a;
	p->next = b;
    }

    role_type_print(title, sizeof(title), _("HELP FOR %sRULE CONFIGURATION"), rflags);

    if(pat->inherit){
	(*ctmp)->flags    |= ((firstitem ? CF_STARTITEM : 0) |
			      CF_NOSELECT | CF_INHERIT);
    }
    else{
	(*ctmp)->flags    |= (firstitem ? CF_STARTITEM : 0);
	(*ctmp)->value     = cpystr((pat && pat->patgrp && pat->patgrp->nick)
				    ? pat->patgrp->nick : "?");
    }

    (*ctmp)->d.r.patline = patline;
    (*ctmp)->d.r.pat     = pat;
    (*ctmp)->keymenu     = &role_conf_km;
    (*ctmp)->help        = (rflags & ROLE_DO_INCOLS) ? h_rules_incols :
			    (rflags & ROLE_DO_OTHER)  ? h_rules_other :
			     (rflags & ROLE_DO_FILTER) ? h_rules_filter :
			      (rflags & ROLE_DO_SCORES) ? h_rules_score :
			       (rflags & ROLE_DO_ROLES)  ? h_rules_roles :
			        (rflags & ROLE_DO_SRCH)   ? h_rules_srch :
			        NO_HELP;
    (*ctmp)->help_title  = title;
    (*ctmp)->tool        = role_config_tool;
    (*ctmp)->valoffset   = 4;
}


void
add_fake_first_role(CONF_S **ctmp, int before, long int rflags)
{
    char title[80];
    char add[80];

    new_confline(ctmp);

    if(before){
	/*
	 * New_confline appends ctmp after old current instead of inserting
	 * it, so we have to adjust. We have
	 *  <- a <-> b <-> p <-> c -> and want <- a <-> p <-> b <-> c ->
	 */

	CONF_S *a, *b, *c, *p;

	p = *ctmp;
	b = (*ctmp)->prev;
	c = (*ctmp)->next;
	a = b ? b->prev : NULL;
	if(a)
	  a->next = p;

	if(b){
	    b->prev = p;
	    b->next = c;
	}

	if(c)
	  c->prev = b;

	p->prev = a;
	p->next = b;
    }

    role_type_print(title, sizeof(title), _("HELP FOR %sRULE CONFIGURATION"), rflags);
    role_type_print(add, sizeof(add), _("Use Add to add a %sRule"), rflags);

    (*ctmp)->value      = cpystr(add);
    (*ctmp)->keymenu    = &role_conf_km;
    (*ctmp)->help        = (rflags & ROLE_DO_INCOLS) ? h_rules_incols :
			    (rflags & ROLE_DO_OTHER)  ? h_rules_other :
			     (rflags & ROLE_DO_FILTER) ? h_rules_filter :
			      (rflags & ROLE_DO_SCORES) ? h_rules_score :
			       (rflags & ROLE_DO_ROLES)  ? h_rules_roles :
			        (rflags & ROLE_DO_SRCH)   ? h_rules_srch :
			        NO_HELP;
    (*ctmp)->help_title = title;
    (*ctmp)->tool       = role_config_tool;
    (*ctmp)->flags     |= CF_STARTITEM;
    (*ctmp)->valoffset  = 4;
}


int
role_config_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int       first_one = 0, rv = 0;
    char      exitpmt[80];
    PAT_S    *pat;

    if(!(pat = first_pattern(role_global_pstate)) ||
       (pat->inherit && !next_pattern(role_global_pstate)))
      first_one++;

    switch(cmd){
      case MC_DELETE :
	if(first_one)
	  q_status_message(SM_ORDER|SM_DING, 0, 3,
			   _("Nothing to Delete, use Add"));
	else
	  rv = role_config_del(ps, cl, role_global_flags);

	break;

      case MC_ADD :
	rv = role_config_add(ps, cl, role_global_flags);
	break;

      case MC_EDIT :
	if(first_one)
	  rv = role_config_add(ps, cl, role_global_flags);
	else
	  rv = role_config_edit(ps, cl, role_global_flags);

	break;

      case MC_SHUFFLE :
	if(first_one)
	  q_status_message(SM_ORDER|SM_DING, 0, 3,
			   _("Nothing to Shuffle, use Add"));
	else
	  rv = role_config_shuffle(ps, cl);

	break;

      case MC_EXIT :
	role_type_print(exitpmt, sizeof(exitpmt), "%sRule Setup", role_global_flags);
	rv = screen_exit_cmd(flags, exitpmt);
	break;

      case MC_ADDFILE :
	rv = role_config_addfile(ps, cl, role_global_flags);
	break;

      case MC_DELFILE :
	rv = role_config_delfile(ps, cl, role_global_flags);
	break;

      case MC_COPY :
	if(first_one)
	  q_status_message(SM_ORDER|SM_DING, 0, 3,
			   _("Nothing to Replicate, use Add"));
	else
	  rv = role_config_replicate(ps, cl, role_global_flags);

	break;

      default:
	rv = -1;
	break;
    }

    return(rv);
}


/*
 * Add a new role.
 *
 * Returns  1 -- There were changes
 *          0 -- No changes
 */
int
role_config_add(struct pine *ps, CONF_S **cl, long int rflags)
{
    int         rv = 0, first_pat = 0;
    PAT_S      *new_pat = NULL, *cur_pat;
    PAT_LINE_S *new_patline, *cur_patline;
    PAT_STATE   pstate;
    char        title[80];

    if((*cl)->d.r.patline &&
       (*cl)->d.r.patline->readonly
       && (*cl)->d.r.patline->type == File){
	q_status_message(SM_ORDER, 0, 3, _("Can't add rule to ReadOnly file"));
	return(rv);
    }

    role_type_print(title, sizeof(title), "ADD A %sRULE", rflags);

    if(role_config_edit_screen(ps, NULL, title, rflags,
			       &new_pat) == 1 && new_pat){
	if(ps->never_allow_changing_from &&
	   new_pat->action &&
	   new_pat->action->from)
	  q_status_message(SM_ORDER|SM_DING, 0, 3,
      _("Site policy doesn't allow changing From address so From is ignored"));

	if(rflags & ROLE_DO_ROLES &&
	   new_pat->patgrp &&
	   new_pat->patgrp->nick &&
	   nonempty_patterns(ROLE_DO_ROLES, &pstate)){
	    PAT_S *pat;
	    
	    for(pat = first_pattern(&pstate);
		pat;
		pat = next_pattern(&pstate)){
		if(pat->patgrp && pat->patgrp->nick &&
		   !strucmp(pat->patgrp->nick, new_pat->patgrp->nick)){
		    q_status_message(SM_ORDER|SM_DING, 3, 7, _("Warning: The nickname of the new role is already in use."));
		    break;
		}
	    }
	}
	
	rv = 1;
	cur_pat = (*cl)->d.r.pat;
	if(!cur_pat)
	  first_pat++;

	set_pathandle(rflags);
	cur_patline = first_pat ? (*cur_pat_h)->patlinehead : cur_pat->patline;

	/* need a new pat_line */
	if(first_pat || (cur_patline && cur_patline->type == Literal)){
	    new_patline = (PAT_LINE_S *)fs_get(sizeof(*new_patline));
	    memset((void *)new_patline, 0, sizeof(*new_patline));
	    new_patline->type = Literal;
	    (*cur_pat_h)->dirtypinerc = 1;
	}

	if(cur_patline){
	    if(first_pat || cur_patline->type == Literal){
		new_patline->prev = cur_patline;
		new_patline->next = cur_patline->next;
		if(cur_patline->next)
		  cur_patline->next->prev = new_patline;

		cur_patline->next = new_patline;

		/* tie new patline and new pat together */
		new_pat->patline   = new_patline;
		new_patline->first = new_patline->last = new_pat;
	    }
	    else if(cur_patline->type == File){ /* don't need a new pat_line */
		/* tie together */
		new_pat->patline = cur_patline;
		cur_patline->dirty = 1;

		/* Splice new_pat after cur_pat */
		new_pat->prev = cur_pat;
		new_pat->next = cur_pat->next;
		if(cur_pat->next)
		  cur_pat->next->prev = new_pat;
		else
		  cur_patline->last = new_pat;

		cur_pat->next = new_pat;
	    }
	}
	else{
	    /* tie new first patline and pat together */
	    new_pat->patline   = new_patline;
	    new_patline->first = new_patline->last = new_pat;

	    /* set head of list */
	    (*cur_pat_h)->patlinehead = new_patline;
	}

	/*
	 * If this is the first role, we replace the "Use Add" fake role
	 * with this real one.
	 */
	if(first_pat){
	    /* Adjust conf_scroll_screen variables */
	    (*cl)->d.r.pat = new_pat;
	    (*cl)->d.r.patline = new_pat->patline;
	    if((*cl)->value)
	      fs_give((void **)&(*cl)->value);

	    (*cl)->value = cpystr((new_pat && new_pat->patgrp &&
				   new_pat->patgrp->nick)
					 ? new_pat->patgrp->nick : "?");
	}
	/* Else we are inserting a new role after the cur role */
	else
	  add_role_to_display(cl, new_pat->patline, new_pat, 0, NULL,
			      1, rflags);
    }

    return(rv);
}


/*
 * Replicate a role.
 *
 * Returns  1 -- There were changes
 *          0 -- No changes
 */
int
role_config_replicate(struct pine *ps, CONF_S **cl, long int rflags)
{
    int         rv = 0, first_pat = 0;
    PAT_S      *new_pat = NULL, *cur_pat, *defpat = NULL;
    PAT_LINE_S *new_patline, *cur_patline;
    PAT_STATE   pstate;
    char        title[80];

    if((*cl)->d.r.patline &&
       (*cl)->d.r.patline->readonly
       && (*cl)->d.r.patline->type == File){
	q_status_message(SM_ORDER, 0, 3, _("Can't add rule to ReadOnly file"));
	return(rv);
    }

    if((*cl)->d.r.pat && (defpat = copy_pat((*cl)->d.r.pat))){
	/* change nickname */
	if(defpat->patgrp && defpat->patgrp->nick){
#define CLONEWORD " Copy"
	    char *oldnick = defpat->patgrp->nick;
	    size_t len;

	    len = strlen(oldnick)+strlen(CLONEWORD);
	    defpat->patgrp->nick = (char *)fs_get((len+1) * sizeof(char));
	    strncpy(defpat->patgrp->nick, oldnick, len);
	    defpat->patgrp->nick[len] = '\0';
	    strncat(defpat->patgrp->nick, CLONEWORD,
		    len+1-1-strlen(defpat->patgrp->nick));
	    fs_give((void **)&oldnick);
	    if(defpat->action){
		if(defpat->action->nick)
		  fs_give((void **)&defpat->action->nick);
		
		defpat->action->nick = cpystr(defpat->patgrp->nick);
	    }
	}

	/* set this so that even if we don't edit at all, we'll be asked */
	rflags |= ROLE_CHANGES;

	role_type_print(title, sizeof(title), "CHANGE THIS %sRULE", rflags);

	if(role_config_edit_screen(ps, defpat, title, rflags,
				   &new_pat) == 1 && new_pat){

	if(ps->never_allow_changing_from &&
	   new_pat->action &&
	   new_pat->action->from)
	  q_status_message(SM_ORDER|SM_DING, 0, 3,
      _("Site policy doesn't allow changing From address so From is ignored"));

	if(rflags & ROLE_DO_ROLES &&
	   new_pat->patgrp &&
	   new_pat->patgrp->nick &&
	   nonempty_patterns(ROLE_DO_ROLES, &pstate)){
	    PAT_S *pat;
	    
	    for(pat = first_pattern(&pstate);
		pat;
		pat = next_pattern(&pstate)){
		if(pat->patgrp && pat->patgrp->nick &&
		   !strucmp(pat->patgrp->nick, new_pat->patgrp->nick)){
		    q_status_message(SM_ORDER|SM_DING, 3, 7, _("Warning: The nickname of the new role is already in use."));
		    break;
		}
	    }
	}
	
	rv = 1;
	cur_pat = (*cl)->d.r.pat;
	if(!cur_pat)
	  first_pat++;

	set_pathandle(rflags);
	cur_patline = first_pat ? (*cur_pat_h)->patlinehead : cur_pat->patline;

	/* need a new pat_line */
	if(first_pat || (cur_patline && cur_patline->type == Literal)){
	    new_patline = (PAT_LINE_S *)fs_get(sizeof(*new_patline));
	    memset((void *)new_patline, 0, sizeof(*new_patline));
	    new_patline->type = Literal;
	    (*cur_pat_h)->dirtypinerc = 1;
	}

	if(cur_patline){
	    if(first_pat || cur_patline->type == Literal){
		new_patline->prev = cur_patline;
		new_patline->next = cur_patline->next;
		if(cur_patline->next)
		  cur_patline->next->prev = new_patline;

		cur_patline->next = new_patline;

		/* tie new patline and new pat together */
		new_pat->patline   = new_patline;
		new_patline->first = new_patline->last = new_pat;
	    }
	    else if(cur_patline->type == File){ /* don't need a new pat_line */
		/* tie together */
		new_pat->patline = cur_patline;
		cur_patline->dirty = 1;

		/* Splice new_pat after cur_pat */
		new_pat->prev = cur_pat;
		new_pat->next = cur_pat->next;
		if(cur_pat->next)
		  cur_pat->next->prev = new_pat;
		else
		  cur_patline->last = new_pat;

		cur_pat->next = new_pat;
	    }
	}
	else{
	    /* tie new first patline and pat together */
	    new_pat->patline   = new_patline;
	    new_patline->first = new_patline->last = new_pat;

	    /* set head of list */
	    (*cur_pat_h)->patlinehead = new_patline;
	}

	/*
	 * If this is the first role, we replace the "Use Add" fake role
	 * with this real one.
	 */
	if(first_pat){
	    /* Adjust conf_scroll_screen variables */
	    (*cl)->d.r.pat = new_pat;
	    (*cl)->d.r.patline = new_pat->patline;
	    if((*cl)->value)
	      fs_give((void **)&(*cl)->value);

	    (*cl)->value = cpystr((new_pat && new_pat->patgrp &&
				   new_pat->patgrp->nick)
					 ? new_pat->patgrp->nick : "?");
	}
	/* Else we are inserting a new role after the cur role */
	else
	  add_role_to_display(cl, new_pat->patline, new_pat, 0, NULL,
			      1, rflags);
	}
    }

    if(defpat)
      free_pat(&defpat);

    return(rv);
}


/* 
 * Change the current role.
 *
 * Returns  1 -- There were changes
 *          0 -- No changes
 */
int
role_config_edit(struct pine *ps, CONF_S **cl, long int rflags)
{
    int         rv = 0;
    PAT_S      *new_pat = NULL, *cur_pat;
    char        title[80];

    if((*cl)->d.r.patline->readonly){
	q_status_message(SM_ORDER, 0, 3, _("Can't change ReadOnly rule"));
	return(rv);
    }

    cur_pat = (*cl)->d.r.pat;

    role_type_print(title, sizeof(title), "CHANGE THIS %sRULE", rflags);

    if(role_config_edit_screen(ps, cur_pat, title,
			       rflags, &new_pat) == 1 && new_pat){

	if(ps->never_allow_changing_from &&
	   new_pat->action &&
	   new_pat->action->from)
	  q_status_message(SM_ORDER|SM_DING, 0, 3,
      _("Site policy doesn't allow changing From address so From is ignored"));

	if(rflags & ROLE_DO_ROLES && new_pat->patgrp && new_pat->patgrp->nick){
	    PAT_S *pat;
	    
	    for(pat = first_pattern(role_global_pstate);
		pat;
		pat = next_pattern(role_global_pstate)){
		if(pat->patgrp && pat->patgrp->nick && pat != cur_pat &&
		   !strucmp(pat->patgrp->nick, new_pat->patgrp->nick)){
		    q_status_message(SM_ORDER|SM_DING, 3, 7, _("Warning: The nickname of this role is also used for another role."));
		    break;
		}
	    }
	}
	
	rv = 1;

	/*
	 * Splice in new_pat in place of cur_pat
	 */

	if(cur_pat->prev)
	  cur_pat->prev->next = new_pat;

	if(cur_pat->next)
	  cur_pat->next->prev = new_pat;

	new_pat->prev = cur_pat->prev;
	new_pat->next = cur_pat->next;

	/* tie together patline and pat (new_pat gets patline in editor) */
	if(new_pat->patline->first == cur_pat)
	  new_pat->patline->first = new_pat;

	if(new_pat->patline->last == cur_pat)
	  new_pat->patline->last = new_pat;
	
	if(new_pat->patline->type == Literal){
	    set_pathandle(rflags);
	    if(*cur_pat_h)
	      (*cur_pat_h)->dirtypinerc = 1;
	}
	else
	  new_pat->patline->dirty = 1;

	cur_pat->next = NULL;
	free_pat(&cur_pat);

	/* Adjust conf_scroll_screen variables */
	(*cl)->d.r.pat = new_pat;
	(*cl)->d.r.patline = new_pat->patline;
	if((*cl)->value)
	  fs_give((void **)&(*cl)->value);

	(*cl)->value = cpystr((new_pat->patgrp && new_pat->patgrp->nick)
				? new_pat->patgrp->nick : "?");
    }

    return(rv);
}


/*
 * Delete a role.
 *
 * Returns  1 -- There were changes
 *          0 -- No changes
 */
int
role_config_del(struct pine *ps, CONF_S **cl, long int rflags)
{
    int  rv = 0;
    char msg[80];
    char prompt[100];

    if((*cl)->d.r.patline->readonly){
	q_status_message(SM_ORDER, 0, 3, _("Can't delete ReadOnly rule"));
	return(rv);
    }

    role_type_print(msg, sizeof(msg), _("Really delete %srule"), rflags);
    snprintf(prompt, sizeof(prompt), "%s \"%s\" ", msg, (*cl)->value);
    prompt[sizeof(prompt)-1] = '\0';

    ps->mangled_footer = 1;
    if(want_to(prompt,'n','n',h_config_role_del, WT_FLUSH_IN) == 'y'){
	rv = ps->mangled_body = 1;
	delete_a_role(cl, rflags);
    }
    else
      q_status_message(SM_ORDER, 0, 3, _("Rule not deleted"));
    
    return(rv);
}


void
delete_a_role(CONF_S **cl, long int rflags)
{
    PAT_S      *cur_pat;
    CONF_S     *cp, *cq;
    PAT_LINE_S *cur_patline;
    int         inherit = 0;

    cur_pat     = (*cl)->d.r.pat;
    cur_patline = (*cl)->d.r.patline;

    if(cur_patline->type == Literal){	/* delete patline */
	set_pathandle(rflags);
	if(cur_patline->prev)
	  cur_patline->prev->next = cur_patline->next;
	else{
	    if(*cur_pat_h)		/* this has to be true */
	      (*cur_pat_h)->patlinehead = cur_patline->next;
	}

	if(cur_patline->next)
	  cur_patline->next->prev = cur_patline->prev;
	
	if(*cur_pat_h)		/* this has to be true */
	  (*cur_pat_h)->dirtypinerc = 1;

	cur_patline->next = NULL;
	free_patline(&cur_patline);
    }
    else if(cur_patline->type == File){	/* or delete pat */
	if(cur_pat->prev)
	  cur_pat->prev->next = cur_pat->next;
	else
	  cur_patline->first = cur_pat->next;

	if(cur_pat->next)
	  cur_pat->next->prev = cur_pat->prev;
	else
	  cur_patline->last = cur_pat->prev;

	cur_patline->dirty = 1;

	cur_pat->next = NULL;
	free_pat(&cur_pat);
    }

    /* delete the conf line */

    /* deleting last real rule */
    if(!first_pattern(role_global_pstate) ||
       ((inherit=first_pattern(role_global_pstate)->inherit) &&
	 !next_pattern(role_global_pstate))){

	cq = *cl;

	/*
	 * Find the start and prepend the fake first role.
	 */
	while(*cl && (*cl)->prev)
	  *cl = (*cl)->prev;

	add_fake_first_role(cl, inherit ? 0 : 1, rflags);
	snip_confline(&cq);
	opt_screen->top_line = (*cl);
	opt_screen->current = (*cl);
    }
    else{
	/* find next selectable line */
	for(cp = (*cl)->next;
	    cp && (cp->flags & CF_NOSELECT);
	    cp = cp->next)
	  ;
	
	if(!cp){	/* no next selectable, find previous selectable */
	    if(*cl == opt_screen->top_line)
	      opt_screen->top_line = (*cl)->prev;

	    for(cp = (*cl)->prev;
		cp && (cp->flags & CF_NOSELECT);
		cp = cp->prev)
	      ;
	}
	else if(*cl == opt_screen->top_line)
	  opt_screen->top_line = (*cl)->next;

	cq = *cl;
	*cl = cp;
	snip_confline(&cq);
    }
}


/*
 * Shuffle the current role up or down.
 *
 * Returns  1 -- There were changes
 *          0 -- No changes
 */
int
role_config_shuffle(struct pine *ps, CONF_S **cl)
{
    int      rv = 0, deefault, i;
    int      readonlyabove = 0, readonlybelow = 0;
    ESCKEY_S opts[5];
    HelpType help;
    char     tmp[200];
    CONF_S  *a, *b;
    PAT_TYPE curtype, prevtype, nexttype;

    if(!((*cl)->prev || (*cl)->next)){
	q_status_message(SM_ORDER, 0, 3,
	   _("Shuffle only makes sense when there is more than one rule defined"));
	return(rv);
    }

    /* Move it up or down? */
    i = 0;
    opts[i].ch      = 'u';
    opts[i].rval    = 'u';
    opts[i].name    = "U";
    opts[i++].label = N_("Up");

    opts[i].ch      = 'd';
    opts[i].rval    = 'd';
    opts[i].name    = "D";
    opts[i++].label = N_("Down");

    opts[i].ch      = 'b';
    opts[i].rval    = 'b';
    opts[i].name    = "B";
    opts[i++].label = N_("Before File");

    opts[i].ch      = 'a';
    opts[i].rval    = 'a';
    opts[i].name    = "A";
    opts[i++].label = N_("After File");

    opts[i].ch = -1;
    deefault = 'u';

    curtype = ((*cl)->d.r.patline) ? (*cl)->d.r.patline->type : TypeNotSet;

    prevtype = ((*cl)->prev && (*cl)->prev->d.r.patline)
		? (*cl)->prev->d.r.patline->type : TypeNotSet;
    if(curtype == File && prevtype == File && (*cl)->prev->d.r.pat == NULL)
      prevtype = TypeNotSet;

    nexttype = ((*cl)->next && (*cl)->next->d.r.patline)
		? (*cl)->next->d.r.patline->type : TypeNotSet;
    if(curtype == File && nexttype == File && (*cl)->next->d.r.pat == NULL)
      nexttype = TypeNotSet;


    if(curtype == Literal){
	if(prevtype == TypeNotSet ||
	   prevtype == Inherit){	/* no up, at top	*/
	    opts[0].ch = -2;
	    opts[2].ch = -2;
	    deefault = 'd';
	}
	else if(prevtype == Literal){	/* regular up		*/
	    opts[2].ch = -2;
	}
	else if(prevtype == File){	/* file above us	*/
	    if((*cl)->prev->d.r.patline->readonly)
	      readonlyabove++;
	}

	if(nexttype == TypeNotSet){	/* no down, at bottom	*/
	    opts[1].ch = -2;
	    opts[3].ch = -2;
	}
	else if(nexttype == Literal){	/* regular down		*/
	    opts[3].ch = -2;
	}
	else if(nexttype == File){	/* file below us	*/
	    if((*cl)->next->d.r.patline->readonly)
	      readonlybelow++;
	}
    }
    else if(curtype == File){
	if((*cl)->d.r.patline && (*cl)->d.r.patline->readonly){
	    q_status_message(SM_ORDER, 0, 3, _("Can't change ReadOnly file"));
	    return(0);
	}

	opts[2].ch = -2;
	opts[3].ch = -2;
    }
    else{
	q_status_message(SM_ORDER, 0, 3,
	"Programming Error: unknown line type in role_shuffle");
	return(rv);
    }

    snprintf(tmp, sizeof(tmp), "Shuffle \"%s\" %s%s%s%s%s%s%s ? ",
	    (*cl)->value,
	    (opts[0].ch != -2) ? N_("UP") : "",
	    (opts[0].ch != -2  && opts[1].ch != -2) ? " or " : "",
	    (opts[1].ch != -2) ? N_("DOWN") : "",
	    ((opts[0].ch != -2 ||
	      opts[1].ch != -2) && opts[2].ch != -2) ? " or " : "",
	    (opts[2].ch != -2) ? N_("BEFORE") : "",
	    ((opts[0].ch != -2 ||
	      opts[1].ch != -2 ||
	      opts[2].ch != -2) && opts[3].ch != -2) ? " or " : "",
	    (opts[3].ch != -2) ? N_("AFTER") : "");
    tmp[sizeof(tmp)-1] = '\0';

    help = (opts[0].ch == -2) ? h_role_shuf_down
			      : (opts[1].ch == -2) ? h_role_shuf_up
						   : h_role_shuf;

    rv = radio_buttons(tmp, -FOOTER_ROWS(ps), opts, deefault, 'x',
		       help, RB_NORM);

    if(rv == 'x'){
	cmd_cancelled("Shuffle");
	return(0);
    }

    if((readonlyabove && rv == 'u' && curtype != prevtype) ||
       (readonlybelow && rv == 'd' && curtype != nexttype)){
	q_status_message(SM_ORDER, 0, 3, _("Can't shuffle into ReadOnly file"));
	return(0);
    }

    if(rv == 'u' && curtype == Literal && prevtype == Literal){
	rv = 1;
	a = (*cl)->prev;
	b = (*cl);
	if(a == opt_screen->top_line)
	  opt_screen->top_line = b;

	swap_literal_roles(a, b);
	ps->mangled_body = 1;
    }
    else if(rv == 'd' && curtype == Literal && nexttype == Literal){
	rv = 1;
	a = (*cl);
	b = (*cl)->next;
	if(a == opt_screen->top_line)
	  opt_screen->top_line = b;

	swap_literal_roles(a, b);
	ps->mangled_body = 1;
    }
    else if(rv == 'u' && curtype == File && prevtype == File){
	rv = 1;
	a = (*cl)->prev;
	b = (*cl);
	if(a == opt_screen->top_line)
	  opt_screen->top_line = b;

	swap_file_roles(a, b);
	ps->mangled_body = 1;
    }
    else if(rv == 'u' && curtype == File){
	rv = 1;
	move_role_outof_file(cl, 1);
	ps->mangled_body = 1;
    }
    else if(rv == 'd' && curtype == File && nexttype == File){
	rv = 1;
	a = (*cl);
	b = (*cl)->next;
	if(a == opt_screen->top_line)
	  opt_screen->top_line = b;

	swap_file_roles(a, b);
	ps->mangled_body = 1;
    }
    else if(rv == 'd' && curtype == File){
	rv = 1;
	if(*cl == opt_screen->top_line)
	  opt_screen->top_line = (*cl)->next;

	move_role_outof_file(cl, 0);
	ps->mangled_body = 1;
    }
    else if(rv == 'u' && curtype == Literal && prevtype == File){
	rv = 1;
	move_role_into_file(cl, 1);
	ps->mangled_body = 1;
    }
    else if(rv == 'd' && curtype == Literal && nexttype == File){
	rv = 1;
	if(*cl == opt_screen->top_line)
	  opt_screen->top_line = (*cl)->next;

	move_role_into_file(cl, 0);
	ps->mangled_body = 1;
    }
    else if(rv == 'b'){
	rv = 1;
	move_role_around_file(cl, 1);
	ps->mangled_body = 1;
    }
    else if(rv == 'a'){
	rv = 1;
	if(*cl == opt_screen->top_line)
	  opt_screen->top_line = (*cl)->next;

	move_role_around_file(cl, 0);
	ps->mangled_body = 1;
    }

    return(rv);
}


int
role_config_addfile(struct pine *ps, CONF_S **cl, long int rflags)
{
    char        filename[MAXPATH+1], full_filename[MAXPATH+1];
    char        dir2[MAXPATH+1], pdir[MAXPATH+1];
    char       *lc, *newfile = NULL;
    PAT_LINE_S *file_patline;
    int         rv = 0, len;
    int         r = 1, flags;
    HelpType    help = NO_HELP;
    PAT_TYPE    curtype;
    CONF_S     *first_line = NULL, *add_line, *save_current;
    struct variable *vars = ps->vars;

    if(ps->restricted){
	q_status_message(SM_ORDER, 0, 3, "Alpine demo can't read files");
	return(rv);
    }

    curtype = ((*cl)->d.r.patline && (*cl)->d.r.patline)
	        ? (*cl)->d.r.patline->type : TypeNotSet;

    if(curtype == File){
	q_status_message(SM_ORDER, 0, 3, _("Current rule is already part of a file. Move outside any files first."));
	return(rv);
    }

    /*
     * Parse_pattern_file uses signature_path to figure out where to look
     * for the file. In signature_path we read signature files relative
     * to the pinerc dir, so if user selects one that is in there we'll
     * use relative instead of absolute, so it looks nicer.
     */
    pdir[0] = '\0';
    if(VAR_OPER_DIR){
	strncpy(pdir, VAR_OPER_DIR, sizeof(pdir)-1);
	pdir[sizeof(pdir)-1] = '\0';
	len = strlen(pdir) + 1;
    }
    else if((lc = last_cmpnt(ps->pinerc)) != NULL){
	strncpy(pdir, ps->pinerc, MIN(sizeof(pdir)-1,lc-ps->pinerc));
	pdir[MIN(sizeof(pdir)-1, lc-ps->pinerc)] = '\0';
	len = strlen(pdir);
    }

    strncpy(dir2, pdir, sizeof(dir2)-1);
    dir2[sizeof(dir2)-1] = '\0';
    filename[0] = '\0';
    full_filename[0] = '\0';
    ps->mangled_footer = 1;

    while(1){
	flags = OE_APPEND_CURRENT;
	r = optionally_enter(filename, -FOOTER_ROWS(ps), 0, sizeof(filename),
			     "Name of file to be added to rules: ",
			     NULL, help, &flags);
	
	if(r == 3){
	    help = (help == NO_HELP) ? h_config_role_addfile : NO_HELP;
	    continue;
	}
	else if(r == 10 || r == 11){    /* Browser or File Completion */
	    continue;
	}
	else if(r == 1 || (r == 0 && filename[0] == '\0')){
	    cmd_cancelled("IncludeFile");
	    return(rv);
	}
	else if(r == 4){
	    continue;
	}
	else if(r != 0){
	    Writechar(BELL, 0);
	    continue;
	}

	removing_leading_and_trailing_white_space(filename);
	if(is_absolute_path(filename))
	  newfile = cpystr(filename);
	else{
	    build_path(full_filename, dir2, filename, sizeof(full_filename));
	    removing_leading_and_trailing_white_space(full_filename);
	    if(!strncmp(full_filename, pdir, strlen(pdir)))
	      newfile = cpystr(full_filename + len); 
	    else
	      newfile = cpystr(full_filename);
	}
	
	if(newfile && *newfile)
	  break;
    }

    if(!newfile)
      return(rv);
    
    set_pathandle(rflags);

    if((file_patline = parse_pat_file(newfile)) != NULL){
	/*
	 * Insert the file after the current line.
	 */
	PAT_LINE_S *cur_patline;
	int         first_pat;

	rv = ps->mangled_screen = 1;
	first_pat = !(*cl)->d.r.pat;
	cur_patline = (*cl)->d.r.patline ? (*cl)->d.r.patline :
		       (*cur_pat_h) ? (*cur_pat_h)->patlinehead : NULL;

	if(*cur_pat_h)
	  (*cur_pat_h)->dirtypinerc = 1;

	file_patline->dirty = 1;

	if(cur_patline){
	    file_patline->prev = cur_patline;
	    file_patline->next = cur_patline->next;
	    if(cur_patline->next)
	      cur_patline->next->prev = file_patline;

	    cur_patline->next = file_patline;
	}
	else{
	    /* set head of list */
	    if(*cur_pat_h)
	      (*cur_pat_h)->patlinehead = file_patline;
	}

	if(first_pat){
	    if(file_patline->first){
		/* get rid of Fake Add line */
		add_line = *cl;
		opt_screen->top_line = NULL;
		add_patline_to_display(ps, cl, 0, &first_line,
				       &opt_screen->top_line, file_patline,
				       rflags);
		opt_screen->current = first_line;
		snip_confline(&add_line);
	    }
	    else{
		/* we're _appending_ to the Fake Add line */
		save_current = opt_screen->current;
		add_patline_to_display(ps, cl, 0, NULL, NULL, file_patline,
				       rflags);
		opt_screen->current = save_current;
	    }
	}
	else{
	    opt_screen->top_line = NULL;
	    save_current = opt_screen->current;
	    add_patline_to_display(ps, cl, 0, &first_line,
				   &opt_screen->top_line, file_patline,
				   rflags);
	    if(first_line)
	      opt_screen->current = first_line;
	    else
	      opt_screen->current = save_current;
	}
    }

    if(newfile)
      fs_give((void **)&newfile);
    
    return(rv);
}


int
role_config_delfile(struct pine *ps, CONF_S **cl, long int rflags)
{
    int         rv = 0;
    PAT_LINE_S *cur_patline;
    char        prompt[100];

    if(!(cur_patline = (*cl)->d.r.patline)){
	q_status_message(SM_ORDER, 0, 3,
			 "Unknown problem in role_config_delfile");
	return(rv);
    }

    if(cur_patline->type != File){
	q_status_message(SM_ORDER, 0, 3, _("Current rule is not part of a file. Use Delete to remove current rule"));
	return(rv);
    }

    snprintf(prompt, sizeof(prompt), _("Really remove rule file \"%s\" from rules config "),
	    cur_patline->filename);
    prompt[sizeof(prompt)-1] = '\0';

    ps->mangled_footer = 1;
    if(want_to(prompt,'n','n',h_config_role_delfile, WT_FLUSH_IN) == 'y'){
	CONF_S *ctmp, *cp;

	set_pathandle(rflags);
	rv = ps->mangled_screen = 1;
	if(*cur_pat_h)
	  (*cur_pat_h)->dirtypinerc = 1;

	if(cur_patline->prev)
	  cur_patline->prev->next = cur_patline->next;
	else{
	    if(*cur_pat_h)
	      (*cur_pat_h)->patlinehead = cur_patline->next;
	}

	if(cur_patline->next)
	  cur_patline->next->prev = cur_patline->prev;
	
	/* delete the conf lines */

	/* find the first one associated with this file */
	for(ctmp = *cl;
	    ctmp && ctmp->prev && ctmp->prev->d.r.patline == cur_patline;
	    ctmp = ctmp->prev)
	  ;
	
	if(ctmp->prev)	/* this file wasn't the first thing in config */
	  *cl = ctmp->prev;
	else{		/* this file was first in config */
	    for(cp = ctmp; cp && cp->next; cp = cp->next)
	      ;

	    if(cp->d.r.patline == cur_patline)
	      *cl = NULL;
	    else
	      *cl = cp;
	}
	
	/* delete lines from the file */
	while(ctmp && ctmp->d.r.patline == cur_patline){
	    cp = ctmp;
	    ctmp = ctmp->next;
	    snip_confline(&cp);
	}

	/* deleting last real rule */
	if(!first_pattern(role_global_pstate)){
	    /*
	     * Find the start and prepend the fake first role
	     * in there.
	     */
	    while(*cl && (*cl)->prev)
	      *cl = (*cl)->prev;

	    add_fake_first_role(cl, 1, rflags);
	}
	else if(first_pattern(role_global_pstate)->inherit &&
	       !next_pattern(role_global_pstate)){
	    while(*cl && (*cl)->prev)
	      *cl = (*cl)->prev;
	    
	    /* append fake first after inherit */
	    add_fake_first_role(cl, 0, rflags);
	}

	opt_screen->top_line = first_confline(*cl);
	opt_screen->current  = first_sel_confline(opt_screen->top_line);

	cur_patline->next = NULL;
	free_patline(&cur_patline);
    }
    else
      q_status_message(SM_ORDER, 0, 3, _("Rule file not removed"));
    
    return(rv);
}


/*
 * Swap from a, b to b, a.
 */
void
swap_literal_roles(CONF_S *a, CONF_S *b)
{
    PAT_LINE_S *patline_a, *patline_b;

    patline_a = a->d.r.patline;
    patline_b = b->d.r.patline;

    set_pathandle(role_global_flags);
    if(*cur_pat_h)
      (*cur_pat_h)->dirtypinerc = 1;

    /* first swap the patlines */
    if(patline_a->next == patline_b){
	patline_b->prev = patline_a->prev;
	if(patline_a->prev)
	  patline_a->prev->next = patline_b;

	patline_a->next = patline_b->next;
	if(patline_b->next)
	  patline_b->next->prev = patline_a;

	patline_b->next = patline_a;
	patline_a->prev = patline_b;
    }
    else{
	PAT_LINE_S *new_a_prev, *new_a_next;

	new_a_prev = patline_b->prev;
	new_a_next = patline_b->next;

	patline_b->prev = patline_a->prev;
	patline_b->next = patline_a->next;
	if(patline_b->prev)
	  patline_b->prev->next = patline_b;
	if(patline_b->next)
	  patline_b->next->prev = patline_b;

	patline_a->prev = new_a_prev;
	patline_a->next = new_a_next;
	if(patline_a->prev)
	  patline_a->prev->next = patline_a;
	if(patline_a->next)
	  patline_a->next->prev = patline_a;
    }

    /*
     * If patline_b is now the first one in the list, we need to fix the
     * head of the list to point to this new role.
     */
    if(patline_b->prev == NULL && *cur_pat_h)
      (*cur_pat_h)->patlinehead = patline_b;


    /* and then swap the conf lines */

    b->prev = a->prev;
    if(a->prev)
      a->prev->next = b;
    
    a->next = b->next;
    if(b->next)
      b->next->prev = a;
    
    b->next = a;
    a->prev = b;
}


/*
 * Swap from a, b to b, a.
 */
void
swap_file_roles(CONF_S *a, CONF_S *b)
{
    PAT_S      *pat_a, *pat_b;
    PAT_LINE_S *patline;

    pat_a = a->d.r.pat;
    pat_b = b->d.r.pat;
    patline = pat_a->patline;

    patline->dirty = 1;

    /* first swap the pats */
    if(pat_a->next == pat_b){
	pat_b->prev = pat_a->prev;
	if(pat_a->prev)
	  pat_a->prev->next = pat_b;
	
	pat_a->next = pat_b->next;
	if(pat_b->next)
	  pat_b->next->prev = pat_a;
	
	pat_b->next = pat_a;
	pat_a->prev = pat_b;
    }
    else{
	PAT_S *new_a_prev, *new_a_next;

	new_a_prev = pat_b->prev;
	new_a_next = pat_b->next;

	pat_b->prev = pat_a->prev;
	pat_b->next = pat_a->next;
	if(pat_b->prev)
	  pat_b->prev->next = pat_b;
	if(pat_b->next)
	  pat_b->next->prev = pat_b;

	pat_a->prev = new_a_prev;
	pat_a->next = new_a_next;
	if(pat_a->prev)
	  pat_a->prev->next = pat_a;
	if(pat_a->next)
	  pat_a->next->prev = pat_a;
    }

    /*
     * Fix the first and last pointers.
     */
    if(patline->first == pat_a)
      patline->first = pat_b;
    if(patline->last == pat_b)
      patline->last = pat_a;

    /* and then swap the conf lines */

    b->prev = a->prev;
    if(a->prev)
      a->prev->next = b;
    
    a->next = b->next;
    if(b->next)
      b->next->prev = a;
    
    b->next = a;
    a->prev = b;
}


/*
 */
void
move_role_into_file(CONF_S **cl, int up)
{
    PAT_LINE_S *cur_patline, *file_patline;
    PAT_S      *pat;
    CONF_S     *a, *b;

    cur_patline = (*cl)->d.r.patline;

    if(up){
	file_patline = (*cl)->prev->d.r.patline;
	a = (*cl)->prev;
	b = (*cl);
	b->d.r.patline = file_patline;
    }
    else{
	file_patline = (*cl)->next->d.r.patline;
	a = (*cl);
	b = (*cl)->next;
	a->d.r.patline = file_patline;
    }

    set_pathandle(role_global_flags);
    if(*cur_pat_h)
      (*cur_pat_h)->dirtypinerc = 1;

    file_patline->dirty = 1;

    pat = cur_patline->first;

    if(!up && *cur_pat_h && cur_patline == (*cur_pat_h)->patlinehead)
      (*cur_pat_h)->patlinehead = (*cur_pat_h)->patlinehead->next;

    if(file_patline->first){
	if(up){
	    file_patline->last->next = pat;
	    pat->prev = file_patline->last;
	    file_patline->last = pat;
	}
	else{
	    file_patline->first->prev = pat;
	    pat->next = file_patline->first;
	    file_patline->first = pat;
	}
    }
    else		/* will be only role in file */
      file_patline->first = file_patline->last = pat;

    pat->patline = file_patline;

    /* delete the now unused cur_patline */
    cur_patline->first = cur_patline->last = NULL;
    if(cur_patline->prev)
      cur_patline->prev->next = cur_patline->next;
    if(cur_patline->next)
      cur_patline->next->prev = cur_patline->prev;
    
    cur_patline->next = NULL;
    free_patline(&cur_patline);

    /* and then swap the conf lines */

    b->prev = a->prev;
    if(a->prev)
      a->prev->next = b;
    
    a->next = b->next;
    if(b->next)
      b->next->prev = a;
    
    b->next = a;
    a->prev = b;
}


/*
 */
void
move_role_outof_file(CONF_S **cl, int up)
{
    PAT_LINE_S *file_patline, *new_patline;
    PAT_S      *pat;
    CONF_S     *a, *b;

    new_patline = (PAT_LINE_S *)fs_get(sizeof(*new_patline));
    memset((void *)new_patline, 0, sizeof(*new_patline));
    new_patline->type = Literal;

    file_patline = (*cl)->d.r.patline;
    pat = (*cl)->d.r.pat;

    if(up){
	a = (*cl)->prev;
	b = (*cl);

	if(pat->prev)
	  pat->prev->next = pat->next;
	else
	  file_patline->first = pat->next;

	if(pat->next)
	  pat->next->prev = pat->prev;
	else
	  file_patline->last = pat->prev;

	if(file_patline->first)
	  file_patline->first->prev = NULL;

	if(file_patline->last)
	  file_patline->last->next = NULL;
	
	if(file_patline->prev)
	  file_patline->prev->next = new_patline;
	
	new_patline->prev = file_patline->prev;
	new_patline->next = file_patline;
	file_patline->prev = new_patline;
	b->d.r.patline = new_patline;
    }
    else{
	a = (*cl);
	b = (*cl)->next;

	if(pat->prev)
	  pat->prev->next = pat->next;
	else
	  file_patline->first = pat->next;

	if(pat->next)
	  pat->next->prev = pat->prev;
	else
	  file_patline->last = pat->prev;

	if(file_patline->first)
	  file_patline->first->prev = NULL;

	if(file_patline->last)
	  file_patline->last->next = NULL;

	if(file_patline->next)
	  file_patline->next->prev = new_patline;
	
	new_patline->next = file_patline->next;
	new_patline->prev = file_patline;
	file_patline->next = new_patline;
	a->d.r.patline = new_patline;
    }

    set_pathandle(role_global_flags);
    if(*cur_pat_h)
      (*cur_pat_h)->dirtypinerc = 1;

    file_patline->dirty = 1;

    new_patline->first = new_patline->last = pat;
    pat->patline = new_patline;
    pat->prev = pat->next = NULL;

    if(up && *cur_pat_h && file_patline == (*cur_pat_h)->patlinehead)
      (*cur_pat_h)->patlinehead = new_patline;

    /* and then swap the conf lines */

    b->prev = a->prev;
    if(a->prev)
      a->prev->next = b;
    
    a->next = b->next;
    if(b->next)
      b->next->prev = a;
    
    b->next = a;
    a->prev = b;
}


/*
 * This is a move of a literal role from before a file to after a file,
 * or vice versa.
 */
void
move_role_around_file(CONF_S **cl, int up)
{
    PAT_LINE_S *file_patline, *lit_patline;
    CONF_S     *cp;

    set_pathandle(role_global_flags);
    lit_patline = (*cl)->d.r.patline;
    if(up)
      file_patline = (*cl)->prev->d.r.patline;
    else{
	if(*cur_pat_h && lit_patline == (*cur_pat_h)->patlinehead)
	  (*cur_pat_h)->patlinehead = (*cur_pat_h)->patlinehead->next;

	file_patline = (*cl)->next->d.r.patline;
    }

    if(*cur_pat_h)
      (*cur_pat_h)->dirtypinerc = 1;

    /* remove the lit_patline from the list */
    if(lit_patline->prev)
      lit_patline->prev->next = lit_patline->next;
    if(lit_patline->next)
      lit_patline->next->prev = lit_patline->prev;

    /* and reinsert it on the other side of the file */
    if(up){
	if(*cur_pat_h && file_patline == (*cur_pat_h)->patlinehead)
	  (*cur_pat_h)->patlinehead = lit_patline;

	lit_patline->prev = file_patline->prev;
	lit_patline->next = file_patline;

	if(file_patline->prev)
	  file_patline->prev->next = lit_patline;
	
	file_patline->prev = lit_patline;
    }
    else{
	lit_patline->next = file_patline->next;
	lit_patline->prev = file_patline;

	if(file_patline->next)
	  file_patline->next->prev = lit_patline;
	
	file_patline->next = lit_patline;
    }

    /*
     * And then move the conf line around the file conf lines.
     */

    /* find it's new home */
    if(up)
      for(cp = (*cl);
	  cp && cp->prev && cp->prev->d.r.patline == file_patline;
	  cp = prev_confline(cp))
	;
    else
      for(cp = (*cl);
	  cp && cp->next && cp->next->d.r.patline == file_patline;
	  cp = next_confline(cp))
	;

    /* remove it from where it is */
    if((*cl)->prev)
      (*cl)->prev->next = (*cl)->next;
    if((*cl)->next)
      (*cl)->next->prev = (*cl)->prev;
    
    /* cp points to top or bottom of the file lines */
    if(up){
	(*cl)->prev = cp->prev;
	if(cp->prev)
	  cp->prev->next = (*cl);
	
	cp->prev = (*cl);
	(*cl)->next = cp;
    }
    else{
	(*cl)->next = cp->next;
	if(cp->next)
	  cp->next->prev = (*cl);
	
	cp->next = (*cl);
	(*cl)->prev = cp;
    }
}


#define SETUP_PAT_STATUS(ctmp,svar,val,htitle,hval)			\
   {char tmp[MAXPATH+1];						\
    int i, j, lv;							\
    NAMEVAL_S *f;							\
									\
    /* Blank line */							\
    new_confline(&ctmp);						\
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;				\
									\
    new_confline(&ctmp);						\
    ctmp->var       = &svar;						\
    ctmp->keymenu   = &config_radiobutton_keymenu;			\
    ctmp->help      = NO_HELP;						\
    ctmp->tool      = NULL;						\
    snprintf(tmp, sizeof(tmp), "%-s =", svar.name);			\
    tmp[sizeof(tmp)-1] = '\0';						\
    ctmp->varname   = cpystr(tmp);					\
    ctmp->varnamep  = ctmpb = ctmp;					\
    ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);			\
									\
    new_confline(&ctmp);						\
    ctmp->var       = NULL;						\
    ctmp->valoffset = rindent;						\
    ctmp->keymenu   = &config_radiobutton_keymenu;			\
    ctmp->help      = NO_HELP;						\
    ctmp->tool      = NULL;						\
    ctmp->varnamep  = ctmpb;						\
    ctmp->flags    |= CF_NOSELECT;					\
    ctmp->value     = cpystr("Set    Choose One");			\
									\
    new_confline(&ctmp);						\
    ctmp->var       = NULL;						\
    ctmp->valoffset = rindent;						\
    ctmp->keymenu   = &config_radiobutton_keymenu;			\
    ctmp->help      = NO_HELP;						\
    ctmp->tool      = radio_tool;					\
    ctmp->varnamep  = ctmpb;						\
    ctmp->flags    |= CF_NOSELECT;					\
    ctmp->value     = cpystr(set_choose);				\
									\
    /* find longest value's name */					\
    for(lv = 0, i = 0; (f = role_status_types(i)); i++)			\
      if(lv < (j = utf8_width(f->name)))				\
	lv = j;								\
    									\
    lv = MIN(lv, 100);							\
    									\
    for(i = 0; (f = role_status_types(i)); i++){			\
	new_confline(&ctmp);						\
	ctmp->help_title= htitle;					\
	ctmp->var       = &svar;					\
	ctmp->valoffset = rindent;						\
	ctmp->keymenu   = &config_radiobutton_keymenu;			\
	ctmp->help      = hval;						\
	ctmp->varmem    = i;						\
	ctmp->tool      = radio_tool;					\
	ctmp->varnamep  = ctmpb;					\
	utf8_snprintf(tmp, sizeof(tmp), "(%c)  %-*.*w", (((!(def && def->patgrp) ||	\
					 val == -1) &&			\
					f->value == PAT_STAT_EITHER) ||	\
				      (def && def->patgrp &&		\
				      f->value == val))			\
					 ? R_SELD : ' ',		\
		lv, lv, f->name);					\
	tmp[sizeof(tmp)-1] = '\0';					\
	ctmp->value     = cpystr(tmp);					\
    }									\
   }

#define SETUP_MSG_STATE(ctmp,svar,val,htitle,hval)			\
   {char tmp[MAXPATH+1];						\
    int i, j, lv;							\
    NAMEVAL_S *f;							\
									\
    /* Blank line */							\
    new_confline(&ctmp);						\
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;				\
									\
    new_confline(&ctmp);						\
    ctmp->var       = &svar;						\
    ctmp->keymenu   = &config_radiobutton_keymenu;			\
    ctmp->help      = NO_HELP;						\
    ctmp->tool      = NULL;						\
    snprintf(tmp, sizeof(tmp), "%-s =", svar.name);			\
    tmp[sizeof(tmp)-1] = '\0';						\
    ctmp->varname   = cpystr(tmp);					\
    ctmp->varnamep  = ctmpb = ctmp;					\
    ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);			\
									\
    new_confline(&ctmp);						\
    ctmp->var       = NULL;						\
    ctmp->valoffset = rindent;						\
    ctmp->keymenu   = &config_radiobutton_keymenu;			\
    ctmp->help      = NO_HELP;						\
    ctmp->tool      = NULL;						\
    ctmp->varnamep  = ctmpb;						\
    ctmp->flags    |= CF_NOSELECT;					\
    ctmp->value     = cpystr("Set    Choose One");			\
									\
    new_confline(&ctmp);						\
    ctmp->var       = NULL;						\
    ctmp->valoffset = rindent;						\
    ctmp->keymenu   = &config_radiobutton_keymenu;			\
    ctmp->help      = NO_HELP;						\
    ctmp->tool      = radio_tool;					\
    ctmp->varnamep  = ctmpb;						\
    ctmp->flags    |= CF_NOSELECT;					\
    ctmp->value     = cpystr(set_choose);				\
									\
    /* find longest value's name */					\
    for(lv = 0, i = 0; (f = msg_state_types(i)); i++)			\
      if(lv < (j = utf8_width(f->name)))				\
	lv = j;								\
    									\
    lv = MIN(lv, 100);							\
    									\
    for(i = 0; (f = msg_state_types(i)); i++){				\
	new_confline(&ctmp);						\
	ctmp->help_title= htitle;					\
	ctmp->var       = &svar;					\
	ctmp->valoffset = rindent;						\
	ctmp->keymenu   = &config_radiobutton_keymenu;			\
	ctmp->help      = hval;						\
	ctmp->varmem    = i;						\
	ctmp->tool      = radio_tool;					\
	ctmp->varnamep  = ctmpb;					\
	utf8_snprintf(tmp, sizeof(tmp), "(%c)  %-*.*w", (f->value == val)	\
					 ? R_SELD : ' ',		\
		lv, lv, f->name);					\
	tmp[sizeof(tmp)-1] = '\0';					\
	ctmp->value     = cpystr(tmp);					\
    }									\
   }


#define  FEAT_SENTDATE   0
#define  FEAT_IFNOTDEL   1
#define  FEAT_NONTERM    2
bitmap_t feat_option_list;

#define  INABOOK_FROM      0
#define  INABOOK_REPLYTO   1
#define  INABOOK_SENDER    2
#define  INABOOK_TO        3
#define  INABOOK_CC        4
bitmap_t inabook_type_list;


#define INICK_INICK_CONF    0
#define INICK_FROM_CONF     1
#define INICK_REPLYTO_CONF  2
#define INICK_FCC_CONF      3
#define INICK_LITSIG_CONF   4	/* this needs to come before SIG_CONF */
#define INICK_SIG_CONF      5
#define INICK_TEMPL_CONF    6
#define INICK_CSTM_CONF     7
#define INICK_SMTP_CONF     8
#define INICK_NNTP_CONF     9
CONF_S *inick_confs[INICK_NNTP_CONF+1];


/*
 * Screen for editing configuration of a role.
 *
 * Args     ps -- pine struct
 *         def -- default role values to start with
 *       title -- part of title at top of screen
 *      rflags -- which parts of role to edit
 *      result -- This is the returned PAT_S, freed by caller.
 *
 * Returns:  0 if no change
 *           1 if user requested a change
 *               (change is stored in raw_server and hasn't been acted upon yet)
 *          10 user says abort
 */
int
role_config_edit_screen(struct pine *ps, PAT_S *def, char *title, long int rflags, PAT_S **result)
{
    OPT_SCREEN_S     screen, *saved_screen;
    CONF_S          *ctmp = NULL, *ctmpb, *first_line = NULL;
    struct variable  nick_var, to_pat_var, from_pat_var,
		     comment_var,
		     sender_pat_var, cc_pat_var, recip_pat_var, news_pat_var,
		     subj_pat_var, inick_var, fldr_type_var, folder_pat_var,
		     abook_type_var, abook_pat_var,
		     alltext_pat_var, scorei_pat_var, partic_pat_var,
		     bodytext_pat_var, age_pat_var, size_pat_var,
		     keyword_pat_var, charset_pat_var,
		     stat_new_var, stat_del_var, stat_imp_var, stat_ans_var,
		     stat_rec_var, stat_8bit_var,
		     stat_bom_var, stat_boy_var,
		     cat_cmd_var, cati_var, cat_lim_var,
		     from_act_var, replyto_act_var, fcc_act_var,
		     sig_act_var, litsig_act_var, templ_act_var,
                     cstm_act_var, smtp_act_var, nntp_act_var,
		     sort_act_var, iform_act_var, startup_var,
		     repl_type_var, forw_type_var, comp_type_var, score_act_var,
		     hdrtok_act_var,
		     rolecolor_vars[2], filter_type_var, folder_act_var,
		     keyword_set_var, keyword_clr_var,
		     filt_new_var, filt_del_var, filt_imp_var, filt_ans_var;
    struct variable *v, *varlist[65], opt_var, inabook_type_var;
    char            *nick = NULL, *inick = NULL, *fldr_type_pat = NULL,
		    *comment = NULL,
		    *scorei_pat = NULL, *age_pat = NULL, *size_pat = NULL,
		    *abook_type_pat = NULL,
		    *stat_new = NULL, *stat_del = NULL, *stat_imp = NULL,
		    *stat_rec = NULL, *stat_ans = NULL, *stat_8bit = NULL,
		    *stat_bom = NULL, *stat_boy = NULL,
		    *filt_new = NULL, *filt_del = NULL, *filt_imp = NULL,
		    *filt_ans = NULL, *cati = NULL, *cat_lim = NULL,
		    *from_act = NULL, *replyto_act = NULL, *fcc_act = NULL,
		    *sig_act = NULL, *litsig_act = NULL, *sort_act = NULL,
		    *templ_act = NULL, *repl_type = NULL, *forw_type = NULL,
		    *comp_type = NULL, *rc_fg = NULL, *rc_bg = NULL,
		    *score_act = NULL, *filter_type = NULL,
		    *hdrtok_act = NULL,
		    *iform_act = NULL, *startup_act = NULL,
		    *old_fg = NULL, *old_bg = NULL;
    char           **to_pat = NULL, **from_pat = NULL, **sender_pat = NULL,
		   **cc_pat = NULL, **news_pat = NULL, **recip_pat = NULL,
		   **partic_pat = NULL, **subj_pat = NULL,
		   **alltext_pat = NULL, **bodytext_pat = NULL,
		   **keyword_pat = NULL, **folder_pat = NULL,
		   **charset_pat = NULL,
		   **abook_pat = NULL, **folder_act = NULL,
		   **keyword_set = NULL, **keyword_clr = NULL,
		   **cat_cmd = NULL, **cstm_act = NULL, **smtp_act = NULL,
		   **nntp_act = NULL, **spat;
    char             tmp[MAXPATH+1], **apval, **lval, ***alval, *p;
    /* TRANSLATORS: These next 4 are subheading for sections of a configuration screen. */
    char            *fstr = _(" CURRENT FOLDER CONDITIONS BEGIN HERE ");
    char             mstr[50];
    char            *astr = _(" ACTIONS BEGIN HERE ");
    char            *ustr = _(" USES BEGIN HERE ");
    char            *ostr = _(" OPTIONS BEGIN HERE ");
    char            *s_p_v_n = _("Subject pattern");	/* longest one of these */
    char            *u_s_s = _("Use SMTP Server");		/* ditto */
    char            *c_v_n = _("Exit Status Interval");	/* ditto */
    SortOrder        def_sort;
    int              def_sort_rev;
    ARBHDR_S        *aa, *a;
    EARB_S          *earb = NULL, *ea;
    int              rv, i, j, lv, pindent, maxpindent, rindent,
		     scoreval = 0, edit_role, wid,
		     edit_incol, edit_score, edit_filter, edit_other, edit_srch,
		     dval, ival, nval, aval, fval,
		     per_folder_only, need_uses, need_options;
    int	        (*radio_tool)(struct pine *, int, CONF_S **, unsigned);
    int	        (*addhdr_tool)(struct pine *, int, CONF_S **, unsigned);
    int	        (*t_tool)(struct pine *, int, CONF_S **, unsigned);
    NAMEVAL_S       *f;

    dprint((4, "role_config_edit_screen()\n"));
    edit_role	= rflags & ROLE_DO_ROLES;
    edit_incol	= rflags & ROLE_DO_INCOLS;
    edit_score	= rflags & ROLE_DO_SCORES;
    edit_filter	= rflags & ROLE_DO_FILTER;
    edit_other	= rflags & ROLE_DO_OTHER;
    edit_srch	= rflags & ROLE_DO_SRCH;

    per_folder_only = (edit_other &&
		       !(edit_role || edit_incol || edit_score || edit_filter || edit_srch));
    need_uses       = edit_role;
    need_options    = !per_folder_only;

    radio_tool = edit_filter ? role_filt_radiobutton_tool
			     : role_radiobutton_tool;
    t_tool = edit_filter ? role_filt_text_tool : role_text_tool;
    addhdr_tool = edit_filter ? role_filt_addhdr_tool : role_addhdr_tool;

    rindent = 12;	/* radio indent */

    /*
     * We edit by making a nested call to conf_scroll_screen.
     * We use some fake struct variables to get back the results in, and
     * so we can use the existing tools from the config screen.
     */
    varlist[j = 0] = &nick_var;
    varlist[++j] = &comment_var;
    varlist[++j] = &to_pat_var;
    varlist[++j] = &from_pat_var;
    varlist[++j] = &sender_pat_var;
    varlist[++j] = &cc_pat_var;
    varlist[++j] = &recip_pat_var;
    varlist[++j] = &partic_pat_var;
    varlist[++j] = &news_pat_var;
    varlist[++j] = &subj_pat_var;
    varlist[++j] = &alltext_pat_var;
    varlist[++j] = &bodytext_pat_var;
    varlist[++j] = &keyword_pat_var;
    varlist[++j] = &charset_pat_var;
    varlist[++j] = &age_pat_var;
    varlist[++j] = &size_pat_var;
    varlist[++j] = &scorei_pat_var;
    varlist[++j] = &stat_new_var;
    varlist[++j] = &stat_rec_var;
    varlist[++j] = &stat_del_var;
    varlist[++j] = &stat_imp_var;
    varlist[++j] = &stat_ans_var;
    varlist[++j] = &stat_8bit_var;
    varlist[++j] = &stat_bom_var;
    varlist[++j] = &stat_boy_var;
    varlist[++j] = &cat_cmd_var;
    varlist[++j] = &cati_var;
    varlist[++j] = &cat_lim_var;
    varlist[++j] = &inick_var;
    varlist[++j] = &fldr_type_var;
    varlist[++j] = &folder_pat_var;
    varlist[++j] = &abook_type_var;
    varlist[++j] = &abook_pat_var;
    varlist[++j] = &from_act_var;
    varlist[++j] = &replyto_act_var;
    varlist[++j] = &fcc_act_var;
    varlist[++j] = &sig_act_var;
    varlist[++j] = &litsig_act_var;
    varlist[++j] = &sort_act_var;
    varlist[++j] = &iform_act_var;
    varlist[++j] = &startup_var;
    varlist[++j] = &templ_act_var;
    varlist[++j] = &cstm_act_var;
    varlist[++j] = &smtp_act_var;
    varlist[++j] = &nntp_act_var;
    varlist[++j] = &score_act_var;
    varlist[++j] = &hdrtok_act_var;
    varlist[++j] = &repl_type_var;
    varlist[++j] = &forw_type_var;
    varlist[++j] = &comp_type_var;
    varlist[++j] = &rolecolor_vars[0];
    varlist[++j] = &rolecolor_vars[1];
    varlist[++j] = &filter_type_var;
    varlist[++j] = &folder_act_var;
    varlist[++j] = &keyword_set_var;
    varlist[++j] = &keyword_clr_var;
    varlist[++j] = &filt_new_var;
    varlist[++j] = &filt_del_var;
    varlist[++j] = &filt_imp_var;
    varlist[++j] = &filt_ans_var;
    varlist[++j] = &opt_var;
    varlist[++j] = &inabook_type_var;
    varlist[++j] = NULL;
    for(j = 0; varlist[j]; j++)
      memset(varlist[j], 0, sizeof(struct variable));

    if(def && ((def->patgrp && def->patgrp->bogus) || (def->action && def->action->bogus))){
	char msg[MAX_SCREEN_COLS+1];

	snprintf(msg, sizeof(msg),
		_("Rule contains unknown %s element, possibly from newer Alpine"),
		(def->patgrp && def->patgrp->bogus) ? "pattern" : "action");
	msg[sizeof(msg)-1] = '\0';
	q_status_message(SM_ORDER | SM_DING, 7, 7, msg);
	q_status_message(SM_ORDER | SM_DING, 7, 7,
		_("Editing with this version of Alpine will destroy information"));
        flush_status_messages(0);
    }

    role_forw_ptr = role_repl_ptr = role_fldr_ptr = role_filt_ptr = NULL;
    role_status1_ptr = role_status2_ptr = role_status3_ptr = NULL;
    role_status4_ptr = role_status5_ptr = role_status6_ptr = NULL;
    role_status7_ptr = NULL; role_status8_ptr = NULL;
    msg_state1_ptr = msg_state2_ptr = NULL;
    msg_state3_ptr = msg_state4_ptr = NULL;
    role_afrom_ptr = startup_ptr = NULL;
    role_comment_ptr = NULL;

    nick_var.name       = cpystr(_("Nickname"));
    nick_var.is_used    = 1;
    nick_var.is_user    = 1;
    apval = APVAL(&nick_var, ew);
    *apval = (def && def->patgrp && def->patgrp->nick)
				? cpystr(def->patgrp->nick) : NULL;

    nick_var.global_val.p = cpystr(edit_role
				    ? "Alternate Role"
				    : (edit_other
				       ? "Other Rule"
				       : (edit_incol
					  ? "Index Color Rule"
					  : (edit_score
					     ? "Score Rule"
					     : "Filter Rule"))));
    set_current_val(&nick_var, FALSE, FALSE);

    role_comment_ptr    = &comment_var;		/* so radiobuttons can tell */
    comment_var.name    = cpystr(_("Comment"));
    comment_var.is_used = 1;
    comment_var.is_user = 1;
    apval = APVAL(&comment_var, ew);
    *apval = (def && def->patgrp && def->patgrp->comment)
				? cpystr(def->patgrp->comment) : NULL;
    set_current_val(&comment_var, FALSE, FALSE);

    /* TRANSLATORS: Quite a few of the translations to follow are from the
       rules editing screens. These are mostly headings of individual categories
       of criteria which can be set in a rule. */
    setup_dummy_pattern_var(&to_pat_var, _("To pattern"),
			   (def && def->patgrp) ? def->patgrp->to : NULL);
    setup_dummy_pattern_var(&from_pat_var, _("From pattern"),
			   (def && def->patgrp) ? def->patgrp->from : NULL);
    setup_dummy_pattern_var(&sender_pat_var, _("Sender pattern"),
			   (def && def->patgrp) ? def->patgrp->sender : NULL);
    setup_dummy_pattern_var(&cc_pat_var, _("Cc pattern"),
			   (def && def->patgrp) ? def->patgrp->cc : NULL);
    setup_dummy_pattern_var(&news_pat_var, _("News pattern"),
			   (def && def->patgrp) ? def->patgrp->news : NULL);
    setup_dummy_pattern_var(&subj_pat_var, s_p_v_n,
			   (def && def->patgrp) ? def->patgrp->subj : NULL);
    /* TRANSLATORS: Recip is an abbreviation for Recipients which stands for
       all of the recipients of a message. */
    setup_dummy_pattern_var(&recip_pat_var, _("Recip pattern"),
			   (def && def->patgrp) ? def->patgrp->recip : NULL);
    /* TRANSLATORS: Partic is an abbreviation for Participants which stands for
       all of the recipients plus the sender of a message. */
    setup_dummy_pattern_var(&partic_pat_var, _("Partic pattern"),
			   (def && def->patgrp) ? def->patgrp->partic : NULL);
    /* TRANSLATORS: AllText means all of the text of a message */
    setup_dummy_pattern_var(&alltext_pat_var, _("AllText pattern"),
			   (def && def->patgrp) ? def->patgrp->alltext : NULL);
    /* TRANSLATORS: BdyText means the text of a message but not the text in the headers */
    setup_dummy_pattern_var(&bodytext_pat_var, _("BdyText pattern"),
			   (def && def->patgrp) ? def->patgrp->bodytext : NULL);

    setup_dummy_pattern_var(&keyword_pat_var, _("Keyword pattern"),
			   (def && def->patgrp) ? def->patgrp->keyword : NULL);
    setup_dummy_pattern_var(&charset_pat_var, _("Charset pattern"),
			   (def && def->patgrp) ? def->patgrp->charsets : NULL);

    age_pat_global_ptr     = &age_pat_var;
    /* TRANSLATORS: Age interval is a setting for how old the message is. */
    age_pat_var.name       = cpystr(_("Age interval"));
    age_pat_var.is_used    = 1;
    age_pat_var.is_user    = 1;
    if(def && def->patgrp && def->patgrp->do_age){
	apval = APVAL(&age_pat_var, ew);
	*apval = stringform_of_intvl(def->patgrp->age);
    }

    set_current_val(&age_pat_var, FALSE, FALSE);

    size_pat_global_ptr     = &size_pat_var;
    size_pat_var.name       = cpystr(_("Size interval"));
    size_pat_var.is_used    = 1;
    size_pat_var.is_user    = 1;
    if(def && def->patgrp && def->patgrp->do_size){
	apval = APVAL(&size_pat_var, ew);
	*apval = stringform_of_intvl(def->patgrp->size);
    }

    set_current_val(&size_pat_var, FALSE, FALSE);

    scorei_pat_global_ptr     = &scorei_pat_var;
    /* TRANSLATORS: Score is an alpine concept where the score can be kept for a
       message to see if it is a message you want to look at. */
    scorei_pat_var.name       = cpystr(_("Score interval"));
    scorei_pat_var.is_used    = 1;
    scorei_pat_var.is_user    = 1;
    if(def && def->patgrp && def->patgrp->do_score){
	apval = APVAL(&scorei_pat_var, ew);
	*apval = stringform_of_intvl(def->patgrp->score);
    }

    set_current_val(&scorei_pat_var, FALSE, FALSE);

    role_status1_ptr = &stat_del_var;		/* so radiobuttons can tell */
    stat_del_var.name       = cpystr(_("Message is Deleted?"));
    stat_del_var.is_used    = 1;
    stat_del_var.is_user    = 1;
    apval = APVAL(&stat_del_var, ew);
    *apval = (f=role_status_types((def && def->patgrp) ? def->patgrp->stat_del : -1)) ? cpystr(f->name) : NULL;
    set_current_val(&stat_del_var, FALSE, FALSE);

    role_status2_ptr = &stat_new_var;		/* so radiobuttons can tell */
    stat_new_var.name       = cpystr(_("Message is New (Unseen)?"));
    stat_new_var.is_used    = 1;
    stat_new_var.is_user    = 1;
    apval = APVAL(&stat_new_var, ew);
    *apval = (f=role_status_types((def && def->patgrp) ? def->patgrp->stat_new : -1)) ? cpystr(f->name) : NULL;
    set_current_val(&stat_new_var, FALSE, FALSE);

    role_status3_ptr = &stat_imp_var;		/* so radiobuttons can tell */
    stat_imp_var.name       = cpystr(_("Message is Important?"));
    stat_imp_var.is_used    = 1;
    stat_imp_var.is_user    = 1;
    apval = APVAL(&stat_imp_var, ew);
    *apval = (f=role_status_types((def && def->patgrp) ? def->patgrp->stat_imp : -1)) ? cpystr(f->name) : NULL;
    set_current_val(&stat_imp_var, FALSE, FALSE);

    role_status4_ptr = &stat_ans_var;		/* so radiobuttons can tell */
    stat_ans_var.name       = cpystr(_("Message is Answered?"));
    stat_ans_var.is_used    = 1;
    stat_ans_var.is_user    = 1;
    apval = APVAL(&stat_ans_var, ew);
    *apval = (f=role_status_types((def && def->patgrp) ? def->patgrp->stat_ans : -1)) ? cpystr(f->name) : NULL;
    set_current_val(&stat_ans_var, FALSE, FALSE);

    role_status5_ptr = &stat_8bit_var;		/* so radiobuttons can tell */
    stat_8bit_var.name       = cpystr(_("Subject contains raw 8-bit?"));
    stat_8bit_var.is_used    = 1;
    stat_8bit_var.is_user    = 1;
    apval = APVAL(&stat_8bit_var, ew);
    *apval = (f=role_status_types((def && def->patgrp) ? def->patgrp->stat_8bitsubj : -1)) ? cpystr(f->name) : NULL;
    set_current_val(&stat_8bit_var, FALSE, FALSE);

    role_status6_ptr = &stat_rec_var;		/* so radiobuttons can tell */
    stat_rec_var.name       = cpystr(_("Message is Recent?"));
    stat_rec_var.is_used    = 1;
    stat_rec_var.is_user    = 1;
    apval = APVAL(&stat_rec_var, ew);
    *apval = (f=role_status_types((def && def->patgrp) ? def->patgrp->stat_rec : -1)) ? cpystr(f->name) : NULL;
    set_current_val(&stat_rec_var, FALSE, FALSE);

    role_status7_ptr = &stat_bom_var;		/* so radiobuttons can tell */
    stat_bom_var.name       = cpystr(_("Beginning of Month?"));
    stat_bom_var.is_used    = 1;
    stat_bom_var.is_user    = 1;
    apval = APVAL(&stat_bom_var, ew);
    *apval = (f=role_status_types((def && def->patgrp) ? def->patgrp->stat_bom : -1)) ? cpystr(f->name) : NULL;
    set_current_val(&stat_bom_var, FALSE, FALSE);

    role_status8_ptr = &stat_boy_var;		/* so radiobuttons can tell */
    stat_boy_var.name       = cpystr(_("Beginning of Year?"));
    stat_boy_var.is_used    = 1;
    stat_boy_var.is_user    = 1;
    apval = APVAL(&stat_boy_var, ew);
    *apval = (f=role_status_types((def && def->patgrp) ? def->patgrp->stat_boy : -1)) ? cpystr(f->name) : NULL;
    set_current_val(&stat_boy_var, FALSE, FALSE);



    convert_statebits_to_vals((def && def->action) ? def->action->state_setting_bits : 0L, &dval, &aval, &ival, &nval);
    msg_state1_ptr = &filt_del_var;		/* so radiobuttons can tell */
    /* TRANSLATORS: these are actions that might be taken by the rule */
    filt_del_var.name       = cpystr(_("Set Deleted Status"));
    filt_del_var.is_used    = 1;
    filt_del_var.is_user    = 1;
    apval = APVAL(&filt_del_var, ew);
    *apval = (f=msg_state_types(dval)) ? cpystr(f->name) : NULL;
    set_current_val(&filt_del_var, FALSE, FALSE);

    msg_state2_ptr = &filt_new_var;		/* so radiobuttons can tell */
    filt_new_var.name       = cpystr(_("Set New Status"));
    filt_new_var.is_used    = 1;
    filt_new_var.is_user    = 1;
    apval = APVAL(&filt_new_var, ew);
    *apval = (f=msg_state_types(nval)) ? cpystr(f->name) : NULL;
    set_current_val(&filt_new_var, FALSE, FALSE);

    msg_state3_ptr = &filt_imp_var;		/* so radiobuttons can tell */
    filt_imp_var.name       = cpystr(_("Set Important Status"));
    filt_imp_var.is_used    = 1;
    filt_imp_var.is_user    = 1;
    apval = APVAL(&filt_imp_var, ew);
    *apval = (f=msg_state_types(ival)) ? cpystr(f->name) : NULL;
    set_current_val(&filt_imp_var, FALSE, FALSE);

    msg_state4_ptr = &filt_ans_var;		/* so radiobuttons can tell */
    filt_ans_var.name       = cpystr(_("Set Answered Status"));
    filt_ans_var.is_used    = 1;
    filt_ans_var.is_user    = 1;
    apval = APVAL(&filt_ans_var, ew);
    *apval = (f=msg_state_types(aval)) ? cpystr(f->name) : NULL;
    set_current_val(&filt_ans_var, FALSE, FALSE);
    
    inick_var.name       = cpystr(_("Initialize settings using role"));
    inick_var.is_used    = 1;
    inick_var.is_user    = 1;
    apval = APVAL(&inick_var, ew);
    *apval = (def && def->action && def->action->inherit_nick &&
	      def->action->inherit_nick[0])
	       ? cpystr(def->action->inherit_nick) : NULL;

    role_fldr_ptr = &fldr_type_var;		/* so radiobuttons can tell */
    fldr_type_var.name       = cpystr(_("Current Folder Type"));
    fldr_type_var.is_used    = 1;
    fldr_type_var.is_user    = 1;
    apval = APVAL(&fldr_type_var, ew);
    *apval = (f=pat_fldr_types((def && def->patgrp) ? def->patgrp->fldr_type : (!def && edit_filter) ? FLDR_SPECIFIC : FLDR_DEFL)) ? cpystr(f->name) : NULL;
    set_current_val(&fldr_type_var, FALSE, FALSE);

    setup_dummy_pattern_var(&folder_pat_var, _("Folder List"),
			   (def && def->patgrp) ? def->patgrp->folder : NULL);
    /* special default for folder_pat */
    alval = ALVAL(&folder_pat_var, ew);
    if(alval && !*alval && !def && edit_filter){
	char **ltmp;

	ltmp    = (char **) fs_get(2 * sizeof(*ltmp));
	ltmp[0] = cpystr(ps_global->inbox_name);
	ltmp[1] = NULL;
	*alval  = ltmp;
	set_current_val(&folder_pat_var, FALSE, FALSE);
    }

    role_afrom_ptr = &abook_type_var;		/* so radiobuttons can tell */
    abook_type_var.name       = cpystr(_("Address in address book?"));
    abook_type_var.is_used    = 1;
    abook_type_var.is_user    = 1;
    apval = APVAL(&abook_type_var, ew);
    *apval = (f=inabook_fldr_types((def && def->patgrp) ? def->patgrp->inabook : IAB_EITHER)) ? cpystr(f->name) : NULL;
    set_current_val(&abook_type_var, FALSE, FALSE);

    /* TRANSLATORS: Abook is an abbreviation for Address Book */
    setup_dummy_pattern_var(&abook_pat_var, _("Abook List"),
			   (def && def->patgrp) ? def->patgrp->abooks : NULL);

    /*
     * This is a little different from some of the other patterns. Tt is
     * actually a char ** in the struct instead of a PATTERN_S.
     */
    cat_cmd_global_ptr     = &cat_cmd_var;
    cat_cmd_var.name       = cpystr(_("External Categorizer Commands"));
    cat_cmd_var.is_used    = 1;
    cat_cmd_var.is_user    = 1;
    cat_cmd_var.is_list    = 1;
    alval = ALVAL(&cat_cmd_var, ew);
    *alval = (def && def->patgrp && def->patgrp->category_cmd &&
	      def->patgrp->category_cmd[0])
	       ? copy_list_array(def->patgrp->category_cmd) : NULL;
    set_current_val(&cat_cmd_var, FALSE, FALSE);

    cati_global_ptr     = &cati_var;
    cati_var.name       = cpystr(c_v_n);
    cati_var.is_used    = 1;
    cati_var.is_user    = 1;
    if(def && def->patgrp && def->patgrp->do_cat && def->patgrp->category_cmd &&
       def->patgrp->category_cmd[0]){
	apval = APVAL(&cati_var, ew);
	*apval = stringform_of_intvl(def->patgrp->cat);
    }

    set_current_val(&cati_var, FALSE, FALSE);

    cat_lim_global_ptr     = &cat_lim_var;
    cat_lim_var.name       = cpystr(_("Character Limit"));
    cat_lim_var.is_used    = 1;
    cat_lim_var.is_user    = 1;
    cat_lim_var.global_val.p = cpystr("-1");
    apval = APVAL(&cat_lim_var, ew);
    if(def && def->patgrp && def->patgrp->category_cmd &&
       def->patgrp->category_cmd[0] && def->patgrp->cat_lim != -1){
	*apval = (char *) fs_get(20 * sizeof(char));
	snprintf(*apval, 20, "%ld", def->patgrp->cat_lim);
	(*apval)[20-1] = '\0';
    }

    set_current_val(&cat_lim_var, FALSE, FALSE);

    from_act_var.name       = cpystr(_("Set From"));
    from_act_var.is_used    = 1;
    from_act_var.is_user    = 1;
    if(def && def->action && def->action->from){
	char *bufp;
	size_t len;

	len = est_size(def->action->from);
	bufp = (char *) fs_get(len * sizeof(char));
	apval = APVAL(&from_act_var, ew);
	*apval = addr_string_mult(def->action->from, bufp, len);
    }
    else{
	apval = APVAL(&from_act_var, ew);
	*apval = NULL;
    }

    replyto_act_var.name       = cpystr(_("Set Reply-To"));
    replyto_act_var.is_used    = 1;
    replyto_act_var.is_user    = 1;
    if(def && def->action && def->action->replyto){
	char *bufp;
	size_t len;

	len = est_size(def->action->replyto);
	bufp = (char *) fs_get(len * sizeof(char));
	apval = APVAL(&replyto_act_var, ew);
	*apval = addr_string_mult(def->action->replyto, bufp, len);
    }
    else{
	apval = APVAL(&replyto_act_var, ew);
	*apval = NULL;
    }

    fcc_act_var.name       = cpystr(_("Set Fcc"));
    fcc_act_var.is_used    = 1;
    fcc_act_var.is_user    = 1;
    apval = APVAL(&fcc_act_var, ew);
    *apval = (def && def->action && def->action->fcc)
	       ? cpystr(def->action->fcc) : NULL;

    sort_act_var.name       = cpystr(_("Set Sort Order"));
    sort_act_var.is_used    = 1;
    sort_act_var.is_user    = 1;
    apval = APVAL(&sort_act_var, ew);
    if(def && def->action && def->action->is_a_other &&
       def->action->sort_is_set){
	snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%s%s", sort_name(def->action->sortorder),
		(def->action->revsort) ? "/Reverse" : "");
	tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	*apval = cpystr(tmp_20k_buf);
    }
    else
      *apval = NULL;

    iform_act_var.name       = cpystr(_("Set Index Format"));
    iform_act_var.is_used    = 1;
    iform_act_var.is_user    = 1;
    apval = APVAL(&iform_act_var, ew);
    *apval = (def && def->action && def->action->is_a_other &&
	      def->action->index_format)
	       ? cpystr(def->action->index_format) : NULL;
    if(ps_global->VAR_INDEX_FORMAT){
	iform_act_var.global_val.p = cpystr(ps_global->VAR_INDEX_FORMAT);
	set_current_val(&iform_act_var, FALSE, FALSE);
    }

    startup_ptr            = &startup_var;
    startup_var.name       = cpystr(_("Set Startup Rule"));
    startup_var.is_used    = 1;
    startup_var.is_user    = 1;
    apval = APVAL(&startup_var, ew);
    *apval = NULL;
    if(def && def->action && def->action->is_a_other){
	*apval = (f=startup_rules(def->action->startup_rule))
					? cpystr(f->name) : NULL;
	set_current_val(&startup_var, FALSE, FALSE);
    }
    if(!*apval){
	*apval = (f=startup_rules(IS_NOTSET)) ? cpystr(f->name) : NULL;
	set_current_val(&startup_var, FALSE, FALSE);
    }

    /* TRANSLATORS: LiteralSig is a way to keep the signature in the configuration
       file instead of in a separate Signature file. */
    litsig_act_var.name       = cpystr(_("Set LiteralSig"));
    litsig_act_var.is_used    = 1;
    litsig_act_var.is_user    = 1;
    apval = APVAL(&litsig_act_var, ew);
    *apval = (def && def->action && def->action->litsig)
	       ? cpystr(def->action->litsig) : NULL;

    sig_act_var.name       = cpystr(_("Set Signature"));
    sig_act_var.is_used    = 1;
    sig_act_var.is_user    = 1;
    apval = APVAL(&sig_act_var, ew);
    *apval = (def && def->action && def->action->sig)
	       ? cpystr(def->action->sig) : NULL;

    /* TRANSLATORS: A template is a skeleton of a message to be used
       for composing a new message */
    templ_act_var.name       = cpystr(_("Set Template"));
    templ_act_var.is_used    = 1;
    templ_act_var.is_user    = 1;
    apval = APVAL(&templ_act_var, ew);
    *apval = (def && def->action && def->action->template)
		 ? cpystr(def->action->template) : NULL;

    /* TRANSLATORS: Hdrs is an abbreviation for Headers */
    cstm_act_var.name       = cpystr(_("Set Other Hdrs"));
    cstm_act_var.is_used    = 1;
    cstm_act_var.is_user    = 1;
    cstm_act_var.is_list    = 1;
    alval = ALVAL(&cstm_act_var, ew);
    *alval = (def && def->action && def->action->cstm)
		 ? copy_list_array(def->action->cstm) : NULL;

    smtp_act_var.name       = cpystr(u_s_s);
    smtp_act_var.is_used    = 1;
    smtp_act_var.is_user    = 1;
    smtp_act_var.is_list    = 1;
    alval = ALVAL(&smtp_act_var, ew);
    *alval = (def && def->action && def->action->smtp)
		 ? copy_list_array(def->action->smtp) : NULL;

    nntp_act_var.name       = cpystr(_("Use NNTP Server"));
    nntp_act_var.is_used    = 1;
    nntp_act_var.is_user    = 1;
    nntp_act_var.is_list    = 1;
    alval = ALVAL(&nntp_act_var, ew);
    *alval = (def && def->action && def->action->nntp)
		 ? copy_list_array(def->action->nntp) : NULL;

    score_act_global_ptr     = &score_act_var;
    score_act_var.name       = cpystr(_("Score Value"));
    score_act_var.is_used    = 1;
    score_act_var.is_user    = 1;
    if(def && def->action && def->action->scoreval >= SCORE_MIN &&
       def->action->scoreval <= SCORE_MAX)
      scoreval = def->action->scoreval;

    score_act_var.global_val.p = cpystr("0");
    if(scoreval != 0){
	apval = APVAL(&score_act_var, ew);
	*apval = (char *)fs_get(20 * sizeof(char));
	snprintf(*apval, 20, "%d", scoreval);
	(*apval)[20-1] = '\0';
    }

    set_current_val(&score_act_var, FALSE, FALSE);

    hdrtok_act_var.name       = cpystr(_("Score From Header"));
    hdrtok_act_var.is_used    = 1;
    hdrtok_act_var.is_user    = 1;
    if(def && def->action && def->action->scorevalhdrtok){
	apval = APVAL(&hdrtok_act_var, ew);
	*apval = hdrtok_to_stringform(def->action->scorevalhdrtok);
    }

    set_current_val(&hdrtok_act_var, FALSE, FALSE);

    role_repl_ptr = &repl_type_var;		/* so radiobuttons can tell */
    /* TRANSLATORS: For these, Use is a now. This part of the rule describes how
       it will be used when Replying so it is the Reply Use */
    repl_type_var.name       = cpystr(_("Reply Use"));
    repl_type_var.is_used    = 1;
    repl_type_var.is_user    = 1;
    apval = APVAL(&repl_type_var, ew);
    *apval = (f=role_repl_types((def && def->action) ? def->action->repl_type : -1)) ? cpystr(f->name) : NULL;
    set_current_val(&repl_type_var, FALSE, FALSE);

    role_forw_ptr = &forw_type_var;		/* so radiobuttons can tell */
    forw_type_var.name       = cpystr(_("Forward Use"));
    forw_type_var.is_used    = 1;
    forw_type_var.is_user    = 1;
    apval = APVAL(&forw_type_var, ew);
    *apval = (f=role_forw_types((def && def->action) ? def->action->forw_type : -1)) ? cpystr(f->name) : NULL;
    set_current_val(&forw_type_var, FALSE, FALSE);

    comp_type_var.name       = cpystr(_("Compose Use"));
    comp_type_var.is_used    = 1;
    comp_type_var.is_user    = 1;
    apval = APVAL(&comp_type_var, ew);
    *apval = (f=role_comp_types((def && def->action) ? def->action->comp_type : -1)) ? cpystr(f->name) : NULL;
    set_current_val(&comp_type_var, FALSE, FALSE);

    rolecolor_vars[0].is_used    = 1;
    rolecolor_vars[0].is_user    = 1;
    apval = APVAL(&rolecolor_vars[0], ew);
    *apval = (def && def->action && def->action->incol &&
	      def->action->incol->fg[0])
	         ? cpystr(def->action->incol->fg) : NULL;
    rolecolor_vars[1].is_used    = 1;
    rolecolor_vars[1].is_user    = 1;
    rolecolor_vars[0].name = cpystr("ic-foreground-color");
    rolecolor_vars[1].name = cpystr(rolecolor_vars[0].name);
    strncpy(rolecolor_vars[1].name + 3, "back", 4);
    apval = APVAL(&rolecolor_vars[1], ew);
    *apval = (def && def->action && def->action->incol &&
	      def->action->incol->bg[0])
	         ? cpystr(def->action->incol->bg) : NULL;
    set_current_val(&rolecolor_vars[0], FALSE, FALSE);
    set_current_val(&rolecolor_vars[1], FALSE, FALSE);
    old_fg = PVAL(&rolecolor_vars[0], ew) ? cpystr(PVAL(&rolecolor_vars[0], ew))
					  : NULL;
    old_bg = PVAL(&rolecolor_vars[1], ew) ? cpystr(PVAL(&rolecolor_vars[1], ew))
					  : NULL;


    /* save the old opt_screen before calling scroll screen again */
    saved_screen = opt_screen;

    pindent = utf8_width(s_p_v_n);	/* the longest one */
    maxpindent = pindent + 6;
    for(a = (def && def->patgrp) ? def->patgrp->arbhdr : NULL; a; a = a->next)
      if((lv=utf8_width(a->field ? a->field : "")+utf8_width(" pattern")) > pindent)
	pindent = lv;

    pindent = MIN(pindent, maxpindent);
    
    pindent += NOTLEN;	/* width of `! ' */

    pindent += 3;	/* width of ` = ' */

    /* Nickname */
    new_confline(&ctmp);
    ctmp->help_title= _("HELP FOR NICKNAME");
    ctmp->var       = &nick_var;
    ctmp->valoffset = pindent;
    ctmp->keymenu   = &config_role_keymenu;
    ctmp->help      = edit_role ? h_config_role_nick :
		       edit_incol ? h_config_incol_nick :
			edit_score ? h_config_score_nick :
			 edit_other ? h_config_other_nick
			            : h_config_filt_nick;
    ctmp->tool      = t_tool;
    utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", pindent-3, pindent-3, nick_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);
    ctmp->varmem    = -1;

    first_line = ctmp;
    if(rflags & ROLE_CHANGES)
      first_line->flags |= CF_CHANGES;

    /* Comment */
    new_confline(&ctmp);
    ctmp->help_title= _("HELP FOR COMMENT");
    ctmp->var       = &comment_var;
    ctmp->valoffset = pindent;
    ctmp->keymenu   = &config_role_keymenu;
    ctmp->help      = h_config_role_comment;
    ctmp->tool      = role_litsig_text_tool;
    utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", pindent-3, pindent-3, comment_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);
    ctmp->varmem    = -1;

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT;
    if(ps->ttyo->screen_cols >= (wid=utf8_width(fstr)) + 4){
	int dashes;

	dashes = (ps->ttyo->screen_cols - wid)/2;
	snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%s%s%s", repeat_char(dashes, '='),
		 fstr, repeat_char(ps->ttyo->screen_cols-wid-dashes, '='));
	ctmp->value = cpystr(tmp_20k_buf);
    }
    else
      ctmp->value = cpystr(repeat_char(ps->ttyo->screen_cols, '='));

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

    /* Folder Type */
    new_confline(&ctmp);
    ctmp->var       = &fldr_type_var;
    ctmp->keymenu   = &config_radiobutton_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = NULL;
    snprintf(tmp, sizeof(tmp), "%s =", fldr_type_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmpb = ctmp;
    ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);

    new_confline(&ctmp);
    ctmp->var       = NULL;
    ctmp->valoffset = rindent;
    ctmp->keymenu   = &config_radiobutton_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = NULL;
    ctmp->varnamep  = ctmpb;
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr("Set    Choose One");

    new_confline(&ctmp);
    ctmp->var       = NULL;
    ctmp->valoffset = rindent;
    ctmp->keymenu   = &config_radiobutton_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = radio_tool;
    ctmp->varnamep  = ctmpb;
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr(set_choose);				\

    /* find longest value's name */
    for(lv = 0, i = 0; (f = pat_fldr_types(i)); i++)
      if(lv < (j = utf8_width(f->name)))
	lv = j;
    
    lv = MIN(lv, 100);

    fval = -1;
    for(i = 0; (f = pat_fldr_types(i)); i++){
	new_confline(&ctmp);
	ctmp->help_title= _("HELP FOR CURRENT FOLDER TYPE");
	ctmp->var       = &fldr_type_var;
	ctmp->valoffset = rindent;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = edit_role ? h_config_role_fldr_type :
			   edit_incol ? h_config_incol_fldr_type :
			    edit_score ? h_config_score_fldr_type :
			     edit_other ? h_config_other_fldr_type
				        : h_config_filt_fldr_type;
	ctmp->varmem    = i;
	ctmp->tool      = radio_tool;
	ctmp->varnamep  = ctmpb;

	if((PVAL(&fldr_type_var, ew) &&
	    !strucmp(PVAL(&fldr_type_var, ew), f->name))
	   || (!PVAL(&fldr_type_var, ew) && f->value == FLDR_DEFL))
	  fval = f->value;

	utf8_snprintf(tmp, sizeof(tmp), "(%c)  %-*.*w",
		(fval == f->value) ? R_SELD : ' ',
		lv, lv, f->name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->value     = cpystr(tmp);
    }

    /* Folder */
    /* 5 is the width of `(*)  ' */
    setup_role_pat_alt(ps, &ctmp, &folder_pat_var,
		       edit_role ? h_config_role_fldr_type :
			edit_incol ? h_config_incol_fldr_type :
			 edit_score ? h_config_score_fldr_type :
			  edit_other ? h_config_other_fldr_type
				     : h_config_filt_fldr_type,
		       _("HELP FOR FOLDER LIST"),
		       &config_role_patfolder_keymenu, t_tool, rindent+5,
		       !(fval == FLDR_SPECIFIC));

  if(!per_folder_only){		/* sorry about that indent */

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT;

    /* TRANSLATORS: The %s is replaced with one of the 4 or 5 words below, CURRENT,
       SCORED, and so on. */
    snprintf(mstr, sizeof(mstr), _(" %s MESSAGE CONDITIONS BEGIN HERE "),
	    edit_role ? _("CURRENT") :
	     edit_score ? _("SCORED") :
	      edit_incol ? _("COLORED") :
	       edit_filter ? _("FILTERED") : _("CURRENT"));
    mstr[sizeof(mstr)-1] = '\0';

    if(ps->ttyo->screen_cols >= (wid=utf8_width(mstr)) + 4){
	int dashes;

	dashes = (ps->ttyo->screen_cols - wid)/2;
	snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%s%s%s", repeat_char(dashes, '='),
		 mstr, repeat_char(ps->ttyo->screen_cols-wid-dashes, '='));
	ctmp->value = cpystr(tmp_20k_buf);
    }
    else
      ctmp->value = cpystr(repeat_char(ps->ttyo->screen_cols, '='));

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

    setup_role_pat(ps, &ctmp, &to_pat_var,
		   edit_role ? h_config_role_topat :
		    edit_incol ? h_config_incol_topat :
		     edit_score ? h_config_score_topat :
		      edit_other ? h_config_other_topat
			         : h_config_filt_topat,
		   _("HELP FOR TO PATTERN"),
		   &config_role_addr_pat_keymenu, t_tool, &earb, pindent);

    setup_role_pat(ps, &ctmp, &from_pat_var, h_config_role_frompat,
		   _("HELP FOR FROM PATTERN"),
		   &config_role_addr_pat_keymenu, t_tool, &earb, pindent);

    setup_role_pat(ps, &ctmp, &sender_pat_var, h_config_role_senderpat,
		   _("HELP FOR SENDER PATTERN"),
		   &config_role_addr_pat_keymenu, t_tool, &earb, pindent);

    setup_role_pat(ps, &ctmp, &cc_pat_var, h_config_role_ccpat,
		   _("HELP FOR CC PATTERN"),
		   &config_role_addr_pat_keymenu, t_tool, &earb, pindent);

    setup_role_pat(ps, &ctmp, &news_pat_var, h_config_role_newspat,
		   _("HELP FOR NEWS PATTERN"),
		   &config_role_keymenu_not, t_tool, &earb, pindent);

    setup_role_pat(ps, &ctmp, &subj_pat_var, h_config_role_subjpat,
		   _("HELP FOR SUBJECT PATTERN"),
		   &config_role_keymenu_not, t_tool, &earb, pindent);

    setup_role_pat(ps, &ctmp, &recip_pat_var, h_config_role_recippat,
		   _("HELP FOR RECIPIENT PATTERN"),
		   &config_role_addr_pat_keymenu, t_tool, &earb, pindent);

    setup_role_pat(ps, &ctmp, &partic_pat_var, h_config_role_particpat,
		   _("HELP FOR PARTICIPANT PATTERN"),
		   &config_role_addr_pat_keymenu, t_tool, &earb, pindent);

    /* Arbitrary Patterns */
    ea = NULL;
    for(j = 0, a = (def && def->patgrp) ? def->patgrp->arbhdr : NULL;
	a;
	j++, a = a->next){
	char *fn = (a->field) ? a->field : "";

	if(ea){
	    ea->next = (EARB_S *)fs_get(sizeof(*ea));
	    ea = ea->next;
	}
	else{
	    earb = (EARB_S *)fs_get(sizeof(*ea));
	    ea = earb;
	}

	memset((void *)ea, 0, sizeof(*ea));
	ea->v = (struct variable *)fs_get(sizeof(struct variable));
	memset((void *)ea->v, 0, sizeof(struct variable));
	ea->a = (ARBHDR_S *)fs_get(sizeof(ARBHDR_S));
	memset((void *)ea->a, 0, sizeof(ARBHDR_S));

	ea->a->field = cpystr(fn);

	p = (char *) fs_get(strlen(fn) + strlen(" pattern") + 1);
	snprintf(p, strlen(fn) + strlen(" pattern") + 1, "%s pattern", fn);
	p[strlen(fn) + strlen(" pattern") + 1 - 1] = '\0';
	setup_dummy_pattern_var(ea->v, p, a->p);
	fs_give((void **) &p);
	setup_role_pat(ps, &ctmp, ea->v, h_config_role_arbpat,
		       ARB_HELP, &config_role_xtrahdr_keymenu,
		       t_tool, &earb, pindent);
    }

    new_confline(&ctmp);
    ctmp->help_title = _("HELP FOR EXTRA HEADERS");
    ctmp->value = cpystr(ADDXHDRS);
    ctmp->keymenu = &config_role_keymenu_extra;
    ctmp->help      = h_config_role_arbpat;
    ctmp->tool      = addhdr_tool;
    ctmp->d.earb    = &earb;
    ctmp->varmem    = -1;

    setup_role_pat(ps, &ctmp, &alltext_pat_var, h_config_role_alltextpat,
		   _("HELP FOR ALL TEXT PATTERN"),
		   &config_role_keymenu_not, t_tool, &earb, pindent);

    setup_role_pat(ps, &ctmp, &bodytext_pat_var, h_config_role_bodytextpat,
		   _("HELP FOR BODY TEXT PATTERN"),
		   &config_role_keymenu_not, t_tool, &earb, pindent);

    /* Age Interval */
    new_confline(&ctmp);
    ctmp->help_title= _("HELP FOR AGE INTERVAL");
    ctmp->var       = &age_pat_var;
    ctmp->valoffset = pindent;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_role_age;
    ctmp->tool      = t_tool;
    utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", pindent-3, pindent-3, age_pat_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);

    /* Size Interval */
    new_confline(&ctmp);
    ctmp->help_title= _("HELP FOR SIZE INTERVAL");
    ctmp->var       = &size_pat_var;
    ctmp->valoffset = pindent;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_role_size;
    ctmp->tool      = t_tool;
    utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", pindent-3, pindent-3, size_pat_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);

    if(!edit_score){
	/* Score Interval */
	new_confline(&ctmp);
	ctmp->help_title= _("HELP FOR SCORE INTERVAL");
	ctmp->var       = &scorei_pat_var;
	ctmp->valoffset = pindent;
	ctmp->keymenu   = &config_text_keymenu;
	ctmp->help      = h_config_role_scorei;
	ctmp->tool      = t_tool;
	utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", pindent-3, pindent-3, scorei_pat_var.name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmp;
	ctmp->value     = pretty_value(ps, ctmp);
    }

    /* Keyword Pattern */
    setup_role_pat(ps, &ctmp, &keyword_pat_var, h_config_role_keywordpat,
		   _("HELP FOR KEYWORD PATTERN"),
		   &config_role_keyword_keymenu_not, role_text_tool_kword,
		   NULL, pindent);

    /* Charset Pattern */
    setup_role_pat(ps, &ctmp, &charset_pat_var, h_config_role_charsetpat,
		   _("HELP FOR CHARACTER SET PATTERN"),
		   &config_role_charset_keymenu_not, role_text_tool_charset,
		   NULL, pindent);

    /* Important Status */
    SETUP_PAT_STATUS(ctmp, stat_imp_var, def->patgrp->stat_imp,
		     _("HELP FOR IMPORTANT STATUS"), h_config_role_stat_imp);
    /* New Status */
    SETUP_PAT_STATUS(ctmp, stat_new_var, def->patgrp->stat_new,
		     _("HELP FOR NEW STATUS"), h_config_role_stat_new);
    /* Recent Status */
    SETUP_PAT_STATUS(ctmp, stat_rec_var, def->patgrp->stat_rec,
		     _("HELP FOR RECENT STATUS"), h_config_role_stat_recent);
    /* Deleted Status */
    SETUP_PAT_STATUS(ctmp, stat_del_var, def->patgrp->stat_del,
		     _("HELP FOR DELETED STATUS"), h_config_role_stat_del);
    /* Answered Status */
    SETUP_PAT_STATUS(ctmp, stat_ans_var, def->patgrp->stat_ans,
		     _("HELP FOR ANSWERED STATUS"), h_config_role_stat_ans);
    /* 8-bit Subject */
    SETUP_PAT_STATUS(ctmp, stat_8bit_var, def->patgrp->stat_8bitsubj,
		     _("HELP FOR 8-BIT SUBJECT"), h_config_role_stat_8bitsubj);
    /* Beginning of month */
    SETUP_PAT_STATUS(ctmp, stat_bom_var, def->patgrp->stat_bom,
		     _("HELP FOR BEGINNING OF MONTH"), h_config_role_bom);
    /* Beginning of year */
    SETUP_PAT_STATUS(ctmp, stat_boy_var, def->patgrp->stat_boy,
		     _("HELP FOR BEGINNING OF YEAR"), h_config_role_boy);

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

    /* From in Addrbook */
    new_confline(&ctmp);
    ctmp->var       = &abook_type_var;
    ctmp->keymenu   = &config_radiobutton_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = NULL;
    snprintf(tmp, sizeof(tmp), "%s =", abook_type_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmpb = ctmp;
    ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);

    new_confline(&ctmp);
    ctmp->var       = NULL;
    ctmp->valoffset = rindent;
    ctmp->keymenu   = &config_radiobutton_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = NULL;
    ctmp->varnamep  = ctmpb;
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr("Set    Choose One");

    new_confline(&ctmp);
    ctmp->var       = NULL;
    ctmp->valoffset = rindent;
    ctmp->keymenu   = &config_radiobutton_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = radio_tool;
    ctmp->varnamep  = ctmpb;
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr(set_choose);				\

    /* find longest value's name */
    for(lv = 0, i = 0; (f = inabook_fldr_types(i)); i++)
      if(lv < (j = utf8_width(f->name)))
	lv = j;
    
    lv = MIN(lv, 100);

    fval = -1;
    for(i = 0; (f = inabook_fldr_types(i)); i++){
	new_confline(&ctmp);
	ctmp->help_title= _("HELP FOR ADDRESS IN ADDRESS BOOK");
	ctmp->var       = &abook_type_var;
	ctmp->valoffset = rindent;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = h_config_role_abookfrom;
	ctmp->varmem    = i;
	ctmp->tool      = radio_tool;
	ctmp->varnamep  = ctmpb;

	if((PVAL(&abook_type_var, ew) &&
	    !strucmp(PVAL(&abook_type_var, ew), f->name))
	   || (!PVAL(&abook_type_var, ew) && f->value == IAB_DEFL))
	  fval = f->value;

	utf8_snprintf(tmp, sizeof(tmp), "(%c)  %-*.*w",
		(fval == f->value) ? R_SELD : ' ',
		lv, lv, f->name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->value     = cpystr(tmp);
    }

    /* Specific list of abooks */
    /* 5 is the width of `(*)  ' */
    setup_role_pat_alt(ps, &ctmp, &abook_pat_var, h_config_role_abookfrom,
		       _("HELP FOR ABOOK LIST"),
		       &config_role_afrom_keymenu, role_text_tool_afrom, rindent+5,
		       !(fval == IAB_SPEC_YES || fval == IAB_SPEC_NO));

    /* Which addresses to check for */
    inabook_type_var.name      = cpystr(_("Types of addresses to check for in address book"));
    inabook_type_var.is_used   = 1;
    inabook_type_var.is_user   = 1;
    inabook_type_var.is_list   = 1;
    clrbitmap(inabook_type_list);
    if(def && def->patgrp && def->patgrp->inabook & IAB_FROM)
      setbitn(INABOOK_FROM, inabook_type_list);
    if(def && def->patgrp && def->patgrp->inabook & IAB_REPLYTO)
      setbitn(INABOOK_REPLYTO, inabook_type_list);
    if(def && def->patgrp && def->patgrp->inabook & IAB_SENDER)
      setbitn(INABOOK_SENDER, inabook_type_list);
    if(def && def->patgrp && def->patgrp->inabook & IAB_TO)
      setbitn(INABOOK_TO, inabook_type_list);
    if(def && def->patgrp && def->patgrp->inabook & IAB_CC)
      setbitn(INABOOK_CC, inabook_type_list);

    /* default setting */
    if(!(bitnset(INABOOK_FROM, inabook_type_list)
	 || bitnset(INABOOK_REPLYTO, inabook_type_list)
	 || bitnset(INABOOK_SENDER, inabook_type_list)
	 || bitnset(INABOOK_TO, inabook_type_list)
	 || bitnset(INABOOK_CC, inabook_type_list))){
	setbitn(INABOOK_FROM, inabook_type_list);
	setbitn(INABOOK_REPLYTO, inabook_type_list);
    }

    new_confline(&ctmp);
    ctmp->var       = &inabook_type_var;
    ctmp->varoffset = 4;
    ctmp->valoffset = 23;
    ctmp->keymenu   = &config_checkbox_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = NULL;
    snprintf(tmp, sizeof(tmp), "%-20s =", inabook_type_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmpb = ctmp;
    ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);

    new_confline(&ctmp);
    ctmp->var       = NULL;
    ctmp->valoffset = rindent;
    ctmp->keymenu   = &config_checkbox_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = inabook_checkbox_tool;
    ctmp->varnamep  = ctmpb;
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr("Set   Address types");

    new_confline(&ctmp);
    ctmp->var       = NULL;
    ctmp->valoffset = rindent;
    ctmp->keymenu   = &config_checkbox_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = inabook_checkbox_tool;
    ctmp->varnamep  = ctmpb;
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr(set_choose);

    /*  find longest value's name */
    for(lv = 0, i = 0; (f = inabook_feature_list(i)); i++){
	if(lv < (j = utf8_width(f->name)))
	  lv = j;
    }
    
    lv = MIN(lv, 100);

    for(i = 0; (f = inabook_feature_list(i)); i++){
	new_confline(&ctmp);
	ctmp->var       = &opt_var;
	ctmp->help_title= _("HELP FOR ADDRESS TYPES");
	ctmp->varnamep  = ctmpb;
	ctmp->keymenu   = &config_checkbox_keymenu;
	switch(i){
	  case INABOOK_FROM:
	    ctmp->help      = h_config_inabook_from;
	    break;
	  case INABOOK_REPLYTO:
	    ctmp->help      = h_config_inabook_replyto;
	    break;
	  case INABOOK_SENDER:
	    ctmp->help      = h_config_inabook_sender;
	    break;
	  case INABOOK_TO:
	    ctmp->help      = h_config_inabook_to;
	    break;
	  case INABOOK_CC:
	    ctmp->help      = h_config_inabook_cc;
	    break;
	}

	ctmp->tool      = inabook_checkbox_tool;
	ctmp->valoffset = rindent;
	ctmp->varmem    = i;
	utf8_snprintf(tmp, sizeof(tmp), "[%c]  %-*.*w", 
		bitnset(f->value, inabook_type_list) ? 'X' : ' ',
		lv, lv, f->name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->value     = cpystr(tmp);
    }

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

    /* 4 is indent of "Command", c_v_n is longest of these three, 3 is ` = ' */
    i = 4 + utf8_width(c_v_n) + 3;

    /* External Command Categorizer */
    new_confline(&ctmp);
    ctmp->var       = &cat_cmd_var;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = NULL;
    snprintf(tmp, sizeof(tmp), "%s =", cat_cmd_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmpb = ctmp;
    ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);

    /* Commands */
    new_confline(&ctmp);
    ctmp->help_title= _("HELP FOR CATEGORIZER COMMAND");
    ctmp->var       = &cat_cmd_var;
    ctmp->varoffset = 4;
    ctmp->valoffset = i;
    ctmp->keymenu   = &config_text_wshuf_keymenu;
    ctmp->help      = h_config_role_cat_cmd;
    ctmp->tool      = t_tool;
    utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", i-4-3, i-4-3, "Command");
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmpb = ctmp;
    ctmp->flags     = CF_STARTITEM;

    if((lval = LVAL(&cat_cmd_var, ew)) != NULL && lval[0]){
	for(j = 0; lval[j]; j++){
	    if(j)
	      (void) new_confline(&ctmp);
	    
	    ctmp->var       = &cat_cmd_var;
	    ctmp->varmem    = j;
	    ctmp->valoffset = i;
	    ctmp->keymenu   = &config_text_wshuf_keymenu;
	    ctmp->help      = h_config_role_cat_cmd;
	    ctmp->tool      = t_tool;
	    ctmp->varnamep  = ctmp;
	    ctmp->value     = pretty_value(ps, ctmp);
	}
    }
    else
      ctmp->value = pretty_value(ps, ctmp);

    /* Exit status interval */
    new_confline(&ctmp);
    ctmp->help_title= _("HELP FOR CATEGORIZER EXIT STATUS");
    ctmp->var       = &cati_var;
    ctmp->varoffset = 4;
    ctmp->valoffset = i;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_role_cat_status;
    ctmp->tool      = t_tool;
    utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", i-4-3, i-4-3, cati_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);

    /* Character Limit */
    new_confline(&ctmp);
    ctmp->help_title= _("HELP FOR CHARACTER LIMIT");
    ctmp->var       = &cat_lim_var;
    ctmp->varoffset = 4;
    ctmp->valoffset = i;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_role_cat_limit;
    ctmp->tool      = t_tool;
    utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", i-4-3, i-4-3, cat_lim_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);
    ctmp->flags    |= CF_NUMBER;
  }

  if(!edit_srch){		/* sorry about that indent */
    /* Actions */

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT;
    if(ps->ttyo->screen_cols >= (wid=utf8_width(astr)) + 4){
	int dashes;

	dashes = (ps->ttyo->screen_cols - wid)/2;
	snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%s%s%s", repeat_char(dashes, '='),
		 astr, repeat_char(ps->ttyo->screen_cols-wid-dashes, '='));
	ctmp->value = cpystr(tmp_20k_buf);
    }
    else
      ctmp->value = cpystr(repeat_char(ps->ttyo->screen_cols, '='));

    if(edit_role){
	int roindent;

	roindent = utf8_width(u_s_s);	/* the longest one */
	roindent += 3;	/* width of ` = ' */

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	/* Inherit Nickname */
	new_confline(&ctmp);
	inick_confs[INICK_INICK_CONF] = ctmp;
	ctmp->help_title= _("HELP FOR INITIAL SET NICKNAME");
	ctmp->var       = &inick_var;
	ctmp->keymenu   = &config_role_inick_keymenu;
	ctmp->help      = h_config_role_inick;
	ctmp->tool      = role_text_tool_inick;
	snprintf(tmp, sizeof(tmp), "%s :", inick_var.name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->valoffset = utf8_width(tmp)+1;
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmp;

	/* From Action */
	new_confline(&ctmp);
	inick_confs[INICK_FROM_CONF] = ctmp;
	ctmp->help_title= _("HELP FOR SET FROM ACTION");
	ctmp->var       = &from_act_var;
	ctmp->valoffset = roindent;
	ctmp->keymenu   = &config_role_addr_act_keymenu;
	ctmp->help      = h_config_role_setfrom;
	ctmp->tool      = role_text_tool;
	utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", roindent-3, roindent-3, from_act_var.name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmp;

	/* Reply-To Action */
	new_confline(&ctmp);
	inick_confs[INICK_REPLYTO_CONF] = ctmp;
	ctmp->help_title= _("HELP FOR SET REPLY-TO ACTION");
	ctmp->var       = &replyto_act_var;
	ctmp->valoffset = roindent;
	ctmp->keymenu   = &config_role_addr_act_keymenu;
	ctmp->help      = h_config_role_setreplyto;
	ctmp->tool      = role_text_tool;
	utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", roindent-3, roindent-3, replyto_act_var.name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmp;

	/* Fcc Action */
	new_confline(&ctmp);
	inick_confs[INICK_FCC_CONF] = ctmp;
	ctmp->help_title= _("HELP FOR SET FCC ACTION");
	ctmp->var       = &fcc_act_var;
	ctmp->valoffset = roindent;
	ctmp->keymenu   = &config_role_actionfolder_keymenu;
	ctmp->help      = h_config_role_setfcc;
	ctmp->tool      = role_text_tool;
	utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", roindent-3, roindent-3, fcc_act_var.name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmp;

	/* LitSig Action */
	new_confline(&ctmp);
	inick_confs[INICK_LITSIG_CONF] = ctmp;
	ctmp->help_title= _("HELP FOR SET LITERAL SIGNATURE ACTION");
	ctmp->var       = &litsig_act_var;
	ctmp->valoffset = roindent;
	ctmp->keymenu   = &config_text_keymenu;
	ctmp->help      = h_config_role_setlitsig;
	ctmp->tool      = role_litsig_text_tool;
	utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", roindent-3, roindent-3, litsig_act_var.name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmp;

	/* Sig Action */
	new_confline(&ctmp);
	inick_confs[INICK_SIG_CONF] = ctmp;
	ctmp->help_title= _("HELP FOR SET SIGNATURE ACTION");
	ctmp->var       = &sig_act_var;
	ctmp->valoffset = roindent;
	if(F_ON(F_DISABLE_ROLES_SIGEDIT, ps_global))
	  ctmp->keymenu   = &config_role_file_res_keymenu;
	else
	  ctmp->keymenu   = &config_role_file_keymenu;

	ctmp->help      = h_config_role_setsig;
	ctmp->tool      = role_text_tool;
	utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", roindent-3, roindent-3, sig_act_var.name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmp;

	/* Template Action */
	new_confline(&ctmp);
	inick_confs[INICK_TEMPL_CONF] = ctmp;
	ctmp->help_title= _("HELP FOR SET TEMPLATE ACTION");
	ctmp->var       = &templ_act_var;
	ctmp->valoffset = roindent;
	if(F_ON(F_DISABLE_ROLES_TEMPLEDIT, ps_global))
	  ctmp->keymenu   = &config_role_file_res_keymenu;
	else
	  ctmp->keymenu   = &config_role_file_keymenu;

	ctmp->help      = h_config_role_settempl;
	ctmp->tool      = role_text_tool;
	utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", roindent-3, roindent-3, templ_act_var.name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmp;

	/* Other Headers Action */
	new_confline(&ctmp);
	inick_confs[INICK_CSTM_CONF] = ctmp;
	ctmp->help_title= _("HELP FOR SET OTHER HEADERS ACTION");
	ctmp->var       = &cstm_act_var;
	ctmp->valoffset = roindent;
	ctmp->keymenu   = &config_text_wshuf_keymenu;
	ctmp->help      = h_config_role_setotherhdr;
	ctmp->tool      = role_cstm_text_tool;
	utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", roindent-3, roindent-3, cstm_act_var.name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmpb = ctmp;
	ctmp->flags     = CF_STARTITEM;

	if((lval = LVAL(&cstm_act_var, ew)) != NULL){
	    for(i = 0; lval[i]; i++){
		if(i)
		  (void)new_confline(&ctmp);

		ctmp->var       = &cstm_act_var;
		ctmp->varmem    = i;
		ctmp->valoffset = roindent;
		ctmp->keymenu   = &config_text_wshuf_keymenu;
		ctmp->help      = h_config_role_setotherhdr;
		ctmp->tool      = role_cstm_text_tool;
		ctmp->varnamep  = ctmpb;
	    }
	}
	else
	  ctmp->varmem = 0;

	/* SMTP Server Action */
	new_confline(&ctmp);
	inick_confs[INICK_SMTP_CONF] = ctmp;
	ctmp->help_title= _("HELP FOR SMTP SERVER ACTION");
	ctmp->var       = &smtp_act_var;
	ctmp->valoffset = roindent;
	ctmp->keymenu   = &config_text_wshuf_keymenu;
	ctmp->help      = h_config_role_usesmtp;
	ctmp->tool      = t_tool;
	utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", roindent-3, roindent-3, smtp_act_var.name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmpb = ctmp;
	ctmp->flags     = CF_STARTITEM;

	if((lval = LVAL(&smtp_act_var, ew)) != NULL){
	    for(i = 0; lval[i]; i++){
		if(i)
		  (void)new_confline(&ctmp);

		ctmp->var       = &smtp_act_var;
		ctmp->varmem    = i;
		ctmp->valoffset = roindent;
		ctmp->keymenu   = &config_text_wshuf_keymenu;
		ctmp->help      = h_config_role_usesmtp;
		ctmp->tool      = t_tool;
		ctmp->varnamep  = ctmpb;
	    }
	}
	else
	  ctmp->varmem = 0;

	/* NNTP Server Action */
	new_confline(&ctmp);
	inick_confs[INICK_NNTP_CONF] = ctmp;
	ctmp->help_title= _("HELP FOR NNTP SERVER ACTION");
	ctmp->var       = &nntp_act_var;
	ctmp->valoffset = roindent;
	ctmp->keymenu   = &config_text_wshuf_keymenu;
	ctmp->help      = h_config_role_usenntp;
	ctmp->tool      = t_tool;
	utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", roindent-3, roindent-3, nntp_act_var.name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmpb = ctmp;
	ctmp->flags     = CF_STARTITEM;

	if((lval = LVAL(&nntp_act_var, ew)) != NULL){
	    for(i = 0; lval[i]; i++){
		if(i)
		  (void)new_confline(&ctmp);

		ctmp->var       = &nntp_act_var;
		ctmp->varmem    = i;
		ctmp->valoffset = roindent;
		ctmp->keymenu   = &config_text_wshuf_keymenu;
		ctmp->help      = h_config_role_usenntp;
		ctmp->tool      = t_tool;
		ctmp->varnamep  = ctmpb;
	    }
	}
	else
	  ctmp->varmem = 0;

	calculate_inick_stuff(ps);
    }
    else
      inick_confs[INICK_INICK_CONF] = NULL;

    if(edit_score){
	int sindent;

	sindent = MAX(utf8_width(score_act_var.name),utf8_width(hdrtok_act_var.name)) + 3;

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	/* Score Value -- This doesn't inherit from inick */
	new_confline(&ctmp);
	ctmp->help_title= _("HELP FOR SCORE VALUE ACTION");
	ctmp->var       = &score_act_var;
	ctmp->valoffset = sindent;
	ctmp->keymenu   = &config_text_keymenu;
	ctmp->help      = h_config_role_scoreval;
	ctmp->tool      = text_tool;
	ctmp->flags    |= CF_NUMBER;
	utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", sindent-3, sindent-3, score_act_var.name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmp;
	ctmp->value     = pretty_value(ps, ctmp);

	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT;
	ctmp->value     = cpystr("   OR");

	/* Score From Header */
	new_confline(&ctmp);
	ctmp->help_title= _("HELP FOR SCORE VALUE FROM HEADER ACTION");
	ctmp->var       = &hdrtok_act_var;
	ctmp->valoffset = sindent;
	ctmp->keymenu   = &config_text_keymenu;
	ctmp->help      = h_config_role_scorehdrtok;
	ctmp->tool      = text_tool;
	utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", sindent-3, sindent-3, hdrtok_act_var.name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmp;
	ctmp->value     = pretty_value(ps, ctmp);
    }

    if(edit_filter){
	/*
	 * Filtering got added in stages, so instead of simply having a
	 * variable in action which is set to one of the three possible
	 * values (FILTER_KILL, FILTER_STATE, FILTER_FOLDER) we infer
	 * the value from other variables. (Perhaps it would still make
	 * sense to change this.)
	 * Action->kill is set iff the user checks Delete.
	 * If the user checks the box that says Just Set State, then kill
	 * is not set and action->folder is not set (and vice versa).
	 * And finally, FILTER_FOLDER is set if !kill and action->folder is set.
	 * (And it is set here as the default if there is no default
	 * action and the user is required to fill in the Folder.)
	 */
	if(def && def->action && def->action->kill)
	  fval = FILTER_KILL;
	else if(def && def->action && !def->action->kill &&
		!def->action->folder)
	  fval = FILTER_STATE;
	else
	  fval = FILTER_FOLDER;

	role_filt_ptr = &filter_type_var;	/* so radiobuttons can tell */
	filter_type_var.name       = cpystr(_("Filter Action"));
	filter_type_var.is_used    = 1;
	filter_type_var.is_user    = 1;
	apval = APVAL(&filter_type_var, ew);
	*apval = (f=filter_types(fval)) ? cpystr(f->name) : NULL;
	set_current_val(&filter_type_var, FALSE, FALSE);

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	/* Filter Type */
	new_confline(&ctmp);
	ctmp->var       = &filter_type_var;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = NULL;
	snprintf(tmp, sizeof(tmp), "%s =", filter_type_var.name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmpb = ctmp;
	ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);

	new_confline(&ctmp);
	ctmp->var       = NULL;
	ctmp->valoffset = rindent;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = NULL;
	ctmp->varnamep  = ctmpb;
	ctmp->flags    |= CF_NOSELECT;
	ctmp->value     = cpystr("Set    Choose One");

	new_confline(&ctmp);
	ctmp->var       = NULL;
	ctmp->valoffset = rindent;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = radio_tool;
	ctmp->varnamep  = ctmpb;
	ctmp->flags    |= CF_NOSELECT;
	ctmp->value     = cpystr(set_choose);				\

	/* find longest value's name */
	for(lv = 0, i = 0; (f = filter_types(i)); i++)
	  if(lv < (j = utf8_width(f->name)))
	    lv = j;
	
	lv = MIN(lv, 100);
	
	for(i = 0; (f = filter_types(i)); i++){
	    new_confline(&ctmp);
	    ctmp->help_title= _("HELP FOR FILTER ACTION");
	    ctmp->var       = &filter_type_var;
	    ctmp->valoffset = rindent;
	    ctmp->keymenu   = &config_radiobutton_keymenu;
	    ctmp->help      = h_config_filt_rule_type;
	    ctmp->varmem    = i;
	    ctmp->tool      = radio_tool;
	    ctmp->varnamep  = ctmpb;
	    utf8_snprintf(tmp, sizeof(tmp), "(%c)  %-*.*w", (f->value == fval) ? R_SELD : ' ',
		    lv, lv, f->name);
	    tmp[sizeof(tmp)-1] = '\0';
	    ctmp->value     = cpystr(tmp);
	}

	/* Specific list of folders to copy to */
	setup_dummy_pattern_var(&folder_act_var, _("Folder List"),
			        (def && def->action)
				   ? def->action->folder : NULL);

	/* 5 is the width of `(*)  ' */
	setup_role_pat_alt(ps, &ctmp, &folder_act_var, h_config_filter_folder,
			   _("HELP FOR FILTER FOLDER NAME"),
			   &config_role_actionfolder_keymenu, t_tool, rindent+5,
			   !(fval == FILTER_FOLDER));

	SETUP_MSG_STATE(ctmp, filt_imp_var, ival,
		      _("HELP FOR SET IMPORTANT STATUS"), h_config_filt_stat_imp);
	SETUP_MSG_STATE(ctmp, filt_new_var, nval,
			_("HELP FOR SET NEW STATUS"), h_config_filt_stat_new);
	SETUP_MSG_STATE(ctmp, filt_del_var, dval,
			_("HELP FOR SET DELETED STATUS"), h_config_filt_stat_del);
	SETUP_MSG_STATE(ctmp, filt_ans_var, aval,
			_("HELP FOR SET ANSWERED STATUS"), h_config_filt_stat_ans);

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	/* Keywords to be Set */
	setup_dummy_pattern_var(&keyword_set_var, _("Set These Keywords"),
			        (def && def->action)
				  ? def->action->keyword_set : NULL);
	setup_role_pat(ps, &ctmp, &keyword_set_var, h_config_filter_kw_set,
		       _("HELP FOR KEYWORDS TO BE SET"),
		       &config_role_keyword_keymenu, role_text_tool_kword,
		       NULL, 23);

	/* Keywords to be Cleared */
	setup_dummy_pattern_var(&keyword_clr_var, _("Clear These Keywords"),
			        (def && def->action)
				  ? def->action->keyword_clr : NULL);
	setup_role_pat(ps, &ctmp, &keyword_clr_var, h_config_filter_kw_clr,
		       _("HELP FOR KEYWORDS TO BE CLEARED"),
		       &config_role_keyword_keymenu, role_text_tool_kword,
		       NULL, 23);
    }

    if(edit_other){
	char     *pval;
	int       oindent;

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags		|= CF_NOSELECT | CF_B_LINE;

	new_confline(&ctmp)->var  = NULL;
	snprintf(tmp, sizeof(tmp), "%s =", sort_act_var.name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->varname		  = cpystr(tmp);
	ctmp->varnamep		  = ctmpb = ctmp;
	ctmp->keymenu		  = &config_radiobutton_keymenu;
	ctmp->help		  = NO_HELP;
	ctmp->tool		  = role_sort_tool;
	ctmp->valoffset		  = rindent;
	ctmp->flags		 |= CF_NOSELECT;

	new_confline(&ctmp)->var  = NULL;
	ctmp->varnamep		  = ctmpb;
	ctmp->keymenu		  = &config_radiobutton_keymenu;
	ctmp->help		  = NO_HELP;
	ctmp->tool		  = role_sort_tool;
	ctmp->valoffset		  = rindent;
	ctmp->flags		 |= CF_NOSELECT;
	ctmp->value = cpystr("Set    Sort Options");

	new_confline(&ctmp)->var = NULL;
	ctmp->varnamep		  = ctmpb;
	ctmp->keymenu		  = &config_radiobutton_keymenu;
	ctmp->help		  = NO_HELP;
	ctmp->tool		  = role_sort_tool;
	ctmp->valoffset	    	  = rindent;
	ctmp->flags              |= CF_NOSELECT;
	ctmp->value     = cpystr(set_choose);				\

	pval = PVAL(&sort_act_var, ew);
	if(pval)
	  decode_sort(pval, &def_sort, &def_sort_rev);

	/* allow user to set their default sort order */
	new_confline(&ctmp)->var = &sort_act_var;
	ctmp->varnamep	      = ctmpb;
	ctmp->keymenu	      = &config_radiobutton_keymenu;
	ctmp->help	      = h_config_perfolder_sort;
	ctmp->tool	      = role_sort_tool;
	ctmp->valoffset	      = rindent;
	ctmp->varmem	      = -1;
	ctmp->value	      = generalized_sort_pretty_value(ps, ctmp, 0);

	for(j = 0; j < 2; j++){
	    for(i = 0; ps->sort_types[i] != EndofList; i++){
		new_confline(&ctmp)->var = &sort_act_var;
		ctmp->varnamep	      = ctmpb;
		ctmp->keymenu	      = &config_radiobutton_keymenu;
		ctmp->help	      = h_config_perfolder_sort;
		ctmp->tool	      = role_sort_tool;
		ctmp->valoffset	      = rindent;
		ctmp->varmem	      = i + (j * EndofList);
		ctmp->value	      = generalized_sort_pretty_value(ps, ctmp,
								      0);
	    }
	}


	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags		|= CF_NOSELECT | CF_B_LINE;

	oindent = utf8_width(iform_act_var.name) + 3;

	/* Index Format Action */
	new_confline(&ctmp);
	ctmp->help_title= _("HELP FOR SET INDEX FORMAT ACTION");
	ctmp->var       = &iform_act_var;
	ctmp->valoffset = oindent;
	ctmp->keymenu   = &config_text_keymenu;
	ctmp->help      = h_config_set_index_format;
	ctmp->tool      = text_tool;
	utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", oindent-3, oindent-3, iform_act_var.name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmp;
	ctmp->value     = pretty_value(ps, ctmp);

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	new_confline(&ctmp);
	ctmp->flags    |= CF_STARTITEM;
	utf8_snprintf(tmp, sizeof(tmp), "%s =", startup_var.name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->varname		  = cpystr(tmp);
	standard_radio_setup(ps, &ctmp, &startup_var, NULL);
    }

    if(edit_incol && pico_usingcolor()){
	char *pval0, *pval1;
	int def;

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	new_confline(&ctmp);
	ctmp->var		 = &rolecolor_vars[0];	/* foreground */
	ctmp->varname		 = cpystr(_("Index Line Color ="));
	ctmp->varnamep		 = ctmpb = ctmp;
	ctmp->flags		|= (CF_STARTITEM | CF_NOSELECT);
	ctmp->keymenu		 = &role_color_setting_keymenu;

	pval0 = PVAL(&rolecolor_vars[0], ew);
	pval1 = PVAL(&rolecolor_vars[1], ew);
	if(pval0 && pval1)
	  def = !(pval0[0] && pval1[1]);
	else
	  def = 1;

	add_color_setting_disp(ps, &ctmp, &rolecolor_vars[0], ctmpb,
			       &role_color_setting_keymenu,
			       &config_checkbox_keymenu,
			       h_config_incol,
			       rindent, 0,
			       def ? ps->VAR_NORM_FORE_COLOR
				   : PVAL(&rolecolor_vars[0], ew),
			       def ? ps->VAR_NORM_BACK_COLOR
				   : PVAL(&rolecolor_vars[1], ew),
			       def);
    }
  }

    if(need_options){
	/* Options */

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT;
	if(ps->ttyo->screen_cols >= (wid=utf8_width(ostr)) + 4){
	    int dashes;

	    dashes = (ps->ttyo->screen_cols - wid)/2;
	    snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%s%s%s", repeat_char(dashes, '='),
		     ostr, repeat_char(ps->ttyo->screen_cols-wid-dashes, '='));
	    ctmp->value = cpystr(tmp_20k_buf);
	}
	else
	  ctmp->value = cpystr(repeat_char(ps->ttyo->screen_cols, '='));

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	opt_var.name      = cpystr(_("Features"));
	opt_var.is_used   = 1;
	opt_var.is_user   = 1;
	opt_var.is_list   = 1;
	clrbitmap(feat_option_list);
	if(def && def->patgrp && def->patgrp->age_uses_sentdate)
	  setbitn(FEAT_SENTDATE, feat_option_list);

	if(edit_filter){
	    if(def && def->action && def->action->move_only_if_not_deleted)
	      setbitn(FEAT_IFNOTDEL, feat_option_list);
	    if(def && def->action && def->action->non_terminating)
	      setbitn(FEAT_NONTERM, feat_option_list);
	}

	/* Options */
	new_confline(&ctmp);
	ctmp->var       = &opt_var;
	ctmp->valoffset = 23;
	ctmp->keymenu   = &config_checkbox_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = NULL;
	snprintf(tmp, sizeof(tmp), "%-20s =", opt_var.name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmpb = ctmp;
	ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);

	new_confline(&ctmp);
	ctmp->var       = NULL;
	ctmp->valoffset = rindent;
	ctmp->keymenu   = &config_checkbox_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = feat_checkbox_tool;
	ctmp->varnamep  = ctmpb;
	ctmp->flags    |= CF_NOSELECT;
	ctmp->value     = cpystr("Set    Feature Name");

	new_confline(&ctmp);
	ctmp->var       = NULL;
	ctmp->valoffset = rindent;
	ctmp->keymenu   = &config_checkbox_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = feat_checkbox_tool;
	ctmp->varnamep  = ctmpb;
	ctmp->flags    |= CF_NOSELECT;
	ctmp->value     = cpystr(set_choose);				\

	/*  find longest value's name */
	for(lv = 0, i = 0; (f = feat_feature_list(i)); i++){
	    if(!edit_filter && (i == FEAT_IFNOTDEL || i == FEAT_NONTERM))
	      continue;

	    if(lv < (j = utf8_width(f->name)))
	      lv = j;
	}
	
	lv = MIN(lv, 100);

	for(i = 0; (f = feat_feature_list(i)); i++){
	    if(!edit_filter && (i == FEAT_IFNOTDEL || i == FEAT_NONTERM))
	      continue;

	    new_confline(&ctmp);
	    ctmp->var       = &opt_var;
	    ctmp->help_title= _("HELP FOR FILTER FEATURES");
	    ctmp->varnamep  = ctmpb;
	    ctmp->keymenu   = &config_checkbox_keymenu;
	    switch(i){
	      case FEAT_SENTDATE:
		ctmp->help      = h_config_filt_opts_sentdate;
		break;
	      case FEAT_IFNOTDEL:
		ctmp->help      = h_config_filt_opts_notdel;
		break;
	      case FEAT_NONTERM:
		ctmp->help      = h_config_filt_opts_nonterm;
		break;
	    }

	    ctmp->tool      = feat_checkbox_tool;
	    ctmp->valoffset = rindent;
	    ctmp->varmem    = i;
	    utf8_snprintf(tmp, sizeof(tmp), "[%c]  %-*.*w", 
		    bitnset(f->value, feat_option_list) ? 'X' : ' ',
		    lv, lv, f->name);
	    tmp[sizeof(tmp)-1] = '\0';
	    ctmp->value     = cpystr(tmp);
	}
    }

    if(need_uses){
	/* Uses */

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT;
	if(ps->ttyo->screen_cols >= (wid=utf8_width(ustr)) + 4){
	    int dashes;

	    dashes = (ps->ttyo->screen_cols - wid)/2;
	    snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%s%s%s", repeat_char(dashes, '='),
		     ustr, repeat_char(ps->ttyo->screen_cols-wid-dashes, '='));
	    ctmp->value = cpystr(tmp_20k_buf);
	}
	else
	  ctmp->value = cpystr(repeat_char(ps->ttyo->screen_cols, '='));

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	/* Reply Type */
	new_confline(&ctmp);
	ctmp->var       = &repl_type_var;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = NULL;
	snprintf(tmp, sizeof(tmp), "%s =", repl_type_var.name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmpb = ctmp;
	ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);

	new_confline(&ctmp);
	ctmp->var       = NULL;
	ctmp->valoffset = rindent;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = NULL;
	ctmp->varnamep  = ctmpb;
	ctmp->flags    |= CF_NOSELECT;
	ctmp->value     = cpystr("Set    Choose One");

	new_confline(&ctmp);
	ctmp->var       = NULL;
	ctmp->valoffset = rindent;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = radio_tool;
	ctmp->varnamep  = ctmpb;
	ctmp->flags    |= CF_NOSELECT;
	ctmp->value     = cpystr(set_choose);				\

	/* find longest value's name */
	for(lv = 0, i = 0; (f = role_repl_types(i)); i++)
	  if(lv < (j = utf8_width(f->name)))
	    lv = j;
	
	lv = MIN(lv, 100);

	for(i = 0; (f = role_repl_types(i)); i++){
	    new_confline(&ctmp);
	    ctmp->help_title= _("HELP FOR ROLE REPLY USE");
	    ctmp->var       = &repl_type_var;
	    ctmp->valoffset = rindent;
	    ctmp->keymenu   = &config_radiobutton_keymenu;
	    ctmp->help      = h_config_role_replyuse;
	    ctmp->varmem    = i;
	    ctmp->tool      = radio_tool;
	    ctmp->varnamep  = ctmpb;
	    utf8_snprintf(tmp, sizeof(tmp), "(%c)  %-*.*w", (((!(def && def->action) ||
					     def->action->repl_type == -1) &&
					    f->value == ROLE_REPL_DEFL) ||
					  (def && def->action &&
					  f->value == def->action->repl_type))
					     ? R_SELD : ' ',
		    lv, lv, f->name);
	    tmp[sizeof(tmp)-1] = '\0';
	    ctmp->value     = cpystr(tmp);
	}

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	/* Forward Type */
	new_confline(&ctmp);
	ctmp->var       = &forw_type_var;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = NULL;
	snprintf(tmp, sizeof(tmp), "%s =", forw_type_var.name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmpb = ctmp;
	ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);

	new_confline(&ctmp);
	ctmp->var       = NULL;
	ctmp->valoffset = rindent;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = NULL;
	ctmp->varnamep  = ctmpb;
	ctmp->flags    |= CF_NOSELECT;
	ctmp->value     = cpystr("Set    Choose One");

	new_confline(&ctmp);
	ctmp->var       = NULL;
	ctmp->valoffset = rindent;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = radio_tool;
	ctmp->varnamep  = ctmpb;
	ctmp->flags    |= CF_NOSELECT;
	ctmp->value     = cpystr(set_choose);				\

	/* find longest value's name */
	for(lv = 0, i = 0; (f = role_forw_types(i)); i++)
	  if(lv < (j = utf8_width(f->name)))
	    lv = j;
	
	lv = MIN(lv, 100);

	for(i = 0; (f = role_forw_types(i)); i++){
	    new_confline(&ctmp);
	    ctmp->help_title= _("HELP FOR ROLE FORWARD USE");
	    ctmp->var       = &forw_type_var;
	    ctmp->valoffset = rindent;
	    ctmp->keymenu   = &config_radiobutton_keymenu;
	    ctmp->help      = h_config_role_forwarduse;
	    ctmp->varmem    = i;
	    ctmp->tool      = radio_tool;
	    ctmp->varnamep  = ctmpb;
	    utf8_snprintf(tmp, sizeof(tmp), "(%c)  %-*.*w", (((!(def && def->action) ||
					     def->action->forw_type == -1) &&
					    f->value == ROLE_FORW_DEFL) ||
					  (def && def->action &&
					  f->value == def->action->forw_type))
					     ? R_SELD : ' ',
		    lv, lv, f->name);
	    tmp[sizeof(tmp)-1] = '\0';
	    ctmp->value     = cpystr(tmp);
	}

	/* Blank line */
	new_confline(&ctmp);
	ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

	/* Compose Type */
	new_confline(&ctmp);
	ctmp->var       = &comp_type_var;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = NULL;
	snprintf(tmp, sizeof(tmp), "%s =", comp_type_var.name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->varname   = cpystr(tmp);
	ctmp->varnamep  = ctmpb = ctmp;
	ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);

	new_confline(&ctmp);
	ctmp->var       = NULL;
	ctmp->valoffset = rindent;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = NULL;
	ctmp->varnamep  = ctmpb;
	ctmp->flags    |= CF_NOSELECT;
	ctmp->value     = cpystr("Set    Choose One");

	new_confline(&ctmp);
	ctmp->var       = NULL;
	ctmp->valoffset = rindent;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = NO_HELP;
	ctmp->tool      = radio_tool;
	ctmp->varnamep  = ctmpb;
	ctmp->flags    |= CF_NOSELECT;
	ctmp->value     = cpystr(set_choose);				\

	/* find longest value's name */
	for(lv = 0, i = 0; (f = role_comp_types(i)); i++)
	  if(lv < (j = utf8_width(f->name)))
	    lv = j;
	
	lv = MIN(lv, 100);
	
	for(i = 0; (f = role_comp_types(i)); i++){
	    new_confline(&ctmp);
	    ctmp->help_title= _("HELP FOR ROLE COMPOSE USE");
	    ctmp->var       = &comp_type_var;
	    ctmp->valoffset = rindent;
	    ctmp->keymenu   = &config_radiobutton_keymenu;
	    ctmp->help      = h_config_role_composeuse;
	    ctmp->varmem    = i;
	    ctmp->tool      = radio_tool;
	    ctmp->varnamep  = ctmpb;
	    utf8_snprintf(tmp, sizeof(tmp), "(%c)  %-*.*w", (((!(def && def->action) ||
					     def->action->comp_type == -1) &&
					    f->value == ROLE_COMP_DEFL) ||
					  (def && def->action &&
					  f->value == def->action->comp_type))
					     ? R_SELD : ' ',
		    lv, lv, f->name);
	    tmp[sizeof(tmp)-1] = '\0';
	    ctmp->value     = cpystr(tmp);
	}
    }

    memset(&screen, 0, sizeof(screen));
    screen.ro_warning = saved_screen ? saved_screen->deferred_ro_warning : 0;
    /* TRANSLATORS: Print something1 using something2.
       "roles" is something1 */
    rv = conf_scroll_screen(ps, &screen, first_line, title, _("roles"),
			    (edit_incol && pico_usingcolor()) ? 1 : 0);

    /*
     * Now look at the fake variables and extract the information we
     * want from them.
     */

    if(rv == 1 && result){
	/*
	 * We know these variables exist, so we don't have to check that
	 * apval is nonnull before evaluating *apval.
	 */
	apval = APVAL(&nick_var, ew);
	nick = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(nick);

	apval = APVAL(&comment_var, ew);
	comment = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(comment);

	alval = ALVAL(&to_pat_var, ew);
	to_pat = *alval;
	*alval = NULL;

	alval = ALVAL(&from_pat_var, ew);
	from_pat = *alval;
	*alval = NULL;

	alval = ALVAL(&sender_pat_var, ew);
	sender_pat = *alval;
	*alval = NULL;

	alval = ALVAL(&cc_pat_var, ew);
	cc_pat = *alval;
	*alval = NULL;

	alval = ALVAL(&recip_pat_var, ew);
	recip_pat = *alval;
	*alval = NULL;

	alval = ALVAL(&partic_pat_var, ew);
	partic_pat = *alval;
	*alval = NULL;

	alval = ALVAL(&news_pat_var, ew);
	news_pat = *alval;
	*alval = NULL;

	alval = ALVAL(&subj_pat_var, ew);
	subj_pat = *alval;
	*alval = NULL;

	alval = ALVAL(&alltext_pat_var, ew);
	alltext_pat = *alval;
	*alval = NULL;

	alval = ALVAL(&bodytext_pat_var, ew);
	bodytext_pat = *alval;
	*alval = NULL;

	alval = ALVAL(&keyword_pat_var, ew);
	keyword_pat = *alval;
	*alval = NULL;

	alval = ALVAL(&charset_pat_var, ew);
	charset_pat = *alval;
	*alval = NULL;

	apval = APVAL(&age_pat_var, ew);
	age_pat = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(age_pat);

	apval = APVAL(&size_pat_var, ew);
	size_pat = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(size_pat);

	apval = APVAL(&scorei_pat_var, ew);
	scorei_pat = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(scorei_pat);

	apval = APVAL(&stat_del_var, ew);
	stat_del = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(stat_del);

	apval = APVAL(&stat_new_var, ew);
	stat_new = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(stat_new);

	apval = APVAL(&stat_rec_var, ew);
	stat_rec = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(stat_rec);

	apval = APVAL(&stat_imp_var, ew);
	stat_imp = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(stat_imp);

	apval = APVAL(&stat_ans_var, ew);
	stat_ans = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(stat_ans);

	apval = APVAL(&stat_8bit_var, ew);
	stat_8bit = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(stat_8bit);

	apval = APVAL(&stat_bom_var, ew);
	stat_bom = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(stat_bom);

	apval = APVAL(&stat_boy_var, ew);
	stat_boy = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(stat_boy);

	apval = APVAL(&fldr_type_var, ew);
	fldr_type_pat = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(fldr_type_pat);

	alval = ALVAL(&folder_pat_var, ew);
	folder_pat = *alval;
	*alval = NULL;

	apval = APVAL(&abook_type_var, ew);
	abook_type_pat = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(abook_type_pat);

	alval = ALVAL(&abook_pat_var, ew);
	abook_pat = *alval;
	*alval = NULL;

	apval = APVAL(&cati_var, ew);
	cati = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(cati);

	apval = APVAL(&cat_lim_var, ew);
	cat_lim = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(cat_lim);

	apval = APVAL(&inick_var, ew);
	inick = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(inick);

	apval = APVAL(&from_act_var, ew);
	from_act = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(from_act);

	apval = APVAL(&replyto_act_var, ew);
	replyto_act = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(replyto_act);

	apval = APVAL(&fcc_act_var, ew);
	fcc_act = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(fcc_act);

	apval = APVAL(&litsig_act_var, ew);
	litsig_act = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(litsig_act);

	apval = APVAL(&sort_act_var, ew);
	sort_act = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(sort_act);

	apval = APVAL(&iform_act_var, ew);
	iform_act = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(iform_act);

	apval = APVAL(&startup_var, ew);
	startup_act = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(startup_act);

	apval = APVAL(&sig_act_var, ew);
	sig_act = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(sig_act);

	apval = APVAL(&templ_act_var, ew);
	templ_act = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(templ_act);

	apval = APVAL(&score_act_var, ew);
	score_act = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(score_act);

	apval = APVAL(&hdrtok_act_var, ew);
	hdrtok_act = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(hdrtok_act);

	apval = APVAL(&repl_type_var, ew);
	repl_type = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(repl_type);

	apval = APVAL(&forw_type_var, ew);
	forw_type = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(forw_type);

	apval = APVAL(&comp_type_var, ew);
	comp_type = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(comp_type);

	apval = APVAL(&rolecolor_vars[0], ew);
	rc_fg = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(rc_fg);

	apval = APVAL(&rolecolor_vars[1], ew);
	rc_bg = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(rc_bg);

	apval = APVAL(&filter_type_var, ew);
	filter_type = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(filter_type);

	alval = ALVAL(&folder_act_var, ew);
	folder_act = *alval;
	*alval = NULL;

	alval = ALVAL(&keyword_set_var, ew);
	keyword_set = *alval;
	*alval = NULL;

	alval = ALVAL(&keyword_clr_var, ew);
	keyword_clr = *alval;
	*alval = NULL;

	apval = APVAL(&filt_imp_var, ew);
	filt_imp = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(filt_imp);

	apval = APVAL(&filt_del_var, ew);
	filt_del = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(filt_del);

	apval = APVAL(&filt_new_var, ew);
	filt_new = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(filt_new);

	apval = APVAL(&filt_ans_var, ew);
	filt_ans = *apval;
	*apval = NULL;
	removing_leading_and_trailing_white_space(filt_ans);


	alval = ALVAL(&cat_cmd_var, ew);
	cat_cmd = *alval;
	*alval = NULL;

	alval = ALVAL(&cstm_act_var, ew);
	cstm_act = *alval;
	*alval = NULL;

	alval = ALVAL(&smtp_act_var, ew);
	smtp_act = *alval;
	*alval = NULL;

	alval = ALVAL(&nntp_act_var, ew);
	nntp_act = *alval;
	*alval = NULL;

	if(ps->VAR_OPER_DIR && sig_act &&
	   is_absolute_path(sig_act) &&
	   !in_dir(ps->VAR_OPER_DIR, sig_act)){
	    q_status_message1(SM_ORDER | SM_DING, 3, 4,
			      _("Warning: Sig file can't be outside of %s"),
			      ps->VAR_OPER_DIR);
	}

	if(ps->VAR_OPER_DIR && templ_act &&
	   is_absolute_path(templ_act) &&
	   !in_dir(ps->VAR_OPER_DIR, templ_act)){
	    q_status_message1(SM_ORDER | SM_DING, 3, 4,
			    _("Warning: Template file can't be outside of %s"),
			    ps->VAR_OPER_DIR);
	}

	if(ps->VAR_OPER_DIR && folder_act)
	  for(i = 0; folder_act[i]; i++){
	    if(folder_act[i][0] && is_absolute_path(folder_act[i]) &&
	       !in_dir(ps->VAR_OPER_DIR, folder_act[i])){
		q_status_message1(SM_ORDER | SM_DING, 3, 4,
				  _("Warning: Folder can't be outside of %s"),
				  ps->VAR_OPER_DIR);
	    }
	  }

	*result = (PAT_S *)fs_get(sizeof(**result));
	memset((void *)(*result), 0, sizeof(**result));

	(*result)->patgrp = (PATGRP_S *)fs_get(sizeof(*(*result)->patgrp));
	memset((void *)(*result)->patgrp, 0, sizeof(*(*result)->patgrp));

	(*result)->action = (ACTION_S *)fs_get(sizeof(*(*result)->action));
	memset((void *)(*result)->action, 0, sizeof(*(*result)->action));

	(*result)->patline = def ? def->patline : NULL;
	
	if(nick && *nick){
	    (*result)->patgrp->nick = nick;
	    nick = NULL;
	}
	else
	  (*result)->patgrp->nick = cpystr(nick_var.global_val.p);

	if(comment && *comment){
	    (*result)->patgrp->comment = comment;
	    comment = NULL;
	}

	(*result)->action->nick = cpystr((*result)->patgrp->nick);

	(*result)->action->is_a_role   = edit_role   ? 1 : 0;
	(*result)->action->is_a_incol  = edit_incol  ? 1 : 0;
	(*result)->action->is_a_score  = edit_score  ? 1 : 0;
	(*result)->action->is_a_filter = edit_filter ? 1 : 0;
	(*result)->action->is_a_other  = edit_other  ? 1 : 0;
	(*result)->action->is_a_srch   = edit_srch   ? 1 : 0;

	(*result)->patgrp->to      = editlist_to_pattern(to_pat);
	if((*result)->patgrp->to && !strncmp(to_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->to->not = 1;

	(*result)->patgrp->from    = editlist_to_pattern(from_pat);
	if((*result)->patgrp->from && !strncmp(from_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->from->not = 1;

	(*result)->patgrp->sender  = editlist_to_pattern(sender_pat);
	if((*result)->patgrp->sender &&
	   !strncmp(sender_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->sender->not = 1;

	(*result)->patgrp->cc      = editlist_to_pattern(cc_pat);
	if((*result)->patgrp->cc && !strncmp(cc_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->cc->not = 1;

	(*result)->patgrp->recip   = editlist_to_pattern(recip_pat);
	if((*result)->patgrp->recip &&
	   !strncmp(recip_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->recip->not = 1;

	(*result)->patgrp->partic  = editlist_to_pattern(partic_pat);
	if((*result)->patgrp->partic &&
	   !strncmp(partic_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->partic->not = 1;

	(*result)->patgrp->news    = editlist_to_pattern(news_pat);
	if((*result)->patgrp->news && !strncmp(news_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->news->not = 1;

	(*result)->patgrp->subj    = editlist_to_pattern(subj_pat);
	if((*result)->patgrp->subj && !strncmp(subj_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->subj->not = 1;

	(*result)->patgrp->alltext = editlist_to_pattern(alltext_pat);
	if((*result)->patgrp->alltext &&
	   !strncmp(alltext_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->alltext->not = 1;

	(*result)->patgrp->bodytext = editlist_to_pattern(bodytext_pat);
	if((*result)->patgrp->bodytext &&
	   !strncmp(bodytext_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->bodytext->not = 1;

	(*result)->patgrp->keyword = editlist_to_pattern(keyword_pat);
	if((*result)->patgrp->keyword &&
	   !strncmp(keyword_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->keyword->not = 1;

	(*result)->patgrp->charsets = editlist_to_pattern(charset_pat);
	if((*result)->patgrp->charsets &&
	   !strncmp(charset_pat_var.name, NOT, NOTLEN))
	  (*result)->patgrp->charsets->not = 1;

	(*result)->patgrp->age_uses_sentdate =
		bitnset(FEAT_SENTDATE, feat_option_list) ? 1 : 0;

	if(age_pat){
	    if(((*result)->patgrp->age  = parse_intvl(age_pat)) != NULL)
	      (*result)->patgrp->do_age = 1;
	}

	if(size_pat){
	    if(((*result)->patgrp->size  = parse_intvl(size_pat)) != NULL)
	      (*result)->patgrp->do_size = 1;
	}

	if(scorei_pat){
	    if(((*result)->patgrp->score = parse_intvl(scorei_pat)) != NULL)
	      (*result)->patgrp->do_score  = 1;
	}

	(*result)->patgrp->cat_lim = -1L; /* default */
	if(cat_cmd){
	    if(!cat_cmd[0])
	      fs_give((void **) &cat_cmd);

	    /* quick check for absolute paths */
	    if(cat_cmd)
	      for(j = 0; cat_cmd[j]; j++)
		if(!is_absolute_path(cat_cmd[j]))
		  q_status_message1(SM_ORDER | SM_DING, 3, 4,
			_("Warning: command must be absolute path: \"%s\""),
			cat_cmd[j]);

	    (*result)->patgrp->category_cmd = cat_cmd;
	    cat_cmd = NULL;

	    if(cati){
		if(((*result)->patgrp->cat = parse_intvl(cati)) != NULL)
		  (*result)->patgrp->do_cat  = 1;
	    }
	    if(cat_lim && *cat_lim)
	      (*result)->patgrp->cat_lim = atol(cat_lim);
	}

	if(stat_del && *stat_del){
	    for(j = 0; (f = role_status_types(j)); j++)
	      if(!strucmp(stat_del, f->name)){
		  (*result)->patgrp->stat_del = f->value;
		  break;
	      }
	}
	else
	  (*result)->patgrp->stat_del = PAT_STAT_EITHER;

	if(stat_new && *stat_new){
	    for(j = 0; (f = role_status_types(j)); j++)
	      if(!strucmp(stat_new, f->name)){
		  (*result)->patgrp->stat_new = f->value;
		  break;
	      }
	}
	else
	  (*result)->patgrp->stat_new = PAT_STAT_EITHER;

	if(stat_rec && *stat_rec){
	    for(j = 0; (f = role_status_types(j)); j++)
	      if(!strucmp(stat_rec, f->name)){
		  (*result)->patgrp->stat_rec = f->value;
		  break;
	      }
	}
	else
	  (*result)->patgrp->stat_rec = PAT_STAT_EITHER;

	if(stat_imp && *stat_imp){
	    for(j = 0; (f = role_status_types(j)); j++)
	      if(!strucmp(stat_imp, f->name)){
		  (*result)->patgrp->stat_imp = f->value;
		  break;
	      }
	}
	else
	  (*result)->patgrp->stat_imp = PAT_STAT_EITHER;

	if(stat_ans && *stat_ans){
	    for(j = 0; (f = role_status_types(j)); j++)
	      if(!strucmp(stat_ans, f->name)){
		  (*result)->patgrp->stat_ans = f->value;
		  break;
	      }
	}
	else
	  (*result)->patgrp->stat_ans = PAT_STAT_EITHER;

	if(stat_8bit && *stat_8bit){
	    for(j = 0; (f = role_status_types(j)); j++)
	      if(!strucmp(stat_8bit, f->name)){
		  (*result)->patgrp->stat_8bitsubj = f->value;
		  break;
	      }
	}
	else
	  (*result)->patgrp->stat_8bitsubj = PAT_STAT_EITHER;

	if(stat_bom && *stat_bom){
	    for(j = 0; (f = role_status_types(j)); j++)
	      if(!strucmp(stat_bom, f->name)){
		  (*result)->patgrp->stat_bom = f->value;
		  break;
	      }
	}
	else
	  (*result)->patgrp->stat_bom = PAT_STAT_EITHER;

	if(stat_boy && *stat_boy){
	    for(j = 0; (f = role_status_types(j)); j++)
	      if(!strucmp(stat_boy, f->name)){
		  (*result)->patgrp->stat_boy = f->value;
		  break;
	      }
	}
	else
	  (*result)->patgrp->stat_boy = PAT_STAT_EITHER;

	if(sort_act){
	    decode_sort(sort_act, &def_sort, &def_sort_rev);
	    (*result)->action->sort_is_set = 1;
	    (*result)->action->sortorder = def_sort;
	    (*result)->action->revsort = (def_sort_rev ? 1 : 0);
	    /*
	     * Don't try to re-sort until next open of folder. If user
	     * $-sorted then it probably shouldn't change anyway. Why
	     * bother keeping track of that?
	     */
	}

	(*result)->action->index_format = iform_act;
	iform_act = NULL;

	if(startup_act && *startup_act){
	    for(j = 0; (f = startup_rules(j)); j++)
	      if(!strucmp(startup_act, f->name)){
		  (*result)->action->startup_rule = f->value;
		  break;
	      }
	}
	else
	  (*result)->action->startup_rule = IS_NOTSET;

	aa = NULL;
	for(ea = earb; ea; ea = ea->next){
	    char *xyz;

	    if(aa){
		aa->next = (ARBHDR_S *)fs_get(sizeof(*aa));
		aa = aa->next;
	    }
	    else{
		(*result)->patgrp->arbhdr =
				      (ARBHDR_S *)fs_get(sizeof(ARBHDR_S));
		aa = (*result)->patgrp->arbhdr;
	    }

	    memset(aa, 0, sizeof(*aa));

	    aa->field = cpystr((ea->a && ea->a->field) ? ea->a->field : "");

	    alval = ALVAL(ea->v, ew);
	    spat = *alval;
	    *alval = NULL;
	    aa->p = editlist_to_pattern(spat);
	    if(aa->p && ea->v && ea->v->name &&
	       !strncmp(ea->v->name, NOT, NOTLEN))
	      aa->p->not = 1;

	    if((xyz = pattern_to_string(aa->p)) != NULL){
		if(!*xyz)
		  aa->isemptyval = 1;
		
		fs_give((void **)&xyz);
	    }
	}

	if(fldr_type_pat && *fldr_type_pat){
	    for(j = 0; (f = pat_fldr_types(j)); j++)
	      if(!strucmp(fldr_type_pat, f->name)){
		  (*result)->patgrp->fldr_type = f->value;
		  break;
	      }
	}
	else{
	    f = pat_fldr_types(FLDR_DEFL);
	    if(f)
	      (*result)->patgrp->fldr_type = f->value;
	}

	(*result)->patgrp->folder = editlist_to_pattern(folder_pat);

	if(abook_type_pat && *abook_type_pat){
	    for(j = 0; (f = inabook_fldr_types(j)); j++)
	      if(!strucmp(abook_type_pat, f->name)){
		  (*result)->patgrp->inabook = f->value;
		  break;
	      }

	      if(bitnset(INABOOK_FROM, inabook_type_list))
		(*result)->patgrp->inabook |= IAB_FROM;
	      if(bitnset(INABOOK_REPLYTO, inabook_type_list))
		(*result)->patgrp->inabook |= IAB_REPLYTO;
	      if(bitnset(INABOOK_SENDER, inabook_type_list))
		(*result)->patgrp->inabook |= IAB_SENDER;
	      if(bitnset(INABOOK_TO, inabook_type_list))
		(*result)->patgrp->inabook |= IAB_TO;
	      if(bitnset(INABOOK_CC, inabook_type_list))
		(*result)->patgrp->inabook |= IAB_CC;

	      if(!((*result)->patgrp->inabook & IAB_TYPE_MASK))
		(*result)->patgrp->inabook |= (IAB_FROM | IAB_REPLYTO);
	}
	else{
	    f = inabook_fldr_types(IAB_DEFL);
	    if(f)
	      (*result)->patgrp->inabook = f->value;
	}

	(*result)->patgrp->abooks = editlist_to_pattern(abook_pat);


	(*result)->action->inherit_nick = inick;
	inick = NULL;
	(*result)->action->fcc = fcc_act;
	fcc_act = NULL;
	(*result)->action->litsig = litsig_act;
	litsig_act = NULL;
	(*result)->action->sig = sig_act;
	sig_act = NULL;
	(*result)->action->template = templ_act;
	templ_act = NULL;

	if(cstm_act){
	    /*
	     * Check for From or Reply-To and eliminate them.
	     */
	    for(i = 0; cstm_act[i]; i++){
		char *free_this;

		if((!struncmp(cstm_act[i],"from",4) &&
		    (cstm_act[i][4] == ':' ||
		     cstm_act[i][4] == '\0')) ||
		   (!struncmp(cstm_act[i],"reply-to",8) &&
		    (cstm_act[i][8] == ':' ||
		     cstm_act[i][8] == '\0'))){
		    free_this = cstm_act[i];
		    /* slide the rest up */
		    for(j = i; cstm_act[j]; j++)
		      cstm_act[j] = cstm_act[j+1];

		    fs_give((void **)&free_this);
		    i--;	/* recheck now that we've slid them up */
		}
	    }

	    /* nothing left */
	    if(!cstm_act[0])
	      fs_give((void **)&cstm_act);

	    (*result)->action->cstm = cstm_act;
	    cstm_act = NULL;
	}

	if(smtp_act){
	    if(!smtp_act[0])
	      fs_give((void **)&smtp_act);

	    (*result)->action->smtp = smtp_act;
	    smtp_act = NULL;
	}

	if(nntp_act){
	    if(!nntp_act[0])
	      fs_give((void **)&nntp_act);

	    (*result)->action->nntp = nntp_act;
	    nntp_act = NULL;
	}

	if(filter_type && *filter_type){
	  (*result)->action->non_terminating =
			bitnset(FEAT_NONTERM, feat_option_list) ? 1 : 0;
	  for(i = 0; (f = filter_types(i)); i++){
	    if(!strucmp(filter_type, f->name)){
	      if(f->value == FILTER_FOLDER){
		(*result)->action->folder = editlist_to_pattern(folder_act);
		(*result)->action->move_only_if_not_deleted =
			bitnset(FEAT_IFNOTDEL, feat_option_list) ? 1 : 0;
	      }
	      else if(f->value == FILTER_STATE){
		(*result)->action->kill = 0;
	      }
	      else if(f->value == FILTER_KILL){
	        (*result)->action->kill = 1;
	      }

	      /*
	       * This is indented an extra indent because we used to condition
	       * this on !kill. We changed it so that you can set state bits
	       * even if you're killing. This is marginally helpful if you
	       * have another client running that doesn't know about this
	       * filter, but you want to, for example, have the messages show
	       * up now as deleted instead of having that deferred until we
	       * exit. It is controlled by the user by setting the status
	       * action bits along with the Delete.
	       */
		if(filt_imp && *filt_imp){
		  for(j = 0; (f = msg_state_types(j)); j++){
		    if(!strucmp(filt_imp, f->name)){
		      switch(f->value){
			case ACT_STAT_LEAVE:
			  break;
			case ACT_STAT_SET:
			  (*result)->action->state_setting_bits |= F_FLAG;
			  break;
			case ACT_STAT_CLEAR:
			  (*result)->action->state_setting_bits |= F_UNFLAG;
			  break;
		      }
		      break;
		    }
		  }
		}

		if(filt_del && *filt_del){
		  for(j = 0; (f = msg_state_types(j)); j++){
		    if(!strucmp(filt_del, f->name)){
		      switch(f->value){
			case ACT_STAT_LEAVE:
			  break;
			case ACT_STAT_SET:
			  (*result)->action->state_setting_bits |= F_DEL;
			  break;
			case ACT_STAT_CLEAR:
			  (*result)->action->state_setting_bits |= F_UNDEL;
			  break;
		      }
		      break;
		    }
		  }
		}

		if(filt_ans && *filt_ans){
		  for(j = 0; (f = msg_state_types(j)); j++){
		    if(!strucmp(filt_ans, f->name)){
		      switch(f->value){
			case ACT_STAT_LEAVE:
			  break;
			case ACT_STAT_SET:
			  (*result)->action->state_setting_bits |= F_ANS;
			  break;
			case ACT_STAT_CLEAR:
			  (*result)->action->state_setting_bits |= F_UNANS;
			  break;
		      }
		      break;
		    }
		  }
		}

		if(filt_new && *filt_new){
		  for(j = 0; (f = msg_state_types(j)); j++){
		    if(!strucmp(filt_new, f->name)){
		      switch(f->value){
			case ACT_STAT_LEAVE:
			  break;
			case ACT_STAT_SET:
			  (*result)->action->state_setting_bits |= F_UNSEEN;
			  break;
			case ACT_STAT_CLEAR:
			  (*result)->action->state_setting_bits |= F_SEEN;
			  break;
		      }
		      break;
		    }
		  }
		}

		(*result)->action->keyword_set =
					editlist_to_pattern(keyword_set);
		(*result)->action->keyword_clr =
					editlist_to_pattern(keyword_clr);

	      break;
	    }
	  }
	}

	if(from_act && *from_act)
	  rfc822_parse_adrlist(&(*result)->action->from, from_act,
			       ps->maildomain);

	if(replyto_act && *replyto_act)
	  rfc822_parse_adrlist(&(*result)->action->replyto, replyto_act,
			       ps->maildomain);

	if(score_act && (j = atoi(score_act)) >= SCORE_MIN && j <= SCORE_MAX)
	  (*result)->action->scoreval = (long) j;

	if(hdrtok_act)
	  (*result)->action->scorevalhdrtok = stringform_to_hdrtok(hdrtok_act);

	if(repl_type && *repl_type){
	    for(j = 0; (f = role_repl_types(j)); j++)
	      if(!strucmp(repl_type, f->name)){
		  (*result)->action->repl_type = f->value;
		  break;
	      }
	}
	else{
	    f = role_repl_types(ROLE_REPL_DEFL);
	    if(f)
	      (*result)->action->repl_type = f->value;
	}

	if(forw_type && *forw_type){
	    for(j = 0; (f = role_forw_types(j)); j++)
	      if(!strucmp(forw_type, f->name)){
		  (*result)->action->forw_type = f->value;
		  break;
	      }
	}
	else{
	    f = role_forw_types(ROLE_FORW_DEFL);
	    if(f)
	      (*result)->action->forw_type = f->value;
	}

	if(comp_type && *comp_type){
	    for(j = 0; (f = role_comp_types(j)); j++)
	      if(!strucmp(comp_type, f->name)){
		  (*result)->action->comp_type = f->value;
		  break;
	      }
	}
	else{
	    f = role_comp_types(ROLE_COMP_DEFL);
	    if(f)
	      (*result)->action->comp_type = f->value;
	}

	if(rc_fg && *rc_fg && rc_bg && *rc_bg){
	    if(!old_fg || !old_bg || strucmp(old_fg, rc_fg) ||
	       strucmp(old_bg, rc_bg))
	      clear_index_cache(ps_global->mail_stream, 0);

	    /*
	     * If same as normal color, don't set it. This may or may
	     * not surprise the user when they change the normal color.
	     * This color will track the normal color instead of staying
	     * the same as the old normal color, which is probably
	     * what they want.
	     */
	    if(!ps_global->VAR_NORM_FORE_COLOR ||
	       !ps_global->VAR_NORM_BACK_COLOR ||
	       strucmp(ps_global->VAR_NORM_FORE_COLOR, rc_fg) ||
	       strucmp(ps_global->VAR_NORM_BACK_COLOR, rc_bg))
	      (*result)->action->incol = new_color_pair(rc_fg, rc_bg);
	}
    }

    for(j = 0; varlist[j]; j++){
	v = varlist[j];
	free_variable_values(v);
	if(v->name)
	  fs_give((void **)&v->name);
    }

    if(earb)
      free_earb(&earb);
    if(nick)
      fs_give((void **)&nick);
    if(comment)
      fs_give((void **)&comment);
    if(to_pat)
      free_list_array(&to_pat);
    if(from_pat)
      free_list_array(&from_pat);
    if(sender_pat)
      free_list_array(&sender_pat);
    if(cc_pat)
      free_list_array(&cc_pat);
    if(recip_pat)
      free_list_array(&recip_pat);
    if(partic_pat)
      free_list_array(&partic_pat);
    if(news_pat)
      free_list_array(&news_pat);
    if(subj_pat)
      free_list_array(&subj_pat);
    if(alltext_pat)
      free_list_array(&alltext_pat);
    if(bodytext_pat)
      free_list_array(&bodytext_pat);
    if(keyword_pat)
      free_list_array(&keyword_pat);
    if(charset_pat)
      free_list_array(&charset_pat);
    if(age_pat)
      fs_give((void **)&age_pat);
    if(size_pat)
      fs_give((void **)&size_pat);
    if(scorei_pat)
      fs_give((void **)&scorei_pat);
    if(cati)
      fs_give((void **)&cati);
    if(cat_lim)
      fs_give((void **)&cat_lim);
    if(stat_del)
      fs_give((void **)&stat_del);
    if(stat_new)
      fs_give((void **)&stat_new);
    if(stat_rec)
      fs_give((void **)&stat_rec);
    if(stat_imp)
      fs_give((void **)&stat_imp);
    if(stat_ans)
      fs_give((void **)&stat_ans);
    if(stat_8bit)
      fs_give((void **)&stat_8bit);
    if(stat_bom)
      fs_give((void **)&stat_bom);
    if(stat_boy)
      fs_give((void **)&stat_boy);
    if(fldr_type_pat)
      fs_give((void **)&fldr_type_pat);
    if(folder_pat)
      free_list_array(&folder_pat);
    if(abook_type_pat)
      fs_give((void **)&abook_type_pat);
    if(abook_pat)
      free_list_array(&abook_pat);
    if(inick)
      fs_give((void **)&inick);
    if(from_act)
      fs_give((void **)&from_act);
    if(replyto_act)
      fs_give((void **)&replyto_act);
    if(fcc_act)
      fs_give((void **)&fcc_act);
    if(litsig_act)
      fs_give((void **)&litsig_act);
    if(sort_act)
      fs_give((void **)&sort_act);
    if(iform_act)
      fs_give((void **)&iform_act);
    if(keyword_set)
      free_list_array(&keyword_set);
    if(keyword_clr)
      free_list_array(&keyword_clr);
    if(startup_act)
      fs_give((void **)&startup_act);
    if(sig_act)
      fs_give((void **)&sig_act);
    if(templ_act)
      fs_give((void **)&templ_act);
    if(score_act)
      fs_give((void **)&score_act);
    if(hdrtok_act)
      fs_give((void **)&hdrtok_act);
    if(repl_type)
      fs_give((void **)&repl_type);
    if(forw_type)
      fs_give((void **)&forw_type);
    if(comp_type)
      fs_give((void **)&comp_type);
    if(rc_fg)
      fs_give((void **)&rc_fg);
    if(rc_bg)
      fs_give((void **)&rc_bg);
    if(old_fg)
      fs_give((void **)&old_fg);
    if(old_bg)
      fs_give((void **)&old_bg);
    if(filt_del)
      fs_give((void **)&filt_del);
    if(filt_new)
      fs_give((void **)&filt_new);
    if(filt_ans)
      fs_give((void **)&filt_ans);
    if(filt_imp)
      fs_give((void **)&filt_imp);
    if(folder_act)
      free_list_array(&folder_act);
    if(filter_type)
      fs_give((void **)&filter_type);

    if(cat_cmd)
      free_list_array(&cat_cmd);

    if(cstm_act)
      free_list_array(&cstm_act);

    if(smtp_act)
      free_list_array(&smtp_act);

    if(nntp_act)
      free_list_array(&nntp_act);

    opt_screen = saved_screen;
    ps->mangled_screen = 1;
    return(rv);
}


void
setup_dummy_pattern_var(struct variable *v, char *name, PATTERN_S *defpat)
{
    char ***alval;

    if(!(v && name))
      panic("setup_dummy_pattern_var");

    v->name = (char *) fs_get(strlen(name)+NOTLEN+1);
    snprintf(v->name, strlen(name)+NOTLEN+1, "%s%s", (defpat && defpat->not) ? NOT : "", name);
    v->name[ strlen(name)+NOTLEN+1-1] = '\0';
    v->is_used    = 1;
    v->is_user    = 1;
    v->is_list    = 1;
    alval = ALVAL(v, ew);
    *alval = pattern_to_editlist(defpat);
    set_current_val(v, FALSE, FALSE);
}


void
setup_role_pat(struct pine *ps, CONF_S **ctmp, struct variable *v, HelpType help,
	       char *help_title, struct key_menu *keymenu,
	       int (*tool)(struct pine *, int, CONF_S **, unsigned),
	       EARB_S **earb, int indent)
{
    char    tmp[MAXPATH+1];
    char  **lval;
    int     i;
    CONF_S *ctmpb;

    new_confline(ctmp);
    (*ctmp)->help_title = help_title;
    (*ctmp)->var        = v;
    (*ctmp)->valoffset  = indent;
    (*ctmp)->keymenu    = keymenu;
    (*ctmp)->help       = help;
    (*ctmp)->tool       = tool;
    utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", indent-3, indent-3, (v && v->name) ? v->name : "");
    tmp[sizeof(tmp)-1] = '\0';
    (*ctmp)->varname    = cpystr(tmp);
    (*ctmp)->varnamep   = *ctmp;
    (*ctmp)->value      = pretty_value(ps, *ctmp);
    (*ctmp)->d.earb     = earb;
    (*ctmp)->varmem     = 0;
    (*ctmp)->flags      = CF_STARTITEM;

    ctmpb = (*ctmp);

    lval = LVAL(v, ew);
    if(lval){
	for(i = 0; lval[i]; i++){
	    if(i)
	      new_confline(ctmp);
	    
	    (*ctmp)->var       = v;
	    (*ctmp)->varmem    = i;
	    (*ctmp)->valoffset = indent;
	    (*ctmp)->value     = pretty_value(ps, *ctmp);
	    (*ctmp)->d.earb    = earb;
	    (*ctmp)->keymenu   = keymenu;
	    (*ctmp)->help      = help;
	    (*ctmp)->tool      = tool;
	    (*ctmp)->varnamep  = ctmpb;
	}
    }
}


/*
 * Watch out for polarity of nosel flag. Setting it means to turn on
 * the NOSELECT flag, which means to make that line unselectable.
 */
void
setup_role_pat_alt(struct pine *ps, CONF_S **ctmp, struct variable *v, HelpType help,
		   char *help_title, struct key_menu *keymenu,
		   int (*tool)(struct pine *, int, CONF_S **, unsigned),
		   int voff, int nosel)
{
    char    tmp[MAXPATH+1];
    char  **lval;
    int     i, j, k;
    CONF_S *ctmpb;

    new_confline(ctmp);
    (*ctmp)->help_title = help_title;
    (*ctmp)->var        = v;

    (*ctmp)->varoffset  = voff;
    k                   = utf8_width(v->name);
    j                   = voff+k+3;
    (*ctmp)->valoffset  = j;

    (*ctmp)->keymenu    = keymenu;
    (*ctmp)->help       = help;
    (*ctmp)->tool       = tool;

    utf8_snprintf(tmp, sizeof(tmp), "%*.*w =", k, k, v->name);
    tmp[sizeof(tmp)-1] = '\0';
    (*ctmp)->varname    = cpystr(tmp);

    (*ctmp)->varnamep   = *ctmp;
    (*ctmp)->value      = pretty_value(ps, *ctmp);
    (*ctmp)->varmem     = 0;

    (*ctmp)->flags      = (nosel ? CF_NOSELECT : 0);

    ctmpb = (*ctmp);

    lval = LVAL(v, ew);
    if(lval){
	for(i = 0; lval[i]; i++){
	    if(i)
	      new_confline(ctmp);
	    
	    (*ctmp)->var       = v;
	    (*ctmp)->varmem    = i;
	    (*ctmp)->varoffset = voff;
	    (*ctmp)->valoffset = j;
	    (*ctmp)->value     = pretty_value(ps, *ctmp);
	    (*ctmp)->keymenu   = keymenu;
	    (*ctmp)->help      = help;
	    (*ctmp)->tool      = tool;
	    (*ctmp)->varnamep  = ctmpb;
	    (*ctmp)->flags     = (nosel ? CF_NOSELECT : 0);
	}
    }
}


void
free_earb(EARB_S **ea)
{
    if(ea && *ea){
	free_earb(&(*ea)->next);
	if((*ea)->v){
	    free_variable_values((*ea)->v);
	    if((*ea)->v->name)
	      fs_give((void **) &(*ea)->v->name);

	    fs_give((void **) &(*ea)->v);;
	}

	free_arbhdr(&(*ea)->a);
	fs_give((void **) ea);
    }
}


void
calculate_inick_stuff(struct pine *ps)
{
    ACTION_S        *role, *irole;
    CONF_S          *ctmp, *ctmpa;
    struct variable *v;
    int              i;
    char            *nick;

    if(inick_confs[INICK_INICK_CONF] == NULL)
      return;

    for(i = INICK_FROM_CONF; i <= INICK_NNTP_CONF; i++){
	v = inick_confs[i] ? inick_confs[i]->var : NULL;
	if(v){
	  if(v->is_list){
	      if(v->global_val.l)
		free_list_array(&v->global_val.l);
	  }
	  else{
	      if(v->global_val.p)
		fs_give((void **)&v->global_val.p);
	  }
	}
    }

    nick = PVAL(inick_confs[INICK_INICK_CONF]->var, ew);

    if(nick){
	/*
	 * Use an empty role with inherit_nick set to nick and then use the
	 * combine function to find the action values.
	 */
	role = (ACTION_S *)fs_get(sizeof(*role));
	memset((void *)role, 0, sizeof(*role));
	role->is_a_role = 1;
	role->inherit_nick = cpystr(nick);
	irole = combine_inherited_role(role);

	ctmp = inick_confs[INICK_FROM_CONF];
	v = ctmp ? ctmp->var : NULL;

	if(irole && irole->from){
	    char *bufp;
	    size_t len;

	    len = est_size(irole->from);
	    bufp = (char *) fs_get(len * sizeof(char));
	    v->global_val.p = addr_string_mult(irole->from, bufp, len);
	}

	ctmp = inick_confs[INICK_REPLYTO_CONF];
	v = ctmp ? ctmp->var : NULL;

	if(irole && irole->replyto){
	    char *bufp;
	    size_t len;

	    len = est_size(irole->replyto);
	    bufp = (char *) fs_get(len * sizeof(char));
	    v->global_val.p = addr_string_mult(irole->replyto, bufp, len);
	}

	ctmp = inick_confs[INICK_FCC_CONF];
	v = ctmp ? ctmp->var : NULL;
	v->global_val.p = (irole && irole->fcc) ? cpystr(irole->fcc) : NULL;
	
	ctmp = inick_confs[INICK_LITSIG_CONF];
	v = ctmp ? ctmp->var : NULL;
	v->global_val.p = (irole && irole->litsig) ? cpystr(irole->litsig)
						   : NULL;

	ctmp = inick_confs[INICK_SIG_CONF];
	v = ctmp ? ctmp->var : NULL;
	v->global_val.p = (irole && irole->sig) ? cpystr(irole->sig) : NULL;

	ctmp = inick_confs[INICK_TEMPL_CONF];
	v = ctmp ? ctmp->var : NULL;
	v->global_val.p = (irole && irole->template)
					? cpystr(irole->template) : NULL;

	ctmp = inick_confs[INICK_CSTM_CONF];
	v = ctmp ? ctmp->var : NULL;
	v->global_val.l = (irole && irole->cstm) ? copy_list_array(irole->cstm)
						 : NULL;

	ctmp = inick_confs[INICK_SMTP_CONF];
	v = ctmp ? ctmp->var : NULL;
	v->global_val.l = (irole && irole->smtp) ? copy_list_array(irole->smtp)
						 : NULL;

	ctmp = inick_confs[INICK_NNTP_CONF];
	v = ctmp ? ctmp->var : NULL;
	v->global_val.l = (irole && irole->nntp) ? copy_list_array(irole->nntp)
						 : NULL;

	free_action(&role);
	free_action(&irole);
    }

    for(i = INICK_INICK_CONF; i <= INICK_NNTP_CONF; i++){
	ctmp = inick_confs[i];
	v = ctmp ? ctmp->var : NULL;
	/*
	 * If we didn't set a global_val using the nick above, then
	 * set one here for each variable that uses one.
	 */
	if(v && !v->global_val.p){
	    char    *str, *astr, *lc, pdir[MAXPATH+1];
	    ADDRESS *addr;
	    int      len;

	    switch(i){
	      case INICK_FROM_CONF:
		addr = generate_from();
		astr = addr_list_string(addr, NULL, 1);
		str = (astr && astr[0]) ? astr : "?";
		v->global_val.p = (char *)fs_get((strlen(str) + 20) *
							    sizeof(char));
		snprintf(v->global_val.p, strlen(str) + 20, "%s%s)", DSTRING, str);
		v->global_val.p[strlen(str) + 20 - 1] = '\0';
		if(astr)
		  fs_give((void **)&astr);

		if(addr)
		  mail_free_address(&addr);

		break;

	      case INICK_FCC_CONF:
		v->global_val.p = cpystr(VSTRING);
		break;

	      case INICK_LITSIG_CONF:
		/*
		 * This default works this way because of the ordering
		 * of the choices in the detoken routine.
		 */
		if(ps->VAR_LITERAL_SIG){
		    str = ps->VAR_LITERAL_SIG;
		    v->global_val.p = (char *)fs_get((strlen(str) + 20) *
								sizeof(char));
		    snprintf(v->global_val.p, strlen(str) + 20,
			    "%s%s)", DSTRING, str);
		    v->global_val.p[strlen(str) + 20 - 1] = '\0';
		}

		break;

	      case INICK_SIG_CONF:
		pdir[0] = '\0';
		if(ps_global->VAR_OPER_DIR){
		    strncpy(pdir, ps_global->VAR_OPER_DIR, MAXPATH);
		    pdir[MAXPATH] = '\0';
		    len = strlen(pdir) + 1;
		}
		else if((lc = last_cmpnt(ps_global->pinerc)) != NULL){
		    strncpy(pdir, ps_global->pinerc,
			    MIN(MAXPATH,lc-ps_global->pinerc));
		    pdir[MIN(MAXPATH, lc-ps_global->pinerc)] = '\0';
		    len = strlen(pdir);
		}

		if(pdir[0] && ps->VAR_SIGNATURE_FILE &&
		   ps->VAR_SIGNATURE_FILE[0] &&
		   is_absolute_path(ps->VAR_SIGNATURE_FILE) &&
		   !strncmp(ps->VAR_SIGNATURE_FILE, pdir, len)){
		    str = ps->VAR_SIGNATURE_FILE + len;
		}
		else
		  str = (ps->VAR_SIGNATURE_FILE && ps->VAR_SIGNATURE_FILE[0])
			  ? ps->VAR_SIGNATURE_FILE : NULL;
		if(str){
		    v->global_val.p = (char *)fs_get((strlen(str) + 20) *
								sizeof(char));
		    snprintf(v->global_val.p, strlen(str) + 20, "%s%s)", DSTRING, str);
		    v->global_val.p[strlen(str) + 20 - 1] = '\0';
		}

		break;

	      case INICK_INICK_CONF:
	      case INICK_REPLYTO_CONF:
	      case INICK_TEMPL_CONF:
	      case INICK_CSTM_CONF:
	      case INICK_SMTP_CONF:
	      case INICK_NNTP_CONF:
		break;
	    }
	}

	if(v)
	  set_current_val(v, FALSE, FALSE);

	if(ctmp){
	    CONF_S          *ctmpsig = NULL;
	    struct variable *vlsig;

	    for(ctmpa = ctmp;
		ctmpa && ctmpa->varnamep == ctmp;
		ctmpa = ctmpa->next){
		if(ctmpa->value)
		  fs_give((void **)&ctmpa->value);

		ctmpa->value = pretty_value(ps, ctmpa);
	    }

	    if(i == INICK_SIG_CONF){
		/*
		 * Turn off NOSELECT, but possibly turn it on again
		 * in next line.
		 */
		if((ctmpsig = inick_confs[INICK_SIG_CONF]) != NULL)
		  ctmpsig->flags &= ~CF_NOSELECT;

		if(inick_confs[INICK_LITSIG_CONF] && 
		   (vlsig = inick_confs[INICK_LITSIG_CONF]->var) &&
		   vlsig->current_val.p &&
		   vlsig->current_val.p[0]){
		    if(ctmp->value)
		      fs_give((void **)&ctmp->value);

		    ctmp->value =
				cpystr("<Ignored: using LiteralSig instead>");
		    
		    ctmp->flags |= CF_NOSELECT;
		}
	    }
	}
    }
}


/* Arguments:
 *      lst:  a list of folders
 *      action: a 1 or 0 value which basically says that str is associated with
 *   the filter action if true or the Current Folder type if false.
 * Return:
 *   Returns 2 on success (user wants to exit) and 0 on failure.
 *
 * This function cycles through a list of folders and checks whether or not each
 * folder exists.  If they exist, return 2, if they don't exist, notify the user
 * or offer to create depending on whether or not it is a filter action or not. 
 * With each of these prompts, the user can abort their desire to exit.
 */
int
check_role_folders(char **lst, unsigned int action)
{
    char       *cur_fn, wt_res, prompt[MAX_SCREEN_COLS];
    int         i, rv = 2;
    CONTEXT_S  *cntxt = NULL;
    char        nbuf1[MAX_SCREEN_COLS], nbuf2[MAX_SCREEN_COLS];
    int         space, w1, w2, exists;

    if(!(lst && *lst)){
      if(action)
	q_status_message(SM_ORDER, 3, 5,
			 _("Set a valid Filter Action before Exiting"));
      else
	q_status_message(SM_ORDER, 3, 5,
			 _("Set a valid Specific Folder before Exiting"));
      rv = 0;
      return rv;
    }

    for(i = 0; lst[i]; i++){
	if(action)
	  cur_fn = detoken_src(lst[i], FOR_FILT, NULL, NULL, NULL, NULL);
	else
	  cur_fn = lst[i];

	removing_leading_and_trailing_white_space(cur_fn);
	if(*cur_fn != '\0'){
	  space = MAXPROMPT;
	  if(is_absolute_path(cur_fn) || !context_isambig(cur_fn))
	    cntxt = NULL;
	  else
	    cntxt = default_save_context(ps_global->context_list);

	  if(!(exists=folder_exists(cntxt, cur_fn))
	    && (action
		|| (ps_global->context_list->use & CNTXT_INCMNG
		    && !folder_is_nick(cur_fn,FOLDERS(ps_global->context_list), 0)))){
	    if(cntxt && (action == 1)){
	      space -= 37;		/* for fixed part of prompt below */
	      w1 = MAX(1,MIN(strlen(cur_fn),space/2));
	      w2 = MIN(MAX(1,space-w1),strlen(cntxt->nickname));
	      w1 += MAX(0,space-w1-w2);
	      snprintf(prompt, sizeof(prompt),
		      _("Folder \"%s\" in <%s> doesn't exist. Create"),
		      short_str(cur_fn,nbuf1,sizeof(nbuf1),w1,MidDots),
		      short_str(cntxt->nickname,nbuf2,sizeof(nbuf2),w2,MidDots));
	      prompt[sizeof(prompt)-1] = '\0';
	    }
	    else if(cntxt && (action == 0)){
	      space -= 51;		/* for fixed part of prompt below */
	      w1 = MAX(1,MIN(strlen(cur_fn),space/2));
	      w2 = MIN(MAX(1,space-w1),strlen(cntxt->nickname));
	      w1 += MAX(0,space-w1-w2);
	      snprintf(prompt, sizeof(prompt),
		      _("Folder \"%s\" in <%s> doesn't exist. Exit and save anyway"),
		      short_str(cur_fn,nbuf1,sizeof(nbuf1),w1,MidDots),
		      short_str(cntxt->nickname,nbuf2,sizeof(nbuf2),w2,MidDots));
	      prompt[sizeof(prompt)-1] = '\0';
	    }
	    else if(!cntxt && (action == 1)){
	      space -= 31;		/* for fixed part of prompt below */
	      w1 = MAX(1,space);
	      snprintf(prompt, sizeof(prompt),
		      _("Folder \"%s\" doesn't exist. Create"),
		      short_str(cur_fn,nbuf1,sizeof(nbuf1),w1,MidDots));
	      prompt[sizeof(prompt)-1] = '\0';
	    }
	    else{ /*!cntxt && (action == 0) */
	      space -= 45;		/* for fixed part of prompt below */
	      w1 = MAX(1,space);
	      snprintf(prompt, sizeof(prompt),
		      _("Folder \"%s\" doesn't exist. Exit and save anyway"),
		      short_str(cur_fn,nbuf1,sizeof(nbuf1),w1,MidDots));
	      prompt[sizeof(prompt)-1] = '\0';
	    }

	    wt_res = want_to(prompt, 'n', 'x', NO_HELP, WT_NORM);
	    if(wt_res == 'y'){
	      if(action){
		if(context_create(cntxt, NULL, cur_fn)){
		  q_status_message(SM_ORDER,3,5,_("Folder created"));
		  maybe_add_to_incoming(cntxt, cur_fn);
		}
	      }
	      /* No message to notify of changes being saved, we can't */
	      /* assume that the role screen isn't exited yet          */
	      rv = 2;
	    }
	    else if(wt_res == 'n' && action){
	      rv = 2;
	      q_status_message(SM_ORDER,3,5,_("Folder not created"));
	    }
	    else{
	      q_status_message(SM_ORDER,3,5,_("Exit cancelled"));
	      rv = 0;
	      break;
	    }
	  }
	  else{
	      if(exists & FEX_ERROR){
	        if(ps_global->mm_log_error && ps_global->c_client_error)
		  q_status_message(SM_ORDER,3,5,ps_global->c_client_error);
		else
		  q_status_message1(SM_ORDER,3,5,"\"%s\": Trouble checking for folder existence", cur_fn);
	      }

	      rv = 2;
	  }
	}
	else{ /* blank item in list of folders */
	  if(action && lst[i+1] == '\0')
	    q_status_message(SM_ORDER,3,5,_("Set a valid Filter Action before Exiting"));
	  else /* !action && lst[i+1] == '\0' */
	    q_status_message(SM_ORDER,3,5,_("Set a valid Specific Folder before Exiting"));
	  rv = 0;
	  break;
	}

	if(cur_fn && cur_fn != lst[i])
	  fs_give((void **) &cur_fn);
    }

    return(rv);
}


void
maybe_add_to_incoming(CONTEXT_S *cntxt, char *cur_fn)
{
    char      name[MAILTMPLEN], nname[32];
    char      nbuf1[MAX_SCREEN_COLS], nbuf2[MAX_SCREEN_COLS];
    char      prompt[MAX_SCREEN_COLS];
    char   ***alval;
    int       i, found, space, w1, w2;
    FOLDER_S *f;

    if(ps_global->context_list->use & CNTXT_INCMNG &&
       ((alval = ALVAL(&ps_global->vars[V_INCOMING_FOLDERS], Main)) != NULL)){
	(void)context_apply(name, cntxt, cur_fn, sizeof(name));
	/*
	 * Since the folder didn't exist it is very unlikely that it is
	 * in the incoming-folders list already, but we're just checking
	 * to be sure. We should really be canonicalizing both names
	 * before comparing, but...
	 */
	for(found = 0, i = 0; *alval && (*alval)[i] && !found; i++){
	    char *nickname, *folder;

	    get_pair((*alval)[i], &nickname, &folder, 0, 0);
	    if(folder && !strucmp((*alval)[i], folder))
	      found++;
	    
	    if(nickname)
	      fs_give((void **)&nickname);
	    if(folder)
	      fs_give((void **)&folder);
	}

	if(found)
	  return;
	
	space = MAXPROMPT;
	space -= 15;		/* for fixed part of prompt below */
	w2 = MAX(1,
		MIN(space/2,MIN(strlen(ps_global->context_list->nickname),20)));
	w1 = MAX(1,space - w2);
	snprintf(prompt, sizeof(prompt),
		"Add \"%s\" to %s list",
		short_str(name,nbuf1,sizeof(nbuf1),w1,MidDots),
		short_str(ps_global->context_list->nickname,nbuf2,sizeof(nbuf2),w2,MidDots));
	prompt[sizeof(prompt)-1] = '\0';
	if(want_to(prompt, 'n', 'x', NO_HELP, WT_NORM) == 'y'){
	    char *pp;

	    nname[0] = '\0';
	    space = MAXPROMPT;
	    space -= 25;
	    w1 = MAX(1, space);
	    snprintf(prompt, sizeof(prompt), "Nickname for folder \"%s\" : ",
		    short_str(name,nbuf1,sizeof(nbuf1),w1,MidDots));
	    prompt[sizeof(prompt)-1] = '\0';
	    while(1){
		int rc;
		int flags = OE_APPEND_CURRENT;

		rc = optionally_enter(nname, -FOOTER_ROWS(ps_global), 0,
				      sizeof(nname), prompt, NULL,
				      NO_HELP, &flags);
		removing_leading_and_trailing_white_space(nname);
		if(rc == 0 && *nname){
		    /* see if nickname already exists */
		    found = 0;
		    if(!strucmp(ps_global->inbox_name, nname))
		      found++;

		    for(i = 0;
			!found &&
			  i < folder_total(FOLDERS(ps_global->context_list));
			i++){
			FOLDER_S *f;

			f = folder_entry(i, FOLDERS(ps_global->context_list));
			if(!strucmp(FLDR_NAME(f), nname))
			  found++;
		    }

		    if(found){
			q_status_message1(SM_ORDER | SM_DING, 3, 5,
				      _("Nickname \"%s\" is already in use"),
					  nname);
			continue;
		    }

		    break;
		}
		else if(rc == 3)
		  q_status_message(SM_ORDER, 0, 3, _("No help yet."));
		else if(rc == 1){
		    q_status_message1(SM_ORDER, 0, 3,
		    _("Not adding nickname to %s list"),
		    ps_global->context_list->nickname);
		    return;
		}
	    }

	    pp = put_pair(nname, name);
	    f = new_folder(name, line_hash(pp));
	    f->nickname = cpystr(nname);
	    f->name_len = strlen(nname);
	    folder_insert(folder_total(FOLDERS(ps_global->context_list)), f,
			  FOLDERS(ps_global->context_list));
	    
	    if(!*alval){
		i = 0;
		*alval = (char **)fs_get(2 * sizeof(char *));
	    }
	    else{
		for(i = 0; (*alval)[i]; i++)
		  ;
		
		fs_resize((void **)alval, (i + 2) * sizeof(char *));
	    }

	    (*alval)[i]   = pp;
	    (*alval)[i+1] = NULL;
	    set_current_val(&ps_global->vars[V_INCOMING_FOLDERS], TRUE, TRUE);
	    write_pinerc(ps_global, ew, WRP_NONE);
	}
    }
}


int
role_filt_exitcheck(CONF_S **cl, unsigned int flags)
{
    int             rv, j, action;
    char          **to_folder = NULL, **spec_fldr = NULL;
    CONF_S         *ctmp;
    NAMEVAL_S      *f;
#define ACT_UNKNOWN		0
#define ACT_KILL		1
#define ACT_MOVE		2
#define ACT_MOVE_NOFOLDER	3
#define ACT_STATE		4

    /*
     * We have to locate the lines which define the Filter Action and
     * then check to see that it is set to something before allowing
     * user to Exit.
     */
    action = ACT_UNKNOWN;
    if(flags & CF_CHANGES && role_filt_ptr && PVAL(role_filt_ptr,ew)){
	for(j = 0; (f = filter_types(j)); j++)
	  if(!strucmp(PVAL(role_filt_ptr,ew), f->name))
	    break;
	
	switch(f ? f->value : -1){
	  case FILTER_KILL:
	    action = ACT_KILL;
	    break;

	  case FILTER_STATE:
	    action = ACT_STATE;
	    break;

	  case FILTER_FOLDER:
	    /*
	     * Check that the folder is set to something.
	     */

	    action = ACT_MOVE_NOFOLDER;
	    /* go to end of screen */
	    for(ctmp = (*cl);
		ctmp && ctmp->next;
		ctmp = next_confline(ctmp))
	      ;

	    /* back up to start of Filter Action */
	    for(;
		ctmp &&
		!(ctmp->flags & CF_STARTITEM && ctmp->var == role_filt_ptr);
		ctmp = prev_confline(ctmp))
	      ;
	    
	    /* skip back past NOSELECTs */
	    for(;
		ctmp && (ctmp->flags & CF_NOSELECT);
		ctmp = next_confline(ctmp))
	      ;

	    /* find line with new var (the Folder line) */
	    for(;
		ctmp && (ctmp->var == role_filt_ptr);
		ctmp = next_confline(ctmp))
	      ;
	    
	    /* ok, we're finally at the Folder line */
	    if(ctmp && ctmp->var && LVAL(ctmp->var,ew)){
		to_folder = copy_list_array(LVAL(ctmp->var,ew));
		if(to_folder && to_folder[0])
		  action = ACT_MOVE;
	    }

	    break;

	  default:
	    dprint((1,
		    "Can't happen, role_filt_ptr set to %s\n",
		    PVAL(role_filt_ptr,ew) ? PVAL(role_filt_ptr,ew) : "?"));
	    break;
	}
    }

    if(flags & CF_CHANGES){
	switch(want_to((action == ACT_KILL)
	   ? _("Commit changes (\"Yes\" means matching messages will be deleted)")
	   : EXIT_PMT, 'y', 'x', h_config_undo, WT_FLUSH_IN)){
	  case 'y':
	    switch(action){
	      case ACT_KILL:
		if((spec_fldr = get_role_specific_folder(cl)) != NULL){
		  rv = check_role_folders(spec_fldr, 0);
		  free_list_array(&spec_fldr);
		  if(rv == 2)
		    q_status_message(SM_ORDER,0,3,_("Ok, messages matching that Pattern will be deleted"));
		}
		else{
		  q_status_message(SM_ORDER, 0, 3,
				   _("Ok, messages matching that Pattern will be deleted"));
		  rv = 2;
		}
		break;

	      case ACT_MOVE:
		if((spec_fldr = get_role_specific_folder(cl)) != NULL){
		  rv = check_role_folders(spec_fldr, 0);
		  free_list_array(&spec_fldr);
		  if(to_folder && rv == 2)
		    rv = check_role_folders(to_folder, 1);  
		}
		else
		  rv = check_role_folders(to_folder, 1);  

		break;

	      case ACT_MOVE_NOFOLDER:
		rv = 0;
		q_status_message(SM_ORDER, 3, 5,
				 _("Set a valid Filter Action before Exiting"));
		break;

	      case ACT_STATE:
		if((spec_fldr = get_role_specific_folder(cl)) != NULL){
		  rv = check_role_folders(spec_fldr, 0);
		  free_list_array(&spec_fldr);
		}
		else
		  rv = 2;

		break;

	      default:
		rv = 2;
		dprint((1,
		    "This can't happen, role_filt_ptr or to_folder not set\n"));
		break;
	    }

	    break;

	  case 'n':
	    q_status_message(SM_ORDER,3,5,_("No filter changes saved"));
	    rv = 10;
	    break;

	  case 'x':  /* ^C */
	  default :
	    q_status_message(SM_ORDER,3,5,_("Changes not yet saved"));
	    rv = 0;
	    break;
	}
    }
    else
      rv = 2;

    if(to_folder)
      free_list_array(&to_folder);

    return(rv);
}


/*
 * Don't allow exit unless user has set the action to something.
 */
int
role_filt_text_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int             rv;

    switch(cmd){
      case MC_EXIT:
	rv = role_filt_exitcheck(cl, flags);
	break;

      default:
	rv = role_text_tool(ps, cmd, cl, flags);
	break;
    }

    return(rv);
}


int
role_filt_addhdr_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int rv;

    switch(cmd){
      case MC_EXIT:
	rv = role_filt_exitcheck(cl, flags);
	break;

      default:
	rv = role_addhdr_tool(ps, cmd, cl, flags);
	break;
    }

    return rv;
}

int
role_addhdr_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int rv;

    switch(cmd){
      case MC_ADDHDR:
      case MC_EXIT:
	rv = role_text_tool(ps, cmd, cl, flags);
	break;

      default:
	rv = -1;
	break;
    }

    return rv;
}

/*
 * Don't allow exit unless user has set the action to something.
 */
int
role_filt_radiobutton_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int             rv;

    switch(cmd){
      case MC_EXIT:
	rv = role_filt_exitcheck(cl, flags);
	break;

      default:
	rv = role_radiobutton_tool(ps, cmd, cl, flags);
	break;
    }

    return(rv);
}


/*
 * simple radio-button style variable handler
 */
int
role_radiobutton_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int	       rv = 0, i;
    CONF_S    *ctmp, *spec_ctmp = NULL;
    NAMEVAL_S *rule;
    char     **apval;

    switch(cmd){
      case MC_CHOICE :				/* set/unset feature */

	/* back up to first line */
	for(ctmp = (*cl);
	    ctmp && ctmp->prev && !(ctmp->prev->flags & CF_NOSELECT);
	    ctmp = prev_confline(ctmp))
	  ;
	
	for(i = 0; ctmp && (!(ctmp->flags & CF_NOSELECT)
			    || (*cl)->var == role_fldr_ptr 
			    || (*cl)->var == role_afrom_ptr
			    || (*cl)->var == role_filt_ptr);
	    ctmp = next_confline(ctmp), i++){
	    if(((*cl)->var == role_fldr_ptr) ||
	       ((*cl)->var == role_afrom_ptr) ||
	       ((*cl)->var == role_filt_ptr)){
		if((((*cl)->var == role_fldr_ptr) && !pat_fldr_types(i))
		   || (((*cl)->var == role_afrom_ptr)
		       && !inabook_fldr_types(i))
		   || (((*cl)->var == role_filt_ptr) && !filter_types(i))){
		    spec_ctmp = ctmp;
		    break;
		}
	    }

	    ctmp->value[1] = ' ';
	}

	/* turn on current value */
	(*cl)->value[1] = R_SELD;

	if((*cl)->var == role_fldr_ptr){
	    for(ctmp = spec_ctmp;
		ctmp && ctmp->varnamep == spec_ctmp;
		ctmp = next_confline(ctmp))
	      if((*cl)->varmem == FLDR_SPECIFIC)
		ctmp->flags &= ~CF_NOSELECT;
	      else
		ctmp->flags |= CF_NOSELECT;

	    rule = pat_fldr_types((*cl)->varmem);
	}
	else if((*cl)->var == role_afrom_ptr){
	    for(ctmp = spec_ctmp;
		ctmp && ctmp->varnamep == spec_ctmp;
		ctmp = next_confline(ctmp))
	      if(((*cl)->varmem == IAB_SPEC_YES)
	         || ((*cl)->varmem == IAB_SPEC_NO))
	        ctmp->flags &= ~CF_NOSELECT;
	      else
	        ctmp->flags |= CF_NOSELECT;

	    rule = inabook_fldr_types((*cl)->varmem);
	}
	else if((*cl)->var == role_filt_ptr){
	    for(ctmp = spec_ctmp;
		ctmp && ctmp->varnamep == spec_ctmp;
		ctmp = next_confline(ctmp))
	      if((*cl)->varmem == FILTER_FOLDER)
		ctmp->flags &= ~CF_NOSELECT;
	      else
		ctmp->flags |= CF_NOSELECT;

	    rule = filter_types((*cl)->varmem);
	}
	else if((*cl)->var == role_forw_ptr)
	  rule = role_forw_types((*cl)->varmem);
	else if((*cl)->var == role_repl_ptr)
	  rule = role_repl_types((*cl)->varmem);
	else if((*cl)->var == role_status1_ptr ||
	        (*cl)->var == role_status2_ptr ||
	        (*cl)->var == role_status3_ptr ||
	        (*cl)->var == role_status4_ptr ||
	        (*cl)->var == role_status5_ptr ||
	        (*cl)->var == role_status6_ptr ||
	        (*cl)->var == role_status7_ptr ||
	        (*cl)->var == role_status8_ptr)
	  rule = role_status_types((*cl)->varmem);
	else if((*cl)->var == msg_state1_ptr ||
	        (*cl)->var == msg_state2_ptr ||
	        (*cl)->var == msg_state3_ptr ||
	        (*cl)->var == msg_state4_ptr)
	  rule = msg_state_types((*cl)->varmem);
	else
	  rule = role_comp_types((*cl)->varmem);

	apval = APVAL((*cl)->var, ew);
	if(apval && *apval)
	  fs_give((void **)apval);

	if(apval)
	  *apval = cpystr(rule->name);

	ps->mangled_body = 1;	/* BUG: redraw it all for now? */
	rv = 1;

	break;

      case MC_EXIT:				/* exit */
	rv = role_text_tool(ps, cmd, cl, flags);
	break;

      default :
	rv = -1;
	break;
    }

    return(rv);
}


int
role_sort_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int	       rv = 0, i;
    CONF_S    *ctmp;
    char     **apval;
    SortOrder  def_sort;
    int        def_sort_rev;

    apval = APVAL((*cl)->var, ew);

    switch(cmd){
      case MC_CHOICE :				/* set/unset feature */

	if((*cl)->varmem >= 0){
	    def_sort_rev = (*cl)->varmem >= (short) EndofList;
	    def_sort = (SortOrder)((*cl)->varmem - (def_sort_rev * EndofList));

	    snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%s%s", sort_name(def_sort),
		    (def_sort_rev) ? "/Reverse" : "");
	    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	}

	if(apval){
	    if(*apval)
	      fs_give((void **)apval);
	    
	    if((*cl)->varmem >= 0)
	      *apval = cpystr(tmp_20k_buf);
	}

	/* back up to first line */
	for(ctmp = (*cl);
	    ctmp && ctmp->prev && !(ctmp->prev->flags & CF_NOSELECT);
	    ctmp = prev_confline(ctmp))
	  ;
	
	/* turn off all values */
	for(i = 0;
	    ctmp && !(ctmp->flags & CF_NOSELECT);
	    ctmp = next_confline(ctmp), i++)
	  ctmp->value[1] = ' ';

	/* turn on current value */
	(*cl)->value[1] = R_SELD;

	ps->mangled_body = 1;	/* BUG: redraw it all for now? */
	rv = 1;

	break;

      case MC_EXIT:				/* exit */
	rv = role_text_tool(ps, cmd, cl, flags);
	break;

      default :
	rv = -1;
	break;
    }

    return(rv);
}

/*
 * Return an allocated list of the Specific Folder list for 
 * roles, or NULL if Current Folder type is not set to 
 * to Specific Folder
 *
 * WARNING, the method used in obtaining the specific folder is
 * VERY dependent on the order in which it is presented on the 
 * screen.  If the Current Folder radio buttons were changed,
 * this function would probably need to be fixed accordingly.
 */
char **
get_role_specific_folder(CONF_S **cl)
{
    CONF_S   *ctmp;

    /* go to the first line */
    for(ctmp = *cl;
	ctmp && ctmp->prev;
	ctmp = prev_confline(ctmp))
      ;
  
    /* go to the current folder radio button list */
    while(ctmp && ctmp->var != role_fldr_ptr)
      ctmp = next_confline(ctmp);

    /* go to the specific folder button (caution) */
    while(ctmp && ctmp->varmem != FLDR_SPECIFIC)
      ctmp = next_confline(ctmp);

    /* check if selected (assumption of format "(*)" */
    if(ctmp && ctmp->value[1] == R_SELD){
	/* go to next line, the start of the list */
	ctmp = next_confline(ctmp);
	if(LVAL(ctmp->var, ew))
	  return copy_list_array(LVAL(ctmp->var, ew));
	else{
	    char **ltmp;

	    /*
	     * Need to allocate empty string so as not to confuse it
	     * with the possibility that Specific Folder is not selected.
	     */
	    ltmp    = (char **) fs_get(2 * sizeof(char *));
	    ltmp[0] = cpystr("");
	    ltmp[1] = NULL;
	    return(ltmp);
	}
    }
    else
      return NULL;
}


/*
 */
int
role_litsig_text_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int   rv;

    switch(cmd){
      case MC_ADD :
      case MC_EDIT :
	rv = litsig_text_tool(ps, cmd, cl, flags);
	if(rv)
	  calculate_inick_stuff(ps);

	break;

      default :
	rv = role_text_tool(ps, cmd, cl, flags);
	break;
    }

    return(rv);
}


/*
 */
int
role_cstm_text_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int   rv;

    switch(cmd){
      case MC_EXIT :
	rv = role_text_tool(ps, cmd, cl, flags);
	break;

      default :
	rv = text_tool(ps, cmd, cl, flags);
	if(rv == 1 && (*cl)->var){
	    char **lval;

	    lval = LVAL((*cl)->var, ew);
	    if(lval && lval[(*cl)->varmem] &&
	       ((!struncmp(lval[(*cl)->varmem],"from",4) &&
		 (lval[(*cl)->varmem][4] == ':' ||
		  lval[(*cl)->varmem][4] == '\0')) ||
	        (!struncmp(lval[(*cl)->varmem],"reply-to",8) &&
		 (lval[(*cl)->varmem][8] == ':' ||
		  lval[(*cl)->varmem][8] == '\0'))))
	      q_status_message1(SM_ORDER|SM_DING, 5, 7,
				"Use \"Set %s\" instead, Change ignored",
				!struncmp(lval[(*cl)->varmem],"from",4)
				    ? "From" : "Reply-To");
	}

	break;
    }

    return(rv);
}


/*
 */
int
role_text_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    OPT_SCREEN_S *saved_screen;
    int   rv = -1, oeflags, len, sig, r, i, cancel = 0;
    char *file, *err, title[20], *newfile, *lc, *addr, *fldr = NULL, *tmpfldr;
    char  dir2[MAXPATH+1], pdir[MAXPATH+1], *p;
    char  full_filename[MAXPATH+1], filename[MAXPATH+1];
    char  tmp[MAXPATH+1], **spec_fldr, **apval;
    EARB_S *earb, *ea, *eaprev;
    CONF_S *ctmp, *ctmpb, *newcp, *ctend;
    HelpType help;

    switch(cmd){
      case MC_EXIT :
	if(flags & CF_CHANGES){
	  switch(want_to(EXIT_PMT, 'y', 'x', h_config_role_undo, WT_FLUSH_IN)){
	    case 'y':
	      if((spec_fldr = get_role_specific_folder(cl)) != NULL){
		rv = check_role_folders(spec_fldr, 0);
		free_list_array(&spec_fldr);
	      }
	      else 
		rv = 2;
	      break;

	    case 'n':
	      q_status_message(SM_ORDER,3,5,_("No changes saved"));
	      rv = 10;
	      break;

	    case 'x':  /* ^C */
	      q_status_message(SM_ORDER,3,5,_("Changes not yet saved"));
	      rv = 0;
	      break;
	  }
	}
	else
	  rv = 2;

	break;

      case MC_NOT :		/* toggle between !matching and matching */
	ctmp = (*cl)->varnamep;
	if(ctmp->varname && ctmp->var && ctmp->var->name){
	    if(!strncmp(ctmp->varname, NOT, NOTLEN) &&
	       !strncmp(ctmp->var->name, NOT, NOTLEN)){
		rplstr(ctmp->var->name, strlen(ctmp->var->name)+1, NOTLEN, "");
		rplstr(ctmp->varname, strlen(ctmp->varname)+1, NOTLEN, "");
		strncpy(ctmp->varname+strlen(ctmp->varname)-1,
		       repeat_char(NOTLEN, ' '), NOTLEN+1);
		strncat(ctmp->varname, "=", NOTLEN);
	    }
	    else{
		rplstr(ctmp->var->name, strlen(ctmp->var->name)+NOTLEN+1, 0, NOT);
		strncpy(ctmp->varname+strlen(ctmp->varname)-1-NOTLEN, "=", NOTLEN);
		rplstr(ctmp->varname, strlen(ctmp->varname)+NOTLEN+1, 0, NOT);
	    }

	    rv = 1;
	}

	break;

      case MC_CHOICE :				/* Choose a file */
	/*
	 * In signature_path we read signature files relative to the pinerc
	 * dir, so if user selects one that is in there we'll make it
	 * relative instead of absolute, so it looks nicer.
	 */
	pdir[0] = '\0';
	if(ps_global->VAR_OPER_DIR){
	    strncpy(pdir, ps_global->VAR_OPER_DIR, MAXPATH);
	    pdir[MAXPATH] = '\0';
	    len = strlen(pdir) + 1;
	}
	else if((lc = last_cmpnt(ps_global->pinerc)) != NULL){
	    strncpy(pdir, ps_global->pinerc, MIN(MAXPATH,lc-ps_global->pinerc));
	    pdir[MIN(MAXPATH, lc-ps_global->pinerc)] = '\0';
	    len = strlen(pdir);
	}

	strncpy(title, "CHOOSE A", 15);
	strncpy(dir2, pdir, MAXPATH);

	filename[0] = '\0';
	build_path(full_filename, dir2, filename, sizeof(full_filename));

	r = file_lister(title, dir2, sizeof(dir2), filename, sizeof(filename), TRUE, FB_READ);
	ps->mangled_screen = 1;

	if(r == 1){
	    build_path(full_filename, dir2, filename, sizeof(full_filename));
	    removing_leading_and_trailing_white_space(full_filename);
	    if(!strncmp(full_filename, pdir, strlen(pdir)))
	      newfile = cpystr(full_filename + len); 
	    else
	      newfile = cpystr(full_filename);

	    apval = APVAL((*cl)->var, ew);
	    if(apval && *apval)
	      fs_give((void **)apval);
	    
	    if(apval)
	      *apval = newfile;

	    if((*cl)->value)
	      fs_give((void **)&((*cl)->value));

	    (*cl)->value = pretty_value(ps, *cl);
	    rv = 1;
	}
	else
	  rv = 0;

	break;

      case MC_CHOICEB :		/* Choose Addresses, no full names */
	addr = addr_book_multaddr_nf();
	ps->mangled_screen = 1;
	if(addr && (*cl)->var && (*cl)->var->is_list){
	    char **ltmp, *tmp;
	    int    i;

	    i = 0;
	    for(tmp = addr; *tmp; tmp++)
	      if(*tmp == ',')
		i++;	/* conservative count of ,'s */

	    ltmp = parse_list(addr, i + 1, PL_COMMAQUOTE, NULL);
	    fs_give((void **) &addr);

	    if(ltmp && ltmp[0])
	      config_add_list(ps, cl, ltmp, NULL, 0);

	    if(ltmp)
	      fs_give((void **) &ltmp);
	    
	    if((*cl)->value)
	      fs_give((void **)&((*cl)->value));

	    (*cl)->value = pretty_value(ps, *cl);
	    rv = 1;
	}
	else
	  rv = 0;

	break;

      case MC_CHOICEC :		/* Choose an Address, no full name */
	addr = addr_book_oneaddr();
	ps->mangled_screen = 1;
	if(addr){
	    apval = APVAL((*cl)->var, ew);
	    if(apval && *apval)			/* replace current value */
	      fs_give((void **)apval);
	    
	    if(apval)
	      *apval = addr;

	    if((*cl)->value)
	      fs_give((void **)&((*cl)->value));

	    (*cl)->value = pretty_value(ps, *cl);
	    rv = 1;
	}
	else
	  rv = 0;

	break;

      case MC_CHOICED :				/* Choose a Folder */
      case MC_CHOICEE :
	saved_screen = opt_screen;
	if(cmd == MC_CHOICED)
	  tmpfldr = folder_for_config(FOR_PATTERN);
	else
	  tmpfldr = folder_for_config(0);
	
	if(tmpfldr){
	    fldr = add_comma_escapes(tmpfldr);
	    fs_give((void**) &tmpfldr);
	}

	opt_screen = saved_screen;

	ps->mangled_screen = 1;
	if(fldr && *fldr && (*cl)->var && (*cl)->var->is_list){
	    char **ltmp;

	    ltmp    = (char **) fs_get(2 * sizeof(char *));
	    ltmp[0] = fldr;
	    ltmp[1] = NULL;
	    fldr    = NULL;

	    if(ltmp && ltmp[0])
	      config_add_list(ps, cl, ltmp, NULL, 0);

	    if(ltmp)
	      fs_give((void **) &ltmp);
	    
	    if((*cl)->value)
	      fs_give((void **) &((*cl)->value));

	    (*cl)->value = pretty_value(ps, *cl);
	    rv = 1;
	}
	else if(fldr && *fldr && (*cl)->var && !(*cl)->var->is_list){
	    apval = APVAL((*cl)->var, ew);
	    if(apval && *apval)			/* replace current value */
	      fs_give((void **)apval);

	    if(apval){
		*apval = fldr;
		fldr   = NULL;
	    }

	    if((*cl)->value)
	      fs_give((void **) &((*cl)->value));

	    (*cl)->value = pretty_value(ps, *cl);
	    rv = 1;
	}
	else
	  rv = 0;

	if(fldr)
	  fs_give((void **) &fldr);

	break;

      case MC_EDITFILE :
	file = ((*cl)->var && PVAL((*cl)->var, ew))
		    ? cpystr(PVAL((*cl)->var, ew)) : NULL;
	if(file)
	  removing_leading_and_trailing_white_space(file);

	sig = (srchstr((*cl)->varname, "signature") != NULL);
	if(!file || !*file){
	    err = (char *)fs_get(100 * sizeof(char));
	    snprintf(err, 100, "No %s file defined. First define a file name.",
		    sig ? "signature" : "template");
	    err[100-1] = '\0';
	}
	else{
	    if(file[len=(strlen(file)-1)] == '|')
	      file[len] = '\0';

	    snprintf(title, sizeof(title), "%s EDITOR", sig ? "SIGNATURE" : "TEMPLATE");
	    title[sizeof(title)-1] = '\0';
	    err = signature_edit(file, title);
	}

	fs_give((void **)&file);
	if(err){
	    q_status_message1(SM_ORDER, 3, 5, "%s", err);
	    fs_give((void **)&err);
	}

	rv = 0;
	ps->mangled_screen = 1;
	break;

      /* Add an arbitrary header to this role */
      case MC_ADDHDR :
	rv = 0;
	/* make earb point to last one */
	for(earb = *(*cl)->d.earb; earb && earb->next; earb = earb->next)
	  ;

	/* Add new one to end of list */
	ea = (EARB_S *)fs_get(sizeof(*ea));
	memset((void *)ea, 0, sizeof(*ea));
	ea->v = (struct variable *)fs_get(sizeof(struct variable));
	memset((void *)ea->v, 0, sizeof(struct variable));
	ea->a = (ARBHDR_S *)fs_get(sizeof(ARBHDR_S));
	memset((void *)ea->a, 0, sizeof(ARBHDR_S));

	/* get new header field name */
	help = NO_HELP;
	tmp[0] = '\0';
	while(1){
	    i = optionally_enter(tmp, -FOOTER_ROWS(ps), 0, sizeof(tmp),
			     _("Enter the name of the header field to be added: "),
				 NULL, help, NULL);
	    if(i == 0)
	      break;
	    else if(i == 1){
		cmd_cancelled("eXtraHdr");
		cancel = 1;
		break;
	    }
	    else if(i == 3){
		help = help == NO_HELP ? h_config_add_pat_hdr : NO_HELP;
		continue;
	    }
	    else
	      break;
	}

	ps->mangled_footer = 1;

	removing_leading_and_trailing_white_space(tmp);
	if(tmp[strlen(tmp)-1] == ':')  /* remove trailing colon */
	  tmp[strlen(tmp)-1] = '\0';

	removing_trailing_white_space(tmp);

	if(cancel || !tmp[0])
	  break;

	tmp[0] = islower((unsigned char)tmp[0]) ? toupper((unsigned char)tmp[0])
						: tmp[0];
	ea->a->field = cpystr(tmp);

	if(earb)
	  earb->next = ea;
	else
	  *((*cl)->d.earb) = ea;

	/* go to first line */
	for(ctmp = *cl; prev_confline(ctmp); ctmp = prev_confline(ctmp))
	  ;

	/*
	 * Go to the Add Extra Headers line. We will put the
	 * new header before this line.
	 */
	for(; ctmp; ctmp = next_confline(ctmp))
	  if(ctmp->value && !strcmp(ctmp->value, ADDXHDRS))
	    break;

	/* move back one */
	if(ctmp)
	  ctmp = prev_confline(ctmp);
	
	/*
	 * Add a new line after this point, which is after the last
	 * extra header (if any) or after the Participant pattern, and
	 * before the Add Extra Headers placeholder line.
	 */
	p = (char *) fs_get(strlen(tmp) + strlen(" pattern") + 1);
	snprintf(p, strlen(tmp) + strlen(" pattern") + 1, "%s pattern", tmp);
	p[strlen(tmp) + strlen(" pattern") + 1 - 1] = '\0';
	setup_dummy_pattern_var(ea->v, p, NULL);
	fs_give((void **) &p);

	/* find what indent should be */
	if(ctmp && ctmp->varnamep && ctmp->varnamep->varname)
	  i = MIN(MAX(utf8_width(ctmp->varnamep->varname) + 1, 3), 200);
	else
	  i = 20;
	
	setup_role_pat(ps, &ctmp, ea->v, h_config_role_arbpat,
		       ARB_HELP, &config_role_xtrahdr_keymenu,
		       ctmp->prev->tool, ctmp->prev->d.earb, i);

	/*
	 * move current line to new line
	 */

	newcp = ctmp;

	/* check if new line comes before the top of the screen */
	ctmpb = (opt_screen && opt_screen->top_line)
		    ? opt_screen->top_line->prev : NULL;
	for(; ctmpb; ctmpb = prev_confline(ctmpb))
	  if(ctmpb == newcp)
	    break;
	/*
	 * Keep the right lines visible.
	 * The if triggers if the new line is off the top of the screen, and
	 * it makes the new line be the top line.
	 * The else counts how many lines down the screen the new line is.
	 */

	i = 0;
	if(ctmpb == newcp)
	  opt_screen->top_line = newcp;
	else{
	    for(ctmp = opt_screen->top_line; ctmp && ctmp != newcp;
		i++, ctmp = next_confline(ctmp))
	      ;
	}

	if(i >= BODY_LINES(ps)){	/* new line is off screen */
	    /* move top line down this far */
	    i = i + 1 - BODY_LINES(ps);
	    for(ctmp = opt_screen->top_line;
		i > 0;
		i--, ctmp = next_confline(ctmp))
	      ;

	    opt_screen->top_line = ctmp;
	}

	*cl = newcp;

	ps->mangled_screen = 1;
	rv = 1;
	break;

      /* Delete an arbitrary header from this role */
      case MC_DELHDR :
	/*
	 * Find this one in earb list. We don't have a good way to locate
	 * it so we match the ea->v->name with ctmp->varname.
	 */
	rv = 0;
	eaprev = NULL;
	for(ea = *(*cl)->d.earb; ea; ea = ea->next){
	    if((*cl)->varnamep && (*cl)->varnamep->varname
	       && ea->v && ea->v->name
	       && !strncmp((*cl)->varnamep->varname,
			   ea->v->name, strlen(ea->v->name)))
	      break;
	    
	    eaprev = ea;
	}
	
	snprintf(tmp, sizeof(tmp), _("Really remove \"%s\" pattern from this rule"),
		(ea && ea->a && ea->a->field) ? ea->a->field : "this");
	tmp[sizeof(tmp)-1] = '\0';
	if(want_to(tmp, 'y', 'n', NO_HELP, WT_NORM) != 'y'){
	    cmd_cancelled("RemoveHdr");
	    return(rv);
	}

	/* delete the earb element from the list */
	if(ea){
	    if(eaprev)
	      eaprev->next = ea->next;
	    else
	      *(*cl)->d.earb = ea->next;
	    
	    ea->next = NULL;
	    free_earb(&ea);
	}

	/* remember start of deleted header */
	ctmp = (*cl && (*cl)->varnamep) ? (*cl)->varnamep : NULL;

	/* and end of deleted header */
	for(ctend = *cl; ctend; ctend = next_confline(ctend))
	  if(!ctend->next || ctend->next->varnamep != ctmp)
	    break;

	/* check if top line is one we're deleting */
	for(ctmpb = ctmp; ctmpb; ctmpb = next_confline(ctmpb)){
	    if(ctmpb == opt_screen->top_line)
	      break;
	    
	    if(ctmpb == (*cl))
	      break;
	}

	if(ctmpb == opt_screen->top_line)
	  opt_screen->top_line = ctend ? ctend->next : NULL;

	/* move current line after this header */
	*cl = ctend ? ctend->next : NULL;

	/* remove deleted header lines */
	if(ctmp && ctend){
	    /* remove from linked list */
	    if(ctmp->prev)
	      ctmp->prev->next = ctend->next;
	    
	    if(ctend->next)
	      ctend->next->prev = ctmp->prev;
	    
	    /* free memory */
	    ctmp->prev = ctend->next = NULL;
	    free_conflines(&ctmp);
	}
	
	ps->mangled_body = 1;
	rv = 1;
	break;

      default :
	if(((*cl)->var == scorei_pat_global_ptr
	    || (*cl)->var == age_pat_global_ptr
	    || (*cl)->var == size_pat_global_ptr
	    || (*cl)->var == cati_global_ptr)
	   &&
	   (cmd == MC_EDIT || (cmd == MC_ADD && !PVAL((*cl)->var, ew)))){
	    char prompt[60];

	    rv = 0;
	    snprintf(prompt, sizeof(prompt), "%s the interval : ",
		    PVAL((*cl)->var, ew) ? "Change" : "Enter");
	    prompt[sizeof(prompt)-1] = '\0';

	    ps->mangled_footer = 1;
	    help = NO_HELP;
	    tmp[0] = '\0';
	    snprintf(tmp, sizeof(tmp),
		    "%s", PVAL((*cl)->var, ew) ? PVAL((*cl)->var, ew) : "");
	    tmp[sizeof(tmp)-1] = '\0';
	    while(1){
		oeflags = OE_APPEND_CURRENT;
		i = optionally_enter(tmp, -FOOTER_ROWS(ps), 0, sizeof(tmp),
				     prompt, NULL, help, &oeflags);
		if(i == 0){
		    rv = ps->mangled_body = 1;
		    apval = APVAL((*cl)->var, ew);
		    if(apval && *apval)
		      fs_give((void **)apval);
		    
		    if(apval && tmp[0])
		      *apval = cpystr(tmp);

		    fix_side_effects(ps, (*cl)->var, 0);
		    if((*cl)->value)
		      fs_give((void **)&(*cl)->value);
		    
		    (*cl)->value = pretty_value(ps, *cl);
		}
		else if(i == 1)
		  cmd_cancelled(cmd == MC_ADD ? "Add" : "Change");
		else if(i == 3){
		    help = help == NO_HELP ? h_config_edit_scorei : NO_HELP;
		    continue;
		}
		else if(i == 4)
		  continue;

		break;
	    }
	}
	else{
	    if(cmd == MC_ADD && (*cl)->var && !(*cl)->var->is_list)
	      cmd = MC_EDIT;

	    rv = text_toolit(ps, cmd, cl, flags, 1);

	    /* make sure the earb pointers are set */
	    for(ctmp = (*cl)->varnamep;
		ctmp->next && ctmp->next->var == ctmp->var;
		ctmp = next_confline(ctmp))
	      ctmp->next->d.earb = ctmp->d.earb;
	}

	break;
    }

    /*
     * If the inherit nickname changed, we have to re-calculate the
     * global_vals and values for the action variables.
     * We may have to do the same if literal sig changed, too.
     */
    if(rv)
      calculate_inick_stuff(ps);

    return(rv);
}


/*
 */
int
role_text_tool_inick(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int   rv = -1;
    char **apval;

    switch(cmd){
      case MC_EXIT :
	rv = role_text_tool(ps, cmd, cl, flags);
	break;

      case MC_CHOICE :		/* Choose a role nickname */
	{void (*prev_screen)(struct pine *) = ps->prev_screen,
	      (*redraw)(void) = ps->redrawer;
	 OPT_SCREEN_S *saved_screen;
	 ACTION_S     *role;

	ps->redrawer = NULL;
	ps->next_screen = SCREEN_FUN_NULL;

	saved_screen = opt_screen;
	if(role_select_screen(ps, &role, 0) == 0){
	    apval = APVAL((*cl)->var, ew);
	    if(apval && *apval)
	      fs_give((void **)apval);
	    
	    if(apval)
	      *apval = (role && role->nick) ? cpystr(role->nick) : NULL;

	    if((*cl)->value)
	      fs_give((void **)&((*cl)->value));

	    (*cl)->value = pretty_value(ps, *cl);
	    rv = 1;
	}
	else{
	    ps->next_screen = prev_screen;
	    ps->redrawer = redraw;
	    rv = 0;
	}

	opt_screen = saved_screen;
	}

	ps->mangled_screen = 1;
	break;

      case MC_EDIT :
      case MC_ADD :
      case MC_DELETE :
	rv = text_tool(ps, cmd, cl, flags);
	ps->mangled_screen = 1;
	break;

      default :
	rv = text_tool(ps, cmd, cl, flags);
	break;
    }

    /*
     * If the inherit nickname changed, we have to re-calculate the
     * global_vals and values for the action variables.
     * We may have to do the same if literal sig changed, too.
     */
    if(rv)
      calculate_inick_stuff(ps);

    return(rv);
}


/*
 */
int
role_text_tool_kword(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int    i, j, rv = -1;
    char **lval;

    switch(cmd){
      case MC_CHOICE :		/* Choose keywords from list and add them */
	{void (*prev_screen)(struct pine *) = ps->prev_screen,
	      (*redraw)(void) = ps->redrawer;
	 OPT_SCREEN_S *saved_screen;
	 char         *esc;
	 char        **kw;

	ps->redrawer = NULL;
	ps->next_screen = SCREEN_FUN_NULL;

	saved_screen = opt_screen;

	if((kw=choose_list_of_keywords()) != NULL){
	    for(i = 0; kw[i]; i++){
		esc = add_roletake_escapes(kw[i]);
		fs_give((void **) &kw[i]);
		kw[i] = esc;
	    }

	    /* eliminate duplicates before the add */
	    lval = LVAL((*cl)->var, ew);
	    if(lval && *lval){
		for(i = 0; kw[i]; ){
		    /* if kw[i] is a dup, eliminate it */
		    for(j = 0; lval[j]; j++)
		      if(!strcmp(kw[i], lval[j]))
			break;

		    if(lval[j]){		/* it is a dup */
			for(j = i; kw[j]; j++)
			  kw[j] = kw[j+1];
		    }
		    else
		      i++;
		}
	    }

	    if(kw[0])
	      config_add_list(ps, cl, kw, NULL, 0);
	    
	    fs_give((void **) &kw);

	    if((*cl)->value)
	      fs_give((void **) &((*cl)->value));

	    (*cl)->value = pretty_value(ps, *cl);
	    rv = 1;
	}
	else{
	    ps->next_screen = prev_screen;
	    ps->redrawer = redraw;
	    rv = 0;
	}

	opt_screen = saved_screen;
	}

	ps->mangled_screen = 1;
	break;

      case MC_EDIT :
      case MC_ADD :
      case MC_DELETE :
      case MC_NOT :
	rv = role_text_tool(ps, cmd, cl, flags);
	ps->mangled_screen = 1;
	break;

      case MC_EXIT :
      default :
	rv = role_text_tool(ps, cmd, cl, flags);
	break;
    }

    return(rv);
}


/*
 */
int
role_text_tool_charset(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int    i, j, rv = -1;
    char **lval;

    switch(cmd){
      case MC_CHOICE :		/* Choose charsets from list and add them */
	{void (*prev_screen)(struct pine *) = ps->prev_screen,
	      (*redraw)(void) = ps->redrawer;
	 OPT_SCREEN_S *saved_screen;
	 char         *esc;
	 char        **kw;

	ps->redrawer = NULL;
	ps->next_screen = SCREEN_FUN_NULL;

	saved_screen = opt_screen;

	if((kw=choose_list_of_charsets()) != NULL){
	    for(i = 0; kw[i]; i++){
		esc = add_roletake_escapes(kw[i]);
		fs_give((void **) &kw[i]);
		kw[i] = esc;
	    }

	    /* eliminate duplicates before the add */
	    lval = LVAL((*cl)->var, ew);
	    if(lval && *lval){
		for(i = 0; kw[i]; ){
		    /* if kw[i] is a dup, eliminate it */
		    for(j = 0; lval[j]; j++)
		      if(!strcmp(kw[i], lval[j]))
			break;

		    if(lval[j]){		/* it is a dup */
			for(j = i; kw[j]; j++)
			  kw[j] = kw[j+1];
		    }
		    else
		      i++;
		}
	    }

	    if(kw[0])
	      config_add_list(ps, cl, kw, NULL, 0);
	    
	    fs_give((void **) &kw);

	    if((*cl)->value)
	      fs_give((void **) &((*cl)->value));

	    (*cl)->value = pretty_value(ps, *cl);
	    rv = 1;
	}
	else{
	    ps->next_screen = prev_screen;
	    ps->redrawer = redraw;
	    rv = 0;
	}

	opt_screen = saved_screen;
	}

	ps->mangled_screen = 1;
	break;

      case MC_EDIT :
      case MC_ADD :
      case MC_DELETE :
      case MC_NOT :
	rv = role_text_tool(ps, cmd, cl, flags);
	ps->mangled_screen = 1;
	break;

      case MC_EXIT :
      default :
	rv = role_text_tool(ps, cmd, cl, flags);
	break;
    }

    return(rv);
}


/*
 * Choose an address book nickname
 */
int
role_text_tool_afrom(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int   rv = -1;

    switch(cmd){
      case MC_EXIT :
	rv = role_text_tool(ps, cmd, cl, flags);
	break;

      case MC_CHOICE :		/* Choose an addressbook */
	{OPT_SCREEN_S *saved_screen;
	 char *abook = NULL, *abookesc = NULL;

	ps->redrawer = NULL;
	ps->next_screen = SCREEN_FUN_NULL;
	saved_screen = opt_screen;

	abook = abook_select_screen(ps);
	if(abook){
	    abookesc = add_comma_escapes(abook);
	    fs_give((void**) &abook);
	}

	ps->mangled_screen = 1;
	if(abookesc && *abookesc && (*cl)->var && (*cl)->var->is_list){
	    char **ltmp;

	    ltmp     = (char **) fs_get(2 * sizeof(char *));
	    ltmp[0]  = abookesc;
	    ltmp[1]  = NULL;
	    abookesc = NULL;

	    if(ltmp && ltmp[0])
	      config_add_list(ps, cl, ltmp, NULL, 0);

	    if(ltmp)
	      fs_give((void **) &ltmp);
	    
	    if((*cl)->value)
	      fs_give((void **) &((*cl)->value));

	    (*cl)->value = pretty_value(ps, *cl);
	    rv = 1;
	}
	else
	  rv = 0;

	if(abookesc)
	  fs_give((void **) &abookesc);

	opt_screen = saved_screen;
	}

	ps->mangled_screen = 1;
	break;

      default :
	rv = text_tool(ps, cmd, cl, flags);
	ps->mangled_screen = 1;
	break;
    }

    return(rv);
}


/*
 * Args fmt -- a printf style fmt string with a single %s
 *      buf -- place to put result, assumed large enough (strlen(fmt)+11)
 *   rflags -- controls what goes in buf
 *
 * Returns -- pointer to buf
 */
char *
role_type_print(char *buf, size_t buflen, char *fmt, long int rflags)
{
#define CASE_MIXED	1
#define CASE_UPPER	2
#define CASE_LOWER	3
    int   cas = CASE_UPPER;
    int   prev_word_is_a = 0;
    char *q, *p;

    /* find %sRule to see what case */
    if((p = srchstr(fmt, "%srule")) != NULL){
	if(p[2] == 'R'){
	    if(p[3] == 'U')
	      cas = CASE_UPPER;
	    else
	      cas = CASE_MIXED;
	}
	else
	  cas = CASE_LOWER;
	
	if(p-3 >= fmt &&
	   p[-1] == SPACE &&
	   (p[-2] == 'a' || p[-2] == 'A')
	   && p[-3] == SPACE)
	  prev_word_is_a++;
    }

    if(cas == CASE_UPPER)
      q = (rflags & ROLE_DO_INCOLS) ? "INDEX COLOR " :
	   (rflags & ROLE_DO_FILTER) ? "FILTERING " :
	    (rflags & ROLE_DO_SCORES) ? "SCORING " :
	     (rflags & ROLE_DO_OTHER)  ? "OTHER " :
	      (rflags & ROLE_DO_SRCH)   ? "SEARCH " :
	       (rflags & ROLE_DO_ROLES)  ? "ROLE " : "";
    else if(cas == CASE_LOWER)
      q = (rflags & ROLE_DO_INCOLS) ? "index color " :
	   (rflags & ROLE_DO_FILTER) ? "filtering " :
	    (rflags & ROLE_DO_SCORES) ? "scoring " :
	     (rflags & ROLE_DO_OTHER)  ? "other " :
	      (rflags & ROLE_DO_OTHER)  ? "search " :
	       (rflags & ROLE_DO_ROLES)  ? "role " : "";
    else
      q = (rflags & ROLE_DO_INCOLS) ? "Index Color " :
	   (rflags & ROLE_DO_FILTER) ? "Filtering " :
	    (rflags & ROLE_DO_SCORES) ? "Scoring " :
	     (rflags & ROLE_DO_OTHER)  ? "Other " :
	      (rflags & ROLE_DO_OTHER)  ? "Search " :
	       (rflags & ROLE_DO_ROLES)  ? "Role " : "";
    
    /* it ain't right to say "a index" */
    if(prev_word_is_a && !struncmp(q, "index", 5))
      q += 6;
      
    snprintf(buf, buflen, fmt, q);
    buf[buflen-1] = '\0';
    return(buf);
}


/*
 * filter option list manipulation tool
 * 
 * 
 * returns:  -1 on unrecognized cmd, 0 if no change, 1 if change
 */
int
feat_checkbox_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int  rv = 0;

    switch(cmd){
      case MC_TOGGLE:				/* mark/unmark option */
	rv = 1;
	toggle_feat_option_bit(ps, (*cl)->varmem, (*cl)->var, (*cl)->value);
	break;

      case MC_EXIT:				 /* exit */
        rv = role_filt_exitcheck(cl, flags);
	break;

      default :
	rv = -1;
	break;
    }

    return(rv);
}


void
toggle_feat_option_bit(struct pine *ps, int index, struct variable *var, char *value)
{
    NAMEVAL_S  *f;

    f  = feat_feature_list(index);

    /* flip the bit */
    if(bitnset(f->value, feat_option_list))
      clrbitn(f->value, feat_option_list);
    else
      setbitn(f->value, feat_option_list);

    if(value)
      value[1] = bitnset(f->value, feat_option_list) ? 'X' : ' ';
}


NAMEVAL_S *
feat_feature_list(int index)
{
    static NAMEVAL_S opt_feat_list[] = {
	{"use-date-header-for-age",        NULL, FEAT_SENTDATE},
	{"move-only-if-not-deleted",       NULL, FEAT_IFNOTDEL},
	{"dont-stop-even-if-rule-matches", NULL, FEAT_NONTERM}
    };

    return((index >= 0 &&
	    index < (sizeof(opt_feat_list)/sizeof(opt_feat_list[0])))
		   ? &opt_feat_list[index] : NULL);
}


/*
 * address type list manipulation tool
 * 
 * 
 * returns:  -1 on unrecognized cmd, 0 if no change, 1 if change
 */
int
inabook_checkbox_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int  rv = 0;

    switch(cmd){
      case MC_TOGGLE:				/* mark/unmark option */
	rv = 1;
	toggle_inabook_type_bit(ps, (*cl)->varmem, (*cl)->var, (*cl)->value);
	break;

      case MC_EXIT:				 /* exit */
        rv = role_filt_exitcheck(cl, flags);
	break;

      default :
	rv = -1;
	break;
    }

    return(rv);
}


void
toggle_inabook_type_bit(struct pine *ps, int index, struct variable *var, char *value)
{
    NAMEVAL_S  *f;

    f  = inabook_feature_list(index);

    /* flip the bit */
    if(bitnset(f->value, inabook_type_list))
      clrbitn(f->value, inabook_type_list);
    else
      setbitn(f->value, inabook_type_list);

    if(value)
      value[1] = bitnset(f->value, inabook_type_list) ? 'X' : ' ';
}


NAMEVAL_S *
inabook_feature_list(int index)
{
    static NAMEVAL_S inabook_feat_list[] = {
	{"From",      NULL, INABOOK_FROM},
	{"Reply-To",  NULL, INABOOK_REPLYTO},
	{"Sender",    NULL, INABOOK_SENDER},
	{"To",        NULL, INABOOK_TO},
	{"Cc",        NULL, INABOOK_CC}
    };

    return((index >= 0 &&
	    index < (sizeof(inabook_feat_list)/sizeof(inabook_feat_list[0])))
		   ? &inabook_feat_list[index] : NULL);
}
