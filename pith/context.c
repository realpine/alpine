#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: context.c 1144 2008-08-14 16:53:34Z hubert@u.washington.edu $";
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

#include "../pith/headers.h"
#include "../pith/conf.h"
#include "../pith/context.h"
#include "../pith/state.h"
#include "../pith/status.h"
#include "../pith/stream.h"
#include "../pith/folder.h"
#include "../pith/util.h"
#include "../pith/tempfile.h"


/*
 * Internal prototypes
 */
char    *context_percent_quote(char *);


/* Context Manager context format digester
 * Accepts: context string and buffers for sprintf-suitable context,
 *          remote host (for display), remote context (for display), 
 *          and view string
 * Returns: NULL if successful, else error string
 *
 * Comments: OK, here's what we expect a fully qualified context to 
 *           look like:
 *
 * [*] [{host ["/"<proto>] [:port]}] [<cntxt>] "[" [<view>] "]" [<cntxt>]
 *
 *         2) It's understood that double "[" or "]" are used to 
 *            quote a literal '[' or ']' in a context name.
 *
 *         3) an empty view in context implies a view of '*', so that's 
 *            what get's put in the view string
 *
 *   The 2nd,3rd,4th, and 5th args all have length at least len.
 *
 */
char *
context_digest(char *context, char *scontext, char *host, char *rcontext,
	       char *view, size_t len)
{
    char *p, *viewp = view;
    char *scontextp = scontext;
    int   i = 0;

    if((p = context) == NULL || *p == '\0'){
	if(scontext)			/* so the caller can apply */
	  strncpy(scontext, "%s", len);	/* folder names as is.     */

	scontext[len-1] = '\0';
	return(NULL);			/* no error, just empty context */
    }

    /* find hostname if requested and exists */
    if(*p == '{' || (*p == '*' && *++p == '{')){
	for(p++; *p && *p != '}' ; p++)
	  if(host && p-context < len-1)
	    *host++ = *p;		/* copy host if requested */

	while(*p && *p != '}')		/* find end of imap host */
	  p++;

	if(*p == '\0')
	  return("Unbalanced '}'");	/* bogus. */
	else
	  p++;				/* move into next field */
    }

    for(; *p ; p++){			/* get thru context */
	if(rcontext && i < len-1)
	  rcontext[i] = *p;		/* copy if requested */
	
	i++;

	if(*p == '['){			/* done? */
	    if(*++p == '\0')		/* look ahead */
	      return("Unbalanced '['");

	    if(*p != '[')		/* not quoted: "[[" */
	      break;
	}
	else if(*p == ']' && *(p+1) == ']')	/* these may be quoted, too */
	  p++;
    }

    if(*p == '\0')
      return("No '[' in context");

    for(; *p ; p++){			/* possibly in view portion ? */
	if(*p == ']'){
	    if(*(p+1) == ']')		/* is quoted */
	      p++;
	    else
	      break;
	}

	if(viewp && viewp-view < len-1)
	  *viewp++ = *p;
    }

    if(*p != ']')
      return("No ']' in context");

    for(; *p ; p++){			/* trailing context ? */
	if(rcontext && i < len-1)
	  rcontext[i] = *p;
	i++;
    }

    if(host) *host = '\0';
    if(rcontext && i < len) rcontext[i] = '\0';
    if(viewp) {
/* MAIL_LIST: dealt with it in new_context since could be either '%' or '*'
	if(viewp == view && viewp-view < len-1)
	  *viewp++ = '*';
*/

	*viewp = '\0';
    }

    if(scontextp){			/* sprint'able context request ? */
	if(*context == '*'){
	    if(scontextp-scontext < len-1)
	      *scontextp++ = *context;

	    context++;
	}

	if(*context == '{'){
	    while(*context && *context != '}'){
		if(scontextp-scontext < len-1)
		  *scontextp++ = *context;
		
		context++;
	    }

	    *scontextp++ = '}';
	}

	for(p = rcontext; *p ; p++){
	    if(*p == '[' && *(p+1) == ']'){
		if(scontextp-scontext < len-2){
		    *scontextp++ = '%';	/* replace "[]" with "%s" */
		    *scontextp++ = 's';
		}

		p++;			/* skip ']' */
	    }
	    else if(scontextp-scontext < len-1)
	      *scontextp++ = *p;
	}

	*scontextp = '\0';
    }

    return(NULL);			/* no problems to report... */
}


/* Context Manager apply name to context
 * Accepts: buffer to write, context to apply, ambiguous folder name
 * Returns: buffer filled with fully qualified name in context
 *          No context applied if error
 */
