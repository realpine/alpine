/*
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

/*
 *  rfc1522.c
 *
 *  right now this is just rfc1522_encode (taken straight out of pine/strings.c, 
 *  but if were to become necessary,
 *  it could be made to do rfc1522_decode too, and it already has some strings functions.
 */
#include "pmapi.h"

#define	RFC1522_INIT	"=?"
#define	RFC1522_INIT_L	2
#define RFC1522_TERM	"?="
#define	RFC1522_TERM_L	2
#define	RFC1522_DLIM	"?"
#define	RFC1522_DLIM_L	1
#define	RFC1522_MAXW	75
#define	ESPECIALS	"()<>@,;:\"/[]?.="
#define	RFC1522_OVERHEAD(S)	(RFC1522_INIT_L + RFC1522_TERM_L +	\
				 (2 * RFC1522_DLIM_L) + strlen(S) + 1);
#define	RFC1522_ENC_CHAR(C)	(((C) & 0x80) || !rfc1522_valtok(C)	\
				 || (C) == '_' )
#define SPACE		' '		/* space character      */
#define ESCAPE		'\033'		/* the escape		*/
#define UNKNOWN_CHARSET		"X-UNKNOWN"

/*
 * Hex conversion aids
 */
#define HEX_ARRAY	"0123456789ABCDEF"
#define	HEX_CHAR1(C)	HEX_ARRAY[((C) & 0xf0) >> 4]
#define	HEX_CHAR2(C)	HEX_ARRAY[(C) & 0xf]

#define	C2XPAIR(C, S)	{ \
			    *(S)++ = HEX_CHAR1(C); \
			    *(S)++ = HEX_CHAR2(C); \
			}


int	       rfc1522_token PROTO((char *, int (*) PROTO((int)), char *,
				    char **));
int	       rfc1522_valtok PROTO((int));
int	       rfc1522_valenc PROTO((int));
int	       rfc1522_valid PROTO((char *, char **, char **, char **,
				    char **));
char	      *rfc1522_8bit PROTO((void *, int));
char	      *rfc1522_binary PROTO((void *, int));
unsigned char *rfc1522_encoded_word PROTO((unsigned char *, int, char *));
char	      *strindex PROTO((char *, int));
void	    sstrcpy PROTO((char **, char *));
void	    sstrncpy PROTO((char **, char *, int));

int 	    removing_double_quotes PROTO((char *));

static char *known_escapes[] = {
    "(B",  "(J",  "$@",  "$B",			/* RFC 1468 */
    "(H",
    NULL};
/* different for non-Windows */

int
match_escapes(esc_seq)
    char *esc_seq;
{
    char **p;
    int    n;

    for(p = known_escapes; *p && strncmp(esc_seq, *p, n = strlen(*p)); p++)
      ;

    return(*p ? n + 1 : 0);
}

/*----------------------------------------------------------------------
    A replacement for strchr or index ...

    Returns a pointer to the first occurrence of the character
    'ch' in the specified string or NULL if it doesn't occur

 ....so we don't have to worry if it's there or not. We bring our own.
If we really care about efficiency and think the local one is more
efficient the local one can be used, but most of the things that take
a long time are in the c-client and not in pine.
 ----*/
char *
strindex(buffer, ch)
    char *buffer;
    int ch;
{
    do
      if(*buffer == ch)
	return(buffer);
    while (*buffer++ != '\0');

    return(NULL);
}

/*----------------------------------------------------------------------
  copy the source string onto the destination string returning with
  the destination string pointer at the end of the destination text

  motivation for this is to avoid twice passing over a string that's
  being appended to twice (i.e., strcpy(t, x); t += strlen(t))
 ----*/
void
sstrcpy(d, s)
    char **d;
    char *s;
{
    while((**d = *s++) != '\0')
      (*d)++;
}

void
sstrncpy(d, s, n)
    char **d;
    char *s;
    int n;
{
    while(n-- > 0 && (**d = *s++) != '\0')
      (*d)++;
}

/*
 * rfc1522_token - scan the given source line up to the end_str making
 *		   sure all subsequent chars are "valid" leaving endp
 *		   a the start of the end_str.
 * Returns: TRUE if we got a valid token, FALSE otherwise
 */
int
rfc1522_token(s, valid, end_str, endp)
    char  *s;
    int	 (*valid) PROTO((int));
    char  *end_str;
    char **endp;
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
rfc1522_valtok(c)
    int c;
{
    return(!(c == SPACE || iscntrl(c & 0x7f) || strindex(ESPECIALS, c)));
}


/*
 * rfc1522_valenc - test for valid character in the RFC 1522 encoded
 *		    word's encoded-text field.
 */
int
rfc1522_valenc(c)
    int c;
{
    return(!(c == '?' || c == SPACE) && isprint((unsigned char)c));
}


/*
 * rfc1522_valid - validate the given string as to it's rfc1522-ness
 */
