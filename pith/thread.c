#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: thread.c 942 2008-03-04 18:21:33Z hubert@u.washington.edu $";
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
#include "../pith/thread.h"
#include "../pith/flag.h"
#include "../pith/icache.h"
#include "../pith/mailindx.h"
#include "../pith/msgno.h"
#include "../pith/sort.h"
#include "../pith/pineelt.h"
#include "../pith/status.h"
#include "../pith/news.h"
#include "../pith/search.h"
#include "../pith/mailcmd.h"
#include "../pith/ablookup.h"


/*
 * Internal prototypes
 */
long *sort_thread_flatten(THREADNODE *, MAILSTREAM *, long *,
			  char *, long, PINETHRD_S *, unsigned);
void		   make_thrdflags_consistent(MAILSTREAM *, MSGNO_S *, PINETHRD_S *, int);
THREADNODE	  *collapse_threadnode_tree(THREADNODE *);
THREADNODE	  *collapse_threadnode_tree_sorted(THREADNODE *);
THREADNODE	  *sort_threads_and_collapse(THREADNODE *);
THREADNODE        *insert_tree_in_place(THREADNODE *, THREADNODE *);
unsigned long      branch_greatest_num(THREADNODE *, int);
long		   calculate_visible_threads(MAILSTREAM *);


PINETHRD_S *
fetch_thread(MAILSTREAM *stream, long unsigned int rawno)
{
    MESSAGECACHE *mc;
    PINELT_S     *pelt;
    PINETHRD_S   *thrd = NULL;

    if(stream && rawno > 0L && rawno <= stream->nmsgs
       && !sp_need_to_rethread(stream)){
	mc = (rawno > 0L && stream && rawno <= stream->nmsgs)
	        ? mail_elt(stream, rawno) : NULL;
	if(mc && (pelt = (PINELT_S *) mc->sparep))
	  thrd = pelt->pthrd;
    }

    return(thrd);
}


PINETHRD_S *
fetch_head_thread(MAILSTREAM *stream)
{
    unsigned long rawno;
    PINETHRD_S   *thrd = NULL;

    if(stream){
	/* first find any thread */
	for(rawno = 1L; !thrd && rawno <= stream->nmsgs; rawno++)
	  thrd = fetch_thread(stream, rawno);

	if(thrd && thrd->head)
	  thrd = fetch_thread(stream, thrd->head);
    }

    return(thrd);
}


/*
 * Set flag f to v for all messages in thrd.
 *
 * Watch out when calling this. The thrd->branch is not part of thrd.
 * Branch is a sibling to thrd, not a child. Zero out branch before calling
 * or call on thrd->next and worry about thrd separately.
 * Ok to call it on top-level thread which has no branch already.
 */
void
set_flags_for_thread(MAILSTREAM *stream, MSGNO_S *msgmap, int f, PINETHRD_S *thrd, int v)
{
    PINETHRD_S *nthrd, *bthrd;

    if(!(stream && thrd && msgmap))
      return;

    set_lflag(stream, msgmap, mn_raw2m(msgmap, thrd->rawno), f, v);

    if(thrd->next){
	nthrd = fetch_thread(stream, thrd->next);
	if(nthrd)
	  set_flags_for_thread(stream, msgmap, f, nthrd, v);
    }

    if(thrd->branch){
	bthrd = fetch_thread(stream, thrd->branch);
	if(bthrd)
	  set_flags_for_thread(stream, msgmap, f, bthrd, v);
    }
}


void
erase_threading_info(MAILSTREAM *stream, MSGNO_S *msgmap)
{
    unsigned long n;
    MESSAGECACHE *mc;
    PINELT_S     *peltp;

    if(!(stream && stream->spare))
      return;
    
    ps_global->view_skipped_index = 0;
    sp_set_viewing_a_thread(stream, 0);
    
    if(THRD_INDX())
      setup_for_thread_index_screen();
    else
      setup_for_index_index_screen();

    stream->spare = 0;

    for(n = 1L; n <= stream->nmsgs; n++){
	set_lflag(stream, msgmap, mn_raw2m(msgmap, n),
		  MN_COLL | MN_CHID | MN_CHID2, 0);
	mc = mail_elt(stream, n);
	if(mc && mc->sparep){
	    peltp = (PINELT_S *) mc->sparep;
	    if(peltp->pthrd)
	      fs_give((void **) &peltp->pthrd);
	}
    }
}


