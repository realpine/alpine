#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: handle.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
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

#include "../pith/headers.h"
#include "../pith/handle.h"
#include "../pith/mailview.h"


HANDLE_S *
get_handle(HANDLE_S *handles, int key)
{
    HANDLE_S *h;

    if((h = handles) != NULL){
	for( ; h ; h = h->next)
	  if(h->key == key)
	    return(h);

	for(h = handles->prev ; h ; h = h->prev)
	  if(h->key == key)
	    return(h);
    }

    return(NULL);
}


void
init_handles(HANDLE_S **handlesp)
{
    if(handlesp)
      *handlesp = NULL;
      
    (void) url_external_specific_handler(NULL, 0);
}



HANDLE_S *
new_handle(HANDLE_S **handlesp)
{
    HANDLE_S *hp, *h = NULL;

    if(handlesp){
	h = (HANDLE_S *) fs_get(sizeof(HANDLE_S));
	memset(h, 0, sizeof(HANDLE_S));

	/* Put it in the list */
	if((hp = *handlesp) != NULL){
	    while(hp->next)
	      hp = hp->next;

	    h->key = hp->key + 1;
	    hp->next = h;
	    h->prev = hp;
	}
	else{
	    /* Assumption #2,340: There are NO ZERO KEY HANDLES */
	    h->key = 1;
	    *handlesp = h;
	}
    }

    return(h);
}


/*
 * Normally we ignore the is_used bit in HANDLE_S. However, if we are
 * using the delete_quotes filter, we pay attention to it. All of the is_used
 * bits are off by default, and the delete_quotes filter turns them on
 * if it is including lines with those handles.
 *
 * This is a bit of a crock, since it depends heavily on the order of the
 * filters. Notice that the charset_editorial filter, which comes after
 * delete_quotes and adds a handle, has to explicitly set the is_used bit!
 */
void
delete_unused_handles(HANDLE_S **handlesp)
{
    HANDLE_S *h, *nexth;

    if(handlesp && *handlesp && (*handlesp)->using_is_used){
	for(h = *handlesp; h && h->prev; h = h->prev)
	  ;
	
	for(; h; h = nexth){
	    nexth = h->next;
	    if(h->is_used == 0){
		if(h == *handlesp)
		  *handlesp = nexth;

		free_handle(&h);
	    }
	}
    }
}


void
free_handle(HANDLE_S **h)
{
    if(h){
	if((*h)->next)				/* clip from list */
	  (*h)->next->prev = (*h)->prev;

	if((*h)->prev)
	  (*h)->prev->next = (*h)->next;

	if((*h)->type == URL){			/* destroy malloc'd data */
	    if((*h)->h.url.path)
	      fs_give((void **) &(*h)->h.url.path);

	   if((*h)->h.url.tool)
	     fs_give((void **) &(*h)->h.url.tool);

	    if((*h)->h.url.name)
	      fs_give((void **) &(*h)->h.url.name);
	}

	free_handle_locations(&(*h)->loc);

	fs_give((void **) h);
    }
}


void
free_handles(HANDLE_S **handlesp)
{
    HANDLE_S *h;

    if(handlesp && *handlesp){
	while((h = (*handlesp)->next) != NULL)
	  free_handle(&h);

	while((h = (*handlesp)->prev) != NULL)
	  free_handle(&h);

	free_handle(handlesp);
    }
}


void
free_handle_locations(POSLIST_S **l)
{
    if(*l){
	free_handle_locations(&(*l)->next);
	fs_give((void **) l);
    }
}


