/*
 * $Id: conf.h 1155 2008-08-21 18:33:21Z hubert@u.washington.edu $
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

#ifndef PITH_CONFIG_INCLUDED
#define PITH_CONFIG_INCLUDED


#include "../pith/conftype.h"
#include "../pith/remtype.h"
#include "../pith/state.h"
#include "../pith/pattern.h"
#include "../pith/color.h"


#ifndef DF_REMOTE_ABOOK_VALIDITY
#define DF_REMOTE_ABOOK_VALIDITY "5"
#endif
#ifndef DF_GOTO_DEFAULT_RULE
#define DF_GOTO_DEFAULT_RULE	"inbox-or-folder-in-recent-collection"
#endif
#ifndef DF_INCOMING_STARTUP
#define DF_INCOMING_STARTUP	"first-unseen"
#endif
#ifndef DF_PRUNING_RULE
#define DF_PRUNING_RULE		"ask-ask"
#endif
#ifndef DF_REOPEN_RULE
#define DF_REOPEN_RULE		"ask-no-n"
#endif
#ifndef DF_THREAD_DISP_STYLE
#define DF_THREAD_DISP_STYLE	"struct"
#endif
#ifndef DF_THREAD_INDEX_STYLE
#define DF_THREAD_INDEX_STYLE	"exp"
#endif
#ifndef DF_THREAD_MORE_CHAR
#define DF_THREAD_MORE_CHAR	">"
#endif
#ifndef DF_THREAD_EXP_CHAR
#define DF_THREAD_EXP_CHAR	"."
#endif
#ifndef DF_THREAD_LASTREPLY_CHAR
#define DF_THREAD_LASTREPLY_CHAR "\\"
#endif
#ifndef DF_MAILDROPCHECK
#define DF_MAILDROPCHECK 	"60"
#endif
#ifndef DF_MAXREMSTREAM
#define DF_MAXREMSTREAM 	"3"
#endif
#ifndef DF_VIEW_MARGIN_RIGHT
#define DF_VIEW_MARGIN_RIGHT 	"4"
#endif
#ifndef DF_QUOTE_SUPPRESSION
#define DF_QUOTE_SUPPRESSION 	"0"
#endif
#ifndef DF_DEADLETS
#define DF_DEADLETS 		"1"
#endif
#ifndef DF_NMW_WIDTH
#define DF_NMW_WIDTH 		"80"
#endif


#define VAR_PERSONAL_NAME	     vars[V_PERSONAL_NAME].current_val.p
#define FIX_PERSONAL_NAME	     vars[V_PERSONAL_NAME].fixed_val.p
#define COM_PERSONAL_NAME	     vars[V_PERSONAL_NAME].cmdline_val.p
#define VAR_USER_ID		     vars[V_USER_ID].current_val.p
#define COM_USER_ID		     vars[V_USER_ID].cmdline_val.p
#define VAR_USER_DOMAIN		     vars[V_USER_DOMAIN].current_val.p
#define VAR_SMTP_SERVER		     vars[V_SMTP_SERVER].current_val.l
#define FIX_SMTP_SERVER		     vars[V_SMTP_SERVER].fixed_val.l
#define COM_SMTP_SERVER		     vars[V_SMTP_SERVER].cmdline_val.l
#define GLO_SMTP_SERVER		     vars[V_SMTP_SERVER].global_val.l
#define VAR_INBOX_PATH		     vars[V_INBOX_PATH].current_val.p
#define GLO_INBOX_PATH		     vars[V_INBOX_PATH].global_val.p
#define VAR_INCOMING_FOLDERS	     vars[V_INCOMING_FOLDERS].current_val.l
#define VAR_FOLDER_SPEC		     vars[V_FOLDER_SPEC].current_val.l
#define GLO_FOLDER_SPEC		     vars[V_FOLDER_SPEC].global_val.l
#define VAR_NEWS_SPEC		     vars[V_NEWS_SPEC].current_val.l
#define VAR_ARCHIVED_FOLDERS	     vars[V_ARCHIVED_FOLDERS].current_val.l
#define VAR_PRUNED_FOLDERS	     vars[V_PRUNED_FOLDERS].current_val.l
#define GLO_PRUNED_FOLDERS	     vars[V_PRUNED_FOLDERS].global_val.l
#define VAR_DEFAULT_FCC		     vars[V_DEFAULT_FCC].current_val.p
#define GLO_DEFAULT_FCC		     vars[V_DEFAULT_FCC].global_val.p
#define VAR_DEFAULT_SAVE_FOLDER	     vars[V_DEFAULT_SAVE_FOLDER].current_val.p
#define GLO_DEFAULT_SAVE_FOLDER	     vars[V_DEFAULT_SAVE_FOLDER].global_val.p
#define VAR_POSTPONED_FOLDER	     vars[V_POSTPONED_FOLDER].current_val.p
#define GLO_POSTPONED_FOLDER	     vars[V_POSTPONED_FOLDER].global_val.p
#define VAR_MAIL_DIRECTORY	     vars[V_MAIL_DIRECTORY].current_val.p
#define GLO_MAIL_DIRECTORY	     vars[V_MAIL_DIRECTORY].global_val.p
#define VAR_READ_MESSAGE_FOLDER	     vars[V_READ_MESSAGE_FOLDER].current_val.p
#define GLO_READ_MESSAGE_FOLDER	     vars[V_READ_MESSAGE_FOLDER].global_val.p
#define VAR_FORM_FOLDER		     vars[V_FORM_FOLDER].current_val.p
#define GLO_FORM_FOLDER		     vars[V_FORM_FOLDER].global_val.p
#define VAR_TRASH_FOLDER	     vars[V_TRASH_FOLDER].current_val.p
#define GLO_TRASH_FOLDER	     vars[V_TRASH_FOLDER].global_val.p
#define VAR_SIGNATURE_FILE	     vars[V_SIGNATURE_FILE].current_val.p
#define GLO_SIGNATURE_FILE	     vars[V_SIGNATURE_FILE].global_val.p
#define VAR_LITERAL_SIG		     vars[V_LITERAL_SIG].current_val.p
#define VAR_GLOB_ADDRBOOK	     vars[V_GLOB_ADDRBOOK].current_val.l
#define VAR_ADDRESSBOOK		     vars[V_ADDRESSBOOK].current_val.l
#define GLO_ADDRESSBOOK		     vars[V_ADDRESSBOOK].global_val.l
#define FIX_ADDRESSBOOK		     vars[V_ADDRESSBOOK].fixed_val.l
#define VAR_FEATURE_LIST	     vars[V_FEATURE_LIST].current_val.l
#define VAR_INIT_CMD_LIST	     vars[V_INIT_CMD_LIST].current_val.l
#define GLO_INIT_CMD_LIST	     vars[V_INIT_CMD_LIST].global_val.l
#define COM_INIT_CMD_LIST	     vars[V_INIT_CMD_LIST].cmdline_val.l
#define VAR_COMP_HDRS		     vars[V_COMP_HDRS].current_val.l
#define GLO_COMP_HDRS		     vars[V_COMP_HDRS].global_val.l
#define VAR_CUSTOM_HDRS		     vars[V_CUSTOM_HDRS].current_val.l
#define VAR_VIEW_HEADERS	     vars[V_VIEW_HEADERS].current_val.l
#define VAR_VIEW_MARGIN_LEFT	     vars[V_VIEW_MARGIN_LEFT].current_val.p
#define GLO_VIEW_MARGIN_LEFT	     vars[V_VIEW_MARGIN_LEFT].global_val.p
#define VAR_VIEW_MARGIN_RIGHT	     vars[V_VIEW_MARGIN_RIGHT].current_val.p
#define GLO_VIEW_MARGIN_RIGHT	     vars[V_VIEW_MARGIN_RIGHT].global_val.p
#define VAR_QUOTE_SUPPRESSION	     vars[V_QUOTE_SUPPRESSION].current_val.p
#define GLO_QUOTE_SUPPRESSION	     vars[V_QUOTE_SUPPRESSION].global_val.p
#ifndef	_WINDOWS
#define VAR_COLOR_STYLE		     vars[V_COLOR_STYLE].current_val.p
#define GLO_COLOR_STYLE		     vars[V_COLOR_STYLE].global_val.p
#endif
#define VAR_INDEX_COLOR_STYLE	     vars[V_INDEX_COLOR_STYLE].current_val.p
#define GLO_INDEX_COLOR_STYLE	     vars[V_INDEX_COLOR_STYLE].global_val.p
#define VAR_TITLEBAR_COLOR_STYLE     vars[V_TITLEBAR_COLOR_STYLE].current_val.p
#define GLO_TITLEBAR_COLOR_STYLE     vars[V_TITLEBAR_COLOR_STYLE].global_val.p
#define VAR_SAVED_MSG_NAME_RULE	     vars[V_SAVED_MSG_NAME_RULE].current_val.p
#define GLO_SAVED_MSG_NAME_RULE	     vars[V_SAVED_MSG_NAME_RULE].global_val.p
#define VAR_FCC_RULE		     vars[V_FCC_RULE].current_val.p
#define GLO_FCC_RULE		     vars[V_FCC_RULE].global_val.p
#define VAR_SORT_KEY		     vars[V_SORT_KEY].current_val.p
#define GLO_SORT_KEY		     vars[V_SORT_KEY].global_val.p
#define COM_SORT_KEY		     vars[V_SORT_KEY].cmdline_val.p
#define VAR_AB_SORT_RULE	     vars[V_AB_SORT_RULE].current_val.p
#define GLO_AB_SORT_RULE	     vars[V_AB_SORT_RULE].global_val.p
#define VAR_FLD_SORT_RULE	     vars[V_FLD_SORT_RULE].current_val.p
#define GLO_FLD_SORT_RULE	     vars[V_FLD_SORT_RULE].global_val.p
#ifndef	_WINDOWS
#define VAR_CHAR_SET		     vars[V_CHAR_SET].current_val.p
#define GLO_CHAR_SET		     vars[V_CHAR_SET].global_val.p
#define VAR_OLD_CHAR_SET	     vars[V_OLD_CHAR_SET].current_val.p
#define VAR_KEY_CHAR_SET	     vars[V_KEY_CHAR_SET].current_val.p
#endif	/* ! _WINDOWS */
#define VAR_POST_CHAR_SET	     vars[V_POST_CHAR_SET].current_val.p
#define GLO_POST_CHAR_SET	     vars[V_POST_CHAR_SET].global_val.p
#define VAR_UNK_CHAR_SET	     vars[V_UNK_CHAR_SET].current_val.p
#define VAR_EDITOR		     vars[V_EDITOR].current_val.l
#define GLO_EDITOR		     vars[V_EDITOR].global_val.l
#define VAR_SPELLER		     vars[V_SPELLER].current_val.p
#define GLO_SPELLER		     vars[V_SPELLER].global_val.p
#define VAR_FILLCOL		     vars[V_FILLCOL].current_val.p
#define GLO_FILLCOL		     vars[V_FILLCOL].global_val.p
#define VAR_DEADLETS		     vars[V_DEADLETS].current_val.p
#define GLO_DEADLETS		     vars[V_DEADLETS].global_val.p
#define VAR_REPLY_STRING	     vars[V_REPLY_STRING].current_val.p
#define GLO_REPLY_STRING	     vars[V_REPLY_STRING].global_val.p
#define VAR_WORDSEPS		     vars[V_WORDSEPS].current_val.l
#define VAR_QUOTE_REPLACE_STRING     vars[V_QUOTE_REPLACE_STRING].current_val.p
#define GLO_QUOTE_REPLACE_STRING     vars[V_QUOTE_REPLACE_STRING].global_val.p
#define VAR_REPLY_INTRO		     vars[V_REPLY_INTRO].current_val.p
#define GLO_REPLY_INTRO		     vars[V_REPLY_INTRO].global_val.p
#define VAR_EMPTY_HDR_MSG	     vars[V_EMPTY_HDR_MSG].current_val.p
#define GLO_EMPTY_HDR_MSG	     vars[V_EMPTY_HDR_MSG].global_val.p
#define VAR_IMAGE_VIEWER	     vars[V_IMAGE_VIEWER].current_val.p
#define GLO_IMAGE_VIEWER	     vars[V_IMAGE_VIEWER].global_val.p
#define VAR_USE_ONLY_DOMAIN_NAME     vars[V_USE_ONLY_DOMAIN_NAME].current_val.p
#define GLO_USE_ONLY_DOMAIN_NAME     vars[V_USE_ONLY_DOMAIN_NAME].global_val.p
#define VAR_PRINTER		     vars[V_PRINTER].current_val.p
#define GLO_PRINTER		     vars[V_PRINTER].global_val.p
#define VAR_PERSONAL_PRINT_COMMAND   vars[V_PERSONAL_PRINT_COMMAND].current_val.l
#define GLO_PERSONAL_PRINT_COMMAND   vars[V_PERSONAL_PRINT_COMMAND].global_val.l
#define VAR_PERSONAL_PRINT_CATEGORY  vars[V_PERSONAL_PRINT_CATEGORY].current_val.p
#define GLO_PERSONAL_PRINT_CATEGORY  vars[V_PERSONAL_PRINT_CATEGORY].global_val.p
#define VAR_STANDARD_PRINTER	     vars[V_STANDARD_PRINTER].current_val.l
#define GLO_STANDARD_PRINTER	     vars[V_STANDARD_PRINTER].global_val.l
#define FIX_STANDARD_PRINTER	     vars[V_STANDARD_PRINTER].fixed_val.l
#define VAR_LAST_TIME_PRUNE_QUESTION vars[V_LAST_TIME_PRUNE_QUESTION].current_val.p
#define VAR_LAST_VERS_USED	     vars[V_LAST_VERS_USED].current_val.p
#define VAR_BUGS_FULLNAME	     vars[V_BUGS_FULLNAME].current_val.p
#define GLO_BUGS_FULLNAME	     vars[V_BUGS_FULLNAME].global_val.p
#define VAR_BUGS_ADDRESS	     vars[V_BUGS_ADDRESS].current_val.p
#define GLO_BUGS_ADDRESS	     vars[V_BUGS_ADDRESS].global_val.p
#define VAR_BUGS_EXTRAS		     vars[V_BUGS_EXTRAS].current_val.p
#define GLO_BUGS_EXTRAS		     vars[V_BUGS_EXTRAS].global_val.p
#define VAR_SUGGEST_FULLNAME	     vars[V_SUGGEST_FULLNAME].current_val.p
#define GLO_SUGGEST_FULLNAME	     vars[V_SUGGEST_FULLNAME].global_val.p
#define VAR_SUGGEST_ADDRESS	     vars[V_SUGGEST_ADDRESS].current_val.p
#define GLO_SUGGEST_ADDRESS	     vars[V_SUGGEST_ADDRESS].global_val.p
#define VAR_LOCAL_FULLNAME	     vars[V_LOCAL_FULLNAME].current_val.p
#define GLO_LOCAL_FULLNAME	     vars[V_LOCAL_FULLNAME].global_val.p
#define VAR_LOCAL_ADDRESS	     vars[V_LOCAL_ADDRESS].current_val.p
#define GLO_LOCAL_ADDRESS	     vars[V_LOCAL_ADDRESS].global_val.p
#define VAR_FORCED_ABOOK_ENTRY	     vars[V_FORCED_ABOOK_ENTRY].current_val.l
#define VAR_KBLOCK_PASSWD_COUNT	     vars[V_KBLOCK_PASSWD_COUNT].current_val.p
#define GLO_KBLOCK_PASSWD_COUNT	     vars[V_KBLOCK_PASSWD_COUNT].global_val.p
#define VAR_STATUS_MSG_DELAY	     vars[V_STATUS_MSG_DELAY].current_val.p
#define GLO_STATUS_MSG_DELAY	     vars[V_STATUS_MSG_DELAY].global_val.p
#define	VAR_ACTIVE_MSG_INTERVAL	     vars[V_ACTIVE_MSG_INTERVAL].current_val.p
#define	GLO_ACTIVE_MSG_INTERVAL	     vars[V_ACTIVE_MSG_INTERVAL].global_val.p
#define GLO_SENDMAIL_PATH	     vars[V_SENDMAIL_PATH].global_val.p
#define FIX_SENDMAIL_PATH	     vars[V_SENDMAIL_PATH].fixed_val.p
#define COM_SENDMAIL_PATH	     vars[V_SENDMAIL_PATH].cmdline_val.p
#define VAR_OPER_DIR		     vars[V_OPER_DIR].current_val.p
#define GLO_OPER_DIR		     vars[V_OPER_DIR].global_val.p
#define FIX_OPER_DIR		     vars[V_OPER_DIR].fixed_val.p
#define COM_OPER_DIR		     vars[V_OPER_DIR].cmdline_val.p
#define VAR_DISPLAY_FILTERS	     vars[V_DISPLAY_FILTERS].current_val.l
#define VAR_SEND_FILTER		     vars[V_SEND_FILTER].current_val.l
#define GLO_SEND_FILTER		     vars[V_SEND_FILTER].global_val.l
#define VAR_ALT_ADDRS		     vars[V_ALT_ADDRS].current_val.l
#define VAR_KEYWORDS		     vars[V_KEYWORDS].current_val.l
#define VAR_KW_COLORS		     vars[V_KW_COLORS].current_val.l
#define VAR_KW_BRACES		     vars[V_KW_BRACES].current_val.p
#define GLO_KW_BRACES		     vars[V_KW_BRACES].global_val.p
#define VAR_OPENING_SEP		     vars[V_OPENING_SEP].current_val.p
#define GLO_OPENING_SEP		     vars[V_OPENING_SEP].global_val.p
#define VAR_ABOOK_FORMATS	     vars[V_ABOOK_FORMATS].current_val.l
#define VAR_INDEX_FORMAT	     vars[V_INDEX_FORMAT].current_val.p
#define VAR_OVERLAP		     vars[V_OVERLAP].current_val.p
#define GLO_OVERLAP		     vars[V_OVERLAP].global_val.p
#define VAR_MAXREMSTREAM	     vars[V_MAXREMSTREAM].current_val.p
#define GLO_MAXREMSTREAM	     vars[V_MAXREMSTREAM].global_val.p
#define VAR_PERMLOCKED		     vars[V_PERMLOCKED].current_val.l
#define VAR_MARGIN		     vars[V_MARGIN].current_val.p
#define GLO_MARGIN		     vars[V_MARGIN].global_val.p
#define VAR_MAILCHECK		     vars[V_MAILCHECK].current_val.p
#define GLO_MAILCHECK		     vars[V_MAILCHECK].global_val.p
#define VAR_MAILCHECKNONCURR	     vars[V_MAILCHECKNONCURR].current_val.p
#define GLO_MAILCHECKNONCURR	     vars[V_MAILCHECKNONCURR].global_val.p
#define VAR_MAILDROPCHECK	     vars[V_MAILDROPCHECK].current_val.p
#define GLO_MAILDROPCHECK	     vars[V_MAILDROPCHECK].global_val.p
#define VAR_NNTPRANGE     	     vars[V_NNTPRANGE].current_val.p
#define GLO_NNTPRANGE     	     vars[V_NNTPRANGE].global_val.p
#define VAR_NEWSRC_PATH		     vars[V_NEWSRC_PATH].current_val.p
#define GLO_NEWSRC_PATH		     vars[V_NEWSRC_PATH].global_val.p
#define VAR_NEWS_ACTIVE_PATH	     vars[V_NEWS_ACTIVE_PATH].current_val.p
#define GLO_NEWS_ACTIVE_PATH	     vars[V_NEWS_ACTIVE_PATH].global_val.p
#define VAR_NEWS_SPOOL_DIR	     vars[V_NEWS_SPOOL_DIR].current_val.p
#define GLO_NEWS_SPOOL_DIR	     vars[V_NEWS_SPOOL_DIR].global_val.p
#define VAR_DISABLE_DRIVERS	     vars[V_DISABLE_DRIVERS].current_val.l
#define VAR_DISABLE_AUTHS	     vars[V_DISABLE_AUTHS].current_val.l
#define VAR_REMOTE_ABOOK_METADATA    vars[V_REMOTE_ABOOK_METADATA].current_val.p
#define VAR_REMOTE_ABOOK_HISTORY     vars[V_REMOTE_ABOOK_HISTORY].current_val.p
#define GLO_REMOTE_ABOOK_HISTORY     vars[V_REMOTE_ABOOK_HISTORY].global_val.p
#define VAR_REMOTE_ABOOK_VALIDITY    vars[V_REMOTE_ABOOK_VALIDITY].current_val.p
#define GLO_REMOTE_ABOOK_VALIDITY    vars[V_REMOTE_ABOOK_VALIDITY].global_val.p
  /* Elm style save is obsolete in Pine 3.81 (see saved msg name rule) */
