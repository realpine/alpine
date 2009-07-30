#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: mailview.c 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $";
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

/*======================================================================

     mailview.c

     Implements the mailview screen
     Also includes scrolltool used to display help text

  ====*/

#include "headers.h"
#include "mailcmd.h"
#include "mailview.h"
#include "mailindx.h"
#include "mailpart.h"
#include "adrbkcmd.h"
#include "keymenu.h"
#include "status.h"
#include "radio.h"
#include "help.h"
#include "imap.h"
#include "reply.h"
#include "folder.h"
#include "alpine.h"
#include "titlebar.h"
#include "signal.h"
#include "send.h"
#include "dispfilt.h"
#include "busy.h"
#include "smime.h"
#include "../pith/conf.h"
#include "../pith/filter.h"
#include "../pith/msgno.h"
#include "../pith/escapes.h"
#include "../pith/flag.h"
#include "../pith/mimedesc.h"
#include "../pith/url.h"
#include "../pith/bldaddr.h"
#include "../pith/mailcmd.h"
#include "../pith/newmail.h"
#include "../pith/pipe.h"
#include "../pith/thread.h"
#include "../pith/util.h"
#include "../pith/detoken.h"
#include "../pith/editorial.h"
#include "../pith/maillist.h"
#include "../pith/hist.h"
#include "../pith/busy.h"
#include "../pith/list.h"
#include "../pith/detach.h"


/*----------------------------------------------------------------------
    Saved state for scrolling text 
 ----*/
typedef struct scroll_text {
    SCROLL_S	*parms;		/* Original text (file, char *, char **)     */
    char       **text_lines,	/* Lines to display			     */
		*fname;		/* filename of line offsets in "text"	     */
    FILE	*findex;	/* file pointer to line offsets in "text"    */
    short	*line_lengths;	/* Length of each line in "text_lines"	     */
    long	 top_text_line,	/* index in "text_lines" top displayed line  */
		 num_lines;	/* number of valid pointers in "text_lines"  */
    int		 lines_allocated; /* size of "text_lines" array		     */
    struct {
	int length,		/* count of displayable lines (== PGSIZE)    */
	    width,		/* width of displayable lines		     */
	    start_line,		/* line number to start painting on	     */
	    other_lines;	/* # of lines not for scroll text	     */
    } screen;			/* screen parameters			     */
} SCRLCTRL_S;


typedef struct scroll_file {
    long	offset;
    int		len;
} SCRLFILE_S;


/*
 * Struct to help write lines do display as they're decoded
 */
struct view_write_s {
    char      *line;
    int	       index,
	       screen_line,
	       last_screen_line;
#ifdef	_WINDOWS
    long       lines;
#endif
    HANDLE_S **handles;
    STORE_S   *store;
} *g_view_write;

#define LINEBUFSIZ (4096)

#define MAX_FUDGE (1024*1024)

/*
 * Definitions to help scrolltool
 */
#define SCROLL_LINES_ABOVE(X)	HEADER_ROWS(X)
#define SCROLL_LINES_BELOW(X)	FOOTER_ROWS(X)
#define	SCROLL_LINES(X)		MAX(((X)->ttyo->screen_rows               \
			 - SCROLL_LINES_ABOVE(X) - SCROLL_LINES_BELOW(X)), 0)
#define	scroll_text_lines()	(scroll_state(SS_CUR)->num_lines)


/*
 * Definitions for various scroll state manager's functions
 */
#define	SS_NEW	1
#define	SS_CUR	2
#define	SS_FREE	3


/*
 * Handle hints.
 */
#define	HANDLE_INIT_MSG		\
  _("Selectable items in text -- Use Up/Down Arrows to choose, Return to view")
#define	HANDLE_ABOVE_ERR	\
  _("No selected item displayed -- Use PrevPage to bring choice into view")
#define	HANDLE_BELOW_ERR	\
  _("No selected item displayed -- Use NextPage to bring choice into view")


#define	PGSIZE(X)      (ps_global->ttyo->screen_rows - (X)->screen.other_lines)

#define TYPICAL_BIG_MESSAGE_LINES	200


/*
 * Internal prototypes
 */
void	    view_writec_killbuf(void);
int	    view_end_scroll(SCROLL_S *);
long	    format_size_guess(BODY *);
int	    scroll_handle_prompt(HANDLE_S *, int);
int	    scroll_handle_launch(HANDLE_S *, int);
int	    scroll_handle_obscured(HANDLE_S *);
HANDLE_S   *scroll_handle_in_frame(long);
long	    scroll_handle_reframe(int, int);
int	    handle_on_line(long, int);
int	    handle_on_page(HANDLE_S *, long, long);
int	    scroll_handle_selectable(HANDLE_S *);
HANDLE_S   *scroll_handle_next_sel(HANDLE_S *);
HANDLE_S   *scroll_handle_prev_sel(HANDLE_S *);
HANDLE_S   *scroll_handle_next(HANDLE_S *);
HANDLE_S   *scroll_handle_prev(HANDLE_S *);
HANDLE_S   *scroll_handle_boundary(HANDLE_S *, HANDLE_S *(*)(HANDLE_S *));
int	    scroll_handle_column(int, int);
int	    scroll_handle_index(int, int);
void	    scroll_handle_set_loc(POSLIST_S **, int, int);
int	    dot_on_handle(long, int);
int	    url_launch(HANDLE_S *);
int	    url_launch_too_long(int);
char	   *url_external_handler(HANDLE_S *, int);
void	    url_mailto_addr(ADDRESS **, char *);
int	    url_local_imap(char *);
int	    url_local_nntp(char *);
int	    url_local_news(char *);
int	    url_local_file(char *);
int	    url_local_phone_home(char *);
static int  print_to_printer(SCROLL_S *);
int	    search_text(int, long, int, char **, Pos *, int *);
void	    update_scroll_titlebar(long, int);
SCRLCTRL_S *scroll_state(int);
void	    set_scroll_text(SCROLL_S *, long, SCRLCTRL_S *);
void	    redraw_scroll_text(void);
void	    zero_scroll_text(void);
void	    format_scroll_text(void);
void	    ScrollFile(long);
long	    make_file_index(void);
char	   *scroll_file_line(FILE *, char *, SCRLFILE_S *, int *);
long	    scroll_scroll_text(long, HANDLE_S *, int);
int	    search_scroll_text(long, int, char *, Pos *, int *);
char	   *search_scroll_line(char *, char *, int, int);
int         visible_linelen(int);
long        doubleclick_handle(SCROLL_S *, HANDLE_S *, int *, int *);
#ifdef	_WINDOWS
int	    format_message_popup(SCROLL_S *, int);
int	    simple_text_popup(SCROLL_S *, int);
int	    mswin_readscrollbuf(int);
int	    pcpine_do_scroll(int, long);
char	   *pcpine_help_scroll(char *);
int	    pcpine_view_cursor(int, long);
#endif



/*----------------------------------------------------------------------
     Format a buffer with the text of the current message for browser

    Args: ps - pine state structure
  
  Result: The scrolltool is called to display the message

  Loop here viewing mail until the folder changed or a command takes
us to another screen. Inside the loop the message text is fetched and
formatted into a buffer allocated for it.  These are passed to the
scrolltool(), that displays the message and executes commands. It
returns when it's time to display a different message, when we
change folders, when it's time for a different screen, or when
there are no more messages available.
  ---*/

void
mail_view_screen(struct pine *ps)
{
    char            last_was_full_header = 0;
    long            last_message_viewed = -1L, raw_msgno, offset = 0L;
    OtherMenu       save_what = FirstMenu;
    int             we_cancel = 0, flags, cmd = 0;
    int             force_prefer = 0;
    MESSAGECACHE   *mc;
    ENVELOPE       *env;
    BODY           *body;
    STORE_S        *store;
    HANDLE_S	   *handles = NULL;
    SCROLL_S	    scrollargs;
    SourceType	    src = CharStar;

    dprint((1, "\n\n  -----  MAIL VIEW  -----\n"));

    ps->prev_screen = mail_view_screen;
    ps->force_prefer_plain = ps->force_no_prefer_plain = 0;

    if(ps->ttyo->screen_rows - HEADER_ROWS(ps) - FOOTER_ROWS(ps) < 1){
	q_status_message(SM_ORDER | SM_DING, 0, 3,
			 _("Screen too small to view message"));
	ps->next_screen = mail_index_screen;
	return;
    }

    /*----------------- Loop viewing messages ------------------*/
    do {

	ps->user_says_cancel = 0;
	ps->some_quoting_was_suppressed = 0;

	/*
	 * Check total to make sure there's something to view.  Check it
	 * inside the loop to make sure everything wasn't expunged while
	 * we were viewing.  If so, make sure we don't just come back.
	 */
	if(mn_get_total(ps->msgmap) <= 0L || !ps->mail_stream){
	    q_status_message(SM_ORDER, 0, 3, _("No messages to read!"));
	    ps->next_screen = mail_index_screen;
	    break;
	}

	we_cancel = busy_cue(NULL, NULL, 1);

	if(mn_get_cur(ps->msgmap) <= 0L)
	  mn_set_cur(ps->msgmap,
		     THREADING() ? first_sorted_flagged(F_NONE,
							ps->mail_stream,
							0L, FSF_SKIP_CHID)
				 : 1L);

	raw_msgno = mn_m2raw(ps->msgmap, mn_get_cur(ps->msgmap));
	body      = NULL;
	if(raw_msgno == 0L
	   || !(env = pine_mail_fetchstructure(ps->mail_stream,raw_msgno,&body))
	   || !(raw_msgno > 0L && ps->mail_stream
	        && raw_msgno <= ps->mail_stream->nmsgs
		&& (mc = mail_elt(ps->mail_stream, raw_msgno)))){
	    q_status_message1(SM_ORDER, 3, 3,
			      "Error getting message %s data",
			      comatose(mn_get_cur(ps->msgmap)));
	    dprint((1, "!!!! ERROR fetching %s of msg %ld\n",
		       env ? "elt" : "env", mn_get_cur(ps->msgmap)));
		       
	    ps->next_screen = mail_index_screen;
	    break;
	}
	else
	  ps->unseen_in_view = !mc->seen;

	init_handles(&handles);

	store = so_get(src, NULL, EDIT_ACCESS);
	so_truncate(store, format_size_guess(body) + 2048);

	view_writec_init(store, &handles, SCROLL_LINES_ABOVE(ps),
			 SCROLL_LINES_ABOVE(ps) + 
			 ps->ttyo->screen_rows - (SCROLL_LINES_ABOVE(ps)
						  + SCROLL_LINES_BELOW(ps)));

	flags = FM_DISPLAY;
	if(last_message_viewed != mn_get_cur(ps->msgmap)
	   || last_was_full_header == 2 || cmd == MC_TOGGLE)
	  flags |= FM_NEW_MESS;

	if(F_OFF(F_QUELL_FULL_HDR_RESET, ps_global)
	   && last_message_viewed != -1L
	   && last_message_viewed != mn_get_cur(ps->msgmap))
	  ps->full_header = 0;

	if(offset)		/* no pre-paint during resize */
	  view_writec_killbuf();

#ifdef SMIME
	/* Attempt to handle S/MIME bodies */
	if(ps->smime)
	  ps->smime->need_passphrase = 0;

	if(F_OFF(F_DONT_DO_SMIME, ps_global) && fiddle_smime_message(body, raw_msgno))
	 flags |= FM_NEW_MESS; /* body was changed, force a reload */
#endif

#ifdef _WINDOWS
	mswin_noscrollupdate(1);
#endif
	ps->cur_uid_stream = ps->mail_stream;
	ps->cur_uid = mail_uid(ps->mail_stream, raw_msgno);
	(void) format_message(raw_msgno, env, body, &handles, flags | force_prefer,
			      view_writec);
#ifdef _WINDOWS
	mswin_noscrollupdate(0);
#endif

	view_writec_destroy();

	last_message_viewed  = mn_get_cur(ps->msgmap);
	last_was_full_header = ps->full_header;

	ps->next_screen = SCREEN_FUN_NULL;

	memset(&scrollargs, 0, sizeof(SCROLL_S));
	scrollargs.text.text	= so_text(store);
	scrollargs.text.src	= src;
	scrollargs.text.desc	= "message";

	/*
	 * make first selectable handle the default
	 */
	if(handles){
	    HANDLE_S *hp;

	    hp = handles;
	    while(!scroll_handle_selectable(hp) && hp != NULL)
	      hp = hp->next;

	    if((scrollargs.text.handles = hp) != NULL)
	      scrollargs.body_valid = (hp == handles);
	    else
	      free_handles(&handles);
	}
	else
	  scrollargs.body_valid = 1;

	if(offset){		/* resize?  preserve paging! */
	    scrollargs.start.on		= Offset;
	    scrollargs.start.loc.offset = offset;
	    scrollargs.body_valid = 0;
	    offset = 0L;
	}
	  
	scrollargs.use_indexline_color = 1;

	/* TRANSLATORS: a screen title */
	scrollargs.bar.title	= _("MESSAGE TEXT");
	scrollargs.end_scroll	= view_end_scroll;
	scrollargs.resize_exit	= 1;
	scrollargs.help.text	= h_mail_view;
	scrollargs.help.title	= _("HELP FOR MESSAGE TEXT VIEW");
	scrollargs.keys.menu	= &view_keymenu;
	scrollargs.keys.what    = save_what;
	setbitmap(scrollargs.keys.bitmap);
	if(F_OFF(F_ENABLE_PIPE, ps_global))
	  clrbitn(VIEW_PIPE_KEY, scrollargs.keys.bitmap);

	/* 
	 * turn off attachment viewing for raw msg txt, atts 
	 * haven't been set up at this point
	 */
	if(ps_global->full_header == 2
	   && F_ON(F_ENABLE_FULL_HDR_AND_TEXT, ps_global))
	  clrbitn(VIEW_ATT_KEY, scrollargs.keys.bitmap);

	if(F_OFF(F_ENABLE_BOUNCE, ps_global))
	  clrbitn(BOUNCE_KEY, scrollargs.keys.bitmap);

	if(F_OFF(F_ENABLE_FLAG, ps_global))
	  clrbitn(FLAG_KEY, scrollargs.keys.bitmap);

	if(F_OFF(F_ENABLE_AGG_OPS, ps_global))
	  clrbitn(VIEW_SELECT_KEY, scrollargs.keys.bitmap);

	if(F_OFF(F_ENABLE_FULL_HDR, ps_global))
	  clrbitn(VIEW_FULL_HEADERS_KEY, scrollargs.keys.bitmap);

#ifdef SMIME
	if(!(ps->smime && ps->smime->need_passphrase))
	  clrbitn(DECRYPT_KEY, scrollargs.keys.bitmap);

	if(F_ON(F_DONT_DO_SMIME, ps_global)){
	    clrbitn(DECRYPT_KEY, scrollargs.keys.bitmap);
	    clrbitn(SECURITY_KEY, scrollargs.keys.bitmap);
	}
#endif

	if(!handles){
	    /*
	     * NOTE: the comment below only really makes sense if we
	     *	     decide to replace the "Attachment Index" with
	     *	     a model that lets you highlight the attachments
	     *	     in the header.  In a way its consistent, but
	     *	     would mean more keymenu monkeybusiness since not
	     *	     all things would be available all the time. No wait.
	     *	     Then what would "Save" mean; the attachment, url or
	     *	     message you're currently viewing?  Would Save
	     *	     of a message then only be possible from the message
	     *	     index?  Clumsy.  what about arrow-navigation.  isn't
	     *	     that now inconsistent?  Maybe next/prev url shouldn't
	     *	     be bound to the arrow/^N/^P navigation?
	     */
	    clrbitn(VIEW_VIEW_HANDLE, scrollargs.keys.bitmap);
	    clrbitn(VIEW_PREV_HANDLE, scrollargs.keys.bitmap);
	    clrbitn(VIEW_NEXT_HANDLE, scrollargs.keys.bitmap);
	}

#ifdef	_WINDOWS
	scrollargs.mouse.popup = format_message_popup;
#endif

	if(((cmd = scrolltool(&scrollargs)) == MC_RESIZE
	    || (cmd == MC_FULLHDR && ps_global->full_header == 1))
	   && scrollargs.start.on == Offset)
	  offset = scrollargs.start.loc.offset;

	if(cmd == MC_TOGGLE && ps->force_prefer_plain == 0 && ps->force_no_prefer_plain == 0){
	    if(F_ON(F_PREFER_PLAIN_TEXT, ps_global))
	      ps->force_no_prefer_plain = 1;
	    else
	      ps->force_prefer_plain = 1;
	}
	else{
	    ps->force_prefer_plain = ps->force_no_prefer_plain = 0;
	}

	/*
	 * We could use the values directly but this is the way it
	 * already worked with the flags, so leave it alone.
	 */
	if(ps->force_prefer_plain == 0 && ps->force_no_prefer_plain == 0)
	  force_prefer = 0;
	else if(ps->force_prefer_plain)
	  force_prefer = FM_FORCEPREFPLN;
	else if(ps->force_no_prefer_plain)
	  force_prefer = FM_FORCENOPREFPLN;

	save_what = scrollargs.keys.what;
	ps_global->unseen_in_view = 0;
	so_give(&store);	/* free resources associated with store */
	free_handles(&handles);
#ifdef	_WINDOWS
	mswin_destroyicons();
#endif
    }
    while(ps->next_screen == SCREEN_FUN_NULL);

    if(we_cancel)
      cancel_busy_cue(-1);

    ps->force_prefer_plain = ps->force_no_prefer_plain = 0;

    ps->cur_uid_stream = NULL;
    ps->cur_uid = 0;

    /*
     * Unless we're going into attachment screen,
     * start over with  full_header.
     */
    if(F_OFF(F_QUELL_FULL_HDR_RESET, ps_global)
       && ps->next_screen != attachment_screen)
      ps->full_header = 0;
}



/*
 * view_writec_init - function to create and init struct that manages
 *		      writing to the display what we can as soon
 *		      as we can.
 */
void
view_writec_init(STORE_S *store, HANDLE_S **handlesp, int first_line, int last_line)
{
    char tmp[MAILTMPLEN];

    g_view_write = (struct view_write_s *)fs_get(sizeof(struct view_write_s));
    memset(g_view_write, 0, sizeof(struct view_write_s));
    g_view_write->store		   = store;
    g_view_write->handles	   = handlesp;
    g_view_write->screen_line	   = first_line;
    g_view_write->last_screen_line = last_line;

    if(!dfilter_trigger(NULL, tmp, sizeof(tmp))){
	g_view_write->line = (char *) fs_get(LINEBUFSIZ*sizeof(char));
#ifdef _WINDOWS
	mswin_beginupdate();
	scroll_setrange(0L, 0L);
#endif

	if(ps_global->VAR_DISPLAY_FILTERS)
	  ClearLines(first_line, last_line - 1);
    }
}



void
view_writec_destroy(void)
{
    if(g_view_write){
	if(g_view_write->line && g_view_write->index)
	  view_writec('\n');		/* flush pending output! */

	while(g_view_write->screen_line < g_view_write->last_screen_line)
	  ClearLine(g_view_write->screen_line++);

	view_writec_killbuf();

	fs_give((void **) &g_view_write);
    }

#ifdef _WINDOWS
    mswin_endupdate();
#endif
}



void
view_writec_killbuf(void)
{
    if(g_view_write->line)
      fs_give((void **) &g_view_write->line);
}



/*
 * view_screen_pc - write chars into the final buffer
 */
int
view_writec(int c)
{
    static int in_color = 0;

    if(g_view_write->line){
	/*
	 * This only works if the 2nd and 3rd parts of the || don't happen.
	 * The only way it breaks is if we get a really, really long line
	 * because there are oodles of tags in it.  In that case we will
	 * wrap incorrectly or split a tag across lines (losing color perhaps)
	 * but we won't crash.
	 */
	if(c == '\n' ||
	   (!in_color &&
	    (char)c != TAG_EMBED &&
	    g_view_write->index >= (LINEBUFSIZ - 20 * (RGBLEN+2))) ||
	   (g_view_write->index >= LINEBUFSIZ - 1)){
	    int rv;

	    in_color = 0;
	    suspend_busy_cue();
	    ClearLine(g_view_write->screen_line);
	    if(c != '\n')
	      g_view_write->line[g_view_write->index++] = (char) c;

	    PutLine0n8b(g_view_write->screen_line++, 0,
			g_view_write->line, g_view_write->index,
			g_view_write->handles ? *g_view_write->handles : NULL);

	    resume_busy_cue(0);
	    rv = so_nputs(g_view_write->store,
			  g_view_write->line,
			  g_view_write->index);
	    g_view_write->index = 0;

	    if(g_view_write->screen_line >= g_view_write->last_screen_line){
		fs_give((void **) &g_view_write->line);
		fflush(stdout);
	    }

	    if(!rv)
	      return(0);
	    else if(c != '\n')
	      return(1);
	}
	else{
	    /*
	     * Don't split embedded things over multiple lines. Colors are
	     * the longest tags so use their length.
	     */
	    if((char)c == TAG_EMBED)
	      in_color = RGBLEN+1;
	    else if(in_color)
	      in_color--;

	    g_view_write->line[g_view_write->index++] = (char) c;
	    return(1);
	}
    }
#ifdef	_WINDOWS
    else if(c == '\n' && g_view_write->lines++ > SCROLL_LINES(ps_global))
      scroll_setrange(SCROLL_LINES(ps_global),
		      g_view_write->lines + SCROLL_LINES(ps_global));
#endif

    return(so_writec(c, g_view_write->store));
}

