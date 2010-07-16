#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: folder.c 1142 2008-08-13 17:22:21Z hubert@u.washington.edu $";
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
#include "../pith/folder.h"
#include "../pith/state.h"
#include "../pith/context.h"
#include "../pith/init.h"
#include "../pith/conf.h"
#include "../pith/stream.h"
#include "../pith/imap.h"
#include "../pith/util.h"
#include "../pith/flag.h"
#include "../pith/status.h"
#include "../pith/busy.h"
#include "../pith/mailindx.h"


typedef struct _build_folder_list_data {
    long	 mask;			/* bitmap of responses to ignore    */
    LISTARGS_S	 args;
    LISTRES_S	 response;
    int          is_move_folder;
    FLIST	*list;
} BFL_DATA_S;


#define FCHUNK  64


/*
 * Internal prototypes
 */
void	    mail_list_exists(MAILSTREAM *, char *, int, long, void *, unsigned);
void        init_incoming_folder_list(struct pine *, CONTEXT_S *);
void	    mail_list_filter(MAILSTREAM *, char *, int, long, void *, unsigned);
void	    mail_lsub_filter(MAILSTREAM *, char *, int, long, void *, unsigned);
int	    mail_list_in_collection(char **, char *, char *, char *);
char	   *folder_last_cmpnt(char *, int);
void        free_folder_entries(FLIST **);
int	    folder_insert_sorted(int, int, int, FOLDER_S *, FLIST *,
				 int (*)(FOLDER_S *, FOLDER_S *));
void        folder_insert_index(FOLDER_S *, int, FLIST *);
void        resort_folder_list(FLIST *flist);
int         compare_folders_alpha_qsort(const qsort_t *a1, const qsort_t *a2);
int         compare_folders_dir_alpha_qsort(const qsort_t *a1, const qsort_t *a2);
int         compare_folders_alpha_dir_qsort(const qsort_t *a1, const qsort_t *a2);
int         compare_names(const qsort_t *, const qsort_t *);
void        init_incoming_unseen_data(struct pine *, FOLDER_S *f);


char *
folder_lister_desc(CONTEXT_S *cntxt, FDIR_S *fdp)
{
    char *p;

    /* Provide context in new collection header */
    snprintf(tmp_20k_buf, SIZEOF_20KBUF, "Dir: %s",
	    ((p = strstr(cntxt->context, "%s")) && !*(p+2)
	     && !strncmp(fdp->ref, cntxt->context, p - cntxt->context))
	      ? fdp->ref + (p - cntxt->context) : fdp->ref);
    return(cpystr(tmp_20k_buf));
}


void
reset_context_folders(CONTEXT_S *cntxt)
{
    CONTEXT_S *tc;

    for(tc = cntxt; tc && tc->prev ; tc = tc->prev)
      ;				/* start at beginning */

    for( ; tc ; tc = tc->next){
	free_folder_list(tc);

	while(tc->dir->prev){
	    FDIR_S *tp = tc->dir->prev;
	    free_fdir(&tc->dir, 0);
	    tc->dir = tp;
	}
    }
}


/*
 * next_folder_dir - return a directory structure with the folders it
 *		     contains
 */
FDIR_S *
next_folder_dir(CONTEXT_S *context, char *new_dir, int build_list, MAILSTREAM **streamp)
{
    char      tmp[MAILTMPLEN], dir[3];
    FDIR_S   *tmp_fp, *fp;

    fp = (FDIR_S *) fs_get(sizeof(FDIR_S));
    memset(fp, 0, sizeof(FDIR_S));
    (void) context_apply(tmp, context, new_dir, MAILTMPLEN);
    dir[0] = context->dir->delim;
    dir[1] = '\0';
    strncat(tmp, dir, sizeof(tmp)-1-strlen(tmp));
    fp->ref	      = cpystr(tmp);
    fp->delim	      = context->dir->delim;
    fp->view.internal = cpystr(NEWS_TEST(context) ? "*" : "%");
    fp->folders	      = init_folder_entries();
    fp->status	      = CNTXT_NOFIND;
    tmp_fp	      = context->dir;	/* temporarily rebind */
    context->dir      = fp;

    if(build_list)
      build_folder_list(streamp, context, NULL, NULL,
			NEWS_TEST(context) ? BFL_LSUB : BFL_NONE);

    context->dir = tmp_fp;
    return(fp);
}


/*
 * Return which pinerc incoming folder #index is in.
 */
EditWhich
config_containing_inc_fldr(FOLDER_S *folder)
{
    char    **t;
    int       i, keep_going = 1, inheriting = 0;
    struct variable *v = &ps_global->vars[V_INCOMING_FOLDERS];

    if(v->is_fixed)
      return(None);

    /* is it in exceptions config? */
    if(v->post_user_val.l){
	for(i = 0, t=v->post_user_val.l; t[i]; i++){
	    if(expand_variables(tmp_20k_buf, SIZEOF_20KBUF, t[i], 0)){
		keep_going = 0;

		if(!strcmp(tmp_20k_buf, INHERIT))
		  inheriting = 1;
		else if(folder->varhash == line_hash(tmp_20k_buf))
		  return(Post);
	    }
	}
    }

    if(inheriting)
      keep_going = 1;

    /* is it in main config? */
    if(keep_going && v->main_user_val.l){
	for(i = 0, t=v->main_user_val.l; t[i]; i++){
	    if(expand_variables(tmp_20k_buf, SIZEOF_20KBUF, t[i], 0) &&
	       folder->varhash == line_hash(tmp_20k_buf))
	      return(Main);
	}
    }

    return(None);
}


/*----------------------------------------------------------------------
      Format the given folder name for display for the user

   Args: folder -- The folder name to fix up

Not sure this always makes it prettier. It could do nice truncation if we
passed in a length. Right now it adds the path name of the mail 
subdirectory if appropriate.
 ----*/
      
char *
pretty_fn(char *folder)
{
    if(!strucmp(folder, ps_global->inbox_name))
      return(ps_global->inbox_name);
    else
      return(folder);
}


/*----------------------------------------------------------------------
  Return the path delimiter for the given folder on the given server

  Args: folder -- folder type for delimiter

    ----*/
int
get_folder_delimiter(char *folder)
{
    MM_LIST_S    ldata;
    LISTRES_S	 response;
    int          ourstream = 0;

    memset(mm_list_info = &ldata, 0, sizeof(MM_LIST_S));
    ldata.filter = mail_list_response;
    memset(ldata.data = &response, 0, sizeof(LISTRES_S));

    if(*folder == '{'
       && !(ldata.stream = sp_stream_get(folder, SP_MATCH))
       && !(ldata.stream = sp_stream_get(folder, SP_SAME))){
	if((ldata.stream = pine_mail_open(NULL,folder,
				 OP_HALFOPEN|OP_SILENT|SP_USEPOOL|SP_TEMPUSE,
					 NULL)) != NULL){
	    ourstream++;
	}
	else{
	    return(FEX_ERROR);
	}
    }

    pine_mail_list(ldata.stream, folder, "", NULL);

    if(ourstream)
      pine_mail_close(ldata.stream);

    return(response.delim);
}


/*----------------------------------------------------------------------
       Check to see if folder exists in given context

  Args: cntxt -- context inwhich to interpret "file" arg
        file  -- name of folder to check
 
 Result: returns FEX_ISFILE if the folder exists and is a folder
		 FEX_ISDIR  if the folder exists and is a directory
                 FEX_NOENT  if it doesn't exist
		 FEX_ERROR  on error

  The two existence return values above may be logically OR'd

  Uses mail_list to sniff out the existence of the requested folder.
  The context string is just here for convenience.  Checking for
  folder's existence within a given context is probably more efficiently
  handled outside this function for now using build_folder_list().

    ----*/
int
folder_exists(CONTEXT_S *cntxt, char *file)
{
    return(folder_name_exists(cntxt, file, NULL));
}


/*----------------------------------------------------------------------
       Check to see if folder exists in given context

  Args: cntxt -- context in which to interpret "file" arg
        file  -- name of folder to check
	name  -- name of folder folder with context applied
 
 Result: returns FEX_ISFILE if the folder exists and is a folder
		 FEX_ISDIR  if the folder exists and is a directory
                 FEX_NOENT  if it doesn't exist
		 FEX_ERROR  on error

  The two existence return values above may be logically OR'd

  Uses mail_list to sniff out the existence of the requested folder.
  The context string is just here for convenience.  Checking for
  folder's existence within a given context is probably more efficiently
  handled outside this function for now using build_folder_list().

    ----*/
int
folder_name_exists(CONTEXT_S *cntxt, char *file, char **fullpath)
{
    MM_LIST_S    ldata;
    EXISTDATA_S	 parms;
    int          we_cancel = 0, res;
    char	*p, reference[MAILTMPLEN], tmp[MAILTMPLEN], *tfolder = NULL;

    /*
     * No folder means "inbox".
     */
    if(*file == '{' && (p = strchr(file, '}')) && (!*(p+1))){
	size_t l;

	l = strlen(file)+strlen("inbox");
	tfolder = (char *) fs_get((l+1) * sizeof(char));
	snprintf(tfolder, l+1, "%s%s", file, "inbox");
	file = tfolder;
	cntxt = NULL;
    }

    mm_list_info = &ldata;		/* tie down global reference */
    memset(&ldata, 0, sizeof(ldata));
    ldata.filter = mail_list_exists;

    ldata.stream = sp_stream_get(context_apply(tmp, cntxt, file, sizeof(tmp)),
				 SP_SAME);

    memset(ldata.data = &parms, 0, sizeof(EXISTDATA_S));

    /*
     * If no preset reference string, must be at top of context
     */
    if(cntxt && context_isambig(file)){
	/* inbox in first context is the real inbox */
	if(ps_global->context_list == cntxt && !strucmp(file, ps_global->inbox_name)){
	    reference[0] = '\0';
	    parms.args.reference = reference;
	}
	else if(!(parms.args.reference = cntxt->dir->ref)){
	    char *p;

	    if((p = strstr(cntxt->context, "%s")) != NULL){
		strncpy(parms.args.reference = reference,
			cntxt->context,
			MIN(p - cntxt->context, sizeof(reference)-1));
		reference[MIN(p - cntxt->context, sizeof(reference)-1)] = '\0';
		if(*(p += 2))
		  parms.args.tail = p;
	    }
	    else
	      parms.args.reference = cntxt->context;
	}

	parms.fullname = fullpath;
    }

    ps_global->mm_log_error = 0;
    ps_global->noshow_error = 1;

    we_cancel = busy_cue(NULL, NULL, 1);

    parms.args.name = file;

    res = pine_mail_list(ldata.stream, parms.args.reference, parms.args.name,
			 &ldata.options);

    if(we_cancel)
      cancel_busy_cue(-1);

    ps_global->noshow_error = 0;

    if(cntxt && cntxt->dir && parms.response.delim)
      cntxt->dir->delim = parms.response.delim;

    if(tfolder)
      fs_give((void **)&tfolder);
    return(((res == FALSE) || ps_global->mm_log_error)
	    ? FEX_ERROR
	    : (((parms.response.isfile)
		  ? FEX_ISFILE : 0 )
	       | ((parms.response.isdir)
		  ? FEX_ISDIR : 0)
	       | ((parms.response.ismarked)
		  ? FEX_ISMARKED : 0)
	       | ((parms.response.unmarked)
		  ? FEX_UNMARKED : 0)));
}


