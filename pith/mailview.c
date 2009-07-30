#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: mailview.c 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $";
#endif

/*
 * ========================================================================
 * Copyright 2006-2009 University of Washington
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
     Implements message data gathering and formatting

  ====*/


#include "headers.h"
#include "../pith/mailview.h"
#include "../pith/conf.h"
#include "../pith/msgno.h"
#include "../pith/editorial.h"
#include "../pith/mimedesc.h"
#include "../pith/margin.h"
#include "../pith/color.h"
#include "../pith/strlst.h"
#include "../pith/charset.h"
#include "../pith/status.h"
#include "../pith/maillist.h"
#include "../pith/mailcmd.h"
#include "../pith/mailindx.h"
#include "../pith/imap.h"
#include "../pith/detach.h"
#include "../pith/text.h"
#include "../pith/url.h"
#include "../pith/rfc2231.h"
#include "../pith/list.h"
#include "../pith/stream.h"
#include "../pith/send.h"
#include "../pith/filter.h"
#include "../pith/string.h"
#include "../pith/ablookup.h"
#include "../pith/escapes.h"
#include "../pith/keyword.h"
#include "../pith/smime.h"


#define FBUF_LEN	(50)

#define	ISRFCEOL(S)    (*(S) == '\015' && *((S)+1) == '\012')


/*
 *  This is a list of header fields that are represented canonically
 *  by the c-client's ENVELOPE structure.  The list is used by the
 *  two functions below to decide if a given field is included in this
 *  set.
 */
static struct envelope_s {
    char *name;
    long val;
} envelope_hdrs[] = {
    {"from",		FE_FROM},
    {"sender",		FE_SENDER},
    {"date",		FE_DATE},
    {"to",		FE_TO},
    {"cc",		FE_CC},
    {"bcc",		FE_BCC},
    {"newsgroups",	FE_NEWSGROUPS},
    {"subject",		FE_SUBJECT},
    {"message-id",	FE_MESSAGEID},
    {"reply-to",	FE_REPLYTO},
    {"followup-to",	FE_FOLLOWUPTO},
    {"in-reply-to",	FE_INREPLYTO},
/*  {"return-path",	FE_RETURNPATH},      not usually filled in */
    {"references",	FE_REFERENCES},
    {NULL,		0}
};


/*
 * Hook for optional display of rfc2369 content
 */
int  (*pith_opt_rfc2369_editorial)(long, HANDLE_S **, int, int, gf_io_t);




/*
 * Internal prototypes
 */
int	    format_blip_seen(long);
int	    is_an_env_hdr(char *);
int         is_an_addr_hdr(char *);
void	    format_env_hdr(MAILSTREAM *, long, char *, ENVELOPE *,
			   fmt_env_t, gf_io_t, char *, char *, int);
int	    delineate_this_header(char *, char *, char **, char **);
char	   *url_embed(int);
int         color_headers(long, char *, LT_INS_S **, void *);
int	    url_hilite_hdr(long, char *, LT_INS_S **, void *);
int	    pad_to_right_edge(long, char *, LT_INS_S **, void *);
int	    url_bogus_imap(char **, char *, char *);
int	    format_raw_header(MAILSTREAM *, long, char *, gf_io_t);
void	    format_envelope(MAILSTREAM *, long, char *, ENVELOPE *,
			    gf_io_t, long, char *, int);
int         any_hdr_color(char *);
void	    format_addr_string(MAILSTREAM *, long, char *, char *,
			       ADDRESS *, int, char *, gf_io_t);
void        pine_rfc822_write_address_noquote(ADDRESS *, gf_io_t, int *);
void	    format_newsgroup_string(char *, char *, int, gf_io_t);
int	    format_raw_hdr_string(char *, char *, gf_io_t, char *, int);
int	    format_env_puts(char *, gf_io_t);
int	    find_field(char **, char *, size_t);
int	    embed_color(COLOR_PAIR *, gf_io_t);
COLOR_PAIR *get_cur_embedded_color(void);
void        clear_cur_embedded_color(void);




/*----------------------------------------------------------------------
   Format a message message for viewing

 Args: msgno -- The number of the message to view
       env   -- pointer to the message's envelope
       body  -- pointer to the message's body
       handlesp -- address of pointer to the message's handles
       flgs  -- possible flags listed in pith/mailview.h with
		prefix FM_
       pc    -- write to this function

Result: Returns true if no problems encountered, else false.

First the envelope is formatted; next a list of all attachments is
formatted if there is more than one. Then all the body parts are
formatted, fetching them as needed. This includes headers of included
message. Richtext is also formatted. An entry is made in the text for
parts that are not displayed or can't be displayed.

 ----*/    
int
format_message(long int msgno, ENVELOPE *env, struct mail_bodystruct *body,
	       HANDLE_S **handlesp, int flgs, gf_io_t pc)
{
    char     *decode_err = NULL;
    HEADER_S  h;
    int       width;

    clear_cur_embedded_color();

    if(!(flgs & FM_DISPLAY))
      flgs |= FM_NOINDENT;

    width = (flgs & FM_DISPLAY) ? ps_global->ttyo->screen_cols : 80;

    /*---- format and copy envelope ----*/
    if(!(flgs & FM_NOEDITORIAL)){
	if(ps_global->full_header == 1)
	  /* TRANSLATORS: User is viewing a message and all the quoted text is
	     being shown. */
	  q_status_message(SM_INFO, 0, 3, _("All quoted text being included"));
	else if(ps_global->full_header == 2)
	  q_status_message(SM_INFO, 0, 3,
			   /* TRANSLATORS: User is viewing a message and all of
			      the header text is being shown. */
			   _("Full header mode ON.  All header text being included"));
    }

    HD_INIT(&h, ps_global->VAR_VIEW_HEADERS, ps_global->view_all_except, FE_DEFAULT);
    switch(format_header(ps_global->mail_stream, msgno, NULL,
			 env, &h, NULL, handlesp, flgs, NULL, pc)){
			      
      case -1 :			/* write error */
	goto write_error;

      case 1 :			/* fetch error */
	if(!(gf_puts("[ Error fetching header ]",  pc)
	     && !gf_puts(NEWLINE, pc)))
	  goto write_error;

	break;
    }

    if(!(body == NULL 
	 || (ps_global->full_header == 2
	     && F_ON(F_ENABLE_FULL_HDR_AND_TEXT, ps_global))))
      format_attachment_list(msgno, body, handlesp, flgs, width, pc);

    /* write delimiter and body */
    if(gf_puts(NEWLINE, pc)
       && (decode_err = format_body(msgno, body, handlesp, &h, flgs, width, pc)) == NULL)
      return(1);


  write_error:

    if(!(flgs & FM_DISPLAY))
      q_status_message1(SM_ORDER, 3, 4, _("Error writing message: %s"),
			decode_err ? decode_err : error_description(errno));

    return(0);
}


char *
format_body(long int msgno, BODY *body, HANDLE_S **handlesp, HEADER_S *hp, int flgs, int width, gf_io_t pc)
{
    int		  filt_only_c0 = 0, wrapflags, error_found = 0;
    int		  is_in_sig = OUT_SIG_BLOCK;
    char	 *charset, *decode_err = NULL, *tmp1, *description;
    ATTACH_S	 *a;
    URL_HILITE_S  uh;
    gf_io_t	  gc;

    if(body == NULL 
       || (ps_global->full_header == 2
	   && F_ON(F_ENABLE_FULL_HDR_AND_TEXT, ps_global))) {

        /*--- Server is not an IMAP2bis, It can't parse MIME
              so we just show the text here. Hopefully the 
              message isn't a MIME message 
          ---*/
	void *text2;

        if((text2 = (void *)pine_mail_fetch_text(ps_global->mail_stream,
						msgno, NULL, NULL, NIL)) != NULL){

 	    if(!gf_puts(NEWLINE, pc))		/* write delimiter */
	      return("Write Error");

	    gf_set_readc(&gc, text2, (unsigned long)strlen(text2), CharStar, 0);
	    gf_filter_init();

	    /*
	     * We need to translate the message
	     * into UTF-8, but that's trouble in the full header case
	     * because we don't know what to translate from. We'll just
	     * take a guess by looking for the first text part and
	     * using its charset.
	     */
	    if(body && body->type == TYPETEXT)
	      charset = parameter_val(body->parameter, "charset");
	    else if(body && body->type == TYPEMULTIPART && body->nested.part
		    && body->nested.part->body.type == TYPETEXT)
	      charset = parameter_val(body->nested.part->body.parameter, "charset");
	    else
	      charset = ps_global->display_charmap;

	    if(strucmp(charset, "us-ascii") && strucmp(charset, "utf-8")){
		/* transliterate message text to UTF-8 */
		gf_link_filter(gf_utf8, gf_utf8_opt(charset));
	    }

	    /* link in filters, similar to what is done in decode_text() */
	    if(!ps_global->pass_ctrl_chars){
		gf_link_filter(gf_escape_filter, NULL);
		filt_only_c0 = 1;
		gf_link_filter(gf_control_filter,
			       gf_control_filter_opt(&filt_only_c0));
	    }

	    gf_link_filter(gf_tag_filter, NULL);

	    if((F_ON(F_VIEW_SEL_URL, ps_global)
		|| F_ON(F_VIEW_SEL_URL_HOST, ps_global)
		|| F_ON(F_SCAN_ADDR, ps_global))
	       && handlesp){
		gf_link_filter(gf_line_test,
			       gf_line_test_opt(url_hilite,
					        gf_url_hilite_opt(&uh,handlesp,0)));
	    }

	    if((flgs & FM_DISPLAY)
	       && !(flgs & FM_NOCOLOR)
	       && pico_usingcolor()
	       && ps_global->VAR_SIGNATURE_FORE_COLOR
	       && ps_global->VAR_SIGNATURE_BACK_COLOR){
		gf_link_filter(gf_line_test, gf_line_test_opt(color_signature, &is_in_sig));
	    }

	    if((flgs & FM_DISPLAY)
	       && !(flgs & FM_NOCOLOR)
	       && pico_usingcolor()
	       && ps_global->VAR_QUOTE1_FORE_COLOR
	       && ps_global->VAR_QUOTE1_BACK_COLOR){
		gf_link_filter(gf_line_test, gf_line_test_opt(color_a_quote, NULL));
	    }

	    if(!(flgs & FM_NOWRAP)){
		wrapflags = (flgs & FM_DISPLAY) ? (GFW_HANDLES|GFW_SOFTHYPHEN) : GFW_NONE;
		if(flgs & FM_DISPLAY
		   && !(flgs & FM_NOCOLOR)
		   && pico_usingcolor())
		  wrapflags |= GFW_USECOLOR;
		gf_link_filter(gf_wrap, gf_wrap_filter_opt(width, width,
							   (flgs & FM_NOINDENT)
							      ? NULL : format_view_margin(),
							   0,
							   wrapflags));
	    }

	    gf_link_filter(gf_nvtnl_local, NULL);
	    if((decode_err = gf_pipe(gc, pc)) != NULL){
		/* TRANSLATORS: There was an error putting together a message for
		   viewing. The arg is the description of the error. */
                snprintf(tmp_20k_buf, SIZEOF_20KBUF, _("Formatting error: %s"), decode_err);
		tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		if(gf_puts(NEWLINE, pc) && gf_puts(NEWLINE, pc)
		   && !format_editorial(tmp_20k_buf, width, flgs, handlesp, pc)
		   && gf_puts(NEWLINE, pc))
		  decode_err = NULL;
		else
		  return(decode_err);
	    }
	}

	if(!text2){
	    if(!gf_puts(NEWLINE, pc)
	       || !gf_puts(_("    [ERROR fetching text of message]"), pc)
	       || !gf_puts(NEWLINE, pc)
	       || !gf_puts(NEWLINE, pc))
	      return("Write Error");
	}
    }
    else{
	int show_parts = 0;

	/*======== Now loop through formatting all the parts =======*/
	for(a = ps_global->atmts; a->description != NULL; a++) {

	    if(a->body->type == TYPEMULTIPART){
#ifdef SMIME
		if(strucmp(a->body->subtype, OUR_PKCS7_ENCLOSURE_SUBTYPE)==0){
		    if(a->description){
			if(!(!format_editorial(a->description, width, flgs, handlesp, pc)
			     && gf_puts(NEWLINE, pc) && gf_puts(NEWLINE, pc)))
			  return("Write Error");
		    }
		}
#endif /* SMIME */
		continue;
	    }

	    if(!a->shown) {
		if(a->suppress_editorial)
		  continue;

		if(!(flgs & FM_NOEDITORIAL)
		   && (!gf_puts(NEWLINE, pc)
		       || (decode_err = part_desc(a->number, a->body,
						  (flgs & FM_DISPLAY)
						  ? (a->can_display != MCD_NONE)
						  ? 1 : 2
						  : 3, width, flgs, pc))))
		  return("Write Error");

		continue;
	    } 

	    switch(a->body->type){

	      case TYPETEXT:
		/*
		 * If a message is multipart *and* the first part of it
		 * is text *and that text is empty, there is a good chance that
		 * there was actually something there that c-client was
		 * unable to parse.  Here we report the empty message body
		 * and insert the raw RFC822.TEXT (if full-headers are
		 * on).
		 */
		if(body->type == TYPEMULTIPART
		   && a == ps_global->atmts
		   && a->body->size.bytes == 0
		   && F_ON(F_ENABLE_FULL_HDR, ps_global)){
		    char *err = NULL;

		    snprintf(tmp_20k_buf, SIZEOF_20KBUF,
			     "Empty or malformed message%s.",
			     ps_global->full_header == 2 
			     ? ". Displaying raw text"
			     : ". Use \"H\" to see raw text");
		    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';

		    if(!(gf_puts(NEWLINE, pc) && gf_puts(NEWLINE, pc)
			 && !format_editorial(tmp_20k_buf, width, flgs, handlesp, pc)
			 && gf_puts(NEWLINE, pc) && gf_puts(NEWLINE, pc)))
		      return("Write Error");

		    if(ps_global->full_header == 2
		       && (err = detach_raw(ps_global->mail_stream, msgno,
					    a->number, pc, flgs))){
			snprintf(tmp_20k_buf, SIZEOF_20KBUF,
				 "%s%s  [ Formatting error: %s ]%s%s",
				 NEWLINE, NEWLINE, err, NEWLINE, NEWLINE);
			tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
			if(!gf_puts(tmp_20k_buf, pc))
			  return("Write Error");
		    }

		    break;
		}

		/*
		 * Don't write our delimiter if this text part is
		 * the first part of a message/rfc822 segment...
		 */
		if(show_parts && a != ps_global->atmts 
		   && !((a[-1].body && a[-1].body->type == TYPEMESSAGE)
#ifdef SMIME
		       || (a[-1].body->type == TYPEMULTIPART
		           && a[-1].body->subtype
			   && (strucmp(a[-1].body->subtype, OUR_PKCS7_ENCLOSURE_SUBTYPE)==0)
			   && &a[-1] != ps_global->atmts
			   && a[-2].body && a[-2].body->type == TYPEMESSAGE)
#endif /* SMIME */
		       )
		   && !(flgs & FM_NOEDITORIAL)){
		    tmp1 = a->body->description ? a->body->description
		      : "Attached Text";
		    description = iutf8ncpy((char *)(tmp_20k_buf+10000),
					    (char *)rfc1522_decode_to_utf8((unsigned char *)(tmp_20k_buf+15000), 5000, tmp1), 5000);
		
		    snprintf(tmp_20k_buf, SIZEOF_20KBUF, "Part %s: \"%.1024s\"", a->number,
			     description);
		    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		    if(!(gf_puts(NEWLINE, pc) && gf_puts(NEWLINE, pc)
			 && !format_editorial(tmp_20k_buf, width, flgs, handlesp, pc)
			 && gf_puts(NEWLINE, pc) && gf_puts(NEWLINE, pc)))
		      return("Write Error");
		}

		error_found += decode_text(a, msgno, pc, handlesp,
					   (flgs & FM_DISPLAY) ? InLine : QStatus,
					   flgs);
		break;

	      case TYPEMESSAGE:
		tmp1 = a->body->description ? a->body->description
		  : (strucmp(a->body->subtype, "delivery-status") == 0)
		  ? "Delivery Status"
		  : "Included Message";
		description = iutf8ncpy((char *)(tmp_20k_buf+10000),
					(char *)rfc1522_decode_to_utf8((unsigned char *)(tmp_20k_buf+15000), 5000, tmp1), 5000);
	    
		snprintf(tmp_20k_buf, SIZEOF_20KBUF, "Part %s: \"%.1024s\"", a->number,
			 description);
		tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';

		if(!(gf_puts(NEWLINE, pc) && gf_puts(NEWLINE, pc)
		     && !format_editorial(tmp_20k_buf, width, flgs, handlesp, pc)
		     && gf_puts(NEWLINE, pc) && gf_puts(NEWLINE, pc)))
		  return("Write Error");

		if(a->body->subtype && strucmp(a->body->subtype, "rfc822") == 0){
		    /* imapenvonly, we may not have all the headers we need */
		    if(a->body->nested.msg->env->imapenvonly)
		      mail_fetch_header(ps_global->mail_stream, msgno,
					a->number, NULL, NULL, FT_PEEK);
		    switch(format_header(ps_global->mail_stream, msgno, a->number,
					 a->body->nested.msg->env, hp,
					 NULL, handlesp, flgs, NULL, pc)){
		      case -1 :			/* write error */
			return("Write Error");

		      case 1 :			/* fetch error */
			if(!(gf_puts("[ Error fetching header ]",  pc)
			     && !gf_puts(NEWLINE, pc)))
			  return("Write Error");

			break;
		    }
		}
		else if(a->body->subtype && strucmp(a->body->subtype, "external-body") == 0){
		    int *margin, avail, m1, m2;

		    avail = width;
		    margin = (flgs & FM_NOINDENT) ? NULL : format_view_margin();

		    m1 = MAX(MIN(margin ? margin[0] : 0, avail), 0);
		    avail -= m1;

		    m2 = MAX(MIN(margin ? margin[1] : 0, avail), 0);
		    avail -= m2;

		    if(format_editorial("This part is not included and can be fetched as follows:", avail, flgs, handlesp, pc)
		       || !gf_puts(NEWLINE, pc)
		       || format_editorial(display_parameters(a->body->parameter), avail, flgs, handlesp, pc))
		      return("Write Error");
		}
		else
		  error_found += decode_text(a, msgno, pc, handlesp,
					     (flgs&FM_DISPLAY) ? InLine : QStatus,
					     flgs);

		if(!gf_puts(NEWLINE, pc))
		  return("Write Error");

		break;

	      default:
		if((decode_err = part_desc(a->number, a->body,
					   (flgs & FM_DISPLAY) ? 1 : 3,
					   width, flgs, pc)) != NULL)
		  return("Write Error");
	    }

	    show_parts++;
	}

	if(!(!error_found
	     && (pith_opt_rfc2369_editorial ? (*pith_opt_rfc2369_editorial)(msgno, handlesp, flgs, width, pc) : 1)
	     && format_blip_seen(msgno)))
	  return("Cannot format body.");
    }

    return(NULL);
}


