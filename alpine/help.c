#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: help.c 1032 2008-04-11 00:30:04Z hubert@u.washington.edu $";
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
#include "help.h"
#include "keymenu.h"
#include "status.h"
#include "mailview.h"
#include "mailindx.h"
#include "mailcmd.h"
#include "reply.h"
#include "signal.h"
#include "radio.h"
#include "send.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/filter.h"
#include "../pith/msgno.h"
#include "../pith/pipe.h"
#include "../pith/util.h"
#include "../pith/detoken.h"
#include "../pith/list.h"
#include "../pith/margin.h"


typedef struct _help_scroll {
    unsigned	keys_formatted:1;	/* Has full keymenu been formatted? */
    char      **help_source;		/* Source of displayed help text */
} HELP_SCROLL_S;


static struct {
    unsigned   crlf:1;
    char     **line,
	      *offset;
} g_h_text;


typedef struct _help_print_state {
    int	  page;
    char *title;
    int   title_len;
} HPRT_S;

static HPRT_S *g_hprt;


static char att_cur_msg[] = "\
         Reporting a bug...\n\
\n\
  If you think that the \"current\" message may be related to the bug you\n\
  are reporting you may include it as an attachment.  If you want to\n\
  include a message but you aren't sure if it is the current message,\n\
  cancel this bug report, go to the folder index, place the cursor on\n\
  the message you wish to include, then return to the main menu and run\n\
  the bug report command again.  Answer \"Y\" when asked the question\n\
  \"Attach current message to report?\"\n\
\n\
  This bug report will also automatically include your pine\n\
  configuration file, which is helpful when investigating the problem.";


#define	GRIPE_OPT_CONF	0x01
#define	GRIPE_OPT_MSG	0x02
#define	GRIPE_OPT_LOCAL	0x04
#define	GRIPE_OPT_KEYS	0x08


/*
 * Internal prototypes
 */
int	 helper_internal(HelpType, char *, char *, int);
int	 help_processor(int, MSGNO_S *, SCROLL_S *);
void	 help_keymenu_tweek(SCROLL_S *, int);
void     print_all_help(void);
void	 print_help_page_title(char *, size_t, HPRT_S *);
int	 print_help_page_break(long, char *, LT_INS_S **, void *);
int	 help_bogus_input(UCS);
int	 gripe_newbody(struct pine *, BODY **, long, int);
ADDRESS *gripe_token_addr(char *);
char	*gripe_id(char *);
void	 att_cur_drawer(void);
int	 journal_processor(int, MSGNO_S *, SCROLL_S *);
int	 help_popup(SCROLL_S *, int);
#ifdef	_WINDOWS
int	 help_subsection_popup(SCROLL_S *, int);
#endif



/*----------------------------------------------------------------------
     Get the help text in the proper format and call scroller

    Args: text   -- The help text to display (from pine.help --> helptext.c)
          title  -- The title of the help text 

  Result: format text and call scroller

  The pages array contains the line number of the start of the pages in
the text. Page 1 is in the 0th element of the array.
The list is ended with a page line number of -1. Line number 0 is also
the first line in the text.
  -----*/
