#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: text.c 388 2007-01-24 18:57:42Z hubert@u.washington.edu $";
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

/*======================================================================

     text.c
     Implements message text fetching and decoding

  ====*/


#include "../pith/headers.h"
#include "../pith/text.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/handle.h"
#include "../pith/margin.h"
#include "../pith/editorial.h"
#include "../pith/color.h"
#include "../pith/filter.h"
#include "../pith/charset.h"
#include "../pith/status.h"
#include "../pith/mailview.h"
#include "../pith/rfc2231.h"
#include "../pith/detach.h"


/* internal prototypes */
int	 charset_editorial(char *, long, HANDLE_S **, int, int, int, gf_io_t);
int	 delete_quotes(long, char *, LT_INS_S **, void *);
int	 replace_quotes(long, char *, LT_INS_S **, void *);
void     mark_handles_in_line(char *, HANDLE_S **, int);


/*
 * If the number of lines in the quote is equal to lines or less, then
 * we show the quote. If the number of lines in the quote is more than lines,
 * then we show lines-1 of the quote followed by one line of editorial
 * comment.
 */
typedef struct del_q_s {
    int        lines;		/* show this many lines (counting editorial) */
    int        indent_length;	/* skip over this indent                     */
    int        is_flowed;	/* message is labelled flowed                */
    HANDLE_S **handlesp;
    int        in_quote;	/* dynamic data */
    char     **saved_line;	/*   "          */
} DELQ_S;


#define	CHARSET_DISCLAIMER_1	\
	"The following text is in the \"%.40s\" character set.\015\012Your "
#define	CHARSET_DISCLAIMER_2	"display is set"
#define	CHARSET_DISCLAIMER_3	\
       " for the \"%.40s\" character set. \015\012Some %.40scharacters may be displayed incorrectly."
#define ENCODING_DISCLAIMER      \
        "The following text contains the unknown encoding type \"%.20s\". \015\012Some or all of the text may be displayed incorrectly."



/*----------------------------------------------------------------------
   Handle fetching and filtering a text message segment to be displayed
   by scrolltool or printed or exported or piped.

Args: att   -- segment to fetch
      msgno -- message number segment is a part of
      pc    -- function to write characters from segment with
      style -- Indicates special handling for error messages
      flags -- Indicates special necessary handling

Returns: 1 if errors encountered, 0 if everything went A-OK

 ----*/     
