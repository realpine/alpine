#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: init.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
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
     Routines for pine start up and initialization
  ====*/


#include "../pith/headers.h"
#include "../pith/init.h"
#include "../pith/conf.h"
#include "../pith/status.h"
#include "../pith/folder.h"


/*
 * Internal prototypes
 */
int	 compare_sm_files(const qsort_t *, const qsort_t *);



/*----------------------------------------------------------------------
    Sets  login, full_username and home_dir

   Args: ps -- The Pine structure to put the user name, etc in

  Result: sets the fullname, login and home_dir field of the pine structure
          returns 0 on success, -1 if not.
  ----*/

int
init_username(struct pine *ps)
{
    char *expanded;
    int	  rv;

    rv       = 0;
    expanded = NULL;
#if defined(DOS) || defined(OS2)
    if(ps->COM_USER_ID)
      expanded = expand_variables(tmp_20k_buf, SIZEOF_20KBUF,
				  ps->COM_USER_ID, 0);
    
    if(!expanded && ps->vars[V_USER_ID].post_user_val.p)
      expanded = expand_variables(tmp_20k_buf, SIZEOF_20KBUF,
				  ps->vars[V_USER_ID].post_user_val.p, 0);

    if(!expanded && ps->vars[V_USER_ID].main_user_val.p)
      expanded = expand_variables(tmp_20k_buf, SIZEOF_20KBUF,
				  ps->vars[V_USER_ID].main_user_val.p, 0);

    if(!expanded)
      ps->blank_user_id = 1;

    ps->VAR_USER_ID = cpystr(expanded ? expanded : "");
#else
    ps->VAR_USER_ID = cpystr(ps->ui.login);
    if(!ps->VAR_USER_ID[0]){
        fprintf(stderr, "Who are you? (Unable to look up login name)\n");
        rv = -1;
    }
#endif

    expanded = NULL;
    if(ps->vars[V_PERSONAL_NAME].is_fixed){
	if(ps->FIX_PERSONAL_NAME){
            expanded = expand_variables(tmp_20k_buf, SIZEOF_20KBUF,
					ps->FIX_PERSONAL_NAME, 0);
	}
	if(ps->vars[V_PERSONAL_NAME].main_user_val.p ||
	   ps->vars[V_PERSONAL_NAME].post_user_val.p){
	    ps_global->give_fixed_warning = 1;
	    ps_global->fix_fixed_warning = 1;
	}
	else if(ps->COM_PERSONAL_NAME)
	  ps_global->give_fixed_warning = 1;
    }
    else{
	if(ps->COM_PERSONAL_NAME)
	  expanded = expand_variables(tmp_20k_buf, SIZEOF_20KBUF,
				      ps->COM_PERSONAL_NAME, 0);

	if(!expanded && ps->vars[V_PERSONAL_NAME].post_user_val.p)
	  expanded = expand_variables(tmp_20k_buf, SIZEOF_20KBUF,
			      ps->vars[V_PERSONAL_NAME].post_user_val.p, 0);

	if(!expanded && ps->vars[V_PERSONAL_NAME].main_user_val.p)
	  expanded = expand_variables(tmp_20k_buf, SIZEOF_20KBUF,
			      ps->vars[V_PERSONAL_NAME].main_user_val.p, 0);
    }

    if(!expanded){
	expanded = ps->ui.fullname;
#if defined(DOS) || defined(OS2)
	ps->blank_personal_name = 1;
#endif
    }

    ps->VAR_PERSONAL_NAME = cpystr(expanded ? expanded : "");

    dprint((1, "Userid: %s\nFullname: \"%s\"\n",
               ps->VAR_USER_ID, ps->VAR_PERSONAL_NAME));
    return(rv);
}


/*----------------------------------------------------------------------
    Sets  home_dir

   Args: ps -- The Pine structure to put the user name, etc in

  Result: sets the home_dir field of the pine structure
          returns 0 on success, -1 if not.
  ----*/

int
init_userdir(struct pine *ps)
{
    char fld_dir[MAXPATH+1];

    if(strlen(ps->home_dir) + strlen(ps->VAR_MAIL_DIRECTORY)+2 > MAXPATH){
        printf(_("Folders directory name is longer than %d\n"), MAXPATH);
        printf(_("Directory name: \"%s/%s\"\n"),ps->home_dir,
               ps->VAR_MAIL_DIRECTORY);
        return(-1);
    }
#if defined(DOS) || defined(OS2)
    if(ps->VAR_MAIL_DIRECTORY[1] == ':'){
	strncpy(fld_dir, ps->VAR_MAIL_DIRECTORY, sizeof(fld_dir)-1);
	fld_dir[sizeof(fld_dir)-1] = '\0';
    }
    else
#endif
    build_path(fld_dir, ps->home_dir, ps->VAR_MAIL_DIRECTORY, sizeof(fld_dir));
    ps->folders_dir = cpystr(fld_dir);

    return(0);
}