int
helper_internal(HelpType text, char *frag, char *title, int flags)
{
    char	  **shown_text;
    int		    cmd = MC_NONE;
    long	    offset = 0L;
    char	   *error = NULL, tmp_title[MAX_SCREEN_COLS + 1];
    STORE_S	   *store;
    HANDLE_S	   *handles = NULL, *htmp;
    HELP_SCROLL_S   hscroll;
    gf_io_t	    pc;

    dprint((1, "\n\n    ---- HELPER ----\n"));

    /* assumption here is that HelpType is char **  */
    shown_text = text;

    if(F_ON(F_BLANK_KEYMENU,ps_global)){
	FOOTER_ROWS(ps_global) = 3;
	clearfooter(ps_global);
	ps_global->mangled_screen = 1;
    }

    if(flags & HLPD_NEWWIN){
 	fix_windsize(ps_global);
	init_sigwinch();
    }

    /*
     * At this point, shown_text is a charstarstar with html
     * Turn it into a charstar with digested html
     */
    do{
	init_helper_getc(shown_text);
	init_handles(&handles);

	memset(&hscroll, 0, sizeof(HELP_SCROLL_S));
	hscroll.help_source = shown_text;
	if((store = so_get(CharStar, NULL, EDIT_ACCESS)) != NULL){
	    gf_set_so_writec(&pc, store);
	    gf_filter_init();

	    if(!struncmp(shown_text[0], "<html>", 6))
	      gf_link_filter(gf_html2plain,
			     gf_html2plain_opt("x-alpine-help:",
					       ps_global->ttyo->screen_cols,
					       non_messageview_margin(),
					       &handles, NULL, GFHP_LOCAL_HANDLES));
	    else
	      gf_link_filter(gf_wrap, gf_wrap_filter_opt(
						  ps_global->ttyo->screen_cols,
						  ps_global->ttyo->screen_cols,
						  NULL, 0, GFW_HANDLES | GFW_SOFTHYPHEN));

	    error = gf_pipe(helper_getc, pc);

	    gf_clear_so_writec(store);

	    if(!error){
		SCROLL_S	sargs;
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
		sargs.text.desc	   = _("help text");
		if((sargs.text.handles = handles) != NULL)
		  while(sargs.text.handles->type == URL
			&& !sargs.text.handles->h.url.path
			&& sargs.text.handles->next)
		    sargs.text.handles = sargs.text.handles->next;

		if(!(sargs.bar.title = title)){
		    if(!struncmp(shown_text[0], "<html>", 6)){
			char *p;
			int   i;

			/* if we're looking at html, look for a <title>
			 * in the <head>... */
			for(i = 1;
			    shown_text[i]
			      && struncmp(shown_text[i], "</head>", 7);
			    i++)
			  if(!struncmp(shown_text[i], "<title>", 7)){
			      strncpy(tmp_20k_buf, &shown_text[i][7], SIZEOF_20KBUF);
			      tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
			      if((p = strchr(tmp_20k_buf, '<')) != NULL)
				*p = '\0';

			      snprintf(sargs.bar.title = tmp_title, sizeof(tmp_title),
				      "%s -- %.*s", _("HELP"),
				      ps_global->ttyo->screen_cols-10,
				      strsquish(tmp_20k_buf +  500, SIZEOF_20KBUF,
					_(tmp_20k_buf),
					ps_global->ttyo->screen_cols / 3));
			      tmp_title[sizeof(tmp_title)-1] = '\0';
			      break;
			  }
		    }

		    if(!sargs.bar.title)
		      sargs.bar.title = _("HELP TEXT");
		}

		sargs.bar.style	   = TextPercent;
		sargs.proc.tool	   = help_processor;
		sargs.proc.data.p  = (void *) &hscroll;
		sargs.resize_exit  = 1;
		sargs.help.text	   = h_special_help_nav;
		sargs.help.title   = _("HELP FOR HELP TEXT");
		sargs.keys.menu	   = &km;
		km		   = help_keymenu;
		km.keys		   = keys;
		memcpy(&keys[0], help_keymenu.keys,
		       (help_keymenu.how_many * 12) * sizeof(struct key));
		setbitmap(sargs.keys.bitmap);
		if(flags & HLPD_FROMHELP){
		    km.keys[HLP_EXIT_KEY].name	     = "P";
		    km.keys[HLP_EXIT_KEY].label	     = _("Prev Help");
		    km.keys[HLP_EXIT_KEY].bind.cmd   = MC_FINISH;
		    km.keys[HLP_EXIT_KEY].bind.ch[0] = 'p';

		    km.keys[HLP_SUBEXIT_KEY].name	= "E";
		    km.keys[HLP_SUBEXIT_KEY].label	= _("Exit Help");
		    km.keys[HLP_SUBEXIT_KEY].bind.cmd   = MC_EXIT;
		    km.keys[HLP_SUBEXIT_KEY].bind.ch[0] = 'e';
		}
		else if(text == h_special_help_nav){
		    km.keys[HLP_EXIT_KEY].name	     = "P";
		    km.keys[HLP_EXIT_KEY].label	     = _("Prev Help");
		    km.keys[HLP_EXIT_KEY].bind.cmd   = MC_FINISH;
		    km.keys[HLP_EXIT_KEY].bind.ch[0] = 'p';

		    clrbitn(HLP_MAIN_KEY, sargs.keys.bitmap);
		    clrbitn(HLP_SUBEXIT_KEY, sargs.keys.bitmap);
		}
		else{
		    km.keys[HLP_EXIT_KEY].name	     = "E";
		    km.keys[HLP_EXIT_KEY].label	     = _("Exit Help");
		    km.keys[HLP_EXIT_KEY].bind.cmd   = MC_EXIT;
		    km.keys[HLP_EXIT_KEY].bind.ch[0] = 'e';

		    km.keys[HLP_SUBEXIT_KEY].name	= "?";
		    /* TRANSLATORS: this is the label of a command where
		       the user is asking for Help about the Help command */
		    km.keys[HLP_SUBEXIT_KEY].label	= _("Help Help");
		    km.keys[HLP_SUBEXIT_KEY].bind.cmd   = MC_HELP;
		    km.keys[HLP_SUBEXIT_KEY].bind.ch[0] = '?';
		}

		if(flags & HLPD_SIMPLE){
		    clrbitn(HLP_MAIN_KEY, sargs.keys.bitmap);
		}
		else
		  sargs.bogus_input = help_bogus_input;

		if(handles){
		    sargs.keys.each_cmd = help_keymenu_tweek;
		    hscroll.keys_formatted = 0;
		}
		else{
		    clrbitn(HLP_VIEW_HANDLE, sargs.keys.bitmap);
		    clrbitn(HLP_PREV_HANDLE, sargs.keys.bitmap);
		    clrbitn(HLP_NEXT_HANDLE, sargs.keys.bitmap);
		}

		if(text != main_menu_tx
		   && text != h_mainhelp_pinehelp)
		  clrbitn(HLP_ALL_KEY, sargs.keys.bitmap);

		if(frag){
		    sargs.start.on	 = Fragment;
		    sargs.start.loc.frag = frag;
		    frag		 = NULL; /* ignore next time */
		}
		else if(offset){
		    sargs.start.on	   = Offset;
		    sargs.start.loc.offset = offset;
		}
		else
		  sargs.start.on = FirstPage;

#ifdef	_WINDOWS
		sargs.mouse.popup = (flags & HLPD_FROMHELP)
				      ? help_subsection_popup : help_popup;
#endif

		cmd = scrolltool(&sargs);

		offset = sargs.start.loc.offset;

		if(F_ON(F_BLANK_KEYMENU,ps_global))
		  FOOTER_ROWS(ps_global) = 1;

		ClearScreen();
	    }

	    so_give(&store);
	}

	free_handles(&handles);
    }
    while(cmd == MC_RESIZE);

    return(cmd);
}


