#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: utf8.c 1184 2008-12-16 23:52:15Z hubert@u.washington.edu $";
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


/* includable WITHOUT dependency on c-client */
#include "../../c-client/mail.h"
#include "../../c-client/utf8.h"

#ifdef _WINDOWS
/* wingdi.h uses ERROR (!) and we aren't using the c-client ERROR so... */
#undef ERROR
#endif

#include <system.h>

#include "../../c-client/fs.h"

/* includable WITHOUT dependency on pico */
#include "../../pico/keydefs.h"

#include "../osdep/collate.h"
#include "../filttype.h"

#include "utf8.h"

#include <stdarg.h>


unsigned single_width_chars_a_to_b(UCS *, int, int);


static char locale_charmap[50];

static int   native_utf8;
static void *display_data;

void
init_utf8_display(int utf8, void *rmap)
{
    native_utf8 = utf8;
    display_data = rmap;
}


/*
 * Argument is a UCS-4 wide character.
 * Returns the environment dependent cell width of the
 * character when printed to the screen.
 * This will be -1 if the character is not printable.
 * It will be >= zero if it is printable.
 *
 * Note that in the case it is not printable but it is still sent to
 * Writechar, Writechar will print a '?' with width 1.
 */
int
wcellwidth(UCS ucs)
{
    char dummy[32];
    long w;

    /*
     * We believe that on modern unix systems wchar_t is a UCS-4 character.
     * That's the assumption here.
     */

    if(native_utf8){			/* display is UTF-8 capable */
	w = ucs4_width((unsigned long) ucs);
	return((w & U4W_ERROR) ? -1 : w);
    }
    else if(display_data){
	if(wtomb(dummy, ucs) < 0)
	  return(-1);
	else{
	    w = ucs4_width((unsigned long) ucs);
	    return((w & U4W_ERROR) ? -1 : w);
	}
    }
#ifndef _WINDOWS
    else
      return(wcwidth((wchar_t) ucs));
#else
    return(0);
#endif
}


/*
 * Argument is a UCS-4 wide character.
 * It is converted to the multibyte version (for example UTF8 or EUC-JP).
 * Dest is a buffer at least xx chars wide where the multi-byte version
 * of the wide character will be written.
 * The returned value is the number of bytes written to dest or -1
 * if the conversion can't be done.
 */
int
wtomb(char *dest, UCS ucs)
{
    /*
     * We believe that on modern unix systems wchar_t is a UCS-4 character.
     * That's the assumption here.
     */

    if(native_utf8){
	unsigned char *newdptr;

	newdptr = utf8_put((unsigned char *) dest, (unsigned long) ucs);
	return((newdptr == (unsigned char *) dest) ? -1 : newdptr - (unsigned char *) dest);
    }
    else if(display_data){
	unsigned long ucs4;
	int           ret;

	ucs4 = (unsigned long) ucs;
	ret = ucs4_rmaplen(&ucs4, 1, (unsigned short *) display_data, 0);
	if(ret >= 0)
	  ucs4_rmapbuf((unsigned char *) dest, &ucs4, 1, (unsigned short *) display_data, 0);
	else
	  ret = -1;

	return(ret);
    }
    else
      return(wcrtomb(dest, (wchar_t) ucs, NULL));
}


/*
 * This function does not necessarily update inputp and remaining_octets, so
 * don't rely on that. The c-client version does but the other doesn't.
 */
UCS
mbtow(void *input_cs, unsigned char **inputp, unsigned long *remaining_octets)
{
    UCS ucs;

    if(input_cs){
	CHARSET *cast_input_cs;

	cast_input_cs = (CHARSET *) input_cs;

	switch((ucs = (UCS) ucs4_cs_get(cast_input_cs, inputp, remaining_octets))){
	  case U8G_ENDSTRG:
	  case U8G_ENDSTRI:
	    return(CCONV_NEEDMORE);

	  default:
	    if(ucs & U8G_ERROR || ucs == UBOGON)
	      return(CCONV_BADCHAR);

	    return(ucs);
	}
    }
    else{
	size_t ret;
	wchar_t w;

	/*
	 * Warning:  input_cs and remaining_octets are unused in this
	 * half of the if/else.
	 *
	 * Unfortunately, we can't tell the difference between a source string
	 * that is just not long enough and one that has characters that can't
	 * be converted even though it is long enough. We return NEEDMORE in both cases.
	 */
	ret = mbstowcs(&w, (char *) (*inputp), 1);
	if(ret == (size_t)(-1))
	  return(CCONV_NEEDMORE);
	else{
	  ucs = (UCS) w;
	  return(ucs);
	}
    }
}


void
set_locale_charmap(char *charmap)
{
    if(charmap){
	strncpy(locale_charmap, charmap, sizeof(locale_charmap));
	locale_charmap[sizeof(locale_charmap)-1] = '\0';
    }
    else
      locale_charmap[0] = '\0';
}


/*
 * This ensures that the string is UTF-8. If str is already a UTF-8 string,
 * NULL is returned. Otherwise, an allocated string which is UTF-8 is returned.
 * The caller is responsible for freeing the returned value.
 *
 * Args  str     -- the string to convert
 */
char *
convert_to_utf8(char *str, char *fromcharset, int flags)
{
    char          *ret = NULL;
    char          *fcharset;
    SIZEDTEXT      src, result;
    const CHARSET *cs;
    int            try;

    src.data = (unsigned char *) str;
    src.size = strlen(str);

    /* already UTF-8, return NULL */
    if(!(flags & CU8_NOINFER)
       && (cs = utf8_infercharset(&src))
       && (cs->type == CT_ASCII || cs->type == CT_UTF8))
      return(ret);

    try = 1;
    while(try < 5){
	switch(try){
	  case 1:
	    fcharset = fromcharset;
	    if(fcharset && strucmp("UTF-8", fcharset) != 0)
	      break;	/* give it a try */
	    else
	      try++;	/* fall through */

	  case 2:
	    if(!(flags & CU8_NOINFER)){
		fcharset = cs ? cs->name : NULL;
		if(fcharset && strucmp("UTF-8", fcharset) != 0)
		  break;
		else
		  try++;	/* fall through */
	    }
	    else
	      try++;	/* fall through */

	  case 3:
	    fcharset = locale_charmap;
	    if(fcharset && strucmp("UTF-8", fcharset) != 0)
	      break;
	    else
	      try++;	/* fall through */

	  default:
	    fcharset = "ISO-8859-1";		/* this will "work" */
	    break;
	}

	memset(&result, 0, sizeof(result));

	if(fcharset && utf8_text(&src, fcharset, &result, 0L)){
	    if(!(result.size == src.size && result.data == src.data)){
		ret = (char *) fs_get((result.size+1) * sizeof(char));
		strncpy(ret, (char *) result.data, result.size);
		ret[result.size] = '\0';
	    }
	    /* else no conversion necessary */

	    return(ret);
	}

	try++;
    }

    /* won't make it to here */
    return(ret);
}


/*
 * Convert from UTF-8 to user's locale charset.
 * This actually uses the wtomb routine to do the conversion, and that
 * relies on setup_for_input_output having been called.
 * If no conversion is necessary, NULL is returned, otherwise an allocated
 * string in the locale charset is returned and the caller is responsible
 * for freeing it.
 */
char *
convert_to_locale(char *utf8str)
{
#define CHNK 500
    char *inp, *retp, *ret = NULL;
    CBUF_S cb;
    int r, alloced;

    if(native_utf8 || !utf8str || !utf8str[0])
      return(NULL);

    cb.cbuf[0] = '\0';
    cb.cbufp = cb.cbufend = cb.cbuf;
    inp = utf8str;

    alloced = CHNK;
    ret = (char *) fs_get(alloced * sizeof(char));
    retp = ret;

    /*
     * There's gotta be a better way to do this but utf8_to_locale was
     * available and everything looks like a nail when all you have
     * is a hammer.
     */
    while(*inp){
	/*
	 * We're placing the outgoing stream of characters in ret, a multi-byte
	 * array of characters in the user's locale charset. See if there is
	 * enough room for the next wide characters worth of output chars
	 * and allocate more space if not.
	 */
	if((alloced - (retp-ret)) < MAX(MB_LEN_MAX,32)){
	    alloced += CHNK;
	    fs_resize((void **) &ret, alloced * sizeof(char));
	}

	r = utf8_to_locale((int) *inp++, &cb,
			   (unsigned char *) retp, alloced-(retp-ret));

	retp += r;
    }

    *retp = '\0';

    fs_resize((void **) &ret, strlen(ret)+1);

    return(ret);
}


/*
 * Pass in a stream of UTF-8 characters in 'c' and return obuf
 * filled in with multi-byte characters. The return value is the
 * number of valid characters in obuf to be used.
 */
