/*
 * $Id: forkwait.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PITH_OSDEP_FORKWAIT_INCLUDED
#define PITH_OSDEP_FORKWAIT_INCLUDED


#if	HAVE_UNION_WAIT
#define WAITSTATUS_T	union wait
#else
#define WAITSTATUS_T	int
#endif

#ifndef	WEXITSTATUS
#define	WEXITSTATUS(X)	(((X) >> 8) & 0xff)	/* high bits tell exit value */
#endif
#ifndef	WIFEXITED
#define	WIFEXITED(X)	(!((X) & 0xff))		/* low bits tell how it died */
#endif

#if	!HAVE_WORKING_VFORK
#define	vfork fork
#endif


#endif /* PITH_OSDEP_FORKWAIT_INCLUDED */

