/*
 * $Id: help.h 769 2007-10-24 00:15:40Z hubert@u.washington.edu $
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

#ifndef PINE_HELP_INCLUDED
#define PINE_HELP_INCLUDED


#include "../pith/help.h"


/*
 * Flags to help manage help display
 */
#define	HLPD_NONE	   0
#define	HLPD_NEWWIN	0x01
#define	HLPD_SECONDWIN	0x02
#define	HLPD_SIMPLE	0x04
#define	HLPD_FROMHELP	0x08


/* exported protoypes */
int	    url_local_helper(char *);
int	    url_local_config(char *);
void	    init_helper_getc(char **);
int	    helper_getc(char *);
int	    helper(HelpType, char *, int);
void	    review_messages(void);
void	    print_help(char **);
#ifdef	_WINDOWS
char	   *pcpine_help(HelpType);
#endif
#ifdef	DEBUG
void	    dump_config(struct pine *, gf_io_t, int);
void	    dump_pine_struct(struct pine *, gf_io_t);
#endif


#endif /* PINE_HELP_INCLUDED */