int
decode_text(ATTACH_S *att, long int msgno, gf_io_t pc, HANDLE_S **handlesp,
	    DetachErrStyle style, int flags)
{
    FILTLIST_S	filters[14];
    char       *err, *charset;
    int		filtcnt = 0, error_found = 0, column, wrapit;
    int         is_in_sig = OUT_SIG_BLOCK;
    int         is_flowed_msg = 0;
    int         is_delsp_yes = 0;
    int         filt_only_c0 = 0;
    char       *parmval;
    char       *free_this = NULL;
    DELQ_S      dq;

    column = (flags & FM_DISPLAY) ? ps_global->ttyo->screen_cols : 80;

    if(!(flags & FM_DISPLAY))
      flags |= FM_NOINDENT;

    wrapit = column;
    ps_global->some_quoting_was_suppressed = 0;

    memset(filters, 0, sizeof(filters));

    /* charset the body part is in */
    charset = rfc2231_get_param(att->body->parameter, "charset", NULL, NULL);

    /* determined if it's flowed, affects wrapping and quote coloring */
    if(att->body->type == TYPETEXT
       && !strucmp(att->body->subtype, "plain")
       && (parmval = rfc2231_get_param(att->body->parameter,
				       "format", NULL, NULL))){
	if(!strucmp(parmval, "flowed"))
	  is_flowed_msg = 1;
	fs_give((void **) &parmval);

	if(is_flowed_msg){
	    if(parmval = rfc2231_get_param(att->body->parameter,
					   "delsp", NULL, NULL)){
		if(!strucmp(parmval, "yes"))
		  is_delsp_yes = 1;

		fs_give((void **) &parmval);
	    }
	}
    }

    if(flags & (FM_UTF8 | FM_DISPLAY)){
	/* translate message text to UTF-8 */
	flags |= FM_UTF8;
	if(strucmp((char *) charset, "us-ascii") && strucmp((char *) charset, "utf-8")){
	    if(utf8able(charset)){
		filters[filtcnt].filter = gf_utf8;
		filters[filtcnt++].data = gf_utf8_opt(charset);
	    }
	    else{
		/* BUG: what to do if not transliterable? */
		flags &= ~FM_UTF8;
	    }
	}
    }
    else
      wrapit = 0;

    if(!ps_global->pass_ctrl_chars){
	filters[filtcnt++].filter = gf_escape_filter;
	filters[filtcnt].filter = gf_control_filter;

	filt_only_c0 = (ps_global->pass_c1_ctrl_chars || (flags & FM_UTF8)) ? 1 : 0;
	filters[filtcnt++].data = gf_control_filter_opt(&filt_only_c0);
    }

    if(flags & FM_DISPLAY)
      filters[filtcnt++].filter = gf_tag_filter;

    /*
     * if it's just plain old text, look for url's
     */
    if(!(att->body->subtype && strucmp(att->body->subtype, "plain"))){
	struct variable *vars = ps_global->vars;

	if((F_ON(F_VIEW_SEL_URL, ps_global)
	    || F_ON(F_VIEW_SEL_URL_HOST, ps_global)
	    || F_ON(F_SCAN_ADDR, ps_global))
	   && handlesp){

	    /*
	     * The url_hilite filter really ought to come
	     * after flowing, because flowing with the DelSp=yes parameter
	     * can reassemble broken urls back into identifiable urls.
	     * We add the preflow filter to do only the reassembly part
	     * of the flowing so that we can spot the urls.
	     * At this time (2005-03-29) we know that Apple Mail does
	     * send mail like this sometimes. This filter removes the
	     * sequence  SP CRLF  if that seems safe.
	     */
	    if(ps_global->full_header != 2 && is_delsp_yes)
              filters[filtcnt++].filter = gf_preflow;

	    filters[filtcnt].filter = gf_line_test;
	    filters[filtcnt++].data = gf_line_test_opt(url_hilite, handlesp);
	}

	/*
	 * First, paint the signature.
	 * Disclaimers noted below for coloring quotes apply here as well.
	 */
	if((flags & FM_DISPLAY)
	   && !(flags & FM_NOCOLOR)
	   && pico_usingcolor()
	   && VAR_SIGNATURE_FORE_COLOR
	   && VAR_SIGNATURE_BACK_COLOR){
	    filters[filtcnt].filter = gf_line_test;
	    filters[filtcnt++].data = gf_line_test_opt(color_signature,
						       &is_in_sig);
	}

	/*
	 * Gotta be careful with this. The color_a_quote filter adds color
	 * to the beginning and end of the line. This will break some
	 * line_test-style filters which come after it. For example, if they
	 * are looking for something at the start of a line (like color_a_quote
	 * itself). I guess we could fix that by ignoring tags at the
	 * beginning of the line when doing the search.
	 */
	if((flags & FM_DISPLAY)
	   && !(flags & FM_NOCOLOR)
	   && pico_usingcolor()
	   && VAR_QUOTE1_FORE_COLOR
	   && VAR_QUOTE1_BACK_COLOR){
	    filters[filtcnt].filter = gf_line_test;
	    filters[filtcnt++].data = gf_line_test_opt(color_a_quote,
						       &is_flowed_msg);
	}
    }
    else if(!strucmp(att->body->subtype, "richtext")){
	/* maybe strip everything! */
	filters[filtcnt].filter = gf_rich2plain;
	filters[filtcnt++].data = gf_rich2plain_opt(!(flags&FM_DISPLAY));
	/* width to use for file or printer */
	if(wrapit - 5 > 0)
	  wrapit -= 5;
    }
    else if(!strucmp(att->body->subtype, "enriched")){
	filters[filtcnt].filter = gf_enriched2plain;
	filters[filtcnt++].data = gf_enriched2plain_opt(!(flags&FM_DISPLAY));
	/* width to use for file or printer */
	if(wrapit - 5 > 0)
	  wrapit -= 5;
    }
    else if(!strucmp(att->body->subtype, "html")
	    && ps_global->full_header < 2){
/*BUG:	    sniff the params for "version=2.0" ala draft-ietf-html-spec-01 */
	int opts = 0;

	if(flags & FM_DISPLAY){
	    if(handlesp)		/* pass on handles awareness */
	      opts |= GFHP_HANDLES;
	}
	else
	  opts |= GFHP_STRIPPED;	/* don't embed anything! */

	if(flags & FM_NOHTMLREL)
	  opts |= GFHP_NO_RELATIVE;

	if(flags & FM_HTMLRELATED)
	  opts |= GFHP_RELATED_CONTENT;

	wrapit = 0;		/* wrap already handled! */
	filters[filtcnt].filter = gf_html2plain;
	filters[filtcnt++].data = gf_html2plain_opt(NULL, column,
						    (flags & FM_NOINDENT)
						      ? 0
						      : format_view_margin(),
						    handlesp, opts);
    }

    /*
     * If the message is not flowed, we do the quote suppression before
     * the wrapping, because the wrapping does not preserve the quote
     * characters at the beginnings of the lines in that case.
     * Otherwise, we defer until after the wrapping.
     *
     * Also, this is a good place to do quote-replacement on nonflowed
     * messages because no other filters depend on the "> ".
     * Quote-replacement is easier in the flowed case and occurs
     * automatically in the flowed wrapping filter.
     */
    if(!is_flowed_msg
       && ps_global->full_header == 0
       && !(att->body->subtype && strucmp(att->body->subtype, "plain"))
       && (flags & FM_DISPLAY)){
	if(ps_global->quote_suppression_threshold != 0){
	    memset(&dq, 0, sizeof(dq));
	    dq.lines = ps_global->quote_suppression_threshold;
	    dq.is_flowed = is_flowed_msg;
	    dq.indent_length = 0;		/* indent didn't happen yet */
	    dq.saved_line = &free_this;
	    dq.handlesp   = handlesp;

	    filters[filtcnt].filter = gf_line_test;
	    filters[filtcnt++].data = gf_line_test_opt(delete_quotes, &dq);
	}
	if(ps_global->VAR_QUOTE_REPLACE_STRING
	    && F_ON(F_QUOTE_REPLACE_NOFLOW, ps_global)){
	    filters[filtcnt].filter = gf_line_test;
	    filters[filtcnt++].data = gf_line_test_opt(replace_quotes, NULL);
	}
    }

    /*
     * If we're doing wrapping we'd better have converted to
     * UTF-8, because that's what gf_wrap expects.
     */
    if(wrapit && !(flags & FM_NOWRAP)){
	int   margin = 0, wrapflags = (flags & FM_DISPLAY) ? GFW_HANDLES : 0;

	if(flags & FM_DISPLAY
	   && !(flags & FM_NOCOLOR)
	   && pico_usingcolor())
	  wrapflags |= GFW_USECOLOR;

	/* text/flowed (RFC 2646 + draft)? */
	if(ps_global->full_header != 2 && is_flowed_msg){
	    wrapflags |= GFW_FLOWED;

	    if(is_delsp_yes)
	      wrapflags |= GFW_DELSP;
	}

	if(flags & FM_UTF8)
	  wrapflags |= GFW_UTF8;

	filters[filtcnt].filter = gf_wrap;
	filters[filtcnt++].data = gf_wrap_filter_opt(wrapit, column,
						     (flags & FM_NOINDENT)
						       ? 0
						       : format_view_margin(),
						     0, wrapflags);
    }

    /*
     * This has to come after wrapping has happened because the user tells
     * us how many quoted lines to display, and we want that number to be
     * the wrapped lines, not the pre-wrapped lines. We do it before the
     * wrapping if the message is not flowed, because the wrapping does not
     * preserve the quote characters at the beginnings of the lines in that
     * case.
     */
    if(is_flowed_msg
       && ps_global->full_header == 0
       && !(att->body->subtype && strucmp(att->body->subtype, "plain"))
       && (flags & FM_DISPLAY)
       && ps_global->quote_suppression_threshold != 0){
	memset(&dq, 0, sizeof(dq));
	dq.lines = ps_global->quote_suppression_threshold;
	dq.is_flowed = is_flowed_msg;
	dq.indent_length = (wrapit && !(flags & FM_NOWRAP)
			    && !(flags & FM_NOINDENT))
			       ? format_view_margin()[0]
			       : 0;
	dq.saved_line = &free_this;
	dq.handlesp   = handlesp;

	filters[filtcnt].filter = gf_line_test;
	filters[filtcnt++].data = gf_line_test_opt(delete_quotes, &dq);
    }

    if(charset){
	if(F_OFF(F_QUELL_CHARSET_WARNING, ps_global)
	   && !(flags & FM_NOEDITORIAL)){
	    int rv = TRUE;
	    CONV_TABLE *ct = NULL;

	    /*
	     * Need editorial if message charset is not ascii
	     * and the display charset is not either the same
	     * as the message charset or UTF-8.
	     */
	    if(strucmp(charset, "us-ascii")
	       && !((ps_global->display_charmap
		     && !strucmp(charset, ps_global->display_charmap))
	            || !strucmp("UTF-8", ps_global->display_charmap))){

		/*
		 * This is probably overkill. We're just using this
		 * conversion_table routine to get at the quality.
		 */
		if(ps_global->display_charmap)
		  ct = conversion_table(charset, ps_global->display_charmap);

		rv = charset_editorial(charset, msgno, handlesp, flags,
				       ct ? ct->quality : CV_NO_TRANSLATE_POSSIBLE,
				       column, pc);
	    }
	    if(!rv)
	      goto write_error;
	}

	fs_give((void **) &charset);
    }

    /* Format editorial comment about unknown encoding */
    if(att->body->encoding > ENCQUOTEDPRINTABLE){
	char buf[2048];

	snprintf(buf, sizeof(buf), ENCODING_DISCLAIMER, body_encodings[att->body->encoding]);

	if(!(format_editorial(buf, column, flags, handlesp, pc) == NULL
	     && gf_puts(NEWLINE, pc) && gf_puts(NEWLINE, pc)))
	  goto write_error;
    }

    err = detach(ps_global->mail_stream, msgno, att->number, 0L,
		 NULL, pc, filtcnt ? filters : NULL, 0);
    
    delete_unused_handles(handlesp);
    
    if(free_this)
      fs_give((void **) &free_this);
    
    if(err) {
	error_found++;
	if(style == QStatus) {
	    q_status_message1(SM_ORDER, 3, 4, "%.200s", err);
	} else if(style == InLine) {
	    char buftmp[MAILTMPLEN];

	    snprintf(buftmp, sizeof(buftmp), "%s",
		    att->body->description ? att->body->description : "");
	    snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%s   [Error: %s]  %c%s%c%s%s",
		    NEWLINE, err,
		    att->body->description ? '\"' : ' ',
		    att->body->description ? buftmp : "",
		    att->body->description ? '\"' : ' ',
		    NEWLINE, NEWLINE);
	    if(!gf_puts(tmp_20k_buf, pc))
	      goto write_error;
	}
    }

    if(att->body->subtype
       && (!strucmp(att->body->subtype, "richtext")
	   || !strucmp(att->body->subtype, "enriched"))
       && !(flags & FM_DISPLAY)){
	if(!gf_puts(NEWLINE, pc) || !gf_puts(NEWLINE, pc))
	  goto write_error;
    }

    return(error_found);

  write_error:
    if(style == QStatus)
      q_status_message1(SM_ORDER, 3, 4, "Error writing message: %.200s", 
			error_description(errno));

    return(1);
}