int
view_end_scroll(SCROLL_S *sparms)
{
    int done = 0, result, force;
    
    if(F_ON(F_ENABLE_SPACE_AS_TAB, ps_global)){

	if(F_ON(F_ENABLE_TAB_DELETES, ps_global)){
	    long save_msgno;

	    /* Let the TAB advance cur msgno for us */
	    save_msgno = mn_get_cur(ps_global->msgmap);
	    (void) cmd_delete(ps_global, ps_global->msgmap, MCMD_NONE, NULL);
	    mn_set_cur(ps_global->msgmap, save_msgno);
	}

	/* act like the user hit a TAB */
	result = process_cmd(ps_global, ps_global->mail_stream,
			     ps_global->msgmap, MC_TAB, View, &force);

	if(result == 1)
	  done = 1;
    }

    return(done);
}


/*
 * format_size_guess -- Run down the given body summing the text/plain
 *			pieces we're likely to display.  It need only
 *			be a guess since this is intended for preallocating
 *			our display buffer...
 */
long
format_size_guess(struct mail_bodystruct *body)
{
    long  size = 0L;
    long  extra = 0L;
    char *free_me = NULL;

    if(body){
	if(body->type == TYPEMULTIPART){
	    PART *part;

	    for(part = body->nested.part; part; part = part->next)
	      size += format_size_guess(&part->body);
	}
	else if(body->type == TYPEMESSAGE
		&& body->subtype && !strucmp(body->subtype, "rfc822"))
	  size = format_size_guess(body->nested.msg->body);
	else if((!body->type || body->type == TYPETEXT)
		&& (!body->subtype || !strucmp(body->subtype, "plain"))
		&& ((body->disposition.type
		     && !strucmp(body->disposition.type, "inline"))
		    || !(free_me = parameter_val(body->parameter, "name")))){
	    /*
	     * Handles and colored quotes cause memory overhead. Figure about
	     * 100 bytes per level of quote per line and about 100 bytes per
	     * handle. Make a guess of 1 handle or colored quote per
	     * 10 lines of message. If we guess too low, we'll do a resize in
	     * so_cs_writec. Most of the overhead comes from the colors, so
	     * if we can see we don't do colors or don't have these features
	     * turned on, skip it.
	     */
	    if(pico_usingcolor() &&
	       ((ps_global->VAR_QUOTE1_FORE_COLOR &&
	         ps_global->VAR_QUOTE1_BACK_COLOR) ||
	        F_ON(F_VIEW_SEL_URL, ps_global) ||
	        F_ON(F_VIEW_SEL_URL_HOST, ps_global) ||
	        F_ON(F_SCAN_ADDR, ps_global)))
	      extra = MIN(100/10 * body->size.lines, MAX_FUDGE);

	    size = body->size.bytes + extra;
	}

	if(free_me)
	  fs_give((void **) &free_me);
    }

    return(size);
}


int
scroll_handle_prompt(HANDLE_S *handle, int force)
{
    char prompt[256], tmp[MAILTMPLEN];
    int  rc, flags, local_h;
    static ESCKEY_S launch_opts[] = {
	/* TRANSLATORS: command names, editURL means user gets to edit a URL if they
	   want, editApp is edit application where they edit the application used to
	   view a URL */
	{'y', 'y', "Y", N_("Yes")},
	{'n', 'n', "N", N_("No")},
	{-2, 0, NULL, NULL},
	{-2, 0, NULL, NULL},
	{0, 'u', "U", N_("editURL")},
	{0, 'a', "A", N_("editApp")},
	{-1, 0, NULL, NULL}};

    if(handle->type == URL){
	launch_opts[4].ch = 'u';

	if(!(local_h = !struncmp(handle->h.url.path, "x-alpine-", 9))
	   && (handle->h.url.tool
	       || ((local_h = url_local_handler(handle->h.url.path) != NULL)
		   && (handle->h.url.tool = url_external_handler(handle,1)))
	       || (!local_h
		   && (handle->h.url.tool = url_external_handler(handle,0))))){
#ifdef	_WINDOWS
	    /* if NOT special DDE hack */
	    if(handle->h.url.tool[0] != '*')
#endif
	    if(ps_global->vars[V_BROWSER].is_fixed)
	      launch_opts[5].ch = -1;
	    else
	      launch_opts[5].ch = 'a';
	}
	else{
	    launch_opts[5].ch = -1;
	    if(!local_h){
	      if(ps_global->vars[V_BROWSER].is_fixed){
		  q_status_message(SM_ORDER, 3, 4,
				   _("URL-Viewer is disabled by sys-admin"));
		  return(0);
	      }
	      else{
		/* TRANSLATORS: a question */
		if(want_to(_("No URL-Viewer application defined.  Define now"),
			   'y', 0, NO_HELP, WT_SEQ_SENSITIVE) == 'y'){
		    /* Prompt for the displayer? */
		    tmp[0] = '\0';
		    while(1){
			flags = OE_APPEND_CURRENT |
				OE_SEQ_SENSITIVE |
				OE_KEEP_TRAILING_SPACE;

			rc = optionally_enter(tmp, -FOOTER_ROWS(ps_global), 0,
					      sizeof(tmp),
					      _("Web Browser: "),
					      NULL, NO_HELP, &flags);
			if(rc == 0){
			    if((flags & OE_USER_MODIFIED) && *tmp){
				if(can_access(tmp, EXECUTE_ACCESS) == 0){
				    int	   n;
				    char **l;

				    /*
				     * Save it for next time...
				     */
				    for(l = ps_global->VAR_BROWSER, n = 0;
					l && *l;
					l++)
				      n++; /* count */

				    l = (char **) fs_get((n+2)*sizeof(char *));
				    for(n = 0;
					ps_global->VAR_BROWSER
					  && ps_global->VAR_BROWSER[n];
					n++)
				      l[n] = cpystr(ps_global->VAR_BROWSER[n]);

				    l[n++] = cpystr(tmp);
				    l[n]   = NULL;

				    set_variable_list(V_BROWSER, l, TRUE, Main);
				    free_list_array(&l);

				    handle->h.url.tool = cpystr(tmp);
				    break;
				}
				else{
				    q_status_message1(SM_ORDER | SM_DING, 2, 2,
						    _("Browser not found: %s"),
						     error_description(errno));
				    continue;
				}
			    }
			    else
			      return(0);
			}
			else if(rc == 1 || rc == -1){
			    return(0);
			}
			else if(rc == 4){
			    if(ps_global->redrawer)
			      (*ps_global->redrawer)();
			}
		    }
		}
		else
		  return(0);
	      }
	    }
	}
    }
    else
      launch_opts[4].ch = -1;

    if(force
       || (handle->type == URL
	   && !struncmp(handle->h.url.path, "x-alpine-", 9)))
      return(1);

    while(1){
	int sc = ps_global->ttyo->screen_cols;

	/*
	 * Customize the prompt for mailto, all the other schemes make
	 * sense if you just say View selected URL ...
	 */
	if(handle->type == URL &&
	   !struncmp(handle->h.url.path, "mailto:", 7))
	  snprintf(prompt, sizeof(prompt), "Compose mail to \"%.*s%s\" ? ",
		  MIN(MAX(0,sc - 25), sizeof(prompt)-50), handle->h.url.path+7,
		  (strlen(handle->h.url.path+7) > MAX(0,sc-25)) ? "..." : "");
	else
	  snprintf(prompt, sizeof(prompt), "View selected %s %s%.*s%s ? ",
		  (handle->type == URL) ? "URL" : "Attachment",
		  (handle->type == URL) ? "\"" : "",
		  MIN(MAX(0,sc-27), sizeof(prompt)-50),
		  (handle->type == URL) ? handle->h.url.path : "",
		  (handle->type == URL)
		    ? ((strlen(handle->h.url.path) > MAX(0,sc-27))
			    ? "...\"" : "\"") : "");

	prompt[sizeof(prompt)-1] = '\0';

	switch(radio_buttons(prompt, -FOOTER_ROWS(ps_global),
			     launch_opts, 'y', 'n', NO_HELP, RB_SEQ_SENSITIVE)){
	  case 'y' :
	    return(1);

	  case 'u' :
	    strncpy(tmp, handle->h.url.path, sizeof(tmp)-1);
	    tmp[sizeof(tmp)-1] = '\0';
	    while(1){
		flags = OE_APPEND_CURRENT |
			OE_SEQ_SENSITIVE |
			OE_KEEP_TRAILING_SPACE;

		rc = optionally_enter(tmp, -FOOTER_ROWS(ps_global), 0,
				      sizeof(tmp), _("Edit URL: "),
				      NULL, NO_HELP, &flags);
		if(rc == 0){
		    if(flags & OE_USER_MODIFIED){
			if(handle->h.url.path)
			  fs_give((void **) &handle->h.url.path);

			handle->h.url.path = cpystr(tmp);
		    }

		    break;
		}
		else if(rc == 1 || rc == -1){
		    return(0);
		}
		else if(rc == 4){
		    if(ps_global->redrawer)
		      (*ps_global->redrawer)();
		}
	    }

	    continue;

	  case 'a' :
	    if(handle->h.url.tool){
		strncpy(tmp, handle->h.url.tool, sizeof(tmp)-1);
		tmp[sizeof(tmp)-1] = '\0';
	    }
	    else
	      tmp[0] = '\0';

	    while(1){
		flags = OE_APPEND_CURRENT |
			OE_SEQ_SENSITIVE |
			OE_KEEP_TRAILING_SPACE |
			OE_DISALLOW_HELP;

		rc = optionally_enter(tmp, -FOOTER_ROWS(ps_global), 0,
				      sizeof(tmp), _("Viewer Command: "),
				      NULL, NO_HELP, &flags);
		if(rc == 0){
		    if(flags & OE_USER_MODIFIED){
			if(handle->h.url.tool)
			  fs_give((void **) &handle->h.url.tool);

			handle->h.url.tool = cpystr(tmp);
		    }

		    break;
		}
		else if(rc == 1 || rc == -1){
		    return(0);
		}
		else if(rc == 4){
		    if(ps_global->redrawer)
		      (*ps_global->redrawer)();
		}
	    }

	    continue;

	  case 'n' :
	  default :
	    return(0);
	}
    }
}



int
scroll_handle_launch(HANDLE_S *handle, int force)
{
    switch(handle->type){
      case URL :
	if(handle->h.url.path){
	    if(scroll_handle_prompt(handle, force)){
		if(url_launch(handle)
		   || ps_global->next_screen != SCREEN_FUN_NULL)
		  return(1);	/* done with this screen */
	    }
	    else
	      return(-1);
	}

      break;

      case Attach :
	if(scroll_handle_prompt(handle, force))
	  display_attachment(mn_m2raw(ps_global->msgmap,
				      mn_get_cur(ps_global->msgmap)),
			     handle->h.attach, DA_FROM_VIEW | DA_DIDPROMPT);
	else
	  return(-1);

	break;

      case Folder :
	break;

      case Function :
	(*handle->h.func.f)(handle->h.func.args.stream,
			    handle->h.func.args.msgmap,
			    handle->h.func.args.msgno);
	break;


      default :
	panic("Unexpected HANDLE type");
    }

    return(0);
}


int
scroll_handle_obscured(HANDLE_S *handle)
{
    SCRLCTRL_S *st = scroll_state(SS_CUR);

    return(handle_on_page(handle, st->top_text_line,
			  st->top_text_line + st->screen.length));
}



/*
 * scroll_handle_in_frame -- return handle pointer to visible handle.
 */
HANDLE_S *
scroll_handle_in_frame(long int top_line)
{
    SCRLCTRL_S *st = scroll_state(SS_CUR);
    HANDLE_S   *hp;

    switch(handle_on_page(hp = st->parms->text.handles, top_line,
			  top_line + st->screen.length)){
      case -1 :			/* handle above page */
	/* Find first handle from top of page */
	for(hp = st->parms->text.handles->next; hp; hp = hp->next)
	  if(scroll_handle_selectable(hp))
	    switch(handle_on_page(hp, top_line, top_line + st->screen.length)){
	      case  0 : return(hp);
	      case  1 : return(NULL);
	      case -1 : default : break;
	    }

	break;

      case 1 :			/* handle below page */
	/* Find first handle from top of page */
	for(hp = st->parms->text.handles->prev; hp; hp = hp->prev)
	  if(scroll_handle_selectable(hp))
	    switch(handle_on_page(hp, top_line, top_line + st->screen.length)){
	      case  0 : return(hp);
	      case -1 : return(NULL);
	      case  1 : default : break;
	    }

	break;

      case  0 :
      default :
	break;
    }

    return(hp);
}

/*
 * scroll_handle_reframe -- adjust display params to display given handle
 */
long
scroll_handle_reframe(int key, int center)
{
    long	l, offset, dlines, start_line;
    SCRLCTRL_S *st = scroll_state(SS_CUR);

    dlines     = PGSIZE(st);
    offset     = (st->parms->text.src == FileStar) ? st->top_text_line : 0;
    start_line = st->top_text_line;

    if(key < 0)
      key = st->parms->text.handles->key;

    /* Searc down from the top line */
    for(l = start_line; l < st->num_lines; l++) {
	if(st->parms->text.src == FileStar && l > offset + dlines)
	  ScrollFile(offset += dlines);

	if(handle_on_line(l - offset, key))
	  break;
    }

    if(l < st->num_lines){
	if(l >= dlines + start_line)			/* bingo! */
	  start_line = l - ((center ? (dlines / 2) : dlines) - 1);
    }
    else{
	if(st->parms->text.src == FileStar)		/* wrap offset */
	  ScrollFile(offset = 0);

	for(l = 0; l < start_line; l++) {
	    if(st->parms->text.src == FileStar && l > offset + dlines)
	      ScrollFile(offset += dlines);

	    if(handle_on_line(l - offset, key))
	      break;
	}

	if(l == start_line)
	  panic("Internal Error: no handle found");
	else
	  start_line = l;
    }

    return(start_line);
}


int
handle_on_line(long int line, int goal)
{
    int		i, n, key;
    SCRLCTRL_S *st = scroll_state(SS_CUR);

    for(i = 0; i < st->line_lengths[line]; i++)
      if(st->text_lines[line][i] == TAG_EMBED
	 && st->text_lines[line][++i] == TAG_HANDLE){
	  for(key = 0, n = st->text_lines[line][++i]; n; n--)
	    key = (key * 10) + (st->text_lines[line][++i] - '0');

	  if(key == goal)
	    return(1);
      }

    return(0);
}


int
handle_on_page(HANDLE_S *handle, long int first_line, long int last_line)
{
    POSLIST_S *l;
    int	       rv = 0;

    if(handle && (l = handle->loc)){
	for( ; l; l = l->next)
	  if(l->where.row < first_line){
	      if(!rv)
		rv = -1;
	  }
	  else if(l->where.row >= last_line){
	      if(!rv)
		rv = 1;
	  }
	  else
	    return(0);				/* found! */
    }

    return(rv);
}


int
scroll_handle_selectable(HANDLE_S *handle)
{
    return(handle && (handle->type != URL
		      || (handle->h.url.path && *handle->h.url.path)));
}    


HANDLE_S *
scroll_handle_next_sel(HANDLE_S *handles)
{
    while(handles && !scroll_handle_selectable(handles = handles->next))
      ;

    return(handles);
}


HANDLE_S *
scroll_handle_prev_sel(HANDLE_S *handles)
{
    while(handles && !scroll_handle_selectable(handles = handles->prev))
      ;

    return(handles);
}


HANDLE_S *
scroll_handle_next(HANDLE_S *handles)
{
    HANDLE_S *next = NULL;

    if(scroll_handle_obscured(handles) <= 0){
	next = handles;
	while((next = scroll_handle_next_sel(next))
	      && scroll_handle_obscured(next))
	  ;
    }

    return(next);
}



HANDLE_S *
scroll_handle_prev(HANDLE_S *handles)
{
    HANDLE_S *prev = NULL;

    if(scroll_handle_obscured(handles) >= 0){
	prev = handles;
	while((prev = scroll_handle_prev_sel(prev))
	      && scroll_handle_obscured(prev))
	  ;
    }

    return(prev);
}


HANDLE_S *
scroll_handle_boundary(HANDLE_S *handle, HANDLE_S *(*f) (HANDLE_S *))
{
    HANDLE_S  *hp, *whp = NULL;

    /* Multi-line handle? Punt! */
    if(handle && (!(hp = handle)->loc->next))
      while((hp = (*f)(hp))
	    && hp->loc->where.row == handle->loc->where.row)
	whp = hp;

    return(whp);
}


int
scroll_handle_column(int line, int offset)
{
    SCRLCTRL_S *st = scroll_state(SS_CUR);
    int		i, n, col;
    int         key, limit;

    limit = (offset > -1) ? offset : st->line_lengths[line];

    for(i = 0, col = 0; i < limit;){
      if(st && st->text_lines && st->text_lines[line])
	switch(st->text_lines[line][i]){
	  case TAG_EMBED:
	    i++;
	    switch((i < limit) ? st->text_lines[line][i] : 0){
	      case TAG_HANDLE:
		for(key = 0, n = st->text_lines[line][++i]; n > 0 && i < limit-1; n--)
		  key = (key * 10) + (st->text_lines[line][++i] - '0');

		i++;
		break;

	      case TAG_FGCOLOR :
	      case TAG_BGCOLOR :
		i += (RGBLEN + 1);	/* 1 for TAG, RGBLEN for color */
		break;

	      case TAG_INVON:
	      case TAG_INVOFF:
	      case TAG_BOLDON:
	      case TAG_BOLDOFF:
	      case TAG_ULINEON:
	      case TAG_ULINEOFF:
		i++;
		break;
	    
	      default:	/* literal embed char */
		break;
	    }

	    break;

	  case TAB:
	    i++;
	    while(((++col) &  0x07) != 0) /* add tab's spaces */
	      ;

	    break;

	  default:
	    col += width_at_this_position((unsigned char*) &st->text_lines[line][i],
					  st->line_lengths[line] - i);
	    i++;
	    break;
	}
    }

    return(col);
}


int
scroll_handle_index(int row, int column)
{
    SCRLCTRL_S *st = scroll_state(SS_CUR);
    int index = 0;

    for(index = 0; column > 0;)
      switch(st->text_lines[row][index++]){
	case TAG_EMBED :
	  switch(st->text_lines[row][index++]){
	    case TAG_HANDLE:
	      index += st->text_lines[row][index] + 1;
	      break;

	    case TAG_FGCOLOR :
	    case TAG_BGCOLOR :
	      index += RGBLEN;
	      break;

	    default :
	      break;
	  }

	  break;

	case TAB : /* add tab's spaces */
	  while(((--column) &  0x07) != 0)
	    ;

	  break;

	default :
	  column--;
	  break;
      }

    return(index);
}


void
scroll_handle_set_loc(POSLIST_S **lpp, int line, int column)
{
    POSLIST_S *lp;

    lp = (POSLIST_S *) fs_get(sizeof(POSLIST_S));
    lp->where.row = line;
    lp->where.col = column;
    lp->next	= NULL;
    for(; *lpp; lpp = &(*lpp)->next)
      ;

    *lpp = lp;
}


int
dot_on_handle(long int line, int goal)
{
    int		i = 0, n, key = 0, column = -1;
    SCRLCTRL_S *st = scroll_state(SS_CUR);

    do{
	if(i >= st->line_lengths[line])
	  return(0);

	switch(st->text_lines[line][i++]){
	  case TAG_EMBED :
	    switch(st->text_lines[line][i++]){
	      case TAG_HANDLE :
		for(key = 0, n = st->text_lines[line][i++]; n; n--)
		  key = (key * 10) + (st->text_lines[line][i++] - '0');

		break;

	      case TAG_FGCOLOR :
	      case TAG_BGCOLOR :
		i += RGBLEN;	/* advance past color setting */
		break;

	      case TAG_BOLDOFF :
		key = 0;
		break;
	    }

	    break;

	  case TAB :
	    while(((++column) &  0x07) != 0) /* add tab's spaces */
	      ;

	    break;

	  default :
	    column += width_at_this_position((unsigned char*) &st->text_lines[line][i-1],
					     st->line_lengths[line] - (i-1));
	    break;
	}
    }
    while(column < goal);

    return(key);
}


/*
 * url_launch - Sniff the given url, see if we can do anything with
 *		it.  If not, hand off to user-defined app.
 *
 */