#define VAR_ELM_STYLE_SAVE           vars[V_ELM_STYLE_SAVE].current_val.p
#define GLO_ELM_STYLE_SAVE           vars[V_ELM_STYLE_SAVE].global_val.p
  /* Header in reply is obsolete in Pine 3.83 (see feature list) */
#define VAR_HEADER_IN_REPLY          vars[V_HEADER_IN_REPLY].current_val.p
#define GLO_HEADER_IN_REPLY          vars[V_HEADER_IN_REPLY].global_val.p
  /* Feature level is obsolete in Pine 3.83 (see feature list) */
#define VAR_FEATURE_LEVEL            vars[V_FEATURE_LEVEL].current_val.p
#define GLO_FEATURE_LEVEL            vars[V_FEATURE_LEVEL].global_val.p
  /* Old style reply is obsolete in Pine 3.83 (see feature list) */
#define VAR_OLD_STYLE_REPLY          vars[V_OLD_STYLE_REPLY].current_val.p
#define GLO_OLD_STYLE_REPLY          vars[V_OLD_STYLE_REPLY].global_val.p
  /* Save by sender is obsolete in Pine 3.83 (see saved msg name rule) */
#define VAR_SAVE_BY_SENDER           vars[V_SAVE_BY_SENDER].current_val.p
#define GLO_SAVE_BY_SENDER           vars[V_SAVE_BY_SENDER].global_val.p
#define VAR_NNTP_SERVER              vars[V_NNTP_SERVER].current_val.l
#define FIX_NNTP_SERVER              vars[V_NNTP_SERVER].fixed_val.l
#ifdef	ENABLE_LDAP
#define VAR_LDAP_SERVERS             vars[V_LDAP_SERVERS].current_val.l
#endif
#define VAR_RSS_NEWS                 vars[V_RSS_NEWS].current_val.p
#define VAR_RSS_WEATHER              vars[V_RSS_WEATHER].current_val.p
#define VAR_WP_INDEXHEIGHT           vars[V_WP_INDEXHEIGHT].current_val.p
#define VAR_WP_INDEXLINES            vars[V_WP_INDEXLINES].current_val.p
#define GLO_WP_INDEXHEIGHT           vars[V_WP_INDEXHEIGHT].global_val.p
#define VAR_WP_AGGSTATE              vars[V_WP_AGGSTATE].current_val.p
#define GLO_WP_AGGSTATE              vars[V_WP_AGGSTATE].global_val.p
#define VAR_WP_STATE                 vars[V_WP_STATE].current_val.p
#define GLO_WP_STATE                 vars[V_WP_STATE].global_val.p
#define VAR_WP_COLUMNS               vars[V_WP_COLUMNS].current_val.p
#define VAR_UPLOAD_CMD		     vars[V_UPLOAD_CMD].current_val.p
#define VAR_UPLOAD_CMD_PREFIX	     vars[V_UPLOAD_CMD_PREFIX].current_val.p
#define VAR_DOWNLOAD_CMD	     vars[V_DOWNLOAD_CMD].current_val.p
#define VAR_DOWNLOAD_CMD_PREFIX	     vars[V_DOWNLOAD_CMD_PREFIX].current_val.p
#define VAR_GOTO_DEFAULT_RULE	     vars[V_GOTO_DEFAULT_RULE].current_val.p
#define GLO_GOTO_DEFAULT_RULE	     vars[V_GOTO_DEFAULT_RULE].global_val.p
#define VAR_MAILCAP_PATH	     vars[V_MAILCAP_PATH].current_val.p
#define VAR_MIMETYPE_PATH	     vars[V_MIMETYPE_PATH].current_val.p
#define VAR_FIFOPATH		     vars[V_FIFOPATH].current_val.p
#define VAR_NMW_WIDTH		     vars[V_NMW_WIDTH].current_val.p
#define GLO_NMW_WIDTH		     vars[V_NMW_WIDTH].global_val.p
#define VAR_USERINPUTTIMEO     	     vars[V_USERINPUTTIMEO].current_val.p
#define GLO_USERINPUTTIMEO     	     vars[V_USERINPUTTIMEO].global_val.p
#define VAR_TCPOPENTIMEO	     vars[V_TCPOPENTIMEO].current_val.p
#define VAR_TCPREADWARNTIMEO	     vars[V_TCPREADWARNTIMEO].current_val.p
#define VAR_TCPWRITEWARNTIMEO	     vars[V_TCPWRITEWARNTIMEO].current_val.p
#define VAR_TCPQUERYTIMEO	     vars[V_TCPQUERYTIMEO].current_val.p
#define VAR_RSHOPENTIMEO	     vars[V_RSHOPENTIMEO].current_val.p
#define VAR_RSHPATH		     vars[V_RSHPATH].current_val.p
#define VAR_RSHCMD		     vars[V_RSHCMD].current_val.p
#define VAR_SSHOPENTIMEO	     vars[V_SSHOPENTIMEO].current_val.p
#define VAR_INCCHECKTIMEO     	     vars[V_INCCHECKTIMEO].current_val.p
#define GLO_INCCHECKTIMEO     	     vars[V_INCCHECKTIMEO].global_val.p
#define VAR_INCCHECKINTERVAL   	     vars[V_INCCHECKINTERVAL].current_val.p
#define GLO_INCCHECKINTERVAL   	     vars[V_INCCHECKINTERVAL].global_val.p
#define VAR_INC2NDCHECKINTERVAL      vars[V_INC2NDCHECKINTERVAL].current_val.p
#define GLO_INC2NDCHECKINTERVAL      vars[V_INC2NDCHECKINTERVAL].global_val.p
#define VAR_INCCHECKLIST     	     vars[V_INCCHECKLIST].current_val.l
#define VAR_SSHPATH		     vars[V_SSHPATH].current_val.p
#define GLO_SSHPATH		     vars[V_SSHPATH].global_val.p
#define VAR_SSHCMD		     vars[V_SSHCMD].current_val.p
#define GLO_SSHCMD		     vars[V_SSHCMD].global_val.p
#define VAR_NEW_VER_QUELL	     vars[V_NEW_VER_QUELL].current_val.p
#define VAR_BROWSER		     vars[V_BROWSER].current_val.l
#define VAR_INCOMING_STARTUP	     vars[V_INCOMING_STARTUP].current_val.p
#define GLO_INCOMING_STARTUP	     vars[V_INCOMING_STARTUP].global_val.p
#define VAR_PRUNING_RULE	     vars[V_PRUNING_RULE].current_val.p
#define GLO_PRUNING_RULE	     vars[V_PRUNING_RULE].global_val.p
#define VAR_REOPEN_RULE		     vars[V_REOPEN_RULE].current_val.p
#define GLO_REOPEN_RULE		     vars[V_REOPEN_RULE].global_val.p
#define VAR_THREAD_DISP_STYLE	     vars[V_THREAD_DISP_STYLE].current_val.p
#define GLO_THREAD_DISP_STYLE	     vars[V_THREAD_DISP_STYLE].global_val.p
#define VAR_THREAD_INDEX_STYLE	     vars[V_THREAD_INDEX_STYLE].current_val.p
#define GLO_THREAD_INDEX_STYLE	     vars[V_THREAD_INDEX_STYLE].global_val.p
#define VAR_THREAD_MORE_CHAR	     vars[V_THREAD_MORE_CHAR].current_val.p
#define GLO_THREAD_MORE_CHAR	     vars[V_THREAD_MORE_CHAR].global_val.p
#define VAR_THREAD_EXP_CHAR	     vars[V_THREAD_EXP_CHAR].current_val.p
#define GLO_THREAD_EXP_CHAR	     vars[V_THREAD_EXP_CHAR].global_val.p
#define VAR_THREAD_LASTREPLY_CHAR    vars[V_THREAD_LASTREPLY_CHAR].current_val.p
#define GLO_THREAD_LASTREPLY_CHAR    vars[V_THREAD_LASTREPLY_CHAR].global_val.p

