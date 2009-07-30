#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: detach.c 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $";
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
#include "../pith/detach.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/store.h"
#include "../pith/filter.h"
#include "../pith/mailview.h"
#include "../pith/status.h"
#include "../pith/addrstring.h"
#include "../pith/bldaddr.h"
#include "../pith/mimedesc.h"
#include "../pith/adjtime.h"
#include "../pith/pipe.h"
#include "../pith/busy.h"
#include "../pith/signal.h"
#include "../pico/osdep/filesys.h"


/*
 * We need to define simple functions here for the piping and 
 * temporary storage object below.  We can use the filter.c functions
 * because they're already in use for the "putchar" function passed to
 * detach.
 */
static STORE_S *detach_so = NULL;


/*
 * The display filter is locally global because it's set in df_trigger_cmp
 * which sniffs at lines of the unencoded segment...
 */
typedef struct _trigger {
    int		   (*cmp)(char *, char *);
    char	    *text;
    char	    *cmd;
    struct _trigger *next;
} TRGR_S;

static char	*display_filter;
static TRGR_S	*df_trigger_list;

FETCH_READC_S *g_fr_desc;

#define	INIT_FETCH_CHUNK	((unsigned long)(8 * 1024L))
#define	MIN_FETCH_CHUNK		((unsigned long)(4 * 1024L))
#define	MAX_FETCH_CHUNK		((unsigned long)(256 * 1024L))
#define	TARGET_INTR_TIME	((unsigned long)2000000L) /* two seconds */
#define	FETCH_READC	g_fr_desc->readc


/*
 * Internal Prototypes
 */
/*
 * This function is intentionally declared without an argument type so
 * that warnings will go away in Windows. We're using gf_io_t for both
 * input and output functions and the arguments aren't actually the
 * same in the two cases. We should really have a read version and
 * a write version of gf_io_t. That's why this is like this for now.
 */
int	    detach_writec();
TRGR_S	   *build_trigger_list(void);
void	    blast_trigger_list(TRGR_S **);
int	    df_trigger_cmp(long, char *, LT_INS_S **, void *);
int	    df_trigger_cmp_text(char *, char *);
int	    df_trigger_cmp_lwsp(char *, char *);
int	    df_trigger_cmp_start(char *, char *);
int	    fetch_readc_cleanup(void);
char	   *fetch_gets(readfn_t, void *, unsigned long, GETS_DATA *);
int	    fetch_readc(unsigned char *);



/*----------------------------------------------------------------------
  detach the given raw body part; don't do any decoding

  Args: a bunch

  Returns: NULL on success, error message otherwise
  ----*/
char *
detach_raw(MAILSTREAM *stream,	/* c-client stream to use         */
	   long int msg_no,	/* message number to deal with	  */
	   char *part_no,	/* part number of message 	  */
	   gf_io_t pc,		/* where to put it		  */
	   int flags)
{
    FETCH_READC_S  *frd = (FETCH_READC_S *)fs_get(sizeof(FETCH_READC_S));
    char *err = NULL;
    int column = (flags & FM_DISPLAY) ? ps_global->ttyo->screen_cols : 80;
    
    memset(g_fr_desc = frd, 0, sizeof(FETCH_READC_S));
    frd->stream  = stream;
    frd->msgno   = msg_no;
    frd->section = part_no;
    frd->size    = 0;		        /* wouldn't be here otherwise     */
    frd->readc	 = fetch_readc;
    frd->chunk   = pine_mail_fetch_text(stream, msg_no, NULL, &frd->read, 0);
    frd->endp    = &frd->chunk[frd->read];
    frd->chunkp  = frd->chunk;

    gf_filter_init();
    if (!(flags & FM_NOWRAP))
      gf_link_filter(gf_wrap, gf_wrap_filter_opt(column, column, NULL, 0,
						 (flags & FM_DISPLAY)
						   ? GFW_HANDLES : 0));
    err = gf_pipe(FETCH_READC, pc);

    return(err);
}


