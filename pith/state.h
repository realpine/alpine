/*
 * $Id: state.h 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $
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

#ifndef PITH_STATE_INCLUDED
#define PITH_STATE_INCLUDED


#include "../pith/conftype.h"
#include "../pith/indxtype.h"
#include "../pith/bitmap.h"
#include "../pith/charset.h"
#include "../pith/context.h"
#include "../pith/keyword.h"
#include "../pith/atttype.h"
#include "../pith/msgno.h"
#include "../pith/pattern.h"
#include "../pith/pipe.h"
#include "../pith/send.h"
#include "../pith/sorttype.h"
#include "../pith/stream.h"
#include "../pith/color.h"
#include "../pith/user.h"


/*
 * Printing control structure
 */
typedef struct print_ctrl {
#ifndef	DOS
    PIPE_S	*pipe;		/* control struct for pipe to write text */
    FILE	*fp;		/* file pointer to write printed text into */
    char	*result;	/* file containing print command's output */
#endif
#ifdef	OS2
    int		ispipe;
#endif
    int		err;		/* bit indicating something went awry */
} PRINT_S;


/*
 * Keeps track of display dimensions
 */
struct ttyo {
    int	screen_rows,
	screen_cols,
	header_rows,	/* number of rows for titlebar and whitespace */
	footer_rows;	/* number of rows for status and keymenu      */
};

/*
 * HEADER_ROWS is always 2.  1 for the titlebar and 1 for the
 * blank line after the titlebar.  We should probably make it go down
 * to 0 when the screen shrinks but instead we're just figuring out
 * if there is enough room by looking at screen_rows.
 * FOOTER_ROWS is either 3 or 1.  Normally it is 3, 2 for the keymenu plus 1
 * for the status line.  If the NoKeyMenu command has been given, then it is 1.
 */
#define HEADER_ROWS(X) ((X)->ttyo->header_rows)
#define FOOTER_ROWS(X) ((X)->ttyo->footer_rows)


/*----------------------------------------------------------------------
   This structure sort of takes the place of global variables or perhaps
is the global variable.  (It can be accessed globally as ps_global.  One
advantage to this is that as soon as you see a reference to the structure
you know it is a global variable. 
   In general it is treated as global by the lower level and utility
routines, but it is not treated so by the main screen driving routines.
Each of them receives it as an argument and then sets ps_global to the
argument they received.  This is sort of with the thought that things
might be coupled more loosely one day and that Pine might run where there
is more than one window and more than one instance.  But we haven't kept
up with this convention very well.
 ----*/
  
struct pine {
    void       (*next_screen)(struct pine *);	/* See loop at end of main() for how */
    void       (*prev_screen)(struct pine *);	/* these are used...		     */
    void       (*redrawer)(void);	/* NULL means stay in current screen */

    CONTEXT_S   *context_list;		/* list of user defined contexts */
    CONTEXT_S   *context_current;	/* default open context          */
    CONTEXT_S   *context_last;		/* most recently open context    */

    SP_S         s_pool;		/* stream pool */

    char         inbox_name[MAXFOLDER+1];
    char         pine_pre_vers[10];	/* highest version previously run */
    char         vers_internal[10];
    
    MAILSTREAM  *mail_stream;		/* ptr to current folder stream */
    MSGNO_S	*msgmap;		/* ptr to current message map   */

    unsigned     read_predicted:1;

    char         cur_folder[MAXPATH+1];
    char         last_unambig_folder[MAXPATH+1];
    char         last_save_folder[MAXPATH+1];
    CONTEXT_S   *last_save_context;
    ATTACH_S    *atmts;
    int          atmts_allocated;
    int	         remote_abook_validity;	/* minutes, -1=never, 0=only on opens */

    INDEX_COL_S *index_disp_format;

    char        *folders_dir;

    unsigned     mangled_footer:1; 	/* footer needs repainting */
    unsigned     mangled_header:1;	/* header needs repainting */
    unsigned     mangled_body:1;	/* body of screen needs repainting */
    unsigned     mangled_screen:1;	/* whole screen needs repainting */

    unsigned     in_init_seq:1;		/* executing initial cmd list */
    unsigned     save_in_init_seq:1;
    unsigned     dont_use_init_cmds:1;	/* use keyboard input when true */