void
mail_list_exists(MAILSTREAM *stream, char *mailbox, int delim, long int attribs,
		 void *data, unsigned int options)
{
    if(delim)
      ((EXISTDATA_S *) data)->response.delim = delim;

    if(mailbox && *mailbox){
	if(!(attribs & LATT_NOSELECT)){
	    ((EXISTDATA_S *) data)->response.isfile  = 1;
	    ((EXISTDATA_S *) data)->response.count  += 1;
	}

	if(!(attribs & LATT_NOINFERIORS)){
	    ((EXISTDATA_S *) data)->response.isdir  = 1;
	    ((EXISTDATA_S *) data)->response.count  += 1;
	}
    
	if(attribs & LATT_MARKED)
	  ((EXISTDATA_S *) data)->response.ismarked = 1;

	/* don't mark #move folders unmarked */
	if(attribs & LATT_UNMARKED && !(options & PML_IS_MOVE_MBOX))
	  ((EXISTDATA_S *) data)->response.unmarked = 1;

	if(attribs & LATT_HASCHILDREN)
	  ((EXISTDATA_S *) data)->response.haschildren = 1;

	if(attribs & LATT_HASNOCHILDREN)
	  ((EXISTDATA_S *) data)->response.hasnochildren = 1;

	if(((EXISTDATA_S *) data)->fullname
	   && ((((EXISTDATA_S *) data)->args.reference[0] != '\0')
	         ? struncmp(((EXISTDATA_S *) data)->args.reference, mailbox,
			    strlen(((EXISTDATA_S *)data)->args.reference))
		 : struncmp(((EXISTDATA_S *) data)->args.name, mailbox,
			    strlen(((EXISTDATA_S *) data)->args.name)))){
	    char   *p;
	    size_t  alloclen;
	    size_t  len = (((stream && stream->mailbox)
			      ? strlen(stream->mailbox) : 0)
			   + strlen(((EXISTDATA_S *) data)->args.reference)
			   + strlen(((EXISTDATA_S *) data)->args.name)
			   + strlen(mailbox)) * sizeof(char);

	    /*
	     * Fully qualify (in the c-client name structure sense)
	     * anything that's not in the context of the "reference"...
	     */
	    if(*((EXISTDATA_S *) data)->fullname)
	      fs_give((void **) ((EXISTDATA_S *) data)->fullname);

	    alloclen = len;
	    *((EXISTDATA_S *) data)->fullname = (char *) fs_get(alloclen);
	    if(*mailbox != '{'
	       && stream && stream->mailbox && *stream->mailbox == '{'
	       && (p = strindex(stream->mailbox, '}'))){
		len = (p - stream->mailbox) + 1;
		strncpy(*((EXISTDATA_S *) data)->fullname,
			stream->mailbox, MIN(len,alloclen));
		p = *((EXISTDATA_S *) data)->fullname + len;
	    }
	    else
	      p = *((EXISTDATA_S *) data)->fullname;

	    strncpy(p, mailbox, alloclen-(p-(*((EXISTDATA_S *) data)->fullname)));
	    (*((EXISTDATA_S *) data)->fullname)[alloclen-1] = '\0';;
	}
    }
}


void
mail_list_response(MAILSTREAM *stream, char *mailbox, int delim, long int attribs,
		   void *data, unsigned int options)
{
    int counted = 0;

    if(delim)
      ((LISTRES_S *) data)->delim = delim;

    if(mailbox && *mailbox){
	if(!(attribs & LATT_NOSELECT)){
	    counted++;
	    ((LISTRES_S *) data)->isfile  = 1;
	    ((LISTRES_S *) data)->count	 += 1;
	}

	if(!(attribs & LATT_NOINFERIORS)){
	    ((LISTRES_S *) data)->isdir  = 1;

	    if(!counted)
	      ((LISTRES_S *) data)->count += 1;
	}

	if(attribs & LATT_HASCHILDREN)
	  ((LISTRES_S *) data)->haschildren = 1;

	if(attribs & LATT_HASNOCHILDREN)
	  ((LISTRES_S *) data)->hasnochildren = 1;
    }
}


char *
folder_as_breakout(CONTEXT_S *cntxt, char *name)
{
    if(context_isambig(name)){		/* if simple check doesn't pan out */
	char tmp[2*MAILTMPLEN], *p, *f;	/* look harder */

	if(!cntxt->dir->delim){
	    (void) context_apply(tmp, cntxt, "", sizeof(tmp)/2);
	    cntxt->dir->delim = get_folder_delimiter(tmp);
	}

	if((p = strindex(name, cntxt->dir->delim)) != NULL){
	    if(p == name){		/* assumption 6,321: delim is root */
		if(cntxt->context[0] == '{'
		   && (p = strindex(cntxt->context, '}'))){
		    strncpy(tmp, cntxt->context,
			    MIN((p - cntxt->context) + 1, sizeof(tmp)/2));
		    tmp[MIN((p - cntxt->context) + 1, sizeof(tmp)/2)] = '\0';
		    strncpy(&tmp[MIN((p - cntxt->context) + 1, sizeof(tmp)/2)],
			    name, sizeof(tmp)/2-strlen(tmp));
		    tmp[sizeof(tmp)-1] = '\0';
		    return(cpystr(tmp));
		}
		else
		  return(cpystr(name));
	    }
	    else{			/* assumption 6,322: no create ~foo */
		strncpy(tmp, name, MIN(p - name, MAILTMPLEN));
		/* lop off trailingpath */
		tmp[MIN(p - name, sizeof(tmp)/2)] = '\0';
		f	      = NULL;
		(void)folder_name_exists(cntxt, tmp, &f);
		if(f){
		    snprintf(tmp, sizeof(tmp), "%s%s",f,p);
		    tmp[sizeof(tmp)-1] = '\0';
		    fs_give((void **) &f);
		    return(cpystr(tmp));
		}
	    }
	}
    }

    return(NULL);
}


/*----------------------------------------------------------------------
 Initialize global list of contexts for folder collections.

 Interprets collections defined in the pinerc and orders them for
 pine's use.  Parses user-provided context labels and sets appropriate 
 use flags and the default prototype for that collection. 
 (See find_folders for how the actual folder list is found).

  ----*/
void
init_folders(struct pine *ps)
{
    CONTEXT_S  *tc, *top = NULL, **clist;
    int         i, prime = 0;

    clist = &top;

    /*
     * If no incoming folders are config'd, but the user asked for
     * them via feature, make sure at least "inbox" ends up there...
     */
    if(F_ON(F_ENABLE_INCOMING, ps) && !ps->VAR_INCOMING_FOLDERS){
	ps->VAR_INCOMING_FOLDERS    = (char **)fs_get(2 * sizeof(char *));
	ps->VAR_INCOMING_FOLDERS[0] = cpystr(ps->inbox_name);
	ps->VAR_INCOMING_FOLDERS[1] = NULL;
    }

    /*
     * Build context that's a list of folders the user's defined
     * as receiveing new messages.  At some point, this should
     * probably include adding a prefix with the new message count.
     * fake new context...
     */
    if(ps->VAR_INCOMING_FOLDERS && ps->VAR_INCOMING_FOLDERS[0]
       && (tc = new_context("Incoming-Folders []", NULL))){
	tc->dir->status &= ~CNTXT_NOFIND;
	tc->use    |= CNTXT_INCMNG;	/* mark this as incoming collection */
	if(tc->label)
	  fs_give((void **) &tc->label);

	/* TRANSLATORS: a label */
	tc->label = cpystr(_("Incoming Message Folders"));

	*clist = tc;
	clist  = &tc->next;

	init_incoming_folder_list(ps, tc);
    }

    /*
     * Build list of folder collections.  Because of the way init.c
     * works, we're guaranteed at least a default.  Also write any
     * "bogus format" messages...
     */
    for(i = 0; ps->VAR_FOLDER_SPEC && ps->VAR_FOLDER_SPEC[i] ; i++)
      if((tc = new_context(ps->VAR_FOLDER_SPEC[i], &prime)) != NULL){
	  *clist    = tc;			/* add it to list   */
	  clist	    = &tc->next;		/* prepare for next */
	  tc->var.v = &ps->vars[V_FOLDER_SPEC];
	  tc->var.i = i;
      }


    /*
     * Whoah cowboy!!!  Guess we couldn't find a valid folder
     * collection???
     */
    if(!prime)
      panic(_("No folder collections defined"));

    /*
     * At this point, insert the INBOX mapping as the leading
     * folder entry of the first collection...
     */
    init_inbox_mapping(ps->VAR_INBOX_PATH, top);

    set_news_spec_current_val(TRUE, TRUE);

    /*
     * If news groups, loop thru list adding to collection list
     */
    for(i = 0; ps->VAR_NEWS_SPEC && ps->VAR_NEWS_SPEC[i] ; i++)
      if(ps->VAR_NEWS_SPEC[i][0]
	 && (tc = new_context(ps->VAR_NEWS_SPEC[i], NULL))){
	  *clist      = tc;			/* add it to list   */
	  clist       = &tc->next;		/* prepare for next */
	  tc->var.v   = &ps->vars[V_NEWS_SPEC];
	  tc->var.i   = i;
      }

    ps->context_list    = top;	/* init pointers */
    ps->context_current = (top->use & CNTXT_INCMNG) ? top->next : top;
    ps->context_last    = NULL;
    /* Tie up all the previous pointers */
    for(; top; top = top->next)
      if(top->next)
	top->next->prev = top;

#ifdef	DEBUG
      dump_contexts();
#endif
}


/*
 * Add incoming list of folders to context.
 */
