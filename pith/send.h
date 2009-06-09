/*
 * $Id: send.h 197 2006-10-24 22:14:07Z hubert@u.washington.edu $
 *
 * ========================================================================
 * Copyright 2006 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */

#ifndef PITH_SEND_INCLUDED
#define PITH_SEND_INCLUDED


#include "../pith/context.h"
#include "../pith/pattern.h"
#include "../pith/repltype.h"
#include "../pith/store.h"


#ifndef TCPSTREAM
#define TCPSTREAM void
#endif

#define	MIME_VER	"MIME-Version: 1.0\015\012"

#define UNKNOWN_CHARSET		"X-UNKNOWN"

#define OUR_HDRS_LIST		"X-Our-Headers"


/*
 * Redraft flags...
 */
#define	REDRAFT_NONE	0
#define	REDRAFT_PPND	0x01
#define	REDRAFT_DEL	0x02


/*
 * Child posting control structure
 */
typedef struct post_s {
    int		pid;		/* proc id of child doing posting */
    int		status;		/* child's exit status */
    char       *fcc;		/* fcc we may have copied */
} POST_S;


/*------------------------------
  Structures and enum used to expand the envelope structure to
  support user defined headers. PINEFIELDs are sort of used for two
  different purposes. The main use is to store information about headers
  in pine_send. There is a pf for every header. It is also used for the
  related task of parsing the customized-hdrs and role->cstm headers and
  storing information about those.
  ----*/

typedef enum {FreeText, Address, Fcc,
	      Attachment, Subject, TypeUnknown} FieldType;
typedef enum {NoMatch = 0,		/* no match for this header */
	      UseAsDef=1,		/* use only if no value set yet */
	      Combine=2,		/* combine if News, To, Cc, Bcc, else
					   replace existing value */
	      Replace=3} CustomType;	/* replace existing value */

typedef struct pine_field {
    char     *name;			/* field's literal name		    */
    FieldType type;			/* field's type			    */
    unsigned  canedit:1;		/* allow editing of this field	    */
    unsigned  writehdr:1;		/* write rfc822 header for field    */
    unsigned  localcopy:1;		/* copy to fcc or postponed	    */
    unsigned  rcptto:1;			/* send to this if type Address	    */
    unsigned  posterr:1;		/* posting error occured in field   */
    /* the next three fields are used only for customized-hdrs and friends */
    unsigned  standard:1;		/* this hdr already in pf_template  */
    CustomType cstmtype;		/* for customized-hdrs and r->cstm  */
    char     *val;			/* field's config'd value	    */
    ADDRESS **addr;			/* used if type is Address	    */
    char     *scratch;			/* scratch pointer for Address type */
    char    **text;			/* field's free text form	    */
    char     *textbuf;			/* need to free this when done	    */
    void     *extdata;			/* hook for extra data pointer	    */
    struct pine_field *next;		/* next pine field		    */
} PINEFIELD;


typedef struct {
    ENVELOPE   *env;		/* standard c-client envelope		*/
    PINEFIELD  *local;		/* this is all of the headers		*/
    PINEFIELD  *custom;		/* ptr to start of custom headers	*/
    PINEFIELD **sending_order;	/* array giving writing order of hdrs	*/
} METAENV;


/*
 * Return values for check_address()
 */
#define CA_OK	  0		/* Address is OK                           */
#define CA_EMPTY  1		/* Address is OK, but no deliverable addrs */
#define CA_BAD   -1		/* Address is bogus                        */



/*
 * call_mailer bigresult_f flags
 */
#define	CM_BR_VERBOSE	0x01
#define	CM_BR_ERROR	0x02


/*
 * call_mailer flags
 */
#define	CM_NONE		NULL
#define	CM_VERBOSE	0x01	/* request verbose mode */
#define CM_DSN_NEVER	0x02	/* request NO DSN */
#define CM_DSN_DELAY	0x04	/* DSN delay notification */
#define CM_DSN_SUCCESS	0x08	/* DSN success notification */
#define CM_DSN_FULL	0x10	/* DSN full notificiation */
#define CM_DSN_SHOW	0x20	/* show DSN result (place holder) */


