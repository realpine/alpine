#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: ldapconf.c 1012 2008-03-26 00:44:22Z hubert@u.washington.edu $";
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
#include "ldapconf.h"
#include "keymenu.h"
#include "radio.h"
#include "status.h"
#include "confscroll.h"
#include "adrbkcmd.h"
#include "titlebar.h"
#include "takeaddr.h"
#include "../pith/ldap.h"
#include "../pith/state.h"
#include "../pith/bitmap.h"
#include "../pith/mailcmd.h"
#include "../pith/list.h"


/*
 * Internal prototypes
 */
#ifdef	ENABLE_LDAP
int	 addr_select_tool(struct pine *, int, CONF_S **, unsigned);
void     dir_init_display(struct pine *, CONF_S **, char **, struct variable *, CONF_S **);
int	 dir_config_tool(struct pine *, int, CONF_S **, unsigned);
void	 dir_config_add(struct pine *, CONF_S **);
void	 dir_config_shuffle(struct pine *, CONF_S **);
void	 dir_config_edit(struct pine *, CONF_S **);
int      dir_edit_screen(struct pine *, LDAP_SERV_S *, char *, char **);
int      dir_edit_tool(struct pine *, int, CONF_S **, unsigned);
void	 dir_config_del(struct pine *, CONF_S **);
void     add_ldap_fake_first_server(struct pine *, CONF_S **, struct variable *,
				    struct key_menu *, HelpType,
				    int (*)(struct pine *, int, CONF_S **, unsigned));
void     add_ldap_server_to_display(struct pine *, CONF_S **, char *, char *,
				    struct variable *, int, struct key_menu *, HelpType,
				    int (*)(struct pine *, int, CONF_S **, unsigned),
				    int, CONF_S **);
int	 ldap_checkbox_tool(struct pine *, int, CONF_S **, unsigned);
void	 toggle_ldap_option_bit(struct pine *, int, struct variable *, char *);
NAMEVAL_S *ldap_feature_list(int);


static char *srch_res_help_title = N_("HELP FOR SEARCH RESULTS INDEX");
static char *set_choose = "---  ----------------------";
#define ADD_FIRST_LDAP_SERVER _("Use Add to add a directory server")
#define ADDR_SELECT_EXIT_VAL 5
#define ADDR_SELECT_GOBACK_VAL 6
#define ADDR_SELECT_FORCED_EXIT_VAL 7


static int some_selectable;
static char *dserv = N_("Directory Server on ");

/*
 * Let user choose an ldap entry (or return an entry if user doesn't need
 * to be consulted).
 *
 * Returns  0 if ok,
 *         -1 if Exit was chosen
 *         -2 if none were selectable
 *         -3 if no entries existed at all
 *         -4 go back to Abook List was chosen
 *         -5 caller shouldn't free ac->res_head
 *
 *      When 0 is returned the winner is pointed to by result.
 *         Result is an allocated LDAP_SEARCH_WINNER_S which has pointers
 *         to the ld and entry that were chosen. Those are pointers into
 *         the initial data, not copies. The two-pointer structure is
 *         allocated here and freed by the caller.
 */
