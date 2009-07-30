/*
 * $Id: mode.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef	MODE_H
#define	MODE_H


/*
 * definitions for various PICO modes 
 */
#define	MDWRAP		0x00000001	/* word wrap			*/
#define	MDSPELL		0x00000002	/* spell error parcing		*/
#define	MDEXACT		0x00000004	/* Exact matching for searches	*/
#define	MDVIEW		0x00000008	/* read-only buffer		*/
#define MDFKEY		0x00000010	/* function key  mode		*/
#define MDSCUR		0x00000020	/* secure (for demo) mode	*/
#define MDSSPD		0x00000040	/* suspendable mode		*/
#define MDADVN		0x00000080	/* Pico's advanced mode		*/
#define MDTOOL		0x00000100	/* "tool" mode (quick exit)	*/
#define MDBRONLY	0x00000200	/* indicates standalone browser	*/
#define MDCURDIR	0x00000400	/* use current dir for lookups	*/
#define MDALTNOW	0x00000800	/* enter alt ed sans hesitation */
#define	MDSPWN		0x00001000	/* spawn subshell for suspend	*/
#define	MDCMPLT		0x00002000	/* enable file name completion  */
#define	MDDTKILL	0x00004000	/* kill from dot to eol		*/
#define	MDSHOCUR	0x00008000	/* cursor follows hilite in browser */
#define	MDHBTIGN	0x00010000	/* ignore chars with hi bit set */
#define	MDDOTSOK	0x00020000	/* browser displays dot files   */
#define	MDEXTFB		0x00040000	/* stand alone File browser     */
#define	MDTREE		0x00080000	/* confine to a subtree         */
#define	MDMOUSE		0x00100000	/* allow mouse (part. in xterm) */
#define	MDONECOL	0x00200000	/* single column browser        */
#define	MDHDRONLY	0x00400000	/* header editing exclusively   */
#define	MDGOTO		0x00800000	/* support "Goto" in file browser */
#define MDREPLACE       0x01000000      /* allow "Replace" in "Where is"*/
#define MDTCAPWINS      0x02000000      /* Termcap overrides defaults   */

#endif /* MODE_H */