int
format_attachment_list(long int msgno, BODY *body, HANDLE_S **handlesp, int flgs, int width, gf_io_t pc)
{
    ATTACH_S *a;

    if(flgs & FM_NEW_MESS) {
	zero_atmts(ps_global->atmts);
	describe_mime(body, "", 1, 1, 0, flgs);
    }

    /*----- First do the list of parts/attachments if needed ----*/
    if((flgs & FM_DISPLAY)
       && (ps_global->atmts[1].description
	   || (ps_global->atmts[0].body
	       && ps_global->atmts[0].body->type != TYPETEXT))){
	char tmp[6*MAX_SCREEN_COLS + 1], *tmpp;
	int  i, n, maxnumwid = 0, maxsizewid = 0, *margin;
	int  avail, m1, m2, hwid, s1, s2, s3, s4, s5, dwid, shownwid;
	int  sizewid, descwid, dashwid, partwid, padwid;
	COLOR_PAIR *hdrcolor = NULL;

	if((flgs & FM_DISPLAY)
	   && !(flgs & FM_NOCOLOR)
	   && pico_usingcolor()
	   && ps_global->VAR_HEADER_GENERAL_FORE_COLOR
	   && ps_global->VAR_HEADER_GENERAL_BACK_COLOR
	   && ps_global->VAR_NORM_FORE_COLOR
	   && ps_global->VAR_NORM_BACK_COLOR
	   && (colorcmp(ps_global->VAR_HEADER_GENERAL_FORE_COLOR,
		        ps_global->VAR_NORM_FORE_COLOR)
	       || colorcmp(ps_global->VAR_HEADER_GENERAL_BACK_COLOR,
		        ps_global->VAR_NORM_BACK_COLOR))){

	    if((hdrcolor = new_color_pair(ps_global->VAR_HEADER_GENERAL_FORE_COLOR,
					  ps_global->VAR_HEADER_GENERAL_BACK_COLOR)) != NULL){
		if(!pico_is_good_colorpair(hdrcolor))
		  free_color_pair(&hdrcolor);
	    }
	}

	margin = (flgs & FM_NOINDENT) ? NULL : format_view_margin();

	/*
	 * Attachment list header
	 */

	avail = width;

	m1 = MAX(MIN(margin ? margin[0] : 0, avail), 0);
	avail -= m1;

	m2 = MAX(MIN(margin ? margin[1] : 0, avail), 0);
	avail -= m2;

	hwid = MAX(avail, 0);

	i = utf8_width(_("Parts/Attachments:"));
	partwid = MIN(i, hwid);
	padwid = hdrcolor ? (hwid-partwid) : 0;

	if(m1 > 0){
	    snprintf(tmp, sizeof(tmp), "%*.*s", m1, m1, "");
	    if(!gf_puts(tmp, pc))
	      return(0);
	}

	utf8_snprintf(tmp, sizeof(tmp),
	    "%-*.*w%*.*s",
	    /* TRANSLATORS: A label */
	    partwid, partwid, _("Parts/Attachments:"),
	    padwid, padwid, "");

	if(!((!hdrcolor || embed_color(hdrcolor, pc)) && gf_puts(tmp, pc) && gf_puts(NEWLINE, pc)))
	  return(0);


	/*----- Figure max display widths -----*/
        for(a = ps_global->atmts; a->description != NULL; a++){
	    if((n = utf8_width(a->number)) > maxnumwid)
	      maxnumwid = n;

	    if((n = utf8_width(a->size)) > maxsizewid)
	      maxsizewid = n;
	}

	/*
	 *   ----- adjust max lengths for nice display -----
	 *
	 *  marg _ D _ number _ Shown _ _ _ size _ _ description marg
	 *
	 */

	avail = width - m1 - m2;

	s1 = MAX(MIN(1, avail), 0);
	avail -= s1;

	dwid = MAX(MIN(1, avail), 0);
	avail -= dwid;

	s2 = MAX(MIN(1, avail), 0);
	avail -= s2;

	maxnumwid = MIN(maxnumwid, width/3);
	maxnumwid = MAX(MIN(maxnumwid, avail), 0);
	avail -= maxnumwid;

	s3 = MAX(MIN(1, avail), 0);
	avail -= s3;

	shownwid = MAX(MIN(5, avail), 0);
	avail -= shownwid;

	s4 = MAX(MIN(3, avail), 0);
	avail -= s4;

	sizewid = MAX(MIN(maxsizewid, avail), 0);
	avail -= sizewid;

	s5 = MAX(MIN(2, avail), 0);
	avail -= s5;

	descwid = MAX(0, avail);

	/*----- Format the list of attachments -----*/
	for(a = ps_global->atmts; a->description != NULL; a++){
	    COLOR_PAIR *lastc = NULL;
	    char numbuf[50];
	    int thisdescwid, padwid;

#ifdef SMIME
	    if(a->body->type == TYPEMULTIPART
	       && (strucmp(a->body->subtype, OUR_PKCS7_ENCLOSURE_SUBTYPE)==0))
	      continue;
#endif /* SMIME */

	    i = utf8_width((descwid > 2 && a->description) ? a->description : "");
	    thisdescwid = MIN(i, descwid);
	    padwid = hdrcolor ? (descwid-thisdescwid) : 0;
		
	    if(m1 > 0){
		snprintf(tmp, sizeof(tmp), "%*.*s", m1, m1, "");
		if(!gf_puts(tmp, pc))
		  return(0);
	    }

	    utf8_snprintf(tmp, sizeof(tmp),
		"%*.*s%*.*w%*.*s%-*.*w%*.*s%*.*w%*.*s%*.*w%*.*s%-*.*w",
		s1, s1, "",
		dwid, dwid,
		msgno_part_deleted(ps_global->mail_stream, msgno, a->number) ? "D" : "",
		s2, s2, "",
		maxnumwid, maxnumwid,
		a->number
		    ? short_str(a->number, numbuf, sizeof(numbuf), maxnumwid, FrontDots)
		    : "",
		s3, s3, "",
		shownwid, shownwid,
		a->shown ? "Shown" :
		  (a->can_display != MCD_NONE && !(a->can_display & MCD_EXT_PROMPT))
		    ? "OK " : "",
		s4, s4, "",
		sizewid, sizewid,
		a->size ? a->size : "",
		s5, s5, "",
		thisdescwid, thisdescwid,
		(descwid > 2 && a->description) ? a->description : "");

	    if(!(!hdrcolor || embed_color(hdrcolor, pc)))
	      return(0);

	    if(F_ON(F_VIEW_SEL_ATTACH, ps_global) && handlesp){
		char      buf[16], color[64];
		int	  l;
		HANDLE_S *h;

		for(tmpp = tmp; *tmpp && *tmpp == ' '; tmpp++)
		  if(!(*pc)(' '))
		    return(0);

		h	    = new_handle(handlesp);
		h->type	    = Attach;
		h->h.attach = a;

		snprintf(buf, sizeof(buf), "%d", h->key);
		buf[sizeof(buf)-1] = '\0';

		if(!(flgs & FM_NOCOLOR)
		   && handle_start_color(color, sizeof(color), &l, 1)){
		    lastc = get_cur_embedded_color();
		    if(!gf_nputs(color, (long) l, pc))
		      return(0);
		}
		else if(F_OFF(F_SLCTBL_ITEM_NOBOLD, ps_global)
			&& (!((*pc)(TAG_EMBED) && (*pc)(TAG_BOLDON))))
		  return(0);

		if(!((*pc)(TAG_EMBED) && (*pc)(TAG_HANDLE)
		     && (*pc)(strlen(buf)) && gf_puts(buf, pc)))
		  return(0);
	    }
	    else
	      tmpp = tmp;

	    if(!format_env_puts(tmpp, pc))
	      return(0);

	    if(F_ON(F_VIEW_SEL_ATTACH, ps_global) && handlesp){
		if(lastc){
		    if(F_OFF(F_SLCTBL_ITEM_NOBOLD, ps_global)){
			if(!((*pc)(TAG_EMBED) && (*pc)(TAG_BOLDOFF)))
			  return(0);
		    }

		    if(!embed_color(lastc, pc))
		      return(0);

		    free_color_pair(&lastc);
		}
		else if(!((*pc)(TAG_EMBED) && (*pc)(TAG_BOLDOFF)))
		  return(0);

		if(!((*pc)(TAG_EMBED) && (*pc)(TAG_INVOFF)))
		  return(0);
	    }

	    if(padwid > 0){
		snprintf(tmp, sizeof(tmp), "%*.*s", padwid, padwid, "");
		if(!gf_puts(tmp, pc))
		  return(0);
	    }

	    if(!gf_puts(NEWLINE, pc))
	      return(0);
        }

	/*
	 * Dashed line after list
	 */

	if(hdrcolor){
	    avail = width - m1 - m2;
	    hwid = MAX(avail, 0);

	    dashwid = MAX(MIN(40, hwid-2), 0);
	    padwid = hwid - dashwid;
	    if(m1 > 0){
		snprintf(tmp, sizeof(tmp), "%*.*s", m1, m1, "");
		if(!gf_puts(tmp, pc))
		  return(0);
	    }

	    snprintf(tmp, sizeof(tmp),
		"%s%*.*s",
		repeat_char(dashwid, '-'),
		padwid, padwid, "");
	}
	else{
	    avail = width - m1 -2;

	    dashwid = MAX(MIN(40, avail), 0);
	    avail -= dashwid;

	    snprintf(tmp, sizeof(tmp),
		"%*.*s%s",
		m1, m1, "",
		repeat_char(dashwid, '-'));
	}

	if(!((!hdrcolor || embed_color(hdrcolor, pc)) && gf_puts(tmp, pc) && gf_puts(NEWLINE, pc)))
	  return(0);

	if(hdrcolor)
	  free_color_pair(&hdrcolor);
    }

    return(1);
}



