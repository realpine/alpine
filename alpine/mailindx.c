#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: mailindx.c 1142 2008-08-13 17:22:21Z hubert@u.washington.edu $";
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
#include "mailindx.h"
#include "mailcmd.h"
#include "status.h"
#include "context.h"
#include "keymenu.h"
#include "alpine.h"
#include "help.h"
#include "radio.h"
#include "titlebar.h"
#include "../pith/flag.h"
#include "../pith/newmail.h"
#include "../pith/thread.h"
#include "../pith/conf.h"
#include "../pith/msgno.h"
#include "../pith/icache.h"
#include "../pith/state.h"
#include "../pith/bitmap.h"
#include "../pith/news.h"
#include "../pith/strlst.h"
#include "../pith/sequence.h"
#include "../pith/sort.h"
#include "../pith/hist.h"
#include "../pith/busy.h"
#include "../pith/signal.h"


struct save_thrdinfo {
    ICE_S    *(*format_index_line)(INDEXDATA_S *);
    void      (*setup_header_widths)(MAILSTREAM *);
    unsigned  viewing_a_thread:1;
};


static OtherMenu what_keymenu = FirstMenu;

struct index_state *current_index_state = NULL;


/*
 * Internal prototypes
 */
void	 index_index_screen(struct pine *);
void	 thread_index_screen(struct pine *);
int	 update_index(struct pine *, struct index_state *);
int	 index_scroll_up(long);
int	 index_scroll_down(long);
int	 index_scroll_to_pos(long);
long	 top_ent_calc(MAILSTREAM *, MSGNO_S *, long, long);
void	 reset_index_border(void);
void	 redraw_index_body(void);
int	 paint_index_line(ICE_S *, int, long, IndexColType, IndexColType, IndexColType,
			  struct entry_state *, int, int);
void	 pine_imap_envelope(MAILSTREAM *, unsigned long, ENVELOPE *);
void     index_search(struct pine *, MAILSTREAM *, int, MSGNO_S *);
#ifdef	_WINDOWS
int	 index_scroll_callback(int,long);
int	 index_gettext_callback(char *, size_t, void **, long *, int *);
void	 index_popup(IndexType style, MAILSTREAM *, MSGNO_S *, int);
char	*pcpine_help_index(char *);
char	*pcpine_help_index_simple(char *);
#endif



/*----------------------------------------------------------------------


  ----*/
struct key_menu *
do_index_border(CONTEXT_S *cntxt, char *folder, MAILSTREAM *stream, MSGNO_S *msgmap,
		IndexType style, int *which_keys, int flags)
{
    struct key_menu *km = (style == ThreadIndex)
			    ? &thread_keymenu
			    : (ps_global->mail_stream != stream)
			      ? &simple_index_keymenu
			      : &index_keymenu;

    if(flags & INDX_CLEAR)
      ClearScreen();

    if(flags & INDX_HEADER)
      set_titlebar((style == ThreadIndex)
		     /* TRANSLATORS: these are some screen titles */
		     ? _("THREAD INDEX")
		     : (stream == ps_global->mail_stream)
		       ? (style == MsgIndex || style == MultiMsgIndex)
		         ? _("MESSAGE INDEX")
			 : _("ZOOMED MESSAGE INDEX")
		       : (!strcmp(folder, INTERRUPTED_MAIL))
			 ? _("COMPOSE: SELECT INTERRUPTED")
			 : (ps_global->VAR_FORM_FOLDER
			    && !strcmp(ps_global->VAR_FORM_FOLDER, folder))
			     ? _("COMPOSE: SELECT FORM LETTER")
			     : _("COMPOSE: SELECT POSTPONED"),
		   stream, cntxt, folder, msgmap, 1,
		   (style == ThreadIndex) ? ThrdIndex
					  : (THREADING()
					     && sp_viewing_a_thread(stream))
					    ? ThrdMsgNum
					    : MessageNumber,
		   0, 0, NULL);

    if(flags & INDX_FOOTER) {
	bitmap_t bitmap;
	int	 cmd;

	setbitmap(bitmap);

	if(km == &index_keymenu){
	    if(THREADING() && sp_viewing_a_thread(stream)){
		menu_init_binding(km, '<', MC_THRDINDX, "<",
				  N_("ThrdIndex"), BACK_KEY);
		menu_add_binding(km, ',', MC_THRDINDX);
	    }
	    else{
		menu_init_binding(km, '<', MC_FOLDERS, "<",
				  N_("FldrList"), BACK_KEY);
		menu_add_binding(km, ',', MC_FOLDERS);
	    }
	    if(F_OFF(F_ENABLE_PIPE,ps_global))
	      clrbitn(VIEW_PIPE_KEY, bitmap);  /* always clear for DOS */
	    if(F_OFF(F_ENABLE_FULL_HDR,ps_global))
	      clrbitn(VIEW_FULL_HEADERS_KEY, bitmap);
	    if(F_OFF(F_ENABLE_BOUNCE,ps_global))
	      clrbitn(BOUNCE_KEY, bitmap);
	    if(F_OFF(F_ENABLE_FLAG,ps_global))
	      clrbitn(FLAG_KEY, bitmap);
	    if(F_OFF(F_ENABLE_AGG_OPS,ps_global)){
		clrbitn(SELECT_KEY, bitmap);
		clrbitn(APPLY_KEY, bitmap);
		clrbitn(SELCUR_KEY, bitmap);
		if(style != ZoomIndex)
		  clrbitn(ZOOM_KEY, bitmap);

	    }

	    if(style == MultiMsgIndex){
		clrbitn(PREVM_KEY, bitmap);
		clrbitn(NEXTM_KEY, bitmap);
	    }
	}

	if(km == &index_keymenu || km == &thread_keymenu){
	    if(IS_NEWS(stream)){
		km->keys[EXCLUDE_KEY].label = N_("eXclude");
		KS_OSDATASET(&km->keys[EXCLUDE_KEY], KS_NONE);
	    }
	    else {
		clrbitn(UNEXCLUDE_KEY, bitmap);
		km->keys[EXCLUDE_KEY].label = N_("eXpunge");
		KS_OSDATASET(&km->keys[EXCLUDE_KEY], KS_EXPUNGE);
	    }
	}

	if(km != &simple_index_keymenu && !THRD_COLLAPSE_ENABLE())
	  clrbitn(COLLAPSE_KEY, bitmap);

	menu_clear_binding(km, KEY_LEFT);
	menu_clear_binding(km, KEY_RIGHT);
	if(F_ON(F_ARROW_NAV, ps_global)){
	    if((cmd = menu_clear_binding(km, '<')) != MC_UNKNOWN){
		menu_add_binding(km, '<', cmd);
		menu_add_binding(km, KEY_LEFT, cmd);
	    }

	    if((cmd = menu_clear_binding(km, '>')) != MC_UNKNOWN){
		menu_add_binding(km, '>', cmd);
		menu_add_binding(km, KEY_RIGHT, cmd);
	    }
	}

	if(menu_binding_index(km, MC_JUMP) >= 0){
	    for(cmd = 0; cmd < 10; cmd++)
	      if(F_ON(F_ENABLE_JUMP, ps_global))
		(void) menu_add_binding(km, '0' + cmd, MC_JUMP);
	      else
		(void) menu_clear_binding(km, '0' + cmd);
	}

        draw_keymenu(km, bitmap, ps_global->ttyo->screen_cols,
		     1-FOOTER_ROWS(ps_global), 0, what_keymenu);
	what_keymenu = SameMenu;
	if(which_keys)
	  *which_keys = km->which;  /* pass back to caller */
    }

    return(km);
}

      
    
/*----------------------------------------------------------------------
        Main loop executing commands for the mail index screen

   Args: state -- the pine_state structure for next/prev screen pointers
                  and to pass to the index manager...
 ----*/

void
mail_index_screen(struct pine *state)
{
    if(!state->mail_stream) {
	q_status_message(SM_ORDER, 0, 3, _("No folder is currently open"));
        state->prev_screen = mail_index_screen;
	state->next_screen = main_menu_screen;
	return;
    }

    state->prev_screen = mail_index_screen;
    state->next_screen = SCREEN_FUN_NULL;

    if(THRD_AUTO_VIEW()
       && sp_viewing_a_thread(state->mail_stream)
       && state->view_skipped_index
       && unview_thread(state, state->mail_stream, state->msgmap)){
	state->next_screen = mail_index_screen;
	state->view_skipped_index = 0;
	state->mangled_screen = 1;
    }

    adjust_cur_to_visible(state->mail_stream, state->msgmap);

    if(THRD_INDX())
      thread_index_screen(state);
    else
      index_index_screen(state);
}


void
index_index_screen(struct pine *state)
{
    dprint((1, "\n\n ---- MAIL INDEX ----\n"));

    setup_for_index_index_screen();

    index_lister(state, state->context_current, state->cur_folder,
		 state->mail_stream, state->msgmap);
}


void
thread_index_screen(struct pine *state)
{
    dprint((1, "\n\n ---- THREAD INDEX ----\n"));

    setup_for_thread_index_screen();

    index_lister(state, state->context_current, state->cur_folder,
		 state->mail_stream, state->msgmap);
}


void *
stop_threading_temporarily(void)
{
    struct save_thrdinfo *ti;

    ps_global->turn_off_threading_temporarily = 1;

    ti = (struct save_thrdinfo *) fs_get(sizeof(*ti));
    ti->format_index_line = format_index_line;
    ti->setup_header_widths = setup_header_widths;
    ti->viewing_a_thread = sp_viewing_a_thread(ps_global->mail_stream);

    setup_for_index_index_screen();

    return((void *) ti);
}


void
restore_threading(void **p)
{
    struct save_thrdinfo *ti;

    ps_global->turn_off_threading_temporarily = 0;

    if(p && *p){
	ti = (struct save_thrdinfo *) (*p);
	format_index_line = ti->format_index_line;
	setup_header_widths = ti->setup_header_widths;
	sp_set_viewing_a_thread(ps_global->mail_stream, ti->viewing_a_thread);

	fs_give(p);
    }
}


/*----------------------------------------------------------------------
        Main loop executing commands for the mail index screen

   Args: state -- pine_state structure for display flags and such
         msgmap -- c-client/pine message number mapping struct
 ----*/

