#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: addrbook.c 90 2006-07-19 22:30:36Z hubert@u.washington.edu $";
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
    addrbook.c
    display format/support routines
 ====*/

#include "../c-client/c-client.h"

#include <system.h>
#include <general.h>

#include "addrbook.h"
#include "state.h"
#include "adrbklib.h"
#include "abdlc.h"
#include "conf.h"
#include "conftype.h"
#include "status.h"
#include "debug.h"
#include "osdep/collate.h"


/* internal prototypes */
void           parse_format(char *, COL_S *);




/*
 * We have to call this to set up the format of the columns. There is a
 * separate format for each addrbook, so we need to call this for each
 * addrbook. We call it when the pab's are built. It also depends on
 * whether or not as.checkboxes is set, so if we go into a Select mode
 * from the address book maintenance screen we need to re-call this. Since
 * we can't go back out of ListMode we don't have that problem. Restore_state
 * has to call it because of the as.checkboxes possibly being different in
 * the two states.
 */
void
addrbook_new_disp_form(PerAddrBook *pab, char **list, int addrbook_num,
		       int (*prefix_f)(PerAddrBook *, int *))
{
    char *last_one;
    int   column = 0;

    dprint((9, "- init_disp_form(%s) -\n",
	   (pab && pab->abnick) ? pab->abnick : "?"));

    memset((void *)pab->disp_form, 0, NFIELDS*sizeof(COL_S));
    pab->disp_form[1].wtype = WeCalculate; /* so we don't get false AllAuto */

    if(prefix_f)
      as.do_bold = (*prefix_f)(pab, &column);

    /* if custom format is specified */
    if(list && list[0] && list[0][0]){
	/* find the one for addrbook_num */
	for(last_one = *list;
	    *list != NULL && addrbook_num;
	    addrbook_num--,list++)
	  last_one = *list;

	/* If not enough to go around, last one repeats */
	if(*list == NULL)
	  parse_format(last_one, &(pab->disp_form[column]));
	else
	  parse_format(*list, &(pab->disp_form[column]));
    }
    else{  /* default */
	/* If 2nd wtype is AllAuto, the widths are calculated old way */
	pab->disp_form[1].wtype   = AllAuto;

	pab->disp_form[column++].type  = Nickname;
	pab->disp_form[column++].type  = Fullname;
	pab->disp_form[column++].type  = Addr;
	/* Fill in rest */
	while(column < NFIELDS)
	  pab->disp_form[column++].type = Notused;
    }
}


struct parse_tokens {
    char *name;
    ColumnType ctype;
};

struct parse_tokens ptokens[] = {
    {"NICKNAME", Nickname},
    {"FULLNAME", Fullname},
    {"ADDRESS",  Addr},
    {"FCC",      Filecopy},
    {"COMMENT",  Comment},
    {"DEFAULT",  Def},
    {NULL,       Notused}
};

/*
 * Parse format_str and fill in disp_form structure based on what's there.
 *
 * Args: format_str -- The format string from pinerc.
 *        disp_form -- This is where we fill in the answer.
 *
 * The format string consists of special tokens which give the order of
 * the columns to be displayed.  The possible tokens are NICKNAME,
 * FULLNAME, ADDRESS, FCC, COMMENT.  If a token is followed by
 * parens with an integer inside (FULLNAME(16)) then that means we
 * make that variable that many characters wide.  If it is a percentage, we
 * allocate that percentage of the columns to that variable.  If no
 * parens, that means we calculate it for the user.  The tokens are
 * delimited by white space.  A token of DEFAULT means to calculate the
 * whole thing as we would if no spec was given.  This makes it possible
 * to specify default for one addrbook and something special for another.
 */
