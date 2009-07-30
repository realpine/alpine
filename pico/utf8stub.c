#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: utf8stub.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
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


#include <system.h>
#include "utf8stub.h"


/*
 * Stub functions to fill in functions used in utf8 support routines
 */


void *
fs_get (size_t size)
{
  void *block = malloc (size ? size : (size_t) 1);
  if (!block) fatal ("Out of memory");
  return (block);
}


void
fs_resize(void **block, size_t size)
{
  if (!(*block = realloc (*block,size ? size : (size_t) 1)))
    fatal ("Can't resize memory");
}

void
fs_give (void **block)
{
  free (*block);
  *block = NULL;
}

void
fatal(char *s)
{
    ;
}


int
compare_ulong(unsigned long l1, unsigned long l2)
{
  if (l1 < l2) return -1;
  if (l1 > l2) return 1;
  return 0;
}


int
compare_cstring(unsigned char *s1, unsigned char *s2)
{
  int i;
  if (!s1) return s2 ? -1 : 0;	/* empty string cases */
  else if (!s2) return 1;
  for (; *s1 && *s2; s1++,s2++)
    if ((i = (compare_ulong (islower (*s1) ? toupper (*s1) : *s1,
			    islower (*s2) ? toupper (*s2) : *s2))) != 0)
      return i;			/* found a difference */
  if (*s1) return 1;		/* first string is longer */
  return *s2 ? -1 : 0;		/* second string longer : strings identical */
}

int
panicking(void)
{
    return(0);
}