int
index_lister(struct pine *state, CONTEXT_S *cntxt, char *folder, MAILSTREAM *stream, MSGNO_S *msgmap)
{
    UCS          ch;
    int		 cmd, which_keys, force,
		 cur_row, cur_col, km_popped, paint_status;
    int          old_day = -1;
    long	 i, j, k, old_max_msgno;
    char        *utf8str;
    IndexType    style, old_style = MsgIndex;
    struct index_state id;
    struct key_menu *km = NULL;


    dprint((1, "\n\n ---- INDEX MANAGER ----\n"));
    
    ch                    = 'x';	/* For displaying msg 1st time thru */
    force                 = 0;
    km_popped             = 0;
    state->mangled_screen = 1;
    what_keymenu          = FirstMenu;
    old_max_msgno         = mn_get_total(msgmap);
    memset((void *)&id, 0, sizeof(struct index_state));
    current_index_state   = &id;
    id.msgmap		  = msgmap;
    if(msgmap->top != 0L)
      id.msg_at_top = msgmap->top;

    id.stream = stream;
    set_need_format_setup(stream);

    while (1) {
	ps_global->user_says_cancel = 0;

	if(km_popped){
	    km_popped--;
	    if(km_popped == 0){
		clearfooter(state);
		if(!state->mangled_body
		   && id.entry_state
		   && id.lines_per_page > 1){
		    id.entry_state[id.lines_per_page-2].id = 0;
		    id.entry_state[id.lines_per_page-1].id = 0;
		}
		else
		  state->mangled_body = 1;
	    }
	}

	/*------- Check for new mail -------*/
        new_mail(force, NM_TIMING(ch), NM_STATUS_MSG);
	force = 0;			/* may not need to next time around */

	/*
	 * If the width of the message number field in the display changes
	 * we need to flush the cache and redraw. When the cache is cleared
	 * the widths are recalculated, taking into account the max msgno.
	 */

	if(format_includes_msgno(stream) &&
	   ((old_max_msgno < 1000L && mn_get_total(msgmap) >= 1000L)
	    || (old_max_msgno < 10000L && mn_get_total(msgmap) >= 10000L)
	    || (old_max_msgno < 100000L && mn_get_total(msgmap) >= 100000L))){
	    clear_index_cache(stream, IC_CLEAR_WIDTHS_DONE);
	    state->mangled_body = 1;
        }

	old_max_msgno = mn_get_total(msgmap);

	/*
	 * If the display includes the SMARTDATE ("Today", "Yesterday", ...)
	 * then when the day changes the date column will change. All of the
	 * Today's will become Yesterday's at midnight. So we have to
	 * clear the cache at midnight.
	 */
	if(format_includes_smartdate(stream)){
	    char        db[200];
	    struct date nnow;

	    rfc822_date(db);
	    parse_date(db, &nnow);
	    if(old_day != -1 && nnow.day != old_day){
		clear_index_cache(stream, IC_CLEAR_WIDTHS_DONE);
		state->mangled_body = 1;
	    }

	    old_day = nnow.day;
	}

        if(streams_died())
          state->mangled_header = 1;

        if(state->mangled_screen){
            state->mangled_header = 1;
            state->mangled_body   = 1;
            state->mangled_footer = 1;
            state->mangled_screen = 0;
        }

	/*
	 * events may have occured that require us to shift from
	 * mode to another...
	 */
	style = THRD_INDX()
		  ? ThreadIndex
		  : (any_lflagged(msgmap, MN_HIDE))
		    ? ZoomIndex
		    : (mn_total_cur(msgmap) > 1L) ? MultiMsgIndex : MsgIndex;
	if(style != old_style){
            state->mangled_header = 1;
            state->mangled_footer = 1;
	    old_style = style;
	    if(!(style == ThreadIndex || old_style == ThreadIndex))
	      id.msg_at_top = 0L;
	}

        /*------------ Update the title bar -----------*/
	if(state->mangled_header) {
	    km = do_index_border(cntxt, folder, stream, msgmap,
				 style, NULL, INDX_HEADER);
	    state->mangled_header = 0;
	    paint_status = 0;
	} 
	else if(mn_get_total(msgmap) > 0) {
	    update_titlebar_message();
	    /*
	     * If flags aren't available to update the status,
	     * defer it until after all the fetches associated
	     * with building index lines are done (no extra rtts!)...
	     */
	    paint_status = !update_titlebar_status();
	}

	current_index_state = &id;

        /*------------ draw the index body ---------------*/
	cur_row = update_index(state, &id);
	if(F_OFF(F_SHOW_CURSOR, state)){
	    cur_row = state->ttyo->screen_rows - FOOTER_ROWS(state);
	    cur_col = 0;
	}
	else if(id.status_col >= 0)
	  cur_col = MIN(id.status_col, state->ttyo->screen_cols-1);

        ps_global->redrawer = redraw_index_body;

	if(paint_status)
	  (void) update_titlebar_status();

        /*------------ draw the footer/key menus ---------------*/
	if(state->mangled_footer) {
            if(!state->painted_footer_on_startup){
		if(km_popped){
		    FOOTER_ROWS(state) = 3;
		    clearfooter(state);
		}

		km = do_index_border(cntxt, folder, stream, msgmap, style,
				     &which_keys, INDX_FOOTER);
		if(km_popped){
		    FOOTER_ROWS(state) = 1;
		    mark_keymenu_dirty();
		}
	    }

	    state->mangled_footer = 0;
	}

        state->painted_body_on_startup   = 0;
        state->painted_footer_on_startup = 0;

	/*-- Display any queued message (eg, new mail, command result --*/
	if(km_popped){
	    FOOTER_ROWS(state) = 3;
	    mark_status_unknown();
	}

        display_message(ch);
	if(km_popped){
	    FOOTER_ROWS(state) = 1;
	    mark_status_unknown();
	}

	if(F_ON(F_SHOW_CURSOR, state) && cur_row < 0){
	    cur_row = state->ttyo->screen_rows - FOOTER_ROWS(state);
	}

	cur_row = MIN(MAX(cur_row, 0), state->ttyo->screen_rows-1);
	MoveCursor(cur_row, cur_col);

        /* Let read_command do the fflush(stdout) */

        /*---------- Read command and validate it ----------------*/
#ifdef	MOUSE
	mouse_in_content(KEY_MOUSE, -1, -1, 0x5, 0);
	register_mfunc(mouse_in_content, HEADER_ROWS(ps_global), 0,
		       state->ttyo->screen_rows-(FOOTER_ROWS(ps_global)+1),
		       state->ttyo->screen_cols);
#endif
#ifdef	_WINDOWS
	mswin_setscrollcallback (index_scroll_callback);
	mswin_sethelptextcallback((stream == state->mail_stream)
				    ? pcpine_help_index
				    : pcpine_help_index_simple);
	mswin_setviewinwindcallback(view_in_new_window);
#endif
	ch = READ_COMMAND(&utf8str);
#ifdef	MOUSE
	clear_mfunc(mouse_in_content);
#endif
#ifdef	_WINDOWS
	mswin_setscrollcallback(NULL);
	mswin_sethelptextcallback(NULL);
	mswin_setviewinwindcallback(NULL);
#endif

	cmd = menu_command(ch, km);

	if(km_popped)
	  switch(cmd){
	    case MC_NONE :
	    case MC_OTHER :
	    case MC_RESIZE :
	    case MC_REPAINT :
	      km_popped++;
	      break;

	    default:
	      clearfooter(state);
	      break;
	  }

	/*----------- Execute the command ------------------*/
	switch(cmd){

            /*---------- Roll keymenu ----------*/
	  case MC_OTHER :
	    if(F_OFF(F_USE_FK, ps_global))
	      warn_other_cmds();

	    what_keymenu = NextMenu;
	    state->mangled_footer = 1;
	    break;


            /*---------- Scroll line up ----------*/
	  case MC_CHARUP :
	    (void) process_cmd(state, stream, msgmap, MC_PREVITEM,
			       (style == MsgIndex
				|| style == MultiMsgIndex
				|| style == ZoomIndex)
				   ? MsgIndx
				   : (style == ThreadIndex)
				     ? ThrdIndx
				     : View,
			       &force);
	    if(mn_get_cur(msgmap) < (id.msg_at_top + HS_MARGIN(state)))
	      index_scroll_up(1L);

	    break;


            /*---------- Scroll line down ----------*/
	  case MC_CHARDOWN :
	    /*
	     * Special Page framing handling here.  If we
	     * did something that should scroll-by-a-line, frame
	     * the page by hand here rather than leave it to the
	     * page-by-page framing in update_index()...
	     */
	    (void) process_cmd(state, stream, msgmap, MC_NEXTITEM,
			       (style == MsgIndex
				|| style == MultiMsgIndex
				|| style == ZoomIndex)
				   ? MsgIndx
				   : (style == ThreadIndex)
				     ? ThrdIndx
				     : View,
			       &force);
	    for(j = 0L, k = i = id.msg_at_top; ; i++){
		if(!msgline_hidden(stream, msgmap, i, 0)){
		    k = i;
		    if(j++ >= id.lines_per_page)
		      break;
		}

		if(i >= mn_get_total(msgmap)){
		    k = 0L;		/* don't scroll */
		    break;
		}
	    }

	    if(k && (mn_get_cur(msgmap) + HS_MARGIN(state)) >= k)
	      index_scroll_down(1L);

	    break;


            /*---------- Scroll page up ----------*/
	  case MC_PAGEUP :
	    j = -1L;
	    for(k = i = id.msg_at_top; ; i--){
		if(!msgline_hidden(stream, msgmap, i, 0)){
		    k = i;
		    if(++j >= id.lines_per_page){
			if((id.msg_at_top = i) == 1L)
			  q_status_message(SM_ORDER, 0, 1, _("First Index page"));

			break;
		    }
	        }

		if(i <= 1L){
		    if((!THREADING() && mn_get_cur(msgmap) == 1L)
		       || (THREADING()
		           && mn_get_cur(msgmap) == first_sorted_flagged(F_NONE,
								         stream,
									 0L,
							        FSF_SKIP_CHID)))
		      q_status_message(SM_ORDER, 0, 1,
			  _("Already at start of Index"));

		    break;
		}
	    }

	    if(mn_get_total(msgmap) > 0L && mn_total_cur(msgmap) == 1L)
	      mn_set_cur(msgmap, k);

	    break;


            /*---------- Scroll page forward ----------*/
	  case MC_PAGEDN :
	    j = -1L;
	    for(k = i = id.msg_at_top; ; i++){
		if(!msgline_hidden(stream, msgmap, i, 0)){
		    k = i;
		    if(++j >= id.lines_per_page){
			if(i+id.lines_per_page > mn_get_total(msgmap))
			  q_status_message(SM_ORDER, 0, 1, _("Last Index page"));

			id.msg_at_top = i;
			break;
		    }
		}

		if(i >= mn_get_total(msgmap)){
		    if(mn_get_cur(msgmap) == k)
		      q_status_message(SM_ORDER,0,1,_("Already at end of Index"));

		    break;
		}
	    }

	    if(mn_get_total(msgmap) > 0L && mn_total_cur(msgmap) == 1L)
	      mn_set_cur(msgmap, k);

	    break;


            /*---------- Scroll to first page ----------*/
	  case MC_HOMEKEY :
	    if((mn_get_total(msgmap) > 0L)
	       && (mn_total_cur(msgmap) <= 1L)){
		long cur_msg = mn_get_cur(msgmap), selected;

		if(any_lflagged(msgmap, MN_HIDE | MN_CHID)){
		    do {
			selected = cur_msg;
			mn_dec_cur(stream, msgmap, MH_NONE);
			cur_msg = mn_get_cur(msgmap);
		    }
		    while(selected != cur_msg);
		}
		else
		  cur_msg = (mn_get_total(msgmap) > 0L) ? 1L : 0L;
		mn_set_cur(msgmap, cur_msg);
		q_status_message(SM_ORDER, 0, 3, _("First Index Page"));
	    }
	    break;

            /*---------- Scroll to last page ----------*/
	  case MC_ENDKEY :
	    if((mn_get_total(msgmap) > 0L)
	       && (mn_total_cur(msgmap) <= 1L)){
		long cur_msg = mn_get_cur(msgmap), selected;

		if(any_lflagged(msgmap, MN_HIDE | MN_CHID)){
		    do {
			selected = cur_msg;
			mn_inc_cur(stream, msgmap, MH_NONE);
			cur_msg = mn_get_cur(msgmap);
		    }
		    while(selected != cur_msg);
		}
		else
		  cur_msg = mn_get_total(msgmap);
		mn_set_cur(msgmap, cur_msg);
		q_status_message(SM_ORDER, 0, 3, _("Last Index Page"));
	    }
	    break;

	    /*---------- Search (where is command) ----------*/
	  case MC_WHEREIS :
	    index_search(state, stream, -FOOTER_ROWS(ps_global), msgmap);
	    state->mangled_footer = 1;
	    break;


            /*-------------- jump command -------------*/
	    /* NOTE: preempt the process_cmd() version because
	     *	     we need to get at the number..
	     */
	  case MC_JUMP :
	    j = jump_to(msgmap, -FOOTER_ROWS(ps_global), ch, NULL,
			(style == ThreadIndex) ? ThrdIndx : MsgIndx);
	    if(j > 0L){
		if(style == ThreadIndex){
		    PINETHRD_S *thrd;

		    thrd = find_thread_by_number(stream, msgmap, j, NULL);

		    if(thrd && thrd->rawno)
		      mn_set_cur(msgmap, mn_raw2m(msgmap, thrd->rawno));
		}
		else{
		    /* jump to message */
		    if(mn_total_cur(msgmap) > 1L){
			mn_reset_cur(msgmap, j);
		    }
		    else{
			mn_set_cur(msgmap, j);
		    }
		}

		id.msg_at_top = 0L;
	    }

	    state->mangled_footer = 1;
	    break;


	  case MC_VIEW_ENTRY :		/* only happens in thread index */

	    /*
	     * If the feature F_THRD_AUTO_VIEW is turned on and there
	     * is only one message in the thread, then we skip the index
	     * view of the thread and go straight to the message view.
	     */
view_a_thread:
	    if(THRD_AUTO_VIEW() && style == ThreadIndex){
		PINETHRD_S *thrd;

		thrd = fetch_thread(stream,
				    mn_m2raw(msgmap, mn_get_cur(msgmap)));
		if(thrd
		   && (count_lflags_in_thread(stream, thrd,
		       msgmap, MN_NONE) == 1)){
		    if(view_thread(state, stream, msgmap, 1)){
			state->view_skipped_index = 1;
			cmd = MC_VIEW_TEXT;
			goto do_the_default;
		    }
		}
	    }

	    if(view_thread(state, stream, msgmap, 1)){
		ps_global->next_screen = mail_index_screen;
		ps_global->redrawer = NULL;
		current_index_state = NULL;
		if(id.entry_state)
		  fs_give((void **)&(id.entry_state));

		return(0);
	    }

	    break;


	  case MC_THRDINDX :
	    msgmap->top = msgmap->top_after_thrd;
	    if(unview_thread(state, stream, msgmap)){
		state->next_screen = mail_index_screen;
		state->view_skipped_index = 0;
		state->mangled_screen = 1;
		ps_global->redrawer = NULL;
		current_index_state = NULL;
		if(id.entry_state)
		  fs_give((void **)&(id.entry_state));

		return(0);
	    }

	    break;


#ifdef MOUSE	    
	  case MC_MOUSE:
	    {
	      MOUSEPRESS mp;
	      int	 new_cur;

	      mouse_get_last (NULL, &mp);
	      mp.row -= 2;

	      for(i = id.msg_at_top;
		  mp.row >= 0 && i <= mn_get_total(msgmap);
		  i++)
		if(!msgline_hidden(stream, msgmap, i, 0)){
		    mp.row--;
		    new_cur = i;
		}

	      if(mn_get_total(msgmap) && mp.row < 0){
		  switch(mp.button){
		    case M_BUTTON_LEFT :
		      if(mn_total_cur(msgmap) == 1L)
			mn_set_cur(msgmap, new_cur);

		      if(mp.flags & M_KEY_CONTROL){
			  if(F_ON(F_ENABLE_AGG_OPS, ps_global)){
			      (void) select_by_current(state, msgmap, MsgIndx);
			  }
		      }
		      else if(!(mp.flags & M_KEY_SHIFT)){
			  if (THREADING()
			      && mp.col >= 0
			      && mp.col == id.plus_col
			      && style != ThreadIndex){
			      collapse_or_expand(state, stream, msgmap,
						 mn_get_cur(msgmap));
			  }
			  else if (mp.doubleclick){
			      if(mp.button == M_BUTTON_LEFT){
				  if(stream == state->mail_stream){
				      if(THRD_INDX()){
					  cmd = MC_VIEW_ENTRY;
					  goto view_a_thread;
				      }
				      else{
					  cmd = MC_VIEW_TEXT;
					  goto do_the_default;
				      }
				  }

				  ps_global->redrawer = NULL;
				  current_index_state = NULL;
				  if(id.entry_state)
				    fs_give((void **)&(id.entry_state));

				  return(0);
			      }
			  }
		      }

		      break;

		    case M_BUTTON_MIDDLE:
		      break;

		    case M_BUTTON_RIGHT :
#ifdef _WINDOWS
		      if (!mp.doubleclick){
			  if(mn_total_cur(msgmap) == 1L)
			    mn_set_cur(msgmap, new_cur);

			  cur_row = update_index(state, &id);

			  index_popup(style, stream, msgmap, TRUE);
		      }
#endif
		      break;
		  }
	      }
	      else{
		  switch(mp.button){
		    case M_BUTTON_LEFT :
		      break;

		    case M_BUTTON_MIDDLE :
		      break;

		    case M_BUTTON_RIGHT :
#ifdef	_WINDOWS
		      index_popup(style, stream, msgmap, FALSE);
#endif
		      break;
		  }
	      }
	    }

	    break;
#endif	/* MOUSE */

            /*---------- Resize ----------*/
          case MC_RESIZE:
	    /*
	     * If we were smarter we could do the
	     * IC_CLEAR_WIDTHS_DONE trick here. The problem is
	     * that entire columns of the format can go away or
	     * appear because the width gets smaller or larger,
	     * so in that case we need to re-do. If we could tell
	     * when that happened or not we could set the flag
	     * selectively.
	     */
	    clear_index_cache(stream, 0);
	    reset_index_border();
	    break;


            /*---------- Redraw ----------*/
	  case MC_REPAINT :
	    force = 1;			/* check for new mail! */
	    reset_index_border();
	    break;


            /*---------- No op command ----------*/
          case MC_NONE :
            break;	/* no op check for new mail */


	    /*--------- keystroke not bound to command --------*/
	  case MC_CHARRIGHT :
	  case MC_CHARLEFT :
	  case MC_GOTOBOL :
	  case MC_GOTOEOL :
	  case MC_UNKNOWN :
	    if(cmd == MC_UNKNOWN && (ch == 'i' || ch == 'I'))
	      q_status_message(SM_ORDER, 0, 1, "Already in Index");
	    else
	      bogus_command(ch, F_ON(F_USE_FK,state) ? "F1" : "?");

	    break;


	  case MC_COLLAPSE :
	    thread_command(state, stream, msgmap, ch, -FOOTER_ROWS(state));
	    break;

          case MC_DELETE :
          case MC_UNDELETE :
          case MC_REPLY :
          case MC_FORWARD :
          case MC_TAKE :
          case MC_SAVE :
          case MC_EXPORT :
          case MC_BOUNCE :
          case MC_PIPE :
          case MC_FLAG :
          case MC_SELCUR :
	    { int collapsed = 0;
	      unsigned long rawno;
	      PINETHRD_S *thrd = NULL;

	      if(THREADING()){
		  rawno = mn_m2raw(msgmap, mn_get_cur(msgmap));
		  if(rawno)
		    thrd = fetch_thread(stream, rawno);

		  collapsed = thrd && thrd->next
			      && get_lflag(stream, NULL, rawno, MN_COLL);
	      }

	      if(collapsed){
		  thread_command(state, stream, msgmap,
				 ch, -FOOTER_ROWS(state));
		  /* increment current */
		  if(cmd == MC_DELETE){
		      advance_cur_after_delete(state, stream, msgmap,
					       (style == MsgIndex
						  || style == MultiMsgIndex
						  || style == ZoomIndex)
						    ? MsgIndx
						    : (style == ThreadIndex)
						      ? ThrdIndx
						      : View);
		  }
		  else if((cmd == MC_SELCUR
			   && (state->ugly_consider_advancing_bit
			       || F_OFF(F_UNSELECT_WONT_ADVANCE, state)))
			  || (state->ugly_consider_advancing_bit
			      && cmd == MC_SAVE
			      && F_ON(F_SAVE_ADVANCES, state))){
		      mn_inc_cur(stream, msgmap, MH_NONE);
		  }
	      }
	      else
		goto do_the_default;
	    }

            break;


	  case MC_UTF8:
	    bogus_utf8_command(utf8str, NULL);
	    break;


            /*---------- First HELP command with menu hidden ----------*/
	  case MC_HELP :
	    if(FOOTER_ROWS(state) == 1 && km_popped == 0){
		km_popped = 2;
		mark_status_unknown();
		mark_keymenu_dirty();
		state->mangled_footer = 1;
		break;
	    }
	    /* else fall thru to normal default */


            /*---------- Default -- all other command ----------*/
          default:
   do_the_default:
	    if(stream == state->mail_stream){
		msgmap->top = id.msg_at_top;
		process_cmd(state, stream, msgmap, cmd,
			    (style == MsgIndex
			     || style == MultiMsgIndex
			     || style == ZoomIndex)
			       ? MsgIndx
			       : (style == ThreadIndex)
				 ? ThrdIndx
				 : View,
			    &force);
		if(state->next_screen != SCREEN_FUN_NULL){
		    ps_global->redrawer = NULL;
		    current_index_state = NULL;
		    if(id.entry_state)
		      fs_give((void **)&(id.entry_state));

		    return(0);
		}
		else{
		    if(stream != state->mail_stream){
			/*
			 * Must have had an failed open.  repair our
			 * pointers...
			 */
			id.stream = stream = state->mail_stream;
			id.msgmap = msgmap = state->msgmap;
		    }

		    current_index_state = &id;

		    if(cmd == MC_ZOOM && THRD_INDX())
		      id.msg_at_top = 0L;
		}
	    }
	    else{			/* special processing */
		switch(cmd){
		  case MC_HELP :
		    helper(h_simple_index,
			   (!strcmp(folder, INTERRUPTED_MAIL))
			     ? _("HELP FOR SELECTING INTERRUPTED MSG")
			     : _("HELP FOR SELECTING POSTPONED MSG"),
			   HLPD_SIMPLE);
		    state->mangled_screen = 1;
		    break;

		  case MC_DELETE :	/* delete */
		    dprint((3, "Special delete: msg %s\n",
			      long2string(mn_get_cur(msgmap))));
		    {
			long	      raw, t;
			int	      del = 0;
			MESSAGECACHE *mc;

			raw = mn_m2raw(msgmap, mn_get_cur(msgmap));
			if(raw > 0L && stream
			   && raw <= stream->nmsgs
			   && (mc = mail_elt(stream, raw))
			   && !mc->deleted){
			    if((t = mn_get_cur(msgmap)) > 0L)
			      clear_index_cache_ent(stream, t, 0);

			    mail_setflag(stream,long2string(raw),"\\DELETED");
			    update_titlebar_status();
			    del++;
			}

			q_status_message1(SM_ORDER, 0, 1,
					  del ? _("Message %s deleted") : _("Message %s already deleted"),
					  long2string(mn_get_cur(msgmap)));
		    }

		    break;

		  case MC_UNDELETE :	/* UNdelete */
		    dprint((3, "Special UNdelete: msg %s\n",
			      long2string(mn_get_cur(msgmap))));
		    {
			long	      raw, t;
			int	      del = 0;
			MESSAGECACHE *mc;

			raw = mn_m2raw(msgmap, mn_get_cur(msgmap));
			if(raw > 0L && stream
			   && raw <= stream->nmsgs
			   && (mc = mail_elt(stream, raw))
			   && mc->deleted){
			    if((t = mn_get_cur(msgmap)) > 0L)
			      clear_index_cache_ent(stream, t, 0);

			    mail_clearflag(stream, long2string(raw),
					   "\\DELETED");
			    update_titlebar_status();
			    del++;
			}

			q_status_message1(SM_ORDER, 0, 1,
					  del ? _("Message %s UNdeleted") : _("Message %s NOT deleted"),
					  long2string(mn_get_cur(msgmap)));
		    }

		    break;

		  case MC_EXIT :	/* exit */
		    ps_global->redrawer = NULL;
		    current_index_state = NULL;
		    if(id.entry_state)
		      fs_give((void **)&(id.entry_state));

		    return(1);

		  case MC_SELECT :	/* select */
		    ps_global->redrawer = NULL;
		    current_index_state = NULL;
		    if(id.entry_state)
		      fs_give((void **)&(id.entry_state));

		    return(0);

		  case MC_PREVITEM :		/* previous */
		    mn_dec_cur(stream, msgmap, MH_NONE);
		    break;

		  case MC_NEXTITEM :		/* next */
		    mn_inc_cur(stream, msgmap, MH_NONE);
		    break;

		  default :
		    bogus_command(ch, NULL);
		    break;
		}
	    }
	}				/* The big switch */
    }					/* the BIG while loop! */
}