void
sort_thread_callback(MAILSTREAM *stream, THREADNODE *tree)
{
    THREADNODE *collapsed_tree = NULL;
    PINETHRD_S   *thrd = NULL;
    unsigned long msgno, rawno;
    int           un_view_thread = 0;
    long          raw_current;
    char         *dup_chk = NULL;


    dprint((2, "sort_thread_callback\n"));

    g_sort.msgmap->max_thrdno = 0L;

    /*
     * Eliminate dummy nodes from tree and collapse the tree in a logical
     * way. If the dummy node is at the top-level, then its children are
     * promoted to the top-level as separate threads.
     */
    if(F_ON(F_THREAD_SORTS_BY_ARRIVAL, ps_global))
      collapsed_tree = collapse_threadnode_tree_sorted(tree);
    else
      collapsed_tree = collapse_threadnode_tree(tree);

    /* dup_chk is like sort with an origin of 1 */
    dup_chk = (char *) fs_get((mn_get_nmsgs(g_sort.msgmap)+1) * sizeof(char));
    memset(dup_chk, 0, (mn_get_nmsgs(g_sort.msgmap)+1) * sizeof(char));

    memset(&g_sort.msgmap->sort[1], 0, mn_get_total(g_sort.msgmap) * sizeof(long));

    (void) sort_thread_flatten(collapsed_tree, stream,
			       &g_sort.msgmap->sort[1],
			       dup_chk, mn_get_nmsgs(g_sort.msgmap),
			       NULL, THD_TOP);

    /* reset the inverse array */
    msgno_reset_isort(g_sort.msgmap);

    if(dup_chk)
      fs_give((void **) &dup_chk);

    if(collapsed_tree)
      mail_free_threadnode(&collapsed_tree);

    if(any_lflagged(g_sort.msgmap, MN_HIDE))
      g_sort.msgmap->visible_threads = calculate_visible_threads(stream);
    else
      g_sort.msgmap->visible_threads = g_sort.msgmap->max_thrdno;

    raw_current = mn_m2raw(g_sort.msgmap, mn_get_cur(g_sort.msgmap));

    sp_set_need_to_rethread(stream, 0);

    /*
     * Set appropriate bits to start out collapsed if desired. We use the
     * stream spare bit to tell us if we've done this before for this
     * stream.
     */
    if(!stream->spare
       && (COLL_THRDS() || SEP_THRDINDX())
       && mn_get_total(g_sort.msgmap) > 1L){

	collapse_threads(stream, g_sort.msgmap, NULL);
    }
    else if(stream->spare){
	
	/*
	 * If we're doing auto collapse then new threads need to have
	 * their collapse bit set. This happens below if we're in the
	 * thread index, but if we're in the regular index with auto
	 * collapse we have to look for these.
	 */
	if(any_lflagged(g_sort.msgmap, MN_USOR)){
	    if(COLL_THRDS()){
		for(msgno = 1L; msgno <= mn_get_total(g_sort.msgmap); msgno++){
		    rawno = mn_m2raw(g_sort.msgmap, msgno);
		    if(get_lflag(stream, NULL, rawno, MN_USOR)){
			thrd = fetch_thread(stream, rawno);

			/*
			 * Node is new, unsorted, top-level thread,
			 * and we're using auto collapse.
			 */
			if(thrd && !thrd->parent)
			  set_lflag(stream, g_sort.msgmap, msgno, MN_COLL, 1);
			
			/*
			 * If a parent is collapsed, clear that parent's
			 * index cache entry. This is only necessary if
			 * the parent's index display can depend on its
			 * children, of course.
			 */
			if(thrd && thrd->parent){
			    thrd = fetch_thread(stream, thrd->parent);
			    while(thrd){
				long t;

				if(get_lflag(stream, NULL, thrd->rawno, MN_COLL)
				   && (t = mn_raw2m(g_sort.msgmap,
						    (long) thrd->rawno)))
				  clear_index_cache_ent(stream, t, 0);

				if(thrd->parent)
				  thrd = fetch_thread(stream, thrd->parent);
				else
				  thrd = NULL;
			    }
			}

		    }
		}
	    }

	    set_lflags(stream, g_sort.msgmap, MN_USOR, 0);
	}

	if(sp_viewing_a_thread(stream)){
	    if(any_lflagged(g_sort.msgmap, MN_CHID2)){
		/* current should be part of viewed thread */
		if(get_lflag(stream, NULL, raw_current, MN_CHID2)){
		    thrd = fetch_thread(stream, raw_current);
		    if(thrd && thrd->top && thrd->top != thrd->rawno)
		      thrd = fetch_thread(stream, thrd->top);
		    
		    if(thrd){
			/*
			 * For messages that are part of thread set MN_CHID2
			 * and for messages that aren't part of the thread
			 * clear MN_CHID2. Easiest is to just do it instead
			 * of checking if it is true first.
			 */
			set_lflags(stream, g_sort.msgmap, MN_CHID2, 0);
			set_thread_lflags(stream, thrd, g_sort.msgmap,
					  MN_CHID2, 1);
			
			/*
			 * Outside of the viewed thread everything else
			 * should be collapsed at the top-levels.
			 */
			collapse_threads(stream, g_sort.msgmap, thrd);

			/*
			 * Inside of the thread, the top of the thread
			 * can't be hidden, the rest are hidden if a
			 * parent somewhere above them is collapsed.
			 * There can be collapse points that are hidden
			 * inside of the tree. They remain collapsed even
			 * if the parent above them uncollapses.
			 */
			msgno = mn_raw2m(g_sort.msgmap, (long) thrd->rawno);
			if(msgno)
			  set_lflag(stream, g_sort.msgmap, msgno, MN_CHID, 0);

			if(thrd->next){
			    PINETHRD_S *nthrd;

			    nthrd = fetch_thread(stream, thrd->next);
			    if(nthrd)
			      make_thrdflags_consistent(stream, g_sort.msgmap,
							nthrd,
							get_lflag(stream, NULL,
								  thrd->rawno,
								  MN_COLL));
			}
		    }
		    else
		      un_view_thread++;
		}
		else
		  un_view_thread++;
	    }
	    else
	      un_view_thread++;

	    if(un_view_thread){
		set_lflags(stream, g_sort.msgmap, MN_CHID2, 0);
		unview_thread(ps_global, stream, g_sort.msgmap);
	    }
	    else{
		mn_reset_cur(g_sort.msgmap,
			     mn_raw2m(g_sort.msgmap, raw_current));
		view_thread(ps_global, stream, g_sort.msgmap, 0);
	    }
	}
	else if(SEP_THRDINDX()){
	    set_lflags(stream, g_sort.msgmap, MN_CHID2, 0);
	    collapse_threads(stream, g_sort.msgmap, NULL);
	}
	else{
	    thrd = fetch_head_thread(stream);
	    while(thrd){
		/*
		 * The top-level threads aren't hidden by collapse.
		 */
		msgno = mn_raw2m(g_sort.msgmap, thrd->rawno);
		if(msgno)
		  set_lflag(stream, g_sort.msgmap, msgno, MN_CHID, 0);

		if(thrd->next){
		    PINETHRD_S *nthrd;

		    nthrd = fetch_thread(stream, thrd->next);
		    if(nthrd)
		      make_thrdflags_consistent(stream, g_sort.msgmap,
						nthrd,
						get_lflag(stream, NULL,
							  thrd->rawno,
							  MN_COLL));
		}

		if(thrd->nextthd)
		  thrd = fetch_thread(stream, thrd->nextthd);
		else
		  thrd = NULL;
	    }
	}
    }

    stream->spare = 1;

    dprint((2, "sort_thread_callback done\n"));
}


void
collapse_threads(MAILSTREAM *stream, MSGNO_S *msgmap, PINETHRD_S *not_this_thread)
{
    PINETHRD_S   *thrd = NULL, *nthrd;
    unsigned long msgno;

    dprint((9, "collapse_threads\n"));

    thrd = fetch_head_thread(stream);
    while(thrd){
	if(thrd != not_this_thread){
	    msgno = mn_raw2m(g_sort.msgmap, thrd->rawno);

	    /* set collapsed bit */
	    if(msgno){
		set_lflag(stream, g_sort.msgmap, msgno, MN_COLL, 1);
		set_lflag(stream, g_sort.msgmap, msgno, MN_CHID, 0);
	    }

	    /* hide its children */
	    if(thrd->next && (nthrd = fetch_thread(stream, thrd->next)))
	      set_thread_subtree(stream, nthrd, msgmap, 1, MN_CHID);
	}

	if(thrd->nextthd)
	  thrd = fetch_thread(stream, thrd->nextthd);
	else
	  thrd = NULL;
    }

    dprint((9, "collapse_threads done\n"));
}


