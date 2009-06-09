/*-----------------------------------------------------------------------
 $Id: alpined.h 391 2007-01-25 03:53:59Z mikes@u.washington.edu $
  -----------------------------------------------------------------------*/

/* ========================================================================
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
extern char	*peSocketName;

/*
 * Protoypes for various functions
 */

/* alpined.c */
void	sml_addmsg(int, char *);

