#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: confscroll.c 1169 2008-08-27 06:42:06Z hubert@u.washington.edu $";
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
#include "confscroll.h"
#include "keymenu.h"
#include "status.h"
#include "titlebar.h"
#include "help.h"
#include "radio.h"
#include "print.h"
#include "ldapconf.h"
#include "roleconf.h"
#include "colorconf.h"
#include "mailview.h"
#include "mailcmd.h"
#include "mailindx.h"
#include "talk.h"
#include "setup.h"
#include "smime.h"
#include "../pith/state.h"
#include "../pith/flag.h"
#include "../pith/list.h"
#include "../pith/conf.h"
#include "../pith/util.h"
#include "../pith/newmail.h"
#include "../pith/sort.h"
#include "../pith/thread.h"
#include "../pith/color.h"
#include "../pith/hist.h"
#include "../pith/icache.h"
#include "../pith/conf.h"
#include "../pith/init.h"
#include "../pith/folder.h"
#include "../pith/busy.h"
#include "../pith/tempfile.h"
#include "../pith/pattern.h"
#include "../pith/charconv/utf8.h"


#define	CONFIG_SCREEN_HELP_TITLE	_("HELP FOR SETUP CONFIGURATION")

/* TRANSLATORS: Empty Value is what is shown in the configuration
   screen when the user not only does not set an option but also
   wants to explicitly not use the default value. Empty value means
   an option with no value. */
char *empty_val  = N_("Empty Value");
char *empty_val2 = N_("<Empty Value>");
/* TRANSLATORS: No Value set is similar to Empty Value, but the
   user has not explicitly decided to not use the default. It is
   just an option which the user has left at the default value. */
char *no_val     = N_("No Value Set");
/* TRANSLATORS: Value is Fixed is what is displayed in the config
   screen when the system managers have set an option to a specific
   value and they don't allow the user to change it. The value
   is fixed to a certain value. This isn't the same word as
   Repaired, it means Unchanging. */
char *fixed_val  = N_("Value is Fixed");
char yesstr[] = "Yes";
char nostr[]  = "No";


EditWhich ew = Main;


OPT_SCREEN_S *opt_screen;


/*
 * This is pretty ugly. Some of the routines operate differently depending
 * on which variable they are operating on. Sometimes those variables are
 * global (real alpine.h V_ style variables) and sometimes they are just
 * local variables (as in role_config_edit_screen). These pointers are here
 * so that the routines can figure out which variable they are operating
 * on and do the right thing.
 */
struct variable	        *score_act_global_ptr,
			*scorei_pat_global_ptr,
			*age_pat_global_ptr,
			*size_pat_global_ptr,
			*cati_global_ptr,
			*cat_cmd_global_ptr,
			*cat_lim_global_ptr,
			*startup_ptr,
			*role_comment_ptr,
			*role_forw_ptr,
			*role_repl_ptr,
			*role_fldr_ptr,
			*role_afrom_ptr,
			*role_filt_ptr,
			*role_status1_ptr,
			*role_status2_ptr,
			*role_status3_ptr,
			*role_status4_ptr,
			*role_status5_ptr,
			*role_status6_ptr,
			*role_status7_ptr,
			*role_status8_ptr,
			*msg_state1_ptr,
			*msg_state2_ptr,
			*msg_state3_ptr,
			*msg_state4_ptr;


typedef NAMEVAL_S *(*PTR_TO_RULEFUNC)(int);


/*
 * Internal prototypes
 */
PTR_TO_RULEFUNC rulefunc_from_var(struct pine *, struct variable *);
void     set_radio_pretty_vals(struct pine *, CONF_S **);
int      save_include(struct pine *, struct variable *, int);
void	 config_scroll_up(long);
void	 config_scroll_down(long);
void	 config_scroll_to_pos(long);
CONF_S  *config_top_scroll(struct pine *, CONF_S *);
void	 update_option_screen(struct pine *, OPT_SCREEN_S *, Pos *);
void	 print_option_screen(OPT_SCREEN_S *, char *);
void	 option_screen_redrawer(void);
char    *text_pretty_value(struct pine *, CONF_S *);
char    *checkbox_pretty_value(struct pine *, CONF_S *);
char    *yesno_pretty_value(struct pine *, CONF_S *);
char    *radio_pretty_value(struct pine *, CONF_S *);
char    *sigfile_pretty_value(struct pine *, CONF_S *);
char    *color_pretty_value(struct pine *, CONF_S *);
char    *sort_pretty_value(struct pine *, CONF_S *);
int      longest_feature_name(void);
COLOR_PAIR *sample_color(struct pine *, struct variable *);
COLOR_PAIR *sampleexc_color(struct pine *, struct variable *);
void     clear_feature(char ***, char *);
CONF_S	*last_confline(CONF_S *);
#ifdef	_WINDOWS
int	 config_scroll_callback(int, long);
#endif


/*
 * We test for this same set of vars in a few places.
 */
int
standard_radio_var(struct pine *ps, struct variable *v)
{
    return(v == &ps->vars[V_SAVED_MSG_NAME_RULE] ||
	   v == &ps->vars[V_FCC_RULE] ||
	   v == &ps->vars[V_GOTO_DEFAULT_RULE] ||
	   v == &ps->vars[V_INCOMING_STARTUP] ||
	   v == &ps->vars[V_PRUNING_RULE] ||
	   v == &ps->vars[V_REOPEN_RULE] ||
	   v == &ps->vars[V_THREAD_DISP_STYLE] ||
	   v == &ps->vars[V_THREAD_INDEX_STYLE] ||
	   v == &ps->vars[V_FLD_SORT_RULE] ||
#ifndef	_WINDOWS
	   v == &ps->vars[V_COLOR_STYLE] ||
#endif
	   v == &ps->vars[V_INDEX_COLOR_STYLE] ||
	   v == &ps->vars[V_TITLEBAR_COLOR_STYLE] ||
	   v == &ps->vars[V_AB_SORT_RULE]);
}


PTR_TO_RULEFUNC
rulefunc_from_var(struct pine *ps, struct variable *v)
{
    PTR_TO_RULEFUNC rulefunc = NULL;

    if(v == &ps->vars[V_SAVED_MSG_NAME_RULE])
      rulefunc = save_msg_rules;
    else if(v == &ps->vars[V_FCC_RULE])
      rulefunc = fcc_rules;
    else if(v == &ps->vars[V_GOTO_DEFAULT_RULE])
      rulefunc = goto_rules;
    else if(v == &ps->vars[V_INCOMING_STARTUP])
      rulefunc = incoming_startup_rules;
    else if(v == startup_ptr)
      rulefunc = startup_rules;
    else if(v == &ps->vars[V_PRUNING_RULE])
      rulefunc = pruning_rules;
    else if(v == &ps->vars[V_REOPEN_RULE])
      rulefunc = reopen_rules;
    else if(v == &ps->vars[V_THREAD_DISP_STYLE])
      rulefunc = thread_disp_styles;
    else if(v == &ps->vars[V_THREAD_INDEX_STYLE])
      rulefunc = thread_index_styles;
    else if(v == &ps->vars[V_FLD_SORT_RULE])
      rulefunc = fld_sort_rules;
    else if(v == &ps->vars[V_AB_SORT_RULE])
      rulefunc = ab_sort_rules;
    else if(v == &ps->vars[V_INDEX_COLOR_STYLE])
      rulefunc = index_col_style;
    else if(v == &ps->vars[V_TITLEBAR_COLOR_STYLE])
      rulefunc = titlebar_col_style;
#ifndef	_WINDOWS
    else if(v == &ps->vars[V_COLOR_STYLE])
      rulefunc = col_style;
#endif
    
    return(rulefunc);
}


void
standard_radio_setup(struct pine *ps, CONF_S **cl, struct variable *v, CONF_S **first_line)
{
    int     i, rindent = 12;
    CONF_S *ctmpb;
    PTR_TO_RULEFUNC rulefunc;
    NAMEVAL_S *f;
    char b[100];

    if(!(cl && *cl))
      return;

    rulefunc = rulefunc_from_var(ps, v);
    ctmpb = (*cl);

    (*cl)->flags		|= CF_NOSELECT;
    (*cl)->keymenu      	 = &config_radiobutton_keymenu;
    (*cl)->tool			 = NULL;

    /* put a nice delimiter before list */
    new_confline(cl)->var	 = NULL;
    (*cl)->varnamep		 = ctmpb;
    (*cl)->keymenu		 = &config_radiobutton_keymenu;
    (*cl)->help			 = NO_HELP;
    (*cl)->tool			 = radiobutton_tool;
    (*cl)->valoffset		 = rindent;
    (*cl)->flags		|= CF_NOSELECT;
    /* TRANSLATORS: Set and Rule Values are the headings for an option
       that can take one of several values. Underneath the Set heading
       will be a column where one possibility is turned on (is Set).
       The other column will be very short descriptions of what
       the possibilities are (the Rule Values). */
    utf8_snprintf(b, sizeof(b), "%-5.5w  %s", _("Set"), _("Rule Values"));
    (*cl)->value		 = cpystr(b);

    new_confline(cl)->var	 = NULL;
    (*cl)->varnamep		 = ctmpb;
    (*cl)->keymenu		 = &config_radiobutton_keymenu;
    (*cl)->help			 = NO_HELP;
    (*cl)->tool			 = radiobutton_tool;
    (*cl)->valoffset		 = rindent;
    (*cl)->flags		|= CF_NOSELECT;
    (*cl)->value		 = cpystr("---  ----------------------");

    if(rulefunc)
      for(i = 0; (f = (*rulefunc)(i)); i++){
	new_confline(cl)->var	= v;
	if(first_line && !*first_line && !pico_usingcolor())
	  *first_line = (*cl);

	(*cl)->varnamep		= ctmpb;
	(*cl)->keymenu		= &config_radiobutton_keymenu;
	(*cl)->help		= (v == startup_ptr)
				    ? h_config_other_startup
				    : config_help(v - ps->vars,0);
	(*cl)->tool		= radiobutton_tool;
	(*cl)->valoffset	= rindent;
	(*cl)->varmem		= i;
	(*cl)->value		= pretty_value(ps, *cl);
      }
}


/*
 * Reset the displayed values for all of the lines for this
 * variable because others besides this line may change.
 */
void
set_radio_pretty_vals(struct pine *ps, CONF_S **cl)
{
    CONF_S *ctmp;

    if(!(cl && *cl &&
       ((*cl)->var == &ps->vars[V_SORT_KEY] ||
        standard_radio_var(ps, (*cl)->var) ||
	(*cl)->var == startup_ptr)))
      return;

    /* hunt backwards */
    for(ctmp = *cl;
	ctmp && !(ctmp->flags & CF_NOSELECT) && !ctmp->varname;
	ctmp = prev_confline(ctmp)){
	if(ctmp->value)
	  fs_give((void **)&ctmp->value);
	
	ctmp->value = pretty_value(ps, ctmp);
    }

    /* hunt forwards */
    for(ctmp = *cl;
	ctmp && !ctmp->varname && !(ctmp->flags & CF_NOSELECT);
	ctmp = next_confline(ctmp)){
	if(ctmp->value)
	  fs_give((void **)&ctmp->value);
	
	ctmp->value = pretty_value(ps, ctmp);
    }
}


/*
 * test whether or not a var is 
 *
 * returns:  1 if it should be excluded, 0 otw
 */
int
exclude_config_var(struct pine *ps, struct variable *var, int allow_hard_to_config_remotely)
{
    if((ew != Main && (var->is_onlymain)) ||
       (ew != ps_global->ew_for_except_vars && var->is_outermost))
      return(1);

    if(allow_hard_to_config_remotely)
      return(!(var->is_user && var->is_used && !var->is_obsolete));

    switch(var - ps->vars){
      case V_MAIL_DIRECTORY :
      case V_INCOMING_FOLDERS :
      case V_FOLDER_SPEC :
      case V_NEWS_SPEC :
      case V_STANDARD_PRINTER :
      case V_LAST_TIME_PRUNE_QUESTION :
      case V_LAST_VERS_USED :
      case V_ADDRESSBOOK :
      case V_GLOB_ADDRBOOK :
      case V_DISABLE_DRIVERS :
      case V_DISABLE_AUTHS :
      case V_REMOTE_ABOOK_METADATA :
      case V_REMOTE_ABOOK_HISTORY :
      case V_REMOTE_ABOOK_VALIDITY :
      case V_OPER_DIR :
      case V_USERINPUTTIMEO :
      case V_TCPOPENTIMEO :
      case V_TCPREADWARNTIMEO :
      case V_TCPWRITEWARNTIMEO :
      case V_TCPQUERYTIMEO :
      case V_RSHCMD :
      case V_RSHPATH :
      case V_RSHOPENTIMEO :
      case V_SSHCMD :
      case V_SSHPATH :
      case V_SSHOPENTIMEO :
      case V_SENDMAIL_PATH :
      case V_NEW_VER_QUELL :
      case V_PATTERNS :
      case V_PAT_ROLES :
      case V_PAT_FILTS :
      case V_PAT_SCORES :
      case V_PAT_INCOLS :
      case V_PAT_OTHER :
      case V_PAT_SRCH :
      case V_PRINTER :
      case V_PERSONAL_PRINT_COMMAND :
      case V_PERSONAL_PRINT_CATEGORY :
      case V_RSS_NEWS :
      case V_RSS_WEATHER :
      case V_WP_INDEXHEIGHT :
      case V_WP_INDEXLINES :
      case V_WP_AGGSTATE :
      case V_WP_STATE :
      case V_WP_COLUMNS :
#ifndef	_WINDOWS
      case V_OLD_CHAR_SET :
#endif	/* ! _WINDOWS */
#if defined(DOS) || defined(OS2)
      case V_UPLOAD_CMD :
      case V_UPLOAD_CMD_PREFIX :
      case V_DOWNLOAD_CMD :
      case V_DOWNLOAD_CMD_PREFIX :
#ifdef	_WINDOWS
      case V_FONT_NAME :
      case V_FONT_SIZE :
      case V_FONT_STYLE :
      case V_FONT_CHAR_SET :
      case V_PRINT_FONT_NAME :
      case V_PRINT_FONT_SIZE :
      case V_PRINT_FONT_STYLE :
      case V_PRINT_FONT_CHAR_SET :
      case V_WINDOW_POSITION :
      case V_CURSOR_STYLE :
#endif	/* _WINDOWS */
#endif	/* DOS */
#ifdef	ENABLE_LDAP
      case V_LDAP_SERVERS :
#endif	/* ENABLE_LDAP */
	return(1);

      default:
	break;
    }

    return(!(var->is_user && var->is_used && !var->is_obsolete &&
#ifdef	SMIME
	     !smime_related_var(ps, var) &&
#endif	/* SMIME */
	     !color_related_var(ps, var)));
}


/*
 * Test to indicate what should be saved in case user wants to abandon
 * changes.
 */
int
save_include(struct pine *ps, struct variable *v, int allow_hard_to_config_remotely)
{
    return(!exclude_config_var(ps, v, allow_hard_to_config_remotely)
	   || (v->is_user
	    && v->is_used
	    && !v->is_obsolete
	    && (v == &ps->vars[V_PERSONAL_PRINT_COMMAND]
#ifdef	ENABLE_LDAP
	     || v == &ps->vars[V_LDAP_SERVERS]
#endif
		)));
}


/*
 * Handles screen painting and motion.  Passes other commands to
 * custom tools.
 *
 * Tool return values:  Tools should return the following:
 *     0 nothing changed
 *    -1 unrecognized command
 *     1 something changed, conf_scroll_screen should remember that
 *     2 tells conf_scroll_screen to return with value 1 or 0 depending
 *       on whether or not it has previously gotten a 1 from some tool.
 *     3 tells conf_scroll_screen to return 1 (like 1 and 2 combined)
 *     ? Other tool-specific values can be used.  They will cause
 *       conf_scroll_screen to return that value.
 *
 * Return values:
 *     0 if nothing happened.  That is, a tool returned 2 and we hadn't
 *       previously noted a return of 1
 *     1 if something happened.  That is, a tool returned 2 and we had
 *       previously noted a return of 1
 *     ? Tool-returned value different from -1, 0, 1, 2, or 3. This is it.
 *
 * Special proviso: If first_line->flags has CF_CHANGES set on entry, then
 * that will cause things to behave like a change was made since entering
 * this function.
 */
