#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: takeaddr.c 1012 2008-03-26 00:44:22Z hubert@u.washington.edu $";
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
    takeaddr.c
    Mostly support for Take Address command.
 ====*/


#include "headers.h"
#include "takeaddr.h"
#include "addrbook.h"
#include "adrbkcmd.h"
#include "status.h"
#include "confscroll.h"
#include "keymenu.h"
#include "radio.h"
#include "titlebar.h"
#include "alpine.h"
#include "help.h"
#include "mailcmd.h"
#include "mailpart.h"
#include "roleconf.h"
#include "../pith/state.h"
#include "../pith/msgno.h"
#include "../pith/adrbklib.h"
#include "../pith/bldaddr.h"
#include "../pith/bitmap.h"
#include "../pith/util.h"
#include "../pith/addrstring.h"
#include "../pith/remote.h"
#include "../pith/newmail.h"
#include "../pith/list.h"
#include "../pith/abdlc.h"
#include "../pith/ablookup.h"
#include "../pith/stream.h"
#include "../pith/mailcmd.h"
#include "../pith/busy.h"


typedef struct takeaddress_screen {
    ScreenMode mode;
    TA_S      *current,
              *top_line;
} TA_SCREEN_S;

static TA_SCREEN_S *ta_screen;
static char        *fakedomain = "@";

static ESCKEY_S save_or_export[] = {
    {'s', 's', "S", N_("Save")},
    {'e', 'e', "E", N_("Export")},
    {-1, 0, NULL, NULL}};


/* internal prototypes */
int            edit_nickname(AdrBk *, AddrScrn_Disp *, int, char *, char *, HelpType, int, int);
void           add_abook_entry(TA_S *, char *, char *, char *, char *, int, TA_STATE_S **, char *);
void           take_to_addrbooks_frontend(char **, char *, char *, char *, char *,
					  char *, int, TA_STATE_S **, char *);
void           take_to_addrbooks(char **, char *, char *, char *, char *,
				 char *, int, TA_STATE_S **, char *);
PerAddrBook   *use_this_addrbook(int, char *);
PerAddrBook   *check_for_addrbook(char *);
int            takeaddr_screen(struct pine *, TA_S *, int, ScreenMode, TA_STATE_S **, char *);
void           takeaddr_bypass(struct pine *, TA_S *, TA_STATE_S **);
int	       ta_do_take(TA_S *, int, int, TA_STATE_S **, char *);
TA_S          *whereis_taline(TA_S *);
int            ta_take_marked_addrs(int, TA_S *, int, TA_STATE_S **, char *);
int            ta_take_single_addr(TA_S *, int, TA_STATE_S **, char *);
int            update_takeaddr_screen(struct pine *, TA_S *, TA_SCREEN_S *, Pos *);
void           takeaddr_screen_redrawer_list(void);
void           takeaddr_screen_redrawer_single(void);
int	       attached_addr_handler(TA_S *, int);
int            take_without_edit(TA_S *, int, int, TA_STATE_S **, char *);
void           export_vcard_att(struct pine *, int, long, ATTACH_S *);
int            take_export_tool(struct pine *, int, CONF_S **, unsigned);
				
#ifdef  _WINDOWS
int            ta_scroll_up(long);
int            ta_scroll_down(long);
int            ta_scroll_to_pos(long);
int            ta_scroll_callback(int, long);
#endif


/*
 * Edit a nickname field.
 *
 * Args: abook     -- the addressbook handle
 *       dl        -- display list line (NULL if new entry)
 *    command_line -- line to prompt on
 *       orig      -- nickname to edit
 *       prompt    -- prompt
 *      this_help  -- help
 * return_existing -- changes the behavior when a user types in a nickname
 *                    which already exists in this abook.  If not set, it
 *                    will just keep looping until the user changes; if set,
 *                    it will return -8 to the caller and orig will be set
 *                    to the matching nickname.
 *
 * Returns: -10 to cancel
 *          -9  no change
 *          -7  only case of nickname changed (only happens if dl set)
 *          -8  existing nickname chosen (only happens if return_existing set)
 *           0  new value copied into orig
 */
int
edit_nickname(AdrBk *abook, AddrScrn_Disp *dl, int command_line, char *orig,
	      char *prompt, HelpType this_help, int return_existing, int takeaddr)
{
    char         edit_buf[MAX_NICKNAME + 1];
    HelpType     help;
    int          i, flags, lastrc, rc;
    AdrBk_Entry *check, *passed_in_ae;
    ESCKEY_S     ekey[3];
    SAVE_STATE_S state;  /* For saving state of addrbooks temporarily */
    char        *error = NULL;

    ekey[i = 0].ch    = ctrl('T');
    ekey[i].rval  = 2;
    ekey[i].name  = "^T";
    ekey[i++].label = N_("To AddrBk");

    if(F_ON(F_ENABLE_TAB_COMPLETE,ps_global)){
	ekey[i].ch      =  ctrl('I');
	ekey[i].rval    = 11;
	ekey[i].name    = "TAB";
	ekey[i++].label = N_("Complete");
    }

    ekey[i].ch    = -1;

    strncpy(edit_buf, orig, sizeof(edit_buf)-1);
    edit_buf[sizeof(edit_buf)-1] = '\0';
    if(dl)
      passed_in_ae = adrbk_get_ae(abook, (a_c_arg_t) dl->elnum);
    else
      passed_in_ae = (AdrBk_Entry *)NULL;

    help  = NO_HELP;
    rc    = 0;
    check = NULL;
    do{
	if(error){
	    q_status_message(SM_ORDER, 3, 4, error);
	    fs_give((void **)&error);
	}

	/* display a message because adrbk_lookup_by_nick returned positive */
	if(check){
	    if(return_existing){
		strncpy(orig, edit_buf, sizeof(edit_buf)-1);
		orig[sizeof(edit_buf)-1] = '\0';
		if(passed_in_ae)
		  (void)adrbk_get_ae(abook, (a_c_arg_t) dl->elnum);
		return -8;
	    }

            q_status_message1(SM_ORDER, 0, 4,
		    _("Already an entry with nickname \"%s\""), edit_buf);
	}

	lastrc = rc;
	if(rc == 3)
          help = (help == NO_HELP ? this_help : NO_HELP);

	flags = OE_APPEND_CURRENT;
	rc = optionally_enter(edit_buf, command_line, 0, sizeof(edit_buf),
			      prompt, ekey, help, &flags);

	if(rc == 1)  /* ^C */
	  break;

	if(rc == 2){ /* ^T */
	    void (*redraw) (void) = ps_global->redrawer;
	    char *returned_nickname;

	    push_titlebar_state();
	    save_state(&state);
	    if(takeaddr)
	      returned_nickname = addr_book_takeaddr();
	    else
	      returned_nickname = addr_book_selnick();

	    restore_state(&state);
	    if(returned_nickname){
		strncpy(edit_buf, returned_nickname, sizeof(edit_buf)-1);
		edit_buf[sizeof(edit_buf)-1] = '\0';
		fs_give((void **)&returned_nickname);
	    }

	    ClearScreen();
	    pop_titlebar_state();
	    redraw_titlebar();
	    if((ps_global->redrawer = redraw) != NULL) /* reset old value, and test */
	      (*ps_global->redrawer)();
	}
	else if(rc == 11){ /* TAB */
	    if(edit_buf[0]){
	      char *new_nickname = NULL;
	      int ambiguity;

	      ambiguity = abook_nickname_complete(edit_buf, &new_nickname,
				  (lastrc==rc && !(flags & OE_USER_MODIFIED)), 0);
	      if(new_nickname){
		if(*new_nickname){
		  strncpy(edit_buf, new_nickname, sizeof(edit_buf));
		  edit_buf[sizeof(edit_buf)-1] = '\0';
	        }

		fs_give((void **) &new_nickname);
	      }

	      if(ambiguity != 2)
		Writechar(BELL, 0);
	    }
	}
            
    }while(rc == 2 ||
	   rc == 3 ||
	   rc == 4 ||
	   rc == 11||
	   nickname_check(edit_buf, &error) ||
           ((check =
	       adrbk_lookup_by_nick(abook, edit_buf, (adrbk_cntr_t *)NULL)) &&
	     check != passed_in_ae));

    if(rc != 0){
	if(passed_in_ae)
	  (void)adrbk_get_ae(abook, (a_c_arg_t) dl->elnum);

	return -10;
    }

    /* only the case of nickname changed */
    if(passed_in_ae && check == passed_in_ae && strcmp(edit_buf, orig)){
	(void)adrbk_get_ae(abook, (a_c_arg_t) dl->elnum);
	strncpy(orig, edit_buf, sizeof(edit_buf)-1);
	orig[sizeof(edit_buf)-1] = '\0';
	return -7;
    }

    if(passed_in_ae)
      (void)adrbk_get_ae(abook, (a_c_arg_t) dl->elnum);

    if(strcmp(edit_buf, orig) == 0) /* no change */
      return -9;
    
    strncpy(orig, edit_buf, sizeof(edit_buf)-1);
    orig[sizeof(edit_buf)-1] = '\0';
    return 0;
}


/*
 * Add an entry to address book.
 * It is for capturing addresses off incoming mail.
 * This is a front end for take_to_addrbooks.
 * It is also used for replacing an existing entry and for adding a single
 * new address to an existing list.
 *
 * The reason this is here is so that when Taking a single address, we can
 * rearrange the fullname to be Last, First instead of First Last.
 *
 * Args: ta_entry -- the entry from the take screen
 *   command_line -- line to prompt on
 *
 * Result: item is added to one of the address books,
 *       an error message is queued if appropriate.
 */
void
add_abook_entry(TA_S *ta_entry, char *nick, char *fullname, char *fcc,
		char *comment, int command_line, TA_STATE_S **tas, char *cmd)
{
    ADDRESS *addr;
    char     new_fullname[6*MAX_FULLNAME + 1], new_address[6*MAX_ADDRESS + 1];
    char   **new_list;

    dprint((5, "-- add_abook_entry --\n"));

    /*-- rearrange full name (Last, First) ---*/
    new_fullname[0] = '\0';
    addr = ta_entry->addr;
    if(!fullname && addr->personal != NULL){
	if(F_ON(F_DISABLE_TAKE_LASTFIRST, ps_global)){
	    strncpy(new_fullname, addr->personal, sizeof(new_fullname)-1);
	    new_fullname[sizeof(new_fullname)-1] = '\0';
	}
	else{
	    char old_fullname[6*MAX_FULLNAME + 1];

	    snprintf(old_fullname, sizeof(old_fullname), "%s", addr->personal);
	    old_fullname[sizeof(old_fullname)-1] = '\0';
	    switch_to_last_comma_first(old_fullname, new_fullname, sizeof(new_fullname));
	}
    }

    /* initial value for new address */
    new_address[0] = '\0';
    if(addr->mailbox && addr->mailbox[0]){
	char *scratch, *p, *t, *u;
	size_t es;
	unsigned long l;
	RFC822BUFFER rbuf;

	es = est_size(addr);
	scratch = (char *) fs_get(es);
	scratch[0] = '\0';
	rbuf.f   = dummy_soutr;
	rbuf.s   = NULL;
	rbuf.beg = scratch;
	rbuf.cur = scratch;
	rbuf.end = scratch+es-1;
	rfc822_output_address_list(&rbuf, addr, 0L, NULL);
	*rbuf.cur = '\0';
	if((p = srchstr(scratch, "@" RAWFIELD)) != NULL){
	  for(t = p; ; t--)
	    if(*t == '&'){		/* find "leading" token */
		*t++ = ' ';		/* replace token */
		*p = '\0';		/* tie off string */
		u = (char *)rfc822_base64((unsigned char *)t,
					  (unsigned long)strlen(t), &l);
		*p = '@';		/* restore 'p' */
		rplstr(p, es-(p-scratch), 12, "");	/* clear special token */
		rplstr(t, es-(t-scratch), strlen(t), u);  /* Null u is handled */
		if(u)
		  fs_give((void **)&u);
	    }
	    else if(t == scratch)
	      break;
	}

	strncpy(new_address, scratch, sizeof(new_address)-1);
	new_address[sizeof(new_address)-1] = '\0';

	if(scratch)
	  fs_give((void **)&scratch);
    }

    if(ta_entry->frwrded){
	ADDRESS *a;
	int i, j;

	for(i = 0, a = addr; a; i++, a = a->next)
	  ;/* just counting for alloc below */

	/* catch special case where empty addr was set in vcard_to_ta */
	if(i == 1 && !addr->host && !addr->mailbox && !addr->personal)
	  i = 0;

	new_list = (char **) fs_get((i+1) * sizeof(char *));
	for(j = 0, a = addr; i && a; j++, a = a->next){
	    ADDRESS *next_addr;
	    char    *bufp;
	    size_t   len;

	    next_addr = a->next;
	    a->next = NULL;
	    len = est_size(a);
	    bufp = (char *) fs_get(len * sizeof(char));
	    new_list[j] = cpystr(addr_string(a, bufp, len));
	    a->next = next_addr;
	    fs_give((void **) &bufp);
	}

	new_list[j] = NULL;
    }
    else{
	int i = 0, j;

	j = (ta_entry->strvalue && ta_entry->strvalue[0]) ? 2 : 1;

	new_list = (char **) fs_get(j * sizeof(char *));

	if(j == 2)
	  new_list[i++] = cpystr(ta_entry->strvalue);

	new_list[i] = NULL;
    }

    take_to_addrbooks_frontend(new_list, nick,
			       fullname ? fullname : new_fullname,
			       new_address, fcc, comment, command_line,
			       tas, cmd);
    free_list_array(&new_list);
}


