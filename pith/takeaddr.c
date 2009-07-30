#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: takeaddr.c 1012 2008-03-26 00:44:22Z hubert@u.washington.edu $";
#endif

/*
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

/*====================================================================== 
    takeaddr.c
    Mostly support for Take Address command.
 ====*/


#include "../pith/headers.h"
#include "../pith/takeaddr.h"
#include "../pith/conf.h"
#include "../pith/bldaddr.h"
#include "../pith/adrbklib.h"
#include "../pith/copyaddr.h"
#include "../pith/addrstring.h"
#include "../pith/status.h"
#include "../pith/mailview.h"
#include "../pith/reply.h"
#include "../pith/url.h"
#include "../pith/mailpart.h"
#include "../pith/sequence.h"
#include "../pith/stream.h"
#include "../pith/busy.h"
#include "../pith/ablookup.h"
#include "../pith/list.h"


static char        *fakedomain = "@";
long                msgno_for_pico_callback;
BODY               *body_for_pico_callback = NULL;
ENVELOPE           *env_for_pico_callback = NULL;


/*
 * Previous selectable TA line.
 * skips over the elements with skip_it set
 */
TA_S *
pre_sel_taline(TA_S *current)
{
    TA_S *p;

    if(!current)
      return NULL;

    p = current->prev;
    while(p && p->skip_it)
      p = p->prev;
    
    return(p);
}


/*
 * Previous TA line, selectable or just printable.
 * skips over the elements with skip_it set
 */
TA_S *
pre_taline(TA_S *current)
{
    TA_S *p;

    if(!current)
      return NULL;

    p = current->prev;
    while(p && (p->skip_it && !p->print))
      p = p->prev;
    
    return(p);
}


/*
 * Next selectable TA line.
 * skips over the elements with skip_it set
 */
TA_S *
next_sel_taline(TA_S *current)
{
    TA_S *p;

    if(!current)
      return NULL;

    p = current->next;
    while(p && p->skip_it)
      p = p->next;
    
    return(p);
}


/*
 * Next TA line, including print only lines.
 * skips over the elements with skip_it set unless they are print lines
 */
TA_S *
next_taline(TA_S *current)
{
    TA_S *p;

    if(!current)
      return NULL;

    p = current->next;
    while(p && (p->skip_it && !p->print))
      p = p->next;
    
    return(p);
}


/*
 * Mark all of the addresses with a check.
 *
 * Args: f_line -- the first ta line
 *
 * Returns the number of lines checked.
 */
int
ta_mark_all(TA_S *f_line)
{
    TA_S *ctmp;
    int   how_many_selected = 0;

    for(ctmp = f_line; ctmp; ctmp = next_sel_taline(ctmp)){
	ctmp->checked = 1;
	how_many_selected++;
    }

    return(how_many_selected);
}


/*
 * Does the takeaddr list consist of a single address?
 *
 * Args: f_line -- the first ta line
 *
 * Returns 1 if only one address and 0 otherwise.
 */
int
is_talist_of_one(TA_S *f_line)
{
    if(f_line == NULL)
      return 0;

    /* there is at least one, see if there are two */
    if(next_sel_taline(f_line) != NULL)
      return 0;
    
    return 1;
}


/*
 * Turn off all of the check marks.
 *
 * Args: f_line -- the first ta line
 *
 * Returns the number of lines checked (0).
 */
int
ta_unmark_all(TA_S *f_line)
{
    TA_S *ctmp;

    for(ctmp = f_line; ctmp; ctmp = ctmp->next)
      ctmp->checked = 0;

    return 0;
}


/*
 * new_taline - create new TA_S, zero it out, and insert it after current.
 *                NOTE current gets set to the new TA_S, too!
 */
TA_S *
new_taline(TA_S **current)
{
    TA_S *p;

    p = (TA_S *)fs_get(sizeof(TA_S));
    memset((void *)p, 0, sizeof(TA_S));
    if(current){
	if(*current){
	    p->next	     = (*current)->next;
	    (*current)->next = p;
	    p->prev	     = *current;
	    if(p->next)
	      p->next->prev = p;
	}

	*current = p;
    }

    return(p);
}


void
free_taline(TA_S **p)
{
    if(p && *p){
	if((*p)->addr)
	  mail_free_address(&(*p)->addr);

	if((*p)->strvalue)
	  fs_give((void **)&(*p)->strvalue);

	if((*p)->nickname)
	  fs_give((void **)&(*p)->nickname);

	if((*p)->fullname)
	  fs_give((void **)&(*p)->fullname);

	if((*p)->fcc)
	  fs_give((void **)&(*p)->fcc);

	if((*p)->comment)
	  fs_give((void **)&(*p)->comment);

	if((*p)->prev)
	  (*p)->prev->next = (*p)->next;

	if((*p)->next)
	  (*p)->next->prev = (*p)->prev;

	fs_give((void **)p);
    }
}


void
free_talines(TA_S **ta_list)
{
    TA_S *current, *ctmp;

    if(ta_list && (current = (*ta_list))){
	while(current->prev)
	  current = current->prev;

	while(current){
	    ctmp = current->next;
	    free_taline(&current);
	    current = ctmp;
	}

	*ta_list = NULL;
    }
}


void
free_ltline(LINES_TO_TAKE **p)
{
    if(p && *p){
	if((*p)->printval)
	  fs_give((void **)&(*p)->printval);

	if((*p)->exportval)
	  fs_give((void **)&(*p)->exportval);

	if((*p)->prev)
	  (*p)->prev->next = (*p)->next;

	if((*p)->next)
	  (*p)->next->prev = (*p)->prev;

	fs_give((void **)p);
    }
}


void
free_ltlines(LINES_TO_TAKE **lt_list)
{
    LINES_TO_TAKE *current, *ctmp;

    if(lt_list && (current = (*lt_list))){
	while(current->prev)
	  current = current->prev;

	while(current){
	    ctmp = current->next;
	    free_ltline(&current);
	    current = ctmp;
	}

	*lt_list = NULL;
    }
}


/*
 * Return the first selectable TakeAddr line.
 *
 * Args: q -- any line in the list
 */
TA_S *
first_sel_taline(TA_S *q)
{
    TA_S *first;

    if(q == NULL)
      return NULL;

    first = NULL;
    /* back up to the head of the list */
    while(q){
	first = q;
	q = pre_sel_taline(q);
    }

    /*
     * If first->skip_it, that means we were already past the first
     * legitimate line, so we have to look in the other direction.
     */
    if(first->skip_it)
      first = next_sel_taline(first);
    
    return(first);
}


/*
 * Return the last selectable TakeAddr line.
 *
 * Args: q -- any line in the list
 */
TA_S *
last_sel_taline(TA_S *q)
{
    TA_S *last;

    if(q == NULL)
      return NULL;

    last = NULL;
    /* go to the end of the list */
    while(q){
	last = q;
	q = next_sel_taline(q);
    }

    /*
     * If last->skip_it, that means we were already past the last
     * legitimate line, so we have to look in the other direction.
     */
    if(last->skip_it)
      last = pre_sel_taline(last);
    
    return(last);
}


/*
 * Return the first TakeAddr line, selectable or just printable.
 *
 * Args: q -- any line in the list
 */
TA_S *
first_taline(TA_S *q)
{
    TA_S *first;

    if(q == NULL)
      return NULL;

    first = NULL;
    /* back up to the head of the list */
    while(q){
	first = q;
	q = pre_taline(q);
    }

    /*
     * If first->skip_it, that means we were already past the first
     * legitimate line, so we have to look in the other direction.
     */
    if(first->skip_it && !first->print)
      first = next_taline(first);
    
    return(first);
}


