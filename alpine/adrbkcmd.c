#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: adrbkcmd.c 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $";
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

/*====================================================================== 
    adrbkcmds.c
    Commands called from the addrbook screens.
 ====*/


#include "headers.h"
#include "adrbkcmd.h"
#include "addrbook.h"
#include "takeaddr.h"
#include "status.h"
#include "keymenu.h"
#include "mailview.h"
#include "mailcmd.h"
#include "mailindx.h"
#include "radio.h"
#include "folder.h"
#include "reply.h"
#include "help.h"
#include "titlebar.h"
#include "signal.h"
#include "roleconf.h"
#include "send.h"
#include "../pith/adrbklib.h"
#include "../pith/addrbook.h"
#include "../pith/abdlc.h"
#include "../pith/ablookup.h"
#include "../pith/bldaddr.h"
#include "../pith/ldap.h"
#include "../pith/state.h"
#include "../pith/filter.h"
#include "../pith/conf.h"
#include "../pith/msgno.h"
#include "../pith/addrstring.h"
#include "../pith/remote.h"
#include "../pith/url.h"
#include "../pith/util.h"
#include "../pith/detoken.h"
#include "../pith/stream.h"
#include "../pith/send.h"
#include "../pith/list.h"
#include "../pith/busy.h"


/* internal prototypes */
int	       url_hilite_abook(long, char *, LT_INS_S **, void *);
int            process_abook_view_cmd(int, MSGNO_S *, SCROLL_S *);
int            expand_addrs_for_pico(struct headerentry *, char ***);
char          *view_message_for_pico(char **);
int            verify_nick(char *, char **, char **, BUILDER_ARG *, int *);
int            verify_addr(char *, char **, char **, BUILDER_ARG *, int *);
int            pico_sendexit_for_adrbk(struct headerentry *, void(*)(void), int, char **);
char          *pico_cancelexit_for_adrbk(char *, void (*)(void));
char          *pico_cancel_for_adrbk_take(void (*)(void));
char          *pico_cancel_for_adrbk_edit(void (*)(void));
int            ab_modify_abook_list(int, int, int, char *, char *, char *);
int            convert_abook_to_remote(struct pine *, PerAddrBook *, char *, size_t, int);
int            any_rule_files_to_warn_about(struct pine *);
int            verify_folder_name(char *,char **,char **,BUILDER_ARG *, int *);
int            verify_server_name(char *,char **,char **,BUILDER_ARG *, int *);
int            verify_abook_nick(char *, char **,char **,BUILDER_ARG *, int *);
int            do_the_shuffle(int *, int, int, char **);
void           ab_compose_internal(BuildTo, int);
int            ab_export(struct pine *, long, int, int);
VCARD_INFO_S  *prepare_abe_for_vcard(struct pine *, AdrBk_Entry *, int);
void           write_single_tab_entry(gf_io_t, VCARD_INFO_S *);
int            percent_done_copying(void);
int            cmp_action_list(const qsort_t *, const qsort_t *);
void           set_act_list_member(ACTION_LIST_S *, a_c_arg_t, PerAddrBook *, PerAddrBook *, char *);
void           convert_pinerc_to_remote(struct pine *, char *);

#ifdef	ENABLE_LDAP
typedef struct _saved_query {
    char *qq,
	 *cn,
	 *sn,
	 *gn,
	 *mail,
	 *org,
	 *unit,
	 *country,
	 *state,
	 *locality,
	 *custom;
} SAVED_QUERY_S;

int            process_ldap_cmd(int, MSGNO_S *, SCROLL_S *);
int            pico_simpleexit(struct headerentry *, void (*)(void), int, char **);
char          *pico_simplecancel(void (*)(void));
void           save_query_parameters(SAVED_QUERY_S *);
SAVED_QUERY_S *copy_query_parameters(SAVED_QUERY_S *);
void           free_query_parameters(SAVED_QUERY_S **);
int            restore_query_parameters(struct headerentry *, char ***);

static char *expander_address;
#endif	/* ENABLE_LDAP */

static char *fakedomain = "@";


#define	VIEW_ABOOK_NONE		0
#define	VIEW_ABOOK_EDITED	1
#define	VIEW_ABOOK_WARPED	2

/*
 * View an addrbook entry.
 * Call scrolltool to do the work.
 */
void
view_abook_entry(struct pine *ps, long int cur_line)
{
    AdrBk_Entry *abe;
    STORE_S    *in_store, *out_store;
    char       *string, *errstr;
    SCROLL_S	sargs;
    HANDLE_S   *handles = NULL;
    URL_HILITE_S uh;
    gf_io_t	pc, gc;
    int         cmd, abook_indent;
    long	offset = 0L;
    char        b[500];

    dprint((5, "- view_abook_entry -\n"));

    if(is_addr(cur_line)){
	abe = ae(cur_line);
	if(!abe){
	    q_status_message(SM_ORDER, 0, 3, _("Error reading entry"));
	    return;
	}
    }
    else{
	q_status_message(SM_ORDER, 0, 3, _("Nothing to view"));
	return;
    }

    if(!(in_store = so_get(CharStar, NULL, EDIT_ACCESS))){
	q_status_message(SM_ORDER | SM_DING, 3, 3, _("Error allocating space."));
	return;
    }

    abook_indent = utf8_width(_("Nickname")) + 2;

    /* TRANSLATORS: Nickname is a shorthand name for something */
    utf8_snprintf(b, sizeof(b), "%-*.*w: ", abook_indent, abook_indent, _("Nickname"));
    so_puts(in_store, b);
    if(abe->nickname)
      so_puts(in_store, abe->nickname);

    so_puts(in_store, "\015\012");

    /* TRANSLATORS: Full name is the name that goes with an email address.
       For example, in
       Fred Flintstone <fred@bedrock.org>
       Fred Flintstone is the Full Name. */
    utf8_snprintf(b, sizeof(b), "%-*.*w: ", abook_indent, abook_indent, _("Fullname"));
    so_puts(in_store, b);
    if(abe->fullname)
      so_puts(in_store, abe->fullname);

    so_puts(in_store, "\015\012");

    /* TRANSLATORS: Fcc is an abbreviation for File carbon copy. It is like
       a cc which is a copy of a message that goes to somebody other than the
       main recipient, only this is a copy of a message which is put into a
       file on the user's computer. */
    utf8_snprintf(b, sizeof(b), "%-*.*w: ", abook_indent, abook_indent, _("Fcc"));
    so_puts(in_store, b);
    if(abe->fcc)
      so_puts(in_store, abe->fcc);

    so_puts(in_store, "\015\012");

    utf8_snprintf(b, sizeof(b), "%-*.*w: ", abook_indent, abook_indent, AB_COMMENT_STR);
    so_puts(in_store, b);
    if(abe->extra)
      so_puts(in_store, abe->extra);

    so_puts(in_store, "\015\012");

    /* TRANSLATORS: Addresses refers to email Addresses */
    utf8_snprintf(b, sizeof(b), "%-*.*w: ", abook_indent, abook_indent, _("Addresses"));
    so_puts(in_store, b);
    if(abe->tag == Single){
	char *tmp = abe->addr.addr ? abe->addr.addr : "";
#ifdef	ENABLE_LDAP
	if(!strncmp(tmp, QRUN_LDAP, LEN_QRL))
	  string = LDAP_DISP;
	else
#endif
	  string = tmp;

	so_puts(in_store, string);
    }
    else{
	char **ll;

	for(ll = abe->addr.list; ll && *ll; ll++){
	    if(ll != abe->addr.list){
		so_puts(in_store, "\015\012");
		so_puts(in_store, repeat_char(abook_indent+2, SPACE));
	    }

#ifdef	ENABLE_LDAP
	    if(!strncmp(*ll, QRUN_LDAP, LEN_QRL))
	      string = LDAP_DISP;
	    else
#endif
	      string = *ll;

	    so_puts(in_store, string);

	    if(*(ll+1))	/* not the last one */
	      so_puts(in_store, ",");
	}
    }

    so_puts(in_store, "\015\012");

    do{
	if(!(out_store = so_get(CharStar, NULL, EDIT_ACCESS))){
	    so_give(&in_store);
	    q_status_message(SM_ORDER | SM_DING, 3, 3,
			     _("Error allocating space."));
	    return;
	}

	so_seek(in_store, 0L, 0);

	init_handles(&handles);
	gf_filter_init();

	if(F_ON(F_VIEW_SEL_URL, ps_global)
	   || F_ON(F_VIEW_SEL_URL_HOST, ps_global)
	   || F_ON(F_SCAN_ADDR, ps_global))
	  gf_link_filter(gf_line_test,
			 gf_line_test_opt(url_hilite_abook,
					  gf_url_hilite_opt(&uh,&handles,0)));

	gf_link_filter(gf_wrap, gf_wrap_filter_opt(ps->ttyo->screen_cols - 4,
						   ps->ttyo->screen_cols,
						   NULL, abook_indent+2,
						   GFW_HANDLES));
	gf_link_filter(gf_nvtnl_local, NULL);

	gf_set_so_readc(&gc, in_store);
	gf_set_so_writec(&pc, out_store);

	if((errstr = gf_pipe(gc, pc)) != NULL){
	    so_give(&in_store);
	    so_give(&out_store);
	    free_handles(&handles);
	    q_status_message1(SM_ORDER | SM_DING, 3, 3,
			      /* TRANSLATORS: %s is the error message */
			      _("Can't format entry: %s"), errstr);
	    return;
	}

	gf_clear_so_writec(out_store);
	gf_clear_so_readc(in_store);

	memset(&sargs, 0, sizeof(SCROLL_S));
	sargs.text.text	   = so_text(out_store);
	sargs.text.src	   = CharStar;
	sargs.text.desc	   = _("expanded entry");
	sargs.text.handles = handles;

	if(offset){		/* resize?  preserve paging! */
	    sargs.start.on	   = Offset;
	    sargs.start.loc.offset = offset;
	    offset = 0L;
	}
	  
	/* TRANSLATORS: a screen title. We are viewing an address book */
	sargs.bar.title	  = _("ADDRESS BOOK (View)");
	sargs.bar.style	  = TextPercent;
	sargs.proc.tool	  = process_abook_view_cmd;
	sargs.proc.data.i = VIEW_ABOOK_NONE;
	sargs.resize_exit = 1;
	sargs.help.text	  = h_abook_view;
	/* TRANSLATORS: help screen title */
	sargs.help.title  = _("HELP FOR ADDRESS BOOK VIEW");
	sargs.keys.menu	  = &abook_view_keymenu;
	setbitmap(sargs.keys.bitmap);

	if(handles)
	  sargs.keys.menu->how_many = 2;
	else{
	    sargs.keys.menu->how_many = 1;
	    clrbitn(OTHER_KEY, sargs.keys.bitmap);
	}

	if((cmd = scrolltool(&sargs)) == MC_RESIZE)
	  offset = sargs.start.loc.offset;

	so_give(&out_store);
	free_handles(&handles);
    }
    while(cmd == MC_RESIZE);

    so_give(&in_store);

    if(sargs.proc.data.i != VIEW_ABOOK_NONE){
	long old_l_p_p, old_top_ent, old_cur_row;

	if(sargs.proc.data.i == VIEW_ABOOK_WARPED){
	    /*
	     * Warped means we got plopped down somewhere in the display
	     * list so that we don't know where we are relative to where
	     * we were before we warped.  The current line number will
	     * be zero, since that is what the warp would have set.
	     */
	    as.top_ent = first_line(0L - as.l_p_page/2L);
	    as.cur_row = 0L - as.top_ent;
	}
	else if(sargs.proc.data.i == VIEW_ABOOK_EDITED){
	    old_l_p_p   = as.l_p_page;
	    old_top_ent = as.top_ent;
	    old_cur_row = as.cur_row;
	}

	/* Window size may have changed while in pico. */
	ab_resize();

	/* fix up what ab_resize messed up */
	if(sargs.proc.data.i != VIEW_ABOOK_WARPED && old_l_p_p == as.l_p_page){
	    as.top_ent     = old_top_ent;
	    as.cur_row     = old_cur_row;
	    as.old_cur_row = old_cur_row;
	}

	cur_line = as.top_ent+as.cur_row;
    }

    ps->mangled_screen = 1;
}


int
url_hilite_abook(long int linenum, char *line, LT_INS_S **ins, void *local)
{
    register char *lp;

    if((lp = strchr(line, ':')) &&
       !strncmp(line, AB_COMMENT_STR, strlen(AB_COMMENT_STR)))
      (void) url_hilite(linenum, lp + 1, ins, local);

    return(0);
}


int
process_abook_view_cmd(int cmd, MSGNO_S *msgmap, SCROLL_S *sparms)
{
    int rv = 1, i;
    PerAddrBook *pab;
    AddrScrn_Disp *dl;
    static ESCKEY_S text_or_vcard[] = {
	/* TRANSLATORS: Text refers to plain old text, probably the text of
	   an email message */
	{'t', 't', "T", N_("Text")},
	/* TRANSLATORS: A VCard is kind of like an electronic business card. It is not
	   something specific to alpine, it is universal. */
	{'v', 'v', "V", N_("VCard")},
	{-1, 0, NULL, NULL}};

    ps_global->next_screen = SCREEN_FUN_NULL;

    switch(cmd){
      case MC_EDIT :
	/*
	 * MC_EDIT works a little differently from the other cmds here.
	 * The others return 0 to scrolltool so that we are still in
	 * the view screen. This one is different because we may have
	 * changed what we're viewing. We handle that by returning 1
	 * to scrolltool and setting the sparms opt union's gint
	 * to the value below.
	 *
	 * (Late breaking news. Now we're going to return 1 from all these
	 * commands and then in the caller we're going to bounce back out
	 * to the index view instead of the view of the individual entry.
	 * So there is some dead code around for now.)
	 * 
	 * Then, in the view_abook_entry function we check the value
	 * of this field on scrolltool's return and if it is one of
	 * the two special values below view_abook_entry resets the
	 * current line if necessary, flushes the display cache (in
	 * ab_resize) and loops back and starts over, effectively
	 * putting us back in the view screen but with updated
	 * contents. A side effect is that the screen above that (the
	 * abook index) will also have been flushed and corrected by
	 * the ab_resize.
	 */
	pab = &as.adrbks[cur_addr_book()];
	if(pab && pab->access == ReadOnly){
	    /* TRANSLATORS: Address book can be viewed but not changed */
	    q_status_message(SM_ORDER, 0, 4, _("AddressBook is Read Only"));
	    rv = 0;
	    break;
	}

	if(adrbk_check_all_validity_now()){
	    if(resync_screen(pab, AddrBookScreen, 0)){
		q_status_message(SM_ORDER | SM_DING, 3, 4,
		 /* TRANSLATORS: The address book was changed by some other
		    process. The user is being told that their change (their
		    Update) has been canceled and they should try again. */
		 _("Address book changed. Update cancelled. Try again."));
		ps_global->mangled_screen = 1;
		break;
	    }
	}

	if(pab && pab->access == ReadOnly){
	    q_status_message(SM_ORDER, 0, 4, _("AddressBook is Read Only"));
	    rv = 0;
	    break;
	}

	/*
	 * Arguments all come from globals, not from the arguments to
	 * process_abook_view_cmd. It would be a little cleaner if the
	 * information was contained in att, I suppose.
	 */
	if(is_addr(as.cur_row+as.top_ent)){
	    AdrBk_Entry *abe, *abe_copy;
	    a_c_arg_t    entry;
	    int          warped = 0;

	    dl = dlist(as.top_ent+as.cur_row);
	    entry = dl->elnum;
	    abe = adrbk_get_ae(pab->address_book, entry);
	    abe_copy = copy_ae(abe);
	    dprint((9,"Calling edit_entry to edit from view\n"));
	    /* TRANSLATORS: update as in update an address book entry */
	    edit_entry(pab->address_book, abe_copy, entry,
		       abe->tag, 0, &warped, _("update"));
	    /*
	     * The ABOOK_EDITED case doesn't mean that we necessarily
	     * changed something, just that we might have but we know
	     * we didn't change the sort order (causing the warp).
	     */
	    sparms->proc.data.i = warped
				   ? VIEW_ABOOK_WARPED : VIEW_ABOOK_EDITED;

	    free_ae(&abe_copy);
	    rv = 1;			/* causes scrolltool to return */
	}
	else{
	    q_status_message(SM_ORDER, 0, 4,
			     "Something wrong, entry not updateable");
	    rv = 0;
	    break;
	}

	break;

      case MC_SAVE :
	pab = &as.adrbks[cur_addr_book()];
	/*
	 * Arguments all come from globals, not from the arguments to
	 * process_abook_view_cmd. It would be a little cleaner if the
	 * information was contained in att, I suppose.
	 */
	(void)ab_save(ps_global, pab ? pab->address_book : NULL,
		      as.top_ent+as.cur_row, -FOOTER_ROWS(ps_global), 0);
	rv = 1;
	break;

      case MC_COMPOSE :
	(void)ab_compose_to_addr(as.top_ent+as.cur_row, 0, 0);
	rv = 1;
	break;

      case MC_ROLE :
	(void)ab_compose_to_addr(as.top_ent+as.cur_row, 0, 1);
	rv = 1;
	break;

      case MC_FORWARD :
	rv = 1;
	/* TRANSLATORS: A question with two choices for the answer. Forward
	   as text means to include the text of the message being forwarded
	   in the new message. Forward as a Vcard attachment means to
	   attach it to the message in a special format so it is recognizable
	   as a Vcard. */
	i = radio_buttons(_("Forward as text or forward as Vcard attachment ? "),
			  -FOOTER_ROWS(ps_global), text_or_vcard, 't', 'x',
			  h_ab_text_or_vcard, RB_NORM);
	switch(i){
	  case 'x':
	    q_status_message(SM_INFO, 0, 2, _("Address book forward cancelled"));
	    rv = 0;
	    break;

	  case 't':
	    forward_text(ps_global, sparms->text.text, sparms->text.src);
	    break;

	  case 'v':
	    (void)ab_forward(ps_global, as.top_ent+as.cur_row, 0);
	    break;

	  default:
	    q_status_message(SM_ORDER, 3, 3,
			     "can't happen in process_abook_view_cmd");
	    break;
	}

	break;

      default:
	panic("Unexpected command in process_abook_view_cmd");
	break;
    }

    return(rv);
}


/*
 * Give expanded view of this address entry.
 * Call scrolltool to do the work.
 *
 * Args: headents -- The headerentry array from pico.
 *              s -- Unused here.
 *
 * Returns -- Always 0.
 */
int
expand_addrs_for_pico(struct headerentry *headents, char ***s)
{
    BuildTo      bldto;
    STORE_S     *store;
    char        *error = NULL, *addr = NULL, *fullname = NULL, *address = NULL;
    SAVE_STATE_S state;
    ADDRESS     *adrlist = NULL, *a;
    int          j, address_index = -1, fullname_index = -1, no_a_fld = 0;
    void       (*redraw)(void) = ps_global->redrawer;

    char        *tmp, *tmp2, *tmp3;
    SCROLL_S	 sargs;
    AdrBk_Entry  abe;
    char         fakeaddrpmt[500];
    int          abook_indent;

    dprint((5, "- expand_addrs_for_pico -\n"));

    abook_indent = utf8_width(_("Nickname")) + 2;
    utf8_snprintf(fakeaddrpmt, sizeof(fakeaddrpmt), "%-*.*w: ", abook_indent, abook_indent, _("Address"));

    if(s)
      *s = NULL;

    ps_global->redrawer = NULL;
    fix_windsize(ps_global);

    ab_nesting_level++;
    save_state(&state);

    for(j=0;
	headents[j].name != NULL && (address_index < 0 || fullname_index < 0);
	j++){
	if(!strncmp(headents[j].name, "Address", 7) || !strncmp(headents[j].name, _("Address"), strlen(_("Address"))))
	  address_index = j;
	else if(!strncmp(headents[j].name, "Fullname", 8) || !strncmp(headents[j].name, _("Fullname"), strlen(_("Fullname"))))
	  fullname_index = j;
    }

    if(address_index >= 0)
      address = *headents[address_index].realaddr;
    else{
	address_index = 1000;	/* a big number */
	no_a_fld++;
    }

    if(fullname_index >= 0)
      fullname = adrbk_formatname(*headents[fullname_index].realaddr,
				  NULL, NULL);

    memset(&abe, 0, sizeof(abe));
    if(fullname)
      abe.fullname = cpystr(fullname);
    
    if(address){
	char *tmp_a_string;

	tmp_a_string = cpystr(address);
	rfc822_parse_adrlist(&adrlist, tmp_a_string, fakedomain);
	fs_give((void **)&tmp_a_string);
	if(adrlist && adrlist->next){
	    int cnt = 0;

	    abe.tag = List;
	    for(a = adrlist; a; a = a->next)
	      cnt++;
	    
	    abe.addr.list = (char **)fs_get((cnt+1) * sizeof(char *));
	    cnt = 0;
	    for(a = adrlist; a; a = a->next){
		if(a->host && a->host[0] == '@')
		  abe.addr.list[cnt++] = cpystr(a->mailbox);
		else if(a->host && a->host[0] && a->mailbox && a->mailbox[0])
		  abe.addr.list[cnt++] =
				  cpystr(simple_addr_string(a, tmp_20k_buf,
							    SIZEOF_20KBUF));
	    }

	    abe.addr.list[cnt] = '\0';
	}
	else{
	    abe.tag = Single;
	    abe.addr.addr = address;
	}

	if(adrlist)
	  mail_free_address(&adrlist);

	bldto.type = Abe;
	bldto.arg.abe = &abe;
	our_build_address(bldto, &addr, &error, NULL, NULL);
	if(error){
	    q_status_message1(SM_ORDER, 3, 4, "%s", error);
	    fs_give((void **)&error);
	}
	
	if(addr){
	    tmp_a_string = cpystr(addr);
	    rfc822_parse_adrlist(&adrlist, tmp_a_string, ps_global->maildomain);
	    fs_give((void **)&tmp_a_string);
	}
    }
#ifdef	ENABLE_LDAP
    else if(no_a_fld && expander_address){
	WP_ERR_S wp_err;

	memset(&wp_err, 0, sizeof(wp_err));
	adrlist = wp_lookups(expander_address, &wp_err, 0);
	if(fullname && *fullname){
	    if(adrlist){
		if(adrlist->personal)
		  fs_give((void **)&adrlist->personal);
		
		adrlist->personal = cpystr(fullname);
	    }
	}

	if(wp_err.error){
	    q_status_message1(SM_ORDER, 3, 4, "%s", wp_err.error);
	    fs_give((void **)&wp_err.error);
	}
    }
#endif
    
    if(!(store = so_get(CharStar, NULL, EDIT_ACCESS))){
	q_status_message(SM_ORDER | SM_DING, 3, 3, _("Error allocating space."));
	restore_state(&state);
	return(0);
    }

    for(j = 0; j < address_index && headents[j].name != NULL; j++){
	if((tmp = fold(*headents[j].realaddr,
		      ps_global->ttyo->screen_cols,
		      ps_global->ttyo->screen_cols,
		      headents[j].prompt,
		      repeat_char(headents[j].prwid, SPACE), FLD_NONE)) != NULL){
	    so_puts(store, tmp);
	    fs_give((void **)&tmp);
	}
    }

    /*
     * There is an assumption that Addresses is the last field.
     */
    tmp3 = cpystr(repeat_char(headents[0].prwid + 2, SPACE));
    if(adrlist){
	for(a = adrlist; a; a = a->next){
	    ADDRESS *next_addr;
	    char    *bufp;
	    size_t   len;

	    next_addr = a->next;
	    a->next = NULL;
	    len = est_size(a);
	    bufp = (char *) fs_get(len * sizeof(char));
	    (void) addr_string(a, bufp, len);
	    a->next = next_addr;

	    /*
	     * Another assumption, all the prwids are the same.
	     */
	    if((tmp = fold(bufp,
			  ps_global->ttyo->screen_cols,
			  ps_global->ttyo->screen_cols,
			  (a == adrlist) ? (no_a_fld
					      ? fakeaddrpmt
					      : headents[address_index].prompt)
					 : tmp3+2,
			  tmp3, FLD_NONE)) != NULL){
		so_puts(store, tmp);
		fs_give((void **) &tmp);
	    }

	    fs_give((void **) &bufp);
	}
    }
    else{
	tmp2 = NULL;
	if((tmp = fold(tmp2=cpystr("<none>"),
		      ps_global->ttyo->screen_cols,
		      ps_global->ttyo->screen_cols,
		      no_a_fld ? fakeaddrpmt
			       : headents[address_index].prompt,
		      tmp3, FLD_NONE)) != NULL){
	    so_puts(store, tmp);
	    fs_give((void **) &tmp);
	}

	if(tmp2)
	  fs_give((void **)&tmp2);
    }

    if(tmp3)
      fs_give((void **)&tmp3);

    memset(&sargs, 0, sizeof(SCROLL_S));
    sargs.text.text = so_text(store);
    sargs.text.src  = CharStar;
    sargs.text.desc = _("expanded entry");
    /* TRANSLATORS: screen title for viewing an address book entry in Rich
       view mode. Rich just means it is expanded to include everything. */
    sargs.bar.title = _("ADDRESS BOOK (Rich View)");
    sargs.bar.style = TextPercent;
    sargs.keys.menu = &abook_text_km;
    setbitmap(sargs.keys.bitmap);

    scrolltool(&sargs);
    so_give(&store);

    restore_state(&state);
    ab_nesting_level--;

    if(addr)
      fs_give((void **)&addr);
    
    if(adrlist)
      mail_free_address(&adrlist);

    if(fullname)
      fs_give((void **)&fullname);

    if(abe.fullname)
      fs_give((void **)&abe.fullname);

    if(abe.tag == List && abe.addr.list)
      free_list_array(&(abe.addr.list));

    ps_global->redrawer = redraw;

    return(0);
}


/*
 * Callback from TakeAddr editing screen to see message that was being
 * viewed.  Call scrolltool to do the work.
 */
char *
view_message_for_pico(char **error)
{
    STORE_S     *store;
    gf_io_t      pc;
    void       (*redraw)(void) = ps_global->redrawer;
    SourceType   src = CharStar;
    SCROLL_S	 sargs;

    dprint((5, "- view_message_for_pico -\n"));

    ps_global->redrawer = NULL;
    fix_windsize(ps_global);

#ifdef DOS
    src = TmpFileStar;
#endif

    if(!(store = so_get(src, NULL, EDIT_ACCESS))){
	q_status_message(SM_ORDER | SM_DING, 3, 3, _("Error allocating space."));
	return(NULL);
    }

    gf_set_so_writec(&pc, store);

    format_message(msgno_for_pico_callback, env_for_pico_callback,
		   body_for_pico_callback, NULL, FM_NEW_MESS | FM_DISPLAY, pc);

    gf_clear_so_writec(store);

    memset(&sargs, 0, sizeof(SCROLL_S));
    sargs.text.text = so_text(store);
    sargs.text.src  = src;
    sargs.text.desc = _("expanded entry");
    /* TRANSLATORS: this is a screen title */
    sargs.bar.title = _("MESSAGE TEXT");
    sargs.bar.style = TextPercent;
    sargs.keys.menu = &abook_text_km;
    setbitmap(sargs.keys.bitmap);

    scrolltool(&sargs);

    so_give(&store);

    ps_global->redrawer = redraw;

    return(NULL);
}


