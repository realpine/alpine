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

#ifndef PITH_MAILVIEW_INCLUDED
#define PITH_MAILVIEW_INCLUDED


#include "../pith/store.h"
#include "../pith/handle.h"
#include "../pith/bitmap.h"
#include "../pith/helptext.h"
#include "../pith/msgno.h"
#include "../pith/filttype.h"
#include "../pith/pattern.h"
#include "../pith/state.h"
#include "../pith/charset.h"
#include "../pith/color.h"


/* format_message flags */
#define	FM_DISPLAY	  0x0001	/* result is headed for display		*/
#define	FM_NEW_MESS	  0x0002	/* a new message so zero out attachment descrip */
#define	FM_NOWRAP	  0x0008	/* no wrapping done			*/
#define	FM_NOCOLOR	  0x0010	/* no color added			*/
#define	FM_NOINDENT	  0x0020	/* no indents, but only works has effect if wrapping */
#define	FM_NOEDITORIAL	  0x0040	/* no editorial comments		*/
#define	FM_NOHTMLREL	  0x0200	/* no relative links			*/
#define	FM_HTMLRELATED	  0x0400	/* allow multi/related			*/
#define	FM_FORCEPREFPLN	  0x0800	/* force prefer-plain this time		*/
#define	FM_FORCENOPREFPLN 0x1000	/* force not prefer-plain this time	*/
#define	FM_HIDESERVER	  0x2000	/* HIDE servername after active HTML links */
#define	FM_HTML		  0x4000	/* filter/preserve HTML markup		*/
#define	FM_HTMLIMAGES	  0x8000	/* filter/preserve HTML IMG tags	*/


#define SIGDASHES	"-- "
#define START_SIG_BLOCK	2
#define IN_SIG_BLOCK	1
#define OUT_SIG_BLOCK	0


/*
 * Which header fields should format_envelope output?
 */
#define	FE_FROM		0x0001
#define	FE_SENDER	0x0002
#define	FE_DATE		0x0004
#define	FE_TO		0x0008
#define	FE_CC		0x0010
#define	FE_BCC		0x0020
#define	FE_NEWSGROUPS	0x0040
#define	FE_SUBJECT	0x0080
#define	FE_MESSAGEID	0x0100
#define	FE_REPLYTO	0x0200
#define	FE_FOLLOWUPTO	0x0400
#define	FE_INREPLYTO	0x0800
#define	FE_RETURNPATH	0x1000
#define	FE_REFERENCES	0x2000
#define	FE_DEFAULT	(FE_FROM | FE_DATE | FE_TO | FE_CC | FE_BCC \
			 | FE_NEWSGROUPS | FE_SUBJECT | FE_REPLYTO \
			 | FE_FOLLOWUPTO)


/*
 * Function to format 
 */
typedef void (*fmt_env_t)(MAILSTREAM *, long int, char *, ENVELOPE *, gf_io_t, long int, char *, int);

/*
 * Structure and macros to help control format_header_text
 */
typedef struct header_s {
    unsigned type:4;
    unsigned except:1;
    union {
	char **l;		/* list of char *'s */
	long   b;		/* bit field of header fields (FE_* above) */
    } h;
    char charset[CSET_MAX];
} HEADER_S;


/*
 * Macro's to help sort out how we display MIME types
 */
#define	MCD_NONE	0x00
#define	MCD_INTERNAL	0x01
#define	MCD_EXTERNAL	0x02
#define	MCD_EXT_PROMPT	0x04


#define	HD_LIST		1
#define	HD_BFIELD	2
#define	HD_INIT(H, L, E, B)	{					\
				    if((L) && (L)[0]){			\
				      (H)->type = HD_LIST;		\
				      (H)->except = (E);		\
				      (H)->h.l = (L);			\
				  }					\
				  else{					\
				      (H)->type = HD_BFIELD;		\
				      (H)->h.b = (B);			\
				      (H)->except = 0;			\
				  }					\
				  (H)->charset[0] = '\0';		\
				}


/* exported protoypes */
int	 format_message(long, ENVELOPE *, BODY *, HANDLE_S **, int, gf_io_t);
int	 format_attachment_list(long int, BODY *, HANDLE_S **, int, int, gf_io_t);
char	*format_body(long int, BODY *, HANDLE_S **, HEADER_S *, int, int, gf_io_t);
int	 url_hilite(long, char *, LT_INS_S **, void *);
int	 handle_start_color(char *, size_t, int *, int);
int	 handle_end_color(char *, size_t, int *);

/*
 * BUG:  BELOW IS UNIX/PC ONLY since config'd browser means nothing to webpine
 */

int	    url_external_specific_handler(char *, int);
int	    url_imap_folder(char *, char **, imapuid_t *, imapuid_t *, char **, int);
int	    url_bogus(char *, char *);
void        pine_rfc822_address(ADDRESS *, gf_io_t);
void        pine_rfc822_cat(char *, const char *, gf_io_t);
int	    format_header(MAILSTREAM *, long, char *, ENVELOPE *, HEADER_S *,
			  char *, HANDLE_S **, int,  fmt_env_t, gf_io_t);
COLOR_PAIR *hdr_color(char *, char *, SPEC_COLOR_S *);
char	   *display_parameters(PARAMETER *);
char	   *pine_fetch_header(MAILSTREAM *, long, char *, char **, long);
int         color_signature(long, char *, LT_INS_S **, void *);
int	    scroll_handle_start_color(char *, size_t, int *);
int	    scroll_handle_end_color(char *, size_t, int *, int);
int         width_at_this_position(unsigned char *, unsigned long);

/* currently mandatory to implement stubs */

/* this is used in rfc2369_editorial() in format_message() */
void	   rfc2369_display(MAILSTREAM *, MSGNO_S *, long);


#endif /* PITH_MAILVIEW_INCLUDED */
