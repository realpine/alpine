#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: help.c 900 2008-01-05 01:13:26Z hubert@u.washington.edu $";
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
#include "../pith/help.h"
#include "../pith/flag.h"
#include "../pith/conf.h"
#include "../pith/sort.h"


REV_MSG_S rmjoarray[RMJLEN];	/* For regular journal */
REV_MSG_S rmloarray[RMLLEN];	/* debug 0-4 */
REV_MSG_S rmhiarray[RMHLEN];	/* debug 5-9 */
int       rmjofirst = -1, rmjolast = -1;
int       rmlofirst = -1, rmlolast = -1;
int       rmhifirst = -1, rmhilast = -1;
int       rm_not_right_now;



HelpType
help_name2section(char *url, int url_len)
{
    char		name[256];
    HelpType		newhelp = NO_HELP;
    struct help_texts *t;

    snprintf(name, sizeof(name), "%.*s", MIN(url_len,sizeof(name)), url);

    for(t = h_texts; t->help_text != NO_HELP; t++)
      if(!strucmp(t->tag, name)){
	  newhelp = t->help_text;
	  break;
      }

    return(newhelp);
}


char *
get_alpine_revision_string(char *buf, size_t nbuf)
{
    char ourbuf[100], *p;
    char *rev = NULL;

    buf[0] = '\0';
    ourbuf[0] = '\0';

    /* HelpType (the type of h_revision) is assumed to be char ** */
    if(h_revision && h_revision[0] && h_revision[0][0]){
	strncpy(ourbuf, h_revision[0], sizeof(ourbuf)-1);
	ourbuf[sizeof(ourbuf)-1] = '\0';
    }

    if(ourbuf[0]){
	/* move to revision number */
	for(p = ourbuf; *p && !isdigit((unsigned char) (*p)); p++)
	  ;

	if(*p)
	  rev = p;

	if(rev){
	    /* skip to following space */
	    for(; *p && !isspace((unsigned char) (*p)); p++)
	      ;

	    /* skip whitespace */
	    for(; *p && isspace((unsigned char) (*p)); p++)
	      ;

	    /* skip over date to following space */
	    for(; *p && !isspace((unsigned char) (*p)); p++)
	      ;

	    strncpy(buf, rev, MIN(p-rev, nbuf-1));
	    buf[MIN(p-rev,nbuf-1)] = '\0';
	}
    }

    return(buf);
}


char *
get_alpine_revision_number(char *buf, size_t nbuf)
{
    char ourbuf[100], *p;
    char *rev = NULL;

    buf[0] = '\0';
    ourbuf[0] = '\0';

    /* HelpType (the type of h_revision) is assumed to be char ** */
    if(h_revision && h_revision[0] && h_revision[0][0]){
	strncpy(ourbuf, h_revision[0], sizeof(ourbuf)-1);
	ourbuf[sizeof(ourbuf)-1] = '\0';
    }

    if(ourbuf[0]){
	/* move to revision number */
	for(p = ourbuf; *p && !isdigit((unsigned char) (*p)); p++)
	  ;

	if(*p)
	  rev = p;

	if(rev){
	    /* skip to following space */
	    for(; *p && !isspace((unsigned char) (*p)); p++)
	      ;

	    strncpy(buf, rev, MIN(p-rev, nbuf-1));
	    buf[MIN(p-rev,nbuf-1)] = '\0';
	}
    }

    return(buf);
}


#ifdef DEBUG

void
debugjournal_to_file(FILE *dfile)
{
    int donejo, donelo, donehi, jo, lo, hi;
    RMCat rmcat;

    if(dfile && (rmjofirst >= 0 || rmlofirst >= 0 || rmhifirst >= 0)
       && rmjofirst < RMJLEN && rmjolast < RMJLEN
       && rmlofirst < RMLLEN && rmlolast < RMLLEN
       && rmhifirst < RMHLEN && rmhilast < RMHLEN
       && (rmjofirst < 0 || rmjolast >= 0)
       && (rmlofirst < 0 || rmlolast >= 0)
       && (rmhifirst < 0 || rmhilast >= 0)){

	donejo = donehi = donelo = 0;
	jo = rmjofirst;
	if(jo < 0)
	  donejo = 1;

	lo = rmlofirst;
	if(lo < 0)
	  donelo = 1;

	hi = rmhifirst;
	if(hi < 0)
	  donehi = 1;

	while(!(donejo && donelo && donehi)){
	    REV_MSG_S *pjo, *plo, *phi, *p;

	    if(!donejo)
	      pjo = &rmjoarray[jo];
	    else
	      pjo = NULL;

	    if(!donelo)
	      plo = &rmloarray[lo];
	    else
	      plo = NULL;

	    if(!donehi)
	      phi = &rmhiarray[hi];
	    else
	      phi = NULL;

	    if(pjo && (!plo || pjo->seq <= plo->seq)
	       && (!phi || pjo->seq <= phi->seq))
	      rmcat = Jo;
	    else if(plo && (!phi || plo->seq <= phi->seq))
	      rmcat = Lo;
	    else if(phi)
	      rmcat = Hi;
	    else
	      rmcat = No;

	    if(rmcat == Jo){
		p = pjo;
		if(jo == rmjofirst &&
		   (((rmjolast + 1) % RMJLEN) == rmjofirst) &&
       fputs("*** Level -1 entries prior to this are deleted", dfile) == EOF)
		  break;
	    }
	    else if(rmcat == Lo){
		p = plo;
		if(lo == rmlofirst &&
		   (((rmlolast + 1) % RMLLEN) == rmlofirst) &&
       fputs("*** Level 0-4 entries prior to this are deleted", dfile) == EOF)
		  break;
	    }
	    else if(rmcat == Hi){
		p = phi;
		if(hi == rmhifirst &&
		   (((rmhilast + 1) % RMHLEN) == rmhifirst) &&
       fputs("*** Level 5-9 entries prior to this are deleted", dfile) == EOF)
		  break;
	    }
	    else if(rmcat == No){
		p = NULL;
	    }

	    if(p){
		if(p->timestamp && p->timestamp[0]
		   && (fputs(p->timestamp, dfile) == EOF
		       || fputs(": ", dfile) == EOF))
		  break;

		if(p->message && p->message[0]
		   && (fputs(p->message, dfile) == EOF
		       || fputs("\n", dfile) == EOF))
		  break;
	    }

	    switch(rmcat){
	      case Jo:
		if(jo == rmjolast)
		  donejo++;
		else
		  jo = (jo + 1) % RMJLEN;

		break;

	      case Lo:
		if(lo == rmlolast)
		  donelo++;
		else
		  lo = (lo + 1) % RMLLEN;

		break;

	      case Hi:
		if(hi == rmhilast)
		  donehi++;
		else
		  hi = (hi + 1) % RMHLEN;

		break;

	      default:
		donejo++;
		donelo++;
		donehi++;
		break;
	    }
	}
    }
}

