/*
 * $Id: keymenu.h 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $
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

#ifndef PINE_KEYMENU_INCLUDED
#define PINE_KEYMENU_INCLUDED


#include <general.h>
#include "../pith/state.h"
#include "../pith/bitmap.h"


/*------
   A key menu has two ways to turn on and off individual items in the menu.
   If there is a null entry in the key_menu structure for that key, then
   it is off.  Also, if the passed bitmap has a zero in the position for
   that key, then it is off.  This means you can usually set all of the
   bitmaps and only turn them off if you want to kill a key that is normally
   there otherwise.
   Each key_menu is an array of keys with a multiple of 12 number of keys.
  ------*/
/*------
  Argument to draw_keymenu().  These are to identify which of the possibly
  multiple sets of twelve keys should be shown in the keymenu.  That is,
  a keymenu may have 24 or 36 keys, so that there are 2 or 3 different
  screens of key menus for that keymenu.  FirstMenu means to use the
  first twelve, NextTwelve uses the one after the previous one, SameTwelve
  uses the same one.
  ------*/
typedef enum {MenuNotSet = 0, FirstMenu, NextMenu, SameMenu,
	      SecondMenu, ThirdMenu, FourthMenu} OtherMenu;


struct key {
    char   *name;			/* the short name               */
    char   *label;			/* the descriptive label        */
    struct {				/*                              */
	short  cmd;			/* the resulting command        */
	short  nch;			/* how many of ch are active    */
	UCS    ch[11];			/* which keystrokes trigger cmd */
    } bind;				/*                              */
    KS_OSDATAVAR			/* slot for port-specific data  */
    short  column;			/* menu col after formatting    */
};


struct key_menu {
    unsigned int  how_many:4;		/* how many separate sets of 12      */
    unsigned int  which:4;		/* which of the sets are we using    */
    unsigned int  width:8;		/* screen width when formatting done */
    struct key	 *keys;			/* array of how_many*12 size structs */
    unsigned int  formatted_hm:4;	/* how_many when formatting done     */
    bitmap_t      bitmap;
};


/*
 * Definitions for the various Menu Commands...
 */
#define	MC_NONE		0		/* NO command defined */
#define	MC_UNKNOWN	200
#define	MC_UTF8		201

/* Cursor/page Motion */
#define	MC_CHARUP	100
#define	MC_CHARDOWN	101
#define	MC_CHARRIGHT	102
#define	MC_CHARLEFT	103
#define	MC_GOTOBOL	104
#define	MC_GOTOEOL	105
#define	MC_GOTOSOP	106
#define	MC_GOTOEOP	107
#define	MC_PAGEUP	108
#define	MC_PAGEDN	109
#define	MC_MOUSE	110
#define MC_HOMEKEY      111
#define MC_ENDKEY       112

/* New Screen Commands */
#define	MC_HELP		500
#define	MC_QUIT		501
#define	MC_OTHER	502
#define	MC_MAIN		503
#define	MC_INDEX	504
#define	MC_VIEW_TEXT	505
#define	MC_VIEW_ATCH	506
#define	MC_FOLDERS	507
#define	MC_ADDRBOOK	508
#define	MC_RELNOTES	509
#define	MC_KBLOCK	510
#define	MC_JOURNAL	511
#define	MC_SETUP	512
#define	MC_COLLECTIONS	513
#define	MC_PARENT	514
#define	MC_ROLE		515
#define	MC_LISTMGR	516
#define	MC_THRDINDX	517
#define	MC_SECURITY	518