void
take_to_addrbooks_frontend(char **new_entries, char *nick, char *fullname,
			   char *addr, char *fcc, char *comment, int cmdline,
			   TA_STATE_S **tas, char *cmd)
{
    jmp_buf save_jmp_buf;
    int    *save_nesting_level;

    dprint((5, "-- take_to_addrbooks_frontend --\n"));

    if(ps_global->remote_abook_validity > 0 &&
       adrbk_check_and_fix_all(ab_nesting_level == 0, 0, 0))
      ps_global->mangled_footer = 1;

    save_nesting_level = cpyint(ab_nesting_level);
    memcpy(save_jmp_buf, addrbook_changed_unexpectedly, sizeof(jmp_buf));
    if(setjmp(addrbook_changed_unexpectedly)){
	q_status_message(SM_ORDER, 5, 10, _("Resetting address book..."));
	dprint((1,
	    "RESETTING address book... take_to_addrbooks_frontend!\n"));
	addrbook_reset();
	ab_nesting_level = *save_nesting_level;
    }

    ab_nesting_level++;
    take_to_addrbooks(new_entries, nick, fullname, addr, fcc, comment, cmdline,
		      tas, cmd);
    memcpy(addrbook_changed_unexpectedly, save_jmp_buf, sizeof(jmp_buf));
    ab_nesting_level--;
    if(save_nesting_level)
      fs_give((void **)&save_nesting_level);
}


/*
 * Add to address book, called from take screen.
 * It is also used for adding to an existing list or replacing an existing
 * entry.
 *
 * Args: new_entries -- a list of addresses to add to a list or to form
 *                      a new list with
 *              nick -- if adding new entry, suggest this for nickname
 *          fullname -- if adding new entry, use this for fullname
 *              addr -- if only one new_entry, this is its addr
 *               fcc -- if adding new entry, use this for fcc
 *           comment -- if adding new entry, use this for comment
 *      command_line -- line to prompt on
 *
 * Result: item is added to one of the address books,
 *       an error message is queued if appropriate.
 */
void
take_to_addrbooks(char **new_entries, char *nick, char *fullname, char *addr,
		  char *fcc, char *comment, int command_line, TA_STATE_S **tas, char *cmd)
{
    char          new_nickname[6*MAX_NICKNAME + 1], exist_nick[6*MAX_NICKNAME + 1];
    char          prompt[200], **p;
    int           rc, listadd = 0, ans, i;
    AdrBk        *abook;
    SAVE_STATE_S  state;
    PerAddrBook  *pab;
    AdrBk_Entry  *abe = (AdrBk_Entry *)NULL, *abe_copy;
    adrbk_cntr_t  entry_num = NO_NEXT;
    size_t        tot_size, new_size, old_size;
    Tag           old_tag;
    char         *tmp_a_string;
    char         *simple_a = NULL;
    ADDRESS      *a = NULL;


    dprint((5, "-- take_to_addrbooks --\n"));

    if(tas && *tas)
      pab = (*tas)->pab;
    else
      pab = setup_for_addrbook_add(&state, command_line, cmd);

    /* check we got it opened ok */
    if(pab == NULL || pab->address_book == NULL)
      goto take_to_addrbooks_cancel;

    adrbk_check_validity(pab->address_book, 1L);
    if(pab->address_book->flags & FILE_OUTOFDATE ||
       (pab->address_book->rd &&
	pab->address_book->rd->flags & REM_OUTOFDATE)){
	q_status_message3(SM_ORDER, 0, 4,
      "Address book%s%s has changed: %stry again",
      (as.n_addrbk > 1 && pab->abnick) ? " " : "",
      (as.n_addrbk > 1 && pab->abnick) ? pab->abnick : "",
      (ps_global->remote_abook_validity == -1) ? "resynchronize and " : "");
	if(tas && *tas){
	    restore_state(&((*tas)->state));
	    (*tas)->pab   = NULL;
	}
	else
	  restore_state(&state);

	return;
    }

    abook = pab->address_book;
    new_nickname[0] = '\0';
    exist_nick[0]   = '\0';

    if(addr){
	simple_a = NULL;
	a = NULL;
	/* rfc822_parse_adrlist feels free to destroy input so send copy */
	tmp_a_string = cpystr(addr);
	rfc822_parse_adrlist(&a, tmp_a_string, fakedomain);
	if(tmp_a_string)
	  fs_give((void **)&tmp_a_string);
	
	if(a){
	    simple_a = simple_addr_string(a, tmp_20k_buf, SIZEOF_20KBUF);
	    mail_free_address(&a);
	}

	if(simple_a && *simple_a)
	  abe = adrbk_lookup_by_addr(abook, simple_a, NULL);
	
	if(abe){
	    snprintf(prompt, sizeof(prompt), _("Warning: address exists with %s%s, continue "),
		    (abe->nickname && abe->nickname[0]) ? "nickname "
		     : (abe->fullname && abe->fullname[0]) ? "fullname "
							   : "no nickname",
		    (abe->nickname && abe->nickname[0]) ? abe->nickname
		     : (abe->fullname && abe->fullname[0]) ? abe->fullname
							   : "");
	    prompt[sizeof(prompt)-1] = '\0';
	    switch(want_to(prompt, 'y', 'x', NO_HELP, WT_NORM)){
	      case 'y':
		if(abe->nickname && abe->nickname[0]){
		    strncpy(new_nickname, abe->nickname, sizeof(new_nickname));
		    new_nickname[sizeof(new_nickname)-1] = '\0';
		    strncpy(exist_nick, new_nickname, sizeof(exist_nick));
		    exist_nick[sizeof(exist_nick)-1] = '\0';
		}

		break;
	      
	      default:
		goto take_to_addrbooks_cancel;
	    }
	}
    }

get_nick:
    abe = NULL;
    old_tag = NotSet;
    entry_num = NO_NEXT;

    /*----- nickname ------*/
    snprintf(prompt, sizeof(prompt),
      _("Enter new or existing nickname (one word and easy to remember): "));
    prompt[sizeof(prompt)-1] = '\0';
    if(!new_nickname[0] && nick){
	strncpy(new_nickname, nick, sizeof(new_nickname));
	new_nickname[sizeof(new_nickname)-1] = '\0';
    }

    rc = edit_nickname(abook, (AddrScrn_Disp *)NULL, command_line,
		new_nickname, prompt, h_oe_takenick, 1, 1);
    if(rc == -8){  /* this means an existing nickname was entered */
	abe = adrbk_lookup_by_nick(abook, new_nickname, &entry_num);
	if(!abe){  /* this shouldn't happen */
	    q_status_message1(SM_ORDER, 0, 4,
		_("Already an entry %s in address book!"), new_nickname);
	    goto take_to_addrbooks_cancel;
	}

	old_tag = abe->tag;

	if(abe->tag == Single && !strcmp(new_nickname, exist_nick)){
	    static ESCKEY_S choices[] = {
		{'r', 'r', "R", N_("Replace")},
		{'n', 'n', "N", N_("No")},
		{-1, 0, NULL, NULL}};

	    snprintf(prompt, sizeof(prompt), _("Entry %s (%s) exists, replace ? "),
		  new_nickname,
		  (abe->fullname && abe->fullname[0])
			? (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf,
							 SIZEOF_20KBUF, abe->fullname)
			: "<no long name>");
	    prompt[sizeof(prompt)-1] = '\0';
	    ans = radio_buttons(prompt,
				command_line,
				choices,
				'r',
				'x',
				h_oe_take_replace,
				RB_NORM);
	}
	else{
	    static ESCKEY_S choices[] = {
		{'r', 'r', "R", N_("Replace")},
		{'a', 'a', "A", N_("Add")},
		{'n', 'n', "N", N_("No")},
		{-1, 0, NULL, NULL}};

	    snprintf(prompt, sizeof(prompt),
		  _("%s %s (%s) exists, replace or add addresses to it ? "),
		  abe->tag == List ? "List" : "Entry",
		  new_nickname,
		  (abe->fullname && abe->fullname[0])
			? (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf,
							 SIZEOF_20KBUF, abe->fullname)
			: "<no long name>");
	    prompt[sizeof(prompt)-1] = '\0';

	    ans = radio_buttons(prompt,
				command_line,
				choices,
				'a',
				'x',
				h_oe_take_replace_or_add,
				RB_NORM);
	}

	switch(ans){
	  case 'y':
	  case 'r':
	    break;
	  
	  case 'n':
	    goto get_nick;
	    break;
	  
	  case 'a':
	    listadd++;
	    break;

	  default:
	    goto take_to_addrbooks_cancel;
	}
    }
    else if(rc != 0 && rc != -9)  /* -9 means a null nickname */
      goto take_to_addrbooks_cancel;

    if((long)abook->count > MAX_ADRBK_SIZE ||
       (old_tag == NotSet && (long)abook->count >= MAX_ADRBK_SIZE)){
	q_status_message(SM_ORDER, 3, 5,
	    _("Address book is at maximum size, cancelled."));
	dprint((2, "Addrbook at Max size, TakeAddr cancelled\n"));
	goto take_to_addrbooks_cancel;
    }

    if(listadd){
	/* count up size of existing list */
	if(abe->tag == List){
	    for(p = abe->addr.list; p != NULL && *p != NULL; p++)
	      ;/* do nothing */
	
	    old_size = p - abe->addr.list;
	}
	/* or size of existing single address */
	else if(abe->addr.addr && abe->addr.addr[0])
	  old_size = 1;
	else
	  old_size = 0;
    }
    else /* don't care about old size, they will be tossed in edit_entry */
      old_size = 0;

    /* make up an abe to pass to edit_entry */
    abe_copy = adrbk_newentry();
    abe_copy->nickname = cpystr(new_nickname);
    abe_copy->tag = List;
    abe_copy->addr.list = NULL;

    if(listadd){
	abe_copy->fullname = cpystr((abe->fullname && abe->fullname[0])
							? abe->fullname : "");
	abe_copy->fcc   = cpystr((abe->fcc && abe->fcc[0]) ? abe->fcc : "");
	abe_copy->extra = cpystr((abe->extra&&abe->extra[0]) ? abe->extra : "");
    }
    else{
	/*
	 * use passed in info if available
	 */
	abe_copy->fullname = cpystr((fullname && fullname[0])
				      ? fullname
				      : (abe && abe->fullname)
				        ? abe->fullname
				        : "");
	abe_copy->fcc      = cpystr((fcc && fcc[0])
				      ? fcc
				      : (abe && abe->fcc)
				        ? abe->fcc
				        : "");
	abe_copy->extra    = cpystr((comment && comment[0])
				      ? comment
				      : (abe && abe->extra)
				        ? abe->extra
				        : "");
    }

    /* get rid of duplicates */
    if(listadd){
	if(abe->tag == List){
	    int      elim_dup;
	    char   **q, **r;
	    ADDRESS *newadr, *oldadr;

	    for(q = new_entries; q != NULL && *q != NULL;){

		tmp_a_string = cpystr(*q);
		newadr = NULL;
		rfc822_parse_adrlist(&newadr, tmp_a_string, fakedomain);
		fs_give((void **) &tmp_a_string);

		elim_dup = (newadr == NULL);
		for(p = abe->addr.list;
		    !elim_dup && p != NULL && *p != NULL;
		    p++){
		    tmp_a_string = cpystr(*p);
		    oldadr = NULL;
		    rfc822_parse_adrlist(&oldadr, tmp_a_string, fakedomain);
		    fs_give((void **) &tmp_a_string);

		    if(address_is_same(newadr, oldadr))
		      elim_dup++;
		    
		    if(oldadr)
		      mail_free_address(&oldadr);
		}

		/* slide the addresses down one to eliminate newadr */
		if(elim_dup){
		    char *f;

		    f = *q;
		    for(r = q; r != NULL && *r != NULL; r++)
		      *r = *(r+1);
		      
		    if(f)
		      fs_give((void **) &f);
		}
		else
		  q++;

		if(newadr)
		  mail_free_address(&newadr);
	    }
	}
	else{
	    char   **q, **r;
	    ADDRESS *newadr, *oldadr;

	    tmp_a_string = cpystr(abe->addr.addr ? abe->addr.addr : "");
	    oldadr = NULL;
	    rfc822_parse_adrlist(&oldadr, tmp_a_string, fakedomain);
	    fs_give((void **) &tmp_a_string);

	    for(q = new_entries; q != NULL && *q != NULL;){

		tmp_a_string = cpystr(*q);
		newadr = NULL;
		rfc822_parse_adrlist(&newadr, tmp_a_string, fakedomain);
		fs_give((void **) &tmp_a_string);

		/* slide the addresses down one to eliminate newadr */
		if(address_is_same(newadr, oldadr)){
		    char *f;

		    f = *q;
		    for(r = q; r != NULL && *r != NULL; r++)
		      *r = *(r+1);
		      
		    if(f)
		      fs_give((void **) &f);
		}
		else
		  q++;

		if(newadr)
		  mail_free_address(&newadr);
	    }

	    if(oldadr)
	      mail_free_address(&oldadr);
	}

	if(!new_entries || !*new_entries){
	    q_status_message1(SM_ORDER, 0, 4,
		     _("All of the addresses are already included in \"%s\""),
		     new_nickname);
	    free_ae(&abe_copy);
	    if(tas && *tas){
		restore_state(&((*tas)->state));
		(*tas)->pab   = NULL;
	    }
	    else
	      restore_state(&state);

	    return;
	}
    }

    /* count up size of new list */
    for(p = new_entries; p != NULL && *p != NULL; p++)
      ;/* do nothing */
    
    new_size = p - new_entries;
    tot_size = old_size + new_size;
    abe_copy->addr.list = (char **) fs_get((tot_size+1) * sizeof(char *));
    memset((void *) abe_copy->addr.list, 0, (tot_size+1) * sizeof(char *));
    if(old_size > 0){
	if(abe->tag == List){
	    for(i = 0; i < old_size; i++)
	      abe_copy->addr.list[i] = cpystr(abe->addr.list[i]);
	}
	else
	  abe_copy->addr.list[0] = cpystr(abe->addr.addr);
    }

    /* add new addresses to list */
    if(tot_size == 1 && addr)
      abe_copy->addr.list[0] = cpystr(addr);
    else
      for(i = 0; i < new_size; i++)
	abe_copy->addr.list[old_size + i] = cpystr(new_entries[i]);

    abe_copy->addr.list[tot_size] = NULL;

    if(F_ON(F_DISABLE_TAKE_FULLNAMES, ps_global)){
	for(i = 0; abe_copy->addr.list[i]; i++){
	    simple_a = NULL;
	    a = NULL;
	    tmp_a_string = cpystr(abe_copy->addr.list[i]);
	    rfc822_parse_adrlist(&a, tmp_a_string, fakedomain);
	    if(tmp_a_string)
	      fs_give((void **) &tmp_a_string);

	    if(a){
		simple_a = simple_addr_string(a, tmp_20k_buf, SIZEOF_20KBUF);
		mail_free_address(&a);
	    }

	    /* replace the old addr string with one with no full name */
	    if(simple_a && *simple_a){
		if(abe_copy->addr.list[i])
		  fs_give((void **) &abe_copy->addr.list[i]);

		abe_copy->addr.list[i] = cpystr(simple_a);
	    }
	}
    }

    edit_entry(abook, abe_copy, (a_c_arg_t) entry_num, old_tag, 0, NULL, cmd);

    /* free copy */
    free_ae(&abe_copy);
    if(tas && *tas){
	restore_state(&((*tas)->state));
	(*tas)->pab   = NULL;
    }
    else
      restore_state(&state);

    return;

take_to_addrbooks_cancel:
    q_status_message(SM_INFO, 0, 2, _("Address book addition cancelled"));
    if(tas && *tas){
	restore_state(&((*tas)->state));
	(*tas)->pab   = NULL;
    }
    else
      restore_state(&state);
}


