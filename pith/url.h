/*
 * $Id: url.h 764 2007-10-23 23:44:49Z hubert@u.washington.edu $
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

#ifndef PITH_URL_INCLUDED
#define PITH_URL_INCLUDED


/* exported protoypes */
char	   *rfc1738_scan(char *, int *);
char	   *rfc1738_str(char *);
unsigned long rfc1738_num(char **);
int	    rfc1738_group(char *);
char	   *rfc1738_encode_mailto(char *);
int	    rfc1808_tokens(char *, char **, char **, char **, char **, char **, char **);
char	   *web_host_scan(char *, int *);
char	   *mail_addr_scan(char *, int *);


#endif /* PITH_URL_INCLUDED */
