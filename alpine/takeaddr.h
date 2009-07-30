/*
 * $Id: takeaddr.h 1012 2008-03-26 00:44:22Z hubert@u.washington.edu $
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

#ifndef PINE_TAKEADDR_INCLUDED
#define PINE_TAKEADDR_INCLUDED


#include "../pith/takeaddr.h"
#include "../pith/state.h"
#include "../pith/adrbklib.h"
#include "../pith/msgno.h"
#include "../pith/mailpart.h"
#include "../pith/ldap.h"


/* exported prototypes */
int	       cmd_take_addr(struct pine *, MSGNO_S *, int);
void           take_this_one_entry(struct pine *, TA_STATE_S **, AdrBk *, long);
void           save_vcard_att(struct pine *, int, long, ATTACH_S *);
void           take_to_export(struct pine *, LINES_TO_TAKE *);
PerAddrBook   *setup_for_addrbook_add(SAVE_STATE_S *, int, char *);
#ifdef	ENABLE_LDAP
void           save_ldap_entry(struct pine *, LDAP_CHOOSE_S *, int);
#endif


#endif /* PINE_TAKEADDR_INCLUDED */
