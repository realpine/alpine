#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: titlebar.c 1075 2008-06-04 00:19:39Z hubert@u.washington.edu $";
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
#include "titlebar.h"
#include "folder.h"
#include "../pith/state.h"
#include "../pith/bitmap.h"
#include "../pith/msgno.h"
#include "../pith/thread.h"
#include "../pith/conf.h"
#include "../pith/init.h"
#include "../pith/sort.h"
#include "../pith/news.h"
#include "../pith/util.h"


/*
 * Internal prototypes
 */
int   digit_count(long);
void  output_titlebar(TITLE_S *);
char  sort_letter(SortOrder);
char *percentage(long, long, int);


/*
 * some useful macros...
 */
#define	MS_DEL			(0x01)
#define	MS_NEW			(0x02)
#define	MS_ANS			(0x04)
#define	MS_FWD			(0x08)

#define	STATUS_BITS(X)	(!(X && (X)->valid) ? 0				      \
			   : (X)->deleted ? MS_DEL			      \
			     : (X)->answered ? MS_ANS			      \
				: (as.stream && user_flag_is_set(as.stream, (X)->msgno, FORWARDED_FLAG)) ? MS_FWD \
				   : (as.stream				      \
				      && (ps_global->unseen_in_view	      \
				          || (!(X)->seen		      \
					      && (!IS_NEWS(as.stream)	      \
					          || F_ON(F_FAKE_NEW_IN_NEWS, \
						          ps_global)))))      \
				        ? MS_NEW : 0)

#define	BAR_STATUS(X)	(((X) & MS_DEL) ? "DEL"   \
			 : ((X) & MS_ANS) ? "ANS"   \
			  : ((X) & MS_FWD) ? "FWD"   \
		           : (as.stream		      \
			      && (!IS_NEWS(as.stream)   \
				  || F_ON(F_FAKE_NEW_IN_NEWS, ps_global)) \
			      && ((X) & MS_NEW)) ? "NEW" : "   ")


static struct titlebar_state {
    MAILSTREAM	*stream;
    MSGNO_S	*msgmap;
    char	*title,
		*folder_name,
		*context_name;
    long	 current_msg,
		 current_line,
		 current_thrd,
		 total_lines;
    int		 msg_state,
		 cur_mess_col,
		 del_column, 
		 percent_column,
		 page_column,
		 screen_cols;
    enum	 {Normal, OnlyRead, Closed} stream_status;
    TitleBarType style;
    TITLE_S      titlecontainer;
} as, titlebar_stack;


static int titlebar_is_dirty = 1;


void
push_titlebar_state(void)
{
    titlebar_stack     = as;
    as.folder_name     = NULL;	/* erase knowledge of malloc'd data */
    as.context_name    = NULL;
}


void
pop_titlebar_state(void)
{
    /* guard against case where push pushed no state */
    if(titlebar_stack.style != TitleBarNone){
	fs_give((void **)&(as.folder_name)); /* free malloc'd values */
	fs_give((void **)&(as.context_name));
	as = titlebar_stack;
    }
}


int
digit_count(long int n)
{
    int i;

    return((n > 9)
	     ? (1 + (((i = digit_count(n / 10L)) == 3 || i == 7) ? i+1 : i))
	     : 1);
}


void
mark_titlebar_dirty(void)
{
    titlebar_is_dirty = 1;
}


/*----------------------------------------------------------------------
      Sets up style and contents of current titlebar line

    All of the text is assumed to be UTF-8 text, which probably isn't
    true yet.

    Args: title -- The title that appears in the center of the line
		     This title is in UTF-8 characters.
          display_on_screen -- flag whether to display on screen or generate
                                string
          style  -- The format/style of the titlebar line
	  msgmap -- MSGNO_S * to selected message map
          current_pl -- The current page or line number
          total_pl   -- The total line or page count

  Set the contents of the anchor line. It's called an anchor line
because it's always present and titlebars the user. This accesses a
number of global variables, but doesn't change any. There are several
different styles of the titlebar line.

It's OK to call this with a bogus current message - it is only used
to look up status of current message 
 
Formats only change the right section (part3).
  FolderName:      "<folder>"  xx Messages
  MessageNumber:   "<folder>" message x,xxx of x,xxx XXX
  TextPercent:     line xxx of xxx  xx%
  MsgTextPercent:  "<folder>" message x,xxx of x,xxx  xx% XXX
  FileTextPercent: "<filename>" line xxx of xxx  xx%

Several strings and column numbers are saved so later updates to the status 
line for changes in message number or percentage can be done efficiently.
 ----*/

