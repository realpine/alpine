#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: hist.c 807 2007-11-09 01:21:33Z hubert@u.washington.edu $";
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
#include "../pith/conf.h"
#include "../pith/hist.h"


void
init_hist(HISTORY_S **history, int histsize)
{
    size_t l;
    
    if(!history)
      return;

    if(!*history){
	l = sizeof(**history) + histsize * sizeof(ONE_HIST_S);
	*history = (HISTORY_S *) fs_get(l);
	memset(*history, 0, l);
	(*history)->histsize = histsize;
	(*history)->origindex = histsize - 1;
	add_to_histlist(history);
    }

    (*history)->curindex = (*history)->origindex;
}


void
free_hist(HISTORY_S **history)
{
    int i;

    if(history && *history){

	for(i = 0; i < (*history)->histsize; i++)
	  if((*history)->hist[i] && (*history)->hist[i]->str)
	    fs_give((void **) &(*history)->hist[i]->str);

	fs_give((void **) history);
    }
}


char *
get_prev_hist(HISTORY_S *history, char *savethis, unsigned saveflags, void *cntxt)
{
    int nextcurindex;
    size_t l;

    if(!(history && history->histsize > 0))
      return NULL;

    nextcurindex = (history->curindex + 1) % history->histsize;

    /* already at start of history */
    if(nextcurindex == history->origindex
       || !(history->hist[nextcurindex] && history->hist[nextcurindex]->str
            && history->hist[nextcurindex]->str[0]))
      return NULL;

    /* save what user typed */
    if(history->curindex == history->origindex){
	if(!savethis)
	  savethis = "";

	if(!history->hist[history->origindex]){
	    history->hist[history->origindex] = (ONE_HIST_S *) fs_get(sizeof(ONE_HIST_S));
	    memset(history->hist[history->origindex], 0, sizeof(ONE_HIST_S));
	}

	if(history->hist[history->origindex]->str){
	    if(strlen(history->hist[history->origindex]->str) < (l=strlen(savethis)))
	      fs_resize((void **) &history->hist[history->origindex]->str, l+1);

	    strncpy(history->hist[history->origindex]->str, savethis, l+1);
	    history->hist[history->origindex]->str[l] = '\0';
	}
	else
	  history->hist[history->origindex]->str = cpystr(savethis);

	history->hist[history->origindex]->flags = saveflags;

	history->hist[history->origindex]->cntxt = cntxt;
    }

    history->curindex = nextcurindex;

    return((history->hist[history->curindex] && history->hist[history->curindex]->str)
           ? history->hist[history->curindex]->str : NULL);
}


char *
get_next_hist(HISTORY_S *history, char *savethis, unsigned saveflags, void *cntxt)
{
    if(!(history && history->histsize > 0))
      return NULL;

    /* already at end (most recent) of history */
    if(history->curindex == history->origindex)
      return NULL;

    history->curindex = (history->curindex + history->histsize - 1) % history->histsize;

    return((history->hist[history->curindex] && history->hist[history->curindex]->str)
           ? history->hist[history->curindex]->str : NULL);
}


void
save_hist(HISTORY_S *history, char *savethis, unsigned saveflags, void *cntxt)
{
    size_t l;
    int plusone;

    if(!(history && history->histsize > 0))
      return;

    plusone = (history->origindex + 1) % history->histsize;

    if(!history->hist[history->origindex]){
	history->hist[history->origindex] = (ONE_HIST_S *) fs_get(sizeof(ONE_HIST_S));
	memset(history->hist[history->origindex], 0, sizeof(ONE_HIST_S));
    }

    if(savethis && savethis[0]
       && (!history->hist[history->origindex]->str
           || strcmp(history->hist[history->origindex]->str, savethis)
	   || history->hist[history->origindex]->flags != saveflags
	   || history->hist[history->origindex]->cntxt != cntxt)
       && !(history->hist[plusone] && history->hist[plusone]->str
	    && !strcmp(history->hist[plusone]->str, savethis)
	    && history->hist[history->origindex]->flags == saveflags
	    && history->hist[history->origindex]->cntxt == cntxt)){
	if(history->hist[history->origindex]->str){
	    if(strlen(history->hist[history->origindex]->str) < (l=strlen(savethis)))
	      fs_resize((void **) &history->hist[history->origindex]->str, l+1);

	    strncpy(history->hist[history->origindex]->str, savethis, l+1);
	    history->hist[history->origindex]->str[l] = '\0';
	}
	else{
	    history->hist[history->origindex]->str = cpystr(savethis);
	}

	history->hist[history->origindex]->flags = saveflags;
	history->hist[history->origindex]->cntxt = cntxt;

	history->origindex = (history->origindex + history->histsize - 1) % history->histsize;
	if(history->hist[history->origindex] && history->hist[history->origindex]->str)
	  history->hist[history->origindex]->str[0] = '\0';
    }
}


/*
 * Returns count of items entered into history.
 */
int
items_in_hist(HISTORY_S *history)
{
    int i, cnt = 0;

    if(history && history->histsize > 0)
      for(i = 0; i < history->histsize; i++)
        if(history->hist[i] && history->hist[i]->str)
	  cnt++;

    return(cnt);
}


static HISTORY_S **histlist[100];

void
add_to_histlist(HISTORY_S **history)
{
    int i;

    if(history){
	/* find empty slot */
	for(i = 0; i < 100; i++)
	  if(!histlist[i])
	    break;

	if(i < 100)
	  histlist[i] = history;
    }
}


void
free_histlist(void)
{
    int i;

    for(i = 0; i < 100; i++)
      if(histlist[i])
	free_hist(histlist[i]);
}
