#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: news.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
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
#include "../pith/news.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/context.h"
#include "../pith/stream.h"
#include "../pith/util.h"


typedef enum {NotChecked, NotInCache, Found, Missing, End} NgCacheReturns;


/*
 * Internal prototypes
 */
NgCacheReturns chk_newsgrp_cache(char *);
void           add_newsgrp_cache(char *, NgCacheReturns);


/*----------------------------------------------------------------------
  Function to see if a given MAILSTREAM mailbox is in the news namespace

  Input: stream -- mail stream to test

 Result: 
  ----*/
int
ns_test(char *mailbox, char *namespace)
{
    if(mailbox){
	switch(*mailbox){
	  case '#' :
	    return(!struncmp(mailbox + 1, namespace, strlen(namespace)));

	  case '{' :
	  {
	      NETMBX mbx;

	      if(mail_valid_net_parse(mailbox, &mbx))
		return(ns_test(mbx.mailbox, namespace));
	  }

	  break;

	  default :
	    break;
	}
    }

    return(0);
}


int
news_in_folders(struct variable *var)
{
    int        i, found_news = 0;
    CONTEXT_S *tc;

    if(!(var && var->current_val.l))
      return(found_news);

    for(i=0; !found_news && var->current_val.l[i]; i++){
	if((tc = new_context(var->current_val.l[i], NULL)) != NULL){
	    if(tc->use & CNTXT_NEWS)
	      found_news++;
	    
	    free_context(&tc);
	}
    }

    return(found_news);
}


/*----------------------------------------------------------------------
    Verify and canonicalize news groups names. 
    Called from the message composer

Args:  given_group    -- List of groups typed by user
       expanded_group -- pointer to point to expanded list, which will be
			 allocated here and freed in caller.  If this is
			 NULL, don't attempt to validate.
       error          -- pointer to store error message
       fcc            -- pointer to point to fcc, which will be
			 allocated here and freed in caller

Returns:  0 if all is OK
         -1 if addresses weren't valid

Test the given list of newstroups against those recognized by our nntp
servers.  Testing by actually trying to open the list is much cheaper, both
in bandwidth and memory, than yanking the whole list across the wire.
  ----*/
