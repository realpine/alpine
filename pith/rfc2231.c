#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: rfc2231.c 1012 2008-03-26 00:44:22Z hubert@u.washington.edu $";
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
#include "../pith/rfc2231.h"
#include "../pith/mimedesc.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/store.h"
#include "../pith/status.h"
#include "../pith/send.h"
#include "../pith/string.h"


/*
 *  * * * * * * * *      RFC 2231 support routines      * * * * * * * *
 */


/* Useful def's */
#define	RFC2231_MAX	64


char *
rfc2231_get_param(PARAMETER *parms, char *name,
		  char **charset, char **lang)
{
    char *buf, *p;
    int	  decode = 0, name_len, i;
    unsigned n;

    name_len = strlen(name);
    for(; parms ; parms = parms->next)
      if(!struncmp(name, parms->attribute, name_len)){
	if(parms->attribute[name_len] == '*'){
	    for(p = &parms->attribute[name_len + 1], n = 0; *(p+n); n++)
	      ;

	    decode = *(p + n - 1) == '*';

	    if(isdigit((unsigned char) *p)){
		char *pieces[RFC2231_MAX];
		int   count = 0, len;

		memset(pieces, 0, RFC2231_MAX * sizeof(char *));

		while(parms){
		    n = 0;
		    do
		      n = (n * 10) + (*p - '0');
		    while(isdigit(*++p) && n < RFC2231_MAX);

		    if(n < RFC2231_MAX){
			pieces[n] = parms->value;
			if(n > count)
			  count = n;
		    }
		    else{
			q_status_message1(SM_ORDER | SM_DING, 0, 3,
		   "Invalid attachment parameter segment number: %.25s",
					  name);
			return(NULL);		/* Too many segments! */
		    }

		    while((parms = parms->next) != NULL)
		      if(!struncmp(name, parms->attribute, name_len)){
			  if(*(p = &parms->attribute[name_len]) == '*'
			      && isdigit((unsigned char) *++p))
			    break;
			  else
			    return(NULL);	/* missing segment no.! */
		      }
		}

		for(i = len = 0; i <= count; i++)
		  if(pieces[i])
		    len += strlen(pieces[i]);
		  else{
		      q_status_message1(SM_ORDER | SM_DING, 0, 3,
		        "Missing attachment parameter sequence: %.25s",
					name);

		    return(NULL);		/* hole! */
		  }

		buf = (char *) fs_get((len + 1) * sizeof(char));

		for(i = len = 0; i <= count; i++){
		    if((n = *(p = pieces[i]) == '\"') != 0) /* quoted? */
		      p++;

		    while(*p && !(n && *p == '\"' && !*(p+1)))
		      buf[len++] = *p++;
		}

		buf[len] = '\0';
	    }
	    else
	      buf = cpystr(parms->value);

	    /* Do any RFC 2231 decoding? */
	    if(decode){
		char *converted = NULL, cs[1000];

		cs[0] = '\0';
		n = 0;

		if((p = strchr(buf, '\'')) != NULL){
		    n = (p - buf) + 1;
		    *p = '\0';
		    strncpy(cs, buf, sizeof(cs));
		    cs[sizeof(cs)-1] = '\0';
		    *p = '\'';
		    if(charset)
		      *charset = cpystr(cs);

		    if((p = strchr(&buf[n], '\'')) != NULL){
			n = (p - buf) + 1;
			if(lang){
			    *p = '\0';
			    *lang = cpystr(p);
			    *p = '\'';
			}
		    }
		}

		if(n){
		    /* Suck out the charset & lang while decoding hex */
		    p = &buf[n];
		    for(i = 0; (buf[i] = *p) != '\0'; i++)
		      if(*p++ == '%' && isxpair(p)){
			  buf[i] = X2C(p);
			  p += 2;
		      }
		}
		else
		  fs_give((void **) &buf);	/* problems!?! */

		/*
		 * Callers will expect the returned value to be UTF-8
		 * text, so we may need to translate here.
		 */
		if(buf)
		  converted = convert_to_utf8(buf, cs, 0);

		if(converted && converted != buf){
		    fs_give((void **) &buf);
		    buf = converted;
		}
	    }
	    
	    return(buf);
	}
	else
	  return(cpystr(parms->value ? parms->value : ""));
      }

    return(NULL);
}