/*
 * helper -- compatibility function around newer helper_internal
 */
int
helper(HelpType text, char *title, int flags)
{
    return(helper_internal(text, NULL, title, flags));
}


void
init_helper_getc(char **help_txt)
{
    g_h_text.crlf   = 0;
    g_h_text.line   = help_txt;
    g_h_text.offset = *g_h_text.line;
    if(g_h_text.offset && g_h_text.offset[0])
      g_h_text.offset = _(g_h_text.offset);
}


int
helper_getc(char *c)
{
    if(g_h_text.crlf){
	*c = '\012';
	g_h_text.crlf = 0;
	return(1);
    }
    else if(g_h_text.offset && *g_h_text.line){
	if(!(*c = *g_h_text.offset++)){
	    g_h_text.offset = *++g_h_text.line;
	    if(g_h_text.offset && g_h_text.offset[0])
	      g_h_text.offset = _(g_h_text.offset);

	    *c = '\015';
	    g_h_text.crlf = 1;
	}

	return(1);
    }

    return(0);
}


int
help_processor(int cmd, MSGNO_S *msgmap, SCROLL_S *sparms)
{
    int rv = 0;
    char message[64];

    switch(cmd){
	/*----------- Print all the help ------------*/
      case MC_PRINTALL :
	print_all_help();
	break;

      case MC_PRINTMSG :
	snprintf(message, sizeof(message), "%s", STYLE_NAME(sparms));
	message[sizeof(message)-1] = '\0';
	if(open_printer(message) == 0){
	    print_help(((HELP_SCROLL_S *)sparms->proc.data.p)->help_source);
	    close_printer();
	}

	break;

      case MC_FINISH :
	rv = 1;
	break;

      default :
	panic("Unhandled case");
    }

    return(rv);
}


void
help_keymenu_tweek(SCROLL_S *sparms, int handle_hidden)
{
    if(handle_hidden){
	sparms->keys.menu->keys[HLP_VIEW_HANDLE].name  = "";
	sparms->keys.menu->keys[HLP_VIEW_HANDLE].label = "";
    }
    else{
	if(!((HELP_SCROLL_S *)sparms->proc.data.p)->keys_formatted){
	    /* If label's always been blank, force reformatting */
	    mark_keymenu_dirty();
	    sparms->keys.menu->width = 0;
	    ((HELP_SCROLL_S *)sparms->proc.data.p)->keys_formatted = 1;
	}

	sparms->keys.menu->keys[HLP_VIEW_HANDLE].name  = "V";
	sparms->keys.menu->keys[HLP_VIEW_HANDLE].label = "[" N_("View Link") "]";
    }
}


/*
 * print_help - send the raw array of lines to printer
 */
void
print_help(char **text)
{
    char   *error, buf[256];
    HPRT_S  help_data;

    init_helper_getc(text);

    memset(g_hprt = &help_data, 0, sizeof(HPRT_S));

    help_data.page = 1;
    
    gf_filter_init();

    if(!struncmp(text[0], "<html>", 6)){
	int   i;
	char *p;

	gf_link_filter(gf_html2plain,
		       gf_html2plain_opt(NULL,80,non_messageview_margin(),
					 NULL,NULL,GFHP_STRIPPED));
	for(i = 1; i <= 5 && text[i]; i++)
	  if(!struncmp(text[i], "<title>", 7)
	     && (p = srchstr(text[i] + 7, "</title>"))
	     && p - text[i] > 7){
	      help_data.title	  = text[i] + 7;
	      help_data.title_len = p - help_data.title;
	      break;
	  }
    }
    else
      gf_link_filter(gf_wrap, gf_wrap_filter_opt(80, 80, NULL, 0, GFW_NONE));

    gf_link_filter(gf_line_test,
		   gf_line_test_opt(print_help_page_break, NULL));
    gf_link_filter(gf_nvtnl_local, NULL);

    print_help_page_title(buf, sizeof(buf), &help_data);
    print_text(buf);
    print_text(NEWLINE);		/* terminate it */
    print_text(NEWLINE);		/* and write two blank links */
    print_text(NEWLINE);

    if((error = gf_pipe(helper_getc, print_char)) != NULL)
      q_status_message1(SM_ORDER | SM_DING, 3, 3, _("Printing Error: %s"), error);

    print_char(ctrl('L'));		/* new page. */
}


void
print_all_help(void)
{
    struct help_texts *t;
    char **h;
    int we_turned_on = 0;

    if(open_printer(_("all 150+ pages of help text")) == 0) {
	we_turned_on = intr_handling_on();
	for(t = h_texts; (h = t->help_text) != NO_HELP; t++) {
	    if(ps_global->intr_pending){
		q_status_message(SM_ORDER, 3, 3,
				 _("Print of all help cancelled"));
		break;
	    }

	    print_help(h);
        }

	if(we_turned_on)
	  intr_handling_off();

        close_printer();
    }
}


/*
 * print_help_page_title -- 
 */
void
print_help_page_title(char *buf, size_t buflen, HPRT_S *hprt)
{
    snprintf(buf, buflen, "  Alpine Help%s%.*s%*s%d",
	    hprt->title_len ? ": " : " Text",
	    MIN(55, hprt->title_len), hprt->title_len ? hprt->title : "",
	    59 - (hprt->title_len ? MIN(55, hprt->title_len) : 5),
	    "Page ", hprt->page);
    buf[buflen-1] = '\0';
}