/*
 * Prep addrbook for TakeAddr add operation.
 *
 * Arg: savep -- Address of a pointer to save addrbook state in.
 *      stp   -- Address of a pointer to save addrbook state in.
 *
 * Returns: a PerAddrBook pointer, or NULL.
 */
PerAddrBook *
setup_for_addrbook_add(SAVE_STATE_S *state, int command_line, char *cmd)
{
    PerAddrBook *pab;
    int          save_rem_abook_valid = 0;

    init_ab_if_needed();
    save_state(state);

    if(as.n_addrbk == 0){
        q_status_message(SM_ORDER, 3, 4, _("No address book configured!"));
        return NULL;
    }
    else
      pab = use_this_addrbook(command_line, cmd);
    
    if(!pab)
      return NULL;

    if((pab->type & REMOTE_VIA_IMAP) && ps_global->remote_abook_validity == -1){
	save_rem_abook_valid = -1;
	ps_global->remote_abook_validity = 0;
    }

    /* initialize addrbook so we can add to it */
    init_abook(pab, Open);

    if(save_rem_abook_valid)
	ps_global->remote_abook_validity = save_rem_abook_valid;

    if(pab->ostatus != Open){
        q_status_message(SM_ORDER, 3, 4, _("Can't open address book!"));
        return NULL;
    }

    if(pab->access != ReadWrite){
	if(pab->access == ReadOnly)
	  q_status_message(SM_ORDER, 0, 4, _("AddressBook is Read Only"));
	else if(pab->access == NoAccess)
	  q_status_message(SM_ORDER, 3, 4,
		_("AddressBook not accessible, permission denied"));

        return NULL;
    }

    return(pab);
}


/*
 * Interact with user to figure out which address book they want to add a
 * new entry (TakeAddr) to.
 *
 * Args: command_line -- just the line to prompt on
 *
 * Results: returns a pab pointing to the selected addrbook, or NULL.
 */
PerAddrBook *
use_this_addrbook(int command_line, char *cmd)
{
    HelpType   help;
    int        rc = 0;
    PerAddrBook  *pab, *the_only_pab;
#define MAX_ABOOK 2000
    int        i, abook_num, count_read_write;
    char       addrbook[MAX_ABOOK + 1],
               prompt[MAX_ABOOK + 81];
    static ESCKEY_S ekey[] = {
	{-2,   0,   NULL, NULL},
	{ctrl('P'), 10, "^P", N_("Prev AddrBook")},
	{ctrl('N'), 11, "^N", N_("Next AddrBook")},
	{KEY_UP,    10, "", ""},
	{KEY_DOWN,  11, "", ""},
	{-1, 0, NULL, NULL}};

    dprint((9, "- use_this_addrbook -\n"));

    /* check for only one ReadWrite addrbook */
    count_read_write = 0;
    for(i = 0; i < as.n_addrbk; i++){
	pab = &as.adrbks[i];
	/*
	 * NoExists is counted, too, so the user can add to an empty
	 * addrbook the first time.
	 */
	if(pab->access == ReadWrite ||
	   pab->access == NoExists ||
	   pab->access == MaybeRorW){
	    count_read_write++;
	    the_only_pab = &as.adrbks[i];
	}
    }

    /* only one usable addrbook, use it */
    if(count_read_write == 1)
      return(the_only_pab);

    /* no addrbook to write to */
    if(count_read_write == 0){
	q_status_message2(SM_ORDER | SM_DING, 3, 4,
			  "No %sAddressbook to %s to!",
			  (as.n_addrbk > 0) ? "writable " : "", cmd);
	return NULL;
    }

    /* start with the first addrbook */
    abook_num = 0;
    pab       = &as.adrbks[abook_num];
    strncpy(addrbook, pab->abnick, sizeof(addrbook)-1);
    addrbook[sizeof(addrbook)-1] = '\0';
    snprintf(prompt, sizeof(prompt), "%c%s to which addrbook : %s",
	islower((unsigned char)(*cmd)) ? toupper((unsigned char)*cmd) : *cmd,
	cmd+1,
	(pab->access == ReadOnly || pab->access == NoAccess) ?
	    "[ReadOnly] " : "");
    prompt[sizeof(prompt)-1] = '\0';
    help = NO_HELP;
    ps_global->mangled_footer = 1;
    do{
	int flags;

	if(!pab)
          q_status_message1(SM_ORDER, 3, 4, _("No addressbook \"%s\""),
			    addrbook);

	if(rc == 3)
          help = (help == NO_HELP ? h_oe_chooseabook : NO_HELP);

	flags = OE_APPEND_CURRENT;
	rc = optionally_enter(addrbook, command_line, 0, sizeof(addrbook),
			      prompt, ekey, help, &flags);

	if(rc == 1){ /* ^C */
	    char capcmd[50];

	    snprintf(capcmd, sizeof(capcmd),
		"%c%s",
		islower((unsigned char)(*cmd)) ? toupper((unsigned char)*cmd)
					       : *cmd,
		cmd+1);
	    capcmd[sizeof(capcmd)-1] = '\0';
	    cmd_cancelled(capcmd);
	    break;
	}

	if(rc == 10){			/* Previous addrbook */
	    if(--abook_num < 0)
	      abook_num = as.n_addrbk - 1;

	    pab = &as.adrbks[abook_num];
	    strncpy(addrbook, pab->abnick, sizeof(addrbook)-1);
	    addrbook[sizeof(addrbook)-1] = '\0';
	    snprintf(prompt, sizeof(prompt), "%s to which addrbook : %s", cmd,
		(pab->access == ReadOnly || pab->access == NoAccess) ?
		    "[ReadOnly] " : "");
	    prompt[sizeof(prompt)-1] = '\0';
	}
	else if(rc == 11){			/* Next addrbook */
	    if(++abook_num > as.n_addrbk - 1)
	      abook_num = 0;

	    pab = &as.adrbks[abook_num];
	    strncpy(addrbook, pab->abnick, sizeof(addrbook)-1);
	    addrbook[sizeof(addrbook)-1] = '\0';
	    snprintf(prompt, sizeof(prompt), "%s to which addrbook : %s", cmd,
		(pab->access == ReadOnly || pab->access == NoAccess) ?
		    "[ReadOnly] " : "");
	    prompt[sizeof(prompt)-1] = '\0';
	}

    }while(rc == 2 || rc == 3 || rc == 4 || rc == 10 || rc == 11 || rc == 12 ||
           !(pab = check_for_addrbook(addrbook)));
            
    ps_global->mangled_footer = 1;

    if(rc != 0)
      return NULL;

    return(pab);
}


/*
 * Return a pab pointer to the addrbook which corresponds to the argument.
 * 
 * Args: addrbook -- the string representing the addrbook.
 *
 * Results: returns a PerAddrBook pointer for the referenced addrbook, NULL
 *          if none.  First the nicknames are checked and then the filenames.
 *          This must be one of the existing addrbooks.
 */
PerAddrBook *
check_for_addrbook(char *addrbook)
{
    register int i;
    register PerAddrBook *pab;

    for(i = 0; i < as.n_addrbk; i++){
	pab = &as.adrbks[i];
	if(strcmp(pab->abnick, addrbook) == 0)
	  break;
    }

    if(i < as.n_addrbk)
      return(pab);

    for(i = 0; i < as.n_addrbk; i++){
	pab = &as.adrbks[i];
	if(strcmp(pab->filename, addrbook) == 0)
	  break;
    }

    if(i < as.n_addrbk)
      return(pab);
    
    return NULL;
}


/*
 * Screen for selecting which addresses to Take to address book.
 *
 * Args:      ps -- Pine state
 *       ta_list -- Screen is formed from this list of addresses
 *  how_many_selected -- how many checked initially in ListMode
 *          mode -- which mode to start in
 *
 * Result: an address book may be updated
 * Returns  -- 0 normally
 *             1 if it returns before redrawing screen
 */