int
url_launch(HANDLE_S *handle)
{
    int	       rv = 0;
    url_tool_t f;
#define	URL_MAX_LAUNCH	(2 * MAILTMPLEN)

    if(handle->h.url.tool){
	char	*toolp, *cmdp, *p, *q, cmd[URL_MAX_LAUNCH + 4];
	char    *left_double_quote, *right_double_quote;
	int	 mode, quotable = 0, copied = 0, double_quoted = 0;
	int      escape_single_quotes = 0;
	PIPE_S  *syspipe;

	toolp = handle->h.url.tool;

	/*
	 * Figure out if we need to quote the URL. If there are shell
	 * metacharacters in it we want to quote it, because we don't want
	 * the shell to interpret them. However, if the user has already
	 * quoted the URL in the command definition we don't want to quote
	 * again. So, we try to see if there are a pair of unescaped
	 * quotes surrounding _URL_ in the cmd.
	 * If we quote when we shouldn't have, it'll cause it not to work.
	 * If we don't quote when we should have, it's a possible security
	 * problem (and it still won't work).
	 *
	 * In bash and ksh $( executes a command, so we use single quotes
	 * instead of double quotes to do our quoting. If configured command
	 * is double-quoted we change that to single quotes.
	 */
#ifdef	_WINDOWS
	/*
	 * It should be safe to not quote any of the characters from the
	 * string below.  It was quoting with '?' and '&' in a URL, which is
	 * unnecessary.  Also the quoting code below only quotes with
	 * ' (single quote), so we'd want it to at least do double quotes
	 * instead, for Windows.
	 */
	if(toolp)
	  quotable = 0;		/* always never quote */
	else
#endif
	/* quote shell specials */
	if(strpbrk(handle->h.url.path, "&*;<>?[]|~$(){}'\"") != NULL){
	    escape_single_quotes++;
	    if((p = strstr(toolp, "_URL_")) != NULL){  /* explicit arg? */
		int in_quote = 0;

		/* see whether or not it is already quoted */

	        quotable = 1;

		for(q = toolp; q < p; q++)
		  if(*q == '\'' && (q == toolp || q[-1] != '\\'))
		    in_quote = 1 - in_quote;
		
		if(in_quote){
		    for(q = p+5; *q; q++)
		      if(*q == '\'' && q[-1] != '\\'){
			  /* already single quoted, leave it alone */
			  quotable = 0;
			  break;
		      }
		}

		if(quotable){
		    in_quote = 0;
		    for(q = toolp; q < p; q++)
		      if(*q == '\"' && (q == toolp || q[-1] != '\\')){
			  in_quote = 1 - in_quote;
			  if(in_quote)
			    left_double_quote = q;
		      }
		    
		    if(in_quote){
			for(q = p+5; *q; q++)
			  if(*q == '\"' && q[-1] != '\\'){
			      /* we'll replace double quotes with singles */
			      double_quoted = 1;
			      right_double_quote = q;
			      break;
			  }
		    }
		}
	    }
	    else
	      quotable = 1;
	}
	else
	  quotable = 0;

	/* Build the command */
	cmdp = cmd;
	while(cmdp-cmd < URL_MAX_LAUNCH)
	  if((!*toolp && !copied)
	     || (*toolp == '_' && !strncmp(toolp + 1, "URL_", 4))){

	      /* implicit _URL_ at end */
	      if(!*toolp)
		*cmdp++ = ' ';

	      /* add single quotes */
	      if(quotable && !double_quoted && cmdp-cmd < URL_MAX_LAUNCH)
		*cmdp++ = '\'';

	      copied = 1;
	      /*
	       * If the url.path contains a single quote we should escape
	       * that single quote to remove its special meaning.
	       * Since backslash-quote is ignored inside single quotes we
	       * close the quotes, backslash escape the quote, then open
	       * the quotes again. So
	       *         'fred's car'
	       * becomes 'fred'\''s car'
	       */
	      for(p = handle->h.url.path;
		  p && *p && cmdp-cmd < URL_MAX_LAUNCH; p++){
		  if(escape_single_quotes && *p == '\''){
		      *cmdp++ = '\'';	/* closing quote */
		      *cmdp++ = '\\';
		      *cmdp++ = '\'';	/* opening quote comes from p below */
		  }

		  *cmdp++ = *p;
	      }

	      if(quotable && !double_quoted && cmdp-cmd < URL_MAX_LAUNCH){
		  *cmdp++ = '\'';
		  *cmdp = '\0';
	      }

	      if(*toolp)
		toolp += 5;		/* length of "_URL_" */
	  }
	  else{
	      /* replace double quotes with single quotes */
	      if(double_quoted &&
		 (toolp == left_double_quote || toolp == right_double_quote)){
		  *cmdp++ = '\'';
		  toolp++;
	      }
	      else if(!(*cmdp++ = *toolp++))
		break;
	  }


	if(cmdp-cmd >= URL_MAX_LAUNCH)
	  return(url_launch_too_long(rv));
	
	mode = PIPE_RESET | PIPE_USER | PIPE_RUNNOW ;
	if((syspipe = open_system_pipe(cmd, NULL, NULL, mode, 0, pipe_callback, pipe_report_error)) != NULL){
	    close_system_pipe(&syspipe, NULL, pipe_callback);
	    q_status_message(SM_ORDER, 0, 4, _("VIEWER command completed"));
	}
	else
	  q_status_message1(SM_ORDER, 3, 4,
			    /* TRANSLATORS: Cannot start command : <command name> */
			    _("Cannot start command : %s"), cmd);
    }
    else if((f = url_local_handler(handle->h.url.path)) != NULL){
	if((*f)(handle->h.url.path) > 1)
	  rv = 1;		/* done! */
    }
    else
      q_status_message1(SM_ORDER, 2, 2,
			_("\"URL-Viewer\" not defined: Can't open %s"),
			handle->h.url.path);
    
    return(rv);
}


int
url_launch_too_long(int return_value)
{
    q_status_message(SM_ORDER | SM_DING, 3, 3,
		     "Can't spawn.  Command too long.");
    return(return_value);
}


char *
url_external_handler(HANDLE_S *handle, int specific)
{
    char **l, *test, *cmd, *p, *q, *ep;
    int	   i, specific_match;

    for(l = ps_global->VAR_BROWSER ; l && *l; l++){
	get_pair(*l, &test, &cmd, 0, 1);
	dprint((5, "TEST: \"%s\"  CMD: \"%s\"\n",
		   test ? test : "<NULL>", cmd ? cmd : "<NULL>"));
	removing_quotes(cmd);
	if(valid_filter_command(&cmd)){
	    specific_match = 0;

	    if((p = test) != NULL){
		while(*p && cmd)
		  if(*p == '_'){
		      if(!strncmp(p+1, "TEST(", 5)
			 && (ep = strstr(p+6, ")_"))){
			  *ep = '\0';

			  if(exec_mailcap_test_cmd(p+6) == 0){
			      p = ep + 2;
			  }
			  else{
			      dprint((5,"failed handler TEST\n"));
			      fs_give((void **) &cmd);
			  }
		      }
		      else if(!strncmp(p+1, "SCHEME(", 7)
			      && (ep = strstr(p+8, ")_"))){
			  *ep = '\0';

			  p += 8;
			  do
			    if((q = strchr(p, ',')) != NULL)
			      *q++ = '\0';
			    else
			      q = ep;
			  while(!((i = strlen(p))
				  && ((p[i-1] == ':'
				       && handle->h.url.path[i - 1] == ':')
				      || (p[i-1] != ':'
					  && handle->h.url.path[i] == ':'))
				  && !struncmp(handle->h.url.path, p, i))
				&& *(p = q));

			  if(*p){
			      specific_match = 1;
			      p = ep + 2;
			  }
			  else{
			      dprint((5,"failed handler SCHEME\n"));
			      fs_give((void **) &cmd);
			  }
		      }
		      else{
			  dprint((5, "UNKNOWN underscore test\n"));
			  fs_give((void **) &cmd);
		      }
		  }
		  else if(isspace((unsigned char) *p)){
		      p++;
		  }
		  else{
		      dprint((1,"bogus handler test: \"%s\"",
			     test ? test : "?"));
		      fs_give((void **) &cmd);
		  }

		fs_give((void **) &test);
	    }

	    if(cmd && (!specific || specific_match))
	      return(cmd);
	}

	if(test)
	  fs_give((void **) &test);

	if(cmd)
	  fs_give((void **) &cmd);
    }

    cmd = NULL;

    if(!specific){
	cmd = url_os_specified_browser(handle->h.url.path);
	/*
	 * Last chance, anything handling "text/html" in mailcap...
	 */
	if(!cmd && mailcap_can_display(TYPETEXT, "html", NULL, 0)){
	    MCAP_CMD_S *mc_cmd;

	    mc_cmd = mailcap_build_command(TYPETEXT, "html",
					   NULL, "_URL_", NULL, 0);
	    /* 
	     * right now URL viewing won't return anything requiring
	     * special handling
	     */
	    if(mc_cmd){
		cmd = mc_cmd->command;
		fs_give((void **)&mc_cmd);
	    }
	}
    }

    return(cmd);
}


url_tool_t
url_local_handler(char *s)
{
    int i;
    static struct url_t {
	char       *url;
	short	    len;
	url_tool_t  f;
    } handlers[] = {
	{"mailto:", 7, url_local_mailto},	/* see url_tool_t def's */
	{"imap://", 7, url_local_imap},		/* for explanations */
	{"nntp://", 7, url_local_nntp},
	{"file://", 7, url_local_file},
#ifdef	ENABLE_LDAP
	{"ldap://", 7, url_local_ldap},
#endif
	{"news:", 5, url_local_news},
	{"x-alpine-gripe:", 15, gripe_gripe_to},
	{"x-alpine-help:", 14, url_local_helper},
	{"x-alpine-phone-home:", 20, url_local_phone_home},
	{"x-alpine-config:", 16, url_local_config},
	{"x-alpine-cert:", 14, url_local_certdetails},
	{"#", 1, url_local_fragment},
	{NULL, 0, NULL}
    };

    for(i = 0; handlers[i].url ; i++)
      if(!struncmp(s, handlers[i].url, handlers[i].len))
	return(handlers[i].f);

    return(NULL);
}



/*
 * mailto URL digester ala draft-hoffman-mailto-url-02.txt
 */
int
url_local_mailto(char *url)
{
    return(url_local_mailto_and_atts(url, NULL));
}

int
url_local_mailto_and_atts(char *url, PATMT *attachlist)
{
    ENVELOPE *outgoing;
    BODY     *body = NULL;
    REPLY_S   fake_reply;
    char     *sig, *urlp, *p, *hname, *hvalue;
    int	      rv = 0;
    long      rflags;
    int       was_a_body = 0, impl, template_len = 0;
    char     *fcc = NULL;
    PAT_STATE dummy;
    REDRAFT_POS_S *redraft_pos = NULL;
    ACTION_S  *role = NULL;

    outgoing		 = mail_newenvelope();
    outgoing->message_id = generate_message_id();
    body		 = mail_newbody();
    body->type		 = TYPETEXT;
    if((body->contents.text.data = (void *) so_get(PicoText,NULL,EDIT_ACCESS)) != NULL){
	/*
	 * URL format is:
	 *
	 *	mailtoURL  =  "mailto:" [ to ] [ headers ]
	 *	to         =  #mailbox
	 *	headers    =  "?" header *( "&" header )
	 *	header     =  hname "=" hvalue
	 *	hname      =  *urlc
	 *	hvalue     =  *urlc
	 *
	 * NOTE2: "from" and "bcc" are intentionally excluded from
	 *	  the list of understood "header" fields
	 */
	if((p = strchr(urlp = cpystr(url+7), '?')) != NULL)
	  *p++ = '\0';			/* headers?  Tie off mailbox */

	/* grok mailbox as first "to", then roll thru specific headers */
	if(*urlp)
	  rfc822_parse_adrlist(&outgoing->to,
			       rfc1738_str(urlp),
			       ps_global->maildomain);

	while(p){
	    /* Find next "header" */
	    if((p = strchr(hname = p, '&')) != NULL)
	      *p++ = '\0';		/* tie off "header" */

	    if((hvalue = strchr(hname, '=')) != NULL)
	      *hvalue++ = '\0';		/* tie off hname */

	    if(!hvalue || !strucmp(hname, "subject")){
		char *sub = rfc1738_str(hvalue ? hvalue : hname);

		if(outgoing->subject){
		    int len = strlen(outgoing->subject);

		    fs_resize((void **) &outgoing->subject,
			      (len + strlen(sub) + 2) * sizeof(char));
		    snprintf(outgoing->subject + len, strlen(sub)+2, " %s", sub);
		    outgoing->subject[len + strlen(sub) + 2 - 1] = '\0';
		}
		else
		  outgoing->subject = cpystr(sub);
	    }
	    else if(!strucmp(hname, "to")){
		url_mailto_addr(&outgoing->to, hvalue);
	    }
	    else if(!strucmp(hname, "cc")){
		url_mailto_addr(&outgoing->cc, hvalue);
	    }
	    else if(!strucmp(hname, "bcc")){
		q_status_message(SM_ORDER, 3, 4,
				 "\"Bcc\" header in mailto url ignored");
	    }
	    else if(!strucmp(hname, "from")){
		q_status_message(SM_ORDER, 3, 4,
				 "\"From\" header in mailto url ignored");
	    }
	    else if(!strucmp(hname, "body")){
		char *sub = rfc1738_str(hvalue ? hvalue : "");

		so_puts((STORE_S *)body->contents.text.data, sub);
		so_puts((STORE_S *)body->contents.text.data, NEWLINE);
		was_a_body++;
	    }
	}

	fs_give((void **) &urlp);

	rflags = ROLE_COMPOSE;
	if(nonempty_patterns(rflags, &dummy)){
	    role = set_role_from_msg(ps_global, rflags, -1L, NULL);
	    if(confirm_role(rflags, &role))
	      role = combine_inherited_role(role);
	    else{			/* cancel */
		role = NULL;
		cmd_cancelled("Composition");
		goto outta_here;
	    }
	}

	if(role)
	  q_status_message1(SM_ORDER, 3, 4, "Composing using role \"%s\"",
			    role->nick);

	if(!was_a_body && role && role->template){
	    char *filtered;

	    impl = 1;	/* leave cursor in header if not explicit */
	    filtered = detoken(role, NULL, 0, 0, 0, &redraft_pos, &impl);
	    if(filtered){
		if(*filtered){
		    so_puts((STORE_S *)body->contents.text.data, filtered);
		    if(impl == 1)
		      template_len = strlen(filtered);
		}
		
		fs_give((void **)&filtered);
	    }
	}
	else
	  impl = 1;

	if(!was_a_body && (sig = detoken(role, NULL, 2, 0, 1, &redraft_pos,
					 &impl))){
	    if(impl == 2)
	      redraft_pos->offset += template_len;

	    if(*sig)
	      so_puts((STORE_S *)body->contents.text.data, sig);

	    fs_give((void **)&sig);
	}

	memset((void *)&fake_reply, 0, sizeof(fake_reply));
	fake_reply.pseudo = 1;
	fake_reply.data.pico_flags = (outgoing->subject) ? P_BODY : P_HEADEND;


	if(!(role && role->fcc))
	  fcc = get_fcc_based_on_to(outgoing->to);

	if(attachlist)
	  create_message_body(&body, attachlist, 0);

	pine_send(outgoing, &body, "\"MAILTO\" COMPOSE",
		  role, fcc, &fake_reply, redraft_pos, NULL, NULL, PS_STICKY_TO);
	rv++;
	ps_global->mangled_screen = 1;
    }
    else
      q_status_message(SM_ORDER | SM_DING, 3, 4,
		       _("Can't create space for composer"));

outta_here:
    if(outgoing)
      mail_free_envelope(&outgoing);

    if(body)
      pine_free_body(&body);
    
    if(fcc)
      fs_give((void **)&fcc);
    
    free_redraft_pos(&redraft_pos);
    free_action(&role);

    return(rv);
}


void
url_mailto_addr(struct mail_address **a, char *a_raw)
{
    char *p = cpystr(rfc1738_str(a_raw));

    while(*a)				/* append to address list */
      a = &(*a)->next;

    rfc822_parse_adrlist(a, p, ps_global->maildomain);
    fs_give((void **) &p);
}


/*
 * imap URL digester ala RFC 2192
 */
int
url_local_imap(char *url)
{
    char      *folder, *mailbox = NULL, *errstr = NULL, *search = NULL,
	       newfolder[MAILTMPLEN];
    int	       rv;
    long       i;
    imapuid_t uid = 0L, uid_val = 0L;
    CONTEXT_S *fake_context;
    MESSAGECACHE *mc;

    rv = url_imap_folder(url, &folder, &uid, &uid_val, &search, 0);
    switch(rv & URL_IMAP_MASK){
      case URL_IMAP_IMAILBOXLIST :
/* BUG: deal with lsub tag */
	if((fake_context = new_context(folder, NULL)) != NULL){
	    newfolder[0] = '\0';
	    if(display_folder_list(&fake_context, newfolder,
				   0, folders_for_goto))
	      if(strlen(newfolder) + 1 < MAILTMPLEN)
		mailbox = newfolder;
	}
	else
	  errstr = "Problem building URL's folder list";

	fs_give((void **) &folder);
	free_context(&fake_context);

	if(mailbox){
	    return(1);
	}
	else if(errstr)
	  q_status_message(SM_ORDER|SM_DING, 3, 3, errstr);
	else
	  cmd_cancelled("URL Launch");

	break;

      case URL_IMAP_IMESSAGEPART :
      case URL_IMAP_IMESSAGELIST :
	if(ps_global && ps_global->ttyo){
	    blank_keymenu(ps_global->ttyo->screen_rows - 2, 0);
	    ps_global->mangled_footer = 1;
	}

	rv = do_broach_folder(folder, NULL, NULL, 0L);
	fs_give((void **) &folder);
	switch(rv){
	  case -1 :				/* utter failure */
	    ps_global->next_screen = main_menu_screen;
	    break;

	  case 0 :				/* same folder reopened */
	    ps_global->next_screen = mail_index_screen;
	    break;

	  case 1 :				/* requested folder open */
	    ps_global->next_screen = mail_index_screen;

	    if(uid_val && uid_val != ps_global->mail_stream->uid_validity){
		/* Complain! */
		q_status_message(SM_ORDER|SM_DING, 3, 3,
		     "Warning!  Referenced folder changed since URL recorded");
	    }

	    if(uid){
		/*
		 * Make specified message the currently selected..
		 */
		for(i = 1L; i <= mn_get_total(ps_global->msgmap); i++)
		  if(mail_uid(ps_global->mail_stream, i) == uid){
		      ps_global->next_screen = mail_view_screen;
		      mn_set_cur(ps_global->msgmap, i);
		      break;
		  }

		if(i > mn_get_total(ps_global->msgmap))
		  q_status_message(SM_ORDER, 2, 3,
				   "Couldn't find specified article number");
	    }
	    else if(search){
		/*
		 * Select the specified messages
		 * and present a zoom'd index...
		 */
/* BUG: not dealing with CHARSET yet */

/* ANOTHER BUG: mail_criteria is a compatibility routine for IMAP2BIS
 * so it doesn't know about IMAP4 search criteria, like SENTSINCE.
 * It also doesn't handle literals. */

		pine_mail_search_full(ps_global->mail_stream, NULL,
				      mail_criteria(search),
				      SE_NOPREFETCH | SE_FREE);

		for(i = 1L; i <= mn_get_total(ps_global->msgmap); i++)
		  if(ps_global->mail_stream
		     && i <= ps_global->mail_stream->nmsgs
		     && (mc = mail_elt(ps_global->mail_stream, i))
		     && mc->searched)
		    set_lflag(ps_global->mail_stream,
			      ps_global->msgmap, i, MN_SLCT, 1);

		if((i = any_lflagged(ps_global->msgmap, MN_SLCT)) != 0){

		    q_status_message2(SM_ORDER, 0, 3,
				      "%s message%s selected",
				      long2string(i), plural(i));
		    /* Zoom the index! */
		    zoom_index(ps_global, ps_global->mail_stream,
			       ps_global->msgmap, MN_SLCT);
		}
	    }
	}

	return(1);

      default:
      case URL_IMAP_ERROR :
	break;
    }

    return(0);
}


int
url_local_nntp(char *url)
{
    char folder[2*MAILTMPLEN], *group;
    int  group_len;
    long i, article_num;

	/* no hostport, no url, end of story */
	if((group = strchr(url + 7, '/')) != 0){
	    group++;
	    for(group_len = 0; group[group_len] && group[group_len] != '/';
		group_len++)
	      if(!rfc1738_group(&group[group_len]))
		/* TRANSLATORS: these are errors in news group URLs */
		return(url_bogus(url, _("Invalid newsgroup specified")));
	      
	    if(group_len){
	      snprintf(folder, sizeof(folder), "{%.*s/nntp}#news.%.*s",
		      MIN((group - 1) - (url + 7), MAILTMPLEN-20), url + 7,
		      MIN(group_len, MAILTMPLEN-20), group);
	      folder[sizeof(folder)-1] = '\0';
	    }
	    else
	      return(url_bogus(url, _("No newsgroup specified")));
	}
	else
	  return(url_bogus(url, _("No server specified")));

	if(ps_global && ps_global->ttyo){
	    blank_keymenu(ps_global->ttyo->screen_rows - 2, 0);
	    ps_global->mangled_footer = 1;
	}

	switch(do_broach_folder(rfc1738_str(folder), NULL, NULL, 0L)){
	  case -1 :				/* utter failure */
	    ps_global->next_screen = main_menu_screen;
	    break;

	  case 0 :				/* same folder reopened */
	    ps_global->next_screen = mail_index_screen;
	    break;

	  case 1 :				/* requested folder open */
	    ps_global->next_screen = mail_index_screen;

	    /* grok article number --> c-client UID */
	    if(group[group_len++] == '/'
	       && (article_num = atol(&group[group_len]))){
		/*
		 * Make the requested article our current message
		 */
		for(i = 1; i <= mn_get_nmsgs(ps_global->msgmap); i++)
		  if(mail_uid(ps_global->mail_stream, i) == article_num){
		      ps_global->next_screen = mail_view_screen;
		      if((i = mn_raw2m(ps_global->msgmap, i)) != 0)
			mn_set_cur(ps_global->msgmap, i);
		      break;
		  }

		if(i == 0 || i > mn_get_total(ps_global->msgmap))
		  q_status_message(SM_ORDER, 2, 3,
				   _("Couldn't find specified article number"));
	    }

	    break;
	}

	ps_global->redrawer = (void(*)(void))NULL;
	return(1);
}