/*
 * print_help_page_break -- insert page breaks and such for printed
 *			    help text
 */
int
print_help_page_break(long int linenum, char *line, LT_INS_S **ins, void *local)
{
    char buf[256];

    if(((linenum + (g_hprt->page * 3)) % 62) == 0){
	g_hprt->page++;			/* start on new page */
	buf[0] = ctrl('L');
	print_help_page_title(buf + 1, sizeof(buf)-1, g_hprt);
	strncat(buf, "\015\012\015\012\015\012", sizeof(buf)-strlen(buf)-1);
	buf[sizeof(buf)-1] = '\0';
	ins = gf_line_test_new_ins(ins, line, buf, strlen(buf));
    }

    return(0);
}


/*
 * help_bogus_input - used by scrolltool to complain about
 *		      invalid user input.
 */
int
help_bogus_input(UCS ch)
{
    bogus_command(ch, NULL);
    return(0);
}


int
url_local_helper(char *url)
{
    if(!struncmp(url, "x-alpine-help:", 14) && *(url += 14)){
	char		   *frag;
	HelpType	    newhelp;

	/* internal fragment reference? */
	if((frag = strchr(url, '#')) != NULL){
	    size_t len;

	    if((len = frag - url) != 0){
		newhelp = help_name2section(url, len);
	    }
	    else{
		url_local_fragment(url);
		return(1);
	    }
	}
	else
	  newhelp = help_name2section(url, strlen(url));


	if(newhelp != NO_HELP){
	    int rv;

	    rv = helper_internal(newhelp, frag, _("HELP SUB-SECTION"),
				 HLPD_NEWWIN | HLPD_SIMPLE | HLPD_FROMHELP);
	    ps_global->mangled_screen = 1;
	    return((rv == MC_EXIT) ? 2 : 1);
	}
    }

    q_status_message1(SM_ORDER | SM_DING, 0, 3,
		      _("Unrecognized Internal help: \"%s\""), url);
    return(0);
}


int
url_local_config(char *url)
{
    if(!struncmp(url, "x-alpine-config:", 16)){
	char **config;
	int    rv;

	config = get_supported_options();
	if(config){
	    /* TRANSLATORS: Help for configuration */
	    rv = helper_internal(config, NULL, _("HELP CONFIG"),
				 HLPD_NEWWIN | HLPD_SIMPLE | HLPD_FROMHELP);
	    free_list_array(&config);
	}

	ps_global->mangled_screen = 1;
	return((rv == MC_EXIT) ? 2 : 1);
    }

    q_status_message1(SM_ORDER | SM_DING, 0, 3,
		      _("Unrecognized Internal help: \"%s\""), url);
    return(0);
}


/*----------------------------------------------------------------------
     Review latest status messages
  -----*/
