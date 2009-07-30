#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: charset.c 1032 2008-04-11 00:30:04Z hubert@u.washington.edu $";
#endif

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

#include "../pith/headers.h"
#include "../pith/charset.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/escapes.h"
#include "../pith/mimedesc.h"
#include "../pith/filter.h"
#include "../pith/string.h"
#include "../pith/options.h"


/*
 * Internal prototypes
 */
int	       rfc1522_token(char *, int (*)(int), char *, char **);
int	       rfc1522_valtok(int);
int	       rfc1522_valenc(int);
int	       rfc1522_valid(char *, char **, char **, char **, char **);
void	       rfc1522_copy_and_transliterate(unsigned char *, unsigned char **, size_t,
					      unsigned char *, unsigned long, char *);
unsigned char *rfc1522_encoded_word(unsigned char *, int, char *);
char	      *rfc1522_8bit(void *, int);
char	      *rfc1522_binary(void *, int);


char *
body_charset(MAILSTREAM *stream, long int msgno, unsigned char *section)
{
    BODY *body;
    char *charset;


    if((body = mail_body(stream, msgno, section)) && body->type == TYPETEXT){
	if(!(charset = parameter_val(body->parameter, "charset")))
	  charset = cpystr("US-ASCII");

	return(charset);
    }

    return(NULL);
}


/*
 * Copies the source string into allocated space with the 8-bit EUC codes
 * (on Unix) or the Shift-JIS (on PC) converted into ISO-2022-JP.
 * Caller is responsible for freeing the result.
 */
unsigned char *
trans_euc_to_2022_jp(unsigned char *src)
{
    size_t len, alloc;
    unsigned char *rv, *p, *q;
    int    inside_esc_seq = 0;
    int    c1 = -1;		/* remembers first of pair for Shift-JIS */

    if(!src)
      return(NULL);

    len = strlen((char *) src);

    /*
     * Worst possible increase is every other character an 8-bit character.
     * In that case, each of those gets 6 extra charactes for the escape
     * sequences. We're not too concerned about the extra length because
     * these are relatively short strings.
     */
    alloc = len + 1 + ((len+1)/2) * 6;
    rv = (unsigned char *) fs_get(alloc * sizeof(char));

    for(p = src, q = rv; *p; p++){
	if(inside_esc_seq){
	    if(c1 >= 0){			/* second of a pair? */
		int adjust = *p < 159;
		int rowOffset = c1 < 160 ? 112 : 176;
		int cellOffset = adjust ? (*p > 127 ? 32 : 31) : 126;

		*q++ = ((c1 - rowOffset) << 1) - adjust;
		*q++ = *p - cellOffset;
		c1 = -1;
	    }
	    else if(*p & 0x80){
		*q++ = (*p & 0x7f);
	    }
	    else{
		*q++ = '\033';
		*q++ = '(';
		*q++ = 'B';
		*q++ = (*p);
		c1 = -1;
		inside_esc_seq = 0;
	    }
	}
	else{
	    if(*p & 0x80){
		*q++ = '\033';
		*q++ = '$';
		*q++ = 'B';
		*q++ = (*p & 0x7f);
		inside_esc_seq = 1;
	    }
	    else{
		*q++ = (*p);
	    }
	}
    }

    if(inside_esc_seq){
	*q++ = '\033';
	*q++ = '(';
	*q++ = 'B';
    }

    *q = '\0';

    return(rv);
}


/*
 *  * * * * * * * *      RFC 1522 support routines      * * * * * * * *
 *
 *   RFC 1522 support is *very* loosely based on code contributed
 *   by Lars-Erik Johansson <lej@cdg.chalmers.se>.  Thanks to Lars-Erik,
 *   and appologies for taking such liberties with his code.
 */

#define	RFC1522_INIT	"=?"
#define	RFC1522_INIT_L	2
#define RFC1522_TERM	"?="
#define	RFC1522_TERM_L	2
#define	RFC1522_DLIM	"?"
#define	RFC1522_DLIM_L	1
#define	RFC1522_MAXW	256	/* RFC's say 75, but no senders seem to care*/
#define	ESPECIALS	"()<>@,;:\"/[]?.="
#define	RFC1522_OVERHEAD(S)	(RFC1522_INIT_L + RFC1522_TERM_L +	\
				 (2 * RFC1522_DLIM_L) + strlen(S) + 1);