#ifdef	DEBUG_PLUS
#define	TIME_STAMP(str, l)	{ \
				  struct timeval  tp; \
				  struct timezone tzp; \
				  if(gettimeofday(&tp, &tzp) == 0) \
				    dprint((l, \
					   "\nKACHUNK (%s) : %.8s.%3.3ld\n", \
					   str, ctime(&tp.tv_sec) + 11, \
					   tp.tv_usec));\
				}
#else	/* !DEBUG_PLUS */
#define	TIME_STAMP(str, l)
#endif	/* !DEBUG_PLUS */

/*
 * Most number of errors call_mailer should report
 */
#define MAX_ADDR_ERROR 2  /* Only display 2 address errors */


#if	defined(DOS) && !defined(WIN32)
#define	FCC_SOURCE	TmpFileStar
#define	FCC_STREAM_MODE	OP_SHORTCACHE
#else
#define	FCC_SOURCE	CharStar
#define	FCC_STREAM_MODE	0L
#endif


struct local_message_copy {
    STORE_S  *so;
    unsigned  text_only:1;
    unsigned  all_written:1;
    unsigned  text_written:1;
};


#define TONAME "To"
#define CCNAME "cc"
#define SUBJNAME "Subject"


/* this is used in pine_send and pine_simple_send */
/* name::type::canedit::writehdr::localcopy::rcptto */
static PINEFIELD pf_template[] = {
  {"X-Auth-Received",    FreeText,	0, 1, 1, 0},	/* N_AUTHRCVD */
  {"From",        Address,	0, 1, 1, 0},
  {"Reply-To",    Address,	0, 1, 1, 0},
  {TONAME,        Address,	1, 1, 1, 1},
  {CCNAME,        Address,	1, 1, 1, 1},
  {"bcc",         Address,	1, 0, 1, 1},
  {"Newsgroups",  FreeText,	1, 1, 1, 0},
  {"Fcc",         Fcc,		1, 0, 0, 0},
  {"Lcc",         Address,	1, 0, 1, 1},
  {"Attchmnt",    Attachment,	1, 1, 1, 0},
  {SUBJNAME,      Subject,	1, 1, 1, 0},
  {"References",  FreeText,	0, 1, 1, 0},
  {"Date",        FreeText,	0, 1, 1, 0},
  {"In-Reply-To", FreeText,	0, 1, 1, 0},
  {"Message-ID",  FreeText,	0, 1, 1, 0},
  {"To",          Address,	0, 0, 0, 0},	/* N_NOBODY */
  {"X-Post-Error",FreeText,	0, 0, 0, 0},	/* N_POSTERR */
  {"X-Reply-UID", FreeText,	0, 0, 0, 0},	/* N_RPLUID */
  {"X-Reply-Mbox",FreeText,	0, 0, 0, 0},	/* N_RPLMBOX */
  {"X-SMTP-Server",FreeText,	0, 0, 0, 0},	/* N_SMTP */
  {"X-NNTP-Server",FreeText,	0, 0, 0, 0},	/* N_NNTP */
  {"X-Cursor-Pos",FreeText,	0, 0, 0, 0},	/* N_CURPOS */
  {"X-Our-ReplyTo",FreeText,	0, 0, 0, 0},	/* N_OURREPLYTO */
  {OUR_HDRS_LIST, FreeText,	0, 0, 0, 0},	/* N_OURHDRS */
#if	!(defined(DOS) || defined(OS2)) || defined(NOAUTH)
  {"X-X-Sender",    Address,	0, 1, 1, 0},
#endif
  {NULL,         FreeText}
};


/*
 * Note, these are in the same order in the he_template and pf_template arrays.
 */