/*
 * format_blip_seen - if seen bit (which is usually cleared as a side-effect
 *		      of body part fetches as we're formatting) for the
 *		      given message isn't set (likely because there
 *		      weren't any parts suitable for display), then make
 *		      sure to set it here.
 */
int
format_blip_seen(long int msgno)
{
    MESSAGECACHE *mc;

    if(msgno > 0L && ps_global->mail_stream
       && msgno <= ps_global->mail_stream->nmsgs
       && (mc = mail_elt(ps_global->mail_stream, msgno))
       && !mc->seen
       && !ps_global->mail_stream->rdonly)
      mail_flag(ps_global->mail_stream, long2string(msgno), "\\SEEN", ST_SET);

    return(1);
}


/*
 * is_an_env_hdr - is this name a header in the envelope structure?
 *
 *             name - the header name to check
 */
int
is_an_env_hdr(char *name)
{
    register int i;

    for(i = 0; envelope_hdrs[i].name; i++)
      if(!strucmp(name, envelope_hdrs[i].name))
	return(1);

    return(0);
}




/*
 * is_an_addr_hdr - is this an address header?
 *
 *             name - the header name to check
 */
int
is_an_addr_hdr(char *fieldname)
{
    char   fbuf[FBUF_LEN+1];
    char  *colon, *fname;
    static char *addr_headers[] = {
	"from",
	"reply-to",
	"to",
	"cc",
	"bcc",
	"return-path",
	"sender",
	"x-sender",
	"x-x-sender",
	"resent-from",
	"resent-to",
	"resent-cc",
	NULL
    };

    /* so it is pointing to NULL */
    char **p = addr_headers + sizeof(addr_headers)/sizeof(*addr_headers) - 1;

    if((colon = strindex(fieldname, ':')) != NULL){
	strncpy(fbuf, fieldname, MIN(colon-fieldname,sizeof(fbuf)));
	fbuf[MIN(colon-fieldname,sizeof(fbuf)-1)] = '\0';
	fname = fbuf;
    }
    else
      fname = fieldname;

    if(fname && *fname){
	for(p = addr_headers; *p; p++)
	  if(!strucmp(fname, *p))
	    break;
    }

    return((*p) ? 1 : 0);
}


/*
 * Format a single field from the envelope
 */
void
format_env_hdr(MAILSTREAM *stream, long int msgno, char *section, ENVELOPE *env,
	       fmt_env_t fmt_env, gf_io_t pc, char *field, char *oacs, int flags)
{
    register int i;

    if(!fmt_env)
      fmt_env = format_envelope;

    for(i = 0; envelope_hdrs[i].name; i++)
      if(!strucmp(field, envelope_hdrs[i].name)){
	  (*fmt_env)(stream, msgno, section, env, pc, envelope_hdrs[i].val, oacs, flags);
	  return;
      }
}


/*
 * Look through header string beginning with "begin", for the next
 * occurrence of header "field".  Set "start" to that.  Set "end" to point one
 * position past all of the continuation lines that go with "field".
 * That is, if "end" is converted to a null
 * character then the string "start" will be the next occurence of header
 * "field" including all of its continuation lines. Assume we
 * have CRLF's as end of lines.
 *
 * If "field" is NULL, then we just leave "start" pointing to "begin" and
 * make "end" the end of that header.
 *
 * Returns 1 if found, 0 if not.
 */
int
delineate_this_header(char *field, char *begin, char **start, char **end)
{
    char tmpfield[MAILTMPLEN+2]; /* copy of field with colon appended */
    char *p;
    char *begin_srch;

    if(field == NULL){
	if(!begin || !*begin || isspace((unsigned char)*begin))
	  return 0;
	else
	  *start = begin;
    }
    else{
	strncpy(tmpfield, field, sizeof(tmpfield)-2);
	tmpfield[sizeof(tmpfield)-2] = '\0';
	strncat(tmpfield, ":", sizeof(tmpfield)-strlen(tmpfield)-1);
	tmpfield[sizeof(tmpfield)-1] = '\0';

	/*
	 * We require that start is at the beginning of a line, so
	 * either it equals begin (which we assume is the beginning of a
	 * line) or it is preceded by a CRLF.
	 */
	begin_srch = begin;
	*start = srchstr(begin_srch, tmpfield);
	while(*start && *start != begin
	             && !(*start - 2 >= begin && ISRFCEOL(*start - 2))){
	    begin_srch = *start + 1;
	    *start = srchstr(begin_srch, tmpfield);
	}

	if(!*start)
	  return 0;
    }

    for(p = *start; *p; p++){
	if(ISRFCEOL(p)
	   && (!isspace((unsigned char)*(p+2)) || *(p+2) == '\015')){
	    /*
	     * The final 015 in the test above is to test for the end
	     * of the headers.
	     */
	    *end = p+2;
	    break;
	}
    }

    if(!*p)
      *end = p;
    
    return 1;
}



int
handle_start_color(char *colorstring, size_t buflen, int *len, int use_hdr_color)
{
    *len = 0;

    if(pico_usingcolor()){
	char *fg = NULL, *bg = NULL, *s;
	char *basefg = NULL, *basebg = NULL;

	basefg = use_hdr_color	? ps_global->VAR_HEADER_GENERAL_FORE_COLOR
				: ps_global->VAR_NORM_FORE_COLOR;
	basebg = use_hdr_color	? ps_global->VAR_HEADER_GENERAL_BACK_COLOR
				: ps_global->VAR_NORM_BACK_COLOR;

	if(ps_global->VAR_SLCTBL_FORE_COLOR
	   && colorcmp(ps_global->VAR_SLCTBL_FORE_COLOR, basefg))
	  fg = ps_global->VAR_SLCTBL_FORE_COLOR;

	if(ps_global->VAR_SLCTBL_BACK_COLOR
	   && colorcmp(ps_global->VAR_SLCTBL_BACK_COLOR, basebg))
	  bg = ps_global->VAR_SLCTBL_BACK_COLOR;

	if(bg || fg){
	    COLOR_PAIR *tmp;

	    /*
	     * The blacks are just known good colors for
	     * testing whether the other color is good.
	     */
	    if((tmp = new_color_pair(fg ? fg : colorx(COL_BLACK),
				     bg ? bg : colorx(COL_BLACK))) != NULL){
		if(pico_is_good_colorpair(tmp))
		  for(s = color_embed(fg, bg);
		      (*len) < buflen && (colorstring[*len] = *s);
		      s++, (*len)++)
		    ;

		free_color_pair(&tmp);
	    }
	}

	if(F_OFF(F_SLCTBL_ITEM_NOBOLD, ps_global)){
	    strncpy(colorstring + (*len), url_embed(TAG_BOLDON), MIN(3,buflen-(*len)));
	    *len += 2;
	}
    }

    colorstring[buflen-1] = '\0';

    return(*len != 0);
}


int
handle_end_color(char *colorstring, size_t buflen, int *len)
{
    *len = 0;
    if(pico_usingcolor()){
	char *fg = NULL, *bg = NULL, *s;

	/*
	 * We need to change the fg and bg colors back even if they
	 * are the same as the Normal Colors so that color_a_quote
	 * will have a chance to pick up those colors as the ones to
	 * switch to. We don't do this before the handle above so that
	 * the quote color will flow into the selectable item when
	 * the selectable item color is partly the same as the
	 * normal color. That is, suppose the normal color was black on
	 * cyan and the selectable color was blue on cyan, only a fg color
	 * change. We preserve the only-a-fg-color-change in a quote by
	 * letting the quote background color flow into the selectable text.
	 */
	if(ps_global->VAR_SLCTBL_FORE_COLOR)
	  fg = ps_global->VAR_NORM_FORE_COLOR;

	if(ps_global->VAR_SLCTBL_BACK_COLOR)
	  bg = ps_global->VAR_NORM_BACK_COLOR;

	if(F_OFF(F_SLCTBL_ITEM_NOBOLD, ps_global)){
	    strncpy(colorstring, url_embed(TAG_BOLDOFF), MIN(3,buflen));
	    *len = 2;
	}

	if(fg || bg)
	  for(s = color_embed(fg, bg); (*len) < buflen && (colorstring[*len] = *s); s++, (*len)++)
	    ;
    }

    colorstring[buflen-1] = '\0';

    return(*len != 0);
}


char *
url_embed(int embed)
{
    static char buf[3] = {TAG_EMBED};
    buf[1] = embed;
    buf[2] = '\0';
    return(buf);
}


/*
 * Paint the signature.
 */
int
color_signature(long int linenum, char *line, LT_INS_S **ins, void *is_in_sig)
{
    struct variable *vars = ps_global->vars;
    int             *in_sig_block;
    COLOR_PAIR      *col = NULL;

    if(is_in_sig == NULL)
      return 0;

    in_sig_block = (int *) is_in_sig;
    
    if(!strcmp(line, SIGDASHES))
      *in_sig_block = START_SIG_BLOCK; 
    else if(*line == '\0')
      /* 
       * Suggested by Eduardo: allow for a blank line right after 
       * the sigdashes. 
       */
      *in_sig_block = (*in_sig_block == START_SIG_BLOCK)
			  ? IN_SIG_BLOCK : OUT_SIG_BLOCK;
    else
      *in_sig_block = (*in_sig_block != OUT_SIG_BLOCK)
			  ? IN_SIG_BLOCK : OUT_SIG_BLOCK;

    if(*in_sig_block != OUT_SIG_BLOCK
       && VAR_SIGNATURE_FORE_COLOR && VAR_SIGNATURE_BACK_COLOR
       && (col = new_color_pair(VAR_SIGNATURE_FORE_COLOR,
				VAR_SIGNATURE_BACK_COLOR))){
	if(!pico_is_good_colorpair(col))
	  free_color_pair(&col);
    }
    
    if(col){
	char *p, fg[RGBLEN + 1], bg[RGBLEN + 1], rgbbuf[RGBLEN + 1];

	ins = gf_line_test_new_ins(ins, line,
				   color_embed(col->fg, col->bg),
				   (2 * RGBLEN) + 4);

        strncpy(fg, color_to_asciirgb(VAR_NORM_FORE_COLOR), sizeof(fg));
	fg[sizeof(fg)-1] = '\0';
	strncpy(bg, color_to_asciirgb(VAR_NORM_BACK_COLOR), sizeof(bg));
	bg[sizeof(bg)-1] = '\0';

	/*
	 * Loop watching colors, and override with 
	 * signature color whenever the normal foreground and background
	 * colors are in force.
	 */

	for(p = line; *p; )
	  if(*p++ == TAG_EMBED){

	      switch(*p++){
		case TAG_HANDLE :
		  p += *p + 1;	/* skip handle key */
		  break;

		case TAG_FGCOLOR :
		  snprintf(rgbbuf, sizeof(rgbbuf), "%.*s", RGBLEN, p);
		  rgbbuf[sizeof(rgbbuf)-1] = '\0';
		  p += RGBLEN;	/* advance past color value */
		  
		  if(!colorcmp(rgbbuf, VAR_NORM_FORE_COLOR)
		     && !colorcmp(bg, VAR_NORM_BACK_COLOR))
		    ins = gf_line_test_new_ins(ins, p,
					       color_embed(col->fg,NULL),
					       RGBLEN + 2);
		  break;

		case TAG_BGCOLOR :
		  snprintf(rgbbuf, sizeof(rgbbuf), "%.*s", RGBLEN, p);
		  rgbbuf[sizeof(rgbbuf)-1] = '\0';
		  p += RGBLEN;	/* advance past color value */
		  
		  if(!colorcmp(rgbbuf, VAR_NORM_BACK_COLOR)
		     && !colorcmp(fg, VAR_NORM_FORE_COLOR))
		    ins = gf_line_test_new_ins(ins, p,
					       color_embed(NULL,col->bg),
					       RGBLEN + 2);

		  break;

		default :
		  break;
	      }
	  }
 
	ins = gf_line_test_new_ins(ins, line + strlen(line),
				   color_embed(VAR_NORM_FORE_COLOR,
					       VAR_NORM_BACK_COLOR),
				   (2 * RGBLEN) + 4);
        free_color_pair(&col);
    }

    return 0;
}