#define	RFC1522_ENC_CHAR(C)	(((C) & 0x80) || !rfc1522_valtok(C)	\
				 || (C) == '_' )

/*
 * rfc1522_decode_to_utf8 - try to decode the given source string ala RFC 2047
 *                  (obsoleted RFC 1522) into the given destination buffer,
 *		    encoded in UTF-8.
 *
 * How large should d be? The decoded string of octets will fit in
 * the same size string as the source string. However, because we're
 * translating that into UTF-8 the result may expand. Currently the
 * Thai character set has single octet characters which expand to
 * three octets in UTF-8. So it would be safe to use 3 * strlen(s)
 * for the size of d. One can imagine a currently non-existent
 * character set that expanded to 4 octets instead, so use 4 to be
 * super safe.
 *
 * Returns: pointer to either the destination buffer containing the
 *	    decoded text, or a pointer to the source buffer if there was
 *          no valid 'encoded-word' found during scanning.
 */
unsigned char *
rfc1522_decode_to_utf8(unsigned char *d, size_t len, char *s)
{
    unsigned char *rv = NULL, *p;
    char	  *start = s, *sw, *enc, *txt, *ew, **q, *lang;
    char          *cset;
    unsigned long  l;
    int		   i;

    *d = '\0';					/* init destination */

    while(s && (sw = strstr(s, RFC1522_INIT))){
	/* validate the rest of the encoded-word */
	if(rfc1522_valid(sw, &cset, &enc, &txt, &ew)){
	    if(!rv)
	      rv = d;				/* remember start of dest */

	    /*
	     * We may have been putting off copying the first part of the
	     * source while waiting to see if we have to copy at all.
	     */
	    if(rv == d && s != start){
		rfc1522_copy_and_transliterate(rv, &d, len, (unsigned char *) start,
					       sw - start, NULL);
		s = sw;
	    }

	    /* copy everything between s and sw to destination */
	    for(i = 0; &s[i] < sw; i++)
	      if(!isspace((unsigned char)s[i])){ /* if some non-whitespace */
		  while(s < sw && d-rv<len-1)
		    *d++ = (unsigned char) *s++;

		  break;
	      }

	    enc[-1] = txt[-1] = ew[0] = '\0';	/* tie off token strings */

	    if((lang = strchr(cset, '*')) != NULL)
	      *lang++ = '\0';

	    /* based on encoding, write the encoded text to output buffer */
	    switch(*enc){
	      case 'Q' :			/* 'Q' encoding */
	      case 'q' :
		/* special hocus-pocus to deal with '_' exception, too bad */
		for(l = 0L, i = 0; txt[l]; l++)
		  if(txt[l] == '_')
		    i++;

		if(i){
		    q = (char **) fs_get((i + 1) * sizeof(char *));
		    for(l = 0L, i = 0; txt[l]; l++)
		      if(txt[l] == '_'){
			  q[i++] = &txt[l];
			  txt[l] = SPACE;
		      }

		    q[i] = NULL;
		}
		else
		  q = NULL;

		if((p = rfc822_qprint((unsigned char *)txt, strlen(txt), &l)) != NULL){
		    rfc1522_copy_and_transliterate(rv, &d, len, p, l, cset);
		    fs_give((void **)&p);	/* free encoded buf */
		}
		else{
		    if(q)
		      fs_give((void **) &q);

		    goto bogus;
		}

		if(q){				/* restore underscores */
		    for(i = 0; q[i]; i++)
		      *(q[i]) = '_';

		    fs_give((void **)&q);
		}

		break;

	      case 'B' :			/* 'B' encoding */
	      case 'b' :
		if((p = rfc822_base64((unsigned char *) txt, strlen(txt), &l)) != NULL){
		    rfc1522_copy_and_transliterate(rv, &d, len, p, l, cset);
		    fs_give((void **)&p);	/* free encoded buf */
		}
		else
		  goto bogus;

		break;

	      default:
		rfc1522_copy_and_transliterate(rv, &d, len, (unsigned char *) txt,
					       strlen(txt), NULL);
		dprint((1, "RFC1522_decode: Unknown ENCODING: %s\n",
		       enc ? enc : "?"));
		break;
	    }

	    /* restore trompled source string */
	    enc[-1] = txt[-1] = '?';
	    ew[0]   = RFC1522_TERM[0];

	    /* advance s to start of text after encoded-word */
	    s = ew + RFC1522_TERM_L;

	    if(lang)
	      lang[-1] = '*';
	}
	else{

	    /*
	     * Found intro, but bogus data followed, treat it as normal text.
	     */

	    /* if already copying to destn, copy it */
	    l = (sw - s) + RFC1522_INIT_L;
	    if(rv){
		rfc1522_copy_and_transliterate(rv, &d, len, (unsigned char *) s, l, NULL);
		*d = '\0';
		s += l;				/* advance s beyond intro */
	    }
	    else	/* probably won't have to copy it at all, wait */
	      s += l;
	}
    }

    if(rv){
	if(s && *s){				/* copy remaining text */
	    rfc1522_copy_and_transliterate(rv, &d, len, (unsigned char *) s, strlen(s), NULL);
	    rv[len-1] = '\0';
	}
    }
    else if(s){
	rv = d;
	rfc1522_copy_and_transliterate(rv, &d, len, (unsigned char *) s, strlen(s), NULL);
	rv[len-1] = '\0';
    }

    return(rv ? rv : (unsigned char *) start);

  bogus:
    dprint((1, "RFC1522_decode: BOGUS INPUT: -->%s<--\n",
	   start ? start : "?"));
    return((unsigned char *) start);
}