/*
 * Find the first TakeAddr line which is checked, beginning with the
 * passed in line.
 *
 * Args: head -- A passed in TakeAddr line, usually will be the first
 *
 * Result: returns a pointer to the first checked line.
 *         NULL if no such line
 */
TA_S *
first_checked(TA_S *head)
{
    TA_S *p;

    p = head;

    for(p = head; p; p = next_sel_taline(p))
      if(p->checked && !p->skip_it)
	break;

    return(p);
}


/*
 * Form a list of strings which are addresses to go in a list.
 * These are entries in a list, so can be full rfc822 addresses.
 * The strings are allocated here.
 *
 * Args: head -- A passed in TakeAddr line, usually will be the first
 *
 * Result: returns an allocated array of pointers to allocated strings
 */
char **
list_of_checked(TA_S *head)
{
    TA_S  *p;
    int    count;
    char **list, **cur;
    ADDRESS *a;

    /* first count them */
    for(p = head, count = 0; p; p = next_sel_taline(p)){
	if(p->checked && !p->skip_it){
	    if(p->frwrded){
		/*
		 * Remove fullname, fcc, comment, and nickname since not
		 * appropriate for list values.
		 */
		if(p->fullname)
		  fs_give((void **)&p->fullname);
		if(p->fcc)
		  fs_give((void **)&p->fcc);
		if(p->comment)
		  fs_give((void **)&p->comment);
		if(p->nickname)
		  fs_give((void **)&p->nickname);
	      
		for(a = p->addr; a; a = a->next)
		  count++;
	    }
	    else{
		/*
		 * Don't even attempt to include bogus addresses in
		 * the list.  If the user wants to get at those, they
		 * have to try taking only that single address.
		 */
		if(p->addr && p->addr->host && p->addr->host[0] == '.')
		  p->skip_it = 1;
		else
		  count++;
	    }
	}
    }
    
    /* allocate pointers */
    list = (char **)fs_get((count + 1) * sizeof(char *));
    memset((void *)list, 0, (count + 1) * sizeof(char *));

    cur = list;

    /* allocate and point to address strings */
    for(p = head; p; p = next_sel_taline(p)){
	if(p->checked && !p->skip_it){
	    if(p->frwrded)
	      for(a = p->addr; a; a = a->next){
		  ADDRESS *next_addr;
		  char    *bufp;
		  size_t   len;

		  next_addr = a->next;
		  a->next = NULL;
		  len = est_size(a);
		  bufp = (char *) fs_get(len * sizeof(char));
		  *cur++ = cpystr(addr_string(a, bufp, len));
		  a->next = next_addr;
		  fs_give((void **)&bufp);
	      }
	    else
	      *cur++ = cpystr(p->strvalue);
	}
    }

    return(list);
}

/* jpf: This used to be the guts of cmd_take_addr, but I made this 
 * function so I could generalize with WP.  It still has some display
 * stuff that we need make sure not to do when in WP mode.
 *
 * Set things up for when we take addresses
 *
 * Args: ps           -- pine state
 *       msgmap       -- the MessageMap
 *       ta_ret       -- the take addr lines
 *       selected_num -- the number of selectable addresses
 *       flags        -- takeaddr flags, like whether or not
 *                       we're doing aggs, and whether to do prompts
 *
 * Returns: -1 on "failure"
 */
int
set_up_takeaddr(int cmd, struct pine *ps, MSGNO_S *msgmap, TA_S **ta_ret,
		int *selected_num, int flags, int (*att_addr_f)(TA_S *, int))
{
    long      i;
    ENVELOPE *env;
    int       how_many_selected = 0,
	      added, rtype = 0,
	      we_cancel = 0,
	      special_processing = 0,
	      select_froms = 0;
    TA_S     *current = NULL,
	     *prev_comment_line,
	     *ta;
    BODY     **body_h,
	      *special_body = NULL,
	      *body = NULL;

    dprint((2, "\n - taking address into address book - \n"));

    if(!(cmd == 'a' || cmd == 'e'))
      return -1;

    if((flags & TA_AGG) && !pseudo_selected(ps_global->mail_stream, msgmap))
      return -1;

    if(mn_get_total(msgmap) > 0 && mn_total_cur(msgmap) == 1)
      special_processing = 1;

    if(flags & TA_AGG)
      select_froms = 1;

    /* this is a non-selectable label */
    current = fill_in_ta(&current, (ADDRESS *) NULL, 0,
	       /* TRANSLATORS: This is a heading describing some addresses the
	          user will have the chance to choose from. */
	       _(" These entries are taken from the attachments "));
    prev_comment_line = current;

    /*
     * Add addresses from special attachments for each message.
     */
    added = 0;
    for(i = mn_first_cur(msgmap); i > 0L; i = mn_next_cur(msgmap)){
	env = pine_mail_fetchstructure(ps->mail_stream, mn_m2raw(msgmap, i), &body);
	if(!env){
	    q_status_message(SM_ORDER | SM_DING, 3, 4,
	       _("Can't take address into address book. Error accessing folder"));
	    rtype = -1;
	    goto doneskee;
	}

	added += process_vcard_atts(ps->mail_stream, mn_m2raw(msgmap, i),
				    body, body, NULL, &current);
    }

    if(!(flags & TA_AGG)
       && added > 1
       && att_addr_f
       && (*att_addr_f)(current, added) <= 0)
      goto doneskee;

    if(!(flags & TA_NOPROMPT))
      we_cancel = busy_cue(NULL, NULL, 1);

    /*
     * add a comment line to separate attachment address lines
     * from header address lines
     */
    if(added > 0){
	current = fill_in_ta(&current, (ADDRESS *) NULL, 0,
	       /* TRANSLATORS: msg is message */
	       _(" These entries are taken from the msg headers "));
	prev_comment_line = current;
	how_many_selected += added;
	select_froms = 0;
    }
    else{  /* turn off header comment, and no separator comment */
	prev_comment_line->print = 0;
	prev_comment_line        = NULL;
    }

    /*
     * Add addresses from headers of messages.
     */
    for(i = mn_first_cur(msgmap); i > 0L; i = mn_next_cur(msgmap)){

	if(special_processing)
	  body_h = &special_body;
	else
	  body_h = NULL;

	env = pine_mail_fetchstructure(ps->mail_stream, mn_m2raw(msgmap, i),
				       body_h);
	if(!env){
	    if(we_cancel)
	      cancel_busy_cue(-1);

	    q_status_message(SM_ORDER | SM_DING, 3, 4,
	       _("Can't take address into address book. Error accessing folder"));
	    rtype = -1;
	    goto doneskee;
	}

	added = add_addresses_to_talist(ps, i, "from", &current,
					env->from, select_froms);
	if(select_froms)
	  how_many_selected += added;

	if(!address_is_same(env->from, env->reply_to))
	  (void)add_addresses_to_talist(ps, i, "reply-to", &current,
					env->reply_to, 0);

	if(!address_is_same(env->from, env->sender))
	  (void)add_addresses_to_talist(ps, i, "sender", &current,
					env->sender, 0);

	(void)add_addresses_to_talist(ps, i, "to", &current, env->to, 0);
	(void)add_addresses_to_talist(ps, i, "cc", &current, env->cc, 0);
	(void)add_addresses_to_talist(ps, i, "bcc", &current, env->bcc, 0);
    }
	
    /*
     * Check to see if we added an explanatory line about the
     * header addresses but no header addresses.  If so, remove the
     * explanatory line.
     */
    if(prev_comment_line){
	how_many_selected -=
	    eliminate_dups_and_us(first_sel_taline(current));
	for(ta = prev_comment_line->next; ta; ta = ta->next)
	  if(!ta->skip_it)
	    break;

	/* all entries were skip_it entries, turn off print */
	if(!ta){
	    prev_comment_line->print = 0;
	    prev_comment_line        = NULL;
	}
    }

    /*
     * If there's only one message we let the user view it using ^T
     * from the header editor.
     */
    if(!(flags & TA_NOPROMPT) && special_processing && env && special_body){
	msgno_for_pico_callback = mn_m2raw(msgmap, mn_first_cur(msgmap));
	body_for_pico_callback  = special_body;
	env_for_pico_callback   = env;
    }
    else{
	env_for_pico_callback   = NULL;
	body_for_pico_callback  = NULL;
    }

    current = fill_in_ta(&current, (ADDRESS *)NULL, 0,
     _(" Below this line are some possibilities taken from the text of the msg "));
    prev_comment_line = current;

    /*
     * Add addresses from the text of the body.
     */
    added = 0;
    for(i = mn_first_cur(msgmap); i > 0L; i = mn_next_cur(msgmap)){

	body = NULL;
	env = pine_mail_fetchstructure(ps->mail_stream, mn_m2raw(msgmap, i),
				       &body);
	if(env && body)
	  added += grab_addrs_from_body(ps->mail_stream,
				        mn_m2raw(msgmap, i),
				        body, &current);
    }

    if(added == 0){
	prev_comment_line->print = 0;
	prev_comment_line        = NULL;
    }

    /*
     * Check to see if all we added are dups.
     * If so, remove the comment line.
     */
    if(prev_comment_line){
	how_many_selected -= eliminate_dups_and_us(first_sel_taline(current));
	for(ta = prev_comment_line->next; ta; ta = ta->next)
	  if(!ta->skip_it)
	    break;

	/* all entries were skip_it entries, turn off print */
	if(!ta)
	  prev_comment_line->print = 0;
    }

    if(we_cancel)
      cancel_busy_cue(-1);

doneskee:
    env_for_pico_callback   = NULL;
    body_for_pico_callback  = NULL;
    
    ps->mangled_screen = 1;

    if(flags & TA_AGG)
      restore_selected(msgmap);

    *ta_ret = current;

    if(selected_num)
      *selected_num = how_many_selected;

    return(rtype);
}


