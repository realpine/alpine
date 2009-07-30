/*
 * $Id: ldapconf.h 1012 2008-03-26 00:44:22Z hubert@u.washington.edu $
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

#ifndef PINE_LDAPCONF_INCLUDED
#define PINE_LDAPCONF_INCLUDED


#include "conftype.h"
#include "../pith/ldap.h"
#include "../pith/state.h"
#include "../pith/bldaddr.h"


/* exported protoypes */
#ifdef	ENABLE_LDAP
void	    directory_config(struct pine *, int);
int	    ldap_radiobutton_tool(struct pine *, int, CONF_S **, unsigned);
#endif


#endif /* PINE_LDAPCONF_INCLUDED */
