/*
 * $Id: conftype.h 1155 2008-08-21 18:33:21Z hubert@u.washington.edu $
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

#ifndef PITH_CONFTYPE_INCLUDED
#define PITH_CONFTYPE_INCLUDED


#include "../pith/helptext.h"
#include "../pith/remtype.h"


typedef enum {Sapling, Seedling, Seasoned} FeatureLevel;


/*
 * The array is initialized in pith/conf.c so the order of that initialization
 * must correspond to the order of the values here.  The order is
 * significant in that it determines the order that the variables
 * are written into the pinerc file and the order they show up in in the
 * config screen.
 */
typedef	enum {    V_PERSONAL_NAME = 0
		, V_USER_ID
		, V_USER_DOMAIN
		, V_SMTP_SERVER
		, V_NNTP_SERVER
		, V_INBOX_PATH
		, V_ARCHIVED_FOLDERS
		, V_PRUNED_FOLDERS
		, V_DEFAULT_FCC
		, V_DEFAULT_SAVE_FOLDER
		, V_POSTPONED_FOLDER
		, V_READ_MESSAGE_FOLDER
		, V_FORM_FOLDER
		, V_TRASH_FOLDER
		, V_LITERAL_SIG
		, V_SIGNATURE_FILE
		, V_FEATURE_LIST
		, V_INIT_CMD_LIST
		, V_COMP_HDRS
		, V_CUSTOM_HDRS
		, V_VIEW_HEADERS
		, V_VIEW_MARGIN_LEFT
		, V_VIEW_MARGIN_RIGHT
		, V_QUOTE_SUPPRESSION
		, V_SAVED_MSG_NAME_RULE
		, V_FCC_RULE
		, V_SORT_KEY
		, V_AB_SORT_RULE
		, V_FLD_SORT_RULE
		, V_GOTO_DEFAULT_RULE
		, V_INCOMING_STARTUP
		, V_PRUNING_RULE
		, V_REOPEN_RULE
		, V_THREAD_DISP_STYLE
		, V_THREAD_INDEX_STYLE
		, V_THREAD_MORE_CHAR
		, V_THREAD_EXP_CHAR
		, V_THREAD_LASTREPLY_CHAR
#ifndef	_WINDOWS
		, V_CHAR_SET
		, V_OLD_CHAR_SET
		, V_KEY_CHAR_SET
#endif	/* ! _WINDOWS */
		, V_POST_CHAR_SET
		, V_UNK_CHAR_SET
		, V_EDITOR
		, V_SPELLER
		, V_FILLCOL
		, V_REPLY_STRING
		, V_REPLY_INTRO
		, V_QUOTE_REPLACE_STRING
		, V_WORDSEPS
		, V_EMPTY_HDR_MSG
		, V_IMAGE_VIEWER
		, V_USE_ONLY_DOMAIN_NAME
		, V_BUGS_FULLNAME
		, V_BUGS_ADDRESS
		, V_BUGS_EXTRAS
		, V_SUGGEST_FULLNAME
		, V_SUGGEST_ADDRESS
		, V_LOCAL_FULLNAME
		, V_LOCAL_ADDRESS
		, V_FORCED_ABOOK_ENTRY
		, V_KBLOCK_PASSWD_COUNT
		, V_DISPLAY_FILTERS
		, V_SEND_FILTER
		, V_ALT_ADDRS
		, V_KEYWORDS
		, V_KW_BRACES
		, V_OPENING_SEP
		, V_ABOOK_FORMATS
		, V_INDEX_FORMAT
		, V_OVERLAP
		, V_MARGIN
		, V_STATUS_MSG_DELAY
		, V_ACTIVE_MSG_INTERVAL
		, V_MAILCHECK
		, V_MAILCHECKNONCURR
		, V_MAILDROPCHECK
		, V_NNTPRANGE
		, V_NEWSRC_PATH
		, V_NEWS_ACTIVE_PATH
		, V_NEWS_SPOOL_DIR
		, V_UPLOAD_CMD
		, V_UPLOAD_CMD_PREFIX
		, V_DOWNLOAD_CMD
		, V_DOWNLOAD_CMD_PREFIX
		, V_MAILCAP_PATH
		, V_MIMETYPE_PATH
		, V_BROWSER
		, V_MAXREMSTREAM
		, V_PERMLOCKED
		, V_INCCHECKTIMEO
		, V_INCCHECKINTERVAL
		, V_INC2NDCHECKINTERVAL
		, V_INCCHECKLIST
		, V_DEADLETS
#if !defined(DOS) && !defined(OS2) && !defined(LEAVEOUTFIFO)
		, V_FIFOPATH
#endif
		, V_NMW_WIDTH
    /*
     * Starting here, the rest of the variables are hidden by default in config
     * screen. They are exposed with expose-hidden-config feature.
     */
		, V_INCOMING_FOLDERS
		, V_MAIL_DIRECTORY
		, V_FOLDER_SPEC
		, V_NEWS_SPEC
		, V_ADDRESSBOOK
		, V_GLOB_ADDRBOOK
		, V_STANDARD_PRINTER
		, V_LAST_TIME_PRUNE_QUESTION
		, V_LAST_VERS_USED
		, V_SENDMAIL_PATH
		, V_OPER_DIR
		, V_USERINPUTTIMEO
#ifdef DEBUGJOURNAL
		, V_DEBUGMEM		/* obsolete */
#endif
		, V_TCPOPENTIMEO
		, V_TCPREADWARNTIMEO
		, V_TCPWRITEWARNTIMEO
		, V_TCPQUERYTIMEO
		, V_RSHCMD
		, V_RSHPATH
		, V_RSHOPENTIMEO
		, V_SSHCMD
		, V_SSHPATH
		, V_SSHOPENTIMEO
		, V_NEW_VER_QUELL
		, V_DISABLE_DRIVERS
		, V_DISABLE_AUTHS
		, V_REMOTE_ABOOK_METADATA
		, V_REMOTE_ABOOK_HISTORY
		, V_REMOTE_ABOOK_VALIDITY
		, V_PRINTER
		, V_PERSONAL_PRINT_COMMAND
		, V_PERSONAL_PRINT_CATEGORY
		, V_PATTERNS		/* obsolete */
		, V_PAT_ROLES
		, V_PAT_FILTS
		, V_PAT_FILTS_OLD	/* obsolete */
		, V_PAT_SCORES
		, V_PAT_SCORES_OLD	/* obsolete */
		, V_PAT_INCOLS
		, V_PAT_OTHER
		, V_PAT_SRCH
		, V_ELM_STYLE_SAVE	/* obsolete */
		, V_HEADER_IN_REPLY	/* obsolete */
		, V_FEATURE_LEVEL	/* obsolete */
		, V_OLD_STYLE_REPLY	/* obsolete */
		, V_COMPOSE_MIME	/* obsolete */
		, V_SHOW_ALL_CHARACTERS	/* obsolete */
		, V_SAVE_BY_SENDER	/* obsolete */
#if defined(DOS) || defined(OS2)
		, V_FILE_DIR
		, V_FOLDER_EXTENSION
#endif
#ifndef	_WINDOWS
		, V_COLOR_STYLE
#endif
		, V_INDEX_COLOR_STYLE
		, V_TITLEBAR_COLOR_STYLE
		, V_NORM_FORE_COLOR
		, V_NORM_BACK_COLOR
		, V_REV_FORE_COLOR
		, V_REV_BACK_COLOR
		, V_TITLE_FORE_COLOR
		, V_TITLE_BACK_COLOR
		, V_TITLECLOSED_FORE_COLOR
		, V_TITLECLOSED_BACK_COLOR
		, V_STATUS_FORE_COLOR
		, V_STATUS_BACK_COLOR
		, V_KEYLABEL_FORE_COLOR
		, V_KEYLABEL_BACK_COLOR
		, V_KEYNAME_FORE_COLOR
		, V_KEYNAME_BACK_COLOR
		, V_SLCTBL_FORE_COLOR
		, V_SLCTBL_BACK_COLOR
		, V_METAMSG_FORE_COLOR
		, V_METAMSG_BACK_COLOR
		, V_QUOTE1_FORE_COLOR
		, V_QUOTE1_BACK_COLOR
		, V_QUOTE2_FORE_COLOR
		, V_QUOTE2_BACK_COLOR
		, V_QUOTE3_FORE_COLOR
		, V_QUOTE3_BACK_COLOR
		, V_INCUNSEEN_FORE_COLOR
		, V_INCUNSEEN_BACK_COLOR
		, V_SIGNATURE_FORE_COLOR
		, V_SIGNATURE_BACK_COLOR
		, V_PROMPT_FORE_COLOR
		, V_PROMPT_BACK_COLOR
		, V_HEADER_GENERAL_FORE_COLOR
		, V_HEADER_GENERAL_BACK_COLOR
		, V_IND_PLUS_FORE_COLOR
		, V_IND_PLUS_BACK_COLOR
		, V_IND_IMP_FORE_COLOR
		, V_IND_IMP_BACK_COLOR
		, V_IND_DEL_FORE_COLOR
		, V_IND_DEL_BACK_COLOR
		, V_IND_ANS_FORE_COLOR
		, V_IND_ANS_BACK_COLOR
		, V_IND_NEW_FORE_COLOR
		, V_IND_NEW_BACK_COLOR
		, V_IND_REC_FORE_COLOR
		, V_IND_REC_BACK_COLOR
		, V_IND_FWD_FORE_COLOR
		, V_IND_FWD_BACK_COLOR
		, V_IND_UNS_FORE_COLOR
		, V_IND_UNS_BACK_COLOR
		, V_IND_HIPRI_FORE_COLOR
		, V_IND_HIPRI_BACK_COLOR
		, V_IND_LOPRI_FORE_COLOR
		, V_IND_LOPRI_BACK_COLOR
		, V_IND_ARR_FORE_COLOR
		, V_IND_ARR_BACK_COLOR
		, V_IND_SUBJ_FORE_COLOR
		, V_IND_SUBJ_BACK_COLOR
		, V_IND_FROM_FORE_COLOR
		, V_IND_FROM_BACK_COLOR
		, V_IND_OP_FORE_COLOR
		, V_IND_OP_BACK_COLOR
		, V_VIEW_HDR_COLORS
		, V_KW_COLORS
#if defined(DOS) || defined(OS2)
#ifdef	_WINDOWS
		, V_FONT_NAME
		, V_FONT_SIZE
		, V_FONT_STYLE
		, V_FONT_CHAR_SET
		, V_PRINT_FONT_NAME
		, V_PRINT_FONT_SIZE
		, V_PRINT_FONT_STYLE
		, V_PRINT_FONT_CHAR_SET
		, V_WINDOW_POSITION
		, V_CURSOR_STYLE
#endif
#endif
#ifdef	SMIME
		, V_PUBLICCERT_DIR
		, V_PUBLICCERT_CONTAINER
		, V_PRIVATEKEY_DIR
		, V_PRIVATEKEY_CONTAINER
		, V_CACERT_DIR
		, V_CACERT_CONTAINER
#endif
#ifdef	ENABLE_LDAP
		, V_LDAP_SERVERS  /* should be last so make will work right */
#endif
		, V_RSS_NEWS
		, V_RSS_WEATHER
		, V_WP_INDEXHEIGHT
		, V_WP_INDEXLINES
		, V_WP_AGGSTATE
		, V_WP_STATE
		, V_WP_COLUMNS
		, V_DUMMY
} VariableIndex;