int
convert_ta_to_lines(TA_S *ta_list, LINES_TO_TAKE **old_current)
{
    ADDRESS       *a;
    TA_S          *ta;
    LINES_TO_TAKE *new_current;
    char          *exportval, *printval;
    int            ret = 0;

    for(ta = first_sel_taline(ta_list);
	ta;
	ta = next_sel_taline(ta)){
	if(ta->skip_it)
	  continue;
	
	if(ta->frwrded){
	    if(ta->fullname)
	      fs_give((void **)&ta->fullname);
	    if(ta->fcc)
	      fs_give((void **)&ta->fcc);
	    if(ta->comment)
	      fs_give((void **)&ta->comment);
	    if(ta->nickname)
	      fs_give((void **)&ta->nickname);
	}
	else if(ta->addr && ta->addr->host && ta->addr->host[0] == '.')
	  ta->skip_it = 1;

	if(ta->frwrded){
	    for(a = ta->addr; a; a = a->next){
		ADDRESS *next_addr;

		if(a->host && a->host[0] == '.')
		  continue;

		next_addr = a->next;
		a->next = NULL;

		exportval = cpystr(simple_addr_string(a, tmp_20k_buf,
						      SIZEOF_20KBUF));
		if(!exportval || !exportval[0]){
		    if(exportval)
		      fs_give((void **)&exportval);
		    
		    a->next = next_addr;
		    continue;
		}

		printval = addr_list_string(a, NULL, 0);
		if(!printval || !printval[0]){
		    if(printval)
		      fs_give((void **)&printval);
		    
		    printval = cpystr(exportval);
		}
	    
		new_current = new_ltline(old_current);
		new_current->flags = LT_NONE;

		new_current->printval = printval;
		new_current->exportval = exportval;
		a->next = next_addr;
		ret++;
	    }
	}
	else{
	    if(ta->addr){
		exportval = cpystr(simple_addr_string(ta->addr, tmp_20k_buf,
						      SIZEOF_20KBUF));
		if(exportval && exportval[0]){
		    new_current = new_ltline(old_current);
		    new_current->flags = LT_NONE;

		    new_current->exportval = exportval;

		    if(ta->strvalue && ta->strvalue[0])
		      new_current->printval = cpystr(ta->strvalue);
		    else
		      new_current->printval = cpystr(new_current->exportval);

		    ret++;
		}
		else if(exportval)
		  fs_give((void **)&exportval);
	    }
	}
    }

    return(ret);
}


/*
 * new_ltline - create new LINES_TO_TAKE, zero it out,
 *                and insert it after current.
 *                NOTE current gets set to the new current.
 */
LINES_TO_TAKE *
new_ltline(LINES_TO_TAKE **current)
{
    LINES_TO_TAKE *p;

    p = (LINES_TO_TAKE *)fs_get(sizeof(LINES_TO_TAKE));
    memset((void *)p, 0, sizeof(LINES_TO_TAKE));
    if(current){
	if(*current){
	    p->next	     = (*current)->next;
	    (*current)->next = p;
	    p->prev	     = *current;
	    if(p->next)
	      p->next->prev = p;
	}

	*current = p;
    }

    return(p);
}


int
add_addresses_to_talist(struct pine *ps, long int msgno, char *field,
			TA_S **old_current, struct mail_address *adrlist, int checked)
{
    ADDRESS *addr;
    int      added = 0;

    for(addr = adrlist; addr; addr = addr->next){
	if(addr->host && addr->host[0] == '.'){
	    char *h, *fields[2];

	    fields[0] = field;
	    fields[1] = NULL;
	    if((h = pine_fetchheader_lines(ps->mail_stream, msgno,NULL,fields)) != NULL){
		char *p, fname[32];
		int q;

		q = strlen(h);

		snprintf(fname, sizeof(fname), "%s:", field);
		for(p = h; (p = strstr(p, fname)) != NULL; )
		  rplstr(p, q-(p-h), strlen(fname), "");   /* strip field strings */
		
		sqznewlines(h);                   /* blat out CR's & LF's */
		for(p = h; (p = strchr(p, TAB)) != NULL; )
		  *p++ = ' ';                     /* turn TABs to space */
		
		if(*h){
		    unsigned long l;
		    ADDRESS *new_addr;
		    size_t ll;

		    new_addr = (ADDRESS *)fs_get(sizeof(ADDRESS));
		    memset(new_addr, 0, sizeof(ADDRESS));

		    /*
		     * Base64 armor plate the gunk to protect against
		     * c-client quoting in output.
		     */
		    p = (char *)rfc822_binary((void *)h,
					      (unsigned long)strlen(h), &l);
		    sqznewlines(p);
		    ll = strlen(p) + 3;
		    new_addr->mailbox = (char *) fs_get((ll+1) * sizeof(char));
		    snprintf(new_addr->mailbox, ll+1, "&%s", p);
		    fs_give((void **)&p);
		    new_addr->host = cpystr(RAWFIELD);
		    fill_in_ta(old_current, new_addr, 0, (char *)NULL);
		    mail_free_address(&new_addr);
		}

		fs_give((void **)&h);
	    }

	    return(0);
	}
    }

    /* no syntax errors in list, add them all */
    for(addr = adrlist; addr; addr = addr->next){
	fill_in_ta(old_current, addr, checked, (char *)NULL);
	added++;
    }

    return(added);
}