/*----------------------------------------------------------------------
        Fetch the hostname of the current system and put it in pine struct

   Args: ps -- The pine structure to put the hostname, etc in

  Result: hostname, localdomain, userdomain and maildomain are set


** Pine uses the following set of names:
  hostname -    The fully-qualified hostname.  Obtained with
		gethostbyname() which reads /etc/hosts or does a DNS
		lookup.  This may be blank.
  localdomain - The domain name without the host.  Obtained from the
		above hostname if it has a "." in it.  Removes first
		segment.  If hostname has no "." in it then the hostname
		is used.  This may be blank.
  userdomain -  Explicitly configured domainname.  This is read out of the
		global pine.conf or user's .pinerc.  The user's entry in the
		.pinerc overrides.

** Pine has the following uses for such names:

  1. On outgoing messages in the From: line
	Uses userdomain if there is one.  If not uses, uses
	hostname unless Pine has been configured to use localdomain.

  2. When expanding/fully-qualifying unqualified addresses during
     composition
	(same as 1)

  3. When expanding/fully-qualifying unqualified addresses during
     composition when a local entry in the password file exists for
     name.
        If no userdomain is given, then this lookup is always done
	and the hostname is used unless Pine's been configured to 
	use the localdomain.  If userdomain is defined, it is used,
	but no local lookup is done.  We can't assume users on the
	local host are valid in the given domain (and, for simplicity,
	have chosen to ignore the cases userdomain matches localdomain
	or localhost).  Setting user-lookup-even-if-domain-mismatch
	feature will tell pine to override this behavior and perform
	the local lookup anyway.  The problem of a global "even-if"
	set and a .pinerc-defined user-domain of something odd causing
	the local lookup, but this will only effect the personal name, 
	and is not judged to be a significant problem.

  4. In determining if an address is that of the current pine user for
     formatting index and filtering addresses when replying
	If a userdomain is specified the address must match the
	userdomain exactly.  If a userdomain is not specified or the
	userdomain is the same as the hostname or domainname, then
	an address will be considered the users if it matches either
	the domainname or the hostname.  Of course, the userid must
	match too. 

  5. In Message ID's
	The fully-qualified hostname is always users here.


** Setting the domain names
  To set the domain name for all Pine users on the system to be
different from what Pine figures out from DNS, set the domain name in
the "user-domain" variable in pine.conf.  To set the domain name for an
individual user, set the "user-domain" variable in his .pinerc.
The .pinerc setting overrides any other setting.
 ----*/
int
init_hostname(struct pine *ps)
{
    char hostname[MAX_ADDRESS+1], domainname[MAX_ADDRESS+1];

    getdomainnames(hostname, sizeof(hostname)-1,
		   domainname, sizeof(domainname)-1);

    if(ps->hostname)
      fs_give((void **)&ps->hostname);

    ps->hostname = cpystr(hostname);

    if(ps->localdomain)
      fs_give((void **)&ps->localdomain);

    ps->localdomain = cpystr(domainname);
    ps->userdomain  = NULL;

    if(ps->VAR_USER_DOMAIN && ps->VAR_USER_DOMAIN[0]){
        ps->maildomain = ps->userdomain = ps->VAR_USER_DOMAIN;
    }else{
#if defined(DOS) || defined(OS2)
	if(ps->VAR_USER_DOMAIN)
	  ps->blank_user_domain = 1;	/* user domain set to null string! */

        ps->maildomain = ps->localdomain[0] ? ps->localdomain : ps->hostname;
#else
        ps->maildomain = strucmp(ps->VAR_USE_ONLY_DOMAIN_NAME, "yes")
			  ? ps->hostname : ps->localdomain;
#endif
    }

    /*
     * Tell c-client what domain to use when completing unqualified
     * addresses it finds in local mailboxes.  Remember, it won't 
     * affect what's to the right of '@' for unqualified addresses in
     * remote folders...
     */
    mail_parameters(NULL, SET_LOCALHOST, (void *) ps->maildomain);
    if(F_OFF(F_QUELL_MAILDOMAIN_WARNING, ps) && !strchr(ps->maildomain, '.')){
	snprintf(tmp_20k_buf, SIZEOF_20KBUF, _("Incomplete maildomain \"%s\"."),
		ps->maildomain);
	init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
	strncpy(tmp_20k_buf,
	       _("Return address in mail you send may be incorrect."), SIZEOF_20KBUF);
	tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	init_error(ps, SM_ORDER | SM_DING, 3, 5, tmp_20k_buf);
    }

    dprint((1,"User domain name being used \"%s\"\n",
               ps->userdomain == NULL ? "" : ps->userdomain));
    dprint((1,"Local Domain name being used \"%s\"\n",
               ps->localdomain ? ps->localdomain : "?"));
    dprint((1,"Host name being used \"%s\"\n",
               ps->hostname ? ps->hostname : "?"));
    dprint((1,
	       "Mail Domain name being used (by c-client too) \"%s\"\n",
               ps->maildomain ? ps->maildomain : "?"));

    if(!ps->maildomain || !ps->maildomain[0]){
#if defined(DOS) || defined(OS2)
	if(ps->blank_user_domain)
	  return(0);		/* prompt for this in send.c:dos_valid_from */
#endif
        fprintf(stderr, _("No host name or domain name set\n"));
        return(-1);
    }
    else
      return(0);
}