int
utf8_to_locale(int c, CBUF_S *cb, unsigned char obuf[], size_t obuf_size)
{
    int outchars = 0;
    
    if(!(cb && cb->cbufp))
      return(0);

    if(cb->cbufp < cb->cbuf+sizeof(cb->cbuf)){
	unsigned char *inputp;
	unsigned long remaining_octets;
	UCS ucs;

	*(cb->cbufp)++ = (unsigned char) c;
	inputp = cb->cbuf;
	remaining_octets = (cb->cbufp - cb->cbuf) * sizeof(unsigned char);
	ucs = (UCS) utf8_get(&inputp, &remaining_octets);

	switch(ucs){
	  case U8G_ENDSTRG:	/* incomplete character, wait */
	  case U8G_ENDSTRI:	/* incomplete character, wait */
	    break;

	  default:
	    if(ucs & U8G_ERROR || ucs == UBOGON){
		/*
		 * None of these cases is supposed to happen. If it
		 * does happen then the input stream isn't UTF-8
		 * so something is wrong. Treat each character in the
		 * input buffer as a separate error character and
		 * print a '?' for each.
		 */
		for(inputp = cb->cbuf; inputp < cb->cbufp; inputp++)
		  obuf[outchars++] = '?';

		cb->cbufp = cb->cbuf;
	    }
	    else{
		if(ucs >= 0x80 && wcellwidth(ucs) < 0){
		    /*
		     * This happens when we have a UTF-8 character that
		     * we aren't able to print in our locale. For example,
		     * if the locale is setup with the terminal
		     * expecting ISO-8859-1 characters then there are
		     * lots of UTF-8 characters that can't be printed.
		     * Print a '?' instead.
		     */
		    obuf[outchars++] = '?';
		}
		else{
		    /*
		     * Convert the ucs into the multibyte
		     * character that corresponds to the
		     * ucs in the users locale.
		     */
		    outchars = wtomb((char *) obuf, ucs);
		    if(outchars < 0){
			obuf[0] = '?';
			outchars = 1;
		    }
		}

		/* update the input buffer */
		if(inputp >= cb->cbufp)	/* this should be the case */
		  cb->cbufp = cb->cbuf;
		else{		/* extra chars for some reason? */
		    unsigned char *q, *newcbufp;

		    newcbufp = (cb->cbufp - inputp) + cb->cbuf;
		    q = cb->cbuf;
		    while(inputp < cb->cbufp)
		      *q++ = *inputp++;

		    cb->cbufp = newcbufp;
		}
	    }

	    break;
	}
    }
    else{			/* error */
	obuf[0] = '?';
	outchars = 1;
	cb->cbufp = cb->cbuf;	/* start over */
    }

    return(outchars);
}


/*
 * Returns the screen cells width of the UCS-4 string argument.
 * The source string is zero terminated.
 */
unsigned
ucs4_str_width(UCS *ucsstr)
{
    unsigned width = 0;
    int w;

    if(ucsstr)
      while(*ucsstr){
	w = wcellwidth(*ucsstr++);
	if(w != U4W_CTLSRGT)
	  width += (w < 0 ? 1 : w);
      }

    return width;
}


/*
 * Returns the screen cells width of the UCS-4 string argument
 * from ucsstr[a] through (inclusive) ucsstr[b].
 * No checking is done to make sure a starts in the middle
 * of a UCS-4 array.
 */
unsigned
ucs4_str_width_a_to_b(UCS *ucsstr, int a, int b)
{
    unsigned width = 0;
    int i, w;

    if(ucsstr)
      for(i = a; i <= b && ucsstr[i]; i++){
	w = wcellwidth(ucsstr[i]);
	if(w != U4W_CTLSRGT)
	  width += (w < 0 ? 1 : w);
      }

    return width;
}


/*
 * Returns the screen cells width of the UCS-4 string argument
 * from ustart through (exclusive) uend.
 * No checking is done to make sure it starts in the middle
 * of a UCS-4 array.
 */
unsigned
ucs4_str_width_ptr_to_ptr(UCS *ustart, UCS *uend)
{
    UCS *u;
    unsigned width = 0;
    int w;

    if(!ustart)
      return width;

    if(ustart)
      for(u = ustart; u < uend; u++){
	w = wcellwidth(*u);
	if(w != U4W_CTLSRGT)
	  width += (w < 0 ? 1 : w);
      }

    return(width);
}


/*
 * Return the largest possible pointer into ucs4str so that the width
 * of the string from ucs4str to the pointer (exclusive)
 * is maxwidth or less. Also stops at a null character.
 */
UCS *
ucs4_particular_width(UCS *ucs4str, int maxwidth)
{
    UCS *u;
    int w_consumed = 0, w, done = 0;

    u = ucs4str;

    if(u)
      while(!done && *u && w_consumed <= maxwidth){
	w = wcellwidth(*u);
	w = (w >= 0 ? w : 1);
	if(w_consumed + w <= maxwidth){
	    w_consumed += w;
	    ++u;
	}
	else
	  ++done;
      }

    return(u);
}


/*
 * Convert and copy a UTF-8 string into a UCS-4 NULL
 * terminated array. Just like cpystr only it converts
 * from UTF-8 to UCS-4.
 *
 * Returned UCS-4 string needs to be freed by caller.
 */
UCS *
utf8_to_ucs4_cpystr(char *utf8src)
{
    size_t         retsize;
    UCS           *ret = NULL;
    UCS            ucs;
    unsigned long  remaining_octets;
    unsigned char *readptr;
    size_t         arrayindex;

    /*
     * We don't know how big to allocate the return array
     * because variable numbers of octets in the src array
     * will combine to make UCS-4 characters. The number of
     * UCS-4 characters is less than or equal to the number
     * of src characters, though.
     */

    if(!utf8src)
      return NULL;

    retsize = strlen(utf8src) + 1;

    ret = (UCS *) fs_get(retsize * sizeof(*ret));
    memset(ret, 0, retsize * sizeof(*ret));

    readptr = (unsigned char *) utf8src;
    remaining_octets = retsize-1;
    arrayindex = 0;

    while(remaining_octets > 0 && *readptr && arrayindex < retsize-1){
	ucs = (UCS) utf8_get(&readptr, &remaining_octets);

	if(ucs & U8G_ERROR || ucs == UBOGON)
	  remaining_octets = 0;
	else
	  ret[arrayindex++] = ucs;
    }

    ret[arrayindex] = '\0';

    /* get rid of excess size */
    if(arrayindex+1 < retsize)
      fs_resize((void **) &ret, (arrayindex + 1) * sizeof(*ret));

    return ret;
}


/*
 * Convert and copy a UCS-4 zero-terminated array into a UTF-8 NULL
 * terminated string. Just like cpystr only it converts
 * from UCS-4 to UTF-8.
 *
 * Returned UTF-8 string needs to be freed by caller.
 */
char *
ucs4_to_utf8_cpystr(UCS *ucs4src)
{
    unsigned char *ret = NULL;
    unsigned char *writeptr;
    int            i;

    if(!ucs4src)
      return NULL;

    /*
     * Over-allocate and then resize at the end.
     */

    /* count characters in source */
    for(i = 0; ucs4src[i]; i++)
      ;

    ret = (unsigned char *) fs_get((6*i + 1) * sizeof(*ret));
    memset(ret, 0, (6*i + 1) * sizeof(*ret));

    writeptr = ret;
    for(i = 0; ucs4src[i]; i++)
      writeptr = utf8_put(writeptr, (unsigned long) ucs4src[i]);

    /* get rid of excess size */
    fs_resize((void **) &ret, (writeptr - ret + 1) * sizeof(*ret));

    return ((char *) ret);
}


/*
 * Similar to above but copy a fixed number of source
 * characters instead of going until null terminator.
 */
char *
ucs4_to_utf8_cpystr_n(UCS *ucs4src, int ucs4src_len)
{
    unsigned char *ret = NULL;
    unsigned char *writeptr;
    int            i;

    if(!ucs4src)
      return NULL;

    /*
     * Over-allocate and then resize at the end.
     */

    ret = (unsigned char *) fs_get((6*ucs4src_len + 1) * sizeof(*ret));
    memset(ret, 0, (6*ucs4src_len + 1) * sizeof(*ret));

    writeptr = ret;
    for(i = 0; i < ucs4src_len; i++)
      writeptr = utf8_put(writeptr, (unsigned long) ucs4src[i]);

    /* get rid of excess size */
    fs_resize((void **) &ret, (writeptr - ret + 1) * sizeof(*ret));

    return ((char *) ret);
}


#ifdef _WINDOWS
/*
 * Convert a UTF-8 argument into an LPTSTR version
 * of that argument. The result is allocated here
 * and should be freed by the caller.
 */