void
review_messages(void)
{
    SCROLL_S	    sargs;
    STORE_S        *in_store = NULL, *out_store = NULL;
    gf_io_t         gc, pc;
    int             jo, lo, hi, donejo, donelo, donehi;
    RMCat           rmcat;
    int             cmd, timestamps=0, show_level=-1;
    char            debugkeylabel[20];
    /* TRANSLATORS: command label asking pine to include time stamps in output */
    char            timestampkeylabel[] = N_("Timestamps");
    /* TRANSLATORS: do not include time stamps in output */
    char           *notimestampkeylabel = N_("NoTimestamps");

    if((rmjofirst < 0 && rmlofirst < 0 && rmhifirst < 0)
       || rmjofirst >= RMJLEN || rmjolast >= RMJLEN
       || rmlofirst >= RMLLEN || rmlolast >= RMLLEN
       || rmhifirst >= RMHLEN || rmhilast >= RMHLEN
       || (rmjofirst >= 0 && rmjolast < 0)
       || (rmlofirst >= 0 && rmlolast < 0)
       || (rmhifirst >= 0 && rmhilast < 0))
      return;
    
    do{

	if(!(in_store = so_get(CharStar, NULL, EDIT_ACCESS)) ||
	   !(out_store = so_get(CharStar, NULL, EDIT_ACCESS))){
	    if(in_store)
	      so_give(&in_store);

	    q_status_message(SM_ORDER | SM_DING, 3, 4,
			     _("Failed allocating memory"));
	    return;
	}
	
	add_review_message(_("Turning off new messages while reviewing"), 0);
	rm_not_right_now = 1;

	donejo = donehi = donelo = 0;
	jo = rmjofirst;
	if(jo < 0)
	  donejo = 1;

	lo = rmlofirst;
	if(lo < 0)
	  donelo = 1;

	hi = rmhifirst;
	if(hi < 0)
	  donehi = 1;

	while(!(donejo && donelo && donehi)){
	    REV_MSG_S *pjo, *plo, *phi, *p;

	    if(!donejo)
	      pjo = &rmjoarray[jo];
	    else
	      pjo = NULL;

	    if(!donelo)
	      plo = &rmloarray[lo];
	    else
	      plo = NULL;

	    if(!donehi)
	      phi = &rmhiarray[hi];
	    else
	      phi = NULL;

	    if(pjo && (!plo || pjo->seq <= plo->seq)
	       && (!phi || pjo->seq <= phi->seq))
	      rmcat = Jo;
	    else if(plo && (!phi || plo->seq <= phi->seq))
	      rmcat = Lo;
	    else if(phi)
	      rmcat = Hi;
	    else
	      rmcat = No;

	    switch(rmcat){
	      case Jo:
		p = pjo;
		if(jo == rmjofirst && (((rmjolast + 1) % RMJLEN) == rmjofirst))
		  so_puts(in_store,
	_("**** Journal entries prior to this point have been trimmed. ****\n"));
		break;

	      case Lo:
		p = plo;
		if(show_level >= 0 &&
		   lo == rmlofirst && (((rmlolast + 1) % RMLLEN) == rmlofirst))
		  so_puts(in_store,
	_("**** Debug 0-4 entries prior to this point have been trimmed. ****\n"));
		break;

	      case Hi:
		p = phi;
		if(show_level >= 5 &&
		   hi == rmhifirst && (((rmhilast + 1) % RMHLEN) == rmhifirst))
		  so_puts(in_store,
	_("**** Debug 5-9 entries prior to this point have been trimmed. ****\n"));
		break;

	      default:
		p = NULL;
		break;
	    }

	    if(p){
		if(p->level <= show_level){
		    if(timestamps && p->timestamp && p->timestamp[0]){
			so_puts(in_store, p->timestamp);
			so_puts(in_store, ": ");
		    }

		    if(p->message && p->message[0]){
			if(p->continuation)
			  so_puts(in_store, ">");

			so_puts(in_store, p->message);
			so_puts(in_store, "\n");
		    }
		}
	    }

	    switch(rmcat){
	      case Jo:
		if(jo == rmjolast)
		  donejo++;
		else
		  jo = (jo + 1) % RMJLEN;

		break;

	      case Lo:
		if(lo == rmlolast)
		  donelo++;
		else
		  lo = (lo + 1) % RMLLEN;

		break;

	      case Hi:
		if(hi == rmhilast)
		  donehi++;
		else
		  hi = (hi + 1) % RMHLEN;

		break;

	      default:
		donejo++;
		donelo++;
		donehi++;
		break;
	    }
	}


	so_seek(in_store, 0L, 0);
	gf_filter_init();
	gf_link_filter(gf_wrap,
		       gf_wrap_filter_opt(ps_global->ttyo->screen_cols - 4,
					  ps_global->ttyo->screen_cols,
					  NULL, show_level < 0 ? 2 : 0, GFW_NONE));
	gf_set_so_readc(&gc, in_store);
	gf_set_so_writec(&pc, out_store);
	gf_pipe(gc, pc);
	gf_clear_so_writec(out_store);
	gf_clear_so_readc(in_store);

	memset(&sargs, 0, sizeof(SCROLL_S));
	sargs.text.text     = so_text(out_store);
	sargs.text.src      = CharStar;
	sargs.text.desc     = _("journal");
	sargs.keys.menu     = &rev_msg_keymenu;
	sargs.proc.tool     = journal_processor;
	sargs.start.on      = LastPage;
	sargs.resize_exit   = 1;
	sargs.proc.data.p   = (void *)&show_level;
	setbitmap(sargs.keys.bitmap);

#ifdef DEBUG
#ifdef DEBUGJOURNAL
	sargs.jump_is_debug = 1;
	/* TRANSLATORS: these are some screen titles */
	sargs.help.title    = _("HELP FOR DEBUG JOURNAL");
	sargs.help.text     = h_debugjournal;
	sargs.bar.title     = _("REVIEW DEBUGGING");
#else	/* !DEBUGJOURNAL */
	clrbitn(DEBUG_KEY, sargs.keys.bitmap);
	sargs.help.title    = _("HELP FOR JOURNAL");
	sargs.help.text     = h_journal;
	sargs.bar.title     = _("REVIEW RECENT MESSAGES");
#endif	/* !DEBUGJOURNAL */
#else	/* !DEBUG */
	clrbitn(DEBUG_KEY, sargs.keys.bitmap);
	clrbitn(TIMESTAMP_KEY, sargs.keys.bitmap);
	sargs.help.title    = _("HELP FOR JOURNAL");
	sargs.help.text     = h_journal;
	sargs.bar.title     = _("REVIEW RECENT MESSAGES");
#endif	/* !DEBUG */

	if(timestamps)
	  rev_msg_keys[TIMESTAMP_KEY].label = notimestampkeylabel;
	else
	  rev_msg_keys[TIMESTAMP_KEY].label = timestampkeylabel;

	if(show_level >= 0)
	  /* TRANSLATORS: shows what numeric level Debug output is displayed at */
	  snprintf(debugkeylabel, sizeof(debugkeylabel), _("Debug (%d)"), show_level);
	else
	  /* TRANSLATORS: include debug output in journal */
	  strncpy(debugkeylabel, _("DebugView"), sizeof(debugkeylabel));

	debugkeylabel[sizeof(debugkeylabel)-1] = '\0';

	rev_msg_keys[DEBUG_KEY].label = debugkeylabel;
	KS_OSDATASET(&rev_msg_keys[DEBUG_KEY], KS_NONE);

	if((cmd = scrolltool(&sargs)) == MC_TOGGLE)
	  timestamps = !timestamps;

	so_give(&in_store);
	so_give(&out_store);
	
    }while(cmd != MC_EXIT);

    rm_not_right_now = 0;
    add_review_message("Done reviewing", 0);
}


int
journal_processor(int cmd, MSGNO_S *msgmap, SCROLL_S *sparms)
{
    switch(cmd){
      case MC_TOGGLE:		/* turn timestamps on or off */
        break;

      default:
        panic("Unexpected command in journal_processor");
	break;
    }
    
    return(1);
}


/*
 * standard type of storage object used for body parts...
 */
