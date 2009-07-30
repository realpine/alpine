#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: fnexpand.c 761 2007-10-23 22:35:18Z hubert@u.washington.edu $";
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

#include "../charconv/filesys.h"

#include "fnexpand.h"




/*----------------------------------------------------------------------
       Expand the ~ in a file ala the csh (as home directory)

   Args: buf --  The filename to expand (nothing happens unless begins with ~)
         len --  The length of the buffer passed in (expansion is in place)

 Result: Expanded string is returned using same storage as passed in.
         If expansion fails, NULL is returned
 ----*/
char *
fnexpand(char *buf, int len)
{
#ifdef	_WINDOWS
    /* We used to use ps_global->home_dir, now we have to build it */
    if(*buf == '~' && *(buf+1) == '\\'){
	char temp_path[_MAX_PATH], home_buf[_MAX_PATH], *temp_home_str;

	if(getenv("HOME") != NULL)
	  temp_home_str = getenv("HOME");
	else{
	    /* should eventually strip this out of get_user_info */
	    char *p, *q;

	    if((p = (char *) getenv("HOMEDRIVE"))
	       && (q = (char *) getenv("HOMEPATH")))
	      snprintf(home_buf, sizeof(home_buf), "%s%s", p, q);
	    else
	      snprintf(home_buf, sizeof(home_buf), "%c:\\", '@' + _getdrive());

	    temp_home_str = home_buf;
	}
	snprintf(temp_path, sizeof(temp_path), "%s", buf+1);
	snprintf(buf, sizeof(buf), "%s%s", temp_path, fname_to_utf8(temp_home_str));
    }
    return(buf);
#else	/* UNIX */
    struct passwd *pw;
    register char *x,*y;
    char name[20], *tbuf;
    
    if(*buf == '~') {
        for(x = buf+1, y = name;
	    *x != '/' && *x != '\0' && y < name + sizeof(name)-1;
	    *y++ = *x++)
	  ;

        *y = '\0';
        if(x == buf + 1) 
          pw = getpwuid(getuid());
        else
          pw = getpwnam(name);

        if(pw == NULL)
          return((char *)NULL);
        if(strlen(pw->pw_dir) + strlen(buf) > len) {
          return((char *)NULL);
        }

	if((tbuf = (char *) malloc((len+1)*sizeof(char))) != NULL){
	    snprintf(tbuf, len, "%s%s", pw->pw_dir, x);
	    snprintf(buf, len, "%s", tbuf);
	    free((void *)tbuf);
	}
    }

    return(len ? buf : (char *)NULL);
#endif	/* UNIX */
}


