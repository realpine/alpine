#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: canonicl.c 764 2007-10-23 23:44:49Z hubert@u.washington.edu $";
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

#include "../../c-client/mail.h"
extern unsigned char *lcase(unsigned char *);

#ifdef _WINDOWS
/* wingdi.h uses ERROR (!) and we aren't using the c-client ERROR so... */
#undef ERROR
#endif

#include <system.h>

#include "canonicl.h"


/*----------------------------------------------------------------------
       Return canonical form of host name ala c-client (UNIX version).

   Args: host      -- The host name

 Result: Canonical form, or input argument (worst case)

 You can call it twice without worrying about copying
 the results, but not more than twice.
 ----*/
char *
canonical_name(char *host)
{
    struct hostent *hent;
    char tmp[MAILTMPLEN];
    static int  whichbuf = 0;
    static char buf[2][NETMAXHOST+1];
    char       *b;

    whichbuf = (whichbuf + 1) % 2;
    b = buf[whichbuf];

                                /* domain literal is easy */
    if (host[0] == '[' && host[(strlen (host))-1] == ']')
      strncpy(b, host, NETMAXHOST);
    else{
	strncpy(tmp, host, sizeof(tmp)-1);
	tmp[sizeof(tmp)-1] = '\0';

	hent = gethostbyname((char *) lcase((unsigned char *) tmp));
	if(hent && hent->h_name)
	  strncpy(b, hent->h_name, NETMAXHOST);
	else
	  strncpy(b, host, NETMAXHOST);
    }

    b[NETMAXHOST] = '\0';
    return(b);
}


