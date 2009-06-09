/*
 * $Id: keyword.h 136 2006-09-22 20:06:05Z hubert@u.washington.edu $
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

#ifndef PITH_KEYWORD_INCLUDED
#define PITH_KEYWORD_INCLUDED


#include "../pith/msgno.h"


/* these are always UTF-8 */
typedef	struct keywords {
    char		*kw;
    char		*nick;
    struct keywords	*next;
} KEYWORD_S;


/* exported protoypes */
KEYWORD_S  *init_keyword_list(char **);
KEYWORD_S  *new_keyword_s(char *, char *);
void	    free_keyword_list(KEYWORD_S **);
char       *nick_to_keyword(char *);
char       *keyword_to_nick(char *);
int         user_flag_is_set(MAILSTREAM *, unsigned long, char *);
int         user_flag_index(MAILSTREAM *, char *);
void	    flag_string(MESSAGECACHE *, long, char *, size_t);
void        set_keywords_in_msgid_msg(MAILSTREAM *, MESSAGECACHE *, MAILSTREAM *, char *, long);
long        get_msgno_by_msg_id(MAILSTREAM *, char *, MSGNO_S *);
int         keyword_check(char *, char **);


#endif /* PITH_KEYWORD_INCLUDED */
