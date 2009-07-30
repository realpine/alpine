#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: filesys.c 770 2007-10-24 00:23:09Z hubert@u.washington.edu $";
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

/* includable WITHOUT dependency on c-client */
#include "../../c-client/mail.h"
#include "../../c-client/utf8.h"

#ifdef _WINDOWS
/* wingdi.h uses ERROR (!) and we aren't using the c-client ERROR so... */
#undef ERROR
#endif

#include <system.h>
#include <general.h>

#include "../../c-client/fs.h"

/* includable WITHOUT dependency on pico */
#include "../../pico/keydefs.h"
#ifdef _WINDOWS
#include "../../pico/osdep/mswin.h"
#endif

#include "filesys.h"
#include "utf8.h"


#define bad_char ((UCS) '?')


/*
 * Make it easier to use the convert_to_locale function for filenames
 * and directory names. Note, only one at a time because there's only
 * one buffer.
 * This isn't being freed as it stands now.
 */
char *
fname_to_locale(char *fname)
{
    static char *fname_locale_buf = NULL;
    static size_t fname_locale_len = 0;
    char *converted_fname, *p;

    p = convert_to_locale(fname);
    if(p)
      converted_fname = p;
    else
      converted_fname = fname;

    if(converted_fname){
	if(strlen(converted_fname)+1 > fname_locale_len){
	    if(fname_locale_buf)
	      fs_give((void **) &fname_locale_buf);

	    fname_locale_len = strlen(converted_fname)+1;
	    fname_locale_buf = (char *) fs_get(fname_locale_len * sizeof(char));
	}

	strncpy(fname_locale_buf, converted_fname, fname_locale_len);
	fname_locale_buf[fname_locale_len-1] = '\0';
    }
    else{
	if(fname_locale_len == 0){
	    fname_locale_len = 1;
	    fname_locale_buf = (char *) fs_get(fname_locale_len * sizeof(char));
	}

	fname_locale_buf[0] = '\0';
    }

    if(p)
      fs_give((void **) &p);

    return(fname_locale_buf);
}


/*
 * Make it easier to use the convert_to_utf8 function for filenames
 * and directory names. Note, only one at a time because there's only
 * one buffer.
 * This isn't being freed as it stands now.
 */
char *
fname_to_utf8(char *fname)
{
    static char *fname_utf8_buf = NULL;
    static size_t fname_utf8_len = 0;
    char *converted_fname, *p;

    p = convert_to_utf8(fname, NULL, 0);
    if(p)
      converted_fname = p;
    else
      converted_fname = fname;

    if(converted_fname){
	if(strlen(converted_fname)+1 > fname_utf8_len){
	    if(fname_utf8_buf)
	      fs_give((void **) &fname_utf8_buf);

	    fname_utf8_len = strlen(converted_fname)+1;
	    fname_utf8_buf = (char *) fs_get(fname_utf8_len * sizeof(char));
	}

	strncpy(fname_utf8_buf, converted_fname, fname_utf8_len);
	fname_utf8_buf[fname_utf8_len-1] = '\0';
    }
    else{
	if(fname_utf8_len == 0){
	    fname_utf8_len = 1;
	    fname_utf8_buf = (char *) fs_get(fname_utf8_len * sizeof(char));
	}

	fname_utf8_buf[0] = '\0';
    }

    if(p)
      fs_give((void **) &p);

    return(fname_utf8_buf);
}


/*
 * The fp file pointer is open for read on a file which has contents
 * that are encoded in the user's locale charset. That multibyte stream
 * of characters is converted to wide characters and returned one at
 * a time.
 *
 * Not sure what to do if an uninterpretable character happens. Returning
 * the bad character now.
 */
UCS
read_a_wide_char(FILE *fp,
		 void *input_cs)	/* input_cs ignored in Windows */
{
#ifdef _WINDOWS
    _TINT val;

    val = _fgettc(fp);
    if(val == _TEOF)
      return(CCONV_EOF);

    return((UCS) val);
#else /* UNIX */
    unsigned long octets_so_far, remaining_octets;
    unsigned char *inputp;
    unsigned char inputbuf[20];
    int c;
    UCS ucs;

    c = fgetc(fp);
    if(c == EOF)
      return(CCONV_EOF);

    /*
     * Read enough bytes to make up a character and convert it to UCS-4.
     */
    memset(inputbuf, 0, sizeof(inputbuf));
    inputbuf[0] = (unsigned char) c;
    octets_so_far = 1;
    for(;;){
	remaining_octets = octets_so_far;
	inputp = inputbuf;
	ucs = mbtow(input_cs, &inputp, &remaining_octets);
	switch(ucs){
	  case CCONV_BADCHAR:
	    return(bad_char);
	  
	  case CCONV_NEEDMORE:
	    if(octets_so_far >= sizeof(inputbuf))
	      return(bad_char);

	    c = fgetc(fp);
	    if(c == EOF)
	      return(CCONV_EOF);

	    inputbuf[octets_so_far++] = (unsigned char) c;
	    break;

	  default:
	    /* got a good UCS-4 character */
	    return(ucs);
	}
    }

    return(bad_char);
#endif /* UNIX */
}