/*
 * rfc1522_token - scan the given source line up to the end_str making
 *		   sure all subsequent chars are "valid" leaving endp
 *		   a the start of the end_str.
 * Returns: TRUE if we got a valid token, FALSE otherwise
 */
int
rfc1522_token(char *s, int (*valid) (int), char *end_str, char **endp)
{
    while(*s){
	if((char) *s == *end_str		/* test for matching end_str */
	   && ((end_str[1])
	        ? !strncmp((char *)s + 1, end_str + 1, strlen(end_str + 1))
	        : 1)){
	    *endp = s;
	    return(TRUE);
	}

	if(!(*valid)(*s++))			/* test for valid char */
	  break;
    }

    return(FALSE);
}


/*
 * rfc1522_valtok - test for valid character in the RFC 1522 encoded
 *		    word's charset and encoding fields.
 */
int
rfc1522_valtok(int c)
{
    return(!(c == SPACE || iscntrl(c & 0x7f) || strindex(ESPECIALS, c)));
}


/*
 * rfc1522_valenc - test for valid character in the RFC 1522 encoded
 *		    word's encoded-text field.
 */
int
rfc1522_valenc(int c)
{
    return(!(c == '?' || c == SPACE) && isprint((unsigned char)c));
}


/*
 * rfc1522_valid - validate the given string as to it's rfc1522-ness
 */
int
rfc1522_valid(char *s, char **charset, char **enc, char **txt, char **endp)
{
    char *c, *e, *t, *p;
    int   rv;

    rv = rfc1522_token(c = s+RFC1522_INIT_L, rfc1522_valtok, RFC1522_DLIM, &e)
	   && rfc1522_token(++e, rfc1522_valtok, RFC1522_DLIM, &t)
	   && rfc1522_token(++t, rfc1522_valenc, RFC1522_TERM, &p)
	   && p - s <= RFC1522_MAXW;

    if(charset)
      *charset = c;

    if(enc)
      *enc = e;

    if(txt)
      *txt = t;

    if(endp)
      *endp = p;

    return(rv);
}


/*
 * rfc1522_copy_and_transliterate - copy given buf to destination buffer
 *				    as UTF-8 characters
 */
