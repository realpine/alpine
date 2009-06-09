#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: init.c 101 2006-08-10 22:53:04Z mikes@u.washington.edu $";
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
     init.c

  Session initiation support routines

 ====*/


#include "headers.h"

#include "../pith/init.h"
#include "../pith/conf.h"
#include "../pith/status.h"
#include "../pith/folder.h"
#include "../pith/context.h"

#include "init.h"


/* these are used to report folder directory creation problems */
CONF_TXT_T init_md_exists[] =	"The \"%s\" subdirectory already exists, but it is not writable by Alpine so Alpine cannot run.  Please correct the permissions and restart Alpine.";

CONF_TXT_T init_md_file[] =	"Alpine requires a directory called \"%s\" and usualy creates it.  You already have a regular file by that name which means Alpine cannot create the directory.  Please move or remove it and start Alpine again.";

CONF_TXT_T init_md_create[] =	"Creating subdirectory \"%s\" where Alpine will store its mail folders.";


/*
 * Internal prototypes
 */
void	 display_init_err(char *, int);
char	*context_string(char *);
int	 prune_folders(CONTEXT_S *, char *, int, char *, unsigned);
int	 prune_move_folder(char *, char *, CONTEXT_S *);
void	 delete_old_mail(struct sm_folder *, CONTEXT_S *, char *);



/*----------------------------------------------------------------------
    Make sure the alpine folders directory exists, with proper folders

   Args: ps -- alpine structure to get mail directory and contexts from

  Result: returns 0 if it exists or it is created and all is well
                  1 if it is missing and can't be created.
  ----*/
int
init_mail_dir(struct pine *ps)
{

/* MAIL_LIST: can we use imap4 CREATE? */
    /*
     * We don't really care if mail_dir exists if it isn't 
     * part of the first folder collection specified.  If this
     * is the case, it must have been created external to us, so
     * just move one...
     */
    if(ps->VAR_FOLDER_SPEC && ps->VAR_FOLDER_SPEC[0]){
	char *p  = context_string(ps->VAR_FOLDER_SPEC[0]);
	int   rv = strncmp(p, ps->VAR_MAIL_DIRECTORY,
			   strlen(ps->VAR_MAIL_DIRECTORY));
	fs_give((void **)&p);
	if(rv)
	  return(0);
    }

    switch(is_writable_dir(ps->folders_dir)){
      case 0:
        /* --- all is well --- */
	return(0);

      case 1:
	snprintf(tmp_20k_buf, SIZEOF_20KBUF, init_md_exists, ps->folders_dir);
	display_init_err(tmp_20k_buf, 1);
	return(-1);

      case 2:
	snprintf(tmp_20k_buf, SIZEOF_20KBUF, init_md_file, ps->folders_dir);
	display_init_err(tmp_20k_buf, 1);
	return(-1);

      case 3:
	snprintf(tmp_20k_buf, SIZEOF_20KBUF, init_md_create, ps->folders_dir);
	display_init_err(tmp_20k_buf, 0);
#ifndef	_WINDOWS
    	sleep(4);
#endif
        if(create_mail_dir(ps->folders_dir) < 0){
            snprintf(tmp_20k_buf, SIZEOF_20KBUF, "Error creating subdirectory \"%s\" : %s",
		    ps->folders_dir, error_description(errno));
	    display_init_err(tmp_20k_buf, 1);
            return(-1);
        }
    }

    return(0);
}


/*----------------------------------------------------------------------
  Make sure the default save folders exist in the default
  save context.
  ----*/
void
display_init_err(char *s, int err)
{
#ifdef	_WINDOWS
    mswin_messagebox(s, err);
#else
    int n = 0;

    if(err)
      fputc(BELL, stdout);

    for(; *s; s++)
      if(++n > 60 && isspace((unsigned char)*s)){
	  n = 0;
	  fputc('\n', stdout);
	  while(*(s+1) && isspace((unsigned char)*(s+1)))
	    s++;
      }
      else
	fputc(*s, stdout);

    fputc('\n', stdout);
#endif
}


/*
 * Return malloc'd string containing only the context specifier given
 * a string containing the raw pinerc entry
 */
char *
context_string(char *s)
{
    CONTEXT_S *t = new_context(s, NULL);
    char      *rv = NULL;

    if(t){
	/*
	 * Take what we want from context, then free the rest...
	 */
	rv = t->context;
	t->context = NULL;			/* don't free it! */
	free_context(&t);
    }

    return(rv ? rv : cpystr(""));
}