/* Commands within Screens */
#define	MC_NEXTITEM	700
#define	MC_PREVITEM	701
#define	MC_DELETE	702
#define	MC_UNDELETE	703
#define	MC_COMPOSE	704
#define	MC_REPLY	705
#define	MC_FORWARD	706
#define	MC_GOTO		707
#define	MC_TAB		708
#define	MC_WHEREIS	709
#define	MC_ZOOM		710
#define	MC_PRINTMSG	711
#define	MC_PRINTTXT	712
#define	MC_TAKE		713
#define	MC_SAVE		714
#define	MC_EXPORT	715
#define	MC_IMPORT	716
#define	MC_EXPUNGE	717
#define	MC_UNEXCLUDE	718
#define	MC_CHOICE	719
#define	MC_SELECT	720
#define	MC_SELCUR	721
#define	MC_SELALL	722
#define	MC_UNSELALL	723
#define	MC_APPLY	724
#define	MC_SORT		725
#define	MC_FULLHDR	726
#define	MC_BOUNCE	727
#define	MC_FLAG		728
#define	MC_PIPE		729
#define	MC_EXIT		730
#define	MC_PRINTALL	731
#define	MC_REPAINT	732
#define	MC_JUMP		733
#define	MC_RESIZE	734
#define	MC_FWDTEXT	735
#define	MC_SAVETEXT	736
#define	MC_ABOUTATCH	737
#define	MC_LISTMODE	738
#define	MC_RENAMEFLDR	739
#define	MC_ADDFLDR	740
#define	MC_SUBSCRIBE	741
#define	MC_UNSUBSCRIBE	742
#define	MC_ADD		743
#define	MC_TOGGLE	744
#define	MC_EDIT		745
#define	MC_ADDABOOK	746
#define	MC_DELABOOK	747
#define	MC_VIEW_ENTRY	748
#define	MC_EDITABOOK	750
#define	MC_OPENABOOK	751
#define	MC_POPUP	752
#define	MC_EXPAND	753
#define	MC_UNEXPAND	754
#define	MC_COPY		755
#define	MC_SHUFFLE	756
#define	MC_VIEW_HANDLE	757
#define	MC_NEXT_HANDLE	758
#define	MC_PREV_HANDLE	759
#define	MC_QUERY_SERV	760
#define MC_GRIPE_LOCAL  761
#define MC_GRIPE_PIC    762
#define MC_GRIPE_READ   763
#define MC_GRIPE_POST   764
#define	MC_FINISH	765
#define	MC_PRINTFLDR	766
#define	MC_OPENFLDR	767
#define	MC_EDITFILE	768
#define	MC_ADDFILE	769
#define	MC_DELFILE	770
#define	MC_CHOICEB	771
#define	MC_CHOICEC	772
#define	MC_CHOICED	773
#define	MC_CHOICEE	774
#define	MC_DEFAULT	775
#define	MC_TOGGLEB	776
#define	MC_TOGGLEC	777
#define	MC_TOGGLED	778
#define	MC_RGB1		779
#define	MC_RGB2		780
#define	MC_RGB3		781
#define MC_EXITQUERY    782
#define	MC_ADDHDR	783
#define	MC_DELHDR	784
#define	MC_PRINTER	785
#define	MC_PASSWD	786
#define	MC_CONFIG	787
#define	MC_SIG		788
#define	MC_ABOOKS	789
#define	MC_CLISTS	790
#define	MC_RULES	791
#define	MC_DIRECTORY	792
#define	MC_KOLOR	793
#define	MC_EXCEPT	794
#define	MC_REMOTE	795
#define	MC_NO_EXCEPT	796
#define	MC_YES		797
#define	MC_NO		798
#define	MC_NOT		799
#define	MC_COLLAPSE	800
#define	MC_CHK_RECENT	801
#define	MC_DECRYPT	802


/*
 * Some standard Key/Command Bindings 
 */
#define	NULL_MENU	{NULL, NULL, {MC_NONE}, KS_NONE}
/*
 * TRANSLATORS: Alpine has a key menu at the bottom of each screen which
 * lists all of the available commands. Each command is a single character
 * which is followed with a short descriptive word which describes what
 * the command does. Commands are almost always to be thought of as verbs
 * so the descriptive words describe what the command does.
 * These descriptive words have to fit across the
 * screen so they would normally only be 8-10 characters long, though
 * they will be truncated if there isn't enough space. The command
 * descriptions are usually chosen so that they will be a mnemonic
 * for the command. For example, the command S is Save,
 * E is Export, Q is Quit, and ? is Help. You get to translate the
 * label (the Save part) but the actual command (the S) will stay
 * the same, so it will be very difficult to come up with mnemonic
 * labels. The mnemonic isn't necessary, just nice. You can see that
 * we have stretched to the edge of the usefullness of mnemonics with
 * cases like K Kolor (instead of Color) and X eXceptions (because E
 * already meant something else).
 */
