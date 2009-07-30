#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: listsel.c 918 2008-01-23 19:39:38Z hubert@u.washington.edu $";
#endif
/* ========================================================================
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
#include "listsel.h"
#include "status.h"
#include "confscroll.h"
#include "../pith/state.h"


/*
 * Internal prototypes
 */
int      select_from_list_tool(struct pine *, int, CONF_S **, unsigned);
int      select_from_list_tool_allow_noselections(struct pine *, int, CONF_S **, unsigned);


/*
 * This is intended to be a generic tool to select strings from a list
 * of strings.
 *
 * Args     lsel -- the items as well as the answer are contained in this list
 *         flags -- There is some inconsistent flags usage. Notice that the
 *                  flag SFL_ALLOW_LISTMODE is a flag passed in the flags
 *                  argument whereas the flag SFL_NOSELECT is a per item
 *                  (that is, per LIST_SEL_S) flag.
 *         title -- passed to conf_scroll_screen
 *         pdesc -- passed to conf_scroll_screen
 *          help -- passed to conf_scroll_screen
 *     helptitle -- passed to conf_scroll_screen
 *
 * You have screen width - 4 columns to work with. If you want to overflow to
 * a second (or third or fourth) line for an item just send another item
 * in the list but with the SFL_NOSELECT flag set. Only the selectable lines
 * will be highlighted, which is kind of a crock, but it looked like a lot
 * of work to fix that.
 *
 * Returns 0 on successful choice
 *        -1 if cancelled
 */
int
select_from_list_screen(LIST_SEL_S *lsel, long unsigned int flags, char *title,
			char *pdesc, HelpType help, char *htitle,
			LIST_SEL_S *starting_val)
{
    CONF_S      *ctmp = NULL, *first_line = NULL;
    OPT_SCREEN_S screen;
    int          j, lv, ret = -1;
    LIST_SEL_S  *p;
    char        *display;
    size_t       l;
    ScreenMode   listmode = SingleMode;
    int         (*tool)(struct pine *, int, CONF_S **, unsigned);

    if(!lsel)
      return(ret);
    
    /* find longest value's length */
    for(lv = 0, p = lsel; p; p = p->next){
	if(!(p->flags & SFL_NOSELECT)){
	    display = p->display_item ? p->display_item :
			p->item ? p->item : "";
	    if(lv < (j = utf8_width(display)))
	      lv = j;
	}
    }

    lv = MIN(lv, ps_global->ttyo->screen_cols - 4);

    tool = (flags & SFL_CTRLC)	? select_from_list_tool_allow_noselections
				: select_from_list_tool;

    /*
     * Convert the passed in list to conf_scroll lines.
     */

    if(flags & SFL_ALLOW_LISTMODE){

	if(flags & SFL_ONLY_LISTMODE) {assert(flags & SFL_STARTIN_LISTMODE);}

	for(p = lsel; p; p = p->next){

	    display = p->display_item ? p->display_item :
			p->item ? p->item : "";
	    new_confline(&ctmp);
	    if(!first_line && !(p->flags & SFL_NOSELECT))
	      first_line = ctmp;
	    if(!first_line && !(p->flags & SFL_NOSELECT))
	      if(!starting_val || (starting_val == p))
	        first_line = ctmp;

	    /* generous allocation */
	    l = lv + 4 + strlen(display);
	    ctmp->value        = (char *) fs_get((l + 1) * sizeof(char));
	    utf8_snprintf(ctmp->value, l+1, "    %-*.*w", lv, lv, display);
	    ctmp->value[l] = '\0';

	    ctmp->d.l.lsel     = p;
	    ctmp->d.l.listmode = &listmode;
	    if(flags & SFL_ONLY_LISTMODE){
	      if(flags & SFL_CTRLC)
	        ctmp->keymenu      = &sel_from_list_olm_ctrlc;
	      else
	        ctmp->keymenu      = &sel_from_list_olm;
	    }
	    else{
	      if(flags & SFL_CTRLC)
	        ctmp->keymenu      = &sel_from_list_sm_ctrlc;
	      else
	        ctmp->keymenu      = &sel_from_list_sm;
	    }

	    ctmp->help         = help;
	    ctmp->help_title   = htitle;
	    ctmp->tool         = tool;
	    ctmp->flags        = CF_STARTITEM |
				 ((p->flags & SFL_NOSELECT) ? CF_NOSELECT : 0);
	}
    }
    else{

	assert(!(flags & SFL_ONLY_LISTMODE));
	assert(!(flags & SFL_STARTIN_LISTMODE));

	for(p = lsel; p; p = p->next){

	    display = p->display_item ? p->display_item :
			p->item ? p->item : "";
	    new_confline(&ctmp);
	    if(!first_line && !(p->flags & SFL_NOSELECT))
	      if(!starting_val || (starting_val == p))
	        first_line = ctmp;

	    l = lv + strlen(display);
	    ctmp->value        = (char *) fs_get((l + 1) * sizeof(char));
	    utf8_snprintf(ctmp->value, l+1, "%-*.*w", lv, lv, display);
	    ctmp->value[l] = '\0';

	    ctmp->d.l.lsel     = p;
	    ctmp->d.l.listmode = &listmode;
	    if(flags & SFL_CTRLC)
	      ctmp->keymenu      = &sel_from_list_ctrlc;
	    else
	      ctmp->keymenu      = &sel_from_list;

	    ctmp->help         = help;
	    ctmp->help_title   = htitle;
	    ctmp->tool         = tool;
	    ctmp->flags        = CF_STARTITEM |
				 ((p->flags & SFL_NOSELECT) ? CF_NOSELECT : 0);
	    ctmp->valoffset    = 4;
	}
    }

    /* just convert to start in listmode after the fact, easier that way */
    if(flags & SFL_STARTIN_LISTMODE){
	listmode = ListMode;

	for(ctmp = first_line; ctmp; ctmp = next_confline(ctmp))
	  if(!(ctmp->flags & CF_NOSELECT) && ctmp->value){
	      ctmp->value[0] = '[';
	      ctmp->value[1] = ctmp->d.l.lsel->selected ? 'X' : SPACE;
	      ctmp->value[2] = ']';
	      if(flags & SFL_ONLY_LISTMODE){
	        if(flags & SFL_CTRLC)
	          ctmp->keymenu      = &sel_from_list_olm_ctrlc;
	        else
	          ctmp->keymenu      = &sel_from_list_olm;
	      }
	      else{
	        if(flags & SFL_CTRLC)
	          ctmp->keymenu      = &sel_from_list_lm_ctrlc;
	        else
	          ctmp->keymenu      = &sel_from_list_lm;
	      }
	  }
    }

    memset(&screen, 0, sizeof(screen));
    switch(conf_scroll_screen(ps_global, &screen, first_line, title, pdesc, 0)){
      case 1:
        ret = 0;
	break;
      
      default:
	break;
    }

    ps_global->mangled_screen = 1;
    return(ret);
}