void
make_thrdflags_consistent(MAILSTREAM *stream, MSGNO_S *msgmap, PINETHRD_S *thrd,
			  int a_parent_is_collapsed)
{
    PINETHRD_S *nthrd, *bthrd;
    unsigned long msgno;

    if(!thrd)
      return;

    msgno = mn_raw2m(msgmap, thrd->rawno);

    if(a_parent_is_collapsed){
	/* if some parent is collapsed, we should be hidden */
	if(msgno)
	  set_lflag(stream, msgmap, msgno, MN_CHID, 1);
    }
    else{
	/* no parent is collapsed so we are not hidden */
	if(msgno)
	  set_lflag(stream, msgmap, msgno, MN_CHID, 0);
    }

    if(thrd->next){
	nthrd = fetch_thread(stream, thrd->next);
	if(nthrd)
	  make_thrdflags_consistent(stream, msgmap, nthrd,
				    a_parent_is_collapsed
				      ? a_parent_is_collapsed
				      : get_lflag(stream, NULL, thrd->rawno,
						  MN_COLL));
    }

    if(thrd->branch){
	bthrd = fetch_thread(stream, thrd->branch);
	if(bthrd)
	  make_thrdflags_consistent(stream, msgmap, bthrd,
				    a_parent_is_collapsed);
    }
}


long
calculate_visible_threads(MAILSTREAM *stream)
{
    PINETHRD_S   *thrd = NULL;
    long          vis = 0L;

    thrd = fetch_head_thread(stream);
    while(thrd){
	vis += (thread_has_some_visible(stream, thrd) ? 1 : 0);

	if(thrd->nextthd)
	  thrd = fetch_thread(stream, thrd->nextthd);
	else
	  thrd = NULL;
    }

    return(vis);
}


/*
 * This routine does a couple things. The input is the THREADNODE node
 * that we get from c-client because of the THREAD command. The rest of
 * the arguments are used to help guide this function through its
 * recursive steps. One thing it does is to place the sort order in
 * the array initially pointed to by the entry argument. All it is doing
 * is walking the tree in the next then branch order you see below and
 * incrementing the entry number one for each node. The other thing it
 * is doing at the same time is to create a PINETHRD_S tree from the
 * THREADNODE tree. The two trees are completely equivalent but the
 * PINETHRD_S version has additional back pointers and parent pointers
 * and so on to make it easier for alpine to deal with it. Each node
 * of that tree is tied to the data associated with a particular message
 * by the msgno_thread_info() call, so that we can go from a message
 * number to the place in the thread tree that message sits.
 */
long *
sort_thread_flatten(THREADNODE *node, MAILSTREAM *stream,
		    long *entry, char *dup_chk, long maxno,
		    PINETHRD_S *thrd, unsigned int flags)
{
    PINETHRD_S *newthrd = NULL;

    if(node){
	if(node->num > 0L && node->num <= maxno){		/* holes happen */
	    if(!dup_chk[node->num]){				/* not a duplicate */
		*entry = node->num;
		dup_chk[node->num] = 1;

		/*
		 * Build a richer threading structure that will help us paint
		 * and operate on threads and subthreads.
		 */
		newthrd = msgno_thread_info(stream, node->num, thrd, flags);
		if(newthrd){
		  entry++;

		  if(node->next)
		    entry = sort_thread_flatten(node->next, stream,
						entry, dup_chk, maxno,
						newthrd, THD_NEXT);

		  if(node->branch)
		    entry = sort_thread_flatten(node->branch, stream,
						entry, dup_chk, maxno,
						newthrd,
						(flags == THD_TOP) ? THD_TOP
								   : THD_BRANCH);
		}
	    }
	}
    }

    return(entry);
}


/*
 * Make a copy of c-client's THREAD tree while eliminating dummy nodes.
 */
THREADNODE *
collapse_threadnode_tree(THREADNODE *tree)
{
    THREADNODE *newtree = NULL;

    if(tree){
	if(tree->num){
	    newtree = mail_newthreadnode(NULL);
	    newtree->num  = tree->num;
	    if(tree->next)
	      newtree->next = collapse_threadnode_tree(tree->next);

	    if(tree->branch)
	      newtree->branch = collapse_threadnode_tree(tree->branch);
	}
	else{
	    if(tree->next)
	      newtree = collapse_threadnode_tree(tree->next);
	    
	    if(tree->branch){
		if(newtree){
		    THREADNODE *last_branch = NULL;

		    /*
		     * Next moved up to replace "tree" in the tree.
		     * If next has no branches, then we want to branch off
		     * of next. If next has branches, we want to branch off
		     * of the last of those branches instead.
		     */
		    last_branch = newtree;
		    while(last_branch->branch)
		      last_branch = last_branch->branch;
		    
		    last_branch->branch = collapse_threadnode_tree(tree->branch);
		}
		else
		  newtree = collapse_threadnode_tree(tree->branch);
	    }
	}
    }

    return(newtree);
}


/*
 * Like collapse_threadnode_tree, we collapse the dummy nodes.
 * In addition we rearrange the threads by order of arrival of
 * the last message in the thread, rather than the first message
 * in the thread.
 */
THREADNODE *
collapse_threadnode_tree_sorted(THREADNODE *tree)
{
    THREADNODE *sorted_tree = NULL;

    sorted_tree = sort_threads_and_collapse(tree);

    /* 
     * We used to eliminate top-level dummy nodes here so that
     * orphans would still get sorted together, but we changed
     * to sort the orphans themselves as top-level threads.
     *
     * It might be a matter of choice how they get sorted, but
     * we'll try doing it this way and not add another feature.
     */

    return(sorted_tree);
}

/*
 * Recurse through the tree, sorting each top-level branch by the
 * greatest num in the thread.
 */
