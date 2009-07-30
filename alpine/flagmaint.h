/*
 * $Id: flagmaint.h 807 2007-11-09 01:21:33Z hubert@u.washington.edu $
 *
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

#ifndef PINE_FLAGMAINT_INCLUDED
#define PINE_FLAGMAINT_INCLUDED


#include "../pith/state.h"


/*
 * Structures to control flag maintenance screen
 */
struct flag_table {
    char     *name;		/* flag's name or keyword's nickname */
    HelpType  help;		/* help text */
    long      flag;		/* flag tag (i.e., F_DEL ) */
    unsigned  set:2;		/* its state (set, unset, unknown) */
    unsigned  ukn:1;		/* allow unknown state */
    char     *keyword;		/* only for user-defined, could be same as name */
    char     *comment;		/* comment about the name, keyword name if there is a nick */
};


struct flag_screen {
    char	      **explanation;
    struct flag_table **flag_table;
};

/*
 * Some defs to help keep flag setting straight...
 */
#define	CMD_FLAG_CLEAR	FALSE
#define	CMD_FLAG_SET	TRUE
#define	CMD_FLAG_UNKN	2


/* exported protoypes */
int	flag_maintenance_screen(struct pine *, struct flag_screen *);


#endif /* PINE_FLAGMAINT_INCLUDED */