int
conf_scroll_screen(struct pine *ps, OPT_SCREEN_S *screen, CONF_S *start_line, char *title, char *pdesc, int multicol)
{
    char	  tmp[MAXPATH+1];
    char         *utf8str;
    UCS           ch = 'x';
    int		  cmd, i, j, done = 0, changes = 0;
    int		  retval = 0;
    int		  km_popped = 0, stay_in_col = 0;
    struct	  key_menu  *km = NULL;
    CONF_S	 *ctmpa = NULL, *ctmpb = NULL;
    Pos           cursor_pos;
    OtherMenu	  what_keymenu = FirstMenu;
    void        (*prev_redrawer)(void);

    dprint((7, "conf_scroll_screen()\n"));

    if(BODY_LINES(ps) < 1){
	q_status_message(SM_ORDER | SM_DING, 3, 3, _("Screen too small"));
	return(0);
    }

    if(screen && screen->ro_warning)
      q_status_message1(SM_ORDER, 1, 3,
			/* TRANSLATORS: "Config file not changeable," is what replaces the %s */
			_("%s can't change options or settings"),
			ps_global->restricted ? "Alpine demo"
					      : _("Config file not changeable,"));

    screen->current    = start_line;
    if(start_line && start_line->flags & CF_CHANGES)
      changes++;

    opt_screen	       = screen;
    ps->mangled_screen = 1;
    ps->redrawer       = option_screen_redrawer;

    while(!done){
	ps->user_says_cancel = 0;
	if(km_popped){
	    km_popped--;
	    if(km_popped == 0){
		clearfooter(ps);
		ps->mangled_body = 1;
	    }
	}

	if(ps->mangled_screen){
	    ps->mangled_header = 1;
	    ps->mangled_footer = 1;
	    ps->mangled_body   = 1;
	    ps->mangled_screen = 0;
	}

	/*----------- Check for new mail -----------*/
        if(new_mail(0, NM_TIMING(ch), NM_STATUS_MSG | NM_DEFER_SORT) >= 0)
          ps->mangled_header = 1;

	if(ps->mangled_header){
	    set_titlebar(title, ps->mail_stream,
			 ps->context_current,
			 ps->cur_folder, ps->msgmap, 1, FolderName, 0, 0, NULL);
	    ps->mangled_header = 0;
	}

	update_option_screen(ps, screen, &cursor_pos);

	if(F_OFF(F_SHOW_CURSOR, ps)){
	    cursor_pos.row = ps->ttyo->screen_rows - FOOTER_ROWS(ps);
	    cursor_pos.col = 0;
	}

	/*---- This displays new mail notification, or errors ---*/
	if(km_popped){
	    FOOTER_ROWS(ps) = 3;
	    mark_status_unknown();
	}

        display_message(ch);
	if(km_popped){
	    FOOTER_ROWS(ps) = 1;
	    mark_status_unknown();
	}

	if(ps->mangled_footer || km != screen->current->keymenu){
	    bitmap_t	 bitmap;

	    setbitmap(bitmap);

	    ps->mangled_footer = 0;
	    km                 = screen->current->keymenu;

	    if(multicol &&
	       (F_OFF(F_ARROW_NAV, ps_global) ||
	        F_ON(F_RELAXED_ARROW_NAV, ps_global))){
		menu_clear_binding(km, KEY_LEFT);
		menu_clear_binding(km, KEY_RIGHT);
		menu_clear_binding(km, KEY_UP);
		menu_clear_binding(km, KEY_DOWN);
		menu_add_binding(km, KEY_UP, MC_CHARUP);
		menu_add_binding(km, KEY_DOWN, MC_CHARDOWN);	
		menu_add_binding(km, KEY_LEFT, MC_PREVITEM);
		menu_add_binding(km, ctrl('B'), MC_PREVITEM);
		menu_add_binding(km, KEY_RIGHT, MC_NEXTITEM);
		menu_add_binding(km, ctrl('F'), MC_NEXTITEM);
	    }
	    else{
		menu_clear_binding(km, KEY_LEFT);
		menu_clear_binding(km, KEY_RIGHT);
		menu_clear_binding(km, KEY_UP);
		menu_clear_binding(km, KEY_DOWN);

		/*
		 * Fix up arrow nav mode if necessary...
		 */
		if(F_ON(F_ARROW_NAV, ps_global)){
		    int cmd;

		    if((cmd = menu_clear_binding(km, '<')) != MC_UNKNOWN){
			menu_add_binding(km, '<', cmd);
			menu_add_binding(km, KEY_LEFT, cmd);
		    }

		    if((cmd = menu_clear_binding(km, '>')) != MC_UNKNOWN){
			menu_add_binding(km, '>', cmd);
			menu_add_binding(km, KEY_RIGHT, cmd);
		    }

		    if((cmd = menu_clear_binding(km, 'p')) != MC_UNKNOWN){
			menu_add_binding(km, 'p', cmd);
			menu_add_binding(km, KEY_UP, cmd);
		    }

		    if((cmd = menu_clear_binding(km, 'n')) != MC_UNKNOWN){
			menu_add_binding(km, 'n', cmd);
			menu_add_binding(km, KEY_DOWN, cmd);
		    }
		}
	    }

	    if(km_popped){
		FOOTER_ROWS(ps) = 3;
		clearfooter(ps);
	    }

	    draw_keymenu(km, bitmap, ps->ttyo->screen_cols,
			 1-FOOTER_ROWS(ps), 0, what_keymenu);
	    what_keymenu = SameMenu;

	    if(km_popped){
		FOOTER_ROWS(ps) = 1;
		mark_keymenu_dirty();
	    }
	}

	MoveCursor(cursor_pos.row, cursor_pos.col);
#ifdef	MOUSE
	mouse_in_content(KEY_MOUSE, -1, -1, 0, 0);	/* prime the handler */
	register_mfunc(mouse_in_content, HEADER_ROWS(ps_global), 0,
		       ps_global->ttyo->screen_rows -(FOOTER_ROWS(ps_global)+1),
		       ps_global->ttyo->screen_cols);
#endif
#ifdef	_WINDOWS
	mswin_setscrollcallback(config_scroll_callback);
#endif
        /*------ Read the command from the keyboard ----*/
	ch = READ_COMMAND(&utf8str);
#ifdef	MOUSE
	clear_mfunc(mouse_in_content);
#endif
#ifdef	_WINDOWS
	mswin_setscrollcallback(NULL);
#endif

	cmd = menu_command(ch, km);

	if(km_popped)
	  switch(cmd){
	    case MC_NONE:
	    case MC_OTHER: 
	    case MC_RESIZE: 
	    case MC_REPAINT:
	      km_popped++;
	      break;
	    
	    default:
	      clearfooter(ps);
	      break;
	  }

	switch(cmd){
	  case MC_OTHER :
	    what_keymenu = NextMenu;
	    ps->mangled_footer = 1;
	    break;

	  case MC_HELP:					/* help! */
	    if(FOOTER_ROWS(ps) == 1 && km_popped == 0){
		km_popped = 2;
		ps->mangled_footer = 1;
		break;
	    }

	    if(screen->current->help != NO_HELP){
	        prev_redrawer = ps_global->redrawer;
		helper(screen->current->help,
		       (screen->current->help_title)
		         ? screen->current->help_title
		         : CONFIG_SCREEN_HELP_TITLE,
		       HLPD_SIMPLE);
		ps_global->redrawer = prev_redrawer;
		ps->mangled_screen = 1;
	    }
	    else
	      q_status_message(SM_ORDER,0,3,_("No help yet."));

	    break;


	  case MC_NEXTITEM:			/* next list element */
	  case MC_CHARDOWN:
	    stay_in_col = 0;
	    if(screen->current->flags & CF_DOUBLEVAR){
		/* if going from col1 to col2, it's simple */
		if(!(screen->current->flags & CF_VAR2) && cmd == MC_NEXTITEM){
		    screen->current->flags |= CF_VAR2;
		    break;
		}

		/* otherwise we fall through to normal next */
		stay_in_col = (screen->current->flags & CF_VAR2 &&
			       cmd == MC_CHARDOWN);
		screen->current->flags &= ~CF_VAR2;
	    }

	    for(ctmpa = next_confline(screen->current), i = 1;
		ctmpa && (ctmpa->flags & CF_NOSELECT);
		ctmpa = next_confline(ctmpa), i++)
	      ;

	    if(ctmpa){
		screen->current = ctmpa;
		if(screen->current->flags & CF_DOUBLEVAR && stay_in_col)
		  screen->current->flags |= CF_VAR2;

		if(cmd == MC_CHARDOWN){
		    for(ctmpa = screen->top_line,
			j = BODY_LINES(ps) - 1 - HS_MARGIN(ps);
			j > 0 && ctmpa && ctmpa != screen->current;
			ctmpa = next_confline(ctmpa), j--)
		      ;

		    if(!j && ctmpa){
			for(i = 0;
			    ctmpa && ctmpa != screen->current;
			    ctmpa = next_confline(ctmpa), i++)
			  ;

			if(i)
			  config_scroll_up(i);
		    }
		}
	    }
	    else{
		/*
		 * Scroll screen a bit so we show the non-selectable
		 * lines at the bottom.
		 */

		/* set ctmpa to the bottom line on the screen */
		for(ctmpa = screen->top_line, j = BODY_LINES(ps) - 1;
		    j > 0 && ctmpa;
		    ctmpa = next_confline(ctmpa), j--)
		  ;

		i = 0;
		if(ctmpa){
		    for(ctmpa = next_confline(ctmpa);
			ctmpa &&
			(ctmpa->flags & (CF_NOSELECT | CF_B_LINE)) ==
								CF_NOSELECT;
			ctmpa = next_confline(ctmpa), i++)
		      ;
		}

		if(i)
		  config_scroll_up(i);
		else
		  q_status_message(SM_ORDER,0,1, _("Already at end of screen"));
	    }

	    break;

	  case MC_PREVITEM:			/* prev list element */
	  case MC_CHARUP:
	    stay_in_col = 0;
	    if(screen->current->flags & CF_DOUBLEVAR){
		if(screen->current->flags & CF_VAR2 && cmd == MC_PREVITEM){
		    screen->current->flags &= ~CF_VAR2;
		    break;
		}

		/* otherwise we fall through to normal prev */
		stay_in_col = (!(screen->current->flags & CF_VAR2) &&
			       cmd == MC_CHARUP);
		screen->current->flags &= ~CF_VAR2;
	    }
	    else if(cmd == MC_CHARUP)
	      stay_in_col = 1;

	    ctmpa = screen->current;
	    i = 0;
	    do
	      if(ctmpa == config_top_scroll(ps, screen->top_line))
		i = 1;
	      else if(i)
		i++;
	    while((ctmpa = prev_confline(ctmpa))
		  && (ctmpa->flags&CF_NOSELECT));

	    if(ctmpa){
		screen->current = ctmpa;
		if(screen->current->flags & CF_DOUBLEVAR && !stay_in_col)
		  screen->current->flags |= CF_VAR2;

		if((cmd == MC_CHARUP) && i)
		  config_scroll_down(i);
	    }
	    else
	      q_status_message(SM_ORDER, 0, 1,
			       _("Already at start of screen"));

	    break;

	  case MC_PAGEDN:			/* page forward */
	    screen->current->flags &= ~CF_VAR2;
	    for(ctmpa = screen->top_line, i = BODY_LINES(ps);
		i > 0 && ctmpa;
		ctmpb = ctmpa, ctmpa = next_confline(ctmpa), i--)
	      ;

	    if(ctmpa){			/* first line off bottom of screen */
		ctmpb = ctmpa;
		ps->mangled_body = 1;
		/* find first selectable line on next page */
		for(screen->top_line = ctmpa;
		    ctmpa && (ctmpa->flags & CF_NOSELECT);
		    ctmpa = next_confline(ctmpa))
		  ;
		
		/*
		 * No selectable lines on next page. Slide up to first
		 * selectable.
		 */
		if(!ctmpa){
		    for(ctmpa = prev_confline(ctmpb);
			ctmpa && (ctmpa->flags & CF_NOSELECT);
			ctmpa = prev_confline(ctmpa))
		      ;
		    
		    if(ctmpa)
		      screen->top_line = ctmpa;
		}
	    }
	    else{  /* on last screen */
		/* just move current down to last entry on screen */
		if(ctmpb){		/* last line of data */
		    for(ctmpa = ctmpb, i = BODY_LINES(ps); 
			i > 0 && ctmpa && (ctmpa->flags & CF_NOSELECT);
			ctmpa = prev_confline(ctmpa), i--)
		      ;

		    if(ctmpa == screen->current){
			q_status_message(SM_ORDER,0,1,
					 _("Already at end of screen"));
			goto no_down;
		    }

		    ps->mangled_body = 1;
		}
	    }

	    if(ctmpa)
	      screen->current = ctmpa;
no_down:
	    break;

	  case MC_PAGEUP:			/* page backward */
	    ps->mangled_body = 1;
	    screen->current->flags &= ~CF_VAR2;
	    if(!(ctmpa=prev_confline(screen->top_line)))
	      ctmpa = screen->current;

	    for(i = BODY_LINES(ps) - 1;
		i > 0 && prev_confline(ctmpa);
		i--, ctmpa = prev_confline(ctmpa))
	      ;

	    for(screen->top_line = ctmpa;
	        ctmpa && (ctmpa->flags & CF_NOSELECT);
		ctmpa = next_confline(ctmpa))
	      ;

	    if(ctmpa){
		if(ctmpa == screen->current){
		    /*
		     * We get to here if there was nothing selectable on
		     * the previous page. There still may be something
		     * selectable further back than the previous page,
		     * so look for that.
		     */
		    for(ctmpa = prev_confline(screen->top_line);
			ctmpa && (ctmpa->flags & CF_NOSELECT);
			ctmpa = prev_confline(ctmpa))
		      ;

		    if(!ctmpa){
			ctmpa = screen->current;
			q_status_message(SM_ORDER, 0, 1,
					 _("Already at start of screen"));
		    }
		}

		screen->current = ctmpa;
	    }

	    break;

#ifdef MOUSE	    
	  case MC_MOUSE:
	    {   
		MOUSEPRESS mp;

		mouse_get_last (NULL, &mp);
		mp.row -= HEADER_ROWS(ps);
		ctmpa = screen->top_line;

		while (mp.row && ctmpa != NULL) {
		    --mp.row;
		    ctmpa = ctmpa->next;
		}

		if (ctmpa != NULL && !(ctmpa->flags & CF_NOSELECT)){
		    if(screen->current->flags & CF_DOUBLEVAR)
		      screen->current->flags &= ~CF_VAR2;

		    screen->current = ctmpa;

		    if(screen->current->flags & CF_DOUBLEVAR &&
		       mp.col >= screen->current->val2offset)
		      screen->current->flags |= CF_VAR2;

		    update_option_screen(ps, screen, &cursor_pos);

		    if(mp.button == M_BUTTON_LEFT && mp.doubleclick){
		       
			if(screen->current->tool){
			    unsigned flags;
			    int default_cmd;

			    flags  = screen->current->flags;
			    flags |= (changes ? CF_CHANGES : 0);

			    default_cmd = menu_command(ctrl('M'), km);
			    switch(i=(*screen->current->tool)(ps, default_cmd,
						     &screen->current, flags)){
			      case -1:
			      case 0:
				break;

			      case 1:
				changes = 1;
				break;

			      case 2:
				retval = changes;
				done++;
				break;

			      case 3:
				retval = 1;
				done++;
				break;

			      default:
				retval = i;
				done++;
				break;
			    }
			}
		    }
#ifdef	_WINDOWS
		    else if(mp.button == M_BUTTON_RIGHT) {
			MPopup other_popup[20];
			int    n = -1, cmd, i;
			struct key_menu *sckm = screen->current->keymenu; /* only for popup */

			if((cmd = menu_command(ctrl('M'), sckm)) != MC_UNKNOWN){
			    i = menu_binding_index(sckm, cmd);
			    other_popup[++n].type	= tQueue;
			    other_popup[n].label.style  = lNormal;
			    other_popup[n].label.string = sckm->keys[i].label;
			    other_popup[n].data.val   = ctrl('M');
			}
			else if((cmd = menu_command('>', sckm)) != MC_UNKNOWN){
			    i = menu_binding_index(sckm, cmd);
			    other_popup[++n].type	= tQueue;
			    other_popup[n].label.style  = lNormal;
			    other_popup[n].label.string = sckm->keys[i].label;
			    other_popup[n].data.val	= '>';
			}

			if(((i = menu_binding_index(sckm, MC_RGB1)) >= 0) ||
			   ((i = menu_binding_index(sckm, MC_RGB2)) >= 0)){
			    other_popup[++n].type	= tQueue;
			    other_popup[n].label.style  = lNormal;
			    other_popup[n].label.string = sckm->keys[i].label;
			    other_popup[n].data.val	=
							sckm->keys[i].bind.ch[0];
			}

			if((cmd = menu_command('<', sckm)) != MC_UNKNOWN){
			    i = menu_binding_index(sckm, cmd);
			    other_popup[++n].type	= tQueue;
			    other_popup[n].label.style  = lNormal;
			    other_popup[n].label.string = sckm->keys[i].label;
			    other_popup[n].data.val	= '<';
			}
			else if((i = menu_binding_index(sckm, MC_EXIT)) >= 0){
			    other_popup[++n].type	= tQueue;
			    other_popup[n].label.style  = lNormal;
			    other_popup[n].label.string = sckm->keys[i].label;
			    other_popup[n].data.val	=
							sckm->keys[i].bind.ch[0];
			}

			if((i = menu_binding_index(sckm, MC_HELP)) >= 0){
			    if(n > 0)
			      other_popup[++n].type = tSeparator;

			    other_popup[++n].type	= tQueue;
			    other_popup[n].label.style  = lNormal;
			    other_popup[n].label.string = sckm->keys[i].label;
			    other_popup[n].data.val = sckm->keys[i].bind.ch[0];
			}

			if(n > 0){
			    other_popup[++n].type = tTail;
			    mswin_popup(other_popup);
			}
		    }
		}
		else if(mp.button == M_BUTTON_RIGHT) {
		    MPopup other_popup[20];
		    int    n = -1, cmd, i;
		    struct key_menu *sckm = screen->current->keymenu; /* only for popup */

		    if((cmd = menu_command('<', sckm)) != MC_UNKNOWN){
			i = menu_binding_index(sckm, cmd);
			other_popup[++n].type	    = tQueue;
			other_popup[n].label.style  = lNormal;
			other_popup[n].label.string = sckm->keys[i].label;
			other_popup[n].data.val	    = '<';
		    }
		    else if((i = menu_binding_index(sckm, MC_EXIT)) >= 0){
			other_popup[++n].type	    = tQueue;
			other_popup[n].label.style  = lNormal;
			other_popup[n].label.string = sckm->keys[i].label;
			other_popup[n].data.val	    = sckm->keys[i].bind.ch[0];
		    }

		    other_popup[++n].type = tTail;

		    if(n > 0)
		      mswin_popup(other_popup);
#endif
		}
	    }
	    break;
#endif

	  case MC_PRINTTXT:			/* print screen */
	    print_option_screen(screen, pdesc ? pdesc : "");
	    break;

	  case MC_WHEREIS:			/* whereis */
	    /*--- get string  ---*/
	    {int   rc, found = 0;
#define FOUND_IT       0x01
#define FOUND_CURRENT  0x02
#define FOUND_WRAPPED  0x04
#define FOUND_NOSELECT 0x08
#define FOUND_ABOVE    0x10
	     char *result = NULL, buf[64];
	     char *p, last[64];
	     static HISTORY_S *history = NULL;
	     HelpType help;
	     static ESCKEY_S ekey[] = {
		{0, 0, "", ""},
		/* TRANSLATORS: go to Top of screen */
		{ctrl('Y'), 10, "^Y", N_("Top")},
		{ctrl('V'), 11, "^V", N_("Bottom")},
		{KEY_UP,    30, "", ""},
		{KEY_DOWN,  31, "", ""},
		{-1, 0, NULL, NULL}};
#define KU_WI (3)	/* index of KEY_UP */

	     init_hist(&history, HISTSIZE);
	     last[0] = '\0';
	     if((p = get_prev_hist(history, "", 0, NULL)) != NULL){
		strncpy(last, p, sizeof(last));
		last[sizeof(last)-1] = '\0';
	     }

	     ps->mangled_footer = 1;
	     buf[0] = '\0';
	     snprintf(tmp, sizeof(tmp), "Word to find %s%s%s: ",
		     (last[0]) ? "[" : "",
		     (last[0]) ? last : "",
		     (last[0]) ? "]" : "");
	     tmp[sizeof(tmp)-1] = '\0';
	     help = NO_HELP;
	     while(1){
		 int flags = OE_APPEND_CURRENT;

		/*
		 * 2 is really 1 because there will be one real entry and
		 * one entry of "" because of the get_prev_hist above.
		 */
		if(items_in_hist(history) > 2){
		    ekey[KU_WI].name  = HISTORY_UP_KEYNAME;
		    ekey[KU_WI].label = HISTORY_KEYLABEL;
		    ekey[KU_WI+1].name  = HISTORY_DOWN_KEYNAME;
		    ekey[KU_WI+1].label = HISTORY_KEYLABEL;
		}
		else{
		    ekey[KU_WI].name  = "";
		    ekey[KU_WI].label = "";
		    ekey[KU_WI+1].name  = "";
		    ekey[KU_WI+1].label = "";
		}

		 rc = optionally_enter(buf,-FOOTER_ROWS(ps),0,sizeof(buf),
					 tmp,ekey,help,&flags);
		 if(rc == 3)
		   help = help == NO_HELP ? h_config_whereis : NO_HELP;
		 else if(rc == 30){
		    if((p = get_prev_hist(history, buf, 0, NULL)) != NULL){
			strncpy(buf, p, sizeof(buf));
			buf[sizeof(buf)-1] = '\0';
		    }
		    else
		      Writechar(BELL, 0);

		    continue;
		 }
		 else if(rc == 31){
		    if((p = get_next_hist(history, buf, 0, NULL)) != NULL){
			strncpy(buf, p, sizeof(buf));
			buf[sizeof(buf)-1] = '\0';
		    }
		    else
		      Writechar(BELL, 0);

		    continue;
		 }
		 else if(rc == 0 || rc == 1 || rc == 10 || rc == 11 || !buf[0]){
		     if(rc == 0 && !buf[0] && last[0])
		       strncpy(buf, last, 64);

		     break;
		 }
	     }

	     screen->current->flags &= ~CF_VAR2;
	     if(rc == 0 && buf[0]){
		 CONF_S *started_here;

		 save_hist(history, buf, 0, NULL);

		 ch   = KEY_DOWN;
		 ctmpa = screen->current;
		 /*
		  * Skip over the unselectable lines of this "item"
		  * before starting search so that we don't find the
		  * same one again.
		  */
		 while((ctmpb = next_confline(ctmpa)) &&
		       (ctmpb->flags & CF_NOSELECT) &&
		       !(ctmpb->flags & CF_STARTITEM))
		   ctmpa = ctmpb;

		 started_here = next_confline(ctmpa);
		 while((ctmpa = next_confline(ctmpa)) != NULL)
		   if(srchstr(ctmpa->varname, buf)
		      || srchstr(ctmpa->value, buf)){

		       found = FOUND_IT;
		       /*
			* If this line is not selectable, back up to the
			* previous selectable line, but not past the
			* start of this "entry".
			*/
		       if(ctmpa->flags & CF_NOSELECT)
			 found |= FOUND_NOSELECT;

		       while((ctmpa->flags & CF_NOSELECT) &&
			     !(ctmpa->flags & CF_STARTITEM) &&
			     (ctmpb = prev_confline(ctmpa)))
			 ctmpa = ctmpb;
		       
		       /*
			* If that isn't selectable, better search forward
			* for something that is.
			*/
		       while((ctmpa->flags & CF_NOSELECT) &&
			     (ctmpb = next_confline(ctmpa))){
			   ctmpa = ctmpb;
			   found |= FOUND_ABOVE;
		       }

		       /*
			* If that still isn't selectable, better search
			* backwards for something that is.
			*/
		       while((ctmpa->flags & CF_NOSELECT) &&
			     (ctmpb = prev_confline(ctmpa))){
			   ctmpa = ctmpb;
			   found &= ~FOUND_ABOVE;
		       }

		       break;
		   }

		 if(!found){
		     found = FOUND_WRAPPED;
		     ctmpa = first_confline(screen->current);

		     while(ctmpa != started_here)
		       if(srchstr(ctmpa->varname, buf)
			  || srchstr(ctmpa->value, buf)){

			   found |= FOUND_IT;
			   if(ctmpa->flags & CF_NOSELECT)
			       found |= FOUND_NOSELECT;

			   while((ctmpa->flags & CF_NOSELECT) &&
				 !(ctmpa->flags & CF_STARTITEM) &&
				 (ctmpb = prev_confline(ctmpa)))
			     ctmpa = ctmpb;

			   while((ctmpa->flags & CF_NOSELECT) &&
				 (ctmpb = next_confline(ctmpa))){
			       ctmpa = ctmpb;
			       found |= FOUND_ABOVE;
			   }

			   if(ctmpa == screen->current)
			     found |= FOUND_CURRENT;

			   break;
		       }
		       else
			 ctmpa = next_confline(ctmpa);
		 }
	     }
	     else if(rc == 10){
		 screen->current = first_confline(screen->current);
		 if(screen->current && screen->current->flags & CF_NOSELECT){
		    for(ctmpa = next_confline(screen->current);
			ctmpa && (ctmpa->flags & CF_NOSELECT);
			ctmpa = next_confline(ctmpa))
		      ;
		    
		    if(ctmpa)
		      screen->current = ctmpa;
		 }

		 /* TRANSLATORS: Searched to ... is the result of the search, searched
		    to top means the search went past the bottom of the screen and
		    wrapped back around to the top. */
		 result = _("Searched to top");
	     }
	     else if(rc == 11){
		 screen->current = last_confline(screen->current);
		 if(screen->current && screen->current->flags & CF_NOSELECT){
		    for(ctmpa = prev_confline(screen->current);
			ctmpa && (ctmpa->flags & CF_NOSELECT);
			ctmpa = prev_confline(ctmpa))
		      ;
		    
		    if(ctmpa)
		      screen->current = ctmpa;
		 }
		 
		 result = _("Searched to bottom");
	     }
	     else
	       result = _("WhereIs cancelled");

	     if((found & FOUND_IT) && ctmpa){
		 strncpy(last, buf, 64);
		 result =
    (found & FOUND_CURRENT && found & FOUND_WRAPPED && found & FOUND_NOSELECT)
      ? _("Current item contains the only match")
      : (found & FOUND_CURRENT && found & FOUND_WRAPPED)
	? _("Current line contains the only match")
	: (found & FOUND_NOSELECT && found & FOUND_WRAPPED)
	  ? ((found & FOUND_ABOVE)
	       ? _("Search wrapped: word found in text above current line")
	       : _("Search wrapped: word found in text below current line"))
	  : (found & FOUND_WRAPPED)
	    ? _("Search wrapped to beginning: word found")
	    : (found & FOUND_NOSELECT)
	      ? ((found & FOUND_ABOVE)
		   ? _("Word found in text above current line")
		   : _("Word found in text below current line"))
	      : _("Word found");
		 screen->current = ctmpa;
	     }

	     q_status_message(SM_ORDER,0,3,result ? result : _("Word not found"));
	    }

	    break;

	  case MC_HOMEKEY:
	    screen->current = first_confline(screen->current);
	    if(screen->current && screen->current->flags & CF_NOSELECT){
		for(ctmpa = next_confline(screen->current);
		    ctmpa && (ctmpa->flags & CF_NOSELECT);
		    ctmpa = next_confline(ctmpa))
		  ;
		
		if(ctmpa)
		  screen->current = ctmpa;
	    }

	    q_status_message(SM_ORDER,0,3, _("Moved to top"));
	    break;

	  case MC_ENDKEY:
	    screen->current = last_confline(screen->current);
	    if(screen->current && screen->current->flags & CF_NOSELECT){
		for(ctmpa = prev_confline(screen->current);
		    ctmpa && (ctmpa->flags & CF_NOSELECT);
		    ctmpa = prev_confline(ctmpa))
		  ;
		
		if(ctmpa)
		  screen->current = ctmpa;
	    }

	    q_status_message(SM_ORDER,0,3, _("Moved to bottom"));
	    break;

	  case MC_REPAINT:			/* redraw the display */
	  case MC_RESIZE:
	    ClearScreen();
	    ps->mangled_screen = 1;
	    break;

	  default:
	    if(screen && screen->ro_warning){
		if(cmd == MC_EXIT){
		    retval = 0;
		    done++;
		}
		else
		  q_status_message1(SM_ORDER|SM_DING, 1, 3,
		     _("%s can't change options or settings"),
		     ps_global->restricted ? "Alpine demo"
					   : _("Config file not changeable,"));
	    }
	    else if(screen->current->tool){
		unsigned flags;

		flags  = screen->current->flags;
		flags |= (changes ? CF_CHANGES : 0);

		switch(i=(*screen->current->tool)(ps, cmd,
		    &screen->current, flags)){
		  case -1:
		    q_status_message2(SM_ORDER, 0, 2,
		      /* TRANSLATORS: Command <command letter> not defined here.
		         Leave the trailing %s which might be a parenthetical
			 remark. */
		      _("Command \"%s\" not defined here.%s"),
		      pretty_command(ch),
		      F_ON(F_BLANK_KEYMENU,ps) ? "" : "  See key menu below.");
		    break;

		  case 0:
		    break;

		  case 1:
		    changes = 1;
		    break;

		  case 2:
		    retval = changes;
		    done++;
		    break;

		  case 3:
		    retval = 1;
		    done++;
		    break;

		  default:
		    retval = i;
		    done++;
		    break;
		}
	    }

	    break;

	  case MC_UTF8:
	    bogus_utf8_command(utf8str, "?");
	    break;

	  case MC_NONE:				/* simple timeout */
	    break;
	}
    }

    screen->current = first_confline(screen->current);
    free_conflines(&screen->current);
    return(retval);
}


/*
 *
 */
void
config_scroll_up(long int n)
{
    CONF_S *ctmp = opt_screen->top_line;
    int     cur_found = 0;

    if(n < 0)
      config_scroll_down(-n);
    else if(n){
      for(; n>0 && ctmp->next; n--){
	ctmp = next_confline(ctmp);
	if(prev_confline(ctmp) == opt_screen->current)
	  cur_found++;
      }

      opt_screen->top_line = ctmp;
      ps_global->mangled_body = 1;
      if(cur_found){
	for(ctmp = opt_screen->top_line;
	    ctmp && (ctmp->flags & CF_NOSELECT);
	    ctmp = next_confline(ctmp))
	  ;

	if(ctmp)
	  opt_screen->current = opt_screen->prev = ctmp;
	else {
	  while(opt_screen->top_line->flags & CF_NOSELECT)
	    opt_screen->top_line = prev_confline(opt_screen->top_line);
	  opt_screen->current = opt_screen->prev = opt_screen->top_line;
	}
      }
    }
}


/*
 * config_scroll_down -
 */
void
config_scroll_down(long int n)
{
    CONF_S *ctmp = opt_screen->top_line, *last_sel = NULL;
    int     i;

    if(n < 0)
      config_scroll_up(-n);
    else if(n){
	for(; n>0 && ctmp->prev; n--)
	  ctmp = prev_confline(ctmp);

	opt_screen->top_line = ctmp;
	ps_global->mangled_body = 1;
	for(ctmp = opt_screen->top_line, i = BODY_LINES(ps_global);
	    i > 0 && ctmp && ctmp != opt_screen->current;
	    ctmp = next_confline(ctmp), i--)
	  if(!(ctmp->flags & CF_NOSELECT))
	    last_sel = ctmp;

	if(!i && last_sel)
	  opt_screen->current = opt_screen->prev = last_sel;
    }
}


/*
 * config_scroll_to_pos -
 */
