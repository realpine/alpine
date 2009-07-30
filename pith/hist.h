/*
 * $Id: hist.h 768 2007-10-24 00:10:03Z hubert@u.washington.edu $
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

#ifndef PITH_HIST_INCLUDED
#define PITH_HIST_INCLUDED


#define HISTSIZE (20+1)

typedef struct one_hist {
    char      *str;
    unsigned   flags;
    void      *cntxt;
} ONE_HIST_S;


typedef struct history_s {
    int         histsize;
    int         origindex;
    int         curindex;
    ONE_HIST_S *hist[1];	/* has size histsize */
} HISTORY_S;


#define HISTORY_UP_KEYNAME  "Up"
#define HISTORY_DOWN_KEYNAME  "Down"
#define HISTORY_KEYLABEL N_("History")


void        init_hist(HISTORY_S **, int);
void        free_hist(HISTORY_S **);
char       *get_prev_hist(HISTORY_S *, char *, unsigned, void *);
char       *get_next_hist(HISTORY_S *, char *, unsigned, void *);
void        save_hist(HISTORY_S *, char *, unsigned, void *);
int         items_in_hist(HISTORY_S *);
void        add_to_histlist(HISTORY_S **);
void        free_histlist(void);


#endif /* PITH_HIST_INCLUDED */