char *
set_titlebar(char *title, MAILSTREAM *stream, CONTEXT_S *cntxt, char *folder,
	     MSGNO_S *msgmap, int display_on_screen, TitleBarType style,
	     long int current_pl, long int total_pl, COLOR_PAIR *color)
{
    struct variable *vars = ps_global->vars;
    MESSAGECACHE  *mc = NULL;
    PINETHRD_S    *thrd = NULL;
    TITLE_S       *tc;

    dprint((9, "set_titlebar - style: %d  current message cnt:%ld",
	       style, mn_total_cur(msgmap)));
    dprint((9, "  current_pl: %ld  total_pl: %ld\n", 
	       current_pl, total_pl));

    as.current_msg = (mn_get_total(msgmap) > 0L)
			 ? MAX(0, mn_get_cur(msgmap)) : 0L;
    as.msgmap	     = msgmap;
    as.style	     = style;
    as.title	     = title;
    as.stream	     = stream;
    as.stream_status = (!as.stream || (sp_dead_stream(as.stream)))
			 ? Closed : as.stream->rdonly ? OnlyRead : Normal;

    if(ps_global->first_open_was_attempted
       && as.stream_status == Closed
       && VAR_TITLECLOSED_FORE_COLOR && VAR_TITLECLOSED_BACK_COLOR){
	memset(&as.titlecontainer.color, 0, sizeof(as.titlecontainer.color));
	strncpy(as.titlecontainer.color.fg,
		VAR_TITLECLOSED_FORE_COLOR, MAXCOLORLEN);
	as.titlecontainer.color.fg[MAXCOLORLEN] = '\0';
	strncpy(as.titlecontainer.color.bg,
		VAR_TITLECLOSED_BACK_COLOR, MAXCOLORLEN);
	as.titlecontainer.color.bg[MAXCOLORLEN] = '\0';
    }
    else{
	if(color){
	    memset(&as.titlecontainer.color, 0, sizeof(as.titlecontainer.color));
	    if(color->fg){
		strncpy(as.titlecontainer.color.fg, color->fg, MAXCOLORLEN);
		as.titlecontainer.color.fg[MAXCOLORLEN] = '\0';
	    }

	    if(color->bg){
		strncpy(as.titlecontainer.color.bg, color->bg, MAXCOLORLEN);
		as.titlecontainer.color.bg[MAXCOLORLEN] = '\0';
	    }
	}
	else{
	    memset(&as.titlecontainer.color, 0, sizeof(as.titlecontainer.color));
	    if(VAR_TITLE_FORE_COLOR){
		strncpy(as.titlecontainer.color.fg,
			VAR_TITLE_FORE_COLOR, MAXCOLORLEN);
		as.titlecontainer.color.fg[MAXCOLORLEN] = '\0';
	    }

	    if(VAR_TITLE_BACK_COLOR){
		strncpy(as.titlecontainer.color.bg,
			VAR_TITLE_BACK_COLOR, MAXCOLORLEN);
		as.titlecontainer.color.bg[MAXCOLORLEN] = '\0';
	    }
	}
    }

    if(as.folder_name)
      fs_give((void **)&as.folder_name);

    if(folder){
	if(!strucmp(folder, ps_global->inbox_name) && cntxt == ps_global->context_list)
	  as.folder_name = cpystr(pretty_fn(folder));
	else
	  as.folder_name = cpystr(folder);
    }

    if(!as.folder_name)
      as.folder_name = cpystr("");

    if(as.context_name)
      fs_give((void **)&as.context_name);

    /*
     * Handle setting up the context if appropriate.
     */
    if(cntxt && context_isambig(folder) && ps_global->context_list->next
       && (strucmp(as.folder_name, ps_global->inbox_name) || cntxt != ps_global->context_list)){
	/*
	 * if there's more than one context and the current folder
	 * is in it (ambiguous name), set the context name...
	 */
	as.context_name = cpystr(cntxt->nickname
				   ? cntxt->nickname
				   : cntxt->context);
    }

    if(!as.context_name)
      as.context_name = cpystr("");

    if(as.stream && style != FolderName
       && style != ThrdIndex && as.current_msg > 0L) {
	long rawno;

	if((rawno = mn_m2raw(msgmap, as.current_msg)) > 0L
	   && rawno <= as.stream->nmsgs
	   && !((mc = mail_elt(as.stream, rawno)) && mc->valid)){
	    pine_mail_fetch_flags(as.stream, long2string(rawno), NIL);
	    mc = mail_elt(as.stream, rawno);
	}
    }
    
    if(style == ThrdIndex || style == ThrdMsgNum || style == ThrdMsgPercent){
	if(mn_get_total(msgmap) > 0L)
	  thrd = fetch_thread(stream, mn_m2raw(msgmap, mn_get_cur(msgmap)));
	
	if(thrd && thrd->top && thrd->top != thrd->rawno)
	  thrd = fetch_thread(stream, thrd->top);

	if(thrd)
	  as.current_thrd = thrd->thrdno;
    }

    switch(style) {
      case ThrdIndex:
	as.total_lines = msgmap->max_thrdno;
	break;

      case TextPercent:
      case MsgTextPercent:
      case FileTextPercent :
      case ThrdMsgPercent:
        as.total_lines = total_pl;
        as.current_line = current_pl;
				        /* fall through to set state */
      case ThrdMsgNum:
      case MessageNumber:
        as.msg_state = STATUS_BITS(mc);

      case FolderName:		        /* nothing more to do for this one */
	break;
      default:
	break;
    }

    tc = format_titlebar();
    if(display_on_screen)
      output_titlebar(tc);

    return(tc->titlebar_line);
}