void
init_incoming_folder_list(struct pine *ps, CONTEXT_S *cntxt)
{
    int       i;
    char     *folder_string, *nickname;
    FOLDER_S *f;

    for(i = 0; ps->VAR_INCOMING_FOLDERS[i] ; i++){
	/*
	 * Parse folder line for nickname and folder name.
	 * No nickname on line is OK.
	 */
	get_pair(ps->VAR_INCOMING_FOLDERS[i], &nickname, &folder_string,0,0);

	/*
	 * Allow for inbox to be specified in the incoming list, but
	 * don't let it show up along side the one magically inserted
	 * above!
	 */
	if(!folder_string || !strucmp(ps->inbox_name, folder_string)){
	    if(folder_string)
	      fs_give((void **)&folder_string);

	    if(nickname)
	      fs_give((void **)&nickname);

	    continue;
	}
	else if(update_bboard_spec(folder_string, tmp_20k_buf, SIZEOF_20KBUF)){
	    fs_give((void **) &folder_string);
	    folder_string = cpystr(tmp_20k_buf);
	}

	f = new_folder(folder_string,
		       line_hash(ps->VAR_INCOMING_FOLDERS[i]));
	f->isfolder = 1;
	fs_give((void **)&folder_string);

	if(nickname){
	    if(strucmp(ps->inbox_name, nickname)){
		f->nickname = nickname;
		f->name_len = strlen(f->nickname);
	    }
	    else
	      fs_give((void **)&nickname);
	}

	init_incoming_unseen_data(ps, f);

	folder_insert(f->nickname 
		      && (strucmp(f->nickname, ps->inbox_name) == 0)
			    ? -1 : folder_total(FOLDERS(cntxt)),
		      f, FOLDERS(cntxt));
    }
}


void
init_incoming_unseen_data(struct pine *ps, FOLDER_S *f)
{
    int j, check_this = 0;

    if(!f)
      return;

    /* see if this folder is in the monitoring list */
    if(F_ON(F_ENABLE_INCOMING_CHECKING, ps)){
	if(!ps->VAR_INCCHECKLIST)
	  check_this++;			/* everything in by default */
	else{
	    for(j = 0; !check_this && ps->VAR_INCCHECKLIST[j]; j++){
		if((f->nickname && !strucmp(ps->VAR_INCCHECKLIST[j],f->nickname))
		   || (f->name && !strucmp(ps->VAR_INCCHECKLIST[j],f->name)))
		  check_this++;
	    }
	}
    }

    if(check_this)
      f->last_unseen_update = LUU_INIT;
    else
      f->last_unseen_update = LUU_NEVERCHK;
}


void
reinit_incoming_folder_list(struct pine *ps, CONTEXT_S *context)
{
    free_folder_entries(&(FOLDERS(context)));
    FOLDERS(context) = init_folder_entries();
    init_incoming_folder_list(ps_global, context);
    init_inbox_mapping(ps_global->VAR_INBOX_PATH, context);
}


/*
 *
 */
void
init_inbox_mapping(char *path, CONTEXT_S *cntxt)
{
    FOLDER_S *f;
    int check_this, j;

    /*
     * If mapping already exists, blast it and replace it below...
     */
    if((f = folder_entry(0, FOLDERS(cntxt)))
       && f->nickname && !strcmp(f->nickname, ps_global->inbox_name))
      folder_delete(0, FOLDERS(cntxt));

    if(path){
	f = new_folder(path, 0);
	f->nickname = cpystr(ps_global->inbox_name);
	f->name_len = strlen(f->nickname);
    }
    else
      f = new_folder(ps_global->inbox_name, 0);

    f->isfolder = 1;

    /* see if this folder is in the monitoring list */
    check_this = 0;
    if(F_ON(F_ENABLE_INCOMING_CHECKING, ps_global) && ps_global->VAR_INCOMING_FOLDERS && ps_global->VAR_INCOMING_FOLDERS[0]){
	if(!ps_global->VAR_INCCHECKLIST)
	  check_this++;			/* everything in by default */
	else{
	    for(j = 0; !check_this && ps_global->VAR_INCCHECKLIST[j]; j++){
		if((f->nickname && !strucmp(ps_global->VAR_INCCHECKLIST[j],f->nickname))
		   || (f->name && !strucmp(ps_global->VAR_INCCHECKLIST[j],f->name)))
		  check_this++;
	    }
	}
    }

    if(check_this)
      f->last_unseen_update = LUU_INIT;
    else
      f->last_unseen_update = LUU_NEVERCHK;

    folder_insert(0, f, FOLDERS(cntxt));
}


FDIR_S *
new_fdir(char *ref, char *view, int wildcard)
{
    FDIR_S *rv = (FDIR_S *) fs_get(sizeof(FDIR_S));

    memset((void *) rv, 0, sizeof(FDIR_S));

    /* Monkey with the view to make sure it has wildcard? */
    if(view && *view){
	rv->view.user = cpystr(view);

	/*
	 * This is sorta hairy since, for simplicity we allow
	 * users to use '*' in the view, but for mail
	 * we really mean '%' as def'd in 2060...
	 */
	if(wildcard == '*'){	/* must be #news. */
	    if(strindex(view, wildcard))
	      rv->view.internal = cpystr(view);
	}
	else{			/* must be mail */
	    char *p;

	    if((p = strpbrk(view, "*%")) != NULL){
		rv->view.internal = p = cpystr(view);
		while((p = strpbrk(p, "*%")) != NULL)
		  *p++ = '%';	/* convert everything to '%' */
	    }
	}

	if(!rv->view.internal){
	    size_t l;

	    l = strlen(view)+2;
	    rv->view.internal = (char *) fs_get((l+1) * sizeof(char));
	    snprintf(rv->view.internal, l+1, "%c%s%c", wildcard, view, wildcard);
	}
    }
    else{
	rv->view.internal = (char *) fs_get(2 * sizeof(char));
	snprintf(rv->view.internal, 2, "%c", wildcard);
    }

    if(ref)
      rv->ref = ref;

    rv->folders = init_folder_entries();
    rv->status = CNTXT_NOFIND;
    return(rv);
}


void
free_fdir(FDIR_S **f, int recur)
{
    if(f && *f){
	if((*f)->prev && recur)
	  free_fdir(&(*f)->prev, 1);

	if((*f)->ref)
	  fs_give((void **)&(*f)->ref);

	if((*f)->view.user)
	  fs_give((void **)&(*f)->view.user);

	if((*f)->view.internal)
	  fs_give((void **)&(*f)->view.internal);

	if((*f)->desc)
	  fs_give((void **)&(*f)->desc);

	free_folder_entries(&(*f)->folders);
	fs_give((void **) f);
    }
}


int
update_bboard_spec(char *bboard, char *buf, size_t buflen)
{
    char *p = NULL, *origbuf;
    int	  nntp = 0, bracket = 0;

    origbuf = buf;

    if(*bboard == '*'
       || (*bboard == '{' && (p = strindex(bboard, '}')) && *(p+1) == '*')){
	/* server name ? */
	if(p || (*(bboard+1) == '{' && (p = strindex(++bboard, '}'))))
	  while(bboard <= p && (buf-origbuf < buflen))		/* copy it */
	    if((*buf++ = *bboard++) == '/' && !strncmp(bboard, "nntp", 4))
	      nntp++;

	if(*bboard == '*')
	  bboard++;

	if(!nntp)
	  /*
	   * See if path portion looks newsgroup-ish while being aware
	   * of the "view" portion of the spec...
	   */
	  for(p = bboard; *p; p++)
	    if(*p == '['){
		if(bracket)		/* only one set allowed! */
		  break;
		else
		  bracket++;
	    }
	    else if(*p == ']'){
		if(bracket != 1)	/* must be closing bracket */
		  break;
		else
		  bracket++;
	    }
	    else if(!(isalnum((unsigned char) *p) || strindex(".-", *p)))
	      break;

	snprintf(buf, buflen-(buf-origbuf), "%s%s%s",
		(!nntp && *p) ? "#public" : "#news.",
		(!nntp && *p && *bboard != '/') ? "/" : "",
		bboard);

	return(1);
    }

    return(0);
}


/*
 * build_folder_list - call mail_list to fetch us a list of folders
 *		       from the given context.
 */
void
build_folder_list(MAILSTREAM **stream, CONTEXT_S *context, char *pat, char *content, int flags)
{
    MM_LIST_S  ldata;
    BFL_DATA_S response;
    int	       local_open = 0, we_cancel = 0, resort = 0;
    char       reference[2*MAILTMPLEN], *p;

    if(!(context->dir->status & CNTXT_NOFIND)
       || (context->dir->status & CNTXT_PARTFIND))
      return;				/* find already done! */

    dprint((7, "build_folder_list: %s %s\n",
	       context ? context->context : "NULL",
	       pat ? pat : "NULL"));

    we_cancel = busy_cue(NULL, NULL, 1);

    /*
     * Set up the pattern of folder name's to match within the
     * given context.
     */
    if(!pat || ((*pat == '*' || *pat == '%') && *(pat+1) == '\0')){
	context->dir->status &= ~CNTXT_NOFIND;	/* let'em know we tried */
	pat = context->dir->view.internal;
    }
    else
      context->use |= CNTXT_PARTFIND;	/* or are in a partial find */

    memset(mm_list_info = &ldata, 0, sizeof(MM_LIST_S));
    ldata.filter = (NEWS_TEST(context)) ? mail_lsub_filter : mail_list_filter;
    memset(ldata.data = &response, 0, sizeof(BFL_DATA_S));
    response.list = FOLDERS(context);

    if(flags & BFL_FLDRONLY)
      response.mask = LATT_NOSELECT;

    /*
     * if context is associated with a server, prepare a stream for
     * sending our request.
     */
    if(*context->context == '{'){
	/*
	 * Try using a stream we've already got open...
	 */
	if(stream && *stream
	   && !(ldata.stream = same_stream(context->context, *stream))){
	    pine_mail_close(*stream);
	    *stream = NULL;
	}

	if(!ldata.stream)
	  ldata.stream = sp_stream_get(context->context, SP_MATCH);

	if(!ldata.stream)
	  ldata.stream = sp_stream_get(context->context, SP_SAME);

	/* gotta open a new one? */
	if(!ldata.stream){
	    ldata.stream = mail_cmd_stream(context, &local_open);
	    if(stream)
	      *stream = ldata.stream;
	}

	dprint((ldata.stream ? 7 : 1, "build_folder_list: mail_open(%s) %s.\n",
		context->server ? context->server : "?",
		ldata.stream ? "OK" : "FAILED"));

	if(!ldata.stream){
	    context->use &= ~CNTXT_PARTFIND;	/* unset partial find bit */
	    if(we_cancel)
	      cancel_busy_cue(-1);

	    return;
	}
    }
    else if(stream && *stream){			/* no server, simple case */
	if(!sp_flagged(*stream, SP_LOCKED))
	  pine_mail_close(*stream);

	*stream = NULL;
    }

    /*
     * If preset reference string, we're somewhere in the hierarchy.
     * ELSE we must be at top of context...
     */
    response.args.name = pat;
    if(!(response.args.reference = context->dir->ref)){
	if((p = strstr(context->context, "%s")) != NULL){
	    strncpy(response.args.reference = reference,
		    context->context,
		    MIN(p - context->context, sizeof(reference)/2));
	    reference[MIN(p - context->context, sizeof(reference)/2)] = '\0';
	    if(*(p += 2))
	      response.args.tail = p;
	}
	else
	  response.args.reference = context->context;
    }

    if(flags & BFL_SCAN)
      mail_scan(ldata.stream, response.args.reference,
		response.args.name, content);
    else if(flags & BFL_LSUB)
      mail_lsub(ldata.stream, response.args.reference, response.args.name);
    else{
      set_read_predicted(1);
      pine_mail_list(ldata.stream, response.args.reference, response.args.name,
		     &ldata.options);
      set_read_predicted(0);
    }

    if(context->dir && response.response.delim)
      context->dir->delim = response.response.delim;

    if(!(flags & (BFL_LSUB|BFL_SCAN)) && F_ON(F_QUELL_EMPTY_DIRS, ps_global)){
	LISTRES_S  listres;
	FOLDER_S  *f;
	int	   i;

	for(i = 0; i < folder_total(response.list); i++)
	  if((f = folder_entry(i, FOLDERS(context)))->isdir){
	      /* 
	       * we don't need to do a list if we know already
	       * whether there are children or not.
	       */
	      if(!f->haschildren && !f->hasnochildren){
		  memset(ldata.data = &listres, 0, sizeof(LISTRES_S));
		  ldata.filter = mail_list_response;

		  if(context->dir->ref)
		    snprintf(reference, sizeof(reference), "%s%s", context->dir->ref, f->name);
		  else
		    context_apply(reference, context, f->name, sizeof(reference)/2);

		  /* append the delimiter to the reference */
		  for(p = reference; *p; p++)
		    ;

		  *p++ = context->dir->delim;
		  *p   = '\0';

		  pine_mail_list(ldata.stream, reference, "%", NULL);

		  /* anything interesting inside? */
		  f->hasnochildren = (listres.count <= 1L);
		  f->haschildren   = !f->hasnochildren;;
	      }

	      if(f->hasnochildren){
		  if(f->isfolder){
		      f->isdir = 0;
		      if(ps_global->fld_sort_rule == FLD_SORT_ALPHA_DIR_FIRST
			 || ps_global->fld_sort_rule == FLD_SORT_ALPHA_DIR_LAST)
		        resort = 1;
		  }
		  /*
		   * We don't want to hide directories
		   * that are only directories even if they are
		   * empty. We only want to hide the directory
		   * piece of a dual-use folder when there are
		   * no children in the directory (and the user
		   * most likely thinks of it as a folder instead
		   * of a folder and a directory).
		   */
		  else if(f->isdir && f->isdual){
		      folder_delete(i, FOLDERS(context));
		      i--;
		  }
	      }
	  }
    }

    if(resort)
      resort_folder_list(response.list);

    if(local_open && !stream)
      pine_mail_close(ldata.stream);

    if(context->use & CNTXT_PRESRV)
      folder_select_restore(context);

    context->use &= ~CNTXT_PARTFIND;	/* unset partial list bit */
    if(we_cancel)
      cancel_busy_cue(-1);
}


