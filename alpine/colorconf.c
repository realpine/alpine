#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: colorconf.c 934 2008-02-23 00:44:29Z hubert@u.washington.edu $";
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
#include "colorconf.h"
#include "keymenu.h"
#include "status.h"
#include "confscroll.h"
#include "radio.h"
#include "mailview.h"
#include "../pith/state.h"
#include "../pith/util.h"
#include "../pith/color.h"
#include "../pith/icache.h"
#include "../pith/mailcmd.h"
#include "../pith/list.h"


/*
 * Internal prototypes
 */
char	       *color_setting_text_line(struct pine *, struct variable *);
void	        revert_to_saved_color_config(struct pine *, SAVED_CONFIG_S *);
SAVED_CONFIG_S *save_color_config_vars(struct pine *);
void	        free_saved_color_config(struct pine *, SAVED_CONFIG_S **);
void	        color_config_init_display(struct pine *, CONF_S **, CONF_S **);
void	        add_header_color_line(struct pine *, CONF_S **, char *, int);
int     	is_rgb_color(char *);
char	       *new_color_line(char *, int, int, int);
int	        color_text_tool(struct pine *, int, CONF_S **, unsigned);
int             offer_normal_color_for_var(struct pine *, struct variable *);
int             offer_none_color_for_var(struct pine *, struct variable *);
void		color_update_selected(struct pine *, CONF_S *, char *, char *, int);
int		color_edit_screen(struct pine *, CONF_S **);


int	treat_color_vars_as_text;


void
color_config_screen(struct pine *ps, int edit_exceptions)
{
    CONF_S	   *ctmp = NULL, *first_line = NULL;
    SAVED_CONFIG_S *vsave;
    OPT_SCREEN_S    screen;
    int             readonly_warning = 0;

    ps->next_screen = SCREEN_FUN_NULL;

    mailcap_free(); /* free resources we won't be using for a while */

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

    color_config_init_display(ps, &ctmp, &first_line);

    vsave = save_color_config_vars(ps);

    memset(&screen, 0, sizeof(screen));
    screen.deferred_ro_warning = readonly_warning;
    switch(conf_scroll_screen(ps, &screen, first_line,
			      edit_exceptions ? _("SETUP COLOR EXCEPTIONS")
					      : _("SETUP COLOR"),
			      /* TRANSLATORS: Print something1 using something2.
				 configuration is something1 */
			      _("configuration"), 0)){
      case 0:
	break;

      case 1:
	write_pinerc(ps, ew, WRP_NONE);
	break;
    
      case 10:
	revert_to_saved_color_config(ps, vsave);
	break;
      
      default:
	q_status_message(SM_ORDER, 7, 10,
			 _("conf_scroll_screen bad ret in color_config"));
	break;
    }

    free_saved_color_config(ps, &vsave);

#ifdef _WINDOWS
    mswin_set_quit_confirm (F_OFF(F_QUIT_WO_CONFIRM, ps_global));
#endif
}


char *
sample_text(struct pine *ps, struct variable *v)
{
    char *ret = SAMP2;
    char *pvalfg, *pvalbg;

    pvalfg = PVAL(v, ew);
    pvalbg = PVAL(v+1, ew);

    if((v && v->name &&
        srchstr(v->name, "-foreground-color") &&
	pvalfg && pvalfg[0] && pvalbg && pvalbg[0]) ||
       (v == &ps->vars[V_VIEW_HDR_COLORS] || v == &ps->vars[V_KW_COLORS]))
      ret = SAMP1;

    return(ret);
}


char *
sampleexc_text(struct pine *ps, struct variable *v)
{
    char *ret = "";
    char *pvalfg, *pvalbg;

    pvalfg = PVAL(v, Post);
    pvalbg = PVAL(v+1, Post);
    if(v && color_holding_var(ps, v) &&
       srchstr(v->name, "-foreground-color")){
	if(ew == Main && pvalfg && pvalfg[0] && pvalbg && pvalbg[0])
	  ret = SAMPEXC;
    }

    return(ret);
}


char *
color_setting_text_line(struct pine *ps, struct variable *v)
{
    char *p;
    char  tmp[1200];

    p = sampleexc_text(ps, v);

    /*
     * Don't need to use utf8_snprintf if we're not trying to
     * control the widths of fields.
     */
    snprintf(tmp, sizeof(tmp), "%s     %s%*s%s%s", SAMPLE_LEADER,
	    sample_text(ps,v), *p ? SBS : 0, "", p,
	    color_parenthetical(v));
    return(cpystr(tmp));
}


/*
 * Compare saved user_val with current user_val to see if it changed.
 * If any have changed, change it back and take the appropriate action.
 */
void
revert_to_saved_color_config(struct pine *ps, SAVED_CONFIG_S *vsave)
{
    struct variable *vreal;
    SAVED_CONFIG_S  *v;
    int i, n;
    int changed = 0;
    char *pval, **apval, **lval, ***alval;

    v = vsave;
    for(vreal = ps->vars; vreal->name; vreal++,v++){
	if(!(color_related_var(ps, vreal) || vreal==&ps->vars[V_FEATURE_LIST]))
	  continue;

	if(vreal->is_list){
	    lval  = LVAL(vreal, ew);
	    alval = ALVAL(vreal, ew);

	    if((v->saved_user_val.l && !lval)
	       || (!v->saved_user_val.l && lval))
	      changed++;
	    else if(!v->saved_user_val.l && !lval)
	      ;/* no change, nothing to do */
	    else
	      for(i = 0; v->saved_user_val.l[i] || lval[i]; i++)
		if((v->saved_user_val.l[i]
		      && (!lval[i]
			 || strcmp(v->saved_user_val.l[i], lval[i])))
		   ||
		     (!v->saved_user_val.l[i] && lval[i])){
		    changed++;
		    break;
		}
	    
	    if(changed){
		char  **list;

		if(alval){
		    if(*alval)
		      free_list_array(alval);
		
		    /* copy back the original one */
		    if(v->saved_user_val.l){
			list = v->saved_user_val.l;
			n = 0;
			/* count how many */
			while(list[n])
			  n++;

			*alval = (char **)fs_get((n+1) * sizeof(char *));

			for(i = 0; i < n; i++)
			  (*alval)[i] = cpystr(v->saved_user_val.l[i]);

			(*alval)[n] = NULL;
		    }
		}
	    }
	}
	else{
	    pval  = PVAL(vreal, ew);
	    apval = APVAL(vreal, ew);

	    if((v->saved_user_val.p &&
	        (!pval || strcmp(v->saved_user_val.p, pval))) ||
	       (!v->saved_user_val.p && pval)){
		/* It changed, fix it */
		changed++;
		if(apval){
		    /* free the changed value */
		    if(*apval)
		      fs_give((void **)apval);

		    if(v->saved_user_val.p)
		      *apval = cpystr(v->saved_user_val.p);
		}
	    }
	}

	if(changed){
	    if(vreal == &ps->vars[V_FEATURE_LIST])
	      set_feature_list_current_val(vreal);
	    else
	      set_current_val(vreal, TRUE, FALSE);

	    fix_side_effects(ps, vreal, 1);
	}
    }

    if(changed){
	set_current_color_vals(ps);
	ClearScreen();
	ps->mangled_screen = 1;
    }
}


SAVED_CONFIG_S *
save_color_config_vars(struct pine *ps)
{
    struct variable *vreal;
    SAVED_CONFIG_S *vsave, *v;

    vsave = (SAVED_CONFIG_S *)fs_get((V_LAST_VAR+1)*sizeof(SAVED_CONFIG_S));
    memset((void *)vsave, 0, (V_LAST_VAR+1)*sizeof(SAVED_CONFIG_S));
    v = vsave;
    for(vreal = ps->vars; vreal->name; vreal++,v++){
	if(!(color_related_var(ps, vreal) || vreal==&ps->vars[V_FEATURE_LIST]))
	  continue;
	
	if(vreal->is_list){
	    int n, i;
	    char **list;

	    if(LVAL(vreal, ew)){
		/* count how many */
		n = 0;
		list = LVAL(vreal, ew);
		while(list[n])
		  n++;

		v->saved_user_val.l = (char **)fs_get((n+1) * sizeof(char *));
		memset((void *)v->saved_user_val.l, 0, (n+1)*sizeof(char *));
		for(i = 0; i < n; i++)
		  v->saved_user_val.l[i] = cpystr(list[i]);

		v->saved_user_val.l[n] = NULL;
	    }
	}
	else{
	    if(PVAL(vreal, ew))
	      v->saved_user_val.p = cpystr(PVAL(vreal, ew));
	}
    }

    return(vsave);
}


void
free_saved_color_config(struct pine *ps, SAVED_CONFIG_S **vsavep)
{
    struct variable *vreal;
    SAVED_CONFIG_S  *v;

    if(vsavep && *vsavep){
	for(v = *vsavep, vreal = ps->vars; vreal->name; vreal++,v++){
	    if(!(color_related_var(ps, vreal) ||
	       (vreal == &ps->vars[V_FEATURE_LIST])))
	      continue;
	    
	    if(vreal->is_list){  /* free saved_user_val.l */
		if(v && v->saved_user_val.l)
		  free_list_array(&v->saved_user_val.l);
	    }
	    else if(v && v->saved_user_val.p)
	      fs_give((void **)&v->saved_user_val.p);
	}

	fs_give((void **)vsavep);
    }
}


