/*
 * $Id: stream.h 768 2007-10-24 00:10:03Z hubert@u.washington.edu $
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

#ifndef PITH_STREAM_INCLUDED
#define PITH_STREAM_INCLUDED


#include "../pith/context.h"
#include "../pith/msgno.h"
#include "../pith/savetype.h"


#define	AOS_NONE	0x00		/* alredy_open_stream: no flag  */
#define	AOS_RW_ONLY	0x01		/* don't match readonly streams */

/* pine_mail_list flags */
#define	PML_IS_MOVE_MBOX	0x01


/*
 * The stream pool is where we keep pointers to open streams. Some of them are
 * actively being used, some are connected to a folder but aren't actively
 * in use, some are random temporary use streams left open for possible
 * re-use. Each open stream should be in the streams array, which is of
 * size nstream altogether. Streams which are not to be re-used (don't have
 * the flag SP_USEPOOL set) are in the array anyway.
 */

/*
 * Structure holds global information about the stream pool. The per-stream
 * information is stored in a PER_STREAM_S struct attached to each stream.
 */
typedef struct stream_pool {
    int          max_remstream;	/* max implicitly cached remote streams	*/
    int          nstream;	/* size of streams array 		*/
    MAILSTREAM **streams;	/* the array of streams in stream pool	*/
} SP_S;

/*
 * Pine's private per-stream data stored on the c-client's stream
 * spare pointer.
 */
typedef struct pine_per_stream_data {
    MSGNO_S      *msgmap;
    CONTEXT_S    *context;		/* context fldr was interpreted in */
    char         *fldr;			/* folder name, alloced copy   */
    unsigned long flags;
    unsigned long icache_flags;
    int           ref_cnt;
    long          new_mail_count;   /* new mail since the last new_mail check */
    long          mail_since_cmd;   /* new mail since last key pressed */
    long          expunge_count;
    long          first_unseen;
    long          recent_since_visited;
    int           check_cnt;
    time_t        first_status_change;
    time_t        last_ping;		/* Keeps track of when the last    */
					/* command was. The command wasn't */
					/* necessarily a ping.             */
    time_t        last_expunged_reaper;	/* Some IMAP commands defer the    */
					/* return of EXPUNGE responses.    */
					/* This is the time of the last    */
					/* command which did not defer.    */
    time_t        last_chkpnt_done;
    time_t        last_use_time;
    time_t        last_activity;
    imapuid_t     saved_uid_validity;
    imapuid_t     saved_uid_last;
    char         *saved_cur_msg_id;
    unsigned      unsorted_newmail:1;
    unsigned      need_to_rethread:1;
    unsigned      io_error_on_stream:1;
    unsigned      mail_box_changed:1;
    unsigned      viewing_a_thread:1;
    unsigned      dead_stream:1;
    unsigned      noticed_dead_stream:1;
    unsigned      closing:1;
} PER_STREAM_S;