int
url_local_news(char *url)
{
    char       folder[MAILTMPLEN], *p;
    CONTEXT_S *cntxt = NULL;

    /*
     * NOTE: NO SUPPORT for '*' or message-id
     */
    if(*(url+5)){
	if(*(url+5) == '/' && *(url+6) == '/')
	  return(url_local_nntp(url)); /* really meant "nntp://"? */

	if(ps_global->VAR_NNTP_SERVER && ps_global->VAR_NNTP_SERVER[0])
	  /*
	   * BUG: Only the first NNTP server is tried.
	   */
	  snprintf(folder, sizeof(folder), "{%s/nntp}#news.", ps_global->VAR_NNTP_SERVER[0]);
	else
	  strncpy(folder, "#news.", sizeof(folder));

	folder[sizeof(folder)-1] = '\0';

	for(p = strncpy(folder + strlen(folder), url + 5, sizeof(folder)-strlen(folder)-1); 
	    *p && rfc1738_group(p);
	    p++)
	  ;

	if(*p)
	  return(url_bogus(url, "Invalid newsgroup specified"));
    }
    else{			/* fish first group from newsrc */
	FOLDER_S  *f;
	int	   alphaorder;

	folder[0] = '\0';

	/* Find first news context */
	for(cntxt = ps_global->context_list;
	    cntxt && !(cntxt->use & CNTXT_NEWS);
	    cntxt = cntxt->next)
	  ;

	if(cntxt){
	    if((alphaorder = F_OFF(F_READ_IN_NEWSRC_ORDER, ps_global)) != 0)
	      (void) F_SET(F_READ_IN_NEWSRC_ORDER, ps_global, 1);

	    build_folder_list(NULL, cntxt, NULL, NULL, BFL_LSUB);
	    if((f = folder_entry(0, FOLDERS(cntxt))) != NULL){
		strncpy(folder, f->name, sizeof(folder));
		folder[sizeof(folder)-1] = '\0';
	    }

	    free_folder_list(cntxt);

	    if(alphaorder)
	      (void) F_SET(F_READ_IN_NEWSRC_ORDER, ps_global, 0);
	}

	if(folder[0] == '\0'){
	    q_status_message(SM_ORDER | SM_DING, 3, 3,
			     "No default newsgroup");
	    return(0);
	}
    }

    if(ps_global && ps_global->ttyo){
	blank_keymenu(ps_global->ttyo->screen_rows - 2, 0);
	ps_global->mangled_footer = 1;
    }

    if(do_broach_folder(rfc1738_str(folder), cntxt, NULL, 0L) < 0)
      ps_global->next_screen = main_menu_screen;
    else
      ps_global->next_screen = mail_index_screen;

    ps_global->redrawer = (void(*)(void))NULL;

    return(1);
}


int
url_local_file(char *file_url)
{
    if(want_to(
 /* TRANSLATORS: this is a warning that the file URL can cause programs to run which may
    be a security problem. We are asking the user to confirm that they want to do this. */
 _("\"file\" URL may cause programs to be run on your system. Run anyway"),
               'n', 0, NO_HELP, WT_NORM) == 'y'){
	HANDLE_S handle;

	/* fake a handle */
	handle.h.url.path = file_url;
	if((handle.h.url.tool = url_external_handler(&handle, 1))
	   || (handle.h.url.tool = url_external_handler(&handle, 0))){
	    url_launch(&handle);
	    return 1;
	}
	else{
	    q_status_message(SM_ORDER, 0, 4,
		  _("No viewer for \"file\" URL.  VIEWER command cancelled"));
	    return 0;
	}
    }
    q_status_message(SM_ORDER, 0, 4, _("VIEWER command cancelled"));
    return 0;
}


int
url_local_fragment(char *fragment)
{
    SCRLCTRL_S *st = scroll_state(SS_CUR);
    HANDLE_S   *hp;

    /*
     * find a handle with the fragment's name
     */
    for(hp = st->parms->text.handles; hp; hp = hp->next)
      if(hp->type == URL && hp->h.url.name
	 && !strcmp(hp->h.url.name, fragment + 1))
	break;

    if(!hp)
      for(hp = st->parms->text.handles->prev; hp; hp = hp->prev)
	if(hp->type == URL && hp->h.url.name
	   && !strcmp(hp->h.url.name, fragment + 1))
	  break;

    /*
     * set the top line of the display to contain this line
     */
    if(hp && hp->loc){
	st->top_text_line = hp->loc->where.row;
	ps_global->mangled_body = 1;
    }
    else
      q_status_message1(SM_ORDER | SM_DING, 0, 3,
			"Can't find fragment: %s", fragment);

    return(1);
}


int
url_local_phone_home(char *url)
{
    phone_home(url + strlen("x-alpine-phone-home:"));
    return(2);
}


/*
 * Format editorial comment referencing screen offering
 * List-* header supplied commands
 */
int
rfc2369_editorial(long int msgno, HANDLE_S **handlesp, int flags, int width, gf_io_t pc)
{
    char     *p, *hdrp, *hdrs[MLCMD_COUNT + 1],
	      color[64], buf[2048];
    int	      i, n, rv = TRUE;
    HANDLE_S *h = NULL;

    if((flags & FM_DISPLAY)
       && (hdrp = pine_fetchheader_lines(ps_global->mail_stream, msgno,
					 NULL, rfc2369_hdrs(hdrs)))){
	if(*hdrp){
	    snprintf(buf, sizeof(buf), "Note: This message contains ");
	    buf[sizeof(buf)-1] = '\0';
	    p = buf + strlen(buf);

	    if(handlesp){
		h		      = new_handle(handlesp);
		h->type		      = Function;
		h->h.func.f	      = rfc2369_display;
		h->h.func.args.stream = ps_global->mail_stream;
		h->h.func.args.msgmap = ps_global->msgmap;
		h->h.func.args.msgno  = msgno;

		if(!(flags & FM_NOCOLOR)
		   && handle_start_color(color, sizeof(color), &n, 0)){
		    if((p-buf)+n < sizeof(buf))
		      for(i = 0; i < n; i++)
		        *p++ = color[i];
		}
		else if(F_OFF(F_SLCTBL_ITEM_NOBOLD, ps_global)){
		    *p++ = TAG_EMBED;
		    *p++ = TAG_BOLDON;
		}

		if((p-buf)+2 < sizeof(buf)){
		    *p++ = TAG_EMBED;
		    *p++ = TAG_HANDLE;
		}

		snprintf(p + 1, sizeof(buf)-(p+1-buf), "%d", h->key);
		buf[sizeof(buf)-1] = '\0';
		*p  = strlen(p + 1);
		p  += (*p + 1);
	    }

	    sstrncpy(&p, "email list management information", sizeof(buf)-(p-buf));
	    buf[sizeof(buf)-1] = '\0';

	    if(h){
		/* in case it was the current selection */
		if((p-buf)+2 < sizeof(buf)){
		    *p++ = TAG_EMBED;
		    *p++ = TAG_INVOFF;
		}

		if(handle_end_color(color, sizeof(color), &n)){
		    if((p-buf)+n < sizeof(buf))
		      for(i = 0; i < n; i++)
		        *p++ = color[i];
		}
		else{
		    if((p-buf)+2 < sizeof(buf)){
			*p++ = TAG_EMBED;
			*p++ = TAG_BOLDOFF;
		    }
		}

		if(p-buf < sizeof(buf))
		  *p = '\0';
	    }

	    buf[sizeof(buf)-1] = '\0';

	    rv = (gf_puts(NEWLINE, pc)
		  && format_editorial(buf, width, flags, handlesp, pc) == NULL
		  && gf_puts(NEWLINE, pc));
	}
       
	fs_give((void **) &hdrp);
    }

    return(rv);
}



/*----------------------------------------------------------------------
   routine for displaying text on the screen.

  Args: sparms -- structure of args controlling what happens outside
		  just the business of managing text scrolling
 
   This displays in three different kinds of text. One is an array of
lines passed in in text_array. The other is a simple long string of
characters passed in in text.

  The style determines what some of the error messages will be, and
what commands are available as different things are appropriate for
help text than for message text etc.

 ---*/