void
config_scroll_to_pos(long int n)
{
    CONF_S *ctmp;

    for(ctmp = first_confline(opt_screen->current);
	n && ctmp && ctmp != opt_screen->top_line;
	ctmp = next_confline(ctmp), n--)
      ;

    if(n == 0)
      while(ctmp && ctmp != opt_screen->top_line)
	if((ctmp = next_confline(ctmp)) != NULL)
	  n--;

    config_scroll_up(n);
}


/*
 * config_top_scroll - return pointer to the 
 */
CONF_S *
config_top_scroll(struct pine *ps, CONF_S *topline)
{
    int     i;
    CONF_S *ctmp;

    for(ctmp = topline, i = HS_MARGIN(ps);
	ctmp && i;
	ctmp = next_confline(ctmp), i--)
      ;

    return(ctmp ? ctmp : topline);
}


int
text_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    return(text_toolit(ps, cmd, cl, flags, 0));
}


/*
 * simple text variable handler
 *
 * note, things get a little involved due to the
 *	 screen struct <--> variable mapping. (but, once its
 *       running it shouldn't need changing ;).
 *
 *    look_for_backslash == 1 means that backslash is an escape character.
 *                In particular, \, can be used to put a literal comma
 *                into a value. The value will still have the backslash
 *                in it, but the comma after the backslash won't be treated
 *                as an item separator.
 * 
 * returns:  -1 on unrecognized cmd, 0 if no change, 1 if change
 *           returns what conf_exit_cmd returns for exit command.
 */