LPTSTR
utf8_to_lptstr(LPSTR arg_utf8)
{
     int lptstr_len;
     LPTSTR lptstr_ret = NULL;

     lptstr_len = MultiByteToWideChar( CP_UTF8, 0, arg_utf8, -1, NULL, 0 );
     if(lptstr_len > 0)
     {
         lptstr_ret = (LPTSTR)fs_get(lptstr_len * sizeof(TCHAR));
         lptstr_len = MultiByteToWideChar( CP_UTF8, 0,
             arg_utf8, -1, lptstr_ret, lptstr_len );
     }

     if(!lptstr_len)
     {
         /* check GetLastError()? */
         lptstr_ret = (LPTSTR)fs_get(sizeof(TCHAR));
         lptstr_ret[0] = 0;
     }

     return lptstr_ret;
}


/*
 * Convert an LPTSTR argument into a UTF-8 version
 * of that argument. The result is allocated here
 * and should be freed by the caller.
 */
LPSTR
lptstr_to_utf8(LPTSTR arg_lptstr)
{
     int utf8str_len;
     LPSTR utf8str_ret = NULL;

     utf8str_len = WideCharToMultiByte( CP_UTF8, 0, arg_lptstr, -1, NULL, 0, NULL, NULL );
     if(utf8str_len > 0)
     {
         utf8str_ret = (LPSTR)fs_get(utf8str_len * sizeof(CHAR));
         utf8str_len = WideCharToMultiByte( CP_UTF8, 0,
             arg_lptstr, -1, utf8str_ret, utf8str_len, NULL, NULL );
     }

     if(!utf8str_len)
     {
         /* check GetLastError()? */
         utf8str_ret = (LPSTR)fs_get(sizeof(CHAR));
         utf8str_ret[0] = 0;
     }

     return utf8str_ret;
}


/*
 * Convert a UCS4 argument into an LPTSTR version
 * of that argument. The result is allocated here
 * and should be freed by the caller.
 */
LPTSTR
ucs4_to_lptstr(UCS *arg_ucs4)
{
    LPTSTR ret_lptstr = NULL;
    size_t len;
    size_t i;

    if(arg_ucs4){
	len = ucs4_strlen(arg_ucs4);
	ret_lptstr = (LPTSTR) fs_get((len+1) * sizeof(TCHAR));
	/* bogus conversion ignores UTF-16 */
	for(i = 0; i < len; i++)
	  ret_lptstr[i] = arg_ucs4[i];

	ret_lptstr[len] = '\0';
    }

    return(ret_lptstr);
}


/*
 * Convert an LPTSTR argument into a UCS4 version
 * of that argument. The result is MemAlloc'd here
 * and should be freed by the caller.
 */
UCS *
lptstr_to_ucs4(LPTSTR arg_lptstr)
{
    UCS *ret_ucs4 = NULL;
    size_t len;
    size_t i;

    if(arg_lptstr){
	len = _tcslen(arg_lptstr);
	ret_ucs4 = (UCS *) fs_get((len+1)*sizeof(UCS));
	/* bogus conversion ignores UTF-16 */
	for(i = 0; i < len; i++)
	  ret_ucs4[i] = arg_lptstr[i];

	ret_ucs4[len] = '\0';
    }

    return(ret_ucs4);
}

#endif /* _WINDOWS */


/*
 * Pass in a stream of UTF-8 characters 1-at-a-time in 'c' and return obuf
 * 1-at-a-time filled in with UCS characters. The return value is the
 * number of valid characters in obuf to be used. It can only
 * be 1 or 0 characters since we're only getting one UTF-8 character
 * at a time.
 */
int
utf8_to_ucs4_oneatatime(int c, CBUF_S *cb, UCS *obuf, int *obufwidth)
{
    int  width = 0, outchars = 0;
    
    if(!(cb && cb->cbufp))
      return(0);

    if(cb->cbufp < cb->cbuf+sizeof(cb->cbuf)){
	unsigned char *inputp;
	unsigned long remaining_octets;
	UCS ucs;

	*cb->cbufp++ = (unsigned char) c;
	inputp = cb->cbuf;
	remaining_octets = (cb->cbufp - cb->cbuf) * sizeof(unsigned char);
	ucs = (UCS) utf8_get(&inputp, &remaining_octets);

	switch(ucs){
	  case U8G_ENDSTRG:	/* incomplete character, wait */
	  case U8G_ENDSTRI:	/* incomplete character, wait */
	    break;

	  default:
	    if(ucs & U8G_ERROR || ucs == UBOGON){
		/*
		 * None of these cases is supposed to happen. If it
		 * does happen then the input stream isn't UTF-8
		 * so something is wrong.
		 */
		outchars++;
		*obuf = '?';
		cb->cbufp = cb->cbuf;
		width = 1;
	    }
	    else{
		outchars++;
		if(ucs < 0x80 && ucs >= 0x20)
		  width = 1;

		if(ucs >= 0x80 && (width=wcellwidth(ucs)) < 0){
		    /*
		     * This happens when we have a UTF-8 character that
		     * we aren't able to print in our locale. For example,
		     * if the locale is setup with the terminal
		     * expecting ISO-8859-1 characters then there are
		     * lots of UTF-8 characters that can't be printed.
		     * Print a '?' instead.
		     * Don't think this should happen in Windows.
		     */
		    *obuf = '?';
		}
		else{
		    *obuf = ucs;
		}

		/* update the input buffer */
		if(inputp >= cb->cbufp)	/* this should be the case */
		  cb->cbufp = cb->cbuf;
		else{		/* extra chars for some reason? */
		    unsigned char *q, *newcbufp;

		    newcbufp = (cb->cbufp - inputp) + cb->cbuf;
		    q = cb->cbuf;
		    while(inputp < cb->cbufp)
		      *q++ = *inputp++;

		    cb->cbufp = newcbufp;
		}
	    }

	    break;
	}
    }
    else{			/* error */
	*obuf = '?';
	outchars = 1;
	width = 1;
	cb->cbufp = cb->cbuf;	/* start over */
    }

    if(obufwidth)
      *obufwidth = width;

    return(outchars);
}


/*
 * Return an allocated copy of a zero-terminated UCS-4 string.
 */
UCS *
ucs4_cpystr(UCS *ucs4src)
{
    size_t         arraysize;
    UCS           *ret = NULL;
    size_t         i;

    if(!ucs4src)
      return NULL;

    arraysize = ucs4_strlen(ucs4src);

    ret = (UCS *) fs_get((arraysize+1) * sizeof(*ret));
    memset(ret, 0, (arraysize+1) * sizeof(*ret));

    for(i = 0; i < arraysize; i++)
      ret[i] = ucs4src[i];

    return ret;
}


UCS *
ucs4_strncpy(UCS *ucs4dst, UCS *ucs4src, size_t n)
{
    size_t i;

    if(ucs4src && ucs4dst){
	for(i = 0; i < n; i++){
	    ucs4dst[i] = ucs4src[i];
	    if(ucs4dst[i] == '\0')
	      break;
	}
    }

    return ucs4dst;
}


UCS *
ucs4_strncat(UCS *ucs4dst, UCS *ucs4src, size_t n)
{
    size_t i;
    UCS *u;

    if(ucs4src && ucs4dst){
	for(u = ucs4dst; *u; u++)
	  ;

	for(i = 0; i < n; i++){
	    u[i] = ucs4src[i];
	    if(u[i] == '\0')
	      break;
	}

	if(i == n)
	  u[i] = '\0';
    }

    return ucs4dst;
}


/*
 * Like strlen only this returns the number of non-zero characters
 * in a zero-terminated UCS-4 array.
 */
size_t
ucs4_strlen(UCS *ucs4str)
{
    size_t i = 0;

    if(ucs4str)
      while(ucs4str[i])
        i++;

    return(i);
}


int
ucs4_strcmp(UCS *s1, UCS *s2)
{
    for(; *s1 == *s2; s1++, s2++)
      if(*s1 == '\0')
        return 0;

    return((*s1 < *s2) ? -1 : 1);
}


UCS *
ucs4_strchr(UCS *s, UCS c)
{
    if(!s)
      return NULL;

    while(*s && *s != c)
      s++;

    if(*s || !c)
      return s;
    else
      return NULL;
}


UCS *
ucs4_strrchr(UCS *s, UCS c)
{
    UCS *ret = NULL;

    if(!s)
      return ret;

    while(*s){
	if(*s == c)
	  ret = s;

	s++;
    }

    return ret;
}


/*
 * Returns the screen cells width of the UTF-8 string argument.
 */