#define	V_LAST_VAR	(V_DUMMY - 1)


/*
 * The list of feature numbers (which bit goes with which feature).
 * The order of the features is not significant.
 */
typedef enum {
	F_OLD_GROWTH = 0,
	F_ENABLE_FULL_HDR,
	F_ENABLE_PIPE,
	F_ENABLE_TAB_COMPLETE,
	F_QUIT_WO_CONFIRM,
	F_ENABLE_JUMP,
	F_ENABLE_ALT_ED,
	F_ENABLE_BOUNCE,
	F_ENABLE_AGG_OPS,
	F_ENABLE_FLAG,
	F_CAN_SUSPEND,
	F_USE_FK,
	F_INCLUDE_HEADER,
	F_SIG_AT_BOTTOM,
	F_DEL_SKIPS_DEL,
	F_AUTO_EXPUNGE,
	F_FULL_AUTO_EXPUNGE,
	F_EXPUNGE_MANUALLY,
	F_AUTO_READ_MSGS,
	F_AUTO_FCC_ONLY,
	F_READ_IN_NEWSRC_ORDER,
	F_SELECT_WO_CONFIRM,
	F_SAVE_PARTIAL_WO_CONFIRM,
	F_NEXT_THRD_WO_CONFIRM,
	F_USE_CURRENT_DIR,
	F_STARTUP_STAYOPEN,
	F_USE_RESENTTO,
	F_SAVE_WONT_DELETE,
	F_SAVE_ADVANCES,
	F_UNSELECT_WONT_ADVANCE,
	F_FORCE_LOW_SPEED,
	F_FORCE_ARROW,
	F_PRUNE_USES_ISO,
	F_ALT_ED_NOW,
	F_SHOW_DELAY_CUE,
	F_CANCEL_CONFIRM,
	F_AUTO_OPEN_NEXT_UNREAD,
	F_DISABLE_INDEX_LOCALE_DATES,
	F_SELECTED_SHOWN_BOLD,
	F_QUOTE_ALL_FROMS,
	F_AUTO_INCLUDE_IN_REPLY,
	F_DISABLE_CONFIG_SCREEN,
	F_DISABLE_PASSWORD_CACHING,
	F_DISABLE_REGEX,
	F_DISABLE_PASSWORD_CMD,
	F_DISABLE_UPDATE_CMD,
	F_DISABLE_KBLOCK_CMD,
	F_DISABLE_SIGEDIT_CMD,
	F_DISABLE_ROLES_SETUP,
	F_DISABLE_ROLES_SIGEDIT,
	F_DISABLE_ROLES_TEMPLEDIT,
	F_DISABLE_PIPES_IN_SIGS,
	F_DISABLE_PIPES_IN_TEMPLATES,
	F_ATTACHMENTS_IN_REPLY,
	F_ENABLE_INCOMING,
	F_ENABLE_INCOMING_CHECKING,
	F_INCOMING_CHECKING_TOTAL,
	F_INCOMING_CHECKING_RECENT,
	F_NO_NEWS_VALIDATION,
	F_QUELL_EXTRA_POST_PROMPT,
	F_DISABLE_TAKE_LASTFIRST,
	F_DISABLE_TAKE_FULLNAMES,
	F_DISABLE_TERM_RESET_DISP,
	F_DISABLE_SENDER,
	F_ROT13_MESSAGE_ID,
	F_QUELL_LOCAL_LOOKUP,
	F_COMPOSE_TO_NEWSGRP,
	F_PRESERVE_START_STOP,
	F_COMPOSE_REJECTS_UNQUAL,
	F_FAKE_NEW_IN_NEWS,
	F_SUSPEND_SPAWNS,
	F_ENABLE_8BIT,
	F_COMPOSE_MAPS_DEL,
	F_ENABLE_8BIT_NNTP,
	F_ENABLE_MOUSE,
	F_SHOW_CURSOR,
	F_PASS_CONTROL_CHARS,
	F_PASS_C1_CONTROL_CHARS,
	F_SINGLE_FOLDER_LIST,
	F_VERTICAL_FOLDER_LIST,
	F_TAB_CHK_RECENT,
	F_AUTO_REPLY_TO,
	F_VERBOSE_POST,
	F_FCC_ON_BOUNCE,
	F_SEND_WO_CONFIRM,
	F_USE_SENDER_NOT_X,
	F_BLANK_KEYMENU,
	F_DISABLE_SAVE_INPUT_HISTORY,
	F_CUSTOM_PRINT,
	F_DEL_FROM_DOT,
	F_AUTO_ZOOM,
	F_AUTO_UNZOOM,
	F_PRINT_INDEX,
	F_ALLOW_TALK,
	F_AGG_PRINT_FF,
	F_ENABLE_DOT_FILES,
	F_ENABLE_DOT_FOLDERS,
	F_FIRST_SEND_FILTER_DFLT,
	F_ALWAYS_LAST_FLDR_DFLT,
	F_TAB_TO_NEW,
	F_MARK_FOR_CC,
	F_WARN_ABOUT_NO_SUBJECT,
	F_WARN_ABOUT_NO_FCC,
	F_WARN_ABOUT_NO_TO_OR_CC,
	F_QUELL_DEAD_LETTER,
	F_QUELL_BEEPS,
	F_QUELL_LOCK_FAILURE_MSGS,
	F_ENABLE_SPACE_AS_TAB,
	F_USE_BORING_SPINNER,
	F_ENABLE_TAB_DELETES,
	F_FLAG_SCREEN_KW_SHORTCUT,
	F_FLAG_SCREEN_DFLT,
	F_ENABLE_XTERM_NEWMAIL,
	F_ENABLE_NEWMAIL_SHORT_TEXT,
	F_EXPANDED_DISTLISTS,
	F_AGG_SEQ_COPY,
	F_DISABLE_ALARM,
	F_DISABLE_SETLOCALE_COLLATE,
	F_FROM_DELIM_IN_PRINT,
	F_BACKGROUND_POST,
	F_ALLOW_GOTO,
	F_DSN,
	F_ENABLE_SEARCH_AND_REPL,
	F_ARROW_NAV,
	F_RELAXED_ARROW_NAV,
	F_TCAP_WINS,
	F_ENABLE_SIGDASHES,
	F_ENABLE_STRIP_SIGDASHES,
	F_QUELL_PARTIAL_FETCH,
	F_QUELL_PERSONAL_NAME_PROMPT,
	F_QUELL_USER_ID_PROMPT,
	F_VIEW_SEL_ATTACH,
	F_VIEW_SEL_URL,
	F_VIEW_SEL_URL_HOST,
	F_SCAN_ADDR,
	F_FORCE_ARROWS,
	F_PREFER_PLAIN_TEXT,
	F_QUELL_CHARSET_WARNING,
	F_COPY_TO_TO_FROM,
	F_ENABLE_EDIT_REPLY_INDENT,
	F_ENABLE_PRYNT,
	F_ALLOW_CHANGING_FROM,
	F_ENABLE_SUB_LISTS,
	F_ENABLE_LESSTHAN_EXIT,
	F_ENABLE_FAST_RECENT,
	F_TAB_USES_UNSEEN,
	F_ENABLE_ROLE_TAKE,
	F_ENABLE_TAKE_EXPORT,
	F_QUELL_ATTACH_EXTRA_PROMPT,
	F_QUELL_ASTERISKS,
	F_QUELL_ATTACH_EXT_WARN,
	F_QUELL_FILTER_MSGS,
	F_QUELL_FILTER_DONE_MSG,
	F_SHOW_SORT,
	F_FIX_BROKEN_LIST,
	F_ENABLE_MULNEWSRCS,
	F_PREDICT_NNTP_SERVER,
	F_NEWS_CROSS_DELETE,
	F_NEWS_CATCHUP,
	F_QUELL_INTERNAL_MSG,
	F_QUELL_IMAP_ENV_CB,
	F_QUELL_NEWS_ENV_CB,
	F_SEPARATE_FLDR_AS_DIR,
	F_CMBND_ABOOK_DISP,
	F_CMBND_FOLDER_DISP,
	F_CMBND_SUBDIR_DISP,
	F_EXPANDED_ADDRBOOKS,
	F_EXPANDED_FOLDERS,
	F_QUELL_EMPTY_DIRS,
	F_SHOW_TEXTPLAIN_INT,
	F_ROLE_CONFIRM_DEFAULT,
	F_TAB_NO_CONFIRM,
	F_DATES_TO_LOCAL,
	F_RET_INBOX_NO_CONFIRM,
	F_CHECK_MAIL_ONQUIT,
	F_PREOPEN_STAYOPENS,
	F_EXPUNGE_STAYOPENS,
	F_EXPUNGE_INBOX,
	F_NO_FCC_ATTACH,
	F_DO_MAILCAP_PARAM_SUBST,
	F_PREFER_ALT_AUTH,
	F_SLCTBL_ITEM_NOBOLD,
	F_QUELL_PINGS_COMPOSING,
	F_QUELL_PINGS_COMPOSING_INBOX,
	F_QUELL_BEZERK_TIMEZONE,
	F_QUELL_CONTENT_ID,
	F_QUELL_MAILDOMAIN_WARNING,
	F_DISABLE_SHARED_NAMESPACES,
	F_HIDE_NNTP_PATH,
	F_MAILDROPS_PRESERVE_STATE,
	F_EXPOSE_HIDDEN_CONFIG,
	F_ALT_COMPOSE_MENU,
	F_ALT_ROLE_MENU,
	F_ALWAYS_SPELL_CHECK,
	F_QUELL_TIMEZONE,
	F_QUELL_USERAGENT,
	F_COLOR_LINE_IMPORTANT,
	F_SLASH_COLL_ENTIRE,
	F_ENABLE_FULL_HDR_AND_TEXT,
	F_QUELL_FULL_HDR_RESET,
	F_MARK_FCC_SEEN,
	F_MULNEWSRC_HOSTNAMES_AS_TYPED,
	F_STRIP_WS_BEFORE_SEND,
	F_QUELL_FLOWED_TEXT,
	F_COMPOSE_ALWAYS_DOWNGRADE,
	F_SORT_DEFAULT_FCC_ALPHA,
	F_SORT_DEFAULT_SAVE_ALPHA,
	F_QUOTE_REPLACE_NOFLOW,
	F_AUTO_UNSELECT,
	F_SEND_CONFIRM_ON_EXPAND,
	F_ENABLE_NEWMAIL_SOUND,
	F_RENDER_HTML_INTERNALLY,
	F_ENABLE_JUMP_CMD,
	F_FORWARD_AS_ATTACHMENT,
#ifndef	_WINDOWS
	F_USE_SYSTEM_TRANS,
#endif	/* ! _WINDOWS */
	F_QUELL_HOST_AFTER_URL,
	F_NNTP_SEARCH_USES_OVERVIEW,
	F_THREAD_SORTS_BY_ARRIVAL,
#ifdef	_WINDOWS
	F_ENABLE_TRAYICON,
	F_QUELL_SSL_LARGEBLOCKS,
	F_STORE_WINPOS_IN_CONFIG,
#endif
#ifdef	ENABLE_LDAP
	F_ADD_LDAP_TO_ABOOK,
#endif
#ifdef	SMIME
	F_DONT_DO_SMIME,
	F_SIGN_DEFAULT_ON,
	F_ENCRYPT_DEFAULT_ON,
	F_REMEMBER_SMIME_PASSPHRASE,
#ifdef	APPLEKEYCHAIN
	F_PUBLICCERTS_IN_KEYCHAIN,
#endif
#endif
	F_FEATURE_LIST_COUNT	/* Number of features */
} FeatureList;