/*
 * Look for Text/directory attachments and process them.
 * Return number of lines added to the ta_list.
 */
int
process_vcard_atts(MAILSTREAM *stream, long int msgno,
		   struct mail_bodystruct *root, struct mail_bodystruct *body,
		   char *partnum, TA_S **ta_list)
{
    char      *addrs,        /* comma-separated list of addresses */
	      *value,
	      *encoded,
	      *escval,
	      *nickname,
	      *fullname,
	      *struct_name,
	      *fcc,
	      *tag,
	      *decoded,
	      *num = NULL,
	      *defaulttype = NULL,
	      *charset = NULL,
	      *altcharset,
	      *comma,
	      *p,
	      *comment,
	      *title,
	     **lines = NULL,
	     **ll;
    size_t     space;
    int        used = 0,
	       is_encoded = 0,
	       vcard_nesting = 0,
	       selected = 0;
    PART      *part;
    PARAMETER *parm;
    static char a_comma = ',';

    /*
     * Look through all the subparts searching for our special type.
     */
    if(body && body->type == TYPEMULTIPART){
	for(part = body->nested.part; part; part = part->next)
	  selected += process_vcard_atts(stream, msgno, root, &part->body,
					 partnum, ta_list);
    }
    /* only look in rfc822 subtype of type message */
    else if(body && body->type == TYPEMESSAGE
	    && body->subtype && !strucmp(body->subtype, "rfc822")){
	selected += process_vcard_atts(stream, msgno, root,
				       body->nested.msg->body,
				       partnum, ta_list);
    }
    /* found one (TYPEAPPLICATION for back compat.) */
    else if(body && MIME_VCARD(body->type,body->subtype)){

	dprint((4, "\n - found abook attachment to process - \n"));

	/*
	 * The Text/directory type has a possible parameter called
	 * "defaulttype" that we need to look for (this is from an old draft
	 * and is supported only for backwards compatibility). There is
	 * also a possible default "charset". (Default charset is also from
	 * an old draft and is no longer allowed.) We don't care about any of
	 * the other parameters.
	 */
	for(parm = body->parameter; parm; parm = parm->next)
	  if(!strucmp("defaulttype", parm->attribute))
	    break;
	
	if(parm)
	  defaulttype = parm->value;

	/* ...and look for possible charset parameter */
	for(parm = body->parameter; parm; parm = parm->next)
	  if(!strucmp("charset", parm->attribute))
	    break;
	
	if(parm)
	  charset = parm->value;

	num = partnum ? cpystr(partnum) : partno(root, body);
	lines = detach_vcard_att(stream, msgno, body, num);
	if(num)
	  fs_give((void **)&num);

	nickname = fullname = comment = title = fcc = struct_name = NULL;
#define CHUNK (size_t)500
	space = CHUNK;
	/* make comma-separated list of email addresses in addrs */
	addrs = (char *)fs_get((space+1) * sizeof(char));
	*addrs = '\0';
	for(ll = lines; ll && *ll; ll++){
	    altcharset = NULL;
	    decoded = NULL;
	    escval = NULL;
	    is_encoded = 0;
	    value = getaltcharset(*ll, &tag, &altcharset, &is_encoded);

	    if(!value || !tag){
		if(tag)
		  fs_give((void **)&tag);

		if(altcharset)
		  fs_give((void **)&altcharset);

		continue;
	    }

	    if(is_encoded){
		unsigned long length;

		decoded = (char *)rfc822_base64((unsigned char *)value,
						(unsigned long)strlen(value),
						&length);
		if(decoded){
		    decoded[length] = '\0';
		    value = decoded;
		}
		else
		  value = "<malformed B64 data>";
	    }

	    /* support default tag (back compat.) */
	    if(*tag == '\0' && defaulttype){
		fs_give((void **)&tag);
		tag = cpystr(defaulttype);
	    }

	    if(!strucmp(tag, "begin")){  /* vCard BEGIN */
		vcard_nesting++;
	    }
	    else if(!strucmp(tag, "end")){  /* vCard END */
		if(--vcard_nesting == 0){
		    if(title){
			if(!comment)
			  comment = title;
			else
			  fs_give((void **)&title);
		    }

		    /* add this entry */
		    selected += vcard_to_ta(addrs, fullname, struct_name,
					    nickname, comment, fcc, ta_list);
		    if(addrs)
		      *addrs = '\0';
		    
		    used = 0;
		    nickname = fullname = comment = title = fcc = NULL;
		    struct_name = NULL;
		}
	    }
	    /* add another address to addrs */
	    else if(!strucmp(tag, "email")){
		if(*value){
		    escval = vcard_unescape(value);
		    encoded = encode_fullname_of_addrstring(escval,
			(altcharset && *altcharset) ? altcharset
						    : (charset && *charset)
						     ? charset
						     : ps_global->posting_charmap);
		    if(encoded){
			/* allocate more space */
			if((used + strlen(encoded) + 1) > space){
			    space += CHUNK;
			    fs_resize((void **)&addrs, (space+1)*sizeof(char));
			}

			if(*addrs){
			  strncat(addrs, ",", space+1-1-strlen(addrs));
			  addrs[space-1] = '\0';
			}

			strncat(addrs, encoded, space+1-1-strlen(addrs));
			addrs[space-1] = '\0';
			used += (strlen(encoded) + 1);
			fs_give((void **)&encoded);
		    }
		}
	    }
	    else if(!strucmp(tag, "note") || !strucmp(tag, "misc")){
		if(*value){
		    escval = vcard_unescape(value);
		    encoded = rfc1522_encode(tmp_20k_buf, SIZEOF_20KBUF,
					     (unsigned char *)escval,
			(altcharset && *altcharset) ? altcharset
						    : (charset && *charset)
						     ? charset
						     : ps_global->posting_charmap);
		    if(encoded){
			/* Last one wins */
			if(comment)
			  fs_give((void **)&comment);

			comment = cpystr(encoded);
		    }
		}
	    }
	    else if(!strucmp(tag, "title")){
		if(*value){
		    escval = vcard_unescape(value);
		    encoded = rfc1522_encode(tmp_20k_buf, SIZEOF_20KBUF,
					     (unsigned char *)escval,
			(altcharset && *altcharset) ? altcharset
						    : (charset && *charset)
						     ? charset
						     : ps_global->posting_charmap);
		    if(encoded){
			/* Last one wins */
			if(title)
			  fs_give((void **)&title);

			title = cpystr(encoded);
		    }
		}
	    }
	    else if(!strucmp(tag, "x-fcc")){
		if(*value){
		    escval = vcard_unescape(value);
		    encoded = rfc1522_encode(tmp_20k_buf, SIZEOF_20KBUF,
					     (unsigned char *)escval,
			(altcharset && *altcharset) ? altcharset
						    : (charset && *charset)
						     ? charset
						     : ps_global->posting_charmap);
		    if(encoded){
			/* Last one wins */
			if(fcc)
			  fs_give((void **)&fcc);

			fcc = cpystr(encoded);
		    }
		}
	    }
	    else if(!strucmp(tag, "fn") || !strucmp(tag, "cn")){
		if(*value){
		    escval = vcard_unescape(value);
		    encoded = rfc1522_encode(tmp_20k_buf, SIZEOF_20KBUF,
					     (unsigned char *)escval,
			(altcharset && *altcharset) ? altcharset
						    : (charset && *charset)
						     ? charset
						     : ps_global->posting_charmap);
		    if(encoded){
			/* Last one wins */
			if(fullname)
			  fs_give((void **)&fullname);

			fullname = cpystr(encoded);
		    }
		}
	    }
	    else if(!strucmp(tag, "n")){
		/*
		 * This is a structured name field. It has up to five
		 * fields separated by ';'. The fields are Family Name (which
		 * we'll take to be Last name for our purposes), Given Name
		 * (First name for us), additional names, prefixes, and
		 * suffixes. We'll ignore prefixes and suffixes.
		 *
		 * If we find one of these records we'll use it in preference
		 * to the formatted name field (fn).
		 */
		if(*value && *value != ';'){
		    char *last, *first, *middle = NULL, *rest = NULL;
		    char *esclast, *escfirst, *escmiddle;
		    static char a_semi = ';';

		    last = value;

		    first = NULL;
		    p = last;
		    while(p && (first = strindex(p, a_semi)) != NULL){
			if(first > last && first[-1] != '\\')
			  break;
			else
			  p = first + 1;
		    }

		    if(first)
		      *first = '\0';

		    if(first && *(first+1) && *(first+1) != a_semi){
			first++;

			middle = NULL;
			p = first;
			while(p && (middle = strindex(p, a_semi)) != NULL){
			    if(middle > first && middle[-1] != '\\')
			      break;
			    else
			      p = middle + 1;
			}

			if(middle)
			  *middle = '\0';

			if(middle && *(middle+1) && *(middle+1) != a_semi){
			    middle++;

			    rest = NULL;
			    p = middle;
			    while(p && (rest = strindex(p, a_semi)) != NULL){
				if(rest > middle && rest[-1] != '\\')
				  break;
				else
				  p = rest + 1;
			    }

			    /* we don't care about the rest */
			    if(rest)
			      *rest = '\0';
			}
			else
			  middle = NULL;
		    }
		    else
		      first = NULL;

		    /*
		     * Structured name pieces can have multiple values.
		     * We're just going to take the first value in each part.
		     * Skip excaped commas, though.
		     */
		    p = last;
		    while(p && (comma = strindex(p, a_comma)) != NULL){
			if(comma > last && comma[-1] != '\\'){
			    *comma = '\0';
			    break;
			}
			else
			  p = comma + 1;
		    }

		    p = first;
		    while(p && (comma = strindex(p, a_comma)) != NULL){
			if(comma > first && comma[-1] != '\\'){
			    *comma = '\0';
			    break;
			}
			else
			  p = comma + 1;
		    }

		    p = middle;
		    while(p && (comma = strindex(p, a_comma)) != NULL){
			if(comma > middle && comma[-1] != '\\'){
			    *comma = '\0';
			    break;
			}
			else
			  p = comma + 1;
		    }

		    esclast = vcard_unescape(last);
		    escfirst = vcard_unescape(first);
		    escmiddle = vcard_unescape(middle);
		    snprintf(tmp_20k_buf+10000, SIZEOF_20KBUF-10000, "%s%s%s%s%s",
			    esclast ? esclast : "",
			    escfirst ? ", " : "",
			    escfirst ? escfirst : "",
			    escmiddle ? " " : "",
			    escmiddle ? escmiddle : "");
		    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';

		    encoded = rfc1522_encode(tmp_20k_buf, SIZEOF_20KBUF-10000,
					     (unsigned char *)tmp_20k_buf+10000,
			(altcharset && *altcharset) ? altcharset
						    : (charset && *charset)
						     ? charset
						     : ps_global->posting_charmap);
		    tmp_20k_buf[SIZEOF_20KBUF-10000-1] = '\0';

		    if(esclast && *esclast && escfirst && *escfirst){
			if(struct_name)
			  fs_give((void **)&struct_name);

			struct_name = cpystr(encoded);
		    }
		    else{
			/* in case we don't get a fullname better than this */
			if(!fullname)
			  fullname = cpystr(encoded);
		    }

		    if(esclast)
		      fs_give((void **)&esclast);
		    if(escfirst)
		      fs_give((void **)&escfirst);
		    if(escmiddle)
		      fs_give((void **)&escmiddle);
		}
	    }
	    /* suggested nickname */
	    else if(!strucmp(tag, "nickname") || !strucmp(tag, "x-nickname")){
		if(*value){
		    /* Last one wins */
		    if(nickname)
		      fs_give((void **)&nickname);

		    /*
		     * Nickname can have multiple values. We're just
		     * going to take the first. Skip escaped commas, though.
		     */
		    p = value;
		    while(p && (comma = strindex(p, a_comma)) != NULL){
			if(comma > value && comma[-1] != '\\'){
			    *comma = '\0';
			    break;
			}
			else
			  p = comma + 1;
		    }

		    nickname = vcard_unescape(value);
		}
	    }

	    if(tag)
	      fs_give((void **)&tag);

	    if(altcharset)
	      fs_give((void **)&altcharset);

	    if(decoded)
	      fs_give((void **)&decoded);

	    if(escval)
	      fs_give((void **)&escval);
	}

	if(title){
	    if(!comment)
	      comment = title;
	    else
	      fs_give((void **)&title);
	}

	if(fullname || struct_name || nickname || fcc
	   || comment || (addrs && *addrs))
	  selected += vcard_to_ta(addrs, fullname, struct_name, nickname,
				  comment, fcc, ta_list);

	if(addrs)
	  fs_give((void **)&addrs);

	free_list_array(&lines);
    }

    return(selected);
}


