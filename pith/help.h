/*
 * $Id: help.h 900 2008-01-05 01:13:26Z hubert@u.washington.edu $
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

#ifndef PITH_HELP_INCLUDED
#define PITH_HELP_INCLUDED


#include "../pith/state.h"
#include "../pith/filttype.h"
#include "../pith/helptext.h"


#define RMMSGLEN 120
#define RMTIMLEN 15
#define RMJLEN   500
#define RMLLEN   2000
#define RMHLEN   2000

typedef struct _rev_msg {
    unsigned long    seq;
    short            level;	/* -1 for journal, debuglevel for dprint */
    unsigned         continuation:1;
    char             message[RMMSGLEN+1];
    char             timestamp[RMTIMLEN+1];
} REV_MSG_S;


typedef enum {No, Jo, Lo, Hi} RMCat;

extern REV_MSG_S rmjoarray[RMJLEN];	/* For regular journal */
extern REV_MSG_S rmloarray[RMLLEN];	/* debug 0-4 */
extern REV_MSG_S rmhiarray[RMHLEN];	/* debug 5-9 */
extern int       rmjofirst, rmjolast;
extern int       rmlofirst, rmlolast;
extern int       rmhifirst, rmhilast;
extern int       rm_not_right_now;


/* exported protoypes */
HelpType    help_name2section(char *, int);
void        debugjournal_to_file(FILE *);
void	    add_review_message(char *, int);
int         gripe_gripe_to(char *);
char       *get_alpine_revision_string(char *, size_t);
char       *get_alpine_revision_number(char *, size_t);


#endif /* PITH_HELP_INCLUDED */