typedef struct init_err {
    int   flags, min_time, max_time;
    char *message;
} INIT_ERR_S;


struct variable {
    char     *name;
    unsigned  is_obsolete:1;	/* variable read in, not written unless set */
    unsigned  is_used:1;	/* Some variables are disabled              */
    unsigned  been_written:1;
    unsigned  is_user:1;
    unsigned  is_global:1;
    unsigned  is_list:1;	/* flag indicating variable is a list       */
    unsigned  is_fixed:1;	/* sys mgr has fixed this variable          */
    unsigned  is_onlymain:1;	/* read and written from main_user_val	    */
    unsigned  is_outermost:1;	/* read and written from outermost pinerc   */
    unsigned  del_quotes:1;	/* remove double quotes                     */
    unsigned  is_changed_val:1; /* WP: use the changed val instead of cur val */
    char     *dname;		/* display name                             */
    char     *descrip;		/* description                              */
    union {
	char *p;		/* pointer to single string value           */
	char **l;		/* pointer to list of string values         */
    } current_val;
    union {
	char *p;		/* pointer to single string value           */
	char **l;		/* pointer to list of string values         */
    } main_user_val;		/* from pinerc                              */
    union {
	char *p;		/* pointer to single string value           */
	char **l;		/* pointer to list of string values         */
    } changed_val;		/* currently different from pinerc          */
    union {
	char *p;		/* pointer to single string value           */
	char **l;		/* pointer to list of string values         */
    } post_user_val;		/* from pinerc                              */
    union {
	char *p;		/* pointer to single string value           */
	char **l;		/* pointer to list of string values         */
    } global_val;		/* from default or pine.conf                */
    union {
	char *p;		/* pointer to single string value           */
	char **l;		/* pointer to list of string values         */
    } fixed_val;		/* fixed value assigned in pine.conf.fixed  */
    union {
	char *p;		/* pointer to single string value           */
	char **l;		/* pointer to list of string values         */
    } cmdline_val;		/* user typed as cmdline arg                */
};