#if defined(DOS) || defined(OS2)
#define	VAR_FILE_DIR		     vars[V_FILE_DIR].current_val.p
#define	GLO_FILE_DIR		     vars[V_FILE_DIR].current_val.p
#define VAR_FOLDER_EXTENSION	     vars[V_FOLDER_EXTENSION].current_val.p
#define GLO_FOLDER_EXTENSION	     vars[V_FOLDER_EXTENSION].global_val.p

#ifdef	_WINDOWS
#define VAR_FONT_NAME		     vars[V_FONT_NAME].current_val.p
#define VAR_FONT_SIZE		     vars[V_FONT_SIZE].current_val.p
#define VAR_FONT_STYLE		     vars[V_FONT_STYLE].current_val.p
#define VAR_FONT_CHAR_SET      	     vars[V_FONT_CHAR_SET].current_val.p
#define VAR_PRINT_FONT_NAME	     vars[V_PRINT_FONT_NAME].current_val.p
#define VAR_PRINT_FONT_SIZE	     vars[V_PRINT_FONT_SIZE].current_val.p
#define VAR_PRINT_FONT_STYLE	     vars[V_PRINT_FONT_STYLE].current_val.p
#define VAR_PRINT_FONT_CHAR_SET	     vars[V_PRINT_FONT_CHAR_SET].current_val.p
#define VAR_WINDOW_POSITION	     vars[V_WINDOW_POSITION].current_val.p
#define VAR_CURSOR_STYLE	     vars[V_CURSOR_STYLE].current_val.p
#endif	/* _WINDOWS */
#endif	/* DOS or OS2 */

