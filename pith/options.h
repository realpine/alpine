/*
 * $Id: options.h 101 2006-08-10 22:53:04Z mikes@u.washington.edu $
 *
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

#ifndef PITH_OPTIONS_INCLUDED
#define PITH_OPTIONS_INCLUDED

/*
 * function hooks to fill in optional/user-selectable behaviors in pith functions
 *
 * You'll find a very brief explanation of what they do here, but you'll need to
 * look at the places they're called to fully understand how they're intended to
 * be called and what they're intended to provide.
 */


#include "indxtype.h"			/* for ICE_S */
#include "thread.h"			/* for PINETHRD_S */
#include "handle.h"			/* for HANDLE_S */
#include "filttype.h"			/* for gf_io_t */
#include "state.h"			/* for struct pine */
#include "adrbklib.h"			/* for SAVE_STATE_S */
#ifdef	ENABLE_LDAP
#include "ldap.h"
#endif


/*
 * optional call in mailindx.c:{from,subject}_str to shrink thread 
 * relationship cue if desired
 */
extern int (*pith_opt_condense_thread_cue)(PINETHRD_S *, ICE_S *, char **, size_t *, int, int);


/*
 * optional call in mailindx.c:setup_header_widths to save various bits
 * of format state
 */
extern void (*pith_opt_save_index_state)(int);


/*
 * optional call in mailindx.c:load_overview to paint the gathered overview data
 * on the display
 */
extern void (*pith_opt_paint_index_hline)(MAILSTREAM *, long, ICE_S *);

/*
 * optional hook in mailview.c:format_message to allow for inserting an
 * [editorial commment] in message text to indicate the message contains
 * list-management pointers
 */
extern int  (*pith_opt_rfc2369_editorial)(long, HANDLE_S **, int, int, gf_io_t);

#ifdef	ENABLE_LDAP
/*
 * optional hook in ldap.c:wp_lookups to ask user where to save chosen LDAP result
 */
extern void (*pith_opt_save_ldap_entry)(struct pine *, LDAP_CHOOSE_S *, int);
#endif

/*
 * optional hook in addrbook.c:bunch-of-funcs to allow saving/restoring
 * state (screen state and such) before and after calls that might be
 * reentered
 */
extern void (*pith_opt_save_and_restore)(int, SAVE_STATE_S *);

/*
 * optional hooks in newmail.c:new_mail to allow for various indicators
 * during the new mail check/arrival and checkpoint process
 */
extern void (*pith_opt_newmail_announce)(MAILSTREAM *, long, long);
extern void (*pith_opt_newmail_check_cue)(int);
extern void (*pith_opt_checkpoint_cue)(int);
extern void (*pith_opt_icon_text)(char *, int);

/*
 * optional hook in remote.c to provide name for storing address book
 * metadata
 */
extern char *(*pith_opt_rd_metadata_name)(void);

/*
 * optional hook in conf.c:read_pinerc to let caller deal with hard
 * unreadable remote config file error
 * Return TRUE to continue, FALSE otherwise
 */
extern int (*pith_opt_remote_pinerc_failure)(void);

/*
 * optional hook in mailcmd.c:do_broach_folder allowing for user prompt
 * of closed folder open.
 * Return -1 on cancel, zero otherwise.  Set second arg by reference
 * to TRUE for reopen.
 */
extern int (*pith_opt_reopen_folder)(struct pine *, int *);

/*
 * optional call in mailcmd.c:expunge_and_close to prompt for read message removal 
 */
extern int (*pith_opt_read_msg_prompt)(long, char *);

/*
 * optional hook in mailcmd.c:expunge_and_close to prompt for expunge
 * confirmation.  Return 'y' to expunge/delete. Do not allow cancel.
 */
extern int (*pith_opt_expunge_prompt)(MAILSTREAM *, char *, long);

/*
 * optional hook in mailcmd.c:expunge_and_close called when a folder is
 * about to be closed and expunged.  Flags passed to expunge_and_close are
 * in turn passed in this call.
 */
extern void (*pith_opt_begin_closing)(int, char *);

/*
 * optional hook in reply.c:reply_harvet to allow for user selection
 * of reply-to vs. from address
 * Return 'y' to use "reply-to" field.
 */
extern int (*pith_opt_replyto_prompt)(void);


/*
 * optional hook in reply.c:reply_harvet to allow for user choice
 * of reply to all vs just sender
 * Return -1 to cancel reply altogether, set reply flag by reference
 */
extern int (*pith_opt_reply_to_all_prompt)(int *);


/*
 * optional hook in save.c:create_for_save to allow for user confirmation
 * of folder being created.
 * Return: 1 for proceed, -1 for decline, 0 for error
 */
extern int (*pith_opt_save_create_prompt)(CONTEXT_S *, char *, int);


/*
 * optional hook in send.c:check_addresses to allow for user confirmation
 * of sending to MAILER-DAEMON
 */
extern int (*pith_opt_daemon_confirm)(void);


/*
 * optional hook in save.c to prompt for permission to continue save
 * in spite of size error.  Return 'y' to continue or 'a' to answer
 * yes to all until next reinitialization of the function.
 */
extern int (*pith_opt_save_size_changed_prompt)(long, int);


/*
 * optional hook to process filter patterns using external command
 * on message contents
 */
extern void (*pith_opt_filter_pattern_cmd)(char **, SEARCHSET *, MAILSTREAM *, long, INTVL_S *);


/*
 * Hook to read signature from local file
 */
extern char *(*pith_opt_get_signature_file)(char *, int, int, int);


/*
 * Hook to make variable names pretty in help text
 */
extern char *(*pith_opt_pretty_var_name)(char *);


/*
 * Hook to make feature names pretty in help text
 */
extern char *(*pith_opt_pretty_feature_name)(char *, int);


/*
 * optional hook in mailindx.c:{from,subject}_str to cause the returned
 * string to be truncated at the formatted width. 1 truncates.
 * This is useful because the truncated string ends slightly
 * differently than it would if it weren't truncated, for
 * example it might end ... or something like that.
 */
extern int (*pith_opt_truncate_sfstr)(void);


/*
 * A stream is being closed. If there is something in the
 * application that needs to react to that handle it here.
 */
extern void (*pith_opt_closing_stream)(MAILSTREAM *);


/*
 * Callback from mm_expunged to let us know the "current"
 * message was expunged
 */
extern void (*pith_opt_current_expunged)(long unsigned int);

/*
 * Option User-Agent Header Prefix
 */
extern char *(*pith_opt_user_agent_prefix)(void);


/*
 * optional call to prompt for S/MIME passphase
 */
extern int (*pith_opt_smime_get_passphrase)(void);


#endif /* PITH_OPTIONS_INCLUDED */