#define	HELP_MENU	{"?", N_("Help"), \
			 {MC_HELP, 2, {'?',ctrl('G')}}, \
			 KS_SCREENHELP}
/* TRANSLATORS: Show Other commands */
#define	OTHER_MENU	{"O", N_("OTHER CMDS"), \
			 {MC_OTHER, 1, {'o'}}, \
			 KS_NONE}
/* TRANSLATORS: WhereIs command searches for text */
#define	WHEREIS_MENU	{"W", N_("WhereIs"), \
			 {MC_WHEREIS, 2, {'w',ctrl('W')}}, \
			 KS_WHEREIS}
/* TRANSLATORS: the Main Menu is the menu of commands you get when
   you first start alpine. */
#define	MAIN_MENU	{"M", N_("Main Menu"), \
			 {MC_MAIN, 1, {'m'}}, \
			 KS_MAINMENU}
#define	QUIT_MENU	{"Q", N_("Quit Alpine"), \
			 {MC_QUIT, 1, {'q'}}, \
			 KS_EXIT}
/* TRANSLATORS: go to Previous Message */
#define	PREVMSG_MENU	{"P", N_("PrevMsg"), \
			 {MC_PREVITEM, 1, {'p'}}, \
			 KS_PREVMSG}
/* TRANSLATORS: go to Next Message */
#define	NEXTMSG_MENU	{"N", N_("NextMsg"), \
			 {MC_NEXTITEM, 1, {'n'}}, \
			 KS_NEXTMSG}
#define	HOMEKEY_MENU	{"Hme", N_("FirstPage"), \
			 {MC_HOMEKEY, 1, {KEY_HOME}}, \
			 KS_NONE}
#define	ENDKEY_MENU	{"End", N_("LastPage"), \
			 {MC_ENDKEY, 1, {KEY_END}}, \
			 KS_NONE}
/* TRANSLATORS: go to Previous Page */
#define	PREVPAGE_MENU	{"-", N_("PrevPage"), \
			 {MC_PAGEUP, 3, {'-',ctrl('Y'),KEY_PGUP}}, \
			 KS_PREVPAGE}
#define	NEXTPAGE_MENU	{"Spc", N_("NextPage"), \
			 {MC_PAGEDN, 4, {'+',' ',ctrl('V'),KEY_PGDN}}, \
			 KS_NEXTPAGE}
/* TRANSLATORS: Jump to a different message in the index */
#define	JUMP_MENU	{"J", N_("Jump"), \
			 {MC_JUMP, 1, {'j'}}, \
			 KS_JUMPTOMSG}
/* TRANSLATORS: Forward Email */
#define	FWDEMAIL_MENU	{"F", N_("Fwd Email"), \
			{MC_FWDTEXT,1,{'f'}}, \
			 KS_FORWARD}
#define	PRYNTMSG_MENU	{"%", N_("Print"), \
			 {MC_PRINTMSG,1,{'%'}}, \
			 KS_PRINT}
#define	PRYNTTXT_MENU	{"%", N_("Print"), \
			 {MC_PRINTTXT,1,{'%'}}, \
			 KS_PRINT}
/* TRANSLATORS: In alpine, Save means to save something internally
   within alpine or within the email system. */
#define	SAVE_MENU	{"S", N_("Save"), \
			 {MC_SAVE,1,{'s'}}, \
			 KS_SAVE}
/* TRANSLATORS: In alpine, Export means to save something externally
   to alpine, to a file in the user's home directory, for example. */
#define	EXPORT_MENU	{"E", N_("Export"), \
			 {MC_EXPORT, 1, {'e'}}, \
			 KS_EXPORT}
/* TRANSLATORS: Edit a new message to be sent as email */
#define	COMPOSE_MENU	{"C", N_("Compose"), \
			 {MC_COMPOSE,1,{'c'}}, \
			 KS_COMPOSER}
