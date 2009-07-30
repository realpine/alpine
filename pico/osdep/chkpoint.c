#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: chkpoint.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
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


#include <system.h>
#include <general.h>

#include "../estruct.h"
#include "../mode.h"
#include "../pico.h"
#include "../edef.h"
#include "../efunc.h"
#include "../keydefs.h"
#include "filesys.h"

#include "../../pith/charconv/filesys.h"

#include "chkpoint.h"


/*
 * chkptinit -- initialize anything we need to support composer
 *		checkpointing
 */
void
chkptinit(char *file, size_t filelen)
{
#ifndef _WINDOWS
    unsigned pid;
    char    *chp;
#endif

    if(!file[0]){
	long gmode_save = gmode;

	if(gmode&MDCURDIR)
	  gmode &= ~MDCURDIR;  /* so fixpath will use home dir */

#ifndef _WINDOWS
	strncpy(file, "#picoXXXXX#", filelen);
#else
	strncpy(file, "#picoTM0.txt", filelen);
#endif
	file[filelen-1] = '\0';
	fixpath(file, filelen);
	gmode = gmode_save;
    }
    else{
	size_t l = strlen(file);

	if(l+2 <= filelen && file[l-1] != C_FILESEP){
	    file[l++] = C_FILESEP;
	    file[l]   = '\0';
	}

#ifndef _WINDOWS
	strncpy(file + l, "#picoXXXXX#", filelen-l);
#else
	strncpy(file + l, "#picoTM0.txt", filelen-l);
#endif
	file[filelen-1] = '\0';
    }

#ifndef _WINDOWS
    pid = (unsigned)getpid();
    for(chp = file+strlen(file) - 2; *chp == 'X'; chp--){
	*chp = (pid % 10) + '0';
	pid /= 10;
    }
#else
    if(fexist(file, "r", (off_t *)NULL) == FIOSUC){ /* does file exist? */
	char copy[NLINE];

	strncpy(copy, "#picoTM1.txt", sizeof(copy));
	copy[sizeof(copy)-1] = '\0';
	fixpath(copy, sizeof(copy));
	our_rename(file, copy);  /* save so we don't overwrite it */
    }
#endif /* _WINDOWS */

    our_unlink(file);
}

