#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: store.c 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $";
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

/*
 * GENERALIZED STORAGE FUNCTIONS.  Idea is to allow creation of
 * storage objects that can be written into and read from without
 * the caller knowing if the storage is core or in a file
 * or whatever.
 */


#include "../pith/headers.h"
#include "../pith/store.h"
#include "../pith/status.h"
#include "../pith/state.h"
#include "../pico/keydefs.h"
#ifdef SMIME
#include <openssl/buffer.h>
#endif /* SMIME */


/*
 * Internal prototypes
 */
void   *so_file_open(STORE_S *);
int	so_cs_writec(int, STORE_S *);
int	so_cs_writec_locale(int, STORE_S *);
int	so_file_writec(int, STORE_S *);
int	so_file_writec_locale(int, STORE_S *);
int	so_cs_readc(unsigned char *, STORE_S *);
int	so_cs_readc_locale(unsigned char *, STORE_S *);
int	so_cs_readc_getchar(unsigned char *c, void *extraarg);
int	so_file_readc(unsigned char *, STORE_S *);
int	so_file_readc_locale(unsigned char *, STORE_S *);
int	so_file_readc_getchar(unsigned char *c, void *extraarg);
int	so_cs_puts(STORE_S *, char *);
int	so_cs_puts_locale(STORE_S *, char *);
int	so_file_puts(STORE_S *, char *);
int	so_file_puts_locale(STORE_S *, char *);
int	so_reaquire(STORE_S *);
#ifdef _WINDOWS
int	so_file_readc_windows(unsigned char *, STORE_S *);
#endif /* _WINDOWS */
#ifdef SMIME
int	so_bio_writec(int, STORE_S *);
int	so_bio_readc(unsigned char *, STORE_S *);
int	so_bio_puts(STORE_S *, char *);
#endif /* SMIME */


/*
 * place holders for externally defined storage object driver
 */
static struct externalstoreobjectdata {
    STORE_S *(*get)(void);
    int      (*give)(STORE_S **);
    int      (*writec)(int, STORE_S *);
    int      (*readc)(unsigned char *, STORE_S *);
    int      (*puts)(STORE_S *, char *);
    int      (*seek)(STORE_S *, long, int);
    int      (*truncate)(STORE_S *, off_t);
    int      (*tell)(STORE_S *);
} etsod;


#define	MSIZE_INIT	8192
#define	MSIZE_INC	4096


#ifdef	S_IREAD
#define	OP_MD_USER	(S_IREAD | S_IWRITE)
#else
#define	OP_MD_USER	0600
#endif

#ifdef	S_IRUSR
#define	OP_MD_ALL	(S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | \
			 S_IROTH | S_IWOTH)
#else
#define	OP_MD_ALL	0666
#endif


/*
 * allocate resources associated with the specified type of
 * storage.  If requesting a named file object, open it for
 * appending, else just open a temp file.
 *
 * return the filled in storage object
 */