/*
 * Complicated set of flags for stream pool cache.
 * LOCKED, PERMLOCKED, TEMPUSE, and USEPOOL are flags stored in the stream
 * flags of the PER_STREAM_S structure.
 *
 *     SP_LOCKED -- In use, don't re-use this.
 *                  That isn't a good description of SP_LOCKED. Every time
 *                  we pine_mail_open a stream it is SP_LOCKED and a ref_cnt
 *                  is incremented. Pine_mail_close decrements the ref_cnt
 *                  and unlocks it when we get to zero.
 * SP_PERMLOCKED -- Should always be kept open, like INBOX. Right now the
 *                  only significance of this is that the expunge_and_close
 *                  won't happen if this is set (like the way INBOX works).
 *                  If a stream is PERMLOCKED it should no doubt be LOCKED
 *                  as well (it isn't done implicitly in the tests).
 *      SP_INBOX -- This stream is open on the INBOX.
 *   SP_USERFLDR -- This stream was opened by the user explicitly, not
 *                  implicitly like would happen with a save or a remote
 *                  address book, etc.
 *   SP_FILTERED -- This stream was opened by the user explicitly and
 *                  filtered.
 *    SP_TEMPUSE -- If a stream is not SP_LOCKED, that says we can re-use
 *                  it if need be but we should prefer to use another unused
 *                  slot if there is one. If a stream is marked TEMPUSE we
 *                  should consider re-using it before we consider re-using
 *                  a stream which is not LOCKED but not marked TEMPUSE.
 *                  This flag is not only stored in the PER_STREAM_S flags,
 *                  it is also an input argument to sp_stream_get.
 *                  It may make sense to mark a stream both SP_LOCKED and
 *                  SP_TEMPUSE. That way, when we close the stream it will
 *                  be SP_TEMPUSE and more re-usable than if we didn't.
 *    SP_USEPOOL -- Passed to pine_mail_open, it means to consider the
 *                  stream pool when opening and to put it into the stream
 *                  pool after we open it. If this is not set when we open,
 *                  we do an honest open and an honest close when we close.
 *
 *  These flags are input flags to sp_stream_get.
 *       SP_MATCH -- We're looking for a stream that is already open on
 *                   this mailbox. This is good if we are reopening the
 *                   same mailbox we already had opened.
 *        SP_SAME -- We're looking for a stream that is open to the same
 *                   server. For example, we might want to do a STATUS
 *                   command or a DELETE. We could use any stream that
 *                   is already open for this. Unless SP_MATCH is also
 *                   set, this will not return exact matches. (For example,
 *                   it is a bad idea to do a STATUS command on an already
 *                   selected mailbox. There may be locking problems if you
 *                   try to delete a folder that is selected...)
 *     SP_TEMPUSE -- The checking for SP_SAME streams is controlled by these
 *    SP_UNLOCKED    two flags. If SP_TEMPUSE is set then we will only match
 *                   streams which are marked TEMPUSE and not LOCKED.
 *                   If TEMPUSE is not set but UNLOCKED is, then we will
 *                   match on any same stream that is not locked. We'll choose
 *                   SP_TEMPUSE streams in preference to those that aren't
 *                   SP_TEMPUSE. If neither SP_TEMPUSE or SP_UNLOCKED is set,
 *                   then we'll consider any stream, even if it is locked.
 *                   We'll still prefer TEMPUSE first, then UNLOCKED, then any.
 *
 * Careful with the values of these flags. Some of them should really be
 * in separate name spaces, but we've combined all of them for convenience.
 * In particular, SP_USERFLDR, SP_INBOX, SP_USEPOOL, and SP_TEMPUSE are
 * all passed in the pine_mail_open flags argument, alongside OP_DEBUG and
 * friends from c-client. So they have to have different values than
 * those OP_ flags. SP_PERMLOCKED was passed at one time but isn't anymore.
 * Still, include it in the careful set. C-client reserves the bits
 * 0xff000000 for client flags.
 */

#define SP_USERFLDR	0x01000000
#define SP_INBOX	0x02000000
#define SP_USEPOOL	0x04000000
#define SP_TEMPUSE	0x08000000

#define SP_PERMLOCKED	0x10000000
#define SP_LOCKED	0x20000000

#define SP_MATCH	0x00100000
#define SP_SAME		0x00200000
#define SP_UNLOCKED	0x00400000
#define SP_FILTERED	0x00800000
#define SP_RO_OK	0x01000000	/* Readonly stream ok for SP_MATCH */

/* these are for icache_flags */
#define SP_NEED_FORMAT_SETUP         0x01
#define SP_FORMAT_INCLUDES_MSGNO     0x02
#define SP_FORMAT_INCLUDES_SMARTDATE 0x04


/* access value of first_unseen, but don't set it with this */
#define sp_first_unseen(stream)                                              \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->first_unseen : 0L)

/* use this to set it */
#define sp_set_first_unseen(stream,val) do{                                  \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->first_unseen = (val);}while(0)

#define sp_flags(stream)                                                     \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->flags : 0L)

#define sp_set_flags(stream,val) do{                                         \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->flags = (val);}while(0)

#define sp_icache_flags(stream)                                              \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->icache_flags : 0L)

#define sp_set_icache_flags(stream,val) do{                                  \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->icache_flags = (val);}while(0)

#define sp_ref_cnt(stream)                                                   \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->ref_cnt : 0L)

#define sp_set_ref_cnt(stream,val) do{                                       \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->ref_cnt = (val);}while(0)

#define sp_expunge_count(stream)                                             \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->expunge_count : 0L)

#define sp_set_expunge_count(stream,val) do{                                 \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->expunge_count = (val);}while(0)

#define sp_new_mail_count(stream)                                            \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->new_mail_count : 0L)

#define sp_set_new_mail_count(stream,val) do{                                \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->new_mail_count = (val);}while(0)

#define sp_mail_since_cmd(stream)                                            \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->mail_since_cmd : 0L)

#define sp_set_mail_since_cmd(stream,val) do{                                \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->mail_since_cmd = (val);}while(0)

#define sp_recent_since_visited(stream)                                      \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->recent_since_visited : 0L)

#define sp_set_recent_since_visited(stream,val) do{                          \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->recent_since_visited = (val);}while(0)

#define sp_check_cnt(stream)                                                 \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->check_cnt : 0L)

#define sp_set_check_cnt(stream,val) do{                                     \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->check_cnt = (val);}while(0)

#define sp_first_status_change(stream)                                      \
			    ((sp_data(stream) && *sp_data(stream))          \
			      ? (*sp_data(stream))->first_status_change : 0L)

#define sp_set_first_status_change(stream,val) do{                          \
		    if(sp_data(stream) && *sp_data(stream))                 \
		      (*sp_data(stream))->first_status_change = (val);}while(0)

