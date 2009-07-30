/*
 * $Id: mailindx.h 770 2007-10-24 00:23:09Z hubert@u.washington.edu $
 *
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

#ifndef PINE_MAILINDX_INCLUDED
#define PINE_MAILINDX_INCLUDED


#include "../pith/mailindx.h"
#include "context.h"
#include "keymenu.h"
#include "../pith/state.h"
#include "../pith/msgno.h"
#include "../pith/indxtype.h"
#include "../pith/thread.h"
#include "../pith/color.h"


/*-----------
  Saved state to redraw message index body 
  ----*/
struct entry_state {
    unsigned hilite:1;
    unsigned bolded:1;
    unsigned already_erased:1;
    int      plus;
    long     msgno;		/* last msgno we drew */
    unsigned long id;
};


struct index_state {
    long         msg_at_top,
	         lines_per_page;
    struct       entry_state *entry_state;
    MSGNO_S     *msgmap;
    MAILSTREAM  *stream;
    IndexColType status_fld;		/* field for select X's */
    int          status_col;		/* column for select X's */
    IndexColType plus_fld;		/* field for threading '+' or '>' */
    int          plus_col;		/* column for threading '+' or '>' */
    IndexColType arrow_fld;		/* field for cursor arrow */
};


#define	AC_NONE		0x00		/* flags modifying apply_command */
#define	AC_FROM_THREAD	0x01		/* called from thread_command    */
#define	AC_COLL		0x02		/* offer collapse command        */
#define	AC_EXPN		0x04		/* offer expand command          */
#define	AC_UNSEL	0x08		/* all selected, offer UnSelect  */


/*
 * Macro to reveal horizontal scroll margin.  It can be no greater than
 * half the number of lines on the display...
 */
#define	HS_MAX_MARGIN(p)	(((p)->ttyo->screen_rows-FOOTER_ROWS(p)-3)/2)
#define	HS_MARGIN(p)		MIN((p)->scroll_margin, HS_MAX_MARGIN(p))


/*
 * Flags to indicate how much index border to paint
 */
#define	INDX_CLEAR	0x01
#define	INDX_HEADER	0x02
#define	INDX_FOOTER	0x04


typedef enum {MsgIndex, MultiMsgIndex, ZoomIndex, ThreadIndex} IndexType;


/*
 * Macro to simplify clearing body portion of pine's display
 */
#define ClearBody()	ClearLines(1, ps_global->ttyo->screen_rows 	      \
						  - FOOTER_ROWS(ps_global) - 1)


extern struct index_state *current_index_state;


/* exported protoypes */
void		 mail_index_screen(struct pine *);
struct key_menu *do_index_border(CONTEXT_S *, char *, MAILSTREAM *, MSGNO_S *, IndexType, int *, int);
void		*stop_threading_temporarily(void);
void		 restore_threading(void **);
int		 index_lister(struct pine *, CONTEXT_S *, char *, MAILSTREAM *, MSGNO_S *);
ICE_S		*build_header_line(struct pine *, MAILSTREAM *, MSGNO_S *, long, int *);
int		 condensed_thread_cue(PINETHRD_S *, ICE_S *, char **, size_t *, int, int);
int		 truncate_subj_and_from_strings(void);
void		 paint_index_hline(MAILSTREAM *, long, ICE_S *);
void		 setup_index_state(int);
void		 warn_other_cmds(void);
void		 thread_command(struct pine *, MAILSTREAM *, MSGNO_S *, UCS, int);
COLOR_PAIR      *apply_rev_color(COLOR_PAIR *, int);
#ifdef	_WINDOWS
int		 index_sort_callback(int, long);
void             view_in_new_window(void);
#endif


#endif /* PINE_MAILINDX_INCLUDED */