/*
prompt::name::help::prwid::maxlen::realaddr::
builder::affected_entry::next_affected::selector::key_label::fileedit::nickcmpl
display_it::break_on_comma::is_attach::rich_header::only_file_chars::
single_space::sticky::dirty::start_here::blank::KS_ODATAVAR
*/
static struct headerentry headents_for_edit[]={
  {"Nickname  : ",  N_("Nickname"),  h_composer_abook_nick, 12, 0, NULL,
   /* TRANSLATORS: To AddrBk is a command that takes the user to
      the address book screen to select an entry from there. */
   verify_nick,   NULL, NULL, addr_book_nick_for_edit, N_("To AddrBk"), NULL, abook_nickname_complete,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Fullname  : ",  N_("Fullname"),  h_composer_abook_full, 12, 0, NULL,
   NULL,          NULL, NULL, view_message_for_pico,   N_("To Message"), NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  /* TRANSLATORS: a File Copy is a copy of a sent message saved in a regular
     file on the computer's disk */
  {"Fcc       : ",  N_("FileCopy"),  h_composer_abook_fcc, 12, 0, NULL,
   /* TRANSLATORS: To Folders */
   NULL,          NULL, NULL, folders_for_fcc,         N_("To Fldrs"), NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Comment   : ",  N_("Comment"),   h_composer_abook_comment, 12, 0, NULL,
   NULL,          NULL, NULL, view_message_for_pico,   N_("To Message"), NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Addresses : ",  N_("Addresses"), h_composer_abook_addrs, 12, 0, NULL,
   verify_addr,   NULL, NULL, addr_book_change_list,   N_("To AddrBk"), NULL, abook_nickname_complete,
   1, 1, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {NULL, NULL, NO_HELP, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE}
};
#define NNN_NICK    0
#define NNN_FULL    1
#define NNN_FCC     2
#define NNN_COMMENT 3
#define NNN_ADDR    4
#define NNN_END     5

static char        *nick_saved_for_pico_check;
static AdrBk       *abook_saved_for_pico_check;

/*
 * Args: abook  -- Address book handle
 *       abe    -- AdrBk_Entry of old entry to work on.  If NULL, this will
 *                 be a new entry.  This has to be a pointer to a copy of
 *                 an abe that won't go away until we finish this function.
 *                 In other words, don't pass in a pointer to an abe in
 *                 the cache, copy it first.  The tag on this abe is only
 *                 used to decide whether to read abe->addr.list or
 *                 abe->addr.addr, not to determine what the final result
 *                 will be.  That's determined solely by how many addresses
 *                 there are after the user edits.
 *       entry  -- The entry number of the old entry that we will be changing.
 *      old_tag -- If we're changing an old entry, then this is the tag of
 *                 that old entry.
 *     readonly -- Call pico with readonly flag
 *       warped -- We warped to a new part of the addrbook
 *                 (We also overload warped in a couple places and use it's
 *                  being set as an indicator of whether we are Taking or
 *                  not.  It will be NULL if we are Taking.)
 */
void
edit_entry(AdrBk *abook, AdrBk_Entry *abe, a_c_arg_t entry, Tag old_tag, int readonly, int *warped, char *cmd)
{
    AdrBk_Entry local_abe;
    struct headerentry *he;
    PICO pbf;
    STORE_S *msgso;
    adrbk_cntr_t old_entry_num, new_entry_num = NO_NEXT;
    int rc = 0, resort_happened = 0, list_changed = 0, which_addrbook;
    int editor_result, i = 0, add, n_end, ldap = 0;
    char *nick, *full, *fcc, *comment, *fname, *pp;
    char **orig_addrarray = NULL;
    char **new_addrarray = NULL;
    char *comma_sep_addr = NULL;
    char **p, **q;
    Tag new_tag;
    char titlebar[40];
    char nickpmt[100], fullpmt[100], fccpmt[100], cmtpmt[100], addrpmt[100]; 
    int  abook_indent;
    long length;
    SAVE_STATE_S   state;  /* For saving state of addrbooks temporarily */

    dprint((2, "- edit_entry -\n"));

    old_entry_num = (adrbk_cntr_t) entry;
    save_state(&state);
    abook_saved_for_pico_check = abook;

    add = (abe == NULL);  /* doing add or change? */
    if(add){
	local_abe.nickname  = "";
	local_abe.fullname  = "";
	local_abe.fcc       = "";
	local_abe.extra     = "";
	local_abe.addr.addr = "";
	local_abe.tag       = NotSet;
	abe = &local_abe;
	old_entry_num = NO_NEXT;
    }

    new_tag = abe->tag;

#ifdef	ENABLE_LDAP
    expander_address = NULL;
    if(abe->tag == Single &&
       abe->addr.addr &&
       !strncmp(abe->addr.addr,QRUN_LDAP,LEN_QRL)){
	ldap = 1;
	expander_address = cpystr(abe->addr.addr);
	removing_double_quotes(expander_address);
    }
    else if(abe->tag == List &&
	    abe->addr.list &&
	    abe->addr.list[0] &&
	    !abe->addr.list[1] &&
            !strncmp(abe->addr.list[0],QRUN_LDAP,LEN_QRL)){
	ldap = 2;
	expander_address = cpystr(abe->addr.list[0]);
	removing_double_quotes(expander_address);
    }
#endif

    standard_picobuf_setup(&pbf);
    pbf.exittest      = pico_sendexit_for_adrbk;
    pbf.canceltest    = warped ? pico_cancel_for_adrbk_edit
				: pico_cancel_for_adrbk_take;
    pbf.expander      = expand_addrs_for_pico;
    pbf.ctrlr_label   = _("RichView");
    /* xgettext: c-format */
    if(readonly)
      /* TRANSLATORS: screen titles */
      snprintf(titlebar, sizeof(titlebar), _("ADDRESS BOOK (View)"));
    else
      snprintf(titlebar, sizeof(titlebar), _("ADDRESS BOOK (%c%s)"),
	       islower((unsigned char)(*cmd))
				    ? toupper((unsigned char)*cmd)
				    : *cmd, (cmd+1));

    pbf.pine_anchor   = set_titlebar(titlebar,
				      ps_global->mail_stream,
				      ps_global->context_current,
				      ps_global->cur_folder,ps_global->msgmap, 
				      0, FolderName, 0, 0, NULL);
    pbf.pine_flags   |= P_NOBODY;
    if(readonly)
      pbf.pine_flags |= P_VIEW;

    /* An informational message */
    if((msgso = so_get(PicoText, NULL, EDIT_ACCESS)) != NULL){
	pbf.msgtext = (void *)so_text(msgso);
	/*
	 * It's nice if we can make it so these lines make sense even if
	 * they don't all make it on the screen, because the user can't
	 * scroll down to see them.  So just make each line a whole sentence
	 * that doesn't need the others below it to make sense.
	 */
	if(add){
	    so_puts(msgso,
/*
 * TRANSLATORS: The following lines go together to form a screen of
 * explanation about how to edit an address book entry.
 */
_("\n Fill in the fields. It is ok to leave fields blank."));
	    so_puts(msgso,
_("\n To form a list, just enter multiple comma-separated addresses."));
	}
	else{
	    so_puts(msgso,
/* TRANSLATORS: Same here, but a different version of the screen. */
_("\n Change any of the fields. It is ok to leave fields blank."));
	    if(ldap)
	      so_puts(msgso,
_("\n Since this entry does a directory lookup you may not edit the address field."));
	    else
	      so_puts(msgso,
_("\n Additional comma-separated addresses may be entered in the address field."));
	}

	so_puts(msgso,
_("\n Press \"^X\" to save the entry, \"^C\" to cancel, \"^G\" for help."));
	so_puts(msgso,
_("\n If you want to use quotation marks inside the Fullname field, it is best"));
	so_puts(msgso,
_("\n to use single quotation marks; for example: George 'Husky' Washington."));
    }

    he = (struct headerentry *) fs_get((NNN_END+1) * sizeof(struct headerentry));
    memset((void *)he, 0, (NNN_END+1) * sizeof(struct headerentry));
    pbf.headents      = he;

    abook_indent = utf8_width(_("Nickname")) + 2;

    /* make a copy of each field */
    nick = cpystr(abe->nickname ? abe->nickname : "");
    removing_leading_and_trailing_white_space(nick);
    nick_saved_for_pico_check = cpystr(nick);
    he[NNN_NICK]          = headents_for_edit[NNN_NICK];
    he[NNN_NICK].realaddr = &nick;
    utf8_snprintf(nickpmt, sizeof(nickpmt), "%-*.*w: ", abook_indent, abook_indent, _("Nickname"));
    he[NNN_NICK].prompt   = nickpmt;
    he[NNN_NICK].prwid    = abook_indent+2;
    if(F_OFF(F_ENABLE_TAB_COMPLETE,ps_global))
      he[NNN_NICK].nickcmpl = NULL;

    full = cpystr(abe->fullname ? abe->fullname : "");
    removing_leading_and_trailing_white_space(full);
    he[NNN_FULL]          = headents_for_edit[NNN_FULL];
    he[NNN_FULL].realaddr = &full;
    utf8_snprintf(fullpmt, sizeof(fullpmt), "%-*.*w: ", abook_indent, abook_indent, _("Fullname"));
    he[NNN_FULL].prompt   = fullpmt;
    he[NNN_FULL].prwid    = abook_indent+2;

    fcc = cpystr(abe->fcc ? abe->fcc : "");
    removing_leading_and_trailing_white_space(fcc);
    he[NNN_FCC]          = headents_for_edit[NNN_FCC];
    he[NNN_FCC].realaddr = &fcc;
    utf8_snprintf(fccpmt, sizeof(fccpmt), "%-*.*w: ", abook_indent, abook_indent, _("Fcc"));
    he[NNN_FCC].prompt   = fccpmt;
    he[NNN_FCC].prwid    = abook_indent+2;

    comment = cpystr(abe->extra ? abe->extra : "");
    removing_leading_and_trailing_white_space(comment);
    he[NNN_COMMENT]          = headents_for_edit[NNN_COMMENT];
    he[NNN_COMMENT].realaddr = &comment;
    utf8_snprintf(cmtpmt, sizeof(cmtpmt), "%-*.*w: ", abook_indent, abook_indent, _("Comment"));
    he[NNN_COMMENT].prompt   = cmtpmt;
    he[NNN_COMMENT].prwid    = abook_indent+2;

    n_end = NNN_END;
    if(ldap)
      n_end--;

    if(!ldap){
	he[NNN_ADDR]          = headents_for_edit[NNN_ADDR];
	he[NNN_ADDR].realaddr = &comma_sep_addr;
	utf8_snprintf(addrpmt, sizeof(addrpmt), "%-*.*w: ", abook_indent, abook_indent, _("Addresses"));
	he[NNN_ADDR].prompt   = addrpmt;
	he[NNN_ADDR].prwid    = abook_indent+2;
	if(F_OFF(F_ENABLE_TAB_COMPLETE,ps_global))
	  he[NNN_NICK].nickcmpl = NULL;

	if(abe->tag == Single){
	    if(abe->addr.addr){
		orig_addrarray = (char **) fs_get(2 * sizeof(char *));
		orig_addrarray[0] = cpystr(abe->addr.addr);
		orig_addrarray[1] = NULL;
	    }
	}
	else if(abe->tag == List){
	    if(listmem_count_from_abe(abe) > 0){
		orig_addrarray = (char **) fs_get(
				      (size_t)(listmem_count_from_abe(abe) + 1)
					* sizeof(char *));
		for(q = orig_addrarray, p = abe->addr.list; p && *p; p++, q++)
		  *q = cpystr(*p);
		
		*q = NULL;
	    }
	}

	/* figure out how large a string we need to allocate */
	length = 0L;
	for(p = orig_addrarray; p && *p; p++)
	  length += (strlen(*p) + 2);
	
	if(length)
	  length -= 2L;

	pp = comma_sep_addr = (char *) fs_get((size_t)(length+1L) * sizeof(char));
	*pp = '\0';
	for(p = orig_addrarray; p && *p; p++){
	    sstrncpy(&pp, *p, length-(pp-comma_sep_addr));
	    if(*(p+1))
	      sstrncpy(&pp, ", ", length-(pp-comma_sep_addr));
	}

	comma_sep_addr[length] = '\0';

	if(verify_addr(comma_sep_addr, NULL, NULL, NULL, NULL) < 0)
	  he[NNN_ADDR].start_here = 1;
    }

    he[n_end] = headents_for_edit[NNN_END];
    for(i = 0; i < n_end; i++){
	/* no callbacks in some cases */
	if(readonly || ((i == NNN_FULL || i == NNN_COMMENT) && !env_for_pico_callback)){
	    he[i].selector  = NULL;
	    he[i].key_label = NULL;
	}

	/* no builders for readonly */
	if(readonly)
	  he[i].builder = NULL;
    }

    /* pass to pico and let user change them */
    editor_result = pico(&pbf);
    ps_global->mangled_screen = 1;
    standard_picobuf_teardown(&pbf);

    if(editor_result & COMP_GOTHUP)
      hup_signal();
    else{
	fix_windsize(ps_global);
	init_signals();
    }

    if(editor_result & COMP_CANCEL){
	if(!readonly)
	  /* TRANSLATOR: Something like
	     Address book save cancelled */
	  q_status_message1(SM_INFO, 0, 2, _("Address book %s cancelled"), cmd);
    }
    else if(editor_result & COMP_EXIT){
	removing_leading_and_trailing_white_space(nick);
	removing_leading_and_trailing_white_space(full);
	removing_leading_and_trailing_white_space(fcc);
	removing_leading_and_trailing_white_space(comment);
	removing_leading_and_trailing_white_space(comma_sep_addr);

	/* not needed if pico is returning UTF-8 */
	convert_possibly_encoded_str_to_utf8(&nick);
	convert_possibly_encoded_str_to_utf8(&full);
	convert_possibly_encoded_str_to_utf8(&fcc);
	convert_possibly_encoded_str_to_utf8(&comment);
	convert_possibly_encoded_str_to_utf8(&comma_sep_addr);

	/* don't allow adding null entry */
	if(add && !*nick && !*full && !*fcc && !*comment && !*comma_sep_addr)
	  goto outtahere;

	/*
	 * comma_sep_addr is now the string which has been edited
	 */
	if(comma_sep_addr)
	  new_addrarray = parse_addrlist(comma_sep_addr);

	if(!ldap && (!new_addrarray || !new_addrarray[0]))
	  q_status_message(SM_ORDER, 3, 5, _("Warning: entry has no addresses"));

	if(!new_addrarray || !new_addrarray[0] || !new_addrarray[1])
	  new_tag = Single;  /* one or zero addresses means its a Single */
	else
	  new_tag = List;    /* more than one addresses means its a List */

	if(new_tag == List && old_tag == List){
	    /*
	     * If Taking, make sure we write it even if user didn't edit
	     * it any further.
	     */
	    if(!warped)
	      list_changed++;
	    else if(he[NNN_ADDR].dirty)
	      for(q = orig_addrarray, p = new_addrarray; p && *p && q && *q; p++, q++)
	        if(strcmp(*p, *q) != 0){
		    list_changed++;
		    break;
	        }
	    
	    if(!list_changed && he[NNN_ADDR].dirty
	      && ((!(p && *p) && (q && *q)) || ((p && *p) && !(q && *q))))
	      list_changed++;

	    if(list_changed){
		/*
		 * need to delete old list members and add new members below
		 */
		rc = adrbk_listdel_all(abook, (a_c_arg_t) old_entry_num);
	    }
	    else{
		/* don't need new_addrarray */
		free_list_array(&new_addrarray);
	    }

	    if(comma_sep_addr)
	      fs_give((void **) &comma_sep_addr);
	}
	else if((new_tag == List && old_tag == Single)
	       || (new_tag == Single && old_tag == List)){
	    /* delete old entry */
	    rc = adrbk_delete(abook, (a_c_arg_t) old_entry_num, 0, 0, 0, 0);
	    old_entry_num = NO_NEXT;
	    if(comma_sep_addr && new_tag == List)
	      fs_give((void **) &comma_sep_addr);
	}

	/*
	 * This will be an edit in the cases where the tag didn't change
	 * and an add in the cases where it did.
	 */
	if(rc == 0)
	  rc = adrbk_add(abook,
		       (a_c_arg_t)old_entry_num,
		       nick,
		       he[NNN_FULL].dirty ? full : abe->fullname,
		       new_tag == Single ? (ldap == 1 ? abe->addr.addr :
					      ldap == 2 ? abe->addr.list[0] :
					        comma_sep_addr)
					 : NULL,
		       fcc,
		       he[NNN_COMMENT].dirty ? comment : abe->extra,
		       new_tag,
		       &new_entry_num,
		       &resort_happened,
		       1,
		       0,
		       (new_tag != List || !new_addrarray));
    }
    
    if(rc == 0 && new_tag == List && new_addrarray)
      rc = adrbk_nlistadd(abook, (a_c_arg_t) new_entry_num, &new_entry_num,
			  &resort_happened, new_addrarray, 1, 0, 1);

    restore_state(&state);

    if(rc == -2 || rc == -3){
	q_status_message1(SM_ORDER | SM_DING, 3, 4,
			_("Error updating address book: %s"),
			rc == -2 ? error_description(errno) : "Alpine bug");
    }
    else if(rc == 0
       && strucmp(nick, nick_saved_for_pico_check) != 0
       && (editor_result & COMP_EXIT)){
	int added_to;

	for(added_to = 0; added_to < as.n_addrbk; added_to++)
	  if(abook_saved_for_pico_check == as.adrbks[added_to].address_book)
	    break;
	
	if(added_to >= as.n_addrbk)
	  added_to = -1;

	fname = addr_lookup(nick, &which_addrbook, added_to);
	if(fname){
	    q_status_message4(SM_ORDER, 5, 9,
	       /* TRANSLATORS: The first %s is the nickname the user is
	          trying to use that exists in another address book.
		  The second %s is the name of the other address book.
		  The third %s is " as " because it will say something
		  like also exists in <name> as <description>. */
	       _("Warning! Nickname %s also exists in \"%s\"%s%s"),
		nick, as.adrbks[which_addrbook].abnick,
		(fname && *fname) ? _(" as ") : "",
		(fname && *fname) ? fname : "");
	    fs_give((void **)&fname);
	}
    }

    if(resort_happened || list_changed){
	DL_CACHE_S dlc_restart;

	dlc_restart.adrbk_num = as.cur;
	dlc_restart.dlcelnum  = new_entry_num;
	switch(new_tag){
	  case Single:
	    dlc_restart.type = DlcSimple;
	    break;
	  
	  case List:
	    dlc_restart.type = DlcListHead;
	    break;

	  default:
	    break;
	}

	warp_to_dlc(&dlc_restart, 0L);
	if(warped)
	  *warped = 1;
    }

outtahere:
    if(he)
      free_headents(&he);

    if(msgso)
      so_give(&msgso);

    if(nick)
      fs_give((void **)&nick);
    if(full)
      fs_give((void **)&full);
    if(fcc)
      fs_give((void **)&fcc);
    if(comment)
      fs_give((void **)&comment);

    if(comma_sep_addr)
      fs_give((void **)&comma_sep_addr);
    if(nick_saved_for_pico_check)
      fs_give((void **)&nick_saved_for_pico_check);

    free_list_array(&orig_addrarray);
    free_list_array(&new_addrarray);
#ifdef	ENABLE_LDAP
    if(expander_address)
      fs_give((void **) &expander_address);
#endif
}


/*ARGSUSED*/
int
verify_nick(char *given, char **expanded, char **error, BUILDER_ARG *fcc, int *mangled)
{
    char *tmp;

    dprint((7, "- verify_nick - (%s)\n", given ? given : "nul"));

    tmp = cpystr(given);
    removing_leading_and_trailing_white_space(tmp);

    if(nickname_check(tmp, error)){
	fs_give((void **)&tmp);
	if(mangled){
	    if(ps_global->mangled_screen)
	      *mangled |= BUILDER_SCREEN_MANGLED;
	    else if(ps_global->mangled_footer)
	      *mangled |= BUILDER_FOOTER_MANGLED;
	}

	return -2;
    }

    if(ps_global->remote_abook_validity > 0 &&
       adrbk_check_and_fix_all(ab_nesting_level == 0, 0, 0) && mangled)
      *mangled |= BUILDER_SCREEN_MANGLED;

    ab_nesting_level++;
    if(strucmp(tmp, nick_saved_for_pico_check) != 0
       && adrbk_lookup_by_nick(abook_saved_for_pico_check,
			       tmp, (adrbk_cntr_t *)NULL)){
	if(error){
	    char buf[MAX_NICKNAME + 80];

	    /* TRANSLATORS: The %s is the nickname of an entry that is already
	       in the address book */
	    snprintf(buf, sizeof(buf), _("\"%s\" already in address book."), tmp);
	    buf[sizeof(buf)-1] = '\0';
	    *error = cpystr(buf);
	}
	
	ab_nesting_level--;
	fs_give((void **)&tmp);
	if(mangled){
	    if(ps_global->mangled_screen)
	      *mangled |= BUILDER_SCREEN_MANGLED;
	    else if(ps_global->mangled_footer)
	      *mangled |= BUILDER_FOOTER_MANGLED;
	}
	
	return -2;
    }

    ab_nesting_level--;
    if(expanded)
      *expanded = tmp;
    else
      fs_give((void **)&tmp);

    /* This is so pico will erase any old message */
    if(error)
      *error = cpystr("");

    if(mangled){
	if(ps_global->mangled_screen)
	  *mangled |= BUILDER_SCREEN_MANGLED;
	else if(ps_global->mangled_footer)
	  *mangled |= BUILDER_FOOTER_MANGLED;
    }

    return 0;
}


/*
 * Args: to      -- the passed in line to parse
 *       full_to -- Address of a pointer to return the full address in.
 *		    This will be allocated here and freed by the caller.
 *                  However, this function is just going to copy "to".
 *                  (special case for the route-addr-hack in build_address_int)
 *                  We're just looking for the error messages.
 *       error   -- Address of a pointer to return an error message in.
 *		    This will be allocated here and freed by the caller.
 *       fcc     -- This should be passed in NULL.
 *                  This builder doesn't support affected_entry's.
 *
 * Result:  0 is returned if address was OK, 
 *         -2 if address wasn't OK.
 *
 * Side effect: Can flush addrbook entry cache entries so they need to be
 * re-fetched afterwords.
 */
int
verify_addr(char *to, char **full_to, char **error, BUILDER_ARG *fcc, int *mangled)
{
    register char *p;
    int            ret_val;
    BuildTo        bldto;
    jmp_buf        save_jmp_buf;
    int           *save_nesting_level;

    dprint((7, "- verify_addr - (%s)\n", to ? to : "nul"));

    /* check to see if to string is empty to avoid work */
    for(p = to; p && *p && isspace((unsigned char)(*p)); p++)
      ;/* do nothing */

    if(!p || !*p){
	if(full_to)
	  *full_to = cpystr(to ? to : "");  /* because pico does a strcmp() */

	return 0;
    }

    if(full_to != NULL)
      *full_to = (char *)NULL;

    if(error != NULL)
      *error = (char *)NULL;
    
    /*
     * If we end up jumping back here because somebody else changed one of
     * our addrbooks out from underneath us, we may well leak some memory.
     * That's probably ok since this will be very rare.
     */
    memcpy(save_jmp_buf, addrbook_changed_unexpectedly, sizeof(jmp_buf));
    save_nesting_level = cpyint(ab_nesting_level);
    if(setjmp(addrbook_changed_unexpectedly)){
	if(full_to && *full_to)
	  fs_give((void **)full_to);

	/* TRANSLATORS: This is sort of an error, something unexpected has
	   happened and alpine is re-initializing the address book. */
	q_status_message(SM_ORDER, 3, 5, _("Resetting address book..."));
	dprint((1,
	    "RESETTING address book... verify_addr(%s)!\n", to ? to : "?"));
	addrbook_reset();
	ab_nesting_level = *save_nesting_level;
    }

    bldto.type    = Str;
    bldto.arg.str = to;

    if(ps_global->remote_abook_validity > 0 &&
       adrbk_check_and_fix_all(ab_nesting_level == 0, 0, 0) && mangled)
      *mangled |= BUILDER_SCREEN_MANGLED;

    ab_nesting_level++;

    ret_val = build_address_internal(bldto, full_to, error, NULL, NULL, NULL,
				     save_and_restore, 1, mangled);

    ab_nesting_level--;
    if(save_nesting_level)
      fs_give((void **)&save_nesting_level);

    if(full_to && *full_to && ret_val >= 0)
      removing_leading_and_trailing_white_space(*full_to);

    /* This is so pico will erase the old message */
    if(error != NULL && *error == NULL)
      *error = cpystr("");

    if(ret_val < 0)
      ret_val = -2;  /* cause pico to stay on same header line */

    memcpy(addrbook_changed_unexpectedly, save_jmp_buf, sizeof(jmp_buf));
    return(ret_val);
}


/*
 *  Call back for pico to prompt the user for exit confirmation
 *
 * Returns: either NULL if the user accepts exit, or string containing
 *	 reason why the user declined.
 */      
int
pico_sendexit_for_adrbk(struct headerentry *he, void (*redraw_pico)(void),
			int allow_flowed, char **result)
{
    char *rstr = NULL;
    void (*redraw)(void) = ps_global->redrawer;

    ps_global->redrawer = redraw_pico;
    fix_windsize(ps_global);
    
    /* TRANSLATORS: A question */
    switch(want_to(_("Exit and save changes "), 'y', 0, NO_HELP, WT_NORM)){
      case 'y':
	break;

      case 'n':
	rstr = _("Use ^C to abandon changes you've made");
	break;
    }

    if(result)
      *result = rstr;

    ps_global->redrawer = redraw;
    return((rstr == NULL) ? 0 : 1);
}


/*
 *  Call back for pico to prompt the user for exit confirmation
 *
 * Returns: either NULL if the user accepts exit, or string containing
 *	 reason why the user declined.
 */      
char *
pico_cancelexit_for_adrbk(char *word, void (*redraw_pico)(void))
{
    char prompt[90];
    char *rstr = NULL;
    void (*redraw)(void) = ps_global->redrawer;

    /* TRANSLATORS: A question. The %s is a noun describing what is being cancelled. */
    snprintf(prompt, sizeof(prompt), _("Cancel %s (answering \"Yes\" will abandon any changes made) "), word);
    ps_global->redrawer = redraw_pico;
    fix_windsize(ps_global);
    
    switch(want_to(prompt, 'y', 'x', NO_HELP, WT_NORM)){
      case 'y':
	rstr = "";
	break;

      case 'n':
      case 'x':
	break;
    }

    ps_global->redrawer = redraw;
    return(rstr);
}


char *
pico_cancel_for_adrbk_take(void (*redraw_pico)(void))
{
    return(pico_cancelexit_for_adrbk(_("take"), redraw_pico));
}


char *
pico_cancel_for_adrbk_edit(void (*redraw_pico)(void))
{
    return(pico_cancelexit_for_adrbk(_("changes"), redraw_pico));
}


/*
prompt::name::help::prwid::maxlen::realaddr::
builder::affected_entry::next_affected::selector::key_label::fileedit::
display_it::break_on_comma::is_attach::rich_header::only_file_chars::
single_space::sticky::dirty::start_here::blank::KS_ODATAVAR
*/
static struct headerentry headents_for_add[]={
  {"Server Name : ",  N_("Server"),  h_composer_abook_add_server, 14, 0, NULL,
   verify_server_name, NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Folder Name : ",  N_("Folder"),  h_composer_abook_add_folder, 14, 0, NULL,
   verify_folder_name, NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"NickName    : ",  N_("Nickname"),  h_composer_abook_add_nick, 14, 0, NULL,
   verify_abook_nick,  NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {NULL, NULL, NO_HELP, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE}
};
#define NN_SERVER 0
#define NN_FOLDER 1
#define NN_NICK   2
#define NN_END    3

/*
 * Args:             global -- Add a global address book, not personal.
 *           add_after_this -- This is the addrbook number which should come
 *                             right before the new addrbook we're adding, if
 *                             that makes sense. If this is -1, append to end
 *                             of the list.
 *
 * Returns:  addrbook number of new addrbook, or
 *           -1, no addrbook added
 */
int
ab_add_abook(int global, int add_after_this)
{
    int ret;

    dprint((2, "- ab_add_abook -\n"));

    ret = ab_modify_abook_list(0, global, add_after_this, NULL, NULL, NULL);

    if(ret >= 0)
      q_status_message(SM_ORDER, 0, 3,
		       _("New address book added. Use \"$\" to adjust order"));
    
    return(ret);
}


/*
 * Args:          global -- Add a global address book, not personal.
 *             abook_num -- Abook num of the entry we are editing.
 *                  serv -- Default server.
 *                folder -- Default folder.
 *                  nick -- Default nickname.
 *
 * Returns:  abook_num if successful,
 *           -1, if not
 */
int
ab_edit_abook(int global, int abook_num, char *serv, char *folder, char *nick)
{
    dprint((2, "- ab_edit_abook -\n"));

    return(ab_modify_abook_list(1, global, abook_num, serv, folder, nick));
}

static int the_one_were_editing;


/*
 * Args:            edit -- Edit existing entry
 *                global -- Add a global address book, not personal.
 *             abook_num -- This is the addrbook number which should come
 *                            right before the new addrbook we're adding, if
 *                            that makes sense. If this is -1, append to end
 *                            of the list.
 *                            If we are editing instead of adding, this is
 *                            the abook number of the entry we are editing.
 *              def_serv -- Default server.
 *              def_fold -- Default folder.
 *              def_nick -- Default nickname.
 *
 * Returns:  addrbook number of new addrbook, or
 *           -1, no addrbook added
 */
int
ab_modify_abook_list(int edit, int global, int abook_num, char *def_serv, char *def_fold, char *def_nick)
{
    struct headerentry *he;
    PICO pbf;
    STORE_S *msgso;
    int editor_result, i, how_many_in_list, new_abook_num, num_in_list;
    int ret = 0;
    char *server, *folder, *nickname;
    char *new_item = NULL;
    EditWhich ew;
    AccessType remember_access_result;
    PerAddrBook *pab;
    char titlebar[100];
    char         **list, **new_list = NULL;
    char         tmp[1000+MAXFOLDER];
    int abook_indent;
    char servpmt[100], foldpmt[100], nickpmt[100];
    struct variable *vars = ps_global->vars;

    dprint((2, "- ab_modify_abook_list -\n"));

    if(ps_global->readonly_pinerc){
	if(edit)
	  /* TRANSLATORS: Change was the name of the command the user was
	     trying to perform. It is what was cancelled. */
	  q_status_message(SM_ORDER, 0, 3, _("Change cancelled: config file not changeable"));
	else
	  /* TRANSLATORS: Add was the command that is being cancelled. */
	  q_status_message(SM_ORDER, 0, 3, _("Add cancelled: config file not changeable"));

	return -1;
    }

    ew = Main;

    if((global && vars[V_GLOB_ADDRBOOK].is_fixed) ||
       (!global && vars[V_ADDRESSBOOK].is_fixed)){
	if(global)
	  /* TRANSLATORS: Operation was cancelled because the system management
	     does not allow the changing of global address books */
	  q_status_message(SM_ORDER, 0, 3, _("Cancelled: Sys. Mgmt. does not allow changing global address books"));
	else
	  q_status_message(SM_ORDER, 0, 3, _("Cancelled: Sys. Mgmt. does not allow changing address books"));
	
	return -1;
    }

    init_ab_if_needed();

    if(edit){
	if((!global &&
		(abook_num < 0 || abook_num >= as.how_many_personals)) ||
	   (global &&
	        (abook_num < as.how_many_personals ||
		    abook_num >= as.n_addrbk))){
	    dprint((1, "Programming botch in ab_modify_abook_list: global=%d abook_num=%d n_addrbk=%d\n", global, abook_num, as.n_addrbk));
	    q_status_message(SM_ORDER, 0, 3, "Programming botch, bad abook_num");
	    return -1;
	}

	the_one_were_editing = abook_num;
    }
    else
      the_one_were_editing = -1;

    standard_picobuf_setup(&pbf);
    pbf.exittest      = pico_sendexit_for_adrbk;
    pbf.canceltest    = pico_cancel_for_adrbk_edit;
    if(edit)
      /* TRANSLATORS: screen title */
      strncpy(titlebar, _("CHANGE ADDRESS BOOK"), sizeof(titlebar));
    else
      /* TRANSLATORS: screen title */
      strncpy(titlebar, _("ADD ADDRESS BOOK"), sizeof(titlebar));

    titlebar[sizeof(titlebar)-1] = '\0';
    pbf.pine_anchor   = set_titlebar(titlebar,
				      ps_global->mail_stream,
				      ps_global->context_current,
				      ps_global->cur_folder,ps_global->msgmap, 
				      0, FolderName, 0, 0, NULL);
    pbf.pine_flags   |= P_NOBODY;

    /* An informational message */
    if((msgso = so_get(PicoText, NULL, EDIT_ACCESS)) != NULL){
	int lines_avail;
char *t1 =
/* TRANSLATORS: The next few lines go together to explain how to add
   the address book entry the user is working on. */
_(" To add a local address book that will be accessed *only* by Alpine running\n on this machine, leave the server field blank.");
char *t2 =
_(" To add an address book that will be accessed by IMAP, fill in the\n server name.");
char *t3 =
_(" (NOTE: An address book cannot be accessed by IMAP from\n one Alpine and as a local file from another. It is either always accessed\n by IMAP or it is a local address book which is never accessed by IMAP.)");
char *t4 =
_(" In the Folder field, type the remote folder name or local file name.");
char *t5 =
_(" In the Nickname field, give the address book a nickname or leave it blank.");
char *t6 =
_(" To get help specific to an item, press ^G.");
char *t7 =
_(" To exit and save the configuration, press ^X. To cancel, press ^C.");

	pbf.msgtext = (void *)so_text(msgso);
	/*
	 * It's nice if we can make it so these lines make sense even if
	 * they don't all make it on the screen, because the user can't
	 * scroll down to see them.
	 *
	 * The 3 is the number of fields to be defined, the 1 is for a
	 * single blank line after the field definitions.
	 */
	lines_avail = ps_global->ttyo->screen_rows - HEADER_ROWS(ps_global) -
			FOOTER_ROWS(ps_global) - 3 - 1;

	if(lines_avail >= 15){		/* extra blank line */
	    so_puts(msgso, "\n");
	    lines_avail--;
	}

	if(lines_avail >= 2){
	    so_puts(msgso, t1);
	    lines_avail -= 2;
	}

	if(lines_avail >= 5){
	    so_puts(msgso, "\n\n");
	    so_puts(msgso, t2);
	    so_puts(msgso, t3);
	    lines_avail -= 5;
	}
	else if(lines_avail >= 3){
	    so_puts(msgso, "\n\n");
	    so_puts(msgso, t2);
	    lines_avail -= 3;
	}

	if(lines_avail >= 2){
	    so_puts(msgso, "\n\n");
	    so_puts(msgso, t4);
	    lines_avail -= 2;
	}

	if(lines_avail >= 2){
	    so_puts(msgso, "\n\n");
	    so_puts(msgso, t5);
	    lines_avail -= 2;
	}

	if(lines_avail >= 3){
	    so_puts(msgso, "\n\n");
	    so_puts(msgso, t6);
	    so_puts(msgso, "\n");
	    so_puts(msgso, t7);
	}
	else if(lines_avail >= 2){
	    so_puts(msgso, "\n\n");
	    so_puts(msgso, t7);
	}
    }

    he = (struct headerentry *)fs_get((NN_END+1) * sizeof(struct headerentry));
    memset((void *)he, 0, (NN_END+1) * sizeof(struct headerentry));
    pbf.headents      = he;

    abook_indent = utf8_width(_("Server Name")) + 2;

    /* make a copy of each field */
    server = cpystr(def_serv ? def_serv : "");
    he[NN_SERVER]          = headents_for_add[NN_SERVER];
    he[NN_SERVER].realaddr = &server;
    utf8_snprintf(servpmt, sizeof(servpmt), "%-*.*w: ", abook_indent, abook_indent, _("Server Name"));
    he[NN_SERVER].prompt   = servpmt;
    he[NN_SERVER].prwid    = abook_indent+2;

    folder = cpystr(def_fold ? def_fold : "");
    he[NN_FOLDER]          = headents_for_add[NN_FOLDER];
    he[NN_FOLDER].realaddr = &folder;
    utf8_snprintf(foldpmt, sizeof(foldpmt), "%-*.*w: ", abook_indent, abook_indent, _("Folder Name"));
    he[NN_FOLDER].prompt   = foldpmt;
    he[NN_FOLDER].prwid    = abook_indent+2;

    nickname = cpystr(def_nick ? def_nick : "");
    he[NN_NICK]          = headents_for_add[NN_NICK];
    he[NN_NICK].realaddr = &nickname;
    utf8_snprintf(nickpmt, sizeof(nickpmt), "%-*.*w: ", abook_indent, abook_indent, _("Nickname"));
    he[NN_NICK].prompt   = nickpmt;
    he[NN_NICK].prwid    = abook_indent+2;

    he[NN_END] = headents_for_add[NN_END];

    /* pass to pico and let user change them */
    editor_result = pico(&pbf);
    standard_picobuf_teardown(&pbf);

    if(editor_result & COMP_GOTHUP){
	ret = -1;
	hup_signal();
    }
    else{
	fix_windsize(ps_global);
	init_signals();
    }

    if(editor_result & COMP_CANCEL){
	ret = -1;
	if(edit)
	  q_status_message(SM_ORDER, 0, 3, _("Address book change is cancelled"));
	else
	  q_status_message(SM_ORDER, 0, 3, _("Address book add is cancelled"));
    }
    else if(editor_result & COMP_EXIT){
	if(edit &&
	   !strcmp(server, def_serv ? def_serv : "") &&
	   !strcmp(folder, def_fold ? def_fold : "") &&
	   !strcmp(nickname, def_nick ? def_nick : "")){
	    ret = -1;
	    if(edit)
	      q_status_message(SM_ORDER, 0, 3, _("No change: Address book change is cancelled"));
	    else
	      q_status_message(SM_ORDER, 0, 3, _("No change: Address book add is cancelled"));
	}
	else{
	    if(global){
		list = VAR_GLOB_ADDRBOOK;
		how_many_in_list = as.n_addrbk - as.how_many_personals;
		if(edit)
		  new_abook_num = abook_num;
		else if(abook_num < 0)
		  new_abook_num  = as.n_addrbk;
		else
		  new_abook_num  = MAX(MIN(abook_num + 1, as.n_addrbk),
				       as.how_many_personals);

		num_in_list      = new_abook_num - as.how_many_personals;
	    }
	    else{
		list = VAR_ADDRESSBOOK;
		how_many_in_list = as.how_many_personals;
		new_abook_num = abook_num;
		if(edit)
		  new_abook_num = abook_num;
		else if(abook_num < 0)
		  new_abook_num  = as.how_many_personals;
		else
		  new_abook_num  = MIN(abook_num + 1, as.how_many_personals);

		num_in_list      = new_abook_num;
	    }

	    if(!edit)
	      how_many_in_list++;		/* for new abook */

	    removing_leading_and_trailing_white_space(server);
	    removing_leading_and_trailing_white_space(folder);
	    removing_leading_and_trailing_white_space(nickname);

	    /* convert nickname to UTF-8 */
	    if(nickname){
		char *conv;

		conv = convert_to_utf8(nickname, NULL, 0);
		if(conv){
		    fs_give((void **) &nickname);
		    nickname = conv;
		}
	    }

	    /* eliminate surrounding brackets */
	    if(server[0] == '{' && server[strlen(server)-1] == '}'){
		char *p;

		server[strlen(server)-1] = '\0';
		for(p = server; *p; p++)
		  *p = *(p+1);
	    }

	    snprintf(tmp, sizeof(tmp), "%s%s%s%.*s",
		    *server ? "{" : "",
		    *server ? server : "",
		    *server ? "}" : "",
		    MAXFOLDER, folder);
	    tmp[sizeof(tmp)-1] = '\0';
	    
	    new_item = put_pair(nickname, tmp);

	    if(!new_item || *new_item == '\0'){
		if(edit)
		  q_status_message(SM_ORDER, 0, 3, _("Address book change is cancelled"));
		else
		  q_status_message(SM_ORDER, 0, 3, _("Address book add is cancelled"));

		ret = -1;
		goto get_out;
	    }

	    /* allocate for new list */
	    new_list = (char **)fs_get((how_many_in_list + 1) * sizeof(char *));

	    /* copy old list up to where we will insert new entry */
	    for(i = 0; i < num_in_list; i++)
	      new_list[i] = cpystr(list[i]);
	    
	    /* insert the new entry */
	    new_list[i++] = cpystr(new_item);

	    /* copy rest of old list, skip current if editing */
	    for(; i < how_many_in_list; i++)
	      new_list[i] = cpystr(list[edit ? i : (i-1)]);

	    new_list[i] = NULL;

	    /* this frees old variable contents for us */
	    if(set_variable_list(global ? V_GLOB_ADDRBOOK : V_ADDRESSBOOK,
				 new_list, TRUE, ew)){
		if(edit)
		  q_status_message(SM_ORDER, 0, 3, _("Change cancelled: couldn't save configuration file"));
		else
		  q_status_message(SM_ORDER, 0, 3, _("Add cancelled: couldn't save configuration file"));

		set_current_val(&vars[global ? V_GLOB_ADDRBOOK : V_ADDRESSBOOK],
				TRUE, FALSE);
		ret = -1;
		goto get_out;
	    }

	    ret = new_abook_num;
	    set_current_val(&vars[global ? V_GLOB_ADDRBOOK : V_ADDRESSBOOK],
			    TRUE, FALSE);

	    addrbook_reset();
	    init_ab_if_needed();

	    /*
	     * Test to see if this definition is going to work.
	     * Error messages are a good side effect.
	     */
	    pab = &as.adrbks[num_in_list];
	    init_abook(pab, NoDisplay);
	    remember_access_result = pab->access;
	    addrbook_reset();
	    init_ab_if_needed();
	    /* if we had trouble, give a clue to user (other than error msg) */
	    if(remember_access_result == NoAccess){
		pab = &as.adrbks[num_in_list];
		pab->access = remember_access_result;
	    }
	}
    }
    
get_out:

    if(he)
      free_headents(&he);

    if(new_list)
      free_list_array(&new_list);

    if(new_item)
      fs_give((void **)&new_item);

    if(msgso)
      so_give(&msgso);

    if(server)
      fs_give((void **)&server);
    if(folder)
      fs_give((void **)&folder);
    if(nickname)
      fs_give((void **)&nickname);
    
    return(ret);
}


int
any_addrbooks_to_convert(struct pine *ps)
{
    PerAddrBook *pab;
    int          i, count = 0;

    init_ab_if_needed();

    for(i = 0; i < as.n_addrbk; i++){
	pab = &as.adrbks[i];
	if(pab && !(pab->type & REMOTE_VIA_IMAP) && !(pab->type & GLOBAL))
	  count++;
    }

    return(count);
}


int
convert_addrbooks_to_remote(struct pine *ps, char *rem_folder_prefix, size_t len)
{
    PerAddrBook *pab;
    int          i, count = 0, ret = 0;

    init_ab_if_needed();

    for(i = 0; i < as.n_addrbk; i++){
	pab = &as.adrbks[i];
	if(pab && !(pab->type & REMOTE_VIA_IMAP) && !(pab->type & GLOBAL))
	  count++;
    }

    for(i = 0; ret != -1 && i < as.n_addrbk; i++){
	pab = &as.adrbks[i];
	if(pab && !(pab->type & REMOTE_VIA_IMAP) && !(pab->type & GLOBAL))
	  ret = convert_abook_to_remote(ps, pab, rem_folder_prefix, len, count);
    }

    return(ret);
}


/*
 * Returns -1 if cancelled, -2 on error, 0 otherwise.
 */
int
convert_abook_to_remote(struct pine *ps, PerAddrBook *pab, char *rem_folder_prefix, size_t len, int count)
{
#define DEF_ABOOK_NAME "remote_addrbook"
    char  local_file[MAILTMPLEN];
    char  rem_abook[MAILTMPLEN+3], prompt[MAILTMPLEN], old_nick[MAILTMPLEN];
    char *p = NULL, *err_msg = NULL, *q;
    char *serv = NULL, *nick = NULL, *file = NULL, *folder = NULL;
    int   ans, rc, offset, i, abook_num = -1, flags = OE_APPEND_CURRENT;
    HelpType help;

    snprintf(old_nick, sizeof(old_nick), "%s%s%s",
	    count > 1 ? " \"" : "",
	    count > 1 ? pab->abnick : "",
	    count > 1 ? "\"" : "");
    old_nick[sizeof(old_nick)-1] = '\0';

    snprintf(prompt, sizeof(prompt), _("Convert addressbook%s to a remote addrbook "), old_nick);
    prompt[sizeof(prompt)-1] = '\0';
    if((ans=want_to(prompt, 'y', 'x', h_convert_abook, WT_NORM)) != 'y')
      return(ans == 'n' ? 0 : -1);

    /* make sure the addrbook has been opened before, so that the file exists */
    if(pab->ostatus == Closed || pab->ostatus == HalfOpen){
	(void)init_addrbooks(NoDisplay, 0, 0, 0);
	(void)init_addrbooks(Closed, 0, 0, 0);
    }

    if(pab->filename){
	strncpy(local_file, pab->filename, sizeof(local_file)-1);
	local_file[sizeof(local_file)-1] = '\0';
#if	defined(DOS)
	p = strrindex(pab->filename, '\\');
#else
	p = strrindex(pab->filename, '/');
#endif
    }

    strncpy(rem_abook, rem_folder_prefix, sizeof(rem_abook)-3);
    if(!*rem_abook){
	/* TRANSLATORS: The user is defining an address book which will be
	   stored on another server. This is called a remote addrbook and
	   this is a question asking for the name of the server. */
	snprintf(prompt, sizeof(prompt), _("Name of server to contain remote addrbook : "));
	prompt[sizeof(prompt)-1] = '\0';
	help = NO_HELP;
	while(1){
	    rc = optionally_enter(rem_abook, -FOOTER_ROWS(ps), 0,
				  sizeof(rem_abook), prompt, NULL,
				  help, &flags);
	    removing_leading_and_trailing_white_space(rem_abook);
	    if(rc == 3){
		help = help == NO_HELP ? h_convert_pinerc_server : NO_HELP;
	    }
	    else if(rc == 1){
		cmd_cancelled(NULL);
		return(-1);
	    }
	    else if(rc == 0){
		if(*rem_abook){
		    /* add brackets */
		    offset = strlen(rem_abook);
		    for(i = offset; i >= 0; i--)
		      rem_abook[i+1] = rem_abook[i];
		    
		    rem_abook[0] = '{';
		    rem_abook[++offset] = '}';
		    rem_abook[++offset] = '\0';
		    break;
		}
	    }
	}
    }

    if(*rem_abook){
	if(p && count > 1)
	  strncat(rem_abook, p+1,
		  sizeof(rem_abook)-1-strlen(rem_abook));
	else
	  strncat(rem_abook, DEF_ABOOK_NAME,
		  sizeof(rem_abook)-1-strlen(rem_abook));
    }

    if(*rem_abook){
	file = cpystr(rem_abook);
	if(pab->abnick){
	    nick = (char *)fs_get((MAX(strlen(pab->abnick),strlen("Address Book"))+8) * sizeof(char));
	    snprintf(nick, sizeof(nick), "Remote %s",
	            (pab->abnick && !strcmp(pab->abnick, DF_ADDRESSBOOK))
			? "Address Book" : pab->abnick);
	    nick[sizeof(nick)-1] = '\0';
	}
	else
	  nick = cpystr("Remote Address Book");
	
	if(file && *file == '{'){
	    q = file + 1;
	    if((p = strindex(file, '}'))){
		*p = '\0';
		serv = q;
		folder = p+1;
	    }
	    else if(file)
	      fs_give((void **)&file);
	}
	else
	  folder = file;
    }
	
    q_status_message(SM_ORDER, 3, 5,
       _("You now have a chance to change the name of the remote addrbook..."));
    abook_num = ab_modify_abook_list(0, 0, -1, serv, folder, nick);

    /* extract folder name of new abook so we can copy to it */
    if(abook_num >= 0){
	char    **lval;
	EditWhich ew = Main;

	lval = LVAL(&ps->vars[V_ADDRESSBOOK], ew);
	get_pair(lval[abook_num], &nick, &file, 0, 0);
	if(nick)
	  fs_give((void **)&nick);
	
	if(file){
	    strncpy(rem_abook, file, sizeof(rem_abook)-1);
	    rem_abook[sizeof(rem_abook)-1] = '\0';
	    fs_give((void **)&file);
	}
    }

    /* copy the abook */
    if(abook_num >= 0 && copy_abook(local_file, rem_abook, &err_msg)){
	if(err_msg){
	    q_status_message(SM_ORDER | SM_DING, 7, 10, err_msg);
	    fs_give((void **)&err_msg);
	}

	return(-2);
    }
    else if(abook_num >= 0){			/* give user some info */
	STORE_S  *store;
	SCROLL_S  sargs;
	char     *beg, *end;

	/*
	 * Save the hostname in rem_folder_prefix so we can use it again
	 * for other conversions if needed.
	 */
	if((beg = rem_abook)
	   && (*beg == '{' || (*beg == '*' && *++beg == '{'))
	   && (end = strindex(rem_abook, '}'))){
	    rem_folder_prefix[0] = '{';
	    strncpy(rem_folder_prefix+1, beg+1, MIN(end-beg,len-2));
	    rem_folder_prefix[MIN(end-beg,len-2)] = '}';
	    rem_folder_prefix[MIN(end-beg+1,len-1)] = '\0';
	}

	if(!(store = so_get(CharStar, NULL, EDIT_ACCESS))){
	    q_status_message(SM_ORDER | SM_DING, 7, 10,
			     _("Error allocating space for message."));
	    return(-2);
	}

	/* TRANSLATORS: Several lines in a row here that go together. */
	snprintf(prompt, sizeof(prompt), _("\nYour addressbook%s has been copied to the"), old_nick);
	prompt[sizeof(prompt)-1] = '\0';
	so_puts(store, prompt);
	so_puts(store, _("\nremote folder \""));
	so_puts(store, rem_abook);
	so_puts(store, "\".");
	so_puts(store, _("\nA definition for this remote address book has been added to your list"));
	so_puts(store, _("\nof address books. The definition for the address book it was copied"));
	so_puts(store, _("\nfrom is also still there. You may want to remove that after you"));
	so_puts(store, _("\nare confident that the new address book is complete and working."));
	so_puts(store, _("\nUse the Setup/AddressBooks command to do that.\n"));

	memset(&sargs, 0, sizeof(SCROLL_S));
	sargs.text.text  = so_text(store);
	sargs.text.src   = CharStar;
	sargs.text.desc  = _("Remote Address Book Information");
	/* TRANSLATORS: a screen title */
	sargs.bar.title  = _("ABOUT REMOTE ABOOK");
	sargs.help.text  = NO_HELP;
	sargs.help.title = NULL;

	scrolltool(&sargs);

	so_give(&store);	/* free resources associated with store */
	ps->mangled_screen = 1;
    }

    return(0);
}


int
any_sigs_to_convert(struct pine *ps)
{
    char       *sigfile, *litsig;
    long        rflags;
    PAT_STATE   pstate;
    PAT_S      *pat;
    PAT_LINE_S *patline;

    /* first check main signature file */
    sigfile = ps->VAR_SIGNATURE_FILE;
    litsig = ps->VAR_LITERAL_SIG;

    if(sigfile && *sigfile && !litsig && sigfile[strlen(sigfile)-1] != '|' &&
       !IS_REMOTE(sigfile))
      return(1);

    rflags = (ROLE_DO_ROLES | PAT_USE_MAIN);
    if(any_patterns(rflags, &pstate)){
	set_pathandle(rflags);
	for(patline = *cur_pat_h ?  (*cur_pat_h)->patlinehead : NULL;
	    patline; patline = patline->next){
	    for(pat = patline->first; pat; pat = pat->next){

		/*
		 * See detoken() for when a sig file is used with a role.
		 */
		sigfile = pat->action ? pat->action->sig : NULL;
		litsig = pat->action ? pat->action->litsig : NULL;

		if(sigfile && *sigfile && !litsig &&
		   sigfile[strlen(sigfile)-1] != '|' &&
		   !IS_REMOTE(sigfile))
		  return(1);
	    }
	}
    }

    return(0);
}


int
any_rule_files_to_warn_about(struct pine *ps)
{
    long        rflags;
    PAT_STATE   pstate;
    PAT_S      *pat;

    rflags = (ROLE_DO_ROLES | ROLE_DO_INCOLS | ROLE_DO_SCORES |
	      ROLE_DO_FILTER | ROLE_DO_OTHER | ROLE_DO_SRCH | PAT_USE_MAIN);
    if(any_patterns(rflags, &pstate)){
	for(pat = first_pattern(&pstate);
	    pat;
	    pat = next_pattern(&pstate)){
	    if(pat->patline && pat->patline->type == File)
	      break;
	}

	if(pat)
	  return(1);
    }

    return(0);
}


int
convert_sigs_to_literal(struct pine *ps, int interactive)
{
    EditWhich   ew = Main;
    char       *sigfile, *litsig, *cstring_version, *nick, *src = NULL;
    char        prompt[MAILTMPLEN];
    STORE_S    *store;
    SCROLL_S	sargs;
    long        rflags;
    int         ans;
    PAT_STATE   pstate;
    PAT_S      *pat;
    PAT_LINE_S *patline;

    /* first check main signature file */
    sigfile = ps->VAR_SIGNATURE_FILE;
    litsig = ps->VAR_LITERAL_SIG;

    if(sigfile && *sigfile && !litsig && sigfile[strlen(sigfile)-1] != '|' &&
       !IS_REMOTE(sigfile)){
	if(interactive){
	    snprintf(prompt,sizeof(prompt),
		    /* TRANSLATORS: A literal sig is a way to store a signature
		       in alpine. It isn't a very descriptive name. Instead of
		       storing it in its own file it is stored in the configuration
		       file. */
		    _("Convert signature file \"%s\" to a literal sig "),
		    sigfile);
	    prompt[sizeof(prompt)-1] = '\0';
	    ClearBody();
	    ps->mangled_body = 1;
	    if((ans=want_to(prompt, 'y', 'x', h_convert_sig, WT_NORM)) == 'x'){
		cmd_cancelled(NULL);
		return(-1);
	    }
	}
	else
	  ans = 'y';

	if(ans == 'y' && (src = get_signature_file(sigfile, 0, 0, 0)) != NULL){
	    cstring_version = string_to_cstring(src);
	    set_variable(V_LITERAL_SIG, cstring_version, 0, 0, ew);

	    if(cstring_version)
	      fs_give((void **)&cstring_version);

	    fs_give((void **)&src);

	    if(interactive){
		if(!(store = so_get(CharStar, NULL, EDIT_ACCESS))){
		    q_status_message(SM_ORDER | SM_DING, 7, 10,
				     _("Error allocating space for message."));
		    return(-1);
		}

		snprintf(prompt, sizeof(prompt),
    /* TRANSLATORS: following lines go together */
    _("\nYour signature file \"%s\" has been converted"), sigfile);
		prompt[sizeof(prompt)-1] = '\0';
		so_puts(store, prompt);
		so_puts(store,
    _("\nto a literal signature, which means it is contained in your"));
		so_puts(store,
    _("\nAlpine configuration instead of being in a file of its own."));
		so_puts(store,
    _("\nIf that configuration is copied to a remote folder then the"));
		so_puts(store,
    _("\nsignature will be available remotely also."));
		so_puts(store,
    _("\nChanges to the signature file itself will no longer have any"));
		so_puts(store,
    _("\neffect on Alpine but you may still edit the signature with the"));
		so_puts(store,
    _("\nSetup/Signature command.\n"));

		memset(&sargs, 0, sizeof(SCROLL_S));
		sargs.text.text  = so_text(store);
		sargs.text.src   = CharStar;
		sargs.text.desc  = _("Literal Signature Information");
		/* TRANSLATORS: screen title */
		sargs.bar.title  = _("ABOUT LITERAL SIG");
		sargs.help.text  = NO_HELP;
		sargs.help.title = NULL;

		scrolltool(&sargs);

		so_give(&store);
		ps->mangled_screen = 1;
	    }
	}
    }

    rflags = (ROLE_DO_ROLES | PAT_USE_MAIN);
    if(any_patterns(rflags, &pstate)){
	set_pathandle(rflags);
	for(patline = *cur_pat_h ?  (*cur_pat_h)->patlinehead : NULL;
	    patline; patline = patline->next){
	    for(pat = patline->first; pat; pat = pat->next){

		/*
		 * See detoken() for when a sig file is used with a role.
		 */
		sigfile = pat->action ? pat->action->sig : NULL;
		litsig = pat->action ? pat->action->litsig : NULL;
		nick = (pat->action && pat->action->nick && pat->action->nick[0]) ? pat->action->nick : NULL;

		if(sigfile && *sigfile && !litsig &&
		   sigfile[strlen(sigfile)-1] != '|' &&
		   !IS_REMOTE(sigfile)){
		    if(interactive){
			snprintf(prompt,sizeof(prompt),
		 /* TRANSLATORS: asking whether a signature file should be converted to what
		    we call a literal signature, which is one contained in the regular
		    configuration file. Think of the set of 4 %s arguments as a
		    single argument which is the name of the signature file. */
		 _("Convert signature file \"%s\"%s%s%s to a literal sig "),
				sigfile,
				nick ? " in role \"" : "",
				nick ? nick : "",
				nick ? "\"" : "");
			prompt[sizeof(prompt)-1] = '\0';
			ClearBody();
			ps->mangled_body = 1;
			if((ans=want_to(prompt, 'y', 'x',
					h_convert_sig, WT_NORM)) == 'x'){
			    cmd_cancelled(NULL);
			    return(-1);
			}
		    }
		    else
		      ans = 'y';

		    if(ans == 'y' &&
		       (src = get_signature_file(sigfile,0,0,0)) != NULL){

			cstring_version = string_to_cstring(src);

			if(pat->action->litsig)
			  fs_give((void **)&pat->action->litsig);

			pat->action->litsig = cstring_version;
			fs_give((void **)&src);

			set_pathandle(rflags);
			if(patline->type == Literal)
			  (*cur_pat_h)->dirtypinerc = 1;
			else
			  patline->dirty = 1;

			if(write_patterns(rflags) == 0){
			    if(interactive){
				/*
				 * Flush out current_vals of anything we've
				 * possibly changed.
				 */
				close_patterns(ROLE_DO_ROLES | PAT_USE_CURRENT);

				if(!(store=so_get(CharStar,NULL,EDIT_ACCESS))){
				    q_status_message(SM_ORDER | SM_DING, 7, 10,
					 _("Error allocating space for message."));
				    return(-1);
				}

				snprintf(prompt, sizeof(prompt),
		    /* TRANSLATORS: Keep the %s's together, they are sort of
		       the name of the file. */
		    _("Your signature file \"%s\"%s%s%s has been converted"),
					sigfile,
					nick ? " in role \"" : "",
					nick ? nick : "",
					nick ? "\"" : "");
				prompt[sizeof(prompt)-1] = '\0';
				so_puts(store, prompt);
				so_puts(store,
	    /* TRANSLATORS: several lines that go together */
	    _("\nto a literal signature, which means it is contained in your"));
				so_puts(store,
	    _("\nAlpine configuration instead of being in a file of its own."));
				so_puts(store,
	    _("\nIf that configuration is copied to a remote folder then the"));
				so_puts(store,
	    _("\nsignature will be available remotely also."));
				so_puts(store,
	    _("\nChanges to the signature file itself will no longer have any"));
				so_puts(store,
	    _("\neffect on Alpine. You may edit the signature with the"));
				so_puts(store,
	    _("\nSetup/Rules/Roles command.\n"));

				memset(&sargs, 0, sizeof(SCROLL_S));
				sargs.text.text  = so_text(store);
				sargs.text.src   = CharStar;
				sargs.text.desc  =
					    _("Literal Signature Information");
				/* TRANSLATORS: a screen title */
				sargs.bar.title  = _("ABOUT LITERAL SIG");
				sargs.help.text  = NO_HELP;
				sargs.help.title = NULL;

				scrolltool(&sargs);

				so_give(&store);
				ps->mangled_screen = 1;
			    }
			}
			else if(interactive){
			  q_status_message(SM_ORDER | SM_DING, 7, 10,
					   /* TRANSLATORS: config is an abbreviation for configuration */
					   _("Error writing rules config."));
			}
			else{
			    /* TRANSLATORS: sig is signature */
			    fprintf(stderr, _("Error converting role sig\n"));
			    return(-1);
			}
		    }
		}
	    }
	}
    }

    return(0);
}


void
warn_about_rule_files(struct pine *ps)
{
    STORE_S    *store;
    SCROLL_S	sargs;

    if(any_rule_files_to_warn_about(ps)){
	if(!(store = so_get(CharStar, NULL, EDIT_ACCESS))){
	    q_status_message(SM_ORDER | SM_DING, 7, 10,
			     _("Error allocating space for message."));
	    return;
	}

	/* TRANSLATORS: several lines that go together */
	so_puts(store, _("\nSome of your Rules are contained in Rule files instead of being directly"));
	so_puts(store, _("\ncontained in your Alpine configuration file. To make those rules"));
	so_puts(store, _("\navailable remotely you will need to move them out of the files."));
	so_puts(store, _("\nThat can be done using the Shuffle command in the appropriate"));
	so_puts(store, _("\nSetup/Rules subcommands.\n"));

	memset(&sargs, 0, sizeof(SCROLL_S));
	sargs.text.text  = so_text(store);
	sargs.text.src   = CharStar;
	sargs.text.desc  = _("Rule Files Information");
	/* TRANSLATORS: a screen title */
	sargs.bar.title  = _("ABOUT RULE FILES");
	sargs.help.text  = NO_HELP;
	sargs.help.title = NULL;

	scrolltool(&sargs);

	so_give(&store);
	ps->mangled_screen = 1;
    }
}


void
convert_to_remote_config(struct pine *ps, int edit_exceptions)
{
    char       rem_pinerc_prefix[MAILTMPLEN];
    char      *beg, *end;
    CONTEXT_S *context;
    int        abooks, sigs;

    if(edit_exceptions){
	/* TRANSLATORS: The exceptions command (X) was typed but it doesn't make sense */
	q_status_message(SM_ORDER, 3, 5,
			 _("eXceptions does not make sense with this command"));
	return;
    }

    if(!ps->prc)
      panic("NULL prc in convert_to_remote_config");

    dprint((2, "convert_to_remote_config\n"));
    
    if(ps->prc->type == RemImap){	/* pinerc is already remote */
	char prompt[MAILTMPLEN];

	/*
	 * Check to see if there is anything at all to do. If there are
	 * address books to convert, sigfiles to convert, or rule files
	 * to comment on, we have something to do. Otherwise, just bail.
	 */
	abooks = any_addrbooks_to_convert(ps);
	sigs   = any_sigs_to_convert(ps);

	if(abooks || sigs){
	    if(abooks && sigs)
	      /* TRANSLATORS: AddressBooks is Address Books */
	      snprintf(prompt, sizeof(prompt), _("Config is already remote, convert AddressBooks and signature files "));
	    else if(abooks)
	      snprintf(prompt, sizeof(prompt), _("Config is already remote, convert AddressBooks "));
	    else
	      snprintf(prompt, sizeof(prompt), _("Config is already remote, convert signature files "));

	    prompt[sizeof(prompt)-1] = '\0';
	    if(want_to(prompt, 'y', 'x',
		       (abooks && sigs) ? h_convert_abooks_and_sigs :
			abooks           ? h_convert_abooks :
			 sigs             ? h_convert_sigs : NO_HELP,
		       WT_NORM) != 'y'){
		cmd_cancelled(NULL);
		return;
	    }
	}
    }

    /*
     * Figure out a good default for where to put the remote config.
     * If the default collection is remote we'll take the hostname and
     * and modifiers from there. If not, we'll try to get the hostname from
     * the inbox-path. In either case, we use the home directory on the
     * server, not the directory where the folder collection is (if different).
     * If we don't have a clue, we'll ask user.
     */
    if((context = default_save_context(ps->context_list)) != NULL &&
       IS_REMOTE(context_apply(rem_pinerc_prefix, context, "",
	         sizeof(rem_pinerc_prefix)))){
	/* just use the host from the default collection, not the whole path */
	if((end = strrindex(rem_pinerc_prefix, '}')) != NULL)
	  *(end + 1) = '\0';
    }
    else{
	/* use host from inbox path */
	rem_pinerc_prefix[0] = '\0';
	if((beg = ps->VAR_INBOX_PATH)
	   && (*beg == '{' || (*beg == '*' && *++beg == '{'))
	   && (end = strindex(ps->VAR_INBOX_PATH, '}'))){
	    rem_pinerc_prefix[0] = '{';
	    strncpy(rem_pinerc_prefix+1, beg+1,
		    MIN(end-beg, sizeof(rem_pinerc_prefix)-2));
	    rem_pinerc_prefix[MIN(end-beg, sizeof(rem_pinerc_prefix)-2)] = '}';
	    rem_pinerc_prefix[MIN(end-beg+1,sizeof(rem_pinerc_prefix)-1)]='\0';
	}
    }

    /* ask about converting addrbooks to remote abooks */
    if(ps->prc->type != RemImap || abooks)
      if(convert_addrbooks_to_remote(ps, rem_pinerc_prefix,
				     sizeof(rem_pinerc_prefix)) == -1){
	  cmd_cancelled(NULL);
	  return;
      }

    /* ask about converting sigfiles to literal sigs */
    if(ps->prc->type != RemImap || sigs)
      if(convert_sigs_to_literal(ps, 1) == -1){
	  cmd_cancelled(NULL);
	  return;
      }

    warn_about_rule_files(ps);

    /* finally, copy the config file */
    if(ps->prc->type == Loc)
      convert_pinerc_to_remote(ps, rem_pinerc_prefix);
    else if(!(abooks || sigs))
      q_status_message(SM_ORDER, 3, 5,
		       _("Cannot copy config file since it is already remote."));
}


void
convert_pinerc_to_remote(struct pine *ps, char *rem_pinerc_prefix)
{
#define DEF_FOLDER_NAME "remote_pinerc"
    char       prompt[MAILTMPLEN], rem_pinerc[MAILTMPLEN];
    char      *err_msg = NULL;
    int        i, rc, offset;
    HelpType   help;
    int        flags = OE_APPEND_CURRENT;

    ClearBody();
    ps->mangled_body = 1;
    strncpy(rem_pinerc, rem_pinerc_prefix, sizeof(rem_pinerc)-1);
    rem_pinerc[sizeof(rem_pinerc)-1] = '\0';

    if(*rem_pinerc == '\0'){
	snprintf(prompt, sizeof(prompt), _("Name of server to contain remote Alpine config : "));
	prompt[sizeof(prompt)-1] = '\0';
	help = NO_HELP;
	while(1){
	    rc = optionally_enter(rem_pinerc, -FOOTER_ROWS(ps), 0,
				  sizeof(rem_pinerc), prompt, NULL,
				  help, &flags);
	    removing_leading_and_trailing_white_space(rem_pinerc);
	    if(rc == 3){
		help = help == NO_HELP ? h_convert_pinerc_server : NO_HELP;
	    }
	    else if(rc == 1){
		cmd_cancelled(NULL);
		return;
	    }
	    else if(rc == 0){
		if(*rem_pinerc){
		    /* add brackets */
		    offset = strlen(rem_pinerc);
		    for(i = offset; i >= 0; i--)
		      if(i+1 < sizeof(rem_pinerc))
		        rem_pinerc[i+1] = rem_pinerc[i];
		    
		    rem_pinerc[0] = '{';
		    if(offset+2 < sizeof(rem_pinerc)){
			rem_pinerc[++offset] = '}';
			rem_pinerc[++offset] = '\0';
		    }

		    break;
		}
	    }
	}
    }

    rem_pinerc[sizeof(rem_pinerc)-1] = '\0';

    /*
     * Add a default folder name.
     */
    if(*rem_pinerc){
	/*
	 * Add /user= to modify hostname so that user won't be asked who they
	 * are each time they login.
	 */
	if(!strstr(rem_pinerc, "/user=") && ps->VAR_USER_ID &&
	   ps->VAR_USER_ID[0]){
	    char *p;

	    p = rem_pinerc + strlen(rem_pinerc) - 1;
	    if(*p == '}')	/* this should be the case */
	      snprintf(p, sizeof(rem_pinerc)-(p-rem_pinerc), "/user=\"%s\"}", ps->VAR_USER_ID); 

	    rem_pinerc[sizeof(rem_pinerc)-1] = '\0';
	}

	strncat(rem_pinerc, DEF_FOLDER_NAME,
		sizeof(rem_pinerc) - strlen(rem_pinerc) - 1);
	rem_pinerc[sizeof(rem_pinerc)-1] = '\0';
    }

    /* ask user about folder name for remote config */
    snprintf(prompt, sizeof(prompt), _("Folder to contain remote config : "));
    prompt[sizeof(prompt)-1] = '\0';
    help = NO_HELP;
    while(1){
        rc = optionally_enter(rem_pinerc, -FOOTER_ROWS(ps), 0, 
			      sizeof(rem_pinerc), prompt, NULL, help, &flags);
	removing_leading_and_trailing_white_space(rem_pinerc);
        if(rc == 0 && *rem_pinerc){
	    break;
	}

        if(rc == 3){
	    help = (help == NO_HELP) ? h_convert_pinerc_folder : NO_HELP;
	}
	else if(rc == 1 || rem_pinerc[0] == '\0'){
	    cmd_cancelled(NULL);
	    return;
	}
    }

#ifndef	_WINDOWS
    /*
     * If we are on a Unix system, writing to a remote config, we want the
     * remote config to work smoothly from a PC, too. If we don't have a
     * user-id on the PC then we will be asked for our password.
     * So add user-id to the pinerc before we copy it.
     */
    if(!ps->vars[V_USER_ID].main_user_val.p && ps->VAR_USER_ID)
      ps->vars[V_USER_ID].main_user_val.p = cpystr(ps->VAR_USER_ID);
    
    ps->vars[V_USER_ID].is_used = 1;	/* so it will write to pinerc */
    ps->prc->outstanding_pinerc_changes = 1;
#endif

    if(ps->prc->outstanding_pinerc_changes)
      write_pinerc(ps, Main, WRP_NONE);

#ifndef	_WINDOWS
    ps->vars[V_USER_ID].is_used = 0;
#endif

    /* copy the pinerc */
    if(copy_pinerc(ps->prc->name, rem_pinerc, &err_msg)){
	if(err_msg){
	    q_status_message(SM_ORDER | SM_DING, 7, 10, err_msg);
	    fs_give((void **)&err_msg);
	}
	
	return;
    }

    /* tell user about command line flags */
    if(ps->prc->type != RemImap){
	STORE_S *store;
	SCROLL_S sargs;

	if(!(store = so_get(CharStar, NULL, EDIT_ACCESS))){
	    q_status_message(SM_ORDER | SM_DING, 7, 10,
			     _("Error allocating space for message."));
	    return;
	}

	/* TRANSLATORS: several lines that go together */
	so_puts(store, _("\nYou may want to save a copy of this information!"));
	so_puts(store, _("\n\nYour Alpine configuration data has been copied to"));
	so_puts(store, "\n\n   ");
	so_puts(store, rem_pinerc);
	so_puts(store, "\n");
	so_puts(store, _("\nTo use that remote configuration from this computer you will"));
	so_puts(store, _("\nhave to change the way you start Alpine by using the command line option"));
	so_puts(store, _("\n\"alpine -p <remote_folder>\". The command should probably be"));
#ifdef	_WINDOWS
	so_puts(store, "\n\n   ");
	so_puts(store, "alpine -p ");
	so_puts(store, rem_pinerc);
	so_puts(store, "\n");
	so_puts(store, _("\nWith PC-Alpine, you may want to create a shortcut which"));
	so_puts(store, _("\nhas the required arguments."));
#else
	so_puts(store, "\n\n   ");
	so_puts(store, "alpine -p \"");
	so_puts(store, rem_pinerc);
	so_puts(store, "\"\n");
	so_puts(store, _("\nThe quotes are there around the last argument to protect the special"));
	so_puts(store, _("\ncharacters in the folder name (like braces) from the command shell"));
	so_puts(store, _("\nyou use. If you are not running Alpine from a command shell which knows"));
	so_puts(store, _("\nabout quoting, it is possible you will have to remove those quotes"));
	so_puts(store, _("\nfrom the command. For example, if you also use PC-Alpine you will probably"));
	so_puts(store, _("\nwant to create a shortcut, and you would not need the quotes there."));
	so_puts(store, _("\nWithout the quotes, the command might look like"));
	so_puts(store, "\n\n   ");
	so_puts(store, "alpine -p ");
	so_puts(store, rem_pinerc);
	so_puts(store, "\n");
	so_puts(store, _("\nConsider creating an alias or shell script to execute this command to make"));
	so_puts(store, _("\nit more convenient."));
#endif
	so_puts(store, _("\n\nIf you want to use your new remote configuration for this session, quit"));
	so_puts(store, _("\nAlpine now and restart with the changed command line options mentioned above.\n"));

	memset(&sargs, 0, sizeof(SCROLL_S));
	sargs.text.text  = so_text(store);
	sargs.text.src   = CharStar;
	sargs.text.desc  = _("Remote Config Information");
	/* TRANSLATORS: a screen title */
	sargs.bar.title  = _("ABOUT REMOTE CONFIG");
	sargs.help.text  = NO_HELP;
	sargs.help.title = NULL;

	scrolltool(&sargs);

	so_give(&store);	/* free resources associated with store */
	ps->mangled_screen = 1;
    }
}


int
verify_folder_name(char *given, char **expanded, char **error, BUILDER_ARG *fcc, int *mangled)
{
    char *tmp;

    tmp = cpystr(given ? given : "");
    removing_leading_and_trailing_white_space(tmp);

    if(expanded)
      *expanded = tmp;
    else
      fs_give((void **)&tmp);
    
    if(error)
      *error = cpystr("");

    return 0;
}


int
verify_server_name(char *given, char **expanded, char **error, BUILDER_ARG *fcc, int *mangled)
{
    char *tmp;

    tmp = cpystr(given ? given : "");
    removing_leading_and_trailing_white_space(tmp);

    if(*tmp){
	/*
	 * could try to verify the hostname here
	 */
    }

    if(expanded)
      *expanded = tmp;
    else
      fs_give((void **)&tmp);
    
    if(error)
      *error = cpystr("");

    return 0;
}


int
verify_abook_nick(char *given, char **expanded, char **error, BUILDER_ARG *fcc, int *mangled)
{
    int   i;
    char *tmp;

    tmp = cpystr(given ? given : "");
    removing_leading_and_trailing_white_space(tmp);

    if(strindex(tmp, '"')){
	fs_give((void **)&tmp);
	if(error)
	  /* TRANSLATORS: Double quote refers to the " character */
	  *error = cpystr(_("Double quote not allowed in nickname"));
	
	return -2;
    }

    for(i = 0; i < as.n_addrbk; i++)
      if(i != the_one_were_editing && !strcmp(tmp, as.adrbks[i].abnick))
	break;
    
    if(i < as.n_addrbk){
	fs_give((void **)&tmp);

	if(error)
	  *error = cpystr(_("Nickname is already being used"));
	
	return -2;
    }

    if(expanded)
      *expanded = tmp;
    else
      fs_give((void **)&tmp);
    
    if(error)
      *error = cpystr("");

    return 0;
}


/*
 * Delete an addressbook.
 *
 * Args: cur_line     -- The current line position (in global display list)
 *			 of cursor
 *       command_line -- The screen line on which to prompt
 *       err          -- Points to error message
 *
 * Returns -- 0, deleted addrbook
 *            -1, addrbook not deleted
 */
int
ab_del_abook(long int cur_line, int command_line, char **err)
{
    int          abook_num, varnum, delete_data = 0,
		 num_in_list, how_many_in_list, i, cnt, warn_about_revert = 0;
    char       **list, **new_list, **t, **lval;
    char         tmp[200];
    PerAddrBook *pab;
    struct variable *vars = ps_global->vars;
    EditWhich    ew;
    enum {NotSet,
	  Modify,
	  RevertToDefault,
	  OverRideDefault,
	  DontChange} modify_config;

    /* restrict address book config to normal config file */
    ew = Main;

    if(ps_global->readonly_pinerc){
	if(err)
	  *err = _("Delete cancelled: config file not changeable");
	
	return -1;
    }

    abook_num = adrbk_num_from_lineno(cur_line);

    pab = &as.adrbks[abook_num];

    dprint((2, "- ab_del_abook(%s) -\n",
	   pab->abnick ? pab->abnick : "?"));

    varnum = (pab->type & GLOBAL) ? V_GLOB_ADDRBOOK : V_ADDRESSBOOK;

    if(vars[varnum].is_fixed){
	if(err){
	    if(pab->type & GLOBAL)
	      *err =
    _("Cancelled: Sys. Mgmt. does not allow changing global address book config");
	    else
	      *err =
    _("Cancelled: Sys. Mgmt. does not allow changing address book config");
	}

	return -1;
    }

    /*
     * Deal with reverting to default values of the address book
     * variables, or with user deleting a default value.
     */
    modify_config = NotSet;
    
    /* First count how many address books are in the user's config. */
    cnt = 0;
    lval = LVAL(&vars[varnum], ew);
    if(lval && lval[0])
      for(t = lval; *t != NULL; t++)
	cnt++;
    
    /*
     * Easy case, we can just delete one from the user's list.
     */
    if(cnt > 1){
	modify_config = Modify;
    }
    /*
     * Also easy. We'll revert to the default if it exists, and warn
     * the user about that.
     */
    else if(cnt == 1){
	modify_config = RevertToDefault;
	/* see if there's a default to revert to */
	cnt = 0;
	if(vars[varnum].global_val.l && vars[varnum].global_val.l[0])
	  for(t = vars[varnum].global_val.l; *t != NULL; t++)
	    cnt++;
	
	warn_about_revert = cnt;
    }
    /*
     * User is already using the default. Split it into two cases. If there
     * is one address book in default ask user if they want to delete that
     * default from their config. If there is more than one, ask them if
     * they want to ignore all the defaults or just delete this one.
     */
    else{
	/* count how many in default */
	cnt = 0;
	if(vars[varnum].global_val.l && vars[varnum].global_val.l[0])
	  for(t = vars[varnum].global_val.l; *t != NULL; t++)
	    cnt++;
	
	if(cnt > 1){
	    static ESCKEY_S opts[] = {
		/* TRANSLATORS: Ignore All means ignore all of the default values,
		   and Remove One means just remove this one default value. */
		{'i', 'i', "I", N_("Ignore All")},
		{'r', 'r', "R", N_("Remove One")},
		{-1, 0, NULL, NULL}};

	    snprintf(tmp, sizeof(tmp),
	    /* TRANSLATORS: %s is an adjective modifying address books */
	   _("Ignore all default %s address books or just remove this one ? "),
	       /* TRANSLATORS: global or personal address books */
	       pab->type & GLOBAL ? _("global") : _("personal"));
	    tmp[sizeof(tmp)-1] = '\0';
	    switch(radio_buttons(tmp, command_line, opts, 'i', 'x',
		   h_ab_del_ignore, RB_NORM)){
	      case 'i':
		modify_config = OverRideDefault;
		break;
		
	      case 'r':
		modify_config = Modify;
		break;
		
	      case 'x':
		if(err)
		  *err = _("Delete cancelled");

		return -1;
	    }
	}
	else{
	    /* TRANSLATORS: a question */
	    switch(want_to(_("Delete this default address book from config "),
		 'n', 'x', h_ab_del_default, WT_NORM)){
	      case 'n':
	      case 'x':
	        if(err)
		  *err = _("Delete cancelled");

	        return -1;
	    
	      case 'y':
		modify_config = OverRideDefault;
	        break;
	    }
	}
    }

    /*
     * ReadWrite means it exists and MaybeRorW means it is remote and we
     * haven't selected it yet to know our access permissions. The remote
     * folder should have been created, though, unless we didn't even have
     * permissions for that, in which case we got some error messages earlier.
     */
    if(pab->access == ReadWrite || pab->access == MaybeRorW){
	static ESCKEY_S o[] = {
	    /* TRANSLATORS: user is asked whether to remove just data files, just configuration,
	       or both for an address book. */
	    {'d', 'd', "D", N_("Data")},
	    {'c', 'c', "C", N_("Config")},
	    {'b', 'b', "B", N_("Both")},
	    {-1, 0, NULL, NULL}};

	switch(radio_buttons(_("Delete data, config, or both ? "),
			     command_line, o, 'c', 'x',
			     (modify_config == RevertToDefault)
			       ? h_ab_del_data_revert
			       : h_ab_del_data_modify,
			     RB_NORM)){
	  case 'b':				/* Delete Both */
	    delete_data = 1;
	    break;
	    
	  case 'd':				/* Delete only Data */
	    modify_config = DontChange;
	    delete_data = 1;
	    break;

	  case 'c':				/* Delete only Config */
	    break;
	    
	  case 'x':				/* Cancel */
	  default:
	    if(err)
	      *err = _("Delete cancelled");

	    return -1;
	}
    }
    else{
	/*
	 * Deleting config for address book which doesn't yet exist (hasn't
	 * ever been opened).
	 */
	/* TRANSLATORS: a question */
	switch(want_to(_("Delete configuration for highlighted addressbook "),
		       'n', 'x',
		       (modify_config == RevertToDefault)
		         ? h_ab_del_config_revert
			 : h_ab_del_config_modify,
		       WT_NORM)){
          case 'n':
          case 'x':
	  default:
	    if(err)
	      *err = _("Delete cancelled");

	    return -1;
	
          case 'y':
	    break;
	}
    }

    if(delete_data){
	char warning[800];

	dprint((5, "deleting addrbook data\n"));
	warning[0] = '\0';

	/*
	 * In order to delete the address book it is easiest if we open
	 * it first. That fills in the filenames we want to delete.
	 */
	if(pab->address_book == NULL){
	    warning[300] = '\0';
	    pab->address_book = adrbk_open(pab, ps_global->home_dir,
					   &warning[300], sizeof(warning)-300,
					   AB_SORT_RULE_NONE);
	    /*
	     * Couldn't get it open.
	     */
	    if(pab->address_book == NULL){
		if(warning[300])
		  /* TRANSLATORS: %s is an error message */
		  snprintf(warning, 300, _("Can't delete data: %s"), &warning[300]);
		else
		  strncpy(warning, _("Can't delete address book data"), 100);
	    }
	}

	/*
	 * If we have it open, set the delete bits and close to get the
	 * local copies. Delete the remote folder by hand.
	 */
	if(pab->address_book){
	    char       *file, *origfile = NULL;
	    int         f=0, o=0;

	    /*
	     * We're about to destroy addrbook data, better ask again.
	     */
	    if(pab->address_book->count > 0){
		char prompt[100];

		/* TRANSLATORS: a question */
		snprintf(prompt, sizeof(prompt),
			_("About to delete the contents of address book (%ld entries), really delete "), (long) adrbk_count(pab->address_book));
		prompt[sizeof(prompt)-1] = '\0';

		switch(want_to(prompt, 'n', 'n', h_ab_really_delete, WT_NORM)){
		  case 'y':
		    break;
		    
		  case 'n':
		  default:
		    if(err)
		      *err = _("Delete cancelled");

		    return -1;
		}
	    }

	    pab->address_book->flags |= DEL_FILE;
	    file = cpystr(pab->address_book->filename);
	    if(pab->type & REMOTE_VIA_IMAP)
	      origfile = cpystr(pab->address_book->orig_filename);

	    /*
	     * In order to avoid locking problems when we delete the
	     * remote folder, we need to actually close the remote stream
	     * instead of just putting it back in the stream pool.
	     * So we will remove this stream from the re-usable portion
	     * of the stream pool by clearing the SP_USEPOOL flag.
	     * Init_abook(pab, TotallyClosed) via rd_close_remdata is
	     * going to pine_mail_close it.
	     */
	    if(pab->type && REMOTE_VIA_IMAP
	       && pab->address_book
	       && pab->address_book->type == Imap
	       && pab->address_book->rd
	       && rd_stream_exists(pab->address_book->rd)){

		sp_unflag(pab->address_book->rd->t.i.stream, SP_USEPOOL);
	    }

	    /* This deletes the files because of DEL_ bits we set above. */
	    init_abook(pab, TotallyClosed);

	    /*
	     * Delete the remote folder.
	     */
	    if(pab->type & REMOTE_VIA_IMAP){
		REMDATA_S  *rd;
		int         exists;

		ps_global->c_client_error[0] = '\0';
		if(!pine_mail_delete(NULL, origfile) &&
		   ps_global->c_client_error[0] != '\0'){
		    dprint((1, "%s: %s\n", origfile ? origfile : "?",
			   ps_global->c_client_error));
		}

		/* delete line from metadata */
		rd = rd_new_remdata(RemImap, origfile, NULL);
		rd_write_metadata(rd, 1);
		rd_close_remdata(&rd);

		/* Check to see if it's still there */
		if((exists=folder_exists(NULL, origfile)) &&
		   (exists != FEX_ERROR)){
		    o++;
		    dprint((1, "Trouble deleting %s\n",
			   origfile ? origfile : "?"));
		}
	    }

	    if(can_access(file, ACCESS_EXISTS) == 0){
		f++;
		dprint((1, "Trouble deleting %s\n",
		       file ? file : "?"));
	    }

	    if(f || o){
		snprintf(warning, sizeof(warning), _("Trouble deleting data %s%s%s%s"),
		      f ? file : "",
		      (f && o) ? (o ? ", " : " and ") : "",
		      o ? " and " : "",
		      o ? origfile : "");
		warning[sizeof(warning)-1] = '\0';
	    }

	    fs_give((void **) &file);
	    if(origfile)
	      fs_give((void **) &origfile);
	}

	if(*warning){
	    q_status_message(SM_ORDER, 3, 3, warning);
	    dprint((1, "%s\n", warning));
	    display_message(NO_OP_COMMAND);
	}
	else if(modify_config == DontChange)
	  q_status_message(SM_ORDER, 0, 1, _("Addressbook data deleted"));
    }

    if(modify_config == DontChange){
	/*
	 * We return -1 to indicate that the addrbook wasn't deleted (as far
	 * as we're concerned) but we don't fill in err so that no error
	 * message will be printed.
	 * Since the addrbook is still an addrbook we need to reinitialize it.
	 */
	pab->access = adrbk_access(pab);
	if(pab->type & GLOBAL && pab->access != NoAccess)
	  pab->access = ReadOnly;
	
	init_abook(pab, HalfOpen);
	return -1;
    }
    else if(modify_config == Modify){
	list = vars[varnum].current_val.l;
	if(pab->type & GLOBAL){
	    how_many_in_list = as.n_addrbk - as.how_many_personals - 1;
	    num_in_list      = abook_num - as.how_many_personals;
	}
	else{
	    how_many_in_list = as.how_many_personals - 1;
	    num_in_list      = abook_num;
	}
    }
    else if(modify_config == OverRideDefault)
      how_many_in_list = 1;
    else if(modify_config == RevertToDefault)
      how_many_in_list = 0;
    else
      q_status_message(SM_ORDER, 3, 3, "can't happen in ab_del_abook");

    /* allocate for new list */
    if(how_many_in_list)
      new_list = (char **)fs_get((how_many_in_list + 1) * sizeof(char *));
    else
      new_list = NULL;

    /*
     * This case is both for modifying the users user_val and for the
     * case where the user wants to modify the global_val default and
     * use the modified version for his or her new user_val. We just
     * copy from the existing global_val, deleting the one addrbook
     * and put the result in user_val.
     */
    if(modify_config == Modify){
	/* copy old list up to where we will delete entry */
	for(i = 0; i < num_in_list; i++)
	  new_list[i] = cpystr(list[i]);
	
	/* copy rest of old list */
	for(; i < how_many_in_list; i++)
	  new_list[i] = cpystr(list[i+1]);
	
	new_list[i] = NULL;
    }
    else if(modify_config == OverRideDefault){
	new_list[0] = cpystr("");
	new_list[1] = NULL;
    }

    /* this also frees old variable contents for us */
    if(set_variable_list(varnum, new_list, TRUE, ew)){
	if(err)
	  *err = _("Delete cancelled: couldn't save pine configuration file");

	set_current_val(&vars[varnum], TRUE, FALSE);
	free_list_array(&new_list);

	return -1;
    }

    set_current_val(&vars[varnum], TRUE, FALSE);

    if(warn_about_revert){
	/* TRANSLATORS: the %s may be "global " or nothing */
	snprintf(tmp, sizeof(tmp), _("Reverting to default %saddress books"),
		pab->type & GLOBAL ? _("global ") : "");
	tmp[sizeof(tmp)-1] = '\0';
	q_status_message(SM_ORDER, 3, 4, tmp);
    }

    free_list_array(&new_list);
    
    return 0;
}


/*
 * Shuffle addrbooks.
 *
 * Args:    pab -- Pab from current addrbook.
 *        slide -- return value, tells how far to slide the cursor. If slide
 *                   is negative, slide it up, if positive, slide it down.
 * command_line -- The screen line on which to prompt
 *          msg -- Points to returned message, if any. Should be freed by
 *                   caller.
 *
 * Result: Two address books are swapped in the display order. If the shuffle
 *         crosses the Personal/Global boundary, then instead of swapping
 *         two address books the highlighted abook is moved from one section
 *         to the other.
 *          >= 0 on success.
 *          < 0 on failure, no changes.
 *          > 0 If the return value is greater than zero it means that we've
 *                reverted one of the variables to its default value. That
 *                means we've added at least one new addrbook, so the caller
 *                should reset. The value returned is the number of the
 *                moved addrbook + 1 (+1 so it won't be confused with zero).
 *          = 0 If the return value is zero we've just moved addrbooks around.
 *                No reset need be done.
 */
int
ab_shuffle(PerAddrBook *pab, int *slide, int command_line, char **msg)
{
    ESCKEY_S opts[3];
    char     tmp[200];
    int      i, deefault, rv, target = 0;
    int      up_into_empty = 0, down_into_empty = 0;
    HelpType help;
    struct variable *vars = ps_global->vars;

    dprint((2, "- ab_shuffle() -\n"));

    *slide = 0;

    if(ps_global->readonly_pinerc){
	if(msg)
	  *msg = cpystr(_("Shuffle cancelled: config file not changeable"));
	
	return -1;
    }

    /* Move it up or down? */
    i = 0;
    opts[i].ch      = 'u';
    opts[i].rval    = 'u';
    opts[i].name    = "U";
    /* TRANSLATORS: shuffle something Up or Down in a list */
    opts[i++].label = N_("Up");

    opts[i].ch      = 'd';
    opts[i].rval    = 'd';
    opts[i].name    = "D";
    opts[i++].label = N_("Down");

    opts[i].ch = -1;
    deefault = 'u';

    if(pab->type & GLOBAL){
	if(vars[V_GLOB_ADDRBOOK].is_fixed){
	    if(msg)
	      *msg = cpystr(_("Cancelled: Sys. Mgmt. does not allow changing global address book config"));
	    
	    return -1;
	}

	if(as.cur == 0){
	    if(as.config)
	      up_into_empty++;
	    else{					/* no up */
		opts[0].ch = -2;
		deefault = 'd';
	    }
	}

	if(as.cur == as.n_addrbk - 1)			/* no down */
	  opts[1].ch = -2;
    }
    else{
	if(vars[V_ADDRESSBOOK].is_fixed){
	    if(msg)
	      *msg = cpystr(_("Cancelled: Sys. Mgmt. does not allow changing address book config"));
	    
	    return -1;
	}

	if(as.cur == 0){				/* no up */
	    opts[0].ch = -2;
	    deefault = 'd';
	}

	if(as.cur == as.n_addrbk - 1){
	    if(as.config)
	      down_into_empty++;
	    else
	      opts[1].ch = -2;				/* no down */
	}
    }

    snprintf(tmp, sizeof(tmp), _("Shuffle \"%s\" %s%s%s ? "),
	    pab->abnick,
	    (opts[0].ch != -2) ? _("UP") : "",
	    (opts[0].ch != -2 && opts[1].ch != -2) ? " or " : "",
	    (opts[1].ch != -2) ? _("DOWN") : "");
    tmp[sizeof(tmp)-1] = '\0';
    help = (opts[0].ch == -2) ? h_ab_shuf_down
			      : (opts[1].ch == -2) ? h_ab_shuf_up
						   : h_ab_shuf;

    rv = radio_buttons(tmp, command_line, opts, deefault, 'x',
		       help, RB_NORM);

    ps_global->mangled_footer = 1;

    if((rv == 'u' && up_into_empty) || (rv == 'd' && down_into_empty))
      target = -1;
    else
      target = as.cur + (rv == 'u' ? -1 : 1);

    if(rv == 'x'){
	if(msg)
	  *msg = cpystr(_("Shuffle cancelled"));
	
	return -1;
    }
    else
      return(do_the_shuffle(slide, as.cur, target, msg));
}


/*
 * Actually shuffle the config variables and address books structures around.
 *
 * Args:  anum1, anum2 -- The numbers of the address books
 *                 msg -- Points to returned message, if any.
 *
 * Returns: >= 0 on success.
 *          < 0 on failure, no changes.
 *          > 0 If the return value is greater than zero it means that we've
 *                reverted one of the variables to its default value. That
 *                means we've added at least one new addrbook, so the caller
 *                should reset. The value returned is the number of the
 *                moved addrbook + 1 (+1 so it won't be confused with zero).
 *          = 0 If the return value is zero we've just moved addrbooks around.
 *                No reset need be done.
 *
 * Anum1 is the one that we want to move, anum2 is the one that it will be
 * swapped with. When anum1 and anum2 are on the opposite sides of the
 * Personal/Global boundary then instead of swapping we just move anum1 to
 * the other side of the boundary.
 *
 * Anum2 of -1 means it is a swap into the other type of address book, which
 * is currently empty.
 */
int
do_the_shuffle(int *slide, int anum1, int anum2, char **msg)
{
    PerAddrBook *pab;
    enum {NotSet, Pers, Glob, Empty} type1, type2;
    int          i, j, retval = -1;
    struct variable *vars = ps_global->vars;
    char **lval;
    EditWhich ew;
    char *cancel_msg = _("Shuffle cancelled: couldn't save configuration file");

    dprint((5, "- do_the_shuffle(%d, %d) -\n", anum1, anum2));

    /* restrict address book config to normal config file */
    ew = Main;

    if(anum1 == -1)
      type1 = Empty;
    else{
	pab = &as.adrbks[anum1];
	type1 = (pab->type & GLOBAL) ? Glob : Pers;
    }

    if(type1 == Empty){
	if(msg)
	  *msg =
	      cpystr(_("Shuffle cancelled: highlight entry you wish to shuffle"));

	return(retval);
    }

    if(anum2 == -1)
      type2 = Empty;
    else{
	pab = &as.adrbks[anum2];
	type2 = (pab->type & GLOBAL) ? Glob : Pers;
    }

    if(type2 == Empty)
      type2 = (type1 == Pers) ? Glob : Pers;

    if((type1 == Pers || type2 == Pers) && vars[V_ADDRESSBOOK].is_fixed){
	if(msg)
	  *msg = cpystr(_("Cancelled: Sys. Mgmt. does not allow changing address book configuration"));

	return(retval);
    }

    if((type1 == Glob || type2 == Glob) && vars[V_GLOB_ADDRBOOK].is_fixed){
	if(msg)
	  *msg = cpystr(_("Cancelled: Sys. Mgmt. does not allow changing global address book config"));

	return(retval);
    }

    /*
     * There are two cases. If the shuffle is two address books within the
     * same variable, then they just swap places. If it is a shuffle of an
     * addrbook from one side of the boundary to the other, just that one
     * is moved.
     */
    if((type1 == Glob && type2 == Glob) ||
       (type1 == Pers && type2 == Pers)){
	int          how_many_in_list, varnum;
	int          anum1_rel, anum2_rel;	/* position in specific list */
	char       **list, **new_list;
	PerAddrBook  tmppab;

	*slide = (anum1 < anum2) ? LINES_PER_ABOOK : -1 * LINES_PER_ABOOK;

	if(type1 == Pers){
	    how_many_in_list = as.how_many_personals;
	    list             = VAR_ADDRESSBOOK;
	    varnum           = V_ADDRESSBOOK;
	    anum1_rel        = anum1;
	    anum2_rel        = anum2;
	}
	else{
	    how_many_in_list = as.n_addrbk - as.how_many_personals;
	    list             = VAR_GLOB_ADDRBOOK;
	    varnum           = V_GLOB_ADDRBOOK;
	    anum1_rel        = anum1 - as.how_many_personals;
	    anum2_rel        = anum2 - as.how_many_personals;
	}

	/* allocate for new list, same size as old list */
	new_list = (char **)fs_get((how_many_in_list + 1) * sizeof(char *));

	/* fill in new_list */
	for(i = 0; i < how_many_in_list; i++){
	    /* swap anum1 and anum2 */
	    if(i == anum1_rel)
	      j = anum2_rel;
	    else if(i == anum2_rel)
	      j = anum1_rel;
	    else
	      j = i;

	    new_list[i] = cpystr(list[j]);
	}

	new_list[i] = NULL;

	if(set_variable_list(varnum, new_list, TRUE, ew)){
	    if(msg)
	      *msg = cpystr(cancel_msg);

	    /* restore old values */
	    set_current_val(&vars[varnum], TRUE, FALSE);
	    free_list_array(&new_list);
	    return(retval);
	}

	retval = 0;
	set_current_val(&vars[varnum], TRUE, FALSE);
	free_list_array(&new_list);

	/* Swap PerAddrBook structs */
	tmppab = as.adrbks[anum1];
	as.adrbks[anum1] = as.adrbks[anum2];
	as.adrbks[anum2] = tmppab;
    }
    else if((type1 == Pers && type2 == Glob) ||
            (type1 == Glob && type2 == Pers)){
	int how_many_in_srclist, how_many_in_dstlist;
	int srcvarnum, dstvarnum, srcanum;
	int cnt, warn_about_revert = 0;
	char **t;
	char **new_src, **new_dst, **srclist, **dstlist;
	char   tmp[200];
	enum {NotSet, Modify, RevertToDefault, OverRideDefault} modify_config;

	/*
	 * how_many_in_srclist = # in orig src list (Pers or Glob list).
	 * how_many_in_dstlist = # in orig dst list
	 * srcanum = # of highlighted addrbook that is being shuffled
	 */
	if(type1 == Pers){
	    how_many_in_srclist = as.how_many_personals;
	    how_many_in_dstlist = as.n_addrbk - as.how_many_personals;
	    srclist             = VAR_ADDRESSBOOK;
	    dstlist             = VAR_GLOB_ADDRBOOK;
	    srcvarnum           = V_ADDRESSBOOK;
	    dstvarnum           = V_GLOB_ADDRBOOK;
	    srcanum             = as.how_many_personals - 1;
	    *slide              = (how_many_in_srclist == 1)
				    ? (LINES_PER_ADD_LINE + XTRA_LINES_BETWEEN)
				    : XTRA_LINES_BETWEEN;
	}
	else{
	    how_many_in_srclist = as.n_addrbk - as.how_many_personals;
	    how_many_in_dstlist = as.how_many_personals;
	    srclist             = VAR_GLOB_ADDRBOOK;
	    dstlist             = VAR_ADDRESSBOOK;
	    srcvarnum           = V_GLOB_ADDRBOOK;
	    dstvarnum           = V_ADDRESSBOOK;
	    srcanum             = as.how_many_personals;
	    *slide              = (how_many_in_dstlist == 0)
				    ? (LINES_PER_ADD_LINE + XTRA_LINES_BETWEEN)
				    : XTRA_LINES_BETWEEN;
	    *slide              = -1 * (*slide);
	}


	modify_config = Modify;
	if(how_many_in_srclist == 1){
	    /*
	     * Deal with reverting to default values of the address book
	     * variables, or with user deleting a default value.
	     */
	    modify_config = NotSet;
	    
	    /*
	     * Count how many address books are in the user's config.
	     * This has to be one or zero, because how_many_in_srclist == 1.
	     */
	    cnt = 0;
	    lval = LVAL(&vars[srcvarnum], ew);
	    if(lval && lval[0])
	      for(t = lval; *t != NULL; t++)
		cnt++;
	    
	    /*
	     * We'll revert to the default if it exists, and warn
	     * the user about that.
	     */
	    if(cnt == 1){
		modify_config = RevertToDefault;
		/* see if there's a default to revert to */
		cnt = 0;
		if(vars[srcvarnum].global_val.l &&
		   vars[srcvarnum].global_val.l[0])
		  for(t = vars[srcvarnum].global_val.l; *t != NULL; t++)
		    cnt++;
		
		warn_about_revert = cnt;
		if(warn_about_revert > 1 && type1 == Pers)
		  *slide = LINES_PER_ABOOK * warn_about_revert +
						      XTRA_LINES_BETWEEN;
	    }
	    /*
	     * User is already using the default.
	     */
	    else if(cnt == 0){
		modify_config = OverRideDefault;
	    }
	}

	/*
	 * We're adding one to the dstlist, so need how_many + 1 + 1.
	 */
	new_dst = (char **)fs_get((how_many_in_dstlist + 2) * sizeof(char *));
	j = 0;

	/*
	 * Because the Personal list comes before the Global list, when
	 * we move to Global we're inserting a new first element into
	 * the global list (the dstlist).
	 *
	 * When we move from Global to Personal, we're appending a new
	 * last element onto the personal list (the dstlist).
	 */
	if(type2 == Glob)
	  new_dst[j++] = cpystr(srclist[how_many_in_srclist-1]);

	for(i = 0; i < how_many_in_dstlist; i++)
	  new_dst[j++] = cpystr(dstlist[i]);

	if(type2 == Pers)
	  new_dst[j++] = cpystr(srclist[0]);

	new_dst[j] = NULL;

	/*
	 * The srclist is complicated by the reverting to default
	 * behaviors.
	 */
	if(modify_config == Modify){
	    /*
	     * In this case we're just removing one from the srclist
	     * so the new_src is of size how_many -1 +1.
	     */
	    new_src = (char **)fs_get((how_many_in_srclist) * sizeof(char *));
	    j = 0;

	    for(i = 0; i < how_many_in_srclist-1; i++)
	      new_src[j++] = cpystr(srclist[i + ((type1 == Glob) ? 1 : 0)]);

	    new_src[j] = NULL;
	}
	else if(modify_config == OverRideDefault){
	    /*
	     * We were using default and will now revert to nothing.
	     */
	    new_src = (char **)fs_get(2 * sizeof(char *));
	    new_src[0] = cpystr("");
	    new_src[1] = NULL;
	}
	else if(modify_config == RevertToDefault){
	    /*
	     * We are moving our last user variable out and reverting
	     * to the default value for this variable.
	     */
	    new_src = NULL;
	}

	if(set_variable_list(dstvarnum, new_dst, TRUE, ew) ||
	   set_variable_list(srcvarnum, new_src, TRUE, ew)){
	    if(msg)
	      *msg = cpystr(cancel_msg);

	    /* restore old values */
	    set_current_val(&vars[dstvarnum], TRUE, FALSE);
	    set_current_val(&vars[srcvarnum], TRUE, FALSE);
	    free_list_array(&new_dst);
	    free_list_array(&new_src);
	    return(retval);
	}
	
	set_current_val(&vars[dstvarnum], TRUE, FALSE);
	set_current_val(&vars[srcvarnum], TRUE, FALSE);
	free_list_array(&new_dst);
	free_list_array(&new_src);

	retval = (type1 == Pers && warn_about_revert)
		    ? (warn_about_revert + 1) : (srcanum + 1);

	/*
	 * This is a tough case. We're adding one or more new address books
	 * in this case so we need to reset the addrbooks and start over.
	 * We return the number of the address book we just moved after the
	 * reset so that the caller can focus attention on the moved one.
	 * Actually, we return 1+the number so that we can tell it apart
	 * from a return of zero, which just means everything is ok.
	 */
	if(warn_about_revert){
	    snprintf(tmp, sizeof(tmp),
     "This address book now %s, reverting to default %s address %s",
		    (type1 == Glob) ? "Personal" : "Global",
		    (type1 == Glob) ? "Global" : "Personal",
		    warn_about_revert > 1 ? "books" : "book");
	    tmp[sizeof(tmp)-1] = '\0';
	    if(msg)
	      *msg = cpystr(tmp);
	}
	else{
	    /*
	     * Modify PerAddrBook struct and adjust boundary.
	     * In this case we aren't swapping two addrbooks, but just modifying
	     * one from being global to personal or the reverse. It will
	     * still be the same element in the as.adrbks array.
	     */
	    pab = &as.adrbks[srcanum];
	    if(type2 == Glob){
		as.how_many_personals--;
		pab->type |= GLOBAL;
		if(pab->access != NoAccess)
		  pab->access = ReadOnly;
	    }
	    else{
		as.how_many_personals++;
		pab->type &= ~GLOBAL;
		if(pab->access != NoAccess && pab->access != MaybeRorW)
		  pab->access = ReadWrite;
	    }

	    snprintf(tmp, sizeof(tmp),
		    "This address book now %s",
		    (type1 == Glob) ? "Personal" : "Global");
	    tmp[sizeof(tmp)-1] = '\0';
	    if(msg)
	      *msg = cpystr(tmp);
	}
    }

    return(retval);
}


int
ab_compose_to_addr(long int cur_line, int agg, int allow_role)
{
    AddrScrn_Disp *dl;
    AdrBk_Entry   *abe;
    SAVE_STATE_S   state;
    BuildTo        bldto;

    dprint((2, "- ab_compose_to_addr -\n"));

    save_state(&state);

    bldto.type    = Str;
    bldto.arg.str = NULL;

    if(agg){
	int    i;
	size_t incr = 100, avail, alloced;
	char  *to = NULL;

	to = (char *)fs_get(incr);
	*to = '\0';
	avail = incr;
	alloced = incr;

	/*
	 * Run through all of the selected entries
	 * in all of the address books.
	 * Put the nicknames together into one long
	 * string with comma separators.
	 */
	for(i = 0; i < as.n_addrbk; i++){
	    adrbk_cntr_t num;
	    PerAddrBook *pab;
	    EXPANDED_S  *next_one;

	    pab = &as.adrbks[i];
	    if(pab->address_book)
	      next_one = pab->address_book->selects;
	    else
	      continue;

	    while((num = entry_get_next(&next_one)) != NO_NEXT){
		char          *a_string;
		AddrScrn_Disp  fake_dl;

		abe = adrbk_get_ae(pab->address_book, (a_c_arg_t) num);

		/*
		 * Since we're picking up address book entries
		 * directly from the address books and have
		 * no knowledge of the display lines they came
		 * from, we don't know the dl's that go with
		 * them.  We need to pass a dl to abe_to_nick
		 * but it really is only going to use the
		 * type in this case.
		 */
		dl = &fake_dl;
		dl->type = (abe->tag == Single) ? Simple : ListHead;
		a_string = abe_to_nick_or_addr_string(abe, dl, i);

		while(abe && avail < (size_t)strlen(a_string)+1){
		    alloced += incr;
		    avail   += incr;
		    fs_resize((void **)&to, alloced);
		}

		if(!*to){
		    strncpy(to, a_string, alloced);
		    to[alloced-1] = '\0';
		}
		else{
		    strncat(to, ",", alloced-strlen(to)-1);
		    to[alloced-1] = '\0';
		    strncat(to, a_string, alloced-strlen(to)-1);
		    to[alloced-1] = '\0';
		}

		avail -= (strlen(a_string) + 1);
		fs_give((void **)&a_string);
	    }
	}

	bldto.type    = Str;
	bldto.arg.str = to;
    }
    else{
	if(is_addr(cur_line)){

	    dl  = dlist(cur_line);
	    abe = ae(cur_line);

	    if(dl->type == ListEnt){
		bldto.type    = Str;
		bldto.arg.str = cpystr(listmem(cur_line));
	    }
	    else{
		bldto.type    = Abe;
		bldto.arg.abe = abe;
	    }
	}
    }

    if(bldto.type == Str && bldto.arg.str == NULL)
      bldto.arg.str = cpystr("");

    ab_compose_internal(bldto, allow_role);

    restore_state(&state);

    if(bldto.type == Str && bldto.arg.str)
      fs_give((void **)&bldto.arg.str);
    
    /*
     * Window size may have changed in composer.
     * Pine_send will have reset the window size correctly,
     * but we still have to reset our address book data structures.
     */
    ab_resize();
    ps_global->mangled_screen = 1;
    return(1);
}


/*
 * Used by the two compose routines.
 */
void
ab_compose_internal(BuildTo bldto, int allow_role)
{
    int		   good_addr;
    char          *addr, *fcc, *error = NULL;
    ACTION_S      *role = NULL;
    void (*prev_screen)(struct pine *) = ps_global->prev_screen,
	 (*redraw)(void) = ps_global->redrawer;

    if(allow_role)
      ps_global->redrawer = NULL;

    ps_global->next_screen = SCREEN_FUN_NULL;

    fcc  = NULL;
    addr = NULL;

    good_addr = (our_build_address(bldto, &addr, &error, &fcc, NULL) >= 0);
	
    if(error){
	q_status_message1(SM_ORDER, 3, 4, "%s", error);
	fs_give((void **)&error);
    }

    if(!good_addr && addr && *addr)
      fs_give((void **)&addr); /* relying on fs_give setting addr to NULL */

    if(allow_role){
	/* Setup role */
	if(role_select_screen(ps_global, &role, MC_COMPOSE) < 0){
	    cmd_cancelled("Composition");
	    ps_global->next_screen = prev_screen;
	    ps_global->redrawer = redraw;
	    return;
	}

	/*
	 * If default role was selected (NULL) we need to make up a role which
	 * won't do anything, but will cause compose_mail to think there's
	 * already a role so that it won't try to confirm the default.
	 */
	if(role)
	  role = copy_action(role);
	else{
	    role = (ACTION_S *)fs_get(sizeof(*role));
	    memset((void *)role, 0, sizeof(*role));
	    role->nick = cpystr("Default Role");
	}
    }

    compose_mail(addr, fcc, role, NULL, NULL);

    if(addr)
      fs_give((void **)&addr);

    if(fcc)
      fs_give((void **)&fcc);
}


/*
 * Export addresses into a file.
 *
 * Args: cur_line     -- The current line position (in global display list)
 *			 of cursor
 *       command_line -- The screen line on which to prompt
 *
 * Returns -- 1 if the export is done
 *            0 if not
 */
int
ab_export(struct pine *ps, long int cur_line, int command_line, int agg)
{
    int		   ret = 0, i, retflags = GER_NONE;
    int            r, orig_errno, failure = 0;
    struct variable *vars = ps->vars;
    char     filename[MAXPATH+1], full_filename[MAXPATH+1];
    STORE_S *store;
    gf_io_t  pc;
    long     start_of_append;
    char    *addr = NULL, *error = NULL;
    BuildTo  bldto;
    char    *p;
    int      good_addr, plur, vcard = 0, tab = 0;
    static HISTORY_S *history = NULL;
    AdrBk_Entry *abe;
    VCARD_INFO_S *vinfo;
    static ESCKEY_S ab_export_opts[] = {
	{ctrl('T'), 10, "^T", N_("To Files")},
	{-1, 0, NULL, NULL},
	{-1, 0, NULL, NULL}};
    static ESCKEY_S vcard_or_addresses[] = {
	{'a', 'a', "A", N_("Address List")},
	{'v', 'v', "V", N_("VCard")},
	/* TRANSLATORS: TabSep is a command key label meaning Tab Separated List */
	{'t', 't', "T", N_("TabSep")},
	{-1, 0, NULL, NULL}};


    dprint((2, "- ab_export -\n"));

    if(ps->restricted){
	q_status_message(SM_ORDER, 0, 3,
	    "Alpine demo can't export addresses to files");
	return(ret);
    }

    while(1){
	i = radio_buttons(_("Export list of addresses, vCard format, or Tab Separated ? "),
			  command_line, vcard_or_addresses, 'a', 'x',
			  NO_HELP, RB_NORM|RB_RET_HELP);
	if(i == 3){
	    /* TRANSLATORS: a screen title */
	    helper(h_ab_export_vcard, _("HELP FOR EXPORT FORMAT"),
		   HLPD_SIMPLE);
	    ps_global->mangled_screen = 1;
	}
	else
	  break;
    }

    switch(i){
      case 'x':
	q_status_message(SM_INFO, 0, 2, _("Address book export cancelled"));
	return(ret);
    
      case 'a':
	break;

      case 'v':
	vcard++;
	break;

      case 't':
	tab++;
	break;

      default:
	q_status_message(SM_ORDER, 3, 3, "can't happen in ab_export");
	return(ret);
    }

    if(agg)
      plur = 1;
    else{
	abe = ae(cur_line);
	plur = (abe && abe->tag == List);
    }

    filename[0] = '\0';
    r = 0;

    if(F_ON(F_ENABLE_TAB_COMPLETE,ps_global)){
	ab_export_opts[++r].ch  =  ctrl('I');
	ab_export_opts[r].rval  = 11;
	ab_export_opts[r].name  = "TAB";
	ab_export_opts[r].label = N_("Complete");
    }

    ab_export_opts[++r].ch = -1;

    r = get_export_filename(ps, filename, NULL, full_filename, sizeof(filename),
			    plur ? _("addresses") : _("address"),
			    _("EXPORT"), ab_export_opts,
			    &retflags, command_line, GE_IS_EXPORT, &history);

    if(r < 0){
	switch(r){
	  case -1:
	    q_status_message(SM_INFO, 0, 2, _("Address book export cancelled"));
	    break;

	  case -2:
	    q_status_message1(SM_ORDER, 0, 2,
		_("Can't export to file outside of %s"), VAR_OPER_DIR);
	    break;
	}

	goto fini;
    }

    dprint((5, "Opening file \"%s\" for export\n",
	   full_filename ? full_filename : "?"));

    if(!(store = so_get(FileStar, full_filename, WRITE_ACCESS|WRITE_TO_LOCALE))){
	q_status_message2(SM_ORDER | SM_DING, 3, 4,
		  _("Error opening file \"%s\" for address export: %s"),
		  full_filename, error_description(errno));
	goto fini;
    }

    /*
     * The write_single_vcard_entry function wants a pc.
     */
    if(vcard || tab)
      gf_set_so_writec(&pc, store);

    start_of_append = so_tell(store);

    if(agg){
	for(i = 0; !failure && i < as.n_addrbk; i++){
	    adrbk_cntr_t num;
	    PerAddrBook *pab;
	    EXPANDED_S  *next_one;

	    pab = &as.adrbks[i];
	    if(pab->address_book)
	      next_one = pab->address_book->selects;
	    else
	      continue;

	    while(!failure && (num = entry_get_next(&next_one)) != NO_NEXT){

		abe = adrbk_get_ae(pab->address_book, (a_c_arg_t) num);
		if((vcard || tab) && abe){
		    /*
		     * There is no place to store the charset information
		     * so we don't ask for it.
		     */
		    if(!(vinfo=prepare_abe_for_vcard(ps, abe, 1)))
		      failure++;
		    else{
			if(vcard)
			  write_single_vcard_entry(ps, pc, vinfo);
			else
			  write_single_tab_entry(pc, vinfo);

			free_vcard_info(&vinfo);
		    }
		}
		else if(abe){
		    bldto.type    = Abe;
		    bldto.arg.abe = abe;
		    error         = NULL;
		    addr          = NULL;
		    good_addr = (our_build_address(bldto,&addr,&error,NULL,NULL) >= 0);
	
		    if(error){
			q_status_message1(SM_ORDER, 0, 4, "%s", error);
			fs_give((void **)&error);
		    }

		    /* rfc1522_decode the addr */
		    if(addr){
			size_t len;

			len = 4*strlen(addr)+1;
			p = (char *)fs_get(len * sizeof(char));
			if(rfc1522_decode_to_utf8((unsigned char *)p,len,addr) == (unsigned char *)p){
			    fs_give((void **)&addr);
			    addr = p;
			}
			else
			  fs_give((void **)&p);
		    }

		    if(good_addr){
			int   quoted = 0;

			/*
			 * Change the unquoted commas into newlines.
			 * Not worth it to do complicated quoting,
			 * just consider double quotes.
			 */
			for(p = addr; *p; p++){
			    if(*p == '"')
			      quoted = !quoted;
			    else if(!quoted && *p == ','){
				*p++ = '\n';
				removing_leading_white_space(p);
				p--;
			    }
			}

			if(!so_puts(store, addr) || !so_puts(store, NEWLINE)){
			    orig_errno = errno;
			    failure    = 1;
			}
		    }

		    if(addr)
		      fs_give((void **)&addr);
		}
	    }
	}
    }
    else{
	AddrScrn_Disp *dl;

	dl  = dlist(cur_line);
	abe = ae(cur_line);
	if((vcard || tab) && abe){
	    if(!(vinfo=prepare_abe_for_vcard(ps, abe, 1)))
	      failure++;
	    else{
		if(vcard)
		  write_single_vcard_entry(ps, pc, vinfo);
		else
		  write_single_tab_entry(pc, vinfo);

		free_vcard_info(&vinfo);
	    }
	}
	else{

	    if(dl->type == ListHead && listmem_count_from_abe(abe) == 0){
		error = _("List is empty, nothing to export!");
		good_addr = 0;
	    }
	    else if(dl->type == ListEnt){
		bldto.type    = Str;
		bldto.arg.str = listmem(cur_line);
		good_addr = (our_build_address(bldto,&addr,&error,NULL,NULL) >= 0);
	    }
	    else{
		bldto.type    = Abe;
		bldto.arg.abe = abe;
		good_addr = (our_build_address(bldto,&addr,&error,NULL,NULL) >= 0);
	    }

	    if(error){
		q_status_message1(SM_ORDER, 3, 4, "%s", error);
		fs_give((void **)&error);
	    }

	    /* Have to rfc1522_decode the addr */
	    if(addr){
		size_t len;
		len = 4*strlen(addr)+1;
		p = (char *)fs_get(len * sizeof(char));
		if(rfc1522_decode_to_utf8((unsigned char *)p,len,addr) == (unsigned char *)p){
		    fs_give((void **)&addr);
		    addr = p;
		}
		else
		  fs_give((void **)&p);
	    }

	    if(good_addr){
		int   quoted = 0;

		/*
		 * Change the unquoted commas into newlines.
		 * Not worth it to do complicated quoting,
		 * just consider double quotes.
		 */
		for(p = addr; *p; p++){
		    if(*p == '"')
		      quoted = !quoted;
		    else if(!quoted && *p == ','){
			*p++ = '\n';
			removing_leading_white_space(p);
			p--;
		    }
		}

		if(!so_puts(store, addr) || !so_puts(store, NEWLINE)){
		    orig_errno = errno;
		    failure    = 1;
		}
	    }

	    if(addr)
	      fs_give((void **)&addr);
	}
    }

    if(vcard || tab)
      gf_clear_so_writec(store);

    if(so_give(&store))				/* release storage */
      failure++;

    if(failure){
	our_truncate(full_filename, (off_t)start_of_append);
	dprint((1, "FAILED Export: file \"%s\" : %s\n",
	       full_filename ? full_filename : "?",
	       error_description(orig_errno)));
	q_status_message2(SM_ORDER | SM_DING, 3, 4,
		  _("Error exporting to \"%s\" : %s"),
		  filename, error_description(orig_errno));
    }
    else{
	ret = 1;
	q_status_message3(SM_ORDER,0,3,
			  "%s %s to file \"%s\"",
			  (vcard || tab) ? (agg ? "Entries" : "Entry")
			        : (plur ? "Addresses" : "Address"),
			  retflags & GER_OVER
			      ? "overwritten"
			      : retflags & GER_APPEND ? "appended" : "exported",
			  filename);
    }

fini:
    ps->mangled_footer = 1;
    return(ret);
}


/*
 * Forward an address book entry or entries via email attachment.
 *
 *   We use the vCard standard to send entries. We group multiple entries
 * using the BEGIN/END construct of vCard, not with multiple MIME parts.
 * A limitation of vCard is that there can be only one charset for the
 * whole group we send, so we might lose that information.
 *
 * Args: cur_line     -- The current line position (in global display list)
 *			 of cursor
 *       command_line -- The screen line on which to prompt
 */
int
ab_forward(struct pine *ps, long int cur_line, int agg)
{
    AddrScrn_Disp *dl;
    AdrBk_Entry   *abe;
    ENVELOPE      *outgoing = NULL;
    BODY          *pb, *body = NULL;
    PART         **pp;
    char          *sig;
    gf_io_t        pc;
    int            i, ret = 0;
    VCARD_INFO_S  *vinfo;
    ACTION_S      *role = NULL;

    dprint((2, "- ab_forward -\n"));

    if(!agg){
	dl  = dlist(cur_line);
	if(dl->type != ListHead && dl->type != Simple)
	  return(ret);

	abe = ae(cur_line);
	if(!abe){
	    q_status_message(SM_ORDER, 3, 3, _("Trouble accessing current entry"));
	    return(ret);
	}
    }

    outgoing             = mail_newenvelope();
    outgoing->message_id = generate_message_id();
    if(agg && as.selections > 1)
      outgoing->subject = cpystr("Forwarded address book entries from Alpine");
    else
      outgoing->subject = cpystr("Forwarded address book entry from Alpine");

    body                                      = mail_newbody();
    body->type                                = TYPEMULTIPART;
    /*---- The TEXT part/body ----*/
    body->nested.part                       = mail_newbody_part();
    body->nested.part->body.type            = TYPETEXT;
    /*--- Allocate an object for the body ---*/
    if((body->nested.part->body.contents.text.data =
				(void *)so_get(PicoText, NULL, EDIT_ACCESS)) != NULL){
	int       did_sig = 0;
	long      rflags = ROLE_COMPOSE;
	PAT_STATE dummy;

	pp = &(body->nested.part->next);

	if(nonempty_patterns(rflags, &dummy)){
	    /*
	     * This is really more like Compose, even though it
	     * is called Forward.
	     */
	    if(confirm_role(rflags, &role))
	      role = combine_inherited_role(role);
	    else{			/* cancel */
		role = NULL;
		cmd_cancelled("Composition");
		goto bomb;
	    }
	}

	if(role)
	  q_status_message1(SM_ORDER, 3, 4, _("Composing using role \"%s\""),
			    role->nick);

	if((sig = detoken(role, NULL, 2, 0, 1, NULL, NULL)) != NULL){
	    if(*sig){
		so_puts((STORE_S *)body->nested.part->body.contents.text.data,
			sig);
		did_sig++;
	    }

	    fs_give((void **)&sig);
	}

	/* so we don't have an empty part */
	if(!did_sig)
	  so_puts((STORE_S *)body->nested.part->body.contents.text.data, "\n");
    }
    else{
	q_status_message(SM_ORDER | SM_DING, 3, 4,
			 _("Problem creating space for message text"));
	goto bomb;
    }


    /*---- create the attachment, and write abook entry into it ----*/
    *pp             = mail_newbody_part();
    pb              = &((*pp)->body);
    pb->type        = TYPETEXT;
    pb->encoding    = ENCOTHER;  /* let data decide */
    pb->id          = generate_message_id();
    pb->subtype     = cpystr("DIRECTORY");
    if(agg && as.selections > 1)
      pb->description = cpystr("Alpine addressbook entries");
    else
      pb->description = cpystr("Alpine addressbook entry");

    pb->parameter = NULL;
    set_parameter(&pb->parameter, "profile", "vCard");

    if((pb->contents.text.data = (void *)so_get(CharStar, NULL, EDIT_ACCESS)) != NULL){
	int are_some_unqualified = 0, expand_nicks = 0;
	adrbk_cntr_t num;
	PerAddrBook *pab;
	EXPANDED_S  *next_one;
	 
	gf_set_so_writec(&pc, (STORE_S *) pb->contents.text.data);
    
	if(agg){
	    for(i = 0; i < as.n_addrbk && !are_some_unqualified; i++){

		pab = &as.adrbks[i];
		if(pab->address_book)
		  next_one = pab->address_book->selects;
		else
		  continue;
		
		while((num = entry_get_next(&next_one)) != NO_NEXT &&
		      !are_some_unqualified){

		    abe = adrbk_get_ae(pab->address_book, (a_c_arg_t) num);
		    if(abe->tag == Single){
			if(abe->addr.addr && abe->addr.addr[0]
			   && !strindex(abe->addr.addr, '@'))
			  are_some_unqualified++;
		    }
		    else{
			char **ll;

			for(ll = abe->addr.list; ll && *ll; ll++){
			    if(!strindex(*ll, '@')){
				are_some_unqualified++;
				break;
			    }
			}
		    }
		}
	    }
	}
	else{
	    /*
	     * Search through the addresses to see if there are any
	     * that are unqualified, and so would be different if
	     * expanded.
	     */
	    if(abe->tag == Single){
		if(abe->addr.addr && abe->addr.addr[0]
		   && !strindex(abe->addr.addr, '@'))
		  are_some_unqualified++;
	    }
	    else{
		char **ll;

		for(ll = abe->addr.list; ll && *ll; ll++){
		    if(!strindex(*ll, '@')){
			are_some_unqualified++;
			break;
		    }
		}
	    }
	}

	if(are_some_unqualified){
	    switch(want_to(_("Expand nicknames"), 'y', 'x', h_ab_forward,WT_NORM)){
	      case 'x':
		gf_clear_so_writec((STORE_S *) pb->contents.text.data);
		q_status_message(SM_INFO, 0, 2, _("Address book forward cancelled"));
		goto bomb;
		
	      case 'y':
		expand_nicks = 1;
		break;
		
	      case 'n':
		expand_nicks = 0;
		break;
	    }

	    ps->mangled_footer = 1;
	}

	if(agg){
	    for(i = 0; i < as.n_addrbk; i++){

		pab = &as.adrbks[i];
		if(pab->address_book)
		  next_one = pab->address_book->selects;
		else
		  continue;
		
		while((num = entry_get_next(&next_one)) != NO_NEXT){

		    abe = adrbk_get_ae(pab->address_book, (a_c_arg_t) num);
		    if(!(vinfo=prepare_abe_for_vcard(ps, abe, expand_nicks))){
			gf_clear_so_writec((STORE_S *) pb->contents.text.data);
			goto bomb;
		    }
		    else{
			write_single_vcard_entry(ps, pc, vinfo);
			free_vcard_info(&vinfo);
		    }
		}
	    }
	}
	else{
	    if(!(vinfo=prepare_abe_for_vcard(ps, abe, expand_nicks))){
		gf_clear_so_writec((STORE_S *) pb->contents.text.data);
		goto bomb;
	    }
	    else{
		write_single_vcard_entry(ps, pc, vinfo);
		free_vcard_info(&vinfo);
	    }
	}

	/* This sets parameter charset, if necessary, and encoding */
	set_mime_type_by_grope(pb);
	set_charset_possibly_to_ascii(pb, "UTF-8");
	pb->size.bytes =
		strlen((char *)so_text((STORE_S *)pb->contents.text.data));
    }
    else{
	q_status_message(SM_ORDER | SM_DING, 3, 4,
			 _("Problem creating space for message text"));
	goto bomb;
    }

    gf_clear_so_writec((STORE_S *) pb->contents.text.data);

    pine_send(outgoing, &body, _("FORWARDING ADDRESS BOOK ENTRY"), role, NULL,
 	      NULL, NULL, NULL, NULL, 0);
    
    ps->mangled_screen = 1;
    ret = 1;

bomb:
    if(outgoing)
      mail_free_envelope(&outgoing);

    if(body)
      pine_free_body(&body);
    
    free_action(&role);
    return(ret);
}


/*
 * Given an Adrbk_Entry fill in some of the fields in a VCARD_INFO_S
 * for use by write_single_vcard_entry. The returned structure is freed
 * by the caller.
 */
VCARD_INFO_S *
prepare_abe_for_vcard(struct pine *ps, AdrBk_Entry *abe, int expand_nicks)
{
    VCARD_INFO_S *vinfo = NULL;
    char         *init_addr = NULL, *addr = NULL, *astring;
    int           cnt;
    ADDRESS      *adrlist = NULL;

    if(!abe)
      return(vinfo);

    vinfo = (VCARD_INFO_S *) fs_get(sizeof(*vinfo));
    memset((void *) vinfo, 0, sizeof(*vinfo));

    if(abe->nickname && abe->nickname[0]){
	vinfo->nickname = (char **) fs_get((1+1) * sizeof(char *));
	vinfo->nickname[0] = cpystr(abe->nickname);
	vinfo->nickname[1] = NULL;
    }

    if(abe->fcc && abe->fcc[0]){
	vinfo->fcc = (char **) fs_get((1+1) * sizeof(char *));
	vinfo->fcc[0] = cpystr(abe->fcc);
	vinfo->fcc[1] = NULL;
    }

    if(abe->extra && abe->extra[0]){
	vinfo->note = (char **) fs_get((1+1) * sizeof(char *));
	vinfo->note[0] = cpystr(abe->extra);
	vinfo->note[1] = NULL;
    }
    
    if(abe->fullname && abe->fullname[0]){
	char *fn, *last = NULL, *middle = NULL, *first = NULL;

	fn = adrbk_formatname(abe->fullname, &first, &last);
	if(fn){
	    if(*fn){
		vinfo->fullname = (char **)fs_get((1+1) * sizeof(char *));
		vinfo->fullname[0] = fn;
		vinfo->fullname[1] = NULL;
	    }
	    else
	      fs_give((void **)&fn);
	}

	if(last && *last){
	    if(first && (middle=strindex(first, ' '))){
		*middle++ = '\0';
		middle = skip_white_space(middle);
	    }

	    vinfo->last = last;
	    vinfo->first = first;
	    vinfo->middle = middle ? cpystr(middle) : NULL;
	    first = NULL;
	    last = NULL;
	}

	if(last)
	  fs_give((void **)&last);
	if(first)
	  fs_give((void **)&first);
    }

    /* expand nicknames and fully-qualify unqualified names */
    if(expand_nicks){
	char    *error = NULL;
	BuildTo  bldto;
	ADDRESS *a;

	if(abe->tag == Single)
	  init_addr = cpystr(abe->addr.addr);
	else{
	    char **ll;
	    char  *p;
	    long   length;

	    /* figure out how large a string we need to allocate */
	    length = 0L;
	    for(ll = abe->addr.list; ll && *ll; ll++)
	      length += (strlen(*ll) + 2);

	    if(length)
	      length -= 2L;
	    
	    init_addr = (char *)fs_get((size_t)(length+1L) * sizeof(char));
	    p = init_addr;
	
	    for(ll = abe->addr.list; ll && *ll; ll++){
		sstrncpy(&p, *ll, length-(p-init_addr));
		if(*(ll+1))
		  sstrncpy(&p, ", ", length-(p-init_addr));
	    }

	    init_addr[length] = '\0';
	}

	bldto.type    = Str;
	bldto.arg.str = init_addr; 
	our_build_address(bldto, &addr, &error, NULL, NULL);
	if(error){
	    q_status_message1(SM_ORDER, 3, 4, "%s", error);
	    fs_give((void **)&error);
	    free_vcard_info(&vinfo);
	    return(NULL);
	}

	if(addr)
	  rfc822_parse_adrlist(&adrlist, addr, ps->maildomain);
	
	for(cnt = 0, a = adrlist; a; a = a->next)
	  cnt++;

	vinfo->email = (char **)fs_get((cnt+1) * sizeof(char *));

	for(cnt = 0, a = adrlist; a; a = a->next){
	    char    *bufp;
	    ADDRESS *next_addr;
	    size_t   len;

	    next_addr = a->next;
	    a->next = NULL;
	    len = est_size(a);
	    bufp = (char *) fs_get(len * sizeof(char));
	    astring = addr_string(a, bufp, len);
	    a->next = next_addr;
	    vinfo->email[cnt++] = cpystr(astring ? astring : "");
	    fs_give((void **)&bufp);
	}

	vinfo->email[cnt] = '\0';
    }
    else{ /* don't expand or qualify */
	if(abe->tag == Single){
	    astring =
	       (abe->addr.addr && abe->addr.addr[0]) ? abe->addr.addr : "";
	    vinfo->email = (char **)fs_get((1+1) * sizeof(char *));
	    vinfo->email[0] = cpystr(astring);
	    vinfo->email[1] = '\0';
	}
	else{
	    char **ll;

	    for(cnt = 0, ll = abe->addr.list; ll && *ll; ll++)
	      cnt++;

	    vinfo->email = (char **)fs_get((cnt+1) * sizeof(char *));
	    for(cnt = 0, ll = abe->addr.list; ll && *ll; ll++)
	      vinfo->email[cnt++] = cpystr(*ll);

	    vinfo->email[cnt] = '\0';
	}
    }

    return(vinfo);
}


void
free_vcard_info(VCARD_INFO_S **vinfo)
{
    if(vinfo && *vinfo){
	if((*vinfo)->nickname)
	  free_list_array(&(*vinfo)->nickname);
	if((*vinfo)->fullname)
	  free_list_array(&(*vinfo)->fullname);
	if((*vinfo)->fcc)
	  free_list_array(&(*vinfo)->fcc);
	if((*vinfo)->note)
	  free_list_array(&(*vinfo)->note);
	if((*vinfo)->title)
	  free_list_array(&(*vinfo)->title);
	if((*vinfo)->tel)
	  free_list_array(&(*vinfo)->tel);
	if((*vinfo)->email)
	  free_list_array(&(*vinfo)->email);

	if((*vinfo)->first)
	  fs_give((void **)&(*vinfo)->first);
	if((*vinfo)->middle)
	  fs_give((void **)&(*vinfo)->middle);
	if((*vinfo)->last)
	  fs_give((void **)&(*vinfo)->last);

	fs_give((void **)vinfo);
    }
}


/*
 * 
 */
void
write_single_vcard_entry(struct pine *ps, gf_io_t pc, VCARD_INFO_S *vinfo)
{
    char  *decoded, *tmp2, *tmp = NULL, *hdr;
    char **ll;
    int    i, did_fn = 0, did_n = 0;
    int    cr;
    char   eol[3];
#define FOLD_BY 75

    if(!vinfo)
      return;

#if defined(DOS) || defined(OS2)
      cr = 1;
#else
      cr = 0;
#endif

    if(cr)
      strncpy(eol, "\r\n", sizeof(eol));
    else
      strncpy(eol, "\n", sizeof(eol));

    eol[sizeof(eol)-1] = '\0';

    gf_puts("BEGIN:VCARD", pc);
    gf_puts(eol, pc);
    gf_puts("VERSION:3.0", pc);
    gf_puts(eol, pc);

    for(i = 0; i < 7; i++){
	switch(i){
	  case 0:
	    ll = vinfo->nickname;
	    hdr = "NICKNAME:";
	    break;

	  case 1:
	    ll = vinfo->fullname;
	    hdr = "FN:";
	    break;

	  case 2:
	    ll = vinfo->email;
	    hdr = "EMAIL:";
	    break;

	  case 3:
	    ll = vinfo->title;
	    hdr = "TITLE:";
	    break;

	  case 4:
	    ll = vinfo->note;
	    hdr = "NOTE:";
	    break;

	  case 5:
	    ll = vinfo->fcc;
	    hdr = "X-FCC:";
	    break;

	  case 6:
	    ll = vinfo->tel;
	    hdr = "TEL:";
	    break;
	  
	  default:
	    panic("can't happen in write_single_vcard_entry");
	}

	for(; ll && *ll; ll++){
	    decoded = (char *)rfc1522_decode_to_utf8((unsigned char *)(tmp_20k_buf), SIZEOF_20KBUF, *ll);

	    tmp = vcard_escape(decoded);
	    if(tmp){
		if((tmp2 = fold(tmp, FOLD_BY, FOLD_BY, hdr, " ", FLD_PWS | (cr ? FLD_CRLF : 0))) != NULL){
		    gf_puts(tmp2, pc);
		    fs_give((void **)&tmp2);
		    if(i == 1)
		      did_fn++;
		}

		fs_give((void **)&tmp);
	    }
	}
    }

    if(vinfo->last && vinfo->last[0]){
	char *pl, *pf, *pm;

	pl = vcard_escape(vinfo->last);
	pf = (vinfo->first && *vinfo->first) ? vcard_escape(vinfo->first)
					     : NULL;
	pm = (vinfo->middle && *vinfo->middle) ? vcard_escape(vinfo->middle)
					       : NULL;
	snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%s%s%s%s%s",
		(pl && *pl) ? pl : "", 
		((pf && *pf) || (pm && *pm)) ? ";" : "",
		(pf && *pf) ? pf : "", 
		(pm && *pm) ? ";" : "", 
		(pm && *pm) ? pm : "");

	if((tmp2 = fold(tmp_20k_buf, FOLD_BY, FOLD_BY, "N:", " ",
		       FLD_PWS | (cr ? FLD_CRLF : 0))) != NULL){
	    gf_puts(tmp2, pc);
	    fs_give((void **)&tmp2);
	    did_n++;
	}

	if(pl)
	  fs_give((void **)&pl);
	if(pf)
	  fs_give((void **)&pf);
	if(pm)
	  fs_give((void **)&pm);
    }