/*
 * Format editorial comment about charset mismatch
 */
int
charset_editorial(char *charset, long int msgno, HANDLE_S **handlesp, int flags,
		  int quality, int width, gf_io_t pc)
{
    char     *p, color[64], buf[2048];
    int	      i, n;
    HANDLE_S *h = NULL;

    snprintf(buf, sizeof(buf), CHARSET_DISCLAIMER_1, charset ? charset : "US-ASCII");
    p = &buf[strlen(buf)];

    if(!(ps_global->display_charmap
	 && strucmp(ps_global->display_charmap, "US-ASCII"))
       && handlesp
       && (flags & FM_DISPLAY) == FM_DISPLAY){
	h		      = new_handle(handlesp);
	h->type		      = URL;
	h->h.url.path	      = cpystr("x-pine-help:h_config_char_set");

	/*
	 * Because this filter comes after the delete_quotes filter, we need
	 * to tell delete_quotes that this handle is used, so it won't
	 * delete it. This is a dangerous thing.
	 */
	h->is_used = 1; 

	if(!(flags & FM_NOCOLOR)
	   && scroll_handle_start_color(color, sizeof(color), &n)){
	    for(i = 0; i < n; i++)
	      if(p-buf < sizeof(buf))
	        *p++ = color[i];
	}
	else if(F_OFF(F_SLCTBL_ITEM_NOBOLD, ps_global)){
	    if(p-buf+1 < sizeof(buf)){
	      *p++ = TAG_EMBED;
	      *p++ = TAG_BOLDON;
	    }
	}

	if(p-buf+1 < sizeof(buf)){
	  *p++ = TAG_EMBED;
	  *p++ = TAG_HANDLE;
	}

	snprintf(p + 1, sizeof(buf)-(p+1-buf), "%d", h->key);
	if(p-buf < sizeof(buf))
	  *p  = strlen(p + 1);

	p  += (*p + 1);
    }

    sstrncpy(&p, CHARSET_DISCLAIMER_2, sizeof(buf)-(p-buf));

    if(h){
	/* in case it was the current selection */
	if(p-buf+1 < sizeof(buf)){
	  *p++ = TAG_EMBED;
	  *p++ = TAG_INVOFF;
	}

	if(scroll_handle_end_color(color, sizeof(color), &n)){
	    for(i = 0; i < n; i++)
	      if(p-buf < sizeof(buf))
	        *p++ = color[i];
	}
	else{
	    if(p-buf+1 < sizeof(buf)){
	      *p++ = TAG_EMBED;
	      *p++ = TAG_BOLDOFF;
	    }
	}

	if(p-buf < sizeof(buf))
	  *p = '\0';
    }

    snprintf(p, sizeof(buf)-(p-buf), CHARSET_DISCLAIMER_3,
	    ps_global->display_charmap ?  ps_global->display_charmap : "US-ASCII",
	    (quality == CV_LOSES_SPECIAL_CHARS) ? "special " : "");

    buf[sizeof(buf)-1] = '\0';

    return(format_editorial(buf, width, flags, handlesp, pc) == NULL
	   && gf_puts(NEWLINE, pc) && gf_puts(NEWLINE, pc));
}