char *
context_apply(char *b, CONTEXT_S *c, char *name, size_t len)
{
    if(!c || IS_REMOTE(name) ||
       (!IS_REMOTE(c->context) && is_absolute_path(name))){
	strncpy(b, name, len-1);			/* no context! */
    }
    else if(name[0] == '#'){
	if(IS_REMOTE(c->context)){
	    char *p = strchr(c->context, '}');	/* name specifies namespace */
	    snprintf(b, len, "%.*s", MIN(p - c->context + 1, len-1), c->context);
	    b[MIN(p - c->context + 1, len-1)] = '\0';
	    snprintf(b+strlen(b), len-strlen(b), "%.*s", len-1-strlen(b), name);
	}
	else{
	    strncpy(b, name, len-1);
	}
    }
    else if(c->dir && c->dir->ref){		/* has reference string! */
	snprintf(b, len, "%.*s", len-1, c->dir->ref);
	b[len-1] = '\0';
	snprintf(b+strlen(b), len-strlen(b), "%.*s", len-1-strlen(b), name);
    }
    else{					/* no ref, apply to context */
	char *pq = NULL;

	/*
	 * Have to quote %s for the sprintf because we're using context
	 * as a format string.
	 */
	pq = context_percent_quote(c->context);

	if(strlen(c->context) + strlen(name) < len)
	  snprintf(b, len, pq, name);
	else{
	    char *t;
	    size_t l;

	    l = strlen(pq)+strlen(name);
	    t = (char *) fs_get((l+1) * sizeof(char));
	    snprintf(t, l+1, pq, name);
	    strncpy(b, t, len-1);
	    fs_give((void **)&t);
	}

	if(pq)
	  fs_give((void **) &pq);
    }

    b[len-1] = '\0';
    return(b);
}


/*
 * Insert % before existing %'s so printf will print a real %.
 * This is a special routine just for contexts. It only does the % stuffing
 * for %'s inside of the braces of the context name, not for %'s to
 * the right of the braces, which we will be using for printf format strings.
 * Returns a malloced string which the caller is responsible for.
 */
char *
context_percent_quote(char *context)
{
    char *pq = NULL;

    if(!context || !*context)
      pq = cpystr("");
    else{
	if(IS_REMOTE(context)){
	    char *end, *p, *q;

	    /* don't worry about size efficiency, just allocate double */
	    pq = (char *) fs_get((2*strlen(context) + 1) * sizeof(char));

	    end = strchr(context, '}');
	    p = context;
	    q = pq;
	    while(*p){
		if(*p == '%' && p < end)
		  *q++ = '%';
		
		*q++ = *p++;
	    }

	    *q = '\0';
	}
	else
	  pq = cpystr(context);
    }

    return(pq);
}


/* Context Manager check if name is ambiguous
 * Accepts: candidate string
 * Returns: T if ambiguous, NIL if fully-qualified
 */

int
context_isambig (char *s)
{
    return(!(*s == '{' || *s == '#'));
}


/*----------------------------------------------------------------------
    Check to see if user is allowed to read or write this folder.

   Args:  s  -- the name to check
 
 Result: Returns 1 if OK
         Returns 0 and posts an error message if access is denied
  ----*/
int
context_allowed(char *s)
{
    struct variable *vars = ps_global ? ps_global->vars : NULL;
    int retval = 1;
    MAILSTREAM stream; /* fake stream for error message in mm_notify */

    if(ps_global
       && ps_global->restricted
       && (strindex("./~", s[0]) || srchstr(s, "/../"))){
	stream.mailbox = s;
	mm_notify(&stream, "Restricted mode doesn't allow operation", WARN);
	retval = 0;
    }
    else if(vars && VAR_OPER_DIR
	    && s[0] != '{' && !(s[0] == '*' && s[1] == '{')
	    && strucmp(s,ps_global->inbox_name) != 0
	    && strcmp(s, ps_global->VAR_INBOX_PATH) != 0){
	char *p, *free_this = NULL;

	p = s;
	if(strindex(s, '~')){
	    p = strindex(s, '~');
	    free_this = (char *)fs_get(strlen(p) + 200);
	    strncpy(free_this, p, strlen(p)+200);
	    fnexpand(free_this, strlen(p)+200);
	    p = free_this;
	}
	else if(p[0] != '/'){  /* add home dir to relative paths */
	    free_this = p = (char *)fs_get(strlen(s)
					    + strlen(ps_global->home_dir) + 2);
	    build_path(p, ps_global->home_dir, s,
		       strlen(s)+strlen(ps_global->home_dir)+2);
	}
	
	if(!in_dir(VAR_OPER_DIR, p)){
	    char err[200];

	    /* TRANSLATORS: User is restricted to operating within a certain directory */
	    snprintf(err, sizeof(err), _("Not allowed outside of %.150s"), VAR_OPER_DIR);
	    stream.mailbox = p;
	    mm_notify(&stream, err, WARN);
	    retval = 0;
	}
	else if(srchstr(p, "/../")){  /* check for .. in path */
	    stream.mailbox = p;
	    mm_notify(&stream, "\"..\" not allowed in name", WARN);
	    retval = 0;
	}

	if(free_this)
	  fs_give((void **)&free_this);
    }
    
    return retval;
}