int
rfc2231_output(STORE_S *so, char *attrib, char *value, char *specials, char *charset)
{
    int  i, line = 0, encode = 0, quote = 0;

    /*
     * scan for hibit first since encoding clue has to
     * come on first line if any parms are broken up...
     */
    for(i = 0; value && value[i]; i++)
      if(value[i] & 0x80){
	  encode++;
	  break;
      }

    for(i = 0; ; i++){
	if(!(value && value[i]) || i > 80){	/* flush! */
	    if((line++ && !so_puts(so, ";\015\012        "))
	       || !so_puts(so, attrib))
		return(0);

	    if(value){
		if(((value[i] || line > 1) /* more lines or already lines */
		    && !(so_writec('*', so)
			 && so_puts(so, int2string(line - 1))))
		   || (encode && !so_writec('*', so))
		   || !so_writec('=', so)
		   || (quote && !so_writec('\"', so))
		   || ((line == 1 && encode)
		       && !(so_puts(so, charset ? charset : UNKNOWN_CHARSET)
			     && so_puts(so, "''"))))
		  return(0);

		while(i--){
		    if(*value & 0x80){
			char tmp[3], *p;

			p = tmp;
			C2XPAIR(*value, p);
			*p = '\0';
			if(!(so_writec('%', so) && so_puts(so, tmp)))
			  return(0);
		    }
		    else if(((*value == '\\' || *value == '\"')
			     && !so_writec('\\', so))
			    || !so_writec(*value, so))
		      return(0);

		    value++;
		}

		if(quote && !so_writec('\"', so))
		  return(0);

		if(*value)			/* more? */
		  i = quote = 0;		/* reset! */
		else
		  return(1);			/* done! */
	    }
	    else
	      return(1);
	}

	if(!quote && strchr(specials, value[i]))
	  quote++;
    }
}


PARMLIST_S *
rfc2231_newparmlist(PARAMETER *params)
{
    PARMLIST_S *p = NULL;

    if(params){
	p = (PARMLIST_S *) fs_get(sizeof(PARMLIST_S));
	memset(p, 0, sizeof(PARMLIST_S));
	p->list = params;
    }

    return(p);
}


void
rfc2231_free_parmlist(PARMLIST_S **p)
{
    if(*p){
	if((*p)->value)
	  fs_give((void **) &(*p)->value);

	mail_free_body_parameter(&(*p)->seen);
	fs_give((void **) p);
    }
}


int
rfc2231_list_params(PARMLIST_S *plist)
{
    PARAMETER *pp, **ppp;
    int	       i;

    if(plist->value)
      fs_give((void **) &plist->value);

    for(pp = plist->list; pp; pp = pp->next){
      /* get a name */
      for(i = 0; i < 32; i++)
	if(!(plist->attrib[i] = pp->attribute[i]) ||  pp->attribute[i] == '*'){
	    plist->attrib[i] = '\0';

	    for(ppp = &plist->seen;
		*ppp && strucmp((*ppp)->attribute, plist->attrib);
		ppp = &(*ppp)->next)
	      ;

	    if(!*ppp){
		plist->list = pp->next;
		*ppp = mail_newbody_parameter();	/* add to seen list */
		(*ppp)->attribute = cpystr(plist->attrib);
		plist->value = parameter_val(pp,plist->attrib);
		return(TRUE);
	    }

	    break;
	}

      if(i >= 32)
	q_status_message1(SM_ORDER | SM_DING, 0, 3,
		       "Overly long attachment parameter ignored: %.25s...",
			  pp->attribute);
    }


    return(FALSE);
}