/* TRANSLATORS: Edit a new message while acting in one of
   your roles */
#define	RCOMPOSE_MENU	{"#", N_("Role"), \
			 {MC_ROLE,1,{'#'}}, \
			 KS_NONE}
#define	DELETE_MENU	{"D", N_("Delete"), \
			 {MC_DELETE,2,{'d',KEY_DEL}}, \
			 KS_DELETE}
#define	UNDELETE_MENU	{"U", N_("Undelete"), \
			 {MC_UNDELETE,1,{'u'}}, \
			 KS_UNDELETE}
/* TRANSLATORS: Reply to an email message */
#define	REPLY_MENU	{"R", N_("Reply"), \
			 {MC_REPLY,1,{'r'}}, \
			 KS_REPLY}
/* TRANSLATORS: Forward an email message to someone else */
#define	FORWARD_MENU	{"F", N_("Forward"), \
			 {MC_FORWARD,1,{'f'}}, \
			 KS_FORWARD}
/* TRANSLATORS: go to List of Folders */
#define	LISTFLD_MENU	{"L", N_("ListFldrs"), \
			 {MC_COLLECTIONS,1,{'l'}}, \
			 KS_FLDRLIST}
/* TRANSLATORS: the index is a list of email messages, go there */
#define	INDEX_MENU	{"I", N_("Index"), \
			 {MC_INDEX,1,{'i'}}, \
			 KS_FLDRINDEX}
/* TRANSLATORS: Go To Folder, a command to go view another mail folder */
#define	GOTO_MENU	{"G", N_("GotoFldr"), \
			 {MC_GOTO,1,{'g'}}, \
			 KS_GOTOFLDR}
/* TRANSLATORS: Take Address is a command to copy addresses from a
   message to the user's address book */
#define	TAKE_MENU	{"T", N_("TakeAddr"), \
			 {MC_TAKE,1,{'t'}}, \
			 KS_TAKEADDR}
/* TRANSLATORS: To Flag a message means to mark it in some way,
   for example, flag it as important */
#define	FLAG_MENU	{"*", N_("Flag"), \
			 {MC_FLAG,1,{'*'}}, \
			 KS_FLAG}
/* TRANSLATORS: Pipe refers to the Unix pipe command (the vertical bar). */
#define	PIPE_MENU	{"|", N_("Pipe"), \
			 {MC_PIPE,1,{'|'}}, \
			 KS_NONE}
/* TRANSLATORS: Bounce is sometimes called re-sending a message.
   A user would use this command if a message had been incorrectly
   sent to him or her. */
#define	BOUNCE_MENU	{"B", N_("Bounce"), \
			 {MC_BOUNCE,1,{'b'}}, \
			 KS_BOUNCE}
/* TRANSLATORS: Header Mode, refers to showing more or fewer message
   headers when viewing a message. This command toggles between more
   and fewer each time it is typed. */
#define	HDRMODE_MENU	{"H", N_("HdrMode"), \
			 {MC_FULLHDR,1,{'h'}}, \
			 KS_HDRMODE}
/* TRANSLATORS: The Tab key goes to the Next New message */
#define	TAB_MENU	{"Tab", N_("NextNew"), \
			 {MC_TAB,1,{TAB}}, \
			 KS_NONE}
/* TRANSLATORS: go to the Previous item */
#define	PREV_MENU	{"P", N_("Prev"), \
			 {MC_PREVITEM,1,{'p'}}, \
			 KS_NONE}
#define	NEXT_MENU	{"N", N_("Next"), \
			 {MC_NEXTITEM,2,{'n','\t'}}, \
			 KS_NONE}
#define	EXIT_SETUP_MENU	{"E", N_("Exit Setup"), \
			 {MC_EXIT,1,{'e'}}, \
			 KS_EXITMODE}
#define	TOGGLE_MENU	{"X", "[" N_("Set/Unset") "]", \
			 {MC_TOGGLE,3,{'x',ctrl('M'),ctrl('J')}}, \
			 KS_NONE}