int
scrolltool(SCROLL_S *sparms)
{
    register long    cur_top_line,  num_display_lines;
    UCS              ch;
    int              result, done, cmd, found_on, found_on_index,
		     first_view, force, scroll_lines, km_size,
		     cursor_row, cursor_col, km_popped;
    char            *utf8str;
    long             jn;
    struct key_menu *km;
    HANDLE_S	    *next_handle;
    bitmap_t         bitmap;
    OtherMenu        what;
    Pos		     whereis_pos;

    num_display_lines	      = SCROLL_LINES(ps_global);
    km_popped		      = 0;
    ps_global->mangled_header = 1;
    ps_global->mangled_footer = 1;
    ps_global->mangled_body   = !sparms->body_valid;
      

    what	    = sparms->keys.what;	/* which key menu to display */
    cur_top_line    = 0;
    done	    = 0;
    found_on	    = -1;
    found_on_index  = -1;
    first_view	    = 1;
    if(sparms->quell_first_view)
      first_view    = 0;

    force	    = 0;
    ch		    = 'x';			/* for first time through */
    whereis_pos.row = 0;
    whereis_pos.col = 0;
    next_handle	    = sparms->text.handles;

    set_scroll_text(sparms, cur_top_line, scroll_state(SS_NEW));
    format_scroll_text();

    if((km = sparms->keys.menu) != NULL){
	memcpy(bitmap, sparms->keys.bitmap, sizeof(bitmap_t));
    }
    else{
	setbitmap(bitmap);
	km = &simple_text_keymenu;
#ifdef	_WINDOWS
	sparms->mouse.popup = simple_text_popup;
#endif
    }

    if(!sparms->bar.title)
      sparms->bar.title = "Text";

    if(sparms->bar.style == TitleBarNone){
	if(THREADING() && sp_viewing_a_thread(ps_global->mail_stream))
	  sparms->bar.style = ThrdMsgPercent;
	else
	  sparms->bar.style = MsgTextPercent;
    }

    switch(sparms->start.on){
      case LastPage :
	cur_top_line = MAX(0, scroll_text_lines() - (num_display_lines-2));
	if(F_ON(F_SHOW_CURSOR, ps_global)){
	    whereis_pos.row = scroll_text_lines() - cur_top_line;
	    found_on	    = scroll_text_lines() - 1;
	}

	break;

      case Fragment :
	if(sparms->start.loc.frag){
	    (void) url_local_fragment(sparms->start.loc.frag);

	    cur_top_line = scroll_state(SS_CUR)->top_text_line;

	    if(F_ON(F_SHOW_CURSOR, ps_global)){
		whereis_pos.row = scroll_text_lines() - cur_top_line;
		found_on	= scroll_text_lines() - 1;
	    }
	}

	break;

      case Offset :
	if(sparms->start.loc.offset){
	    for(cur_top_line = 0L;
		cur_top_line + 1 < scroll_text_lines()
		  && (sparms->start.loc.offset 
				      -= scroll_handle_column(cur_top_line,
								    -1)) >= 0;
		cur_top_line++)
	      ;
	}

	break;

      case Handle :
	if(scroll_handle_obscured(sparms->text.handles))
	  cur_top_line = scroll_handle_reframe(-1, TRUE);

	break;

      default :			/* no-op */
	break;
    }

    /* prepare for calls below to tell us where to go */
    ps_global->next_screen = SCREEN_FUN_NULL;

    cancel_busy_cue(-1);

    while(!done) {
	ps_global->user_says_cancel = 0;
	if(km_popped){
	    km_popped--;
	    if(km_popped == 0){
		clearfooter(ps_global);
		ps_global->mangled_body = 1;
	    }
	}

	if(ps_global->mangled_screen) {
	    ps_global->mangled_header = 1;
	    ps_global->mangled_footer = 1;
            ps_global->mangled_body   = 1;
	}

        if(!sparms->quell_newmail && streams_died())
          ps_global->mangled_header = 1;

        dprint((9, "@@@@ current:%ld\n",
		   mn_get_cur(ps_global->msgmap)));


        /*==================== All Screen painting ====================*/
        /*-------------- The title bar ---------------*/
	update_scroll_titlebar(cur_top_line, ps_global->mangled_header);

	if(ps_global->mangled_screen){
	    /* this is the only line not cleared by header, body or footer
	     * repaint calls....
	     */
	    ClearLine(1);
            ps_global->mangled_screen = 0;
	}

        /*---- Scroll or update the body of the text on the screen -------*/
        cur_top_line		= scroll_scroll_text(cur_top_line, next_handle,
						     ps_global->mangled_body);
	ps_global->redrawer	= redraw_scroll_text;
        ps_global->mangled_body = 0;

	/*--- Check to see if keymenu might change based on next_handle --*/
	if(sparms->text.handles != next_handle)
	  ps_global->mangled_footer = 1;
	
	if(next_handle)
	  sparms->text.handles = next_handle;

        /*------------- The key menu footer --------------------*/
	if(ps_global->mangled_footer || sparms->keys.each_cmd){
	    if(km_popped){
		FOOTER_ROWS(ps_global) = 3;
		clearfooter(ps_global);
	    }

	    if(F_ON(F_ARROW_NAV, ps_global)){
		menu_clear_binding(km, KEY_LEFT);
		if((cmd = menu_clear_binding(km, '<')) != MC_UNKNOWN){
		    menu_add_binding(km, '<', cmd);
		    menu_add_binding(km, KEY_LEFT, cmd);
		}
	    }

	    if(F_ON(F_ARROW_NAV, ps_global)){
		menu_clear_binding(km, KEY_RIGHT);
		if((cmd = menu_clear_binding(km, '>')) != MC_UNKNOWN){
		    menu_add_binding(km, '>', cmd);
		    menu_add_binding(km, KEY_RIGHT, cmd);
		}
	    }

	    if(sparms->keys.each_cmd){
		(*sparms->keys.each_cmd)(sparms,
				 scroll_handle_obscured(sparms->text.handles));
		memcpy(bitmap, sparms->keys.bitmap, sizeof(bitmap_t));
	    }

	    if(menu_binding_index(km, MC_JUMP) >= 0){
	      for(cmd = 0; cmd < 10; cmd++)
		if(F_ON(F_ENABLE_JUMP, ps_global))
		  (void) menu_add_binding(km, '0' + cmd, MC_JUMP);
		else
		  (void) menu_clear_binding(km, '0' + cmd);
	    }

            draw_keymenu(km, bitmap, ps_global->ttyo->screen_cols,
			 1-FOOTER_ROWS(ps_global), 0, what);
	    what = SameMenu;
	    ps_global->mangled_footer = 0;
	    if(km_popped){
		FOOTER_ROWS(ps_global) = 1;
		mark_keymenu_dirty();
	    }
	}

	if((ps_global->first_time_user || ps_global->show_new_version)
	   && first_view && sparms->text.handles
	   && (sparms->text.handles->next || sparms->text.handles->prev)
	   && !sparms->quell_help)
	  q_status_message(SM_ORDER, 0, 3, HANDLE_INIT_MSG);

	/*============ Check for New Mail and CheckPoint ============*/
        if(!sparms->quell_newmail &&
	   new_mail(force, NM_TIMING(ch), NM_STATUS_MSG) >= 0){
	    update_scroll_titlebar(cur_top_line, 1);
	    if(ps_global->mangled_footer)
              draw_keymenu(km, bitmap, ps_global->ttyo->screen_cols,
			   1-FOOTER_ROWS(ps_global), 0, what);

	    ps_global->mangled_footer = 0;
	}

	/*
	 * If an expunge of the current message happened during the
	 * new mail check we want to bail out of here. See mm_expunged.
	 */
	if(ps_global->next_screen != SCREEN_FUN_NULL){
	    done = 1;
	    continue;
	}

	if(ps_global->noticed_change_in_unseen){
	    cmd = MC_RESIZE;	/* causes cursor to be saved in folder_lister */
	    done = 1;
	    continue;
	}

	if(first_view && num_display_lines >= scroll_text_lines())
	  q_status_message1(SM_INFO, 0, 1, "ALL of %s", STYLE_NAME(sparms));
			    

	force      = 0;		/* may not need to next time around */
	first_view = 0;		/* check_point a priority any more? */

	/*==================== Output the status message ==============*/
	if(!sparms->no_stat_msg){
	    if(km_popped){
		FOOTER_ROWS(ps_global) = 3;
		mark_status_unknown();
	    }

	    display_message(ch);
	    if(km_popped){
		FOOTER_ROWS(ps_global) = 1;
		mark_status_unknown();
	    }
	}

	if(F_ON(F_SHOW_CURSOR, ps_global)){
#ifdef	WINDOWS
	    if(cur_top_line != scroll_state(SS_CUR)->top_text_line)
	      whereis_pos.row = 0;
#endif
	
	    if(whereis_pos.row > 0){
		cursor_row  = SCROLL_LINES_ABOVE(ps_global)
				+ whereis_pos.row - 1;
		cursor_col  = whereis_pos.col;
	    }
	    else{
		POSLIST_S  *lp = NULL;

		if(sparms->text.handles &&
		   !scroll_handle_obscured(sparms->text.handles)){
		    SCRLCTRL_S *st = scroll_state(SS_CUR);

		    for(lp = sparms->text.handles->loc; lp; lp = lp->next)
		      if(lp->where.row >= st->top_text_line
			 && lp->where.row < st->top_text_line
							  + st->screen.length){
			  cursor_row = lp->where.row - cur_top_line
					      + SCROLL_LINES_ABOVE(ps_global);
			  cursor_col = lp->where.col;
			  break;
		      }
		}

		if(!lp){
		    cursor_col = 0;
		    /* first new line of text */
		    cursor_row  = SCROLL_LINES_ABOVE(ps_global) +
			((cur_top_line == 0) ? 0 : ps_global->viewer_overlap);
		}
	    }
	}
	else{
	    cursor_col = 0;
	    cursor_row = ps_global->ttyo->screen_rows
			    - SCROLL_LINES_BELOW(ps_global);
	}

	MoveCursor(cursor_row, cursor_col);

	/*================ Get command and validate =====================*/
#ifdef	MOUSE
#ifndef	WIN32
	if(sparms->text.handles)
#endif
	{
	    mouse_in_content(KEY_MOUSE, -1, -1, 0x5, 0);
	    register_mfunc(mouse_in_content, HEADER_ROWS(ps_global), 0,
			   ps_global->ttyo->screen_rows
						- (FOOTER_ROWS(ps_global) + 1),
			   ps_global->ttyo->screen_cols);
	}
#endif
#ifdef	_WINDOWS
	mswin_allowcopy(mswin_readscrollbuf);
	mswin_setscrollcallback(pcpine_do_scroll);

	if(sparms->help.text != NO_HELP)
	  mswin_sethelptextcallback(pcpine_help_scroll);

	if(sparms->text.handles
	   && sparms->text.handles->type != Folder)
	  mswin_mousetrackcallback(pcpine_view_cursor);

	if(ps_global->prev_screen == mail_view_screen)
	  mswin_setviewinwindcallback(view_in_new_window);
#endif
	ch = (sparms->quell_newmail || read_command_prep()) ? read_command(&utf8str) : NO_OP_COMMAND;
#ifdef	MOUSE
#ifndef	WIN32
	if(sparms->text.handles)
#endif
	  clear_mfunc(mouse_in_content);
#endif
#ifdef	_WINDOWS
	mswin_allowcopy(NULL);
	mswin_setscrollcallback(NULL);
	mswin_sethelptextcallback(NULL);
	mswin_mousetrackcallback(NULL);
	mswin_setviewinwindcallback(NULL);
	cur_top_line = scroll_state(SS_CUR)->top_text_line;
#endif

	cmd = menu_command(ch, km);

	if(km_popped)
	  switch(cmd){
	    case MC_NONE :
	    case MC_OTHER :
	    case MC_RESIZE:
	    case MC_REPAINT :
	      km_popped++;
	      break;
	    
	    default:
	      clearfooter(ps_global);
	      break;
	  }


	/*============= Execute command =======================*/
	switch(cmd){

            /* ------ Help -------*/
	  case MC_HELP :
	    if(FOOTER_ROWS(ps_global) == 1 && km_popped == 0){
		km_popped = 2;
		ps_global->mangled_footer = 1;
		break;
	    }

	    whereis_pos.row = 0;
            if(sparms->help.text == NO_HELP){
                q_status_message(SM_ORDER, 0, 5,
				 _("No help text currently available"));
                break;
            }

	    km_size = FOOTER_ROWS(ps_global);

	    helper(sparms->help.text, sparms->help.title, 0);

	    if(ps_global->next_screen != main_menu_screen
	       && km_size == FOOTER_ROWS(ps_global)) {
		/* Have to reset because helper uses scroll_text */
		num_display_lines	  = SCROLL_LINES(ps_global);
		ps_global->mangled_screen = 1;
	    }
	    else
	      done = 1;

            break; 


            /*---------- Roll keymenu ------*/
	  case MC_OTHER :
	    if(F_OFF(F_USE_FK, ps_global))
	      warn_other_cmds();

	    what = NextMenu;
	    ps_global->mangled_footer = 1;
	    break;
            

            /* -------- Scroll back one page -----------*/
	  case MC_PAGEUP :
	    whereis_pos.row = 0;
	    if(cur_top_line) {
		scroll_lines = MIN(MAX(num_display_lines -
			ps_global->viewer_overlap, 1), num_display_lines);
		cur_top_line -= scroll_lines;
		if(cur_top_line <= 0){
		    cur_top_line = 0;
		    q_status_message1(SM_INFO, 0, 1, "START of %s",
				      STYLE_NAME(sparms));
		}
	    }
	    else{
		/* hilite last available handle */
		next_handle = NULL;
		if(sparms->text.handles){
		    HANDLE_S *h = sparms->text.handles;

		    while((h = scroll_handle_prev_sel(h))
			  && !scroll_handle_obscured(h))
		      next_handle = h;
		}

		if(!next_handle)
		  q_status_message1(SM_ORDER, 0, 1, _("Already at start of %s"),
				    STYLE_NAME(sparms));

	    }


            break;


            /*---- Scroll down one page -------*/
	  case MC_PAGEDN :
	    if(cur_top_line + num_display_lines < scroll_text_lines()){
		whereis_pos.row = 0;
		scroll_lines = MIN(MAX(num_display_lines -
			ps_global->viewer_overlap, 1), num_display_lines);
		cur_top_line += scroll_lines;

		if(cur_top_line + num_display_lines >= scroll_text_lines())
		  q_status_message1(SM_INFO, 0, 1, "END of %s",
				    STYLE_NAME(sparms));
            }
	    else if(!sparms->end_scroll
		    || !(done = (*sparms->end_scroll)(sparms))){
		q_status_message1(SM_ORDER, 0, 1, _("Already at end of %s"),
				  STYLE_NAME(sparms));
		/* hilite last available handle */
		if(sparms->text.handles){
		    HANDLE_S *h = sparms->text.handles;

		    while((h = scroll_handle_next_sel(h)) != NULL)
		      next_handle = h;
		}
	    }

            break;

	    /* scroll to the top page */
	  case MC_HOMEKEY:
	    if(cur_top_line){
		cur_top_line = 0;
		q_status_message1(SM_INFO, 0, 1, "START of %s",
				  STYLE_NAME(sparms));
	    }

	    next_handle = NULL;
	    if(sparms->text.handles){
		HANDLE_S *h = sparms->text.handles;

		while((h = scroll_handle_prev_sel(h)) != NULL)
		  next_handle = h;
	    }
	    break;

	    /* scroll to the bottom page */
	  case MC_ENDKEY:
	    if(cur_top_line + num_display_lines < scroll_text_lines()){
		cur_top_line = scroll_text_lines() - MIN(5, num_display_lines);
		q_status_message1(SM_INFO, 0, 1, "END of %s",
				  STYLE_NAME(sparms));
	    }

	    if(sparms->text.handles){
		HANDLE_S *h = sparms->text.handles;

		while((h = scroll_handle_next_sel(h)) != NULL)
		  next_handle = h;
	    }
	    break;

            /*------ Scroll down one line -----*/
	  case MC_CHARDOWN :
	    next_handle = NULL;
	    if(sparms->text.handles){
		if(sparms->vert_handle){
		    HANDLE_S *h, *h2;
		    int	      i, j, k;

		    h2 = sparms->text.handles;
		    if(h2->type == Folder && h2->prev && h2->prev->is_dual_do_open)
		      h2 = h2->prev;

		    i = h2->loc->where.row + 1;
		    j = h2->loc->where.col;
		    for(h = NULL, k = h2->key;
			h2 && (!h
			       || (h->loc->where.row == h2->loc->where.row));
			h2 = h2->next)
		      /* must be different key */
		      /* ... below current line */
		      /* ... pref'bly to left */
		      if(h2->key != k
			 && h2->loc->where.row >= i){
			  if(h2->loc->where.col > j){
			      if(!h)
				h = h2;

			      break;
			  }
			  else
			    h = h2;
		      }

		    if(h){
			whereis_pos.row = 0;
			next_handle = h;
			if((result = scroll_handle_obscured(next_handle)) != 0){
			    long new_top;

			    if(scroll_handle_obscured(sparms->text.handles)
			       && result > 0)
			      next_handle = sparms->text.handles;

			    ps_global->mangled_body++;
			    new_top = scroll_handle_reframe(next_handle->key,0);
			    if(new_top >= 0)
			      cur_top_line = new_top;
			}
		    }
		}
		else if(!(ch == ctrl('N') || F_ON(F_FORCE_ARROWS, ps_global)))
		  next_handle = scroll_handle_next(sparms->text.handles);
	    }

	    if(!next_handle){
		if(cur_top_line + num_display_lines < scroll_text_lines()){
		    whereis_pos.row = 0;
		    cur_top_line++;
		    if(cur_top_line + num_display_lines >= scroll_text_lines())
		      q_status_message1(SM_INFO, 0, 1, "END of %s",
					STYLE_NAME(sparms));
		}
		else
		  q_status_message1(SM_ORDER, 0, 1, _("Already at end of %s"),
				    STYLE_NAME(sparms));
	    }

	    break;


            /* ------ Scroll back up one line -------*/
	  case MC_CHARUP :
	    next_handle = NULL;
	    if(sparms->text.handles){
		if(sparms->vert_handle){
		    HANDLE_S *h, *h2;
		    int	  i, j, k;

		    h2 = sparms->text.handles;
		    if(h2->type == Folder && h2->prev && h2->prev->is_dual_do_open)
		      h2 = h2->prev;

		    i = h2->loc->where.row - 1;
		    j = h2->loc->where.col;

		    for(h = NULL, k = h2->key;
			h2 && (!h
			       || (h->loc->where.row == h2->loc->where.row));
			h2 = h2->prev)
		      /* must be new key, above current
		       * line and pref'bly to right
		       */
		      if(h2->key != k
			 && h2->loc->where.row <= i){
			  if(h2->loc->where.col < j){
			      if(!h)
				h = h2;

			      break;
			  }
			  else
			    h = h2;
		      }

		    if(h){
			whereis_pos.row = 0;
			next_handle = h;
			if((result = scroll_handle_obscured(next_handle)) != 0){
			    long new_top;

			    if(scroll_handle_obscured(sparms->text.handles)
			       && result < 0)
			      next_handle = sparms->text.handles;

			    ps_global->mangled_body++;
			    new_top = scroll_handle_reframe(next_handle->key,0);
			    if(new_top >= 0)
			      cur_top_line = new_top;
			}
		    }
		}
		else if(!(ch == ctrl('P') || F_ON(F_FORCE_ARROWS, ps_global)))
		  next_handle = scroll_handle_prev(sparms->text.handles);
	    }

	    if(!next_handle){
		whereis_pos.row = 0;
		if(cur_top_line){
		    cur_top_line--;
		    if(cur_top_line == 0)
		      q_status_message1(SM_INFO, 0, 1, "START of %s",
					STYLE_NAME(sparms));
		}
		else
		  q_status_message1(SM_ORDER, 0, 1,
				    _("Already at start of %s"),
				    STYLE_NAME(sparms));
	    }

	    break;


	  case MC_NEXT_HANDLE :
	    if((next_handle = scroll_handle_next_sel(sparms->text.handles)) != NULL){
		whereis_pos.row = 0;
		if((result = scroll_handle_obscured(next_handle)) != 0){
		    long new_top;

		    if(scroll_handle_obscured(sparms->text.handles)
		       && result > 0)
		      next_handle = sparms->text.handles;

		    ps_global->mangled_body++;
		    new_top = scroll_handle_reframe(next_handle->key, 0);
		    if(new_top >= 0)
		      cur_top_line = new_top;
		}
	    }
	    else{
		if(scroll_handle_obscured(sparms->text.handles)){
		    long new_top;

		    ps_global->mangled_body++;
		    if((new_top = scroll_handle_reframe(-1, 0)) >= 0){
			whereis_pos.row = 0;
			cur_top_line = new_top;
		    }
		}

		q_status_message1(SM_ORDER, 0, 1,
				  _("Already on last item in %s"),
				  STYLE_NAME(sparms));
	    }

	    break;


	  case MC_PREV_HANDLE :
	    if((next_handle = scroll_handle_prev_sel(sparms->text.handles)) != NULL){
		whereis_pos.row = 0;
		if((result = scroll_handle_obscured(next_handle)) != 0){
		    long new_top;

		    if(scroll_handle_obscured(sparms->text.handles)
		       && result < 0)
		      next_handle = sparms->text.handles;

		    ps_global->mangled_body++;
		    new_top = scroll_handle_reframe(next_handle->key, 0);
		    if(new_top >= 0)
		      cur_top_line = new_top;
		}
	    }
	    else{
		if(scroll_handle_obscured(sparms->text.handles)){
		    long new_top;

		    ps_global->mangled_body++;
		    if((new_top = scroll_handle_reframe(-1, 0)) >= 0){
			whereis_pos.row = 0;
			cur_top_line = new_top;
		    }
		}

		q_status_message1(SM_ORDER, 0, 1,
				  _("Already on first item in %s"),
				  STYLE_NAME(sparms));
	    }

	    break;


	    /*------  View the current handle ------*/
	  case MC_VIEW_HANDLE :
	    switch(scroll_handle_obscured(sparms->text.handles)){
	      default :
	      case 0 :
		switch(scroll_handle_launch(sparms->text.handles,
					sparms->text.handles->force_display)){
		  case 1 :
		    cmd = MC_EXIT;		/* propagate */
		    done = 1;
		    break;

		  case -1 :
		    cmd_cancelled(NULL);
		    break;

		  default :
		    break;
		}

		cur_top_line = scroll_state(SS_CUR)->top_text_line;
		break;

	      case 1 :
		q_status_message(SM_ORDER, 0, 2, HANDLE_BELOW_ERR);
		break;

	      case -1 :
		q_status_message(SM_ORDER, 0, 2, HANDLE_ABOVE_ERR);
		break;
	    }

	    break;

            /*---------- Search text (where is) ----------*/
	  case MC_WHEREIS :
            ps_global->mangled_footer = 1;
	    {long start_row;
	     int  start_index, key = 0;
	     char *report = NULL;

	     start_row = cur_top_line;
	     start_index = 0;

	     if(F_ON(F_SHOW_CURSOR,ps_global)){
		 if(found_on < 0
		    || found_on >= scroll_text_lines()
		    || found_on < cur_top_line
		    || found_on >= cur_top_line + num_display_lines){
		     start_row = cur_top_line;
		     start_index = 0;
		 }
		 else{
		     if(found_on_index < 0){
			 start_row = found_on + 1;
			 start_index = 0;
		     }
		     else{
			 start_row = found_on;
			 start_index = found_on_index+1;
		     }
		 }
	     }
	     else if(sparms->srch_handle){
		 HANDLE_S   *h;

		 if((h = scroll_handle_next_sel(sparms->text.handles)) != NULL){
		     /*
		      * Translate the screen's column into the
		      * line offset to start on...
		      *
		      * This makes it so search_text never returns -3
		      * so we don't know it is the same match. That's
		      * because we start well after the current handle
		      * (at the next handle) and that causes us to
		      * think the one we just matched on is a different
		      * one from before. Can't think of an easy way to
		      * fix it, though, and it isn't a big deal. We still
		      * match, we just don't say current line contains
		      * the only match.
		      */
		     start_row = h->loc->where.row;
		     start_index = scroll_handle_index(start_row, h->loc->where.col);
		 }
		 else{
		     /* last handle, start over at top */
		     start_row = cur_top_line;
		     start_index = 0;
		 }
	     }
	     else{
		 start_row = (found_on < 0
			      || found_on >= scroll_text_lines()
			      || found_on < cur_top_line
			      || found_on >= cur_top_line + num_display_lines)
				? cur_top_line : found_on + 1,
		 start_index = 0;
	     }

             found_on = search_text(-FOOTER_ROWS(ps_global), start_row,
				     start_index, &report,
				     &whereis_pos, &found_on_index);

	     if(found_on == -4){	/* search to top of text */
		 whereis_pos.row = 0;
		 whereis_pos.col = 0;
		 found_on	 = 0;
		 if(sparms->text.handles && sparms->srch_handle)
		   key = 1;
	     }
	     else if(found_on == -5){	/* search to bottom of text */
		 HANDLE_S *h;

		 whereis_pos.row = MAX(scroll_text_lines() - 1, 0);
		 whereis_pos.col = 0;
		 found_on	 = whereis_pos.row;
		 if((h = sparms->text.handles) && sparms->srch_handle)
		   do
		     key = h->key;
		   while((h = h->next) != NULL);
	     }
	     else if(found_on == -3){
		 whereis_pos.row = found_on = start_row;
		 found_on_index = start_index - 1;
		 q_status_message(SM_ORDER, 1, 3,
				  _("Current line contains the only match"));
	     }

	     if(found_on >= 0){
		result = found_on < cur_top_line;
		if(!key)
		  key = (sparms->text.handles)
			  ? dot_on_handle(found_on, whereis_pos.col) : 0;

		if(F_ON(F_FORCE_LOW_SPEED,ps_global)
		   || ps_global->low_speed
		   || F_ON(F_SHOW_CURSOR,ps_global)
		   || key){
		    if((found_on >= cur_top_line + num_display_lines ||
		       found_on < cur_top_line) &&
		       num_display_lines > ps_global->viewer_overlap){
			cur_top_line = found_on - ((found_on > 0) ? 1 : 0);
			if(scroll_text_lines()-cur_top_line < 5)
			  cur_top_line = MAX(0,
			      scroll_text_lines()-MIN(5,num_display_lines));
		    }
		    /* else leave cur_top_line alone */
		}
		else{
		    cur_top_line = found_on - ((found_on > 0) ? 1 : 0);
		    if(scroll_text_lines()-cur_top_line < 5)
		      cur_top_line = MAX(0,
			  scroll_text_lines()-MIN(5,num_display_lines));
		}

		whereis_pos.row = whereis_pos.row - cur_top_line + 1;
		if(report)
		  q_status_message(SM_ORDER, 0, 3, report);
		else
		  q_status_message2(SM_ORDER, 0, 3,
				    "%sFound on line %s on screen",
				    result ? "Search wrapped to start. " : "",
				    int2string(whereis_pos.row));

		if(key){
		    if(sparms->text.handles->key < key)
		      for(next_handle = sparms->text.handles->next;
			  next_handle->key != key;
			  next_handle = next_handle->next)
			;
		    else
		      for(next_handle = sparms->text.handles;
			  next_handle->key != key;
			  next_handle = next_handle->prev)
			;
		}
            }
	    else if(found_on == -1)
	      cmd_cancelled("Search");
            else
	      q_status_message(SM_ORDER, 0, 3, _("Word not found"));
	    }

            break; 


            /*-------------- jump command -------------*/
	    /* NOTE: preempt the process_cmd() version because
	     *	     we need to get at the number..
	     */
	  case MC_JUMP :
	    jn = jump_to(ps_global->msgmap, -FOOTER_ROWS(ps_global), ch,
			 sparms, View);
	    if(sparms && sparms->jump_is_debug)
	      done = 1;
	    else if(jn > 0 && jn != mn_get_cur(ps_global->msgmap)){

		if(mn_total_cur(ps_global->msgmap) > 1L)
		  mn_reset_cur(ps_global->msgmap, jn);
		else
		  mn_set_cur(ps_global->msgmap, jn);
		
		done = 1;
	    }
	    else
	      ps_global->mangled_footer = 1;

	    break;


#ifdef MOUSE	    
            /*-------------- Mouse Event -------------*/
	  case MC_MOUSE:
	    {
	      MOUSEPRESS mp;
	      long	line;
	      int	key;

	      mouse_get_last (NULL, &mp);
	      mp.row -= 2;

	      /* The clicked line have anything special on it? */
	      if((line = cur_top_line + mp.row) < scroll_text_lines()
		 && (key = dot_on_handle(line, mp.col))){
		  switch(mp.button){
		    case M_BUTTON_RIGHT :
#ifdef	_WINDOWS
		      if(sparms->mouse.popup){
			  if(sparms->text.handles->key < key)
			    for(next_handle = sparms->text.handles->next;
				next_handle->key != key;
				next_handle = next_handle->next)
			      ;
			  else
			    for(next_handle = sparms->text.handles;
				next_handle->key != key;
				next_handle = next_handle->prev)
			      ;

			  if(sparms->mouse.popup){
			      cur_top_line = scroll_scroll_text(cur_top_line,
						     next_handle,
						     ps_global->mangled_body);
			      fflush(stdout);
			      switch((*sparms->mouse.popup)(sparms, key)){
				case 1 :
				  cur_top_line = doubleclick_handle(sparms, next_handle, &cmd, &done);
				  break;

				case 2 :
				  done++;
				  break;
			      }
			  }
		      }

#endif
		      break;

		    case M_BUTTON_LEFT :
		      if(sparms->text.handles->key < key)
			for(next_handle = sparms->text.handles->next;
			    next_handle->key != key;
			    next_handle = next_handle->next)
			  ;
		      else
			for(next_handle = sparms->text.handles;
			    next_handle->key != key;
			    next_handle = next_handle->prev)
			  ;

		      if(mp.doubleclick)		/* launch url */
			cur_top_line = doubleclick_handle(sparms, next_handle, &cmd, &done);
		      else if(sparms->mouse.click)
			(*sparms->mouse.click)(sparms);

		      break;

		    case M_BUTTON_MIDDLE :		/* NO-OP for now */
		      break;

		    default:				/* just ignore */
		      break;
		  }
	      }
#ifdef	_WINDOWS
	      else if(mp.button == M_BUTTON_RIGHT){
		  /*
		   * Toss generic popup on to the screen
		   */
		  if(sparms->mouse.popup)
		    if((*sparms->mouse.popup)(sparms, 0) == 2){
			done++;
		    }
	      }
#endif
	    }

	    break;
#endif	/* MOUSE */


            /*-------------- Display Resize -------------*/
          case MC_RESIZE :
	    if(sparms->resize_exit){
		long	    line;
		
		/*
		 * Figure out char offset of the char in the top left
		 * corner of the display.  Pass it back to the
		 * fetcher/formatter and have it pass the offset
		 * back to us...
		 */
		sparms->start.on = Offset;
		for(sparms->start.loc.offset = line = 0L;
		    line < cur_top_line;
		    line++)
		  sparms->start.loc.offset += scroll_handle_column(line, -1);

		done = 1;
		ClearLine(1);
		break;
	    }
	    /* else no reformatting neccessary, fall thru to repaint */


            /*-------------- refresh -------------*/
          case MC_REPAINT :
            num_display_lines = SCROLL_LINES(ps_global);
	    mark_status_dirty();
	    mark_keymenu_dirty();
	    mark_titlebar_dirty();
            ps_global->mangled_screen = 1;
	    force                     = 1;
            break;


            /*------- no op timeout to check for new mail ------*/
          case MC_NONE :
            break;


	    /*------- Forward displayed text ------*/
	  case MC_FWDTEXT :
	    forward_text(ps_global, sparms->text.text, sparms->text.src);
	    break;


	    /*----------- Save the displayed text ------------*/
	  case MC_SAVETEXT :
	    (void)simple_export(ps_global, sparms->text.text,
				sparms->text.src, "text", NULL);
	    break;


	    /*----------- Exit this screen ------------*/
	  case MC_EXIT :
	    done = 1;
	    break;


	    /*----------- Pop back to the Main Menu ------------*/
	  case MC_MAIN :
	    ps_global->next_screen = main_menu_screen;
	    done = 1;
	    break;


	    /*----------- Print ------------*/
	  case MC_PRINTTXT :
	    print_to_printer(sparms);
	    break;


	    /* ------- First handle on Line ------ */
	  case MC_GOTOBOL :
	    if(sparms->text.handles){
		next_handle = scroll_handle_boundary(sparms->text.handles, 
						     scroll_handle_prev_sel);

		break;
	    }
	    /* fall thru as bogus */

	    /* ------- Last handle on Line ------ */
	  case MC_GOTOEOL :
	    if(sparms->text.handles){
		next_handle = scroll_handle_boundary(sparms->text.handles, 
						     scroll_handle_next_sel);

		break;
	    }
	    /* fall thru as bogus */

            /*------- BOGUS INPUT ------*/
	  case MC_CHARRIGHT :
	  case MC_CHARLEFT :
          case MC_UNKNOWN :
	    if(sparms->bogus_input)
	      done = (*sparms->bogus_input)(ch);
	    else
	      bogus_command(ch, F_ON(F_USE_FK,ps_global) ? "F1" : "?");

            break;


	  case MC_UTF8:
	    bogus_utf8_command(utf8str, F_ON(F_USE_FK, ps_global) ? "F1" : "?");
	    break;


	    /*------- Standard commands ------*/
          default:
	    whereis_pos.row = 0;
	    if(sparms->proc.tool)
	      result = (*sparms->proc.tool)(cmd, ps_global->msgmap, sparms);
	    else
	      result = process_cmd(ps_global, ps_global->mail_stream,
				   ps_global->msgmap, cmd, View, &force);

	    dprint((7, "PROCESS_CMD return: %d\n", result));

	    if(ps_global->next_screen != SCREEN_FUN_NULL || result == 1){
		done = 1;
		if(cmd == MC_FULLHDR){
		  if(ps_global->full_header == 1){
		      long	    line;
		    
		      /*
		       * Figure out char offset of the char in the top left
		       * corner of the display.  Pass it back to the
		       * fetcher/formatter and have it pass the offset
		       * back to us...
		       */
		      sparms->start.on = Offset;
		      for(sparms->start.loc.offset = line = 0L;
			  line < cur_top_line;
			  line++)
			sparms->start.loc.offset +=
					scroll_handle_column(line, -1);
		  }
		  else
		    sparms->start.on = 0;

		  switch(km->which){
		    case 0:
		      sparms->keys.what = FirstMenu;
		      break;
		    case 1:
		      sparms->keys.what = SecondMenu;
		      break;
		    case 2:
		      sparms->keys.what = ThirdMenu;
		      break;
		    case 3:
		      sparms->keys.what = FourthMenu;
		      break;
		  }
		}
	    }
	    else if(!scroll_state(SS_CUR)){
		num_display_lines	  = SCROLL_LINES(ps_global);
		ps_global->mangled_screen = 1;
	    }

	    break;

	} /* End of switch() */

	/* Need to frame some handles? */
	if(sparms->text.handles
	   && ((!next_handle
		&& handle_on_page(sparms->text.handles, cur_top_line,
				  cur_top_line + num_display_lines))
	       || (next_handle
		   && handle_on_page(next_handle, cur_top_line,
				     cur_top_line + num_display_lines))))
	  next_handle = scroll_handle_in_frame(cur_top_line);

    } /* End of while() -- loop executing commands */

    ps_global->redrawer	= NULL;	/* next statement makes this invalid! */
    zero_scroll_text();		/* very important to zero out on return!!! */
    scroll_state(SS_FREE);
    if(sparms->bar.color)
      free_color_pair(&sparms->bar.color);

#ifdef	_WINDOWS
    scroll_setrange(0L, 0L);
#endif
    return(cmd);
}