#define sp_last_ping(stream)                                                \
			    ((sp_data(stream) && *sp_data(stream))          \
			      ? (*sp_data(stream))->last_ping : 0L)

#define sp_set_last_ping(stream,val) do{                                    \
		    if(sp_data(stream) && *sp_data(stream))                 \
		      (*sp_data(stream))->last_ping = (val);}while(0)

#define sp_last_expunged_reaper(stream)                                     \
			    ((sp_data(stream) && *sp_data(stream))          \
			      ? (*sp_data(stream))->last_expunged_reaper : 0L)

#define sp_set_last_expunged_reaper(stream,val) do{                         \
		    if(sp_data(stream) && *sp_data(stream))                 \
		      (*sp_data(stream))->last_expunged_reaper = (val);}while(0)

#define sp_last_chkpnt_done(stream)                                         \
			    ((sp_data(stream) && *sp_data(stream))          \
			      ? (*sp_data(stream))->last_chkpnt_done : 0L)

#define sp_set_last_chkpnt_done(stream,val) do{                             \
		    if(sp_data(stream) && *sp_data(stream))                 \
		      (*sp_data(stream))->last_chkpnt_done = (val);}while(0)

#define sp_last_use_time(stream)                                            \
			    ((sp_data(stream) && *sp_data(stream))          \
			      ? (*sp_data(stream))->last_use_time : 0L)

#define sp_set_last_use_time(stream,val) do{                                \
		    if(sp_data(stream) && *sp_data(stream))                 \
		      (*sp_data(stream))->last_use_time = (val);}while(0)

#define sp_last_activity(stream)                                            \
			    ((sp_data(stream) && *sp_data(stream))          \
			      ? (*sp_data(stream))->last_activity : 0L)

#define sp_set_last_activity(stream,val) do{                                \
		    if(sp_data(stream) && *sp_data(stream))                 \
		      (*sp_data(stream))->last_activity = (val);}while(0)

#define sp_saved_uid_validity(stream)                                        \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->saved_uid_validity : 0L)

#define sp_set_saved_uid_validity(stream,val) do{                            \
		    if(sp_data(stream) && *sp_data(stream))                  \
		     (*sp_data(stream))->saved_uid_validity = (val);}while(0)

#define sp_saved_uid_last(stream)                                            \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->saved_uid_last : 0L)

#define sp_set_saved_uid_last(stream,val) do{                                \
		    if(sp_data(stream) && *sp_data(stream))                  \
		     (*sp_data(stream))->saved_uid_last = (val);}while(0)

#define sp_mail_box_changed(stream)                                          \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->mail_box_changed : 0L)

#define sp_set_mail_box_changed(stream,val) do{                              \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->mail_box_changed = (val);}while(0)

#define sp_unsorted_newmail(stream)                                          \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->unsorted_newmail : 0L)

#define sp_set_unsorted_newmail(stream,val) do{                              \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->unsorted_newmail = (val);}while(0)

#define sp_need_to_rethread(stream)                                          \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->need_to_rethread : 0L)

#define sp_set_need_to_rethread(stream,val) do{                              \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->need_to_rethread = (val);}while(0)

#define sp_viewing_a_thread(stream)                                          \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->viewing_a_thread : 0L)

#define sp_set_viewing_a_thread(stream,val) do{                              \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->viewing_a_thread = (val);}while(0)

#define sp_dead_stream(stream)                                               \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->dead_stream : 0L)

#define sp_set_dead_stream(stream,val) do{                                   \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->dead_stream = (val);}while(0)

#define sp_noticed_dead_stream(stream)                                       \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->noticed_dead_stream : 0L)

#define sp_set_noticed_dead_stream(stream,val) do{                           \
		if(sp_data(stream) && *sp_data(stream))                      \
		  (*sp_data(stream))->noticed_dead_stream = (val);}while(0)

#define sp_closing(stream)                                                   \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->closing : 0L)

#define sp_set_closing(stream,val) do{                                       \
		    if(sp_data(stream) && *sp_data(stream))                  \
		      (*sp_data(stream))->closing = (val);}while(0)

#define sp_io_error_on_stream(stream)                                        \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->io_error_on_stream : 0L)

#define sp_set_io_error_on_stream(stream,val) do{                            \
		  if(sp_data(stream) && *sp_data(stream))                    \
		    (*sp_data(stream))->io_error_on_stream = (val);}while(0)

#define sp_fldr(stream)                                                      \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->fldr : (char *) NULL)

#define sp_saved_cur_msg_id(stream)                                          \
		    ((sp_data(stream) && *sp_data(stream))                   \
		      ? (*sp_data(stream))->saved_cur_msg_id : (char *) NULL)