#define VAR_NORM_FORE_COLOR	     vars[V_NORM_FORE_COLOR].current_val.p
#define GLO_NORM_FORE_COLOR	     vars[V_NORM_FORE_COLOR].global_val.p
#define VAR_NORM_BACK_COLOR	     vars[V_NORM_BACK_COLOR].current_val.p
#define GLO_NORM_BACK_COLOR	     vars[V_NORM_BACK_COLOR].global_val.p
#define VAR_REV_FORE_COLOR	     vars[V_REV_FORE_COLOR].current_val.p
#define GLO_REV_FORE_COLOR	     vars[V_REV_FORE_COLOR].global_val.p
#define VAR_REV_BACK_COLOR	     vars[V_REV_BACK_COLOR].current_val.p
#define GLO_REV_BACK_COLOR	     vars[V_REV_BACK_COLOR].global_val.p
#define VAR_TITLE_FORE_COLOR	     vars[V_TITLE_FORE_COLOR].current_val.p
#define GLO_TITLE_FORE_COLOR	     vars[V_TITLE_FORE_COLOR].global_val.p
#define VAR_TITLE_BACK_COLOR	     vars[V_TITLE_BACK_COLOR].current_val.p
#define GLO_TITLE_BACK_COLOR	     vars[V_TITLE_BACK_COLOR].global_val.p
#define VAR_TITLECLOSED_FORE_COLOR   vars[V_TITLECLOSED_FORE_COLOR].current_val.p
#define GLO_TITLECLOSED_FORE_COLOR   vars[V_TITLECLOSED_FORE_COLOR].global_val.p
#define VAR_TITLECLOSED_BACK_COLOR   vars[V_TITLECLOSED_BACK_COLOR].current_val.p
#define GLO_TITLECLOSED_BACK_COLOR   vars[V_TITLECLOSED_BACK_COLOR].global_val.p
#define VAR_STATUS_FORE_COLOR	     vars[V_STATUS_FORE_COLOR].current_val.p
#define VAR_STATUS_BACK_COLOR	     vars[V_STATUS_BACK_COLOR].current_val.p
#define VAR_HEADER_GENERAL_FORE_COLOR vars[V_HEADER_GENERAL_FORE_COLOR].current_val.p
#define VAR_HEADER_GENERAL_BACK_COLOR vars[V_HEADER_GENERAL_BACK_COLOR].current_val.p
#define VAR_IND_PLUS_FORE_COLOR	     vars[V_IND_PLUS_FORE_COLOR].current_val.p
#define GLO_IND_PLUS_FORE_COLOR	     vars[V_IND_PLUS_FORE_COLOR].global_val.p
#define VAR_IND_PLUS_BACK_COLOR	     vars[V_IND_PLUS_BACK_COLOR].current_val.p
#define GLO_IND_PLUS_BACK_COLOR	     vars[V_IND_PLUS_BACK_COLOR].global_val.p
#define VAR_IND_IMP_FORE_COLOR	     vars[V_IND_IMP_FORE_COLOR].current_val.p
#define GLO_IND_IMP_FORE_COLOR	     vars[V_IND_IMP_FORE_COLOR].global_val.p
#define VAR_IND_IMP_BACK_COLOR	     vars[V_IND_IMP_BACK_COLOR].current_val.p
#define GLO_IND_IMP_BACK_COLOR	     vars[V_IND_IMP_BACK_COLOR].global_val.p
#define VAR_IND_DEL_FORE_COLOR	     vars[V_IND_DEL_FORE_COLOR].current_val.p
#define VAR_IND_DEL_BACK_COLOR	     vars[V_IND_DEL_BACK_COLOR].current_val.p
#define VAR_IND_ANS_FORE_COLOR	     vars[V_IND_ANS_FORE_COLOR].current_val.p
#define GLO_IND_ANS_FORE_COLOR	     vars[V_IND_ANS_FORE_COLOR].global_val.p
#define VAR_IND_ANS_BACK_COLOR	     vars[V_IND_ANS_BACK_COLOR].current_val.p
#define GLO_IND_ANS_BACK_COLOR	     vars[V_IND_ANS_BACK_COLOR].global_val.p
#define VAR_IND_NEW_FORE_COLOR	     vars[V_IND_NEW_FORE_COLOR].current_val.p
#define GLO_IND_NEW_FORE_COLOR	     vars[V_IND_NEW_FORE_COLOR].global_val.p
#define VAR_IND_NEW_BACK_COLOR	     vars[V_IND_NEW_BACK_COLOR].current_val.p
#define GLO_IND_NEW_BACK_COLOR	     vars[V_IND_NEW_BACK_COLOR].global_val.p
#define VAR_IND_REC_FORE_COLOR	     vars[V_IND_REC_FORE_COLOR].current_val.p
#define VAR_IND_REC_BACK_COLOR	     vars[V_IND_REC_BACK_COLOR].current_val.p
#define VAR_IND_FWD_FORE_COLOR	     vars[V_IND_FWD_FORE_COLOR].current_val.p
#define VAR_IND_FWD_BACK_COLOR	     vars[V_IND_FWD_BACK_COLOR].current_val.p
#define VAR_IND_UNS_FORE_COLOR	     vars[V_IND_UNS_FORE_COLOR].current_val.p
#define VAR_IND_UNS_BACK_COLOR	     vars[V_IND_UNS_BACK_COLOR].current_val.p
#define VAR_IND_OP_FORE_COLOR	     vars[V_IND_OP_FORE_COLOR].current_val.p
#define GLO_IND_OP_FORE_COLOR	     vars[V_IND_OP_FORE_COLOR].global_val.p
#define VAR_IND_OP_BACK_COLOR	     vars[V_IND_OP_BACK_COLOR].current_val.p
#define GLO_IND_OP_BACK_COLOR	     vars[V_IND_OP_BACK_COLOR].global_val.p
#define VAR_IND_SUBJ_FORE_COLOR	     vars[V_IND_SUBJ_FORE_COLOR].current_val.p
#define VAR_IND_SUBJ_BACK_COLOR	     vars[V_IND_SUBJ_BACK_COLOR].current_val.p
#define VAR_IND_FROM_FORE_COLOR	     vars[V_IND_FROM_FORE_COLOR].current_val.p
#define VAR_IND_FROM_BACK_COLOR	     vars[V_IND_FROM_BACK_COLOR].current_val.p
#define VAR_IND_HIPRI_FORE_COLOR     vars[V_IND_HIPRI_FORE_COLOR].current_val.p
#define VAR_IND_HIPRI_BACK_COLOR     vars[V_IND_HIPRI_BACK_COLOR].current_val.p
#define VAR_IND_LOPRI_FORE_COLOR     vars[V_IND_LOPRI_FORE_COLOR].current_val.p
#define VAR_IND_LOPRI_BACK_COLOR     vars[V_IND_LOPRI_BACK_COLOR].current_val.p
#define VAR_IND_ARR_FORE_COLOR	     vars[V_IND_ARR_FORE_COLOR].current_val.p
#define VAR_IND_ARR_BACK_COLOR	     vars[V_IND_ARR_BACK_COLOR].current_val.p
#define VAR_KEYLABEL_FORE_COLOR	     vars[V_KEYLABEL_FORE_COLOR].current_val.p
#define VAR_KEYLABEL_BACK_COLOR	     vars[V_KEYLABEL_BACK_COLOR].current_val.p
#define VAR_KEYNAME_FORE_COLOR	     vars[V_KEYNAME_FORE_COLOR].current_val.p
#define VAR_KEYNAME_BACK_COLOR	     vars[V_KEYNAME_BACK_COLOR].current_val.p
#define VAR_SLCTBL_FORE_COLOR	     vars[V_SLCTBL_FORE_COLOR].current_val.p
#define VAR_SLCTBL_BACK_COLOR	     vars[V_SLCTBL_BACK_COLOR].current_val.p
#define VAR_METAMSG_FORE_COLOR	     vars[V_METAMSG_FORE_COLOR].current_val.p
#define GLO_METAMSG_FORE_COLOR	     vars[V_METAMSG_FORE_COLOR].global_val.p
#define VAR_METAMSG_BACK_COLOR	     vars[V_METAMSG_BACK_COLOR].current_val.p
#define GLO_METAMSG_BACK_COLOR	     vars[V_METAMSG_BACK_COLOR].global_val.p
#define VAR_QUOTE1_FORE_COLOR	     vars[V_QUOTE1_FORE_COLOR].current_val.p
#define GLO_QUOTE1_FORE_COLOR	     vars[V_QUOTE1_FORE_COLOR].global_val.p
#define VAR_QUOTE1_BACK_COLOR	     vars[V_QUOTE1_BACK_COLOR].current_val.p
#define GLO_QUOTE1_BACK_COLOR	     vars[V_QUOTE1_BACK_COLOR].global_val.p
#define VAR_QUOTE2_FORE_COLOR	     vars[V_QUOTE2_FORE_COLOR].current_val.p
#define GLO_QUOTE2_FORE_COLOR	     vars[V_QUOTE2_FORE_COLOR].global_val.p
#define VAR_QUOTE2_BACK_COLOR	     vars[V_QUOTE2_BACK_COLOR].current_val.p
#define GLO_QUOTE2_BACK_COLOR	     vars[V_QUOTE2_BACK_COLOR].global_val.p
#define VAR_QUOTE3_FORE_COLOR	     vars[V_QUOTE3_FORE_COLOR].current_val.p
#define GLO_QUOTE3_FORE_COLOR	     vars[V_QUOTE3_FORE_COLOR].global_val.p
#define VAR_QUOTE3_BACK_COLOR	     vars[V_QUOTE3_BACK_COLOR].current_val.p
#define GLO_QUOTE3_BACK_COLOR	     vars[V_QUOTE3_BACK_COLOR].global_val.p
#define VAR_INCUNSEEN_FORE_COLOR     vars[V_INCUNSEEN_FORE_COLOR].current_val.p
#define VAR_INCUNSEEN_BACK_COLOR     vars[V_INCUNSEEN_BACK_COLOR].current_val.p
#define VAR_SIGNATURE_FORE_COLOR     vars[V_SIGNATURE_FORE_COLOR].current_val.p
#define GLO_SIGNATURE_FORE_COLOR     vars[V_SIGNATURE_FORE_COLOR].global_val.p
#define VAR_SIGNATURE_BACK_COLOR     vars[V_SIGNATURE_BACK_COLOR].current_val.p
#define GLO_SIGNATURE_BACK_COLOR     vars[V_SIGNATURE_BACK_COLOR].global_val.p
#define VAR_PROMPT_FORE_COLOR	     vars[V_PROMPT_FORE_COLOR].current_val.p
#define VAR_PROMPT_BACK_COLOR	     vars[V_PROMPT_BACK_COLOR].current_val.p
#define VAR_VIEW_HDR_COLORS	     vars[V_VIEW_HDR_COLORS].current_val.l
#ifdef SMIME
#define VAR_PUBLICCERT_DIR	     vars[V_PUBLICCERT_DIR].current_val.p
#define GLO_PUBLICCERT_DIR	     vars[V_PUBLICCERT_DIR].global_val.p
#define VAR_PRIVATEKEY_DIR	     vars[V_PRIVATEKEY_DIR].current_val.p
#define GLO_PRIVATEKEY_DIR	     vars[V_PRIVATEKEY_DIR].global_val.p
#define VAR_CACERT_DIR		     vars[V_CACERT_DIR].current_val.p
#define GLO_CACERT_DIR		     vars[V_CACERT_DIR].global_val.p
#define VAR_PUBLICCERT_CONTAINER     vars[V_PUBLICCERT_CONTAINER].current_val.p
#define VAR_PRIVATEKEY_CONTAINER     vars[V_PRIVATEKEY_CONTAINER].current_val.p
#define VAR_CACERT_CONTAINER	     vars[V_CACERT_CONTAINER].current_val.p
#endif /* SMIME */