int
cmp_swoop_list(const qsort_t *a1, const qsort_t *a2)
{
    SWOOP_S *x = (SWOOP_S *)a1;
    SWOOP_S *y = (SWOOP_S *)a2;

    if(x->dup){
	if(y->dup)
	  return((x->dst_enum > y->dst_enum) ? -1
		   : (x->dst_enum == y->dst_enum) ? 0 : 1);
	else
	  return(-1);
    }
    else if(y->dup)
      return(1);
    else
      return((x > y) ? -1 : (x == y) ? 0 : 1);  /* doesn't matter */
}



/*
 * Take the split out contents of a vCard entry and turn them into a TA.
 *
 * Always returns 1, the number of TAs added to ta_list.
 */
int
vcard_to_ta(char *addrs, char *fullname, char *better_fullname, char *nickname,
	    char *comment, char *fcc, TA_S **ta_list)
{
    ADDRESS   *addrlist = NULL;

    /*
     * Parse it into an addrlist, which is what fill_in_ta wants
     * to see.
     */
    rfc822_parse_adrlist(&addrlist, addrs, fakedomain);
    if(!addrlist)
      addrlist = mail_newaddr();  /* empty addr, to make right thing happen */

    *ta_list = fill_in_ta(ta_list, addrlist, 1,
			  fullname ? fullname
				   : better_fullname ? better_fullname
				   		     : "Forwarded Entry");
    (*ta_list)->nickname = nickname ? nickname : cpystr("");
    (*ta_list)->comment  = comment;
    (*ta_list)->fcc      = fcc;

    /*
     * We are tempted to want to call switch_to_last_comma_first() here when
     * we don't have a better_fullname (which is already last, first).
     * But, since this is the way it was sent to us we should probably leave
     * it alone. That means that if we use a fullname culled from the
     * body of a message, or from a header, or from an "N" vcard record,
     * we will make it be Last, First; but if we use one from an "FN" record
     * we won't.
     */
    (*ta_list)->fullname = better_fullname ? better_fullname
					   : fullname;

    if((*ta_list)->nickname)
      convert_possibly_encoded_str_to_utf8(&(*ta_list)->nickname);
    
    if((*ta_list)->comment)
      convert_possibly_encoded_str_to_utf8(&(*ta_list)->comment);
    
    if((*ta_list)->fcc)
      convert_possibly_encoded_str_to_utf8(&(*ta_list)->fcc);
    
    if((*ta_list)->fullname)
      convert_possibly_encoded_str_to_utf8(&(*ta_list)->fullname);
    
    /*
     * The TA list will free its members but we have to take care of this
     * extra one that isn't assigned to a TA member.
     */
    if(better_fullname && fullname)
      fs_give((void **)&fullname);

    if(addrlist)
      mail_free_address(&addrlist);

    return(1);
}