/*
 * Validate LIST response to command issued from build_folder_list
 *
 */
void
mail_list_filter(MAILSTREAM *stream, char *mailbox, int delim, long int attribs, void *data, unsigned int options)
{
    BFL_DATA_S *ld = (BFL_DATA_S *) data;
    FOLDER_S   *new_f = NULL, *dual_f = NULL;
    int         suppress_folder_add = 0;

    if(!ld)
      return;

    if(delim)
      ld->response.delim = delim;

    /* test against mask of DIS-allowed attributes */
    if((ld->mask & attribs)){
	dprint((3, "mail_list_filter: failed attribute test"));
	return;
    }

    /*
     * First, make sure response fits our "reference" arg 
     * NOTE: build_folder_list can't supply breakout?
     */
    if(!mail_list_in_collection(&mailbox, ld->args.reference,
				ld->args.name, ld->args.tail))
      return;

    /* ignore dotfolders unless told not to */
    if(F_OFF(F_ENABLE_DOT_FOLDERS, ps_global) && *mailbox == '.'){
	dprint((3, "mail_list_filter: dotfolder disallowed"));
	return;
    }

    /*
     * If this response is INBOX we have to handle it specially.
     * The special cases are:
     *
     * Incoming folders are enabled and this INBOX is in the Incoming
     * folders list already. We don't want to list it here, as well,
     * unless it is also a directory.
     *
     * Incoming folders are not enabled, but we inserted INBOX into
     * this primary collection (with init_inbox_mapping()) so it is
     * already accounted for unless it is also a directory.
     *
     * Incoming folders are not enabled, but we inserted INBOX into
     * the primary collection (which we are not looking at here) so
     * it is already accounted for there. We'll add it as a directory
     * as well here if that is what is called for.
     */
    if(!strucmp(mailbox, ps_global->inbox_name)){
	int ftotal, i;
	FOLDER_S *f;
	char fullname[1000], tmp1[1000], tmp2[1000], *l1, *l2;

	/* fullname is the name of the mailbox in the list response */
	if(ld->args.reference){
	    strncpy(fullname, ld->args.reference, sizeof(fullname)-1);
	    fullname[sizeof(fullname)-1] = '\0';
	}
	else
	  fullname[0] = '\0';

	strncat(fullname, mailbox, sizeof(fullname)-strlen(fullname)-1);
	fullname[sizeof(fullname)-1] = '\0';

	/* check if Incoming Folders are enabled */
	if(ps_global->context_list && ps_global->context_list->use & CNTXT_INCMNG){
	    int this_inbox_is_in_incoming = 0;

	    /*
	     * Figure out if this INBOX is already in the Incoming list.
	     */

	    /* compare fullname to each incoming folder */
	    ftotal = folder_total(FOLDERS(ps_global->context_list));
	    for(i = 0; i < ftotal && !this_inbox_is_in_incoming; i++){
		f = folder_entry(i, FOLDERS(ps_global->context_list));
		if(f && f->name){
		    if(same_remote_mailboxes(fullname, f->name)
		       || ((!IS_REMOTE(fullname) && !IS_REMOTE(f->name))
			   && (l1=mailboxfile(tmp1,fullname))
			   && (l2=mailboxfile(tmp2,f->name))
			   && !strcmp(l1,l2)))
		      this_inbox_is_in_incoming++;
		}
	    }
// UNFIX
/*	    if(this_inbox_is_in_incoming){
		/*
		 * Don't add a folder for this, only a directory if called for.
		 * If it isn't a directory, skip it.
		 */
/*		if(!(delim && !(attribs & LATT_NOINFERIORS)))
		  return;

		suppress_folder_add++;
	    }*/
	}
	else{
	    int inbox_is_in_this_collection = 0;

	    /* is INBOX already in this folder list? */
	    ftotal = folder_total(ld->list);
	    for(i = 0; i < ftotal && !inbox_is_in_this_collection; i++){
		f = folder_entry(i, ld->list);
	        if(!strucmp(FLDR_NAME(f), ps_global->inbox_name))
		  inbox_is_in_this_collection++;
	    }

	    if(inbox_is_in_this_collection){

		/*
		 * Inbox is already inserted in this collection. Unless
		 * it is also a directory, we are done.
		 */
// UNFIX
/*		if(!(delim && !(attribs & LATT_NOINFERIORS)))
		  return;*/

		/*
		 * Since it is also a directory, what we do depends on
		 * the F_SEP feature. If that feature is not set we just
		 * want to mark the existing entry dual-use. If F_SEP is
		 * set we want to add a new directory entry.
		 */
		if(F_ON(F_SEPARATE_FLDR_AS_DIR, ps_global)){
		    f->isdir = 0;
		    f->haschildren = 0;
		    f->hasnochildren = 0;
		    /* fall through and add a new directory */
		}
		else{
		    /* mark existing entry dual-use and return */
		    ld->response.count++;
		    ld->response.isdir = 1;
		    f->isdir = 1;
		    if(attribs & LATT_HASCHILDREN)
		      f->haschildren = 1;

		    if(attribs & LATT_HASNOCHILDREN)
		      f->hasnochildren = 1;

		    return;
		}

		suppress_folder_add++;
	    }
	    else{
		int found_it = 0;
		int this_inbox_is_primary_inbox = 0;

		/*
		 * See if this INBOX is the same as the INBOX we inserted
		 * in the primary collection.
		 */

		/* first find the existing INBOX entry */
		ftotal = folder_total(FOLDERS(ps_global->context_list));
		for(i = 0; i < ftotal && !found_it; i++){
		    f = folder_entry(i, FOLDERS(ps_global->context_list));
		    if(!strucmp(FLDR_NAME(f), ps_global->inbox_name))
		      found_it++;
		}

		if(found_it && f && f->name){
		    if(same_remote_mailboxes(fullname, f->name)
		       || ((!IS_REMOTE(fullname) && !IS_REMOTE(f->name))
			   && (l1=mailboxfile(tmp1,fullname))
			   && (l2=mailboxfile(tmp2,f->name))
			   && !strcmp(l1,l2)))
		      this_inbox_is_primary_inbox++;
		}
/* UNFIX
		if(this_inbox_is_primary_inbox){
		    /*
		     * Don't add a folder for this, only a directory if called for.
		     * If it isn't a directory, skip it.
		     */
/*		    if(!(delim && !(attribs & LATT_NOINFERIORS)))
		      return;

		    suppress_folder_add++;
		}*/
	    }
	}
    }

    /* is it a mailbox? */
    if(!(attribs & LATT_NOSELECT) && !suppress_folder_add){
	ld->response.count++;
	ld->response.isfile = 1;
	new_f = new_folder(mailbox, 0);
	new_f->isfolder = 1;

	if(F_ON(F_SEPARATE_FLDR_AS_DIR, ps_global)){
	    folder_insert(-1, new_f, ld->list);
	    dual_f = new_f;
	    new_f = NULL;
	}
    }

    /* directory? */
    if(delim && !(attribs & LATT_NOINFERIORS)){
	ld->response.count++;
	ld->response.isdir = 1;

	if(!new_f)
	  new_f = new_folder(mailbox, 0);

	new_f->isdir = 1;
	if(attribs & LATT_HASCHILDREN)
	  new_f->haschildren = 1;
	if(attribs & LATT_HASNOCHILDREN)
	  new_f->hasnochildren = 1;

	/*
	 * When we have F_SEPARATE_FLDR_AS_DIR we still want to know
	 * whether the name really represents both so that we don't
	 * inadvertently delete both when the user meant one or the
	 * other.
	 */
	if(dual_f){
	    if(attribs & LATT_HASCHILDREN)
	      dual_f->haschildren = 1;

	    if(attribs & LATT_HASNOCHILDREN)
	      dual_f->hasnochildren = 1;

	    dual_f->isdual = 1;
	    new_f->isdual = 1;
	}
    }

    if(new_f)
      folder_insert(-1, new_f, ld->list);

    if(attribs & LATT_MARKED)
      ld->response.ismarked = 1;

    /* don't mark #move folders unmarked */
    if(attribs & LATT_UNMARKED && !(options & PML_IS_MOVE_MBOX))
      ld->response.unmarked = 1;

    if(attribs & LATT_HASCHILDREN)
      ld->response.haschildren = 1;

    if(attribs & LATT_HASNOCHILDREN)
      ld->response.hasnochildren = 1;
}


