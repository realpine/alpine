#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: state.c 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $";
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
     state.c
     Implements the Pine state management routines
  ====*/


#include "../pith/headers.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/init.h"
#include "../pith/sort.h"
#include "../pith/atttype.h"
#include "../pith/util.h"
#include "../pith/mailindx.h"
#include "../pith/remote.h"
#include "../pith/list.h"
#include "../pith/smime.h"


/*
 * Globals referenced throughout pine...
 */
struct pine *ps_global;				/* THE global variable! */

#ifdef	DEBUG
/*
 * Debug level and output file defined here, referenced globally.
 * The debug file is opened and initialized below...
 */
int          debug        = DEFAULT_DEBUG;
#endif


/*----------------------------------------------------------------------
  General use big buffer. It is used in the following places:
    compose_mail:    while parsing header of postponed message
    append_message2: while writing header into folder
    q_status_messageX: while doing printf formatting
    addr_book: Used to return expanded address in. (Can only use here 
               because mm_log doesn't q_status on PARSE errors !)
    alpine.c: When address specified on command line
    init.c: When expanding variable values
    and many many more...

 ----*/
char         tmp_20k_buf[SIZEOF_20KBUF];


/*
 * new_pine_struct - allocate and fill in with default values a new pine struct
 */
struct pine *
new_pine_struct(void)
{
    struct pine *p;

    p		       = (struct pine *)fs_get(sizeof (struct pine));
    memset((void *) p, 0, sizeof(struct pine));
    p->def_sort        = SortArrival;
    p->sort_types[0]   = SortSubject;
    p->sort_types[1]   = SortArrival;
    p->sort_types[2]   = SortFrom;
    p->sort_types[3]   = SortTo;
    p->sort_types[4]   = SortCc;
    p->sort_types[5]   = SortDate;
    p->sort_types[6]   = SortSize;
    p->sort_types[7]   = SortSubject2;
    p->sort_types[8]   = SortScore;
    p->sort_types[9]   = SortThread;
    p->sort_types[10]  = EndofList;
#ifdef SMIME
    /*
     * We need to have access to p->smime even before calling
     * smime_init() so that we can set do_encrypt and do_sign.
     */
    p->smime           = new_smime_struct();
#endif /* SMIME */
    p->atmts           = (ATTACH_S *) fs_get(sizeof(ATTACH_S));
    p->atmts_allocated = 1;
    p->atmts->description = NULL;
    p->low_speed       = 1;
    p->init_context    = -1;
    msgno_init(&p->msgmap, 0L, SortArrival, 0);
    init_init_vars(p);

    return(p);
}



/*
 * free_pine_struct -- free up allocated data in pine struct and then the
 *		       struct itself
 */
void
free_pine_struct(struct pine **pps)
{ 
    if(!(pps && (*pps)))
      return;

    if((*pps)->hostname != NULL)
      fs_give((void **)&(*pps)->hostname);

    if((*pps)->localdomain != NULL)
      fs_give((void **)&(*pps)->localdomain);

    if((*pps)->ttyo != NULL)
      fs_give((void **)&(*pps)->ttyo);

    if((*pps)->home_dir != NULL)
      fs_give((void **)&(*pps)->home_dir);

    if((*pps)->folders_dir != NULL)
      fs_give((void **)&(*pps)->folders_dir);

    if((*pps)->ui.homedir)
      fs_give((void **)&(*pps)->ui.homedir);

    if((*pps)->ui.login)
      fs_give((void **)&(*pps)->ui.login);

    if((*pps)->ui.fullname)
      fs_give((void **)&(*pps)->ui.fullname);

    free_index_format(&(*pps)->index_disp_format);

    if((*pps)->conv_table){
	if((*pps)->conv_table->table)
	  fs_give((void **) &(*pps)->conv_table->table);
	
	if((*pps)->conv_table->from_charset)
	  fs_give((void **) &(*pps)->conv_table->from_charset);
	
	if((*pps)->conv_table->to_charset)
	  fs_give((void **) &(*pps)->conv_table->to_charset);

	fs_give((void **)&(*pps)->conv_table);
    }

    if((*pps)->pinerc)
      fs_give((void **)&(*pps)->pinerc);

#if defined(DOS) || defined(OS2)
    if((*pps)->pine_dir)
      fs_give((void **)&(*pps)->pine_dir);

    if((*pps)->aux_files_dir)
      fs_give((void **)&(*pps)->aux_files_dir);
#endif

    if((*pps)->display_charmap)
      fs_give((void **)&(*pps)->display_charmap);

    if((*pps)->keyboard_charmap)
      fs_give((void **)&(*pps)->keyboard_charmap);

    if((*pps)->posting_charmap)
      fs_give((void **)&(*pps)->posting_charmap);

#ifdef PASSFILE
    if((*pps)->passfile)
      fs_give((void **)&(*pps)->passfile);
#endif /* PASSFILE */

    if((*pps)->hdr_colors)
      free_spec_colors(&(*pps)->hdr_colors);

    if((*pps)->keywords)
      free_keyword_list(&(*pps)->keywords);

    if((*pps)->kw_colors)
      free_spec_colors(&(*pps)->kw_colors);

    if((*pps)->atmts){
	int i;

	for(i = 0; (*pps)->atmts[i].description; i++){
	    fs_give((void **) &(*pps)->atmts[i].description);
	    fs_give((void **) &(*pps)->atmts[i].number);
	}

	fs_give((void **) &(*pps)->atmts);
    }

    if((*pps)->msgmap)
      msgno_give(&(*pps)->msgmap);
    
    free_vars(*pps);

    fs_give((void **) pps);
}


