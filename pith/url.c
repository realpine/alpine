#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: url.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
#endif

/*
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

#include "../pith/headers.h"
#include "../pith/url.h"
#include "../pith/mailview.h"
#include "../pith/string.h"

/*
 * Internal prototypes
 */
char *rfc1738_scheme_part(char *);
int   rfc1738uchar(char *);
int   rfc1738xchar(char *);


/*
 *  * * * * * * * *      RFC 1738 support routines      * * * * * * * *
 */


/*
 * Various helpful definitions
 */
#define	RFC1738_SAFE	"$-_.+"			/* "safe" */
#define	RFC1738_EXTRA	"!*'(),"		/* "extra" */
#define	RFC1738_RSVP	";/?:@&="		/* "reserved" */
#define	RFC1738_NEWS	"-.+_"			/* valid for "news:" URL */
#define	RFC1738_FUDGE	"#{}|\\^~[]"		/* Unsafe, but popular */
#define	RFC1738_ESC(S)	(*(S) == '%' && isxpair((S) + 1))


/*
 * rfc1738_scan -- Scan the given line for possible URLs as defined
 *		   in RFC1738
 */
char *
rfc1738_scan(char *line, int *len)
{
    char *colon, *start, *end;
    int   n;

    /* process each : in the line */
    for(; (colon = strindex(line, ':')) != NULL; line = end){
	end = colon + 1;
	if(colon == line)		/* zero length scheme? */
	  continue;

	/*
	 * Valid URL (ala RFC1738 BNF)?  First, first look to the
	 * left to make sure there are valid "scheme" chars...
	 */
	start = colon - 1;
	while(1)
	  if(!(isdigit((unsigned char) *start)
	       || isalpha((unsigned char) *start)
	       || strchr("+-.", *start))){
	      start++;			/* advance over bogus char */
	      break;
	  }
	  else if(start > line)
	    start--;
	  else
	    break;

	/*
	 * Make sure everyhing up to the colon is a known scheme...
	 */
	if(start && (n = colon - start) && !isdigit((unsigned char) *start)
	   && (((n == 3
		 && (*start == 'F' || *start == 'f')
		 && !struncmp(start+1, "tp", 2))
		|| (n == 4
		    && (((*start == 'H' || *start == 'h') 
			 && !struncmp(start + 1, "ttp", 3))
			|| ((*start == 'N' || *start == 'n')
			    && !struncmp(start + 1, "ews", 3))
			|| ((*start == 'N' || *start == 'n')
			    && !struncmp(start + 1, "ntp", 3))
			|| ((*start == 'W' || *start == 'w')
			    && !struncmp(start + 1, "ais", 3))
#ifdef	ENABLE_LDAP
			|| ((*start == 'L' || *start == 'l')
			    && !struncmp(start + 1, "dap", 3))
#endif
			|| ((*start == 'I' || *start == 'i')
			    && !struncmp(start + 1, "map", 3))
			|| ((*start == 'F' || *start == 'f')
			    && !struncmp(start + 1, "ile", 3))))
		|| (n == 5
		    && (*start == 'H' || *start == 'h')
		    && !struncmp(start+1, "ttps", 4))
		|| (n == 6
		    && (((*start == 'G' || *start == 'g')
			 && !struncmp(start+1, "opher", 5))
			|| ((*start == 'M' || *start == 'm')
			    && !struncmp(start + 1, "ailto", 5))
			|| ((*start == 'T' || *start == 't')
			    && !struncmp(start + 1, "elnet", 5))))
		|| (n == 8
		    && (*start == 'P' || *start == 'p')
		    && !struncmp(start + 1, "rospero", 7)))
	       || url_external_specific_handler(start, n))){
		/*
		 * Second, make sure that everything to the right of the
		 * colon is valid for a "schemepart"...
		 */

	    if((end = rfc1738_scheme_part(colon + 1)) - colon > 1){
		int i, j;

		/* make sure something useful follows colon */
		for(i = 0, j = end - colon; i < j; i++)
		  if(!strchr(RFC1738_RSVP, colon[i]))
		    break;

		if(i != j){
		    *len = end - start;

		    /*
		     * Special case handling for comma.
		     * See the problem is comma's valid, but if it's the
		     * last character in the url, it's likely intended
		     * as a delimiter in the text rather part of the URL.
		     * In most cases any way, that's why we have the
		     * exception.
		     */
		    if(*(end - 1) == ','
		       || (*(end - 1) == '.' && (!*end  || *end == ' ')))
		      (*len)--;

		    if(*len - (colon - start) > 0)
		      return(start);
		}
	    }
	}
    }

    return(NULL);
}