/*
 * This one is a little more complicated because it comes late in the
 * filtering sequence after colors have been added and url's hilited and
 * so on. So if it wants to look at the beginning of the line it has to
 * wade through some stuff done by the other filters first. This one is
 * skipping the leading indent added by the viewer-margin stuff and leading
 * tags.
 */
int
delete_quotes(long int linenum, char *line, LT_INS_S **ins, void *local)
{
    DELQ_S *dq;
    char   *lp;
    int     i, lines, not_a_quote = 0;
    size_t  len;

    dq = (DELQ_S *) local;
    if(!dq)
      return(0);

    if(dq->lines > 0 || dq->lines == Q_DEL_ALL){

	lines = (dq->lines == Q_DEL_ALL) ? 0 : dq->lines;

	/*
	 * First determine if this line is part of a quote.
	 */

	/* skip over indent added by viewer-margin-left */
	lp = line;
	for(i = dq->indent_length; i > 0 && !not_a_quote && *lp; i--)
	  if(*lp++ != SPACE)
	    not_a_quote++;
	
	/* skip over leading tags */
	while(!not_a_quote
	      && ((unsigned char) (*lp) == (unsigned char) TAG_EMBED)){
	    switch(*++lp){
	      case '\0':
		not_a_quote++;
	        break;

	      default:
		++lp;
		break;

	      case TAG_FGCOLOR:
	      case TAG_BGCOLOR:
	      case TAG_HANDLE:
		switch(*lp){
		  case TAG_FGCOLOR:
		  case TAG_BGCOLOR:
		    len = RGBLEN + 1;
		    break;

		  case TAG_HANDLE:
		    len = *++lp + 1;
		    break;
		}

		if(strlen(lp) < len)
		  not_a_quote++;
		else
		  lp += len;

		break;

	      case TAG_EMBED:
		break;
	    }
	}

	/* skip over whitespace */
	if(!dq->is_flowed)
	  while(isspace((unsigned char) *lp))
	    lp++;

	/* check first character to see if it is a quote */
	if(!not_a_quote && *lp != '>')
	  not_a_quote++;

	if(not_a_quote){
	    if(dq->in_quote > lines+1){
	      char tmp[500];

	      /*
	       * Display how many lines were hidden.
	       */
	      snprintf(tmp, sizeof(tmp),
		      "%.200s[ %d lines of quoted text hidden from view ]\r\n",
		      repeat_char(dq->indent_length, SPACE),
		      dq->in_quote - lines);
	      if(strlen(tmp)-2 > ps_global->ttyo->screen_cols){

		snprintf(tmp, sizeof(tmp), "%.200s[ %d lines of quoted text hidden ]\r\n",
			repeat_char(dq->indent_length, SPACE),
			dq->in_quote - lines);

		if(strlen(tmp)-2 > ps_global->ttyo->screen_cols){
		  snprintf(tmp, sizeof(tmp), "%.200s[ %d lines hidden ]\r\n",
			  repeat_char(dq->indent_length, SPACE),
			  dq->in_quote - lines);

		  if(strlen(tmp)-2 > ps_global->ttyo->screen_cols){
		    snprintf(tmp, sizeof(tmp), "%.200s[ %d hidden ]\r\n",
			    repeat_char(dq->indent_length, SPACE),
			    dq->in_quote - lines);
		  
		    if(strlen(tmp)-2 > ps_global->ttyo->screen_cols){
		      snprintf(tmp, sizeof(tmp), "%.200s[...]\r\n",
			      repeat_char(dq->indent_length, SPACE));

		      if(strlen(tmp)-2 > ps_global->ttyo->screen_cols){
		        snprintf(tmp, sizeof(tmp), "%.200s...\r\n",
			        repeat_char(dq->indent_length, SPACE));

			if(strlen(tmp)-2 > ps_global->ttyo->screen_cols){
		          snprintf(tmp, sizeof(tmp), "%.200s\r\n",
			      repeat_char(MIN(ps_global->ttyo->screen_cols,3),
					  '.'));
			}
		      }
		    }
		  }
		}
	      }

	      ins = gf_line_test_new_ins(ins, line, tmp, strlen(tmp));
	      mark_handles_in_line(line, dq->handlesp, 1);
	      ps_global->some_quoting_was_suppressed = 1;
	    }
	    else if(dq->in_quote == lines+1
		    && dq->saved_line && *dq->saved_line){
		/*
		 * We would have only had to delete a single line. Instead
		 * of deleting it, just show it.
		 */
		ins = gf_line_test_new_ins(ins, line,
					   *dq->saved_line,
					   strlen(*dq->saved_line));
		mark_handles_in_line(*dq->saved_line, dq->handlesp, 1);
	    }
	    else
	      mark_handles_in_line(line, dq->handlesp, 1);

	    if(dq->saved_line && *dq->saved_line)
	      fs_give((void **) dq->saved_line);

	    dq->in_quote = 0;
	}
	else{
	    dq->in_quote++;				/* count it */
	    if(dq->in_quote > lines){
		/*
		 * If the hidden part is a single line we'll show it instead.
		 */
		if(dq->in_quote == lines+1){
		    if(dq->saved_line && *dq->saved_line)
		      fs_give((void **) dq->saved_line);
		    
		    *dq->saved_line = fs_get(strlen(line) + 3);
		    snprintf(*dq->saved_line, strlen(line)+3, "%s\r\n", line);
		}

		mark_handles_in_line(line, dq->handlesp, 0);
		/* skip it, at least for now */
		return(2);
	    }

	    mark_handles_in_line(line, dq->handlesp, 1);
	}
    }

    return(0);
}


