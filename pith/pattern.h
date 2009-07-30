/*
 * $Id: pattern.h 942 2008-03-04 18:21:33Z hubert@u.washington.edu $
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

#ifndef PITH_PATTERN_INCLUDED
#define PITH_PATTERN_INCLUDED


#include "../pith/msgno.h"
#include "../pith/sorttype.h"
#include "../pith/string.h"
#include "../pith/indxtype.h"


/*
 * This structure is used to contain strings which are matched against
 * header fields. The match is a simple substring match. The match is
 * an OR of all the patterns in the PATTERN_S list. That is,
 * substring1_matches OR substring2_matches OR substring3_matches.
 * If not is set in the _head_ of the PATTERN_S, it is a NOT of the
 * whole pattern, that is,
 * NOT (substring1_matches OR substring2_matches OR substring3_matches).
 * The not variable is not meaningful except in the head member of the
 * PATTERN_S list.
 */
typedef struct pattern_s {
    int               not;		/* NOT of whole pattern */
    char             *substring;
    struct pattern_s *next;
} PATTERN_S;

/*
 * List of these is a list of arbitrary freetext headers and patterns.
 * This may be part of a pattern group.
 * The isemptyval bit is to keep track of the difference between an arb
 * header with no value set and one with the empty value "" set. For the
 * other builtin headers this difference is kept track of by whether or
 * not the header is in the config file at all or not. Here we want to
 * be able to add a header to the config file without necessarily giving
 * it a value.
 */
typedef struct arbhdr_s {
    char            *field;
    PATTERN_S       *p;
    int              isemptyval;
    struct arbhdr_s *next;
} ARBHDR_S;

/*
 * A list of intervals of integers.
 */
typedef struct intvl_s {
    long            imin, imax;
    struct intvl_s *next;
} INTVL_S;

/*
 * A Pattern group gives characteristics of an envelope to match against. Any of
 * the characteristics (to, from, ...) which is non-null must match for the
 * whole thing to be considered a match. That is, it is an AND of all the
 * non-null members.
 */
typedef struct patgrp_s {
    char      *nick;
    char      *comment;		/* for user, not used for anything */
    PATTERN_S *to,
	      *from,
	      *sender,
	      *cc,
	      *recip,
	      *partic,
	      *news,
	      *subj,
	      *alltext,
	      *bodytext,
	      *keyword,
	      *charsets;
    STRLIST_S *charsets_list;	/* used for efficiency, computed from charset */
    ARBHDR_S  *arbhdr;		/* list of arbitrary hdrnames and patterns */
    int        fldr_type;	/* see FLDR_* below			*/
    PATTERN_S *folder;		/* folder if type FLDR_SPECIFIC		*/
    int        inabook;		/* see IAB_* below			*/
    PATTERN_S *abooks;
    int        do_score;
    INTVL_S   *score;
    int        do_age;
    INTVL_S   *age;		/* ages are in days			*/
    int        do_size;
    INTVL_S   *size;
    int        age_uses_sentdate; /* on or off				*/
    int        do_cat;
    char     **category_cmd;
    INTVL_S   *cat;
    long       cat_lim;		/* -1 no limit  0 only headers		*/
    int        bogus;		/* patgrp contains unknown stuff	*/
    int        stat_new,	/* msg status is New (Unseen)		*/
	       stat_rec,	/* msg status is Recent			*/
	       stat_del,	/* msg status is Deleted		*/
	       stat_imp,	/* msg is flagged Important		*/
	       stat_ans,	/* msg is flagged Answered		*/
	       stat_8bitsubj,	/* subject contains 8bit chars		*/
	       stat_bom,	/* this is first pine run of the month	*/
	       stat_boy;	/* this is first pine run of the year	*/
} PATGRP_S;

#define	FLDR_ANY	0
#define	FLDR_NEWS	1
#define	FLDR_EMAIL	2
#define	FLDR_SPECIFIC	3

#define	FLDR_DEFL	FLDR_EMAIL


#define	IAB_EITHER		0x0	/* don't care if in or not */

#define IAB_TYPE_MASK		0xf
#define	IAB_YES			0x1	/* addresses in any abook */
#define	IAB_NO			0x2	/*   "      not  " */
#define	IAB_SPEC_YES		0x3	/* addresses in specific abooks */
#define	IAB_SPEC_NO		0x4

/*
 * Warning about reply-to. We're using the c-client envelope reply-to which
 * means if there isn't a real reply-to it uses the From!
 */
#define IAB_ADDR_MASK		0xff0
#define	IAB_FROM		0x10	/* from address included in list */
#define	IAB_REPLYTO		0x20	/* reply-to address included in list */
#define	IAB_SENDER		0x40	/* sender address included in list */
#define	IAB_TO			0x80	/* to address included in list */
#define	IAB_CC			0x100	/* cc address included in list */