/*
 * Look through line which is supposed to look like
 *
 *  type;charset=iso-8859-1;encoding=b: value
 *
 * Type might be email, or nickname, ...  It is optional because of the
 * defaulttype parameter (there isn't such a thing as defaulttype parameters
 * as of Nov. 96 draft). The semicolon and colon are special chars.  Each
 * parameter in this line is a semicolon followed by the parameter type "="
 * the parameter value.  Parameters are optional, too.  There is always a colon,
 * followed by value.  Whitespace can be everywhere up to where value starts.
 * There is also an optional <group> "." preceding the type, which we will
 * ignore. It's for grouping related lines.
 * (As of July, 97 draft, it is no longer permissible to include a charset
 * parameter in each line. Only one is permitted in the content-type mime hdr.)
 * (Also as of July, 97 draft, all white space is supposed to be significant.
 * We will continue to skip white space surrounding '=' and so forth for
 * backwards compatibility and because it does no harm in almost all cases.)
 * (As of Nov, 97 draft, some white space is not significant. That is, it is
 * allowed in some places.)
 *
 *
 * Args: line     -- the line to look at
 *       type     -- this is a return value, and is an allocated copy of
 *                    the type, freed by the caller. For example, "email".
 *       alt      -- this is a return value, and is an allocated copy of
 *                    the value of the alternate charset, to be freed by
 *                    the caller.  For example, this might be "iso-8859-2".
 *     encoded    -- this is a return value, and is set to 1 if the value
 *                    is b-encoded
 *
 * Return value: a pointer to the start of value, or NULL if the syntax
 * isn't right.  It's possible for value to be equal to "".
 */
char *
getaltcharset(char *line, char **type, char **alt, int *encoded)
{
    char        *p, *q, *left_semi, *group_dot, *colon;
    char        *start_of_enc, *start_of_cset, tmpsave;
    static char *cset = "charset";
    static char *enc = "encoding";

    if(type)
      *type = NULL;

    if(alt)
      *alt = NULL;

    if(encoded)
      *encoded = 0;

    colon = strindex(line, ':');
    if(!colon)
      return NULL;

    left_semi = strindex(line, ';');
    if(left_semi && left_semi > colon)
      left_semi = NULL;
    
    group_dot = strindex(line, '.');
    if(group_dot && (group_dot > colon || (left_semi && group_dot > left_semi)))
      group_dot = NULL;

    /*
     * Type is everything up to the semicolon, or the colon if no semicolon.
     * However, we want to skip optional <group> ".".
     */
    if(type){
	q = left_semi ? left_semi : colon;
	tmpsave = *q;
	*q = '\0';
	*type = cpystr(group_dot ? group_dot+1 : line);
	*q = tmpsave;
	sqzspaces(*type);
    }

    if(left_semi && alt
       && (p = srchstr(left_semi+1, cset))
       && p < colon){
	p += strlen(cset);
	p = skip_white_space(p);
	if(*p++ == '='){
	    p = skip_white_space(p);
	    if(p < colon){
		start_of_cset = p;
		p++;
		while(p < colon && !isspace((unsigned char)*p) && *p != ';')
		  p++;
		
		tmpsave = *p;
		*p = '\0';
		*alt = cpystr(start_of_cset);
		*p = tmpsave;
	    }
	}
    }

    if(encoded && left_semi
       && (p = srchstr(left_semi+1, enc))
       && p < colon){
	p += strlen(enc);
	p = skip_white_space(p);
	if(*p++ == '='){
	    p = skip_white_space(p);
	    if(p < colon){
		start_of_enc = p;
		p++;
		while(p < colon && !isspace((unsigned char)*p) && *p != ';')
		  p++;
		
		if(*start_of_enc == 'b' && start_of_enc + 1 == p)
		  *encoded = 1;
	    }
	}
    }

    p = colon + 1;

    return(skip_white_space(p));
}


/*
 * Return incoming_fullname in Last, First form.
 * The passed in fullname should already be in UTF-8.
 * If there is already a comma, we add quotes around the fullname so we can
 *   tell it isn't a Last, First comma but a real comma in the Fullname.
 *
 * Args: incoming_fullname -- 
 *                new_full -- passed in pointer to place to put new fullname
 *                    nbuf -- size of new_full
 * Returns: new_full
 */
void
switch_to_last_comma_first(char *incoming_fullname, char *new_full, size_t nbuf)
{
    char  save_value;
    char *save_end, *nf, *inc, *p = NULL;

    if(nbuf < 1)
      return;

    if(incoming_fullname == NULL){
	new_full[0] = '\0';
	return;
    }

    /* get rid of leading white space */
    for(inc = incoming_fullname; *inc && isspace((unsigned char)*inc); inc++)
      ;/* do nothing */

    /* get rid of trailing white space */
    for(p = inc + strlen(inc) - 1; *p && p >= inc && 
	  isspace((unsigned char)*p); p--)
      ;/* do nothing */
    
    save_end = ++p;
    if(save_end == inc){
	new_full[0] = '\0';
	return;
    }

    save_value = *save_end;  /* so we don't alter incoming_fullname */
    *save_end = '\0';
    nf = new_full;
    memset(new_full, 0, nbuf);		/* need this the way it is done below */

    if(strindex(inc, ',') != NULL){
	int add_quotes = 0;

	/*
	 * Quote already existing comma.
	 *
	 * We'll get this wrong if it is already quoted but the quote
	 * doesn't start right at the beginning.
	 */
	if(inc[0] != '"'){
	    add_quotes++;
	    if(nf-new_full < nbuf-1)
	      *nf++ = '"';
	}

	strncpy(nf, inc, MIN(nbuf - (add_quotes ? 3 : 1), nbuf-(nf-new_full)-1));
	new_full[nbuf-1] = '\0';
	if(add_quotes){
	  strncat(nf, "\"", nbuf-(nf-new_full)-1);
	  new_full[nbuf-1] = '\0';
	}
    }
    else if(strindex(inc, SPACE)){
	char *last, *end;

	end = new_full + nbuf - 1;

	/*
	 * Switch First Middle Last to Last, First Middle
	 */

	/* last points to last word, which does exist */
	last = skip_white_space(strrindex(inc, SPACE));

	/* copy Last */
	for(p = last; *p && nf < end-2 && nf-new_full < nbuf; *nf++ = *p++)
	  ;/* do nothing */

	if(nf-new_full < nbuf-1)
	  *nf++ = ',';

	if(nf-new_full < nbuf-1)
	  *nf++ = SPACE;

	/* copy First Middle */
	for(p = inc; p < last && nf < end && nf-new_full < nbuf-1; *nf++ = *p++)
	  ;/* do nothing */

	new_full[nbuf-1] = '\0';

	removing_trailing_white_space(new_full);
    }
    else
      strncpy(new_full, inc, nbuf-1);

    new_full[nbuf-1] = '\0';

    *save_end = save_value;
}