/*
 * Validate LSUB response to command issued from build_folder_list
 *
 */
void
mail_lsub_filter(MAILSTREAM *stream, char *mailbox, int delim, long int attribs,
		 void *data, unsigned int options)
{
    BFL_DATA_S *ld = (BFL_DATA_S *) data;
    FOLDER_S   *new_f = NULL;
    char       *ref;

    if(delim)
      ld->response.delim = delim;

    /* test against mask of DIS-allowed attributes */
    if((ld->mask & attribs)){
	dprint((3, "mail_lsub_filter: failed attribute test"));
	return;
    }

    /* Normalize mailbox and reference strings re: namespace */
    if(!strncmp(mailbox, "#news.", 6))
      mailbox += 6;

    if(!strncmp(ref = ld->args.reference, "#news.", 6))
      ref += 6;

    if(!mail_list_in_collection(&mailbox, ref, ld->args.name, ld->args.tail))
      return;

    if(!(attribs & LATT_NOSELECT)){
	ld->response.count++;
	ld->response.isfile = 1;
	new_f = new_folder(mailbox, 0);
	new_f->isfolder = 1;
	folder_insert(F_ON(F_READ_IN_NEWSRC_ORDER, ps_global)
			? folder_total(ld->list) : -1,
		      new_f, ld->list);
    }

    /* We don't support directories in #news */
}


int
mail_list_in_collection(char **mailbox, char *ref, char *name, char *tail)
{
    int	  boxlen, reflen, taillen;
    char *p;

    boxlen  = strlen(*mailbox);
    reflen  = ref ? strlen(ref) : 0;
    taillen = tail ? strlen(tail) : 0;

    if(boxlen
       && (reflen ? !struncmp(*mailbox, ref, reflen)
		  : (p = strpbrk(name, "%*"))
		       ? !struncmp(*mailbox, name, p - name)
		       : !strucmp(*mailbox,name))
       && (!taillen
	   || (taillen < boxlen - reflen
	       && !strucmp(&(*mailbox)[boxlen - taillen], tail)))){
	if(taillen)
	  (*mailbox)[boxlen - taillen] = '\0';

	if(*(*mailbox += reflen))
	  return(TRUE);
    }
    /*
     * else don't worry about context "breakouts" since
     * build_folder_list doesn't let the user introduce
     * one...
     */

    return(FALSE);
}


/*
 * rebuild_folder_list -- free up old list and re-issue commands to build
 *			  a new list.
 */
void
refresh_folder_list(CONTEXT_S *context, int nodirs, int startover, MAILSTREAM **streamp)
{
    if(startover)
      free_folder_list(context);

    build_folder_list(streamp, context, NULL, NULL,
		      (NEWS_TEST(context) ? BFL_LSUB : BFL_NONE)
		        | ((nodirs) ? BFL_FLDRONLY : BFL_NONE));
}


/*
 * free_folder_list - loop thru the context's lists of folders
 *                     clearing all entries without nicknames
 *                     (as those were user provided) AND reset the 
 *                     context's find flag.
 *
 * NOTE: if fetched() information (e.g., like message counts come back
 *       in bboard collections), we may want to have a check before
 *       executing the loop and setting the FIND flag.
 */
void
free_folder_list(CONTEXT_S *cntxt)
{
    int n, i;

    /*
     * In this case, don't blast the list as it was given to us by the
     * user and not the result of a mail_list call...
     */
    if(cntxt->use & CNTXT_INCMNG)
      return;

    if(cntxt->use & CNTXT_PRESRV)
      folder_select_preserve(cntxt);

    for(n = folder_total(FOLDERS(cntxt)), i = 0; n > 0; n--)
      if(folder_entry(i, FOLDERS(cntxt))->nickname)
	i++;					/* entry wasn't from LIST */
      else
	folder_delete(i, FOLDERS(cntxt));

    cntxt->dir->status |= CNTXT_NOFIND;		/* do find next time...  */
						/* or add the fake entry */
    cntxt->use &= ~(CNTXT_PSEUDO | CNTXT_PRESRV | CNTXT_ZOOM);
}


/*
 * default_save_context - return the default context for saved messages
 */
CONTEXT_S *
default_save_context(CONTEXT_S *cntxt)
{
    while(cntxt)
      if((cntxt->use) & CNTXT_SAVEDFLT)
	return(cntxt);
      else
	cntxt = cntxt->next;

    return(NULL);
}



/*
 * folder_complete - foldername completion routine
 *
 *   Result: returns 0 if the folder doesn't have a any completetion
 *		     1 if the folder has a completion (*AND* "name" is
 *		       replaced with the completion)
 *
 */
int
folder_complete(CONTEXT_S *context, char *name, size_t namelen, int *completions)
{
    return(folder_complete_internal(context, name, namelen, completions, FC_NONE));
}


/*
 * 
 */
int
folder_complete_internal(CONTEXT_S *context, char *name, size_t namelen,
			 int *completions, int flags)
{
    int	      i, match = -1, ftotal;
    char      tmp[MAXFOLDER+2], *a, *b, *fn, *pat;
    FOLDER_S *f;

    if(completions)
      *completions = 0;

    if(*name == '\0' || !context_isambig(name))
      return(0);

    if(!((context->use & CNTXT_INCMNG) || ALL_FOUND(context))){
	/*
	 * Build the folder list from scratch since we may need to
	 * traverse hierarchy...
	 */
	
	free_folder_list(context);
	snprintf(tmp, sizeof(tmp), "%s%c", name, NEWS_TEST(context) ? '*' : '%');
	build_folder_list(NULL, context, tmp, NULL,
			  (NEWS_TEST(context) & !(flags & FC_FORCE_LIST))
			    ? BFL_LSUB : BFL_NONE);
    }

    *tmp = '\0';			/* find uniq substring */
    ftotal = folder_total(FOLDERS(context));
    for(i = 0; i < ftotal; i++){
	f   = folder_entry(i, FOLDERS(context));
	fn  = FLDR_NAME(f);
	pat = name;
	if(!(NEWS_TEST(context) || (context->use & CNTXT_INCMNG))){
	    fn  = folder_last_cmpnt(fn, context->dir->delim);
	    pat = folder_last_cmpnt(pat, context->dir->delim);
	}

	if(!strncmp(fn, pat, strlen(pat))){
	    if(match != -1){		/* oh well, do best we can... */
		a = fn;
		if(match >= 0){
		    f  = folder_entry(match, FOLDERS(context));
		    fn = FLDR_NAME(f);
		    if(!NEWS_TEST(context))
		      fn = folder_last_cmpnt(fn, context->dir->delim);

		    strncpy(tmp, fn, sizeof(tmp)-1);
		    tmp[sizeof(tmp)-1] = '\0';
		}

		match = -2;
		b     = tmp;		/* remember largest common text */
		while(*a && *b && *a == *b)
		  *b++ = *a++;

		*b = '\0';
	    }
	    else		
	      match = i;		/* bingo?? */
	}
    }

    if(match >= 0){			/* found! */
	f  = folder_entry(match, FOLDERS(context));
	fn = FLDR_NAME(f);
	if(!(NEWS_TEST(context) || (context->use & CNTXT_INCMNG)))
	  fn = folder_last_cmpnt(fn, context->dir->delim);

	strncpy(pat, fn, namelen-(pat-name));
	name[namelen-1] = '\0';
	if(f->isdir && !f->isfolder){
	    name[i = strlen(name)] = context->dir->delim;
	    name[i+1] = '\0';
	}
    }
    else if(match == -2){		/* closest we could find */
	strncpy(pat, tmp, namelen-(pat-name));
	name[namelen-1] = '\0';
    }

    if(completions)
      *completions = ftotal;

    if(!((context->use & CNTXT_INCMNG) || ALL_FOUND(context)))
      free_folder_list(context);

    return((match >= 0) ? ftotal : 0);
}


char *
folder_last_cmpnt(char *s, int d)
{
    register char *p;
    
    if(d)
      for(p = s; (p = strindex(p, d)); s = ++p)
	;

    return(s);
}


/*
 * init_folder_entries - return a piece of memory suitable for attaching 
 *                   a list of folders...
 *
 */
FLIST *
init_folder_entries(void)
{
    FLIST *flist = (FLIST *) fs_get(sizeof(FLIST));
    flist->folders = (FOLDER_S **) fs_get(FCHUNK * sizeof(FOLDER_S *));
    memset((void *)flist->folders, 0, (FCHUNK * sizeof(FOLDER_S *)));
    flist->allocated = FCHUNK;
    flist->used      = 0;
    return(flist);
}


/*
 * new_folder - return a brand new folder entry, with the given name
 *              filled in.
 *
 * NOTE: THIS IS THE ONLY WAY TO PUT A NAME INTO A FOLDER ENTRY!!!
 * STRCPY WILL NOT WORK!!!
 */
FOLDER_S *
new_folder(char *name, long unsigned int hash)
{
    FOLDER_S *tmp;
    size_t    l = strlen(name);

    tmp = (FOLDER_S *)fs_get(sizeof(FOLDER_S) + (l * sizeof(char)));
    memset((void *)tmp, 0, sizeof(FOLDER_S));
    strncpy(tmp->name, name, l);
    tmp->name[l] = '\0';
    tmp->name_len = (unsigned char) l;
    tmp->varhash = hash;
    return(tmp);
}


/*
 * folder_entry - folder struct access routine.  Permit reference to
 *                folder structs via index number. Serves two purposes:
 *            1) easy way for callers to access folder data 
 *               conveniently
 *            2) allows for a custom manager to limit memory use
 *               under certain rather limited "operating systems"
 *               who shall renameless, but whose initials are DOS
 *
 *
 */
FOLDER_S *
folder_entry(int i, FLIST *flist)
{
    return((i >= flist->used) ? NULL:flist->folders[i]);
}


/*
 * free_folder_entries - release all resources associated with the given 
 *			 list of folder entries
 */
void
free_folder_entries(FLIST **flist)
{
    register int i;

    if(!(flist && *flist))
      return;

    i = (*flist)->used;
    while(i--){
	if((*flist)->folders[i]->nickname)
	  fs_give((void **) &(*flist)->folders[i]->nickname);

	fs_give((void **) &((*flist)->folders[i]));
    }

    fs_give((void **) &((*flist)->folders));
    fs_give((void **) flist);
}


/*
 * return the number of folders associated with the given folder list
 */
int
folder_total(FLIST *flist)
{
    return((int) flist->used);
}


/*
 * return the index number of the given name in the given folder list
 */
