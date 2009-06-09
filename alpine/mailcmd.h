/*
 * $Id: mailcmd.h 453 2007-02-27 00:10:47Z hubert@u.washington.edu $
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

#ifndef PINE_MAILCMD_INCLUDED
#define PINE_MAILCMD_INCLUDED


#include <general.h>
#include "context.h"
#include "mailview.h"
#include "radio.h"
#include "../pith/mailcmd.h"
#include "../pith/mailindx.h"
#include "../pith/state.h"
#include "../pith/msgno.h"
#include "../pith/store.h"
#include "../pith/filter.h"


#define USER_INPUT_TIMEOUT(ps) ((ps->hours_to_timeout > 0) && \
  ((time(0) - time_of_last_input()) > 60*60*(ps->hours_to_timeout)))


#define GE_NONE			0x00	/* get_export_filename flags    */
#define GE_IS_EXPORT		0x01	/* include EXPORT: in prompt    */
#define GE_SEQ_SENSITIVE	0x02	/* Sensitive to seq # changes   */
#define GE_NO_APPEND		0x04	/* No appending to file allowed */
#define GE_IS_IMPORT		0x08	/* No writing of file           */
#define GE_ALLPARTS		0x10	/* Add AllParts toggle to options */

#define GER_NONE		0x00	/* get_export_filename return flags */
#define GER_OVER		0x01	/* overwrite of existing file       */
#define GER_APPEND		0x02	/* append of existing file          */
#define GER_ALLPARTS		0x04	/* AllParts toggle is on            */


typedef enum {DontAsk, NoDel, Del, RetNoDel, RetDel} SaveDel;

typedef enum {View, MsgIndx, ThrdIndx} CmdWhere;


/* exported protoypes */
int	    process_cmd(struct pine *, MAILSTREAM *, MSGNO_S *, int, CmdWhere, int *);
char	   *pretty_command(UCS);
void	    bogus_command(UCS, char *);
void	    bogus_utf8_command(char *, char *);
int	    save_prompt(struct pine *, CONTEXT_S **, char *, size_t, 
			char *, ENVELOPE *, long, char *, SaveDel *, int *);
int	    create_for_save_prompt(CONTEXT_S *, char *, int);
int	    expunge_prompt(MAILSTREAM *, char *, long);
void	    expunge_and_close_begins(int, char *);
int         simple_export(struct pine *, void *, SourceType, char *, char *);
int         get_export_filename(struct pine *, char *, char *, char *, size_t, char *,
				char *, ESCKEY_S *, int *, int, int);
char	   *build_updown_cmd(char *, size_t, char *, char *, char*);
int	    bezerk_delimiter(ENVELOPE *, MESSAGECACHE *, gf_io_t, int);
long	    jump_to(MSGNO_S *, int, int, SCROLL_S *, CmdWhere);
char	   *broach_folder(int, int, int *, CONTEXT_S **);
int	    ask_mailbox_reopen(struct pine *, int *);
void	    visit_folder(struct pine *, char *, CONTEXT_S *, MAILSTREAM *, unsigned long);
int	    select_by_current(struct pine *, MSGNO_S *, CmdWhere);
int	    apply_command(struct pine *, MAILSTREAM *, MSGNO_S *, UCS, int, int);
char      **choose_list_of_keywords(void);
char      **choose_list_of_charsets(void);
int	    display_folder_list(CONTEXT_S **, char *, int,
				int (*)(struct pine *, CONTEXT_S **, char *, int));
int	    file_lister(char *, char *, size_t, char *, size_t, int, int);
int	    read_msg_prompt(long, char *);
void        advance_cur_after_delete(struct pine *, MAILSTREAM *, MSGNO_S *, CmdWhere);
#ifdef	_WINDOWS
int	    header_mode_callback(int, long);
int	    zoom_mode_callback(int, long);
int	    any_selected_callback(int, long);
int	    flag_callback(int, long);
MPopup	   *flag_submenu(MESSAGECACHE *);
#endif


#endif /* PINE_MAILCMD_INCLUDED */