int
rfc1522_valid(s, charset, enc, txt, endp)
    char  *s;
    char **charset;
    char **enc;
    char **txt;
    char **endp;
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
rfc1522_encode(d, len, s, charset)
    char	  *d;
    size_t         len;		/* length of d */
    unsigned char *s;
    char	  *charset;
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
	    if(d-rv < len-1-(RFC1522_INIT_L+2*RFC1522_DLIM_L+1)){
		sstrcpy(&d, RFC1522_INIT);	/* insert intro header, */
		sstrcpy(&d, charset);		/* character set tag, */
		sstrcpy(&d, RFC1522_DLIM);	/* and encoding flavor */
		*d++ = enc;
		sstrcpy(&d, RFC1522_DLIM);
	    }

	    /*
	     * feed lines to encoder such that they're guaranteed
	     * less than RFC1522_MAXW.
	     */
	    p = rfc1522_encoded_word(s, enc, charset);
	    if(enc == 'B')			/* insert encoded data */
	      sstrncpy(&d, t = rfc1522_binary(s, p - s), len-1-(d-rv));
	    else				/* 'Q' encoding */
	      sstrncpy(&d, t = rfc1522_8bit(s, p - s), len-1-(d-rv));

	    sstrncpy(&d, RFC1522_TERM, len-1-(d-rv));	/* insert terminator */
	    fs_give((void **) &t);
	    if(*p)				/* more src string follows */
	      sstrncpy(&d, "\015\012 ", len-1-(d-rv));	/* insert cont. line */

	    s = p;				/* advance s */
	}

	rv[len-1] = '\0';
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
rfc1522_encoded_word(s, enc, charset)
    unsigned char *s;
    int		   enc;
    char	  *charset;
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
rfc1522_8bit(src, slen)
    void *src;
    int   slen;
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
rfc1522_binary (src, srcl)
    void *src;
    int   srcl;
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
 *  Function to parse the given string into two space-delimited fields
 *  Quotes may be used to surround labels or values with spaces in them.
 *  Backslash negates the special meaning of a quote.
 *  Unescaping of backslashes only happens if the pair member is quoted,
 *    this provides for backwards compatibility.
 *
 * Args -- string -- the source string
 *          label -- the first half of the string, a return value
 *          value -- the last half of the string, a return value
 *        firstws -- if set, the halves are delimited by the first unquoted
 *                    whitespace, else by the last unquoted whitespace
 *   strip_internal_label_quotes -- unescaped quotes in the middle of the label
 *                                   are removed. This is useful for vars
 *                                   like display-filters and url-viewers
 *                                   which may require quoting of an arg
 *                                   inside of a _TOKEN_.
 */
void
get_pair(string, label, value, firstws, strip_internal_label_quotes)
    char *string, **label, **value;
    int   firstws;
    int   strip_internal_label_quotes;
{
    char *p, *q, *tmp, *token = NULL;
    int	  quoted = 0;

    *label = *value = NULL;

    /*
     * This for loop just finds the beginning of the value. If firstws
     * is set, then it begins after the first whitespace. Otherwise, it begins
     * after the last whitespace. Quoted whitespace doesn't count as
     * whitespace. If there is no unquoted whitespace, then there is no
     * label, there's just a value.
     */
    for(p = string; p && *p;){
	if(*p == '"')				/* quoted label? */
	  quoted = (quoted) ? 0 : 1;

	if(*p == '\\' && *(p+1) == '"')		/* escaped quote? */
	  p++;					/* skip it... */

	if(isspace((unsigned char)*p) && !quoted){	/* if space,  */
	    while(*++p && isspace((unsigned char)*p))	/* move past it */
	      ;

	    if(!firstws || !token)
	      token = p;			/* remember start of text */
	}
	else
	  p++;
    }

    if(token){					/* copy label */
	*label = p = (char *)fs_get(((token - string) + 1) * sizeof(char));

	/* make a copy of the string */
	tmp = (char *)fs_get(((token - string) + 1) * sizeof(char));
	strncpy(tmp, string, token - string);
	tmp[token-string] = '\0';

	removing_leading_and_trailing_white_space(tmp);
	quoted = removing_double_quotes(tmp);
	
	for(q = tmp; *q; q++){
	    if(quoted && *q == '\\' && (*(q+1) == '"' || *(q+1) == '\\'))
	      *p++ = *++q;
	    else if(!(strip_internal_label_quotes && *q == '"'))
	      *p++ = *q;
	}

	*p = '\0';				/* tie off label */
	fs_give((void **)&tmp);
	if(*label == '\0')
	  fs_give((void **)label);
    }
    else
      token = string;

    if(token){					/* copy value */
	*value = p = (char *)fs_get((strlen(token) + 1) * sizeof(char));

	tmp = cpystr(token);
	removing_leading_and_trailing_white_space(tmp);
	quoted = removing_double_quotes(tmp);

	for(q = tmp; *q ; q++){
	    if(quoted && *q == '\\' && (*(q+1) == '"' || *(q+1) == '\\'))
	      *p++ = *++q;
	    else
	      *p++ = *q;
	}

	*p = '\0';				/* tie off value */
	fs_give((void **)&tmp);
    }
}

void
removing_leading_and_trailing_white_space(string)
     char *string;
{
    register char *p, *q = NULL;

    if(!string)
      return;

    for(p = string; *p; p++)		/* find the first non-blank  */
      if(!isspace((unsigned char)*p)){
	  while(*string = *p++){	/* copy back from there... */
	      q = (!isspace((unsigned char)*string)) ? NULL : (!q) ? string : q;
	      string++;
	  }

	  if(q)
	    *q = '\0';
	    
	  return;
      }

    if(*string != '\0')
      *string = '\0';
}

/*----------------------------------------------------------------------  
       Remove one set of double quotes surrounding string in place
       Returns 1 if quotes were removed
  
  Args: string -- string to remove quotes from
  ----*/
int
removing_double_quotes(string)
     char *string;
{
    register char *p;
    int ret = 0;

    if(string && string[0] == '"' && string[1] != '\0'){
	p = string + strlen(string) - 1;
	if(*p == '"'){
	    ret++;
	    *p = '\0';
	    for(p = string; *p; p++) 
	      *p = *(p+1);
	}
    }

    return(ret);
}
