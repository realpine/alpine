#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: addrstring.c 770 2007-10-24 00:23:09Z hubert@u.washington.edu $";
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
#include "../pith/addrstring.h"
#include "../pith/state.h"
#include "../pith/copyaddr.h"
#include "../pith/charset.h"
#include "../pith/stream.h"


/*
 * Internal prototypes
 */
void	rfc822_write_address_decode(char *, size_t, ADDRESS *, int);


/*
 * Format an address structure into a string
 *
 * Args: addr -- Single ADDRESS structure to turn into a string
 *
 * Result:  Fills in buf and returns pointer to it.
 * Just uses the c-client call to do this.
 *		(the address is not rfc1522 decoded)
 */
char *
addr_string(struct mail_address *addr, char *buf, size_t buflen)
{
    ADDRESS *next_addr;
    RFC822BUFFER rbuf;

    *buf = '\0';
    next_addr = addr->next;
    addr->next = NULL;
    rbuf.f   = dummy_soutr;
    rbuf.s   = NULL;
    rbuf.beg = buf;
    rbuf.cur = buf;
    rbuf.end = buf+buflen-1;
    rfc822_output_address_list(&rbuf, addr, 0L, NULL);
    *rbuf.cur = '\0';
    addr->next = next_addr;
    return(buf);
}


/*
 * Same as addr_string only it doesn't have to be a
 * single address.
 */
char *
addr_string_mult(struct mail_address *addr, char *buf, size_t buflen)
{
    RFC822BUFFER rbuf;

    *buf = '\0';
    rbuf.f   = dummy_soutr;
    rbuf.s   = NULL;
    rbuf.beg = buf;
    rbuf.cur = buf;
    rbuf.end = buf+buflen-1;
    rfc822_output_address_list(&rbuf, addr, 0L, NULL);
    *rbuf.cur = '\0';
    return(buf);
}


/*
 * Format an address structure into a simple string: "mailbox@host"
 *
 * Args: addr -- Single ADDRESS structure to turn into a string
 *	 buf -- buffer to write address in;
 *
 * Result:  Returns pointer to buf;
 */
char *
simple_addr_string(struct mail_address *addr, char *buf, size_t buflen)
{
    RFC822BUFFER rbuf;

    rbuf.f   = dummy_soutr;
    rbuf.s   = NULL;
    rbuf.beg = buf;
    rbuf.cur = buf;
    rbuf.end = buf+buflen-1;
    rfc822_output_address(&rbuf, addr);
    *rbuf.cur = '\0';

    return(buf);
}


/*
 * Format an address structure into a simple string: "mailbox@host"
 * Like simple_addr_string but can be multiple addresses.
 *
 * Args: addr -- ADDRESS structure to turn into a string
 *	  buf -- buffer to write address in;
 *     buflen -- length of buffer
 *        sep -- separator string
 *
 * Result:  Returns pointer to internal static formatted string.
 * Just uses the c-client call to do this.
 */
char *
simple_mult_addr_string(struct mail_address *addr, char *buf, size_t buflen, char *sep)
{
    ADDRESS *a;
    char    *dest = buf;
    size_t   seplen = 0;

    if(sep)
      seplen = strlen(sep);

    *dest = '\0';
    for(a = addr; a; a = a->next){
	if(dest > buf && seplen > 0 && buflen-1-(dest-buf) >= seplen){
	    strncpy(dest, sep, seplen);
	    dest += seplen;
	    *dest = '\0';
	}
	   
	simple_addr_string(a, dest, buflen-(dest-buf));
	dest += strlen(dest);
    }

    buf[buflen-1] = '\0';

    return(buf);
}


/*
 * 1522 encode the personal name portion of addr and return an allocated
 * copy of the resulting address string.
 */