/*
 * Define some bitmap operations for manipulating features.
 */

#define _BITCHAR(bit)		((bit) / 8)
#define _BITBIT(bit)		(1 << ((bit) % 8))

/* is bit set? */
#define bitnset(bit,map)	(((map)[_BITCHAR(bit)] & _BITBIT(bit)) ? 1 : 0)
/* set bit */
#define setbitn(bit,map)	((map)[_BITCHAR(bit)] |= _BITBIT(bit))
/* clear bit */
#define clrbitn(bit,map)	((map)[_BITCHAR(bit)] &= ~_BITBIT(bit))


/* Feature list support */
/* Is feature "feature" turned on? */
#define F_ON(feature,ps)	(bitnset((feature),(ps)->feature_list))
#define F_OFF(feature,ps)	(!F_ON(feature,ps))
#define F_TURN_ON(feature,ps)   (setbitn((feature),(ps)->feature_list))
#define F_TURN_OFF(feature,ps)  (clrbitn((feature),(ps)->feature_list))
/* turn off or on depending on value */
#define F_SET(feature,ps,value) ((value) ? F_TURN_ON((feature),(ps))       \
					 : F_TURN_OFF((feature),(ps)))

/*
 * Save message rules.  if these grow, widen pine
 * struct's save_msg_rule...
 */
