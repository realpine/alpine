/*
 * $Id: talk.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PITH_TALK_INCLUDED
#define PITH_TALK_INCLUDED


/*
 * Macros to aid hack to turn off talk permission.
 * Bit 020 is usually used to turn off talk permission, we turn off
 * 002 also for good measure, since some mesg commands seem to do that.
 */
#define	TALK_BIT		020		/* mode bits */
#define	GM_BIT			002
#define	TMD_CLEAR		0		/* functions */
#define	TMD_SET			1
#define	TMD_RESET		2
#define	allow_talk(p)		tty_chmod((p), TALK_BIT, TMD_SET)
#define	disallow_talk(p)	tty_chmod((p), TALK_BIT|GM_BIT, TMD_CLEAR)


/* exported protoypes */


#endif /* PITH_TALK_INCLUDED */
