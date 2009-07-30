/*
 * $Id: reply.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PINE_REPLY_INCLUDED
#define PINE_REPLY_INCLUDED


#include "../pith/reply.h"
#include "../pith/state.h"
#include "../pith/pattern.h"
#include "../pith/store.h"


/* exported protoypes */
int	    reply(struct pine *, ACTION_S *);
int	    confirm_role(long, ACTION_S **);
int	    reply_to_all_query(int *);
int	    reply_using_replyto_query(void);
int	    reply_text_query(struct pine *, long, char **);
int	    reply_news_test(ENVELOPE *, ENVELOPE *);
char       *get_signature_file(char *, int, int, int);
int	    forward(struct pine *, ACTION_S *);
void	    forward_text(struct pine *, void *, SourceType);
int	    bounce(struct pine *, ACTION_S *);
char	   *bounce_msg(MAILSTREAM *, long, char *, ACTION_S *, char **, char *, char *, char *);
char	   *signature_edit(char *, char *);
char	   *signature_edit_lit(char *, char **, char *, HelpType);
void	    standard_picobuf_setup(PICO *);
void	    standard_picobuf_teardown(PICO *);


#endif /* PINE_REPLY_INCLUDED */
