/*
 * $Id: msgno.h 1142 2008-08-13 17:22:21Z hubert@u.washington.edu $
 *
 * ========================================================================
 * Copyright 2006 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */

#ifndef PITH_MSGNO_INCLUDED
#define PITH_MSGNO_INCLUDED


#include "../pith/sorttype.h"


/*
 * Macros to support anything you'd ever want to do with a message
 * number...
 */
#define	mn_init(P, m)		msgno_init((P), (m), \
					   ps_global->def_sort, ps_global->def_sort_rev)

#define	mn_get_cur(p)		(((p) && (p)->select) 			      \
				  ? (p)->select[(p)->sel_cur] : -1)

#define	mn_set_cur(p, m)	do{					      \
				  if(p){				      \
				    (p)->select[(p)->sel_cur] = (m);	      \
				  }					      \
				}while(0)

#define	mn_inc_cur(s, p, f)	msgno_inc(s, p, f)

#define	mn_dec_cur(s, p, f)	msgno_dec(s, p, f)

#define	mn_add_cur(p, m)	do{					      \
				  if(p){				      \
				      if((p)->sel_cnt+1L > (p)->sel_size){    \
					  (p)->sel_size += 10L;		      \
					  fs_resize((void **)&((p)->select),  \
						    (size_t)(p)->sel_size     \
						             * sizeof(long)); \
				      }					      \
				      (p)->select[((p)->sel_cnt)++] = (m);    \
				  }					      \
				}while(0)

#define	mn_total_cur(p)		((p) ? (p)->sel_cnt : 0L)

#define	mn_first_cur(p)		(((p) && (p)->sel_cnt > 0L)		      \
				  ? (p)->select[(p)->sel_cur = 0] : 0L)

#define	mn_next_cur(p)		(((p) && ((p)->sel_cur + 1) < (p)->sel_cnt)   \
				  ? (p)->select[++((p)->sel_cur)] : -1L)

#define	mn_is_cur(p, m)		msgno_in_select((p), (m))

#define	mn_reset_cur(p, m)	do{					      \
				  if(p){				      \
				      (p)->sel_cur  = 0L;		      \
				      (p)->sel_cnt  = 1L;		      \
				      (p)->sel_size = 8L;		      \
				      fs_resize((void **)&((p)->select),      \
					(size_t)(p)->sel_size * sizeof(long));\
				      (p)->select[0] = (m);		      \
				  }					      \
			        }while(0)

#define	mn_m2raw(p, m)		(((p) && (p)->sort && (m) > 0 		      \
				  && (m) <= mn_get_total(p)) 		      \
				   ? (p)->sort[m] : 0L)

#define	mn_raw2m(p, m)		(((p) && (p)->isort && (m) > 0 		      \
				  && (m) <= mn_get_nmsgs(p)) 		      \
				   ? (p)->isort[m] : 0L)

#define	mn_get_total(p)		((p) ? (p)->max_msgno : 0L)

#define	mn_set_total(p, m)	do{ if(p) (p)->max_msgno = (m); }while(0)

#define	mn_get_nmsgs(p)		((p) ? (p)->nmsgs : 0L)

#define	mn_set_nmsgs(p, m)	do{ if(p) (p)->nmsgs = (m); }while(0)

#define	mn_add_raw(p, m)	msgno_add_raw((p), (m))

#define	mn_flush_raw(p, m)	msgno_flush_raw((p), (m))

#define	mn_get_sort(p)		((p) ? (p)->sort_order : SortArrival)

#define	mn_set_sort(p, t)	msgno_set_sort((p), (t))

#define	mn_get_revsort(p)	((p) ? (p)->reverse_sort : 0)

#define	mn_set_revsort(p, t)	do{					      \
				  if(p)					      \
				    (p)->reverse_sort = (t);		      \
				}while(0)

#define	mn_get_mansort(p)	((p) ? (p)->manual_sort : 0)

#define	mn_set_mansort(p, t)	do{					      \
				  if(p)					      \
				    (p)->manual_sort = (t);		      \
				}while(0)

#define	mn_give(P)		msgno_give(P)


/*
 * This is *the* struct that keeps track of the pine message number to
 * raw c-client sequence number mappings.  The mapping is necessary
 * because pine may re-sort or even hide (exclude) c-client numbers
 * from the displayed list of messages.  See mailindx.c:msgno_* and
 * the mn_* macros above for how this things gets used.  See
 * mailcmd.c:pseudo_selected for an explanation of the funny business
 * going on with the "hilited" field...
 */