int
takeaddr_screen(struct pine *ps, TA_S *ta_list, int how_many_selected,
		ScreenMode mode, TA_STATE_S **tas, char *command)
{
    UCS           ch = 'x';
    int		  cmd, dline, give_warn_message, command_line;
    int		  km_popped = 0,
		  directly_to_take = 0,
		  ret = 0,
		  done = 0;
    TA_S	 *current = NULL,
		 *ctmp = NULL;
    TA_SCREEN_S   screen;
    Pos           cursor_pos;
    char         *utf8str;
    struct key_menu *km;

    dprint((2, "- takeaddr_screen -\n"));

    command_line = -FOOTER_ROWS(ps);  /* third line from the bottom */

    screen.current = screen.top_line = NULL;
    screen.mode    = mode;

    if(ta_list == NULL){
	/* TRANSLATORS: something like
	   No addresses to save, cancelled */
	q_status_message1(SM_INFO, 0, 2, "No addresses to %s, cancelled",
			  command);
	return 1;
    }

    current	       = first_sel_taline(ta_list);
    ps->mangled_screen = 1;
    ta_screen	       = &screen;

    if(is_talist_of_one(current)){
	directly_to_take++;
	screen.mode = SingleMode; 
    }
    else if(screen.mode == ListMode)
      q_status_message(SM_INFO, 0, 1,
	    _("List mode: Use \"X\" to mark addresses to be included in list"));
    else
      q_status_message(SM_INFO, 0, 1,
	    _("Single mode: Use \"P\" or \"N\" to select desired address"));

    while(!done){
	if(km_popped){
	    km_popped--;
	    if(km_popped == 0){
		clearfooter(ps);
		ps->mangled_body = 1;
	    }
	}

	if(screen.mode == ListMode)
	  ps->redrawer = takeaddr_screen_redrawer_list;
	else
	  ps->redrawer = takeaddr_screen_redrawer_single;

	if(ps->mangled_screen){
	    ps->mangled_header = 1;
	    ps->mangled_footer = 1;
	    ps->mangled_body   = 1;
	    ps->mangled_screen = 0;
	}

	/*----------- Check for new mail -----------*/
	if(new_mail(0, NM_TIMING(ch), NM_STATUS_MSG | NM_DEFER_SORT) >= 0)
	  ps->mangled_header = 1;

#ifdef _WINDOWS
	mswin_beginupdate();
#endif
	if(ps->mangled_header){
	    char tbuf[40];

	    snprintf(tbuf, sizeof(tbuf), "TAKE ADDRESS SCREEN (%s Mode)",
				    (screen.mode == ListMode) ? "List"
							      : "Single");
	    tbuf[sizeof(tbuf)-1] = '\0';
            set_titlebar(tbuf, ps->mail_stream, ps->context_current,
                             ps->cur_folder, ps->msgmap, 1, FolderName, 0, 0,
			     NULL);
	    ps->mangled_header = 0;
	}

	dline = update_takeaddr_screen(ps, current, &screen, &cursor_pos);
	if(F_OFF(F_SHOW_CURSOR, ps)){
	    cursor_pos.row = ps->ttyo->screen_rows - FOOTER_ROWS(ps);
	    cursor_pos.col = 0;
	}

	/*---- This displays new mail notification, or errors ---*/
	if(km_popped){
	    FOOTER_ROWS(ps_global) = 3;
	    mark_status_unknown();
	}

        display_message(ch);
	if(km_popped){
	    FOOTER_ROWS(ps_global) = 1;
	    mark_status_unknown();
	}

	/*---- Redraw footer ----*/
	if(ps->mangled_footer){
	    bitmap_t bitmap;

	    if(km_popped){
		FOOTER_ROWS(ps) = 3;
		clearfooter(ps);
	    }

	    setbitmap(bitmap);
	    ps->mangled_footer = 0;

	    km = (screen.mode == ListMode) ? &ta_keymenu_lm : &ta_keymenu_sm;

	    menu_clear_binding(km, KEY_LEFT);
	    menu_clear_binding(km, KEY_RIGHT);
	    if(F_ON(F_ARROW_NAV, ps_global)){
		int cmd;

		if((cmd = menu_clear_binding(km, '<')) != MC_UNKNOWN){
		    menu_add_binding(km, '<', cmd);
		    menu_add_binding(km, KEY_LEFT, cmd);
		}

		if((cmd = menu_clear_binding(km, '>')) != MC_UNKNOWN){
		    menu_add_binding(km, '>', cmd);
		    menu_add_binding(km, KEY_RIGHT, cmd);
		}
	    }

	    draw_keymenu(km, bitmap, ps->ttyo->screen_cols,
			 1 - FOOTER_ROWS(ps_global), 0, FirstMenu);

	    if(km_popped){
		FOOTER_ROWS(ps) = 1;
		mark_keymenu_dirty();
	    }
	}

#ifdef _WINDOWS
	mswin_endupdate();
#endif
        /*------ Read the command from the keyboard ----*/
	MoveCursor(cursor_pos.row, cursor_pos.col);

	if(directly_to_take){  /* bypass this screen */
	    cmd = MC_TAKE;
	    blank_keymenu(ps_global->ttyo->screen_rows - 2, 0);
	}
	else {
#ifdef	MOUSE
	    mouse_in_content(KEY_MOUSE, -1, -1, 0, 0);
	    register_mfunc(mouse_in_content, HEADER_ROWS(ps_global), 0,
			   ps_global->ttyo->screen_rows - (FOOTER_ROWS(ps)+1),
			   ps_global->ttyo->screen_cols);
#endif

#ifdef  _WINDOWS
	    mswin_setscrollcallback(ta_scroll_callback);
#endif
	    ch = READ_COMMAND(&utf8str);
#ifdef	MOUSE
	    clear_mfunc(mouse_in_content);
#endif

#ifdef  _WINDOWS
	    mswin_setscrollcallback(NULL);
#endif
	    cmd = menu_command(ch, km);
	    if (ta_screen->current)
	      current = ta_screen->current;

	    if(km_popped)
	      switch(cmd){
		case MC_NONE :
		case MC_OTHER :
		case MC_RESIZE :
		case MC_REPAINT :
		  km_popped++;
		  break;

		default:
		  clearfooter(ps);
		  break;
	      }
	}

	switch(cmd){
	  case MC_HELP :			/* help! */
	    if(FOOTER_ROWS(ps_global) == 1 && km_popped == 0){
		km_popped = 2;
		ps_global->mangled_footer = 1;
		break;
	    }

	    helper(h_takeaddr_screen, _("HELP FOR TAKE ADDRESS SCREEN"),
		   HLPD_SIMPLE);
	    ps->mangled_screen = 1;
	    break;

	  case MC_EXIT:				/* exit takeaddr screen */
	    q_status_message(SM_INFO, 0, 2, _("Address book addition cancelled"));
	    ret = 1;
	    done++;
	    break;

	  case MC_TAKE:
	    if(ta_do_take(current, how_many_selected, command_line, tas,
	       command))
	      done++;
	    else
	      directly_to_take = 0;

	    break;

	  case MC_CHARDOWN :			/* next list element */
	    if((ctmp = next_sel_taline(current)) != NULL)
	      current = ctmp;
	    else
	      q_status_message(SM_INFO, 0, 1, _("Already on last line."));

	    break;

	  case MC_CHARUP:			/* previous list element */
	    if((ctmp = pre_sel_taline(current)) != NULL)
	      current = ctmp;
	    else
	      q_status_message(SM_INFO, 0, 1, _("Already on first line."));

	    break;

	  case MC_PAGEDN :			/* page forward */
	    give_warn_message = 1;
	    while(dline++ < ps->ttyo->screen_rows - FOOTER_ROWS(ps)){
	        if((ctmp = next_sel_taline(current)) != NULL){
		    current = ctmp;
		    give_warn_message = 0;
		}
	        else
		    break;
	    }

	    if(give_warn_message)
	      q_status_message(SM_INFO, 0, 1, _("Already on last page."));

	    break;

	  case MC_PAGEUP :			/* page backward */
	    /* move to top of screen */
	    give_warn_message = 1;
	    while(dline-- > HEADER_ROWS(ps_global)){
	        if((ctmp = pre_sel_taline(current)) != NULL){
		    current = ctmp;
		    give_warn_message = 0;
		}
	        else
		    break;
	    }

	    /* page back one screenful */
	    while(++dline < ps->ttyo->screen_rows - FOOTER_ROWS(ps)){
	        if((ctmp = pre_sel_taline(current)) != NULL){
		    current = ctmp;
		    give_warn_message = 0;
		}
	        else
		    break;
	    }

	    if(give_warn_message)
	      q_status_message(SM_INFO, 0, 1, _("Already on first page."));

	    break;

	  case MC_WHEREIS :			/* whereis */
	    if((ctmp = whereis_taline(current)) != NULL)
 	      current = ctmp;
	    
	    ps->mangled_footer = 1;
	    break;

	  case KEY_SCRLTO:
	    /* no op for now */
	    break;

#ifdef MOUSE	    
	  case MC_MOUSE:
	    {
		MOUSEPRESS mp;

		mouse_get_last(NULL, &mp);
		mp.row -= HEADER_ROWS(ps_global);
		ctmp = screen.top_line;
		if(mp.doubleclick){
		    if(screen.mode == SingleMode){
			if(ta_do_take(current, how_many_selected, command_line,
				      tas, command))
			  done++;
			else
			  directly_to_take = 0;
		    }
		    else{
			current->checked = !current->checked;  /* flip it */
			how_many_selected += (current->checked ? 1 : -1);
		    }
		}
		else{
		    while(mp.row && ctmp != NULL){
			--mp.row;
			do  ctmp = ctmp->next;
			while(ctmp != NULL && ctmp->skip_it && !ctmp->print);
		    }

		    if(ctmp != NULL && !ctmp->skip_it)
		      current = ctmp;
		}
	    }
	    break;
#endif

	  case MC_REPAINT :
          case MC_RESIZE :
	    ClearScreen();
	    ps->mangled_screen = 1;
	    break;

	  case MC_CHOICE :			/* [UN]select this addr */
	    current->checked = !current->checked;  /* flip it */
	    how_many_selected += (current->checked ? 1 : -1);
	    break;

	  case MC_SELALL :			/* select all */
	    how_many_selected = ta_mark_all(first_sel_taline(current));
	    ps->mangled_body = 1;
	    break;

	  case MC_UNSELALL:			/* unselect all */
	    how_many_selected = ta_unmark_all(first_sel_taline(current));
	    ps->mangled_body = 1;
	    break;

	  case MC_LISTMODE:			/* switch to SingleMode */
	    if(screen.mode == ListMode){
		screen.mode = SingleMode;
		q_status_message(SM_INFO, 0, 1,
		  _("Single mode: Use \"P\" or \"N\" to select desired address"));
	    }
	    else{
		screen.mode = ListMode;
		q_status_message(SM_INFO, 0, 1,
	    _("List mode: Use \"X\" to mark addresses to be included in list"));

		if(how_many_selected <= 1){
		    how_many_selected =
				    ta_unmark_all(first_sel_taline(current));
		    current->checked = 1;
		    how_many_selected++;
		}
	    }

	    ps->mangled_screen = 1;
	    break;


	  case MC_NONE :			/* simple timeout */
	    break;


	    /* Unbound (or not dealt with) keystroke */
	  case MC_CHARRIGHT :
	  case MC_CHARLEFT :
	  case MC_GOTOBOL :
	  case MC_GOTOEOL :
	  case MC_UNKNOWN :
	  default:
	    bogus_command(ch, F_ON(F_USE_FK, ps) ? "F1" : "?");
	    break;

	  case MC_UTF8:
	    bogus_utf8_command(utf8str, F_ON(F_USE_FK, ps) ? "F1" : "?");
	    break;
	}
    }

    ps->mangled_screen = 1;

    return(ret);
}


/*
 * Do what takeaddr_screen does except bypass the takeaddr_screen and
 * go directly to do_take.
 */
void
takeaddr_bypass(struct pine *ps, TA_S *current, TA_STATE_S **tasp)
{
    TA_SCREEN_S screen;	 /* We have to fake out ta_do_take because */
			 /* we're bypassing takeaddr_screen.       */
    ta_screen = &screen;
    ta_screen->mode = SingleMode;
    current = first_sel_taline(current);
    (void) ta_do_take(current, 1, -FOOTER_ROWS(ps_global), tasp, _("save"));
    ps->mangled_screen = 1;
}


/*
 *
 */
int
ta_do_take(TA_S *current, int how_many_selected, int command_line,
	   TA_STATE_S **tas, char *cmd)
{
    return((ta_screen->mode == ListMode)
	      ? ta_take_marked_addrs(how_many_selected,
				     first_sel_taline(current),
				     command_line, tas, cmd)
	      : ta_take_single_addr(current, command_line, tas, cmd));
}


/*
 * WhereIs for TakeAddr screen.
 *
 * Returns the line match is found in or NULL.
 */
TA_S *
whereis_taline(TA_S *current)
{
    TA_S *p;
    int   rc, found = 0, wrapped = 0, flags;
    char *result = NULL, buf[MAX_SEARCH+1], tmp[MAX_SEARCH+20];
    static char last[MAX_SEARCH+1];
    HelpType help;
    static ESCKEY_S ekey[] = {
	{0, 0, "", ""},
	{ctrl('Y'), 10, "^Y", N_("Top")},
	{ctrl('V'), 11, "^V", N_("Bottom")},
	{-1, 0, NULL, NULL}};

    if(!current)
      return NULL;

    /*--- get string  ---*/
    buf[0] = '\0';
    snprintf(tmp, sizeof(tmp), _("Word to find %s%.*s%s: "),
	     (last[0]) ? "[" : "",
	     sizeof(tmp)-20, (last[0]) ? last : "",
	     (last[0]) ? "]" : "");
    tmp[sizeof(tmp)-1] = '\0';
    help = NO_HELP;
    flags = OE_APPEND_CURRENT | OE_KEEP_TRAILING_SPACE;
    while(1){
	rc = optionally_enter(buf,-FOOTER_ROWS(ps_global),0,sizeof(buf),
				 tmp,ekey,help,&flags);
	if(rc == 3)
	  help = help == NO_HELP ? h_config_whereis : NO_HELP;
	else if(rc == 0 || rc == 1 || rc == 10 || rc == 11 || !buf[0]){
	    if(rc == 0 && !buf[0] && last[0]){
		strncpy(buf, last, sizeof(buf)-1);
		buf[sizeof(buf)-1] = '\0';
	    }

	    break;
	}
    }

    if(rc == 0 && buf[0]){
	p = current;
	while((p = next_taline(p)) != NULL)
	  if(srchstr((char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf,
						    SIZEOF_20KBUF, p->strvalue),
		     buf)){
	      found++;
	      break;
	  }

	if(!found){
	    p = first_taline(current);

	    while(p != current)
	      if(srchstr((char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf,
							SIZEOF_20KBUF, p->strvalue),
			 buf)){
		  found++;
		  wrapped++;
		  break;
	      }
	      else
		p = next_taline(p);
	}
    }
    else if(rc == 10){
	current = first_sel_taline(current);
	result = _("Searched to top");
    }
    else if(rc == 11){
	current = last_sel_taline(current);
	result = _("Searched to bottom");
    }
    else{
	current = NULL;
	result = _("WhereIs cancelled");
    }

    if(found){
	current = p;
	result  = wrapped ? _("Search wrapped to beginning") : _("Word found");
	strncpy(last, buf, sizeof(last)-1);
	last[sizeof(last)-1] = '\0';
    }

    q_status_message(SM_ORDER,0,3,result ? result : _("Word not found"));
    return(current);
}


/*
 * Call the addrbook functions which add the checked addresses.
 *
 * Args: how_many_selected -- how many addresses are checked
 *                  f_line -- the first ta line
 *
 * Returns: 1 -- we're done, caller should return
 *          0 -- we're not done
 */
int
ta_take_marked_addrs(int how_many_selected, TA_S *f_line, int command_line,
		     TA_STATE_S **tas, char *cmd)
{
    char **new_list;
    TA_S  *p;

    if(how_many_selected == 0){
	q_status_message(SM_ORDER, 0, 4,
  _("No addresses marked for taking. Use ExitTake to leave TakeAddr screen"));
	return 0;
    }

    if(how_many_selected == 1){
	for(p = f_line; p; p = next_sel_taline(p))
	  if(p->checked && !p->skip_it)
	    break;

	if(p)
	  add_abook_entry(p,
			  (p->nickname && p->nickname[0]) ? p->nickname : NULL,
			  (p->fullname && p->fullname[0]) ? p->fullname : NULL,
			  (p->fcc && p->fcc[0]) ? p->fcc : NULL,
			  (p->comment && p->comment[0]) ? p->comment : NULL,
			  command_line, tas, cmd);
    }
    else{
	new_list = list_of_checked(f_line);
	for(p = f_line; p; p = next_sel_taline(p))
	  if(p->checked && !p->skip_it)
	    break;

	take_to_addrbooks_frontend(new_list, p ? p->nickname : NULL,
		p ? p->fullname : NULL, NULL, p ? p->fcc : NULL,
		p ? p->comment : NULL, command_line, tas, cmd);
	free_list_array(&new_list);
    }

    return 1;
}


int
ta_take_single_addr(TA_S *cur, int command_line, TA_STATE_S **tas, char *cmd)
{
    add_abook_entry(cur,
		    (cur->nickname && cur->nickname[0]) ? cur->nickname : NULL,
		    (cur->fullname && cur->fullname[0]) ? cur->fullname : NULL,
		    (cur->fcc && cur->fcc[0]) ? cur->fcc : NULL,
		    (cur->comment && cur->comment[0]) ? cur->comment : NULL,
		    command_line, tas, cmd);

    return 1;
}


