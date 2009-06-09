/*-----------------------------------------------------------------------
 $Id: alpined.h 129 2006-09-22 04:11:35Z mikes@u.washington.edu $
  -----------------------------------------------------------------------*/

/* ========================================================================
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

/*
 * Various constants
 */
#define	WP_MAXSTATUS	1024

#define	WP_MAXAUTHREQ	256

/*
 * Seconds afterwhich we bail on imap connections
 */
#define	WP_TCP_TIMEOUT	(10 * 60)


/*
 * Various external definitions
 */
extern int       peNoPassword;
extern int       peCredentialError;
extern char	 peCredentialRequestor[];

/*
 * Protoypes for various functions
 */

/* alpined.c */
void	sml_addmsg(int, char *);

