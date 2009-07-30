/*
 * $Id: util.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PITH_UTIL_INCLUDED
#define PITH_UTIL_INCLUDED


#include "../pith/news.h"


#define plural(n)	((n) == 1 ? "" : "s")


#define	READONLY_FOLDER(S)  ((S) && (S)->rdonly && !IS_NEWS(S))

#define STREAMNAME(S)	(((S) && sp_fldr(S) && sp_fldr(S)[0])        \
			  ? sp_fldr(S)                               \
			  : ((S) && (S)->mailbox && (S)->mailbox[0]) \
			    ? (S)->mailbox                           \
			    : "?")


/*
 * Simple, handy macro to determine if folder name is remote 
 * (on an imap server)
 */
#define	IS_REMOTE(X)	(*(X) == '{' && *((X) + 1) && *((X) + 1) != '}' \
			 && strchr(((X) + 2), '}'))


/* (0,0) is upper left */
typedef struct screen_position {
    int row;
    int col;
} Pos;


#define SCREEN_FUN_NULL ((void (*)(struct pine *)) NULL)


/* exported protoypes */
int           *cpyint(int);

/* currently mandatory to implement stubs */

/* called when we detect a serious program error */
void	  panic(char *);

/* called when testing to see if panic state is in effect */
int	 panicking(void);

/* logs or prints a message then exits */
void	  exceptional_exit(char *, int);


#endif /* PITH_UTIL_INCLUDED */