STORE_S *
so_get(SourceType source, char *name, int rtype)
                       			/* requested storage type */
                     			/* file name 		  */
       		      			/* file access type	  */
{
    STORE_S *so = (STORE_S *)fs_get(sizeof(STORE_S));

    memset(so, 0, sizeof(STORE_S));
    so->flags |= rtype;

    so->cb.cbuf[0] = '\0';
    so->cb.cbufp   = so->cb.cbuf;
    so->cb.cbufend = so->cb.cbuf;

    if(name)					/* stash the name */
      so->name = cpystr(name);
#ifdef	DOS
    else if(source == TmpFileStar || source == FileStar){
	/*
	 * Coerce to TmpFileStar.  The MSC library's "tmpfile()"
	 * doesn't observe the "TMP" or "TEMP" environment vars and
	 * always wants to write "\".  This is problematic in shared,
	 * networked environments.
	 */
	source   = TmpFileStar;
	so->name = temp_nam(NULL, "pi");
    }
#else
    else if(source == TmpFileStar)		/* make one up! */
      so->name = temp_nam(NULL, "pine-tmp");
#endif

    so->src = source;
    if(so->src == FileStar || so->src == TmpFileStar){
#ifdef _WINDOWS
	so->writec =                              so_file_writec;
	so->readc  = (rtype & READ_FROM_LOCALE)	? so_file_readc_windows
						: so_file_readc;
#else /* UNIX */
	so->writec = (rtype & WRITE_TO_LOCALE)	? so_file_writec_locale
						: so_file_writec;
	so->readc  = (rtype & READ_FROM_LOCALE)	? so_file_readc_locale
						: so_file_readc;
#endif /* UNIX */
	so->puts   = (rtype & WRITE_TO_LOCALE)	? so_file_puts_locale
						: so_file_puts;

	/*
	 * The reason for both FileStar and TmpFileStar types is
	 * that, named or unnamed, TmpFileStar's are unlinked
	 * when the object is given back to the system.  This is
	 * useful for keeping us from running out of file pointers as
	 * the pointer associated with the object can be temporarily
	 * returned to the system without destroying the object.
	 *
	 * The programmer is warned to be careful not to assign the
	 * TmpFileStar type to any files that are expected to remain
	 * after the dust has settled!
	 */
	if(so->name){
	    if(!(so->txt = so_file_open(so))){
		dprint((1, "so_get error: %s : %s",
		       so->name ? so->name : "?",
		       error_description(errno)));
		if(source == TmpFileStar)
		  our_unlink(so->name);

		fs_give((void **)&so->name);
		fs_give((void **)&so); 		/* so freed & set to NULL */
	    }
	}
	else{
	    if(!(so->txt = (void *) create_tmpfile())){
		dprint((1, "so_get error: tmpfile : %s",
			   error_description(errno)));
		fs_give((void **)&so);		/* so freed & set to NULL */
	    }
	}
    }
    else if(so->src == ExternalText && etsod.get){
	so->writec = etsod.writec;
	so->readc  = etsod.readc;
	so->puts   = etsod.puts;
	if(!(so->txt = (*etsod.get)())){
	    dprint((1, "so_get error: external driver allocation error"));

	    if(so->name)
	      fs_give((void **)&so->name);

	    fs_give((void **)&so);		/* so freed & set to NULL */
	}
    }
#ifdef SMIME
    else if(so->src == BioType){
	so->writec = so_bio_writec;
	so->readc  = so_bio_readc;
	so->puts   = so_bio_puts;

	if(!(so->txt = BIO_new(BIO_s_mem()))){
	    dprint((1, "so_get error: BIO driver allocation error"));

	    if(so->name)
	      fs_give((void **) &so->name);

	    fs_give((void **) &so);		/* so freed & set to NULL */
	}
    }
#endif /* SMIME */
    else{
	so->writec = (rtype & WRITE_TO_LOCALE)	? so_cs_writec_locale
						: so_cs_writec;
	so->readc  = (rtype & READ_FROM_LOCALE)	? so_cs_readc_locale
						: so_cs_readc;
	so->puts   = (rtype & WRITE_TO_LOCALE)	? so_cs_puts_locale
						: so_cs_puts;

	so->txt	   = (void *)fs_get((size_t) MSIZE_INIT * sizeof(char));
	so->dp	   = so->eod = (unsigned char *) so->txt;
	so->eot	   = so->dp + MSIZE_INIT;
	memset(so->eod, 0, so->eot - so->eod);
    }

    return(so);
}


/*
 * so_give - free resources associated with a storage object and then
 *           the object itself.
 */
int
so_give(STORE_S **so)
{
    int ret = 0;

    if(!(so && (*so)))
      return(ret);

    if((*so)->src == FileStar || (*so)->src == TmpFileStar){
        if((*so)->txt)			/* disassociate from storage */
	  ret = fclose((FILE *)(*so)->txt) == EOF ? -1 : 0;

	if((*so)->name && (*so)->src == TmpFileStar)
	  our_unlink((*so)->name);	/* really disassociate! */
    }
    else if((*so)->txt && (*so)->src == ExternalText){
	if(etsod.give)
	  (*etsod.give)((*so)->txt);
    }
#ifdef SMIME
    else if((*so)->txt && (*so)->src == BioType){
	BIO *b = (BIO *) (*so)->txt;

	BIO_free(b);
    }
#endif /* SMIME */
    else if((*so)->txt)
      fs_give((void **)&((*so)->txt));

    if((*so)->name)
      fs_give((void **)&((*so)->name));	/* blast the name            */

					/* release attribute list */
    mail_free_body_parameter(&(*so)->attr);

    fs_give((void **)so);		/* release the object        */

    return(ret);
}