/*
 * Line filter to add color to displayed headers.
 */
int
color_headers(long int linenum, char *line, LT_INS_S **ins, void *local)
{
    static char field[FBUF_LEN + 1];
    char        fg[RGBLEN + 1], bg[RGBLEN + 1], rgbbuf[RGBLEN + 1];
    char       *p, *q, *value, *beg;
    COLOR_PAIR *color;
    int         in_quote = 0, in_comment = 0, did_color = 0;
    struct variable *vars = ps_global->vars;

    field[FBUF_LEN] = '\0';

    if(isspace((unsigned char)*line))		/* continuation line */
      value = line;
    else{
	if(!(value = strindex(line, ':')))
	  return(0);
	
	memset(field, 0, sizeof(field));
	strncpy(field, line, MIN(value-line, sizeof(field)-1));
    }

    for(value++; isspace((unsigned char)*value); value++)
      ;

    strncpy(fg, color_to_asciirgb(VAR_HEADER_GENERAL_FORE_COLOR), sizeof(fg));
    fg[sizeof(fg)-1] = '\0';
    strncpy(bg, color_to_asciirgb(VAR_HEADER_GENERAL_BACK_COLOR), sizeof(bg));
    bg[sizeof(bg)-1] = '\0';

    /*
     * Split into two cases depending on whether this is a header which
     * contains addresses or not. We may color addresses separately.
     */
    if(is_an_addr_hdr(field)){

	/*
	 * If none of the patterns are for this header, don't bother parsing
	 * and checking each address.
	 */
	if(!any_hdr_color(field))
	  return(0);

	/*
	 * First check for patternless patterns which color whole line.
	 */
	if((color = hdr_color(field, NULL, ps_global->hdr_colors)) != NULL){
	    if(pico_is_good_colorpair(color)){
		ins = gf_line_test_new_ins(ins, value,
					   color_embed(color->fg, color->bg),
					   (2 * RGBLEN) + 4);
		strncpy(fg, color_to_asciirgb(color->fg), sizeof(fg));
		fg[sizeof(fg)-1] = '\0';
		strncpy(bg, color_to_asciirgb(color->bg), sizeof(bg));
		bg[sizeof(bg)-1] = '\0';
		did_color++;
	    }
	    else
	      free_color_pair(&color);
	}

	/*
	 * Then go through checking address by address.
	 * Keep track of quotes and watch for color changes, and override
	 * with most recent header color whenever the normal foreground
	 * and background colors are in force.
	 */
	beg = p = value;
	while(*p){
	    switch(*p){
	      case '\\':
		/* skip next character */
		if(*(p+1) && (in_comment || in_quote))
		  p += 2;
		else
		  p++;

		break;

	      case '"':
		if(!in_comment)
		  in_quote = 1 - in_quote;

		p++;
		break;

	      case '(':
		in_comment++;
		p++;
		break;

	      case ')':
		if(in_comment > 0)
		  in_comment--;

		p++;
		break;

	      case ',':
		if(!(in_quote || in_comment)){
		    /* we reached the end of this address */   
		    *p = '\0';
		    if(color)
		      free_color_pair(&color);

		    if((color = hdr_color(field, beg,
					  ps_global->hdr_colors)) != NULL){
			if(pico_is_good_colorpair(color)){
			    did_color++;
			    ins = gf_line_test_new_ins(ins, beg,
						       color_embed(color->fg,
								   color->bg),
						       (2 * RGBLEN) + 4);
			    *p = ',';
			    for(q = p; q > beg && 
				  isspace((unsigned char)*(q-1)); q--)
			      ;

			    ins = gf_line_test_new_ins(ins, q,
						       color_embed(fg, bg),
						       (2 * RGBLEN) + 4);
			}
			else
			  free_color_pair(&color);
		    }
		    else
		      *p = ',';
		    
		    for(p++; isspace((unsigned char)*p); p++)
		      ;
		    
		    beg = p;
		}
		else
		  p++;

		break;
	    
	      case TAG_EMBED:
		switch(*(++p)){
		  case TAG_HANDLE:
		    p++;
		    p += *p + 1;	/* skip handle key */
		    break;

		  case TAG_FGCOLOR:
		    p++;
		    snprintf(rgbbuf, sizeof(rgbbuf), "%.*s", RGBLEN, p);
		    rgbbuf[sizeof(rgbbuf)-1] = '\0';
		    p += RGBLEN;	/* advance past color value */
		  
		    if(!colorcmp(rgbbuf, VAR_HEADER_GENERAL_FORE_COLOR))
		      ins = gf_line_test_new_ins(ins, p,
					         color_embed(color->fg,NULL),
					         RGBLEN + 2);
		    break;

		  case TAG_BGCOLOR:
		    p++;
		    snprintf(rgbbuf, sizeof(rgbbuf), "%.*s", RGBLEN, p);
		    rgbbuf[sizeof(rgbbuf)-1] = '\0';
		    p += RGBLEN;	/* advance past color value */
		  
		    if(!colorcmp(rgbbuf, VAR_HEADER_GENERAL_BACK_COLOR))
		      ins = gf_line_test_new_ins(ins, p,
					         color_embed(NULL,color->bg),
					         RGBLEN + 2);

		    break;

		  default:
		    break;
	        }

		break;

	      default:
		p++;
		break;
	    }
	}

	for(q = beg; *q && isspace((unsigned char)*q); q++)
	  ;

	if(*q && !(in_quote || in_comment)){
	    /* we reached the end of this address */   
	    if(color)
	      free_color_pair(&color);

	    if((color = hdr_color(field, beg, ps_global->hdr_colors)) != NULL){
		if(pico_is_good_colorpair(color)){
		    did_color++;
		    ins = gf_line_test_new_ins(ins, beg,
					       color_embed(color->fg,
							   color->bg),
					       (2 * RGBLEN) + 4);
		    for(q = p; q > beg && isspace((unsigned char)*(q-1)); q--)
		      ;

		    ins = gf_line_test_new_ins(ins, q,
					       color_embed(fg, bg),
					       (2 * RGBLEN) + 4);
		}
		else
		  free_color_pair(&color);
	    }
	}

	if(color)
	  free_color_pair(&color);

	if(did_color)
	  ins = gf_line_test_new_ins(ins, line + strlen(line),
				     color_embed(VAR_HEADER_GENERAL_FORE_COLOR,
					         VAR_HEADER_GENERAL_BACK_COLOR),
				     (2 * RGBLEN) + 4);
    }
    else{

	color = hdr_color(field, value, ps_global->hdr_colors);

	if(color){
	    if(pico_is_good_colorpair(color)){
		ins = gf_line_test_new_ins(ins, value,
					   color_embed(color->fg, color->bg),
					   (2 * RGBLEN) + 4);

		/*
		 * Loop watching colors, and override with header
		 * color whenever the normal foreground and background
		 * colors are in force.
		 */
		p = value;
		while(*p)
		  if(*p++ == TAG_EMBED){

		      switch(*p++){
			case TAG_HANDLE:
			  p += *p + 1;	/* skip handle key */
			  break;

			case TAG_FGCOLOR:
			  snprintf(rgbbuf, sizeof(rgbbuf), "%.*s", RGBLEN, p);
			  rgbbuf[sizeof(rgbbuf)-1] = '\0';
			  p += RGBLEN;	/* advance past color value */
			  
			  if(!colorcmp(rgbbuf, VAR_HEADER_GENERAL_FORE_COLOR)
			     && !colorcmp(bg, VAR_HEADER_GENERAL_BACK_COLOR))
			    ins = gf_line_test_new_ins(ins, p,
						       color_embed(color->fg,NULL),
						       RGBLEN + 2);
			  break;

			case TAG_BGCOLOR:
			  snprintf(rgbbuf, sizeof(rgbbuf), "%.*s", RGBLEN, p);
			  rgbbuf[sizeof(rgbbuf)-1] = '\0';
			  p += RGBLEN;	/* advance past color value */
			  
			  if(!colorcmp(rgbbuf, VAR_HEADER_GENERAL_BACK_COLOR)
			     && !colorcmp(fg, VAR_HEADER_GENERAL_FORE_COLOR))
			    ins = gf_line_test_new_ins(ins, p,
						       color_embed(NULL,color->bg),
						       RGBLEN + 2);

			  break;

			default:
			  break;
		      }
		  }

		ins = gf_line_test_new_ins(ins, line + strlen(line),
					   color_embed(VAR_HEADER_GENERAL_FORE_COLOR,
						       VAR_HEADER_GENERAL_BACK_COLOR),
					   (2 * RGBLEN) + 4);
	    }

	    free_color_pair(&color);
	}
    }

    return(0);
}


int
url_hilite(long int linenum, char *line, LT_INS_S **ins, void *local)
{
    register char *lp, *up = NULL, *urlp = NULL,
		  *weburlp = NULL, *mailurlp = NULL;
    int		   n, n1, n2, n3, l;
    char	   buf[256], color[256];
    HANDLE_S	  *h;
    URL_HILITE_S  *uh;

    for(lp = line; ; lp = up + n){
	/* scan for all of them so we can choose the first */
	if(F_ON(F_VIEW_SEL_URL,ps_global))
	  urlp = rfc1738_scan(lp, &n1);
	if(F_ON(F_VIEW_SEL_URL_HOST,ps_global))
	  weburlp = web_host_scan(lp, &n2);
	if(F_ON(F_SCAN_ADDR,ps_global))
	  mailurlp = mail_addr_scan(lp, &n3);
	
	if(urlp || weburlp || mailurlp){
	    up = urlp ? urlp : 
		  weburlp ? weburlp : mailurlp;
	    if(up == urlp && weburlp && weburlp < up)
	      up = weburlp;
	    if(mailurlp && mailurlp < up)
	      up = mailurlp;

	    if(up == urlp){
		n = n1;
		weburlp = mailurlp = NULL;
	    }
	    else if(up == weburlp){
		n = n2;
		mailurlp = NULL;
	    }
	    else{
		n = n3;
		weburlp = NULL;
	    }
	}
	else
	  break;

	uh = (URL_HILITE_S *) local;

	h	      = new_handle(uh->handlesp);
	h->type	      = URL;
	h->h.url.path = (char *) fs_get((n + 10) * sizeof(char));
	snprintf(h->h.url.path, n+10, "%s%.*s",
		weburlp ? "http://" : (mailurlp ? "mailto:" : ""), n, up);
	h->h.url.path[n+10-1] = '\0';

	if(handle_start_color(color, sizeof(color), &l, uh->hdr_color))
	  ins = gf_line_test_new_ins(ins, up, color, l);
	else if(F_OFF(F_SLCTBL_ITEM_NOBOLD, ps_global))
	  ins = gf_line_test_new_ins(ins, up, url_embed(TAG_BOLDON), 2);

	buf[0] = TAG_EMBED;
	buf[1] = TAG_HANDLE;
	snprintf(&buf[3], sizeof(buf)-3, "%d", h->key);
	buf[sizeof(buf)-1] = '\0';
	buf[2] = strlen(&buf[3]);
	ins = gf_line_test_new_ins(ins, up, buf, (int) buf[2] + 3);

	/* in case it was the current selection */
	ins = gf_line_test_new_ins(ins, up + n, url_embed(TAG_INVOFF), 2);

	if(scroll_handle_end_color(color, sizeof(color), &l, uh->hdr_color))
	  ins = gf_line_test_new_ins(ins, up + n, color, l);
	else
	  ins = gf_line_test_new_ins(ins, up + n, url_embed(TAG_BOLDOFF), 2);

	urlp = weburlp = mailurlp = NULL;
    }

    return(0);
}


int
url_hilite_hdr(long int linenum, char *line, LT_INS_S **ins, void *local)
{
    static int     check_for_urls = 0;
    register char *lp;

    if(isspace((unsigned char)*line))	/* continuation, check or not 
					   depending on last line */
      lp = line;
    else{
	check_for_urls = 0;
	if((lp = strchr(line, ':')) != NULL){	/* there ought to always be a colon */
	    FieldType ft;

	    *lp = '\0';

	    if(((ft = pine_header_standard(line)) == FreeText
		|| ft == Subject
		|| ft == TypeUnknown)
	       && strucmp(line, "message-id")
	       && strucmp(line, "newsgroups")
	       && strucmp(line, "references")
	       && strucmp(line, "in-reply-to")
	       && strucmp(line, "received")
	       && strucmp(line, "date")){
		check_for_urls = 1;
	    }

	    *lp = ':';
	}
    }

    if(check_for_urls)
      (void) url_hilite(linenum, lp + 1, ins, local);

    return(0);
}