int
ldap_addr_select(struct pine *ps, ADDR_CHOOSE_S *ac, LDAP_CHOOSE_S **result,
		 LDAPLookupStyle style, WP_ERR_S *wp_err, char *srchstr)
{
    LDAPMessage     *e;
    LDAP_SERV_RES_S *res_list;
    CONF_S          *ctmpa = NULL, *first_line = NULL, *alt_first_line = NULL;
    int              i, retval = 0, got_n_mail = 0, got_n_entries = 0;
    int              need_mail;
    OPT_SCREEN_S     screen;
    struct key_menu *km;
    char             ee[200];
    HelpType         help;
    void           (*prev_redrawer) (void);

    dprint((4, "ldap_addr_select()\n"));

    need_mail = (style == AlwaysDisplay || style == DisplayForURL) ? 0 : 1;
    if(style == AlwaysDisplay){
	km = &addr_s_km_with_view;
	help = h_address_display;
    }
    else if(style == AlwaysDisplayAndMailRequired){
	km = &addr_s_km_with_goback;
	help = h_address_select;
    }
    else if(style == DisplayForURL){
	km = &addr_s_km_for_url;
	help = h_address_display;
    }
    else{
	km = &addr_s_km;
	help = h_address_select;
    }

    if(result)
      *result = NULL;

    some_selectable = 0;

    for(res_list = ac->res_head; res_list; res_list = res_list->next){
	for(e = ldap_first_entry(res_list->ld, res_list->res);
	    e != NULL;
	    e = ldap_next_entry(res_list->ld, e)){
	    char       *dn, *a;
	    char      **cn, **org, **unit, **title, **mail, **sn;
	    BerElement *ber;
	    int         indent, have_mail;
	    
	    dn = NULL;
	    cn = org = title = unit = mail = sn = NULL;
	    for(a = ldap_first_attribute(res_list->ld, e, &ber);
		a != NULL;
		a = ldap_next_attribute(res_list->ld, e, ber)){

		dprint((9, " %s", a ? a : "?"));
		if(strcmp(a, res_list->info_used->cnattr) == 0){
		    if(!cn)
		      cn = ldap_get_values(res_list->ld, e, a);

		    if(cn && !(cn[0] && cn[0][0])){
			ldap_value_free(cn);
			cn = NULL;
		    }
		}
		else if(strcmp(a, res_list->info_used->mailattr) == 0){
		    if(!mail)
		      mail = ldap_get_values(res_list->ld, e, a);
		}
		else if(strcmp(a, "o") == 0){
		    if(!org)
		      org = ldap_get_values(res_list->ld, e, a);
		}
		else if(strcmp(a, "ou") == 0){
		    if(!unit)
		      unit = ldap_get_values(res_list->ld, e, a);
		}
		else if(strcmp(a, "title") == 0){
		    if(!title)
		      title = ldap_get_values(res_list->ld, e, a);
		}

		our_ldap_memfree(a);
	    }

	    dprint((9, "\n"));

	    if(!cn){
		for(a = ldap_first_attribute(res_list->ld, e, &ber);
		    a != NULL;
		    a = ldap_next_attribute(res_list->ld, e, ber)){

		    if(strcmp(a,  res_list->info_used->snattr) == 0){
			if(!sn)
			  sn = ldap_get_values(res_list->ld, e, a);

			if(sn && !(sn[0] && sn[0][0])){
			    ldap_value_free(sn);
			    sn = NULL;
			}
		    }

		    our_ldap_memfree(a);
		}
	    }

	    if(mail && mail[0] && mail[0][0])
	      have_mail = 1;
	    else
	      have_mail = 0;

	    got_n_mail += have_mail;
	    got_n_entries++;
	    indent = 2;

	    /*
	     * First line is either cn, sn, or dn.
	     */
	    if(cn){
		new_confline(&ctmpa);
		if(!alt_first_line)
		  alt_first_line = ctmpa;

		ctmpa->flags     |= CF_STARTITEM;
		if(need_mail && !have_mail)
		  ctmpa->flags     |= CF_PRIVATE;

		ctmpa->value      = cpystr(cn[0]);
		ldap_value_free(cn);
		ctmpa->valoffset  = indent;
		ctmpa->keymenu    = km;
		ctmpa->help       = help;
		ctmpa->help_title = _(srch_res_help_title);
		ctmpa->tool       = addr_select_tool;
		ctmpa->d.a.ld     = res_list->ld;
		ctmpa->d.a.entry  = e;
		ctmpa->d.a.info_used = res_list->info_used;
		ctmpa->d.a.serv   = res_list->serv;
		ctmpa->d.a.ac     = ac;
		if(!first_line && (have_mail || !need_mail))
		  first_line = ctmpa;
	    }
	    
	    /* only happens if no cn */
	    if(sn){
		new_confline(&ctmpa);
		if(!alt_first_line)
		  alt_first_line = ctmpa;

		ctmpa->flags     |= CF_STARTITEM;
		if(need_mail && !have_mail)
		  ctmpa->flags     |= CF_PRIVATE;

		ctmpa->value      = cpystr(sn[0]);
		ldap_value_free(sn);
		ctmpa->valoffset  = indent;
		ctmpa->keymenu    = km;
		ctmpa->help       = help;
		ctmpa->help_title = _(srch_res_help_title);
		ctmpa->tool       = addr_select_tool;
		ctmpa->d.a.ld     = res_list->ld;
		ctmpa->d.a.entry  = e;
		ctmpa->d.a.info_used = res_list->info_used;
		ctmpa->d.a.serv   = res_list->serv;
		ctmpa->d.a.ac     = ac;
		if(!first_line && (have_mail || !need_mail))
		  first_line = ctmpa;
	    }

	    if(!sn && !cn){
		new_confline(&ctmpa);
		if(!alt_first_line)
		  alt_first_line = ctmpa;

		ctmpa->flags     |= CF_STARTITEM;
		if(need_mail && !have_mail)
		  ctmpa->flags     |= CF_PRIVATE;

		dn = ldap_get_dn(res_list->ld, e);

		if(dn && !dn[0]){
		    our_ldap_dn_memfree(dn);
		    dn = NULL;
		}

		ctmpa->value      = cpystr(dn ? dn : "?");
		if(dn)
		  our_ldap_dn_memfree(dn);

		ctmpa->valoffset  = indent;
		ctmpa->keymenu    = km;
		ctmpa->help       = help;
		ctmpa->help_title = _(srch_res_help_title);
		ctmpa->tool       = addr_select_tool;
		ctmpa->d.a.ld     = res_list->ld;
		ctmpa->d.a.entry  = e;
		ctmpa->d.a.info_used = res_list->info_used;
		ctmpa->d.a.serv   = res_list->serv;
		ctmpa->d.a.ac     = ac;
		if(!first_line && (have_mail || !need_mail))
		  first_line = ctmpa;
	    }

	    if(title){
		for(i = 0; title[i] && title[i][0]; i++){
		    new_confline(&ctmpa);
		    ctmpa->flags     |= CF_NOSELECT;
		    ctmpa->valoffset  = indent + 2;
		    ctmpa->value      = cpystr(title[i]);
		    ctmpa->keymenu    = km;
		    ctmpa->help       = help;
		    ctmpa->help_title = _(srch_res_help_title);
		    ctmpa->tool       = addr_select_tool;
		    ctmpa->d.a.ld     = res_list->ld;
		    ctmpa->d.a.entry  = e;
		    ctmpa->d.a.info_used = res_list->info_used;
		    ctmpa->d.a.serv   = res_list->serv;
		    ctmpa->d.a.ac     = ac;
		}

		ldap_value_free(title);
	    }

	    if(unit){
		for(i = 0; unit[i] && unit[i][0]; i++){
		    new_confline(&ctmpa);
		    ctmpa->flags     |= CF_NOSELECT;
		    ctmpa->valoffset  = indent + 2;
		    ctmpa->value      = cpystr(unit[i]);
		    ctmpa->keymenu    = km;
		    ctmpa->help       = help;
		    ctmpa->help_title = _(srch_res_help_title);
		    ctmpa->tool       = addr_select_tool;
		    ctmpa->d.a.ld     = res_list->ld;
		    ctmpa->d.a.entry  = e;
		    ctmpa->d.a.info_used = res_list->info_used;
		    ctmpa->d.a.serv   = res_list->serv;
		    ctmpa->d.a.ac     = ac;
		}

		ldap_value_free(unit);
	    }

	    if(org){
		for(i = 0; org[i] && org[i][0]; i++){
		    new_confline(&ctmpa);
		    ctmpa->flags     |= CF_NOSELECT;
		    ctmpa->valoffset  = indent + 2;
		    ctmpa->value      = cpystr(org[i]);
		    ctmpa->keymenu    = km;
		    ctmpa->help       = help;
		    ctmpa->help_title = _(srch_res_help_title);
		    ctmpa->tool       = addr_select_tool;
		    ctmpa->d.a.ld     = res_list->ld;
		    ctmpa->d.a.entry  = e;
		    ctmpa->d.a.info_used = res_list->info_used;
		    ctmpa->d.a.serv   = res_list->serv;
		    ctmpa->d.a.ac     = ac;
		}

		ldap_value_free(org);
	    }

	    if(have_mail){
		/* Don't show long list of email addresses. */
		if(!(mail[0] && mail[0][0]) ||
		   !(mail[1] && mail[1][0]) ||
		   !(mail[2] && mail[2][0]) ||
		   !(mail[3] && mail[3][0])){
		    for(i = 0; mail[i] && mail[i][0]; i++){
			new_confline(&ctmpa);
			ctmpa->flags     |= CF_NOSELECT;
			ctmpa->valoffset  = indent + 2;
			ctmpa->value      = cpystr(mail[i]);
			ctmpa->keymenu    = km;
			ctmpa->help       = help;
			ctmpa->help_title = _(srch_res_help_title);
			ctmpa->tool       = addr_select_tool;
			ctmpa->d.a.ld     = res_list->ld;
			ctmpa->d.a.entry  = e;
			ctmpa->d.a.info_used = res_list->info_used;
			ctmpa->d.a.serv   = res_list->serv;
			ctmpa->d.a.ac     = ac;
		    }
		}
		else{
		    char tmp[200];

		    for(i = 4; mail[i] && mail[i][0]; i++)
		      ;
		    
		    new_confline(&ctmpa);
		    ctmpa->flags     |= CF_NOSELECT;
		    ctmpa->valoffset  = indent + 2;
		    snprintf(tmp, sizeof(tmp), _("(%d email addresses)"), i);
		    tmp[sizeof(tmp)-1] = '\0';
		    ctmpa->value      = cpystr(tmp);
		    ctmpa->keymenu    = km;
		    ctmpa->help       = help;
		    ctmpa->help_title = _(srch_res_help_title);
		    ctmpa->tool       = addr_select_tool;
		    ctmpa->d.a.ld     = res_list->ld;
		    ctmpa->d.a.entry  = e;
		    ctmpa->d.a.info_used = res_list->info_used;
		    ctmpa->d.a.serv   = res_list->serv;
		    ctmpa->d.a.ac     = ac;
		}
	    }
	    else{
		new_confline(&ctmpa);
		ctmpa->flags     |= CF_NOSELECT;
		ctmpa->valoffset  = indent + 2;
		ctmpa->value      = cpystr(_("<No Email Address Available>"));
		ctmpa->keymenu    = km;
		ctmpa->help       = help;
		ctmpa->help_title = _(srch_res_help_title);
		ctmpa->tool       = addr_select_tool;
		ctmpa->d.a.ld     = res_list->ld;
		ctmpa->d.a.entry  = e;
		ctmpa->d.a.info_used = res_list->info_used;
		ctmpa->d.a.serv   = res_list->serv;
		ctmpa->d.a.ac     = ac;
	    }

	    if(mail)
	      ldap_value_free(mail);

	    new_confline(&ctmpa);		/* blank line */
	    ctmpa->keymenu    = km;
	    ctmpa->help       = help;
	    ctmpa->help_title = _(srch_res_help_title);
	    ctmpa->tool       = addr_select_tool;
	    ctmpa->flags     |= CF_NOSELECT | CF_B_LINE;
	}
    }

    if(first_line)
      some_selectable++;
    else if(alt_first_line)
      first_line = alt_first_line;
    else{
	new_confline(&ctmpa);		/* blank line */
	ctmpa->keymenu    = need_mail ? &addr_s_km_exit : &addr_s_km_goback;
	ctmpa->help       = help;
	ctmpa->help_title = _(srch_res_help_title);
	ctmpa->tool       = addr_select_tool;
	ctmpa->flags     |= CF_NOSELECT | CF_B_LINE;

	new_confline(&ctmpa);
	first_line = ctmpa;
	strncpy(ee, "[ ", sizeof(ee));
	ee[sizeof(ee)-1] = '\0';
	if(wp_err && wp_err->ldap_errno)
	  /* TRANSLATORS: No matches returned for an LDAP search */
	  snprintf(ee+2, sizeof(ee)-2, _("%s, No Matches Returned"),
		  ldap_err2string(wp_err->ldap_errno));
	else
	  strncpy(ee+2, _("No Matches"), sizeof(ee)-2);

	ee[sizeof(ee)-1] = '\0';

	/* TRANSLATORS: a request for user to choose Exit after they read text */
	strncat(ee, _(" -- Choose Exit ]"),  sizeof(ee)-strlen(ee)-1);
	ee[sizeof(ee)-1] = '\0';
	ctmpa->value      = cpystr(ee);
	ctmpa->valoffset  = 10;
	ctmpa->keymenu    = need_mail ? &addr_s_km_exit : &addr_s_km_goback;
	ctmpa->help       = help;
	ctmpa->help_title = _(srch_res_help_title);
	ctmpa->tool       = addr_select_tool;
	ctmpa->flags     |= CF_NOSELECT;
    }

    if(style == AlwaysDisplay || style == DisplayForURL ||
       style == AlwaysDisplayAndMailRequired ||
       (style == DisplayIfOne && got_n_mail >= 1) ||
       (style == DisplayIfTwo && got_n_mail >= 1 && got_n_entries >= 2)){
	if(wp_err && wp_err->mangled)
	  *wp_err->mangled = 1;

	prev_redrawer = ps_global->redrawer;
	push_titlebar_state();

	memset(&screen, 0, sizeof(screen));
	/* TRANSLATORS: Print something1 using something2.
	   "this" is something1 */
	switch(conf_scroll_screen(ps,&screen,first_line,ac->title,_("this"),0)){
	  case ADDR_SELECT_EXIT_VAL:
	    retval = -1;
	    break;

	  case ADDR_SELECT_GOBACK_VAL:
	    retval = -4;
	    break;

	  case ADDR_SELECT_FORCED_EXIT_VAL:
	    if(alt_first_line)	/* some entries, but none suitable */
	      retval = -2;
	    else
	      retval = -3;

	    break;

	  default:
	    retval = 0;
	    break;
	}

	ClearScreen();
	pop_titlebar_state();
	redraw_titlebar();
	if((ps_global->redrawer = prev_redrawer) != NULL)
	  (*ps_global->redrawer)();

	if(result && retval == 0 && ac->selected_ld && ac->selected_entry){
	    (*result) = (LDAP_CHOOSE_S *)fs_get(sizeof(LDAP_CHOOSE_S));
	    (*result)->ld    = ac->selected_ld;
	    (*result)->selected_entry = ac->selected_entry;
	    (*result)->info_used = ac->info_used;
	    (*result)->serv  = ac->selected_serv;
	}
    }
    else if(style == DisplayIfOne && got_n_mail < 1){
	if(alt_first_line)	/* some entries, but none suitable */
	  retval = -2;
	else
	  retval = -3;

	first_line = first_confline(ctmpa);
	free_conflines(&first_line);
    }
    else if(style == DisplayIfTwo && (got_n_mail < 1 || got_n_entries < 2)){
	if(got_n_mail < 1){
	    if(alt_first_line)	/* some entries, but none suitable */
	      retval = -2;
	    else
	      retval = -3;
	}
	else{
	    retval = 0;
	    if(result){
		(*result) = (LDAP_CHOOSE_S *)fs_get(sizeof(LDAP_CHOOSE_S));
		(*result)->ld    = first_line->d.a.ld;
		(*result)->selected_entry   = first_line->d.a.entry;
		(*result)->info_used = first_line->d.a.info_used;
		(*result)->serv  = first_line->d.a.serv;
	    }
	}

	first_line = first_confline(ctmpa);
	free_conflines(&first_line);
    }

    return(retval);
}


