#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: context.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
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

#include "headers.h"
#include "context.h"
#include "confscroll.h"
#include "status.h"
#include "folder.h"
#include "radio.h"
#include "alpine.h"
#include "mailindx.h"
#include "mailcmd.h"
#include "send.h"
#include "../pith/list.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/thread.h"


/*
 * Internal prototypes
 */
int      context_config_tool(struct pine *, int, CONF_S **, unsigned);
int      context_config_add(struct pine *, CONF_S **);
int      context_config_shuffle(struct pine *, CONF_S **);
int      context_config_edit(struct pine *, CONF_S **);
int      context_config_delete(struct pine *, CONF_S **);
int      ccs_var_delete(struct pine *, CONTEXT_S *);
int      ccs_var_insert(struct pine *, char *, struct variable *, char **, int);
int	 context_select_tool(struct pine *, int, CONF_S **, unsigned);


/*
 * Setup CollectionLists. Build a context list on the fly from the config
 * variable and edit it. This won't include Incoming-Folders because that
 * is a pseudo collection, but that's ok since we can't do the operations
 * on it, anyway. Reset real config list at the end.
 */
void
context_config_screen(struct pine *ps, CONT_SCR_S *cs, int edit_exceptions)
{
    CONTEXT_S    *top, **clist, *cp;
    CONF_S	 *ctmpa, *first_line, *heading;
    OPT_SCREEN_S  screen;
    int           i, readonly_warning, some_defined, ret;
    int           reinit_contexts = 0, prime;
    char        **lval, **lval1, **lval2, ***alval;
    struct variable fake_fspec_var, fake_nspec_var;
    struct variable *fake_fspec, *fake_nspec;

    ew = edit_exceptions ? ps_global->ew_for_except_vars : Main;

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

go_again:
    top = NULL; ctmpa = NULL; first_line = NULL;
    some_defined = 0, prime = 0;
    fake_fspec = &fake_fspec_var;
    fake_nspec = &fake_nspec_var;
    memset((void *)fake_fspec, 0, sizeof(*fake_fspec));
    memset((void *)fake_nspec, 0, sizeof(*fake_nspec));

    /* so fixed_var() will work right */
    fake_fspec->is_list = 1;
    fake_nspec->is_list = 1;
    if((ps->vars[V_FOLDER_SPEC]).is_fixed){
	fake_fspec->is_fixed = 1;
	if((ps->vars[V_FOLDER_SPEC]).fixed_val.l
	   && (ps->vars[V_FOLDER_SPEC]).fixed_val.l[0]){
	    fake_fspec->fixed_val.l = (char **) fs_get(2 * sizeof(char *));
	    fake_fspec->fixed_val.l[0]
			    = cpystr((ps->vars[V_FOLDER_SPEC]).fixed_val.l[0]);
	    fake_fspec->fixed_val.l[1] = NULL;
	}
    }

    if((ps->vars[V_NEWS_SPEC]).is_fixed){
	fake_nspec->is_fixed = 1;
	if((ps->vars[V_NEWS_SPEC]).fixed_val.l
	   && (ps->vars[V_NEWS_SPEC]).fixed_val.l[0]){
	    fake_nspec->fixed_val.l = (char **) fs_get(2 * sizeof(char *));
	    fake_nspec->fixed_val.l[0] = cpystr(INHERIT);
	    fake_nspec->fixed_val.l[0]
			    = cpystr((ps->vars[V_NEWS_SPEC]).fixed_val.l[0]);
	    fake_nspec->fixed_val.l[1] = NULL;
	}
    }

    clist = &top;
    lval1 = LVAL(&ps->vars[V_FOLDER_SPEC], ew);
    lval2 = LVAL(&ps->vars[V_NEWS_SPEC], ew);

    alval = ALVAL(fake_fspec, ew);
    if(lval1)
      *alval = copy_list_array(lval1);
    else if(!edit_exceptions && ps->VAR_FOLDER_SPEC && ps->VAR_FOLDER_SPEC[0] &&
	    ps->VAR_FOLDER_SPEC[0][0])
      *alval = copy_list_array(ps->VAR_FOLDER_SPEC);
    else
      fake_fspec = NULL;

    if(fake_fspec){
	lval = LVAL(fake_fspec, ew);
	for(i = 0; lval && lval[i]; i++){
	    cp = NULL;
	    if(i == 0 && !strcmp(lval[i], INHERIT)){
		cp = (CONTEXT_S *)fs_get(sizeof(*cp));
		memset((void *)cp, 0, sizeof(*cp));
		cp->use = CNTXT_INHERIT;
		cp->label = cpystr("Default collections are inherited");
	    }
	    else if((cp = new_context(lval[i], &prime)) != NULL){
		cp->var.v = fake_fspec;
		cp->var.i = i;
	    }
	    
	    if(cp){
		*clist    = cp;			/* add it to list   */
		clist     = &cp->next;		/* prepare for next */
	    }
	}

	set_current_val(fake_fspec, FALSE, FALSE);
    }

    alval = ALVAL(fake_nspec, ew);
    if(lval2)
      *alval = copy_list_array(lval2);
    else if(!edit_exceptions && ps->VAR_NEWS_SPEC && ps->VAR_NEWS_SPEC[0] &&
	    ps->VAR_NEWS_SPEC[0][0])
      *alval = copy_list_array(ps->VAR_NEWS_SPEC);
    else
      fake_nspec = NULL;

    if(fake_nspec){
	lval = LVAL(fake_nspec, ew);
	for(i = 0; lval && lval[i]; i++){
	    cp = NULL;
	    if(i == 0 && !strcmp(lval[i], INHERIT)){
		cp = (CONTEXT_S *)fs_get(sizeof(*cp));
		memset((void *)cp, 0, sizeof(*cp));
		cp->use = CNTXT_INHERIT;
		cp->label = cpystr("Default collections are inherited");
	    }
	    else if((cp = new_context(lval[i], &prime)) != NULL){
		cp->var.v = fake_nspec;
		cp->var.i = i;
	    }
	    
	    if(cp){
		*clist    = cp;			/* add it to list   */
		clist     = &cp->next;		/* prepare for next */
	    }
	}

	set_current_val(fake_nspec, FALSE, FALSE);
    }
    
    for(cp = top; cp; cp = cp->next)
      if(!(cp->use & CNTXT_INHERIT)){
	  some_defined++;
	  break;
      }

    if(edit_exceptions && !some_defined){
	q_status_message(SM_ORDER, 3, 7,
 _("No exceptions to edit. First collection exception must be set by editing file"));
	free_contexts(&top);
	if(reinit_contexts){
	    free_contexts(&ps_global->context_list);
	    init_folders(ps_global);
	}

	return;
    }


    /* fix up prev pointers */
    for(cp = top; cp; cp = cp->next)
      if(cp->next)
	cp->next->prev = cp;

    new_confline(&ctmpa);		/* blank line */
    ctmpa->keymenu    = cs->keymenu;
    ctmpa->help       = cs->help.text;
    ctmpa->help_title = cs->help.title;
    ctmpa->tool       = context_config_tool;
    ctmpa->flags     |= (CF_NOSELECT | CF_B_LINE);

    for(cp = top; cp; cp = cp->next){
	new_confline(&ctmpa);
	heading		  = ctmpa;
	if(!(cp->use & CNTXT_INHERIT))
	  ctmpa->value	  = cpystr(cp->nickname ? cp->nickname : cp->context);

	ctmpa->var	  = cp->var.v;
	ctmpa->keymenu    = cs->keymenu;
	ctmpa->help       = cs->help.text;
	ctmpa->help_title = cs->help.title;
	ctmpa->tool       = context_config_tool;
	ctmpa->flags	 |= CF_STARTITEM;
	ctmpa->valoffset  = 4;
	ctmpa->d.c.ct     = cp;
	ctmpa->d.c.cs	  = cs;
	if(cp->use & CNTXT_INHERIT)
	  ctmpa->flags |= CF_INHERIT | CF_NOSELECT;

	if((!first_line && !(cp->use & CNTXT_INHERIT)) ||
	   (!(cp->use & CNTXT_INHERIT) &&
	    cp->var.v &&
	    (cs->starting_var == cp->var.v) &&
	    (cs->starting_varmem == cp->var.i)))
	  first_line = ctmpa;

	/* Add explanatory text */
	new_confline(&ctmpa);
	ctmpa->value	  = cpystr(cp->label ? cp->label : "* * *");
	ctmpa->keymenu	  = cs->keymenu;
	ctmpa->help	  = cs->help.text;
	ctmpa->help_title = cs->help.title;
	ctmpa->tool	  = context_config_tool;
	ctmpa->flags	 |= CF_NOSELECT;
	ctmpa->valoffset  = 8;

	/* Always add blank line, make's shuffling a little easier */
	new_confline(&ctmpa);
	heading->headingp  = ctmpa;		/* use headingp to mark end */
	ctmpa->keymenu	   = cs->keymenu;
	ctmpa->help	   = cs->help.text;
	ctmpa->help_title  = cs->help.title;
	ctmpa->tool	   = context_config_tool;
	ctmpa->flags	  |= CF_NOSELECT | CF_B_LINE;
	ctmpa->valoffset   = 0;
    }

    cs->starting_var = NULL;


    memset(&screen, 0, sizeof(screen));
    screen.ro_warning = readonly_warning;
    ret = conf_scroll_screen(ps, &screen, first_line, cs->title,
			     cs->print_string, 0);

    free_contexts(&top);
    
    if(ret)
      reinit_contexts++;

    /*
     * 15 means the tool wants us to reset and go again. The config var
     * has been changed. The contexts will be built again from the
     * config variable and the real contexts will be rebuilt below.
     * This is easier and safer than having the tools rebuild the context
     * list and the display correct. It's difficult to do because of all
     * the inheriting and defaulting going on.
     */
    if(ret == 15){
	if(edit_exceptions && !LVAL(fake_fspec, ew) && !LVAL(fake_nspec, ew)){
	    if(want_to(_("Really delete last exceptional collection"),
		       'n', 'n', h_config_context_del_except,
		       WT_FLUSH_IN) != 'y'){
		free_variable_values(fake_fspec);
		free_variable_values(fake_nspec);
		goto go_again;
	    }
	}

	/* resolve variable changes */
	if((lval1 && !equal_list_arrays(lval1, LVAL(fake_fspec, ew))) ||
	   (fake_fspec && !equal_list_arrays(ps->VAR_FOLDER_SPEC,
					     LVAL(fake_fspec, ew)))){
	    i = set_variable_list(V_FOLDER_SPEC,
				  LVAL(fake_fspec, ew), TRUE, ew);
	    set_current_val(&ps->vars[V_FOLDER_SPEC], TRUE, FALSE);

	    if(i)
	      q_status_message(SM_ORDER, 3, 3,
			       _("Trouble saving change, cancelled"));
	    else if(!edit_exceptions && lval1 && !LVAL(fake_fspec, ew)){
		cs->starting_var = fake_fspec;
		cs->starting_varmem = 0;
		q_status_message(SM_ORDER, 3, 3,
		       _("Deleted last Folder-Collection, reverting to default"));
	    }
	    else if(!edit_exceptions && !lval1 && !LVAL(fake_fspec, ew)){
		cs->starting_var = fake_fspec;
		cs->starting_varmem = 0;
		q_status_message(SM_ORDER, 3, 3,
	       _("Deleted default Folder-Collection, reverting back to default"));
	    }
	}

	if((lval2 && !equal_list_arrays(lval2, LVAL(fake_nspec, ew))) ||
	   (fake_nspec && !equal_list_arrays(ps->VAR_NEWS_SPEC,
					     LVAL(fake_nspec, ew)))){
	    i = set_variable_list(V_NEWS_SPEC,
				  LVAL(fake_nspec, ew), TRUE, ew);
	    set_news_spec_current_val(TRUE, FALSE);

	    if(i)
	      q_status_message(SM_ORDER, 3, 3,
			       _("Trouble saving change, cancelled"));
	    else if(!edit_exceptions && lval2 && !LVAL(fake_nspec, ew) &&
		    ps->VAR_NEWS_SPEC && ps->VAR_NEWS_SPEC[0] &&
		    ps->VAR_NEWS_SPEC[0][0]){
		cs->starting_var = fake_nspec;
		cs->starting_varmem = 0;
		q_status_message(SM_ORDER, 3, 3,
		       _("Deleted last News-Collection, reverting to default"));
	    }
	    else if(!edit_exceptions && !lval2 && !LVAL(fake_nspec, ew) &&
		    ps->VAR_NEWS_SPEC && ps->VAR_NEWS_SPEC[0] &&
		    ps->VAR_NEWS_SPEC[0][0]){
		cs->starting_var = fake_nspec;
		cs->starting_varmem = 0;
	        q_status_message(SM_ORDER, 3, 3,
	         _("Deleted default News-Collection, reverting back to default"));
	    }
	}

	free_variable_values(fake_fspec);
	free_variable_values(fake_nspec);
	goto go_again;
    }

    ps->mangled_screen = 1;

    /* make the real context list match the changed config variables */
    if(reinit_contexts){
	free_contexts(&ps_global->context_list);
	init_folders(ps_global);
    }
}