int
write_a_wide_char(UCS ucs, FILE *fp)
{
#ifdef _WINDOWS
    int   rv = 1;
    TCHAR w;

    w = (TCHAR) ucs;
    if(_fputtc(w, fp) == _TEOF)
      rv = EOF;

    return(rv);
#else /* UNIX */
    int rv = 1;
    int i, outchars;
    unsigned char obuf[MAX(MB_LEN_MAX,32)];

     if(ucs < 0x80){
	obuf[0] = (unsigned char) ucs;
	outchars = 1;
    }
    else{
	outchars = wtomb((char *) obuf, ucs);
	if(outchars < 0){
	    outchars = 1;
	    obuf[0] = bad_char;		/* ??? */
	}
    }

    for(i = 0; i < outchars; i++)
      if(fputc(obuf[i], fp) == EOF){
	  rv = EOF;
	  break;
      }
        
    return(rv);
#endif /* UNIX */
}


int
our_stat(char *filename, struct stat *sbuf)
{
#ifdef _WINDOWS
    LPTSTR f = NULL;
    int    ret = -1;
    struct _stat s;

    f = utf8_to_lptstr((LPSTR) filename);
    if(f){
	ret = _tstat(f, &s);

	sbuf->st_dev = s.st_dev;
	sbuf->st_ino = s.st_ino;
	sbuf->st_mode = s.st_mode;
	sbuf->st_nlink = s.st_nlink;
	sbuf->st_uid = s.st_uid;
	sbuf->st_gid = s.st_gid;
	sbuf->st_rdev = s.st_rdev;
	sbuf->st_size = s.st_size;
	sbuf->st_atime = (time_t) s.st_atime;
	sbuf->st_mtime = (time_t) s.st_mtime;
	sbuf->st_ctime = (time_t) s.st_ctime;

	fs_give((void **) &f);
    }

    return ret;
#else /* UNIX */
    return(stat(fname_to_locale(filename), sbuf));
#endif /* UNIX */
}


int
our_lstat(char *filename, struct stat *sbuf)
{
#ifdef _WINDOWS
    assert(0);		/* lstat not used in Windows */
    return(-1);
#else /* UNIX */
    return(lstat(fname_to_locale(filename), sbuf));
#endif /* UNIX */
}


FILE *
our_fopen(char *path, char *mode)
{
#ifdef _WINDOWS
    LPTSTR p = NULL, m = NULL;
    FILE  *ret = NULL;
    char  *mode_with_ccs = NULL;
    char   buf[500];
    size_t len;

    if(mode && (*mode == 'r' || *mode == 'a')){
	char  *force_bom_check = ", ccs=UNICODE";

	if(strchr(mode, 'b'))
	  mode_with_ccs = mode;
	else{
	    /*
	     * The docs seem to say that we don't need the ccs parameter and
	     * if the file has a BOM at the beginning it will notice that and
	     * use it. However, we're not seeing that. Instead, what we see is
	     * that giving a parameter of UNICODE causes the desired behavior.
	     * This causes it to check for a BOM and if it finds one it uses it.
	     * If it doesn't find one, it treats the file as ANSI, which is what
	     * we want.
	     */
	    if((len = strlen(mode) + strlen(force_bom_check)) < sizeof(buf)){
		len = sizeof(buf)-1;
		mode_with_ccs = buf;
	    }
	    else
	      mode_with_ccs = (char *) MemAlloc((len+1) * sizeof(char));

	    if(mode_with_ccs)
	      snprintf(mode_with_ccs, len+1, "%s%s", mode, force_bom_check);
	    else
	      mode_with_ccs = mode;	/* can't happen */
	}
    }
    else if(mode && (*mode == 'w')){
	char  *force_utf8 = ", ccs=UTF-8";

	if(strchr(mode, 'b'))
	  mode_with_ccs = mode;
	else{
	    if((len = strlen(mode) + strlen(force_utf8)) < sizeof(buf)){
		len = sizeof(buf)-1;
		mode_with_ccs = buf;
	    }
	    else
	      mode_with_ccs = (char *) MemAlloc((len+1) * sizeof(char));

	    if(mode_with_ccs)
	      snprintf(mode_with_ccs, len+1, "%s%s", mode, force_utf8);
	    else
	      mode_with_ccs = mode;	/* can't happen */
	}
    }

    p = utf8_to_lptstr((LPSTR) path);

    if(p){
	m = utf8_to_lptstr((LPSTR) mode_with_ccs);
	if(m){
	    ret = _tfopen(p, m);
	    MemFree((void *) m);
	}

	fs_give((void **) &p);
    }

    if(mode_with_ccs && mode_with_ccs != buf && mode_with_ccs != mode)
      MemFree((void *) mode_with_ccs);

    return ret;
#else /* UNIX */
    return(fopen(fname_to_locale(path), mode));
#endif /* UNIX */
}