int
folder_index(char *name, CONTEXT_S *cntxt, int flags)
{
    register  int i = 0;
    FOLDER_S *f;
    char     *fname;

    for(i = 0; (f = folder_entry(i, FOLDERS(cntxt))); i++)
      if(((flags & FI_FOLDER) && (f->isfolder || (cntxt->use & CNTXT_INCMNG)))
	 || ((flags & FI_DIR) && f->isdir)){
	  fname = FLDR_NAME(f);
#if defined(DOS) || defined(OS2)
	  if(flags & FI_RENAME){	/* case-dependent for rename */
	    if(*name == *fname && strcmp(name, fname) == 0)
	      return(i);
	  }
	  else{
	    if(toupper((unsigned char)(*name))
	       == toupper((unsigned char)(*fname)) && strucmp(name, fname) == 0)
	      return(i);
	  }
#else
	  if(*name == *fname && strcmp(name, fname) == 0)
	    return(i);
#endif
      }

    return(-1);
}


/*
 * folder_is_nick - check to see if the given name is a nickname
 *                  for some folder in the given context...
 *
 *  NOTE: no need to check if mm_list_names has been done as 
 *        nicknames can only be set by configuration...
 */
char *
folder_is_nick(char *nickname, FLIST *flist, int flags)
{
    register  int  i = 0;
    FOLDER_S *f;

    if(!(nickname && *nickname && flist))
      return(NULL);

    while((f = folder_entry(i, flist)) != NULL){
	/*
	 * The second part of the OR is checking in a case-indep
	 * way for INBOX. It should be restricted to the context
	 * to which we add the INBOX folder, which would be either
	 * the Incoming Folders collection or the first collection
	 * if there is no Incoming collection. We don't need to check
	 * the collection because nickname assignment has already
	 * done that for us. Most folders don't have nicknames, only
	 * incoming folders and folders like inbox if not in incoming.
	 */
	if(f->nickname
	   && (!strcmp(nickname, f->nickname)
	       || (!strucmp(nickname, f->nickname)
		   && !strucmp(nickname, ps_global->inbox_name)))){
	    char source[MAILTMPLEN], *target = NULL;

	    /*
	     * If f is a maildrop, then we want to return the
	     * destination folder, not the whole #move thing.
	     */
	    if(!(flags & FN_WHOLE_NAME)
	       && check_for_move_mbox(f->name, source, sizeof(source), &target))
	      return(target);
	    else
	      return(f->name);
	}
	else
	  i++;
    }

    return(NULL);
}


char *
folder_is_target_of_nick(char *longname, CONTEXT_S *cntxt)
{
    register  int  i = 0;
    FOLDER_S *f;
    FLIST *flist = NULL;

    if(cntxt && cntxt == ps_global->context_list)
      flist = FOLDERS(cntxt);

    if(!(longname && *longname && flist))
      return(NULL);

    while((f = folder_entry(i, flist)) != NULL){
	if(f->nickname && f->name && !strcmp(longname, f->name))
	  return(f->nickname);
	else
	  i++;
    }

    return(NULL);
}


/*----------------------------------------------------------------------
  Insert the given folder name into the sorted folder list
  associated with the given context.  Only allow ambiguous folder
  names IF associated with a nickname.

   Args: index  -- Index to insert at, OR insert in sorted order if -1
         folder -- folder structure to insert into list
	 flist  -- folder list to insert folder into

  **** WARNING ****
  DON'T count on the folder pointer being valid after this returns
  *** ALL FOLDER ELEMENT READS SHOULD BE THRU folder_entry() ***

  ----*/
int
folder_insert(int index, FOLDER_S *folder, FLIST *flist)
{
    /* requested index < 0 means add to sorted list */
    if(index < 0 && (index = folder_total(flist)) > 0)
      index = folder_insert_sorted(index / 2, 0, index, folder, flist,
		     (ps_global->fld_sort_rule == FLD_SORT_ALPHA_DIR_FIRST)
			? compare_folders_dir_alpha
			: (ps_global->fld_sort_rule == FLD_SORT_ALPHA_DIR_LAST)
			     ? compare_folders_alpha_dir
			     : compare_folders_alpha);

    folder_insert_index(folder, index, flist);
    return(index);
}


/*
 * folder_insert_sorted - Insert given folder struct into given list
 *			  observing sorting specified by given
 *			  comparison function
 */
int
folder_insert_sorted(int index, int min_index, int max_index, FOLDER_S *folder,
		     FLIST *flist, int (*compf)(FOLDER_S *, FOLDER_S *))
{
    int	      i;

    return(((i = (*compf)(folder_entry(index, flist), folder)) == 0)
	     ? index
	     : (i < 0)
		? ((++index >= max_index)
		    ? max_index
		    : ((*compf)(folder_entry(index, flist), folder) > 0)
		       ? index
		       : folder_insert_sorted((index + max_index) / 2, index,
					      max_index, folder, flist, compf))
		: ((index <= min_index)
		    ? min_index
		    : folder_insert_sorted((min_index + index) / 2, min_index, index,
					   folder, flist, compf)));
}


/* 
 * folder_insert_index - Insert the given folder struct into the global list
 *                 at the given index.
 */
void
folder_insert_index(FOLDER_S *folder, int index, FLIST *flist)
{
    register FOLDER_S **flp, **iflp;

    /* if index is beyond size, place at end of list */
    index = MIN(index, flist->used);

    /* grow array ? */
    if(flist->used + 1 > flist->allocated){
	flist->allocated += FCHUNK;
	fs_resize((void **)&(flist->folders),
		  flist->allocated * sizeof(FOLDER_S *));
    }

    /* shift array left */
    iflp = &((flist->folders)[index]);
    for(flp = &((flist->folders)[flist->used]); 
	flp > iflp; flp--)
      flp[0] = flp[-1];

    flist->folders[index] = folder;
    flist->used          += 1;
}


void
resort_folder_list(FLIST *flist)
{
    if(flist && folder_total(flist) > 1 && flist->folders)
      qsort(flist->folders, folder_total(flist), sizeof(flist->folders[0]),
	    (ps_global->fld_sort_rule == FLD_SORT_ALPHA_DIR_FIRST)
		? compare_folders_dir_alpha_qsort
		: (ps_global->fld_sort_rule == FLD_SORT_ALPHA_DIR_LAST)
		     ? compare_folders_alpha_dir_qsort
		     : compare_folders_alpha_qsort);
}


/*----------------------------------------------------------------------
    Removes a folder at the given index in the given context's
    list.

Args: index -- Index in folder list of folder to be removed
      flist -- folder list
 ----*/
void
folder_delete(int index, FLIST *flist)
{
    register int  i;
    FOLDER_S     *f;

    if(flist->used 
       && (index < 0 || index >= flist->used))
      return;				/* bogus call! */

    if((f = folder_entry(index, flist))->nickname)
      fs_give((void **)&(f->nickname));
      
    fs_give((void **) &(flist->folders[index]));
    for(i = index; i < flist->used - 1; i++)
      flist->folders[i] = flist->folders[i+1];


    flist->used -= 1;
}


/*----------------------------------------------------------------------
      compare two names for qsort, case independent

   Args: pointers to strings to compare

 Result: integer result of strcmp of the names.  Uses simple 
         efficiency hack to speed the string comparisons up a bit.

  ----------------------------------------------------------------------*/
int
compare_names(const qsort_t *x, const qsort_t *y)
{
    char *a = *(char **)x, *b = *(char **)y;
    int r;
#define	CMPI(X,Y)	((X)[0] - (Y)[0])
#define	UCMPI(X,Y)	((isupper((unsigned char)((X)[0]))	\
				? (X)[0] - 'A' + 'a' : (X)[0])	\
			  - (isupper((unsigned char)((Y)[0]))	\
				? (Y)[0] - 'A' + 'a' : (Y)[0]))

    /*---- Inbox always sorts to the top ----*/
    if(UCMPI(a, ps_global->inbox_name) == 0
       && strucmp(a, ps_global->inbox_name) == 0)
      return((CMPI(a, b) == 0 && strcmp(a, b) == 0) ? 0 : -1);
    else if((UCMPI(b, ps_global->inbox_name)) == 0
       && strucmp(b, ps_global->inbox_name) == 0)
      return((CMPI(a, b) == 0 && strcmp(a, b) == 0) ? 0 : 1);

    /*----- The sent-mail folder, is always next unless... ---*/
    else if(F_OFF(F_SORT_DEFAULT_FCC_ALPHA, ps_global)
	    && CMPI(a, ps_global->VAR_DEFAULT_FCC) == 0
	    && strcmp(a, ps_global->VAR_DEFAULT_FCC) == 0)
      return((CMPI(a, b) == 0 && strcmp(a, b) == 0) ? 0 : -1);
    else if(F_OFF(F_SORT_DEFAULT_FCC_ALPHA, ps_global)
	    && CMPI(b, ps_global->VAR_DEFAULT_FCC) == 0
	    && strcmp(b, ps_global->VAR_DEFAULT_FCC) == 0)
      return((CMPI(a, b) == 0 && strcmp(a, b) == 0) ? 0 : 1);

    /*----- The saved-messages folder, is always next unless... ---*/
    else if(F_OFF(F_SORT_DEFAULT_SAVE_ALPHA, ps_global)
	    && CMPI(a, ps_global->VAR_DEFAULT_SAVE_FOLDER) == 0
	    && strcmp(a, ps_global->VAR_DEFAULT_SAVE_FOLDER) == 0)
      return((CMPI(a, b) == 0 && strcmp(a, b) == 0) ? 0 : -1);
    else if(F_OFF(F_SORT_DEFAULT_SAVE_ALPHA, ps_global)
	    && CMPI(b, ps_global->VAR_DEFAULT_SAVE_FOLDER) == 0
	    && strcmp(b, ps_global->VAR_DEFAULT_SAVE_FOLDER) == 0)
      return((CMPI(a, b) == 0 && strcmp(a, b) == 0) ? 0 : 1);

    else
      return((r = CMPI(a, b)) ? r : strcmp(a, b));
}


/*----------------------------------------------------------------------
      compare two folder structs for ordering alphabetically

   Args: pointers to folder structs to compare

 Result: integer result of dir-bit and strcmp of the folders.
  ----------------------------------------------------------------------*/
int
compare_folders_alpha(FOLDER_S *f1, FOLDER_S *f2)
{
    int	  i;
    char *f1name = FLDR_NAME(f1),
	 *f2name = FLDR_NAME(f2);

    return(((i = compare_names(&f1name, &f2name)) != 0)
	       ? i : (f2->isdir - f1->isdir));
}


int
compare_folders_alpha_qsort(const qsort_t *a1, const qsort_t *a2)
{
    FOLDER_S *f1 = *((FOLDER_S **) a1);
    FOLDER_S *f2 = *((FOLDER_S **) a2);

    return(compare_folders_alpha(f1, f2));
}