    /*
     * These two types are required in draft-ietf-asid-mime-vcard-06, which
     * is April 98 and is in last call.
     */
    if(!did_fn || !did_n){
	if(did_n){
	    snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%s%s%s%s%s",
		    (vinfo->first && *vinfo->first) ? vinfo->first : "",
		    (vinfo->first && *vinfo->first &&
		     vinfo->middle && *vinfo->middle) ? " " : "", 
		    (vinfo->middle && *vinfo->middle) ? vinfo->middle : "",
		    (((vinfo->first && *vinfo->first) ||
		      (vinfo->middle && *vinfo->middle)) &&
		     vinfo->last && *vinfo->last) ? " " : "", 
		    (vinfo->last && *vinfo->last) ? vinfo->last : "");

	    tmp = vcard_escape(tmp_20k_buf);
	    if(tmp){
		if((tmp2 = fold(tmp, FOLD_BY, FOLD_BY, "FN:", " ",
			       FLD_PWS | (cr ? FLD_CRLF : 0))) != NULL){
		    gf_puts(tmp2, pc);
		    fs_give((void **)&tmp2);
		    did_n++;
		}

		fs_give((void **)&tmp);
	    }
	}
	else{
	    if(!did_fn){
		gf_puts("FN:<Unknown>", pc);
		gf_puts(eol, pc);
	    }

	    gf_puts("N:<Unknown>", pc);
	    gf_puts(eol, pc);
	}
    }