#define	TOGGLEB_MENU	{"X", "[" N_("Set/Unset") "]", \
			 {MC_TOGGLEB,3,{'x',ctrl('M'),ctrl('J')}}, \
			 KS_NONE}
#define	TOGGLEC_MENU	{"X", "[" N_("Set/Unset") "]", \
			 {MC_TOGGLEC,3,{'x',ctrl('M'),ctrl('J')}}, \
			 KS_NONE}
#define	TOGGLED_MENU	{"X", "[" N_("Set/Unset") "]", \
			 {MC_TOGGLED,3,{'x',ctrl('M'),ctrl('J')}}, \
			 KS_NONE}
/* TRANSLATORS: go to the Previous Collection. A Collection in Alpine refers
   to a collection of mail folders. */
#define	PREVC_MENU	{"P", N_("PrevCltn"), \
			 {MC_PREVITEM,1,{'p'}}, \
			 KS_NONE}
/* TRANSLATORS: Next Collection. */
#define	NEXTC_MENU	{"N", N_("NextCltn"), \
			 {MC_NEXTITEM,2,{'n','\t'}}, \
			 KS_NONE}
/* TRANSLATORS: Delete Collection. */
#define	DELC_MENU	{"D", N_("Del Cltn"), \
			 {MC_DELETE,2,{'d',KEY_DEL}}, \
			 KS_NONE}
/* TRANSLATORS: go to the Previous Folder (in a list of folders). */
#define	PREVF_MENU	{"P", N_("PrevFldr"), \
			 {MC_PREV_HANDLE,3,{'p',ctrl('B'),KEY_LEFT}}, \
			 KS_NONE}
/* TRANSLATORS: Next Folder (in a list of folders). */
#define	NEXTF_MENU	{"N", N_("NextFldr"), \
			 {MC_NEXT_HANDLE,4,{'n',ctrl('F'),TAB,KEY_RIGHT}}, \
			 KS_NONE}
/* TRANSLATORS: Current Index of messages (go to the current index) */
#define	CIND_MENU	{"I", N_("CurIndex"), \
			 {MC_INDEX,1,{'i'}}, \
			 KS_FLDRINDEX}
/* TRANSLATORS: View this Message */
#define	VIEWMSG_MENU	{">", "[" N_("ViewMsg") "]", \
			 {MC_VIEW_TEXT, 5,{'v','.','>',ctrl('M'),ctrl('J')}}, \
			 KS_VIEW}
/* TRANSLATORS: Sort the index of messages */
#define	FLDRSORT_MENU	{"$", N_("SortIndex"), \
			 {MC_SORT,1,{'$'}}, \
			 KS_SORT}
/* TRANSLATORS: Exit the Take Address screen */
#define	TA_EXIT_MENU	{"<",N_("ExitTake"), \
			 {MC_EXIT,4,{'e',ctrl('C'),'<',','}}, \
			 KS_EXITMODE}
#define	TA_NEXT_MENU	{"N",N_("Next"), \
			 {MC_CHARDOWN,4,{'n','\t',ctrl('N'),KEY_DOWN}}, \
			 KS_NONE}
/* TRANSLATORS: abbreviation for Previous */
#define	TA_PREV_MENU	{"P",N_("Prev"), \
			 {MC_CHARUP, 3, {'p',ctrl('P'),KEY_UP}}, \
			 KS_NONE}


/*
 * It's bogus that these are defined here. They go with the structures
 * defined in keymenu.c and have to stay in sync with them.
 */