#define	IAB_DEFL		IAB_EITHER


#define	FILTER_STATE	0
#define	FILTER_KILL	1
#define	FILTER_FOLDER	2

/*
 * For the Status parts of a PATGRP_S. For example, stat_del is Deleted
 * status. User sets EITHER means they don't care, it always matches.
 * YES means it must be deleted to match. NO means it must not be deleted.
 */
#define	PAT_STAT_EITHER		0  /* we don't care which, yes or no	*/
#define	PAT_STAT_YES		1  /* yes, this status is true		*/
#define	PAT_STAT_NO		2  /* no, this status is not true	*/

/*
 * For the State setting part of a filter action
 */
#define	ACT_STAT_LEAVE		0  /* leave msg state alone		*/
#define	ACT_STAT_SET		1  /* set this part of msg state	*/
#define	ACT_STAT_CLEAR		2  /* clear this part of msg state	*/

typedef struct action_s {
    unsigned	 is_a_role:1;	/* this is a role action		*/
    unsigned	 is_a_incol:1;	/* this is an index color action	*/
    unsigned	 is_a_score:1;	/* this is a score setting action	*/
    unsigned	 is_a_filter:1;	/* this is a filter action		*/
    unsigned	 is_a_other:1;	/* this is a miscellaneous action	*/
    unsigned	 is_a_srch:1;	/* this is for Select cmd, no action	*/
    unsigned	 bogus:1;	/* action contains unknown stuff	*/
    unsigned	 been_here_before:1;  /* inheritance loop prevention	*/
	    /* --- These are for roles --- */
    ADDRESS	*from;		/* value to set for From		*/
    ADDRESS	*replyto;	/* value to set for Reply-To		*/
    char       **cstm;		/* custom headers			*/
    char       **smtp;		/* custom SMTP server for this role	*/
    char       **nntp;		/* custom NNTP server for this role	*/
    char	*fcc;		/* value to set for Fcc			*/
    char	*litsig;	/* value to set Literal Signature	*/
    char	*sig;		/* value to set for Sig File		*/
    char	*template;	/* value to set for Template		*/
    char	*nick;		/* value to set for Nickname		*/
    int		 repl_type;	/* see ROLE_REPL_* below		*/
    int		 forw_type;	/* see ROLE_FORW_* below		*/
    int		 comp_type;	/* see ROLE_COMP_* below		*/
    char	*inherit_nick;	/* pattern we inherit actions from	*/
	    /* --- This is for indexcoloring --- */
    COLOR_PAIR  *incol;		/* colors for index line		*/
	    /* --- This is for scoring --- */
    long         scoreval;
    HEADER_TOK_S *scorevalhdrtok;
	    /* --- These are for filtering --- */
    int	 	 kill;
    long	 state_setting_bits;
    PATTERN_S	*keyword_set;	/* set these keywords			*/
    PATTERN_S	*keyword_clr;	/* clear these keywords			*/
    PATTERN_S	*folder;	/* folders to recv. filtered mail	*/
    int          move_only_if_not_deleted;	/* on or off		*/
    int          non_terminating;		/* on or off		*/
	    /* --- These are for other --- */
	      /* sort order of folder */
    unsigned     sort_is_set:1;
    SortOrder	 sortorder;	/* sorting order			*/
    int          revsort;	/* whether or not to reverse sort	*/
	      /* Index format of folder */
    char	*index_format;
    unsigned     startup_rule;
} ACTION_S;

/* flags for first_pattern..., set_role_from_msg, and confirm_role() */
#define	PAT_CLOSED	0x00000000	/* closed                            */
#define	PAT_OPENED	0x00000001	/* opened successfully		     */
#define	PAT_OPEN_FAILED	0x00000002
#define	PAT_USE_CURRENT	0x00000010	/* use current_val to set up pattern */
#define	PAT_USE_CHANGED	0x00000020	/* use changed_val to set up pattern */
#define	PAT_USE_MAIN	0x00000040	/* use main_user_val		     */
#define	PAT_USE_POST	0x00000080	/* use post_user_val		     */
#define ROLE_COMPOSE	0x00000100	/* roles with compose value != NO   */
#define ROLE_REPLY	0x00000200	/* roles with reply value != NO	    */
#define ROLE_FORWARD	0x00000400	/* roles with forward value != NO   */
#define ROLE_INCOL	0x00000800	/* patterns with non-Normal colors  */
#define ROLE_SCORE	0x00001000	/* patterns with non-zero scorevals */
#define ROLE_DO_ROLES	0x00010000	/* role patterns		    */
#define ROLE_DO_INCOLS	0x00020000	/* index line color patterns	    */
#define ROLE_DO_SCORES	0x00040000	/* set score patterns		    */
#define	ROLE_DO_FILTER	0x00080000	/* filter patterns		    */
#define	ROLE_DO_OTHER	0x00100000	/* miscellaneous patterns	    */
#define ROLE_DO_SRCH	0x00200000	/* index line color patterns	    */
#define	ROLE_OLD_PAT	0x00400000	/* old patterns variable            */
#define	ROLE_OLD_FILT	0x00800000	/* old patterns-filters variable    */
#define	ROLE_OLD_SCORE	0x01000000	/* old patterns-scores variable     */
#define ROLE_CHANGES	0x02000000	/* start editing with changes
					   already registered */