/*----------------------------------------------------------------------
  detach the given body part using the given encoding

  Args: a bunch

  Returns: NULL on success, error message otherwise
  ----*/
char *
detach(MAILSTREAM *stream,		/* c-client stream to use         */
       long int msg_no,			/* message number to deal with	  */
       char *part_no,			/* part number of message 	  */
       long int partial,		/* if >0, limit read to this many bytes */
       long int *len,			/* returns bytes read in this arg */
       gf_io_t pc,			/* where to put it		  */
       FILTLIST_S *aux_filters,		/* null terminated array of filts */
       long flags)
{
    unsigned long  rv;
    unsigned long  size;
    long           fetch_flags;
    int            we_cancel = 0, is_text;
    char          *status, trigger[MAILTMPLEN];
    char          *charset = NULL;
    BODY	  *body;
    static char    err_string[100];
    FETCH_READC_S  fetch_part;

    err_string[0] = '\0';

    if(!ps_global->print && !pc_is_picotext(pc))
      we_cancel = busy_cue(NULL, NULL, 1);

    gf_filter_init();			/* prepare for filtering! */

    if(!(body = mail_body(stream, msg_no, (unsigned char *) part_no)))
      return(_("Can't find body for requested message"));

    is_text = body->type == TYPETEXT;

    size = body->size.bytes;
    if(partial > 0L && partial < size)
      size = partial;

    fetch_flags = (flags & ~DT_NODFILTER);
    fetch_readc_init(&fetch_part, stream, msg_no, part_no, body->size.bytes, partial,
		     fetch_flags);
    rv = size ? size : 1;

    switch(body->encoding) {		/* handle decoding */
      case ENC7BIT:
      case ENC8BIT:
      case ENCBINARY:
        break;

      case ENCBASE64:
	gf_link_filter(gf_b64_binary, NULL);
        break;

      case ENCQUOTEDPRINTABLE:
	gf_link_filter(gf_qp_8bit, NULL);
        break;

      case ENCOTHER:
      default:
	dprint((1, "detach: unknown CTE: \"%s\" (%d)\n",
		   (body->encoding <= ENCMAX
		    && body_encodings[body->encoding])
		       ? body_encodings[body->encoding]
		       : "BEYOND-KNOWN-TYPES",
		   body->encoding));
	break;
    }

    /* convert all text to UTF-8 */
    if(is_text){
	charset = parameter_val(body->parameter, "charset");

	/*
	 * If the charset is unlabeled or unknown replace it
	 * with the user's configured unknown charset.
	 */
	if(!charset || !strucmp(charset, UNKNOWN_CHARSET) || !strucmp(charset, "us-ascii")){
	    if(charset)
	      fs_give((void **) &charset);

	    if(ps_global->VAR_UNK_CHAR_SET)
	      charset = cpystr(ps_global->VAR_UNK_CHAR_SET);
	}

	/* convert to UTF-8 */
	if(!(charset && !strucmp(charset, "utf-8")))
	  gf_link_filter(gf_utf8, gf_utf8_opt(charset));

	if(charset)
	  fs_give((void **) &charset);
    }

    /*
     * If we're detaching a text segment and there are user-defined
     * filters and there are text triggers to look for, install filter
     * to let us look at each line...
     */
    display_filter = NULL;
    if(is_text
       && ps_global->tools.display_filter
       && ps_global->tools.display_filter_trigger
       && ps_global->VAR_DISPLAY_FILTERS
       && !(flags & DT_NODFILTER)){
	/* check for "static" triggers (i.e., none or CHARSET) */
	if(!(display_filter = (*ps_global->tools.display_filter_trigger)(body, trigger, sizeof(trigger)))
	   && (df_trigger_list = build_trigger_list())){
	    /* else look for matching text trigger */
	    gf_link_filter(gf_line_test,
			   gf_line_test_opt(df_trigger_cmp, NULL));
	}
    }
    else
      /* add aux filters if we're not going to MIME decode into a temporary
       * storage object, otherwise we pass the aux_filters on to gf_filter
       * below so it can pass what comes out of the external filter command
       * thru the rest of the filters...
       */
      for( ; aux_filters && aux_filters->filter; aux_filters++)
	gf_link_filter(aux_filters->filter, aux_filters->data);

    /*
     * Following canonical model, after decoding convert newlines from
     * crlf to local convention.  ALSO, convert newlines if we're fetching
     * a multipart segment since an external handler's going to have to
     * make sense of it...
     */
    if(is_text || body->type == TYPEMESSAGE || body->type == TYPEMULTIPART)
      gf_link_filter(gf_nvtnl_local, NULL);

    /*
     * If we're detaching a text segment and a user-defined filter may
     * need to be invoked later (see below), decode the segment into
     * a temporary storage object...
     */
    if(is_text
       && ps_global->tools.display_filter
       && ps_global->tools.display_filter_trigger
       && ps_global->VAR_DISPLAY_FILTERS
       && !(flags & DT_NODFILTER)
       && !(detach_so = so_get(CharStar, NULL, EDIT_ACCESS))){
      strncpy(err_string,
	   _("Formatting error: no space to make copy, no display filters used"), sizeof(err_string));
      err_string[sizeof(err_string)-1] = '\0';
    }

    if((status = gf_pipe(FETCH_READC, detach_so ? detach_writec : pc)) != NULL) {
	snprintf(err_string, sizeof(err_string), "Formatting error: %s", status);
        rv = 0L;
    }

    /*
     * If we wrote to a temporary area, there MAY be a user-defined
     * filter to invoke.  Filter it it (or not if no trigger match)
     * *AND* send the output thru any auxiliary filters, destroy the
     * temporary object and be done with it...
     */
    if(detach_so){
	if(!err_string[0] && display_filter && *display_filter){
	    FILTLIST_S *p, *aux = NULL;
	    size_t	count;

	    if(aux_filters){
		/* insert NL conversion filters around remaining aux_filters
		 * so they're not tripped up by local NL convention
		 */
		for(p = aux_filters; p->filter; p++)	/* count aux_filters */
		  ;

		count = (p - aux_filters) + 3;
		p = aux = (FILTLIST_S *) fs_get(count * sizeof(FILTLIST_S));
		memset(p, 0, count * sizeof(FILTLIST_S));
		p->filter = gf_local_nvtnl;
		p++;
		for(; aux_filters->filter; p++, aux_filters++)
		  *p = *aux_filters;

		p->filter = gf_nvtnl_local;
	    }

	    if((status = (*ps_global->tools.display_filter)(display_filter, detach_so, pc, aux)) != NULL){
		snprintf(err_string, sizeof(err_string), "Formatting error: %s", status);
		rv = 0L;
	    }

	    if(aux)
	      fs_give((void **)&aux);
	}
	else{					/*  just copy it, then */
	    gf_io_t	   gc;

	    gf_set_so_readc(&gc, detach_so);
	    so_seek(detach_so, 0L, 0);
	    gf_filter_init();
	    if(aux_filters){
		/* if other filters are involved, correct for
		 * newlines on either side of the pipe...
		 */
		gf_link_filter(gf_local_nvtnl, NULL);
		for( ; aux_filters->filter ; aux_filters++)
		  gf_link_filter(aux_filters->filter, aux_filters->data);

		gf_link_filter(gf_nvtnl_local, NULL);
	    }

	    if((status = gf_pipe(gc, pc)) != NULL){	/* Second pass, sheesh */
		snprintf(err_string, sizeof(err_string), "Formatting error: %s", status);
		rv = 0L;
	    }

	    gf_clear_so_readc(detach_so);
	}

	so_give(&detach_so);			/* blast temp copy */
    }

    if(!ps_global->print && we_cancel)
      cancel_busy_cue(0);

    if (len)
      *len = rv;

    if(df_trigger_list)
      blast_trigger_list(&df_trigger_list);

    return((err_string[0] == '\0') ? NULL : err_string);
}