/* Context Manager create mailbox
 * Accepts: context
 *	    mail stream
 *	    mailbox name to create
 * Returns: T on success, NIL on failure
 */

long
context_create (CONTEXT_S *context, MAILSTREAM *stream, char *mailbox)
{
  char tmp[MAILTMPLEN];		/* must be within context */

  return(context_allowed(context_apply(tmp, context, mailbox, sizeof(tmp)))
	 ? pine_mail_create(stream,tmp) : 0L);
}


/* Context Manager open
 * Accepts: context
 *	    candidate stream for recycling
 *	    mailbox name
 *	    open options
 * Returns: stream to use on success, NIL on failure
 */

MAILSTREAM *
context_open (CONTEXT_S *context, MAILSTREAM *old, char *name, long int opt, long int *retflags)
{
  char tmp[MAILTMPLEN];		/* build FQN from ambiguous name */

  if(!context_allowed(context_apply(tmp, context, name, sizeof(tmp)))){
      /*
       * If a stream is passed to context_open, we will either re-use
       * it or close it.
       */
      if(old)
	pine_mail_close(old);

      return((MAILSTREAM *)NULL);
  }

  return(pine_mail_open(old, tmp, opt, retflags));
}


/* Context Manager status
 * Accepts: context
 *	    candidate stream for recycling
 *	    mailbox name
 *	    open options
 * Returns: T if call succeeds, NIL on failure
 */

long
context_status(CONTEXT_S *context, MAILSTREAM *stream, char *name, long int opt)
{
    return(context_status_full(context, stream, name, opt, NULL, NULL));
}


long
context_status_full(CONTEXT_S *context, MAILSTREAM *stream, char *name,
		    long int opt, imapuid_t *uidvalidity, imapuid_t *uidnext)
{
    char tmp[MAILTMPLEN];	/* build FQN from ambiguous name */
    long flags = opt;

    return(context_allowed(context_apply(tmp, context, name, sizeof(tmp)))
	    ? pine_mail_status_full(stream,tmp,flags,uidvalidity,uidnext) : 0L);
}


/* Context Manager status
 *
 * This is very similar to context_status. Instead of a stream pointer we
 * receive a pointer to a pointer so that we can return a stream that we
 * opened for further use by the caller.
 *
 * Accepts: context
 *	    candidate stream for recycling
 *	    mailbox name
 *	    open options
 * Returns: T if call succeeds, NIL on failure
 */

long
context_status_streamp(CONTEXT_S *context, MAILSTREAM **streamp, char *name, long int opt)
{
    return(context_status_streamp_full(context,streamp,name,opt,NULL,NULL));
}


long
context_status_streamp_full(CONTEXT_S *context, MAILSTREAM **streamp, char *name,
			    long int opt, imapuid_t *uidvalidity, imapuid_t *uidnext)
{
    MAILSTREAM *stream;
    char        tmp[MAILTMPLEN];	/* build FQN from ambiguous name */

    if(!context_allowed(context_apply(tmp, context, name, sizeof(tmp))))
      return(0L);

    if(!streamp)
      stream = NULL;
    else{
	if(!*streamp && IS_REMOTE(tmp)){
	    *streamp = pine_mail_open(NULL, tmp,
				      OP_SILENT|OP_HALFOPEN|SP_USEPOOL, NULL);
	}

	stream = *streamp;
    }

    return(pine_mail_status_full(stream, tmp, opt, uidvalidity, uidnext));
}


/* Context Manager rename
 * Accepts: context
 *	    mail stream
 *	    old mailbox name
 *	    new mailbox name
 * Returns: T on success, NIL on failure
 */

long
context_rename (CONTEXT_S *context, MAILSTREAM *stream, char *old, char *new)
{
    char tmp[MAILTMPLEN],tmp2[MAILTMPLEN];

    return((context_allowed(context_apply(tmp, context, old, sizeof(tmp)))
	    && context_allowed(context_apply(tmp2, context, new, sizeof(tmp2))))
	     ? pine_mail_rename(stream,tmp,tmp2) : 0L);
}