unsigned
utf8_width(char *str)
{
    unsigned width = 0;
    int this_width;
    UCS ucs;
    unsigned long remaining_octets;
    char *readptr;

    if(!(str && *str))
      return(width);

    readptr = str;
    remaining_octets = readptr ? strlen(readptr) : 0;

    while(remaining_octets > 0 && *readptr){

	ucs = (UCS) utf8_get((unsigned char **) &readptr, &remaining_octets);

	if(ucs & U8G_ERROR || ucs == UBOGON){
	    /*
	     * This should not happen, but do something to handle it anyway.
	     * Treat each character as a single width character, which is what should
	     * probably happen when we actually go to write it out.
	     */
	    remaining_octets--;
	    readptr++;
	    this_width = 1;
	}
	else{
	    this_width = wcellwidth(ucs);

	    /*
	     * If this_width is -1 that means we can't print this character
	     * with our current locale. Writechar will print a '?'.
	     */
	    if(this_width < 0)
	      this_width = 1;
	}

	width += (unsigned) this_width;
    }

    return(width);
}


/*
 * Copy UTF-8 characters from src into dst.
 * This is intended to be used if you want to truncate a string at
 * the start instead of the end. For example, you have a long string
 * like
 *       this_is_a_long_string
 * but not enough space to fit it into a particular field. You want to
 * end up with
 *             s_a_long_string
 * where that fits in a particular width. Perhaps you'd use this with ...
 * to get
 *          ...s_a_long_string
 * This right adjusts the end of the string in the width space and
 * cuts it off at the start. If there is enough width for the whole
 * string it will copy the string into dst with no padding.
 *
 * Copy enough characters so that the result will have screen width of
 * want_width screen cells in current locale.
 *
 * Dstlen is the available space in dst. No more than dstlen bytes will be written
 *   to dst. This is just for protection, it shouldn't be relied on to
 *   do anything useful. Dstlen should be large enough. Otherwise you'll get
 *   characters truncated in the middle or something like that.
 *
 * Returned value is the number of bytes written to dst, not including
 *   the possible terminating null.
 *
 * If we can't hit want_width exactly because of double width characters
 *   then we will pad the end of the string with space in order to make
 *   the width exact.
 */
size_t
utf8_to_width_rhs(char *dst,		/* destination buffer */
		  char *src,		/* source string */
		  size_t dstlen,	/* space in dest */
		  unsigned want_width)	/* desired screen width */
{
    int this_width;
    unsigned width_consumed = 0;
    UCS ucs;
    unsigned long remaining_octets;
    char *readptr, *goodreadptr, *savereadptr, *endptr;
    size_t nb = 0;

    if(!src){
	if(dstlen > 0)
	  dst[0] = '\0';

	return nb;
    }

    /*
     * Start at the end of the source string and go backwards until we
     * get to the desired width, but not more than the width.
     */
    readptr = src + strlen(src);
    endptr = readptr;
    goodreadptr = readptr;
    width_consumed = 0;
    savereadptr = readptr;

    for(readptr = savereadptr-1; readptr >= src && width_consumed < want_width && (endptr - readptr) < dstlen;
	readptr = savereadptr-1){

	savereadptr = readptr;
	remaining_octets = goodreadptr - readptr;
	ucs = (UCS) utf8_get((unsigned char **) &readptr, &remaining_octets);

	/*
	 * Handling the error case is tough because an error will be the normal thing that
	 * happens as we back through the string. So we're just going to punt on the
	 * error for now.
	 */
	if(!(ucs & U8G_ERROR || ucs == UBOGON)){
	    if(remaining_octets > 0){
		/*
		 * This means there are some bad octets after this good
		 * character so things are not going to work out well.
		 * Bail out.
		 */
		savereadptr = src;	/* we're done */
	    }
	    else{
		this_width = wcellwidth(ucs);

		if(this_width < 0)
		  this_width = 1;

		if(width_consumed + (unsigned) this_width <= want_width){  /* ok */
		    width_consumed += (unsigned) this_width;
		    goodreadptr = savereadptr;
		}
		else
		  savereadptr = src;	/* we're done */
	    }
	}
    }

    /*
     * Copy characters from goodreadptr to endptr into dst.
     */
    nb = MIN(endptr-goodreadptr, dstlen-1);
    strncpy(dst, goodreadptr, nb);
    dst[nb] = '\0';

    /*
     * Pad out with spaces in order to hit width exactly.
     */
    while(width_consumed < want_width && nb < dstlen-1){
	dst[nb++] = ' ';
	dst[nb] = '\0';
	width_consumed++;
    }

    return nb;
}


/*
 * The arguments being converted are UTF-8 strings.
 * This routine attempts to make it possible to use screen cell
 * widths in a format specifier. In a one-byte per screen cell
 * world we might have used %10.10s to cause a string to occupy
 * 10 screen positions. Since the width and precision are really
 * referring to numbers of bytes instead of screen positions that
 * won't work with UTF-8 input. We emulate that behavior with
 * the format string %w. %m.nw means to use the m and n as
 * screen width indicators instead of bytes indicators.
 *
 * There is no reason to use this routine unless you want to use
 * min field with or precision with the specifier. A plain %w without
 * widths is equivalent exactly to a plain %s in a regular printf.
 *
 * Double-width characters complicate things. It may not be possible
 * to satisfy the request exactly. For example, %3w for an input
 * string that is made up of two double-width characters.
 * This routine will arbitrarily use a trailing space character if
 * needed to make the width come out correctly where a half of a
 * double-width character would have been needed. We'll see how
 * that works for us.
 *
 * %w only works for strings (it's a %s replacement).
 *
 * Buffer overflow is handled by the size argument. %.30s will work
 * to limit a particular string to 30 bytes, but you lose that
 * ability with %w, since it may write more than precision bytes
 * in order to get to the desired width. It is best to choose
 * size large enough so that it doesn't come into play, otherwise
 * it may be possible to get partial UTF-8 characters because of
 * the truncation.
 *
 * The return value isn't quite the same as the return value
 * of snprintf. It is the number of bytes written, not counting
 * the trailing null, just like snprintf. However, if it is
 * truncated due to size then the output is size, not the
 * number of characters that would have been written.
 */