/*----------------------------------------------------------------------
  Manage index body painting

  Args: state - pine struct containing selected message data
	index_state - struct describing what's currently displayed

  Returns: screen row number of first highlighted message

  The idea is pretty simple.  Maintain an array of index line id's that
  are displayed and their hilited state.  Decide what's to be displayed
  and update the screen appropriately.  All index screen painting
  is done here.  Pretty simple, huh?
 ----*/
int
update_index(struct pine *state, struct index_state *screen)
{
    int  i, retval = -1, row, already_fetched = 0;
    long n, visible;
    PINETHRD_S *thrd = NULL;
    int  we_cancel = 0;

    dprint((7, "--update_index--\n"));

    if(!screen)
      return(-1);
    
#ifdef _WINDOWS
    mswin_beginupdate();
#endif

    /*---- reset the works if necessary ----*/
    if(state->mangled_body){
	ClearBody();
	if(screen->entry_state){
	    fs_give((void **)&(screen->entry_state));
	    screen->lines_per_page = 0;
	}
    }

    state->mangled_body = 0;

    /*---- make sure we have a place to write state ----*/
    if(screen->lines_per_page
	!= MAX(0, state->ttyo->screen_rows - FOOTER_ROWS(state)
					   - HEADER_ROWS(state))){
	i = screen->lines_per_page;
	screen->lines_per_page
	    = MAX(0, state->ttyo->screen_rows - FOOTER_ROWS(state)
					      - HEADER_ROWS(state));
	if(!i){
	    size_t len = screen->lines_per_page * sizeof(struct entry_state);
	    screen->entry_state = (struct entry_state *) fs_get(len);
	}
	else
	  fs_resize((void **)&(screen->entry_state),
		    (size_t)screen->lines_per_page);

	for(; i < screen->lines_per_page; i++)	/* init new entries */
	  memset(&screen->entry_state[i], 0, sizeof(struct entry_state));
    }

    /*---- figure out the first message on the display ----*/
    if(screen->msg_at_top < 1L
       || msgline_hidden(screen->stream, screen->msgmap, screen->msg_at_top,0)){
	screen->msg_at_top = top_ent_calc(screen->stream, screen->msgmap,
					  screen->msg_at_top,
					  screen->lines_per_page);
    }
    else if(mn_get_cur(screen->msgmap) < screen->msg_at_top){
	long i, j, k;

	/* scroll back a page at a time until current is displayed */
	while(mn_get_cur(screen->msgmap) < screen->msg_at_top){
	    for(i = screen->lines_per_page, j = screen->msg_at_top-1L, k = 0L;
		i > 0L && j > 0L;
		j--)
	      if(!msgline_hidden(screen->stream, screen->msgmap, j, 0)){
		  k = j;
		  i--;
	      }

	    if(i == screen->lines_per_page)
	      break;				/* can't scroll back ? */
	    else
	      screen->msg_at_top = k;
	}
    }
    else if(mn_get_cur(screen->msgmap) >= screen->msg_at_top
						     + screen->lines_per_page){
	long i, j, k;

	while(1){
	    for(i = screen->lines_per_page, j = k = screen->msg_at_top;
		j <= mn_get_total(screen->msgmap) && i > 0L;
		j++)
	      if(!msgline_hidden(screen->stream, screen->msgmap, j, 0)){
		  k = j;
		  i--;
		  if(mn_get_cur(screen->msgmap) <= k)
		    break;
	      }

	    if(mn_get_cur(screen->msgmap) <= k)
	      break;
	    else{
		/* set msg_at_top to next displayed message */
		for(i = k + 1L; i <= mn_get_total(screen->msgmap); i++)
		  if(!msgline_hidden(screen->stream, screen->msgmap, i, 0)){
		      k = i;
		      break;
		  }

		screen->msg_at_top = k;
	    }
	}
    }

#ifdef	_WINDOWS
    /*
     * Set scroll range and position.  Note that message numbers start at 1
     * while scroll position starts at 0.
     */

    if(THREADING() && sp_viewing_a_thread(screen->stream)
       && mn_get_total(screen->msgmap) > 1L){
	long x = 0L, range = 0L, lowest_numbered_msg;

	/*
	 * We know that all visible messages in the thread are marked
	 * with MN_CHID2.
	 */
	thrd = fetch_thread(screen->stream,
			    mn_m2raw(screen->msgmap,
				     mn_get_cur(screen->msgmap)));
	if(thrd && thrd->top && thrd->top != thrd->rawno)
	  thrd = fetch_thread(screen->stream, thrd->top);

	if(thrd){
	    if(mn_get_revsort(screen->msgmap)){
		n = mn_raw2m(screen->msgmap, thrd->rawno);
		while(n > 1L && get_lflag(screen->stream, screen->msgmap,
					  n-1L, MN_CHID2))
		  n--;

		lowest_numbered_msg = n;
	    }
	    else
	      lowest_numbered_msg = mn_raw2m(screen->msgmap, thrd->rawno);
	}
	
	if(thrd){
	    n = lowest_numbered_msg;
	    for(; n <= mn_get_total(screen->msgmap); n++){

		if(!get_lflag(screen->stream, screen->msgmap, n, MN_CHID2))
		  break;
		
		if(!msgline_hidden(screen->stream, screen->msgmap, n, 0)){
		    range++;
		    if(n < screen->msg_at_top)
		      x++;
		}
	    }
	}

	scroll_setrange(screen->lines_per_page, range-1L);
	scroll_setpos(x);
    }
    else if(THRD_INDX()){
	if(any_lflagged(screen->msgmap, MN_HIDE)){
	    long x = 0L, range;

	    range = screen->msgmap->visible_threads - 1L;
	    scroll_setrange(screen->lines_per_page, range);
	    if(range >= screen->lines_per_page){	/* else not needed */
		PINETHRD_S *topthrd;
		int         thrddir;
		long        xdir;

		/* find top of currently displayed top line */
		topthrd = fetch_thread(screen->stream,
				       mn_m2raw(screen->msgmap,
					        screen->msg_at_top));
		if(topthrd && topthrd->top != topthrd->rawno)
		  topthrd = fetch_thread(screen->stream, topthrd->top);
		
		if(topthrd){
		    /*
		     * Split into two halves to speed up finding scroll pos.
		     * It's tricky because the thread list always goes from
		     * past to future but the thrdno's will be reversed if
		     * the sort is reversed and of course the order on the
		     * screen will be reversed.
		     */
		    if((!mn_get_revsort(screen->msgmap)
		        && topthrd->thrdno <= screen->msgmap->max_thrdno/2)
		       ||
		       (mn_get_revsort(screen->msgmap)
		        && topthrd->thrdno > screen->msgmap->max_thrdno/2)){

			/* start with head of thread list */
			if(topthrd && topthrd->head)
			  thrd = fetch_thread(screen->stream, topthrd->head);
			else
			  thrd = NULL;

			thrddir = 1;
		    }
		    else{
			long tailrawno;

			/*
			 * Start with tail thread and work back.
			 */
		        if(mn_get_revsort(screen->msgmap))
			  tailrawno = mn_m2raw(screen->msgmap, 1L);
			else
			  tailrawno = mn_m2raw(screen->msgmap,
					       mn_get_total(screen->msgmap));

			thrd = fetch_thread(screen->stream, tailrawno);
			if(thrd && thrd->top && thrd->top != thrd->rawno)
			  thrd = fetch_thread(screen->stream, thrd->top);

			thrddir = -1;
		    }

		    /*
		     * x is the scroll position. We try to use the fewest
		     * number of steps to find it, so we start with either
		     * the beginning or the end.
		     */
		    if(topthrd->thrdno <= screen->msgmap->max_thrdno/2){
			x = 0L;
			xdir = 1L;
		    }
		    else{
			x = range;
			xdir = -1L;
		    }

		    while(thrd && thrd != topthrd){
			if(!msgline_hidden(screen->stream, screen->msgmap,
					   mn_raw2m(screen->msgmap,thrd->rawno),
					   0))
			  x += xdir;
			
			if(thrddir > 0 && thrd->nextthd)
			  thrd = fetch_thread(screen->stream, thrd->nextthd);
			else if(thrddir < 0 && thrd->prevthd)
			  thrd = fetch_thread(screen->stream, thrd->prevthd);
			else
			  thrd = NULL;
		    }
		}

		scroll_setpos(x);
	    }
	}
	else{
	    /*
	     * This works for forward or reverse sort because the thrdno's
	     * will have been reversed.
	     */
	    thrd = fetch_thread(screen->stream,
				mn_m2raw(screen->msgmap, screen->msg_at_top));
	    if(thrd){
		scroll_setrange(screen->lines_per_page,
				screen->msgmap->max_thrdno - 1L);
		scroll_setpos(thrd->thrdno - 1L);
	    }
	}
    }
    else if(n = any_lflagged(screen->msgmap, MN_HIDE | MN_CHID)){
	long x, range;

	range = mn_get_total(screen->msgmap) - n - 1L;
	scroll_setrange(screen->lines_per_page, range);

	if(range >= screen->lines_per_page){	/* else not needed */
	    if(screen->msg_at_top < mn_get_total(screen->msgmap) / 2){
		for(n = 1, x = 0; n != screen->msg_at_top; n++)
		  if(!msgline_hidden(screen->stream, screen->msgmap, n, 0))
		    x++;
	    }
	    else{
		for(n = mn_get_total(screen->msgmap), x = range;
		    n != screen->msg_at_top; n--)
		  if(!msgline_hidden(screen->stream, screen->msgmap, n, 0))
		    x--;
	    }

	    scroll_setpos(x);
	}
    }
    else{
	scroll_setrange(screen->lines_per_page,
			mn_get_total(screen->msgmap) - 1L);
	scroll_setpos(screen->msg_at_top - 1L);
    }
#endif

    /*
     * Set up c-client call back to tell us about IMAP envelope arrivals
     * Can't do it (easily) if single lines on the screen need information
     * about more than a single message before they can be drawn.
     */
    if(F_OFF(F_QUELL_IMAP_ENV_CB, ps_global) && !THRD_INDX()
       && !(THREADING() && (sp_viewing_a_thread(screen->stream)
                            || ps_global->thread_disp_style == THREAD_MUTTLIKE
                            || any_lflagged(screen->msgmap, MN_COLL))))
      mail_parameters(NULL, SET_IMAPENVELOPE, (void *) pine_imap_envelope);

    if(THRD_INDX())
      visible = screen->msgmap->visible_threads;
    else if(THREADING() && sp_viewing_a_thread(screen->stream)){
	/*
	 * We know that all visible messages in the thread are marked
	 * with MN_CHID2.
	 */
	for(visible = 0L, n = screen->msg_at_top;
	    visible < (int) screen->lines_per_page
	    && n <= mn_get_total(screen->msgmap); n++){

	    if(!get_lflag(screen->stream, screen->msgmap, n, MN_CHID2))
	      break;
	    
	    if(!msgline_hidden(screen->stream, screen->msgmap, n, 0))
	      visible++;
	}
    }
    else
      visible = mn_get_total(screen->msgmap)
		  - any_lflagged(screen->msgmap, MN_HIDE|MN_CHID);

    /*---- march thru display lines, painting whatever is needed ----*/
    for(i = 0, n = screen->msg_at_top; i < (int) screen->lines_per_page; i++){
	if(visible == 0L || n < 1 || n > mn_get_total(screen->msgmap)){
	    if(screen->entry_state[i].id != LINE_HASH_N){
		screen->entry_state[i].hilite = 0;
		screen->entry_state[i].bolded = 0;
		screen->entry_state[i].msgno  = 0L;
		screen->entry_state[i].id     = LINE_HASH_N;
		ClearLine(HEADER_ROWS(state) + i);
	    }
	}
	else{
	    ICE_S *ice;

	    /*
	     * This changes status_col as a side effect so it has to be
	     * executed before next line.
	     */
	    ice = build_header_line(state, screen->stream, screen->msgmap,
				    n, &already_fetched);
	    if(visible > 0L)
	      visible--;

	    if(THRD_INDX()){
		unsigned long rawno;

		rawno = mn_m2raw(screen->msgmap, n);
		if(rawno)
		  thrd = fetch_thread(screen->stream, rawno);
	    }

	    row = paint_index_line(ice, i,
				   (THRD_INDX() && thrd) ? thrd->thrdno : n,
				   screen->status_fld, screen->plus_fld,
				   screen->arrow_fld, &screen->entry_state[i],
				   mn_is_cur(screen->msgmap, n),
				   THRD_INDX()
				     ? (count_lflags_in_thread(screen->stream,
							       thrd,
							       screen->msgmap,
							       MN_SLCT) > 0)
				     : get_lflag(screen->stream, screen->msgmap,
					         n, MN_SLCT));
	    fflush(stdout);
	    if(row && retval < 0)
	      retval = row;
	}

	/*--- increment n ---*/
	while((visible == -1L || visible > 0L)
	      && ++n <= mn_get_total(screen->msgmap)
	      && msgline_hidden(screen->stream, screen->msgmap, n, 0))
	  ;
    }

    if(we_cancel)
      cancel_busy_cue(-1);

    mail_parameters(NULL, SET_IMAPENVELOPE, (void *) NULL);

#ifdef _WINDOWS
    mswin_endupdate();
#endif
    fflush(stdout);
    dprint((7, "--update_index done\n"));
    return(retval);
}