int
detach_writec(int c)
{
    return(so_writec(c, detach_so));
}


/*
 * build_trigger_list - return possible triggers in a list of triggers 
 *			structs
 */
TRGR_S *
build_trigger_list(void)
{
    TRGR_S  *tp = NULL, **trailp;
    char   **l, *test, *str, *ep, *cmd = NULL;
    int	     i;

    trailp = &tp;
    for(l = ps_global->VAR_DISPLAY_FILTERS ; l && *l; l++){
	get_pair(*l, &test, &cmd, 1, 1);
	if(test && valid_filter_command(&cmd)){
	    *trailp	  = (TRGR_S *) fs_get(sizeof(TRGR_S));
	    (*trailp)->cmp = df_trigger_cmp_text;
	    str		   = test;
	    if(*test == '_' && (i = strlen(test)) > 10
	       && *(ep = &test[i-1]) == '_' && *--ep == ')'){
		if(struncmp(test, "_CHARSET(", 9) == 0){
		    fs_give((void **)&test);
		    fs_give((void **)&cmd);
		    fs_give((void **)trailp);
		    continue;
		}

		if(strncmp(test+1, "LEADING(", 8) == 0){
		    (*trailp)->cmp = df_trigger_cmp_lwsp;
		    *ep		   = '\0';
		    str		   = cpystr(test+9);
		    fs_give((void **)&test);
		}
		else if(strncmp(test+1, "BEGINNING(", 10) == 0){
		    (*trailp)->cmp = df_trigger_cmp_start;
		    *ep		   = '\0';
		    str		   = cpystr(test+11);
		    fs_give((void **)&test);
		}
	    }

	    (*trailp)->text = str;
	    (*trailp)->cmd = cmd;
	    *(trailp = &(*trailp)->next) = NULL;
	}
	else{
	    fs_give((void **)&test);
	    fs_give((void **)&cmd);
	}
    }

    return(tp);
}