/*
 * rfc1738_scheme_part - make sure what's to the right of the
 *			 colon is valid
 *
 * NOTE: we have a problem matching closing parens when users 
 *       bracket the url in parens.  So, lets try terminating our
 *	 match on any closing paren that doesn't have a coresponding
 *       open-paren.
 */
char *
rfc1738_scheme_part(char *s)
{
    int n, paren = 0, bracket = 0;

    while(1)
      switch(*s){
	default :
	  if((n = rfc1738xchar(s)) != 0){
	      s += n;
	      break;
	  }

	case '\0' :
	  return(s);

	case '[' :
	  bracket++;
	  s++;
	  break;

	case ']' :
	  if(bracket--){
	      s++;
	      break;
	  }

	  return(s);

	case '(' :
	  paren++;
	  s++;
	  break;

	case ')' :
	  if(paren--){
	      s++;
	      break;
	  }

	  return(s);
      }
}



/*
 * rfc1738_str - convert rfc1738 escaped octets in place
 */
char *
rfc1738_str(char *s)
{
    register char *p = s, *q = s;

    while(1)
      switch(*q = *p++){
	case '%' :
	  if(isxpair(p)){
	      *q = X2C(p);
	      p += 2;
	  }

	default :
	  q++;
	  break;

	case '\0':
	  return(s);
      }
}


/*
 * rfc1738uchar - returns TRUE if the given char fits RFC 1738 "uchar" BNF
 */
int
rfc1738uchar(char *s)
{
    return((RFC1738_ESC(s))		/* "escape" */
	     ? 2
	     : (isalnum((unsigned char) *s)	/* alphanumeric */
		|| strchr(RFC1738_SAFE, *s)	/* other special stuff */
		|| strchr(RFC1738_EXTRA, *s)));
}


/*
 * rfc1738xchar - returns TRUE if the given char fits RFC 1738 "xchar" BNF
 */
int
rfc1738xchar(char *s)
{
    int n;

    return((n = rfc1738uchar(s))
	    ? n
	    : (strchr(RFC1738_RSVP, *s) != NULL
	       || strchr(RFC1738_FUDGE, *s)));
}


/*
 * rfc1738_num - return long value of a string of digits, possibly escaped
 */
unsigned long
rfc1738_num(char **s)
{
    register char *p = *s;
    unsigned long n = 0L;

    for(; *p; p++)
      if(*p == '%' && isxpair(p+1)){
	  int c = X2C(p+1);
	  if(isdigit((unsigned char) c)){
	      n = (c - '0') + (n * 10);
	      p += 2;
	  }
	  else
	    break;
      }
      else if(isdigit((unsigned char) *p))
	n = (*p - '0') + (n * 10);
      else
	break;

    *s = p;
    return(n);
}


int
rfc1738_group(char *s)
{
    return(isalnum((unsigned char) *s)
	   || RFC1738_ESC(s)
	   || strchr(RFC1738_NEWS, *s));
}


/*
 * Encode (hexify) a mailto url.
 *
 * Args  s -- src url
 *
 * Returns  An allocated string which is suitably encoded.
 *          Result should be freed by caller.
 *
 * Since we don't know here which characters are reserved characters (? and &)
 * for use in delimiting the pieces of the url and which are just those
 * characters contained in the data that should be encoded, we always encode
 * them. That's because we know we don't use those as reserved characters.
 * If you do use those as reserved characters you have to encode each part
 * separately.
 */
char *
rfc1738_encode_mailto(char *s)
{
    char *d, *ret = NULL;

    if(s){
	/* Worst case, encode every character */
	ret = d = (char *)fs_get((3*strlen(s) + 1) * sizeof(char));
	while(*s){
	    if(isalnum((unsigned char)*s)
	       || strchr(RFC1738_SAFE, *s)
	       || strchr(RFC1738_EXTRA, *s))
	      *d++ = *s++;
	    else{
		*d++ = '%';
		C2XPAIR(*s, d);
		s++;
	    }
	}

	*d = '\0';
    }

    return(ret);
}


/*
 *  * * * * * * * *      RFC 1808 support routines      * * * * * * * *
 */


