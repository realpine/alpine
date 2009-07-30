#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: keyword.c 807 2007-11-09 01:21:33Z hubert@u.washington.edu $";
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
#include "../pith/keyword.h"
#include "../pith/state.h"
#include "../pith/flag.h"
#include "../pith/string.h"
#include "../pith/status.h"
#include "../pith/util.h"


/*
 * Internal prototypes
 */


/*
 * Read the keywords array into a KEYWORD_S structure.
 * Make sure that all of the strings are UTF-8.
 */
KEYWORD_S *
init_keyword_list(char **keywordarray)
{
    char     **t, *nickname, *keyword;
    KEYWORD_S *head = NULL, **tail;

    tail = &head;
    for(t = keywordarray; t && *t && **t; t++){
	nickname = keyword = NULL;
	get_pair(*t, &nickname, &keyword, 0, 0);
	*tail = new_keyword_s(keyword, nickname);
	tail  = &(*tail)->next;

	if(keyword)
	  fs_give((void **) &keyword);

	if(nickname)
	  fs_give((void **) &nickname);
    }

    return(head);
}


KEYWORD_S *
new_keyword_s(char *keyword, char *nickname)
{
    KEYWORD_S *kw = NULL;

    kw = (KEYWORD_S *) fs_get(sizeof(*kw));
    memset(kw, 0, sizeof(*kw));

    if(keyword && *keyword)
      kw->kw = cpystr(keyword);

    if(nickname && *nickname)
      kw->nick = cpystr(nickname);
    
    return(kw);
}


void
free_keyword_list(KEYWORD_S **kl)
{
    if(kl && *kl){
	if((*kl)->next)
	  free_keyword_list(&(*kl)->next);

	if((*kl)->kw)
	  fs_give((void **) &(*kl)->kw);

	if((*kl)->nick)
	  fs_give((void **) &(*kl)->nick);

	fs_give((void **) kl);
    }
}


/*
 * Return a pointer to the keyword associated with a nickname, or the
 * input itself if no match.
 */
char *
nick_to_keyword(char *nick)
{
    KEYWORD_S *kw;
    char      *ret;

    ret = nick;
    for(kw = ps_global->keywords; kw; kw = kw->next)
      if(!strcmp(nick, kw->nick ? kw->nick : kw->kw ? kw->kw : "")){
	  if(kw->nick)
	    ret = kw->kw;

	  break;
      }
    
    return(ret);
}


/*
 * Return a pointer to the nickname associated with a keyword, or the
 * input itself if no match.
 */
char *
keyword_to_nick(char *keyword)
{
    KEYWORD_S *kw;
    char      *ret;

    ret = keyword;
    for(kw = ps_global->keywords; kw; kw = kw->next)
      if(!strcmp(keyword, kw->kw ? kw->kw : "")){
	  if(kw->nick)
	    ret = kw->nick;

	  break;
      }
    
    return(ret);
}


/*
 * Return a pointer to the keyword associated with an initial, or the
 * input itself if no match.
 * Only works for ascii initials.
 */
char *
initial_to_keyword(char *initial)
{
    KEYWORD_S *kw;
    char      *ret;
    int        init;
    int        kwinit;

    ret = initial;
    if(initial[0] && initial[1] == '\0'){
	init = initial[0];
	if(isascii(init) && isupper(init))
	  init = tolower((unsigned char) init);

	for(kw = ps_global->keywords; kw; kw = kw->next){
	    if(kw->nick)
	      kwinit = kw->nick[0];
	    else if(kw->kw)
	      kwinit = kw->kw[0];

	    if(isascii(kwinit) && isupper(kwinit))
	      kwinit = tolower((unsigned char) kwinit);

	    if(kwinit == init){
		ret = kw->kw;
		break;
	    }
	}
    }
    
    return(ret);
}


