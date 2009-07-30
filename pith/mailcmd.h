/*
 * $Id: mailcmd.h 1142 2008-08-13 17:22:21Z hubert@u.washington.edu $
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

#ifndef PITH_MAILCMD_INCLUDED
#define PITH_MAILCMD_INCLUDED


#include "../pith/state.h"
#include "../pith/msgno.h"
#include "../pith/context.h"
#include "../pith/indxtype.h"


#define	EC_NONE		0x00		/* flags modifying expunge_and_close */
#define	EC_NO_CLOSE	0x01		/* don't close at end                */


/*
 * mailcmd options
 */
#define	MCMD_NONE	0
#define	MCMD_AGG	0x01
#define	MCMD_AGG_2	0x02
#define	MCMD_SILENT	0x04


/* do_broach_folder flags */
#define DB_NOVISIT	0x01	/* this is a preopen, not a real visit */
#define DB_FROMTAB	0x02	/* opening because of TAB command      */
#define DB_INBOXWOCNTXT	0x04	/* interpret inbox as one true inbox */


/*
 * generic "is aggregate message command?" test
 */
#define	MCMD_ISAGG(O)	((O) & (MCMD_AGG | MCMD_AGG_2))


/* exported protoypes */
int	   any_messages(MSGNO_S *, char *, char *);
void	   bogus_utf8_command(char *, char *);
int	   can_set_flag(struct pine *, char *, int);
void	   cmd_cancelled(char *);
int	   cmd_delete(struct pine *, MSGNO_S *, int, char *(*)(struct pine *, MSGNO_S *));
int	   cmd_undelete(struct pine *, MSGNO_S *, int);
int	   cmd_expunge_work(MAILSTREAM *, MSGNO_S *);
CONTEXT_S *broach_get_folder(CONTEXT_S *, int *, char **);
int	   do_broach_folder(char *, CONTEXT_S *, MAILSTREAM **, unsigned long);
void	   expunge_and_close(MAILSTREAM *, char **, unsigned long);
void	   agg_select_all(MAILSTREAM *, MSGNO_S *, long *, int);
char	  *move_read_msgs(MAILSTREAM *, char *, char *, size_t, long);
char	  *move_read_incoming(MAILSTREAM *, CONTEXT_S *, char *, char **, char *, size_t);
void	   cross_delete_crossposts(MAILSTREAM *);
long	   zoom_index(struct pine *, MAILSTREAM *, MSGNO_S *, int);
int	   unzoom_index(struct pine *, MAILSTREAM *, MSGNO_S *);
int	   agg_text_select(MAILSTREAM *, MSGNO_S *, char, int, int, char *,
			   char *, SEARCHSET **);
int	   agg_flag_select(MAILSTREAM *, int, int, SEARCHSET **);
char	  *get_uname(char *, char *, int);
int        expand_foldername(char *, size_t);


#endif /* PITH_MAILCMD_INCLUDED */