void
rfc1522_copy_and_transliterate(unsigned char  *rv,
			       unsigned char **d,
			       size_t	       len,
			       unsigned char  *s,
			       unsigned long   l,
			       char	      *cset)
{
    unsigned long i;
    SIZEDTEXT	  src, xsrc;

    src.data = s;
    src.size = l;
    memset(&xsrc, 0, sizeof(SIZEDTEXT));

    /* transliterate decoded segment to utf-8 */
    if(cset){
	if(strucmp((char *) cset, "us-ascii")
	   && strucmp((char *) cset, "utf-8")){
	    if(utf8_charset(cset)){
		if(!utf8_text(&src, cset, &xsrc, 0L)){
		    /* should not happen */
		    panic("c-client failed to transliterate recognized characterset");
		}
	    }
	    else{
		/* non-xlatable charset */
		for(i = 0; i < l; i++)
		  if(src.data[i] & 0x80){
		      xsrc.data = (unsigned char *) fs_get((l+1) * sizeof(unsigned char));
		      xsrc.size = l;
		      for(i = 0; i < l; i++)
			xsrc.data[i] = (src.data[i] & 0x80) ? '?' : src.data[i];

		      break;
		  }
	    }
	}
    }
    else{
	const CHARSET *cs;

	src.data = s;
	src.size = strlen((char *) s);

	if((cs = utf8_infercharset(&src))){
	    if(!(cs->type == CT_ASCII || cs->type == CT_UTF8)){
		if(!utf8_text_cs(&src, cs, &xsrc, 0L, 0L)){
		    /* should not happen */
		    panic("c-client failed to transliterate recognized characterset");
		}
	    }
	}
	else if((cset=ps_global->VAR_UNK_CHAR_SET)
		&& strucmp((char *) cset, "us-ascii")
	        && strucmp((char *) cset, "utf-8")
		&& utf8_charset(cset)){
		if(!utf8_text(&src, cset, &xsrc, 0L)){
		    /* should not happen */
		    panic("c-client failed to transliterate recognized character set");
		}
	}
	else{
	    /* unknown bytes - mask off high bit chars */
	    for(i = 0; i < l; i++)
	      if(src.data[i] & 0x80){
		  xsrc.data = (unsigned char *) fs_get((l+1) * sizeof(unsigned char));
		  xsrc.size = l;
		  for(i = 0; i < l; i++)
		    xsrc.data[i] = (src.data[i] & 0x80) ? '?' : src.data[i];

		  break;
	      }
	}
    }

    if(xsrc.data){
	s = xsrc.data;
	l = xsrc.size;
    }

    i = MIN(l,len-1-((*d)-rv));
    strncpy((char *) (*d), (char *) s, i);
    (*d)[i] = '\0';
    *d += l;			/* advance dest ptr to EOL */
    if((*d)-rv > len-1)
      *d = rv+len-1;

    if(xsrc.data && src.data != xsrc.data)
      fs_give((void **) &xsrc.data);
}



/*
 * rfc1522_encode - encode the given source string ala RFC 1522,
 *		    IF NECESSARY, into the given destination buffer.
 *		    Don't bother copying if it turns out encoding
 *		    isn't necessary.
 *
 * Returns: pointer to either the destination buffer containing the
 *	    encoded text, or a pointer to the source buffer if we didn't
 *          have to encode anything.
 */
char *
rfc1522_encode(char *d, size_t dlen, unsigned char *s, char *charset)
{
    unsigned char *p, *q;
    int		   n;

    if(!s)
      return((char *) s);

    if(!charset)
      charset = UNKNOWN_CHARSET;

    /* look for a reason to encode */
    for(p = s, n = 0; *p; p++)
      if((*p) & 0x80){
	  n++;
      }
      else if(*p == RFC1522_INIT[0]
	      && !strncmp((char *) p, RFC1522_INIT, RFC1522_INIT_L)){
	  if(rfc1522_valid((char *) p, NULL, NULL, NULL, (char **) &q))
	    p = q + RFC1522_TERM_L - 1;		/* advance past encoded gunk */
      }
      else if(*p == ESCAPE && match_escapes((char *)(p+1))){
	  n++;
      }

    if(n){					/* found, encoding to do */
	char *rv  = d, *t,
	      enc = (n > (2 * (p - s)) / 3) ? 'B' : 'Q';

	while(*s){
	    if(d-rv < dlen-1-(RFC1522_INIT_L+2*RFC1522_DLIM_L+1)){
		sstrncpy(&d, RFC1522_INIT, dlen-(d-rv));	/* insert intro header, */
		sstrncpy(&d, charset, dlen-(d-rv));		/* character set tag, */
		sstrncpy(&d, RFC1522_DLIM, dlen-(d-rv));	/* and encoding flavor */
		if(dlen-(d-rv) > 0) 
		  *d++ = enc;

		sstrncpy(&d, RFC1522_DLIM, dlen-(d-rv));
	    }

	    /*
	     * feed lines to encoder such that they're guaranteed
	     * less than RFC1522_MAXW.
	     */
	    p = rfc1522_encoded_word(s, enc, charset);
	    if(enc == 'B')			/* insert encoded data */
	      sstrncpy(&d, t = rfc1522_binary(s, p - s), dlen-1-(d-rv));
	    else				/* 'Q' encoding */
	      sstrncpy(&d, t = rfc1522_8bit(s, p - s), dlen-1-(d-rv));

	    sstrncpy(&d, RFC1522_TERM, dlen-1-(d-rv));	/* insert terminator */
	    fs_give((void **) &t);
	    if(*p)				/* more src string follows */
	      sstrncpy(&d, "\015\012 ", dlen-1-(d-rv));	/* insert cont. line */

	    s = p;				/* advance s */
	}

	rv[dlen-1] = '\0';
	return(rv);
    }
    else
      return((char *) s);			/* no work for us here */
}