void 
redraw_titlebar(void)
{
    output_titlebar(format_titlebar());
}


void
output_titlebar(TITLE_S *tc)
{
    COLOR_PAIR *lastc = NULL, *newcolor;

    if(ps_global->ttyo
       && (ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global)) < 1){
	titlebar_is_dirty = 1;
	return;
    }

    newcolor = tc ? &tc->color : NULL;

    if(newcolor)
      lastc = pico_set_colorp(newcolor, PSC_REV | PSC_RET);
	    
    if(tc && tc->titlebar_line)
      PutLine0(0, 0, tc->titlebar_line);

    if(lastc){
	(void)pico_set_colorp(lastc, PSC_NONE);
	free_color_pair(&lastc);
    }

    fflush(stdout);
}


void
titlebar_stream_closing(MAILSTREAM *stream)
{
    if(stream == as.stream)
      as.stream = NULL;
}


/* caller should free returned color pair */
COLOR_PAIR *
current_titlebar_color(void)
{
    TITLE_S       *tc;
    COLOR_PAIR    *col;
    COLOR_PAIR    *the_color = NULL;

    tc = format_titlebar();
    col = tc ? &tc->color : NULL;

    if(col && col->fg && col->fg[0] && col->bg && col->bg[0])
      the_color = new_color_pair(col->fg, col->bg);

    return(the_color);
}


/*----------------------------------------------------------------------
      Redraw or draw the top line, the title bar 

 The titlebar has Four fields:
     1) "Version" of fixed length and is always positioned two spaces 
        in from left display edge.
     2) "Location" which is fixed for each style of titlebar and
        is positioned two spaces from the right display edge
     3) "Title" which is of fixed length, and is centered if
        there's space
     4) "Folder" whose existence depends on style and which can
        have it's length adjusted (within limits) so it will
        equally share the space between 1) and 2) with the 
        "Title".  The rule for existence is that in the
        space between 1) and 2) there must be one space between
        3) and 4) AND at least 50% of 4) must be displayed.

   Returns - Formatted title bar 
 ----*/