/*----------------------------------------------------------------------
      Rename the current sent-mail folder to sent-mail for last month

   open up sent-mail and get date of very first message
   if date is last month rename and...
       if files from 3 months ago exist ask if they should be deleted and...
           if files from previous months and yes ask about them, too.   
  ----------------------------------------------------------------------*/
int
expire_sent_mail(void)
{
    int		 cur_month, ok = 1;
    time_t	 now;
    char	 tmp[50], **p;
    struct tm	*tm_now;
    CONTEXT_S	*prune_cntxt;

    dprint((5, "==== expire_mail called ====\n"));

    if(!check_prune_time(&now, &tm_now))
      return 0;

    cur_month = (1900 + tm_now->tm_year) * 12 + tm_now->tm_mon;
    dprint((5, "Current month %d\n", cur_month));

    /*
     * locate the default save context...
     */
    if(!(prune_cntxt = default_save_context(ps_global->context_list)))
      prune_cntxt = ps_global->context_list;

    /*
     * Since fcc's and read-mail can be an IMAP mailbox, be sure to only
     * try expiring a list if it's an ambiguous name associated with some
     * collection...
     *
     * If sentmail set outside a context, then pruning is up to the
     * user...
     */
    if(prune_cntxt){
	if(ps_global->VAR_DEFAULT_FCC && *ps_global->VAR_DEFAULT_FCC
	   && context_isambig(ps_global->VAR_DEFAULT_FCC))
	  ok = prune_folders(prune_cntxt, ps_global->VAR_DEFAULT_FCC,
			     cur_month, " SENT",
			     ps_global->pruning_rule);

	if(ok && ps_global->VAR_READ_MESSAGE_FOLDER 
	   && *ps_global->VAR_READ_MESSAGE_FOLDER
	   && context_isambig(ps_global->VAR_READ_MESSAGE_FOLDER))
	  ok = prune_folders(prune_cntxt, ps_global->VAR_READ_MESSAGE_FOLDER,
			     cur_month, " READ",
			     ps_global->pruning_rule);
    }

    /*
     * Within the default prune context,
     * prune back the folders with the given name
     */
    if(ok && prune_cntxt && (p = ps_global->VAR_PRUNED_FOLDERS))
      for(; ok && *p; p++)
	if(**p && context_isambig(*p))
	  ok = prune_folders(prune_cntxt, *p, cur_month, "",
			     ps_global->pruning_rule);

    /*
     * Mark that we're done for this month...
     */
    if(ok){
	ps_global->last_expire_year = tm_now->tm_year;
	ps_global->last_expire_month = tm_now->tm_mon;
	snprintf(tmp, sizeof(tmp), "%d.%d", ps_global->last_expire_year,
		ps_global->last_expire_month + 1);
	set_variable(V_LAST_TIME_PRUNE_QUESTION, tmp, 1, 1, Main);
    }

    return(1);
}


/*----------------------------------------------------------------------
     Offer to delete old sent-mail folders

  Args: sml -- The list of sent-mail folders
 
  ----*/