int
pad_to_right_edge(long int linenum, char *line, LT_INS_S **ins, void *local)
{
    char *p;
    int wid = 0;
    int total_wid;
    struct variable *vars = ps_global->vars;

    if(!line[0])
      return 0;

    total_wid = *((int *) local);

    /* calculate width of line */
    p = line;
    while(*p){

	switch(*p){
	  case TAG_EMBED:
	    p++;
	    switch(*p){
	      case TAG_HANDLE:
		p++;
		p += *p + 1;  /* skip handle key */
		break;

	      case TAG_FGCOLOR :
	      case TAG_BGCOLOR :
		p += (RGBLEN + 1);	/* 1 for TAG, RGBLEN for color */
		break;

	      case TAG_INVON:
	      case TAG_INVOFF:
	      case TAG_BOLDON:
	      case TAG_BOLDOFF:
	      case TAG_ULINEON:
	      case TAG_ULINEOFF:
		p++;
		break;
	    
	      default:	/* literal embed char */
		break;
	    }

	    break;

	  case TAB:
	    p++;
	    while(((++wid) &  0x07) != 0) /* add tab's spaces */
	      ;

	    break;

	  default:
	    wid += width_at_this_position((unsigned char *) p, strlen(p));
	    p++;
	    break;
	}
    }

    if(total_wid > wid){
	ins = gf_line_test_new_ins(ins, line + strlen(line),
				   color_embed(VAR_HEADER_GENERAL_FORE_COLOR,
					       VAR_HEADER_GENERAL_BACK_COLOR),
				   (2 * RGBLEN) + 4);
	ins = gf_line_test_new_ins(ins, line+strlen(line),
				   repeat_char(total_wid-wid, ' '), total_wid-wid);
	ins = gf_line_test_new_ins(ins, line + strlen(line),
				   color_embed(VAR_NORM_FORE_COLOR,
					       VAR_NORM_BACK_COLOR),
				   (2 * RGBLEN) + 4);
    }

    return(0);
}



#define	UES_LEN	12
#define	UES_MAX	32
int
url_external_specific_handler(char *url, int len)
{
    static char list[UES_LEN * UES_MAX];

    if(url){
	char *p;
	int   i;

	for(i = 0; i < UES_MAX && *(p = &list[i * UES_LEN]); i++)
	  if(strlen(p) == len && !struncmp(p, url, len))
	    return(1);
    }
    else{					/* initialize! */
	char **l, *test, *cmd, *p, *p2;
	int    i = 0, n;

	memset(list, 0, sizeof(list));
	for(l = ps_global->VAR_BROWSER ; l && *l; l++){
	    get_pair(*l, &test, &cmd, 1, 1);

	    if((p = srchstr(test, "_scheme(")) && (p2 = strstr(p+8, ")_"))){
		*p2 = '\0';

		for(p += 8; *p && i < UES_MAX; p += n)
		  if((p2 = strchr(p, ',')) != NULL){
		      if((n = p2 - p) < UES_LEN){
			strncpy(&list[i * UES_LEN], p, MIN(n, sizeof(list)-(i * UES_LEN)));
			i++;
		      }
		      else
			dprint((1,
				   "* * * HANLDER TOO LONG: %.*s\n", n,
				   p ? p : "?"));

		      n++;
		  }
		  else{
		      if(strlen(p) <= UES_LEN){
			strncpy(&list[i * UES_LEN], p, sizeof(list)-(i * UES_LEN));
			i++;
		      }

		      break;
		  }
	    }

	    if(test)
	      fs_give((void **) &test);

	    if(cmd)
	      fs_give((void **) &cmd);
	}
    }

    return(0);
}


int
url_imap_folder(char *true_url, char **folder, imapuid_t *uid_val,
		imapuid_t *uid, char **search, int silent)
{
    char *url, *scheme, *p, *cmd, *server = NULL,
	 *user = NULL, *auth = NULL, *mailbox = NULL,
	 *section = NULL;
    size_t l;
    int   rv = URL_IMAP_ERROR;

    /*
     * Since we're planting nulls, operate on a temporary copy...
     */
    scheme = silent ? NULL : "IMAP";
    url = cpystr(true_url + 7);

    /* Try to pick apart the "iserver" portion */
    if((cmd = strchr(url, '/')) != NULL){		/* iserver "/" [mailbox] ? */
	*cmd++ = '\0';
    }
    else{
	dprint((2, "-- URL IMAP FOLDER: missing: %s\n",
	       url ? url : "?"));
	cmd = &url[strlen(url)-1];	/* assume only iserver */
    }

    if((p = strchr(url, '@')) != NULL){		/* user | auth | pass? */
	*p++ = '\0';
	server = rfc1738_str(p);
	
	/* only ";auth=*" supported (and also ";auth=anonymous") */
	if((p = srchstr(url, ";auth=")) != NULL){
	    *p = '\0';
	    auth = rfc1738_str(p + 6);
	}

	if(*url)
	  user = rfc1738_str(url);
    }
    else
      server = rfc1738_str(url);

    if(!*server)
      return(url_bogus_imap(&url, scheme, "No server specified"));

    /*
     * "iserver" in hand, pick apart the "icommand"...
     */
    p = NULL;
    if(!*cmd || (p = srchstr(cmd, ";type="))){
	char *criteria;

	/*
	 * No "icommand" (all top-level folders) or "imailboxlist"...
	 */
	if(p){
	    *p	 = '\0';		/* tie off criteria */
	    criteria = rfc1738_str(cmd);	/* get "enc_list_mailbox" */
	    if(!strucmp(p = rfc1738_str(p+6), "lsub"))
	      rv |= URL_IMAP_IMBXLSTLSUB;
	    else if(strucmp(p, "list"))
	      return(url_bogus_imap(&url, scheme,
				    "Invalid list type specified"));
	}
	else{
	    rv |= URL_IMAP_ISERVERONLY;
	    criteria = "";
	}
		
	/* build folder list from specified server/criteria/list-method */
	l = strlen(server) + strlen(criteria) + 10 + (user ? (strlen(user)+2) : 9);
	*folder = (char *) fs_get((l+1) * sizeof(char));
	snprintf(*folder, l+1, "{%s/%s%s%s}%s%s%s", server,
		user ? "user=\"" : "Anonymous",
		user ? user : "",
		user ? "\"" : "",
		*criteria ? "[" : "", criteria, *criteria ? "[" : "");
	(*folder)[l] = '\0';
	rv |= URL_IMAP_IMAILBOXLIST;
    }
    else{
	if((p = srchstr(cmd, "/;uid=")) != NULL){ /* "imessagepart" */
	    *p = '\0';		/* tie off mailbox [uidvalidity] */
	    if((section = srchstr(p += 6, "/;section=")) != NULL){
		*section = '\0';	/* tie off UID */
		section  = rfc1738_str(section + 10);
/* BUG: verify valid section spec ala rfc 2060 */
		dprint((2,
			   "-- URL IMAP FOLDER: section not used: %s\n",
			   section ? section : "?"));
	    }

	    if(!(*uid = rfc1738_num(&p)) || *p) /* decode UID */
	      return(url_bogus_imap(&url, scheme, "Invalid data in UID"));

	    /* optional "uidvalidity"? */
	    if((p = srchstr(cmd, ";uidvalidity=")) != NULL){
		*p  = '\0';
		p  += 13;
		if(!(*uid_val = rfc1738_num(&p)) || *p)
		  return(url_bogus_imap(&url, scheme,
					"Invalid UIDVALIDITY"));
	    }

	    mailbox = rfc1738_str(cmd);
	    rv = URL_IMAP_IMESSAGEPART;
	}
	else{			/* "imessagelist" */
	    /* optional "uidvalidity"? */
	    if((p = srchstr(cmd, ";uidvalidity=")) != NULL){
		*p  = '\0';
		p  += 13;
		if(!(*uid_val = rfc1738_num(&p)) || *p)
		  return(url_bogus_imap(&url, scheme,
					"Invalid UIDVALIDITY"));
	    }

	    /* optional "enc_search"? */
	    if((p = strchr(cmd, '?')) != NULL){
		*p = '\0';
		if(search)
		  *search = cpystr(rfc1738_str(p + 1));
/* BUG: verify valid search spec ala rfc 2060 */
	    }

	    mailbox = rfc1738_str(cmd);
	    rv = URL_IMAP_IMESSAGELIST;
	}

	if(auth && *auth != '*' && strucmp(auth, "anonymous"))
	  q_status_message(SM_ORDER, 3, 3,
	     "Unsupported authentication method.  Using standard login.");

	/*
	 * At this point our structure should contain the
	 * digested url.  Now put it together for c-client...
	 */
	l = strlen(server) + 8 + (mailbox ? strlen(mailbox) : 0)
				   + (user ? (strlen(user)+2) : 9);
	*folder = (char *) fs_get((l+1) * sizeof(char));
	snprintf(*folder, l+1, "{%s%s%s%s%s}%s", server,
		 (user || !(auth && strucmp(auth, "anonymous"))) ? "/" : "",
		 user ? "user=\"" : ((auth && strucmp(auth, "anonymous")) ? "" : "Anonymous"),
		 user ? user : "",
		 user ? "\"" : "",
		 mailbox);
	(*folder)[l] = '\0';
    }

    fs_give((void **) &url);
    return(rv);
}


int
url_bogus_imap(char **freeme, char *url, char *problem)
{
    fs_give((void **) freeme);
    (void) url_bogus(url, problem);
    return(URL_IMAP_ERROR);
}


/*
 * url_bogus - report url syntax errors and such
 */
int
url_bogus(char *url, char *reason)
{
    dprint((2, "-- bogus url \"%s\": %s\n",
	       url ? url : "<NULL URL>", reason ? reason : "?"));
    if(url)
      q_status_message3(SM_ORDER|SM_DING, 2, 3,
		        "Malformed \"%.*s\" URL: %.200s",
			(void *) (strchr(url, ':') - url), url, reason);

    return(0);
}



/*----------------------------------------------------------------------
  Format header text suitable for display

  Args: stream -- mail stream for various header text fetches
	msgno -- sequence number in stream of message we're interested in
	section -- which section of message 
	env -- pointer to msg's envelope
	hdrs -- struct containing what's to get formatted
	prefix -- prefix to append to each output line
        handlesp -- address of pointer to the message's handles
	flags -- FM_ flags, see pith/mailview.h
	final_pc -- function to write header text with

  Result: 0 if all's well, -1 if write error, 1 if fetch error

  NOTE: Blank-line delimiter is NOT written here.  Newlines are written
	in the local convention.

 ----*/
