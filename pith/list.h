/*
 * $Id: list.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
 *
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

#ifndef PITH_LIST_INCLUDED
#define PITH_LIST_INCLUDED


#define	PL_NONE		0x00		/* flags modifying parse_list    */
#define	PL_REMSURRQUOT	0x01		/* rm surrounding quotes         */
#define	PL_COMMAQUOTE	0x02		/* backslash quotes comma        */


/* exported protoypes */
char	  **parse_list(char *, int, int, char **);
char      **copy_list_array(char **);
void        free_list_array(char ***);
int         equal_list_arrays(char **, char **);


#endif /* PITH_LIST_INCLUDED */