/*----------------------------------------------------------------------
  Make sure the default save folders exist in the default
  save context.
  ----*/
void
init_save_defaults(void)
{
    CONTEXT_S  *save_cntxt;

    if(!ps_global->VAR_DEFAULT_FCC ||
       !*ps_global->VAR_DEFAULT_FCC ||
       !ps_global->VAR_DEFAULT_SAVE_FOLDER ||
       !*ps_global->VAR_DEFAULT_SAVE_FOLDER)
      return;

    if(!(save_cntxt = default_save_context(ps_global->context_list)))
      save_cntxt = ps_global->context_list;

    if(!(folder_exists(save_cntxt, ps_global->VAR_DEFAULT_FCC) & FEX_ISFILE))
      context_create(save_cntxt, NULL, ps_global->VAR_DEFAULT_FCC);

    if(!(folder_exists(save_cntxt, ps_global->VAR_DEFAULT_SAVE_FOLDER) &
								 FEX_ISFILE))
      context_create(save_cntxt, NULL, ps_global->VAR_DEFAULT_SAVE_FOLDER);

    free_folder_list(save_cntxt);
}


/*----------------------------------------------------------------------
      Put sent-mail files in date order 

   Args: a, b  -- The names of two files.  Expects names to be sent-mail-mmm-yy
                  Other names will sort in order and come before those
                  in above format.
 ----*/
int   
compare_sm_files(const qsort_t *aa, const qsort_t *bb)
{
    struct sm_folder *a = (struct sm_folder *)aa,
                     *b = (struct sm_folder *)bb;

    if(a->month_num == -1 && b->month_num == -1 && a->name && b->name)
      return(strucmp(a->name, b->name));
    if(a->month_num == -1)      return(-1);
    if(b->month_num == -1)      return(1);

    return(a->month_num - b->month_num);
}



/*----------------------------------------------------------------------
      Create an ordered list of sent-mail folders and their month numbers

   Args: dir -- The directory to find the list of files in

 Result: Pointer to list of files is returned. 

This list includes all files that start with "sent-mail", but not "sent-mail" 
itself.
  ----*/
