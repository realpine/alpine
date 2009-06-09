/*
 * $Id: text.h 143 2006-09-27 23:24:39Z hubert@u.washington.edu $
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

#ifndef PITH_TEXT_INCLUDED
#define PITH_TEXT_INCLUDED


#include "../pith/atttype.h"
#include "../pith/filter.h"
#include "../pith/handle.h"


typedef enum {InLine, QStatus} DetachErrStyle;


typedef struct local_for_2022_jp_trans {
    int report_err;
} LOC_2022_JP;


/* exported protoypes */
int	decode_text(ATTACH_S *, long, gf_io_t, HANDLE_S **, DetachErrStyle, int);
int	translate_utf8_to_2022_jp(long, char *, LT_INS_S **, void *);


#endif /* PITH_TEXT_INCLUDED */