#define	FHT_OK		0
#define	FHT_WRTERR	-1
#define	FHT_FTCHERR	1
int
format_header(MAILSTREAM *stream, long int msgno, char *section, ENVELOPE *env,
	      HEADER_S *hdrs, char *prefix, HANDLE_S **handlesp, int flags,
	      fmt_env_t fmt_env, gf_io_t final_pc)
{
    int	     rv = FHT_OK;
    int	     nfields, i;
    char    *h = NULL, **fields = NULL, **v, *q, *start,
	    *finish, *current;
    STORE_S *tmp_store;
    URL_HILITE_S uh;
    gf_io_t  tmp_pc, tmp_gc;
    struct variable *vars = ps_global->vars;

    if((tmp_store = so_get(CharStar, NULL, EDIT_ACCESS)) != NULL)
      gf_set_so_writec(&tmp_pc, tmp_store);
    else
      return(FHT_WRTERR);

    if(!fmt_env)
      fmt_env = format_envelope;

    if(ps_global->full_header == 2){
	rv = format_raw_header(stream, msgno, section, tmp_pc);
    }
    else{
	/*
	 * First, calculate how big a fields array we need.
	 */

	/* Custom header viewing list specified */
	if(hdrs->type == HD_LIST){
	    /* view all these headers */
	    if(!hdrs->except){
		for(nfields = 0, v = hdrs->h.l; (q = *v) != NULL; v++)
		  if(!is_an_env_hdr(q))
		    nfields++;
	    }
	    /* view all except these headers */
	    else{
		for(nfields = 0, v = hdrs->h.l; *v != NULL; v++)
		  nfields++;

		if(nfields > 1)
		  nfields--; /* subtract one for ALL_EXCEPT field */
	    }
	}
	else
	  nfields = 6;				/* default view */

	/* allocate pointer space */
	if(nfields){
	    fields = (char **)fs_get((size_t)(nfields+1) * sizeof(char *));
	    memset(fields, 0, (size_t)(nfields+1) * sizeof(char *));
	}

	if(hdrs->type == HD_LIST){
	    /* view all these headers */
	    if(!hdrs->except){
		/* put the non-envelope headers in fields */
		if(nfields)
		  for(i = 0, v = hdrs->h.l; (q = *v) != NULL; v++)
		    if(!is_an_env_hdr(q))
		      fields[i++] = q;
	    }
	    /* view all except these headers */
	    else{
		/* put the list of headers not to view in fields */
		if(nfields)
		  for(i = 0, v = hdrs->h.l; (q = *v) != NULL; v++)
		    if(strucmp(ALL_EXCEPT, q))
		      fields[i++] = q;
	    }

	    v = hdrs->h.l;
	}
	else{
	    if(nfields){
		fields[i = 0] = "Resent-Date";
		fields[++i]   = "Resent-From";
		fields[++i]   = "Resent-To";
		fields[++i]   = "Resent-cc";
		fields[++i]   = "Resent-Subject";
	    }

	    v = fields;
	}

	/* custom view with exception list */
	if(hdrs->type == HD_LIST && hdrs->except){
	    /*
	     * Go through each header in h and print it.
	     * First we check to see if it is an envelope header so we
	     * can print our envelope version of it instead of the raw version.
	     */

	    /* fetch all the other headers */
	    if(nfields)
	      h = pine_fetchheader_lines_not(stream, msgno, section, fields);

	    for(current = h;
		h && delineate_this_header(NULL, current, &start, &finish);
		current = finish){
		char tmp[MAILTMPLEN+1];
		char *colon_loc;

		colon_loc = strindex(start, ':');
		if(colon_loc && colon_loc < finish){
		    strncpy(tmp, start, MIN(colon_loc-start, sizeof(tmp)-1));
		    tmp[MIN(colon_loc-start, sizeof(tmp)-1)] = '\0';
		}
		else
		  colon_loc = NULL;

		if(colon_loc && is_an_env_hdr(tmp)){
		    char *dummystart, *dummyfinish;

		    /*
		     * Pretty format for env hdrs.
		     * If the same header appears more than once, only
		     * print the last to avoid duplicates.
		     * They should have been combined in the env when parsed.
		     */
		    if(!delineate_this_header(tmp, current+1, &dummystart,
					      &dummyfinish))
		      format_env_hdr(stream, msgno, section, env,
				     fmt_env, tmp_pc, tmp, hdrs->charset, flags);
		}
		else{
		    if((rv = format_raw_hdr_string(start, finish, tmp_pc,
						   hdrs->charset, flags)) != 0)
		      goto write_error;
		    else
		      start = finish;
		}
	    }
	}
	/* custom view or default */
	else{
	    /* fetch the non-envelope headers */
	    if(nfields)
	      h = pine_fetchheader_lines(stream, msgno, section, fields);

	    /* default envelope for default view */
	    if(hdrs->type == HD_BFIELD)
	      (*fmt_env)(stream, msgno, section, env, tmp_pc, hdrs->h.b, hdrs->charset, flags);

	    /* go through each header in list, v initialized above */
	    for(; (q = *v) != NULL; v++){
		if(is_an_env_hdr(q)){
		    /* pretty format for env hdrs */
		    format_env_hdr(stream, msgno, section, env,
				   fmt_env, tmp_pc, q, hdrs->charset, flags);
		}
		else{
		    /*
		     * Go through h finding all occurences of this header
		     * and all continuation lines, and output.
		     */
		    for(current = h;
			h && delineate_this_header(q,current,&start,&finish);
			current = finish){
			if((rv = format_raw_hdr_string(start, finish, tmp_pc,
						       hdrs->charset, flags)) != 0)
			  goto write_error;
			else
			  start = finish;
		    }
		}
	    }
	}
    }


  write_error:

    gf_clear_so_writec(tmp_store);

    if(!rv){			/* valid data?  Do wrapping and filtering... */
	int	 column;
	char	*errstr, *display_filter = NULL, trigger[MAILTMPLEN];
	STORE_S *df_store = NULL;

	so_seek(tmp_store, 0L, 0);

	column = (flags & FM_DISPLAY) ? ps_global->ttyo->screen_cols : 80;

	/*
	 * Test for and act on any display filter
	 * This barely makes sense. The display filter is going
	 * to be getting UTF-8'ized headers here. In pre-alpine
	 * pine the display filter was being fed already decoded
	 * headers in whatever character set they were in.
	 * The good news is that that didn't make much
	 * sense either, so this shouldn't break anything.
	 * It seems unlikely that anybody is doing anything useful
	 * with the header part of display filters.
	 */
	if(ps_global->tools.display_filter
	   && ps_global->tools.display_filter_trigger
	   && (display_filter = (*ps_global->tools.display_filter_trigger)(NULL, trigger, sizeof(trigger)))){
	    if((df_store = so_get(CharStar, NULL, EDIT_ACCESS)) != NULL){

		gf_set_so_writec(&tmp_pc, df_store);
		gf_set_so_readc(&tmp_gc, df_store);
		if((errstr = (*ps_global->tools.display_filter)(display_filter, tmp_store, tmp_pc, NULL)) != NULL){
		    q_status_message1(SM_ORDER | SM_DING, 3, 3,
				      _("Formatting error: %s"), errstr);
		    rv = FHT_WRTERR;
		}
		else
		  so_seek(df_store, 0L, 0);

		gf_clear_so_writec(df_store);
	    }
	    else{
		q_status_message(SM_ORDER | SM_DING, 3, 3,
				  "No space for filtered text.");
		rv = FHT_WRTERR;
	    }
	}
	else{
	    so_seek(tmp_store, 0L, 0);
	    gf_set_so_readc(&tmp_gc, tmp_store);
	}

	if(!rv){
	    int *margin, wrapflags = GFW_ONCOMMA;

	    gf_filter_init();
	    gf_link_filter(gf_local_nvtnl, NULL);

	    if((F_ON(F_VIEW_SEL_URL, ps_global)
		|| F_ON(F_VIEW_SEL_URL_HOST, ps_global)
		|| F_ON(F_SCAN_ADDR, ps_global))
	       && handlesp){
		gf_link_filter(gf_line_test,
			       gf_line_test_opt(url_hilite_hdr,
					        gf_url_hilite_opt(&uh,handlesp,1)));
		wrapflags |= GFW_HANDLES;
	    }

	    if((flags & FM_DISPLAY)
	       && !(flags & FM_NOCOLOR)
	       && pico_usingcolor()
	       && ((VAR_NORM_FORE_COLOR
		    && VAR_HEADER_GENERAL_FORE_COLOR
		    && colorcmp(VAR_NORM_FORE_COLOR, VAR_HEADER_GENERAL_FORE_COLOR))
	          ||
	           (VAR_NORM_BACK_COLOR
		    && VAR_HEADER_GENERAL_BACK_COLOR
		    && colorcmp(VAR_NORM_BACK_COLOR, VAR_HEADER_GENERAL_BACK_COLOR))))
	      wrapflags |= GFW_HDRCOLOR;

	    if((flags & FM_DISPLAY)
	       && !(flags & FM_NOCOLOR)
	       && pico_usingcolor()
	       && ps_global->hdr_colors){
		gf_link_filter(gf_line_test,
			       gf_line_test_opt(color_headers, NULL));
		wrapflags |= (GFW_HANDLES | GFW_HDRCOLOR);
	    }

	    if(prefix && *prefix)
	      column = MAX(column-strlen(prefix), 50);

	    margin = format_view_margin();

	    if(!(flags & FM_NOWRAP))
	      gf_link_filter(gf_wrap,
			     gf_wrap_filter_opt(column, column,
					        (flags & FM_NOINDENT) ? NULL : margin,
						4, wrapflags));

	    if(prefix && *prefix)
	      gf_link_filter(gf_prefix, gf_prefix_opt(prefix));

	    if((flags & FM_DISPLAY)
	       && !(flags & FM_NOCOLOR)
	       && pico_usingcolor()
	       && ((VAR_NORM_FORE_COLOR
		    && VAR_HEADER_GENERAL_FORE_COLOR
		    && colorcmp(VAR_NORM_FORE_COLOR, VAR_HEADER_GENERAL_FORE_COLOR))
	          ||
	           (VAR_NORM_BACK_COLOR
		    && VAR_HEADER_GENERAL_BACK_COLOR
		    && colorcmp(VAR_NORM_BACK_COLOR, VAR_HEADER_GENERAL_BACK_COLOR)))){
		int right_margin;
		int total_wid;

		right_margin = margin ? margin[1] : 0;
		total_wid = column - right_margin;

		gf_link_filter(gf_line_test,
			       gf_line_test_opt(pad_to_right_edge, (void *) &total_wid));
		wrapflags |= GFW_HANDLES;
	    }

	    gf_link_filter(gf_nvtnl_local, NULL);

	    if((errstr = gf_pipe(tmp_gc, final_pc)) != NULL){
		rv = FHT_WRTERR;
		q_status_message1(SM_ORDER | SM_DING, 3, 3,
				  "Can't build header : %.200s", errstr);
	    }
	}

	if(df_store){
	    gf_clear_so_readc(df_store);
	    so_give(&df_store);
	}
	else
	  gf_clear_so_readc(tmp_store);
    }

    so_give(&tmp_store);

    if(h)
      fs_give((void **)&h);

    if(fields)
      fs_give((void **)&fields);

    return(rv);
}


/*----------------------------------------------------------------------
  Format RAW header text for display

  Args: stream -- mail stream for various header text fetches
	rawno -- sequence number in stream of message we're interested in
	section -- which section of message 
	pc -- function to write header text with

  Result: 0 if all's well, -1 if write error, 1 if fetch error

  NOTE: Blank-line delimiter is NOT written here.  Newlines are written
	in the local convention.

 ----*/
int
format_raw_header(MAILSTREAM *stream, long int msgno, char *section, gf_io_t pc)
{
    char *h = mail_fetch_header(stream, msgno, section, NULL, NULL, FT_PEEK);
    unsigned char c;

    if(h){
	while(*h){
	    if(ISRFCEOL(h)){
		h += 2;
		if(!gf_puts(NEWLINE, pc))
		  return(FHT_WRTERR);

		if(ISRFCEOL(h))		/* all done! */
		  return(FHT_OK);
	    }
	    else if((unsigned char)(*h) < 0x80 && FILTER_THIS(*h) &&
		    !(*(h+1) && *h == ESCAPE && match_escapes(h+1))){
		c = (unsigned char) *h++;
		if(!((*pc)(c >= 0x80 ? '~' : '^')
		     && (*pc)((c == 0x7f) ? '?' : (c & 0x1f) + '@')))
		  return(FHT_WRTERR);
	    }
	    else if(!(*pc)(*h++))
	      return(FHT_WRTERR);
	}
    }
    else
      return(FHT_FTCHERR);

    return(FHT_OK);
}



/*----------------------------------------------------------------------
  Format c-client envelope data suitable for display

  Args: s -- mail stream for various header text fetches
	n -- raw sequence number in stream of message we're interested in
	sect -- which section of message 
	e -- pointer to msg's envelope
	pc -- function to write header text with
	which -- which header lines to write
	oacs -- 
	flags -- FM_ flags, see pith/mailview.h

  Result: 0 if all's well, -1 if write error, 1 if fetch error

  NOTE: Blank-line delimiter is NOT written here.  Newlines are written
	in the local convention.

 ----*/
void
format_envelope(MAILSTREAM *s, long int n, char *sect, ENVELOPE *e, gf_io_t pc,
		long int which, char *oacs, int flags)
{
    char *q, *p2, buftmp[MAILTMPLEN];

    if(!e)
      return;

    if((which & FE_DATE) && e->date) {
	q = "Date: ";
	snprintf(buftmp, sizeof(buftmp), "%s",
		 F_ON(F_DATES_TO_LOCAL,ps_global)
		    ? convert_date_to_local((char *) e->date) : (char *) e->date);
	buftmp[sizeof(buftmp)-1] = '\0';
	p2 = (char *)rfc1522_decode_to_utf8((unsigned char *) tmp_20k_buf,
					    SIZEOF_20KBUF, buftmp);
	gf_puts(q, pc);
	format_env_puts(p2, pc);
	gf_puts(NEWLINE, pc);
    }

    if((which & FE_FROM) && e->from)
      format_addr_string(s, n, sect, "From: ", e->from, flags, oacs, pc);

    if((which & FE_REPLYTO) && e->reply_to
       && (!e->from || !address_is_same(e->reply_to, e->from)))
      format_addr_string(s, n, sect, "Reply-To: ", e->reply_to, flags, oacs, pc);

    if((which & FE_TO) && e->to)
      format_addr_string(s, n, sect, "To: ", e->to, flags, oacs, pc);

    if((which & FE_CC) && e->cc)
      format_addr_string(s, n, sect, "Cc: ", e->cc, flags, oacs, pc);

    if((which & FE_BCC) && e->bcc)
      format_addr_string(s, n, sect, "Bcc: ", e->bcc, flags, oacs, pc);

    if((which & FE_RETURNPATH) && e->return_path)
      format_addr_string(s, n, sect, "Return-Path: ", e->return_path,
			 flags, oacs, pc);

    if((which & FE_NEWSGROUPS) && e->newsgroups)
      format_newsgroup_string("Newsgroups: ", e->newsgroups, flags, pc);

    if((which & FE_FOLLOWUPTO) && e->followup_to)
      format_newsgroup_string("Followup-To: ", e->followup_to, flags, pc);

    if((which & FE_SUBJECT) && e->subject && e->subject[0]){
	char *freeme = NULL;

	q = "Subject: ";
	gf_puts(q, pc);

	p2 = iutf8ncpy((char *)(tmp_20k_buf+10000), (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf, 10000, e->subject), SIZEOF_20KBUF-10000);

	if(flags & FM_DISPLAY
	   && (ps_global->display_keywords_in_subject
	       || ps_global->display_keywordinits_in_subject)){

	    /* don't bother if no keywords are defined */
	    if(some_user_flags_defined(s))
	      p2 = freeme = prepend_keyword_subject(s, n, p2,
			  ps_global->display_keywords_in_subject ? KW : KWInit,
			  NULL, ps_global->VAR_KW_BRACES);
	}

	format_env_puts(p2, pc);

	if(freeme)
	  fs_give((void **) &freeme);

	gf_puts(NEWLINE, pc);
    }

    if((which & FE_SENDER) && e->sender
       && (!e->from || !address_is_same(e->sender, e->from)))
      format_addr_string(s, n, sect, "Sender: ", e->sender, flags, oacs, pc);

    if((which & FE_MESSAGEID) && e->message_id){
	q = "Message-ID: ";
	gf_puts(q, pc);
	p2 = iutf8ncpy((char *)(tmp_20k_buf+10000), (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf, 10000, e->message_id), SIZEOF_20KBUF-10000);
	format_env_puts(p2, pc);
	gf_puts(NEWLINE, pc);
    }

    if((which & FE_INREPLYTO) && e->in_reply_to){
	q = "In-Reply-To: ";
	gf_puts(q, pc);
	p2 = iutf8ncpy((char *)(tmp_20k_buf+10000), (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf, 10000, e->in_reply_to), SIZEOF_20KBUF-10000);
	format_env_puts(p2, pc);
	gf_puts(NEWLINE, pc);
    }

    if((which & FE_REFERENCES) && e->references) {
	q = "References: ";
	gf_puts(q, pc);
	p2 = iutf8ncpy((char *)(tmp_20k_buf+10000), (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf, 10000, e->references), SIZEOF_20KBUF-10000);
	format_env_puts(p2, pc);
	gf_puts(NEWLINE, pc);
    }
}




