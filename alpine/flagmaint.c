#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: flagmaint.c 807 2007-11-09 01:21:33Z hubert@u.washington.edu $";
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
#include "flagmaint.h"
#include "confscroll.h"
#include "../pith/state.h"
#include "../pith/flag.h"
#include "../pith/status.h"
#include "../pith/mailcmd.h"
#include "../pith/icache.h"


#define FLAG_ADD_RETURN 15


/*
 * Internal prototypes
 */
int	 flag_checkbox_tool(struct pine *, int, CONF_S **, unsigned);


/*----------------------------------------------------------------------
   Function to control flag set/clearing

   Basically, turn the flags into a fake list of features...

   Returns 0 unless user has added a keyword, then 1.

 ----*/
int
flag_maintenance_screen(struct pine *ps, struct flag_screen *flags)
{
    int		  i, lv, lc, maxwidth, offset, need, rv = 0;
    char	  tmp[1200], **p, *spacer;
    CONF_S	 *ctmpa, *first_line;
    struct	  flag_table  *fp;
    OPT_SCREEN_S  screen;

try_again:
    maxwidth = MAX(MIN((ps->ttyo ? ps->ttyo->screen_cols : 80), 150), 30);
    first_line = NULL;
    ctmpa = NULL;

    for(p = flags->explanation; p && *p; p++){
	new_confline(&ctmpa);
	ctmpa->keymenu   = &flag_keymenu;
	ctmpa->help      = NO_HELP;
	ctmpa->tool      = flag_checkbox_tool;
	ctmpa->flags    |= CF_NOSELECT;
	ctmpa->valoffset = 0;
	ctmpa->value     = cpystr(_(*p));
    }

    /* Now wire flags checkboxes together */
    for(lv = 0, lc = 0, fp = (flags->flag_table ? *flags->flag_table : NULL);
	fp && fp->name; fp++){	/* longest name */
	if(fp->flag != F_COMMENT){
	    if(lv < (i = utf8_width(_(fp->name))))
	      lv = i;
	    if(fp->comment && lc < (i = utf8_width(fp->comment)))
	      lc = i;
	}
    }
    
    lv = MIN(lv,100);
    lc = MIN(lc,100);
    if(lc > 0)
      spacer = "    ";
    else
      spacer = "";

    offset = 6;
    if((need = offset + 5 + lv + strlen(spacer) + lc) > maxwidth){
	offset -= (need - maxwidth);
	offset = MAX(0,offset);
	if((need = offset + 5 + lv + strlen(spacer) + lc) > maxwidth){
	    spacer = " ";
	    if((need = offset + 5 + lv + strlen(spacer) + lc) > maxwidth){
		lc -= (need - maxwidth);
		lc = MAX(0,lc);
		if(lc == 0)
		  spacer = "";
	    }
	}
    }

    new_confline(&ctmpa);
    ctmpa->keymenu   = &flag_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = flag_checkbox_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->valoffset = 0;
    ctmpa->value     = cpystr("");

    new_confline(&ctmpa);
    ctmpa->keymenu   = &flag_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = flag_checkbox_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->valoffset = 0;
    utf8_snprintf(tmp, sizeof(tmp), "%*.*w  %s", offset+3, offset+3, _("Set"), _("Flag/Keyword Name"));
    tmp[sizeof(tmp)-1] = '\0';
    ctmpa->value = cpystr(tmp);

    new_confline(&ctmpa);
    ctmpa->keymenu   = &flag_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = flag_checkbox_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->valoffset = 0;
    snprintf(tmp, sizeof(tmp), "%*.*s---  %.*s",
	    offset, offset, "",
	    lv+lc+strlen(spacer), repeat_char(lv+lc+strlen(spacer), '-'));
    tmp[sizeof(tmp)-1] = '\0';
    ctmpa->value = cpystr(tmp);

    for(fp = (flags->flag_table ? *flags->flag_table : NULL);
	fp && fp->name; fp++){	/* build the list */
	new_confline(&ctmpa);
	if(!first_line && (fp->flag != F_COMMENT))
	  first_line = ctmpa;

	ctmpa->keymenu   = &flag_keymenu;
	ctmpa->tool      = flag_checkbox_tool;
	ctmpa->valoffset = offset;

	if(fp->flag == F_COMMENT){
	    ctmpa->help   = NO_HELP;
	    ctmpa->flags |= CF_NOSELECT;
	    ctmpa->value  = cpystr(fp->name);
	}
	else{
	    ctmpa->help		  = fp->help;
	    ctmpa->d.f.ftbl	  = flags->flag_table;
	    ctmpa->d.f.fp	  = fp;

	    utf8_snprintf(tmp, sizeof(tmp), "[%c]  %-*.*w%s%-*.*w",
		    (fp->set == 0) ? ' ' : (fp->set == 1) ? 'X' : '?',
		    lv, lv, _(fp->name),
		    spacer, lc, lc, fp->comment ? fp->comment : "");
	    ctmpa->value = cpystr(tmp);
	}
    }
      
    memset(&screen, 0, sizeof(screen));
    /*
     * TRANSLATORS: FLAG MAINTENANCE is a screen title.
     * Print something1 using something2.  configuration is something1
     */
    if(conf_scroll_screen(ps, &screen, first_line,
			  _("FLAG MAINTENANCE"),
			  _("configuration"), 0) == FLAG_ADD_RETURN){
	int flags, r;
	char keyword[500];
	char nickname[500];
	char prompt[500];
	char *error = NULL;
	KEYWORD_S *kw;
	HelpType help;

	/*
	 * User is asking to add a new keyword. We will add it to the
	 * mailbox if necessary and add it to the keywords list from
	 * Setup/Config. Then we will modify the flag_table and present
	 * the flag modification screen again.
	 */

	ps->mangled_screen = 1;
	keyword[0] = '\0';
	flags = OE_APPEND_CURRENT;
	help = NO_HELP;

	do{
	    if(error){
		q_status_message(SM_ORDER, 3, 4, error);
		fs_give((void **) &error);
	    }

	    strncpy(prompt, _("Keyword to be added : "), sizeof(prompt)-1);
	    prompt[sizeof(prompt)-1] = '\0';
	    r = optionally_enter(keyword, -FOOTER_ROWS(ps_global), 0,
				 sizeof(keyword), prompt, NULL, help, &flags);

	    if(r == 3)
	      help = help == NO_HELP ? h_type_keyword : NO_HELP;
	    else if(r == 1){
		cmd_cancelled("Add Keyword");
		goto try_again;
	    }

	    removing_leading_and_trailing_white_space(keyword);

	}while(r == 3 || keyword_check(keyword, &error));

	for(kw = ps->keywords; kw; kw = kw->next){
	    if(kw->kw && !strucmp(kw->kw, keyword)){
		q_status_message(SM_ORDER, 3, 4, _("Keyword already configured, changing nickname"));
		break;
	    }
	}

	snprintf(prompt, sizeof(prompt), _("Optional nickname for \"%s\" : "), keyword);

	nickname[0] = '\0';
	help = NO_HELP;

	do{
	    r = optionally_enter(nickname, -FOOTER_ROWS(ps_global), 0,
				 sizeof(nickname), prompt, NULL, help, &flags);

	    if(r == 3)
	      help = help == NO_HELP ? h_type_keyword_nickname : NO_HELP;
	    else if(r == 1){
		cmd_cancelled("Add Keyword");
		goto try_again;
	    }

	    removing_leading_and_trailing_white_space(nickname);

	}while(r == 3);

	if(keyword[0]){
	    char ***alval;
	    int offset = -1;
	    struct variable *var;

	    var = &ps_global->vars[V_KEYWORDS];
	    alval = ALVAL(var, Main);

	    for(kw = ps->keywords; kw; kw = kw->next){
		offset++;
		if(kw->kw && !strucmp(kw->kw, keyword)){
		    /* looks like it should already exist at offset */
		    break;
		}
	    }

	    if(!kw)
	      offset = -1;

	    if(offset >= 0 && (*alval) && (*alval)[offset]){
		fs_give((void **) &(*alval)[offset]);
		(*alval)[offset] = put_pair(nickname, keyword);
	    }
	    else if(!*alval){
		offset = 0;
		*alval = (char **) fs_get(2*sizeof(char *));
		(*alval)[offset] = put_pair(nickname, keyword);
		(*alval)[offset+1] = NULL;
	    }
	    else{
		for(offset=0; (*alval)[offset]; offset++);
		  ;

		fs_resize((void **) alval, (offset + 2) * sizeof(char *));
		(*alval)[offset] = put_pair(nickname, keyword);
		(*alval)[offset+1] = NULL;
	    }

	    set_current_val(var, TRUE, FALSE);
	    if(ps_global->prc)
	      ps_global->prc->outstanding_pinerc_changes = 1;

	    if(ps_global->keywords)
	      free_keyword_list(&ps_global->keywords);

	    if(var->current_val.l && var->current_val.l[0])
	      ps_global->keywords = init_keyword_list(var->current_val.l);

	    clear_index_cache(ps_global->mail_stream, 0);

	    rv = 1;
	}
    }

    ps->mangled_screen = 1;

    return(rv);
}


/*
 * Message flag manipulation tool
 * 
 * 
 * returns:  -1 on unrecognized cmd, 0 if no change, 1 if change
 */
int
flag_checkbox_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int rv = 0, state;

    switch(cmd){
      case MC_TOGGLE:				/* mark/unmark feature */
	state = (*cl)->d.f.fp->set;
	state = (state == 1) ? 0 : (!state && ((*cl)->d.f.fp->ukn)) ? 2 : 1;
	(*cl)->value[1] = (state == 0) ? ' ' : ((state == 1) ? 'X': '?');
	(*cl)->d.f.fp->set = state;
	rv = 1;
	break;

      case MC_ADD:
	rv = FLAG_ADD_RETURN;
	break;

      case MC_EXIT:				/* exit */
	rv = simple_exit_cmd(flags);
	break;

      default :
	rv = -1;
	break;
    }

    return(rv);
}