/*
 * so_register_external_driver - hokey way to get pico-dependent storage object
 *                               support out of pith library
 */
void
so_register_external_driver(STORE_S *(*get)(void),
			    int (*give)(STORE_S **),
			    int (*writec)(int, STORE_S *),
			    int (*readc)(unsigned char *, STORE_S *),
			    int (*puts)(STORE_S *, char *),
			    int (*seek)(STORE_S *, long int, int),
			    int (*truncate)(STORE_S *, off_t),
			    int (*tell)(STORE_S *))
{
    memset(&etsod, 0, sizeof(etsod));
    if(get)
      etsod.get = get;

    if(give)
      etsod.give = give;

    if(writec)
      etsod.writec = writec;

    if(readc)
      etsod.readc = readc;

    if(puts)
      etsod.puts = puts;

    if(seek)
      etsod.seek = seek;

    if(truncate)
      etsod.truncate = truncate;

    if(tell)
      etsod.tell = tell;
}


void *
so_file_open(STORE_S *so)
{
    char *type  = ((so->flags) & WRITE_ACCESS) ? "a+" : "r";
    int   flags, fd,
	  mode  = (((so->flags) & OWNER_ONLY) || so->src == TmpFileStar)
		   ? OP_MD_USER : OP_MD_ALL;

    /*
     * Careful. EDIT_ACCESS and WRITE_TO_LOCALE/READ_FROM_LOCALE will
     * not work together.
     */
    if(so->flags & WRITE_ACCESS){
	flags = O_RDWR | O_CREAT | O_APPEND | O_BINARY;
    }
    else{
	flags = O_RDONLY;
	if(so->flags & READ_FROM_LOCALE){
	   flags |= _O_WTEXT;
	}
	else{
	   flags |= O_BINARY;
	}
    }

    /*
     * Use open instead of fopen so we can make temp files private.
     * I believe the "b" or "t" in the type isn't necessary in the fdopen call
     * because we already set O_BINARY or _O_U8TEXT or _O_WTEXT in the open.
     */
    return(((fd = our_open(so->name, flags, mode)) > -1)
	     ? (so->txt = (void *) fdopen(fd, type)) : NULL);
}


/*
 * put a character into the specified storage object,
 * expanding if neccessary
 *
 * return 1 on success and 0 on failure
 */
int
so_cs_writec(int c, STORE_S *so)
{
    if(so->dp >= so->eot){
	size_t incr;
	size_t cur_o  = so->dp - (unsigned char *) so->txt;
	size_t data_o = so->eod - (unsigned char *) so->txt;
	size_t size   = (so->eot - (unsigned char *) so->txt);

	/*
	 * We estimate the size we're going to need at the beginning
	 * so it shouldn't normally happen that we run out of space and
	 * come here. If we do come here, it is probably because there
	 * are lots of handles in the message or lots of quote coloring.
	 * In either case, the overhead of those is high so we don't want
	 * to keep adding little bits and resizing. Add 50% each time
	 * instead.
	 */
	incr = MAX(size/2, MSIZE_INC);
	size += incr;

	fs_resize(&so->txt, size * sizeof(char));
	so->dp   = (unsigned char *) so->txt + cur_o;
	so->eod  = (unsigned char *) so->txt + data_o;
	so->eot  = (unsigned char *) so->txt + size;
	memset(so->eod, 0, so->eot - so->eod);
    }

    *so->dp++ = (unsigned char) c;
    if(so->dp > so->eod)
      so->eod = so->dp;

    return(1);
}