#ifdef	DOS
#define		  PART_SO_TYPE	TmpFileStar
#else
#define		  PART_SO_TYPE	CharStar
#endif


int
gripe_gripe_to(url)
    char *url;
{
    char      *composer_title, *url_copy, *optstr, *p;
    int	       opts = 0;
    BODY      *body = NULL;
    ENVELOPE  *outgoing = NULL;
    REPLY_S    fake_reply;
    PINEFIELD *pf = NULL;
    long       msgno = mn_m2raw(ps_global->msgmap, 
				mn_get_cur(ps_global->msgmap));

    url_copy = cpystr(url + strlen("x-alpine-gripe:"));
    if((optstr = strchr(url_copy, '?')) != NULL)
      *optstr++ = '\0';

    outgoing		 = mail_newenvelope();
    outgoing->message_id = generate_message_id();

    if((outgoing->to = gripe_token_addr(url_copy)) != NULL){
	composer_title = _("COMPOSE TO LOCAL SUPPORT");
	dprint((1, 
		   "\n\n   -- Send to local support(%s@%s) --\n",
		   outgoing->to->mailbox ? outgoing->to->mailbox : "NULL",
		   outgoing->to->host ? outgoing->to->host : "NULL"));
    }
    else{			/* must be global */
	composer_title = _("REQUEST FOR ASSISTANCE");
	rfc822_parse_adrlist(&outgoing->to, url_copy, ps_global->maildomain);
    }

    /*
     * Sniff thru options
     */
    while(optstr){
	if((p = strchr(optstr, '?')) != NULL)	/* tie off list item */
	  *p++ = '\0';

	if(!strucmp(optstr, "config"))
	  opts |= GRIPE_OPT_CONF;
	else if(!strucmp(optstr, "curmsg"))
	  opts |= GRIPE_OPT_MSG;
	else if(!strucmp(optstr, "local"))
	  opts |= GRIPE_OPT_LOCAL;
	else if(!strucmp(optstr, "keys"))
	  opts |= GRIPE_OPT_KEYS;

	optstr = p;
    }

    /* build body and hand off to composer... */
    if(gripe_newbody(ps_global, &body, msgno, opts) == 0){
	pf = (PINEFIELD *) fs_get(sizeof(PINEFIELD));
	memset(pf, 0, sizeof(PINEFIELD));
	pf->name		   = cpystr("X-Generated-Via");
	pf->type		   = FreeText;
	pf->textbuf		   = gripe_id("Alpine Bug Report screen");
	memset((void *)&fake_reply, 0, sizeof(fake_reply));
	fake_reply.pseudo	   = 1;
	fake_reply.data.pico_flags = P_HEADEND;
	pine_send(outgoing, &body, composer_title, NULL, NULL,
		  &fake_reply, NULL, NULL, pf, PS_STICKY_TO);
    }
    
    ps_global->mangled_screen = 1;
    mail_free_envelope(&outgoing);

    if(body)
      pine_free_body(&body);

    fs_give((void **) &url_copy);
    
    return(10);
}