    unsigned     give_fixed_warning:1;	/* warn user about "fixed" vars */
    unsigned     fix_fixed_warning:1;	/* offer to fix it              */

    unsigned     user_says_cancel:1;	/* user typed ^C to abort open */

    unsigned     unseen_in_view:1;
    unsigned     start_in_context:1;	/* start fldr_scrn in current cntxt */
    unsigned     def_sort_rev:1;	/* true if reverse sort is default  */ 
    unsigned     restricted:1;

    unsigned	 save_msg_rule:5;
    unsigned	 fcc_rule:3;
    unsigned	 ab_sort_rule:3;
    unsigned     color_style:3;
    unsigned     index_color_style:3;
    unsigned     titlebar_color_style:3;
    unsigned	 fld_sort_rule:3;
    unsigned	 inc_startup_rule:3;
    unsigned	 pruning_rule:3;
    unsigned	 reopen_rule:4;
    unsigned	 goto_default_rule:3;
    unsigned	 thread_disp_style:3;
    unsigned	 thread_index_style:3;

    unsigned     full_header:2;         /* display full headers		   */
					/* 0 means normal		   */
					/* 1 means display all quoted text */
					/* 2 means full headers		   */
    unsigned     some_quoting_was_suppressed:1;
    unsigned     orig_use_fkeys:1;
    unsigned     try_to_create:1;	/* Save should try mail_create */
    unsigned     low_speed:1;	      /* various opt's 4 low connect speed */
    unsigned     postpone_no_flow:1;  /* don't set flowed when we postpone */
				      /* and don't reflow when we resume.  */
    unsigned     mm_log_error:1;
    unsigned     show_new_version:1;
    unsigned     pre441:1;
    unsigned     first_time_user:1;
    unsigned	 intr_pending:1;	/* received SIGINT and haven't acted */
    unsigned	 expunge_in_progress:1;	/* don't want to re-enter c-client   */
    unsigned	 never_allow_changing_from:1;	/* not even for roles */

    unsigned	 readonly_pinerc:1;
    unsigned	 view_all_except:1;
    unsigned     start_in_index:1;	/* cmd line flag modified on startup */
    unsigned     noshow_error:1;	/* c-client error callback controls */
    unsigned     noshow_warn:1;
    unsigned	 noshow_timeout:1;
    unsigned	 conceal_sensitive_debugging:1;
    unsigned	 turn_off_threading_temporarily:1;
    unsigned	 view_skipped_index:1;
    unsigned	 a_format_contains_score:1;
    unsigned	 ugly_consider_advancing_bit:1;
    unsigned	 dont_count_flagchanges:1;
    unsigned	 in_folder_screen:1;
    unsigned	 noticed_change_in_unseen:1;
    unsigned	 first_open_was_attempted:1;
    unsigned	 force_prefer_plain:1;
    unsigned	 force_no_prefer_plain:1;

    unsigned	 phone_home:1;
    unsigned     painted_body_on_startup:1;
    unsigned     painted_footer_on_startup:1;
    unsigned     open_readonly_on_startup:1;
    unsigned     exit_if_no_pinerc:1;
    unsigned     pass_ctrl_chars:1;
    unsigned     pass_c1_ctrl_chars:1;
    unsigned     display_keywords_in_subject:1;
    unsigned     display_keywordinits_in_subject:1;
    unsigned     beginning_of_month:1;
    unsigned     beginning_of_year:1;

    unsigned 	 viewer_overlap:8;
    unsigned	 scroll_margin:8;
    unsigned 	 remote_abook_history:8;

#if defined(DOS) || defined(OS2)
    unsigned     blank_user_id:1;
    unsigned     blank_personal_name:1;
    unsigned     blank_user_domain:1;
#ifdef	_WINDOWS
    unsigned	 update_registry:2;
    unsigned     install_flag:1;
#endif
#endif

    unsigned 	 debug_malloc:6;
    unsigned 	 debug_timestamp:1;
    unsigned 	 debug_flush:1;
    unsigned 	 debug_tcp:1;
    unsigned 	 debug_imap:3;
    unsigned 	 debug_nfiles:5;
    unsigned     debugmem:1;
#ifdef	LOCAL_PASSWD_CACHE
    unsigned     nowrite_password_cache:1;
#endif