/*
 * Fetch this body part, content-decode it if necessary, split it into
 * lines, and return the lines in an allocated array, which is freed
 * by the caller.  Folded lines are replaced by single long lines.
 */
char **
detach_vcard_att(MAILSTREAM *stream, long int msgno, struct mail_bodystruct *body, char *partnum)
{
    unsigned long length;
    char    *text  = NULL, /* raw text */
	    *dtext = NULL, /* content-decoded text */
	   **res   = NULL, /* array of decoded lines */
	    *lptr,         /* line pointer */
	    *next_line;
    int      i, count;

    /* make our own copy of text so we can change it */
    text = cpystr(mail_fetchbody_full(stream, msgno, partnum, &length,FT_PEEK));
    text[length] = '\0';

    /* decode the text */
    switch(body->encoding){
      default:
      case ENC7BIT:
      case ENC8BIT:
      case ENCBINARY:
	dtext = text;
	break;

      case ENCBASE64:
	dtext = (char *)rfc822_base64((unsigned char *)text,
				      (unsigned long)strlen(text),
				      &length);
	if(dtext)
	  dtext[length] = '\0';
	else
	  q_status_message(SM_ORDER | SM_DING, 3, 4,
	       "Malformed B64 data in address book attachment.");

	if(text)
	  fs_give((void **)&text);

	break;

      case ENCQUOTEDPRINTABLE:
	dtext = (char *)rfc822_qprint((unsigned char *)text,
				      (unsigned long)strlen(text),
				      &length);
	if(dtext)
	  dtext[length] = '\0';
	else
	  q_status_message(SM_ORDER | SM_DING, 3, 4,
	       "Malformed QP data in address book attachment.");

	if(text)
	  fs_give((void **)&text);

	break;
    }

    /* count the lines */
    next_line = dtext;
    count = 0;
    for(lptr = next_line; lptr && *lptr; lptr = next_line){
	for(next_line = lptr; *next_line; next_line++){
	    /*
	     * Find end of current line.
	     */
	    if(*next_line == '\r' && *(next_line+1) == '\n'){
		next_line += 2;
		/* not a folded line, count it */
		if(!*next_line || (*next_line != SPACE && *next_line != TAB))
		  break;
	    }
	}

	count++;
    }

    /* allocate space for resulting array of lines */
    res = (char **)fs_get((count + 1) * sizeof(char *));
    memset((void *)res, 0, (count + 1) * sizeof(char *));
    next_line = dtext;
    for(i=0, lptr=next_line; lptr && *lptr && i < count; lptr=next_line, i++){
	/*
	 * Move next_line to start of next line and null terminate
	 * current line.
	 */
	for(next_line = lptr; *next_line; next_line++){
	    /*
	     * Find end of current line.
	     */
	    if(*next_line == '\r' && *(next_line+1) == '\n'){
		next_line += 2;
		/* not a folded line, terminate it */
		if(!*next_line || (*next_line != SPACE && *next_line != TAB)){
		    *(next_line-2) = '\0';
		    break;
		}
	    }
	}

	/* turn folded lines into long lines in place */
	vcard_unfold(lptr);
	res[i] = cpystr(lptr);
    }

    if(dtext)
      fs_give((void **)&dtext);

    res[count] = '\0';
    return(res);
}


/*
 * Look for possible addresses in the first text part of a message for
 * use by TakeAddr command.
 * Returns the number of TA lines added.
 */
int
grab_addrs_from_body(MAILSTREAM *stream, long int msgno,
		     struct mail_bodystruct *body, TA_S **ta_list)
{
#define MAXLINESZ 2000
    char       line[MAXLINESZ + 1];
    STORE_S   *so;
    gf_io_t    pc;
    SourceType src = CharStar;
    int        added = 0, failure;

    dprint((9, "\n - grab_addrs_from_body - \n"));

    /*
     * If it is text/plain or it is multipart with a first part of text/plain,
     * we want to continue, else forget it.
     */
    if(!((body->type == TYPETEXT && body->subtype
	  && ALLOWED_SUBTYPE(body->subtype))
			      ||
         (body->type == TYPEMULTIPART && body->nested.part
	  && body->nested.part->body.type == TYPETEXT
	  && ALLOWED_SUBTYPE(body->nested.part->body.subtype))))
      return 0;

#ifdef DOS
    src = TmpFileStar;
#endif

    if((so = so_get(src, NULL, EDIT_ACCESS)) == NULL)
      return 0;

    gf_set_so_writec(&pc, so);

    failure = !get_body_part_text(stream, body, msgno, "1", 0L, pc,
				  NULL, NULL, GBPT_NONE);

    gf_clear_so_writec(so);

    if(failure){
	so_give(&so);
	return 0;
    }

    so_seek(so, 0L, 0);

    while(get_line_of_message(so, line, sizeof(line))){
	ADDRESS *addr;
	char     save_end, *start, *tmp_a_string, *tmp_personal, *p;
	int      n;

	/* process each @ in the line */
	for(p = (char *) line; (start = mail_addr_scan(p, &n)); p = start + n){

	    tmp_personal = NULL;

	    if(start > line && *(start-1) == '<' && *(start+n) == '>'){
		int   words, in_quote;
		char *fn_start;

		/*
		 * Take a shot at looking for full name
		 * If we can find a colon maybe we've got a header line
		 * embedded in the body.
		 * This is real ad hoc.
		 */

		/*
		 * Go back until we run into a character that probably
		 * isn't a fullname character.
		 */
		fn_start = start-1;
		in_quote = words = 0;
		while(fn_start > p && (in_quote || !(*(fn_start-1) == ':'
		      || *(fn_start-1) == ';' || *(fn_start-1) == ','))){
		    fn_start--;
		    if(!in_quote && isspace((unsigned char)*fn_start))
		      words++;
		    else if(*fn_start == '"' &&
			    (fn_start == p || *(fn_start-1) != '\\')){
			if(in_quote){
			    in_quote = 0;
			    break;
			}
			else
			  in_quote = 1;
		    }

		    if(words == 5)
		      break;
		}

		/* wasn't a real quote, forget about fullname */
		if(in_quote)
		  fn_start = start-1;

		/* Skip forward over the white space. */
		while(isspace((unsigned char)*fn_start) || *fn_start == '(')
		  fn_start++;

		/*
		 * Make sure the first word is capitalized.
		 * (just so it is more likely it is a name)
		 */
		while(fn_start < start-1 &&
		      !(isupper(*fn_start) || *fn_start == '"')){
		    if(*fn_start == '('){
			fn_start++;
			continue;
		    }

		    /* skip forward over this word */
		    while(fn_start < start-1 && 
			  !isspace((unsigned char)*fn_start))
		      fn_start++;

		    while(fn_start < start-1 && 
			  isspace((unsigned char)*fn_start))
		      fn_start++;
		}

		if(fn_start < start-1){
		    char *fn_end, save_fn_end;
		    
		    /* remove white space between fullname and start */
		    fn_end = start-1;
		    while(fn_end > fn_start
			  && isspace((unsigned char)*(fn_end - 1)))
		      fn_end--;

		    save_fn_end = *fn_end;
		    *fn_end = '\0';

		    /* remove matching quotes */
		    if(*fn_start == '"' && *(fn_end-1) == '"'){
			fn_start++;
			*fn_end = save_fn_end;
			fn_end--;
			save_fn_end = *fn_end;
			*fn_end = '\0';
		    }

		    /* copy fullname */
		    if(*fn_start)
		      tmp_personal = cpystr(fn_start);

		    *fn_end = save_fn_end;
		}
	    }


	    save_end = *(start+n);
	    *(start+n) = '\0';
	    /* rfc822_parse_adrlist feels free to destroy input so send copy */
	    tmp_a_string = cpystr(start);
	    *(start+n) = save_end;
	    addr = NULL;
	    ps_global->c_client_error[0] = '\0';
	    rfc822_parse_adrlist(&addr, tmp_a_string, fakedomain);
	    if(tmp_a_string)
	      fs_give((void **)&tmp_a_string);

	    if(tmp_personal){
		if(addr){
		    if(addr->personal)
		      fs_give((void **)&addr->personal);

		    addr->personal = tmp_personal;
		}
		else
		  fs_give((void **)&tmp_personal);
	    }

	    if((addr && addr->host && addr->host[0] == '@') ||
	       ps_global->c_client_error[0]){
		mail_free_address(&addr);
		continue;
	    }

	    if(addr && addr->mailbox && addr->host){
		added++;
		*ta_list = fill_in_ta(ta_list, addr, 0, (char *)NULL);
	    }

	    if(addr)
	      mail_free_address(&addr);
	}
    }

