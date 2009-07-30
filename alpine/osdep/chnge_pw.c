#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: chnge_pw.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
#endif

/*
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

#include <system.h>		/* os-dep defs/includes */
#include <general.h>		/* generally useful definitions */

#include "../../c-client/mail.h"	/* for MAILSTREAM and friends */
#include "../../c-client/osdep.h"
#include "../../c-client/rfc822.h"	/* for soutr_t and such */
#include "../../c-client/misc.h"	/* for cpystr proto */
#include "../../c-client/utf8.h"	/* for CHARSET and such*/
#include "../../c-client/imap4r1.h"

#include "../../pith/osdep/color.h"
#include "../../pith/state.h"

#include "termin.gen.h"
#include "termout.gen.h"
#include "chnge_pw.h"

/*----------------------------------------------------------------------
       Call the system to change the passwd
 
It would be nice to talk to the passwd program via a pipe or ptty so the
user interface could be consistent, but we can't count on the the prompts
and responses from the passwd program to be regular so we just let the user 
type at the passwd program with some screen space, hope he doesn't scroll 
off the top and repaint when he's done.
 ----*/        
void
change_passwd(void)
{
#ifdef	PASSWD_PROG
    char cmd_buf[100];

    ClearLines(1, ps_global->ttyo->screen_rows - 1);

    MoveCursor(5, 0);
    fflush(stdout);

    PineRaw(0);
    strncpy(cmd_buf, PASSWD_PROG, sizeof(cmd_buf));
    cmd_buf[sizeof(cmd_buf)-1] = '\0';
    system(cmd_buf);
    sleep(3);
    PineRaw(1);
#endif /* PASSWD_PROG */
}