TITLE_S *
format_titlebar(void)
{
    char    version[50], fold_tmp[6*MAXPATH+1], *titlebar_line,
	    loc1[200], loc_label[10], *thd_label, *ss_string, *q,
	   *plus, *loc2 = "", title[200];
    int     title_len = 0, ver_len, loc1_len = 0, loc2_len = 0, fold_len = 0, num_len,
	    s1 = 0, s2 = 0, s3 = 0, s4 = 0, s5 = 0, s6 = 0, tryloc = 1,
	    cur_mess_col_offset = -1, percent_column_offset = -1, page_column_offset = -1,
	    ss_len, thd_len, is_context, avail, extra;

    titlebar_is_dirty = 0;

    if(!as.title)
      return NULL;

    if(!as.folder_name)
      as.folder_name = cpystr("");

    if(!as.context_name)
      as.context_name = cpystr("");

    /*
     * Full blown title looks like:
     *
     * | LV vers VT title TF folder FL location LR |
     */
#define LV 2	/* space between Left edge and Version, must be >= 2 */
#define VT 3	/* space between Version and Title */
#define TF 1	/* space between Title and Folder */
#define FL 2	/* space between Folder and Location */
#define LR 2	/* space between Location and Right edge, >= 2 */
/* half of n but round up */
#define HRU(n) (((n) <= 0) ? 0 : (((n)%2) ? ((n)+1)/2 : (n)/2))
/* half of n but round down */
#define HRD(n) (((n) <= 0) ? 0 : ((n)/2))

    titlebar_line = as.titlecontainer.titlebar_line;

    avail = MIN(ps_global->ttyo->screen_cols, MAX_SCREEN_COLS);

    /* initialize */
    as.del_column     = -1;
    as.cur_mess_col   = -1;
    as.percent_column = -1;
    as.page_column    = -1;

    is_context        = as.context_name ? strlen(as.context_name) : 0;

    {char revision[10];
    snprintf(version, sizeof(version), "ALPINE %s(%s)", ALPINE_VERSION, get_alpine_revision_number(revision, sizeof(revision)));
    }
    version[sizeof(version)-1] = '\0';
    ver_len = (int) utf8_width(version);	/* fixed version field width */

    title[0] = '\0';
    if(as.title){
	strncpy(title, as.title, sizeof(title));
	title[sizeof(title)-1] = '\0';
    }

    /* Add Sort indicator to title */
    if(F_ON(F_SHOW_SORT, ps_global) &&
       !(as.style == FileTextPercent || as.style == TextPercent)){
	SortOrder current_sort;
	int       current_rev;
	char      let;

	current_sort = mn_get_sort(ps_global->msgmap);
	current_rev  = mn_get_revsort(ps_global->msgmap);

	/* turn current_sort into a letter */
	let = sort_letter(current_sort);
	if(let == 'A' && current_rev){
	    let = 'R';
	    current_rev = 0;
	}

	snprintf(title+strlen(title), sizeof(title)-strlen(title),
		 " [%s%c]", current_rev ? "R" : "", let);
	title[sizeof(title)-1] = '\0';
    }

    title_len = (int) utf8_width(title);	/* title field width   */

    s1 = MAX(MIN(LV, avail), 0);	/* left two spaces */
    avail -= s1;

    s6 = MAX(MIN(LR, avail), 0);	/* right two spaces */
    avail -= s6;

    title_len = MAX(MIN(title_len, avail), 0);
    avail -= title_len;

    if(ver_len + VT > avail){		/* can only fit title */
	ver_len = 0;

	s2 = MAX(MIN(HRD(avail), avail), 0);
	avail -= s2;

	s3 = MAX(0, avail);
    }
    else{
	s2 = MAX(MIN(VT, avail), 0);
	avail -= s2;

	ver_len = MAX(MIN(ver_len, avail), 0);
	avail -= ver_len;

try_smaller_loc:

	/* 
	 * set location field's length and value based on requested style 
	 */
	if(as.style == ThrdIndex)
	  /* TRANSLATORS: In titlebar, Thd is an abbreviation for Thread, Msg for Message.
	     They are used when there isn't enough space so need to be short.
	     The formatting isn't very flexible. These come before the number
	     of the message or thread, as in
	     Message 17
	     when reading message number 17. */
	  snprintf(loc_label, sizeof(loc_label), "%s ", (is_context || tryloc==2) ? _("Thd") : _("Thread"));
	else
	  snprintf(loc_label, sizeof(loc_label), "%s ", (is_context || tryloc==2) ? _("Msg") : _("Message"));

	if(tryloc == 3 && as.style != FolderName && mn_get_total(as.msgmap))
	  loc_label[0] = '\0';

	loc_label[sizeof(loc_label)-1] = '\0';

	if(as.style == ThrdMsgNum || as.style == ThrdMsgPercent){
	    thd_label = is_context ? _("Thd") : _("Thread");
	    thd_len = (int) utf8_width(thd_label);
	}

	loc1_len = (int) utf8_width(loc_label);		/* initial length */

	if(!mn_get_total(as.msgmap)){
	    loc_label[strlen(loc_label)-1] = 's';
	    snprintf(loc1, sizeof(loc1), "%s %s", _("No"), loc_label);
	    loc1[sizeof(loc1)-1]= '\0';
	}
	else{
	    switch(as.style){
	      case FolderName :			/* "x,xxx <loc_label>s" */
		if(*loc_label){
		    if(mn_get_total(as.msgmap) != 1)
		      loc_label[strlen(loc_label)-1] = 's';
		    else
		      loc_label[strlen(loc_label)-1] = '\0';
		}

		snprintf(loc1, sizeof(loc1), "%s %s", comatose(mn_get_total(as.msgmap)), loc_label);
		loc1[sizeof(loc1)-1]= '\0';
		break;

	      case MessageNumber :	       	/* "<loc_label> xxx of xxx DEL"  */
		num_len = strlen(comatose(mn_get_total(as.msgmap)));
		cur_mess_col_offset = loc1_len;
		snprintf(loc1, sizeof(loc1), "%s%*.*s of %s", loc_label,
			num_len, num_len,
			comatose(as.current_msg),
			comatose(mn_get_total(as.msgmap)));
		loc1[sizeof(loc1)-1]= '\0';
		loc2 = BAR_STATUS(as.msg_state);
		loc2_len = 3;
		break;

	      case ThrdIndex :	       	/* "<loc_label> xxx of xxx"  */
		num_len = strlen(comatose(as.total_lines));
		cur_mess_col_offset = loc1_len;
		snprintf(loc1, sizeof(loc1), "%s%*.*s of %s", loc_label,
			num_len, num_len,
			comatose(as.current_thrd),
			comatose(as.total_lines));
		loc1[sizeof(loc1)-1]= '\0';
		break;

	      case ThrdMsgNum :	       	/* "<loc_label> xxx in Thd xxx DEL"  */
		num_len = strlen(comatose(mn_get_total(as.msgmap)));
		cur_mess_col_offset = loc1_len;
		snprintf(loc1, sizeof(loc1), "%s%*.*s in %s %s", loc_label,
			num_len, num_len,
			comatose(as.current_msg),
			thd_label,
			comatose(as.current_thrd));
		loc1[sizeof(loc1)-1]= '\0';
		loc2 = BAR_STATUS(as.msg_state);
		loc2_len = 3;
		break;

	      case MsgTextPercent :		/* "<loc_label> xxx of xxx xx% DEL" */
		num_len = strlen(comatose(mn_get_total(as.msgmap)));
		cur_mess_col_offset = loc1_len;
		percent_column_offset = 3;
		snprintf(loc1, sizeof(loc1), "%s%*.*s of %s %s", loc_label, 
			num_len, num_len,
			comatose(as.current_msg),
			comatose(mn_get_total(as.msgmap)),
			percentage(as.current_line, as.total_lines, 1));
		loc1[sizeof(loc1)-1]= '\0';
		loc2 = BAR_STATUS(as.msg_state);
		loc2_len = 3;
		break;

	      case ThrdMsgPercent :	  /* "<loc_label> xxx in Thd xxx xx% DEL"  */
		num_len = strlen(comatose(mn_get_total(as.msgmap)));
		cur_mess_col_offset = loc1_len;
		percent_column_offset = 3;
		snprintf(loc1, sizeof(loc1), "%s%*.*s in %s %s %s", loc_label, 
			num_len, num_len,
			comatose(as.current_msg),
			thd_label,
			comatose(as.current_thrd),
			percentage(as.current_line, as.total_lines, 1));
		loc1[sizeof(loc1)-1]= '\0';
		loc2 = BAR_STATUS(as.msg_state);
		loc2_len = 3;
		break;

	      case TextPercent :
		/* NOTE: no fold_tmp setup below for TextPercent style */
	      case FileTextPercent :	/* "Line xxx of xxx xx%" */
		num_len = strlen(comatose(as.total_lines));
		page_column_offset = 5;
		percent_column_offset = 3;
		snprintf(loc1, sizeof(loc1), "Line %*.*s of %s %s",
			num_len, num_len,
			comatose(as.current_line),
			comatose(as.total_lines),
			percentage(as.current_line, as.total_lines, 1));
		loc1[sizeof(loc1)-1]= '\0';
		break;
	      default:
		break;
	    }
	}

	loc1_len = utf8_width(loc1);

	if(loc1_len + loc2_len + ((loc2_len > 0) ? 1 : 0) >= avail){		/* can't fit location in */
	    if(tryloc < 3){
		tryloc++;
		goto try_smaller_loc;
	    }

	    loc1_len = loc2_len = 0;

	    avail += s2;			/* re-allocate s2 to center title */

	    s2 = MAX(MIN(HRD(avail), avail), 0);
	    avail -= s2;

	    s3 = MAX(0, avail);
	}
	else{
	    loc1_len = MAX(MIN(loc1_len, avail), 0);
	    avail -= loc1_len;

	    loc2_len = MAX(MIN(loc2_len, avail), 0);
	    avail -= loc2_len;

	    if(loc2_len > 0){
		s5 = MAX(MIN(1, avail), 0);
		avail -= s5;
	    }
	    else
	      s5 = 0;

	    s3 = MAX(MIN(TF, avail), 0);
	    avail -= s3;

	    s4 = MAX(MIN(FL, avail), 0);
	    avail -= s4;

	    if(avail){
		/* TRANSLATORS: it might say READONLY or CLOSED in the titlebar, referring to
		   the current folder. */
		ss_string         = as.stream_status == Closed ? _("(CLOSED)") :
				    (as.stream_status == OnlyRead
				     && !IS_NEWS(as.stream))
				       ? _("(READONLY)") : "";
		ss_len            = (int) utf8_width(ss_string);

		/* figure folder_length and what's to be displayed */
		fold_tmp[0] = '\0';
		if(as.style == FileTextPercent || as.style == TextPercent){
		    if(as.style == FileTextPercent){
			extra    = (int) utf8_width("File: ");
			fold_len = (int) utf8_width(as.folder_name);
			if(fold_len + extra <= avail){ 	/* all of folder fit? */
			    strncpy(fold_tmp, "File: ", sizeof(fold_tmp));
			    q = fold_tmp + strlen(fold_tmp);
			    strncpy(q, as.folder_name, sizeof(fold_tmp)-(q-fold_tmp));
			}
			else if(HRU(fold_len) + extra+3 <= avail){
			    /*
			     * fold_tmp = ...partial_folder_name
			     */
			    strncpy(fold_tmp, "File: ...", sizeof(fold_tmp));
			    q = fold_tmp + strlen(fold_tmp);
			    (void) utf8_to_width_rhs(q, as.folder_name, sizeof(fold_tmp)-(q-fold_tmp), avail-(extra+3));
			}
		    }
		    /* else leave folder/file name blank */
		}
		else{
		    int    ct_len;

		    fold_len = (int) utf8_width(as.folder_name);

		    if(is_context
		      && as.stream_status != Closed
		      && (ct_len = (int) utf8_width(as.context_name))){

			extra = 3;		/* length from "<" ">" and SPACE */

			if(ct_len + fold_len + ss_len + extra <= avail){
			    q = fold_tmp;
			    *q++ = '<';
			    strncpy(q, as.context_name, sizeof(fold_tmp)-(q-fold_tmp));
			    q += strlen(q);
			    *q++ = '>';
			    *q++ = ' ';
			    strncpy(q, as.folder_name, sizeof(fold_tmp)-(q-fold_tmp));
			    q += strlen(q);
			    strncpy(q, ss_string, sizeof(fold_tmp)-(q-fold_tmp));
			}
			else if(ct_len + fold_len + ss_len + extra <= avail){
			    q = fold_tmp;
			    *q++ = '<';
			    strncpy(q, as.context_name, sizeof(fold_tmp)-(q-fold_tmp));
			    q += strlen(q);
			    *q++ = '>';
			    *q++ = ' ';
			    strncpy(q, as.folder_name, sizeof(fold_tmp)-(q-fold_tmp));
			    q += strlen(q);
			    strncpy(q, ss_string, sizeof(fold_tmp)-(q-fold_tmp));
			}
			else if(HRU(ct_len) + fold_len + ss_len + extra <= avail){
			    q = fold_tmp;
			    *q++ = '<';
			    q += utf8_pad_to_width(q, as.context_name, sizeof(fold_tmp)-(q-fold_tmp), avail-(fold_len+ss_len+extra), 1);
			    *q++ = '>';
			    *q++ = ' ';
			    strncpy(q, as.folder_name, sizeof(fold_tmp)-(q-fold_tmp));
			    q += strlen(q);
			    strncpy(q, ss_string, sizeof(fold_tmp)-(q-fold_tmp));
			}
			else if(HRU(ct_len) + HRU(fold_len) + ss_len + extra <= avail){
			    q = fold_tmp;
			    *q++ = '<';
			    q += utf8_pad_to_width(q, as.context_name, sizeof(fold_tmp)-(q-fold_tmp), HRU(ct_len), 1);
			    *q++ = '>';
			    *q++ = ' ';
			    q += utf8_to_width_rhs(q, as.folder_name, sizeof(fold_tmp)-(q-fold_tmp), avail-(HRU(ct_len)+ss_len+extra));
			    strncpy(q, ss_string, sizeof(fold_tmp)-(q-fold_tmp));
			}
			else if(ss_len > 0 && ss_len <= avail){
			    q = fold_tmp;
			    strncpy(q, ss_string, sizeof(fold_tmp)-(q-fold_tmp));
			}
			/* else leave it out */
		    }
		    else{
			/* TRANSLATORS: the name of the open folder follows this in the titlebar */
			extra = strlen(_("Folder: "));
			if(fold_len + ss_len + extra <= avail){
			    q = fold_tmp;
			    strncpy(q, "Folder: ", sizeof(fold_tmp)-(q-fold_tmp));
			    q += strlen(q);
			    strncpy(q, as.folder_name, sizeof(fold_tmp)-(q-fold_tmp));
			    q += strlen(q);
			    strncpy(q, ss_string, sizeof(fold_tmp)-(q-fold_tmp));
			}
			else{
			    if(fold_len + ss_len <= avail){
				q = fold_tmp;
				strncpy(q, as.folder_name, sizeof(fold_tmp)-(q-fold_tmp));
				q += strlen(q);
				strncpy(q, ss_string, sizeof(fold_tmp)-(q-fold_tmp));
			    }
			    else if(((HRU(fold_len) + ss_len <= avail)
				     || (avail > ss_len+3 && avail > 10)) && fold_len > 5){
				q = fold_tmp;
				strncpy(q, "...", sizeof(fold_tmp));
				q += strlen(q);
				q += utf8_to_width_rhs(q, as.folder_name, sizeof(fold_tmp)-(q-fold_tmp), avail-(3+ss_len));
				strncpy(q, ss_string, sizeof(fold_tmp)-(q-fold_tmp));
			    }
			    else if(ss_len > 0 && ss_len <= avail){
				q = fold_tmp;
				strncpy(q, ss_string, sizeof(fold_tmp)-(q-fold_tmp));
			    }
			    /* else leave it out */
			}
		    }
		}

		fold_tmp[sizeof(fold_tmp)-1] = '\0';
		
		/* write title, location and, optionally, the folder name */
		fold_len = (int) utf8_width(fold_tmp);
	    }

	    fold_len = MAX(MIN(fold_len, avail), 0);
	    avail -= fold_len;

	    /* center folder in its space */
	    if(avail){
		int inc;

		inc = HRU(avail);

		s3 += inc;
		avail -= inc;

		s4 += avail;
	    }
	}
    }

    plus = "";

    if(as.style != FileTextPercent && as.style != TextPercent){
	NETMBX mb;

	if(as.stream
	   && as.stream->mailbox
	   && mail_valid_net_parse(as.stream->mailbox, &mb)
	   && (mb.sslflag || mb.tlsflag))
	  plus = "+";
    }


    if(loc1_len > 0 && cur_mess_col_offset >= 0)
      as.cur_mess_col = s1+ver_len+s2+title_len+s3+fold_len+s4 + cur_mess_col_offset;

    if(loc1_len > 0 && page_column_offset >= 0)
      as.page_column = s1+ver_len+s2+title_len+s3+fold_len+s4 + page_column_offset;

    if(loc2_len > 0)
      as.del_column = s1+ver_len+s2+title_len+s3+fold_len+s4+loc1_len+s5;

    if(loc1_len > 0 && percent_column_offset > 0)
      as.percent_column = s1+ver_len+s2+title_len+s3+fold_len+s4+loc1_len - percent_column_offset;


    utf8_snprintf(titlebar_line, 6*MAX_SCREEN_COLS, "%*.*s%-*.*w%*.*s%-*.*w%*.*s%-*.*w%*.*s%-*.*w%*.*s%-*.*w%*.*s",
		  s1, s1, "",
		  ver_len, ver_len, version,
		  s2, s2, "",
		  title_len, title_len, title,
		  s3, s3, "",
		  fold_len, fold_len, fold_tmp,
		  s4, s4, "",
		  loc1_len, loc1_len, loc1,
		  s5, s5, "",
		  loc2_len, loc2_len, loc2,
		  s6, s6, plus);

    return(&as.titlecontainer);
}