int
our_open(char *path, int flags, mode_t mode)
{
#ifdef _WINDOWS
    LPTSTR p = NULL;
    int ret = -1;

    /*
     * Setting the _O_WTEXT flag when opening a file for reading
     * will cause us to read the first few bytes to check for
     * a BOM and to translate from that encoding if we find it.
     * This only works with stream I/O, not low-level read/write.
     *
     * When opening for writing the flag _O_U8TEXT will cause
     * us to put a UTF-8 BOM at the start of the file.
     *
     * O_TEXT will cause LF -> CRLF on output, opposite on input
     * O_BINARY suppresses that.
     * _O_U8TEXT implies O_TEXT.
     */

    p = utf8_to_lptstr((LPSTR) path);

    if(p){
	ret = _topen(p, flags, mode);
	fs_give((void **) &p);
    }

    return ret;
#else /* UNIX */
    return(open(fname_to_locale(path), flags, mode));
#endif /* UNIX */
}


int
our_creat(char *path, mode_t mode)
{
#ifdef _WINDOWS
    LPTSTR p = NULL;
    int ret = -1;

    p = utf8_to_lptstr((LPSTR) path);

    if(p){
	ret = _tcreat(p, mode);
	fs_give((void **) &p);
    }

    return ret;
#else /* UNIX */
    return(creat(fname_to_locale(path), mode));
#endif /* UNIX */
}


int
our_mkdir(char *path, mode_t mode)
{
#ifdef _WINDOWS
    /* mode is a noop for _WINDOWS */
    LPTSTR p = NULL;
    int ret = -1;

    p = utf8_to_lptstr((LPSTR) path);

    if(p){
	ret = _tmkdir(p);
	fs_give((void **) &p);
    }

    return ret;
#else /* UNIX */
    return(mkdir(fname_to_locale(path), mode));
#endif /* UNIX */
}


int
our_rename(char *oldpath, char *newpath)
{
#ifdef _WINDOWS
    LPTSTR pold = NULL, pnew = NULL;
    int ret = -1;

    pold = utf8_to_lptstr((LPSTR) oldpath);
    pnew = utf8_to_lptstr((LPSTR) newpath);

    if(pold && pnew)
      ret = _trename(pold, pnew);

    if(pold)
      fs_give((void **) &pold);
    if(pnew)
      fs_give((void **) &pnew);

    return ret;
#else /* UNIX */
    char *p, *pold;
    size_t len;
    int ret = -1;

    p = fname_to_locale(oldpath);
    if(p){
	len = strlen(p);
	pold = (char *) fs_get((len+1) * sizeof(char));
	strncpy(pold, p, len+1);
	pold[len] = '\0';

	ret = rename(pold, fname_to_locale(newpath));
	fs_give((void **) &pold);
    }

    return ret;
#endif /* UNIX */
}


int
our_unlink(char *path)
{
#ifdef _WINDOWS
    LPTSTR p = NULL;
    int ret = -1;

    p = utf8_to_lptstr((LPSTR) path);

    if(p){
	ret = _tunlink(p);
	fs_give((void **) &p);
    }

    return ret;
#else /* UNIX */
    return(unlink(fname_to_locale(path)));
#endif /* UNIX */
}


int
our_link(char *oldpath, char *newpath)
{
#ifdef _WINDOWS
    assert(0);		/* link not used in Windows */
    return(-1);
#else /* UNIX */
    char *p, *pold;
    size_t len;
    int ret = -1;

    p = fname_to_locale(oldpath);
    if(p){
	len = strlen(p);
	pold = (char *) fs_get((len+1) * sizeof(char));
	strncpy(pold, p, len+1);
	pold[len] = '\0';

	ret = link(pold, fname_to_locale(newpath));
	fs_give((void **) &pold);
    }

    return ret;
#endif /* UNIX */
}


