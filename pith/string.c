#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: string.c 910 2008-01-14 22:28:38Z hubert@u.washington.edu $";
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

/*======================================================================
    string.c
    Misc extra and useful string functions
      - rplstr         replace a substring with another string
      - sqzspaces      Squeeze out the extra blanks in a string
      - sqznewlines    Squeeze out \n and \r.
      - removing_trailing_white_space 
      - short_str      Replace part of string with ... for display
      - removing_leading_white_space 
		       Remove leading or trailing white space
      - removing_double_quotes 
		       Remove surrounding double quotes
      - strclean       
                       both of above plus convert to lower case
      - skip_white_space
		       return pointer to first non-white-space char
      - skip_to_white_space
		       return pointer to first white-space char
      - srchstr        Search a string for first occurrence of a sub string
      - srchrstr       Search a string for last occurrence of a sub string
      - strindex       Replacement for strchr/index
      - strrindex      Replacement for strrchr/rindex
      - sstrncpy       Copy one string onto another, advancing dest'n pointer
      - istrncpy       Copy n chars between bufs, making ctrl chars harmless
      - month_abbrev   Return three letter abbreviations for months
      - month_num      Calculate month number from month/year string
      - cannon_date    Formalize format of a some what formatted date
      - repeat_char    Returns a string n chars long
      - fold           Inserts newlines for folding at whitespace.
      - byte_string    Format number of bytes with Kb, Mb, Gb or bytes
      - enth-string    Format number i.e. 1: 1st, 983: 983rd....
      - string_to_cstring  Convert string to C-style constant string with \'s
      - cstring_to_hexstring  Convert cstring to hex string
      - cstring_to_string  Convert C-style string to string
      - add_backslash_escapes    Escape / and \ with \
      - remove_backslash_escapes Undo the \ escaping, and stop string at /.

 ====*/

#include "../pith/headers.h"
#include "../pith/string.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/escapes.h"
#include "../pith/util.h"


void        char_to_octal_triple(int, char *);
char       *dollar_escape_dollars(char *);
time_t      date_to_local_time_t(char *date);



/*----------------------------------------------------------------------
       Replace dl characters in one string with another given string

   args: os -- pointer into output string
      oslen -- size of output string starting at os
         dl -- the number of character to delete starting at os
         is -- The string to insert
  
 Result: returns pointer in originl string to end of string just inserted
  ---*/
char *
rplstr(char *os, size_t oslen, int dl, char *is)
{   
    char *x1, *x2, *x3;
    int   diff;

    if(os == NULL)
        return(NULL);
       
    x1 = os + strlen(os);

    /* can't delete more characters than there are */
    if(dl > x1 - os)
        dl = x1 - os;
        
    x2 = is;      
    if(is != NULL)
      x2 = is + strlen(is);

    diff = (x2 - is) - dl;

    if(diff < 0){				/* String shrinks */
        x3 = os;
        if(is != NULL)
          for(x2 = is; *x2; *x3++ = *x2++)	/* copy new string in */
	      ;

        for(x2 = x3 - diff; *x2; *x3++ = *x2++)	/* shift for delete */
	  ;

        *x3 = *x2;				/* the null */
    }
    else{					/* String grows */
	/* make room for insert */
	x3 = x1 + diff;
	if(x3 >= os + oslen)		/* just protecting ourselves */
	  x3 = os + oslen - 1;

        for(; x3 >= os + (x2 - is); *x3-- = *x1--); /* shift*/
	  ;

	if(is != NULL)
          for(x1 = os, x2 = is; *x2 ; *x1++ = *x2++)
	    ;

        while(*x3) x3++;                 
    }

    os[oslen-1] = '\0';

    return(x3);
}



/*----------------------------------------------------------------------
     Squeeze out blanks 
  ----------------------------------------------------------------------*/
void
sqzspaces(char *string)
{
    char *p = string;

    while((*string = *p++) != '\0')	   /* while something to copy       */
      if(!isspace((unsigned char)*string)) /* only really copy if non-blank */
	string++;
}



/*----------------------------------------------------------------------
     Squeeze out CR's and LF's 
  ----------------------------------------------------------------------*/
void
sqznewlines(char *string)
{
    char *p = string;

    while((*string = *p++) != '\0')	      /* while something to copy  */
      if(*string != '\r' && *string != '\n')  /* only copy if non-newline */
	string++;
}



/*----------------------------------------------------------------------  
       Remove leading white space from a string in place
  
  Args: string -- string to remove space from
  ----*/
void
removing_leading_white_space(char *string)
{
    register char *p;

    if(!string || !*string || !isspace(*string))
      return;

    for(p = string; *p; p++)		/* find the first non-blank  */
      if(!isspace((unsigned char) *p)){
	  while((*string++ = *p++) != '\0')	/* copy back from there... */
	    ;

	  return;
      }
}



/*----------------------------------------------------------------------  
       Remove trailing white space from a string in place
  
  Args: string -- string to remove space from
  ----*/
void
removing_trailing_white_space(char *string)
{
    char *p = NULL;

    if(!string || !*string)
      return;

    for(; *string; string++)		/* remember start of whitespace */
      p = (!isspace((unsigned char)*string)) ? NULL : (!p) ? string : p;

    if(p)				/* if whitespace, blast it */
      *p = '\0';
}


