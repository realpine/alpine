/*-----------------------------------------------------------------------
 $Id: alpined.h 1142 2008-08-13 17:22:21Z hubert@u.washington.edu $
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

/*
 * Seconds afterwhich we bail on imap connections
 */
#define	WP_TCP_TIMEOUT	(10 * 60)

/*
 * buf to hold hostname for auth/cert
 */
#define	CRED_REQ_SIZE	256

/*
 * Various external definitions
 */
extern int	  peNoPassword;
extern int	  peCredentialError;
extern char	  peCredentialRequestor[];
extern int	  peCertQuery;
extern int	  peCertFailure;
extern char	 *peSocketName;
extern STRLIST_S *peCertHosts;

/*
 * Protoypes for various functions
 */

/* alpined.c */
void	sml_addmsg(int, char *);

