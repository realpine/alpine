/*
 * $Id: takeaddr.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
 *
 * ========================================================================
 * Copyright 2006 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */

#ifndef PITH_TAKEADDR_INCLUDED
#define PITH_TAKEADDR_INCLUDED


#include "../pith/adrbklib.h"
#include "../pith/msgno.h"
#include "../pith/state.h"
#include "../pith/store.h"


/*
 * Information used to paint and maintain a line on the TakeAddr screen
 */
typedef struct takeaddr_line {
    unsigned int	  checked:1;/* addr is selected                     */
    unsigned int	  skip_it:1;/* skip this one                        */
    unsigned int	  print:1;  /* for printing only                    */
    unsigned int	  frwrded:1;/* forwarded from another pine          */
    char                 *strvalue; /* alloc'd value string                 */
    ADDRESS              *addr;     /* original ADDRESS this line came from */
    char                 *nickname; /* The first TA may carry this extra    */
    char                 *fullname; /*   information, or,                   */
    char                 *fcc;      /* all vcard ta's have it.              */
    char                 *comment;
    struct takeaddr_line *next, *prev;
} TA_S;

typedef struct a_list {
    int          dup;
    adrbk_cntr_t dst_enum;
    TA_S        *ta;
} SWOOP_S;

typedef struct lines_to_take {
    char *printval;
    char *exportval;
    int   flags;
    struct lines_to_take *next, *prev;
} LINES_TO_TAKE;

#define	LT_NONE		0x00
#define	LT_NOSELECT	0x01


/*
 * Flag definitions to control takeaddr behavior
 */
#define TA_NONE       0
#define TA_AGG        1    /* if set, use the aggregate operation */
#define TA_NOPROMPT   2    /* if set, we aren't in interactive mode */


/* these should really be in ../pine */
#define	RS_NONE		0x00		/* rule_setup_type flags	*/
#define	RS_RULES	0x01		/* include Rules as option	*/
#define	RS_INCADDR	0x02		/* include Addrbook as option	*/
#define	RS_INCEXP	0x04		/* include Export as option	*/
#define	RS_INCFILTNOW	0x08		/* filter now			*/


/* exported prototypes */
int            set_up_takeaddr(int, struct pine *, MSGNO_S *, TA_S **, 
			       int *, int, int (*)(TA_S *, int));
TA_S          *pre_sel_taline(TA_S *);
TA_S          *pre_taline(TA_S *);
TA_S          *next_sel_taline(TA_S *);
TA_S          *next_taline(TA_S *);
int            ta_mark_all(TA_S *);
int            is_talist_of_one(TA_S *);
int            ta_unmark_all(TA_S *);
TA_S          *new_taline(TA_S **);
void           free_taline(TA_S **);
void           free_talines(TA_S **);
void           free_ltline(LINES_TO_TAKE **);
void           free_ltlines(LINES_TO_TAKE **);
TA_S          *first_sel_taline(TA_S *);
TA_S          *last_sel_taline(TA_S *);
TA_S          *first_taline(TA_S *);
TA_S          *first_checked(TA_S *);
char         **list_of_checked(TA_S *);
int            convert_ta_to_lines(TA_S *, LINES_TO_TAKE **);
LINES_TO_TAKE *new_ltline(LINES_TO_TAKE **);
int            add_addresses_to_talist(struct pine *, long, char *, TA_S **, ADDRESS *, int);
int            process_vcard_atts(MAILSTREAM *, long, BODY *, BODY *, char *, TA_S **);
int            cmp_swoop_list(const qsort_t *, const qsort_t *);
int            vcard_to_ta(char *, char *, char *, char *, char *, char *, TA_S **);
char          *getaltcharset(char *, char **, char **, int *);
void           switch_to_last_comma_first(char *, char *, size_t);
char         **detach_vcard_att(MAILSTREAM *, long, BODY *, char *);
int	       grab_addrs_from_body(MAILSTREAM *,long,BODY *,TA_S **);
int            get_line_of_message(STORE_S *, char *, int);
TA_S          *fill_in_ta(TA_S **, ADDRESS *, int, char *);
int            eliminate_dups_and_us(TA_S *);
int            eliminate_dups_but_not_us(TA_S *);
int            eliminate_dups_and_maybe_us(TA_S *, int);
int            dup_addrs(ADDRESS *, ADDRESS *);


#endif /* PITH_TAKEADDR_INCLUDED */
