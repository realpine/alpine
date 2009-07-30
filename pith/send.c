#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: send.c 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $";
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

#include "../pith/headers.h"
#include "../pith/send.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/store.h"
#include "../pith/mimedesc.h"
#include "../pith/context.h"
#include "../pith/status.h"
#include "../pith/folder.h"
#include "../pith/bldaddr.h"
#include "../pith/pipe.h"
#include "../pith/mailview.h"
#include "../pith/mailindx.h"
#include "../pith/list.h"
#include "../pith/filter.h"
#include "../pith/reply.h"
#include "../pith/addrstring.h"
#include "../pith/rfc2231.h"
#include "../pith/stream.h"
#include "../pith/util.h"
#include "../pith/adrbklib.h"
#include "../pith/options.h"
#include "../pith/busy.h"
#include "../pith/text.h"
#include "../pith/imap.h"
#include "../pith/ablookup.h"
#include "../pith/sort.h"
#include "../pith/smime.h"

#include "../c-client/smtp.h"
#include "../c-client/nntp.h"


/* this is used in pine_send and pine_simple_send */
/* name::type::canedit::writehdr::localcopy::rcptto */
PINEFIELD pf_template[] = {
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
  {PRIORITYNAME,  FreeText,	0, 1, 1, 0},
  {"User-Agent",  FreeText,	0, 1, 1, 0},
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


PRIORITY_S priorities[] = {
    {1, "Highest"},
    {2, "High"},
    {3, "Normal"},
    {4, "Low"},
    {5, "Lowest"},
    {0, NULL}
};


#define ctrl(c) ((c) & 0x1f)

/* which message part to test for xliteration */
typedef enum {MsgBody, HdrText} MsgPart;


/*
 * Internal prototypes
 */
long       post_rfc822_output(char *, ENVELOPE *, BODY *, soutr_t, TCPSTREAM *, long);
int        l_flush_net(int);
int        l_putc(int);
int	   pine_write_header_line(char *, char *, STORE_S *);
int	   pine_write_params(PARAMETER *, STORE_S *);
char      *tidy_smtp_mess(char *, char *, char *, size_t);
int	   lmc_body_header_line(char *, int);
int	   lmc_body_header_finish(void);
int	   pwbh_finish(int, STORE_S *);
int	   sent_percent(void);
unsigned short *setup_avoid_table(void);
#ifndef	_WINDOWS
int	   mta_handoff(METAENV *, BODY *, char *, size_t, void (*)(char *, int),
		       void (*)(PIPE_S *, int, void *));
char	  *post_handoff(METAENV *, BODY *, char *, size_t, void (*)(char *, int),
			void (*)(PIPE_S *, int, void *));
char	  *mta_parse_post(METAENV *, BODY *, char *, char *, size_t, void (*)(char *, int),
			  void (*)(PIPE_S *, int, void *));
long	   pine_pipe_soutr_nl(void *, char *);
#endif
char	  *smtp_command(char *, size_t);
void	  *piped_smtp_open(char *, char *, unsigned long);
void	  *piped_aopen(NETMBX *, char *, char *);
long	   piped_soutr(void *, char *);
long	   piped_sout(void *, char *, unsigned long);
char	  *piped_getline(void *);
void	   piped_close(void *);
void	   piped_abort(void *);
char	  *piped_host(void *);
unsigned long piped_port(void *);
char	  *posting_characterset(void *, char *, MsgPart);
int        body_is_translatable(void *, char *);
int        text_is_translatable(void *, char *);
int        dummy_putc(int);
unsigned long *init_charsetchecker(char *);
int        representable_in_charset(unsigned long, char *);
char      *most_preferred_charset(unsigned long);

/* 
 * Storage object where the FCC (or postponed msg) is to be written.
 * This is amazingly bogus.  Much work was done to put messages 
 * together and encode them as they went to the tmp file for sendmail
 * or into the SMTP slot (especially for for DOS, to prevent a temporary
 * file (and needlessly copying the message).
 * 
 * HOWEVER, since there's no piping into c-client routines
 * (particularly mail_append() which copies the fcc), the fcc will have
 * to be copied to disk.  This global tells pine's copy of the rfc822
 * output functions where to also write the message bytes for the fcc.
 * With piping in the c-client we could just have two pipes to shove
 * down rather than messing with damn copies.  FIX THIS!
 *
 * The function open_fcc, locates the actual folder and creates it if
 * requested before any mailing or posting is done.
 */
struct local_message_copy lmc;


/*
 * Locally global pointer to stream used for sending/posting.
 * It's also used to indicate when/if we write the Bcc: field in
 * the header.
 */
static SENDSTREAM *sending_stream = NULL;


static struct hooks {
    void	*rfc822_out;			/* Message outputter */
} sending_hooks;


static FILE	  *verbose_send_output = NULL;
static long	   send_bytes_sent, send_bytes_to_send;
static METAENV	  *send_header = NULL;

/*
 * Hooks for prompts and confirmations
 */
int (*pith_opt_daemon_confirm)(void);


static NETDRIVER piped_io = {
    piped_smtp_open,		/* open a connection */
    piped_aopen,		/* open an authenticated connection */
    piped_getline,		/* get a line */
    NULL,			/* get a buffer */
    piped_soutr,		/* output pushed data */
    piped_sout,			/* output string */
    piped_close,		/* close connection */
    piped_host,			/* return host name */
    piped_host,			/* remotehost */
    piped_port,			/* return port number */
    piped_host			/* return local host (NOTE: same as host!) */
};


/*
 * Since c-client preallocates, it's necessary here to define a limit
 * such that we don't blow up in c-client (see rfc822_address_line()).
 */
#define	MAX_SINGLE_ADDR	MAILTMPLEN

#define AVOID_2022_JP_FOR_PUNC "AVOID_2022_JP_FOR_PUNC"


/*
 * Phone home hash controls
 */
#define PH_HASHBITS	24
#define PH_MAXHASH	(1<<(PH_HASHBITS))


/*
 * postponed_stream - return stream associated with postponed messages
 *                    in argument.
 */
int
postponed_stream(MAILSTREAM **streamp, char *mbox, char *type, int checknmsgs)
{
    MAILSTREAM *stream = NULL;
    CONTEXT_S  *p_cntxt = NULL;
    char       *p, *q, tmp[MAILTMPLEN], *fullname = NULL;
    int	        exists;

    if(!(streamp && mbox))
      return(0);

    *streamp = NULL;

    /*
     * find default context to look for folder...
     *
     * The "mbox" is assumed to be local if we're given what looks
     * like an absolute path.  This is different from Goto/Save
     * where we do alot of work to interpret paths relative to the
     * server.  This reason is to support all the pre-4.00 pinerc'
     * that specified a path and because there's yet to be a way
     * in c-client to specify otherwise in the face of a remote
     * context.
     */
    if(!is_absolute_path(mbox)
       && !(p_cntxt = default_save_context(ps_global->context_list)))
      p_cntxt = ps_global->context_list;

    /* check to see if the folder exists, the user wants to continue
     * and that we can actually read something in...
     */
    exists = folder_name_exists(p_cntxt, mbox, &fullname);
    if(fullname)
      mbox = fullname;

    if(exists & FEX_ISFILE){
	context_apply(tmp, p_cntxt, mbox, sizeof(tmp));
	if(!(IS_REMOTE(tmp) || is_absolute_path(tmp))){
	    /*
	     * The mbox is relative to the home directory.
	     * Make it absolute so we can compare it to
	     * stream->mailbox.
	     */
	    build_path(tmp_20k_buf, ps_global->ui.homedir, tmp,
		       SIZEOF_20KBUF);
	    strncpy(tmp, tmp_20k_buf, sizeof(tmp));
	    tmp[sizeof(tmp)-1] = '\0';
	}

	if((stream = ps_global->mail_stream)
	   && !(stream->mailbox
		&& ((*tmp != '{' && !strcmp(tmp, stream->mailbox))
		    || (*tmp == '{'
			&& same_stream(tmp, stream)
			&& (p = strchr(tmp, '}'))
			&& (q = strchr(stream->mailbox,'}'))
			&& !strcmp(p + 1, q + 1)))))
	  stream = NULL;

	if(!stream){
	    stream = context_open(p_cntxt, NULL, mbox,
				  SP_USEPOOL|SP_TEMPUSE, NULL);
	    if(stream && !stream->halfopen){
		if(stream->nmsgs > 0)
		  refresh_sort(stream, sp_msgmap(stream), SRT_NON);

		if(checknmsgs && stream->nmsgs < 1){
		    pine_mail_close(stream);
		    exists = 0;
		    stream = NULL;
		}
	    }
	    else{
		q_status_message2(SM_ORDER | SM_DING, 3, 3,
				  _("Can't open %s mailbox: %s"), type, mbox);
		if(stream)
		  pine_mail_close(stream);

		exists = 0;
		stream = NULL;
	    }
	}
    }
    else{
	if(F_ON(F_ALT_COMPOSE_MENU, ps_global)){
	    q_status_message1(SM_ORDER | SM_DING, 3, 3,
			     _("%s message folder doesn't exist!"), type);
	}
    }

    if(fullname)
      fs_give((void **) &fullname);

    *streamp = stream;

    return(exists);
}


int
redraft_work(MAILSTREAM **streamp, long int cont_msg, ENVELOPE **outgoing,
	     struct mail_bodystruct **body, char **fcc, char **lcc,
	     REPLY_S **reply, REDRAFT_POS_S **redraft_pos, PINEFIELD **custom,
	     ACTION_S **role, int flags, STORE_S *so)
{
    MAILSTREAM	*stream;
    ENVELOPE	*e = NULL;
    BODY	*b;
    PART        *part;
    PINEFIELD   *pf;
    gf_io_t	 pc;
    char	*extras, **fields, **values, *p;
    char        *hdrs[2], *h, *charset;
    char       **smtp_servers = NULL, **nntp_servers = NULL;
    int		 i, pine_generated = 0, our_replyto = 0;
    int          added_to_role = 0;
    unsigned	 gbpt_flags = GBPT_NONE;
    MESSAGECACHE *mc;

    if(!(streamp && *streamp))
      return(redraft_cleanup(streamp, TRUE, flags));

    stream = *streamp;

    if(flags & REDRAFT_HTML)
      gbpt_flags |= GBPT_HTML_OK;

    /* grok any user-defined or non-c-client headers */
    if((e = pine_mail_fetchstructure(stream, cont_msg, &b)) != NULL){

	/*
	 * The custom headers to look for in the suspended message should
	 * have been stored in the X-Our-Headers header. So first we get
	 * that list. If we can't find it (version that stored the
	 * message < 4.30) then we use the global custom list.
	 */
	hdrs[0] = OUR_HDRS_LIST;
	hdrs[1] = NULL;
	if((h = pine_fetchheader_lines(stream, cont_msg, NULL, hdrs)) != NULL){
	    int    commas = 0;
	    char **list;
	    char  *hdrval = NULL;

	    if((hdrval = strindex(h, ':')) != NULL){
		for(hdrval++; *hdrval && isspace((unsigned char)*hdrval); 
		    hdrval++)
		  ;
	    }

	    /* count elements in list */
	    for(p = hdrval; p && *p; p++)
	      if(*p == ',')
		commas++;

	    if(hdrval && (list = parse_list(hdrval,commas+1,0,NULL)) != NULL){
		
		*custom = parse_custom_hdrs(list, Replace);
		add_defaults_from_list(*custom,
				       ps_global->VAR_CUSTOM_HDRS);
		free_list_array(&list);
	    }

	    if(*custom && !(*custom)->name){
		free_customs(*custom);
		*custom = NULL;
	    }

	    fs_give((void **)&h);
	}

	if(!*custom)
	  *custom = parse_custom_hdrs(ps_global->VAR_CUSTOM_HDRS, UseAsDef);

#define INDEX_FCC         0
#define INDEX_POSTERR     1
#define INDEX_REPLYUID    2
#define INDEX_REPLYMBOX   3
#define INDEX_SMTP        4
#define INDEX_NNTP        5
#define INDEX_CURSORPOS   6
#define INDEX_OUR_REPLYTO 7
#define INDEX_LCC         8	/* MUST REMAIN LAST FIELD DECLARED */
#define FIELD_COUNT       9

	i = count_custom_hdrs_pf(*custom,1) + FIELD_COUNT + 1;

	/*
	 * Having these two fields separated isn't the slickest, but
	 * converting the pointer array for fetchheader_lines() to
	 * a list of structures or some such for simple_header_parse()
	 * is too goonie.  We could do something like re-use c-client's
	 * PARAMETER struct which is a simple char * pairing, but that
	 * doesn't make sense to pass to fetchheader_lines()...
	 */
	fields = (char **) fs_get((size_t) i * sizeof(char *));
	values = (char **) fs_get((size_t) i * sizeof(char *));
	memset(fields, 0, (size_t) i * sizeof(char *));
	memset(values, 0, (size_t) i * sizeof(char *));

	fields[i = INDEX_FCC] = "Fcc";	/* Fcc: special case */
	fields[++i]   = "X-Post-Error";	/* posting errors too */
	fields[++i]   = "X-Reply-UID";	/* Reply'd to msg's UID */
	fields[++i]   = "X-Reply-Mbox";	/* Reply'd to msg's Mailbox */
	fields[++i]   = "X-SMTP-Server";/* SMTP server to use */
	fields[++i]   = "X-NNTP-Server";/* NNTP server to use */
	fields[++i]   = "X-Cursor-Pos";	/* Cursor position */
	fields[++i]   = "X-Our-ReplyTo";	/* ReplyTo is real */
	fields[++i]   = "Lcc";		/* Lcc: too... */
	if(++i != FIELD_COUNT)
	  panic("Fix FIELD_COUNT");

	for(pf = *custom; pf && pf->name; pf = pf->next)
	  if(!pf->standard)
	    fields[i++] = pf->name;		/* assign custom fields */

	if((extras = pine_fetchheader_lines(stream, cont_msg, NULL,fields)) != NULL){
	    simple_header_parse(extras, fields, values);
	    fs_give((void **) &extras);

	    /*
	     * translate RFC 1522 strings,
	     * starting with "Lcc" field
	     */
	    for(i = INDEX_LCC; fields[i]; i++)
	      if(values[i]){
		  size_t len;
		  char *bufp, *biggerbuf = NULL;
  
		  if((len=4*strlen(values[i])) > SIZEOF_20KBUF-1){
		      len++;
		      biggerbuf = (char *)fs_get(len * sizeof(char));
		      bufp = biggerbuf;
		  }
		  else{
		      bufp = tmp_20k_buf;
		      len = SIZEOF_20KBUF;
		  }
  
		  p = (char *)rfc1522_decode_to_utf8((unsigned char*)bufp, len, values[i]);

		  if(p == tmp_20k_buf){
		      fs_give((void **)&values[i]);
		      values[i] = cpystr(p);
		  }

		  if(biggerbuf)
		    fs_give((void **)&biggerbuf);
	      }

	    for(pf = *custom, i = FIELD_COUNT;
		pf && pf->name;
		pf = pf->next){
		if(pf->standard){
		    /*
		     * Because the value is already in the envelope.
		     */
		    pf->cstmtype = NoMatch;
		    continue;
		}

		if(values[i]){	/* use this instead of default */
		    if(pf->textbuf)
		      fs_give((void **)&pf->textbuf);

		    pf->textbuf = values[i]; /* freed in pine_send! */
		}
		else if(pf->textbuf)  /* was erased before postpone */
		  fs_give((void **)&pf->textbuf);
		
		i++;
	    }

	    if(values[INDEX_FCC])		/* If "Fcc:" was there...  */
	      pine_generated = 1;		/* we put it there? */

	    /*
	     * Since c-client fills in the reply_to field in the envelope
	     * even if there isn't a Reply-To header in the message we
	     * have to work around that. When we postpone we add
	     * a second header that has value "Empty" if there really
	     * was a Reply-To and it was empty. It has the
	     * value "Full" if we put the Reply-To contents there
	     * intentionally (and it isn't empty).
	     */
	    if(values[INDEX_OUR_REPLYTO]){
		if(values[INDEX_OUR_REPLYTO][0] == 'E')
		  our_replyto = 'E';  /* we put an empty one there */
		else if(values[INDEX_OUR_REPLYTO][0] == 'F')
		  our_replyto = 'F';  /* we put it there */

		fs_give((void **) &values[INDEX_OUR_REPLYTO]);
	    }

	    if(fcc)				/* fcc: special case... */
	      *fcc = values[INDEX_FCC] ? values[INDEX_FCC] : cpystr("");
	    else if(values[INDEX_FCC])
	      fs_give((void **) &values[INDEX_FCC]);

	    if(values[INDEX_POSTERR]){		/* x-post-error?!?1 */
		q_status_message(SM_ORDER|SM_DING, 4, 4,
				 values[INDEX_POSTERR]);
		fs_give((void **) &values[INDEX_POSTERR]);
	    }

	    if(values[INDEX_REPLYUID]){
		if(reply)
		  *reply = build_reply_uid(values[INDEX_REPLYUID]);

		fs_give((void **) &values[INDEX_REPLYUID]);

		if(values[INDEX_REPLYMBOX] && reply && *reply)
		  (*reply)->origmbox = cpystr(values[INDEX_REPLYMBOX]);
		
		if(reply && *reply && !(*reply)->origmbox && (*reply)->mailbox)
		  (*reply)->origmbox = cpystr((*reply)->mailbox);
	    }

	    if(values[INDEX_REPLYMBOX])
	      fs_give((void **) &values[INDEX_REPLYMBOX]);

	    if(values[INDEX_SMTP]){
		char  *q;
		size_t cnt = 0;

		/*
		 * Turn the space delimited list of smtp servers into
		 * a char ** list.
		 */
		p = values[INDEX_SMTP];
		do{
		    if(!*p || isspace((unsigned char) *p))
		      cnt++;
		} while(*p++);
		
		smtp_servers = (char **) fs_get((cnt+1) * sizeof(char *));
		memset(smtp_servers, 0, (cnt+1) * sizeof(char *));

		cnt = 0;
		q = p = values[INDEX_SMTP];
		do{
		    if(!*p || isspace((unsigned char) *p)){
			if(*p){
			    *p = '\0';
			    smtp_servers[cnt++] = cpystr(q);
			    *p = ' ';
			    q = p+1;
			}
			else
			  smtp_servers[cnt++] = cpystr(q);
		    }
		} while(*p++);

		fs_give((void **) &values[INDEX_SMTP]);
	    }

	    if(values[INDEX_NNTP]){
		char  *q;
		size_t cnt = 0;

		/*
		 * Turn the space delimited list of smtp nntp into
		 * a char ** list.
		 */
		p = values[INDEX_NNTP];
		do{
		    if(!*p || isspace((unsigned char) *p))
		      cnt++;
		} while(*p++);
		
		nntp_servers = (char **) fs_get((cnt+1) * sizeof(char *));
		memset(nntp_servers, 0, (cnt+1) * sizeof(char *));

		cnt = 0;
		q = p = values[INDEX_NNTP];
		do{
		    if(!*p || isspace((unsigned char) *p)){
			if(*p){
			    *p = '\0';
			    nntp_servers[cnt++] = cpystr(q);
			    *p = ' ';
			    q = p+1;
			}
			else
			  nntp_servers[cnt++] = cpystr(q);
		    }
		} while(*p++);

		fs_give((void **) &values[INDEX_NNTP]);
	    }

	    if(values[INDEX_CURSORPOS]){
		/*
		 * The redraft cursor position is written as two fields
		 * separated by a space. First comes the name of the
		 * header field we're in, or just a ":" if we're in the
		 * body. Then comes the offset into that header or into
		 * the body.
		 */
		if(redraft_pos){
		    char *q1, *q2;

		    *redraft_pos
			      = (REDRAFT_POS_S *)fs_get(sizeof(REDRAFT_POS_S));
		    (*redraft_pos)->offset = 0L;

		    q1 = skip_white_space(values[INDEX_CURSORPOS]);
		    if(*q1 && (q2 = strindex(q1, SPACE))){
			*q2 = '\0';
			(*redraft_pos)->hdrname = cpystr(q1);
			q1 = skip_white_space(q2+1);
			if(*q1)
			  (*redraft_pos)->offset = atol(q1);
		    }
		    else
		      (*redraft_pos)->hdrname = cpystr(":");
		}

		fs_give((void **) &values[INDEX_CURSORPOS]);
	    }

	    if(lcc)
	      *lcc = values[INDEX_LCC];
	    else
	      fs_give((void **) &values[INDEX_LCC]);
	}

	fs_give((void **)&fields);
	fs_give((void **)&values);

	*outgoing = copy_envelope(e);

	/*
	 * If the postponed message has a From which is different from
	 * the default, it is either because allow-changing-from is on
	 * or because there was a role with a from that allowed it to happen.
	 * If allow-changing-from is not on, put this back in a role
	 * so that it will be allowed again in pine_send.
	 */
	if(role && *role == NULL &&
	   !ps_global->never_allow_changing_from &&
	   *outgoing){
	    /*
	     * Now check to see if the from is different from default from.
	     */
	    ADDRESS *deffrom;

	    deffrom = generate_from();
	    if(!((*outgoing)->from &&
		 address_is_same(deffrom, (*outgoing)->from) &&
	         ((!(deffrom->personal && deffrom->personal[0]) &&
		   !((*outgoing)->from->personal &&
		     (*outgoing)->from->personal[0])) ||
		  (deffrom->personal && (*outgoing)->from->personal &&
		   !strcmp(deffrom->personal, (*outgoing)->from->personal))))){

		*role = (ACTION_S *)fs_get(sizeof(**role));
		memset((void *)*role, 0, sizeof(**role));
		if(!(*outgoing)->from)
		  (*outgoing)->from = mail_newaddr();

		(*role)->from = (*outgoing)->from;
		(*outgoing)->from = NULL;
		added_to_role++;
	    }

	    mail_free_address(&deffrom);
	}

	/*
	 * Look at each empty address and see if the user has specified
	 * a default for that field or not.  If they have, that means
	 * they have erased it before postponing, so they won't want
	 * the default to come back.  If they haven't specified a default,
	 * then the default should be generated in pine_send.  We prevent
	 * the default from being assigned by assigning an empty address
	 * to the variable here.
	 *
	 * BUG: We should do this for custom Address headers, too, but
	 * there isn't such a thing yet.
	 */
	if(!(*outgoing)->to && hdr_is_in_list("to", *custom))
	  (*outgoing)->to = mail_newaddr();
	if(!(*outgoing)->cc && hdr_is_in_list("cc", *custom))
	  (*outgoing)->cc = mail_newaddr();
	if(!(*outgoing)->bcc && hdr_is_in_list("bcc", *custom))
	  (*outgoing)->bcc = mail_newaddr();

	if(our_replyto == 'E'){
	    /* user erased reply-to before postponing */
	    if((*outgoing)->reply_to)
	      mail_free_address(&(*outgoing)->reply_to);

	    /*
	     * If empty is not the normal default, make the outgoing
	     * reply_to be an emtpy address. If it is default, leave it
	     * as NULL and the default will be used.
	     */
	    if(hdr_is_in_list("reply-to", *custom)){
		PINEFIELD pf;

		pf.name = "reply-to";
		set_default_hdrval(&pf, *custom);
		if(pf.textbuf){
		    if(pf.textbuf[0])	/* empty is not default */
		      (*outgoing)->reply_to = mail_newaddr();

		    fs_give((void **)&pf.textbuf);
		}
	    }
	}
	else if(our_replyto == 'F'){
	    int add_to_role = 0;

	    /*
	     * The reply-to is real. If it is different from the default
	     * reply-to, put it in the role so that it will show up when
	     * the user edits.
	     */
	    if(hdr_is_in_list("reply-to", *custom)){
		PINEFIELD pf;
		char *str;

		pf.name = "reply-to";
		set_default_hdrval(&pf, *custom);
		if(pf.textbuf && pf.textbuf[0]){
		    if((str = addr_list_string((*outgoing)->reply_to,NULL,1)) != NULL){
			if(!strcmp(str, pf.textbuf)){
			    /* standard value, leave it alone */
			    ;
			}
			else  /* not standard, put in role */
			  add_to_role++;

			fs_give((void **)&str);
		    }
		}
		else  /* not standard, put in role */
		  add_to_role++;

		if(pf.textbuf)
		  fs_give((void **)&pf.textbuf);
	    }
	    else  /* not standard, put in role */
	      add_to_role++;

	    if(add_to_role && role && (*role == NULL || added_to_role)){
		if(*role == NULL){
		    added_to_role++;
		    *role = (ACTION_S *)fs_get(sizeof(**role));
		    memset((void *)*role, 0, sizeof(**role));
		}

		(*role)->replyto = (*outgoing)->reply_to;
		(*outgoing)->reply_to = NULL;
	    }
	}
	else{
	    /* this is a bogus c-client generated replyto */
	    if((*outgoing)->reply_to)
	      mail_free_address(&(*outgoing)->reply_to);
	}

	if((smtp_servers || nntp_servers)
	   && role && (*role == NULL || added_to_role)){
	    if(*role == NULL){
		*role = (ACTION_S *)fs_get(sizeof(**role));
		memset((void *)*role, 0, sizeof(**role));
	    }

	    if(smtp_servers)
	      (*role)->smtp = smtp_servers;
	    if(nntp_servers)
	      (*role)->nntp = nntp_servers;
	}

	if(!(*outgoing)->subject && hdr_is_in_list("subject", *custom))
	  (*outgoing)->subject = cpystr("");

	if(!pine_generated){
	    /*
	     * Now, this is interesting.  We should have found 
	     * the "fcc:" field if pine wrote the message being
	     * redrafted.  Hence, we probably can't trust the 
	     * "originator" type fields, so we'll blast them and let
	     * them get set later in pine_send.  This should allow
	     * folks with custom or edited From's and such to still
	     * use redraft reasonably, without inadvertently sending
	     * messages that appear to be "From" others...
	     */
	    if((*outgoing)->from)
	      mail_free_address(&(*outgoing)->from);

	    /*
	     * Ditto for Reply-To and Sender...
		 */
	    if((*outgoing)->reply_to)
	      mail_free_address(&(*outgoing)->reply_to);

	    if((*outgoing)->sender)
	      mail_free_address(&(*outgoing)->sender);
	}

	if(!pine_generated || !(flags & REDRAFT_DEL)){

	    /*
	     * Generate a fresh message id for pretty much the same
	     * reason From and such got wacked...
	     * Also, if we're coming from a form letter, we need to
	     * generate a different id each time.
	     */
	    if((*outgoing)->message_id)
	      fs_give((void **)&(*outgoing)->message_id);

	    (*outgoing)->message_id = generate_message_id();
	}

	if(b && b->type != TYPETEXT){
	  if(b->type == TYPEMULTIPART){
	      if(strucmp(b->subtype, "mixed")){
		  q_status_message1(SM_INFO, 3, 4, 
			     "Converting Multipart/%s to Multipart/Mixed", 
				 b->subtype);
		  fs_give((void **)&b->subtype);
		  b->subtype = cpystr("mixed");
	      }
	  }
	  else{
	      q_status_message2(SM_ORDER | SM_DING, 3, 4, 
				"Unable to resume type %s/%s message",
				body_types[b->type], b->subtype);
	      return(redraft_cleanup(streamp, TRUE, flags));
	  }
	}
		
	gf_set_so_writec(&pc, so);

	if(b && b->type != TYPETEXT){	/* already TYPEMULTIPART */
	    *body			   = copy_body(NULL, b);
	    part			   = (*body)->nested.part;
	    part->body.contents.text.data = (void *)so;
	    set_mime_type_by_grope(&part->body);
	    if(part->body.type != TYPETEXT){
		q_status_message2(SM_ORDER | SM_DING, 3, 4,
		      "Unable to resume; first part is non-text: %s/%s",
				  body_types[part->body.type], 
				  part->body.subtype);
		return(redraft_cleanup(streamp, TRUE, flags));
	    }
		
	    if((charset = parameter_val(part->body.parameter,"charset")) != NULL){
		/* let outgoing routines decide on charset */
		if(!strucmp(charset, "US-ASCII") || !strucmp(charset, "UTF-8"))
		  set_parameter(&part->body.parameter, "charset", NULL);
		  
		fs_give((void **) &charset);
	    }

	    ps_global->postpone_no_flow = 1;

	    get_body_part_text(stream, &b->nested.part->body,
			       cont_msg, "1", 0L, pc, NULL, NULL, gbpt_flags);
	    ps_global->postpone_no_flow = 0;

	    if(!fetch_contents(stream, cont_msg, NULL, *body))
	      q_status_message(SM_ORDER | SM_DING, 3, 4,
			       _("Error including all message parts"));
	}
	else{
	    *body	  = mail_newbody();
	    (*body)->type = TYPETEXT;
	    if(b->subtype)
	      (*body)->subtype = cpystr(b->subtype);

	    if((charset = parameter_val(b->parameter,"charset")) != NULL){
		/* let outgoing routines decide on charset */
		if(!strucmp(charset, "US-ASCII") || !strucmp(charset, "UTF-8"))
		  fs_give((void **) &charset);
		else{
		    (*body)->parameter	      = mail_newbody_parameter();
		    (*body)->parameter->attribute = cpystr("charset");
		    if(utf8_charset(charset)){
			fs_give((void **) &charset);
			(*body)->parameter->value = cpystr("UTF-8");
		    }
		    else
		      (*body)->parameter->value = charset;
		}
	    }

	    (*body)->contents.text.data = (void *)so;
	    ps_global->postpone_no_flow = 1;
	    get_body_part_text(stream, b, cont_msg, "1", 0L, pc,
			       NULL, NULL, gbpt_flags);
	    ps_global->postpone_no_flow = 0;
	}

	gf_clear_so_writec(so);

	    /* We have what we want, blast this message... */
	if((flags & REDRAFT_DEL)
	   && cont_msg > 0L && stream && cont_msg <= stream->nmsgs
	   && (mc = mail_elt(stream, cont_msg)) && !mc->deleted)
	  mail_flag(stream, long2string(cont_msg), "\\DELETED", ST_SET);
    }
    else
      return(redraft_cleanup(streamp, TRUE, flags));

    return(redraft_cleanup(streamp, FALSE, flags));
}


/*----------------------------------------------------------------------
    Clear deleted messages from given stream and expunge if necessary

Args:	stream --
	problem --

 ----*/
int
redraft_cleanup(MAILSTREAM **streamp, int problem, int flags)
{
    MAILSTREAM *stream;

    if(!(streamp && *streamp))
      return(0);

    if(!problem && streamp && (stream = *streamp)){
	if(stream->nmsgs){
	    ps_global->expunge_in_progress = 1;
	    mail_expunge(stream);		/* clean out deleted */
	    ps_global->expunge_in_progress = 0;
	}

	if(!stream->nmsgs){			/* close and delete folder */
	    int do_the_broach = 0;
	    char *mbox = NULL;

	    if(stream){
		if(stream->original_mailbox && stream->original_mailbox[0])
		  mbox = cpystr(stream->original_mailbox);
		else if(stream->mailbox && stream->mailbox[0])
		  mbox = cpystr(stream->mailbox);
	    }

	    /* if it is current, we have to change folders */
	    if(stream == ps_global->mail_stream)
	      do_the_broach++;

	    /*
	     * This is the stream to the empty postponed-msgs folder.
	     * We are going to delete the folder in a second. It is
	     * probably preferable to unselect the mailbox and leave
	     * this stream open for re-use instead of actually closing it,
	     * so we do that if possible.
	     */
	    if(is_imap_stream(stream) && LEVELUNSELECT(stream)){
		/*
		 * This does the UNSELECT on the stream. A NULL
		 * return should mean that something went wrong and
		 * a mail_close already happened, so that should have
		 * cleaned things up in the callback.
		 */
		if((stream=mail_open(stream, stream->mailbox,
			  OP_HALFOPEN | (stream->debug ? OP_DEBUG : NIL))) != NULL){
		    /* now close it so it is put into the stream cache */
		    sp_set_flags(stream, sp_flags(stream) | SP_TEMPUSE);
		    pine_mail_close(stream);
		}
	    }
	    else
	      pine_mail_actually_close(stream);

	    *streamp = NULL;

	    if(do_the_broach){
		ps_global->mail_stream = NULL;	/* already closed above */
	    }

	    if(mbox && !pine_mail_delete(NULL, mbox))
	      q_status_message1(SM_ORDER|SM_DING, 3, 3,
				/* TRANSLATORS: Arg is a mailbox name */
				_("Can't delete %s"), mbox);

	    if(mbox)
	      fs_give((void **) &mbox);
	}
    }

    return(!problem);
}


/*----------------------------------------------------------------------
    Parse the given header text for any given fields

Args:  text -- Text to parse for fcc and attachments refs
       fields -- array of field names to look for
       values -- array of pointer to save values to, returned NULL if
                 fields isn't in text.

This function simply looks for the given fields in the given header
text string.
NOTE: newlines are expected CRLF, and we'll ignore continuations
 ----*/
void
simple_header_parse(char *text, char **fields, char **values)
{
    int   i, n;
    char *p, *t;

    for(i = 0; fields[i]; i++)
      values[i] = NULL;				/* clear values array */

    /*---- Loop until the end of the header ----*/
    for(p = text; *p; ){
	for(i = 0; fields[i]; i++)		/* find matching field? */
	  if(!struncmp(p, fields[i], (n=strlen(fields[i]))) && p[n] == ':'){
	      for(p += n + 1; *p; p++){		/* find start of value */
		  if(*p == '\015' && *(p+1) == '\012'
		     && !isspace((unsigned char) *(p+2)))
		    break;

		  if(!isspace((unsigned char) *p))
		    break;			/* order here is key... */
	      }

	      if(!values[i]){			/* if we haven't already */
		  values[i] = fs_get(strlen(text) + 1);
		  values[i][0] = '\0';		/* alloc space for it */
	      }

	      if(*p && *p != '\015'){		/* non-blank value. */
		  t = values[i] + (values[i][0] ? strlen(values[i]) : 0);
		  while(*p){			/* check for cont'n lines */
		      if(*p == '\015' && *(p+1) == '\012'){
			  if(isspace((unsigned char) *(p+2))){
			      p += 2;
			      continue;
			  }
			  else
			    break;
		      }

		      *t++ = *p++;
		  }

		  *t = '\0';
	      }

	      break;
	  }

        /* Skip to end of line, what ever it was */
        for(; *p ; p++)
	  if(*p == '\015' && *(p+1) == '\012'){
	      p += 2;
	      break;
	  }
    }
}


/*----------------------------------------------------------------------
    build a fresh REPLY_S from the given string (see pine_send for format)

Args:	s -- "X-Reply-UID" header value

Returns: filled in REPLY_S or NULL on parse error
 ----*/
REPLY_S *
build_reply_uid(char *s)
{
    char    *p, *prefix = NULL, *val, *seq, *mbox;
    int	     i, nseq, forwarded = 0;
    REPLY_S *reply = NULL;

    /* FORMAT: (n prefix)(n validity uidlist)mailbox */
    /* if 'n prefix' is empty, uid list represents forwarded msgs */
    if(*s == '('){
	if(*(p = s + 1) == ')'){
	    forwarded = 1;
	}
	else{
	    for(; isdigit(*p); p++)
	      ;

	    if(*p == ' '){
		*p++ = '\0';

		if((i = atoi(s+1)) && i < strlen(p)){
		    prefix = p;
		    *(p += i) = '\0';
		}
	    }
	    else
	      return(NULL);
	}

	if(*++p == '(' && *++p){
	    for(seq = p; isdigit(*p); p++)
	      ;

	    if(*p == ' '){
		*p++ = '\0';
		for(val = p; isdigit(*p); p++)
		  ;

		if(*p == ' '){
		    *p++ = '\0';

		    if((nseq = atoi(seq)) && isdigit(*(seq = p))
		       && (p = strchr(p, ')')) && *(mbox = ++p)){
			imapuid_t *uidl;

			uidl = (imapuid_t *) fs_get ((nseq+1)*sizeof(imapuid_t));
			for(i = 0; i < nseq; i++)
			  if((p = strchr(seq,',')) != NULL){
			      *p = '\0';
			      if((uidl[i]= strtoul(seq,NULL,10)) != 0)
				seq = ++p;
			      else
				break;
			  }
			  else if((p = strchr(seq, ')')) != NULL){
			      if((uidl[i] = strtoul(seq,NULL,10)) != 0)
				i++;

			      break;
			  }

			if(i == nseq){
			    reply = (REPLY_S *)fs_get(sizeof(REPLY_S));
			    memset(reply, 0, sizeof(REPLY_S));
			    reply->uid		     = 1;
			    reply->data.uid.validity = strtoul(val, NULL, 10);
			    if(forwarded)
			      reply->forwarded	     = 1;
			    else
			      reply->prefix	     = cpystr(prefix);

			    reply->mailbox	     = cpystr(mbox);
			    uidl[nseq]	     = 0;
			    reply->data.uid.msgs = uidl;
			}
			else
			  fs_give((void **) &uidl);
		    }
		}
	    }
	}
    }

    return(reply);
}


/*
 * pine_new_env - allocate a new METAENV and fill it in.
 */
METAENV *
pine_new_env(ENVELOPE *outgoing, char **fccp, char ***tobufpp, PINEFIELD *custom)
{
    int		        cnt, i, stdcnt;
    char	       *p;
    PINEFIELD	       *pfields, *pf, **sending_order;
    METAENV	       *header;

    header = (METAENV *) fs_get(sizeof(METAENV));

    /* how many fields are there? */
    for(cnt = 0; pf_template && pf_template[cnt].name; cnt++)
      ;

    stdcnt = cnt;

    for(pf = custom; pf; pf = pf->next)
      cnt++;	

    /* temporary PINEFIELD array */
    i = (cnt + 1) * sizeof(PINEFIELD);
    pfields = (PINEFIELD *)fs_get((size_t) i);
    memset(pfields, 0, (size_t) i);

    i             = (cnt + 1) * sizeof(PINEFIELD *);
    sending_order = (PINEFIELD **)fs_get((size_t) i);
    memset(sending_order, 0, (size_t) i);

    header->env           = outgoing;
    header->local         = pfields;
    header->custom        = custom;
    header->sending_order = sending_order;

#if	!(defined(DOS) || defined(OS2)) || defined(NOAUTH)
# define NN 4
#else
# define NN 3
#endif

    /* initialize pfield */
    pf = pfields;
    for(i=0; i < stdcnt; i++, pf++){

        pf->name        = cpystr(pf_template[i].name);
	if(i == N_SENDER && F_ON(F_USE_SENDER_NOT_X, ps_global))
	  /* slide string over so it is Sender instead of X-X-Sender */
	  for(p=pf->name; *(p+1); p++)
	    *p = *(p+4);

        pf->type        = pf_template[i].type;
	pf->canedit     = pf_template[i].canedit;
	pf->rcptto      = pf_template[i].rcptto;
	pf->writehdr    = pf_template[i].writehdr;
	pf->localcopy   = pf_template[i].localcopy;
        pf->extdata     = NULL; /* unused */
        pf->next        = pf + 1;

        switch(pf->type){
          case FreeText:
            switch(i){
              case N_AUTHRCVD:
		sending_order[0]	= pf;
                break;

              case N_NEWS:
		pf->text		= &outgoing->newsgroups;
		sending_order[1]	= pf;
                break;

              case N_DATE:
		pf->text		= (char **) &outgoing->date;
		sending_order[2]	= pf;
                break;

              case N_INREPLY:
		pf->text		= &outgoing->in_reply_to;
		sending_order[NN+9]	= pf;
                break;

              case N_MSGID:
		pf->text		= &outgoing->message_id;
		sending_order[NN+10]	= pf;
                break;

              case N_REF:			/* won't be used here */
		sending_order[NN+11]	= pf;
                break;

	      case N_PRIORITY:
		sending_order[NN+12]    = pf;
		break;

	      case N_USERAGENT:
		pf->text		= &pf->textbuf;
		pf->textbuf		= generate_user_agent();
		sending_order[NN+13]    = pf;
		break;

              case N_POSTERR:			/* won't be used here */
		sending_order[NN+14]	= pf;
                break;

              case N_RPLUID:			/* won't be used here */
		sending_order[NN+15]	= pf;
                break;

              case N_RPLMBOX:			/* won't be used here */
		sending_order[NN+16]	= pf;
                break;

              case N_SMTP:			/* won't be used here */
		sending_order[NN+17]	= pf;
                break;

              case N_NNTP:			/* won't be used here */
		sending_order[NN+18]	= pf;
                break;

              case N_CURPOS:			/* won't be used here */
		sending_order[NN+19]	= pf;
                break;

              case N_OURREPLYTO:		/* won't be used here */
		sending_order[NN+20]	= pf;
                break;

              case N_OURHDRS:			/* won't be used here */
		sending_order[NN+21]	= pf;
                break;

              default:
                q_status_message1(SM_ORDER,3,3,
		    "Internal error: 1)FreeText header %s", comatose(i));
                break;
            }

            break;

          case Attachment:
            break;

          case Address:
            switch(i){
	      case N_FROM:
		sending_order[3]	= pf;
		pf->addr		= &outgoing->from;
		break;

	      case N_TO:
		sending_order[NN+2]	= pf;
		pf->addr		= &outgoing->to;
		if(tobufpp)
		  (*tobufpp)		= &pf->scratch;

		break;

	      case N_CC:
		sending_order[NN+3]	= pf;
		pf->addr		= &outgoing->cc;
		break;

	      case N_BCC:
		sending_order[NN+4]	= pf;
		pf->addr		= &outgoing->bcc;
		break;

	      case N_REPLYTO:
		sending_order[NN+1]	= pf;
		pf->addr		= &outgoing->reply_to;
		break;

	      case N_LCC:		/* won't be used here */
		sending_order[NN+7]	= pf;
		break;

#if	!(defined(DOS) || defined(OS2)) || defined(NOAUTH)
              case N_SENDER:
		sending_order[4]	= pf;
		pf->addr		= &outgoing->sender;
                break;
#endif

              case N_NOBODY: /* won't be used here */
		sending_order[NN+5]	= pf;
                break;

              default:
                q_status_message1(SM_ORDER,3,3,
		    "Internal error: Address header %s", comatose(i));
                break;
            }
            break;

          case Fcc:
	    sending_order[NN+8] = pf;
	    pf->text		= fccp;
            break;

	  case Subject:
	    sending_order[NN+6]	= pf;
	    pf->text		= &outgoing->subject;
	    break;

          default:
            q_status_message1(SM_ORDER,3,3,
		"Unknown header type %d in pine_new_send", (void *)pf->type);
            break;
        }
    }

    if(((--pf)->next = custom) != NULL){
	i--;

	/* 
	 * NOTE: "i" is assumed to now index first custom field in sending
	 *       order.
	 */
	for(pf = pf->next; pf && pf->name; pf = pf->next){
	    if(pf->standard)
	      continue;

	    pf->canedit     = 1;
	    pf->rcptto      = 0;
	    pf->writehdr    = 1;
	    pf->localcopy   = 1;
	
	    switch(pf->type){
	      case Address:
		if(pf->addr){				/* better be set */
		    char    *addr = NULL;
		    BuildTo  bldto;

		    bldto.type    = Str;
		    bldto.arg.str = pf->textbuf;
		    
		    sending_order[i++] = pf;
		    /* change default text into an ADDRESS */
		    /* strip quotes around whole default */
		    removing_trailing_white_space(pf->textbuf);
		    (void)removing_double_quotes(pf->textbuf);
		    build_address_internal(bldto, &addr, NULL, NULL, NULL, NULL, NULL, 0, NULL);
		    rfc822_parse_adrlist(pf->addr, addr, ps_global->maildomain);
		    fs_give((void **)&addr);
		    if(pf->textbuf)
		      fs_give((void **)&pf->textbuf);
		}

		break;

	      case FreeText:
		sending_order[i++] = pf;
		pf->text           = &pf->textbuf;
		break;

	      default:
		q_status_message1(SM_ORDER,0,7,"Unknown custom header type %d",
				  (void *)pf->type);
		break;
	    }
	}
    }


    return(header);
}


void
pine_free_env(METAENV **menv)
{
    int cnt;


    if((*menv)->local){
	for(cnt = 0; pf_template && pf_template[cnt].name; cnt++)
	  ;

	for(; cnt >= 0; cnt--){
	    if((*menv)->local[cnt].textbuf)
	      fs_give((void **) &(*menv)->local[cnt].textbuf);

	    fs_give((void **) &(*menv)->local[cnt].name);
	}

	fs_give((void **) &(*menv)->local);
    }

    if((*menv)->sending_order)
      fs_give((void **) &(*menv)->sending_order);

    fs_give((void **) menv);
}


/*----------------------------------------------------------------------
   Check for addresses the user is not permitted to send to, or probably
   doesn't want to send to
   
Returns:  0 if OK
          1 if there are only empty groups
         -1 if the message shouldn't be sent

Queues a message indicating what happened
  ---*/
int
check_addresses(METAENV *header)
{
    PINEFIELD *pf;
    ADDRESS *a;
    int	     send_daemon = 0, rv = CA_EMPTY;

    /*---- Is he/she trying to send mail to the mailer-daemon ----*/
    for(pf = header->local; pf && pf->name; pf = pf->next)
      if(pf->type == Address && pf->rcptto && pf->addr && *pf->addr)
	for(a = *pf->addr; a != NULL; a = a->next){
	    if(a->host && (a->host[0] == '.'
			   || (F_ON(F_COMPOSE_REJECTS_UNQUAL, ps_global)
			       && a->host[0] == '@'))){
		q_status_message2(SM_ORDER, 4, 7,
				  /* TRANSLATORS: First arg is the address we can't
				     send to, second arg is "not in addressbook". */
				  _("Can't send to address %s: %s"),
				  a->mailbox,
				  (a->host[0] == '.')
				    ? a->host
				    : _("not in addressbook"));
		return(CA_BAD);
	    }
	    else if(ps_global->restricted
		    && !address_is_us(*pf->addr, ps_global)){
		q_status_message(SM_ORDER, 3, 3,
	"Restricted demo version of Alpine. You may only send mail to yourself");
		return(CA_BAD);
	    }
	    else if(a->mailbox && strucmp(a->mailbox, "mailer-daemon") == 0 && !send_daemon){
		send_daemon = 1;
		rv = (pith_opt_daemon_confirm && (*pith_opt_daemon_confirm)()) ? CA_OK : CA_BAD;
	    }
	    else if(a->mailbox && a->host){
		rv = CA_OK;
	    }
	}

    return(rv);
}


/*
 * If this isn't general enough we can modify it. The value passed in
 * is expected to be one of the desc settings from the priorities array,
 * like "High". The header value is X-Priority: 2 (High)
 * or something similar. If value doesn't match any of the values then
 * the actual value is used instead.
 */
PINEFIELD *
set_priority_header(METAENV *header, char *value)
{
    PINEFIELD *pf;

    for(pf = header->local; pf && pf->name; pf = pf->next)
      if(pf->type == FreeText && !strcmp(pf->name, PRIORITYNAME))
        break;

    if(pf){
	if(pf->textbuf)
	  fs_give((void **) &pf->textbuf);

	if(value){
	    PRIORITY_S *p;

	    for(p = priorities; p && p->desc; p++)
	      if(!strcmp(p->desc, value))
		break;

	    if(p && p->desc){
		char buf[100];

		snprintf(buf, sizeof(buf), "%d (%s)", p->val, p->desc);
		pf->textbuf = cpystr(buf);
	    }
	    else
	      pf->textbuf = cpystr(value);
	}
    }
}


/*----------------------------------------------------------------------
    Set answered flags for messages specified by reply structure
     
Args: reply -- 

Returns: with appropriate flags set and index cache entries suitably tweeked
----*/      
void
update_answered_flags(REPLY_S *reply)
{
    char       *seq = NULL, *p;
    long	i, ourstream = 0, we_cancel = 0;
    MAILSTREAM *stream = NULL;

    /* nothing to flip in a pseudo reply */
    if(reply && (reply->msgno || reply->uid)){
	int         j;
	MAILSTREAM *m;

	/*
	 * If an established stream will do, use it, else
	 * build one unless we have an array of msgno's...
	 *
	 * I was just mimicking what was already here. I don't really
	 * understand why we use strcmp instead of same_stream_and_mailbox().
	 * Or sp_stream_get(reply->mailbox, SP_MATCH).
	 * Hubert 2003-07-09
	 */
	for(j = 0; !stream && j < ps_global->s_pool.nstream; j++){
	    m = ps_global->s_pool.streams[j];
	    if(m && reply->mailbox && m->mailbox
	       && !strcmp(reply->mailbox, m->mailbox))
	      stream = m;
	}

	if(!stream && reply->msgno)
	  return;

	/*
	 * This is here only for people who ran pine4.42 and are
	 * processing postponed mail from 4.42 now. Pine4.42 saved the
	 * original mailbox name in the canonical name's position in
	 * the postponed-msgs folder so it won't match the canonical
	 * name from the stream.
	 */
	if(!stream && (!reply->origmbox ||
		       (reply->mailbox &&
		        !strcmp(reply->origmbox, reply->mailbox))))
	  stream = sp_stream_get(reply->mailbox, SP_MATCH);

	/* TRANSLATORS: program is busy updating the Answered flags so warns user */
	we_cancel = busy_cue(_("Updating \"Answered\" Flags"), NULL, 0);
	if(!stream){
	    if((stream = pine_mail_open(NULL,
				       reply->origmbox ? reply->origmbox
						       : reply->mailbox,
				       OP_SILENT | SP_USEPOOL | SP_TEMPUSE,
				       NULL)) != NULL){
		ourstream++;
	    }
	    else{
		if(we_cancel)
		  cancel_busy_cue(0);

		return;
	    }
	}

	if(stream->uid_validity == reply->data.uid.validity){
	    for(i = 0L, p = tmp_20k_buf; reply->data.uid.msgs[i]; i++){
		if(i){
		    sstrncpy(&p, ",", SIZEOF_20KBUF-(p-tmp_20k_buf));
		    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		}

		sstrncpy(&p, ulong2string(reply->data.uid.msgs[i]),
			 SIZEOF_20KBUF-(p-tmp_20k_buf));
		tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	    }

	    if(reply->forwarded){
		/*
		 * $Forwarded is a regular keyword so we only try to
		 * set it if the stream allows keywords.
		 * We could mess up if the stream has keywords but just
		 * isn't allowing anymore and $Forwarded already exists,
		 * but what are the odds?
		 */
		if(stream && stream->kwd_create)
		  mail_flag(stream, seq = cpystr(tmp_20k_buf),
			    FORWARDED_FLAG,
			    ST_SET | ((reply->uid) ? ST_UID : 0L));
	    }
	    else
	      mail_flag(stream, seq = cpystr(tmp_20k_buf),
		        "\\ANSWERED",
		        ST_SET | ((reply->uid) ? ST_UID : 0L));

	    if(seq)
	      fs_give((void **)&seq);
	}

	if(ourstream)
	  pine_mail_close(stream);	/* clean up dangling stream */

	if(we_cancel)
	  cancel_busy_cue(0);
    }
}


/*
 * phone_home_from - make phone home request's from address IMpersonal.
 *		     Doesn't include user's personal name.
 */
ADDRESS *
phone_home_from(void)
{
    ADDRESS *addr = mail_newaddr();
    char     tmp[32];

    /* garble up mailbox name */
    snprintf(tmp, sizeof(tmp), "hash_%08u", phone_home_hash(ps_global->VAR_USER_ID));
    tmp[sizeof(tmp)-1] = '\0';
    addr->mailbox = cpystr(tmp);
    addr->host	  = cpystr(ps_global->maildomain);
    return(addr);
}


/*
 * one-way-hash a username into an 8-digit decimal number 
 *
 * Corey Satten, corey@cac.washington.edu, 7/15/98
 */
unsigned int
phone_home_hash(char *s)
{
    unsigned int h;
    
    for (h=0; *s; ++s) {
        if (h & 1)
	  h = (h>>1) | (PH_MAXHASH/2);
        else 
	  h = (h>>1);

        h = ((h+1) * ((unsigned char) *s)) & (PH_MAXHASH - 1);
    }
    
    return (h);
}


/*----------------------------------------------------------------------
     Call the mailer, SMTP, sendmail or whatever
     
Args: header -- full header (envelope and local parts) of message to send
      body -- The full body of the message including text
      alt_smtp_servers --
      verbosefile -- non-null means caller wants verbose interaction and the resulting
                     output file name to be returned

Returns: -1 if failed, 1 if succeeded
----*/      
int
call_mailer(METAENV *header, struct mail_bodystruct *body, char **alt_smtp_servers,
	    int flags, void (*bigresult_f)(char *, int),
	    void (*pipecb_f)(PIPE_S *, int, void *))
{
    char         error_buf[200], *error_mess = NULL, *postcmd;
    ADDRESS     *a;
    ENVELOPE	*fake_env = NULL;
    int          addr_error_count, we_cancel = 0;
    long	 smtp_opts = 0L;
    char	*verbose_file = NULL;
    BODY	*bp = NULL;
    PINEFIELD	*pf;
    BODY	*origBody = body;

    dprint((4, "Sending mail...\n"));

    /* Check for any recipients */
    for(pf = header->local; pf && pf->name; pf = pf->next)
      if(pf->type == Address && pf->rcptto && pf->addr && *pf->addr)
	break;

    if(!pf){
	q_status_message(SM_ORDER,3,3,
	    _("Can't send message. No recipients specified!"));
	return(0);
    }

#ifdef SMIME
    if(ps_global->smime && (ps_global->smime->do_encrypt || ps_global->smime->do_sign)){
    	int result;
	
    	STORE_S *so = lmc.so;
	lmc.so = NULL;
    
    	result = 1;
    
    	if(ps_global->smime->do_encrypt)
    	  result = encrypt_outgoing_message(header, &body);
	
	/* need to free new body from encrypt if sign fails? */
	if(result && ps_global->smime->do_sign)
	  result = sign_outgoing_message(header, &body, ps_global->smime->do_encrypt);
	
	lmc.so = so;
	
	if(!result)
	  return 0;
    }
#endif

    /* set up counts and such to keep track sent percentage */
    send_bytes_sent = 0;
    gf_filter_init();				/* zero piped byte count, 'n */
    send_bytes_to_send = send_body_size(body);	/* count body bytes	     */
    ps_global->c_client_error[0] = error_buf[0] = '\0';
    we_cancel = busy_cue(_("Sending mail"),
			 send_bytes_to_send ? sent_percent : NULL, 0);

#ifndef	_WINDOWS

    /* try posting via local "<mta> <-t>" if specified */
    if(mta_handoff(header, body, error_buf, sizeof(error_buf), bigresult_f, pipecb_f)){
	if(error_buf[0])
	  error_mess = error_buf;

	goto done;
    }

#endif

    /*
     * If the user's asked for it, and we find that the first text
     * part (attachments all get b64'd) is non-7bit, ask for 8BITMIME.
     */
    if(F_ON(F_ENABLE_8BIT, ps_global) && (bp = first_text_8bit(body)))
       smtp_opts |= SOP_8BITMIME;

#ifdef	DEBUG
#ifndef DEBUGJOURNAL
    if(debug > 5 || (flags & CM_VERBOSE))
#endif
      smtp_opts |= SOP_DEBUG;
#endif

    if(flags & (CM_DSN_NEVER | CM_DSN_DELAY | CM_DSN_SUCCESS | CM_DSN_FULL)){
	smtp_opts |= SOP_DSN;
	if(!(flags & CM_DSN_NEVER)){ /* if never, don't turn others on */
	    if(flags & CM_DSN_DELAY)
	      smtp_opts |= SOP_DSN_NOTIFY_DELAY;
	    if(flags & CM_DSN_SUCCESS)
	      smtp_opts |= SOP_DSN_NOTIFY_SUCCESS;

	    /*
	     * If it isn't Never, then we're always going to let them
	     * know about failures.  This means we don't allow for the
	     * possibility of setting delay or success without failure.
	     */
	    smtp_opts |= SOP_DSN_NOTIFY_FAILURE;

	    if(flags & CM_DSN_FULL)
	      smtp_opts |= SOP_DSN_RETURN_FULL;
	}
    }


    /*
     * Set global header pointer so post_rfc822_output can get at it when
     * it's called back from c-client's sending routine...
     */
    send_header = header;

    /*
     * Fabricate a fake ENVELOPE to hand c-client's SMTP engine.
     * The purpose is to give smtp_mail the list for SMTP RCPT when
     * there are recipients in pine's METAENV that are outside c-client's
     * envelope.
     *  
     * NOTE: If there aren't any, don't bother.  Dealt with it below.
     */
    for(pf = header->local; pf && pf->name; pf = pf->next)
      if(pf->type == Address && pf->rcptto && pf->addr && *pf->addr
	 && !(*pf->addr == header->env->to || *pf->addr == header->env->cc
	      || *pf->addr == header->env->bcc))
	break;

    if(pf && pf->name){
	ADDRESS **tail;

	fake_env = (ENVELOPE *)fs_get(sizeof(ENVELOPE));
	memset(fake_env, 0, sizeof(ENVELOPE));
	fake_env->return_path = rfc822_cpy_adr(header->env->return_path);
	tail = &(fake_env->to);
	for(pf = header->local; pf && pf->name; pf = pf->next)
	  if(pf->type == Address && pf->rcptto && pf->addr && *pf->addr){
	      *tail = rfc822_cpy_adr(*pf->addr);
	      while(*tail)
		tail = &((*tail)->next);
	  }
    }

    /*
     * Install our rfc822 output routine 
     */
    sending_hooks.rfc822_out = mail_parameters(NULL, GET_RFC822OUTPUT, NULL);
    (void)mail_parameters(NULL, SET_RFC822OUTPUT, (void *)post_rfc822_output);

    /*
     * Allow for verbose posting
     */
    (void) mail_parameters(NULL, SET_SMTPVERBOSE,
			   (void *) pine_smtp_verbose_out);

    /*
     * We do this because we want mm_log to put the error message into
     * c_client_error instead of showing it itself.
     */
    ps_global->noshow_error = 1;

    /*
     * OK, who posts what?  We tried an mta_handoff above, but there
     * was either none specified or we decided not to use it.  So,
     * if there's an smtp-server defined anywhere, 
     */
    if(alt_smtp_servers && alt_smtp_servers[0] && alt_smtp_servers[0][0]){
	/*---------- SMTP ----------*/
	dprint((4, "call_mailer: via TCP (%s)\n",
		alt_smtp_servers[0]));
	TIME_STAMP("smtp-open start (tcp)", 1);
	sending_stream = smtp_open(alt_smtp_servers, smtp_opts);
    }
    else if(ps_global->VAR_SMTP_SERVER && ps_global->VAR_SMTP_SERVER[0]
	    && ps_global->VAR_SMTP_SERVER[0][0]){
	/*---------- SMTP ----------*/
	dprint((4, "call_mailer: via TCP\n"));
	TIME_STAMP("smtp-open start (tcp)", 1);
	sending_stream = smtp_open(ps_global->VAR_SMTP_SERVER, smtp_opts);
    }
    else if((postcmd = smtp_command(ps_global->c_client_error, sizeof(ps_global->c_client_error))) != NULL){
	char *cmdlist[2];

	/*----- Send via LOCAL SMTP agent ------*/
	dprint((4, "call_mailer: via \"%s\"\n", postcmd));

	TIME_STAMP("smtp-open start (pipe)", 1);
	fs_give((void **) &postcmd);
	cmdlist[0] = "localhost";
	cmdlist[1] = NULL;
	sending_stream = smtp_open_full(&piped_io, cmdlist, "smtp",
					SMTPTCPPORT, smtp_opts);
/* BUG: should provide separate stderr output! */
    }

    ps_global->noshow_error = 0;

    TIME_STAMP("smtp open", 1);
    if(sending_stream){
	unsigned short save_encoding, added_encoding;

	dprint((1, "Opened SMTP server \"%s\"\n",
	       net_host(sending_stream->netstream)
	         ? net_host(sending_stream->netstream) : "?"));

	if(flags & CM_VERBOSE){
	    TIME_STAMP("verbose start", 1);
	    if((verbose_file = temp_nam(NULL, "sd")) != NULL){
		if((verbose_send_output = our_fopen(verbose_file, "w")) != NULL){
		    if(!smtp_verbose(sending_stream)){
			snprintf(error_mess = error_buf, sizeof(error_buf),
			      "Mail not sent.  VERBOSE mode error%s%.50s.",
			      (sending_stream && sending_stream->reply)
			        ? ": ": "",
			      (sending_stream && sending_stream->reply)
			        ? sending_stream->reply : "");
			error_buf[sizeof(error_buf)-1] = '\0';
		    }
		}
		else{
		    our_unlink(verbose_file);
		    strncpy(error_mess = error_buf,
			    "Can't open tmp file for VERBOSE mode.", sizeof(error_buf));
		    error_buf[sizeof(error_buf)-1] = '\0';
		}
	    }
	    else{
		strncpy(error_mess = error_buf,
		        "Can't create tmp file name for VERBOSE mode.", sizeof(error_buf));
		error_buf[sizeof(error_buf)-1] = '\0';
	    }

	    TIME_STAMP("verbose end", 1);
	}

	/*
	 * Before we actually send data, see if we have to protect
	 * the first text body part from getting encoded.  We protect
	 * it from getting encoded in "pine_rfc822_output_body" by
	 * temporarily inventing a synonym for ENC8BIT...
	 * This works like so:
	 *   Suppose bp->encoding is set to ENC8BIT.
	 *   We change that here to some unused value (added_encoding) and
	 *    set body_encodings[added_encoding] to "8BIT".
	 *   Then post_rfc822_output is called which calls
	 *    pine_rfc822_output_body. Inside that routine
	 *    pine_write_body_header writes out the encoding for the
	 *    part. Normally it would see encoding == ENC8BIT and it would
	 *    change that to QUOTED-PRINTABLE, but since encoding has been
	 *    set to added_encoding it uses body_encodings[added_encoding]
	 *    which is "8BIT" instead. Then the actual body is written by
	 *    pine_write_body_header which does not do the gf_8bit_qp
	 *    filtering because encoding != ENC8BIT (instead it's equal
	 *    to added_encoding).
	 */
	if(bp && sending_stream->protocol.esmtp.eightbit.ok
	      && sending_stream->protocol.esmtp.eightbit.want){
	    int i;

	    for(i = 0; (i <= ENCMAX) && body_encodings[i]; i++)
	      ;

	    if(i > ENCMAX){		/* no empty encoding slots! */
		bp = NULL;
	    }
	    else {
		added_encoding = i;
		body_encodings[added_encoding] = body_encodings[ENC8BIT];
		save_encoding = bp->encoding;
		bp->encoding = added_encoding;
	    }
	}

	if(sending_stream->protocol.esmtp.ok
	   && sending_stream->protocol.esmtp.dsn.want
	   && !sending_stream->protocol.esmtp.dsn.ok)
	  q_status_message(SM_ORDER,3,3,
	    _("Delivery Status Notification not available from this server."));

	TIME_STAMP("smtp start", 1);
	if(!error_mess && !smtp_mail(sending_stream, "MAIL",
				     fake_env ? fake_env : header->env, body)){

	    snprintf(error_buf, sizeof(error_buf),
		    _("Mail not sent. Sending error%s%s"),
		    (sending_stream && sending_stream->reply) ? ": ": ".",
		    (sending_stream && sending_stream->reply)
		      ? sending_stream->reply : "");
	    error_buf[sizeof(error_buf)-1] = '\0';
	    dprint((1, error_buf));
	    addr_error_count = 0;
	    if(fake_env){
		for(a = fake_env->to; a != NULL; a = a->next)
		  if(a->error != NULL){
		      if(addr_error_count++ < MAX_ADDR_ERROR){

			  /*
			   * Too complicated to figure out which header line
			   * has the error in the fake_env case, so just
			   * leave cursor at default.
			   */


			  if(error_mess)	/* previous error? */
			    q_status_message(SM_ORDER, 4, 7, error_mess);

			  error_mess = tidy_smtp_mess(a->error,
						      _("Mail not sent: %.80s"),
						      error_buf, sizeof(error_buf));
		      }

		      dprint((1, "Send Error: \"%s\"\n",
				 a->error));
		  }
	    }
	    else{
	      for(pf = header->local; pf && pf->name; pf = pf->next)
	        if(pf->type == Address && pf->rcptto && pf->addr && *pf->addr)
		  for(a = *pf->addr; a != NULL; a = a->next)
		    if(a->error != NULL){
		      if(addr_error_count++ < MAX_ADDR_ERROR){

			  if(error_mess)	/* previous error? */
			    q_status_message(SM_ORDER, 4, 7, error_mess);

			  error_mess = tidy_smtp_mess(a->error,
						      _("Mail not sent: %.80s"),
						      error_buf, sizeof(error_buf));
		      }

		      dprint((1, "Send Error: \"%s\"\n",
				 a->error));
		    }
	    }

	    if(!error_mess)
	      error_mess = error_buf;
	}

	/* repair modified "body_encodings" array? */
	if(bp && sending_stream->protocol.esmtp.eightbit.ok
	      && sending_stream->protocol.esmtp.eightbit.want){
	    body_encodings[added_encoding] = NULL;
	    bp->encoding = save_encoding;
	}

	TIME_STAMP("smtp closing", 1);
	smtp_close(sending_stream);
	sending_stream = NULL;
	TIME_STAMP("smtp done", 1);
    }
    else if(!error_mess){
	snprintf(error_mess = error_buf, sizeof(error_buf), _("Error sending%.2s%.80s"),
	        ps_global->c_client_error[0] ? ": " : "",
		ps_global->c_client_error);
	error_buf[sizeof(error_buf)-1] = '\0';
    }

    if(verbose_file){
	if(verbose_send_output){
	    TIME_STAMP("verbose start", 1);
	    fclose(verbose_send_output);
	    verbose_send_output = NULL;
	    q_status_message(SM_ORDER, 0, 3, "Verbose SMTP output received");

	    if(bigresult_f)
	      (*bigresult_f)(verbose_file, CM_BR_VERBOSE);

	    TIME_STAMP("verbose end", 1);
	}

	fs_give((void **)&verbose_file);
    }

    /*
     * Restore original 822 emitter...
     */
    (void) mail_parameters(NULL, SET_RFC822OUTPUT, sending_hooks.rfc822_out);

    if(fake_env)
      mail_free_envelope(&fake_env);

  done:

#ifdef SMIME
    /* Free replacement encrypted body */
    if(F_OFF(F_DONT_DO_SMIME, ps_global) && body != origBody){
    
    	if(body->type == TYPEMULTIPART){
	    /* Just get rid of first part, it's actually origBody */
	    void *x = body->nested.part;
	    
	    body->nested.part = body->nested.part->next;
	    
	    fs_give(&x);
	}
    
    	pine_free_body(&body);
    }
#endif

    if(we_cancel)
      cancel_busy_cue(0);

    TIME_STAMP("call_mailer done", 1);
    /*-------- Did message make it ? ----------*/
    if(error_mess){
        /*---- Error sending mail -----*/
	if(lmc.so && !lmc.all_written)
	  so_give(&lmc.so);

	if(error_mess){
	    q_status_message(SM_ORDER | SM_DING, 4, 7, error_mess);
	    dprint((1, "call_mailer ERROR: %s\n", error_mess));
	}

	return(-1);
    }
    else{
	lmc.all_written = 1;
	return(1);
    }
}


/*
 * write_postponed - exported method to write the given message
 *		     to the postponed folder
 */
int
write_postponed(METAENV *header, struct mail_bodystruct *body)
{
    char      **pp, *folder;
    int		rv = 0, sz;
    CONTEXT_S  *fcc_cntxt = NULL;
    PINEFIELD *pf;
    static char *writem[] = {"To", "References", "Fcc", "X-Reply-UID", NULL};

    if(!ps_global->VAR_POSTPONED_FOLDER
       || !ps_global->VAR_POSTPONED_FOLDER[0]){
	q_status_message(SM_ORDER | SM_DING, 3, 3, 
			 _("No postponed file defined"));
	return(-1);
    }

    folder = cpystr(ps_global->VAR_POSTPONED_FOLDER);

    lmc.all_written = lmc.text_written = lmc.text_only = 0;

    lmc.so = open_fcc(folder, &fcc_cntxt, 1, NULL, NULL);

    if(lmc.so){
	/* BUG: writem sufficient ? */
	for(pf = header->local; pf && pf->name; pf = pf->next)
	  for(pp = writem; *pp; pp++)
	    if(!strucmp(pf->name, *pp)){
		pf->localcopy = 1;
		pf->writehdr = 1;
		break;
	    }

	/*
	 * Work around c-client reply-to bug. C-client will
	 * return a reply_to in an envelope even if there is
	 * no reply-to header field. We want to note here whether
	 * the reply-to is real or not.
	 */
	if(header->env->reply_to || hdr_is_in_list("reply-to", header->custom))
	  for(pf = header->local; pf; pf = pf->next)
	    if(!strcmp(pf->name, "Reply-To")){
		pf->writehdr = 1;
		pf->localcopy = 1;
		if(header->env->reply_to)
		  pf->textbuf = cpystr("Full");
		else
		  pf->textbuf = cpystr("Empty");
	    }

	/*
	 * Write the list of custom headers to the
	 * X-Our-Headers header so that we can recover the
	 * list in redraft.
	 */
	sz = 0;
	for(pf = header->custom; pf && pf->name; pf = pf->next)
	  sz += strlen(pf->name) + 1;

	if(sz){
	    int i;
	    char *pstart, *pend;

	    for(i = 0, pf = header->local; i != N_OURHDRS; i++, pf = pf->next)
	      ;

	    pf->writehdr  = 1;
	    pf->localcopy = 1;
	    pf->textbuf = pstart = pend = (char *) fs_get(sz + 1);
	    pf->text = &pf->textbuf;
	    pf->textbuf[sz] = '\0';			/* tie off overflow */
            /* note: "pf" overloaded */
	    for(pf = header->custom; pf && pf->name; pf = pf->next){
		int r = sz - (pend - pstart);		/* remaining buffer */

		if(r > 0 && r != sz){
		    r--;
		    *pend++ = ',';
		}

		sstrncpy(&pend, pf->name, r);
	    }
	}

	if(pine_rfc822_output(header, body, NULL, NULL) < 0
	   || write_fcc(folder, fcc_cntxt, lmc.so, NULL, "postponed message", NULL) < 0)
	  rv = -1;

	so_give(&lmc.so);
    }
    else {
	q_status_message1(SM_ORDER | SM_DING, 3, 3, 
			  "Can't allocate internal storage: %s ",
			  error_description(errno));
	rv = -1;
    }

    fs_give((void **) &folder);
    return(rv);
}


int
commence_fcc(char *fcc, CONTEXT_S **fcc_cntxt, int forced)
{
    if(fcc && *fcc){
	lmc.all_written = lmc.text_written = 0;
	lmc.text_only = F_ON(F_NO_FCC_ATTACH, ps_global) != 0;
	return((lmc.so = open_fcc(fcc, fcc_cntxt, 0, NULL, NULL)) != NULL);
    }
    else
      lmc.so = NULL;

    return(TRUE);
}


int
wrapup_fcc(char *fcc, CONTEXT_S *fcc_cntxt, METAENV *header, struct mail_bodystruct *body)
{
    int rv = TRUE;

    if(lmc.so){
	if(!header || pine_rfc822_output(header, body, NULL, NULL)){
	    char label[50];

	    strncpy(label, "Fcc", sizeof(label));
	    label[sizeof(label)-1] = '\0';
	    if(strcmp(fcc, ps_global->VAR_DEFAULT_FCC)){
		snprintf(label + 3, sizeof(label)-3, " to %.40s", fcc);
		label[sizeof(label)-1] = '\0';
	    }

	    rv = write_fcc(fcc,fcc_cntxt,lmc.so,NULL,NULL,NULL);
	}
	else{
	    rv = FALSE;
	}

	so_give(&lmc.so);
    }

    return(rv);
}


/*----------------------------------------------------------------------
    Checks to make sure the fcc is available and can be opened

Args: fcc -- the name of the fcc to create.  It can't be NULL.
      fcc_cntxt -- Returns the context the fcc is in.
      force -- supress user option prompt

Returns allocated storage object on success, NULL on failure
  ----*/
STORE_S *
open_fcc(char *fcc, CONTEXT_S **fcc_cntxt, int force, char *err_prefix, char *err_suffix)
{
    int		exists, ok = 0;

    ps_global->mm_log_error = 0;

    /* 
     * check for fcc's existance...
     */
    TIME_STAMP("open_fcc start", 1);
    if(!is_absolute_path(fcc) && context_isambig(fcc)
       && (strucmp(ps_global->inbox_name, fcc) != 0)){
	int flip_dot = 0;

	/*
	 * Don't want to preclude a user from Fcc'ing a .name'd folder
	 */
	if(F_OFF(F_ENABLE_DOT_FOLDERS, ps_global)){
	    flip_dot = 1;
	    F_TURN_ON(F_ENABLE_DOT_FOLDERS, ps_global);
	}

	/*
	 * We only want to set the "context" if fcc is an ambiguous
	 * name.  Otherwise, our "relativeness" rules for contexts 
	 * (implemented in context.c) might cause the name to be
	 * interpreted in the wrong context...
	 */
	if(!(*fcc_cntxt || (*fcc_cntxt = default_save_context(ps_global->context_list))))
	  *fcc_cntxt = ps_global->context_list;

        build_folder_list(NULL, *fcc_cntxt, fcc, NULL, BFL_FLDRONLY);
        if(folder_index(fcc, *fcc_cntxt, FI_FOLDER) < 0){
	    if(force
	       || (pith_opt_save_create_prompt
		   && (*pith_opt_save_create_prompt)(*fcc_cntxt, fcc, 0) > 0)){

		ps_global->noshow_error = 1;

		if(context_create(*fcc_cntxt, NULL, fcc))
		  ok++;

		ps_global->noshow_error = 0;
	    }
	    else
	      ok--;			/* declined! */
        }
	else
	  ok++;				/* found! */

	if(flip_dot)
	  F_TURN_OFF(F_ENABLE_DOT_FOLDERS, ps_global);

        free_folder_list(*fcc_cntxt);
    }
    else if((exists = folder_exists(NULL, fcc)) != FEX_ERROR){
	if(exists & (FEX_ISFILE | FEX_ISDIR)){
	    ok++;
	}
	else{
	    if(force
	       || (pith_opt_save_create_prompt
		   && (*pith_opt_save_create_prompt)(NULL, fcc, 0) > 0)){

		ps_global->mm_log_error = 0;
		ps_global->noshow_error = 1;

		ok = pine_mail_create(NULL, fcc) != 0L;

		ps_global->noshow_error = 0;
	    }
	    else
	      ok--;			/* declined! */
	}
    }

    TIME_STAMP("open_fcc done.", 1);
    if(ok > 0){
	return(so_get(FCC_SOURCE, NULL, WRITE_ACCESS));
    }
    else{
	int   l1, l2, l3, wid, w;
	char *errstr, tmp[MAILTMPLEN];
	char *s1, *s2;

	if(ok == 0){
	    if(ps_global->mm_log_error){
		s1 = err_prefix ? err_prefix : "Fcc Error: ";
		s2 = err_suffix ? err_suffix : " Message NOT sent or copied.";

		l1 = strlen(s1);
		l2 = strlen(s2);
		l3 = strlen(ps_global->c_client_error);
		wid = (ps_global->ttyo && ps_global->ttyo->screen_cols > 0)
		       ? ps_global->ttyo->screen_cols : 80;
		w = wid - l1 - l2 - 5;

		snprintf(errstr = tmp, sizeof(tmp),
			"%.99s\"%.*s%.99s\".%.99s",
			s1,
			(l3 > w) ? MAX(w-3,0) : MAX(w,0),
			ps_global->c_client_error,
			(l3 > w) ? "..." : "",
			s2);
		tmp[sizeof(tmp)-1] = '\0';

	    }
	    else
	      errstr = _("Fcc creation error. Message NOT sent or copied.");
	}
	else
	  errstr = _("Fcc creation rejected. Message NOT sent or copied.");

	q_status_message(SM_ORDER | SM_DING, 3, 3, errstr);
    }

    return(NULL);
}


/*----------------------------------------------------------------------
   mail_append() the fcc accumulated in temp_storage to proper destination

Args:  fcc -- name of folder
       fcc_cntxt -- context for folder
       temp_storage -- String of file where Fcc has been accumulated

This copies the string of file to the actual folder, which might be IMAP
or a disk folder.  The temp_storage is freed after it is written.
An error message is produced if this fails.
  ----*/
int
write_fcc(char *fcc, CONTEXT_S *fcc_cntxt, STORE_S *tmp_storage,
	  MAILSTREAM *stream, char *label, char *flags)
{
    STRING      msg;
    CONTEXT_S  *cntxt;
    int         we_cancel = 0;

    if(!tmp_storage)
      return(0);

    TIME_STAMP("write_fcc start.", 1);
    dprint((4, "Writing %s\n", (label && *label) ? label : ""));
    if(label && *label){
	char msg_buf[80];

	strncpy(msg_buf, "Writing ", sizeof(msg_buf));
	msg_buf[sizeof(msg_buf)-1] = '\0';
	strncat(msg_buf, label, sizeof(msg_buf)-10);
	we_cancel = busy_cue(msg_buf, NULL, 0);
    }
    else
      we_cancel = busy_cue(NULL, NULL, 1);

    so_seek(tmp_storage, 0L, 0);

/*
 * Before changing this note that these lines depend on the
 * definition of FCC_SOURCE.
 */
    INIT(&msg, mail_string, (void *)so_text(tmp_storage), 
	     strlen((char *)so_text(tmp_storage)));

    cntxt      = fcc_cntxt;

    if(!context_append_full(cntxt, stream, fcc, flags, NULL, &msg)){
	cancel_busy_cue(-1);
	we_cancel = 0;

	q_status_message1(SM_ORDER | SM_DING, 3, 5,
			  "Write to \"%s\" FAILED!!!", fcc);
	dprint((1, "ERROR appending %s in \"%s\"",
	       fcc ? fcc : "?",
	       (cntxt && cntxt->context) ? cntxt->context : "NULL"));
	return(0);
    }

    if(we_cancel)
      cancel_busy_cue(label ? 0 : -1);

    dprint((4, "done.\n"));
    TIME_STAMP("write_fcc done.", 1);
    return(1);
}


/*
 * first_text_8bit - return TRUE if somewhere in the body 8BIT data's
 *		     contained.
 */
BODY *
first_text_8bit(struct mail_bodystruct *body)
{
    if(body->type == TYPEMULTIPART)	/* advance to first contained part */
      body = &body->nested.part->body;

    return((body->type == TYPETEXT && body->encoding != ENC7BIT)
	     ? body : NULL);
}


/*
 * Build and return the "From:" address for outbound messages from
 * global data...
 */
ADDRESS *
generate_from(void)
{
    ADDRESS *addr = mail_newaddr();
    if(ps_global->VAR_PERSONAL_NAME){
	addr->personal = cpystr(ps_global->VAR_PERSONAL_NAME);
	removing_leading_and_trailing_white_space(addr->personal);
	if(addr->personal[0] == '\0')
	  fs_give((void **)&addr->personal);
    }

    addr->mailbox = cpystr(ps_global->VAR_USER_ID);
    addr->host    = cpystr(ps_global->maildomain);
    removing_leading_and_trailing_white_space(addr->mailbox);
    removing_leading_and_trailing_white_space(addr->host);
    return(addr);
}


/*
 * set_mime_type_by_grope - sniff the given storage object to determine its 
 *                  type, subtype, encoding, and charset
 *
 *		"Type" and "encoding" must be set before calling this routine.
 *		If "type" is set to something other than TYPEOTHER on entry,
 *		then that is the "type" we wish to use.  Same for "encoding"
 *		using ENCOTHER instead of TYPEOTHER.  Otherwise, we
 *		figure them out here.  If "type" is already set, we also
 *		leave subtype alone.  If not, we figure out subtype here.
 *		There is a chance that we will upgrade encoding to a "higher"
 *		level.  For example, if it comes in as 7BIT we may change
 *		that to 8BIT if we find a From_ we want to escape.
 *		We may also set the charset attribute if the type is TEXT.
 *
 * NOTE: this is rather inefficient if the store object is a CharStar
 *       but the win is all types are handled the same
 */
void
set_mime_type_by_grope(struct mail_bodystruct *body)
{
#define RBUFSZ	(8193)
    unsigned char   *buf, *p, *bol;
    register size_t  n;
    long             max_line = 0L,
                     eight_bit_chars = 0L,
                     line_so_far = 0L,
                     len = 0L;
    STORE_S         *so = (STORE_S *)body->contents.text.data;
    unsigned short   new_encoding = ENCOTHER;
    int              we_cancel = 0;
#ifdef ENCODE_FROMS
    short            froms = 0, dots = 0,
                     bmap  = 0x1, dmap = 0x1;
#endif

    we_cancel = busy_cue(NULL, NULL, 1);

    buf = (unsigned char *)fs_get(RBUFSZ);
    so_seek(so, 0L, 0);

    for(n = 0; n < RBUFSZ-1 && so_readc(&buf[n], so) != 0; n++)
      ;

    buf[n] = '\0';

    if(n){    /* check first few bytes to look for magic numbers */
	if(body->type == TYPEOTHER){
	    if(buf[0] == 'G' && buf[1] == 'I' && buf[2] == 'F'){
		body->type    = TYPEIMAGE;
		body->subtype = cpystr("GIF");
	    }
	    else if((n > 9) && buf[0] == 0xFF && buf[1] == 0xD8
		    && buf[2] == 0xFF && buf[3] == 0xE0
		    && !strncmp((char *)&buf[6], "JFIF", 4)){
	        body->type    = TYPEIMAGE;
	        body->subtype = cpystr("JPEG");
	    }
	    else if((buf[0] == 'M' && buf[1] == 'M')
		    || (buf[0] == 'I' && buf[1] == 'I')){
		body->type    = TYPEIMAGE;
		body->subtype = cpystr("TIFF");
	    }
	    else if((buf[0] == '%' && buf[1] == '!')
		     || (buf[0] == '\004' && buf[1] == '%' && buf[2] == '!')){
		body->type    = TYPEAPPLICATION;
		body->subtype = cpystr("PostScript");
	    }
	    else if(buf[0] == '%' && !strncmp((char *)buf+1, "PDF-", 4)){
		body->type = TYPEAPPLICATION;
		body->subtype = cpystr("PDF");
	    }
	    else if(buf[0] == '.' && !strncmp((char *)buf+1, "snd", 3)){
		body->type    = TYPEAUDIO;
		body->subtype = cpystr("Basic");
	    }
	    else if((n > 3) && buf[0] == 0x00 && buf[1] == 0x05
		            && buf[2] == 0x16 && buf[3] == 0x00){
	        body->type    = TYPEAPPLICATION;
		body->subtype = cpystr("APPLEFILE");
	    }
	    else if((n > 3) && buf[0] == 0x50 && buf[1] == 0x4b
		            && buf[2] == 0x03 && buf[3] == 0x04){
	        body->type    = TYPEAPPLICATION;
		body->subtype = cpystr("ZIP");
	    }

	    /*
	     * if type was set above, but no encoding specified, go
	     * ahead and make it BASE64...
	     */
	    if(body->type != TYPEOTHER && body->encoding == ENCOTHER)
	      body->encoding = ENCBINARY;
	}
    }
    else{
	/* PROBLEM !!! */
	if(body->type == TYPEOTHER){
	    body->type = TYPEAPPLICATION;
	    body->subtype = cpystr("octet-stream");
	    if(body->encoding == ENCOTHER)
		body->encoding = ENCBINARY;
	}
    }

    if (body->encoding == ENCOTHER || body->type == TYPEOTHER){
#if defined(DOS) || defined(OS2) /* for binary file detection */
	int lastchar = '\0';
#define BREAKOUT 300   /* a value that a character can't be */
#endif

	p   = bol = buf;
	len = n;
	while (n--){
/* Some people don't like quoted-printable caused by leading Froms */
#ifdef ENCODE_FROMS
	    Find_Froms(froms, dots, bmap, dmap, *p);
#endif
	    if(*p == '\n'){
		max_line    = MAX(max_line, line_so_far + p - bol);
		bol	    = NULL;		/* clear beginning of line */
		line_so_far = 0L;		/* clear line count */
#if	defined(DOS) || defined(OS2)
		/* LF with no CR!! */
		if(lastchar != '\r')		/* must be non-text data! */
		  lastchar = BREAKOUT;
#endif
	    }
	    else if(*p & 0x80){
		eight_bit_chars++;
	    }
	    else if(!*p){
		/* NULL found. Unless we're told otherwise, must be binary */
		if(body->type == TYPEOTHER){
		    body->type    = TYPEAPPLICATION;
		    body->subtype = cpystr("octet-stream");
		}

		/*
		 * The "TYPETEXT" here handles the case that the NULL
		 * comes from imported text generated by some external
		 * editor that permits or inserts NULLS.  Otherwise,
		 * assume it's a binary segment...
		 */
		new_encoding = (body->type==TYPETEXT) ? ENC8BIT : ENCBINARY;

		/*
		 * Since we've already set encoding, count this as a 
		 * hi bit char and continue.  The reason is that if this
		 * is text, there may be a high percentage of encoded 
		 * characters, so base64 may get set below...
		 */
		if(body->type == TYPETEXT)
		  eight_bit_chars++;
		else
		  break;
	    }

#if defined(DOS) || defined(OS2) /* for binary file detection */
	    if(lastchar != BREAKOUT)
	      lastchar = *p;
#endif

	    /* read another buffer in */
	    if(n == 0){
		if(bol)
		  line_so_far += p - bol;

		for (n = 0; n < RBUFSZ-1 && so_readc(&buf[n], so) != 0; n++)
		  ;

		len += n;
		p = buf;
	    }
	    else
	      p++;

	    /*
	     * If there's no beginning-of-line pointer, then we must
	     * have seen an end-of-line.  Set bol to the start of the
	     * new line...
	     */
	    if(!bol)
	      bol = p;

#if defined(DOS) || defined(OS2) /* for binary file detection */
	    /* either a lone \r or lone \n indicate binary file */
	    if(lastchar == '\r' || lastchar == BREAKOUT){
		if(lastchar == BREAKOUT || n == 0 || *p != '\n'){
		    if(body->type == TYPEOTHER){
			body->type    = TYPEAPPLICATION;
			body->subtype = cpystr("octet-stream");
		    }

		    new_encoding = ENCBINARY;
		    break;
		}
	    }
#endif
	}
    }

    /* stash away for later */
    so_attr(so, "maxline", long2string(max_line));

    if(body->encoding == ENCOTHER || body->type == TYPEOTHER){
	/*
	 * Since the type or encoding aren't set yet, fall thru a 
	 * series of tests to make sure an adequate type and 
	 * encoding are set...
	 */

	if(max_line >= 1000L){ 		/* 1000 comes from rfc821 */
	    if(body->type == TYPEOTHER){
		/*
		 * Since the types not set, then we didn't find a NULL.
		 * If there's no NULL, then this is likely text.  However,
		 * since we can't be *completely* sure, we set it to
		 * the generic type.
		 */
		body->type    = TYPEAPPLICATION;
		body->subtype = cpystr("octet-stream");
	    }

	    if(new_encoding != ENCBINARY)
	      /*
	       * As with NULL handling, if we're told it's text, 
	       * qp-encode it, else it gets base 64...
	       */
	      new_encoding = (body->type == TYPETEXT) ? ENC8BIT : ENCBINARY;
	}

	if(eight_bit_chars == 0L){
	    if(body->type == TYPEOTHER)
	      body->type = TYPETEXT;

	    if(new_encoding == ENCOTHER)
	      new_encoding = ENC7BIT;  /* short lines, no 8 bit */
	}
	else if(len <= 3000L || (eight_bit_chars * 100L)/len < 30L){
	    /*
	     * The 30% threshold is based on qp encoded readability
	     * on non-MIME UA's.
	     */
	    if(body->type == TYPEOTHER)
	      body->type = TYPETEXT;

	    if(new_encoding != ENCBINARY)
	      new_encoding = ENC8BIT;  /* short lines, < 30% 8 bit chars */
	}
	else{
	    if(body->type == TYPEOTHER){
		body->type    = TYPEAPPLICATION;
		body->subtype = cpystr("octet-stream");
	    }

	    /*
	     * Apply maximal encoding regardless of previous
	     * setting.  This segment's either not text, or is 
	     * unlikely to be readable with > 30% of the
	     * text encoded anyway, so we might as well save space...
	     */
	    new_encoding = ENCBINARY;   /*  > 30% 8 bit chars */
	}
    }

#ifdef ENCODE_FROMS
    /* If there were From_'s at the beginning of a line or standalone dots */
    if((froms || dots) && new_encoding != ENCBINARY)
      new_encoding = ENC8BIT;
#endif

    /* Set the subtype */
    if(body->subtype == NULL)
      body->subtype = cpystr(rfc822_default_subtype(body->type));

    if(body->encoding == ENCOTHER)
      body->encoding = new_encoding;

    fs_give((void **)&buf);

    if(we_cancel)
      cancel_busy_cue(-1);
}


/*
 * Call this to set the charset of an attachment we have
 * created. If the attachment contains any non-ascii characters
 * then we'll set the charset to the passed in charset, otherwise
 * we'll make it us-ascii.
 */
void
set_charset_possibly_to_ascii(struct mail_bodystruct *body, char *charset)
{
    unsigned char c;
    int           can_be_ascii = 1;
    STORE_S      *so = (STORE_S *)body->contents.text.data;
    int           we_cancel = 0;

    if(!body || body->type != TYPETEXT)
      return;

    we_cancel = busy_cue(NULL, NULL, 1);

    so_seek(so, 0L, 0);

    while(can_be_ascii && so_readc(&c, so))
      if(!c || c & 0x80)
	can_be_ascii--;

    if(can_be_ascii)
      set_parameter(&body->parameter, "charset", "US-ASCII");
    else if(charset && *charset && strucmp(charset, "US-ASCII"))
      set_parameter(&body->parameter, "charset", charset);
    else{
	/*
	 * Else we don't know. There are non ascii characters but we either
	 * don't have a charset to set it to or that charset is just us_ascii,
	 * which is impossible. So we label it unknown. An alternative would
	 * have been to strip the high bits instead and label it ascii.
	 */
      set_parameter(&body->parameter, "charset", UNKNOWN_CHARSET);
    }

    if(we_cancel)
      cancel_busy_cue(-1);
}


/*
 * since encoding happens on the way out the door, this is basically
 * just needed to handle TYPEMULTIPART
 */
void
pine_encode_body (struct mail_bodystruct *body)
{
  PART *part;

  dprint((4, "-- pine_encode_body: %d\n", body ? body->type : 0));
  if (body) switch (body->type) {
    char *freethis;

    case TYPEMULTIPART:		/* multi-part */
      if(!(freethis=parameter_val(body->parameter, "BOUNDARY"))){
	  char tmp[MAILTMPLEN];	/* make cookie not in BASE64 or QUOTEPRINT*/

	  snprintf (tmp,sizeof(tmp),"%ld-%ld-%ld=:%ld",gethostid (),random (),(long) time (0),
		    (long) getpid ());
	  tmp[sizeof(tmp)-1] = '\0';
	  set_parameter(&body->parameter, "BOUNDARY", tmp);
      }

      if(freethis)
        fs_give((void **) &freethis);

      part = body->nested.part;	/* encode body parts */
      do pine_encode_body (&part->body);
      while ((part = part->next) != NULL);	/* until done */
      break;

    case TYPETEXT :
	/*
	 * If the part is text we edited, then it is UTF-8.
	 * The user may be asking us to send it as something else
	 * or we may want to downconvert to a more-specific characterset.
	 * Mark it for conversion here so the right MIME header's written.
	 * Do conversion pine_rfc822_output_body.
	 * Attachments are left as is.
	 */
      if(body->contents.text.data
	 && so_attr((STORE_S *) body->contents.text.data, "edited", NULL)){
	  char *charset, *posting_charset, *lp;

	  if(!((charset = parameter_val(body->parameter, "charset"))
	        && !strucmp(charset, UNKNOWN_CHARSET))
	     && (posting_charset = posting_characterset(body, charset, MsgBody))){

	      set_parameter(&body->parameter, "charset", posting_charset);
		
	      /*
	       * Fix iso-2022-jp encoding to ENC7BIT since it's escape based
	       * and doesn't use anything but ASCII characters.
	       * Why is it not ENC7BIT already? Because when we set the encoding
	       * in set_mime_type_by_grope we were groping through UTF-8 text
	       * not 2022 text. Not only that, but we didn't know at that point
	       * that it wouldn't stay UTF-8 when we sent it, which would require
	       * encoding.
	       */
	      if(!strucmp(posting_charset, "iso-2022-jp")
		 && (lp = so_attr((STORE_S *) body->contents.text.data, "maxline", NULL))
		 && strlen(lp) < 4)
		body->encoding = ENC7BIT;
	  }

	  if(charset)
	    fs_give((void **)&charset);
      }	  

      break;

/* case MESSAGE:	*/	/* here for documentation */
      /* Encapsulated messages are always treated as text objects at this point.
	 This means that you must replace body->contents.msg with
	 body->contents.text, which probably involves copying
	 body->contents.msg.text to body->contents.text */
    default:			/* all else has some encoding */
      /*
       * but we'll delay encoding it until the message is on the way
       * into the mail slot...
       */
      break;
  }
}


/*
 * pine_header_line - simple wrapper around c-client call to contain
 *                    repeated code, and to write fcc if required.
 */
int
pine_header_line(char *field, METAENV *header, char *text, soutr_t f, void *s,
		 int writehdr, int localcopy)
{
    int   ret = 1;
    int   big = 10000;
    char *value, *folded = NULL, *cs;
    char *converted;

    if(!text)
      return 1;

    converted = utf8_to_charset(text, cs = posting_characterset(text, NULL, HdrText), 0);

    if(converted){
	if(cs && !strucmp(cs, "us-ascii"))
	  value = converted;
	else
	  value = encode_header_value(tmp_20k_buf, SIZEOF_20KBUF,
				      (unsigned char *) converted, cs,
				      encode_whole_header(field, header));

	if(value && value == converted){	/* no encoding was done, have to fold */
	    int   fold_by, len;
	    char *actual_field;

	    len = ((header && header->env && header->env->remail)
			? strlen("ReSent-") : 0) +
		  (field ? strlen(field) : 0) + 2;
		  
	    actual_field = (char *)fs_get((len+1) * sizeof(char));
	    snprintf(actual_field, len+1, "%s%s: ",
		    (header && header->env && header->env->remail) ? "ReSent-" : "",
		    field ? field : "");
	    actual_field[len] = '\0';

	    /*
	     * We were folding everything except message-id, but that wasn't
	     * sufficient. Since 822 only allows folding where linear-white-space
	     * is allowed we'd need a smarter folder than "fold" to do it. So,
	     * instead of inventing that smarter folder (which would have to
	     * know 822 syntax)
	     *
	     * We could just alloc space and copy the actual_field followed by
	     * the value into it, but since that's what fold does anyway we'll
	     * waste some cpu time and use fold with a big fold parameter.
	     *
	     * We upped the references folding from 75 to 256 because we were
	     * encountering longer-than-75 message ids, and to break one line
	     * in references is to break them all.
	     */
	    if(field && !strucmp("Subject", field))
	      fold_by = 75;
	    else if(field && !strucmp("References", field))
	      fold_by = 256;
	    else
	      fold_by = big;

	    folded = fold(value, fold_by, big, actual_field, " ", FLD_CRLF);

	    if(actual_field)
	      fs_give((void **)&actual_field);
	}
	else if(value){			/* encoding was done */
	    RFC822BUFFER rbuf;
	    size_t ll;

	    /*
	     * rfc1522_encode already inserted continuation lines and did
	     * the necessary folding so we don't have to do it. Let
	     * rfc822_header_line add the trailing crlf and the resent- if
	     * necessary. The 20 could actually be a 12.
	     */
	    ll = strlen(field) + strlen(value) + 20;
	    folded = (char *) fs_get(ll * sizeof(char));
	    *folded = '\0';
	    rbuf.f   = dummy_soutr;
	    rbuf.s   = NULL;
	    rbuf.beg = folded;
	    rbuf.cur = folded;
	    rbuf.end = folded+ll-1;
	    rfc822_output_header_line(&rbuf, field,
		(header && header->env && header->env->remail) ? LONGT : 0L, value);
	    *rbuf.cur = '\0';
	}

	if(value && folded){
	    if(writehdr && f)
	      ret = (*f)(s, folded);
	    
	    if(ret && localcopy && lmc.so && !lmc.all_written)
	      ret = so_puts(lmc.so, folded);
	}

	if(folded)
	  fs_give((void **)&folded);

	if(converted && converted != text)
	  fs_give((void **) &converted);
    }
    else
      ret = 0;
    
    return(ret);
}


/*
 * Do appropriate encoding of text header lines.
 * For some field types (those that consist of 822 *text) we just encode
 * the whole thing. For structured fields we encode only within comments
 * if possible.
 *
 * Args      d -- Destination buffer if needed. (tmp_20k_buf)
 *           s -- Source string.
 *     charset -- Charset to encode with.
 *  encode_all -- If set, encode the whole string. If not, try to encode
 *                only within comments if possible.
 *
 * Returns   S is returned if no encoding is done. D is returned if encoding
 *           was needed.
 */
char *
encode_header_value(char *d, size_t dlen, unsigned char *s, char *charset, int encode_all)
{
    char *p, *q, *r, *start_of_comment = NULL, *value = NULL;
    int   in_comment = 0;

    if(!s)
      return((char *)s);
    
    if(dlen < SIZEOF_20KBUF)
      panic("bad call to encode_header_value");

    if(!encode_all){
	/*
	 * We don't have to worry about keeping track of quoted-strings because
	 * none of these fields which aren't addresses contain quoted-strings.
	 * We do keep track of escaped parens inside of comments and comment
	 * nesting.
	 */
	p = d+7000;
	for(q = (char *)s; *q; q++){
	    switch(*q){
	      case LPAREN:
		if(in_comment++ == 0)
		  start_of_comment = q;

		break;

	      case RPAREN:
		if(--in_comment == 0){
		    /* encode the comment, excluding the outer parens */
		    if(p-d < dlen-1)
		      *p++ = LPAREN;

		    *q = '\0';
		    r = rfc1522_encode(d+14000, dlen-14000,
				       (unsigned char *)start_of_comment+1,
				       charset);
		    if(r != start_of_comment+1)
		      value = d+7000;  /* some encoding was done */

		    start_of_comment = NULL;
		    if(r)
		      sstrncpy(&p, r, dlen-1-(p-d));

		    *q = RPAREN;
		    if(p-d < dlen-1)
		      *p++ = *q;
		}
		else if(in_comment < 0){
		    in_comment = 0;
		    if(p-d < dlen-1)
		      *p++ = *q;
		}

		break;

	      case BSLASH:
		if(!in_comment && *(q+1)){
		    if(p-d < dlen-2){
			*p++ = *q++;
			*p++ = *q;
		    }
		}

		break;

	      default:
		if(!in_comment && p-d < dlen-1)
		  *p++ = *q;

		break;
	    }
	}

	if(value){
	    /* Unterminated comment (wasn't really a comment) */
	    if(start_of_comment)
	      sstrncpy(&p, start_of_comment, dlen-1-(p-d));
	    
	    *p = '\0';
	}
    }

    /*
     * We have to check if there is anything that needs to be encoded that
     * wasn't in a comment. If there is, we'd better just start over and
     * encode the whole thing. So, if no encoding has been done within
     * comments, or if encoding is needed both within and outside of
     * comments, then we encode the whole thing. Otherwise, go with
     * the version that has only comments encoded.
     */
    if(!value || rfc1522_encode(d, dlen,
				(unsigned char *)value, charset) != value)
      return(rfc1522_encode(d, dlen, s, charset));
    else{
	strncpy(d, value, dlen-1);
	d[dlen-1] = '\0';
	return(d);
    }
}


/*
 * pine_address_line - write a header field containing addresses,
 *                     one by one (so there's no buffer limit), and
 *                     wrapping where necessary.
 * Note: we use c-client functions to properly build the text string,
 *       but have to screw around with pointers to fool c-client functions
 *       into not blatting all the text into a single buffer.  Yeah, I know.
 */
int
pine_address_line(char *field, METAENV *header, struct mail_address *alist,
		  soutr_t f, void *s, int writehdr, int localcopy)
{
    char     tmp[MAX_SINGLE_ADDR], *tmpptr = NULL;
    size_t   alloced = 0, sz;
    char    *delim, *ptmp, *mtmp, buftmp[MAILTMPLEN];
    char    *converted, *cs;
    ADDRESS *atmp;
    int      i, count;
    int      in_group = 0, was_start_of_group = 0, fix_lcc = 0, failed = 0;
    RFC822BUFFER rbuf;
    static   char comma[]     = ", ";
    static   char end_group[] = ";";
#define	no_comma	(&comma[1])

    if(!alist)				/* nothing in field! */
      return(1);

    if(!alist->host && alist->mailbox){ /* c-client group convention */
        in_group++;
	was_start_of_group++;
	/* encode mailbox of group */
	mtmp	    = alist->mailbox;
	if(mtmp){
	    snprintf(buftmp, sizeof(buftmp), "%s", mtmp);
	    buftmp[sizeof(buftmp)-1] = '\0';
	    converted = utf8_to_charset(buftmp, cs = posting_characterset(buftmp, NULL, HdrText), 0);
	    if(converted){
		alist->mailbox = cpystr(rfc1522_encode(tmp_20k_buf, SIZEOF_20KBUF,
							(unsigned char *) converted, cs));
		if(converted && converted != buftmp)
		  fs_give((void **) &converted);
	    }
	    else{
		failed++;
		goto bail_out;
	    }
	}
    }
    else
      mtmp = NULL;

    ptmp	    = alist->personal;	/* remember personal name */
    /* make sure personal name is encoded */
    if(ptmp){
	snprintf(buftmp, sizeof(buftmp), "%s", ptmp);
	buftmp[sizeof(buftmp)-1] = '\0';
	converted = utf8_to_charset(buftmp, cs = posting_characterset(buftmp, NULL, HdrText), 0);
	if(converted){
	    alist->personal = cpystr(rfc1522_encode(tmp_20k_buf, SIZEOF_20KBUF,
						    (unsigned char *) converted, cs));
	    if(converted && converted != buftmp)
	      fs_give((void **) &converted);
	}
	else{
	    failed++;
	    goto bail_out;
	}
    }

    atmp            = alist->next;
    alist->next	    = NULL;		/* digest only first address! */

    /* use automatic buffer unless it isn't big enough */
    if((alloced = est_size(alist)) > sizeof(tmp)){
      tmpptr = (char *)fs_get(alloced);
      sz = alloced;
    }
    else{
      tmpptr = tmp;
      sz = sizeof(tmp);
    }

    rbuf.f   = dummy_soutr;
    rbuf.s   = NULL;
    rbuf.beg = tmpptr;
    rbuf.cur = tmpptr;
    rbuf.end = tmpptr+sz-1;
    rfc822_output_address_line(&rbuf, field,
	(header && header->env && header->env->remail) ? LONGT : 0L, alist, NULL);
    *rbuf.cur = '\0';

    alist->next	    = atmp;		/* restore pointer to next addr */

    if(alist->personal && alist->personal != ptmp)
      fs_give((void **) &alist->personal);

    alist->personal = ptmp;		/* in case it changed, restore name */

    if(mtmp){
	if(alist->mailbox && alist->mailbox != mtmp)
	  fs_give((void **) &alist->mailbox);

	alist->mailbox = mtmp;
    }

    if((count = strlen(tmpptr)) > 2){	/* back over CRLF */
	count -= 2;
	tmpptr[count] = '\0';
    }

    /*
     * If there is no sending_stream and we are writing the Lcc header,
     * then we are piping it to sendmail -t which expects it to be a bcc,
     * not lcc.
     *
     * When we write it to the fcc or postponed (the lmc.so),
     * we want it to be lcc, not bcc, so we put it back.
     */
    if(!sending_stream && writehdr && struncmp("lcc:", tmpptr, 4) == 0)
      fix_lcc = 1;

    if(writehdr && f && *tmpptr){
	if(fix_lcc)
	  tmpptr[0] = 'b';
	
	failed = !(*f)(s, tmpptr);
	if(fix_lcc)
	  tmpptr[0] = 'L';

	if(failed)
	  goto bail_out;
    }

    if(localcopy && lmc.so &&
       !lmc.all_written && *tmpptr && !so_puts(lmc.so, tmpptr))
      goto bail_out;

    for(alist = atmp; alist; alist = alist->next){
	delim = comma;
	/* account for c-client's representation of group names */
	if(in_group){
	    if(!alist->host){  /* end of group */
		in_group = 0;
		was_start_of_group = 0;
		/*
		 * Rfc822_write_address no longer writes out the end of group
		 * unless the whole group address is passed to it, so we do
		 * it ourselves.
		 */
		delim = end_group;
	    }
	    else if(!localcopy || !lmc.so || lmc.all_written)
	      continue;
	}
	/* start of new group, print phrase below */
        else if(!alist->host && alist->mailbox){
	    in_group++;
	    was_start_of_group++;
	}

	/* no comma before first address in group syntax */
	if(was_start_of_group && alist->host){
	    delim = no_comma;
	    was_start_of_group = 0;
	}

	/* write delimiter */
	if(((!in_group||was_start_of_group) && writehdr && f && !(*f)(s, delim))
	   || (localcopy && lmc.so && !lmc.all_written
	       && !so_puts(lmc.so, delim)))
	  goto bail_out;

	ptmp		= alist->personal; /* remember personal name */
	snprintf(buftmp, sizeof(buftmp), "%.200s", ptmp ? ptmp : "");
	buftmp[sizeof(buftmp)-1] = '\0';
	converted = utf8_to_charset(buftmp, cs = posting_characterset(buftmp, NULL, HdrText), 0);
	if(converted){
	    alist->personal = cpystr(rfc1522_encode(tmp_20k_buf, SIZEOF_20KBUF,
						    (unsigned char *) converted, cs));
	    if(converted && converted != buftmp)
	      fs_give((void **) &converted);
	}
	else{
	    failed++;
	    goto bail_out;
	}

	atmp		= alist->next;
	alist->next	= NULL;		/* tie off linked list */
	if((i = est_size(alist)) > MAX(sizeof(tmp), alloced)){
	    alloced = i;
	    sz = alloced;
	    fs_resize((void **)&tmpptr, alloced);
	}

	*tmpptr  = '\0';
	/* make sure we don't write out group end with rfc822_write_address */
        if(alist->host || alist->mailbox){
	    rbuf.f   = dummy_soutr;
	    rbuf.s   = NULL;
	    rbuf.beg = tmpptr;
	    rbuf.cur = tmpptr;
	    rbuf.end = tmpptr+sz-1;
	    rfc822_output_address_list(&rbuf, alist, 0L, NULL);
	    *rbuf.cur = '\0';
	}

	alist->next	= atmp;		/* restore next pointer */

	if(alist->personal && alist->personal != ptmp)
	  fs_give((void **) &alist->personal);

	alist->personal = ptmp;		/* in case it changed, restore name */

	/*
	 * BUG
	 * With group syntax addresses we no longer have two identical
	 * streams of output.  Instead, for the fcc/postpone copy we include
	 * all of the addresses inside the :; of the group, and for the
	 * mail we're sending we don't include them.  That means we aren't
	 * correctly keeping track of the column to wrap in, below.  That is,
	 * we are keeping track of the fcc copy but we aren't keeping track
	 * of the regular copy.  It could result in too long or too short
	 * lines.  Should almost never come up since group addresses are almost
	 * never followed by other addresses in the same header, and even
	 * when they are, you have to go out of your way to get the headers
	 * messed up.
	 */
	if(count + 2 + (i = strlen(tmpptr)) > 78){ /* wrap long lines... */
	    count = i + 4;
	    if((!in_group && writehdr && f && !(*f)(s, "\015\012    "))
	       || (localcopy && lmc.so && !lmc.all_written &&
				       !so_puts(lmc.so, "\015\012    ")))
	      goto bail_out;
	}
	else
	  count += i + 2;

	if(((!in_group || was_start_of_group)
	    && writehdr && *tmpptr && f && !(*f)(s, tmpptr))
	   || (localcopy && lmc.so && !lmc.all_written
	       && *tmpptr && !so_puts(lmc.so, tmpptr)))
	  goto bail_out;
    }

bail_out:
    if(tmpptr && tmpptr != tmp)
      fs_give((void **)&tmpptr);

    if(failed)
      return(0);

    return((writehdr && f ? (*f)(s, "\015\012") : 1)
	   && ((localcopy && lmc.so
	       && !lmc.all_written) ? so_puts(lmc.so, "\015\012") : 1));
}


/*
 * mutated pine version of c-client's rfc822_header() function. 
 * changed to call pine-wrapped header and address functions
 * so we don't have to limit the header size to a fixed buffer.
 * This function also calls pine's body_header write function
 * because encoding is delayed until output_body() is called.
 */
long
pine_rfc822_header(METAENV *header, struct mail_bodystruct *body, soutr_t f, void *s)
{
    PINEFIELD  *pf;
    int         j;

    if(header->env->remail){			/* if remailing */
	long i = strlen (header->env->remail);
	if(i > 4 && header->env->remail[i-4] == '\015')
	  header->env->remail[i-2] = '\0'; /* flush extra blank line */

	if((f && !(*f)(s, header->env->remail)) 
	  || (lmc.so && !lmc.all_written
	      && !so_puts(lmc.so, header->env->remail)))
	  return(0L);				/* start with remail header */
    }

    j = 0;
    for(pf = header->sending_order[j]; pf; pf = header->sending_order[++j]){
	switch(pf->type){
	  /*
	   * Warning:  This is confusing.  The 2nd to last argument used to
	   * be just pf->writehdr.  We want Bcc lines to be written out
	   * if we are handing off to a sendmail temp file but not if we
	   * are talking smtp, so bcc's writehdr is set to 0 and
	   * pine_address_line was sending if writehdr OR !sending_stream.
	   * That works as long as we want to write everything when
	   * !sending_stream (an mta handoff to sendmail).  But then we
	   * added the undisclosed recipients line which should only get
	   * written if writehdr is set, and not when we pass to a
	   * sendmail temp file.  So pine_address_line has been changed
	   * so it bases its decision solely on the writehdr passed to it,
	   * and the logic that worries about Bcc and sending_stream
	   * was moved up to the caller (here) to decide when to set it.
	   *
	   * So we have:
	   *   undisclosed recipients:;  This will just be written
	   *                             if writehdr was set and not
	   *                             otherwise, nothing magical.
	   *** We may want to change this, because sendmail -t doesn't handle
	   *** the empty group syntax well unless it has been configured to
	   *** do so.  It isn't configured by default, or in any of the
	   *** sendmail v8 configs.  So we may want to not write this line
	   *** if we're doing an mta_handoff (!sending_stream).
	   *
	   *   !sending_stream (which means a handoff to a sendmail -t)
	   *           bcc or lcc both set the arg so they'll get written
	   *             (There is also Lcc hocus pocus in pine_address_line
	   *              which converts the Lcc: to Bcc: for sendmail
	   *              processing.)
	   *   sending_stream (which means an smtp handoff)
	   *           bcc and lcc will never have writehdr set, so
	   *             will never be written (They both do have rcptto set,
	   *             so they both do cause RCPT TO commands.)
	   *
	   *   The localcopy is independent of sending_stream and is just
	   *   written if it is set for all of these.
	   */
	  case Address:
	    if(!pine_address_line(pf->name,
				  header,
				  pf->addr ? *pf->addr : NULL,
				  f,
				  s,
				  (!strucmp("bcc",pf->name ? pf->name : "")
				    || !strucmp("Lcc",pf->name ? pf->name : ""))
					   ? !sending_stream
					   : pf->writehdr,
				  pf->localcopy))
	      return(0L);

	    break;

	  case Fcc:
	  case FreeText:
	  case Subject:
	    if(!pine_header_line(pf->name, header,
				 pf->text ? *pf->text : NULL,
				 f, s, pf->writehdr, pf->localcopy))
	      return(0L);

	    break;

	  default:
	    q_status_message1(SM_ORDER,3,7,"Unknown header type: %.200s",
			      pf->name);
	    break;
	}
    }


#if	(defined(DOS) || defined(OS2)) && !defined(NOAUTH)
    /*
     * Add comforting "X-" header line indicating what sort of 
     * authenticity the receiver can expect...
     */
    if(F_OFF(F_DISABLE_SENDER, ps_global)){
	 NETMBX	     netmbox;
	 char	     sstring[MAILTMPLEN], *label;	/* place to write  */
	 MAILSTREAM *m;
	 int         i, anonymous = 1;

	 for(i = 0; anonymous && i < ps_global->s_pool.nstream; i++){
	     m = ps_global->s_pool.streams[i];
	     if(m && sp_flagged(m, SP_LOCKED)
		&& mail_valid_net_parse(m->mailbox, &netmbox)
		&& !netmbox.anoflag)
	       anonymous = 0;
	 }

	 if(!anonymous){
	     char  last_char = netmbox.host[strlen(netmbox.host) - 1],
		  *user = (*netmbox.user)
			    ? netmbox.user
			    : cached_user_name(netmbox.mailbox);
	     snprintf(sstring, sizeof(sstring), "%.300s@%s%.300s%s", user ? user : "NULL", 
		     isdigit((unsigned char)last_char) ? "[" : "",
		     netmbox.host,
		     isdigit((unsigned char) last_char) ? "]" : "");
	     sstring[sizeof(sstring)-1] = '\0';
	     label = "X-X-Sender";		/* Jeez. */
	     if(F_ON(F_USE_SENDER_NOT_X,ps_global))
	       label += 4;
	 }
	 else{
	     strncpy(sstring,"UNAuthenticated Sender", sizeof(sstring));
	     sstring[sizeof(sstring)-1] = '\0';
	     label = "X-Warning";
	 }

	 if(!pine_header_line(label, header, sstring, f, s, 1, 1))
	   return(0L);
     }
#endif

    if(body && !header->env->remail){	/* not if remail or no body */
	if((f && !(*f)(s, MIME_VER))
	   || (lmc.so && !lmc.all_written && !so_puts(lmc.so, MIME_VER))
	   || !pine_write_body_header(body, f, s))
	  return(0L);
    }
    else{					/* write terminating newline */
	if((f && !(*f)(s, "\015\012"))
	   || (lmc.so && !lmc.all_written && !so_puts(lmc.so, "\015\012")))
	  return(0L);
    }

    return(1L);
}


/*
 * pine_rfc822_output - pine's version of c-client call.  Necessary here
 *			since we're not using its structures as intended!
 */
long
pine_rfc822_output(METAENV *header, struct mail_bodystruct *body, soutr_t f, void *s)
{
    int we_cancel = 0;
    long retval;

    dprint((4, "-- pine_rfc822_output\n"));

    we_cancel = busy_cue(NULL, NULL, 1);
    pine_encode_body(body);		/* encode body as necessary */
    /* build and output RFC822 header, output body */
    retval = pine_rfc822_header(header, body, f, s)
	   && (body ? pine_rfc822_output_body(body, f, s) : 1L);

    if(we_cancel)
      cancel_busy_cue(-1);

    return(retval);
}


/*
 * post_rfc822_output - cloak for pine's 822 output routine.  Since
 *			we can't pass opaque envelope thru c-client posting
 *			logic, we need to wrap the real output inside
 *			something that c-client knows how to call.
 */
long
post_rfc822_output(char *tmp,
		   ENVELOPE *env,
		   struct mail_bodystruct *body,
		   soutr_t f,
		   void *s,
		   long int ok8bit)
{
    return(pine_rfc822_output(send_header, body, f, s));
}


/*
 * posting_characterset- determine what transliteration is reasonable
 *                       for posting the given non-ascii messsage data.
 *
 *       preferred_charset is the charset the original data was labeled in.
 *                         If we can keep that we do.
 *
 *  Returns: always returns the preferred character set.
 */
char *
posting_characterset(void *data, char *preferred_charset, MsgPart mp)
{
    unsigned long *charsetmap = NULL;
    unsigned long validbitmap;
    static char *ascii = "US-ASCII";
    static char *utf8 = "UTF-8";
    int notcjk = 0;

    if(!ps_global->post_utf8){
	validbitmap = 0;

	if(mp == HdrText){
	    char *text = NULL;
	    UCS *ucs = NULL, *ucsp;

	    text = (char *) data;

	    /* convert text in header to UCS characters */
	    if(text)
	      ucsp = ucs = utf8_to_ucs4_cpystr(text);

	    if(!(ucs && *ucs))
	      return(ascii);

	    /*
	     * After the while loop is done the validbitmap has
	     * a 1 bit for all the character sets that can
	     * represent all of the characters of this header.
	     */
	    charsetmap = init_charsetchecker(preferred_charset);

	    if(!charsetmap)
	      return(utf8);

	    validbitmap = ~0;
	    while((validbitmap & ~0x1) && (*ucsp)){
		if(*ucsp > 0xffff){
		    fs_give((void **) &ucs);
		    return(utf8);
		}

		validbitmap &= charsetmap[(unsigned long) (*ucsp++)];
	    }

	    fs_give((void **) &ucs);

	    notcjk = validbitmap & 0x1;
	    validbitmap &= ~0x1;

	    if(!validbitmap)
	      return(utf8);
	}
	else{
	    struct mail_bodystruct *body = NULL;
	    STORE_S *the_text = NULL;
	    int outchars;
	    unsigned char c;
	    UCS ucs;
	    CBUF_S cbuf;

	    cbuf.cbuf[0] = '\0';
	    cbuf.cbufp = cbuf.cbuf;
	    cbuf.cbufend = cbuf.cbuf;

	    body = (struct mail_bodystruct *) data;

	    if(body && body->type == TYPEMULTIPART)
	      body = &body->nested.part->body;

	    if(body && body->type == TYPETEXT)
	      the_text = (STORE_S *) body->contents.text.data;

	    if(!the_text)
	      return(ascii);

	    so_seek(the_text, 0L, 0);		/* rewind */

	    charsetmap = init_charsetchecker(preferred_charset);

	    if(!charsetmap)
	      return(utf8);

	    validbitmap = ~0;

	    /*
	     * Read a stream of UTF-8 characters from the_text
	     * and convert them to UCS-4 characters for the translatable
	     * test.
	     */
	    while((validbitmap & ~0x1) && so_readc(&c, the_text)){
		if((outchars = utf8_to_ucs4_oneatatime(c, &cbuf, &ucs, NULL)) > 0){
		    /* got a ucs character */
		    if(ucs > 0xffff)
		      return(utf8);

		    validbitmap &= charsetmap[(unsigned long) ucs];
		}
	    }

	    notcjk = validbitmap & 0x1;
	    validbitmap &= ~0x1;

	    if(!validbitmap)
	      return(utf8);
	}

	/* user chooses something other than UTF-8 */
	if(strucmp(ps_global->posting_charmap, utf8)){
	    /*
	     * If we're to post in other than UTF-8, and it can be
	     * transliterated without losing fidelity, do it.
	     * Else, use UTF-8.
	     */

	    /* if ascii works, always use that */
	    if(representable_in_charset(validbitmap, ascii))
	      return(ascii);

	    /* does the user's posting character set work? */
	    if(representable_in_charset(validbitmap, ps_global->posting_charmap))
	      return(ps_global->posting_charmap);

	    /* this is the charset the message we are replying to was in */
	    if(preferred_charset
	       && strucmp(preferred_charset, ascii)
	       && representable_in_charset(validbitmap, preferred_charset))
	      return(preferred_charset);

	    /* else, use UTF-8 */

	}
	/* user chooses nothing, going with the default */
	else if(ps_global->vars[V_POST_CHAR_SET].main_user_val.p == NULL
		&& ps_global->vars[V_POST_CHAR_SET].post_user_val.p == NULL
		&& ps_global->vars[V_POST_CHAR_SET].fixed_val.p == NULL){
	    char *most_preferred;

	    /*
	     * In this case the user didn't specify a posting character set
	     * and we will choose the most-specific one from our list.
	     */

	    /* ascii is best */
	    if(representable_in_charset(validbitmap, ascii))
	      return(ascii);

	    /* Can we keep the original from the message we're replying to?  */
	    if(preferred_charset
	       && strucmp(preferred_charset, ascii)
	       && representable_in_charset(validbitmap, preferred_charset))
	      return(preferred_charset);

	    /* choose the best of the rest */
	    most_preferred = most_preferred_charset(validbitmap);
	    if(!most_preferred)
	      return(utf8);

	    /*
	     * If the text we're labeling contains something like
	     * smart quotes but no CJK characters, then instead of
	     * labeling it as ISO-2022-JP we want to use UTF-8.
	     */
	    if(notcjk){
		const CHARSET *cs;

		cs = utf8_charset(most_preferred);
		if(!cs
		   || cs->script == SC_CHINESE_SIMPLIFIED
		   || cs->script == SC_CHINESE_TRADITIONAL
		   || cs->script == SC_JAPANESE
		   || cs->script == SC_KOREAN)
		  return(utf8);
	    }

	    return(most_preferred);
	}
	/* user explicitly chooses UTF-8 */
	else{
	    /* if ascii works, always use that */
	    if(representable_in_charset(validbitmap, ascii))
	      return(ascii);

	    /* else, use UTF-8 */

	}
    }

    return(utf8);
}


static char **charsetlist = NULL;
static int items_in_charsetlist = 0;
static unsigned long *charsetmap = NULL;

static char *downgrades[] = {
    "US-ASCII",
    "ISO-8859-15",
    "ISO-8859-1",
    "ISO-8859-2",
    "VISCII",
    "KOI8-R",
    "KOI8-U",
    "ISO-8859-7",
    "ISO-8859-6",
    "ISO-8859-8",
    "TIS-620",
    "ISO-2022-JP",
    "GB2312",
    "BIG5",
    "EUC-KR"
};


unsigned long *
init_charsetchecker(char *preferred_charset)
{
    int i, count = 0, reset = 0;
    char *ascii = "US-ASCII";
    char *utf8 = "UTF-8";

    /*
     * When user doesn't set a posting character set posting_charmap ends up
     * set to UTF-8. That also happens if user sets UTF-8 explicitly.
     * That's where the strange set of if-else's come from.
     */

    /* user chooses something other than UTF-8 */
    if(strucmp(ps_global->posting_charmap, utf8)){
	count++;	/* US-ASCII */
	if(items_in_charsetlist < 1 || strucmp(charsetlist[0], ascii))
	  reset++;

	/* if posting_charmap is valid, include it in list */
	if(ps_global->posting_charmap && ps_global->posting_charmap[0]
	   && strucmp(ps_global->posting_charmap, ascii)
	   && strucmp(ps_global->posting_charmap, utf8)
	   && utf8_charset(ps_global->posting_charmap)){
	    count++;
	    if(!reset
	       && (items_in_charsetlist < count
	           || strucmp(charsetlist[count-1], ps_global->posting_charmap)))
	      reset++;
	}

	if(preferred_charset && preferred_charset[0]
	   && strucmp(preferred_charset, ascii)
	   && strucmp(preferred_charset, utf8)
	   && (count < 2 || strucmp(preferred_charset, ps_global->posting_charmap))){
	    count++;
	    if(!reset
	       && (items_in_charsetlist < count
	           || strucmp(charsetlist[count-1], preferred_charset)))
	      reset++;
	}

	if(items_in_charsetlist != count)
	  reset++;

	if(reset){
	    if(charsetlist)
	      free_list_array(&charsetlist);

	    items_in_charsetlist = count;
	    charsetlist = (char **) fs_get((count + 1) * sizeof(char *));

	    i = 0;
	    charsetlist[i++] = cpystr(ascii);

	    if(ps_global->posting_charmap && ps_global->posting_charmap[0]
	       && strucmp(ps_global->posting_charmap, ascii)
	       && strucmp(ps_global->posting_charmap, utf8)
	       && utf8_charset(ps_global->posting_charmap))
	      charsetlist[i++] = cpystr(ps_global->posting_charmap);

	    if(preferred_charset && preferred_charset[0]
	       && strucmp(preferred_charset, ascii)
	       && strucmp(preferred_charset, utf8)
	       && (i < 2 || strucmp(preferred_charset, ps_global->posting_charmap)))
	      charsetlist[i++] = cpystr(preferred_charset);

	    charsetlist[i] = NULL;
	}
    }
    /* user chooses nothing, going with the default */
    else if(ps_global->vars[V_POST_CHAR_SET].main_user_val.p == NULL
	    && ps_global->vars[V_POST_CHAR_SET].post_user_val.p == NULL
	    && ps_global->vars[V_POST_CHAR_SET].fixed_val.p == NULL){
	int add_preferred = 0;

	/* does preferred_charset have to be added to the list?  */
	if(preferred_charset && preferred_charset[0] && strucmp(preferred_charset, utf8)){
	    add_preferred = 1;
	    for(i = 0; add_preferred && i < sizeof(downgrades)/sizeof(downgrades[0]); i++)
	      if(!strucmp(downgrades[i], preferred_charset))
		add_preferred = 0;
	}

	if(add_preferred){
	    /* existing list is right size already */
	    if(items_in_charsetlist == sizeof(downgrades)/sizeof(downgrades[0]) + 1){
		/* just check to see if last list item is the preferred_charset */
		if(strucmp(preferred_charset, charsetlist[items_in_charsetlist-1])){
		    /* no, fix it */
		    reset++;
		    fs_give((void **) &charsetlist[items_in_charsetlist-1]);
		    charsetlist[items_in_charsetlist-1] = cpystr(preferred_charset);
		}
	    }
	    else{
		reset++;
		if(charsetlist)
		  free_list_array(&charsetlist);

		count = sizeof(downgrades)/sizeof(downgrades[0]) + 1;
		items_in_charsetlist = count;
		charsetlist = (char **) fs_get((count + 1) * sizeof(char *));
		for(i = 0; i < sizeof(downgrades)/sizeof(downgrades[0]); i++)
		  charsetlist[i] = cpystr(downgrades[i]);

		charsetlist[i++] = cpystr(preferred_charset);
		charsetlist[i] = NULL;
	    }
	}
	else{
	    /* if list is same size as downgrades, consider it good */
	    if(items_in_charsetlist != sizeof(downgrades)/sizeof(downgrades[0]))
	      reset++;

	    if(reset){
		if(charsetlist)
		  free_list_array(&charsetlist);

		count = sizeof(downgrades)/sizeof(downgrades[0]);
		items_in_charsetlist = count;
		charsetlist = (char **) fs_get((count + 1) * sizeof(char *));
		for(i = 0; i < sizeof(downgrades)/sizeof(downgrades[0]); i++)
		  charsetlist[i] = cpystr(downgrades[i]);

		charsetlist[i] = NULL;
	    }
	}
    }
    /* user explicitly chooses UTF-8 */
    else{
	/* include possibility of ascii even if they explicitly ask for UTF-8 */
	count++;	/* US-ASCII */
	if(items_in_charsetlist < 1 || strucmp(charsetlist[0], ascii))
	  reset++;

	if(items_in_charsetlist != count)
	  reset++;

	if(reset){
	    if(charsetlist)
	      free_list_array(&charsetlist);

	    /* the list is just ascii and nothing else */
	    items_in_charsetlist = count;
	    charsetlist = (char **) fs_get((count + 1) * sizeof(char *));

	    i = 0;
	    charsetlist[i++] = cpystr(ascii);
	    charsetlist[i] = NULL;
	}
    }


    if(reset){
	if(charsetmap)
	  fs_give((void **) &charsetmap);

	if(charsetlist)
	  charsetmap = utf8_csvalidmap(charsetlist);
    }

    return(charsetmap);
}


/* total reset */
void
free_charsetchecker(void)
{
    if(charsetlist)
      free_list_array(&charsetlist);

    items_in_charsetlist = 0;

    if(charsetmap)
      fs_give((void **) &charsetmap);
}


int
representable_in_charset(unsigned long validbitmap, char *charset)
{
    int i, done = 0, ret = 0;
    unsigned long j;

    if(!(charset && charset[0]))
      return ret;

    if(!strucmp(charset, "UTF-8"))
      return 1;

    for(i = 0; !done && i < items_in_charsetlist; i++){
	if(!strucmp(charset, charsetlist[i])){
	    j = 1;
	    j <<= (i+1);
	    done++;
	    if(validbitmap & j)
	      ret = 1;
	}
    }

    return ret;
}


char *
most_preferred_charset(unsigned long validbitmap)
{
    unsigned long bm;
    unsigned long rm;
    int index;

    if(!(validbitmap && items_in_charsetlist > 0))
      return("UTF-8");

    /* careful, find_rightmost_bit modifies the bitmap */
    bm = validbitmap;
    rm = find_rightmost_bit(&bm);
    index = MIN(MAX(rm-1,0), items_in_charsetlist-1);

    return(charsetlist[index]);
}


/*
 * Set parameter to new value.
 */
void
set_parameter(PARAMETER **param, char *paramname, char *new_value)
{
    PARAMETER *pm;

    if(!param || !(paramname && *paramname))
      return;

    if(*param == NULL){
	pm = (*param) = mail_newbody_parameter();
	pm->attribute = cpystr(paramname);
    }
    else{
	int nomatch;

	for(pm = *param;
	    (nomatch=strucmp(pm->attribute, paramname)) && pm->next != NULL;
	    pm = pm->next)
	  ;/* searching for paramname parameter */

	if(nomatch){			/* add charset parameter */
	    pm->next = mail_newbody_parameter();
	    pm = pm->next;
	    pm->attribute = cpystr(paramname);
	}
	/* else pm is existing paramname parameter */
    }

    if(pm){
	if(!(pm->value && new_value && !strcmp(pm->value, new_value))){
	    if(pm->value)
	      fs_give((void **) &pm->value);

	    if(new_value)
	      pm->value = cpystr(new_value);
	}
    }
}


/*----------------------------------------------------------------------
    Remove the leading digits from SMTP error messages
 -----*/
char *
tidy_smtp_mess(char *error, char *printstring, char *outbuf, size_t outbuflen)
{
    while(isdigit((unsigned char)*error) || isspace((unsigned char)*error) ||
	  (*error == '.' && isdigit((unsigned char)*(error+1))))
      error++;

    snprintf(outbuf, outbuflen, printstring, error);
    outbuf[outbuflen-1] = '\0';
    return(outbuf);
}


/*
 * Local globals pine's body output routine needs
 */
static soutr_t    l_f;
static TCPSTREAM *l_stream;
static unsigned   c_in_buf = 0;

/*
 * def to make our pipe write's more friendly
 */
#ifdef	PIPE_MAX
#if	PIPE_MAX > 20000
#undef	PIPE_MAX
#endif
#endif

#ifndef	PIPE_MAX
#define	PIPE_MAX	1024
#endif


/*
 * l_flust_net - empties gf_io terminal function's buffer
 */
int
l_flush_net(int force)
{
    if(c_in_buf && c_in_buf < SIZEOF_20KBUF){
	char *p = &tmp_20k_buf[0], *lp = NULL, c = '\0';

	tmp_20k_buf[c_in_buf] = '\0';
	if(!force){
	    /*
	     * The start of each write is expected to be the start of a
	     * "record" (i.e., a CRLF terminated line).  Make sure that is true
	     * else we might screw up SMTP dot quoting...
	     */
	    for(p = tmp_20k_buf, lp = NULL;
		(p = strstr(p, "\015\012")) != NULL;
		lp = (p += 2))
	      ;
	      

	    if(!lp && c_in_buf > 2)			/* no CRLF! */
	      for(p = &tmp_20k_buf[c_in_buf] - 2;
		  p > &tmp_20k_buf[0] && *p == '.';
		  p--)					/* find last non-dot */
		;

	    if(lp && *lp && lp >= tmp_20k_buf && lp < tmp_20k_buf + SIZEOF_20KBUF){
		/* snippet remains */
		c = *lp;
		*lp = '\0';
	    }
	}

	if((l_f && !(*l_f)(l_stream, tmp_20k_buf))
	   || (lmc.so && !lmc.all_written
	       && !(lmc.text_only && lmc.text_written)
	       && !so_puts(lmc.so, tmp_20k_buf)))
	  return(0);

	c_in_buf = 0;
	if(lp && lp >= tmp_20k_buf && lp < tmp_20k_buf + SIZEOF_20KBUF && (*lp = c))				/* Shift text left? */
	  while(c_in_buf < SIZEOF_20KBUF && (tmp_20k_buf[c_in_buf] = *lp))
	    c_in_buf++, lp++;
    }

    return(1);
}


/*
 * l_putc - gf_io terminal function that calls smtp's soutr_t function.
 *
 */
int
l_putc(int c)
{
    if(c_in_buf >= 0 && c_in_buf < SIZEOF_20KBUF)
      tmp_20k_buf[c_in_buf++] = (char) c;

    return((c_in_buf >= PIPE_MAX) ? l_flush_net(FALSE) : TRUE);
}



/*
 * pine_rfc822_output_body - pine's version of c-client call.  Again, 
 *                necessary since c-client doesn't know about how
 *                we're treating attachments
 */
long
pine_rfc822_output_body(struct mail_bodystruct *body, soutr_t f, void *s)
{
    PART *part;
    PARAMETER *param;
    char *t, *cookie = NIL, *encode_error;
    char tmp[MAILTMPLEN];
    int                add_trailing_crlf;
    LOC_2022_JP ljp;
    gf_io_t            gc;

    dprint((4, "-- pine_rfc822_output_body: %d\n",
	       body ? body->type : 0));
    if(body->type == TYPEMULTIPART) {   /* multipart gets special handling */
	part = body->nested.part;	/* first body part */
					/* find cookie */
	for (param = body->parameter; param && !cookie; param = param->next)
	  if (!strucmp (param->attribute,"BOUNDARY")) cookie = param->value;
	if (!cookie) cookie = "-";	/* yucky default */

	/*
	 * Output a bit of text before the first multipart delimiter
	 * to warn unsuspecting users of non-mime-aware ua's that
	 * they should expect weirdness...
	 */
	if(f && !(*f)(s, "  This message is in MIME format.  The first part should be readable text,\015\012  while the remaining parts are likely unreadable without MIME-aware tools.\015\012\015\012"))
	  return(0);

	do {				/* for each part */
					/* build cookie */
	    snprintf (tmp, sizeof(tmp), "--%s\015\012", cookie);
	    tmp[sizeof(tmp)-1] = '\0';
					/* append cookie,mini-hdr,contents */
	    if((f && !(*f)(s, tmp))
	       || (lmc.so && !lmc.all_written && !so_puts(lmc.so, tmp))
	       || !pine_write_body_header(&part->body,f,s)
	       || !pine_rfc822_output_body (&part->body,f,s))
	      return(0);
	} while ((part = part->next) != NULL);	/* until done */
					/* output trailing cookie */
	snprintf (t = tmp, sizeof(tmp), "--%s--",cookie);
	tmp[sizeof(tmp)-1] = '\0';
	if(lmc.so && !lmc.all_written){
	    so_puts(lmc.so, t);
	    so_puts(lmc.so, "\015\012");
	}

	return(f ? ((*f) (s,t) && (*f) (s,"\015\012")) : 1);
    }

    l_f      = f;			/* set up for writing chars...  */
    l_stream = s;			/* out other end of pipe...     */
    gf_filter_init();
    dprint((4, "-- pine_rfc822_output_body: segment %ld bytes\n",
	       body->size.bytes));

    if(body->contents.text.data)
      gf_set_so_readc(&gc, (STORE_S *) body->contents.text.data);
    else
      return(1);

    /*
     * Don't add trailing line if it is ExternalText, which already guarantees
     * a trailing newline.
     */
    add_trailing_crlf = !(((STORE_S *) body->contents.text.data)->src == ExternalText);

    so_seek((STORE_S *) body->contents.text.data, 0L, 0);

    if(body->type != TYPEMESSAGE){ 	/* NOT encapsulated message */
	char *charset;

	if(body->type == TYPETEXT
	   && so_attr((STORE_S *) body->contents.text.data, "edited", NULL)
	   && (charset = parameter_val(body->parameter, "charset"))){
	    if(strucmp(charset, "utf-8") && strucmp(charset, "us-ascii")){
		if(!strucmp(charset, "iso-2022-jp")){
		    ljp.report_err = 0;
		    gf_link_filter(gf_line_test,
				   gf_line_test_opt(translate_utf8_to_2022_jp,&ljp));
		}
		else{
		    void *table = utf8_rmap(charset);

		    if(table){
			gf_link_filter(gf_convert_utf8_charset,
				       gf_convert_utf8_charset_opt(table,0));
		    }
		    else{
			/* else, just send it? */
			set_parameter(&body->parameter, "charset", "UTF-8");
		    }
		}
	    }

	    fs_give((void **)&charset);
	}

	/*
	 * Convert text pieces to canonical form
	 * BEFORE applying any encoding (rfc1341: appendix G)...
	 * NOTE: almost all filters expect CRLF newlines 
	 */
	if(body->type == TYPETEXT
	   && body->encoding != ENCBASE64
	   && !so_attr((STORE_S *) body->contents.text.data, "rawbody", NULL)){
	    gf_link_filter(gf_local_nvtnl, NULL);
	}

	switch (body->encoding) {	/* all else needs filtering */
	  case ENC8BIT:			/* encode 8BIT into QUOTED-PRINTABLE */
	    gf_link_filter(gf_8bit_qp, NULL);
	    break;

	  case ENCBINARY:		/* encode binary into BASE64 */
	    gf_link_filter(gf_binary_b64, NULL);
	    break;

	  default:			/* otherwise text */
	    break;
	}
    }

    if((encode_error = gf_pipe(gc, l_putc)) != NULL){ /* shove body part down pipe */
	q_status_message1(SM_ORDER | SM_DING, 3, 4,
			  _("Encoding Error \"%s\""), encode_error);
	display_message('x');
    }

    gf_clear_so_readc((STORE_S *) body->contents.text.data);

    if(encode_error || !l_flush_net(TRUE))
      return(0);

    send_bytes_sent += gf_bytes_piped();
    so_release((STORE_S *)body->contents.text.data);

    if(lmc.so && !lmc.all_written && lmc.text_only){
	if(lmc.text_written){	/* we have some splainin' to do */
	    char tmp[MAILTMPLEN];
	    char *name = NULL;

	    if(!(so_puts(lmc.so,_("The following attachment was sent,\015\012"))
		 && so_puts(lmc.so,_("but NOT saved in the Fcc copy:\015\012"))))
	      return(0);

	    /*
	     * BUG: If this name is not ascii it's going to cause trouble.
	     */
	    name = parameter_val(body->parameter, "name");
	    snprintf(tmp, sizeof(tmp),
    "    A %s/%s%s%s%s segment of about %s bytes.\015\012",
		    body_type_names(body->type), 
		    body->subtype ? body->subtype : "Unknown",
		    name ? " (Name=\"" : "",
		    name ? name : "",
		    name ? "\")" : "",
		    comatose(body->size.bytes));
	    tmp[sizeof(tmp)-1] = '\0';
	    if(name)
	      fs_give((void **)&name);

	    if(!so_puts(lmc.so, tmp))
	      return(0);
	}
	else			/* suppress everything after first text part */
	  lmc.text_written = (body->type == TYPETEXT
			      && (!body->subtype
				  || !strucmp(body->subtype, "plain")));
    }

    if(add_trailing_crlf)
      return((f ? (*f)(s, "\015\012") : 1)	/* output final stuff */
	   && ((lmc.so && !lmc.all_written) ? so_puts(lmc.so,"\015\012") : 1));
    else
      return(1);
}


/*
 * pine_write_body_header - another c-client clone.  This time
 *                          so the final encoding labels get set 
 *                          correctly since it hasn't happened yet,
 *			    and to be paranoid about line lengths.
 *
 * Returns: TRUE/nonzero on success, zero on error
 */
int
pine_write_body_header(struct mail_bodystruct *body, soutr_t f, void *s)
{
    char tmp[MAILTMPLEN];
    RFC822BUFFER rbuf;
    int  i;
    unsigned char c;
    STRINGLIST *stl;
    STORE_S    *so;
    extern const char *tspecials;

    if((so = so_get(CharStar, NULL, WRITE_ACCESS)) != NULL){
	if(!(so_puts(so, "Content-Type: ")
	     && so_puts(so, body_types[body->type])
	     && so_puts(so, "/")
	     && so_puts(so, body->subtype
			      ? body->subtype
			      : rfc822_default_subtype (body->type))))
	  return(pwbh_finish(0, so));
	    
	if(body->parameter){
	    if(!pine_write_params(body->parameter, so))
	      return(pwbh_finish(0, so));
	}
	else if(!so_puts(so, "; CHARSET=US-ASCII"))
	  return(pwbh_finish(0, so));
	    
	if(!so_puts(so, "\015\012"))
	  return(pwbh_finish(0, so));

	if ((body->encoding		/* note: encoding 7BIT never output! */
	     && !(so_puts(so, "Content-Transfer-Encoding: ")
		  && so_puts(so, body_encodings[(body->encoding==ENCBINARY)
						? ENCBASE64
						: (body->encoding == ENC8BIT)
						  ? ENCQUOTEDPRINTABLE
						  : (body->encoding <= ENCMAX)
						    ? body->encoding
						    : ENCOTHER])
		  && so_puts(so, "\015\012")))
	    /*
	     * If requested, strip Content-ID headers that don't look like they
	     * are needed. Microsoft's Outlook XP has a bug that causes it to
	     * not show that there is an attachment when there is a Content-ID
	     * header present on that attachment.
	     *
	     * If user has quell-content-id turned on, don't output content-id
	     * unless it is of type message/external-body.
	     * Since this code doesn't look inside messages being forwarded
	     * type message content-ids will remain as is and type multipart
	     * alternative will remain as is. We don't create those on our
	     * own. If we did, we'd have to worry about getting this right.
	     */
	    || (body->id && (F_OFF(F_QUELL_CONTENT_ID, ps_global)
	                     || (body->type == TYPEMESSAGE
			         && body->subtype
				 && !strucmp(body->subtype, "external-body")))
		&& !(so_puts(so, "Content-ID: ") && so_puts(so, body->id)
		     && so_puts(so, "\015\012")))
	    || (body->description
		&& strlen(body->description) < 5000	/* arbitrary! */
		&& !pine_write_header_line("Content-Description: ", body->description, so))
	    || (body->md5
		&& !(so_puts(so, "Content-MD5: ")
		     && so_puts(so, body->md5)
		     && so_puts(so, "\015\012"))))
	  return(pwbh_finish(0, so));

	if ((stl = body->language) != NULL) {
	    if(!so_puts(so, "Content-Language: "))
	      return(pwbh_finish(0, so));

	    do {
		if(strlen((char *)stl->text.data) > 500) /* arbitrary! */
		  return(pwbh_finish(0, so));

		tmp[0] = '\0';
		rbuf.f   = dummy_soutr;
		rbuf.s   = NULL;
		rbuf.beg = tmp;
		rbuf.cur = tmp;
		rbuf.end = tmp+sizeof(tmp)-1;
		rfc822_output_cat(&rbuf, (char *)stl->text.data, tspecials);
		*rbuf.cur = '\0';

		if(!so_puts(so, tmp)
		   || ((stl = stl->next) && !so_puts(so, ", ")))
		  return(pwbh_finish(0, so));
	    }
	    while (stl);

	    if(!so_puts(so, "\015\012"))
	      return(pwbh_finish(0, so));
	}

	if (body->disposition.type) {
	    if(!(so_puts(so, "Content-Disposition: ")
		 && so_puts(so, body->disposition.type)))
	      return(pwbh_finish(0, so));

	    if(!pine_write_params(body->disposition.parameter, so))
	      return(pwbh_finish(0, so));	      

	    if(!so_puts(so, "\015\012"))
	      return(pwbh_finish(0, so));
	}

	/* copy out of so, a line at a time (or less than a K)
	 * and send it down the pike
	 */
	so_seek(so, 0L, 0);
	i = 0;
	while(so_readc(&c, so))
	  if((tmp[i++] = c) == '\012' || i > sizeof(tmp) - 3){
	      tmp[i] = '\0';
	      if((f && !(*f)(s, tmp))
		 || !lmc_body_header_line(tmp, i <= sizeof(tmp) - 3))
		return(pwbh_finish(0, so));

	      i = 0;
	  }

	/* Finally, write blank line */
	if((f && !(*f)(s, "\015\012")) || !lmc_body_header_finish())
	  return(pwbh_finish(0, so));
	
	return(pwbh_finish(i == 0, so)); /* better of ended on LF */
    }

    return(0);
}


/*
 * pine_write_header - convert, encode (if needed) and
 *                     write "header-name: field-body"
 */
int
pine_write_header_line(char *hdr, char *val, STORE_S *so)
{
    char *cv, *cs, *vp;
    int   rv;

    cs = posting_characterset(val, NULL, HdrText);
    cv = utf8_to_charset(val, cs, 0);
    vp = rfc1522_encode(tmp_20k_buf, SIZEOF_20KBUF,
			(unsigned char *) cv, cs);

    rv = (so_puts(so, hdr) && so_puts(so, vp) && so_puts(so, "\015\012"));

    if(cv && cv != val)
      fs_give((void **) &cv);


    return(rv);
}


/*
 * pine_write_param - convert, encode and write MIME header-field parameters
 */
int
pine_write_params(PARAMETER *param, STORE_S *so)
{	      
    for(; param; param = param->next){
	int   rv;
	char *cv, *cs;
	extern const char *tspecials;

	cs = posting_characterset(param->value, NULL, HdrText);
	cv = utf8_to_charset(param->value, cs, 0);
	rv = (so_puts(so, "; ")
	      && rfc2231_output(so, param->attribute, cv, (char *) tspecials, cs));

	if(cv && cv != param->value)
	  fs_give((void **) &cv);

	if(!rv)
	  return(0);
    }

    return(1);
}



int
lmc_body_header_line(char *line, int beginning)
{
    if(lmc.so && !lmc.all_written){
	if(beginning && lmc.text_only && lmc.text_written
	   && (!struncmp(line, "content-type:", 13)
	       || !struncmp(line, "content-transfer-encoding:", 26)
	       || !struncmp(line, "content-disposition:", 20))){
	    /*
	     * "comment out" the real values since our comment isn't
	     * likely the same type, disposition nor encoding...
	     */
	    if(!so_puts(lmc.so, "X-"))
	      return(FALSE);
	}

	return(so_puts(lmc.so, line));
    }

    return(TRUE);
}


int
lmc_body_header_finish(void)
{
    if(lmc.so && !lmc.all_written){
	if(lmc.text_only && lmc.text_written
	   && !so_puts(lmc.so, "Content-Type: TEXT/PLAIN\015\012"))
	  return(FALSE);

	return(so_puts(lmc.so, "\015\012"));
    }

    return(TRUE);
}



int
pwbh_finish(int rv, STORE_S *so)
{
    if(so)
      so_give(&so);

    return(rv);
}


/*
 * pine_free_body - c-client call wrapper so the body data pointer we
 *		    we're using in a way c-client doesn't know about
 *		    gets free'd appropriately.
 */
void
pine_free_body(struct mail_bodystruct **body)
{
    /*
     * Preempt c-client's contents.text.data clean up since we've
     * usurped it's meaning for our own purposes...
     */
    pine_free_body_data (*body);

    /* Then let c-client handle the rest... */
    mail_free_body(body);
}


/* 
 * pine_free_body_data - free pine's interpretations of the body part's
 *			 data pointer.
 */
void
pine_free_body_data(struct mail_bodystruct *body)
{
    if(body){
	if(body->type == TYPEMULTIPART){
	    PART *part = body->nested.part;
	    do				/* for each part */
	      pine_free_body_data(&part->body);
	    while ((part = part->next) != NULL);	/* until done */
	}
	else if(body->contents.text.data)
	  so_give((STORE_S **) &body->contents.text.data);
    }
}


long
send_body_size(struct mail_bodystruct *body)
{
    long  l = 0L;
    PART *part;

    if(body->type == TYPEMULTIPART) {   /* multipart gets special handling */
	part = body->nested.part;	/* first body part */
	do				/* for each part */
	  l += send_body_size(&part->body);
	while ((part = part->next) != NULL);	/* until done */
	return(l);
    }

    return(l + body->size.bytes);
}


int
sent_percent(void)
{
    int i = (int) (((send_bytes_sent + gf_bytes_piped()) * 100)
							/ send_bytes_to_send);
    return(MIN(i, 100));
}


/*
 * pine_smtp_verbose_out - write 
 */
void
pine_smtp_verbose_out(char *s)
{
#ifdef _WINDOWS
    LPTSTR slpt;
#endif
    if(verbose_send_output && s){
	char *p, last = '\0';

	for(p = s; *p; p++)
	  if(*p == '\015')
	    *p = ' ';
	  else
	    last = *p;

#ifdef _WINDOWS
	/*
	 * The stream is opened in Unicode mode, so we need to fix the
	 * argument to fputs.
	 */
	slpt = utf8_to_lptstr((LPSTR) s);
	if(slpt){
	    _fputts(slpt, verbose_send_output);
	    fs_give((void **) &slpt);
	}

	if(last != '\012')
	  _fputtc(L'\n', verbose_send_output);
#else
	fputs(s, verbose_send_output);
	if(last != '\012')
	  fputc('\n', verbose_send_output);
#endif
    }

}


/*
 * pine_header_forbidden - is this name a "forbidden" header?
 *
 *          name - the header name to check
 *  We don't allow user to change these.
 */
int
pine_header_forbidden(char *name)
{
    char **p;
    static char *forbidden_headers[] = {
	"sender",
	"x-sender",
	"x-x-sender",
	"date",
	"received",
	"message-id",
	"in-reply-to",
	"path",
	"resent-message-id",
	"resent-date",
	"resent-from",
	"resent-sender",
	"resent-to",
	"resent-cc",
	"resent-reply-to",
	"mime-version",
	"content-type",
	"x-priority",
	"user-agent",
	NULL
    };

    for(p = forbidden_headers; *p; p++)
      if(!strucmp(name, *p))
	break;

    return((*p) ? 1 : 0);
}


/*
 * hdr_is_in_list - is there a custom value for this header?
 *
 *          hdr - the header name to check
 *       custom - the list to check in
 *  Returns 1 if there is a custom value, 0 otherwise.
 */
int
hdr_is_in_list(char *hdr, PINEFIELD *custom)
{
    PINEFIELD *pf;

    for(pf = custom; pf && pf->name; pf = pf->next)
      if(strucmp(pf->name, hdr) == 0)
	return 1;
    
    return 0;
}


/*
 * count_custom_hdrs_pf - returns number of custom headers in arg
 *            custom -- the list to be counted
 *  only_nonstandard -- only count headers which aren't standard pine headers
 */
int
count_custom_hdrs_pf(PINEFIELD *custom, int only_nonstandard)
{
    int ret = 0;

    for(; custom && custom->name; custom = custom->next)
      if(!only_nonstandard || !custom->standard)
        ret++;

    return(ret);
}


/*
 * count_custom_hdrs_list - returns number of custom headers in arg
 */
int
count_custom_hdrs_list(char **list)
{
    char **p;
    char  *q     = NULL;
    char  *name;
    char  *t;
    char   save;
    int    ret   = 0;

    if(list){
	for(p = list; (q = *p) != NULL; p++){
	    if(q[0]){
		/* remove leading whitespace */
		name = skip_white_space(q);
		
		/* look for colon or space or end */
		for(t = name;
		    *t && !isspace((unsigned char)*t) && *t != ':'; t++)
		  ;/* do nothing */

		save = *t;
		*t = '\0';
		if(!pine_header_forbidden(name))
		  ret++;

		*t = save;
	    }
	}
    }

    return(ret);
}


/*
 * set_default_hdrval - put the user's default value for this header
 *                      into pf->textbuf.
 *             setthis - the pinefield to be set
 *              custom - where to look for the default
 */
CustomType
set_default_hdrval(PINEFIELD *setthis, PINEFIELD *custom)
{
    PINEFIELD *pf;
    CustomType ret = NoMatch;

    if(!setthis || !setthis->name){
	q_status_message(SM_ORDER,3,7,"Internal error setting default header");
	return(ret);
    }

    setthis->textbuf = NULL;

    for(pf = custom; pf && pf->name; pf = pf->next){
	if(strucmp(pf->name, setthis->name) != 0)
	  continue;

	ret = pf->cstmtype;

	/* turn on editing */
	if(strucmp(pf->name, "From") == 0 || strucmp(pf->name, "Reply-To") == 0)
	  setthis->canedit = 1;

	if(pf->val)
	  setthis->textbuf = cpystr(pf->val);
    }

    if(!setthis->textbuf)
      setthis->textbuf = cpystr("");
    
    return(ret);
}


/*
 * pine_header_standard - is this name a "standard" header?
 *
 *          name - the header name to check
 */
FieldType
pine_header_standard(char *name)
{
    int    i;

    /* check to see if this is a standard header */
    for(i = 0; pf_template[i].name; i++)
      if(!strucmp(name, pf_template[i].name))
	return(pf_template[i].type);

    return(TypeUnknown);
}


/*
 * customized_hdr_setup - setup the PINEFIELDS for all the customized headers
 *                    Allocates space for each name and addr ptr.
 *                    Allocates space for default in textbuf, even if empty.
 *
 *              head - the first PINEFIELD to fill in
 *              list - the list to parse
 */
void
customized_hdr_setup(PINEFIELD *head, char **list, CustomType cstmtype)
{
    char **p, *q, *t, *name, *value, save;
    PINEFIELD *pf;

    pf = head;

    if(list){
        for(p = list; (q = *p) != NULL; p++){

	    if(q[0]){

		/* anything after leading whitespace? */
		if(!*(name = skip_white_space(q)))
		  continue;

		/* look for colon or space or end */
	        for(t = name;
		    *t && !isspace((unsigned char)*t) && *t != ':'; t++)
	    	  ;/* do nothing */

		/* if there is a space in the field-name, skip it */
		if(isspace((unsigned char)*t)){
		    q_status_message1(SM_ORDER, 3, 3,
				    _("Space not allowed in header name (%s)"),
				      name);
		    continue;
		}

		save = *t;
		*t = '\0';

		/* Don't allow any of the forbidden headers. */
		if(pine_header_forbidden(name)){
		    q_status_message1(SM_ORDER | SM_DING, 3, 3,
				      _("Not allowed to change header \"%s\""),
				      name);

		    *t = save;
		    continue;
		}

		if(pf){
		    if(pine_header_standard(name) != TypeUnknown)
		      pf->standard = 1;

		    pf->name     = cpystr(name);
		    pf->type     = FreeText;
		    pf->cstmtype = cstmtype;
		    pf->next     = pf+1;

#ifdef OLDWAY
		    /*
		     * Some mailers apparently break if we change
		     * user@domain into Fred <user@domain> for
		     * return-receipt-to,
		     * so we'll just call this a FreeText field, too.
		     */
		    /*
		     * For now, all custom headers are FreeText except for
		     * this one that we happen to know about.  We might
		     * have to add some syntax to the config option so that
		     * people can tell us their custom header takes addresses.
		     */
		    if(!strucmp(pf->name, "Return-Receipt-to")){
			pf->type = Address;
			pf->addr = (ADDRESS **)fs_get(sizeof(ADDRESS *));
			*pf->addr = (ADDRESS *)NULL;
		    }
#endif /* OLDWAY */

		    *t = save;

		    /* remove space between name and colon */
		    value = skip_white_space(t);

		    /* give them an alloc'd default, even if empty */
		    pf->textbuf = cpystr((*value == ':')
					   ? skip_white_space(++value) : "");
		    if(pf->textbuf && pf->textbuf[0])
		      pf->val = cpystr(pf->textbuf);

		    pf++;
		}
		else
		  *t = save;
	    }
	}
    }

    /* fix last next pointer */
    if(head && pf != head)
      (pf-1)->next = NULL;
}


/*
 * add_defaults_from_list - the PINEFIELDS list given by "head" is already
 *                          setup except that it doesn't have default values.
 *                          Add those defaults if they exist in "list".
 *
 *              head - the first PINEFIELD to add a default to
 *              list - the list to get the defaults from
 */
void
add_defaults_from_list(PINEFIELD *head, char **list)
{
    char **p, *q, *t, *name, *value, save;
    PINEFIELD *pf;

    for(pf = head; pf && list; pf = pf->next){
	if(!pf->name)
	  continue;

        for(p = list; (q = *p) != NULL; p++){

	    if(q[0]){

		/* anything after leading whitespace? */
		if(!*(name = skip_white_space(q)))
		  continue;

		/* look for colon or space or end */
	        for(t = name;
		    *t && !isspace((unsigned char)*t) && *t != ':'; t++)
	    	  ;/* do nothing */

		/* if there is a space in the field-name, skip it */
		if(isspace((unsigned char)*t))
		  continue;

		save = *t;
		*t = '\0';

		if(strucmp(name, pf->name) != 0){
		    *t = save;
		    continue;
		}

		*t = save;

		/*
		 * Found the right header. See if it has a default
		 * value set.
		 */

		/* remove space between name and colon */
		value = skip_white_space(t);

		if(*value == ':'){
		    char *defval;

		    defval = skip_white_space(++value);
		    if(defval && *defval){
			if(pf->val)
			  fs_give((void **)&pf->val);
			
			pf->val = cpystr(defval);
		    }
		}

		break;		/* on to next pf */
	    }
	}
    }
}


/*
 * parse_custom_hdrs - allocate PINEFIELDS for custom headers
 *                     fill in the defaults
 * Args -     list -- The list to parse.
 *        cstmtype -- Fill the in cstmtype field with this value
 */
PINEFIELD *
parse_custom_hdrs(char **list, CustomType cstmtype)
{
    PINEFIELD          *pfields;
    int			i;

    /*
     * add one for possible use by fcc
     *   What is this "possible use"? I don't see it. I don't
     *   think it exists anymore. I think +1 would be ok.   hubert 2000-08-02
     */
    i       = (count_custom_hdrs_list(list) + 2) * sizeof(PINEFIELD);
    pfields = (PINEFIELD *)fs_get((size_t) i);
    memset(pfields, 0, (size_t) i);

    /* set up the custom header pfields */
    customized_hdr_setup(pfields, list, cstmtype);

    return(pfields);
}


/*
 * Combine the two lists of headers into one list which is allocated here
 * and freed by the caller. Eliminate duplicates with values from the role
 * taking precedence over values from the default.
 */
PINEFIELD *
combine_custom_headers(PINEFIELD *dflthdrs, PINEFIELD *rolehdrs)
{
    PINEFIELD *pfields, *pf, *pf2;
    int        max_hdrs, i;

    max_hdrs = count_custom_hdrs_pf(rolehdrs,0) +
               count_custom_hdrs_pf(dflthdrs,0);

    pfields = (PINEFIELD *)fs_get((size_t)(max_hdrs+1)*sizeof(PINEFIELD));
    memset(pfields, 0, (size_t)(max_hdrs+1)*sizeof(PINEFIELD));

    pf = pfields;
    for(pf2 = rolehdrs; pf2 && pf2->name; pf2 = pf2->next){
	pf->name     = pf2->name ? cpystr(pf2->name) : NULL;
	pf->type     = pf2->type;
	pf->cstmtype = pf2->cstmtype;
	pf->textbuf  = pf2->textbuf ? cpystr(pf2->textbuf) : NULL;
	pf->val      = pf2->val ? cpystr(pf2->val) : NULL;
	pf->standard = pf2->standard;
	pf->next     = pf+1;
	pf++;
    }

    /* if these aren't already there, add them */
    for(pf2 = dflthdrs; pf2 && pf2->name; pf2 = pf2->next){
	/* check for already there */
	for(i = 0;
	    pfields[i].name && strucmp(pfields[i].name, pf2->name);
	    i++)
	  ;
	
	if(!pfields[i].name){			/* this is a new one */
	    pf->name     = pf2->name ? cpystr(pf2->name) : NULL;
	    pf->type     = pf2->type;
	    pf->cstmtype = pf2->cstmtype;
	    pf->textbuf  = pf2->textbuf ? cpystr(pf2->textbuf) : NULL;
	    pf->val      = pf2->val ? cpystr(pf2->val) : NULL;
	    pf->standard = pf2->standard;
	    pf->next     = pf+1;
	    pf++;
	}
    }

    /* fix last next pointer */
    if(pf != pfields)
      (pf-1)->next = NULL;

    return(pfields);
}


/*
 * free_customs - free misc. resources associated with custom header fields
 *
 *           pf - pointer to first custom field
 */
void
free_customs(PINEFIELD *head)
{
    PINEFIELD *pf;

    for(pf = head; pf && pf->name; pf = pf->next){

	fs_give((void **)&pf->name);

	if(pf->val)
	  fs_give((void **)&pf->val);

	/* only true for FreeText */
	if(pf->textbuf)
	  fs_give((void **)&pf->textbuf);

	/* only true for Address */
	if(pf->addr && *pf->addr)
	  mail_free_address(pf->addr);
    }

    fs_give((void **)&head);
}


/*
 * encode_whole_header
 *
 * Returns     1 if whole value should be encoded
 *             0 to encode only within comments
 */
int
encode_whole_header(char *field, METAENV *header)
{
    int        retval = 0;
    PINEFIELD *pf;

    if(field && (!strucmp(field, "Subject") ||
		 !strucmp(field, "Comment") ||
		 !struncmp(field, "X-", 2)))
      retval++;
    else if(field && *field && header && header->custom){
	for(pf = header->custom; pf && pf->name; pf = pf->next){

	    if(!pf->standard && !strucmp(pf->name, field)){
		retval++;
		break;
	    }
	}
    }

    return(retval);
}


/*----------------------------------------------------------------------
      Post news via NNTP or inews

Args: env -- envelope of message to post
       body -- body of message to post

Returns: -1 if failed or cancelled, 1 if succeeded

WARNING: This call function has the side effect of writing the message
    to the lmc.so object.   
  ----*/
int
news_poster(METAENV *header, struct mail_bodystruct *body, char **alt_nntp_servers,
	    void (*pipecb_f)(PIPE_S *, int, void *))
{
    char *error_mess, error_buf[200], **news_servers;
    char **servers_to_use;
    int	  we_cancel = 0, server_no = 0, done_posting = 0;
    void *orig_822_output;
    BODY *bp = NULL;

    error_buf[0] = '\0';
    we_cancel = busy_cue("Posting news", NULL, 0);

    dprint((4, "Posting: [%s]\n",
	   (header && header->env && header->env->newsgroups)
	     ? header->env->newsgroups : "?"));

    if((alt_nntp_servers && alt_nntp_servers[0] && alt_nntp_servers[0][0])
       || (ps_global->VAR_NNTP_SERVER && ps_global->VAR_NNTP_SERVER[0]
       && ps_global->VAR_NNTP_SERVER[0][0])){
       /*---------- NNTP server defined ----------*/
	error_mess = NULL;
	servers_to_use = (alt_nntp_servers && alt_nntp_servers[0]
			  && alt_nntp_servers[0][0]) 
	  ? alt_nntp_servers : ps_global->VAR_NNTP_SERVER;

	/*
	 * Install our rfc822 output routine 
	 */
	orig_822_output = mail_parameters(NULL, GET_RFC822OUTPUT, NULL);
	(void) mail_parameters(NULL, SET_RFC822OUTPUT,
			       (void *)post_rfc822_output);

	server_no = 0;
	news_servers = (char **)fs_get(2 * sizeof(char *));
	news_servers[1] = NULL;
	while(!done_posting && servers_to_use[server_no] && 
	      servers_to_use[server_no][0]){
	    news_servers[0] = servers_to_use[server_no];
	    ps_global->noshow_error = 1;
#ifdef DEBUG
	    sending_stream = nntp_open(news_servers, debug ? NOP_DEBUG : 0L);
#else
	    sending_stream = nntp_open(news_servers, 0L);
#endif
	    ps_global->noshow_error = 0;
	    
	    if(sending_stream != NULL) {
		unsigned short save_encoding, added_encoding;

		/*
		 * Fake that we've got clearance from the transport agent
		 * for 8bit transport for the benefit of our output routines...
		 */
		if(F_ON(F_ENABLE_8BIT_NNTP, ps_global)
		   && (bp = first_text_8bit(body))){
		    int i;

		    for(i = 0; (i <= ENCMAX) && body_encodings[i]; i++)
		        ;

		    if(i > ENCMAX){		/* no empty encoding slots! */
		        bp = NULL;
		    }
		    else {
			added_encoding = i;
			body_encodings[added_encoding] = body_encodings[ENC8BIT];
			save_encoding = bp->encoding;
			bp->encoding = added_encoding;
		    }
		}

		/*
		 * Set global header pointer so we can get at it later...
		 */
		send_header = header;
		ps_global->noshow_error = 1;
		if(nntp_mail(sending_stream, header->env, body) == 0)
		    snprintf(error_mess = error_buf, sizeof(error_buf),
			    _("Error posting message: %s"),
			    sending_stream->reply);
		else{
		    done_posting = 1;
		    error_buf[0] = '\0';
		    error_mess = NULL;
		}

		error_buf[sizeof(error_buf)-1] = '\0';
		smtp_close(sending_stream);
		ps_global->noshow_error = 0;
		sending_stream = NULL;
		if(F_ON(F_ENABLE_8BIT_NNTP, ps_global) && bp){
		    body_encodings[added_encoding] = NULL;
		    bp->encoding = save_encoding;
		}
		
	    } else {
	        /*---- Open of NNTP connection failed ------ */
	        snprintf(error_mess = error_buf, sizeof(error_buf),
			_("Error connecting to news server: %s"),
			ps_global->c_client_error);
		error_buf[sizeof(error_buf)-1] = '\0';
		dprint((1, error_buf));
	    }
	    server_no++;
	}
	fs_give((void **)&news_servers);
	(void) mail_parameters (NULL, SET_RFC822OUTPUT, orig_822_output);
    } else {
        /*----- Post via local mechanism -------*/
#ifdef	_WINDOWS
	snprintf(error_mess = error_buf, sizeof(error_buf),
		 _("Can't post, NNTP-server must be defined!"));
#else  /* UNIX */
        error_mess = post_handoff(header, body, error_buf, sizeof(error_buf), NULL,
				  pipecb_f);
#endif
    }

    if(we_cancel)
      cancel_busy_cue(0);

    if(error_mess){
	if(lmc.so && !lmc.all_written)
	  so_give(&lmc.so);	/* clean up any fcc data */

        q_status_message(SM_ORDER | SM_DING, 4, 5, error_mess);
        return(-1);
    }

    lmc.all_written = 1;
    return(1);
}


/* ----------------------------------------------------------------------
   Figure out command to start local SMTP agent

  Args: errbuf   -- buffer for reporting errors (assumed non-NULL)

  Returns an alloc'd copy of the local SMTP agent invocation or NULL

  ----*/
char *
smtp_command(char *errbuf, size_t errbuflen)
{
#ifdef	_WINDOWS
    if(!(ps_global->VAR_SMTP_SERVER && ps_global->VAR_SMTP_SERVER[0]
	 && ps_global->VAR_SMTP_SERVER[0][0]))
      strncpy(errbuf,_("SMTP-server must be defined!"),errbuflen);

    errbuf[errbuflen-1] = '\0';
#else	/* UNIX */
# if	defined(SENDMAIL) && defined(SENDMAILFLAGS)
    char tmp[256];

    snprintf(tmp, sizeof(tmp), "%.*s %.*s", (sizeof(tmp)-3)/2, SENDMAIL,
	    (sizeof(tmp)-3)/2, SENDMAILFLAGS);
    return(cpystr(tmp));
# else
    strncpy(errbuf, _("No default posting command."), errbuflen);
    errbuf[errbuflen-1] = '\0';
# endif
#endif

    return(NULL);
}



#ifndef	_WINDOWS
/*----------------------------------------------------------------------
   Hand off given message to local posting agent

  Args: envelope -- The envelope for the BCC and debugging
        header   -- The text of the message header
        errbuf   -- buffer for reporting errors (assumed non-NULL)
	len      -- Length of errbuf
     
   ----*/
int
mta_handoff(METAENV *header, struct mail_bodystruct *body,
	    char *errbuf, size_t len,
	    void (*bigresult_f) (char *, int),
	    void (*pipecb_f)(PIPE_S *, int, void *))
{
#ifdef	DF_SENDMAIL_PATH
    char  cmd_buf[256];
#endif
    char *cmd = NULL;

    /*
     * A bit of complicated policy implemented here.
     * There are two posting variables sendmail-path and smtp-server.
     * Precedence is in that order.
     * They can be set one of 4 ways: fixed, command-line, user, or globally.
     * Precedence is in that order.
     * Said differently, the order goes something like what's below.
     * 
     * NOTE: the fixed/command-line/user precendence handling is also
     *	     indicated by what's pointed to by ps_global->VAR_*, but since
     *	     that also includes the global defaults, it's not sufficient.
     */

    if(ps_global->FIX_SENDMAIL_PATH
       && ps_global->FIX_SENDMAIL_PATH[0]){
	cmd = ps_global->FIX_SENDMAIL_PATH;
    }
    else if(!(ps_global->FIX_SMTP_SERVER
	      && ps_global->FIX_SMTP_SERVER[0])){
	if(ps_global->COM_SENDMAIL_PATH
	   && ps_global->COM_SENDMAIL_PATH[0]){
	    cmd = ps_global->COM_SENDMAIL_PATH;
	}
	else if(!(ps_global->COM_SMTP_SERVER
		  && ps_global->COM_SMTP_SERVER[0])){
	    if((ps_global->vars[V_SENDMAIL_PATH].post_user_val.p
	        && ps_global->vars[V_SENDMAIL_PATH].post_user_val.p[0]) ||
	       (ps_global->vars[V_SENDMAIL_PATH].main_user_val.p
	        && ps_global->vars[V_SENDMAIL_PATH].main_user_val.p[0])){
		if(ps_global->vars[V_SENDMAIL_PATH].post_user_val.p
		   && ps_global->vars[V_SENDMAIL_PATH].post_user_val.p[0])
		  cmd = ps_global->vars[V_SENDMAIL_PATH].post_user_val.p;
		else
		  cmd = ps_global->vars[V_SENDMAIL_PATH].main_user_val.p;
	    }
	    else if(!((ps_global->vars[V_SMTP_SERVER].post_user_val.l
	               && ps_global->vars[V_SMTP_SERVER].post_user_val.l[0]) ||
		      (ps_global->vars[V_SMTP_SERVER].main_user_val.l
	               && ps_global->vars[V_SMTP_SERVER].main_user_val.l[0]))){
		if(ps_global->GLO_SENDMAIL_PATH
		   && ps_global->GLO_SENDMAIL_PATH[0]){
		    cmd = ps_global->GLO_SENDMAIL_PATH;
		}
#ifdef	DF_SENDMAIL_PATH
		/*
		 * This defines the default method of posting.  So,
		 * unless we're told otherwise use it...
		 */
		else if(!(ps_global->GLO_SMTP_SERVER
			  && ps_global->GLO_SMTP_SERVER[0])){
		    strncpy(cmd = cmd_buf, DF_SENDMAIL_PATH, sizeof(cmd_buf)-1);
		    cmd_buf[sizeof(cmd_buf)-1] = '\0';
		}
#endif
	    }
	}
    }

    *errbuf = '\0';
    if(cmd){
	dprint((4, "call_mailer via cmd: %s\n", cmd ? cmd : "?"));

	(void) mta_parse_post(header, body, cmd, errbuf, len, bigresult_f, pipecb_f);
	return(1);
    }
    else
      return(0);
}



/*----------------------------------------------------------------------
   Hand off given message to local posting agent

  Args: envelope -- The envelope for the BCC and debugging
        header   -- The text of the message header
        errbuf   -- buffer for reporting errors (assumed non-NULL)
	errbuflen -- Length of errbuf
     
  Fork off mailer process and pipe the message into it
  Called to post news via Inews when NNTP is unavailable
  
   ----*/
char *
post_handoff(METAENV *header, struct mail_bodystruct *body, char *errbuf,
	     size_t errbuflen,
	     void (*bigresult_f) (char *, int),
	     void (*pipecb_f)(PIPE_S *, int, void *))
{
    char *err = NULL;
#ifdef	SENDNEWS
    char *s;
    char  tmp[200];

    if(s = strstr(header->env->date," (")) /* fix the date format for news */
      *s = '\0';

    if(err = mta_parse_post(header, body, SENDNEWS, errbuf, errbuflen, bigresult_f, pipecb_f)){
	strncpy(tmp, err, sizeof(tmp)-1);
	tmp[sizeof(tmp)-1] = '\0';
	snprintf(err = errbuf, errbuflen, _("News not posted: \"%s\": %s"),
		 SENDNEWS, tmp);
    }

    if(s)
      *s = ' ';				/* restore the date */

#else /* !SENDNEWS */  /* this is the default case */
    strncpy(err = errbuf, _("Can't post, NNTP-server must be defined!"), errbuflen-1);
    err[errbuflen-1] = '\0';
#endif /* !SENDNEWS */
    return(err);
}



/*----------------------------------------------------------------------
   Hand off message to local MTA; it parses recipients from 822 header

  Args: header -- struct containing header data
        body  -- struct containing message body data
	cmd -- command to use for handoff (%s says where file should go)
	errs -- pointer to buf to hold errors

   ----*/
char *
mta_parse_post(METAENV *header, struct mail_bodystruct *body, char *cmd,
	       char *errs, size_t errslen, void (*bigresult_f)(char *, int),
	       void (*pipecb_f)(PIPE_S *, int, void *))
{
    char   *result = NULL;
    PIPE_S *pipe;

    dprint((1, "=== mta_parse_post(%s) ===\n", cmd ? cmd : "?"));

    if((pipe = open_system_pipe(cmd, &result, NULL,
			       PIPE_STDERR|PIPE_WRITE|PIPE_PROT|PIPE_NOSHELL|PIPE_DESC,
			       0, pipecb_f, pipe_report_error)) != NULL){
	if(!pine_rfc822_output(header, body, pine_pipe_soutr_nl,
			       (TCPSTREAM *) pipe)){
	  strncpy(errs, _("Error posting."), errslen-1);
	  errs[errslen-1] = '\0';
	}

	if(close_system_pipe(&pipe, NULL, pipecb_f) && !*errs){
	    snprintf(errs, errslen, _("Posting program %s returned error"), cmd);
	    if(result && bigresult_f)
	      (*bigresult_f)(result, CM_BR_ERROR);
	}
    }
    else
      snprintf(errs, errslen, _("Error running \"%s\""), cmd);

    if(result){
	our_unlink(result);
	fs_give((void **)&result);
    }

    return(*errs ? errs : NULL);
}


/* 
 * pine_pipe_soutr - Replacement for tcp_soutr that writes one of our
 *		     pipes rather than a tcp stream
 */
long
pine_pipe_soutr_nl (void *stream, char *s)
{
    long    rv = T;
    char   *p;
    size_t  n;

    while(*s && rv){
	if((n = (p = strstr(s, "\015\012")) ? p - s : strlen(s)) != 0)
	  while((rv = write(((PIPE_S *)stream)->out.d, s, n)) != n)
	    if(rv < 0){
		if(errno != EINTR){
		    rv = 0;
		    break;
		}
	    }
	    else{
		s += rv;
		n -= rv;
	    }

	if(p && rv){
	    s = p + 2;			/* write UNIX EOL */
	    while((rv = write(((PIPE_S *)stream)->out.d,"\n",1)) != 1)
	      if(rv < 0 && errno != EINTR){
		  rv = 0;
		  break;
	      }
	}
	else
	  break;
    }

    return(rv);
}
#endif


/**************** "PIPE" READING POSTING I/O ROUTINES ****************/


/*
 * helpful def's
 */
#define	S(X)		((PIPE_S *)(X))
#define	GETBUFLEN	(4 * MAILTMPLEN)


void *
piped_smtp_open (char *host, char *service, long unsigned int port)
{
    char *postcmd;
    void *rv = NULL;

    if(strucmp(host, "localhost")){
	char tmp[MAILTMPLEN];

	snprintf(tmp, sizeof(tmp), _("Unexpected hostname for piped SMTP: %.*s"), 
		sizeof(tmp)-50, host);
	tmp[sizeof(tmp)-1] = '\0';
	mm_log(tmp, ERROR);
    }
    else if((postcmd = smtp_command(ps_global->c_client_error, sizeof(ps_global->c_client_error))) != NULL){
	rv = open_system_pipe(postcmd, NULL, NULL,
			      PIPE_READ|PIPE_STDERR|PIPE_WRITE|PIPE_PROT|PIPE_NOSHELL|PIPE_DESC,
			      0, NULL, pipe_report_error);
	fs_give((void **) &postcmd);
    }
    else
      mm_log(ps_global->c_client_error, ERROR);

    return(rv);
}


void *
piped_aopen (NETMBX *mb, char *service, char *user)
{
    return(NULL);
}


/* 
 * piped_soutr - Replacement for tcp_soutr that writes one of our
 *		     pipes rather than a tcp stream
 */
long
piped_soutr (void *stream, char *s)
{
    return(piped_sout(stream, s, strlen(s)));
}


/* 
 * piped_sout - Replacement for tcp_soutr that writes one of our
 *		     pipes rather than a tcp stream
 */
long
piped_sout (void *stream, char *s, long unsigned int size)
{
    int i, o;

    if(S(stream)->out.d < 0)
      return(0L);

    if((i = (int) size) != 0){
	while((o = write(S(stream)->out.d, s, i)) != i)
	  if(o < 0){
	      if(errno != EINTR){
		  piped_abort(stream);
		  return(0L);
	      }
	  }
	  else{
	      s += o;			/* try again, fix up counts */
	      i -= o;
	  }
    }

    return(1L);
}


/* 
 * piped_getline - Replacement for tcp_getline that reads one
 *		       of our pipes rather than a tcp pipe
 *
 *                     C-client expects that the \r\n will be stripped off.
 */
char *
piped_getline (void *stream)
{
    static int   cnt;
    static char *ptr;
    int		 n, m;
    char	*ret, *s, *sp, c = '\0', d;

    if(S(stream)->in.d < 0)
      return(NULL);

    if(!S(stream)->tmp){		/* initialize! */
	/* alloc space to collect input (freed in close_system_pipe) */
	S(stream)->tmp = (char *) fs_get(GETBUFLEN);
	memset(S(stream)->tmp, 0, GETBUFLEN);
	cnt = -1;
    }

    while(cnt < 0){
	while((cnt = read(S(stream)->in.d, S(stream)->tmp, GETBUFLEN)) < 0)
	  if(errno != EINTR){
	      piped_abort(stream);
	      return(NULL);
	  }

	if(cnt == 0){
	    piped_abort(stream);
	    return(NULL);
	}

	ptr = S(stream)->tmp;
    }

    s = ptr;
    n = 0;
    while(cnt--){
	d = *ptr++;
	if((c == '\015') && (d == '\012')){
	    ret = (char *)fs_get (n--);
	    memcpy(ret, s, n);
	    ret[n] = '\0';
	    return(ret);
	}

	n++;
	c = d;
    }
					/* copy partial string from buffer */
    memcpy((ret = sp = (char *) fs_get (n)), s, n);
					/* get more data */
    while(cnt < 0){
	memset(S(stream)->tmp, 0, GETBUFLEN);
	while((cnt = read(S(stream)->in.d, S(stream)->tmp, GETBUFLEN)) < 0)
	  if(errno != EINTR){
	      fs_give((void **) &ret);
	      piped_abort(stream);
	      return(NULL);
	  }

	if(cnt == 0){
	    if(n > 0)
	      ret[n-1] = '\0';  /* to try to get error message logged */
	    else{
		piped_abort(stream);
		return(NULL);
	    }
	}

	ptr = S(stream)->tmp;
    }

    if(c == '\015' && *ptr == '\012'){
	ptr++;
	cnt--;
	ret[n - 1] = '\0';		/* tie off string with null */
    }
    else if ((s = piped_getline(stream)) != NULL) {
	ret = (char *) fs_get(n + 1 + (m = strlen (s)));
	memcpy(ret, sp, n);		/* copy first part */
	memcpy(ret + n, s, m);		/* and second part */
	fs_give((void **) &sp);		/* flush first part */
	fs_give((void **) &s);		/* flush second part */
	ret[n + m] = '\0';		/* tie off string with null */
    }

    return(ret);
}


/* 
 * piped_close - Replacement for tcp_close that closes pipes to our
 *		     child rather than a tcp connection
 */
void
piped_close(void *stream)
{
    /*
     * Uninstall our hooks into smtp_send since it's being used by
     * the nntp driver as well...
     */
    (void) close_system_pipe((PIPE_S **) &stream, NULL, NULL);
}


/*
 * piped_abort - close down the pipe we were using to post
 */
void
piped_abort(void *stream)
{
    if(S(stream)->in.d >= 0){
	close(S(stream)->in.d);
	S(stream)->in.d = -1;
    }

    if(S(stream)->out.d){
	close(S(stream)->out.d);
	S(stream)->out.d = -1;
    }
}


char *
piped_host(void *stream)
{
    return(ps_global->hostname ? ps_global->hostname : "localhost");
}


unsigned long
piped_port(void *stream)
{
    return(0L);
}
