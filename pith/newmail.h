/*
 * $Id: newmail.h 807 2007-11-09 01:21:33Z hubert@u.washington.edu $
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

#ifndef PITH_NEWMAIL_INCLUDED
#define PITH_NEWMAIL_INCLUDED


typedef enum {GoodTime = 0, BadTime, VeryBadTime, DoItNow} CheckPointTime;


/*
 * Macro to help with new mail check timing...
 */
#define	NM_TIMING(X)	(((X)==NO_OP_IDLE) ? GoodTime : \
                                (((X)==NO_OP_COMMAND) ? BadTime : VeryBadTime))
#define	NM_NONE		0x00
#define	NM_STATUS_MSG	0x01
#define	NM_DEFER_SORT	0x02
#define	NM_FROM_COMPOSER 0x04


/*
 * Icon text types, to tell which icon to use
 */
#define IT_NEWMAIL 0
#define IT_MCLOSED 1


/* exported protoypes */
long	new_mail(int, CheckPointTime, int);
void	format_new_mail_msg(char *, long, ENVELOPE *, char *, char *, char *, char *, size_t);
void	check_point_change(MAILSTREAM *);
void	reset_check_point(MAILSTREAM *);
int	changes_to_checkpoint(MAILSTREAM *);
void	zero_new_mail_count(void);
void	init_newmailfifo(char *);
void	close_newmailfifo(void);


/* mandatory to implement prototypes */
void	newmail_check_cue(int);
void	newmail_check_point_cue(int);
void	icon_text(char *, int);
time_t	time_of_last_input(void);


#endif /* PITH_NEWMAIL_INCLUDED */
