#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: pw_stuff.c 763 2007-10-23 23:37:34Z hubert@u.washington.edu $";
#endif

/*
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

#include <system.h>
#include <general.h>

#include "../charconv/utf8.h"
#include "../charconv/filesys.h"
#include "pw_stuff.h"

/*
 * internal prototypes
 */
#ifndef	_WINDOWS

static char *gcos_name(char *, char *);


/*----------------------------------------------------------------------
      Pull the name out of the gcos field if we have that sort of /etc/passwd

   Args: gcos_field --  The long name or GCOS field to be parsed
         logname    --  Replaces occurances of & with logname string

 Result: returns pointer to buffer with name
  ----*/
static char *
gcos_name(char *gcos_field, char *logname)
{
    static char fullname[MAX_FULLNAME+1];
    register char *fncp, *gcoscp, *lncp, *end;

    /*
     * Full name is all chars up to first ',' (or whole gcos, if no ',').
     * Replace any & with Logname.
     */

    for(fncp = fullname, gcoscp= gcos_field, end = fullname + MAX_FULLNAME;
        (*gcoscp != ',' && *gcoscp != '\0' && fncp < end);
	gcoscp++){

	if(*gcoscp == '&'){
	    for(lncp = logname; *lncp && fncp < end; fncp++, lncp++)
	      *fncp = (lncp == logname) ? toupper((unsigned char) (*lncp))
				        : (*lncp);
	}else
	  *fncp++ = *gcoscp;
    }
    
    *fncp = '\0';
    return(fullname);
}

#endif /* !_WINDOWS */



/*----------------------------------------------------------------------
      Fill in homedir, login, and fullname for the logged in user.
      These are all pointers to static storage so need to be copied
      in the caller.

 Args: ui    -- struct pointer to pass back answers

 Result: fills in the fields
  ----*/
void
get_user_info(struct user_info *ui)
{
#ifndef	_WINDOWS
    struct passwd *unix_pwd;

    unix_pwd = getpwuid(getuid());
    if(unix_pwd == NULL) {
      ui->homedir = (char *) malloc(sizeof(char));
      ui->homedir[0] = '\0';
      ui->login = (char *) malloc(sizeof(char));
      ui->login[0] = '\0';
      ui->fullname = (char *) malloc(sizeof(char));
      ui->fullname[0] = '\0';
    }else {
	char *s; 
	size_t len;

	len = strlen(fname_to_utf8(unix_pwd->pw_dir));
	ui->homedir = (char *) malloc((len+1) * sizeof(char));
	snprintf(ui->homedir, len+1, "%s", fname_to_utf8(unix_pwd->pw_dir));

	len = strlen(fname_to_utf8(unix_pwd->pw_name));
	ui->login = (char *) malloc((len+1) * sizeof(char));
	snprintf(ui->login, len+1, "%s", fname_to_utf8(unix_pwd->pw_name));

	if((s = gcos_name(unix_pwd->pw_gecos, unix_pwd->pw_name)) != NULL){
	    len = strlen(fname_to_utf8(s));
	    ui->fullname = (char *) malloc((len+1) * sizeof(char));
	    snprintf(ui->fullname, len+1, "%s", fname_to_utf8(s));
	}
    }

#else  /* _WINDOWS */
    char buf[_MAX_PATH], *p, *q;
    TCHAR lptstr_buf[_MAX_PATH];
    int	 len = _MAX_PATH;

    if(GetUserName(lptstr_buf, &len))
      ui->login = lptstr_to_utf8(lptstr_buf);
    else
      ui->login = our_getenv("USERNAME");

    if((p = our_getenv("HOMEDRIVE"))
       && (q = our_getenv("HOMEPATH")))
      snprintf(buf, sizeof(buf), "%s%s", p, q);
    else
      snprintf(buf, sizeof(buf), "%c:\\", '@' + _getdrive());

    if(p)
      free((void *)p);

    if(q)
      free((void *)q);

    ui->homedir = (char *) malloc((strlen(buf)+1) * sizeof(char));
    if(ui->homedir){
	strncpy(ui->homedir, buf, strlen(buf));
	ui->homedir[strlen(buf)] = '\0';
    }

    ui->fullname = (char *) malloc(sizeof(char));
    if(ui->fullname)
      ui->fullname[0] = '\0';
#endif /* _WINDOWS */
}


/*----------------------------------------------------------------------
      Look up a userid on the local system and return rfc822 address

 Args: name  -- possible login name on local system

 Result: returns NULL or pointer to alloc'd string rfc822 address.
  ----*/
char *
local_name_lookup(char *name)
{
#ifndef	_WINDOWS
    struct passwd *pw = getpwnam(name);

    if(pw == NULL){
	char *p;

	for(p = name; *p; p++)
	  if(isupper((unsigned char)*p))
	    break;

	/* try changing it to all lower case */
	if(p && *p){
	    char lcase[256];
	    size_t l;

	    snprintf(lcase, sizeof(lcase), "%s", name);
	    
	    l = strlen(name);
	    for(p = lcase; *p; p++)
	      if(isupper((unsigned char)*p))
		*p = tolower((unsigned char)*p);

	    pw = getpwnam(lcase);

	    if(pw){
	      strncpy(name, lcase, l+1);
	      name[l] = '\0';
	    }
	}
    }

    if(pw != NULL){
	char   *gn, *s = NULL;
	size_t  l;

	if((gn = gcos_name(pw->pw_gecos, name)) != NULL
	   && (s = (char *) malloc(l  = ((strlen(gn) + 1) * sizeof(char)))) != NULL)
	  snprintf(s, l, "%s", gn);

	return(s);
    }
    else
      return((char *) NULL);
#else  /* _WINDOWS */
    return(NULL);
#endif /* _WINDOWS */
}


