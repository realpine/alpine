#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: hostname.c 1176 2008-09-29 21:16:42Z hubert@u.washington.edu $";
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

#if	HAVE_GETHOSTNAME

#elif	HAVE_UNAME

#include <sys/utsname.h>

#elif	defined(SYSTEMID)

#elif	defined(XENIX)

#endif

#include "hostname.h"



/*----------------------------------------------------------------------
       Call system gethostname

  Args: hostname -- buffer to return host name in 
        size     -- Size of buffer hostname is to be returned in

 Result: returns 0 if the hostname is correctly set,
         -1 if not (and errno is set).
 ----*/
int
hostname(char *hostname, int size)
{
#if	HAVE_GETHOSTNAME

    if (gethostname(hostname, size))
      return -1;

    /* sanity check of hostname string */
    for (*dn = hname; (*dn > 0x20) && (*dn < 0x7f); ++dn)
      ;

    if (*dn)		/* non-ascii in hostname */
      strncpy(hostname, "unknown", size-1);

    hostname[size - 1] = '\0';
    return 0;

#elif	HAVE_UNAME

    /** This routine compliments of Scott McGregor at the HP
	    Corporate Computing Center **/
     
    int uname(struct utsname *);
    struct utsname name;

    (void)uname(&name);
    (void)strncpy(hostname,name.nodename,size-1);

    hostname[size - 1] = '\0';
    return 0;

#elif	defined(SYSTEMID)
	char    buf[32];
	FILE    *fp;
	char    *p;

	if ((fp = our_fopen("/etc/systemid", "rb")) != 0) {
	  fgets(buf, sizeof(buf) - 1, fp);
	  fclose(fp);
	  if ((p = strindex(buf, '\n')) != NULL)
	    *p = '\0';
	  (void) strncpy(hostname, buf, size - 1);
	  hostname[size - 1] = '\0';
	  return 0;
	}

#elif	defined(XENIX)

#ifdef DOUNAME
	/** This routine compliments of Scott McGregor at the HP
	    Corporate Computing Center **/
     
	int uname();
	struct utsname name;

	(void) uname(&name);
	(void) strncpy(hostname,name.nodename,size-1);
#else
	(void) strncpy(hostname, HOSTNAME, size-1);
#endif	/* DOUNAME */

	hostname[size - 1] = '\0';
	return 0;
#else
	/* We shouldn't get here except for the windows
	 * case, which currently doesn't use this (as
	 * it appears nothing else does as well)
	 */
	return -1;

#endif
}


