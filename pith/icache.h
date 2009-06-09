/*
 * $Id: icache.h 136 2006-09-22 20:06:05Z hubert@u.washington.edu $
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

#ifndef PITH_ICACHE_INCLUDED
#define PITH_ICACHE_INCLUDED


#include "../pith/msgno.h"
#include "../pith/thread.h"
#include "../pith/indxtype.h"


/* flags for clear_index_cache() */
#define IC_USE_RAW_MSGNO      0x01
#define IC_CLEAR_WIDTHS_DONE  0x02


/* exported protoypes */
void	  clear_index_cache_ent(MAILSTREAM *, long, unsigned);
void	  clear_index_cache(MAILSTREAM *, unsigned);
void	  clear_index_cache_for_thread(MAILSTREAM *, PINETHRD_S *, MSGNO_S *);
void	  clear_icache_flags(MAILSTREAM *);
void	  set_need_format_setup(MAILSTREAM *);
int	  need_format_setup(MAILSTREAM *);
void	  set_format_includes_msgno(MAILSTREAM *);
int	  format_includes_msgno(MAILSTREAM *);
void	  set_format_includes_smartdate(MAILSTREAM *);
int	  format_includes_smartdate(MAILSTREAM *);
void	  free_ice(ICE_S **);
void	  clear_ice(ICE_S **);
void	  free_ifield(IFIELD_S **);
void	  free_ielem(IELEM_S **);
ICE_S	 *fetch_ice(MAILSTREAM *, unsigned long);
IFIELD_S *new_ifield(IFIELD_S **);
IELEM_S	 *new_ielem(IELEM_S **);


#endif /* PITH_ICACHE_INCLUDED */