/*
 * Manage display of the Take Address screen.
 *
 * Args:     ps -- pine state
 *      current -- the current TA line
 *       screen -- the TA screen
 *   cursor_pos -- return good cursor position here
 */
int
update_takeaddr_screen(struct pine *ps, TA_S *current, TA_SCREEN_S *screen, Pos *cursor_pos)
{
    int	   dline;
    TA_S  *top_line,
          *ctmp;
    int    longest, i, j;
    char   buf1[6*MAX_SCREEN_COLS + 30];
    char   buf2[6*MAX_SCREEN_COLS + 30];
    char  *p, *q;
    int screen_width = ps->ttyo->screen_cols;
    Pos    cpos;

    cpos.row = HEADER_ROWS(ps); /* default return value */

    /* calculate top line of display */
    dline = 0;
    top_line = 0;

    if (ta_screen->top_line){
      for(dline = 0, ctmp = ta_screen->top_line; 
	 ctmp && ctmp != current; ctmp = next_taline(ctmp))
	dline++;

      if (ctmp && (dline < ps->ttyo->screen_rows - HEADER_ROWS(ps)
		   - FOOTER_ROWS(ps)))
	top_line = ta_screen->top_line;
    }

    if (!top_line){
      dline = 0;
      ctmp = top_line = first_taline(current);
      do
	if(((dline++) % (ps->ttyo->screen_rows - HEADER_ROWS(ps)
			 - FOOTER_ROWS(ps))) == 0)
	  top_line = ctmp;
      while(ctmp != current && (ctmp = next_taline(ctmp)));
    }

#ifdef _WINDOWS
    /*
     * Figure out how far down the top line is from the top and how many
     * total lines there are.  Dumb to loop every time thru, but
     * there aren't that many lines, and it's cheaper than rewriting things
     * to maintain a line count in each structure...
     */
    for(dline = 0, ctmp = pre_taline(top_line); ctmp; ctmp = pre_taline(ctmp))
      dline++;

    scroll_setpos(dline);

    for(ctmp = next_taline(top_line); ctmp ; ctmp = next_taline(ctmp))
      dline++;

    scroll_setrange(ps->ttyo->screen_rows - FOOTER_ROWS(ps) - HEADER_ROWS(ps),
		   dline);
#endif


    /* mangled body or new page, force redraw */
    if(ps->mangled_body || screen->top_line != top_line)
      screen->current = NULL;
    
    /* find width of longest line for nicer formatting */
    longest = 0;
    for(ctmp = first_taline(top_line); ctmp; ctmp = next_taline(ctmp)){
	int width;

	if(ctmp
	   && !ctmp->print
	   && ctmp->strvalue
	   && longest < (width = utf8_width((char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf,
									   SIZEOF_20KBUF, ctmp->strvalue))))
	  longest = width;
    }

#define LENGTH_OF_THAT_STRING 5   /* "[X]  " */
    longest = MIN(longest, ps->ttyo->screen_cols);

    /* loop thru painting what's needed */
    for(dline = 0, ctmp = top_line;
	dline < ps->ttyo->screen_rows - FOOTER_ROWS(ps) - HEADER_ROWS(ps);
	dline++, ctmp = next_taline(ctmp)){

	/*
	 * only fall thru painting if something needs painting...
	 */
	if(!ctmp || !screen->current || ctmp == screen->current ||
				   ctmp == top_line || ctmp == current){
	    ClearLine(dline + HEADER_ROWS(ps));
	    if(!ctmp || !ctmp->strvalue)
	      continue;
	}

	p = buf1;
	if(ctmp == current){
	    cpos.row = dline + HEADER_ROWS(ps);  /* col set below */
	    StartInverse();
	}

	if(ctmp->print)
	  j = 0;
	else
	  j = LENGTH_OF_THAT_STRING;

	/*
	 * Copy the value to a temp buffer expanding tabs, and
	 * making sure not to write beyond screen right...
	 */
	q = (char *) rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf,
					    SIZEOF_20KBUF, ctmp->strvalue);

	for(i = 0; q[i] && j < ps->ttyo->screen_cols && p-buf1 < sizeof(buf1); i++){
	    if(q[i] == ctrl('I')){
		do
		  *p++ = SPACE;
		while(j < ps->ttyo->screen_cols && ((++j)&0x07) && p-buf1 < sizeof(buf1));
		      
	    }
	    else{
		*p++ = q[i];
		j++;
	    }
	}

	if(p-buf1 < sizeof(buf1))
	  *p = '\0';

	if(utf8_width(buf1) < longest){
	    (void) utf8_pad_to_width(buf2, buf1, sizeof(buf2), longest, 1);
	    /* it's expected to be in buf1 below */
	    strncpy(buf1, buf2, sizeof(buf1));
	    buf1[sizeof(buf1)-1] = '\0';
	}

	/* mark lines which have check marks */
	if(ctmp == current){
	    if(screen->mode == ListMode){
	        snprintf(buf2, sizeof(buf2), "[%c]  %s", ctmp->checked ? 'X' : SPACE, buf1);
		buf2[sizeof(buf2)-1] = '\0';
		cpos.col = 1; /* position on the X */
	    }
	    else{
	        snprintf(buf2, sizeof(buf2), "     %s", buf1);
		buf2[sizeof(buf2)-1] = '\0';
		cpos.col = 5; /* 5 spaces before text */
	    }
	}
	else{
	    if(ctmp->print){
		int width, actual_width;
		size_t len;

		/*
		 * In buf2 make ------string--------
		 * which reaches all the way across the screen. String will
		 * already have a leading and trailing space.
		 */
		width = utf8_width(buf1);

		if(width > screen_width){
		    actual_width = utf8_truncate(buf1, screen_width);
		    /* it might be 1 less */
		    if(actual_width < screen_width && (len=strlen(buf1))+1 < sizeof(buf1)){
		      buf1[len] = SPACE;
		      buf1[len+1] = '\0';
		    }
		}
		else{
		    snprintf(buf2, sizeof(buf2), "%s%s",
			     repeat_char((screen_width-width)/2, '-'), buf1);
		    buf2[sizeof(buf2)-1] = '\0';
		    len = strlen(buf2);
		    width = utf8_width(buf2);
		    snprintf(buf2+len, sizeof(buf2)-len, "%s",
			     repeat_char(screen_width-width, '-'));
		    buf2[sizeof(buf2)-1] = '\0';
		}
	    }
	    else{
		if(screen->mode == ListMode)
	          snprintf(buf2, sizeof(buf2), "[%c]  %.*s", ctmp->checked ? 'X' : SPACE,
			  sizeof(buf2)-6, buf1);
		else
	          snprintf(buf2, sizeof(buf2), "     %.*s", sizeof(buf2)-6, buf1);

		buf2[sizeof(buf2)-1] = '\0';
	    }
	}

	PutLine0(dline + HEADER_ROWS(ps), 0, buf2);

	if(ctmp == current)
	  EndInverse();
    }

    ps->mangled_body = 0;
    screen->top_line = top_line;
    screen->current  = current;
    if(cursor_pos)
      *cursor_pos = cpos;

    return(cpos.row);
}


void
takeaddr_screen_redrawer_list(void)
{
    ps_global->mangled_body = 1;
    (void)update_takeaddr_screen(ps_global, ta_screen->current, ta_screen,
	(Pos *)NULL);
}


void
takeaddr_screen_redrawer_single(void)
{
    ps_global->mangled_body = 1;
    (void)update_takeaddr_screen(ps_global, ta_screen->current, ta_screen,
	(Pos *)NULL);
}


/* jpf work in progress
 * Execute command to take addresses out of message and put in the address book
 *
 * Args: ps     -- pine state
 *       msgmap -- the MessageMap
 *       agg    -- this is aggregate operation if set
 *
 * Result: The entry is added to an address book.
 */     
int
cmd_take_addr(struct pine *ps, MSGNO_S *msgmap, int agg)
{
    TA_S *ta_list = NULL;
    int  how_many_selected = 0, rtype;

    /* Ask user what kind of Take they want to do */
    if(!agg && F_ON(F_ENABLE_ROLE_TAKE, ps)){
	rtype = rule_setup_type(ps,
				RS_RULES |
				  ((mn_get_total(msgmap) > 0)
				    ? (F_ON(F_ENABLE_TAKE_EXPORT, ps)
				       ? (RS_INCADDR | RS_INCEXP)
				       : RS_INCADDR)
				    : RS_NONE),
				"Take to : ");
    }
    else if(F_ON(F_ENABLE_TAKE_EXPORT, ps) && mn_get_total(msgmap) > 0)
      rtype = rule_setup_type(ps, RS_INCADDR | RS_INCEXP, "Take to : ");
    else
      rtype = 'a';

    if(rtype == 'x' || rtype == 'Z'){
	if(rtype == 'x')
	  cmd_cancelled(NULL);
	else if(rtype == 'Z')
	  q_status_message(SM_ORDER | SM_DING, 3, 5,
			   "Try turning on color with the Setup/Kolor command.");
	return -1;
    }

    ps->mangled_footer = 1;

    if(rtype > 0)
      switch(rtype){
	case 'e':
	  {
	      LINES_TO_TAKE *lines_to_take = NULL;

	      rtype = set_up_takeaddr('e', ps, msgmap, &ta_list, &how_many_selected,
				      agg ? TA_AGG : 0, NULL);

	      if(rtype >= 0){
		  if(convert_ta_to_lines(ta_list, &lines_to_take)){
		      while(lines_to_take && lines_to_take->prev)
			lines_to_take = lines_to_take->prev;

		      take_to_export(ps, lines_to_take);

		      free_ltlines(&lines_to_take);
		  }
		  else
		    q_status_message(SM_ORDER, 3, 4, _("Can't find anything to export"));
	      }
	  }

	  break;

	case 'r':
	case 's':
	case 'i':
	case 'f':
	case 'o':
	case 'c':
	case 'x':
	  role_take(ps, msgmap, rtype);
	  break;

	case 'a':
	  rtype = set_up_takeaddr('a', ps, msgmap, &ta_list, &how_many_selected,
				  agg ? TA_AGG : 0, attached_addr_handler);
	  if(rtype >= 0){
	      (void) takeaddr_screen(ps, ta_list, how_many_selected,
				     agg ? ListMode : SingleMode, 
				     NULL, _("take"));
	  }

	  break;

	default:
	  break;
      }
    
    /* clean up */
    free_talines(&ta_list);
    env_for_pico_callback   = NULL;
    body_for_pico_callback  = NULL;

    return(rtype >= 0 ? 1 : 0);
}


int
attached_addr_handler(TA_S *current, int added)
{
    char prompt[200];
    int	 command_line = -FOOTER_ROWS(ps_global);

    snprintf(prompt, sizeof(prompt),
	     "Take %d entries from attachment to addrbook all at once ",
	     added);
    switch(want_to(prompt, 'n', 'x', NO_HELP, WT_NORM)){
      case 'y':
	if(take_without_edit(current, added, command_line, NULL, "take") >= 0)
	  return(0);		/* all taken care of */
	else
	  return(-1);		/* problem */
	  
      case 'x':
	cmd_cancelled("Take");
	return(-1);		/* problem */
	  
      default:
	return(1);		/* proceed */
    }
}