char *
encode_fullname_of_addrstring(char *addr, char *charset)
{
    char    *pers_encoded,
            *tmp_a_string,
            *ret = NULL;
    ADDRESS *adr;
    static char *fakedomain = "@";
    RFC822BUFFER rbuf;
    size_t len;

    tmp_a_string = cpystr(addr ? addr : "");
    adr = NULL;
    rfc822_parse_adrlist(&adr, tmp_a_string, fakedomain);
    fs_give((void **)&tmp_a_string);

    if(!adr)
      return(cpystr(""));

    if(adr->personal && adr->personal[0]){
	pers_encoded = cpystr(rfc1522_encode(tmp_20k_buf, SIZEOF_20KBUF,
				(unsigned char *)adr->personal,
				charset));
	fs_give((void **)&adr->personal);
	adr->personal = pers_encoded;
    }

    len = est_size(adr);
    ret = (char *) fs_get(len * sizeof(char));
    ret[0] = '\0';
    rbuf.f   = dummy_soutr;
    rbuf.s   = NULL;
    rbuf.beg = ret;
    rbuf.cur = ret;
    rbuf.end = ret+len-1;
    rfc822_output_address_list(&rbuf, adr, 0L, NULL);
    *rbuf.cur = '\0';
    mail_free_address(&adr);
    return(ret);
}


/*
 * 1522 decode the personal name portion of addr and return an allocated
 * copy of the resulting address string.
 */
char *
decode_fullname_of_addrstring(char *addr, int verbose)
{
    char    *pers_decoded,
            *tmp_a_string,
	    *ret = NULL;
    ADDRESS *adr;
    static char *fakedomain = "@";
    RFC822BUFFER rbuf;
    size_t len;

    tmp_a_string = cpystr(addr ? addr : "");
    adr = NULL;
    rfc822_parse_adrlist(&adr, tmp_a_string, fakedomain);
    fs_give((void **)&tmp_a_string);

    if(!adr)
      return(cpystr(""));

    if(adr->personal && adr->personal[0]){
	pers_decoded
	    = cpystr((char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf,
						    SIZEOF_20KBUF, adr->personal));
	fs_give((void **)&adr->personal);
	adr->personal = pers_decoded;
    }

    len = est_size(adr);
    ret = (char *) fs_get(len * sizeof(char));
    ret[0] = '\0';
    rbuf.f   = dummy_soutr;
    rbuf.s   = NULL;
    rbuf.beg = ret;
    rbuf.cur = ret;
    rbuf.end = ret+len-1;
    rfc822_output_address_list(&rbuf, adr, 0L, NULL);
    *rbuf.cur = '\0';
    mail_free_address(&adr);
    return(ret);
}


/*
 * Turn a list of address structures into a formatted string
 *
 * Args: adrlist -- An adrlist
 *       f       -- Function to use to print one address in list.  If NULL,
 *                  use rfc822_write_address_decode to print whole list.
 *      do_quote -- Quote quotes and dots (only used if f == NULL).
 * Result:  comma separated list of addresses which is
 *                                     malloced here and returned
 *		(the list is rfc1522 decoded unless f is *not* NULL)
 */
char *
addr_list_string(struct mail_address *adrlist,
		 char *(*f)(struct mail_address *, char *, size_t),
		 int do_quote)
{
    size_t            len;
    char             *list, *s, string[MAX_ADDR_EXPN+1];
    register ADDRESS *a;

    if(!adrlist)
      return(cpystr(""));
    
    if(f){
	len = 0;
	for(a = adrlist; a; a = a->next)
	  len += (strlen((*f)(a, string, sizeof(string))) + 2);

	list = (char *) fs_get((len+1) * sizeof(char));
	s    = list;
	s[0] = '\0';

	for(a = adrlist; a; a = a->next){
	    sstrncpy(&s, (*f)(a, string, sizeof(string)), len-(s-list));
	    if(a->next && len-(s-list) > 2){
		*s++ = ',';
		*s++ = SPACE;
	    }
	}
    }
    else{
	len = est_size(adrlist);
	list = (char *) fs_get((len+1) * sizeof(char));
	list[0] = '\0';
	rfc822_write_address_decode(list, len+1, adrlist, do_quote);
	removing_leading_and_trailing_white_space(list);
    }

    list[len] = '\0';
    return(list);
}


static long rfc822_dummy_soutr (void *stream, char *string)
{
    return LONGT;
}

/*
 * Copied from c-client/rfc822.c buf with dot and double quote removed.
 */
static const char *rspecials_minus_quote_and_dot = "()<>@,;:\\[]\1\2\3\4\5\6\7\10\11\12\13\14\15\16\17\20\21\22\23\24\25\26\27\30\31\32\33\34\35\36\37\177";

