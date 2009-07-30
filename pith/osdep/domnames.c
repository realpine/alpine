#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: domnames.c 1176 2008-09-29 21:16:42Z hubert@u.washington.edu $";
#endif

/*
 * ========================================================================
 * Copyright 2008 University of Washington
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

#include "domnames.h"


/*----------------------------------------------------------------------
       Get the current host and domain names

    Args: hostname   -- buffer to return the hostname in
          hsize      -- size of buffer above
          domainname -- buffer to return domain name in
          dsize      -- size of buffer above

  Result: The system host and domain names are returned. If the full host
          name is akbar.cac.washington.edu then the domainname is
          cac.washington.edu.

On Internet connected hosts this look up uses /etc/hosts and DNS to
figure all this out. On other less well connected machines some other
file may be read. If there is no notion of a domain name the domain
name may be left blank. On a PC where there really isn't a host name
this should return blank strings. The .pinerc will take care of
configuring the domain names. That is, this should only return the
native system's idea of what the names are if the system has such
a concept.
 ----*/
void
getdomainnames(char *hostname, int hsize, char *domainname, int dsize)
{
#if HAVE_NETDB_H
    char           *dn, hname[MAX_ADDRESS+1];
    struct hostent *he;
    char          **alias;
    char           *maybe = NULL;

    if(gethostname(hname, MAX_ADDRESS))
      hname[0] = 0xff;

    /* sanity check of hostname string */
    for(dn = hname; (*dn > 0x20) && (*dn < 0x7f); ++dn)
      ;

    if(*dn){		/* if invalid string returned, return "unknown" */
      strncpy(domainname, "unknown", dsize-1);
      domainname[dsize-1] = '\0';
      strncpy(hostname, "unknown", hsize-1);
      hostname[hsize-1] = '\0';
      return;
    }

    he = gethostbyname(hname);
    hostname[0] = '\0';

    if(he == NULL){
	strncpy(hostname, hname, hsize-1);
	hostname[hsize-1] = '\0';
    }
    else{
	/*
	 * If no dot in hostname it may be the case that there
	 * is an alias which is really the fully-qualified
	 * hostname. This could happen if the administrator has
	 * (incorrectly) put the unqualified name first in the
	 * hosts file, for example. The problem with looking for
	 * an alias with a dot is that now we're guessing, since
	 * the aliases aren't supposed to be the official hostname.
	 * We'll compromise and only use an alias if the primary
	 * name has no dot and exactly one of the aliases has a
	 * dot.
	 */
	strncpy(hostname, he->h_name, hsize-1);
	hostname[hsize-1] = '\0';
	if(strchr(hostname, '.') == NULL){		/* no dot in hostname */
	    for(alias = he->h_aliases; *alias; alias++){
		if(strchr(*alias, '.') != NULL){	/* found one */
		    if(maybe){		/* oops, this is the second one */
			maybe = NULL;
			break;
		    }
		    else
		      maybe = *alias;
		}
	    }

	    if(maybe){
		strncpy(hostname, maybe, hsize-1);
		hostname[hsize-1] = '\0';
	    }
	}
    }

    hostname[hsize-1] = '\0';

    if((dn = strchr(hostname, '.')) != NULL)
      strncpy(domainname, dn+1, dsize-1);
    else
      strncpy(domainname, hostname, dsize-1);

    domainname[dsize-1] = '\0';
#else  /* !HAVE_NETDB_H */
    /* should only be _WINDOWS */

#ifdef _WINDOWS
    char *p;
    extern char *mylocalhost(void);

    hostname[0] = domainname[0] = '\0';
    if(p = mylocalhost())
      snprintf(hostname, hsize, "%s", p);

    snprintf(domainname, dsize, "%s", 
	    (hostname[0] && hostname[0] != '[' && (p = strchr(hostname,'.')))
	      ? p+1 : hostname);
#else /* !_WINDOWS */

    char *p, hname[MAX_ADDRESS+1];

    hostname[0] = domainname[0] = '\0';
    if(gethostname(hname, MAX_ADDRESS) == 0)
      snprintf(hostname, hsize, "%s", hname);

    snprintf(domainname, dsize, "%s", 
	    (hostname[0] && hostname[0] != '[' && (p = strchr(hostname,'.')))
	      ? p+1 : hostname);
#endif /* !_WINDOWS */
#endif /* !HAVE_NETDB_H */
}