/*----------------------------------------------------------------------
   Function to display/manage collections

 ----*/
CONTEXT_S *
context_select_screen(struct pine *ps, CONT_SCR_S *cs, int ro_warn)
{
    CONTEXT_S	 *cp;
    CONF_S	 *ctmpa = NULL, *first_line = NULL, *heading;
    OPT_SCREEN_S  screen, *saved_screen;
    int           readonly_warning = 0;

    /* restrict to normal config */
    ew = Main;

    if(!cs->edit)
      readonly_warning = 0;
    else if(ps->restricted)
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
	if(ro_warn && prc && prc->quit_to_edit){
	    quit_to_edit_msg(prc);
	    return(NULL);
	}
    }

    readonly_warning *= ro_warn;

    /*
     * Loop thru available contexts, setting up for display
     * (note: if no "cp" we're hosed.  should never happen ;)
     */
    for(cp = *cs->contexts; cp->prev; cp = cp->prev)
      ;
    
    /* delimiter for Mail Collections */
    new_confline(&ctmpa);		/* blank line */
    ctmpa->keymenu    = cs->keymenu;
    ctmpa->help       = cs->help.text;
    ctmpa->help_title = cs->help.title;
    ctmpa->tool       = context_select_tool;
    ctmpa->flags     |= CF_NOSELECT | CF_B_LINE;

    do{
	new_confline(&ctmpa);
	heading		  = ctmpa;
	ctmpa->value	  = cpystr(cp->nickname ? cp->nickname : cp->context);
	ctmpa->var	  = cp->var.v;
	ctmpa->keymenu    = cs->keymenu;
	ctmpa->help       = cs->help.text;
	ctmpa->help_title = cs->help.title;
	ctmpa->tool       = context_select_tool;
	ctmpa->flags	 |= CF_STARTITEM;
	ctmpa->valoffset  = 4;
	ctmpa->d.c.ct     = cp;
	ctmpa->d.c.cs	  = cs;

	if(!first_line || cp == cs->start)
	  first_line = ctmpa;

	/* Add explanatory text */
	new_confline(&ctmpa);
	ctmpa->value	  = cpystr(cp->label ? cp->label : "* * *");
	ctmpa->keymenu	  = cs->keymenu;
	ctmpa->help	  = cs->help.text;
	ctmpa->help_title = cs->help.title;
	ctmpa->tool	  = context_select_tool;
	ctmpa->flags	 |= CF_NOSELECT;
	ctmpa->valoffset  = 8;

	/* Always add blank line, make's shuffling a little easier */
	new_confline(&ctmpa);
	heading->headingp  = ctmpa;
	ctmpa->keymenu	   = cs->keymenu;
	ctmpa->help	   = cs->help.text;
	ctmpa->help_title  = cs->help.title;
	ctmpa->tool	   = context_select_tool;
	ctmpa->flags	  |= CF_NOSELECT | CF_B_LINE;
	ctmpa->valoffset   = 0;
    }
    while((cp = cp->next) != NULL);


    saved_screen = opt_screen;
    memset(&screen, 0, sizeof(screen));
    screen.ro_warning = readonly_warning;
    (void) conf_scroll_screen(ps, &screen, first_line, cs->title,
			      cs->print_string, 0);
    opt_screen = saved_screen;
    ps->mangled_screen = 1;
    return(cs->selected);
}


