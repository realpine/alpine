/*
 * $Id: mailcap.h 1012 2008-03-26 00:44:22Z hubert@u.washington.edu $
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

#ifndef PITH_MAILCAP_INCLUDED
#define PITH_MAILCAP_INCLUDED


typedef struct mcap_cmd {
    char *command;                              /* command to execute      */
    int   special_handling;                     /* special os handling     */
} MCAP_CMD_S;


/* exported protoypes */
char       *mc_conf_path(char *, char *, char *, int, char *);
int	    mailcap_can_display(int, char *, BODY *, int);
MCAP_CMD_S *mailcap_build_command(int, char *, BODY *, char *, int *, int);
void	    mailcap_free(void);

/* currently mandatory to implement stubs */

/* return exit status of test command */
int	exec_mailcap_test_cmd(char *);



#endif /* PITH_MAILCAP_INCLUDED */