MAILSTREAM *
context_already_open_stream(CONTEXT_S *context, char *name, int flags)
{
    char tmp[MAILTMPLEN];

    return(already_open_stream(context_apply(tmp, context, name, sizeof(tmp)),
			       flags));
}


/* Context Manager delete mailbox
 * Accepts: context
 *	    mail stream
 *	    mailbox name to delete
 * Returns: T on success, NIL on failure
 */

long
context_delete (CONTEXT_S *context, MAILSTREAM *stream, char *name)
{
    char tmp[MAILTMPLEN];		/* build FQN from ambiguous name */

    return(context_allowed(context_apply(tmp, context, name, sizeof(tmp)))
	     ? pine_mail_delete(stream, tmp) : 0L);
}


/* Context Manager append message string
 * Accepts: context
 *	    mail stream
 *	    destination mailbox
 *	    stringstruct of message to append
 * Returns: T on success, NIL on failure
 */

long
context_append (CONTEXT_S *context, MAILSTREAM *stream, char *name, STRING *msg)
{
    char tmp[MAILTMPLEN];	/* build FQN from ambiguous name */

    return(context_allowed(context_apply(tmp, context, name, sizeof(tmp)))
	     ? pine_mail_append(stream, tmp, msg) : 0L);
}


/* Context Manager append message string with flags
 * Accepts: context
 *	    mail stream
 *	    destination mailbox
 *          flags to assign message being appended
 *          date of message being appended
 *	    stringstruct of message to append
 * Returns: T on success, NIL on failure
 */

long
context_append_full(CONTEXT_S *context, MAILSTREAM *stream, char *name,
		    char *flags, char *date, STRING *msg)
{
    char tmp[MAILTMPLEN];		/* build FQN from ambiguous name */

    return(context_allowed(context_apply(tmp, context, name, sizeof(tmp)))
	     ? pine_mail_append_full(stream, tmp, flags, date, msg) : 0L);
}


/* Context Manager append multiple message
 * Accepts: context
 *	    mail stream
 *	    destination mailbox
 *	    append data callback
 *	    arbitrary ata for callback use
 * Returns: T on success, NIL on failure
 */

long
context_append_multiple(CONTEXT_S *context, MAILSTREAM *stream, char *name,
			append_t af, APPENDPACKAGE *data, MAILSTREAM *not_this_stream)
{
    char tmp[MAILTMPLEN];		/* build FQN from ambiguous name */

    return(context_allowed(context_apply(tmp, context, name, sizeof(tmp)))
     ? pine_mail_append_multiple(stream, tmp, af, data, not_this_stream) : 0L);
}


/* Mail copy message(s)
 * Accepts: context
 *	    mail stream
 *	    sequence
 *	    destination mailbox
 */

long
context_copy (CONTEXT_S *context, MAILSTREAM *stream, char *sequence, char *name)
{
    char *s, tmp[MAILTMPLEN];		/* build FQN from ambiguous name */

    tmp[0] = '\0';

    if(context_apply(tmp, context, name, sizeof(tmp))[0] == '{'){
	if((s = strindex(tmp, '}')) != NULL)
	  s++;
	else
	  return(0L);
    }
    else
      s = tmp;

    tmp[sizeof(tmp)-1] = '\0';

    if(!*s)
      strncpy(s = tmp, "INBOX", sizeof(tmp));		/* presume "inbox" ala c-client */

    tmp[sizeof(tmp)-1] = '\0';

    return(context_allowed(s) ? pine_mail_copy(stream, sequence, s) : 0L);
}


/*
 * Context manager stream usefulness test
 * Accepts: context
 *	    mail name
 *	    mailbox name
 *	    mail stream to test against mailbox name
 * Returns: stream if useful, else NIL
 */
MAILSTREAM *
context_same_stream(CONTEXT_S *context, char *name, MAILSTREAM *stream)
{
    extern MAILSTREAM *same_stream(char *name, MAILSTREAM *stream);
    char tmp[MAILTMPLEN];		/* build FQN from ambiguous name */

    return(same_stream(context_apply(tmp, context, name, sizeof(tmp)), stream));
}


/*
 * new_context - creates and fills in a new context structure, leaving
 *               blank the actual folder list (to be filled in later as
 *               needed).  Also, parses the context string provided 
 *               picking out any user defined label.  Context lines are
 *               of the form:
 *
 *               [ ["] <string> ["] <white-space>] <context>
 *
 */
