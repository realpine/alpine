/*
 * $Id: mailpart.h 769 2007-10-24 00:15:40Z hubert@u.washington.edu $
 *
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

#ifndef PINE_MAILPART_INCLUDED
#define PINE_MAILPART_INCLUDED


#include "../pith/mailpart.h"
#include "context.h"
#include "../pith/state.h"


#define	DA_SAVE		0x01		/* flags used by display_attachment */
#define	DA_FROM_VIEW	0x02		/* see mailpart.c		    */
#define	DA_RESIZE	0x04
#define DA_DIDPROMPT    0x08            /* Already prompted to view att     */


/* exported protoypes */
void	    attachment_screen(struct pine *);
void	    write_attachment(int, long, ATTACH_S *, char *);
int         write_attachment_to_file(MAILSTREAM *, long, ATTACH_S *, int, char *);
int	    display_attachment(long, ATTACH_S *, int);
int         dispatch_attachment(ATTACH_S *);
#ifdef	_WINDOWS
void	    display_att_window(ATTACH_S *);
#endif


#endif /* PINE_MAILPART_INCLUDED */
