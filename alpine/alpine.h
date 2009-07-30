/*
 * $Id: alpine.h 767 2007-10-24 00:03:59Z hubert@u.washington.edu $
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

#ifndef ALPINE_ALPINE_INCLUDED
#define ALPINE_ALPINE_INCLUDED


#include "../pith/state.h"


/* exported protoypes */
void	      main_menu_screen(struct pine *);
unsigned long pine_gets_bytes(int);
void	      quit_screen(struct pine *);
int	      panicking(void);
int           rule_setup_type(struct pine *ps, int flags, char *prompt);
UCS          *user_wordseps(char **);
STORE_S	     *pine_pico_get(void);
int	      pine_pico_give(STORE_S **);
int	      pine_pico_writec(int, STORE_S *);
int	      pine_pico_writec_noucs(int, STORE_S *);
int	      pine_pico_readc(unsigned char *, STORE_S *);
int	      pine_pico_readc_noucs(unsigned char *, STORE_S *);
int	      pine_pico_puts(STORE_S *, char *);
int	      pine_pico_puts_noucs(STORE_S *, char *);
int	      pine_pico_seek(STORE_S *, long, int);


#endif /* ALPINE_ALPINE_INCLUDED */