int
user_flag_is_set(MAILSTREAM *stream, long unsigned int rawno, char *keyword)
{
    int           j, is_set = 0;
    MESSAGECACHE *mc;

    if(stream && keyword && keyword[0]){
	if(rawno > 0L && stream
	   && rawno <= stream->nmsgs
	   && (mc = mail_elt(stream, rawno)) != NULL){
	    j = user_flag_index(stream, keyword);
	    if(j >= 0 && j < NUSERFLAGS && ((1 << j) & mc->user_flags))
	      is_set++;
	}
    }
	
    return(is_set);
}


/*
 * Returns the bit position of the keyword in stream, else -1.
 */
int
user_flag_index(MAILSTREAM *stream, char *keyword)
{
    int i, retval = -1;
    char *p;

    if(stream && keyword && keyword[0]){
	for(i = 0; i < NUSERFLAGS; i++)
	  if((p=stream_to_user_flag_name(stream, i)) && !strucmp(keyword, p)){
	      retval = i;
	      break;
	  }
    }

    return(retval);
}


char *
stream_to_user_flag_name(MAILSTREAM *stream, int k)
{
    if(stream && stream->user_flags && k >= 0 && k < NUSERFLAGS)
      return(stream->user_flags[k]);

    return(NULL);
}


int
some_user_flags_defined(MAILSTREAM *stream)
{
    int k;

    if(stream)
      for(k = 0; k < NUSERFLAGS; k++)
	if(stream_to_user_flag_name(stream, k))
	  return 1;

    return 0;
}


/*----------------------------------------------------------------------
  Build flags string based on requested flags and what's set in messagecache

   Args: flags -- flags to test

 Result: allocated, space-delimited flags string is returned
 ----*/
char *
flag_string(MAILSTREAM *stream, long rawno, long int flags)
{
    MESSAGECACHE *mc;
    char *p, *q, *returned_flags = NULL;
    size_t len = 0;
    int k;

    mc = (rawno > 0L && stream && rawno <= stream->nmsgs)
			    ? mail_elt(stream, rawno) : NULL;
    if(!mc)
      return returned_flags;

    if((flags & F_DEL) && mc->deleted)
      len += strlen("\\DELETED") + 1;

    if((flags & F_ANS) && mc->answered)
      len += strlen("\\ANSWERED") + 1;

    if((flags & F_FWD) && user_flag_is_set(stream, rawno, FORWARDED_FLAG))
      len += strlen(FORWARDED_FLAG) + 1;

    if((flags & F_FLAG) && mc->flagged)
      len += strlen("\\FLAGGED") + 1;

    if((flags & F_SEEN) && mc->seen)
      len += strlen("\\SEEN") + 1;

    if((flags & F_KEYWORD) && stream->user_flags){
	for(k = 0; k < NUSERFLAGS; k++){
	    if((q=stream_to_user_flag_name(stream, k))
	       && user_flag_is_set(stream, rawno, q)){
		len += strlen(q) + 1;
	    }
	}
    }

    returned_flags = (char *) fs_get((len+1) * sizeof(char));
    p = returned_flags;
    *p = '\0';

    if((flags & F_DEL) && mc->deleted)
      sstrncpy(&p, "\\DELETED ", len+1-(p-returned_flags));

    if((flags & F_ANS) && mc->answered)
      sstrncpy(&p, "\\ANSWERED ", len+1-(p-returned_flags));

    if((flags & F_FWD) && user_flag_is_set(stream, rawno, FORWARDED_FLAG)){
	sstrncpy(&p, FORWARDED_FLAG, len+1-(p-returned_flags));
	sstrncpy(&p, " ", len+1-(p-returned_flags));
    }

    if((flags & F_FLAG) && mc->flagged)
      sstrncpy(&p, "\\FLAGGED ", len+1-(p-returned_flags));

    if((flags & F_SEEN) && mc->seen)
      sstrncpy(&p, "\\SEEN ", len+1-(p-returned_flags));

    if((flags & F_KEYWORD) && stream->user_flags){
	for(k = 0; k < NUSERFLAGS; k++){
	    if((q=stream_to_user_flag_name(stream, k))
	       && user_flag_is_set(stream, rawno, q)){
		sstrncpy(&p, q, len+1-(p-returned_flags));
		sstrncpy(&p, " ", len+1-(p-returned_flags));
		len += strlen(p) + 1;
	    }
	}
    }

    if(p != returned_flags && (len+1-(p-returned_flags)>0))
      *--p = '\0';

    return returned_flags;
}