/*----------------------------------------------------------------------
      Create a string summarizing the message header for index on screen

   Args: stream -- mail stream to fetch envelope info from
	 msgmap -- message number to pine sort mapping
	 msgno  -- Message number to create line for

  Result: returns a malloced string
          saves string in a cache for next call for same header
 ----*/
ICE_S *
build_header_line(struct pine *state, MAILSTREAM *stream, MSGNO_S *msgmap, long int msgno, int *already_fetched)
{
    return(build_header_work(state, stream, msgmap, msgno,
			     current_index_state->msg_at_top,
			     current_index_state->lines_per_page,
			     already_fetched));
}


/*----------------------------------------------------------------------
     Paint the given index line


  Args:   ice -- index cache entry
	 line -- index line number on screen, starting at 0 for first
		 visible line, 1, 2, ...
	msgno -- for painting the message number field
	 sfld -- field type of the status field, which is
		 where we'll put the X for selected if necessary
	 pfld -- field type where the thread indicator symbol goes
	 afld -- field type of column which corresponds to the
		 index-format ARROW token
	entry -- cache used to help us decide whether or not we need to
	 	 redraw the index line or if we can just leave it alone because
		 we know it is already correct
	  cur -- is this the current message?
	  sel -- is this message in the selected set?

  Returns: screen row number if this is current message, else 0
 ----*/