#define PAT_OPEN_MASK	0x0000000f
#define PAT_USE_MASK	0x000000f0
#define ROLE_MASK	0x00ffff00

#define	ROLE_REPL_NO		0  /* never use for reply		 */
#define	ROLE_REPL_YES		1  /* use for reply with confirmation	 */
#define	ROLE_REPL_NOCONF	2  /* use for reply without confirmation */
#define	ROLE_FORW_NO		0  /* ... forward ...			 */
#define	ROLE_FORW_YES		1
#define	ROLE_FORW_NOCONF	2
#define	ROLE_COMP_NO		0  /* ... compose ...			 */
#define	ROLE_COMP_YES		1
#define	ROLE_COMP_NOCONF	2

#define	ROLE_REPL_DEFL		ROLE_REPL_YES	/* default reply value */
#define	ROLE_FORW_DEFL		ROLE_FORW_YES	/* default forward value */
#define	ROLE_COMP_DEFL		ROLE_COMP_NO	/* default compose value */
#define	ROLE_NOTAROLE_DEFL	ROLE_COMP_NO

#define INTVL_INF	  (2147483646L)
#define INTVL_UNDEF	  (INTVL_INF + 1L)
#define SCORE_UNDEF	  INTVL_UNDEF
#define SCORE_MIN	  (-100)
#define SCORE_MAX	  (100)
#define SCOREUSE_GET	  0x000
#define SCOREUSE_INVALID  0x001	/* will recalculate scores_in_use next time */
#define SCOREUSE_ROLES    0x010	/* scores are used for roles                */
#define SCOREUSE_INCOLS   0x020	/* scores are used for index line colors    */
#define SCOREUSE_FILTERS  0x040	/* scores are used for filters              */
#define SCOREUSE_OTHER    0x080	/* scores are used for miscellaneous stuff  */
#define SCOREUSE_INDEX    0x100	/* scores are used in index-format          */
#define SCOREUSE_STATEDEP 0x200	/* scores depend on message state           */

/*
 * A message is compared with a pattern group to see if it matches.
 * If it does match, then there are actions which are taken.
 */
typedef struct pat_s {
    PATGRP_S          *patgrp;
    ACTION_S          *action;
    struct pat_line_s *patline;		/* pat_line that goes with this pat */
    char              *raw;
    unsigned           inherit:1;
    struct pat_s      *next;
    struct pat_s      *prev;
} PAT_S;

typedef	enum {TypeNotSet = 0, Literal, File, Inherit} PAT_TYPE;

/*
 * There's one of these for each line in the pinerc variable.
 * Normal type=Literal patterns have a patline with both first and last
 * pointing to the pattern. Type File has one patline for the file and first
 * and last point to the first and last patterns in the file.
 * The patterns aren't linked into one giant list, the patlines are.
 * To traverse all the patterns you have to go through the patline list
 * and then for each patline go from first to last through the patterns.
 * That's what next_pattern and friends do.
 */
typedef struct pat_line_s {
    PAT_TYPE           type;
    PAT_S             *first;	/* 1st pattern in list belonging to this line */
    PAT_S             *last;
    char              *filename; /* If type File, the filename */
    char              *filepath;
    unsigned	       readonly:1;
    unsigned	       dirty:1;	/* needs to be written back to storage */
    struct pat_line_s *next;
    struct pat_line_s *prev;
} PAT_LINE_S;

typedef struct pat_handle {
    PAT_LINE_S *patlinehead;	/* list of in-core, parsed pat lines */
    unsigned    dirtypinerc:1;	/* needs to be written */
} PAT_HANDLE;

typedef struct pat_state {
    long        rflags;
    int         cur_rflag_num;
    PAT_LINE_S *patlinecurrent;
    PAT_S      *patcurrent;	/* current pat within patline */
} PAT_STATE;