int
rfc1808_tokens(char *url, char **scheme, char **net_loc, char **path,
	       char **parms, char **query, char **frag)
{
    char *p, *q, *start, *tmp = cpystr(url);

    start = tmp;
    if((p = strchr(start, '#')) != NULL){	/* fragment spec? */
	*p++ = '\0';
	if(*p)
	  *frag = cpystr(p);
    }

    if((p = strchr(start, ':')) && p != start){ /* scheme part? */
	for(q = start; q < p; q++)
	  if(!(isdigit((unsigned char) *q)
	       || isalpha((unsigned char) *q)
	       || strchr("+-.", *q)))
	    break;

	if(p == q){
	    *p++ = '\0';
	    *scheme = cpystr(start);
	    start = p;
	}
    }

    if(*start == '/' && *(start+1) == '/'){ /* net_loc */
	if((p = strchr(start+2, '/')) != NULL)
	  *p++ = '\0';

	*net_loc = cpystr(start+2);
	if(p)
	  start = p;
	else *start = '\0';		/* End of parse */
    }

    if((p = strchr(start, '?')) != NULL){
	*p++ = '\0';
	*query = cpystr(p);
    }

    if((p = strchr(start, ';')) != NULL){
	*p++ = '\0';
	*parms = cpystr(p);
    }

    if(*start)
      *path = cpystr(start);

    fs_give((void **) &tmp);

    return(1);
}



/*
 * web_host_scan -- Scan the given line for possible web host names
 *
 * NOTE: scan below is limited to DNS names ala RFC1034
 */
char *
web_host_scan(char *line, int *len)
{
    char *end, last = '\0';

    for(; *line; last = *line++)
      if((*line == 'w' || *line == 'W')
	 && (!last || !(isalnum((unsigned char) last)
			|| last == '.' || last == '-' || last == '/'))
	 && (((*(line + 1) == 'w' || *(line + 1) == 'W')	/* "www." */
	      && (*(line + 2) == 'w' || *(line + 2) == 'W'))
	     || ((*(line + 1) == 'e' || *(line + 1) == 'E')	/* "web." */
		 && (*(line + 2) == 'b' || *(line + 2) == 'B')))
	 && (*(line + 3) == '.')){
	  end = rfc1738_scheme_part(line + 3);
	  if((*len = end - line) > ((*(line+3) == '.') ? 4 : 3)){
	      /* Dread comma exception, see note in rfc1738_scan */
	      if(strchr(",:", *(line + (*len) - 1))
		 || (*(line + (*len) - 1) == '.'
		     && (!*(line + (*len)) || *(line + (*len)) == ' ')))
		(*len)--;

	      return(line);
	  }
	  else
	    line += 3;
      }

    return(NULL);
}


/*
 * mail_addr_scan -- Scan the given line for possible RFC822 addr-spec's
 *
 * NOTE: Well, OK, not strictly addr-specs since there's alot of junk
 *	 we're tying to sift thru and we'd like to minimize false-pos
 *	 matches.
 */
char *
mail_addr_scan(char *line, int *len)
{
    char *amp, *start, *end;
/*
 * This list is not the whole standards-based list, this is just a list
 * of likely email address characters. We don't want to include everything
 * because punctuation in the text might get mixed in with the address.
 */
#define NONALPHANUMOK ".-_+%/="

    /* process each : in the line */
    for(; (amp = strindex(line, '@')) != NULL; line = end){
	end = amp + 1;
	/* zero length addr? */
	if(amp == line || !(isalnum((unsigned char) *(start = amp - 1))
			    || strchr(NONALPHANUMOK, *start)))
	  continue;

	/*
	 * Valid address (ala RFC822 BNF)?  First, first look to the
	 * left to make sure there are valid "scheme" chars...
	 */
	while(1)
	  /* NOTE: we're not doing quoted-strings */
	  if(!(isalnum((unsigned char) *start) || strchr(NONALPHANUMOK, *start))){
	      /* advance over bogus char, and erase leading punctuation */
	      for(start++; *start && strchr(NONALPHANUMOK, *start); start++)
	        ;

	      break;
	  }
	  else if(start > line)
	    start--;
	  else
	    break;

	/*
	 * Make sure everyhing up to the colon is a known scheme...
	 */
	if(start && (amp - start) > 0){
	    /*
	     * Second, make sure that everything to the right of
	     * amp is valid for a "domain"...
	     */
	    if(*(end = amp + 1) == '['){ /* domain literal */
		int dots = 3;

		for(++end; *end ; end++)
		  if(*end == ']'){
		      if(!dots){
			  *len = end - start + 1;
			  return(start);
		      }
		      else
			break;		/* bogus */
		  }
		  else if(*end == '.'){
		      if(--dots < 0)
			break;		/* bogus */
		  }
		  else if(!isdigit((unsigned char) *end))
		    break;		/* bogus */
	    }
	    else if(isalnum((unsigned char) *end)){ /* domain name? */
		for(++end; ; end++)
		  if(!(*end && (isalnum((unsigned char) *end)
				|| *end == '-'
				|| *end == '.'
				|| *end == '_'))){
		      /* can't end with dash, dot or underscore */
		      while(!isalnum((unsigned char) *(end - 1)))
			end--;

		      *len = end - start;
		      return(start);
		  }
	    }
	}
    }

    return(NULL);
}