/*----------------------------------------------------------------------
      compare two folder structs alphabetically with dirs first

   Args: pointers to folder structs to compare

 Result: integer result of dir-bit and strcmp of the folders.
  ----------------------------------------------------------------------*/
int
compare_folders_dir_alpha(FOLDER_S *f1, FOLDER_S *f2)
{
    int	  i;

    if((i = (f2->isdir - f1->isdir)) == 0){
	char *f1name = FLDR_NAME(f1),
	     *f2name = FLDR_NAME(f2);

	return(compare_names(&f1name, &f2name));
    }

    return(i);
}


int
compare_folders_dir_alpha_qsort(const qsort_t *a1, const qsort_t *a2)
{
    FOLDER_S *f1 = *((FOLDER_S **) a1);
    FOLDER_S *f2 = *((FOLDER_S **) a2);

    return(compare_folders_dir_alpha(f1, f2));
}


/*----------------------------------------------------------------------
      compare two folder structs alphabetically with dirs last

   Args: pointers to folder structs to compare

 Result: integer result of dir-bit and strcmp of the folders.
  ----------------------------------------------------------------------*/
int
compare_folders_alpha_dir(FOLDER_S *f1, FOLDER_S *f2)
{
    int	  i;

    if((i = (f1->isdir - f2->isdir)) == 0){
	char *f1name = FLDR_NAME(f1),
	     *f2name = FLDR_NAME(f2);

	return(compare_names(&f1name, &f2name));
    }

    return(i);
}


int
compare_folders_alpha_dir_qsort(const qsort_t *a1, const qsort_t *a2)
{
    FOLDER_S *f1 = *((FOLDER_S **) a1);
    FOLDER_S *f2 = *((FOLDER_S **) a2);

    return(compare_folders_alpha_dir(f1, f2));
}


/*
 * Find incoming folders and update the unseen counts
 * if necessary.
 */
void
folder_unseen_count_updater(unsigned long flags)
{
  CONTEXT_S *ctxt;
  time_t oldest, started_checking;
  int ftotal, i, first = -1;
  FOLDER_S *f;

  /*
   * We would only do this if there is an incoming collection, the
   * user wants us to monitor, and we're in the folder screen.
   */
  if(ps_global->in_folder_screen && F_ON(F_ENABLE_INCOMING_CHECKING, ps_global)
     && (ctxt=ps_global->context_list) && ctxt->use & CNTXT_INCMNG
     && (ftotal = folder_total(FOLDERS(ctxt)))){
    /*
     * Search through the last_unseen_update times to find
     * the one that was updated longest ago, and start with
     * that one. We don't want to delay long doing these
     * checks so may only check some of them each time we
     * get called. An update time equal to 1 means don't check
     * this folder at all.
     */
    for(i = 0; i < ftotal; i++){
      f = folder_entry(i, FOLDERS(ctxt));
      if(f && LUU_YES(f->last_unseen_update)){
	first = i;
	oldest = f->last_unseen_update;
	break;
      }
    }

    /*
     * Now first is the first in the list that
     * should ever be checked. Next find the
     * one that should be checked next, the one
     * that was checked longest ago.
     */
    if(first >= 0){
      for(i = 1; i < ftotal; i++){
        f = folder_entry(i, FOLDERS(ctxt));
        if(f && LUU_YES(f->last_unseen_update) && f->last_unseen_update < oldest){
	  first = i;
	  oldest = f->last_unseen_update;
	}
      }
    }

    /* now first is the next one to be checked */

    started_checking = time(0);

    for(i = first; i < ftotal; i++){
      /* update the next one */
      f = folder_entry(i, FOLDERS(ctxt));
      if(f && LUU_YES(f->last_unseen_update)
         && (flags & UFU_FORCE
	     /* or it's been long enough and we've not been in this function too long */
	     || (((time(0) - f->last_unseen_update) >= ps_global->inc_check_interval)
		 && ((time(0) - started_checking) < MIN(4,ps_global->inc_check_timeout)))))
	update_folder_unseen(f, ctxt, flags, NULL);
    }

    for(i = 0; i < first; i++){
      f = folder_entry(i, FOLDERS(ctxt));
      if(f && LUU_YES(f->last_unseen_update)
         && (flags & UFU_FORCE
	     || (((time(0) - f->last_unseen_update) >= ps_global->inc_check_interval)
		 && ((time(0) - started_checking) < MIN(4,ps_global->inc_check_timeout)))))
	update_folder_unseen(f, ctxt, flags, NULL);
    }
  }
}


/*
 * Update the count of unseen in the FOLDER_S struct
 * for this folder. This will update if the time
 * interval has passed or if the FORCE flag is set.
 */
void
update_folder_unseen(FOLDER_S *f, CONTEXT_S *ctxt, unsigned long flags,
		     MAILSTREAM *this_is_the_stream)
{
    time_t now;
    int orig_valid;
    int use_imap_interval = 0;
    int stream_is_open = 0;
    unsigned long orig_unseen, orig_new, orig_tot;
    char mailbox_name[MAILTMPLEN];
    char *target = NULL;
    DRIVER *d;

    if(!f || !LUU_YES(f->last_unseen_update))
      return;

    now = time(0);
    context_apply(mailbox_name, ctxt, f->name, MAILTMPLEN);

    if(!mailbox_name[0])
      return;

    if(check_for_move_mbox(mailbox_name, NULL, 0, &target)){
	MAILSTREAM *strm;

	/*
	 * If this maildrop is the currently open stream use that.
	 * I'm not altogether sure that this is a good way to
	 * check this.
	 */
	if(target
	   && ((strm=ps_global->mail_stream)
		&& strm->snarf.name
		&& (!strcmp(target,strm->mailbox)
		    || !strcmp(target,strm->original_mailbox)))){
	    stream_is_open++;
	}
    }
    else{
	MAILSTREAM *m = NULL;

	stream_is_open = (this_is_the_stream
			  || (m=sp_stream_get(mailbox_name, SP_MATCH | SP_RO_OK))
			  || ((m=ps_global->mail_stream) && !sp_dead_stream(m)
			      && same_stream_and_mailbox(mailbox_name,m))
	                  || (!IS_REMOTE(mailbox_name)
			      && (m=already_open_stream(mailbox_name, AOS_NONE)))) ? 1 : 0;

	if(stream_is_open){
	    if(!this_is_the_stream)
	      this_is_the_stream = m;
	}
	else{
	    /*
	     * If it's IMAP or local we use a shorter interval.
	     */
	    d = mail_valid(NIL, mailbox_name, (char *) NIL);
	    if((d && !strcmp(d->name, "imap")) || !IS_REMOTE(mailbox_name))
	      use_imap_interval++;
	}
    }

    /*
     * Update if forced, or if it's been a while, or if we have a
     * stream open to this mailbox already.
     */
    if(flags & UFU_FORCE
       || stream_is_open
       || ((use_imap_interval
	    && (now - f->last_unseen_update) >= ps_global->inc_check_interval)
	   || ((now - f->last_unseen_update) >= ps_global->inc_second_check_interval))){
	unsigned long tot, uns, new;
	unsigned long *totp = NULL, *unsp = NULL, *newp = NULL;

	orig_valid  = f->unseen_valid;
	orig_unseen = f->unseen;
	orig_new    = f->new;
	orig_tot    = f->total;

	if(F_ON(F_INCOMING_CHECKING_RECENT, ps_global))
	  newp = &new;
	else
	  unsp = &uns;

	if(F_ON(F_INCOMING_CHECKING_TOTAL, ps_global))
	  totp = &tot;

	f->unseen_valid = 0;

	dprint((9, "update_folder_unseen(%s)", FLDR_NAME(f)));
	if(get_recent_in_folder(mailbox_name, newp, unsp, totp, this_is_the_stream)){
	    f->last_unseen_update = time(0);
	    f->unseen_valid = 1;
	    if(unsp)
	      f->unseen = uns;

	    if(newp)
	      f->new = new;

	    if(totp)
	      f->total = tot;

	    if(!orig_valid){
		dprint((9, "update_folder_unseen(%s): original: %s%s%s%s",
		    FLDR_NAME(f),
		    F_ON(F_INCOMING_CHECKING_RECENT,ps_global) ? "new=" : "unseen=",
		    F_ON(F_INCOMING_CHECKING_RECENT,ps_global) ? comatose(f->new) : comatose(f->unseen),
		    F_ON(F_INCOMING_CHECKING_TOTAL,ps_global) ? " tot=" : "",
		    F_ON(F_INCOMING_CHECKING_TOTAL,ps_global) ? comatose(f->total) : ""));
	    }

	    if(orig_valid
	       && ((F_ON(F_INCOMING_CHECKING_RECENT, ps_global)
		    && orig_new != f->new)
		   ||
	           (F_OFF(F_INCOMING_CHECKING_RECENT, ps_global)
		    && orig_unseen != f->unseen)
		   ||
	           (F_ON(F_INCOMING_CHECKING_TOTAL, ps_global)
		    && orig_tot != f->total))){

		if(ps_global->in_folder_screen)
		  ps_global->noticed_change_in_unseen = 1;

		dprint((9, "update_folder_unseen(%s): changed: %s%s%s%s",
		    FLDR_NAME(f),
		    F_ON(F_INCOMING_CHECKING_RECENT,ps_global) ? "new=" : "unseen=",
		    F_ON(F_INCOMING_CHECKING_RECENT,ps_global) ? comatose(f->new) : comatose(f->unseen),
		    F_ON(F_INCOMING_CHECKING_TOTAL,ps_global) ? " tot=" : "",
		    F_ON(F_INCOMING_CHECKING_TOTAL,ps_global) ? comatose(f->total) : ""));

		if(flags & UFU_ANNOUNCE
		   && ((F_ON(F_INCOMING_CHECKING_RECENT, ps_global)
			&& orig_new < f->new)
		       ||
		       (F_OFF(F_INCOMING_CHECKING_RECENT, ps_global)
			&& orig_unseen < f->unseen))){
		    if(F_ON(F_INCOMING_CHECKING_RECENT, ps_global))
		      q_status_message3(SM_ASYNC, 1, 3, "%s: %s %s",
					FLDR_NAME(f), comatose(f->new),
					_("new"));
		    else
		      q_status_message3(SM_ASYNC, 1, 3, "%s: %s %s",
					FLDR_NAME(f), comatose(f->unseen),
					_("unseen"));
		}
	    }
	}
	else
	  f->last_unseen_update = LUU_NOMORECHK;	/* no further checking */
    }
}