void
color_config_init_display(struct pine *ps, CONF_S **ctmp, CONF_S **first_line)
{
    char	  **lval;
    int		    i, saw_first_index = 0;
    struct	    variable  *vtmp;
    char           *dashes = "--------------";

#ifndef	_WINDOWS
    vtmp = &ps->vars[V_COLOR_STYLE];

    new_confline(ctmp);
    (*ctmp)->flags       |= (CF_NOSELECT | CF_STARTITEM);
    (*ctmp)->keymenu      = &config_radiobutton_keymenu;
    (*ctmp)->tool	  = NULL;
    (*ctmp)->varname	  = cpystr("Color Style");
    (*ctmp)->varnamep	  = *ctmp;

    standard_radio_setup(ps, ctmp, vtmp, first_line);

    new_confline(ctmp);
    /* Blank line */
    (*ctmp)->flags |= (CF_NOSELECT | CF_B_LINE);

    if(!pico_usingcolor()){
	/* add a line explaining that color is not turned on */
	new_confline(ctmp);
	(*ctmp)->help			 = NO_HELP;
	(*ctmp)->flags			|= CF_NOSELECT;
	(*ctmp)->value = cpystr(COLORNOSET);

	new_confline(ctmp);
	/* Blank line */
	(*ctmp)->flags |= (CF_NOSELECT | CF_B_LINE);
    }

#endif

    vtmp = &ps->vars[V_INDEX_COLOR_STYLE];

    new_confline(ctmp);
    (*ctmp)->flags       |= (CF_NOSELECT | CF_STARTITEM);
    (*ctmp)->keymenu      = &config_radiobutton_keymenu;
    (*ctmp)->tool	  = NULL;
    (*ctmp)->varname	  = cpystr(_("Current Indexline Style"));
    (*ctmp)->varnamep	  = *ctmp;

    standard_radio_setup(ps, ctmp, vtmp, NULL);

    new_confline(ctmp);
    /* Blank line */
    (*ctmp)->flags |= (CF_NOSELECT | CF_B_LINE);

    vtmp = &ps->vars[V_TITLEBAR_COLOR_STYLE];

    new_confline(ctmp);
    (*ctmp)->flags       |= (CF_NOSELECT | CF_STARTITEM);
    (*ctmp)->keymenu      = &config_radiobutton_keymenu;
    (*ctmp)->tool	  = NULL;
    (*ctmp)->varname	  = cpystr(_("Titlebar Color Style"));
    (*ctmp)->varnamep	  = *ctmp;

    standard_radio_setup(ps, ctmp, vtmp, NULL);

    new_confline(ctmp);
    /* Blank line */
    (*ctmp)->flags |= (CF_NOSELECT | CF_B_LINE);

    new_confline(ctmp);
    /* title before general colors */
    (*ctmp)->help			 = NO_HELP;
    (*ctmp)->flags			|= CF_NOSELECT;
    (*ctmp)->value                       = cpystr(dashes);
    new_confline(ctmp);
    (*ctmp)->help			 = NO_HELP;
    (*ctmp)->flags			|= CF_NOSELECT;
    (*ctmp)->value = cpystr(_("GENERAL COLORS"));
    new_confline(ctmp);
    (*ctmp)->help			 = NO_HELP;
    (*ctmp)->flags			|= CF_NOSELECT;
    (*ctmp)->value                       = cpystr(dashes);

    for(vtmp = ps->vars; vtmp->name; vtmp++){
	if(!color_holding_var(ps, vtmp))
	  continue;

	/* If not foreground, skip it */
	if(!srchstr(vtmp->name, "-foreground-color"))
	  continue;

	/* skip this for now and include it with HEADER COLORS */
	if(vtmp == &ps->vars[V_HEADER_GENERAL_FORE_COLOR])
	  continue;

	if(!saw_first_index && !struncmp(vtmp->name, "index-", 6)){
	    saw_first_index++;
	    new_confline(ctmp);		/* Blank line */
	    (*ctmp)->flags |= (CF_NOSELECT | CF_B_LINE);
	    new_confline(ctmp);
	    (*ctmp)->help			 = NO_HELP;
	    (*ctmp)->flags			|= CF_NOSELECT;
	    (*ctmp)->value                       = cpystr(dashes);
	    new_confline(ctmp);
	    (*ctmp)->help			 = NO_HELP;
	    (*ctmp)->flags			|= CF_NOSELECT;
	    (*ctmp)->value = cpystr(_("INDEX COLORS"));
	    new_confline(ctmp);
	    (*ctmp)->help			 = NO_HELP;
	    (*ctmp)->flags			|= CF_NOSELECT;
	    (*ctmp)->value                       = cpystr(dashes);
	}

	new_confline(ctmp);
	/* Blank line */
	(*ctmp)->flags |= CF_NOSELECT | CF_B_LINE;

	new_confline(ctmp)->var = vtmp;
	if(first_line && !*first_line)
	  *first_line = *ctmp;

	(*ctmp)->varnamep		 = *ctmp;
	(*ctmp)->keymenu		 = &color_setting_keymenu;
	(*ctmp)->help		 	 = config_help(vtmp - ps->vars, 0);
	(*ctmp)->tool			 = color_setting_tool;
	(*ctmp)->flags |= (CF_STARTITEM | CF_COLORSAMPLE | CF_POT_SLCTBL);
	if(!pico_usingcolor())
	  (*ctmp)->flags |= CF_NOSELECT;

	(*ctmp)->value			 = pretty_value(ps, *ctmp);
	(*ctmp)->valoffset		 = COLOR_INDENT;
    }

    /*
     * custom header colors
     */
    new_confline(ctmp);		/* Blank line */
    (*ctmp)->flags |= CF_NOSELECT | CF_B_LINE;
    new_confline(ctmp);
    (*ctmp)->help			 = NO_HELP;
    (*ctmp)->flags			|= CF_NOSELECT;
    (*ctmp)->value                       = cpystr(dashes);
    new_confline(ctmp);
    (*ctmp)->help			 = NO_HELP;
    (*ctmp)->flags			|= CF_NOSELECT;
    (*ctmp)->value = cpystr(_("HEADER COLORS"));
    new_confline(ctmp);
    (*ctmp)->help			 = NO_HELP;
    (*ctmp)->flags			|= CF_NOSELECT;
    (*ctmp)->value                       = cpystr(dashes);

    vtmp = &ps->vars[V_HEADER_GENERAL_FORE_COLOR];
    new_confline(ctmp);
    /* Blank line */
    (*ctmp)->flags |= CF_NOSELECT | CF_B_LINE;

    new_confline(ctmp)->var = vtmp;

    (*ctmp)->varnamep		 = *ctmp;
    (*ctmp)->keymenu		 = &color_setting_keymenu;
    (*ctmp)->help		 = config_help(vtmp - ps->vars, 0);
    (*ctmp)->tool		 = color_setting_tool;
    (*ctmp)->flags |= (CF_STARTITEM | CF_COLORSAMPLE | CF_POT_SLCTBL);
    if(!pico_usingcolor())
      (*ctmp)->flags |= CF_NOSELECT;

    (*ctmp)->value		 = pretty_value(ps, *ctmp);
    (*ctmp)->valoffset		 = COLOR_INDENT;

    vtmp = &ps->vars[V_VIEW_HDR_COLORS];
    lval = LVAL(vtmp, ew);

    if(lval && lval[0] && lval[0][0]){
	for(i = 0; lval && lval[i]; i++)
	  add_header_color_line(ps, ctmp, lval[i], i);
    }
    else{
	new_confline(ctmp);		/* Blank line */
	(*ctmp)->flags |= CF_NOSELECT | CF_B_LINE;
	new_confline(ctmp);
	(*ctmp)->help			 = NO_HELP;
	(*ctmp)->flags			|= CF_NOSELECT;
	(*ctmp)->value = cpystr(_(ADDHEADER_COMMENT));
	(*ctmp)->valoffset		 = COLOR_INDENT;
    }


    /*
     * custom keyword colors
     */
    new_confline(ctmp);		/* Blank line */
    (*ctmp)->flags |= CF_NOSELECT | CF_B_LINE;
    new_confline(ctmp);
    (*ctmp)->help			 = NO_HELP;
    (*ctmp)->flags			|= CF_NOSELECT;
    (*ctmp)->value                       = cpystr(dashes);
    new_confline(ctmp);
    (*ctmp)->help			 = NO_HELP;
    (*ctmp)->flags			|= CF_NOSELECT;
    (*ctmp)->value = cpystr(KW_COLORS_HDR);
    new_confline(ctmp);
    (*ctmp)->help			 = NO_HELP;
    (*ctmp)->flags			|= CF_NOSELECT;
    (*ctmp)->value                       = cpystr(dashes);


    if(ps->keywords){
	KEYWORD_S *kw;
	char      *name, *comment, *word;
	int        j, lv = 0, lc = 0, ltot = 0, eq_col = EQ_COL;

	vtmp = &ps->vars[V_KW_COLORS];

	/* first figure out widths for display */
	for(kw = ps->keywords; kw; kw = kw->next){
	    word = kw->nick ? kw->nick : kw->kw ? kw->kw : "";
	    i = utf8_width(word);
	    if(lv < i)
	      lv = i;

	    j = 0;
	    if(kw->nick && kw->kw && kw->kw[0]){
		word = kw->kw;
		j = utf8_width(word) + 2;
		if(lc < j)
		  lc = j;
	    }

	    if(ltot < (i + (j > 2 ? j : 0)))
	      ltot = (i + (j > 2 ? j : 0));
	}

	lv = MIN(lv, 100);
	lc = MIN(lc, 100);
	ltot = MIN(ltot, 100);

	/*
	 * SC is width of " Color"
	 * SS is space between nickname and keyword value
	 * SW is space between words and Sample
	 */
#define SC 6
#define SS 1
#define SW 3
	if(COLOR_INDENT + SC + ltot + (lc>0?SS:0) > eq_col - SW){
	  eq_col = MIN(MAX(ps->ttyo->screen_cols - 10, 20),
		       COLOR_INDENT + SC + ltot + (lc>0?SS:0) + SW);
	  if(COLOR_INDENT + SC + ltot + (lc>0?SS:0) > eq_col - SW){
	    eq_col = MIN(MAX(ps->ttyo->screen_cols - 10, 20),
		         COLOR_INDENT + SC + lv + (lc>0?SS:0) + lc + SW);
	    if(COLOR_INDENT + SC + lv + (lc>0?SS:0) + lc > eq_col - SW){
	      lc = MIN(lc, MAX(eq_col - SW - COLOR_INDENT - SC - lv - SS, 7));
	      if(COLOR_INDENT + SC + lv + (lc>0?SS:0) + lc > eq_col - SW){
	        lc = 0;
	        if(COLOR_INDENT + SC + lv > eq_col - SW){
		  lv = MAX(eq_col - SW - COLOR_INDENT - SC, 7);
		}
	      }
	    }
	  }
	}

	lval = LVAL(vtmp, ew);
	if(lval && lval[0] && !strcmp(lval[0], INHERIT)){
	    new_confline(ctmp);
	    (*ctmp)->flags		|= CF_NOSELECT | CF_B_LINE;

	    new_confline(ctmp)->var	 = vtmp;
	    (*ctmp)->varnamep		 = *ctmp;
	    (*ctmp)->keymenu		 = &kw_color_setting_keymenu;
	    (*ctmp)->help		 = config_help(vtmp - ps->vars, 0);
	    (*ctmp)->tool		 = color_setting_tool;
	    (*ctmp)->varmem		 = CFC_SET_COLOR(i, 0);
	    (*ctmp)->valoffset		 = COLOR_INDENT;

	    (*ctmp)->flags = (CF_NOSELECT | CF_INHERIT);
	}

	/* now create the config lines */
	for(kw = ps->keywords, i = 0; kw; kw = kw->next, i++){
	    char tmp[2000];

	    /* Blank line */
	    new_confline(ctmp);
	    (*ctmp)->flags		|= CF_NOSELECT | CF_B_LINE;

	    new_confline(ctmp)->var	 = vtmp;
	    (*ctmp)->varnamep		 = *ctmp;
	    (*ctmp)->keymenu		 = &kw_color_setting_keymenu;
	    (*ctmp)->help		 = config_help(vtmp - ps->vars, 0);
	    (*ctmp)->tool		 = color_setting_tool;
	    (*ctmp)->flags |= (CF_STARTITEM | CF_COLORSAMPLE | CF_POT_SLCTBL);
	    if(!pico_usingcolor())
	      (*ctmp)->flags |= CF_NOSELECT;

	    /*
	     * Not actually a color in this case, it is an index into
	     * the keywords list.
	     */
	    (*ctmp)->varmem		 = CFC_SET_COLOR(i, 0);

	    name = short_str(kw->nick ? kw->nick : kw->kw ? kw->kw : "",
			     tmp_20k_buf+10000, 1000, lv, EndDots);
	    if(lc > 0 && kw->nick && kw->kw && kw->kw[0])
	      comment = short_str(kw->kw, tmp_20k_buf+11000, SIZEOF_20KBUF - 11000, lc, EndDots);
	    else
	      comment = NULL;

	    utf8_snprintf(tmp, sizeof(tmp), "%.*w%*s%s%.*w%s Color%*s %s%s",
		    lv, name,
		    (lc > 0 && comment) ? SS : 0, "",
		    (lc > 0 && comment) ? "(" : "",
		    (lc > 0 && comment) ? lc : 0, (lc > 0 && comment) ? comment : "",
		    (lc > 0 && comment) ? ")" : "",
		    MAX(MIN(eq_col - COLOR_INDENT - MIN(lv,utf8_width(name))
			- SC - 1
			- ((lc > 0 && comment)
			    ? (SS+2+MIN(lc,utf8_width(comment))) : 0), 100), 0), "",
		    sample_text(ps,vtmp),
		    color_parenthetical(vtmp));

	    (*ctmp)->value		 = cpystr(tmp);
	    (*ctmp)->valoffset		 = COLOR_INDENT;
	}
    }
    else{
	new_confline(ctmp);		/* Blank line */
	(*ctmp)->flags |= CF_NOSELECT | CF_B_LINE;
	new_confline(ctmp);
	(*ctmp)->help			 = NO_HELP;
	(*ctmp)->flags			|= CF_NOSELECT;
	(*ctmp)->value = cpystr(_("[ Use Setup/Config command to add Keywords ]"));
	(*ctmp)->valoffset		 = COLOR_INDENT;
    }
}


char *
color_parenthetical(struct variable *var)
{
    int    norm, exc, exc_inh;
    char **lval, *ret = "";

    if(var == &ps_global->vars[V_VIEW_HDR_COLORS]
       || var == &ps_global->vars[V_KW_COLORS]){
	norm    = (LVAL(var, Main) != NULL);
	exc     = (LVAL(var, ps_global->ew_for_except_vars) != NULL);
	exc_inh = ((lval=LVAL(var, ps_global->ew_for_except_vars)) &&
		   lval[0] && !strcmp(INHERIT, lval[0]));

	/* editing normal but there is an exception config */
	if((ps_global->ew_for_except_vars != Main) && (ew == Main)){
	    if((exc && !exc_inh))
	      ret = _(" (overridden by exceptions)");
	    else if(exc && exc_inh)
	      ret = _(" (more in exceptions)");
	}
	/* editing exception config */
	else if((ps_global->ew_for_except_vars != Main) &&
		(ew == ps_global->ew_for_except_vars)){
	    if(exc && exc_inh && norm)
	      ret = _(" (more in main config)");
	}
    }

    return(ret);
}


