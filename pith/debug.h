/*
 * $Id: debug.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PITH_DEBUG_INCLUDED
#define PITH_DEBUG_INCLUDED


/*======================================================================
       Macros for debug printfs 
   n is debugging level:
       1 logs only highest level events and errors
       2 logs events like file writes
       3
       4 logs each command
       5
       6 
       7 logs details of command execution (7 is highest to run any production)
         allows core dumps without cleaning up terminal modes
       8
       9 logs gross details of command execution
    
  ====*/


#ifdef DEBUG

#define   dprint(x)	{ output_debug_msg x ; }

/* global debugging level */
extern int debug;

/* mandatory to implement stubs */
void	output_debug_msg(int, char *fmt, ...);
void	dump_configuration(int);
void	dump_contexts();



#else	/* !DEBUG */

#define   dprint(x)

#endif	/* !DEBUG */


#endif /* PITH_DEBUG_INCLUDED */