int
gripe_newbody(ps, body, msgno, flags)
    struct pine *ps;
    BODY       **body;
    long         msgno;
    int          flags;
{
    BODY        *pb;
    PART       **pp;
    STORE_S	*store;
    gf_io_t      pc;
    static char *err = "Problem creating space for message text.";
    int          i;
    char         tmp[MAILTMPLEN], *p;

    if((store = so_get(PicoText, NULL, EDIT_ACCESS)) != NULL){
	*body = mail_newbody();

	if((p = detoken(NULL, NULL, 2, 0, 1, NULL, NULL)) != NULL){
	    if(*p)
	      so_puts(store, p);

	    fs_give((void **) &p);
	}
    }
    else{
	q_status_message(SM_ORDER | SM_DING, 3, 4, err);
	return(-1);
    }

    if(flags){
	/*---- Might have multiple parts ----*/
	(*body)->type			= TYPEMULTIPART;
	/*---- The TEXT part/body ----*/
	(*body)->nested.part            = mail_newbody_part();
	(*body)->nested.part->body.type = TYPETEXT;
	(*body)->nested.part->body.contents.text.data = (void *) store;

	/*---- create object, and write current config into it ----*/
	pp = &((*body)->nested.part->next);

	if(flags & GRIPE_OPT_CONF){
	    *pp			     = mail_newbody_part();
	    pb			     = &((*pp)->body);
	    pp			     = &((*pp)->next);
	    pb->type		     = TYPETEXT;
	    pb->id		     = generate_message_id();
	    pb->description	     = cpystr("Alpine Configuration Data");
	    pb->parameter	     = mail_newbody_parameter();
	    pb->parameter->attribute = cpystr("name");
	    pb->parameter->value     = cpystr("config.txt");

	    if((store = so_get(CharStar, NULL, EDIT_ACCESS)) != NULL){
		extern char datestamp[], hoststamp[];

		pb->contents.text.data = (void *) store;
		gf_set_so_writec(&pc, store);
		gf_puts("Alpine built ", pc);
		gf_puts(datestamp, pc);
		gf_puts(" on host: ", pc);
		gf_puts(hoststamp, pc);
		gf_puts("\n", pc);

#ifdef DEBUG
		dump_pine_struct(ps, pc);
		dump_config(ps, pc, 0);
#endif /* DEBUG */

		pb->size.bytes = strlen((char *) so_text(store));
		gf_clear_so_writec(store);
	    }
	    else{
		q_status_message(SM_ORDER | SM_DING, 3, 4, err);
		return(-1);
	    }
	}

	if(flags & GRIPE_OPT_KEYS){
	    *pp			     = mail_newbody_part();
	    pb			     = &((*pp)->body);
	    pp			     = &((*pp)->next);
	    pb->type		     = TYPETEXT;
	    pb->id		     = generate_message_id();
	    pb->description	     = cpystr("Recent User Input");
	    pb->parameter	      = mail_newbody_parameter();
	    pb->parameter->attribute  = cpystr("name");
	    pb->parameter->value      = cpystr("uinput.txt");

	    if((store = so_get(CharStar, NULL, EDIT_ACCESS)) != NULL){
		pb->contents.text.data = (void *) store;

		so_puts(store, "User's most recent input:\n");

		/* dump last n keystrokes */
		so_puts(store, "========== Latest keystrokes ==========\n");
		while((i = key_playback(0)) != -1){
		    snprintf(tmp, sizeof(tmp), "\t%s\t(0x%x)\n", pretty_command(i), i);
		    tmp[sizeof(tmp)-1] = '\0';
		    so_puts(store, tmp);
		}

		pb->size.bytes = strlen((char *) so_text(store));
	    }
	    else{
		q_status_message(SM_ORDER | SM_DING, 3, 4, err);
		return(-1);
	    }
	}

	/* check for local debugging info? */
	if((flags & GRIPE_OPT_LOCAL)
	   && ps_global->VAR_BUGS_EXTRAS 
	   && can_access(ps_global->VAR_BUGS_EXTRAS, EXECUTE_ACCESS) == 0){
	    char *error		      = NULL;

	    *pp			      = mail_newbody_part();
	    pb			      = &((*pp)->body);
	    pp			      = &((*pp)->next);
	    pb->type		      = TYPETEXT;
	    pb->id		      = generate_message_id();
	    pb->description	      = cpystr("Local Configuration Data");
	    pb->parameter	      = mail_newbody_parameter();
	    pb->parameter->attribute  = cpystr("name");
	    pb->parameter->value      = cpystr("lconfig.txt");

	    if((store = so_get(CharStar, NULL, EDIT_ACCESS)) != NULL){
		PIPE_S  *syspipe;		
		gf_io_t  gc;
		
		pb->contents.text.data = (void *) store;
		gf_set_so_writec(&pc, store);
		if((syspipe = open_system_pipe(ps_global->VAR_BUGS_EXTRAS,
					 NULL, NULL,
					 PIPE_READ | PIPE_STDERR | PIPE_USER,
					 0, pipe_callback, pipe_report_error)) != NULL){
		    gf_set_readc(&gc, (void *)syspipe, 0, PipeStar, 0);
		    gf_filter_init();
		    error = gf_pipe(gc, pc);
		    (void) close_system_pipe(&syspipe, NULL, pipe_callback);
		}
		else
		  error = "executing config collector";

		gf_clear_so_writec(store);
	    }
	    
	    if(error){
		q_status_message1(SM_ORDER | SM_DING, 3, 4, 
				  "Problem %s", error);
		return(-1);
	    }
	    else			/* fixup attachment's size */
	      pb->size.bytes = strlen((char *) so_text(store));
	}

	if((flags & GRIPE_OPT_MSG) && mn_get_total(ps->msgmap) > 0L){
	    int ch = 0;
	
	    ps->redrawer = att_cur_drawer;
	    att_cur_drawer();

	    if((ch = one_try_want_to("Attach current message to report",
				     'y','x',NO_HELP,
				     WT_FLUSH_IN|WT_SEQ_SENSITIVE)) == 'y'){
		*pp		      = mail_newbody_part();
		pb		      = &((*pp)->body);
		pp		      = &((*pp)->next);
		pb->type	      = TYPEMESSAGE;
		pb->id		      = generate_message_id();
		snprintf(tmp, sizeof(tmp), "Problem Message (%ld of %ld)",
			mn_get_cur(ps->msgmap), mn_get_total(ps->msgmap));
		tmp[sizeof(tmp)-1] = '\0';
		pb->description	      = cpystr(tmp);

		/*---- Package each message in a storage object ----*/
		if((store = so_get(PART_SO_TYPE, NULL, EDIT_ACCESS)) != NULL){
		    pb->contents.text.data = (void *) store;
		}
		else{
		    q_status_message(SM_ORDER | SM_DING, 3, 4, err);
		    return(-1);
		}

		/* write the header */
		if((p = mail_fetch_header(ps->mail_stream, msgno, NIL, NIL,
					  NIL, FT_PEEK)) && *p)
		  so_puts(store, p);
		else
		  return(-1);

		pb->size.bytes = strlen(p);
		so_puts(store, "\015\012");

		if((p = pine_mail_fetch_text(ps->mail_stream, 
					     msgno, NULL, NULL, NIL))
		   &&  *p)
		  so_puts(store, p);
		else
		  return(-1);

		pb->size.bytes += strlen(p);
	    }
	    else if(ch == 'x'){
		q_status_message(SM_ORDER, 0, 3, "Bug report cancelled.");
		return(-1);
	    }
	}
    }
    else{
	/*---- Only one part! ----*/
	(*body)->type = TYPETEXT;
	(*body)->contents.text.data = (void *) store;
    }

    return(0);
}