/*
 * The locale version converts from UTF-8 to user's locale charset
 * before writing the characters.
 */
int
so_cs_writec_locale(int c, STORE_S *so)
{
    int rv = 1;
    int i, outchars;
    unsigned char obuf[MAX(MB_LEN_MAX,32)];

    if((outchars = utf8_to_locale(c, &so->cb, obuf, sizeof(obuf))) != 0){
	for(i = 0; i < outchars; i++)
	  if(so_cs_writec(obuf[i], so) != 1){
	      rv = 0;
	      break;
	  }
    }

    return(rv);
}


int
so_file_writec(int c, STORE_S *so)
{
    unsigned char ch = (unsigned char) c;
    int rv = 0;

    if(so->txt || so_reaquire(so))
      do
	rv = fwrite(&ch,sizeof(unsigned char),(size_t)1,(FILE *)so->txt);
      while(!rv && ferror((FILE *)so->txt) && errno == EINTR);

    return(rv);
}


/*
 * The locale version converts from UTF-8 to user's locale charset
 * before writing the characters.
 */
int
so_file_writec_locale(int c, STORE_S *so)
{
    int rv = 1;
    int i, outchars;
    unsigned char obuf[MAX(MB_LEN_MAX,32)];

    if((outchars = utf8_to_locale(c, &so->cb, obuf, sizeof(obuf))) != 0){
	for(i = 0; i < outchars; i++)
	  if(so_file_writec(obuf[i], so) != 1){
	      rv = 0;
	      break;
	  }
    }

    return(rv);
}


/*
 * get a character from the specified storage object.
 *
 * return 1 on success and 0 on failure
 */
int
so_cs_readc(unsigned char *c, STORE_S *so)
{
    return((so->dp < so->eod) ? *c = *(so->dp)++, 1 : 0);
}


/*
 * The locale version converts from user's locale charset to UTF-8
 * after reading the characters and before returning to the caller.
 */
int
so_cs_readc_locale(unsigned char *c, STORE_S *so)
{
    return(generic_readc_locale(c, so_cs_readc_getchar, so, &so->cb));
}


int
so_cs_readc_getchar(unsigned char *c, void *extraarg)
{
    STORE_S *so;

    so = (STORE_S *) extraarg;
    return(so_cs_readc(c, so));
}


int
so_file_readc(unsigned char *c, STORE_S *so)
{
    int rv = 0;

    if(so->txt || so_reaquire(so))
      do
	rv = fread(c, sizeof(char), (size_t)1, (FILE *)so->txt);
      while(!rv && ferror((FILE *)so->txt) && errno == EINTR);

    return(rv);
}


/*
 * The locale version converts from user's locale charset to UTF-8
 * after reading the characters and before returning to the caller.
 */
int
so_file_readc_locale(unsigned char *c, STORE_S *so)
{
    return(generic_readc_locale(c, so_file_readc_getchar, so, &so->cb));
}


int
so_file_readc_getchar(unsigned char *c, void *extraarg)
{
    STORE_S *so;

    so = (STORE_S *) extraarg;
    return(so_file_readc(c, so));
}


#ifdef _WINDOWS
/*
 * Read unicode characters from windows filesystem and return
 * them as a stream of UTF-8 characters. The stream is assumed
 * opened so that it will know how to put together the unicode.
 */
int
so_file_readc_windows(unsigned char *c, STORE_S *so)
{
    int rv = 0;
    UCS ucs;

    /* already got some from previous call? */
    if(so->cb.cbufend > so->cb.cbuf){
	*c = *so->cb.cbufp;
	so->cb.cbufp++;
	rv++;
	if(so->cb.cbufp >= so->cb.cbufend){
	    so->cb.cbufend = so->cb.cbuf;
	    so->cb.cbufp   = so->cb.cbuf;
	}

	return(rv);
    }

    if(so->txt || so_reaquire(so)){
	/* windows only so second arg is ignored */
	ucs = read_a_wide_char((FILE *) so->txt, NULL);
	rv = (ucs == CCONV_EOF) ? 0 : 1;
    }

    if(rv){
	/*
	 * Now we need to convert the UCS character to UTF-8
	 * and dole out the UTF-8 one char at a time.
	 */
	so->cb.cbufend = utf8_put(so->cb.cbuf, (unsigned long) ucs);
	so->cb.cbufp = so->cb.cbuf;
	if(so->cb.cbufend > so->cb.cbuf){
	    *c = *so->cb.cbufp;
	    so->cb.cbufp++;
	    if(so->cb.cbufp >= so->cb.cbufend){
		so->cb.cbufend = so->cb.cbuf;
		so->cb.cbufp   = so->cb.cbuf;
	    }
	}
	else
	  *c = '?';
    }

    return(rv);
}
#endif /* _WINDOWS */


