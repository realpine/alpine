#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: print.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
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
#include "print.h"
#include "confscroll.h"
#include "keymenu.h"
#include "radio.h"
#include "status.h"
#include "../pith/state.h"
#include "../pith/mailcmd.h"


/*
 * Internal prototypes
 */
void	 set_def_printer_value(char *);
int      print_select_tool(struct pine *, int, CONF_S **, unsigned);
int      print_edit_tool(struct pine *, int, CONF_S **, unsigned);


static char **def_printer_line;
static char no_ff[] = "-no-formfeed";


#ifndef	DOS

/*
 * Information used to paint and maintain a line on the configuration screen
 */
/*----------------------------------------------------------------------
    The printer selection screen

   Draws the screen and prompts for the printer number and the custom
   command if so selected.

 ----*/
void
select_printer(struct pine *ps, int edit_exceptions)
{
    struct        variable  *vtmp;
    CONF_S       *ctmpa = NULL, *ctmpb = NULL, *heading = NULL,
		 *start_line = NULL;
    PINERC_S     *prc = NULL;
    int i, saved_printer_cat, readonly_warning = 0, no_ex;
    SAVED_CONFIG_S *vsave;
    char *saved_printer, **lval;
    OPT_SCREEN_S screen;
    size_t l;

    if(edit_exceptions){
	q_status_message(SM_ORDER, 3, 7,
			 _("Exception Setup not implemented for printer"));
	return;
    }

    if(fixed_var(&ps_global->vars[V_PRINTER], "change", "printer"))
      return;

    ew = edit_exceptions ? ps_global->ew_for_except_vars : Main;
    
    no_ex = (ps_global->ew_for_except_vars == Main);

    if(ps->restricted)
      readonly_warning = 1;
    else{
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

    saved_printer = cpystr(ps->VAR_PRINTER);
    saved_printer_cat = ps->printer_category;

    new_confline(&ctmpa);
    ctmpa->valoffset = 2;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
#ifdef OS2
      = cpystr("\"Select\" a port or |pipe-command as your default printer.");
#else
      = cpystr(_("You may \"Select\" a print command as your default printer."));
#endif

    new_confline(&ctmpa);
    ctmpa->valoffset = 2;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
#ifdef OS2
      = cpystr("You may also add alternative ports or !pipes to the list in the");
#else
      = cpystr(_("You may also add custom print commands to the list in the"));
#endif

    new_confline(&ctmpa);
    ctmpa->valoffset = 2;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
#ifdef OS2
      = cpystr("\"Personally selected port or |pipe\" section below.");
#else
      = cpystr(_("\"Personally selected print command\" section below."));
#endif

    new_confline(&ctmpa);
    ctmpa->valoffset = 2;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT | CF_B_LINE;

    new_confline(&ctmpa);
    ctmpa->valoffset = 4;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT;
    def_printer_line = &ctmpa->value;
    set_def_printer_value(ps->VAR_PRINTER);

    new_confline(&ctmpa);
    ctmpa->valoffset = 2;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT | CF_B_LINE;

#ifndef OS2
    new_confline(&ctmpa);
    heading = ctmpa;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->varname
	= cpystr(_(" Printer attached to IBM PC or compatible, Macintosh"));
    ctmpa->flags    |= (CF_NOSELECT | CF_STARTITEM);
    ctmpa->value     = cpystr("");
    ctmpa->headingp  = heading;

    new_confline(&ctmpa);
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
      = cpystr(_("This may not work with all attached printers, and will depend on the"));
    ctmpa->headingp  = heading;

    new_confline(&ctmpa);
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
      = cpystr(_("terminal emulation/communications software in use. It is known to work"));
    ctmpa->headingp  = heading;

    new_confline(&ctmpa);
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
      = cpystr("with Kermit and the latest UW version of NCSA telnet on Macs and PCs,");
    ctmpa->headingp  = heading;

    new_confline(&ctmpa);
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
      = cpystr("Versaterm Pro on Macs, and WRQ Reflections on PCs.");
    ctmpa->headingp  = heading;

    new_confline(&ctmpa);
    start_line = ctmpb = ctmpa; /* default start line */
    ctmpa->valoffset = 17;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = h_config_set_att_ansi;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOHILITE;
    ctmpa->varoffset = 8;
    ctmpa->varname   = cpystr("Printer:");
    ctmpa->value     = cpystr(ANSI_PRINTER);
    ctmpa->varnamep  = ctmpb;
    ctmpa->headingp  = heading;

    new_confline(&ctmpa);
    ctmpa->valoffset = 17;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = h_config_set_att_ansi2;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOHILITE;
    ctmpa->varoffset = 8;
    l = strlen(ANSI_PRINTER)+strlen(no_ff);
    ctmpa->value     = (char *)fs_get((l+1) * sizeof(char));
    ctmpa->varnamep  = ctmpb;
    ctmpa->headingp  = heading;
    snprintf(ctmpa->value, l+1, "%s%s", ANSI_PRINTER, no_ff);
    ctmpa->value[l] = '\0';

    new_confline(&ctmpa);
    ctmpa->valoffset = 17;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = h_config_set_att_wyse;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOHILITE;
    ctmpa->varoffset = 8;
    ctmpa->value     = cpystr(WYSE_PRINTER);
    ctmpa->varnamep  = ctmpb;
    ctmpa->headingp  = heading;

    new_confline(&ctmpa);
    ctmpa->valoffset = 17;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = h_config_set_att_wyse2;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOHILITE;
    ctmpa->varoffset = 8;
    l = strlen(WYSE_PRINTER)+strlen(no_ff);
    ctmpa->value     = (char *)fs_get((l+1) * sizeof(char));
    ctmpa->varnamep  = ctmpb;
    ctmpa->headingp  = heading;
    snprintf(ctmpa->value, l+1, "%s%s", WYSE_PRINTER, no_ff);
    ctmpa->value[l] = '\0';
#endif

    new_confline(&ctmpa);
    ctmpa->valoffset = 0;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT | CF_B_LINE;
    ctmpa->var = &ps->vars[V_STANDARD_PRINTER];


    new_confline(&ctmpa);
    heading = ctmpa;
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->varname
#ifdef OS2
        = cpystr(" Standard OS/2 printer port");
#else
	= cpystr(_(" Standard UNIX print command"));
#endif
    ctmpa->value = cpystr("");
    ctmpa->flags    |= (CF_NOSELECT | CF_STARTITEM);
    ctmpa->headingp  = heading;
    ctmpa->var = &ps->vars[V_STANDARD_PRINTER];

    new_confline(&ctmpa);
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
#ifdef OS2
      = cpystr("Using this option may require you to use the OS/2 \"MODE\" command to");
#else
      = cpystr(_("Using this option may require setting your \"PRINTER\" or \"LPDEST\""));
#endif
    ctmpa->headingp  = heading;
    ctmpa->var = &ps->vars[V_STANDARD_PRINTER];

    new_confline(&ctmpa);
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
#ifdef OS2
      = cpystr("direct printer output to the correct port.");
#else
      = cpystr(_("environment variable using the standard UNIX utilities."));
#endif
    ctmpa->headingp  = heading;
    ctmpa->var = &ps->vars[V_STANDARD_PRINTER];

    vtmp = &ps->vars[V_STANDARD_PRINTER];
    for(i = 0; vtmp->current_val.l[i]; i++){
	new_confline(&ctmpa);
	ctmpa->valoffset = 22;
	ctmpa->keymenu   = &printer_select_keymenu;
	ctmpa->help      = NO_HELP;
	ctmpa->help      = h_config_set_stand_print;
	ctmpa->tool      = print_select_tool;
	if(i == 0){
	    ctmpa->varoffset = 8;
	    ctmpa->varname   = cpystr(_("Printer List:"));
	    ctmpa->flags    |= CF_NOHILITE|CF_PRINTER;
#ifdef OS2
	    start_line = ctmpb = ctmpa; /* default start line */
#else
	    ctmpb = ctmpa;
#endif
	}

	ctmpa->varnamep  = ctmpb;
	ctmpa->headingp  = heading;
	ctmpa->varmem = i;
	ctmpa->var = vtmp;
	ctmpa->value = printer_name(vtmp->current_val.l[i]);
    }

    new_confline(&ctmpa);
    ctmpa->valoffset = 0;
    ctmpa->keymenu   = &printer_select_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_select_tool;
    ctmpa->flags    |= CF_NOSELECT | CF_B_LINE;

    new_confline(&ctmpa);
    heading = ctmpa;
    ctmpa->valoffset = 0;
    ctmpa->keymenu   = &printer_edit_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_edit_tool;
    ctmpa->varname
#ifdef OS2
        = cpystr(" Personally selected port or |command");
#else
	= cpystr(_(" Personally selected print command"));
#endif
    ctmpa->flags    |= (CF_NOSELECT | CF_STARTITEM);
    ctmpa->value = cpystr("");
    ctmpa->headingp  = heading;
    ctmpa->var = &ps->vars[V_PERSONAL_PRINT_COMMAND];


    new_confline(&ctmpa);
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_edit_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_edit_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
#ifdef OS2
      = cpystr("The text to be printed will be sent to the printer or command given here.");
#else
      = cpystr(_("The text to be printed will be piped into the command given here. The"));
#endif
    ctmpa->headingp  = heading;
    ctmpa->var = &ps->vars[V_PERSONAL_PRINT_COMMAND];

    new_confline(&ctmpa);
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_edit_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_edit_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
#ifdef OS2
      = cpystr("The printer port or |pipe is in the 2nd column, the printer name is in");
#else
      = cpystr(_("command is in the 2nd column, the printer name is in the first column. Some"));
#endif
    ctmpa->headingp  = heading;
    ctmpa->var = &ps->vars[V_PERSONAL_PRINT_COMMAND];

    new_confline(&ctmpa);
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_edit_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_edit_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
#ifdef OS2
      = cpystr("the first column. Examples: \"LPT1\", \"COM2\", \"|enscript\". A command may");
#else
      = cpystr(_("examples are: \"prt\", \"lpr\", \"lp\", or \"enscript\". The command may be given"));
#endif
    ctmpa->headingp  = heading;
    ctmpa->var = &ps->vars[V_PERSONAL_PRINT_COMMAND];

    new_confline(&ctmpa);
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_edit_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_edit_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
#ifdef OS2
      = cpystr("be given options, for example \"|ascii2ps -p LPT1\" or \"|txt2hplc -2\". Use");
#else
      = cpystr(_("with options, for example \"enscript -2 -r\" or \"lpr -Plpacc170\". The"));
#endif
    ctmpa->headingp  = heading;
    ctmpa->var = &ps->vars[V_PERSONAL_PRINT_COMMAND];

    new_confline(&ctmpa);
    ctmpa->valoffset = 6;
    ctmpa->keymenu   = &printer_edit_keymenu;
    ctmpa->help      = NO_HELP;
    ctmpa->tool      = print_edit_tool;
    ctmpa->flags    |= CF_NOSELECT;
    ctmpa->value
#ifdef OS2
      = cpystr("the |command method for printers that require conversion from ASCII.");
#else
      = cpystr(_("commands and options on your system may be different from these examples."));
#endif
    ctmpa->headingp  = heading;
    ctmpa->var = &ps->vars[V_PERSONAL_PRINT_COMMAND];

    vtmp = &ps->vars[V_PERSONAL_PRINT_COMMAND];
    lval = no_ex ? vtmp->current_val.l : LVAL(vtmp, ew);
    if(lval){
	for(i = 0; lval[i]; i++){
	    new_confline(&ctmpa);
	    ctmpa->valoffset = 22;
	    ctmpa->keymenu   = &printer_edit_keymenu;
	    ctmpa->help      = h_config_set_custom_print;
	    ctmpa->tool      = print_edit_tool;
	    if(i == 0){
		ctmpa->varoffset = 8;
		ctmpa->varname   = cpystr(_("Printer List:"));
		ctmpa->flags    |= CF_NOHILITE|CF_PRINTER;
		ctmpb = ctmpa;
	    }

	    ctmpa->varnamep  = ctmpb;
	    ctmpa->headingp  = heading;
	    ctmpa->varmem = i;
	    ctmpa->var = vtmp;
	    ctmpa->value = printer_name(lval[i]);
	}
    }
    else{
	new_confline(&ctmpa);
	ctmpa->valoffset = 22;
	ctmpa->keymenu   = &printer_edit_keymenu;
	ctmpa->help      = h_config_set_custom_print;
	ctmpa->tool      = print_edit_tool;
	ctmpa->flags    |= CF_NOHILITE;
	ctmpa->varoffset = 8;
	ctmpa->varname   = cpystr(_("Printer List:"));
	ctmpa->varnamep  = ctmpa;
	ctmpa->headingp  = heading;
	ctmpa->varmem    = 0;
	ctmpa->var       = vtmp;
	ctmpa->value     = cpystr("");
    }

    memset(&screen, 0, sizeof(screen));
    screen.ro_warning = readonly_warning;
    vsave = save_config_vars(ps, 0);
    /* TRANSLATORS: SETUP... is a screen title
       Print something1 using something2.
       "printer config" is something1 */
    switch(conf_scroll_screen(ps, &screen, start_line,
			      edit_exceptions ? _("SETUP PRINTER EXCEPTIONS")
					      : _("SETUP PRINTER"),
			      _("printer config"), 0)){
      case 0:
	break;
    
      case 1:
	write_pinerc(ps, ew, WRP_NONE);
	break;
    
      case 10:
	revert_to_saved_config(ps, vsave, 0);
	ps->printer_category = saved_printer_cat;
	set_variable(V_PRINTER, saved_printer, 1, 0, ew);
	set_variable(V_PERSONAL_PRINT_CATEGORY, comatose(ps->printer_category),
		     1, 0, ew);
	if(prc)
	  prc->outstanding_pinerc_changes = 0;

	break;
    }

    def_printer_line = NULL;
    free_saved_config(ps, &vsave, 0);
    fs_give((void **)&saved_printer);
}

#endif	/* !DOS */


void
set_def_printer_value(char *printer)
{
    char *p, *nick, *cmd;
    int set, editing_norm_except_exists;
    size_t l;

    if(!def_printer_line)
      return;
    
    editing_norm_except_exists = ((ps_global->ew_for_except_vars != Main) &&
				  (ew == Main));

    parse_printer(printer, &nick, &cmd, NULL, NULL, NULL, NULL);
    p = *nick ? nick : cmd;
    set = *p;
    if(*def_printer_line)
      fs_give((void **)def_printer_line);

    l = strlen(p) + 60;
    *def_printer_line = fs_get((l+1) * sizeof(char));
    snprintf(*def_printer_line, l+1, "Default printer %s%s%s%s%s",
	set ? "set to \"" : "unset", set ? p : "", set ? "\"" : "",
	(set && editing_norm_except_exists) ? " (in exception config)" : "",
	set ? "." : ""); 
    (*def_printer_line)[l] = '\0';

    fs_give((void **)&nick);
    fs_give((void **)&cmd);
}


int
print_select_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int rc, retval, no_ex, printer_msg = 0;
    char *p, **lval, *printer_was;
    struct variable *vtmp;

    no_ex = (ps_global->ew_for_except_vars == Main);

    printer_was = ps->VAR_PRINTER ? cpystr(ps->VAR_PRINTER) : NULL;

    switch(cmd){
      case MC_EXIT:
        retval = config_exit_cmd(flags);
	break;

      case MC_CHOICE :
	if(cl && *cl){
	    char aname[100], wname[100];

	    strncpy(aname, ANSI_PRINTER, sizeof(aname)-1);
	    aname[sizeof(aname)-1] = '\0';
	    strncat(aname, no_ff, sizeof(aname)-strlen(aname)-1);
	    strncpy(wname, WYSE_PRINTER, sizeof(wname)-1);
	    wname[sizeof(wname)-1] = '\0';
	    strncat(wname, no_ff, sizeof(wname)-strlen(wname)-1);
	    if((*cl)->var){
		vtmp = (*cl)->var;
		lval = (no_ex || !vtmp->is_user) ? vtmp->current_val.l
						 : LVAL(vtmp, ew);
		rc = set_variable(V_PRINTER, lval ? lval[(*cl)->varmem] : NULL,
				  1, 0, ew);
		if(rc == 0){
		    if(vtmp == &ps->vars[V_STANDARD_PRINTER])
		      ps->printer_category = 2;
		    else if(vtmp == &ps->vars[V_PERSONAL_PRINT_COMMAND])
		      ps->printer_category = 3;

		    set_variable(V_PERSONAL_PRINT_CATEGORY, 
				 comatose(ps->printer_category), 1, 0, ew);

		    printer_msg++;
		}
		else
		  q_status_message(SM_ORDER,3,5, _("Trouble setting default printer"));

		retval = 1;
	    }
	    else if(!strcmp((*cl)->value,ANSI_PRINTER)){
		rc = set_variable(V_PRINTER, ANSI_PRINTER, 1, 0, ew);
		if(rc == 0){
		    ps->printer_category = 1;
		    set_variable(V_PERSONAL_PRINT_CATEGORY, 
				 comatose(ps->printer_category), 1, 0, ew);
		    printer_msg++;
		}
		else
		  q_status_message(SM_ORDER,3,5, _("Trouble setting default printer"));

		retval = 1;
	    }
	    else if(!strcmp((*cl)->value,aname)){
		rc = set_variable(V_PRINTER, aname, 1, 0, ew);
		if(rc == 0){
		    ps->printer_category = 1;
		    set_variable(V_PERSONAL_PRINT_CATEGORY, 
				 comatose(ps->printer_category), 1, 0, ew);
		    printer_msg++;
		}
		else
		  q_status_message(SM_ORDER,3,5, _("Trouble setting default printer"));

		retval = 1;
	    }
	    else if(!strcmp((*cl)->value,WYSE_PRINTER)){
		rc = set_variable(V_PRINTER, WYSE_PRINTER, 1, 0, ew);
		if(rc == 0){
		    ps->printer_category = 1;
		    set_variable(V_PERSONAL_PRINT_CATEGORY, 
				 comatose(ps->printer_category), 1, 0, ew);
		    printer_msg++;
		}
		else
		  q_status_message(SM_ORDER,3,5, _("Trouble setting default printer"));

		retval = 1;
	    }
	    else if(!strcmp((*cl)->value,wname)){
		rc = set_variable(V_PRINTER, wname, 1, 0, ew);
		if(rc == 0){
		    ps->printer_category = 1;
		    set_variable(V_PERSONAL_PRINT_CATEGORY, 
				 comatose(ps->printer_category), 1, 0, ew);
		    printer_msg++;
		}
		else
		  q_status_message(SM_ORDER,3,5, _("Trouble setting default printer"));

		retval = 1;
	    }
	    else
	      retval = 0;
	}
	else
	  retval = 0;

	if(retval){
	    ps->mangled_body = 1;	/* BUG: redraw it all for now? */
	    set_def_printer_value(ps->VAR_PRINTER);
	}

	break;

      default:
	retval = -1;
	break;
    }

    if(printer_msg){
	p = NULL;
	if(ps->VAR_PRINTER){
	    char *nick, *q;

	    parse_printer(ps->VAR_PRINTER, &nick, &q,
			  NULL, NULL, NULL, NULL);
	    p = cpystr(*nick ? nick : q);
	    fs_give((void **)&nick);
	    fs_give((void **)&q);
	}

	q_status_message4(SM_ORDER, 0, 3,
			  "Default printer%s %s%s%s",
			  ((!printer_was && !ps->VAR_PRINTER) ||
			   (printer_was && ps->VAR_PRINTER &&
			    !strcmp(printer_was,ps->VAR_PRINTER)))
			      ? " still" : "",
			  p ? "set to \"" : "unset",
			  p ? p : "", p ? "\"" : ""); 

	if(p)
	  fs_give((void **)&p);
    }

    if(printer_was)
      fs_give((void **)&printer_was);

    return(retval);
}


