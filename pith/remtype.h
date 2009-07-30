/*
 * $Id: remtype.h 764 2007-10-23 23:44:49Z hubert@u.washington.edu $
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

#ifndef PITH_REMTYPE_INCLUDED
#define PITH_REMTYPE_INCLUDED

#include "../pith/store.h"


typedef enum {Loc, RemImap} RemType;


typedef enum {ReadOnly, ReadWrite, MaybeRorW, NoAccess, NoExists} AccessType;


/* Remote data folder bookkeeping */
typedef struct remote_data {
    RemType      type;
    char        *rn;		/* remote name (name of folder)              */
    char        *lf;		/* name of local file                        */
    STORE_S	*sonofile;	/* storage object which takes place of lf    */
    AccessType   access;	/* of remote folder                          */
    time_t       last_use;	/* when remote was last accessed             */
    time_t       last_valid_chk;/* when remote valid check was done          */
    time_t       last_local_valid_chk;
    STORE_S	*so;		/* storage object to use                     */
    char         read_status;	/* R for readonly                            */
    unsigned long flags;
    unsigned long cookie;
    union type_specific_data {
      struct imap_remote_data {
	char         *special_hdr;	/* header name for this type folder  */
	MAILSTREAM   *stream;		/* stream to use for remote folder   */
	char	     *chk_date;		/* Date of last message              */
	unsigned long chk_nmsgs;	/* Number of messages in folder      */
	unsigned long shouldbe_nmsgs;	/* Number which should be in folder  */
	imapuid_t     uidvalidity;	/* UIDVALIDITY of folder             */
	imapuid_t     uidnext;		/* UIDNEXT of folder                 */
	imapuid_t     uid;		/* UID of last message in folder     */
      }i;
    }t;
} REMDATA_S;


/* exported protoypes */


#endif /* PITH_REMTYPE_INCLUDED */
