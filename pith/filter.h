/*
 * $Id: filter.h 1169 2008-08-27 06:42:06Z hubert@u.washington.edu $
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

#ifndef PITH_FILTER_INCLUDED
#define PITH_FILTER_INCLUDED


#include "../pith/filttype.h"
#include "../pith/handle.h"
#include "../pith/store.h"
#include "../pith/osdep/pipe.h"


/* gf_html2plain flags */
#define GFHP_STRIPPED		0x01
#define GFHP_HANDLES		0x02
#define GFHP_LOCAL_HANDLES	0x04
#define GFHP_NO_RELATIVE	0x08
#define GFHP_RELATED_CONTENT	0x10
#define	GFHP_SHOW_SERVER	0x20
#define	GFHP_HTML		0x40
#define	GFHP_HTML_IMAGES	0x80


/* gf_wrap flags */
#define	GFW_NONE		0x000	/* no flags			*/
#define	GFW_HANDLES		0x001	/* anticpate handle data	*/
#define	GFW_ONCOMMA		0x002	/* prefer comma wrap to spaces  */
#define	GFW_FLOWED		0x004
#define	GFW_FLOW_RESULT		0x008
#define	GFW_DELSP		0x010
#define GFW_USECOLOR		0x020
#define GFW_HDRCOLOR		0x040
#define GFW_FORCOMPOSE          0x080
#define GFW_SOFTHYPHEN          0x100	/* do something special with soft-hyphens */


/* gf_url_hilite flags */
#define	URH_NONE		0x00	/* no flags			    */
#define	URH_HDRCOLOR		0x01	/* in header, use header base color */


#define TAG_EMBED	'\377'	/* Announces embedded data in text string */
#define	TAG_INVON	'A'	/* Supported character attributes	  */
#define	TAG_INVOFF	'a'
#define	TAG_BOLDON	'B'
#define	TAG_BOLDOFF	'b'
#define	TAG_ULINEON	'C'
#define	TAG_ULINEOFF	'c'
#define	TAG_FGCOLOR	'D'	/* Change to this foreground color	  */
#define	TAG_BGCOLOR	'd'	/* Change to this background color	  */
#define	TAG_ITALICON	'E'
#define	TAG_ITALICOFF	'e'
#define	TAG_STRIKEON	'F'
#define	TAG_STRIKEOFF	'f'
#define	TAG_BIGON	'G'
#define	TAG_BIGOFF	'g'
#define	TAG_SMALLON	'H'
#define	TAG_SMALLOFF	'h'
#define	TAG_HANDLE	'Z'	/* indicate's a handle to an action	  */
#define	TAG_HANDLEOFF	'z'	/* indicate's end of handle text	  */


typedef struct url_hilite_s {
    HANDLE_S **handlesp;
    int        hdr_color;
} URL_HILITE_S;

typedef struct _rss_item_s {
	char *title;
	char *link;
	char *description;
	char *source;
	struct _rss_item_s *next;
} RSS_ITEM_S;

typedef struct _rss_feed_s {
	char   *title;
	char   *image;
	char   *link;
	char   *description;
	char   *source;
	int     ttl;
	struct _rss_item_s *items;
} RSS_FEED_S;


/*
 * This searchs for lines beginning with From<space> so that we can QP-encode
 * them.  It also searches for lines consisting of only a dot.  Some mailers
 * will mangle these lines.  The reason it is ifdef'd is because most people
 * seem to prefer the >From style escape provided by a lot of mail software
 * to the QP-encoding.
 * Froms, dots, bmap, and dmap may be any integer type.  C is the next char.
 * bmap and dmap should be initialized to 1.
 * froms is incremented by 1 whenever a line beginning From_ is found.
 * dots is incremented by 1 whenever a line with only a dot is found.
 */
#define Find_Froms(froms,dots,bmap,dmap,c) { int x,y;			\
				switch (c) {				\
				  case '\n': case '\r':			\
				    x = 0x1;				\
				    y = 0x7;				\
				    bmap = 0;				\
				    break;				\
				  case 'F':				\
				    x = 0x3;				\
				    y = 0;				\
				    break;				\
				  case 'r':				\
				    x = 0x7;				\
				    y = 0;				\
				    break;				\
				  case 'o':				\
				    x = 0xf;				\
				    y = 0;				\
				    break;				\
				  case 'm':				\
				    x = 0x1f;				\
				    y = 0;				\
				    break;				\
				  case ' ':				\
				    x = 0x3f;				\
				    y = 0;				\
				    break;				\
				  case '.':				\
				    x = 0;				\
				    y = 0x3;				\
				    break;				\
				  default:				\
				    x = 0;				\
				    y = 0;				\
				    break;				\
				}					\
				bmap = ((x >> 1) == bmap) ? x : 0;	\
				froms += (bmap == 0x3f ? 1 : 0);	\
				if(y == 0x7 && dmap != 0x3){		\
				    y = 0x1;				\
				    dmap = 0;				\
				}					\
				dmap = ((y >> 1) == dmap) ? y : 0;	\
				dots += (dmap == 0x7 ? 1 : 0);		\
			     }