typedef struct msg_nos {
    long      *select,				/* selected message array  */
	       sel_cur,				/* current interesting msg */
	       sel_cnt,				/* its size		   */
	       sel_size,			/* its size		   */
              *sort,				/* sorted array of msgno's */
               sort_size,			/* its size		   */
              *isort,				/* inverse of sort array   */
               isort_size,			/* its size		   */
	       max_msgno,			/* total messages in table */
	       nmsgs,				/* total msgs in folder    */
	       hilited,				/* holder for "current" msg*/
	       top,				/* message at top of screen*/
	       max_thrdno,
	       top_after_thrd;			/* top after thrd view     */
    SortOrder  sort_order;			/* list's current sort     */
    unsigned   reverse_sort:1;			/* whether that's reversed */
    unsigned   manual_sort:1;			/* sorted with $ command   */
    long       flagged_hid,			/* hidden count		   */
	       flagged_exld,			/* excluded count	   */
	       flagged_coll,			/* collapsed count	   */
	       flagged_chid,			/* collapsed-hidden count  */
	       flagged_chid2,			/* */
	       flagged_usor,			/* new unsorted mail	   */
	       flagged_tmp,			/* tmp flagged count	   */
	       flagged_stmp,			/* stmp flagged count	   */
	       flagged_invisible,		/* this one's different    */
	       flagged_srch,			/* search result/not slctd */
	       visible_threads;			/* so is this one          */
} MSGNO_S;


#define	MSG_EX_DELETE	  0x0001	/* part is deleted */
#define	MSG_EX_RECENT	  0x0002
#define	MSG_EX_TESTED	  0x0004	/* filtering has been run on this msg */
#define	MSG_EX_FILTERED	  0x0008	/* msg has actually been filtered away*/
#define	MSG_EX_FILED	  0x0010	/* msg has been filed */
#define	MSG_EX_FILTONCE	  0x0020
#define	MSG_EX_FILEONCE	  0x0040	/* These last two mean that the
					   message has been filtered or filed
					   already but the filter rule was
					   non-terminating so it is still
					   possible it will get filtered
					   again. When we're done, we flip
					   these two to EX_FILTERED and
					   EX_FILED, the permanent versions. */
#define	MSG_EX_PEND_EXLD  0x0080	/* pending exclusion */
#define	MSG_EX_MANUNDEL   0x0100	/* has been manually undeleted */
#define	MSG_EX_STATECHG	  0x0200	/* state change since filtering */

/* msgno_include flags */
#define	MI_NONE		0x00
#define	MI_REFILTERING	0x01
#define	MI_STATECHGONLY	0x02
#define	MI_CLOSING	0x04


/* exported protoypes */
void	    msgno_init(MSGNO_S **, long, SortOrder, int);
void        msgno_reset_isort(MSGNO_S *);
void	    msgno_give(MSGNO_S **);
void	    msgno_inc(MAILSTREAM *, MSGNO_S *, int);
void	    msgno_dec(MAILSTREAM *, MSGNO_S *, int);
void	    msgno_exclude_deleted(MAILSTREAM *, MSGNO_S *);
void	    msgno_exclude(MAILSTREAM *, MSGNO_S *, long, int);
int	    msgno_include(MAILSTREAM *, MSGNO_S *, int);
void	    msgno_add_raw(MSGNO_S *, long);
void	    msgno_flush_raw(MSGNO_S *, long);
void        msgno_flush_selected(MSGNO_S *, long);
void	    msgno_set_sort(MSGNO_S *, SortOrder);
int	    msgno_in_select(MSGNO_S *, long);
int	    msgno_exceptions(MAILSTREAM *, long, char *, int *, int);
int	    msgno_any_deletedparts(MAILSTREAM *, MSGNO_S *);
int	    msgno_part_deleted(MAILSTREAM *, long, char *);
long        get_msg_score(MAILSTREAM *, long);
void        clear_msg_score(MAILSTREAM *, long);
void        clear_folder_scores(MAILSTREAM *);
int         calculate_some_scores(MAILSTREAM *, SEARCHSET *, int);
void	    free_pine_elt(void **);


#endif /* PITH_MSGNO_INCLUDED */