void
add_header_color_line(struct pine *ps, CONF_S **ctmp, char *val, int which)
{
    struct variable *vtmp;
    SPEC_COLOR_S     *hc;
    char	     tmp[100+1];
    int              l;

    vtmp = &ps->vars[V_VIEW_HDR_COLORS];
    l = strlen(HEADER_WORD);

    /* Blank line */
    new_confline(ctmp);
    (*ctmp)->flags		|= CF_NOSELECT | CF_B_LINE;

    new_confline(ctmp)->var	 = vtmp;
    (*ctmp)->varnamep		 = *ctmp;
    (*ctmp)->keymenu		 = &custom_color_setting_keymenu;
    (*ctmp)->help		 = config_help(vtmp - ps->vars, 0);
    (*ctmp)->tool		 = color_setting_tool;
    (*ctmp)->flags |= (CF_STARTITEM | CF_COLORSAMPLE | CF_POT_SLCTBL);
    if(!pico_usingcolor())
      (*ctmp)->flags |= CF_NOSELECT;

    /* which is an index into the variable list */
    (*ctmp)->varmem		 = CFC_SET_COLOR(which, 0);

    hc = spec_color_from_var(val, 0);
    if(hc && hc->inherit)
      (*ctmp)->flags = (CF_NOSELECT | CF_INHERIT);
    else{
	/*
	 * This isn't quite right because of the assumption that the
	 * first character of spec fits in one octet. I haven't tried
	 * to figure out what we're really trying to accomplish
	 * with all this. It probably doesn't happen in real life.
	 */
	utf8_snprintf(tmp, sizeof(tmp), "%s%c%.*w Color%*w %s%s",
		HEADER_WORD,
		(hc && hc->spec) ? (islower((unsigned char)hc->spec[0])
					    ? toupper((unsigned char)hc->spec[0])
					    : hc->spec[0]) : '?',
		MIN(utf8_width((hc && hc->spec && hc->spec[0]) ? hc->spec+1 : ""),30-l),
		(hc && hc->spec && hc->spec[0]) ? hc->spec+1 : "",
		MAX(EQ_COL - COLOR_INDENT -1 -
		   MIN(utf8_width((hc && hc->spec && hc->spec[0]) ? hc->spec+1 : ""),30-l)
			    - l - 6 - 1, 0), "",
		sample_text(ps,vtmp),
		color_parenthetical(vtmp));
	tmp[sizeof(tmp)-1] = '\0';
	(*ctmp)->value		 = cpystr(tmp);
    }

    (*ctmp)->valoffset		 = COLOR_INDENT;

    if(hc)
      free_spec_colors(&hc);
}


/*
 * Set up the standard color setting display for one color.
 *
 * Args   fg -- the current foreground color
 *        bg -- the current background color
 *       def -- default box should be checked
 */
void
add_color_setting_disp(struct pine *ps, CONF_S **ctmp, struct variable *var,
		       CONF_S *varnamep, struct key_menu *km,
		       struct key_menu *cb_km, HelpType help, int indent,
		       int which, char *fg, char *bg, int def)
{
    int             i, lv, count, trans_is_on, transparent;
    char	    tmp[100+1];
    char           *title   = _("HELP FOR SETTING UP COLOR");
    int             fg_is_custom = 1, bg_is_custom = 1;
#ifdef	_WINDOWS
    CONF_S         *cl_custom = NULL;
#else
    char           *pvalfg, *pvalbg;
#endif


    /* find longest value's name */
    count = pico_count_in_color_table();
    lv = COLOR_BLOB_LEN;

    trans_is_on = pico_trans_is_on();

    /* put a title before list */
    new_confline(ctmp);
    (*ctmp)->varnamep		 = varnamep;
    (*ctmp)->keymenu		 = km;
    (*ctmp)->help		 = NO_HELP;
    (*ctmp)->tool		 = color_setting_tool;
    (*ctmp)->valoffset		 = indent;
    (*ctmp)->flags		|= CF_NOSELECT;
    (*ctmp)->varmem		 = 0;

    /*
     * The width of Foreground plus the spaces before Background should
     * be about lv + 5 + SPACE_BETWEEN_DOUBLEVARS.
     */
    (*ctmp)->value = cpystr("Foreground    Background");

    new_confline(ctmp)->var	 = var;
    (*ctmp)->varnamep		 = varnamep;
    (*ctmp)->keymenu		 = km;
    (*ctmp)->help		 = NO_HELP;
    (*ctmp)->tool		 = color_setting_tool;
    (*ctmp)->valoffset		 = indent;
    (*ctmp)->flags		|= (CF_COLORSAMPLE | CF_NOSELECT);
    (*ctmp)->varmem		 = CFC_SET_COLOR(which, 0);
    (*ctmp)->value		 = color_setting_text_line(ps, var);

    for(i = 0; i < count; i++){
	new_confline(ctmp)->var	 = var;
	(*ctmp)->varnamep	 = varnamep;
	(*ctmp)->keymenu	 = km;
	(*ctmp)->help		 = help;
	(*ctmp)->help_title	 = title;
	(*ctmp)->tool		 = color_setting_tool;
	(*ctmp)->valoffset	 = indent;
	/* 5 is length of "( )  " */
	(*ctmp)->val2offset	 = indent + lv + 5 + SPACE_BETWEEN_DOUBLEVARS;
	(*ctmp)->flags		|= CF_DOUBLEVAR;
	(*ctmp)->varmem		 = CFC_SET_COLOR(which, i);

	transparent = (trans_is_on && i == count-1);
	if(transparent)
	  (*ctmp)->help		 = h_config_usetransparent_color;

	if(fg_is_custom && fg && !strucmp(color_to_canonical_name(fg), colorx(i)))
	  fg_is_custom  = 0;

	if(bg_is_custom && bg && !strucmp(color_to_canonical_name(bg), colorx(i)))
	  bg_is_custom  = 0;

	(*ctmp)->value		 = new_color_line(transparent ? COLOR_BLOB_TRAN
							      : COLOR_BLOB,
						  fg &&
			   !strucmp(color_to_canonical_name(fg), colorx(i)),
						  bg &&
			   !strucmp(color_to_canonical_name(bg), colorx(i)),
						  lv);
    }

#ifdef	_WINDOWS
    new_confline(ctmp)->var  = var;
    (*ctmp)->varnamep	     = varnamep;
    (*ctmp)->keymenu	     = (km == &custom_color_changing_keymenu)
				 ? &custom_rgb_keymenu :
				 (km == &kw_color_changing_keymenu)
				   ? &kw_rgb_keymenu : &color_rgb_keymenu;
    (*ctmp)->help	     = help;
    (*ctmp)->help_title	     = title;
    (*ctmp)->tool	     = color_setting_tool;
    (*ctmp)->valoffset	     = indent;
    /* 5 is length of "( )  " */
    (*ctmp)->val2offset	     = indent + lv + 5 + SPACE_BETWEEN_DOUBLEVARS;
    (*ctmp)->flags	    |= CF_DOUBLEVAR;
    (*ctmp)->varmem	     = CFC_SET_COLOR(which, i);
    cl_custom = (*ctmp);
#endif

    if(offer_normal_color_for_var(ps, var)){
	new_confline(ctmp)->var		= var;
	(*ctmp)->varnamep	 	= varnamep;
	(*ctmp)->keymenu		= km;
	(*ctmp)->help			= h_config_usenormal_color;
	(*ctmp)->help_title		= title;
	(*ctmp)->tool			= color_setting_tool;
	(*ctmp)->valoffset		= indent;
	/* 5 is length of "( )  " */
	(*ctmp)->val2offset	 = indent + lv + 5 + SPACE_BETWEEN_DOUBLEVARS;
	(*ctmp)->flags		|= CF_DOUBLEVAR;
	(*ctmp)->varmem		 = CFC_SET_COLOR(which, CFC_ICOLOR_NORM);
	if(fg_is_custom && fg && !struncmp(fg, MATCH_NORM_COLOR, RGBLEN))
	  fg_is_custom  = 0;

	if(bg_is_custom && bg && !struncmp(bg, MATCH_NORM_COLOR, RGBLEN))
	  bg_is_custom  = 0;

	(*ctmp)->value		 = new_color_line(COLOR_BLOB_NORM,
						  fg && !struncmp(fg, MATCH_NORM_COLOR, RGBLEN),
						  bg && !struncmp(bg, MATCH_NORM_COLOR, RGBLEN),
						  lv);
    }

    if(offer_none_color_for_var(ps, var)){
	new_confline(ctmp)->var		= var;
	(*ctmp)->varnamep	 	= varnamep;
	(*ctmp)->keymenu		= km;
	(*ctmp)->help			= h_config_usenone_color;
	(*ctmp)->help_title		= title;
	(*ctmp)->tool			= color_setting_tool;
	(*ctmp)->valoffset		= indent;
	/* 5 is length of "( )  " */
	(*ctmp)->val2offset	 = indent + lv + 5 + SPACE_BETWEEN_DOUBLEVARS;
	(*ctmp)->flags		|= CF_DOUBLEVAR;
	(*ctmp)->varmem		 = CFC_SET_COLOR(which, CFC_ICOLOR_NONE);
	if(fg_is_custom && fg && !struncmp(fg, MATCH_NONE_COLOR, RGBLEN))
	  fg_is_custom  = 0;

	if(bg_is_custom && bg && !struncmp(bg, MATCH_NONE_COLOR, RGBLEN))
	  bg_is_custom  = 0;

	(*ctmp)->value		 = new_color_line(COLOR_BLOB_NONE,
						  fg && !struncmp(fg, MATCH_NONE_COLOR, RGBLEN),
						  bg && !struncmp(bg, MATCH_NONE_COLOR, RGBLEN),
						  lv);
    }

#ifdef	_WINDOWS
    if(cl_custom)
      cl_custom->value = new_color_line("Custom", fg_is_custom, bg_is_custom, lv);
#endif

    new_confline(ctmp)->var	= var;
    (*ctmp)->varnamep		= varnamep;
    (*ctmp)->keymenu		= cb_km;
    (*ctmp)->help		= h_config_dflt_color;
    (*ctmp)->help_title		= title;
    (*ctmp)->tool		= color_setting_tool;
    (*ctmp)->valoffset		= indent;
    (*ctmp)->varmem		= CFC_SET_COLOR(which, 0);
#ifdef	_WINDOWS
    snprintf(tmp, sizeof(tmp), "[%c]  %s", def ? 'X' : ' ', "Default");
    tmp[sizeof(tmp)-1] = '\0';
#else
    pvalfg = PVAL(var,Main);
    pvalbg = PVAL(var+1,Main);
    if(!var->is_list &&
       ((var == &ps->vars[V_NORM_FORE_COLOR]) ||
        (ew == Post && pvalfg && pvalfg[0] && pvalbg && pvalbg[0]) ||
        (var->global_val.p && var->global_val.p[0] &&
         (var+1)->global_val.p && (var+1)->global_val.p[0])))
      snprintf(tmp, sizeof(tmp), "[%c]  %s", def ? 'X' : ' ', "Default");
    else if(var == &ps->vars[V_REV_FORE_COLOR])
      snprintf(tmp, sizeof(tmp), "[%c]  %s", def ? 'X' : ' ',
	  "Default (terminal's standout mode, usually reverse video)");
    else if(var == &ps->vars[V_SLCTBL_FORE_COLOR])
      snprintf(tmp, sizeof(tmp), "[%c]  %s", def ? 'X' : ' ',
	  "Default (Bold Normal Color)");
    else if(var == &ps->vars[V_TITLECLOSED_FORE_COLOR])
      snprintf(tmp, sizeof(tmp), "[%c]  %s", def ? 'X' : ' ',
	  "Default (same as Title Color)");
    else if(var_defaults_to_rev(var))
      snprintf(tmp, sizeof(tmp), "[%c]  %s", def ? 'X' : ' ',
	  "Default (same as Reverse Color)");
    else if(km == &kw_color_changing_keymenu)
      snprintf(tmp, sizeof(tmp), "[%c]  %s", def ? 'X' : ' ',
	  "Default (same as Indexline Color)");
    else
      snprintf(tmp, sizeof(tmp), "[%c]  %s", def ? 'X' : ' ',
	  "Default (same as Normal Color)");

    tmp[sizeof(tmp)-1] = '\0';
#endif
    (*ctmp)->value		= cpystr(tmp);

    /*
     * Add a checkbox to turn bold on or off for selectable-item color.
     */
    if(var == &ps->vars[V_SLCTBL_FORE_COLOR]){
	char     **lval;

	new_confline(ctmp)->var	= var;
	(*ctmp)->varnamep	= varnamep;
	(*ctmp)->keymenu	= &selectable_bold_checkbox_keymenu;
	(*ctmp)->help		= h_config_bold_slctbl;
	(*ctmp)->help_title	= title;
	(*ctmp)->tool		= color_setting_tool;
	(*ctmp)->valoffset	= indent;
	(*ctmp)->varmem		= feature_list_index(F_SLCTBL_ITEM_NOBOLD);

	lval = LVAL(&ps->vars[V_FEATURE_LIST], ew);
	/*
	 * We don't use checkbox_pretty_value here because we just want
	 * the word Bold instead of the name of the variable and because
	 * we are actually using the negative of the feature. That is,
	 * the feature is really NOBOLD and we are using Bold.
	 */
	snprintf(tmp, sizeof(tmp), "[%c]  %s",
		test_feature(lval, feature_list_name(F_SLCTBL_ITEM_NOBOLD), 0)
		    ? ' ' : 'X', "Bold");
	tmp[sizeof(tmp)-1] = '\0';

	(*ctmp)->value		= cpystr(tmp);
    }
}