int
our_truncate(char *path, off_t size)
{
    int ret = -1;
#if defined(_WINDOWS) || !defined(HAVE_TRUNCATE)
    int fdes;
#endif

#ifdef _WINDOWS
    if((fdes = our_open(path, O_RDWR | O_CREAT | S_IREAD | S_IWRITE | _O_U8TEXT, 0600)) != -1){
	if(chsize(fdes, size) == 0)
	  ret = 0;

	close(fdes);
    }

#else /* UNIX */

#ifdef	HAVE_TRUNCATE
    ret = truncate(fname_to_locale(path), size);
#else	/* !HAVE_TRUNCATE */

    if((fdes = our_open(path, O_RDWR, 0600)) != -1){
	ret = chsize(fdes, size) ;

	if(close(fdes))
	    ret = -1;
    }
#endif /* !HAVE_TRUNCATE */
#endif /* UNIX */

    return ret;
}


int
our_chmod(char *path, mode_t mode)
{
#ifdef _WINDOWS
    LPTSTR p = NULL;
    int    ret = -1;

    p = utf8_to_lptstr((LPSTR) path);
    if(p){
	ret = _tchmod(p, mode);
	fs_give((void **) &p);
    }

    return ret;
#else /* UNIX */
    return(chmod(fname_to_locale(path), mode));
#endif /* UNIX */
}


int
our_chown(char *path, uid_t owner, gid_t group)
{
#ifdef _WINDOWS
    return 0;
#else /* UNIX */
    return(chown(fname_to_locale(path), owner, group));
#endif /* UNIX */
}


int
our_utime(char *path, struct utimbuf *buf)
{
#ifdef _WINDOWS
    LPTSTR p = NULL;
    int ret = -1;

    p = utf8_to_lptstr((LPSTR) path);

    if(p){
	ret = _tutime(p, buf);
	fs_give((void **) &p);
    }

    return ret;
#else /* UNIX */
    return(utime(fname_to_locale(path), buf));
#endif /* UNIX */
}

/* 
 * Return a malloc'd utf8-encoded char * of the provided environment 
 * variable. The env_variable argument is assumed not to be UTF-8.  Returns
 * NULL if no such environment variable.
 *
 * We'll pretty much swap out getenv's where convenient.  Windows pretty
 * much doesn't want to do getenv once we do unicode
 */
char *
our_getenv(char *env_variable)
{
#ifdef _WINDOWS
    TCHAR lptstr_env_variable[MAXPATH+1], *p;
    int i;

    for(i = 0; env_variable[i] && i < MAXPATH; i++)
      lptstr_env_variable[i] = env_variable[i];
    lptstr_env_variable[i] = '\0';
    if(p = _tgetenv(lptstr_env_variable))
      return(lptstr_to_utf8(p));
    else
      return(NULL);
#else /* !_WINDOWS */
    char *p, *utf8_p, *env_cpy;
    size_t len;
    if((p = getenv(env_variable)) != NULL){
	/* all this when what we want is a cpystr */
	utf8_p = fname_to_utf8(p);
	len = strlen(utf8_p);
	env_cpy = (char *)fs_get((len+1)*sizeof(char));
	strncpy(env_cpy, utf8_p, len+1);
	env_cpy[len] = '\0';

	return(env_cpy);
    }
    else
      return(NULL);
#endif /* !_WINDOWS */
}


int
our_access(char *path, int mode)
{
#ifdef _WINDOWS
    LPTSTR p = NULL;
    int    ret = -1;

    p = utf8_to_lptstr((LPSTR) path);
    if(p){
	ret = _taccess(p, mode);
	fs_give((void **) &p);
    }

    return ret;
#else /* UNIX */
    return(access(fname_to_locale(path), mode));
#endif /* UNIX */
}


/*
 * Fgets that doesn't do any character encoding translation or any
 * of that Windows stuff.
 */
char *
fgets_binary(char *s, int size, FILE *fp)
{
#ifdef _WINDOWS
    char *p;
    char c;
    int r;

    /*
     * Use fread low-level input instead of fgets.
     * Maybe if we understood better we wouldn't need this.
     */
    if(!s)
      return s;

    p = s;
    while(p-s < size-1 && (r=fread(&c, sizeof(c), (size_t) 1, fp)) == 1 && c != '\n')
      *p++ = c;

    if(p-s < size-1 && r == 1){
	/* must have gotten to end of line */
	*p++ = '\n';
    }

    *p = '\0';
    return(s);

#else /* UNIX */
    return(fgets(s, size, fp));
#endif /* UNIX */
}
