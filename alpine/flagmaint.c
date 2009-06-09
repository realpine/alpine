#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: flagmaint.c 203 2006-10-26 17:23:46Z hubert@u.washington.edu $";
#endif

/*
 * ========================================================================
 * Copyright 2006 University of Washington
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


/*
 * Internal prototypes
 */
int	 flag_checkbox_tool(struct pine *, int, CONF_S **, unsigned);


/*----------------------------------------------------------------------
   Function to control flag set/clearing

   Basically, turn the flags into a fake list of features...

 ----*/
void
flag_maintenance_screen(struct pine *ps, struct flag_screen *flags)
{
    int		  i, lv, lc, maxwidth, offset, need;
    char	  tmp[1200], **p, *spacer;
    CONF_S	 *ctmpa = NULL, *first_line = NULL;
    struct	  flag_table  *fp;
    OPT_SCREEN_S  screen;

    maxwidth = MAX(MIN((ps->ttyo ? ps->ttyo->screen_cols : 80), 150), 30);

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
	if(lv < (i = utf8_width(_(fp->name))))
	  lv = i;
	if(fp->comment && lc < (i = utf8_width(fp->comment)))
	  lc = i;
    }
    
    lv = MIN(lv,100);
    lc = MIN(lc,100);
    if(lc > 0)
      spacer = "    ";
    else
      spacer = "";

    offset = 12;
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
    utf8_snprintf(tmp, sizeof(tmp), "%*.*w  %s", offset+3, offset+3, _("Set"), _("Flag Name"));
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
	if(!first_line)
	  first_line = ctmpa;

	ctmpa->keymenu		  = &flag_keymenu;
	ctmpa->help		  = fp->help;
	ctmpa->tool		  = flag_checkbox_tool;
	ctmpa->d.f.ftbl		  = flags->flag_table;
	ctmpa->d.f.fp		  = fp;
	ctmpa->valoffset	  = offset;

	utf8_snprintf(tmp, sizeof(tmp), "[%c]  %-*.*w%s%-*.*w",
		(fp->set == 0) ? ' ' : (fp->set == 1) ? 'X' : '?',
		lv, lv, _(fp->name),
		spacer, lc, lc, fp->comment ? fp->comment : "");
	ctmpa->value = cpystr(tmp);
    }
      
    memset(&screen, 0, sizeof(screen));
    /*
     * TRANSLATORS: FLAG MAINTENANCE is a screen title.
     * Print something1 using something2.  configuration is something1
     */
    (void) conf_scroll_screen(ps, &screen, first_line,
			      _("FLAG MAINTENANCE"), _("configuration"), 0);
    ps->mangled_screen = 1;
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

      case MC_EXIT:				/* exit */
	rv = simple_exit_cmd(flags);
	break;

      default :
	rv = -1;
	break;
    }

    return(rv);
}