/*
 * blast_trigger_list - zot any list of triggers we've been using 
 */
void
blast_trigger_list(TRGR_S **tlist)
{
    if((*tlist)->next)
      blast_trigger_list(&(*tlist)->next);

    fs_give((void **)&(*tlist)->text);
    fs_give((void **)&(*tlist)->cmd);
    fs_give((void **)tlist);
}


/*
 * df_trigger_cmp - compare the line passed us with the list of defined
 *		    display filter triggers
 */
int
df_trigger_cmp(long int n, char *s, LT_INS_S **e, void *l)
{
    register TRGR_S *tp;
    int		     result;

    if(!display_filter)				/* already found? */
      for(tp = df_trigger_list; tp; tp = tp->next)
	if(tp->cmp){
	    if((result = (*tp->cmp)(s, tp->text)) < 0)
	      tp->cmp = NULL;
	    else if(result > 0)
	      return(((display_filter = tp->cmd) != NULL) ? 1 : 0);
	}

    return(0);
}


/*
 * df_trigger_cmp_text - return 1 if s1 is in s2
 */
int
df_trigger_cmp_text(char *s1, char *s2)
{
    return(strstr(s1, s2) != NULL);
}


/*
 * df_trigger_cmp_lwsp - compare the line passed us with the list of defined
 *		         display filter triggers. returns:
 *
 *		    0  if we don't know yet
 *		    1  if we match
 *		   -1  if we clearly don't match
 */
int
df_trigger_cmp_lwsp(char *s1, char *s2)
{
    while(*s1 && isspace((unsigned char)*s1))
      s1++;

    return((*s1) ? (!strncmp(s1, s2, strlen(s2)) ? 1 : -1) : 0);
}


/*
 * df_trigger_cmp_start - return 1 if first strlen(s2) chars start s1
 */
int
df_trigger_cmp_start(char *s1, char *s2)
{
    return(!strncmp(s1, s2, strlen(s2)));
}


/*
 * valid_filter_command - make sure argv[0] of command really exists.
 *		      "cmd" is required to be an alloc'd string since
 *		      it will get realloc'd if the command's path is
 *		      expanded.
 */