/*
 * rfc1522_encoded_word -- cut given string into max length encoded word
 *
 * Return: pointer into 's' such that the encoded 's' is no greater
 *	   than RFC1522_MAXW
 *
 *  NOTE: this line break code is NOT cognizant of any SI/SO
 *  charset requirements nor similar strategies using escape
 *  codes.  Hopefully this will matter little and such
 *  representation strategies don't also include 8bit chars.
 */
unsigned char *
rfc1522_encoded_word(unsigned char *s, int enc, char *charset)
{
    int goal = RFC1522_MAXW - RFC1522_OVERHEAD(charset);

    if(enc == 'B')			/* base64 encode */
      for(goal = ((goal / 4) * 3) - 2; goal && *s; goal--, s++)
	;
    else				/* special 'Q' encoding */
      for(; goal && *s; s++)
	if((goal -= RFC1522_ENC_CHAR(*s) ? 3 : 1) < 0)
	  break;

    return(s);
}



/*
 * rfc1522_8bit -- apply RFC 1522 'Q' encoding to the given 8bit buffer
 *
 * Return: alloc'd buffer containing encoded string
 */
char *
rfc1522_8bit(void *src, int slen)
{
    char *ret = (char *) fs_get ((size_t) (3*slen + 2));
    char *d = ret;
    unsigned char c;
    unsigned char *s = (unsigned char *) src;

    while (slen--) {				/* for each character */
	if (((c = *s++) == '\015') && (*s == '\012') && slen) {
	    *d++ = '\015';			/* true line break */
	    *d++ = *s++;
	    slen--;
	}
	else if(c == SPACE){			/* special encoding case */
	    *d++ = '_';
	}
	else if(RFC1522_ENC_CHAR(c)){
	    *d++ = '=';				/* quote character */
	    C2XPAIR(c, d);
	}
	else
	  *d++ = (char) c;			/* ordinary character */
    }

    *d = '\0';					/* tie off destination */
    return(ret);
}


/*
 * rfc1522_binary -- apply RFC 1522 'B' encoding to the given 8bit buffer
 *
 * Return: alloc'd buffer containing encoded string
 */
char *
rfc1522_binary (void *src, int srcl)
{
    static char *v =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    unsigned char *s = (unsigned char *) src;
    char *ret, *d;

    d = ret = (char *) fs_get ((size_t) ((((srcl + 2) / 3) * 4) + 1));
    for (; srcl; s += 3) {	/* process tuplets */
				/* byte 1: high 6 bits (1) */
	*d++ = v[s[0] >> 2];
				/* byte 2: low 2 bits (1), high 4 bits (2) */
	*d++ = v[((s[0] << 4) + (--srcl ? (s[1] >> 4) : 0)) & 0x3f];
				/* byte 3: low 4 bits (2), high 2 bits (3) */
	*d++ = srcl ? v[((s[1] << 2) + (--srcl ? (s[2] >> 6) :0)) & 0x3f] :'=';
				/* byte 4: low 6 bits (3) */
	*d++ = srcl ? v[s[2] & 0x3f] : '=';
	if(srcl)
	  srcl--;		/* count third character if processed */
    }

    *d = '\0';			/* tie off string */
    return(ret);		/* return the resulting string */
}


/*
 * Checks if charset conversion is possible and which quality could be achieved
 *
 * args: from_cs -- charset to convert from
 *       to_cs   -- charset to convert to
 *
 * Results:
 * CONV_TABLE->table   -- conversion table, NULL if conversion not needed
 *                        or not supported
 * CONV_TABLE->quality -- conversion quality (conversion not supported, not
 *                        needed, loses special chars, or loses letters
 *
 * The other entries of CONV_TABLE are used inside this function only
 * and may not be used outside unless this documentation is updated.
 */