int
utf8_snprintf(char *dest, size_t size, char *fmt, ...)
{
    char    newfmt[100], buf[20], *q, *pdest, *width_str, *end;
    char   *start_of_specifier;
    char   *input_str;
    int     int_arg;
    double  double_arg;
    void   *ptr_arg;
    unsigned got_width;
    int     more_flags, ret, w;
    int     min_field_width, field_precision, modifier;
    int     flags_minus, flags_plus, flags_space, flags_zero, flags_pound;
    va_list args;

    newfmt[0] = '\0';
    q = newfmt;

    pdest = dest;

#define IS_ROOM_IN_DEST(n_more_chars)			\
    ((pdest - dest + (n_more_chars) <= size) ? 1 : 0)

    /*
     * Strategy: Look through the fmt string for %w's. Replace the
     * %w's in the format string with %s's but with possibly different
     * width and precision arguments which will make it come out right.
     * Then call the regular system vsnprintf with the altered format
     * string but same arguments.
     *
     * That would be nice but it doesn't quite work. Why? Because a
     * %*w will need to have the value in the integer argument the *
     * refers to modified. Can't do it as far as I can tell. Or we could
     * remove the integer argument somehow before calling printf. Can't
     * do it. Or we could somehow add an additional conversion specifier
     * that caused nothing to be printed but ate up the integer arg.
     * Can't figure out how to do that either.
     *
     * Since we can't figure out how to do it, the alternative is to
     * construct the result one piece at a time, pasting together the
     * pieces from the different conversions.
     */
    va_start(args, fmt);

    while(*fmt && IS_ROOM_IN_DEST(1)){
	if(*fmt == '%'){
	    start_of_specifier = fmt++;

	    min_field_width = field_precision = -1;
	    flags_minus = flags_plus = flags_space = flags_zero = flags_pound = 0;

	    /* flags */
	    more_flags = 1;
	    while(more_flags){
		switch(*fmt){
		  case '-':
		    flags_minus++;
		    fmt++;
		    break;

		  case '+':
		    flags_plus++;
		    fmt++;
		    break;

		  case ' ':
		    flags_space++;
		    fmt++;
		    break;

		  case '0':
		    flags_zero++;
		    fmt++;
		    break;

		  case '#':
		    flags_pound++;
		    fmt++;
		    break;
		  
		  default:
		    more_flags = 0;
		    break;
		}
	    }

	    /* minimum field width */
	    if(*fmt == '*'){
		min_field_width = va_arg(args, int);
		fmt++;
	    }
	    else if(*fmt >= '0' && *fmt <= '9'){
		width_str = fmt;
		while (*fmt >= '0' && *fmt <= '9')
		  fmt++;

		strncpy(buf, width_str, MIN(fmt-width_str,sizeof(buf)));
		if(sizeof(buf) > fmt-width_str)
		  buf[fmt-width_str] = '\0';

		buf[sizeof(buf)-1] = '\0';

		min_field_width = atoi(width_str);
	    }

	    /* field precision */
	    if(*fmt == '.'){
		fmt++;
		if(*fmt == '*'){
		    field_precision = va_arg(args, int);
		    fmt++;
		}
		else if(*fmt >= '0' && *fmt <= '9'){
		    width_str = fmt;
		    while (*fmt >= '0' && *fmt <= '9')
		      fmt++;

		    strncpy(buf, width_str, MIN(fmt-width_str,sizeof(buf)));
		    if(sizeof(buf) > fmt-width_str)
		      buf[fmt-width_str] = '\0';

		    buf[sizeof(buf)-1] = '\0';

		    field_precision = atoi(width_str);
		}
	    }

	    /* length modifier */
	    if(*fmt == 'h' || *fmt == 'l' || *fmt == 'L')
	      modifier = *fmt++;

	    /* conversion character */
	    switch(*fmt){
	      case 'w':
		/*
		 * work with va_arg(char *) to figure out width
		 * and precision needed to produce the screen width
		 * and precision asked for in %w using some of the
		 * utf8 width routines we have.
		 */

		input_str = va_arg(args, char *);
		if(field_precision >=0 || min_field_width >= 0)
		  w = utf8_width(input_str);

		if(field_precision >= 0){
		    if(w <= field_precision)
		      field_precision = -1;  /* print it all */
		    else{
			/*
			 * We need to cut off some of the input_str
			 * in this case.
			 */
			end = utf8_count_forw_width(input_str, field_precision, &got_width);
			field_precision = (int) (end - input_str);
			/* new w with this field_precision */
			w = got_width;
		    }
		}

		/* need some padding */
		if(min_field_width >= 0)
		  min_field_width = ((field_precision >= 0) ? field_precision : strlen(input_str)) +
				      MAX(0, min_field_width - w);

		/*
		 * Now we just need to get the new format string
		 * set correctly in newfmt.
		 */
		q = newfmt;
		if(q-newfmt < sizeof(newfmt))
		  *q++ = '%';

		if(flags_minus && q-newfmt < sizeof(newfmt))
		  *q++ = '-';
		if(flags_plus && q-newfmt < sizeof(newfmt))
		  *q++ = '+';
		if(flags_space && q-newfmt < sizeof(newfmt))
		  *q++ = ' ';
		if(flags_zero && q-newfmt < sizeof(newfmt))
		  *q++ = '0';
		if(flags_pound && q-newfmt < sizeof(newfmt))
		  *q++ = '#';

		if(min_field_width >= 0){
		    snprintf(buf, sizeof(buf), "%d", min_field_width);
		    sstrncpy(&q, buf, sizeof(newfmt)-(q-newfmt));
		}

		if(field_precision >= 0){
		    if(q-newfmt < sizeof(newfmt))
		      *q++ = '.';

		    snprintf(buf, sizeof(buf), "%d", field_precision);
		    sstrncpy(&q, buf, sizeof(newfmt)-(q-newfmt));
		}

		if(q-newfmt < sizeof(newfmt))
		  *q++ = 's';

		if(q-newfmt < sizeof(newfmt))
		  *q++ = '\0';

		snprintf(pdest, size - (pdest-dest), newfmt, input_str);
		pdest += strlen(pdest);

	        break;

	      case '\0':
		fmt--;
	        break;

	      default:
		/* make a new format which leaves out the dynamic '*' arguments */
		q = newfmt;
		if(q-newfmt < sizeof(newfmt))
		  *q++ = '%';

		if(flags_minus && q-newfmt < sizeof(newfmt))
		  *q++ = '-';
		if(flags_plus && q-newfmt < sizeof(newfmt))
		  *q++ = '+';
		if(flags_space && q-newfmt < sizeof(newfmt))
		  *q++ = ' ';
		if(flags_zero && q-newfmt < sizeof(newfmt))
		  *q++ = '0';
		if(flags_pound && q-newfmt < sizeof(newfmt))
		  *q++ = '#';

		if(min_field_width >= 0){
		    snprintf(buf, sizeof(buf), "%d", min_field_width);
		    sstrncpy(&q, buf, sizeof(newfmt)-(q-newfmt));
		}

		if(field_precision >= 0){
		    if(q-newfmt < sizeof(newfmt))
		      *q++ = '.';

		    snprintf(buf, sizeof(buf), "%d", field_precision);
		    sstrncpy(&q, buf, sizeof(newfmt)-(q-newfmt));
		}

		if(q-newfmt < sizeof(newfmt))
		  *q++ = *fmt;

		if(q-newfmt < sizeof(newfmt))
		  *q++ = '\0';

		switch(*fmt){
		  case 'd': case 'i': case 'o':
		  case 'x': case 'X': case 'u': case 'c':
		    int_arg = va_arg(args, int);
		    snprintf(pdest, size - (pdest-dest), newfmt, int_arg);
		    pdest += strlen(pdest);
		    break;

		  case 's':
		    input_str = va_arg(args, char *);
		    snprintf(pdest, size - (pdest-dest), newfmt, input_str);
		    pdest += strlen(pdest);
		    break;

		  case 'f': case 'e': case 'E':
		  case 'g': case 'G':
		    double_arg = va_arg(args, double);
		    snprintf(pdest, size - (pdest-dest), newfmt, double_arg);
		    pdest += strlen(pdest);
		    break;

		  case 'p':
		    ptr_arg = va_arg(args, void *);
		    snprintf(pdest, size - (pdest-dest), newfmt, ptr_arg);
		    pdest += strlen(pdest);
		    break;

		  case '%':
		    if(IS_ROOM_IN_DEST(1))
		      *pdest++ =  '%';

		    break;
		  
		  default:
		    /* didn't think of this type */
		    assert(0);
		    break;
		}

	        break;
	    }

	    fmt++;
	}
	else{
	    if(IS_ROOM_IN_DEST(1))
	      *pdest++ = *fmt++;
	}
    }

    ret = pdest - dest;

    if(IS_ROOM_IN_DEST(1))
      *pdest++ = '\0';

    va_end(args);

    return ret;
}


/*
 * Copy UTF-8 characters from src into dst.
 * Copy enough characters so that the result will have (<=) screen width of
 * want_width screen cells in current locale.
 *
 * Dstlen is the available space in dst. No more than dstlen bytes will be written
 *   to dst.
 *
 * Returned value is the number of bytes written to dst, not including
 *   the possible terminating null.
 * Got_width is another returned value. It is the width in screen cells of
 *   the string placed in dst. It will be the same as want_width if there
 *   are enough characters in the src to do that and if the character widths
 *   hit the width exactly. It will be less than want_width if we run out
 *   of src characters or if the next character width would skip over the
 *   width we want, because it is double width.
 *
 * Zero width characters are collected and included at the end of the string.
 *   That is, if we make it to want_width but there is still a zero length
 *   character sitting in src, we add that to dst. This might be an accent
 *   or something like that.
 */
size_t
utf8_to_width(char *dst,		/* destination buffer */
	      char *src,		/* source string */
	      size_t dstlen,		/* space in dst */
	      unsigned want_width,	/* desired screen width */
	      unsigned *got_width)	/* returned screen width in dst */
{
    int this_width;
    unsigned width_consumed = 0;
    UCS ucs;
    unsigned long remaining_octets;
    char *writeptr, *readptr, *savereadptr, *endptr;
    int ran_out_of_space = 0;

    readptr = src;

    remaining_octets = readptr ? strlen(readptr) : 0;

    writeptr = dst;
    endptr = writeptr + dstlen;

    if(readptr && writeptr){
      while(width_consumed <= want_width && remaining_octets > 0 && writeptr < dst + dstlen && !ran_out_of_space){
	savereadptr = readptr;
	ucs = (UCS) utf8_get((unsigned char **) &readptr, &remaining_octets);

	if(ucs & U8G_ERROR || ucs == UBOGON)
	  remaining_octets = 0;
	else{
	  this_width = wcellwidth(ucs);

	  /*
	   * If this_width is -1 that means we can't print this character
	   * with our current locale. Writechar will print a '?'.
	   */
	  if(this_width < 0)
	    this_width = 1;

	  if(width_consumed + (unsigned) this_width <= want_width){
	    /* append this utf8 character to dst if it will fit */
	    if(writeptr + (readptr - savereadptr) < endptr){
	      width_consumed += this_width;
	      while(savereadptr < readptr)
	        *writeptr++ = *savereadptr++;
	    }
	    else
	      ran_out_of_space++;	/* no more utf8 to dst */
	  }
	  else
	    remaining_octets = 0;	/* we're done */
	}
      }

      if(writeptr < endptr)
        *writeptr = '\0';
    }

    if(got_width)
      *got_width = width_consumed;

    return(writeptr ? (writeptr - dst) : 0);
}


/*
 * Str is a UTF-8 string.
 * Count forward width screencell positions and return a pointer to the
 * end of the string that is width wide.
 * The returned pointer points at the next character (where the null would
 * be placed).
 *
 * Got_width is another returned value. It is the width in screen cells of
 *   the string from str to the returned pointer. It will be the same as
 *   want_width if there are enough characters in the str to do that
 *   and if the character widths hit the width exactly. It will be less
 *   than want_width if we run out of characters or if the next character
 *   width would skip over the width we want, because it is double width.
 */