CONTEXT_S *
new_context(char *cntxt_string, int *prime)
{
    CONTEXT_S  *c;
    char        host[MAXPATH], rcontext[MAXPATH],
		view[MAXPATH], dcontext[MAXPATH],
	       *nickname = NULL, *c_string = NULL, *p;

    /*
     * do any context string parsing (like splitting user-supplied 
     * label from actual context)...
     */
    get_pair(cntxt_string, &nickname, &c_string, 0, 0);

    if(update_bboard_spec(c_string, tmp_20k_buf, SIZEOF_20KBUF)){
	fs_give((void **) &c_string);
	c_string = cpystr(tmp_20k_buf);
    }

    if(c_string && *c_string == '\"')
      (void)removing_double_quotes(c_string);

    view[0] = rcontext[0] = host[0] = dcontext[0] = '\0';
    if((p = context_digest(c_string, dcontext, host, rcontext, view, MAXPATH)) != NULL){
	q_status_message2(SM_ORDER | SM_DING, 3, 4,
			  "Bad context, %.200s : %.200s", p, c_string);
	fs_give((void **) &c_string);
	if(nickname)
	  fs_give((void **)&nickname);

	return(NULL);
    }
    else
      fs_give((void **) &c_string);

    c = (CONTEXT_S *) fs_get(sizeof(CONTEXT_S)); /* get new context   */
    memset((void *) c, 0, sizeof(CONTEXT_S));	/* and initialize it */
    if(*host)
      c->server  = cpystr(host);		/* server + params */

    c->context = cpystr(dcontext);

    if(strstr(c->context, "#news."))
      c->use |= CNTXT_NEWS;

    c->dir = new_fdir(NULL, view, (c->use & CNTXT_NEWS) ? '*' : '%');

    /* fix up nickname */
    if(!(c->nickname = nickname)){			/* make one up! */
	snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%s%s%.100s",
		(c->use & CNTXT_NEWS)
		  ? "News"
		  : (c->dir->ref)
		      ? (c->dir->ref) :"Mail",
		(c->server) ? " on " : "",
		(c->server) ? c->server : "");
	c->nickname = cpystr(tmp_20k_buf);
    }

    if(prime && !*prime){
	*prime = 1;
	c->use |= CNTXT_SAVEDFLT;
    }

    /* fix up label */
    if(NEWS_TEST(c)){
	snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%sews groups%s%.100s",
		(*host) ? "N" : "Local n", (*host) ? " on " : "",
		(*host) ? host : "");
    }
    else{
	p = srchstr(rcontext, "[]");
	snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%solders%s%.100s in %.*s%s",
		(*host) ? "F" : "Local f", (*host) ? " on " : "",
		(*host) ? host : "",
		p ? MIN(p - rcontext, 100) : 0,
		rcontext, (p && (p - rcontext) > 0) ? "" : "home directory");
    }

    c->label = cpystr(tmp_20k_buf);

    dprint((5, "Context: %s serv:%s ref: %s view: %s\n",
	       c->context ? c->context : "?",
	       (c->server) ? c->server : "\"\"",
	       (c->dir->ref) ? c->dir->ref : "\"\"",
	       (c->dir->view.user) ? c->dir->view.user : "\"\""));

    return(c);
}


/*
 *  Release resources associated with global context list
 */
void
free_contexts(CONTEXT_S **ctxt)
{
    if(ctxt && *ctxt){
	free_contexts(&(*ctxt)->next);
	free_context(ctxt);
    }
}


/*
 *  Release resources associated with the given context structure
 */
void
free_context(CONTEXT_S **cntxt)
{
  if(cntxt && *cntxt){
    if(*cntxt == ps_global->context_current)
      ps_global->context_current = NULL;

    if(*cntxt == ps_global->context_last)
      ps_global->context_last = NULL;

    if(*cntxt == ps_global->last_save_context)
      ps_global->last_save_context = NULL;

    if((*cntxt)->context)
      fs_give((void **) &(*cntxt)->context);

    if((*cntxt)->server)
      fs_give((void **) &(*cntxt)->server);

    if((*cntxt)->nickname)
      fs_give((void **)&(*cntxt)->nickname);

    if((*cntxt)->label)
      fs_give((void **) &(*cntxt)->label);

    if((*cntxt)->comment)
      fs_give((void **) &(*cntxt)->comment);

    if((*cntxt)->selected.reference)
      fs_give((void **) &(*cntxt)->selected.reference);

    if((*cntxt)->selected.sub)
      free_selected(&(*cntxt)->selected.sub);

    free_strlist(&(*cntxt)->selected.folders);

    free_fdir(&(*cntxt)->dir, 1);

    fs_give((void **)cntxt);
  }
}
