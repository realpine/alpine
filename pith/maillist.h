/*
 * $Id: maillist.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PITH_MAILLIST_INCLUDED
#define PITH_MAILLIST_INCLUDED


/*
 * Constants and structs to aid RFC 2369 support
 */
#define	MLCMD_HELP	0
#define	MLCMD_UNSUB	1
#define	MLCMD_SUB	2
#define	MLCMD_POST	3
#define	MLCMD_OWNER	4
#define	MLCMD_ARCHIVE	5
#define	MLCMD_COUNT	6
#define	MLCMD_MAXDATA	3
#define	MLCMD_REASON	8192


typedef	struct	rfc2369_field_s {
    char  *name,
	  *description,
	  *action;
} RFC2369FIELD_S;

typedef	struct rfc2369_data_s {
    char *value,
	 *comment,
	 *error;
} RFC2369DATA_S;

typedef struct rfc2369_s {
    RFC2369FIELD_S  field;
    RFC2369DATA_S   data[MLCMD_MAXDATA];
} RFC2369_S;


/* exported protoypes */
char	  **rfc2369_hdrs(char **);
int	    rfc2369_parse_fields(char *, RFC2369_S *);


#endif /* PITH_MAILLIST_INCLUDED */
