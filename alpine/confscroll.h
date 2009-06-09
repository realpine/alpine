/*
 * $Id: confscroll.h 136 2006-09-22 20:06:05Z hubert@u.washington.edu $
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

#ifndef PINE_CONFSCROLL_INCLUDED
#define PINE_CONFSCROLL_INCLUDED


#include "conftype.h"
#include "../pith/conf.h"
#include "../pith/state.h"


#define	BODY_LINES(X)	((X)->ttyo->screen_rows -HEADER_ROWS(X)-FOOTER_ROWS(X))

#define	R_SELD				'*'

#define	EXIT_PMT "Commit changes (\"Yes\" replaces settings, \"No\" abandons changes)"


/* another in a long line of hacks in this stuff */
#define DSTRING "default ("
#define VSTRING "value from fcc-name-rule"


#define next_confline(p)  ((p) ? (p)->next : NULL)
#define prev_confline(p)  ((p) ? (p)->prev : NULL)


extern char *empty_val;
extern char *empty_val2;


extern struct variable  *score_act_global_ptr,
			*scorei_pat_global_ptr,
			*age_pat_global_ptr,
			*size_pat_global_ptr,
			*cati_global_ptr,
			*cat_cmd_global_ptr,
			*cat_lim_global_ptr,
			*startup_ptr,
			*role_comment_ptr,
			*role_forw_ptr,
			*role_repl_ptr,
			*role_fldr_ptr,
			*role_afrom_ptr,
			*role_filt_ptr,
			*role_status1_ptr,
			*role_status2_ptr,
			*role_status3_ptr,
			*role_status4_ptr,
			*role_status5_ptr,
			*role_status6_ptr,
			*role_status7_ptr,
			*role_status8_ptr,
			*msg_state1_ptr,
			*msg_state2_ptr,
			*msg_state3_ptr,
			*msg_state4_ptr;


extern OPT_SCREEN_S *opt_screen;


extern EditWhich ew;


/* exported protoypes */
int	 conf_scroll_screen(struct pine *, OPT_SCREEN_S *, CONF_S *, char *, char *, int);
void     standard_radio_setup(struct pine *, CONF_S **, struct variable *, CONF_S **);
int      standard_radio_var(struct pine *, struct variable *);
int      delete_user_vals(struct variable *);
CONF_S	*new_confline(CONF_S **);
CONF_S	*first_confline(CONF_S *);
CONF_S  *first_sel_confline(CONF_S *);
char    *pretty_value(struct pine *, CONF_S *);
SAVED_CONFIG_S *save_config_vars(struct pine *, int);
int      text_tool(struct pine *, int, CONF_S **, unsigned);
int	 checkbox_tool(struct pine *, int, CONF_S **, unsigned);
int	 radiobutton_tool(struct pine *, int, CONF_S **, unsigned);
int	 yesno_tool(struct pine *, int, CONF_S **, unsigned);
char    *generalized_sort_pretty_value(struct pine *, CONF_S *, int);


#endif /* PINE_CONFSCROLL_INCLUDED */