#define sp_context(stream)                                                   \
			    ((sp_data(stream) && *sp_data(stream))           \
			      ? (*sp_data(stream))->context : 0L)

#define sp_set_context(stream,val) do{                                       \
		  if(sp_data(stream) && *sp_data(stream))                    \
		    (*sp_data(stream))->context = (val);}while(0)


extern MAILSTATUS *pine_cached_status;


/*
 * Macros to help fetch specific fields
 */
#define	pine_fetchheader_lines(S,N,M,F)	     pine_fetch_header(S,N,M,F,0L)
#define	pine_fetchheader_lines_not(S,N,M,F)  pine_fetch_header(S,N,M,F,FT_NOT)


/* exported protoypes */
MAILSTREAM    *pine_mail_open(MAILSTREAM *, char *, long, long *);
long	       pine_mail_create(MAILSTREAM *, char *);
long	       pine_mail_delete(MAILSTREAM *, char *);
long	       pine_mail_append_full(MAILSTREAM *, char *, char *, char *, STRING *);
#define pine_mail_append(stream,mailbox,message) \
   pine_mail_append_full(stream,mailbox,NULL,NULL,message)
long	       pine_mail_append_multiple(MAILSTREAM *, char *, append_t, APPENDPACKAGE *, MAILSTREAM *);
long	       pine_mail_copy_full(MAILSTREAM *, char *, char *, long);
#define pine_mail_copy(stream,sequence,mailbox) \
   pine_mail_copy_full(stream,sequence,mailbox,0)
long	       pine_mail_rename(MAILSTREAM *, char *, char *);
void	       pine_mail_close(MAILSTREAM *);
void	       pine_mail_actually_close(MAILSTREAM *);
void	       maybe_kill_old_stream(MAILSTREAM *);
long	       pine_mail_search_full(MAILSTREAM *, char *, SEARCHPGM *, long);
void	       pine_mail_fetch_flags(MAILSTREAM *, char *, long);
ENVELOPE      *pine_mail_fetchenvelope(MAILSTREAM *, unsigned long);
ENVELOPE      *pine_mail_fetch_structure(MAILSTREAM *, unsigned long, BODY **, long);
ENVELOPE      *pine_mail_fetchstructure(MAILSTREAM *, unsigned long, BODY **);
char	      *pine_mail_fetch_body(MAILSTREAM *, unsigned long, char *, unsigned long *, long);
char	      *pine_mail_fetch_text(MAILSTREAM *, unsigned long, char *, unsigned long *, long);
char	      *pine_mail_partial_fetch_wrapper(MAILSTREAM *, unsigned long, char *, unsigned long *,
					       long, unsigned long, char **, int);
long	       pine_mail_ping(MAILSTREAM *);
void	       pine_mail_check(MAILSTREAM *);
int	       pine_mail_list(MAILSTREAM *, char *, char *, unsigned *);
long	       pine_mail_status(MAILSTREAM *, char *, long);
long	       pine_mail_status_full(MAILSTREAM *, char *, long, imapuid_t *, imapuid_t *);
int	       check_for_move_mbox(char *, char *, size_t, char **);
MAILSTREAM    *already_open_stream(char *, int);
void	       pine_imap_cmd_happened(MAILSTREAM *, char *, long);
void	       sp_cleanup_dead_streams(void);
int	       sp_flagged(MAILSTREAM *, unsigned long);
void	       sp_set_fldr(MAILSTREAM *, char *);
void	       sp_set_saved_cur_msg_id(MAILSTREAM *, char *);
void	       sp_flag(MAILSTREAM *, unsigned long);
void	       sp_unflag(MAILSTREAM *, unsigned long);
void	       sp_mark_stream_dead(MAILSTREAM *);
int	       sp_nremote_permlocked(void);
MAILSTREAM    *sp_stream_get(char *, unsigned long);
void	       sp_end(void);
int	       sp_a_locked_stream_is_dead(void);
int	       sp_a_locked_stream_changed(void);
MAILSTREAM    *sp_inbox_stream(void);
PER_STREAM_S **sp_data(MAILSTREAM *);
MSGNO_S	      *sp_msgmap(MAILSTREAM *);
void	       sp_free_callback(void **);
MAILSTREAM    *same_stream(char *, MAILSTREAM *);
MAILSTREAM    *same_stream_and_mailbox(char *, MAILSTREAM *);
int	       same_remote_mailboxes(char *, char *);
int	       is_imap_stream(MAILSTREAM *);
int	       modern_imap_stream(MAILSTREAM *);
int	       streams_died(void);
void           appenduid_cb(char *mailbox,unsigned long uidvalidity, SEARCHSET *set);
imapuid_t      get_last_append_uid(void);
MAILSTREAM    *mail_cmd_stream(CONTEXT_S *, int *);
long           dummy_soutr(void *, char *);


#endif /* PITH_STREAM_INCLUDED */
