#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: maillist.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
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

/*
 *  * * * * * * * *      RFC 2369 support routines      * * * * * * * *
 */


#include "../pith/headers.h"
#include "../pith/maillist.h"
#include "../pith/state.h"
#include "../pith/url.h"


/*
 * Internal prototypes
 */
int	    rfc2369_parse(char *, RFC2369_S *);

/*
 * * NOTE * These have to remain in sync with the MLCMD_* macros
 *	    in maillist.h.  Sorry.
 */
static RFC2369FIELD_S rfc2369_fields[] = {
    {"List-Help",
     "get information about the list and instructions on how to join",
     "seek help"},
    {"List-Unsubscribe",
     "remove yourself from the list (Unsubscribe)",
     "UNsubscribe"},
    {"List-Subscribe",
     "add yourself to the list (Subscribe)",
     "Subscribe"},
    {"List-Post",
     "send a message to the entire list (Post)",
     "post a message"},
    {"List-Owner",
     "send a message to the list owner",
     "contact the list owner"},
    {"List-Archive",
     "view archive of messages sent to the list",
     "view the archive"}
};


char **
rfc2369_hdrs(char **hdrs)
{
    int i;

    for(i = 0; i < MLCMD_COUNT; i++)
      hdrs[i] = rfc2369_fields[i].name;

    hdrs[i] = NULL;
    return(hdrs);
}


int
rfc2369_parse_fields(char *h, RFC2369_S *data)
{
    char *ep, *nhp, *tp;
    int	  i, rv = FALSE;

    for(i = 0; i < MLCMD_COUNT; i++)
      data[i].field = rfc2369_fields[i];

    for(nhp = h; h; h = nhp){
	/* coerce h to start of field */
	for(ep = h;;)
	  if((tp = strpbrk(ep, "\015\012")) != NULL){
	      if(strindex(" \t", *((ep = tp) + 2))){
		  *ep++ = ' ';		/* flatten continuation */
		  *ep++ = ' ';
		  for(; *ep; ep++)	/* advance past whitespace */
		    if(*ep == '\t')
		      *ep = ' ';
		    else if(*ep != ' ')
		      break;
	      }
	      else{
		  *ep = '\0';		/* tie off header data */
		  nhp = ep + 2;		/* start of next header */
		  break;
	      }
	  }
	  else{
	      while(*ep)		/* find the end of this line */
		ep++;

	      nhp = NULL;		/* no more header fields */
	      break;
	  }

	/* if length is within reason, see if we're interested */
	if(ep - h < MLCMD_REASON && rfc2369_parse(h, data))
	  rv = TRUE;
    }

    return(rv);
}


int
rfc2369_parse(char *h, RFC2369_S *data)
{
    int   l, ifield, idata = 0;
    char *p, *p1, *url, *comment;

    /* look for interesting name's */
    for(ifield = 0; ifield < MLCMD_COUNT; ifield++)
      if(!struncmp(h, rfc2369_fields[ifield].name,
		   l = strlen(rfc2369_fields[ifield].name))
	 && *(h += l) == ':'){
	  /* unwrap any transport encodings */
	  if((p = (char *) rfc1522_decode_to_utf8((unsigned char *) tmp_20k_buf,
						  SIZEOF_20KBUF, ++h)) == tmp_20k_buf)
	    strcpy(h, p);		/* assumption #383: decoding shrinks */

	  url = comment = NULL;
	  while(*h){
	      while(*h == ' ')
		h++;

	      switch(*h){
		case '<' :		/* URL */
		  if((p = strindex(h, '>')) != NULL){
		      url = ++h;	/* remember where it starts */
		      *p = '\0';	/* tie it off */
		      h  = p + 1;	/* advance h */
		      for(p = p1 = url; (*p1 = *p) != '\0'; p++)
			if(*p1 != ' ')
			  p1++;		/* remove whitespace ala RFC */
		  }
		  else
		    *h = '\0';		/* tie off junk */

		  break;

		case '(' :			/* Comment */
		  comment = rfc822_skip_comment(&h, LONGT);
		  break;

		case 'N' :			/* special case? */
		case 'n' :
		  if(ifield == MLCMD_POST
		     && (*(h+1) == 'O' || *(h+1) == 'o')
		     && (!*(h+2) || *(h+2) == ' ')){
		      ;			/* yup! */

		      url = h;
		      *(h + 2) = '\0';
		      h += 3;
		      break;
		  }

		default :
		  removing_trailing_white_space(h);
		  if(!url
		     && (url = rfc1738_scan(h, &l))
		     && url == h && l == strlen(h)){
		      removing_trailing_white_space(h);
		      data[ifield].data[idata].value = url;
		  }
		  else
		    data[ifield].data[idata].error = h;

		  return(1);		/* return junk */
	      }

	      while(*h == ' ')
		h++;

	      switch(*h){
		case ',' :
		  h++;

		case '\0':
		  if(url || (comment && *comment)){
		      data[ifield].data[idata].value = url;
		      data[ifield].data[idata].comment = comment;
		      url = comment = NULL;
		  }

		  if(++idata == MLCMD_MAXDATA)
		    *h = '\0';

		default :
		  break;
	      }
	  }
      }

    return(idata);
}
