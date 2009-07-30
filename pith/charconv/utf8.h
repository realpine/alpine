/*-----------------------------------------------------------------------
 $Id: utf8.h 1025 2008-04-08 22:59:38Z hubert@u.washington.edu $
  -----------------------------------------------------------------------*/

/*
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

#ifndef PITH_CHARCONV_UTF8_INCLUDED
#define PITH_CHARCONV_UTF8_INCLUDED


#include <general.h>
#include "../filttype.h"


/* flags for convert_to_utf8 */
#define CU8_NONE	0x00
#define CU8_NOINFER	0x01	/* Not ok to infer charset */


/*
 * The data in vl and dl is UCS-4 characters.
 * They are arrays of size vlen and dlen of unsigned longs.
 */
struct display_line {
    int   row, col;	/* where display starts					*/
    UCS  *vl;		/* virtual line, the actual data string			*/
    int   vlen;		/* size of vl array					*/
    int   vused;	/* elements of vl in use				*/
    int   vbase;	/* index into array, first virtual char on display	*/
    UCS  *dl;		/* visible part of virtual line on display		*/
    UCS  *olddl;
    int   dlen;		/* size of dl array					*/
    int   dwid;		/* screenwidth avail for dl				*/
    void  (*movecursor)(int, int);
    void  (*writechar)(UCS);
};


/*
 * Exported Prototypes
 */
void           init_utf8_display(int, void *);
int            wcellwidth(UCS);
int            wtomb(char *, UCS);
UCS            mbtow(void *, unsigned char **, unsigned long *);
void           set_locale_charmap(char *);
char          *convert_to_utf8(char *, char *, int);
char          *convert_to_locale(char *);
int            utf8_to_locale(int c, CBUF_S *cb, unsigned char obuf[], size_t obuf_size);
unsigned       ucs4_str_width(UCS *);
unsigned       ucs4_str_width_a_to_b(UCS *, int, int);
unsigned       ucs4_str_width_ptr_to_ptr(UCS *, UCS *);
UCS           *ucs4_particular_width(UCS*, int);
UCS           *utf8_to_ucs4_cpystr(char *);
char          *ucs4_to_utf8_cpystr(UCS *);
char          *ucs4_to_utf8_cpystr_n(UCS *, int);
#ifdef _WINDOWS
LPTSTR         utf8_to_lptstr(LPSTR);
LPSTR          lptstr_to_utf8(LPTSTR);
LPTSTR         ucs4_to_lptstr(UCS *);
UCS           *lptstr_to_ucs4(LPTSTR);
#endif /* _WINDOWS */
int            utf8_to_ucs4_oneatatime(int, CBUF_S *, UCS *, int *);
size_t         ucs4_strlen(UCS *s);
int            ucs4_strcmp(UCS *s1, UCS *s2);
UCS           *ucs4_cpystr(UCS *s);
UCS           *ucs4_strncpy(UCS *ucs4dst, UCS *ucs4src, size_t n);
UCS           *ucs4_strncat(UCS *ucs4dst, UCS *ucs4src, size_t n);
UCS           *ucs4_strchr(UCS *s, UCS c);
UCS           *ucs4_strrchr(UCS *s, UCS c);
unsigned       utf8_width(char *);
size_t         utf8_to_width_rhs(char *, char *, size_t, unsigned);
int            utf8_snprintf(char *, size_t, char *, ...);
size_t         utf8_to_width(char *, char *, size_t, unsigned, unsigned *);
size_t         utf8_pad_to_width(char *, char *, size_t, unsigned, int);
unsigned       utf8_truncate(char *, unsigned);
char          *utf8_count_back_width(char *, char *, unsigned, unsigned *);
char          *utf8_count_forw_width(char *, unsigned, unsigned *);
void	       sstrncpy(char **, char *, int);
int            setup_for_input_output(int, char **, char **, void **, char **);
int            input_charset_is_supported(char *);
int            output_charset_is_supported(char *);
int            posting_charset_is_supported(char *);
char          *utf8_to_charset(char *, char *, int);
char          *comatose(long);
char          *tose(long);
void           line_paint(int, struct display_line *, int *);

#if	!defined(_WINDOWS) && HAVE_LANGINFO_H && defined(CODESET)
char          *nl_langinfo_codeset_wrapper(void);
#endif


#endif	/* PITH_CHARCONV_UTF8_INCLUDED */