int
valid_filter_command(char **cmd)
{
    int  i;
    char cpath[MAXPATH+1], *p;

    if(!(cmd && *cmd))
      return(FALSE);

    /*
     * copy cmd to build expanded path if necessary.
     */
    for(i = 0; i < sizeof(cpath) && (cpath[i] = (*cmd)[i]); i++)
      if(isspace((unsigned char)(*cmd)[i])){
	  cpath[i] = '\0';		/* tie off command's path*/
	  break;
      }

#if	defined(DOS) || defined(OS2)
    if(is_absolute_path(cpath)){
	size_t l;

	fixpath(cpath, sizeof(cpath));
	l = strlen(cpath) + strlen(&(*cmd)[i]);
	p = (char *) fs_get((l+1) * sizeof(char));
	strncpy(p, cpath, l);		/* copy new path */
	p[l] = '\0';
	strncat(p, &(*cmd)[i], l+1-1-strlen(p));		/* and old args */
	p[l] = '\0';
	fs_give((void **) cmd);		/* free it */
	*cmd = p;			/* and assign new buf */
    }
#else
    if(cpath[0] == '~'){
	if(fnexpand(cpath, sizeof(cpath))){
	    size_t l;

	    l = strlen(cpath) + strlen(&(*cmd)[i]);
	    p = (char *) fs_get((l+1) * sizeof(char));
	    strncpy(p, cpath, l);		/* copy new path */
	    p[l] = '\0';
	    strncat(p, &(*cmd)[i], l+1-1-strlen(p));		/* and old args */
	    p[l] = '\0';
	    fs_give((void **) cmd);	/* free it */
	    *cmd = p;			/* and assign new buf */
	}
	else
	  return(FALSE);
    }
#endif

    return(is_absolute_path(cpath) && can_access(cpath, EXECUTE_ACCESS) == 0);
}


void
fetch_readc_init(FETCH_READC_S *frd, MAILSTREAM *stream, long int msgno,
		 char *section, unsigned long size, long partial, long int flags)
{
    int nointr = 0;

    nointr = flags & DT_NOINTR;
    flags &= ~DT_NOINTR;

    memset(g_fr_desc = frd, 0, sizeof(FETCH_READC_S));
    frd->stream  = stream;
    frd->msgno   = msgno;
    frd->section = section;
    frd->flags   = flags;
    frd->size    = size;
    frd->readc	 = fetch_readc;

#ifdef SMIME
    /*
     * The call to imap_cache below will return true in the case where
     * we've already stashed fake data in the content of the part.
     * This happens when an S/MIME message is decrypted.
     */
#endif

    if(modern_imap_stream(stream)
       && !imap_cache(stream, msgno, section, NULL, NULL)
       && (size > INIT_FETCH_CHUNK || (partial > 0L && partial < size))
       && (F_OFF(F_QUELL_PARTIAL_FETCH, ps_global)
           ||
#ifdef	_WINDOWS
	   F_ON(F_QUELL_SSL_LARGEBLOCKS, ps_global)
#else
	   0
#endif
	   )){

	if(partial > 0L && partial < size){
	    /* partial fetch is being asked for */
	    frd->size = partial;
	}

	frd->allocsize = MIN(INIT_FETCH_CHUNK,frd->size);
	frd->chunk = (char *) fs_get ((frd->allocsize + 1) * sizeof(char));
	frd->chunksize = frd->allocsize/2;  /* this gets doubled 1st time */
	frd->endp  = frd->chunk;
	frd->free_me = 1;

	if(!nointr)
	  if(intr_handling_on())
	    frd->we_turned_on = 1;

	if(!(partial > 0L && partial < size)){
	    frd->cache = so_get(CharStar, NULL, EDIT_ACCESS);
	    so_truncate(frd->cache, size);		/* pre-allocate */
	}
    }
    else{				/* fetch the whole bloody thing here */
	frd->chunk = mail_fetch_body(stream, msgno, section, &frd->read, flags);

	/* This only happens if the server gave us a bogus size */
	if(partial > 0L && partial < size){
	    /* partial fetch is being asked for */
	    frd->size = partial;
	    frd->endp  = &frd->chunk[frd->size];
	}
	else if(size != frd->read){
	    dprint((1,
	  "fetch_readc_init: size mismatch: size=%lu read=%lu, continue...\n",
		   frd->size, frd->read));
	    q_status_message(SM_ORDER | SM_DING, 0, 3,
		 _("Message size does not match expected size, continuing..."));
	    frd->size = MIN(size, frd->read);
	    frd->endp  = &frd->chunk[frd->read];
	}
	else
	  frd->endp  = &frd->chunk[frd->read];
    }

    frd->chunkp = frd->chunk;
}