int
paint_index_line(ICE_S *argice, int line, long int msgno, IndexColType sfld,
		 IndexColType pfld, IndexColType afld, struct entry_state *entry,
		 int cur, int sel)
{
  COLOR_PAIR *lastc = NULL, *base_color = NULL;
  ICE_S      *ice;
  IFIELD_S   *ifield, *previfield = NULL;
  IELEM_S    *ielem;
  int         save_schar1 = -1, save_schar2 = -1, save_pchar = -1, i;
  int         draw_whole_line = 0, draw_partial_line = 0;
  int         n = MAX_SCREEN_COLS*6;
  char        draw[MAX_SCREEN_COLS*6+1], *p;

  ice = (THRD_INDX() && argice) ? argice->tice : argice;

  /* This better not happen! */
  if(!ice){
    q_status_message3(SM_ORDER | SM_DING, 5, 5,
		      "NULL ice in paint_index_line: %s, msgno=%s line=%s",
		      THRD_INDX() ? "THRD_INDX" : "reg index",
		      comatose(msgno), comatose(line));
    dprint((1, "NULL ice in paint_index_line: %s, msgno=%ld line=%d\n",
			       THRD_INDX() ? "THRD_INDX" : "reg index",
			       msgno, line));
    return 0;
  }

  if(entry->msgno != msgno || ice->id == 0 || entry->id != ice->id){
    entry->id = 0L;
    entry->msgno = 0L;
    draw_whole_line = 1;
  }
  else if((cur != entry->hilite) || (sel != entry->bolded)
          || (ice->plus != entry->plus)){
    draw_partial_line = 1;
  }

  if(draw_whole_line || draw_partial_line){

    if(F_ON(F_FORCE_LOW_SPEED,ps_global) || ps_global->low_speed){

      memset(draw, 0, sizeof(draw));
      p = draw;

      for(ifield = ice->ifield; ifield && p-draw < n; ifield = ifield->next){

	/* space between fields */
	if(ifield != ice->ifield && !(previfield && previfield->ctype == iText))
	  *p++ = ' ';

	/* message number string is generated on the fly */
	if(ifield->ctype == iMessNo){
	  ielem = ifield->ielem;
	  if(ielem && ielem->datalen >= ifield->width){
	    snprintf(ielem->data, ielem->datalen+1, "%*.ld", ifield->width, msgno);
	    ielem->data[ifield->width] = '\0';
	    ielem->data[ielem->datalen] = '\0';
	  }
	}

	if(ifield->ctype == sfld){
	  ielem = ifield->ielem;
	  if(ielem && ielem->datalen > 0){
	    if(draw_partial_line)
	      MoveCursor(HEADER_ROWS(ps_global) + line, utf8_width(draw));

	    if(ielem->datalen == 1){
	      save_schar1 = ielem->data[0];
	      ielem->data[0] = (sel) ? 'X' : (cur && save_schar1 == ' ') ?  '-' : save_schar1;
	      if(draw_partial_line)
	        Writechar(ielem->data[0], 0);

	      if(ielem->next && ielem->next->datalen){
	        save_schar2 = ielem->next->data[0];
	        if(cur)
	          ielem->next->data[0] = '>';

	        if(draw_partial_line)
	          Writechar(ielem->next->data[0], 0);
	      }
	    }
	    else if(ielem->datalen > 1){
	      save_schar1 = ielem->data[0];
	      ielem->data[0] = (sel) ? 'X' : (cur && save_schar1 == ' ') ?  '-' : save_schar1;
	      if(draw_partial_line)
	        Writechar(ielem->data[0], 0);

	      save_schar2 = ielem->data[1];
	      if(cur){
	        ielem->data[1] = '>';
	        if(draw_partial_line)
	          Writechar(ielem->data[1], 0);
	      }
	    }
	  }
	}
	else if(ifield->ctype == afld){

	  if(draw_partial_line){
	    MoveCursor(HEADER_ROWS(ps_global) + line, utf8_width(draw));
	    for(i = 0; i < ifield->width-1; i++)
	      Writechar(cur ? '-' : ' ', 0);

	    Writechar(cur ? '>' : ' ', 0);
	  }

	  ielem = ifield->ielem;
	  if(ielem && ielem->datalen >= ifield->width){
	    for(i = 0; i < ifield->width-1; i++)
	      ielem->data[i] = cur ? '-' : ' ';

	    ielem->data[i] = cur ? '>' : ' ';
	  }
	}
	else if(ifield->ctype == pfld){
	  ielem = ifield->ielem;
	  if(ielem && ielem->datalen > 0){
	    save_pchar = ielem->data[0];
	    ielem->data[0] = ice->plus;

	    if(draw_partial_line){
	      MoveCursor(HEADER_ROWS(ps_global) + line, utf8_width(draw));
	      Writechar(ielem->data[0], 0);
	      Writechar(' ', 0);
	    }
	  }
        }

	for(ielem = ifield->ielem;
	    ielem && ielem->print_format && p-draw < n;
	    ielem = ielem->next){
	  char *src;
	  size_t bytes_added;

	  src = ielem->data;
	  bytes_added = utf8_pad_to_width(p, src,
					  ((n+1)-(p-draw)) * sizeof(char),
					  ielem->wid, ifield->leftadj);
	  p += bytes_added;
	}

        draw[n] = '\0';

	if(ifield->ctype == sfld){
	  ielem = ifield->ielem;
	  if(ielem && ielem->datalen > 0){
	    if(ielem->datalen == 1){
	      ielem->data[0] = save_schar1;
	      if(ielem->next && ielem->next->datalen)
	        ielem->next->data[0] = save_schar2;
	    }
	    else if(ielem->datalen > 1){
	      ielem->data[0] = save_schar1;
	      ielem->data[1] = save_schar2;
	    }
	  }
	}
	else if(ifield->ctype == afld){
	  ielem = ifield->ielem;
	  if(ielem && ielem->datalen >= ifield->width)
	    for(i = 0; i < ifield->width; i++)
	      ielem->data[i] = ' ';
	}
	else if(ifield->ctype == pfld){
	  ielem = ifield->ielem;
	  if(ielem && ielem->datalen > 0)
	    ielem->data[0] = save_pchar;
	}

	previfield = ifield;
      }

      *p = '\0';

      if(draw_whole_line)
        PutLine0(HEADER_ROWS(ps_global) + line, 0, draw);
    }
    else{
      int   uc, ac, do_arrow;
      int   i, drew_X = 0;
      int   inverse_hack = 0, need_inverse_hack = 0;
      int   doing_bold = 0;

      /* so we can restore current color at the end */
      if((uc=pico_usingcolor()) != 0)
        lastc = pico_get_cur_color();

      /*
       * There are two possible "arrow" cursors. One is the one that
       * you get when you are at a slow speed or you turn that slow
       * speed one on. It is drawn as part of the status column.
       * That one is the one associated with the variable "ac".
       * It is always the base_color or the inverse of the base_color.
       *
       * The other "arrow" cursor is the one you get by including the
       * ARROW token in the index-format. It may be configured to
       * be colored.
       *
       * The arrow cursors have two special properties that make
       * them different from other sections or fields.
       * First, the arrow cursors only show up on the current line.
       * Second, the arrow cursors are drawn with generated data, not
       * data that is present in the passed in data.
       */

      /* ac is for the old integrated arrow cursor */
      ac = F_ON(F_FORCE_ARROW,ps_global);

      /* do_arrow is for the ARROW token in index-format */
      do_arrow = (afld != iNothing);

      MoveCursor(HEADER_ROWS(ps_global) + line, 0);

      /* find the base color for the whole line */
      if(cur && !ac && !do_arrow){
	/*
	 * This stanza handles the current line marking in the
	 * regular, non-arrow-cursor case.
	 */

	/*
	 * If the current line has a linecolor, apply the
	 * appropriate reverse transformation to show it is current.
	 */
	if(uc && ice->linecolor && ice->linecolor->fg[0]
	   && ice->linecolor->bg[0] && pico_is_good_colorpair(ice->linecolor)){
	  base_color = apply_rev_color(ice->linecolor,
				       ps_global->index_color_style);

          (void)pico_set_colorp(base_color, PSC_NONE);
	}
	else{
	  inverse_hack++;
	  if(uc){
	      COLOR_PAIR *rev;

	      if((rev = pico_get_rev_color()) != NULL){
		  base_color = new_color_pair(rev->fg, rev->bg);
		  (void)pico_set_colorp(base_color, PSC_NONE);
	      }
	      else
		base_color = lastc;
	  }
	}
      }
      else if(uc && ice->linecolor && ice->linecolor->fg[0]
	      && ice->linecolor->bg[0]
	      && pico_is_good_colorpair(ice->linecolor)){
	(void)pico_set_colorp(ice->linecolor, PSC_NONE);
	base_color = ice->linecolor;
      }
      else
        base_color = lastc;

      memset(draw, 0, sizeof(draw));
      p = draw;

      doing_bold = (sel && F_ON(F_SELECTED_SHOWN_BOLD, ps_global) && StartBold());

      /* draw each field */
      for(ifield = ice->ifield; ifield && p-draw < n; ifield = ifield->next){

	drew_X = 0;

	/*
	 * Fix up the data for some special cases.
	 */

	/* message number string is generated on the fly */
	if(ifield->ctype == iMessNo){
	  ielem = ifield->ielem;
	  if(ielem && ielem->datalen >= ifield->width){
	    snprintf(ielem->data, ielem->datalen+1, "%*.ld", ifield->width, msgno);
	    ielem->data[ifield->width] = '\0';
	    ielem->data[ielem->datalen] = '\0';
	  }
	}

	if(ifield->ctype == sfld){
	  ielem = ifield->ielem;
	  if(ielem && ielem->datalen > 0){
	    if(ielem->datalen == 1){
	      save_schar1 = ielem->data[0];
	      if(sel && !doing_bold){
	        ielem->data[0] = 'X';
		drew_X++;
	      }
	      else if(ac && cur && ielem->data[0] == ' ')
	        ielem->data[0] = '-';

	      if(ielem->next && ielem->next->datalen){
	        save_schar2 = ielem->next->data[0];
		if(ac && cur && ielem->next->data[0] != '\0')
	          ielem->next->data[0] = '>';
	      }
	    }
	    else if(ielem->datalen > 1){
	      if(sel && !doing_bold){
	        ielem->data[0] = 'X';
		drew_X++;
	      }
	      else if(ac && cur && ielem->data[0] == ' ')
	        ielem->data[0] = '-';

	      save_schar2 = ielem->data[1];
	      if(ac && cur && ielem->data[1] != '\0')
	        ielem->data[1] = '>';
	    }
	  }
	}
	else if(ifield->ctype == afld && do_arrow && cur){

	  ielem = ifield->ielem;
	  if(ielem && ielem->datalen >= ifield->width){
	    for(i = 0; i < ifield->width-1; i++)
	      ielem->data[i] = cur ? '-' : ' ';

	    ielem->data[i] = '>';
	  }
	}
	else if(ifield->ctype == pfld){
	  ielem = ifield->ielem;
	  if(ielem && ielem->datalen > 0){
	    save_pchar = ielem->data[0];
	    ielem->data[0] = ice->plus;
	  }
        }

	/* space between fields */
	if(ifield != ice->ifield && !(previfield && previfield->ctype == iText)){
	  if(inverse_hack)
	    StartInverse();

	  Write_to_screen(" ");
	  if(inverse_hack)
	    EndInverse();
	}

	for(ielem = ifield->ielem; ielem; ielem = ielem->next){
	  char *src;
	  size_t bytes_added;

	  src = ielem->data;
	  bytes_added = utf8_pad_to_width(draw, src,
					  (n+1) * sizeof(char),
					  ielem->wid, ifield->leftadj);
	  draw[n] = '\0';

          /*
           * Switch to color for ielem.
	   * But don't switch if we drew an X in this column,
	   * because that overwrites the colored thing, and don't
	   * switch if this is the ARROW field and this is not
	   * the current message. ARROW field is only colored for
	   * the current message.
	   * And don't switch if current line and type eTypeCol.
	   */
	  if(ielem->color && pico_is_good_colorpair(ielem->color)
	     && !(do_arrow && ifield->ctype == afld && !cur)
	     && (!drew_X || ielem != ifield->ielem)
	     && !(cur && ielem->type == eTypeCol)){
	    need_inverse_hack = 0;
	    (void) pico_set_colorp(ielem->color, PSC_NORM);
	  }
	  else
	    need_inverse_hack = 1;

	  if(need_inverse_hack && inverse_hack)
	    StartInverse();

	  Write_to_screen(draw);
	  if(need_inverse_hack && inverse_hack)
	    EndInverse();

	  (void) pico_set_colorp(base_color, PSC_NORM);
	}

	/*
	 * Restore the data for the special cases.
	 */

	if(ifield->ctype == sfld){
	  ielem = ifield->ielem;
	  if(ielem && ielem->datalen > 0){
	    if(ielem->datalen == 1){
	      ielem->data[0] = save_schar1;
	      if(ielem->next && ielem->next->datalen)
	        ielem->next->data[0] = save_schar2;
	    }
	    else if(ielem->datalen > 1){
	      ielem->data[0] = save_schar1;
	      ielem->data[1] = save_schar2;
	    }
	  }
	}
	else if(ifield->ctype == afld){
	  ielem = ifield->ielem;
	  if(ielem && ielem->datalen >= ifield->width)
	    for(i = 0; i < ifield->width; i++)
	      ielem->data[i] = ' ';
	}
	else if(ifield->ctype == pfld){
	  ielem = ifield->ielem;
	  if(ielem && ielem->datalen > 0)
	    ielem->data[0] = save_pchar;
	}

	previfield = ifield;
      }

      if(doing_bold)
        EndBold();

      if(base_color && base_color != lastc && base_color != ice->linecolor)
        free_color_pair(&base_color);

      if(lastc){
	(void)pico_set_colorp(lastc, PSC_NORM);
	free_color_pair(&lastc);
      }
    }
  }

  entry->hilite = cur;
  entry->bolded = sel;
  entry->msgno  = msgno;
  entry->plus   = ice->plus;
  entry->id     = ice->id;

  if(!ice->color_lookup_done && pico_usingcolor())
    entry->id = 0;

  return(cur ? (line + HEADER_ROWS(ps_global)) : 0);
}

/*
 * setup_index_state - hooked onto pith_opt_save_index_state to setup
 *		       current_index_state after setup_{index,thread}_header_widths
 */
void
setup_index_state(int threaded)
{
    if(current_index_state){
	if(threaded){
	    current_index_state->status_col  = 0;
	    current_index_state->status_fld  = iStatus;
	    current_index_state->plus_fld    = iNothing;
	    current_index_state->arrow_fld   = iNothing;
	} else {
	    INDEX_COL_S	 *cdesc, *prevcdesc = NULL;
	    IndexColType  sfld, altfld, plusfld, arrowfld;
	    int           width, fld, col, pluscol, scol, altcol;

	    col = 0;
	    scol = -1;
	    sfld = iNothing;
	    altcol = -1;
	    altfld = iNothing;
	    /* figure out which field is status field */
	    for(cdesc = ps_global->index_disp_format, fld = 0;
		cdesc->ctype != iNothing;
		cdesc++){
		width = cdesc->width;
		if(width == 0)
		  continue;

		/* space between columns */
		if(col > 0 && !(prevcdesc && prevcdesc->ctype == iText))
		  col++;

		if(cdesc->ctype == iStatus){
		    scol = col;
		    sfld = cdesc->ctype;
		    break;
		}

		if(cdesc->ctype == iFStatus || cdesc->ctype == iIStatus){
		    scol = col;
		    sfld = cdesc->ctype;
		    break;
		}

		if(cdesc->ctype == iMessNo){
		    altcol = col;
		    altfld = cdesc->ctype;
		}

		col += width;
		fld++;
		prevcdesc = cdesc;
	    }

	    if(sfld == iNothing){
		if(altcol == -1){
		    scol = 0;
		}
		else{
		    scol = altcol;
		    sfld = altfld;
		}
	    }


	    current_index_state->status_col  = scol;
	    current_index_state->status_fld  = sfld;

	    col = 0;
	    plusfld = iNothing;
	    pluscol = -1;
	    prevcdesc = NULL;
	    /* figure out which column to use for threading '+' */
	    if(THREADING()
	       && ps_global->thread_disp_style != THREAD_NONE
	       && ps_global->VAR_THREAD_MORE_CHAR[0]
	       && ps_global->VAR_THREAD_EXP_CHAR[0])
	      for(cdesc = ps_global->index_disp_format, fld = 0;
		  cdesc->ctype != iNothing;
		  cdesc++){
		  width = cdesc->width;
		  if(width == 0)
		    continue;

		  /* space between columns */
		  if(col > 0 && !(prevcdesc && prevcdesc->ctype == iText))
		    col++;

		  if((cdesc->ctype == iSubject
		      || cdesc->ctype == iSubjectText
		      || cdesc->ctype == iSubjKey
		      || cdesc->ctype == iSubjKeyText
		      || cdesc->ctype == iSubjKeyInit
		      || cdesc->ctype == iSubjKeyInitText)
		     && (ps_global->thread_disp_style == THREAD_STRUCT
			 || ps_global->thread_disp_style == THREAD_MUTTLIKE
			 || ps_global->thread_disp_style == THREAD_INDENT_SUBJ1
			 || ps_global->thread_disp_style == THREAD_INDENT_SUBJ2)){
		      plusfld = cdesc->ctype;
		      pluscol = col;
		      break;
		  }

		  if((cdesc->ctype == iFrom
		      || cdesc->ctype == iFromToNotNews
		      || cdesc->ctype == iFromTo
		      || cdesc->ctype == iAddress
		      || cdesc->ctype == iMailbox)
		     && (ps_global->thread_disp_style == THREAD_INDENT_FROM1
			 || ps_global->thread_disp_style == THREAD_INDENT_FROM2
			 || ps_global->thread_disp_style == THREAD_STRUCT_FROM)){
		      plusfld = cdesc->ctype;
		      pluscol = col;
		      break;
		  }

		  col += width;
		  fld++;
		  prevcdesc = cdesc;
	      }

	    current_index_state->plus_fld    = plusfld;
	    current_index_state->plus_col    = pluscol;

	    arrowfld = iNothing;
	    /* figure out which field is arrow field, if any */
	    for(cdesc = ps_global->index_disp_format, fld = 0;
		cdesc->ctype != iNothing;
		cdesc++){
		width = cdesc->width;
		if(width == 0)
		  continue;

		if(cdesc->ctype == iArrow){
		    arrowfld   = cdesc->ctype;
		    break;
		}

		fld++;
	    }

	    current_index_state->arrow_fld   = arrowfld;
	}
    }
}


/*
 * insert_condensed_thread_cue - used on pith hook to add decoration to
 *				 subject or from text to show condensed thread info
 */
int
condensed_thread_cue(PINETHRD_S *thd, ICE_S *ice,
		     char **fieldstr, size_t *strsize, int width, int collapsed)
{
    if(current_index_state->plus_fld != iNothing && !THRD_INDX() && fieldstr && *fieldstr){
	/*
	 * WARNING!
	 * There is an unwarranted assumption here that VAR_THREAD_MORE_CHAR[0]
	 * and VAR_THREAD_EXP_CHAR[0] are ascii.
	 * Could do something similar to the conversions done with keyword
	 * initials in key_str.
	 */
	if(ice)
	  ice->plus = collapsed ? ps_global->VAR_THREAD_MORE_CHAR[0]
				: (thd && thd->next)
				   ? ps_global->VAR_THREAD_EXP_CHAR[0] : ' ';

	if(strsize && *strsize > 0 && width != 0){
	    *(*fieldstr)++ = ' ';
	    (*strsize)--;
	    if(width > 0)
	      width--;
	}

	if(strsize && *strsize > 0 && width != 0){
	    *(*fieldstr)++ = ' ';
	    (*strsize)--;
	    if(width > 0)
	      width--;
	}
    }

    return(width);
}


int
truncate_subj_and_from_strings(void)
{
    return 1;
}


/*
 * paint_index_hline - paint index line given what we got
 */
