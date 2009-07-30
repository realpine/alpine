/*----------------------------------------------------------------------
  $Id: general.h 764 2007-10-23 23:44:49Z hubert@u.washington.edu $
  ----------------------------------------------------------------------*/

/* ========================================================================
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


#ifndef _GENERAL_INCLUDED
#define _GENERAL_INCLUDED


#include "system.h"

/*
 *  Generally useful definitions and constants
 */


/* Might as well be consistant */
#undef	FALSE
#define FALSE   0                       /* False, no, bad, etc.         */
#undef	TRUE
#define TRUE    1                       /* True, yes, good, etc.        */
#define ABORT   2                       /* Death, ^G, abort, etc.       */


#undef  MIN
#define MIN(a,b)	((a) < (b) ? (a) : (b))
#undef  MAX
#define MAX(a,b)	((a) > (b) ? (a) : (b))


#ifdef O_BINARY
#define STDIO_READ	"rb"
#define STDIO_APPEND	"a+b"
#else /* !O_BINARY */
#define O_BINARY	0
#define STDIO_READ	"r"
#define STDIO_APPEND	"a+"
#endif /* !O_BINARY */

#ifndef O_TEXT
#define O_TEXT		0
#endif /* !O_TEXT */

#ifndef _O_WTEXT
#define _O_WTEXT	0
#endif /* !O_WTEXT */

#ifndef _O_U8TEXT
#define _O_U8TEXT	0
#endif /* !O_U8TEXT */


/* Various character constants */
#define BACKSPACE	'\b'     	/* backspace character  */
#define TAB		'\t'            /* tab character        */
#define RETURN		'\r'     	/* carriage return char */
#define LINE_FEED	'\n'     	/* line feed character  */
#define FORMFEED	'\f'     	/* form feed (^L) char  */
#define COMMA		','		/* comma character      */
#define SPACE		' '		/* space character      */
#define ESCAPE		'\033'		/* the escape		*/
#define	BELL		'\007'		/* the bell		*/
#define LPAREN		'('		/* left parenthesis	*/
#define RPAREN		')'		/* right parenthesis	*/
#define BSLASH		'\\'		/* back slash		*/
#define QUOTE		'"'		/* double quote char	*/
#define DEL		'\177'		/* delete		*/

/*
 * These help with isspace when dealing with UCS-4 characters.
 * 0x3000 == Ideographic Space (as wide as a CJK character cell)
 * 0x2002 == En Space
 * 0x2003 == Em Space
 */
#define SPECIAL_SPACE(c)	((c) == 0x3000 || (c) == 0x2002 || (c) == 0x2003)




/* Longest foldername we ever expect */  
#define MAXFOLDER      (128)


/* Various maximum field lengths, probably shouldn't be changed. */
#define MAX_FULLNAME     (100) 
#define MAX_NICKNAME      (80)
#define MAX_ADDRESS      (500)
#define MAX_NEW_LIST     (500)  /* Max addrs to be added when creating list */
#define MAX_SEARCH       (100)  /* Longest string to search for             */
#define MAX_ADDR_EXPN   (1000)  /* Longest expanded addr                    */
#define MAX_ADDR_FIELD (10000)  /* Longest fully-expanded addr field        */

/*
 * Input timeout fudge factor
 */
#define IDLE_TIMEOUT	(8)
#define FUDGE		(30)	/* better be at least 20 */


/*
 * We use a 32 bit unsigned int to carry UCS-4 characters.
 * C-client uses unsigned long for this, so we have to do
 * some minor conversion at that interface. UCS-4 characters
 * are really only 21 bits. We do use the extra space for
 * defining some special values that a character might have.
 * In particular, the set of values like KEY_UP, KEY_RESIZE,
 * NO_OP_IDLE, F12, and so on are 32 bit quantities that don't
 * interfere with the actual character values. They are also
 * all positive values with the most significant bit set to 0,
 * so a 32 bit signed integer could hold them all.
 */
typedef UINT32 UCS;

/*
 * The type of an IMAP UID, which is a 32-bit unsigned int.
 * This could be UINT32 instead of unsigned long but we use
 * unsigned long because that's what c-client uses.
 */
typedef unsigned long imapuid_t;


#endif /* _GENERAL_INCLUDED */