/* Write RFC822 address with 1522 decoding of personal name
 * and optional quoting.
 *
 * The idea is that there are some places where we'd just like to display
 * the personal name as is before applying confusing quoting. However,
 * we do want to be careful not to break things that should be quoted so
 * we'll only use this where we are sure. Quoting may look ugly but it
 * doesn't usually break anything.
 */
void
rfc822_write_address_decode(char *dest, size_t destlen, struct mail_address *adr, int do_quote)
{
    RFC822BUFFER buf;
    extern const char *rspecials;
    ADDRESS *copy, *a;

    /*
     * We want to print the adr list after decoding it. C-client knows
     * how to parse and print, so we want to use that. But c-client
     * doesn't decode. So we make a copy of the address list, decode
     * things there, and let c-client print that.
     */
    copy = copyaddrlist(adr);
    for(a = copy; a; a = a->next){
	if(a->host){		/* ordinary address? */
	    if(a->personal && *a->personal){
		unsigned char *p;

		p = rfc1522_decode_to_utf8((unsigned char *) tmp_20k_buf,
					   SIZEOF_20KBUF, a->personal);

		if(p && (char *) p != a->personal){
		    fs_give((void **) &a->personal);
		    a->personal = cpystr((char *) p);
		}
	    }
	}
	else if(a->mailbox && *a->mailbox){	/* start of group? */
	    unsigned char *p;

	    p = rfc1522_decode_to_utf8((unsigned char *) tmp_20k_buf, SIZEOF_20KBUF, a->mailbox);

	    if(p && (char *) p != a->mailbox){
		fs_give((void **) &a->mailbox);
		a->mailbox = cpystr((char *) p);
	    }
	}
    }

    buf.end = (buf.beg = buf.cur = dest) + destlen;
    buf.f = rfc822_dummy_soutr;
    *buf.cur = '\0';
    buf.s = NIL;

    (void) rfc822_output_address_list(&buf, copy, 0,
				      do_quote ? rspecials : rspecials_minus_quote_and_dot);

    *buf.cur = '\0';

    if(copy)
      mail_free_address(&copy);
}


/*
 * Compute an upper bound on the size of the array required by
 * rfc822_write_address for this list of addresses.
 *
 * Args: adrlist -- The address list.
 *
 * Returns -- an integer giving the upper bound
 */
int
est_size(struct mail_address *a)
{
    int cnt = 0;

    for(; a; a = a->next){

	/* two times personal for possible quoting */
	cnt   += 2 * (a->personal  ? (strlen(a->personal)+1)  : 0);
	cnt   += 2 * (a->mailbox  ? (strlen(a->mailbox)+1)    : 0);
	cnt   += (a->adl      ? strlen(a->adl)      : 0);
	cnt   += (a->host     ? strlen(a->host)     : 0);

	/*
	 * add room for:
         *   possible single space between fullname and addr
         *   left and right brackets
         *   @ sign
         *   possible : for route addr
         *   , <space>
	 *
	 * So I really think that adding 7 is enough.  Instead, I'll add 10.
	 */
	cnt   += 10;
    }

    return(MAX(cnt, 50));  /* just making sure */
}


/*
 * Returns the number of addresses in the list.
 */
int
count_addrs(struct mail_address *adrlist)
{
    int cnt = 0;

    while(adrlist){
	if(adrlist->mailbox && adrlist->mailbox[0])
	  cnt++;

	adrlist = adrlist->next;
    }
    
    return(cnt);
}


/*
 * Buf is at least size maxlen+1
 */
void
a_little_addr_string(struct mail_address *addr, char *buf, size_t maxlen)
{
    buf[0] = '\0';
    if(addr){
	if(addr->personal && addr->personal[0]){
	    char tmp[MAILTMPLEN];

	    snprintf(tmp, sizeof(tmp), "%s", addr->personal);
	    iutf8ncpy(buf, (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf,
							  SIZEOF_20KBUF, tmp),
		      maxlen);
	}
	else if(addr->mailbox && addr->mailbox[0]){
	    strncpy(buf, addr->mailbox, maxlen);
	    buf[maxlen] = '\0';
	    if(addr->host && addr->host[0] && addr->host[0] != '.'){
		strncat(buf, "@", maxlen+1-1-strlen(buf));
		strncat(buf, addr->host, maxlen+1-1-strlen(buf));
	    }
	}
    }

    buf[maxlen] = '\0';
}