THREADNODE *
sort_threads_and_collapse(THREADNODE *tree)
{
    THREADNODE *newtree = NULL, *newbranchtree = NULL, *newtreefree = NULL;

    if(tree){
	newtree = mail_newthreadnode(NULL);
	newtree->num  = tree->num;

	/* 
	 * Only sort at the top level.  Individual threads can
	 * rely on collapse_threadnode_tree 
	 */
	if(tree->next)
	  newtree->next = collapse_threadnode_tree(tree->next);

	if(tree->branch){
	    /*
	     * This recursive call returns an already re-sorted tree.
	     * With that, we can loop through and inject ourselves
	     * where we fit in with that sort, and pass back to the
	     * caller to inject themselves.
	     */
	    newbranchtree = sort_threads_and_collapse(tree->branch);
	}

	if(newtree->num)
	  newtree = insert_tree_in_place(newtree, newbranchtree);
	else{
	    /*
	     * If top node is a dummy, here is where we collapse it.
	     */
	    newtreefree = newtree;
	    newtree = insert_tree_in_place(newtree->next, newbranchtree);
	    newtreefree->next = NULL;
	    mail_free_threadnode(&newtreefree);
	}
    }

    return(newtree);
}

/*
 * Recursively insert each of the top-level nodes in newtree in their place
 * in tree according to which tree has the most recent arrival
 */
THREADNODE *
insert_tree_in_place(THREADNODE *newtree, THREADNODE *tree)
{
    THREADNODE *node = NULL;
    unsigned long newtree_greatest_num = 0;
    if(newtree->branch){
	node = newtree->branch;
	newtree->branch = NULL;
	tree = insert_tree_in_place(node, tree);
    }

    newtree_greatest_num = branch_greatest_num(newtree, 0);

    if(tree){
	/*
	 * Since tree is already sorted, we can insert when we find something
	 * newtree is less than
	 */
	if(newtree_greatest_num < branch_greatest_num(tree, 0))
	  newtree->branch = tree;
	else {
	    for(node = tree; node->branch; node = node->branch){
		if(newtree_greatest_num < branch_greatest_num(node->branch, 0)){
		    newtree->branch = node->branch;
		    node->branch = newtree;
		    break;
		}
	    }
	    if(!node->branch)
	      node->branch = newtree;

	    newtree = tree;
	}
    }

    return(newtree);
}

/*
 * Given a thread, return the greatest num in the tree.
 * is_subthread tells us not to recurse through branches, so
 * we can split the top level into threads.
 */
unsigned long
branch_greatest_num(THREADNODE *tree, int is_subthread)
{
    unsigned long ret, branch_ret;

    ret = tree->num;

    if(tree->next && (branch_ret = branch_greatest_num(tree->next, 1)) > ret)
      ret = branch_ret;
    if(is_subthread && tree->branch &&
       (branch_ret = branch_greatest_num(tree->branch, 1)) > ret)
      ret = branch_ret;

    return ret;
}


/*
 * Args      stream -- the usual
 *            rawno -- the raw msg num associated with this new node
 * attached_to_thrd -- the PINETHRD_S node that this is either a next or branch
 *                       off of
 *            flags --
 */
PINETHRD_S *
msgno_thread_info(MAILSTREAM *stream, long unsigned int rawno,
		  PINETHRD_S *attached_to_thrd, unsigned int flags)
{
    PINELT_S   **peltp;
    MESSAGECACHE *mc;

    if(!stream || rawno < 1L || rawno > stream->nmsgs)
      return NULL;

    /*
     * any private elt data yet?
     */
    if((mc = mail_elt(stream, rawno))
       && (*(peltp = (PINELT_S **) &mc->sparep) == NULL)){
	*peltp = (PINELT_S *) fs_get(sizeof(PINELT_S));
	memset(*peltp, 0, sizeof(PINELT_S));
    }

    if((*peltp)->pthrd == NULL)
      (*peltp)->pthrd = (PINETHRD_S *) fs_get(sizeof(PINETHRD_S));

    memset((*peltp)->pthrd, 0, sizeof(PINETHRD_S));

    (*peltp)->pthrd->rawno = rawno;

    if(attached_to_thrd)
      (*peltp)->pthrd->head = attached_to_thrd->head;
    else
      (*peltp)->pthrd->head = (*peltp)->pthrd->rawno;	/* it's me */

    if(flags == THD_TOP){
	/*
	 * We can tell this thread is a top-level thread because it doesn't
	 * have a parent.
	 */
	(*peltp)->pthrd->top = (*peltp)->pthrd->rawno;	/* I am a top */
	if(attached_to_thrd){
	    attached_to_thrd->nextthd = (*peltp)->pthrd->rawno;
	    (*peltp)->pthrd->prevthd  = attached_to_thrd->rawno;
	    (*peltp)->pthrd->thrdno   = attached_to_thrd->thrdno + 1L;
	}
	else
	    (*peltp)->pthrd->thrdno   = 1L;		/* 1st thread */

	g_sort.msgmap->max_thrdno = (*peltp)->pthrd->thrdno;
    }
    else if(flags == THD_NEXT){
	if(attached_to_thrd){
	    attached_to_thrd->next  = (*peltp)->pthrd->rawno;
	    (*peltp)->pthrd->parent = attached_to_thrd->rawno;
	    (*peltp)->pthrd->top    = attached_to_thrd->top;
	}
    }
    else if(flags == THD_BRANCH){
	if(attached_to_thrd){
	    attached_to_thrd->branch = (*peltp)->pthrd->rawno;
	    (*peltp)->pthrd->parent  = attached_to_thrd->parent;
	    (*peltp)->pthrd->top     = attached_to_thrd->top;
	}
    }

    return((*peltp)->pthrd);
}


/*
 * Collapse or expand a threading subtree. Not called from separate thread
 * index.
 */