#define	SAV_RULE_DEFLT			0
#define	SAV_RULE_LAST			1
#define	SAV_RULE_FROM			2
#define	SAV_RULE_NICK_FROM_DEF		3
#define	SAV_RULE_NICK_FROM		4
#define	SAV_RULE_FCC_FROM_DEF		5
#define	SAV_RULE_FCC_FROM		6
#define	SAV_RULE_RN_FROM_DEF		7
#define	SAV_RULE_RN_FROM		8
#define	SAV_RULE_SENDER			9
#define	SAV_RULE_NICK_SENDER_DEF	10
#define	SAV_RULE_NICK_SENDER		11
#define	SAV_RULE_FCC_SENDER_DEF		12
#define	SAV_RULE_FCC_SENDER		13
#define	SAV_RULE_RN_SENDER_DEF		14
#define	SAV_RULE_RN_SENDER		15
#define	SAV_RULE_RECIP			16
#define	SAV_RULE_NICK_RECIP_DEF		17
#define	SAV_RULE_NICK_RECIP		18
#define	SAV_RULE_FCC_RECIP_DEF		19
#define	SAV_RULE_FCC_RECIP		20
#define	SAV_RULE_RN_RECIP_DEF		21
#define	SAV_RULE_RN_RECIP		22
#define	SAV_RULE_REPLYTO		23
#define	SAV_RULE_NICK_REPLYTO_DEF	24
#define	SAV_RULE_NICK_REPLYTO		25
#define	SAV_RULE_FCC_REPLYTO_DEF	26
#define	SAV_RULE_FCC_REPLYTO		27
#define	SAV_RULE_RN_REPLYTO_DEF		28
#define	SAV_RULE_RN_REPLYTO		29

/*
 * Fcc rules.  if these grow, widen pine
 * struct's fcc_rule...
 */
#define	FCC_RULE_DEFLT		0
#define	FCC_RULE_RECIP		1
#define	FCC_RULE_LAST		2
#define	FCC_RULE_NICK		3
#define	FCC_RULE_NICK_RECIP	4
#define	FCC_RULE_CURRENT	5

/*
 * Addrbook sorting rules.  if these grow, widen pine
 * struct's ab_sort_rule...
 */
#define	AB_SORT_RULE_FULL_LISTS		0
#define	AB_SORT_RULE_FULL  		1
#define	AB_SORT_RULE_NICK_LISTS         2
#define	AB_SORT_RULE_NICK 		3
#define	AB_SORT_RULE_NONE		4

/*
 * Incoming startup rules.  if these grow, widen pine
 * struct's inc_startup_rule and reset_startup_rule().
 */
#define	IS_FIRST_UNSEEN			0
#define	IS_FIRST_RECENT  		1
#define	IS_FIRST_IMPORTANT  		2
#define	IS_FIRST_IMPORTANT_OR_UNSEEN  	3
#define	IS_FIRST_IMPORTANT_OR_RECENT  	4
#define	IS_FIRST         		5
#define	IS_LAST 			6
#define	IS_NOTSET			7	/* for reset version */

/*
 * Pruning rules. If these grow, widen pruning_rule.
 */
