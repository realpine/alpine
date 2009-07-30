/*
 * $Id: text.h 768 2007-10-24 00:10:03Z hubert@u.washington.edu $
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

#ifndef PITH_TEXT_INCLUDED
#define PITH_TEXT_INCLUDED


#include "../pith/atttype.h"
#include "../pith/filter.h"
#include "../pith/handle.h"


typedef enum {InLine, QStatus} DetachErrStyle;


typedef struct local_for_2022_jp_trans {
    int report_err;
} LOC_2022_JP;


/*
 * If the number of lines in the quote is equal to lines or less, then
 * we show the quote. If the number of lines in the quote is more than lines,
 * then we show lines-1 of the quote followed by one line of editorial
 * comment.
 */
typedef struct del_q_s {
    int        lines;		/* show this many lines (counting editorial) */
    int        indent_length;	/* skip over this indent                     */
    int        is_flowed;	/* message is labelled flowed                */
    int        do_color;
    int        delete_all;	/* delete quotes completely, no comments     */
    HANDLE_S **handlesp;
    int        in_quote;	/* dynamic data */
    char     **saved_line;	/*   "          */
} DELQ_S;


/* exported protoypes */
int	decode_text(ATTACH_S *, long, gf_io_t, HANDLE_S **, DetachErrStyle, int);
int	translate_utf8_to_2022_jp(long, char *, LT_INS_S **, void *);
int	delete_quotes(long, char *, LT_INS_S **, void *);


#endif /* PITH_TEXT_INCLUDED */
