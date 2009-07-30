#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: collate.c 766 2007-10-23 23:59:00Z hubert@u.washington.edu $";
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

#include "collate.h"


/*
 * global hook 
 */
int (*pcollator)();


void
set_collation(int collation, int ctype)
{
    extern int collator();  /* set to strcoll if available in system.h */

    pcollator = strucmp;

#ifdef LC_COLLATE
  if(collation){
      char *status = NULL;

    /*
     * This may not have the desired effect, if collator is not
     * defined to be strcoll in os.h and strcmp and friends
     * don't know about locales. If your system does have strcoll
     * but we haven't defined collator to be strcoll in os.h, let us know.
     */
    status = setlocale(LC_COLLATE, "");

    /*
     * If there is an error or if the locale is the "C" locale, then we
     * don't want to use strcoll because in the default "C" locale strcoll
     * uses strcmp ordering and we want strucmp ordering.
     *
     * The problem with this is that setlocale returns a string which is
     * not equal to "C" on some systems even when the locale is "C", so we
     * can't really tell on those systems. On some systems like that, we
     * may end up with a strcmp-style collation instead of a strucmp-style.
     * We recommend that the users of those systems explicitly set
     * LC_COLLATE in their environment.
     */
    if(status && !(status[0] == 'C' && status[1] == '\0'))
      pcollator = collator;
  }
#endif
#ifdef LC_CTYPE
  if(ctype){
    (void)setlocale(LC_CTYPE, "");
  }
#endif

#ifdef LC_TIME
  setlocale(LC_TIME, "");
#endif
}


/*
 * sstrcasecmp - compare two pointers to strings case independently
 */
int
sstrcasecmp(const qsort_t *s1, const qsort_t *s2)
{
    return((*pcollator)(*(char **)s1, *(char **)s2));
}


#ifndef	_WINDOWS

/*--------------------------------------------------
     A case insensitive strcmp()     
  
   Args: o, r -- The two strings to compare

 Result: integer indicating which is greater
  ---*/
int
strucmp(register char *o, register char *r)
{
    if(o == NULL){
	if(r == NULL)
	  return 0;
	else
	  return -1;
    }
    else if(r == NULL)
      return 1;

    while(*o && *r
	  && ((isupper((unsigned char)(*o))
				  ? (unsigned char)tolower((unsigned char)(*o))
				  : (unsigned char)(*o))
	     == (isupper((unsigned char)(*r))
				  ? (unsigned char)tolower((unsigned char)(*r))
				  : (unsigned char)(*r)))){
	o++;
	r++;
    }

    return((isupper((unsigned char)(*o))
				? tolower((unsigned char)(*o))
				: (int)(unsigned char)(*o))
	   - (isupper((unsigned char)(*r))
			        ? tolower((unsigned char)(*r))
				: (int)(unsigned char)(*r)));
}

/*----------------------------------------------------------------------
     A case insensitive strncmp()     
  
   Args: o, r -- The two strings to compare
         n    -- length to stop comparing strings at

 Result: integer indicating which is greater
   
  ----*/
int
struncmp(register char *o, register char *r, register int n)
{
    if(n < 1)
      return 0;

    if(o == NULL){
	if(r == NULL)
	  return 0;
	else
	  return -1;
    }
    else if(r == NULL)
      return 1;

    n--;
    while(n && *o && *r
	  && ((isupper((unsigned char)(*o))
				  ? (unsigned char)tolower((unsigned char)(*o))
				  : (unsigned char)(*o))
	     == (isupper((unsigned char)(*r))
				  ? (unsigned char)tolower((unsigned char)(*r))
				  : (unsigned char)(*r)))){
	o++;
	r++;
	n--;
    }

    return((isupper((unsigned char)(*o))
				? tolower((unsigned char)(*o))
				: (int)(unsigned char)(*o))
	   - (isupper((unsigned char)(*r))
			        ? tolower((unsigned char)(*r))
				: (int)(unsigned char)(*r)));
}
#endif