void
free_pinerc_strings(struct pine **pps)
{
    if((*pps)->prc){
	if((*pps)->prc->outstanding_pinerc_changes)
	  write_pinerc((*pps), Main, WRP_NONE);

	if((*pps)->prc->rd)
	  rd_close_remdata(&(*pps)->prc->rd);
	
	free_pinerc_s(&(*pps)->prc);
    }

    if((*pps)->pconf)
      free_pinerc_s(&(*pps)->pconf);

    if((*pps)->post_prc){
	if((*pps)->post_prc->outstanding_pinerc_changes)
	  write_pinerc((*pps), Post, WRP_NONE);

	if((*pps)->post_prc->rd)
	  rd_close_remdata(&(*pps)->post_prc->rd);
	
	free_pinerc_s(&(*pps)->post_prc);
    }
}


/*
 * free_vars -- give back resources acquired when we defined the
 *		variables list
 */
void
free_vars(struct pine *ps)
{
    register int i;

    for(i = 0; ps && i <= V_LAST_VAR; i++)
      free_variable_values(&ps->vars[i]);
}


void
free_variable_values(struct variable *var)
{
    if(var){
	if(var->is_list){
	    free_list_array(&var->current_val.l);
	    free_list_array(&var->main_user_val.l);
	    free_list_array(&var->post_user_val.l);
	    free_list_array(&var->global_val.l);
	    free_list_array(&var->fixed_val.l);
	    free_list_array(&var->cmdline_val.l);
	}
	else{
	    if(var->current_val.p)
	      fs_give((void **)&var->current_val.p);
	    if(var->main_user_val.p)
	      fs_give((void **)&var->main_user_val.p);
	    if(var->post_user_val.p)
	      fs_give((void **)&var->post_user_val.p);
	    if(var->global_val.p)
	      fs_give((void **)&var->global_val.p);
	    if(var->fixed_val.p)
	      fs_give((void **)&var->fixed_val.p);
	    if(var->cmdline_val.p)
	      fs_give((void **)&var->cmdline_val.p);
	}
    }
}


PINERC_S *
new_pinerc_s(char *name)
{
    PINERC_S *prc = NULL;

    if(name){
	prc = (PINERC_S *)fs_get(sizeof(*prc));
	memset((void *)prc, 0, sizeof(*prc));
	prc->name = cpystr(name);
	if(IS_REMOTE(name))
	  prc->type = RemImap;
	else
	  prc->type = Loc;
    }

    return(prc);
}


void
free_pinerc_s(PINERC_S **prc)
{
    if(prc && *prc){
	if((*prc)->name)
	  fs_give((void **)&(*prc)->name);

	if((*prc)->rd)
	  rd_free_remdata(&(*prc)->rd);

	if((*prc)->pinerc_lines)
	  free_pinerc_lines(&(*prc)->pinerc_lines);

	fs_give((void **)prc);
    }
}
