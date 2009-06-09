#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: remote.c 101 2006-08-10 22:53:04Z mikes@u.washington.edu $";
#endif

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

#include <system.h>
#include <general.h>

#include "../../../c-client/c-client.h"

#include "../../../pith/remote.h"
#include "../../../pith/msgno.h"
#include "../../../pith/filter.h"
#include "../../../pith/util.h"
#include "../../../pith/debug.h"
#include "../../../pith/osdep/collate.h"


/*
 * Internal prototypes
 */

int
rd_prompt_about_forged_remote_data(reason, rd, extra)
    int        reason;
    REMDATA_S *rd;
    char      *extra;
{
    char  tmp[2000];
    char *unknown = "<unknown>";
    char *foldertype, *foldername, *special;

    /*
     * Since we're web based the user doesn't have much recourse in the event of one of these
     * weird errors, so we just report what happened and forge ahead
     */

    foldertype = (rd && rd->t.i.special_hdr && !strucmp(rd->t.i.special_hdr, REMOTE_ABOOK_SUBTYPE))
		   ? "address book"
		   : (rd && rd->t.i.special_hdr && !strucmp(rd->t.i.special_hdr, REMOTE_PINERC_SUBTYPE))
		       ? "configuration"
		       : "data";
    foldername = (rd && rd->rn) ? rd->rn : unknown;
    special = (rd && rd->t.i.special_hdr) ? rd->t.i.special_hdr : unknown;

    dprint((1, "rd_check_out_forged_remote_data: reason: %d, type: $s, name: %s",
	    reason, foldertype ? foldertype : "?", foldername ? foldername : "?"));

    if(rd && rd->flags & USER_SAID_NO)
      return(-1);

    if(reason == -2){
	snprintf(tmp, sizeof(tmp), _("Missing \"%s\" header in remote pinerc"), special);
	tmp[sizeof(tmp)-1] = '\0';
    }
    else if(reason == -1){
	snprintf(tmp, sizeof(tmp), _("Unexpected \"Received\" header in remote pinerc"));
	tmp[sizeof(tmp)-1] = '\0';
    }
    else if(reason >= 0){
	snprintf(tmp, sizeof(tmp), _("Unexpected value \"%s: %s\" in remote pinerc"), special, (extra && *extra) ? extra : "?");
	tmp[sizeof(tmp)-1] = '\0';
    }

    rd->flags |= USER_SAID_YES;
    return(1);
}
