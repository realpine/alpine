/*
 * $Id: send.h 136 2006-09-22 20:06:05Z hubert@u.washington.edu $
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

#ifndef PINE_SEND_INCLUDED
#define PINE_SEND_INCLUDED


#include "../pith/send.h"
#include "../pith/reply.h"
#include "../pith/state.h"
#include "../pith/pattern.h"
#include "../pith/filter.h"


#define SS_PROMPTFORTO		0x01	/* Simple_send: prompt for To	*/
#define	SS_NULLRP		0x02	/* Null Return-path		*/


/* exported protoypes */
void	    compose_screen(struct pine *); 
void	    alt_compose_screen(struct pine *);
void	    compose_mail(char *, char *, ACTION_S *, PATMT *, gf_io_t);
int	    pine_simple_send(ENVELOPE *, BODY **, ACTION_S *, char *, char *, char **, int);
void	    pine_send(ENVELOPE *, BODY **, char *, ACTION_S *, char *, REPLY_S *,
		      REDRAFT_POS_S *, char *, PINEFIELD *, int);
int	    upload_msg_to_pico(char *, size_t, long *);
void	    phone_home(char *);
void        create_message_body(BODY **, PATMT *, char *, int);
char	   *pine_send_status(int, char *, char *, size_t, int *);
int	    confirm_daemon_send(void);


#endif /* PINE_SEND_INCLUDED */