#define	PRUNE_ASK_AND_ASK		0
#define	PRUNE_ASK_AND_NO		1
#define	PRUNE_YES_AND_ASK  		2
#define	PRUNE_YES_AND_NO  		3
#define	PRUNE_NO_AND_ASK  		4
#define	PRUNE_NO_AND_NO  		5

/*
 * Folder reopen rules. If these grow, widen reopen_rule.
 */
#define	REOPEN_YES_YES			0
#define	REOPEN_YES_ASK_Y		1
#define	REOPEN_YES_ASK_N		2
#define	REOPEN_YES_NO			3
#define	REOPEN_ASK_ASK_Y		4
#define	REOPEN_ASK_ASK_N		5
#define	REOPEN_ASK_NO_Y			6
#define	REOPEN_ASK_NO_N			7
#define	REOPEN_NO_NO			8

/*
 * Goto default rules.
 */
#define	GOTO_INBOX_RECENT_CLCTN		0
#define	GOTO_INBOX_FIRST_CLCTN		1
#define	GOTO_LAST_FLDR			2
#define	GOTO_FIRST_CLCTN		3
#define	GOTO_FIRST_CLCTN_DEF_INBOX	4

/*
 * Thread display styles. If these grow, widen thread_disp_rule.
 */
#define	THREAD_NONE			0
#define	THREAD_STRUCT			1
#define	THREAD_MUTTLIKE			2
#define	THREAD_INDENT_SUBJ1		3
#define	THREAD_INDENT_SUBJ2		4
#define	THREAD_INDENT_FROM1		5
#define	THREAD_INDENT_FROM2		6
#define	THREAD_STRUCT_FROM		7

/*
 * Thread index styles. If these grow, widen thread_index_rule.
 */
#define	THRDINDX_EXP			0
#define	THRDINDX_COLL			1
#define	THRDINDX_SEP			2
#define	THRDINDX_SEP_AUTO		3

/*
 * Titlebar color styles. If these grow, widen titlebar_color_style.
 */
#define	TBAR_COLOR_DEFAULT		0
#define	TBAR_COLOR_INDEXLINE		1
#define	TBAR_COLOR_REV_INDEXLINE	2

/*
 * PC-Pine update registry modes
 */
#ifdef _WINDOWS
#define UREG_NORMAL       0
#define UREG_ALWAYS_SET   1
#define UREG_NEVER_SET    2
#endif


/*
 * Folder list sort rules
 */
#define	FLD_SORT_ALPHA			0
#define	FLD_SORT_ALPHA_DIR_LAST		1
#define	FLD_SORT_ALPHA_DIR_FIRST	2


/*
 * Color styles
 */
#define	COL_NONE	0
#define	COL_TERMDEF	1
#define	COL_ANSI8	2
#define	COL_ANSI16	3
#define	COL_ANSI256	4


/* styles for apply_rev_color() */
#define IND_COL_FLIP		0
#define IND_COL_REV		1
#define IND_COL_FG		2
#define IND_COL_FG_NOAMBIG	3
#define IND_COL_BG		4
#define IND_COL_BG_NOAMBIG	5



/*
 * Macros to help set numeric pinerc variables.  Defined here are the 
 * allowed min and max values as well as the name of the var as it
 * should be displayed in error messages.
 */
#define Q_SUPP_LIMIT (4)
#define Q_DEL_ALL    (-10)
#define	SVAR_OVERLAP(ps,n,e,el)	strtoval((ps)->VAR_OVERLAP,		  \
					 &(n), 0, 20, 0, (e),		  \
					  (el), 			  \
					 "Viewer-Overlap")
#define	SVAR_MAXREMSTREAM(ps,n,e,el) strtoval((ps)->VAR_MAXREMSTREAM,	  \
					 &(n), 0, 15, 0, (e),		  \
					  (el), 			  \
					 "Max-Remote-Connections")
#define	SVAR_MARGIN(ps,n,e,el)	strtoval((ps)->VAR_MARGIN,		  \
					 &(n), 0, 20, 0, (e),		  \
					  (el), 			  \
					 "Scroll-Margin")
#define	SVAR_FILLCOL(ps,n,e,el)	strtoval((ps)->VAR_FILLCOL,		  \
					 &(n), 0, MAX_FILLCOL, 0, (e),	  \
					  (el), 			  \
					 "Composer-Wrap-Column")
#define	SVAR_QUOTE_SUPPRESSION(ps,n,e,el) strtoval((ps)->VAR_QUOTE_SUPPRESSION, \
					 &(n), -(Q_SUPP_LIMIT-1),	  \
					 1000, Q_DEL_ALL, (e),		  \
					  (el), 			  \
					 "Quote-Suppression-Threshold")
#define	SVAR_DEADLETS(ps,n,e,el) strtoval((ps)->VAR_DEADLETS,		  \
					 &(n), 0, 9, 0, (e),		  \
					  (el), 			  \
					 "Dead-Letter-Files")
#define	SVAR_MSGDLAY(ps,n,e,el)	strtoval((ps)->VAR_STATUS_MSG_DELAY,	  \
					 &(n), -10, 30, 0, (e),		  \
					  (el), 			  \
					"Status-Message-Delay")
#define	SVAR_ACTIVEINTERVAL(ps,n,e,el)	strtoval((ps)->VAR_ACTIVE_MSG_INTERVAL, \
					 &(n), 0, 20, 0, (e),		  \
					  (el), 			  \
					"Active-Msg-Updates-Per-Second")
#define	SVAR_MAILCHK(ps,n,e,el)	strtoval((ps)->VAR_MAILCHECK,		  \
					 &(n), 15, 30000, 0, (e),	  \
					  (el), 			  \
					"Mail-Check-Interval")
#define	SVAR_MAILCHKNONCURR(ps,n,e,el) strtoval((ps)->VAR_MAILCHECKNONCURR,  \
					 &(n), 15, 30000, 0, (e),	  \
					  (el), 			  \
					"Mail-Check-Interval-Noncurrent")
#define	SVAR_AB_HIST(ps,n,e,el)	strtoval((ps)->VAR_REMOTE_ABOOK_HISTORY,  \
					 &(n), 0, 100, 0, (e),		  \
					  (el), 			  \
					"Remote-Abook-History")
#define	SVAR_AB_VALID(ps,n,e,el) strtoval((ps)->VAR_REMOTE_ABOOK_VALIDITY, \
					 &(n), -1, 30000, 0, (e),	  \
					  (el), 			  \
					"Remote-Abook-Validity")
#define	SVAR_USER_INPUT(ps,n,e,el) strtoval((ps)->VAR_USERINPUTTIMEO,	  \
					 &(n), 0, 1000, 0, (e),		  \
					  (el), 			  \
					"User-Input-Timeout")
#define	SVAR_TCP_OPEN(ps,n,e,el) strtoval((ps)->VAR_TCPOPENTIMEO, 	  \
					 &(n), 5, 30000, 5, (e),	  \
					  (el), 			  \
					"Tcp-Open-Timeout")
#define	SVAR_TCP_READWARN(ps,n,e,el) strtoval((ps)->VAR_TCPREADWARNTIMEO,  \
					 &(n), 5, 30000, 5, (e),	  \
					  (el), 			  \
					"Tcp-Read-Warning-Timeout")
#define	SVAR_TCP_WRITEWARN(ps,n,e,el) strtoval((ps)->VAR_TCPWRITEWARNTIMEO, \
					 &(n), 5, 30000, 0, (e),	  \
					  (el), 			  \
					"Tcp-Write-Warning-Timeout")
#define	SVAR_TCP_QUERY(ps,n,e,el) strtoval((ps)->VAR_TCPQUERYTIMEO, 	  \
					 &(n), 5, 30000, 0, (e),	  \
					  (el), 			  \
					"Tcp-Query-Timeout")