#endif /* DEBUG */


/*----------------------------------------------------------------------
     Add a message to the circular status message review buffer

    Args: message  -- The message to add
  -----*/
void
add_review_message(char *message, int level)
{
    int   next_is_continuation = 0, cur_is_continuation = 0;
    char *p, *q;
    static unsigned long rmseq = 0L;

    if(rm_not_right_now || !(message && *message))
      return;

    /*
     * Debug output can have newlines in it, so split up each newline piece
     * by hand and make them separate messages.
     */
    rm_not_right_now = 1;
    for(p = message; *p; p = (*q && !next_is_continuation) ? q+1 : q){
	for(q = p; *q && *q != '\n' && (q-p) < RMMSGLEN; q++)
	  ;

	if(p == q)
	  continue;

	cur_is_continuation = next_is_continuation;

	if((q-p) == RMMSGLEN && *q && *q != '\n')
	  next_is_continuation = 1;
	else
	  next_is_continuation = 0;

	if(level < 0){
	    if(rmjofirst < 0){
		rmjofirst = 0;
		rmjolast  = 0;
	    }
	    else{
		rmjolast = (rmjolast + 1) % RMJLEN;
		if(rmjolast == rmjofirst)
		  rmjofirst = (rmjofirst + 1) % RMJLEN;
	    }

	    rmjoarray[rmjolast].level = (short) level;
	    rmjoarray[rmjolast].seq   = rmseq++;
	    rmjoarray[rmjolast].continuation = cur_is_continuation ? 1 : 0;
	    memset(rmjoarray[rmjolast].message, 0, (RMMSGLEN+1)*sizeof(char));
	    strncpy(rmjoarray[rmjolast].message, p, MIN(q-p,RMMSGLEN));
#ifdef DEBUG
	    memset(rmjoarray[rmjolast].timestamp, 0, (RMTIMLEN+1)*sizeof(char));
	    strncpy(rmjoarray[rmjolast].timestamp, debug_time(0,1), RMTIMLEN);
#endif
	}
	else if(level <= 4){
	    if(rmlofirst < 0){
		rmlofirst = 0;
		rmlolast  = 0;
	    }
	    else{
		rmlolast = (rmlolast + 1) % RMLLEN;
		if(rmlolast == rmlofirst)
		  rmlofirst = (rmlofirst + 1) % RMLLEN;
	    }

	    rmloarray[rmlolast].level = (short) level;
	    rmloarray[rmlolast].seq   = rmseq++;
	    rmloarray[rmlolast].continuation = cur_is_continuation ? 1 : 0;
	    memset(rmloarray[rmlolast].message, 0, (RMMSGLEN+1)*sizeof(char));
	    strncpy(rmloarray[rmlolast].message, p, MIN(q-p,RMMSGLEN));
#ifdef DEBUG
	    memset(rmloarray[rmlolast].timestamp, 0, (RMTIMLEN+1)*sizeof(char));
	    strncpy(rmloarray[rmlolast].timestamp, debug_time(0,1), RMTIMLEN);
#endif
	}
	else{
	    if(rmhifirst < 0){
		rmhifirst = 0;
		rmhilast  = 0;
	    }
	    else{
		rmhilast = (rmhilast + 1) % RMHLEN;
		if(rmhilast == rmhifirst)
		  rmhifirst = (rmhifirst + 1) % RMHLEN;
	    }

	    rmhiarray[rmhilast].level = (short) level;
	    rmhiarray[rmhilast].seq   = rmseq++;
	    rmhiarray[rmhilast].continuation = cur_is_continuation ? 1 : 0;
	    memset(rmhiarray[rmhilast].message, 0, (RMMSGLEN+1)*sizeof(char));
	    strncpy(rmhiarray[rmhilast].message, p, MIN(q-p,RMMSGLEN));
#ifdef DEBUG
	    memset(rmhiarray[rmhilast].timestamp, 0, (RMTIMLEN+1)*sizeof(char));
	    strncpy(rmhiarray[rmhilast].timestamp, debug_time(0,1), RMTIMLEN);
#endif
	}
    }

    rm_not_right_now = 0;
}