int
addr_select_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int retval = 0;

    switch(cmd){
      case MC_CHOICE :
	if(flags & CF_PRIVATE){
	    q_status_message(SM_ORDER | SM_DING, 0, 3,
     _("No email address available for this entry; choose another or ExitSelect"));
	}
	else if(some_selectable){
	    (*cl)->d.a.ac->selected_ld    = (*cl)->d.a.ld;
	    (*cl)->d.a.ac->selected_entry = (*cl)->d.a.entry;
	    (*cl)->d.a.ac->info_used      = (*cl)->d.a.info_used;
	    (*cl)->d.a.ac->selected_serv  = (*cl)->d.a.serv;
	    retval = simple_exit_cmd(flags);
	}
	else
	  retval = ADDR_SELECT_FORCED_EXIT_VAL;

	break;

      case MC_VIEW_TEXT :
      case MC_SAVE :
      case MC_FWDTEXT :
      case MC_COMPOSE :
      case MC_ROLE :
	{LDAP_CHOOSE_S *e;

	  if((*cl)->d.a.ld && (*cl)->d.a.entry){
	    e = (LDAP_CHOOSE_S *)fs_get(sizeof(LDAP_CHOOSE_S));
	    e->ld    = (*cl)->d.a.ld;
	    e->selected_entry   = (*cl)->d.a.entry;
	    e->info_used = (*cl)->d.a.info_used;
	    e->serv  = (*cl)->d.a.serv;
	    if(cmd == MC_VIEW_TEXT)
	      view_ldap_entry(ps, e);
	    else if(cmd == MC_SAVE)
	      save_ldap_entry(ps, e, 0);
	    else if(cmd == MC_COMPOSE)
	      compose_to_ldap_entry(ps, e, 0);
	    else if(cmd == MC_ROLE)
	      compose_to_ldap_entry(ps, e, 1);
	    else
	      forward_ldap_entry(ps, e);

	    fs_give((void **)&e);
	  }
	}

	break;

      case MC_ADDRBOOK :
        retval = ADDR_SELECT_GOBACK_VAL;
	break;

      case MC_EXIT :
        retval = ADDR_SELECT_EXIT_VAL;
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
dir_init_display(struct pine *ps, CONF_S **ctmp, char **servers,
		 struct variable *var, CONF_S **first_line)
{
    int   i;
    char *serv;
    char *subtitle;
    LDAP_SERV_S *info;

    if(first_line)
      *first_line = NULL;

    if(servers && servers[0] && servers[0][0]){
	for(i = 0; servers[i]; i++){
	    info = break_up_ldap_server(servers[i]);
	    serv = (info && info->nick && *info->nick) ? cpystr(info->nick) :
		     (info && info->serv && *info->serv) ? cpystr(info->serv) :
		       cpystr(_("Bad Server Config, Delete this"));
	    subtitle = (char *)fs_get((((info && info->serv && *info->serv)
					    ? strlen(info->serv)
					    : 3) +
					       strlen(_(dserv)) + 15) *
					 sizeof(char));
	    if(info && info->port >= 0)
	      snprintf(subtitle, sizeof(subtitle), "%s%s:%d",
		      _(dserv),
		      (info && info->serv && *info->serv) ? info->serv : "<?>",
		      info->port);
	    else
	      snprintf(subtitle, sizeof(subtitle), "%s%s",
		      _(dserv),
		      (info && info->serv && *info->serv) ? info->serv : "<?>");

	    subtitle[sizeof(subtitle)-1] = '\0';

	    add_ldap_server_to_display(ps, ctmp, serv, subtitle, var,
				       i, &dir_conf_km, h_direct_config,
				       dir_config_tool, 0,
				       (first_line && *first_line == NULL)
					  ? first_line
					  : NULL);

	    free_ldap_server_info(&info);
	}
    }
    else{
	add_ldap_fake_first_server(ps, ctmp, var,
				   &dir_conf_km, h_direct_config,
				   dir_config_tool);
	if(first_line)
	  *first_line = *ctmp;
    }
}


void
directory_config(struct pine *ps, int edit_exceptions)
{
    CONF_S   *ctmp = NULL, *first_line = NULL;
    OPT_SCREEN_S  screen;
    int           no_ex, readonly_warning = 0;

    if(edit_exceptions){
	q_status_message(SM_ORDER, 3, 7,
			 _("Exception Setup not implemented for directory"));
	return;
    }

    ew = edit_exceptions ? ps_global->ew_for_except_vars : Main;
    
    no_ex = (ps_global->ew_for_except_vars == Main);

    if(ps->restricted)
      readonly_warning = 1;
    else{
	PINERC_S *prc = NULL;

	switch(ew){
	  case Main:
	    prc = ps->prc;
	    break;
	  case Post:
	    prc = ps->post_prc;
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

    if(ps->fix_fixed_warning)
      offer_to_fix_pinerc(ps);

    dir_init_display(ps, &ctmp, no_ex ? ps->VAR_LDAP_SERVERS
				      : LVAL(&ps->vars[V_LDAP_SERVERS], ew),
		     &ps->vars[V_LDAP_SERVERS], &first_line);

    memset(&screen, 0, sizeof(screen));
    screen.deferred_ro_warning = readonly_warning;
    /* TRANSLATORS: Print something1 using something2.
       "servers" is something1 */
    (void)conf_scroll_screen(ps, &screen, first_line,
			     _("SETUP DIRECTORY SERVERS"), _("servers"), 0);
    ps->mangled_screen = 1;
}


int
dir_config_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int first_one, rv = 0;

    first_one = (*cl)->value &&
		(strcmp((*cl)->value, ADD_FIRST_LDAP_SERVER) == 0);
    switch(cmd){
      case MC_DELETE :
	if(first_one)
	  q_status_message(SM_ORDER|SM_DING, 0, 3,
			   _("Nothing to Delete, use Add"));
	else
	  dir_config_del(ps, cl);

	break;

      case MC_ADD :
	if(!fixed_var((*cl)->var, NULL, "directory list"))
	  dir_config_add(ps, cl);

	break;

      case MC_EDIT :
	if(!fixed_var((*cl)->var, NULL, "directory list")){
	    if(first_one)
	      dir_config_add(ps, cl);
	    else
	      dir_config_edit(ps, cl);
	}

	break;

      case MC_SHUFFLE :
	if(!fixed_var((*cl)->var, NULL, "directory list")){
	    if(first_one)
	      q_status_message(SM_ORDER|SM_DING, 0, 3,
			       _("Nothing to Shuffle, use Add"));
	    else
	      dir_config_shuffle(ps, cl);
	}

	break;

      case MC_EXIT :
	rv = 2;
	break;

      default:
	rv = -1;
	break;
    }

    return(rv);
}


/*
 * Add LDAP directory entry
 */
void
dir_config_add(struct pine *ps, CONF_S **cl)
{
    char *raw_server = NULL;
    LDAP_SERV_S *info = NULL;
    char **lval;
    int no_ex;

    no_ex = (ps_global->ew_for_except_vars == Main);

    if(dir_edit_screen(ps, NULL, "ADD A", &raw_server) == 1){

	info = break_up_ldap_server(raw_server);

	if(info && info->serv && *info->serv){
	    char  *subtitle;
	    int    i, cnt = 0;
	    char **new_list;
	    CONF_S *cp;

	    lval = no_ex ? (*cl)->var->current_val.l : LVAL((*cl)->var, ew);
	    if(lval)
	      while(lval[cnt])
		cnt++;

	    /* catch the special "" case */
	    if(cnt == 0 ||
	       (cnt == 1 && lval[0][0] == '\0')){
		new_list = (char **)fs_get((1 + 1) * sizeof(char *));
		new_list[0] = raw_server;
		new_list[1] = NULL;
	    }
	    else{
		/* add one for new value */
		cnt++;
		new_list = (char **)fs_get((cnt + 1) * sizeof(char *));

		for(i = 0; i < (*cl)->varmem; i++)
		  new_list[i] = cpystr(lval[i]);

		new_list[(*cl)->varmem] = raw_server;

		for(i = (*cl)->varmem; i < cnt; i++)
		  new_list[i+1] = cpystr(lval[i]);
	    }

	    raw_server = NULL;
	    set_variable_list(V_LDAP_SERVERS, new_list, FALSE, ew);
	    free_list_array(&new_list);
	    set_current_val((*cl)->var, TRUE, FALSE);
	    subtitle = (char *)fs_get((((info && info->serv && *info->serv)
					    ? strlen(info->serv)
					    : 3) +
					       strlen(_(dserv)) + 15) *
					 sizeof(char));
	    if(info && info->port >= 0)
	      snprintf(subtitle, sizeof(subtitle), "%s%s:%d",
		      _(dserv),
		      (info && info->serv && *info->serv) ? info->serv : "<?>",
		      info->port);
	    else
	      snprintf(subtitle, sizeof(subtitle), "%s%s",
		      _(dserv),
		      (info && info->serv && *info->serv) ? info->serv : "<?>");

	    subtitle[sizeof(subtitle)-1] = '\0';

	    if(cnt < 2){			/* first one */
		struct variable *var;
		struct key_menu *keymenu;
		HelpType         help;
		int            (*tool)(struct pine *, int, CONF_S **, unsigned);

		var      = (*cl)->var;
		keymenu  = (*cl)->keymenu;
		help     = (*cl)->help;
		tool     = (*cl)->tool;
		*cl = first_confline(*cl);
		free_conflines(cl);
		add_ldap_server_to_display(ps, cl,
					   (info && info->nick && *info->nick)
					     ? cpystr(info->nick)
					     : cpystr(info->serv),
					   subtitle, var, 0, keymenu, help,
					   tool, 0, NULL);

		opt_screen->top_line = NULL;
	    }
	    else{
		/*
		 * Insert new server.
		 */
		add_ldap_server_to_display(ps, cl,
					   (info && info->nick && *info->nick)
					     ? cpystr(info->nick)
					     : cpystr(info->serv),
					   subtitle,
					   (*cl)->var,
					   (*cl)->varmem,
					   (*cl)->keymenu,
					   (*cl)->help,
					   (*cl)->tool,
					   1,
					   NULL);
		/* adjust the rest of the varmems */
		for(cp = (*cl)->next; cp; cp = cp->next)
		  cp->varmem++;
	    }

	    /* because add_ldap advanced cl to its third line */
	    (*cl) = (*cl)->prev->prev;
	    
	    fix_side_effects(ps, (*cl)->var, 0);
	    write_pinerc(ps, ew, WRP_NONE);
	}
	else
	  q_status_message(SM_ORDER, 0, 3, _("Add cancelled, no server name"));
    }

    free_ldap_server_info(&info);
    if(raw_server)
      fs_give((void **)&raw_server);
}


/*
 * Shuffle order of LDAP directory entries
 */
void
dir_config_shuffle(struct pine *ps, CONF_S **cl)
{
    int      cnt, rv, current_num, new_num, i, j, deefault;
    char   **new_list, **lval;
    char     tmp[200];
    HelpType help;
    ESCKEY_S opts[3];
    CONF_S  *a, *b;
    int no_ex;

    no_ex = (ps_global->ew_for_except_vars == Main);

    /* how many are in our current list? */
    lval = no_ex ? (*cl)->var->current_val.l : LVAL((*cl)->var, ew);
    for(cnt = 0; lval && lval[cnt]; cnt++)
      ;
    
    if(cnt < 2){
	q_status_message(SM_ORDER, 0, 3,
	 _("Shuffle only makes sense when there is more than one server in list"));
	return;
    }

    current_num = (*cl)->varmem;  /* variable number of highlighted directory */

    /* Move it up or down? */
    i = 0;
    opts[i].ch      = 'u';
    opts[i].rval    = 'u';
    opts[i].name    = "U";
    opts[i++].label = _("Up");

    opts[i].ch      = 'd';
    opts[i].rval    = 'd';
    opts[i].name    = "D";
    opts[i++].label = _("Down");

    opts[i].ch = -1;
    deefault = 'u';

    if(current_num == 0){			/* no up */
	opts[0].ch = -2;
	deefault = 'd';
    }
    else if(current_num == cnt - 1)		/* no down */
      opts[1].ch = -2;

    snprintf(tmp, sizeof(tmp), "Shuffle \"%s\" %s%s%s ? ",
	    (*cl)->value,
	    (opts[0].ch != -2) ? _("UP") : "",
	    (opts[0].ch != -2 && opts[1].ch != -2) ? " or " : "",
	    (opts[1].ch != -2) ? _("DOWN") : "");
    tmp[sizeof(tmp)-1] = '\0';
    help = (opts[0].ch == -2) ? h_dir_shuf_down
			      : (opts[1].ch == -2) ? h_dir_shuf_up
						   : h_dir_shuf;

    rv = radio_buttons(tmp, -FOOTER_ROWS(ps), opts, deefault, 'x',
		       help, RB_NORM);

    switch(rv){
      case 'x':
	cmd_cancelled("Shuffle");
	return;

      case 'u':
	new_num = current_num - 1;
	a = (*cl)->prev->prev->prev;
	b = *cl;
	break;

      case 'd':
	new_num = current_num + 1;
	a = *cl;
	b = (*cl)->next->next->next;
	break;
    }

    /* allocate space for new list */
    new_list = (char **)fs_get((cnt + 1) * sizeof(char *));

    /* fill in new_list */
    for(i = 0; i < cnt; i++){
	if(i == current_num)
	  j = new_num;
	else if (i == new_num)
	  j = current_num;
	else
	  j = i;

	/* notice this works even if we were using default */
	new_list[i] = cpystr(lval[j]);
    }

    new_list[i] = NULL;

    j = set_variable_list((*cl)->var - ps->vars, new_list, TRUE, ew);
    free_list_array(&new_list);
    if(j){
	q_status_message(SM_ORDER, 0, 3,
			 _("Shuffle cancelled: couldn't save configuration file"));
	set_current_val((*cl)->var, TRUE, FALSE);
	return;
    }

    set_current_val((*cl)->var, TRUE, FALSE);

    if(a == opt_screen->top_line)
      opt_screen->top_line = b;
    
    j = a->varmem;
    a->varmem = b->varmem;
    b->varmem = j;

    /*
     * Swap display lines. To start with, a is lower in list, b is higher.
     * The fact that there are 3 lines per entry is totally entangled in
     * the code.
     */
    a->next->next->next = b->next->next->next;
    if(b->next->next->next)
      b->next->next->next->prev = a->next->next;
    b->prev = a->prev;
    if(a->prev)
      a->prev->next = b;
    b->next->next->next = a;
    a->prev = b->next->next;

    ps->mangled_body = 1;
    write_pinerc(ps, ew, WRP_NONE);
}


/*
 * Edit LDAP directory entry
 */
void
dir_config_edit(struct pine *ps, CONF_S **cl)
{
    char        *raw_server = NULL, **lval;
    LDAP_SERV_S *info;
    int no_ex;

    no_ex = (ps_global->ew_for_except_vars == Main);

    lval = no_ex ? (*cl)->var->current_val.l : LVAL((*cl)->var, ew);
    info = break_up_ldap_server((lval && lval[(*cl)->varmem])
				    ? lval[(*cl)->varmem] : NULL);
    
    if(dir_edit_screen(ps, info, "CHANGE THIS", &raw_server) == 1){

	free_ldap_server_info(&info);
	info = break_up_ldap_server(raw_server);

	if(lval && lval[(*cl)->varmem] &&
	   strcmp(lval[(*cl)->varmem], raw_server) == 0)
	  q_status_message(SM_ORDER, 0, 3, _("No change, cancelled"));
	else if(!(info && info->serv && *info->serv))
	  q_status_message(SM_ORDER, 0, 3,
	      _("Change cancelled, use Delete if you want to remove this server"));
	else{
	    char  *subtitle;
	    int    i, cnt;
	    char **new_list;

	    for(cnt = 0; lval && lval[cnt]; cnt++)
	      ;

	    new_list = (char **)fs_get((cnt + 1) * sizeof(char *));

	    for(i = 0; i < (*cl)->varmem; i++)
	      new_list[i] = cpystr(lval[i]);

	    new_list[(*cl)->varmem] = raw_server;
	    raw_server = NULL;

	    for(i = (*cl)->varmem + 1; i < cnt; i++)
	      new_list[i] = cpystr(lval[i]);
	    
	    new_list[cnt] = NULL;
	    set_variable_list(V_LDAP_SERVERS, new_list, FALSE, ew);
	    free_list_array(&new_list);
	    set_current_val((*cl)->var, TRUE, FALSE);

	    if((*cl)->value)
	      fs_give((void **)&(*cl)->value);

	    (*cl)->value = cpystr((info->nick && *info->nick) ? info->nick
							      : info->serv);

	    if((*cl)->next->value)
	      fs_give((void **)&(*cl)->next->value);

	    subtitle = (char *)fs_get((((info && info->serv && *info->serv)
					    ? strlen(info->serv)
					    : 3) +
					       strlen(_(dserv)) + 15) *
					 sizeof(char));
	    if(info && info->port >= 0)
	      snprintf(subtitle, sizeof(subtitle), "%s%s:%d",
		      _(dserv),
		      (info && info->serv && *info->serv) ? info->serv : "<?>",
		      info->port);
	    else
	      snprintf(subtitle, sizeof(subtitle), "%s%s",
		      _(dserv),
		      (info && info->serv && *info->serv) ? info->serv : "<?>");

	    subtitle[sizeof(subtitle)-1] = '\0';

	    (*cl)->next->value = subtitle;

	    fix_side_effects(ps, (*cl)->var, 0);
	    write_pinerc(ps, ew, WRP_NONE);
	}
    }

    free_ldap_server_info(&info);
    if(raw_server)
      fs_give((void **)&raw_server);
}


#define   LDAP_F_IMPL  0
#define   LDAP_F_RHS   1
#define   LDAP_F_REF   2
#define   LDAP_F_NOSUB 3
#define   LDAP_F_TLS   4
#define   LDAP_F_TLSMUST 5
bitmap_t  ldap_option_list;
struct variable *ldap_srch_rule_ptr;

/*
 * Gives user screen to edit config values for ldap server.
 *
 * Args    ps  -- pine struct
 *         def -- default values to start with
 *       title -- part of title at top of screen
 *  raw_server -- This is the returned item, allocated here and freed by caller.
 *
 * Returns:  0 if no change
 *           1 if user requested a change
 *               (change is stored in raw_server and hasn't been acted upon yet)
 *          10 user says abort
 */
int
dir_edit_screen(struct pine *ps, LDAP_SERV_S *def, char *title, char **raw_server)
{
    OPT_SCREEN_S    screen, *saved_screen;
    CONF_S         *ctmp = NULL, *ctmpb, *first_line = NULL;
    char            tmp[MAXPATH+1], custom_scope[MAXPATH], **apval;
    int             rv, i, j, lv, indent, rindent;
    NAMEVAL_S      *f;
    struct variable server_var, base_var, binddn_var, port_var, nick_var,
		    srch_type_var, srch_rule_var, time_var,
		    size_var, mailattr_var, cnattr_var,
		    snattr_var, gnattr_var, cust_var,
		    opt_var, *v, *varlist[21];
    char           *server = NULL, *base = NULL, *port = NULL, *nick = NULL,
		   *srch_type = NULL, *srch_rule = NULL, *ttime = NULL,
		   *c_s_f = "custom-search-filter", *binddn = NULL,
		   *ssize = NULL, *mailattr = NULL, *cnattr = NULL,
		   *snattr = NULL, *gnattr = NULL, *cust = NULL;

    /*
     * We edit by making a nested call to conf_scroll_screen.
     * We use some fake struct variables to get back the results in, and
     * so we can use the existing tools from the config screen.
     */

    custom_scope[0] = '\0';

    varlist[j = 0] = &server_var;
    varlist[++j] = &base_var;
    varlist[++j] = &port_var;
    varlist[++j] = &binddn_var;
    varlist[++j] = &nick_var;
    varlist[++j] = &srch_type_var;
    varlist[++j] = &srch_rule_var;
    varlist[++j] = &time_var;
    varlist[++j] = &size_var;
    varlist[++j] = &mailattr_var;
    varlist[++j] = &cnattr_var;
    varlist[++j] = &snattr_var;
    varlist[++j] = &gnattr_var;
    varlist[++j] = &cust_var;
    varlist[++j] = &opt_var;
    varlist[++j] = NULL;
    for(j = 0; varlist[j]; j++)
      memset(varlist[j], 0, sizeof(struct variable));

    server_var.name       = cpystr("ldap-server");
    server_var.is_used    = 1;
    server_var.is_user    = 1;
    apval = APVAL(&server_var, ew);
    *apval = (def && def->serv && def->serv[0]) ? cpystr(def->serv) : NULL;
    set_current_val(&server_var, FALSE, FALSE);

    base_var.name       = cpystr("search-base");
    base_var.is_used    = 1;
    base_var.is_user    = 1;
    apval = APVAL(&base_var, ew);
    *apval = (def && def->base && def->base[0]) ? cpystr(def->base) : NULL;
    set_current_val(&base_var, FALSE, FALSE);

    port_var.name       = cpystr("port");
    port_var.is_used    = 1;
    port_var.is_user    = 1;
    if(def && def->port >= 0){
	apval = APVAL(&port_var, ew);
	*apval = cpystr(int2string(def->port));
    }

    port_var.global_val.p = cpystr(int2string(LDAP_PORT));
    set_current_val(&port_var, FALSE, FALSE);

    binddn_var.name       = cpystr("bind-dn");
    binddn_var.is_used    = 1;
    binddn_var.is_user    = 1;
    apval = APVAL(&binddn_var, ew);
    *apval = (def && def->binddn && def->binddn[0]) ? cpystr(def->binddn) : NULL;
    set_current_val(&binddn_var, FALSE, FALSE);

    nick_var.name       = cpystr("nickname");
    nick_var.is_used    = 1;
    nick_var.is_user    = 1;
    apval = APVAL(&nick_var, ew);
    *apval = (def && def->nick && def->nick[0]) ? cpystr(def->nick) : NULL;
    set_current_val(&nick_var, FALSE, FALSE);

    srch_type_var.name       = cpystr("search-type");
    srch_type_var.is_used    = 1;
    srch_type_var.is_user    = 1;
    apval = APVAL(&srch_type_var, ew);
    *apval = (f=ldap_search_types(def ? def->type : -1))
			? cpystr(f->name) : NULL;
    srch_type_var.global_val.p =
	(f=ldap_search_types(DEF_LDAP_TYPE)) ? cpystr(f->name) : NULL;
    set_current_val(&srch_type_var, FALSE, FALSE);

    ldap_srch_rule_ptr = &srch_rule_var;	/* so radiobuttons can tell */
    srch_rule_var.name       = cpystr("search-rule");
    srch_rule_var.is_used    = 1;
    srch_rule_var.is_user    = 1;
    apval = APVAL(&srch_rule_var, ew);
    *apval = (f=ldap_search_rules(def ? def->srch : -1))
			? cpystr(f->name) : NULL;
    srch_rule_var.global_val.p =
	(f=ldap_search_rules(DEF_LDAP_SRCH)) ? cpystr(f->name) : NULL;
    set_current_val(&srch_rule_var, FALSE, FALSE);

    time_var.name       = cpystr("timelimit");
    time_var.is_used    = 1;
    time_var.is_user    = 1;
    if(def && def->time >= 0){
	apval = APVAL(&time_var, ew);
	*apval = cpystr(int2string(def->time));
    }

    time_var.global_val.p = cpystr(int2string(DEF_LDAP_TIME));
    set_current_val(&time_var, FALSE, FALSE);

    size_var.name       = cpystr("sizelimit");
    size_var.is_used    = 1;
    size_var.is_user    = 1;
    if(def && def->size >= 0){
	apval = APVAL(&size_var, ew);
	*apval = cpystr(int2string(def->size));
    }

    size_var.global_val.p = cpystr(int2string(DEF_LDAP_SIZE));
    set_current_val(&size_var, FALSE, FALSE);

    mailattr_var.name       = cpystr("email-attribute");
    mailattr_var.is_used    = 1;
    mailattr_var.is_user    = 1;
    apval = APVAL(&mailattr_var, ew);
    *apval = (def && def->mailattr && def->mailattr[0])
		    ? cpystr(def->mailattr) : NULL;
    mailattr_var.global_val.p = cpystr(DEF_LDAP_MAILATTR);
    set_current_val(&mailattr_var, FALSE, FALSE);

    cnattr_var.name       = cpystr("name-attribute");
    cnattr_var.is_used    = 1;
    cnattr_var.is_user    = 1;
    apval = APVAL(&cnattr_var, ew);
    *apval = (def && def->cnattr && def->cnattr[0])
		    ? cpystr(def->cnattr) : NULL;
    cnattr_var.global_val.p = cpystr(DEF_LDAP_CNATTR);
    set_current_val(&cnattr_var, FALSE, FALSE);

    snattr_var.name       = cpystr("surname-attribute");
    snattr_var.is_used    = 1;
    snattr_var.is_user    = 1;
    apval = APVAL(&snattr_var, ew);
    *apval = (def && def->snattr && def->snattr[0])
		    ? cpystr(def->snattr) : NULL;
    snattr_var.global_val.p = cpystr(DEF_LDAP_SNATTR);
    set_current_val(&snattr_var, FALSE, FALSE);

    gnattr_var.name       = cpystr("givenname-attribute");
    gnattr_var.is_used    = 1;
    gnattr_var.is_user    = 1;
    apval = APVAL(&gnattr_var, ew);
    *apval = (def && def->gnattr && def->gnattr[0])
		    ? cpystr(def->gnattr) : NULL;
    gnattr_var.global_val.p = cpystr(DEF_LDAP_GNATTR);
    set_current_val(&gnattr_var, FALSE, FALSE);

    cust_var.name       = cpystr(c_s_f);
    cust_var.is_used    = 1;
    cust_var.is_user    = 1;
    apval = APVAL(&cust_var, ew);
    *apval = (def && def->cust && def->cust[0]) ? cpystr(def->cust) : NULL;
    set_current_val(&cust_var, FALSE, FALSE);

    /* TRANSLATORS: Features is a section title in the LDAP configuration screen. Following
       this are a list of features or options that can be turned on or off. */
    opt_var.name          = cpystr(_("Features"));
    opt_var.is_used       = 1;
    opt_var.is_user       = 1;
    opt_var.is_list       = 1;
    clrbitmap(ldap_option_list);
    if(def && def->impl)
      setbitn(LDAP_F_IMPL, ldap_option_list);
    if(def && def->rhs)
      setbitn(LDAP_F_RHS, ldap_option_list);
    if(def && def->ref)
      setbitn(LDAP_F_REF, ldap_option_list);
    if(def && def->nosub)
      setbitn(LDAP_F_NOSUB, ldap_option_list);
    if(def && def->tls)
      setbitn(LDAP_F_TLS, ldap_option_list);
    if(def && def->tlsmust)
      setbitn(LDAP_F_TLSMUST, ldap_option_list);

    /* save the old opt_screen before calling scroll screen again */
    saved_screen = opt_screen;

    indent = utf8_width(c_s_f) + 3;
    rindent = 12;

    /* Server */
    new_confline(&ctmp);
    ctmp->help_title= _("HELP FOR LDAP SERVER");
    ctmp->var       = &server_var;
    ctmp->valoffset = indent;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_ldap_server;
    ctmp->tool      = dir_edit_tool;
    utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", indent-3,indent-3,server_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);

    first_line = ctmp;

    /* Search Base */
    new_confline(&ctmp);
    ctmp->help_title= _("HELP FOR SERVER SEARCH BASE");
    ctmp->var       = &base_var;
    ctmp->valoffset = indent;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_ldap_base;
    ctmp->tool      = dir_edit_tool;
    utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", indent-3,indent-3,base_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);

    /* Port */
    new_confline(&ctmp);
    ctmp->help_title= _("HELP FOR PORT NUMBER");
    ctmp->var       = &port_var;
    ctmp->valoffset = indent;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_ldap_port;
    ctmp->tool      = dir_edit_tool;
    utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", indent-3,indent-3,port_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);
    ctmp->flags    |= CF_NUMBER;

    /* Bind DN (DN to bind to if needed) */
    new_confline(&ctmp);
    ctmp->help_title= _("HELP FOR SERVER BIND DN");
    ctmp->var       = &binddn_var;
    ctmp->valoffset = indent;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_ldap_binddn;
    ctmp->tool      = dir_edit_tool;
    utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", indent-3,indent-3,binddn_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);

    /* Nickname */
    new_confline(&ctmp);
    ctmp->help_title= _("HELP FOR SERVER NICKNAME");
    ctmp->var       = &nick_var;
    ctmp->valoffset = indent;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_ldap_nick;
    ctmp->tool      = dir_edit_tool;
    utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", indent-3,indent-3,nick_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

    /* Options */
    new_confline(&ctmp);
    ctmp->var       = &opt_var;
    ctmp->keymenu   = &config_checkbox_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = NULL;
    snprintf(tmp, sizeof(tmp), "%s =", opt_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmpb = ctmp;
    ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);

    new_confline(&ctmp);
    ctmp->var       = NULL;
    ctmp->valoffset = rindent;
    ctmp->keymenu   = &config_checkbox_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = ldap_checkbox_tool;
    ctmp->varnamep  = ctmpb;
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr("Set    Feature Name");

    new_confline(&ctmp);
    ctmp->var       = NULL;
    ctmp->valoffset = rindent;
    ctmp->keymenu   = &config_checkbox_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = ldap_checkbox_tool;
    ctmp->varnamep  = ctmpb;
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr(set_choose);

    /*  find longest value's name */
    for(lv = 0, i = 0; (f = ldap_feature_list(i)); i++)
      if(lv < (j = utf8_width(f->name)))
	lv = j;
    
    lv = MIN(lv, 100);

    for(i = 0; (f = ldap_feature_list(i)); i++){
	new_confline(&ctmp);
	ctmp->var       = &opt_var;
	ctmp->help_title= _("HELP FOR LDAP FEATURES");
	ctmp->varnamep  = ctmpb;
	ctmp->keymenu   = &config_checkbox_keymenu;
	switch(i){
	  case LDAP_F_IMPL:
	    ctmp->help      = h_config_ldap_opts_impl;
	    break;
	  case LDAP_F_RHS:
	    ctmp->help      = h_config_ldap_opts_rhs;
	    break;
	  case LDAP_F_REF:
	    ctmp->help      = h_config_ldap_opts_ref;
	    break;
	  case LDAP_F_NOSUB:
	    ctmp->help      = h_config_ldap_opts_nosub;
	    break;
	  case LDAP_F_TLS:
	    ctmp->help      = h_config_ldap_opts_tls;
	    break;
	  case LDAP_F_TLSMUST:
	    ctmp->help      = h_config_ldap_opts_tlsmust;
	    break;
	}

	ctmp->tool      = ldap_checkbox_tool;
	ctmp->valoffset = rindent;
	ctmp->varmem    = i;
	utf8_snprintf(tmp, sizeof(tmp), "[%c]  %-*.*w", 
		bitnset(f->value, ldap_option_list) ? 'X' : ' ',
		lv, lv, f->name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->value     = cpystr(tmp);
    }

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

    /* Search Type */
    new_confline(&ctmp);
    ctmp->var       = &srch_type_var;
    ctmp->keymenu   = &config_radiobutton_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = NULL;
    snprintf(tmp, sizeof(tmp), "%s =", srch_type_var.name);
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
    ctmp->value     = cpystr("Set    Rule Values");

    new_confline(&ctmp);
    ctmp->var       = NULL;
    ctmp->valoffset = rindent;
    ctmp->keymenu   = &config_radiobutton_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = ldap_radiobutton_tool;
    ctmp->varnamep  = ctmpb;
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr(set_choose);

    /* find longest value's name */
    for(lv = 0, i = 0; (f = ldap_search_types(i)); i++)
      if(lv < (j = utf8_width(f->name)))
	lv = j;
    
    lv = MIN(lv, 100);
    
    for(i = 0; (f = ldap_search_types(i)); i++){
	new_confline(&ctmp);
	ctmp->help_title= _("HELP FOR SEARCH TYPE");
	ctmp->var       = &srch_type_var;
	ctmp->valoffset = rindent;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = h_config_ldap_searchtypes;
	ctmp->varmem    = i;
	ctmp->tool      = ldap_radiobutton_tool;
	ctmp->varnamep  = ctmpb;
	utf8_snprintf(tmp, sizeof(tmp), "(%c)  %-*.*w", (((!def || def->type == -1) &&
				        f->value == DEF_LDAP_TYPE) ||
				      (def && f->value == def->type))
				         ? R_SELD : ' ',
		lv, lv, f->name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->value     = cpystr(tmp);
    }

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;
    ctmp->varname   = cpystr("");

    /* Search Rule */
    new_confline(&ctmp);
    ctmp->var       = &srch_rule_var;
    ctmp->keymenu   = &config_radiobutton_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = NULL;
    snprintf(tmp, sizeof(tmp), "%s =", srch_rule_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmpb = ctmp;
    ctmp->flags    |= (CF_NOSELECT | CF_STARTITEM);

    /* Search Rule */
    new_confline(&ctmp);
    ctmp->var       = NULL;
    ctmp->valoffset = rindent;
    ctmp->keymenu   = &config_radiobutton_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = NULL;
    ctmp->varnamep  = ctmpb;
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr("Set    Rule Values");

    new_confline(&ctmp);
    ctmp->var       = NULL;
    ctmp->valoffset = rindent;
    ctmp->keymenu   = &config_radiobutton_keymenu;
    ctmp->help      = NO_HELP;
    ctmp->tool      = ldap_radiobutton_tool;
    ctmp->varnamep  = ctmpb;
    ctmp->flags    |= CF_NOSELECT;
    ctmp->value     = cpystr(set_choose);

    /* find longest value's name */
    for(lv = 0, i = 0; (f = ldap_search_rules(i)); i++)
      if(lv < (j = utf8_width(f->name)))
	lv = j;
    
    lv = MIN(lv, 100);
    
    for(i = 0; (f = ldap_search_rules(i)); i++){
	new_confline(&ctmp);
	ctmp->help_title= _("HELP FOR SEARCH RULE");
	ctmp->var       = &srch_rule_var;
	ctmp->valoffset = rindent;
	ctmp->keymenu   = &config_radiobutton_keymenu;
	ctmp->help      = h_config_ldap_searchrules;
	ctmp->varmem    = i;
	ctmp->tool      = ldap_radiobutton_tool;
	ctmp->varnamep  = ctmpb;
	utf8_snprintf(tmp, sizeof(tmp), "(%c)  %-*.*w", (((!def || def->srch == -1) &&
				        f->value == DEF_LDAP_SRCH) ||
				      (def && f->value == def->srch))
				         ? R_SELD : ' ',
		lv, lv, f->name);
	tmp[sizeof(tmp)-1] = '\0';
	ctmp->value     = cpystr(tmp);
    }

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;
    ctmp->varname   = cpystr("");

    /* Email attribute name */
    new_confline(&ctmp);
    ctmp->help_title= _("HELP FOR EMAIL ATTRIBUTE NAME");
    ctmp->var       = &mailattr_var;
    ctmp->valoffset = indent;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_ldap_email_attr;
    ctmp->tool      = dir_edit_tool;
    utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", indent-3,indent-3,mailattr_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);

    /* Name attribute name */
    new_confline(&ctmp);
    ctmp->help_title= _("HELP FOR NAME ATTRIBUTE NAME");
    ctmp->var       = &cnattr_var;
    ctmp->valoffset = indent;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_ldap_cn_attr;
    ctmp->tool      = dir_edit_tool;
    utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", indent-3,indent-3,cnattr_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);

    /* Surname attribute name */
    new_confline(&ctmp);
    ctmp->help_title= _("HELP FOR SURNAME ATTRIBUTE NAME");
    ctmp->var       = &snattr_var;
    ctmp->valoffset = indent;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_ldap_sn_attr;
    ctmp->tool      = dir_edit_tool;
    utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", indent-3,indent-3,snattr_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);

    /* Givenname attribute name */
    new_confline(&ctmp);
    ctmp->help_title= _("HELP FOR GIVEN NAME ATTRIBUTE NAME");
    ctmp->var       = &gnattr_var;
    ctmp->valoffset = indent;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_ldap_gn_attr;
    ctmp->tool      = dir_edit_tool;
    utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", indent-3,indent-3,gnattr_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;
    ctmp->varname   = cpystr("");

    /* Time limit */
    new_confline(&ctmp);
    ctmp->help_title= _("HELP FOR SERVER TIMELIMIT");
    ctmp->var       = &time_var;
    ctmp->valoffset = indent;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_ldap_time;
    ctmp->tool      = dir_edit_tool;
    utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", indent-3,indent-3,time_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);
    ctmp->flags    |= CF_NUMBER;

    /* Size limit */
    new_confline(&ctmp);
    ctmp->var       = &size_var;
    ctmp->help_title= _("HELP FOR SERVER SIZELIMIT");
    ctmp->valoffset = indent;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_ldap_size;
    ctmp->tool      = dir_edit_tool;
    utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", indent-3,indent-3,size_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);
    ctmp->flags    |= CF_NUMBER;

    /* Blank line */
    new_confline(&ctmp);
    ctmp->flags    |= CF_NOSELECT | CF_B_LINE;

    /* Custom Search Filter */
    new_confline(&ctmp);
    ctmp->help_title= _("HELP FOR CUSTOM SEARCH FILTER");
    ctmp->var       = &cust_var;
    ctmp->valoffset = indent;
    ctmp->keymenu   = &config_text_keymenu;
    ctmp->help      = h_config_ldap_cust;
    ctmp->tool      = dir_edit_tool;
    utf8_snprintf(tmp, sizeof(tmp), "%-*.*w =", indent-3,indent-3,cust_var.name);
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname   = cpystr(tmp);
    ctmp->varnamep  = ctmp;
    ctmp->value     = pretty_value(ps, ctmp);


    snprintf(tmp, sizeof(tmp), "%s DIRECTORY SERVER", title);
    tmp[sizeof(tmp)-1] = '\0';
    memset(&screen, 0, sizeof(screen));
    screen.ro_warning = saved_screen ? saved_screen->deferred_ro_warning : 0;
    /* TRANSLATORS: Print something1 using something2.
       servers is something1 */
    rv = conf_scroll_screen(ps, &screen, first_line, tmp, _("servers"), 0);

    /*
     * Now look at the fake variables and extract the information we
     * want from them.
     */

    if(rv == 1 && raw_server){
	char dir_tmp[2200], *p;
	int portval = -1, timeval = -1, sizeval = -1;

	apval = APVAL(&server_var, ew);
	server = *apval;
	*apval = NULL;

	apval = APVAL(&base_var, ew);
	base = *apval;
	*apval = NULL;

	apval = APVAL(&port_var, ew);
	port = *apval;
	*apval = NULL;

	apval = APVAL(&binddn_var, ew);
	binddn = *apval;
	*apval = NULL;

	apval = APVAL(&nick_var, ew);
	nick = *apval;
	*apval = NULL;

	apval = APVAL(&srch_type_var, ew);
	srch_type = *apval;
	*apval = NULL;

	apval = APVAL(&srch_rule_var, ew);
	srch_rule = *apval;
	*apval = NULL;

	apval = APVAL(&time_var, ew);
	ttime = *apval;
	*apval = NULL;

	apval = APVAL(&size_var, ew);
	ssize = *apval;
	*apval = NULL;

	apval = APVAL(&cust_var, ew);
	cust = *apval;
	*apval = NULL;

	apval = APVAL(&mailattr_var, ew);
	mailattr = *apval;
	*apval = NULL;

	apval = APVAL(&snattr_var, ew);
	snattr = *apval;
	*apval = NULL;

	apval = APVAL(&gnattr_var, ew);
	gnattr = *apval;
	*apval = NULL;

	apval = APVAL(&cnattr_var, ew);
	cnattr = *apval;
	*apval = NULL;

	if(server)
	  removing_leading_and_trailing_white_space(server);

	if(base){
	    removing_leading_and_trailing_white_space(base);
	    (void)removing_double_quotes(base);
	    p = add_backslash_escapes(base);
	    fs_give((void **)&base);
	    base = p;
	}

	if(port){
	    removing_leading_and_trailing_white_space(port);
	    if(*port)
	      portval = atoi(port);
	}
	
	if(binddn){
	    removing_leading_and_trailing_white_space(binddn);
	    (void)removing_double_quotes(binddn);
	    p = add_backslash_escapes(binddn);
	    fs_give((void **)&binddn);
	    binddn = p;
	}

	if(nick){
	    removing_leading_and_trailing_white_space(nick);
	    (void)removing_double_quotes(nick);
	    p = add_backslash_escapes(nick);
	    fs_give((void **)&nick);
	    nick = p;
	}

	if(ttime){
	    removing_leading_and_trailing_white_space(ttime);
	    if(*ttime)
	      timeval = atoi(ttime);
	}
	
	if(ssize){
	    removing_leading_and_trailing_white_space(ssize);
	    if(*ssize)
	      sizeval = atoi(ssize);
	}
	
	if(cust){
	    removing_leading_and_trailing_white_space(cust);
	    p = add_backslash_escapes(cust);
	    fs_give((void **)&cust);
	    cust = p;
	}

	if(mailattr){
	    removing_leading_and_trailing_white_space(mailattr);
	    p = add_backslash_escapes(mailattr);
	    fs_give((void **)&mailattr);
	    mailattr = p;
	}

	if(snattr){
	    removing_leading_and_trailing_white_space(snattr);
	    p = add_backslash_escapes(snattr);
	    fs_give((void **)&snattr);
	    snattr = p;
	}

	if(gnattr){
	    removing_leading_and_trailing_white_space(gnattr);
	    p = add_backslash_escapes(gnattr);
	    fs_give((void **)&gnattr);
	    gnattr = p;
	}

	if(cnattr){
	    removing_leading_and_trailing_white_space(cnattr);
	    p = add_backslash_escapes(cnattr);
	    fs_give((void **)&cnattr);
	    cnattr = p;
	}

	/*
	 * Don't allow user to edit scope but if one is present then we
	 * leave it (so they could edit it by hand).
	 */
	if(def && def->scope != -1 && def->scope != DEF_LDAP_SCOPE){
	    NAMEVAL_S *v;

	    v = ldap_search_scope(def->scope);
	    if(v){
		snprintf(custom_scope, sizeof(custom_scope), "/scope=%s", v->name);
		custom_scope[sizeof(custom_scope)-1] = '\0';
	    }
	}

	snprintf(dir_tmp, sizeof(dir_tmp), "%s%s%s \"/base=%s/binddn=%s/impl=%d/rhs=%d/ref=%d/nosub=%d/tls=%d/tlsm=%d/type=%s/srch=%s%s/time=%s/size=%s/cust=%s/nick=%s/matr=%s/catr=%s/satr=%s/gatr=%s\"",
		server ? server : "",
		(portval >= 0 && port && *port) ? ":" : "",
		(portval >= 0 && port && *port) ? port : "",
		base ? base : "",
		binddn ? binddn : "",
		bitnset(LDAP_F_IMPL, ldap_option_list) ? 1 : 0,
		bitnset(LDAP_F_RHS, ldap_option_list) ? 1 : 0,
		bitnset(LDAP_F_REF, ldap_option_list) ? 1 : 0,
		bitnset(LDAP_F_NOSUB, ldap_option_list) ? 1 : 0,
		bitnset(LDAP_F_TLS, ldap_option_list) ? 1 : 0,
		bitnset(LDAP_F_TLSMUST, ldap_option_list) ? 1 : 0,
		srch_type ? srch_type : "",
		srch_rule ? srch_rule : "",
		custom_scope,
		(timeval >= 0 && ttime && *ttime) ? ttime : "",
		(sizeval >= 0 && ssize && *ssize) ? ssize : "",
		cust ? cust : "",
		nick ? nick : "",
		mailattr ? mailattr : "",
		cnattr ? cnattr : "",
		snattr ? snattr : "",
		gnattr ? gnattr : "");
	dir_tmp[sizeof(dir_tmp)-1] = '\0';
	
	*raw_server = cpystr(dir_tmp);
    }

    for(j = 0; varlist[j]; j++){
	v = varlist[j];
	if(v->current_val.p)
	  fs_give((void **)&v->current_val.p);
	if(v->global_val.p)
	  fs_give((void **)&v->global_val.p);
	if(v->main_user_val.p)
	  fs_give((void **)&v->main_user_val.p);
	if(v->post_user_val.p)
	  fs_give((void **)&v->post_user_val.p);
	if(v->name)
	  fs_give((void **)&v->name);
    }

    if(server)
      fs_give((void **)&server);
    if(base)
      fs_give((void **)&base);
    if(port)
      fs_give((void **)&port);
    if(binddn)
      fs_give((void **)&binddn);
    if(nick)
      fs_give((void **)&nick);
    if(srch_type)
      fs_give((void **)&srch_type);
    if(srch_rule)
      fs_give((void **)&srch_rule);
    if(ttime)
      fs_give((void **)&ttime);
    if(ssize)
      fs_give((void **)&ssize);
    if(mailattr)
      fs_give((void **)&mailattr);
    if(cnattr)
      fs_give((void **)&cnattr);
    if(snattr)
      fs_give((void **)&snattr);
    if(gnattr)
      fs_give((void **)&gnattr);
    if(cust)
      fs_give((void **)&cust);

    opt_screen = saved_screen;
    ps->mangled_screen = 1;
    return(rv);
}


/*
 * Just calls text_tool except for intercepting MC_EXIT.
 */
int
dir_edit_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    if(cmd == MC_EXIT)
      return(config_exit_cmd(flags));
    else
      return(text_tool(ps, cmd, cl, flags));
}


/*
 * Delete LDAP directory entry
 */
void
dir_config_del(struct pine *ps, CONF_S **cl)
{
    char    prompt[81];
    int     rv = 0, i;

    if(fixed_var((*cl)->var, NULL, NULL)){
	if((*cl)->var->post_user_val.l || (*cl)->var->main_user_val.l){
	    if(want_to(_("Delete (unused) directory servers "),
		       'n', 'n', NO_HELP, WT_FLUSH_IN) == 'y'){
		rv = 1;
		delete_user_vals((*cl)->var);
	    }
	}
	else
	  q_status_message(SM_ORDER, 3, 3,
			   _("Can't delete sys-admin defined value"));
    }
    else{
	int cnt, ans = 0, no_ex;
	char  **new_list, **lval, **nelval;

	no_ex = (ps_global->ew_for_except_vars == Main);

	/* This can't happen, intercepted at caller by first_one case */
	nelval = no_ex ? (*cl)->var->current_val.l : LVAL((*cl)->var, ew);
	lval = LVAL((*cl)->var, ew);
	if(lval && lval[0] && lval[0][0] == '\0')
	  ans = 'r';

	/* how many servers defined? */
	for(cnt = 0; nelval[cnt]; cnt++)
	  ;

	/*
	 * If using default and there is more than one in list, ask if user
	 * wants to ignore them all or delete just this one. If just this
	 * one, copy rest to user_val. If ignore all, copy "" to user_val
	 * to override.
	 */
	if(!lval && cnt > 1){
	    static ESCKEY_S opts[] = {
		{'i', 'i', "I", N_("Ignore All")},
		{'r', 'r', "R", N_("Remove One")},
		{-1, 0, NULL, NULL}};
	    ans = radio_buttons(
	_("Ignore all default directory servers or just remove this one ? "),
				-FOOTER_ROWS(ps), opts, 'i', 'x',
				h_ab_del_dir_ignore, RB_NORM);
	}

	if(ans == 0){
	    snprintf(prompt, sizeof(prompt), _("Really delete %s \"%s\" from directory servers "),
		  ((*cl)->value && *(*cl)->value)
		      ? "server"
		      : "item",
		  ((*cl)->value && *(*cl)->value)
		      ? (*cl)->value
		      : int2string((*cl)->varmem + 1));
	    prompt[sizeof(prompt)-1] = '\0';
	}
	

	ps->mangled_footer = 1;
	if(ans == 'i'){
	    rv = ps->mangled_body = 1;

	    /*
	     * Ignore all of default by adding an empty string. Make it
	     * look just like there are no servers defined.
	     */

	    new_list = (char **)fs_get((1 + 1) * sizeof(char *));
	    new_list[0] = cpystr("");
	    new_list[1] = NULL;
	    set_variable_list(V_LDAP_SERVERS, new_list, FALSE, ew);
	    free_list_array(&new_list);
	    *cl = first_confline(*cl);
	    free_conflines(cl);
	    opt_screen->top_line = NULL;

	    add_ldap_fake_first_server(ps, cl, &ps->vars[V_LDAP_SERVERS],
				       &dir_conf_km, h_direct_config,
				       dir_config_tool);
	}
	else if(ans == 'r' ||
	       (ans != 'x' &&
	        want_to(prompt, 'n', 'n', NO_HELP, WT_FLUSH_IN) == 'y')){
	    CONF_S *cp;
	    char    **servers;
	    int       move_top = 0, this_one, revert_to_default,
		      default_there_to_revert_to;

	    /*
	     * Remove one from current list.
	     */

	    rv = ps->mangled_body = 1;

	    this_one = (*cl)->varmem;

	    /* might have to re-adjust screen to see new current */
	    move_top = (this_one > 0) &&
		       (this_one == cnt - 1) &&
		       (((*cl)    == opt_screen->top_line) ||
		        ((*cl)->prev == opt_screen->top_line) ||
		        ((*cl)->prev->prev == opt_screen->top_line));

	    /*
	     * If this is last one and there is a default available, revert
	     * to it.
	     */
	    revert_to_default = ((cnt == 1) && lval);
	    if(cnt > 1){
		new_list = (char **)fs_get((cnt + 1) * sizeof(char *));
		for(i = 0; i < this_one; i++)
		  new_list[i] = cpystr(nelval[i]);

		for(i = this_one; i < cnt; i++)
		  new_list[i] = cpystr(nelval[i+1]);

		set_variable_list(V_LDAP_SERVERS, new_list, FALSE, ew);
		free_list_array(&new_list);
	    }
	    else if(revert_to_default){
		char ***alval;

		alval = ALVAL((*cl)->var, ew);
		if(alval && *alval)
		  free_list_array(alval);
	    }
	    else{
		/* cnt is one and we want to hide default */
		new_list = (char **)fs_get((1 + 1) * sizeof(char *));
		new_list[0] = cpystr("");
		new_list[1] = NULL;
		set_variable_list(V_LDAP_SERVERS, new_list, FALSE, ew);
		free_list_array(&new_list);
	    }
		
	    if(cnt == 1){	/* delete display line for this_one */
		if(revert_to_default){
		    servers = (*cl)->var->global_val.l;
		    default_there_to_revert_to = (servers != NULL);
		}

		*cl = first_confline(*cl);
		free_conflines(cl);
		opt_screen->top_line = NULL;
		if(revert_to_default && default_there_to_revert_to){
		    CONF_S   *first_line = NULL;

		    q_status_message(SM_ORDER, 0, 3,
				     _("Reverting to default directory server"));
		    dir_init_display(ps, cl, servers,
				     &ps->vars[V_LDAP_SERVERS], &first_line);
		    *cl = first_line;
		}
		else{
		    add_ldap_fake_first_server(ps, cl,
					       &ps->vars[V_LDAP_SERVERS],
					       &dir_conf_km, h_direct_config,
					       dir_config_tool);
		}
	    }
	    else if(this_one == cnt - 1){	/* deleted last one */
		/* back up and delete it */
		*cl = (*cl)->prev;
		free_conflines(&(*cl)->next);
		/* now back up to first line of this server */
		*cl = (*cl)->prev->prev;
		if(move_top)
		  opt_screen->top_line = *cl;
	    }
	    else{			/* deleted one out of the middle */
		if(*cl == opt_screen->top_line)
		  opt_screen->top_line = (*cl)->next->next->next;

		cp = *cl;
		*cl = (*cl)->next;	/* move to next line, then */
		snip_confline(&cp);	/* snip 1st deleted line   */
		cp = *cl;
		*cl = (*cl)->next;	/* move to next line, then */
		snip_confline(&cp);	/* snip 2nd deleted line   */
		cp = *cl;
		*cl = (*cl)->next;	/* move to next line, then */
		snip_confline(&cp);	/* snip 3rd deleted line   */
		/* adjust varmems */
		for(cp = *cl; cp; cp = cp->next)
		  cp->varmem--;
	    }
	}
	else
	  q_status_message(SM_ORDER, 0, 3, _("Server not deleted"));
    }

    if(rv == 1){
	set_current_val((*cl)->var, TRUE, FALSE);
	fix_side_effects(ps, (*cl)->var, 0);
	write_pinerc(ps, ew, WRP_NONE);
    }
}


/*
 * Utility routine to help set up display
 */
void
add_ldap_fake_first_server(struct pine *ps, CONF_S **ctmp, struct variable *var,
			   struct key_menu *km, HelpType help,
			   int (*tool)(struct pine *, int, CONF_S **, unsigned))
{
    new_confline(ctmp);
    (*ctmp)->help_title= _("HELP FOR DIRECTORY SERVER CONFIGURATION");
    (*ctmp)->value     = cpystr(ADD_FIRST_LDAP_SERVER);
    (*ctmp)->var       = var;
    (*ctmp)->varmem    = 0;
    (*ctmp)->keymenu   = km;
    (*ctmp)->help      = help;
    (*ctmp)->tool      = tool;
    (*ctmp)->valoffset = 2;
}


/*
 * Add an ldap server to the display list.
 *
 * Args  before -- Insert it before current, else append it after.
 */
void
add_ldap_server_to_display(struct pine *ps, CONF_S **ctmp, char *serv, char *subtitle,
			   struct variable *var, int member, struct key_menu *km,
			   HelpType help,
			   int (*tool)(struct pine *, int, CONF_S **, unsigned),
			   int before, CONF_S **first_line)
{
    new_confline(ctmp);
    if(first_line)
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

    (*ctmp)->help_title= _("HELP FOR DIRECTORY SERVER CONFIGURATION");
    (*ctmp)->value     = serv;
    (*ctmp)->var       = var;
    (*ctmp)->varmem    = member;
    (*ctmp)->keymenu   = km;
    (*ctmp)->help      = help;
    (*ctmp)->tool      = tool;
    (*ctmp)->flags    |= CF_STARTITEM;
    (*ctmp)->valoffset = 4;

    new_confline(ctmp);
    (*ctmp)->value     = subtitle;
    (*ctmp)->keymenu   = km;
    (*ctmp)->help      = help;
    (*ctmp)->tool      = tool;
    (*ctmp)->flags    |= CF_NOSELECT;
    (*ctmp)->valoffset = 8;

    new_confline(ctmp);
    (*ctmp)->keymenu   = km;
    (*ctmp)->help      = help;
    (*ctmp)->tool      = tool;
    (*ctmp)->flags    |= CF_NOSELECT | CF_B_LINE;
    (*ctmp)->valoffset = 0;
}


/*
 * ldap option list manipulation tool
 * 
 * 
 * returns:  -1 on unrecognized cmd, 0 if no change, 1 if change
 */
int
ldap_checkbox_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int  rv = 0;

    switch(cmd){
      case MC_TOGGLE:				/* mark/unmark option */
	rv = 1;
	toggle_ldap_option_bit(ps, (*cl)->varmem, (*cl)->var, (*cl)->value);
	break;

      case MC_EXIT:				 /* exit */
        rv = config_exit_cmd(flags);
	break;

      default :
	rv = -1;
	break;
    }

    return(rv);
}


void
toggle_ldap_option_bit(struct pine *ps, int index, struct variable *var, char *value)
{
    NAMEVAL_S  *f;

    f  = ldap_feature_list(index);

    /* flip the bit */
    if(bitnset(f->value, ldap_option_list))
      clrbitn(f->value, ldap_option_list);
    else
      setbitn(f->value, ldap_option_list);

    if(value)
      value[1] = bitnset(f->value, ldap_option_list) ? 'X' : ' ';
}


NAMEVAL_S *
ldap_feature_list(int index)
{
    static NAMEVAL_S ldap_feat_list[] = {
	{"use-implicitly-from-composer",      NULL, LDAP_F_IMPL},
	{"lookup-addrbook-contents",          NULL, LDAP_F_RHS},
	{"save-search-criteria-not-result",   NULL, LDAP_F_REF},
	{"disable-ad-hoc-space-substitution", NULL, LDAP_F_NOSUB},
	{"attempt-tls-on-connection",         NULL, LDAP_F_TLS},
	{"require-tls-on-connection",         NULL, LDAP_F_TLSMUST}
    };

    return((index >= 0 &&
	    index < (sizeof(ldap_feat_list)/sizeof(ldap_feat_list[0])))
		   ? &ldap_feat_list[index] : NULL);
}


/*
 * simple radio-button style variable handler
 */
int
ldap_radiobutton_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int	       rv = 0;
    CONF_S    *ctmp;
    NAMEVAL_S *rule;
    char     **apval;

    switch(cmd){
      case MC_CHOICE :				/* set/unset feature */

	/* hunt backwards, turning off old values */
	for(ctmp = *cl; ctmp && !(ctmp->flags & CF_NOSELECT) && !ctmp->varname;
	    ctmp = prev_confline(ctmp))
	  ctmp->value[1] = ' ';

	/* hunt forwards, turning off old values */
	for(ctmp = *cl; ctmp && !(ctmp->flags & CF_NOSELECT) && !ctmp->varname;
	    ctmp = next_confline(ctmp))
	  ctmp->value[1] = ' ';

	/* turn on current value */
	(*cl)->value[1] = R_SELD;

	if((*cl)->var == ldap_srch_rule_ptr)
	  rule = ldap_search_rules((*cl)->varmem);
	else
	  rule = ldap_search_types((*cl)->varmem);

	apval = APVAL((*cl)->var, ew);
	if(apval && *apval)
	  fs_give((void **)apval);

	if(apval)
	  *apval = cpystr(rule->name);

	ps->mangled_body = 1;	/* BUG: redraw it all for now? */
	rv = 1;

	break;

      case MC_EXIT:				/* exit */
	rv = config_exit_cmd(flags);
	break;

      default :
	rv = -1;
	break;
    }

    return(rv);
}

#endif	/* ENABLE_LDAP */