char
sort_letter(SortOrder sort)
{
    char let = 'A', *p;

    if((p = sort_name(sort)) != NULL && *p){
	while(*(p+1) && islower((unsigned char) *p))
	  p++;
	
	if(*p)
	  let = *p;
    }

    return(let);
}


/*
 *  Update the titlebar line if the message number changed
 *
 *  Args: None, uses state setup on previous call to set_titlebar.
 */
void
update_titlebar_message(void)
{
    long        curnum, maxnum, oldnum;
    PINETHRD_S *thrd = NULL;
    COLOR_PAIR *lastc = NULL, *titlecolor;
    char        buf[50];
    int         num_len;
    unsigned long rawno;

    if(ps_global->ttyo
       && (ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global)) < 1){
	titlebar_is_dirty = 1;
	return;
    }

    if(as.style == ThrdIndex){

	oldnum = as.current_thrd;

	if(as.stream && (rawno=mn_m2raw(as.msgmap, mn_get_cur(as.msgmap))))
	  thrd = fetch_thread(as.stream, rawno);
	
	if(thrd && thrd->top && (thrd=fetch_thread(as.stream, thrd->top)))
	  curnum = thrd->thrdno;
    }
    else if(as.cur_mess_col >= 0){
	curnum = mn_get_cur(as.msgmap);
	oldnum = as.current_msg;
    }

    if(as.cur_mess_col >= 0 && oldnum != curnum){

	if(as.style == ThrdIndex){
	    as.current_thrd = curnum;
	    maxnum = as.msgmap->max_thrdno;
	}
	else{
	    as.current_msg = curnum;
	    maxnum = mn_get_total(as.msgmap);
	}

	titlecolor = &as.titlecontainer.color;

	if(titlecolor)
	  lastc = pico_set_colorp(titlecolor, PSC_REV|PSC_RET);

	num_len = strlen(comatose(maxnum));

	snprintf(buf, sizeof(buf), "%*.*s", num_len, num_len, comatose(curnum));

	PutLine0(0, as.cur_mess_col, buf);

	if(lastc){
	    (void)pico_set_colorp(lastc, PSC_NONE);
	    free_color_pair(&lastc);
	}

	fflush(stdout);
    }
}



