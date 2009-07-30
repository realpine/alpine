/*
 * $Id: addrbook.h 1012 2008-03-26 00:44:22Z hubert@u.washington.edu $
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

#ifndef PINE_ADDRBOOK_INCLUDED
#define PINE_ADDRBOOK_INCLUDED


#include "../pith/adrbklib.h"
#include "../pith/state.h"


/*
 * Flags to abook_nickname_complete().
 * ANC_AFTERCOMMA means the passed in prefix
 * looks like "stuff, stuff, prefix" and
 * we are to peel off the stuff before the prefix,
 * look for matches, then put the stuff back before
 * returning the answer.
 */
#define ANC_AFTERCOMMA      0x1


/* exported protoypes */
int            cur_is_open(void);
void	       init_disp_form(PerAddrBook *, char **, int);
void	       init_abook_screen(void);
void	       save_and_restore(int, SAVE_STATE_S *);
void           save_state(SAVE_STATE_S *);
void           restore_state(SAVE_STATE_S *);
AdrBk_Entry   *ae(long);
char          *get_abook_display_line(long, int, char **, char **, int *, char *, size_t);
void	       addr_book_screen(struct pine *);
void	       addr_book_config(struct pine *, int);
char	      *addr_book_oneaddr(void);
char	      *addr_book_multaddr_nf(void);
char	      *addr_book_oneaddr_nf(void);
char	      *addr_book_compose(char **);
char	      *addr_book_compose_lcc(char **);
char          *addr_book_change_list(char **);
char	      *addr_book_bounce(void);
char          *addr_book_takeaddr(void);
char          *addr_book_nick_for_edit(char **);
char          *addr_book_selnick(void);
char          *abook_select_screen(struct pine *);
int            abook_nickname_complete(char *, char **, int, unsigned);
int            is_addr(long);
long           first_line(long);
void           ab_resize(void);
int            cur_addr_book(void);
int            resync_screen(PerAddrBook *, AddrBookArg, int);
int            nickname_check(char *, char **);
int            calculate_field_widths(void);
void           erase_selections(void);


#endif /* PINE_ADDRBOOK_INCLUDED */
