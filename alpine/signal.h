/*
 * $Id: signal.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PINE_SIGNAL_INCLUDED
#define PINE_SIGNAL_INCLUDED


#include <general.h>

#include "../pith/osdep/pipe.h"	/* for PIPE_S */

#include "../pith/state.h"
#include "../pith/signal.h"

#define MAX_BM			150  /* max length of busy message */


/* exported protoypes */
RETSIGTYPE  hup_signal(void);
RETSIGTYPE  child_signal(int);
void	    user_input_timeout_exit(int);
void	    init_signals(void);
void	    init_sigwinch(void);
void	    end_signals(int);
int	    ttyfix(int);
UCS	    do_suspend(void);
void	    winch_cleanup(void);
void	    pipe_callback(PIPE_S *, int, void *);
void	    fix_windsize(struct pine *);


#endif /* PINE_SIGNAL_INCLUDED */