void
parse_format(char *format_str, COL_S *disp_form)
{
    int column = 0;
    char *p, *q;
    struct parse_tokens *pt;
    int nicknames, fullnames, addresses, not_allauto;
    int warnings = 0;

    p = format_str;
    while(p && *p && column < NFIELDS){
	p = skip_white_space(p);	/* space for next word */
    
	/* look for the ptoken this word matches */
	for(pt = ptokens; pt->name; pt++)
	    if(!struncmp(pt->name, p, strlen(pt->name)))
	      break;
	
	/* ignore unrecognized word */
	if(!pt->name){
	    char *r;

	    if((r=strindex(p, SPACE)) != NULL)
	      *r = '\0';

	    dprint((2, "parse_format: ignoring unrecognized word \"%s\" in address-book-formats\n", p ? p : "?"));
	    q_status_message1(SM_ORDER, warnings++==0 ? 1 : 0, 4,
		/* TRANSLATORS: an informative error message */
		_("Ignoring unrecognized word \"%s\" in Address-Book-Formats"), p);
	    /* put back space */
	    if(r)
	      *r = SPACE;

	    /* skip unrecognized word */
	    while(p && *p && !isspace((unsigned char)(*p)))
	      p++;

	    continue;
	}

	disp_form[column].type = pt->ctype;

	/* skip over name and look for parens */
	p += strlen(pt->name);
	if(*p == '('){
	    p++;
	    q = p;
	    while(p && *p && isdigit((unsigned char)*p))
	      p++;
	    
	    if(p && *p && *p == ')' && p > q){
		disp_form[column].wtype = Fixed;
		disp_form[column].req_width = atoi(q);
	    }
	    else if(p && *p && *p == '%' && p > q){
		disp_form[column].wtype = Percent;
		disp_form[column].req_width = atoi(q);
	    }
	    else{
		disp_form[column].wtype = WeCalculate;
		if(disp_form[column].type == Nickname)
		  disp_form[column].req_width = 8;
		else
		  disp_form[column].req_width = 3;
	    }
	}
	else{
	    disp_form[column].wtype     = WeCalculate;
	    if(disp_form[column].type == Nickname)
	      disp_form[column].req_width = 8;
	    else
	      disp_form[column].req_width = 3;
	}

	if(disp_form[column].type == Def){
	    /* If any type is DEFAULT, the widths are calculated old way */
assign_default:
	    column = 0;

	    disp_form[column].wtype  = AllAuto;
	    disp_form[column++].type = Nickname;
	    disp_form[column].wtype  = AllAuto;
	    disp_form[column++].type = Fullname;
	    disp_form[column].wtype  = AllAuto;
	    disp_form[column++].type = Addr;
	    /* Fill in rest */
	    while(column < NFIELDS)
	      disp_form[column++].type = Notused;

	    return;
	}

	column++;
	/* skip text at end of word */
	while(p && *p && !isspace((unsigned char)(*p)))
	  p++;
    }

    if(column == 0){
	q_status_message(SM_ORDER, 0, 4,
	_("Address-Book-Formats has no recognizable words, using default format"));
	goto assign_default;
    }

    /* Fill in rest */
    while(column < NFIELDS)
      disp_form[column++].type = Notused;

    /* check to see if user is just re-ordering default fields */
    nicknames = 0;
    fullnames = 0;
    addresses = 0;
    not_allauto = 0;
    for(column = 0; column < NFIELDS; column++){
	if(disp_form[column].type != Notused
	   && disp_form[column].wtype != WeCalculate)
	  not_allauto++;

	switch(disp_form[column].type){
	  case Nickname:
	    nicknames++;
	    break;

	  case Fullname:
	    fullnames++;
	    break;

	  case Addr:
	    addresses++;
	    break;

	  case Filecopy:
	  case Comment:
	    not_allauto++;
	    break;

	  default:
	    break;
	}
    }

    /*
     * Special case: if there is no address field specified, we put in
     * a special field called WhenNoAddrDisplayed, which causes list
     * entries to be displayable in all cases.
     */
    if(!addresses){
	for(column = 0; column < NFIELDS; column++)
	  if(disp_form[column].type == Notused)
	    break;
	
	if(column < NFIELDS){
	    disp_form[column].type  = WhenNoAddrDisplayed;
	    disp_form[column].wtype = Special;
	}
    }

    if(nicknames == 1 && fullnames == 1 && addresses == 1 && not_allauto == 0)
      disp_form[0].wtype = AllAuto; /* set to do default widths */
}


/*
 * Find the first selectable line greater than or equal to line.  That is,
 * the first line the cursor is allowed to start on.
 * (If there are none >= line, it will find the highest one.)
 *
 * Returns the line number of the found line or NO_LINE if there isn't one.
 */
long
first_selectable_line(long int line)
{
    long lineno;
    register PerAddrBook *pab;
    int i;

    /* skip past non-selectable lines */
    for(lineno=line;
	!line_is_selectable(lineno) && dlist(lineno)->type != End;
	lineno++)
	;/* do nothing */

    if(line_is_selectable(lineno))
      return(lineno);

    /*
     * There were no selectable lines from lineno on down.  Trying looking
     * back up the list.
     */
    for(lineno=line-1;
	!line_is_selectable(lineno) && dlist(lineno)->type != Beginning;
	lineno--)
	;/* do nothing */

    if(line_is_selectable(lineno))
      return(lineno);

    /*
     * No selectable lines at all.
     * If some of the addrbooks are still not displayed, it is too
     * early to set the no_op_possbl flag.  Or, if some of the addrbooks
     * are empty but writable, then we should not set it either.
     */
    for(i = 0; i < as.n_addrbk; i++){
	pab = &as.adrbks[i];
	if(pab->ostatus != Open &&
	   pab->ostatus != HalfOpen &&
	   pab->ostatus != ThreeQuartOpen)
	  return NO_LINE;

	if(pab->access == ReadWrite && adrbk_count(pab->address_book) == 0)
	  return NO_LINE;
    }

    as.no_op_possbl++;
    return NO_LINE;
}


/*
 * Returns 1 if this line is of a type that can have a cursor on it.
 */
int
line_is_selectable(long int lineno)
{
    register AddrScrn_Disp *dl;

    if((dl = dlist(lineno)) && (dl->type == Text      ||
				dl->type == ListEmpty ||
				dl->type == TitleCmb  ||
				dl->type == Beginning ||
				dl->type == End)){

	return 0;
    }

    return 1;
}