#define PATTERN_MAGIC     "P#Pats"
#define PATTERN_FILE_VERS "01"


/*
 * This is a little dangerous. We're passing flags to match_pattern and
 * peeling some of them off for our own use while passing the rest on
 * to mail_search_full. So we need to define ours so they don't overlap
 * with the c-client flags that can be passed to mail_search_full.
 * We could formalize it with mrc.
 */
#define MP_IN_CCLIENT_CB	0x10000   /* we're in a c-client callback! */
#define MP_NOT			0x20000   /* use ! of patgrp for search    */


/* match_pattern_folder_specific flags */
#define FOR_PATTERN		0x01
#define FOR_FILTER		0x02
#define FOR_OPTIONSCREEN	0x04


extern PAT_HANDLE **cur_pat_h;


/* exported protoypes */
void	    role_process_filters(void);
int         add_to_pattern(PAT_S *, long);
char       *add_pat_escapes(char *);
char       *remove_pat_escapes(char *);
char	   *add_roletake_escapes(char *);
char	   *add_comma_escapes(char *);
void        set_pathandle(long);
void        close_every_pattern(void);
void        close_patterns(long);
int         nonempty_patterns(long, PAT_STATE *);
int         any_patterns(long, PAT_STATE *);
int         edit_pattern(PAT_S *, int, long);
int         add_pattern(PAT_S *, long);
int         delete_pattern(int, long);
int         shuffle_pattern(int, int, long);
PAT_LINE_S *parse_pat_file(char *);
INTVL_S    *parse_intvl(char *);
char       *stringform_of_intvl(INTVL_S *);
char       *hdrtok_to_stringform(HEADER_TOK_S *);
HEADER_TOK_S *stringform_to_hdrtok(char *);
char       *hdrtok_to_config(HEADER_TOK_S *);
HEADER_TOK_S *config_to_hdrtok(char *);
int         scores_are_used(int);
int         patgrp_depends_on_state(PATGRP_S *);
int         patgrp_depends_on_active_state(PATGRP_S *);
PATTERN_S  *parse_pattern(char *, char *, int);
PATTERN_S  *string_to_pattern(char *);
char	   *pattern_to_string(PATTERN_S *);
char       *pattern_to_config(PATTERN_S *);
PATTERN_S  *config_to_pattern(char *);
PATTERN_S  *editlist_to_pattern(char **);
char      **pattern_to_editlist(PATTERN_S *);
PATGRP_S   *nick_to_patgrp(char *, int);
PAT_S      *first_pattern(PAT_STATE *);
PAT_S      *last_pattern(PAT_STATE *);
PAT_S      *prev_pattern(PAT_STATE *);
PAT_S      *next_pattern(PAT_STATE *);
int         write_patterns(long);
void        convert_statebits_to_vals(long, int *, int *, int *, int *);
int         match_pattern(PATGRP_S *, MAILSTREAM *, SEARCHSET *,char *,
			  long (*)(MAILSTREAM *, long), long);
void        find_8bitsubj_in_messages(MAILSTREAM *, SEARCHSET *, int, int);
void        find_charsets_in_messages(MAILSTREAM *, SEARCHSET *, PATGRP_S *, int);
int         compare_strlists_for_match(STRLIST_S *, STRLIST_S *);
int	    match_pattern_folder(PATGRP_S *, MAILSTREAM *);
int	    match_pattern_folder_specific(PATTERN_S *, MAILSTREAM *, int);
SEARCHPGM  *match_pattern_srchpgm(PATGRP_S   *, MAILSTREAM *, SEARCHSET *);
void        calc_extra_hdrs(void);
char       *get_extra_hdrs(void);
void        free_extra_hdrs(void);
void        free_pat(PAT_S **);
void        free_pattern(PATTERN_S **);
void        free_action(ACTION_S **);
PAT_S      *copy_pat(PAT_S *);
PATGRP_S   *copy_patgrp(PATGRP_S *);
ACTION_S   *copy_action(ACTION_S *);
ACTION_S   *combine_inherited_role(ACTION_S *);
void        mail_expunge_prefilter(MAILSTREAM *, int);
void	    process_filter_patterns(MAILSTREAM *, MSGNO_S *, long);
void	    reprocess_filter_patterns(MAILSTREAM *, MSGNO_S *, int);
int         trivial_patgrp(PATGRP_S *);
void        free_patgrp(PATGRP_S **);
void        free_patline(PAT_LINE_S **patline);
int         some_filter_depends_on_active_state(void);
void	    delete_filtered_msgs(MAILSTREAM *);
void        free_intvl(INTVL_S **);
void        free_arbhdr(ARBHDR_S **);


#endif /* PITH_PATTERN_INCLUDED */
