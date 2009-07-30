/*
 * $Id: mailview.h 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $
 *
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

#ifndef PINE_MAILVIEW_INCLUDED
#define PINE_MAILVIEW_INCLUDED


#include <general.h>
#include "../pith/mailview.h"
#include "titlebar.h"
#include "keymenu.h"
#include "../pith/handle.h"
#include "../pith/store.h"
#include "../pith/bitmap.h"


/*
 * display_output_file mode flags
 */
#define	DOF_NONE	0
#define	DOF_EMPTY	1
#define	DOF_BRIEF	2


#define STYLE_NAME(s)   ((s)->text.desc ? (s)->text.desc : "text")


/*
 * Struct defining scrolltool operating parameters.
 */
typedef	struct scrolltool_s {
    struct {			/* Data and its attributes to scroll	 */
	void	   *text;	/* what to scroll			 */
	SourceType  src;	/* it's form (char **,char *,FILE *)	 */
	char	   *desc;	/* Description of "type" of data shown	 */
	HANDLE_S   *handles;	/* embedded data descriptions		 */
    } text;
    struct {			/* titlebar state			 */
	char	     *title;	/* screen's title			 */
	TitleBarType  style;	/* it's type				 */
	COLOR_PAIR   *color;
    } bar;
    struct {			/* screen's keymenu/command bindings	 */
	struct key_menu	 *menu;
	bitmap_t	  bitmap;
	OtherMenu	  what;
	void		(*each_cmd)(struct scrolltool_s *, int);
    } keys;
    struct {			/* help for this attachment		 */
	HelpType  text;		/* help text				 */
	char	 *title;	/* title for help screen		 */
    } help;
    struct {
	int (*click)(struct scrolltool_s *);
	int (*clickclick)(struct scrolltool_s *);
#ifdef	_WINDOWS
	/*
	 * For systems that support it, allow caller to do popup menu
	 */
	int (*popup)(struct scrolltool_s *, int);
#endif
    } mouse;
    struct {			/* where to start paging from		 */
	enum {FirstPage = 0, LastPage, Fragment, Offset, Handle} on;
	union {
	    char	*frag;	/* fragment in html text to start on	 */
	    long	 offset;
	} loc;
    } start;
    struct {			/* Non-default Command Processor	 */
	int (*tool)(int, MSGNO_S *, struct scrolltool_s *);
	/* The union below is opaque as far as scrolltool itself is
	 * concerned, but is provided as a container to pass data
	 * between the scrolltool caller and the given "handler"
	 * callback (or any other callback that takes a scrolltool_s *).
	 */
	union {
	    void *p;
	    int	  i;
	} data;
    } proc;
				/* End of page processing		 */
    int	       (*end_scroll)(struct scrolltool_s *);
				/* Handler for invalid command input	 */
    int	       (*bogus_input)(UCS);
    unsigned	 resize_exit:1;	/* Return from scrolltool if resized	 */
    unsigned	 body_valid:1;	/* Screen's body already displayed	 */
    unsigned	 no_stat_msg:1;	/* Don't display status messages	 */
    unsigned	 vert_handle:1;	/* hunt up and down on arrows/ctrl-[np]  */
    unsigned	 srch_handle:1;	/* search to next handle		 */
    unsigned	 quell_help:1;	/* Don't show handle nav help message    */
    unsigned	 quell_newmail:1; /* Don't check for new mail		 */
    unsigned	 quell_first_view:1; /* Don't act special first time through */
    unsigned	 jump_is_debug:1;
    unsigned	 use_indexline_color:1;
} SCROLL_S;


/* exported protoypes */
void	    mail_view_screen(struct pine *);
url_tool_t  url_local_handler(char *);
int	    url_local_mailto(char *);
int	    url_local_mailto_and_atts(char *, PATMT *);
int	    url_local_fragment(char *);
int	    scrolltool(SCROLL_S *);
int         ng_scroll_edit(CONTEXT_S *, int);
int         folder_select_update(CONTEXT_S *, int);
int         scroll_add_listmode(CONTEXT_S *, int);
void	    display_output_file(char *, char *, char *, int);
int	    rfc2369_editorial(long, HANDLE_S **, int, int, gf_io_t);
void	    view_writec_init(STORE_S *, HANDLE_S **, int, int);
void	    view_writec_destroy(void);
int	    view_writec(int);


#endif /* PINE_MAILVIEW_INCLUDED */