int
take_without_edit(TA_S *ta_list, int num_in_list, int command_line, TA_STATE_S **tas, char *cmd)
{
    PerAddrBook   *pab_dst;
    SAVE_STATE_S   state;  /* For saving state of addrbooks temporarily */
    int            rc, total_to_copy;
    int		   how_many_dups = 0, how_many_to_copy = 0, skip_dups = 0;
    int		   ret = 0;
    int		   err = 0, need_write = 0, we_cancel = 0;
    adrbk_cntr_t   new_entry_num;
    char           warn[2][MAX_NICKNAME+1];
    char           tmp[200];
    TA_S          *current;
    SWOOP_S       *swoop_list = NULL, *sw;

    dprint((2, "\n - take_without_edit(%d) - \n",
	       num_in_list));

    /* move to beginning of the list */
    if(ta_list)
      while(ta_list->prev)
	ta_list = ta_list->prev;

    pab_dst = setup_for_addrbook_add(&state, command_line, cmd);
    if(!pab_dst)
      goto get_out;
    
    swoop_list = (SWOOP_S *)fs_get((num_in_list+1) * sizeof(SWOOP_S));
    memset((void *)swoop_list, 0, (num_in_list+1) * sizeof(SWOOP_S));
    sw = swoop_list;

    /*
     * Look through all the vcards for those with nicknames already
     * existing in the destination abook (dups) and build a list of
     * entries to be acted on.
     */
    for(current = ta_list; current; current = current->next){
	adrbk_cntr_t dst_enum;

	if(current->skip_it)
	  continue;
	
	/* check to see if this nickname already exists in the dest abook */
	if(current->nickname && current->nickname[0]){
	    AdrBk_Entry *abe;

	    current->checked = 0;
	    abe = adrbk_lookup_by_nick(pab_dst->address_book,
				       current->nickname, &dst_enum);
	    /*
	     * This nickname already exists.
	     */
	    if(abe){
		sw->dup = 1;
		sw->dst_enum = dst_enum;
		if(how_many_dups < 2){
		  strncpy(warn[how_many_dups], current->nickname, MAX_NICKNAME);
		  warn[how_many_dups][MAX_NICKNAME] = '\0';
		}
		
		how_many_dups++;
	    }
	}

	sw->ta = current;
	sw++;
	how_many_to_copy++;
    }

    /*
     * If there are some nicknames which already exist in the selected
     * abook, ask user what to do.
     */
    if(how_many_dups > 0){
	if(how_many_dups == 1){
	    if(how_many_to_copy == 1 && num_in_list == 1){
		ret = 'T';  /* use Take */
		if(tas && *tas){
		    (*tas)->state = state;
		    (*tas)->pab   = pab_dst;
		}

		goto get_out;
	    }
	    else{
		snprintf(tmp, sizeof(tmp),
		        "Entry with nickname \"%.*s\" already exists, replace ",
		        sizeof(tmp)-50, warn[0]);
	    }
	}
	else if(how_many_dups == 2)
	  snprintf(tmp, sizeof(tmp),
		  "Nicknames \"%.*s\" and \"%.*s\" already exist, replace ",
		  (sizeof(tmp)-50)/2, warn[0], (sizeof(tmp)-50)/2, warn[1]);
	else
	  snprintf(tmp, sizeof(tmp), "%d of the nicknames already exist, replace ",
		  how_many_dups);
	
	switch(want_to(tmp, 'n', 'x', h_ab_copy_dups, WT_NORM)){
	  case 'n':
	    skip_dups++;
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
     * the correct entries. In order to do that we'll sort the swoop_list
     * to give us a safe order.
     */
    if(!skip_dups && how_many_dups > 1)
      qsort((qsort_t *)swoop_list, (size_t)num_in_list, sizeof(*swoop_list),
	    cmp_swoop_list);

    we_cancel = busy_cue("Saving addrbook entries", NULL, 0);
    total_to_copy = how_many_to_copy - (skip_dups ? how_many_dups : 0);

    /*
     * Add the list of entries to the destination abook.
     */
    for(sw = swoop_list; sw && sw->ta; sw++){
	Tag  tag;
	char abuf[MAX_ADDRESS + 1];
	int  count_of_addrs;
	
	if(skip_dups && sw->dup)
	  continue;

	/*
	 * Delete existing dups and replace them.
	 */
	if(sw->dup){

	    /* delete the existing entry */
	    rc = 0;
	    if(adrbk_delete(pab_dst->address_book,
			    (a_c_arg_t)sw->dst_enum, 1, 0, 0, 0) == 0){
		need_write++;
	    }
	    else{
		q_status_message2(SM_ORDER | SM_DING, 3, 5,
				 "Error replacing entry in %.200s: %.200s",
				 pab_dst->abnick,
				 error_description(errno));
		err++;
		goto get_out;
	    }
	}

	/*
	 * We need to count the number of addresses in this entry in order
	 * to tell the adrbk routines if it is a List or a Single, and in
	 * order to pass the right stuff to be added.
	 */
	count_of_addrs = count_addrs(sw->ta->addr);
	tag = (count_of_addrs > 1) ? List : Single;
	if(tag == Single){
	    if(sw->ta->addr->mailbox && sw->ta->addr->mailbox[0]){
		char *scratch, *p, *t, *u;
		unsigned long l;
		RFC822BUFFER rbuf;
		size_t es;

		es = est_size(sw->ta->addr);
		scratch = (char *) fs_get(es * sizeof(char));
		scratch[0] = '\0';
		rbuf.f   = dummy_soutr;
		rbuf.s   = NULL;
		rbuf.beg = scratch;
		rbuf.cur = scratch;
		rbuf.end = scratch+es-1;
		rfc822_output_address_list(&rbuf, sw->ta->addr, 0L, NULL);
		*rbuf.cur = '\0';
		if((p = srchstr(scratch, "@" RAWFIELD)) != NULL){
		  for(t = p; ; t--)
		    if(*t == '&'){		/* find "leading" token */
			*t++ = ' ';		/* replace token */
			*p = '\0';		/* tie off string */
			u = (char *)rfc822_base64((unsigned char *)t,
						  (unsigned long)strlen(t), &l);
			*p = '@';		/* restore 'p' */
			rplstr(p, es-(p-scratch), 12, "");	/* clear special token */
			rplstr(t, es-(t-scratch), strlen(t), u);  /* Null u is handled */
			if(u)
			  fs_give((void **)&u);
		    }
		    else if(t == scratch)
		      break;
		}

		strncpy(abuf, scratch, sizeof(abuf)-1);
		abuf[sizeof(abuf)-1] = '\0';

		if(scratch)
		  fs_give((void **)&scratch);
	    }
	    else
	      abuf[0] = '\0';
	}
	  

	/*
	 * Now we have a clean slate to work with.
	 */
	if(total_to_copy <= 1)
	  rc = adrbk_add(pab_dst->address_book,
		         NO_NEXT,
		         sw->ta->nickname,
		         sw->ta->fullname,
		         tag == Single ? abuf : NULL,
		         sw->ta->fcc,
		         sw->ta->comment,
		         tag,
		         &new_entry_num,
		         (int *)NULL,
		         0,
		         0,
		         0);
	else
	  rc = adrbk_append(pab_dst->address_book,
		            sw->ta->nickname,
		            sw->ta->fullname,
		            tag == Single ? abuf : NULL,
		            sw->ta->fcc,
		            sw->ta->comment,
		            tag,
			    &new_entry_num);

	if(rc == 0)
	  need_write++;
	
	/*
	 * If the entry we copied is a list, we also have to add
	 * the list members to the copy.
	 */
	if(rc == 0 && tag == List){
	    int i, save_sort_rule;
	    ADDRESS *a, *save_next;
	    char **list;

	    list = (char **)fs_get((count_of_addrs + 1) * sizeof(char *));
	    memset((void *)list, 0, (count_of_addrs+1) * sizeof(char *));
	    i = 0;
	    for(a = sw->ta->addr; a; a = a->next){
		save_next = a->next;
		a->next = NULL;

		if(a->mailbox && a->mailbox[0]){
		    char *scratch, *p, *t, *u;
		    unsigned long l;
		    RFC822BUFFER rbuf;
		    size_t es;

		    es = est_size(a);
		    scratch = (char *) fs_get(es * sizeof(char));
		    scratch[0] = '\0';
		    rbuf.f   = dummy_soutr;
		    rbuf.s   = NULL;
		    rbuf.beg = scratch;
		    rbuf.cur = scratch;
		    rbuf.end = scratch+es-1;
		    rfc822_output_address_list(&rbuf, a, 0L, NULL);
		    *rbuf.cur = '\0';
		    if((p = srchstr(scratch, "@" RAWFIELD)) != NULL){
		      for(t = p; ; t--)
			if(*t == '&'){		/* find "leading" token */
			    *t++ = ' ';		/* replace token */
			    *p = '\0';		/* tie off string */
			    u = (char *)rfc822_base64((unsigned char *)t,
						      (unsigned long)strlen(t), &l);
			    *p = '@';		/* restore 'p' */
			    rplstr(p, es-(p-scratch), 12, "");	/* clear special token */
			    rplstr(t, es-(t-scratch), strlen(t), u);  /* Null u is handled */
			    if(u)
			      fs_give((void **)&u);
			}
			else if(t == scratch)
			  break;
		    }

		    strncpy(abuf, scratch, sizeof(abuf)-1);
		    abuf[sizeof(abuf)-1] = '\0';

		    if(scratch)
		      fs_give((void **)&scratch);
		}
		else
		  abuf[0] = '\0';

		list[i++] = cpystr(abuf);
		a->next = save_next;
	    }

	    /*
	     * We want it to copy the list in the exact order
	     * without sorting it.
	     */
	    save_sort_rule = pab_dst->address_book->sort_rule;
	    pab_dst->address_book->sort_rule = AB_SORT_RULE_NONE;

	    rc = adrbk_nlistadd(pab_dst->address_book,
				(a_c_arg_t)new_entry_num, NULL, NULL,
				list, 0, 0, 0);

	    pab_dst->address_book->sort_rule = save_sort_rule;
	    free_list_array(&list);
	}

	if(rc != 0){
	    q_status_message1(SM_ORDER | SM_DING, 3, 5,
			      "Error saving: %.200s",
			      error_description(errno));
	    err++;
	    goto get_out;
	}
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

    if(!ret)
      restore_state(&state);

    if(swoop_list)
      fs_give((void **)&swoop_list);

    ps_global->mangled_footer = 1;

    if(err){
	char capcmd[50];

	ret = -1;
	snprintf(capcmd, sizeof(capcmd),
		"%c%.*s",
		islower((unsigned char)(*cmd)) ? toupper((unsigned char)*cmd)
					       : *cmd,
		sizeof(capcmd)-2, cmd+1);
	if(need_write)
	  q_status_message1(SM_ORDER | SM_DING, 3, 4,
			   "%.200s only partially completed", capcmd);
	else
	  cmd_cancelled(capcmd);
    }
    else if(ret != 'T' && total_to_copy > 0){

	ret = 1;
	snprintf(tmp, sizeof(tmp), "Saved %d %s to \"%.*s\"",
		total_to_copy,
		(total_to_copy > 1) ? "entries" : "entry",
		sizeof(tmp)-30, pab_dst->abnick);
	q_status_message(SM_ORDER, 4, 4, tmp);
    }

    return(ret);
}


/*
 * Special case interface to allow a more interactive Save in the case where
 * the user seems to be wanting to save an exact copy of an existing entry.
 * For example, they might be trying to save a copy of a list with the intention
 * of changing it a little bit. The regular save doesn't allow this, since no
 * editing takes place, but this version plops them into the address book
 * editor.
 */
void
take_this_one_entry(struct pine *ps, TA_STATE_S **tasp, AdrBk *abook, long int cur_line)
{
    AdrBk_Entry *abe;
    AddrScrn_Disp *dl;
    char        *fcc = NULL, *comment = NULL, *fullname = NULL,
		*nickname = NULL;
    ADDRESS     *addr;
    int          how_many_selected;
    TA_S        *current = NULL;

    dl = dlist(cur_line);
    abe = ae(cur_line);
    if(!abe){
	q_status_message(SM_ORDER, 0, 4, _("Nothing to save, cancelled"));
	return;
    }

    if(dl->type == ListHead || dl->type == Simple){
	fcc      = (abe->fcc && abe->fcc[0]) ? abe->fcc : NULL;
	comment  = (abe->extra && abe->extra[0]) ? abe->extra : NULL;
	fullname = (abe->fullname && abe->fullname[0]) ? abe->fullname : NULL;
	nickname = (abe->nickname && abe->nickname[0]) ? abe->nickname : NULL;
    }

    addr = abe_to_address(abe, dl, abook, &how_many_selected);
    if(!addr){
	addr = mail_newaddr();
	addr->host = cpystr("");
	addr->mailbox = cpystr("");
    }

    switch(abe->tag){
      case Single:
#ifdef	ENABLE_LDAP
	/*
	 * Special case. When user is saving an entry with a runtime
	 * ldap lookup address, they may be doing it because the lookup
	 * has become stale. Give them a way to get the old address out
	 * of the lookup entry so they can save that, instead.
	 */
	if(!addr->personal && !strncmp(addr->mailbox, RUN_LDAP, LEN_RL)){
	    LDAP_SERV_S *info = NULL;
	    int i = 'l';
	    static ESCKEY_S backup_or_ldap[] = {
		{'b', 'b', "B", N_("Backup")},
		{'l', 'l', "L", N_("LDAP")},
		{-1, 0, NULL, NULL}};

	    info = break_up_ldap_server(addr->mailbox + LEN_RL);
	    if(info && info->mail && *info->mail)
	      i = radio_buttons(_("Copy backup address or retain LDAP search criteria ? "),
				-FOOTER_ROWS(ps_global), backup_or_ldap,
				'b', 'x',
				h_ab_backup_or_ldap, RB_NORM);

	    if(i == 'b'){
		ADDRESS *a = NULL;

		rfc822_parse_adrlist(&a, info->mail, fakedomain);

		if(a){
		    if(addr->mailbox)
		      fs_give((void **)&addr->mailbox);
		    if(addr->host)
		      fs_give((void **)&addr->host);

		    addr->mailbox = a->mailbox;
		    a->mailbox = NULL;
		    addr->host = a->host;
		    a->host = NULL;
		    mail_free_address(&a);
		}
	    }

	    if(info)
	      free_ldap_server_info(&info);
	}
#endif	/* ENABLE_LDAP */
	current = fill_in_ta(&current, addr, 0, (char *)NULL);
	break;

      case List:
	/*
	 * The empty string for the last argument is significant. Fill_in_ta
	 * copies the whole adrlist into a single TA if there is a print
	 * string.
	 */
	if(dl->type == ListHead)
	  current = fill_in_ta(&current, addr, 1, "");
	else
	  current = fill_in_ta(&current, addr, 0, (char *)NULL);

	break;
      
      default:
	dprint((1,
		"default case in take_this_one_entry, shouldn't happen\n"));
	return;
    } 

    if(current->strvalue && !strcmp(current->strvalue, "@")){
	fs_give((void **)&current->strvalue);
	if(fullname && fullname[0])
	  current->strvalue = cpystr(fullname);
	else if(nickname && nickname[0])
	  current->strvalue = cpystr(nickname);
	else
	  current->strvalue = cpystr("?");

	convert_possibly_encoded_str_to_utf8(&current->strvalue);
    }

    if(addr)
      mail_free_address(&addr);

    if(current){
	current = first_sel_taline(current);
	if(fullname && *fullname){
	    current->fullname = cpystr(fullname);
	    convert_possibly_encoded_str_to_utf8(&current->fullname);
	}

	if(fcc && *fcc){
	    current->fcc = cpystr(fcc);
	    convert_possibly_encoded_str_to_utf8(&current->fcc);
	}

	if(comment && *comment){
	    current->comment = cpystr(comment);
	    convert_possibly_encoded_str_to_utf8(&current->comment);
	}

	if(nickname && *nickname){
	    current->nickname = cpystr(nickname);
	    convert_possibly_encoded_str_to_utf8(&current->nickname);
	}

	takeaddr_bypass(ps, current, tasp);
    }
    else
      q_status_message(SM_ORDER, 0, 4, _("Nothing to save, cancelled"));

    free_talines(&current);
}


/*
 * Execute command to save addresses out of vcard attachment.
 */
void
save_vcard_att(struct pine *ps, int qline, long int msgno, ATTACH_S *a)
{
    int       how_many_selected, j;
    TA_S     *current = NULL;
    TA_STATE_S tas, *tasp;


    dprint((2, "\n - saving vcard attachment - \n"));

    j = radio_buttons(_("Save to address book or Export to filesystem ? "),
		      qline, save_or_export, 's', 'x',
		      h_ab_save_exp, RB_NORM|RB_SEQ_SENSITIVE);
    
    switch(j){
      case 'x':
	q_status_message(SM_INFO, 0, 2, _("Address book save cancelled"));
	return;

      case 'e':
	export_vcard_att(ps, qline, msgno, a);
	return;

      case 's':
	break;

      default:
        q_status_message(SM_ORDER, 3, 3, "can't happen in save_vcard_att");
	return;
    }

    dprint((2, "\n - saving attachment into address book - \n"));
    ps->mangled_footer = 1;
    current = NULL;
    how_many_selected = process_vcard_atts(ps->mail_stream, msgno, NULL,
					   a->body, a->number, &current);
    if(how_many_selected > 0){
	tas.pab = NULL;
	tasp    = &tas;
	if(how_many_selected == 1){
	    takeaddr_bypass(ps, current, NULL);
	}
	else if(take_without_edit(current, how_many_selected, qline,
			     &tasp, "save") == 'T'){
	    /*
	     * Eliminate dups.
	     */
	    how_many_selected -=
			eliminate_dups_but_not_us(first_sel_taline(current));

	    (void)takeaddr_screen(ps, current, how_many_selected, SingleMode,
				  &tasp, _("save"));
	    
	    /*
	     * If takeaddr_screen or its children didn't do this for us,
	     * we do it here.
	     */
	    if(tas.pab)
	      restore_state(&(tas.state));
	}
    }
    else if(how_many_selected == 0)
      q_status_message(SM_ORDER, 0, 3,
		       _("Save cancelled: no entries in attachment"));

    free_talines(&current);
}


/*
 * Execute command to export vcard attachment.
 */
void
export_vcard_att(struct pine *ps, int qline, long int msgno, ATTACH_S *a)
{
    int       how_many_selected, i;
    TA_S     *current;
    STORE_S  *srcstore = NULL;
    SourceType srctype;
    static ESCKEY_S vcard_or_addresses[] = {
	{'a', 'a', "A", N_("Address List")},
	{'v', 'v', "V", N_("VCard")},
	{-1, 0, NULL, NULL}};

    if(ps->restricted){
	q_status_message(SM_ORDER, 0, 3,
	    "Alpine demo can't export addresses to files");
	return;
    }

    dprint((2, "\n - exporting vcard attachment - \n"));

    i = radio_buttons(_("Export list of addresses or vCard text ? "),
		      qline, vcard_or_addresses, 'a', 'x',
		      h_ab_export_vcard, RB_NORM|RB_SEQ_SENSITIVE);

    switch(i){
      case 'x':
	q_status_message(SM_INFO, 0, 2, _("Address book export cancelled"));
	return;
    
      case 'a':
	break;

      case 'v':
	write_attachment(qline, msgno, a, "EXPORT");
	return;

      default:
	q_status_message(SM_ORDER, 3, 3, _("can't happen in export_vcard_att"));
	return;
    }

    ps->mangled_footer = 1;
    current = NULL;
    how_many_selected = process_vcard_atts(ps->mail_stream, msgno, NULL,
					   a->body, a->number, &current);
    /*
     * Run through all of the list and run through
     * the addresses in each ta->addr, writing them into a storage object.
     * Then export to filesystem.
     */
    srctype = CharStar;
    if(how_many_selected > 0 &&
       (srcstore = so_get(srctype, NULL, EDIT_ACCESS)) != NULL){
	ADDRESS *aa, *bb;
	int are_some = 0;

	for(current = first_taline(current);
	    current;
	    current = next_taline(current)){

	    for(aa = current->addr; aa; aa = aa->next){
		bb = aa->next;
		aa->next = NULL;
		so_puts(srcstore, addr_list_string(aa, NULL, 0));
		are_some++;
		so_puts(srcstore, "\n");
		aa->next = bb;
	    }
	}

	if(are_some)
	  simple_export(ps, so_text(srcstore), srctype, "addresses", NULL);
	else
	  q_status_message(SM_ORDER, 0, 3, _("No addresses to export"));
	  
	so_give(&srcstore);
    }
    else{
	if(how_many_selected == 0)
	  q_status_message(SM_ORDER, 0, 3, _("Nothing to export"));
	else
	  q_status_message(SM_ORDER,0,2, _("Error allocating space"));
    }
}


void
take_to_export(struct pine *ps, LINES_TO_TAKE *lines_to_take)
{
    CONF_S        *ctmp = NULL, *first_line = NULL;
    OPT_SCREEN_S   screen;
    LINES_TO_TAKE *li;
    char          *help_title = _("HELP FOR TAKE EXPORT SCREEN");
    char          *p;
    ScreenMode     listmode = SingleMode;

    for(li = lines_to_take; li; li = li->next){

	new_confline(&ctmp);
	ctmp->flags        |= CF_STARTITEM;
	if(li->flags & LT_NOSELECT)
	  ctmp->flags      |= CF_NOSELECT;
	else if(!first_line)
	  first_line = ctmp;

	p = li->printval ? li->printval : "";

	if(ctmp->flags & CF_NOSELECT)
	  ctmp->value = cpystr(p);
	else{
	    size_t l;

	    /* 5 is for "[X]  " */
	    l = strlen(p)+5;
	    ctmp->value = (char *)fs_get((l+1) * sizeof(char));
	    snprintf(ctmp->value, l+1, "    %s", p);
	    ctmp->value[l] = '\0';
	}

	/* this points to data, it doesn't have its own copy */
	ctmp->d.t.exportval = li->exportval ? li->exportval : NULL;
	ctmp->d.t.selected  = 0;
	ctmp->d.t.listmode  = &listmode;

	ctmp->tool          = take_export_tool;
	ctmp->help_title    = help_title;
	ctmp->help          = h_takeexport_screen;
	ctmp->keymenu       = &take_export_keymenu_sm;
    }

    if(!first_line)
      q_status_message(SM_ORDER, 3, 3, _("No lines to export"));
    else{
	memset(&screen, 0, sizeof(screen));
	conf_scroll_screen(ps, &screen, first_line, _("Take Export"), NULL, 0);
    }
}


int
take_export_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned int flags)
{
    CONF_S    *ctmp;
    int        retval = 0;
    int        some_selected = 0, something_to_export = 0;
    SourceType srctype;
    STORE_S   *srcstore = NULL;
    char      *prompt_msg;

    switch(cmd){
      case MC_TAKE :
	srctype = CharStar;
	if((srcstore = so_get(srctype, NULL, EDIT_ACCESS)) != NULL){
	    if(*(*cl)->d.t.listmode == SingleMode){
		some_selected++;
		if((*cl)->d.t.exportval && (*cl)->d.t.exportval[0]){
		    so_puts(srcstore, (*cl)->d.t.exportval);
		    so_puts(srcstore, "\n");
		    something_to_export++;
		    prompt_msg = "selection";
		}
	    }
	    else{
		/* go to first line */
		for(ctmp = *cl; prev_confline(ctmp); ctmp = prev_confline(ctmp))
		  ;
		
		for(; ctmp; ctmp = next_confline(ctmp))
		  if(!(ctmp->flags & CF_NOSELECT) && ctmp->d.t.selected){
		      some_selected++;
		      if(ctmp->d.t.exportval && ctmp->d.t.exportval[0]){
			  so_puts(srcstore, ctmp->d.t.exportval);
			  so_puts(srcstore, "\n");
			  something_to_export++;
			  prompt_msg = "selections";
		      }
		  }
	    }
	}
	  
	if(!srcstore)
	  q_status_message(SM_ORDER, 0, 3, _("Error allocating space"));
	else if(something_to_export)
	  simple_export(ps, so_text(srcstore), srctype, prompt_msg, NULL);
	else if(!some_selected && *(*cl)->d.t.listmode == ListMode)
	  q_status_message(SM_ORDER, 0, 3, _("Use \"X\" to mark selections"));
	else
	  q_status_message(SM_ORDER, 0, 3, _("Nothing to export"));

	if(srcstore)
	  so_give(&srcstore);

	break;

      case MC_LISTMODE :
        if(*(*cl)->d.t.listmode == SingleMode){
	    /*
	     * UnHide the checkboxes
	     */

	    *(*cl)->d.t.listmode = ListMode;

	    /* go to first line */
	    for(ctmp = *cl; prev_confline(ctmp); ctmp = prev_confline(ctmp))
	      ;
	    
	    for(; ctmp; ctmp = next_confline(ctmp))
	      if(!(ctmp->flags & CF_NOSELECT) && ctmp->value){
		  ctmp->value[0] = '[';
		  ctmp->value[1] = ctmp->d.t.selected ? 'X' : SPACE;
		  ctmp->value[2] = ']';
		  ctmp->keymenu  = &take_export_keymenu_lm;
	      }
	}
	else{
	    /*
	     * Hide the checkboxes
	     */

	    *(*cl)->d.t.listmode = SingleMode;

	    /* go to first line */
	    for(ctmp = *cl; prev_confline(ctmp); ctmp = prev_confline(ctmp))
	      ;
	    
	    for(; ctmp; ctmp = next_confline(ctmp))
	      if(!(ctmp->flags & CF_NOSELECT) && ctmp->value){
		  ctmp->value[0] = ctmp->value[1] = ctmp->value[2] = SPACE;
		  ctmp->keymenu  = &take_export_keymenu_sm;
	      }
	}

	ps->mangled_body = ps->mangled_footer = 1;
	break;

      case MC_CHOICE :
	if((*cl)->value[1] == 'X'){
	    (*cl)->d.t.selected = 0;
	    (*cl)->value[1] = SPACE;
	}
	else{
	    (*cl)->d.t.selected = 1;
	    (*cl)->value[1] = 'X';
	}

	ps->mangled_body = 1;
        break;

      case MC_SELALL :
	/* go to first line */
	for(ctmp = *cl; prev_confline(ctmp); ctmp = prev_confline(ctmp))
	  ;
	
	for(; ctmp; ctmp = next_confline(ctmp)){
	    if(!(ctmp->flags & CF_NOSELECT) && ctmp->value){
		ctmp->d.t.selected = 1;
		if(ctmp->value)
		  ctmp->value[1] = 'X';
	    }
	}

	ps->mangled_body = 1;
        break;

      case MC_UNSELALL :
	/* go to first line */
	for(ctmp = *cl; prev_confline(ctmp); ctmp = prev_confline(ctmp))
	  ;
	
	for(; ctmp; ctmp = next_confline(ctmp)){
	    if(!(ctmp->flags & CF_NOSELECT) && ctmp->value){
		ctmp->d.t.selected = 0;
		if(ctmp->value)
		  ctmp->value[1] = SPACE;
	    }
	}

	ps->mangled_body = 1;
        break;

      case MC_EXIT :
        retval = simple_exit_cmd(flags);
	break;

      default:
	retval = -1;
	break;
    }

    return(retval);
}

#ifdef	ENABLE_LDAP
/*
 * Save an LDAP directory entry somewhere
 *
 * Args: ps        -- pine struct
 *       e         -- the entry to save
 *       save      -- If this is set, then bypass the question about whether
 *                    to save or export and just do the save.
 */
void
save_ldap_entry(struct pine *ps, LDAP_CHOOSE_S *e, int save)
{
    char  *fullname = NULL,
	  *address = NULL,
	  *first = NULL,
	  *last = NULL,
	  *comment = NULL;
    char **cn = NULL,
	 **mail = NULL,
	 **sn = NULL,
	 **givenname = NULL,
	 **title = NULL,
	 **telephone = NULL,
	 **elecmail = NULL,
	 **note = NULL,
	 **ll;
    int    j,
	   export = 0;


    dprint((2, "\n - save_ldap_entry - \n"));

    if(e){
	char       *a;
	BerElement *ber;

	for(a = ldap_first_attribute(e->ld, e->selected_entry, &ber);
	    a != NULL;
	    a = ldap_next_attribute(e->ld, e->selected_entry, ber)){

	    dprint((9, " %s", a ? a : "?"));
	    if(strcmp(a, e->info_used->cnattr) == 0){
		if(!cn)
		  cn = ldap_get_values(e->ld, e->selected_entry, a);
	    }
	    else if(strcmp(a, e->info_used->mailattr) == 0){
		if(!mail)
		  mail = ldap_get_values(e->ld, e->selected_entry, a);
	    }
	    else if(strcmp(a, "electronicmail") == 0){
		if(!elecmail)
		  elecmail = ldap_get_values(e->ld, e->selected_entry, a);
	    }
	    else if(strcmp(a, "comment") == 0){
		if(!note)
		  note = ldap_get_values(e->ld, e->selected_entry, a);
	    }
	    else if(strcmp(a, e->info_used->snattr) == 0){
		if(!sn)
		  sn = ldap_get_values(e->ld, e->selected_entry, a);
	    }
	    else if(strcmp(a, e->info_used->gnattr) == 0){
		if(!givenname)
		  givenname = ldap_get_values(e->ld, e->selected_entry, a);
	    }
	    else if(strcmp(a, "telephonenumber") == 0){
		if(!telephone)
		  telephone = ldap_get_values(e->ld, e->selected_entry, a);
	    }
	    else if(strcmp(a, "title") == 0){
		if(!title)
		  title = ldap_get_values(e->ld, e->selected_entry, a);
	    }
	}

	if(!save){
	    j = radio_buttons(_("Save to address book or Export to filesystem ? "),
			      -FOOTER_ROWS(ps), save_or_export, 's', 'x',
			      h_ab_save_exp, RB_NORM);
	    
	    switch(j){
	      case 'x':
		q_status_message(SM_INFO, 0, 2, _("Address book save cancelled"));
		break;

	      case 'e':
		export++;
		break;

	      case 's':
		save++;
		break;

	      default:
		q_status_message(SM_ORDER, 3, 3,
				 "can't happen in save_ldap_ent");
		break;
	    }
	}
    }

    if(elecmail){
	if(elecmail[0] && elecmail[0][0] && !mail)
	  mail = elecmail;
	else
	  ldap_value_free(elecmail);

	elecmail = NULL;
    }

    if(save){				/* save into the address book */
	ADDRESS *addr;
	char    *d,
		*fakedomain = "@";
	size_t   len;
	char   **cm;
	int      how_many_selected = 0;

	
	if(cn && cn[0] && cn[0][0])
	  fullname = cpystr(cn[0]);
	if(sn && sn[0] && sn[0][0])
	  last = cpystr(sn[0]);
	if(givenname && givenname[0] && givenname[0][0])
	  first = cpystr(givenname[0]);

	if(note && note[0] && note[0][0])
	  cm = note;
	else
	  cm = title;

	for(ll = cm, len = 0; ll && *ll; ll++)
	  len += strlen(*ll) + 2;

	if(len){
	    comment = (char *)fs_get(len * sizeof(char));
	    d = comment;
	    ll = cm;
	    while(*ll){
		sstrncpy(&d, *ll, len-(d-comment));
		ll++;
		if(*ll)
		  sstrncpy(&d, "; ", len-(d-comment));
	    }

	    comment[len-1] = '\0';
	}

	for(ll = mail, len = 0; ll && *ll; ll++)
	  len += strlen(*ll) + 1;
	
	/* paste the email addresses together */
	if(len){
	    address = (char *)fs_get(len * sizeof(char));
	    d = address;
	    ll = mail;
	    while(*ll){
		sstrncpy(&d, *ll, len-(d-address));
		ll++;
		if(*ll)
		  sstrncpy(&d, ", ", len-(d-address));
	    }

	    address[len-1] = '\0';
	}

	addr = NULL;
	if(address)
	  rfc822_parse_adrlist(&addr, address, fakedomain);

	if(addr && fullname && !(first && last)){
	    if(addr->personal)
	      fs_give((void **)&addr->personal);
	    
	    addr->personal = cpystr(fullname);
	}

	if(addr && e->serv && *e->serv){	/* save by reference */
	    char *dn, *edn = NULL;

	    dn = ldap_get_dn(e->ld, e->selected_entry);
	    if(dn){
		edn = add_backslash_escapes(dn);
		our_ldap_dn_memfree(dn);
	    }

	    if(e->serv && *e->serv && edn && *edn){
		char  buf[MAILTMPLEN+1];
		char *backup_mail = NULL;

		how_many_selected++;

		if(addr && addr->mailbox && addr->host){
		    strncpy(buf, addr->mailbox, sizeof(buf)-2),
		    buf[sizeof(buf)-2] = '\0';
		    strncat(buf, "@", sizeof(buf)-1-strlen(buf));
		    strncat(buf, addr->host, sizeof(buf)-1-strlen(buf));
		    buf[sizeof(buf)-1] = '\0';
		    backup_mail = cpystr(buf);
		}

		/*
		 * We only need one addr which we will use to hold the
		 * pointer to the query.
		 */
		if(addr->next)
		  mail_free_address(&addr->next);

		if(addr->mailbox)
		  fs_give((void **)&addr->mailbox);
		if(addr->host)
		  fs_give((void **)&addr->host);
		if(addr->personal)
		  fs_give((void **)&addr->personal);

		snprintf(buf, sizeof(buf),
	       "%s%s /base=%s/scope=base/cust=(objectclass=*)%s%s",
			RUN_LDAP,
			e->serv,
			edn,
			backup_mail ? "/mail=" : "",
			backup_mail ? backup_mail : "");
		buf[sizeof(buf)-1] = '\0';

		if(backup_mail)
		  fs_give((void **)&backup_mail);

		/*
		 * Put the search parameters in mailbox and put @ in
		 * host so that expand_address accepts it as an unqualified
		 * address and doesn't try to add localdomain.
		 */
		addr->mailbox = cpystr(buf);
		addr->host    = cpystr("@");
	    }

	    if(edn)
	      fs_give((void **)&edn);
	}
	else{					/* save by value */
	    how_many_selected++;
	    if(!addr)
	      addr = mail_newaddr();
	}

	if(how_many_selected > 0){
	    TA_S  *current = NULL;

	    /*
	     * The empty string for the last argument is significant.
	     * Fill_in_ta copies the whole adrlist into a single TA if
	     * there is a print string.
	     */
	    current = fill_in_ta(&current, addr, 1, "");
	    current = first_sel_taline(current);

	    if(first && last && current){
		char *p;
		size_t l;

		l = strlen(last) + 2 + strlen(first);
		p = (char *)fs_get((l+1) * sizeof(char));
		snprintf(p, l, "%s, %s", last, first);
		p[l] = '\0';
		current->fullname = p;
		convert_possibly_encoded_str_to_utf8(&current->fullname);
	    }

	    if(comment && current){
		current->comment = cpystr(comment);
		convert_possibly_encoded_str_to_utf8(&current->comment);
	    }
	    
	    /*
	     * We don't want the personal name to make it into the address
	     * field in an LDAP: query sort of address, so move it
	     * out of the addr.
	     */
	    if(e->serv && *e->serv && current && fullname){
		if(current->fullname)
		  fs_give((void **)&current->fullname);

		current->fullname = fullname;
		fullname = NULL;
		convert_possibly_encoded_str_to_utf8(&current->fullname);
	    }

	    mail_free_address(&addr);

	    if(current)
	      takeaddr_bypass(ps, current, NULL);
	    else
	      q_status_message(SM_ORDER, 0, 4, "Nothing to save");

	    free_talines(&current);
	}
	else
	  q_status_message(SM_ORDER, 0, 4, "Nothing to save");

    }
    else if(export){				/* export to filesystem */
	STORE_S *srcstore = NULL;
	SourceType srctype;
	static ESCKEY_S text_or_vcard[] = {
	    {'t', 't', "T", N_("Text")},
	    {'a', 'a', "A", N_("Addresses")},
	    {'v', 'v', "V", N_("VCard")},
	    {-1, 0, NULL, NULL}};

	j = radio_buttons(
		_("Export text of entry, address, or VCard format ? "),
		-FOOTER_ROWS(ps), text_or_vcard, 't', 'x',
		h_ldap_text_or_vcard, RB_NORM);
	
	switch(j){
	  case 'x':
	    q_status_message(SM_INFO, 0, 2, _("Address book export cancelled"));
	    break;
	
	  case 't':
	    srctype = CharStar;
	    if(!(srcstore = prep_ldap_for_viewing(ps, e, srctype, NULL)))
	      q_status_message(SM_ORDER, 0, 2, _("Error allocating space"));
	    else{
		(void)simple_export(ps_global, so_text(srcstore), srctype,
				    "text", NULL);
		so_give(&srcstore);
	    }

	    break;
	
	  case 'a':
	    if(mail && mail[0] && mail[0][0]){

		srctype = CharStar;
		if(!(srcstore = so_get(srctype, NULL, EDIT_ACCESS)))
		  q_status_message(SM_ORDER,0,2, _("Error allocating space"));
		else{
		    for(ll = mail; ll && *ll; ll++){
			so_puts(srcstore, *ll);
			so_puts(srcstore, "\n");
		    }

		    (void)simple_export(ps_global, so_text(srcstore),
					srctype, "addresses", NULL);
		    so_give(&srcstore);
		}
	    }

	    break;
	
	  case 'v':
	    srctype = CharStar;
	    if(!(srcstore = so_get(srctype, NULL, EDIT_ACCESS)))
	      q_status_message(SM_ORDER,0,2, _("Error allocating space"));
	    else{
		gf_io_t       pc;
		VCARD_INFO_S *vinfo;

		vinfo = (VCARD_INFO_S *)fs_get(sizeof(VCARD_INFO_S));
		memset((void *)vinfo, 0, sizeof(VCARD_INFO_S));

		if(cn && cn[0] && cn[0][0])
		  vinfo->fullname = copy_list_array(cn);

		if(note && note[0] && note[0][0])
		  vinfo->note = copy_list_array(note);

		if(title && title[0] && title[0][0])
		  vinfo->title = copy_list_array(title);

		if(telephone && telephone[0] && telephone[0][0])
		  vinfo->tel = copy_list_array(telephone);

		if(mail && mail[0] && mail[0][0])
		  vinfo->email = copy_list_array(mail);

		if(sn && sn[0] && sn[0][0])
		  vinfo->last = cpystr(sn[0]);

		if(givenname && givenname[0] && givenname[0][0])
		  vinfo->first = cpystr(givenname[0]);

		gf_set_so_writec(&pc, srcstore);

		write_single_vcard_entry(ps_global, pc, vinfo);

		free_vcard_info(&vinfo);

		(void)simple_export(ps_global, so_text(srcstore),
				    srctype, "vcard text", NULL);
		so_give(&srcstore);
	    }

	    break;
	
	  default:
	    q_status_message(SM_ORDER, 3, 3, "can't happen in text_or_vcard");
	    break;
	}
    }

    if(cn)
      ldap_value_free(cn);
    if(mail)
      ldap_value_free(mail);
    if(elecmail)
      ldap_value_free(elecmail);
    if(note)
      ldap_value_free(note);
    if(sn)
      ldap_value_free(sn);
    if(givenname)
      ldap_value_free(givenname);
    if(telephone)
      ldap_value_free(telephone);
    if(title)
      ldap_value_free(title);
    if(fullname)
      fs_give((void **)&fullname);
    if(address)
      fs_give((void **)&address);
    if(first)
      fs_give((void **)&first);
    if(last)
      fs_give((void **)&last);
    if(comment)
      fs_give((void **)&comment);
}
#endif	/* ENABLE_LDAP */


#ifdef _WINDOWS

int
ta_scroll_up(count)
     long count;
{
  if(count<0)
    return(ta_scroll_down(-count));
  else if (count){
    long i=count;
    TA_S *next_sel;

    while(i && ta_screen->top_line->next){
      if(ta_screen->top_line ==  ta_screen->current){
	if(next_sel = next_sel_taline(ta_screen->current)){
	  ta_screen->current = next_sel;
	  ta_screen->top_line = next_taline(ta_screen->top_line);
	  i--;
	}
	else i = 0;
      }
      else{
	ta_screen->top_line = next_taline(ta_screen->top_line);
	i--;
      }
    }
  }
  return(TRUE);
}

int
ta_scroll_down(count)
     long count;
{
  if(count < 0)
    return(ta_scroll_up(-count));
  else if (count){
    long i,dline;
    long page_size = ps_global->ttyo->screen_rows - 
                      FOOTER_ROWS(ps_global) - HEADER_ROWS(ps_global);
    TA_S *ctmp;
    TA_S *first = first_taline(ta_screen->top_line);

    i=count;
    dline=0;
    for(ctmp = ta_screen->top_line; 
	ctmp != ta_screen->current; ctmp = next_taline(ctmp))
      dline++;
    while(i && ta_screen->top_line != first){    
      ta_screen->top_line = pre_taline(ta_screen->top_line);
      i--;
      dline++;
      if(dline >= page_size){
	ctmp = pre_sel_taline(ta_screen->current);
	if(ctmp == NULL){
	  i = 0;
	  ta_screen->top_line = next_taline(ta_screen->top_line);
	}
	else {
	  for(; ctmp != ta_screen->current; 
	      ta_screen->current = pre_taline(ta_screen->current)) 
	    dline--;
	}
      }
    }
  }
  return(TRUE);
}

int ta_scroll_to_pos(line)
     long line;
{
  TA_S *ctmp;
  int dline;

  for(dline = 0, ctmp = first_taline(ta_screen->top_line); 
      ctmp != ta_screen->top_line; ctmp = next_taline(ctmp))
    dline++;

  if (!ctmp)
    dline = 1;

  return(ta_scroll_up(line - dline));
}

int
ta_scroll_callback (cmd, scroll_pos)
     int cmd;
    long scroll_pos;
{
    int paint = TRUE;
    long page_size = ps_global->ttyo->screen_rows - HEADER_ROWS(ps_global)
                       - FOOTER_ROWS(ps_global);

    switch (cmd) {
    case MSWIN_KEY_SCROLLUPLINE:
      paint = ta_scroll_down (scroll_pos);
      break;

    case MSWIN_KEY_SCROLLDOWNLINE:
      paint = ta_scroll_up (scroll_pos);
      break;

    case MSWIN_KEY_SCROLLUPPAGE:
      paint = ta_scroll_down (page_size);
      break;

    case MSWIN_KEY_SCROLLDOWNPAGE:
      paint = ta_scroll_up (page_size);
      break;

    case MSWIN_KEY_SCROLLTO:
      paint = ta_scroll_to_pos (scroll_pos);
      break;
    }

    if(paint)
      update_takeaddr_screen(ps_global, ta_screen->current, ta_screen, (Pos *)NULL);

    return(paint);
}

#endif /* _WINDOWS */