void
update_folder_unseen_by_stream(MAILSTREAM *strm, unsigned long flags)
{
  CONTEXT_S *ctxt;
  int ftotal, i;
  char mailbox_name[MAILTMPLEN];
  char *cn, tmp[MAILTMPLEN];
  FOLDER_S *f;

  /*
   * Attempt to figure out which incoming folder this stream
   * is open to, if any, so we can update the unseen counters.
   */
  if(strm
     && F_ON(F_ENABLE_INCOMING_CHECKING, ps_global)
     && (ctxt=ps_global->context_list) && ctxt->use & CNTXT_INCMNG
     && (ftotal = folder_total(FOLDERS(ctxt)))){
    for(i = 0; i < ftotal; i++){
      f = folder_entry(i, FOLDERS(ctxt));
      context_apply(mailbox_name, ctxt, f->name, MAILTMPLEN);
      if(same_stream_and_mailbox(mailbox_name, strm)
         || (!IS_REMOTE(mailbox_name) && (cn=mailboxfile(tmp,mailbox_name)) && (*cn) && (!strcmp(cn, strm->mailbox) || !strcmp(cn, strm->original_mailbox)))){
	/* if we failed earlier on this one, give it another go */
	if(f->last_unseen_update == LUU_NOMORECHK)
	  init_incoming_unseen_data(ps_global, f);

	update_folder_unseen(f, ctxt, flags, strm);
	return;
      }
    }
  }
}


/*
 * Find the number of new, unseen, and the total number of
 * messages in mailbox_name.
 * If the corresponding arg is NULL it will skip the work
 * necessary for that flag.
 *
 * Returns 1 if successful, 0 if not.
 */
int
get_recent_in_folder(char *mailbox_name, long unsigned int *new,
		     long unsigned int *unseen, long unsigned int *total,
		     MAILSTREAM *this_is_the_stream)
{
    MAILSTREAM   *strm = NIL;
    unsigned long tot, nw, uns;
    int           gotit = 0;
    int           maildrop = 0;
    char         *target = NULL;
    MSGNO_S      *msgmap;
    long          excluded, flags;
    extern MAILSTATUS mm_status_result;

    dprint((9, "get_recent_in_folder(%s)", mailbox_name ? mailbox_name : "?"));

    if(check_for_move_mbox(mailbox_name, NULL, 0, &target)){

	maildrop++;

	/*
	 * If this maildrop is the currently open stream use that.
	 */
	if(target
	   && ((strm=ps_global->mail_stream)
		&& strm->snarf.name
		&& (!strcmp(target,strm->mailbox)
		    || !strcmp(target,strm->original_mailbox)))){
	    gotit++;
	    msgmap = sp_msgmap(strm);
	    excluded = any_lflagged(msgmap, MN_EXLD);

	    tot = strm->nmsgs - excluded;
	    if(tot){
	      if(new){
		  if(sp_recent_since_visited(strm) == 0)
		    nw = 0;
		  else
		    nw = count_flagged(strm, F_RECENT | F_UNSEEN | F_UNDEL);
	      }

	      if(unseen)
		uns = count_flagged(strm, F_UNSEEN | F_UNDEL);
	    }
	    else{
	      nw = 0;
	      uns = 0;
	    }
	}
	/* else fall through to just open it case */
    }

    /* do we already have it selected? */
    if(!gotit
       && ((strm = this_is_the_stream)
           || (strm = sp_stream_get(mailbox_name, SP_MATCH | SP_RO_OK))
           || (!IS_REMOTE(mailbox_name)
	       && (strm = already_open_stream(mailbox_name, AOS_NONE))))){
	gotit++;

	/*
	 * Unfortunately, we have to worry about excluded
	 * messages. The user doesn't want to have
	 * excluded messages count in the totals, especially
	 * recent excluded messages.
	 */

	msgmap = sp_msgmap(strm);
	excluded = any_lflagged(msgmap, MN_EXLD);

	tot = strm->nmsgs - excluded;
	if(tot){
	  if(new){
	      if(sp_recent_since_visited(strm) == 0)
		nw = 0;
	      else
		nw = count_flagged(strm, F_RECENT | F_UNSEEN | F_UNDEL);
	  }

	  if(unseen)
	    uns = count_flagged(strm, F_UNSEEN | F_UNDEL);
	}
	else{
	  nw = 0;
	  uns = 0;
	}
    }
    /*
     * No, but how about another stream to same server which
     * could be used for a STATUS command?
     */
    else if(!gotit && (strm = sp_stream_get(mailbox_name, SP_SAME))
	    && modern_imap_stream(strm)){

	flags = 0L;
	if(total)
	  flags |= SA_MESSAGES;

	if(new)
	  flags |= SA_RECENT;

	if(unseen)
	  flags |= SA_UNSEEN;

	mm_status_result.flags = 0L;

	pine_mail_status(strm, mailbox_name, flags);
	if(total){
	    if(mm_status_result.flags & SA_MESSAGES){
		tot = mm_status_result.messages;
		gotit++;
	    }
	}

	if(!(total && !gotit)){
	    if(new){
		if(mm_status_result.flags & SA_RECENT){
		    nw = mm_status_result.recent;
		    gotit++;
		}
		else
		  gotit = 0;
	    }
	}

	if(!((total || new) && !gotit)){
	    if(unseen){
		if(mm_status_result.flags & SA_UNSEEN){
		    uns = mm_status_result.unseen;
		    gotit++;
		}
		else
		  gotit = 0;
	    }
	}
    }

    /* Let's just Select it. */
    if(!gotit){
	long saved_timeout;
	long openflags;

	/* 
	 * Traditional unix folders don't notice new mail if
	 * they are opened readonly. So maildrops with unix folder
	 * targets will snarf to the file but the stream that is
	 * opened won't see the new mail. So make all maildrop
	 * opens non-readonly here.
	 */
	openflags = SP_USEPOOL | SP_TEMPUSE | (maildrop ? 0 : OP_READONLY);

	saved_timeout = (long) mail_parameters(NULL, GET_OPENTIMEOUT, NULL);
	mail_parameters(NULL, SET_OPENTIMEOUT, (void *) (long) ps_global->inc_check_timeout);
	strm = pine_mail_open(NULL, mailbox_name, openflags, NULL);
	mail_parameters(NULL, SET_OPENTIMEOUT, (void *) saved_timeout);

	if(strm){
	    gotit++;
	    msgmap = sp_msgmap(strm);
	    excluded = any_lflagged(msgmap, MN_EXLD);

	    tot = strm->nmsgs - excluded;
	    if(tot){
	      if(new){
		  if(sp_recent_since_visited(strm) == 0)
		    nw = 0;
		  else
		    nw = count_flagged(strm, F_RECENT | F_UNSEEN | F_UNDEL);
	      }

	      if(unseen)
		uns = count_flagged(strm, F_UNSEEN | F_UNDEL);
	    }
	    else{
	      nw = 0;
	      uns = 0;
	    }

	    pine_mail_close(strm);
	}
    }

    if(gotit){
	if(new)
	  *new = nw;

	if(unseen)
	  *unseen = uns;

	if(total)
	  *total = tot;
    }

    return(gotit);
}


void
clear_incoming_valid_bits(void)
{
    CONTEXT_S *ctxt;
    int ftotal, i;
    FOLDER_S *f;

    if(F_ON(F_ENABLE_INCOMING_CHECKING, ps_global)
       && (ctxt=ps_global->context_list) && ctxt->use & CNTXT_INCMNG
       && (ftotal = folder_total(FOLDERS(ctxt))))
      for(i = 0; i < ftotal; i++){
	  f = folder_entry(i, FOLDERS(ctxt));
	  init_incoming_unseen_data(ps_global, f);
      }
}


int
selected_folders(CONTEXT_S *context)
{
    int	      i, n, total;

    n = folder_total(FOLDERS(context));
    for(total = i = 0; i < n; i++)
      if(folder_entry(i, FOLDERS(context))->selected)
	total++;

    return(total);
}


SELECTED_S *
new_selected(void)
{
  SELECTED_S *selp;
  
  selp = (SELECTED_S *)fs_get(sizeof(SELECTED_S));
  selp->sub = NULL;
  selp->reference = NULL;
  selp->folders = NULL;
  selp->zoomed = 0;

  return selp;
}


/*
 *  Free the current selected struct and all of the 
 *  following structs in the list
 */
void
free_selected(SELECTED_S **selp)
{
    if(!selp || !(*selp))
      return;
    if((*selp)->sub)
      free_selected(&((*selp)->sub));

    free_strlist(&(*selp)->folders);
    if((*selp)->reference)
      fs_give((void **) &(*selp)->reference);

    fs_give((void **) selp);
}


void
folder_select_preserve(CONTEXT_S *context)
{
    if(context
       && !(context->use & CNTXT_PARTFIND)){
	FOLDER_S   *fp;
	STRLIST_S **slpp;
	SELECTED_S *selp = &context->selected;
	int	    i, folder_n;

	if(!context->dir->ref){
	  if(!context->selected.folders)
	    slpp     = &context->selected.folders;
	  else
	    return;
	}
	else{
	  if(!selected_folders(context))
	    return;
	  else{
	    while(selp->sub){
	      selp = selp->sub;
	      if(!strcmp(selp->reference, context->dir->ref))
		return;
	    }
	    selp->sub = new_selected();
	    selp = selp->sub;
	    slpp = &(selp->folders);
	  }
	}
	folder_n = folder_total(FOLDERS(context));

	for(i = 0; i < folder_n; i++)
	  if((fp = folder_entry(i, FOLDERS(context)))->selected){
	      *slpp = new_strlist(fp->name);
	      slpp = &(*slpp)->next;
	  }

	/* Only remember "ref" if any folders were selected */
	if(selp->folders && context->dir->ref)
	  selp->reference = cpystr(context->dir->ref);

	selp->zoomed = (context->use & CNTXT_ZOOM) != 0;
    }
}


int
folder_select_restore(CONTEXT_S *context)
{
    int rv = 0;

    if(context
       && !(context->use & CNTXT_PARTFIND)){
	STRLIST_S *slp;
	SELECTED_S *selp, *pselp = NULL;
	int	   i, found = 0;

	selp = &(context->selected);

	if(context->dir->ref){
	  pselp = selp;
	  selp = selp->sub;
	  while(selp && strcmp(selp->reference, context->dir->ref)){
	    pselp = selp;
	    selp = selp->sub;
	  }
	  if (selp)
	    found = 1;
	}
	else 
	  found = selp->folders != 0;
	if(found){
	  for(slp = selp->folders; slp; slp = slp->next)
	    if(slp->name
	       && (i = folder_index(slp->name, context, FI_FOLDER)) >= 0){
		folder_entry(i, FOLDERS(context))->selected = 1;
		rv++;
	    }

	  /* Used, always clean them up */
	  free_strlist(&selp->folders);
	  if(selp->reference)
	    fs_give((void **) &selp->reference);

	  if(selp->zoomed){
	    context->use |= CNTXT_ZOOM;
	    selp->zoomed = 0;
	  }
	  if(!(selp == &context->selected)){
	    if(pselp){
	      pselp->sub = selp->sub;
	      fs_give((void **) &selp);
	    }
	  }
	}
    }

    return(rv);
}

