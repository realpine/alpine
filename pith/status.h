/*
 * $Id: status.h 770 2007-10-24 00:23:09Z hubert@u.washington.edu $
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

#ifndef PITH_STATUS_INCLUDED
#define PITH_STATUS_INCLUDED


#include <general.h>


/*
 * Status message types
 */
#define	SM_DING		0x01			/* ring bell when displayed  */
#define	SM_ASYNC	0x02			/* display any time	     */
#define	SM_ORDER	0x04			/* ordered, flush on prompt  */
#define	SM_MODAL	0x08			/* display, wait for user    */
#define	SM_INFO		0x10			/* Handy, but discardable    */


/* exported protoypes */
void	q_status_message1(int, int, int, char *, void *);
void	q_status_message2(int, int, int, char *, void *, void *);
void	q_status_message3(int, int, int, char *, void *, void *, void *);
void	q_status_message4(int, int, int, char *, void *, void *, void *, void *);
void	q_status_message5(int, int, int, char *, void *, void *, void *, void *, void *);
void	q_status_message6(int, int, int, char *, void *, void *, void *, void *, void *, void *);
void	q_status_message7(int, int, int, char *, void *, void *,
			  void *, void *, void *, void *, void *);
void	q_status_message8(int, int, int, char *, void *, void *,
			  void *, void *, void *, void *, void *, void *);
			      

/* currently mandatory to implement stubs */
void	q_status_message(int, int, int, char *);
int	status_message_remaining(void);
int	display_message(UCS);
void	flush_status_messages(int);


#endif /* PITH_STATUS_INCLUDED */