CONV_TABLE *
conversion_table(char *from_cs, char *to_cs)
{
    int               i, j;
    unsigned char    *p = NULL;
    unsigned short   *fromtab, *totab;
    CONV_TABLE       *ct = NULL;
    const CHARSET    *from, *to;
    static CONV_TABLE null_tab;

    if(!(from_cs && *from_cs && to_cs && *to_cs) || !strucmp(from_cs, to_cs)){
	memset(&null_tab, 0, sizeof(null_tab));
	null_tab.quality = CV_NO_TRANSLATE_NEEDED;
	return(&null_tab);
    }

    /*
     * First check to see if we are already set up for this pair of charsets.
     */
    if((ct = ps_global->conv_table) != NULL
       && ct->from_charset && ct->to_charset
       && !strucmp(ct->from_charset, from_cs)
       && !strucmp(ct->to_charset, to_cs))
      return(ct);

    /*
     * No such luck. Get rid of the cache of the previous translation table
     * and build a new one.
     */
    if(ct){
	if(ct->table && (ct->convert != gf_convert_utf8_charset))
	  fs_give((void **) &ct->table);
	
	if(ct->from_charset)
	  fs_give((void **) &ct->from_charset);
	
	if(ct->to_charset)
	  fs_give((void **) &ct->to_charset);
    }
    else
      ct = ps_global->conv_table = (CONV_TABLE *) fs_get(sizeof(*ct));
    
    memset(ct, 0, sizeof(*ct));

    ct->from_charset = cpystr(from_cs);
    ct->to_charset   = cpystr(to_cs);
    ct->quality = CV_NO_TRANSLATE_POSSIBLE;

    /*
     * Check to see if a translation is feasible.
     */
    from = utf8_charset(from_cs);
    to =   utf8_charset(to_cs);

    if(from && to){		/* if both charsets found */
				/* no mapping if same or from is ASCII */
	if((from->type == to->type && from->tab == to->tab)
	   || (from->type == CT_ASCII))
	    ct->quality = CV_NO_TRANSLATE_NEEDED;
	else switch(from->type){
	case CT_1BYTE0:		/* 1 byte no table */
	case CT_1BYTE:		/* 1 byte ASCII + table 0x80-0xff */
	case CT_1BYTE8:		/* 1 byte table 0x00 - 0xff */
	    switch(to->type){
	    case CT_1BYTE0:	/* 1 byte no table */
	    case CT_1BYTE:	/* 1 byte ASCII + table 0x80-0xff */
	    case CT_1BYTE8:	/* 1 byte table 0x00 - 0xff */
	        ct->quality = (from->script & to->script) ?
		  CV_LOSES_SOME_LETTERS : CV_LOSES_SPECIAL_CHARS;
		break;
	    }
	    break;
	case CT_UTF8:		/* variable UTF-8 encoded Unicode no table */
	/* If source is UTF-8, see if destination charset has an 8 or 16 bit
	 * coded character set that we can translate to.  By special
	 * dispensation, kludge ISO-2022-JP to EUC or Shift-JIS, but don't
	 * try to do any other ISO 2022 charsets or UTF-7.
	 */
	    switch (to->type){
	    case CT_SJIS:	/* 2 byte Shift-JIS */
				/* only win if can get EUC-JP chartab */
		if(utf8_charset("EUC-JP"))
		    ct->quality = CV_LOSES_SOME_LETTERS;
		break;
	    case CT_ASCII:	/* 7-bit ASCII no table */
	    case CT_1BYTE0:	/* 1 byte no table */
	    case CT_1BYTE:	/* 1 byte ASCII + table 0x80-0xff */
	    case CT_1BYTE8:	/* 1 byte table 0x00 - 0xff */
	    case CT_EUC:	/* 2 byte ASCII + utf8_eucparam base/CS2/CS3 */
	    case CT_DBYTE:	/* 2 byte ASCII + utf8_eucparam */
	    case CT_DBYTE2:	/* 2 byte ASCII + utf8_eucparam plane1/2 */
		ct->quality = CV_LOSES_SOME_LETTERS;
		break;
	    }
	    break;
	}

	switch (ct->quality) {	/* need to map? */
	case CV_NO_TRANSLATE_POSSIBLE:
	case CV_NO_TRANSLATE_NEEDED:
	  break;		/* no mapping needed */
	default:		/* do mapping */
	    switch (from->type) {
	    case CT_UTF8:	/* UTF-8 to legacy character set */
	      if((ct->table = utf8_rmap (to_cs)) != NULL)
		ct->convert = gf_convert_utf8_charset;
	      break;

	    case CT_1BYTE0:	/* ISO 8859-1 */
	    case CT_1BYTE:	/* low part ASCII, high part other */
	    case CT_1BYTE8:	/* low part has some non-ASCII */
	    /*
	     * The fromtab and totab tables are mappings from the 128 character
	     * positions 128-255 to their Unicode values (so unsigned shorts).
	     * The table we are creating is such that if
	     *
	     *    from_char_value -> unicode_value
	     *    to_char_value   -> same_unicode_value
	     *
	     *  then we want to map from_char_value -> to_char_value
	     *
	     * To simplify conversions we create the whole 256 element array,
	     * with the first 128 positions just the identity. If there is no
	     * conversion for a particular from_char_value (that is, no
	     * to_char_value maps to the same unicode character) then we put
	     *  '?' in that character. We may want to output blob on the PC,
	     * but don't so far.
	     *
	     * If fromtab or totab are NULL, that means the mapping is simply
	     * the identity mapping. Since that is still useful to us, we
	     * create it on the fly.
	     */
		fromtab = (unsigned short *) from->tab;
		totab   = (unsigned short *) to->tab;

		ct->convert = gf_convert_8bit_charset;
		p = ct->table = (unsigned char *)
		  fs_get(256 * sizeof(unsigned char));
		for(i = 0; i < 256; i++){
		    unsigned int fc;
		    p[i] = '?';
		    switch(from->type){	/* get "from" UCS-2 codepoint */
		    case CT_1BYTE0:	/* ISO 8859-1 */
			fc = i;
			break;
		    case CT_1BYTE:	/* low part ASCII, high part other */
			fc = (i < 128) ? i : fromtab[i-128];
			break;
		    case CT_1BYTE8:	/* low part has some non-ASCII */
			fc = fromtab[i];
			break;
		    }
		    switch(to->type){ /* match against "to" UCS-2 codepoint */
		    case CT_1BYTE0: /* identity match for ISO 8859-1*/
			if(fc < 256)
			  p[i] = fc;
			break;
		    case CT_1BYTE: /* ASCII is identity, search high part */
			if(fc < 128) p[i] = fc;
			else for(j = 0; j < 128; j++){
			    if(fc == totab[j]){
				p[i] = 128 + j;
				break;
			    }
			}
			break;
		    case CT_1BYTE8: /* search all codepoints */
			for(j = 0; j < 256; j++){
			    if(fc == totab[j]){
			      p[i] = j;
			      break;
			    }
			}
			break;
		    }
		}
		break;
	    }
	}
    }

    return(ct);
}


