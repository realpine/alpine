#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: strlst.c 761 2007-10-23 22:35:18Z hubert@u.washington.edu $";
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

     strlst.c
     Implements STRINGLIST creation destruction routines

  ====*/


#include "../pith/headers.h"
#include "../pith/strlst.h"


STRINGLIST *
new_strlst(char **l)
{
    STRINGLIST *sl = mail_newstringlist();

    sl->text.data = (unsigned char *) (*l);
    sl->text.size = strlen(*l);
    sl->next = (*++l) ? new_strlst(l) : NULL;
    return(sl);
}


void
free_strlst(struct string_list **sl)
{
    if(*sl){
	if((*sl)->next)
	  free_strlst(&(*sl)->next);

	fs_give((void **) sl);
    }
}
