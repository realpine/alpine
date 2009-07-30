/*
 * $Id: folder.h 880 2007-12-18 00:57:56Z hubert@u.washington.edu $
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

#ifndef PITH_FOLDER_INCLUDED
#define PITH_FOLDER_INCLUDED


#include "../pith/foldertype.h"
#include "../pith/conftype.h"
#include "../pith/context.h"
#include "../pith/state.h"


/*
 * Structs to ease c-client LIST/LSUB interaction
 */
typedef struct _listargs {
    char *reference,		/* IMAP LIST "reference" arg	    */
	 *name,			/* IMAP LIST "name" arg		    */
	 *tail;			/* Pine "context" after "name" part */
} LISTARGS_S;

typedef struct _listresponse {
    long     count;
    int	     delim;
    unsigned isfile:1,
	     isdir:1,
	     ismarked:1,
	     unmarked:1,
	     haschildren:1,
	     hasnochildren:1;
} LISTRES_S;


typedef struct _existdata {
    LISTARGS_S	 args;
    LISTRES_S	 response;
    char       **fullname;
    int          is_move_folder;
} EXISTDATA_S;


#define	FEX_NOENT	0x0000		/* file_exists: doesn't exist	    */
#define	FEX_ISFILE	0x0001		/* file_exists: name is a file	    */
#define	FEX_ISDIR	0x0002		/* file_exists: name is a dir	    */
#define	FEX_ISMARKED	0x0004		/* file_exists: is interesting	    */
#define	FEX_UNMARKED	0x0008		/* file_exists: known UNinteresting */
#define	FEX_ERROR	0x1000		/* file_exists: error occured	    */

#define	BFL_NONE	0x00		/* build_folder_list: no flag */
#define	BFL_FLDRONLY	0x01		/* ignore directories	      */
#define	BFL_LSUB	0x02		/* use mail_lsub vs mail_list */
#define	BFL_SCAN	0x04		/* use mail_scan vs mail_list */
#define	BFL_CHILDREN	0x08		/* make sure haschildren is accurate */

#define	FI_FOLDER	0x01		/* folder_index flags */
#define	FI_DIR		0x02
#define	FI_RENAME	0x04
#define	FI_ANY		(FI_FOLDER | FI_DIR)

#define	FN_NONE		0x00		/* flags modifying folder_is_nick */
#define	FN_WHOLE_NAME	0x01		/* return long name if #move      */

#define	FC_NONE		0		/* flags for folder_complete */
#define	FC_FORCE_LIST	1

#define	UFU_NONE	0x00		/* flags for update_folder_unseen */
#define	UFU_FORCE	0x01
#define	UFU_ANNOUNCE	0x02		/* announce increases with q_status */


/* exported protoypes */
char	   *folder_lister_desc(CONTEXT_S *, FDIR_S *);
void	    reset_context_folders(CONTEXT_S *);
FDIR_S	   *next_folder_dir(CONTEXT_S *, char *, int, MAILSTREAM **);
EditWhich   config_containing_inc_fldr(FOLDER_S *);
char	   *pretty_fn(char *);
int	    get_folder_delimiter(char *);
int	    folder_exists(CONTEXT_S *, char *);
int	    folder_name_exists(CONTEXT_S *, char *, char **);
char	   *folder_as_breakout(CONTEXT_S *, char *);
void	    init_folders(struct pine *);
void	    init_inbox_mapping(char *, CONTEXT_S *);
void	    reinit_incoming_folder_list(struct pine *, CONTEXT_S *);
FDIR_S	   *new_fdir(char *, char *, int);
void	    free_fdir(FDIR_S **, int);
void	    build_folder_list(MAILSTREAM **, CONTEXT_S *, char *, char *, int);
void	    free_folder_list(CONTEXT_S *);
CONTEXT_S  *default_save_context(CONTEXT_S *);
int	    folder_complete(CONTEXT_S *, char *, size_t, int *);
FOLDER_S   *new_folder(char *, unsigned long);
FOLDER_S   *folder_entry(int, FLIST *);
int         folder_total(FLIST *);
int	    folder_index(char *, CONTEXT_S *, int);
char	   *folder_is_nick(char *, FLIST *, int);
char	   *folder_is_target_of_nick(char *, CONTEXT_S *);
int	    folder_insert(int, FOLDER_S *, FLIST *);
FLIST      *init_folder_entries(void);
int	    compare_folders_alpha(FOLDER_S *, FOLDER_S *);
int	    compare_folders_dir_alpha(FOLDER_S *, FOLDER_S *);
int	    compare_folders_alpha_dir(FOLDER_S *, FOLDER_S *);
void	    mail_list_response(MAILSTREAM *, char *, int, long, void *, unsigned);
void	    folder_seen_count_updater(void *);
void	    folder_unseen_count_updater(unsigned long);
void	    update_folder_unseen(FOLDER_S *, CONTEXT_S *, unsigned long, MAILSTREAM *);
void        update_folder_unseen_by_stream(MAILSTREAM *, unsigned long);
int	    get_recent_in_folder(char *, unsigned long *, unsigned long *,
				 unsigned long *, MAILSTREAM *);
void        clear_incoming_valid_bits(void);
int	    selected_folders(CONTEXT_S *);
SELECTED_S *new_selected(void);
void	    free_selected(SELECTED_S **);
void	    folder_select_preserve(CONTEXT_S *);
int	    folder_select_restore(CONTEXT_S *);
int	    update_bboard_spec(char *, char *, size_t);
void	    refresh_folder_list(CONTEXT_S *, int, int, MAILSTREAM **);
int	    folder_complete_internal(CONTEXT_S *, char *, size_t, int *, int);
void        folder_delete(int, FLIST *);


#endif /* PITH_FOLDER_INCLUDED */