int
news_grouper(char *given_group, char **expanded_group, char **error,
	     char **fccptr, void (*delay_warning)(void))
{
    char	 ng_error[90], *p1, *p2, *name, *end, *ep, **server,
		 ng_ref[MAILTMPLEN];
    int          expanded_len = 0, num_in_error = 0, cnt_errs;

    MAILSTREAM  *stream = NULL;
    struct ng_list {
	char  *groupname;
	NgCacheReturns  found;
	struct ng_list *next;
    }*nglist = NULL, **ntmpp, *ntmp;
#ifdef SENDNEWS
    static int no_servers = 0;
#endif

    dprint((5,
	"- news_build - (%s)\n", given_group ? given_group : "nul"));

    if(error)
      *error = NULL;

    ng_ref[0] = '\0';

    /*------ parse given entries into a list ----*/
    ntmpp = &nglist;
    for(name = given_group; *name; name = end){

	/* find start of next group name */
        while(*name && (isspace((unsigned char)*name) || *name == ','))
	  name++;

	/* find end of group name */
	end = name;
	while(*end && !isspace((unsigned char)*end) && *end != ',')
	  end++;

        if(end != name){
	    *ntmpp = (struct ng_list *)fs_get(sizeof(struct ng_list));
	    (*ntmpp)->next      = NULL;
	    (*ntmpp)->found     = NotChecked;
            (*ntmpp)->groupname = fs_get(end - name + 1);
            strncpy((*ntmpp)->groupname, name, end - name);
            (*ntmpp)->groupname[end - name] = '\0';
	    ntmpp = &(*ntmpp)->next;
	    if(!expanded_group)
	      break;  /* no need to continue if just doing fcc */
        }
    }

    /*
     * If fcc is not set or is set to default, then replace it if
     * one of the recipient rules is in effect.
     */
    if(fccptr){
	if((ps_global->fcc_rule == FCC_RULE_RECIP ||
	    ps_global->fcc_rule == FCC_RULE_NICK_RECIP) &&
	       (nglist && nglist->groupname)){
	  if(*fccptr)
	    fs_give((void **) fccptr);

	  *fccptr = cpystr(nglist->groupname);
	}
	else if(!*fccptr) /* already default otherwise */
	  *fccptr = cpystr(ps_global->VAR_DEFAULT_FCC);
    }

    if(!nglist){
	if(expanded_group)
	  *expanded_group = cpystr("");
        return 0;
    }

    if(!expanded_group)
      return 0;

#ifdef	DEBUG
    for(ntmp = nglist; debug >= 9 && ntmp; ntmp = ntmp->next)
      dprint((9, "Parsed group: --[%s]--\n",
	     ntmp->groupname ? ntmp->groupname : "?"));
#endif

    /* If we are doing validation */
    if(F_OFF(F_NO_NEWS_VALIDATION, ps_global)){
	int need_to_talk_to_server = 0;

	/*
	 * First check our cache of validated newsgroups to see if we even
	 * have to open a stream.
	 */
	for(ntmp = nglist; ntmp; ntmp = ntmp->next){
	    ntmp->found = chk_newsgrp_cache(ntmp->groupname);
	    if(ntmp->found == NotInCache)
	      need_to_talk_to_server++;
	}

	if(need_to_talk_to_server){

#ifdef SENDNEWS
	  if(no_servers == 0)
#endif
	    if(delay_warning)
	      (*delay_warning)();
	  
	    /*
	     * Build a stream to the first server that'll talk to us...
	     */
	    for(server = ps_global->VAR_NNTP_SERVER;
		server && *server && **server;
		server++){
		snprintf(ng_ref, sizeof(ng_ref), "{%.*s/nntp}#news.",
			sizeof(ng_ref)-30, *server);
		if((stream = pine_mail_open(stream, ng_ref,
					   OP_HALFOPEN|SP_USEPOOL|SP_TEMPUSE,
					   NULL)) != NULL)
		  break;
	    }
	    if(!server || !stream){
		if(error)
#ifdef SENDNEWS
		{
		 /* don't say this over and over */
		 if(no_servers == 0){
		    if(!server || !*server || !**server)
		      no_servers++;

		    *error = cpystr(no_servers
			    /* TRANSLATORS: groups refers to news groups */
			    ? _("Can't validate groups.  No servers defined")
			    /* TRANSLATORS: groups refers to news groups */
			    : _("Can't validate groups.  No servers responding"));
		 }
		}
#else
		  *error = cpystr((!server || !*server || !**server)
			    ? _("No servers defined for posting to newsgroups")
			    /* TRANSLATORS: groups refers to news groups */
			    : _("Can't validate groups.  No servers responding"));
#endif
		*expanded_group = cpystr(given_group);
		goto done;
	    }
	}

	/*
	 * Now, go thru the list, making sure we can at least open each one...
	 */
	for(server = ps_global->VAR_NNTP_SERVER;
	    server && *server && **server; server++){
	    /*
	     * It's faster and easier right now just to open the stream and
	     * do our own finds than to use the current folder_exists()
	     * interface...
	     */
	    for(ntmp = nglist; ntmp; ntmp = ntmp->next){
	        if(ntmp->found == NotInCache){
		  snprintf(ng_ref, sizeof(ng_ref), "{%.*s/nntp}#news.%.*s", 
			  sizeof(ng_ref)/2 - 10, *server,
			  sizeof(ng_ref)/2 - 10, ntmp->groupname);
		  ps_global->noshow_error = 1;
		  stream = pine_mail_open(stream, ng_ref,
					  OP_SILENT|SP_USEPOOL|SP_TEMPUSE,
					  NULL);
		  ps_global->noshow_error = 0;
		  if(stream)
		    add_newsgrp_cache(ntmp->groupname, ntmp->found = Found);
		}

	    }

	    if(stream){
		pine_mail_close(stream);
		stream = NULL;
	    }

	}

    }

    /* figure length of string for matching groups */
    for(ntmp = nglist; ntmp; ntmp = ntmp->next){
      if(ntmp->found == Found || F_ON(F_NO_NEWS_VALIDATION, ps_global))
	expanded_len += strlen(ntmp->groupname) + 2;
      else{
	num_in_error++;
	if(ntmp->found == NotInCache)
	  add_newsgrp_cache(ntmp->groupname, ntmp->found = Missing);
      }
    }

    /*
     * allocate and write the allowed, and error lists...
     */
    p1 = *expanded_group = fs_get((expanded_len + 1) * sizeof(char));
    if(error && num_in_error){
	cnt_errs = num_in_error;
	memset((void *)ng_error, 0, sizeof(ng_error));
	snprintf(ng_error, sizeof(ng_error), "Unknown news group%s: ", plural(num_in_error));
	ep = ng_error + strlen(ng_error);
    }
    for(ntmp = nglist; ntmp; ntmp = ntmp->next){
	p2 = ntmp->groupname;
	if(ntmp->found == Found || F_ON(F_NO_NEWS_VALIDATION, ps_global)){
	    while(*p2)
	      *p1++ = *p2++;

	    if(ntmp->next){
		*p1++ = ',';
		*p1++ = ' ';
	    }
	}
	else if (error){
	    while(*p2 && (ep - ng_error < sizeof(ng_error)-1))
	      *ep++ = *p2++;

	    if(--cnt_errs > 0 && (ep - ng_error < sizeof(ng_error)-3)){
		strncpy(ep, ", ", sizeof(ng_error)-(ep-ng_error));
		ep += 2;
	    }
	}
    }

    *p1 = '\0';

    if(error && num_in_error)
      *error = cpystr(ng_error);

done:
    while((ntmp = nglist) != NULL){
	nglist = nglist->next;
	fs_give((void **)&ntmp->groupname);
	fs_give((void **)&ntmp);
    }

    return(num_in_error ? -1 : 0);
}


