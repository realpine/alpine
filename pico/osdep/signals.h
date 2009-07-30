/*
 * $Id: signals.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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


#ifndef PICO_OSDEP_SIGNALS_INCLUDED
#define PICO_OSDEP_SIGNALS_INCLUDED


/*
 * [Re]Define signal functions as needed...
 */
#ifdef	POSIX_SIGNALS
/*
 * Redefine signal call to our wrapper of POSIX sigaction
 */
#define	signal(SIG,ACT)		posix_signal(SIG,ACT)
#define	our_sigunblock(SIG)	posix_sigunblock(SIG)
#else	/* !POSIX_SIGNALS */
#ifdef	SYSV_SIGNALS
/*
 * Redefine signal calls to SYSV style call.
 */
#define	signal(SIG,ACT)		sigset(SIG,ACT)
#define	our_sigunblock(SIG)	sigrelse(SIG)
#else	/* !SYSV_SIGNALS */
#ifdef	_WINDOWS
#define	our_sigunblock(SIG)
#else	/* !_WINDOWS */
/*
 * Good ol' BSD signals.
 */
#define	our_sigunblock(SIG)
#endif /* !_WINDOWS */
#endif /* !SYSV_SIGNALS */
#endif /* !POSIX_SIGNALS */



/* exported prototypes */
void		picosigs(void);
#if	defined(SIGWINCH) && defined(TIOCGWINSZ)
RETSIGTYPE	winch_handler(int);
#endif

#ifdef POSIX_SIGNALS
void		(*posix_signal(int, RETSIGTYPE (*)()))(int);
int		posix_sigunblock(int);
#endif

#endif /* PICO_OSDEP_SIGNALS_INCLUDED */