int
prune_folders(CONTEXT_S *prune_cntxt, char *folder_base, int cur_month,
	      char *type, unsigned int pr)
{
    char         path2[MAXPATH+1],  prompt[128], tmp[21];
    int          month_to_use, exists;
    struct sm_folder *mail_list, *sm;

    mail_list = get_mail_list(prune_cntxt, folder_base);
    free_folder_list(prune_cntxt);

#ifdef	DEBUG
    for(sm = mail_list; sm != NULL && sm->name != NULL; sm++)
      dprint((5,"Old sent-mail: %5d  %s\n", sm->month_num,
	     sm->name[0] ? sm->name : "?"));
#endif

    for(sm = mail_list; sm != NULL && sm->name != NULL; sm++)
      if(sm->month_num == cur_month - 1)
        break;  /* matched a month */
 
    month_to_use = (sm == NULL || sm->name == NULL) ? cur_month - 1 : 0;

    dprint((5, "Month_to_use : %d\n", month_to_use));

    if(month_to_use == 0 || pr == PRUNE_NO_AND_ASK || pr == PRUNE_NO_AND_NO)
      goto delete_old;

    strncpy(path2, folder_base, sizeof(path2)-1);
    path2[sizeof(path2)-1] = '\0';

    if(F_ON(F_PRUNE_USES_ISO,ps_global)){             /* sent-mail-yyyy-mm */
	snprintf(path2 + strlen(path2), sizeof(path2)-strlen(path2), "-%4.4d-%2.2d", month_to_use/12,
		month_to_use % 12 + 1);
    }
    else{
	strncpy(tmp, month_abbrev((month_to_use % 12)+1), 20);
	tmp[sizeof(tmp)-1] = '\0';
	lcase((unsigned char *) tmp);
#ifdef	DOS
	if(*prune_cntxt->context != '{'){
	  int i;

	  i = strlen(path2);
	  snprintf(path2 + (size_t)((i > 4) ? 4 : i),
	           sizeof(path2)- ((i > 4) ? 4 : i),
		  "%2.2d%2.2d", (month_to_use % 12) + 1,
		  ((month_to_use / 12) - 1900) % 100);
	}
	else
#endif
	snprintf(path2 + strlen(path2), sizeof(path2)-strlen(path2), "-%.20s-%d", tmp, month_to_use/12);
    }

    Writechar(BELL, 0);
    snprintf(prompt, sizeof(prompt), "Move current \"%.50s\" to \"%.50s\"", folder_base, path2);
    if((exists = folder_exists(prune_cntxt, folder_base)) == FEX_ERROR){
        dprint((5, "prune_folders: Error testing existence\n"));
        return(0);
    }
    else if(exists == FEX_NOENT){	/* doesn't exist */
        dprint((5, "prune_folders: nothing to prune <%s %s>\n",
		   prune_cntxt->context ? prune_cntxt->context : "?",
		   folder_base ? folder_base : "?"));
        goto delete_old;
    }
    else if(!(exists & FEX_ISFILE)
	    || pr == PRUNE_NO_AND_ASK || pr == PRUNE_NO_AND_NO
	    || ((pr == PRUNE_ASK_AND_ASK || pr == PRUNE_ASK_AND_NO) &&
	        want_to(prompt, 'n', 0, h_wt_expire, WT_FLUSH_IN) == 'n')){
	dprint((5, "User declines renaming %s\n",
	       ps_global->VAR_DEFAULT_FCC ? ps_global->VAR_DEFAULT_FCC : "?"));
	goto delete_old;
    }

    prune_move_folder(folder_base, path2, prune_cntxt);

  delete_old:
    if(pr == PRUNE_ASK_AND_ASK || pr == PRUNE_YES_AND_ASK
       || pr == PRUNE_NO_AND_ASK)
      delete_old_mail(mail_list, prune_cntxt, type);

    if((sm = mail_list) != NULL){
	while(sm->name){
	    fs_give((void **)&(sm->name));
	    sm++;
	}

        fs_give((void **)&mail_list);
    }

    return(1);
}


/*----------------------------------------------------------------------
     Offer to delete old sent-mail folders

  Args: sml       -- The list of sent-mail folders
        fcc_cntxt -- context to delete list of folders in
        type      -- label indicating type of folders being deleted
 
  ----*/
void
delete_old_mail(struct sm_folder *sml, CONTEXT_S *fcc_cntxt, char *type)
{
    char  prompt[150];
    struct sm_folder *sm;

    for(sm = sml; sm != NULL && sm->name != NULL; sm++){
	if(sm->name[0] == '\0')		/* can't happen */
	  continue;

        snprintf(prompt, sizeof(prompt),
	       "To save disk space, delete old%.10s mail folder \"%.30s\" ",
	       type, sm->name);
        if(want_to(prompt, 'n', 0, h_wt_delete_old, WT_FLUSH_IN) == 'y'){

	    if(!context_delete(fcc_cntxt, NULL, sm->name)){
		q_status_message1(SM_ORDER,
				  3, 3, "Error deleting \"%s\".", sm->name);
		dprint((1, "Error context_deleting %s in %s\n", sm->name,
			   (fcc_cntxt && fcc_cntxt->context)
				     ? fcc_cntxt->context : "<null>"));
            }
	    else{
		int index;

		if((index = folder_index(sm->name, fcc_cntxt, FI_FOLDER)) >= 0)
		  folder_delete(index, FOLDERS(fcc_cntxt));
	    }
	}else{
		/* break;*/ /* skip out of the whole thing when he says no */
		/* Decided to keep asking anyway */
        }
    }
}



