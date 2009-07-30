/*
 * $Id: radio.h 1025 2008-04-08 22:59:38Z hubert@u.washington.edu $
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

#ifndef PINE_RADIO_INCLUDED
#define PINE_RADIO_INCLUDED

#include "../pith/helptext.h"

#include "help.h"


/* 
 * For optionally_enter and radio_buttons, special meanings for keys.
 */
typedef struct esckey {
    long ch;			/* holds all UCS values plus -1 and -2 */
    int  rval;			/* return value                        */
    char *name;			/* short name                          */
    char *label;		/* long descriptive key name           */
} ESCKEY_S;


/*
 * Minimum space needed after the prompt for optionally_enter() to work well.
 * If the available width after the prompt is smaller than this then the prompt will
 * have the leading edge cut off by optionally_enter.
 * (note: used to be 5)
 */
#define MIN_OPT_ENT_WIDTH (7)

/* max length prompt for optionally_enter and want_to */
#define MAXPROMPT	(ps_global->ttyo->screen_cols - MIN_OPT_ENT_WIDTH)


#define SEQ_EXCEPTION   (-3)		/* returned when seq # change and no
					   on_ctrl_C available.          */
#define	RB_NORM		0x00		/* flags modifying radio_buttons */
#define	RB_ONE_TRY	0x01		/* one shot answer, else default */
#define	RB_FLUSH_IN	0x02		/* discard pending input	 */
#define	RB_NO_NEWMAIL	0x04		/* Quell new mail check		 */
#define	RB_SEQ_SENSITIVE 0x08		/* Sensitive to seq # changes    */
#define	RB_RET_HELP	0x10		/* Return when help key pressed  */


#define OE_NONE			0x00	/* optionally_enter flags	*/
#define OE_DISALLOW_CANCEL	0x01	/* Turn off Cancel menu item	*/
#define	OE_DISALLOW_HELP	0x02	/* Turn off Help menu item	*/
#define OE_USER_MODIFIED	0x08	/* set on return if user input	*/
#define OE_KEEP_TRAILING_SPACE  0x10	/* Allow trailing white-space	*/
#define OE_SEQ_SENSITIVE  	0x20	/* Sensitive to seq # changes   */
#define OE_APPEND_CURRENT  	0x40	/* append, don't truncate       */
#define OE_PASSWD  		0x80	/* Use asterisks to echo input  */
#define OE_PASSWD_NOAST	  	0x100	/* Don't echo user input at all */

#define	WT_NORM		 0x00		/* flags modifying want_to       */
#define	WT_FLUSH_IN	 0x01		/* discard pending input         */
#define	WT_SEQ_SENSITIVE 0x02 		/* Sensitive to seq # changes  */


/* exported protoypes */
int	want_to(char *, int, int, HelpType, int);
int	one_try_want_to(char *, int, int, HelpType, int);
int	radio_buttons(char *, int, ESCKEY_S *, int, int, HelpType, int);
int	double_radio_buttons(char *, int, ESCKEY_S *, int, int, HelpType, int);
					


#endif /* PINE_RADIO_INCLUDED */