/*
 * write a string into the specified storage object,
 * expanding if necessary (and cheating if the object
 * happens to be a file!)
 *
 * return 1 on success and 0 on failure
 */
int
so_cs_puts(STORE_S *so, char *s)
{
    int slen = strlen(s);

    if(so->dp + slen >= so->eot){
	register size_t cur_o  = so->dp - (unsigned char *) so->txt;
	register size_t data_o = so->eod - (unsigned char *) so->txt;
	register size_t len   = so->eot - (unsigned char *) so->txt;
	while(len <= cur_o + slen + 1){
	    size_t incr;

	    incr = MAX(len/2, MSIZE_INC);
	    len += incr;
	}

	fs_resize(&so->txt, len * sizeof(char));
	so->dp	 = (unsigned char *)so->txt + cur_o;
	so->eod	 = (unsigned char *)so->txt + data_o;
	so->eot	 = (unsigned char *)so->txt + len;
	memset(so->eod, 0, so->eot - so->eod);
    }

    memcpy(so->dp, s, slen);
    so->dp += slen;
    if(so->dp > so->eod)
      so->eod = so->dp;

    return(1);
}


int
so_cs_puts_locale(STORE_S *so, char *s)
{
    int slen = strlen(s);

    while(slen--)
      if(!so_cs_writec_locale((unsigned char) *s++, so))
	return(0);

    return(1);
}


int
so_file_puts(STORE_S *so, char *s)
{
    int rv = *s ? 0 : 1;

    if(!rv && (so->txt || so_reaquire(so)))
      do
	rv = fwrite(s, strlen(s)*sizeof(char), (size_t)1, (FILE *)so->txt);
      while(!rv && ferror((FILE *)so->txt) && errno == EINTR);

    return(rv);
}


int
so_file_puts_locale(STORE_S *so, char *s)
{
    int slen = strlen(s);

    while(slen--)
      if(!so_file_writec_locale((unsigned char) *s++, so))
	return(0);

    return(1);
}


#ifdef SMIME
/*
 * put a character into the specified storage object,
 * expanding if neccessary
 *
 * return 1 on success and 0 on failure
 */
int
so_bio_writec(int c, STORE_S *so)
{
    if(so->txt && so->src == BioType){
	unsigned char ch[1];
	BIO *b = (BIO *) so->txt;

	ch[0] = (unsigned char) (c & 0xff);

	if(BIO_write(b, ch, 1) >= 1)
	  return(1);
    }

    return(0);
}


int
so_bio_readc(unsigned char *c, STORE_S *so)
{
    if(so->txt && so->src == BioType){
	unsigned char ch[1];
	BIO *b = (BIO *) so->txt;

	if(BIO_read(b, ch, 1) >= 1){
	    *c = ch[0];
	    return(1);
	}
    }

    return(0);
}


/*
 * write a string into the specified storage object,
 * expanding if necessary (and cheating if the object
 * happens to be a file!)
 *
 * return 1 on success and 0 on failure
 */
int
so_bio_puts(STORE_S *so, char *s)
{

    if(so->txt && so->src == BioType){
	BIO *b = (BIO *) so->txt;
	int slen = strlen(s);

	if(BIO_puts(b, s) >= slen)
	  return(1);
    }

    return(1);
}
#endif /* SMIME */


/*
 *
 */