int
fetch_readc_cleanup(void)
{
    if(g_fr_desc){
	if(g_fr_desc->we_turned_on)
	  intr_handling_off();

	if(g_fr_desc->chunk && g_fr_desc->free_me)
	  fs_give((void **) &g_fr_desc->chunk);

	if(g_fr_desc->cache){
	    SIZEDTEXT text;

	    text.size = g_fr_desc->size;
	    text.data = (unsigned char *) so_text(g_fr_desc->cache);
	    imap_cache(g_fr_desc->stream, g_fr_desc->msgno,
		       g_fr_desc->section, NULL, &text);
	    g_fr_desc->cache->txt = (void *) NULL;
	    so_give(&g_fr_desc->cache);
	}
    }

    return(0);
}


char *
fetch_gets(readfn_t f, void *stream, long unsigned int size, GETS_DATA *md)
{
    unsigned long n;

    n		     = MIN(g_fr_desc->chunksize, size);
    g_fr_desc->read += n;
    g_fr_desc->endp  = &g_fr_desc->chunk[n];

    (*f) (stream, n, g_fr_desc->chunkp = g_fr_desc->chunk);

    if(g_fr_desc->cache)
      so_nputs(g_fr_desc->cache, g_fr_desc->chunk, (long) n);

    /* BUG: need to read requested "size" in case it's larger than chunk? */

    return(NULL);
}