int
is_rgb_color(char *color)
{
    int i, j;

    for(i = 0; i < 3; i++){
	if(i && *color++ != ',')
	  return(FALSE);

	for(j = 0; j < 3; j++, color++)
	  if(!isdigit((unsigned char) *color))
	    return(FALSE);
    }

    return(TRUE);
}


char *
new_color_line(char *color, int fg, int bg, int len)
{
    char tmp[256];

    snprintf(tmp, sizeof(tmp), "(%c)  %-*.*s%*s(%c)  %-*.*s",
	    fg ? R_SELD : ' ', len, len, color, SPACE_BETWEEN_DOUBLEVARS, "",
	    bg ? R_SELD : ' ', len, len, color);
    tmp[sizeof(tmp)-1] = '\0';
    return(cpystr(tmp));
}


int
color_text_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int             rv = 0, i;
    struct variable v, *save_var;
    SPEC_COLOR_S    *hc, *hcolors;
    char           *starting_val, *val, tmp[100], ***alval, **apval;

    if(cmd == MC_EXIT)
      return(simple_exit_cmd(flags));

    alval = ALVAL((*cl)->var, ew);
    if(!alval || !*alval)
      return(rv);

    hcolors = spec_colors_from_varlist(*alval, 0);

    for(hc = hcolors, i=0; hc; hc = hc->next, i++)
      if(CFC_ICUST(*cl) == i)
	break;

    starting_val = (hc && hc->val) ? pattern_to_string(hc->val) : NULL;

    memset(&v, 0, sizeof(v));
    v.is_used    = 1;
    v.is_user    = 1;
    snprintf(tmp, sizeof(tmp), "\"%c%s Pattern\"",
	    islower((unsigned char)hc->spec[0])
					? toupper((unsigned char)hc->spec[0])
					: hc->spec[0],
	    hc->spec[1] ? hc->spec + 1 : "");
    tmp[sizeof(tmp)-1] = '\0';
    v.name       = tmp;
    /* have to use right part of v so text_tool will work right */
    apval = APVAL(&v, ew);
    *apval = starting_val ? cpystr(starting_val) : NULL;
    set_current_val(&v, FALSE, FALSE);

    save_var = (*cl)->var;
    (*cl)->var = &v;
    rv = text_tool(ps, cmd, cl, flags);

    if(rv == 1){
	val = *apval;
	*apval = NULL;
	if(val)
	  removing_leading_and_trailing_white_space(val);
	
	if(hc->val)
	  fs_give((void **)&hc->val);
	
	hc->val = string_to_pattern(val);

	(*cl)->var = save_var;

	if((*alval)[CFC_ICUST(*cl)])
	  fs_give((void **)&(*alval)[CFC_ICUST(*cl)]);

	(*alval)[CFC_ICUST(*cl)] = var_from_spec_color(hc);
	set_current_color_vals(ps);
	ps->mangled_screen = 1;
    }
    else
      (*cl)->var = save_var;
    
    if(hcolors)
      free_spec_colors(&hcolors);
    if(*apval)
      fs_give((void **)apval);
    if(v.current_val.p)
      fs_give((void **)&v.current_val.p);
    if(starting_val)
      fs_give((void **)&starting_val);

    return(rv);
}


/*
 * Test whether or not a var is one of the vars which might have a
 * color value stored in it.
 *
 * returns:  1 if it is a color var, 0 otherwise
 */
int
color_holding_var(struct pine *ps, struct variable *var)
{
    if(treat_color_vars_as_text)
      return(0);
    else
      return(var && var->name &&
	     (srchstr(var->name, "-foreground-color") ||
	      srchstr(var->name, "-background-color") ||
	      var == &ps->vars[V_VIEW_HDR_COLORS] ||
	      var == &ps->vars[V_KW_COLORS]));
}


/*
 * test whether or not a var is one of the vars having to do with color
 *
 * returns:  1 if it is a color var, 0 otherwise
 */
int
color_related_var(struct pine *ps, struct variable *var)
{
    return(
#ifndef	_WINDOWS
	   var == &ps->vars[V_COLOR_STYLE] ||
#endif
           var == &ps->vars[V_INDEX_COLOR_STYLE] ||
           var == &ps->vars[V_TITLEBAR_COLOR_STYLE] ||
	   color_holding_var(ps, var));
}


int
offer_normal_color_for_var(struct pine *ps, struct variable *var)
{
    return(color_holding_var(ps, var)
	   && var != &ps->vars[V_NORM_FORE_COLOR]
	   && var != &ps->vars[V_NORM_BACK_COLOR]
	   && var != &ps->vars[V_REV_FORE_COLOR]
	   && var != &ps->vars[V_REV_BACK_COLOR]
	   && var != &ps->vars[V_SLCTBL_FORE_COLOR]
	   && var != &ps->vars[V_SLCTBL_BACK_COLOR]);
}


int
offer_none_color_for_var(struct pine *ps, struct variable *var)
{
    return(color_holding_var(ps, var)
	   && (!struncmp(var->name, "index-", 6)
	       || var == &ps->vars[V_KW_COLORS]));
}


int
color_setting_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int	             rv = 0, i, cancel = 0, deefault;
    int              curcolor, prevcolor, nextcolor, another;
    CONF_S          *ctmp, *first_line, *beg = NULL, *end = NULL,
		    *cur_beg, *cur_end, *prev_beg, *prev_end,
		    *next_beg, *next_end;
    struct variable *v, *fgv, *bgv, *setv = NULL, *otherv;
    SPEC_COLOR_S    *hc = NULL, *new_hcolor;
    SPEC_COLOR_S    *hcolors = NULL;
    KEYWORD_S       *kw;
    char            *old_val, *confline = NULL;
    char             prompt[100], sval[MAXPATH+1];
    char           **lval, **apval, ***alval, **t;
    char           **apval1, **apval2;
    HelpType         help;
    ESCKEY_S         opts[3];
#ifdef	_WINDOWS
    char            *pval;