typedef struct feature_entry {
    char       *name;
    char       *dname;		/* display name, same as name if NULL */
    int		id;
    HelpType	help;
    int		section;	/* for grouping in config screen */
    int         defval;		/* default value, 0 or 1 */
} FEATURE_S;


typedef struct pinerc_line {
  char *line;
  struct variable *var;
  unsigned int  is_var:1;
  unsigned int  is_quoted:1;
  unsigned int  obsolete_var:1;
} PINERC_LINE;


/*
 * Each pinerc has one of these.
 */
typedef struct pinerc_s {
    RemType           type;	/* type of pinerc, remote or local	*/
    char	     *name;	/* file name or remote name		*/
    REMDATA_S 	     *rd;	/* remote data structure		*/
    time_t	      pinerc_written;
    unsigned	      readonly:1;
    unsigned	      outstanding_pinerc_changes:1;
    unsigned	      quit_to_edit:1;
    PINERC_LINE	     *pinerc_lines;
} PINERC_S;


typedef enum {ParsePers, ParsePersPost, ParseGlobal, ParseFixed} ParsePinerc;


/* data stored in a line in the metadata file */
typedef struct remote_data_meta {
    char         *local_cache_file;
    imapuid_t     uidvalidity;
    imapuid_t     uidnext;
    imapuid_t     uid;
    unsigned long nmsgs;
    char          read_status;	/* 'R' for readonly, 'W' for readwrite */
    char         *date;
} REMDATA_META_S;


