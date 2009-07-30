#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: newuser.c 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $";
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
#include "newuser.h"
#include "mailview.h"
#include "help.h"
#include "keymenu.h"
#include "osdep/print.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/msgno.h"
#include "../pith/filter.h"
#include "../pith/init.h"
#include "../pith/margin.h"


/*
 * Internal prototypes
 */
int   nuov_processor(int, MSGNO_S *, SCROLL_S *);


/*
 * Display a new user or new version message.
 */
void
new_user_or_version(struct pine *ps)
{
    char	  **shown_text;
    int		    cmd = MC_NONE;
    int             first_time_alpine_user = 0;
    char	   *error = NULL;
    HelpType	    text;
    SCROLL_S	    sargs;
    STORE_S	   *store;
    HANDLE_S	   *handles = NULL, *htmp;
    gf_io_t	    pc;
    char           *vers = ps->vers_internal;

    first_time_alpine_user = (ps->first_time_user
			      || (ps->pine_pre_vers
				  && isdigit((unsigned char) ps->pine_pre_vers[0])
				  && ps->pine_pre_vers[1] == '.'
				  && isdigit((unsigned char) vers[0])
				  && vers[1] == '.'
				  && ps->pine_pre_vers[0] < '5'	/* it was Pine */
				  && vers[0] >= '5'));		/* Alpine */

    text = ps->first_time_user    ? new_user_greeting :
            first_time_alpine_user ? new_alpine_user_greeting : new_version_greeting;

    shown_text = text;

    /*
     * Set it if the major revision number
     * (the first after the dot) has changed.
     */
    ps->phone_home = (first_time_alpine_user
		      || (ps->pine_pre_vers
			  && isdigit((unsigned char) ps->pine_pre_vers[0])
			  && ps->pine_pre_vers[1] == '.'
			  && isdigit((unsigned char) ps->pine_pre_vers[2])
			  && isdigit((unsigned char) vers[0])
			  && vers[1] == '.'
			  && isdigit((unsigned char) vers[2])
			  && strncmp(ps->pine_pre_vers, vers, 3) < 0));

    /*
     * At this point, shown_text is a charstarstar with html
     * Turn it into a charstar with digested html
     */
    do{
	init_helper_getc(shown_text);
	init_handles(&handles);

	if((store = so_get(CharStar, NULL, EDIT_ACCESS)) != NULL){
	    gf_set_so_writec(&pc, store);
	    gf_filter_init();

	    gf_link_filter(gf_html2plain,
			   gf_html2plain_opt("x-alpine-help:",
					     ps->ttyo->screen_cols, non_messageview_margin(),
					     &handles, NULL, GFHP_LOCAL_HANDLES));

	    error = gf_pipe(helper_getc, pc);

	    gf_clear_so_writec(store);

	    if(!error){
		struct key_menu km;
		struct key	keys[24];

		for(htmp = handles; htmp; htmp = htmp->next)
		  if(htmp->type == URL
		     && htmp->h.url.path
		     && (htmp->h.url.path[0] == 'x'
			 || htmp->h.url.path[0] == '#'))
		    htmp->force_display = 1;

		/* This is mostly here to get the curses variables
		 * for line and column in sync with where the
		 * cursor is on the screen. This gets warped when
		 * the composer is called because it does it's own
		 * stuff
		 */
		ClearScreen();

		memset(&sargs, 0, sizeof(SCROLL_S));
		sargs.text.text	   = so_text(store);
		sargs.text.src	   = CharStar;
		sargs.text.desc	   = "greeting text";
		sargs.text.handles = handles;
		sargs.bar.title	   = "GREETING TEXT";
		sargs.bar.style	   = TextPercent;
		sargs.proc.tool	   = nuov_processor;
		sargs.help.text	   = main_menu_tx;
		sargs.help.title   = "MAIN PINE HELP";
		sargs.resize_exit  = 1;
		sargs.keys.menu	   = &km;
		km		   = nuov_keymenu;
		km.keys		   = keys;
		memcpy(&keys[0], nuov_keymenu.keys,
		       (nuov_keymenu.how_many * 12) * sizeof(struct key));
		setbitmap(sargs.keys.bitmap);

		if(ps->phone_home){
		    km.keys[NUOV_EXIT].label = "Exit this greeting";
		    km.keys[NUOV_EXIT].bind.nch = 1;
		}
		else{
		    km.keys[NUOV_EXIT].label	= "[Exit this greeting]";
		    km.keys[NUOV_EXIT].bind.nch = 3;
		    clrbitn(NUOV_VIEW, sargs.keys.bitmap);
		}

		if(ps->first_time_user)
		  clrbitn(NUOV_RELNOTES, sargs.keys.bitmap);

		cmd = scrolltool(&sargs);

		flush_input();

		if(F_ON(F_BLANK_KEYMENU,ps_global))
		  FOOTER_ROWS(ps_global) = 1;

		ClearScreen();
	    }

	    so_give(&store);
	}

	free_handles(&handles);
    }
    while(cmd == MC_RESIZE);
}


int
nuov_processor(int cmd, MSGNO_S *msgmap, SCROLL_S *sparms)
{
    int rv = 0;

    switch(cmd){
	/*----------- Print all the help ------------*/
      case MC_PRINTMSG :
	if(open_printer(sparms->text.desc) == 0){
	    print_help(sparms->proc.data.p);
	    close_printer();
	}

	break;

      case MC_RELNOTES :
	helper(h_news, "ALPINE RELEASE NOTES", 0);
	ps_global->mangled_screen = 1;
	break;

      case MC_EXIT :
	rv = 1;
	break;

      default :
	panic("Unhandled case");
    }

    return(rv);
}