void
paint_index_hline(MAILSTREAM *stream, long int msgno, ICE_S *ice)
{
    PINETHRD_S *thrd;

    /*
     * Trust only what we get back that isn't bogus since
     * we were prevented from doing any fetches and such...
     */
    if((ps_global->redrawer == redraw_index_body
	|| ps_global->prev_screen == mail_index_screen)
       && current_index_state
       && current_index_state->stream == stream
       && !ps_global->msgmap->hilited){
	int line;

	/*
	 * This test isn't right if there are hidden lines. The line will
	 * fail the test because it seems like it is past the end of the
	 * screen but since the hidden lines don't take up space the line
	 * might actually be on the screen. Don't know that it is worth
	 * it to fix this, though, since you may have to file through
	 * many hidden lines before finding the visible ones. I'm not sure
	 * if the logic inside the if is correct when we do pass the
	 * top-level test. Leave it for now.  Hubert - 2002-06-28
	 */
	if((line = (int)(msgno - current_index_state->msg_at_top)) >= 0
	   && line < current_index_state->lines_per_page){
	    if(any_lflagged(ps_global->msgmap, MN_HIDE | MN_CHID)){
		long n;
		long zoomhide, collapsehide;

		zoomhide = any_lflagged(ps_global->msgmap, MN_HIDE);
		collapsehide = any_lflagged(ps_global->msgmap, MN_CHID);

		/*
		 * Line is visible if it is selected and not hidden due to
		 * thread collapse, or if there is no zooming happening and
		 * it is not hidden due to thread collapse.
		 */
		for(line = 0, n = current_index_state->msg_at_top;
		    n != msgno;
		    n++)
		  if((zoomhide
		      && get_lflag(stream, current_index_state->msgmap,
				   n, MN_SLCT)
		      && (!collapsehide
		          || !get_lflag(stream, current_index_state->msgmap, n,
				        MN_CHID)))
		     ||
		     (!zoomhide
		      && !get_lflag(stream, current_index_state->msgmap,
				    n, MN_CHID)))
		    line++;
	    }

	    thrd = NULL;
	    if(THRD_INDX()){
		unsigned long rawno;

		rawno = mn_m2raw(current_index_state->msgmap, msgno);
		if(rawno)
		  thrd = fetch_thread(stream, rawno);
	    }

	    paint_index_line(ice, line,
			     (THRD_INDX() && thrd) ? thrd->thrdno : msgno,
			     current_index_state->status_fld,
			     current_index_state->plus_fld,
			     current_index_state->arrow_fld,
			     &current_index_state->entry_state[line],
			     mn_is_cur(current_index_state->msgmap, msgno),
			     THRD_INDX()
			       ? (count_lflags_in_thread(stream, thrd,
						   current_index_state->msgmap,
						         MN_SLCT) > 0)
			       : get_lflag(stream, current_index_state->msgmap,
					   msgno, MN_SLCT));
	    fflush(stdout);
	}
    }
}




/*
 * pine_imap_env -- C-client's telling us an envelope just arrived
 *		    from the server.  Use it if we can...
 */
void
pine_imap_envelope(MAILSTREAM *stream, long unsigned int rawno, ENVELOPE *env)
{
    MESSAGECACHE *mc;

    dprint((7, "imap_env(%ld)\n", rawno));
    if(stream && !sp_mail_box_changed(stream)
       && stream == ps_global->mail_stream
       && rawno > 0L && rawno <= stream->nmsgs
       && (mc = mail_elt(stream,rawno))
       && mc->valid
       && mc->rfc822_size
       && !get_lflag(stream, NULL, rawno, MN_HIDE | MN_CHID | MN_EXLD)){
	INDEXDATA_S  idata;
	ICE_S	    *ice;

	memset(&idata, 0, sizeof(INDEXDATA_S));
	idata.no_fetch = 1;
	idata.size     = mc->rfc822_size;
	idata.rawno    = rawno;
	idata.msgno    = mn_raw2m(sp_msgmap(stream), rawno);
	idata.stream   = stream;

	index_data_env(&idata, env);

	/*
	 * Look for resent-to already in MAILCACHE data 
	 */
	if(mc->private.msg.header.text.data){
	    STRINGLIST *lines;
	    SIZEDTEXT	szt;
	    static char *linelist[] = {"resent-to" , NULL};

	    if(mail_match_lines(lines = new_strlst(linelist),
				mc->private.msg.lines, 0L)){
		idata.valid_resent_to = 1;
		memset(&szt, 0, sizeof(SIZEDTEXT));
		textcpy(&szt, &mc->private.msg.header.text);
		mail_filter((char *) szt.data, szt.size, lines, 0L);
		idata.resent_to_us = parsed_resent_to_us((char *) szt.data);
		if(szt.data)
		  fs_give((void **) &szt.data);
	    }

	    free_strlst(&lines);
	}

	ice = (*format_index_line)(&idata);
	if(idata.bogus)
	  clear_ice(&ice);
	else
	  paint_index_hline(stream, idata.msgno, ice);
    }
}


/*----------------------------------------------------------------------
     Scroll to specified postion.


  Args: pos - position to scroll to.

  Returns: TRUE - did the scroll operation.
	   FALSE - was not able to do the scroll operation.
 ----*/
int
index_scroll_to_pos (long int pos)
{
    static short bad_timing = 0;
    long	i, j, k;
    
    if(bad_timing)
      return (FALSE);

    /*
     * Put the requested line at the top of the screen...
     */

    /*
     * Starting at msg 'pos' find next visible message.
     */
    for(i=pos; i <= mn_get_total(current_index_state->msgmap); i++) {
      if(!msgline_hidden(current_index_state->stream,
			 current_index_state->msgmap, i, 0)){
	  current_index_state->msg_at_top = i;
	  break;
      }
    }

    /*
     * If single selection, move selected message to be on the screen.
     */
    if (mn_total_cur(current_index_state->msgmap) == 1L) {
      if (current_index_state->msg_at_top > 
			      mn_get_cur (current_index_state->msgmap)) {
	/* Selection was above screen, move to top of screen. */
	mn_set_cur(current_index_state->msgmap,current_index_state->msg_at_top);
      }
      else {
	/* Scan through the screen.  If selection found, leave where is.
	 * Otherwise, move to end of screen */
        for(  i = current_index_state->msg_at_top, 
	        j = current_index_state->lines_per_page;
	      i != mn_get_cur(current_index_state->msgmap) && 
		i <= mn_get_total(current_index_state->msgmap) && 
		j > 0L;
	      i++) {
	    if(!msgline_hidden(current_index_state->stream,
			       current_index_state->msgmap, i, 0)){
	        j--;
	        k = i;
            }
        }
	if(j <= 0L)
	    /* Move to end of screen. */
	    mn_set_cur(current_index_state->msgmap, k);
      }
    }

    bad_timing = 0;
    return (TRUE);
}



/*----------------------------------------------------------------------
     Adjust the index display state down a line

  Args: scroll_count -- number of lines to scroll

  Returns: TRUE - did the scroll operation.
	   FALSE - was not able to do the scroll operation.
 ----*/
int
index_scroll_down(long int scroll_count)
{
    static short bad_timing = 0;
    long i, j, k;
    long cur, total;

    if(bad_timing)
      return (FALSE);

    bad_timing = 1;
    
    
    j = -1L;
    total = mn_get_total (current_index_state->msgmap);
    for(k = i = current_index_state->msg_at_top; ; i++){

	/* Only examine non-hidden messages. */
	if(!msgline_hidden(current_index_state->stream,
			   current_index_state->msgmap, i, 0)){
	    /* Remember this message */
	    k = i;
	    /* Increment count of lines.  */
	    if (++j >= scroll_count) {
		/* Counted enough lines, stop. */
		current_index_state->msg_at_top = k;
		break;
	    }
	}
	    
	/* If at last message, stop. */
	if (i >= total){
	    current_index_state->msg_at_top = k;
	    break;
	}
    }

    /*
     * If not multiple selection, see if selected message visable.  if not
     * set it to last visable message. 
     */
    if(mn_total_cur(current_index_state->msgmap) == 1L) {
	j = 0L;
	cur = mn_get_cur (current_index_state->msgmap);
	for (i = current_index_state->msg_at_top; i <= total; ++i) {
	    if(!msgline_hidden(current_index_state->stream,
			       current_index_state->msgmap, i, 0)){
	        if (++j >= current_index_state->lines_per_page) {
		    break;
	        }
		if (i == cur) 
		    break;
	    }
        }
	if (i != cur) 
	    mn_set_cur(current_index_state->msgmap,
		       current_index_state->msg_at_top);
    }

    bad_timing = 0;
    return (TRUE);
}



/*----------------------------------------------------------------------
     Adjust the index display state up a line

  Args: scroll_count -- number of lines to scroll

  Returns: TRUE - did the scroll operation.
	   FALSE - was not able to do the scroll operation.

 ----*/
int
index_scroll_up(long int scroll_count)
{
    static short bad_timing = 0;
    long i, j, k;
    long cur;

    if(bad_timing)
      return(FALSE);

    bad_timing = 1;
    
    j = -1L;
    for(k = i = current_index_state->msg_at_top; ; i--){

	/* Only examine non-hidden messages. */
	if(!msgline_hidden(current_index_state->stream,
			   current_index_state->msgmap, i, 0)){
	    /* Remember this message */
	    k = i;
	    /* Increment count of lines.  */
	    if (++j >= scroll_count) {
		/* Counted enough lines, stop. */
		current_index_state->msg_at_top = k;
		break;
	    }
	}
	    
	/* If at first message, stop */
	if (i <= 1L){
	    current_index_state->msg_at_top = k;
	    break;
	}
    }

    
    /*
     * If not multiple selection, see if selected message visable.  if not
     * set it to last visable message. 
     */
    if(mn_total_cur(current_index_state->msgmap) == 1L) {
	j = 0L;
	cur = mn_get_cur (current_index_state->msgmap);
	for (	i = current_index_state->msg_at_top; 
		i <= mn_get_total(current_index_state->msgmap);
		++i) {
	    if(!msgline_hidden(current_index_state->stream,
			       current_index_state->msgmap, i, 0)){
	        if (++j >= current_index_state->lines_per_page) {
		    k = i;
		    break;
	        }
		if (i == cur) 
		    break;
	    }
        }
	if (i != cur) 
	    mn_set_cur(current_index_state->msgmap, k);
    }


    bad_timing = 0;
    return (TRUE);
}



/*----------------------------------------------------------------------
     Calculate the message number that should be at the top of the display

  Args: current - the current message number
        lines_per_page - the number of lines for the body of the index only

  Returns: -1 if the current message is -1 
           the message entry for the first message at the top of the screen.

When paging in the index it is always on even page boundies, and the
current message is always on the page thus the top of the page is
completely determined by the current message and the number of lines
on the page. 
 ----*/
long
top_ent_calc(MAILSTREAM *stream, MSGNO_S *msgs, long int at_top, long int lines_per_page)
{
    long current, hidden, lastn;
    long n, m = 0L, t = 1L;

    current = (mn_total_cur(msgs) <= 1L) ? mn_get_cur(msgs) : at_top;

    if(current < 0L)
      return(-1);

    if(lines_per_page == 0L)
      return(current);

    if(THRD_INDX_ENABLED()){
	long rawno;
	PINETHRD_S *thrd = NULL;

	rawno = mn_m2raw(msgs, mn_get_cur(msgs));
	if(rawno)
	  thrd = fetch_thread(stream, rawno);

	if(THRD_INDX()){

	    if(any_lflagged(msgs, MN_HIDE)){
		PINETHRD_S *is_current_thrd;

		is_current_thrd = thrd;
		if(is_current_thrd){
		    if(mn_get_revsort(msgs)){
			/* start with top of tail of thread list */
			thrd = fetch_thread(stream, mn_m2raw(msgs, 1L));
			if(thrd && thrd->top && thrd->top != thrd->rawno)
			  thrd = fetch_thread(stream, thrd->top);
		    }
		    else{
			/* start with head of thread list */
			thrd = fetch_head_thread(stream);
		    }

		    t = 1L;
		    m = 0L;
		    if(thrd)
		      n = mn_raw2m(msgs, thrd->rawno);

		    while(thrd){
			if(!msgline_hidden(stream, msgs, n, 0)
			   && (++m % lines_per_page) == 1L)
			  t = n;
			
			if(thrd == is_current_thrd)
			  break;

			if(mn_get_revsort(msgs) && thrd->prevthd)
			  thrd = fetch_thread(stream, thrd->prevthd);
			else if(!mn_get_revsort(msgs) && thrd->nextthd)
			  thrd = fetch_thread(stream, thrd->nextthd);
			else
			  thrd = NULL;

			if(thrd)
			  n = mn_raw2m(msgs, thrd->rawno);
		    }
		}
	    }
	    else{
		if(thrd){
		    n = thrd->thrdno;
		    m = lines_per_page * ((n - 1L)/ lines_per_page) + 1L;
		    n = thrd->rawno;
		    /*
		     * We want to find the m'th thread and the
		     * message number that goes with that. We just have
		     * to back up from where we are to get there.
		     * If we have a reverse sort backing up is going
		     * forward through the thread.
		     */
		    while(thrd && m < thrd->thrdno){
			n = thrd->rawno;
			if(mn_get_revsort(msgs) && thrd->nextthd)
			  thrd = fetch_thread(stream, thrd->nextthd);
			else if(!mn_get_revsort(msgs) && thrd->prevthd)
			  thrd = fetch_thread(stream, thrd->prevthd);
			else
			  thrd = NULL;
		    }

		    if(thrd)
		      n = thrd->rawno;

		    t = mn_raw2m(msgs, n);
		}
	    }
	}
	else{		/* viewing a thread */

	    lastn = mn_get_total(msgs);
	    t = 1L;

	    /* get top of thread */
	    if(thrd && thrd->top && thrd->top != thrd->rawno)
	      thrd = fetch_thread(stream, thrd->top);

	    if(thrd){
		if(mn_get_revsort(msgs))
		  lastn = mn_raw2m(msgs, thrd->rawno);
		else
		  t = mn_raw2m(msgs, thrd->rawno);
	    }

	    n = 0L;

	    /* n is the end of this thread */
	    while(thrd){
		n = mn_raw2m(msgs, thrd->rawno);
		if(thrd->branch)
		  thrd = fetch_thread(stream, thrd->branch);
		else if(thrd->next)
		  thrd = fetch_thread(stream, thrd->next);
		else
		  thrd = NULL;
	    }

	    if(n){
		if(mn_get_revsort(msgs))
		  t = n;
		else
		  lastn = n;
	    }

	    for(m = 0L, n = t; n <= MIN(current, lastn); n++)
	      if(!msgline_hidden(stream, msgs, n, 0)
		 && (++m % lines_per_page) == 1L)
		t = n;
	}

	return(t);
    }
    else if((hidden = any_lflagged(msgs, MN_HIDE | MN_CHID)) != 0){

	if(current < mn_get_total(msgs) / 2){
	    t = 1L;
	    m = 0L;
	    for(n = 1L; n <= MIN(current, mn_get_total(msgs)); n++)
	      if(!msgline_hidden(stream, msgs, n, 0)
		 && (++m % lines_per_page) == 1L)
	        t = n;
	}
	else{
	    t = current+1L;
	    m = mn_get_total(msgs)-hidden+1L;
	    for(n = mn_get_total(msgs); n >= 1L && t > current; n--)
	      if(!msgline_hidden(stream, msgs, n, 0)
		 && (--m % lines_per_page) == 1L)
	        t = n;
	    
	    if(t > current)
	      t = 1L;
	}

	return(t);
    }
    else
      return(lines_per_page * ((current - 1L)/ lines_per_page) + 1L);
}