/*----------------------------------------------------------------------
      Print text on paper

    Args:  text -- The text to print out
	   source -- What type of source text is
	   message -- Message for open_printer()
    Handling of error conditions is very poor.

  ----*/
static int
print_to_printer(SCROLL_S *sparms)
{
    char message[64];

    snprintf(message, sizeof(message), "%s", STYLE_NAME(sparms));
    message[sizeof(message)-1] = '\0';

    if(open_printer(message) != 0)
      return(-1);

    switch(sparms->text.src){
      case CharStar :
	if(sparms->text.text != (char *)NULL)
	  print_text((char *)sparms->text.text);

	break;

      case CharStarStar :
	if(sparms->text.text != (char **)NULL){
	    register char **t;

	    for(t = sparms->text.text; *t != NULL; t++){
		print_text(*t);
		print_text(NEWLINE);
	    }
	}

	break;

      case FileStar :
	if(sparms->text.text != (FILE *)NULL) {
	    size_t n;
	    int i;

	    fseek((FILE *)sparms->text.text, 0L, 0);
	    n = SIZEOF_20KBUF - 1;
	    while((i = fread((void *)tmp_20k_buf, sizeof(char),
			    n, (FILE *)sparms->text.text)) != 0) {
		tmp_20k_buf[i] = '\0';
		print_text(tmp_20k_buf);
	    }
	}

      default :
	break;
    }

    close_printer();
    return(0);
}


/*----------------------------------------------------------------------
   Search text being viewed (help or message)

      Args: q_line      -- The screen line to prompt for search string on
            start_line  -- Line number in text to begin search on
            start_index -- Where to begin search at in first line of text
            cursor_pos  -- position of cursor is returned to caller here
			   (Actually, this isn't really the position of the
			    cursor because we don't know where we are on the
			    screen.  So row is set to the line number and col
			    is set to the right column.)
            offset_in_line -- Offset where match was found.

    Result: returns line number string was found on
            -1 for cancel
            -2 if not found
            -3 if only match is at start_index - 1
	    -4 if search to first line
	    -5 if search to last line
 ---*/
int
search_text(int q_line, long int start_line, int start_index, char **report,
	    Pos *cursor_pos, int *offset_in_line)
{
    char        prompt[MAX_SEARCH+50], nsearch_string[MAX_SEARCH+1], *p;
    HelpType	help;
    int         rc, flags;
    static HISTORY_S *history = NULL;
    char search_string[MAX_SEARCH+1];
    static ESCKEY_S word_search_key[] = { { 0, 0, "", "" },
					 {ctrl('Y'), 10, "^Y", N_("First Line")},
					 {ctrl('V'), 11, "^V", N_("Last Line")},
					 {KEY_UP,    30, "", ""},
					 {KEY_DOWN,  31, "", ""},
					 {-1, 0, NULL, NULL}
					};
#define KU_ST (3)	/* index of KEY_UP */

    init_hist(&history, HISTSIZE);

    /*
     * Put the last one used in the default search_string,
     * not in nsearch_string.
     */
    search_string[0] = '\0';
    if((p = get_prev_hist(history, "", 0, NULL)) != NULL){
	strncpy(search_string, p, sizeof(search_string));
	search_string[sizeof(search_string)-1] = '\0';
    }

    snprintf(prompt, sizeof(prompt), _("Word to search for [%s] : "), search_string);
    help = NO_HELP;
    nsearch_string[0] = '\0';

    while(1) {
	flags = OE_APPEND_CURRENT | OE_SEQ_SENSITIVE | OE_KEEP_TRAILING_SPACE;

	/*
	 * 2 is really 1 because there will be one real entry and
	 * one entry of "" because of the get_prev_hist above.
	 */
	if(items_in_hist(history) > 2){
	    word_search_key[KU_ST].name  = HISTORY_UP_KEYNAME;
	    word_search_key[KU_ST].label = HISTORY_KEYLABEL;
	    word_search_key[KU_ST+1].name  = HISTORY_DOWN_KEYNAME;
	    word_search_key[KU_ST+1].label = HISTORY_KEYLABEL;
	}
	else{
	    word_search_key[KU_ST].name  = "";
	    word_search_key[KU_ST].label = "";
	    word_search_key[KU_ST+1].name  = "";
	    word_search_key[KU_ST+1].label = "";
	}
	
        rc = optionally_enter(nsearch_string, q_line, 0, sizeof(nsearch_string),
                              prompt, word_search_key, help, &flags);

        if(rc == 3) {
            help = help == NO_HELP ? h_oe_searchview : NO_HELP;
            continue;
        }
	else if(rc == 10){
	    if(report)
	      *report = _("Searched to First Line.");

	    return(-4);
	}
	else if(rc == 11){
	    if(report)
	      *report = _("Searched to Last Line.");

	    return(-5);
	}
	else if(rc == 30){
	    if((p = get_prev_hist(history, nsearch_string, 0, NULL)) != NULL){
		strncpy(nsearch_string, p, sizeof(nsearch_string));
		nsearch_string[sizeof(nsearch_string)-1] = '\0';
	    }
	    else
	      Writechar(BELL, 0);

	    continue;
	}
	else if(rc == 31){
	    if((p = get_next_hist(history, nsearch_string, 0, NULL)) != NULL){
		strncpy(nsearch_string, p, sizeof(nsearch_string));
		nsearch_string[sizeof(nsearch_string)-1] = '\0';
	    }
	    else
	      Writechar(BELL, 0);

	    continue;
	}

        if(rc != 4){			/* 4 is redraw */
	    save_hist(history, nsearch_string, 0, NULL);
	    break;
	}
    }

    if(rc == 1 || (search_string[0] == '\0' && nsearch_string[0] == '\0'))
      return(-1);

    if(nsearch_string[0] != '\0'){
	strncpy(search_string, nsearch_string, sizeof(search_string)-1);
	search_string[sizeof(search_string)-1] = '\0';
    }

    rc = search_scroll_text(start_line, start_index, search_string, cursor_pos,
			    offset_in_line);
    return(rc);
}


/*----------------------------------------------------------------------
  Update the scroll tool's titlebar

    Args:  cur_top_line --
	   redraw -- flag to force updating

  ----*/
void
update_scroll_titlebar(long int cur_top_line, int redraw)
{
    SCRLCTRL_S *st = scroll_state(SS_CUR);
    int		num_display_lines = SCROLL_LINES(ps_global);
    long	new_line = (cur_top_line + num_display_lines > st->num_lines)
			     ? st->num_lines
			     : cur_top_line + num_display_lines;
    long        raw_msgno;
    COLOR_PAIR *returned_color = NULL;
    COLOR_PAIR *titlecolor = NULL;
    int         colormatch;
    SEARCHSET  *ss = NULL;

    if(st->parms->use_indexline_color
       && ps_global->titlebar_color_style != TBAR_COLOR_DEFAULT){
	raw_msgno = mn_m2raw(ps_global->msgmap, mn_get_cur(ps_global->msgmap));
	if(raw_msgno > 0L && ps_global->mail_stream
	   && raw_msgno <= ps_global->mail_stream->nmsgs){
	    ss = mail_newsearchset();
	    ss->first = ss->last = (unsigned long) raw_msgno;
	}

	if(ss){
	    PAT_STATE  *pstate = NULL;

	    colormatch = get_index_line_color(ps_global->mail_stream,
					      ss, &pstate, &returned_color);
	    mail_free_searchset(&ss);

	    /*
	     * This is a bit tricky. If there is a colormatch but returned_color
	     * is NULL, that means that the user explicitly wanted the
	     * Normal color used in this index line, so that is what we
	     * use. If no colormatch then we will use the TITLE color
	     * instead of Normal.
	     */
	    if(colormatch){
		if(returned_color)
		  titlecolor = returned_color;
		else
		  titlecolor = new_color_pair(ps_global->VAR_NORM_FORE_COLOR,
					      ps_global->VAR_NORM_BACK_COLOR);
	    }

	    if(titlecolor
	       && ps_global->titlebar_color_style == TBAR_COLOR_REV_INDEXLINE){
		char cbuf[MAXCOLORLEN+1];

		strncpy(cbuf, titlecolor->fg, MAXCOLORLEN);
		strncpy(titlecolor->fg, titlecolor->bg, MAXCOLORLEN);
		strncpy(titlecolor->bg, cbuf, MAXCOLORLEN);
	    }
	}
	
	/* Did the color change? */
	if((!titlecolor && st->parms->bar.color)
	   ||
	   (titlecolor && !st->parms->bar.color)
	   ||
	   (titlecolor && st->parms->bar.color
	    && (strcmp(titlecolor->fg, st->parms->bar.color->fg)
		|| strcmp(titlecolor->bg, st->parms->bar.color->bg)))){

	    redraw++;
	    if(st->parms->bar.color)
	      free_color_pair(&st->parms->bar.color);
	    
	    st->parms->bar.color = titlecolor;
	    titlecolor = NULL;
	}

	if(titlecolor)
	  free_color_pair(&titlecolor);
    }


    if(redraw){
	set_titlebar(st->parms->bar.title, ps_global->mail_stream,
		     ps_global->context_current, ps_global->cur_folder,
		     ps_global->msgmap, 1, st->parms->bar.style,
		     new_line, st->num_lines, st->parms->bar.color);
	ps_global->mangled_header = 0;
    }
    else if(st->parms->bar.style == TextPercent)
      update_titlebar_lpercent(new_line);
    else
      update_titlebar_percent(new_line);
}


/*----------------------------------------------------------------------
  manager of global (to this module, anyway) scroll state structures


  ----*/
SCRLCTRL_S *
scroll_state(int func)
{
    struct scrollstack {
	SCRLCTRL_S	    s;
	struct scrollstack *prev;
    } *s;
    static struct scrollstack *stack = NULL;

    switch(func){
      case SS_CUR:			/* no op */
	break;
      case SS_NEW:
	s = (struct scrollstack *)fs_get(sizeof(struct scrollstack));
	memset((void *)s, 0, sizeof(struct scrollstack));
	s->prev = stack;
	stack  = s;
	break;
      case SS_FREE:
	if(stack){
	    s = stack->prev;
	    fs_give((void **)&stack);
	    stack = s;
	}
	break;
      default:				/* BUG: should complain */
	break;
    }

    return(stack ? &stack->s : NULL);
}


/*----------------------------------------------------------------------
      Save all the data for scrolling text and paint the screen


  ----*/
void
set_scroll_text(SCROLL_S *sparms, long int current_line, SCRLCTRL_S *st)
{
    /* save all the stuff for possible asynchronous redraws */
    st->parms		   = sparms;
    st->top_text_line      = current_line;
    st->screen.start_line  = SCROLL_LINES_ABOVE(ps_global);
    st->screen.other_lines = SCROLL_LINES_ABOVE(ps_global)
				+ SCROLL_LINES_BELOW(ps_global);
    st->screen.width	   = -1;	/* Force text formatting calculation */
}


/*----------------------------------------------------------------------
     Redraw the text on the screen, possibly reformatting if necessary

   Args None

 ----*/
void
redraw_scroll_text(void)
{
    int		i, offset;
    SCRLCTRL_S *st = scroll_state(SS_CUR);

    format_scroll_text();

    offset = (st->parms->text.src == FileStar) ? 0 : st->top_text_line;

#ifdef _WINDOWS
    mswin_beginupdate();
#endif
    /*---- Actually display the text on the screen ------*/
    for(i = 0; i < st->screen.length; i++){
	ClearLine(i + st->screen.start_line);
	if((offset + i) < st->num_lines) 
	  PutLine0n8b(i + st->screen.start_line, 0, st->text_lines[offset + i],
		      st->line_lengths[offset + i], st->parms->text.handles);
    }


    fflush(stdout);
#ifdef _WINDOWS
    mswin_endupdate();
#endif
}


/*----------------------------------------------------------------------
  Free memory used as scrolling buffers for text on disk.  Also mark
  text_lines as available
  ----*/
void
zero_scroll_text(void)
{
    SCRLCTRL_S	 *st = scroll_state(SS_CUR);
    register int  i;

    for(i = 0; i < st->lines_allocated; i++)
      if(st->parms->text.src == FileStar && st->text_lines[i])
	fs_give((void **)&st->text_lines[i]);
      else
	st->text_lines[i] = NULL;

    if(st->parms->text.src == FileStar && st->findex != NULL){
	fclose(st->findex);
	st->findex = NULL;
	if(st->fname){
	    our_unlink(st->fname);
	    fs_give((void **)&st->fname);
	}
    }

    if(st->text_lines)
      fs_give((void **)&st->text_lines);

    if(st->line_lengths)
      fs_give((void **) &st->line_lengths);
}


/*----------------------------------------------------------------------

Always format at least 20 chars wide. Wrapping lines would be crazy for
screen widths of 1-20 characters 
  ----*/
void
format_scroll_text(void)
{
    int		     i;
    char	    *p, **pp;
    SCRLCTRL_S	    *st = scroll_state(SS_CUR);
    register short  *ll;
    register char  **tl, **tl_end;

    if(!st || (st->screen.width == (i = ps_global->ttyo->screen_cols)
	       && st->screen.length == PGSIZE(st)))
        return;

    st->screen.width = MAX(20, i);
    st->screen.length = PGSIZE(st);

    if(st->lines_allocated == 0) {
        st->lines_allocated = TYPICAL_BIG_MESSAGE_LINES;
        st->text_lines = (char **)fs_get(st->lines_allocated *sizeof(char *));
	memset(st->text_lines, 0, st->lines_allocated * sizeof(char *));
        st->line_lengths = (short *)fs_get(st->lines_allocated *sizeof(short));
    }

    tl     = st->text_lines;
    ll     = st->line_lengths;
    tl_end = &st->text_lines[st->lines_allocated];

    if(st->parms->text.src == CharStarStar) {
        /*---- original text is already list of lines -----*/
        /*   The text could be wrapped nicely for narrow screens; for now
             it will get truncated as it is displayed */
        for(pp = (char **)st->parms->text.text; *pp != NULL;) {
            *tl++ = *pp++;
            *ll++ = st->screen.width;
            if(tl >= tl_end) {
		i = tl - st->text_lines;
                st->lines_allocated *= 2;
                fs_resize((void **)&st->text_lines,
                          st->lines_allocated * sizeof(char *));
                fs_resize((void **)&st->line_lengths,
                          st->lines_allocated*sizeof(short));
                tl     = &st->text_lines[i];
                ll     = &st->line_lengths[i];
                tl_end = &st->text_lines[st->lines_allocated];
            }
        }

	st->num_lines = tl - st->text_lines;
    }
    else if (st->parms->text.src == CharStar) {
	/*------ Format the plain text ------*/
	for(p = (char *)st->parms->text.text; *p; ) {
            *tl = p;

	    for(; *p && !(*p == RETURN || *p == LINE_FEED); p++)
	      ;

	    *ll = p - *tl;
	    ll++; tl++;
	    if(tl >= tl_end) {
		i = tl - st->text_lines;
		st->lines_allocated *= 2;
		fs_resize((void **)&st->text_lines,
			  st->lines_allocated * sizeof(char *));
		fs_resize((void **)&st->line_lengths,
			  st->lines_allocated*sizeof(short));
		tl     = &st->text_lines[i];
		ll     = &st->line_lengths[i];
		tl_end = &st->text_lines[st->lines_allocated];
	    }

	    if(*p == '\r' && *(p+1) == '\n') 
	      p += 2;
	    else if(*p == '\n' || *p == '\r')
	      p++;
	}

	st->num_lines = tl - st->text_lines;
    }
    else {
	/*------ Display text is in a file --------*/

	/*
	 * This is pretty much only useful under DOS where we can't fit
	 * all of big messages in core at once.  This scheme makes
	 * some simplifying assumptions:
	 *  1. Lines are on disk just the way we'll display them.  That
	 *     is, line breaks and such are left to the function that
	 *     writes the disk file to catch and fix.
	 *  2. We get away with this mainly because the DOS display isn't
	 *     going to be resized out from under us.
	 *
	 * The idea is to use the already alloc'd array of char * as a 
	 * buffer for sections of what's on disk.  We'll set up the first
	 * few lines here, and read new ones in as needed in 
	 * scroll_scroll_text().
	 *  
	 * but first, make sure there are enough buffer lines allocated
	 * to serve as a place to hold lines from the file.
	 *
	 *   Actually, this is also used under windows so the display will
	 *   be resized out from under us.  So I changed the following
	 *   to always
	 *	1.  free old text_lines, which may have been allocated
	 *	    for a narrow screen.
	 *	2.  insure we have enough text_lines
	 *	3.  reallocate all text_lines that are needed.
	 *   (tom unger  10/26/94)
	 */

	/* free old text lines, which may be too short. */
	for(i = 0; i < st->lines_allocated; i++)
	  if(st->text_lines[i]) 		/* clear alloc'd lines */
	    fs_give((void **)&st->text_lines[i]);

        /* Insure we have enough text lines. */
	if(st->lines_allocated < (2 * PGSIZE(st)) + 1){
	    st->lines_allocated = (2 * PGSIZE(st)) + 1; /* resize */

	    fs_resize((void **)&st->text_lines,
		      st->lines_allocated * sizeof(char *));
	    memset(st->text_lines, 0, st->lines_allocated * sizeof(char *));
	    fs_resize((void **)&st->line_lengths,
		      st->lines_allocated*sizeof(short));
	}

	/* reallocate all text lines that are needed. */
	for(i = 0; i <= PGSIZE(st); i++)
	  if(st->text_lines[i] == NULL)
	    st->text_lines[i] = (char *)fs_get((st->screen.width + 1)
							       * sizeof(char));

	tl = &st->text_lines[i];

	st->num_lines = make_file_index();

	ScrollFile(st->top_text_line);		/* then load them up */
    }

    /*
     * Efficiency hack.  If there are handles, fill in their
     * line number field for later...
     */
    if(st->parms->text.handles){
	long	  line;
	int	  i, col, n, key;
	HANDLE_S *h;

	for(line = 0; line < st->num_lines; line++)
	  for(i = 0, col = 0; i < st->line_lengths[line];)
	    switch(st->text_lines[line][i]){
	      case TAG_EMBED:
		i++;
		switch((i < st->line_lengths[line]) ? st->text_lines[line][i]
						    : 0){
		  case TAG_HANDLE:
		    for(key = 0, n = st->text_lines[line][++i]; n > 0; n--)
		      key = (key * 10) + (st->text_lines[line][++i] - '0');

		    i++;
		    for(h = st->parms->text.handles; h; h = h->next)
		      if(h->key == key){
			  scroll_handle_set_loc(&h->loc, line, col);
			  break;
		      }

		    if(!h)	/* anything behind us? */
		      for(h = st->parms->text.handles->prev; h; h = h->prev)
			if(h->key == key){
			    scroll_handle_set_loc(&h->loc, line, col);
			    break;
			}
		    
		    break;

		  case TAG_FGCOLOR :
		  case TAG_BGCOLOR :
		    i += (RGBLEN + 1);	/* 1 for TAG, RGBLEN for color */
		    break;

		  case TAG_INVON:
		  case TAG_INVOFF:
		  case TAG_BOLDON:
		  case TAG_BOLDOFF:
		  case TAG_ULINEON:
		  case TAG_ULINEOFF:
		    i++;
		    break;
		
		  default:	/* literal embed char */
		    break;
		}

		break;

	      case TAB:
		i++;
		while(((++col) &  0x07) != 0) /* add tab's spaces */
		  ;

		break;

	      default:
		col += width_at_this_position((unsigned char*) &st->text_lines[line][i],
					      st->line_lengths[line] - i);
	        i++;				/* character count */
		break;
	    }
    }

#ifdef	_WINDOWS
    scroll_setrange (st->screen.length, st->num_lines);
#endif

    *tl = NULL;
}


/*
 * ScrollFile - scroll text into the st struct file making sure 'line'
 *              of the file is the one first in the text_lines buffer.
 *
 *   NOTE: talk about massive potential for tuning...
 *         Goes without saying this is still under constuction
 */