/*
 * Generic name/value pair structure
 */
typedef struct nameval {
    char *name;			/* the name that goes on the screen */
    char *shortname;		/* if non-NULL, name that goes in config file */
    int   value;		/* the internal bit number */
} NAMEVAL_S;


typedef enum {Main, Post, None} EditWhich;


#ifdef SMIME

typedef enum {Directory, Container, Keychain, Nada} SmimeHolderType;

typedef struct certlist {
    char            *name;
    void            *x509_cert;		/* this is type (X509 *) */
    struct certlist *next;
}CertList;

typedef struct smime_stuff {
    unsigned inited:1;
    unsigned do_sign:1;			/* set true if signing */
    unsigned do_encrypt:1;		/* set true if encrypting */
    unsigned need_passphrase:1;		/* set true if loading a key failed due to lack of passphrase */
    unsigned entered_passphrase:1;	/* user entered a passphrase */
    unsigned already_auto_asked:1;	/* asked for passphrase automatically, not again */
    volatile char passphrase[100];	/* storage for the entered passphrase */
    char    *passphrase_emailaddr;	/* pointer to allocated storage */

    /*
     * If we are using the Container type it is easiest if we
     * read in and maintain a list of certs and then write them
     * out all at once. For Directory type we just leave the data
     * in the individual files and read or write the individual
     * files when needed, so we don't have a list of all the certs.
     */
    SmimeHolderType publictype;
    char           *publicpath;
    char           *publiccontent;
    CertList       *publiccertlist;

    SmimeHolderType privatetype;
    char           *privatepath;
    char           *privatecontent;
    void           *personal_certs;	/* this is type (PERSONAL_CERT *) */

    SmimeHolderType catype;
    char           *capath;
    char           *cacontent;

} SMIME_STUFF_S;

#endif /* SMIME */


/* exported protoypes */


#endif /* PITH_CONFTYPE_INCLUDED */