void
collapse_or_expand(struct pine *state, MAILSTREAM *stream, MSGNO_S *msgmap,
		   long unsigned int msgno)
{
    int           collapsed, adjust_current = 0;
    PINETHRD_S   *thrd = NULL, *nthrd;
    unsigned long rawno;

    if(!stream)
      return;

    /*
     * If msgno is a good msgno, then we collapse or expand the subthread
     * which begins at msgno. If msgno is 0, we collapse or expand the
     * entire current thread.
     */

    if(msgno > 0L && msgno <= mn_get_total(msgmap)){
	rawno = mn_m2raw(msgmap, msgno);
	if(rawno)
	  thrd = fetch_thread(stream, rawno);
    }
    else if(msgno == 0L){
	rawno = mn_m2raw(msgmap, mn_get_cur(msgmap));
	if(rawno)
	  thrd = fetch_thread(stream, rawno);
	
	if(thrd && thrd->top != thrd->rawno){
	    adjust_current++;
	    thrd = fetch_thread(stream, thrd->top);
	    
	    /*
	     * Special case. If the user is collapsing the entire thread
	     * (msgno == 0), and we are in a Zoomed view, and the top of
	     * the entire thread is not part of the Zoomed view, then watch
	     * out. If we were to collapse the entire thread it would just
	     * disappear, because the top is not in the Zoom. Therefore,
	     * don't allow it. Do what the user probably wants, which is to
	     * collapse the thread at that point instead of the entire thread,
	     * leaving behind the top of the subthread to expand if needed.
	     * In other words, treat it as if they didn't have the
	     * F_SLASH_COLL_ENTIRE feature set.
	     */
	    collapsed = get_lflag(stream, NULL, thrd->rawno, MN_COLL)
			&& thrd->next;

	    if(!collapsed && get_lflag(stream, NULL, thrd->rawno, MN_HIDE))
	      thrd = fetch_thread(stream, rawno);
	}
    }


    if(!thrd)
      return;

    collapsed = get_lflag(stream, NULL, thrd->rawno, MN_COLL) && thrd->next;

    if(collapsed){
	msgno = mn_raw2m(msgmap, thrd->rawno);
	if(msgno > 0L && msgno <= mn_get_total(msgmap)){
	    set_lflag(stream, msgmap, msgno, MN_COLL, 0);
	    if(thrd->next){
		if((nthrd = fetch_thread(stream, thrd->next)) != NULL)
		  set_thread_subtree(stream, nthrd, msgmap, 0, MN_CHID);

		clear_index_cache_ent(stream, msgno, 0);
	    }
	}
    }
    else if(thrd && thrd->next){
	msgno = mn_raw2m(msgmap, thrd->rawno);
	if(msgno > 0L && msgno <= mn_get_total(msgmap)){
	    set_lflag(stream, msgmap, msgno, MN_COLL, 1);
	    if((nthrd = fetch_thread(stream, thrd->next)) != NULL)
	      set_thread_subtree(stream, nthrd, msgmap, 1, MN_CHID);

	    clear_index_cache_ent(stream, msgno, 0);
	}
    }
    else
      q_status_message(SM_ORDER, 0, 1,
		       _("No thread to collapse or expand on this line"));
    
    /* if current is hidden, adjust */
    if(adjust_current)
      adjust_cur_to_visible(stream, msgmap);
}


/*
 * Select the messages in a subthread. If all of the messages are already
 * selected, unselect them. This routine is a bit strange because it
 * doesn't set the MN_SLCT bit. Instead, it sets MN_STMP in apply_command
 * and then thread_command copies the MN_STMP messages back to MN_SLCT.
 */
void
select_thread_stmp(struct pine *state, MAILSTREAM *stream, MSGNO_S *msgmap)
{
    PINETHRD_S   *thrd;
    unsigned long rawno, in_thread, set_in_thread, save_branch;

    /* ugly bit means the same thing as return of 1 from individual_select */
    state->ugly_consider_advancing_bit = 0;

    if(!(stream && msgmap))
      return;

    rawno = mn_m2raw(msgmap, mn_get_cur(msgmap));
    if(rawno)
      thrd = fetch_thread(stream, rawno);
    
    if(!thrd)
      return;
    
    /* run through thrd to see if it is all selected */
    save_branch = thrd->branch;
    thrd->branch = 0L;
    if((set_in_thread = count_lflags_in_thread(stream, thrd, msgmap, MN_STMP))
       == (in_thread = count_lflags_in_thread(stream, thrd, msgmap, MN_NONE))){
	/*
	 * If everything is selected, the first unselect should cause
	 * an autozoom. In order to trigger the right code in
	 *   thread_command()
	 *     copy_lflags()
	 * we set the MN_HIDE bit on the current message here.
	 */
	if(F_ON(F_AUTO_ZOOM, state) && !any_lflagged(msgmap, MN_HIDE)
	   && any_lflagged(msgmap, MN_STMP) == mn_get_total(msgmap))
	  set_lflag(stream, msgmap, mn_get_cur(msgmap), MN_HIDE, 1);
	set_thread_lflags(stream, thrd, msgmap, MN_STMP, 0);
    }
    else{
	set_thread_lflags(stream, thrd, msgmap, MN_STMP, 1);
	state->ugly_consider_advancing_bit = 1;
    }

    thrd->branch = save_branch;
    
    if(set_in_thread == in_thread)
      q_status_message1(SM_ORDER, 0, 3, _("Unselected %s messages in thread"),
			comatose((long) in_thread));
    else if(set_in_thread == 0)
      q_status_message1(SM_ORDER, 0, 3, _("Selected %s messages in thread"),
			comatose((long) in_thread));
    else
      q_status_message1(SM_ORDER, 0, 3,
			_("Selected %s more messages in thread"),
			comatose((long) (in_thread-set_in_thread)));
}


/*
 * Count how many of this system flag in this thread subtree.
 * If flags == 0 count the messages in the thread.
 *
 * Watch out when calling this. The thrd->branch is not part of thrd.
 * Branch is a sibling to thrd, not a child. Zero out branch before calling
 * or call on thrd->next and worry about thrd separately.
 * Ok to call it on top-level thread which has no branch already.
 */
unsigned long
count_flags_in_thread(MAILSTREAM *stream, PINETHRD_S *thrd, long int flags)
{
    unsigned long count = 0;
    PINETHRD_S *nthrd, *bthrd;
    MESSAGECACHE *mc;

    if(!thrd || !stream || thrd->rawno < 1L || thrd->rawno > stream->nmsgs)
      return count;
    
    if(thrd->next){
	nthrd = fetch_thread(stream, thrd->next);
	if(nthrd)
	  count += count_flags_in_thread(stream, nthrd, flags);
    }

    if(thrd->branch){
	bthrd = fetch_thread(stream, thrd->branch);
	if(bthrd)
	  count += count_flags_in_thread(stream, bthrd, flags);
    }

    mc = (thrd && thrd->rawno > 0L && stream && thrd->rawno <= stream->nmsgs)
	  ? mail_elt(stream, thrd->rawno) : NULL;
    if(mc && mc->valid && FLAG_MATCH(flags, mc, stream))
      count++;

    return count;
}


/*
 * Count how many of this local flag in this thread subtree.
 * If flags == MN_NONE then we just count the messages instead of whether
 * the messages have a flag set.
 *
 * Watch out when calling this. The thrd->branch is not part of thrd.
 * Branch is a sibling to thrd, not a child. Zero out branch before calling
 * or call on thrd->next and worry about thrd separately.
 * Ok to call it on top-level thread which has no branch already.
 */