int
fetch_readc(unsigned char *c)
{
    extern void gf_error(char *);

    if(ps_global->intr_pending){
	(void) fetch_readc_cleanup();
	/* TRANSLATORS: data transfer was interrupted by something */
	gf_error(g_fr_desc->error ? g_fr_desc->error :_("Transfer interrupted!"));
	/* no return */
    }
    else if(g_fr_desc->chunkp == g_fr_desc->endp){

	/* Anything to read, do it */
	if(g_fr_desc->read < g_fr_desc->size){
	    void *old_gets;
	    int	  rv;
	    TIMEVAL_S before, after;
	    long diff, wdiff;
	    unsigned long save_read;

	    old_gets = mail_parameters(g_fr_desc->stream, GET_GETS,
				       (void *)NULL);
	    mail_parameters(g_fr_desc->stream, SET_GETS, (void *) fetch_gets);

	    /*
	     * Adjust chunksize with the goal that it will be about
	     * TARGET_INTR_TIME useconds +- 20%
	     * to finish the partial fetch. We want that time
	     * to be small so that interrupts will happen fast, but we want
	     * the chunksize large so that the whole fetch will happen
	     * fast. So it's a tradeoff between those two things.
	     *
	     * If the estimated fetchtime is getting too large, we
	     * half the chunksize. If it is small, we double
	     * the chunksize. If it is in between, we leave it. There is
	     * some risk of oscillating between two values, but who cares?
	     */
	    if(g_fr_desc->fetchtime <
				    TARGET_INTR_TIME - TARGET_INTR_TIME/5)
	      g_fr_desc->chunksize *= 2;
	    else if(g_fr_desc->fetchtime >
				    TARGET_INTR_TIME + TARGET_INTR_TIME/5)
	      g_fr_desc->chunksize /= 2;

	    g_fr_desc->chunksize = MIN(MAX_FETCH_CHUNK,
				       MAX(MIN_FETCH_CHUNK,
					   g_fr_desc->chunksize));
	    
#ifdef	_WINDOWS
	    /*
	     * If this feature is set, limit the max size to less than
	     * 16K - 5, the magic number that avoids Microsoft's bug.
	     * Let's just go with 12K instead of 16K - 5.
	     */
	    if(F_ON(F_QUELL_SSL_LARGEBLOCKS, ps_global))
	      g_fr_desc->chunksize =
		  MIN(AVOID_MICROSOFT_SSL_CHUNKING_BUG, g_fr_desc->chunksize);
#endif

	    /* don't ask for more than there should be left to ask for */
	    g_fr_desc->chunksize =
		MIN(g_fr_desc->size - g_fr_desc->read, g_fr_desc->chunksize);

	    /*
	     * If chunksize grew, reallocate chunk.
	     */
	    if(g_fr_desc->chunksize > g_fr_desc->allocsize){
		g_fr_desc->allocsize = g_fr_desc->chunksize;
		fs_give((void **) &g_fr_desc->chunk);
		g_fr_desc->chunk = (char *) fs_get ((g_fr_desc->allocsize + 1)
								* sizeof(char));
		g_fr_desc->endp   = g_fr_desc->chunk;
		g_fr_desc->chunkp = g_fr_desc->chunk;
	    }

	    save_read = g_fr_desc->read;
	    (void)get_time(&before);

	    rv = mail_partial_body(g_fr_desc->stream, g_fr_desc->msgno,
				   g_fr_desc->section, g_fr_desc->read,
				   g_fr_desc->chunksize, g_fr_desc->flags);

	    /*
	     * If the amount we actually read is less than the amount we
	     * asked for we assume that is because the server gave us a
	     * bogus size when we originally asked for it.
	     */
	    if(g_fr_desc->chunksize > (g_fr_desc->read - save_read)){
		dprint((1,
  "partial_body returned less than asked for: asked=%lu got=%lu, continue...\n",
		       g_fr_desc->chunksize, g_fr_desc->read - save_read));
		if(g_fr_desc->read - save_read > 0)
		  q_status_message(SM_ORDER | SM_DING, 0, 3,
		   _("Message size does not match expected size, continuing..."));
		else{
		    rv = 0;
		    q_status_message(SM_ORDER | SM_DING, 3, 3,
		     _("Server returns zero bytes, Quell-Partial-Fetch feature may help"));
		}

		g_fr_desc->size = g_fr_desc->read;
	    }

	    if(get_time(&after) == 0){
		diff = time_diff(&after, &before);
		wdiff = MIN(TARGET_INTR_TIME + TARGET_INTR_TIME/2,
			    MAX(TARGET_INTR_TIME - TARGET_INTR_TIME/2, diff));
		/*
		 * Fetchtime is an exponentially weighted average of the number
		 * of usecs it takes to do a single fetch of whatever the
		 * current chunksize is. Since the fetch time probably isn't
		 * simply proportional to the chunksize, we don't try to
		 * calculate a chunksize by keeping track of the bytes per
		 * second. Instead, we just double or half the chunksize if
		 * we are too fast or too slow. That happens the next time
		 * through the loop a few lines up.
		 * Too settle it down a bit, Windsorize the mean.
		 */
		g_fr_desc->fetchtime = (g_fr_desc->fetchtime == 0)
					? wdiff
					: g_fr_desc->fetchtime/2 + wdiff/2;
		dprint((8,
	         "fetch: diff=%ld wdiff=%ld fetchave=%ld prev chunksize=%ld\n",
	         diff, wdiff, g_fr_desc->fetchtime, g_fr_desc->chunksize));
	    }
	    else  /* just set it so it won't affect anything */
	      g_fr_desc->fetchtime = TARGET_INTR_TIME;

	    /* UNinstall mailgets */
	    mail_parameters(g_fr_desc->stream, SET_GETS, old_gets);

	    if(!rv){
		(void) fetch_readc_cleanup();
		gf_error("Partial fetch failed!");
		/* no return */
	    }
	}
	else				/* clean up and return done. */
	  return(fetch_readc_cleanup());
    }

    *c = *g_fr_desc->chunkp++;

    return(1);
}
