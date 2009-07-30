#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: copyaddr.c 761 2007-10-23 22:35:18Z hubert@u.washington.edu $";
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

#include "../pith/headers.h"
#include "../pith/copyaddr.h"


/*
 * Copy the first address in list a and return it in allocated memory.
 */
ADDRESS *
copyaddr(struct mail_address *a)
{
    ADDRESS *new = NULL;

    if(a){
	new = mail_newaddr();
	if(a->personal)
	  new->personal = cpystr(a->personal);

	if(a->adl)
	  new->adl      = cpystr(a->adl);

	if(a->mailbox)
	  new->mailbox  = cpystr(a->mailbox);

	if(a->host)
	  new->host     = cpystr(a->host);

	new->next = NULL;
    }

    return(new);
}


/*
 * Copy the whole list a.
 */
ADDRESS *
copyaddrlist(struct mail_address *a)
{
    ADDRESS *new = NULL, *head = NULL, *current;

    for(; a; a = a->next){
	new = copyaddr(a);
	if(!head)
	  head = current = new;
	else{
	    current->next = new;
	    current = new;
	}
    }

    return(head);
}