#define	SVAR_RSH_OPEN(ps,n,e,el) strtoval((ps)->VAR_RSHOPENTIMEO, 	  \
					 &(n), 5, 30000, 0, (e),	  \
					  (el), 			  \
					"Rsh-Open-Timeout")
#define	SVAR_SSH_OPEN(ps,n,e,el) strtoval((ps)->VAR_SSHOPENTIMEO, 	  \
					 &(n), 5, 30000, 0, (e),	  \
					  (el), 			  \
					"Ssh-Open-Timeout")
#define	SVAR_INC_CHECK_TIMEO(ps,n,e,el) strtoval((ps)->VAR_INCCHECKTIMEO, \
					 &(n), 1, 30000, 1, (e),	  \
					  (el), 			  \
					"Incoming-Check-Timeout")
#define	SVAR_INC_CHECK_INTERV(ps,n,e,el) strtoval((ps)->VAR_INCCHECKINTERVAL, \
					 &(n), 15, 30000, 0, (e),	  \
					  (el), 			  \
					"Incoming-Check-Interval")

#define	SVAR_INC_2NDCHECK_INTERV(ps,n,e,el) strtoval((ps)->VAR_INC2NDCHECKINTERVAL, \
					 &(n), 15, 30000, 0, (e),	  \
					  (el), 			  \
					"Incoming-Secondary-Check-Interval")

#define	SVAR_NNTPRANGE(ps,n,e,el) strtolval((ps)->VAR_NNTPRANGE,	  \
					  &(n), 0L, 30000L, 0L, (e),	  \
					  (el), 			  \
					  "Nntp-Range")
#define	SVAR_MAILDCHK(ps,n,e,el) strtolval((ps)->VAR_MAILDROPCHECK,	  \
					  &(n), 60L, 30000L, 0L, (e),	  \
					  (el), 			  \
					  "Maildrop-Check-Minimum")
#define	SVAR_NMW_WIDTH(ps,n,e,el) strtoval((ps)->VAR_NMW_WIDTH,		  \
					 &(n), 20, 170, 0, (e),		  \
					  (el), 			  \
					 "NewMail-Window-Width")


/*----------------------------------------------------------------------
   struct for pruning old Fcc, usually "sent-mail" folders.     
  ----*/
struct sm_folder {
    char *name;
    int   month_num;
};


#define PVAL(v,w) ((v) ? (((w) == Main) ? (v)->main_user_val.p :	\
	                                  (v)->post_user_val.p) : NULL)
#define APVAL(v,w) ((v) ? (((w) == Main) ? &(v)->main_user_val.p :	\
	                                   &(v)->post_user_val.p) : NULL)
#define LVAL(v,w) ((v) ? (((w) == Main) ? (v)->main_user_val.l :	\
	                                  (v)->post_user_val.l) : NULL)
#define ALVAL(v,w) ((v) ? (((w) == Main) ? &(v)->main_user_val.l :	\
	                                   &(v)->post_user_val.l) : NULL)


/* use shortname if present, else regular long name */
#define S_OR_L(v)	(((v) && (v)->shortname) ? (v)->shortname : \
			  ((v) ? (v)->name : NULL))


/*
 * Flags for write_pinerc
 */
#define	WRP_NONE		   0
#define	WRP_NOUSER		0x01
#define	WRP_PRESERV_WRITTEN	0x02


#define ALL_EXCEPT	"all-except"
#define INHERIT		"INHERIT"
#define HIDDEN_PREF	"Normally Hidden Preferences"
#define WYSE_PRINTER	"attached-to-wyse"
#define METASTR "\nremote-abook-metafile="


#define	CONF_TXT_T	char


/* exported protoypes */
void       init_init_vars(struct pine *);
void       init_pinerc(struct pine *, char **);
void       init_vars(struct pine *, void (*)(struct pine *, char **));
char      *feature_list_section(FEATURE_S *);
FEATURE_S *feature_list(int);
int        feature_list_index(int);
int        feature_list_id(char *);
char      *feature_list_name(int);
struct variable *var_from_name(char *);
HelpType   feature_list_help(int);
void       process_feature_list(struct pine *, char **, int , int, int);
void       cur_rule_value(struct variable *, int, int);
NAMEVAL_S *save_msg_rules(int);
NAMEVAL_S *fcc_rules(int);
NAMEVAL_S *ab_sort_rules(int);
NAMEVAL_S *col_style(int);
NAMEVAL_S *index_col_style(int);
NAMEVAL_S *titlebar_col_style(int);
NAMEVAL_S *fld_sort_rules(int);
NAMEVAL_S *incoming_startup_rules(int);
NAMEVAL_S *startup_rules(int);
NAMEVAL_S *pruning_rules(int);
NAMEVAL_S *reopen_rules(int);
NAMEVAL_S *thread_disp_styles(int);
NAMEVAL_S *thread_index_styles(int);
NAMEVAL_S *goto_rules(int);
NAMEVAL_S *pat_fldr_types(int);
NAMEVAL_S *inabook_fldr_types(int);
NAMEVAL_S *filter_types(int);
NAMEVAL_S *role_repl_types(int);
NAMEVAL_S *role_forw_types(int);
NAMEVAL_S *role_comp_types(int);
NAMEVAL_S *role_status_types(int);
NAMEVAL_S *msg_state_types(int);
#ifdef	ENABLE_LDAP
NAMEVAL_S *ldap_search_rules(int);
NAMEVAL_S *ldap_search_types(int);
NAMEVAL_S *ldap_search_scope(int);
#endif
void       set_current_val(struct variable *, int, int);
void       set_news_spec_current_val(int, int);
void       set_feature_list_current_val(struct variable *);
char      *expand_variables(char *, size_t, char *, int);
void       init_error(struct pine *, int, int, int, char *);
void       read_pinerc(PINERC_S *, struct variable *, ParsePinerc);
int        write_pinerc(struct pine *, EditWhich, int);
void       quit_to_edit_msg(PINERC_S *);
int        var_in_pinerc(char *);
void       dump_global_conf(void);
void       dump_new_pinerc(char *);
int        set_variable(int, char *, int, int, EditWhich);
int        set_variable_list(int, char **, int, EditWhich);
void       set_current_color_vals(struct pine *);
int        var_defaults_to_rev(struct variable *);
void       set_custom_spec_colors(struct pine *);
SPEC_COLOR_S *spec_color_from_var(char *, int);
SPEC_COLOR_S *spec_colors_from_varlist(char **, int);
char      *var_from_spec_color(SPEC_COLOR_S *);
char     **varlist_from_spec_colors(SPEC_COLOR_S *);
void	   update_posting_charset(struct pine *, int);
int	   test_old_growth_bits(struct pine *, int);
int        feature_gets_an_x(struct pine *, struct variable *, FEATURE_S *, char **, EditWhich);
int        longest_feature_comment(struct pine *, EditWhich);
void       toggle_feature(struct pine *, struct variable *, FEATURE_S *, int, EditWhich);
int        feature_in_list(char **, char *);
int        test_feature(char **, char *, int);
void       set_feature(char ***, char *, int);
int        reset_character_set_stuff(char **);
void       parse_printer(char *, char **, char **, char **, char **, char **, char **);
int        copy_pinerc(char *, char *, char **);
int        copy_abook(char *, char *, char **);
HelpType   config_help(int, int);
int        printer_value_check_and_adjust(void);
char     **get_supported_options(void);
unsigned   reset_startup_rule(MAILSTREAM *);
void	   free_pinerc_lines(PINERC_LINE **);
void	   panic1(char *, char *);

/* mandatory to implement prototypes */
int	   set_input_timeout(int);
int	   get_input_timeout();

/* decide what to do: return 0 to continue, nonzero to abort pinerc write */
int	   unexpected_pinerc_change();


#endif /* PITH_CONFIG_INCLUDED */