#endif

    sval[0] = '\0';

    switch(cmd){
      case MC_CHOICE :				/* set a color */

	if(((*cl)->flags & CF_VAR2 && fixed_var((*cl)->var+1, NULL, NULL)) ||
	   (!((*cl)->flags & CF_VAR2) && fixed_var((*cl)->var, NULL, NULL))){
	    if(((*cl)->var->post_user_val.p ||
	        ((*cl)->var+1)->post_user_val.p ||
	        (*cl)->var->main_user_val.p ||
		((*cl)->var+1)->main_user_val.p)
	       && want_to(_("Delete old unused personal option setting"),
			  'y', 'n', NO_HELP, WT_FLUSH_IN) == 'y'){
		delete_user_vals((*cl)->var);
		delete_user_vals((*cl)->var+1);
		q_status_message(SM_ORDER, 0, 3, _("Deleted"));
		rv = 1;
	    }

	    return(rv);
	}

	fgv = (*cl)->var;				/* foreground color */
	bgv = (*cl)->var+1;				/* background color */
	v = ((*cl)->flags & CF_VAR2) ? bgv : fgv;	/* var being changed */

	apval = APVAL(v, ew);
	old_val = apval ? *apval : NULL;

	if(apval){
	    if(CFC_ICOLOR(*cl) < pico_count_in_color_table())
	      *apval = cpystr(colorx(CFC_ICOLOR(*cl)));
	    else if(CFC_ICOLOR(*cl) == CFC_ICOLOR_NORM)
	      *apval = cpystr(MATCH_NORM_COLOR);
	    else if(CFC_ICOLOR(*cl) == CFC_ICOLOR_NONE)
	      *apval = cpystr(MATCH_NONE_COLOR);
	    else if(old_val)
	      *apval = cpystr(is_rgb_color(old_val)
			       ? old_val : color_to_asciirgb(old_val));
	    else if(v->current_val.p)
	      *apval = cpystr(is_rgb_color(v->current_val.p)
				       ? v->current_val.p
				       : color_to_asciirgb(v->current_val.p));
	    else if(v == fgv)
	      *apval = cpystr(color_to_asciirgb(colorx(COL_BLACK)));
	    else
	      *apval = cpystr(color_to_asciirgb(colorx(COL_WHITE)));
	}

	if(old_val)
	  fs_give((void **)&old_val);

	set_current_val(v, TRUE, FALSE);

	/*
	 * If the user sets one of foreground/background and the
	 * other is not yet set, set the other.
	 */
	if(PVAL(v, ew)){
	    if(v == fgv && !PVAL(bgv, ew)){
		setv   = bgv;
		otherv = fgv;
	    }
	    else if(v == bgv && !PVAL(fgv, ew)){
		setv   = fgv;
		otherv = bgv;
	    }
	}
	
	if(setv){
	    if((apval = APVAL(setv, ew)) != NULL){
		if(setv->current_val.p)
		  *apval = cpystr(setv->current_val.p);
		else if (setv == fgv && ps_global->VAR_NORM_FORE_COLOR)
		  *apval = cpystr(ps_global->VAR_NORM_FORE_COLOR);
		else if (setv == bgv && ps_global->VAR_NORM_BACK_COLOR)
		  *apval = cpystr(ps_global->VAR_NORM_BACK_COLOR);
		else if(!strucmp(color_to_canonical_name(otherv->current_val.p),
				 colorx(COL_WHITE)))
		  *apval = cpystr(colorx(COL_BLACK));
		else
		  *apval = cpystr(colorx(COL_WHITE));
	    }

	    set_current_val(setv, TRUE, FALSE);
	}

	fix_side_effects(ps, v, 0);
	set_current_color_vals(ps);

	/*
	 * Turn on selected *'s for default selections, if any, and
	 * for ones we forced on.
	 */
	color_update_selected(ps, *cl, PVAL(fgv, ew), PVAL(bgv, ew), TRUE);

	ClearScreen();
	rv = ps->mangled_screen = 1;
	break;

      case MC_CHOICEB :				/* set a custom hdr color */
	/*
	 * Find the SPEC_COLOR_S for header.
	 */
	lval = LVAL((*cl)->var, ew);
	hcolors = spec_colors_from_varlist(lval, 0);
	for(hc = hcolors, i=0; hc; hc = hc->next, i++)
	  if(CFC_ICUST(*cl) == i)
	    break;

	if(hc){
	    if((*cl)->flags & CF_VAR2){
		old_val = hc->bg;
		if(CFC_ICOLOR(*cl) < pico_count_in_color_table())
		  hc->bg = cpystr(colorx(CFC_ICOLOR(*cl)));
		else if(CFC_ICOLOR(*cl) == CFC_ICOLOR_NORM)
		  hc->bg = cpystr(MATCH_NORM_COLOR);
		else if(CFC_ICOLOR(*cl) == CFC_ICOLOR_NONE)
		  hc->bg = cpystr(MATCH_NONE_COLOR);
		else if(old_val)
		  hc->bg = cpystr(is_rgb_color(old_val)
				    ? old_val
				    : color_to_asciirgb(old_val));
		else
		  hc->bg = cpystr(color_to_asciirgb(colorx(COL_WHITE)));

		if(old_val)
		  fs_give((void **) &old_val);

		/*
		 * If the user sets one of foreground/background and the
		 * other is not yet set, set it.
		 */
		if(!(hc->fg && hc->fg[0])){
		    if(hc->fg)
		      fs_give((void **)&hc->fg);

		    hc->fg = cpystr(ps->VAR_NORM_FORE_COLOR);
		}
	    }
	    else{
		old_val = hc->fg;

		if(CFC_ICOLOR(*cl) < pico_count_in_color_table())
		  hc->fg = cpystr(colorx(CFC_ICOLOR(*cl)));
		else if(CFC_ICOLOR(*cl) == CFC_ICOLOR_NORM)
		  hc->fg = cpystr(MATCH_NORM_COLOR);
		else if(CFC_ICOLOR(*cl) == CFC_ICOLOR_NONE)
		  hc->fg = cpystr(MATCH_NONE_COLOR);
		else if(old_val)
		  hc->fg = cpystr(is_rgb_color(old_val)
				    ? old_val
				    : color_to_asciirgb(old_val));
		else
		  hc->fg = cpystr(color_to_asciirgb(colorx(COL_BLACK)));

		if(old_val)
		  fs_give((void **) &old_val);

		if(!(hc->bg && hc->bg[0])){
		    if(hc->bg)
		      fs_give((void **)&hc->bg);

		    hc->bg = cpystr(ps->VAR_NORM_BACK_COLOR);
		}
	    }
	}

	/*
	 * Turn on selected *'s for default selections, if any, and
	 * for ones we forced on.
	 */
	color_update_selected(ps, *cl, 
			      (hc && hc->fg && hc->fg[0]
			       && hc->bg && hc->bg[0])
				  ? hc->fg : ps->VAR_NORM_FORE_COLOR,
			      (hc && hc->fg && hc->fg[0]
			       && hc->bg && hc->bg[0])
				  ? hc->bg : ps->VAR_NORM_BACK_COLOR,
			      TRUE);

	if(hc && lval && lval[i]){
	    fs_give((void **)&lval[i]);
	    lval[i] = var_from_spec_color(hc);
	}
	
	if(hcolors)
	  free_spec_colors(&hcolors);

	set_current_color_vals(ps);
	ClearScreen();
	rv = ps->mangled_screen = 1;
	break;


      case MC_CHOICEC :			/* set a custom keyword color */
	/* find keyword associated with color we are editing */
	for(kw=ps->keywords, i=0; kw; kw=kw->next, i++)
	  if(CFC_ICUST(*cl) == i)
	    break;

	if(!kw){					/* can't happen */
	    dprint((1,
		"This can't happen, kw not set when setting keyword color\n"));
	    break;
	}

	hcolors = spec_colors_from_varlist(LVAL((*cl)->var, ew), 0);

	/*
	 * Look through hcolors, derived from lval, to find this keyword
	 * and its current color.
	 */
	for(hc = hcolors; hc; hc = hc->next)
	  if(hc->spec && ((kw->nick && !strucmp(kw->nick, hc->spec))
			  || (kw->kw && !strucmp(kw->kw, hc->spec))))
	    break;

	if(!hc){	/* this keyword didn't have a color set, add to list */
	    SPEC_COLOR_S *new;

	    new = (SPEC_COLOR_S *) fs_get(sizeof(*hc));
	    memset((void *) new, 0, sizeof(*new));
	    new->spec = cpystr(kw->kw);
	    new->fg = cpystr(ps->VAR_NORM_FORE_COLOR);
	    new->bg = cpystr(ps->VAR_NORM_BACK_COLOR);

	    if(hcolors){
		for(hc = hcolors; hc->next; hc = hc->next)
		  ;

		hc->next = new;
	    }
	    else
	      hcolors = new;

	    hc = new;
	}

	if(hc){
	    if((*cl)->flags & CF_VAR2){
		old_val = hc->bg;
		if(CFC_ICOLOR(*cl) < pico_count_in_color_table())
		  hc->bg = cpystr(colorx(CFC_ICOLOR(*cl)));
		else if(CFC_ICOLOR(*cl) == CFC_ICOLOR_NORM)
		  hc->bg = cpystr(MATCH_NORM_COLOR);
		else if(CFC_ICOLOR(*cl) == CFC_ICOLOR_NONE)
		  hc->bg = cpystr(MATCH_NONE_COLOR);
		else if(old_val)
		  hc->bg = cpystr(is_rgb_color(old_val)
				    ? old_val
				    : color_to_asciirgb(old_val));
		else
		  hc->bg = cpystr(color_to_asciirgb(colorx(COL_WHITE)));

		if(old_val)
		  fs_give((void **) &old_val);

		/*
		 * If the user sets one of foreground/background and the
		 * other is not yet set, set it.
		 */
		if(!(hc->fg && hc->fg[0])){
		    if(hc->fg)
		      fs_give((void **)&hc->fg);

		    hc->fg = cpystr(ps->VAR_NORM_FORE_COLOR);
		}
	    }
	    else{
		old_val = hc->fg;

		if(CFC_ICOLOR(*cl) < pico_count_in_color_table())
		  hc->fg = cpystr(colorx(CFC_ICOLOR(*cl)));
		else if(CFC_ICOLOR(*cl) == CFC_ICOLOR_NORM)
		  hc->fg = cpystr(MATCH_NORM_COLOR);
		else if(CFC_ICOLOR(*cl) == CFC_ICOLOR_NONE)
		  hc->fg = cpystr(MATCH_NONE_COLOR);
		else if(old_val)
		  hc->fg = cpystr(is_rgb_color(old_val)
				    ? old_val
				    : color_to_asciirgb(old_val));
		else
		  hc->fg = cpystr(color_to_asciirgb(colorx(COL_BLACK)));

		if(old_val)
		  fs_give((void **) &old_val);

		if(!(hc->bg && hc->bg[0])){
		    if(hc->bg)
		      fs_give((void **)&hc->bg);

		    hc->bg = cpystr(ps->VAR_NORM_BACK_COLOR);
		}
	    }
	}

	/*
	 * Turn on selected *'s for default selections, if any, and
	 * for ones we forced on.
	 */
	color_update_selected(ps, *cl, 
			      (hc && hc->fg && hc->fg[0]
			       && hc->bg && hc->bg[0])
				  ? hc->fg : ps->VAR_NORM_FORE_COLOR,
			      (hc && hc->fg && hc->fg[0]
			       && hc->bg && hc->bg[0])
				  ? hc->bg : ps->VAR_NORM_BACK_COLOR,
			      TRUE);

	alval = ALVAL((*cl)->var, ew);
	free_list_array(alval);
	*alval = varlist_from_spec_colors(hcolors);

	if(hcolors)
	  free_spec_colors(&hcolors);

	fix_side_effects(ps, (*cl)->var, 0);
	set_current_color_vals(ps);
	ClearScreen();
	rv = ps->mangled_screen = 1;
	break;

      case MC_TOGGLE :		/* toggle default on or off */
	fgv = (*cl)->var;				/* foreground color */
	bgv = (*cl)->var+1;				/* background color */

	if((*cl)->value[1] == 'X'){		/* turning default off */
	    (*cl)->value[1] = ' ';
	    /*
	     * Take whatever color is the current_val and suck it
	     * into the user_val. Same colors remain checked.
	     */
	    apval1 = APVAL(fgv, ew);
	    if(apval1){
		if(*apval1)
		  fs_give((void **)apval1);
	    }

	    apval2 = APVAL(bgv, ew);
	    if(apval2){
		if(*apval2)
		  fs_give((void **)apval2);
	    }

	    /* editing normal but there is an exception config */
	    if((ps->ew_for_except_vars != Main) && (ew == Main)){
		COLOR_PAIR *newc;

		/* use global_val if it is set */
		if(fgv && fgv->global_val.p && fgv->global_val.p[0] &&
		   bgv && bgv->global_val.p && bgv->global_val.p[0]){
		    *apval1 = cpystr(fgv->global_val.p);
		    *apval2 = cpystr(bgv->global_val.p);
		}
		else if(var_defaults_to_rev(fgv) &&
		        (newc = pico_get_rev_color())){
		    *apval1 = cpystr(newc->fg);
		    *apval2 = cpystr(newc->bg);
		}
		else{
		    *apval1 = cpystr(ps->VAR_NORM_FORE_COLOR);
		    *apval2 = cpystr(ps->VAR_NORM_BACK_COLOR);
		}
	    }
	    else{				/* editing outermost config */
		/* just use current_vals */
		if(fgv->current_val.p)
		  *apval1 = cpystr(fgv->current_val.p);
		if(bgv->current_val.p)
		  *apval2 = cpystr(bgv->current_val.p);
	    }
	}
	else{					/* turning default on */
	    char *starred_fg = NULL, *starred_bg = NULL;

	    (*cl)->value[1] = 'X';
	    apval = APVAL(fgv, ew);
	    if(apval && *apval)
	      fs_give((void **)apval);

	    apval = APVAL(bgv, ew);
	    if(apval && *apval)
	      fs_give((void **)apval);

	    if(fgv->cmdline_val.p)
	      fs_give((void **)&fgv->cmdline_val.p);

	    if(bgv->cmdline_val.p)
	      fs_give((void **)&bgv->cmdline_val.p);

	    set_current_color_vals(ps);

	    if(fgv == &ps->vars[V_SLCTBL_FORE_COLOR]){
		F_TURN_OFF(F_SLCTBL_ITEM_NOBOLD, ps);
		(*cl)->next->value[1] = 'X';
	    }

	    /* editing normal but there is an exception config */
	    if((ps->ew_for_except_vars != Main) && (ew == Main)){
		COLOR_PAIR *newc;

		/* use global_val if it is set */
		if(fgv && fgv->global_val.p && fgv->global_val.p[0] &&
		   bgv && bgv->global_val.p && bgv->global_val.p[0]){
		    starred_fg = fgv->global_val.p;
		    starred_bg = bgv->global_val.p;
		}
		else if(var_defaults_to_rev(fgv) &&
		        (newc = pico_get_rev_color())){
		    starred_fg = newc->fg;
		    starred_bg = newc->bg;
		}
		else{
		    starred_fg = ps->VAR_NORM_FORE_COLOR;
		    starred_bg = ps->VAR_NORM_BACK_COLOR;
		}
	    }
	    else{				/* editing outermost config */
		starred_fg = fgv->current_val.p;
		starred_bg = bgv->current_val.p;
	    }

	    /*
	     * Turn on selected *'s for default selections.
	     */
	    color_update_selected(ps, prev_confline(*cl),
				  starred_fg, starred_bg, FALSE);

	    ps->mangled_body = 1;
	}

	fix_side_effects(ps, fgv, 0);
	rv = 1;
	break;

      case MC_TOGGLEB :		/* toggle default on or off, hdr color */
	/*
	 * Find the SPEC_COLOR_S for header.
	 */
	rv = 1;
	lval = LVAL((*cl)->var, ew);
	hcolors = spec_colors_from_varlist(lval, 0);
	for(hc = hcolors, i=0; hc; hc = hc->next, i++)
	  if(CFC_ICUST(*cl) == i)
	    break;

	if((*cl)->value[1] == 'X'){		/* turning default off */
	    (*cl)->value[1] = ' ';
	    /*
	     * Take whatever color is the default value and suck it
	     * into the hc structure.
	     */
	    if(hc){
		if(hc->bg)
		  fs_give((void **)&hc->bg);
		if(hc->fg)
		  fs_give((void **)&hc->fg);
		
		if(ps->VAR_NORM_FORE_COLOR &&
		   ps->VAR_NORM_FORE_COLOR[0] &&
		   ps->VAR_NORM_BACK_COLOR &&
		   ps->VAR_NORM_BACK_COLOR[0]){
		    hc->fg = cpystr(ps->VAR_NORM_FORE_COLOR);
		    hc->bg = cpystr(ps->VAR_NORM_BACK_COLOR);
		}

		if(lval && lval[i]){
		    fs_give((void **)&lval[i]);
		    lval[i] = var_from_spec_color(hc);
		}
	    }
	}
	else{					/* turning default on */
	    (*cl)->value[1] = 'X';
	    /* Remove current colors, leaving val */
	    if(hc){
		if(hc->bg)
		  fs_give((void **)&hc->bg);
		if(hc->fg)
		  fs_give((void **)&hc->fg);

		if(lval && lval[i]){
		    fs_give((void **)&lval[i]);
		    lval[i] = var_from_spec_color(hc);
		}
	    }

	    set_current_color_vals(ps);
	    ClearScreen();
	    ps->mangled_screen = 1;

	}

	if(hcolors)
	  free_spec_colors(&hcolors);

	/*
	 * Turn on selected *'s for default selections.
	 */
	color_update_selected(ps, prev_confline(*cl),
			      ps->VAR_NORM_FORE_COLOR,
			      ps->VAR_NORM_BACK_COLOR,
			      FALSE);

	break;

      case MC_TOGGLEC :		/* toggle default on or off, keyword color */
	for(kw=ps->keywords, i=0; kw; kw=kw->next, i++)
	  if(CFC_ICUST(*cl) == i)
	    break;

	if(!kw){					/* can't happen */
	    dprint((1,
		"This can't happen, kw not set when togglec keyword color\n"));
	    break;
	}

	hcolors = spec_colors_from_varlist(LVAL((*cl)->var, ew), 0);

	/*
	 * Look through hcolors, derived from lval, to find this keyword
	 * and its current color.
	 */
	for(hc = hcolors; hc; hc = hc->next)
	  if(hc->spec && ((kw->nick && !strucmp(kw->nick, hc->spec))
			  || (kw->kw && !strucmp(kw->kw, hc->spec))))
	    break;

	/* Remove this color from list */
	if(hc){
	    SPEC_COLOR_S *tmp;

	    if(hc == hcolors){
		hcolors = hc->next;
		hc->next = NULL;
		free_spec_colors(&hc);
	    }
	    else{
		for(tmp = hcolors; tmp->next; tmp = tmp->next)
		  if(tmp->next == hc)
		    break;

		if(tmp->next){
		    tmp->next = hc->next;
		    hc->next = NULL;
		    free_spec_colors(&hc);
		}
	    }
	}

	if((*cl)->value[1] == 'X')
	  (*cl)->value[1] = ' ';
	else
	  (*cl)->value[1] = 'X';

	/*
	 * Turn on selected *'s for default selections, if any, and
	 * for ones we forced on.
	 */
	color_update_selected(ps, prev_confline(*cl), 
			      ps->VAR_NORM_FORE_COLOR,
			      ps->VAR_NORM_BACK_COLOR,
			      FALSE);

	alval = ALVAL((*cl)->var, ew);
	free_list_array(alval);
	*alval = varlist_from_spec_colors(hcolors);

	if(hcolors)
	  free_spec_colors(&hcolors);

	fix_side_effects(ps, (*cl)->var, 0);
	set_current_color_vals(ps);
	ClearScreen();
	rv = ps->mangled_screen = 1;
	break;

      case MC_TOGGLED :		/* toggle selectable item bold on or off */
	toggle_feature_bit(ps, feature_list_index(F_SLCTBL_ITEM_NOBOLD),
			   &ps->vars[V_FEATURE_LIST], *cl, 1);
	ps->mangled_body = 1;		/* to fix Sample Text */
	rv = 1;
	break;

      case MC_DEFAULT :				/* restore default values */

	/* First, confirm that user wants to restore all default colors */
	if(want_to(_("Really restore all colors to default values"),
		   'y', 'n', NO_HELP, WT_NORM) != 'y'){
	    cmd_cancelled("RestoreDefs");
	    return(rv);
	}

	/* get rid of all user set colors */
	for(v = ps->vars; v->name; v++){
	    if(!color_holding_var(ps, v)
	       || v == &ps->vars[V_VIEW_HDR_COLORS]
	       || v == &ps->vars[V_KW_COLORS])
	      continue;

	    apval = APVAL(v, ew);
	    if(apval && *apval)
	      fs_give((void **)apval);

	    if(v->cmdline_val.p)
	      fs_give((void **)&v->cmdline_val.p);
	}

	/*
	 * For custom header colors, we want to remove the color values
	 * but leave the spec value so that it is easy to reset.
	 */
	alval = ALVAL(&ps->vars[V_VIEW_HDR_COLORS], ew);
	if(alval && *alval){
	    SPEC_COLOR_S *global_hcolors = NULL, *hcg;

	    v = &ps->vars[V_VIEW_HDR_COLORS];
	    if(v->global_val.l && v->global_val.l[0])
	      global_hcolors = spec_colors_from_varlist(v->global_val.l, 0);

	    hcolors = spec_colors_from_varlist(*alval, 0);
	    for(hc = hcolors; hc; hc = hc->next){
		if(hc->fg)
		  fs_give((void **)&hc->fg);
		if(hc->bg)
		  fs_give((void **)&hc->bg);

		for(hcg = global_hcolors; hcg; hcg = hcg->next){
		    if(hc->spec && hcg->spec && !strucmp(hc->spec, hcg->spec)){
			hc->fg = hcg->fg;
			hcg->fg = NULL;
			hc->bg = hcg->bg;
			hcg->bg = NULL;
			if(hc->val && !hcg->val)
			  fs_give((void **) &hc->val);
		    }
		}

		if(global_hcolors)
		  free_spec_colors(&global_hcolors);
	    }

	    free_list_array(alval);
	    *alval = varlist_from_spec_colors(hcolors);

	    if(hcolors)
	      free_spec_colors(&hcolors);
	}

	/*
	 * Same for keyword colors.
	 */
	alval = ALVAL(&ps->vars[V_KW_COLORS], ew);
	if(alval && *alval){
	    hcolors = spec_colors_from_varlist(*alval, 0);
	    for(hc = hcolors; hc; hc = hc->next){
		if(hc->fg)
		  fs_give((void **)&hc->fg);
		if(hc->bg)
		  fs_give((void **)&hc->bg);
	    }

	    free_list_array(alval);
	    *alval = varlist_from_spec_colors(hcolors);

	    if(hcolors)
	      free_spec_colors(&hcolors);
	}

	/* set bold for selectable items */
	F_TURN_OFF(F_SLCTBL_ITEM_NOBOLD, ps);
	lval = LVAL(&ps->vars[V_FEATURE_LIST], ew);
	if(test_feature(lval, feature_list_name(F_SLCTBL_ITEM_NOBOLD), 0))
	  toggle_feature_bit(ps, feature_list_index(F_SLCTBL_ITEM_NOBOLD),
			     &ps->vars[V_FEATURE_LIST], *cl, 1);

	set_current_color_vals(ps);
	clear_index_cache(ps->mail_stream, 0);

	/* redo config display */
	*cl = first_confline(*cl);
	free_conflines(cl);
	opt_screen->top_line = NULL;
	first_line = NULL;
	color_config_init_display(ps, cl, &first_line);
	*cl = first_line;
	ClearScreen();
	ps->mangled_screen = 1;
	rv = 1;
	break;

      case MC_ADD :				/* add custom header color */
	/* get header field name */
	help = NO_HELP;
	while(1){
	    i = optionally_enter(sval, -FOOTER_ROWS(ps), 0, sizeof(sval),
			     _("Enter the name of the header field to be added: "),
				 NULL, help, NULL);
	    if(i == 0)
	      break;
	    else if(i == 1){
		cmd_cancelled("Add");
		cancel = 1;
		break;
	    }
	    else if(i == 3){
		help = help == NO_HELP ? h_config_add_custom_color : NO_HELP;
		continue;
	    }
	    else
	      break;
	}

	ps->mangled_footer = 1;

	removing_leading_and_trailing_white_space(sval);
	if(sval[strlen(sval)-1] == ':')  /* remove trailing colon */
	  sval[strlen(sval)-1] = '\0';

	removing_trailing_white_space(sval);

	if(cancel || !sval[0])
	  break;

	new_hcolor = (SPEC_COLOR_S *)fs_get(sizeof(*new_hcolor));
	memset((void *)new_hcolor, 0, sizeof(*new_hcolor));
	new_hcolor->spec = cpystr(sval);
	confline = var_from_spec_color(new_hcolor);

	/* add it to end of list */
	alval = ALVAL(&ps->vars[V_VIEW_HDR_COLORS], ew);
	if(alval){
	    /* get rid of possible empty value first */
	    if((t = *alval) && t[0] && !t[0][0] && !(t+1)[0])
	      free_list_array(alval);

	    for(t = *alval, i=0; t && t[0]; t++)
	      i++;
	    
	    if(i)
	      fs_resize((void **)alval, sizeof(char *) * (i+1+1));
	    else
	      *alval = (char **)fs_get(sizeof(char *) * (i+1+1));
	    
	    (*alval)[i] = confline;
	    (*alval)[i+1] = NULL;
	}

	set_current_color_vals(ps);

	/* go to end of display */
	for(ctmp = *cl; ctmp && ctmp->next; ctmp = next_confline(ctmp))
	  ;

	/* back up to the KEYWORD COLORS title line */
	for(; ctmp && (!ctmp->value || strcmp(ctmp->value, KW_COLORS_HDR))
	      && ctmp->prev;
	    ctmp = prev_confline(ctmp))
	  ;

	/*
	 * Back up to last header line, or comment line if no header lines.
	 * One of many in a long line of dangerous moves in the config
	 * screens.
	 */
	ctmp = prev_confline(ctmp);		/* ------- line */
	ctmp = prev_confline(ctmp);		/* blank line */
	ctmp = prev_confline(ctmp);		/* the line */
	
	*cl = ctmp;
	
	/* delete the comment line if there were no headers before this */
	if(i == 0){
	    beg = ctmp->prev;
	    end = ctmp;

	    *cl = beg ? beg->prev : NULL;

	    if(beg && beg->prev)		/* this will always be true */
	      beg->prev->next = end ? end->next : NULL;
	    
	    if(end && end->next)
	      end->next->prev = beg ? beg->prev : NULL;
	    
	    if(end)
	      end->next = NULL;
	    
	    if(beg == opt_screen->top_line || end == opt_screen->top_line)
	      opt_screen->top_line = NULL;

	    free_conflines(&beg);
	}

	add_header_color_line(ps, cl, confline, i);

	/* be sure current is on selectable line */
	for(; *cl && ((*cl)->flags & CF_NOSELECT); *cl = next_confline(*cl))
	  ;
	for(; *cl && ((*cl)->flags & CF_NOSELECT); *cl = prev_confline(*cl))
	  ;

	rv = ps->mangled_body = 1;
	break;

      case MC_DELETE :				/* delete custom header color */
	if((*cl)->var != &ps->vars[V_VIEW_HDR_COLORS]){
	    q_status_message(SM_ORDER, 0, 2,
			     _("Can't delete this color setting"));
	    break;
	}

	if(want_to(_("Really delete header color from config"),
		   'y', 'n', NO_HELP, WT_NORM) != 'y'){
	    cmd_cancelled("Delete");
	    return(rv);
	}

	alval = ALVAL((*cl)->var, ew);
	if(alval){
	    int n, j;

	    for(t = *alval, n=0; t && t[0]; t++)
	      n++;
	    
	    j = CFC_ICUST(*cl);

	    if(n > j){		/* it better be */
		if((*alval)[j])
		  fs_give((void **)&(*alval)[j]);

		for(i = j; i < n; i++)
		  (*alval)[i] = (*alval)[i+1];
	    }
	}

	set_current_color_vals(ps);

	/*
	 * Note the conf lines that go with this header. That's the
	 * blank line before and the current line.
	 */
	beg = (*cl)->prev;
	end = *cl;

	another = 0;
	/* reset current line */
	if(end && end->next && end->next->next && 
	   end->next->next->var == &ps->vars[V_VIEW_HDR_COLORS]){
	    *cl = end->next->next;		/* next Header Color */
	    another++;
	}
	else if(beg && beg->prev &&
	   beg->prev->var == &ps->vars[V_VIEW_HDR_COLORS]){
	    *cl = beg->prev;			/* prev Header Color */
	    another++;
	}

	/* adjust SPEC_COLOR_S index (varmem) values */
	for(ctmp = end; ctmp; ctmp = next_confline(ctmp))
	  if(ctmp->var == &ps->vars[V_VIEW_HDR_COLORS])
	    ctmp->varmem = CFC_ICUST_DEC(ctmp);
	
	/*
	 * If that was the last header color line, add in the comment
	 * line placeholder. If there is another, just delete the
	 * old conf lines.
	 */
	if(another){
	    if(beg && beg->prev)		/* this will always be true */
	      beg->prev->next = end ? end->next : NULL;
	    
	    if(end && end->next)
	      end->next->prev = beg ? beg->prev : NULL;
	    
	    if(end)
	      end->next = NULL;
	    
	    if(beg == opt_screen->top_line || end == opt_screen->top_line)
	      opt_screen->top_line = NULL;

	    free_conflines(&beg);
	}
	else if(end){
	    if(end->varname)
	      fs_give((void **) &end->varname);

	    if(end->value)
	      fs_give((void **) &end->value);

	    end->flags     = CF_NOSELECT;
	    end->help      = NO_HELP;
	    end->value     = cpystr(_(ADDHEADER_COMMENT));
	    end->valoffset = COLOR_INDENT;
	    end->varnamep  = NULL;
	    end->varmem    = 0;
	    end->keymenu   = NULL;
	    end->tool      = NULL;
	}

	/* if not selectable, find next selectable line */
	for(; *cl && ((*cl)->flags & CF_NOSELECT) && next_confline(*cl); *cl = next_confline(*cl))
	  ;
	/* if no next selectable line, search backwards for one */
	for(; *cl && ((*cl)->flags & CF_NOSELECT) && prev_confline(*cl); *cl = prev_confline(*cl))
	  ;

	rv = ps->mangled_body = 1;
	q_status_message(SM_ORDER, 0, 3, _("header color deleted"));
	break;

      case MC_SHUFFLE :  /* shuffle order of custom headers */
	if((*cl)->var != &ps->vars[V_VIEW_HDR_COLORS]){
	    q_status_message(SM_ORDER, 0, 2,
			     _("Can't shuffle this color setting"));
	    break;
	}

	alval = ALVAL((*cl)->var, ew);
	if(!alval)
	  return(rv);

	curcolor = CFC_ICUST(*cl);
	prevcolor = curcolor-1;
	nextcolor = curcolor+1;
	if(!*alval || !(*alval)[nextcolor])
	  nextcolor = -1;

	if((prevcolor < 0 && nextcolor < 0) || !*alval){
	    q_status_message(SM_ORDER, 0, 3,
   _("Shuffle only makes sense when there is more than one Header Color defined"));
	    return(rv);
	}

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

	if(prevcolor < 0){		/* no up */
	    opts[0].ch = -2;
	    deefault = 'd';
	}
	else if(nextcolor < 0)
	  opts[1].ch = -2;	/* no down */

	snprintf(prompt, sizeof(prompt), _("Shuffle %s%s%s ? "),
		(opts[0].ch != -2) ? _("UP") : "",
		(opts[0].ch != -2 && opts[1].ch != -2) ? " or " : "",
		(opts[1].ch != -2) ? _("DOWN") : "");
	prompt[sizeof(prompt)-1] = '\0';
	help = (opts[0].ch == -2) ? h_hdrcolor_shuf_down
				  : (opts[1].ch == -2) ? h_hdrcolor_shuf_up
						       : h_hdrcolor_shuf;

	i = radio_buttons(prompt, -FOOTER_ROWS(ps), opts, deefault, 'x',
			   help, RB_NORM);

	switch(i){
	  case 'x':
	    cmd_cancelled("Shuffle");
	    return(rv);

	  case 'u':
	  case 'd':
	    break;
	}
		
	/* swap order */
	if(i == 'd'){
	    old_val = (*alval)[curcolor];
	    (*alval)[curcolor] = (*alval)[nextcolor];
	    (*alval)[nextcolor] = old_val;
	}
	else if(i == 'u'){
	    old_val = (*alval)[curcolor];
	    (*alval)[curcolor] = (*alval)[prevcolor];
	    (*alval)[prevcolor] = old_val;
	}
	else		/* can't happen */
	  return(rv);

	set_current_color_vals(ps);

	/*
	 * Swap the conf lines.
	 */

	cur_beg = (*cl)->prev;
	cur_end = *cl;

	if(i == 'd'){
	    next_beg = cur_end->next;
	    next_end = next_beg ? next_beg->next : NULL;

	    if(next_end->next)
	      next_end->next->prev = cur_end;
	    cur_end->next = next_end->next;
	    next_end->next = cur_beg;
	    if(cur_beg->prev)
	      cur_beg->prev->next = next_beg;
	    next_beg->prev = cur_beg->prev;
	    cur_beg->prev = next_end;

	    /* adjust SPEC_COLOR_S index values */
	    cur_beg->varmem	  = CFC_ICUST_INC(cur_beg);
	    cur_beg->next->varmem = CFC_ICUST_INC(cur_beg->next);

	    next_beg->varmem	   = CFC_ICUST_DEC(next_beg);
	    next_beg->next->varmem = CFC_ICUST_DEC(next_beg->next);

	    if(opt_screen->top_line == cur_end)
	      opt_screen->top_line = next_end;
	    else if(opt_screen->top_line == cur_beg)
	      opt_screen->top_line = next_beg;
	}
	else{
	    prev_end = cur_beg->prev;
	    prev_beg = prev_end ? prev_end->prev : NULL;

	    if(prev_beg && prev_beg->prev)
	      prev_beg->prev->next = cur_beg;
	    cur_beg->prev = prev_beg->prev;
	    prev_beg->prev = cur_end;
	    if(cur_end->next)
	      cur_end->next->prev = prev_end;
	    prev_end->next = cur_end->next;
	    cur_end->next = prev_beg;

	    /* adjust SPEC_COLOR_S index values */
	    cur_beg->varmem	  = CFC_ICUST_DEC(cur_beg);
	    cur_beg->next->varmem = CFC_ICUST_DEC(cur_beg->next);

	    prev_beg->varmem	   = CFC_ICUST_INC(prev_beg);
	    prev_beg->next->varmem = CFC_ICUST_INC(prev_beg->next);

	    if(opt_screen->top_line == prev_end)
	      opt_screen->top_line = cur_end;
	    else if(opt_screen->top_line == prev_beg)
	      opt_screen->top_line = cur_beg;
	}

	rv = ps->mangled_body = 1;
	q_status_message(SM_ORDER, 0, 3, _("Header Colors shuffled"));
	break;

      case MC_EDIT:
	rv = color_edit_screen(ps, cl);
	if((*cl)->value && (*cl)->var &&
	   srchstr((*cl)->var->name, "-foreground-color")){
	    fs_give((void **)&(*cl)->value);
	    (*cl)->value = pretty_value(ps, *cl);
	}

	break;

      case MC_EXIT:				/* exit */
	if((*cl)->keymenu == &color_changing_keymenu ||
	   (*cl)->keymenu == &kw_color_changing_keymenu ||
	   (*cl)->keymenu == &custom_color_changing_keymenu ||
	   ((*cl)->prev &&
	    ((*cl)->prev->keymenu == &color_changing_keymenu ||
	     (*cl)->prev->keymenu == &kw_color_changing_keymenu ||
	     (*cl)->prev->keymenu == &custom_color_changing_keymenu)) ||
	   ((*cl)->prev->prev &&
	    ((*cl)->prev->prev->keymenu == &color_changing_keymenu ||
	     (*cl)->prev->prev->keymenu == &kw_color_changing_keymenu ||
	     (*cl)->prev->prev->keymenu == &custom_color_changing_keymenu)))
	  rv = simple_exit_cmd(flags);
	else
	  rv = config_exit_cmd(flags);

	break;

#ifdef	_WINDOWS
      case MC_RGB1 :
	fgv = (*cl)->var;
	bgv = (*cl)->var+1;
	v = (*cl)->var;
	if((*cl)->flags & CF_VAR2)
	  v += 1;

	pval = PVAL(v, ew);
	apval = APVAL(v, ew);
	if(old_val = mswin_rgbchoice(pval ? pval : v->current_val.p)){
	    if(*apval)
	      fs_give((void **)apval);

	    *apval = old_val;
	    set_current_val(v, TRUE, FALSE);
	    fix_side_effects(ps, v, 0);
	    set_current_color_vals(ps);
	    color_update_selected(ps, *cl, PVAL(fgv, ew), PVAL(bgv, ew), TRUE);
	    rv = ps->mangled_screen = 1;
	}

	break;

      case MC_RGB2 :
	/*
	 * Find the SPEC_COLOR_S for header.
	 */
	alval = ALVAL((*cl)->var, ew);
	hcolors = spec_colors_from_varlist(*alval, 0);

	for(hc = hcolors, i = 0; hc; hc = hc->next, i++)
	  if(CFC_ICUST(*cl) == i){
	      char **pc = ((*cl)->flags & CF_VAR2) ? &hc->bg : &hc->fg;

	      if(old_val = mswin_rgbchoice(*pc)){
		  fs_give((void **) pc);
		  *pc = old_val;
		  color_update_selected(ps, *cl,
					(hc->fg && hc->fg[0]
					 && hc->bg && hc->bg[0])
					  ? hc->fg : ps->VAR_NORM_FORE_COLOR,
					(hc->fg && hc->fg[0]
					 && hc->bg && hc->bg[0])
					  ? hc->bg : ps->VAR_NORM_BACK_COLOR,
					TRUE);

		  if(hc && *alval && (*alval)[i]){
		      fs_give((void **)&(*alval)[i]);
		      (*alval)[i] = var_from_spec_color(hc);
		  }
	
		  if(hcolors)
		    free_spec_colors(&hcolors);

		  set_current_color_vals(ps);
		  ClearScreen();
		  rv = ps->mangled_screen = 1;
	      }

	      break;
	  }

	break;

      case MC_RGB3 :
	/*
	 * Custom colored keywords.
	 */
	for(kw=ps->keywords, i=0; kw; kw=kw->next, i++)
	  if(CFC_ICUST(*cl) == i)
	    break;

	if(!kw){					/* can't happen */
	    dprint((1,
		"This can't happen, kw not set in MC_RGB3\n"));
	    break;
	}

	hcolors = spec_colors_from_varlist(LVAL((*cl)->var, ew), 0);

	/*
	 * Look through hcolors, derived from lval, to find this keyword
	 * and its current color.
	 */
	for(hc = hcolors; hc; hc = hc->next)
	  if(hc->spec && ((kw->nick && !strucmp(kw->nick, hc->spec))
			  || (kw->kw && !strucmp(kw->kw, hc->spec))))
	    break;

	if(!hc){	/* this keyword didn't have a color set, add to list */
	    SPEC_COLOR_S *new;

	    new = (SPEC_COLOR_S *) fs_get(sizeof(*hc));
	    memset((void *) new, 0, sizeof(*new));
	    new->spec = cpystr(kw->kw);
	    new->fg = cpystr(ps->VAR_NORM_FORE_COLOR);
	    new->bg = cpystr(ps->VAR_NORM_BACK_COLOR);

	    if(hcolors){
		for(hc = hcolors; hc->next; hc = hc->next)
		  ;

		hc->next = new;
	    }
	    else
	      hcolors = new;

	    hc = new;
	}

	if(hc){
	    char **pc = ((*cl)->flags & CF_VAR2) ? &hc->bg : &hc->fg;

	    if(old_val = mswin_rgbchoice(*pc)){
		fs_give((void **) pc);
		*pc = old_val;

		/*
		 * Turn on selected *'s for default selections, if any, and
		 * for ones we forced on.
		 */
		color_update_selected(ps, *cl, 
				      (hc && hc->fg && hc->fg[0]
				       && hc->bg && hc->bg[0])
					  ? hc->fg : ps->VAR_NORM_FORE_COLOR,
				      (hc && hc->fg && hc->fg[0]
				       && hc->bg && hc->bg[0])
					  ? hc->bg : ps->VAR_NORM_BACK_COLOR,
				      TRUE);

		alval = ALVAL((*cl)->var, ew);
		free_list_array(alval);
		*alval = varlist_from_spec_colors(hcolors);
		fix_side_effects(ps, (*cl)->var, 0);
		set_current_color_vals(ps);
		ClearScreen();
		rv = 1;
	    }
	}

	if(hcolors)
	  free_spec_colors(&hcolors);

	ps->mangled_screen = 1;
	break;
#endif

      default :
	rv = -1;
	break;
    }


    if(rv == 1)
      exception_override_warning((*cl)->var);

    return(rv);
}


