#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: editorial.c 203 2006-10-26 17:23:46Z hubert@u.washington.edu $";
#endif

/* ========================================================================
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

/*======================================================================

     editorial.c
     Implements editorial text insertion/formatting

  ====*/


#include "../pith/headers.h"
#include "../pith/conf.h"
#include "../pith/state.h"
#include "../pith/margin.h"
#include "../pith/filttype.h"
#include "../pith/handle.h"
#include "../pith/mailview.h"
#include "../pith/editorial.h"

/*
 * Internal prototypes
 */
int	 quote_editorial(long, char *, LT_INS_S **, void *);


/*
 * Struct to help with editorial comment insertion
 */
#define	EDITORIAL_MAX	128
typedef struct _editorial_s {
    char prefix[EDITORIAL_MAX];
    int	 prelen;
    char postfix[EDITORIAL_MAX];
    int	 postlen;
} EDITORIAL_S;


char *
format_editorial(char *s, int width, int flags, HANDLE_S **handlesp, gf_io_t pc)
{
    gf_io_t   gc;
    char     *t;
    size_t    n, len;
    int      *margin;
    unsigned char *p, *tmp = NULL;
    EDITORIAL_S es;
 
    /*
     * Warning. Some callers of this routine use the first half
     * of tmp_20k_buf to store the passed in source string, out to as
     * many as 10000 characters. However, we don't need to preserve the
     * source string after we have copied it.
     */
    if((n = 4*strlen(s)) > SIZEOF_20KBUF-10000-1){
	len = n+1;
	p = tmp = (unsigned char *)fs_get(len * sizeof(unsigned char));
    }
    else{
	len = SIZEOF_20KBUF-10000;
	p = (unsigned char *)(tmp_20k_buf+10000);
    }

    /* ASSUME: Any text *we* insert MUST be in UTF-8 */
    t = (char *)rfc1522_decode(p, len, s, NULL);

    gf_set_readc(&gc, t, strlen(t), CharStar);

    if(tmp)
      fs_give((void **)&tmp);

    margin = format_view_margin();
    if(flags & FM_NOINDENT)
      margin[0] = margin[1] = 0;

    /* safety net */
    if(width - (margin[0] + margin[1]) < 5){
	margin[0] = margin[1] = 0;
	if(width < 5)
	  width = 80;
    }

    width -= (margin[0] + margin[1]);

    if(width > 40){
	width -= 12;

	es.prelen = MAX(2, MIN(margin[0] + 6, sizeof(es.prefix) - 3));
	snprintf(es.prefix, sizeof(es.prefix), "%s[ ", repeat_char(es.prelen - 2, ' '));
	es.postlen = 2;
	strncpy(es.postfix, " ]", sizeof(es.postfix));
	es.postfix[sizeof(es.postfix)-1] = '\0';
    }
    else if(width > 20){
	width -= 6;

	es.prelen = MAX(2, MIN(margin[0] + 3, sizeof(es.prefix) - 3));
	snprintf(es.prefix, sizeof(es.prefix), "%s[ ", repeat_char(es.prelen - 2, ' '));
	es.postlen = 2;
	strncpy(es.postfix, " ]", sizeof(es.postfix));
	es.postfix[sizeof(es.postfix)-1] = '\0';
    }
    else{
	width -= 2;
	strncpy(es.prefix, "[", sizeof(es.prefix));
	es.prefix[sizeof(es.prefix)-1] = '\0';
	strncpy(es.postfix, "]", sizeof(es.postfix));
	es.postfix[sizeof(es.postfix)-1] = '\0';
	es.prelen = 1;
	es.postlen = 1;
    }

    gf_filter_init();

    /* catch urls */
    if((F_ON(F_VIEW_SEL_URL, ps_global)
	|| F_ON(F_VIEW_SEL_URL_HOST, ps_global)
	|| F_ON(F_SCAN_ADDR, ps_global))
       && handlesp){
	gf_link_filter(gf_line_test, gf_line_test_opt(url_hilite, handlesp));
    }
      
    gf_link_filter(gf_wrap, gf_wrap_filter_opt(width, width, NULL, 0,
					       (handlesp ? GFW_HANDLES : 0) | GFW_UTF8));
    gf_link_filter(gf_line_test, gf_line_test_opt(quote_editorial, &es));

    /* If not for display, change to local end of line */
    if(!(flags & FM_DISPLAY))
	gf_link_filter(gf_nvtnl_local, NULL);

    return(gf_pipe(gc, pc));
}


int
quote_editorial(long int linenum, char *line, LT_INS_S **ins, void *local)
{
    ins = gf_line_test_new_ins(ins, line,
			       ((EDITORIAL_S *)local)->prefix,
			       ((EDITORIAL_S *)local)->prelen);
    ins = gf_line_test_new_ins(ins, line + strlen(line),
			       ((EDITORIAL_S *)local)->postfix,
			       ((EDITORIAL_S *)local)->postlen);
    return(0);
}
