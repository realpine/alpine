#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: list.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
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

/*======================================================================
     list.c
  ====*/


#include "../pith/headers.h"
#include "../pith/list.h"
#include "../pith/string.h"


/*
 * Internal prototypes
 */


/*
 * parse_list - takes a comma delimited list of "count" elements and 
 *              returns an array of pointers to each element neatly
 *              malloc'd in its own array.  Any errors are returned
 *              in the string pointed to by "error"
 *
 *	If remove_surrounding_double_quotes is set, then double quotes around
 *	each element of the list are removed. We can't do this for all list
 *	variables. For example, incoming folders look like
 *		nickname foldername
 *	in the config file. Each of those may be quoted separately.
 *
 *  NOTE: only recognizes escaped quotes
 */
char **
parse_list(char *list, int count, int flags, char **error)
{
    char **lvalue, *p2, *p3, *p4;
    int    was_quoted = 0;
    int    remove_surrounding_double_quotes;
    int    commas_may_be_escaped;

    remove_surrounding_double_quotes = (flags & PL_REMSURRQUOT);
    commas_may_be_escaped = (flags & PL_COMMAQUOTE);

    lvalue = (char **) fs_get((count+1) * sizeof(char *));
    count  = 0;
    while(*list){			/* pick elements from list */
	p2 = list;		/* find end of element */
	while(1){
	    if(*p2 == '"')	/* ignore ',' if quoted   */
	      was_quoted = (was_quoted) ? 0 : 1 ;

	    if(*p2 == '\\' && *(p2+1) == '"')
	      p2++;		/* preserve escaped quotes, too */

	    if((*p2 == ',' && !was_quoted) || *p2 == '\0')
	      break;

	    if(commas_may_be_escaped && *p2 == '\\' && *(p2+1) == ',')
	      p2++;

	    p2++;
	}

	if(was_quoted){		/* unbalanced quotes! */
	    if(error)
	      *error = "Unbalanced quotes";

	    break;
	}

	/*
	 * if element found, eliminate trailing 
	 * white space and tie into variable list
	 */
	if(p2 != list){
	    for(p3 = p2 - 1; isspace((unsigned char) *p3) && list < p3; p3--)
	      ;

	    p4 = fs_get(((p3 - list) + 2) * sizeof(char));
	    lvalue[count] = p4;
	    while(list <= p3)
	      *p4++ = *list++;

	    *p4 = '\0';

	    if(remove_surrounding_double_quotes)
	      removing_double_quotes(lvalue[count]);
	    
	    count++;
	}

	if(*(list = p2) != '\0'){	/* move to beginning of next val */
	    while(*list == ',' || isspace((unsigned char)*list))
	      list++;
	}
    }

    lvalue[count] = NULL;		/* tie off pointer list */
    return(lvalue);
}


/*
 * Free array of string pointers and associated strings
 *
 * Args: list -- array of char *'s
 */
void
free_list_array(char ***list)
{
    char **p;

    if(list && *list){
	for(p = *list; *p; p++)
	  fs_give((void **) p);

	fs_give((void **) list);
    }
}


/*
 * Copy array of string pointers and associated strings
 *
 * Args: list -- array of char *'s
 *
 * Returns: Allocated array of string pointers and allocated copies of strings.
 *          Caller should free the list array when done.
 */
char **
copy_list_array(char **list)
{
    int    i, cnt = 0;
    char **p, **ret_list = NULL;

    if(list){
	while(list[cnt++])
	  ;

	p = ret_list = (char **)fs_get((cnt+1) * sizeof(char *));
	memset((void *) ret_list, 0, (cnt+1) * sizeof(char *));

	for(i=0; list[i]; i++, p++)
	  *p = cpystr(list[i]);
	
	*p = NULL;

    }

    return(ret_list);
}


int
equal_list_arrays(char **list1, char **list2)
{
    int ret = 0;

    if(list1 && list2){
	while(*list1){
	    if(!*list2 || strcmp(*list1, *list2) != 0)
	      break;
	    
	    list1++;
	    list2++;
	}

	if(*list1 == NULL && *list2 == NULL)
	  ret++;
    }

    return(ret);
}
