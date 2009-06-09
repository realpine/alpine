#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: newmail.c 205 2006-10-26 23:04:44Z hubert@u.washington.edu $";
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

#include "../estruct.h"
#include "../mode.h"
#include "../pico.h"
#include "../edef.h"
#include "../efunc.h"
#include "../keydefs.h"

#include "../../pith/charconv/filesys.h"

#include "newmail.h"




/*
 * pico_new_mail - just checks mtime and atime of mail file and notifies user 
 *	           if it's possible that they have new mail.
 */
int
pico_new_mail(void)
{
#ifndef _WINDOWS
    int ret = 0;
    static time_t lastchk = 0;
    struct stat sbuf;
    char   inbox[256], *p;

    if(p = (char *)getenv("MAIL"))
      /* fix unsafe sprintf - noticed by petter wahlman <petter@bluezone.no> */
      snprintf(inbox, sizeof(inbox), "%s", p);
    else
      snprintf(inbox, sizeof(inbox), "%s/%s", MAILDIR, (char *) getlogin());

    if(our_stat(inbox, &sbuf) == 0){
	ret = sbuf.st_atime <= sbuf.st_mtime &&
	  (lastchk < sbuf.st_mtime && lastchk < sbuf.st_atime);
	lastchk = sbuf.st_mtime;
	return(ret);
    }
    else
      return(ret);
#else /* _WINDOWS */
    return(0);
#endif
}
