#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: filter.c 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $";
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
     filter.c

     This code provides a generalized, flexible way to allow
     piping of data thru filters.  Each filter is passed a structure
     that it will use to hold its static data while it operates on
     the stream of characters that are passed to it.  After processing
     it will either return or call the next filter in
     the pipe with any character (or characters) it has ready to go. This
     means some terminal type of filter has to be the last in the
     chain (i.e., one that writes the passed char someplace, but doesn't
     call another filter).

     See below for more details.

     The motivation is to handle MIME decoding, richtext conversion,
     iso_code stripping and anything else that may come down the
     pike (e.g., PEM) in an elegant fashion.  mikes (920811)

   TODO:
       reasonable error handling

  ====*/


#include "../pith/headers.h"
#include "../pith/filter.h"
#include "../pith/conf.h"
#include "../pith/store.h"
#include "../pith/color.h"
#include "../pith/escapes.h"
#include "../pith/pipe.h"
#include "../pith/status.h"
#include "../pith/string.h"
#include "../pith/util.h"
#include "../pith/url.h"
#include "../pith/init.h"
#include "../pith/help.h"
#include "../pico/keydefs.h"

#ifdef _WINDOWS
#include "../pico/osdep/mswin.h"
#endif


/*
 * Internal prototypes
 */
int	gf_so_writec(int);
int	gf_so_readc(unsigned char *);
int	gf_freadc(unsigned char *);
int	gf_freadc_locale(unsigned char *);
int	gf_freadc_getchar(unsigned char *, void *);
int	gf_fwritec(int);
int	gf_fwritec_locale(int);
#ifdef _WINDOWS
int	gf_freadc_windows(unsigned char *);
#endif /* _WINDOWS */
int	gf_preadc(unsigned char *);
int	gf_preadc_locale(unsigned char *);
int	gf_preadc_getchar(unsigned char *, void *);
int	gf_pwritec(int);
int	gf_pwritec_locale(int);
int	gf_sreadc(unsigned char *);
int	gf_sreadc_locale(unsigned char *);
int	gf_sreadc_getchar(unsigned char *, void *);
int	gf_swritec(int);
int	gf_swritec_locale(int);
void	gf_terminal(FILTER_S *, int);
void    gf_error(char *);
char   *gf_filter_puts(char *);
void	gf_filter_eod(void);

void	gf_8bit_put(FILTER_S *, int);



/*
 * System specific options
 */
#ifdef _WINDOWS
#define CRLF_NEWLINES
#endif


/*
 * Hooks for callers to adjust behavior
 */
char *(*pith_opt_pretty_var_name)(char *);
char *(*pith_opt_pretty_feature_name)(char *, int);


/*
 * pointer to first function in a pipe, and pointer to last filter
 */
FILTER_S         *gf_master = NULL;
static	gf_io_t   last_filter;
static	char     *gf_error_string;
static	long	  gf_byte_count;
static	jmp_buf   gf_error_state;


#define	GF_NOOP		0x01		/* flags used by generalized */
#define GF_EOD		0x02		/* filters                   */
#define GF_DATA		0x04		/* See filter.c for more     */
#define GF_ERROR	0x08		/* details                   */
#define GF_RESET	0x10


/*
 * A list of states used by the various filters.  Reused in many filters.
 */
#define	DFL	0
#define	EQUAL	1
#define	HEX	2
#define	WSPACE	3
#define	CCR	4
#define	CLF	5
#define	TOKEN	6
#define	TAG	7
#define	HANDLE	8
#define	HDATA	9
#define	ESC	10
#define	ESCDOL	11
#define	ESCPAR	12
#define	EUC	13
#define	BOL	14
#define	FL_QLEV	15
#define	FL_STF	16
#define	FL_SIG	17
#define	STOP_DECODING	18
#define	SPACECR	19



/*
 * Macros to reduce function call overhead associated with calling
 * each filter for each byte filtered, and to minimize filter structure
 * dereferences.  NOTE: "queuein" has to do with putting chars into the
 * filter structs data queue.  So, writing at the queuein offset is
 * what a filter does to pass processed data out of itself.  Ditto for
 * queueout.  This explains the FI --> queueout init stuff below.
 */
#define	GF_QUE_START(F)	(&(F)->queue[0])
#define	GF_QUE_END(F)	(&(F)->queue[GF_MAXBUF - 1])

#define	GF_IP_INIT(F)	ip  = (F) ? &(F)->queue[(F)->queuein] : NULL
#define	GF_IP_INIT_GLO(F)  (*ipp)  = (F) ? &(F)->queue[(F)->queuein] : NULL
#define	GF_EIB_INIT(F)	eib = (F) ? GF_QUE_END(F) : NULL
#define	GF_EIB_INIT_GLO(F)  (*eibp) = (F) ? GF_QUE_END(F) : NULL
#define	GF_OP_INIT(F)	op  = (F) ? &(F)->queue[(F)->queueout] : NULL
#define	GF_EOB_INIT(F)	eob = (F) ? &(F)->queue[(F)->queuein] : NULL

#define	GF_IP_END(F)	(F)->queuein  = ip - GF_QUE_START(F)
#define	GF_IP_END_GLO(F)  (F)->queuein  = (unsigned char *)(*ipp) - (unsigned char *)GF_QUE_START(F)
#define	GF_OP_END(F)	(F)->queueout = op - GF_QUE_START(F)

#define	GF_INIT(FI, FO)	unsigned char *GF_OP_INIT(FI);	 \
			unsigned char *GF_EOB_INIT(FI); \
			unsigned char *GF_IP_INIT(FO);  \
			unsigned char *GF_EIB_INIT(FO);

#define	GF_CH_RESET(F)	(op = eob = GF_QUE_START(F), \
					    (F)->queueout = (F)->queuein = 0)

#define	GF_END(FI, FO)	(GF_OP_END(FI), GF_IP_END(FO))

#define	GF_FLUSH(F)	((GF_IP_END(F), (*(F)->f)((F), GF_DATA), \
			       GF_IP_INIT(F), GF_EIB_INIT(F)) ? 1 : 0)
#define	GF_FLUSH_GLO(F)	((GF_IP_END_GLO(F), (*(F)->f)((F), GF_DATA), \
			       GF_IP_INIT_GLO(F), GF_EIB_INIT_GLO(F)) ? 1 : 0)

#define	GF_PUTC(F, C)	((int)(*ip++ = (C), (ip >= eib) ? GF_FLUSH(F) : 1))
#define	GF_PUTC_GLO(F, C) ((int)(*(*ipp)++ = (C), ((*ipp) >= (*eibp)) ? GF_FLUSH_GLO(F) : 1))

/*
 * Introducing the *_GLO macros for use in splitting the big macros out
 * into functions (wrap_flush, wrap_eol).  The reason we need a
 * separate macro is because of the vars ip, eib, op, and eob, which are
 * set up locally in a call to GF_INIT.  To preserve these variables
 * in the new functions, we now pass pointers to these four vars.  Each
 * of these new functions expects the presence of pointer vars
 * ipp, eibp, opp, and eobp.  
 */

#define	GF_GETC(F, C)	((op < eob) ? (((C) = *op++), 1) : GF_CH_RESET(F))

#define GF_COLOR_PUTC(F, C) {                                            \
                              char *p;                                   \
                              char cb[RGBLEN+1];                         \
                              GF_PUTC_GLO((F)->next, TAG_EMBED);         \
                              GF_PUTC_GLO((F)->next, TAG_FGCOLOR);       \
			      strncpy(cb, color_to_asciirgb((C)->fg), sizeof(cb)); \
                              cb[sizeof(cb)-1] = '\0';                   \
			      p = cb;                                    \
                              for(; *p; p++)                             \
                                GF_PUTC_GLO((F)->next, *p);              \
                              GF_PUTC_GLO((F)->next, TAG_EMBED);         \
                              GF_PUTC_GLO((F)->next, TAG_BGCOLOR);       \
			      strncpy(cb, color_to_asciirgb((C)->bg), sizeof(cb)); \
                              cb[sizeof(cb)-1] = '\0';                   \
			      p = cb;                                    \
                              for(; *p; p++)                             \
                                GF_PUTC_GLO((F)->next, *p);              \
                            }

/*
 * Generalized getc and putc routines.  provided here so they don't
 * need to be re-done elsewhere to
 */

/*
 * pointers to objects to be used by the generic getc and putc
 * functions
 */
static struct gf_io_struct {
    FILE          *file;
    PIPE_S        *pipe;
    char          *txtp;
    unsigned long  n;
    int            flags;
    CBUF_S         cb;
} gf_in, gf_out;

#define	GF_SO_STACK	struct gf_so_stack
static GF_SO_STACK {
    STORE_S	*so;
    GF_SO_STACK *next;
} *gf_so_in, *gf_so_out;



/*
 * Returns 1 if pc will write into a PicoText object, 0 otherwise.
 *
 * The purpose of this routine is so that we can avoid setting SIGALARM
 * when writing into a PicoText object, because that type of object uses
 * unprotected malloc/free/realloc, which can't be interrupted.
 */
int
pc_is_picotext(gf_io_t pc)
{
    return(pc == gf_so_writec && gf_so_out && gf_so_out->so &&
	   gf_so_out->so->src == ExternalText);
}



/*
 * setup to use and return a pointer to the generic
 * getc function
 */
void
gf_set_readc(gf_io_t *gc, void *txt, long unsigned int len, SourceType src, int flags)
{
    gf_in.n = len;
    gf_in.flags = flags;
    gf_in.cb.cbuf[0] = '\0';
    gf_in.cb.cbufp   = gf_in.cb.cbuf;
    gf_in.cb.cbufend = gf_in.cb.cbuf;

    if(src == FileStar){
	gf_in.file = (FILE *)txt;
	fseek(gf_in.file, 0L, 0);
#ifdef _WINDOWS
	*gc = (flags & READ_FROM_LOCALE) ? gf_freadc_windows
					 : gf_freadc;
#else /* UNIX */
	*gc = (flags & READ_FROM_LOCALE) ? gf_freadc_locale
					 : gf_freadc;
#endif /* UNIX */
    }
    else if(src == PipeStar){
	gf_in.pipe = (PIPE_S *)txt;
	*gc = gf_preadc;
	*gc = (flags & READ_FROM_LOCALE) ? gf_preadc_locale
					 : gf_preadc;
    }
    else{
	gf_in.txtp = (char *)txt;
	*gc = (flags & READ_FROM_LOCALE) ? gf_sreadc_locale
					 : gf_sreadc;
    }
}


/*
 * setup to use and return a pointer to the generic
 * putc function
 */
void
gf_set_writec(gf_io_t *pc, void *txt, long unsigned int len, SourceType src, int flags)
{
    gf_out.n = len;
    gf_out.flags = flags;
    gf_out.cb.cbuf[0] = '\0';
    gf_out.cb.cbufp   = gf_out.cb.cbuf;
    gf_out.cb.cbufend = gf_out.cb.cbuf;

    if(src == FileStar){
	gf_out.file = (FILE *)txt;
#ifdef _WINDOWS
	*pc =                             gf_fwritec;
#else /* UNIX */
	*pc = (flags & WRITE_TO_LOCALE) ? gf_fwritec_locale
					: gf_fwritec;
#endif /* UNIX */
    }
    else if(src == PipeStar){
	gf_out.pipe = (PIPE_S *)txt;
	*pc = (flags & WRITE_TO_LOCALE) ? gf_pwritec_locale
					: gf_pwritec;
    }
    else{
	gf_out.txtp = (char *)txt;
	*pc = (flags & WRITE_TO_LOCALE) ? gf_swritec_locale
					: gf_swritec;
    }
}


/*
 * setup to use and return a pointer to the generic
 * getc function
 */
void
gf_set_so_readc(gf_io_t *gc, STORE_S *so)
{
    GF_SO_STACK *sp = (GF_SO_STACK *) fs_get(sizeof(GF_SO_STACK));

    sp->so   = so;
    sp->next = gf_so_in;
    gf_so_in = sp;
    *gc      = gf_so_readc;
}


void
gf_clear_so_readc(STORE_S *so)
{
    GF_SO_STACK *sp;

    if((sp = gf_so_in) != NULL){
	if(so == sp->so){
	    gf_so_in = gf_so_in->next;
	    fs_give((void **) &sp);
	}
	else
	  panic("Programmer botch: Can't unstack store readc");
    }
    else
      panic("Programmer botch: NULL store clearing store readc");
}


/*
 * setup to use and return a pointer to the generic
 * putc function
 */
void
gf_set_so_writec(gf_io_t *pc, STORE_S *so)
{
    GF_SO_STACK *sp = (GF_SO_STACK *) fs_get(sizeof(GF_SO_STACK));

    sp->so    = so;
    sp->next  = gf_so_out;
    gf_so_out = sp;
    *pc       = gf_so_writec;
}


void
gf_clear_so_writec(STORE_S *so)
{
    GF_SO_STACK *sp;

    if((sp = gf_so_out) != NULL){
	if(so == sp->so){
	    gf_so_out = gf_so_out->next;
	    fs_give((void **) &sp);
	}
	else
	  panic("Programmer botch: Can't unstack store writec");
    }
    else
      panic("Programmer botch: NULL store clearing store writec");
}


/*
 * put the character to the object previously defined
 */
int
gf_so_writec(int c)
{
    return(so_writec(c, gf_so_out->so));
}


/*
 * get a character from an object previously defined
 */
int
gf_so_readc(unsigned char *c)
{
    return(so_readc(c, gf_so_in->so));
}


/* get a character from a file */
/* assumes gf_out struct is filled in */
int
gf_freadc(unsigned char *c)
{
    int rv = 0;

    do {
	errno = 0;
	clearerr(gf_in.file);
	rv = fread(c, sizeof(unsigned char), (size_t)1, gf_in.file);
    } while(!rv && ferror(gf_in.file) && errno == EINTR);

    return(rv);
}


int
gf_freadc_locale(unsigned char *c)
{
    return(generic_readc_locale(c, gf_freadc_getchar, (void *) gf_in.file, &gf_in.cb));
}


/*
 * This is just to make it work with generic_readc_locale.
 */
int
gf_freadc_getchar(unsigned char *c, void *extraarg)
{
    FILE *file;
    int rv = 0;

    file = (FILE *) extraarg;

    do {
	errno = 0;
	clearerr(file);
	rv = fread(c, sizeof(unsigned char), (size_t)1, file);
    } while(!rv && ferror(file) && errno == EINTR);

    return(rv);
}


/*
 * Put a character to a file.
 * Assumes gf_out struct is filled in.
 * Returns 1 on success, <= 0 on failure.
 */
int
gf_fwritec(int c)
{
    unsigned char ch = (unsigned char)c;
    int rv = 0;

    do
      rv = fwrite(&ch, sizeof(unsigned char), (size_t)1, gf_out.file);
    while(!rv && ferror(gf_out.file) && errno == EINTR);

    return(rv);
}


/*
 * The locale version converts from UTF-8 to user's locale charset
 * before writing the characters.
 */
int
gf_fwritec_locale(int c)
{
    int rv = 1;
    int i, outchars;
    unsigned char obuf[MAX(MB_LEN_MAX,32)];

    if((outchars = utf8_to_locale(c, &gf_out.cb, obuf, sizeof(obuf))) != 0){
	for(i = 0; i < outchars; i++)
	  if(gf_fwritec(obuf[i]) != 1){
	      rv = 0;
	      break;
	  }
    }

    return(rv);
}


#ifdef _WINDOWS
/*
 * Read unicode characters from windows filesystem and return
 * them as a stream of UTF-8 characters. The stream is assumed
 * opened so that it will know how to put together the unicode.
 *
 * (This is totally untested, copied loosely from so_file_readc_windows
 *  which may or may not be appropriate.)
 */
int
gf_freadc_windows(unsigned char *c)
{
    int rv = 0;
    UCS ucs;

    /* already got some from previous call? */
    if(gf_in.cb.cbufend > gf_in.cb.cbuf){
	*c = *gf_in.cb.cbufp;
	gf_in.cb.cbufp++;
	rv++;
	if(gf_in.cb.cbufp >= gf_in.cb.cbufend){
	    gf_in.cb.cbufend = gf_in.cb.cbuf;
	    gf_in.cb.cbufp   = gf_in.cb.cbuf;
	}

	return(rv);
    }

    if(gf_in.file){
	/* windows only so second arg is ignored */
	ucs = read_a_wide_char(gf_in.file, NULL);
	rv = (ucs == CCONV_EOF) ? 0 : 1;
    }

    if(rv){
	/*
	 * Now we need to convert the UCS character to UTF-8
	 * and dole out the UTF-8 one char at a time.
	 */
	gf_in.cb.cbufend = utf8_put(gf_in.cb.cbuf, (unsigned long) ucs);
	gf_in.cb.cbufp = gf_in.cb.cbuf;
	if(gf_in.cb.cbufend > gf_in.cb.cbuf){
	    *c = *gf_in.cb.cbufp;
	    gf_in.cb.cbufp++;
	    if(gf_in.cb.cbufp >= gf_in.cb.cbufend){
		gf_in.cb.cbufend = gf_in.cb.cbuf;
		gf_in.cb.cbufp   = gf_in.cb.cbuf;
	    }
	}
	else
	  *c = '?';
    }

    return(rv);
}
#endif /* _WINDOWS */


int
gf_preadc(unsigned char *c)
{
    return(pipe_readc(c, gf_in.pipe));
}


int
gf_preadc_locale(unsigned char *c)
{
    return(generic_readc_locale(c, gf_preadc_getchar, (void *) gf_in.pipe, &gf_in.cb));
}


/*
 * This is just to make it work with generic_readc_locale.
 */
int
gf_preadc_getchar(unsigned char *c, void *extraarg)
{
    PIPE_S *pipe;

    pipe = (PIPE_S *) extraarg;

    return(pipe_readc(c, pipe));
}


/*
 * Put a character to a pipe.
 * Assumes gf_out struct is filled in.
 * Returns 1 on success, <= 0 on failure.
 */
int
gf_pwritec(int c)
{
    return(pipe_writec(c, gf_out.pipe));
}


/*
 * The locale version converts from UTF-8 to user's locale charset
 * before writing the characters.
 */
int
gf_pwritec_locale(int c)
{
    int rv = 1;
    int i, outchars;
    unsigned char obuf[MAX(MB_LEN_MAX,32)];

    if((outchars = utf8_to_locale(c, &gf_out.cb, obuf, sizeof(obuf))) != 0){
	for(i = 0; i < outchars; i++)
	  if(gf_pwritec(obuf[i]) != 1){
	      rv = 0;
	      break;
	  }
    }

    return(rv);
}


/* get a character from a string, return nonzero if things OK */
/* assumes gf_out struct is filled in */
int
gf_sreadc(unsigned char *c)
{
    return((gf_in.n) ? *c = *(gf_in.txtp)++, gf_in.n-- : 0);
}


int
gf_sreadc_locale(unsigned char *c)
{
    return(generic_readc_locale(c, gf_sreadc_getchar, NULL, &gf_in.cb));
}


int
gf_sreadc_getchar(unsigned char *c, void *extraarg)
{
    /*
     * extraarg is ignored and gf_sreadc just uses globals instead.
     * That's ok as long as we don't call it more than once at a time.
     */
    return(gf_sreadc(c));
}


/*
 * Put a character to a string.
 * Assumes gf_out struct is filled in.
 * Returns 1 on success, <= 0 on failure.
 */
int
gf_swritec(int c)
{
    return((gf_out.n) ? *(gf_out.txtp)++ = c, gf_out.n-- : 0);
}


/*
 * The locale version converts from UTF-8 to user's locale charset
 * before writing the characters.
 */
int
gf_swritec_locale(int c)
{
    int rv = 1;
    int i, outchars;
    unsigned char obuf[MAX(MB_LEN_MAX,32)];

    if((outchars = utf8_to_locale(c, &gf_out.cb, obuf, sizeof(obuf))) != 0){
	for(i = 0; i < outchars; i++)
	  if(gf_swritec(obuf[i]) != 1){
	      rv = 0;
	      break;
	  }
    }

    return(rv);
}


/*
 * output the given string with the given function
 */
int
gf_puts(register char *s, gf_io_t pc)
{
    while(*s != '\0')
      if(!(*pc)((unsigned char)*s++))
	return(0);		/* ERROR putting char ! */

    return(1);
}


/*
 * output the given string with the given function
 */
int
gf_nputs(register char *s, long int n, gf_io_t pc)
{
    while(n--)
      if(!(*pc)((unsigned char)*s++))
	return(0);		/* ERROR putting char ! */

    return(1);
}


/*
 * Read a stream of multi-byte characters from the
 * user's locale charset and return a stream of
 * UTF-8 characters, one at a time. The input characters
 * are obtained by using the get_a_char function.
 *
 * Args        c -- the returned octet
 *    get_a_char -- function to get a single octet of the multibyte
 *                  character. The first arg of that function is the
 *                  returned value and the second arg is for the
 *                  functions use. The second arg is replaced with
 *                  extraarg when it is called.
 *      extraarg -- The second arg to get_a_char.
 *            cb -- Storage area for state between calls to this func.
 */
int
generic_readc_locale(unsigned char *c,
		     int (*get_a_char)(unsigned char *, void *),
		     void *extraarg,
		     CBUF_S *cb)
{
    unsigned long octets_so_far = 0, remaining_octets;
    unsigned char *inputp;
    unsigned char ch;
    UCS ucs;
    unsigned char inputbuf[20];
    int rv = 0;
    int got_one = 0;

    /* already got some from previous call? */
    if(cb->cbufend > cb->cbuf){
	*c = *cb->cbufp;
	cb->cbufp++;
	rv++;
	if(cb->cbufp >= cb->cbufend){
	    cb->cbufend = cb->cbuf;
	    cb->cbufp   = cb->cbuf;
	}

	return(rv);
    }

    memset(inputbuf, 0, sizeof(inputbuf));
    if((*get_a_char)(&ch, extraarg) == 0)
      return(0);

    inputbuf[octets_so_far++] = ch;

    while(!got_one){
	remaining_octets = octets_so_far;
	inputp = inputbuf;
	ucs = mbtow(ps_global->input_cs, &inputp, &remaining_octets);
	switch(ucs){
	  case CCONV_BADCHAR:
	    return(rv);
	  
	  case CCONV_NEEDMORE:
/*
 * Do we need to do something with the characters we've
 * collected that don't form a valid UCS character?
 * Probably need to try discarding them one at a time
 * from the front instead of just throwing them all out.
 */
	    if(octets_so_far >= sizeof(inputbuf))
	      return(rv);

	    if((*get_a_char)(&ch, extraarg) == 0)
	      return(rv);

	    inputbuf[octets_so_far++] = ch;
	    break;

	  default:
	    /* got a good UCS-4 character */
	    got_one++;
	    break;
	}
    }

    /*
     * Now we need to convert the UCS character to UTF-8
     * and dole out the UTF-8 one char at a time.
     */
    rv++;
    cb->cbufend = utf8_put(cb->cbuf, (unsigned long) ucs);
    cb->cbufp = cb->cbuf;
    if(cb->cbufend > cb->cbuf){
	*c = *cb->cbufp;
	cb->cbufp++;
	if(cb->cbufp >= cb->cbufend){
	    cb->cbufend = cb->cbuf;
	    cb->cbufp   = cb->cbuf;
	}
    }
    else
      *c = '?';

    return(rv);
}


/*
 * Start of generalized filter routines
 */

/*
 * initializing function to make sure list of filters is empty.
 */
void
gf_filter_init(void)
{
    FILTER_S *flt, *fltn = gf_master;

    while((flt = fltn) != NULL){	/* free list of old filters */
	fltn = flt->next;
	fs_give((void **)&flt);
    }

    gf_master = NULL;
    gf_error_string = NULL;		/* clear previous errors */
    gf_byte_count = 0L;			/* reset counter */
}



/*
 * link the given filter into the filter chain
 */
void
gf_link_filter(filter_t f, void *data)
{
    FILTER_S *new, *tail;

#ifdef CRLF_NEWLINES
    /*
     * If the system's native EOL convention is CRLF, then there's no
     * point in passing data thru a filter that's not doing anything
     */
    if(f == gf_nvtnl_local || f == gf_local_nvtnl)
      return;
#endif

    new = (FILTER_S *)fs_get(sizeof(FILTER_S));
    memset(new, 0, sizeof(FILTER_S));

    new->f = f;				/* set the function pointer     */
    new->opt = data;			/* set any optional parameter data */
    (*f)(new, GF_RESET);		/* have it setup initial state  */

    if((tail = gf_master) != NULL){	/* or add it to end of existing  */
	while(tail->next)		/* list  */
	  tail = tail->next;

	tail->next = new;
    }
    else				/* attach new struct to list    */
      gf_master = new;			/* start a new list */
}


/*
 * terminal filter, doesn't call any other filters, typically just does
 * something with the output
 */
void
gf_terminal(FILTER_S *f, int flg)
{
    if(flg == GF_DATA){
	GF_INIT(f, f);

	while(op < eob)
	  if((*last_filter)(*op++) <= 0) /* generic terminal filter */
	    gf_error(errno ? error_description(errno) : "Error writing pipe");

	GF_CH_RESET(f);
    }
    else if(flg == GF_RESET)
      errno = 0;			/* prepare for problems */
}


/*
 * set some outside gf_io_t function to the terminal function
 * for example: a function to write a char to a file or into a buffer
 */
void
gf_set_terminal(gf_io_t f)			/* function to set generic filter */
              
{
    last_filter = f;
}


/*
 * common function for filter's to make it known that an error
 * has occurred.  Jumps back to gf_pipe with error message.
 */
void
gf_error(char *s)
{
    /* let the user know the error passed in s */
    gf_error_string = s;
    longjmp(gf_error_state, 1);
}


/*
 * The routine that shoves each byte through the chain of
 * filters.  It sets up error handling, and the terminal function.
 * Then loops getting bytes with the given function, and passing
 * it on to the first filter in the chain.
 */
char *
gf_pipe(gf_io_t gc, gf_io_t pc)
                   			/* how to get a character */
{
    unsigned char c;

    dprint((4, "-- gf_pipe: "));

    /*
     * set up for any errors a filter may encounter
     */
    if(setjmp(gf_error_state)){
	dprint((4, "ERROR: %s\n",
		   gf_error_string ? gf_error_string : "NULL"));
	return(gf_error_string); 	/*  */
    }

    /*
     * set and link in the terminal filter
     */
    gf_set_terminal(pc);
    gf_link_filter(gf_terminal, NULL);

    /*
     * while there are chars to process, send them thru the pipe.
     * NOTE: it's necessary to enclose the loop below in a block
     * as the GF_INIT macro calls some automatic var's into
     * existence.  It can't be placed at the start of gf_pipe
     * because its useful for us to be called without filters loaded
     * when we're just being used to copy bytes between storage
     * objects.
     */
    {
	GF_INIT(gf_master, gf_master);

	while((*gc)(&c)){
	    gf_byte_count++;

#ifdef	_WINDOWS
	    if(!(gf_byte_count & 0x3ff))
	      /* Under windows we yield to allow event processing.
	       * Progress display is handled throught the alarm()
	       * mechinism.
	       */
	      mswin_yield ();
#endif

	    GF_PUTC(gf_master, c & 0xff);
	}

	/*
	 * toss an end-of-data marker down the pipe to give filters
	 * that have any buffered data the opportunity to dump it
	 */
	(void) GF_FLUSH(gf_master);
	(*gf_master->f)(gf_master, GF_EOD);
    }

    dprint((4, "done.\n"));
    return(NULL);			/* everything went OK */
}


/*
 * return the number of bytes piped so far
 */
long
gf_bytes_piped(void)
{
    return(gf_byte_count);
}


/*
 * filter the given input with the given command
 *
 *  Args: cmd -- command string to execute
 *	prepend -- string to prepend to filtered input
 *	source_so -- storage object containing data to be filtered
 *	pc -- function to write filtered output with
 *	aux_filters -- additional filters to pass data thru after "cmd"
 *
 *  Returns: NULL on sucess, reason for failure (not alloc'd!) on error
 */
char *
gf_filter(char *cmd, char *prepend, STORE_S *source_so, gf_io_t pc,
	  FILTLIST_S *aux_filters, int disable_reset,
	  void (*pipecb_f)(PIPE_S *, int, void *))
{
    unsigned char c, obuf[MAX(MB_LEN_MAX,32)];
    int	     flags, outchars, i;
    char   *errstr = NULL, buf[MAILTMPLEN];
    PIPE_S *fpipe;
    CBUF_S  cb;
#ifdef	NON_BLOCKING_IO
    int     n;
#endif

    dprint((4, "so_filter: \"%s\"\n", cmd ? cmd : "?"));

    gf_filter_init();
    
    /*
     * After coming back from user's pipe command we need to convert
     * the output from the pipe back to UTF-8.
     */
    if(ps_global->keyboard_charmap && strucmp("UTF-8", ps_global->keyboard_charmap))
      gf_link_filter(gf_utf8, gf_utf8_opt(ps_global->keyboard_charmap));

    for( ; aux_filters && aux_filters->filter; aux_filters++)
      gf_link_filter(aux_filters->filter, aux_filters->data);

    gf_set_terminal(pc);
    gf_link_filter(gf_terminal, NULL);

    cb.cbuf[0] = '\0';
    cb.cbufp   = cb.cbuf;
    cb.cbufend = cb.cbuf;

    /*
     * Spawn filter feeding it data, and reading what it writes.
     */
    so_seek(source_so, 0L, 0);
    flags = PIPE_WRITE | PIPE_READ | PIPE_NOSHELL |
			    (!disable_reset ? PIPE_RESET : 0);

    if((fpipe = open_system_pipe(cmd, NULL, NULL, flags, 0, pipecb_f, pipe_report_error)) != NULL){

#ifdef	NON_BLOCKING_IO

	if(fcntl(fileno(fpipe->in.f), F_SETFL, NON_BLOCKING_IO) == -1)
	  errstr = "Can't set up non-blocking IO";

	if(prepend && (fputs(prepend, fpipe->out.f) == EOF
		       || fputc('\n', fpipe->out.f) == EOF))
	  errstr = error_description(errno);

	while(!errstr){
	    /* if the pipe can't hold a K we're sunk (too bad PIPE_MAX
	     * isn't ubiquitous ;).
	     */
	    for(n = 0; !errstr && fpipe->out.f && n < 1024; n++)
	      if(!so_readc(&c, source_so)){
		  fclose(fpipe->out.f);
		  fpipe->out.f = NULL;
	      }
	      else{
		  /*
		   * Got a UTF-8 character from source_so.
		   * We need to convert it to the user's locale charset
		   * and then send the result to the pipe.
		   */
		  if((outchars = utf8_to_locale((int) c, &cb, obuf, sizeof(obuf))) != 0)
		    for(i = 0; i < outchars && !errstr; i++)
		      if(fputc(obuf[i], fpipe->out.f) == EOF)
		        errstr = error_description(errno);
	      }

	    /*
	     * Note: We clear errno here and test below, before ferror,
	     *	     because *some* stdio implementations consider
	     *	     EAGAIN and EWOULDBLOCK equivalent to EOF...
	     */
	    errno = 0;
	    clearerr(fpipe->in.f); /* fix from <cananian@cananian.mit.edu> */

	    while(!errstr && fgets(buf, sizeof(buf), fpipe->in.f))
	      errstr = gf_filter_puts(buf);

	    /* then fgets failed! */
	    if(!errstr && !(errno == EAGAIN || errno == EWOULDBLOCK)){
		if(feof(fpipe->in.f))		/* nothing else interesting! */
		  break;
		else if(ferror(fpipe->in.f))	/* bummer. */
		  errstr = error_description(errno);
	    }
	    else if(errno == EAGAIN || errno == EWOULDBLOCK)
	      clearerr(fpipe->in.f);
	}

#else /* !NON_BLOCKING_IO */

	if(prepend && (pipe_puts(prepend, fpipe) == EOF
		       || pipe_putc('\n', fpipe) == EOF))
	  errstr = error_description(errno);

	/*
	 * Well, do the best we can, and hope the pipe we're writing
	 * doesn't fill up before we start reading...
	 */
	while(!errstr && so_readc(&c, source_so))
	  if((outchars = utf8_to_locale((int) c, &cb, obuf, sizeof(obuf))) != 0)
	    for(i = 0; i < outchars && !errstr; i++)
	      if(pipe_putc(obuf[i], fpipe) == EOF)
		errstr = error_description(errno);

	if(pipe_close_write(fpipe))
	  errstr = _("Pipe command returned error.");

	while(!errstr && pipe_gets(buf, sizeof(buf), fpipe))
	  errstr = gf_filter_puts(buf);

#endif /* !NON_BLOCKING_IO */

	if(close_system_pipe(&fpipe, NULL, pipecb_f) && !errstr)
	  errstr = _("Pipe command returned error.");

	gf_filter_eod();
    }
    else
      errstr = _("Error setting up pipe command.");

    return(errstr);
}


/*
 * gf_filter_puts - write the given string down the filter's pipe
 */
char *
gf_filter_puts(register char *s)
{
    GF_INIT(gf_master, gf_master);

    /*
     * set up for any errors a filter may encounter
     */
    if(setjmp(gf_error_state)){
	dprint((4, "ERROR: gf_filter_puts: %s\n",
		   gf_error_string ? gf_error_string : "NULL"));
	return(gf_error_string);
    }

    while(*s)
      GF_PUTC(gf_master, (*s++) & 0xff);

    GF_END(gf_master, gf_master);
    return(NULL);
}


/*
 * gf_filter_eod - flush pending data filter's input queue and deliver
 *		   the GF_EOD marker.
 */
void
gf_filter_eod(void)
{
    GF_INIT(gf_master, gf_master);
    (void) GF_FLUSH(gf_master);
    (*gf_master->f)(gf_master, GF_EOD);
}


/*
 * END OF PIPE SUPPORT ROUTINES, BEGINNING OF FILTERS
 *
 * Filters MUST use the specified interface (pointer to filter
 * structure, the unsigned character buffer in that struct, and a
 * cmd flag), and pass each resulting octet to the next filter in the
 * chain.  Only the terminal filter need not call another filter.
 * As a result, filters share a pretty general structure.
 * Typically three main conditionals separate initialization from
 * data from end-of-data command processing.
 *
 * Lastly, being character-at-a-time, they're a little more complex
 * to write than filters operating on buffers because some state
 * must typically be kept between characters.  However, for a
 * little bit of complexity here, much convenience is gained later
 * as they can be arbitrarily chained together at run time and
 * consume few resources (especially memory or disk) as they work.
 * (NOTE 951005: even less cpu now that data between filters is passed
 *  via a vector.)
 *
 * A few notes about implementing filters:
 *
 *  - A generic filter template looks like:
 *
 *    void
 *    gf_xxx_filter(f, flg)
 *        FILTER_S *f;
 *        int       flg;
 *    {
 *	  GF_INIT(f, f->next);		// def's var's to speed queue drain
 *
 *        if(flg == GF_DATA){
 *	      register unsigned char c;
 *
 *	      while(GF_GETC(f, c)){	// macro taking data off input queue
 *	          // operate on c and pass it on here
 *                GF_PUTC(f->next, c);	// macro writing output queue
 *	      }
 *
 *	      GF_END(f, f->next);	// macro to sync pointers/offsets
 *	      //WARNING: DO NOT RETURN BEFORE ALL INCOMING DATA'S PROCESSED
 *        }
 *        else if(flg == GF_EOD){
 *            // process any buffered data here and pass it on
 *	      GF_FLUSH(f->next);	// flush pending data to next filter
 *            (*f->next->f)(f->next, GF_EOD);
 *        }
 *        else if(flg == GF_RESET){
 *            // initialize any data in the struct here
 *        }
 *    }
 *
 *  - Any free storage allocated during initialization (typically tied
 *    to the "line" pointer in FILTER_S) is the filter's responsibility
 *    to clean up when the GF_EOD command comes through.
 *
 *  - Filter's must pass GF_EOD they receive on to the next
 *    filter in the chain so it has the opportunity to flush
 *    any buffered data.
 *
 *  - All filters expect NVT end-of-lines.  The idea is to prepend
 *    or append either the gf_local_nvtnl or gf_nvtnl_local
 *    os-dependant filters to the data on the appropriate end of the
 *    pipe for the task at hand.
 *
 *  - NOTE: As of 951004, filters no longer take their input as a single
 *    char argument, but rather get data to operate on via a vector
 *    representing the input queue in the FILTER_S structure.
 *
 */



/*
 * BASE64 TO BINARY encoding and decoding routines below
 */


/*
 * BINARY to BASE64 filter (encoding described in rfc1341)
 */
void
gf_binary_b64(FILTER_S *f, int flg)
{
    static char *v =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;
	register unsigned char t = f->t;
	register long n = f->n;

	while(GF_GETC(f, c)){

	    switch(n++){
	      case 0 : case 3 : case 6 : case 9 : case 12: case 15: case 18:
	      case 21: case 24: case 27: case 30: case 33: case 36: case 39:
	      case 42: case 45:
		GF_PUTC(f->next, v[c >> 2]);
					/* byte 1: high 6 bits (1) */
		t = c << 4;		/* remember high 2 bits for next */
		break;

	      case 1 : case 4 : case 7 : case 10: case 13: case 16: case 19:
	      case 22: case 25: case 28: case 31: case 34: case 37: case 40:
	      case 43:
		GF_PUTC(f->next, v[(t|(c>>4)) & 0x3f]);
		t = c << 2;
		break;

	      case 2 : case 5 : case 8 : case 11: case 14: case 17: case 20:
	      case 23: case 26: case 29: case 32: case 35: case 38: case 41:
	      case 44:
		GF_PUTC(f->next, v[(t|(c >> 6)) & 0x3f]);
		GF_PUTC(f->next, v[c & 0x3f]);
		break;
	    }

	    if(n == 45){			/* start a new line? */
		GF_PUTC(f->next, '\015');
		GF_PUTC(f->next, '\012');
		n = 0L;
	    }
	}

	f->n = n;
	f->t = t;
	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){		/* no more data */
	switch (f->n % 3) {		/* handle trailing bytes */
	  case 0:			/* no trailing bytes */
	    break;

	  case 1:
	    GF_PUTC(f->next, v[(f->t) & 0x3f]);
	    GF_PUTC(f->next, '=');	/* byte 3 */
	    GF_PUTC(f->next, '=');	/* byte 4 */
	    break;

	  case 2:
	    GF_PUTC(f->next, v[(f->t) & 0x3f]);
	    GF_PUTC(f->next, '=');	/* byte 4 */
	    break;
	}

	/* end with CRLF */
	if(f->n){
	    GF_PUTC(f->next, '\015');
	    GF_PUTC(f->next, '\012');
	}

	(void) GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint((9, "-- gf_reset binary_b64\n"));
	f->n = 0L;
    }
}



/*
 * BASE64 to BINARY filter (encoding described in rfc1341)
 */
void
gf_b64_binary(FILTER_S *f, int flg)
{
    static char v[] = {65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,
		       65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,65,
		       65,65,65,65,65,65,65,65,65,65,65,62,65,65,65,63,
		       52,53,54,55,56,57,58,59,60,61,65,65,65,64,65,65,
		       65, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
		       15,16,17,18,19,20,21,22,23,24,25,65,65,65,65,65,
		       65,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
		       41,42,43,44,45,46,47,48,49,50,51,65,65,65,65,65};
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;
	register unsigned char t = f->t;
	register int n = (int) f->n;
	register int state = f->f1;

	while(GF_GETC(f, c)){

	    if(state){
		state = 0;
		if (c != '=') {
		    gf_error("Illegal '=' in base64 text");
		    /* NO RETURN */
		}
	    }

	    /* in range, and a valid value? */
	    if((c & ~0x7f) || (c = v[c]) > 63){
		if(c == 64){
		    switch (n++) {	/* check quantum position */
		      case 2:
			state++;	/* expect an equal as next char */
			break;

		      case 3:
			n = 0L;		/* restart quantum */
			break;

		      default:		/* impossible quantum position */
			gf_error("Internal base64 decoder error");
			/* NO RETURN */
		    }
		}
	    }
	    else{
		switch (n++) {		/* install based on quantum position */
		  case 0:		/* byte 1: high 6 bits */
		    t = c << 2;
		    break;

		  case 1:		/* byte 1: low 2 bits */
		    GF_PUTC(f->next, (t|(c >> 4)));
		    t = c << 4;		/* byte 2: high 4 bits */
		    break;

		  case 2:		/* byte 2: low 4 bits */
		    GF_PUTC(f->next, (t|(c >> 2)));
		    t = c << 6;		/* byte 3: high 2 bits */
		    break;

		  case 3:
		    GF_PUTC(f->next, t | c);
		    n = 0L;		/* reinitialize mechanism */
		    break;
		}
	    }
	}

	f->f1 = state;
	f->t = t;
	f->n = n;
	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	(void) GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint((9, "-- gf_reset b64_binary\n"));
	f->n  = 0L;			/* quantum position */
	f->f1 = 0;			/* state holder: equal seen? */
    }
}




/*
 * QUOTED-PRINTABLE ENCODING AND DECODING filters below.
 * encoding described in rfc1341
 */

#define	GF_MAXLINE	80		/* good buffer size */

/*
 * default action for QUOTED-PRINTABLE to 8BIT decoder
 */
#define	GF_QP_DEFAULT(f, c)	{ \
				    if((c) == ' '){ \
					state = WSPACE; \
						/* reset white space! */ \
					(f)->linep = (f)->line; \
					*((f)->linep)++ = ' '; \
				    } \
				    else if((c) == '='){ \
					state = EQUAL; \
				    } \
				    else \
				      GF_PUTC((f)->next, (c)); \
				}


/*
 * QUOTED-PRINTABLE to 8BIT filter
 */
void
gf_qp_8bit(FILTER_S *f, int flg)
{

    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;
	register int state = f->f1;

	while(GF_GETC(f, c)){

	    switch(state){
	      case DFL :		/* default case */
	      default:
		GF_QP_DEFAULT(f, c);
		break;

	      case CCR    :		/* non-significant space */
		state = DFL;
		if(c == '\012')
		  continue;		/* go on to next char */

		GF_QP_DEFAULT(f, c);
		break;

	      case EQUAL  :
		if(c == '\015'){	/* "=\015" is a soft EOL */
		    state = CCR;
		    break;
		}

		if(c == '='){		/* compatibility clause for old guys */
		    GF_PUTC(f->next, '=');
		    state = DFL;
		    break;
		}

		if(!isxdigit((unsigned char)c)){	/* must be hex! */
		    /*
		     * First character after '=' not a hex digit.
		     * This ain't right, but we're going to treat it as
		     * plain old text instead of an '=' followed by hex.
		     * In other words, they forgot to encode the '='.
		     * Before 4.60 we just bailed with an error here, but now
		     * we keep going as long as we are just displaying
		     * the result (and not saving it or something).
		     *
		     * Wait! The users don't like that. They want to be able
		     * to use it even if it might be wrong. So just plow
		     * ahead even if displaying.
		     *
		     * Better have this be a constant string so that if we
		     * get multiple instances of it in a single message we
		     * can avoid the too many error messages problem. It
		     * better be the same message as the one a few lines
		     * below, as well.
		     *
		     * Turn off decoding after encountering such an error and
		     * just dump the rest of the text as is.
		     */
		    state = STOP_DECODING;
		    GF_PUTC(f->next, '=');
		    GF_PUTC(f->next, c);
		    q_status_message(SM_ORDER,3,3,
			_("Warning: Non-hexadecimal character in QP encoding!"));

		    dprint((2, "gf_qp_8bit: warning: non-hex char in QP encoding: char \"%c\" (%d) follows =\n", c, c));
		    break;
		}

		if (isdigit ((unsigned char)c))
		  f->t = c - '0';
		else
		  f->t = c - (isupper((unsigned char)c) ? 'A' - 10 : 'a' - 10);
		
		f->f2 = c;	/* store character in case we have to
				   back out in !isxdigit below */

		state = HEX;
		break;

	      case HEX :
		state = DFL;
		if(!isxdigit((unsigned char)c)){	/* must be hex! */
		    state = STOP_DECODING;
		    GF_PUTC(f->next, '=');
		    GF_PUTC(f->next, f->f2);
		    GF_PUTC(f->next, c);
		    q_status_message(SM_ORDER,3,3,
			_("Warning: Non-hexadecimal character in QP encoding!"));

		    dprint((2, "gf_qp_8bit: warning: non-hex char in QP encoding: char \"%c\" (%d) follows =%c\n", c, c, f->f2));
		    break;
		}

		if (isdigit((unsigned char)c))
		  c -= '0';
		else
		  c -= (isupper((unsigned char)c) ? 'A' - 10 : 'a' - 10);

		GF_PUTC(f->next, c + (f->t << 4));
		break;

	      case WSPACE :
		if(c == ' '){		/* toss it in with other spaces */
		    if(f->linep - f->line < GF_MAXLINE)
		      *(f->linep)++ = ' ';
		    break;
		}

		state = DFL;
		if(c == '\015'){	/* not our white space! */
		    f->linep = f->line;	/* reset buffer */
		    GF_PUTC(f->next, '\015');
		    break;
		}

		/* the spaces are ours, write 'em */
		f->n = f->linep - f->line;
		while((f->n)--)
		  GF_PUTC(f->next, ' ');

		GF_QP_DEFAULT(f, c);	/* take care of 'c' in default way */
		break;

	      case STOP_DECODING :
		GF_PUTC(f->next, c);
		break;
	    }
	}

	f->f1 = state;
	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	fs_give((void **)&(f->line));
	(void) GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint((9, "-- gf_reset qp_8bit\n"));
	f->f1 = DFL;
	f->linep = f->line = (char *)fs_get(GF_MAXLINE * sizeof(char));
    }
}



/*
 * USEFUL MACROS TO HELP WITH QP ENCODING
 */

#define	QP_MAXL	75			/* 76th place only for continuation */

/*
 * Macro to test and wrap long quoted printable lines
 */
#define	GF_8BIT_WRAP(f)		{ \
				    GF_PUTC((f)->next, '='); \
				    GF_PUTC((f)->next, '\015'); \
				    GF_PUTC((f)->next, '\012'); \
				}

/*
 * write a quoted octet in QUOTED-PRINTABLE encoding, adding soft
 * line break if needed.
 */
#define	GF_8BIT_PUT_QUOTE(f, c)	{ \
				    if(((f)->n += 3) > QP_MAXL){ \
					GF_8BIT_WRAP(f); \
					(f)->n = 3;	/* set line count */ \
				    } \
				    GF_PUTC((f)->next, '='); \
				     GF_PUTC((f)->next, HEX_CHAR1(c)); \
				     GF_PUTC((f)->next, HEX_CHAR2(c)); \
				 }

/*
 * just write an ordinary octet in QUOTED-PRINTABLE, wrapping line
 * if needed.
 */
#define	GF_8BIT_PUT(f, c)	{ \
				      if((++(f->n)) > QP_MAXL){ \
					  GF_8BIT_WRAP(f); \
					  f->n = 1L; \
				      } \
				      if(f->n == 1L && c == '.'){ \
					  GF_8BIT_PUT_QUOTE(f, c); \
					  f->n = 3; \
				      } \
				      else \
					GF_PUTC(f->next, c); \
				 }


/*
 * default action for 8bit to quoted printable encoder
 */
#define	GF_8BIT_DEFAULT(f, c)	if((c) == ' '){ \
				     state = WSPACE; \
				 } \
				 else if(c == '\015'){ \
				     state = CCR; \
				 } \
				 else if(iscntrl(c & 0x7f) || (c == 0x7f) \
					 || (c & 0x80) || (c == '=')){ \
				     GF_8BIT_PUT_QUOTE(f, c); \
				 } \
				 else{ \
				   GF_8BIT_PUT(f, c); \
				 }


/*
 * 8BIT to QUOTED-PRINTABLE filter
 */
void
gf_8bit_qp(FILTER_S *f, int flg)
{
    short dummy_dots = 0, dummy_dmap = 1;
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	 register unsigned char c;
	 register int state = f->f1;

	 while(GF_GETC(f, c)){

	     /* keep track of "^JFrom " */
	     Find_Froms(f->t, dummy_dots, f->f2, dummy_dmap, c);

	     switch(state){
	       case DFL :		/* handle ordinary case */
		 GF_8BIT_DEFAULT(f, c);
		 break;

	       case CCR :		/* true line break? */
		 state = DFL;
		 if(c == '\012'){
		     GF_PUTC(f->next, '\015');
		     GF_PUTC(f->next, '\012');
		     f->n = 0L;
		 }
		 else{			/* nope, quote the CR */
		     GF_8BIT_PUT_QUOTE(f, '\015');
		     GF_8BIT_DEFAULT(f, c); /* and don't forget about c! */
		 }
		 break;

	       case WSPACE:
		 state = DFL;
		 if(c == '\015' || f->t){ /* handle the space */
		     GF_8BIT_PUT_QUOTE(f, ' ');
		     f->t = 0;		/* reset From flag */
		 }
		 else
		   GF_8BIT_PUT(f, ' ');

		 GF_8BIT_DEFAULT(f, c);	/* handle 'c' in the default way */
		 break;
	     }
	 }

	 f->f1 = state;
	 GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	 switch(f->f1){
	   case CCR :
	     GF_8BIT_PUT_QUOTE(f, '\015'); /* write the last cr */
	     break;

	   case WSPACE :
	     GF_8BIT_PUT_QUOTE(f, ' ');	/* write the last space */
	     break;
	 }

	 (void) GF_FLUSH(f->next);
	 (*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	 dprint((9, "-- gf_reset 8bit_qp\n"));
	 f->f1 = DFL;			/* state from last character        */
	 f->f2 = 1;			/* state of "^NFrom " bitmap        */
	 f->t  = 0;
	 f->n  = 0L;			/* number of chars in current line  */
    }
}

/*
 * This filter converts characters in one character set (the character
 * set of a message, for example) to another (the user's character set).
 */
void
gf_convert_8bit_charset(FILTER_S *f, int flg)
{
    static unsigned char *conv_table = NULL;
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	 register unsigned char c;

	 while(GF_GETC(f, c)){
	   GF_PUTC(f->next, conv_table ? conv_table[c] : c);
	 }

	 GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	 (void) GF_FLUSH(f->next);
	 (*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	 dprint((9, "-- gf_reset convert_8bit_charset\n"));
	 conv_table = (f->opt) ? (unsigned char *) (f->opt) : NULL;

    }
}


typedef	struct _utf8c_s {
    void *conv_table;
    int   report_err;
} UTF8C_S;


/*
 * This filter converts characters in UTF-8 to an 8-bit or 16-bit charset.
 * Characters missing from the destination set, and invalid UTF-8 sequences,
 * will be converted to "?".
 */
void
gf_convert_utf8_charset(FILTER_S *f, int flg)
{
    static unsigned short *conv_table = NULL;
    static int report_err = 0;
    register int more = f->f2;
    register long u = f->n;

    /*
     * "more" is the number of subsequent octets needed to complete a character,
     * it is stored in f->f2.
     * "u" is the accumulated Unicode character, it is stored in f->n
     */

    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;

	while(GF_GETC(f, c)){
	    if(!conv_table){	/* can't do much if no conversion table */
		GF_PUTC(f->next, c);
	    }
				/* UTF-8 continuation? */
	    else if((c > 0x7f) && (c < 0xc0)){
		if(more){
		    u <<= 6;	/* shift current value by 6 bits */
		    u |= c & 0x3f;
		    if (!--more){ /* last octet? */
			if(u >= 0xffff || (u = conv_table[u]) == NOCHAR){
			    /*
			     * non-BMP character or a UTF-8 character
			     * which is not representable in the
			     * charset we're converting to.
			     */
			    c = '?';
			    if(report_err){
				if(f->opt)
				  fs_give((void **) &f->opt);

				/* TRANSLATORS: error while translating from one
				   character set to another, for example from UTF-8
				   to ISO-2022-JP or something like that. */
				gf_error(_("translation error"));
				/* NO RETURN */
			    }
			}
			else{
			    if(u > 0xff){
				c = (unsigned char) (u >> 8);
				GF_PUTC(f->next, c);
			    }

			    c = (unsigned char) u & 0xff;
			}

			GF_PUTC(f->next, c);
		    }
		}
		else{		/* continuation when not in progress */
		    GF_PUTC(f->next, '?');
		}
	    }
	    else{
	        if(more){	/* incomplete UTF-8 character */
		    GF_PUTC(f->next, '?');
		    more = 0;
		}
		if(c < 0x80){ /* U+0000 - U+007f */
		    GF_PUTC(f->next, c);
		}
		else if(c < 0xe0){ /* U+0080 - U+07ff */
		    u = c & 0x1f; /* first 5 bits of 12 */
		    more = 1;
		}
		else if(c < 0xf0){ /* U+1000 - U+ffff */
		    u = c & 0x0f; /* first 4 bits of 16 */
		    more = 2;
		}
				/* in case we ever support non-BMP Unicode */
		else if (c < 0xf8){ /* U+10000 - U+10ffff */
		    u = c & 0x07; /* first 3 bits of 20.5 */
		    more = 3;
		}
#if 0	/* ISO 10646 not in Unicode */
		else if (c < 0xfc){ /* ISO 10646 20000 - 3ffffff */
		    u = c & 0x03; /* first 2 bits of 26 */
		    more = 4;
		}
		else if (c < 0xfe){ /* ISO 10646 4000000 - 7fffffff */
		    u = c & 0x03; /* first 2 bits of 26 */
		    more = 5;
		}
#endif
		else{		/* not in Unicode */
		    GF_PUTC(f->next, '?');
		}
	    }
	}

	f->f2 = more;
	f->n = u;
	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	(void) GF_FLUSH(f->next);
	if(f->opt)
	  fs_give((void **) &f->opt);

	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint((9, "-- gf_reset convert_utf8_charset\n"));
	conv_table = ((UTF8C_S *) f->opt)->conv_table;
	report_err = ((UTF8C_S *) f->opt)->report_err;
	f->f2 = 0;
	f->n = 0L;
    }
}


void *
gf_convert_utf8_charset_opt(void *table, int report_err)
{
    UTF8C_S *utf8c;

    utf8c = (UTF8C_S *) fs_get(sizeof(UTF8C_S));
    utf8c->conv_table = table;
    utf8c->report_err = report_err;
    return((void *) utf8c);
}


/*
 * ISO-2022-JP to EUC (on Unix) or Shift-JIS (on PC) filter
 *
 * The routine is call ..._to_euc but it is really to either euc (unix Pine)
 * or to Shift-JIS (if PC-Pine).
 */
void
gf_2022_jp_to_euc(FILTER_S *f, int flg)
{
    register unsigned char c;
    register int state = f->f1;

    /*
     * f->t lit means we're in middle of decoding a sequence of characters.
     * f->f2 keeps track of first character of pair for Shift-JIS.
     * f->f1 is the state.
     */

    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	while(GF_GETC(f, c)){
	    switch(state){
	      case ESC:				/* saw ESC */
	        if(!f->t && c == '$')
		  state = ESCDOL;
	        else if(f->t && c == '(')
		  state = ESCPAR;
		else{
		    GF_PUTC(f->next, '\033');
		    GF_PUTC(f->next, c);
		    state = DFL;
		}

	        break;

	      case ESCDOL:			/* saw ESC $ */
	        if(c == 'B' || c == '@'){
		    state = EUC;
		    f->t = 1;			/* filtering into euc */
		    f->f2 = -1;			/* first character of pair */
		}
		else{
		    GF_PUTC(f->next, '\033');
		    GF_PUTC(f->next, '$');
		    GF_PUTC(f->next, c);
		    state = DFL;
		}

	        break;

	      case ESCPAR:			/* saw ESC ( */
	        if(c == 'B' || c == 'J' || c == 'H'){
		    state = DFL;
		    f->t = 0;			/* done filtering */
		}
		else{
		    GF_PUTC(f->next, '\033');	/* Don't set hibit for     */
		    GF_PUTC(f->next, '(');	/* escape sequences, which */
		    GF_PUTC(f->next, c);	/* this appears to be.     */
		}

	        break;

	      case EUC:				/* filtering into euc */
		if(c == '\033')
		  state = ESC;
		else{
#ifdef _WINDOWS					/* Shift-JIS */
		    c &= 0x7f;			/* 8-bit can't win */
		    if (f->f2 >= 0){		/* second of a pair? */
			int rowOffset = (f->f2 < 95) ? 112 : 176;
			int cellOffset = (f->f2 % 2) ? ((c > 95) ? 32 : 31)
						     : 126;

			GF_PUTC(f->next, ((f->f2 + 1) >> 1) + rowOffset);
			GF_PUTC(f->next, c + cellOffset);
			f->f2 = -1;		/* restart */
		    }
		    else if(c > 0x20 && c < 0x7f)
		      f->f2 = c;		/* first of pair */
		    else{
			GF_PUTC(f->next, c);	/* write CTL as itself */
			f->f2 = -1;
		    }
#else						/* EUC */
		    GF_PUTC(f->next, (c > 0x20 && c < 0x7f) ? c | 0x80 : c);
#endif
		}

	        break;

	      case DFL:
	      default:
		if(c == '\033')
		  state = ESC;
		else
		  GF_PUTC(f->next, c);

		break;
	    }
	}

	f->f1 = state;
	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	switch(state){
	  case ESC:
	    GF_PUTC(f->next, '\033');
	    break;

	  case ESCDOL:
	    GF_PUTC(f->next, '\033');
	    GF_PUTC(f->next, '$');
	    break;

	  case ESCPAR:
	    GF_PUTC(f->next, '\033');	/* Don't set hibit for     */
	    GF_PUTC(f->next, '(');	/* escape sequences.       */
	    break;
	}

	(void) GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint((9, "-- gf_reset jp_to_euc\n"));
	f->f1 = DFL;		/* state */
	f->t = 0;		/* not translating to euc */
    }
}


/*
 * EUC (on Unix) or Shift-JIS (on PC) to ISO-2022-JP filter
 */
void
gf_native8bitjapanese_to_2022_jp(FILTER_S *f, int flg)
{
#ifdef _WINDOWS
    gf_sjis_to_2022_jp(f, flg);
#else
    gf_euc_to_2022_jp(f, flg);
#endif
}


void
gf_euc_to_2022_jp(FILTER_S *f, int flg)
{
    register unsigned char c;

    /*
     * f->t lit means we've sent the start esc seq but not the end seq.
     * f->f2 keeps track of first character of pair for Shift-JIS.
     */

    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	while(GF_GETC(f, c)){
	    if(f->t){
		if(c & 0x80){
		    GF_PUTC(f->next, c & 0x7f);
		}
		else{
		    GF_PUTC(f->next, '\033');
		    GF_PUTC(f->next, '(');
		    GF_PUTC(f->next, 'B');
		    GF_PUTC(f->next, c);
		    f->f2 = -1;
		    f->t = 0;
		}
	    }
	    else{
		if(c & 0x80){
		    GF_PUTC(f->next, '\033');
		    GF_PUTC(f->next, '$');
		    GF_PUTC(f->next, 'B');
		    GF_PUTC(f->next, c & 0x7f);
		    f->t = 1;
		}
		else{
		    GF_PUTC(f->next, c);
		}
	    }
	}

	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	if(f->t){
	    GF_PUTC(f->next, '\033');
	    GF_PUTC(f->next, '(');
	    GF_PUTC(f->next, 'B');
	    f->t = 0;
	    f->f2 = -1;
	}

	(void) GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint((9, "-- gf_reset euc_to_jp\n"));
	f->t = 0;
	f->f2 = -1;
    }
}

void
gf_sjis_to_2022_jp(FILTER_S *f, int flg)
{
    register unsigned char c;

    /*
     * f->t lit means we've sent the start esc seq but not the end seq.
     * f->f2 keeps track of first character of pair for Shift-JIS.
     */

    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	while(GF_GETC(f, c)){
	    if(f->t){
		if(f->f2 >= 0){			/* second of a pair? */
		    int adjust = c < 159;
		    int rowOffset = f->f2 < 160 ? 112 : 176;
		    int cellOffset = adjust ? (c > 127 ? 32 : 31) : 126;

		    GF_PUTC(f->next, ((f->f2 - rowOffset) << 1) - adjust);
		    GF_PUTC(f->next, c - cellOffset);
		    f->f2 = -1;
		}
		else if(c & 0x80){
		    f->f2 = c;			/* remember first of pair */
		}
		else{
		    GF_PUTC(f->next, '\033');
		    GF_PUTC(f->next, '(');
		    GF_PUTC(f->next, 'B');
		    GF_PUTC(f->next, c);
		    f->f2 = -1;
		    f->t = 0;
		}
	    }
	    else{
		if(c & 0x80){
		    GF_PUTC(f->next, '\033');
		    GF_PUTC(f->next, '$');
		    GF_PUTC(f->next, 'B');
		    f->f2 = c;
		    f->t = 1;
		}
		else{
		    GF_PUTC(f->next, c);
		}
	    }
	}

	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	if(f->t){
	    GF_PUTC(f->next, '\033');
	    GF_PUTC(f->next, '(');
	    GF_PUTC(f->next, 'B');
	    f->t = 0;
	    f->f2 = -1;
	}

	(void) GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint((9, "-- gf_reset sjis_to_jp\n"));
	f->t = 0;
	f->f2 = -1;
    }
}



/*
 * Various charset to UTF-8 Translation filter
 */

/*
 * utf8 conversion options
 */
typedef	struct _utf8_s {
    CHARSET	  *charset;
    unsigned long  ucsc;
} UTF8_S;

#define	UTF8_BLOCK	1024
#define	UTF8_EOB(f)	((f)->line + (f)->f2 - 1)
#define	UTF8_ADD(f, c) \
			{ \
			    if(p >= eobuf){ \
				f->f2 += UTF8_BLOCK; \
				fs_resize((void **)&f->line, \
				      (size_t) f->f2 * sizeof(char)); \
				eobuf = UTF8_EOB(f); \
				p = eobuf - UTF8_BLOCK; \
			    } \
			    *p++ = c; \
			}
#define	GF_UTF8_FLUSH(f)	{ \
				    register long n; \
				    SIZEDTEXT     intext, outtext; \
				    intext.data = (unsigned char *) f->line; \
				    intext.size = p - f->line; \
				    memset(&outtext, 0, sizeof(SIZEDTEXT)); \
				    if(!((UTF8_S *) f->opt)->charset){ \
					for(n = 0; n < intext.size; n++) \
					  GF_PUTC(f->next, (intext.data[n] & 0x80) ? '?' : intext.data[n]); \
				    } \
				    else if(utf8_text_cs(&intext, ((UTF8_S *) f->opt)->charset, &outtext, NULL, NULL)){ \
					for(n = 0; n < outtext.size; n++) \
					  GF_PUTC(f->next, outtext.data[n]); \
					if(outtext.data && intext.data != outtext.data) \
					  fs_give((void **) &outtext.data); \
				    } \
				    else{ \
					for(n = 0; n < intext.size; n++) \
					  GF_PUTC(f->next, '?'); \
				    } \
				}


/*
 * gf_utf8 - text in specified charset to to UTF-8 filter
 *           Process line-at-a-time rather than character
 *           because ISO-2022-JP.  Call utf8_text_cs by hand
 *           rather than utf8_text to reduce the cost of
 *           utf8_charset() for each line.
 */
void
gf_utf8(FILTER_S *f, int flg)
{
    register char *p = f->linep;
    register char *eobuf = UTF8_EOB(f);
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register int state = f->f1;
	register unsigned char c;

	while(GF_GETC(f, c)){

	    switch(state){
	      case CCR :
		state = DFL;
		if(c == '\012'){
		    GF_UTF8_FLUSH(f);
		    p = f->line;
		    GF_PUTC(f->next, '\015');
		    GF_PUTC(f->next, '\012');
		}
		else{
		    UTF8_ADD(f, '\015');
		    UTF8_ADD(f, c);
		}
		
		break;

	      default :
		if(c == '\015'){
		    state = CCR;
		}
		else
		  UTF8_ADD(f, c);
	    }
	}

	f->f1 = state;
	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){

	if(p != f->line)
	  GF_UTF8_FLUSH(f);

	fs_give((void **) &f->line);
	fs_give((void **) &f->opt);
	(void) GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(GF_RESET){
	dprint((9, "-- gf_reset utf8\n"));
	f->f1 = DFL;
	f->f2 = UTF8_BLOCK;		/* input buffer length */
	f->line = p = (char *) fs_get(f->f2 * sizeof(char));
    }

    f->linep = p;
}


void *
gf_utf8_opt(char *charset)
{
    UTF8_S *utf8;

    utf8 = (UTF8_S *) fs_get(sizeof(UTF8_S));

    utf8->charset = (CHARSET *) utf8_charset(charset);

    /*
     * When we get 8-bit non-ascii characters but it is supposed to
     * be ascii we want it to turn into question marks, not
     * just behave as if it is UTF-8 which is what happens
     * with ascii because there is no translation table.
     * So we need to catch the ascii special case here.
     */
    if(utf8->charset && utf8->charset->type == CT_ASCII)
      utf8->charset = NULL;

    return((void *) utf8);
}


/*
 * RICHTEXT-TO-PLAINTEXT filter
 */

/*
 * option to be used by rich2plain (NOTE: if this filter is ever
 * used more than once in a pipe, all instances will have the same
 * option value)
 */


/*----------------------------------------------------------------------
      richtext to plaintext filter

 Args: f --
	flg  --

  This basically removes all richtext formatting. A cute hack is used
  to get bold and underlining to work.
  Further work could be done to handle things like centering and right
  and left flush, but then it could no longer be done in place. This
  operates on text *with* CRLF's.

  WARNING: does not wrap lines!
 ----*/
void
gf_rich2plain(FILTER_S *f, int flg)
{
    static int rich_bold_on = 0, rich_uline_on = 0;

/* BUG: qoute incoming \255 values */
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	 register unsigned char c;
	 register int state = f->f1;
	 register int plain;

	 plain = f->opt ? (*(int *) f->opt) : 0;

	 while(GF_GETC(f, c)){

	     switch(state){
	       case TOKEN :		/* collect a richtext token */
		 if(c == '>'){		/* what should we do with it? */
		     state       = DFL;	/* return to default next time */
		     *(f->linep) = '\0';	/* cap off token */
		     if(f->line[0] == 'l' && f->line[1] == 't'){
			 GF_PUTC(f->next, '<'); /* literal '<' */
		     }
		     else if(f->line[0] == 'n' && f->line[1] == 'l'){
			 GF_PUTC(f->next, '\015');/* newline! */
			 GF_PUTC(f->next, '\012');
		     }
		     else if(!strcmp("comment", f->line)){
			 (f->f2)++;
		     }
		     else if(!strcmp("/comment", f->line)){
			 f->f2 = 0;
		     }
		     else if(!strcmp("/paragraph", f->line)) {
			 GF_PUTC(f->next, '\r');
			 GF_PUTC(f->next, '\n');
			 GF_PUTC(f->next, '\r');
			 GF_PUTC(f->next, '\n');
		     }
		     else if(!plain /* gf_rich_plain */){
			 if(!strcmp(f->line, "bold")) {
			     GF_PUTC(f->next, TAG_EMBED);
			     GF_PUTC(f->next, TAG_BOLDON);
			     rich_bold_on = 1;
			 } else if(!strcmp(f->line, "/bold")) {
			     GF_PUTC(f->next, TAG_EMBED);
			     GF_PUTC(f->next, TAG_BOLDOFF);
			     rich_bold_on = 0;
			 } else if(!strcmp(f->line, "italic")) {
			     GF_PUTC(f->next, TAG_EMBED);
			     GF_PUTC(f->next, TAG_ULINEON);
			     rich_uline_on = 1;
			 } else if(!strcmp(f->line, "/italic")) {
			     GF_PUTC(f->next, TAG_EMBED);
			     GF_PUTC(f->next, TAG_ULINEOFF);
			     rich_uline_on = 0;
			 } else if(!strcmp(f->line, "underline")) {
			     GF_PUTC(f->next, TAG_EMBED);
			     GF_PUTC(f->next, TAG_ULINEON);
			     rich_uline_on = 1;
			 } else if(!strcmp(f->line, "/underline")) {
			     GF_PUTC(f->next, TAG_EMBED);
			     GF_PUTC(f->next, TAG_ULINEOFF);
			     rich_uline_on = 0;
			 }
		     }
		     /* else we just ignore the token! */

		     f->linep = f->line;	/* reset token buffer */
		 }
		 else{			/* add char to token */
		     if(f->linep - f->line > 40){
			 /* What? rfc1341 says 40 char tokens MAX! */
			 fs_give((void **)&(f->line));
			 gf_error("Richtext token over 40 characters");
			 /* NO RETURN */
		     }

		     *(f->linep)++ = isupper((unsigned char)c) ? c-'A'+'a' : c;
		 }
		 break;

	       case CCR   :
		 state = DFL;		/* back to default next time */
		 if(c == '\012'){	/* treat as single space?    */
		     GF_PUTC(f->next, ' ');
		     break;
		 }
		 /* fall thru to process c */

	       case DFL   :
	       default:
		 if(c == '<')
		   state = TOKEN;
		 else if(c == '\015')
		   state = CCR;
		 else if(!f->f2)		/* not in comment! */
		   GF_PUTC(f->next, c);

		 break;
	     }
	 }

	 f->f1 = state;
	 GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	 if((f->f1 = (f->linep != f->line)) != 0){
	     /* incomplete token!! */
	     gf_error("Incomplete token in richtext");
	     /* NO RETURN */
	 }

	 if(rich_uline_on){
	     GF_PUTC(f->next, TAG_EMBED);
	     GF_PUTC(f->next, TAG_ULINEOFF);
	     rich_uline_on = 0;
	 }
	 if(rich_bold_on){
	     GF_PUTC(f->next, TAG_EMBED);
	     GF_PUTC(f->next, TAG_BOLDOFF);
	     rich_bold_on = 0;
	 }

	 fs_give((void **)&(f->line));
	 (void) GF_FLUSH(f->next);
	 (*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	 dprint((9, "-- gf_reset rich2plain\n"));
	 f->f1 = DFL;			/* state */
	 f->f2 = 0;			/* set means we're in a comment */
	 f->linep = f->line = (char *)fs_get(45 * sizeof(char));
    }
}


/*
 * function called from the outside to set
 * richtext filter's options
 */
void *
gf_rich2plain_opt(int *plain)
{
    return((void *) plain);
}



/*
 * ENRICHED-TO-PLAIN text filter
 */

#define	TEF_QUELL	0x01
#define	TEF_NOFILL	0x02



/*----------------------------------------------------------------------
      enriched text to plain text filter (ala rfc1523)

 Args: f -- state and input data
	flg --

  This basically removes all enriched formatting. A cute hack is used
  to get bold and underlining to work.

  Further work could be done to handle things like centering and right
  and left flush, but then it could no longer be done in place. This
  operates on text *with* CRLF's.

  WARNING: does not wrap lines!
 ----*/
void
gf_enriched2plain(FILTER_S *f, int flg)
{
    static int enr_uline_on = 0, enr_bold_on = 0;

/* BUG: qoute incoming \255 values */
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	 register unsigned char c;
	 register int state = f->f1;
	 register int plain;

	 plain = f->opt ? (*(int *) f->opt) : 0;

	 while(GF_GETC(f, c)){

	     switch(state){
	       case TOKEN :		/* collect a richtext token */
		 if(c == '>'){		/* what should we do with it? */
		     int   off   = *f->line == '/';
		     char *token = f->line + (off ? 1 : 0);
		     state	= DFL;
		     *f->linep   = '\0';
		     if(!strcmp("param", token)){
			 if(off)
			   f->f2 &= ~TEF_QUELL;
			 else
			   f->f2 |= TEF_QUELL;
		     }
		     else if(!strcmp("nofill", token)){
			 if(off)
			   f->f2 &= ~TEF_NOFILL;
			 else
			   f->f2 |= TEF_NOFILL;
		     }
		     else if(!plain /* gf_enriched_plain */){
			 /* Following is a cute hack or two to get
			    bold and underline on the screen.
			    See Putline0n() where these codes are
			    interpreted */
			 if(!strcmp("bold", token)) {
			     GF_PUTC(f->next, TAG_EMBED);
			     GF_PUTC(f->next, off ? TAG_BOLDOFF : TAG_BOLDON);
			     enr_bold_on = off ? 0 : 1;
			 } else if(!strcmp("italic", token)) {
			     GF_PUTC(f->next, TAG_EMBED);
			     GF_PUTC(f->next, off ? TAG_ULINEOFF : TAG_ULINEON);
			     enr_uline_on = off ? 0 : 1;
			 } else if(!strcmp("underline", token)) {
			     GF_PUTC(f->next, TAG_EMBED);
			     GF_PUTC(f->next, off ? TAG_ULINEOFF : TAG_ULINEON);
			     enr_uline_on = off ? 0 : 1;
			 }
		     }
		     /* else we just ignore the token! */

		     f->linep = f->line;	/* reset token buffer */
		 }
		 else if(c == '<'){		/* literal '<'? */
		     if(f->linep == f->line){
			 GF_PUTC(f->next, '<');
			 state = DFL;
		     }
		     else{
			 fs_give((void **)&(f->line));
			 gf_error("Malformed Enriched text: unexpected '<'");
			 /* NO RETURN */
		     }
		 }
		 else{			/* add char to token */
		     if(f->linep - f->line > 60){ /* rfc1523 says 60 MAX! */
			 fs_give((void **)&(f->line));
			 gf_error("Malformed Enriched text: token too long");
			 /* NO RETURN */
		     }

		     *(f->linep)++ = isupper((unsigned char)c) ? c-'A'+'a' : c;
		 }
		 break;

	       case CCR   :
		 if(c != '\012'){	/* treat as single space?    */
		     state = DFL;	/* lone cr? */
		     f->f2 &= ~TEF_QUELL;
		     GF_PUTC(f->next, '\015');
		     goto df;
		 }

		 state = CLF;
		 break;

	       case CLF   :
		 if(c == '\015'){	/* treat as single space?    */
		     state = CCR;	/* repeat crlf's mean real newlines */
		     f->f2 |= TEF_QUELL;
		     GF_PUTC(f->next, '\r');
		     GF_PUTC(f->next, '\n');
		     break;
		 }
		 else{
		     state = DFL;
		     if(!((f->f2) & TEF_QUELL))
		       GF_PUTC(f->next, ' ');

		     f->f2 &= ~TEF_QUELL;
		 }

		 /* fall thru to take care of 'c' */

	       case DFL   :
	       default :
	       df :
		 if(c == '<')
		   state = TOKEN;
		 else if(c == '\015' && (!((f->f2) & TEF_NOFILL)))
		   state = CCR;
		 else if(!((f->f2) & TEF_QUELL))
		   GF_PUTC(f->next, c);

		 break;
	     }
	 }

	 f->f1 = state;
	 GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	 if((f->f1 = (f->linep != f->line)) != 0){
	     /* incomplete token!! */
	     gf_error("Incomplete token in richtext");
	     /* NO RETURN */
	 }
	 if(enr_uline_on){
	     GF_PUTC(f->next, TAG_EMBED);
	     GF_PUTC(f->next, TAG_ULINEOFF);
	     enr_uline_on = 0;
	 }
	 if(enr_bold_on){
	     GF_PUTC(f->next, TAG_EMBED);
	     GF_PUTC(f->next, TAG_BOLDOFF);
	     enr_bold_on = 0;
	 }

	 /* Make sure we end with a newline so everything gets flushed */
	 GF_PUTC(f->next, '\015');
	 GF_PUTC(f->next, '\012');

	 fs_give((void **)&(f->line));

	 (void) GF_FLUSH(f->next);
	 (*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	 dprint((9, "-- gf_reset enriched2plain\n"));
	 f->f1 = DFL;			/* state */
	 f->f2 = 0;			/* set means we're in a comment */
	 f->linep = f->line = (char *)fs_get(65 * sizeof(char));
    }
}


/*
 * function called from the outside to set
 * richtext filter's options
 */
void *
gf_enriched2plain_opt(int *plain)
{
    return((void *) plain);
}



/*
 * HTML-TO-PLAIN text filter
 */


/* OK, here's the plan:

 * a universal output function handles writing  chars and worries
 *    about wrapping.

 * a unversal element collector reads chars and collects params
 * and dispatches the appropriate element handler.

 * element handlers are stacked.  The most recently dispatched gets
 * first crack at the incoming character stream.  It passes bytes it's
 * done with or not interested in to the next

 * installs that handler as the current one collecting data...

 * stacked handlers take their params from the element collector and
 * accept chars or do whatever they need to do.  Sort of a vertical
 * piping? recursion-like? hmmm.

 * at least I think this is how it'll work. tres simple, non?

 */


/*
 * Some important constants
 */
#define	HTML_BUF_LEN	2048		/* max scratch buffer length */
#define	MAX_ENTITY	20		/* maximum length of an entity */
#define	MAX_ELEMENT	72		/* maximum length of an element */
#define HTML_MOREDATA	0		/* expect more entity data */
#define HTML_ENTITY	1		/* valid entity collected */
#define	HTML_BADVALUE	0x0100		/* good data, but bad entity value */
#define	HTML_BADDATA	0x0200		/* bad data found looking for entity */
#define	HTML_LITERAL	0x0400		/* Literal character value */
#define	HTML_NEWLINE	0x010A		/* hard newline */
#define	HTML_DOBOLD	0x0400		/* Start Bold display */
#define	HTML_ID_GET	0		/* indent func: return current val */
#define	HTML_ID_SET	1		/* indent func: set to absolute val */
#define	HTML_ID_INC	2		/* indent func: increment by val */
#define	HTML_HX_CENTER	0x0001
#define	HTML_HX_ULINE	0x0002
#define	RSS_ITEM_LIMIT	20		/* RSS 2.0 ITEM depth limit */


/*
 * Handler data, state information including function that uses it
 */
typedef struct handler_s {
    FILTER_S	      *html_data;
    void	      *element;
    long	       x, y, z;
    void	      *dp;
    unsigned char     *s;
    struct handler_s  *below;
} HANDLER_S;

/*
 * Element Property structure
 */
typedef struct _element_properties {
    char      *element;
    int	     (*handler)(HANDLER_S *, int, int);
    unsigned   blocklevel:1;
} ELPROP_S;

/*
 * Types used to manage HTML parsing
 */
static void html_handoff(HANDLER_S *, int);


/*
 * to help manage line wrapping.
 */
typedef	struct _wrap_line {
    char *buf;				/* buf to collect wrapped text */
    int	  used,				/* number of chars in buf */
	   width,			/* text's width as displayed  */
	   len;				/* length of allocated buf */
} WRAPLINE_S;


/*
 * to help manage centered text
 */
typedef	struct _center_s {
    WRAPLINE_S line;			/* buf to assembled centered text */
    WRAPLINE_S word;			/* word being to append to Line */
    int	       anchor;
    short      space;
} CENTER_S;


/*
 * Collector data and state information
 */
typedef	struct collector_s {
    char        buf[HTML_BUF_LEN];	/* buffer to collect data */
    int		len;			/* length of that buffer  */
    unsigned    end_tag:1;		/* collecting a closing tag */
    unsigned    hit_equal:1;		/* collecting right half of attrib */
    unsigned	mkup_decl:1;		/* markup declaration */
    unsigned	start_comment:1;	/* markup declaration comment */
    unsigned	end_comment:1;		/* legit comment format */
    unsigned	hyphen:1;		/* markup hyphen read */
    unsigned	badform:1;		/* malformed markup element */
    unsigned	overrun:1;		/* Overran buf above */
    unsigned	proc_inst:1;		/* XML processing instructions */
    unsigned	empty:1;		/* empty element */
    unsigned	was_quoted:1;		/* basically to catch null string */
    char	quoted;			/* quoted element param value */
    char       *element;		/* element's collected name */
    PARAMETER  *attribs;		/* element's collected attributes */
    PARAMETER  *cur_attrib;		/* attribute now being collected */
} CLCTR_S;


/*
 * State information for all element handlers
 */
typedef struct html_data {
    HANDLER_S  *h_stack;		/* handler list */
    CLCTR_S    *el_data;		/* element collector data */
    CENTER_S   *centered;		/* struct to manage centered text */
    int	      (*token)(FILTER_S *, int);
    char	quoted;			/* quoted, by either ' or ", text */
    short	indent_level;		/* levels of indention */
    int		in_anchor;		/* text now being written to anchor */
    int		blanks;			/* Consecutive blank line count */
    int		wrapcol;		/* column to wrap lines on */
    int	       *prefix;			/* buffer containing Anchor prefix */
    int		prefix_used;
    long        line_bufsize;           /* current size of the line buffer */
    COLOR_PAIR *color;
    struct {
	 int   state;			/* embedded data state */
	 char *color;			/* embedded color pointer */
    } embedded;
    CBUF_S      cb;			/* utf8->ucs4 conversion state */
    unsigned	wrapstate:1;		/* whether or not to wrap output */
    unsigned	li_pending:1;		/* <LI> next token expected */
    unsigned	de_pending:1;		/* <DT> or <DD> next token expected */
    unsigned	bold_on:1;		/* currently bolding text */
    unsigned	uline_on:1;		/* currently underlining text */
    unsigned	center:1;		/* center output text */
    unsigned	bitbucket:1;		/* Ignore input */
    unsigned	head:1;			/* In doc's HEAD */
    unsigned	body:1;			/* In doc's BODY */
    unsigned	alt_entity:1;		/* use alternative entity values */
    unsigned	wrote:1;		/* anything witten yet? */
} HTML_DATA_S;


/*
 * HTML filter options
 */
typedef	struct _html_opts {
    char	*base;			/* Base URL for this html file */
    int		 columns,		/* Display columns (excluding margins) */
		 indent;		/* Left margin */
    HANDLE_S   **handlesp;		/* Head of handles */
    htmlrisk_t   warnrisk_f;		/* Nasty link warning call */
    ELPROP_S	*element_table;		/* markup element table */
    RSS_FEED_S **feedp;			/* hook for RSS feed response */
    unsigned	strip:1;		/* Hilite TAGs allowed */
    unsigned	handles_loc:1;		/* Local handles requested? */
    unsigned	showserver:1;		/* Display server after anchors */
    unsigned	outputted:1;		/* any */
    unsigned	no_relative_links:1;	/* Disable embeded relative links */
    unsigned	related_content:1;	/* Embeded related content */
    unsigned	html:1;			/* Output content in HTML */
    unsigned	html_imgs:1;		/* Output IMG tags in HTML content */
} HTML_OPT_S;



/*
 * Some macros to make life a little easier
 */
#define	WRAP_COLS(X)	((X)->opt ? ((HTML_OPT_S *)(X)->opt)->columns : 80)
#define	HTML_INDENT(X)	((X)->opt ? ((HTML_OPT_S *)(X)->opt)->indent : 0)
#define	HTML_WROTE(X)	(HD(X)->wrote)
#define	HTML_BASE(X)	((X)->opt ? ((HTML_OPT_S *)(X)->opt)->base : NULL)
#define	STRIP(X)	((X)->opt && ((HTML_OPT_S *)(X)->opt)->strip)
#define	PASS_HTML(X)	((X)->opt && ((HTML_OPT_S *)(X)->opt)->html)
#define	PASS_IMAGES(X)	((X)->opt && ((HTML_OPT_S *)(X)->opt)->html_imgs)
#define	HANDLESP(X)	(((HTML_OPT_S *)(X)->opt)->handlesp)
#define	DO_HANDLES(X)	((X)->opt && HANDLESP(X))
#define	HANDLES_LOC(X)	((X)->opt && ((HTML_OPT_S *)(X)->opt)->handles_loc)
#define	SHOWSERVER(X)	((X)->opt && ((HTML_OPT_S *)(X)->opt)->showserver)
#define	NO_RELATIVE(X)	((X)->opt && ((HTML_OPT_S *)(X)->opt)->no_relative_links)
#define	RELATED_OK(X)	((X)->opt && ((HTML_OPT_S *)(X)->opt)->related_content)
#define	ELEMENTS(X)	(((HTML_OPT_S *)(X)->opt)->element_table)
#define	RSS_FEED(X)	(*(((HTML_OPT_S *)(X)->opt)->feedp))
#define	MAKE_LITERAL(C)	(HTML_LITERAL | ((C) & 0xff))
#define	IS_LITERAL(C)	(HTML_LITERAL & (C))
#define	HD(X)		((HTML_DATA_S *)(X)->data)
#define	ED(X)		(HD(X)->el_data)
#define	EL(X)		((ELPROP_S *) (X)->element)
#define	ASCII_ISSPACE(C) ((C) < 0x80 && isspace((unsigned char) (C)))
#define	HTML_ISSPACE(C)	(IS_LITERAL(C) == 0 && ((C) == HTML_NEWLINE || ASCII_ISSPACE(C)))
#define	NEW_CLCTR(X)	{						\
			   ED(X) = (CLCTR_S *)fs_get(sizeof(CLCTR_S));  \
			   memset(ED(X), 0, sizeof(CLCTR_S));	\
			   HD(X)->token = html_element_collector;	\
			 }

#define	FREE_CLCTR(X)	{						\
			   if(ED(X)->attribs){				\
			       PARAMETER *p;				\
			       while((p = ED(X)->attribs) != NULL){	\
				   ED(X)->attribs = ED(X)->attribs->next; \
				   if(p->attribute)			\
				     fs_give((void **)&p->attribute);	\
				   if(p->value)				\
				     fs_give((void **)&p->value);	\
				   fs_give((void **)&p);		\
			       }					\
			   }						\
			   if(ED(X)->element)				\
			     fs_give((void **) &ED(X)->element);	\
			   fs_give((void **) &ED(X));			\
			   HD(X)->token = NULL;				\
			 }
#define	HANDLERS(X)	(HD(X)->h_stack)
#define	BOLD_BIT(X)	(HD(X)->bold_on)
#define	ULINE_BIT(X)	(HD(X)->uline_on)
#define	CENTER_BIT(X)	(HD(X)->center)
#define	HTML_FLUSH(X)	{						    \
			   html_write(X, (X)->line, (X)->linep - (X)->line); \
			   (X)->linep = (X)->line;			    \
			   (X)->f2 = 0L;   				    \
			 }
#define	HTML_BOLD(X, S) if(! STRIP(X)){					\
			   if((S)){					\
			       html_output((X), TAG_EMBED);		\
			       html_output((X), TAG_BOLDON);		\
			   }						\
			   else if(!(S)){				\
			       html_output((X), TAG_EMBED);		\
			       html_output((X), TAG_BOLDOFF);		\
			   }						\
			 }
#define	HTML_ULINE(X, S)						\
			 if(! STRIP(X)){				\
			   if((S)){					\
			       html_output((X), TAG_EMBED);		\
			       html_output((X), TAG_ULINEON);		\
			   }						\
			   else if(!(S)){				\
			       html_output((X), TAG_EMBED);		\
			       html_output((X), TAG_ULINEOFF);		\
			   }						\
			 }
#define	HTML_ITALIC(X, S)						\
			 if(! STRIP(X)){				\
			   if(S){					\
			       html_output((X), TAG_EMBED);		\
			       html_output((X), TAG_ITALICON);		\
			   }						\
			   else if(!(S)){				\
			       html_output((X), TAG_EMBED);		\
			       html_output((X), TAG_ITALICOFF);		\
			   }						\
			 }
#define	HTML_STRIKE(X, S)						\
			 if(! STRIP(X)){				\
			   if(S){					\
			       html_output((X), TAG_EMBED);		\
			       html_output((X), TAG_STRIKEON);		\
			   }						\
			   else if(!(S)){				\
			       html_output((X), TAG_EMBED);		\
			       html_output((X), TAG_STRIKEOFF);		\
			   }						\
			 }
#define	HTML_BIG(X, S)							\
			 if(! STRIP(X)){				\
			   if(S){					\
			       html_output((X), TAG_EMBED);		\
			       html_output((X), TAG_BIGON);		\
			   }						\
			   else if(!(S)){				\
			       html_output((X), TAG_EMBED);		\
			       html_output((X), TAG_BIGOFF);		\
			   }						\
			 }
#define	HTML_SMALL(X, S)							\
			 if(! STRIP(X)){				\
			   if(S){					\
			       html_output((X), TAG_EMBED);		\
			       html_output((X), TAG_SMALLON);		\
			   }						\
			   else if(!(S)){				\
			       html_output((X), TAG_EMBED);		\
			       html_output((X), TAG_SMALLOFF);		\
			   }						\
			 }
#define WRAPPED_LEN(X)	((HD(f)->centered) \
			    ? (HD(f)->centered->line.width \
				+ HD(f)->centered->word.width \
				+ ((HD(f)->centered->line.width \
				    && HD(f)->centered->word.width) \
				    ? 1 : 0)) \
			    : 0)
#define	HTML_DUMP_LIT(F, S, L)	{					    \
				   int i, c;				    \
				   for(i = 0; i < (L); i++){		    \
				       c = ASCII_ISSPACE((unsigned char)(S)[i])   \
					     ? (S)[i]			    \
					     : MAKE_LITERAL((S)[i]);	    \
				       HTML_TEXT(F, c);			    \
				   }					    \
				 }
#define	HTML_PROC(F, C) {						    \
			   if(HD(F)->token){				    \
			       int i;					    \
			       if((i = (*(HD(F)->token))(F, C)) != 0){	    \
				   if(i < 0){				    \
				       HTML_DUMP_LIT(F, "<", 1);	    \
				       if(HD(F)->el_data->element){	    \
					   HTML_DUMP_LIT(F,		    \
					    HD(F)->el_data->element,	    \
					    strlen(HD(F)->el_data->element));\
				       }				    \
				       if(HD(F)->el_data->len){		    \
					   HTML_DUMP_LIT(F,		    \
						    HD(F)->el_data->buf,    \
						    HD(F)->el_data->len);   \
				       }				    \
				       HTML_TEXT(F, C);			    \
				   }					    \
				   FREE_CLCTR(F);			    \
			       }					    \
			    }						    \
			    else if((C) == '<'){			    \
				NEW_CLCTR(F);				    \
			    }						    \
			    else					    \
			      HTML_TEXT(F, C);				    \
			  }
#define HTML_LINEP_PUTC(F, C) {						    \
		   if((F)->linep - (F)->line >= (HD(F)->line_bufsize - 1)){ \
		       size_t offset = (F)->linep - (F)->line;		    \
		       fs_resize((void **) &(F)->line,			    \
				 (HD(F)->line_bufsize * 2) * sizeof(char)); \
		       HD(F)->line_bufsize *= 2;			    \
		       (F)->linep = &(F)->line[offset];			    \
		   }							    \
		   *(F)->linep++ = (C);					    \
	       }
#define	HTML_TEXT(F, C)	switch((F)->f1){				    \
			     case WSPACE :				    \
			       if(HTML_ISSPACE(C)) /* ignore repeated WS */  \
				 break;					    \
			       HTML_TEXT_OUT(F, ' ');	    \
			       (F)->f1 = DFL;/* stop sending chars here */   \
			       /* fall thru to process 'c' */		    \
			     case DFL:					    \
			       if(HD(F)->bitbucket)			    \
				 (F)->f1 = DFL;	/* no op */		    \
			       else if(HTML_ISSPACE(C) && HD(F)->wrapstate)  \
				 (F)->f1 = WSPACE;/* coalesce white space */ \
			       else HTML_TEXT_OUT(F, C);		    \
			       break;					    \
			 }
#define	HTML_TEXT_OUT(F, C) if(HANDLERS(F)) /* let handlers see C */	    \
			      (*EL(HANDLERS(F))->handler)(HANDLERS(F),(C),GF_DATA); \
			     else					    \
			       html_output(F, C);
#ifdef	DEBUG
#define	HTML_DEBUG_EL(S, D)   {						    \
				 dprint((5, "-- html %s: %s\n",  \
					    S ? S : "?",		    \
					    (D)->element		    \
						 ? (D)->element : "NULL")); \
				 if(debug > 5){				    \
				     PARAMETER *p;			    \
				     for(p = (D)->attribs;		    \
					 p && p->attribute;		    \
					 p = p->next)			    \
				       dprint((6,		    \
						  " PARM: %s%s%s\n",	    \
						  p->attribute		    \
						    ? p->attribute : "NULL",\
						  p->value ? "=" : "",	    \
						  p->value ? p->value : ""));\
				 }					    \
			       }
#else
#define	HTML_DEBUG_EL(S, D)
#endif

#ifndef SYSTEM_PINE_INFO_PATH
#define SYSTEM_PINE_INFO_PATH "/usr/local/lib/pine.info"
#endif
#define CHTML_VAR_EXPAND(S) (!strcmp(S, "PINE_INFO_PATH")   \
			     ? SYSTEM_PINE_INFO_PATH : S)

/*
 * Protos for Tag handlers
 */
int	html_head(HANDLER_S *, int, int);
int	html_base(HANDLER_S *, int, int);
int	html_title(HANDLER_S *, int, int);
int	html_body(HANDLER_S *, int, int);
int	html_a(HANDLER_S *, int, int);
int	html_br(HANDLER_S *, int, int);
int	html_hr(HANDLER_S *, int, int);
int	html_p(HANDLER_S *, int, int);
int	html_table(HANDLER_S *, int, int);
int	html_caption(HANDLER_S *, int, int);
int	html_tr(HANDLER_S *, int, int);
int	html_td(HANDLER_S *, int, int);
int	html_th(HANDLER_S *, int, int);
int	html_thead(HANDLER_S *, int, int);
int	html_tbody(HANDLER_S *, int, int);
int	html_tfoot(HANDLER_S *, int, int);
int	html_col(HANDLER_S *, int, int);
int	html_colgroup(HANDLER_S *, int, int);
int	html_b(HANDLER_S *, int, int);
int	html_u(HANDLER_S *, int, int);
int	html_i(HANDLER_S *, int, int);
int	html_em(HANDLER_S *, int, int);
int	html_strong(HANDLER_S *, int, int);
int	html_s(HANDLER_S *, int, int);
int	html_big(HANDLER_S *, int, int);
int	html_small(HANDLER_S *, int, int);
int	html_font(HANDLER_S *, int, int);
int	html_img(HANDLER_S *, int, int);
int	html_map(HANDLER_S *, int, int);
int	html_area(HANDLER_S *, int, int);
int	html_form(HANDLER_S *, int, int);
int	html_input(HANDLER_S *, int, int);
int	html_option(HANDLER_S *, int, int);
int	html_optgroup(HANDLER_S *, int, int);
int	html_button(HANDLER_S *, int, int);
int	html_select(HANDLER_S *, int, int);
int	html_textarea(HANDLER_S *, int, int);
int	html_label(HANDLER_S *, int, int);
int	html_fieldset(HANDLER_S *, int, int);
int	html_ul(HANDLER_S *, int, int);
int	html_ol(HANDLER_S *, int, int);
int	html_menu(HANDLER_S *, int, int);
int	html_dir(HANDLER_S *, int, int);
int	html_li(HANDLER_S *, int, int);
int	html_h1(HANDLER_S *, int, int);
int	html_h2(HANDLER_S *, int, int);
int	html_h3(HANDLER_S *, int, int);
int	html_h4(HANDLER_S *, int, int);
int	html_h5(HANDLER_S *, int, int);
int	html_h6(HANDLER_S *, int, int);
int	html_blockquote(HANDLER_S *, int, int);
int	html_address(HANDLER_S *, int, int);
int	html_pre(HANDLER_S *, int, int);
int	html_center(HANDLER_S *, int, int);
int	html_div(HANDLER_S *, int, int);
int	html_span(HANDLER_S *, int, int);
int	html_dl(HANDLER_S *, int, int);
int	html_dt(HANDLER_S *, int, int);
int	html_dd(HANDLER_S *, int, int);
int	html_script(HANDLER_S *, int, int);
int	html_applet(HANDLER_S *, int, int);
int	html_style(HANDLER_S *, int, int);
int	html_kbd(HANDLER_S *, int, int);
int	html_dfn(HANDLER_S *, int, int);
int	html_var(HANDLER_S *, int, int);
int	html_tt(HANDLER_S *, int, int);
int	html_samp(HANDLER_S *, int, int);
int	html_sub(HANDLER_S *, int, int);
int	html_sup(HANDLER_S *, int, int);
int	html_cite(HANDLER_S *, int, int);
int	html_code(HANDLER_S *, int, int);
int	html_ins(HANDLER_S *, int, int);
int	html_del(HANDLER_S *, int, int);
int	html_abbr(HANDLER_S *, int, int);

/*
 * Protos for RSS 2.0 Tag handlers
 */
int	rss_rss(HANDLER_S *, int, int);
int	rss_channel(HANDLER_S *, int, int);
int	rss_title(HANDLER_S *, int, int);
int	rss_image(HANDLER_S *, int, int);
int	rss_link(HANDLER_S *, int, int);
int	rss_description(HANDLER_S *, int, int);
int	rss_ttl(HANDLER_S *, int, int);
int	rss_item(HANDLER_S *, int, int);

/*
 * Proto's for support routines
 */
void	  html_pop(FILTER_S *, ELPROP_S *);
int	  html_push(FILTER_S *, ELPROP_S *);
int	  html_element_collector(FILTER_S *, int);
int	  html_element_flush(CLCTR_S *);
void	  html_element_comment(FILTER_S *, char *);
void	  html_element_output(FILTER_S *, int);
int	  html_entity_collector(FILTER_S *, int, UCS *, char **);
void	  html_a_prefix(FILTER_S *);
void	  html_a_finish(HANDLER_S *);
void	  html_a_output_prefix(FILTER_S *, int);
void	  html_a_output_info(HANDLER_S *);
void	  html_a_relative(char *, char *, HANDLE_S *);
int	  html_href_relative(char *);
int	  html_indent(FILTER_S *, int, int);
void	  html_blank(FILTER_S *, int);
void	  html_newline(FILTER_S *);
void	  html_output(FILTER_S *, int);
void	  html_output_string(FILTER_S *, char *);
void	  html_output_raw_tag(FILTER_S *, char *);
void	  html_output_normal(FILTER_S *, int, int);
void	  html_output_flush(FILTER_S *);
void	  html_output_centered(FILTER_S *, int, int);
void	  html_centered_handle(int *, char *, int);
void	  html_centered_putc(WRAPLINE_S *, int);
void	  html_centered_flush(FILTER_S *);
void	  html_centered_flush_line(FILTER_S *);
void	  html_write_anchor(FILTER_S *, int);
void	  html_write_newline(FILTER_S *);
void	  html_write_indent(FILTER_S *, int);
void	  html_write(FILTER_S *, char *, int);
void	  html_putc(FILTER_S *, int);
int	  html_event_attribute(char *);
char	 *rss_skip_whitespace(char *s);
ELPROP_S *element_properties(FILTER_S *, char *);


/*
 * Named entity table -- most from HTML 2.0 (rfc1866) plus some from
 *			 W3C doc "Additional named entities for HTML"
 */
static struct html_entities {
    char *name;			/* entity name */
    UCS   value;		/* UCS entity value */
    char  *plain;		/* US-ASCII representation */
} entity_tab[] = {
    {"quot",		0x0022},	    /* 34 - quotation mark */
    {"amp",		0x0026},	    /* 38 - ampersand */
    {"apos",		0x0027},	    /* 39 - apostrophe */
    {"lt",		0x003C},	    /* 60 - less-than sign */
    {"gt",		0x003E},	    /* 62 - greater-than sign */
    {"nbsp",		0x00A0, " "},	    /* 160 - no-break space */
    {"iexcl",		0x00A1},	    /* 161 - inverted exclamation mark */
    {"cent",		0x00A2},	    /* 162 - cent sign */
    {"pound",		0x00A3},	    /* 163 - pound sign */
    {"curren",		0x00A4, "CUR"},	    /* 164 - currency sign */
    {"yen",		0x00A5},	    /* 165 - yen sign */
    {"brvbar",		0x00A6, "|"},	    /* 166 - broken bar */
    {"sect",		0x00A7},	    /* 167 - section sign */
    {"uml",		0x00A8, "\""},	    /* 168 - diaeresis */
    {"copy",		0x00A9, "(C)"},	    /* 169 - copyright sign */
    {"ordf",		0x00AA, "a"},	    /* 170 - feminine ordinal indicator */
    {"laquo",		0x00AB, "<<"},	    /* 171 - left-pointing double angle quotation mark */
    {"not",		0x00AC, "NOT"},	    /* 172 - not sign */
    {"shy",		0x00AD, "-"},	    /* 173 - soft hyphen */
    {"reg",		0x00AE, "(R)"},	    /* 174 - registered sign */
    {"macr",		0x00AF},	    /* 175 - macron */
    {"deg",		0x00B0, "DEG"},	    /* 176 - degree sign */
    {"plusmn",		0x00B1, "+/-"},	    /* 177 - plus-minus sign */
    {"sup2",		0x00B2},	    /* 178 - superscript two */
    {"sup3",		0x00B3},	    /* 179 - superscript three */
    {"acute",		0x00B4, "'"},	    /* 180 - acute accent */
    {"micro",		0x00B5},	    /* 181 - micro sign */
    {"para",		0x00B6},	    /* 182 - pilcrow sign */
    {"middot",		0x00B7},	    /* 183 - middle dot */
    {"cedil",		0x00B8},	    /* 184 - cedilla */
    {"sup1",		0x00B9},	    /* 185 - superscript one */
    {"ordm",		0x00BA, "o"},	    /* 186 - masculine ordinal indicator */
    {"raquo",		0x00BB, ">>"},	    /* 187 - right-pointing double angle quotation mark */
    {"frac14",		0x00BC, " 1/4"},    /* 188 - vulgar fraction one quarter */
    {"frac12",		0x00BD, " 1/2"},    /* 189 - vulgar fraction one half */
    {"frac34",		0x00BE, " 3/4"},    /* 190 - vulgar fraction three quarters */
    {"iquest",		0x00BF},	    /* 191 - inverted question mark */
    {"Agrave",		0x00C0, "A"},	    /* 192 - latin capital letter a with grave */
    {"Aacute",		0x00C1, "A"},	    /* 193 - latin capital letter a with acute */
    {"Acirc",		0x00C2, "A"},	    /* 194 - latin capital letter a with circumflex */
    {"Atilde",		0x00C3, "A"},	    /* 195 - latin capital letter a with tilde */
    {"Auml",		0x00C4, "AE"},	    /* 196 - latin capital letter a with diaeresis */
    {"Aring",		0x00C5, "A"},	    /* 197 - latin capital letter a with ring above */
    {"AElig",		0x00C6, "AE"},	    /* 198 - latin capital letter ae */
    {"Ccedil",		0x00C7, "C"},	    /* 199 - latin capital letter c with cedilla */
    {"Egrave",		0x00C8, "E"},	    /* 200 - latin capital letter e with grave */
    {"Eacute",		0x00C9, "E"},	    /* 201 - latin capital letter e with acute */
    {"Ecirc",		0x00CA, "E"},	    /* 202 - latin capital letter e with circumflex */
    {"Euml",		0x00CB, "E"},	    /* 203 - latin capital letter e with diaeresis */
    {"Igrave",		0x00CC, "I"},	    /* 204 - latin capital letter i with grave */
    {"Iacute",		0x00CD, "I"},	    /* 205 - latin capital letter i with acute */
    {"Icirc",		0x00CE, "I"},	    /* 206 - latin capital letter i with circumflex */
    {"Iuml",		0x00CF, "I"},	    /* 207 - latin capital letter i with diaeresis */
    {"ETH",		0x00D0, "DH"},	    /* 208 - latin capital letter eth */
    {"Ntilde",		0x00D1, "N"},	    /* 209 - latin capital letter n with tilde */
    {"Ograve",		0x00D2, "O"},	    /* 210 - latin capital letter o with grave */
    {"Oacute",		0x00D3, "O"},	    /* 211 - latin capital letter o with acute */
    {"Ocirc",		0x00D4, "O"},	    /* 212 - latin capital letter o with circumflex */
    {"Otilde",		0x00D5, "O"},	    /* 213 - latin capital letter o with tilde */
    {"Ouml",		0x00D6, "O"},	    /* 214 - latin capital letter o with diaeresis */
    {"times",		0x00D7, "x"},	    /* 215 - multiplication sign */
    {"Oslash",		0x00D8, "O"},	    /* 216 - latin capital letter o with stroke */
    {"Ugrave",		0x00D9, "U"},	    /* 217 - latin capital letter u with grave */
    {"Uacute",		0x00DA, "U"},	    /* 218 - latin capital letter u with acute */
    {"Ucirc",		0x00DB, "U"},	    /* 219 - latin capital letter u with circumflex */
    {"Uuml",		0x00DC, "UE"},	    /* 220 - latin capital letter u with diaeresis */
    {"Yacute",		0x00DD, "Y"},	    /* 221 - latin capital letter y with acute */
    {"THORN",		0x00DE, "P"},	    /* 222 - latin capital letter thorn */
    {"szlig",		0x00DF, "ss"},	    /* 223 - latin small letter sharp s (German <a href="/wiki/Eszett" title="Eszett">Eszett</a>) */
    {"agrave",		0x00E0, "a"},	    /* 224 - latin small letter a with grave */
    {"aacute",		0x00E1, "a"},	    /* 225 - latin small letter a with acute */
    {"acirc",		0x00E2, "a"},	    /* 226 - latin small letter a with circumflex */
    {"atilde",		0x00E3, "a"},	    /* 227 - latin small letter a with tilde */
    {"auml",		0x00E4, "ae"},	    /* 228 - latin small letter a with diaeresis */
    {"aring",		0x00E5, "a"},	    /* 229 - latin small letter a with ring above */
    {"aelig",		0x00E6, "ae"},	    /* 230 - latin lowercase ligature ae */
    {"ccedil",		0x00E7, "c"},	    /* 231 - latin small letter c with cedilla */
    {"egrave",		0x00E8, "e"},	    /* 232 - latin small letter e with grave */
    {"eacute",		0x00E9, "e"},	    /* 233 - latin small letter e with acute */
    {"ecirc",		0x00EA, "e"},	    /* 234 - latin small letter e with circumflex */
    {"euml",		0x00EB, "e"},	    /* 235 - latin small letter e with diaeresis */
    {"igrave",		0x00EC, "i"},	    /* 236 - latin small letter i with grave */
    {"iacute",		0x00ED, "i"},	    /* 237 - latin small letter i with acute */
    {"icirc",		0x00EE, "i"},	    /* 238 - latin small letter i with circumflex */
    {"iuml",		0x00EF, "i"},	    /* 239 - latin small letter i with diaeresis */
    {"eth",		0x00F0, "dh"},	    /* 240 - latin small letter eth */
    {"ntilde",		0x00F1, "n"},	    /* 241 - latin small letter n with tilde */
    {"ograve",		0x00F2, "o"},	    /* 242 - latin small letter o with grave */
    {"oacute",		0x00F3, "o"},	    /* 243 - latin small letter o with acute */
    {"ocirc",		0x00F4, "o"},	    /* 244 - latin small letter o with circumflex */
    {"otilde",		0x00F5, "o"},	    /* 245 - latin small letter o with tilde */
    {"ouml",		0x00F6, "oe"},	    /* 246 - latin small letter o with diaeresis */
    {"divide",		0x00F7, "/"},	    /* 247 - division sign */
    {"oslash",		0x00F8, "o"},	    /* 248 - latin small letter o with stroke */
    {"ugrave",		0x00F9, "u"},	    /* 249 - latin small letter u with grave */
    {"uacute",		0x00FA, "u"},	    /* 250 - latin small letter u with acute */
    {"ucirc",		0x00FB, "u"},	    /* 251 - latin small letter u with circumflex */
    {"uuml",		0x00FC, "ue"},	    /* 252 - latin small letter u with diaeresis */
    {"yacute",		0x00FD, "y"},	    /* 253 - latin small letter y with acute */
    {"thorn",		0x00FE, "p"},	    /* 254 - latin small letter thorn */
    {"yuml",		0x00FF, "y"},	    /* 255 - latin small letter y with diaeresis */
    {"OElig",		0x0152, "OE"},	    /* 338 - latin capital ligature oe */
    {"oelig",		0x0153, "oe"},	    /* 339 - latin small ligature oe */
    {"Scaron",		0x0160, "S"},	    /* 352 - latin capital letter s with caron */
    {"scaron",		0x0161, "s"},	    /* 353 - latin small letter s with caron */
    {"Yuml",		0x0178, "Y"},	    /* 376 - latin capital letter y with diaeresis */
    {"fnof",		0x0192, "f"},	    /* 402 - latin small letter f with hook */
    {"circ",		0x02C6},	    /* 710 - modifier letter circumflex accent */
    {"tilde",		0x02DC, "~"},	    /* 732 - small tilde */
    {"Alpha",		0x0391},	    /* 913 - greek capital letter alpha */
    {"Beta",		0x0392},	    /* 914 - greek capital letter beta */
    {"Gamma",		0x0393},	    /* 915 - greek capital letter gamma */
    {"Delta",		0x0394},	    /* 916 - greek capital letter delta */
    {"Epsilon",		0x0395},	    /* 917 - greek capital letter epsilon */
    {"Zeta",		0x0396},	    /* 918 - greek capital letter zeta */
    {"Eta",		0x0397},	    /* 919 - greek capital letter eta */
    {"Theta",		0x0398},	    /* 920 - greek capital letter theta */
    {"Iota",		0x0399},	    /* 921 - greek capital letter iota */
    {"Kappa",		0x039A},	    /* 922 - greek capital letter kappa */
    {"Lambda",		0x039B},	    /* 923 - greek capital letter lamda */
    {"Mu",		0x039C},	    /* 924 - greek capital letter mu */
    {"Nu",		0x039D},	    /* 925 - greek capital letter nu */
    {"Xi",		0x039E},	    /* 926 - greek capital letter xi */
    {"Omicron",		0x039F},	    /* 927 - greek capital letter omicron */
    {"Pi",		0x03A0},	    /* 928 - greek capital letter pi */
    {"Rho",		0x03A1},	    /* 929 - greek capital letter rho */
    {"Sigma",		0x03A3},	    /* 931 - greek capital letter sigma */
    {"Tau",		0x03A4},	    /* 932 - greek capital letter tau */
    {"Upsilon",		0x03A5},	    /* 933 - greek capital letter upsilon */
    {"Phi",		0x03A6},	    /* 934 - greek capital letter phi */
    {"Chi",		0x03A7},	    /* 935 - greek capital letter chi */
    {"Psi",		0x03A8},	    /* 936 - greek capital letter psi */
    {"Omega",		0x03A9},	    /* 937 - greek capital letter omega */
    {"alpha",		0x03B1},	    /* 945 - greek small letter alpha */
    {"beta",		0x03B2},	    /* 946 - greek small letter beta */
    {"gamma",		0x03B3},	    /* 947 - greek small letter gamma */
    {"delta",		0x03B4},	    /* 948 - greek small letter delta */
    {"epsilon",		0x03B5},	    /* 949 - greek small letter epsilon */
    {"zeta",		0x03B6},	    /* 950 - greek small letter zeta */
    {"eta",		0x03B7},	    /* 951 - greek small letter eta */
    {"theta",		0x03B8},	    /* 952 - greek small letter theta */
    {"iota",		0x03B9},	    /* 953 - greek small letter iota */
    {"kappa",		0x03BA},	    /* 954 - greek small letter kappa */
    {"lambda",		0x03BB},	    /* 955 - greek small letter lamda */
    {"mu",		0x03BC},	    /* 956 - greek small letter mu */
    {"nu",		0x03BD},	    /* 957 - greek small letter nu */
    {"xi",		0x03BE},	    /* 958 - greek small letter xi */
    {"omicron",		0x03BF},	    /* 959 - greek small letter omicron */
    {"pi",		0x03C0},	    /* 960 - greek small letter pi */
    {"rho",		0x03C1},	    /* 961 - greek small letter rho */
    {"sigmaf",		0x03C2},	    /* 962 - greek small letter final sigma */
    {"sigma",		0x03C3},	    /* 963 - greek small letter sigma */
    {"tau",		0x03C4},	    /* 964 - greek small letter tau */
    {"upsilon",		0x03C5},	    /* 965 - greek small letter upsilon */
    {"phi",		0x03C6},	    /* 966 - greek small letter phi */
    {"chi",		0x03C7},	    /* 967 - greek small letter chi */
    {"psi",		0x03C8},	    /* 968 - greek small letter psi */
    {"omega",		0x03C9},	    /* 969 - greek small letter omega */
    {"thetasym",	0x03D1},	    /* 977 - greek theta symbol */
    {"upsih",		0x03D2},	    /* 978 - greek upsilon with hook symbol */
    {"piv",		0x03D6},	    /* 982 - greek pi symbol */
    {"ensp",		0x2002},	    /* 8194 - en space */
    {"emsp",		0x2003},	    /* 8195 - em space */
    {"thinsp",		0x2009},	    /* 8201 - thin space */
    {"zwnj",		0x200C},	    /* 8204 - zero width non-joiner */
    {"zwj",		0x200D},	    /* 8205 - zero width joiner */
    {"lrm",		0x200E},	    /* 8206 - left-to-right mark */
    {"rlm",		0x200F},	    /* 8207 - right-to-left mark */
    {"ndash",		0x2013},	    /* 8211 - en dash */
    {"mdash",		0x2014},	    /* 8212 - em dash */
    {"#8213",		 0x2015, "--"},	    /* 2015 - horizontal bar */
    {"#8214",		 0x2016, "||"},	    /* 2016 - double vertical line */
    {"#8215",		 0x2017, "__"},	    /* 2017 - double low line */
    {"lsquo",		0x2018},	    /* 8216 - left single quotation mark */
    {"rsquo",		0x2019},	    /* 8217 - right single quotation mark */
    {"sbquo",		0x201A},	    /* 8218 - single low-9 quotation mark */
    {"ldquo",		0x201C},	    /* 8220 - left double quotation mark */
    {"rdquo",		0x201D},	    /* 8221 - right double quotation mark */
    {"bdquo",		0x201E, ",,"},	    /* 8222 - double low-9 quotation mark */
    {"#8223",		0x201F, "``"},	    /* 201F -  double high reversed-9 quotation mark  */
    {"dagger",		0x2020},	    /* 8224 - dagger */
    {"Dagger",		0x2021},	    /* 8225 - double dagger */
    {"bull",		0x2022, "*"},	    /* 8226 - bullet */
    {"hellip",		0x2026},	    /* 8230 - horizontal ellipsis */
    {"permil",		0x2030},	    /* 8240 - per mille sign */
    {"prime",		0x2032, "\'"},	    /* 8242 - prime */
    {"Prime",		0x2033, "\'\'"},    /* 8243 - double prime */
    {"#8244",		0x2034, "\'\'\'"},  /* 2034 - triple prime */
    {"lsaquo",		0x2039},	    /* 8249 - single left-pointing angle quotation mark */
    {"rsaquo",		0x203A},	    /* 8250 - single right-pointing angle quotation mark */
    {"#8252",		0x203C, "!!"},	    /* 203C - double exclamation mark */
    {"oline",		0x203E, "-"},	    /* 8254 - overline */
    {"frasl",		0x2044},	    /* 8260 - fraction slash */
    {"#8263",		0x2047, "??"},	    /* 2047 - double question mark */
    {"#8264",		0x2048, "?!"},	    /* 2048 - question exclamation mark */
    {"#8265",		0x2049, "!?"},	    /* 2049 - exclamation question mark */
    {"#8279",		0x2057, "\'\'\'\'"}, /* 2057 - quad prime */
    {"euro",		0x20AC, "EUR"},	    /* 8364 - euro sign */
    {"image",		0x2111},	    /* 8465 - black-letter capital i */
    {"weierp",		0x2118},	    /* 8472 - script capital p (<a href="/wiki/Weierstrass" title="Weierstrass">Weierstrass</a> p) */
    {"real",		0x211C},	    /* 8476 - black-letter capital r */
    {"trade",		0x2122, "[tm]"},    /* 8482 - trademark sign */
    {"alefsym",		0x2135},	    /* 8501 - alef symbol */
    {"larr",		0x2190},	    /* 8592 - leftwards arrow */
    {"uarr",		0x2191},	    /* 8593 - upwards arrow */
    {"rarr",		0x2192},	    /* 8594 - rightwards arrow */
    {"darr",		0x2193},	    /* 8595 - downwards arrow */
    {"harr",		0x2194},	    /* 8596 - left right arrow */
    {"crarr",		0x21B5},	    /* 8629 - downwards arrow with corner leftwards */
    {"lArr",		0x21D0},	    /* 8656 - leftwards double arrow */
    {"uArr",		0x21D1},	    /* 8657 - upwards double arrow */
    {"rArr",		0x21D2},	    /* 8658 - rightwards double arrow */
    {"dArr",		0x21D3},	    /* 8659 - downwards double arrow */
    {"hArr",		0x21D4},	    /* 8660 - left right double arrow */
    {"forall",		0x2200},	    /* 8704 - for all */
    {"part",		0x2202},	    /* 8706 - partial differential */
    {"exist",		0x2203},	    /* 8707 - there exists */
    {"empty",		0x2205},	    /* 8709 - empty set */
    {"nabla",		0x2207},	    /* 8711 - nabla */
    {"isin",		0x2208},	    /* 8712 - element of */
    {"notin",		0x2209},	    /* 8713 - not an element of */
    {"ni",		0x220B},	    /* 8715 - contains as member */
    {"prod",		0x220F},	    /* 8719 - n-ary product */
    {"sum",		0x2211},	    /* 8721 - n-ary summation */
    {"minus",		0x2212},	    /* 8722 - minus sign */
    {"lowast",		0x2217},	    /* 8727 - asterisk operator */
    {"radic",		0x221A},	    /* 8730 - square root */
    {"prop",		0x221D},	    /* 8733 - proportional to */
    {"infin",		0x221E},	    /* 8734 - infinity */
    {"ang",		0x2220},	    /* 8736 - angle */
    {"and",		0x2227},	    /* 8743 - logical and */
    {"or",		0x2228},	    /* 8744 - logical or */
    {"cap",		0x2229},	    /* 8745 - intersection */
    {"cup",		0x222A},	    /* 8746 - union */
    {"int",		0x222B},	    /* 8747 - integral */
    {"there4",		0x2234},	    /* 8756 - therefore */
    {"sim",		0x223C},	    /* 8764 - tilde operator */
    {"cong",		0x2245},	    /* 8773 - congruent to */
    {"asymp",		0x2248},	    /* 8776 - almost equal to */
    {"ne",		0x2260},	    /* 8800 - not equal to */
    {"equiv",		0x2261},	    /* 8801 - identical to (equivalent to) */
    {"le",		0x2264},	    /* 8804 - less-than or equal to */
    {"ge",		0x2265},	    /* 8805 - greater-than or equal to */
    {"sub",		0x2282},	    /* 8834 - subset of */
    {"sup",		0x2283},	    /* 8835 - superset of */
    {"nsub",		0x2284},	    /* 8836 - not a subset of */
    {"sube",		0x2286},	    /* 8838 - subset of or equal to */
    {"supe",		0x2287},	    /* 8839 - superset of or equal to */
    {"oplus",		0x2295},	    /* 8853 - circled plus */
    {"otimes",		0x2297},	    /* 8855 - circled times */
    {"perp",		0x22A5},	    /* 8869 - up tack */
    {"sdot",		0x22C5},	    /* 8901 - dot operator */
    {"lceil",		0x2308},	    /* 8968 - left ceiling */
    {"rceil",		0x2309},	    /* 8969 - right ceiling */
    {"lfloor",		0x230A},	    /* 8970 - left floor */
    {"rfloor",		0x230B},	    /* 8971 - right floor */
    {"lang",		0x2329},	    /* 9001 - left-pointing angle bracket */
    {"rang",		0x232A},	    /* 9002 - right-pointing angle bracket */
    {"loz",		0x25CA},	    /* 9674 - lozenge */
    {"spades",		0x2660},	    /* 9824 - black spade suit */
    {"clubs",		0x2663},	    /* 9827 - black club suit */
    {"hearts",		0x2665},	    /* 9829 - black heart suit */
    {"diams",		0x2666}		    /* 9830 - black diamond suit */
};


/*
 * Table of supported elements and corresponding handlers
 */
static ELPROP_S html_element_table[] = {
    {"HTML"},					/* HTML ignore if seen? */
    {"HEAD",		html_head},		/* slurp until <BODY> ? */
    {"TITLE",		html_title},		/* Document Title */
    {"BASE",		html_base},		/* HREF base */
    {"BODY",		html_body},		/* HTML BODY */
    {"A",		html_a},		/* Anchor */
    {"ABBR",		html_abbr},		/* Abbreviation */
    {"IMG",		html_img},		/* Image */
    {"MAP",		html_map},		/* Image Map */
    {"AREA",		html_area},		/* Image Map Area */
    {"HR",		html_hr, 1},		/* Horizontal Rule */
    {"BR",		html_br},		/* Line Break */
    {"P",		html_p, 1},		/* Paragraph */
    {"OL",		html_ol, 1},		/* Ordered List */
    {"UL",		html_ul, 1},		/* Unordered List */
    {"MENU",		html_menu},		/* Menu List */
    {"DIR",		html_dir},		/* Directory List */
    {"LI",		html_li},		/*  ... List Item */
    {"DL",		html_dl, 1},		/* Definition List */
    {"DT",		html_dt},		/*  ... Def. Term */
    {"DD",		html_dd},		/*  ... Def. Definition */
    {"I",		html_i},		/* Italic Text */
    {"EM",		html_em},		/* Typographic Emphasis */
    {"STRONG",		html_strong},		/* STRONG Typo Emphasis */
    {"VAR",		html_i},		/* Variable Name */
    {"B",		html_b},		/* Bold Text */
    {"U",		html_u},		/* Underline Text */
    {"S",		html_s},		/* Strike-Through Text */
    {"STRIKE",		html_s},		/* Strike-Through Text */
    {"BIG",		html_big},		/* Big Font Text */
    {"SMALL",		html_small},		/* Small Font Text */
    {"FONT",		html_font},		/* Font display directives */
    {"BLOCKQUOTE", 	html_blockquote, 1}, 	/* Blockquote */
    {"ADDRESS",		html_address, 1},	/* Address */
    {"CENTER",		html_center},		/* Centered Text v3.2 */
    {"DIV",		html_div, 1},		/* Document Division 3.2 */
    {"SPAN",		html_span},		/* Text Span */
    {"H1",		html_h1, 1},		/* Headings... */
    {"H2",		html_h2, 1},
    {"H3",		html_h3,1},
    {"H4",		html_h4, 1},
    {"H5",		html_h5, 1},
    {"H6",		html_h6, 1},
    {"PRE",		html_pre, 1},		/* Preformatted Text */
    {"KBD",		html_kbd},		/* Keyboard Input (NO OP) */
    {"DFN",		html_dfn},		/* Definition (NO OP) */
    {"VAR",		html_var},		/* Variable (NO OP) */
    {"TT",		html_tt},		/* Typetype (NO OP) */
    {"SAMP",		html_samp},		/* Sample Text (NO OP) */
    {"CITE",		html_cite},		/* Citation (NO OP) */
    {"CODE",		html_code},		/* Code Text (NO OP) */
    {"INS",		html_ins},		/* Text Inseted (NO OP) */
    {"DEL",		html_del},		/* Text Deleted (NO OP) */
    {"SUP",		html_sup},		/* Text Superscript (NO OP) */
    {"SUB",		html_sub},		/* Text Superscript (NO OP) */
    {"STYLE",		html_style},		/* CSS Definitions */

/*----- Handlers below UNIMPLEMENTED (and won't until later) -----*/

    {"FORM",		html_form, 1},		/* form within a document */
    {"INPUT",		html_input},		/* One input field, options */
    {"BUTTON",		html_button},		/* Push Button */
    {"OPTION",		html_option},		/* One option within Select */
    {"OPTION",		html_optgroup},		/* Option Group Definition */
    {"SELECT",		html_select},		/* Selection from a set */
    {"TEXTAREA",	html_textarea},		/* A multi-line input field */
    {"LABEL",		html_label},		/* Control Label */
    {"FIELDSET",	html_fieldset, 1},	/* Fieldset Control Group */

/*----- Handlers below NEVER TO BE IMPLEMENTED -----*/
    {"SCRIPT",		html_script},		/* Embedded scripting statements */
    {"APPLET",		NULL},			/* Embedded applet statements */
    {"OBJECT",		NULL},			/* Embedded object statements */
    {"LINK",		NULL},			/* References to external data */
    {"PARAM",		NULL},			/* Applet/Object parameters */

/*----- Handlers below provide limited support for RFC 1942 Tables -----*/

    {"TABLE",		html_table, 1},		/* Table */
    {"CAPTION",		html_caption},		/* Table Caption */
    {"TR",		html_tr},		/* Table Table Row */
    {"TD",		html_td},		/* Table Table Data */
    {"TH",		html_th},		/* Table Table Head */
    {"THEAD",		html_thead},		/* Table Table Head */
    {"TBODY",		html_tbody},		/* Table Table Body */
    {"TFOOT",		html_tfoot},		/* Table Table Foot */
    {"COL",		html_col},		/* Table Column Attibutes */
    {"COLGROUP",	html_colgroup},		/* Table Column Group Attibutes */

    {NULL,		NULL}
};


/*
 * Table of supported RSS 2.0 elements
 */
static ELPROP_S rss_element_table[] = {
    {"RSS",		rss_rss},		/* RSS 2.0 version */
    {"CHANNEL",		rss_channel},		/* RSS 2.0 Channel */
    {"TITLE",		rss_title},		/* RSS 2.0 Title */
    {"IMAGE",		rss_image},		/* RSS 2.0 Channel Image */
    {"LINK",		rss_link},		/* RSS 2.0 Channel/Item Link */
    {"DESCRIPTION",	rss_description},	/* RSS 2.0 Channel/Item Description */
    {"ITEM",		rss_item},		/* RSS 2.0 Channel ITEM */
    {"TTL",		rss_ttl},		/* RSS 2.0 Item TTL */
    {NULL,		NULL}
};


/*
 * Initialize the given handler, and add it to the stack if it
 * requests it.
 *
 * Returns: 1 if handler chose to get pushed on stack
 *          0 if handler declined
 */
int
html_push(FILTER_S *fd, ELPROP_S *ep)
{
    HANDLER_S *new;

    new = (HANDLER_S *)fs_get(sizeof(HANDLER_S));
    memset(new, 0, sizeof(HANDLER_S));
    new->html_data = fd;
    new->element   = ep;
    if((*ep->handler)(new, 0, GF_RESET)){ /* stack the handler? */
	 new->below   = HANDLERS(fd);
	 HANDLERS(fd) = new;		/* push */
	 return(1);
    }

    fs_give((void **) &new);
    return(0);
}


/*
 * Remove the most recently installed the given handler
 * after letting it accept its demise.
 */
void
html_pop(FILTER_S *fd, ELPROP_S *ep)
{
    HANDLER_S *tp;

    for(tp = HANDLERS(fd); tp && ep != EL(tp); tp = tp->below){
	HANDLER_S *tp2;
	ELPROP_S  *ep2;
	
	dprint((3, "-- html error: bad nesting: given /%s expected /%s", ep->element, EL(tp)->element));
	/* if no evidence of opening tag, ignore given closing tag */
	for(tp2 = HANDLERS(fd); tp2 && ep != EL(tp2); tp2 = tp2->below)
	  ;

	if(!tp2){
	    dprint((3, "-- html error: no opening tag for given tag /%s", ep->element));
	    return;
	}

	(void) (*EL(tp)->handler)(tp, 0, GF_EOD);
	HANDLERS(fd) = tp->below;
    }

    if(tp){
	(void) (*EL(tp)->handler)(tp, 0, GF_EOD);	/* may adjust handler list */
	if(tp != HANDLERS(fd)){
	    HANDLER_S *p;

	    for(p = HANDLERS(fd); p->below != tp; p = p->below)
	      ;

	    if(p)
	      p->below = tp->below;	/* remove from middle of stack */
	    /* BUG: else programming botch and we should die */
	}
	else
	  HANDLERS(fd) = tp->below;	/* pop */

	fs_give((void **)&tp);
    }
    else{
	/* BUG: should MAKE SURE NOT TO EMIT IT */
	dprint((3, "-- html error: end tag without a start: %s", ep->element));
    }
}


/*
 * Deal with data passed a hander in its GF_DATA state
 */
static void
html_handoff(HANDLER_S *hd, int ch)
{
    if(hd->below)
      (void) (*EL(hd->below)->handler)(hd->below, ch, GF_DATA);
    else
      html_output(hd->html_data, ch);
}


/*
 * HTML <BR> element handler
 */
int
html_br(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "br");
	}
	else{
	    html_output(hd->html_data, HTML_NEWLINE);
	}
    }

    return(0);				/* don't get linked */
}


/*
 * HTML <HR> (Horizontal Rule) element handler
 */
int
html_hr(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "hr");
	}
	else{
	    int	   i, old_wrap, width, align;
	    PARAMETER *p;

	    width = WRAP_COLS(hd->html_data);
	    align = 0;
	    for(p = HD(hd->html_data)->el_data->attribs;
		p && p->attribute;
		p = p->next)
	      if(p->value){
		  if(!strucmp(p->attribute, "ALIGN")){
		      if(!strucmp(p->value, "LEFT"))
			align = 1;
		      else if(!strucmp(p->value, "RIGHT"))
			align = 2;
		  }
		  else if(!strucmp(p->attribute, "WIDTH")){
		      char *cp;

		      width = 0;
		      for(cp = p->value; *cp; cp++)
			if(*cp == '%'){
			    width = (WRAP_COLS(hd->html_data)*MIN(100,width))/100;
			    break;
			}
			else if(isdigit((unsigned char) *cp))
			  width = (width * 10) + (*cp - '0');

		      width = MIN(width, WRAP_COLS(hd->html_data));
		  }
	      }

	    html_blank(hd->html_data, 1);	/* at least one blank line */

	    old_wrap = HD(hd->html_data)->wrapstate;
	    HD(hd->html_data)->wrapstate = 0;
	    if((i = MAX(0, WRAP_COLS(hd->html_data) - width))
	       && ((align == 0) ? i /= 2 : (align == 2)))
	      for(; i > 0; i--)
		html_output(hd->html_data, ' ');

	    for(i = 0; i < width; i++)
	      html_output(hd->html_data, '_');

	    html_blank(hd->html_data, 1);
	    HD(hd->html_data)->wrapstate = old_wrap;
	}
    }

    return(0);				/* don't get linked */
}


/*
 * HTML <P> (paragraph) element handler
 */
int
html_p(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "p");
	}
	else{
	    /* Make sure there's at least 1 blank line */
	    html_blank(hd->html_data, 1);

	    /* adjust indent level if needed */
	    if(HD(hd->html_data)->li_pending){
		html_indent(hd->html_data, 4, HTML_ID_INC);
		HD(hd->html_data)->li_pending = 0;
	    }
	}
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</p>");
	}
	else{
	    /* Make sure there's at least 1 blank line */
	    html_blank(hd->html_data, 1);
	}
    }

    return(1);				/* GET linked */
}


/*
 * HTML Table <TABLE> (paragraph) table row
 */
int
html_table(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	if(PASS_HTML(hd->html_data)){
	    html_handoff(hd, ch);
	}
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "table");
	}
	else
	  /* Make sure there's at least 1 blank line */
	  html_blank(hd->html_data, 0);
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</table>");
	}
	else
	  /* Make sure there's at least 1 blank line */
	  html_blank(hd->html_data, 0);
    }
    return(PASS_HTML(hd->html_data));		/* maybe get linked */
}


/*
 * HTML <CAPTION> (Table Caption) element handler
 */
int
html_caption(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "caption");
	}
	else{
	    /* turn ON the centered bit */
	    CENTER_BIT(hd->html_data) = 1;
	}
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</caption>");
	}
	else{
	    /* turn OFF the centered bit */
	    CENTER_BIT(hd->html_data) = 0;
	}
    }

    return(1);
}


/*
 * HTML Table <TR> (paragraph) table row
 */
int
html_tr(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	if(PASS_HTML(hd->html_data)){
	    html_handoff(hd, ch);
	}
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "tr");
	}
	else
	  /* Make sure there's at least 1 blank line */
	  html_blank(hd->html_data, 0);
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</tr>");
	}
	else
	  /* Make sure there's at least 1 blank line */
	  html_blank(hd->html_data, 0);
    }
    return(PASS_HTML(hd->html_data));		/* maybe get linked */
}


/*
 * HTML Table <TD> (paragraph) table data
 */
int
html_td(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	if(PASS_HTML(hd->html_data)){
	    html_handoff(hd, ch);
	}
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "td");
	}
	else{
	    PARAMETER *p;

	    for(p = HD(hd->html_data)->el_data->attribs;
		p && p->attribute;
		p = p->next)
	      if(!strucmp(p->attribute, "nowrap")
		 && (hd->html_data->f2 || hd->html_data->n)){
		  HTML_DUMP_LIT(hd->html_data, " | ", 3);
		  break;
	      }
	}
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</td>");
	}
    }

    return(PASS_HTML(hd->html_data));		/* maybe get linked */
}


/*
 * HTML Table <TH> (paragraph) table head
 */
int
html_th(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	if(PASS_HTML(hd->html_data)){
	    html_handoff(hd, ch);
	}
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "th");
	}
	else{
	    PARAMETER *p;

	    for(p = HD(hd->html_data)->el_data->attribs;
		p && p->attribute;
		p = p->next)
	      if(!strucmp(p->attribute, "nowrap")
		 && (hd->html_data->f2 || hd->html_data->n)){
		  HTML_DUMP_LIT(hd->html_data, " | ", 3);
		  break;
	      }
	  }
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</th>");
	}
    }

    return(PASS_HTML(hd->html_data));		/* don't get linked */
}


/*
 * HTML Table <THEAD> table head
 */
int
html_thead(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "thead");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</thead>");
	}

	return(1);		/* GET linked */
    }

    return(0);		/* don't get linked */
}


/*
 * HTML Table <TBODY> table body
 */
int
html_tbody(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "tbody");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</tbody>");
	}

	return(1);		/* GET linked */
    }

    return(0);		/* don't get linked */
}


/*
 * HTML Table <TFOOT> table body
 */
int
html_tfoot(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "tfoot");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</tfoot>");
	}

	return(1);		/* GET linked */
    }

    return(0);		/* don't get linked */
}


/*
 * HTML <COL> (Table Column Attributes) element handler
 */
int
html_col(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "col");
	}
    }

    return(0);				/* don't get linked */
}


/*
 * HTML Table <COLGROUP> table body
 */
int
html_colgroup(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "colgroup");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</colgroup>");
	}

	return(1);		/* GET linked */
    }

    return(0);		/* don't get linked */
}


/*
 * HTML <I> (italic text) element handler
 */
int
html_i(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	/* include LITERAL in spaceness test! */
	if(hd->x && !ASCII_ISSPACE((unsigned char) (ch & 0xff))){
	    HTML_ITALIC(hd->html_data, 1);
	    hd->x = 0;
	}

	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	hd->x = 1;
    }
    else if(cmd == GF_EOD){
	if(!hd->x)
	  HTML_ITALIC(hd->html_data, 0);
    }

    return(1);				/* get linked */
}


/*
 * HTML <EM> element handler
 */
int
html_em(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	if(!PASS_HTML(hd->html_data)){
	    /* include LITERAL in spaceness test! */
	    if(hd->x && !ASCII_ISSPACE((unsigned char) (ch & 0xff))){
		HTML_ITALIC(hd->html_data, 1);
		hd->x = 0;
	    }
	}

	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "em");
	}
	else{
	    hd->x = 1;
	}
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</em>");
	}
	else{
	    if(!hd->x)
	      HTML_ITALIC(hd->html_data, 0);
	}
    }

    return(1);				/* get linked */
}


/*
 * HTML <STRONG> element handler
 */
int
html_strong(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	if(!PASS_HTML(hd->html_data)){
	    /* include LITERAL in spaceness test! */
	    if(hd->x && !ASCII_ISSPACE((unsigned char) (ch & 0xff))){
		HTML_ITALIC(hd->html_data, 1);
		hd->x = 0;
	    }
	}

	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "strong");
	}
	else{
	    hd->x = 1;
	}
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</strong>");
	}
	else{
	    if(!hd->x)
	      HTML_ITALIC(hd->html_data, 0);
	}
    }

    return(1);				/* get linked */
}


/*
 * HTML <u> (Underline text) element handler
 */
int
html_u(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "u");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</u>");
	}

	return(1);		/* get linked */
    }

    return(0);			/* do NOT get linked */
}


/*
 * HTML <b> (Bold text) element handler
 */
int
html_b(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	if(!PASS_HTML(hd->html_data)){
	    /* include LITERAL in spaceness test! */
	    if(hd->x && !ASCII_ISSPACE((unsigned char) (ch & 0xff))){
		HTML_BOLD(hd->html_data, 1);
		hd->x = 0;
	    }
	}
	
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "b");
	}
	else{
	    hd->x = 1;
	}
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</b>");
	}
	else{
	    if(!hd->x)
	      HTML_BOLD(hd->html_data, 0);
	}
    }

    return(1);				/* get linked */
}


/*
 * HTML <s> (strike-through text) element handler
 */
int
html_s(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	if(!PASS_HTML(hd->html_data)){
	    /* include LITERAL in spaceness test! */
	    if(hd->x && !ASCII_ISSPACE((unsigned char) (ch & 0xff))){
		HTML_STRIKE(hd->html_data, 1);
		hd->x = 0;
	    }
	}

	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "s");
	}
	else{
	    hd->x = 1;
	}
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</s>");
	}
	else{
	    if(!hd->x)
	      HTML_STRIKE(hd->html_data, 0);
	}
    }

    return(1);				/* get linked */
}


/*
 * HTML <big> (BIG text) element handler
 */
int
html_big(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	/* include LITERAL in spaceness test! */
	if(hd->x && !ASCII_ISSPACE((unsigned char) (ch & 0xff))){
	    HTML_BIG(hd->html_data, 1);
	    hd->x = 0;
	}

	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	hd->x = 1;
    }
    else if(cmd == GF_EOD){
	if(!hd->x)
	  HTML_BIG(hd->html_data, 0);
    }

    return(1);				/* get linked */
}


/*
 * HTML <small> (SMALL text) element handler
 */
int
html_small(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	/* include LITERAL in spaceness test! */
	if(hd->x && !ASCII_ISSPACE((unsigned char) (ch & 0xff))){
	    HTML_SMALL(hd->html_data, 1);
	    hd->x = 0;
	}

	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	hd->x = 1;
    }
    else if(cmd == GF_EOD){
	if(!hd->x)
	  HTML_SMALL(hd->html_data, 0);
    }

    return(1);				/* get linked */
}


/*
 * HTML <FONT> element handler
 */
int
html_font(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "font");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</font>");
	}

	return(1);				/* get linked */
    }

    return(0);
}


/*
 * HTML <IMG> element handler
 */
int
html_img(HANDLER_S *hd, int ch, int cmd)
{
    PARAMETER *p;
    char      *alt = NULL, *src = NULL, *s;

    if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "img");
	}
	else{
	    for(p = HD(hd->html_data)->el_data->attribs;
		p && p->attribute;
		p = p->next)
	      if(p->value && p->value[0]){
		  if(!strucmp(p->attribute, "alt"))
		    alt = p->value;
		  if(!strucmp(p->attribute, "src"))
		    src = p->value;
	      }

	    /*
	     * Multipart/Related Content ID pointer
	     * ONLY attached messages are recognized
	     * if we ever decide web bugs aren't a problem
	     * anymore then we might expand the scope
	     */
	    if(src
	       && DO_HANDLES(hd->html_data)
	       && RELATED_OK(hd->html_data)
	       && struncmp(src, "cid:", 4) == 0){
		char      buf[32];
		int	      i, n;
		HANDLE_S *h = new_handle(HANDLESP(hd->html_data));

		h->type	 = IMG;
		h->h.img.src = cpystr(src + 4);
		h->h.img.alt = cpystr((alt) ? alt : "Attached Image");

		HTML_TEXT(hd->html_data, TAG_EMBED);
		HTML_TEXT(hd->html_data, TAG_HANDLE);

		sprintf(buf, "%d", h->key);
		n = strlen(buf);
		HTML_TEXT(hd->html_data, n);
		for(i = 0; i < n; i++){
		    unsigned int uic = buf[i];
		    HTML_TEXT(hd->html_data, uic);
		}

		return(0);
	    }
	    else if(alt && strlen(alt) < 256){ /* arbitrary "reasonable" limit */
		HTML_DUMP_LIT(hd->html_data, alt, strlen(alt));
		HTML_TEXT(hd->html_data, ' ');
		return(0);
	    }
	    else if(src
		    && (s = strrindex(src, '/'))
		    && *++s != '\0'){
		HTML_TEXT(hd->html_data, '[');
		HTML_DUMP_LIT(hd->html_data, s, strlen(s));
		HTML_TEXT(hd->html_data, ']');
		HTML_TEXT(hd->html_data, ' ');
		return(0);
	    }

	    /* text filler of last resort */
	    HTML_DUMP_LIT(hd->html_data, "[IMAGE] ", 7);
	}
    }

    return(0);				/* don't get linked */
}


/*
 * HTML <MAP> (Image Map) element handler
 */
int
html_map(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data) && PASS_IMAGES(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "map");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</map>");
	}

	return(1);
    }

    return(0);
}


/*
 * HTML <AREA> (Image Map Area) element handler
 */
int
html_area(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data) && PASS_IMAGES(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "area");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</area>");
	}

	return(1);
    }

    return(0);
}


/*
 * HTML <FORM> (Form) element handler
 */
int
html_form(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    PARAMETER **pp;

	    /* SECURITY: make sure to redirect to new browser instance */
	    for(pp = &(HD(hd->html_data)->el_data->attribs);
		*pp && (*pp)->attribute;
		pp = &(*pp)->next)
	      if(!strucmp((*pp)->attribute, "target")){
		  if((*pp)->value)
		    fs_give((void **) &(*pp)->value);

		  (*pp)->value = cpystr("_blank");
	      }

	    if(!*pp){
		*pp = (PARAMETER *)fs_get(sizeof(PARAMETER));
		memset(*pp, 0, sizeof(PARAMETER));
		(*pp)->attribute = cpystr("target");
		(*pp)->value = cpystr("_blank");
	    }

	    html_output_raw_tag(hd->html_data, "form");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</form>");
	}
    }
    else{
	if(cmd == GF_RESET){
	    html_blank(hd->html_data, 0);
	    HTML_DUMP_LIT(hd->html_data, "[FORM]", 6);
	    html_blank(hd->html_data, 0);
	}
    }

    return(PASS_HTML(hd->html_data));		/* maybe get linked */
}


/*
 * HTML <INPUT> (Form) element handler
 */
int
html_input(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "input");
	}
    }

    return(0);				/* don't get linked */
}


/*
 * HTML <BUTTON> (Form) element handler
 */
int
html_button(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "button");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</button>");
	}

	return(1);				/* get linked */
    }

    return(0);
}


/*
 * HTML <OPTION> (Form) element handler
 */
int
html_option(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "option");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</option>");
	}

	return(1);				/* get linked */
    }

    return(0);
}


/*
 * HTML <OPTGROUP> (Form) element handler
 */
int
html_optgroup(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "optgroup");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</optgroup>");
	}

	return(1);				/* get linked */
    }

    return(0);
}


/*
 * HTML <SELECT> (Form) element handler
 */
int
html_select(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "select");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</select>");
	}

	return(1);				/* get linked */
    }

    return(0);
}


/*
 * HTML <TEXTAREA> (Form) element handler
 */
int
html_textarea(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "textarea");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</textarea>");
	}

	return(1);				/* get linked */
    }

    return(0);
}


/*
 * HTML <LABEL> (Form) element handler
 */
int
html_label(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "label");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</label>");
	}

	return(1);				/* get linked */
    }

    return(0);
}


/*
 * HTML <FIELDSET> (Form) element handler
 */
int
html_fieldset(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "fieldset");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</fieldset>");
	}

	return(1);				/* get linked */
    }

    return(0);
}


/*
 * HTML <HEAD> element handler
 */
int
html_head(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	HD(hd->html_data)->head = 1;
    }
    else if(cmd == GF_EOD){
	HD(hd->html_data)->head = 0;
    }

    return(1);				/* get linked */
}


/*
 * HTML <BASE> element handler
 */
int
html_base(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_RESET){
	if(HD(hd->html_data)->head && !HTML_BASE(hd->html_data)){
	    PARAMETER *p;

	    for(p = HD(hd->html_data)->el_data->attribs;
		p && p->attribute && strucmp(p->attribute, "HREF");
		p = p->next)
	      ;

	    if(p && p->value && !((HTML_OPT_S *)(hd->html_data)->opt)->base)
	      ((HTML_OPT_S *)(hd->html_data)->opt)->base = cpystr(p->value);
	}
    }

    return(0);				/* DON'T get linked */
}


/*
 * HTML <TITLE> element handler
 */
int
html_title(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	if(hd->x + 1 >= hd->y){
	    hd->y += 80;
	    fs_resize((void **)&hd->s, (size_t)hd->y * sizeof(unsigned char));
	}

	hd->s[hd->x++] = (unsigned char) ch;
    }
    else if(cmd == GF_RESET){
	hd->x = 0L;
	hd->y = 80L;
	hd->s = (unsigned char *)fs_get((size_t)hd->y * sizeof(unsigned char));
    }
    else if(cmd == GF_EOD){
	/* Down the road we probably want to give these bytes to
	 * someone...
	 */
	hd->s[hd->x] = '\0';
	fs_give((void **)&hd->s);
    }

    return(1);				/* get linked */
}


/*
 * HTML <BODY> element handler
 */
int
html_body(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    PARAMETER  *p, *tp;
	    char      **style = NULL, *text = NULL, *bgcolor = NULL, *pcs;

	    /* modify any attributes in a useful way? */
	    for(p = HD(hd->html_data)->el_data->attribs;
		p && p->attribute;
		p = p->next)
	      if(p->value){
		  if(!strucmp(p->attribute, "style"))
		    style = &p->value;
		  else if(!strucmp(p->attribute, "text"))
		    text = p->value;
		  /*
		   * bgcolor NOT passed since user setting takes precedence
		   *
		  else if(!strucmp(p->attribute, "bgcolor"))
		    bgcolor = p->value;
		  */
	      }

	    /* colors pretty much it */
	    if(text || bgcolor){
		if(!style){
		    tp = (PARAMETER *)fs_get(sizeof(PARAMETER));
		    memset(tp, 0, sizeof(PARAMETER));
		    tp->next = HD(hd->html_data)->el_data->attribs;
		    HD(hd->html_data)->el_data->attribs = tp;
		    tp->attribute = cpystr("style");

		    tmp_20k_buf[0] = '\0';
		    style = &tp->value;
		    pcs = "%s%s%s%s%s";
		}
		else{
		    snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%s", *style);
		    fs_give((void **) style);
		    pcs = "; %s%s%s%s%s";
		}

		snprintf(tmp_20k_buf + strlen(tmp_20k_buf),
			 SIZEOF_20KBUF - strlen(tmp_20k_buf),
			 pcs,
			 (text) ? "color: " : "", (text) ? text : "",
			 (text && bgcolor) ? ";" : "", 
			 (bgcolor) ? "background-color: " : "", (bgcolor) ? bgcolor : "");
		*style = cpystr(tmp_20k_buf);
	    }

	    html_output_raw_tag(hd->html_data, "div");
	}

	HD(hd->html_data)->body = 1;
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</div>");
	}

	HD(hd->html_data)->body = 0;
    }

    return(1);				/* get linked */
}


/*
 * HTML <A> (Anchor) element handler
 */
int
html_a(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);

	if(hd->dp)		/* remember text within anchor tags */
	  so_writec(ch, (STORE_S *) hd->dp);
    }
    else if(cmd == GF_RESET){
	int	   i, n, x;
	char	   buf[256];
	HANDLE_S  *h;
	PARAMETER *p, *href = NULL, *name = NULL;

	/*
	 * Pending Anchor!?!?
	 * space insertion/line breaking that's yet to get done...
	 */
	if(HD(hd->html_data)->prefix){
	    dprint((2, "-- html error: nested or unterminated anchor\n"));
	    html_a_finish(hd);
	}

	/*
	 * Look for valid Anchor data vis the filter installer's parms
	 * (e.g., Only allow references to our internal URLs if asked)
	 */
	for(p = HD(hd->html_data)->el_data->attribs;
	    p && p->attribute;
	    p = p->next)
	  if(!strucmp(p->attribute, "HREF")
	     && p->value
	     && (HANDLES_LOC(hd->html_data)
		 || struncmp(p->value, "x-alpine-", 9)
		 || p->value[0] == '#'))
	    href = p;
	  else if(!strucmp(p->attribute, "NAME"))
	    name = p;

	if(DO_HANDLES(hd->html_data) && (href || name)){
	    h = new_handle(HANDLESP(hd->html_data));

	    /*
	     * Enhancement: we might want to get fancier and parse the
	     * href a bit further such that we can launch images using
	     * our image viewer, or browse local files or directories
	     * with our internal tools.  Of course, having the jump-off
	     * point into text/html always be the defined "web-browser",
	     * just might be the least confusing UI-wise...
	     */
	    h->type = URL;

	    if(name && name->value)
	      h->h.url.name = cpystr(name->value);

	    /*
	     * Prepare to build embedded prefix...
	     */
	    HD(hd->html_data)->prefix = (int *) fs_get(64 * sizeof(int));
	    x = 0;

	    /*
	     * Is this something that looks like a URL?  If not and
	     * we were giving some "base" string, proceed ala RFC1808...
	     */
	    if(href){
		if(HTML_BASE(hd->html_data) && !rfc1738_scan(href->value, &n)){
		    html_a_relative(HTML_BASE(hd->html_data), href->value, h);
		}
		else if(!(NO_RELATIVE(hd->html_data) && html_href_relative(href->value)))
		  h->h.url.path = cpystr(href->value);

		if(pico_usingcolor()){
		    char *fg = NULL, *bg = NULL, *q;

		    if(ps_global->VAR_SLCTBL_FORE_COLOR
		       && colorcmp(ps_global->VAR_SLCTBL_FORE_COLOR,
				   ps_global->VAR_NORM_FORE_COLOR))
		      fg = ps_global->VAR_SLCTBL_FORE_COLOR;

		    if(ps_global->VAR_SLCTBL_BACK_COLOR
		       && colorcmp(ps_global->VAR_SLCTBL_BACK_COLOR,
				   ps_global->VAR_NORM_BACK_COLOR))
		      bg = ps_global->VAR_SLCTBL_BACK_COLOR;

		    if(fg || bg){
			COLOR_PAIR *tmp;

			/*
			 * The blacks are just known good colors for testing
			 * whether the other color is good.
			 */
			tmp = new_color_pair(fg ? fg : colorx(COL_BLACK),
					     bg ? bg : colorx(COL_BLACK));
			if(pico_is_good_colorpair(tmp)){
			    q = color_embed(fg, bg);

			    for(i = 0; q[i]; i++)
			      HD(hd->html_data)->prefix[x++] = q[i];
			}

			if(tmp)
			  free_color_pair(&tmp);
		    }

		    if(F_OFF(F_SLCTBL_ITEM_NOBOLD, ps_global))
		      HD(hd->html_data)->prefix[x++] = HTML_DOBOLD;
		}
		else
		  HD(hd->html_data)->prefix[x++] = HTML_DOBOLD;
	    }

	    HD(hd->html_data)->prefix[x++] = TAG_EMBED;
	    HD(hd->html_data)->prefix[x++] = TAG_HANDLE;

	    snprintf(buf, sizeof(buf), "%ld", hd->x = h->key);
	    HD(hd->html_data)->prefix[x++] = n = strlen(buf);
	    for(i = 0; i < n; i++)
	      HD(hd->html_data)->prefix[x++] = buf[i];

	    HD(hd->html_data)->prefix_used = x;

	    hd->dp = (void *) so_get(CharStar, NULL, EDIT_ACCESS);
	}
    }
    else if(cmd == GF_EOD){
	html_a_finish(hd);
    }

    return(1);				/* get linked */
}


void
html_a_prefix(FILTER_S *f)
{
    int *prefix, n;

    /* Do this so we don't visit from html_output... */
    prefix = HD(f)->prefix;
    HD(f)->prefix = NULL;

    for(n = 0; n < HD(f)->prefix_used; n++)
      html_a_output_prefix(f, prefix[n]);

    fs_give((void **) &prefix);
}


/*
 * html_a_finish - house keeping associated with end of link tag
 */
void
html_a_finish(HANDLER_S *hd)
{
    if(DO_HANDLES(hd->html_data)){
	if(HD(hd->html_data)->prefix){
	    if(!PASS_HTML(hd->html_data)){
		char *empty_link = "[LINK]";
		int   i;

		html_a_prefix(hd->html_data);
		for(i = 0; empty_link[i]; i++)
		  html_output(hd->html_data, empty_link[i]);
	    }
	}

	if(pico_usingcolor()){
	    char *fg = NULL, *bg = NULL, *p;
	    int   i;

	    if(ps_global->VAR_SLCTBL_FORE_COLOR
	       && colorcmp(ps_global->VAR_SLCTBL_FORE_COLOR,
			   ps_global->VAR_NORM_FORE_COLOR))
	      fg = ps_global->VAR_NORM_FORE_COLOR;

	    if(ps_global->VAR_SLCTBL_BACK_COLOR
	       && colorcmp(ps_global->VAR_SLCTBL_BACK_COLOR,
			   ps_global->VAR_NORM_BACK_COLOR))
	      bg = ps_global->VAR_NORM_BACK_COLOR;

	    if(F_OFF(F_SLCTBL_ITEM_NOBOLD, ps_global))
	      HTML_BOLD(hd->html_data, 0);	/* turn OFF bold */

	    if(fg || bg){
		COLOR_PAIR *tmp;

		/*
		 * The blacks are just known good colors for testing
		 * whether the other color is good.
		 */
		tmp = new_color_pair(fg ? fg : colorx(COL_BLACK),
				     bg ? bg : colorx(COL_BLACK));
		if(pico_is_good_colorpair(tmp)){
		    p = color_embed(fg, bg);

		    for(i = 0; p[i]; i++)
		      html_output(hd->html_data, p[i]);
		}

		if(tmp)
		  free_color_pair(&tmp);
	    }
	}
	else
	  HTML_BOLD(hd->html_data, 0);	/* turn OFF bold */

	html_output(hd->html_data, TAG_EMBED);
	html_output(hd->html_data, TAG_HANDLEOFF);

	html_a_output_info(hd);
    }
}


/*
 * html_output_a_prefix - dump Anchor prefix data
 */
void
html_a_output_prefix(FILTER_S *f, int c)
{
    switch(c){
      case HTML_DOBOLD :
	HTML_BOLD(f, 1);
	break;

      default :
	html_output(f, c);
	break;
    }
}



/*
 * html_a_output_info - dump possibly deceptive link info into text.
 *                      phark the phishers.
 */
void
html_a_output_info(HANDLER_S *hd)
{
    int	      l, risky = 0, hl = 0, tl;
    char     *url = NULL, *hn = NULL, *txt;
    HANDLE_S *h;

    /* find host anchor references */
    if((h = get_handle(*HANDLESP(hd->html_data), (int) hd->x)) != NULL
       && h->h.url.path != NULL
       && (hn = rfc1738_scan(rfc1738_str(url = cpystr(h->h.url.path)), &l)) != NULL
       && (hn = srchstr(hn,"://")) != NULL){

	for(hn += 3, hl = 0; hn[hl] && hn[hl] != '/' && hn[hl] != '?'; hl++)
	  ;
    }

    if(hn && hl){
	/*
	 * look over anchor's text to see if there's a
	 * mismatch between href target and url-ish
	 * looking text.  throw a red flag if so.
	 * similarly, toss one if the target's referenced
	 * by a 
	 */
	if(hd->dp){
	    so_writec('\0', (STORE_S *) hd->dp);

	    if((txt = (char *) so_text((STORE_S *) hd->dp)) != NULL
	       && (txt = rfc1738_scan(txt, &tl)) != NULL
	       && (txt = srchstr(txt,"://")) != NULL){

		for(txt += 3, tl = 0; txt[tl] && txt[tl] != '/' && txt[tl] != '?'; tl++)
		  ;

		if(tl != hl)
		  risky++;
		else
		  /* look for non matching text */
		  for(l = 0; l < tl && l < hl; l++)
		    if(tolower((unsigned char) txt[l]) != tolower((unsigned char) hn[l])){
			risky++;
			break;
		    }
	    }

	    so_give((STORE_S **) &hd->dp);
	}

	/* look for literal IP, anything possibly encoded or auth specifier */
	if(!risky){
	    int digits = 1;

	    for(l = 0; l < hl; l++){
		if(hn[l] == '@' || hn[l] == '%'){
		    risky++;
		    break;
		}
		else if(!(hn[l] == '.' || isdigit((unsigned char) hn[l])))
		  digits = 0;
	    }

	    if(digits)
	      risky++;
	}

	/* Insert text of link's domain */
	if(SHOWSERVER(hd->html_data)){
	    char *q;
	    COLOR_PAIR *col = NULL, *colnorm = NULL;

	    html_output(hd->html_data, ' ');
	    html_output(hd->html_data, '[');

	    if(pico_usingcolor()
	       && ps_global->VAR_METAMSG_FORE_COLOR
	       && ps_global->VAR_METAMSG_BACK_COLOR
	       && (col = new_color_pair(ps_global->VAR_METAMSG_FORE_COLOR,
					ps_global->VAR_METAMSG_BACK_COLOR))){
		if(!pico_is_good_colorpair(col))
		  free_color_pair(&col);

		if(col){
		    q = color_embed(col->fg, col->bg);

		    for(l = 0; q[l]; l++)
		      html_output(hd->html_data, q[l]);
		}
	    }

	    for(l = 0; l < hl; l++)
	      html_output(hd->html_data, hn[l]);

	    if(col){
		if(ps_global->VAR_NORM_FORE_COLOR
		   && ps_global->VAR_NORM_BACK_COLOR
		   && (colnorm = new_color_pair(ps_global->VAR_NORM_FORE_COLOR,
						ps_global->VAR_NORM_BACK_COLOR))){
		    if(!pico_is_good_colorpair(colnorm))
		      free_color_pair(&colnorm);

		    if(colnorm){
			q = color_embed(colnorm->fg, colnorm->bg);
			free_color_pair(&colnorm);

			for(l = 0; q[l]; l++)
			  html_output(hd->html_data, q[l]);
		    }
		}

		free_color_pair(&col);
	    }

	    html_output(hd->html_data, ']');
	}
    }

    /*
     * if things look OK so far, make sure nothing within
     * the url looks too fishy...
     */
    while(!risky && hn
	  && (hn = rfc1738_scan(hn, &l)) != NULL
	  && (hn = srchstr(hn,"://")) != NULL){
	int digits = 1;

	for(hn += 3, hl = 0; hn[hl] && hn[hl] != '/' && hn[hl] != '?'; hl++){
	    /*
	     * auth spec, encoded characters, or possibly non-standard port
	     * should raise a red flag
	     */
	    if(hn[hl] == '@' || hn[hl] == '%' || hn[hl] == ':'){
		risky++;	
		break;
	    }
	    else if(!(hn[hl] == '.' || isdigit((unsigned char) hn[hl])))
	      digits = 0;
	}

	/* dotted-dec/raw-int address should cause suspicion as well */
	if(digits)
	  risky++;
    }

    if(risky && ((HTML_OPT_S *) hd->html_data->opt)->warnrisk_f)
      (*((HTML_OPT_S *) hd->html_data->opt)->warnrisk_f)();

    fs_give((void **) &url);
}



/*
 * relative_url - put full url path in h based on base and relative url
 */
void
html_a_relative(char *base_url, char *rel_url, HANDLE_S *h)
{
    size_t  len;
    char    tmp[MAILTMPLEN], *p, *q;
    char   *scheme = NULL, *net = NULL, *path = NULL,
	   *parms = NULL, *query = NULL, *frag = NULL,
	   *base_scheme = NULL, *base_net_loc = NULL,
	   *base_path = NULL, *base_parms = NULL,
	   *base_query = NULL, *base_frag = NULL,
	   *rel_scheme = NULL, *rel_net_loc = NULL,
	   *rel_path = NULL, *rel_parms = NULL,
	   *rel_query = NULL, *rel_frag = NULL;

    /* Rough parse of base URL */
    rfc1808_tokens(base_url, &base_scheme, &base_net_loc, &base_path,
		   &base_parms, &base_query, &base_frag);

    /* Rough parse of this URL */
    rfc1808_tokens(rel_url, &rel_scheme, &rel_net_loc, &rel_path,
		   &rel_parms, &rel_query, &rel_frag);

    scheme = rel_scheme;	/* defaults */
    net    = rel_net_loc;
    path   = rel_path;
    parms  = rel_parms;
    query  = rel_query;
    frag   = rel_frag;
    if(!scheme && base_scheme){
	scheme = base_scheme;
	if(!net){
	    net = base_net_loc;
	    if(path){
		if(*path != '/'){
		    if(base_path){
			for(p = q = base_path;	/* Drop base path's tail */
			    (p = strchr(p, '/'));
			    q = ++p)
			  ;

			len = q - base_path;
		    }
		    else
		      len = 0;

		    if(len + strlen(rel_path) < sizeof(tmp)-1){
			if(len)
			  snprintf(path = tmp, sizeof(tmp), "%.*s", len, base_path);

			strncpy(tmp + len, rel_path, sizeof(tmp)-len);
			tmp[sizeof(tmp)-1] = '\0';

			/* Follow RFC 1808 "Step 6" */
			for(p = tmp; (p = strchr(p, '.')); )
			  switch(*(p+1)){
			      /*
			       * a) All occurrences of "./", where "." is a
			       *    complete path segment, are removed.
			       */
			    case '/' :
			      if(p > tmp)
				for(q = p; (*q = *(q+2)) != '\0'; q++)
				  ;
			      else
				p++;

			      break;

			      /*
			       * b) If the path ends with "." as a
			       *    complete path segment, that "." is
			       *    removed.
			       */
			    case '\0' :
			      if(p == tmp || *(p-1) == '/')
				*p = '\0';
			      else
				p++;

			      break;

			      /*
			       * c) All occurrences of "<segment>/../",
			       *    where <segment> is a complete path
			       *    segment not equal to "..", are removed.
			       *    Removal of these path segments is
			       *    performed iteratively, removing the
			       *    leftmost matching pattern on each
			       *    iteration, until no matching pattern
			       *    remains.
			       *
			       * d) If the path ends with "<segment>/..",
			       *    where <segment> is a complete path
			       *    segment not equal to "..", that
			       *    "<segment>/.." is removed.
			       */
			    case '.' :
			      if(p > tmp + 1){
				  for(q = p - 2; q > tmp && *q != '/'; q--)
				    ;

				  if(*q == '/')
				    q++;

				  if(q + 1 == p		/* no "//.." */
				     || (*q == '.'	/* and "../.." */
					 && *(q+1) == '.'
					 && *(q+2) == '/')){
				      p += 2;
				      break;
				  }

				  switch(*(p+2)){
				    case '/' :
				      len = (p - q) + 3;
				      p = q;
				      for(; (*q = *(q+len)) != '\0'; q++)
					;

				      break;

				    case '\0':
				      *(p = q) = '\0';
				      break;

				    default:
				      p += 2;
				      break;
				  }
			      }
			      else
				p += 2;

			      break;

			    default :
			      p++;
			      break;
			  }
		    }
		    else
		      path = "";		/* lame. */
		}
	    }
	    else{
		path = base_path;
		if(!parms){
		    parms = base_parms;
		    if(!query)
		      query = base_query;
		}
	    }
	}
    }

    len = (scheme ? strlen(scheme) : 0) + (net ? strlen(net) : 0)
	  + (path ? strlen(path) : 0) + (parms ? strlen(parms) : 0)
	  + (query ? strlen(query) : 0) + (frag  ? strlen(frag ) : 0) + 8;

    h->h.url.path = (char *) fs_get(len * sizeof(char));
    snprintf(h->h.url.path, len, "%s%s%s%s%s%s%s%s%s%s%s%s",
	    scheme ? scheme : "", scheme ? ":" : "",
	    net ? "//" : "", net ? net : "",
	    (path && *path == '/') ? "" : ((path && net) ? "/" : ""),
	    path ? path : "",
	    parms ? ";" : "", parms ? parms : "",
	    query ? "?" : "", query ? query : "",
	    frag ? "#" : "", frag ? frag : "");

    if(base_scheme)
      fs_give((void **) &base_scheme);

    if(base_net_loc)
      fs_give((void **) &base_net_loc);

    if(base_path)
      fs_give((void **) &base_path);

    if(base_parms)
      fs_give((void **) &base_parms);

    if(base_query)
      fs_give((void **) &base_query);

    if(base_frag)
      fs_give((void **) &base_frag);

    if(rel_scheme)
      fs_give((void **) &rel_scheme);

    if(rel_net_loc)
      fs_give((void **) &rel_net_loc);

    if(rel_parms)
      fs_give((void **) &rel_parms);

    if(rel_query)
      fs_give((void **) &rel_query);

    if(rel_frag)
      fs_give((void **) &rel_frag);

    if(rel_path)
      fs_give((void **) &rel_path);
}


/*
 * html_href_relative - href 
 */
int
html_href_relative(char *url)
{
    int i;

    if(url)
      for(i = 0; i < 32 && url[i]; i++)
	if(!(isalpha((unsigned char) url[i]) || url[i] == '_' || url[i] == '-')){
	  if(url[i] == ':')
	    return(FALSE);
	  else
	    break;
	}

    return(TRUE);
}


/*
 * HTML <UL> (Unordered List) element handler
 */
int
html_ul(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "ul");
	}
	else{
	    HD(hd->html_data)->li_pending = 1;
	    html_blank(hd->html_data, 0);
	}
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</ul>");
	}
	else{
	    html_blank(hd->html_data, 0);

	    if(!HD(hd->html_data)->li_pending)
	      html_indent(hd->html_data, -4, HTML_ID_INC);
	    else
	      HD(hd->html_data)->li_pending = 0;
	}
    }

    return(1);				/* get linked */
}


/*
 * HTML <OL> (Ordered List) element handler
 */
int
html_ol(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "ol");
	}
	else{
	    /*
	     * Signal that we're expecting to see <LI> as our next elemnt
	     * and set the the initial ordered count.
	     */
	    HD(hd->html_data)->li_pending = 1;
	    hd->x = 1L;
	    html_blank(hd->html_data, 0);
	}
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</ol>");
	}
	else{
	    html_blank(hd->html_data, 0);

	    if(!HD(hd->html_data)->li_pending)
	      html_indent(hd->html_data, -4, HTML_ID_INC);
	    else
	      HD(hd->html_data)->li_pending = 0;
	}
    }

    return(1);				/* get linked */
}


/*
 * HTML <MENU> (Menu List) element handler
 */
int
html_menu(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "menu");
	}
	else{
	    HD(hd->html_data)->li_pending = 1;
	}
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</menu>");
	}
	else{
	    html_blank(hd->html_data, 0);

	    if(!HD(hd->html_data)->li_pending)
	      html_indent(hd->html_data, -4, HTML_ID_INC);
	    else
	      HD(hd->html_data)->li_pending = 0;
	}
    }

    return(1);				/* get linked */
}


/*
 * HTML <DIR> (Directory List) element handler
 */
int
html_dir(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "dir");
	}
	else{
	    HD(hd->html_data)->li_pending = 1;
	}
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</dir>");
	}
	else{
	    html_blank(hd->html_data, 0);

	    if(!HD(hd->html_data)->li_pending)
	      html_indent(hd->html_data, -4, HTML_ID_INC);
	    else
	      HD(hd->html_data)->li_pending = 0;
	}
    }

    return(1);				/* get linked */
}


/*
 * HTML <LI> (List Item) element handler
 */
int
html_li(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	if(PASS_HTML(hd->html_data)){
	    html_handoff(hd, ch);
	}
    }
    else if(cmd == GF_RESET){
	HANDLER_S *p, *found = NULL;

	/*
	 * There better be a an unordered list, ordered list,
	 * Menu or Directory handler installed
	 * or else we crap out...
	 */
	for(p = HANDLERS(hd->html_data); p; p = p->below)
	  if(EL(p)->handler == html_ul
	     || EL(p)->handler == html_ol
	     || EL(p)->handler == html_menu
	     || EL(p)->handler == html_dir){
	      found = p;
	      break;
	  }

	if(found){
	    if(PASS_HTML(hd->html_data)){
	    }
	    else{
		char buf[8], *p;
		int  wrapstate;

		/* Start a new line */
		html_blank(hd->html_data, 0);

		/* adjust indent level if needed */
		if(HD(hd->html_data)->li_pending){
		    html_indent(hd->html_data, 4, HTML_ID_INC);
		    HD(hd->html_data)->li_pending = 0;
		}

		if(EL(found)->handler == html_ul){
		    int l = html_indent(hd->html_data, 0, HTML_ID_GET);

		    strncpy(buf, "   ", sizeof(buf));
		    buf[1] = (l < 5) ? '*' : (l < 9) ? '+' : (l < 17) ? 'o' : '#';
		}
		else if(EL(found)->handler == html_ol)
		  snprintf(buf, sizeof(buf), "%2ld.", found->x++);
		else if(EL(found)->handler == html_menu){
		    strncpy(buf, " ->", sizeof(buf));
		    buf[sizeof(buf)-1] = '\0';
		}

		html_indent(hd->html_data, -4, HTML_ID_INC);

		/* So we don't munge whitespace */
		wrapstate = HD(hd->html_data)->wrapstate;
		HD(hd->html_data)->wrapstate = 0;

		html_write_indent(hd->html_data, HD(hd->html_data)->indent_level);
		for(p = buf; *p; p++)
		  html_output(hd->html_data, (int) *p);

		HD(hd->html_data)->wrapstate = wrapstate;
		html_indent(hd->html_data, 4, HTML_ID_INC);
	    }
	    /* else BUG: should really bitch about this */
	}

	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "li");
	    return(1);				/* get linked */
	}
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</li>");
	}
    }

    return(PASS_HTML(hd->html_data));	/* DON'T get linked */
}


/*
 * HTML <DL> (Definition List) element handler
 */
int
html_dl(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "dl");
	}
	else{
	    /*
	     * Set indention level for definition terms and definitions...
	     */
	    hd->x = html_indent(hd->html_data, 0, HTML_ID_GET);
	    hd->y = hd->x + 2;
	    hd->z = hd->y + 4;
	}
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</dl>");
	}
	else{
	    html_indent(hd->html_data, (int) hd->x, HTML_ID_SET);
	    html_blank(hd->html_data, 1);
	}
    }

    return(1);				/* get linked */
}


/*
 * HTML <DT> (Definition Term) element handler
 */
int
html_dt(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "dt");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</dt>");
	}

	return(1);				/* get linked */
    }

    if(cmd == GF_RESET){
	HANDLER_S *p;

	/*
	 * There better be a Definition Handler installed
	 * or else we crap out...
	 */
	for(p = HANDLERS(hd->html_data); p && EL(p)->handler != html_dl; p = p->below)
	  ;

	if(p){				/* adjust indent level if needed */
	    html_indent(hd->html_data, (int) p->y, HTML_ID_SET);
	    html_blank(hd->html_data, 1);
	}
	/* BUG: else should really bitch about this */
    }

    return(0);				/* DON'T get linked */
}


/*
 * HTML <DD> (Definition Definition) element handler
 */
int
html_dd(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "dd");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</dd>");
	}

	return(1);				/* get linked */
    }

    if(cmd == GF_RESET){
	HANDLER_S *p;

	/*
	 * There better be a Definition Handler installed
	 * or else we crap out...
	 */
	for(p = HANDLERS(hd->html_data); p && EL(p)->handler != html_dl; p = p->below)
	  ;

	if(p){				/* adjust indent level if needed */
	    html_indent(hd->html_data, (int) p->z, HTML_ID_SET);
	    html_blank(hd->html_data, 0);
	}
	/* BUG: should really bitch about this */
    }

    return(0);				/* DON'T get linked */
}


/*
 * HTML <H1> (Headings 1) element handler.
 *
 * Bold, very-large font, CENTERED. One or two blank lines
 * above and below.  For our silly character cell's that
 * means centered and ALL CAPS...
 */
int
html_h1(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "h1");
	}
	else{
	    /* turn ON the centered bit */
	    CENTER_BIT(hd->html_data) = 1;
	}
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</h1>");
	}
	else{
	    /* turn OFF the centered bit, add blank line */
	    CENTER_BIT(hd->html_data) = 0;
	    html_blank(hd->html_data, 1);
	}
    }

    return(1);				/* get linked */
}


/*
 * HTML <H2> (Headings 2) element handler
 */
int
html_h2(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	if(PASS_HTML(hd->html_data)){
	    html_handoff(hd, ch);
	}
	else{
	    if((hd->x & HTML_HX_ULINE) && !ASCII_ISSPACE((unsigned char) (ch & 0xff))){
		HTML_ULINE(hd->html_data, 1);
		hd->x ^= HTML_HX_ULINE;	/* only once! */
	    }

	    html_handoff(hd, (ch < 128 && islower((unsigned char) ch))
			 ? toupper((unsigned char) ch) : ch);
	}
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "h2");
	}
	else{
	    /*
	     * Bold, large font, flush-left. One or two blank lines
	     * above and below.
	     */
	    if(CENTER_BIT(hd->html_data)) /* stop centering for now */
	      hd->x = HTML_HX_CENTER;
	    else
	      hd->x = 0;

	    hd->x |= HTML_HX_ULINE;
	    
	    CENTER_BIT(hd->html_data) = 0;
	    hd->y = html_indent(hd->html_data, 0, HTML_ID_SET);
	    hd->z = HD(hd->html_data)->wrapcol;
	    HD(hd->html_data)->wrapcol = WRAP_COLS(hd->html_data) - 8;
	    html_blank(hd->html_data, 1);
	}
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</h2>");
	}
	else{
	    /*
	     * restore previous centering, and indent level
	     */
	    if(!(hd->x & HTML_HX_ULINE))
	      HTML_ULINE(hd->html_data, 0);

	    html_indent(hd->html_data, hd->y, HTML_ID_SET);
	    html_blank(hd->html_data, 1);
	    CENTER_BIT(hd->html_data)  = (hd->x & HTML_HX_CENTER) != 0;
	    HD(hd->html_data)->wrapcol = hd->z;
	}
    }

    return(1);				/* get linked */
}


/*
 * HTML <H3> (Headings 3) element handler
 */
int
html_h3(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	if(!PASS_HTML(hd->html_data)){
	    if((hd->x & HTML_HX_ULINE) && !ASCII_ISSPACE((unsigned char) (ch & 0xff))){
		HTML_ULINE(hd->html_data, 1);
		hd->x ^= HTML_HX_ULINE;	/* only once! */
	    }
	}

	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "h3");
	}
	else{
	    /*
	     * Italic, large font, slightly indented from the left
	     * margin. One or two blank lines above and below.
	     */
	    if(CENTER_BIT(hd->html_data)) /* stop centering for now */
	      hd->x = HTML_HX_CENTER;
	    else
	      hd->x = 0;

	    hd->x |= HTML_HX_ULINE;
	    CENTER_BIT(hd->html_data) = 0;
	    hd->y = html_indent(hd->html_data, 2, HTML_ID_SET);
	    hd->z = HD(hd->html_data)->wrapcol;
	    HD(hd->html_data)->wrapcol = WRAP_COLS(hd->html_data) - 8;
	    html_blank(hd->html_data, 1);
	}
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</h3>");
	}
	else{
	    /*
	     * restore previous centering, and indent level
	     */
	    if(!(hd->x & HTML_HX_ULINE))
	      HTML_ULINE(hd->html_data, 0);

	    html_indent(hd->html_data, hd->y, HTML_ID_SET);
	    html_blank(hd->html_data, 1);
	    CENTER_BIT(hd->html_data)  = (hd->x & HTML_HX_CENTER) != 0;
	    HD(hd->html_data)->wrapcol = hd->z;
	}
    }

    return(1);				/* get linked */
}


/*
 * HTML <H4> (Headings 4) element handler
 */
int
html_h4(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "h4");
	}
	else{
	    /*
	     * Bold, normal font, indented more than H3. One blank line
	     * above and below.
	     */
	    hd->x = CENTER_BIT(hd->html_data); /* stop centering for now */
	    CENTER_BIT(hd->html_data) = 0;
	    hd->y = html_indent(hd->html_data, 4, HTML_ID_SET);
	    hd->z = HD(hd->html_data)->wrapcol;
	    HD(hd->html_data)->wrapcol = WRAP_COLS(hd->html_data) - 8;
	    html_blank(hd->html_data, 1);
	}
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</h4>");
	}
	else{
	    /*
	     * restore previous centering, and indent level
	     */
	    html_indent(hd->html_data, (int) hd->y, HTML_ID_SET);
	    html_blank(hd->html_data, 1);
	    CENTER_BIT(hd->html_data)  = hd->x;
	    HD(hd->html_data)->wrapcol = hd->z;
	}
    }

    return(1);				/* get linked */
}


/*
 * HTML <H5> (Headings 5) element handler
 */
int
html_h5(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "h5");
	}
	else{
	    /*
	     * Italic, normal font, indented as H4. One blank line
	     * above.
	     */
	    hd->x = CENTER_BIT(hd->html_data); /* stop centering for now */
	    CENTER_BIT(hd->html_data) = 0;
	    hd->y = html_indent(hd->html_data, 6, HTML_ID_SET);
	    hd->z = HD(hd->html_data)->wrapcol;
	    HD(hd->html_data)->wrapcol = WRAP_COLS(hd->html_data) - 8;
	    html_blank(hd->html_data, 1);
	}
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</h5>");
	}
	else{
	    /*
	     * restore previous centering, and indent level
	     */
	    html_indent(hd->html_data, (int) hd->y, HTML_ID_SET);
	    html_blank(hd->html_data, 1);
	    CENTER_BIT(hd->html_data)  = hd->x;
	    HD(hd->html_data)->wrapcol = hd->z;
	}
    }

    return(1);				/* get linked */
}


/*
 * HTML <H6> (Headings 6) element handler
 */
int
html_h6(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "h6");
	}
	else{
	    /*
	     * Bold, indented same as normal text, more than H5. One
	     * blank line above.
	     */
	    hd->x = CENTER_BIT(hd->html_data); /* stop centering for now */
	    CENTER_BIT(hd->html_data) = 0;
	    hd->y = html_indent(hd->html_data, 8, HTML_ID_SET);
	    hd->z = HD(hd->html_data)->wrapcol;
	    HD(hd->html_data)->wrapcol = WRAP_COLS(hd->html_data) - 8;
	    html_blank(hd->html_data, 1);
	}
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</h6>");
	}
	else{
	    /*
	     * restore previous centering, and indent level
	     */
	    html_indent(hd->html_data, (int) hd->y, HTML_ID_SET);
	    html_blank(hd->html_data, 1);
	    CENTER_BIT(hd->html_data)  = hd->x;
	    HD(hd->html_data)->wrapcol = hd->z;
	}
    }

    return(1);				/* get linked */
}


/*
 * HTML <BlockQuote> element handler
 */
int
html_blockquote(HANDLER_S *hd, int ch, int cmd)
{
    int	 j;
#define	HTML_BQ_INDENT	6

    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "blockquote");
	}
	else{
	    /*
	     * A typical rendering might be a slight extra left and
	     * right indent, and/or italic font. The Blockquote element
	     * causes a paragraph break, and typically provides space
	     * above and below the quote.
	     */
	    html_indent(hd->html_data, HTML_BQ_INDENT, HTML_ID_INC);
	    j = HD(hd->html_data)->wrapstate;
	    HD(hd->html_data)->wrapstate = 0;
	    html_blank(hd->html_data, 1);
	    HD(hd->html_data)->wrapstate = j;
	    HD(hd->html_data)->wrapcol  -= HTML_BQ_INDENT;
	}
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</blockquote>");
	}
	else{
	    html_blank(hd->html_data, 1);

	    j = HD(hd->html_data)->wrapstate;
	    HD(hd->html_data)->wrapstate = 0;
	    html_indent(hd->html_data, -(HTML_BQ_INDENT), HTML_ID_INC);
	    HD(hd->html_data)->wrapstate = j;
	    HD(hd->html_data)->wrapcol  += HTML_BQ_INDENT;
	}
    }

    return(1);				/* get linked */
}


/*
 * HTML <Address> element handler
 */
int
html_address(HANDLER_S *hd, int ch, int cmd)
{
    int	 j;
#define	HTML_ADD_INDENT	2

    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "address");
	}
	else{
	    /*
	     * A typical rendering might be a slight extra left and
	     * right indent, and/or italic font. The Blockquote element
	     * causes a paragraph break, and typically provides space
	     * above and below the quote.
	     */
	    html_indent(hd->html_data, HTML_ADD_INDENT, HTML_ID_INC);
	    j = HD(hd->html_data)->wrapstate;
	    HD(hd->html_data)->wrapstate = 0;
	    html_blank(hd->html_data, 1);
	    HD(hd->html_data)->wrapstate = j;
	}
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</address>");
	}
	else{
	    html_blank(hd->html_data, 1);

	    j = HD(hd->html_data)->wrapstate;
	    HD(hd->html_data)->wrapstate = 0;
	    html_indent(hd->html_data, -(HTML_ADD_INDENT), HTML_ID_INC);
	    HD(hd->html_data)->wrapstate = j;
	}
    }

    return(1);				/* get linked */
}


/*
 * HTML <PRE> (Preformatted Text) element handler
 */
int
html_pre(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	/*
	 * remove CRLF after '>' in element.
	 * We see CRLF because wrapstate is off.
	 */
	switch(hd->y){
	  case 2 :
	    if(ch == '\012'){
		hd->y = 3;
		return(1);
	    }
	    else
	      html_handoff(hd, '\015');

	    break;

	  case 1 :
	    if(ch == '\015'){
		hd->y = 2;
		return(1);
	    }

	  case 3 :
	    /* passing tags?  replace CRLF with <BR> to make
	     * sure hard newline survives in the end...
	     */
	    if(PASS_HTML(hd->html_data))
	      hd->y = 4;		/* keep looking for CRLF */
	    else
	      hd->y = 0;		/* stop looking */

	    break;

	  case 4 :
	    if(ch == '\015'){
		hd->y = 5;
		return(1);
	    }

	    break;

	  case 5 :
	    hd->y = 4;
	    if(ch == '\012'){
		html_output_string(hd->html_data, "<br />");
		return(1);
	    }
	    else
	      html_handoff(hd, '\015');	/* not CRLF, pass raw CR */

	    break;

	  default :			/* zero case */
	    break;
	}

	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	hd->y = 1;
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "pre");
	}
	else{
	    if(hd->html_data)
	      hd->html_data->f1 = DFL;				\

	    html_blank(hd->html_data, 1);
	    hd->x = HD(hd->html_data)->wrapstate;
	    HD(hd->html_data)->wrapstate = 0;
	}
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</pre>");
	}
	else{
	    HD(hd->html_data)->wrapstate = (hd->x != 0);
	    html_blank(hd->html_data, 0);
	}
    }

    return(1);
}


/*
 * HTML <CENTER> (Centerd Text) element handler
 */
int
html_center(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "center");
	}
	else{
	    /* turn ON the centered bit */
	    CENTER_BIT(hd->html_data) = 1;
	}
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</center>");
	}
	else{
	    /* turn OFF the centered bit */
	    CENTER_BIT(hd->html_data) = 0;
	}
    }

    return(1);
}


/*
 * HTML <DIV> (Document Divisions) element handler
 */
int
html_div(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	if(PASS_HTML(hd->html_data)){
	    html_output_raw_tag(hd->html_data, "div");
	}
	else{
	    PARAMETER *p;

	    for(p = HD(hd->html_data)->el_data->attribs;
		p && p->attribute;
		p = p->next)
	      if(!strucmp(p->attribute, "ALIGN")){
		  if(p->value){
		      /* remember previous values */
		      hd->x = CENTER_BIT(hd->html_data);
		      hd->y = html_indent(hd->html_data, 0, HTML_ID_GET);

		      html_blank(hd->html_data, 0);
		      CENTER_BIT(hd->html_data) = !strucmp(p->value, "CENTER");
		      html_indent(hd->html_data, 0, HTML_ID_SET);
		      /* NOTE: "RIGHT" not supported yet */
		  }
	      }
	  }
    }
    else if(cmd == GF_EOD){
	if(PASS_HTML(hd->html_data)){
	    html_output_string(hd->html_data, "</div>");
	}
	else{
	    /* restore centered bit and indentiousness */
	    CENTER_BIT(hd->html_data) = hd->y;
	    html_indent(hd->html_data, hd->y, HTML_ID_SET);
	    html_blank(hd->html_data, 0);
	}
    }

    return(1);
}


/*
 * HTML <SPAN> (Text Span) element handler
 */
int
html_span(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "span");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</span>");
	}

	return(1);
    }

    return(0);
}


/*
 * HTML <KBD> (Text Kbd) element handler
 */
int
html_kbd(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "kbd");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</kbd>");
	}

	return(1);
    }

    return(0);
}


/*
 * HTML <DFN> (Text Definition) element handler
 */
int
html_dfn(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "dfn");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</dfn>");
	}

	return(1);
    }

    return(0);
}


/*
 * HTML <TT> (Text Tt) element handler
 */
int
html_tt(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "tt");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</tt>");
	}

	return(1);
    }

    return(0);
}


/*
 * HTML <VAR> (Text Var) element handler
 */
int
html_var(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "var");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</var>");
	}

	return(1);
    }

    return(0);
}


/*
 * HTML <SAMP> (Text Samp) element handler
 */
int
html_samp(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "samp");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</samp>");
	}

	return(1);
    }

    return(0);
}


/*
 * HTML <SUP> (Text Superscript) element handler
 */
int
html_sup(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "sup");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</sup>");
	}

	return(1);
    }

    return(0);
}


/*
 * HTML <SUB> (Text Subscript) element handler
 */
int
html_sub(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "sub");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</sub>");
	}

	return(1);
    }

    return(0);
}


/*
 * HTML <CITE> (Text Citation) element handler
 */
int
html_cite(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "cite");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</cite>");
	}

	return(1);
    }

    return(0);
}


/*
 * HTML <CODE> (Text Code) element handler
 */
int
html_code(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "code");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</code>");
	}

	return(1);
    }

    return(0);
}


/*
 * HTML <INS> (Text Inserted) element handler
 */
int
html_ins(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "ins");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</ins>");
	}

	return(1);
    }

    return(0);
}


/*
 * HTML <DEL> (Text Deleted) element handler
 */
int
html_del(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "del");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</del>");
	}

	return(1);
    }

    return(0);
}


/*
 * HTML <ABBR> (Text Abbreviation) element handler
 */
int
html_abbr(HANDLER_S *hd, int ch, int cmd)
{
    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    html_handoff(hd, ch);
	}
	else if(cmd == GF_RESET){
	    html_output_raw_tag(hd->html_data, "abbr");
	}
	else if(cmd == GF_EOD){
	    html_output_string(hd->html_data, "</abbr>");
	}

	return(1);
    }

    return(0);
}


/*
 * HTML <SCRIPT> element handler
 */
int
html_script(HANDLER_S *hd, int ch, int cmd)
{
    /* Link in and drop everything within on the floor */
    return(1);
}


/*
 * HTML <APPLET> element handler
 */
int
html_applet(HANDLER_S *hd, int ch, int cmd)
{
    /* Link in and drop everything within on the floor */
    return(1);
}


/*
 * HTML <STYLE> CSS element handler
 */
int
html_style(HANDLER_S *hd, int ch, int cmd)
{
    static STORE_S  *css_stuff ;

    if(PASS_HTML(hd->html_data)){
	if(cmd == GF_DATA){
	    /* collect style settings */
	    so_writec(ch, css_stuff);
	}
	else if(cmd == GF_RESET){
	    if(css_stuff)
	      so_give(&css_stuff);

	    css_stuff = so_get(CharStar, NULL, EDIT_ACCESS);
	}
	else if(cmd == GF_EOD){
	    /*
	     * TODO: strip anything mischievous and pass on
	     */

	    so_give(&css_stuff);
	}
    }

    return(1);
}

/*
 *  RSS 2.0 <RSS> version
 */
int
rss_rss(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_RESET){
	PARAMETER *p;

	for(p = HD(hd->html_data)->el_data->attribs;
	    p && p->attribute;
	    p = p->next)
	  if(!strucmp(p->attribute, "VERSION")){
	      if(p->value && !strucmp(p->value,"2.0"))
		return(0);	/* do not link in */
	  }

	gf_error("Incompatible RSS version");
	/* NO RETURN */
    }

    return(0);	/* not linked or error means we never get here */
}

/*
 *  RSS 2.0 <CHANNEL>
 */
int
rss_channel(HANDLER_S *hd, int ch, int cmd)
{
    if(cmd == GF_DATA){
	html_handoff(hd, ch);
    }
    else if(cmd == GF_RESET){
	RSS_FEED_S *feed;

	feed = RSS_FEED(hd->html_data) = fs_get(sizeof(RSS_FEED_S));
	memset(feed, 0, sizeof(RSS_FEED_S));
    }

    return(1);			/* link in */
}

/*
 *  RSS 2.0 <TITLE>
 */
int
rss_title(HANDLER_S *hd, int ch, int cmd)
{
    static STORE_S *title_so;

    if(cmd == GF_DATA){
	/* collect data */
	if(title_so){
	    so_writec(ch, title_so);
	}
    }
    else if(cmd == GF_RESET){
	if(RSS_FEED(hd->html_data)){
	    /* prepare for data */
	    if(title_so)
	      so_give(&title_so);

	    title_so = so_get(CharStar, NULL, EDIT_ACCESS);
	}
    }
    else if(cmd == GF_EOD){
	if(title_so){
	    RSS_FEED_S *feed = RSS_FEED(hd->html_data);
	    RSS_ITEM_S *rip;

	    if(feed){
		if(rip = feed->items){
		    for(; rip->next; rip = rip->next)
		      ;

		    if(rip->title)
		      fs_give((void **) &rip->title);

		    rip->title = cpystr(rss_skip_whitespace(so_text(title_so)));
		}
		else{
		    if(feed->title)
		      fs_give((void **) &feed->title);

		    feed->title = cpystr(rss_skip_whitespace(so_text(title_so)));
		}
	    }

	    so_give(&title_so);
	}
    }

    return(1);			/* link in */
}

/*
 *  RSS 2.0 <IMAGE>
 */
int
rss_image(HANDLER_S *hd, int ch, int cmd)
{
    static STORE_S *img_so;

    if(cmd == GF_DATA){
	/* collect data */
	if(img_so){
	    so_writec(ch, img_so);
	}
    }
    else if(cmd == GF_RESET){
	if(RSS_FEED(hd->html_data)){
	    /* prepare to collect data */
	    if(img_so)
	      so_give(&img_so);

	    img_so = so_get(CharStar, NULL, EDIT_ACCESS);
	}
    }
    else if(cmd == GF_EOD){
	if(img_so){
	    RSS_FEED_S *feed = RSS_FEED(hd->html_data);

	    if(feed){
		if(feed->image)
		  fs_give((void **) &feed->image);

		feed->image = cpystr(rss_skip_whitespace(so_text(img_so)));
	    }

	    so_give(&img_so);
	}
    }

    return(1);			/* link in */
}

/*
 *  RSS 2.0 <LINK>
 */
int
rss_link(HANDLER_S *hd, int ch, int cmd)
{
    static STORE_S *link_so;

    if(cmd == GF_DATA){
	/* collect data */
	if(link_so){
	    so_writec(ch, link_so);
	}
    }
    else if(cmd == GF_RESET){
	if(RSS_FEED(hd->html_data)){
	    /* prepare to collect data */
	    if(link_so)
	      so_give(&link_so);

	    link_so = so_get(CharStar, NULL, EDIT_ACCESS);
	}
    }
    else if(cmd == GF_EOD){
	if(link_so){
	    RSS_FEED_S *feed = RSS_FEED(hd->html_data);
	    RSS_ITEM_S *rip;

	    if(feed){
		if(rip = feed->items){
		    for(; rip->next; rip = rip->next)
		      ;

		    if(rip->link)
		      fs_give((void **) &rip->link);

		    rip->link = cpystr(rss_skip_whitespace(so_text(link_so)));
		}
		else{
		    if(feed->link)
		      fs_give((void **) &feed->link);

		    feed->link = cpystr(rss_skip_whitespace(so_text(link_so)));
		}
	    }

	    so_give(&link_so);
	}
    }

    return(1);			/* link in */
}

/*
 *  RSS 2.0 <DESCRIPTION>
 */
int
rss_description(HANDLER_S *hd, int ch, int cmd)
{
    static STORE_S *desc_so;

    if(cmd == GF_DATA){
	/* collect data */
	if(desc_so){
	    so_writec(ch, desc_so);
	}
    }
    else if(cmd == GF_RESET){
	if(RSS_FEED(hd->html_data)){
	    /* prepare to collect data */
	    if(desc_so)
	      so_give(&desc_so);

	    desc_so = so_get(CharStar, NULL, EDIT_ACCESS);
	}
    }
    else if(cmd == GF_EOD){
	if(desc_so){
	    RSS_FEED_S *feed = RSS_FEED(hd->html_data);
	    RSS_ITEM_S *rip;

	    if(feed){
		if(rip = feed->items){
		    for(; rip->next; rip = rip->next)
		      ;

		    if(rip->description)
		      fs_give((void **) &rip->description);

		    rip->description = cpystr(rss_skip_whitespace(so_text(desc_so)));
		}
		else{
		    if(feed->description)
		      fs_give((void **) &feed->description);

		    feed->description = cpystr(rss_skip_whitespace(so_text(desc_so)));
		}
	    }

	    so_give(&desc_so);
	}
    }

    return(1);			/* link in */
}

/*
 *  RSS 2.0 <TTL> (in minutes)
 */
int
rss_ttl(HANDLER_S *hd, int ch, int cmd)
{
    RSS_FEED_S *feed = RSS_FEED(hd->html_data);

    if(cmd == GF_DATA){
	if(isdigit((unsigned char) ch))
	  feed->ttl = ((feed->ttl * 10) + (ch - '0'));
    }
    else if(cmd == GF_RESET){
	/* prepare to collect data */
	feed->ttl = 0;
    }
    else if(cmd == GF_EOD){
    }

    return(1);			/* link in */
}

/*
 *  RSS 2.0 <ITEM>
 */
int
rss_item(HANDLER_S *hd, int ch, int cmd)
{
    /* BUG: verify no ITEM nesting? */
    if(cmd == GF_RESET){
	RSS_FEED_S *feed;

	if((feed = RSS_FEED(hd->html_data)) != NULL){
	    RSS_ITEM_S **rip;
	    int		 n = 0;

	    for(rip = &feed->items; *rip; rip = &(*rip)->next)
	      if(++n > RSS_ITEM_LIMIT)
		return(0);

	    *rip = fs_get(sizeof(RSS_ITEM_S));
	    memset(*rip, 0, sizeof(RSS_ITEM_S));
	}
    }

    return(0);			/* don't link in */
}


char *
rss_skip_whitespace(char *s)
{
    for(; *s && isspace((unsigned char) *s); s++)
      ;

    return(s);
}


/*
 * return the function associated with the given element name
 */
ELPROP_S *
element_properties(FILTER_S *fd, char *el_name)
{
    register ELPROP_S *el_table = ELEMENTS(fd);

    for(; el_table->element; el_table++)
      if(!strucmp(el_name, el_table->element))
	return(el_table);

    return(NULL);
}


/*
 * collect element's name and any attribute/value pairs then
 * dispatch to the appropriate handler.
 *
 * Returns 1 : got what we wanted
 *	   0 : we need more data
 *	  -1 : bogus input
 */
int
html_element_collector(FILTER_S *fd, int ch)
{
    if(ch == '>'){
	if(ED(fd)->overrun){
	    /*
	     * If problem processing, don't bother doing anything
	     * internally, just return such that none of what we've
	     * digested is displayed.
	     */
	    HTML_DEBUG_EL("too long", ED(fd));
	    return(1);			/* Let it go, Jim */
	}
	else if(ED(fd)->mkup_decl){
	    if(ED(fd)->badform){
		dprint((2, "-- html error: bad form: %.*s\n",
			   ED(fd)->len, ED(fd)->buf ? ED(fd)->buf : "?"));
		/*
		 * Invalid comment -- make some guesses as
		 * to whether we should stop with this greater-than...
		 */
		if(ED(fd)->buf[0] != '-'
		   || ED(fd)->len < 4
		   || (ED(fd)->buf[1] == '-'
		       && ED(fd)->buf[ED(fd)->len - 1] == '-'
		       && ED(fd)->buf[ED(fd)->len - 2] == '-'))
		  return(1);
	    }
	    else{
		dprint((5, "-- html: OK: %.*s\n",
			   ED(fd)->len, ED(fd)->buf ? ED(fd)->buf : "?"));
		if(ED(fd)->start_comment == ED(fd)->end_comment){
		    if(ED(fd)->len > 10){
			ED(fd)->buf[ED(fd)->len - 2] = '\0';
			html_element_comment(fd, ED(fd)->buf + 2);
		    }

		    return(1);
		}
		/* else keep collecting comment below */
	    }
	}
	else if(ED(fd)->proc_inst){
	    return(1);			/* return without display... */
	}
	else if(!ED(fd)->quoted || ED(fd)->badform){
	    ELPROP_S *ep;

	    /*
	     * We either have the whole thing or all that we could
	     * salvage from it.  Try our best...
	     */

	    if(HD(fd)->bitbucket)
	      return(1);		/* element inside chtml clause! */

	    if(!ED(fd)->badform && html_element_flush(ED(fd)))
	      return(1);		/* return without display... */

	    /*
	     * If we ran into an empty tag or we don't know how to deal
	     * with it, just go on, ignoring it...
	     */
	    if(ED(fd)->element && (ep = element_properties(fd, ED(fd)->element))){
		if(ep->handler){
		    /* dispatch the element's handler */
		    HTML_DEBUG_EL(ED(fd)->end_tag ? "POP" : "PUSH", ED(fd));
		    if(ED(fd)->end_tag){
			html_pop(fd, ep);	/* remove it's handler */
		    }
		    else{
			/* if a block element, pop any open <p>'s */
			if(ep->blocklevel){
			    HANDLER_S *tp;

			    for(tp = HANDLERS(fd); tp && EL(tp)->handler == html_p; tp = tp->below){
				HTML_DEBUG_EL("Unclosed <P>", ED(fd));
				html_pop(fd, EL(tp));
				break;
			    }
			}

			/* enforce table nesting */
			if(!strucmp(ep->element, "tr")){
			    if(!HANDLERS(fd) || (strucmp(EL(HANDLERS(fd))->element, "table") && strucmp(EL(HANDLERS(fd))->element, "tbody") && strucmp(EL(HANDLERS(fd))->element, "thead"))){
				dprint((2, "-- html error: bad nesting for <TR>, GOT %s\n", (HANDLERS(fd)) ? EL(HANDLERS(fd))->element : "NO-HANDLERS"));
				if(HANDLERS(fd) && !strucmp(EL(HANDLERS(fd))->element,"tr")){
				    dprint((2, "-- html error: bad nesting popping previous <TR>"));
				    html_pop(fd, EL(HANDLERS(fd)));
				}
				else{
				    dprint((2, "-- html error: bad nesting pusing <TABLE>"));
				    html_push(fd, element_properties(fd, "table"));
				}
			    }
			}
			else if(!strucmp(ep->element, "td") || !strucmp(ep->element, "th")){
			    if(!HANDLERS(fd)){
				dprint((2, "-- html error: bad nesting: NO HANDLERS before <TD>"));
				html_push(fd, element_properties(fd, "table"));
				html_push(fd, element_properties(fd, "tr"));
			    }
			    else if(strucmp(EL(HANDLERS(fd))->element, "tr")){
				dprint((2, "-- html error: bad nesting for <TD>, GOT %s\n", EL(HANDLERS(fd))->element));
				html_push(fd, element_properties(fd, "tr"));
			    }
			    else if(!strucmp(EL(HANDLERS(fd))->element, "td")){
				dprint((2, "-- html error: bad nesting popping <TD>"));
				html_pop(fd, EL(HANDLERS(fd)));
			    }
			}

			/* add it's handler */
			if(html_push(fd, ep)){
			    if(ED(fd)->empty){
				/* remove empty element */
				html_pop(fd, ep);
			    }
			}
		    }
		}
		else {
		    HTML_DEBUG_EL("IGNORED", ED(fd));
		}
	    }
	    else{			/* else, empty or unrecognized */
		HTML_DEBUG_EL("?", ED(fd));
	    }

	    return(1);			/* all done! see, that didn't hurt */
	}
    }
    else if(ch == '/' && ED(fd)->element && ED(fd)->len){
	ED(fd)->empty = 1;
    }
    else
      ED(fd)->empty = 0;

    if(ED(fd)->mkup_decl){
	if((ch &= 0xff) == '-'){
	    if(ED(fd)->hyphen){
		ED(fd)->hyphen = 0;
		if(ED(fd)->start_comment)
		  ED(fd)->end_comment = 1;
		else
		  ED(fd)->start_comment = 1;
	    }
	    else
	      ED(fd)->hyphen = 1;
	}
	else{
	    if(ED(fd)->end_comment)
	      ED(fd)->start_comment = ED(fd)->end_comment = 0;

	    /*
	     * no "--" after ! or non-whitespace between comments - bad
	     */
	    if(ED(fd)->len < 2 || (!ED(fd)->start_comment
				   && !ASCII_ISSPACE((unsigned char) ch)))
	      ED(fd)->badform = 1;	/* non-comment! */

	    ED(fd)->hyphen = 0;
	}

	/*
	 * Remember the comment for possible later processing, if
	 * it get's too long, remember first and last few chars
	 * so we know when to terminate (and throw some garbage
	 * in between when we toss out what's between.
	 */
	if(ED(fd)->len == HTML_BUF_LEN){
	    ED(fd)->buf[2] = ED(fd)->buf[3] = 'X';
	    ED(fd)->buf[4] = ED(fd)->buf[ED(fd)->len - 2];
	    ED(fd)->buf[5] = ED(fd)->buf[ED(fd)->len - 1];
	    ED(fd)->len    = 6;
	}

	ED(fd)->buf[(ED(fd)->len)++] = ch;
	return(0);			/* comments go in the bit bucket */
    }
    else if(ED(fd)->overrun || ED(fd)->badform){
	return(0);			/* swallow char's until next '>' */
    }
    else if(!ED(fd)->element && !ED(fd)->len){
	if(ch == '/'){		/* validate leading chars */
	    ED(fd)->end_tag = 1;
	    return(0);
	}
	else if(ch == '!'){
	    ED(fd)->mkup_decl = 1;
	    return(0);
	}
	else if(ch == '?'){
	    ED(fd)->proc_inst = 1;
	    return(0);
	}
	else if(!isalpha((unsigned char) ch))
	  return(-1);			/* can't be a tag! */
    }
    else if(ch == '\"' || ch == '\''){
	if(!ED(fd)->hit_equal){
	    ED(fd)->badform = 1;	/* quote in element name?!? */
	    return(0);
	}

	if(ED(fd)->quoted){
	    if(ED(fd)->quoted == (char) ch){
		/* end of a quoted value */
		ED(fd)->quoted = 0;
		if(ED(fd)->len && html_element_flush(ED(fd)))
		  ED(fd)->badform = 1;

		return(0);		/* continue collecting chars */
	    }
	    /* ELSE fall thru writing other quoting char */
	}
	else{
	    ED(fd)->quoted = (char) ch;
	    ED(fd)->was_quoted = 1;
	    return(0);			/* need more data */
	}
    }

    ch &= 0xff;			/* strip any "literal" high bits */
    if(ED(fd)->quoted
       || isalnum(ch)
       || strchr("#-.!", ch)){
	if(ED(fd)->len < ((ED(fd)->element || !ED(fd)->hit_equal)
			       ? HTML_BUF_LEN:MAX_ELEMENT)){
	    ED(fd)->buf[(ED(fd)->len)++] = ch;
	}
	else
	  ED(fd)->overrun = 1;		/* flag it broken */
    }
    else if(ASCII_ISSPACE((unsigned char) ch) || ch == '='){
	if((ED(fd)->len || ED(fd)->was_quoted) && html_element_flush(ED(fd))){
	    ED(fd)->badform = 1;
	    return(0);		/* else, we ain't done yet */
	}

	if(!ED(fd)->hit_equal)
	  ED(fd)->hit_equal = (ch == '=');
    }
    else
      ED(fd)->badform = 1;		/* unrecognized data?? */

    return(0);				/* keep collecting */
}


/*
 * Element collector found complete string, integrate it and reset
 * internal collection buffer.
 *
 * Returns zero if element collection buffer flushed, error flag otherwise
 */
int
html_element_flush(CLCTR_S *el_data)
{
    int rv = 0;

    if(el_data->hit_equal){		/* adding a value */
	el_data->hit_equal = 0;
	if(el_data->cur_attrib){
	    if(!el_data->cur_attrib->value){
		el_data->cur_attrib->value = cpystr(el_data->len
						    ? el_data->buf : "");
	    }
	    else{
		dprint((2, "** element: unexpected value: %.10s...\n",
			(el_data->len && el_data->buf) ? el_data->buf : "\"\""));
		rv = 1;
	    }
	}
	else{
	    dprint((2, "** element: missing attribute name: %.10s...\n",
		    (el_data->len && el_data->buf) ? el_data->buf : "\"\""));
	    rv = 2;
	}
    }
    else if(el_data->len){
	if(!el_data->element){
	    el_data->element = cpystr(el_data->buf);
	}
	else{
	    PARAMETER *p = (PARAMETER *)fs_get(sizeof(PARAMETER));
	    memset(p, 0, sizeof(PARAMETER));
	    if(el_data->attribs){
		el_data->cur_attrib->next = p;
		el_data->cur_attrib = p;
	    }
	    else
	      el_data->attribs = el_data->cur_attrib = p;

	    p->attribute = cpystr(el_data->buf);
	}

    }

    el_data->was_quoted = 0;	/* reset collector buf and state */
    el_data->len = 0;
    memset(el_data->buf, 0, HTML_BUF_LEN);
    return(rv);			/* report whatever happened above */
}


/*
 * html_element_comment - "Special" comment handling here
 */
void
html_element_comment(FILTER_S *f, char *s)
{
    char *p;

    while(*s && ASCII_ISSPACE((unsigned char) *s))
      s++;

    /*
     * WARNING: "!--chtml" denotes "Conditional HTML", a UW-ism.
     */
    if(!struncmp(s, "chtml ", 6)){
	s += 6;
	if(!struncmp(s, "if ", 3)){
	    HD(f)->bitbucket = 1;	/* default is failure! */
	    switch(*(s += 3)){
	      case 'P' :
	      case 'p' :
		if(!struncmp(s + 1, "inemode=", 8)){
		    if(!strucmp(s = removing_quotes(s + 9), "function_key")
		       && F_ON(F_USE_FK, ps_global))
		      HD(f)->bitbucket = 0;
		    else if(!strucmp(s, "running"))
		      HD(f)->bitbucket = 0;
		    else if(!strucmp(s, "phone_home") && ps_global->phone_home)
		      HD(f)->bitbucket = 0;
#ifdef	_WINDOWS
		    else if(!strucmp(s, "os_windows"))
		      HD(f)->bitbucket = 0;
#endif
		}

		break;

	      case '[' :	/* test */
		if((p = strindex(++s, ']')) != NULL){
		    *p = '\0';		/* tie off test string */
		    removing_leading_white_space(s);
		    removing_trailing_white_space(s);
		    if(*s == '-' && *(s+1) == 'r'){ /* readable file? */
			for(s += 2; *s && ASCII_ISSPACE((unsigned char) *s); s++)
			  ;


			HD(f)->bitbucket = (can_access(CHTML_VAR_EXPAND(removing_quotes(s)),
						       READ_ACCESS) != 0);
		    }
		}

		break;

	      default :
		break;
	    }
	}
	else if(!strucmp(s, "else")){
	    HD(f)->bitbucket = !HD(f)->bitbucket;
	}
	else if(!strucmp(s, "endif")){
	    /* Clean up after chtml here */
	    HD(f)->bitbucket = 0;
	}
    }
    else if(!HD(f)->bitbucket){
	if(!struncmp(s, "#include ", 9)){
	    char  buf[MAILTMPLEN], *bufp;
	    int   len, end_of_line;
	    FILE *fp;

	    /* Include the named file */
	    if(!struncmp(s += 9, "file=", 5)
	       && (fp = our_fopen(CHTML_VAR_EXPAND(removing_quotes(s+5)), "r"))){
		html_element_output(f, HTML_NEWLINE);

		while(fgets(buf, sizeof(buf), fp)){
		    if((len = strlen(buf)) && buf[len-1] == '\n'){
			end_of_line = 1;
			buf[--len]  = '\0';
		    }
		    else
		      end_of_line = 0;

		    for(bufp = buf; len; bufp++, len--)
		      html_element_output(f, (int) *bufp);

		    if(end_of_line)
		      html_element_output(f, HTML_NEWLINE);
		}

		fclose(fp);
		html_element_output(f, HTML_NEWLINE);
		HD(f)->blanks = 0;
		if(f->f1 == WSPACE)
		  f->f1 = DFL;
	    }
	}
	else if(!struncmp(s, "#echo ", 6)){
	    if(!struncmp(s += 6, "var=", 4)){
		char	*p, buf[MAILTMPLEN];
		ADDRESS *adr;
		extern char datestamp[];

		if(!strcmp(s = removing_quotes(s + 4), "ALPINE_VERSION")){
		    p = ALPINE_VERSION;
		}
		else if(!strcmp(s, "ALPINE_REVISION")){
		    p = get_alpine_revision_string(buf, sizeof(buf));
		}
		else if(!strcmp(s, "C_CLIENT_VERSION")){
		    p = CCLIENTVERSION;
		}
		else if(!strcmp(s, "ALPINE_COMPILE_DATE")){
		    p = datestamp;
		}
		else if(!strcmp(s, "ALPINE_TODAYS_DATE")){
		    rfc822_date(p = buf);
		}
		else if(!strcmp(s, "_LOCAL_FULLNAME_")){
		    p = (ps_global->VAR_LOCAL_FULLNAME
			 && ps_global->VAR_LOCAL_FULLNAME[0])
			    ? ps_global->VAR_LOCAL_FULLNAME
			    : "Local Support";
		}
		else if(!strcmp(s, "_LOCAL_ADDRESS_")){
		    p = (ps_global->VAR_LOCAL_ADDRESS
			 && ps_global->VAR_LOCAL_ADDRESS[0])
			   ? ps_global->VAR_LOCAL_ADDRESS
			   : "postmaster";
		    adr = rfc822_parse_mailbox(&p, ps_global->maildomain);
		    snprintf(p = buf, sizeof(buf), "%s@%s", adr->mailbox, adr->host);
		    mail_free_address(&adr);
		}
		else if(!strcmp(s, "_BUGS_FULLNAME_")){
		    p = (ps_global->VAR_BUGS_FULLNAME
			 && ps_global->VAR_BUGS_FULLNAME[0])
			    ? ps_global->VAR_BUGS_FULLNAME
			    : "Place to report Alpine Bugs";
		}
		else if(!strcmp(s, "_BUGS_ADDRESS_")){
		    p = (ps_global->VAR_BUGS_ADDRESS
			 && ps_global->VAR_BUGS_ADDRESS[0])
			    ? ps_global->VAR_BUGS_ADDRESS : "postmaster";
		    adr = rfc822_parse_mailbox(&p, ps_global->maildomain);
		    snprintf(p = buf, sizeof(buf), "%s@%s", adr->mailbox, adr->host);
		    mail_free_address(&adr);
		}
		else if(!strcmp(s, "CURRENT_DIR")){
		    getcwd(p = buf, sizeof(buf));
		}
		else if(!strcmp(s, "HOME_DIR")){
		    p = ps_global->home_dir;
		}
		else if(!strcmp(s, "PINE_CONF_PATH")){
#if defined(_WINDOWS) || !defined(SYSTEM_PINERC)
		    p = "/usr/local/lib/pine.conf";
#else
		    p = SYSTEM_PINERC;
#endif
		}
		else if(!strcmp(s, "PINE_CONF_FIXED_PATH")){
#ifdef SYSTEM_PINERC_FIXED
		    p = SYSTEM_PINERC_FIXED;
#else
		    p = "/usr/local/lib/pine.conf.fixed";
#endif
		}
		else if(!strcmp(s, "PINE_INFO_PATH")){
		    p = SYSTEM_PINE_INFO_PATH;
		}
		else if(!strcmp(s, "MAIL_SPOOL_PATH")){
		    p = sysinbox();
		}
		else if(!strcmp(s, "MAIL_SPOOL_LOCK_PATH")){
		    /* Don't put the leading /tmp/. */
		    int i, j;

		    p = sysinbox();
		    if(p){
			for(j = 0, i = 0; p[i] && j < MAILTMPLEN - 1; i++){
			    if(p[i] == '/')
				buf[j++] = '\\';
			    else
			      buf[j++] = p[i];
			}
			buf[j++] = '\0';
			p = buf;
		    }
		}
		else if(!struncmp(s, "VAR_", 4)){
		    p = s+4;
		    if(pith_opt_pretty_var_name)
		      p = (*pith_opt_pretty_var_name)(p);
		}
		else if(!struncmp(s, "FEAT_", 5)){
		    p = s+5;
		    if(pith_opt_pretty_feature_name)
		      p = (*pith_opt_pretty_feature_name)(p, -1);
		}
		else
		  p = NULL;

		if(p){
		    if(f->f1 == WSPACE){
			html_element_output(f, ' ');
			f->f1 = DFL;			/* clear it */
		    }

		    while(*p)
		      html_element_output(f, (int) *p++);
		}
	    }
	}
    }
}


void
html_element_output(FILTER_S *f, int ch)
{
    if(HANDLERS(f))
      (*EL(HANDLERS(f))->handler)(HANDLERS(f), ch, GF_DATA);
    else
      html_output(f, ch);
}


/*
 * collect html entity and return its UCS value when done.
 *
 * Returns HTML_MOREDATA : we need more data
 *	   HTML_ENTITY	 : entity collected
 *	   HTML_BADVALUE : good data, but no named match or out of range
 *	   HTML_BADDATA  : invalid input
 *
 * NOTES:
 *  - entity format is "'&' tag ';'" and represents a literal char
 *  - named entities are CASE SENSITIVE.
 *  - numeric char references (where the tag is prefixed with a '#')
 *    are a char with that numbers value
 *  - numeric vals are 0-255 except for the ranges: 0-8, 11-31, 127-159.
 */
int
html_entity_collector(FILTER_S *f, int ch, UCS *ucs, char **alt)
{
    static int  len = 0;
    static char buf[MAX_ENTITY+2];
    int		rv, i;

    if(len == MAX_ENTITY){
	rv = HTML_BADDATA;
    }
    else if((len == 0)
	      ? (isalpha((unsigned char) ch) || ch == '#')
	      : ((isdigit((unsigned char) ch)
		  || (isalpha((unsigned char) ch) && buf[0] != '#')))){
	buf[len++] = ch;
	return(HTML_MOREDATA);
    }
    else if(ch == ';' || ASCII_ISSPACE((unsigned char) ch)){
	buf[len] = '\0';		/* got something! */
	if(buf[0] == '#'){
	    *ucs = (UCS) strtoul(&buf[1], NULL, 10);
	    if(alt){
		*alt = NULL;
		for(i = 0; i < sizeof(entity_tab)/sizeof(struct html_entities); i++)
		  if(entity_tab[i].value == *ucs){
		      *alt = entity_tab[i].plain;
		      break;
		  }
	    }

	    len = 0;
	    return(HTML_ENTITY);
	}
	else{
	    rv = HTML_BADVALUE;		/* in case of no match */
	    for(i = 0; i < sizeof(entity_tab)/sizeof(struct html_entities); i++)
	      if(strcmp(entity_tab[i].name, buf) == 0){
		  *ucs = entity_tab[i].value;
		  if(alt)
		    *alt = entity_tab[i].plain;

		  len = 0;
		  return(HTML_ENTITY);
	      }
	}
    }
    else
      rv = HTML_BADDATA;		/* bogus input! */

    if(alt){
	buf[len]   = '\0';
	*alt	   = buf;
    }

    len = 0;
    return(rv);
}


/*----------------------------------------------------------------------
  HTML text to plain text filter

  This basically tries to do the best it can with HTML 2.0 (RFC1866)
  with bits of RFC 1942 (plus some HTML 3.2 thrown in as well) text
  formatting.

 ----*/
void
gf_html2plain(FILTER_S *f, int flg)
{
/* BUG: qoute incoming \255 values (see "yuml" above!) */
    if(flg == GF_DATA){
	register int c;
	GF_INIT(f, f->next);

	if(!HTML_WROTE(f)){
	    int ii;

	    for(ii = HTML_INDENT(f); ii > 0; ii--)
	      html_putc(f, ' ');

	    HTML_WROTE(f) = 1;
	}

	while(GF_GETC(f, c)){
	    /*
	     * First we have to collect any literal entities...
	     * that is, IF we're not already collecting one
	     * AND we're not in element's text or, if we are, we're
	     * not in quoted text.  Whew.
	     */
	    if(f->t){
		char *alt = NULL;
		UCS   ucs;

		switch(html_entity_collector(f, c, &ucs, &alt)){
		  case HTML_MOREDATA:	/* more data required? */
		    continue;		/* go get another char */

		  case HTML_BADVALUE :
		  case HTML_BADDATA :
		    /* if supplied, process bogus data */
		    HTML_PROC(f, '&');
		    for(; *alt; alt++){
			unsigned int uic = *alt;
			HTML_PROC(f, uic);
		    }

		    if(c == '&' && !HD(f)->quoted){
			f->t = '&';
			continue;
		    }
		    else
		      f->t = 0;		/* don't come back next time */

		    break;

		  default :		/* thing to process */
		    f->t = 0;		/* don't come back */

		    /*
		     * do something with UCS codepoint.  If it's
		     * not displayable then use the alt version 
		     * otherwise 
		     * cvt UCS to UTF-8 and toss into next filter.
		     */
		    if(ucs > 127 && wcellwidth(ucs) < 0){
			if(alt){
			    for(; *alt; alt++){
				c = MAKE_LITERAL(*alt);
				HTML_PROC(f, c);
			    }

			    continue;
			}
			else
			  c = MAKE_LITERAL('?');
		    }
		    else{
			unsigned char utf8buf[8], *p1, *p2;

			p2 = utf8_put(p1 = (unsigned char *) utf8buf, (unsigned long) ucs);
			for(; p1 < p2; p1++){
			    c = MAKE_LITERAL(*p1);
			    HTML_PROC(f, c);
			}

			continue;
		    }

		    break;
		}
	    }
	    else if(!PASS_HTML(f) && c == '&' && !HD(f)->quoted){
		f->t = '&';
		continue;
	    }

	    /*
	     * then we process whatever we got...
	     */

	    HTML_PROC(f, c);
	}

	GF_OP_END(f);			/* clean up our input pointers */
    }
    else if(flg == GF_EOD){
	while(HANDLERS(f)){
	    dprint((2, "-- html error: no closing tag for %s",EL(HANDLERS(f))->element));
	    html_pop(f, EL(HANDLERS(f)));
	}

	html_output(f, HTML_NEWLINE);
	if(ULINE_BIT(f))
	  HTML_ULINE(f, ULINE_BIT(f) = 0);

	if(BOLD_BIT(f))
	  HTML_BOLD(f, BOLD_BIT(f) = 0);

	HTML_FLUSH(f);
	fs_give((void **)&f->line);
	if(HD(f)->color)
	  free_color_pair(&HD(f)->color);

	fs_give(&f->data);
	if(f->opt){
	    if(((HTML_OPT_S *)f->opt)->base)
	      fs_give((void **) &((HTML_OPT_S *)f->opt)->base);

	    fs_give(&f->opt);
	}

	(*f->next->f)(f->next, GF_DATA);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint((9, "-- gf_reset html2plain\n"));
	f->data  = (HTML_DATA_S *) fs_get(sizeof(HTML_DATA_S));
	memset(f->data, 0, sizeof(HTML_DATA_S));
	/* start with flowing text */
	HD(f)->wrapstate = !PASS_HTML(f);
	HD(f)->wrapcol   = WRAP_COLS(f);
	f->f1    = DFL;			/* state */
	f->f2    = 0;			/* chars in wrap buffer */
	f->n     = 0L;			/* chars on line so far */
	f->linep = f->line = (char *)fs_get(HTML_BUF_LEN * sizeof(char));
	HD(f)->line_bufsize = HTML_BUF_LEN; /* initial bufsize of line */
	HD(f)->alt_entity =  (!ps_global->display_charmap
			      || strucmp(ps_global->display_charmap, "iso-8859-1"));
	HD(f)->cb.cbufp = HD(f)->cb.cbufend = HD(f)->cb.cbuf;
    }
}



/*
 * html_indent - do the requested indent level function with appropriate
 *		 flushing and such.
 *
 *   Returns: indent level prior to set/increment
 */
int
html_indent(FILTER_S *f, int val, int func)
{
    int old = HD(f)->indent_level;

    /* flush pending data at old indent level */
    switch(func){
      case HTML_ID_INC :
	html_output_flush(f);
	if((HD(f)->indent_level += val) < 0)
	  HD(f)->indent_level = 0;

	break;

      case HTML_ID_SET :
	html_output_flush(f);
	HD(f)->indent_level = val;
	break;

      default :
	break;
    }

    return(old);
}



/*
 * html_blanks - Insert n blank lines into output
 */
void
html_blank(FILTER_S *f, int n)
{
    /* Cap off any flowing text, and then write blank lines */
    if(f->f2 || f->n || CENTER_BIT(f) || HD(f)->centered || WRAPPED_LEN(f))
      html_output(f, HTML_NEWLINE);

    if(HD(f)->wrapstate)
      while(HD(f)->blanks < n)	/* blanks inc'd by HTML_NEWLINE */
	html_output(f, HTML_NEWLINE);
}



/*
 *  html_newline -- insert a newline mindful of embedded tags
 */
void
html_newline(FILTER_S *f)
{
    html_write_newline(f);		/* commit an actual newline */

    if(f->n){				/* and keep track of blank lines */
	HD(f)->blanks = 0;
	f->n = 0L;
    }
    else
      HD(f)->blanks++;
}


/*
 * output the given char, handling any requested wrapping.
 * It's understood that all whitespace handed us is written.  In other
 * words, junk whitespace is weeded out before it's given to us here.
 *
 */
void
html_output(FILTER_S *f, int ch)
{
    UCS uc;
    int width;
    void (*o_f)(FILTER_S *, int, int) = CENTER_BIT(f) ? html_output_centered : html_output_normal;

    /*
     * if ch is a control token, just pass it on, else, collect
     * utf8-encoded characters to determine width,then feed into
     * output routines
     */
    if(ch == TAG_EMBED || HD(f)->embedded.state || (ch > 0xff && IS_LITERAL(ch) == 0)){
	(*o_f)(f, ch, 1);
    }
    else if(utf8_to_ucs4_oneatatime(ch & 0xff, &(HD(f)->cb), &uc, &width)){
	unsigned char *cp;

	for(cp = HD(f)->cb.cbuf; cp <= HD(f)->cb.cbufend; cp++){
	    (*o_f)(f, *cp, width);
	    width = 0;		/* only count it once */
	}

	HD(f)->cb.cbufp = HD(f)->cb.cbufend = HD(f)->cb.cbuf;
    }
    else
      HD(f)->cb.cbufend = HD(f)->cb.cbufp;
    /* else do nothing until we have a full character */
}


void
html_output_string(FILTER_S *f, char *s)
{
    for(; *s; s++)
      html_output(f, *s);
}


void
html_output_raw_tag(FILTER_S *f, char *tag)
{
    PARAMETER *p;
    char      *vp;
    int	       i;

    html_output(f, '<');
    html_output_string(f, tag);
    for(p = HD(f)->el_data->attribs;
	p && p->attribute;
	p = p->next){
	/* SECURITY: no javascript */
	/* PRIVACY: no img src without permission */
	/* BUGS: no class collisions since <head> ignored */
	if(html_event_attribute(p->attribute)
	   || !strucmp(p->attribute, "class")
	   || (!PASS_IMAGES(f) && !strucmp(tag, "img") && !strucmp(p->attribute, "src")))
	  continue;

	/* PRIVACY: sniff out background images */
	if(p->value && !PASS_IMAGES(f)){
	    if(!strucmp(p->attribute, "style")){
		if((vp = srchstr(p->value, "background-image")) != NULL){
		    /* neuter in place */
		    vp[11] = vp[12] = vp[13] = vp[14] = vp[15] = 'X';
		}
		else{
		    for(vp = p->value; (vp = srchstr(vp, "background")) != NULL; vp++)
		      if(vp[10] == ' ' || vp[10] == ':')
			for(i = 11; vp[i] && vp[i] != ';'; i++)
			  if((vp[i] == 'u' && vp[i+1] == 'r' && vp[i+2] == 'l' && vp[i+3] == '(')
			     || vp[i] == ':' || vp[i] == '/' || vp[i] == '.')
			    vp[0] = 'X';
		}
	    }
	    else if(!strucmp(p->attribute, "background")){
		char *ip;

		for(ip = p->value; *ip && !(*ip == ':' || *ip == '/' || *ip == '.'); ip++)
		  ;

		if(ip)
		  continue;
	    }
	}

	html_output(f, ' ');
	html_output_string(f, p->attribute);
	if(p->value){
	    html_output(f, '=');
	    html_output(f, '\"');
	    html_output_string(f, p->value);
	    html_output(f, '\"');
	}
    }

    /* append warning to form submission */
    if(!strucmp(tag, "form")){
	html_output_string(f, " onsubmit=\"return window.confirm('This form is submitting information to an outside server.\\nAre you sure?');\"");
    }

    if(ED(f)->end_tag){
	html_output(f, ' ');
	html_output(f, '/');
    }

    html_output(f, '>');
}


int
html_event_attribute(char *attr)
{
    int i;
    static char *events[] = {
	"onabort",     "onblur",      "onchange",   "onclick",     "ondblclick", "ondragdrop",
	"onerror",     "onfocus",     "onkeydown",  "onkeypress",  "onkeyup",    "onload",
	"onmousedown", "onmousemove", "onmouseout", "onmouseover", "onmouseup",  "onmove",
	"onreset",     "onresize",    "onselec",    "onsubmit",    "onunload"
    };

    if((attr[0] == 'o' || attr[0] == 'O') && (attr[1] == 'n' || attr[1] == 'N'))
      for(i = 0; i < sizeof(events)/sizeof(events[0]); i++)
	if(!strucmp(attr, events[i]))
	  return(TRUE);

    return(FALSE);
}


void
html_output_normal(FILTER_S *f, int ch, int width)
{
    if(HD(f)->centered){
	html_centered_flush(f);
	fs_give((void **) &HD(f)->centered->line.buf);
	fs_give((void **) &HD(f)->centered->word.buf);
	fs_give((void **) &HD(f)->centered);
    }

    if(HD(f)->wrapstate){
	if(ch == HTML_NEWLINE){		/* hard newline */
	    html_output_flush(f);
	    html_newline(f);
	}
	else
	  HD(f)->blanks = 0;		/* reset blank line counter */

	if(ch == TAG_EMBED){	/* takes up no space */
	    HD(f)->embedded.state = -5;
	    HTML_LINEP_PUTC(f, TAG_EMBED);
	}
	else if(HD(f)->embedded.state){	/* ditto */
	    if(HD(f)->embedded.state == -5){
		/* looking for specially handled tags following TAG_EMBED */
		if(ch == TAG_HANDLE)
		  HD(f)->embedded.state = -1;	/* next ch is length */
		else if(ch == TAG_FGCOLOR || ch == TAG_BGCOLOR){
		    if(!HD(f)->color)
		      HD(f)->color = new_color_pair(NULL, NULL);

		    if(ch == TAG_FGCOLOR)
		      HD(f)->embedded.color = HD(f)->color->fg;
		    else
		      HD(f)->embedded.color = HD(f)->color->bg;

		    HD(f)->embedded.state = RGBLEN;
		}
		else
		  HD(f)->embedded.state = 0;	/* non-special */
	    }
	    else if(HD(f)->embedded.state > 0){
		/* collecting up an RGBLEN color or length, ignore tags */
		(HD(f)->embedded.state)--;
		if(HD(f)->embedded.color)
		  *HD(f)->embedded.color++ = ch;

		if(HD(f)->embedded.state == 0 && HD(f)->embedded.color){
		    *HD(f)->embedded.color = '\0';
		    HD(f)->embedded.color = NULL;
		}
	    }
	    else if(HD(f)->embedded.state < 0){
		HD(f)->embedded.state = ch;	/* number of embedded chars */
	    }
	    else{
		(HD(f)->embedded.state)--;
		if(HD(f)->embedded.color)
		  *HD(f)->embedded.color++ = ch;

		if(HD(f)->embedded.state == 0 && HD(f)->embedded.color){
		    *HD(f)->embedded.color = '\0';
		    HD(f)->embedded.color = NULL;
		}
	    }

	    HTML_LINEP_PUTC(f, ch);
	}
	else if(HTML_ISSPACE(ch)){
	    html_output_flush(f);
	}
	else{
	    if(HD(f)->prefix)
	      html_a_prefix(f);

	    if((f->f2 += width) + 1 >= WRAP_COLS(f)){
		HTML_LINEP_PUTC(f, ch & 0xff);
		HTML_FLUSH(f);
		html_newline(f);
		if(HD(f)->in_anchor)
		  html_write_anchor(f, HD(f)->in_anchor);
	    }
	    else
	      HTML_LINEP_PUTC(f, ch & 0xff);
	}
    }
    else{
	if(HD(f)->prefix)
	  html_a_prefix(f);

	html_output_flush(f);

	switch(HD(f)->embedded.state){
	  case 0 :
	    switch(ch){
	      default :
		/*
		 * It's difficult to both preserve whitespace and wrap at the
		 * same time so we'll do a dumb wrap at the edge of the screen.
		 * Since this shouldn't come up much in real life we'll hope
		 * it is good enough.
		 */
		if(!PASS_HTML(f) && (f->n + width) > WRAP_COLS(f))
		  html_newline(f);

		f->n += width;			/* inc displayed char count */
		HD(f)->blanks = 0;		/* reset blank line counter */
		html_putc(f, ch & 0xff);
		break;

	      case TAG_EMBED :	/* takes up no space */
		html_putc(f, TAG_EMBED);
		HD(f)->embedded.state = -2;
		break;

	      case HTML_NEWLINE :		/* newline handling */
		if(!f->n)
		  break;

	      case '\n' :
		html_newline(f);

	      case '\r' :
		break;
	    }

	    break;

	  case -2 :
	    HD(f)->embedded.state = 0;
	    switch(ch){
	      case TAG_HANDLE :
		HD(f)->embedded.state = -1;	/* next ch is length */
		break;

	      case TAG_BOLDON :
		BOLD_BIT(f) = 1;
		break;

	      case TAG_BOLDOFF :
		BOLD_BIT(f) = 0;
		break;

	      case TAG_ULINEON :
		ULINE_BIT(f) = 1;
		break;

	      case TAG_ULINEOFF :
		ULINE_BIT(f) = 0;
		break;

	      case TAG_FGCOLOR :
		if(!HD(f)->color)
		  HD(f)->color = new_color_pair(NULL, NULL);

		HD(f)->embedded.color = HD(f)->color->fg;
		HD(f)->embedded.state = 11;
		break;

	      case TAG_BGCOLOR :
		if(!HD(f)->color)
		  HD(f)->color = new_color_pair(NULL, NULL);

		HD(f)->embedded.color = HD(f)->color->bg;
		HD(f)->embedded.state = 11;
		break;

	      case TAG_HANDLEOFF :
		ch = TAG_INVOFF;
		HD(f)->in_anchor = 0;
		break;

	      default :
		break;
	    }

	    html_putc(f, ch);
	    break;

	  case -1 :
	    HD(f)->embedded.state = ch;	/* number of embedded chars */
	    html_putc(f, ch);
	    break;

	  default :
	    HD(f)->embedded.state--;
	    if(HD(f)->embedded.color)
	      *HD(f)->embedded.color++ = ch;

	    if(HD(f)->embedded.state == 0 && HD(f)->embedded.color){
		*HD(f)->embedded.color = '\0';
		HD(f)->embedded.color = NULL;
	    }

	    html_putc(f, ch);
	    break;
	}
    }
}


/*
 * flush any buffered chars waiting for wrapping.
 */
void
html_output_flush(FILTER_S *f)
{
    if(f->f2){
	if(f->n && ((int) f->n) + 1 + f->f2 > HD(f)->wrapcol)
	  html_newline(f);		/* wrap? */

	if(f->n){			/* text already on the line? */
	    html_putc(f, ' ');
	    f->n++;			/* increment count */
	}
	else{
	    /* write at start of new line */
	    html_write_indent(f, HD(f)->indent_level);

	    if(HD(f)->in_anchor)
	      html_write_anchor(f, HD(f)->in_anchor);
	}

	f->n += f->f2;
	HTML_FLUSH(f);
    }
}



/*
 * html_output_centered - managed writing centered text
 */
void
html_output_centered(FILTER_S *f, int ch, int width)
{
    if(!HD(f)->centered){		/* new text? */
	html_output_flush(f);
	if(f->n)			/* start on blank line */
	  html_newline(f);

	HD(f)->centered = (CENTER_S *) fs_get(sizeof(CENTER_S));
	memset(HD(f)->centered, 0, sizeof(CENTER_S));
	/* and grab a buf to start collecting centered text */
	HD(f)->centered->line.len  = WRAP_COLS(f);
	HD(f)->centered->line.buf  = (char *) fs_get(HD(f)->centered->line.len
							      * sizeof(char));
	HD(f)->centered->line.used = HD(f)->centered->line.width = 0;
	HD(f)->centered->word.len  = 32;
	HD(f)->centered->word.buf  = (char *) fs_get(HD(f)->centered->word.len
							       * sizeof(char));
	HD(f)->centered->word.used = HD(f)->centered->word.width = 0;
    }

    if(ch == HTML_NEWLINE){		/* hard newline */
	html_centered_flush(f);
    }
    else if(ch == TAG_EMBED){		/* takes up no space */
	HD(f)->embedded.state = -5;
	html_centered_putc(&HD(f)->centered->word, TAG_EMBED);
    }
    else if(HD(f)->embedded.state){
	if(HD(f)->embedded.state == -5){
	    /* looking for specially handled tags following TAG_EMBED */
	    if(ch == TAG_HANDLE)
	      HD(f)->embedded.state = -1;	/* next ch is length */
	    else if(ch == TAG_FGCOLOR || ch == TAG_BGCOLOR){
		if(!HD(f)->color)
		  HD(f)->color = new_color_pair(NULL, NULL);

		if(ch == TAG_FGCOLOR)
		  HD(f)->embedded.color = HD(f)->color->fg;
		else
		  HD(f)->embedded.color = HD(f)->color->bg;

		HD(f)->embedded.state = RGBLEN;
	    }
	    else
		  HD(f)->embedded.state = 0;	/* non-special */
	}
	else if(HD(f)->embedded.state > 0){
	    /* collecting up an RGBLEN color or length, ignore tags */
	    (HD(f)->embedded.state)--;
	    if(HD(f)->embedded.color)
	      *HD(f)->embedded.color++ = ch;

	    if(HD(f)->embedded.state == 0 && HD(f)->embedded.color){
		*HD(f)->embedded.color = '\0';
		HD(f)->embedded.color = NULL;
	    }
	}
	else if(HD(f)->embedded.state < 0){
	    HD(f)->embedded.state = ch;	/* number of embedded chars */
	}
	else{
	    (HD(f)->embedded.state)--;
	    if(HD(f)->embedded.color)
	      *HD(f)->embedded.color++ = ch;

	    if(HD(f)->embedded.state == 0 && HD(f)->embedded.color){
		*HD(f)->embedded.color = '\0';
		HD(f)->embedded.color = NULL;
	    }
	}

	html_centered_putc(&HD(f)->centered->word, ch);
    }
    else if(ASCII_ISSPACE((unsigned char) ch)){
	if(!HD(f)->centered->space++){	/* end of a word? flush! */
	    int i;

	    if(WRAPPED_LEN(f) > HD(f)->wrapcol){
		html_centered_flush_line(f);
		/* fall thru to put current "word" on blank "line" */
	    }
	    else if(HD(f)->centered->line.width){
		/* put space char between line and appended word */
		html_centered_putc(&HD(f)->centered->line, ' ');
		HD(f)->centered->line.width++;
	    }

	    for(i = 0; i < HD(f)->centered->word.used; i++)
	      html_centered_putc(&HD(f)->centered->line,
				 HD(f)->centered->word.buf[i]);

	    HD(f)->centered->line.width += HD(f)->centered->word.width;
	    HD(f)->centered->word.used  = 0;
	    HD(f)->centered->word.width = 0;
	}
    }
    else{
	if(HD(f)->prefix)
	  html_a_prefix(f);

	/* ch is start of next word */
	HD(f)->centered->space = 0;
	if(HD(f)->centered->word.width >= WRAP_COLS(f))
	  html_centered_flush(f);

	html_centered_putc(&HD(f)->centered->word, ch);
	HD(f)->centered->word.width++;
    }
}


/*
 * html_centered_putc -- add given char to given WRAPLINE_S
 */
void
html_centered_putc(WRAPLINE_S *wp, int ch)
{
    if(wp->used + 1 >= wp->len){
	wp->len += 64;
	fs_resize((void **) &wp->buf, wp->len * sizeof(char));
    }

    wp->buf[wp->used++] = ch;
}



/*
 * html_centered_flush - finish writing any pending centered output
 */
void
html_centered_flush(FILTER_S *f)
{
    int i;

    /*
     * If word present (what about line?) we need to deal with
     * appending it...
     */
    if(HD(f)->centered->word.width && WRAPPED_LEN(f) > HD(f)->wrapcol)
      html_centered_flush_line(f);

    if(WRAPPED_LEN(f)){
	/* figure out how much to indent */
	if((i = (WRAP_COLS(f) - WRAPPED_LEN(f))/2) > 0)
	  html_write_indent(f, i);

	if(HD(f)->centered->anchor)
	  html_write_anchor(f, HD(f)->centered->anchor);

	html_centered_handle(&HD(f)->centered->anchor,
			     HD(f)->centered->line.buf,
			     HD(f)->centered->line.used);
	html_write(f, HD(f)->centered->line.buf, HD(f)->centered->line.used);

	if(HD(f)->centered->word.used){
	    if(HD(f)->centered->line.width)
	      html_putc(f, ' ');

	    html_centered_handle(&HD(f)->centered->anchor,
				 HD(f)->centered->word.buf,
				 HD(f)->centered->word.used);
	    html_write(f, HD(f)->centered->word.buf,
		       HD(f)->centered->word.used);
	}

	HD(f)->centered->line.used  = HD(f)->centered->word.used  = 0;
	HD(f)->centered->line.width = HD(f)->centered->word.width = 0;
    }
    else{
      if(HD(f)->centered->word.used){
	html_write(f, HD(f)->centered->word.buf,
		   HD(f)->centered->word.used);
	HD(f)->centered->line.used  = HD(f)->centered->word.used  = 0;
	HD(f)->centered->line.width = HD(f)->centered->word.width = 0;
      }
      HD(f)->blanks++;			/* advance the blank line counter */
    }

    html_newline(f);			/* finish the line */
}


/*
 * html_centered_handle - scan the line for embedded handles
 */
void
html_centered_handle(int *h, char *line, int len)
{
    int n;

    while(len-- > 0)
      if(*line++ == TAG_EMBED && len-- > 0)
	switch(*line++){
	  case TAG_HANDLE :
	    if((n = *line++) >= --len){
		*h = 0;
		len -= n;
		while(n--)
		  *h = (*h * 10) + (*line++ - '0');
	    }
	    break;

	  case TAG_HANDLEOFF :
	  case TAG_INVOFF :
	    *h = 0;		/* assumption 23,342: inverse off ends tags */
	    break;

	  default :
	    break;
	}
}



/*
 * html_centered_flush_line - flush the centered "line" only
 */
void
html_centered_flush_line(FILTER_S *f)
{
    if(HD(f)->centered->line.used){
	int i, j;

	/* hide "word" from flush */
	i = HD(f)->centered->word.used;
	j = HD(f)->centered->word.width;
	HD(f)->centered->word.used  = 0;
	HD(f)->centered->word.width = 0;
	html_centered_flush(f);

	HD(f)->centered->word.used  = i;
	HD(f)->centered->word.width = j;
    }
}


/*
 * html_write_indent - write indention mindful of display attributes
 */
void
html_write_indent(FILTER_S *f, int indent)
{
    if(! STRIP(f)){
	if(BOLD_BIT(f)){
	    html_putc(f, TAG_EMBED);
	    html_putc(f, TAG_BOLDOFF);
	}

	if(ULINE_BIT(f)){
	    html_putc(f, TAG_EMBED);
	    html_putc(f, TAG_ULINEOFF);
	}
    }

    f->n = indent;
    while(indent-- > 0)
      html_putc(f, ' ');		/* indent as needed */

    /*
     * Resume any previous embedded state
     */
    if(! STRIP(f)){
	if(BOLD_BIT(f)){
	    html_putc(f, TAG_EMBED);
	    html_putc(f, TAG_BOLDON);
	}

	if(ULINE_BIT(f)){
	    html_putc(f, TAG_EMBED);
	    html_putc(f, TAG_ULINEON);
	}
    }
}


/*
 *
 */
void
html_write_anchor(FILTER_S *f, int anchor)
{
    char buf[256];
    int  i;

    html_putc(f, TAG_EMBED);
    html_putc(f, TAG_HANDLE);
    snprintf(buf, sizeof(buf), "%d", anchor);
    html_putc(f, (int) strlen(buf));

    for(i = 0; buf[i]; i++)
      html_putc(f, buf[i]);
}


/*
 * html_write_newline - write a newline mindful of display attributes
 */
void
html_write_newline(FILTER_S *f)
{
    int i;

    if(! STRIP(f)){			/* First tie, off any embedded state */
	if(HD(f)->in_anchor){
	    html_putc(f, TAG_EMBED);
	    html_putc(f, TAG_INVOFF);
	}

	if(BOLD_BIT(f)){
	    html_putc(f, TAG_EMBED);
	    html_putc(f, TAG_BOLDOFF);
	}

	if(ULINE_BIT(f)){
	    html_putc(f, TAG_EMBED);
	    html_putc(f, TAG_ULINEOFF);
	}

	if(HD(f)->color && (HD(f)->color->fg[0] || HD(f)->color->bg[0])){
	    char        *p;
	    int          i;

	    p = color_embed(ps_global->VAR_NORM_FORE_COLOR,
			    ps_global->VAR_NORM_BACK_COLOR);
	    for(i = 0; i < 2 * (RGBLEN + 2); i++)
	      html_putc(f, p[i]);
	}
    }

    html_write(f, "\015\012", 2);
    for(i = HTML_INDENT(f); i > 0; i--)
      html_putc(f, ' ');

    if(! STRIP(f)){			/* First tie, off any embedded state */
	if(BOLD_BIT(f)){
	    html_putc(f, TAG_EMBED);
	    html_putc(f, TAG_BOLDON);
	}

	if(ULINE_BIT(f)){
	    html_putc(f, TAG_EMBED);
	    html_putc(f, TAG_ULINEON);
	}

	if(HD(f)->color && (HD(f)->color->fg[0] || HD(f)->color->bg[0])){
	    char        *p, *tfg, *tbg;
	    int          i;
	    COLOR_PAIR  *tmp;

	    tfg = HD(f)->color->fg;
	    tbg = HD(f)->color->bg;
	    tmp = new_color_pair(tfg[0] ? tfg 
	      : color_to_asciirgb(ps_global->VAR_NORM_FORE_COLOR),
	      tbg[0] ? tbg
	      : color_to_asciirgb(ps_global->VAR_NORM_BACK_COLOR));
	    if(pico_is_good_colorpair(tmp)){
		p = color_embed(tfg[0] ? tfg 
				: ps_global->VAR_NORM_FORE_COLOR,
				tbg[0] ? tbg 
				: ps_global->VAR_NORM_BACK_COLOR);
		for(i = 0; i < 2 * (RGBLEN + 2); i++)
		  html_putc(f, p[i]);
	    }

	    if(tmp)
	      free_color_pair(&tmp);
	}
    }
}


/*
 * html_write - write given n-length string to next filter
 */
void
html_write(FILTER_S *f, char *s, int n)
{
    GF_INIT(f, f->next);

    while(n-- > 0){
	/* keep track of attribute state?  Not if last char! */
	if(!STRIP(f) && *s == TAG_EMBED && n-- > 0){
	    GF_PUTC(f->next, TAG_EMBED);
	    switch(*++s){
	      case TAG_BOLDON :
		BOLD_BIT(f) = 1;
		break;
	      case TAG_BOLDOFF :
		BOLD_BIT(f) = 0;
		break;
	      case TAG_ULINEON :
		ULINE_BIT(f) = 1;
		break;
	      case TAG_ULINEOFF :
		ULINE_BIT(f) = 0;
		break;
	      case TAG_HANDLEOFF :
		HD(f)->in_anchor = 0;
		GF_PUTC(f->next, TAG_INVOFF);
		s++;
		continue;
	      case TAG_HANDLE :
		if(n-- > 0){
		    int i = *++s;

		    GF_PUTC(f->next, TAG_HANDLE);
		    if(i <= n){
			int	  anum = 0;
			HANDLE_S *h;

			n -= i;
			GF_PUTC(f->next, i);
			while(1){
			    anum = (anum * 10) + (*++s - '0');
			    if(--i)
			      GF_PUTC(f->next, *s);
			    else
			      break;
			}

			if(DO_HANDLES(f)
			   && (h = get_handle(*HANDLESP(f), anum)) != NULL
			   && (h->type == URL || h->type == Attach)){
			    HD(f)->in_anchor = anum;
			}
		    }
		}

		break;
	      default:
		break;
	    }
	}

	GF_PUTC(f->next, (*s++) & 0xff);
    }

    GF_IP_END(f->next);			/* clean up next's input pointers */
}


/*
 * html_putc -- actual work of writing to next filter.
 *		NOTE: Small opt not using full GF_END since our input
 *		      pointers don't need adjusting.
 */
void
html_putc(FILTER_S *f, int ch)
{
    GF_INIT(f, f->next);
    GF_PUTC(f->next, ch & 0xff);
    GF_IP_END(f->next);			/* clean up next's input pointers */
}



/*
 * Only current option is to turn on embedded data stripping for text
 * bound to a printer or composer.
 */
void *
gf_html2plain_opt(char *base,
		  int columns,
		  int *margin,
		  HANDLE_S **handlesp,
		  htmlrisk_t risk_f,
		  int flags)
{
    HTML_OPT_S *op;
    int		margin_l, margin_r;

    op = (HTML_OPT_S *) fs_get(sizeof(HTML_OPT_S));

    op->base	    = cpystr(base);
    margin_l	    = (margin) ? margin[0] : 0;
    margin_r	    = (margin) ? margin[1] : 0;
    op->indent	    = margin_l;
    op->columns	    = columns - (margin_l + margin_r);
    op->strip	    = ((flags & GFHP_STRIPPED) == GFHP_STRIPPED);
    op->handlesp    = handlesp;
    op->handles_loc = ((flags & GFHP_LOCAL_HANDLES) == GFHP_LOCAL_HANDLES);
    op->showserver  = ((flags & GFHP_SHOW_SERVER) == GFHP_SHOW_SERVER);
    op->warnrisk_f  = risk_f;
    op->no_relative_links = ((flags & GFHP_NO_RELATIVE) == GFHP_NO_RELATIVE);
    op->related_content	  = ((flags & GFHP_RELATED_CONTENT) == GFHP_RELATED_CONTENT);
    op->html	    = ((flags & GFHP_HTML) == GFHP_HTML);
    op->html_imgs   = ((flags & GFHP_HTML_IMAGES) == GFHP_HTML_IMAGES);
    op->element_table = html_element_table;
    return((void *) op);
}


void *
gf_html2plain_rss_opt(RSS_FEED_S **feedp, int flags)
{
    HTML_OPT_S *op;
    int		margin_l, margin_r;

    op = (HTML_OPT_S *) fs_get(sizeof(HTML_OPT_S));
    memset(op, 0, sizeof(HTML_OPT_S));

    op->base = cpystr("");
    op->element_table = rss_element_table;
    *(op->feedp = feedp) = NULL;
    return((void *) op);
}

void
gf_html2plain_rss_free(RSS_FEED_S **feedp)
{
    if(feedp && *feedp){
	if((*feedp)->title)
	  fs_give((void **) &(*feedp)->title);

	if((*feedp)->link)
	  fs_give((void **) &(*feedp)->link);

	if((*feedp)->description)
	  fs_give((void **) &(*feedp)->description);

	if((*feedp)->source)
	  fs_give((void **) &(*feedp)->source);

	if((*feedp)->image)
	  fs_give((void **) &(*feedp)->image);

	gf_html2plain_rss_free_items(&((*feedp)->items));
	fs_give((void **) feedp);
    }
}

void
gf_html2plain_rss_free_items(RSS_ITEM_S **itemp)
{
    if(itemp && *itemp){
	if((*itemp)->title)
	  fs_give((void **) &(*itemp)->title);

	if((*itemp)->link)
	  fs_give((void **) &(*itemp)->link);

	if((*itemp)->description)
	  fs_give((void **) &(*itemp)->description);

	if((*itemp)->source)
	  fs_give((void **) &(*itemp)->source);

	gf_html2plain_rss_free_items(&(*itemp)->next);
	fs_give((void **) itemp);
    }
}


/* END OF HTML-TO-PLAIN text filter */

/*
 * ESCAPE CODE FILTER - remove unknown and possibly dangerous escape codes
 * from the text stream.
 */

#define	MAX_ESC_LEN	5

/*
 * the simple filter, removes unknown escape codes from the stream
 */
void
gf_escape_filter(FILTER_S *f, int flg)
{
    register char *p;
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;
	register int state = f->f1;

	while(GF_GETC(f, c)){

	    if(state){
		if(c == '\033' || f->n == MAX_ESC_LEN){
		    f->line[f->n] = '\0';
		    f->n = 0L;
		    if(!match_escapes(f->line)){
			GF_PUTC(f->next, '^');
			GF_PUTC(f->next, '[');
		    }
		    else
		      GF_PUTC(f->next, '\033');

		    p = f->line;
		    while(*p)
		      GF_PUTC(f->next, *p++);

		    if(c == '\033')
		      continue;
		    else
		      state = 0;			/* fall thru */
		}
		else{
		    f->line[f->n++] = c;		/* collect */
		    continue;
		}
	    }

	    if(c == '\033')
	      state = 1;
	    else
	      GF_PUTC(f->next, c);
	}

	f->f1 = state;
	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	if(f->f1){
	    if(!match_escapes(f->line)){
		GF_PUTC(f->next, '^');
		GF_PUTC(f->next, '[');
	    }
	    else
	      GF_PUTC(f->next, '\033');
	}

	for(p = f->line; f->n; f->n--, p++)
	  GF_PUTC(f->next, *p);

	fs_give((void **)&(f->line));	/* free temp line buffer */
	(void) GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint((9, "-- gf_reset escape\n"));
	f->f1    = 0;
	f->n     = 0L;
	f->linep = f->line = (char *)fs_get((MAX_ESC_LEN + 1) * sizeof(char));
    }
}



/*
 * CONTROL CHARACTER FILTER - transmogrify control characters into their
 * corresponding string representations (you know, ^blah and such)...
 */

/*
 * the simple filter transforms unknown control characters in the stream
 * into harmless strings.
 */
void
gf_control_filter(FILTER_S *f, int flg)
{
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;
	register int filt_only_c0;

	filt_only_c0 = f->opt ? (*(int *) f->opt) : 0;

	while(GF_GETC(f, c)){

	    if(((c < 0x20 || c == 0x7f)
		|| (c >= 0x80 && c < 0xA0 && !filt_only_c0))
	       && !(ASCII_ISSPACE((unsigned char) c)
		    || c == '\016' || c == '\017' || c == '\033')){
		GF_PUTC(f->next, c >= 0x80 ? '~' : '^');
		GF_PUTC(f->next, (c == 0x7f) ? '?' : (c & 0x1f) + '@');
	    }
	    else
	      GF_PUTC(f->next, c);
	}

	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	(void) GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
}


/*
 * function called from the outside to set
 * control filter's option, which says to filter C0 control characters
 * but not C1 control chars. We don't call it at all if we don't want
 * to filter C0 chars either.
 */
void *
gf_control_filter_opt(int *filt_only_c0)
{
    return((void *) filt_only_c0);
}


/*
 * TAG FILTER - quote all TAG_EMBED characters by doubling them.
 * This prevents the possibility of embedding other tags.
 * We assume that this filter should only be used for something
 * that is eventually writing to a display, which has the special
 * knowledge of quoted TAG_EMBEDs.
 */
void
gf_tag_filter(FILTER_S *f, int flg)
{
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;

	while(GF_GETC(f, c)){

	    if((c & 0xff) == (TAG_EMBED & 0xff)){
		GF_PUTC(f->next, TAG_EMBED);
		GF_PUTC(f->next, c);
	    }
	    else
	      GF_PUTC(f->next, c);
	}

	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	(void) GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
}


/*
 * LINEWRAP FILTER - insert CRLF's at end of nearest whitespace before
 * specified line width
 */


typedef struct wrap_col_s {
    unsigned	bold:1;
    unsigned	uline:1;
    unsigned	inverse:1;
    unsigned	tags:1;
    unsigned	do_indent:1;
    unsigned	on_comma:1;
    unsigned	flowed:1;
    unsigned	delsp:1;
    unsigned	quoted:1;
    unsigned	allwsp:1;
    unsigned	hard_nl:1;
    unsigned	leave_flowed:1;
    unsigned    use_color:1;
    unsigned    hdr_color:1;
    unsigned    for_compose:1;
    unsigned    handle_soft_hyphen:1;
    unsigned    saw_soft_hyphen:1;
    unsigned	trailing_space:1;
    unsigned char  utf8buf[7];
    unsigned char *utf8bufp;
    COLOR_PAIR *color;
    STORE_S    *spaces;
    short	embedded,
		space_len;
    char       *lineendp;
    int		anchor,
		prefbrk,
		prefbrkn,
		quote_depth,
		quote_count,
		sig,
		state,
		wrap_col,
		wrap_max,
		margin_l,
		margin_r,
		indent;
    char	special[256];
} WRAP_S;

#define	WRAP_MARG_L(F)	(((WRAP_S *)(F)->opt)->margin_l)
#define	WRAP_MARG_R(F)	(((WRAP_S *)(F)->opt)->margin_r)
#define	WRAP_COL(F)	(((WRAP_S *)(F)->opt)->wrap_col - WRAP_MARG_R(F) - ((((WRAP_S *)(F)->opt)->leave_flowed) ? 1 : 0))
#define	WRAP_MAX_COL(F)	(((WRAP_S *)(F)->opt)->wrap_max - WRAP_MARG_R(F) - ((((WRAP_S *)(F)->opt)->leave_flowed) ? 1 : 0))
#define	WRAP_INDENT(F)	(((WRAP_S *)(F)->opt)->indent)
#define	WRAP_DO_IND(F)	(((WRAP_S *)(F)->opt)->do_indent)
#define	WRAP_COMMA(F)	(((WRAP_S *)(F)->opt)->on_comma)
#define	WRAP_FLOW(F)	(((WRAP_S *)(F)->opt)->flowed)
#define	WRAP_DELSP(F)	(((WRAP_S *)(F)->opt)->delsp)
#define	WRAP_FL_QD(F)	(((WRAP_S *)(F)->opt)->quote_depth)
#define	WRAP_FL_QC(F)	(((WRAP_S *)(F)->opt)->quote_count)
#define	WRAP_FL_SIG(F)	(((WRAP_S *)(F)->opt)->sig)
#define	WRAP_HARD(F)	(((WRAP_S *)(F)->opt)->hard_nl)
#define	WRAP_LV_FLD(F)	(((WRAP_S *)(F)->opt)->leave_flowed)
#define	WRAP_USE_CLR(F)	(((WRAP_S *)(F)->opt)->use_color)
#define	WRAP_HDR_CLR(F)	(((WRAP_S *)(F)->opt)->hdr_color)
#define	WRAP_FOR_CMPS(F) (((WRAP_S *)(F)->opt)->for_compose)
#define	WRAP_HANDLE_SOFT_HYPHEN(F) (((WRAP_S *)(F)->opt)->handle_soft_hyphen)
#define	WRAP_SAW_SOFT_HYPHEN(F) (((WRAP_S *)(F)->opt)->saw_soft_hyphen)
#define	WRAP_UTF8BUF(F, C) (((WRAP_S *)(F)->opt)->utf8buf[C])
#define	WRAP_UTF8BUFP(F)   (((WRAP_S *)(F)->opt)->utf8bufp)
#define	WRAP_STATE(F)	(((WRAP_S *)(F)->opt)->state)
#define	WRAP_QUOTED(F)	(((WRAP_S *)(F)->opt)->quoted)
#define	WRAP_TAGS(F)	(((WRAP_S *)(F)->opt)->tags)
#define	WRAP_BOLD(F)	(((WRAP_S *)(F)->opt)->bold)
#define	WRAP_ULINE(F)	(((WRAP_S *)(F)->opt)->uline)
#define	WRAP_INVERSE(F)	(((WRAP_S *)(F)->opt)->inverse)
#define	WRAP_LASTC(F)	(((WRAP_S *)(F)->opt)->lineendp)
#define	WRAP_EMBED(F)	(((WRAP_S *)(F)->opt)->embedded)
#define	WRAP_ANCHOR(F)	(((WRAP_S *)(F)->opt)->anchor)
#define	WRAP_PB_OFF(F)	(((WRAP_S *)(F)->opt)->prefbrk)
#define	WRAP_PB_LEN(F)	(((WRAP_S *)(F)->opt)->prefbrkn)
#define	WRAP_ALLWSP(F)	(((WRAP_S *)(F)->opt)->allwsp)
#define	WRAP_SPC_LEN(F)	(((WRAP_S *)(F)->opt)->space_len)
#define	WRAP_TRL_SPC(F)	(((WRAP_S *)(F)->opt)->trailing_space)
#define	WRAP_SPEC(F, C)	((WRAP_S *) (F)->opt)->special[C]
#define	WRAP_COLOR(F)	(((WRAP_S *)(F)->opt)->color)
#define	WRAP_COLOR_SET(F)  ((WRAP_COLOR(F)) && (WRAP_COLOR(F)->fg[0]))
#define	WRAP_SPACES(F)	(((WRAP_S *)(F)->opt)->spaces)
#define	WRAP_PUTC(F,C,W) {						\
			    if((F)->linep == WRAP_LASTC(F)){		\
				size_t offset = (F)->linep - (F)->line;	\
				fs_resize((void **) &(F)->line,		\
					  (2 * offset) * sizeof(char)); \
				(F)->linep = &(F)->line[offset];	\
				WRAP_LASTC(F) = &(F)->line[2*offset-1];	\
			    }						\
			    *(F)->linep++ = (C);			\
			    (F)->f2 += (W);				\
			}

#define	WRAP_EMBED_PUTC(F,C) {						\
			    if((F)->f2){				\
			        WRAP_PUTC((F), C, 0);			\
			    }						\
			    else					\
			      so_writec(C, WRAP_SPACES(F));		\
}

#define	WRAP_COLOR_UNSET(F)	{					\
			    if(WRAP_COLOR_SET(F)){			\
			      WRAP_COLOR(F)->fg[0] = '\0';		\
			    }						\
			}

/*
 * wrap_flush_embed flags
 */
#define	WFE_NONE	0		/* Nothing special */
#define	WFE_CNT_HANDLE	1		/* account for/don't write handles */


int	wrap_flush(FILTER_S *, unsigned char **, unsigned char **, unsigned char **, unsigned char **);
int	wrap_flush_embed(FILTER_S *, unsigned char **, unsigned char **,
			 unsigned char **, unsigned char **);
int	wrap_flush_s(FILTER_S *,char *, int, int, unsigned char **, unsigned char **,
		     unsigned char **, unsigned char **, int);
int	wrap_eol(FILTER_S *, int, unsigned char **, unsigned char **,
		 unsigned char **, unsigned char **);
int	wrap_bol(FILTER_S *, int, int, unsigned char **,
		 unsigned char **, unsigned char **, unsigned char **);
int	wrap_quote_insert(FILTER_S *, unsigned char **, unsigned char **,
			  unsigned char **, unsigned char **);

/*
 * the no longer simple filter, breaks lines at end of white space nearest
 * to global "gf_wrap_width" in length
 * It also supports margins, indents (inverse indenting, really) and
 * flowed text (ala RFC 3676)
 *
 */
void
gf_wrap(FILTER_S *f, int flg)
{
    register long i;
    GF_INIT(f, f->next);

    /*
     * f->f1    state
     * f->line  buffer where next "word" being considered is stored
     * f->f2    width in screen cells of f->line stuff
     * f->n     width in screen cells of the part of this line committed to next
     *            filter so far
     */

    if(flg == GF_DATA){
	register unsigned char c;
	register int state = f->f1;
	int width, full_character;

	while(GF_GETC(f, c)){

	    switch(state){
	      case CCR :				/* CRLF or CR in text ? */
		state = BOL;				/* either way, handle start */

		if(WRAP_FLOW(f)){
		    /* wrapped line? */
		    if(f->f2 == 0 && WRAP_SPC_LEN(f) && WRAP_TRL_SPC(f)){
			/*
			 * whack trailing space char, but be aware
			 * of embeds in space buffer.  grok them just
			 * in case they contain a 0x20 value
			 */
			if(WRAP_DELSP(f)){
			    char *sb, *sbp, *scp = NULL;
			    int   x;

			    for(sb = sbp = (char *)so_text(WRAP_SPACES(f)); *sbp; sbp++){
				switch(*sbp){
				  case ' ' :
				    scp = sbp;
				    break;

				  case TAG_EMBED :
				    sbp++;
				    switch (*sbp++){
				      case TAG_HANDLE :
					x = (int) *sbp++;
					if(strlen(sbp) >= x)
					  sbp += (x - 1);

					break;

				      case TAG_FGCOLOR :
				      case TAG_BGCOLOR :
					if(strlen(sbp) >= RGBLEN)
					  sbp += (RGBLEN - 1);

					break;

				      default :
					break;
				    }

				    break;

				  default :
				    break;
				}
			    }

			    /* replace space buf without trailing space char */
			    if(scp){
				STORE_S *ns = so_get(CharStar, NULL, EDIT_ACCESS);

				*scp++ = '\0';
				WRAP_SPC_LEN(f)--;
				WRAP_TRL_SPC(f) = 0;

				so_puts(ns, sb);
				so_puts(ns, scp);

				so_give(&WRAP_SPACES(f));
				WRAP_SPACES(f) = ns;
			    }
			}
		    }
		    else{				/* fixed line */
			WRAP_HARD(f) = 1;
			wrap_flush(f, &ip, &eib, &op, &eob);
			wrap_eol(f, 0, &ip, &eib, &op, &eob);

			/*
			 * When we get to a real end of line, we don't need to
			 * remember what the special color was anymore because
			 * we aren't going to be changing back to it. We unset it
			 * so that we don't keep resetting the color to normal.
			 */
			WRAP_COLOR_UNSET(f);
		    }

		    if(c == '\012'){			/* get c following LF */
		      break;
		    }
		    /* else c is first char of new line, fall thru */
		}
		else{
		    wrap_flush(f, &ip, &eib, &op, &eob);
		    wrap_eol(f, 0, &ip, &eib, &op, &eob);
		    WRAP_COLOR_UNSET(f);		/* see note above */
		    if(c == '\012'){
			break;
		    }
		    /* else fall thru to deal with beginning of line */
		}

	      case BOL :
		if(WRAP_FLOW(f)){
		    if(c == '>'){
			WRAP_FL_QC(f) = 1;		/* init it */
			state = FL_QLEV;		/* go collect it */
		    }
		    else {
			/* if EMBEDed, process it and return here */
			if(c == (unsigned char) TAG_EMBED){
			    WRAP_EMBED_PUTC(f, TAG_EMBED);
			    WRAP_STATE(f) = state;
			    state = TAG;
			    continue;
			}

			/* quote level change implies new paragraph */
			if(WRAP_FL_QD(f)){
			    WRAP_FL_QD(f) = 0;
			    if(WRAP_HARD(f) == 0){
				WRAP_HARD(f) = 1;
				wrap_flush(f, &ip, &eib, &op, &eob);
				wrap_eol(f, 0, &ip, &eib, &op, &eob);
				WRAP_COLOR_UNSET(f);	/* see note above */
			    }
			}

			if(WRAP_HARD(f)){
			    wrap_bol(f, 0, 1, &ip, &eib, &op,
				     &eob);   /* write quoting prefix */
			    WRAP_HARD(f) = 0;			    
			}

			switch (c) {
			  case '\015' :			/* a blank line? */
			    wrap_flush(f, &ip, &eib, &op, &eob);
			    state = CCR;		/* go collect it */
			    break;

			  case ' ' :			/* space stuffed */
			    state = FL_STF;		/* just eat it */
			    break;

			  case '-' :			/* possible sig-dash */
			    WRAP_FL_SIG(f) = 1;	        /* init state */
			    state = FL_SIG;		/* go collect it */
			    break;

			  default :
			    state = DFL;		/* go back to normal */
			    goto case_dfl;		/* handle c like DFL case */
			}
		    }
		}
		else{
		    state = DFL;
		    if(WRAP_COMMA(f) && c == TAB){
			wrap_bol(f, 1, 0, &ip, &eib, &op,
				 &eob);    /* convert to normal indent */
			break;
		    }

		    wrap_bol(f,0,0, &ip, &eib, &op, &eob);
		    goto case_dfl;			/* handle c like DFL case */
		}

		break;

	      case  FL_QLEV :
		if(c == '>'){				/* another level */
		    WRAP_FL_QC(f)++;
		}
		else {
		    /* if EMBEDed, process it and return here */
		    if(c == (unsigned char) TAG_EMBED){
			WRAP_EMBED_PUTC(f, TAG_EMBED);
			WRAP_STATE(f) = state;
			state = TAG;
			continue;
		    }

		    /* quote level change signals new paragraph */
		    if(WRAP_FL_QC(f) != WRAP_FL_QD(f)){
			WRAP_FL_QD(f) = WRAP_FL_QC(f);
			if(WRAP_HARD(f) == 0){		/* add hard newline */ 
			    WRAP_HARD(f) = 1;		/* hard newline */
			    wrap_flush(f, &ip, &eib, &op, &eob);
			    wrap_eol(f, 0, &ip, &eib, &op, &eob);
			    WRAP_COLOR_UNSET(f);	/* see note above */
			}
		    }

		    if(WRAP_HARD(f)){
			wrap_bol(f,0,1, &ip, &eib, &op, &eob);
			WRAP_HARD(f) = 0;
		    }

		    switch (c) {
		      case '\015' :			/* a blank line? */
			wrap_flush(f, &ip, &eib, &op, &eob);
			state = CCR;			/* go collect it */
			break;

		      case ' ' :			/* space-stuffed! */
			state = FL_STF;			/* just eat it */
			break;

		      case '-' :			/* sig dash? */
			WRAP_FL_SIG(f) = 1;
			state = FL_SIG;
			break;

		      default :				/* something else */
			state = DFL;
			goto case_dfl;			/* handle c like DFL */
		    }
		}

		break;

	      case FL_STF :				/* space stuffed */
		switch (c) {
		  case '\015' :				/* a blank line? */
		    wrap_flush(f, &ip, &eib, &op, &eob);
		    state = CCR;			/* go collect it */
		    break;

		  case (unsigned char) TAG_EMBED :	/* process TAG data */
		    WRAP_EMBED_PUTC(f, TAG_EMBED);
		    WRAP_STATE(f) = state;		/* and return */
		    state = TAG;
		    continue;

		  case '-' :				/* sig dash? */
		    WRAP_FL_SIG(f) = 1;
		    WRAP_ALLWSP(f) = 0;
		    state = FL_SIG;
		    break;

		  default :				/* something else */
		    state = DFL;
		    goto case_dfl;			/* handle c like DFL */
		}

		break;

	      case FL_SIG :				/* sig-dash collector */
		switch (WRAP_FL_SIG(f)){		/* possible sig-dash? */
		  case 1 :
		    if(c != '-'){			/* not a sigdash */
			if((f->n + WRAP_SPC_LEN(f) + 1) > WRAP_COL(f)){
			    wrap_flush_embed(f, &ip, &eib, &op,
					     &eob);      /* note any embedded*/
			    wrap_eol(f, 1, &ip, &eib,
				     &op, &eob);       /* plunk down newline */
			    wrap_bol(f, 1, 1, &ip, &eib,
				     &op, &eob);         /* write any prefix */
			}

			WRAP_PUTC(f,'-', 1);		/* write what we got */

			WRAP_FL_SIG(f) = 0;
			state = DFL;
			goto case_dfl;
		    }

		    /* don't put anything yet until we know to wrap or not */
		    WRAP_FL_SIG(f) = 2;
		    break;

		  case 2 :
		    if(c != ' '){			    /* not a sigdash */
			WRAP_PUTC(f, '-', 1);
			if((f->n + WRAP_SPC_LEN(f) + 2) > WRAP_COL(f)){
			    wrap_flush_embed(f, &ip, &eib, &op,
					     &eob);      /* note any embedded*/
			    wrap_eol(f, 1, &ip, &eib,
				     &op, &eob);       /* plunk down newline */
			    wrap_bol(f, 1, 1, &ip, &eib, &op, 
				     &eob);   	         /* write any prefix */
			}

			WRAP_PUTC(f,'-', 1);		/* write what we got */

			WRAP_FL_SIG(f) = 0;
			state = DFL;
			goto case_dfl;
		    }

		    /* don't put anything yet until we know to wrap or not */
		    WRAP_FL_SIG(f) = 3;
		    break;

		  case 3 :
		    if(c == '\015'){			/* success! */
			/* known sigdash, newline if soft nl */
			if(WRAP_SPC_LEN(f)){
			    wrap_flush(f, &ip, &eib, &op, &eob);
			    wrap_eol(f, 0, &ip, &eib, &op, &eob);
			    wrap_bol(f, 0, 1, &ip, &eib, &op, &eob);
			}
			WRAP_PUTC(f,'-',1);
			WRAP_PUTC(f,'-',1);
			WRAP_PUTC(f,' ',1);

			state = CCR;
			break;
		    }
		    else{
			WRAP_FL_SIG(f) = 4;		/* possible success */
		    }

		  case 4 :
		    switch(c){
		      case (unsigned char) TAG_EMBED :
			/*
			 * At this point we're almost 100% sure that we've got
			 * a sigdash.  Putc it (adding newline if previous
			 * was a soft nl) so we get it the right color
			 * before we store this new embedded stuff
			 */
			if(WRAP_SPC_LEN(f)){
			    wrap_flush(f, &ip, &eib, &op, &eob);
			    wrap_eol(f, 0, &ip, &eib, &op, &eob);
			    wrap_bol(f, 0, 1, &ip, &eib, &op, &eob);
			}
			WRAP_PUTC(f,'-',1);
			WRAP_PUTC(f,'-',1);
			WRAP_PUTC(f,' ',1);

			WRAP_FL_SIG(f) = 5;
			break;

		      case '\015' :			/* success! */
			/* 
			 * We shouldn't get here, but in case we do, we have
			 * not yet put the sigdash
			 */
			if(WRAP_SPC_LEN(f)){
			    wrap_flush(f, &ip, &eib, &op, &eob);
			    wrap_eol(f, 0, &ip, &eib, &op, &eob);
			    wrap_bol(f, 0, 1, &ip, &eib, &op, &eob);
			}
			WRAP_PUTC(f,'-',1);
			WRAP_PUTC(f,'-',1);
			WRAP_PUTC(f,' ',1);

			state = CCR;
			break;

		      default :				/* that's no sigdash! */
			/* write what we got but didn't put yet */
			WRAP_PUTC(f,'-', 1);
			WRAP_PUTC(f,'-', 1);
			WRAP_PUTC(f,' ', 1);

			WRAP_FL_SIG(f) = 0;
			wrap_flush(f, &ip, &eib, &op, &eob);
			WRAP_SPC_LEN(f) = 1;
			state = DFL;			/* set normal state */
			goto case_dfl;			/* and go do "c" */
		    }

		    break;

		  case 5 :
		    WRAP_STATE(f) = FL_SIG;		/* come back here */
		    WRAP_FL_SIG(f) = 6;			/* and seek EOL */
		    WRAP_EMBED_PUTC(f, TAG_EMBED);
		    state = TAG;			/* process embed */
		    goto case_tag;

		  case 6 :
		    /* 
		     * at this point we've already putc the sigdash in case 4
		     */
		    switch(c){
		      case (unsigned char) TAG_EMBED :
			WRAP_FL_SIG(f) = 5;
			break;

		      case '\015' :			/* success! */
			state = CCR;
			break;

		      default :				/* that's no sigdash! */
			/* 
			 * probably never reached (fake sigdash with embedded
			 * stuff) but if this did get reached, then we
			 * might have accidentally disobeyed a soft nl
			 */
			WRAP_FL_SIG(f) = 0;
			wrap_flush(f, &ip, &eib, &op, &eob);
			WRAP_SPC_LEN(f) = 1;
			state = DFL;			/* set normal state */
			goto case_dfl;			/* and go do "c" */
		    }

		    break;


		  default :
		    dprint((2, "-- gf_wrap: BROKEN FLOW STATE: %d\n",
			       WRAP_FL_SIG(f)));
		    WRAP_FL_SIG(f) = 0;
		    state = DFL;			/* set normal state */
		    goto case_dfl;			/* and go process "c" */
		}

		break;

	      case_dfl :
	      case DFL :
    /*
     * This was just if(WRAP_SPEC(f, c)) before the change to add
     * the == 0 test. This isn't quite right, either. We should really
     * be looking for special characters in the UCS characters, not
     * in the incoming stream of UTF-8. It is not right to
     * call this on bytes that are in the middle of a UTF-8 character,
     * hence the == 0 test which restricts it to the first byte
     * of a character. This isn't right, either, but it's closer.
     * Also change the definition of WRAP_SPEC so that isspace only
     * matches ascii characters, which will never be in the middle
     * of a UTF-8 multi-byte character.
     */
		if((WRAP_UTF8BUFP(f) - &WRAP_UTF8BUF(f, 0)) == 0 && WRAP_SPEC(f, c)){
		    WRAP_SAW_SOFT_HYPHEN(f) = 0;
		    switch(c){
		      default :
			if(WRAP_QUOTED(f))
			  break;

			if(f->f2){			/* any non-lwsp to flush? */
			    if(WRAP_COMMA(f)){
				/* remember our second best break point */
				WRAP_PB_OFF(f) = f->linep - f->line;
				WRAP_PB_LEN(f) = f->f2;
				break;
			    }
			    else
			      wrap_flush(f, &ip, &eib, &op, &eob);
			}

			switch(c){			/* remember separator */
			  case ' ' :
			    WRAP_SPC_LEN(f)++;
			    WRAP_TRL_SPC(f) = 1;
			    so_writec(' ',WRAP_SPACES(f));
			    break;

			  case TAB :
			  {
			      int i = (int) f->n + WRAP_SPC_LEN(f);

			      do
				WRAP_SPC_LEN(f)++;
			      while(++i & 0x07);

			      so_writec(TAB,WRAP_SPACES(f));
			      WRAP_TRL_SPC(f) = 0;
			  }

			  break;

			  default :			/* some control char? */
			    WRAP_SPC_LEN(f) += 2;
			    WRAP_TRL_SPC(f) = 0;
			    break;
			}

			continue;

		      case '\"' :
			WRAP_QUOTED(f) = !WRAP_QUOTED(f);
			break;

		      case '\015' :			/* already has newline? */
			state = CCR;
			continue;

		      case '\012' :			 /* bare LF in text? */
			wrap_flush(f, &ip, &eib, &op, &eob); /* they must've */
			wrap_eol(f, 0, &ip, &eib, &op, &eob);       /* meant */
			wrap_bol(f,1,1, &ip, &eib, &op, &eob); /* newline... */
			continue;

		      case (unsigned char) TAG_EMBED :
			WRAP_EMBED_PUTC(f, TAG_EMBED);
			WRAP_STATE(f) = state;
			state = TAG;
			continue;

		      case ',' :
			if(!WRAP_QUOTED(f)){
			    /* handle this special case in general code below */
			    if(f->n + WRAP_SPC_LEN(f) + f->f2 + 1 > WRAP_MAX_COL(f)
			       && WRAP_ALLWSP(f) && WRAP_PB_OFF(f))
			      break;

			    if(f->n + WRAP_SPC_LEN(f) + f->f2 + 1 > WRAP_COL(f)){
				if(WRAP_ALLWSP(f))    /* if anything visible */
				  wrap_flush(f, &ip, &eib, &op,
					     &eob);  /* ... blat buf'd chars */

				wrap_eol(f, 1, &ip, &eib, &op,
					 &eob);  /* plunk down newline */
				wrap_bol(f, 1, 1, &ip, &eib, &op,
					 &eob);    /* write any prefix */
			    }

			    WRAP_PUTC(f, ',', 1);	/* put out comma */
			    wrap_flush(f, &ip, &eib, &op,
				       &eob);       /* write buf'd chars */
			    continue;
			}

			break;
		    }
		}
		else if(WRAP_HANDLE_SOFT_HYPHEN(f)
			&& (WRAP_UTF8BUFP(f) - &WRAP_UTF8BUF(f, 0)) == 1
			&& WRAP_UTF8BUF(f, 0) == 0xC2 && c == 0xAD){
		    /*
		     * This is a soft hyphen. If there is enough space for
		     * a real hyphen to fit on the line here then we can
		     * flush everything up to before the soft hyphen,
		     * and simply remember that we saw a soft hyphen.
		     * If it turns out that we can't fit the next piece in
		     * then wrap_eol will append a real hyphen to the line.
		     * If we can fit another piece in it will be because we've
		     * reached the next break point. At that point we'll flush
		     * everything but won't include the unneeded hyphen. We erase
		     * the fact that we saw this soft hyphen because it have
		     * become irrelevant.
		     *
		     * If the hyphen is the character that puts us over the edge
		     * we go through the else case.
		     */

		    /* erase this soft hyphen character from buffer */
		    WRAP_UTF8BUFP(f) = &WRAP_UTF8BUF(f, 0);

		    if((f->n + WRAP_SPC_LEN(f) + f->f2 + 1) <= WRAP_COL(f)){
			if(f->f2)			/* any non-lwsp to flush? */
			  wrap_flush(f, &ip, &eib, &op, &eob);

			/* remember that we saw the soft hyphen */
			WRAP_SAW_SOFT_HYPHEN(f) = 1;
		    }
		    else{
			/*
			 * Everything up to the hyphen fits, otherwise it
			 * would have already been flushed the last time
			 * through the loop. But the hyphen won't fit. So
			 * we need to go back to the last line break and
			 * break there instead. Then start a new line with
			 * the buffered up characters and the soft hyphen.
			 */
			wrap_flush_embed(f, &ip, &eib, &op, &eob);
			wrap_eol(f, 1, &ip, &eib, &op,
				 &eob);	    /* plunk down newline */
			wrap_bol(f,1,1, &ip, &eib, &op,
				 &eob);	      /* write any prefix */

			/*
			 * Now we're in the same situation as we would have
			 * been above except we're on a new line. Try to
			 * flush out the characters seen up to the hyphen.
			 */
			if((f->n + WRAP_SPC_LEN(f) + f->f2 + 1) <= WRAP_COL(f)){
			    if(f->f2)			/* any non-lwsp to flush? */
			      wrap_flush(f, &ip, &eib, &op, &eob);

			    /* remember that we saw the soft hyphen */
			    WRAP_SAW_SOFT_HYPHEN(f) = 1;
			}
			else
			  WRAP_SAW_SOFT_HYPHEN(f) = 0;
		    }

		    continue;
		}

		full_character = 0;

		{
		    unsigned char *inputp;
		    unsigned long remaining_octets;
		    UCS ucs;

		    if(WRAP_UTF8BUFP(f) < &WRAP_UTF8BUF(f, 0) + 6){	/* always true */

			*WRAP_UTF8BUFP(f)++ = c;
			remaining_octets = WRAP_UTF8BUFP(f) - &WRAP_UTF8BUF(f, 0);
			if(remaining_octets == 1 && isascii(WRAP_UTF8BUF(f, 0))){
			    full_character++;
			    if(c == TAB){
				int i = (int) f->n;

				while(i & 0x07)
				  i++;

				width = i - f->n;
			    }
			    else if(c < 0x80 && iscntrl((unsigned char) c))
			      width = 2;
			    else
			      width = 1;
			}
			else{
			    inputp = &WRAP_UTF8BUF(f, 0);
			    ucs = (UCS) utf8_get(&inputp, &remaining_octets);
			    switch(ucs){
			      case U8G_ENDSTRG:	/* incomplete character, wait */
			      case U8G_ENDSTRI:	/* incomplete character, wait */
				width = 0;
				break;

			      default:
			        if(ucs & U8G_ERROR || ucs == UBOGON){
				    /*
				     * None of these cases is supposed to happen. If it
				     * does happen then the input stream isn't UTF-8
				     * so something is wrong. Writechar will treat
				     * each octet in the input buffer as a separate
				     * error character and print a '?' for each,
				     * so the width will be the number of octets.
				     */
				    width = WRAP_UTF8BUFP(f) - &WRAP_UTF8BUF(f, 0);
				    full_character++;
				}
				else{
				    /* got a character */
				    width = wcellwidth(ucs);
				    full_character++;

				    if(width < 0){
					/*
					 * This happens when we have a UTF-8 character that
					 * we aren't able to print in our locale. For example,
					 * if the locale is setup with the terminal
					 * expecting ISO-8859-1 characters then there are
					 * lots of UTF-8 characters that can't be printed.
					 * Print a '?' instead.
					 */
					width = 1;
				    }
				}

				break;
			    }
			}
		    }
		    else{
			/*
			 * This cannot happen because an error would have
			 * happened at least by character #6. So if we get
			 * here there is a bug in utf8_get().
			 */
			if(WRAP_UTF8BUFP(f) == &WRAP_UTF8BUF(f, 0) + 6){
			    *WRAP_UTF8BUFP(f)++ = c;
			}

			/*
			 * We could possibly do some more sophisticated
			 * resynchronization here, but we aren't doing
			 * anything in Writechar so it wouldn't match up
			 * with that anyway. Just figure each character will
			 * end up being printed as a ? character.
			 */
			width = WRAP_UTF8BUFP(f) - &WRAP_UTF8BUF(f, 0);
			full_character++;
		    }
		}

		if(WRAP_ALLWSP(f)){
		    /*
		     * Nothing is visible yet but the first word may be too long
		     * all by itself. We need to break early.
		     */
		    if(f->n + WRAP_SPC_LEN(f) + f->f2 + width > WRAP_MAX_COL(f)){
			/*
			 * A little reaching behind the curtain here.
			 * if there's at least a preferable break point, use
			 * it and stuff what's left back into the wrap buffer.
			 * The "nwsp" latch is used to skip leading whitespace
			 * The second half of the test prevents us from wrapping
			 * at the preferred break point in the case that it
			 * is so early in the line that it doesn't help.
			 * That is, the width of the indent is even more than
			 * the width of the first part before the preferred
			 * break point. An example would be breaking after
			 * "To:" when the indent is 4 which is > 3.
			 */
			if(WRAP_PB_OFF(f) && WRAP_PB_LEN(f) >= WRAP_INDENT(f)){
			    char *p1 = f->line + WRAP_PB_OFF(f);
			    char *p2 = f->linep;
			    char  c2;
			    int   nwsp = 0, left_after_wrap;

			    left_after_wrap = f->f2 - WRAP_PB_LEN(f);

			    f->f2 = WRAP_PB_LEN(f);
			    f->linep = p1;

			    wrap_flush(f, &ip, &eib, &op, &eob); /* flush shortened buf */
			    
			    /* put back rest of characters */
			    while(p1 < p2){
				c2 = *p1++;
				if(!(c2 == ' ' || c2 == '\t') || nwsp){
				    WRAP_PUTC(f, c2, 0);
				    nwsp = 1;
				}
				else
				  left_after_wrap--;	/* wrong if a tab! */
			    }

			    f->f2 = MAX(left_after_wrap, 0);

			    wrap_eol(f, 1, &ip, &eib, &op,
				     &eob);     /* plunk down newline */
			    wrap_bol(f,1,1, &ip, &eib, &op,
				     &eob);      /* write any prefix */

			    /*
			     * What's this for?
			     * If we do the less preferable break point at
			     * the space we don't want to lose the fact that
			     * we might be able to break at this comma for
			     * the next one.
			     */
			    if(full_character && c == ','){
				WRAP_PUTC(f, c, 1);
				wrap_flush(f, &ip, &eib, &op, &eob);
				WRAP_UTF8BUFP(f) = &WRAP_UTF8BUF(f, 0);
			    }
			}
			else{
			    wrap_flush(f, &ip, &eib, &op, &eob);

			    wrap_eol(f, 1, &ip, &eib, &op,
				     &eob);     /* plunk down newline */
			    wrap_bol(f,1,1, &ip, &eib, &op,
				     &eob);      /* write any prefix */
			}
		    }
		}
		else if((f->n + WRAP_SPC_LEN(f) + f->f2 + width) > WRAP_COL(f)){
		    wrap_flush_embed(f, &ip, &eib, &op, &eob);
		    wrap_eol(f, 1, &ip, &eib, &op,
			     &eob);	    /* plunk down newline */
		    wrap_bol(f,1,1, &ip, &eib, &op,
			     &eob);	      /* write any prefix */
		}

		/*
		 * Commit entire multibyte UTF-8 character at once
		 * instead of writing partial characters into the
		 * buffer.
		 */
		if(full_character){
		    unsigned char *q;

		    for(q = &WRAP_UTF8BUF(f, 0); q < WRAP_UTF8BUFP(f); q++){
			WRAP_PUTC(f, *q, width);
			width = 0;
		    }

		    WRAP_UTF8BUFP(f) = &WRAP_UTF8BUF(f, 0);
		}

		break;

	      case_tag :
	      case TAG :
		WRAP_EMBED_PUTC(f, c);
		switch(c){
		  case TAG_HANDLE :
		    WRAP_EMBED(f) = -1;
		    state = HANDLE;
		    break;

		  case TAG_FGCOLOR :
		  case TAG_BGCOLOR :
		    WRAP_EMBED(f) = RGBLEN;
		    state = HDATA;
		    break;

		  default :
		    state = WRAP_STATE(f);
		    break;
		}

		break;

	      case HANDLE :
		WRAP_EMBED_PUTC(f, c);
		WRAP_EMBED(f) = c;
		state = HDATA;
		break;

	      case HDATA :
		if(f->f2){
		  WRAP_PUTC(f, c, 0);
		}
		else
		  so_writec(c, WRAP_SPACES(f));

		if(!(WRAP_EMBED(f) -= 1)){
		    state = WRAP_STATE(f);
		}

		break;
	    }
	}

	f->f1 = state;
	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	wrap_flush(f, &ip, &eib, &op, &eob);
	if(WRAP_COLOR(f))
	  free_color_pair(&WRAP_COLOR(f));

	fs_give((void **) &f->line);	/* free temp line buffer */
	so_give(&WRAP_SPACES(f));
	fs_give((void **) &f->opt);	/* free wrap widths struct */
	(void) GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint((9, "-- gf_reset wrap\n"));
	f->f1    = BOL;
	f->n     = 0L;		/* displayed length of line so far */
	f->f2	 = 0;		/* displayed length of buffered chars */
	WRAP_HARD(f) = 1;	/* starting at beginning of line */
	if(! (WRAP_S *) f->opt)
	  f->opt = gf_wrap_filter_opt(75, 80, NULL, 0, 0);

	while(WRAP_INDENT(f) >= WRAP_MAX_COL(f))
	  WRAP_INDENT(f) /= 2;

	f->line  = (char *) fs_get(WRAP_MAX_COL(f) * sizeof(char));
	f->linep = f->line;
	WRAP_LASTC(f) = &f->line[WRAP_MAX_COL(f) - 1];

	for(i = 0; i < 256; i++)
	  ((WRAP_S *) f->opt)->special[i] = ((i == '\"' && WRAP_COMMA(f))
					     || i == '\015'
					     || i == '\012'
					     || (i == (unsigned char) TAG_EMBED
						 && WRAP_TAGS(f))
					     || (i == ',' && WRAP_COMMA(f)
						 && !WRAP_QUOTED(f))
					     || ASCII_ISSPACE(i));
	WRAP_SPACES(f) = so_get(CharStar, NULL, EDIT_ACCESS);
	WRAP_UTF8BUFP(f) = &WRAP_UTF8BUF(f, 0);
    }
}

int
wrap_flush(FILTER_S *f, unsigned char **ipp, unsigned char **eibp,
	   unsigned char **opp, unsigned char **eobp)
{
    register char *s;
    register int   n;

    s = (char *)so_text(WRAP_SPACES(f));
    n = so_tell(WRAP_SPACES(f));
    so_seek(WRAP_SPACES(f), 0L, 0);
    wrap_flush_s(f, s, n, WRAP_SPC_LEN(f), ipp, eibp, opp, eobp, WFE_NONE);
    so_truncate(WRAP_SPACES(f), 0L);
    WRAP_SPC_LEN(f) = 0;
    WRAP_TRL_SPC(f) = 0;
    s = f->line;
    n = f->linep - f->line;
    wrap_flush_s(f, s, n, f->f2, ipp, eibp, opp, eobp, WFE_NONE);
    f->f2    = 0;
    f->linep = f->line;
    WRAP_PB_OFF(f) = 0;
    WRAP_PB_LEN(f) = 0;

    return 0;
}

int
wrap_flush_embed(FILTER_S *f, unsigned char **ipp, unsigned char **eibp, unsigned char **opp, unsigned char **eobp)
{
  register char *s;
  register int   n;
  s = (char *)so_text(WRAP_SPACES(f));
  n = so_tell(WRAP_SPACES(f));
  so_seek(WRAP_SPACES(f), 0L, 0);
  wrap_flush_s(f, s, n, 0, ipp, eibp, opp, eobp, WFE_CNT_HANDLE);
  so_truncate(WRAP_SPACES(f), 0L);
  WRAP_SPC_LEN(f) = 0;
  WRAP_TRL_SPC(f) = 0;

  return 0;
}

int
wrap_flush_s(FILTER_S *f, char *s, int n, int w, unsigned char **ipp,
	     unsigned char **eibp, unsigned char **opp, unsigned char **eobp, int flags)
{
    f->n += w;

    for(; n > 0; n--,s++){
	if(*s == TAG_EMBED){
	    if(n-- > 0){
		switch(*++s){
		  case TAG_BOLDON :
		    GF_PUTC_GLO(f->next,TAG_EMBED);
		    GF_PUTC_GLO(f->next,TAG_BOLDON);
		    WRAP_BOLD(f) = 1;
		    break;
		  case TAG_BOLDOFF :
		    GF_PUTC_GLO(f->next,TAG_EMBED);
		    GF_PUTC_GLO(f->next,TAG_BOLDOFF);
		    WRAP_BOLD(f) = 0;
		    break;
		  case TAG_ULINEON :
		    GF_PUTC_GLO(f->next,TAG_EMBED);
		    GF_PUTC_GLO(f->next,TAG_ULINEON);
		    WRAP_ULINE(f) = 1;
		    break;
		  case TAG_ULINEOFF :
		    GF_PUTC_GLO(f->next,TAG_EMBED);
		    GF_PUTC_GLO(f->next,TAG_ULINEOFF);
		    WRAP_ULINE(f) = 0;
		    break;
		  case TAG_INVOFF :
		    GF_PUTC_GLO(f->next,TAG_EMBED);
		    GF_PUTC_GLO(f->next,TAG_INVOFF);
		    WRAP_ANCHOR(f) = 0;
		    break;
		  case TAG_HANDLE :
		    if((flags & WFE_CNT_HANDLE) == 0)
		      GF_PUTC_GLO(f->next,TAG_EMBED);

		    if(n-- > 0){
			int i = *++s;

			if((flags & WFE_CNT_HANDLE) == 0)
			  GF_PUTC_GLO(f->next, TAG_HANDLE);

			if(i <= n){
			    n -= i;

			    if((flags & WFE_CNT_HANDLE) == 0)
			      GF_PUTC_GLO(f->next, i);

			    WRAP_ANCHOR(f) = 0;
			    while(i-- > 0){
				WRAP_ANCHOR(f) = (WRAP_ANCHOR(f) * 10) + (*++s-'0');

				if((flags & WFE_CNT_HANDLE) == 0)
				  GF_PUTC_GLO(f->next,*s);
			    }

			}
		    }
		    break;
		  case TAG_FGCOLOR :
		    if(pico_usingcolor() && n >= RGBLEN){
			int i;
			GF_PUTC_GLO(f->next,TAG_EMBED);
			GF_PUTC_GLO(f->next,TAG_FGCOLOR);
			if(!WRAP_COLOR(f))
			  WRAP_COLOR(f)=new_color_pair(NULL,NULL);
			strncpy(WRAP_COLOR(f)->fg, s+1, RGBLEN);
			WRAP_COLOR(f)->fg[RGBLEN]='\0';
			i = RGBLEN;
			n -= i;
			while(i-- > 0)
			  GF_PUTC_GLO(f->next,
				  (*++s) & 0xff);
		    }
		    break;
		  case TAG_BGCOLOR :
		    if(pico_usingcolor() && n >= RGBLEN){
			int i;
			GF_PUTC_GLO(f->next,TAG_EMBED);
			GF_PUTC_GLO(f->next,TAG_BGCOLOR);
			if(!WRAP_COLOR(f))
			  WRAP_COLOR(f)=new_color_pair(NULL,NULL);
			strncpy(WRAP_COLOR(f)->bg, s+1, RGBLEN);
			WRAP_COLOR(f)->bg[RGBLEN]='\0';
			i = RGBLEN;
			n -= i;
			while(i-- > 0)
			  GF_PUTC_GLO(f->next,
				  (*++s) & 0xff);
		    }
		    break;
		  default :
		    break;
		}
	    }
	}
	else if(w){

	    if(f->n <= WRAP_MAX_COL(f)){
		GF_PUTC_GLO(f->next, (*s) & 0xff);
	    }
	    else{
		dprint((2, "-- gf_wrap: OVERRUN: %c\n", (*s) & 0xff));
	    }

	    WRAP_ALLWSP(f) = 0;
	}
    }

    return 0;
}

int
wrap_eol(FILTER_S *f, int c, unsigned char **ipp, unsigned char **eibp,
	 unsigned char **opp, unsigned char **eobp)
{
    if(WRAP_SAW_SOFT_HYPHEN(f)){
	WRAP_SAW_SOFT_HYPHEN(f) = 0;
	GF_PUTC_GLO(f->next, '-');	/* real hyphen */
    }

    if(c && WRAP_LV_FLD(f))
      GF_PUTC_GLO(f->next, ' ');

    if(WRAP_BOLD(f)){
	GF_PUTC_GLO(f->next, TAG_EMBED);
	GF_PUTC_GLO(f->next, TAG_BOLDOFF);
    }

    if(WRAP_ULINE(f)){
	GF_PUTC_GLO(f->next, TAG_EMBED);
	GF_PUTC_GLO(f->next, TAG_ULINEOFF);
    }

    if(WRAP_INVERSE(f) || WRAP_ANCHOR(f)){
	GF_PUTC_GLO(f->next, TAG_EMBED);
	GF_PUTC_GLO(f->next, TAG_INVOFF);
    }

    if(WRAP_COLOR_SET(f)){
	char *p;
	char  cb[RGBLEN+1];
	GF_PUTC_GLO(f->next, TAG_EMBED);
	GF_PUTC_GLO(f->next, TAG_FGCOLOR);
	strncpy(cb, color_to_asciirgb(ps_global->VAR_NORM_FORE_COLOR), sizeof(cb));
	cb[sizeof(cb)-1] = '\0';
	p = cb;
	for(; *p; p++)
	  GF_PUTC_GLO(f->next, *p);
	GF_PUTC_GLO(f->next, TAG_EMBED);
	GF_PUTC_GLO(f->next, TAG_BGCOLOR);
	strncpy(cb, color_to_asciirgb(ps_global->VAR_NORM_BACK_COLOR), sizeof(cb));
	cb[sizeof(cb)-1] = '\0';
	p = cb;
	for(; *p; p++)
	  GF_PUTC_GLO(f->next, *p);
    }

    GF_PUTC_GLO(f->next, '\015');
    GF_PUTC_GLO(f->next, '\012');
    f->n = 0L;
    so_truncate(WRAP_SPACES(f), 0L);
    WRAP_SPC_LEN(f) = 0;
    WRAP_TRL_SPC(f) = 0;

    return 0;
}

int
wrap_bol(FILTER_S *f, int ivar, int q, unsigned char **ipp, unsigned char **eibp,
	 unsigned char **opp, unsigned char **eobp)
{
    int n = WRAP_MARG_L(f) + (ivar ? WRAP_INDENT(f) : 0);

    if(WRAP_HDR_CLR(f)){
	char *p;
	char cbuf[RGBLEN+1];
	int k;

	if((k = WRAP_MARG_L(f)) > 0)
	  while(k-- > 0){
	      n--;
	      f->n++;
	      GF_PUTC_GLO(f->next, ' ');
	  }

	GF_PUTC_GLO(f->next, TAG_EMBED);
	GF_PUTC_GLO(f->next, TAG_FGCOLOR);
	strncpy(cbuf,
		color_to_asciirgb(ps_global->VAR_HEADER_GENERAL_FORE_COLOR),
		sizeof(cbuf));
	cbuf[sizeof(cbuf)-1] = '\0';
	p = cbuf;
	for(; *p; p++)
	  GF_PUTC_GLO(f->next, *p);
	GF_PUTC_GLO(f->next, TAG_EMBED);
	GF_PUTC_GLO(f->next, TAG_BGCOLOR);
	strncpy(cbuf,
		color_to_asciirgb(ps_global->VAR_HEADER_GENERAL_BACK_COLOR),
		sizeof(cbuf));
	cbuf[sizeof(cbuf)-1] = '\0';
	p = cbuf;
	for(; *p; p++)
	  GF_PUTC_GLO(f->next, *p);
    }

    while(n-- > 0){
	f->n++;
	GF_PUTC_GLO(f->next, ' ');
    }

    WRAP_ALLWSP(f) = 1;

    if(q)
      wrap_quote_insert(f, ipp, eibp, opp, eobp);

    if(WRAP_BOLD(f)){
	GF_PUTC_GLO(f->next, TAG_EMBED);
	GF_PUTC_GLO(f->next, TAG_BOLDON);
    }
    if(WRAP_ULINE(f)){
	GF_PUTC_GLO(f->next, TAG_EMBED);
	GF_PUTC_GLO(f->next, TAG_ULINEON);
    }
    if(WRAP_INVERSE(f)){
	GF_PUTC_GLO(f->next, TAG_EMBED);
	GF_PUTC_GLO(f->next, TAG_INVON);
    }
    if(WRAP_COLOR_SET(f)){
	char *p;
	if(WRAP_COLOR(f)->fg[0]){
	    char cb[RGBLEN+1];
	    GF_PUTC_GLO(f->next, TAG_EMBED);
	    GF_PUTC_GLO(f->next, TAG_FGCOLOR);
	    strncpy(cb, color_to_asciirgb(WRAP_COLOR(f)->fg), sizeof(cb));
	    cb[sizeof(cb)-1] = '\0';
	    p = cb;
	    for(; *p; p++)
	      GF_PUTC_GLO(f->next, *p);
	}
	if(WRAP_COLOR(f)->bg[0]){
	    char cb[RGBLEN+1];
	    GF_PUTC_GLO(f->next, TAG_EMBED);
	    GF_PUTC_GLO(f->next, TAG_BGCOLOR);
	    strncpy(cb, color_to_asciirgb(WRAP_COLOR(f)->bg), sizeof(cb));
	    cb[sizeof(cb)-1] = '\0';
	    p = cb;
	    for(; *p; p++)
	      GF_PUTC_GLO(f->next, *p);
	}
    }
    if(WRAP_ANCHOR(f)){
	char buf[64]; int i;
	GF_PUTC_GLO(f->next, TAG_EMBED);
	GF_PUTC_GLO(f->next, TAG_HANDLE);
	snprintf(buf, sizeof(buf), "%d", WRAP_ANCHOR(f));
	GF_PUTC_GLO(f->next, (int) strlen(buf));
	for(i = 0; buf[i]; i++)
	  GF_PUTC_GLO(f->next, buf[i]);
    }

    return 0;
}

int
wrap_quote_insert(FILTER_S *f, unsigned char **ipp, unsigned char **eibp,
		  unsigned char **opp, unsigned char **eobp)
{
    int j, i;
    COLOR_PAIR *col = NULL;
    char *prefix = NULL, *last_prefix = NULL;

    if(ps_global->VAR_QUOTE_REPLACE_STRING){
	get_pair(ps_global->VAR_QUOTE_REPLACE_STRING, &prefix, &last_prefix, 0, 0);
	if(!prefix && last_prefix){
	    prefix = last_prefix;
	    last_prefix = NULL;
	}
    }

    for(j = 0; j < WRAP_FL_QD(f); j++){
	if(WRAP_USE_CLR(f)){
	    if((j % 3) == 0
	       && ps_global->VAR_QUOTE1_FORE_COLOR
	       && ps_global->VAR_QUOTE1_BACK_COLOR
	       && (col = new_color_pair(ps_global->VAR_QUOTE1_FORE_COLOR,
					ps_global->VAR_QUOTE1_BACK_COLOR))
	       && pico_is_good_colorpair(col)){
                GF_COLOR_PUTC(f, col);
            }
	    else if((j % 3) == 1
		    && ps_global->VAR_QUOTE2_FORE_COLOR
		    && ps_global->VAR_QUOTE2_BACK_COLOR
		    && (col = new_color_pair(ps_global->VAR_QUOTE2_FORE_COLOR,
					     ps_global->VAR_QUOTE2_BACK_COLOR))
		    && pico_is_good_colorpair(col)){
	        GF_COLOR_PUTC(f, col);
            }
	    else if((j % 3) == 2
		    && ps_global->VAR_QUOTE3_FORE_COLOR
		    && ps_global->VAR_QUOTE3_BACK_COLOR
		    && (col = new_color_pair(ps_global->VAR_QUOTE3_FORE_COLOR,
					     ps_global->VAR_QUOTE3_BACK_COLOR))
		    && pico_is_good_colorpair(col)){
	        GF_COLOR_PUTC(f, col);
            }
	    if(col){
		free_color_pair(&col);
		col = NULL;
	    }
	}

	if(!WRAP_LV_FLD(f)){
	    if(!WRAP_FOR_CMPS(f) && ps_global->VAR_QUOTE_REPLACE_STRING && prefix){
		for(i = 0; prefix[i]; i++)
		  GF_PUTC_GLO(f->next, prefix[i]);
		f->n += utf8_width(prefix);
	    }
	    else if(ps_global->VAR_REPLY_STRING
		    && (!strcmp(ps_global->VAR_REPLY_STRING, ">")
			|| !strcmp(ps_global->VAR_REPLY_STRING, "\">\""))){
		GF_PUTC_GLO(f->next, '>');
		f->n += 1;
	    }
	    else{
		GF_PUTC_GLO(f->next, '>');
		GF_PUTC_GLO(f->next, ' ');
		f->n += 2;
	    }
	}
	else{
	    GF_PUTC_GLO(f->next, '>');
	    f->n += 1;
	}
    }
    if(j && WRAP_LV_FLD(f)){
	GF_PUTC_GLO(f->next, ' ');
	f->n++;
    }
    else if(j && last_prefix){
	for(i = 0; last_prefix[i]; i++)
	  GF_PUTC_GLO(f->next, last_prefix[i]);
	f->n += utf8_width(last_prefix);	
    }

    if(prefix)
      fs_give((void **)&prefix);
    if(last_prefix)
      fs_give((void **)&last_prefix);

    return 0;
}


/*
 * function called from the outside to set
 * wrap filter's width option
 */
void *
gf_wrap_filter_opt(int width, int width_max, int *margin, int indent, int flags)
{
    WRAP_S *wrap;

    /* NOTE: variables MUST be sanity checked before they get here */
    wrap = (WRAP_S *) fs_get(sizeof(WRAP_S));
    memset(wrap, 0, sizeof(WRAP_S));
    wrap->wrap_col     = width;
    wrap->wrap_max     = width_max;
    wrap->indent       = indent;
    wrap->margin_l     = (margin) ? margin[0] : 0;
    wrap->margin_r     = (margin) ? margin[1] : 0;
    wrap->tags	       = (GFW_HANDLES & flags) == GFW_HANDLES;
    wrap->on_comma     = (GFW_ONCOMMA & flags) == GFW_ONCOMMA;
    wrap->flowed       = (GFW_FLOWED & flags) == GFW_FLOWED;
    wrap->leave_flowed = (GFW_FLOW_RESULT & flags) == GFW_FLOW_RESULT;
    wrap->delsp	       = (GFW_DELSP & flags) == GFW_DELSP;
    wrap->use_color    = (GFW_USECOLOR & flags) == GFW_USECOLOR;
    wrap->hdr_color    = (GFW_HDRCOLOR & flags) == GFW_HDRCOLOR;
    wrap->for_compose  = (GFW_FORCOMPOSE & flags) == GFW_FORCOMPOSE;
    wrap->handle_soft_hyphen = (GFW_SOFTHYPHEN & flags) == GFW_SOFTHYPHEN;

    return((void *) wrap);
}


void *
gf_url_hilite_opt(URL_HILITE_S *uh, HANDLE_S **handlesp, int flags)
{
    if(uh){
	memset(uh, 0, sizeof(URL_HILITE_S));
	uh->handlesp  = handlesp;
	uh->hdr_color = (URH_HDRCOLOR & flags) == URH_HDRCOLOR;
    }

    return((void *) uh);
}


#define	PF_QD(F)	(((PREFLOW_S *)(F)->opt)->quote_depth)
#define	PF_QC(F)	(((PREFLOW_S *)(F)->opt)->quote_count)
#define	PF_SIG(F)	(((PREFLOW_S *)(F)->opt)->sig)

typedef struct preflow_s {
    int		quote_depth,
		quote_count,
		sig;
} PREFLOW_S;

/*
 * This would normally be handled in gf_wrap. If there is a possibility
 * that a url we want to recognize is cut in half by a soft newline we
 * want to fix that up by putting the halves back together. We do that
 * by deleting the soft newline and putting it all in one line. It will
 * still get wrapped later in gf_wrap. It isn't pretty with all the
 * goto's, but whatta ya gonna do?
 */
void
gf_preflow(FILTER_S *f, int flg)
{
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;
	register int state  = f->f1;
	register int pending = f->f2;

	while(GF_GETC(f, c)){
	    switch(state){
	      case DFL:
default_case:
		switch(c){
		  case ' ':
		    state = WSPACE;
		    break;
		  
		  case '\015':
		    state = CCR;
		    break;
		  
		  default:
		    GF_PUTC(f->next, c);
		    break;
		}

	        break;

	      case CCR:
		switch(c){
		  case '\012':
		    pending = 1;
		    state = BOL;
		    break;
		  
		  default:
		    GF_PUTC(f->next, '\012');
		    state = DFL;
		    goto default_case;
		    break;
		}

	        break;

	      case WSPACE:
		switch(c){
		  case '\015':
		    state = SPACECR;
		    break;
		  
		  default:
		    GF_PUTC(f->next, ' ');
		    state = DFL;
		    goto default_case;
		    break;
		}

	        break;

	      case SPACECR:
		switch(c){
		  case '\012':
		    pending = 2;
		    state = BOL;
		    break;
		  
		  default:
		    GF_PUTC(f->next, ' ');
		    GF_PUTC(f->next, '\012');
		    state = DFL;
		    goto default_case;
		    break;
		}

	        break;

	      case BOL:
		PF_QC(f) = 0;
		if(c == '>'){		/* count quote level */
		    PF_QC(f)++;
		    state = FL_QLEV;
		}
		else{
done_counting_quotes:
		    if(c == ' '){	/* eat stuffed space */
			state = FL_STF;
			break;
		    }

done_with_stuffed_space:
		    if(c == '-'){	/* look for signature */
			PF_SIG(f) = 1;
			state = FL_SIG;
			break;
		    }

done_with_sig:
		    if(pending == 2){
			if(PF_QD(f) == PF_QC(f) && PF_SIG(f) < 4){
			    /* delete pending */

			    PF_QD(f) = PF_QC(f);

			    /* suppress quotes, too */
			    PF_QC(f) = 0;
			}
			else{
			    /*
			     * This should have been a hard new line
			     * instead so leave out the trailing space.
			     */
			    GF_PUTC(f->next, '\015');
			    GF_PUTC(f->next, '\012');

			    PF_QD(f) = PF_QC(f);
			}
		    }
		    else if(pending == 1){
			GF_PUTC(f->next, '\015');
			GF_PUTC(f->next, '\012');
			PF_QD(f) = PF_QC(f);
		    }
		    else{
			PF_QD(f) = PF_QC(f);
		    }

		    pending = 0;
		    state = DFL;
		    while(PF_QC(f)-- > 0)
		      GF_PUTC(f->next, '>');

		    switch(PF_SIG(f)){
		      case 0:
		      default:
		        break;

		      case 1:
			GF_PUTC(f->next, '-');
		        break;

		      case 2:
			GF_PUTC(f->next, '-');
			GF_PUTC(f->next, '-');
		        break;

		      case 3:
		      case 4:
			GF_PUTC(f->next, '-');
			GF_PUTC(f->next, '-');
			GF_PUTC(f->next, ' ');
		        break;
		    }

		    PF_SIG(f) = 0;
		    goto default_case;		/* to handle c */
		}

		break;

	      case FL_QLEV:		/* count quote level */
		if(c == '>')
		  PF_QC(f)++;
		else
		  goto done_counting_quotes;

		break;

	      case FL_STF:		/* eat stuffed space */
		goto done_with_stuffed_space;
	        break;

	      case FL_SIG:		/* deal with sig indicator */
		switch(PF_SIG(f)){
		  case 1:		/* saw '-' */
		    if(c == '-')
		      PF_SIG(f) = 2;
		    else
		      goto done_with_sig;

		    break;

		  case 2:		/* saw '--' */
		    if(c == ' ')
		      PF_SIG(f) = 3;
		    else
		      goto done_with_sig;

		    break;

		  case 3:		/* saw '-- ' */
		    if(c == '\015')
		      PF_SIG(f) = 4;	/* it really is a sig line */

		    goto done_with_sig;
		    break;
		}

	        break;
	    }
	}

	f->f1 = state;
	f->f2 = pending;
	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	fs_give((void **) &f->opt);
	(void) GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	PREFLOW_S *pf;

	pf = (PREFLOW_S *) fs_get(sizeof(*pf));
	memset(pf, 0, sizeof(*pf));
	f->opt = (void *) pf;

	f->f1     = BOL;	/* state */
	f->f2     = 0;		/* pending */
	PF_QD(f)  = 0;		/* quote depth */
	PF_QC(f)  = 0;		/* quote count */
	PF_SIG(f) = 0;		/* sig level */
    }
}




/*
 * LINE PREFIX FILTER - insert given text at beginning of each
 * line
 */


#define	GF_PREFIX_WRITE(s)	{ \
				    register char *p; \
				    if((p = (s)) != NULL) \
				      while(*p) \
					GF_PUTC(f->next, *p++); \
				}


/*
 * the simple filter, prepends each line with the requested prefix.
 * if prefix is null, does nothing, and as with all filters, assumes
 * NVT end of lines.
 */
void
gf_prefix(FILTER_S *f, int flg)
{
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;
	register int state = f->f1;
	register int first = f->f2;

	while(GF_GETC(f, c)){

	    if(first){				/* write initial prefix!! */
		first = 0;			/* but just once */
		GF_PREFIX_WRITE((char *) f->opt);
	    }

	    /*
	     * State == 0 is the starting state and the usual state.
	     * State == 1 means we saw a CR and haven't acted on it yet.
	     * We are looking for a LF to get the CRLF end of line.
	     * However, we also treat bare CR and bare LF as if they
	     * were CRLF sequences. What else could it mean in text?
	     * This filter is only used for text so that is probably
	     * a reasonable interpretation of the bad input.
	     */
	    if(c == '\015'){		/* CR */
		if(state){			/* Treat pending CR as endofline, */
		    GF_PUTC(f->next, '\015');	/* and remain in saw-a-CR state.  */
		    GF_PUTC(f->next, '\012');
		    GF_PREFIX_WRITE((char *) f->opt);
		}
		else{
		    state = 1;
		}
	    }
	    else if(c == '\012'){	/* LF */
		GF_PUTC(f->next, '\015');	/* Got either a CRLF or a bare LF, */
		GF_PUTC(f->next, '\012');	/* treat both as if a CRLF.    */
		GF_PREFIX_WRITE((char *) f->opt);
		state = 0;
	    }
	    else{			/* any other character */
		if(state){
		    GF_PUTC(f->next, '\015');	/* Treat pending CR as endofline. */
		    GF_PUTC(f->next, '\012');
		    GF_PREFIX_WRITE((char *) f->opt);
		    state = 0;
		}

		GF_PUTC(f->next, c);
	    }
	}

	f->f1 = state;			/* save state for next chunk of data */
	f->f2 = first;
	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	(void) GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint((9, "-- gf_reset prefix\n"));
	f->f1   = 0;
	f->f2   = 1;			/* nothing written yet */
    }
}


/*
 * function called from the outside to set
 * prefix filter's prefix string
 */
void *
gf_prefix_opt(char *prefix)
{
    return((void *) prefix);
}


/*
 * LINE TEST FILTER - accumulate lines and offer each to the provided
 * test function.
 */

typedef struct _linetest_s {
    linetest_t	f;
    void       *local;
} LINETEST_S;


/* accumulator growth increment */
#define	LINE_TEST_BLOCK	1024

#define	GF_LINE_TEST_EOB(f) \
			((f)->line + ((f)->f2 - 1))

#define	GF_LINE_TEST_ADD(f, c) \
			{ \
				    if(p >= eobuf){ \
					f->f2 += LINE_TEST_BLOCK; \
					fs_resize((void **)&f->line, \
					      (size_t) f->f2 * sizeof(char)); \
					eobuf = GF_LINE_TEST_EOB(f); \
					p = eobuf - LINE_TEST_BLOCK; \
				    } \
				    *p++ = c; \
				}

#define	GF_LINE_TEST_TEST(F, D) \
			{ \
			    unsigned char  c; \
			    register char *cp; \
			    register int   l; \
			    LT_INS_S	  *ins = NULL, *insp; \
			    *p = '\0'; \
			    (D) = (*((LINETEST_S *) (F)->opt)->f)((F)->n++, \
					   (F)->line, &ins, \
					   ((LINETEST_S *) (F)->opt)->local); \
			    if((D) < 2){ \
				if((D) < 0){ \
				    if((F)->line) \
				      fs_give((void **) &(F)->line); \
				    if((F)->opt) \
				      fs_give((void **) &(F)->opt); \
				    gf_error(_("translation error")); \
				    /* NO RETURN */ \
				} \
				for(insp = ins, cp = (F)->line; cp < p; ){ \
				  if(insp && cp == insp->where){ \
				    if(insp->len > 0){ \
					for(l = 0; l < insp->len; l++){ \
					  c = (unsigned char) insp->text[l]; \
					  GF_PUTC((F)->next, c);  \
					}  \
					insp = insp->next; \
					continue; \
				    } else if(insp->len < 0){ \
					cp -= insp->len; \
					insp = insp->next; \
					continue; \
				    } \
				  } \
				  GF_PUTC((F)->next, *cp); \
				  cp++; \
				} \
				while(insp){ \
				    for(l = 0; l < insp->len; l++){ \
					c = (unsigned char) insp->text[l]; \
					GF_PUTC((F)->next, c); \
				    } \
				    insp = insp->next; \
				} \
				gf_line_test_free_ins(&ins); \
			    } \
			}



/*
 * this simple filter accumulates characters until a newline, offers it
 * to the provided test function, and then passes it on.  It assumes
 * NVT EOLs.
 */
void
gf_line_test(FILTER_S *f, int flg)
{
    register char *p = f->linep;
    register char *eobuf = GF_LINE_TEST_EOB(f);
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;
	register int state = f->f1;

	while(GF_GETC(f, c)){

	    if(state){
		state = 0;
		if(c == '\012'){
		    int done;

		    GF_LINE_TEST_TEST(f, done);

		    p = (f)->line;

		    if(done == 2)	/* skip this line! */
		      continue;

		    GF_PUTC(f->next, '\015');
		    GF_PUTC(f->next, '\012');
		    /*
		     * if the line tester returns TRUE, it's
		     * telling us its seen enough and doesn't
		     * want to see any more.  Remove ourself
		     * from the pipeline...
		     */
		    if(done){
			if(gf_master == f){
			    gf_master = f->next;
			}
			else{
			    FILTER_S *fprev;

			    for(fprev = gf_master;
				fprev && fprev->next != f;
				fprev = fprev->next)
			      ;

			    if(fprev)		/* wha??? */
			      fprev->next = f->next;
			    else
			      continue;
			}

			while(GF_GETC(f, c))	/* pass input */
			  GF_PUTC(f->next, c);

			(void) GF_FLUSH(f->next);	/* and drain queue */
			fs_give((void **)&f->line);
			fs_give((void **)&f);	/* wax our data */
			return;
		    }
		    else
		      continue;
		}
		else			/* add CR to buffer */
		  GF_LINE_TEST_ADD(f, '\015');
	    } /* fall thru to handle 'c' */

	    if(c == '\015')		/* newline? */
	      state = 1;
	    else
	      GF_LINE_TEST_ADD(f, c);
	}

	f->f1 = state;
	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	int i;

	GF_LINE_TEST_TEST(f, i);	/* examine remaining data */
	fs_give((void **) &f->line);	/* free line buffer */
	fs_give((void **) &f->opt);	/* free test struct */
	(void) GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint((9, "-- gf_reset line_test\n"));
	f->f1 = 0;			/* state */
	f->n  = 0L;			/* line number */
	f->f2 = LINE_TEST_BLOCK;	/* size of alloc'd line */
	f->line = p = (char *) fs_get(f->f2 * sizeof(char));
    }

    f->linep = p;
}


/*
 * function called from the outside to operate on accumulated line.
 */
void *
gf_line_test_opt(linetest_t test_f, void *local)
{
    LINETEST_S *ltp;

    ltp = (LINETEST_S *) fs_get(sizeof(LINETEST_S));
    memset(ltp, 0, sizeof(LINETEST_S));
    ltp->f     = test_f;
    ltp->local = local;
    return((void *) ltp);
}



LT_INS_S **
gf_line_test_new_ins(LT_INS_S **ins, char *p, char *s, int n)
{
    *ins = (LT_INS_S *) fs_get(sizeof(LT_INS_S));
    if(((*ins)->len = n) > 0)
      strncpy((*ins)->text = (char *) fs_get(n * sizeof(char)), s, n);
    else
      (*ins)->text = NULL;

    (*ins)->where = p;
    (*ins)->next  = NULL;
    return(&(*ins)->next);
}


void
gf_line_test_free_ins(LT_INS_S **ins)
{
    if(ins && *ins){
	if((*ins)->next)
	  gf_line_test_free_ins(&(*ins)->next);

	if((*ins)->text)
	  fs_give((void **) &(*ins)->text);

	fs_give((void **) ins);
    }
}


/*
 * PREPEND EDITORIAL FILTER - conditionally prepend output text
 *                            with editorial comment
 */

typedef struct _preped_s {
    prepedtest_t  f;
    char	 *text;
} PREPED_S;


/*
 * gf_prepend_editorial - accumulate filtered text and prepend its 
 *                        output with given text
 * 
 * 
 */
void
gf_prepend_editorial(FILTER_S *f, int flg)
{
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;

	while(GF_GETC(f, c)){
	    so_writec(c, (STORE_S *) f->data);
	}

	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	unsigned char c;

	if(!((PREPED_S *)(f)->opt)->f || (*((PREPED_S *)(f)->opt)->f)()){
	    char *p = ((PREPED_S *)(f)->opt)->text;

	    for( ; p && *p; p++)
	      GF_PUTC(f->next, *p);
	}

	so_seek((STORE_S *) f->data, 0L, 0);
	while(so_readc(&c, (STORE_S *) f->data)){
	    GF_PUTC(f->next, c);
	}

	so_give((STORE_S **) &f->data);
	fs_give((void **) &f->opt);
	(void) GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint((9, "-- gf_reset line_test\n"));
	f->data = (void *) so_get(CharStar, NULL, EDIT_ACCESS);
    }
}


/*
 * function called from the outside to setup prepending editorial
 * to output text
 */
void *
gf_prepend_editorial_opt(prepedtest_t test_f, char *text)
{
    PREPED_S *pep;

    pep = (PREPED_S *) fs_get(sizeof(PREPED_S));
    memset(pep, 0, sizeof(PREPED_S));
    pep->f    = test_f;
    pep->text = text;
    return((void *) pep);
}


/*
 * Network virtual terminal to local newline convention filter
 */
void
gf_nvtnl_local(FILTER_S *f, int flg)
{
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;
	register int state = f->f1;

	while(GF_GETC(f, c)){
	    if(state){
		state = 0;
		if(c == '\012'){
		    GF_PUTC(f->next, '\012');
		    continue;
		}
		else
		  GF_PUTC(f->next, '\015');
		/* fall thru to deal with 'c' */
	    }

	    if(c == '\015')
	      state = 1;
	    else
	      GF_PUTC(f->next, c);
	}

	f->f1 = state;
	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	(void) GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(flg == GF_RESET){
	dprint((9, "-- gf_reset nvtnl_local\n"));
	f->f1 = 0;
    }
}


/*
 * local to network newline convention filter
 */
void
gf_local_nvtnl(FILTER_S *f, int flg)
{
    GF_INIT(f, f->next);

    if(flg == GF_DATA){
	register unsigned char c;

	while(GF_GETC(f, c)){
	    if(c == '\012'){
		GF_PUTC(f->next, '\015');
		GF_PUTC(f->next, '\012');
	    }
	    else
	      GF_PUTC(f->next, c);
	}

	GF_END(f, f->next);
    }
    else if(flg == GF_EOD){
	(void) GF_FLUSH(f->next);
	(*f->next->f)(f->next, GF_EOD);
    }
    else if(GF_RESET){
	dprint((9, "-- gf_reset local_nvtnl\n"));
	/* no op */
    }

}