/* exported protoypes */
int	    generic_readc_locale(unsigned char *c,
				 int (*get_a_char)(unsigned char *, void *),
				 void *extraarg,
				 CBUF_S *cb);
int	    pc_is_picotext(gf_io_t);
void	    gf_set_readc(gf_io_t *, void *, unsigned long, SourceType, int);
void	    gf_set_writec(gf_io_t *, void *, unsigned long, SourceType, int);
void	    gf_set_so_readc(gf_io_t *, STORE_S *);
void	    gf_clear_so_readc(STORE_S *);
void	    gf_set_so_writec(gf_io_t *, STORE_S *);
void	    gf_clear_so_writec(STORE_S *);
int	    gf_puts(char *, gf_io_t);
int	    gf_nputs(char *, long, gf_io_t);
void	    gf_filter_init(void);
void	    gf_link_filter(filter_t, void *);
void	    gf_set_terminal(gf_io_t);
char	   *gf_pipe(gf_io_t, gf_io_t);
long	    gf_bytes_piped(void);
char	   *gf_filter(char *, char *, STORE_S *, gf_io_t, FILTLIST_S *, int,
		      void (*)(PIPE_S *, int, void *));
void	    gf_binary_b64(FILTER_S *, int);
void	    gf_b64_binary(FILTER_S *, int);
void	    gf_qp_8bit(FILTER_S *, int);
void	    gf_8bit_qp(FILTER_S *, int);
void	    gf_convert_8bit_charset(FILTER_S *, int);
void	    gf_convert_utf8_charset(FILTER_S *, int);
void	   *gf_convert_utf8_charset_opt(void *, int);
void	    gf_2022_jp_to_euc(FILTER_S *, int);
void	    gf_native8bitjapanese_to_2022_jp(FILTER_S *, int);
void	    gf_euc_to_2022_jp(FILTER_S *, int);
void	    gf_sjis_to_2022_jp(FILTER_S *, int);
void	    gf_utf8(FILTER_S *, int);
void	   *gf_utf8_opt(char *);
void	    gf_rich2plain(FILTER_S *, int);
void	   *gf_rich2plain_opt(int *);
void	    gf_enriched2plain(FILTER_S *, int);
void	   *gf_enriched2plain_opt(int *);
void	    gf_html2plain(FILTER_S *, int);
void	   *gf_html2plain_opt(char *, int, int *, HANDLE_S **, htmlrisk_t, int);
void	   *gf_html2plain_rss_opt(RSS_FEED_S **, int);
void	    gf_html2plain_rss_free(RSS_FEED_S **);
void	    gf_html2plain_rss_free_items(RSS_ITEM_S **);
void	    gf_escape_filter(FILTER_S *, int);
void	    gf_control_filter(FILTER_S *, int);
void	   *gf_control_filter_opt(int *);
void	    gf_tag_filter(FILTER_S *, int);
void	    gf_wrap(FILTER_S *, int);
void	   *gf_wrap_filter_opt(int, int, int *, int, int);
void	    gf_preflow(FILTER_S *, int);
void	    gf_prefix(FILTER_S *, int);
void	   *gf_prefix_opt(char *);
void	    gf_line_test(FILTER_S *, int);
void	   *gf_line_test_opt(linetest_t, void *);
LT_INS_S  **gf_line_test_new_ins(LT_INS_S **, char *, char *, int);
void	    gf_line_test_free_ins(LT_INS_S **);
void	    gf_prepend_editorial(FILTER_S *, int);
void	   *gf_prepend_editorial_opt(prepedtest_t, char *);
void	    gf_nvtnl_local(FILTER_S *, int);
void	    gf_local_nvtnl(FILTER_S *, int);
void	   *gf_url_hilite_opt(URL_HILITE_S *, HANDLE_S **, int);


#endif /* PITH_FILTER_INCLUDED */