unsigned long
count_lflags_in_thread(MAILSTREAM *stream, PINETHRD_S *thrd, MSGNO_S *msgmap, int flags)
{
    unsigned long count = 0;
    PINETHRD_S *nthrd, *bthrd;

    if(!thrd || !stream || thrd->rawno < 1L || thrd->rawno > stream->nmsgs)
      return count;

    if(thrd->next){
	nthrd = fetch_thread(stream, thrd->next);
	if(nthrd)
	  count += count_lflags_in_thread(stream, nthrd, msgmap, flags);
    }

    if(thrd->branch){
	bthrd = fetch_thread(stream, thrd->branch);
	if(bthrd)
	  count += count_lflags_in_thread(stream, bthrd, msgmap,flags);
    }

    if(flags == MN_NONE)
      count++;
    else
      count += get_lflag(stream, msgmap, mn_raw2m(msgmap, thrd->rawno), flags);

    return count;
}


/*
 * Special-purpose for performance improvement.
 */
int
thread_has_some_visible(MAILSTREAM *stream, PINETHRD_S *thrd)
{
    PINETHRD_S *nthrd, *bthrd;

    if(!thrd || !stream || thrd->rawno < 1L || thrd->rawno > stream->nmsgs)
      return 0;

    if(get_lflag(stream, NULL, thrd->rawno, MN_HIDE) == 0)
      return 1;

    if(thrd->next){
	nthrd = fetch_thread(stream, thrd->next);
	if(nthrd && thread_has_some_visible(stream, nthrd))
	  return 1;
    }

    if(thrd->branch){
	bthrd = fetch_thread(stream, thrd->branch);
	if(bthrd && thread_has_some_visible(stream, bthrd))
	  return 1;
    }

    return 0;
}


int
mark_msgs_in_thread(MAILSTREAM *stream, PINETHRD_S *thrd, MSGNO_S *msgmap)
{
    int           count = 0;
    PINETHRD_S   *nthrd, *bthrd;
    MESSAGECACHE *mc;

    if(!thrd || !stream || thrd->rawno < 1L || thrd->rawno > stream->nmsgs)
      return count;

    if(thrd->next){
	nthrd = fetch_thread(stream, thrd->next);
	if(nthrd)
	  count += mark_msgs_in_thread(stream, nthrd, msgmap);
    }

    if(thrd->branch){
	bthrd = fetch_thread(stream, thrd->branch);
	if(bthrd)
	  count += mark_msgs_in_thread(stream, bthrd, msgmap);
    }

    if(stream && thrd->rawno >= 1L && thrd->rawno <= stream->nmsgs &&
       (mc = mail_elt(stream,thrd->rawno))
       && !mc->sequence
       && !mc->private.msg.env){
	mc->sequence = 1;
	count++;
    }

    return count;
}


/*
 * This sets or clears flags for the messages at this node and below in
 * a tree.
 *
 * Watch out when calling this. The thrd->branch is not part of thrd.
 * Branch is a sibling to thrd, not a child. Zero out branch before calling
 * or call on thrd->next and worry about thrd separately.
 * Ok to call it on top-level thread which has no branch already.
 */
void
set_thread_lflags(MAILSTREAM *stream, PINETHRD_S *thrd, MSGNO_S *msgmap, int flags, int v)
                       
                     
                       
                      		/* flags to set or clear */
                  		/* set or clear? */
{
    unsigned long msgno;
    PINETHRD_S *nthrd, *bthrd;

    if(!thrd || !stream || thrd->rawno < 1L || thrd->rawno > stream->nmsgs)
      return;

    msgno = mn_raw2m(msgmap, thrd->rawno);

    set_lflag(stream, msgmap, msgno, flags, v);

    /*
     * Careful, performance hack. This should logically be a separate
     * operation on the thread but it is convenient to stick it in here.
     *
     * When we back out of viewing a thread to the separate-thread-index
     * we may leave behind some cached hlines that aren't quite right
     * because they were collapsed. In particular, the plus_col character
     * may be wrong. Instead of trying to figure out what it should be just
     * clear the cache entries for the this thread when we come back in
     * to view it again.
     */
    if(msgno > 0L && flags == MN_CHID2 && v == 1)
      clear_index_cache_ent(stream, msgno, 0);

    if(thrd->next){
	nthrd = fetch_thread(stream, thrd->next);
	if(nthrd)
	  set_thread_lflags(stream, nthrd, msgmap, flags, v);
    }

    if(thrd->branch){
	bthrd = fetch_thread(stream, thrd->branch);
	if(bthrd)
	  set_thread_lflags(stream, bthrd, msgmap, flags, v);
    }
}


char
status_symbol_for_thread(MAILSTREAM *stream, PINETHRD_S *thrd, IndexColType type)
{
    char        status = ' ';
    unsigned long save_branch, cnt, tot_in_thrd;

    if(!thrd || !stream || thrd->rawno < 1L || thrd->rawno > stream->nmsgs)
      return status;

    save_branch = thrd->branch;
    thrd->branch = 0L;		/* branch is a sibling, not part of thread */
    
    /*
     * This is D if all of thread is deleted,
     * else A if all of thread is answered,
     * else F if all of thread is forwarded,
     * else N if any unseen and not deleted,
     * else blank.
     */
    if(type == iStatus){
	tot_in_thrd = count_flags_in_thread(stream, thrd, F_NONE);
	/* all deleted */
	if(count_flags_in_thread(stream, thrd, F_DEL) == tot_in_thrd)
	  status = 'D';
	/* all answered */
	else if(count_flags_in_thread(stream, thrd, F_ANS) == tot_in_thrd)
	  status = 'A';
	/* all forwarded */
	else if(count_flags_in_thread(stream, thrd, F_FWD) == tot_in_thrd)
	  status = 'F';
	/* or any new and not deleted */
	else if((!IS_NEWS(stream)
		 || F_ON(F_FAKE_NEW_IN_NEWS, ps_global))
		&& count_flags_in_thread(stream, thrd, F_UNDEL|F_UNSEEN))
	  status = 'N';
    }
    else if(type == iFStatus){
	if(!IS_NEWS(stream) || F_ON(F_FAKE_NEW_IN_NEWS, ps_global)){
	    tot_in_thrd = count_flags_in_thread(stream, thrd, F_NONE);
	    cnt = count_flags_in_thread(stream, thrd, F_UNSEEN);
	    if(cnt)
	      status = (cnt == tot_in_thrd) ? 'N' : 'n';
	}
    }
    else if(type == iIStatus || type == iSIStatus){
	tot_in_thrd = count_flags_in_thread(stream, thrd, F_NONE);

	/* unseen and recent */
	cnt = count_flags_in_thread(stream, thrd, F_RECENT|F_UNSEEN);
	if(cnt)
	  status = (cnt == tot_in_thrd) ? 'N' : 'n';
	else{
	    /* unseen and !recent */
	    cnt = count_flags_in_thread(stream, thrd, F_UNSEEN);
	    if(cnt)
	      status = (cnt == tot_in_thrd) ? 'U' : 'u';
	    else{
		/* seen and recent */
		cnt = count_flags_in_thread(stream, thrd, F_RECENT|F_SEEN);
		if(cnt)
		  status = (cnt == tot_in_thrd) ? 'R' : 'r';
	    }
	}
    }

    thrd->branch = save_branch;

    return status;
}


