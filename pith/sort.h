/*
 * $Id: sort.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PITH_SORT_INCLUDED
#define PITH_SORT_INCLUDED


#include "../pith/sorttype.h"
#include "../pith/msgno.h"


#define	refresh_sort(S,M,F)	sort_folder((S), (M), mn_get_sort(M), \
					    mn_get_revsort(M), (F))

struct global_sort_data {
    MSGNO_S *msgmap;
    SORTPGM *prog;
};


/* sort flags */
#define	SRT_NON	0x0	/* None; no options set		*/
#define	SRT_VRB	0x1	/* Verbose			*/
#define	SRT_MAN	0x2	/* Sorted manually since opened	*/


extern struct global_sort_data g_sort;


/* exported protoypes */
char	*sort_name(SortOrder);
void	 sort_folder(MAILSTREAM *, MSGNO_S *, SortOrder, int, unsigned);
int	 decode_sort(char *, SortOrder *, int *);
void	 reset_sort_order(unsigned);


#endif /* PITH_SORT_INCLUDED */
