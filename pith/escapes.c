#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: escapes.c 761 2007-10-23 22:35:18Z hubert@u.washington.edu $";
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

/*======================================================================

     escapes.c
     Implements known escape code matching

  ====*/


#include "../pith/headers.h"
#include "../pith/escapes.h"



/*------------------------------------------------------------------
   This list of known escape sequences is taken from RFC's 1486 and 1554
   and draft-apng-cc-encoding, and the X11R5 source with only a remote
   understanding of what this all means...

   NOTE: if the length of these should extend beyond 4 chars, fix
	 MAX_ESC_LEN in filter.c
  ----*/
#ifdef	_WINDOWS
static char *known_escapes[] = {
    "(B",  "(J",  "$@",  "$B",			/* RFC 1468 */
    "(H",
    NULL};
#else
static char *known_escapes[] = {
    "(B",  "(J",  "$@",  "$B",			/* RFC 1468 */
    "(H",
    "$A",  "$(C", "$(D", ".A",  ".F",		/* added by RFC 1554 */
    "$)C", "$)A", "$*E", "$*X",			/* those in apng-draft */
    "$+G", "$+H", "$+I", "$+J", "$+K",
    "$+L", "$+M",
    ")I",   "-A",  "-B",  "-C",  "-D",		/* codes form X11R5 source */
    "-F",   "-G",  "-H",   "-L",  "-M",
    "-$(A", "$(B", "$)B", "$)D",
    NULL};
#endif


int
match_escapes(char *esc_seq)
{
    char **p;
    int    n;

    for(p = known_escapes; *p && strncmp(esc_seq, *p, n = strlen(*p)); p++)
      ;

    return(*p ? n + 1 : 0);
}