void
removing_leading_and_trailing_white_space(char *string)
{
    register char *p, *q = NULL;

    if(!string || !*string)
      return;

    for(p = string; *p; p++)		/* find the first non-blank  */
      if(!isspace((unsigned char)*p)){
	  if(p == string){		/* don't have to copy in this case */
	      for(; *string; string++)
		q = (!isspace((unsigned char)*string)) ? NULL : (!q) ? string : q;
	  }
	  else{
	      for(; (*string = *p++) != '\0'; string++)
		q = (!isspace((unsigned char)*string)) ? NULL : (!q) ? string : q;
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
removing_double_quotes(char *string)
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



/*----------------------------------------------------------------------  
  return a pointer to first non-whitespace char in string
  
  Args: string -- string to scan
  ----*/
char *
skip_white_space(char *string)
{
    while(*string && isspace((unsigned char) *string))
      string++;

    return(string);
}



/*----------------------------------------------------------------------  
  return a pointer to first whitespace char in string
  
  Args: string -- string to scan
  ----*/
char *
skip_to_white_space(char *string)
{
    while(*string && !isspace((unsigned char) *string))
      string++;

    return(string);
}



/*----------------------------------------------------------------------  
       Remove quotes from a string in place
  
  Args: string -- string to remove quotes from
  Rreturns: string passed us, but with quotes gone
  ----*/
char *
removing_quotes(char *string)
{
    char *p, *q;

    if(*(p = q = string) == '\"'){
	q++;
	do
	  if(*q == '\"' || *q == '\\')
	    q++;
	while((*p++ = *q++) != '\0');
    }

    return(string);
}



/*---------------------------------------------------
     Remove leading whitespace, trailing whitespace and convert 
     to lowercase

   Args: s, -- The string to clean

 Result: the cleaned string
  ----*/
char *
strclean(char *string)
{
    char *s = string, *sc = NULL, *p = NULL;

    for(; *s; s++){				/* single pass */
	if(!isspace((unsigned char)*s)){
	    p = NULL;				/* not start of blanks   */
	    if(!sc)				/* first non-blank? */
	      sc = string;			/* start copying */
	}
	else if(!p)				/* it's OK if sc == NULL */
	  p = sc;				/* start of blanks? */

	if(sc)					/* if copying, copy */
	  *sc++ = isupper((unsigned char)(*s))
			  ? (unsigned char)tolower((unsigned char)(*s))
			  : (unsigned char)(*s);
    }

    if(p)					/* if ending blanks  */
      *p = '\0';				/* tie off beginning */
    else if(!sc)				/* never saw a non-blank */
      *string = '\0';				/* so tie whole thing off */

    return(string);
}


/*
 * Returns a pointer to a short version of the string.
 * If src is not longer than wid, pointer points to src.
 * If longer than wid, a version which is wid long is made in
 * buf and the pointer points there.
 *
 * Wid refers to UTF-8 screen width, not strlen width.
 *
 * Args  src -- The string to be shortened
 *       buf -- A place to put the short version
 *       wid -- Desired width of shortened string
 *     where -- Where should the dots be in the shortened string. Can be
 *              FrontDots, MidDots, EndDots.
 *
 *     FrontDots           ...stuvwxyz
 *     EndDots             abcdefgh...
 *     MidDots             abcd...wxyz
 */
char *
short_str(char *src, char *buf, size_t buflen, int wid, WhereDots where)
{
    char *ans;
    unsigned alen, first, second;

    if(wid <= 0){
	ans = buf;
	if(buflen > 0)
	  buf[0] = '\0';
    }
    else if((alen = utf8_width(src)) <= wid)
      ans = src;
    else{
	ans = buf;
	if(wid < 5){
	    if(buflen > wid){
		strncpy(buf, "....", buflen);
		buf[wid] = '\0';
	    }
	}
	else{
	    char *q;
	    unsigned got_width;

	    /*
	     * first == length of preellipsis text
	     * second == length of postellipsis text
	     */
	    if(where == FrontDots){
		first = 0;
		second = wid - 3;
	    }
	    else if(where == MidDots){
		first = (wid - 3)/2;
		second = wid - 3 - first;
	    }
	    else if(where == EndDots){
		first = wid - 3;
		second = 0;
	    }

	    q = buf;
	    if(first){
		q += utf8_to_width(q, src, buflen, first, &got_width);
		if(got_width != first){
		  if(second)
		    second++;
		  else
		    while(got_width < first && buflen-(q-buf) > 0)
		      *q++ = '.';
		}
	    }

	    if(buflen - (q-buf) > 3){
		strncpy(q, "...", buflen - (q-buf));
		buf[buflen-1] = '\0';
		q += strlen(q);
	    }
	    
	    if(second){
		char *p;

		p = utf8_count_back_width(src, src+strlen(src), second, &got_width);
		if(buflen - (q-buf) > strlen(p)){
		    strncpy(q, p, buflen - (q-buf));
		    buf[buflen-1] = '\0';
		    q += strlen(q);
		}
	    }

	    if(buflen - (q-buf) > 0)
	      *q = '\0';

	    buf[buflen-1] = '\0';
	}
    }
    
    return(ans);
}



/*----------------------------------------------------------------------
        Search one string for another

   Args:  haystack -- The string to search in, the larger string
          needle   -- The string to search for, the smaller string

   Search for first occurrence of needle in the haystack, and return a pointer
   into the string haystack when it is found. The text we are dealing with is
   UTF-8. We'd like the search to be case-independent but we're not sure what
   that means for UTF-8. We're not even sure what matching means. We're not going
   to worry about composed characters and canonical forms and anything like that
   for now. Instead, we'll do the case-independent thing for ascii but exact
   equality for the rest of the character space.
  ----*/
char *	    
srchstr(char *haystack, char *needle)
{
    char *p, *q;

#define	CMPNOCASE(x, y)	(((isascii((unsigned char) (x)) && isupper((unsigned char) (x))) \
			    ? tolower((unsigned char) (x))		     \
			    : (unsigned char) (x))			     \
		    == ((isascii((unsigned char) (y)) && isupper((unsigned char) (y))) \
				? tolower((unsigned char) (y))		     \
				: (unsigned char) (y)))

    if(needle && haystack)
      for(; *haystack; haystack++)
	for(p = needle, q = haystack; ; p++, q++){
	    if(!*p)
	      return(haystack);			/* winner! */
	    else if(!*q)
	      return(NULL);			/* len(needle) > len(haystack)! */
	    else if(*p != *q && !CMPNOCASE(*p, *q))
	      break;
	}

    return(NULL);
}



/*----------------------------------------------------------------------
        Search one string for another, from right

   Args:  is -- The string to search in, the larger string
          ss -- The string to search for, the smaller string

   Search for last occurrence of ss in the is, and return a pointer
   into the string is when it is found. The search is case indepedent.
  ----*/

char *	    
srchrstr(register char *is, register char *ss)
{                    
    register char *sx, *sy;
    char          *ss_store, *rv;
    char          *begin_is;
    char           temp[251];
    
    if(is == NULL || ss == NULL)
      return(NULL);

    if(strlen(ss) > sizeof(temp) - 2)
      ss_store = (char *)fs_get(strlen(ss) + 1);
    else
      ss_store = temp;

    for(sx = ss, sy = ss_store; *sx != '\0' ; sx++, sy++)
      *sy = isupper((unsigned char)(*sx))
		      ? (unsigned char)tolower((unsigned char)(*sx))
		      : (unsigned char)(*sx);
    *sy = *sx;

    begin_is = is;
    is = is + strlen(is) - strlen(ss_store);
    rv = NULL;
    while(is >= begin_is){
        for(sx = is, sy = ss_store;
	    ((*sx == *sy)
	      || ((isupper((unsigned char)(*sx))
		     ? (unsigned char)tolower((unsigned char)(*sx))
		     : (unsigned char)(*sx)) == (unsigned char)(*sy))) && *sy;
	    sx++, sy++)
	   ;

        if(!*sy){
            rv = is;
            break;
        }

        is--;
    }

    if(ss_store != temp)
      fs_give((void **)&ss_store);

    return(rv);
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
strindex(char *buffer, int ch)
{
    do
      if(*buffer == ch)
	return(buffer);
    while (*buffer++ != '\0');

    return(NULL);
}


/* Returns a pointer to the last occurrence of the character
 * 'ch' in the specified string or NULL if it doesn't occur
 */
char *
strrindex(char *buffer, int ch)
{
    char *address = NULL;

    do
      if(*buffer == ch)
	address = buffer;
    while (*buffer++ != '\0');
    return(address);
}


/*----------------------------------------------------------------------
  copy at most n chars of the UTF-8 source string onto the destination string
  returning pointer to start of destination and converting any undisplayable
  characters to harmless character equivalents.
 ----*/
char *
iutf8ncpy(char *d, char *s, int n)
{
    register int i;

    if(!d || !s)
      return(NULL);

    /*
     * BUG: this needs to get improved to actually count the
     * character "cell" positions.  For now, at least don't break
     * a multi-byte character.
     */
    for(i = 0; i < n && (d[i] = *s) != '\0'; s++, i++)
      if((unsigned char)(*s) < 0x80 && FILTER_THIS(*s)){
	  if(i+1 < n){
	      d[i]   = '^';
	      d[++i] = (*s == 0x7f) ? '?' : *s + '@';
	  }
	  else{
	      d[i] = '\0';
	      break;		/* don't fit */
	  }
      }
      else if(*s & 0x80){
	  /* multi-byte character */
	  if((*s & 0xE0) == 0xC0){
	      if(i+1 < n){
		  if(((d[++i] = *++s) & 0xC0) != 0x80){
		      d[i] = '\0';
		      break;	/* bogus utf-8 */
		  }
	      }
	      else{
		  d[i] = '\0';
		  break;		/* too long */
	      }
	  }
	  else if((*s & 0xF0) == 0xE0){
	      if(i+2 < n){
		  if(!(((d[++i] = *++s) & 0xC0) == 0x80
		       && ((d[++i] = *++s) & 0xC0) == 0x80)){
		      d[i] = '\0';
		      break;	/* bogus utf-8 */
		  }
	      }
	      else{
		  d[i] = '\0';
		  break;	/* won't fit */
	      }
	  }
	  else if((*s & 0xF8) == 0xF0){
	      if(i+3 < n){
		  if(!(((d[++i] = *++s) & 0xC0) == 0x80
		       && ((d[++i] = *++s) & 0xC0) == 0x80
		       && ((d[++i] = *++s) & 0xC0) == 0x80)){
		      d[i] = '\0';
		      break;	/* bogus utf-8 */
		  }
	      }
	      else{
		  d[i] = '\0';
		  break;	/* won't fit */
	      }
	  }
	  else if((*s & 0xFC) == 0xF8){
	      if(i+4 < n){
		  if(!(((d[++i] = *++s) & 0xC0) == 0x80
		       && ((d[++i] = *++s) & 0xC0) == 0x80
		       && ((d[++i] = *++s) & 0xC0) == 0x80
		       && ((d[++i] = *++s) & 0xC0) == 0x80)){
		      d[i] = '\0';
		      break;	/* bogus utf-8 */
		  }
	      }
	      else{
		  d[i] = '\0';
		  break;	/* won't fit */
	      }
	  }
	  else if((*s & 0xFE) == 0xFC){
	      if(i+5 < n){
		  if(!(((d[++i] = *++s) & 0xC0) == 0x80
		       && ((d[++i] = *++s) & 0xC0) == 0x80
		       && ((d[++i] = *++s) & 0xC0) == 0x80
		       && ((d[++i] = *++s) & 0xC0) == 0x80
		       && ((d[++i] = *++s) & 0xC0) == 0x80)){
		      d[i] = '\0';
		      break;	/* bogus utf-8 */
		  }
	      }
	      else{
		  d[i] = '\0';
		  break;	/* won't fit */
	      }
	  }
	  else{
	      d[i] = '\0';
	      break;		/* don't fit */
	  }
      }

    return(d);
}


/*----------------------------------------------------------------------
  copy at most n chars of the source string onto the destination string
  returning pointer to start of destination and converting any undisplayable
  characters to harmless character equivalents.
 ----*/
char *
istrncpy(char *d, char *s, int n)
{
    char *rv = d;
    unsigned char c;

    if(!d || !s)
      return(NULL);
    
    do
      if(*s && (unsigned char)(*s) < 0x80 && FILTER_THIS(*s)
	 && !(*(s+1) && *s == ESCAPE && match_escapes(s+1))){
	if(n-- > 0){
	    c = (unsigned char) *s;
	    *d++ = c >= 0x80 ? '~' : '^';

	    if(n-- > 0){
		s++;
		*d = (c == 0x7f) ? '?' : (c & 0x1f) + '@';
	    }
	}
      }
      else{
	  if(n-- > 0)
	    *d = *s++;
      }
    while(n > 0 && *d++);

    return(rv);
}


char *xdays[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", NULL};

char *
month_abbrev(int month_num)
{
    static char *xmonths[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec", NULL};
    if(month_num < 1 || month_num > 12)
      return("xxx");
    return(xmonths[month_num - 1]);
}

char *
month_abbrev_locale(int month_num)
{
#ifndef DISABLE_LOCALE_DATES
    if(F_OFF(F_DISABLE_INDEX_LOCALE_DATES, ps_global)){
	if(month_num < 1 || month_num > 12)
	  return("xxx");
	else{
	    static char buf[20];
	    struct tm tm;

	    memset(&tm, 0, sizeof(tm));
	    tm.tm_year = 107;
	    tm.tm_mon = month_num-1;
	    our_strftime(buf, sizeof(buf), "%b", &tm);

	    /*
	     * If it is all digits, then use the English
	     * words instead. Look for
	     *    "<digit>"
	     *    "<digit><digit>"    or
	     *    "<space><digit>"
	     */
	    if((buf[0] && !(buf[0] & 0x80)
	         && isdigit((unsigned char)buf[0]) && !buf[1])
	       ||
	       (buf[0] && !(buf[0] & 0x80)
	         && (isdigit((unsigned char)buf[0]) || buf[0] == ' ')
		 && buf[1] && !(buf[1] & 0x80)
		 && isdigit((unsigned char)buf[1]) && !buf[2]))
	      return(month_abbrev(month_num));

	    /*
	     * If buf[0] is a digit then assume that there should be a leading
	     * space if it leads off with a single digit.
	     */
	    if(buf[0] && !(buf[0] & 0x80) && isdigit((unsigned char) buf[0])
	       && !(buf[1] && !(buf[1] & 0x80) && isdigit((unsigned char) buf[1]))){
		char *p;

		/* insert space at start of buf */
		p = buf+strlen(buf) + 1;
		if(p > buf + sizeof(buf) - 1)
		  p = buf + sizeof(buf) - 1;

		for(; p > buf; p--)
		  *p = *(p-1);

		buf[0] = ' ';
	    }

	    return(buf);
	}
    }
    else
      return(month_abbrev(month_num));
#else /* DISABLE_LOCALE_DATES */
    return(month_abbrev(month_num));
#endif /* DISABLE_LOCALE_DATES */
}

char *
month_name(int month_num)
{
    static char *months[] = {"January", "February", "March", "April",
		"May", "June", "July", "August", "September", "October",
		"November", "December", NULL};
    if(month_num < 1 || month_num > 12)
      return("");
    return(months[month_num - 1]);
}

char *
month_name_locale(int month_num)
{
#ifndef DISABLE_LOCALE_DATES
    if(F_OFF(F_DISABLE_INDEX_LOCALE_DATES, ps_global)){
	if(month_num < 1 || month_num > 12)
	  return("");
	else{
	    static char buf[20];
	    struct tm tm;

	    memset(&tm, 0, sizeof(tm));
	    tm.tm_year = 107;
	    tm.tm_mon = month_num-1;
	    our_strftime(buf, sizeof(buf), "%B", &tm);
	    return(buf);
	}
    }
    else
      return(month_name(month_num));
#else /* DISABLE_LOCALE_DATES */
    return(month_name(month_num));
#endif /* DISABLE_LOCALE_DATES */
}


char *
day_abbrev(int day_of_week)
{
    if(day_of_week < 0 || day_of_week > 6)
      return("???");
    return(xdays[day_of_week]);
}

char *
day_abbrev_locale(int day_of_week)
{
#ifndef DISABLE_LOCALE_DATES
    if(F_OFF(F_DISABLE_INDEX_LOCALE_DATES, ps_global)){
	if(day_of_week < 0 || day_of_week > 6)
	  return("???");
	else{
	    static char buf[20];
	    struct tm tm;

	    memset(&tm, 0, sizeof(tm));
	    tm.tm_wday = day_of_week;
	    our_strftime(buf, sizeof(buf), "%a", &tm);
	    return(buf);
	}
    }
    else
      return(day_abbrev(day_of_week));
#else /* DISABLE_LOCALE_DATES */
    return(day_abbrev(day_of_week));
#endif /* DISABLE_LOCALE_DATES */
}

char *
day_name(int day_of_week)
{
    static char *days[] = {"Sunday", "Monday", "Tuesday", "Wednesday",
		"Thursday", "Friday", "Saturday", NULL};
    if(day_of_week < 0 || day_of_week > 6)
      return("");
    return(days[day_of_week]);
}

char *
day_name_locale(int day_of_week)
{
#ifndef DISABLE_LOCALE_DATES
    if(F_OFF(F_DISABLE_INDEX_LOCALE_DATES, ps_global)){
	if(day_of_week < 0 || day_of_week > 6)
	  return("");
	else{
	    static char buf[20];
	    struct tm tm;

	    memset(&tm, 0, sizeof(tm));
	    tm.tm_wday = day_of_week;
	    our_strftime(buf, sizeof(buf), "%A", &tm);
	    return(buf);
	}
    }
    else
      return(day_name(day_of_week));
#else /* DISABLE_LOCALE_DATES */
    return(day_name(day_of_week));
#endif /* DISABLE_LOCALE_DATES */
}


size_t
our_strftime(char *dst, size_t dst_size, char *format, struct tm *tm)
{
#ifdef _WINDOWS
    LPTSTR lptbuf, lptformat;
    char *u;

    lptbuf = (LPTSTR) fs_get(dst_size * sizeof(TCHAR));
    lptbuf[0] = '\0';
    lptformat = utf8_to_lptstr((LPSTR) format);

    _tcsftime(lptbuf, dst_size, lptformat, tm);
    u = lptstr_to_utf8(lptbuf);
    if(u){
	strncpy(dst, u, dst_size);
	dst[dst_size-1] = '\0';
	fs_give((void **) &u);
    }

    return(strlen(dst));
#else
    return(strftime(dst, dst_size, format, tm));
#endif
}


/*----------------------------------------------------------------------
      Return month number of month named in string
  
   Args: s -- string with 3 letter month abbreviation of form mmm-yyyy
 
 Result: Returns month number with January, year 1900, 2000... being 0;
         -1 if no month/year is matched
 ----*/
int
month_num(char *s)
{
    int month = -1, year;
    int i;

    if(F_ON(F_PRUNE_USES_ISO,ps_global)){
	char save, *p;
	char digmon[3];

	if(s && strlen(s) > 4 && s[4] == '-'){
	    save = s[4];
	    s[4] = '\0';
	    year = atoi(s);
	    s[4] = save;
	    if(year == 0)
	      return(-1);
	    
	    p = s + 5;
	    for(i = 0; i < 12; i++){
		digmon[0] = ((i+1) < 10) ? '0' : '1';
		digmon[1] = '0' + (i+1) % 10;
		digmon[2] = '\0';
		if(strcmp(digmon, p) == 0)
		  break;
	    }

	    if(i == 12)
	      return(-1);
	    
	    month = year * 12 + i;
	}
    }
    else{
	if(s && strlen(s) > 3 && s[3] == '-'){
	    for(i = 0; i < 12; i++){
		if(struncmp(month_abbrev(i+1), s, 3) == 0)
		  break;
	    }

	    if(i == 12)
	      return(-1);

	    year = atoi(s + 4);
	    if(year == 0)
	      return(-1);

	    month = year * 12 + i;
	}
    }
    
    return(month);
}


/*
 * Structure containing all knowledge of symbolic time zones.
 * To add support for a given time zone, add it here, but make sure
 * the zone name is in upper case.
 */
static struct {
    char  *zone;
    short  len,
    	   hour_offset,
	   min_offset;
} known_zones[] = {
    {"PST", 3, -8, 0},			/* Pacific Standard */
    {"PDT", 3, -7, 0},			/* Pacific Daylight */
    {"MST", 3, -7, 0},			/* Mountain Standard */
    {"MDT", 3, -6, 0},			/* Mountain Daylight */
    {"CST", 3, -6, 0},			/* Central Standard */
    {"CDT", 3, -5, 0},			/* Central Daylight */
    {"EST", 3, -5, 0},			/* Eastern Standard */
    {"EDT", 3, -4, 0},			/* Eastern Daylight */
    {"JST", 3,  9, 0},			/* Japan Standard */
    {"GMT", 3,  0, 0},			/* Universal Time */
    {"UT",  2,  0, 0},			/* Universal Time */
#ifdef	IST_MEANS_ISREAL
    {"IST", 3,  2, 0},			/* Israel Standard */
#else
#ifdef	IST_MEANS_INDIA
    {"IST", 3,  5, 30},			/* India Standard */
#endif
#endif
    {NULL, 0, 0},
};

/*----------------------------------------------------------------------
  Parse date in or near RFC-822 format into the date structure

Args: given_date -- The input string to parse
      d          -- Pointer to a struct date to place the result in
 
Returns nothing

The following date fomrats are accepted:
  WKDAY DD MM YY HH:MM:SS ZZ
  DD MM YY HH:MM:SS ZZ
  WKDAY DD MM HH:MM:SS YY ZZ
  DD MM HH:MM:SS YY ZZ
  DD MM WKDAY HH:MM:SS YY ZZ
  DD MM WKDAY YY MM HH:MM:SS ZZ

All leading, intervening and trailing spaces tabs and commas are ignored.
The prefered formats are the first or second ones.  If a field is unparsable
it's value is left as -1. 

  ----*/
void
parse_date(char *given_date, struct date *d)
{
    char *p, **i, *q;
    int   month, n;

    d->sec   = -1;
    d->minute= -1;
    d->hour  = -1;
    d->day   = -1;
    d->month = -1;
    d->year  = -1;
    d->wkday = -1;
    d->hours_off_gmt = -1;
    d->min_off_gmt   = -1;

    if(given_date == NULL)
      return;

    p = given_date;
    while(*p && isspace((unsigned char)*p))
      p++;

    /* Start with weekday? */
    if((q=strchr(p, ',')) != NULL){

	if(q - p == 3){
	    *q = '\0';
	    for(i = xdays; *i != NULL; i++) 
	      if(strucmp(p, *i) == 0) /* Match first 3 letters */
		break;

	    *q = ',';

	    if(*i != NULL) {
		/* Started with week day */
		d->wkday = i - xdays;
	    }
	}

	p = q+1;
	while(*p && isspace((unsigned char)*p))
	  p++;
    }
    else if((q=strchr(p, ' ')) != NULL && q - p == 3){
	*q = '\0';
	for(i = xdays; *i != NULL; i++) 
	  if(strucmp(p, *i) == 0) /* Match first 3 letters */
	    break;

	*q = ' ';

	if(*i != NULL) {
	    /* Started with week day */
	    d->wkday = i - xdays;
	    p = q+1;
	    while(*p && isspace((unsigned char)*p))
	      p++;
	}
    }

    if(isdigit((unsigned char)*p)) {
        d->day = atoi(p);
        while(*p && isdigit((unsigned char)*p))
          p++;
        while(*p && (*p == '-' || *p == ',' || isspace((unsigned char)*p)))
          p++;
    }
    for(month = 1; month <= 12; month++)
      if(struncmp(p, month_abbrev(month), 3) == 0)
        break;
    if(month < 13) {
        d->month = month;

    } 
    /* Move over month, (or whatever is there) */
    while(*p && !isspace((unsigned char)*p) && *p != ',' && *p != '-')
       p++;
    while(*p && (isspace((unsigned char)*p) || *p == ',' || *p == '-'))
       p++;

    /* Check again for day */
    if(isdigit((unsigned char)*p) && d->day == -1) {
        d->day = atoi(p);
        while(*p && isdigit((unsigned char)*p))
          p++;
        while(*p && (*p == '-' || *p == ',' || isspace((unsigned char)*p)))
          p++;
    }

    /*-- Check for time --*/
    for(q = p; *q && isdigit((unsigned char)*q); q++);
    if(*q == ':') {
        /* It's the time (out of place) */
        d->hour = atoi(p);
        while(*p && *p != ':' && !isspace((unsigned char)*p))
          p++;
        if(*p == ':') {
            p++;
            d->minute = atoi(p);
            while(*p && *p != ':' && !isspace((unsigned char)*p))
              p++;
            if(*p == ':') {
                d->sec = atoi(p);
                while(*p && !isspace((unsigned char)*p))
                  p++;
            }
        }
        while(*p && isspace((unsigned char)*p))
          p++;
    }
    

    /* Get the year 0-49 is 2000-2049; 50-100 is 1950-1999 and
                                           101-9999 is 101-9999 */
    if(isdigit((unsigned char)*p)) {
        d->year = atoi(p);
        if(d->year < 50)   
          d->year += 2000;
        else if(d->year < 100)
          d->year += 1900;
        while(*p && isdigit((unsigned char)*p))
          p++;
        while(*p && (*p == '-' || *p == ',' || isspace((unsigned char)*p)))
          p++;
    } else {
        /* Something wierd, skip it and try to resynch */
        while(*p && !isspace((unsigned char)*p) && *p != ',' && *p != '-')
          p++;
        while(*p && (isspace((unsigned char)*p) || *p == ',' || *p == '-'))
          p++;
    }

    /*-- Now get hours minutes, seconds and ignore tenths --*/
    for(q = p; *q && isdigit((unsigned char)*q); q++);
    if(*q == ':' && d->hour == -1) {
        d->hour = atoi(p);
        while(*p && *p != ':' && !isspace((unsigned char)*p))
          p++;
        if(*p == ':') {
            p++;
            d->minute = atoi(p);
            while(*p && *p != ':' && !isspace((unsigned char)*p))
              p++;
            if(*p == ':') {
                p++;
                d->sec = atoi(p);
                while(*p && !isspace((unsigned char)*p))
                  p++;
            }
        }
    }
    while(*p && isspace((unsigned char)*p))
      p++;


    /*-- The time zone --*/
    d->hours_off_gmt = 0;
    d->min_off_gmt = 0;
    if(*p) {
        if((*p == '+' || *p == '-')
	   && isdigit((unsigned char)p[1])
	   && isdigit((unsigned char)p[2])
	   && isdigit((unsigned char)p[3])
	   && isdigit((unsigned char)p[4])
	   && !isdigit((unsigned char)p[5])) {
            char tmp[3];
            d->min_off_gmt = d->hours_off_gmt = (*p == '+' ? 1 : -1);
            p++;
            tmp[0] = *p++;
            tmp[1] = *p++;
            tmp[2] = '\0';
            d->hours_off_gmt *= atoi(tmp);
            tmp[0] = *p++;
            tmp[1] = *p++;
            tmp[2] = '\0';
            d->min_off_gmt *= atoi(tmp);
        } else {
	    for(n = 0; known_zones[n].zone; n++)
	      if(struncmp(p, known_zones[n].zone, known_zones[n].len) == 0){
		  d->hours_off_gmt = (int) known_zones[n].hour_offset;
		  d->min_off_gmt   = (int) known_zones[n].min_offset;
		  break;
	      }
        }
    }

    if(d->wkday == -1){
	MESSAGECACHE elt;
	struct tm   *tm;
	time_t       t;

	/*
	 * Not sure why we aren't just using this from the gitgo, but
	 * since not sure will just use it to repair wkday.
	 */
	if(mail_parse_date(&elt, (unsigned char *) given_date)){
	    t = mail_longdate(&elt);
	    tm = localtime(&t);

	    if(tm)
	      d->wkday = tm->tm_wday;
	}
    }
}


char *
convert_date_to_local(char *date)
{
    struct tm  *tm;
    time_t      ltime;
    static char datebuf[26];

    ltime = date_to_local_time_t(date);
    if(ltime == (time_t) -1)
      return(date);

    tm = localtime(&ltime);

    if(tm == NULL)
      return(date);

    snprintf(datebuf, sizeof(datebuf), "%.3s, %d %.3s %d %02d:%02d:%02d",
	     day_abbrev(tm->tm_wday), tm->tm_mday, month_abbrev(tm->tm_mon+1),
	     tm->tm_year+1900, tm->tm_hour, tm->tm_min, tm->tm_sec);

    return(datebuf);
}


time_t
date_to_local_time_t(char *date)
{
    time_t      ourtime;
    struct tm   theirtime;
    struct date d;
    static int  zone = 1000000;		/* initialize timezone offset */
    static int  dst;

    if(zone == 1000000){
      int julian;
      struct tm *tm;
      time_t     now;

      zone = 0;
      /* find difference between gmtime and localtime, from c-client do_date */
      now = time((time_t *) 0);
      if(now != (time_t) -1){
	tm = gmtime(&now);
	if(tm != NULL){
	  zone = tm->tm_hour * 60 + tm->tm_min;		/* minutes */
	  julian = tm->tm_yday;

	  tm = localtime(&now);
	  dst = tm->tm_isdst;		/* for converting back to our time */

	  zone = tm->tm_hour * 60 + tm->tm_min - zone;
	  if((julian = tm->tm_yday - julian) != 0)
	    zone += ((julian < 0) == (abs(julian) == 1)) ? -24*60 : 24*60;

	  zone *= 60;		/* change to seconds */
        }
      }
    }

    parse_date(date, &d);

    /* put d into struct tm so we can use mktime */
    memset(&theirtime, 0, sizeof(theirtime));
    theirtime.tm_year = d.year - 1900;
    theirtime.tm_mon = d.month - 1;
    theirtime.tm_mday = d.day;
    theirtime.tm_hour = d.hour - d.hours_off_gmt;
    theirtime.tm_min = d.minute - d.min_off_gmt;
    theirtime.tm_sec = d.sec;

    theirtime.tm_isdst = dst;

    ourtime = mktime(&theirtime);	/* still theirtime, actually */

    /* convert to the time we want to show */
    if(ourtime != (time_t) -1)
      ourtime += zone;

    return(ourtime);
}


/*----------------------------------------------------------------------
     Create a little string of blanks of the specified length.
   Max n is MAX_SCREEN_COLS. Can use up to e repeat_char results at once.
  ----*/
char *
repeat_char(int n, int c)
{
    static char bb[3][MAX_SCREEN_COLS+1];
    static int whichbb = 0;
    char *b;

    whichbb = (whichbb + 1) % 3;
    b = bb[whichbb];

    if(n > sizeof(bb[0]))
       n = sizeof(bb[0]) - 1;

    bb[whichbb][n--] = '\0';
    while(n >= 0)
      bb[whichbb][n--] = c;

    return(bb[whichbb]);
}


/*----------------------------------------------------------------------
   Format number as amount of bytes, appending Kb, Mb, Gb, bytes

  Args: bytes -- number of bytes to format

 Returns pointer to static string. The numbers are divided to produce a 
nice string with precision of about 2-4 digits
    ----*/
char *
byte_string(long int bytes)
{
    char       *a, aa[5];
    char       *abbrevs = "GMK";
    long        i, ones, tenths;
    static char string[10];

    ones   = 0L;
    tenths = 0L;

    if(bytes == 0L){
        strncpy(string, "0 bytes", sizeof(string));
	string[sizeof(string)-1] = '\0';
    }
    else {
        for(a = abbrevs, i = 1000000000; i >= 1; i /= 1000, a++) {
            if(bytes > i) {
                ones = bytes/i;
                if(ones < 10L && i > 10L)
                  tenths = (bytes - (ones * i)) / (i / 10L);
                break;
            }
        }
    
        aa[0] = *a;  aa[1] = '\0'; 
    
        if(tenths == 0)
          snprintf(string, sizeof(string), "%ld%s%s", ones, aa, *a ? "B" : "bytes");
        else
          snprintf(string, sizeof(string), "%ld.%ld%s%s", ones, tenths, aa, *a ? "B" : "bytes");
    }

    return(string);
}



/*----------------------------------------------------------------------
    Print a string corresponding to the number given:
      1st, 2nd, 3rd, 105th, 92342nd....
 ----*/

char *
enth_string(int i)
{
    static char enth[10];

    enth[0] = '\0';

    switch (i % 10) {
        
      case 1:
        if( (i % 100 ) == 11)
          snprintf(enth, sizeof(enth),"%dth", i);
        else
          snprintf(enth, sizeof(enth),"%dst", i);
        break;

      case 2:
        if ((i % 100) == 12)
          snprintf(enth, sizeof(enth), "%dth",i);
        else
          snprintf(enth, sizeof(enth), "%dnd",i);
        break;

      case 3:
        if(( i % 100) == 13)
          snprintf(enth, sizeof(enth), "%dth",i);
        else
          snprintf(enth, sizeof(enth), "%drd",i);
        break;

      default:
        snprintf(enth, sizeof(enth),"%dth",i);
        break;
    }
    return(enth);
}


/*
 * Inserts newlines for folding at whitespace.
 *
 * Args          src -- The source text.
 *             width -- Approximately where the fold should happen.
 *          maxwidth -- Maximum width we want to fold at.
 *      first_indent -- String to use as indent on first line.
 *            indent -- String to use as indent for subsequent folded lines.
 *             flags -- FLD_CRLF   End of line is \r\n instead of \n.
 *                      FLD_PWS    PreserveWhiteSpace when folding. This is
 *                                 for vcard folding where CRLF SPACE is
 *                                 removed when unfolding, so we need to
 *                                 leave the space in. With rfc2822 unfolding
 *                                 only the CRLF is removed when unfolding.
 *
 * Returns   An allocated string which caller should free.
 */
char *
fold(char *src, int width, int maxwidth, char *first_indent, char *indent, unsigned int flags)
{
    char *next_piece, *res, *p;
    int   i, len = 0, starting_point, winner, eol, this_width;
    int   indent1 = 0,		/* width of first_indent */
	  indent2 = 0,		/* width of indent */
	  nbindent2 = 0,	/* number of bytes in indent */
	  nb = 0;		/* number of bytes needed */
    int   cr, preserve_ws;
    char  save_char;
    char *endptr = NULL;
    unsigned shorter, longer;
    unsigned got_width;

    cr          = (flags & FLD_CRLF);
    preserve_ws = (flags & FLD_PWS);

    if(indent){
	indent2 = (int) utf8_width(indent);
	nbindent2 = strlen(indent);
    }

    if(first_indent){
	indent1 = (int) utf8_width(first_indent);
	nb = strlen(first_indent);
    }

    len = indent1;
    next_piece = src;
    eol = cr ? 2 : 1;
    if(!src || !*src)
      nb += eol;

    /*
     * We can't tell how much space is going to be needed without actually
     * passing through the data to see.
     */
    while(next_piece && *next_piece){
	if(next_piece != src && indent2){
	    len += indent2;
	    nb += nbindent2;
	}

	this_width = (int) utf8_width(next_piece);
	if(this_width + len <= width){
	    nb += (strlen(next_piece) + eol);
	    break;
	}
	else{ /* fold it */
	    starting_point = width - len;	/* space left on this line */
	    /* find a good folding spot */
	    winner = -1;
	    for(i = 0;
		winner == -1 && (starting_point - i > 5 || i < maxwidth - width);
		i++){

		if((shorter=starting_point-i) > 5){
		    endptr = utf8_count_forw_width(next_piece, shorter, &got_width);
		    if(endptr && got_width == shorter && isspace((unsigned char) *endptr))
		      winner = (int) shorter;
		}

		if(winner == -1
		   && (longer=starting_point+i) && i < maxwidth - width){
		    endptr = utf8_count_forw_width(next_piece, longer, &got_width);
		    if(endptr && got_width == longer && isspace((unsigned char) *endptr))
		      winner = (int) longer;
		}
	    }

	    if(winner == -1){ /* if no good folding spot, fold at width */
		winner = starting_point;
		endptr = NULL;
	    }
	    
	    if(endptr == NULL){
		endptr = utf8_count_forw_width(next_piece, (unsigned) winner, &got_width);
		winner = (int) got_width;
	    }

	    nb += ((endptr - next_piece) + eol);
	    next_piece = endptr;
	    if(!preserve_ws && isspace((unsigned char) next_piece[0]))
	      next_piece++;
	}

	len = 0;
    }

    res = (char *) fs_get((nb+1) * sizeof(char));
    p = res;
    sstrncpy(&p, first_indent, nb+1-(p-res));
    len = indent1;
    next_piece = src;

    while(next_piece && *next_piece){
	if(next_piece != src && indent2){
	    sstrncpy(&p, indent, nb+1-(p-res));
	    len += indent2;
	}

	this_width = (int) utf8_width(next_piece);
	if(this_width + len <= width){
	    sstrncpy(&p, next_piece, nb+1-(p-res));
	    if(cr && p-res < nb+1)
	      *p++ = '\r';

	    if(p-res < nb+1)
	      *p++ = '\n';

	    break;
	}
	else{ /* fold it */
	    starting_point = width - len;	/* space left on this line */
	    /* find a good folding spot */
	    winner = -1;
	    for(i = 0;
		winner == -1 && (starting_point - i > 5 || i < maxwidth - width);
		i++){

		if((shorter=starting_point-i) > 5){
		    endptr = utf8_count_forw_width(next_piece, shorter, &got_width);
		    if(endptr && got_width == shorter && isspace((unsigned char) *endptr))
		      winner = (int) shorter;
		}

		if(winner == -1
		   && (longer=starting_point+i) && i < maxwidth - width){
		    endptr = utf8_count_forw_width(next_piece, longer, &got_width);
		    if(endptr && got_width == longer && isspace((unsigned char) *endptr))
		      winner = (int) longer;
		}
	    }

	    if(winner == -1){ /* if no good folding spot, fold at width */
		winner = starting_point;
		endptr = NULL;
	    }
	    
	    if(endptr == NULL){
		endptr = utf8_count_forw_width(next_piece, (unsigned) winner, &got_width);
		winner = (int) got_width;
	    }

	    if(endptr){
		save_char = *endptr;
		*endptr = '\0';
		sstrncpy(&p, next_piece, nb+1-(p-res));
		*endptr = save_char;
		next_piece = endptr;
	    }

	    if(cr && p-res < nb+1)
	      *p++ = '\r';

	    if(p-res < nb+1)
	      *p++ = '\n';
	      
	    if(!preserve_ws && isspace((unsigned char) next_piece[0]))
	      next_piece++;
	}

	len = 0;
    }

    if(!src || !*src){
	if(cr && p-res < nb+1)
	  *p++ = '\r';

	if(p-res < nb+1)
	  *p++ = '\n';
    }

    if(p-res < nb+1)
      *p = '\0';

    res[nb] = '\0';

    return(res);
}


/*
 * strsquish - fancifies a string into the given buffer if it's too
 *	       long to fit in the given width
 */
char *
strsquish(char *buf, size_t buflen, char *src, int width)
{
    /*
     * Replace strsquish() with calls to short_str().
     */
    if(width > 14)
      return(short_str(src, buf, buflen, width, MidDots));
    else
      return(short_str(src, buf, buflen, width, FrontDots));
}


char *
long2string(long int l)
{
    static char string[20];

    snprintf(string, sizeof(string), "%ld", l);
    return(string);
}


char *
ulong2string(unsigned long int l)
{
    static char string[20];

    snprintf(string, sizeof(string), "%lu", l);
    return(string);
}


char *
int2string(int i)
{
    static char string[20];

    snprintf(string, sizeof(string), "%d", i);
    return(string);
}


/*
 * strtoval - convert the given string to a positive integer.
 */
char *
strtoval(char *s, int *val, int minmum, int maxmum, int otherok, char *errbuf,
	 size_t errbuflen, char *varname)
{
    int   i = 0, neg = 1;
    char *p = s, *errstr = NULL;

    removing_leading_and_trailing_white_space(p);
    for(; *p; p++)
      if(isdigit((unsigned char) *p)){
	  i = (i * 10) + (*p - '0');
      }
      else if(*p == '-' && i == 0){
	  neg = -1;
      }
      else{
	  snprintf(errstr = errbuf, errbuflen,
		  "Non-numeric value ('%c' in \"%.8s\") in %s. Using \"%d\"",
		  *p, s, varname, *val);
	  return(errbuf);
      }

    i *= neg;

    /* range describes acceptable values */
    if(maxmum > minmum && (i < minmum || i > maxmum) && i != otherok)
      snprintf(errstr = errbuf, errbuflen,
	      "%s of %d not supported (M%s %d). Using \"%d\"",
	      varname, i, (i > maxmum) ? "ax" : "in",
	      (i > maxmum) ? maxmum : minmum, *val);
    /* range describes unacceptable values */
    else if(minmum > maxmum && !(i < maxmum || i > minmum))
      snprintf(errstr = errbuf, errbuflen, "%s of %d not supported. Using \"%d\"",
	      varname, i, *val);
    else
      *val = i;

    return(errstr);
}


/*
 * strtolval - convert the given string to a positive _long_ integer.
 */
char *
strtolval(char *s, long int *val, long int minmum, long int maxmum, long int otherok,
	  char *errbuf, size_t errbuflen, char *varname)
{
    long  i = 0, neg = 1L;
    char *p = s, *errstr = NULL;

    removing_leading_and_trailing_white_space(p);
    for(; *p; p++)
      if(isdigit((unsigned char) *p)){
	  i = (i * 10L) + (*p - '0');
      }
      else if(*p == '-' && i == 0L){
	  neg = -1L;
      }
      else{
	  snprintf(errstr = errbuf, errbuflen,
		  "Non-numeric value ('%c' in \"%.8s\") in %s. Using \"%ld\"",
		  *p, s, varname, *val);
	  return(errbuf);
      }

    i *= neg;

    /* range describes acceptable values */
    if(maxmum > minmum && (i < minmum || i > maxmum) && i != otherok)
      snprintf(errstr = errbuf, errbuflen,
	      "%s of %ld not supported (M%s %ld). Using \"%ld\"",
	      varname, i, (i > maxmum) ? "ax" : "in",
	      (i > maxmum) ? maxmum : minmum, *val);
    /* range describes unacceptable values */
    else if(minmum > maxmum && !(i < maxmum || i > minmum))
      snprintf(errstr = errbuf, errbuflen, "%s of %ld not supported. Using \"%ld\"",
	      varname, i, *val);
    else
      *val = i;

    return(errstr);
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
get_pair(char *string, char **label, char **value, int firstws, int strip_internal_label_quotes)
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


/*
 *  This is sort of the inverse of get_pair.
 *
 * Args --  label -- the first half of the string
 *          value -- the last half of the string
 *
 * Returns -- an allocated string which is "label" SPACE "value"
 *
 *  Label and value are quoted separately. If quoting is needed (they contain
 *  whitespace) then backslash escaping is done inside the quotes for
 *  " and for \. If quoting is not needed, no escaping is done.
 */
char *
put_pair(char *label, char *value)
{
    char *result, *lab = label, *val = value;
    size_t l;

    if(label && *label)
      lab = quote_if_needed(label);

    if(value && *value)
      val = quote_if_needed(value);

    l = strlen(lab) + strlen(val) +1;
    result = (char *) fs_get((l+1) * sizeof(char));
    
    snprintf(result, l+1, "%s%s%s",
	    lab ? lab : "",
	    (lab && lab[0] && val && val[0]) ? " " : "",
	    val ? val : "");

    if(lab && lab != label)
      fs_give((void **)&lab);
    if(val && val != value)
      fs_give((void **)&val);

    return(result);
}


/*
 * This is for put_pair type uses. It returns either an allocated
 * string which is the quoted src string or it returns a pointer to
 * the src string if no quoting is needed.
 */
char *
quote_if_needed(char *src)
{
    char *result = src, *qsrc = NULL;

    if(src && *src){
	/* need quoting? */
	if(strpbrk(src, " \t") != NULL)
	  qsrc = add_escapes(src, "\\\"", '\\', "", "");

	if(qsrc && !*qsrc)
	  fs_give((void **)&qsrc);

	if(qsrc){
	    size_t l;

	    l = strlen(qsrc)+2;
	    result = (char *) fs_get((l+1) * sizeof(char));
	    snprintf(result, l+1, "\"%s\"", qsrc);
	    fs_give((void **)&qsrc);
	}
    }

    return(result);
}


/*
 * Convert a 1, 2, or 3-digit octal string into an 8-bit character.
 * Only the first three characters of s will be used, and it is ok not
 * to null-terminate it.
 */
int
read_octal(char **s)
{
    register int i, j;

    i = 0;
    for(j = 0; j < 3 && **s >= '0' && **s < '8' ; (*s)++, j++)
      i = (i * 8) + (int)(unsigned char)**s - '0';

    return(i);
}


/*
 * Convert two consecutive HEX digits to an integer.  First two
 * chars pointed to by "s" MUST already be tested for hexness.
 */
int
read_hex(char *s)
{
    return(X2C(s));
}


/*
 * Given a character c, put the 3-digit ascii octal value of that char
 * in the 2nd argument, which must be at least 3 in length.
 */
void
char_to_octal_triple(int c, char *octal)
{
    c &= 0xff;

    octal[2] = (c % 8) + '0';
    c /= 8;
    octal[1] = (c % 8) + '0';
    c /= 8;
    octal[0] = c + '0';
}


/*
 * Convert in memory string s to a C-style string, with backslash escapes
 * like they're used in C character constants.
 * Also convert leading spaces because read_pinerc deletes those
 * if not quoted.
 *
 * Returns allocated C string version of s.
 */
char *
string_to_cstring(char *s)
{
    char *b, *p;
    int   n, i, all_space_so_far = 1;

    if(!s)
      return(cpystr(""));

    n = 20;
    b = (char *)fs_get((n+1) * sizeof(char));
    p  = b;
    *p = '\0';
    i  = 0;

    while(*s){
	if(*s != SPACE)
	  all_space_so_far = 0;

	if(i + 4 > n){
	    /*
	     * The output string may overflow the output buffer.
	     * Make more room.
	     */
	    n += 20;
	    fs_resize((void **)&b, (n+1) * sizeof(char));
	    p = &b[i];
	}
	else{
	    switch(*s){
	      case '\n':
		*p++ = '\\';
		*p++ = 'n';
		i += 2;
		break;

	      case '\r':
		*p++ = '\\';
		*p++ = 'r';
		i += 2;
		break;

	      case '\t':
		*p++ = '\\';
		*p++ = 't';
		i += 2;
		break;

	      case '\b':
		*p++ = '\\';
		*p++ = 'b';
		i += 2;
		break;

	      case '\f':
		*p++ = '\\';
		*p++ = 'f';
		i += 2;
		break;

	      case '\\':
		*p++ = '\\';
		*p++ = '\\';
		i += 2;
		break;

	      case SPACE:
		if(all_space_so_far){	/* use octal output */
		    *p++ = '\\';
		    char_to_octal_triple(*s, p);
		    p += 3;
		    i += 4;
		    break;
		}
		else{
		    /* fall through */
		}


	      default:
		if(*s >= SPACE && *s < '~' && *s != '\"' && *s != '$'){
		    *p++ = *s;
		    i++;
		}
		else{  /* use octal output */
		    *p++ = '\\';
		    char_to_octal_triple(*s, p);
		    p += 3;
		    i += 4;
		}

		break;
	    }

	    s++;
	}
    }

    *p = '\0';
    return(b);
}


/*
 * Convert C-style string, with backslash escapes, into a hex string, two
 * hex digits per character.
 *
 * Returns allocated hexstring version of s.
 */
char *
cstring_to_hexstring(char *s)
{
    char *b, *p;
    int   n, i, c;

    if(!s)
      return(cpystr(""));

    n = 20;
    b = (char *)fs_get((n+1) * sizeof(char));
    p  = b;
    *p = '\0';
    i  = 0;

    while(*s){
	if(i + 2 > n){
	    /*
	     * The output string may overflow the output buffer.
	     * Make more room.
	     */
	    n += 20;
	    fs_resize((void **)&b, (n+1) * sizeof(char));
	    p = &b[i];
	}
	else{
	    if(*s == '\\'){
		s++;
		switch(*s){
		  case 'n':
		    c = '\n';
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  case 'r':
		    c = '\r';
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  case 't':
		    c = '\t';
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  case 'v':
		    c = '\v';
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  case 'b':
		    c = '\b';
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  case 'f':
		    c = '\f';
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  case 'a':
		    c = '\007';
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  case '\\':
		    c = '\\';
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  case '?':
		    c = '?';
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  case '\'':
		    c = '\'';
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  case '\"':
		    c = '\"';
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  case 0: /* reached end of s too early */
		    c = 0;
		    C2XPAIR(c, p);
		    i += 2;
		    s++;
		    break;

		  /* hex number */
		  case 'x':
		    s++;
		    if(isxpair(s)){
			c = X2C(s);
			s += 2;
		    }
		    else if(isxdigit((unsigned char)*s)){
			c = XDIGIT2C(*s);
			s++;
		    }
		    else
		      c = 0;

		    C2XPAIR(c, p);
		    i += 2;

		    break;

		  /* octal number */
		  default:
		    c = read_octal(&s);
		    C2XPAIR(c, p);
		    i += 2;

		    break;
		}
	    }
	    else{
		C2XPAIR(*s, p);
		i += 2;
		s++;
	    }
	}
    }

    *p = '\0';
    return(b);
}


/*
 * Convert C-style string, with backslash escapes, into a regular string.
 * Result goes in dst, which should be as big as src.
 *
 */
void
cstring_to_string(char *src, char *dst)
{
    char *p;
    int   c;

    dst[0] = '\0';
    if(!src)
      return;

    p  = dst;

    while(*src){
	if(*src == '\\'){
	    src++;
	    switch(*src){
	      case 'n':
		*p++ = '\n';
		src++;
		break;

	      case 'r':
		*p++ = '\r';
		src++;
		break;

	      case 't':
		*p++ = '\t';
		src++;
		break;

	      case 'v':
		*p++ = '\v';
		src++;
		break;

	      case 'b':
		*p++ = '\b';
		src++;
		break;

	      case 'f':
		*p++ = '\f';
		src++;
		break;

	      case 'a':
		*p++ = '\007';
		src++;
		break;

	      case '\\':
		*p++ = '\\';
		src++;
		break;

	      case '?':
		*p++ = '?';
		src++;
		break;

	      case '\'':
		*p++ = '\'';
		src++;
		break;

	      case '\"':
		*p++ = '\"';
		src++;
		break;

	      case 0: /* reached end of s too early */
		src++;
		break;

	      /* hex number */
	      case 'x':
		src++;
		if(isxpair(src)){
		    c = X2C(src);
		    src += 2;
		}
		else if(isxdigit((unsigned char)*src)){
		    c = XDIGIT2C(*src);
		    src++;
		}
		else
		  c = 0;

		*p++ = c;

		break;

	      /* octal number */
	      default:
		c = read_octal(&src);
		*p++ = c;
		break;
	    }
	}
	else
	  *p++ = *src++;
    }

    *p = '\0';
}


/*
 * Quotes /'s and \'s with \
 *
 * Args: src -- The source string.
 *
 * Returns: A string with backslash quoting added. Any / in the string is
 *          replaced with \/ and any \ is replaced with \\, and any
 *          " is replaced with \".
 *
 *   The caller is responsible for freeing the memory allocated for the answer.
 */
char *
add_backslash_escapes(char *src)
{
    return(add_escapes(src, "/\\\"", '\\', "", ""));
}


/*
 * Undoes backslash quoting of source string.
 *
 * Args: src -- The source string.
 *
 * Returns: A string with backslash quoting removed or NULL. The string starts
 *          at src and goes until the end of src or until a / is reached. The
 *          / is not included in the string. /'s may be quoted by preceding
 *          them with a backslash (\) and \'s may also be quoted by
 *          preceding them with a \. In fact, \ quotes any character.
 *          Not quite, \nnn is octal escape, \xXX is hex escape.
 *
 *   The caller is responsible for freeing the memory allocated for the answer.
 */
char *
remove_backslash_escapes(char *src)
{
    char *ans = NULL, *q, *p;
    int done = 0;

    if(src){
	p = q = (char *)fs_get(strlen(src) + 1);

	while(!done){
	    switch(*src){
	      case '\\':
		src++;
		if(*src){
		    if(isdigit((unsigned char)*src))
		      *p++ = (char)read_octal(&src);
		    else if((*src == 'x' || *src == 'X') &&
			    *(src+1) && *(src+2) && isxpair(src+1)){
			*p++ = (char)read_hex(src+1);
			src += 3;
		    }
		    else
		      *p++ = *src++;
		}

		break;
	    
	      case '\0':
	      case '/':
		done++;
		break;

	      default:
		*p++ = *src++;
		break;
	    }
	}

	*p = '\0';

	ans = cpystr(q);
	fs_give((void **)&q);
    }

    return(ans);
}


/*
 * Quote values for viewer-hdr-colors. We quote backslash, comma, and slash.
 *  Also replaces $ with $$.
 *
 * Args: src -- The source string.
 *
 * Returns: A string with backslash quoting added.
 *
 *   The caller is responsible for freeing the memory allocated for the answer.
 */
char *
add_viewerhdr_escapes(char *src)
{
    char *tmp, *ans = NULL;

    tmp = add_escapes(src, "/\\", '\\', ",", "");

    if(tmp){
	ans = dollar_escape_dollars(tmp);
	fs_give((void **) &tmp);
    }

    return(ans);
}


/*
 * Quote dollar sign by preceding it with another dollar sign. We use $$
 * instead of \$ so that it will work for both PC-Pine and unix.
 *
 * Args: src -- The source string.
 *
 * Returns: A string with $$ quoting added.
 *
 *   The caller is responsible for freeing the memory allocated for the answer.
 */
char *
dollar_escape_dollars(char *src)
{
    return(add_escapes(src, "$", '$', "", ""));
}


/*
 * This adds the quoting for vcard backslash quoting.
 * That is, commas are backslashed, backslashes are backslashed, 
 * semicolons are backslashed, and CRLFs are \n'd.
 * This is thought to be correct for draft-ietf-asid-mime-vcard-06.txt, Apr 98.
 */
char *
vcard_escape(char *src)
{
    char *p, *q;

    q = add_escapes(src, ";,\\", '\\', "", "");
    if(q){
	/* now do CRLF -> \n in place */
	for(p = q; *p != '\0'; p++)
	  if(*p == '\r' && *(p+1) == '\n'){
	      *p++ = '\\';
	      *p = 'n';
	  }
    }

    return(q);
}


/*
 * This undoes the vcard backslash quoting.
 *
 * In particular, it turns \n into newline, \, into ',', \\ into \, \; -> ;.
 * In fact, \<anything_else> is also turned into <anything_else>. The ID
 * isn't clear on this.
 *
 *   The caller is responsible for freeing the memory allocated for the answer.
 */
char *
vcard_unescape(char *src)
{
    char *ans = NULL, *p;
    int done = 0;

    if(src){
	p = ans = (char *)fs_get(strlen(src) + 1);

	while(!done){
	    switch(*src){
	      case '\\':
		src++;
		if(*src == 'n' || *src == 'N'){
		    *p++ = '\n';
		    src++;
		}
		else if(*src)
		  *p++ = *src++;

		break;
	    
	      case '\0':
		done++;
		break;

	      default:
		*p++ = *src++;
		break;
	    }
	}

	*p = '\0';
    }

    return(ans);
}


/*
 * Turn folded lines into long lines in place.
 *
 * CRLF whitespace sequences are removed, the space is not preserved.
 */
void
vcard_unfold(char *string)
{
    char *p = string;

    while(*string)		      /* while something to copy  */
      if(*string == '\r' &&
         *(string+1) == '\n' &&
	 (*(string+2) == SPACE || *(string+2) == TAB))
	string += 3;
      else
	*p++ = *string++;
    
    *p = '\0';
}


/*
 * Quote specified chars with escape char.
 *
 * Args:          src -- The source string.
 *  quote_these_chars -- Array of chars to quote
 *       quoting_char -- The quoting char to be used (e.g., \)
 *    hex_these_chars -- Array of chars to hex escape
 *    hex_these_quoted_chars -- Array of chars to hex escape if they are
 *                              already quoted with quoting_char (that is,
 *                              turn \, into hex comma)
 *
 * Returns: An allocated copy of string with quoting added.
 *   The caller is responsible for freeing the memory allocated for the answer.
 */
char *
add_escapes(char *src, char *quote_these_chars, int quoting_char,
	    char *hex_these_chars, char *hex_these_quoted_chars)
{
    char *ans = NULL;

    if(!quote_these_chars)
      panic("bad arg to add_escapes");

    if(src){
	char *q, *p, *qchar;

	p = q = (char *)fs_get(2*strlen(src) + 1);

	while(*src){
	    if(*src == quoting_char)
	      for(qchar = hex_these_quoted_chars; *qchar != '\0'; qchar++)
		if(*(src+1) == *qchar)
		  break;

	    if(*src == quoting_char && *qchar){
		src++;	/* skip quoting_char */
		*p++ = '\\';
		*p++ = 'x';
		C2XPAIR(*src, p);
		src++;	/* skip quoted char */
	    }
	    else{
		for(qchar = quote_these_chars; *qchar != '\0'; qchar++)
		  if(*src == *qchar)
		    break;

		if(*qchar){		/* *src is a char to be quoted */
		    *p++ = quoting_char;
		    *p++ = *src++;
		}
		else{
		    for(qchar = hex_these_chars; *qchar != '\0'; qchar++)
		      if(*src == *qchar)
			break;

		    if(*qchar){		/* *src is a char to be escaped */
			*p++ = '\\';
			*p++ = 'x';
			C2XPAIR(*src, p);
			src++;
		    }
		    else			/* a regular char */
		      *p++ = *src++;
		}
	    }

	}

	*p = '\0';

	ans = cpystr(q);
	fs_give((void **)&q);
    }

    return(ans);
}


/*
 * Copy a string enclosed in "" without fixing \" or \\. Skip past \"
 * but copy it as is, removing only the enclosing quotes.
 */
char *
copy_quoted_string_asis(char *src)
{
    char *q, *p;
    int   done = 0, quotes = 0;

    if(src){
	p = q = (char *)fs_get(strlen(src) + 1);

	while(!done){
	    switch(*src){
	      case QUOTE:
		if(++quotes == 2)
		  done++;
		else
		  src++;

		break;

	      case BSLASH:	/* don't count \" as a quote, just copy */
		if(*(src+1) == QUOTE){
		    if(quotes == 1){
			*p++ = *src;
			*p++ = *(src+1);
		    }

		    src += 2;
		}
		else{
		    if(quotes == 1)
		      *p++ = *src;
		    
		    src++;
		}

		break;
	    
	      case '\0':
		fs_give((void **)&q);
		return(NULL);

	      default:
		if(quotes == 1)
		  *p++ = *src;
		
		src++;

		break;
	    }
	}

	*p = '\0';
    }

    return(q);
}


/*
 * isxpair -- return true if the first two chars in string are
 *	      hexidecimal characters
 */
int
isxpair(char *s)
{
    return(isxdigit((unsigned char) *s) && isxdigit((unsigned char) *(s+1)));
}





/*
 *  * * * * * *  something to help managing lists of strings   * * * * * * * *
 */


STRLIST_S *
new_strlist(char *name)
{
    STRLIST_S *sp = (STRLIST_S *) fs_get(sizeof(STRLIST_S));
    memset(sp, 0, sizeof(STRLIST_S));
    if(name)
      sp->name = cpystr(name);

    return(sp);
}


STRLIST_S *
copy_strlist(STRLIST_S *src)
{
    STRLIST_S *ret = NULL, *sl, *ss, *new_sl;

    if(src){
	ss = NULL;
	for(sl = src; sl; sl = sl->next){
	    new_sl = (STRLIST_S *) fs_get(sizeof(*new_sl));
	    memset((void *) new_sl, 0, sizeof(*new_sl));
	    if(sl->name)
	      new_sl->name = cpystr(sl->name);

	    if(ss){
		ss->next = new_sl;
		ss = ss->next;
	    }
	    else{
		ret = new_sl;
		ss = ret;
	    }
	}
    }

    return(ret);
}


/*
 * Add the second list to the end of the first.
 */
void
combine_strlists(STRLIST_S **first, STRLIST_S *second)
{
    STRLIST_S *sl;

    if(!second)
      return;

    if(first){
	if(*first){
	    for(sl = *first; sl->next; sl = sl->next)
	      ;

	    sl->next = second;
	}
	else
	  *first = second;
    }
}


void
free_strlist(STRLIST_S **strp)
{
    if(strp && *strp){
	if((*strp)->next)
	  free_strlist(&(*strp)->next);

	if((*strp)->name)
	  fs_give((void **) &(*strp)->name);

	fs_give((void **) strp);
    }
}