/*
 * Turn on selected *'s for default selections, if any, and
 * for ones we forced on.
 * Adjust the Sample line right above the color selection lines.
 */
void
color_update_selected(struct pine *ps, CONF_S *cl, char *fg, char *bg, int cleardef)
{
    int i, fg_is_custom = 1, bg_is_custom = 1;
#ifdef	_WINDOWS
    CONF_S *cl_custom = NULL;
#endif

    /* back up to header line */
    for(; cl && (cl->flags & CF_DOUBLEVAR); cl = prev_confline(cl))
      ;
    
    /* adjust sample line */
    if(cl && cl->var && cl->flags & CF_COLORSAMPLE){
	if(cl->value)
	  fs_give((void **)&cl->value);
	
	cl->value = color_setting_text_line(ps, cl->var);
    }

    for(i = 0, cl = next_confline(cl);
	i < pico_count_in_color_table() && cl;
	i++, cl = next_confline(cl)){
	if(fg && !strucmp(color_to_canonical_name(fg), colorx(i))){
	    cl->value[1] = R_SELD;
	    fg_is_custom = 0;
	}
	else
	  cl->value[1] = ' ';

	if(bg && !strucmp(color_to_canonical_name(bg), colorx(i))){
	    cl->value[cl->val2offset - cl->valoffset + 1] = R_SELD;
	    bg_is_custom = 0;
	}
	else
	  cl->value[cl->val2offset - cl->valoffset + 1] = ' ';
    }

#ifdef	_WINDOWS
    cl_custom = cl;
    cl = next_confline(cl);
#endif

    if(cl && cl->var && offer_normal_color_for_var(ps, cl->var)){
	if(fg && !struncmp(color_to_canonical_name(fg), MATCH_NORM_COLOR, RGBLEN)){
	    cl->value[1] = R_SELD;
	    fg_is_custom = 0;
	}
	else
	  cl->value[1] = ' ';

	if(bg && !struncmp(color_to_canonical_name(bg), MATCH_NORM_COLOR, RGBLEN)){
	    cl->value[cl->val2offset - cl->valoffset + 1] = R_SELD;
	    bg_is_custom = 0;
	}
	else
	  cl->value[cl->val2offset - cl->valoffset + 1] = ' ';

	cl = next_confline(cl);
    }

    if(cl && cl->var && offer_none_color_for_var(ps, cl->var)){
	if(fg && !struncmp(color_to_canonical_name(fg), MATCH_NONE_COLOR, RGBLEN)){
	    cl->value[1] = R_SELD;
	    fg_is_custom = 0;
	}
	else
	  cl->value[1] = ' ';

	if(bg && !struncmp(color_to_canonical_name(bg), MATCH_NONE_COLOR, RGBLEN)){
	    cl->value[cl->val2offset - cl->valoffset + 1] = R_SELD;
	    bg_is_custom = 0;
	}
	else
	  cl->value[cl->val2offset - cl->valoffset + 1] = ' ';

	cl = next_confline(cl);
    }

    /* Turn off Default X */
    if(cleardef)
      cl->value[1] = ' ';

#ifdef	_WINDOWS
    /* check for a custom setting */
    if(cl_custom){
	cl_custom->value[1] = fg_is_custom ? R_SELD : ' ';
	cl_custom->value[cl_custom->val2offset - cl_custom->valoffset + 1]
	    = bg_is_custom ? R_SELD : ' ';
    }
#endif
}