char *
utf8_count_forw_width(char *str, unsigned want_width, unsigned *got_width)
{
    int this_width;
    unsigned width_consumed = 0;
    UCS ucs;
    unsigned long remaining_octets;
    char *readptr;
    char *retptr;

    retptr = readptr = str;

    remaining_octets = readptr ? strlen(readptr) : 0;

    while(width_consumed <= want_width && remaining_octets > 0){

	ucs = (UCS) utf8_get((unsigned char **) &readptr, &remaining_octets);

	if(ucs & U8G_ERROR || ucs == UBOGON){
	    /*
	     * This should not happen, but do something to handle it anyway.
	     * Treat each character as a single width character, which is what should
	     * probably happen when we actually go to write it out.
	     */
	    remaining_octets--;
	    readptr++;
	    this_width = 1;
	}
	else{
	    this_width = wcellwidth(ucs);

	    /*
	     * If this_width is -1 that means we can't print this character
	     * with our current locale. Writechar will print a '?'.
	     */
	    if(this_width < 0)
	      this_width = 1;
	}

	if(width_consumed + (unsigned) this_width <= want_width){
	    width_consumed += (unsigned) this_width;
	    retptr = readptr;
	}
	else
	  remaining_octets = 0;	/* we're done */
    }

    if(got_width)
      *got_width = width_consumed;

    return(retptr);
}


/*
 * Copy a null terminator into a UTF-8 string in place so that the string is
 * no more than a certain screen width wide. If the string is already less
 * than or equal in width to the requested width, no change is made.
 *
 * The actual width accomplished is returned. Note that it may be less than
 * max_width due to double width characters as well as due to the fact that
 * it fits wholly in the max_width.
 *
 * Returned value is the actual screen width of str when done.
 *
 * A side effect is that a terminating null may have been written into
 * the passed in string.
 */
unsigned
utf8_truncate(char *str, unsigned max_width)
{
    int this_width;
    unsigned width_consumed = 0;
    UCS ucs;
    unsigned long remaining_octets;
    char *readptr, *savereadptr;

    readptr = str;

    remaining_octets = readptr ? strlen(readptr) : 0;

    if(readptr){
      while(width_consumed <= max_width && remaining_octets > 0){

	savereadptr = readptr;
	ucs = (UCS) utf8_get((unsigned char **) &readptr, &remaining_octets);

	if(ucs & U8G_ERROR || ucs == UBOGON){
	    /*
	     * This should not happen, but do something to handle it anyway.
	     * Treat each character as a single width character, which is what should
	     * probably happen when we actually go to write it out.
	     */
	    remaining_octets--;
	    readptr++;
	    this_width = 1;
	}
	else{
	    this_width = wcellwidth(ucs);

	    /*
	     * If this_width is -1 that means we can't print this character
	     * with our current locale. Writechar will print a '?'.
	     */
	    if(this_width < 0)
	      this_width = 1;
	}

	if(width_consumed + (unsigned) this_width <= max_width){
	    width_consumed += (unsigned) this_width;
	}
	else{
	    remaining_octets = 0;	/* we're done */
	    *savereadptr = '\0';
	}
      }
    }

    return(width_consumed);
}


/*
 * Copy UTF-8 characters from src into dst.
 * Copy enough characters so that the result will have screen width of
 * want_width screen cells in current locale.
 * If there aren't enough characters in src to get to want_width, pad on
 * left or right according to left_adjust argument.
 *
 * Dstlen is the available space in dst. No more than dstlen bytes will be written
 *   to dst. Dst will be null terminated if there is enough room, but not
 *   if that would overflow dst's len.
 *
 * Returned value is the number of bytes written to dst, not including
 *   the possible terminating null.
 */
size_t
utf8_pad_to_width(char *dst,		/* destination buffer */
		  char *src,		/* source string */
		  size_t dstlen,	/* space in dst */
		  unsigned want_width,	/* desired screen width */
		  int left_adjust)	/* adjust left or right in want_width columns */
{
    unsigned got_width = 0;
    int      need_more, howmany;
    size_t   len_left, bytes_used;

    bytes_used = utf8_to_width(dst, src, dstlen, want_width, &got_width);
    len_left = dstlen - bytes_used;

    need_more = want_width - got_width;
    howmany = MIN(need_more, len_left);
    
    if(howmany > 0){
	char *end, *newend, *p, *q;

	end = dst + bytes_used;
	newend = end + howmany;
	if(left_adjust){
	    /*
	     * Add padding to end of string. Simply append
	     * the needed number of spaces, or however many will fit
	     * if we don't have enough space.
	     */
	    for(q = end; q < newend; q++)
	      *q = ' ';
	}
	else{
	    /*
	     * Add padding to start of string.
	     */

	    /* slide existing string over */
	    for(p = end - 1, q = newend - 1; p >= dst; p--, q--)
	      *q = *p;

	    /* fill rest with spaces */
	    for(; q >= dst; q--)
	      *q = ' ';
	}

	bytes_used += howmany;
    }

    if(bytes_used < dstlen)
      dst[bytes_used] = '\0';

    return(bytes_used);
}


/*
 * Str is a UTF-8 string.
 * Start_here is a pointer into the string. It points one position past
 * the last byte that should be considered a part of the length string.
 * Count back want_width screencell positions and return a pointer to the
 * start of the string that is want_width wide and ends with start_here.
 *
 * Since characters may be more than one cell width wide we may end up
 * skipping over the exact width. That is, if we need to we'll go back
 * too far (by one cell width). Account for that in the call by looking
 * at got_width.
 *
 * Note that this call gives a possible got_width == want_width+1 as
 * opposed to utf8_count_forw_width which gives got_width == want-1 instead.
 * That was just what was needed at the time, maybe it needs to be
 * optional.
 */
char *
utf8_count_back_width(char *str, char *start_here, unsigned want_width, unsigned *got_width)
{
    unsigned width_consumed = 0;
    int this_width;
    UCS ucs;
    unsigned long remaining_octets;
    char *ptr, *savereadptr, *goodreadptr;

    savereadptr = start_here;
    goodreadptr = start_here;

    for(ptr = savereadptr - 1; width_consumed < want_width && ptr >= str; ptr = savereadptr - 1){

	savereadptr = ptr;
	remaining_octets = goodreadptr - ptr;
	ucs = (UCS) utf8_get((unsigned char **) &ptr, &remaining_octets);

	if(!(ucs & U8G_ERROR || ucs == UBOGON)){
	  if(remaining_octets > 0){
	      /*
	       * This means there are some bad octets after this good
	       * character so things are not going to work out well.
	       * Bail out.
	       */
	      savereadptr = str;	/* we're done */
	  }
	  else{
	    this_width = wcellwidth(ucs);

	    /*
	     * If this_width is -1 that means we can't print this character
	     * with our current locale. Writechar will print a '?'.
	     */
	    if(this_width < 0)
	      this_width = 1;

	    width_consumed += (unsigned) this_width;
	    goodreadptr = savereadptr;
	  }
        }
    }

    if(got_width)
      *got_width = width_consumed;

    return(savereadptr);
}


/*----------------------------------------------------------------------
  copy the source string onto the destination string returning with
  the destination string pointer at the end of the destination text

  motivation for this is to avoid twice passing over a string that's
  being appended to twice (i.e., strcpy(t, x); t += strlen(t))

  This doesn't really belong here but it is used here.
 ----*/
void
sstrncpy(char **d, char *s, int n)
{
    while(n-- > 0 && (**d = *s++) != '\0')
      (*d)++;
}


/*
 * If use_system_routines is set then NULL is the return value and it is
 * not an error. Display_charmap and keyboard_charmap should come over as
 * malloced strings and will be filled in with the result.
 *
 * Returns a void pointer to the input_cs CHARSET which is
 * passed to mbtow via kbseq().
 * If !use_system_routines && NULL is returned, that is an error and err should
 * have a message.
 * display_charmap and keyboard_charmap should be malloced data and may be
 * realloced and changed here.
 */