/*
 * Symbol is * if some message in thread is important,
 * + if some message is to us,
 * - if mark-for-cc and some message is cc to us, else blank.
 */
char
to_us_symbol_for_thread(MAILSTREAM *stream, PINETHRD_S *thrd, int consider_flagged)
{
    char        to_us = ' ';
    char        branch_to_us = ' ';
    PINETHRD_S *nthrd, *bthrd;
    MESSAGECACHE *mc;

    if(!thrd || !stream || thrd->rawno < 1L || thrd->rawno > stream->nmsgs)
      return to_us;

    if(thrd->next){
	nthrd = fetch_thread(stream, thrd->next);
	if(nthrd)
	  to_us = to_us_symbol_for_thread(stream, nthrd, consider_flagged);
    }

    if(((consider_flagged && to_us != '*') || (!consider_flagged && to_us != '+'))
       && thrd->branch){
	bthrd = fetch_thread(stream, thrd->branch);
	if(bthrd)
	  branch_to_us = to_us_symbol_for_thread(stream, bthrd, consider_flagged);

	/* use branch to_us symbol if it has higher priority than what we have so far */
	if(to_us == ' '){
	    if(branch_to_us == '-' || branch_to_us == '+' || branch_to_us == '*')
	      to_us = branch_to_us;
	}
	else if(to_us == '-'){
	    if(branch_to_us == '+' || branch_to_us == '*')
	      to_us = branch_to_us;
	}
	else if(to_us == '+'){
	    if(branch_to_us == '*')
	      to_us = branch_to_us;
	}
    }

    if((consider_flagged && to_us != '*') || (!consider_flagged && to_us != '+')){
	if(consider_flagged && thrd && thrd->rawno > 0L
	   && stream && thrd->rawno <= stream->nmsgs
	   && (mc = mail_elt(stream, thrd->rawno))
	   && FLAG_MATCH(F_FLAG, mc, stream))
	  to_us = '*';
	else if(to_us != '+' && !IS_NEWS(stream)){
	    INDEXDATA_S   idata;
	    MESSAGECACHE *mc;
	    ADDRESS      *addr;

	    memset(&idata, 0, sizeof(INDEXDATA_S));
	    idata.stream   = stream;
	    idata.rawno    = thrd->rawno;
	    idata.msgno    = mn_raw2m(sp_msgmap(stream), idata.rawno);
	    if(idata.rawno > 0L && stream && idata.rawno <= stream->nmsgs
	       && (mc = mail_elt(stream, idata.rawno))){
		idata.size = mc->rfc822_size;
		index_data_env(&idata,
			       pine_mail_fetchenvelope(stream, idata.rawno));
	    }
	    else
	      idata.bogus = 2;

	    for(addr = fetch_to(&idata); addr; addr = addr->next)
	      if(address_is_us(addr, ps_global)){
		  to_us = '+';
		  break;
	      }
	    
	    if(to_us != '+' && resent_to_us(&idata))
	      to_us = '+';

	    if(to_us == ' ' && F_ON(F_MARK_FOR_CC,ps_global))
	      for(addr = fetch_cc(&idata); addr; addr = addr->next)
		if(address_is_us(addr, ps_global)){
		    to_us = '-';
		    break;
		}
	}
    }

    return to_us;
}


/*
 * This sets or clears flags for the messages at this node and below in
 * a tree. It doesn't just blindly do it, perhaps it should. Instead,
 * when un-hiding a subtree it leaves the sub-subtree hidden if a node
 * is collapsed.
 *
 * Watch out when calling this. The thrd->branch is not part of thrd.
 * Branch is a sibling to thrd, not a child. Zero out branch before calling
 * or call on thrd->next and worry about thrd separately.
 * Ok to call it on top-level thread which has no branch already.
 */
void
set_thread_subtree(MAILSTREAM *stream, PINETHRD_S *thrd, MSGNO_S *msgmap, int v, int flags)
                       
                     
                       
                  		/* set or clear? */
                      		/* flags to set or clear */
{
    int hiding;
    unsigned long msgno;
    PINETHRD_S *nthrd, *bthrd;

    hiding = (flags == MN_CHID) && v;

    if(!thrd || !stream || thrd->rawno < 1L || thrd->rawno > stream->nmsgs)
      return;

    msgno = mn_raw2m(msgmap, thrd->rawno);

    set_lflag(stream, msgmap, msgno, flags, v);

    if(thrd->next && (hiding || !get_lflag(stream,NULL,thrd->rawno,MN_COLL))){
	nthrd = fetch_thread(stream, thrd->next);
	if(nthrd)
	  set_thread_subtree(stream, nthrd, msgmap, v, flags);
    }

    if(thrd->branch){
	bthrd = fetch_thread(stream, thrd->branch);
	if(bthrd)
	  set_thread_subtree(stream, bthrd, msgmap, v, flags);
    }
}


/*
 * View a thread. Move from the thread index screen to a message index
 * screen for the current thread.
 *
 *      set_lflags - Set the local flags appropriately to start viewing
 *                   the thread. We would not want to set this if we are
 *                   already viewing the thread (and expunge or new mail
 *                   happened) and we want to preserve the collapsed state
 *                   of the subthreads.
 */