/*
 * The argument fieldname is something like "Subject:..." or "Subject".
 * Look through the specs in speccolor for a match of the fieldname,
 * and return the color that goes with any match, or NULL.
 * Caller should free the color.
 */
COLOR_PAIR *
hdr_color(char *fieldname, char *value, SPEC_COLOR_S *speccolor)
{
    SPEC_COLOR_S *hc = NULL;
    COLOR_PAIR   *color_pair = NULL;
    char         *colon, *fname;
    char          fbuf[FBUF_LEN+1];
    int           gotit;
    PATTERN_S    *pat;

    colon = strindex(fieldname, ':');
    if(colon){
	strncpy(fbuf, fieldname, MIN(colon-fieldname,FBUF_LEN));
	fbuf[MIN(colon-fieldname,FBUF_LEN)] = '\0';
	fname = fbuf;
    }
    else
      fname = fieldname;

    if(fname && *fname)
      for(hc = speccolor; hc; hc = hc->next)
	if(hc->spec && !strucmp(fname, hc->spec)){
	    if(!hc->val)
	      break;
	    
	    gotit = 0;
	    for(pat = hc->val; !gotit && pat; pat = pat->next)
	      if(srchstr(value, pat->substring))
		gotit++;
	    
	    if(gotit)
	      break;
	}

    if(hc && hc->fg && hc->fg[0] && hc->bg && hc->bg[0])
      color_pair = new_color_pair(hc->fg, hc->bg);
    
    return(color_pair);
}


/*
 * The argument fieldname is something like "Subject:..." or "Subject".
 * Look through the specs in hdr_colors for a match of the fieldname,
 * and return 1 if that fieldname is in one of the patterns, 0 otherwise.
 */
int
any_hdr_color(char *fieldname)
{
    SPEC_COLOR_S *hc = NULL;
    char         *colon, *fname;
    char          fbuf[FBUF_LEN+1];

    colon = strindex(fieldname, ':');
    if(colon){
	strncpy(fbuf, fieldname, MIN(colon-fieldname,FBUF_LEN));
	fbuf[MIN(colon-fieldname,FBUF_LEN)] = '\0';
	fname = fbuf;
    }
    else
      fname = fieldname;

    if(fname && *fname)
      for(hc = ps_global->hdr_colors; hc; hc = hc->next)
	if(hc->spec && !strucmp(fname, hc->spec))
	  break;

    return(hc ? 1 : 0);
}


/*----------------------------------------------------------------------
     Format an address field, wrapping lines nicely at commas

  Args: field_name  -- The name of the field we're formatting ("TO: ", ...)
        addr        -- ADDRESS structure to format

 Result: A formatted, malloced string is returned.
  ----------------------------------------------------------------------*/
void
format_addr_string(MAILSTREAM *stream, long int msgno, char *section, char *field_name,
		   struct mail_address *addr, int flags, char *oacs, gf_io_t pc)
{
    char    *ptmp, *mtmp;
    int	     trailing = 0, group = 0;
    ADDRESS *atmp;

    if(!addr)
      return;

    /*
     * quickly run down address list to make sure none are patently bogus.
     * If so, just blat raw field out.
     */
    for(atmp = addr; stream && atmp; atmp = atmp->next)
      if(atmp->host && atmp->host[0] == '.'){
	  char *field, *fields[2];

	  fields[1] = NULL;
	  fields[0] = cpystr(field_name);
	  if((ptmp = strchr(fields[0], ':')) != NULL)
	    *ptmp = '\0';

	  if((field = pine_fetchheader_lines(stream, msgno, section, fields)) != NULL){
	      char *h, *t;

	      for(t = h = field; *h ; t++)
		if(*t == '\015' && *(t+1) == '\012'){
		    *t = '\0';			/* tie off line */
		    format_env_puts(h, pc);
		    if(*(h = (++t) + 1))	/* set new h and skip CRLF */
		      gf_puts(NEWLINE, pc);	/* more to write */
		    else
		      break;
		}
		else if(!*t){			/* shouldn't happen much */
		    if(h != t)
		      format_env_puts(h, pc);

		    break;
		}

	      fs_give((void **)&field);
	  }

	  fs_give((void **)&fields[0]);
	  gf_puts(NEWLINE, pc);
	  dprint((2, "Error in \"%s\" field address\n",
	         field_name ? field_name : "?"));
	  return;
      }

    gf_puts(field_name, pc);

    while(addr){
	atmp	       = addr->next;		/* remember what's next */
	addr->next     = NULL;
	if(!addr->host && addr->mailbox){
	    mtmp = addr->mailbox;
	    addr->mailbox = cpystr((char *)rfc1522_decode_to_utf8(
			   (unsigned char *)tmp_20k_buf,
			   SIZEOF_20KBUF, addr->mailbox));
	}

	ptmp	       = addr->personal;	/* RFC 1522 personal name? */
	addr->personal = iutf8ncpy((char *)tmp_20k_buf, (char *)rfc1522_decode_to_utf8((unsigned char *)(tmp_20k_buf+10000), SIZEOF_20KBUF-10000, addr->personal), 10000);
	tmp_20k_buf[10000-1] = '\0';

	if(!trailing)				/* 1st pass, just address */
	  trailing++;
	else{					/* else comma, unless */
	    if(!((group == 1 && addr->host)	/* 1st addr in group, */
	       || (!addr->host && !addr->mailbox))){ /* or end of group */
		gf_puts(",", pc);
#if	0
		gf_puts(NEWLINE, pc);		/* ONE address/line please */
		gf_puts("   ", pc);
#endif
	    }

	    gf_puts(" ", pc);
	}

	pine_rfc822_write_address_noquote(addr, pc, &group);

	addr->personal = ptmp;			/* restore old personal ptr */
	if(!addr->host && addr->mailbox){
	    fs_give((void **)&addr->mailbox);
	    addr->mailbox = mtmp;
	}

	addr->next = atmp;
	addr       = atmp;
    }

    gf_puts(NEWLINE, pc);
}


 

const char *rspecials_minus_quote_and_dot = "()<>@,;:\\[]";
				/* RFC822 continuation, must start with CRLF */
#define RFC822CONT "\015\012    "

/* Write RFC822 address with some quoting turned off.
 * Accepts: 
 *	    address to interpret
 *
 * (This is a copy of c-client's rfc822_write_address except
 *  we don't quote double quote and dot in personal names. It writes
 *  to a gf_io_t instead of to a buffer so that we don't have to worry
 *  about fixed sized buffer overflowing. It's also special cased to deal
 *  with only a single address.)
 *
 * The idea is that there are some places where we'd just like to display
 * the personal name as is before applying confusing quoting. However,
 * we do want to be careful not to break things that should be quoted so
 * we'll only use this where we are sure. Quoting may look ugly but it
 * doesn't usually break anything.
 */
void
pine_rfc822_write_address_noquote(struct mail_address *adr, gf_io_t pc, int *group)
{
  extern const char *rspecials;

    if (adr->host) {		/* ordinary address? */
      if (!(adr->personal || adr->adl)) pine_rfc822_address (adr, pc);
      else {			/* no, must use phrase <route-addr> form */
        if (adr->personal)
	  pine_rfc822_cat (adr->personal, rspecials_minus_quote_and_dot, pc);

        gf_puts(" <", pc);	/* write address delimiter */
        pine_rfc822_address(adr, pc);
        gf_puts (">", pc);	/* closing delimiter */
      }

      if(*group)
	(*group)++;
    }
    else if (adr->mailbox) {	/* start of group? */
				/* yes, write group name */
      pine_rfc822_cat (adr->mailbox, rspecials, pc);

      gf_puts (": ", pc);	/* write group identifier */
      *group = 1;		/* in a group */
    }
    else if (*group) {		/* must be end of group (but be paranoid) */
      gf_puts (";", pc);
      *group = 0;		/* no longer in that group */
    }
}


/* Write RFC822 route-address to string
 * Accepts:
 *	    address to interpret
 */

void
pine_rfc822_address(struct mail_address *adr, gf_io_t pc)
{
  extern char *wspecials;

  if (adr && adr->host) {	/* no-op if no address */
    if (adr->adl) {		/* have an A-D-L? */
      gf_puts (adr->adl, pc);
      gf_puts (":", pc);
    }
				/* write mailbox name */
    pine_rfc822_cat (adr->mailbox, wspecials, pc);
    if (*adr->host != '@') {	/* unless null host (HIGHLY discouraged!) */
      gf_puts ("@", pc);	/* host delimiter */
      gf_puts (adr->host, pc);	/* write host name */
    }
  }
}


/* Concatenate RFC822 string
 * Accepts: 
 *	    pointer to string to concatenate
 *	    list of special characters
 */

void
pine_rfc822_cat(char *src, const char *specials, gf_io_t pc)
{
  char *s;

  if (strpbrk (src,specials)) {	/* any specials present? */
    gf_puts ("\"", pc);		/* opening quote */
				/* truly bizarre characters in there? */
    while ((s = strpbrk (src,"\\\"")) != NULL) {
      char save[2];

      /* turn it into a null-terminated piece */
      save[0] = *s;
      save[1] = '\0';
      *s = '\0';
      gf_puts (src, pc);	/* yes, output leader */
      *s = save[0];
      gf_puts ("\\", pc);	/* quoting */
      gf_puts (save, pc);	/* output the bizarre character */
      src = ++s;		/* continue after the bizarre character */
    }
    if (*src) gf_puts (src, pc);/* output non-bizarre string */
    gf_puts ("\"", pc);		/* closing quote */
  }
  else gf_puts (src, pc);	/* otherwise it's the easy case */
}


/*----------------------------------------------------------------------
  Format an address field, wrapping lines nicely at commas

  Args: field_name  -- The name of the field we're formatting ("TO:", Cc:...)
        newsgrps    -- ADDRESS structure to format

  Result: A formatted, malloced string is returned.

The resuling lines formatted are 80 columns wide.
  ----------------------------------------------------------------------*/
void
format_newsgroup_string(char *field_name, char *newsgrps, int flags, gf_io_t pc)
{
    char     buf[MAILTMPLEN];
    int	     trailing = 0, llen, alen;
    char    *next_ng;
    
    if(!newsgrps || !*newsgrps)
      return;
    
    gf_puts(field_name, pc);

    llen = strlen(field_name);
    while(*newsgrps){
        for(next_ng = newsgrps; *next_ng && *next_ng != ','; next_ng++);
        strncpy(buf, newsgrps, MIN(next_ng - newsgrps, sizeof(buf)-1));
        buf[MIN(next_ng - newsgrps, sizeof(buf)-1)] = '\0';
        newsgrps = next_ng;
        if(*newsgrps)
          newsgrps++;
	alen = strlen(buf);
	if(!trailing){			/* first time thru, just address */
	    llen += alen;
	    trailing++;
	}
	else{				/* else preceding comma */
	    gf_puts(",", pc);
	    llen++;

	    if(alen + llen + 1 > 76){
		gf_puts(NEWLINE, pc);
		gf_puts("    ", pc);
		llen = alen + 5;
	    }
	    else{
		gf_puts(" ", pc);
		llen += alen + 1;
	    }
	}

	if(alen && llen > 76){		/* handle long addresses */
	    register char *q, *p = &buf[alen-1];

	    while(p > buf){
		if(isspace((unsigned char)*p)
		   && (llen - (alen - (int)(p - buf))) < 76){
		    for(q = buf; q < p; q++)
		      (*pc)(*q);	/* write character */

		    gf_puts(NEWLINE, pc);
		    gf_puts("    ", pc);
		    gf_puts(p, pc);
		    break;
		}
		else
		  p--;
	    }

	    if(p == buf)		/* no reasonable break point */
	      gf_puts(buf, pc);
	}
	else
	  gf_puts(buf, pc);
    }

    gf_puts(NEWLINE, pc);
}