void
ScrollFile(long int line)
{
    SCRLCTRL_S	 *st = scroll_state(SS_CUR);
    SCRLFILE_S	  sf;
    register int  i;

    if(line <= 0){		/* reset and load first couple of pages */
	fseek((FILE *) st->parms->text.text, 0L, 0);
	line = 0L;
    }

    if(!st->text_lines)
      return;

    for(i = 0; i < PGSIZE(st); i++){
	/*** do stuff to get the file pointer into the right place ***/
	/*
	 * BOGUS: this is painfully crude right now, but I just want to get
	 * it going. 
	 *
	 * possibly in the near furture, an array of indexes into the 
	 * file that are the offset for the beginning of each line will
	 * speed things up.  Of course, this
	 * will have limits, so maybe a disk file that is an array
	 * of indexes is the answer.
	 */
	if(fseek(st->findex, (size_t)(line++) * sizeof(SCRLFILE_S), 0) < 0
	   || fread(&sf, sizeof(SCRLFILE_S), (size_t)1, st->findex) != 1
	   || fseek((FILE *) st->parms->text.text, sf.offset, 0) < 0
	   || !st->text_lines[i]
	   || (sf.len && !fgets(st->text_lines[i], sf.len + 1,
				(FILE *) st->parms->text.text)))
	  break;

	st->line_lengths[i] = sf.len;
    }

    for(; i < PGSIZE(st); i++)
      if(st->text_lines[i]){		/* blank out any unused lines */
	  *st->text_lines[i]  = '\0';
	  st->line_lengths[i] = 0;
      }
}


/*
 * make_file_index - do a single pass over the file containing the text
 *                   to display, recording line lengths and offsets.
 *    NOTE: This is never really to be used on a real OS with virtual
 *          memory.  This is the whole reason st->findex exists.  Don't
 *          want to waste precious memory on a stupid array that could 
 *          be very large.
 */
long
make_file_index(void)
{
    SCRLCTRL_S	  *st = scroll_state(SS_CUR);
    SCRLFILE_S	   sf;
    long	   l = 0L;
    int		   state = 0;

    if(!st->findex){
	if(!st->fname)
	  st->fname = temp_nam(NULL, "pi");

	if(!st->fname || (st->findex = our_fopen(st->fname,"w+b")) == NULL){
	    if(st->fname){
		our_unlink(st->fname);
		fs_give((void **)&st->fname);
	    }

	    return(0);
	}
    }
    else
      fseek(st->findex, 0L, 0);

    fseek((FILE *)st->parms->text.text, 0L, 0);

    while(1){
	sf.len = st->screen.width + 1;
	if(scroll_file_line((FILE *) st->parms->text.text,
			    tmp_20k_buf, &sf, &state)){
	    fwrite((void *) &sf, sizeof(SCRLFILE_S), (size_t)1, st->findex);
	    l++;
	}
	else
	  break;
    }

    fseek((FILE *)st->parms->text.text, 0L, 0);

    return(l);
}


/*----------------------------------------------------------------------
     Get the next line to scroll from the given file

 ----*/
char *
scroll_file_line(FILE *fp, char *buf, SCRLFILE_S *sfp, int *wrapt)
{
    register char *s = NULL;

    while(1){
	if(!s){
	    sfp->offset = ftell(fp);
	    if(!(s = fgets(buf, sfp->len, fp)))
	      return(NULL);			/* can't grab a line? */
	}

	if(!*s){
	    *wrapt = 1;			/* remember; that we wrapped */
	    break;
	}
	else if(*s == NEWLINE[0] && (!NEWLINE[1] || *(s+1) == NEWLINE[1])){
	    int empty = (*wrapt && s == buf);

	    *wrapt = 0;			/* turn off wrapped state */
	    if(empty)
	      s = NULL;			/* get a new line */
	    else
	      break;			/* done! */
	}
	else 
	  s++;
    }

    sfp->len = s - buf;
    return(buf);
}


/*----------------------------------------------------------------------
     Scroll the text on the screen

   Args:  new_top_line -- The line to be displayed on top of the screen
          redraw -- Flag to force a redraw even if nothing changed 

   Returns: resulting top line
   Note: the returned line number may be less than new_top_line if
	 reformatting caused the total line count to change.

 ----*/
long
scroll_scroll_text(long int new_top_line, HANDLE_S *handle, int redraw)
{
    SCRLCTRL_S *st = scroll_state(SS_CUR);
    int		num_display_lines, l, top;
    POSLIST_S  *lp, *lp2;

    /* When this is true, we're still on the same page of the display. */
    if(st->top_text_line == new_top_line && !redraw){
	/* handle changed, so hilite the new handle and unhilite the old */
	if(handle && handle != st->parms->text.handles){
	    top = st->screen.start_line - new_top_line;
	    /* hilite the new one */
	    if(!scroll_handle_obscured(handle))
	      for(lp = handle->loc; lp; lp = lp->next)
		if((l = lp->where.row) >= st->top_text_line
		   && l < st->top_text_line + st->screen.length){
		    if(st->parms->text.src == FileStar)
		      l -= new_top_line;

		    PutLine0n8b(top + lp->where.row, 0, st->text_lines[l],
				st->line_lengths[l], handle);
		}

	    /* unhilite the old one */
	    if(!scroll_handle_obscured(st->parms->text.handles))
	      for(lp = st->parms->text.handles->loc; lp; lp = lp->next)
	       if((l = lp->where.row) >= st->top_text_line
		  && l < st->top_text_line + st->screen.length){
		   for(lp2 = handle->loc; lp2; lp2 = lp2->next)
		     if(l == lp2->where.row)
		       break;

		   if(!lp2){
		       if(st->parms->text.src == FileStar)
			 l -= new_top_line;

		       PutLine0n8b(top + lp->where.row, 0, st->text_lines[l],
				   st->line_lengths[l], handle);
		   }
	       }

	    st->parms->text.handles = handle;	/* update current */
	}

	return(new_top_line);
    }

    num_display_lines = PGSIZE(st);

    format_scroll_text();

    if(st->top_text_line >= st->num_lines)	/* don't pop line count */
      new_top_line = st->top_text_line = MAX(st->num_lines - 1, 0);

    if(st->parms->text.src == FileStar)
      ScrollFile(new_top_line);		/* set up new st->text_lines */

#ifdef	_WINDOWS
    scroll_setrange (st->screen.length, st->num_lines);
    scroll_setpos (new_top_line);
#endif

    /* --- 
       Check out the scrolling situation. If we want to scroll, but BeginScroll
       says we can't then repaint,  + 10 is so we repaint most of the time.
      ----*/
    if(redraw ||
       (st->top_text_line - new_top_line + 10 >= num_display_lines ||
        new_top_line - st->top_text_line + 10 >= num_display_lines) ||
	BeginScroll(st->screen.start_line,
                    st->screen.start_line + num_display_lines - 1) != 0) {
        /* Too much text to scroll, or can't scroll -- just repaint */

	if(handle)
	  st->parms->text.handles = handle;

        st->top_text_line = new_top_line;
        redraw_scroll_text();
    }
    else{
	/*
	 * We're going to scroll the screen, but first we have to make sure
	 * the old hilited handles are unhilited if they are going to remain
	 * on the screen.
	 */
	top = st->screen.start_line - st->top_text_line;
	if(handle && handle != st->parms->text.handles
	   && st->parms->text.handles
	   && !scroll_handle_obscured(st->parms->text.handles))
	  for(lp = st->parms->text.handles->loc; lp; lp = lp->next)
	   if((l = lp->where.row) >= MAX(st->top_text_line,new_top_line)
	      && l < MIN(st->top_text_line,new_top_line) + st->screen.length){
	       if(st->parms->text.src == FileStar)
		 l -= new_top_line;

	       PutLine0n8b(top + lp->where.row, 0, st->text_lines[l],
			   st->line_lengths[l], handle);
	   }

	if(new_top_line > st->top_text_line){
	    /*------ scroll down ------*/
	    while(new_top_line > st->top_text_line) {
		ScrollRegion(1);

		l = (st->parms->text.src == FileStar)
		      ? num_display_lines - (new_top_line - st->top_text_line)
		      : st->top_text_line + num_display_lines;

		if(l < st->num_lines){
		  PutLine0n8b(st->screen.start_line + num_display_lines - 1,
			      0, st->text_lines[l], st->line_lengths[l],
			      handle ? handle : st->parms->text.handles);
		  /*
		   * We clear to the end of line in the right background
		   * color. If the line was exactly the width of the screen
		   * then PutLine0n8b will have left _col and _row moved to
		   * the start of the next row. We don't need or want to clear
		   * that next row.
		   */
		  if(pico_usingcolor()
		     && (st->line_lengths[l] < ps_global->ttyo->screen_cols
		         || visible_linelen(l) < ps_global->ttyo->screen_cols))
		    CleartoEOLN();
		}

		st->top_text_line++;
	    }
	}
	else{
	    /*------ scroll up -----*/
	    while(new_top_line < st->top_text_line) {
		ScrollRegion(-1);

		st->top_text_line--;
		l = (st->parms->text.src == FileStar)
		      ? st->top_text_line - new_top_line
		      : st->top_text_line;
		PutLine0n8b(st->screen.start_line, 0, st->text_lines[l],
			    st->line_lengths[l],
			    handle ? handle : st->parms->text.handles);
		/*
		 * We clear to the end of line in the right background
		 * color. If the line was exactly the width of the screen
		 * then PutLine0n8b will have left _col and _row moved to
		 * the start of the next row. We don't need or want to clear
		 * that next row.
		 */
		if(pico_usingcolor()
		   && (st->line_lengths[l] < ps_global->ttyo->screen_cols
		       || visible_linelen(l) < ps_global->ttyo->screen_cols))
		  CleartoEOLN();
	    }
	}

	EndScroll();

	if(handle && handle != st->parms->text.handles){
	    POSLIST_S *lp;

	    for(lp = handle->loc; lp; lp = lp->next)
	      if(lp->where.row >= st->top_text_line
		 && lp->where.row < st->top_text_line + st->screen.length){
		  PutLine0n8b(st->screen.start_line
					 + (lp->where.row - st->top_text_line),
			      0, st->text_lines[lp->where.row],
			      st->line_lengths[lp->where.row],
			      handle);
		  
	      }

	    st->parms->text.handles = handle;
	}

	fflush(stdout);
    }

    return(new_top_line);
}


/*---------------------------------------------------------------------
  Edit individual char in text so that the entire text doesn't need
  to be completely reformatted. 

  Returns 0 if there were no errors, 1 if we would like the entire
  text to be reformatted.
----*/
int
ng_scroll_edit(CONTEXT_S *context, int index)
{
    SCRLCTRL_S *st = scroll_state(SS_CUR);
    char *ngp, tmp[MAILTMPLEN+10];
    int   len;
    FOLDER_S *f;

    if (!(f = folder_entry(index, FOLDERS(context))))
      return 1;
    if(f->subscribed)
      return 0;  /* nothing in scroll needs to be changed */
    tmp[0] = TAG_HANDLE;
    snprintf(tmp+2, sizeof(tmp)-2, "%d", st->parms->text.handles->key);
    tmp[sizeof(tmp)-1] = '\0';
    tmp[1] = len = strlen(tmp+2);
    snprintf(tmp+len+2, sizeof(tmp)-(len+2), "%s ", f->selected ? "[ ]" : "[X]");
    tmp[sizeof(tmp)-1] = '\0';
    snprintf(tmp+len+6, sizeof(tmp)-(len+6), "%.*s", MAILTMPLEN, f->name);
    tmp[sizeof(tmp)-1] = '\0';
    
    ngp = *(st->text_lines);

    ngp = strstr(ngp, tmp);

    if(!ngp) return 1;
    ngp += 3+len;

    /* assumption that text is of form "[ ] xxx.xxx" */

    if(ngp){
      if(*ngp == 'X'){
	*ngp = ' ';
	return 0;
      }
      else if (*ngp == ' '){
	*ngp = 'X';
	return 0;
      }
    }
    return 1;
}


/*---------------------------------------------------------------------
  Similar to ng_scroll_edit, but this is the more general case of 
  selecting a folder, as opposed to selecting a newsgroup for 
  subscription while in listmode.

  Returns 0 if there were no errors, 1 if we would like the entire
  text to be reformatted.
----*/
int
folder_select_update(CONTEXT_S *context, int index)
{
    SCRLCTRL_S *st = scroll_state(SS_CUR);
    FOLDER_S *f;
    char *ngp, tmp[MAILTMPLEN+10];
    int len, total, fnum, num_sel = 0;

    if (!(f = folder_entry(index, FOLDERS(context))))
      return 1;
    ngp = *(st->text_lines);

    total = folder_total(FOLDERS(context));
      
    for (fnum = 0; num_sel < 2 && fnum < total; fnum++)
      if(folder_entry(fnum, FOLDERS(context))->selected)
	num_sel++;
    if(!num_sel || (f->selected && num_sel == 1))
      return 1;   /* need to reformat the whole thing */

    tmp[0] = TAG_HANDLE;
    snprintf(tmp+2, sizeof(tmp)-2, "%d", st->parms->text.handles->key);  
    tmp[sizeof(tmp)-1] = '\0';
    tmp[1] = len = strlen(tmp+2);

    ngp = strstr(ngp, tmp);
    if(!ngp) return 1;

    if(F_ON(F_SELECTED_SHOWN_BOLD, ps_global)){
      ngp += 2 + len;
      while(*ngp && ngp[0] != TAG_EMBED 
	    && ngp[1] != (f->selected ? TAG_BOLDOFF : TAG_BOLDON)
	    && *ngp != *(f->name))
	ngp++;

      if (!(*ngp) || (*ngp == *(f->name)))
	return 1;
      else {
	ngp++;
	*ngp = (f->selected ? TAG_BOLDON : TAG_BOLDOFF);
	return 0;
      }
    }
    else{
      while(*ngp != ' ' && *ngp != *(f->name) && *ngp)
	ngp++;
      if(!(*ngp) || (*ngp == *(f->name)))
	return 1;
      else {
	ngp++;
	*ngp = f->selected ? 'X' : ' ';
	return 0;
      }
    }
}


/*---------------------------------------------------------------------
  We gotta go through all of the formatted text and add "[ ] " in the right
  place.  If we don't do this, we must completely reformat the whole text,
  which could take a very long time.

  Return 1 if we encountered some sort of error and we want to reformat the
  whole text, return 0 if everything went as planned.

  ASSUMPTION: for this to work, we assume that there are only total
  number of handles, numbered 1 through total.
----*/
int
scroll_add_listmode(CONTEXT_S *context, int total)
{
    SCRLCTRL_S	    *st = scroll_state(SS_CUR);
    long             i;
    char            *ngp, *ngname, handle_str[MAILTMPLEN];
    HANDLE_S        *h;


    ngp = *(st->text_lines);
    h = st->parms->text.handles;

    while(h && h->key != 1 && h->prev)
      h = h->prev;
    if (!h) return 1;
    handle_str[0] = TAG_EMBED;
    handle_str[1] = TAG_HANDLE;
    for(i = 1; i <= total && h; i++, h = h->next){
      snprintf(handle_str+3, sizeof(handle_str)-3, "%d", h->key);
      handle_str[sizeof(handle_str)-1] = '\0';
      handle_str[2] = strlen(handle_str+3);
      ngp = strstr(ngp, handle_str);
      if(!ngp){
	ngp = *(st->text_lines);
	if (!ngp)
	  return 1;
      }
      ngname = ngp + strlen(handle_str);
      while (strncmp(ngp, "    ", 4) && !(*ngp == '\n')
	     && !(ngp == *(st->text_lines)))
	ngp--;
      if (strncmp(ngp, "    ", 4)) 
	return 1;
      while(ngp+4 != ngname && *ngp){
	ngp[0] = ngp[4];
	ngp++;
      }

      if(folder_entry(h->h.f.index, FOLDERS(context))->subscribed){
	ngp[0] = 'S';
	ngp[1] = 'U';
	ngp[2] = 'B';
      }
      else{
	ngp[0] = '[';
	ngp[1] = ' ';
	ngp[2] = ']';
      }
      ngp[3] = ' ';
    }

    return 0;
}



/*----------------------------------------------------------------------
      Search the set scrolling text

   Args:   start_line -- line to start searching on
	   start_index -- column to start searching at in first line
           word       -- string to search for
           cursor_pos -- position of cursor is returned to caller here
			 (Actually, this isn't really the position of the
			  cursor because we don't know where we are on the
			  screen.  So row is set to the line number and col
			  is set to the right column.)
           offset_in_line -- Offset where match was found.

   Returns: the line the word was found on, or -2 if it wasn't found, or
	    -3 if the only match is at column start_index - 1.

 ----*/
int
search_scroll_text(long int start_line, int start_index, char *word,
		   Pos *cursor_pos, int *offset_in_line)
{
    SCRLCTRL_S *st = scroll_state(SS_CUR);
    char       *wh;
    long	l, offset, dlines;
#define	SROW(N)		((N) - offset)
#define	SLINE(N)	st->text_lines[SROW(N)]
#define	SLEN(N)		st->line_lengths[SROW(N)]

    dlines = PGSIZE(st);
    offset = (st->parms->text.src == FileStar) ? st->top_text_line : 0;

    if(start_line < st->num_lines){
	/* search first line starting at position start_index in */
	if((wh = search_scroll_line(SLINE(start_line) + start_index,
				    word,
				    SLEN(start_line) - start_index,
				    st->parms->text.handles != NULL)) != NULL){
	    cursor_pos->row = start_line;
	    cursor_pos->col = scroll_handle_column(SROW(start_line),
				     *offset_in_line = wh - SLINE(start_line));
	    return(start_line);
	}

	if(st->parms->text.src == FileStar)
	  offset++;

	for(l = start_line + 1; l < st->num_lines; l++) {
	    if(st->parms->text.src == FileStar && l > offset + dlines)
	      ScrollFile(offset += dlines);

	    if((wh = search_scroll_line(SLINE(l), word, SLEN(l),
				    st->parms->text.handles != NULL)) != NULL){
		cursor_pos->row = l;
		cursor_pos->col = scroll_handle_column(SROW(l), 
					      *offset_in_line = wh - SLINE(l));
		return(l);
	    }
	}
    }
    else
      start_line = st->num_lines;

    if(st->parms->text.src == FileStar)		/* wrap offset */
      ScrollFile(offset = 0);

    for(l = 0; l < start_line; l++) {
	if(st->parms->text.src == FileStar && l > offset + dlines)
	  ScrollFile(offset += dlines);

	if((wh = search_scroll_line(SLINE(l), word, SLEN(l),
				    st->parms->text.handles != NULL)) != NULL){
	    cursor_pos->row = l;
	    cursor_pos->col = scroll_handle_column(SROW(l),
					      *offset_in_line = wh - SLINE(l));
	    return(l);
	}
    }

    /* search in current line */
    if(start_line < st->num_lines
       && (wh = search_scroll_line(SLINE(start_line), word,
				   start_index + strlen(word) - 2,
				   st->parms->text.handles != NULL)) != NULL){
	cursor_pos->row = start_line;
	cursor_pos->col = scroll_handle_column(SROW(start_line),
				     *offset_in_line = wh - SLINE(start_line));

	return(start_line);
    }

    /* see if the only match is a repeat */
    if(start_index > 0 && start_line < st->num_lines
		       && (wh = search_scroll_line(
				SLINE(start_line) + start_index - 1,
				word, strlen(word),
				st->parms->text.handles != NULL)) != NULL){
	cursor_pos->row = start_line;
	cursor_pos->col = scroll_handle_column(SROW(start_line), 
				     *offset_in_line = wh - SLINE(start_line));
	return(-3);
    }

    return(-2);
}


/*----------------------------------------------------------------------
   Search one line of scroll text for given string

   Args:  haystack -- The string to search in, the larger string
          needle   -- The string to search for, the smaller string
	  n        -- The max number of chars in haystack to search

   Search for first occurrence of needle in the haystack, and return a pointer
   into the string haystack when it is found. The search is case independent.
  ----*/
char *	    
search_scroll_line(char *haystack, char *needle, int n, int handles)
{
    char *return_ptr = NULL, *found_it = NULL;
    char *haystack_copy, *p, *free_this = NULL, *end;
    char  buf[1000];
    int   state = 0, i = 0;

    if(n > 0 && haystack){
	if(n < sizeof(buf))
	  haystack_copy = buf;
	else
	  haystack_copy = free_this = (char *) fs_get((n+1) * sizeof(char));

	strncpy(haystack_copy, haystack, n);
	haystack_copy[n] = '\0';

	/*
	 * We don't want to match text inside embedded tags.
	 * Replace embedded octets with nulls and convert
	 * uppercase ascii to lowercase. We should also do
	 * some sort of canonicalization of UTF-8 but that
	 * sounds daunting.
	 */
	for(i = n, p = haystack_copy; i-- > 0 && *p; p++){
	  if(handles)
	    switch(state){
	      case 0 :
		if(*p == TAG_EMBED){
		    *p = '\0';
		    state = -1;
		    continue;
		}
		else{
		    /* lower case just ascii chars */
		    if(!(*p & 0x80) && isupper(*p))
		      *p = tolower(*p);
		}

		break;

	      case -1 :
		state = (*p == TAG_HANDLE)
			  ? -2
			  : (*p == TAG_FGCOLOR || *p == TAG_BGCOLOR) ? RGBLEN : 0;
		*p = '\0';
		continue;

	      case -2 :
		state = *p;	/* length of handle's key */
		*p = '\0';
		continue;

	      default :
		state--;
		*p = '\0';
		continue;
	    }
	}

	/*
	 * The haystack_copy string now looks like
	 *
	 *  "chars\0\0\0\0\0\0chars...\0\0\0chars...    \0"
	 *
	 * with that final \0 at haystack_copy[n].
	 * Search each piece one at a time.
	 */

	end = haystack_copy + n;
	p = haystack_copy;

	while(p < end && !return_ptr){

	    /* skip nulls */
	    while(*p == '\0' && p < end)
	      p++;

	    if(*p != '\0')
	      found_it = srchstr(p, needle);

	    if(found_it){
		/* found it, make result relative to haystack */
		return_ptr = haystack + (found_it - haystack_copy);
	    }

	    /* skip to next null */
	    while(*p != '\0' && p < end)
	      p++;
	}

	if(free_this)
	  fs_give((void **) &free_this);
    }

    return(return_ptr);
}


