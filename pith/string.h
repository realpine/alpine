/*
 * $Id: string.h 769 2007-10-24 00:15:40Z hubert@u.washington.edu $
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

#ifndef PITH_STRING_INCLUDED
#define PITH_STRING_INCLUDED


/*
 * Hex conversion aids
 */
#define HEX_ARRAY	"0123456789ABCDEF"
#define	HEX_CHAR1(C)	HEX_ARRAY[((C) & 0xf0) >> 4]
#define	HEX_CHAR2(C)	HEX_ARRAY[(C) & 0xf]

#define	XDIGIT2C(C)	((C) - (isdigit((unsigned char) (C)) \
			  ? '0' : (isupper((unsigned char)(C))? '7' : 'W')))

#define	X2C(S)		((XDIGIT2C(*(S)) << 4) | XDIGIT2C(*((S)+1)))

#define	C2XPAIR(C, S)	{ \
			    *(S)++ = HEX_CHAR1(C); \
			    *(S)++ = HEX_CHAR2(C); \
			}


/* for flags to fold() routine */
#define FLD_NONE	0x00
#define FLD_CRLF	0x01	/* use CRLF end of line instead of LF */
#define FLD_PWS		0x02	/* preserve whitespace when folding   */


typedef enum {FrontDots, MidDots, EndDots} WhereDots;


/*
 * Macro to help determine when we need to filter out chars
 * from message index or headers...
 */
#define	FILTER_THIS(c)	(((((unsigned char) (c) < 0x20                  \
                            || (unsigned char) (c) == 0x7f)             \
                            && !ps_global->pass_ctrl_chars)             \
                           || (((unsigned char) (c) >= 0x80             \
                                && (unsigned char) (c) < 0xA0)          \
                            && !ps_global->pass_ctrl_chars              \
			    && !ps_global->pass_c1_ctrl_chars))         \
			 && !((c) == SPACE				\
			      || (c) == TAB				\
			      || (c) == '\016'				\
			      || (c) == '\017'))


/*
 * Keeps track of selected folders between instances of
 * the folder list screen.
 */
typedef	struct name_list {
    char		*name;
    struct name_list	*next;
} STRLIST_S;


struct date {
    int	 sec, minute, hour, day, month, 
	 year, hours_off_gmt, min_off_gmt, wkday;
};


/* just a convenient place to put these so everything can access */
#define BUILDER_SCREEN_MANGLED		0x1
#define BUILDER_MESSAGE_DISPLAYED	0x2
#define BUILDER_FOOTER_MANGLED		0x4


/* exported protoypes */
char	   *rplstr(char *, size_t, int, char *);
void	    sqzspaces(char *);
void	    sqznewlines(char *);
void	    removing_leading_white_space(char *);
void	    removing_trailing_white_space(char *);
void	    removing_leading_and_trailing_white_space(char *);
int 	    removing_double_quotes(char *);
char	   *skip_white_space(char *);
char	   *skip_to_white_space(char *);
char	   *removing_quotes(char *);
char	   *strclean(char *);
char	   *short_str(char *, char *, size_t, int, WhereDots);
char	   *srchstr(char *, char *);
char	   *srchrstr(char *, char *);
char	   *strindex(char *, int);
char	   *strrindex(char *, int);
char	   *iutf8ncpy(char *, char *, int);
char	   *istrncpy(char *, char *, int);
char	   *month_abbrev(int);
char	   *month_abbrev_locale(int);
char	   *month_name(int);
char	   *month_name_locale(int);
char	   *day_abbrev(int);
char	   *day_abbrev_locale(int);
char	   *day_name(int);
char	   *day_name_locale(int);
size_t      our_strftime(char *, size_t, char *, struct tm *);
int	    month_num(char *);
void	    parse_date(char *, struct date *);
char       *convert_date_to_local(char *);
char	   *repeat_char(int, int);
char	   *byte_string(long);
char	   *enth_string(int);
char       *fold(char *, int, int, char *, char *, unsigned);
char	   *strsquish(char *, size_t, char *, int);
char	   *long2string(long);
char	   *ulong2string(unsigned long);
char	   *int2string(int);
char	   *strtoval(char *, int *, int, int, int, char *, size_t, char *);
char	   *strtolval(char *, long *, long, long, long, char *, size_t, char *);
void	    get_pair(char *, char **, char **, int, int);
char	   *put_pair(char *, char *);
char       *quote_if_needed(char *);
int	    read_hex(char *);
char	   *string_to_cstring(char *);
char	   *cstring_to_hexstring(char *);
void	    cstring_to_string(char *, char *);
char	   *add_backslash_escapes(char *);
char	   *remove_backslash_escapes(char *);
char	   *add_viewerhdr_escapes(char *);
char	   *vcard_escape(char *);
char	   *vcard_unescape(char *);
void	    vcard_unfold(char *);
char       *add_escapes(char *, char *, int, char *, char *);
char       *copy_quoted_string_asis(char *);
int	    isxpair(char *);
STRLIST_S  *new_strlist(char *);
STRLIST_S  *copy_strlist(STRLIST_S *);
void	    combine_strlists(STRLIST_S **, STRLIST_S *);
void	    free_strlist(STRLIST_S **);
int         read_octal(char **);


#endif /* PITH_STRING_INCLUDED */
