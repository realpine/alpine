#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: keymenu.c 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $";
#endif

/*
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

#include "headers.h"
#include "keymenu.h"
#include "mailcmd.h"
#include "signal.h"
#include "status.h"
#include "../pith/bitmap.h"
#include "../pith/conf.h"
#include "../pith/state.h"


/*
 * We put all of the keymenu definitions here so that it is easy to
 * share them. The names are in keymenu.h, as well. Some of these
 * aren't easily shareable because we modify them dynamically but
 * here they are anyway. It's not altogether clear that this is a good idea.
 * Perhaps it would be better to just define the keymenus multiple times
 * in different source files even though they are the same, with static
 * declarations.
 *
 * The key numbers are sometimes used symbolically, like OTHER_KEY. Those
 * are in keymenu.h, too, and are hard to use. If you change a menu here
 * be sure to check those key numbers that go with it in keymenu.h.
 */

#ifdef _WINDOWS
void configure_menu_items (struct key_menu *, bitmap_t);
#endif


/*
 * Macro to simplify instantiation of key_menu structs from key structs
 */
#define	INST_KEY_MENU(X, Y)	struct key_menu X = \
					{sizeof(Y)/(sizeof(Y[0])*12), 0, 0, Y}

struct key cancel_keys[] = 
       {HELP_MENU,
	{"^C",N_("Cancel"),{MC_NONE},KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(cancel_keymenu, cancel_keys);


/*
 * A bunch of these are NULL_MENU because we want to change them dynamically.
 */
struct key ab_keys[] =
       {HELP_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	/* TRANSLATORS: go to Previous Entry */
	{"P", N_("PrevEntry"), {MC_PREVITEM,1,{'p'}}, KS_NONE},
	/* TRANSLATORS: go to Next Entry */
	{"N", N_("NextEntry"), {MC_NEXTITEM,1,{'n'}}, KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	QUIT_MENU,
	NULL_MENU,
	LISTFLD_MENU,
	GOTO_MENU,
	INDEX_MENU,
	RCOMPOSE_MENU,
	PRYNTTXT_MENU,
	NULL_MENU,
	SAVE_MENU,
	FORWARD_MENU,

        HELP_MENU,
	OTHER_MENU,
	/* TRANSLATORS: Select this Entry */
	{";",N_("Select"),{MC_SELECT,1,{';'}},KS_NONE},
	/* TRANSLATORS: Apply a command to several objects at once */
	{"A",N_("Apply"),{MC_APPLY,1,{'a'}},KS_APPLY},
	/* TRANSLATORS: Select Current entry */
	{":",N_("SelectCur"),{MC_SELCUR,1,{':'}},KS_SELECTCUR},
	/* TRANSLATORS: Zoom refers to zooming in on a smaller set of
	   items, like with a camera zoom lense */
	{"Z",N_("ZoomMode"),{MC_ZOOM,1,{'z'}},KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(ab_keymenu, ab_keys);


struct key abook_select_keys[] = 
       {HELP_MENU,
	NULL_MENU,
        {"E", N_("Exit"), {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{"S", "[" N_("Select") "]", {MC_CHOICE,3,{'s',ctrl('M'),ctrl('J')}}, KS_NONE},
	/* TRANSLATORS: go to Previous entry */
	{"P", N_("Prev"), {MC_PREVITEM, 1, {'p'}}, KS_NONE},
	{"N", N_("Next"), {MC_NEXTITEM, 2, {'n', TAB}}, KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	NULL_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(abook_select_km, abook_select_keys);


struct key abook_view_keys[] =
       {HELP_MENU,
	OTHER_MENU,
	/* TRANSLATORS: Abook is an abbreviation for Address Book */
	{"<",N_("Abook"),{MC_EXIT,2,{'<',','}},KS_NONE},
	{"U",N_("Update"),{MC_EDIT,1,{'u'}},KS_NONE},
	/* TRANSLATORS: ComposeTo means to start editing a new message to
	    this address book entry */
	{"C",N_("ComposeTo"),{MC_COMPOSE,1,{'c'}},KS_COMPOSER},
	RCOMPOSE_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU,
	/* TRANSLATORS: abbreviation for Forward as Email */
	{"F", N_("Fwd Email"), {MC_FORWARD, 1, {'f'}}, KS_FORWARD},
	SAVE_MENU,

	HELP_MENU,
	OTHER_MENU,
	/* TRANSLATORS: View the highlighted link, for example, a URL */
	{"V",N_("ViewLink"),{MC_VIEW_HANDLE,3,{'v',ctrl('m'),ctrl('j')}},KS_NONE},
	NULL_MENU,
	/* TRANSLATORS: go to the previous link, for example, the previous URL */
	{"^B",N_("PrevLink"),{MC_PREV_HANDLE,1,{ctrl('B')}},KS_NONE},
	{"^F",N_("NextLink"),{MC_NEXT_HANDLE,1,{ctrl('F')}},KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(abook_view_keymenu, abook_view_keys);


struct key abook_text_keys[] =
       {HELP_MENU,
	NULL_MENU,
	{"E",N_("Exit Viewer"),{MC_EXIT,1,{'e'}},KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU,
	FWDEMAIL_MENU,
	{"S", N_("Save"), {MC_SAVETEXT,1,{'s'}}, KS_SAVE}};
INST_KEY_MENU(abook_text_km, abook_text_keys);


struct key ldap_view_keys[] =
       {HELP_MENU,
	OTHER_MENU,
	/* TRANSLATORS: go back to the index of results instead of
	   viewing this particular entry */
	{"<",N_("Results Index"),{MC_EXIT,2,{'<',','}},KS_NONE},
	NULL_MENU,
	{"C", N_("ComposeTo"), {MC_COMPOSE,1,{'c'}}, KS_COMPOSER},
	RCOMPOSE_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU,
	FWDEMAIL_MENU,
	SAVE_MENU,

	HELP_MENU,
	OTHER_MENU,
	{"V",N_("ViewLink"),{MC_VIEW_HANDLE,3,{'v',ctrl('m'),ctrl('j')}},KS_NONE},
	NULL_MENU,
	{"^B",N_("PrevLink"),{MC_PREV_HANDLE,1,{ctrl('B')}},KS_NONE},
	{"^F",N_("NextLink"),{MC_NEXT_HANDLE,1,{ctrl('F')}},KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(ldap_view_keymenu, ldap_view_keys);


struct key context_mgr_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
        {"<", N_("Main Menu"), {MC_MAIN,3,{'m','<',','}}, KS_EXITMODE},
	/* TRANSLATORS: View this Collection of folders */
        {">", "[" N_("View Cltn") "]",
	 {MC_CHOICE,5,{'v','>','.',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREVC_MENU,
	NEXTC_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	QUIT_MENU,
	NULL_MENU,
	NULL_MENU,
	GOTO_MENU,
	CIND_MENU,
	COMPOSE_MENU,
	PRYNTTXT_MENU,
	RCOMPOSE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(c_mgr_km, context_mgr_keys);


struct key context_cfg_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	{"E", N_("Exit Setup"), {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{"C", "[" N_("Change") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREVC_MENU,
	NEXTC_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	/* TRANSLATORS; Add a Collection of folders */
	{"A", N_("Add Cltn"), {MC_ADD,1,{'a'}}, KS_NONE},
	DELC_MENU,
	/* TRANSLATORS; Change the order of items in a list */
	{"$", N_("Shuffle"), {MC_SHUFFLE,1,{'$'}},KS_NONE},
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	PRYNTTXT_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(c_cfg_km, context_cfg_keys);


struct key context_select_keys[] = 
       {HELP_MENU,
	{"E", N_("ExitSelect"), {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	NULL_MENU,
	/* TRANSLATORS: View this collection */
	{">", "[" N_("View Cltn") "]",
	 {MC_CHOICE, 5, {'v','>','.',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREVC_MENU,
	NEXTC_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	NULL_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(c_sel_km, context_select_keys);


struct key context_fcc_keys[] = 
       {HELP_MENU,
	{"E", N_("ExitSelect"), {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	NULL_MENU,
	{">", "[" N_("View Cltn") "]",
	 {MC_CHOICE, 5, {'v','>','.',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREVC_MENU,
	NEXTC_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	NULL_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(c_fcc_km, context_fcc_keys);


struct key folder_keys[] =
       {HELP_MENU,
  	OTHER_MENU,
	{"<", NULL, {MC_EXIT,2,{'<',','}}, KS_NONE},
        {">", NULL, {MC_CHOICE,2,{'>','.'}}, KS_NONE},
	PREVF_MENU,
	NEXTF_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	/* TRANSLATORS: make an addition, for example add a new folder
	   or a new entry in an address book */
	{"A",N_("Add"),{MC_ADDFLDR,1,{'a'}},KS_NONE},
	DELETE_MENU,
	/* TRANSLATORS: change the name of something */
	{"R",N_("Rename"),{MC_RENAMEFLDR,1,{'r'}}, KS_NONE},
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	QUIT_MENU,
	MAIN_MENU,
	{"V", "[" N_("View Fldr") "]", {MC_OPENFLDR}, KS_NONE},
	GOTO_MENU,
	CIND_MENU,
	COMPOSE_MENU,
	{"%", N_("Print"), {MC_PRINTFLDR,1,{'%'}}, KS_PRINT},
	{"Z", N_("ZoomMode"), {MC_ZOOM,1,{'z'}}, KS_NONE},
	{";",N_("Select"),{MC_SELECT,1,{';'}},KS_SELECT},
	/* TRANSLATORS: Select current item */
	{":",N_("SelectCur"),{MC_SELCUR,1,{':'}},KS_SELECT},

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	{"$", N_("Shuffle"), {MC_SHUFFLE,1,{'$'}},KS_NONE},
	RCOMPOSE_MENU,
	EXPORT_MENU,
	/* TRANSLATORS: Import refers to bringing something in from
	   outside of alpine's normal world */
	{"U", N_("Import"), {MC_IMPORT,1,{'u'}},KS_NONE},
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(folder_km, folder_keys);


struct key folder_sel_keys[] =
       {HELP_MENU,
	{"E", N_("ExitSelect"), {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	NULL_MENU,
	{NULL, NULL, {MC_CHOICE,0}, KS_NONE},
	PREVF_MENU,
	NEXTF_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"S", N_("Select"), {MC_OPENFLDR,1,{'s'}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(folder_sel_km, folder_sel_keys);


struct key folder_sela_keys[] =
       {HELP_MENU,
	{"E", N_("ExitSelect"), {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	NULL_MENU,
	{NULL, NULL, {MC_CHOICE,0}, KS_NONE},
	PREVF_MENU,
	NEXTF_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"S", N_("Select"), {MC_OPENFLDR,1,{'s'}}, KS_NONE},
	NULL_MENU,
	{"A", N_("AddNew"), {MC_ADD,1,{'a'}}, KS_NONE},
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(folder_sela_km, folder_sela_keys);


struct key folder_sub_keys[] =
       {HELP_MENU,
	/* TRANSLATORS: Subscribe to a news group */
	{"S", N_("Subscribe"), {MC_CHOICE,1,{'s'}}, KS_NONE},
	/* TRANSLATORS: Exit Subscribe screen */
  	{"E", N_("ExitSubscb"), {MC_EXIT,1,{'e'}}, KS_EXITMODE},
        {NULL, "[" N_("Select") "]", {MC_CHOICE,3,{'s',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREVF_MENU,
	NEXTF_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	/* TRANSLATORS: List Mode in alpine is where you can select not just
	   one of something but you can select a whole list of something, for
	   example a whole list of addresses to send to. */
	{"L", N_("List Mode"), {MC_LISTMODE, 1, {'l'}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(folder_sub_km, folder_sub_keys);


struct key folder_post_keys[] =
       {HELP_MENU,
 	NULL_MENU,
	{"E", N_("ExitSelect"), {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{"S", "[" N_("Select") "]", {MC_CHOICE, 3, {'s',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREVF_MENU,
	NEXTF_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	NULL_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(folder_post_km, folder_post_keys);


struct key help_keys[] =
       {MAIN_MENU,
	{NULL,NULL,{MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{NULL,NULL,{MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{NULL,NULL,{MC_VIEW_HANDLE,3,{'v',ctrl('m'),ctrl('j')}},KS_NONE},
	{"^B",N_("PrevLink"),{MC_PREV_HANDLE,1,{ctrl('B')}},KS_NONE},
	{"^F",N_("NextLink"),{MC_NEXT_HANDLE,1,{ctrl('F')}},KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	PRYNTMSG_MENU,
	{"Z",N_("Print All"),{MC_PRINTALL,1,{'z'}},KS_NONE},
	NULL_MENU,
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(help_keymenu, help_keys);


struct key rev_msg_keys[] =
       {HELP_MENU,
	OTHER_MENU,
	{"E",N_("Exit Viewer"),{MC_EXIT,1,{'e'}},KS_EXITMODE},
	NULL_MENU,
	{"T",NULL,{MC_TOGGLE,1,{'t'}},KS_NONE},
	{"D",NULL,{MC_JUMP,1,{'d'}},KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU,
	FWDEMAIL_MENU,
	{"S", N_("Save"), {MC_SAVETEXT,1,{'s'}}, KS_SAVE},

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(rev_msg_keymenu, rev_msg_keys);


struct key ans_certfail_keys[] =
       {NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	/* TRANSLATORS: Continue means to keep going. The user is paused to read
	   something and has to tell us to continue when they are finished. */
	{"C","[" N_("Continue") "]",{MC_YES,3,{'c',ctrl('J'),ctrl('M')}},KS_NONE},
	NULL_MENU,
	NULL_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(ans_certfail_keymenu, ans_certfail_keys);


struct key ans_certquery_keys[] =
       {HELP_MENU,
	NULL_MENU,
	{"Y",N_("Yes, continue"),{MC_YES,1,{'y'}},KS_NONE},
	{"D","[" N_("Details") "]",{MC_VIEW_HANDLE,3,{'d',ctrl('M'),ctrl('J')}},KS_NONE},
	{"N",N_("No"),{MC_NO,1,{'n'}},KS_NONE},
	NULL_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU,
	FWDEMAIL_MENU,
	{"S", N_("Save"), {MC_SAVETEXT,1,{'s'}}, KS_SAVE}};
INST_KEY_MENU(ans_certquery_keymenu, ans_certquery_keys);


struct key forge_keys[] =
       {HELP_MENU,
	NULL_MENU,
	{"Y",N_("Yes, continue"),{MC_YES,1,{'y'}},KS_NONE},
	{"N",N_("No"),{MC_NO,1,{'n'}},KS_NONE},
	NULL_MENU,
	NULL_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU,
	FWDEMAIL_MENU,
	{"S", N_("Save"), {MC_SAVETEXT,1,{'s'}}, KS_SAVE}};
INST_KEY_MENU(forge_keymenu, forge_keys);


struct key listmgr_keys[] =
       {HELP_MENU,
	NULL_MENU,
	/* TRANSLATORS: Exit Command List */
	{"E",N_("Exit CmdList"),{MC_EXIT,1,{'e'}},KS_EXITMODE},
	{"Ret","[" N_("Try Command") "]",{MC_VIEW_HANDLE,3,
				{ctrl('m'),ctrl('j'),KEY_RIGHT}},KS_NONE},
	/* TRANSLATORS: go to Previous Command in list */
	{"^B",N_("Prev Cmd"),{MC_PREV_HANDLE,1,{ctrl('B')}},KS_NONE},
	/* TRANSLATORS: go to Next Command in list */
	{"^F",N_("Next Cmd"),{MC_NEXT_HANDLE,1,{ctrl('F')}},KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(listmgr_keymenu, listmgr_keys);


struct key index_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	{"<", NULL, {MC_FOLDERS,2,{'<',','}}, KS_NONE},
	VIEWMSG_MENU,
	PREVMSG_MENU,
	NEXTMSG_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	DELETE_MENU,
	UNDELETE_MENU,
	REPLY_MENU,
	FORWARD_MENU,

	HELP_MENU,
	OTHER_MENU,
	MAIN_MENU,
	QUIT_MENU,
	COMPOSE_MENU,
	GOTO_MENU,
	TAB_MENU,
	WHEREIS_MENU,
	PRYNTMSG_MENU,
	TAKE_MENU,
	SAVE_MENU,
	EXPORT_MENU,

	HELP_MENU,
	OTHER_MENU,
	{"X",NULL,{MC_EXPUNGE,1,{'x'}},KS_NONE},
	/* TRANSLATORS: this stands for unexclude which is the opposite
	   of the exclude command. Exclude eliminates some messages from
	   the view and unexclude gets them back. */
	{"&",N_("unXclude"),{MC_UNEXCLUDE,1,{'&'}},KS_NONE},
	{";",N_("Select"),{MC_SELECT,1,{';'}},KS_SELECT},
	{"A",N_("Apply"),{MC_APPLY,1,{'a'}},KS_APPLY},
	FLDRSORT_MENU,
	JUMP_MENU,
	HDRMODE_MENU,
	BOUNCE_MENU,
	FLAG_MENU,
	PIPE_MENU,

	HELP_MENU,
	OTHER_MENU,
	{":",N_("SelectCur"),{MC_SELCUR,1,{':'}},KS_SELECTCUR},
	{"Z",N_("ZoomMode"),{MC_ZOOM,1,{'z'}},KS_ZOOM},
	LISTFLD_MENU,
	RCOMPOSE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	NULL_MENU,
	/* TRANSLATORS: toggles a collapsed view or an expanded view
	   of a message thread on and off */
	{"/",N_("Collapse/Expand"),{MC_COLLAPSE,1,{'/'}},KS_NONE},
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(index_keymenu, index_keys);


struct key simple_index_keys[] = 
       {HELP_MENU,
	{"E",N_("ExitSelect"),{MC_EXIT,1,{'e'}},KS_EXITMODE},
	NULL_MENU,
	{"S","[" N_("Select") "]",{MC_SELECT,3,{'s',ctrl('M'),ctrl('J')}},KS_SELECT},
	PREVMSG_MENU,
	NEXTMSG_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	DELETE_MENU,
	UNDELETE_MENU,
	WHEREIS_MENU,
	NULL_MENU};
INST_KEY_MENU(simple_index_keymenu, simple_index_keys);


struct key thread_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	/* TRANSLATORS: go to the Folder List */
	{"<", N_("FldrList"), {MC_FOLDERS,2,{'<',','}}, KS_NONE},
	/* TRANSLATORS: View a Thread of messages */
	{">", "[" N_("ViewThd") "]", {MC_VIEW_ENTRY,5,{'v','.','>',ctrl('M'),ctrl('J')}},
								    KS_VIEW},
	/* TRANSLATORS: go to the Previous Thread */
	{"P", N_("PrevThd"), {MC_PREVITEM, 1, {'p'}}, KS_PREVMSG},
	/* TRANSLATORS: go to the Next Thread */
	{"N", N_("NextThd"), {MC_NEXTITEM, 1, {'n'}}, KS_NEXTMSG},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	DELETE_MENU,
	UNDELETE_MENU,
	REPLY_MENU,
	FORWARD_MENU,

	HELP_MENU,
	OTHER_MENU,
	MAIN_MENU,
	QUIT_MENU,
	COMPOSE_MENU,
	GOTO_MENU,
	TAB_MENU,
	WHEREIS_MENU,
	PRYNTMSG_MENU,
	TAKE_MENU,
	SAVE_MENU,
	EXPORT_MENU,

	HELP_MENU,
	OTHER_MENU,
	{"X",NULL,{MC_EXPUNGE,1,{'x'}},KS_NONE},
	{"&",N_("unXclude"),{MC_UNEXCLUDE,1,{'&'}},KS_NONE},
	{";",N_("Select"),{MC_SELECT,1,{';'}},KS_SELECT},
	{"A",N_("Apply"),{MC_APPLY,1,{'a'}},KS_APPLY},
	FLDRSORT_MENU,
	JUMP_MENU,
	HDRMODE_MENU,
	BOUNCE_MENU,
	FLAG_MENU,
	PIPE_MENU,

	HELP_MENU,
	OTHER_MENU,
	{":",N_("SelectCur"),{MC_SELCUR,1,{':'}},KS_SELECTCUR},
	{"Z",N_("ZoomMode"),{MC_ZOOM,1,{'z'}},KS_ZOOM},
	LISTFLD_MENU,
	RCOMPOSE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	NULL_MENU,
	{"/",N_("Collapse/Expand"),{MC_COLLAPSE,1,{'/'}},KS_NONE},
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(thread_keymenu, thread_keys);


struct key att_index_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	{"<",NULL,{MC_EXIT,2,{'<',','}},KS_EXITMODE},
	{">","[" N_("View") "]",{MC_VIEW_ATCH,5,{'v','>','.',ctrl('M'),ctrl('J')}},
	  KS_VIEW},
	/* TRANSLATORS: go to Previous Attachment */
	{"P", N_("PrevAttch"),{MC_PREVITEM,4,{'p',ctrl('B'),ctrl('P'),KEY_UP}},
	  KS_PREVMSG},
	/* TRANSLATORS: go to Next Attachment */
	{"N", N_("NextAtch"),
	 {MC_NEXTITEM, 5, {'n','\t',ctrl('F'),ctrl('N'), KEY_DOWN}},
	 KS_NEXTMSG},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	DELETE_MENU,
	UNDELETE_MENU,
	{"S", N_("Save"), {MC_SAVETEXT,1,{'s'}}, KS_SAVE},
	{NULL, NULL, {MC_EXPORT, 1, {'e'}}, KS_EXPORT},

	HELP_MENU,
	OTHER_MENU,
	MAIN_MENU,
	QUIT_MENU,
	PIPE_MENU,
	BOUNCE_MENU,
	/* TRANSLATORS: About Attachment, a short description of the attachment */
	{"A",N_("AboutAttch"),{MC_ABOUTATCH,1,{'a'}},KS_NONE},
	WHEREIS_MENU,
	{"%", N_("Print"), {MC_PRINTMSG,1,{'%'}}, KS_PRINT},
	INDEX_MENU,
	REPLY_MENU,
	FORWARD_MENU,

	HELP_MENU,
	OTHER_MENU,
	HDRMODE_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(att_index_keymenu, att_index_keys);


struct key att_view_keys[] =
       {HELP_MENU,
	OTHER_MENU,
	{"<",NULL,{MC_EXIT,2,{'<',','}},KS_EXITMODE},
	/* TRANSLATORS: View highlighted URL */
	{"Ret","[" N_("View Hilite") "]",{MC_VIEW_HANDLE,3,
				{ctrl('m'),ctrl('j'),KEY_RIGHT}},KS_NONE},
	/* TRANSLATORS: go to Previous URL */
	{"^B",N_("Prev URL"),{MC_PREV_HANDLE,1,{ctrl('B')}},KS_NONE},
	/* TRANSLATORS: go to Next URL */
	{"^F",N_("Next URL"),{MC_NEXT_HANDLE,1,{ctrl('F')}},KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	DELETE_MENU,
	UNDELETE_MENU,
	SAVE_MENU,
	EXPORT_MENU,

	HELP_MENU,
	OTHER_MENU,
	MAIN_MENU,
	QUIT_MENU,
	PIPE_MENU,
	BOUNCE_MENU,
	HDRMODE_MENU,
	WHEREIS_MENU,
	{"%", N_("Print"), {MC_PRINTMSG,1,{'%'}}, KS_PRINT},
	NULL_MENU,
	REPLY_MENU,
	FORWARD_MENU};
INST_KEY_MENU(att_view_keymenu, att_view_keys);


struct key view_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	/* TRANSLATORS: go to Message Index */
	{"<",N_("MsgIndex"),{MC_INDEX,3,{'i','<',','}},KS_FLDRINDEX},
	/* TRANSLATORS: View the Attachment */
	{">",N_("ViewAttch"),{MC_VIEW_ATCH,3,{'v','>','.'}},KS_NONE},
	PREVMSG_MENU,
	NEXTMSG_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	DELETE_MENU,
	UNDELETE_MENU,
	REPLY_MENU,
	FORWARD_MENU,

	HELP_MENU,
	OTHER_MENU,
	MAIN_MENU,
	QUIT_MENU,
	LISTFLD_MENU,
	GOTO_MENU,
	COMPOSE_MENU,
	WHEREIS_MENU,
	PRYNTMSG_MENU,
	TAKE_MENU,
	SAVE_MENU,
	EXPORT_MENU,

	HELP_MENU,
	OTHER_MENU,
	/* TRANSLATORS: View the highlighted URL */
	{"Ret","[" N_("View Hilite") "]",{MC_VIEW_HANDLE,3,
				{ctrl('m'),ctrl('j'),KEY_RIGHT}},KS_NONE},
	/* TRANSLATORS: Select the current item */
	{":",N_("SelectCur"),{MC_SELCUR,1,{':'}},KS_SELECTCUR},
	/* TRANSLATORS: go to previous URL */
	{"^B",N_("Prev URL"),{MC_PREV_HANDLE,1,{ctrl('B')}},KS_NONE},
	/* TRANSLATORS: go to next URL */
	{"^F",N_("Next URL"),{MC_NEXT_HANDLE,1,{ctrl('F')}},KS_NONE},
	JUMP_MENU,
	TAB_MENU,
	HDRMODE_MENU,
	BOUNCE_MENU,
	FLAG_MENU,
	PIPE_MENU,

	HELP_MENU,
	OTHER_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	RCOMPOSE_MENU,
	{"A",N_("TogglePreferPlain"),{MC_TOGGLE,1,{'a'}},KS_NONE},
#ifdef SMIME
	{"^D","Decrypt", {MC_DECRYPT,1,{ctrl('d')},KS_NONE}},
	{"^E","Security", {MC_SECURITY,1,{ctrl('e')},KS_NONE}},
#else
	NULL_MENU,
	NULL_MENU,
#endif
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(view_keymenu, view_keys);


struct key simple_text_keys[] =
       {HELP_MENU,
	OTHER_MENU,
	{"E",N_("Exit Viewer"),{MC_EXIT,1,{'e'}},KS_EXITMODE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU,
	FWDEMAIL_MENU,
	{"S", N_("Save"), {MC_SAVETEXT,1,{'s'}}, KS_SAVE},

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(simple_text_keymenu, simple_text_keys);


struct key oe_keys[] = 
       {{"^G",N_("Help"),{MC_NONE},KS_SCREENHELP},
	{"^C",N_("Cancel"),{MC_NONE},KS_NONE},
	{"^T","xxx",{MC_NONE},KS_NONE},
	/* TRANSLATORS: The user is entering characters, for example, the
	   name of a folder. Accept means the user is done and wants to
	   accept what is currently displayed. */
	{"Ret",N_("Accept"),{MC_NONE},KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(oe_keymenu, oe_keys);


struct key choose_setup_keys[] =
       {HELP_MENU,
	OTHER_MENU,
	{"E",N_("Exit Setup"),{MC_EXIT,2,{'e',ctrl('C')}},KS_EXITMODE},
	{"P",N_("Printer"),{MC_PRINTER,1,{'p'}},KS_NONE},
	/* TRANSLATORS: Change password */
	{"N",N_("Newpassword"),{MC_PASSWD,1,{'n'}},KS_NONE},
	/* TRANSLATORS: Configure Alpine */
	{"C",N_("Config"),{MC_CONFIG,1,{'c'}},KS_NONE},
	/* TRANSLATORS: Edit signature block */
	{"S",N_("Signature"),{MC_SIG,1,{'s'}},KS_NONE},
	/* TRANSLATORS: configure address books */
	{"A",N_("AddressBooks"),{MC_ABOOKS,1,{'a'}},KS_NONE},
	/* TRANSLATORS: configure collection lists */
	{"L",N_("collectionList"),{MC_CLISTS,1,{'l'}},KS_NONE},
	/* TRANSLATORS: configure rules, an alpine concept */
	{"R",N_("Rules"),{MC_RULES,1,{'r'}},KS_NONE},
	/* TRANSLATORS: configure directory servers */
	{"D",N_("Directory"),{MC_DIRECTORY,1,{'d'}},KS_NONE},
	/* TRANSLATORS: configure color */
	{"K",N_("Kolor"),{MC_KOLOR,1,{'k'}},KS_NONE},

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	/* TRANSLATORS: remote configuration setup */
	{"Z",N_("RemoteConfigSetup"),{MC_REMOTE,1,{'z'}},KS_NONE},
	/* TRANSLATORS: configure S/MIME */
	{"M",N_("S/Mime"),{MC_SECURITY,1,{'m'}},KS_NONE},
	NULL_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(choose_setup_keymenu, choose_setup_keys);


struct key main_keys[] =
       {HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	/* TRANSLATORS: go to Previous Command in list */
	{"P",N_("PrevCmd"),{MC_PREVITEM,3,{'p',ctrl('P'),KEY_UP}},KS_NONE},
	{"N",N_("NextCmd"),{MC_NEXTITEM,3,{'n',ctrl('N'),KEY_DOWN}},KS_NONE},
	NULL_MENU,
	NULL_MENU,
	/* TRANSLATORS: show release notes */
	{"R",N_("RelNotes"),{MC_RELNOTES,1,{'r'}},KS_NONE},
	/* TRANSLATORS: lock keyboard */
	{"K",N_("KBLock"),{MC_KBLOCK,1,{'k'}},KS_NONE},
	NULL_MENU,
	NULL_MENU,

	HELP_MENU,
	OTHER_MENU,
	QUIT_MENU,
	COMPOSE_MENU,
	LISTFLD_MENU,
	GOTO_MENU,
	{"I",N_("Index"),{MC_INDEX,1,{'i'}},KS_FLDRINDEX},
	/* TRANSLATORS: go to the Journal. The Journal shows past
	   messages that alpine has shown the user. */
	{"J",N_("Journal"),{MC_JOURNAL,1,{'j'}},KS_REVIEW},
	/* TRANSLATORS: go to the Setup screen */
	{"S",N_("Setup"),{MC_SETUP,1,{'s'}},KS_NONE},
	/* TRANSLATORS: go to the address book screen */
	{"A",N_("AddrBook"),{MC_ADDRBOOK,1,{'a'}},KS_ADDRBOOK},
	RCOMPOSE_MENU,
	NULL_MENU};
INST_KEY_MENU(main_keymenu, main_keys);


struct key simple_file_keys[] =
       {HELP_MENU,
	OTHER_MENU,
	{"Q",N_("Quit Viewer"),{MC_EXIT,1,{'q'}},KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU,
	FWDEMAIL_MENU,
	{"S", N_("Save"), {MC_SAVETEXT,1,{'s'}}, KS_SAVE},

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(simple_file_keymenu, simple_file_keys);


struct key nuov_keys[] =
       {HELP_MENU,
	OTHER_MENU,
	{"E",NULL,{MC_EXIT,1,{'e',ctrl('M'),ctrl('J')}},KS_NONE},
	/* TRANSLATORS: Alpine asks the user to be counted when they
	   first start using alpine. */
	{"Ret","[" N_("Be Counted!") "]",{MC_VIEW_HANDLE,2,{ctrl('M'),ctrl('J')}},KS_NONE},
	NULL_MENU,
	NULL_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	PRYNTMSG_MENU,
	NULL_MENU,
	/* TRANSLATORS: show release notes */
	{"R",N_("RelNotes"),{MC_RELNOTES,1,{'r'}},KS_NONE},
	NULL_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(nuov_keymenu, nuov_keys);


struct key modal_message_keys[] =
       {NULL_MENU,
	NULL_MENU,
	{"Ret",N_("Finished"),{MC_EXIT,2,{ctrl('m'),ctrl('j')}},KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(modal_message_keymenu, modal_message_keys);


struct key ta_keys_lm[] = 
       {HELP_MENU,
	WHEREIS_MENU,
	TA_EXIT_MENU,
	/* TRANSLATORS: Take this address into the address book */
	{"T", N_("Take"), {MC_TAKE,1,{'t'}}, KS_NONE},
	TA_PREV_MENU,
	TA_NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"X","[" N_("Set/Unset") "]", {MC_CHOICE,3,{'x',ctrl('M'),ctrl('J')}}, KS_NONE},
	{"A", N_("SetAll"),{MC_SELALL,1,{'a'}},KS_NONE},
	{"U",N_("UnSetAll"),{MC_UNSELALL,1,{'u'}},KS_NONE},
	/* TRANSLATORS: The Take Address screen has a Single mode and a
	   List mode. This command causes us to go into Single mode. */
	{"S",N_("SinglMode"),{MC_LISTMODE,1,{'s'}},KS_NONE}};
INST_KEY_MENU(ta_keymenu_lm, ta_keys_lm);


struct key ta_keys_sm[] = 
       {HELP_MENU,
	WHEREIS_MENU,
	TA_EXIT_MENU,
	{"T","[" N_("Take") "]",{MC_TAKE,3,{'t',ctrl('M'),ctrl('J')}}, KS_NONE},
	TA_PREV_MENU,
	TA_NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	/* TRANSLATORS: The Take Address screen has a Single mode and a
	   List mode. This command causes us to go into List mode. */
	{"L",N_("ListMode"),{MC_LISTMODE,1,{'l'}},KS_NONE}};
INST_KEY_MENU(ta_keymenu_sm, ta_keys_sm);


struct key pipe_cancel_keys[] = 
       {NULL_MENU,
	{"^C",N_("Stop Waiting"),{MC_NONE},KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(pipe_cancel_keymenu, pipe_cancel_keys);


struct key color_pattern_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	/* TRANSLATORS: Change Value */
	{"C", "[" N_("Change Val") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	/* TRANSLATORS: Delete Value */
	{"D", N_("Delete Val"), {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(color_pattern_keymenu, color_pattern_keys);


struct key hdr_color_checkbox_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	EXIT_SETUP_MENU,
	TOGGLEB_MENU,
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(hdr_color_checkbox_keymenu, hdr_color_checkbox_keys);


struct key kw_color_checkbox_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	EXIT_SETUP_MENU,
	TOGGLEC_MENU,
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(kw_color_checkbox_keymenu, kw_color_checkbox_keys);


struct key selectable_bold_checkbox_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	EXIT_SETUP_MENU,
	TOGGLED_MENU,
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(selectable_bold_checkbox_keymenu, selectable_bold_checkbox_keys);


struct key flag_keys[] = 
       {HELP_MENU,
        {"A", N_("Add KW"), {MC_ADD,1,{'a'}}, KS_NONE},
	/* TRANSLATORS: Exit from the Flags screen */
        {"E", N_("Exit Flags"), {MC_EXIT,1,{'e'}}, KS_EXITMODE},
        TOGGLE_MENU,
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(flag_keymenu, flag_keys);


struct key addr_select_keys[] = 
       {HELP_MENU,
        {"E", N_("ExitSelect"), {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	NULL_MENU,
        {"S", "[" N_("Select") "]", {MC_CHOICE,3,{'s',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	NULL_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(addr_s_km, addr_select_keys);


struct key addr_select_with_goback_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	/* TRANSLATORS: go to address book list */
        {"<", N_("AddressBkList"), {MC_ADDRBOOK,2,{'<',','}}, KS_NONE},
        {"S", "[" N_("Select") "]", {MC_CHOICE,3,{'s',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
        {"E", N_("ExitSelect"), {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	WHEREIS_MENU};
INST_KEY_MENU(addr_s_km_with_goback, addr_select_with_goback_keys);

static struct key addr_select_with_view_keys[] = 
       {HELP_MENU,
	RCOMPOSE_MENU,
        {"<", N_("AddressBkList"), {MC_ADDRBOOK,2,{'<',','}}, KS_NONE},
        {">", "[" N_("View") "]",
	   {MC_VIEW_TEXT,5,{'v','>','.',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	/* TRANSLATORS: compose a message to the current address */
        {"C", N_("ComposeTo"), {MC_COMPOSE,1,{'c'}}, KS_COMPOSER},
	FWDEMAIL_MENU,
	SAVE_MENU,
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(addr_s_km_with_view, addr_select_with_view_keys);


struct key addr_select_for_url_keys[] = 
       {HELP_MENU,
	RCOMPOSE_MENU,
        {"<", N_("Exit Viewer"), {MC_ADDRBOOK,3,{'<',',','e'}}, KS_NONE},
        {">", "[" N_("View") "]",
	   {MC_VIEW_TEXT,5,{'v','>','.',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
        {"C", N_("ComposeTo"), {MC_COMPOSE,1,{'c'}}, KS_COMPOSER},
	FWDEMAIL_MENU,
	SAVE_MENU,
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(addr_s_km_for_url, addr_select_for_url_keys);


struct key addr_select_exit_keys[] = 
       {NULL_MENU,
	NULL_MENU,
        {"E", "[" N_("Exit") "]", {MC_EXIT,3,{'e',ctrl('M'),ctrl('J')}},
	   KS_EXITMODE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(addr_s_km_exit, addr_select_exit_keys);


struct key addr_select_goback_keys[] = 
       {NULL_MENU,
	NULL_MENU,
        {"E", "[" N_("Exit") "]", {MC_ADDRBOOK,3,{'e',ctrl('M'),ctrl('J')}},
	   KS_EXITMODE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU};
INST_KEY_MENU(addr_s_km_goback, addr_select_goback_keys);


struct key config_text_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[" N_("Change Val") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", N_("Add Value"), {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", N_("Delete Val"), {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(config_text_keymenu, config_text_keys);


struct key config_text_to_charsets_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[" N_("Change Val") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", N_("Add Value"), {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", N_("Delete Val"), {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	/* TRANSLATORS: go to list of keywords */
	{"T", N_("ToCharsets"), {MC_CHOICE, 2, {'t', ctrl('T')}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(config_text_to_charsets_keymenu, config_text_to_charsets_keys);


struct key direct_config_keys[] = 
       {HELP_MENU,
	NULL_MENU,
        {"E", N_("Exit Setup"), {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{"C", "[" N_("Change") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	/* TRANSLATORS: go to previous LDAP directory server in the list */
	{"P", N_("PrevDir"), {MC_PREVITEM, 1, {'p'}}, KS_NONE},
	{"N", N_("NextDir"), {MC_NEXTITEM, 2, {'n', TAB}}, KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	/* TRANSLATORS: add a directory server to configuration */
	{"A", N_("Add Dir"), {MC_ADD,1,{'a'}}, KS_NONE},
	/* TRANSLATORS: delete a directory */
	{"D", N_("Del Dir"), {MC_DELETE,1,{'d'}}, KS_NONE},
	{"$", N_("Shuffle"), {MC_SHUFFLE,1,{'$'}}, KS_NONE},
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(dir_conf_km, direct_config_keys);


struct key sel_from_list_keys[] = 
       {HELP_MENU,
	NULL_MENU,
        {"E", N_("Exit"),     {MC_EXIT,1,{'e'}}, KS_EXITMODE},
        {"S", "[" N_("Select") "]", {MC_SELECT,3,{'s',ctrl('J'),ctrl('M')}}, KS_NONE},
	{"P", N_("Prev"), {MC_PREVITEM, 1, {'p'}}, KS_NONE},
	{"N", N_("Next"), {MC_NEXTITEM, 2, {'n', TAB}}, KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(sel_from_list, sel_from_list_keys);


struct key sel_from_list_keys_ctrlc[] = 
       {HELP_MENU,
	NULL_MENU,
        {"^C", N_("exit"),     {MC_EXIT,1,{ctrl('C')}}, KS_EXITMODE},
        {"S", "[" N_("Select") "]", {MC_SELECT,3,{'s',ctrl('J'),ctrl('M')}}, KS_NONE},
	{"P", N_("Prev"), {MC_PREVITEM, 1, {'p'}}, KS_NONE},
	{"N", N_("Next"), {MC_NEXTITEM, 2, {'n', TAB}}, KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(sel_from_list_ctrlc, sel_from_list_keys_ctrlc);


struct key sel_from_list_keys_sm[] = 
       {HELP_MENU,
	OTHER_MENU,
        {"E", N_("Exit"),     {MC_EXIT,1,{'e'}}, KS_EXITMODE},
        {"S", "[" N_("Select") "]", {MC_SELECT,3,{'s',ctrl('J'),ctrl('M')}}, KS_NONE},
	{"P", N_("Prev"), {MC_PREVITEM, 1, {'p'}}, KS_NONE},
	{"N", N_("Next"), {MC_NEXTITEM, 2, {'n', TAB}}, KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	{"L",N_("ListMode"),{MC_LISTMODE,1,{'l'}},KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(sel_from_list_sm, sel_from_list_keys_sm);


struct key sel_from_list_keys_sm_ctrlc[] = 
       {HELP_MENU,
	OTHER_MENU,
        {"^C", N_("exit"),     {MC_EXIT,1,{ctrl('C')}}, KS_EXITMODE},
        {"S", "[" N_("Select") "]", {MC_SELECT,3,{'s',ctrl('J'),ctrl('M')}}, KS_NONE},
	{"P", N_("Prev"), {MC_PREVITEM, 1, {'p'}}, KS_NONE},
	{"N", N_("Next"), {MC_NEXTITEM, 2, {'n', TAB}}, KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	{"L",N_("ListMode"),{MC_LISTMODE,1,{'l'}},KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(sel_from_list_sm_ctrlc, sel_from_list_keys_sm_ctrlc);


struct key sel_from_list_keys_lm[] = 
       {HELP_MENU,
	OTHER_MENU,
        {"E", N_("Exit"),     {MC_EXIT,1,{'e'}}, KS_EXITMODE},
        {"S", "[" N_("Select") "]", {MC_SELECT,3,{'s',ctrl('J'),ctrl('M')}}, KS_NONE},
	{"P", N_("Prev"), {MC_PREVITEM, 1, {'p'}}, KS_NONE},
	{"N", N_("Next"), {MC_NEXTITEM, 2, {'n', TAB}}, KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"X", N_("Set/Unset"), {MC_TOGGLE,1,{'x'}}, KS_NONE},
	{"1",N_("SinglMode"),{MC_LISTMODE,1,{'1'}},KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(sel_from_list_lm, sel_from_list_keys_lm);


struct key sel_from_list_keys_lm_ctrlc[] = 
       {HELP_MENU,
	OTHER_MENU,
        {"^C", N_("exit"),     {MC_EXIT,1,{ctrl('C')}}, KS_EXITMODE},
        {"S", "[" N_("Select") "]", {MC_SELECT,3,{'s',ctrl('J'),ctrl('M')}}, KS_NONE},
	{"P", N_("Prev"), {MC_PREVITEM, 1, {'p'}}, KS_NONE},
	{"N", N_("Next"), {MC_NEXTITEM, 2, {'n', TAB}}, KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"X", N_("Set/Unset"), {MC_TOGGLE,1,{'x'}}, KS_NONE},
	{"1",N_("SinglMode"),{MC_LISTMODE,1,{'1'}},KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(sel_from_list_lm_ctrlc, sel_from_list_keys_lm_ctrlc);


struct key sel_from_list_keys_olm[] = 
       {HELP_MENU,
	OTHER_MENU,
        {"E", N_("Exit"),     {MC_EXIT,1,{'e'}}, KS_EXITMODE},
        {"S", "[" N_("Select") "]", {MC_SELECT,3,{'s',ctrl('J'),ctrl('M')}}, KS_NONE},
	{"P", N_("Prev"), {MC_PREVITEM, 1, {'p'}}, KS_NONE},
	{"N", N_("Next"), {MC_NEXTITEM, 2, {'n', TAB}}, KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"X", N_("Set/Unset"), {MC_TOGGLE,1,{'x'}}, KS_NONE},
	NULL_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(sel_from_list_olm, sel_from_list_keys_olm);


struct key sel_from_list_keys_olm_ctrlc[] = 
       {HELP_MENU,
	OTHER_MENU,
        {"^C", N_("exit"),     {MC_EXIT,1,{ctrl('C')}}, KS_EXITMODE},
        {"S", "[" N_("Select") "]", {MC_SELECT,3,{'s',ctrl('J'),ctrl('M')}}, KS_NONE},
	{"P", N_("Prev"), {MC_PREVITEM, 1, {'p'}}, KS_NONE},
	{"N", N_("Next"), {MC_NEXTITEM, 2, {'n', TAB}}, KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"X", N_("Set/Unset"), {MC_TOGGLE,1,{'x'}}, KS_NONE},
	NULL_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(sel_from_list_olm_ctrlc, sel_from_list_keys_olm_ctrlc);


#ifndef	DOS

struct key printer_edit_keys[] = 
       {HELP_MENU,
	PRYNTTXT_MENU,
	EXIT_SETUP_MENU,
	{"S", "[" N_("Select") "]", {MC_CHOICE,3,{'s',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	/* TRANSLATORS: add a printer to configuration */
	{"A", N_("Add Printer"), {MC_ADD,1,{'a'}}, KS_NONE},
	/* TRANSLATORS: delete a printer from configuration */
	{"D", N_("DeletePrint"), {MC_DELETE,1,{'d'}}, KS_NONE},
	{"C", N_("Change"), {MC_EDIT,1,{'c'}}, KS_NONE},
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(printer_edit_keymenu, printer_edit_keys);


struct key printer_select_keys[] = 
       {HELP_MENU,
	PRYNTTXT_MENU,
	EXIT_SETUP_MENU,
	{"S", "[" N_("Select") "]", {MC_CHOICE,3,{'s',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	NULL_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(printer_select_keymenu, printer_select_keys);

#endif	/* !DOS */


struct key role_select_keys[] = 
       {HELP_MENU,
	NULL_MENU,
        {"E", N_("Exit"), {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	NULL_MENU,
	/* TRANSLATORS: go to previous Role in list */
	{"P", N_("PrevRole"), {MC_PREVITEM, 1, {'p'}}, KS_NONE},
	{"N", N_("NextRole"), {MC_NEXTITEM, 2, {'n', TAB}}, KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	{"D", "", {MC_TOGGLE, 1, {'d'}}, KS_NONE},
	WHEREIS_MENU};
INST_KEY_MENU(role_select_km, role_select_keys);


struct key role_config_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
        {"E", N_("Exit Setup"), {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{"C", "[" N_("Change") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	/* TRANSLATORS: go to previous Rule in list */
	{"P", N_("PrevRule"), {MC_PREVITEM, 1, {'p'}}, KS_NONE},
	{"N", N_("NextRule"), {MC_NEXTITEM, 2, {'n', TAB}}, KS_NONE},
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", N_("Add"), {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", N_("Delete"), {MC_DELETE,1,{'d'}}, KS_NONE},
	{"$", N_("Shuffle"), {MC_SHUFFLE,1,{'$'}}, KS_NONE},
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
        NULL_MENU,
        NULL_MENU,
	/* TRANSLATORS: Include a File from filesystem */
	{"I", N_("IncludeFile"), {MC_ADDFILE,1,{'i'}}, KS_NONE},
	{"X", N_("eXcludeFile"), {MC_DELFILE,1,{'x'}}, KS_NONE},
        NULL_MENU,
        NULL_MENU,
	{"R", N_("Replicate"), {MC_COPY,1,{'r'}}, KS_NONE},
        NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(role_conf_km, role_config_keys);


struct key config_text_wshuf_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[" N_("Change Val") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", N_("Add Value"), {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", N_("Delete Val"), {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	{"$", N_("Shuffle"), {MC_SHUFFLE,1,{'$'}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(config_text_wshuf_keymenu, config_text_wshuf_keys);


struct key config_text_wshufandfldr_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[" N_("Change Val") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", N_("Add Value"), {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", N_("Delete Val"), {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	{"$", N_("Shuffle"), {MC_SHUFFLE,1,{'$'}}, KS_NONE},
	{"T", N_("ToFldrs"), {MC_CHOICE,2,{'t', ctrl('T')}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(config_text_wshufandfldr_keymenu, config_text_wshufandfldr_keys);


struct key config_role_file_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[" N_("Change Val") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", N_("Add Value"), {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", N_("Delete Val"), {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	/* TRANSLATORS: go to list of Files */
	{"T", N_("ToFiles"), {MC_CHOICE, 2, {'t', ctrl('T')}}, KS_NONE},
	/* TRANSLATORS: edit a file */
	{"F", N_("editFile"), {MC_EDITFILE, 1, {'f'}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(config_role_file_keymenu, config_role_file_keys);


struct key config_role_file_res_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[" N_("Change Val") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", N_("Add Value"), {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", N_("Delete Val"), {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	{"T", N_("ToFiles"), {MC_CHOICE, 2, {'t', ctrl('T')}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(config_role_file_res_keymenu, config_role_file_res_keys);


struct key config_role_keyword_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[" N_("Change Val") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", N_("Add Value"), {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", N_("Delete Val"), {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	/* TRANSLATORS: go to list of keywords */
	{"T", N_("ToKeywords"), {MC_CHOICE, 2, {'t', ctrl('T')}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(config_role_keyword_keymenu, config_role_keyword_keys);


struct key config_role_keyword_keys_not[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[" N_("Change Val") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", N_("Add Value"), {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", N_("Delete Val"), {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	{"T", N_("ToKeywords"), {MC_CHOICE, 2, {'t', ctrl('T')}}, KS_NONE},
	NULL_MENU,
	/* TRANSLATORS: toggle between NOT and not NOT, turn NOT on or off */
	{"!", N_("toggle NOT"), {MC_NOT,1,{'!'}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(config_role_keyword_keymenu_not, config_role_keyword_keys_not);


struct key config_role_charset_keys_not[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[" N_("Change Val") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", N_("Add Value"), {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", N_("Delete Val"), {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	/* TRANSLATORS: go to list of character sets */
	{"T", N_("ToCharSets"), {MC_CHOICE, 2, {'t', ctrl('T')}}, KS_NONE},
	NULL_MENU,
	{"!", N_("toggle NOT"), {MC_NOT,1,{'!'}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(config_role_charset_keymenu_not, config_role_charset_keys_not);


struct key config_role_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[" N_("Change Val") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", N_("Add Value"), {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", N_("Delete Val"), {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(config_role_keymenu, config_role_keys);


struct key config_role_keys_not[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[" N_("Change Val") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", N_("Add Value"), {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", N_("Delete Val"), {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	/* TRANSLATORS: add extra headers to list */
	{"X", N_("eXtraHdr"), {MC_ADDHDR, 1, {'x'}}, KS_NONE},
	{"!", N_("toggle NOT"), {MC_NOT,1,{'!'}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(config_role_keymenu_not, config_role_keys_not);


struct key config_role_keys_extra[] = 
       {HELP_MENU,
	NULL_MENU,
	EXIT_SETUP_MENU,
	{"X", "[" N_("eXtraHdr") "]", {MC_ADDHDR, 3,{'x',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(config_role_keymenu_extra, config_role_keys_extra);


struct key config_role_addr_pat_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[" N_("Change Val") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", N_("Add Value"), {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", N_("Delete Val"), {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	/* TRANSLATORS: go to address book to get address */
	{"T", N_("ToAddrBk"), {MC_CHOICEB, 2, {'t', ctrl('T')}}, KS_NONE},
	{"X", N_("eXtraHdr"), {MC_ADDHDR, 1, {'x'}}, KS_NONE},
	{"!", N_("toggle NOT"), {MC_NOT,1,{'!'}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(config_role_addr_pat_keymenu, config_role_addr_pat_keys);


struct key config_role_xtrahdr_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[" N_("Change Val") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", N_("Add Value"), {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", N_("Delete Val"), {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	{"X", N_("eXtraHdr"), {MC_ADDHDR, 1, {'x'}}, KS_NONE},
	{"!", N_("toggle NOT"), {MC_NOT,1,{'!'}}, KS_NONE},
	NULL_MENU,
	/* TRANSLATORS: remove a header we previously added */
	{"R", N_("RemoveHdr"), {MC_DELHDR, 1, {'r'}}, KS_NONE},
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(config_role_xtrahdr_keymenu, config_role_xtrahdr_keys);


struct key config_role_addr_act_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[" N_("Change Val") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", N_("Add Value"), {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", N_("Delete Val"), {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	{"T", N_("ToAddrBk"), {MC_CHOICEC, 2, {'t', ctrl('T')}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(config_role_addr_act_keymenu, config_role_addr_act_keys);


struct key config_role_patfolder_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[" N_("Change Val") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", N_("Add Value"), {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", N_("Delete Val"), {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	/* TRANSLATORS: go to list of folders */
	{"T", N_("ToFldrs"), {MC_CHOICED, 2, {'t', ctrl('T')}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(config_role_patfolder_keymenu, config_role_patfolder_keys);


struct key config_role_actionfolder_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[" N_("Change Val") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", N_("Add Value"), {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", N_("Delete Val"), {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	{"T", N_("ToFldrs"), {MC_CHOICEE, 2, {'t', ctrl('T')}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(config_role_actionfolder_keymenu, config_role_actionfolder_keys);


struct key config_role_inick_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[" N_("Change Val") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", N_("Add Value"), {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", N_("Delete Val"), {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	/* TRANSLATORS: go to list of nicknames */
	{"T", N_("ToNicks"), {MC_CHOICE, 2, {'t', ctrl('T')}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(config_role_inick_keymenu, config_role_inick_keys);


struct key config_role_afrom_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[" N_("Change Val") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", N_("Add Value"), {MC_ADD,1,{'a'}}, KS_NONE},
	{"D", N_("Delete Val"), {MC_DELETE,1,{'d'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	/* TRANSLATORS: go to list of address books */
	{"T", N_("ToAbookList"), {MC_CHOICE, 2, {'t', ctrl('T')}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(config_role_afrom_keymenu, config_role_afrom_keys);


struct key config_checkbox_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	EXIT_SETUP_MENU,
	TOGGLE_MENU,
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(config_checkbox_keymenu, config_checkbox_keys);


struct key config_radiobutton_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	EXIT_SETUP_MENU,
	{"*", "[" N_("Select") "]", {MC_CHOICE,3,{'*',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(config_radiobutton_keymenu, config_radiobutton_keys);


struct key config_yesno_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	EXIT_SETUP_MENU,
	{"C", "[" N_("Change") "]", {MC_TOGGLE,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(config_yesno_keymenu, config_yesno_keys);


struct key color_changing_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	{"E", N_("To Colors"), {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{"*", "[" N_("Select") "]", {MC_CHOICE,3,{'*',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(color_changing_keymenu, color_changing_keys);


struct key custom_color_changing_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	/* TRANSLATORS: go to color configuration screen */
	{"E", N_("To Colors"), {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{"*", "[" N_("Select") "]", {MC_CHOICEB,3,{'*',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(custom_color_changing_keymenu, custom_color_changing_keys);


struct key kw_color_changing_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	{"E", N_("To Colors"), {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{"*", "[" N_("Select") "]", {MC_CHOICEC,3,{'*',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(kw_color_changing_keymenu, kw_color_changing_keys);


#ifdef	_WINDOWS

struct key color_rgb_changing_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	{"E", N_("To Colors"), {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{"*", "[" N_("Select") "]", {MC_CHOICE,3,{'*',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"C", N_("Customize"), {MC_RGB1,1,{'c'}},KS_NONE},
	NULL_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(color_rgb_keymenu, color_rgb_changing_keys);


struct key custom_rgb_changing_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	{"E", N_("To Colors"), {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{"*", "[" N_("Select") "]", {MC_CHOICEB,3,{'*',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"C", N_("Customize"), {MC_RGB2,1,{'c'}},KS_NONE},
	NULL_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(custom_rgb_keymenu, custom_rgb_changing_keys);


struct key kw_rgb_changing_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	{"E", N_("To Colors"), {MC_EXIT,1,{'e'}}, KS_EXITMODE},
	{"*", "[" N_("Select") "]", {MC_CHOICEC,3,{'*',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"C", N_("Customize"), {MC_RGB3,1,{'c'}},KS_NONE},
	NULL_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(kw_rgb_keymenu, kw_rgb_changing_keys);

#endif


struct key color_setting_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[" N_("Change") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", N_("AddHeader"), {MC_ADD,1,{'a'}}, KS_NONE},
	/* TRANSLATORS: restore defaults */
	{"R", N_("RestoreDefs"), {MC_DEFAULT,1,{'r'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(color_setting_keymenu, color_setting_keys);


struct key custom_color_setting_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[" N_("Change") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", N_("AddHeader"), {MC_ADD,1,{'a'}}, KS_NONE},
	{"R", N_("RestoreDefs"), {MC_DEFAULT,1,{'r'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

        HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	{"D", N_("DeleteHdr"), {MC_DELETE,1,{'d'}}, KS_NONE},
	/* TRANSLATORS: shuffle headers (change the order of headers) */
	{"$", N_("ShuffleHdr"), {MC_SHUFFLE,1,{'$'}}, KS_NONE},
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(custom_color_setting_keymenu, custom_color_setting_keys);


struct key role_color_setting_keys[] = 
       {HELP_MENU,
	NULL_MENU,
	EXIT_SETUP_MENU,
	{"*", "[" N_("Select") "]", {MC_CHOICE,3,{'*',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU,
	PRYNTTXT_MENU,
	WHEREIS_MENU};
INST_KEY_MENU(role_color_setting_keymenu, role_color_setting_keys);


struct key kw_color_setting_keys[] = 
       {HELP_MENU,
	OTHER_MENU,
	EXIT_SETUP_MENU,
	{"C", "[" N_("Change") "]", {MC_EDIT,3,{'c',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"A", N_("AddHeader"), {MC_ADD,1,{'a'}}, KS_NONE},
	{"R", N_("RestoreDefs"), {MC_DEFAULT,1,{'r'}}, KS_NONE},
	PRYNTTXT_MENU,
	WHEREIS_MENU,

	HELP_MENU,
	OTHER_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(kw_color_setting_keymenu, kw_color_setting_keys);


struct key take_export_keys_sm[] = 
       {HELP_MENU,
	WHEREIS_MENU,
	/* TRANSLATORS: exit the Take Address screen */
	{"<",N_("ExitTake"), {MC_EXIT,4,{'e',ctrl('C'),'<',','}}, KS_EXITMODE},
	{"T","[" N_("Take") "]",{MC_TAKE,3,{'t',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	NULL_MENU,
	{"L",N_("ListMode"),{MC_LISTMODE,1,{'l'}},KS_NONE}};
INST_KEY_MENU(take_export_keymenu_sm, take_export_keys_sm);


struct key take_export_keys_lm[] = 
       {HELP_MENU,
	WHEREIS_MENU,
	{"<",N_("ExitTake"), {MC_EXIT,4,{'e',ctrl('C'),'<',','}}, KS_EXITMODE},
	{"T",N_("Take"), {MC_TAKE,1,{'t'}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	{"X","[" N_("Set/Unset") "]", {MC_CHOICE,3,{'x',ctrl('M'),ctrl('J')}}, KS_NONE},
	{"A", N_("SetAll"),{MC_SELALL,1,{'a'}},KS_NONE},
	{"U",N_("UnSetAll"),{MC_UNSELALL,1,{'u'}},KS_NONE},
	{"S",N_("SinglMode"),{MC_LISTMODE,1,{'s'}},KS_NONE}};
INST_KEY_MENU(take_export_keymenu_lm, take_export_keys_lm);


struct key smime_info_keys[] = 
       {HELP_MENU,
    	OTHER_MENU,
	{"<","Back",{MC_VIEW_TEXT,2,{'<',','}},KS_EXITMODE},
    	NULL_MENU,
    	NULL_MENU,
    	NULL_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
    	NULL_MENU,
    	NULL_MENU,
    	NULL_MENU,
    	NULL_MENU,

	HELP_MENU,
	OTHER_MENU,
	MAIN_MENU,
	QUIT_MENU,
    	NULL_MENU,
    	NULL_MENU,
    	NULL_MENU,
    	NULL_MENU,
    	NULL_MENU,
	INDEX_MENU,
    	NULL_MENU,
    	NULL_MENU};
INST_KEY_MENU(smime_info_keymenu, smime_info_keys);


struct key config_smime_helper_keys[] = 
       {HELP_MENU,
	WHEREIS_MENU,
	EXIT_SETUP_MENU,
	{"T","[" N_("Transfer") "]", {MC_CHOICE,3,{'t',ctrl('M'),ctrl('J')}}, KS_NONE},
	PREV_MENU,
	NEXT_MENU,
	PREVPAGE_MENU,
	NEXTPAGE_MENU,
	NULL_MENU,
	NULL_MENU,
	HOMEKEY_MENU,
	ENDKEY_MENU};
INST_KEY_MENU(config_smime_helper_keymenu, config_smime_helper_keys);


/*
 * Internal prototypes
 */
void  output_keymenu(struct key_menu *, bitmap_t, int, int);
void  format_keymenu(struct key_menu *, bitmap_t, int);
void  menu_clear_cmd_binding(struct key_menu *, int);
#ifdef	MOUSE
void  print_inverted_label(int, MENUITEM *);
#endif


/* Saved key menu drawing state */
static struct {
    struct key_menu *km;
    int              row,
		     column,
                     blanked;
    bitmap_t         bitmap;
} km_state;


/*
 * Longest label that can be displayed in keymenu
 */
#define	MAX_LABEL	40
#define MAX_KEYNAME	 3
static struct key last_time_buf[12];
static int keymenu_is_dirty = 1;

void
mark_keymenu_dirty(void)
{
    keymenu_is_dirty = 1;
}


/*
 * Write an already formatted key_menu to the screen
 *
 * Args: km     -- key_menu structure
 *       bm     -- bitmap, 0's mean don't draw this key
 *       row    -- the row on the screen to begin on, negative values
 *                 are counted from the bottom of the screen up
 *       column -- column on the screen to begin on
 *
 * The bits in the bitmap are used from least significant to most significant,
 * not left to right.  So, if you write out the bitmap in the normal way, for
 * example,
 * bm[0] = 0x5, bm[1] = 0x8, bm[2] = 0x21, bm[3] = bm[4] = bm[5] = 0
 *   0000 0101    0000 1000    0010 0001   ...
 * means that menu item 0 (first row, first column) is set, item 1 (2nd row,
 * first column) is not set, item 2 is set, items 3-10 are not set, item 11
 * (2nd row, 6th and last column) is set.  In the second menu (the second set
 * of 12 bits) items 0-3 are unset, 4 is set, 5-8 unset, 9 set, 10-11 unset.
 * That uses up bm[0] - bm[2].
 * Just to make sure, here it is drawn out for the first set of 12 items in
 * the first keymenu (0-11)
 *    bm[0] x x x x  x x x x   bm[1] x x x x  x x x x
 *          7 6 5 4  3 2 1 0                 1110 9 8
 */
void
output_keymenu(struct key_menu *km, unsigned char *bm, int row, int column)
{
    register struct key *k;
    struct key          *last_time;
    int                  i, j,
			 ufk,        /* using function keys */
			 real_row,
			 max_column, /* number of columns on screen */
			 off;        /* offset into keymap */
    struct variable *vars = ps_global->vars;
    COLOR_PAIR          *lastc=NULL, *label_color=NULL, *name_color=NULL;
#ifdef	MOUSE
			/* 6's are for UTF-8 */
    char		 keystr[6*MAX_KEYNAME + 6*MAX_LABEL + 2];
#endif

    off    	  = km->which * 12;
    max_column    = ps_global->ttyo->screen_cols;

    if((ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global)) < 0
       || max_column <= 0){
	keymenu_is_dirty = 1;
	return;
    }


    real_row = row > 0 ? row : ps_global->ttyo->screen_rows + row;

    if(pico_usingcolor()){
	lastc = pico_get_cur_color();
	if(lastc && VAR_KEYLABEL_FORE_COLOR && VAR_KEYLABEL_BACK_COLOR &&
	   pico_is_good_color(VAR_KEYLABEL_FORE_COLOR) &&
	   pico_is_good_color(VAR_KEYLABEL_BACK_COLOR)){
	    label_color = new_color_pair(VAR_KEYLABEL_FORE_COLOR,
					 VAR_KEYLABEL_BACK_COLOR);
	    if(label_color)
	      (void)pico_set_colorp(label_color, PSC_NONE);
	}

	if(label_color && VAR_KEYNAME_FORE_COLOR && VAR_KEYNAME_BACK_COLOR &&
	   pico_is_good_color(VAR_KEYNAME_FORE_COLOR) &&
	   pico_is_good_color(VAR_KEYNAME_BACK_COLOR)){
	    name_color = new_color_pair(VAR_KEYNAME_FORE_COLOR,
					VAR_KEYNAME_BACK_COLOR);
	}
    }

    if(keymenu_is_dirty){
	ClearLines(real_row, real_row+1);
	keymenu_is_dirty = 0;
	/* first time through, set up storage */
	if(!last_time_buf[0].name){
	    for(i = 0; i < 12; i++){
		last_time = &last_time_buf[i];
		last_time->name  = (char *) fs_get(6*MAX_KEYNAME + 1);
		last_time->label = (char *) fs_get(6*MAX_LABEL + 1);
	    }
	}

	for(i = 0; i < 12; i++)
	  last_time_buf[i].column = -1;
    }

    for(i = 0; i < 12; i++){
	int e;

	e = off + i;
        dprint((9, "%2d %-7.7s %-10.10s %d\n", i,
		   km == NULL ? "(no km)" 
		   : km->keys[e].name == NULL ? "(null)" 
		   : km->keys[e].name, 		   
		   km == NULL ? "(no km)" 
		   : km->keys[e].label == NULL ? "(null)" 
		   : km->keys[e].label, km ? km->keys[e].column : 0));
#ifdef	MOUSE
	register_key(i, NO_OP_COMMAND, "", NULL, 0, 0, 0, NULL, NULL);
#endif
    }

    ufk = F_ON(F_USE_FK, ps_global);
    dprint((9, "row: %d, real_row: %d, column: %d\n", row, 
               real_row, column));

    for(i = 0; i < 2; i++){
	int   c, el, empty, fkey, last_in_row, fix_start;
	short next_col;
	char  temp[6*MAX_SCREEN_COLS+1];
	char  temp2[6*MAX_SCREEN_COLS+1];
	char  this_label[6*MAX_LABEL+1];

	j = 6*i - 1;
	if(i == 1 && !label_color)
	  max_column--;  /* Some terminals scroll if you write in the
			    lower right hand corner. If user has a
			    label_color set we'll take our chances.
			    Otherwise, we'd get one cell of Normal. */

	/*
	 * k is the key struct we're working on
	 * c is the column number
	 * el is an index into the whole keys array
	 * Last_time_buf is ordered strangely. It goes row by row instead
	 * of down each column like km does. J is an index into it.
	 */
        for(c = 0, el = off+i, k = &km->keys[el];
	    k < &km->keys[off+12] && c < max_column;
	    k += 2, el += 2){

            if(k->column > max_column)
              break;

	    j++;
            if(ufk)
              fkey = 1 + k - &km->keys[off];

	    empty     = (!bitnset(el,bm) || !(k->name && *k->name));
	    last_time = &last_time_buf[j];
	    if(k+2 < &km->keys[off+12]){
	        last_in_row = 0;
		next_col    = last_time_buf[j+1].column;
		fix_start = (k == &km->keys[off] ||
			     k == &km->keys[off+1]) ? k->column : 0;
	    }
	    else{
		last_in_row = 1;
		fix_start = 0;
	    }

	    /*
	     * Make sure there is a space between this label and
	     * the next name. That is, we prefer a space to the
	     * extra character of the label because the space
	     * separates the commands and looks nicer.
	     */
	    if(k->label){
		size_t l;
		char tmp_label[6*MAX_LABEL+1];

		if(k->label[0] == '[' && k->label[(l=strlen(k->label))-1] == ']' && l > 2){
		    /*
		     * Can't write in k->label, which might be a constant array.
		     */
		    strncpy(tmp_label, &k->label[1], MIN(sizeof(tmp_label),l-2));
		    tmp_label[MIN(sizeof(tmp_label)-1,l-2)] = '\0';

		    snprintf(this_label, sizeof(this_label), "[%s]", _(tmp_label));
		}
		else
		  strncpy(this_label, _(k->label), sizeof(this_label));

		this_label[sizeof(this_label)-1] = '\0';
		if(!last_in_row){
		    int trunc;

		    trunc = (k+2)->column - k->column
				     - ((k->name ? utf8_width(k->name) : 0) + 1);
		    /*
		     * trunc columns available for label but we don't want the label
		     * to go all the way to the edge
		     */
		    if(utf8_width(this_label) >= trunc){
			if(trunc > 1){
			    strncpy(tmp_label, this_label, sizeof(tmp_label));
			    tmp_label[sizeof(tmp_label)-1] = '\0';
			    l = utf8_pad_to_width(this_label, tmp_label, sizeof(this_label)-2, trunc-1, 1);
			    this_label[l++] = SPACE;
			    this_label[l] = '\0';;
			}
			else if(trunc == 1)
			  this_label[0] = SPACE;
			else
			  this_label[0] = '\0';

			this_label[sizeof(this_label)-1] = '\0';
		    }
		}
	    }
	    else
	      this_label[0] = '\0';

	    if(!(k->column == last_time->column
		 && (last_in_row || (k+2)->column <= next_col)
		 && ((empty && !*last_time->label && !*last_time->name)
		     || (!empty
			 && this_label && !strcmp(this_label,last_time->label)
			 && ((k->name && !strcmp(k->name,last_time->name))
			     || ufk))))){
		if(empty){
		    /* blank out key with spaces */
		    strncpy(temp, repeat_char(
				    ((last_in_row || (k+2)->column > max_column)
					 ? max_column
					 : (k+2)->column) -
				     (fix_start
					 ? 0
					 : k->column),
					 SPACE), sizeof(temp));
		    temp[sizeof(temp)-1] = '\0';
		    last_time->column  = k->column;
		    *last_time->name   = '\0';
		    *last_time->label  = '\0';
		    MoveCursor(real_row + i, column + (fix_start ? 0 : k->column));
		    Write_to_screen(temp);
		    c = (fix_start ? 0 : k->column) + strlen(temp);
		}
		else{
		    /* make sure extra space before key name is there */
		    if(fix_start){
			strncpy(temp, repeat_char(k->column, SPACE), sizeof(temp));
			temp[sizeof(temp)-1] = '\0';
			MoveCursor(real_row + i, column + 0);
			Write_to_screen(temp);
		    }

		    /* short name of the key */
		    if(ufk)
		      snprintf(temp, sizeof(temp), "F%d", fkey);
		    else
		      strncpy(temp, k->name, sizeof(temp));

		    temp[sizeof(temp)-1] = '\0';
		    last_time->column = k->column;
		    strncpy(last_time->name, temp, 6*MAX_KEYNAME);
		    last_time->name[6*MAX_KEYNAME] = '\0';
		    /* make sure name not too long */
#ifdef	MOUSE
		    strncpy(keystr, temp, sizeof(keystr));
		    keystr[sizeof(keystr)-1] = '\0';
#endif
		    MoveCursor(real_row + i, column + k->column);
		    if(!empty){
			if(name_color)
			  (void)pico_set_colorp(name_color, PSC_NONE);
			else
			  StartInverse();
		    }

		    Write_to_screen(temp);
		    c = k->column + utf8_width(temp);
		    if(!empty){
			if(!name_color)
			  EndInverse();
		    }

		    /* now the space after the name and the label */
		    temp[0] = '\0';
		    if(c < max_column){
			temp[0] = SPACE;
			temp[1] = '\0';
			strncat(temp, this_label, sizeof(temp)-strlen(temp)-1);
			
			/* Don't run over the right hand edge */
			if(utf8_width(temp) > max_column - c){
			    size_t l;

			    l = utf8_pad_to_width(temp2, temp, sizeof(temp2)-1, max_column-c, 1);
			    temp2[l] = '\0';
			    strncpy(temp, temp2, sizeof(temp));
			    temp[sizeof(temp)-1] = '\0';
			}

			c += utf8_width(temp);
		    }
		    
#ifdef	MOUSE
		    strncat(keystr, temp, sizeof(keystr)-strlen(keystr)-1);
		    keystr[sizeof(keystr)-1] = '\0';
#endif
		    /* fill out rest of this key with spaces */
		    if(c < max_column){
			if(last_in_row){
			    strncat(temp, repeat_char(max_column - c, SPACE), sizeof(temp)-strlen(temp)-1);
			    c = max_column;
			}
			else{
			    if(c < (k+2)->column){
				strncat(temp,
				    repeat_char((k+2)->column - c, SPACE), sizeof(temp)-strlen(temp)-1);
				c = (k+2)->column;
			    }
			}

			temp[sizeof(temp)-1] = '\0';
		    }

		    strncpy(last_time->label, this_label, 6*MAX_LABEL);
		    last_time->label[6*MAX_LABEL] = '\0';
		    if(label_color)
		      (void)pico_set_colorp(label_color, PSC_NONE);

		    Write_to_screen(temp);
		}
	    }
#ifdef	MOUSE
	    else if(!empty)
	      /* fill in what register_key needs from cached data */
	      snprintf(keystr, sizeof(keystr), "%s %s", last_time->name, last_time->label);

	    if(!empty){
	      int len;

	      /*
	       * If label ends in space,
	       * don't register the space part of label.
	       */
	      len = strlen(keystr);
	      while(keystr[len-1] == SPACE)
		len--;
		len--;

	      register_key(j, ufk ? PF1 + fkey - 1
				  : (k->name[0] == '^')
				    ? ctrl(k->name[1])
				    : (!strucmp(k->name, "ret"))
				      ? ctrl('M')
				      : (!strucmp(k->name, "tab"))
					? '\t'
					: (!strucmp(k->name, "spc"))
					  ? SPACE
					    : (!strucmp(k->name, HISTORY_UP_KEYNAME))
					      ? KEY_UP
					      : (!strucmp(k->name, HISTORY_DOWN_KEYNAME))
						? KEY_DOWN
						: (k->bind.nch)
						  ? ((isascii((int) k->bind.ch[0]) && islower((int) k->bind.ch[0]))
						       ? toupper((unsigned char) k->bind.ch[0])
						       : k->bind.ch[0])
						  : k->name[0],
			   keystr, print_inverted_label,
			   real_row+i, k->column, len, 
			   name_color, label_color);
	    }
#endif

        }

	while(++j < 6*(i+1))
	  last_time_buf[j].column = -1;
    }

    fflush(stdout);
    if(lastc){
	(void)pico_set_colorp(lastc, PSC_NONE);
	free_color_pair(&lastc);
	if(label_color)
	  free_color_pair(&label_color);
	if(name_color)
	  free_color_pair(&name_color);
    }
}


/*
 * Clear the key menu lines.
 */
void
blank_keymenu(int row, int column)
{
    struct variable *vars = ps_global->vars;
    COLOR_PAIR *lastc;

    if(FOOTER_ROWS(ps_global) > 1){
	km_state.blanked    = 1;
	km_state.row        = row;
	km_state.column     = column;
	MoveCursor(row, column);
	lastc = pico_set_colors(VAR_KEYLABEL_FORE_COLOR,
				VAR_KEYLABEL_BACK_COLOR, PSC_NORM|PSC_RET);

	CleartoEOLN();
	MoveCursor(row+1, column);
	CleartoEOLN();
	fflush(stdout);
	if(lastc){
	    (void)pico_set_colorp(lastc, PSC_NONE);
	    free_color_pair(&lastc);
	}
    }
}


void
draw_cancel_keymenu(void)
{
    bitmap_t   bitmap;

    setbitmap(bitmap);
    draw_keymenu(&cancel_keymenu, bitmap, ps_global->ttyo->screen_cols,
		 1-FOOTER_ROWS(ps_global), 0, FirstMenu);
}


void
clearfooter(struct pine *ps)
{
    ClearLines(ps->ttyo->screen_rows - 3, ps->ttyo->screen_rows - 1);
    mark_keymenu_dirty();
    mark_status_unknown();
}
        

/*
 * Calculate formatting for key menu at bottom of screen
 *
 * Args:  km    -- The key_menu structure to format
 *        bm    -- Bitmap indicating which menu items should be displayed.  If
 *		   an item is NULL, that also means it shouldn't be displayed.
 *		   Sometimes the bitmap will be turned on in that case and just
 *		   rely on the NULL entry.
 *        width -- the screen width to format it at
 *
 * If already formatted for this particular screen width and the requested
 * bitmap and formatted bitmap agree, return.
 *
 * The formatting results in the column field in the key_menu being
 * filled in.  The column field is the column to start the label at, the
 * name of the key; after that is the label for the key.  The basic idea
 * is to line up the end of the names and beginning of the labels.  If
 * the name is too long and shifting it left would run into previous
 * label, then shift the whole menu right, or at least that entry if
 * things following are short enough to fit back into the regular
 * spacing.  This has to be calculated and not fixed so it can cope with
 * screen resize.
 */
void
format_keymenu(struct key_menu *km, unsigned char *bm, int width)
{
    int spacing[7], w[6], min_w[6], tw[6], extra[6], ufk, i, set;

    /* already formatted? */
    if(!km || (width == km->width &&
	       km->how_many <= km->formatted_hm &&
	       !memcmp(km->bitmap, bm, BM_SIZE)))
      return;

    /*
     * If we're in the initial command sequence we may be using function
     * keys instead of alphas, or vice versa, so we want to recalculate
     * the formatting next time through.
     */
    if((F_ON(F_USE_FK,ps_global) && ps_global->orig_use_fkeys) ||
        (F_OFF(F_USE_FK,ps_global) && !ps_global->orig_use_fkeys)){
	km->width        = width;
	km->formatted_hm = km->how_many;
	memcpy(km->bitmap, bm, BM_SIZE);
    }

    ufk = F_ON(F_USE_FK,ps_global);	/* ufk = "Using Function Keys" */

    /* set up "ideal" columns to start in, plus fake 7th column start */
    for(i = 0; i < 7; i++)
      spacing[i] = (i * width) / 6;

    /* Loop thru each set of 12 menus */
    for(set = 0; set < km->how_many; set++){
	int         k_top, k_bot, top_name_width, bot_name_width,
	            top_label_width, bot_label_width, done, offset, next_one;
	struct key *keytop, *keybot;

	offset = set * 12;		/* offset into keymenu */

	/*
	 * Find the required widths for each box.
	 */
	for(i = 0; i < 6; i++){
	    k_top  = offset + i*2;
	    k_bot  = k_top + 1;
	    keytop = &km->keys[k_top];
	    keybot = &km->keys[k_bot];

	    /*
	     * The width of a box is the max width of top or bottom name,
	     * plus 1 space, plus the max width of top or bottom label.
	     *
	     *     ? HelpInfo
	     *    ^C Cancel
	     *    |||||||||||  = 2 + 1 + 8 = 11
	     * 
	     * Then we adjust that by adding one space after the box to
	     * separate it from the next box. The last box doesn't need that
	     * but we may need an extra space for last box to avoid putting
	     * a character in the lower right hand cell of display.
	     * We also have a minimum label width (if screen is really narrow)
	     * of 3, so at least "Hel" and "Can" shows and the rest gets
	     * truncated off right hand side.
	     */

	    top_name_width = (keytop->name && bitnset(k_top,bm))
				     ? (ufk ? (i >= 5 ? 3 : 2)
					    : utf8_width(keytop->name)) : 0;
	    bot_name_width = (keybot->name && bitnset(k_bot,bm))
				     ? (ufk ? (i >= 4 ? 3 : 2)
					    : utf8_width(keybot->name)) : 0;
	    /*
	     * Labels are complicated by the fact that we want to look
	     * up their translation, but also by the fact that we surround
	     * the word with brackets like [ViewMsg] when the command is
	     * the default. We want to look up the translation of the
	     * part inside the brackets, not the whole thing.
	     */
	    if(keytop->label && bitnset(k_top,bm)){
		char tmp_label[6*MAX_LABEL+1];
		size_t l;

		if(keytop->label[0] == '[' && keytop->label[(l=strlen(keytop->label))-1] == ']' && l > 2){
		    /*
		     * Can't write in k->label, which might be a constant array.
		     */
		    strncpy(tmp_label, &keytop->label[1], MIN(sizeof(tmp_label),l-2));
		    tmp_label[MIN(sizeof(tmp_label)-1,l-2)] = '\0';

		    top_label_width = 2 + utf8_width(_(tmp_label));
		}
		else
		  top_label_width = utf8_width(_(keytop->label));
	    }
	    else
	      top_label_width = 0;

	    if(keybot->label && bitnset(k_bot,bm)){
		char tmp_label[6*MAX_LABEL+1];
		size_t l;

		if(keybot->label[0] == '[' && keybot->label[(l=strlen(keybot->label))-1] == ']' && l > 2){
		    strncpy(tmp_label, &keybot->label[1], MIN(sizeof(tmp_label),l-2));
		    tmp_label[MIN(sizeof(tmp_label)-1,l-2)] = '\0';

		    bot_label_width = 2 + utf8_width(_(tmp_label));
		}
		else
		  bot_label_width = utf8_width(_(keybot->label));
	    }
	    else
	      bot_label_width = 0;

	    /*
	     * The 1 for i < 5 is the space between adjacent boxes.
	     * The last 1 or 0 when i == 5 is so that we won't try to put
	     * a character in the lower right cell of the display, since that
	     * causes a linefeed on some terminals.
	     */
	    w[i] = MAX(top_name_width, bot_name_width) + 1 +
		   MAX(top_label_width, bot_label_width) +
		   ((i < 5) ? 1
			   : ((bot_label_width >= top_label_width) ? 1 : 0));

	    /*
	     * The smallest we'll squeeze a column.
	     *
	     *    X ABCDEF we'll squeeze to   X ABC
	     *   YZ GHIJ                     YZ GHI
	     */
	    min_w[i] = MAX(top_name_width, bot_name_width) + 1 +
		       MIN(MAX(top_label_width, bot_label_width), 3) +
		     ((i < 5) ? 1
			      : ((bot_label_width >= top_label_width) ? 1 : 0));

	    /* init trial width */
	    tw[i] = spacing[i+1] - spacing[i];
	    extra[i] = tw[i] - w[i];	/* negative if it doesn't fit */
	}

	/*
	 * See if we can fit everything on the screen.
	 */
	done = 0;
	while(!done){
	    int smallest_extra, how_small;

	    /* Find smallest extra */
	    smallest_extra = -1;
	    how_small = 100;
	    for(i = 0; i < 6; i++){
		if(extra[i] < how_small){
		    smallest_extra = i;
		    how_small = extra[i];
		}
	    }
	    
	    if(how_small >= 0)			/* everything fits */
	      done++;
	    else{
		int take_from, how_close;

		/*
		 * Find the one that is closest to the ideal width
		 * that has some extra to spare.
		 */
		take_from = -1;
		how_close = 100;
		for(i = 0; i < 6; i++){
		    if(extra[i] > 0 &&
		       ((spacing[i+1]-spacing[i]) - tw[i]) < how_close){
			take_from = i;
			how_close = (spacing[i+1]-spacing[i]) - tw[i];
		    }
		}

		if(take_from >= 0){
		    /*
		     * Found one. Take one from take_from and add it
		     * to the smallest_extra.
		     */
		    tw[smallest_extra]++;
		    extra[smallest_extra]++;
		    tw[take_from]--;
		    extra[take_from]--;
		}
		else{
		    int used_width;

		    /*
		     * Oops. Not enough space to fit everything in.
		     * Some of the labels are truncated. Some may even be
		     * truncated past the minimum. We make sure that each
		     * field is at least its minimum size, and then we cut
		     * back those over the minimum until we can fit all the
		     * minimal names on the screen (if possible).
		     */
		    for(i = 0; i < 6; i++)
		      tw[i] = MAX(tw[i], min_w[i]);
		    
		    used_width = 0;
		    for(i = 0; i < 6; i++)
		      used_width += tw[i];

		    while(used_width > width && !done){
			int candidate, excess;

			/*
			 * Find the one with the most width over it's
			 * minimum width.
			 */
			candidate = -1;
			excess = -100;
			for(i = 0; i < 6; i++){
			    if(tw[i] - min_w[i] > excess){
				candidate = i;
				excess = tw[i] - min_w[i];
			    }
			}

			if(excess > 0){
			    tw[candidate]--;
			    used_width--;
			}
			else
			  done++;
		    }

		    done++;
		}
	    }
	}

	/*
	 * Assign the format we came up with to the keymenu.
	 */
	next_one = 0;
	for(i = 0; i < 6; i++){
	    k_top  = offset + i*2;
	    k_bot  = k_top + 1;
	    keytop = &km->keys[k_top];
	    keybot = &km->keys[k_bot];
	    top_name_width = (keytop->name && bitnset(k_top,bm))
				     ? (ufk ? (i >= 5 ? 3 : 2)
					    : utf8_width(keytop->name)) : 0;
	    bot_name_width = (keybot->name && bitnset(k_bot,bm))
				     ? (ufk ? (i >= 4 ? 3 : 2)
					    : utf8_width(keybot->name)) : 0;

	    if(top_name_width >= bot_name_width){
		keytop->column = next_one;
		keybot->column = next_one + (top_name_width - bot_name_width);
	    }
	    else{
		keytop->column = next_one + (bot_name_width - top_name_width);
		keybot->column = next_one;
	    }

	    next_one += tw[i];
	}
    }
}


/*
 * Draw the key menu at bottom of screen
 *
 * Args:  km     -- key_menu structure
 *        bitmap -- which fields are active
 *        width  -- the screen width to format it at
 *	  row    -- where to put it
 *	  column -- where to put it
 *        what   -- this is an enum telling us whether to display the
 *		    first menu (first set of 12 keys), or to display the same
 *		    one we displayed last time, or to display a particular
 *		    one (which), or to display the next one.
 *
 * Fields are inactive if *either* the corresponding bitmap entry is 0 *or*
 * the actual entry in the key_menu is NULL.  Therefore, it is sometimes
 * useful to just turn on all the bits in a bitmap and let the NULLs take
 * care of it.  On the other hand, the bitmap gives a convenient method
 * for turning some keys on or off dynamically or due to options.
 * Both methods are used about equally.
 *
 * Also saves the state for a possible redraw later.
 *
 * Row should usually be a negative number.  If row is 0, the menu is not
 * drawn.
 */
void
draw_keymenu(struct key_menu *km, unsigned char *bitmap, int width, int row,
	     int column, OtherMenu what)
{
#ifdef _WINDOWS
    configure_menu_items (km, bitmap);
#endif
    format_keymenu(km, bitmap, width);

    /*--- save state for a possible redraw ---*/
    km_state.km         = km;
    km_state.row        = row;
    km_state.column     = column;
    memcpy(km_state.bitmap, bitmap, BM_SIZE);

    if(row == 0)
      return;

    if(km_state.blanked)
      keymenu_is_dirty = 1;

    if(what == FirstMenu || what == SecondMenu || what == ThirdMenu ||
       what == FourthMenu || what == MenuNotSet){
	if(what == FirstMenu || what == MenuNotSet)
	  km->which = 0;
	else if(what == SecondMenu)
	  km->which = 1;
	else if(what == ThirdMenu)
	  km->which = 2;
	else if(what == FourthMenu)
	  km->which = 3;
	
	if(km->which >= km->how_many)
	  km->which = 0;
    }
    else if(what == NextMenu)
      km->which = (km->which + 1) % km->how_many;
    /* else what must be SameMenu */

    output_keymenu(km, bitmap, row, column);

    km_state.blanked    = 0;
}


void
redraw_keymenu(void)
{
    if(km_state.blanked)
      blank_keymenu(km_state.row, km_state.column);
    else
      draw_keymenu(km_state.km, km_state.bitmap, ps_global->ttyo->screen_cols,
		   km_state.row, km_state.column, SameMenu);
}


/*
 * end_keymenu - free resources associated with keymenu display cache
 */
void
end_keymenu(void)
{
    int i;

    for(i = 0; i < 12; i++){
	if(last_time_buf[i].name)
	  fs_give((void **) &last_time_buf[i].name);

	if(last_time_buf[i].label)
	  fs_give((void **) &last_time_buf[i].label);
    }
}


/*----------------------------------------------------------------------
   Reveal Keymenu to Pine Command mappings

  Args: 

 ----*/
int
menu_command(UCS keystroke, struct key_menu *menu)
{
    int i, n;

    if(keystroke == KEY_UTF8 || keystroke == KEY_UNKNOWN)
      return(MC_UTF8);

    if(!menu)
      return(MC_UNKNOWN);

    if(F_ON(F_USE_FK,ps_global)){
	/* No alpha commands permitted in function key mode */
	if(keystroke < 0x80 && isalpha((unsigned char) keystroke))
	  return(MC_UNKNOWN);

	/* Tres simple: compute offset, and test */
	if(keystroke >= F1 && keystroke <= F12){
	    n = (menu->which * 12) + (keystroke - F1);
	    if(bitnset(n, menu->bitmap))
	      return(menu->keys[n].bind.cmd);
	}
    }
    else if(keystroke >= F1 && keystroke <= F12)
      return(MC_UNKNOWN);

    /* if ascii, coerce lower case */
    if(keystroke < 0x80 && isupper((unsigned char) keystroke))
      keystroke = tolower((unsigned char) keystroke);

    /* keep this here for Windows port */
    if((keystroke = validatekeys(keystroke)) == KEY_JUNK)
      return(MC_UNKNOWN);

    /* Scan the list for any keystroke/command binding */
    if(keystroke != NO_OP_COMMAND)
      for(i = (menu->how_many * 12) - 1;  i >= 0; i--)
        if(bitnset(i, menu->bitmap))
	  for(n = menu->keys[i].bind.nch - 1; n >= 0; n--)
	    if(keystroke == menu->keys[i].bind.ch[n])
	      return(menu->keys[i].bind.cmd);

    /*
     * If explicit mapping failed, check feature mappings and
     * hardwired defaults...
     */
    if(F_ON(F_ENABLE_PRYNT,ps_global)
	&& (keystroke == 'y' || keystroke == 'Y')){
	/* SPECIAL CASE: Scan the list for print bindings */
	for(i = (menu->how_many * 12) - 1;  i >= 0; i--)
	  if(bitnset(i, menu->bitmap))
	    if(menu->keys[i].bind.cmd == MC_PRINTMSG
	       || menu->keys[i].bind.cmd == MC_PRINTTXT)
	      return(menu->keys[i].bind.cmd);
    }

    if(F_ON(F_ENABLE_LESSTHAN_EXIT,ps_global)
       && (keystroke == '<' || keystroke == ','
	   || (F_ON(F_ARROW_NAV,ps_global) && keystroke == KEY_LEFT))){
	/* SPECIAL CASE: Scan the list for MC_EXIT bindings */
	for(i = (menu->how_many * 12) - 1;  i >= 0; i--)
	  if(bitnset(i, menu->bitmap))
	    if(menu->keys[i].bind.cmd == MC_EXIT)
	      return(MC_EXIT);
    }

    /*
     * If no match after scanning bindings, try universally
     * bound keystrokes...
     */
    switch(keystroke){
      case KEY_MOUSE :
	return(MC_MOUSE);

      case ctrl('P') :
      case KEY_UP :
	return(MC_CHARUP);

      case ctrl('N') :
      case KEY_DOWN :
	return(MC_CHARDOWN);

      case ctrl('F') :
      case KEY_RIGHT :
	return(MC_CHARRIGHT);

      case ctrl('B') :
      case KEY_LEFT :
	return(MC_CHARLEFT);

      case ctrl('A') :
	return(MC_GOTOBOL);

      case ctrl('E') :
	return(MC_GOTOEOL);

      case  ctrl('L') :
	return(MC_REPAINT);

      case KEY_RESIZE :
	return(MC_RESIZE);

      case NO_OP_IDLE:
      case NO_OP_COMMAND:
	if(USER_INPUT_TIMEOUT(ps_global))
	  user_input_timeout_exit(ps_global->hours_to_timeout); /* no return */

	return(MC_NONE);

      default :
	break;
    }

    return(MC_UNKNOWN);			/* utter failure */
}



/*----------------------------------------------------------------------
   Set up a binding for cmd, with one key bound to it.
   Use menu_add_binding to add more keys to this binding.

  Args: menu   -- the keymenu
	key    -- the initial key to bind to
	cmd    -- the command to initialize to
	name   -- a pointer to the string to point name to
	label  -- a pointer to the string to point label to
	keynum -- which key in the keys array to initialize

    For translation purposes, the label in the calling routine
    should be wrapped in an N_() macro.

 ----*/
void
menu_init_binding(struct key_menu *menu, UCS key, int cmd, char *name, char *label, int keynum)
{
    /* if ascii, coerce to lower case */
    if(key < 0x80 && isupper((unsigned char)key))
      key = tolower((unsigned char)key);

    /* remove binding from any other key */
    menu_clear_cmd_binding(menu, cmd);

    menu->keys[keynum].name     = name;
    menu->keys[keynum].label    = label;
    menu->keys[keynum].bind.cmd = cmd;
    menu->keys[keynum].bind.nch = 0;
    menu->keys[keynum].bind.ch[menu->keys[keynum].bind.nch++] = key;
}


/*----------------------------------------------------------------------
   Add a key/command binding to the given keymenu structure

  Args: 

 ----*/
void
menu_add_binding(struct key_menu *menu, UCS key, int cmd)
{
    int i, n;

    /* NOTE: cmd *MUST* already have had a binding */
    for(i = (menu->how_many * 12) - 1;  i >= 0; i--)
      if(menu->keys[i].bind.cmd == cmd){
	  for(n = menu->keys[i].bind.nch - 1;
	      n >= 0 && key != menu->keys[i].bind.ch[n];
	      n--)
	    ;

	  /* if ascii, coerce to lower case */
	  if(key < 0x80 && isupper((unsigned char)key))
	    key = tolower((unsigned char)key);

	  if(n < 0)		/* not already bound, bind it */
	    menu->keys[i].bind.ch[menu->keys[i].bind.nch++] = key;

	  break;
      }
}


/*----------------------------------------------------------------------
   REMOVE a key/command binding from the given keymenu structure

  Args: 

 ----*/
int
menu_clear_binding(struct key_menu *menu, UCS key)
{
    int i, n;

    /* if ascii, coerce to lower case */
    if(key < 0x80 && isupper((unsigned char)key))
      key = tolower((unsigned char)key);

    for(i = (menu->how_many * 12) - 1;  i >= 0; i--)
      for(n = menu->keys[i].bind.nch - 1; n >= 0; n--)
	if(key == menu->keys[i].bind.ch[n]){
	    int cmd = menu->keys[i].bind.cmd;

	    for(--menu->keys[i].bind.nch; n < menu->keys[i].bind.nch; n++)
	      menu->keys[i].bind.ch[n] = menu->keys[i].bind.ch[n+1];

	    return(cmd);
	}

    return(MC_UNKNOWN);
}


void
menu_clear_cmd_binding(struct key_menu *menu, int cmd)
{
    int i;

    for(i = (menu->how_many * 12) - 1;  i >= 0; i--){
	if(cmd == menu->keys[i].bind.cmd){
	    menu->keys[i].name     = NULL;
	    menu->keys[i].label    = NULL;
	    menu->keys[i].bind.cmd = 0;
	    menu->keys[i].bind.nch = 0;
	    menu->keys[i].bind.ch[0] = 0;
	}
    }
}


int
menu_binding_index(struct key_menu *menu, int cmd)
{
    int i;

    for(i = 0; i < menu->how_many * 12; i++)
      if(cmd == menu->keys[i].bind.cmd)
	return(i);

    return(-1);
}


#ifdef	MOUSE
/*
 * print_inverted_label - highlight the label of the given menu item.
 * (callback from pico mouse routines)
 *
 * So far, this is only
 * ever called with the top left row equal to the bottom right row.
 * If you change that you probably need to fix it.
 */
void
print_inverted_label(int state, MENUITEM *m)
{
    unsigned i, j, k;
    int      col_offsetwid, col_offsetchars, do_color = 0, skipwid = 0, skipchars = 0, len, c;
    char     prename[100];
    char     namepart[100];
    char     labelpart[100];
    char    *lp, *label;
    COLOR_PAIR *name_color = NULL, *label_color = NULL, *lastc = NULL;
    struct variable *vars = ps_global->vars;

    if(m->label &&  (lp=strchr(m->label, ' '))){
	char save;

	save = *lp;
	*lp = '\0';
	col_offsetwid = utf8_width(m->label);
	col_offsetchars = lp - m->label;
	*lp = save;
    }
    else
      col_offsetwid = col_offsetchars = 0;

    if(pico_usingcolor() && ((VAR_KEYLABEL_FORE_COLOR &&
			      VAR_KEYLABEL_BACK_COLOR) ||
			     (VAR_KEYNAME_FORE_COLOR &&
			      VAR_KEYNAME_BACK_COLOR))){
	lastc = pico_get_cur_color();

	if(VAR_KEYNAME_FORE_COLOR && VAR_KEYNAME_BACK_COLOR){
	    name_color = state ? new_color_pair(VAR_KEYNAME_BACK_COLOR,
						VAR_KEYNAME_FORE_COLOR)
			       : new_color_pair(VAR_KEYNAME_FORE_COLOR,
						VAR_KEYNAME_BACK_COLOR);
	}
	else if(VAR_REV_FORE_COLOR && VAR_REV_BACK_COLOR)
	  name_color = new_color_pair(VAR_REV_FORE_COLOR, VAR_REV_BACK_COLOR);

	if(VAR_KEYLABEL_FORE_COLOR && VAR_KEYLABEL_BACK_COLOR){
	    label_color = state ? new_color_pair(VAR_KEYLABEL_BACK_COLOR,
						 VAR_KEYLABEL_FORE_COLOR)
			        : new_color_pair(VAR_KEYLABEL_FORE_COLOR,
						 VAR_KEYLABEL_BACK_COLOR);
	}
	else if(VAR_REV_FORE_COLOR && VAR_REV_BACK_COLOR){
	    label_color = state ? new_color_pair(VAR_REV_FORE_COLOR,
						 VAR_REV_BACK_COLOR)
			        : new_color_pair(VAR_NORM_FORE_COLOR,
						 VAR_NORM_BACK_COLOR);
	}

	/*
	 * See if we can grok all these colors. If not, we're going to
	 * punt and pretend there are no colors at all.
	 */
	if(!pico_is_good_colorpair(name_color) ||
	   !pico_is_good_colorpair(label_color)){
	    if(name_color)
	      free_color_pair(&name_color);
	    if(label_color)
	      free_color_pair(&label_color);
	    if(lastc)
	      free_color_pair(&lastc);
	}
	else{
	    do_color++;
	    (void)pico_set_colorp(label_color, PSC_NONE);
	    if(!(VAR_KEYLABEL_FORE_COLOR && VAR_KEYLABEL_BACK_COLOR)){
		if(state)
		  StartInverse();
		else
		  EndInverse();
	    }
	}
    }

    if(!do_color){
	/*
	 * Command name's already inverted, leave it.
	 */
	skipwid = state ? 0 : col_offsetwid;
	skipchars = state ? 0 : col_offsetchars;
	if(state)
	  StartInverse();
	else
	  EndInverse();
    }

    MoveCursor((int)(m->tl.r), (int)(m->tl.c) + skipwid);

    label = m->label ? m->label : "";
    len = strlen(label);

    /*
     * this is a bit complicated by the fact that we have to keep track of
     * the screenwidth as we print the label, because the screenwidth might
     * not be the same as the number of characters.
* UNTESTED SINCE switching to UTF-8 *
     */
    for(i = m->tl.r; i <= m->br.r; i++){
      /* collect part before name */
      for(k=0, j = m->tl.c + skipchars; j < MIN(m->lbl.c,m->br.c); j++){
	  if(k < sizeof(prename))
	    prename[k++] = ' ';
      }

      if(k < sizeof(prename))
        prename[k] = '\0';

      /* collect name part */
      for(k=0; j < MIN(m->lbl.c+col_offsetchars,m->br.c); j++){
	  c = (i == m->lbl.r &&
	       j - m->lbl.c < len) ? label[j - m->lbl.c] : ' ';
	  if(k < sizeof(namepart))
	    namepart[k++] = c;
      }

      if(k < sizeof(namepart))
        namepart[k] = '\0';

      /* collect label part */
      for(k=0; j <= m->br.c; j++){
	  c = (i == m->lbl.r &&
	       j - m->lbl.c < len) ? label[j - m->lbl.c] : ' ';
	  if(k < sizeof(labelpart))
	  labelpart[k++] = c;
      }

      if(k < sizeof(labelpart))
        labelpart[k] = '\0';
    }

    if(prename)
      Write_to_screen(prename);

    if(namepart){
	if(name_color && col_offsetchars)
	  (void) pico_set_colorp(name_color, PSC_NONE);

	Write_to_screen(namepart);
    }

    if(labelpart){
	if(name_color && col_offsetchars){
	  if(label_color)
	    (void) pico_set_colorp(label_color, PSC_NONE);
	  else{
	      if(state)
		StartInverse();
	      else
		EndInverse();
	  }
	}

	Write_to_screen(labelpart);
    }

    if(do_color){
	if(lastc){
	    (void)pico_set_colorp(lastc, PSC_NONE);
	    free_color_pair(&lastc);
	}
	else if(state)
	  EndInverse();
	else
	  pico_set_normal_color();
	
	if(name_color)
	  free_color_pair(&name_color);
	if(label_color)
	  free_color_pair(&label_color);
    }
    else{
	if(state)
	  EndInverse();
    }
}
#endif	/* MOUSE */


#ifdef _WINDOWS
/*
 * This function scans the key menu and calls mswin.c functions
 * to build a corresponding windows menu.
 */
void
configure_menu_items (struct key_menu *km, bitmap_t bitmap)
{
    int 		i;
    struct key		*k;
    UCS			key;

    mswin_menuitemclear ();

    if(!km)
      return;

    for (i = 0, k = km->keys ; i < km->how_many * 12; i++, k++) {
	if (k->name != NULL && k->label != NULL && bitnset (i, bitmap) &&
	    k->menuitem != KS_NONE) {

	    if (k->name[0] == '^')
		key = ctrl(k->name[1]);
	    else if (strcmp(k->name, "Ret") == 0) 
		key = '\r';
	    else if (strcmp(k->name, "Spc") == 0) 
		key = ' ';
	    else
		key = k->name[0];

	    mswin_menuitemadd (key, k->label, k->menuitem, 0);
	}
    }
}
#endif