#define	N_AUTHRCVD	 0
#define N_FROM		 1
#define N_REPLYTO	(N_FROM + 1)
#define N_TO		(N_FROM + 2)
#define N_CC		(N_FROM + 3)
#define N_BCC		(N_FROM + 4)
#define N_NEWS		(N_FROM + 5)
#define N_FCC		(N_FROM + 6)
#define N_LCC		(N_FROM + 7)
#define N_ATTCH		(N_FROM + 8)
#define N_SUBJ		(N_FROM + 9)
#define N_REF		(N_FROM + 10)
#define N_DATE		(N_FROM + 11)
#define N_INREPLY	(N_FROM + 12)
#define N_MSGID		(N_FROM + 13)
#define N_NOBODY	(N_FROM + 14)
#define	N_POSTERR	(N_FROM + 15)
#define	N_RPLUID	(N_FROM + 16)
#define	N_RPLMBOX	(N_FROM + 17)
#define	N_SMTP		(N_FROM + 18)
#define	N_NNTP		(N_FROM + 19)
#define	N_CURPOS	(N_FROM + 20)
#define	N_OURREPLYTO	(N_FROM + 21)
#define	N_OURHDRS	(N_FROM + 22)
#define N_SENDER	(N_FROM + 23)


/* ugly globals we'd like to get rid of */
extern struct local_message_copy lmc;
extern char verbose_requested;
extern unsigned char dsn_requested;


/* exported protoypes */
int         postponed_stream(MAILSTREAM **);
int	    redraft_work(MAILSTREAM **, long, ENVELOPE **, BODY **, char **, char **,
			 REPLY_S **, REDRAFT_POS_S **, PINEFIELD **, ACTION_S **, int, STORE_S *);
int	    redraft_cleanup(MAILSTREAM **, int, int);
void	    simple_header_parse(char *, char **, char **);
REPLY_S    *build_reply_uid(char *);
METAENV	   *pine_new_env(ENVELOPE *, char **, char ***, PINEFIELD *);
void	    pine_free_env(METAENV **);
int	    check_addresses(METAENV *);
void	    update_answered_flags(REPLY_S *);
ADDRESS	   *phone_home_from(void);
unsigned int phone_home_hash(char *);
int         call_mailer(METAENV *, BODY *, char **, int, void (*)(char *, int));
int         write_postponed(METAENV *, BODY *);
int         commence_fcc(char *, CONTEXT_S **, int);
int         wrapup_fcc(char *, CONTEXT_S *, METAENV *, BODY *);
STORE_S	   *open_fcc(char *, CONTEXT_S **, int, char *, char *);
int	    write_fcc(char *, CONTEXT_S *, STORE_S *, MAILSTREAM *, char *, char *);
BODY	   *first_text_8bit(BODY *);
ADDRESS    *generate_from(void);
void	    set_mime_type_by_grope(BODY *, char *);
void	    set_only_charset_by_grope(BODY *, char *);
void	    set_mime_charset(PARAMETER *, int, char *);
void        pine_encode_body(BODY *);
int         pine_header_line(char *, METAENV *, char *, soutr_t, TCPSTREAM *, int, int);
char       *encode_header_value(char *, size_t, unsigned char *, char *, int);
int	    pine_address_line(char *, METAENV *, ADDRESS *, soutr_t, TCPSTREAM *, int, int);
long        pine_rfc822_header(METAENV *, BODY *, soutr_t, TCPSTREAM *);
long        pine_rfc822_output(METAENV *, BODY *, soutr_t, TCPSTREAM *);
void	    pine_free_body(BODY **);
long	    send_body_size(BODY *);
long	    pine_smtp_verbose(SENDSTREAM *);
void	    pine_smtp_verbose_out(char *);
int         pine_header_forbidden(char *);
int         hdr_is_in_list(char *, PINEFIELD *);
int         count_custom_hdrs_pf(PINEFIELD *, int);
int         count_custom_hdrs_list(char **);
CustomType  set_default_hdrval(PINEFIELD *, PINEFIELD *);
FieldType   pine_header_standard(char *);
void        customized_hdr_setup(PINEFIELD *, char **, CustomType);
void        add_defaults_from_list(PINEFIELD *, char **);
PINEFIELD  *parse_custom_hdrs(char **, CustomType);
PINEFIELD  *combine_custom_headers(PINEFIELD *, PINEFIELD *);
void        free_customs(PINEFIELD *);
int         encode_whole_header(char *, METAENV *);
int         news_poster(METAENV *, BODY *, char **);


#endif /* PITH_SEND_INCLUDED */