int
color_edit_screen(struct pine *ps, CONF_S **cl)
{
    OPT_SCREEN_S     screen, *saved_screen;
    CONF_S          *ctmp = NULL, *first_line = NULL, *ctmpb;
    int              rv, is_index = 0, is_hdrcolor = 0, indent = 12;
    int              is_general = 0, is_keywordcol = 0;
    char             tmp[1200+1], name[1200], *p;
    struct variable *vtmp, v;
    int              i, def;
    COLOR_PAIR      *color = NULL;
    SPEC_COLOR_S    *hc = NULL, *hcolors = NULL;
    KEYWORD_S       *kw;

    vtmp = (*cl)->var;
    if(vtmp == &ps->vars[V_VIEW_HDR_COLORS])
      is_hdrcolor++;
    else if(vtmp == &ps->vars[V_KW_COLORS])
      is_keywordcol++;
    else if(color_holding_var(ps, vtmp)){
	if(!struncmp(vtmp->name, "index-", 6))
	  is_index++;
	else
	  is_general++;
    }

    new_confline(&ctmp);
    /* Blank line */
    ctmp->flags |= CF_NOSELECT | CF_B_LINE;

    first_line = ctmp;

    new_confline(&ctmp)->var = vtmp;

    name[0] = '\0';
    if(is_general){
	p = srchstr(vtmp->name, "-foreground-color");
	snprintf(name, sizeof(name), "%.*s", p ? MIN(p - vtmp->name, 30) : 30, vtmp->name);
	name[sizeof(name)-1] = '\0';
	if(islower((unsigned char)name[0]))
	  name[0] = toupper((unsigned char)name[0]);
    }
    else if(is_index){
	p = srchstr(vtmp->name, "-foreground-color");
	snprintf(name, sizeof(name), "%.*s Symbol",
		p ? MIN(p - vtmp->name, 30) : 30, vtmp->name);
	name[sizeof(name)-1] = '\0';
	if(islower((unsigned char)name[0]))
	  name[0] = toupper((unsigned char)name[0]);
    }
    else if(is_hdrcolor){
	char **lval;
	
	lval = LVAL(vtmp, ew);
	hcolors = spec_colors_from_varlist(lval, 0);

	for(hc = hcolors, i = 0; hc; hc = hc->next, i++)
	  if(CFC_ICUST(*cl) == i)
	    break;
	
	if(hc){
	    snprintf(name, sizeof(name), "%s%s", HEADER_WORD, hc->spec);
	    name[sizeof(name)-1] = '\0';
	    i = sizeof(HEADER_WORD) - 1;
	    if(islower((unsigned char) name[i]))
	      name[i] = toupper((unsigned char) name[i]);
	}
    }
    else if(is_keywordcol){
	char **lval;

	for(kw=ps->keywords, i=0; kw; kw=kw->next, i++)
	  if(CFC_ICUST(*cl) == i)
	    break;

	if(kw){
	    char *nm, *comment = NULL;

	    nm = kw->nick ? kw->nick : kw->kw ? kw->kw : "";
	    if(utf8_width(nm) > 60)
	      nm = short_str(nm, tmp_20k_buf, SIZEOF_20KBUF, 60, EndDots);

	    if(kw->nick && kw->kw && kw->kw[0])
	      comment = kw->kw;

	    if(utf8_width(nm) + (comment ? utf8_width(comment) : 0) < 60)
	      utf8_snprintf(name, sizeof(name), "%.50w%s%.50w%s",
		      nm,
		      comment ? " (" : "",
		      comment ? comment : "",
		      comment ? ")" : "");
	    else
	      snprintf(name, sizeof(name), "%s", nm);

	    name[sizeof(name)-1] = '\0';

	    lval = LVAL(vtmp, ew);
	    hcolors = spec_colors_from_varlist(lval, 0);
	    if(kw && hcolors)
	      if(!(kw->nick && kw->nick[0]
		   && (color=hdr_color(kw->nick, NULL, hcolors))))
		if(kw->kw && kw->kw[0])
		  color = hdr_color(kw->kw, NULL, hcolors);
	}
    }

    snprintf(tmp, sizeof(tmp), "%s Color =", name[0] ? name : "?");
    tmp[sizeof(tmp)-1] = '\0';
    ctmp->varname		 = cpystr(tmp);
    ctmp->varnamep		 = ctmpb = ctmp;
    ctmp->flags			|= (CF_STARTITEM | CF_NOSELECT);
    ctmp->keymenu		 = &color_changing_keymenu;

    if(is_hdrcolor){
	char **apval;

	def = !(hc && hc->fg && hc->fg[0] && hc->bg && hc->bg[0]);
	
	add_color_setting_disp(ps, &ctmp, vtmp, ctmpb,
			       &custom_color_changing_keymenu,
			       &hdr_color_checkbox_keymenu,
			       config_help(vtmp - ps->vars, 0),
			       indent, CFC_ICUST(*cl),
			       def ? ps_global->VAR_NORM_FORE_COLOR
				   : hc->fg,
			       def ? ps_global->VAR_NORM_BACK_COLOR
				   : hc->bg,
			       def);

	/* optional string to match in header value */
	new_confline(&ctmp);
	ctmp->varnamep		 = ctmpb;
	ctmp->keymenu		 = &color_pattern_keymenu;
	ctmp->help		 = h_config_customhdr_pattern;
	ctmp->tool		 = color_text_tool;
	ctmp->varoffset		 = indent-5;
	ctmp->varname		 = cpystr(_("Pattern to match ="));
	ctmp->valoffset		 = indent-5 + strlen(ctmp->varname) + 1;
	ctmp->varmem		 = (*cl)->varmem;

	/*
	 * This is really ugly. This is just to get the value correct.
	 */
	memset(&v, 0, sizeof(v));
	v.is_used    = 1;
	v.is_user    = 1;
	apval = APVAL(&v, ew);
	if(hc && hc->val && apval)
	  *apval = pattern_to_string(hc->val);

	set_current_val(&v, FALSE, FALSE);
	ctmp->var = &v;
	ctmp->value = pretty_value(ps, ctmp);
	ctmp->var = vtmp;
	if(apval && *apval)
	  fs_give((void **)apval);

	if(v.current_val.p)
	  fs_give((void **)&v.current_val.p);

	if(hcolors)
	  free_spec_colors(&hcolors);
    }
    else if(is_keywordcol){

	def = !(color && color->fg && color->fg[0]
		&& color->bg && color->bg[0]);
	
	add_color_setting_disp(ps, &ctmp, vtmp, ctmpb,
			       &kw_color_changing_keymenu,
			       &kw_color_checkbox_keymenu,
			       config_help(vtmp - ps->vars, 0),
			       indent, CFC_ICUST(*cl),
			       def ? ps_global->VAR_NORM_FORE_COLOR
				   : color->fg,
			       def ? ps_global->VAR_NORM_BACK_COLOR
				   : color->bg,
			       def);

	if(hcolors)
	  free_spec_colors(&hcolors);
    }
    else{
	char       *pvalfg, *pvalbg;
	int         def;
	COLOR_PAIR *newc;

	pvalfg = PVAL(vtmp, ew);
	pvalbg = PVAL(vtmp+1, ew);
	def = !(pvalfg && pvalfg[0] && pvalbg && pvalbg[0]);
	if(def){
	    /* display default val, if there is one */
	    pvalfg = PVAL(vtmp, Main);
	    pvalbg = PVAL(vtmp+1, Main);
	    if(ew == Post && pvalfg && pvalfg[0] && pvalbg && pvalbg[0]){
		;
	    }
	    else if(vtmp && vtmp->global_val.p && vtmp->global_val.p[0] &&
	       (vtmp+1)->global_val.p && (vtmp+1)->global_val.p[0]){
		pvalfg = vtmp->global_val.p;
		pvalbg = (vtmp+1)->global_val.p;
	    }
	    else{
		if(var_defaults_to_rev(vtmp) && (newc = pico_get_rev_color())){
		    pvalfg = newc->fg;
		    pvalbg = newc->bg;
		}
		else{
		    pvalfg = NULL;
		    pvalbg = NULL;
		}
	    }
	}

	add_color_setting_disp(ps, &ctmp, vtmp, ctmpb,
			       &color_changing_keymenu,
			       &config_checkbox_keymenu,
			       config_help(vtmp - ps->vars, 0),
			       indent, 0, pvalfg, pvalbg, def);
    }

    first_line = first_sel_confline(first_line);

    saved_screen = opt_screen;
    memset(&screen, 0, sizeof(screen));
    screen.ro_warning = saved_screen ? saved_screen->deferred_ro_warning : 0;
    rv = conf_scroll_screen(ps, &screen, first_line,
			    ew == Post ? _("SETUP COLOR EXCEPTIONS")
				       : _("SETUP COLOR"),
			    _("configuration"), 1);

    opt_screen = saved_screen;
    ps->mangled_screen = 1;
    return(rv);
}