int
context_config_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int retval = 0;

    switch(cmd){
      case MC_DELETE :
	if(!fixed_var((*cl)->var, "delete", "collection"))
	  retval = context_config_delete(ps, cl);

	break;

      case MC_EDIT :
	if(!fixed_var((*cl)->var, "change", "collection"))
	  retval = context_config_edit(ps, cl);

	break;

      case MC_ADD :
	if(!fixed_var((*cl)->var, "add to", "collection"))
	  retval = context_config_add(ps, cl);

	break;

      case MC_SHUFFLE :
	if(!fixed_var((*cl)->var, "shuffle", "collection"))
	  retval = context_config_shuffle(ps, cl);

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


int
context_config_add(struct pine *ps, CONF_S **cl)
{
    char            *raw_ctxt;
    struct variable *var;
    char           **lval;
    int              count;

    if((raw_ctxt = context_edit_screen(ps, "ADD", NULL, NULL, NULL, NULL)) != NULL){

	/*
	 * If var is non-NULL we add to the end of that var.
	 * If it is NULL, that means we're adding to the current_val, so
	 * we'll have to soak up the default values from there into our
	 * own variable.
	 */
	if((*cl)->var){
	    var = (*cl)->var;
	    lval = LVAL((*cl)->var, ew);
	}
	else{
	    q_status_message(SM_ORDER|SM_DING, 3, 3,
			     "Programmer botch in context_config_add");
	    return(0);
	}

	for(count = 0; lval && lval[count]; count++)
	  ;

	if(!ccs_var_insert(ps, raw_ctxt, var, lval, count)){
	    fs_give((void **)&raw_ctxt);
	    q_status_message(SM_ORDER|SM_DING, 3, 3,
			     _("Error adding new collection"));
	    return(0);
	}

	fs_give((void **)&raw_ctxt);

	(*cl)->d.c.cs->starting_var = var;
	(*cl)->d.c.cs->starting_varmem = count;
	q_status_message(SM_ORDER, 0, 3,
			 _("New collection added.  Use \"$\" to adjust order."));
	return(15);
    }

    ps->mangled_screen = 1;
    return(0);
}


int
context_config_shuffle(struct pine *ps, CONF_S **cl)
{
    char      prompt[256];
    int	      n = 0, cmd, i1, i2, count = 0, insert_num, starting_varmem;
    int       news_problem = 0, deefault = 0;
    ESCKEY_S  ekey[3];
    CONTEXT_S *cur_ctxt, *other_ctxt;
    char     *tmp, **lval, **lval1, **lval2;
    struct variable *cur_var, *other_var;

    if(!((*cl)->d.c.ct && (*cl)->var))
      return(0);

    /* enable UP? */
    if((*cl)->d.c.ct->prev && !((*cl)->d.c.ct->prev->use & CNTXT_INHERIT)){
	/*
	 * Don't allow shuffling news collection up to become
	 * the primary collection. That would happen if prev is primary
	 * and this one is news.
	 */
	if((*cl)->d.c.ct->prev->use & CNTXT_SAVEDFLT &&
	   (*cl)->d.c.ct->use & CNTXT_NEWS)
	  news_problem++;
	else{
	    ekey[n].ch      = 'u';
	    ekey[n].rval    = 'u';
	    ekey[n].name    = "U";
	    ekey[n++].label = N_("Up");
	    deefault        = 'u';
	}
    }

    /* enable DOWN? */
    if((*cl)->d.c.ct->next && !((*cl)->d.c.ct->next->use & CNTXT_INHERIT)){
	/*
	 * Don't allow shuffling down past news collection if this
	 * is primary collection.
	 */
	if((*cl)->d.c.ct->use & CNTXT_SAVEDFLT &&
	   (*cl)->d.c.ct->next->use & CNTXT_NEWS)
	  news_problem++;
	else{
	    ekey[n].ch      = 'd';
	    ekey[n].rval    = 'd';
	    ekey[n].name    = "D";
	    ekey[n++].label = N_("Down");
	    if(!deefault)
	      deefault = 'd';
	}
    }

    if(n){
	ekey[n].ch = -1;
	snprintf(prompt, sizeof(prompt), _("Shuffle selected context %s%s%s? "),
		(ekey[0].ch == 'u') ?  _("UP") : "",
		(n > 1) ? " or " : "",
		(ekey[0].ch == 'd' || n > 1) ? _("DOWN") : "");
	prompt[sizeof(prompt)-1] = '\0';

	cmd = radio_buttons(prompt, -FOOTER_ROWS(ps), ekey,
			    deefault, 'x', NO_HELP, RB_NORM);
	switch(cmd){
	  case 'x':
	  default:
	    cmd_cancelled("Shuffle");
	    return(0);
	  
	  case 'u':
	  case 'd':
	    break;
	}

	/*
	 * This is complicated by the fact that there are two
	 * vars involved, the folder-collections and the news-collections.
	 * We may have to shuffle across collections.
	 */
	cur_ctxt = (*cl)->d.c.ct;
	if(cmd == 'd')
	  other_ctxt = (*cl)->d.c.ct->next;
	else if(cmd == 'u')
	  other_ctxt = (*cl)->d.c.ct->prev;
	
	cur_var = cur_ctxt->var.v;
	other_var = other_ctxt->var.v;

	/* swap elements of config var */
	if(cur_var == other_var){
	    i1 = cur_ctxt->var.i;
	    i2 = other_ctxt->var.i;
	    lval = LVAL(cur_var, ew);
	    if(lval){
		tmp = lval[i1];
		lval[i1] = lval[i2];
		lval[i2] = tmp;
	    }

	    starting_varmem = i2;
	}
	else{
	    /* swap into the other_var */
	    i1 = cur_ctxt->var.i;
	    i2 = other_ctxt->var.i;
	    lval1 = LVAL(cur_var, ew);
	    lval2 = LVAL(other_var, ew);
	    /* count */
	    for(count = 0; lval2 && lval2[count]; count++)
	      ;
	    if(cmd == 'd')
	      insert_num = count ? 1 : 0;
	    else{
		insert_num = count ? count - 1 : count;
	    }

	    starting_varmem = insert_num;
	    if(ccs_var_insert(ps,lval1[i1],other_var,lval2,insert_num)){
		if(!ccs_var_delete(ps, cur_ctxt)){
		    q_status_message(SM_ORDER|SM_DING, 3, 3,
				     _("Error deleting shuffled context"));
		    return(0);
		}
	    }
	    else{
		q_status_message(SM_ORDER, 3, 3,
				 _("Trouble shuffling, cancelled"));
		return(0);
	    }
	}

	(*cl)->d.c.cs->starting_var = other_var;
	(*cl)->d.c.cs->starting_varmem = starting_varmem;

	q_status_message(SM_ORDER, 0, 3, _("Collections shuffled"));
	return(15);
    }

    if(news_problem)
      /* TRANSLATORS: Sorry, can't move news group collections above email collections */
      q_status_message(SM_ORDER, 0, 3, _("Sorry, cannot Shuffle news to top"));
    else
      q_status_message(SM_ORDER, 0, 3, _("Sorry, nothing to Shuffle"));

    return(0);
}


int
context_config_edit(struct pine *ps, CONF_S **cl)
{
    char            *raw_ctxt, tpath[MAILTMPLEN], *p, **lval;
    struct variable *var;
    int              i;

    if(!(*cl)->d.c.ct)
      return(0);

    /* Undigest the context */
    strncpy(tpath, ((*cl)->d.c.ct->context[0] == '{'
		    && (p = strchr((*cl)->d.c.ct->context, '}')))
		      ? ++p
		      : (*cl)->d.c.ct->context, sizeof(tpath)-1);
    tpath[sizeof(tpath)-1] = '\0';

    if((p = strstr(tpath, "%s")) != NULL)
      *p = '\0';

    if((raw_ctxt = context_edit_screen(ps, "EDIT", (*cl)->d.c.ct->nickname,
				      (*cl)->d.c.ct->server, tpath,
				      (*cl)->d.c.ct->dir ?
				             (*cl)->d.c.ct->dir->view.user
					     : NULL)) != NULL){

	if((*cl)->var){
	    var = (*cl)->var;
	    lval = LVAL(var, ew);
	    i = (*cl)->d.c.ct->var.i;

	    if(lval && lval[i] && !strcmp(lval[i], raw_ctxt))
	      q_status_message(SM_ORDER, 0, 3, _("No change"));
	    else if(lval){
		if(lval[i])
		  fs_give((void **) &lval[i]);

		lval[i] = raw_ctxt;
		raw_ctxt = NULL;

		q_status_message(SM_ORDER, 0, 3, _("Collection list entry updated"));
	    }

	    (*cl)->d.c.cs->starting_var = var;
	    (*cl)->d.c.cs->starting_varmem = i;

	    if(raw_ctxt)
	      fs_give((void **) &raw_ctxt);
	}
	else{
	    q_status_message(SM_ORDER|SM_DING, 3, 3,
			     "Programmer botch in context_config_edit");
	    return(0);
	}

	return(15);
    }

    ps->mangled_screen = 1;
    return(0);
}


int
context_config_delete(struct pine *ps, CONF_S **cl)
{
    char       tmp[MAILTMPLEN];

    if(!(*cl)->var){
	q_status_message(SM_ORDER|SM_DING, 3, 3,
			 "Programmer botch in context_config_delete");
	return(0);
    }

    if((*cl)->d.c.ct->use & CNTXT_SAVEDFLT &&
       (*cl)->d.c.ct->next &&
       (*cl)->d.c.ct->next->use & CNTXT_NEWS &&
       (*cl)->d.c.ct->var.v == (*cl)->d.c.ct->next->var.v){
	q_status_message(SM_ORDER|SM_DING, 3, 3,
			 _("Sorry, cannot Delete causing news to move to top"));
	return(0);
    }

    snprintf(tmp, sizeof(tmp), _("Delete the collection definition for \"%s\""),
	    (*cl)->value);
    tmp[sizeof(tmp)-1] = '\0';
    if(want_to(tmp, 'n', 'n', NO_HELP, WT_FLUSH_IN) == 'y'){

	(*cl)->d.c.cs->starting_var = (*cl)->var;
	(*cl)->d.c.cs->starting_varmem = (*cl)->d.c.ct->var.i;
	if((*cl)->d.c.ct->next){
	    if((*cl)->d.c.ct->next->var.v != (*cl)->var){
		(*cl)->d.c.cs->starting_var = (*cl)->d.c.ct->next->var.v;
		(*cl)->d.c.cs->starting_varmem = 0;
	    }
	    else{
		(*cl)->d.c.cs->starting_var = (*cl)->var;
		(*cl)->d.c.cs->starting_varmem = (*cl)->d.c.ct->var.i;
	    }
	}
	else{
	    if((*cl)->d.c.ct->var.i > 0){
		(*cl)->d.c.cs->starting_var = (*cl)->var;
		(*cl)->d.c.cs->starting_varmem = (*cl)->d.c.ct->var.i - 1;
	    }
	    else{
		if((*cl)->d.c.ct->prev){
		    (*cl)->d.c.cs->starting_var = (*cl)->d.c.ct->prev->var.v;
		    (*cl)->d.c.cs->starting_varmem = (*cl)->d.c.ct->prev->var.i;
		}
	    }
	}
	  
	/* Remove from var list */
	if(!ccs_var_delete(ps, (*cl)->d.c.ct)){
	    q_status_message(SM_ORDER|SM_DING, 3, 3,
			     _("Error deleting renamed context"));
	    return(0);
	}

	q_status_message(SM_ORDER, 0, 3, _("Collection deleted"));

	return(15);
    }

    q_status_message(SM_ORDER, 0, 3, _("No collections deleted"));
    return(0);
}


int
ccs_var_delete(struct pine *ps, CONTEXT_S *ctxt)
{
    int		count, i;
    char      **newl = NULL, **lval, **lp, ***alval;

    if(ctxt)
      lval = LVAL(ctxt->var.v, ew);
    else
      lval = NULL;

    for(count = 0; lval && lval[count]; count++)
      ;				/* sum the list */

    if(count > 1){
	newl = (char **) fs_get(count * sizeof(char *));
	for(i = 0, lp = newl; lval[i]; i++)
	  if(i != ctxt->var.i)
	    *lp++ = cpystr(lval[i]);

	*lp = NULL;
    }
    
    alval = ALVAL(ctxt->var.v, ew);
    if(alval){
	free_list_array(alval);
	if(newl){
	    for(i = 0; newl[i] ; i++)	/* count elements */
	      ;

	    *alval = (char **) fs_get((i+1) * sizeof(char *));

	    for(i = 0; newl[i] ; i++)
	      (*alval)[i] = cpystr(newl[i]);

	    (*alval)[i]         = NULL;
	}
    }

    free_list_array(&newl);
    return(1);
}


/*
 * Insert into "var", which currently has values "oldvarval", the "newline"
 * at position "insert".
 */
int
ccs_var_insert(struct pine *ps, char *newline, struct variable *var, char **oldvarval, int insert)
{
    int    count, i, offset;
    char **newl, ***alval;

    for(count = 0; oldvarval && oldvarval[count]; count++)
      ;

    if(insert < 0 || insert > count){
	q_status_message(SM_ORDER,3,5, "unexpected problem inserting folder");
	return(0);
    }

    newl = (char **)fs_get((count + 2) * sizeof(char *));
    newl[insert] = cpystr(newline);
    newl[count + 1]   = NULL;
    for(i = offset = 0; oldvarval && oldvarval[i]; i++){
	if(i == insert)
	  offset = 1;

	newl[i + offset] = cpystr(oldvarval[i]);
    }

    alval = ALVAL(var, ew);
    if(alval){
	free_list_array(alval);
	if(newl){
	    for(i = 0; newl[i] ; i++)	/* count elements */
	      ;

	    *alval = (char **) fs_get((i+1) * sizeof(char *));

	    for(i = 0; newl[i] ; i++)
	      (*alval)[i] = cpystr(newl[i]);

	    (*alval)[i]         = NULL;
	}
    }

    free_list_array(&newl);
    return(1);
}


int
context_select_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int retval = 0;

    switch(cmd){
      case MC_CHOICE :
	(*cl)->d.c.cs->selected = (*cl)->d.c.ct;
	retval = simple_exit_cmd(flags);
	break;

      case MC_EXIT :
        retval = simple_exit_cmd(flags);
	break;

      case MC_MAIN :
        retval = simple_exit_cmd(flags);
	ps_global->next_screen = main_menu_screen;
	break;

      case MC_INDEX :
	if(THREADING()
	   && sp_viewing_a_thread(ps_global->mail_stream)
	   && unview_thread(ps_global, ps_global->mail_stream, ps_global->msgmap)){
	    ps_global->next_screen = mail_index_screen;
	    ps_global->view_skipped_index = 0;
	    ps_global->mangled_screen = 1;
	}

	retval = simple_exit_cmd(flags);
	ps_global->next_screen = mail_index_screen;
	break;

      case MC_COMPOSE :
	retval = simple_exit_cmd(flags);
	ps_global->next_screen = compose_screen;
	break;

      case MC_ROLE :
	retval = simple_exit_cmd(flags);
	ps_global->next_screen = alt_compose_screen;
	break;

      case MC_GOTO :
        {
	    int notrealinbox;
	    CONTEXT_S *c = (*cl)->d.c.ct;
	    char *new_fold = broach_folder(-FOOTER_ROWS(ps), 0, &notrealinbox, &c);

	    if(new_fold && do_broach_folder(new_fold, c, NULL, notrealinbox ? 0L : DB_INBOXWOCNTXT) > 0){
		ps_global->next_screen = mail_index_screen;
		retval = simple_exit_cmd(flags);
	    }
	    else
	      ps->mangled_footer = 1;
        }

	break;

      case MC_QUIT :
	retval = simple_exit_cmd(flags);
	ps_global->next_screen = quit_screen;
	break;

      default:
	retval = -1;
	break;
    }

    if(retval > 0)
      ps->mangled_body = 1;

    return(retval);
}
