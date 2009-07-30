/*
 * $Id: save.h 942 2008-03-04 18:21:33Z hubert@u.washington.edu $
 *
 * ========================================================================
 * Copyright 2006-2008 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */

#ifndef PITH_SAVE_INCLUDED
#define PITH_SAVE_INCLUDED


#include "../pith/savetype.h"
#include "../pith/context.h"
#include "../pith/msgno.h"
#include "../pith/state.h"
#include "../pith/store.h"


#define SV_DELETE		0x1	/* delete source msg after save		*/
#define SV_FOR_FILT		0x2	/* save called from filtering routine,	*/
					/* just different status messages	*/
#define SV_FIX_DELS		0x4	/* remove Del mark before saving	*/
#define SV_INBOXWOCNTXT		0x8	/* interpret "inbox" as one true inbox	*/
#define SV_PRESERVE		0x10	/* preserve order			*/


#define SSCP_NONE		0x0
#define SSCP_INIT		0x1
#define SSCP_END		0x2
#define SSCP_ANSWER_IS_YES	0x4


/* exported protoypes */
char	   *save_get_default(struct pine *, ENVELOPE *, long, char *, CONTEXT_S **);
void        save_get_fldr_from_env(char *, int, ENVELOPE *, struct pine *, long, char *);
long	    save(struct pine *, MAILSTREAM *, CONTEXT_S *, char *, MSGNO_S *, int);
long	    save_fetch_append_cb(MAILSTREAM *, void *, char **, char **, STRING **);
int	    save_fetch_append(MAILSTREAM *, long, char *, MAILSTREAM *, char *, CONTEXT_S *,
			    unsigned long, char *, char *, STORE_S *);
void	    saved_date(char *, char *);
MAILSTREAM *save_msg_stream(CONTEXT_S *, char *, int *);
int	    create_for_save(CONTEXT_S *, char *);


#endif /* PITH_SAVE_INCLUDED */