/*----------------------------------------------------------------------
      Clear various bits that make up a healthy display

 ----*/
void
reset_index_border(void)
{
    mark_status_dirty();
    mark_keymenu_dirty();
    mark_titlebar_dirty();
    ps_global->mangled_screen = 1;	/* signal FULL repaint */
}


/*----------------------------------------------------------------------
    This redraws the body of the index screen, taking into
account any change in the size of the screen. All the state needed to
repaint is in the static variables so this can be called from
anywhere.
 ----*/
void
redraw_index_body(void)
{
    int agg;

    if((agg = (mn_total_cur(current_index_state->msgmap) > 1L)) != 0)
      restore_selected(current_index_state->msgmap);

    ps_global->mangled_body = 1;

    (void) update_index(ps_global, current_index_state);
    if(agg)
      pseudo_selected(current_index_state->stream, current_index_state->msgmap);
}


/*----------------------------------------------------------------------
   Give hint about Other command being optional.  Some people get the idea
   that it is required to use the commands on the 2nd and 3rd keymenus.
   
   Args: none

 Result: message may be printed to status line
  ----*/
void
warn_other_cmds(void)
{
    static int other_cmds = 0;

    other_cmds++;
    if(((ps_global->first_time_user || ps_global->show_new_version) &&
	      other_cmds % 3 == 0 && other_cmds < 10) || other_cmds % 20 == 0)
        q_status_message(SM_ASYNC, 0, 9,
			 _("Remember the \"O\" command is always optional"));
}


void
thread_command(struct pine *state, MAILSTREAM *stream, MSGNO_S *msgmap,
	       UCS preloadkeystroke, int q_line)
{
    PINETHRD_S   *thrd = NULL;
    unsigned long rawno, save_branch;
    int           we_cancel = 0;
    int           flags = AC_FROM_THREAD;

    if(!(stream && msgmap))
      return;

    rawno = mn_m2raw(msgmap, mn_get_cur(msgmap));
    if(rawno)
      thrd = fetch_thread(stream, rawno);

    if(!thrd)
      return;

    save_branch = thrd->branch;
    thrd->branch = 0L;		/* branch is a sibling, not part of thread */

    if(!preloadkeystroke){
	if(!THRD_INDX()){
	    if(get_lflag(stream, NULL, rawno, MN_COLL) && thrd->next)
	      flags |= AC_EXPN;
	    else
	      flags |= AC_COLL;
	}

	if(count_lflags_in_thread(stream, thrd, msgmap, MN_SLCT)
	       == count_lflags_in_thread(stream, thrd, msgmap, MN_NONE))
	  flags |= AC_UNSEL;
    }

    we_cancel = busy_cue(NULL, NULL, 1);

    /* save the SLCT flags in STMP for restoring at the bottom */
    copy_lflags(stream, msgmap, MN_SLCT, MN_STMP);

    /* clear the values from the SLCT flags */
    set_lflags(stream, msgmap, MN_SLCT, 0);

    /* set SLCT for thrd on down */
    set_flags_for_thread(stream, msgmap, MN_SLCT, thrd, 1);
    thrd->branch = save_branch;

    if(we_cancel)
      cancel_busy_cue(0);

    (void ) apply_command(state, stream, msgmap, preloadkeystroke, flags,
			  q_line);

    /* restore the original flags */
    copy_lflags(stream, msgmap, MN_STMP, MN_SLCT);

    if(any_lflagged(msgmap, MN_HIDE) > 0L){
	/* if nothing left selected, unhide all */
	if(any_lflagged(msgmap, MN_SLCT) == 0L){
	    (void) unzoom_index(ps_global, stream, msgmap);
	    dprint((4, "\n\n ---- Exiting ZOOM mode ----\n"));
	    q_status_message(SM_ORDER,0,2, _("Index Zoom Mode is now off"));
	}

	/* if current is hidden, adjust */
	adjust_cur_to_visible(stream, msgmap);
    }
}


/*----------------------------------------------------------------------
      Search the message headers as displayed in index
 
  Args: command_line -- The screen line to prompt on
        msg          -- The current message number to start searching at
        max_msg      -- The largest message number in the current folder

  The headers are searched exactly as they are displayed on the screen. The
search will wrap around to the beginning if not string is not found right 
away.
  ----*/
void
index_search(struct pine *state, MAILSTREAM *stream, int command_line, MSGNO_S *msgmap)
{
    int         rc, select_all = 0, flags, prefetch, we_turned_on = 0;
    long        i, sorted_msg, selected = 0L;
    char        prompt[MAX_SEARCH+50], new_string[MAX_SEARCH+1];
    char        buf[MAX_SCREEN_COLS+1], *p;
    HelpType	help;
    char        search_string[MAX_SEARCH+1];
    ICE_S      *ice, *ic;
    static HISTORY_S *history = NULL;
    static ESCKEY_S header_search_key[] = { {0, 0, NULL, NULL },
					    {ctrl('Y'), 10, "^Y", N_("First Msg")},
					    {ctrl('V'), 11, "^V", N_("Last Msg")},
					    {KEY_UP,    30, "", ""},
					    {KEY_DOWN,  31, "", ""},
					    {-1, 0, NULL, NULL} };
#define KU_IS (3)	/* index of KEY_UP */
#define PREFETCH_THIS_MANY_LINES (50)

    init_hist(&history, HISTSIZE);
    search_string[0] = '\0';
    if((p = get_prev_hist(history, "", 0, NULL)) != NULL){
	strncpy(search_string, p, sizeof(search_string));
	search_string[sizeof(search_string)-1] = '\0';
    }

    dprint((4, "\n - search headers - \n"));

    if(!any_messages(msgmap, NULL, "to search")){
	return;
    }
    else if(mn_total_cur(msgmap) > 1L){
	q_status_message1(SM_ORDER, 0, 2, "%s msgs selected; Can't search",
			  comatose(mn_total_cur(msgmap)));
	return;
    }
    else
      sorted_msg = mn_get_cur(msgmap);

    help = NO_HELP;
    new_string[0] = '\0';

    while(1) {
	snprintf(prompt, sizeof(prompt), _("Word to search for [%s] : "), search_string);

	if(F_ON(F_ENABLE_AGG_OPS, ps_global)){
	    header_search_key[0].ch    = ctrl('X');
	    header_search_key[0].rval  = 12;
	    header_search_key[0].name  = "^X";
	    header_search_key[0].label = N_("Select Matches");
	}
	else{
	    header_search_key[0].ch   = header_search_key[0].rval  = 0;
	    header_search_key[0].name = header_search_key[0].label = NULL;
	}

	/*
	 * 2 is really 1 because there will be one real entry and
	 * one entry of "" because of the get_prev_hist above.
	 */
	if(items_in_hist(history) > 2){
	    header_search_key[KU_IS].name  = HISTORY_UP_KEYNAME;
	    header_search_key[KU_IS].label = HISTORY_KEYLABEL;
	    header_search_key[KU_IS+1].name  = HISTORY_DOWN_KEYNAME;
	    header_search_key[KU_IS+1].label = HISTORY_KEYLABEL;
	}
	else{
	    header_search_key[KU_IS].name  = "";
	    header_search_key[KU_IS].label = "";
	    header_search_key[KU_IS+1].name  = "";
	    header_search_key[KU_IS+1].label = "";
	}
	
	flags = OE_APPEND_CURRENT | OE_KEEP_TRAILING_SPACE;
	
        rc = optionally_enter(new_string, command_line, 0, sizeof(new_string),
			      prompt, header_search_key, help, &flags);

        if(rc == 3) {
	    help = (help != NO_HELP) ? NO_HELP :
		     F_ON(F_ENABLE_AGG_OPS, ps_global) ? h_os_index_whereis_agg
		       : h_os_index_whereis;
            continue;
        }
	else if(rc == 10){
	    q_status_message(SM_ORDER, 0, 3, _("Searched to First Message."));
	    if(any_lflagged(msgmap, MN_HIDE | MN_CHID)){
		do{
		    selected = sorted_msg;
		    mn_dec_cur(stream, msgmap, MH_NONE);
		    sorted_msg = mn_get_cur(msgmap);
		}
		while(selected != sorted_msg);
	    }
	    else
	      sorted_msg = (mn_get_total(msgmap) > 0L) ? 1L : 0L;

	    mn_set_cur(msgmap, sorted_msg);
	    return;
	}
	else if(rc == 11){
	    q_status_message(SM_ORDER, 0, 3, _("Searched to Last Message."));
	    if(any_lflagged(msgmap, MN_HIDE | MN_CHID)){
		do{
		    selected = sorted_msg;
		    mn_inc_cur(stream, msgmap, MH_NONE);
		    sorted_msg = mn_get_cur(msgmap);
		}
		while(selected != sorted_msg);
	    }
	    else
	      sorted_msg = mn_get_total(msgmap);

	    mn_set_cur(msgmap, sorted_msg);
	    return;
	}
	else if(rc == 12){
	    select_all = 1;
	    break;
	}
	else if(rc == 30){
	    if((p = get_prev_hist(history, new_string, 0, NULL)) != NULL){
		strncpy(new_string, p, sizeof(new_string));
		new_string[sizeof(new_string)-1] = '\0';
	    }
	    else
	      Writechar(BELL, 0);

	    continue;
	}
	else if(rc == 31){
	    if((p = get_next_hist(history, new_string, 0, NULL)) != NULL){
		strncpy(new_string, p, sizeof(new_string));
		new_string[sizeof(new_string)-1] = '\0';
	    }
	    else
	      Writechar(BELL, 0);

	    continue;
	}

        if(rc != 4){			/* 4 is redraw */
	    save_hist(history, new_string, 0, NULL);
	    break;
	}
    }

    if(rc == 1 || (new_string[0] == '\0' && search_string[0] == '\0')) {
	cmd_cancelled(_("Search"));
        return;
    }

    if(new_string[0] == '\0'){
	strncpy(new_string, search_string, sizeof(new_string));
	new_string[sizeof(new_string)-1] = '\0';
    }

    strncpy(search_string, new_string, sizeof(search_string));
    search_string[sizeof(search_string)-1] = '\0';

    we_turned_on = intr_handling_on();

    prefetch = 0;
    for(i = sorted_msg + ((select_all)?0:1);
	i <= mn_get_total(msgmap) && !ps_global->intr_pending;
	i++){
      if(msgline_hidden(stream, msgmap, i, 0))
	continue;

      if(prefetch <= 0)
        prefetch = PREFETCH_THIS_MANY_LINES;

      ic = build_header_work(state, stream, msgmap, i, i, prefetch--, NULL);

      ice = (ic && THRD_INDX() && ic->tice) ? ic->tice : ic;

      if(srchstr(simple_index_line(buf, sizeof(buf), ice, i),
				   search_string)){
	selected++;
	if(select_all)
	  set_lflag(stream, msgmap, i, MN_SLCT, 1);
	else
	  break;
      }
    }

    prefetch = 0;
    if(i > mn_get_total(msgmap)){
      for(i = 1; i < sorted_msg && !ps_global->intr_pending; i++){
        if(msgline_hidden(stream, msgmap, i, 0))
	  continue;

	if(prefetch <= 0)
          prefetch = PREFETCH_THIS_MANY_LINES;

        ic = build_header_work(state, stream, msgmap, i, i, prefetch--, NULL);

        ice = (ic && THRD_INDX() && ic->tice) ? ic->tice : ic;

        if(srchstr(simple_index_line(buf, sizeof(buf), ice, i),
				     search_string)){
	  selected++;
	  if(select_all)
	    set_lflag(stream, msgmap, i, MN_SLCT, 1);
	  else
	    break;
        }
      }
    }

    /* search current line */
    if(!select_all && !selected){
	i = sorted_msg;
        if(!msgline_hidden(stream, msgmap, i, 0)){

	    ic = build_header_work(state, stream, msgmap, i, i, 1, NULL);

	    ice = (ic && THRD_INDX() && ic->tice) ? ic->tice : ic;

	    if(srchstr(simple_index_line(buf, sizeof(buf), ice, i),
		       search_string)){
		selected++;
	    }
	}
    }

    if(ps_global->intr_pending){
	q_status_message1(SM_ORDER, 0, 3, _("Search cancelled.%s"),
			  select_all ? _(" Selected set may be incomplete."):"");
    }
    else if(select_all){
	if(selected
	   && any_lflagged(msgmap, MN_SLCT) > 0L
	   && !any_lflagged(msgmap, MN_HIDE)
	   && F_ON(F_AUTO_ZOOM, state))
	  (void) zoom_index(state, stream, msgmap, MN_SLCT);
	    
	q_status_message1(SM_ORDER, 0, 3, _("%s messages found matching word"),
			  long2string(selected));
    }
    else if(selected){
	q_status_message1(SM_ORDER, 0, 3, _("Word found%s"),
			  (i < sorted_msg) ? _(". Search wrapped to beginning") :
			   (i == sorted_msg) ? _(". Current line contains only match") : "");
	mn_set_cur(msgmap, i);
    }
    else
      q_status_message(SM_ORDER, 0, 3, _("Word not found"));

    if(we_turned_on)
      intr_handling_off();
}


/*
 * Original idea from Stephen Casner <casner@acm.org>.
 *
 * Apply the appropriate reverse color transformation to the given
 * color pair and return a new color pair. The caller should free the
 * color pair.
 *
 */