/*
 * This is a line-at-a-time filter that replaces incoming UTF-8 text with
 * ISO-2022-JP text.
 */
int
translate_utf8_to_2022_jp(long int linenum, char *line, LT_INS_S **ins, void *local)
{
    LOC_2022_JP *lpj;
    int ret = 0;

    lpj = (LOC_2022_JP *) local;

    if(line && line[0]){
	char *converted;
	char *last_ptr;
	size_t len;

	converted = utf8_to_charset(line, "ISO-2022-JP", lpj ? lpj->report_err : 0);
	
	if(!converted)
	  ret = -1;

	if(converted && converted != line){
	    /* delete the old line and replace it with the translation */
	    len = strlen(line);
	    ins = gf_line_test_new_ins(ins, line, "", -len);
	    ins = gf_line_test_new_ins(ins, line+len, converted, strlen(converted));
	    fs_give((void **) &converted);
	}
    }

    return ret;
}


/*
 * Replace quotes of nonflowed messages.  This needs to happen
 * towards the end of filtering because a lot of prior filters
 * depend on "> ", such as quote coloring and suppression.
 * Quotes are already colored here, so we have to account for
 * that formatting.
 */
int
replace_quotes(long int linenum, char *line, LT_INS_S **ins, void *local)
{
    char *lp = line, *prefix = NULL, *last_prefix = NULL;
    int no_more_quotes = 0, len, saw_quote = 0;

    if(ps_global->VAR_QUOTE_REPLACE_STRING){
	get_pair(ps_global->VAR_QUOTE_REPLACE_STRING, &prefix, &last_prefix, 0, 0);
	if(!prefix && last_prefix){
	    prefix = last_prefix;
	    last_prefix = NULL;
	}
    }
    else
      return(0);

    while(isspace((unsigned char)(*lp)))
      lp++;
    while(*lp && !no_more_quotes){
	switch(*lp++){
	  case '>':
	    if(*lp == ' '){
		ins = gf_line_test_new_ins(ins, lp - 1, "", -2);
		*lp++;
	    }
	    else
	      ins = gf_line_test_new_ins(ins, lp - 1, "", -1);
	    if(strlen(prefix))
	      ins = gf_line_test_new_ins(ins, lp, prefix, strlen(prefix));
	    saw_quote = 1;
	    break;
	  case TAG_EMBED:
	    switch(*lp++){
	      case TAG_FGCOLOR:
	      case TAG_BGCOLOR:
		len = RGBLEN;
		break;
	      case TAG_HANDLE:
		len = *lp++ + 1;
		break;
	    }
	    if(strlen(lp) < len)
	      no_more_quotes = 1;
	    else
	      lp += len;
	    break;
	  case ' ':
	  case '\t':
	    break;
	  default:
	    if(saw_quote && last_prefix)
	      ins = gf_line_test_new_ins(ins, lp - 1, last_prefix, strlen(last_prefix));
	    no_more_quotes = 1;
	    break;
	}
    }
    if(prefix)
      fs_give((void **)&prefix);
    if(last_prefix)
      fs_give((void **)&last_prefix);
    return(0);
}