ADDRESS *
gripe_token_addr(token)
    char *token;
{
    char    *p;
    ADDRESS *a = NULL;

    if(token && *token++ == '_'){
	if(!strcmp(token, "LOCAL_ADDRESS_")){
	    p = (ps_global->VAR_LOCAL_ADDRESS
		 && ps_global->VAR_LOCAL_ADDRESS[0])
		    ? ps_global->VAR_LOCAL_ADDRESS
		    : "postmaster";
	    a = rfc822_parse_mailbox(&p, ps_global->maildomain);
	    a->personal = cpystr((ps_global->VAR_LOCAL_FULLNAME 
				  && ps_global->VAR_LOCAL_FULLNAME[0])
				    ? ps_global->VAR_LOCAL_FULLNAME 
				    : "Place to report Alpine Bugs");
	}
	else if(!strcmp(token, "BUGS_ADDRESS_")){
	    p = (ps_global->VAR_BUGS_ADDRESS
		 && ps_global->VAR_BUGS_ADDRESS[0])
		    ? ps_global->VAR_BUGS_ADDRESS : "postmaster";
	    a = rfc822_parse_mailbox(&p, ps_global->maildomain);
	    a->personal = cpystr((ps_global->VAR_BUGS_FULLNAME 
				  && ps_global->VAR_BUGS_FULLNAME[0])
				    ? ps_global->VAR_BUGS_FULLNAME 
				    : "Place to report Alpine Bugs");
	}
    }

    return(a);
}


char *
gripe_id(key)
    char *key;
{
    int i,j,k,l;
    
    /*
     * Build our contribution to the subject; part constant string
     * and random 4 character alpha numeric string.
     */
    i = (int)(random() % 36L);
    j = (int)(random() % 36L);
    k = (int)(random() % 36L);
    l = (int)(random() % 36L);
    tmp_20k_buf[0] = '\0';
    snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%s (ID %c%c%d%c%c)", key,
	    (i < 10) ? '0' + i : 'A' + (i - 10),
	    (j < 10) ? '0' + j : 'A' + (j - 10),
	    (int)(random() % 10L),
	    (k < 10) ? '0' + k : 'A' + (k - 10),
	    (l < 10) ? '0' + l : 'A' + (l - 10));
    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
    return(cpystr(tmp_20k_buf));
}


/*
 * Used by gripe_tool.
 */
void
att_cur_drawer(void)
{
    int	       i, dline, j;
    char       buf[256+1];

    /* blat helpful message to screen */
    ClearBody();
    j = 0;
    for(dline = 2;
	dline < ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global);
	dline++){
	for(i = 0; i < 256 && att_cur_msg[j] && att_cur_msg[j] != '\n'; i++)
	  buf[i] = att_cur_msg[j++];

	buf[i] = '\0';
	if(att_cur_msg[j])
	  j++;
	else if(!i)
	  break;

        PutLine0(dline, 1, buf);
    }
}


#ifdef	_WINDOWS

/*
 * 
 */
char *
pcpine_help(HelpType section)
{
    char    **help_lines, *help_text = NULL;
    STORE_S  *store;
    gf_io_t   pc;

    /* assumption here is that HelpType is char **  */
    help_lines = section;
    if (help_lines != NULL){
	init_helper_getc(help_lines);
	if(store = so_get(CharStar, NULL, EDIT_ACCESS)){
	    gf_set_so_writec(&pc, store);
	    gf_filter_init();

	    gf_link_filter(gf_local_nvtnl, NULL);

	    gf_link_filter(gf_html2plain,
			   gf_html2plain_opt(NULL,
					     ps_global->ttyo->screen_cols,
					     non_messageview_margin(), NULL, NULL, GFHP_STRIPPED));

	    if(!gf_pipe(helper_getc, pc)){
		help_text  = (char *) store->txt;
		store->txt = (void *) NULL;
	    }

	    gf_clear_so_writec(store);
	    so_give(&store);
	}
    }

    return(help_text);
}


/*
 * 
 */
int
help_popup(SCROLL_S *sparms, int in_handle)
{
    MPopup hp_menu[10];
    int	   i = -1;

    if(in_handle){
	hp_menu[++i].type	= tQueue;
	hp_menu[i].label.style	= lNormal;
	hp_menu[i].label.string = "View Help Section";
	hp_menu[i].data.val	= 'V';
    }

    hp_menu[++i].type	    = tQueue;
    hp_menu[i].label.style  = lNormal;
    hp_menu[i].label.string = "Exit Help";
    hp_menu[i].data.val	    = 'E';

    hp_menu[++i].type = tTail;

    mswin_popup(hp_menu);

    return(0);
}


/*
 * 
 */
int
help_subsection_popup(SCROLL_S *sparms, int in_handle)
{
    MPopup hp_menu[10];
    int	   i = -1;

    if(in_handle){
	hp_menu[++i].type	= tQueue;
	hp_menu[i].label.style  = lNormal;
	hp_menu[i].label.string = "View Help Section";
	hp_menu[i].data.val	= 'V';
    }

    hp_menu[++i].type	    = tQueue;
    hp_menu[i].label.style  = lNormal;
    hp_menu[i].label.string = "Previous Help Section";
    hp_menu[i].data.val	    = 'P';

    hp_menu[++i].type	    = tQueue;
    hp_menu[i].label.style  = lNormal;
    hp_menu[i].label.string = "Exit Help";
    hp_menu[i].data.val	    = 'E';

    hp_menu[++i].type = tTail;

    if(mswin_popup(hp_menu) == (in_handle ? 1 : 0))
      /*(void) helper_internal()*/;

    return(0);
}

#endif /* _WINDOWS */