    gf_puts("END:VCARD", pc);
    gf_puts(eol, pc);
}


/*
 * 
 */
void
write_single_tab_entry(gf_io_t pc, VCARD_INFO_S *vinfo)
{
    char  *decoded, *tmp = NULL;
    char **ll;
    int    i, first;
    char  *eol;

    if(!vinfo)
      return;

#if defined(DOS) || defined(OS2)
      eol = "\r\n";
#else
      eol = "\n";
#endif

    for(i = 0; i < 4; i++){
	switch(i){
	  case 0:
	    ll = vinfo->nickname;
	    break;

	  case 1:
	    ll = vinfo->fullname;
	    break;

	  case 2:
	    ll = vinfo->email;
	    break;

	  case 3:
	    ll = vinfo->note;
	    break;

	  default:
	    panic("can't happen in write_single_tab_entry");
	}

	if(i)
	  gf_puts("\t", pc);

	for(first = 1; ll && *ll; ll++){

	    decoded = (char *)rfc1522_decode_to_utf8((unsigned char *)(tmp_20k_buf), SIZEOF_20KBUF, *ll);
	    tmp = vcard_escape(decoded);
	    if(tmp){
		if(i == 2 && !first)
		  gf_puts(",", pc);
		else
		  first = 0;

		gf_puts(tmp, pc);
		fs_give((void **)&tmp);
	    }
	}
    }

    gf_puts(eol, pc);
}


/*
 * for ab_save percent done
 */
static int total_to_copy;
static int copied_so_far;
int
percent_done_copying(void)
{
    return((copied_so_far * 100) / total_to_copy);
}

int
cmp_action_list(const qsort_t *a1, const qsort_t *a2)
{
    ACTION_LIST_S *x = (ACTION_LIST_S *)a1;
    ACTION_LIST_S *y = (ACTION_LIST_S *)a2;

    if(x->pab != y->pab)
      return((x->pab > y->pab) ? 1 : -1);  /* order doesn't matter */
    
    /*
     * The only one that matters is when both x and y have dup lit.
     * For the others, just need to be consistent so sort will terminate.
     */
    if(x->dup){
	if(y->dup)
	  return((x->num_in_dst > y->num_in_dst) ? -1
		   : (x->num_in_dst == y->num_in_dst) ? 0 : 1);
	else
	  return(-1);
    }
    else if(y->dup)
      return(1);
    else
      return((x->num > y->num) ? -1 : (x->num == y->num) ? 0 : 1);
}


/*
 * Copy a bunch of address book entries to a particular address book.
 *
 * Args      abook -- the current addrbook handle
 *        cur_line -- the current line the cursor is on
 *    command_line -- the line to prompt on
 *             agg -- 1 if this is an aggregate copy
 *
 * Returns  1 if successful, 0 if not
 */
int
ab_save(struct pine *ps, AdrBk *abook, long int cur_line, int command_line, int agg)
{
    PerAddrBook   *pab_dst, *pab;
    SAVE_STATE_S   state;  /* For saving state of addrbooks temporarily */
    int            rc, i;
    int		   how_many_dups = 0, how_many_to_copy = 0, skip_dups = 0;
    int		   how_many_no_action = 0, ret = 1;
    int		   err = 0, need_write = 0, we_cancel = 0;
    int            act_list_size, special_case = 0;
    adrbk_cntr_t   num, new_entry_num;
    char           warn[2][MAX_NICKNAME+1];
    char           warning[MAX_NICKNAME+1];
    char           tmp[MAX(200,2*MAX_NICKNAME+80)];
    ACTION_LIST_S *action_list = NULL, *al;
    static ESCKEY_S save_or_export[] = {
	{'s', 's', "S", N_("Save")},
	{'e', 'e', "E", N_("Export")},
	{-1, 0, NULL, NULL}};

    if(!agg)
      snprintf(tmp, sizeof(tmp), _("Save highlighted entry to address book or Export to filesystem ? "));
    else if(as.selections > 1)
      snprintf(tmp, sizeof(tmp), _("Save selected entries to address book or Export to filesystem ? "));
    else if(as.selections == 1)
      snprintf(tmp, sizeof(tmp), _("Save selected entry to address book or Export to filesystem ? "));
    else
      snprintf(tmp, sizeof(tmp), _("Save to address book or Export to filesystem ? "));

    i = radio_buttons(tmp, -FOOTER_ROWS(ps), save_or_export, 's', 'x',
		      h_ab_save_exp, RB_NORM);
    switch(i){
      case 'x':
	q_status_message(SM_INFO, 0, 2, _("Address book save cancelled"));
	return(0);

      case 'e':
	return(ab_export(ps, cur_line, command_line, agg));

      case 's':
	break;

      default:
        q_status_message(SM_ORDER, 3, 3, "can't happen in ab_save");
	return(0);
    }

    pab_dst = setup_for_addrbook_add(&state, command_line, _("Save"));
    if(!pab_dst)
      goto get_out;

    pab = &as.adrbks[as.cur];

    dprint((2, "- ab_save: %s -> %s (agg=%d)-\n",
	   pab->abnick ? pab->abnick : "?",
	   pab_dst->abnick ? pab_dst->abnick : "?", agg));

    if(agg)
      act_list_size = as.selections;
    else
      act_list_size = 1;

    action_list = (ACTION_LIST_S *)fs_get((act_list_size+1) *
						    sizeof(ACTION_LIST_S));
    memset((void *)action_list, 0, (act_list_size+1) * sizeof(ACTION_LIST_S));
    al = action_list;

    if(agg){

	for(i = 0; i < as.n_addrbk; i++){
	    EXPANDED_S    *next_one;

	    pab = &as.adrbks[i];
	    if(pab->address_book)
	      next_one = pab->address_book->selects;
	    else
	      continue;

	    while((num = entry_get_next(&next_one)) != NO_NEXT){
		if(pab != pab_dst &&
		   pab->ostatus != Open &&
		   pab->ostatus != NoDisplay)
		  init_abook(pab, NoDisplay);

		if(pab->ostatus != Open && pab->ostatus != NoDisplay){
		    q_status_message1(SM_ORDER, 0, 4,
				 _("Can't re-open address book %s to save from"),
				 pab->abnick);
		    err++;
		    goto get_out;
		}

		set_act_list_member(al, (a_c_arg_t)num, pab_dst, pab, warning);
		if(al->skip)
		  how_many_no_action++;
		else{
		    if(al->dup){
			if(how_many_dups < 2 && warning[0]){
			    strncpy(warn[how_many_dups], warning, MAX_NICKNAME);
			    warn[how_many_dups][MAX_NICKNAME] = '\0';
			}
		    
			how_many_dups++;
		    }

		    how_many_to_copy++;
		}

		al++;
	    }
	}
    }
    else{
	if(is_addr(cur_line)){
	    AddrScrn_Disp *dl;

	    dl = dlist(cur_line);

	    if(dl->type == ListEnt)
	      special_case++;

	    if(pab && dl){
		num = dl->elnum;
		set_act_list_member(al, (a_c_arg_t)num, pab_dst, pab, warning);
	    }
	    else
	      al->skip = 1;

	    if(al->skip)
	      how_many_no_action++;
	    else{
		if(al->dup){
		    if(how_many_dups < 2 && warning[0]){
			strncpy(warn[how_many_dups], warning, MAX_NICKNAME);
			warn[how_many_dups][MAX_NICKNAME] = '\0';
		    }
		
		    how_many_dups++;
		}

		how_many_to_copy++;
	    }
	}
	else{
	    q_status_message(SM_ORDER, 0, 4, _("No current entry to save"));
	    goto get_out;
	}
    }

    if(how_many_to_copy == 0 && how_many_no_action == 1 && act_list_size == 1)
      special_case++;

    if(special_case){
	TA_STATE_S tas, *tasp;

	/* Not going to use the action_list now */
	if(action_list)
	  fs_give((void **)&action_list);

	tasp = &tas;
	tas.state = state;
	tas.pab   = pab_dst;
	take_this_one_entry(ps, &tasp, abook, cur_line);

	/*
	 * If take_this_one_entry or its children didn't do this for
	 * us, we do it here.
	 */
	if(tas.pab)
	  restore_state(&(tas.state));

	/*
	 * We don't have enough information to know what to return.
	 */
	return(0);
    }

    /* nothing to do (except for special Take case below) */
    if(how_many_to_copy == 0){
	if(how_many_no_action == 0){
	    err++;
	    goto get_out;
	}
	else{
	    restore_state(&state);

	    if(how_many_no_action > 1)
	      snprintf(tmp, sizeof(tmp), _("Saved %d entries to %s"), how_many_no_action, pab_dst->abnick);
	    else
	      snprintf(tmp, sizeof(tmp), _("Saved %d entry to %s"), how_many_no_action, pab_dst->abnick);

	    tmp[sizeof(tmp)-1] = '\0';
	    q_status_message(SM_ORDER, 0, 4, tmp);
	    if(action_list)
	      fs_give((void **)&action_list);

	    return(ret);
	}
    }

    /*
     * If there are some nicknames which already exist in the selected
     * abook, ask user what to do.
     */
    if(how_many_dups > 0){
	if(how_many_dups == 1)
	  snprintf(tmp, sizeof(tmp), _("Entry with nickname \"%.*s\" already exists, replace "),
		  MAX_NICKNAME, warn[0]);
	else if(how_many_dups == 2)
	  snprintf(tmp, sizeof(tmp),
		  _("Nicknames \"%.*s\" and \"%.*s\" already exist, replace "),
		  MAX_NICKNAME, warn[0], MAX_NICKNAME, warn[1]);
	else
	  snprintf(tmp, sizeof(tmp), _("%d of the nicknames already exist, replace "),
		  how_many_dups);

	tmp[sizeof(tmp)-1] = '\0';
	
	switch(want_to(tmp, 'n', 'x', h_ab_copy_dups, WT_NORM)){
	  case 'n':
	    skip_dups++;
	    if(how_many_to_copy == how_many_dups){
		restore_state(&state);
		if(action_list)
		  fs_give((void **)&action_list);

		q_status_message(SM_INFO, 0, 2, _("Address book save cancelled"));
		return(ret);
	    }

	    break;

	  case 'y':
	    break;

	  case 'x':
	    err++;
	    goto get_out;
	}
    }

    /*
     * Because the deletes happen immediately we have to delete from high
     * entry number towards lower entry numbers so that we are deleting
     * the correct entries. In order to do that we'll sort the action_list
     * to give us a safe order.
     */
    if(!skip_dups && how_many_dups > 1)
      qsort((qsort_t *)action_list, (size_t)as.selections, sizeof(*action_list),
	    cmp_action_list);

    /*
     * Set up the busy alarm percent counters.
     */
    total_to_copy = how_many_to_copy - (skip_dups ? how_many_dups : 0);
    copied_so_far = 0;
    we_cancel = busy_cue(_("Saving entries"),
			 (total_to_copy > 4) ? percent_done_copying : NULL, 0);

    /*
     * Add the list of entries to the destination abook.
     */
    for(al = action_list; al && al->pab; al++){
	AdrBk_Entry   *abe;
	
	if(al->skip || (skip_dups && al->dup))
	  continue;

	if(!(abe = adrbk_get_ae(al->pab->address_book, (a_c_arg_t) al->num))){
	    q_status_message1(SM_ORDER | SM_DING, 3, 5,
			      _("Error saving entry: %s"),
			      error_description(errno));
	    err++;
	    goto get_out;
	}

	/*
	 * Delete existing dups and replace them.
	 */
	if(al->dup){

	    /* delete the existing entry */
	    rc = 0;
	    if(adrbk_delete(pab_dst->address_book,
			    (a_c_arg_t)al->num_in_dst, 1, 0, 0, 0) == 0){
		need_write++;
	    }
	    else{
		q_status_message2(SM_ORDER | SM_DING, 3, 5,
				 _("Error replacing entry in %s: %s"),
				 pab_dst->abnick,
				 error_description(errno));
		err++;
		goto get_out;
	    }
	}

	/*
	 * Now we have a clean slate to work with.
	 * Add (sorted in correctly) or append abe to the destination
	 * address book.
	 */
	if(total_to_copy <= 1)
	  rc = adrbk_add(pab_dst->address_book,
		         NO_NEXT,
		         abe->nickname,
		         abe->fullname,
		         abe->tag == Single ? abe->addr.addr : NULL,
		         abe->fcc,
		         abe->extra,
		         abe->tag,
		         &new_entry_num,
		         (int *)NULL,
		         0,
		         0,
		         0);
	else
	  rc = adrbk_append(pab_dst->address_book,
		            abe->nickname,
		            abe->fullname,
		            abe->tag == Single ? abe->addr.addr : NULL,
		            abe->fcc,
		            abe->extra,
		            abe->tag,
			    &new_entry_num);

	if(rc == 0)
	  need_write++;
	
	/*
	 * If the entry we copied is a list, we also have to add
	 * the list members to the copy.
	 */
	if(rc == 0 && abe->tag == List){
	    int save_sort_rule;

	    /*
	     * We want it to copy the list in the exact order
	     * without sorting it.
	     */
	    save_sort_rule = pab_dst->address_book->sort_rule;
	    pab_dst->address_book->sort_rule = AB_SORT_RULE_NONE;

	    rc = adrbk_nlistadd(pab_dst->address_book,
				(a_c_arg_t)new_entry_num, NULL, NULL,
				abe->addr.list,
				0, 0, 0);

	    pab_dst->address_book->sort_rule = save_sort_rule;
	}

	if(rc != 0){
	    if(abe && abe->nickname)
	      q_status_message2(SM_ORDER | SM_DING, 3, 5, _("Error saving %s: %s"), abe->nickname, error_description(errno));
	    else
	      q_status_message1(SM_ORDER | SM_DING, 3, 5, _("Error saving entry: %s"), error_description(errno));
	    err++;
	    goto get_out;
	}

	copied_so_far++;
    }

    if(need_write){
	int sort_happened = 0;

	if(adrbk_write(pab_dst->address_book, 0, NULL, &sort_happened, 0, 1)){
	    err++;
	    goto get_out;
	}

	if(sort_happened)
	  ps_global->mangled_screen = 1;
    }

get_out:
    if(we_cancel)
      cancel_busy_cue(1);

    restore_state(&state);
    if(action_list)
      fs_give((void **)&action_list);

    ps_global->mangled_footer = 1;

    if(err){
	ret = 0;
	if(need_write)
	  q_status_message(SM_ORDER | SM_DING, 3, 4,
			   _("Save only partially completed"));
	else
	  q_status_message(SM_INFO, 0, 2, _("Address book save cancelled"));
    }
    else if (how_many_to_copy + how_many_no_action -
	     (skip_dups ? how_many_dups : 0) > 0){

	ret = 1;
	snprintf(tmp, sizeof(tmp), "Saved %d %s to %s",
		how_many_to_copy + how_many_no_action -
		(skip_dups ? how_many_dups : 0),
		((how_many_to_copy + how_many_no_action -
		  (skip_dups ? how_many_dups : 0)) > 1) ? "entries" : "entry",
		pab_dst->abnick);
	tmp[sizeof(tmp)-1] = '\0';
	q_status_message(SM_ORDER, 0, 4, tmp);
    }

    return(ret);
}


/*
 * Warn should point to an array of size MAX_NICKNAME+1.
 */
void
set_act_list_member(ACTION_LIST_S *al, a_c_arg_t numarg, PerAddrBook *pab_dst, PerAddrBook *pab, char *warn)
{
    AdrBk_Entry   *abe1, *abe2;
    adrbk_cntr_t   num;

    num = (adrbk_cntr_t)numarg;

    al->pab = pab;
    al->num = num;

    /* skip if they're copying from and to same addrbook */
    if(pab == pab_dst)
      al->skip = 1;
    else{
	abe1 = adrbk_get_ae(pab->address_book, numarg);
	if(abe1 && abe1->nickname && abe1->nickname[0]){
	    adrbk_cntr_t dst_enum;

	    abe2 = adrbk_lookup_by_nick(pab_dst->address_book,
					abe1->nickname, &dst_enum);
	    /*
	     * This nickname already exists in the destn address book.
	     */
	    if(abe2){
		/* If it isn't different, no problem. Check it out. */
		if(abes_are_equal(abe1, abe2))
		  al->skip = 1;
		else{
		    strncpy(warn, abe1->nickname, MAX_NICKNAME);
		    warn[MAX_NICKNAME] = '\0';
		    al->dup = 1;
		    al->num_in_dst = dst_enum;
		}
	    }
	}
    }
}


/*
 * Print out the display list.
 */
int
ab_print(int agg)
{
    int do_entry = 0, curopen;
    char *prompt;

    dprint((2, "- ab_print -\n"));

    curopen = cur_is_open();
    if(!agg && curopen){
	static ESCKEY_S prt[] = {
	    {'a', 'a', "A", N_("AddressBook")},
	    {'e', 'e', "E", N_("Entry")},
	    {-1, 0, NULL, NULL}};

	prompt = _("Print Address Book or just this Entry? ");
	switch(radio_buttons(prompt, -FOOTER_ROWS(ps_global), prt, 'a', 'x',
			     NO_HELP, RB_NORM)){
	  case 'x' :
	    q_status_message(SM_INFO, 0, 2, _("Address book print cancelled"));
	    ps_global->mangled_footer = 1;
	    return 0;

	  case 'e':
	    do_entry = 1;
	    break;

	  default:
	  case 'a':
	    break;
	}
    }

    /* TRANSLATORS: This is input for
       Print something1 using something2. The thing we're
       defining here is something1. */
    if(agg)
      prompt = _("selected entries");
    else{
	if(!curopen)
	  prompt = _("address book list");
	else if(do_entry)
	  prompt = _("entry");
	else
	  prompt = _("address book");
    }

    if(open_printer(prompt) == 0){
	DL_CACHE_S     dlc_buf, *match_dlc;
	AddrScrn_Disp *dl; 
	AdrBk_Entry   *abe;
	long           save_line;
	char          *addr;
	char           spaces[100];
	char           more_spaces[100];
	char           b[500];
	int            abook_indent;

	save_line = as.top_ent + as.cur_row;
	match_dlc = get_dlc(save_line);
	dlc_buf   = *match_dlc;
	match_dlc = &dlc_buf;

	if(do_entry){		/* print an individual addrbook entry */

	    abook_indent = utf8_width(_("Nickname")) + 2;

	    snprintf(spaces, sizeof(spaces), "%*.*s", abook_indent+2, abook_indent+2, "");
	    snprintf(more_spaces, sizeof(more_spaces), "%*.*s",
		     abook_indent+4, abook_indent+4, "");

	    dl = dlist(save_line);
	    abe = ae(save_line);

	    if(abe){
		int      are_some_unqualified = 0, expand_nicks = 0;
		char    *string, *tmp;
		ADDRESS *adrlist = NULL;

		/*
		 * Search through the addresses to see if there are any
		 * that are unqualified, and so would be different if
		 * expanded.
		 */
		if(abe->tag == Single){
		    if(abe->addr.addr && abe->addr.addr[0]
		       && !strindex(abe->addr.addr, '@'))
		      are_some_unqualified++;
		}
		else{
		    char **ll;

		    for(ll = abe->addr.list; ll && *ll; ll++){
			if(!strindex(*ll, '@')){
			    are_some_unqualified++;
			    break;
			}
		    }
		}

		if(are_some_unqualified){
		    switch(want_to("Expand nicknames", 'y', 'x', h_ab_forward,
								    WT_NORM)){
		      case 'x':
			q_status_message(SM_INFO, 0, 2, _("Address book print cancelled"));
			ps_global->mangled_footer = 1;
			return 0;
			
		      case 'y':
			expand_nicks = 1;
			break;
			
		      case 'n':
			expand_nicks = 0;
			break;
		    }
		}

		/* expand nicknames and fully-qualify unqualified names */
		if(expand_nicks){
		    char    *error = NULL;
		    BuildTo  bldto;
		    char    *init_addr = NULL;

		    if(abe->tag == Single)
		      init_addr = cpystr(abe->addr.addr);
		    else{
			char **ll;
			char  *p;
			long   length;

			/* figure out how large a string we need to allocate */
			length = 0L;
			for(ll = abe->addr.list; ll && *ll; ll++)
			  length += (strlen(*ll) + 2);

			if(length)
			  length -= 2L;
			
			init_addr = (char *)fs_get((size_t)(length+1L) *
								sizeof(char));
			p = init_addr;
		    
			for(ll = abe->addr.list; ll && *ll; ll++){
			    sstrncpy(&p, *ll, length-(p-init_addr));
			    if(*(ll+1))
			      sstrncpy(&p, ", ", length-(p-init_addr));
			}

			init_addr[length] = '\0';
		    }

		    bldto.type    = Str;
		    bldto.arg.str = init_addr; 
		    our_build_address(bldto, &addr, &error, NULL, NULL);
		    if(init_addr)
		      fs_give((void **)&init_addr);

		    if(error){
			q_status_message1(SM_ORDER, 0, 4, "%s", error);
			fs_give((void **)&error);

			ps_global->mangled_footer = 1;
			return 0;
		    }

		    if(addr){
			rfc822_parse_adrlist(&adrlist, addr,
					     ps_global->maildomain);
			fs_give((void **)&addr);
		    }

		    /* Will use adrlist to do the printing below */
		}

		tmp = abe->nickname ? abe->nickname : "";
		string = (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf, SIZEOF_20KBUF, tmp);
		utf8_snprintf(b, sizeof(b), "%-*.*w: ", abook_indent, abook_indent, _("Nickname"));
		if((tmp = fold(string, 80, 80, b, spaces, FLD_NONE)) != NULL){
		    print_text(tmp);
		    fs_give((void **)&tmp);
		}

		tmp = abe->fullname ? abe->fullname : "";
		string = (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf, SIZEOF_20KBUF, tmp);
		utf8_snprintf(b, sizeof(b), "%-*.*w: ", abook_indent, abook_indent, _("Fullname"));
		if((tmp = fold(string, 80, 80, b, spaces, FLD_NONE)) != NULL){
		    print_text(tmp);
		    fs_give((void **)&tmp);
		}

		tmp = abe->fcc ? abe->fcc : "";
		string = (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf, SIZEOF_20KBUF, tmp);
		utf8_snprintf(b, sizeof(b), "%-*.*w: ", abook_indent, abook_indent, _("Fcc"));
		if((tmp = fold(string, 80, 80, b, spaces, FLD_NONE)) != NULL){
		    print_text(tmp);
		    fs_give((void **)&tmp);
		}

		tmp = abe->extra ? abe->extra : "";
		{unsigned char *p, *bb = NULL;
		 size_t n, len;
		 if((n = 4*strlen(tmp)) > SIZEOF_20KBUF-1){
		     len = n+1;
		     p = bb = (unsigned char *)fs_get(len * sizeof(char));
		 }
		 else{
		     len = SIZEOF_20KBUF;
		     p = (unsigned char *)tmp_20k_buf;
		 }

		 string = (char *)rfc1522_decode_to_utf8(p, len, tmp);
		 utf8_snprintf(b, len, "%-*.*w: ", abook_indent, abook_indent, _("Comment"));
		 if((tmp = fold(string, 80, 80, b, spaces, FLD_NONE)) != NULL){
		    print_text(tmp);
		    fs_give((void **)&tmp);
		 }

		 if(bb)
		   fs_give((void **)&bb);
		}

		/*
		 * Print addresses
		 */

		if(expand_nicks){
		    ADDRESS *a;

		    for(a = adrlist; a; a = a->next){
			char    *bufp;
			ADDRESS *next_addr;
			size_t   len;

			next_addr = a->next;
			a->next = NULL;
			len = est_size(a);
			bufp = (char *) fs_get(len * sizeof(char));
			tmp = addr_string(a, bufp, len);
			a->next = next_addr;
			string = (char *)rfc1522_decode_to_utf8((unsigned char *) tmp_20k_buf+10000,
								SIZEOF_20KBUF-10000, tmp);
			utf8_snprintf(b, sizeof(b), "%-*.*w: ", abook_indent, abook_indent, _("Addresses"));
			if((tmp = fold(string, 80, 80,
				      (a == adrlist) ? b : spaces,
				      more_spaces, FLD_NONE)) != NULL){
			    print_text(tmp);
			    fs_give((void **)&tmp);
			}

			fs_give((void **)&bufp);
		    }

		    if(adrlist)
		      mail_free_address(&adrlist);
		    else{
			utf8_snprintf(b, sizeof(b), "%-*.*w:\n", abook_indent, abook_indent, _("Addresses"));
		        print_text(b);
		    }
		}
		else{ /* don't expand or qualify */
		    if(abe->tag == Single){
			tmp = abe->addr.addr ? abe->addr.addr : "";
			string = (char *)rfc1522_decode_to_utf8((unsigned char *) (tmp_20k_buf+10000),
								SIZEOF_20KBUF-10000, tmp);
			utf8_snprintf(b, sizeof(b), "%-*.*w: ", abook_indent, abook_indent, _("Addresses"));
			if((tmp = fold(string, 80, 80, b,
				      more_spaces, FLD_NONE)) != NULL){
			    print_text(tmp);
			    fs_give((void **)&tmp);
			}
		    }
		    else{
			char **ll;

			if(!abe->addr.list || !abe->addr.list[0]){
			    utf8_snprintf(b, sizeof(b), "%-*.*w:\n", abook_indent, abook_indent, _("Addresses"));
			    print_text(b);
			}

			for(ll = abe->addr.list; ll && *ll; ll++){
			    string = (char *)rfc1522_decode_to_utf8((unsigned char *) (tmp_20k_buf+10000),
								    SIZEOF_20KBUF-10000, *ll);
			    utf8_snprintf(b, sizeof(b), "%-*.*w: ", abook_indent, abook_indent, _("Addresses"));
			    if((tmp = fold(string, 80, 80,
					  (ll == abe->addr.list)
					    ? b :  spaces,
					  more_spaces, FLD_NONE)) != NULL){
				print_text(tmp);
				fs_give((void **)&tmp);
			    }
			}
		    }
		}
	    }
	}
	else{
	    long         lineno;
	    char         lbuf[6*MAX_SCREEN_COLS + 1];
	    char        *p;
	    int          i, savecur, savezoomed;
	    OpenStatus   savestatus;
	    PerAddrBook *pab;

	    if(agg){		/* print all selected entries */
		if(F_ON(F_CMBND_ABOOK_DISP,ps_global)){
		    savezoomed = as.zoomed;
		    /*
		     * Fool display code into thinking display is zoomed, so
		     * we'll skip the unselected entries. Since this feature
		     * causes all abooks to be displayed we don't have to
		     * step through each addrbook.
		     */
		    as.zoomed = 1;
		    warp_to_beginning();
		    lineno = 0L;
		    for(dl = dlist(lineno);
			dl->type != End;
			dl = dlist(++lineno)){

			switch(dl->type){
			  case Beginning:
			  case ListClickHere:
			  case ListEmpty:
			  case ClickHereCmb:
			  case Text:
			  case TitleCmb:
			  case Empty:
			  case ZoomEmpty:
			  case AskServer:
			    continue;
			  default:
			    break;
			}

			p = get_abook_display_line(lineno, 0, NULL, NULL,
						   NULL, lbuf, sizeof(lbuf));
			print_text1("%s\n", p);
		    }

		    as.zoomed = savezoomed;
		}
		else{		/* print all selected entries */
		    savecur    = as.cur;
		    savezoomed = as.zoomed;
		    as.zoomed  = 1;

		    for(i = 0; i < as.n_addrbk; i++){
			pab = &as.adrbks[i];
			if(!(pab->address_book &&
			     any_selected(pab->address_book->selects)))
			  continue;
			
			/*
			 * Print selected entries from addrbook i.
			 * We have to put addrbook i into Open state so
			 * that the display code will work right.
			 */
			as.cur = i;
			savestatus = pab->ostatus;
			init_abook(pab, Open);
			init_disp_form(pab, ps_global->VAR_ABOOK_FORMATS, i);
			(void)calculate_field_widths();
			warp_to_beginning();
			lineno = 0L;

			for(dl = dlist(lineno);
			    dl->type != End;
			    dl = dlist(++lineno)){

			    switch(dl->type){
			      case Beginning:
			      case ListClickHere:
			      case ClickHereCmb:
				continue;
			      default:
				break;
			    }

			    p = get_abook_display_line(lineno, 0, NULL, NULL,
						       NULL, lbuf,sizeof(lbuf));
			    print_text1("%s\n", p);
			}

			init_abook(pab, savestatus);
		    }

		    as.cur     = savecur;
		    as.zoomed  = savezoomed;
		    /* restore the display for the current addrbook  */
		    init_disp_form(&as.adrbks[as.cur],
				   ps_global->VAR_ABOOK_FORMATS, as.cur);
		}
	    }
	    else{	/* print either the abook list or a single abook */
		int anum;
		DL_CACHE_S *dlc;

		savezoomed = as.zoomed;
		as.zoomed = 0;

		if(curopen){	/* print a single address book */
		    anum = adrbk_num_from_lineno(as.top_ent+as.cur_row);
		    warp_to_top_of_abook(anum);
		    if(F_ON(F_CMBND_ABOOK_DISP,ps_global))
		      lineno = 0L - XTRA_TITLE_LINES_IN_OLD_ABOOK_DISP;
		    else{
			print_text("      ");
			print_text(_("ADDRESS BOOK"));
			print_text1(" %s\n\n", as.adrbks[as.cur].abnick);
			lineno = 0L;
		    }
		}
		else{		/* print the list of address books */
		    warp_to_beginning();
		    lineno = 0L;
		}

		for(dl = dlist(lineno);
		    dl->type != End && (!curopen ||
					(anum==adrbk_num_from_lineno(lineno) &&
					 (as.n_serv == 0 ||
					  ((dlc=get_dlc(lineno)) &&
					   dlc->type != DlcDirDelim1))));
		    dl = dlist(++lineno)){

		    switch(dl->type){
		      case Beginning:
		      case ListClickHere:
		      case ClickHereCmb:
			continue;
		      default:
			break;
		    }

		    p = get_abook_display_line(lineno, 0, NULL, NULL, NULL,
					       lbuf, sizeof(lbuf));
		    print_text1("%s\n", p);
		}

		as.zoomed = savezoomed;
	    }
	}

	close_printer();

	/*
	 * jump cache back to where we started so that the next
	 * request won't cause us to page through the whole thing
	 */
	if(!do_entry)
	  warp_to_dlc(match_dlc, save_line);

	ps_global->mangled_screen = 1;
    }

    ps_global->mangled_footer = 1;
    return 1;
}


/*
 * Delete address book entries.
 */
int
ab_agg_delete(struct pine *ps, int agg)
{
    int ret = 0, i, ch, rc = 0;
    PerAddrBook *pab;
    adrbk_cntr_t num, ab_count;
    char prompt[80];

    dprint((2, "- ab_agg_delete -\n"));

    if(agg){
	snprintf(prompt, sizeof(prompt), _("Really delete %d selected entries"), as.selections);
	prompt[sizeof(prompt)-1] = '\0';
	ch = want_to(prompt, 'n', 'n', NO_HELP, WT_NORM);
	if(ch == 'y'){
	    adrbk_cntr_t   newelnum, flushelnum = NO_NEXT;
	    DL_CACHE_S     dlc_save, dlc_restart, *dlc;
	    int            we_cancel = 0;
	    int            top_level_display;
	    
	    /*
	     * We want to try to put the cursor in a reasonable position
	     * on the screen when we're done. If we are in the top-level
	     * display, then we can leave it the same. If we have an
	     * addrbook opened, then we want to see if we can get back to
	     * the same entry we are currently on.
	     */
	    if(!(top_level_display = !any_ab_open())){
		dlc = get_dlc(as.top_ent+as.cur_row);
		dlc_save = *dlc;
		newelnum = dlc_save.dlcelnum;
	    }
	    
	    we_cancel = busy_cue(NULL, NULL, 1);

	    for(i = 0; i < as.n_addrbk && rc != -5; i++){
		int orig_selected, selected;

		pab = &as.adrbks[i];
		if(!pab->address_book)
		  continue;

		ab_count = adrbk_count(pab->address_book);
		rc = 0;
		selected = howmany_selected(pab->address_book->selects);
		orig_selected = selected;
		/*
		 * Because deleting an entry causes the addrbook to be
		 * immediately updated, we need to delete from higher entry
		 * numbers to lower numbers. That way, entry number n is still
		 * entry number n in the updated address book because we've
		 * only deleted entries higher than n.
		 */
		for(num = ab_count-1; selected > 0 && rc == 0; num--){
		    if(entry_is_selected(pab->address_book->selects,
				         (a_c_arg_t)num)){
			rc = adrbk_delete(pab->address_book, (a_c_arg_t)num,
					  1, 0, 0, 0);

			selected--;

			/*
			 * This is just here to help us reposition the cursor.
			 */
			if(!top_level_display && as.cur == i && rc == 0){
			    if(num >= newelnum)
			      flushelnum = num;
			    else
			      newelnum--;
			}
		    }
		}

		if(rc == 0 && orig_selected > 0){
		    int sort_happened = 0;

		    rc = adrbk_write(pab->address_book, 0, NULL, &sort_happened, 1, 0);
		    if(sort_happened)
		      ps_global->mangled_screen = 1;
		}

		if(rc && rc != -5){
		    q_status_message2(SM_ORDER | SM_DING, 3, 5,
			              "Error updating %s: %s",
				      (as.n_addrbk > 1) ? pab->abnick
						        : "address book",
				      error_description(errno));
		    dprint((1, "Error updating %s: %s\n",
			   pab->filename ? pab->filename : "?",
			   error_description(errno)));
		}
	    }

	    if(we_cancel)
	      cancel_busy_cue(-1);
	    
	    if(rc == 0){
		q_status_message(SM_ORDER, 0, 2, _("Deletions completed"));
		ret = 1;
		erase_selections();
	    }

	    if(!top_level_display){
		int lost = 0;
		long new_ent;

		if(flushelnum != dlc_save.dlcelnum){
		    /*
		     * We didn't delete current so restart there. The elnum
		     * may have changed if we deleted entries above it.
		     */
		    dlc_restart = dlc_save;
		    dlc_restart.dlcelnum  = newelnum;
		}
		else{
		    /*
		     * Current was deleted.
		     */
		    dlc_restart.adrbk_num = as.cur;
		    pab = &as.adrbks[as.cur];
		    ab_count = adrbk_count(pab->address_book);
		    if(ab_count == 0)
		      dlc_restart.type = DlcEmpty;
		    else{
			AdrBk_Entry     *abe;

			dlc_restart.dlcelnum  = MIN(newelnum, ab_count-1);
			abe = adrbk_get_ae(pab->address_book,
					   (a_c_arg_t) dlc_restart.dlcelnum);
			if(abe && abe->tag == Single)
			  dlc_restart.type = DlcSimple;
			else if(abe && abe->tag == List)
			  dlc_restart.type = DlcListHead;
			else
			  lost++;
		    }
		}

		if(lost){
		    warp_to_top_of_abook(as.cur);
		    as.top_ent = 0L;
		    new_ent    = first_selectable_line(0L);
		    if(new_ent == NO_LINE)
		      as.cur_row = 0L;
		    else
		      as.cur_row = new_ent;
		    
		    /* if it is off screen */
		    if(as.cur_row >= as.l_p_page){
			as.top_ent += (as.cur_row - as.l_p_page + 1);
			as.cur_row  = (as.l_p_page - 1);
		    }
		}
		else if(dlc_restart.type != DlcEmpty &&
			dlc_restart.dlcelnum == dlc_save.dlcelnum &&
			(F_OFF(F_CMBND_ABOOK_DISP,ps_global) || as.cur == 0)){
		    /*
		     * Didn't delete any before this line.
		     * Leave screen about the same. (May have deleted current.)
		     */
		    warp_to_dlc(&dlc_restart, as.cur_row+as.top_ent);
		}
		else{
		    warp_to_dlc(&dlc_restart, 0L);
		    /* put in middle of screen */
		    as.top_ent = first_line(0L - (long)as.l_p_page/2L);
		    as.cur_row = 0L - as.top_ent;
		}

		ps->mangled_body = 1;
	    }
	}
	else
	  cmd_cancelled("Apply Delete command");
    }

    return(ret);
}


/*
 * Delete an entry from the address book
 *
 * Args: abook        -- The addrbook handle into access library
 *       command_line -- The screen line on which to prompt
 *       cur_line     -- The entry number in the display list
 *       warped       -- We warped to a new part of the addrbook
 *
 * Result: returns 1 if an entry was deleted, 0 if not.
 *
 * The main routine above knows what to repaint because it's always the
 * current entry that's deleted.  Here confirmation is asked of the user
 * and the appropriate adrbklib functions are called.
 */
int
single_entry_delete(AdrBk *abook, long int cur_line, int *warped)
{
    char   ch, *cmd, *dname;
    char   prompt[200];
    int    rc;
    register AddrScrn_Disp *dl;
    AdrBk_Entry     *abe;
    DL_CACHE_S      *dlc_to_flush;

    dprint((2, "- single_entry_delete -\n"));

    if(warped)
      *warped = 0;

    dl  = dlist(cur_line);
    abe = adrbk_get_ae(abook, (a_c_arg_t) dl->elnum);

    switch(dl->type){
      case Simple:
	dname =	(abe->fullname && abe->fullname[0])
			? (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf,
							 SIZEOF_20KBUF, abe->fullname)
			: abe->nickname ? abe->nickname : "";
        cmd   = _("Really delete \"%s\"");
        break;

      case ListHead:
	dname =	(abe->fullname && abe->fullname[0])
			? (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf,
							 SIZEOF_20KBUF, abe->fullname)
			: abe->nickname ? abe->nickname : "";
	cmd   = _("Really delete ENTIRE list \"%s\"");
        break;

      case ListEnt:
        dname = (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf,
					       SIZEOF_20KBUF, listmem_from_dl(abook, dl));
	cmd   = _("Really delete \"%s\" from list");
        break;

      default:
        break;
    } 

    dname = dname ? dname : "";
    cmd   = cmd   ? cmd   : "";

    snprintf(prompt, sizeof(prompt), cmd, dname);
    prompt[sizeof(prompt)-1] = '\0';
    ch = want_to(prompt, 'n', 'n', NO_HELP, WT_NORM);
    if(ch == 'y'){
	dlc_to_flush = get_dlc(cur_line);
	if(dl->type == Simple || dl->type == ListHead){
	    /*--- Kill a single entry or an entire list ---*/
            rc = adrbk_delete(abook, (a_c_arg_t)dl->elnum, 1, 1, 0, 1);
	}
	else if(listmem_count_from_abe(abe) > 2){
            /*---- Kill an entry out of a list ----*/
            rc = adrbk_listdel(abook, (a_c_arg_t)dl->elnum,
		    listmem_from_dl(abook, dl));
	}
	else{
	    char *nick, *full, *addr, *fcc, *comment;
	    adrbk_cntr_t new_entry_num = NO_NEXT;

	    /*---- Convert a List to a Single entry ----*/

	    /* Save old info to be transferred */
	    nick    = cpystr(abe->nickname);
	    full    = cpystr(abe->fullname);
	    fcc     = cpystr(abe->fcc);
	    comment = cpystr(abe->extra);
	    if(listmem_count_from_abe(abe) == 2)
	      addr = cpystr(abe->addr.list[1 - dl->l_offset]);
	    else
	      addr = cpystr("");

            rc = adrbk_delete(abook, (a_c_arg_t)dl->elnum, 0, 1, 0, 0);
	    if(rc == 0)
	      adrbk_add(abook,
			NO_NEXT,
			nick,
			full,
			addr,
			fcc,
			comment,
			Single,
			&new_entry_num,
			(int *)NULL,
			1,
			0,
			1);

	    fs_give((void **)&nick);
	    fs_give((void **)&full);
	    fs_give((void **)&fcc);
	    fs_give((void **)&comment);
	    fs_give((void **)&addr);

	    if(rc == 0){
		DL_CACHE_S dlc_restart;

		dlc_restart.adrbk_num = as.cur;
		dlc_restart.dlcelnum  = new_entry_num;
		dlc_restart.type = DlcSimple;
		warp_to_dlc(&dlc_restart, 0L);
		*warped = 1;
		return 1;
	    }
	}

	if(rc == 0){
	    q_status_message(SM_ORDER, 0, 3,
		_("Entry deleted, address book updated"));
            dprint((5, "abook: Entry %s\n",
		(dl->type == Simple || dl->type == ListHead) ? "deleted"
							     : "modified"));
	    /*
	     * Remove deleted line and everything after it from
	     * the dlc cache.  Next time we try to access those lines they
	     * will get filled in with the right info.
	     */
	    flush_dlc_from_cache(dlc_to_flush);
            return 1;
        }
	else{
	    PerAddrBook     *pab;

	    if(rc != -5)
              q_status_message1(SM_ORDER | SM_DING, 3, 5,
			      _("Error updating address book: %s"),
		    error_description(errno));
	    pab = &as.adrbks[as.cur];
            dprint((1, "Error deleting entry from %s (%s): %s\n",
		   pab->abnick ? pab->abnick : "?",
		   pab->filename ? pab->filename : "?",
		   error_description(errno)));
        }

	return 0;
    }
    else{
	q_status_message(SM_INFO, 0, 2, _("Entry not deleted"));
	return 0;
    }
}


void
free_headents(struct headerentry **head)
{
    struct headerentry  *he;
    PrivateTop          *pt;

    if(head && *head){
	for(he = *head; he->name; he++)
	  if(he->bldr_private){
	      pt = (PrivateTop *)he->bldr_private;
	      free_privatetop(&pt);
	  }

	fs_give((void **)head);
    }
}


#ifdef	ENABLE_LDAP
/*
prompt::name::help::prwid::maxlen::realaddr::
builder::affected_entry::next_affected::selector::key_label::fileedit::
display_it::break_on_comma::is_attach::rich_header::only_file_chars::
single_space::sticky::dirty::start_here::blank::KS_ODATAVAR
*/
static struct headerentry headents_for_query[]={
  {"Normal Search : ",  "NormalSearch",  h_composer_qserv_qq, 16, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Name          : ",  "Name",  h_composer_qserv_cn, 16, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Surname       : ",  "SurName",  h_composer_qserv_sn, 16, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Given Name    : ",  "GivenName",  h_composer_qserv_gn, 16, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Email Address : ",  "EmailAddress",  h_composer_qserv_mail, 16, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Organization  : ",  "Organization",  h_composer_qserv_org, 16, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Org Unit      : ",  "OrganizationalUnit", h_composer_qserv_unit, 16, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Country       : ",  "Country",  h_composer_qserv_country, 16, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"State         : ",  "State",  h_composer_qserv_state, 16, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Locality      : ",  "Locality",  h_composer_qserv_locality, 16, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {" ",  "BlankLine",  NO_HELP, 1, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 1, KS_NONE},
  {"Custom Filter : ",  "CustomFilter",  h_composer_qserv_custom, 16, 0, NULL,
   NULL, NULL, NULL, NULL,  NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {NULL, NULL, NO_HELP, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE}
};
#define QQ_QQ		0
#define QQ_CN		1
#define QQ_SN		2
#define QQ_GN		3
#define QQ_MAIL		4
#define QQ_ORG		5
#define QQ_UNIT		6
#define QQ_COUNTRY	7
#define QQ_STATE	8
#define QQ_LOCALITY	9
#define QQ_BLANK	10
#define QQ_CUSTOM	11
#define QQ_END		12

static SAVED_QUERY_S *saved_params;


/*
 * Formulate a query for LDAP servers and send it and view it.
 * This is called from the Address Book screen, either from ^T from the
 * composer (selecting) or just when browsing in the address book screen
 * (selecting == 0).
 *
 * Args    ps -- Pine struct
 *  selecting -- This is set if we're just selecting an address, as opposed
 *               to browsing on an LDAP server
 *        who -- Tells us which server to query
 *      error -- An error message allocated here and freed by caller.
 *
 * Returns -- Null if not selecting, possibly an address if selecting.
 *            The address is 1522 decoded and should be freed by the caller.
 */
char *
query_server(struct pine *ps, int selecting, int *exit, int who, char **error)
{
    struct headerentry *he = NULL;
    PICO pbf;
    STORE_S *msgso = NULL;
    int i, lret, editor_result;
    int r = 4, flags;
    HelpType help = NO_HELP;
#define FILTSIZE 1000
    char fbuf[FILTSIZE+1];
    char *ret = NULL;
    LDAP_CHOOSE_S *winning_e = NULL;
    LDAP_SERV_RES_S *free_when_done = NULL;
    SAVED_QUERY_S *sq = NULL;
    static ESCKEY_S ekey[] = {
	/* TRANSLATORS: go to more complex search screen */
	{ctrl('T'), 10, "^T", N_("To complex search")},
	{-1, 0, NULL, NULL}
    };

    dprint((2, "- query_server(%s) -\n", selecting?"Selecting":""));

    if(!(ps->VAR_LDAP_SERVERS && ps->VAR_LDAP_SERVERS[0] &&
         ps->VAR_LDAP_SERVERS[0][0])){
	if(error)
	  *error = cpystr(_("No LDAP server available for lookup"));

	return(ret);
    }

    fbuf[0] = '\0';
    flags = OE_APPEND_CURRENT | OE_KEEP_TRAILING_SPACE;
    while(r == 4 || r == 3){
	/* TRANSLATORS: we're asking for a character string to search for */
	r = optionally_enter(fbuf, -FOOTER_ROWS(ps), 0, sizeof(fbuf),
			     _("String to search for : "),
			     ekey, help, &flags);
	if(r == 3)
	  help = help == NO_HELP ? h_dir_comp_search : NO_HELP;
    }

    /* strip quotes that user typed by mistake */
    (void)removing_double_quotes(fbuf);

    if(r == 1 || (r != 10 && fbuf[0] == '\0')){
	ps->mangled_footer = 1;
	if(error)
	  *error = cpystr(_("Cancelled"));

	return(ret);
    }

    editor_result = COMP_EXIT;  /* just to get right logic below */

    memset((void *)&pbf, 0, sizeof(pbf));

    if(r == 10){
	standard_picobuf_setup(&pbf);
	pbf.exittest      = pico_simpleexit;
	pbf.exit_label    = _("Search");
	pbf.canceltest    = pico_simplecancel;
	if(saved_params){
	    pbf.expander      = restore_query_parameters;
	    pbf.ctrlr_label   = _("Restore");
	}

	pbf.pine_anchor   = set_titlebar(_("SEARCH DIRECTORY SERVER"),
					  ps_global->mail_stream,
					  ps_global->context_current,
					  ps_global->cur_folder,
					  ps_global->msgmap, 
					  0, FolderName, 0, 0, NULL);
	pbf.pine_flags   |= P_NOBODY;

	/* An informational message */
	if((msgso = so_get(PicoText, NULL, EDIT_ACCESS)) != NULL){
	    pbf.msgtext = (void *)so_text(msgso);
	    /*
	     * It's nice if we can make it so these lines make sense even if
	     * they don't all make it on the screen, because the user can't
	     * scroll down to see them.  So just make each line a whole sentence
	     * that doesn't need the others below it to make sense.
	     */
	    so_puts(msgso,
_("\n Fill in some of the fields above to create a query."));
	    so_puts(msgso,
_("\n The match will be for the exact string unless you include wildcards (*)."));
	    so_puts(msgso,
_("\n All filled-in fields must match in order to be counted as a match."));
	    so_puts(msgso,
_("\n Press \"^R\" to restore previous query values (if you've queried previously)."));
	    so_puts(msgso,
_("\n \"^G\" for help specific to each item. \"^X\" to make the query, or \"^C\" to cancel."));
	}

	he = (struct headerentry *)fs_get((QQ_END+1) *
						sizeof(struct headerentry));
	memset((void *)he, 0, (QQ_END+1) * sizeof(struct headerentry));
	for(i = QQ_QQ; i <= QQ_END; i++)
	  he[i] = headents_for_query[i];

	pbf.headents = he;

	sq = copy_query_parameters(NULL);
	he[QQ_QQ].realaddr		= &sq->qq;
	he[QQ_CN].realaddr		= &sq->cn;
	he[QQ_SN].realaddr		= &sq->sn;
	he[QQ_GN].realaddr		= &sq->gn;
	he[QQ_MAIL].realaddr		= &sq->mail;
	he[QQ_ORG].realaddr		= &sq->org;
	he[QQ_UNIT].realaddr		= &sq->unit;
	he[QQ_COUNTRY].realaddr		= &sq->country;
	he[QQ_STATE].realaddr		= &sq->state;
	he[QQ_LOCALITY].realaddr	= &sq->locality;
	he[QQ_CUSTOM].realaddr		= &sq->custom;

	/* pass to pico and let user set them */
	editor_result = pico(&pbf);
	ps->mangled_screen = 1;
	standard_picobuf_teardown(&pbf);

	if(editor_result & COMP_GOTHUP)
	  hup_signal();
	else{
	    fix_windsize(ps_global);
	    init_signals();
	}
    }

    if(editor_result & COMP_EXIT &&
       ((r == 0 && *fbuf) ||
        (r == 10 && sq &&
	 (*sq->qq || *sq->cn || *sq->sn || *sq->gn || *sq->mail ||
	  *sq->org || *sq->unit || *sq->country || *sq->state ||
	  *sq->locality || *sq->custom)))){
	LDAPLookupStyle  style;
	WP_ERR_S         wp_err;
	int              need_and, mangled;
	char            *string;
	CUSTOM_FILT_S   *filter;
	SAVED_QUERY_S   *s;

	s = copy_query_parameters(sq);
	save_query_parameters(s);

	if(r == 0){
	    string = fbuf;
	    filter = NULL;
	}
	else{
	    int categories = 0;

	    categories = ((*sq->cn != '\0') ? 1 : 0) +
			 ((*sq->sn != '\0') ? 1 : 0) +
			 ((*sq->gn != '\0') ? 1 : 0) +
			 ((*sq->mail != '\0') ? 1 : 0) +
			 ((*sq->org != '\0') ? 1 : 0) +
			 ((*sq->unit != '\0') ? 1 : 0) +
			 ((*sq->country != '\0') ? 1 : 0) +
			 ((*sq->state != '\0') ? 1 : 0) +
			 ((*sq->locality != '\0') ? 1 : 0);
	    need_and = (categories > 1);

	    if((sq->cn ? strlen(sq->cn) : 0 +
	        sq->sn ? strlen(sq->sn) : 0 +
	        sq->gn ? strlen(sq->gn) : 0 +
	        sq->mail ? strlen(sq->mail) : 0 +
	        sq->org ? strlen(sq->org) : 0 +
	        sq->unit ? strlen(sq->unit) : 0 +
	        sq->country ? strlen(sq->country) : 0 +
	        sq->state ? strlen(sq->state) : 0 +
	        sq->locality ? strlen(sq->locality) : 0) > FILTSIZE - 100){
		if(error)
		  *error = cpystr(_("Search strings too long"));
		
		goto all_done;
	    }

	    if(categories > 0){

		snprintf(fbuf, sizeof(fbuf),
		   "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
			need_and ? "(&" : "",
			*sq->cn ? "(cn=" : "",
			*sq->cn ? sq->cn : "",
			*sq->cn ? ")" : "",
			*sq->sn ? "(sn=" : "",
			*sq->sn ? sq->sn : "",
			*sq->sn ? ")" : "",
			*sq->gn ? "(givenname=" : "",
			*sq->gn ? sq->gn : "",
			*sq->gn ? ")" : "",
			*sq->mail ? "(mail=" : "",
			*sq->mail ? sq->mail : "",
			*sq->mail ? ")" : "",
			*sq->org ? "(o=" : "",
			*sq->org ? sq->org : "",
			*sq->org ? ")" : "",
			*sq->unit ? "(ou=" : "",
			*sq->unit ? sq->unit : "",
			*sq->unit ? ")" : "",
			*sq->country ? "(c=" : "",
			*sq->country ? sq->country : "",
			*sq->country ? ")" : "",
			*sq->state ? "(st=" : "",
			*sq->state ? sq->state : "",
			*sq->state ? ")" : "",
			*sq->locality ? "(l=" : "",
			*sq->locality ? sq->locality : "",
			*sq->locality ? ")" : "",
			need_and ? ")" : "");
		fbuf[sizeof(fbuf)-1] = '\0';
	    }

	    if(categories > 0 || *sq->custom)
	      filter = (CUSTOM_FILT_S *)fs_get(sizeof(CUSTOM_FILT_S));

	    /* combine the configed filters with this filter */
	    if(*sq->custom){
		string = "";
		filter->filt = sq->custom;
		filter->combine = 0;
	    }
	    else if(*sq->qq && categories > 0){
		string = sq->qq;
		filter->filt = fbuf;
		filter->combine = 1;
	    }
	    else if(categories > 0){
		string = "";
		filter->filt = fbuf;
		filter->combine = 0;
	    }
	    else{
		string = sq->qq;
		filter = NULL;
	    }
	}
	
	mangled = 0;
	memset(&wp_err, 0, sizeof(wp_err));
	wp_err.mangled = &mangled;
	style = selecting ? AlwaysDisplayAndMailRequired : AlwaysDisplay;

	/* maybe coming from composer */
	fix_windsize(ps_global);
	init_sigwinch();
	clear_cursor_pos();

	lret = ldap_lookup_all(string, who, 0, style, filter, &winning_e,
			       &wp_err, &free_when_done);
	
	if(filter)
	  fs_give((void **)&filter);

	if(wp_err.mangled)
	  ps->mangled_screen = 1;

	if(wp_err.error){
	    if(status_message_remaining() && error)
	      *error = wp_err.error;
	    else
	      fs_give((void **)&wp_err.error);
	}

	if(lret == 0 && winning_e && selecting){
	    ADDRESS *addr;

	    addr = address_from_ldap(winning_e);
	    if(addr){
		if(!addr->host){
		    addr->host = cpystr("missing-hostname");
		    if(error){
			if(*error)
			  fs_give((void **)error);

			*error = cpystr(_("Missing hostname in LDAP address"));
		    }
		}

		ret = addr_list_string(addr, NULL, 1);
		if(!ret || !ret[0]){
		    if(ret)
		      fs_give((void **)&ret);
		    
		    if(exit)
		      *exit = 1;
		    
		    if(error && !*error){
			char buf[200];

			snprintf(buf, sizeof(buf), _("No email address available for \"%s\""),
				(addr->personal && *addr->personal)
				  ? addr->personal
				  : "selected entry");
			buf[sizeof(buf)-1] = '\0';
			*error = cpystr(buf);
		    }
		}

		mail_free_address(&addr);
	    }
	}
	else if(lret == -1 && exit)
	  *exit = 1;
    }
    
all_done:
    if(he)
      free_headents(&he);

    if(free_when_done)
      free_ldap_result_list(&free_when_done);

    if(winning_e)
      fs_give((void **)&winning_e);

    if(msgso)
      so_give(&msgso);

    if(sq)
      free_query_parameters(&sq);

    return(ret);
}


/*
 * View all fields of an LDAP entry while browsing.
 *
 * Args    ps -- Pine struct
 *  winning_e -- The struct containing the information about the entry
 *               to be viewed.
 */
void
view_ldap_entry(struct pine *ps, LDAP_CHOOSE_S *winning_e)
{
    STORE_S    *srcstore = NULL;
    SourceType  srctype = CharStar;
    SCROLL_S	sargs;
    HANDLE_S *handles = NULL;

    dprint((9, "- view_ldap_entry -\n"));

    if((srcstore=prep_ldap_for_viewing(ps,winning_e,srctype,&handles)) != NULL){
	memset(&sargs, 0, sizeof(SCROLL_S));
	sargs.text.text	  = so_text(srcstore);
	sargs.text.src    = srctype;
	sargs.text.desc   = _("expanded entry");
	sargs.text.handles= handles;
	sargs.bar.title   = _("DIRECTORY ENTRY");
	sargs.proc.tool   = process_ldap_cmd;
	sargs.proc.data.p = (void *) winning_e;
	sargs.help.text   = h_ldap_view;
	sargs.help.title  = _("HELP FOR DIRECTORY VIEW");
	sargs.keys.menu   = &ldap_view_keymenu;
	setbitmap(sargs.keys.bitmap);

	if(handles)
	  sargs.keys.menu->how_many = 2;
	else{
	    sargs.keys.menu->how_many = 1;
	    clrbitn(OTHER_KEY, sargs.keys.bitmap);
	}

	scrolltool(&sargs);

	ps->mangled_screen = 1;
	so_give(&srcstore);
	free_handles(&handles);
    }
    else
      q_status_message(SM_ORDER, 0, 2, _("Error allocating space"));
}


/*
 * Compose a message to the email addresses contained in an LDAP entry.
 *
 * Args    ps -- Pine struct
 *  winning_e -- The struct containing the information about the entry
 *               to be viewed.
 */
void
compose_to_ldap_entry(struct pine *ps, LDAP_CHOOSE_S *e, int allow_role)
{
    char  **elecmail = NULL,
	  **mail = NULL,
	  **cn = NULL,
	  **sn = NULL,
	  **givenname = NULL,
	  **ll;
    size_t  len = 0;

    dprint((9, "- compose_to_ldap_entry -\n"));

    if(e){
	char       *a;
	BerElement *ber;

	for(a = ldap_first_attribute(e->ld, e->selected_entry, &ber);
	    a != NULL;
	    a = ldap_next_attribute(e->ld, e->selected_entry, ber)){

	    if(strcmp(a, e->info_used->mailattr) == 0){
		if(!mail)
		  mail = ldap_get_values(e->ld, e->selected_entry, a);
	    }
	    else if(strcmp(a, "electronicmail") == 0){
		if(!elecmail)
		  elecmail = ldap_get_values(e->ld, e->selected_entry, a);
	    }
	    else if(strcmp(a, e->info_used->cnattr) == 0){
		if(!cn)
		  cn = ldap_get_values(e->ld, e->selected_entry, a);
	    }
	    else if(strcmp(a, e->info_used->gnattr) == 0){
		if(!givenname)
		  givenname = ldap_get_values(e->ld, e->selected_entry, a);
	    }
	    else if(strcmp(a, e->info_used->snattr) == 0){
		if(!sn)
		  sn = ldap_get_values(e->ld, e->selected_entry, a);
	    }

	    our_ldap_memfree(a);
	}
    }

    if(elecmail){
	if(elecmail[0] && elecmail[0][0] && !mail)
	  mail = elecmail;
	else
	  ldap_value_free(elecmail);

	elecmail = NULL;
    }

    for(ll = mail; ll && *ll; ll++)
      len += strlen(*ll) + 1;
    
    if(len){
	char        *p, *address, *fn = NULL;
	BuildTo      bldto;
	SAVE_STATE_S state;

	address = (char *)fs_get(len * sizeof(char));
	p = address;
	ll = mail;
	while(*ll){
	    sstrncpy(&p, *ll, len-(p-address));
	    ll++;
	    if(*ll)
	      sstrncpy(&p, ",", len-(p-address));
	}

	address[len-1] = '\0';

	/*
	 * If we have a fullname and there is only a single address and
	 * the address doesn't seem to have a fullname with it, add it.
	 */
	if(mail && mail[0] && mail[0][0] && !mail[1]){
	    if(cn && cn[0] && cn[0][0])
	      fn = cpystr(cn[0]);
	    else if(sn && sn[0] && sn[0][0] &&
	            givenname && givenname[0] && givenname[0][0]){
		size_t l;

		l = strlen(givenname[0]) + strlen(sn[0]) + 1;
		fn = (char *) fs_get((l+1) * sizeof(char));
		snprintf(fn, l+1, "%s %s", givenname[0], sn[0]);
		fn[l] = '\0';
	    }
	}

	if(mail && mail[0] && mail[0][0] && !mail[1] && fn){
	    ADDRESS *adrlist = NULL;
	    char    *tmp_a_string;

	    tmp_a_string = cpystr(address);
	    rfc822_parse_adrlist(&adrlist, tmp_a_string, fakedomain);
	    fs_give((void **)&tmp_a_string);
	    if(adrlist && !adrlist->next && !adrlist->personal){
		char *new_address;
		size_t len;
		RFC822BUFFER rbuf;

		adrlist->personal = cpystr(fn);
		len = est_size(adrlist);
		new_address = (char *) fs_get(len * sizeof(char));
		new_address[0] ='\0';
		rbuf.f   = dummy_soutr;
		rbuf.s   = NULL;
		rbuf.beg = new_address;
		rbuf.cur = new_address;
		rbuf.end = new_address+len-1;
		/* this will quote it if it needs quoting */
		rfc822_output_address_list(&rbuf, adrlist, 0L, NULL);
		*rbuf.cur = '\0';
		fs_give((void **)&address);
		address = new_address;
	    }

	    if(adrlist)
	      mail_free_address(&adrlist);
	}

	bldto.type    = Str;
	bldto.arg.str = address;

	save_state(&state);
	ab_compose_internal(bldto, allow_role);
	restore_state(&state);
	fs_give((void **)&address);
	if(fn)
	  fs_give((void **)&fn);

	/*
	 * Window size may have changed in composer.
         * Pine_send will have reset the window size correctly,
         * but we still have to reset our address book data structures.
	 */
	if(as.initialized)
	  ab_resize();

	ps_global->mangled_screen = 1;
    }
    else
      q_status_message(SM_ORDER, 0, 4, _("No address to compose to"));

    if(mail)
      ldap_value_free(mail);
    if(cn)
      ldap_value_free(cn);
    if(sn)
      ldap_value_free(sn);
    if(givenname)
      ldap_value_free(givenname);
}


/*
 * Forward the text of an LDAP entry via email (not a vcard attachment)
 *
 * Args    ps -- Pine struct
 *  winning_e -- The struct containing the information about the entry
 *               to be viewed.
 */
void
forward_ldap_entry(struct pine *ps, LDAP_CHOOSE_S *winning_e)
{
    STORE_S    *srcstore = NULL;
    SourceType  srctype = CharStar;

    dprint((9, "- forward_ldap_entry -\n"));

    if((srcstore = prep_ldap_for_viewing(ps,winning_e,srctype,NULL)) != NULL){
	forward_text(ps, so_text(srcstore), srctype);
	ps->mangled_screen = 1;
	so_give(&srcstore);
    }
    else
      q_status_message(SM_ORDER, 0, 2, _("Error allocating space"));
}


STORE_S *
prep_ldap_for_viewing(struct pine *ps, LDAP_CHOOSE_S *winning_e, SourceType srctype, HANDLE_S **handlesp)
{
    STORE_S    *store = NULL;
    char       *a, *tmp;
    BerElement *ber;
    int         i, width;
#define W (1000)
#define INDENTHERE (22)
    char        obuf[W+10];
    char        hdr[6*INDENTHERE+1], hdr2[6*INDENTHERE+1];
    char      **cn = NULL;
    int         indent = INDENTHERE;

    if(!(store = so_get(srctype, NULL, EDIT_ACCESS)))
      return(store);

    /* for mailto handles so user can select individual email addrs */
    if(handlesp)
      init_handles(handlesp);

    width = MAX(ps->ttyo->screen_cols, 25);

    snprintf(hdr2, sizeof(hdr2), "%-*.*s: ", indent-2,indent-2, "");
    hdr2[sizeof(hdr2)-1] = '\0';

    if(sizeof(obuf) > ps->ttyo->screen_cols+1){
	memset((void *)obuf, '-', ps->ttyo->screen_cols * sizeof(char));
	obuf[ps->ttyo->screen_cols] = '\n';
	obuf[ps->ttyo->screen_cols+1] = '\0';
    }

    a = ldap_get_dn(winning_e->ld, winning_e->selected_entry);

    so_puts(store, obuf);
    if((tmp = fold(a, width, width, "", "   ", FLD_NONE)) != NULL){
	so_puts(store, tmp);
	fs_give((void **)&tmp);
    }

    so_puts(store, obuf);
    so_puts(store, "\n");

    our_ldap_dn_memfree(a);

    for(a = ldap_first_attribute(winning_e->ld, winning_e->selected_entry, &ber);
	a != NULL;
	a = ldap_next_attribute(winning_e->ld, winning_e->selected_entry, ber)){

	if(a && *a){
	    char **vals;
	    char  *fn = NULL;

	    vals = ldap_get_values(winning_e->ld, winning_e->selected_entry, a);

	    /* save this for mailto */
	    if(handlesp && !cn && !strcmp(a, winning_e->info_used->cnattr))
	      cn = ldap_get_values(winning_e->ld, winning_e->selected_entry, a);

	    if(vals){
		int do_mailto;

		do_mailto = (handlesp &&
			     !strcmp(a, winning_e->info_used->mailattr));

		utf8_snprintf(hdr, sizeof(hdr), "%-*.*w: ", indent-2,indent-2,
			ldap_translate(a, winning_e->info_used));
		hdr[sizeof(hdr)-1] = '\0';
		for(i = 0; vals[i] != NULL; i++){
		    if(do_mailto){
			ADDRESS  *ad = NULL;
			HANDLE_S *h;
			char      buf[20];
			char     *tmp_a_string;
			char     *addr, *new_addr, *enc_addr;
			char     *path = NULL;

			addr = cpystr(vals[i]);
			if(cn && cn[0] && cn[0][0])
			  fn = cpystr(cn[0]);

			if(fn){
			    tmp_a_string = cpystr(addr);
			    rfc822_parse_adrlist(&ad, tmp_a_string, "@");
			    fs_give((void **)&tmp_a_string);
			    if(ad && !ad->next && !ad->personal){
				RFC822BUFFER rbuf;
				size_t len;

				ad->personal = cpystr(fn);
				len = est_size(ad);
				new_addr = (char *) fs_get(len * sizeof(char));
				new_addr[0] = '\0';
				/* this will quote it if it needs quoting */
				rbuf.f   = dummy_soutr;
				rbuf.s   = NULL;
				rbuf.beg = new_addr;
				rbuf.cur = new_addr;
				rbuf.end = new_addr+len-1;
				rfc822_output_address_list(&rbuf, ad, 0L, NULL);
				*rbuf.cur = '\0';
				fs_give((void **) &addr);
				addr = new_addr;
			    }

			    if(ad)
			      mail_free_address(&ad);

			    fs_give((void **)&fn);
			}

			if((enc_addr = rfc1738_encode_mailto(addr)) != NULL){
			    size_t l;

			    l = strlen(enc_addr) + 7;
			    path = (char *) fs_get((l+1) * sizeof(char));
			    snprintf(path, l+1, "mailto:%s", enc_addr);
			    path[l] = '\0';
			    fs_give((void **)&enc_addr);
			}
			
			fs_give((void **)&addr);

			if(path){
			    h = new_handle(handlesp);
			    h->type = URL;
			    h->h.url.path = path;
			    snprintf(buf, sizeof(buf), "%d", h->key);
			    buf[sizeof(buf)-1] = '\0';

			    /*
			     * Don't try to fold this address. Just put it on
			     * one line and let scrolltool worry about
			     * cutting it off before it goes past the
			     * right hand edge. Otherwise, we have to figure
			     * out how to handle the wrapped handle.
			     */
			    snprintf(obuf, sizeof(obuf), "%c%c%c%s%s%c%c%c%c",
				    TAG_EMBED, TAG_HANDLE,
				    strlen(buf), buf, vals[i],
				    TAG_EMBED, TAG_BOLDOFF,
				    TAG_EMBED, TAG_INVOFF);
			    obuf[sizeof(obuf)-1] = '\0';

			    so_puts(store, (i==0) ? hdr : hdr2);
			    so_puts(store, obuf);
			    so_puts(store, "\n");
			}
			else{
			    snprintf(obuf, sizeof(obuf), "%s", vals[i]);
			    obuf[sizeof(obuf)-1] = '\0';

			    if((tmp = fold(obuf, width, width,
					  (i==0) ? hdr : hdr2,
					  repeat_char(indent+2, SPACE),
					  FLD_NONE)) != NULL){
				so_puts(store, tmp);
				fs_give((void **)&tmp);
			    }
			}
		    }
		    else{
			snprintf(obuf, sizeof(obuf), "%s", vals[i]);
			obuf[sizeof(obuf)-1] = '\0';

			if((tmp = fold(obuf, width, width,
				      (i==0) ? hdr : hdr2,
				      repeat_char(indent+2, SPACE),
				      FLD_NONE)) != NULL){
			    so_puts(store, tmp);
			    fs_give((void **)&tmp);
			}
		    }
		}
		
		ldap_value_free(vals);
	    }
	    else{
		utf8_snprintf(obuf, sizeof(obuf), "%-*.*w\n", indent-1,indent-1,
			ldap_translate(a, winning_e->info_used));
		obuf[sizeof(obuf)-1] = '\0';
		so_puts(store, obuf);
	    }
	}

	our_ldap_memfree(a);
    }

    if(cn)
      ldap_value_free(cn);

    return(store);
}


int
process_ldap_cmd(int cmd, MSGNO_S *msgmap, SCROLL_S *sparms)
{
    int rv = 1;

    ps_global->next_screen = SCREEN_FUN_NULL;

    switch(cmd){
      case MC_SAVE :
	save_ldap_entry(ps_global, sparms->proc.data.p, 0);
	rv = 0;
	break;

      case MC_COMPOSE :
	compose_to_ldap_entry(ps_global, sparms->proc.data.p, 0);
	rv = 0;
	break;

      case MC_ROLE :
	compose_to_ldap_entry(ps_global, sparms->proc.data.p, 1);
	rv = 0;
	break;

      default:
	panic("Unexpected command in process_ldap_cmd");
	break;
    }

    return(rv);
}


int
pico_simpleexit(struct headerentry *he, void (*redraw_pico)(void), int allow_flowed,
		char **result)
{
    if(result)
      *result = NULL;

    return(0);
}

char *
pico_simplecancel(void (*redraw_pico)(void))
{
    return("Cancelled");
}


/*
 * Store query parameters so that they can be recalled by user later with ^R.
 */
void
save_query_parameters(SAVED_QUERY_S *params)
{
    free_saved_query_parameters();
    saved_params = params;
}


SAVED_QUERY_S *
copy_query_parameters(SAVED_QUERY_S *params)
{
    SAVED_QUERY_S *sq;

    sq = (SAVED_QUERY_S *)fs_get(sizeof(SAVED_QUERY_S));
    memset((void *)sq, 0, sizeof(SAVED_QUERY_S));

    if(params && params->qq)
      sq->qq = cpystr(params->qq);
    else
      sq->qq = cpystr("");

    if(params && params->cn)
      sq->cn = cpystr(params->cn);
    else
      sq->cn = cpystr("");

    if(params && params->sn)
      sq->sn = cpystr(params->sn);
    else
      sq->sn = cpystr("");

    if(params && params->gn)
      sq->gn = cpystr(params->gn);
    else
      sq->gn = cpystr("");

    if(params && params->mail)
      sq->mail = cpystr(params->mail);
    else
      sq->mail = cpystr("");

    if(params && params->org)
      sq->org = cpystr(params->org);
    else
      sq->org = cpystr("");

    if(params && params->unit)
      sq->unit = cpystr(params->unit);
    else
      sq->unit = cpystr("");

    if(params && params->country)
      sq->country = cpystr(params->country);
    else
      sq->country = cpystr("");

    if(params && params->state)
      sq->state = cpystr(params->state);
    else
      sq->state = cpystr("");

    if(params && params->locality)
      sq->locality = cpystr(params->locality);
    else
      sq->locality = cpystr("");
    
    if(params && params->custom)
      sq->custom = cpystr(params->custom);
    else
      sq->custom = cpystr("");
    
    return(sq);
}


void
free_saved_query_parameters(void)
{
    if(saved_params)
      free_query_parameters(&saved_params);
}


void
free_query_parameters(SAVED_QUERY_S **parm)
{
    if(parm){
	if(*parm){
	    if((*parm)->qq)
	      fs_give((void **)&(*parm)->qq);
	    if((*parm)->cn)
	      fs_give((void **)&(*parm)->cn);
	    if((*parm)->sn)
	      fs_give((void **)&(*parm)->sn);
	    if((*parm)->gn)
	      fs_give((void **)&(*parm)->gn);
	    if((*parm)->mail)
	      fs_give((void **)&(*parm)->mail);
	    if((*parm)->org)
	      fs_give((void **)&(*parm)->org);
	    if((*parm)->unit)
	      fs_give((void **)&(*parm)->unit);
	    if((*parm)->country)
	      fs_give((void **)&(*parm)->country);
	    if((*parm)->state)
	      fs_give((void **)&(*parm)->state);
	    if((*parm)->locality)
	      fs_give((void **)&(*parm)->locality);
	    if((*parm)->custom)
	      fs_give((void **)&(*parm)->custom);
	    
	    fs_give((void **)parm);
	}
    }
}


/*
 * A callback from pico to restore the saved query parameters.
 *
 * Args  he -- Unused.
 *        s -- The place to return the allocated array of values.
 *
 * Returns -- 1 if there are parameters to return, 0 otherwise.
 */
int
restore_query_parameters(struct headerentry *he, char ***s)
{
    int retval = 0, i = 0;

    if(s)
      *s = NULL;

    if(saved_params && s){
	*s = (char **)fs_get((QQ_END + 1) * sizeof(char *));
	(*s)[i++] = cpystr(saved_params->qq ? saved_params->qq : "");
	(*s)[i++] = cpystr(saved_params->cn ? saved_params->cn : "");
	(*s)[i++] = cpystr(saved_params->sn ? saved_params->sn : "");
	(*s)[i++] = cpystr(saved_params->gn ? saved_params->gn : "");
	(*s)[i++] = cpystr(saved_params->mail ? saved_params->mail : "");
	(*s)[i++] = cpystr(saved_params->org ? saved_params->org : "");
	(*s)[i++] = cpystr(saved_params->unit ? saved_params->unit : "");
	(*s)[i++] = cpystr(saved_params->country ? saved_params->country : "");
	(*s)[i++] = cpystr(saved_params->state ? saved_params->state : "");
	(*s)[i++] = cpystr(saved_params->locality ? saved_params->locality :"");
	(*s)[i++] = cpystr(saved_params->custom ? saved_params->custom :"");
	(*s)[i]   = 0;
	retval = 1;
    }

    return(retval);
}


/*
 * Internal handler for viewing an LDAP url.
 */
int
url_local_ldap(char *url)
{
    LDAP            *ld;
    struct timeval   t;
    int              ld_err, mangled = 0, we_cancel, retval = 0, proto = 3;
    int              we_turned_on = 0;
    char             ebuf[300];
    LDAPMessage     *result;
    LDAP_SERV_S     *info;
    LDAP_SERV_RES_S *serv_res = NULL;
    LDAPURLDesc     *ldapurl = NULL;
    WP_ERR_S wp_err;

    dprint((2, "url_local_ldap(%s)\n", url ? url : "?"));

    ld_err = ldap_url_parse(url, &ldapurl);
    if(ld_err || !ldapurl){
      snprintf(ebuf, sizeof(ebuf), "URL parse failed for %s", url);
      ebuf[sizeof(ebuf)-1] = '\0';
      q_status_message(SM_ORDER, 3, 5, ebuf);
      return(retval);
    }

    if(!ldapurl->lud_host){
      /* TRNASLATORS: No host in <url> */
      snprintf(ebuf, sizeof(ebuf), _("No host in %s"), url);
      ebuf[sizeof(ebuf)-1] = '\0';
      q_status_message(SM_ORDER, 3, 5, ebuf);
      ldap_free_urldesc(ldapurl);
      return(retval);
    }
    
    we_turned_on = intr_handling_on();
    we_cancel = busy_cue(_("Searching for LDAP url"), NULL, 0);
    ps_global->mangled_footer = 1;

#if (LDAPAPI >= 11)
    if((ld = ldap_init(ldapurl->lud_host, ldapurl->lud_port)) == NULL)
#else
    if((ld = ldap_open(ldapurl->lud_host, ldapurl->lud_port)) == NULL)
#endif
    {
	if(we_cancel){
	    cancel_busy_cue(-1);
	    we_cancel = 0;
	}

	q_status_message(SM_ORDER,3,5, _("LDAP search failed: can't initialize"));
    }
    else if(!ps_global->intr_pending){
      if(ldap_v3_is_supported(ld) &&
	 our_ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &proto) == 0){
	dprint((5, "ldap: using version 3 protocol\n"));
      }

      /*
       * If we don't set RESTART then the select() waiting for the answer
       * in libldap will be interrupted and stopped by our busy_cue.
       */
      our_ldap_set_option(ld, LDAP_OPT_RESTART, LDAP_OPT_ON);

      t.tv_sec = 30; t.tv_usec = 0;
      ld_err = ldap_search_st(ld, ldapurl->lud_dn, ldapurl->lud_scope,
			      ldapurl->lud_filter, ldapurl->lud_attrs,
			      0, &t, &result);
      if(ld_err != LDAP_SUCCESS){
	  if(we_cancel){
	      cancel_busy_cue(-1);
	      we_cancel = 0;
	  }

	  snprintf(ebuf, sizeof(ebuf), _("LDAP search failed: %s"), ldap_err2string(ld_err));
	  ebuf[sizeof(ebuf)-1] = '\0';
	  q_status_message(SM_ORDER, 3, 5, ebuf);
	  ldap_unbind(ld);
      }
      else if(!ps_global->intr_pending){
	if(we_cancel){
	    cancel_busy_cue(-1);
	    we_cancel = 0;
	}

	if(we_turned_on){
	    intr_handling_off();
	    we_turned_on = 0;
	}

	if(ldap_count_entries(ld, result) == 0){
	  q_status_message(SM_ORDER, 3, 5, _("No matches found for url"));
	  ldap_unbind(ld);
	  if(result)
	    ldap_msgfree(result);
	}
        else{
	  serv_res = (LDAP_SERV_RES_S *)fs_get(sizeof(LDAP_SERV_RES_S));
	  memset((void *)serv_res, 0, sizeof(*serv_res));
	  serv_res->ld  = ld;
	  serv_res->res = result;
	  info = (LDAP_SERV_S *)fs_get(sizeof(LDAP_SERV_S));
	  memset((void *)info, 0, sizeof(*info));
	  info->mailattr = cpystr(DEF_LDAP_MAILATTR);
	  info->snattr   = cpystr(DEF_LDAP_SNATTR);
	  info->gnattr   = cpystr(DEF_LDAP_GNATTR);
	  info->cnattr   = cpystr(DEF_LDAP_CNATTR);
	  serv_res->info_used = info;
	  memset(&wp_err, 0, sizeof(wp_err));
	  wp_err.mangled = &mangled;

	  ask_user_which_entry(serv_res, NULL, NULL, &wp_err, DisplayForURL);
	  if(wp_err.error){
	    q_status_message(SM_ORDER, 3, 5, wp_err.error);
	    fs_give((void **)&wp_err.error);
	  }

	  if(mangled)
	    ps_global->mangled_screen = 1;

	  free_ldap_result_list(&serv_res);
	  retval = 1;
        }
      }
    }

    if(we_cancel)
      cancel_busy_cue(-1);

    if(we_turned_on)
      intr_handling_off();

    if(ldapurl)
      ldap_free_urldesc(ldapurl);

    return(retval);
}
#endif	/* ENABLE_LDAP */