/*
 * We need to delete handles that we are removing from the displayed
 * text. But there is a complication. Some handles appear at more than
 * one location in the text. So we keep track of which handles we're
 * actually using as we go, then at the end we delete the ones we
 * didn't use.
 * 
 * Args    line    -- the text line, which includes tags
 *       handlesp  -- handles pointer
 *         used    -- If 1, set handles in this line to used
 *                    if 0, leave handles in this line set as they already are
 */
void
mark_handles_in_line(char *line, HANDLE_S **handlesp, int used)
{
    unsigned char c;
    size_t        length;
    int           key, n;
    HANDLE_S     *h;

    if(!(line && handlesp && *handlesp))
      return;

    length = strlen(line);

    /* mimic code in PutLine0n8b */
    while(length--){

	c = (unsigned char) *line++;

	if(c == (unsigned char) TAG_EMBED && length){

	    length--;

	    switch(*line++){
	      case TAG_HANDLE :
		length -= *line + 1;	/* key length plus length tag */

		for(key = 0, n = *line++; n; n--)
		  key = (key * 10) + (*line++ - '0');

		h = get_handle(*handlesp, key);
		if(h){
		    h->using_is_used = 1;
		    /* 
		     * If it's already marked is_used, leave it marked
		     * that way. We only call this function with used = 0
		     * in order to set using_is_used.
		     */
		    h->is_used = (h->is_used || used) ? 1 : 0;
		}

		break;

	      case TAG_FGCOLOR :
		if(length < RGBLEN){
		    length = 0;
		    break;
		}

		length -= RGBLEN;
		line += RGBLEN;
		break;

	      case TAG_BGCOLOR :
		if(length < RGBLEN){
		    length = 0;
		    break;
		}

		length -= RGBLEN;
		line += RGBLEN;
		break;

	      default:
		break;
	    }
	}	
    }
}