int
view_thread(struct pine *state, MAILSTREAM *stream, MSGNO_S *msgmap, int set_lflags)
{
    PINETHRD_S   *thrd = NULL;
    unsigned long rawno, cur;

    if(!any_messages(msgmap, NULL, "to View"))
      return 0;

    if(!(stream && msgmap))
      return 0;

    rawno = mn_m2raw(msgmap, mn_get_cur(msgmap));
    if(rawno)
      thrd = fetch_thread(stream, rawno);

    if(thrd && thrd->top && thrd->top != thrd->rawno)
      thrd = fetch_thread(stream, thrd->top);
    
    if(!thrd)
      return 0;
    
    /*
     * Clear hidden and collapsed flag for this thread.
     * And set CHID2.
     * Don't have to worry about there being a branch because
     * this is a toplevel thread.
     */
    if(set_lflags){
	set_thread_lflags(stream, thrd, msgmap, MN_COLL | MN_CHID, 0);
	set_thread_lflags(stream, thrd, msgmap, MN_CHID2, 1);
    }

    /*
     * If this is one of those wacky users who like to sort backwards
     * they would probably prefer that the current message be the last
     * one in the thread (the one highest up the screen).
     */
    if(mn_get_revsort(msgmap)){
	cur = mn_get_cur(msgmap);
	while(cur > 1L && get_lflag(stream, msgmap, cur-1L, MN_CHID2))
	  cur--;

	if(cur != mn_get_cur(msgmap))
	  mn_set_cur(msgmap, cur);
    }

    /* first message in thread might be hidden if zoomed */
    if(any_lflagged(msgmap, MN_HIDE)){
	cur = mn_get_cur(msgmap);
	while(get_lflag(stream, msgmap, cur, MN_HIDE))
          cur++;
	
	if(cur != mn_get_cur(msgmap))
	  mn_set_cur(msgmap, cur);
    }

    msgmap->top = mn_get_cur(msgmap);
    sp_set_viewing_a_thread(stream, 1);

    state->mangled_screen = 1;
    setup_for_index_index_screen();

    return 1;
}


int
unview_thread(struct pine *state, MAILSTREAM *stream, MSGNO_S *msgmap)
{
    PINETHRD_S   *thrd = NULL, *topthrd = NULL;
    unsigned long rawno;

    if(!(stream && msgmap))
      return 0;

    rawno = mn_m2raw(msgmap, mn_get_cur(msgmap));
    if(rawno)
      thrd = fetch_thread(stream, rawno);
    
    if(thrd && thrd->top)
      topthrd = fetch_thread(stream, thrd->top);
    
    if(!topthrd)
      return 0;

    /* hide this thread */
    set_thread_lflags(stream, topthrd, msgmap, MN_CHID, 1);

    /* clear special CHID2 flags for this thread */
    set_thread_lflags(stream, topthrd, msgmap, MN_CHID2, 0);

    /* clear CHID for top-level message and set COLL */
    set_lflag(stream, msgmap, mn_raw2m(msgmap, topthrd->rawno), MN_CHID, 0);
    set_lflag(stream, msgmap, mn_raw2m(msgmap, topthrd->rawno), MN_COLL, 1);

    mn_set_cur(msgmap, mn_raw2m(msgmap, topthrd->rawno));
    sp_set_viewing_a_thread(stream, 0);
    setup_for_thread_index_screen();

    return 1;
}


PINETHRD_S *
find_thread_by_number(MAILSTREAM *stream, MSGNO_S *msgmap, long int target, PINETHRD_S *startthrd)
{
    PINETHRD_S *thrd = NULL;

    if(!(stream && msgmap))
      return(thrd);

    thrd = startthrd;
    
    if(!thrd || !(thrd->prevthd || thrd->nextthd))
      thrd = fetch_thread(stream, mn_m2raw(msgmap, mn_get_cur(msgmap)));

    if(thrd && !(thrd->prevthd || thrd->nextthd) && thrd->head)
      thrd = fetch_thread(stream, thrd->head);

    if(thrd){
	/* go forward from here */
	if(thrd->thrdno < target){
	    while(thrd){
		if(thrd->thrdno == target)
		  break;

		if(mn_get_revsort(msgmap) && thrd->prevthd)
		  thrd = fetch_thread(stream, thrd->prevthd);
		else if(!mn_get_revsort(msgmap) && thrd->nextthd)
		  thrd = fetch_thread(stream, thrd->nextthd);
		else
		  thrd = NULL;
	    }
	}
	/* back up from here */
	else if(thrd->thrdno > target
		&& (mn_get_revsort(msgmap)
		    || (thrd->thrdno - target) < (target - 1L))){
	    while(thrd){
		if(thrd->thrdno == target)
		  break;

		if(mn_get_revsort(msgmap) && thrd->nextthd)
		  thrd = fetch_thread(stream, thrd->nextthd);
		else if(!mn_get_revsort(msgmap) && thrd->prevthd)
		  thrd = fetch_thread(stream, thrd->prevthd);
		else
		  thrd = NULL;
	    }
	}
	/* go forward from head */
	else if(thrd->thrdno > target){
	    if(thrd->head){
		thrd = fetch_thread(stream, thrd->head);
		while(thrd){
		    if(thrd->thrdno == target)
		      break;

		    if(thrd->nextthd)
		      thrd = fetch_thread(stream, thrd->nextthd);
		    else
		      thrd = NULL;
		}
	    }
	}
    }

    return(thrd);
}


/*
 * Set search bit for every message in a thread.
 *
 * Watch out when calling this. The thrd->branch is not part of thrd.
 * Branch is a sibling to thrd, not a child. Zero out branch before calling
 * or call on thrd->next and worry about thrd separately. Top-level threads
 * already have a branch equal to zero.
 *
 *  If msgset is non-NULL, then only set the search bit for a message if that
 *  message is included in the msgset.
 */
void
set_search_bit_for_thread(MAILSTREAM *stream, PINETHRD_S *thrd, SEARCHSET **msgset)
{
    PINETHRD_S *nthrd, *bthrd;

    if(!(stream && thrd))
      return;

    if(thrd->rawno > 0L && thrd->rawno <= stream->nmsgs
       && (!(msgset && *msgset) || in_searchset(*msgset, thrd->rawno)))
      mm_searched(stream, thrd->rawno);

    if(thrd->next){
	nthrd = fetch_thread(stream, thrd->next);
	if(nthrd)
	  set_search_bit_for_thread(stream, nthrd, msgset);
    }

    if(thrd->branch){
	bthrd = fetch_thread(stream, thrd->branch);
	if(bthrd)
	  set_search_bit_for_thread(stream, bthrd, msgset);
    }
}