/*----------------------------------------------------------------------
  Format a text field that's part of some raw (non-envelope) message header

  Args: start --
        finish --
	pc -- 

  Result: Semi-digested text (RFC 1522 decoded, anyway) written with "pc"

  ----------------------------------------------------------------------*/
int
format_raw_hdr_string(char *start, char *finish, gf_io_t pc, char *oacs, int flags)
{
    register char *current;
    unsigned char *p, *tmp = NULL, c;
    size_t	   n, len;
    char	   ch;
    int		   rv = FHT_OK;

    ch = *finish;
    *finish = '\0';

    if((n = 4*(finish-start)) > SIZEOF_20KBUF-1){
	len = n+1;
	p = tmp = (unsigned char *) fs_get(len * sizeof(unsigned char));
    }
    else{
	len = SIZEOF_20KBUF;
	p = (unsigned char *) tmp_20k_buf;
    }

    if(islower((unsigned char)(*start)))
      *start = toupper((unsigned char)(*start));

    current = (char *) rfc1522_decode_to_utf8(p, len, start);

    /* output from start to finish */
    while(*current && rv == FHT_OK)
      if(ISRFCEOL(current)){
	  if(!gf_puts(NEWLINE, pc))
	    rv = FHT_WRTERR;

	  current += 2;
      }
      else if((unsigned char)(*current) < 0x80 && FILTER_THIS(*current) &&
	     !(*(current+1) && *current == ESCAPE && match_escapes(current+1))){
	  c = (unsigned char) *current++;
	  if(!((*pc)(c >= 0x80 ? '~' : '^')
	       && (*pc)((c == 0x7f) ? '?' : (c & 0x1f) + '@')))
	    rv = FHT_WRTERR;
      }
      else if(!(*pc)(*current++))
	rv = FHT_WRTERR;

    if(tmp)
      fs_give((void **) &tmp);

    *finish = ch;

    return(rv);
}




/*----------------------------------------------------------------------
  Format a text field that's part of some raw (non-envelope) message header

  Args: s --
	pc -- 

  Result: Output

  ----------------------------------------------------------------------*/
int
format_env_puts(char *s, gf_io_t pc)
{
    if(ps_global->pass_ctrl_chars)
      return(gf_puts(s, pc));

    for(; *s; s++)
      if((unsigned char)(*s) < 0x80 && FILTER_THIS(*s) && !(*(s+1) && *s == ESCAPE && match_escapes(s+1))){
	  if(!((*pc)((unsigned char) (*s) >= 0x80 ? '~' : '^')
	       && (*pc)((*s == 0x7f) ? '?' : (*s & 0x1f) + '@')))
	    return(0);
      }
      else if(!(*pc)(*s))
	return(0);

    return(1);
}


char *    
display_parameters(PARAMETER *params)
{
    int		n, longest = 0;
    char       *d, *printme;
    PARAMETER  *p;
    PARMLIST_S *parmlist;

    for(p = params; p; p = p->next)	/* ok if we include *'s */
      if(p->attribute && (n = strlen(p->attribute)) > longest)
	longest = MIN(32, n);   /* shouldn't be any bigger than 32 */

    d = tmp_20k_buf;
    tmp_20k_buf[0] = '\0';
    if((parmlist = rfc2231_newparmlist(params)) != NULL){
	n = 0;			/* n overloaded */
	while(rfc2231_list_params(parmlist) && d < tmp_20k_buf + 10000){
	    if(n++){
		snprintf(d, 10000-(d-tmp_20k_buf), "\n");
		tmp_20k_buf[10000-1] = '\0';
		d += strlen(d);
	    }

	    if(parmlist->value){
		if(parmlist->attrib && strucmp(parmlist->attrib, "url") == 0){
		    snprintf(printme = tmp_20k_buf + 11000, 1000, "%s", parmlist->value);
		    sqzspaces(printme);
		}
		else
		  printme = strsquish(tmp_20k_buf + 11000, 1000, parmlist->value, 100);
	    }
	    else
	      printme = "";

            snprintf(d, 10000-(d-tmp_20k_buf), "%-*s: %s", longest,
		     parmlist->attrib ? parmlist->attrib : "", printme);

	    tmp_20k_buf[10000-1] = '\0';
            d += strlen(d);
	}

	rfc2231_free_parmlist(&parmlist);
    }

    return(tmp_20k_buf);
}


/*----------------------------------------------------------------------
      Fetch the requested header fields from the msgno specified

   Args: stream -- mail stream of open folder
         msgno -- number of message to get header lines from
         fields -- array of pointers to desired fields

   Returns: allocated string containing matched header lines,
	    NULL on error.
 ----*/
char *
pine_fetch_header(MAILSTREAM *stream, long int msgno, char *section, char **fields, long int flags)
{
    STRINGLIST *sl;
    char       *p, *m, *h = NULL, *match = NULL, *free_this, tmp[MAILTMPLEN];
    char      **pflds = NULL, **pp = NULL, **qq;

    /*
     * If the user misconfigures it is possible to have one of the fields
     * set to the empty string instead of a header name. We want to catch
     * that here instead of asking the server the nonsensical question.
     */
    for(pp = fields ? &fields[0] : NULL; pp && *pp; pp++)
      if(!**pp)
	break;

    if(pp && *pp){		/* found an empty header field, fix it */
	pflds = copy_list_array(fields);
	for(pp = pflds; pp && *pp; pp++){
	    if(!**pp){			/* scoot rest of the lines up */
		free_this = *pp;
		for(qq = pp; *qq; qq++)
		  *qq = *(qq+1);

		if(free_this)
		  fs_give((void **) &free_this);
	    }
	}

	/* no headers to look for, return NULL */
	if(pflds && !*pflds && !(flags & FT_NOT)){
	    free_list_array(&pflds);
	    return(NULL);
	}
    }
    else
      pflds = fields;

    sl = (pflds && *pflds) ? new_strlst(pflds) : NULL;  /* package up fields */
    h  = mail_fetch_header(stream, msgno, section, sl, NULL, flags | FT_PEEK);
    if (sl) 
      free_strlst(&sl);

    if(!h){
	if(pflds && pflds != fields)
	  free_list_array(&pflds);
	  
	return(NULL);
    }

    while(find_field(&h, tmp, sizeof(tmp))){
	for(pp = &pflds[0]; *pp && strucmp(tmp, *pp); pp++)
	  ;

	/* interesting field? */
	if((p = (flags & FT_NOT) ? ((*pp) ? NULL : tmp) : *pp) != NULL){
	    /*
	     * Hold off allocating space for matching fields until
	     * we at least find one to copy...
	     */
	    if(!match)
	      match = m = fs_get(strlen(h) + strlen(p) + 1);

	    while(*p)				/* copy field name */
	      *m++ = *p++;

	    while(*h && (*m++ = *h++))		/* header includes colon */
	      if(*(m-1) == '\n' && (*h == '\r' || !isspace((unsigned char)*h)))
		break;

	    *m = '\0';				/* tie off match string */
	}
	else{					/* no match, pass this field */
	    while(*h && !(*h++ == '\n'
	          && (*h == '\r' || !isspace((unsigned char)*h))))
	      ;
	}
    }

    if(pflds && pflds != fields)
      free_list_array(&pflds);

    return(match ? match : cpystr(""));
}


int
find_field(char **h, char *tmp, size_t ntmp)
{
    char *otmp = tmp;

    if(!h || !*h || !**h || isspace((unsigned char)**h))
      return(0);

    while(tmp-otmp<ntmp-1 && **h && **h != ':' && !isspace((unsigned char)**h))
      *tmp++ = *(*h)++;

    *tmp = '\0';
    return(1);
}


static char *_last_embedded_fg_color, *_last_embedded_bg_color;


int
embed_color(COLOR_PAIR *cp, gf_io_t pc)
{
    if(cp && cp->fg){
	if(_last_embedded_fg_color)
	  fs_give((void **)&_last_embedded_fg_color);

	_last_embedded_fg_color = cpystr(cp->fg);

	if(!(pc && (*pc)(TAG_EMBED) && (*pc)(TAG_FGCOLOR) &&
	     gf_puts(color_to_asciirgb(cp->fg), pc)))
	  return 0;
    }
    
    if(cp && cp->bg){
	if(_last_embedded_bg_color)
	  fs_give((void **)&_last_embedded_bg_color);

	_last_embedded_bg_color = cpystr(cp->bg);

	if(!(pc && (*pc)(TAG_EMBED) && (*pc)(TAG_BGCOLOR) &&
	     gf_puts(color_to_asciirgb(cp->bg), pc)))
	  return 0;
    }

    return 1;
}


COLOR_PAIR *
get_cur_embedded_color(void)
{
    COLOR_PAIR *ret;

    if(_last_embedded_fg_color && _last_embedded_bg_color)
      ret = new_color_pair(_last_embedded_fg_color, _last_embedded_bg_color);
    else
      ret = pico_get_cur_color();
    
    return(ret);
}


void
clear_cur_embedded_color(void)
{
    if(_last_embedded_fg_color)
      fs_give((void **)&_last_embedded_fg_color);

    if(_last_embedded_bg_color)
      fs_give((void **)&_last_embedded_bg_color);
}


int
scroll_handle_start_color(char *colorstring, size_t buflen, int *len)
{
    *len = 0;

    if(pico_usingcolor()){
	char *fg = NULL, *bg = NULL, *s;

	if(ps_global->VAR_SLCTBL_FORE_COLOR
	   && colorcmp(ps_global->VAR_SLCTBL_FORE_COLOR,
		       ps_global->VAR_NORM_FORE_COLOR))
	  fg = ps_global->VAR_SLCTBL_FORE_COLOR;

	if(ps_global->VAR_SLCTBL_BACK_COLOR
	   && colorcmp(ps_global->VAR_SLCTBL_BACK_COLOR,
		       ps_global->VAR_NORM_BACK_COLOR))
	  bg = ps_global->VAR_SLCTBL_BACK_COLOR;

	if(bg || fg){
	    COLOR_PAIR *tmp;

	    /*
	     * The blacks are just known good colors for
	     * testing whether the other color is good.
	     */
	    if((tmp = new_color_pair(fg ? fg : colorx(COL_BLACK),
				     bg ? bg : colorx(COL_BLACK))) != NULL){
		if(pico_is_good_colorpair(tmp))
		  for(s = color_embed(fg, bg);
		      (*len) < buflen && (colorstring[*len] = *s);
		      s++, (*len)++)
		    ;

		free_color_pair(&tmp);
	    }
	}

	if(F_OFF(F_SLCTBL_ITEM_NOBOLD, ps_global)){
	    strncpy(colorstring + (*len), url_embed(TAG_BOLDON), MIN(3,buflen-(*len)));
	    *len += 2;
	}
    }

    colorstring[buflen-1] = '\0';

    return(*len != 0);
}


int
scroll_handle_end_color(char *colorstring, size_t buflen, int *len, int use_hdr_color)
{
    *len = 0;
    if(pico_usingcolor()){
	char *fg = NULL, *bg = NULL, *s;
	char *basefg = NULL, *basebg = NULL;

	basefg = use_hdr_color	? ps_global->VAR_HEADER_GENERAL_FORE_COLOR
				: ps_global->VAR_NORM_FORE_COLOR;
	basebg = use_hdr_color	? ps_global->VAR_HEADER_GENERAL_BACK_COLOR
				: ps_global->VAR_NORM_BACK_COLOR;

	/*
	 * We need to change the fg and bg colors back even if they
	 * are the same as the Normal Colors so that color_a_quote
	 * will have a chance to pick up those colors as the ones to
	 * switch to. We don't do this before the handle above so that
	 * the quote color will flow into the selectable item when
	 * the selectable item color is partly the same as the
	 * normal color. That is, suppose the normal color was black on
	 * cyan and the selectable color was blue on cyan, only a fg color
	 * change. We preserve the only-a-fg-color-change in a quote by
	 * letting the quote background color flow into the selectable text.
	 */
	if(ps_global->VAR_SLCTBL_FORE_COLOR)
	  fg = basefg;

	if(ps_global->VAR_SLCTBL_BACK_COLOR)
	  bg = basebg;

	if(F_OFF(F_SLCTBL_ITEM_NOBOLD, ps_global)){
	    strncpy(colorstring, url_embed(TAG_BOLDOFF), MIN(3,buflen));
	    *len = 2;
	}

	if(fg || bg)
	  for(s = color_embed(fg, bg); (*len) < buflen && (colorstring[*len] = *s); s++, (*len)++)
	    ;
    }

    colorstring[buflen-1] = '\0';

    return(*len != 0);
}


/*
 * Helper routine that is of limited use.
 * We need to tally up the screen width of
 * a UTF-8 string as we go through the string.
 * We just want the width of the character starting
 * at str (and no longer than remaining_octets).
 * If we're plopped into the middle of a UTF-8
 * character we just want to return width zero.
 */
int
width_at_this_position(unsigned char *str, unsigned long n)
{
    unsigned char *inputp = str;
    unsigned long  remaining_octets = n;
    UCS            ucs;
    int            width = 0;

    ucs = (UCS) utf8_get(&inputp, &remaining_octets);
    if(!(ucs & U8G_ERROR || ucs == UBOGON)){
	width = wcellwidth(ucs);
	/* Writechar will print a '?' */
	if(width < 0)
	  width = 1;
    }

    return(width);
}
