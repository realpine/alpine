/*
 * $Id: conftype.h 1012 2008-03-26 00:44:22Z hubert@u.washington.edu $
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

#ifndef PINE_CONFTYPE_INCLUDED
#define PINE_CONFTYPE_INCLUDED


#include "keymenu.h"
#include "help.h"
#include "flagmaint.h"
#include "context.h"
#include "listsel.h"
#include "../pith/pattern.h"
#include "../pith/conf.h"
#include "../pith/ldap.h"


typedef enum {ListMode, SingleMode} ScreenMode;


typedef struct edit_arb {
    struct variable *v;
    ARBHDR_S        *a;
    struct edit_arb *next;
} EARB_S;


typedef struct conf_line {
    char	     *varname,			/* alloc'd var name string   */
		     *value;			/* alloc'd var value string  */
    short	      varoffset;		/* offset from screen left   */
    short	      valoffset;		/* offset from screen left   */
    short	      val2offset;		/* offset from screen left   */
    struct variable  *var;			/* pointer to pinerc var     */
    long	      varmem;			/* value's index, if list    */
						/* tool to manipulate values */
    int		      (*tool)(struct pine *, int, struct conf_line **, unsigned);
    struct key_menu  *keymenu;			/* tool-specific  keymenu    */
    HelpType	      help;			/* variable's help text      */
    char	     *help_title;
    unsigned          flags;
    struct conf_line *varnamep;		/* pointer to varname        */
    struct conf_line *headingp;		/* pointer to heading        */
    struct conf_line *next, *prev;
    union flag_or_context_data {
	struct flag_conf {
	    struct flag_table **ftbl;	/* address of start of table */
	    struct flag_table  *fp;	/* pointer into table for each row */
	} f;
	struct context_and_screen {
	    CONTEXT_S  *ct;
	    CONT_SCR_S *cs;
	} c;
	struct role_conf {
	    PAT_LINE_S *patline;
	    PAT_S      *pat;
	    PAT_S     **selected;
	    int        *change_def;
	} r;
	struct abook_conf {
	    char **selected;
	    char  *abookname;
	} b;
	EARB_S **earb;
	struct list_select {
	    LIST_SEL_S *lsel;
	    ScreenMode *listmode;
	} l;
#ifdef	ENABLE_LDAP
	struct entry_and_screen {
	    LDAP          *ld;
	    LDAPMessage   *entry;
	    LDAP_SERV_S   *info_used;
	    char          *serv;
	    ADDR_CHOOSE_S *ac;
	} a;
#endif
	struct take_export_val {
	    int         selected;
	    char       *exportval;
	    ScreenMode *listmode;
	} t;
    } d;
} CONF_S;


/*
 * Valid for flags argument of config screen tools or flags field in CONF_S
 */
#define	CF_CHANGES	0x0001		/* Have been earlier changes */
#define	CF_NOSELECT	0x0002		/* This line is unselectable */
#define	CF_NOHILITE	0x0004		/* Don't highlight varname   */
#define	CF_NUMBER	0x0008		/* Input should be numeric   */
#define	CF_INVISIBLEVAR	0x0010		/* Don't show the varname    */
#define CF_PRINTER      0x0020		/* Printer config line       */
#define	CF_H_LINE	0x0040		/* Horizontal line	     */
#define	CF_B_LINE	0x0080		/* Blank line		     */
#define	CF_CENTERED	0x0100		/* Centered text	     */
#define	CF_STARTITEM	0x0200		/* Start of an "item"        */
#define	CF_PRIVATE	0x0400		/* Private flag for tool     */
#define	CF_DOUBLEVAR	0x0800		/* Line has 2 settable vars  */
#define	CF_VAR2		0x1000		/* Cursor on 2nd of 2 vars   */
#define	CF_COLORSAMPLE	0x2000		/* Show color sample here    */
#define	CF_POT_SLCTBL	0x4000		/* Potentially selectable    */
#define	CF_INHERIT	0x8000		/* Inherit Defaults line     */


typedef struct save_config {
    union {
	char  *p;
	char **l;
    } saved_user_val;
} SAVED_CONFIG_S;


typedef struct conf_screen {
    CONF_S  *current,
	    *prev,
	    *top_line;
    int      ro_warning,
	     deferred_ro_warning;
} OPT_SCREEN_S;


/* exported protoypes */


#endif /* PINE_CONFTYPE_INCLUDED */