int
setup_for_input_output(int use_system_routines, char **display_charmap,
		       char **keyboard_charmap, void **input_cs_arg, char **err)
{
    const CHARSET *cs;
    const CHARSET *input_cs = NULL;
    int already_tried = 0;
    int supported = 0;
    char buf[1000];

#define cpstr(s) strcpy((char *)fs_get(1+strlen(s)), s)

    if(err)
      *err = NULL;

    if(!display_charmap || !keyboard_charmap || !input_cs_arg){
	*err = cpstr("Bad call to setup_for_input_output");
	return(-1);
    }

    if(use_system_routines){
#if	PREREQ_FOR_SYS_TRANSLATION
	char *dcm;

	dcm = nl_langinfo_codeset_wrapper();
	dcm = dcm ? dcm : "US-ASCII";

	init_utf8_display(0, NULL);
	if(*display_charmap){
	    if(dcm && strucmp(*display_charmap, dcm)){
		snprintf(buf, sizeof(buf),
		 _("Display character set \"%s\" is ignored when using system translation"),
		     *display_charmap);

		*err = cpstr(buf);
	    }

	    fs_give((void **) display_charmap);
	}

	if(*keyboard_charmap){
	    if(!*err && dcm && strucmp(*keyboard_charmap, dcm)){
		snprintf(buf, sizeof(buf),
		 _("Keyboard character set \"%s\" is ignored when using system translation"),
		     *keyboard_charmap);

		*err = cpstr(buf);
	    }

	    fs_give((void **) keyboard_charmap);
	}

	*display_charmap = cpstr(dcm);
	*keyboard_charmap = cpstr(dcm);
#else
	*err = cpstr("Bad call to setup_for_input_output");
#endif

	*input_cs_arg = NULL;
	return(0);
    }


try_again1:
    if(!(*display_charmap))
      *display_charmap = cpstr("US-ASCII");

    if(!(*keyboard_charmap))
      *keyboard_charmap = cpstr(*display_charmap);

    if(*keyboard_charmap){
	supported = input_charset_is_supported(*keyboard_charmap);

	if(supported){
	    if(!strucmp(*keyboard_charmap, "utf-8"))
	      input_cs = utf8_charset(*keyboard_charmap);
	    else if((cs = utf8_charset(*keyboard_charmap)) != NULL)
	      input_cs = cs;
	}
	else{
	    if(err && !*err){
		int iso2022jp = 0;

		if(!strucmp(*keyboard_charmap, "ISO-2022-JP"))
		  iso2022jp = 1;

		snprintf(buf, sizeof(buf),
		     /* TRANSLATORS: The first argument is the name of the character
		        set the user is trying to use (which is unsupported by alpine).
			The second argument is " (except for posting)" if they are
			trying to use ISO-2022-JP for something other than posting. */
		     _("Character set \"%s\" is unsupported%s, using US-ASCII"),
		     *keyboard_charmap,
		     iso2022jp ? _(" (except for posting)") : "");

		*err = cpstr(buf);
	    }

	    input_cs = NULL;
	    fs_give((void **) keyboard_charmap);
	    *keyboard_charmap = cpstr("US-ASCII");
	    if(!already_tried){
		already_tried++;
		goto try_again1;
	    }
	}
    }
   

try_again2:
    if(!(*display_charmap))
      *display_charmap = cpstr("US-ASCII");

    if(*display_charmap){
	supported = output_charset_is_supported(*display_charmap);
	if(supported){
	    if(!strucmp(*display_charmap, "utf-8"))
	      init_utf8_display(1, NULL);
	    else if((cs = utf8_charset(*display_charmap)) != NULL)
	      init_utf8_display(0, utf8_rmap_gen(cs, NULL));
	}
	else{
	    if(err && !*err){
		int iso2022jp = 0;

		if(!strucmp(*display_charmap, "ISO-2022-JP"))
		  iso2022jp = 1;

		snprintf(buf, sizeof(buf),
		     _("Character set \"%s\" is unsupported%s, using US-ASCII"),
		     *display_charmap,
		     iso2022jp ? _(" (except for posting)") : "");

		*err = cpstr(buf);
	    }

	    fs_give((void **) display_charmap);
	    if(!already_tried){
		already_tried++;
		goto try_again2;
	    }
	}
    }
    else{
	if(err && !*err)
	  *err = cpstr(_("Help, can't figure out display character set or even use US-ASCII."));
    }

#undef cpstr

    *input_cs_arg = (void *) input_cs;

    return(0);
}


int
input_charset_is_supported(char *input_charset)
{
    const CHARSET *cs;

    if(!(input_charset && *input_charset))
      return 0;

    if(!strucmp(input_charset, "utf-8"))
      return 1;

    if((cs = utf8_charset(input_charset)) != NULL){

	/*
	 * This was true 2006-09-25.
	 */
	switch(cs->type){
	  case CT_ASCII: case CT_1BYTE0: case CT_1BYTE:
	  case CT_1BYTE8: case CT_EUC: case CT_DBYTE:
	  case CT_DBYTE2: case CT_SJIS: case CT_UCS2:
	  case CT_UCS4: case CT_UTF16:
	    return 1;
	    break;

	  default:
	    break;
	}
    }

    return 0;
}


int
output_charset_is_supported(char *output_charset)
{
    const CHARSET *cs;

    if(!(output_charset && *output_charset))
      return 0;

    if(!strucmp(output_charset, "utf-8"))
      return 1;

    if((cs = utf8_charset(output_charset)) != NULL && utf8_rmap_gen(cs, NULL))
      return 1;

    return 0;
}


int
posting_charset_is_supported(char *posting_charset)
{
    return(posting_charset && *posting_charset
	   && (!strucmp(posting_charset, "ISO-2022-JP")
	       || output_charset_is_supported(posting_charset)));
}


/*
 * This function is only defined in this special case and so calls
 * to it should be wrapped in the same macro conditionals.
 *
 * Returns the default display charset for a UNIX terminal emulator,
 * it is what nl_langinfo(CODESET) should return but we need to
 * wrap nl_langinfo because we know of strange behaving implementations.
 */
#if !defined(_WINDOWS) && HAVE_LANGINFO_H && defined(CODESET)
char *
nl_langinfo_codeset_wrapper(void)
{
    char *ret = NULL;

    ret = nl_langinfo(CODESET);
    
    /*
     * If the value returned from nl_langinfo() is not a real charset,
     * see if we can figure out what they meant. If we can't figure it
     * out return NULL and let the caller decide what to do.
     */
    if(ret && *ret && !output_charset_is_supported(ret)){
	if(!strcmp("ANSI_X3.4-1968", ret)
	   || !strcmp("646", ret)
	   || !strcmp("ASCII", ret)
	   || !strcmp("C", ret)
	   || !strcmp("POSIX", ret))
	  ret = "US-ASCII";
	else if(!strucmp(ret, "UTF8"))
	  ret = "UTF-8";
	else if(!strucmp(ret, "EUCJP"))
	  ret = "EUC-JP";
	else if(!strucmp(ret, "EUCKP"))
	  ret = "EUC-KP";
	else if(!strucmp(ret, "SJIS"))
	  ret = "SHIFT-JIS";
	else if(strstr(ret, "8859")){
	    char *p;

	    /* check for digits after 8859 */
	    p = strstr(ret, "8859");
	    p += 4;
	    if(!isdigit(*p))
	      p++;

	    if(isdigit(*p)){
		static char buf[12];

		memset(buf, 0, sizeof(buf));
		strncpy(buf, "ISO-8859-", sizeof(buf));
		buf[9] = *p++;
		if(isdigit(*p))
		  buf[10] = *p;

		ret = buf;
	    }
	}
    }

    if(ret && !output_charset_is_supported(ret))
      ret = NULL;

    return(ret);
}
#endif


/*
 * Convert the "orig" string from UTF-8 to "charset". If no conversion is
 * needed the return value will point to orig. If a conversion is done,
 * the return string should be freed by the caller.
 * If not possible, returns NULL.
 */
char *
utf8_to_charset(char *orig, char *charset, int report_err)
{
    SIZEDTEXT src, dst;
    char *ret = orig;

    if(!charset || !charset[0] || !orig || !orig[0] || !strucmp(charset, "utf-8"))
      return ret;

    src.size = strlen(orig);
    src.data = (unsigned char *) orig;

    if(!strucmp(charset, "us-ascii")){
	size_t i;

	for(i = 0; i < src.size; i++)
	  if(src.data[i] & 0x80)
	    return NULL;

	return ret;
    }

    /*
     * This works for ISO-2022-JP because of special code in utf8_cstext
     * but not for other 2022 charsets.
     */
    memset(&dst, 0, sizeof(dst));
    if(utf8_cstext(&src, charset, &dst, report_err ? 0 : '?') && dst.size > 0 && dst.data)
      ret = (char *) dst.data;		/* c-client already null terminates it */
    else
      ret = NULL;

    if((unsigned char *) ret != dst.data && dst.data)
      fs_give((void **) &dst.data);

    return ret;
}


/*
 *      Turn a number into a string with comma's
 *
 * Args: number -- The long to be turned into a string. 
 *
 * Result: pointer to static string representing number with commas
 * Can use up to 3 comatose results at once.
 */