/*
 *  Update titlebar line's message status field ("DEL", "NEW", etc)
 *
 *  Args:  None, operates on state set during most recent set_titlebar call
 */
int
update_titlebar_status(void)
{
    unsigned long rawno;
    MESSAGECACHE *mc;
    COLOR_PAIR *lastc = NULL, *titlecolor;
    
    if(ps_global->ttyo
       && (ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global)) < 1){
	titlebar_is_dirty = 1;
	return(0);
    }

    if(!as.stream || as.current_msg <= 0L || as.del_column < 0)
      return(1);

    if(as.current_msg != mn_get_cur(as.msgmap))
      update_titlebar_message();

    mc = ((rawno = mn_m2raw(as.msgmap, as.current_msg)) > 0L
	  && rawno <= as.stream->nmsgs)
	    ? mail_elt(as.stream, rawno) : NULL;

    if(!(mc && mc->valid))
      return(0);			/* indeterminate */

    if(mc->deleted){			/* deleted takes precedence */
	if(as.msg_state & MS_DEL)
	  return(1);
    }
    else if(mc->answered){		/* then answered */
	if(as.msg_state & MS_ANS)
	  return(1);
    }					/* then forwarded */
    else if(user_flag_is_set(as.stream, mc->msgno, FORWARDED_FLAG)){
	if(as.msg_state & MS_FWD)
	  return(1);
    }
    else if(!mc->seen && as.stream
	    && (!IS_NEWS(as.stream)
		|| F_ON(F_FAKE_NEW_IN_NEWS, ps_global))){
	if(as.msg_state & MS_NEW)	/* then seen */
	  return(1);
    }
    else{
	if(as.msg_state == 0)		/* nothing to change */
	  return(1);
    }

    as.msg_state = STATUS_BITS(mc);

    titlecolor = &as.titlecontainer.color;

    if(titlecolor)
      lastc = pico_set_colorp(titlecolor, PSC_REV|PSC_RET);

    PutLine0(0, as.del_column, BAR_STATUS(as.msg_state));

    if(lastc){
	(void)pico_set_colorp(lastc, PSC_NONE);
	free_color_pair(&lastc);
    }

    fflush(stdout);
    return(1);
}