/*
 * Returns the number of columns taken up by the visible part of the line.
 * That is, account for handles and color changes and so forth.
 */
int
visible_linelen(int line)
{
    SCRLCTRL_S *st = scroll_state(SS_CUR);
    int i, n, len = 0;

    if(line < 0 || line >= st->num_lines)
      return(len);

    for(i = 0, len = 0; i < st->line_lengths[line];)
      switch(st->text_lines[line][i]){

        case TAG_EMBED:
	  i++;
	  switch((i < st->line_lengths[line]) ? st->text_lines[line][i] : 0){
	    case TAG_HANDLE:
	      i++;
	      /* skip the length byte plus <length> more bytes */
	      if(i < st->line_lengths[line]){
		  n = st->text_lines[line][i];
		  i++;
	      }

	      if(i < st->line_lengths[line] && n > 0)
		i += n;

		break;

	    case TAG_FGCOLOR :
	    case TAG_BGCOLOR :
	      i += (RGBLEN + 1);	/* 1 for TAG, RGBLEN for color */
	      break;

	    case TAG_INVON:
	    case TAG_INVOFF:
	    case TAG_BOLDON:
	    case TAG_BOLDOFF:
	    case TAG_ULINEON:
	    case TAG_ULINEOFF:
	      i++;
	      break;

	    case TAG_EMBED:		/* escaped embed character */
	      i++;
	      len++;
	      break;
	    
	    default:			/* the embed char was literal */
	      i++;
	      len += 2;
	      break;
	  }

	  break;

	case TAB:
	  i++;
	  while(((++len) &  0x07) != 0)		/* add tab's spaces */
	    ;

	  break;

	default:
	  i++;
	  len++;
	  break;
      }

    return(len);
}


/*----------------------------------------------------------------------
    Display the contents of the given file (likely output from some command)

  Args: filename -- name of file containing output
	title -- title to be used for screen displaying output
	alt_msg -- if no output, Q this message instead of the default
	mode -- non-zero to display short files in status line
  Returns: none
 ----*/
void
display_output_file(char *filename, char *title, char *alt_msg, int mode)
{
    STORE_S *in_file = NULL, *out_store = NULL;

    if((in_file = so_get(FileStar, filename, READ_ACCESS|READ_FROM_LOCALE))){
	if(mode == DOF_BRIEF){
	    int  msg_q = 0, i = 0;
	    char buf[512], *msg_p[4];
#define	MAX_SINGLE_MSG_LEN	60

	    buf[0]   = '\0';
	    msg_p[0] = buf;
	    
	    /*
	     * Might need to do something about CRLFs for Windows.
	     */
	    while(so_fgets(in_file, msg_p[msg_q], sizeof(buf) - (msg_p[msg_q] - buf)) 
		  && msg_q < 3
		  && (i = strlen(msg_p[msg_q])) < MAX_SINGLE_MSG_LEN){
		msg_p[msg_q+1] = msg_p[msg_q]+strlen(msg_p[msg_q]);
		if (*(msg_p[++msg_q] - 1) == '\n')
		  *(msg_p[msg_q] - 1) = '\0';
	    }

	    if(msg_q < 3 && i < MAX_SINGLE_MSG_LEN){
		if(*msg_p[0])
		  for(i = 0; i < msg_q; i++)
		    q_status_message2(SM_ORDER, 3, 4,
				      "%s Result: %s", title, msg_p[i]);
		else
		  q_status_message2(SM_ORDER, 0, 4, "%s%s", title,
				    alt_msg
				      ? alt_msg
				      : " command completed with no output");

		so_give(&in_file);
		in_file = NULL;
	    }
	}
	else if(mode == DOF_EMPTY){
	    unsigned char c;

	    if(so_readc(&c, in_file) < 1){
		q_status_message2(SM_ORDER, 0, 4, "%s%s", title,
				  alt_msg
				    ? alt_msg
				    : " command completed with no output");
		so_give(&in_file);
		in_file = NULL;
	    }
	}

	/*
	 * We need to translate the file contents from the user's locale
	 * charset to UTF-8 for use in scrolltool. We get that translation
	 * from the READ_FROM_LOCALE in the in_file storage object.
	 * It would be nice to skip this step but scrolltool doesn't use
	 * the storage object routines to read from the file, so would
	 * skip the translation step.
	 */
	if(in_file){
	    char *errstr;
	    gf_io_t gc, pc;

	    if(!(out_store = so_get(CharStar, NULL, EDIT_ACCESS))){
		so_give(&in_file);
		our_unlink(filename);
		q_status_message(SM_ORDER | SM_DING, 3, 3,
				 _("Error allocating space."));
		return;
	    }

	    so_seek(in_file, 0L, 0);

	    gf_filter_init();

	    gf_link_filter(gf_wrap,
			   gf_wrap_filter_opt(ps_global->ttyo->screen_cols - 4,
					      ps_global->ttyo->screen_cols,
					      NULL, 0, GFW_NONE));

	    gf_set_so_readc(&gc, in_file);
	    gf_set_so_writec(&pc, out_store);

	    if((errstr = gf_pipe(gc, pc)) != NULL){
		so_give(&in_file);
		so_give(&out_store);
		our_unlink(filename);
		q_status_message(SM_ORDER | SM_DING, 3, 3,
				 _("Error allocating space."));
		return;
	    }

	    gf_clear_so_writec(out_store);
	    gf_clear_so_readc(in_file);
	    so_give(&in_file);
	}

	if(out_store){
	    SCROLL_S sargs;
	    char     title_buf[64];

		snprintf(title_buf, sizeof(title_buf), "HELP FOR %s VIEW", title);
	    title_buf[sizeof(title_buf)-1] = '\0';

	    memset(&sargs, 0, sizeof(SCROLL_S));
	    sargs.text.text  = so_text(out_store);
	    sargs.text.src   = CharStar;
	    sargs.text.desc  = "output";
	    sargs.bar.title  = title;
	    sargs.bar.style  = TextPercent;
	    sargs.help.text  = h_simple_text_view;
	    sargs.help.title = title_buf;
	    scrolltool(&sargs);
	    ps_global->mangled_screen = 1;
	    so_give(&out_store);
	}

	our_unlink(filename);
    }
    else
      dprint((2, "Error reopening %s to get results: %s\n",
		 filename ? filename : "?", error_description(errno)));
}


/*--------------------------------------------------------------------
  Call the function that will perform the double click operation

  Returns: the current top line
--------*/
long
doubleclick_handle(SCROLL_S *sparms, HANDLE_S *next_handle, int *cmd, int *done)
{
  if(sparms->mouse.clickclick){
    if((*sparms->mouse.clickclick)(sparms))
      *done = 1;
  }
  else
    switch(scroll_handle_launch(next_handle, TRUE)){
    case 1 :
      *cmd = MC_EXIT;		/* propagate */
      *done = 1;
      break;
      
    case -1 :
      cmd_cancelled("View");
      break;
      
    default :
      break;
    }
  
  return(scroll_state(SS_CUR)->top_text_line);
}



#ifdef	_WINDOWS
/*
 * Just a little something to simplify assignments
 */
#define	VIEWPOPUP(p, c, s)	{ \
				    (p)->type	      = tQueue; \
				    (p)->data.val     = c; \
				    (p)->label.style  = lNormal; \
				    (p)->label.string = s; \
				}


/*
 * 
 */
int
format_message_popup(sparms, in_handle)
    SCROLL_S *sparms;
    int	      in_handle;
{
    MPopup	  fmp_menu[32];
    HANDLE_S	 *h = NULL;
    int		  i = -1, n;
    long          rawno;
    MESSAGECACHE *mc;

    /* Reason to offer per message ops? */
    if(mn_get_total(ps_global->msgmap) > 0L){
	if(in_handle){
	    SCRLCTRL_S *st = scroll_state(SS_CUR);

	    switch((h = get_handle(st->parms->text.handles, in_handle))->type){
	      case Attach :
		fmp_menu[++i].type	 = tIndex;
		fmp_menu[i].label.string = "View Attachment";
		fmp_menu[i].label.style  = lNormal;
		fmp_menu[i].data.val     = 'X'; /* for local use */

		if(h->h.attach
		   && dispatch_attachment(h->h.attach) != MCD_NONE
		   && !(h->h.attach->can_display & MCD_EXTERNAL)
		   && h->h.attach->body
		   && (h->h.attach->body->type == TYPETEXT
		       || (h->h.attach->body->type == TYPEMESSAGE
			   && h->h.attach->body->subtype
			   && !strucmp(h->h.attach->body->subtype,"rfc822")))){
		    fmp_menu[++i].type	     = tIndex;
		    fmp_menu[i].label.string = "View Attachment in New Window";
		    fmp_menu[i].label.style  = lNormal;
		    fmp_menu[i].data.val     = 'Y';	/* for local use */
		}

		fmp_menu[++i].type	    = tIndex;
		fmp_menu[i].label.style = lNormal;
		fmp_menu[i].data.val    = 'Z';	/* for local use */
		msgno_exceptions(ps_global->mail_stream,
				 mn_m2raw(ps_global->msgmap,
					  mn_get_cur(ps_global->msgmap)),
				 h->h.attach->number, &n, FALSE);
		fmp_menu[i].label.string = (n & MSG_EX_DELETE)
					      ? "Undelete Attachment"
					      : "Delete Attachment";
		break;

	      case URL :
	      default :
		fmp_menu[++i].type	 = tIndex;
		fmp_menu[i].label.string = "View Link";
		fmp_menu[i].label.style  = lNormal;
		fmp_menu[i].data.val     = 'X';	/* for local use */

		fmp_menu[++i].type	 = tIndex;
		fmp_menu[i].label.string = "Copy Link";
		fmp_menu[i].label.style  = lNormal;
		fmp_menu[i].data.val     = 'W';	/* for local use */
		break;
	    }

	    fmp_menu[++i].type = tSeparator;
	}

	/* Delete or Undelete?  That is the question. */
	fmp_menu[++i].type	= tQueue;
	fmp_menu[i].label.style = lNormal;
	mc = ((rawno = mn_m2raw(ps_global->msgmap,
				mn_get_cur(ps_global->msgmap))) > 0L
	      && ps_global->mail_stream
	      && rawno <= ps_global->mail_stream->nmsgs)
	      ? mail_elt(ps_global->mail_stream, rawno) : NULL;
	if(mc && mc->deleted){
	    fmp_menu[i].data.val     = 'U';
	    fmp_menu[i].label.string = in_handle
					 ? "&Undelete Message" : "&Undelete";
	}
	else{
	    fmp_menu[i].data.val     = 'D';
	    fmp_menu[i].label.string = in_handle
					 ? "&Delete Message" : "&Delete";
	}

	if(F_ON(F_ENABLE_FLAG, ps_global)){
	    fmp_menu[++i].type	     = tSubMenu;
	    fmp_menu[i].label.string = "Flag";
	    fmp_menu[i].data.submenu = flag_submenu(mc);
	}

	i++;
	VIEWPOPUP(&fmp_menu[i], 'S', in_handle ? "&Save Message" : "&Save");

	i++;
	VIEWPOPUP(&fmp_menu[i], 'E', in_handle ? "&Export Message" : "&Export");

	i++;
	VIEWPOPUP(&fmp_menu[i], '%', in_handle ? "Print Message" : "Print");

	i++;
	VIEWPOPUP(&fmp_menu[i], 'R',
		  in_handle ? "&Reply to Message" : "&Reply");

	i++;
	VIEWPOPUP(&fmp_menu[i], 'F',
		  in_handle ? "&Forward Message" : "&Forward");

	i++;
	VIEWPOPUP(&fmp_menu[i], 'B',
		  in_handle ? "&Bounce Message" : "&Bounce");

	i++;
	VIEWPOPUP(&fmp_menu[i], 'T', "&Take Addresses");

	fmp_menu[++i].type = tSeparator;

	if(mn_get_cur(ps_global->msgmap) < mn_get_total(ps_global->msgmap)){
	    i++;
	    VIEWPOPUP(&fmp_menu[i], 'N', "View &Next Message");
	}

	if(mn_get_cur(ps_global->msgmap) > 0){
	    i++;
	    VIEWPOPUP(&fmp_menu[i], 'P', "View &Prev Message");
	}

	if(mn_get_cur(ps_global->msgmap) < mn_get_total(ps_global->msgmap)
	   || mn_get_cur(ps_global->msgmap) > 0)
	  fmp_menu[++i].type = tSeparator;

	/* Offer the attachment screen? */
	for(n = 0; ps_global->atmts && ps_global->atmts[n].description; n++)
	  ;

	if(n > 1){
	    i++;
	    VIEWPOPUP(&fmp_menu[i], 'V', "&View Attachment Index");
	}
    }

    i++;
    VIEWPOPUP(&fmp_menu[i], 'I', "Message &Index");

    i++;
    VIEWPOPUP(&fmp_menu[i], 'M', "&Main Menu");

    fmp_menu[++i].type = tTail;

    if((i = mswin_popup(fmp_menu)) >= 0 && in_handle)
      switch(fmp_menu[i].data.val){
	case 'W' :		/* Copy URL to clipboard */
	  mswin_addclipboard(h->h.url.path);
	  break;

	case 'X' :
	  return(1);		/* return like the user double-clicked */

	break;

	case 'Y' :		/* popup the thing in another window */
	  display_att_window(h->h.attach);
	  break;

	case 'Z' :
	  if(h && h->type == Attach){
	      msgno_exceptions(ps_global->mail_stream,
			       mn_m2raw(ps_global->msgmap,
					mn_get_cur(ps_global->msgmap)),
			       h->h.attach->number, &n, FALSE);
	      n ^= MSG_EX_DELETE;
	      msgno_exceptions(ps_global->mail_stream,
			       mn_m2raw(ps_global->msgmap,
					mn_get_cur(ps_global->msgmap)),
			       h->h.attach->number, &n, TRUE);
	      q_status_message2(SM_ORDER, 0, 3, "Attachment %s %s!",
				h->h.attach->number,
				(n & MSG_EX_DELETE) ? "deleted" : "undeleted");

	      return(2);
	  }

	  break;

	default :
	  break;
      }

    return(0);
}



/*
 * 
 */
int
simple_text_popup(sparms, in_handle)
    SCROLL_S *sparms;
    int	      in_handle;
{
    MPopup    simple_menu[12];
    int	      n = 0;

    VIEWPOPUP(&simple_menu[n], '%', "Print");
    n++;

    VIEWPOPUP(&simple_menu[n], 'S', "Save");
    n++;

    VIEWPOPUP(&simple_menu[n], 'F', "Forward in Email");
    n++;

    simple_menu[n++].type = tSeparator;

    VIEWPOPUP(&simple_menu[n], 'E', "Exit Viewer");
    n++;

    simple_menu[n].type = tTail;

    (void) mswin_popup(simple_menu);
    return(0);
}



/*----------------------------------------------------------------------
    Return characters in scroll tool buffer serially

   Args: n -- index of char to return

   Returns: returns the character at index 'n', or -1 on error or
	    end of buffer.

 ----*/
int
mswin_readscrollbuf(n)
    int n;
{
    SCRLCTRL_S	 *st = scroll_state(SS_CUR);
    int		  c;
    static char **orig = NULL, **l, *p;
    static int    lastn;

    if(!st)
      return(-1);

    /*
     * All of these are mind-numbingly slow at the moment...
     */
    switch(st->parms->text.src){
      case CharStar :
	return((n >= strlen((char *)st->parms->text.text))
	         ? -1 : ((char *)st->parms->text.text)[n]);

      case CharStarStar :
	/* BUG? is this test rigorous enough? */
	if(orig != (char **)st->parms->text.text || n < lastn){
	    lastn = n;
	    if(orig = l = (char **)st->parms->text.text) /* reset l and p */
	      p = *l;
	}
	else{				/* use cached l and p */
	    c = n;			/* and adjust n */
	    n -= lastn;
	    lastn = c;
	}

	while(l){			/* look for 'n' on each line  */
	    for(; n && *p; n--, p++)
	      ;

	    if(n--)			/* 'n' found ? */
	      p = *++l;
	    else
	      break;
	}

	return((l && *l) ? *p ? *p : '\n' : -1);

      case FileStar :
	return((fseek((FILE *)st->parms->text.text, (long) n, 0) < 0
		|| (c = fgetc((FILE *)st->parms->text.text)) == EOF) ? -1 : c);

      default:
	return(-1);
    }
}



/*----------------------------------------------------------------------
     MSWin scroll callback.  Called during scroll message processing.
	     


  Args: cmd - what type of scroll operation.
	scroll_pos - paramter for operation.  
			used as position for SCROLL_TO operation.

  Returns: TRUE - did the scroll operation.
	   FALSE - was not able to do the scroll operation.
 ----*/
int
pcpine_do_scroll (cmd, scroll_pos)
int	cmd;
long	scroll_pos;
{
    SCRLCTRL_S   *st = scroll_state(SS_CUR);
    HANDLE_S	 *next_handle;
    int		  paint = FALSE;
    int		  num_display_lines;
    int		  scroll_lines;
    char	  message[64];
    long	  maxscroll;
    
	
    message[0] = '\0';
    maxscroll = st->num_lines;
    switch (cmd) {
    case MSWIN_KEY_SCROLLUPLINE:
	if(st->top_text_line > 0) {
	    st->top_text_line -= (int) scroll_pos;
	    paint = TRUE;
	    if (st->top_text_line <= 0){
		snprintf(message, sizeof(message), "START of %.*s",
			32, STYLE_NAME(st->parms));
		message[sizeof(message)-1] = '\0';
		st->top_text_line = 0;
	    }
        }
	break;

    case MSWIN_KEY_SCROLLDOWNLINE:
        if(st->top_text_line < maxscroll) {
	    st->top_text_line += (int) scroll_pos;
	    paint = TRUE;
	    if (st->top_text_line >= maxscroll){
		snprintf(message, sizeof(message), "END of %.*s", 32, STYLE_NAME(st->parms));
		message[sizeof(message)-1] = '\0';
		st->top_text_line = maxscroll;
	    }
        }
	break;
		
    case MSWIN_KEY_SCROLLUPPAGE:
	if(st->top_text_line > 0) {
	    num_display_lines = SCROLL_LINES(ps_global);
	    scroll_lines = MIN(MAX(num_display_lines -
			ps_global->viewer_overlap, 1), num_display_lines);
	    if (st->top_text_line > scroll_lines)
		st->top_text_line -= scroll_lines;
	    else {
		st->top_text_line = 0;
		snprintf(message, sizeof(message), "START of %.*s", 32, STYLE_NAME(st->parms));
		message[sizeof(message)-1] = '\0';
	    }
	    paint = TRUE;
        }
	break;
	    
    case MSWIN_KEY_SCROLLDOWNPAGE:
	num_display_lines = SCROLL_LINES(ps_global);
	if(st->top_text_line  < maxscroll) {
	    scroll_lines = MIN(MAX(num_display_lines -
			ps_global->viewer_overlap, 1), num_display_lines);
	    st->top_text_line += scroll_lines;
	    if (st->top_text_line >= maxscroll) {
		st->top_text_line = maxscroll;
		snprintf(message, sizeof(message), "END of %.*s", 32, STYLE_NAME(st->parms));
		message[sizeof(message)-1] = '\0';
	    }
	    paint = TRUE;
	}
	break;
	    
    case MSWIN_KEY_SCROLLTO:
	if (st->top_text_line != scroll_pos) {
	    st->top_text_line = scroll_pos;
	    if (st->top_text_line == 0)
		snprintf(message, sizeof(message), "START of %.*s", 32, STYLE_NAME(st->parms));
	    else if(st->top_text_line >= maxscroll) 
		snprintf(message, sizeof(message), "END of %.*s", 32, STYLE_NAME(st->parms));

	    message[sizeof(message)-1] = '\0';
	    paint = TRUE;
        }
	break;
    }

    /* Need to frame some handles? */
    if(st->parms->text.handles
       && (next_handle = scroll_handle_in_frame(st->top_text_line)))
      st->parms->text.handles = next_handle;

    if (paint) {
	mswin_beginupdate();
	update_scroll_titlebar(st->top_text_line, 0);
	(void) scroll_scroll_text(st->top_text_line,
				  st->parms->text.handles, 1);
	if (message[0])
	  q_status_message(SM_INFO, 0, 1, message);

	/* Display is always called so that the "START(END) of message" 
	 * message gets removed when no longer at the start(end). */
	display_message (KEY_PGDN);
	mswin_endupdate();
    }

    return (TRUE);
}


char *
pcpine_help_scroll(title)
    char *title;
{
    SCRLCTRL_S   *st = scroll_state(SS_CUR);

    if(title)
      strncpy(title, (st->parms->help.title)
		      ? st->parms->help.title : "Alpine Help", 256);

    return(pcpine_help(st->parms->help.text));
}


int
pcpine_view_cursor(col, row)
    int  col;
    long row;
{
    SCRLCTRL_S   *st = scroll_state(SS_CUR);
    int		  key;
    long	  line;

    return((row >= HEADER_ROWS(ps_global)
	    && row < HEADER_ROWS(ps_global) + SCROLL_LINES(ps_global)
	    && (line = (row - 2) + st->top_text_line) < st->num_lines
	    && (key = dot_on_handle(line, col))
	    && scroll_handle_selectable(get_handle(st->parms->text.handles,key)))
	     ? MSWIN_CURSOR_HAND
	     : MSWIN_CURSOR_ARROW);
}
#endif /* _WINDOWS */
