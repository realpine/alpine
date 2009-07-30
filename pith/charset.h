/*
 * $Id: charset.h 765 2007-10-23 23:51:37Z hubert@u.washington.edu $
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

#ifndef PITH_CHARSET_INCLUDED
#define PITH_CHARSET_INCLUDED


#include "../pith/filttype.h"


typedef struct conversion_table {
    char          *from_charset;
    char          *to_charset;
    int            quality;
    void          *table;
    filter_t       convert;
} CONV_TABLE;


/* Conversion table quality of tranlation */
#define	CV_NO_TRANSLATE_POSSIBLE	1	/* We don't know how to      */
						/* translate this pair       */
#define	CV_NO_TRANSLATE_NEEDED		2	/* Not necessary, no-op      */
#define	CV_LOSES_SPECIAL_CHARS		3	/* Letters will translate    */
						/* ok but some special chars */
						/* may be lost               */
#define	CV_LOSES_SOME_LETTERS		4	/* Some special chars and    */
						/* some letters may be lost  */


#define	CSET_MAX		64


/* exported protoypes */
char	      *body_charset(MAILSTREAM *, long, unsigned char *);
unsigned char *trans_euc_to_2022_jp(unsigned char *);
unsigned char *rfc1522_decode_to_utf8(unsigned char *, size_t, char *);
char	      *rfc1522_encode(char *, size_t, unsigned char *, char *);
CONV_TABLE    *conversion_table(char *, char *);
void           decode_addr_names_to_utf8(ADDRESS *);
void           convert_possibly_encoded_str_to_utf8(char **);


#endif /* PITH_CHARSET_INCLUDED */