#define OTHER_KEY		1
#define TWO_KEY			2
#define THREE_KEY		3
#define ADD_KEY			8
#define DELETE_KEY		9
#define SENDTO_KEY		10
#define SECONDARY_MAIN_KEY	15
#define RCOMPOSE_KEY		19
#define TAKE_KEY		21
#define SAVE_KEY		22
#define FORW_KEY		23
#define	KM_COL_KEY		2
#define	KM_SEL_KEY		3
#define	KM_MAIN_KEY		15
#define	KM_ALTVIEW_KEY		16
#define	KM_ZOOM_KEY		21
#define	KM_SELECT_KEY		22
#define	KM_SELCUR_KEY		23
#define	KM_RECENT_KEY		28
#define	KM_SHUFFLE_KEY		30
#define	KM_EXPORT_KEY		32
#define	KM_IMPORT_KEY		33
#define	FC_EXIT_KEY		1
#define	FC_COL_KEY		2
#define	FC_SEL_KEY		3
#define	FC_ALTSEL_KEY		8
#define	SB_SUB_KEY		1
#define SB_EXIT_KEY     	2
#define	SB_SEL_KEY		3
#define	SB_LIST_KEY		8
#define	HLP_MAIN_KEY		0
#define	HLP_SUBEXIT_KEY		1
#define	HLP_EXIT_KEY		2
#define	HLP_VIEW_HANDLE		3
#define	HLP_PREV_HANDLE		4
#define	HLP_NEXT_HANDLE		5
#define	HLP_ALL_KEY		9
#define TIMESTAMP_KEY		4
#define DEBUG_KEY		5
#define	LM_TRY_KEY		3
#define	LM_PREV_KEY		4
#define	LM_NEXT_KEY		5
#define BACK_KEY		2
#define PREVM_KEY		4
#define NEXTM_KEY		5
#define EXCLUDE_KEY		26
#define UNEXCLUDE_KEY		27
#define SELECT_KEY		28
#define APPLY_KEY		29
#define VIEW_FULL_HEADERS_KEY	32
#define BOUNCE_KEY		33
#define FLAG_KEY		34
#define VIEW_PIPE_KEY		35
#define SELCUR_KEY		38
#define ZOOM_KEY		39
#define COLLAPSE_KEY		45
#define	ATT_PARENT_KEY		2
#define	ATT_EXPORT_KEY		11
#define	ATT_PIPE_KEY		16
#define	ATT_BOUNCE_KEY		17
#define	ATT_PRINT_KEY		20
#define	ATT_REPLY_KEY		22
#define	ATT_FORWARD_KEY		23
#define	ATV_BACK_KEY		2
#define	ATV_VIEW_HILITE		3
#define	ATV_PREV_URL		4
#define	ATV_NEXT_URL		5
#define	ATV_EXPORT_KEY		11
#define	ATV_PIPE_KEY		16
#define	ATV_BOUNCE_KEY		17
#define	ATV_PRINT_KEY		20
#define	ATV_REPLY_KEY		22
#define	ATV_FORWARD_KEY		23
#define VIEW_ATT_KEY		 3
#define VIEW_FULL_HEADERS_KEY	32
#define	VIEW_VIEW_HANDLE	26
#define VIEW_SELECT_KEY		27
#define VIEW_PREV_HANDLE	28
#define VIEW_NEXT_HANDLE	29
#define	OE_HELP_KEY		0
#define	OE_CANCEL_KEY		1
#define	OE_CTRL_T_KEY		2
#define	OE_ENTER_KEY		3
#define SETUP_PRINTER		3
#define SETUP_PASSWD		4
#define SETUP_CONFIG		5
#define SETUP_SIG		6
#define SETUP_DIRECTORY		10
#define SETUP_EXCEPT		14
#define SETUP_SMIME		16
#define MAIN_HELP_KEY		0
#define MAIN_DEFAULT_KEY	3
#define MAIN_KBLOCK_KEY		9
#define MAIN_QUIT_KEY		14
#define MAIN_COMPOSE_KEY	15
#define MAIN_FOLDER_KEY		16
#define MAIN_INDEX_KEY		18
#define MAIN_SETUP_KEY		20
#define MAIN_ADDRESS_KEY	21
#define	NUOV_EXIT		2
#define	NUOV_VIEW		3
#define	NUOV_NEXT_PG		6
#define	NUOV_PREV_PG		7
#define	NUOV_RELNOTES		10
#define	DEFAULT_KEY		3
#define	CHANGEDEF_KEY		10
#define SMIME_PARENT_KEY	2
#define DECRYPT_KEY		(VIEW_PIPE_KEY + 7)
#define SECURITY_KEY		(DECRYPT_KEY + 1)


