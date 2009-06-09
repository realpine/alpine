/*
 * $Id: repltype.h 136 2006-09-22 20:06:05Z hubert@u.washington.edu $
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

#ifndef PITH_REPLTYPE_INCLUDED
#define PITH_REPLTYPE_INCLUDED


/*
 * Cursor position when resuming postponed message.
 */
typedef struct redraft_pos_s {
    char	  *hdrname;	/* header field name, : if in body */
    long	   offset;	/* offset into header or body */
} REDRAFT_POS_S;


/*
 * Message Reply control structure
 */
typedef struct reply_s {
    unsigned int   flags:4;	/* how to interpret handle field */
    char	  *mailbox;	/* mailbox handles are valid in */
    char	  *origmbox;	/* above is canonical name, this is orig */
    char	  *prefix;	/* string to prepend reply-to text */
    char	  *orig_charset;
    union {
	long	   pico_flags;	/* Flags to manage pico initialization */
	struct {		/* UID information */
	    unsigned long  validity;	/* validity token */
	    unsigned long *msgs;	/* array of reply'd to msgs */
	} uid;
    } data;
} REPLY_S;


#define	REPLY_PSEUDO	1
#define	REPLY_FORW	2	/* very similar to REPLY_PSEUDO */
#define	REPLY_MSGNO	3
#define	REPLY_UID	4


/*
 * Flag definitions to help control reply header building
 */
#define	RSF_NONE		0x00
#define	RSF_FORCE_REPLY_TO	0x01
#define	RSF_QUERY_REPLY_ALL	0x02
#define	RSF_FORCE_REPLY_ALL	0x04


/*
 * Flag definitions to help build forwarded bodies
 */
#define	FWD_NONE	0
#define	FWD_ANON	1
#define	FWD_NESTED	2


/*
 * Flag definitions to control composition of forwarded subject
 */
#define FS_NONE           0
#define FS_CONVERT_QUOTES 1


/* exported protoypes */


#endif /* PITH_REPLTYPE_INCLUDED */