/*
 * Replace personal names in list of addresses with
 * decoded personal names in UTF-8.
 * Assumes we can free and reallocate the name.
 */
void
decode_addr_names_to_utf8(struct mail_address *a)
{
    for(; a; a = a->next)
      if(a->personal)
	convert_possibly_encoded_str_to_utf8(&a->personal);
}


/*
 * Strp is a pointer to an allocated string.
 * This routine will convert the string to UTF-8, possibly
 * freeing and re-allocating it.
 * The source string may or may not have RFC1522 encoding
 * which will be undone using rfc1522_decode.
 * The string will have been converted on return.
 */
void
convert_possibly_encoded_str_to_utf8(char **strp)
{
    size_t     len, lensrc, lenresult;
    char      *bufp, *decoded;

    if(!strp || !*strp || **strp == '\0')
      return;

    len = 4 * strlen(*strp) + 1;
    bufp = (char *) fs_get(len);

    decoded = (char *) rfc1522_decode_to_utf8((unsigned char *) bufp, len, *strp);
    if(decoded != (*strp)){	/* unchanged */
	if((lensrc=strlen(*strp)) >= (lenresult=strlen(decoded))){
	    strncpy(*strp, decoded, lensrc);
	    (*strp)[lensrc] = '\0';
	}
	else{
	    fs_give((void **) strp);
	    if(decoded == bufp){	/* this will be true */
		fs_resize((void **) &bufp, lenresult+1);
		*strp = bufp;
		bufp = NULL;
	    }
	    else{			/* this is unreachable */
		*strp = cpystr(decoded);
	    }
	}
    }
    /* else, already UTF-8 */

    if(bufp)
      fs_give((void **) &bufp);
}