int
so_nputs(STORE_S *so, char *s, long int n)
{
    while(n--)
      if(!so_writec((unsigned char) *s++, so))
	return(0);		/* ERROR putting char ! */

    return(1);
}


/*
 * Position the storage object's pointer to the given offset
 * from the start of the object's data.
 */
int
so_seek(STORE_S *so, long int pos, int orig)
{
    if(so->src == CharStar){
	switch(orig){
	    case 0 :				/* SEEK_SET */
	      return((pos < so->eod - (unsigned char *) so->txt)
		      ? so->dp = (unsigned char *)so->txt + pos, 0 : -1);
	    case 1 :				/* SEEK_CUR */
	      return((pos > 0)
		       ? ((pos < so->eod - so->dp) ? so->dp += pos, 0: -1)
		       : ((pos < 0)
			   ? ((-pos < so->dp - (unsigned char *)so->txt)
			        ? so->dp += pos, 0 : -1)
			   : 0));
	    case 2 :				/* SEEK_END */
	      return((pos > 0)
		       ? -1
		       : ((-pos <= so->eod - (unsigned char *) so->txt)
			    ? so->dp = so->eod + pos, 0 : -1));
	    default :
	      return(-1);
	}
    }
    else if(so->src == ExternalText){
	if(etsod.seek)
	  return((*etsod.seek)(so->txt, pos, orig));

	fatal("programmer botch: unsupported so_seek call");
	/*NOTREACHED*/
	return(0); /* suppress dumb compiler warnings */
    }
#ifdef SMIME
    else if(so->src == BioType){
	BIO *b = (BIO *) so->txt;

	if(b && BIO_method_type(b) != BIO_TYPE_MEM)
	  (void) BIO_reset(b);

	return(0);
    }
#endif /* SMIME */
    else			/* FileStar or TmpFileStar */
      return((so->txt || so_reaquire(so))
		? fseek((FILE *)so->txt,pos,orig)
		: -1);
}


/*
 * Change the given storage object's size to that specified.  If size
 * is less than the current size, the internal pointer is adjusted and
 * all previous data beyond the given size is lost.
 *
 * Returns 0 on failure.
 */
int
so_truncate(STORE_S *so, long int size)
{
    if(so->src == CharStar){
	if(so->eod < (unsigned char *) so->txt + size){	/* alloc! */
	    unsigned char *newtxt = (unsigned char *) so->txt;
	    register size_t len   = so->eot - (unsigned char *) so->txt;

	    while(len <= size)
	      len += MSIZE_INC;		/* need to resize! */

	    if(len > so->eot - (unsigned char *) newtxt){
		fs_resize((void **) &newtxt, len * sizeof(char));
		so->eot = newtxt + len;
		so->eod = newtxt + (so->eod - (unsigned char *) so->txt);
		memset(so->eod, 0, so->eot - so->eod);
	    }

	    so->eod = newtxt + size;
	    so->dp  = newtxt + (so->dp - (unsigned char *) so->txt);
	    so->txt = newtxt;
	}
	else if(so->eod > (unsigned char *) so->txt + size){
	    if(so->dp > (so->eod = (unsigned char *)so->txt + size))
	      so->dp = so->eod;

	    memset(so->eod, 0, so->eot - so->eod);
	}

	return(1);
    }
    else if(so->src == ExternalText){
	if(etsod.truncate)
	  return((*etsod.truncate)(so, (off_t) size));

	fatal("programmer botch: unsupported so_truncate call");
	/*NOTREACHED*/
	return(0); /* suppress dumb compiler warnings */
    }
#ifdef SMIME
    else if(so->src == BioType){
	fatal("programmer botch: unsupported so_truncate call for BioType");
	/*NOTREACHED*/
	return(0); /* suppress dumb compiler warnings */

#ifdef notdef
	long len;
	BIO *b = (BIO *) so->txt;

	if(b){
	    BUF_MEM *biobuf = NULL;

	    BIO_get_mem_ptr(b, &biobuf);
	    if(biobuf){
		BUF_MEM_grow(biobuf, size);
		return(1);
	    }
	}

	return(0);
#endif /* notdef */
    }
#endif /* SMIME */
    else			/* FileStar or TmpFileStar */
      return(fflush((FILE *) so->txt) != EOF
	     && fseek((FILE *) so->txt, size, 0) == 0
	     && ftruncate(fileno((FILE *)so->txt), (off_t) size) == 0);
}