COLOR_PAIR *
apply_rev_color(COLOR_PAIR *cp, int style)
{
    COLOR_PAIR *rc = pico_get_rev_color();

    if(rc){
	if(style == IND_COL_REV){
	    /* just use Reverse color regardless */
	    return(new_color_pair(rc->fg, rc->bg));
	}
	else if(style == IND_COL_FG){
	    /*
	     * If changing to Rev fg is readable and different
	     * from what it already is, do it.
	     */
	    if(strcmp(rc->fg, cp->bg) && strcmp(rc->fg, cp->fg))
	      return(new_color_pair(rc->fg, cp->bg));
	}
	else if(style == IND_COL_BG){
	    /*
	     * If changing to Rev bg is readable and different
	     * from what it already is, do it.
	     */
	    if(strcmp(rc->bg, cp->fg) && strcmp(rc->bg, cp->bg))
	      return(new_color_pair(cp->fg, rc->bg));
	}
	else if(style == IND_COL_FG_NOAMBIG){
	    /*
	     * If changing to Rev fg is readable, different
	     * from what it already is, and not the same as
	     * the Rev color, do it.
	     */
	    if(strcmp(rc->fg, cp->bg) && strcmp(rc->fg, cp->fg) &&
	       strcmp(rc->bg, cp->bg))
	      return(new_color_pair(rc->fg, cp->bg));
	}
	else if(style == IND_COL_BG_NOAMBIG){
	    /*
	     * If changing to Rev bg is readable, different
	     * from what it already is, and not the same as
	     * the Rev color, do it.
	     */
	    if(strcmp(rc->bg, cp->fg) && strcmp(rc->bg, cp->bg) &&
	       strcmp(rc->fg, cp->fg))
	      return(new_color_pair(cp->fg, rc->bg));
	}
    }

    /* come here for IND_COL_FLIP and for the cases which fail the tests  */
    return(new_color_pair(cp->bg, cp->fg));	/* flip the colors */
}



#ifdef _WINDOWS

/*----------------------------------------------------------------------
  Callback to get the text of the current message.  Used to display
  a message in an alternate window.	  

  Args: cmd - what type of scroll operation.
	text - filled with pointer to text.
	l - length of text.
	style - Returns style of text.  Can be:
		GETTEXT_TEXT - Is a pointer to text with CRLF deliminated
				lines
		GETTEXT_LINES - Is a pointer to NULL terminated array of
				char *.  Each entry points to a line of
				text.
					
		this implementation always returns GETTEXT_TEXT.

  Returns: TRUE - did the scroll operation.
	   FALSE - was not able to do the scroll operation.
 ----*/
int
index_scroll_callback (cmd, scroll_pos)
int	cmd;
long	scroll_pos;
{
    int paint = TRUE;

    switch (cmd) {
      case MSWIN_KEY_SCROLLUPLINE:
	paint = index_scroll_up (scroll_pos);
	break;

      case MSWIN_KEY_SCROLLDOWNLINE:
	paint = index_scroll_down (scroll_pos);
	break;

      case MSWIN_KEY_SCROLLUPPAGE:
	paint = index_scroll_up (current_index_state->lines_per_page);
	break;

      case MSWIN_KEY_SCROLLDOWNPAGE:
	paint = index_scroll_down (current_index_state->lines_per_page);
	break;

      case MSWIN_KEY_SCROLLTO:
	/* Normalize msgno in zoomed case */
	if(any_lflagged(ps_global->msgmap, MN_HIDE | MN_CHID)){
	    long n, x;

	    for(n = 1L, x = 0;
		x < scroll_pos && n < mn_get_total(ps_global->msgmap);
		n++)
	      if(!msgline_hidden(ps_global->mail_stream, ps_global->msgmap,
			         n, 0))
		x++;

	    scroll_pos = n - 1;	/* list-position --> message number  */
	}

	paint = index_scroll_to_pos (scroll_pos + 1);
	break;
    }

    if(paint){
	mswin_beginupdate();
	update_titlebar_message();
	update_titlebar_status();
	redraw_index_body();
	mswin_endupdate();
    }

    return(paint);
}


/*----------------------------------------------------------------------
     MSWin scroll callback to get the text of the current message

  Args: title - title for new window
	text - 
	l - 
	style - 

  Returns: TRUE - got the requested text
	   FALSE - was not able to get the requested text
 ----*/
int
index_gettext_callback(title, titlelen, text, l, style)
    char  *title;
    size_t titlelen;
    void **text;
    long  *l;
    int   *style;
{
    int	      rv = 0;
    ENVELOPE *env;
    BODY     *body;
    STORE_S  *so;
    gf_io_t   pc;

    if(mn_get_total(ps_global->msgmap) > 0L
       && (so = so_get(CharStar, NULL, WRITE_ACCESS))){
	gf_set_so_writec(&pc, so);

	if((env = pine_mail_fetchstructure(ps_global->mail_stream,
					   mn_m2raw(ps_global->msgmap,
					       mn_get_cur(ps_global->msgmap)),
					   &body))
	   && format_message(mn_m2raw(ps_global->msgmap,
				      mn_get_cur(ps_global->msgmap)),
			     env, body, NULL, FM_NEW_MESS, pc)){
	    snprintf(title, titlelen, "Folder %s  --  Message %ld of %ld",
		    strsquish(tmp_20k_buf + 500, SIZEOF_20KBUF-500, ps_global->cur_folder, 50),
		    mn_get_cur(ps_global->msgmap),
		    mn_get_total(ps_global->msgmap));
	    title[titlelen-1] = '\0';
	    *text  = so_text(so);
	    *l     = strlen((char *)so_text(so));
	    *style = GETTEXT_TEXT;

	    /* free alloc'd so, but preserve the text passed back to caller */
	    so->txt = (void *) NULL;
	    rv = 1;
	}

	gf_clear_so_writec(so);
	so_give(&so);
    }

    return(rv);
}


/*
 *
 */
int
index_sort_callback(set, order)
    int  set;
    long order;
{
    int i = 0;

    if(set){
	sort_folder(ps_global->mail_stream, ps_global->msgmap,
		    order & 0x000000ff,
		    (order & 0x00000100) != 0, SRT_VRB);
	mswin_beginupdate();
	update_titlebar_message();
	update_titlebar_status();
	redraw_index_body();
	mswin_endupdate();
	flush_status_messages(1);
    }
    else{
	i = (int) mn_get_sort(ps_global->msgmap);
	if(mn_get_revsort(ps_global->msgmap))
	  i |= 0x0100;
    }

    return(i);
}


/*
 *
 */
void
index_popup(IndexType style, MAILSTREAM *stream, MSGNO_S *msgmap, int full)
{
    int		  n = 0;
    int           view_in_new_wind_index = -1;
    long          rawno;
    MESSAGECACHE *mc;
    MPopup	  view_index_popup[32];
    struct key_menu *km = (style == ThreadIndex)
			    ? &thread_keymenu
			    : (ps_global->mail_stream != stream)
			      ? &simple_index_keymenu
			      : &index_keymenu;

    /*
     * Loosely follow the logic in do_index_border to figure
     * out which commands to show.
     */

    if(full){
	if(km != &simple_index_keymenu){
	    view_index_popup[n].type         = tQueue;
	    view_index_popup[n].label.string = (km == &thread_keymenu)
						 ? "&View Thread" : "&View";
	    view_index_popup[n].label.style  = lNormal;
	    view_index_popup[n++].data.val   = 'V';
	}

	if(km == &index_keymenu){
	    view_in_new_wind_index = n;
	    view_index_popup[n].type           = tIndex;
	    view_index_popup[n].label.style    = lNormal;
	    view_index_popup[n++].label.string = "View in New Window";
	}

	if(km != &simple_index_keymenu)
	  view_index_popup[n++].type = tSeparator;

	if(km == &thread_keymenu){
	    view_index_popup[n].type         = tQueue;
	    view_index_popup[n].label.string = "&Delete Thread";
	    view_index_popup[n].label.style  = lNormal;
	    view_index_popup[n++].data.val   = 'D';

	    view_index_popup[n].type         = tQueue;
	    view_index_popup[n].label.string = "&UnDelete Thread";
	    view_index_popup[n].label.style  = lNormal;
	    view_index_popup[n++].data.val   = 'U';
	}
	else{
	    /* Make "delete/undelete" item sensitive */
	    mc = ((rawno = mn_m2raw(msgmap, mn_get_cur(msgmap))) > 0L
		  && stream && rawno <= stream->nmsgs)
		  ? mail_elt(stream, rawno) : NULL;
	    view_index_popup[n].type        = tQueue;
	    view_index_popup[n].label.style = lNormal;
	    if(mc && mc->deleted){
		view_index_popup[n].label.string   = "&Undelete";
		view_index_popup[n++].data.val     = 'U';
	    }
	    else{
		view_index_popup[n].label.string   = "&Delete";
		view_index_popup[n++].data.val     = 'D';
	    }
	}

	if(km == &index_keymenu && F_ON(F_ENABLE_FLAG, ps_global)){
	    view_index_popup[n].type            = tSubMenu;
	    view_index_popup[n].label.string    = "Flag";
	    view_index_popup[n++].data.submenu  = flag_submenu(mc);
	}

	if(km == &simple_index_keymenu){
	    view_index_popup[n].type         = tQueue;
	    view_index_popup[n].label.style  = lNormal;
	    view_index_popup[n].label.string = "&Select";
	    view_index_popup[n++].data.val   = 'S';
	}
	else{
	    view_index_popup[n].type         = tQueue;
	    view_index_popup[n].label.style  = lNormal;
	    view_index_popup[n].label.string = (km == &thread_keymenu)
						 ? "&Save Thread" : "&Save";
	    view_index_popup[n++].data.val   = 'S';

	    view_index_popup[n].type         = tQueue;
	    view_index_popup[n].label.style  = lNormal;
	    view_index_popup[n].label.string = (km == &thread_keymenu)
						 ? "&Export Thread" : "&Export";
	    view_index_popup[n++].data.val   = 'E';

	    view_index_popup[n].type         = tQueue;
	    view_index_popup[n].label.style  = lNormal;
	    view_index_popup[n].label.string = "Print";
	    view_index_popup[n++].data.val   = '%';

	    view_index_popup[n].type         = tQueue;
	    view_index_popup[n].label.style  = lNormal;
	    view_index_popup[n].label.string = (km == &thread_keymenu)
						 ? "&Reply To Thread" : "&Reply";
	    view_index_popup[n++].data.val   = 'R';

	    view_index_popup[n].type         = tQueue;
	    view_index_popup[n].label.style  = lNormal;
	    view_index_popup[n].label.string = (km == &thread_keymenu)
						 ? "&Forward Thread" : "&Forward";
	    view_index_popup[n++].data.val   = 'F';

	    if(F_ON(F_ENABLE_BOUNCE, ps_global)){
		view_index_popup[n].type         = tQueue;
		view_index_popup[n].label.style  = lNormal;
		view_index_popup[n].label.string = (km == &thread_keymenu)
						     ? "&Bounce Thread" : "&Bounce";
		view_index_popup[n++].data.val   = 'B';
	    }

	    view_index_popup[n].type         = tQueue;
	    view_index_popup[n].label.style  = lNormal;
	    view_index_popup[n].label.string = "&Take Addresses";
	    view_index_popup[n++].data.val   = 'T';

	    if(F_ON(F_ENABLE_AGG_OPS, ps_global)){
		view_index_popup[n].type         = tQueue;
		view_index_popup[n].label.style  = lNormal;
		view_index_popup[n].label.string = "[Un]Select Current";
		view_index_popup[n++].data.val   = ':';
	    }
	}

	view_index_popup[n].type         = tQueue;
	view_index_popup[n].label.style  = lNormal;
	view_index_popup[n].label.string = "&WhereIs";
	view_index_popup[n++].data.val   = 'W';

	view_index_popup[n++].type = tSeparator;
    }

    if(km == &simple_index_keymenu){
	view_index_popup[n].type         = tQueue;
	view_index_popup[n].label.style  = lNormal;
	view_index_popup[n].label.string = "&Exit Select";
	view_index_popup[n++].data.val   = 'E';
    }
    else if(km == &index_keymenu && THREADING() && sp_viewing_a_thread(stream)){
	view_index_popup[n].type         = tQueue;
	view_index_popup[n].label.style  = lNormal;
	view_index_popup[n].label.string = "Thread Index";
	view_index_popup[n++].data.val   = '<';
    }
    else{
	view_index_popup[n].type         = tQueue;
	view_index_popup[n].label.style  = lNormal;
	view_index_popup[n].label.string = "Folder &List";
	view_index_popup[n++].data.val   = '<';
    }

    view_index_popup[n].type	     = tQueue;
    view_index_popup[n].label.style  = lNormal;
    view_index_popup[n].label.string = "&Main Menu";
    view_index_popup[n++].data.val   = 'M';

    view_index_popup[n].type = tTail;

    if(mswin_popup(view_index_popup) == view_in_new_wind_index
       && view_in_new_wind_index >= 0)
      view_in_new_window();
}


char *
pcpine_help_index(title)
    char *title;
{
    /* 
     * Title is size 256 in pico. Put in args.
     */
    if(title)
      strncpy(title, "Alpine MESSAGE INDEX Help", 256);

    return(pcpine_help(h_mail_index));
}

char *
pcpine_help_index_simple(title)
    char *title;
{
    /* 
     * Title is size 256 in pico. Put in args.
     */
    if(title)
      strncpy(title, "Alpine SELECT MESSAGE Help", 256);

    return(pcpine_help(h_simple_index));
}


#include "../pico/osdep/mswin_tw.h"


void
view_in_new_window(void)
{
    char              title[GETTEXT_TITLELEN+1];
    void             *text;
    long              len;
    int	              format;
    MSWIN_TEXTWINDOW *mswin_tw;

    /* Launch text in alt window. */
    if(index_gettext_callback(title, sizeof (title), &text, &len, &format)){
	if(format == GETTEXT_TEXT) 
	  mswin_tw = mswin_displaytext(title, text, (size_t) len, NULL,
				       NULL, MSWIN_DT_USEALTWINDOW);
	else if(format == GETTEXT_LINES) 
	  mswin_tw = mswin_displaytext(title, NULL, 0, text,
				       NULL, MSWIN_DT_USEALTWINDOW);

	if(mswin_tw)
	  mswin_set_readonly(mswin_tw, FALSE);
    }
}

#endif	/* _WINDOWS */