typedef struct ng_cache {
    char          *name;
    NgCacheReturns val;
}NgCache;

static NgCache *ng_cache_ptr;
#if defined(DOS) && !defined(_WINDOWS)
#define MAX_NGCACHE_ENTRIES 15
#else
#define MAX_NGCACHE_ENTRIES 40
#endif
/*
 * Simple newsgroup validity cache.  Opening a newsgroup to see if it
 * exists can be very slow on a heavily loaded NNTP server, so we cache
 * the results.
 */
NgCacheReturns
chk_newsgrp_cache(char *group)
{
    register NgCache *ngp;
    
    for(ngp = ng_cache_ptr; ngp && ngp->name; ngp++){
	if(strcmp(group, ngp->name) == 0)
	  return(ngp->val);
    }

    return NotInCache;
}


/*
 * Add an entry to the newsgroup validity cache.
 *
 * LRU entry is the one on the bottom, oldest on the top.
 * A slot has an entry in it if name is not NULL.
 */
void
add_newsgrp_cache(char *group, NgCacheReturns result)
{
    register NgCache *ngp;
    NgCache save_ngp;

    /* first call, initialize cache */
    if(!ng_cache_ptr){
	int i;

	ng_cache_ptr =
	    (NgCache *)fs_get((MAX_NGCACHE_ENTRIES+1)*sizeof(NgCache));
	for(i = 0; i <= MAX_NGCACHE_ENTRIES; i++){
	    ng_cache_ptr[i].name = NULL;
	    ng_cache_ptr[i].val  = NotInCache;
	}
	ng_cache_ptr[MAX_NGCACHE_ENTRIES].val  = End;
    }

    if(chk_newsgrp_cache(group) == NotInCache){
	/* find first empty slot or End */
	for(ngp = ng_cache_ptr; ngp->name; ngp++)
	  ;/* do nothing */
	if(ngp->val == End){
	    /*
	     * Cache is full, throw away top entry, move everything up,
	     * and put new entry on the bottom.
	     */
	    ngp = ng_cache_ptr;
	    if(ngp->name) /* just making sure */
	      fs_give((void **)&ngp->name);

	    for(; (ngp+1)->name; ngp++){
		ngp->name = (ngp+1)->name;
		ngp->val  = (ngp+1)->val;
	    }
	}
	ngp->name = cpystr(group);
	ngp->val  = result;
    }
    else{
	/*
	 * Move this entry from current location to last to preserve
	 * LRU order.
	 */
	for(ngp = ng_cache_ptr; ngp && ngp->name; ngp++){
	    if(strcmp(group, ngp->name) == 0) /* found it */
	      break;
	}
	save_ngp.name = ngp->name;
	save_ngp.val  = ngp->val;
	for(; (ngp+1)->name; ngp++){
	    ngp->name = (ngp+1)->name;
	    ngp->val  = (ngp+1)->val;
	}
	ngp->name = save_ngp.name;
	ngp->val  = save_ngp.val;
    }
}


void
free_newsgrp_cache(void)
{
    register NgCache *ngp;

    for(ngp = ng_cache_ptr; ngp && ngp->name; ngp++)
      fs_give((void **)&ngp->name);
    if(ng_cache_ptr)
      fs_give((void **)&ng_cache_ptr);
}