extern struct key_menu	cancel_keymenu,
			ab_keymenu,
			abook_select_km,
			abook_view_keymenu,
			abook_text_km,
			ldap_view_keymenu,
			c_mgr_km,
			c_cfg_km,
			c_sel_km,
			c_fcc_km,
			folder_km,
			folder_sel_km,
			folder_sela_km,
			folder_sub_km,
			folder_post_km,
			help_keymenu,
			rev_msg_keymenu,
			ans_certfail_keymenu,
			ans_certquery_keymenu,
			forge_keymenu,
			listmgr_keymenu,
			index_keymenu,
			simple_index_keymenu,
			thread_keymenu,
			att_index_keymenu,
			att_view_keymenu,
			view_keymenu,
			simple_text_keymenu,
			oe_keymenu,
			choose_setup_keymenu,
			main_keymenu,
			simple_file_keymenu,
			nuov_keymenu,
			modal_message_keymenu,
			ta_keymenu_lm,
			ta_keymenu_sm,
			pipe_cancel_keymenu,
			color_pattern_keymenu,
			hdr_color_checkbox_keymenu,
			kw_color_checkbox_keymenu,
			selectable_bold_checkbox_keymenu,
			flag_keymenu,
			addr_s_km,
			addr_s_km_with_goback,
			addr_s_km_with_view,
			addr_s_km_for_url,
			addr_s_km_exit,
			addr_s_km_goback,
			dir_conf_km,
			sel_from_list,
			sel_from_list_ctrlc,
			sel_from_list_sm,
			sel_from_list_sm_ctrlc,
			sel_from_list_lm,
			sel_from_list_lm_ctrlc,
			sel_from_list_olm,
			sel_from_list_olm_ctrlc,
			printer_edit_keymenu,
			printer_select_keymenu,
			role_select_km,
			role_conf_km,
			config_text_wshuf_keymenu,
			config_text_wshufandfldr_keymenu,
			config_role_file_keymenu,
			config_role_file_res_keymenu,
			config_role_keyword_keymenu,
			config_role_keyword_keymenu_not,
			config_role_charset_keymenu_not,
			config_role_keymenu,
			config_role_keymenu_not,
			config_role_keymenu_extra,
			config_role_addr_pat_keymenu,
			config_role_xtrahdr_keymenu,
			config_role_addr_act_keymenu,
			config_role_patfolder_keymenu,
			config_role_actionfolder_keymenu,
			config_role_inick_keymenu,
			config_role_afrom_keymenu,
			config_checkbox_keymenu,
			config_text_keymenu,
			config_text_to_charsets_keymenu,
			config_radiobutton_keymenu,
			config_yesno_keymenu,
			color_changing_keymenu,
			custom_color_changing_keymenu,
			kw_color_changing_keymenu,
			color_rgb_keymenu,
			custom_rgb_keymenu,
			kw_rgb_keymenu,
			color_setting_keymenu,
			custom_color_setting_keymenu,
			role_color_setting_keymenu,
			kw_color_setting_keymenu,
			take_export_keymenu_sm,
			take_export_keymenu_lm,
			config_smime_helper_keymenu,
			smime_info_keymenu;

extern struct key rev_msg_keys[];


/* exported protoypes */
void	    draw_cancel_keymenu(void);
void	    end_keymenu(void);
int	    menu_command(UCS, struct key_menu *);
void	    menu_init_binding(struct key_menu *, UCS, int, char *, char *, int);
void	    menu_add_binding(struct key_menu *, UCS, int);
int	    menu_clear_binding(struct key_menu *, UCS);
int	    menu_binding_index(struct key_menu *, int);
void	    mark_keymenu_dirty(void);
void	    blank_keymenu(int, int);
void	    draw_keymenu(struct key_menu *, bitmap_t, int, int, int, OtherMenu);
void	    redraw_keymenu(void);
void	    clearfooter(struct pine *);


#endif /* PINE_KEYMENU_INCLUDED */
