/*
 * $Id: remote.h 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $
 *
 * ========================================================================
 * Copyright 2006-2008 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */

#ifndef PITH_REMOTE_INCLUDED
#define PITH_REMOTE_INCLUDED


#include "../pith/remtype.h"
#include "../pith/conftype.h"


#define REMOTE_DATA_TYPE	TYPETEXT
#define REMOTE_DATA_VERS_NUM	1
#define REMOTE_ABOOK_SUBTYPE	"x-pine-addrbook"
#define REMOTE_PINERC_SUBTYPE	"x-pine-pinerc"
#define REMOTE_SIG_SUBTYPE	"x-pine-sig"
#define REMOTE_SMIME_SUBTYPE	"x-pine-smime"

#define PMAGIC                   "P#*E@"
#define METAFILE_VERSION_NUM     "01"
#define SIZEOF_PMAGIC       (5)
#define SIZEOF_SPACE        (1)
#define SIZEOF_VERSION_NUM  (2)
#define TO_FIND_HDR_PMAGIC  (0)
#define TO_FIND_VERSION_NUM (TO_FIND_HDR_PMAGIC + SIZEOF_PMAGIC + SIZEOF_SPACE)


/*
 * For flags below. Also for address book flags in struct adrbk. Some of
 * these flags are shared so they need to be in the same namespace.
 */
#define DEL_FILE	0x00001	/* remove addrbook file in adrbk_close     */
#define DO_REMTRIM	0x00004	/* trim remote data if needed and possible */
#define USE_OLD_CACHE	0x00008	/* using cache copy, couldn't check if old */
#define REM_OUTOFDATE	0x00010	/* remote data changed since cached        */
#define NO_STATUSCMD	0x00020	/* imap server doesn't have status command */
#define NO_META_UPDATE	0x00040	/* don't try to update metafile            */
#define NO_PERM_CACHE	0x00080	/* remove temp cache files when done       */
#define NO_FILE		0x00100	/* use memory (sonofile) instead of a file */
#define FILE_OUTOFDATE	0x00400	/* addrbook file discovered out of date    */
#define COOKIE_ONE_OK	0x02000	/* cookie with old value of one is ok      */
#define USER_SAID_NO	0x04000	/* user said no when asked about cookie    */
#define USER_SAID_YES	0x08000	/* user said yes when asked about cookie   */
#define BELIEVE_CACHE	0x10000	/* user said yes when asked about cookie   */


extern char meta_prefix[];


/* exported protoypes */
char       *read_remote_pinerc(PINERC_S *, ParsePinerc);
REMDATA_S  *rd_create_remote(RemType, char *, char *, unsigned *, char *, char *);
REMDATA_S  *rd_new_remdata(RemType, char *, char *);
void	    rd_free_remdata(REMDATA_S **);
void	    rd_trim_remdata(REMDATA_S **);
void	    rd_close_remdata(REMDATA_S **);
int	    rd_read_metadata(REMDATA_S *);
void	    rd_write_metadata(REMDATA_S *, int);
void	    rd_update_metadata(REMDATA_S *, char *);
void	    rd_open_remote(REMDATA_S *);
void	    rd_close_remote(REMDATA_S *);
int	    rd_stream_exists(REMDATA_S *);
int	    rd_ping_stream(REMDATA_S *);
void	    rd_check_readonly_access(REMDATA_S *);
int         rd_init_remote(REMDATA_S *, int);
int	    rd_update_local(REMDATA_S *);
int	    rd_update_remote(REMDATA_S *, char *);
void	    rd_check_remvalid(REMDATA_S *, long);
int	    rd_remote_is_readonly(REMDATA_S *);
int         rd_chk_for_hdr_msg(MAILSTREAM **, REMDATA_S *, char **);

/* currently mandatory to implement stubs */

/*
 * This is used when we try to copy remote config data and find that
 * the remote folder looks fishy. We ask the user what they want to
 * do.
 */
int         rd_prompt_about_forged_remote_data(int,REMDATA_S *, char *);


#endif /* PITH_REMOTE_INCLUDED */
