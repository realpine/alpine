/*
 * $Id: termin.gen.h 890 2007-12-21 05:34:43Z hubert@u.washington.edu $
 *
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

#ifndef PINE_OSDEP_TERMIN_GEN_INCLUDED
#define PINE_OSDEP_TERMIN_GEN_INCLUDED

#include <general.h>
#include "../radio.h"

/* Useful Macros */
#define	READ_COMMAND(U)	(read_command_prep() ? read_command(U) : NO_OP_COMMAND)

/* exported prototypes */
UCS	read_command(char **);
int	read_command_prep(void);
int	key_recorder(int);
int	optionally_enter(char *, int, int, int, char *, ESCKEY_S *, HelpType, int *);
UCS	validatekeys(UCS);
int	process_config_input(UCS *);
int	key_recorder(int);
int	key_playback(int);
int	recent_keystroke(int *, char *, size_t);
int	init_tty_driver(struct pine *);
void	end_tty_driver(struct pine *);
int	PineRaw(int);
UCS     read_char(int);
void	init_keyboard(int);
void	end_keyboard(int);
int	pre_screen_config_opt_enter(char *, int, char *,
				    ESCKEY_S *, HelpType, int *);
#ifdef	_WINDOWS
int	pcpine_oe_cursor(int, long);
#endif

#endif /* PINE_OSDEP_TERMIN_GEN_INCLUDED */
