/*
 * $Id: flag.h 1142 2008-08-13 17:22:21Z hubert@u.washington.edu $
 *
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

#ifndef PITH_FLAG_INCLUDED
#define PITH_FLAG_INCLUDED


#include "../pith/msgno.h"


/*
 * Useful def's to specify interesting flags
 */
#define	F_NONE		0x00000000
#define	F_SEEN		0x00000001
#define	F_UNSEEN	0x00000002
#define	F_DEL		0x00000004
#define	F_UNDEL		0x00000008
#define	F_FLAG		0x00000010
#define	F_UNFLAG	0x00000020
#define	F_ANS		0x00000040
#define	F_UNANS		0x00000080
#define	F_FWD		0x00000100
#define	F_UNFWD		0x00000200
#define	F_RECENT	0x00000400
#define	F_UNRECENT	0x00000800
#define	F_DRAFT		0x00001000
#define	F_UNDRAFT	0x00002000

#define	F_SRCHBACK	0x00004000	/* search backwards instead of forw */
#define	F_NOFILT	0x00008000	/* defer processing filters */

#define	F_OR_SEEN	0x00010000
#define	F_OR_UNSEEN	0x00020000
#define	F_OR_DEL	0x00040000
#define	F_OR_UNDEL	0x00080000
#define	F_OR_FLAG	0x00100000
#define	F_OR_UNFLAG	0x00200000
#define	F_OR_ANS	0x00400000
#define	F_OR_UNANS	0x00800000
#define	F_OR_FWD	0x01000000
#define	F_OR_UNFWD	0x02000000
#define	F_OR_RECENT	0x04000000
#define	F_OR_UNRECENT	0x08000000

#define	F_KEYWORD	0x10000000
#define	F_UNKEYWORD	0x20000000

#define	F_COMMENT	0x40000000


/*
 * Useful flag checking macro
 */
#define FLAG_MATCH(F,M,S) (((((F)&F_SEEN)   ? (M)->seen			     \
				: ((F)&F_UNSEEN)     ? !(M)->seen : 1)	     \
			  && (((F)&F_DEL)    ? (M)->deleted		     \
				: ((F)&F_UNDEL)      ? !(M)->deleted : 1)    \
			  && (((F)&F_ANS)    ? (M)->answered		     \
				: ((F)&F_UNANS)	     ? !(M)->answered : 1)   \
			  && (((F)&F_FWD) ? ((S) && user_flag_is_set((S),(M)->msgno,FORWARDED_FLAG)) \
			      : ((F)&F_UNFWD) ? ((S) && !user_flag_is_set((S),(M)->msgno,FORWARDED_FLAG)) : 1) \
			  && (((F)&F_FLAG) ? (M)->flagged		     \
				: ((F)&F_UNFLAG)   ? !(M)->flagged : 1)	     \
			  && (((F)&F_RECENT) ? (M)->recent		     \
			      : ((F)&F_UNRECENT)   ? !(M)->recent : 1))	     \
			  || ((((F)&F_OR_SEEN) ? (M)->seen		     \
				: ((F)&F_OR_UNSEEN)   ? !(M)->seen : 0)      \
			  || (((F)&F_OR_DEL)   ? (M)->deleted		     \
				: ((F)&F_OR_UNDEL)    ? !(M)->deleted : 0)   \
			  || (((F)&F_OR_ANS)   ? (M)->answered		     \
				: ((F)&F_OR_UNANS)    ? !(M)->answered : 0)  \
			  || (((F)&F_OR_FWD) ? ((S) && user_flag_is_set((S),(M)->msgno,FORWARDED_FLAG)) \
				: ((F)&F_OR_UNFWD)    ? !((S) && user_flag_is_set((S),(M)->msgno,FORWARDED_FLAG)) : 0) \
			  || (((F)&F_OR_FLAG)? (M)->flagged		     \
				: ((F)&F_OR_UNFLAG) ? !(M)->flagged : 0)     \
			  || (((F)&F_OR_RECENT)? (M)->recent		     \
				: ((F)&F_OR_UNRECENT) ? !(M)->recent : 0)))


/*
 * These are def's to help manage local, private flags pine uses
 * to maintain it's mapping table (see MSGNO_S def).  The local flags
 * are actually stored in spare bits in c-client's per-message
 * MESSAGECACHE struct.  But they're private, you ask.  Since the flags
 * are tied to the actual message (independent of the mapping), storing
 * them in the c-client means we don't have to worry about them during
 * sorting and such.  See {set,get}_lflags for more on local flags.
 *
 * MN_HIDE hides messages which are not visible due to Zooming.
 * MN_EXLD hides messages which have been filtered away.
 * MN_SLCT marks messages which have been Selected.
 * MN_SRCH marks messages that are the result of a search
 * MN_COLL marks a point in the thread tree where the view has been
 *          collapsed, hiding the messages below that point
 * MN_CHID hides messages which are collapsed out of view
 * MN_CHID2 is similar to CHID and is introduced for performance reasons.
 *          When using the separate-thread-index the toplevel messages are
 *          MN_COLL and everything else is MN_CHID. However, if we view a
 *          single thread from there, instead of marking all of those top
 *          level threads MN_HIDE (or something) we change the semantics
 *          of the flags. When viewing a single thread we mark the messages
 *          of the thread with MN_CHID2 for performance reasons.
 */
#define	MN_NONE		0x0000	/* No Pine specific flags  */
#define	MN_HIDE		0x0001	/* Pine specific hidden    */
#define	MN_EXLD		0x0002	/* Pine specific excluded  */
#define	MN_SLCT		0x0004	/* Pine specific selected  */
#define	MN_COLL		0x0008	/* Pine specific collapsed */
#define	MN_CHID		0x0010	/* A parent somewhere above us is collapsed */
#define	MN_CHID2	0x0020	/* performance related */
#define	MN_USOR		0x0040	/* New message which hasn't been sorted yet */
#define	MN_STMP		0x0080	/* Temporary storage for a per-message bit */
#define	MN_SRCH		0x0100	/* Search result */


/* next_sorted_flagged options */
#define	NSF_TRUST_FLAGS	0x01	/* input flag, don't need to ping           */
#define	NSF_SKIP_CHID	0x02	/* input flag, skip MN_CHID messages        */
#define	NSF_SEARCH_BACK	0x04	/* input flag, search backward if none forw */
#define	NSF_FLAG_MATCH	0x08	/* return flag, actually got a match        */

/* first_sorted_flagged options */
#define	FSF_LAST	0x01	/* last instead of first  */
#define	FSF_SKIP_CHID	0x02	/* skip MN_CHID messages  */


typedef long MsgNo;


/* exported protoypes */
long	    count_flagged(MAILSTREAM *, long);
MsgNo	    first_sorted_flagged(unsigned long, MAILSTREAM *, long, int);
MsgNo	    next_sorted_flagged(unsigned long, MAILSTREAM *, long, int *);
int	    get_lflag(MAILSTREAM *, MSGNO_S *, long, int);
int	    set_lflag(MAILSTREAM *, MSGNO_S *, long, int, int);
void        copy_lflags(MAILSTREAM *, MSGNO_S *, int, int);
void        set_lflags(MAILSTREAM *, MSGNO_S *, int, int);
long	    any_lflagged(MSGNO_S *, int);


#endif /* PITH_FLAG_INCLUDED */