int
select_from_list_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    CONF_S *ctmp;
    int     retval = 0;

    switch(cmd){
      case MC_SELECT :
	if(*(*cl)->d.l.listmode == SingleMode){
	    (*cl)->d.l.lsel->selected = 1;
	    retval = 3;
	}
	else{
	    /* check if anything is selected */
	    /* go to first line */
	    for(ctmp = *cl; prev_confline(ctmp); ctmp = prev_confline(ctmp))
	      ;
	    
	    for(; ctmp; ctmp = next_confline(ctmp))
	      if(!(ctmp->flags & CF_NOSELECT) && ctmp->d.l.lsel->selected){
		  retval = 3;
		  break;
	      }
	    
	    if(retval == 0){
		q_status_message(SM_ORDER, 0, 3,
		     _("Nothing selected, use Exit to exit without a selection."));
	    }
	}

	break;

      case MC_LISTMODE :
        if(*(*cl)->d.l.listmode == SingleMode){
	    /*
	     * UnHide the checkboxes
	     */

	    *(*cl)->d.l.listmode = ListMode;

	    /* go to first line */
	    for(ctmp = *cl; prev_confline(ctmp); ctmp = prev_confline(ctmp))
	      ;
	    
	    for(; ctmp; ctmp = next_confline(ctmp))
	      if(!(ctmp->flags & CF_NOSELECT) && ctmp->value){
		  ctmp->value[0] = '[';
		  ctmp->value[1] = ctmp->d.l.lsel->selected ? 'X' : SPACE;
		  ctmp->value[2] = ']';
		  ctmp->keymenu  = &sel_from_list_lm;
	      }
	}
	else{
	    /*
	     * Hide the checkboxes
	     */

	    *(*cl)->d.l.listmode = SingleMode;

	    /* go to first line */
	    for(ctmp = *cl; prev_confline(ctmp); ctmp = prev_confline(ctmp))
	      ;
	    
	    for(; ctmp; ctmp = next_confline(ctmp))
	      if(!(ctmp->flags & CF_NOSELECT) && ctmp->value){
		  ctmp->value[0] = ctmp->value[1] = ctmp->value[2] = SPACE;
		  ctmp->keymenu  = &sel_from_list_sm;
	      }
	}

	ps->mangled_body = ps->mangled_footer = 1;
	break;

      case MC_TOGGLE :
	if((*cl)->value[1] == 'X'){
	    (*cl)->d.l.lsel->selected = 0;
	    (*cl)->value[1] = SPACE;
	}
	else{
	    (*cl)->d.l.lsel->selected = 1;
	    (*cl)->value[1] = 'X';
	}

	ps->mangled_body = 1;
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
select_from_list_tool_allow_noselections(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int retval = 0;

    switch(cmd){
      case MC_SELECT :
	retval = 3;
	if(*(*cl)->d.l.listmode == SingleMode)
	  (*cl)->d.l.lsel->selected = 1;

	break;

      default:
        retval = select_from_list_tool(ps, cmd, cl, flags);
	break;
    }

    if(retval > 0)
      ps->mangled_body = 1;

    return(retval);
}