int
text_toolit(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags, int look_for_backslash)
{
    char	     prompt[81], *sval, *tmp, *swap_val, **newval = NULL;
    char            *pval, **apval, **lval, ***alval;
    char            *olddefval = NULL;
    int		     rv = 0, skip_to_next = 0, after = 0, i = 4, j, k;
    int		     lowrange, hirange, incr, oeflags, oebufsize;
    int		     numval, repeat_key = 0;
    int              curindex, previndex, nextindex, deefault;
    HelpType         help;
    ESCKEY_S         ekey[6];

    if((*cl)->var->is_list){
	lval  = LVAL((*cl)->var, ew);
	alval = ALVAL((*cl)->var, ew);
    }
    else{
	pval  = PVAL((*cl)->var, ew);
	apval = APVAL((*cl)->var, ew);
    }

    oebufsize = 6*MAXPATH;
    sval = (char *) fs_get(oebufsize*sizeof(char));
    sval[0] = '\0';

    if(flags&CF_NUMBER){ /* only happens if !is_list */
	incr = 1;
	if((*cl)->var == &ps->vars[V_FILLCOL]){
	    lowrange = 1;
	    hirange  = MAX_FILLCOL;
	}
	else if((*cl)->var == &ps->vars[V_OVERLAP]
		|| (*cl)->var == &ps->vars[V_MARGIN]){
	    lowrange = 0;
	    hirange  = 20;
	}
	else if((*cl)->var == &ps->vars[V_QUOTE_SUPPRESSION]){
	    lowrange = -(Q_SUPP_LIMIT-1);
	    hirange  = 1000;
	}
	else if((*cl)->var == &ps->vars[V_MAXREMSTREAM]){
	    lowrange = 0;
	    hirange  = 15;
	}
	else if((*cl)->var == &ps->vars[V_STATUS_MSG_DELAY]){
	    lowrange = -10;
	    hirange  = 30;
	}
	else if((*cl)->var == &ps->vars[V_ACTIVE_MSG_INTERVAL]){
	    lowrange = 0;
	    hirange  = 20;
	}
	else if((*cl)->var == &ps->vars[V_MAILCHECK] ||
	        (*cl)->var == &ps->vars[V_INCCHECKINTERVAL] ||
	        (*cl)->var == &ps->vars[V_INC2NDCHECKINTERVAL] ||
		(*cl)->var == &ps->vars[V_MAILCHECKNONCURR]){
	    lowrange = 0;
	    hirange  = 25000;
	    incr     = 15;
	}
	else if((*cl)->var == &ps->vars[V_DEADLETS]){
	    lowrange = 0;
	    hirange  = 9;
	}
	else if((*cl)->var == &ps->vars[V_NMW_WIDTH]){
	    lowrange = 20;
	    hirange  = MAX_SCREEN_COLS;
	}
	else if((*cl)->var == score_act_global_ptr){
	    lowrange = -100;
	    hirange  = 100;
	}
	else if((*cl)->var == &ps->vars[V_TCPOPENTIMEO] ||
	        (*cl)->var == &ps->vars[V_TCPREADWARNTIMEO] ||
	        (*cl)->var == &ps->vars[V_TCPQUERYTIMEO]){
	    lowrange = 5;
	    hirange  = 1000;
	}
	else if((*cl)->var == &ps->vars[V_TCPWRITEWARNTIMEO] ||
	        (*cl)->var == &ps->vars[V_RSHOPENTIMEO] ||
	        (*cl)->var == &ps->vars[V_SSHOPENTIMEO] ||
	        (*cl)->var == &ps->vars[V_USERINPUTTIMEO]){
	    lowrange = 0;
	    hirange  = 1000;
	}
	else if((*cl)->var == &ps->vars[V_INCCHECKTIMEO]){
	    lowrange = 1;
	    hirange  = 1000;
	}
	else if((*cl)->var == &ps->vars[V_MAILDROPCHECK]){
	    lowrange = 0;
	    hirange  = 1000000;
	    incr     = 60;
	}
	else if((*cl)->var == &ps->vars[V_NNTPRANGE]){
	    lowrange = 0;
	    hirange  = 1000000;
	    incr     = 100;
	}
	else if((*cl)->var == &ps->vars[V_REMOTE_ABOOK_VALIDITY]){
	    lowrange = -1;
	    hirange  = 25000;
	}
	else if((*cl)->var == &ps->vars[V_REMOTE_ABOOK_HISTORY]){
	    lowrange = 0;
	    hirange  = 100;
	}
	else if((*cl)->var == cat_lim_global_ptr){
	    lowrange = -1;
	    hirange  = 10000000;
	}
	else{
	    lowrange = 0;
	    hirange  = 25000;
	}

	ekey[0].ch    = -2;
	ekey[0].rval  = 'x';
	ekey[0].name  = "";
	ekey[0].label = "";
	ekey[1].ch    = ctrl('P');
	ekey[1].rval  = ctrl('P');
	ekey[1].name  = "^P";
	ekey[1].label = N_("Decrease");
	ekey[2].ch    = ctrl('N');
	ekey[2].rval  = ctrl('N');
	ekey[2].name  = "^N";
	ekey[2].label = N_("Increase");
	ekey[3].ch    = KEY_DOWN;
	ekey[3].rval  = ctrl('P');
	ekey[3].name  = "";
	ekey[3].label = "";
	ekey[4].ch    = KEY_UP;
	ekey[4].rval  = ctrl('N');
	ekey[4].name  = "";
	ekey[4].label = "";
	ekey[5].ch    = -1;
    }

    switch(cmd){
      case MC_ADD:				/* add to list */
	if(fixed_var((*cl)->var, "add to", NULL)){
	    break;
	}
	else if(!(*cl)->var->is_list && pval){
	    q_status_message(SM_ORDER, 3, 3,
			    _("Only single value allowed.  Use \"Change\"."));
	}
	else{
	    int maxwidth;
	    char *p;

	    if((*cl)->var->is_list
	       && lval && lval[0] && lval[0][0]
	       && (*cl)->value){
		char tmpval[101];
		/* regular add to an existing list */

		strncpy(tmpval, (*cl)->value, sizeof(tmpval));
		tmpval[sizeof(tmpval)-1] = '\0';
		removing_trailing_white_space(tmpval);

		/* 33 is the number of chars other than the value */
		maxwidth = MIN(80, ps->ttyo->screen_cols) - 15;
		k = MIN(18, MAX(maxwidth-33,0));
		if(utf8_width(tmpval) > k && k >= 3){
		    (void) utf8_truncate(tmpval, k-3);
		    strncat(tmpval, "...", sizeof(tmpval)-strlen(tmpval)-1);
		    tmpval[sizeof(tmpval)-1] = '\0';
		}

		utf8_snprintf(prompt, sizeof(prompt),
		    _("Enter text to insert before \"%.*w\": "), k, tmpval);
		prompt[sizeof(prompt)-1] = '\0';
	    }
	    else if((*cl)->var->is_list
		    && !lval
		    && (*cl)->var->current_val.l){
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
		switch(radio_buttons(prompt, -FOOTER_ROWS(ps), ekey, 'a', 'x',
				     h_config_replace_add, RB_NORM)){
		  case 'a':
		    p = sval;
		    for(j = 0; (*cl)->var->current_val.l[j]; j++){
			sstrncpy(&p, (*cl)->var->current_val.l[j], oebufsize-(p-sval));
			if(oebufsize-(p-sval) > 2){
			    *p++ = ',';
			    *p++ = ' ';
			}

			if(oebufsize-(p-sval) > 0)
			  *p = '\0';
		    }

		    sval[oebufsize-1] = '\0';

add_text:
		    if(flags & CF_NUMBER)
		      snprintf(prompt, sizeof(prompt), _("Enter the numeric text to be added : "));
		    else
		      snprintf(prompt, sizeof(prompt), _("Enter the text to be added : "));

		    break;
		    
		  case 'r':
replace_text:
		    if(olddefval){
			strncpy(sval, olddefval, oebufsize);
			sval[oebufsize-1] = '\0';
		    }

		    if(flags & CF_NUMBER)
		      snprintf(prompt, sizeof(prompt), _("Enter the  numeric replacement text : "));
		    else
		      snprintf(prompt, sizeof(prompt), _("Enter the  replacement text : "));

		    break;
		    
		  case 'x':
		    i = 1;
		    cmd_cancelled("Add");
		    break;
		}
	    }
	    else{
		if(flags & CF_NUMBER)
		  snprintf(prompt, sizeof(prompt), _("Enter the numeric text to be added : "));
		else
		  snprintf(prompt, sizeof(prompt), _("Enter the text to be added : "));
	    }

	    prompt[sizeof(prompt)-1] = '\0';

	    ps->mangled_footer = 1;

	    if(i == 1)
	      break;

	    help = NO_HELP;
	    while(1){
		if((*cl)->var->is_list
		    && lval && lval[0] && lval[0][0]
		    && (*cl)->value){
		    ekey[0].ch    = ctrl('W');
		    ekey[0].rval  = 5;
		    ekey[0].name  = "^W";
		    /* TRANSLATORS: Insert new item before current item */
		    ekey[0].label = after ? N_("InsertBefore") : N_("InsertAfter");
		    ekey[1].ch    = -1;
		}
		else if(!(flags&CF_NUMBER))
		  ekey[0].ch    = -1;

		oeflags = OE_APPEND_CURRENT;
		i = optionally_enter(sval, -FOOTER_ROWS(ps), 0, oebufsize,
				     prompt,
				     (ekey[0].ch != -1) ? ekey : NULL,
				     help, &oeflags);
		if(i == 0){
		    rv = 1;
		    if((*cl)->var->is_list)
		      ps->mangled_body = 1;
		    else
		      ps->mangled_footer = 1;

		    removing_leading_and_trailing_white_space(sval);
		    /*
		     * Coerce "" and <Empty Value> to empty string input.
		     * Catch <No Value Set> as a substitute for deleting.
		     */
		    if((*sval == '\"' && *(sval+1) == '\"' && *(sval+2) == '\0')
		        || !struncmp(sval, _(empty_val), strlen(_(empty_val))) 
			|| (*sval == '<'
			    && !struncmp(sval+1, _(empty_val), strlen(_(empty_val)))))
		      *sval = '\0';
		    else if(!struncmp(sval, _(no_val), strlen(_(no_val)))
		        || (*sval == '<'
			    && !struncmp(sval+1, _(no_val), strlen(_(no_val)))))
		      goto delete;

		    if((*cl)->var->is_list){
			if(*sval || !lval){
			    char **ltmp;
			    int    i;

			    i = 0;
			    for(tmp = sval; *tmp; tmp++)
			      if(*tmp == ',')
				i++;	/* conservative count of ,'s */

			    if(!i){
				ltmp    = (char **)fs_get(2 * sizeof(char *));
				ltmp[0] = cpystr(sval);
				ltmp[1] = NULL;
			    }
			    else
			      ltmp = parse_list(sval, i + 1,
					        look_for_backslash
						  ? PL_COMMAQUOTE : 0,
						NULL);

			    if(ltmp[0]){
				config_add_list(ps, cl, ltmp, &newval, after);
				if(after)
				  skip_to_next = 1;
			    }
			    else{
				q_status_message1(SM_ORDER, 0, 3,
					 _("Can't add %s to list"), _(empty_val));
				rv = ps->mangled_body = 0;
			    }

			    fs_give((void **)&ltmp);
			}
			else{
			    q_status_message1(SM_ORDER, 0, 3,
					 _("Can't add %s to list"), _(empty_val));
			}
		    }
		    else{
			if(flags&CF_NUMBER && sval[0]
			  && !(isdigit((unsigned char)sval[0])
			       || sval[0] == '-' || sval[0] == '+')){
			    q_status_message(SM_ORDER,3,3,
				  _("Entry must be numeric"));
			    i = 3; /* to keep loop going */
			    continue;
			}

			if(apval && *apval)
			  fs_give((void **)apval);

			if(!(olddefval && !strcmp(sval, olddefval))
			   || ((*cl)->var == &ps->vars[V_POST_CHAR_SET])
			   || want_to(_("Leave unset and use default "),
				      'y', 'y', NO_HELP, WT_FLUSH_IN) == 'n')
			  *apval = cpystr(sval);

			newval = &(*cl)->value;
		    }
		}
		else if(i == 1){
		    cmd_cancelled("Add");
		}
		else if(i == 3){
		    help = help == NO_HELP ? h_config_add : NO_HELP;
		    continue;
		}
		else if(i == 4){		/* no redraw, yet */
		    continue;
		}
		else if(i == 5){ /* change from/to prepend to/from append */
		    char tmpval[101];

		    after = after ? 0 : 1;
		    strncpy(tmpval, (*cl)->value, sizeof(tmpval));
		    tmpval[sizeof(tmpval)-1] = '\0';
		    removing_trailing_white_space(tmpval);
		    /* 33 is the number of chars other than the value */
		    maxwidth = MIN(80, ps->ttyo->screen_cols) - 15;
		    k = MIN(18, MAX(maxwidth-33,0));
		    if(utf8_width(tmpval) > k && k >= 3){
			(void) utf8_truncate(tmpval, k-3);
			strncat(tmpval, "...", sizeof(tmpval)-strlen(tmpval)-1);
			tmpval[sizeof(tmpval)-1] = '\0';
		    }

		    if(after)
		      snprintf(prompt, sizeof(prompt), _("Enter text to insert after \"%.*s\": "), k, tmpval);
		    else
		      snprintf(prompt, sizeof(prompt), _("Enter text to insert before \"%.*s\": "), k, tmpval);

		    continue;
		}
		else if(i == ctrl('P')){
		    if(sval[0])
		      numval = atoi(sval);
		    else{
		      if(pval)
			numval = atoi(pval);
		      else
			numval = lowrange + 1;
		    }

		    if(numval == lowrange){
			/*
			 * Protect user from repeating arrow key that
			 * causes message to appear over and over.
			 */
			if(++repeat_key > 0){
			    q_status_message1(SM_ORDER,3,3,
				_("Minimum value is %s"), comatose(lowrange));
			    repeat_key = -5;
			}
		    }
		    else
		      repeat_key = 0;

		    numval = MAX(numval - incr, lowrange);
		    snprintf(sval, oebufsize, "%d", numval);
		    sval[oebufsize-1] = '\0';
		    continue;
		}
		else if(i == ctrl('N')){
		    if(sval[0])
		      numval = atoi(sval);
		    else{
		      if(pval)
			numval = atoi(pval);
		      else
			numval = lowrange + 1;
		    }

		    if(numval == hirange){
			if(++repeat_key > 0){
			    q_status_message1(SM_ORDER,3,3,
				_("Maximum value is %s"), comatose(hirange));
			    repeat_key = -5;
			}
		    }
		    else
		      repeat_key = 0;

		    numval = MIN(numval + incr, hirange);
		    snprintf(sval, oebufsize, "%d", numval);
		    sval[oebufsize-1] = '\0';
		    continue;
		}

		break;
	    }
	}

	break;

      case MC_DELETE:				/* delete */
delete:
	if(!(*cl)->var->is_list
	    && apval && !*apval
	    && (*cl)->var->current_val.p){
	    char pmt[80];

	    snprintf(pmt, sizeof(pmt), _("Override default with %s"), _(empty_val2));
	    pmt[sizeof(pmt)-1] = '\0';
	    if(want_to(pmt, 'n', 'n', NO_HELP, WT_FLUSH_IN) == 'y'){
		sval[0] = '\0';
		*apval = cpystr(sval);
		newval = &(*cl)->value;
		rv = ps->mangled_footer = 1;
	    }
	}
	else if((*cl)->var->is_list
		&& alval && !lval
		&& (*cl)->var->current_val.l){
	    char pmt[80];

	    snprintf(pmt, sizeof(pmt), _("Override default with %s"), _(empty_val2));
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
	else if(((*cl)->var->is_list && !lval)
		|| (!(*cl)->var->is_list && !pval)){
	    q_status_message(SM_ORDER, 0, 3, _("No set value to delete"));
	}
	else{
	    if((*cl)->var->is_fixed)
	        snprintf(prompt, sizeof(prompt), _("Delete (unused) %s from %s "),
		    (*cl)->var->is_list
		      ? (!*lval[(*cl)->varmem])
			  ? _(empty_val2)
			  : lval[(*cl)->varmem]
		      : (pval)
			  ? (!*pval)
			      ? _(empty_val2)
			      : pval
		 	  : "<NULL VALUE>",
		    (*cl)->var->name);
	    else
	        snprintf(prompt, sizeof(prompt), _("Really delete %s%s from %s "),
		    (*cl)->var->is_list ? "item " : "", 
		    (*cl)->var->is_list
		      ? int2string((*cl)->varmem + 1)
		      : (pval)
			  ? (!*pval)
			      ? _(empty_val2)
			      : pval
		 	  : "<NULL VALUE>",
		    (*cl)->var->name);

	    prompt[sizeof(prompt)-1] = '\0';


	    ps->mangled_footer = 1;
	    if(want_to(prompt, 'n', 'n', NO_HELP, WT_FLUSH_IN) == 'y'){
		rv = 1;
		if((*cl)->var->is_list)
		  ps->mangled_body = 1;
		else
		  ps->mangled_footer = 1;

		if((*cl)->var->is_list){
		    if(lval[(*cl)->varmem])
		      fs_give((void **)&lval[(*cl)->varmem]);

		    config_del_list_item(cl, &newval);
		}
		else{
		    if(apval && *apval)
		      fs_give((void **)apval);

		    newval = &(*cl)->value;
		}
	    }
	    else
	      q_status_message(SM_ORDER, 0, 3, _("Value not deleted"));
	}

	break;

      case MC_EDIT:				/* edit/change list option */
	if(fixed_var((*cl)->var, NULL, NULL)){
	    break;
	}
	else if(((*cl)->var->is_list
		    && !lval
		    && (*cl)->var->current_val.l)
		||
		(!(*cl)->var->is_list
		    && !pval
		    && (*cl)->var->current_val.p)){

	    /*
	     * In non-list case, offer default value for editing.
	     */
	    if(!(*cl)->var->is_list
	       && (*cl)->var != &ps->vars[V_REPLY_INTRO]
	       && (*cl)->var->current_val.p[0]
	       && strcmp(VSTRING,(*cl)->var->current_val.p)){
		int quote_it;
		size_t len;

		olddefval = (char *) fs_get(strlen((*cl)->var->current_val.p)+3);

		if(!strncmp((*cl)->var->current_val.p,
			    DSTRING,
			    (len=strlen(DSTRING)))){
		    /* strip DSTRING and trailing paren */
		    strncpy(olddefval, (*cl)->var->current_val.p+len,
			    strlen((*cl)->var->current_val.p)-len-1);
		    olddefval[strlen((*cl)->var->current_val.p)-len-1] = '\0';
		}
		else{
		    /* quote it if there are trailing spaces */
		    quote_it = ((*cl)->var->current_val.p[strlen((*cl)->var->current_val.p)-1] == SPACE);
		    snprintf(olddefval, strlen((*cl)->var->current_val.p)+3, "%s%s%s", quote_it ? "\"" : "", (*cl)->var->current_val.p, quote_it ? "\"" : "");
		}

		olddefval[strlen((*cl)->var->current_val.p)+3-1] = '\0';
	    }

	    goto replace_text;
	}
	else if(((*cl)->var->is_list
		    && !lval
		    && !(*cl)->var->current_val.l)
		||
		(!(*cl)->var->is_list
		    && !pval
		    && !(*cl)->var->current_val.p)){
	    goto add_text;
	}
	else{
	    HelpType help;
	    char *clptr;

	    if(sval)
	      fs_give((void **)&sval);
	    if((*cl)->var->is_list){
		snprintf(prompt, sizeof(prompt), _("Change field %s list entry : "),
			(*cl)->var->name);
		prompt[sizeof(prompt)-1] = '\0';
 		clptr = lval[(*cl)->varmem] ? lval[(*cl)->varmem] : NULL;
	    }
	    else{
		if(flags & CF_NUMBER)
		  snprintf(prompt, sizeof(prompt), _("Change numeric field %s value : "), (*cl)->var->name);
		else
		  snprintf(prompt, sizeof(prompt), _("Change field %s value : "), (*cl)->var->name);

 		clptr = pval ? pval : NULL;
	    }

 	    oebufsize = clptr ? (int) MAX(MAXPATH, 50+strlen(clptr)) : MAXPATH;
 	    sval = (char *) fs_get(oebufsize * sizeof(char));
 	    snprintf(sval, oebufsize, "%s", clptr ? clptr : "");
	    sval[oebufsize-1] = '\0';

	    ps->mangled_footer = 1;
	    help = NO_HELP;
	    while(1){
		if(!(flags&CF_NUMBER))
		  ekey[0].ch = -1;

		oeflags = OE_APPEND_CURRENT;
		i = optionally_enter(sval, -FOOTER_ROWS(ps), 0, oebufsize,
				     prompt,
				     (ekey[0].ch != -1) ? ekey : NULL,
				     help, &oeflags);
		if(i == 0){
		    removing_leading_and_trailing_white_space(sval);
		    /*
		     * Coerce "" and <Empty Value> to empty string input.
		     * Catch <No Value Set> as a substitute for deleting.
		     */
		    if((*sval == '\"' && *(sval+1) == '\"' && *(sval+2) == '\0')
		        || !struncmp(sval, _(empty_val), strlen(_(empty_val))) 
			|| (*sval == '<'
			    && !struncmp(sval+1, _(empty_val), strlen(_(empty_val)))))
		      *sval = '\0';
		    else if(!struncmp(sval, _(no_val), strlen(_(no_val)))
			|| (*sval == '<'
			    && !struncmp(sval+1, _(no_val), strlen(_(no_val)))))
		      goto delete;

		    rv = 1;
		    if((*cl)->var->is_list)
		      ps->mangled_body = 1;
		    else
		      ps->mangled_footer = 1;

		    if((*cl)->var->is_list){
			char **ltmp = NULL;
			int    i;

			if(lval[(*cl)->varmem])
			  fs_give((void **)&lval[(*cl)->varmem]);

			i = 0;
			for(tmp = sval; *tmp; tmp++)
			  if(*tmp == ',')
			    i++;	/* conservative count of ,'s */

			if(i)
			  ltmp = parse_list(sval, i + 1,
					    look_for_backslash
					      ? PL_COMMAQUOTE : 0,
					    NULL);

			if(ltmp && !ltmp[0])		/* only commas */
			  goto delete;
			else if(!i || (ltmp && !ltmp[1])){  /* only one item */
			    lval[(*cl)->varmem] = cpystr(sval);
			    newval = &(*cl)->value;

			    if(ltmp && ltmp[0])
			      fs_give((void **)&ltmp[0]);
			}
			else if(ltmp){
			    /*
			     * Looks like the value was changed to a 
			     * list, so delete old value, and insert
			     * new list...
			     *
			     * If more than one item in existing list and
			     * current is end of existing list, then we
			     * have to delete and append instead of
			     * deleting and prepending.
			     */
			    if(((*cl)->varmem > 0 || lval[1])
			       && !(lval[(*cl)->varmem+1])){
				after = 1;
				skip_to_next = 1;
			    }

			    config_del_list_item(cl, &newval);
			    config_add_list(ps, cl, ltmp, &newval, after);
			}

			if(ltmp)
			  fs_give((void **)&ltmp);
		    }
		    else{
			if(flags&CF_NUMBER && sval[0]
			  && !(isdigit((unsigned char)sval[0])
			       || sval[0] == '-' || sval[0] == '+')){
			    q_status_message(SM_ORDER,3,3,
				  _("Entry must be numeric"));
			    continue;
			}

			if(apval && *apval)
			  fs_give((void **)apval);

			if(sval[0] && apval)
			  *apval = cpystr(sval);

			newval = &(*cl)->value;
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
		else if(i == ctrl('P')){
		    numval = atoi(sval);
		    if(numval == lowrange){
			/*
			 * Protect user from repeating arrow key that
			 * causes message to appear over and over.
			 */
			if(++repeat_key > 0){
			    q_status_message1(SM_ORDER,3,3,
				_("Minimum value is %s"), comatose(lowrange));
			    repeat_key = -5;
			}
		    }
		    else
		      repeat_key = 0;

		    numval = MAX(numval - incr, lowrange);
		    snprintf(sval, oebufsize, "%d", numval);
		    sval[oebufsize-1] = '\0';
		    continue;
		}
		else if(i == ctrl('N')){
		    numval = atoi(sval);
		    if(numval == hirange){
			if(++repeat_key > 0){
			    q_status_message1(SM_ORDER,3,3,
				_("Maximum value is %s"), comatose(hirange));
			    repeat_key = -5;
			}
		    }
		    else
		      repeat_key = 0;

		    numval = MIN(numval + incr, hirange);
		    snprintf(sval, oebufsize, "%d", numval);
		    sval[oebufsize-1] = '\0';
		    continue;
		}

		break;
	    }
	}

	break;

      case MC_SHUFFLE:
	if(!((*cl)->var && (*cl)->var->is_list)){
	    q_status_message(SM_ORDER, 0, 2,
			     _("Can't shuffle single-valued setting"));
	    break;
	}

	if(!alval)
	  break;

	curindex = (*cl)->varmem;
	previndex = curindex-1;
	nextindex = curindex+1;
	if(!*alval || !(*alval)[nextindex])
	  nextindex = -1;

	if((previndex < 0 && nextindex < 0) || !*alval){
	    q_status_message(SM_ORDER, 0, 3,
   _("Shuffle only makes sense when there is more than one value defined"));
	    break;
	}

	/* Move it up or down? */
	i = 0;
	ekey[i].ch      = 'u';
	ekey[i].rval    = 'u';
	ekey[i].name    = "U";
	ekey[i++].label = N_("Up");

	ekey[i].ch      = 'd';
	ekey[i].rval    = 'd';
	ekey[i].name    = "D";
	ekey[i++].label = N_("Down");

	ekey[i].ch = -1;
	deefault = 'u';

	if(previndex < 0){		/* no up */
	    ekey[0].ch = -2;
	    deefault = 'd';
	}
	else if(nextindex < 0)
	  ekey[1].ch = -2;	/* no down */

	snprintf(prompt, sizeof(prompt), "Shuffle %s%s%s ? ",
		(ekey[0].ch != -2) ? "UP" : "",
		(ekey[0].ch != -2 && ekey[1].ch != -2) ? " or " : "",
		(ekey[1].ch != -2) ? "DOWN" : "");
	help = (ekey[0].ch == -2) ? h_hdrcolor_shuf_down
				  : (ekey[1].ch == -2) ? h_hdrcolor_shuf_up
						       : h_hdrcolor_shuf;
	prompt[sizeof(prompt)-1] = '\0';

	i = radio_buttons(prompt, -FOOTER_ROWS(ps), ekey, deefault, 'x',
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
	    swap_val = (*alval)[curindex];
	    (*alval)[curindex] = (*alval)[nextindex];
	    (*alval)[nextindex] = swap_val;
	}
	else if(i == 'u'){
	    swap_val = (*alval)[curindex];
	    (*alval)[curindex] = (*alval)[previndex];
	    (*alval)[previndex] = swap_val;
	}
	else		/* can't happen */
	  break;

	/*
	 * Fix the conf line values.
	 */

	if((*cl)->value)
	  fs_give((void **)&(*cl)->value);

	(*cl)->value = pretty_value(ps, *cl);

	if(i == 'd'){
	    if((*cl)->next->value)
	      fs_give((void **)&(*cl)->next->value);

	    (*cl)->next->value = pretty_value(ps, (*cl)->next);
	    *cl = next_confline(*cl);
	}
	else{
	    if((*cl)->prev->value)
	      fs_give((void **)&(*cl)->prev->value);

	    (*cl)->prev->value = pretty_value(ps, (*cl)->prev);
	    *cl = prev_confline(*cl);
	}

	rv = ps->mangled_body = 1;
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
     *
     * NOTE: we don't worry about the "fixed variable" case here, because
     *       editing such vars should have been prevented above...
     */
    if(rv == 1){
	/*
	 * Now go and set the current_val based on user_val changes
	 * above.  Turn off command line settings...
	 */
	set_current_val((*cl)->var, TRUE, FALSE);
	fix_side_effects(ps, (*cl)->var, 0);

	/*
	 * Delay setting the displayed value until "var.current_val" is set
	 * in case current val get's changed due to a special case above.
	 */
	if(newval){
	    if(*newval)
	      fs_give((void **) newval);

	    *newval = pretty_value(ps, *cl);
	}

	exception_override_warning((*cl)->var);
    }

    if(sval)
      fs_give((void **) &sval);
 
    if(olddefval)
      fs_give((void **) &olddefval);
      
    return(rv);
}


int
config_exit_cmd(unsigned int flags)
{
    return(screen_exit_cmd(flags, "Configuration"));
}


int
simple_exit_cmd(unsigned int flags)
{
    return(2);
}


/*
 * screen_exit_cmd - basic config/flag screen exit logic
 */
int
screen_exit_cmd(unsigned int flags, char *cmd)
{
    if(flags & CF_CHANGES){
      switch(want_to(EXIT_PMT, 'y', 'x', h_config_undo, WT_FLUSH_IN)){
	case 'y':
	  q_status_message1(SM_ORDER,0,3,"%s changes saved", cmd);
	  return(2);

	case 'n':
	  q_status_message1(SM_ORDER,3,5,"No %s changes saved", cmd);
	  return(10);

	case 'x':  /* ^C */
	default :
	  q_status_message(SM_ORDER,3,5,"Changes not yet saved");
	  return(0);
      }
    }
    else
      return(2);
}


/*
 *
 */
void
config_add_list(struct pine *ps, CONF_S **cl, char **ltmp, char ***newval, int after)
{
    int	    items, i;
    char   *tmp, ***alval;
    CONF_S *ctmp;

    for(items = 0, i = 0; ltmp[i]; i++)		/* count list items */
      items++;

    alval = ALVAL((*cl)->var, ew);

    if(alval && (*alval)){
	if((*alval)[0] && (*alval)[0][0]){
	    /*
	     * Since we were already a list, make room
	     * for the new member[s] and fall thru to
	     * actually fill them in below...
	     */
	    for(i = 0; (*alval)[i]; i++)
	      ;

	    fs_resize((void **)alval, (i + items + 1) * sizeof(char *));

	    /*
	     * move the ones that will be bumped down to the bottom of the list
	     */
	    for(; i >= (*cl)->varmem + (after?1:0); i--)
	      (*alval)[i+items] = (*alval)[i];

	    i = 0;
	}
	else if(alval){
	    (*cl)->varmem = 0;
	    if(*alval)
	      free_list_array(alval);

	    *alval = (char **)fs_get((items+1)*sizeof(char *));
	    memset((void *)(*alval), 0, (items+1)*sizeof(char *));
	    (*alval)[0] = ltmp[0];
	    if(newval)
	      *newval = &(*cl)->value;

	    if((*cl)->value)
	      fs_give((void **)&(*cl)->value);

	    i = 1;
	}
    }
    else if(alval){
	/*
	 * since we were previously empty, we want
	 * to replace the first CONF_S's value with
	 * the first new value, and fill the other
	 * in below if there's a list...
	 *
	 * first, make sure we're at the beginning of this config
	 * section and dump the config lines for the default list,
	 * except for the first one, which we will over-write.
	 */
	*cl = (*cl)->varnamep; 
	while((*cl)->next && (*cl)->next->varnamep == (*cl)->varnamep)
	  snip_confline(&(*cl)->next);

	/*
	 * now allocate the new user_val array and fill in the first entry.
	 */
	*alval = (char **)fs_get((items+1)*sizeof(char *));
	memset((void *)(*alval), 0, (items+1) * sizeof(char *));
	(*alval)[(*cl)->varmem=0] = ltmp[0];
	if(newval)
	  *newval = &(*cl)->value;

	if((*cl)->value)
	  fs_give((void **)&(*cl)->value);

	i = 1;
    }

    /*
     * Make new cl's to fit in the new space.  Move the value from the current
     * line if inserting before it, else leave it where it is.
     */
    for(; i < items ; i++){
	(*alval)[i+(*cl)->varmem + (after?1:0)] = ltmp[i];
	tmp = (*cl)->value;
	new_confline(cl);
	if(after)
	  (*cl)->value   = NULL;
	else
	  (*cl)->value   = tmp;

	(*cl)->var       = (*cl)->prev->var;
	(*cl)->valoffset = (*cl)->prev->valoffset;
	(*cl)->varoffset = (*cl)->prev->varoffset;
	(*cl)->headingp  = (*cl)->prev->headingp;
	(*cl)->keymenu   = (*cl)->prev->keymenu;
	(*cl)->help      = (*cl)->prev->help;
	(*cl)->tool      = (*cl)->prev->tool;
	(*cl)->varnamep  = (*cl)->prev->varnamep;
	*cl		 = (*cl)->prev;
	if(!after)
	  (*cl)->value   = NULL;

	if(newval){
	    if(after)
	      *newval	 = &(*cl)->next->value;
	    else
	      *newval	 = &(*cl)->value;
	}
    }

    /*
     * now fix up varmem values and fill in new values that have been
     * left NULL
     */
    for(ctmp = (*cl)->varnamep, i = 0;
	(*alval)[i];
	ctmp = ctmp->next, i++){
	ctmp->varmem = i;
	if(!ctmp->value){
	    /* BUG:  We should be able to do this without the temp
	     * copy...  
	     */
	    char *ptmp = pretty_value(ps, ctmp);
	    ctmp->value = (ctmp->varnamep->flags & CF_PRINTER) ? printer_name(ptmp) : cpystr(ptmp);
	    fs_give((void **)&ptmp);
	}
    }
}


/*
 *
 */
void
config_del_list_item(CONF_S **cl, char ***newval)
{
    char   **bufp, ***alval;
    int	     i;
    CONF_S  *ctmp;

    alval = ALVAL((*cl)->var, ew);

    if((*alval)[(*cl)->varmem + 1]){
	for(bufp = &(*alval)[(*cl)->varmem];
	    (*bufp = *(bufp+1)) != NULL; bufp++)
	  ;

	if(*cl == (*cl)->varnamep){		/* leading value */
	    if((*cl)->value)
	      fs_give((void **)&(*cl)->value);

	    ctmp = (*cl)->next;
	    (*cl)->value = ctmp->value;
	    ctmp->value  = NULL;
	}
	else{
	    ctmp = *cl;			/* blast the confline */
	    *cl = (*cl)->next;
	    if(ctmp == opt_screen->top_line)
	      opt_screen->top_line = *cl;
	}

	snip_confline(&ctmp);

	for(ctmp = (*cl)->varnamep, i = 0;	/* now fix up varmem values */
	    (*alval)[i];
	    ctmp = ctmp->next, i++)
	  ctmp->varmem = i;
    }
    else if((*cl)->varmem){			/* blasted last in list */
	ctmp = *cl;
	*cl = (*cl)->prev;
	if(ctmp == opt_screen->top_line)
	  opt_screen->top_line = *cl;

	snip_confline(&ctmp);
    }
    else{					/* blasted last remaining */
	if(alval && *alval)
	  fs_give((void **)alval);

	*newval = &(*cl)->value;
    }
}


/*
 * feature list manipulation tool
 * 
 * 
 * returns:  -1 on unrecognized cmd, 0 if no change, 1 if change
 */
int
checkbox_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int  rv = 0;

    switch(cmd){
      case MC_TOGGLE:				/* mark/unmark feature */
	if((*cl)->var == &ps->vars[V_FEATURE_LIST]){
	    rv = 1;
	    toggle_feature_bit(ps, (*cl)->varmem, (*cl)->var, *cl, 0);
	}
	else
	  q_status_message(SM_ORDER | SM_DING, 3, 6,
			   "Programmer botch!  Unknown checkbox type.");

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


/*
 * simple radio-button style variable handler
 */
int
radiobutton_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    char     **apval;
    int	       rv = 0;
    NAMEVAL_S *rule = NULL;
#ifndef	_WINDOWS
    int        old_uc, old_cs;
    CONF_S    *ctmp;
#endif

    apval = APVAL((*cl)->var, ew);

    switch(cmd){
      case MC_CHOICE :				/* set/unset feature */

	if(fixed_var((*cl)->var, NULL, NULL)){
	    if(((*cl)->var->post_user_val.p || (*cl)->var->main_user_val.p)
	       && want_to(_("Delete old unused personal option setting"),
			  'y', 'n', NO_HELP, WT_FLUSH_IN) == 'y'){
		delete_user_vals((*cl)->var);
		q_status_message(SM_ORDER, 0, 3, _("Deleted"));
		rv = 1;
	    }

	    return(rv);
	}

	if(standard_radio_var(ps, (*cl)->var) || (*cl)->var == startup_ptr){
	    PTR_TO_RULEFUNC rulefunc;

#ifndef	_WINDOWS
	    if((*cl)->var == &ps->vars[V_COLOR_STYLE]){
		old_uc = pico_usingcolor();
		old_cs = ps->color_style;
	    }
#endif

	    if((*cl)->var->cmdline_val.p)
	      fs_give((void **)&(*cl)->var->cmdline_val.p);

	    if(apval && *apval)
	      fs_give((void **)apval);

	    rulefunc = rulefunc_from_var(ps, (*cl)->var);
	    if(rulefunc)
	      rule = (*rulefunc)((*cl)->varmem);

	    if(apval && rule)
	      *apval = cpystr(S_OR_L(rule));

	    cur_rule_value((*cl)->var, TRUE, TRUE);
	    set_radio_pretty_vals(ps, cl);

	    if((*cl)->var == &ps->vars[V_AB_SORT_RULE])
	      addrbook_redo_sorts();
	    else if((*cl)->var == &ps->vars[V_THREAD_DISP_STYLE]){
		clear_index_cache(ps->mail_stream, 0);
	    }
	    else if((*cl)->var == &ps->vars[V_THREAD_INDEX_STYLE]){
		MAILSTREAM *m;
		int         i;

		clear_index_cache(ps->mail_stream, 0);
		/* clear all hidden and collapsed flags */
		set_lflags(ps->mail_stream, ps->msgmap, MN_COLL | MN_CHID, 0);

		if(SEP_THRDINDX()
		   && SORT_IS_THREADED(ps->msgmap)
		   && unview_thread(ps, ps->mail_stream, ps->msgmap)){
		    ps->next_screen = mail_index_screen;
		    ps->view_skipped_index = 0;
		    ps->mangled_screen = 1;
		}

		if(SORT_IS_THREADED(ps->msgmap)
		   && (SEP_THRDINDX() || COLL_THRDS()))
		  collapse_threads(ps->mail_stream, ps->msgmap, NULL);

		for(i = 0; i < ps_global->s_pool.nstream; i++){
		    m = ps_global->s_pool.streams[i];
		    if(m)
		      sp_set_viewing_a_thread(m, 0);
		}

		adjust_cur_to_visible(ps->mail_stream, ps->msgmap);
	    }
#ifndef	_WINDOWS
	    else if((*cl)->var == &ps->vars[V_COLOR_STYLE]){
		if(old_cs != ps->color_style){
		    pico_toggle_color(0);
		    switch(ps->color_style){
		      case COL_NONE:
		      case COL_TERMDEF:
			pico_set_color_options(pico_trans_color() ? COLOR_TRANS_OPT : 0);
			break;
		      case COL_ANSI8:
			pico_set_color_options(COLOR_ANSI8_OPT|COLOR_TRANS_OPT);
			break;
		      case COL_ANSI16:
			pico_set_color_options(COLOR_ANSI16_OPT|COLOR_TRANS_OPT);
			break;
		      case COL_ANSI256:
			pico_set_color_options(COLOR_ANSI256_OPT|COLOR_TRANS_OPT);
			break;
		    }

		    if(ps->color_style != COL_NONE)
		      pico_toggle_color(1);
		}
	    
		if(pico_usingcolor())
		  pico_set_normal_color();

		if(!old_uc && pico_usingcolor()){

		    /*
		     * remove the explanatory warning line and a blank line
		     */

		    /* first find the first blank line */
		    for(ctmp = *cl; ctmp; ctmp = next_confline(ctmp))
		      if(ctmp->flags & CF_NOSELECT)
			break;

		    if(ctmp && ctmp->flags & CF_NOSELECT &&
		       ctmp->prev && !(ctmp->prev->flags & CF_NOSELECT) &&
		       ctmp->next && ctmp->next->flags & CF_NOSELECT &&
		       ctmp->next->next &&
		       ctmp->next->next->flags & CF_NOSELECT){
			ctmp->prev->next = ctmp->next->next;
			ctmp->next->next->prev = ctmp->prev;
			ctmp->next->next = NULL;
			free_conflines(&ctmp);
		    }

		    /* make all the colors selectable */
		    for(ctmp = *cl; ctmp; ctmp = next_confline(ctmp))
		      if(ctmp->flags & CF_POT_SLCTBL)
			ctmp->flags &= ~CF_NOSELECT;
		}
		else if(old_uc && !pico_usingcolor()){

		    /*
		     * add the explanatory warning line and a blank line
		     */

		    /* first find the existing blank line */
		    for(ctmp = *cl; ctmp; ctmp = next_confline(ctmp))
		      if(ctmp->flags & CF_NOSELECT)
			break;

		    /* add the explanatory warning line */
		    new_confline(&ctmp);
		    ctmp->help   = NO_HELP;
		    ctmp->flags |= CF_NOSELECT;
		    ctmp->value  = cpystr(COLORNOSET);

		    /* and add another blank line */
		    new_confline(&ctmp);
		    ctmp->flags |= (CF_NOSELECT | CF_B_LINE);

		    /* make all the colors non-selectable */
		    for(ctmp = *cl; ctmp; ctmp = next_confline(ctmp))
		      if(ctmp->flags & CF_POT_SLCTBL)
			ctmp->flags |= CF_NOSELECT;
		}

		clear_index_cache(ps->mail_stream, 0);
		ClearScreen();
		ps->mangled_screen = 1;
	    }
#endif

	    ps->mangled_body = 1;	/* BUG: redraw it all for now? */
	    rv = 1;
	}
	else if((*cl)->var == &ps->vars[V_SORT_KEY]){
	    SortOrder def_sort;
	    int       def_sort_rev;

	    def_sort_rev  = (*cl)->varmem >= (short) EndofList;
	    def_sort      = (SortOrder) ((*cl)->varmem - (def_sort_rev
								 * EndofList));
	    snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%s%s", sort_name(def_sort),
		    (def_sort_rev) ? "/Reverse" : "");

	    if((*cl)->var->cmdline_val.p)
	      fs_give((void **)&(*cl)->var->cmdline_val.p);

	    if(apval){
		if(*apval)
		  fs_give((void **)apval);

		*apval = cpystr(tmp_20k_buf);
	    }

	    set_current_val((*cl)->var, TRUE, TRUE);
	    if(decode_sort(ps->VAR_SORT_KEY, &def_sort, &def_sort_rev) != -1){
		ps->def_sort     = def_sort;
		ps->def_sort_rev = def_sort_rev;
	    }

	    set_radio_pretty_vals(ps, cl);
	    ps->mangled_body = 1;	/* BUG: redraw it all for now? */
	    rv = 1;
	}
	else
	  q_status_message(SM_ORDER | SM_DING, 3, 6,
			   "Programmer botch!  Unknown radiobutton type.");

	break;

      case MC_EXIT:				/* exit */
	rv = config_exit_cmd(flags);
	break;

      default :
	rv = -1;
	break;
    }

    if(rv == 1)
      exception_override_warning((*cl)->var);
      
    return(rv);
}


/*
 * simple yes/no style variable handler
 */
int
yesno_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    int   rv = 0, yes = 0;
    char *pval, **apval;

    pval =  PVAL((*cl)->var, ew);
    apval = APVAL((*cl)->var, ew);

    switch(cmd){
      case MC_TOGGLE:				/* toggle yes to no and back */
	if(fixed_var((*cl)->var, NULL, NULL)){
	    if(((*cl)->var->post_user_val.p || (*cl)->var->main_user_val.p)
	       && want_to(_("Delete old unused personal option setting"),
			  'y', 'n', NO_HELP, WT_FLUSH_IN) == 'y'){
		delete_user_vals((*cl)->var);
		q_status_message(SM_ORDER, 0, 3, _("Deleted"));
		rv = 1;
	    }

	    return(rv);
	}

	rv = 1;
	yes = ((pval && !strucmp(pval, yesstr)) ||
	       (!pval && (*cl)->var->current_val.p &&
	        !strucmp((*cl)->var->current_val.p, yesstr)));
	fs_give((void **)&(*cl)->value);

	if(apval){
	    if(*apval)
	      fs_give((void **)apval);

	    if(yes)
	      *apval = cpystr(nostr);
	    else
	      *apval = cpystr(yesstr);
	}

	set_current_val((*cl)->var, FALSE, FALSE);
	if((*cl)->value)
	  fs_give((void **)&(*cl)->value);

	(*cl)->value = pretty_value(ps, *cl);
	fix_side_effects(ps, (*cl)->var, 0);

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


/*
 * Manage display of the config/options menu body.
 */
void
update_option_screen(struct pine *ps, OPT_SCREEN_S *screen, Pos *cursor_pos)
{
    int		   dline, w, save = '\0';
    CONF_S	  *top_line, *ctmp;
    char          *value;
    unsigned       got_width;
    int            want_width, first_width;
    char          *saveptr = NULL;

#ifdef _WINDOWS
    int		   last_selectable;
    mswin_beginupdate();
#endif
    if(cursor_pos){
	cursor_pos->col = 0;
	cursor_pos->row = -1;		/* to tell us if we've set it yet */
    }

    /*
     * calculate top line of display for reframing if the current field
     * is off the display defined by screen->top_line...
     */
    if((ctmp = screen->top_line) != NULL)
      for(dline = BODY_LINES(ps);
	  dline && ctmp && ctmp != screen->current;
	  ctmp = next_confline(ctmp), dline--)
	;

    if(!ctmp || !dline){		/* force reframing */
	dline = 0;
	ctmp = top_line = first_confline(screen->current);
	do
	  if(((dline++)%BODY_LINES(ps)) == 0)
	    top_line = ctmp;
	while(ctmp != screen->current && (ctmp = next_confline(ctmp)));
    }
    else
      top_line = screen->top_line;

#ifdef _WINDOWS
    /*
     * Figure out how far down the top line is from the top and how many
     * total lines there are.  Dumb to loop every time thru, but
     * there aren't that many lines, and it's cheaper than rewriting things
     * to maintain a line count in each structure...
     */
    for(dline = 0, ctmp = prev_confline(top_line); ctmp; ctmp = prev_confline(ctmp))
      dline++;

    scroll_setpos(dline);
    last_selectable = dline;
    for(ctmp = next_confline(top_line); ctmp ; ctmp = next_confline(ctmp)){
      dline++;
      if (!(ctmp->flags & CF_NOSELECT))
	last_selectable = dline;
    }
    dline = last_selectable;
    scroll_setrange(BODY_LINES(ps), dline);
#endif

    /* mangled body or new page, force redraw */
    if(ps->mangled_body || screen->top_line != top_line)
      screen->prev = NULL;

    /* loop thru painting what's needed */
    for(dline = 0, ctmp = top_line;
	dline < BODY_LINES(ps);
	dline++, ctmp = next_confline(ctmp)){

	/*
	 * only fall thru painting if something needs painting...
	 */
	if(!(!screen->prev || ctmp == screen->prev || ctmp == screen->current
	     || ctmp == screen->prev->varnamep
	     || ctmp == screen->current->varnamep
	     || ctmp == screen->prev->headingp
	     || ctmp == screen->current->headingp))
	  continue;

	ClearLine(dline + HEADER_ROWS(ps));

	if(ctmp){
	    if(ctmp->flags & CF_B_LINE)
	      continue;

	    if(ctmp->varname && !(ctmp->flags & CF_INVISIBLEVAR)){
		if(ctmp == screen->current && cursor_pos)
		  cursor_pos->row  = dline + HEADER_ROWS(ps);

		if((ctmp == screen->current
		    || ctmp == screen->current->varnamep
		    || ctmp == screen->current->headingp)
		   && !(ctmp->flags & CF_NOHILITE))
		  StartInverse();

		if(ctmp->flags & CF_H_LINE){
		    MoveCursor(dline + HEADER_ROWS(ps), 0);
		    Write_to_screen(repeat_char(ps->ttyo->screen_cols, '-'));
		}

		if(ctmp->flags & CF_CENTERED){
		    int offset = ps->ttyo->screen_cols/2
				  - (utf8_width(ctmp->varname)/2);
		    MoveCursor(dline + HEADER_ROWS(ps),
			       (offset > 0) ? offset : 0);
		}
		else if(ctmp->varoffset)
		  MoveCursor(dline+HEADER_ROWS(ps), ctmp->varoffset);

		Write_to_screen(ctmp->varname);
		if((ctmp == screen->current
		    || ctmp == screen->current->varnamep
		    || ctmp == screen->current->headingp)
		   && !(ctmp->flags & CF_NOHILITE))
		  EndInverse();
	    }

	    value = (ctmp->flags & CF_INHERIT) ? INHERIT : ctmp->value;
	    if(value){
		char *p;
		int   i, j;

		memset(tmp_20k_buf, '\0',
		       (6*ps->ttyo->screen_cols + 1) * sizeof(char));
		if(ctmp == screen->current){
		    if(!(ctmp->flags & CF_DOUBLEVAR && ctmp->flags & CF_VAR2))
		      StartInverse();

		    if(cursor_pos)
		      cursor_pos->row  = dline + HEADER_ROWS(ps);
		}

		if(ctmp->flags & CF_H_LINE)
		  memset(tmp_20k_buf, '-',
			 ps->ttyo->screen_cols * sizeof(char));

		if(ctmp->flags & CF_CENTERED){
		    int offset = ps->ttyo->screen_cols/2
				  - (utf8_width(value)/2);
		    /* BUG: tabs screw us figuring length above */
		    if(offset > 0){
			char *q;

			p = tmp_20k_buf + offset;
			if(!*(q = tmp_20k_buf))
			  while(q < p)
			    *q++ = ' ';
		    }
		}
		else
		  p = tmp_20k_buf;

		/*
		 * Copy the value to a temp buffer expanding tabs, and
		 * making sure not to write beyond screen right...
		 */
		for(i = 0, j = ctmp->valoffset; value[i]; i++){
		    if(value[i] == ctrl('I')){
			do
			  *p++ = ' ';
			while((++j) & 0x07);
		    }
		    else{
			*p++ = value[i];
			j++;
		    }
		}

		if(ctmp == screen->current && cursor_pos){
		    if(ctmp->flags & CF_DOUBLEVAR && ctmp->flags & CF_VAR2)
		      cursor_pos->col = ctmp->val2offset;
		    else
		      cursor_pos->col = ctmp->valoffset;

		    if(ctmp->tool == radiobutton_tool
#ifdef	ENABLE_LDAP
		       || ctmp->tool==ldap_radiobutton_tool
#endif
		       || ctmp->tool==role_radiobutton_tool
		       || ctmp->tool==checkbox_tool
		       || (ctmp->tool==color_setting_tool &&
			   ctmp->valoffset != COLOR_INDENT))
		      cursor_pos->col++;
		}

		if(ctmp->flags & CF_DOUBLEVAR){
		    long l;

		    p = tmp_20k_buf;
		    first_width = ctmp->val2offset - ctmp->valoffset - SPACE_BETWEEN_DOUBLEVARS;
		    if((l=utf8_width(p)) > first_width && first_width >= 0){
			saveptr = utf8_count_forw_width(p, first_width, &got_width);
			/*
			 * got_width != first_width indicates there's a problem
			 * that should not happen. Ignore it.
			 */
			if(saveptr){
			    save = *saveptr;
			    *saveptr = '\0';
			}
		    }
		    else
		      save = '\0';

		    /*
		     * If this is a COLOR_BLOB line we do special coloring.
		     * The current object inverse hilite is only on the
		     * checkbox part, the exact format comes from the
		     * new_color_line function. If we change that we'll have
		     * to change this to get the coloring right.
		     */
		    if(p[0] == '(' && p[2] == ')' &&
		       p[3] == ' ' && p[4] == ' ' &&
		       (!strncmp(p+5, COLOR_BLOB, COLOR_BLOB_LEN)
		        || !strncmp(p+5, COLOR_BLOB_TRAN, COLOR_BLOB_LEN)
		        || !strncmp(p+5, COLOR_BLOB_NORM, COLOR_BLOB_LEN))){
			COLOR_PAIR  *lastc = NULL, *newc = NULL;

			MoveCursor(dline+HEADER_ROWS(ps), ctmp->valoffset);
			Write_to_screen_n(p, 3);
			if(!(ctmp->flags & CF_VAR2) && ctmp == screen->current)
			  EndInverse();

			Write_to_screen_n(p+3, 3);
			newc = new_color_pair(colorx(CFC_ICOLOR(ctmp)),
					      colorx(CFC_ICOLOR(ctmp)));
			if(newc){
			    lastc = pico_get_cur_color();
			    (void)pico_set_colorp(newc, PSC_NONE);
			    free_color_pair(&newc);
			}

			Write_to_screen_n(p+6, COLOR_BLOB_LEN-2);

			if(lastc){
			    (void)pico_set_colorp(lastc, PSC_NONE);
			    free_color_pair(&lastc);
			}

			Write_to_screen(p+6+COLOR_BLOB_LEN-2);
		    }
		    else{
			PutLine0(dline+HEADER_ROWS(ps), ctmp->valoffset, p);
			if(!(ctmp->flags & CF_VAR2) && ctmp == screen->current)
			  EndInverse();
		    }

		    if(saveptr)
		      *saveptr = save;

		    PutLine0(dline+HEADER_ROWS(ps),
			     ctmp->val2offset - SPACE_BETWEEN_DOUBLEVARS,
			     repeat_char(SPACE_BETWEEN_DOUBLEVARS, SPACE));

		    if(l > ctmp->val2offset - ctmp->valoffset && ctmp->val2offset - ctmp->valoffset >= 0)
		      p = saveptr + SPACE_BETWEEN_DOUBLEVARS;

		    if(p > tmp_20k_buf){
			if(ctmp->flags & CF_VAR2 && ctmp == screen->current)
			  StartInverse();

			if(p[0] == '(' && p[2] == ')' &&
			   p[3] == ' ' && p[4] == ' ' &&
			   (!strncmp(p+5, COLOR_BLOB, COLOR_BLOB_LEN)
			    || !strncmp(p+5, COLOR_BLOB_TRAN, COLOR_BLOB_LEN)
			    || !strncmp(p+5, COLOR_BLOB_NORM, COLOR_BLOB_LEN))){
			    COLOR_PAIR  *lastc = NULL, *newc = NULL;

			    MoveCursor(dline+HEADER_ROWS(ps), ctmp->val2offset);
			    Write_to_screen_n(p, 3);
			    if(ctmp->flags & CF_VAR2 && ctmp == screen->current)
			      EndInverse();

			    Write_to_screen_n(p+3, 3);
			    newc = new_color_pair(colorx(CFC_ICOLOR(ctmp)),
						  colorx(CFC_ICOLOR(ctmp)));
			    if(newc){
				lastc = pico_get_cur_color();
				(void)pico_set_colorp(newc, PSC_NONE);
				free_color_pair(&newc);
			    }

			    Write_to_screen_n(p+6, COLOR_BLOB_LEN-2);

			    if(lastc){
				(void)pico_set_colorp(lastc, PSC_NONE);
				free_color_pair(&lastc);
			    }

			    Write_to_screen(p+6+COLOR_BLOB_LEN-2);
			}
			else{
			    PutLine0(dline+HEADER_ROWS(ps),ctmp->val2offset,p);
			    if(ctmp->flags & CF_VAR2 && ctmp == screen->current)
			      EndInverse();
			}
		    }
		}
		else{
		    char       *q, *first_space, *sample, *ptr;
		    COLOR_PAIR *lastc, *newc;
		    int         invert;


		    if(ctmp->flags & CF_COLORSAMPLE &&
		       pico_usingcolor() &&
		       ((q = strstr(tmp_20k_buf, SAMPLE_LEADER)) ||
			(q = strstr(tmp_20k_buf, "Color"))) &&
		       (first_space = strindex(q, SPACE)) &&
		       (strstr(value, SAMP1) ||
		        strstr(value, SAMP2))){

			ptr = tmp_20k_buf;

			/* write out first part */
			*first_space = '\0';
			PutLine0(dline+HEADER_ROWS(ps), ctmp->valoffset,
				 ptr);
			*first_space = SPACE;
			ptr = first_space;

			if(ctmp == screen->current)
			  EndInverse();

			sample = skip_white_space(ptr);
			/* if there's enough room to put some sample up */
			save = *sample;
			*sample = '\0';
			w = utf8_width(tmp_20k_buf);
			*sample = save;
			if(ctmp->valoffset + w < ps->ttyo->screen_cols){

			    sample++;	/* for `[' at edge of sample */

			    save = *ptr;
			    *ptr = '\0';
			    w = utf8_width(tmp_20k_buf);
			    *ptr = save;

			    save = *sample;
			    *sample = '\0';
			    /* spaces and bracket before sample1 */
			    PutLine0(dline+HEADER_ROWS(ps), ctmp->valoffset+w, ptr);
			    *sample = save;

			    ptr = sample;

			    /* then the color sample */
			    if(ctmp->var == &ps->vars[V_VIEW_HDR_COLORS]){
				SPEC_COLOR_S *hc, *hcolors;

				lastc = newc = NULL;

				hcolors =
				  spec_colors_from_varlist(LVAL(ctmp->var, ew),
							   0);
				for(hc = hcolors, i=0; hc; hc = hc->next, i++)
				  if(CFC_ICUST(ctmp) == i)
				    break;

				if(hc && hc->fg && hc->fg[0] && hc->bg &&
				   hc->bg[0])
				  newc = new_color_pair(hc->fg, hc->bg);

				if(newc){
				    lastc = pico_get_cur_color();
				    (void)pico_set_colorp(newc, PSC_NONE);
				    free_color_pair(&newc);
				}

				if(hcolors)
				  free_spec_colors(&hcolors);


				/* print out sample1 */

				save = *ptr;
				*ptr = '\0';
				w = utf8_width(tmp_20k_buf);
				*ptr = save;

				want_width = MIN(utf8_width(SAMP1)-2, ps->ttyo->screen_cols - w - ctmp->valoffset);
				saveptr = utf8_count_forw_width(ptr, want_width, &got_width);
				if(saveptr){
				    save = *saveptr;
				    *saveptr = '\0';
				}

				PutLine0(dline+HEADER_ROWS(ps), ctmp->valoffset + w, ptr);
				if(saveptr)
				  *saveptr = save;

				ptr = strindex(ptr, ']');

				if(lastc){
				    (void)pico_set_colorp(lastc, PSC_NONE);
				    free_color_pair(&lastc);
				}
			    }
			    else if(ctmp->var == &ps->vars[V_KW_COLORS]){
				KEYWORD_S    *kw;
				SPEC_COLOR_S *kw_col = NULL;

				lastc = newc = NULL;

				/* find keyword associated with this line */
				for(kw=ps->keywords, i=0; kw; kw=kw->next, i++)
				  if(CFC_ICUST(ctmp) == i)
				    break;

				if(kw)
				  kw_col =
				   spec_colors_from_varlist(LVAL(ctmp->var,ew),
							    0);

				/* color for this keyword */
				if(kw && kw_col
				   && ((kw->nick && kw->nick[0]
				        && (newc=hdr_color(kw->nick, NULL,
							   kw_col)))
				       ||
				       (kw->kw && kw->kw[0]
				        && (newc=hdr_color(kw->kw, NULL,
							   kw_col))))){
				    lastc = pico_get_cur_color();
				    (void)pico_set_colorp(newc, PSC_NONE);
				    free_color_pair(&newc);
				}

				if(kw_col)
				  free_spec_colors(&kw_col);

				/* print out sample1 */

				save = *ptr;
				*ptr = '\0';
				w = utf8_width(tmp_20k_buf);
				*ptr = save;

				want_width = MIN(utf8_width(SAMP1)-2, ps->ttyo->screen_cols - w - ctmp->valoffset);
				saveptr = utf8_count_forw_width(ptr, want_width, &got_width);
				if(saveptr){
				    save = *saveptr;
				    *saveptr = '\0';
				}

				PutLine0(dline+HEADER_ROWS(ps), ctmp->valoffset + w, ptr);
				if(saveptr)
				  *saveptr = save;

				ptr = strindex(ptr, ']');

				if(lastc){
				    (void)pico_set_colorp(lastc, PSC_NONE);
				    free_color_pair(&lastc);
				}
			    }
			    else{
				lastc = NULL;
				invert = 0;
				newc = sample_color(ps, ctmp->var);
				if(newc){
				    if((lastc = pico_get_cur_color()) != NULL)
				      (void)pico_set_colorp(newc, PSC_NONE);

				    free_color_pair(&newc);
				}
				else if(var_defaults_to_rev(ctmp->var)){
				    if((newc = pico_get_rev_color()) != NULL){
					/*
					 * Note, don't have to free newc.
					 */
					if((lastc = pico_get_cur_color()) != NULL)
					  (void)pico_set_colorp(newc, PSC_NONE);
				    }
				    else{
					StartInverse();
					invert = 1;
				    }
				}

				if(ctmp->var==&ps->vars[V_SLCTBL_FORE_COLOR] &&

				   (F_OFF(F_SLCTBL_ITEM_NOBOLD, ps) ||
				    !(PVAL(ctmp->var,ew) &&
				      PVAL(ctmp->var+1,ew))))
				  StartBold();

				/* print out sample1 */

				save = *ptr;
				*ptr = '\0';
				w = utf8_width(tmp_20k_buf);
				*ptr = save;

				want_width = MIN(utf8_width(SAMP1)-2, ps->ttyo->screen_cols - w - ctmp->valoffset);
				saveptr = utf8_count_forw_width(ptr, want_width, &got_width);
				if(saveptr){
				    save = *saveptr;
				    *saveptr = '\0';
				}

				PutLine0(dline+HEADER_ROWS(ps), ctmp->valoffset + w, ptr);
				if(saveptr)
				  *saveptr = save;

				ptr = strindex(ptr, ']');

				if(ctmp->var==&ps->vars[V_SLCTBL_FORE_COLOR] &&

				   (F_OFF(F_SLCTBL_ITEM_NOBOLD, ps) ||
				    !(PVAL(ctmp->var,ew) &&
				      PVAL(ctmp->var+1,ew))))
				  EndBold();

				if(lastc){
				    (void)pico_set_colorp(lastc, PSC_NONE);
				    free_color_pair(&lastc);
				}
				else if(invert)
				  EndInverse();
			    }

			    /*
			     * Finish sample1 with the right bracket.
			     */
			    save = *ptr;
			    *ptr = '\0';
			    w = utf8_width(tmp_20k_buf);
			    *ptr = save;
			    if(ctmp->valoffset + w < ps->ttyo->screen_cols){
				save = *(ptr+1);
				*(ptr+1) = '\0';
				PutLine0(dline+HEADER_ROWS(ps), ctmp->valoffset + w, ptr);
				*(ptr+1) = save;
				ptr++;
				w++;
			    }

			    /*
			     * Now check for an exception sample and paint it.
			     */
			    if(ctmp->valoffset + w + SBS + 1 < ps->ttyo->screen_cols && (q = strstr(ptr, SAMPEXC))){
				/* spaces + `[' */
				save = ptr[SBS+1];
				ptr[SBS+1] = '\0';
				PutLine0(dline+HEADER_ROWS(ps), ctmp->valoffset + w, ptr);
				ptr[SBS+1] = save;
				ptr += (SBS+1);

				/*
				 * Figure out what color to paint it.
				 * This only happens with normal variables,
				 * not with V_VIEW_HDR_COLORS.
				 */
				lastc = NULL;
				invert = 0;
				newc = sampleexc_color(ps, ctmp->var);
				if(newc){
				    if((lastc = pico_get_cur_color()) != NULL)
				      (void)pico_set_colorp(newc, PSC_NONE);

				    free_color_pair(&newc);
				}
				else if(var_defaults_to_rev(ctmp->var)){
				    if((newc = pico_get_rev_color()) != NULL){
					/*
					 * Note, don't have to free newc.
					 */
					if((lastc = pico_get_cur_color()) != NULL)
					  (void)pico_set_colorp(newc, PSC_NONE);
				    }
				    else{
					StartInverse();
					invert = 1;
				    }
				}

				if(ctmp->var==&ps->vars[V_SLCTBL_FORE_COLOR] &&

				   (F_OFF(F_SLCTBL_ITEM_NOBOLD, ps) ||
				    !(PVAL(ctmp->var,Post) &&
				      PVAL(ctmp->var+1,Post))))
				  StartBold();

				/* sample2 */
				save = *ptr;
				*ptr = '\0';
				w = utf8_width(tmp_20k_buf);
				*ptr = save;

				want_width = MIN(utf8_width(SAMPEXC)-2, ps->ttyo->screen_cols - w - ctmp->valoffset);
				saveptr = utf8_count_forw_width(ptr, want_width, &got_width);
				if(saveptr){
				    save = *saveptr;
				    *saveptr = '\0';
				}

				PutLine0(dline+HEADER_ROWS(ps), ctmp->valoffset + w, ptr);
				if(saveptr)
				  *saveptr = save;

				ptr = strindex(ptr, ']');

				/* turn off bold and color */
				if(ctmp->var==&ps->vars[V_SLCTBL_FORE_COLOR] &&

				   (F_OFF(F_SLCTBL_ITEM_NOBOLD, ps) ||
				    !(PVAL(ctmp->var,Post) &&
				      PVAL(ctmp->var+1,Post))))
				  EndBold();

				if(lastc){
				    (void)pico_set_colorp(lastc, PSC_NONE);
				    free_color_pair(&lastc);
				}
				else if(invert)
				  EndInverse();

				/*
				 * Finish sample2 with the right bracket.
				 */
				save = *ptr;
				*ptr = '\0';
				w = utf8_width(tmp_20k_buf);
				*ptr = save;
				if(ctmp->valoffset + w < ps->ttyo->screen_cols){
				    save = *(ptr+1);
				    *(ptr+1) = '\0';
				    PutLine0(dline+HEADER_ROWS(ps), ctmp->valoffset + w, ptr);
				    *(ptr+1) = save;
				    ptr++;
				    w++;
				}
			    }

			    /* paint rest of the line if there is any left */
			    if(ctmp->valoffset + w < ps->ttyo->screen_cols && *ptr){
				want_width = ps->ttyo->screen_cols - w - ctmp->valoffset; 
				saveptr = utf8_count_forw_width(ptr, want_width, &got_width);
				if(saveptr){
				    save = *saveptr;
				    *saveptr = '\0';
				}

				PutLine0(dline+HEADER_ROWS(ps), ctmp->valoffset + w, ptr);
				if(saveptr)
				  *saveptr = save;
			    }
			}
		    }
		    else{
			w = utf8_width(tmp_20k_buf);
			want_width = ps->ttyo->screen_cols - ctmp->valoffset; 
			if(w > want_width){
			    saveptr = utf8_count_forw_width(tmp_20k_buf, want_width, &got_width);
			    if(saveptr)
			      *saveptr = '\0';
			}

			PutLine0(dline+HEADER_ROWS(ps), ctmp->valoffset, tmp_20k_buf);
			if(ctmp == screen->current)
			  EndInverse();
		    }
		}
	    }
	}
    }

    ps->mangled_body = 0;
    screen->top_line = top_line;
    screen->prev     = screen->current;
#ifdef _WINDOWS
    mswin_endupdate();
#endif
}



/*
 * 
 */
void
print_option_screen(OPT_SCREEN_S *screen, char *prompt)
{
    CONF_S *ctmp;
    int     so_far;
    char    line[500];

    if(open_printer(prompt) == 0){
	for(ctmp = first_confline(screen->current);
	    ctmp;
	    ctmp = next_confline(ctmp)){

	    so_far = 0;
	    if(ctmp->varname && !(ctmp->flags & CF_INVISIBLEVAR)){

		snprintf(line, sizeof(line), "%*s%s", ctmp->varoffset, "",
			ctmp->varname);
		line[sizeof(line)-1] = '\0';
		print_text(line);
		so_far = ctmp->varoffset + utf8_width(ctmp->varname);
	    }

	    if(ctmp && ctmp->value){
		char *p = tmp_20k_buf;
		int   i, j, spaces;

		/* Copy the value to a temp buffer expanding tabs. */
		for(i = 0, j = ctmp->valoffset; ctmp->value[i]; i++){
		    if(ctmp->value[i] == ctrl('I')){
			do
			  *p++ = ' ';
			while((++j) & 0x07);
			      
		    }
		    else{
			*p++ = ctmp->value[i];
			j++;
		    }
		}

		*p = '\0';
		removing_trailing_white_space(tmp_20k_buf);

		spaces = MAX(ctmp->valoffset - so_far, 0);
		snprintf(line, sizeof(line), "%*s%s\n", spaces, "", tmp_20k_buf);
		line[sizeof(line)-1] = '\0';
		print_text(line);
	    }
	}

	close_printer();
    }
}



/*
 *
 */
void
option_screen_redrawer(void)
{
    ps_global->mangled_body = 1;
    update_option_screen(ps_global, opt_screen, (Pos *)NULL);
}



/*
 * pretty_value - given the line, return an
 *                alloc'd string for line's value...
 */
char *
pretty_value(struct pine *ps, CONF_S *cl)
{
    struct variable *v;

    v = cl->var;

    if(v == &ps->vars[V_FEATURE_LIST])
      return(checkbox_pretty_value(ps, cl));
    else if(standard_radio_var(ps, v) || v == startup_ptr)
      return(radio_pretty_value(ps, cl));
    else if(v == &ps->vars[V_SORT_KEY])
      return(sort_pretty_value(ps, cl));
    else if(v == &ps->vars[V_SIGNATURE_FILE])
      return(sigfile_pretty_value(ps, cl));
    else if(v == &ps->vars[V_USE_ONLY_DOMAIN_NAME])
      return(yesno_pretty_value(ps, cl));
    else if(color_holding_var(ps, v))
      return(color_pretty_value(ps, cl));
    else
      return(text_pretty_value(ps, cl));
}


char *
text_pretty_value(struct pine *ps, CONF_S *cl)
{
    char  tmp[6*MAX_SCREEN_COLS+20], *pvalnorm, **lvalnorm, *pvalexc, **lvalexc;
    char *p, *pval, **lval, lastchar = '\0';
    int   editing_except, fixed, uvalset, uvalposlen;
    unsigned got_width;
    int   comments, except_set, avail_width;
    int   norm_with_except = 0, norm_with_except_inherit = 0;
    int   inherit_line = 0;

    editing_except = (ew == ps_global->ew_for_except_vars);
    fixed = cl->var->is_fixed;
    if((ps_global->ew_for_except_vars != Main) && (ew == Main))
      norm_with_except++;	/* editing normal and except config exists */

    if(cl->var->is_list){
	lvalnorm   = LVAL(cl->var, Main);
	lvalexc    = LVAL(cl->var, ps_global->ew_for_except_vars);
	if(editing_except){
	    uvalset    = lvalexc != NULL;
	    uvalposlen = uvalset && lvalexc[0] && lvalexc[0][0];
	    lval       = lvalexc;
	}
	else{
	    uvalset    = lvalnorm != NULL;
	    uvalposlen = uvalset && lvalnorm[0] && lvalnorm[0][0];
	    lval       = lvalnorm;
	}

	except_set = lvalexc != NULL;
	comments = cl->var->current_val.l != NULL;
	if(norm_with_except && except_set && lvalexc[0] &&
	   !strcmp(lvalexc[0],INHERIT))
	  norm_with_except_inherit++;

	if(uvalset && !strcmp(lval[0], INHERIT)){
	    if(cl->varmem == 0){
		inherit_line++;
		comments = 0;
	    }
	}

	/* only add extra comments on last member of list */
	if(uvalset && !inherit_line && lval && lval[cl->varmem] &&
	   lval[cl->varmem + 1])
	  comments = 0;
    }
    else{
	pvalnorm   = PVAL(cl->var, Main);
	pvalexc    = PVAL(cl->var, ps_global->ew_for_except_vars);
	if(editing_except){
	    uvalset    = pvalexc != NULL;
	    uvalposlen = uvalset && *pvalexc;
	    pval       = pvalexc;
	}
	else{
	    uvalset    = pvalnorm != NULL;
	    uvalposlen = uvalset && *pvalnorm;
	    pval       = pvalnorm;
	}

	except_set = pvalexc != NULL;
	comments   = cl->var->current_val.p != NULL;
    }

    memset(tmp, 0, sizeof(tmp));
    p = tmp;
    *p = '\0';

    avail_width = ps->ttyo->screen_cols - cl->valoffset;

    if(fixed || !uvalset || !uvalposlen){
      p += utf8_to_width(p, "<", sizeof(tmp)-(p-tmp), avail_width, &got_width);
      avail_width -= got_width;
    }

    if(fixed){
      p += utf8_to_width(p, _(fixed_val), sizeof(tmp)-(p-tmp), avail_width, &got_width);
      avail_width -= got_width;
    }
    else if(!uvalset){
      p += utf8_to_width(p, _(no_val), sizeof(tmp)-(p-tmp), avail_width, &got_width);
      avail_width -= got_width;
    }
    else if(!uvalposlen){
      p += utf8_to_width(p, _(empty_val), sizeof(tmp)-(p-tmp), avail_width, &got_width);
      avail_width -= got_width;
    }
    else if(inherit_line){
      p += utf8_to_width(p, INHERIT, sizeof(tmp)-(p-tmp), avail_width, &got_width);
      avail_width -= got_width;
    }
    else{
	if(cl->var->is_list){
	  p += utf8_to_width(p, lval[cl->varmem], sizeof(tmp)-(p-tmp), avail_width, &got_width);
	  avail_width -= got_width;
	}
	else{
	  p += utf8_to_width(p, pval, sizeof(tmp)-(p-tmp), avail_width, &got_width);
	  avail_width -= got_width;
	}
    }

    if(comments && (fixed || !uvalset || (norm_with_except && except_set))){
	if(fixed || !uvalset){
	  p += utf8_to_width(p, ": using ", sizeof(tmp)-(p-tmp), avail_width, &got_width);
	  avail_width -= got_width;
	}

	if(norm_with_except && except_set){
	    if(!uvalset){
	      p += utf8_to_width(p, "exception ", sizeof(tmp)-(p-tmp), avail_width, &got_width);
	      avail_width -= got_width;
	    }
	    else if(!fixed){
		if(!uvalposlen){
		  p += utf8_to_width(p, ": ", sizeof(tmp)-(p-tmp), avail_width, &got_width);
		  avail_width -= got_width;
		}
		else{
		    p += utf8_to_width(p, " (", sizeof(tmp)-(p-tmp), avail_width, &got_width);
		    avail_width -= got_width;
		}

		if(norm_with_except_inherit){
		  p += utf8_to_width(p, "added to by exception ", sizeof(tmp)-(p-tmp), avail_width, &got_width);
		  avail_width -= got_width;
		}
		else{
		  p += utf8_to_width(p, "overridden by exception ", sizeof(tmp)-(p-tmp), avail_width, &got_width);
		  avail_width -= got_width;
		}
	    }
	}

	if(avail_width >= 7){
	  if(cl->var == &ps_global->vars[V_POST_CHAR_SET]){
	    p += utf8_to_width(p, "most specific (see help)", sizeof(tmp)-(p-tmp), avail_width, &got_width);
	    avail_width -= got_width;
	  }
	  else{
	    sstrncpy(&p, "\"", sizeof(tmp)-(p-tmp));
	    avail_width--;
	    if(cl->var->is_list){
		char **the_list;

		the_list = cl->var->current_val.l;

		if(norm_with_except && except_set)
		  the_list = lvalexc;
		
		if(the_list && the_list[0] && !strcmp(the_list[0], INHERIT))
		  the_list++;

		for(lval = the_list; avail_width-(p-tmp) > 0 && *lval; lval++){
		    if(lval != the_list){
		      p += utf8_to_width(p, ",", sizeof(tmp)-(p-tmp), avail_width, &got_width);
		      avail_width -= got_width;
		    }

		    p += utf8_to_width(p, *lval, sizeof(tmp)-(p-tmp), avail_width, &got_width);
		    avail_width -= got_width;
		}
	    }
	    else{
	      p += utf8_to_width(p, cl->var->current_val.p, sizeof(tmp)-(p-tmp), avail_width, &got_width);
	      avail_width -= got_width;
	    }

	    if(p-tmp+2 < sizeof(tmp)){
		*p++ = '\"';
		*p = '\0';
	    }
	  }
	}
	else if(*(p-1) == SPACE)
	  *--p = '\0';
    }

    tmp[sizeof(tmp)-1] = '\0';

    if(fixed || !uvalset || !uvalposlen)
      lastchar = '>';
    else if(comments && norm_with_except && except_set)
      lastchar = ')';

    if(lastchar){
	if(p-tmp+2 < sizeof(tmp)){
	    *p++ = lastchar;
	    *p = '\0';
	}
    }

    tmp[sizeof(tmp)-1] = '\0';
    avail_width = ps->ttyo->screen_cols - cl->valoffset;

    if(utf8_width(tmp) < avail_width)
      snprintf(tmp+strlen(tmp), sizeof(tmp)-strlen(tmp), "%*s", avail_width-utf8_width(tmp), "");

    tmp[sizeof(tmp)-1] = '\0';

    return(cpystr(tmp));
}


char *
checkbox_pretty_value(struct pine *ps, CONF_S *cl)
{
    char             tmp[6*MAXPATH];
    char            *comment = NULL;
    int              indent, x, screen_width, need;
    int              longest_featname, longest_comment;
    int              nwidcomm;		/* name width with comment */
    int              nwidnocomm;	/* and without comment */
    FEATURE_S	    *feature;

    screen_width = (ps && ps->ttyo) ? ps->ttyo->screen_cols : 80;
    tmp[0] = '\0';

    longest_featname = longest_feature_name();
    longest_comment = longest_feature_comment(ps, ew);
    indent = feature_indent();

    nwidcomm = longest_featname;
    nwidnocomm = longest_featname + 2 + longest_comment;

    if((need = (indent + 5 + longest_featname + 2 + longest_comment) - screen_width) > 0){
	if(need < 10){
	    nwidcomm   -= need;
	    nwidnocomm -= need;
	}
	else{
	    longest_comment = 0;
	    nwidnocomm = longest_featname;
	}
    }

    feature = feature_list(cl->varmem);

    x = feature_gets_an_x(ps, cl->var, feature, &comment, ew);

    if(longest_comment && comment && *comment){
	utf8_snprintf(tmp, sizeof(tmp), "[%c]  %-*.*w  %-*.*w", x ? 'X' : ' ',
		      nwidcomm, nwidcomm,
		      pretty_feature_name(feature->name, nwidcomm),
		      longest_comment, longest_comment, comment ? comment : "");
    }
    else{
	utf8_snprintf(tmp, sizeof(tmp), "[%c]  %-*.*w", x ? 'X' : ' ',
		      nwidnocomm, nwidnocomm,
		      pretty_feature_name(feature->name, nwidnocomm));
    }

    return(cpystr(tmp));
}


int
longest_feature_name(void)
{
    static int lv = -1;
    int i, j;
    FEATURE_S	    *feature;

    if(lv < 0){
	for(lv = 0, i = 0; (feature = feature_list(i)); i++)
	  if(feature_list_section(feature)
	     && lv < (j = utf8_width(pretty_feature_name(feature->name, -1))))
	    lv = j;

	lv = MIN(lv, 100);
    }

    return(lv);
}


int
feature_indent(void)
{
    return(6);
}


char *
yesno_pretty_value(struct pine *ps, CONF_S *cl)
{
    char  tmp[6*MAXPATH], *pvalnorm, *pvalexc;
    char *p, *pval, lastchar = '\0';
    int   editing_except, fixed, norm_with_except, uvalset;
    int   curval, except_set;

    editing_except = (ew == ps_global->ew_for_except_vars);
    fixed = cl->var->is_fixed;
    if((ps_global->ew_for_except_vars == Main) ||
       (ew == ps_global->ew_for_except_vars))
      norm_with_except = 0;
    else
      norm_with_except = 1;	/* editing normal and except config exists */

    pvalnorm   = PVAL(cl->var, Main);
    pvalexc    = PVAL(cl->var, ps_global->ew_for_except_vars);
    if(editing_except){
	uvalset    = (pvalexc != NULL &&
		      (!strucmp(pvalexc,yesstr) || !strucmp(pvalexc,nostr)));
	pval       = pvalexc;
    }
    else{
	uvalset    = (pvalnorm != NULL &&
		      (!strucmp(pvalnorm,yesstr) || !strucmp(pvalnorm,nostr)));
	pval       = pvalnorm;
    }

    except_set = (pvalexc != NULL &&
		  (!strucmp(pvalexc,yesstr) || !strucmp(pvalexc,nostr)));
    curval  = (cl->var->current_val.p != NULL &&
	       (!strucmp(cl->var->current_val.p,yesstr) ||
	        !strucmp(cl->var->current_val.p,nostr)));

    p = tmp;
    *p = '\0';

    if(fixed || !uvalset)
      sstrncpy(&p, "<", sizeof(tmp)-(p-tmp));

    if(fixed)
      sstrncpy(&p, _(fixed_val), sizeof(tmp)-(p-tmp));
    else if(!uvalset)
      sstrncpy(&p, _(no_val), sizeof(tmp)-(p-tmp));
    else if(!strucmp(pval, yesstr))
      sstrncpy(&p, yesstr, sizeof(tmp)-(p-tmp));
    else
      sstrncpy(&p, nostr, sizeof(tmp)-(p-tmp));

    if(curval && (fixed || !uvalset || (norm_with_except && except_set))){
	if(fixed || !uvalset)
	  sstrncpy(&p, ": using ", sizeof(tmp)-(p-tmp));

	if(norm_with_except && except_set){
	    if(!uvalset)
	      sstrncpy(&p, "exception ", sizeof(tmp)-(p-tmp));
	    else if(!fixed){
		sstrncpy(&p, " (", sizeof(tmp)-(p-tmp));
		sstrncpy(&p, "overridden by exception ", sizeof(tmp)-(p-tmp));
	    }
	}

	sstrncpy(&p, "\"", sizeof(tmp)-(p-tmp));
	sstrncpy(&p, !strucmp(cl->var->current_val.p,yesstr) ? yesstr : nostr, sizeof(tmp)-(p-tmp));
	sstrncpy(&p, "\"", sizeof(tmp)-(p-tmp));
    }

    if(fixed || !uvalset)
      lastchar = '>';
    else if(curval && norm_with_except && except_set)
      lastchar = ')';

    if(lastchar && sizeof(tmp)-(p-tmp) > 1){
	*p++ = lastchar;
	*p = '\0';
    }

    tmp[sizeof(tmp)-1] = '\0';

    if(utf8_width(tmp) < ps->ttyo->screen_cols - cl->valoffset)
      snprintf(tmp+strlen(tmp), sizeof(tmp)-strlen(tmp),
	      "%*s", ps->ttyo->screen_cols - cl->valoffset - utf8_width(tmp), "");

    tmp[sizeof(tmp)-1] = '\0';

    return(cpystr(tmp));
}


char *
radio_pretty_value(struct pine *ps, CONF_S *cl)
{
    char  tmp[6*MAXPATH];
    char *pvalnorm, *pvalexc, *pval;
    int   editing_except_which_isnt_normal, editing_normal_which_isnt_except;
    int   fixed, is_set_for_this_level = 0, is_the_one, the_exc_one;
    int   i, j, lv = 0;
    NAMEVAL_S *rule = NULL, *f;
    PTR_TO_RULEFUNC rulefunc;
    struct variable *v;

    tmp[0] = '\0';
    v = cl->var;

    editing_except_which_isnt_normal = (ew == ps_global->ew_for_except_vars &&
					ew != Main);
    editing_normal_which_isnt_except = (ew == Main &&
					ew != ps_global->ew_for_except_vars);
    fixed = cl->var->is_fixed;
    pvalnorm = PVAL(v, Main);
    pvalexc  = PVAL(v, ps_global->ew_for_except_vars);

    rulefunc = rulefunc_from_var(ps, v);
    rule = rulefunc ? (*rulefunc)(cl->varmem) : NULL;

    /* find longest name */
    if(rulefunc)
      for(lv = 0, i = 0; (f = (*rulefunc)(i)); i++)
        if(lv < (j = utf8_width(f->name)))
	  lv = j;

    lv = MIN(lv, 100);

    if(editing_except_which_isnt_normal)
      pval = pvalexc;
    else
      pval = pvalnorm;

    if(pval)
      is_set_for_this_level++;

    if(fixed){
	pval = v->fixed_val.p;
	is_the_one = (pval && !strucmp(pval, S_OR_L(rule)));

	utf8_snprintf(tmp, sizeof(tmp), "(%c)  %-*.*w%s",
		is_the_one ? R_SELD : ' ',
		lv, lv, rule->name, is_the_one ? "   (value is fixed)" : "");
    }
    else if(is_set_for_this_level){
	is_the_one = (pval && !strucmp(pval, S_OR_L(rule)));
	the_exc_one = (editing_normal_which_isnt_except && pvalexc &&
		       !strucmp(pvalexc, S_OR_L(rule)));
	utf8_snprintf(tmp, sizeof(tmp), "(%c)  %-*.*w%s",
		is_the_one ? R_SELD : ' ',
		lv, lv, rule->name,
		(!is_the_one && the_exc_one) ? "   (value set in exceptions)" :
		 (is_the_one && the_exc_one) ? "   (also set in exceptions)" :
		  (is_the_one &&
		   editing_normal_which_isnt_except &&
		   pvalexc &&
		   !the_exc_one)             ? "   (overridden by exceptions)" :
					       "");
    }
    else{
	if(pvalexc){
	    is_the_one = !strucmp(pvalexc, S_OR_L(rule));
	    utf8_snprintf(tmp, sizeof(tmp), "(%c)  %-*.*w%s",
		    is_the_one ? R_SELD : ' ',
		    lv, lv, rule->name,
		    is_the_one ? "   (value set in exceptions)" : "");
	}
	else{
	    pval = v->current_val.p;
	    is_the_one = (pval && !strucmp(pval, S_OR_L(rule)));
	    utf8_snprintf(tmp, sizeof(tmp), "(%c)  %-*.*w%s",
		    is_the_one ? R_SELD : ' ',
		    lv, lv, rule->name,
		    is_the_one ? ((editing_except_which_isnt_normal && pvalnorm) ? "   (default from regular config)" : "   (default)") : "");
	}
    }

    tmp[sizeof(tmp)-1] = '\0';

    return(cpystr(tmp));
}


char *
sigfile_pretty_value(struct pine *ps, CONF_S *cl)
{
    if(cl && cl->var == &ps->vars[V_SIGNATURE_FILE] &&
       cl->prev && cl->prev->var == &ps->vars[V_LITERAL_SIG]){
	if(cl->prev->var->current_val.p){
	    cl->flags |= CF_NOSELECT;		/* side effect */
	    return(cpystr(_("<Ignored: using Literal-Signature instead>")));
	}
	else{
	    cl->flags &= ~CF_NOSELECT;
	    return(text_pretty_value(ps, cl));
	}
    }
    else
      return(cpystr(""));
}


char *
color_pretty_value(struct pine *ps, CONF_S *cl)
{
    char             tmp[6*MAXPATH];
    char            *p, *q;
    struct variable *v;
    int              is_index;

    tmp[0] = '\0';
    v = cl->var;

    if(v && color_holding_var(ps, v) &&
       (p=srchstr(v->name, "-foreground-color"))){

	is_index = !struncmp(v->name, "index-", 6);

	q = sampleexc_text(ps, v);
	utf8_snprintf(tmp, sizeof(tmp), "%c%.*s %sColor%*.50s %.20w%*s%.20w%.20w",
		islower((unsigned char)v->name[0])
					? toupper((unsigned char)v->name[0])
					: v->name[0],
		MIN(p-v->name-1,30), v->name+1,
		is_index ? "Symbol " : "",
		MAX(EQ_COL - COLOR_INDENT -1 - MIN(p-v->name-1,30)
			    - 6 - (is_index ? 7 : 0) - 1,0), "",
		sample_text(ps,v), *q ? SBS : 0, "", q,
		color_parenthetical(v));
    }

    tmp[sizeof(tmp)-1] = '\0';

    return(cpystr(tmp));
}


char *
sort_pretty_value(struct pine *ps, CONF_S *cl)
{
    return(generalized_sort_pretty_value(ps, cl, 1));
}


char *
generalized_sort_pretty_value(struct pine *ps, CONF_S *cl, int default_ok)
{
    char  tmp[6*MAXPATH];
    char *pvalnorm, *pvalexc, *pval;
    int   editing_except_which_isnt_normal, editing_normal_which_isnt_except;
    int   fixed, is_set_for_this_level = 0, is_the_one, the_exc_one;
    int   i, j, lv = 0;
    struct variable *v;
    SortOrder line_sort, var_sort, exc_sort;
    int       line_sort_rev, var_sort_rev, exc_sort_rev;

    tmp[0] = '\0';
    v = cl->var;

    editing_except_which_isnt_normal = (ew == ps_global->ew_for_except_vars &&
					ew != Main);
    editing_normal_which_isnt_except = (ew == Main &&
					ew != ps_global->ew_for_except_vars);
    fixed = cl->var->is_fixed;
    pvalnorm = PVAL(v, Main);
    pvalexc  = PVAL(v, ps_global->ew_for_except_vars);

    /* find longest value's name */
    for(lv = 0, i = 0; ps->sort_types[i] != EndofList; i++)
      if(lv < (j = utf8_width(sort_name(ps->sort_types[i]))))
	lv = j;
    
    lv = MIN(lv, 100);

    if(editing_except_which_isnt_normal)
      pval = pvalexc;
    else
      pval = pvalnorm;

    if(pval)
      is_set_for_this_level++;

    /* the config line we're talking about */
    if(cl->varmem >= 0){
	line_sort_rev = cl->varmem >= (short)EndofList;
	line_sort     = (SortOrder)(cl->varmem - (line_sort_rev * EndofList));
    }

    if(cl->varmem < 0){
	utf8_snprintf(tmp, sizeof(tmp), "(%c)  %-*w",
		(pval == NULL) ? R_SELD : ' ',
		lv, "Default");
    }
    else if(fixed){
	pval = v->fixed_val.p;
	decode_sort(pval, &var_sort, &var_sort_rev);
	is_the_one = (var_sort_rev == line_sort_rev && var_sort == line_sort);

	utf8_snprintf(tmp, sizeof(tmp), "(%c)  %s%-*w%*s%s",
		is_the_one ? R_SELD : ' ',
		line_sort_rev ? "Reverse " : "",
		lv, sort_name(line_sort),
		line_sort_rev ? 0 : 8, "",
		is_the_one ? "   (value is fixed)" : "");
    }
    else if(is_set_for_this_level){
	decode_sort(pval, &var_sort, &var_sort_rev);
	is_the_one = (var_sort_rev == line_sort_rev && var_sort == line_sort);
	decode_sort(pvalexc, &exc_sort, &exc_sort_rev);
	the_exc_one = (editing_normal_which_isnt_except && pvalexc &&
		       exc_sort_rev == line_sort_rev && exc_sort == line_sort);
	utf8_snprintf(tmp, sizeof(tmp), "(%c)  %s%-*w%*s%s",
		is_the_one ? R_SELD : ' ',
		line_sort_rev ? "Reverse " : "",
		lv, sort_name(line_sort),
		line_sort_rev ? 0 : 8, "",
		(!is_the_one && the_exc_one) ? "   (value set in exceptions)" :
		 (is_the_one && the_exc_one) ? "   (also set in exceptions)" :
		  (is_the_one &&
		   editing_normal_which_isnt_except &&
		   pvalexc &&
		   !the_exc_one)             ? "   (overridden by exceptions)" :
					       "");
    }
    else{
	if(pvalexc){
	    decode_sort(pvalexc, &exc_sort, &exc_sort_rev);
	    is_the_one = (exc_sort_rev == line_sort_rev &&
			  exc_sort == line_sort);
	    utf8_snprintf(tmp, sizeof(tmp), "( )  %s%-*w%*s%s",
		    line_sort_rev ? "Reverse " : "",
		    lv, sort_name(line_sort),
		    line_sort_rev ? 0 : 8, "",
		    is_the_one ? "   (value set in exceptions)" : "");
	}
	else{
	    pval = v->current_val.p;
	    decode_sort(pval, &var_sort, &var_sort_rev);
	    is_the_one = ((pval || default_ok) &&
			  var_sort_rev == line_sort_rev &&
			  var_sort == line_sort);
	    utf8_snprintf(tmp, sizeof(tmp), "(%c)  %s%-*w%*s%s",
		    is_the_one ? R_SELD : ' ',
		    line_sort_rev ? "Reverse " : "",
		    lv, sort_name(line_sort),
		    line_sort_rev ? 0 : 8, "",
		    is_the_one ? ((editing_except_which_isnt_normal && pvalnorm) ? "   (default from regular config)" : "   (default)") : "");
	}
    }

    return(cpystr(tmp));
}


COLOR_PAIR *
sample_color(struct pine *ps, struct variable *v)
{
    COLOR_PAIR *cp = NULL;
    char *pvalefg, *pvalebg;
    char *pvalmfg, *pvalmbg;

    pvalefg = PVAL(v, ew);
    pvalebg = PVAL(v+1, ew);
    pvalmfg = PVAL(v, Main);
    pvalmbg = PVAL(v+1, Main);
    if(v && color_holding_var(ps, v) &&
       srchstr(v->name, "-foreground-color")){
	if(pvalefg && pvalefg[0] && pvalebg && pvalebg[0])
	  cp = new_color_pair(pvalefg, pvalebg);
	else if(ew == Post && pvalmfg && pvalmfg[0] && pvalmbg && pvalmbg[0])
	  cp = new_color_pair(pvalmfg, pvalmbg);
	else if(v->global_val.p && v->global_val.p[0] &&
	        (v+1)->global_val.p && (v+1)->global_val.p[0])
	  cp = new_color_pair(v->global_val.p, (v+1)->global_val.p);
    }

    return(cp);
}


COLOR_PAIR *
sampleexc_color(struct pine *ps, struct variable *v)
{
    COLOR_PAIR *cp = NULL;
    char *pvalfg, *pvalbg;

    pvalfg = PVAL(v, Post);
    pvalbg = PVAL(v+1, Post);
    if(v && color_holding_var(ps, v) &&
       srchstr(v->name, "-foreground-color") &&
       pvalfg && pvalfg[0] && pvalbg && pvalbg[0])
	cp = new_color_pair(pvalfg, pvalbg);

    return(cp);
}


void
clear_feature(char ***l, char *f)
{
    char **list = l ? *l : NULL;
    int    count = 0;

    for(; list && *list; list++, count++){
	if(f && !strucmp(((!struncmp(*list,"no-",3)) ? *list + 3 : *list), f)){
	    fs_give((void **)list);
	    f = NULL;
	}

	if(!f)					/* shift  */
	  *list = *(list + 1);
    }

    /*
     * this is helpful to keep the array from growing if a feature
     * get's set and unset repeatedly
     */
    if(!f)
      fs_resize((void **)l, count * sizeof(char *));
}


/*
 *
 */
void
toggle_feature_bit(struct pine *ps, int index, struct variable *var, CONF_S *cl, int just_flip_value)
{
    FEATURE_S *f;
    int        og, on_before;
    char      *p, **vp;

    f = feature_list(index);

    og = test_old_growth_bits(ps, f->id);

    /*
     * if this feature is in the fixed set, or old-growth is in the fixed
     * set and this feature is in the old-growth set, don't alter it...
     */
    for(vp = var->fixed_val.l; vp && *vp; vp++){
	p = (struncmp(*vp, "no-", 3)) ? *vp : *vp + 3;
	if(!strucmp(p, f->name) || (og && !strucmp(p, "old-growth"))){
	    q_status_message(SM_ORDER, 3, 3,
			     _("Can't change value fixed by sys-admin."));
	    return;
	}
    }

    on_before = F_ON(f->id, ps);

    toggle_feature(ps, var, f, just_flip_value, ew);

    /*
     * Handle any alpine-specific features that need attention here. Features
     * that aren't alpine-specific should be handled in toggle_feature instead.
     */
    if(on_before != F_ON(f->id, ps))
      switch(f->id){
	case F_CMBND_ABOOK_DISP :
	  addrbook_reset();
	  break;

	case F_PRESERVE_START_STOP :
	  /* toggle raw mode settings to make tty driver aware of new setting */
	  PineRaw(0);
	  PineRaw(1);
	  break;

	case F_USE_FK :
	  ps->orig_use_fkeys = F_ON(F_USE_FK, ps);
	  ps->mangled_footer = 1;
          mark_keymenu_dirty();
	  break;

	case F_SHOW_SORT :
	  ps->mangled_header = 1;
	  break;

	case F_BLANK_KEYMENU :
	  if(F_ON(f->id, ps)){
	      FOOTER_ROWS(ps) = 1;
	      ps->mangled_body = 1;
	  }
	  else{
	      FOOTER_ROWS(ps) = 3;
	      ps->mangled_footer = 1;
	  }

          clearfooter(ps);
	  break;

	case F_ENABLE_INCOMING :
	  q_status_message(SM_ORDER | SM_DING, 3, 4,
	       "Folder List changes will take effect your next Alpine session.");
	  break;

#ifdef	_WINDOWS
	case F_SHOW_CURSOR :
	  mswin_showcaret(F_ON(f->id,ps));
	  break;

	case F_ENABLE_TRAYICON :
	  mswin_trayicon(F_ON(f->id,ps));
	  break;
#endif

#if !defined(DOS) && !defined(OS2)
	case F_ALLOW_TALK :
	  if(F_ON(f->id, ps))
	    allow_talk(ps);
	  else
	    disallow_talk(ps);

	  break;
#endif

	case F_PASS_CONTROL_CHARS :
	  ps->pass_ctrl_chars = F_ON(F_PASS_CONTROL_CHARS,ps_global) ? 1 : 0;
	  break;

	case F_PASS_C1_CONTROL_CHARS :
	  ps->pass_c1_ctrl_chars = F_ON(F_PASS_C1_CONTROL_CHARS,ps_global) ? 1 : 0;
	  break;

#ifdef	MOUSE
	case F_ENABLE_MOUSE :
	  if(F_ON(f->id, ps)){
	    init_mouse();
	    if(!mouseexist())
	      q_status_message(SM_ORDER | SM_DING, 3, 4,
			  "Mouse tracking still off ($DISPLAY variable set?)");
	  }
	  else
	    end_mouse();

	  break;
#endif
      }

    if(just_flip_value){
	if(cl->value && cl->value[0])
	  cl->value[1] = (cl->value[1] == ' ') ? 'X' : ' ';
    }
    else{
	/*
	 * This fork is only called from the checkbox_tool, which has
	 * varmem set to index correctly and cl->var set correctly.
	 */
	if(cl->value)
	  fs_give((void **)&cl->value);

	cl->value = pretty_value(ps, cl);
    }
}


/*
 * new_confline - create new CONF_S zero it out, and insert it after current.
 *                NOTE current gets set to the new CONF_S too!
 */
CONF_S *
new_confline(CONF_S **current)
{
    CONF_S *p;

    p = (CONF_S *)fs_get(sizeof(CONF_S));
    memset((void *)p, 0, sizeof(CONF_S));
    if(current){
	if(*current){
	    p->next	     = (*current)->next;
	    (*current)->next = p;
	    p->prev	     = *current;
	    if(p->next)
	      p->next->prev = p;
	}

	*current = p;
    }

    return(p);
}


/*
 *
 */
void
snip_confline(CONF_S **p)
{
    CONF_S *q;

    /*
     * Be careful. We need this line because the 
     *   q->prev->next = ...
     * may change q itself if &q == &q->prev->next.
     * Then the use of q in the next line is wrong.
     * That's what happens if we pass in the address of
     * some ->next and use *p directly instead of q.
     */
    q = *p;

    if(q){
	/* Yank it from the linked list */
	if(q->prev)
	  q->prev->next = q->next;

	if(q->next)
	  q->next->prev = q->prev;

	/* Then free up it's memory */
	q->prev = q->next = NULL;
	free_conflines(&q);
    }
}


/*
 *
 */
void
free_conflines(CONF_S **p)
{
    if(*p){
	free_conflines(&(*p)->next);

	if((*p)->varname)
	  fs_give((void **) &(*p)->varname);

	if((*p)->value)
	  fs_give((void **) &(*p)->value);

	fs_give((void **) p);
    }
}


/*
 *
 */
CONF_S *
first_confline(CONF_S *p)
{
    while(p && p->prev)
      p = p->prev;

    return(p);
}


/*
 * First selectable confline.
 */
CONF_S *
first_sel_confline(CONF_S *p)
{
    for(p = first_confline(p); p && (p->flags&CF_NOSELECT); p=next_confline(p))
      ;/* do nothing */

    return(p);
}


/*
 *
 */
CONF_S *
last_confline(CONF_S *p)
{
    while(p && p->next)
      p = p->next;

    return(p);
}


/*
 *
 */
int
fixed_var(struct variable *v, char *action, char *name)
{
    char **lval;

    if(v && v->is_fixed
       && (!v->is_list
           || ((lval=v->fixed_val.l) && lval[0]
	       && strcmp(INHERIT, lval[0]) != 0))){
	q_status_message2(SM_ORDER, 3, 3,
			  "Can't %s sys-admin defined %s.",
			  action ? action : "change", name ? name : "value");
	return(1);
    }

    return(0);
}


void
exception_override_warning(struct variable *v)
{
    char **lval;

    /* if exceptions config file exists and we're not editing it */
    if(v && (ps_global->ew_for_except_vars != Main) && (ew == Main)){
	if((!v->is_list && PVAL(v, ps_global->ew_for_except_vars)) ||
	   (v->is_list && (lval=LVAL(v, ps_global->ew_for_except_vars)) &&
	    lval[0] && strcmp(INHERIT, lval[0]) != 0))
	  q_status_message1(SM_ORDER, 3, 3,
	   _("Warning: \"%s\" is overridden in your exceptions configuration"),
	      v->name);
    }
}


void
offer_to_fix_pinerc(struct pine *ps)
{
    struct variable *v;
    char             prompt[300];
    char            *p, *q;
    char           **list;
    char           **list_fixed;
    int              rv = 0, write_main = 0, write_post = 0;
    int              i, k, j, need, exc;
    char            *clear = ": delete it";
    char          ***plist;

    dprint((4, "offer_to_fix_pinerc()\n"));

    ps->fix_fixed_warning = 0;  /* so we only ask first time */

    if(ps->readonly_pinerc)
      return;

    set_titlebar(_("FIXING PINERC"), ps->mail_stream,
		 ps->context_current,
		 ps->cur_folder, ps->msgmap, 1, FolderName, 0, 0, NULL);

    if(want_to(_("Some of your options conflict with site policy.  Investigate"),
	'y', 'n', NO_HELP, WT_FLUSH_IN) != 'y')
      return;
    
/* space want_to requires in addition to the string you pass in */
#define WANTTO_SPACE 6
    need = WANTTO_SPACE + utf8_width(clear);

    for(v = ps->vars; v->name; v++){
	if(!v->is_fixed ||
	   !v->is_user ||
	    v->is_obsolete ||
	    v == &ps->vars[V_FEATURE_LIST]) /* handle feature-list below */
	  continue;
	
	prompt[0] = '\0';
	
	if(v->is_list &&
	   (v->post_user_val.l || v->main_user_val.l)){
	    char **active_list;

	    active_list = v->post_user_val.l ? v->post_user_val.l
					     : v->main_user_val.l;
	    if(*active_list){
		snprintf(prompt, sizeof(prompt), _("Your setting for %s is "), v->name);
		prompt[sizeof(prompt)-1] = '\0';
		p = prompt + strlen(prompt);
		for(i = 0; active_list[i]; i++){
		    if(utf8_width(prompt) > ps->ttyo->screen_cols - need)
		      break;
		    if(i && sizeof(prompt)-(p-prompt) > 0)
		      *p++ = ',';

		    sstrncpy(&p, active_list[i], sizeof(prompt)-(p-prompt));
		    if(sizeof(prompt)-(p-prompt) > 0)
		      *p = '\0';

		    prompt[sizeof(prompt)-1] = '\0';
		}

		if(sizeof(prompt)-(p-prompt) > 0)
		  *p = '\0';
	    }
	    else
	      snprintf(prompt, sizeof(prompt), _("Your setting for %s is %s"), v->name, _(empty_val2));
	}
	else{
	    if(v->post_user_val.p || v->main_user_val.p){
		char *active_var;

		active_var = v->post_user_val.p ? v->post_user_val.p
						: v->main_user_val.p;
		if(*active_var){
		    snprintf(prompt, sizeof(prompt), _("Your setting for %s is %s"),
			v->name, active_var);
		}
		else{
		    snprintf(prompt, sizeof(prompt), _("Your setting for %s is %s"),
			v->name, _(empty_val2));
		}
	    }
	}

	prompt[sizeof(prompt)-1] = '\0';

	if(*prompt){
	    if(utf8_width(prompt) > ps->ttyo->screen_cols - need)
	      (void) utf8_truncate(prompt, ps->ttyo->screen_cols - need);

	    (void) strncat(prompt, clear, sizeof(prompt)-strlen(prompt)-1);
	    prompt[sizeof(prompt)-1] = '\0';
	    if(want_to(prompt, 'y', 'n', NO_HELP, WT_NORM) == 'y'){
		if(v->is_list){
		    if(v->main_user_val.l)
		      write_main++;
		    if(v->post_user_val.l)
		      write_post++;
		}
		else{
		    if(v->main_user_val.p)
		      write_main++;
		    if(v->post_user_val.p)
		      write_post++;
		}

		if(delete_user_vals(v))
		  rv++;
	    }
	}
    }


    /*
     * As always, feature-list has to be handled separately.
     */
    exc = (ps->ew_for_except_vars != Main);
    v = &ps->vars[V_FEATURE_LIST];
    list_fixed = v->fixed_val.l;

    for(j = 0; j < 2; j++){
      plist = (j==0) ? &v->main_user_val.l : &v->post_user_val.l;
      list = *plist;
      if(list){
        for(i = 0; list[i]; i++){
	  p = list[i];
	  if(!struncmp(p, "no-", 3))
	    p += 3;
	  for(k = 0; list_fixed && list_fixed[k]; k++){
	    q = list_fixed[k];
	    if(!struncmp(q, "no-", 3))
	      q += 3;
	    if(!strucmp(q, p) && strucmp(list[i], list_fixed[k])){
	      snprintf(prompt, sizeof(prompt), "Your %s is %s%s, fixed value is %s",
		  p, p == list[i] ? _("ON") : _("OFF"),
		  exc ? ((plist == &v->main_user_val.l) ? ""
						        : " in postload-config")
		      : "",
		  q == list_fixed[k] ? _("ON") : _("OFF"));

	      prompt[sizeof(prompt)-1] = '\0';
	      if(utf8_width(prompt) > ps->ttyo->screen_cols - need)
		(void) utf8_truncate(prompt, ps->ttyo->screen_cols - need);

	      (void) strncat(prompt, clear, sizeof(prompt)-strlen(prompt)-1);
	      prompt[sizeof(prompt)-1] = '\0';
	      if(want_to(prompt, 'y', 'n', NO_HELP, WT_NORM) == 'y'){
		  rv++;

		  if(plist == &v->main_user_val.l)
		    write_main++;
		  else
		    write_post++;

		  /*
		   * Clear the feature from the user's pinerc
		   * so that we'll stop bothering them when they
		   * start up Pine.
		   */
		  clear_feature(plist, p);

		  /*
		   * clear_feature scoots the list up, so if list[i] was
		   * the last one going in, now it is the end marker.  We
		   * just decrement i so that it will get incremented and
		   * then test == 0 in the for loop.  We could just goto
		   * outta_here to accomplish the same thing.
		   */
		  if(!list[i])
		    i--;
	      }
	    }
	  }
        }
      }
    }


    if(write_main)
      write_pinerc(ps, Main, WRP_NONE);
    if(write_post)
      write_pinerc(ps, Post, WRP_NONE);

    return;
}


/*
 * Adjust side effects that happen because variable changes values.
 *
 * Var->user_val should be set to the new value before calling this.
 */
void
fix_side_effects(struct pine *ps, struct variable *var, int revert)
{
    int    val = 0;
    char **v, *q, **apval;
    struct variable *vars = ps->vars;

    /* move this up here so we get the Using default message */
    if(var == &ps->vars[V_PERSONAL_NAME]){
	if(!(var->main_user_val.p ||
	     var->post_user_val.p) && ps->ui.fullname){
	    if(var->current_val.p)
	      fs_give((void **)&var->current_val.p);

	    var->current_val.p = cpystr(ps->ui.fullname);
	}
    }

    if(!revert
      && ((!var->is_fixed
	    && !var->is_list
	    && !(var->main_user_val.p ||
		 var->post_user_val.p)
	    && var->current_val.p)
	 ||
	 (!var->is_fixed
	    && var->is_list
	    && !(var->main_user_val.l ||
		 var->post_user_val.l)
	    && var->current_val.l)))
      q_status_message(SM_ORDER,0,3,_("Using default value"));

    if(var == &ps->vars[V_USER_DOMAIN]){
	char *p, *q;

	if(ps->VAR_USER_DOMAIN
	   && ps->VAR_USER_DOMAIN[0]
	   && (p = strrindex(ps->VAR_USER_DOMAIN, '@'))){
	    if(*(++p)){
		if(!revert)
		  q_status_message2(SM_ORDER, 3, 5,
		    _("User-Domain (%s) cannot contain \"@\"; using %s"),
		    ps->VAR_USER_DOMAIN, p);
		q = ps->VAR_USER_DOMAIN;
		while((*q++ = *p++) != '\0')
		  ;/* do nothing */
	    }
	    else{
		if(!revert)
		  q_status_message1(SM_ORDER, 3, 5,
		    _("User-domain (%s) cannot contain \"@\"; deleting"),
		    ps->VAR_USER_DOMAIN);

		if(ps->vars[V_USER_DOMAIN].post_user_val.p){
		    fs_give((void **)&ps->vars[V_USER_DOMAIN].post_user_val.p);
		    set_current_val(&vars[V_USER_DOMAIN], TRUE, TRUE);
		}

		if(ps->VAR_USER_DOMAIN
		   && ps->VAR_USER_DOMAIN[0]
		   && (p = strrindex(ps->VAR_USER_DOMAIN, '@'))){
		    if(ps->vars[V_USER_DOMAIN].main_user_val.p){
			fs_give((void **)&ps->vars[V_USER_DOMAIN].main_user_val.p);
			set_current_val(&vars[V_USER_DOMAIN], TRUE, TRUE);
		    }
		}
	    }
	}

	/*
	 * Reset various pointers pertaining to domain name and such...
	 */
	init_hostname(ps);
    }
    else if(var == &ps->vars[V_INBOX_PATH]){
	/*
	 * fixup the inbox path based on global/default values...
	 */
	init_inbox_mapping(ps->VAR_INBOX_PATH, ps->context_list);

	if(!strucmp(ps->cur_folder, ps->inbox_name) && ps->mail_stream
	   && strcmp(ps->VAR_INBOX_PATH, ps->mail_stream->mailbox)){
	    /*
	     * If we currently have "inbox" open and the mailbox name
	     * doesn't match, reset the current folder's name and
	     * remove the SP_INBOX flag.
	     */
	    strncpy(ps->cur_folder, ps->mail_stream->mailbox,
		    sizeof(ps->cur_folder)-1);
	    ps->cur_folder[sizeof(ps->cur_folder)-1] = '\0';
	    sp_set_fldr(ps->mail_stream, ps->cur_folder);
	    sp_unflag(ps->mail_stream, SP_INBOX);
	    ps->mangled_header = 1;
	}
	else if(sp_inbox_stream()
	    && strcmp(ps->VAR_INBOX_PATH, sp_inbox_stream()->original_mailbox)){
	    MAILSTREAM *m = sp_inbox_stream();

	    /*
	     * if we don't have inbox directly open, but have it
	     * open for new mail notification, close the stream like
	     * any other ordinary folder, and clean up...
	     */
	    if(m){
		sp_unflag(m, SP_PERMLOCKED | SP_INBOX);
		sp_set_fldr(m, m->mailbox);
		expunge_and_close(m, NULL, EC_NONE);
	    }
	}
    }
    else if(var == &ps->vars[V_INCCHECKTIMEO]){
	int old_value = ps->inc_check_timeout;

	if(SVAR_INC_CHECK_TIMEO(ps, old_value, tmp_20k_buf, SIZEOF_20KBUF)){
	    if(!revert)
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
	else
	  ps->inc_check_timeout = old_value;

	if(!revert && (F_OFF(F_ENABLE_INCOMING, ps) || F_OFF(F_ENABLE_INCOMING_CHECKING, ps)))
	  q_status_message(SM_ORDER, 0, 3, _("This option has no effect without Enable-Incoming-Folders-Checking"));
    }
    else if(var == &ps->vars[V_INCCHECKINTERVAL]){
	int old_value = ps->inc_check_interval;

	if(SVAR_INC_CHECK_INTERV(ps, old_value, tmp_20k_buf, SIZEOF_20KBUF)){
	    if(!revert)
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
	else
	  ps->inc_check_interval = old_value;

	if(!revert && (F_OFF(F_ENABLE_INCOMING, ps) || F_OFF(F_ENABLE_INCOMING_CHECKING, ps)))
	  q_status_message(SM_ORDER, 0, 3, _("This option has no effect without Enable-Incoming-Folders-Checking"));
    }
    else if(var == &ps->vars[V_INC2NDCHECKINTERVAL]){
	int old_value = ps->inc_second_check_interval;

	if(SVAR_INC_2NDCHECK_INTERV(ps, old_value, tmp_20k_buf, SIZEOF_20KBUF)){
	    if(!revert)
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
	else
	  ps->inc_second_check_interval = old_value;

	if(!revert && (F_OFF(F_ENABLE_INCOMING, ps) || F_OFF(F_ENABLE_INCOMING_CHECKING, ps)))
	  q_status_message(SM_ORDER, 0, 3, _("This option has no effect without Enable-Incoming-Folders-Checking"));
    }
    else if(var == &ps->vars[V_INCCHECKLIST]){
	if(ps->context_list && ps->context_list->use & CNTXT_INCMNG)
	  reinit_incoming_folder_list(ps, ps->context_list);

	if(!revert && (F_OFF(F_ENABLE_INCOMING, ps) || F_OFF(F_ENABLE_INCOMING_CHECKING, ps)))
	  q_status_message(SM_ORDER, 0, 3, _("This option has no effect without Enable-Incoming-Folders-Checking"));
    }
    else if(var == &ps->vars[V_ADDRESSBOOK] ||
	    var == &ps->vars[V_GLOB_ADDRBOOK] ||
#ifdef	ENABLE_LDAP
	    var == &ps->vars[V_LDAP_SERVERS] ||
#endif
	    var == &ps->vars[V_ABOOK_FORMATS]){
	addrbook_reset();
    }
    else if(var == &ps->vars[V_INDEX_FORMAT]){
	reset_index_format();
	clear_index_cache(ps->mail_stream, 0);
    }
    else if(var == &ps->vars[V_DEFAULT_FCC] ||
	    var == &ps->vars[V_DEFAULT_SAVE_FOLDER]){
	init_save_defaults();
    }
    else if(var == &ps->vars[V_KW_BRACES] ||
	    var == &ps->vars[V_OPENING_SEP] ||
	    var == &ps->vars[V_ALT_ADDRS]){
	clear_index_cache(ps->mail_stream, 0);
    }
    else if(var == &ps->vars[V_KEYWORDS]){
	if(ps_global->keywords)
	  free_keyword_list(&ps_global->keywords);
	
	if(var->current_val.l && var->current_val.l[0])
	  ps_global->keywords = init_keyword_list(var->current_val.l);

	clear_index_cache(ps->mail_stream, 0);
    }
    else if(var == &ps->vars[V_INIT_CMD_LIST]){
	if(!revert)
	  q_status_message(SM_ASYNC, 0, 3,
	    _("Initial command changes will affect your next Alpine session."));
    }
    else if(var == &ps->vars[V_VIEW_HEADERS]){
	ps->view_all_except = 0;
	if(ps->VAR_VIEW_HEADERS)
	  for(v = ps->VAR_VIEW_HEADERS; (q = *v) != NULL; v++)
	    if(q[0]){
		char *p;

		removing_leading_white_space(q);
		/* look for colon or space or end */
		for(p = q; *p && !isspace((unsigned char)*p) && *p != ':'; p++)
		  ;/* do nothing */
		
		*p = '\0';
		if(strucmp(q, ALL_EXCEPT) == 0)
		  ps->view_all_except = 1;
	    }
    }
    else if(var == &ps->vars[V_OVERLAP]){
	int old_value = ps->viewer_overlap;

	if(SVAR_OVERLAP(ps, old_value, tmp_20k_buf, SIZEOF_20KBUF)){
	    if(!revert)
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
	else
	  ps->viewer_overlap = old_value;
    }
#ifdef	SMIME
    else if(smime_related_var(ps, var)){
	smime_deinit();
    }
#endif	/* SMIME */
    else if(var == &ps->vars[V_MAXREMSTREAM]){
	int old_value = ps->s_pool.max_remstream;

	if(SVAR_MAXREMSTREAM(ps, old_value, tmp_20k_buf, SIZEOF_20KBUF)){
	    if(!revert )
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
	else
	  ps->s_pool.max_remstream = old_value;

	dprint((9, "max_remstream goes to %d\n",
		ps->s_pool.max_remstream));
    }
#ifndef	_WINDOWS
    else if(var == &ps->vars[V_CHAR_SET]){
	char *err = NULL;

	if(F_ON(F_USE_SYSTEM_TRANS, ps)){
	    if(!revert)
	      q_status_message(SM_ORDER, 5, 5, _("This change has no effect because feature Use-System-Translation is on"));
	}
	else{
	    if(reset_character_set_stuff(&err) == -1)
	      panic(err ? err : "trouble with Character-Set");
	    else if(err){
		q_status_message(SM_ORDER | SM_DING, 3, 5, err);
		fs_give((void **) &err);
	    }
	}
    }
    else if(var == &ps->vars[V_KEY_CHAR_SET]){
	char *err = NULL;

	if(F_ON(F_USE_SYSTEM_TRANS, ps)){
	    if(!revert)
	      q_status_message(SM_ORDER, 5, 5, _("This change has no effect because feature Use-System-Translation is on"));
	}
	else{
	    if(reset_character_set_stuff(&err) == -1)
	      panic(err ? err : "trouble with Character-Set");
	    else if(err){
		q_status_message(SM_ORDER | SM_DING, 3, 5, err);
		fs_give((void **) &err);
	    }
	}
    }
#endif	/* ! _WINDOWS */
    else if(var == &ps->vars[V_POST_CHAR_SET]){
	update_posting_charset(ps, revert);
    }
    else if(var == &ps->vars[V_MARGIN]){
	int old_value = ps->scroll_margin;

	if(SVAR_MARGIN(ps, old_value, tmp_20k_buf, SIZEOF_20KBUF)){
	    if(!revert)
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
	else
	  ps->scroll_margin = old_value;
    }
    else if(var == &ps->vars[V_DEADLETS]){
	int old_value = ps->deadlets;

	if(SVAR_DEADLETS(ps, old_value, tmp_20k_buf, SIZEOF_20KBUF)){
	    if(!revert)
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
	else
	  ps->deadlets = old_value;
    }
    else if(var == &ps->vars[V_FILLCOL]){
	if(SVAR_FILLCOL(ps, ps->composer_fillcol, tmp_20k_buf, SIZEOF_20KBUF)){
	    if(!revert)
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
    }
    else if(var == &ps->vars[V_QUOTE_SUPPRESSION]){
	val = ps->quote_suppression_threshold;
	if(val < Q_SUPP_LIMIT && val > 0)
	  val = -val;

	if(ps->VAR_QUOTE_SUPPRESSION
	   && SVAR_QUOTE_SUPPRESSION(ps, val, tmp_20k_buf, SIZEOF_20KBUF)){
	    if(!revert)
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
	else{
	    if(val > 0 && val < Q_SUPP_LIMIT){
		if(!revert){
		    snprintf(tmp_20k_buf, SIZEOF_20KBUF, _("Ignoring Quote-Suppression-Threshold value of %s, see help"), ps->VAR_QUOTE_SUPPRESSION);
		    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		    q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
		}
	    }
	    else{
		if(val < 0 && val != Q_DEL_ALL)
		  ps->quote_suppression_threshold = -val;
		else
		  ps->quote_suppression_threshold = val;
	    }
	}
    }
    else if(var == &ps->vars[V_STATUS_MSG_DELAY]){
	if(SVAR_MSGDLAY(ps, ps->status_msg_delay, tmp_20k_buf, SIZEOF_20KBUF)){
	    if(!revert)
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
    }
    else if(var == &ps->vars[V_ACTIVE_MSG_INTERVAL]){
	if(SVAR_ACTIVEINTERVAL(ps, ps->active_status_interval, tmp_20k_buf, SIZEOF_20KBUF)){
	    if(!revert)
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
	else{
	    busy_cue(_("Active Example"), NULL, 0);
	    sleep(5);
	    cancel_busy_cue(-1);
	}
    }
#if !defined(DOS) && !defined(OS2) && !defined(LEAVEOUTFIFO)
    else if(var == &ps->vars[V_FIFOPATH]){
	init_newmailfifo(ps->VAR_FIFOPATH);
    }
#endif
    else if(var == &ps->vars[V_NMW_WIDTH]){
	int old_value = ps->nmw_width;

	if(SVAR_NMW_WIDTH(ps, old_value, tmp_20k_buf, SIZEOF_20KBUF)){
	    if(!revert )
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
	else{
#ifdef _WINDOWS
	    if(old_value != ps->nmw_width)
	      mswin_setnewmailwidth(old_value);	/* actually the new value */
#endif
	    ps->nmw_width = old_value;
	}
    }
    else if(var == &ps->vars[V_TCPOPENTIMEO]){
	val = 30;
	if(!revert)
	  if(ps->VAR_TCPOPENTIMEO && SVAR_TCP_OPEN(ps, val, tmp_20k_buf, SIZEOF_20KBUF))
	    q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
    }
    else if(var == &ps->vars[V_TCPREADWARNTIMEO]){
	val = 15;
	if(!revert)
	  if(ps->VAR_TCPREADWARNTIMEO && SVAR_TCP_READWARN(ps,val,tmp_20k_buf, SIZEOF_20KBUF))
	    q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
    }
    else if(var == &ps->vars[V_TCPWRITEWARNTIMEO]){
	val = 0;
	if(!revert)
	 if(ps->VAR_TCPWRITEWARNTIMEO && SVAR_TCP_WRITEWARN(ps,val,tmp_20k_buf, SIZEOF_20KBUF))
	    q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
    }
    else if(var == &ps->vars[V_TCPQUERYTIMEO]){
	val = 60;
	if(!revert)
	  if(ps->VAR_TCPQUERYTIMEO && SVAR_TCP_QUERY(ps, val, tmp_20k_buf, SIZEOF_20KBUF))
	    q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
    }
    else if(var == &ps->vars[V_RSHOPENTIMEO]){
	val = 15;
	if(!revert)
	  if(ps->VAR_RSHOPENTIMEO && SVAR_RSH_OPEN(ps, val, tmp_20k_buf, SIZEOF_20KBUF))
	    q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
    }
    else if(var == &ps->vars[V_SSHOPENTIMEO]){
	val = 15;
	if(!revert)
	  if(ps->VAR_SSHOPENTIMEO && SVAR_SSH_OPEN(ps, val, tmp_20k_buf, SIZEOF_20KBUF))
	    q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
    }
    else if(var == &ps->vars[V_SIGNATURE_FILE]){
	if(ps->VAR_OPER_DIR && ps->VAR_SIGNATURE_FILE &&
	   is_absolute_path(ps->VAR_SIGNATURE_FILE) &&
	   !in_dir(ps->VAR_OPER_DIR, ps->VAR_SIGNATURE_FILE)){
	    char *e;
	    size_t l;

	    l = strlen(ps->VAR_OPER_DIR) + 100;
	    e = (char *) fs_get((l+1) * sizeof(char));
	    snprintf(e, l+1, _("Warning: Sig file can't be outside of %s"),
		    ps->VAR_OPER_DIR);
	    e[l] = '\0';
	    q_status_message(SM_ORDER, 3, 6, e);
	    fs_give((void **)&e);
	}
    }
    else if(var == &ps->vars[V_OPER_DIR]){
	if(ps->VAR_OPER_DIR && !ps->VAR_OPER_DIR[0]){
	    q_status_message(SM_ORDER, 3, 5, "Operating-dir is turned off.");
	    fs_give((void **)&ps->vars[V_OPER_DIR].current_val.p);
	    if(ps->vars[V_OPER_DIR].fixed_val.p)
	      fs_give((void **)&ps->vars[V_OPER_DIR].fixed_val.p);
	    if(ps->vars[V_OPER_DIR].global_val.p)
	      fs_give((void **)&ps->vars[V_OPER_DIR].global_val.p);
	    if(ps->vars[V_OPER_DIR].cmdline_val.p)
	      fs_give((void **)&ps->vars[V_OPER_DIR].cmdline_val.p);
	    if(ps->vars[V_OPER_DIR].post_user_val.p)
	      fs_give((void **)&ps->vars[V_OPER_DIR].post_user_val.p);
	    if(ps->vars[V_OPER_DIR].main_user_val.p)
	      fs_give((void **)&ps->vars[V_OPER_DIR].main_user_val.p);
	}
    }
    else if(var == &ps->vars[V_MAILCHECK]){
	int timeo = 15;
	if(SVAR_MAILCHK(ps, timeo, tmp_20k_buf, SIZEOF_20KBUF)){
	    set_input_timeout(15);
	    if(!revert)
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
	else{
	    set_input_timeout(timeo);
	    if(get_input_timeout() == 0 && !revert){
		q_status_message(SM_ORDER, 4, 6,
    _("Warning: automatic new mail checking and mailbox checkpointing is disabled"));
		if(ps->VAR_INBOX_PATH && ps->VAR_INBOX_PATH[0] == '{')
		  q_status_message(SM_ASYNC, 3, 6,
    _("Warning: Mail-Check-Interval=0 may cause IMAP server connection to time out"));
	    }
	}
    }
    else if(var == &ps->vars[V_MAILCHECKNONCURR]){
	val = (int) ps->check_interval_for_noncurr;
	if(ps->VAR_MAILCHECKNONCURR
	   && SVAR_MAILCHKNONCURR(ps, val, tmp_20k_buf, SIZEOF_20KBUF)){
	    if(!revert)
	      q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	}
	else
	  ps->check_interval_for_noncurr = (time_t) val;
    }
    else if(var == &ps->vars[V_MAILDROPCHECK]){
	long rvl;

	rvl = 60L;
	if(ps->VAR_MAILDROPCHECK && SVAR_MAILDCHK(ps, rvl, tmp_20k_buf, SIZEOF_20KBUF))
	  q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	else{
	    if(rvl == 0L)
	      rvl = (60L * 60L * 24L * 100L);	/* 100 days */

	    if(rvl >= 60L)
	      mail_parameters(NULL, SET_SNARFINTERVAL, (void *) rvl);
	}
    }
    else if(var == &ps->vars[V_NNTPRANGE]){
	long rvl;

	rvl = 0L;
	if(ps->VAR_NNTPRANGE && SVAR_NNTPRANGE(ps, rvl, tmp_20k_buf, SIZEOF_20KBUF))
	  q_status_message(SM_ORDER, 3, 5, tmp_20k_buf);
	else{
	    if(rvl >= 0L)
	      mail_parameters(NULL, SET_NNTPRANGE, (void *) rvl);
	}
    }
    else if(var == &ps->vars[V_CUSTOM_HDRS] || var == &ps->vars[V_COMP_HDRS]){
	/* this will give warnings about headers that can't be changed */
	if(!revert && var->current_val.l && var->current_val.l[0])
	  customized_hdr_setup(NULL, var->current_val.l, UseAsDef);
    }
#if defined(DOS) || defined(OS2)
    else if(var == &ps->vars[V_FOLDER_EXTENSION]){
	mail_parameters(NULL, SET_EXTENSION,
			(void *)var->current_val.p);
    }
    else if(var == &ps->vars[V_NEWSRC_PATH]){
	if(var->current_val.p && var->current_val.p[0])
	  mail_parameters(NULL, SET_NEWSRC,
			  (void *)var->current_val.p);
    }
#endif
    else if(revert && standard_radio_var(ps, var)){

	cur_rule_value(var, TRUE, FALSE);
	if(var == &ps_global->vars[V_AB_SORT_RULE])
	  addrbook_redo_sorts();
	else if(var == &ps_global->vars[V_THREAD_INDEX_STYLE]){
	    clear_index_cache(ps_global->mail_stream, 0);
	    set_lflags(ps_global->mail_stream, ps_global->msgmap,
		       MN_COLL | MN_CHID, 0);
	    if(SORT_IS_THREADED(ps_global->msgmap)
	       && (SEP_THRDINDX() || COLL_THRDS()))
	      collapse_threads(ps_global->mail_stream, ps_global->msgmap, NULL);

	    adjust_cur_to_visible(ps_global->mail_stream, ps_global->msgmap);
	}
#ifndef	_WINDOWS
	else if(var == &ps->vars[V_COLOR_STYLE]){
	    pico_toggle_color(0);
	    switch(ps->color_style){
	      case COL_NONE:
	      case COL_TERMDEF:
		pico_set_color_options(pico_trans_color() ? COLOR_TRANS_OPT : 0);
		break;
	      case COL_ANSI8:
		pico_set_color_options(COLOR_ANSI8_OPT|COLOR_TRANS_OPT);
		break;
	      case COL_ANSI16:
		pico_set_color_options(COLOR_ANSI16_OPT|COLOR_TRANS_OPT);
		break;
	      case COL_ANSI256:
		pico_set_color_options(COLOR_ANSI256_OPT|COLOR_TRANS_OPT);
		break;
	    }

	    if(ps->color_style != COL_NONE)
	      pico_toggle_color(1);
	
	    if(pico_usingcolor())
	      pico_set_normal_color();

	    clear_index_cache(ps_global->mail_stream, 0);
	    ClearScreen();
	    ps->mangled_screen = 1;
	}
#endif
    }
    else if(revert && var == &ps->vars[V_SORT_KEY]){
	int def_sort_rev;

	decode_sort(VAR_SORT_KEY, &ps->def_sort, &def_sort_rev);
	ps->def_sort_rev = def_sort_rev;
    }
    else if(var == &ps->vars[V_THREAD_MORE_CHAR] ||
            var == &ps->vars[V_THREAD_EXP_CHAR] ||
            var == &ps->vars[V_THREAD_LASTREPLY_CHAR]){

	if(var == &ps->vars[V_THREAD_LASTREPLY_CHAR] &&
	   !(var->current_val.p && var->current_val.p[0])){
	    if(var->current_val.p)
	      fs_give((void **) &var->current_val.p);
	    
	    q_status_message1(SM_ORDER, 3, 5,
		      _("\"%s\" can't be Empty, using default"), var->name);

	    apval = APVAL(var, ew);
	    if(*apval)
	      fs_give((void **)apval);

	    set_current_val(var, FALSE, FALSE);

	    if(!(var->current_val.p && var->current_val.p[0]
		 && !var->current_val.p[1])){
		if(var->current_val.p)
		  fs_give((void **) &var->current_val.p);
		
		var->current_val.p = cpystr(DF_THREAD_LASTREPLY_CHAR);
	    }
	}

	if(var == &ps->vars[V_THREAD_MORE_CHAR] ||
	   var == &ps->vars[V_THREAD_EXP_CHAR] ||
	   var == &ps->vars[V_THREAD_LASTREPLY_CHAR]){
	    if(var->current_val.p && var->current_val.p[0] &&
	       var->current_val.p[1]){
		q_status_message1(SM_ORDER, 3, 5,
			  "Only first character of \"%s\" is used",
			  var->name);
		var->current_val.p[1] = '\0';
	    }

	    if(var->main_user_val.p && var->main_user_val.p[0] &&
	       var->main_user_val.p[1])
	      var->main_user_val.p[1] = '\0';

	    if(var->post_user_val.p && var->post_user_val.p[0] &&
	       var->post_user_val.p[1])
	      var->post_user_val.p[1] = '\0';
	}

	clear_index_cache(ps_global->mail_stream, 0);
	set_need_format_setup(ps_global->mail_stream);
    }
    else if(var == &ps->vars[V_NNTP_SERVER]){
	    free_contexts(&ps_global->context_list);
	    init_folders(ps_global);
    }
    else if(var == &ps->vars[V_USE_ONLY_DOMAIN_NAME]){
	init_hostname(ps);
    }
    else if(var == &ps->vars[V_PRINTER]){
	if(!revert && ps->vars[V_PERSONAL_PRINT_COMMAND].is_fixed){
	    if(printer_value_check_and_adjust())
	      q_status_message1(SM_ORDER, 3, 5,
			        _("Can't set \"%s\" to that value, see Setup/Printer"),
			        pretty_var_name(var->name));
	}
    }
    else if(var == &ps->vars[V_KW_COLORS] ||
	    var == &ps->vars[V_IND_PLUS_FORE_COLOR] ||
	    var == &ps->vars[V_IND_IMP_FORE_COLOR]  ||
            var == &ps->vars[V_IND_DEL_FORE_COLOR]  ||
            var == &ps->vars[V_IND_ANS_FORE_COLOR]  ||
            var == &ps->vars[V_IND_NEW_FORE_COLOR]  ||
            var == &ps->vars[V_IND_UNS_FORE_COLOR]  ||
            var == &ps->vars[V_IND_HIPRI_FORE_COLOR]||
            var == &ps->vars[V_IND_LOPRI_FORE_COLOR]||
            var == &ps->vars[V_IND_ARR_FORE_COLOR]  ||
            var == &ps->vars[V_IND_REC_FORE_COLOR]  ||
            var == &ps->vars[V_IND_FWD_FORE_COLOR]  ||
	    var == &ps->vars[V_IND_OP_FORE_COLOR]   ||
	    var == &ps->vars[V_IND_FROM_FORE_COLOR] ||
	    var == &ps->vars[V_IND_SUBJ_FORE_COLOR] ||
            var == &ps->vars[V_IND_PLUS_BACK_COLOR] ||
            var == &ps->vars[V_IND_IMP_BACK_COLOR]  ||
            var == &ps->vars[V_IND_DEL_BACK_COLOR]  ||
            var == &ps->vars[V_IND_ANS_BACK_COLOR]  ||
            var == &ps->vars[V_IND_NEW_BACK_COLOR]  ||
            var == &ps->vars[V_IND_UNS_BACK_COLOR]  ||
            var == &ps->vars[V_IND_ARR_BACK_COLOR]  ||
            var == &ps->vars[V_IND_REC_BACK_COLOR]  ||
            var == &ps->vars[V_IND_FWD_BACK_COLOR]  ||
            var == &ps->vars[V_IND_OP_BACK_COLOR]   ||
            var == &ps->vars[V_IND_FROM_BACK_COLOR] ||
            var == &ps->vars[V_IND_SUBJ_BACK_COLOR]){
	clear_index_cache(ps_global->mail_stream, 0);
    }
    else if(var == score_act_global_ptr){
	int score;

	score = atoi(var->current_val.p);
	if(score < SCORE_MIN || score > SCORE_MAX){
	    q_status_message2(SM_ORDER, 3, 5,
			      _("Score Value must be in range %s to %s"),
			      comatose(SCORE_MIN), comatose(SCORE_MAX));
	    apval = APVAL(var, ew);
	    if(*apval)
	      fs_give((void **)apval);
	    
	    set_current_val(var, FALSE, FALSE);
	}
    }
    else if(var == scorei_pat_global_ptr || var == age_pat_global_ptr
	    || var == size_pat_global_ptr || var == cati_global_ptr){
	apval = APVAL(var, ew);
	if(*apval){
	    INTVL_S *iv;
	    iv = parse_intvl(*apval);
	    if(iv){
		fs_give((void **) apval);
		*apval = stringform_of_intvl(iv);
		free_intvl(&iv);
	    }
	    else
	      fs_give((void **) apval);
	}

	set_current_val(var, FALSE, FALSE);
    }
    else if(var == &ps->vars[V_FEATURE_LIST]){
	process_feature_list(ps, var->current_val.l, 0, 0, 0);
    }
    else if(!revert && (var == &ps->vars[V_LAST_TIME_PRUNE_QUESTION] ||
		        var == &ps->vars[V_REMOTE_ABOOK_HISTORY] ||
		        var == &ps->vars[V_REMOTE_ABOOK_VALIDITY] ||
		        var == &ps->vars[V_USERINPUTTIMEO] ||
		        var == &ps->vars[V_NEWS_ACTIVE_PATH] ||
		        var == &ps->vars[V_NEWS_SPOOL_DIR] ||
		        var == &ps->vars[V_INCOMING_FOLDERS] ||
		        var == &ps->vars[V_FOLDER_SPEC] ||
		        var == &ps->vars[V_NEWS_SPEC] ||
		        var == &ps->vars[V_DISABLE_DRIVERS] ||
		        var == &ps->vars[V_DISABLE_AUTHS] ||
		        var == &ps->vars[V_RSHPATH] ||
		        var == &ps->vars[V_RSHCMD] ||
		        var == &ps->vars[V_SSHCMD] ||
		        var == &ps->vars[V_SSHPATH])){
	q_status_message2(SM_ASYNC, 0, 3,
		      _("Changes%s%s will affect your next Alpine session."),
			  var->name ? " to " : "", var->name ? var->name : "");
    }

    if(!revert && (var == &ps->vars[V_TCPOPENTIMEO] ||
		   var == &ps->vars[V_TCPREADWARNTIMEO] ||
		   var == &ps->vars[V_TCPWRITEWARNTIMEO] ||
		   var == &ps->vars[V_TCPQUERYTIMEO] ||
		   var == &ps->vars[V_RSHOPENTIMEO] ||
		   var == &ps->vars[V_SSHOPENTIMEO]))
      q_status_message(SM_ASYNC, 0, 3,
		       _("Timeout changes will affect your next Alpine session."));
}


/*
 * Compare saved user_val with current user_val to see if it changed.
 * If any have changed, change it back and take the appropriate action.
 */
void
revert_to_saved_config(struct pine *ps, SAVED_CONFIG_S *vsave, int allow_hard_to_config_remotely)
{
    struct variable *vreal;
    SAVED_CONFIG_S  *v;
    int i, n;
    int changed = 0;
    char *pval, **apval, **lval, ***alval;

    v = vsave;
    for(vreal = ps->vars; vreal->name; vreal++,v++){
	if(!save_include(ps, vreal, allow_hard_to_config_remotely))
	  continue;

	changed = 0;
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
}


SAVED_CONFIG_S *
save_config_vars(struct pine *ps, int allow_hard_to_config_remotely)
{
    struct variable *vreal;
    SAVED_CONFIG_S *vsave, *v;

    vsave = (SAVED_CONFIG_S *)fs_get((V_LAST_VAR+1)*sizeof(SAVED_CONFIG_S));
    memset((void *)vsave, 0, (V_LAST_VAR+1)*sizeof(SAVED_CONFIG_S));
    v = vsave;
    for(vreal = ps->vars; vreal->name; vreal++,v++){
	if(!save_include(ps, vreal, allow_hard_to_config_remotely))
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
free_saved_config(struct pine *ps, SAVED_CONFIG_S **vsavep, int allow_hard_to_config_remotely)
{
    struct variable *vreal;
    SAVED_CONFIG_S  *v;

    if(vsavep && *vsavep){
	for(v = *vsavep, vreal = ps->vars; vreal->name; vreal++,v++){
	    if(!save_include(ps, vreal, allow_hard_to_config_remotely))
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


/*
 * Returns positive if any thing was actually deleted.
 */
int
delete_user_vals(struct variable *v)
{
    int rv = 0;

    if(v){
	if(v->is_list){
	    if(v->post_user_val.l){
		rv++;
		free_list_array(&v->post_user_val.l);
	    }
	    if(v->main_user_val.l){
		rv++;
		free_list_array(&v->main_user_val.l);
	    }
	}
	else{
	    if(v->post_user_val.p){
		rv++;
		fs_give((void **)&v->post_user_val.p);
	    }
	    if(v->main_user_val.p){
		rv++;
		fs_give((void **)&v->main_user_val.p);
	    }
	}
    }

    return(rv);
}


/*
 * ../pith/conf.c required function
 */
int
unexpected_pinerc_change(void)
{
    Writechar(BELL, 0);
    if(want_to("Unexpected pinerc change!  Overwrite with current config",
	       'n', 0, NO_HELP, WT_FLUSH_IN) == 'n'){
	return(-1);		/* abort pinerc write */
    }

    return(0);			/* overwrite */
}


#ifdef	_WINDOWS

/*----------------------------------------------------------------------
     MSWin scroll callback.  Called during scroll message processing.
	     


  Args: cmd - what type of scroll operation.
	scroll_pos - paramter for operation.  
			used as position for SCROLL_TO operation.

  Returns: TRUE - did the scroll operation.
	   FALSE - was not able to do the scroll operation.
 ----*/
int
config_scroll_callback (cmd, scroll_pos)
int	cmd;
long	scroll_pos;
{   
    switch (cmd) {
      case MSWIN_KEY_SCROLLUPLINE:
	config_scroll_down (scroll_pos);
	break;

      case MSWIN_KEY_SCROLLDOWNLINE:
	config_scroll_up (scroll_pos);
	break;

      case MSWIN_KEY_SCROLLUPPAGE:
	config_scroll_down (BODY_LINES(ps_global));
	break;

      case MSWIN_KEY_SCROLLDOWNPAGE:
	config_scroll_up (BODY_LINES(ps_global));
	break;

      case MSWIN_KEY_SCROLLTO:
	config_scroll_to_pos (scroll_pos);
	break;
    }

    option_screen_redrawer();
    fflush(stdout);

    return(TRUE);
}

#endif	/* _WINDOWS */