/*
 * Report given storage object's position indicator.
 * Returns 0 on failure.
 */
long
so_tell(STORE_S *so)
{
    if(so->src == CharStar){
	return((long) (so->dp - (unsigned char *) so->txt));
    }
    else if(so->src == ExternalText){
	if(etsod.tell)
	  return((*etsod.tell)(so));

	fatal("programmer botch: unsupported so_tell call");
	/*NOTREACHED*/
	return(0); /* suppress dumb compiler warnings */
    }
#ifdef SMIME
    else if(so->src == BioType){
	fatal("programmer botch: unsupported so_tell call for BioType");
	/*NOTREACHED*/
	return(0); /* suppress dumb compiler warnings */
    }
#endif /* SMIME */
    else			/* FileStar or TmpFileStar */
      return(ftell((FILE *) so->txt));
}


/*
 * so_attr - hook to hang random attributes onto the storage object
 */
char *
so_attr(STORE_S *so, char *key, char *value)
{
    if(so && key){
	if(value){
	    PARAMETER **pp = &so->attr;

	    while(1){
		if(*pp){
		    if((*pp)->attribute && !strcmp(key, (*pp)->attribute)){
			if((*pp)->value)
			  fs_give((void **)&(*pp)->value);

			break;
		    }

		    pp = &(*pp)->next;
		}
		else{
		    *pp = mail_newbody_parameter();
		    (*pp)->attribute = cpystr(key);
		    break;
		}
	    }

	    return((*pp)->value = cpystr(value));
	}
	else{
	    PARAMETER *p;

	    for(p = so->attr; p; p = p->next)
	      if(p->attribute && !strcmp(key, p->attribute))
		return(p->value);
	}
    }

    return(NULL);
}


/*
 * so_release - a rather misnamed function.  the idea is to release
 *              what system resources we can (e.g., open files).
 *              while maintaining a reference to it.
 *              it's up to the functions that deal with this object
 *              next to re-aquire those resources.
 */
int
so_release(STORE_S *so)
{
    if(so->txt && so->name && (so->src == FileStar || so->src == TmpFileStar)){
	if(fget_pos((FILE *)so->txt, (fpos_t *)&(so->used)) == 0){
	    fclose((FILE *)so->txt);		/* free the handle! */
	    so->txt = NULL;
	}
    }

    return(1);
}


/*
 * so_reaquire - get any previously released system resources we
 *               may need for the given storage object.
 *       NOTE: at the moment, only FILE * types of objects are
 *             effected, so it only needs to be called before
 *             references to them.
 *
 */
int
so_reaquire(STORE_S *so)
{
    int   rv = 1;

    if(!so->txt && (so->src == FileStar || so->src == TmpFileStar)){
	if(!(so->txt = so_file_open(so))){
	    q_status_message2(SM_ORDER,3,5, "ERROR reopening %.200s : %.200s",
			      so->name, error_description(errno));
	    rv = 0;
	}
	else if(fset_pos((FILE *)so->txt, (fpos_t *)&(so->used))){
	    q_status_message2(SM_ORDER, 3, 5,
			      "ERROR positioning in %.200s : %.200s",
			      so->name, error_description(errno));
	    rv = 0;
	}
    }

    return(rv);
}


/*
 * so_text - return a pointer to the text the store object passed
 */
void *
so_text(STORE_S *so)
{
    return((so) ? so->txt : NULL);
}


/*
 * Similar to fgets but reading from a storage object.
 */
char *
so_fgets(STORE_S *so, char *s, size_t size)
{
    unsigned char c;
    char *p = s;

    while(--size > 0 && so_readc(&c, so) > 0){
	*p++ = (char) c;
	if(c == '\n')
	  break;
    }

    *p = '\0';

    return((p>s) ? s : NULL);
}