/*
 *  Update the percentage shown in the titlebar line
 *
 *  Args: new_line_number -- line number to calculate new percentage
 */
void
update_titlebar_percent(long int new_line_number)
{
    COLOR_PAIR *lastc = NULL, *titlecolor;

    if(as.percent_column < 0 || new_line_number == as.current_line)
      return;

    if(ps_global->ttyo
       && (ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global)) < 1){
	titlebar_is_dirty = 1;
	return;
    }

    as.current_line = new_line_number;

    titlecolor = &as.titlecontainer.color;

    if(titlecolor)
      lastc = pico_set_colorp(titlecolor, PSC_REV|PSC_RET);

    PutLine0(0, as.percent_column,
	     percentage(as.current_line, as.total_lines, 0));

    if(lastc){
	(void)pico_set_colorp(lastc, PSC_NONE);
	free_color_pair(&lastc);
    }

    fflush(stdout);
}


/*
 *  Update the percentage AND line number shown in the titlebar line
 *
 *  Args: new_line_number -- line number to calculate new percentage
 */
void
update_titlebar_lpercent(long int new_line_number)
{
    COLOR_PAIR *lastc = NULL, *titlecolor;
    int         num_len;
    char        buf[50];

    if(as.page_column < 0 || new_line_number == as.current_line)
      return;

    if(ps_global->ttyo
       && (ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global)) < 1){
	titlebar_is_dirty = 1;
	return;
    }

    as.current_line = new_line_number;

    titlecolor = &as.titlecontainer.color;

    if(titlecolor)
      lastc = pico_set_colorp(titlecolor, PSC_REV|PSC_RET);

    num_len = strlen(comatose(as.total_lines));
    snprintf(buf, sizeof(buf), "%*.*s", num_len, num_len, comatose(as.current_line));

    PutLine0(0, as.page_column, buf);

    PutLine0(0, as.percent_column,
	     percentage(as.current_line, as.total_lines, 0));

    if(lastc){
	(void)pico_set_colorp(lastc, PSC_NONE);
	free_color_pair(&lastc);
    }

    fflush(stdout);
}