int
print_edit_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    char	     prompt[81], sval[MAXPATH+1], name[MAXPATH+1];
    char            *nick, *p, *tmp, **newval = NULL, **ltmp = NULL;
    char           **lval, **nelval;
    int		     rv = 0, skip_to_next = 0, after = 0, i = 4, j, k = 0;
    int		     oeflags, changing_selected = 0, no_ex;
    HelpType         help;
    ESCKEY_S         ekey[6];

    /* need this to preserve old behavior when no exception config file */
    no_ex = (ps_global->ew_for_except_vars == Main);

    if(cmd == MC_CHOICE)
      return(print_select_tool(ps, cmd, cl, flags));

    if(!(cl && *cl && (*cl)->var))
      return(0);

    nelval = no_ex ? (*cl)->var->current_val.l : LVAL((*cl)->var, ew);
    lval = LVAL((*cl)->var, ew);

    switch(cmd){
      case MC_ADD:				/* add to list */
	sval[0] = '\0';
	if(!fixed_var((*cl)->var, "add to", NULL)){

	    if(lval && (*cl)->value){
		strncpy(prompt, _("Enter printer name : "), sizeof(prompt));
		prompt[sizeof(prompt)-1] = '\0';
	    }
	    else if(!lval && nelval){
		/* Add to list which doesn't exist, but default does exist */
		ekey[0].ch    = 'r';
		ekey[0].rval  = 'r';
		ekey[0].name  = "R";
		ekey[0].label = N_("Replace");
		ekey[1].ch    = 'a';
		ekey[1].rval  = 'a';
		ekey[1].name  = "A";
		ekey[1].label = N_("Add To");
		ekey[2].ch    = -1;
		strncpy(prompt, _("Replace or Add To default value ? "), sizeof(prompt));
		prompt[sizeof(prompt)-1] = '\0';
		switch(i = radio_buttons(prompt, -FOOTER_ROWS(ps), ekey, 'a',
					 'x', h_config_replace_add, RB_NORM)){
		  case 'a':
		    /* Make a list of the default commands, leaving room for 
		       the command we are about to add below. */
		    for(k = 0; nelval[k]; k++)
		      ;

		    ltmp = (char **)fs_get((k+2) * sizeof(char *));
		    
		    for(j = 0; j < k; j++)
		      ltmp[j] = cpystr(nelval[j]);

		    ltmp[k + 1] = ltmp[k] = NULL;

add_text:
		    strncpy(prompt, _("Enter name of printer to be added : "), sizeof(prompt));
		    prompt[sizeof(prompt)-1] = '\0';
		    break;
		    
		  case 'r':
replace_text:
		    strncpy(prompt, _("Enter the name for replacement printer : "), sizeof(prompt));
		    prompt[sizeof(prompt)-1] = '\0';
		    break;
		    
		  case 'x':
		    cmd_cancelled("Add");
		    break;
		}

		if(i == 'x')
		  break;
	    }
	    else{
		strncpy(prompt, _("Enter name of printer to be added : "), sizeof(prompt));
		prompt[sizeof(prompt)-1] = '\0';
	    }

	    ps->mangled_footer = 1;
	    help = NO_HELP;

	    name[0] = '\0';
	    i = 2;
	    while(i != 0 && i != 1){
		if(lval && (*cl)->value){
		    ekey[0].ch    = ctrl('W');
		    ekey[0].rval  = 5;
		    ekey[0].name  = "^W";
		    ekey[0].label = after ? N_("InsertBefore") : N_("InsertAfter");
		    ekey[1].ch    = -1;
		}
		else
		  ekey[0].ch    = -1;

		oeflags = OE_APPEND_CURRENT;
		i = optionally_enter(name, -FOOTER_ROWS(ps), 0, sizeof(name),
				     prompt,
				     (ekey[0].ch != -1) ? ekey : NULL,
				     help, &oeflags);
		if(i == 0){
		    rv = ps->mangled_body = 1;
		    removing_leading_and_trailing_white_space(name);
		}
		else if(i == 1){
		    cmd_cancelled("Add");
		}
		else if(i == 3){
		    help = (help == NO_HELP) ? h_config_insert_after : NO_HELP;
		}
		else if(i == 4){		/* no redraw, yet */
		}
		else if(i == 5){ /* change from/to prepend to/from append */
		    after = after ? 0 : 1;
		}
	    }

	    if(i == 0)
	      i = 2;

#ifdef OS2
	    strncpy(prompt, "Enter port or |command : ", sizeof(prompt));
#else
	    strncpy(prompt, _("Enter command for printer : "), sizeof(prompt));
#endif
	    prompt[sizeof(prompt)-1] = '\0';
	    while(i != 0 && i != 1){
		oeflags = OE_APPEND_CURRENT;
		i = optionally_enter(sval, -FOOTER_ROWS(ps), 0, sizeof(sval),
				     prompt,
				     (ekey[0].ch != -1) ? ekey : NULL,
				     help, &oeflags);
		if(i == 0){
		    rv = ps->mangled_body = 1;
		    removing_leading_and_trailing_white_space(sval);
		    if(*sval || !lval){

			for(tmp = sval; *tmp; tmp++)
			  if(*tmp == ',')
			  i++;	/* conservative count of ,'s */

			if(!i){	/* only one item */
			  if (!ltmp){
			    ltmp = (char **)fs_get(2 * sizeof(char *));
			    ltmp[1] = NULL;
			    k = 0;
			  }
			  if(*name){
			    size_t l;

			    l = strlen(name) + 4 + strlen(sval);
			    ltmp[k] = (char *) fs_get((l+1) * sizeof(char));
			    snprintf(ltmp[k], l+1, "%s [] %s", name, sval);
			    ltmp[k][l] = '\0';
			  }
			  else
			    ltmp[k] = cpystr(sval);
			}
			else{
			    /*
			     * Don't allow input of multiple entries at once.
			     */
			    q_status_message(SM_ORDER,3,5,
				_("No commas allowed in command"));
			    i = 2;
			    continue;
			}

			config_add_list(ps, cl, ltmp, &newval, after);

			if(after)
			  skip_to_next = 1;

			fs_give((void **)&ltmp);
			k = 0;
		    }
		    else
		      q_status_message1(SM_ORDER, 0, 3,
					 _("Can't add %s to list"), empty_val);
		}
		else if(i == 1){
		    cmd_cancelled("Add");
		}
		else if(i == 3){
		    help = help == NO_HELP ? h_config_print_cmd : NO_HELP;
		}
		else if(i == 4){		/* no redraw, yet */
		}
		else if(i == 5){ /* change from/to prepend to/from append */
		    after = after ? 0 : 1;
		}
	    }
	}

	break;

      case MC_DELETE:					/* delete */
	if((*cl)->var->current_val.l
	  && (*cl)->var->current_val.l[(*cl)->varmem]
	  && !strucmp(ps->VAR_PRINTER,(*cl)->var->current_val.l[(*cl)->varmem]))
	    changing_selected = 1;

	if(!lval && nelval){
	    char pmt[80];

	    snprintf(pmt, sizeof(pmt), _("Override default with %s"), empty_val2);
	    pmt[sizeof(pmt)-1] = '\0';
	    if(want_to(pmt, 'n', 'n', NO_HELP, WT_FLUSH_IN) == 'y'){
		char **ltmp;

		sval[0] = '\0';
		ltmp    = (char **)fs_get(2 * sizeof(char *));
		ltmp[0] = cpystr(sval);
		ltmp[1] = NULL;
		config_add_list(ps, cl, ltmp, &newval, 0);
		fs_give((void **)&ltmp);
		rv = ps->mangled_body = 1;
	    }
	}
	else if(!lval){
	    q_status_message(SM_ORDER, 0, 3, _("No set value to delete"));
	}
	else{
	    if((*cl)->var->is_fixed){
		parse_printer(lval[(*cl)->varmem],
		    &nick, &p, NULL, NULL, NULL, NULL);
	        snprintf(prompt, sizeof(prompt), _("Delete (unused) printer %s "),
		    *nick ? nick : (!*p) ? empty_val2 : p);
		fs_give((void **)&nick);
		fs_give((void **)&p);
	    }
	    else
	      snprintf(prompt, sizeof(prompt), _("Really delete item %s from printer list "),
		    int2string((*cl)->varmem + 1));

	    prompt[sizeof(prompt)-1] = '\0';

	    ps->mangled_footer = 1;
	    if(want_to(prompt,'n','n',h_config_print_del, WT_FLUSH_IN) == 'y'){
		rv = ps->mangled_body = 1;
		fs_give((void **)&lval[(*cl)->varmem]);
		config_del_list_item(cl, &newval);
	    }
	    else
	      q_status_message(SM_ORDER, 0, 3, _("Printer not deleted"));
	}

	break;

      case MC_EDIT:				/* edit/change list option */
	if((*cl)->var->current_val.l
	  && (*cl)->var->current_val.l[(*cl)->varmem]
	  && !strucmp(ps->VAR_PRINTER,(*cl)->var->current_val.l[(*cl)->varmem]))
	    changing_selected = 1;

	if(fixed_var((*cl)->var, NULL, "printer"))
	  break;
	else if(!lval &&  nelval)
	  goto replace_text;
	else if(!lval && !nelval)
	  goto add_text;
	else{
	    HelpType help;

	    ekey[0].ch    = 'n';
	    ekey[0].rval  = 'n';
	    ekey[0].name  = "N";
	    ekey[0].label = N_("Name");
	    ekey[1].ch    = 'c';
	    ekey[1].rval  = 'c';
	    ekey[1].name  = "C";
	    ekey[1].label = N_("Command");
	    ekey[2].ch    = 'o';
	    ekey[2].rval  = 'o';
	    ekey[2].name  = "O";
	    ekey[2].label = N_("Options");
	    ekey[3].ch    = -1;
	    /* TRANSLATORS: this is a question with three choices */
	    strncpy(prompt, _("Change Name or Command or Options ? "), sizeof(prompt));
	    prompt[sizeof(prompt)-1] = '\0';
	    i = radio_buttons(prompt, -FOOTER_ROWS(ps), ekey, 'c', 'x',
			      h_config_print_name_cmd, RB_NORM);

	    if(i == 'x'){
		cmd_cancelled("Change");
		break;
	    } 
	    else if(i == 'c'){
		char *all_but_cmd;

		parse_printer(lval[(*cl)->varmem],
			      NULL, &p, NULL, NULL, NULL, &all_but_cmd);
		
		strncpy(prompt, _("Change command : "), sizeof(prompt));
		prompt[sizeof(prompt)-1] = '\0';
		strncpy(sval, p ? p : "", sizeof(sval)-1);
		sval[sizeof(sval)-1] = '\0';
		fs_give((void **)&p);

		ps->mangled_footer = 1;
		help = NO_HELP;
		while(1){
		    oeflags = OE_APPEND_CURRENT;
		    i = optionally_enter(sval, -FOOTER_ROWS(ps), 0,
				         sizeof(sval), prompt, NULL,
					 help, &oeflags);
		    if(i == 0){
			removing_leading_and_trailing_white_space(sval);
			rv = ps->mangled_body = 1;
			if(lval[(*cl)->varmem])
			  fs_give((void **)&lval[(*cl)->varmem]);

			i = 0;
			for(tmp = sval; *tmp; tmp++)
			  if(*tmp == ',')
			    i++;	/* count of ,'s */

			if(!i){	/* only one item */
			    size_t l;

			    l = strlen(all_but_cmd) + strlen(sval);
			    lval[(*cl)->varmem] = (char *)fs_get((l+1) * sizeof(char));
			    snprintf(lval[(*cl)->varmem], l+1, "%s%s", all_but_cmd, sval);
			    lval[(*cl)->varmem][l] = '\0';
			    
			    newval = &(*cl)->value;
			}
			else{
			    /*
			     * Don't allow input of multiple entries at once.
			     */
			    q_status_message(SM_ORDER,3,5,
				_("No commas allowed in command"));
			    continue;
			}
		    }
		    else if(i == 1){
			cmd_cancelled("Change");
		    }
		    else if(i == 3){
			help = help == NO_HELP ? h_config_change : NO_HELP;
			continue;
		    }
		    else if(i == 4){		/* no redraw, yet */
			continue;
		    }

		    break;
		}
	    }
	    else if(i == 'n'){
		char *all_but_nick;

		parse_printer(lval[(*cl)->varmem],
			      &p, NULL, NULL, NULL, &all_but_nick, NULL);
		
		strncpy(prompt, _("Change name : "), sizeof(prompt));
		prompt[sizeof(prompt)-1] = '\0';
		strncpy(name, p ? p : "", sizeof(name));
		name[sizeof(name)-1] = '\0';

		fs_give((void **)&p);

		ps->mangled_footer = 1;
		help = NO_HELP;
		while(1){
		    oeflags = OE_APPEND_CURRENT;
		    i = optionally_enter(name, -FOOTER_ROWS(ps), 0,
					 sizeof(name), prompt, NULL,
					 help, &oeflags);
		    if(i == 0){
			size_t l;

			rv = ps->mangled_body = 1;
			removing_leading_and_trailing_white_space(name);
			if(lval[(*cl)->varmem])
			  fs_give((void **)&lval[(*cl)->varmem]);

			l = strlen(name) + 1 + ((*all_but_nick == '[') ? 0 : 3) + strlen(all_but_nick);
			lval[(*cl)->varmem] = (char *)fs_get((l+1) * sizeof(char));
			snprintf(lval[(*cl)->varmem], l+1,
			    "%s %s%s", name,
			    (*all_but_nick == '[') ? "" : "[] ",
			    all_but_nick);
			lval[(*cl)->varmem][l] = '\0';
			
			newval = &(*cl)->value;
		    }
		    else if(i == 1){
			cmd_cancelled("Change");
		    }
		    else if(i == 3){
			help = help == NO_HELP ? h_config_change : NO_HELP;
			continue;
		    }
		    else if(i == 4){		/* no redraw, yet */
			continue;
		    }

		    break;
		}
		
		fs_give((void **)&all_but_nick);
	    }
	    else if(i == 'o'){
		HelpType help;

		ekey[0].ch    = 'i';
		ekey[0].rval  = 'i';
		ekey[0].name  = "I";
		ekey[0].label = N_("Init");
		ekey[1].ch    = 't';
		ekey[1].rval  = 't';
		ekey[1].name  = "T";
		ekey[1].label = N_("Trailer");
		ekey[2].ch    = -1;
		strncpy(prompt, _("Change Init string or Trailer string ? "), sizeof(prompt));
		prompt[sizeof(prompt)-1] = '\0';
		j = radio_buttons(prompt, -FOOTER_ROWS(ps), ekey, 'i', 'x',
				  h_config_print_opt_choice, RB_NORM);

		if(j == 'x'){
		    cmd_cancelled("Change");
		    break;
		} 
		else{
		    char *init, *trailer;

		    parse_printer(lval[(*cl)->varmem],
				  &nick, &p, &init, &trailer, NULL, NULL);
		    
		    if(j == i)
		      snprintf(prompt, sizeof(prompt), _("Change INIT string : "));
		    else
		      snprintf(prompt, sizeof(prompt), _("Change TRAILER string : "));

		    strncpy(sval, (j == 'i') ? init : trailer, sizeof(sval)-1);
		    sval[sizeof(sval)-1] = '\0';

		    tmp = string_to_cstring(sval);
		    strncpy(sval, tmp, sizeof(sval)-1);
		    sval[sizeof(sval)-1] = '\0';
		    fs_give((void **)&tmp);
		    
		    ps->mangled_footer = 1;
		    help = NO_HELP;
		    while(1){
			oeflags = OE_APPEND_CURRENT;
			i = optionally_enter(sval, -FOOTER_ROWS(ps), 0,
			    sizeof(sval), prompt, NULL, help, &oeflags);
			if(i == 0){
			    size_t l;

			    removing_leading_and_trailing_white_space(sval);
			    rv = 1;
			    if(lval[(*cl)->varmem])
			      fs_give((void **)&lval[(*cl)->varmem]);
			    if(j == 'i'){
				init = cstring_to_hexstring(sval);
				tmp = cstring_to_hexstring(trailer);
				fs_give((void **)&trailer);
				trailer = tmp;
			    }
			    else{
				trailer = cstring_to_hexstring(sval);
				tmp = cstring_to_hexstring(init);
				fs_give((void **)&init);
				init = tmp;
			    }

			    l = strlen(nick) + 1 + 2 + strlen("INIT=") + strlen(init) + 1 + strlen("TRAILER=") + strlen(trailer)+ 1 + strlen(p);
			    lval[(*cl)->varmem] = (char *)fs_get((l+1) * sizeof(char));
			    snprintf(lval[(*cl)->varmem], l+1,
				"%s%s%s%s%s%s%s%s%s%s%s",
	    /* nick */	    nick,
	    /* space */	    *nick ? " " : "",
	    /* [ */		    (*nick || *init || *trailer) ? "[" : "",
	    /* INIT= */	    *init ? "INIT=" : "",
	    /* init */	    init,
	    /* space */	    (*init && *trailer) ? " " : "",
	    /* TRAILER= */	    *trailer ? "TRAILER=" : "",
	    /* trailer */	    trailer,
	    /* ] */		    (*nick || *init || *trailer) ? "]" : "",
	    /* space */	    (*nick || *init || *trailer) ? " " : "",
	    /* command */	    p);
			    lval[(*cl)->varmem][l] = '\0';
	    
			    newval = &(*cl)->value;
			}
			else if(i == 1){
			    cmd_cancelled("Change");
			}
			else if(i == 3){
			    help=(help == NO_HELP)?h_config_print_init:NO_HELP;
			    continue;
			}
			else if(i == 4){		/* no redraw, yet */
			    continue;
			}

			break;
		    }

		    fs_give((void **)&nick);
		    fs_give((void **)&p);
		    fs_give((void **)&init);
		    fs_give((void **)&trailer);
		}
	    }
	}

	break;

      case MC_EXIT:				/* exit */
	rv = config_exit_cmd(flags);
	break;

      default:
	rv = -1;
	break;
    }

    if(skip_to_next)
      *cl = next_confline(*cl);

    /*
     * At this point, if changes occurred, var->user_val.X is set.
     * So, fix the current_val, and handle special cases...
     */
    if(rv == 1){
	set_current_val((*cl)->var, TRUE, FALSE);
	fix_side_effects(ps, (*cl)->var, 0);

	if(newval){
	    if(*newval)
	      fs_give((void **)newval);
	    
	    if((*cl)->var->current_val.l)
	      *newval = printer_name((*cl)->var->current_val.l[(*cl)->varmem]);
	    else
	      *newval = cpystr("");
	}

	if(changing_selected)
	  print_select_tool(ps, MC_CHOICE, cl, flags);
    }

    return(rv);
}


/*
 * Given a single printer string from the config file, returns an allocated
 * copy of the friendly printer name, which is
 *      "Nickname"  command
 */
char *
printer_name(char *input)
{
    char *nick, *cmd;
    char *ret;

    parse_printer(input, &nick, &cmd, NULL, NULL, NULL, NULL);
    ret = (char *)fs_get((2+6*22+1+strlen(cmd)) * sizeof(char));
    utf8_snprintf(ret, 2+6*22+1+strlen(cmd), "\"%.21w\"%*s%s",
	*nick ? nick : "",
	22 - MIN(utf8_width(nick), 21),
	"",
	cmd);
    fs_give((void **) &nick);
    fs_give((void **) &cmd);

    return(ret);
}