long
get_msgno_by_msg_id(MAILSTREAM *stream, char *message_id, MSGNO_S *msgmap)
{
    SEARCHPGM  *pgm = NULL;
    long        hint = mn_m2raw(msgmap, mn_get_cur(msgmap));
    long        newmsgno = -1L;
    int         iter = 0;
    MESSAGECACHE *mc;
    extern MAILSTREAM *mm_search_stream;
    extern long        mm_search_count;

    if(!(message_id && message_id[0]) || stream->nmsgs < 1L)
      return(newmsgno);

    mm_search_count = 0L;
    mm_search_stream = stream;
    while(mm_search_count == 0L && iter++ < 3
	  && (pgm = mail_newsearchpgm()) != NULL){
	pgm->message_id = mail_newstringlist();
	pgm->message_id->text.data = (unsigned char *) cpystr(message_id);
	pgm->message_id->text.size = strlen(message_id);

	if(iter > 1 || hint > stream->nmsgs)
	  iter++;

	if(iter == 1){
	    /* restrict to hint message on first try */
	    pgm->msgno = mail_newsearchset();
	    pgm->msgno->first = pgm->msgno->last = hint;
	}
	else if(iter == 2){
	    /* restrict to last 50 messages on 2nd try */
	    pgm->msgno = mail_newsearchset();
	    if(stream->nmsgs > 100L)
	      pgm->msgno->first = stream->nmsgs-50L;
	    else{
		pgm->msgno->first = 1L;
		iter++;
	    }

	    pgm->msgno->last = stream->nmsgs;
	}

	pine_mail_search_full(stream, NULL, pgm, SE_NOPREFETCH | SE_FREE);

	if(mm_search_count){
	    for(newmsgno=stream->nmsgs; newmsgno > 0L; newmsgno--)
	      if((mc = mail_elt(stream, newmsgno)) && mc->searched)
		break;
	}
    }

    return(mn_raw2m(msgmap, newmsgno));
}


/*
 * These chars are not allowed in keywords.
 *
 * Returns 0 if ok, 1 if not.
 * Returns an allocated error message on error.
 */
int
keyword_check(char *kw, char **error)
{
    register char *t;
    char buf[100], *p;

    if(!kw || !kw[0])
      return 1;

    kw = nick_to_keyword(kw);

    for(p = kw; p && *p; p++)
      if(!isascii(*p)){
	if(error)
	  *error = cpystr("Keywords must be all ASCII characters");

	return 1;
      }

    if((t = strindex(kw, SPACE)) ||
       (t = strindex(kw, '{'))   ||
       (t = strindex(kw, '('))   ||
       (t = strindex(kw, ')'))   ||
       (t = strindex(kw, ']'))   ||
       (t = strindex(kw, '%'))   ||
       (t = strindex(kw, '"'))   ||
       (t = strindex(kw, '\\'))  ||
       (t = strindex(kw, '*'))){
	char s[4];
	s[0] = '"';
	s[1] = *t;
	s[2] = '"';
	s[3] = '\0';
	if(error){
	    snprintf(buf, sizeof(buf), "%s not allowed in keywords",
		*t == SPACE ?
		    "Spaces" :
		    *t == '"' ?
			"Quotes" :
			*t == '%' ?
			    "Percents" :
			    s);
	    *error = cpystr(buf);
	}

	return 1;
    }

    return 0;
}