/*----------------------------------------------------------------------
    Return static buf containing portion of lines displayed

  Args:  part -- how much so far
	 total -- how many total

  ---*/
char *
percentage(long int part, long int total, int suppress_top)
{
    static char percent[4];

    if(total == 0L || (total <= ps_global->ttyo->screen_rows
				 - HEADER_ROWS(ps_global)
				 - FOOTER_ROWS(ps_global)))
      strncpy(percent, "ALL", sizeof(percent));
    else if(!suppress_top && part <= ps_global->ttyo->screen_rows
				      - HEADER_ROWS(ps_global)
				      - FOOTER_ROWS(ps_global))
      strncpy(percent, "TOP", sizeof(percent));
    else if(part >= total)
      strncpy(percent, "END", sizeof(percent));
    else
      snprintf(percent, sizeof(percent), "%2ld%%", (100L * part)/total);

    percent[sizeof(percent)-1] = '\0';

    return(percent);
}


/*
 * end_titlebar - free resources associated with titlebar state struct
 */
void
end_titlebar(void)
{
    if(as.folder_name)
      fs_give((void **)&as.folder_name);

    if(as.context_name)
      fs_give((void **)&as.context_name);
}


/*----------------------------------------------------------------------
   Exported method to display status of mail check

   Args: putstr -- should be NO LONGER THAN 2 bytes

 Result: putstr displayed at upper-left-hand corner of screen
  ----*/
void
check_cue_display(char *putstr)
{
    COLOR_PAIR *lastc = NULL, *titlecolor;
    static char check_cue_char;
  
    if(ps_global->read_predicted && 
       (check_cue_char == putstr[0]
	|| (putstr[0] == ' ' && putstr[1] == '\0')))
        return;
    else{
        if(putstr[0] == ' ')
	  check_cue_char = '\0';
	else
	  check_cue_char = putstr[0];
    }

    titlecolor = &as.titlecontainer.color;

    if(titlecolor)
      lastc = pico_set_colorp(titlecolor, PSC_REV|PSC_RET);

    PutLine0(0, 0, putstr);		/* show delay cue */
    if(lastc){
	(void)pico_set_colorp(lastc, PSC_NONE);
	free_color_pair(&lastc);
    }

    fflush(stdout);
}


/*----------------------------------------------------------------------
   Mandatory function of ../pith/newmail.c

   Args: none

 Result: newmail cue displayed at upper-left-hand corner of screen
  ----*/
void
newmail_check_cue(int onoroff)
{
    if(F_ON(F_SHOW_DELAY_CUE, ps_global) && !ps_global->in_init_seq){
	check_cue_display((onoroff == TRUE) ? " *" : "  ");
	MoveCursor(ps_global->ttyo->screen_rows-FOOTER_ROWS(ps_global), 0);
    }

#ifdef _WINDOWS
    if(onoroff)
      mswin_setcursor (MSWIN_CURSOR_BUSY);
    else
      mswin_setcursor (MSWIN_CURSOR_ARROW);
#endif
}


/*----------------------------------------------------------------------
   Mandatory function of ../pith/newmail.c

   Args: none

 Result: checkpoint delay cue displayed at upper-left-hand corner of screen
  ----*/
void
newmail_check_point_cue(int onoroff)
{
    if(F_ON(F_SHOW_DELAY_CUE, ps_global)){
	check_cue_display((onoroff == TRUE) ? "**" : "  ");
	MoveCursor(ps_global->ttyo->screen_rows-FOOTER_ROWS(ps_global), 0);
    }

#ifdef _WINDOWS
    if(onoroff)
      mswin_setcursor (MSWIN_CURSOR_BUSY);
    else
      mswin_setcursor (MSWIN_CURSOR_ARROW);
#endif
}