struct sm_folder *
get_mail_list(CONTEXT_S *list_cntxt, char *folder_base)
{
    register struct sm_folder *sm  = NULL;
    struct sm_folder          *sml = NULL;
    char                      *filename;
    int                        i, folder_base_len;
    int                        max_files;
    char		       searchname[MAXPATH+1];

    if((folder_base_len = strlen(folder_base)) == 0 || !list_cntxt){
	sml = (struct sm_folder *) fs_get(sizeof(struct sm_folder));
	memset((void *)sml, 0, sizeof(struct sm_folder));
        return(sml);
    }

#ifdef	DOS
    if(*list_cntxt->context != '{'){	/* NOT an IMAP collection! */
	snprintf(searchname, sizeof(searchname), "%4.4s*", folder_base);
	folder_base_len = strlen(searchname) - 1;
    }
    else
#endif
    snprintf(searchname, sizeof(searchname), "%.*s*", sizeof(searchname)-2, folder_base);

    build_folder_list(NULL, list_cntxt, searchname, NULL, BFL_FLDRONLY);

    max_files = MIN(MAX(0, folder_total(FOLDERS(list_cntxt))), 5000);
    sml = sm = (struct sm_folder *) fs_get(sizeof(struct sm_folder)*(max_files+1));
    memset((void *)sml, 0, sizeof(struct sm_folder) * (max_files+1));

    for(i = 0; i < folder_total(FOLDERS(list_cntxt)); i++){
	filename = folder_entry(i, FOLDERS(list_cntxt))->name;
#ifdef	DOS
        if(struncmp(filename, folder_base, folder_base_len) == 0
           && strucmp(filename, folder_base)){

	if(*list_cntxt->context != '{'){
	    int j;
	    for(j = 0; j < 4; j++)
	      if(!isdigit((unsigned char)filename[folder_base_len + j]))
		break;

	   if(j < 4)		/* not proper date format! */
	     continue;		/* keep trying */
	}
#else
#ifdef OS2
        if(strnicmp(filename, folder_base, folder_base_len) == 0
           && stricmp(filename, folder_base)){
#else
        if(strncmp(filename, folder_base, folder_base_len) == 0
           && strcmp(filename, folder_base)){
#endif
#endif
	    sm->name = cpystr(filename);
#ifdef	DOS
	    if(*list_cntxt->context != '{'){ /* NOT an IMAP collection! */
		sm->month_num  = (sm->name[folder_base_len] - '0') * 10;
		sm->month_num += sm->name[folder_base_len + 1] - '0';
	    }
	    else
#endif
            sm->month_num = month_num(sm->name + (size_t)folder_base_len + 1);
            sm++;
            if(sm >= &sml[max_files])
               break; /* Too many files, ignore the rest ; shouldn't occur */
        }
    }

    /* anything to sort?? */
    if(sml->name && *(sml->name) && (sml+1)->name && *((sml+1)->name)){
	qsort(sml,
	      sm - sml,
	      sizeof(struct sm_folder),
	      compare_sm_files);
    }

    return(sml);
}



int
check_prune_time(time_t *now, struct tm **tm_now)
{
    char tmp[50];

    *now = time((time_t *) 0);
    *tm_now = localtime(now);

    /*
     * If the last time we did this is blank (as if pine's run for
     * first time), don't go thru list asking, but just note it for 
     * the next time...
     */
    if(ps_global->VAR_LAST_TIME_PRUNE_QUESTION == NULL){
	ps_global->last_expire_year = (*tm_now)->tm_year;
	ps_global->last_expire_month = (*tm_now)->tm_mon;
	snprintf(tmp, sizeof(tmp), "%d.%d", ps_global->last_expire_year,
		ps_global->last_expire_month + 1);
	set_variable(V_LAST_TIME_PRUNE_QUESTION, tmp, 1, 1, Main);
	return(0);
    }

    if(ps_global->last_expire_year != -1 &&
      ((*tm_now)->tm_year <  ps_global->last_expire_year ||
       ((*tm_now)->tm_year == ps_global->last_expire_year &&
        (*tm_now)->tm_mon <= ps_global->last_expire_month)))
      return(0); 
    
    return(1);
}


int
first_run_of_month(void)
{
    time_t     now;
    struct tm *tm_now;

    now = time((time_t *) 0);
    tm_now = localtime(&now);

    if(ps_global->last_expire_year == -1 ||
      (tm_now->tm_year <  ps_global->last_expire_year ||
       (tm_now->tm_year == ps_global->last_expire_year &&
        tm_now->tm_mon <= ps_global->last_expire_month)))
      return(0); 

    return(1);
}


int
first_run_of_year(void)
{
    time_t     now;
    struct tm *tm_now;

    now = time((time_t *) 0);
    tm_now = localtime(&now);

    if(ps_global->last_expire_year == -1 ||
      (tm_now->tm_year <=  ps_global->last_expire_year))
      return(0); 

    return(1);
}


/*
 * prune_move_folder - rename folder in context and delete old copy
 * Returns -1 if unsuccessful.
 */
int
prune_move_folder(char *oldpath, char *newpath, CONTEXT_S *prune_cntxt)
{
    char spath[MAXPATH+1];

    strncpy(spath, oldpath, sizeof(spath)-1);
    spath[sizeof(spath)-1] = '\0';

    /*--- User says OK to rename ---*/
    dprint((5, "rename \"%s\" to \"%s\"\n",
	   spath ? spath : "?", newpath ? newpath : "?"));
    q_status_message1(SM_ORDER, 1, 3,
		      /* TRANSLATORS: arg is a filename */
		      _("Renaming \"%s\" at start of month"),
		      pretty_fn(spath ? spath : "?"));

    if(!context_rename(prune_cntxt, NULL, spath, newpath)){
        q_status_message2(SM_ORDER | SM_DING, 3, 4,
			  /* TRANSLATORS: 1st arg is filename, 2nd is error message */
			  _("Error renaming \"%s\": %s"),
                          pretty_fn(spath ? spath : "?"),
			  error_description(errno));
        dprint((1, "Error renaming %s to %s: %s\n",
                   spath ? spath : "?", newpath ? newpath : "?",
		   error_description(errno)));
        display_message('x');
        return -1;
    }

    context_create(prune_cntxt, NULL, spath);

    return 0;
}