char *
comatose(long int number)
{
    long        i, x, done_one;
    static char buf[3][50];
    static int whichbuf = 0;
    char       *b;

    whichbuf = (whichbuf + 1) % 3;

    if(number == 0){
        strncpy(buf[whichbuf], "0", sizeof(buf[0]));
	buf[whichbuf][sizeof(buf[0])-1] = '\0';
        return(buf[whichbuf]);
    }
    
    done_one = 0;
    b = buf[whichbuf];
    for(i = 1000000000; i >= 1; i /= 1000) {
	x = number / i;
	number = number % i;
	if(x != 0 || done_one) {
	    if(b != buf[whichbuf] && (b-buf[whichbuf]) <  sizeof(buf[0]))
	      *b++ = ',';

	    snprintf(b, sizeof(buf[0])-(b-buf[whichbuf]), done_one ? "%03ld" : "%ld", x);
	    b += strlen(b);
	    done_one = 1;
	}
    }

    if(b-buf[whichbuf] < sizeof(buf[0]))
      *b = '\0';

    return(buf[whichbuf]);
}


/* leave out the commas */
char *
tose(long int number)
{
    static char buf[3][50];
    static int whichbuf = 0;

    whichbuf = (whichbuf + 1) % 3;

    snprintf(buf[whichbuf], sizeof(buf[0]), "%ld", number);

    return(buf[whichbuf]);
}


/*
 * line_paint - where the real work of managing what is displayed gets done.
 */
void
line_paint(int offset,			/* current dot offset into vl */
	   struct display_line *displ,
	   int *passwd)			/* flag to hide display of chars */
{
    int i, w, w2, already_got_one = 0;
    int vfirst, vlast, dfirst, dlast, vi, di;
    int new_vbase;
    unsigned (*width_a_to_b)(UCS *, int, int);

    /*
     * Set passwd to 10 in caller if you want to conceal the
     * password but not print asterisks for feedback.
     *
     * Set passwd to 1 in caller to conceal by printing asterisks.
     */
    if(passwd && *passwd >= 10){	/* don't show asterisks */
	if(*passwd > 10)
	  return;
	else
	  *passwd = 11;		/* only blat once */

	i = 0;
	(*displ->movecursor)(displ->row, displ->col);
	while(i++ <= displ->dwid)
	  (*displ->writechar)(' ');

	(*displ->movecursor)(displ->row, displ->col);
	return;
    }

    if(passwd && *passwd)
      width_a_to_b = single_width_chars_a_to_b;
    else
      width_a_to_b = ucs4_str_width_a_to_b;

    /*
     * vl is the virtual line (the actual data). We operate on it by typing
     * characters to be added and deleting and so forth. In this routine we
     * copy a subset of those UCS-4 characters in vl into dl, the display
     * array, and show that subset on the screen.
     *
     * Offset is the location of the cursor in vl.
     *
     * We will display the string starting from vbase.
     * We have dwid screen cells to work in.
     * We may have to adjust vbase in order to display the
     * part of the string that contains the cursor.
     *
     * We'll make the display look like
     *   vl    a b c d e f g h i j k l m
     *             xxxxxxxxxxxxx  <- width dwid window
     *             < d e f g h >
     *               |
     *             vbase
     * The < will be there if vbase > 0.
     * The > will be there if the string from vbase to the
     * end can't all fit in the window.
     */

    memset(displ->dl, 0, displ->dlen * sizeof(UCS));

    /*
     * Adjust vbase so offset is not out of the window to the right.
     * (The +2 in w + 2 is for a possible " >" if the string goes past
     *  the right hand edge of the window and if the last visible character
     * is double wide. We don't want the offset to be under that > character.)
     */
    for(w = (*width_a_to_b)(displ->vl, displ->vbase, offset);
	w + 2 + (displ->vbase ? 1 : 0) > displ->dwid;
        w = (*width_a_to_b)(displ->vl, displ->vbase, offset)){
	/*
	 * offset is off the window to the right
	 * It looks like   a b c d e f g h
	 *                   |         |
	 *               vbase         offset
	 * and offset is either past the right edge,
	 * or right at the right edge (and maybe under >),
	 * or one before right at the edge (and maybe on space
	 * for half a character).
	 *
	 * Since the characters may be double width it is slightly
	 * complicated to figure out how far to increase vbase.
	 * We're going to scoot over past width w/2 characters and
	 * then see if that's sufficient.
	 */
	new_vbase = displ->vbase + 1;
	for(w2 = (*width_a_to_b)(displ->vl, displ->vbase+1, new_vbase);
	    w2 < displ->dwid/2;
	    w2 = (*width_a_to_b)(displ->vl, displ->vbase+1, new_vbase))
	  new_vbase++;

	displ->vbase = new_vbase;
    }

    /* adjust so offset is not out of the window to the left */
    while(displ->vbase > 0 && displ->vbase >= offset){
	/* add about dwid/2 more width */
	new_vbase = displ->vbase - 1;
	for(w2 = (*width_a_to_b)(displ->vl, new_vbase, displ->vbase);
	    w2 < (displ->dwid+1)/2 && new_vbase > 0;
	    w2 = (*width_a_to_b)(displ->vl, new_vbase, displ->vbase))
	  new_vbase--;

	/* but don't let it get too small, recheck off right end */
	for(w = (*width_a_to_b)(displ->vl, new_vbase, offset);
	    w + 2 + (new_vbase ? 1 : 0) > displ->dwid;
	    w = (*width_a_to_b)(displ->vl, displ->vbase, offset))
	  new_vbase++;

	displ->vbase = MAX(new_vbase, 0);
    }

    if(displ->vbase == 1 && ((passwd && *passwd) || wcellwidth(displ->vl[0]) == 1))
      displ->vbase = 0;
	 
    vfirst = displ->vbase;
    dfirst = 0;
    if(displ->vbase > 0){			/* off screen cue left */
	dfirst = 1;				/* index which matches vfirst */
	displ->dl[0] = '<';
    }

    vlast = displ->vused-1;			/* end */
    w = (*width_a_to_b)(displ->vl, vfirst, vlast);

    if(w + dfirst > displ->dwid){			/* off window right */

	/* find last ucs character to be printed */
	while(w + dfirst > displ->dwid - 1)	/* -1 for > */
	  w = (*width_a_to_b)(displ->vl, vfirst, --vlast);

	/* worry about double-width characters */
	if(w + dfirst == displ->dwid - 1){	/* no prob, hit it exactly */
	    dlast = dfirst + vlast - vfirst + 1;	/* +1 for > */
	    displ->dl[dlast] = '>';
	}
	else{
	    dlast = dfirst + vlast - vfirst + 1;
	    displ->dl[dlast++] = ' ';
	    displ->dl[dlast] = '>';
	}
    }
    else
      dlast = dfirst + vlast - vfirst;

    /*
     * Copy the relevant part of the virtual line into the display line.
     */
    for(vi = vfirst, di = dfirst; vi <= vlast; vi++, di++)
      if(passwd && *passwd)
        displ->dl[di] = '*';		/* to conceal password */
      else
        displ->dl[di] = displ->vl[vi];

    /*
     * Add spaces to clear the rest of the line.
     * We have dwid total space to fill.
     */
    w = (*width_a_to_b)(displ->dl, 0, dlast);	/* width through dlast */
    for(di = dlast+1, i = displ->dwid - w; i > 0 ; i--)
      displ->dl[di++] = ' ';

    /*
     * Draw from left to right, skipping until we get to
     * something that is different. Characters may be different
     * widths than they were initially so paint from there the
     * rest of the way.
     */
    for(di = 0; displ->dl[di]; di++){
	if(already_got_one || displ->dl[di] != displ->olddl[di]){
	    /* move cursor first time */
	    if(!already_got_one++){
		w = (di > 0) ? (*width_a_to_b)(displ->dl, 0, di-1) : 0;
		(*displ->movecursor)(displ->row, displ->col + w);
	    }

	    (*displ->writechar)(displ->dl[di]);
	    displ->olddl[di] = displ->dl[di];
	}
    }

    memset(&displ->olddl[di], 0, (displ->dlen - di) * sizeof(UCS));

    /*
     * Move the cursor to the offset.
     *
     * The offset is relative to the start of the virtual array. We need
     * to find the location on the screen. The offset into the display array
     * will be offset-vbase+dfirst. We want to be at the start of that
     * character, so we need to find the width of all the characters up
     * to that point.
     */
    w = (offset > 0) ? (*width_a_to_b)(displ->dl, 0, offset-displ->vbase+dfirst-1) : 0;

    (*displ->movecursor)(displ->row, displ->col + w);
}


/*
 * This is just like ucs4_str_width_a_to_b() except all of the characters
 * are assumed to be of width 1. This is for printing out *'s when user
 * enters a password, while still managing to use the same code to do the
 * display.
 */
unsigned
single_width_chars_a_to_b(UCS *ucsstr, int a, int b)
{
    unsigned width = 0;
    int i;

    if(ucsstr)
      for(i = a; i <= b && ucsstr[i]; i++)
	width++;

    return width;
}