    unsigned     convert_sigs:1;
    unsigned     dump_supported_options:1;

    unsigned	 noexpunge_on_close:1;

    unsigned	 no_newmail_check_from_optionally_enter:1;

    unsigned	 post_utf8:1;

    unsigned     start_entry;		/* cmd line arg: msg # to start on */

    bitmap_t     feature_list;		/* a bitmap of all the features */
    char       **feat_list_back_compat;

    SPEC_COLOR_S *hdr_colors;		/* list of configed colors for view */

    short	 init_context;

    int         *initial_cmds;         /* cmds to execute on startup */
    int         *free_initial_cmds;    /* used to free when done */

    char         c_client_error[300];  /* when nowhow_error is set and PARSE */

    struct ttyo *ttyo;

    USER_S	 ui;		/* system derived user info */

    POST_S      *post;

    char	*home_dir,
                *hostname,	/* Fully qualified hostname */
                *localdomain,	/* The (DNS) domain this host resides in */
                *userdomain,	/* The per user domain from .pinerc or */
                *maildomain,	/* Domain name for most uses */
#if defined(DOS) || defined(OS2)
                *pine_dir,	/* argv[0] as provided by DOS */
                *aux_files_dir,	/* User's auxiliary files directory */
#endif
#ifdef PASSFILE
                *passfile,
#endif /* PASSFILE */
                *pinerc,	/* Location of user's pinerc */
                *exceptions,	/* Location of user's exceptions */
		*pine_name;	/* name we were invoked under */
    PINERC_S    *prc,		/* structure for personal pinerc */
		*post_prc,	/* structure for post-loaded pinerc */
		*pconf;		/* structure for global pinerc */
    
    EditWhich	 ew_for_except_vars;
    EditWhich	 ew_for_role_take;
    EditWhich	 ew_for_score_take;
    EditWhich	 ew_for_filter_take;
    EditWhich	 ew_for_incol_take;
    EditWhich	 ew_for_other_take;
    EditWhich	 ew_for_srch_take;

    SortOrder    def_sort,	/* Default sort type */
		 sort_types[22];

    int          last_expire_year, last_expire_month;

    int		 printer_category;

    int		 status_msg_delay;

    int		 active_status_interval;

    int		 composer_fillcol;

    int		 nmw_width;

    int          hours_to_timeout;

    int          tcp_query_timeout;

    int          inc_check_timeout;
    int          inc_check_interval;		/* for local and IMAP */
    int          inc_second_check_interval;	/* for other */

    time_t       check_interval_for_noncurr;

    time_t       last_nextitem_forcechk;

    MAILSTREAM  *cur_uid_stream;
    imapuid_t    cur_uid;

    int		 deadlets;

    int		 quote_suppression_threshold;

    char        *display_charmap;	/* needs to be freed */
    char        *keyboard_charmap;	/* needs to be freed */
    void        *input_cs;

    char        *posting_charmap;	/* needs to be freed */

    CONV_TABLE  *conv_table;

    /*
     * Optional tools Pine Data Engine caller might provide
     */
    struct {
        char	*(*display_filter)(char *, STORE_S *, gf_io_t, FILTLIST_S *);
        char	*(*display_filter_trigger)(BODY *, char *, size_t);
    } tools;

    KEYWORD_S   *keywords;
    SPEC_COLOR_S *kw_colors;

    ACTION_S    *default_role;		/* pointer to one of regular roles */

    char	 last_error[500];
    INIT_ERR_S  *init_errs;

    PRINT_S	*print;

#ifdef SMIME
    SMIME_STUFF_S *smime;
#endif /* SMIME */

    struct variable *vars;
};


/*----------------------------------------------------------------------
    The few global variables we use in Pine Data Engine
  ----*/

extern struct pine *ps_global;

#define SIZEOF_20KBUF (20480)
extern char         tmp_20k_buf[];


/* exported protoypes */
struct pine  *new_pine_struct(void);
void          free_pine_struct(struct pine **);
void          free_pinerc_strings(struct pine **);
void	      free_vars(struct pine *);
void	      free_variable_values(struct variable *);
PINERC_S     *new_pinerc_s(char *);
void          free_pinerc_s(PINERC_S **);


#endif /* PITH_STATE_INCLUDED */