    so_give(&so);
    return(added);
}


/*
 * Get the next line of the object pointed to by source.
 * Skips empty lines.
 * Linebuf is a passed in buffer to put the line in.
 * Linebuf is null terminated and \r and \n chars are removed.
 * 0 is returned when there is no more input.
 */
int
get_line_of_message(STORE_S *source, char *linebuf, int linebuflen)
{
    int pos = 0;
    unsigned char c;

    if(linebuflen < 2)
      return 0;

    while(so_readc(&c, source)){
	if(c == '\n' || c == '\r'){
	  if(pos > 0)
	    break;
	}
	else{
	    linebuf[pos++] = c;
	    if(pos >= linebuflen - 2)
	      break;
	}
    }

    linebuf[pos] = '\0';

    return(pos);
}


/*
 * Inserts a new entry based on addr in the TA list.
 * Makes sure everything is UTF-8. That is,
 *   Values used for strvalue will be converted to UTF-8 first.
 *   Personal name fields of addr args will be converted to UTF-8.
 *
 * Args: old_current -- the TA list
 *              addr -- the address for this line
 *           checked -- start this line out checked
 *      print_string -- if non-null, this line is informational and is just
 *                       printed out, it can't be selected
 */
TA_S *
fill_in_ta(TA_S **old_current, struct mail_address *addr, int checked, char *print_string)
{
    TA_S *new_current;

    /* c-client convention for group syntax, which we don't want to deal with */
    if(!print_string && (!addr || !addr->mailbox || !addr->host))
      new_current = *old_current;
    else{

	new_current           = new_taline(old_current);
	if(print_string && addr){	/* implied List when here */
	    new_current->frwrded  = 1;
	    new_current->skip_it  = 0;
	    new_current->print    = 0;
	    new_current->checked  = checked != 0;
	    new_current->addr     = copyaddrlist(addr);
	    decode_addr_names_to_utf8(new_current->addr);
	    new_current->strvalue = cpystr(print_string);
	    convert_possibly_encoded_str_to_utf8(&new_current->strvalue);
	}
	else if(print_string){
	    new_current->frwrded  = 0;
	    new_current->skip_it  = 1;
	    new_current->print    = 1;
	    new_current->checked  = 0;
	    new_current->addr     = 0;
	    new_current->strvalue = cpystr(print_string);
	    convert_possibly_encoded_str_to_utf8(&new_current->strvalue);
	}
	else{
	    new_current->frwrded  = 0;
	    new_current->skip_it  = 0;
	    new_current->print    = 0;
	    new_current->checked  = checked != 0;
	    new_current->addr     = copyaddr(addr);
	    decode_addr_names_to_utf8(new_current->addr);
	    if(addr->host[0] == '.')
	      new_current->strvalue = cpystr("Error in address (ok to try Take anyway)");
	    else{
		char *bufp;
		size_t len;

		len = est_size(new_current->addr);
		bufp = (char *) fs_get(len * sizeof(char));
		new_current->strvalue = cpystr(addr_string(new_current->addr, bufp, len));
		fs_give((void **) &bufp);
	    }

	    convert_possibly_encoded_str_to_utf8(&new_current->strvalue);
	}
    }

    return(new_current);
}


int
eliminate_dups_and_us(TA_S *list)
{
    return(eliminate_dups_and_maybe_us(list, 1));
}


int
eliminate_dups_but_not_us(TA_S *list)
{
    return(eliminate_dups_and_maybe_us(list, 0));
}


/*
 * Look for dups in list and mark them so we'll skip them.
 *
 * We also throw out any addresses that are us (if us_too is set), since
 * we won't usually want to take ourselves to the addrbook.
 * On the otherhand, if there is nothing but us, we leave it.
 *
 * Don't toss out forwarded entries.
 *
 * Returns the number of dups eliminated that were also selected.
 */
int
eliminate_dups_and_maybe_us(TA_S *list, int us_too)
{
    ADDRESS *a, *b;
    TA_S    *ta, *tb;
    int eliminated = 0;

    /* toss dupes */
    for(ta = list; ta; ta = ta->next){

	if(ta->skip_it || ta->frwrded) /* already tossed or forwarded */
	  continue;

	a = ta->addr;

	/* Check addr "a" versus tail of the list */
	for(tb = ta->next; tb; tb = tb->next){
	    if(tb->skip_it || tb->frwrded) /* already tossed or forwarded */
	      continue;

	    b = tb->addr;
	    if(dup_addrs(a, b)){
		if(ta->checked || !(tb->checked)){
		    /* throw out b */
		    if(tb->checked)
		      eliminated++;

		    tb->skip_it = 1;
		}
		else{ /* tb->checked */
		    /* throw out a */
		    ta->skip_it = 1;
		    break;
		}
	    }
	}
    }

    if(us_too){
	/* check whether all remaining addrs are us */
	for(ta = list; ta; ta = ta->next){

	    if(ta->skip_it) /* already tossed */
	      continue;

	    if(ta->frwrded) /* forwarded entry, so not us */
	      break;

	    a = ta->addr;

	    if(!address_is_us(a, ps_global))
	      break;
	}

	/*
	 * if at least one address that isn't us, remove all of us from
	 * the list
	 */
	if(ta){
	    for(ta = list; ta; ta = ta->next){

		if(ta->skip_it || ta->frwrded) /* already tossed or forwarded */
		  continue;

		a = ta->addr;

		if(address_is_us(a, ps_global)){
		    if(ta->checked)
		      eliminated++;

		    /* throw out a */
		    ta->skip_it = 1;
		}
	    }
	}
    }

    return(eliminated);
}


/*
 * Returns 1 if x and y match, 0 if not.
 */
int
dup_addrs(struct mail_address *x, struct mail_address *y)
{
    return(x && y && strucmp(x->mailbox, y->mailbox) == 0
           &&  strucmp(x->host, y->host) == 0);
}
