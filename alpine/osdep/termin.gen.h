/*
 * $Id: termin.gen.h 136 2006-09-22 20:06:05Z hubert@u.washington.edu $
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

#ifndef PINE_OSDEP_TERMIN_GEN_INCLUDED
#define PINE_OSDEP_TERMIN_GEN_INCLUDED

#include <general.h>
#include "../radio.h"

/* exported prototypes */
UCS	read_command(char **);
int	key_recorder(int);
int	optionally_enter(char *, int, int, int, char *, ESCKEY_S *, HelpType, int *);
UCS	validatekeys(UCS);
int	process_config_input(UCS *);
int	key_recorder(int);
int	key_playback(int);
int	recent_keystroke(int *, char *, size_t);
#ifdef	_WINDOWS
int	pcpine_oe_cursor(int, long);
#endif

#endif /* PINE_OSDEP_TERMIN_GEN_INCLUDED */
