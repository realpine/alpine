#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: pattern.c 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $";
#endif
/*
 * ========================================================================
 * Copyright 2006-2009 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */

#include "../pith/headers.h"
#include "../pith/pattern.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/string.h"
#include "../pith/msgno.h"
#include "../pith/status.h"
#include "../pith/list.h"
#include "../pith/flag.h"
#include "../pith/tempfile.h"
#include "../pith/addrstring.h"
#include "../pith/search.h"
#include "../pith/mailcmd.h"
#include "../pith/filter.h"
#include "../pith/save.h"
#include "../pith/mimedesc.h"
#include "../pith/reply.h"
#include "../pith/folder.h"
#include "../pith/maillist.h"
#include "../pith/sort.h"
#include "../pith/copyaddr.h"
#include "../pith/pipe.h"
#include "../pith/list.h"
#include "../pith/news.h"
#include "../pith/util.h"
#include "../pith/sequence.h"
#include "../pith/detoken.h"
#include "../pith/busy.h"
#include "../pith/indxtype.h"
#include "../pith/mailindx.h"
#include "../pith/send.h"
#include "../pith/icache.h"
#include "../pith/ablookup.h"
#include "../pith/keyword.h"


/*
 * Internal prototypes
 */
void        open_any_patterns(long);
void        sub_open_any_patterns(long);
void        sub_close_patterns(long);
int         sub_any_patterns(long, PAT_STATE *);
PAT_LINE_S *parse_pat_lit(char *);
PAT_LINE_S *parse_pat_inherit(void);
PAT_S      *parse_pat(char *);
void        parse_patgrp_slash(char *, PATGRP_S *);
void        parse_action_slash(char *, ACTION_S *);
ARBHDR_S   *parse_arbhdr(char *);
char       *next_arb(char *);
PAT_S      *first_any_pattern(PAT_STATE *);
PAT_S      *last_any_pattern(PAT_STATE *);
PAT_S      *prev_any_pattern(PAT_STATE *);
PAT_S      *next_any_pattern(PAT_STATE *);
int         sub_write_patterns(long);
int         write_pattern_file(char **, PAT_LINE_S *);
int         write_pattern_lit(char **, PAT_LINE_S *);
int         write_pattern_inherit(char **, PAT_LINE_S *);
char       *data_for_patline(PAT_S *);
int         charsets_present_in_msg(MAILSTREAM *, unsigned long, STRLIST_S *);
void        collect_charsets_from_subj(ENVELOPE *, STRLIST_S **);
void        collect_charsets_from_body(BODY *, STRLIST_S **);
SEARCHPGM  *next_not(SEARCHPGM *);
SEARCHOR   *next_or(SEARCHOR **);
void        set_up_search_pgm(char *, PATTERN_S *, SEARCHPGM *);
void        add_type_to_pgm(char *, PATTERN_S *, SEARCHPGM *);
void        set_srch(char *, char *, SEARCHPGM *);
void        set_srch_hdr(char *, char *, SEARCHPGM *);
void        set_search_by_age(INTVL_S *, SEARCHPGM *, int);
void        set_search_by_size(INTVL_S *, SEARCHPGM *);
int	    non_eh(char *);
void        add_eh(char **, char **, char *, int *);
void        set_extra_hdrs(char *);
int         is_ascii_string(char *);
ACTION_S   *combine_inherited_role_guts(ACTION_S *);
int	    move_filtered_msgs(MAILSTREAM *, MSGNO_S *, char *, int, char *);
void        set_some_flags(MAILSTREAM *, MSGNO_S *, long, char **, char **, int, char *);


/*
 *  optional hook for external-program filter test
 */
void (*pith_opt_filter_pattern_cmd)(char **, SEARCHSET *, MAILSTREAM *, long, INTVL_S *);


void
role_process_filters(void)
{
    int         i;
    MAILSTREAM *stream;
    MSGNO_S    *msgmap;

    for(i = 0; i < ps_global->s_pool.nstream; i++){
	stream = ps_global->s_pool.streams[i];
	if(stream && pine_mail_ping(stream)){
	    msgmap = sp_msgmap(stream);
	    if(msgmap)
	      reprocess_filter_patterns(stream, msgmap, MI_REFILTERING);
	}
    }
}


int
add_to_pattern(PAT_S *pat, long int rflags)
{
    PAT_LINE_S *new_patline, *patline;
    PAT_S      *new_pat;
    PAT_STATE   dummy;

    if(!any_patterns(rflags, &dummy))
      return(0);

    /* need a new patline */
    new_patline = (PAT_LINE_S *)fs_get(sizeof(*new_patline));
    memset((void *)new_patline, 0, sizeof(*new_patline));
    new_patline->type = Literal;
    (*cur_pat_h)->dirtypinerc = 1;

    /* and a copy of pat */
    new_pat = copy_pat(pat);

    /* tie together */
    new_patline->first = new_patline->last = new_pat;
    new_pat->patline = new_patline;


    /*
     * Manipulate bits directly in pattern list.
     * Cur_pat_h is set by any_patterns.
     */


    /* find last patline */
    for(patline = (*cur_pat_h)->patlinehead;
	patline && patline->next;
	patline = patline->next)
      ;
    
    /* add new patline to end of list */
    if(patline){
	patline->next = new_patline;
	new_patline->prev = patline;
    }
    else
      (*cur_pat_h)->patlinehead = new_patline;
    
    return(1);
}


/*
 * Does pattern quoting. Takes the string that the user sees and converts
 * it to the config file string.
 *
 * Args: src -- The source string.
 *
 * The last arg to add_escapes causes \, and \\ to be replaced with hex
 * versions of comma and backslash. That's so we can embed commas in
 * list variables without having them act as separators. If the user wants
 * a literal comma, they type backslash comma.
 * If /, \, or " appear (other than the special cases in previous sentence)
 * they are backslash-escaped like \/, \\, or \".
 *
 * Returns: An allocated string with quoting added.
 *
 *   The caller is responsible for freeing the memory allocated for the answer.
 */
char *
add_pat_escapes(char *src)
{
    return(add_escapes(src, "/\\\"", '\\', "", ",\\"));
}


/*
 * Undoes the escape quoting done by add_pat_escapes.
 *
 * Args: src -- The source string.
 *
 * Returns: A string with backslash quoting removed or NULL. The string starts
 *          at src and goes until the end of src or until a / is reached. The
 *          / is not included in the string. /'s may be quoted by preceding
 *          them with a backslash (\) and \'s may also be quoted by
 *          preceding them with a \. In fact, \ quotes any character.
 *          Not quite, \nnn is octal escape, \xXX is hex escape.
 *          Hex escapes are undone but left with a backslash in front.
 *
 *   The caller is responsible for freeing the memory allocated for the answer.
 */
char *
remove_pat_escapes(char *src)
{
    char *ans = NULL, *q, *p;
    int done = 0;

    if(src){
	p = q = (char *)fs_get(strlen(src) + 1);

	while(!done){
	    switch(*src){
	      case '\\':
		src++;
		if(*src){
		    if(isdigit((unsigned char)*src)){	/* octal escape */
			*p++ = '\\';
			*p++ = (char)read_octal(&src);
		    }
		    else if((*src == 'x' || *src == 'X') &&
			    *(src+1) && *(src+2) && isxpair(src+1)){
			*p++ = '\\';
			*p++ = (char)read_hex(src+1);
			src += 3;
		    }
		    else
		      *p++ = *src++;
		}

		break;
	    
	      case '\0':
	      case '/':
		done++;
		break;

	      default:
		*p++ = *src++;
		break;
	    }
	}

	*p = '\0';

	ans = cpystr(q);
	fs_give((void **)&q);
    }

    return(ans);
}


/*
 * This takes envelope data and adds the backslash escapes that the user
 * would have been responsible for adding if editing manually.
 * It just escapes commas and backslashes.
 *
 * Caller must free result.
 */
char *
add_roletake_escapes(char *src)
{
    return(add_escapes(src, ",\\", '\\', "", ""));
}

/*
 * This function only escapes commas.
 */
char *
add_comma_escapes(char *src)
{
    return(add_escapes(src, ",", '\\', "", ""));
}


/*
 * These are the global pattern handles which all of the pattern routines
 * use. Once we open one of these we usually leave it open until exiting
 * pine. The _any versions are only used if we are altering our configuration,
 * the _ne (NonEmpty) versions are used routinely. We open the patterns by
 * calling either nonempty_patterns (normal use) or any_patterns (config).
 *
 * There are eight different pinerc variables which contain patterns. They are
 * patterns-filters2, patterns-roles, patterns-scores2, patterns-indexcolors,
 * patterns-other, and the old patterns, patterns-filters, and patterns-scores.
 * The first five are the active patterns variables and the old variable are
 * kept around so that we can convert old patterns to new. The reason we
 * split it into five separate variables is so that each can independently
 * be controlled by the main pinerc or by the exception pinerc. The reason
 * for the change to filters2 and scores2 was so we could change the semantics
 * of how rules work when there are pieces in the rule that we don't
 * understand. We added a rule to detect 8bitSubjects. So a user might have
 * a filter that deletes messages with 8bitSubjects. The problem was that
 * that same filter in a old patterns-filters pine would match because it
 * would ignore the 8bitSubject part of the pattern and match on the rest.
 * So we changed the semantics so that rules with unknown pieces would be
 * ignored instead of used. We had to change variable names at the same time
 * because we were adding the 8bit thing and the old pines are still out
 * there. Filters and Scores can both be dangerous. Roles, Colors, and Other
 * seem less dangerous so not worth adding a new variable for them.
 *
 * Each of the eight variables has its own handle and status variables below.
 * That means that they operate independently.
 *
 * Looking at just a single one of those variables, it has four possible
 * values. In normal use, we use the current_val of the variable to set
 * up the patterns. We do that by calling nonempty_patterns() with the
 * appropriate rflags. When editing configurations, we have the other two
 * variables to deal with: main_user_val  and post_user_val.
 * We only ever deal with one of those at a time, so we re-use the variables.
 * However, we do sometimes want to deal with one of those and at the same
 * time refer to the current current_val. For example, if we are editing
 * the post or main user_val for the filters variable, we still want
 * to check for new mail. If we find new mail we'll want to call
 * process_filter_patterns which uses the current_val for filter patterns.
 * That means we have to provide for the case where we are using current_val
 * at the same time as we're using one of the user_vals. That's why we have
 * both the _ne variables (NonEmpty) and the _any variables.
 *
 * In any_patterns (and first_pattern...) use_flags may only be set to
 * one value at a time, whereas rflags may be more than one value OR'd together.
 */
PAT_HANDLE	       **cur_pat_h;
static PAT_HANDLE	*pattern_h_roles_ne,    *pattern_h_roles_any,
			*pattern_h_scores_ne,   *pattern_h_scores_any,
			*pattern_h_filts_ne,    *pattern_h_filts_any,
			*pattern_h_filts_cfg,
			*pattern_h_filts_ne,    *pattern_h_filts_any,
			*pattern_h_incol_ne,    *pattern_h_incol_any,
			*pattern_h_other_ne,    *pattern_h_other_any,
			*pattern_h_srch_ne,     *pattern_h_srch_any,
			*pattern_h_oldpat_ne,   *pattern_h_oldpat_any;

/*
 * These contain the PAT_OPEN_MASK open status and the PAT_USE_MASK use status.
 */
static long		*cur_pat_status;
static long	  	 pat_status_roles_ne,    pat_status_roles_any,
			 pat_status_scores_ne,   pat_status_scores_any,
			 pat_status_filts_ne,    pat_status_filts_any,
			 pat_status_incol_ne,    pat_status_incol_any,
			 pat_status_other_ne,    pat_status_other_any,
			 pat_status_srch_ne,     pat_status_srch_any,
			 pat_status_oldpat_ne,   pat_status_oldpat_any,
			 pat_status_oldfilt_ne,  pat_status_oldfilt_any,
			 pat_status_oldscore_ne, pat_status_oldscore_any;

#define SET_PATTYPE(rflags)						\
    set_pathandle(rflags);						\
    cur_pat_status =							\
      ((rflags) & PAT_USE_CURRENT)					\
	? (((rflags) & ROLE_DO_INCOLS) ? &pat_status_incol_ne :		\
	    ((rflags) & ROLE_DO_OTHER)  ? &pat_status_other_ne :	\
	     ((rflags) & ROLE_DO_FILTER) ? &pat_status_filts_ne :	\
	      ((rflags) & ROLE_DO_SCORES) ? &pat_status_scores_ne :	\
	       ((rflags) & ROLE_DO_ROLES)  ? &pat_status_roles_ne :	\
	        ((rflags) & ROLE_DO_SRCH)   ? &pat_status_srch_ne :	\
	         ((rflags) & ROLE_OLD_FILT)  ? &pat_status_oldfilt_ne :	\
	          ((rflags) & ROLE_OLD_SCORE) ? &pat_status_oldscore_ne :\
					         &pat_status_oldpat_ne)	\
	: (((rflags) & ROLE_DO_INCOLS) ? &pat_status_incol_any :	\
	    ((rflags) & ROLE_DO_OTHER)  ? &pat_status_other_any :	\
	     ((rflags) & ROLE_DO_FILTER) ? &pat_status_filts_any :	\
	      ((rflags) & ROLE_DO_SCORES) ? &pat_status_scores_any :	\
	       ((rflags) & ROLE_DO_ROLES)  ? &pat_status_roles_any :	\
	        ((rflags) & ROLE_DO_SRCH)   ? &pat_status_srch_any :	\
	         ((rflags) & ROLE_OLD_FILT)  ? &pat_status_oldfilt_any :\
	          ((rflags) & ROLE_OLD_SCORE) ? &pat_status_oldscore_any:\
					         &pat_status_oldpat_any);
#define CANONICAL_RFLAGS(rflags)	\
    ((((rflags) & (ROLE_DO_ROLES | ROLE_REPLY | ROLE_FORWARD | ROLE_COMPOSE)) \
					? ROLE_DO_ROLES  : 0) |		   \
     (((rflags) & (ROLE_DO_INCOLS | ROLE_INCOL))			   \
					? ROLE_DO_INCOLS : 0) |		   \
     (((rflags) & (ROLE_DO_SCORES | ROLE_SCORE))			   \
					? ROLE_DO_SCORES : 0) |		   \
     (((rflags) & (ROLE_DO_FILTER))					   \
					? ROLE_DO_FILTER : 0) |		   \
     (((rflags) & (ROLE_DO_OTHER))					   \
					? ROLE_DO_OTHER  : 0) |		   \
     (((rflags) & (ROLE_DO_SRCH))					   \
					? ROLE_DO_SRCH   : 0) |		   \
     (((rflags) & (ROLE_OLD_FILT))					   \
					? ROLE_OLD_FILT  : 0) |		   \
     (((rflags) & (ROLE_OLD_SCORE))					   \
					? ROLE_OLD_SCORE : 0) |		   \
     (((rflags) & (ROLE_OLD_PAT))					   \
					? ROLE_OLD_PAT  : 0))

#define SETPGMSTATUS(val,yes,no)	\
    switch(val){			\
      case PAT_STAT_YES:		\
	(yes) = 1;			\
	break;				\
      case PAT_STAT_NO:			\
	(no) = 1;			\
	break;				\
      case PAT_STAT_EITHER:		\
      default:				\
        break;				\
    }

#define SET_STATUS(srchin,srchfor,assignto)				\
    {char *qq, *pp;							\
     int   ii;								\
     NAMEVAL_S *vv;							\
     if((qq = srchstr(srchin, srchfor)) != NULL){			\
	if((pp = remove_pat_escapes(qq+strlen(srchfor))) != NULL){	\
	    for(ii = 0; (vv = role_status_types(ii)); ii++)		\
	      if(!strucmp(pp, vv->shortname)){				\
		  assignto = vv->value;					\
		  break;						\
	      }								\
									\
	    fs_give((void **)&pp);					\
	}								\
     }									\
    }

#define SET_MSGSTATE(srchin,srchfor,assignto)				\
    {char *qq, *pp;							\
     int   ii;								\
     NAMEVAL_S *vv;							\
     if((qq = srchstr(srchin, srchfor)) != NULL){			\
	if((pp = remove_pat_escapes(qq+strlen(srchfor))) != NULL){	\
	    for(ii = 0; (vv = msg_state_types(ii)); ii++)		\
	      if(!strucmp(pp, vv->shortname)){				\
		  assignto = vv->value;					\
		  break;						\
	      }								\
									\
	    fs_give((void **)&pp);					\
	}								\
     }									\
    }

#define PATTERN_N (9)


void
set_pathandle(long int rflags)
{
    cur_pat_h = (rflags & PAT_USE_CURRENT)
		? ((rflags & ROLE_DO_INCOLS) ? &pattern_h_incol_ne :
		    (rflags & ROLE_DO_OTHER)  ? &pattern_h_other_ne :
		     (rflags & ROLE_DO_FILTER) ? &pattern_h_filts_ne :
		      (rflags & ROLE_DO_SCORES) ? &pattern_h_scores_ne :
		       (rflags & ROLE_DO_ROLES)  ? &pattern_h_roles_ne :
		        (rflags & ROLE_DO_SRCH)   ? &pattern_h_srch_ne :
					             &pattern_h_oldpat_ne)
		: ((rflags & PAT_USE_CHANGED)
		 ? &pattern_h_filts_cfg
	         : ((rflags & ROLE_DO_INCOLS) ? &pattern_h_incol_any :
		     (rflags & ROLE_DO_OTHER)  ? &pattern_h_other_any :
		      (rflags & ROLE_DO_FILTER) ? &pattern_h_filts_any :
		       (rflags & ROLE_DO_SCORES) ? &pattern_h_scores_any :
		        (rflags & ROLE_DO_ROLES)  ? &pattern_h_roles_any :
		         (rflags & ROLE_DO_SRCH)   ? &pattern_h_srch_any :
					              &pattern_h_oldpat_any));
}


/*
 * Rflags may be more than one pattern type OR'd together. It also contains
 * the "use" parameter.
 */
void
open_any_patterns(long int rflags)
{
    long canon_rflags;

    dprint((7, "open_any_patterns(0x%x)\n", rflags));

    canon_rflags = CANONICAL_RFLAGS(rflags);

    if(canon_rflags & ROLE_DO_INCOLS)
      sub_open_any_patterns(ROLE_DO_INCOLS | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_DO_FILTER)
      sub_open_any_patterns(ROLE_DO_FILTER | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_DO_OTHER)
      sub_open_any_patterns(ROLE_DO_OTHER  | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_DO_SCORES)
      sub_open_any_patterns(ROLE_DO_SCORES | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_DO_ROLES)
      sub_open_any_patterns(ROLE_DO_ROLES  | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_DO_SRCH)
      sub_open_any_patterns(ROLE_DO_SRCH   | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_OLD_FILT)
      sub_open_any_patterns(ROLE_OLD_FILT  | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_OLD_SCORE)
      sub_open_any_patterns(ROLE_OLD_SCORE | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_OLD_PAT)
      sub_open_any_patterns(ROLE_OLD_PAT   | (rflags & PAT_USE_MASK));
}


/*
 * This should only be called with a single pattern type (plus use flags).
 * We assume that patterns of this type are closed before this is called.
 * This always succeeds unless we run out of memory, in which case fs_get
 * never returns.
 */
void
sub_open_any_patterns(long int rflags)
{
    PAT_LINE_S *patline = NULL, *pl = NULL;
    char      **t = NULL;
    struct variable *var;

    SET_PATTYPE(rflags);

    *cur_pat_h = (PAT_HANDLE *)fs_get(sizeof(**cur_pat_h));
    memset((void *)*cur_pat_h, 0, sizeof(**cur_pat_h));

    if(rflags & ROLE_DO_ROLES)
      var = &ps_global->vars[V_PAT_ROLES];
    else if(rflags & ROLE_DO_FILTER)
      var = &ps_global->vars[V_PAT_FILTS];
    else if(rflags & ROLE_DO_OTHER)
      var = &ps_global->vars[V_PAT_OTHER];
    else if(rflags & ROLE_DO_SCORES)
      var = &ps_global->vars[V_PAT_SCORES];
    else if(rflags & ROLE_DO_INCOLS)
      var = &ps_global->vars[V_PAT_INCOLS];
    else if(rflags & ROLE_DO_SRCH)
      var = &ps_global->vars[V_PAT_SRCH];
    else if(rflags & ROLE_OLD_FILT)
      var = &ps_global->vars[V_PAT_FILTS_OLD];
    else if(rflags & ROLE_OLD_SCORE)
      var = &ps_global->vars[V_PAT_SCORES_OLD];
    else if(rflags & ROLE_OLD_PAT)
      var = &ps_global->vars[V_PATTERNS];

    switch(rflags & PAT_USE_MASK){
      case PAT_USE_CURRENT:
	t = var->current_val.l;
	break;
      case PAT_USE_CHANGED:
	/*
	 * some trickery to only use changed if actually changed.
	 * otherwise, use current_val
	 */
	t = var->is_changed_val ? var->changed_val.l : var->current_val.l;
	break;
      case PAT_USE_MAIN:
	t = var->main_user_val.l;
	break;
      case PAT_USE_POST:
	t = var->post_user_val.l;
	break;
    }

    if(t){
	for(; t[0] && t[0][0]; t++){
	    if(*t && !strncmp("LIT:", *t, 4))
	      patline = parse_pat_lit(*t + 4);
	    else if(*t && !strncmp("FILE:", *t, 5))
	      patline = parse_pat_file(*t + 5);
	    else if(rflags & (PAT_USE_MAIN | PAT_USE_POST) &&
		    patline == NULL && *t && !strcmp(INHERIT, *t))
	      patline = parse_pat_inherit();
	    else
	      patline = NULL;

	    if(patline){
		if(pl){
		    pl->next      = patline;
		    patline->prev = pl;
		    pl = pl->next;
		}
		else{
		    (*cur_pat_h)->patlinehead = patline;
		    pl = patline;
		}
	    }
	    else
	      q_status_message1(SM_ORDER, 0, 3,
				"Invalid patterns line \"%.200s\"", *t);
	}
    }

    *cur_pat_status = PAT_OPENED | (rflags & PAT_USE_MASK);
}


void
close_every_pattern(void)
{
    close_patterns(ROLE_DO_INCOLS | ROLE_DO_FILTER | ROLE_DO_SCORES
		   | ROLE_DO_OTHER | ROLE_DO_ROLES | ROLE_DO_SRCH
		   | ROLE_OLD_FILT | ROLE_OLD_SCORE | ROLE_OLD_PAT
		   | PAT_USE_CURRENT);
    /*
     * Since there is only one set of variables for the other three uses
     * we can just close any one of them. There can only be one open at
     * a time.
     */
    close_patterns(ROLE_DO_INCOLS | ROLE_DO_FILTER | ROLE_DO_SCORES
		   | ROLE_DO_OTHER | ROLE_DO_ROLES | ROLE_DO_SRCH
		   | ROLE_OLD_FILT | ROLE_OLD_SCORE | ROLE_OLD_PAT
		   | PAT_USE_MAIN);
}


/*
 * Can be called with more than one pattern type.
 */
void
close_patterns(long int rflags)
{
    long canon_rflags;

    dprint((7, "close_patterns(0x%x)\n", rflags));

    canon_rflags = CANONICAL_RFLAGS(rflags);

    if(canon_rflags & ROLE_DO_INCOLS)
      sub_close_patterns(ROLE_DO_INCOLS | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_DO_OTHER)
      sub_close_patterns(ROLE_DO_OTHER  | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_DO_FILTER)
      sub_close_patterns(ROLE_DO_FILTER | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_DO_SCORES)
      sub_close_patterns(ROLE_DO_SCORES | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_DO_ROLES)
      sub_close_patterns(ROLE_DO_ROLES  | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_DO_SRCH)
      sub_close_patterns(ROLE_DO_SRCH   | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_OLD_FILT)
      sub_close_patterns(ROLE_OLD_FILT  | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_OLD_SCORE)
      sub_close_patterns(ROLE_OLD_SCORE | (rflags & PAT_USE_MASK));
    if(canon_rflags & ROLE_OLD_PAT)
      sub_close_patterns(ROLE_OLD_PAT   | (rflags & PAT_USE_MASK));
}


/*
 * Can be called with only a single pattern type.
 */
void
sub_close_patterns(long int rflags)
{
    SET_PATTYPE(rflags);

    if(*cur_pat_h != NULL){
	free_patline(&(*cur_pat_h)->patlinehead);
	fs_give((void **)cur_pat_h);
    }

    *cur_pat_status = PAT_CLOSED;

    scores_are_used(SCOREUSE_INVALID);
}


/*
 * Can be called with more than one pattern type.
 * Nonempty always uses PAT_USE_CURRENT (the current_val).
 */
int
nonempty_patterns(long int rflags, PAT_STATE *pstate)
{
    return(any_patterns((rflags & ROLE_MASK) | PAT_USE_CURRENT, pstate));
}


/*
 * Initializes pstate and parses and sets up appropriate pattern variables.
 * May be called with more than one pattern type OR'd together in rflags.
 * Pstate will keep track of that and next_pattern et. al. will increment
 * through all of those pattern types.
 */
int
any_patterns(long int rflags, PAT_STATE *pstate)
{
    int  ret = 0;
    long canon_rflags;

    dprint((7, "any_patterns(0x%x)\n", rflags));

    memset((void *)pstate, 0, sizeof(*pstate));
    pstate->rflags    = rflags;

    canon_rflags = CANONICAL_RFLAGS(pstate->rflags);

    if(canon_rflags & ROLE_DO_INCOLS)
      ret += sub_any_patterns(ROLE_DO_INCOLS, pstate);
    if(canon_rflags & ROLE_DO_OTHER)
      ret += sub_any_patterns(ROLE_DO_OTHER, pstate);
    if(canon_rflags & ROLE_DO_FILTER)
      ret += sub_any_patterns(ROLE_DO_FILTER, pstate);
    if(canon_rflags & ROLE_DO_SCORES)
      ret += sub_any_patterns(ROLE_DO_SCORES, pstate);
    if(canon_rflags & ROLE_DO_ROLES)
      ret += sub_any_patterns(ROLE_DO_ROLES, pstate);
    if(canon_rflags & ROLE_DO_SRCH)
      ret += sub_any_patterns(ROLE_DO_SRCH, pstate);
    if(canon_rflags & ROLE_OLD_FILT)
      ret += sub_any_patterns(ROLE_OLD_FILT, pstate);
    if(canon_rflags & ROLE_OLD_SCORE)
      ret += sub_any_patterns(ROLE_OLD_SCORE, pstate);
    if(canon_rflags & ROLE_OLD_PAT)
      ret += sub_any_patterns(ROLE_OLD_PAT, pstate);

    return(ret);
}


int
sub_any_patterns(long int rflags, PAT_STATE *pstate)
{
    SET_PATTYPE(rflags | (pstate->rflags & PAT_USE_MASK));

    if(*cur_pat_h &&
       (((pstate->rflags & PAT_USE_MASK) == PAT_USE_CURRENT &&
	 (*cur_pat_status & PAT_USE_MASK) != PAT_USE_CURRENT) ||
        ((pstate->rflags & PAT_USE_MASK) != PAT_USE_CURRENT &&
         ((*cur_pat_status & PAT_OPEN_MASK) != PAT_OPENED ||
	  (*cur_pat_status & PAT_USE_MASK) !=
	   (pstate->rflags & PAT_USE_MASK)))))
      close_patterns(rflags | (pstate->rflags & PAT_USE_MASK));
    
    /* open_any always succeeds */
    if(!*cur_pat_h && ((*cur_pat_status & PAT_OPEN_MASK) == PAT_CLOSED))
      open_any_patterns(rflags | (pstate->rflags & PAT_USE_MASK));
    
    if(!*cur_pat_h){		/* impossible */
	*cur_pat_status = PAT_CLOSED;
	return(0);
    }

    /*
     * Opening nonempty can fail. That just means there aren't any
     * patterns of that type.
     */
    if((pstate->rflags & PAT_USE_MASK) == PAT_USE_CURRENT &&
       !(*cur_pat_h)->patlinehead)
      *cur_pat_status = (PAT_OPEN_FAILED | PAT_USE_CURRENT);
       
    return(((*cur_pat_status & PAT_OPEN_MASK) == PAT_OPENED) ? 1 : 0);
}


int
edit_pattern(PAT_S *newpat, int pos, long int rflags)
{
    PAT_S *oldpat;
    PAT_LINE_S *tpatline;
    int i;
    PAT_STATE pstate;

    if(!any_patterns(rflags, &pstate)) return(1);

    for(i = 0, tpatline = (*cur_pat_h)->patlinehead;
	i < pos && tpatline; tpatline = tpatline->next, i++);
    if(i != pos) return(1);
    oldpat = tpatline->first;
    free_pat(&oldpat);
    tpatline->first = tpatline->last = newpat;
    newpat->patline = tpatline;
    tpatline->dirty = 1;

    (*cur_pat_h)->dirtypinerc = 1;
    write_patterns(rflags);

    return(0);
}

int
add_pattern(PAT_S *newpat, long int rflags)
{
    PAT_LINE_S *tpatline, *newpatline;
    PAT_STATE pstate;

    any_patterns(rflags, &pstate);

    for(tpatline = (*cur_pat_h)->patlinehead;
	tpatline && tpatline->next ; tpatline = tpatline->next);
    newpatline = (PAT_LINE_S *)fs_get(sizeof(PAT_LINE_S));
    if(tpatline)
	tpatline->next = newpatline;
    else
      (*cur_pat_h)->patlinehead = newpatline;
    memset((void *)newpatline, 0, sizeof(PAT_LINE_S));
    newpatline->prev = tpatline;
    newpatline->first = newpatline->last = newpat;
    newpatline->type = Literal;
    newpat->patline = newpatline;
    newpatline->dirty = 1;

    (*cur_pat_h)->dirtypinerc = 1;
    write_patterns(rflags);

    return(0);
}

int
delete_pattern(int pos, long int rflags)
{
    PAT_LINE_S *tpatline;
    int i;
    PAT_STATE pstate;

    if(!any_patterns(rflags, &pstate)) return(1);

    for(i = 0, tpatline = (*cur_pat_h)->patlinehead;
	i < pos && tpatline; tpatline = tpatline->next, i++);
    if(i != pos) return(1);

    if(tpatline == (*cur_pat_h)->patlinehead)
      (*cur_pat_h)->patlinehead = tpatline->next;
    if(tpatline->prev) tpatline->prev->next = tpatline->next;
    if(tpatline->next) tpatline->next->prev = tpatline->prev;
    tpatline->prev = NULL;
    tpatline->next = NULL;

    free_patline(&tpatline);

    (*cur_pat_h)->dirtypinerc = 1;
    write_patterns(rflags);

    return(0);
}

int
shuffle_pattern(int pos, int up, long int rflags)
{
    PAT_LINE_S *tpatline, *shufpatline;
    int i;
    PAT_STATE pstate;

    if(!any_patterns(rflags, &pstate)) return(1);

    for(i = 0, tpatline = (*cur_pat_h)->patlinehead;
	i < pos && tpatline; tpatline = tpatline->next, i++);
    if(i != pos) return(1);

    if(up == 1){
	if(tpatline->prev == NULL) return(1);
	shufpatline = tpatline->prev;
	tpatline->prev = shufpatline->prev;
	if(shufpatline->prev)
	  shufpatline->prev->next = tpatline;
	if(tpatline->next)
	  tpatline->next->prev = shufpatline;
	shufpatline->next = tpatline->next;
	shufpatline->prev = tpatline;
	tpatline->next = shufpatline;
	if(shufpatline == (*cur_pat_h)->patlinehead)
	  (*cur_pat_h)->patlinehead = tpatline;
    }
    else if(up == -1){
	if(tpatline->next == NULL) return(1);
	shufpatline = tpatline->next;
	tpatline->next = shufpatline->next;
	if(shufpatline->next)
	  shufpatline->next->prev = tpatline;
	if(tpatline->prev)
	  tpatline->prev->next = shufpatline;
	shufpatline->prev = tpatline->prev;
	shufpatline->next = tpatline;
	tpatline->prev = shufpatline;
	if(tpatline == (*cur_pat_h)->patlinehead)
	  (*cur_pat_h)->patlinehead = shufpatline;
    }
    else return(1);

    shufpatline->dirty = 1;
    tpatline->dirty = 1;

    (*cur_pat_h)->dirtypinerc = 1;
    write_patterns(rflags);

    return(0);
}

PAT_LINE_S *
parse_pat_lit(char *litpat)
{
    PAT_LINE_S *patline;
    PAT_S      *pat;

    patline = (PAT_LINE_S *)fs_get(sizeof(*patline));
    memset((void *)patline, 0, sizeof(*patline));
    patline->type = Literal;


    if((pat = parse_pat(litpat)) != NULL){
	pat->patline   = patline;
	patline->first = pat;
	patline->last  = pat;
    }

    return(patline);
}


/*
 * This always returns a patline even if we can't read the file. The patline
 * returned will say readonly in the worst case and there will be no patterns.
 * If the file doesn't exist, this creates it if possible.
 */
PAT_LINE_S *
parse_pat_file(char *filename)
{
#define BUF_SIZE 5000
    PAT_LINE_S *patline;
    PAT_S      *pat, *p;
    char        path[MAXPATH+1], buf[BUF_SIZE];
    char       *dir, *q;
    FILE       *fp;
    int         ok = 0, some_pats = 0;
    struct variable *vars = ps_global->vars;

    signature_path(filename, path, MAXPATH);

    if(VAR_OPER_DIR && !in_dir(VAR_OPER_DIR, path)){
	q_status_message1(SM_ORDER | SM_DING, 3, 4,
			  "Can't use Roles file outside of %.200s",
			  VAR_OPER_DIR);
	return(NULL);
    }

    patline = (PAT_LINE_S *)fs_get(sizeof(*patline));
    memset((void *)patline, 0, sizeof(*patline));
    patline->type     = File;
    patline->filename = cpystr(filename);
    patline->filepath = cpystr(path);

    if((q = last_cmpnt(path)) != NULL){
	int save;

	save = *--q;
	*q = '\0';
	dir  = cpystr(*path ? path : "/");
	*q = save;
    }
    else
      dir = cpystr(".");

#if	defined(DOS) || defined(OS2)
    /*
     * If the dir has become a drive letter and : (e.g. "c:")
     * then append a "\".  The library function access() in the
     * win 16 version of MSC seems to require this.
     */
    if(isalpha((unsigned char) *dir)
       && *(dir+1) == ':' && *(dir+2) == '\0'){
	*(dir+2) = '\\';
	*(dir+3) = '\0';
    }
#endif	/* DOS || OS2 */

    /*
     * Even if we can edit the file itself, we aren't going
     * to be able to change it unless we can also write in
     * the directory that contains it (because we write into a
     * temp file and then rename).
     */
    if(can_access(dir, EDIT_ACCESS) != 0)
      patline->readonly = 1;

    if(can_access(path, EDIT_ACCESS) == 0){
	if(patline->readonly)
	  q_status_message1(SM_ORDER, 0, 3,
			    "Pattern file directory (%.200s) is ReadOnly", dir);
    }
    else if(can_access(path, READ_ACCESS) == 0)
      patline->readonly = 1;

    if(can_access(path, ACCESS_EXISTS) == 0){
	if((fp = our_fopen(path, "rb")) != NULL){
	    /* Check to see if this is a valid patterns file */
	    if(fp_file_size(fp) <= 0L)
	      ok++;
	    else{
		size_t len;

		len = strlen(PATTERN_MAGIC);
	        if(fread(buf, sizeof(char), len+3, fp) == len+3){
		    buf[len+3] = '\0';
		    buf[len] = '\0';
		    if(strcmp(buf, PATTERN_MAGIC) == 0){
			if(atoi(PATTERN_FILE_VERS) < atoi(buf + len + 1))
			  q_status_message1(SM_ORDER, 0, 4,
  "Pattern file \"%.200s\" is made by newer Alpine, will try to use it anyway",
					    filename);

			ok++;
			some_pats++;
			/* toss rest of first line */
			(void)fgets(buf, BUF_SIZE, fp);
		    }
		}
	    }
		
	    if(!ok){
		patline->readonly = 1;
		q_status_message1(SM_ORDER | SM_DING, 3, 4,
				  "\"%.200s\" is not a Pattern file", path);
	    }

	    p = NULL;
	    while(some_pats && fgets(buf, BUF_SIZE, fp) != NULL){
		if((pat = parse_pat(buf)) != NULL){
		    pat->patline = patline;
		    if(!patline->first)
		      patline->first = pat;

		    patline->last  = pat;

		    if(p){
			p->next   = pat;
			pat->prev = p;
			p = p->next;
		    }
		    else
		      p = pat;
		}
	    }

	    (void)fclose(fp);
	}
	else{
	    patline->readonly = 1;
	    q_status_message2(SM_ORDER | SM_DING, 3, 4,
			  "Error \"%.200s\" reading pattern file \"%.200s\"",
			      error_description(errno), path);
	}
    }
    else{		/* doesn't exist yet, try to create it */
	if(patline->readonly)
	  q_status_message1(SM_ORDER, 0, 3,
			    "Pattern file directory (%.200s) is ReadOnly", dir);
	else{
	    /*
	     * We try to create it by making up an empty patline and calling
	     * write_pattern_file.
	     */
	    patline->dirty = 1;
	    if(write_pattern_file(NULL, patline) != 0){
		patline->readonly = 1;
		patline->dirty = 0;
		q_status_message1(SM_ORDER | SM_DING, 3, 4,
				  "Error creating pattern file \"%.200s\"",
				  path);
	    }
	}
    }

    if(dir)
      fs_give((void **)&dir);

    return(patline);
}


PAT_LINE_S *
parse_pat_inherit(void)
{
    PAT_LINE_S *patline;
    PAT_S      *pat;

    patline = (PAT_LINE_S *)fs_get(sizeof(*patline));
    memset((void *)patline, 0, sizeof(*patline));
    patline->type = Inherit;

    pat = (PAT_S *)fs_get(sizeof(*pat));
    memset((void *)pat, 0, sizeof(*pat));
    pat->inherit = 1;

    pat->patline = patline;
    patline->first = pat;
    patline->last  = pat;

    return(patline);
}


/*
 * There are three forms that a PATTERN_S has at various times. There is
 * the actual PATTERN_S struct which is used internally and is used whenever
 * we are actually doing something with the pattern, like filtering or
 * something. There is the version that goes in the config file. And there
 * is the version the user edits.
 *
 * To go between these three forms we have the helper routines
 * 
 *   pattern_to_config
 *   config_to_pattern
 *   pattern_to_editlist
 *   editlist_to_pattern
 *
 * Here's what is supposed to be happening. A PATTERN_S is a linked list
 * of strings with nothing escaped. That is, a backslash or a comma is
 * just in there as a backslash or comma.
 *
 * The version the user edits is very similar. Because we have historically
 * used commas as separators the user has always had to enter a \, in order
 * to put a real comma in one of the items. That is the only difference
 * between a PATTERN_S string and the editlist strings. Note that backslashes
 * themselves are not escaped. A backslash which is not followed by a comma
 * is a backslash. It doesn't escape the following character. That's a bit
 * odd, it is that way because most people will never know about this
 * backslash stuff but PC-Pine users may have backslashes in folder names.
 *
 * The version that goes in the config file has a few transformations made.
 *      PATTERN_S   intermediate_form   Config string
 *         ,              \,               \x2C
 *         \              \\               \x5C
 *         /                               \/
 *         "                               \"
 *
 * The commas are turned into hex commas so that we can tell the separators
 * in the comma-separated lists from those commas.
 * The backslashes are escaped because they escape commas.
 * The /'s are escaped because they separate pattern pieces.
 * The "'s are escaped because they are significant to parse_list when
 *   parsing the config file.
 *                            hubert - 2004-04-01
 *                                     (date is only coincidental!)
 *
 * Addendum. The not's are handled separately from all the strings. Not sure
 * why that is or if there is a good reason. Nevertheless, now is not the
 * time to figure it out so leave it that way.
 *                            hubert - 2004-07-14
 */
PAT_S *
parse_pat(char *str)
{
    PAT_S *pat = NULL;
    char  *p, *q, *astr, *pstr;
    int    backslashed;
#define PTRN "pattern="
#define PTRNLEN 8
#define ACTN "action="
#define ACTNLEN 7

    if(str)
      removing_trailing_white_space(str);

    if(!str || !*str || *str == '#')
      return(pat);

    pat = (PAT_S *)fs_get(sizeof(*pat));
    memset((void *)pat, 0, sizeof(*pat));

    if((p = srchstr(str, PTRN)) != NULL){
	pat->patgrp = (PATGRP_S *)fs_get(sizeof(*pat->patgrp));
	memset((void *)pat->patgrp, 0, sizeof(*pat->patgrp));
	pat->patgrp->fldr_type = FLDR_DEFL;
	pat->patgrp->inabook = IAB_DEFL;
	pat->patgrp->cat_lim   = -1L;

	if((pstr = copy_quoted_string_asis(p+PTRNLEN)) != NULL){
	    /* move to next slash */
	    for(q=pstr, backslashed=0; *q; q++){
		switch(*q){
		  case '\\':
		    backslashed = !backslashed;
		    break;

		  case '/':
		    if(!backslashed){
			parse_patgrp_slash(q, pat->patgrp);
			if(pat->patgrp->bogus && !pat->raw)
			  pat->raw = cpystr(str);
		    }

		  /* fall through */

		  default:
		    backslashed = 0;
		    break;
		}
	    }

	    /* we always force a nickname */
	    if(!pat->patgrp->nick)
	      pat->patgrp->nick = cpystr("Alternate Role");

	    fs_give((void **)&pstr);
	}
    }

    if((p = srchstr(str, ACTN)) != NULL){
	pat->action = (ACTION_S *)fs_get(sizeof(*pat->action));
	memset((void *)pat->action, 0, sizeof(*pat->action));
	pat->action->startup_rule = IS_NOTSET;
	pat->action->repl_type = ROLE_REPL_DEFL;
	pat->action->forw_type = ROLE_FORW_DEFL;
	pat->action->comp_type = ROLE_COMP_DEFL;
	pat->action->nick = cpystr((pat->patgrp && pat->patgrp->nick
				    && pat->patgrp->nick[0])
				       ? pat->patgrp->nick : "Alternate Role");

	if((astr = copy_quoted_string_asis(p+ACTNLEN)) != NULL){
	    /* move to next slash */
	    for(q=astr, backslashed=0; *q; q++){
		switch(*q){
		  case '\\':
		    backslashed = !backslashed;
		    break;

		  case '/':
		    if(!backslashed){
			parse_action_slash(q, pat->action);
			if(pat->action->bogus && !pat->raw)
			  pat->raw = cpystr(str);
		    }

		  /* fall through */

		  default:
		    backslashed = 0;
		    break;
		}
	    }

	    fs_give((void **)&astr);

	    if(!pat->action->is_a_score)
	      pat->action->scoreval = 0L;
	    
	    if(pat->action->is_a_filter)
	      pat->action->kill = (pat->action->folder
				   || pat->action->kill == -1) ? 0 : 1;
	    else{
		if(pat->action->folder)
		  free_pattern(&pat->action->folder);
	    }

	    if(!pat->action->is_a_role){
		pat->action->repl_type = ROLE_NOTAROLE_DEFL;
		pat->action->forw_type = ROLE_NOTAROLE_DEFL;
		pat->action->comp_type = ROLE_NOTAROLE_DEFL;
		if(pat->action->from)
		  mail_free_address(&pat->action->from);
		if(pat->action->replyto)
		  mail_free_address(&pat->action->replyto);
		if(pat->action->fcc)
		  fs_give((void **)&pat->action->fcc);
		if(pat->action->litsig)
		  fs_give((void **)&pat->action->litsig);
		if(pat->action->sig)
		  fs_give((void **)&pat->action->sig);
		if(pat->action->template)
		  fs_give((void **)&pat->action->template);
		if(pat->action->cstm)
		  free_list_array(&pat->action->cstm);
		if(pat->action->smtp)
		  free_list_array(&pat->action->smtp);
		if(pat->action->nntp)
		  free_list_array(&pat->action->nntp);
		if(pat->action->inherit_nick)
		  fs_give((void **)&pat->action->inherit_nick);
	    }

	    if(!pat->action->is_a_incol){
		if(pat->action->incol)
		  free_color_pair(&pat->action->incol);
	    }

	    if(!pat->action->is_a_other){
		pat->action->sort_is_set = 0;
		pat->action->sortorder = 0;
		pat->action->revsort = 0;
		pat->action->startup_rule = IS_NOTSET;
		if(pat->action->index_format)
		  fs_give((void **)&pat->action->index_format);
	    }
	}
    }

    return(pat);
}


/*
 * Fill in one member of patgrp from str.
 *
 * The multiple constant strings are lame but it evolved this way from
 * previous versions and isn't worth fixing.
 */
void
parse_patgrp_slash(char *str, PATGRP_S *patgrp)
{
    char  *p;

    if(!patgrp)
      panic("NULL patgrp to parse_patgrp_slash");
    else if(!(str && *str)){
	panic("NULL or empty string to parse_patgrp_slash");
	patgrp->bogus = 1;
    }
    else if(!strncmp(str, "/NICK=", 6))
      patgrp->nick = remove_pat_escapes(str+6);
    else if(!strncmp(str, "/COMM=", 6))
      patgrp->comment = remove_pat_escapes(str+6);
    else if(!strncmp(str, "/TO=", 4) || !strncmp(str, "/!TO=", 5))
      patgrp->to = parse_pattern("TO", str, 1);
    else if(!strncmp(str, "/CC=", 4) || !strncmp(str, "/!CC=", 5))
      patgrp->cc = parse_pattern("CC", str, 1);
    else if(!strncmp(str, "/RECIP=", 7) || !strncmp(str, "/!RECIP=", 8))
      patgrp->recip = parse_pattern("RECIP", str, 1);
    else if(!strncmp(str, "/PARTIC=", 8) || !strncmp(str, "/!PARTIC=", 9))
      patgrp->partic = parse_pattern("PARTIC", str, 1);
    else if(!strncmp(str, "/FROM=", 6) || !strncmp(str, "/!FROM=", 7))
      patgrp->from = parse_pattern("FROM", str, 1);
    else if(!strncmp(str, "/SENDER=", 8) || !strncmp(str, "/!SENDER=", 9))
      patgrp->sender = parse_pattern("SENDER", str, 1);
    else if(!strncmp(str, "/NEWS=", 6) || !strncmp(str, "/!NEWS=", 7))
      patgrp->news = parse_pattern("NEWS", str, 1);
    else if(!strncmp(str, "/SUBJ=", 6) || !strncmp(str, "/!SUBJ=", 7))
      patgrp->subj = parse_pattern("SUBJ", str, 1);
    else if(!strncmp(str, "/ALL=", 5) || !strncmp(str, "/!ALL=", 6))
      patgrp->alltext = parse_pattern("ALL", str, 1);
    else if(!strncmp(str, "/BODY=", 6) || !strncmp(str, "/!BODY=", 7))
      patgrp->bodytext = parse_pattern("BODY", str, 1);
    else if(!strncmp(str, "/KEY=", 5) || !strncmp(str, "/!KEY=", 6))
      patgrp->keyword = parse_pattern("KEY", str, 1);
    else if(!strncmp(str, "/CHAR=", 6) || !strncmp(str, "/!CHAR=", 7))
      patgrp->charsets = parse_pattern("CHAR", str, 1);
    else if(!strncmp(str, "/FOLDER=", 8) || !strncmp(str, "/!FOLDER=", 9))
      patgrp->folder = parse_pattern("FOLDER", str, 1);
    else if(!strncmp(str, "/ABOOKS=", 8) || !strncmp(str, "/!ABOOKS=", 9))
      patgrp->abooks = parse_pattern("ABOOKS", str, 1);
    /*
     * A problem with arbhdrs is that more than one of them can appear in
     * the string. We come back here the second time, but we already took
     * care of the whole thing on the first pass. Hence the check for
     * arbhdr already set.
     */
    else if(!strncmp(str, "/ARB", 4) || !strncmp(str, "/!ARB", 5)
	    || !strncmp(str, "/EARB", 5) || !strncmp(str, "/!EARB", 6)){
	if(!patgrp->arbhdr)
	  patgrp->arbhdr = parse_arbhdr(str);
	/* else do nothing */
    }
    else if(!strncmp(str, "/SENTDATE=", 10))
      patgrp->age_uses_sentdate = 1;
    else if(!strncmp(str, "/SCOREI=", 8)){
	if((p = remove_pat_escapes(str+8)) != NULL){
	    if((patgrp->score = parse_intvl(p)) != NULL)
	      patgrp->do_score = 1;

	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/AGE=", 5)){
	if((p = remove_pat_escapes(str+5)) != NULL){
	    if((patgrp->age = parse_intvl(p)) != NULL)
	      patgrp->do_age  = 1;

	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/SIZE=", 6)){
	if((p = remove_pat_escapes(str+6)) != NULL){
	    if((patgrp->size = parse_intvl(p)) != NULL)
	      patgrp->do_size  = 1;

	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/CATCMD=", 8)){
	if((p = remove_pat_escapes(str+8)) != NULL){
	    int   commas = 0;
	    char *q;

	    /* count elements in list */
	    for(q = p; q && *q; q++)
	      if(*q == ',')
		commas++;

	    patgrp->category_cmd = parse_list(p, commas+1, PL_REMSURRQUOT,NULL);
	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/CATVAL=", 8)){
	if((p = remove_pat_escapes(str+8)) != NULL){
	    if((patgrp->cat = parse_intvl(p)) != NULL)
	      patgrp->do_cat = 1;

	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/CATLIM=", 8)){
	if((p = remove_pat_escapes(str+8)) != NULL){
	    long i;

	    i = atol(p);
	    patgrp->cat_lim = i;
	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/FLDTYPE=", 9)){
	if((p = remove_pat_escapes(str+9)) != NULL){
	    int        i;
	    NAMEVAL_S *v;

	    for(i = 0; (v = pat_fldr_types(i)); i++)
	      if(!strucmp(p, v->shortname)){
		  patgrp->fldr_type = v->value;
		  break;
	      }

	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/AFROM=", 7)){
	if((p = remove_pat_escapes(str+7)) != NULL){
	    int        i;
	    NAMEVAL_S *v;

	    for(i = 0; (v = inabook_fldr_types(i)); i++)
	      if(!strucmp(p, v->shortname)){
		  patgrp->inabook |= v->value;
		  break;
	      }

	    /* to match old semantics */
	    patgrp->inabook |= IAB_FROM;
	    patgrp->inabook |= IAB_REPLYTO;

	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/AFROMA=", 8)){
	if((p = remove_pat_escapes(str+8)) != NULL){

	    /* make sure AFROMA comes after AFROM in config lines */
	    patgrp->inabook &= ~IAB_ADDR_MASK;

	    if(strchr(p, 'F'))
	      patgrp->inabook |= IAB_FROM;
	    if(strchr(p, 'R'))
	      patgrp->inabook |= IAB_REPLYTO;
	    if(strchr(p, 'S'))
	      patgrp->inabook |= IAB_SENDER;
	    if(strchr(p, 'T'))
	      patgrp->inabook |= IAB_TO;
	    if(strchr(p, 'C'))
	      patgrp->inabook |= IAB_CC;

	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/STATN=", 7)){
	SET_STATUS(str,"/STATN=",patgrp->stat_new);
    }
    else if(!strncmp(str, "/STATR=", 7)){
	SET_STATUS(str,"/STATR=",patgrp->stat_rec);
    }
    else if(!strncmp(str, "/STATI=", 7)){
	SET_STATUS(str,"/STATI=",patgrp->stat_imp);
    }
    else if(!strncmp(str, "/STATA=", 7)){
	SET_STATUS(str,"/STATA=",patgrp->stat_ans);
    }
    else if(!strncmp(str, "/STATD=", 7)){
	SET_STATUS(str,"/STATD=",patgrp->stat_del);
    }
    else if(!strncmp(str, "/8BITS=", 7)){
	SET_STATUS(str,"/8BITS=",patgrp->stat_8bitsubj);
    }
    else if(!strncmp(str, "/BOM=", 5)){
	SET_STATUS(str,"/BOM=",patgrp->stat_bom);
    }
    else if(!strncmp(str, "/BOY=", 5)){
	SET_STATUS(str,"/BOY=",patgrp->stat_boy);
    }
    else{
	char save;

	patgrp->bogus = 1;

	if((p = strindex(str, '=')) != NULL){
	    save = *(p+1);
	    *(p+1) = '\0';
	}

	dprint((1,
	       "parse_patgrp_slash(%.20s): unrecognized in \"%s\"\n",
	       str ? str : "?",
	       (patgrp && patgrp->nick) ? patgrp->nick : ""));
	q_status_message4(SM_ORDER, 1, 3,
	      "Warning: unrecognized pattern element \"%.20s\"%.20s%.20s%.20s",
	      str, patgrp->nick ? " in rule \"" : "",
	      patgrp->nick ? patgrp->nick : "", patgrp->nick ? "\"" : "");

	if(p)
	  *(p+1) = save;
    }
}


/*
 * Fill in one member of action struct from str.
 *
 * The multiple constant strings are lame but it evolved this way from
 * previous versions and isn't worth fixing.
 */
void
parse_action_slash(char *str, ACTION_S *action)
{
    char      *p;
    int        stateval, i;
    NAMEVAL_S *v;

    if(!action)
      panic("NULL action to parse_action_slash");
    else if(!(str && *str))
      panic("NULL or empty string to parse_action_slash");
    else if(!strncmp(str, "/ROLE=1", 7))
      action->is_a_role = 1;
    else if(!strncmp(str, "/OTHER=1", 8))
      action->is_a_other = 1;
    else if(!strncmp(str, "/ISINCOL=1", 10))
      action->is_a_incol = 1;
    else if(!strncmp(str, "/ISSRCH=1", 9))
      action->is_a_srch = 1;
    /*
     * This is unfortunate. If a new filter is set to only set
     * state bits it will be interpreted by an older pine which
     * doesn't have that feature like a filter that is set to Delete.
     * So we change the filter indicator to FILTER=2 to disable the
     * filter for older versions.
     */
    else if(!strncmp(str, "/FILTER=1", 9) || !strncmp(str, "/FILTER=2", 9))
      action->is_a_filter = 1;
    else if(!strncmp(str, "/ISSCORE=1", 10))
      action->is_a_score = 1;
    else if(!strncmp(str, "/SCORE=", 7)){
	if((p = remove_pat_escapes(str+7)) != NULL){
	    long i;

	    i = atol(p);
	    if(i >= SCORE_MIN && i <= SCORE_MAX)
	      action->scoreval = i;

	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/SCOREHDRTOK=", 13))
      action->scorevalhdrtok = config_to_hdrtok(str+13);
    else if(!strncmp(str, "/FOLDER=", 8))
      action->folder = parse_pattern("FOLDER", str, 1);
    else if(!strncmp(str, "/KEYSET=", 8))
      action->keyword_set = parse_pattern("KEYSET", str, 1);
    else if(!strncmp(str, "/KEYCLR=", 8))
      action->keyword_clr = parse_pattern("KEYCLR", str, 1);
    else if(!strncmp(str, "/NOKILL=", 8))
      action->kill = -1;
    else if(!strncmp(str, "/NOTDEL=", 8))
      action->move_only_if_not_deleted = 1;
    else if(!strncmp(str, "/NONTERM=", 9))
      action->non_terminating = 1;
    else if(!strncmp(str, "/STATI=", 7)){
	stateval = ACT_STAT_LEAVE;
	SET_MSGSTATE(str,"/STATI=",stateval);
	switch(stateval){
	  case ACT_STAT_LEAVE:
	    break;
	  case ACT_STAT_SET:
	    action->state_setting_bits |= F_FLAG;
	    break;
	  case ACT_STAT_CLEAR:
	    action->state_setting_bits |= F_UNFLAG;
	    break;
	}
    }
    else if(!strncmp(str, "/STATD=", 7)){
	stateval = ACT_STAT_LEAVE;
	SET_MSGSTATE(str,"/STATD=",stateval);
	switch(stateval){
	  case ACT_STAT_LEAVE:
	    break;
	  case ACT_STAT_SET:
	    action->state_setting_bits |= F_DEL;
	    break;
	  case ACT_STAT_CLEAR:
	    action->state_setting_bits |= F_UNDEL;
	    break;
	}
    }
    else if(!strncmp(str, "/STATA=", 7)){
	stateval = ACT_STAT_LEAVE;
	SET_MSGSTATE(str,"/STATA=",stateval);
	switch(stateval){
	  case ACT_STAT_LEAVE:
	    break;
	  case ACT_STAT_SET:
	    action->state_setting_bits |= F_ANS;
	    break;
	  case ACT_STAT_CLEAR:
	    action->state_setting_bits |= F_UNANS;
	    break;
	}
    }
    else if(!strncmp(str, "/STATN=", 7)){
	stateval = ACT_STAT_LEAVE;
	SET_MSGSTATE(str,"/STATN=",stateval);
	switch(stateval){
	  case ACT_STAT_LEAVE:
	    break;
	  case ACT_STAT_SET:
	    action->state_setting_bits |= F_UNSEEN;
	    break;
	  case ACT_STAT_CLEAR:
	    action->state_setting_bits |= F_SEEN;
	    break;
	}
    }
    else if(!strncmp(str, "/RTYPE=", 7)){
	/* reply type */
	action->repl_type = ROLE_REPL_DEFL;
	if((p = remove_pat_escapes(str+7)) != NULL){
	    for(i = 0; (v = role_repl_types(i)); i++)
	      if(!strucmp(p, v->shortname)){
		  action->repl_type = v->value;
		  break;
	      }

	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/FTYPE=", 7)){
	/* forward type */
	action->forw_type = ROLE_FORW_DEFL;
	if((p = remove_pat_escapes(str+7)) != NULL){
	    for(i = 0; (v = role_forw_types(i)); i++)
	      if(!strucmp(p, v->shortname)){
		  action->forw_type = v->value;
		  break;
	      }

	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/CTYPE=", 7)){
	/* compose type */
	action->comp_type = ROLE_COMP_DEFL;
	if((p = remove_pat_escapes(str+7)) != NULL){
	    for(i = 0; (v = role_comp_types(i)); i++)
	      if(!strucmp(p, v->shortname)){
		  action->comp_type = v->value;
		  break;
	      }

	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/FROM=", 6)){
	/* get the from */
	if((p = remove_pat_escapes(str+6)) != NULL){
	    rfc822_parse_adrlist(&action->from, p,
				 ps_global->maildomain);
	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/REPL=", 6)){
	/* get the reply-to */
	if((p = remove_pat_escapes(str+6)) != NULL){
	    rfc822_parse_adrlist(&action->replyto, p,
				 ps_global->maildomain);
	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/FCC=", 5))
      action->fcc = remove_pat_escapes(str+5);
    else if(!strncmp(str, "/LSIG=", 6))
      action->litsig = remove_pat_escapes(str+6);
    else if(!strncmp(str, "/SIG=", 5))
      action->sig = remove_pat_escapes(str+5);
    else if(!strncmp(str, "/TEMPLATE=", 10))
      action->template = remove_pat_escapes(str+10);
    /* get the custom headers */
    else if(!strncmp(str, "/CSTM=", 6)){
	if((p = remove_pat_escapes(str+6)) != NULL){
	    int   commas = 0;
	    char *q;

	    /* count elements in list */
	    for(q = p; q && *q; q++)
	      if(*q == ',')
		commas++;

	    action->cstm = parse_list(p, commas+1, 0, NULL);
	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/SMTP=", 6)){
	if((p = remove_pat_escapes(str+6)) != NULL){
	    int   commas = 0;
	    char *q;

	    /* count elements in list */
	    for(q = p; q && *q; q++)
	      if(*q == ',')
		commas++;

	    action->smtp = parse_list(p, commas+1, PL_REMSURRQUOT, NULL);
	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/NNTP=", 6)){
	if((p = remove_pat_escapes(str+6)) != NULL){
	    int   commas = 0;
	    char *q;

	    /* count elements in list */
	    for(q = p; q && *q; q++)
	      if(*q == ',')
		commas++;

	    action->nntp = parse_list(p, commas+1, PL_REMSURRQUOT, NULL);
	    fs_give((void **)&p);
	}
    }
    else if(!strncmp(str, "/INICK=", 7))
      action->inherit_nick = remove_pat_escapes(str+7);
    else if(!strncmp(str, "/INCOL=", 7)){
	if((p = remove_pat_escapes(str+7)) != NULL){
	    char *fg = NULL, *bg = NULL, *z;

	    /*
	     * Color should look like
	     * /FG=white/BG=red
	     */
	    if((z = srchstr(p, "/FG=")) != NULL)
	      fg = remove_pat_escapes(z+4);
	    if((z = srchstr(p, "/BG=")) != NULL)
	      bg = remove_pat_escapes(z+4);

	    if(fg && *fg && bg && *bg)
	      action->incol = new_color_pair(fg, bg);

	    if(fg)
	      fs_give((void **)&fg);
	    if(bg)
	      fs_give((void **)&bg);
	    fs_give((void **)&p);
	}
    }
    /* per-folder sort */
    else if(!strncmp(str, "/SORT=", 6)){
	if((p = remove_pat_escapes(str+6)) != NULL){
	    SortOrder def_sort;
	    int       def_sort_rev;

	    if(decode_sort(p, &def_sort, &def_sort_rev) != -1){
		action->sort_is_set = 1;
		action->sortorder = def_sort;
		action->revsort   = (def_sort_rev ? 1 : 0);
	    }

	    fs_give((void **)&p);
	}
    }
    /* per-folder index-format */
    else if(!strncmp(str, "/IFORM=", 7))
      action->index_format = remove_pat_escapes(str+7);
    /* per-folder startup-rule */
    else if(!strncmp(str, "/START=", 7)){
	if((p = remove_pat_escapes(str+7)) != NULL){
	    for(i = 0; (v = startup_rules(i)); i++)
	      if(!strucmp(p, S_OR_L(v))){
		  action->startup_rule = v->value;
		  break;
	      }

	    fs_give((void **)&p);
	}
    }
    else{
	char save;

	action->bogus = 1;

	if((p = strindex(str, '=')) != NULL){
	    save = *(p+1);
	    *(p+1) = '\0';
	}

	dprint((1,
	       "parse_action_slash(%.20s): unrecognized in \"%s\"\n",
	       str ? str : "?",
	       (action && action->nick) ? action->nick : ""));
	q_status_message4(SM_ORDER, 1, 3,
	      "Warning: unrecognized pattern action \"%.20s\"%.20s%.20s%.20s",
	      str, action->nick ? " in rule \"" : "",
	      action->nick ? action->nick : "", action->nick ? "\"" : "");

	if(p)
	  *(p+1) = save;
    }
}


/*
 * Str looks like (min,max) or a comma-separated list of these.
 *
 * Parens are optional if unambiguous, whitespace is ignored.
 * If min is left out it is -INF. If max is left out it is INF.
 * If only one number and no comma number is min and max is INF.
 *
 * Returns the INTVL_S list.
 */
INTVL_S *
parse_intvl(char *str)
{
    char *q;
    long  left, right;
    INTVL_S *ret = NULL, **next;

    if(!str)
      return(ret);

    q = str;

    for(;;){
	left = right = INTVL_UNDEF;

	/* skip to first number */
	while(isspace((unsigned char) *q) || *q == LPAREN)
	  q++;
	
	/* min number */
	if(*q == COMMA || !struncmp(q, "-INF", 4))
	  left = - INTVL_INF;
	else if(*q == '-' || isdigit((unsigned char) *q))
	  left = atol(q);

	if(left != INTVL_UNDEF){
	    /* skip to second number */
	    while(*q && *q != COMMA && *q != RPAREN)
	      q++;
	    if(*q == COMMA)
	      q++;
	    while(isspace((unsigned char) *q))
	      q++;

	    /* max number */
	    if(*q == '\0' || *q == RPAREN || !struncmp(q, "INF", 3))
	      right = INTVL_INF;
	    else if(*q == '-' || isdigit((unsigned char) *q))
	      right = atol(q);
	}
	
	if(left == INTVL_UNDEF || right == INTVL_UNDEF
	   || left > right){
	    if(left != INTVL_UNDEF || right != INTVL_UNDEF || *q){
		if(left != INTVL_UNDEF && right != INTVL_UNDEF
		   && left > right)
		  q_status_message1(SM_ORDER, 3, 5,
				_("Error: Interval \"%s\", min > max"), str);
		else
		  q_status_message1(SM_ORDER, 3, 5,
		      _("Error: Interval \"%s\": syntax is (min,max)"), str);
	      
		if(ret)
		  free_intvl(&ret);
		
		ret = NULL;
	    }

	    break;
	}
	else{
	    if(!ret){
		ret = (INTVL_S *) fs_get(sizeof(*ret));
		memset((void *) ret, 0, sizeof(*ret));
		ret->imin = left;
		ret->imax = right;
		next = &ret->next;
	    }
	    else{
		*next = (INTVL_S *) fs_get(sizeof(*ret));
		memset((void *) *next, 0, sizeof(*ret));
		(*next)->imin = left;
		(*next)->imax = right;
		next = &(*next)->next;
	    }

	    /* skip to next interval in list */
	    while(*q && *q != COMMA && *q != RPAREN)
	      q++;
	    if(*q == RPAREN)
	      q++;
	    while(*q && *q != COMMA)
	      q++;
	    if(*q == COMMA)
	      q++;
	}
    }

    return(ret);
}


/*
 * Returns string that looks like "(left,right),(left2,right2)".
 * Caller is responsible for freeing memory.
 */
char *
stringform_of_intvl(INTVL_S *intvl)
{
    char *res = NULL;

    if(intvl && intvl->imin != INTVL_UNDEF && intvl->imax != INTVL_UNDEF
       && intvl->imin <= intvl->imax){
	char     lbuf[20], rbuf[20], buf[45], *p;
	INTVL_S *iv;
	int      count = 0;
	size_t   reslen;

	/* find a max size and allocate it for the result */
	for(iv = intvl;
	    (iv && iv->imin != INTVL_UNDEF && iv->imax != INTVL_UNDEF
	     && iv->imin <= iv->imax);
	    iv = iv->next)
	  count++;

	reslen = count * 50 * sizeof(char);
	res = (char *) fs_get(reslen+1);
	memset((void *) res, 0, reslen+1);
	p = res;

	for(iv = intvl;
	    (iv && iv->imin != INTVL_UNDEF && iv->imax != INTVL_UNDEF
	     && iv->imin <= iv->imax);
	    iv = iv->next){

	    if(iv->imin == - INTVL_INF){
	      strncpy(lbuf, "-INF", sizeof(lbuf));
	      lbuf[sizeof(lbuf)-1] = '\0';
	    }
	    else
	      snprintf(lbuf, sizeof(lbuf), "%ld", iv->imin);

	    if(iv->imax == INTVL_INF){
	      strncpy(rbuf, "INF", sizeof(rbuf));
	      rbuf[sizeof(rbuf)-1] = '\0';
	    }
	    else
	      snprintf(rbuf, sizeof(rbuf), "%ld", iv->imax);

	    snprintf(buf, sizeof(buf), "%.1s(%.20s,%.20s)", (p == res) ? "" : ",",
		    lbuf, rbuf);

	    sstrncpy(&p, buf, reslen+1 -(p-res));
	}

	res[reslen] = '\0';
    }

    return(res);
}


char *
hdrtok_to_stringform(HEADER_TOK_S *hdrtok)
{
    char  buf[1024], nbuf[10];
    char *res = NULL;
    char *p, *ptr;

    if(hdrtok){
	snprintf(nbuf, sizeof(nbuf), "%d", hdrtok->fieldnum); 
	ptr = buf;
	sstrncpy(&ptr, hdrtok->hdrname ? hdrtok->hdrname : "", sizeof(buf)-(ptr-buf));
	sstrncpy(&ptr, "(", sizeof(buf)-(ptr-buf));
	sstrncpy(&ptr, nbuf, sizeof(buf)-(ptr-buf));
	sstrncpy(&ptr, ",\"", sizeof(buf)-(ptr-buf));
	p = hdrtok->fieldseps;
	while(p && *p){
	    if((*p == '\"' || *p == '\\') && ptr-buf < sizeof(buf))
	      *ptr++ = '\\';

	    if(ptr-buf < sizeof(buf))
	      *ptr++ = *p++;
	}

	sstrncpy(&ptr, "\")", sizeof(buf)-(ptr-buf));

	if(ptr-buf < sizeof(buf))
	  *ptr = '\0';
	else
	  buf[sizeof(buf)-1] = '\0';

	res = cpystr(buf);
    }

    return(res);
}


HEADER_TOK_S *
stringform_to_hdrtok(char *str)
{
    char  *p, *q, *w, hdrname[200];
    HEADER_TOK_S *hdrtok = NULL;

    if(str && *str){
	p = str;
	hdrname[0] = '\0';
	w = hdrname;

	if(*p == '\"'){				/* quoted name */
	    p++;
	    while(w < hdrname + sizeof(hdrname)-1 && *p != '\"'){
		if(*p == '\\')
		  p++;

		*w++ = *p++;
	    }

	    *w = '\0';
	    if(*p == '\"')
	      p++;
	}
	else{
	    while(w < hdrname + sizeof(hdrname)-1 &&
		  !(!(*p & 0x80) && isspace((unsigned char)*p)) &&
		  *p != '(')
	      *w++ = *p++;

	    *w = '\0';
	}

	if(hdrname[0])
	  hdrtok = new_hdrtok(hdrname);

	if(hdrtok){
	    if(*p == '('){
		p++;
		
		if(*p && isdigit((unsigned char) *p)){
		    q = p;
		    while(*p && isdigit((unsigned char) *p))
		      p++;

		    hdrtok->fieldnum = atoi(q);

		    if(*p == ','){
			int j;

			p++;
			/* don't use default */
			if(*p && *p != ')' && hdrtok->fieldseps){
			    hdrtok->fieldseps[0] = '\0';
			    hdrtok->fieldsepcnt = 0;
			}

			j = 0;
			if(*p == '\"' && strchr(p+1, '\"')){		/* quoted */
			    p++;
			    while(*p && *p != '\"'){
				if(hdrtok->fieldseps)
				  fs_resize((void **) &hdrtok->fieldseps, j+2);

				if(*p == '\\' && *(p+1))
				  p++;

				if(hdrtok->fieldseps){
				    hdrtok->fieldseps[j++] = *p++;
				    hdrtok->fieldseps[j] = '\0';
				    hdrtok->fieldsepcnt = j;
				}
			    }
			}
			else{
			    while(*p && *p != ')'){
				if(hdrtok->fieldseps)
				  fs_resize((void **) &hdrtok->fieldseps, j+2);

				if(*p == '\\' && *(p+1))
				  p++;

				if(hdrtok->fieldseps){
				    hdrtok->fieldseps[j++] = *p++;
				    hdrtok->fieldseps[j] = '\0';
				    hdrtok->fieldsepcnt = j;
				}
			    }
			}
		    }
		    else{
			q_status_message(SM_ORDER | SM_DING, 3, 3, "Missing 2nd argument should be field number, a non-negative digit");
		    }
		}
		else{
		    q_status_message(SM_ORDER | SM_DING, 3, 3, "1st argument should be field number, a non-negative digit");
		}
	    }
	    else{
		q_status_message1(SM_ORDER | SM_DING, 3, 3, "Missing left parenthesis in %s", str);
	    }
	}
	else{
	    q_status_message1(SM_ORDER | SM_DING, 3, 3, "Missing header name in %s", str);
	}
    }

    return(hdrtok);
}


char *
hdrtok_to_config(HEADER_TOK_S *hdrtok)
{
    char *ptr, buf[1024], nbuf[10], *p1, *p2, *p3;
    char *res = NULL;

    if(hdrtok){
	snprintf(nbuf, sizeof(nbuf), "%d", hdrtok->fieldnum); 
	memset(buf, 0, sizeof(buf));
	ptr = buf;
	sstrncpy(&ptr, "/HN=", sizeof(buf)-(ptr-buf));
	sstrncpy(&ptr, (p1=add_pat_escapes(hdrtok->hdrname ? hdrtok->hdrname : "")), sizeof(buf)-(ptr-buf));
	sstrncpy(&ptr, "/FN=", sizeof(buf)-(ptr-buf));
	sstrncpy(&ptr, (p2=add_pat_escapes(nbuf)), sizeof(buf)-(ptr-buf));
	sstrncpy(&ptr, "/FS=", sizeof(buf)-(ptr-buf));
	sstrncpy(&ptr, (p3=add_pat_escapes(hdrtok->fieldseps ? hdrtok->fieldseps : "")), sizeof(buf)-(ptr-buf));

	buf[sizeof(buf)-1] = '\0';
	res = add_pat_escapes(buf);

	if(p1)
	  fs_give((void **)&p1);

	if(p2)
	  fs_give((void **)&p2);

	if(p3)
	  fs_give((void **)&p3);
    }

    return(res);
}


HEADER_TOK_S *
config_to_hdrtok(char *str)
{
    HEADER_TOK_S *hdrtok = NULL;
    char *p, *q;
    int j;

    if(str && *str){
	if((q = remove_pat_escapes(str)) != NULL){
	    char *hn = NULL, *fn = NULL, *fs = NULL, *z;

	    if((z = srchstr(q, "/HN=")) != NULL)
	      hn = remove_pat_escapes(z+4);
	    if((z = srchstr(q, "/FN=")) != NULL)
	      fn = remove_pat_escapes(z+4);
	    if((z = srchstr(q, "/FS=")) != NULL)
	      fs = remove_pat_escapes(z+4);

	    hdrtok = new_hdrtok(hn);
	    if(fn)
	      hdrtok->fieldnum = atoi(fn);

	    if(fs && *fs){
		if(hdrtok->fieldseps){
		    hdrtok->fieldseps[0] = '\0';
		    hdrtok->fieldsepcnt = 0;
		}

		p = fs;
		j = 0;
		if(*p == '\"' && strchr(p+1, '\"')){
		    p++;
		    while(*p && *p != '\"'){
			if(hdrtok->fieldseps)
			  fs_resize((void **) &hdrtok->fieldseps, j+2);

			if(*p == '\\' && *(p+1))
			  p++;

			if(hdrtok->fieldseps){
			    hdrtok->fieldseps[j++] = *p++;
			    hdrtok->fieldseps[j] = '\0';
			    hdrtok->fieldsepcnt = j;
			}
		    }
		}
		else{
		    while(*p){
			if(hdrtok->fieldseps)
			  fs_resize((void **) &hdrtok->fieldseps, j+2);

			if(*p == '\\' && *(p+1))
			  p++;

			if(hdrtok->fieldseps){
			    hdrtok->fieldseps[j++] = *p++;
			    hdrtok->fieldseps[j] = '\0';
			    hdrtok->fieldsepcnt = j;
			}
		    }
		}
	    }

	    if(hn)
	      fs_give((void **)&hn);
	    if(fn)
	      fs_give((void **)&fn);
	    if(fs)
	      fs_give((void **)&fs);

	    fs_give((void **)&q);
	}
    }

    return(hdrtok);
}


/*
 * Args -- flags  - SCOREUSE_INVALID  Mark scores_in_use invalid so that we'll
 *					recalculate if we want to use it again.
 *		  - SCOREUSE_GET    Return whether scores are being used or not.
 *
 * Returns -- 0 - Scores not being used at all.
 *	     >0 - Scores are used. The return value consists of flag values
 *		    OR'd together. Possible values are:
 * 
 *			SCOREUSE_INCOLS  - scores needed for index line colors
 *			SCOREUSE_ROLES   - scores needed for roles
 *			SCOREUSE_FILTERS - scores needed for filters
 *			SCOREUSE_OTHER   - scores needed for other stuff
 *			SCOREUSE_INDEX   - scores needed for index drawing
 *
 *			SCOREUSE_STATEDEP - scores depend on message state
 */
int
scores_are_used(int flags)
{
    static int  scores_in_use = -1;
    long        type1, type2;
    int         scores_are_defined, scores_are_used_somewhere = 0;
    PAT_STATE   pstate1, pstate2;

    if(flags & SCOREUSE_INVALID) /* mark invalid so we recalculate next time */
      scores_in_use = -1;
    else if(scores_in_use == -1){

	/*
	 * Check the patterns to see if scores are potentially
	 * being used.
	 * The first_pattern() in the if checks whether there are any
	 * non-zero scorevals. The loop checks whether any patterns
	 * use those non-zero scorevals.
	 */
	type1 = ROLE_SCORE;
	type2 = (ROLE_REPLY | ROLE_FORWARD | ROLE_COMPOSE |
		 ROLE_INCOL | ROLE_DO_FILTER);
	scores_are_defined = nonempty_patterns(type1, &pstate1)
			     && first_pattern(&pstate1);
	if(scores_are_defined)
	  scores_are_used_somewhere =
	     ((nonempty_patterns(type2, &pstate2) && first_pattern(&pstate2))
	      || ps_global->a_format_contains_score
	      || mn_get_sort(ps_global->msgmap) == SortScore);

	if(scores_are_used_somewhere){
	    PAT_S *pat;

	    /*
	     * Careful. nonempty_patterns() may call close_pattern()
	     * which will set scores_in_use to -1! So we have to be
	     * sure to reset it after we call nonempty_patterns().
	     */
	    scores_in_use = 0;
	    if(ps_global->a_format_contains_score
	       || mn_get_sort(ps_global->msgmap) == SortScore)
	      scores_in_use |= SCOREUSE_INDEX;

	    if(nonempty_patterns(type2, &pstate2))
	      for(pat = first_pattern(&pstate2);
		  pat;
		  pat = next_pattern(&pstate2))
	        if(pat->patgrp && !pat->patgrp->bogus && pat->patgrp->do_score){
		    if(pat->action && pat->action->is_a_incol)
		      scores_in_use |= SCOREUSE_INCOLS;
		    if(pat->action && pat->action->is_a_role)
		      scores_in_use |= SCOREUSE_ROLES;
		    if(pat->action && pat->action->is_a_filter)
		      scores_in_use |= SCOREUSE_FILTERS;
		    if(pat->action && pat->action->is_a_other)
		      scores_in_use |= SCOREUSE_OTHER;
	        }
	    
	    /*
	     * Note whether scores depend on message state or not.
	     */
	    if(scores_in_use)
	      for(pat = first_pattern(&pstate1);
		  pat;
		  pat = next_pattern(&pstate1))
		if(patgrp_depends_on_active_state(pat->patgrp)){
		    scores_in_use |= SCOREUSE_STATEDEP;
		    break;
		}
	      
	}
	else
	  scores_in_use = 0;
    }

    return((scores_in_use == -1) ? 0 : scores_in_use);
}


int
patgrp_depends_on_state(PATGRP_S *patgrp)
{
    return(patgrp && (patgrp_depends_on_active_state(patgrp)
		      || patgrp->stat_rec != PAT_STAT_EITHER));
}


/*
 * Recent doesn't count for this function because it doesn't change while
 * the mailbox is open.
 */
int
patgrp_depends_on_active_state(PATGRP_S *patgrp)
{
    return(patgrp && !patgrp->bogus
           && (patgrp->stat_new  != PAT_STAT_EITHER ||
	       patgrp->stat_del  != PAT_STAT_EITHER ||
	       patgrp->stat_imp  != PAT_STAT_EITHER ||
	       patgrp->stat_ans  != PAT_STAT_EITHER ||
	       patgrp->keyword));
}


/*
 * Look for label in str and return a pointer to parsed string.
 * Actually, we look for "label=" or "!label=", the second means NOT.
 * Converts from string from patterns file which looks like
 *       /NEWS=comp.mail.,comp.mail.pine/TO=...
 * This is the string that came from pattern="string" with the pattern=
 * and outer quotes removed.
 * This converts the string to a PATTERN_S list and returns
 * an allocated copy.
 */
PATTERN_S *
parse_pattern(char *label, char *str, int hex_to_backslashed)
{
    char       copy[50];	/* local copy of label */
    char       copynot[50];	/* local copy of label, NOT'ed */
    char      *q, *labeled_str;
    PATTERN_S *head = NULL;

    if(!label || !str)
      return(NULL);
    
    q = copy;
    sstrncpy(&q, "/", sizeof(copy));
    sstrncpy(&q, label, sizeof(copy) - (q-copy));
    sstrncpy(&q, "=", sizeof(copy) - (q-copy));
    copy[sizeof(copy)-1] = '\0';
    q = copynot;
    sstrncpy(&q, "/!", sizeof(copynot));
    sstrncpy(&q, label, sizeof(copynot) - (q-copynot));
    sstrncpy(&q, "=", sizeof(copynot) - (q-copynot));
    copynot[sizeof(copynot)-1] = '\0';

    if(hex_to_backslashed){
	if((q = srchstr(str, copy)) != NULL){
	    head = config_to_pattern(q+strlen(copy));
	}
	else if((q = srchstr(str, copynot)) != NULL){
	    head = config_to_pattern(q+strlen(copynot));
	    head->not = 1;
	}
    }
    else{
	if((q = srchstr(str, copy)) != NULL){
	    if((labeled_str =
			remove_backslash_escapes(q+strlen(copy))) != NULL){
		head = string_to_pattern(labeled_str);
		fs_give((void **)&labeled_str);
	    }
	}
	else if((q = srchstr(str, copynot)) != NULL){
	    if((labeled_str =
			remove_backslash_escapes(q+strlen(copynot))) != NULL){
		head = string_to_pattern(labeled_str);
		head->not = 1;
		fs_give((void **)&labeled_str);
	    }
	}
    }

    return(head);
}


/*
 * Look for /ARB's in str and return a pointer to parsed ARBHDR_S.
 * Actually, we look for /!ARB and /!EARB as well. Those mean NOT.
 * Converts from string from patterns file which looks like
 *       /ARB<fieldname1>=pattern/.../ARB<fieldname2>=pattern...
 * This is the string that came from pattern="string" with the pattern=
 * and outer quotes removed.
 * This converts the string to a ARBHDR_S list and returns
 * an allocated copy.
 */
ARBHDR_S *
parse_arbhdr(char *str)
{
    char      *q, *s, *equals, *noesc;
    int        not, empty, skip;
    ARBHDR_S  *ahdr = NULL, *a, *aa;
    PATTERN_S *p = NULL;

    if(!str)
      return(NULL);

    aa = NULL;
    for(s = str; (q = next_arb(s)); s = q+1){
	not = (q[1] == '!') ? 1 : 0;
	empty = (q[not+1] == 'E') ? 1 : 0;
	skip = 4 + not + empty;
	if((noesc = remove_pat_escapes(q+skip)) != NULL){
	    if(*noesc != '=' && (equals = strindex(noesc, '=')) != NULL){
		a = (ARBHDR_S *)fs_get(sizeof(*a));
		memset((void *)a, 0, sizeof(*a));
		*equals = '\0';
		a->isemptyval = empty;
		a->field = cpystr(noesc);
		if(empty)
		  a->p     = string_to_pattern("");
		else if(*(equals+1) &&
			(p = string_to_pattern(equals+1)) != NULL)
		  a->p     = p;
		
		if(not && a->p)
		  a->p->not = 1;

		/* keep them in the same order */
		if(aa){
		    aa->next = a;
		    aa = aa->next;
		}
		else{
		    ahdr = a;
		    aa = ahdr;
		}
	    }

	    fs_give((void **)&noesc);
	}
    }

    return(ahdr);
}


char *
next_arb(char *start)
{
    char *q1, *q2, *q3, *q4, *p;

    q1 = srchstr(start, "/ARB");
    q2 = srchstr(start, "/!ARB");
    q3 = srchstr(start, "/EARB");
    q4 = srchstr(start, "/!EARB");

    p = q1;
    if(!p || (q2 && q2 < p))
      p = q2;
    if(!p || (q3 && q3 < p))
      p = q3;
    if(!p || (q4 && q4 < p))
      p = q4;
    
    return(p);
}


/*
 * Converts a string to a PATTERN_S list and returns an
 * allocated copy. The source string looks like
 *        string1,string2,...
 * Commas and backslashes may be backslash-escaped in the original string
 * in order to include actual commas and backslashes in the pattern.
 * So \, is an actual comma and , is the separator character.
 */
PATTERN_S *
string_to_pattern(char *str)
{
    char      *q, *s, *workspace;
    PATTERN_S *p, *head = NULL, **nextp;

    if(!str)
      return(head);
    
    /*
     * We want an empty string to cause an empty substring in the pattern
     * instead of returning a NULL pattern. That can be used as a way to
     * match any header. For example, if all the patterns but the news
     * pattern were null and the news pattern was a substring of "" then
     * we use that to match any message with a newsgroups header.
     */
    if(!*str){
	head = (PATTERN_S *)fs_get(sizeof(*p));
	memset((void *)head, 0, sizeof(*head));
	head->substring = cpystr("");
    }
    else{
	nextp = &head;
	workspace = (char *)fs_get((strlen(str)+1) * sizeof(char));
	s = workspace;
	*s = '\0';
	q = str;
	do {
	    switch(*q){
	      case COMMA:
	      case '\0':
		*s = '\0';
		removing_leading_and_trailing_white_space(workspace);
		p = (PATTERN_S *)fs_get(sizeof(*p));
		memset((void *)p, 0, sizeof(*p));
		p->substring = cpystr(workspace);

		convert_possibly_encoded_str_to_utf8(&p->substring);

		*nextp = p;
		nextp = &p->next;
		s = workspace;
		*s = '\0';
		break;
		
	      case BSLASH:
		if(*(q+1) == COMMA || *(q+1) == BSLASH)
		  *s++ = *(++q);
		else
		  *s++ = *q;

		break;

	      default:
		*s++ = *q;
		break;
	    }
	} while(*q++);

	fs_give((void **)&workspace);
    }

    return(head);
}

    
/*
 * Converts a PATTERN_S list to a string.
 * The resulting string is allocated here and looks like
 *        string1,string2,...
 * Commas and backslashes in the original pattern
 * end up backslash-escaped in the string.
 */
char *
pattern_to_string(PATTERN_S *pattern)
{
    PATTERN_S *p;
    char      *result = NULL, *q, *s;
    size_t     n;

    if(!pattern)
      return(result);

    /* how much space is needed? */
    n = 0;
    for(p = pattern; p; p = p->next){
	n += (p == pattern) ? 0 : 1;
	for(s = p->substring; s && *s; s++){
	    if(*s == COMMA || *s == BSLASH)
	      n++;

	    n++;
	}
    }

    q = result = (char *)fs_get(++n);
    for(p = pattern; p; p = p->next){
	if(p != pattern)
	  *q++ = COMMA;

	for(s = p->substring; s && *s; s++){
	    if(*s == COMMA || *s == BSLASH)
	      *q++ = '\\';

	    *q++ = *s;
	}
    }

    *q = '\0';

    return(result);
}


/*
 * Do the escaping necessary to take a string for a pattern into a comma-
 * separated string with escapes suitable for the config file.
 * Returns an allocated copy of that string.
 * In particular
 *     ,  ->  \,  ->  \x2C
 *     \  ->  \\  ->  \x5C
 *     "       ->      \"
 *     /       ->      \/
 */
char *
pattern_to_config(PATTERN_S *pat)
{
    char *s, *res = NULL;
    
    s = pattern_to_string(pat);
    if(s){
	res = add_pat_escapes(s);
	fs_give((void **) &s);
    }

    return(res);
}

/*
 * Opposite of pattern_to_config.
 */
PATTERN_S *
config_to_pattern(char *str)
{
    char      *s;
    PATTERN_S *pat = NULL;
    
    s = remove_pat_escapes(str);
    if(s){
	pat = string_to_pattern(s);
	fs_give((void **) &s);
    }

    return(pat);
}


/*
 * Converts an array of strings to a PATTERN_S list and returns an
 * allocated copy.
 * The list strings may not contain commas directly, because the UI turns
 * those into separate list members. Instead, the user types \, and
 * that backslash comma is converted to a comma here.
 * It is a bit odd. Backslash itself is not escaped. A backslash which is
 * not followed by a comma is a literal backslash, a backslash followed by
 * a comma is a comma.
 */
PATTERN_S *
editlist_to_pattern(char **list)
{
    PATTERN_S *head = NULL;

    if(!(list && *list))
      return(head);
    
    /*
     * We want an empty string to cause an empty substring in the pattern
     * instead of returning a NULL pattern. That can be used as a way to
     * match any header. For example, if all the patterns but the news
     * pattern were null and the news pattern was a substring of "" then
     * we use that to match any message with a newsgroups header.
     */
    if(!list[0][0]){
	head = (PATTERN_S *) fs_get(sizeof(*head));
	memset((void *) head, 0, sizeof(*head));
	head->substring = cpystr("");
    }
    else{
	char      *str, *s, *q, *workspace = NULL;
	size_t     l = 0;
	PATTERN_S *p, **nextp;
	int        i;

	nextp = &head;
	for(i = 0; (str = list[i]); i++){
	    if(str[0]){
		if(!workspace){
		    l = strlen(str) + 1;
		    workspace = (char *) fs_get(l * sizeof(char));
		}
		else if(strlen(str) + 1 > l){
		    l = strlen(str) + 1;
		    fs_give((void **) &workspace);
		    workspace = (char *) fs_get(l * sizeof(char));
		}

		s = workspace;
		*s = '\0';
		q = str;
		do {
		    switch(*q){
		      case '\0':
			*s = '\0';
			removing_leading_and_trailing_white_space(workspace);
			p = (PATTERN_S *) fs_get(sizeof(*p));
			memset((void *) p, 0, sizeof(*p));
			p->substring = cpystr(workspace);
			*nextp = p;
			nextp = &p->next;
			s = workspace;
			*s = '\0';
			break;
			
		      case BSLASH:
			if(*(q+1) == COMMA)
			  *s++ = *(++q);
			else
			  *s++ = *q;

			break;

		      default:
			*s++ = *q;
			break;
		    }
		} while(*q++);
	    }
	}
    }

    return(head);
}


/*
 * Converts a PATTERN_S to an array of strings and returns an allocated copy.
 * Commas are converted to backslash-comma, because the text_tool UI uses
 * commas to separate items.
 * It is a bit odd. Backslash itself is not escaped. A backslash which is
 * not followed by a comma is a literal backslash, a backslash followed by
 * a comma is a comma.
 */
char **
pattern_to_editlist(PATTERN_S *pat)
{
    int        cnt, i;
    PATTERN_S *p;
    char     **list = NULL;

    if(!pat)
      return(list);

    /* how many in list? */
    for(cnt = 0, p = pat; p; p = p->next)
      cnt++;
    
    list = (char **) fs_get((cnt + 1) * sizeof(*list));
    memset((void *) list, 0, (cnt + 1) * sizeof(*list));

    for(i = 0, p = pat; p; p = p->next, i++)
      list[i] = add_comma_escapes(p->substring);
    
    return(list);
}


PATGRP_S *
nick_to_patgrp(char *nick, int rflags)
{
    PAT_S     *pat;
    PAT_STATE  pstate;
    PATGRP_S  *patgrp = NULL;

    if(!(nick && *nick
	 && nonempty_patterns(rflags, &pstate) && first_pattern(&pstate)))
      return(patgrp);

    for(pat = first_pattern(&pstate);
	!patgrp && pat;
	pat = next_pattern(&pstate))
      if(pat->patgrp && pat->patgrp->nick && !strcmp(pat->patgrp->nick, nick))
	patgrp = copy_patgrp(pat->patgrp);

    return(patgrp);
}


/*
 * Must be called with a pstate, we don't check for it.
 * It respects the cur_rflag_num in pstate. That is, it doesn't start over
 * at i=1, it starts at cur_rflag_num.
 */
PAT_S *
first_any_pattern(PAT_STATE *pstate)
{
    PAT_LINE_S *patline = NULL;
    int         i;
    long        local_rflag;

    /*
     * The rest of pstate should be set before coming here.
     * In particular, the rflags should be set by a call to nonempty_patterns
     * or any_patterns, and cur_rflag_num should be set.
     */
    pstate->patlinecurrent = NULL;
    pstate->patcurrent     = NULL;

    /*
     * The order of these is important. It is the same as the order
     * used for next_any_pattern and opposite of the order used by
     * last and prev. For next_any's benefit, we allow cur_rflag_num to
     * start us out past the first set.
     */
    for(i = pstate->cur_rflag_num; i <= PATTERN_N; i++){

	local_rflag = 0L;

	switch(i){
	  case 1:
	    local_rflag = ROLE_DO_SRCH & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 2:
	    local_rflag = ROLE_DO_INCOLS & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 3:
	    local_rflag = ROLE_DO_ROLES & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 4:
	    local_rflag = ROLE_DO_FILTER & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 5:
	    local_rflag = ROLE_DO_SCORES & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 6:
	    local_rflag = ROLE_DO_OTHER & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 7:
	    local_rflag = ROLE_OLD_FILT & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 8:
	    local_rflag = ROLE_OLD_SCORE & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case PATTERN_N:
	    local_rflag = ROLE_OLD_PAT & CANONICAL_RFLAGS(pstate->rflags);
	    break;
	}

	if(local_rflag){
	    SET_PATTYPE(local_rflag | (pstate->rflags & PAT_USE_MASK));

	    if(*cur_pat_h){
		/* Find first patline with a pat */
		for(patline = (*cur_pat_h)->patlinehead;
		    patline && !patline->first;
		    patline = patline->next)
		  ;
	    }

	    if(patline){
		pstate->cur_rflag_num  = i;
		pstate->patlinecurrent = patline;
		pstate->patcurrent     = patline->first;
	    }
	}

	if(pstate->patcurrent)
	  break;
    }

    return(pstate->patcurrent);
}


/*
 * Return first pattern of the specified types. These types were set by a
 * previous call to any_patterns or nonempty_patterns.
 *
 * Args --  pstate  pattern state. This is set here and passed back for
 *                  use by next_pattern. Must be non-null.
 *                  It must have been initialized previously by a call to
 *                  nonempty_patterns or any_patterns.
 */
PAT_S *
first_pattern(PAT_STATE *pstate)
{
    PAT_S           *pat;
    long             rflags;

    pstate->cur_rflag_num = 1;

    rflags = pstate->rflags;

    for(pat = first_any_pattern(pstate);
	pat && !((pat->action &&
		  ((rflags & ROLE_DO_ROLES && pat->action->is_a_role) ||
	           (rflags & (ROLE_DO_INCOLS|ROLE_INCOL) &&
		    pat->action->is_a_incol) ||
	           (rflags & ROLE_DO_OTHER && pat->action->is_a_other) ||
	           (rflags & ROLE_DO_SRCH && pat->action->is_a_srch) ||
	           (rflags & ROLE_DO_SCORES && pat->action->is_a_score) ||
		   (rflags & ROLE_SCORE && (pat->action->scoreval
		                            || pat->action->scorevalhdrtok)) ||
		   (rflags & ROLE_DO_FILTER && pat->action->is_a_filter) ||
	           (rflags & ROLE_REPLY &&
		    (pat->action->repl_type == ROLE_REPL_YES ||
		     pat->action->repl_type == ROLE_REPL_NOCONF)) ||
	           (rflags & ROLE_FORWARD &&
		    (pat->action->forw_type == ROLE_FORW_YES ||
		     pat->action->forw_type == ROLE_FORW_NOCONF)) ||
	           (rflags & ROLE_COMPOSE &&
		    (pat->action->comp_type == ROLE_COMP_YES ||
		     pat->action->comp_type == ROLE_COMP_NOCONF)) ||
	           (rflags & ROLE_OLD_FILT) ||
	           (rflags & ROLE_OLD_SCORE) ||
	           (rflags & ROLE_OLD_PAT)))
		||
		 pat->inherit);
	pat = next_any_pattern(pstate))
      ;
    
    return(pat);
}


/*
 * Just like first_any_pattern.
 */
PAT_S *
last_any_pattern(PAT_STATE *pstate)
{
    PAT_LINE_S *patline = NULL;
    int         i;
    long        local_rflag;

    /*
     * The rest of pstate should be set before coming here.
     * In particular, the rflags should be set by a call to nonempty_patterns
     * or any_patterns, and cur_rflag_num should be set.
     */
    pstate->patlinecurrent = NULL;
    pstate->patcurrent     = NULL;

    for(i = pstate->cur_rflag_num; i >= 1; i--){

	local_rflag = 0L;

	switch(i){
	  case 1:
	    local_rflag = ROLE_DO_SRCH & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 2:
	    local_rflag = ROLE_DO_INCOLS & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 3:
	    local_rflag = ROLE_DO_ROLES & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 4:
	    local_rflag = ROLE_DO_FILTER & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 5:
	    local_rflag = ROLE_DO_SCORES & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 6:
	    local_rflag = ROLE_DO_OTHER & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 7:
	    local_rflag = ROLE_OLD_FILT & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case 8:
	    local_rflag = ROLE_OLD_SCORE & CANONICAL_RFLAGS(pstate->rflags);
	    break;

	  case PATTERN_N:
	    local_rflag = ROLE_OLD_PAT & CANONICAL_RFLAGS(pstate->rflags);
	    break;
	}

	if(local_rflag){
	    SET_PATTYPE(local_rflag | (pstate->rflags & PAT_USE_MASK));

	    pstate->patlinecurrent = NULL;
	    pstate->patcurrent     = NULL;

	    if(*cur_pat_h){
		/* Find last patline with a pat */
		for(patline = (*cur_pat_h)->patlinehead;
		    patline;
		    patline = patline->next)
		  if(patline->last)
		    pstate->patlinecurrent = patline;
		
		if(pstate->patlinecurrent)
		  pstate->patcurrent = pstate->patlinecurrent->last;
	    }

	    if(pstate->patcurrent)
	      pstate->cur_rflag_num = i;

	    if(pstate->patcurrent)
	      break;
	}
    }

    return(pstate->patcurrent);
}


/*
 * Return last pattern of the specified types. These types were set by a
 * previous call to any_patterns or nonempty_patterns.
 *
 * Args --  pstate  pattern state. This is set here and passed back for
 *                  use by prev_pattern. Must be non-null.
 *                  It must have been initialized previously by a call to
 *                  nonempty_patterns or any_patterns.
 */
PAT_S *
last_pattern(PAT_STATE *pstate)
{
    PAT_S           *pat;
    long             rflags;

    pstate->cur_rflag_num = PATTERN_N;

    rflags = pstate->rflags;

    for(pat = last_any_pattern(pstate);
	pat && !((pat->action &&
		  ((rflags & ROLE_DO_ROLES && pat->action->is_a_role) ||
	           (rflags & (ROLE_DO_INCOLS|ROLE_INCOL) &&
		    pat->action->is_a_incol) ||
	           (rflags & ROLE_DO_OTHER && pat->action->is_a_other) ||
	           (rflags & ROLE_DO_SRCH && pat->action->is_a_srch) ||
	           (rflags & ROLE_DO_SCORES && pat->action->is_a_score) ||
		   (rflags & ROLE_SCORE && (pat->action->scoreval
		                            || pat->action->scorevalhdrtok)) ||
		   (rflags & ROLE_DO_FILTER && pat->action->is_a_filter) ||
	           (rflags & ROLE_REPLY &&
		    (pat->action->repl_type == ROLE_REPL_YES ||
		     pat->action->repl_type == ROLE_REPL_NOCONF)) ||
	           (rflags & ROLE_FORWARD &&
		    (pat->action->forw_type == ROLE_FORW_YES ||
		     pat->action->forw_type == ROLE_FORW_NOCONF)) ||
	           (rflags & ROLE_COMPOSE &&
		    (pat->action->comp_type == ROLE_COMP_YES ||
		     pat->action->comp_type == ROLE_COMP_NOCONF)) ||
	           (rflags & ROLE_OLD_FILT) ||
	           (rflags & ROLE_OLD_SCORE) ||
	           (rflags & ROLE_OLD_PAT)))
		||
		 pat->inherit);
	pat = prev_any_pattern(pstate))
      ;
    
    return(pat);
}

    
/*
 * This assumes that pstate is valid.
 */
PAT_S *
next_any_pattern(PAT_STATE *pstate)
{
    PAT_LINE_S *patline;

    if(pstate->patlinecurrent){
	if(pstate->patcurrent && pstate->patcurrent->next)
	  pstate->patcurrent = pstate->patcurrent->next;
	else{
	    /* Find next patline with a pat */
	    for(patline = pstate->patlinecurrent->next;
		patline && !patline->first;
		patline = patline->next)
	      ;
	    
	    if(patline){
		pstate->patlinecurrent = patline;
		pstate->patcurrent     = patline->first;
	    }
	    else{
		pstate->patlinecurrent = NULL;
		pstate->patcurrent     = NULL;
	    }
	}
    }

    /* we've reached the last, try the next rflag_num (the next pattern type) */
    if(!pstate->patcurrent){
	pstate->cur_rflag_num++;
	pstate->patcurrent = first_any_pattern(pstate);
    }

    return(pstate->patcurrent);
}


/*
 * Return next pattern of the specified types. These types were set by a
 * previous call to any_patterns or nonempty_patterns.
 *
 * Args -- pstate  pattern state. This is set by first_pattern or last_pattern.
 */
PAT_S *
next_pattern(PAT_STATE *pstate)
{
    PAT_S           *pat;
    long             rflags;

    rflags = pstate->rflags;

    for(pat = next_any_pattern(pstate);
	pat && !((pat->action &&
		  ((rflags & ROLE_DO_ROLES && pat->action->is_a_role) ||
	           (rflags & (ROLE_DO_INCOLS|ROLE_INCOL) &&
		    pat->action->is_a_incol) ||
	           (rflags & ROLE_DO_OTHER && pat->action->is_a_other) ||
	           (rflags & ROLE_DO_SRCH && pat->action->is_a_srch) ||
	           (rflags & ROLE_DO_SCORES && pat->action->is_a_score) ||
		   (rflags & ROLE_SCORE && (pat->action->scoreval
		                            || pat->action->scorevalhdrtok)) ||
		   (rflags & ROLE_DO_FILTER && pat->action->is_a_filter) ||
	           (rflags & ROLE_REPLY &&
		    (pat->action->repl_type == ROLE_REPL_YES ||
		     pat->action->repl_type == ROLE_REPL_NOCONF)) ||
	           (rflags & ROLE_FORWARD &&
		    (pat->action->forw_type == ROLE_FORW_YES ||
		     pat->action->forw_type == ROLE_FORW_NOCONF)) ||
	           (rflags & ROLE_COMPOSE &&
		    (pat->action->comp_type == ROLE_COMP_YES ||
		     pat->action->comp_type == ROLE_COMP_NOCONF)) ||
	           (rflags & ROLE_OLD_FILT) ||
	           (rflags & ROLE_OLD_SCORE) ||
	           (rflags & ROLE_OLD_PAT)))
		||
		 pat->inherit);
	pat = next_any_pattern(pstate))
      ;
    
    return(pat);
}

    
/*
 * This assumes that pstate is valid.
 */
PAT_S *
prev_any_pattern(PAT_STATE *pstate)
{
    PAT_LINE_S *patline;

    if(pstate->patlinecurrent){
	if(pstate->patcurrent && pstate->patcurrent->prev)
	  pstate->patcurrent = pstate->patcurrent->prev;
	else{
	    /* Find prev patline with a pat */
	    for(patline = pstate->patlinecurrent->prev;
		patline && !patline->last;
		patline = patline->prev)
	      ;
	    
	    if(patline){
		pstate->patlinecurrent = patline;
		pstate->patcurrent     = patline->last;
	    }
	    else{
		pstate->patlinecurrent = NULL;
		pstate->patcurrent     = NULL;
	    }
	}
    }

    if(!pstate->patcurrent){
	pstate->cur_rflag_num--;
	pstate->patcurrent = last_any_pattern(pstate);
    }

    return(pstate->patcurrent);
}


/*
 * Return prev pattern of the specified types. These types were set by a
 * previous call to any_patterns or nonempty_patterns.
 *
 * Args -- pstate  pattern state. This is set by first_pattern or last_pattern.
 */
PAT_S *
prev_pattern(PAT_STATE *pstate)
{
    PAT_S           *pat;
    long             rflags;

    rflags = pstate->rflags;

    for(pat = prev_any_pattern(pstate);
	pat && !((pat->action &&
		  ((rflags & ROLE_DO_ROLES && pat->action->is_a_role) ||
	           (rflags & (ROLE_DO_INCOLS|ROLE_INCOL) &&
		    pat->action->is_a_incol) ||
	           (rflags & ROLE_DO_OTHER && pat->action->is_a_other) ||
	           (rflags & ROLE_DO_SRCH && pat->action->is_a_srch) ||
	           (rflags & ROLE_DO_SCORES && pat->action->is_a_score) ||
		   (rflags & ROLE_SCORE && (pat->action->scoreval
		                            || pat->action->scorevalhdrtok)) ||
		   (rflags & ROLE_DO_FILTER && pat->action->is_a_filter) ||
	           (rflags & ROLE_REPLY &&
		    (pat->action->repl_type == ROLE_REPL_YES ||
		     pat->action->repl_type == ROLE_REPL_NOCONF)) ||
	           (rflags & ROLE_FORWARD &&
		    (pat->action->forw_type == ROLE_FORW_YES ||
		     pat->action->forw_type == ROLE_FORW_NOCONF)) ||
	           (rflags & ROLE_COMPOSE &&
		    (pat->action->comp_type == ROLE_COMP_YES ||
		     pat->action->comp_type == ROLE_COMP_NOCONF)) ||
	           (rflags & ROLE_OLD_FILT) ||
	           (rflags & ROLE_OLD_SCORE) ||
	           (rflags & ROLE_OLD_PAT)))
		||
		 pat->inherit);
	pat = prev_any_pattern(pstate))
      ;
    
    return(pat);
}

    
/*
 * Rflags may be more than one pattern type OR'd together.
 */
int
write_patterns(long int rflags)
{
    int canon_rflags;
    int err = 0;

    dprint((7, "write_patterns(0x%x)\n", rflags));

    canon_rflags = CANONICAL_RFLAGS(rflags);

    if(canon_rflags & ROLE_DO_INCOLS)
      err += sub_write_patterns(ROLE_DO_INCOLS | (rflags & PAT_USE_MASK));
    if(!err && canon_rflags & ROLE_DO_OTHER)
      err += sub_write_patterns(ROLE_DO_OTHER  | (rflags & PAT_USE_MASK));
    if(!err && canon_rflags & ROLE_DO_FILTER)
      err += sub_write_patterns(ROLE_DO_FILTER | (rflags & PAT_USE_MASK));
    if(!err && canon_rflags & ROLE_DO_SCORES)
      err += sub_write_patterns(ROLE_DO_SCORES | (rflags & PAT_USE_MASK));
    if(!err && canon_rflags & ROLE_DO_ROLES)
      err += sub_write_patterns(ROLE_DO_ROLES  | (rflags & PAT_USE_MASK));
    if(!err && canon_rflags & ROLE_DO_SRCH)
      err += sub_write_patterns(ROLE_DO_SRCH   | (rflags & PAT_USE_MASK));

    if(!err && !(rflags & PAT_USE_CHANGED))
      write_pinerc(ps_global, (rflags & PAT_USE_MAIN) ? Main : Post, WRP_NONE);

    return(err);
}


int
sub_write_patterns(long int rflags)
{
    int            err = 0, lineno = 0;
    char         **lvalue = NULL;
    PAT_LINE_S    *patline;

    SET_PATTYPE(rflags);

    if(!(*cur_pat_h)){
	q_status_message(SM_ORDER | SM_DING, 3, 4,
			  "Unknown error saving patterns");
	return(-1);
    }

    if((*cur_pat_h)->dirtypinerc){
	/* Count how many lines will be in patterns variable */
	for(patline = (*cur_pat_h)->patlinehead;
	    patline;
	    patline = patline->next)
	  lineno++;
    
	lvalue = (char **)fs_get((lineno+1)*sizeof(char *));
	memset(lvalue, 0, (lineno+1) * sizeof(char *));
    }

    for(patline = (*cur_pat_h)->patlinehead, lineno = 0;
	!err && patline;
	patline = patline->next, lineno++){
	if(patline->type == File)
	  err = write_pattern_file((*cur_pat_h)->dirtypinerc
				      ? &lvalue[lineno] : NULL, patline);
	else if(patline->type == Literal && (*cur_pat_h)->dirtypinerc)
	  err = write_pattern_lit(&lvalue[lineno], patline);
	else if(patline->type == Inherit)
	  err = write_pattern_inherit((*cur_pat_h)->dirtypinerc
				      ? &lvalue[lineno] : NULL, patline);
    }

    if((*cur_pat_h)->dirtypinerc){
	if(err)
	  free_list_array(&lvalue);
	else{
	    char ***alval;
	    struct variable *var;

	    if(rflags & ROLE_DO_ROLES)
	      var = &ps_global->vars[V_PAT_ROLES];
	    else if(rflags & ROLE_DO_OTHER)
	      var = &ps_global->vars[V_PAT_OTHER];
	    else if(rflags & ROLE_DO_FILTER)
	      var = &ps_global->vars[V_PAT_FILTS];
	    else if(rflags & ROLE_DO_SCORES)
	      var = &ps_global->vars[V_PAT_SCORES];
	    else if(rflags & ROLE_DO_INCOLS)
	      var = &ps_global->vars[V_PAT_INCOLS];
	    else if(rflags & ROLE_DO_SRCH)
	      var = &ps_global->vars[V_PAT_SRCH];

	    alval = (rflags & PAT_USE_CHANGED) ? &(var->changed_val.l)
	      : ALVAL(var, (rflags & PAT_USE_MAIN) ? Main : Post);
	    if(*alval)
	      free_list_array(alval);
	    
	    if(rflags & PAT_USE_CHANGED) var->is_changed_val = 1;

	    *alval = lvalue;

	    if(!(rflags & PAT_USE_CHANGED))
	      set_current_val(var, TRUE, TRUE);
	}
    }

    if(!err)
      (*cur_pat_h)->dirtypinerc = 0;

    return(err);
}


/*
 * Write pattern lines into a file.
 *
 * Args  lvalue -- Pointer to char * to fill in variable value
 *      patline -- 
 *
 * Returns  0 -- all is ok, lvalue has been filled in, file has been written
 *       else -- error, lvalue untouched, file not written
 */
int
write_pattern_file(char **lvalue, PAT_LINE_S *patline)
{
    char  *p, *tfile;
    int    fd = -1, err = 0;
    FILE  *fp_new;
    PAT_S *pat;

    dprint((7, "write_pattern_file(%s)\n",
	   (patline && patline->filepath) ? patline->filepath : "?"));

    if(lvalue){
	size_t l;

	l = strlen(patline->filename) + 5;
	p = (char *) fs_get((l+1) * sizeof(char));
	strncpy(p, "FILE:", l+1);
	p[l] = '\0';
	strncat(p, patline->filename, l+1-1-strlen(p));
	p[l] = '\0';
	*lvalue = p;
    }

    if(patline->readonly || !patline->dirty)	/* doesn't need writing */
      return(err);

    /* Get a tempfile to write the patterns into */
    if(((tfile = tempfile_in_same_dir(patline->filepath, ".pt", NULL)) == NULL)
       || ((fd = our_open(tfile, O_TRUNC|O_WRONLY|O_CREAT|O_BINARY, 0600)) < 0)
       || ((fp_new = fdopen(fd, "w")) == NULL)){
	q_status_message1(SM_ORDER | SM_DING, 3, 4,
			  "Can't write in directory containing file \"%.200s\"",
			  patline->filepath);
	if(tfile){
	    our_unlink(tfile);
	    fs_give((void **)&tfile);
	}
	
	if(fd >= 0)
	  close(fd);

	return(-1);
    }

    dprint((9, "write_pattern_file: writing into %s\n",
	   tfile ? tfile : "?"));
    
    if(fprintf(fp_new, "%s %s\n", PATTERN_MAGIC, PATTERN_FILE_VERS) == EOF)
      err--;

    for(pat = patline->first; !err && pat; pat = pat->next){
	if((p = data_for_patline(pat)) != NULL){
	    if(fprintf(fp_new, "%s\n", p) == EOF)
	      err--;
	    
	    fs_give((void **)&p);
	}
    }

    if(err || fclose(fp_new) == EOF){
	if(err)
	  (void)fclose(fp_new);

	err--;
	q_status_message2(SM_ORDER | SM_DING, 3, 4,
			  "I/O error: \"%.200s\": %.200s",
			  tfile, error_description(errno));
    }

    if(!err && rename_file(tfile, patline->filepath) < 0){
	err--;
	q_status_message3(SM_ORDER | SM_DING, 3, 4,
			  _("Error renaming \"%s\" to \"%s\": %s"),
			  tfile, patline->filepath, error_description(errno));
	dprint((2,
	       "write_pattern_file: Error renaming (%s,%s): %s\n",
	       tfile ? tfile : "?",
	       (patline && patline->filepath) ? patline->filepath : "?",
	       error_description(errno)));
    }

    if(tfile){
	our_unlink(tfile);
	fs_give((void **)&tfile);
    }

    if(!err)
      patline->dirty = 0;

    return(err);
}


/*
 * Write literal pattern lines into lvalue (pinerc variable).
 *
 * Args  lvalue -- Pointer to char * to fill in variable value
 *      patline -- 
 *
 * Returns  0 -- all is ok, lvalue has been filled in, file has been written
 *       else -- error, lvalue untouched, file not written
 */
int
write_pattern_lit(char **lvalue, PAT_LINE_S *patline)
{
    char  *p = NULL;
    int    err = 0;
    PAT_S *pat;

    pat = patline ? patline->first : NULL;
    
    if(pat && lvalue && (p = data_for_patline(pat)) != NULL){
	size_t l;

	l = strlen(p) + 4;
	*lvalue = (char *) fs_get((l+1) * sizeof(char));
	strncpy(*lvalue, "LIT:", l+1);
	(*lvalue)[l] = '\0';
	strncat(*lvalue, p, l+1-1-strlen(*lvalue));
	(*lvalue)[l] = '\0';
    }
    else{
	q_status_message(SM_ORDER | SM_DING, 3, 4,
			 _("Unknown error saving pattern variable"));
	err--;
    }
    
    if(p)
      fs_give((void **)&p);

    return(err);
}


int
write_pattern_inherit(char **lvalue, PAT_LINE_S *patline)
{
    int    err = 0;

    if(patline && patline->type == Inherit && lvalue)
      *lvalue = cpystr(INHERIT);
    else
      err--;
    
    return(err);
}



char *
data_for_patline(PAT_S *pat)
{
    char          *p = NULL, *q, *to_pat = NULL,
		  *news_pat = NULL, *from_pat = NULL,
		  *sender_pat = NULL, *cc_pat = NULL, *subj_pat = NULL,
		  *arb_pat = NULL, *fldr_type_pat = NULL, *fldr_pat = NULL,
		  *afrom_type_pat = NULL, *abooks_pat = NULL,
		  *alltext_pat = NULL, *scorei_pat = NULL, *recip_pat = NULL,
		  *keyword_pat = NULL, *charset_pat = NULL,
		  *bodytext_pat = NULL, *age_pat = NULL, *sentdate = NULL,
		  *size_pat = NULL,
		  *category_cmd = NULL, *category_pat = NULL,
		  *category_lim = NULL,
		  *partic_pat = NULL, *stat_new_val = NULL,
		  *stat_rec_val = NULL,
		  *stat_imp_val = NULL, *stat_del_val = NULL,
		  *stat_ans_val = NULL, *stat_8bit_val = NULL,
		  *stat_bom_val = NULL, *stat_boy_val = NULL,
		  *from_act = NULL, *replyto_act = NULL, *fcc_act = NULL,
		  *sig_act = NULL, *nick = NULL, *templ_act = NULL,
		  *litsig_act = NULL, *cstm_act = NULL, *smtp_act = NULL,
                  *nntp_act = NULL, *comment = NULL,
		  *repl_val = NULL, *forw_val = NULL, *comp_val = NULL,
		  *incol_act = NULL, *inherit_nick = NULL,
		  *score_act = NULL, *hdrtok_act = NULL,
		  *sort_act = NULL, *iform_act = NULL, *start_act = NULL,
		  *folder_act = NULL, *filt_ifnotdel = NULL,
		  *filt_nokill = NULL, *filt_del_val = NULL,
		  *filt_imp_val = NULL, *filt_ans_val = NULL,
		  *filt_new_val = NULL, *filt_nonterm = NULL,
		  *keyword_set = NULL, *keyword_clr = NULL;
    int            to_not = 0, news_not = 0, from_not = 0,
		   sender_not = 0, cc_not = 0, subj_not = 0,
		   partic_not = 0, recip_not = 0, alltext_not, bodytext_not,
		   keyword_not = 0, charset_not = 0;
    size_t         l;
    ACTION_S      *action = NULL;
    NAMEVAL_S     *f;

    if(!pat)
      return(p);

    if((pat->patgrp && pat->patgrp->bogus)
       || (pat->action && pat->action->bogus)){
	if(pat->raw)
	  p = cpystr(pat->raw);

	return(p);
    }

    if(pat->patgrp){
	if(pat->patgrp->nick)
	  if((nick = add_pat_escapes(pat->patgrp->nick)) && !*nick)
	    fs_give((void **) &nick);

	if(pat->patgrp->comment)
	  if((comment = add_pat_escapes(pat->patgrp->comment)) && !*comment)
	    fs_give((void **) &comment);

	if(pat->patgrp->to){
	    to_pat = pattern_to_config(pat->patgrp->to);
	    to_not = pat->patgrp->to->not;
	}

	if(pat->patgrp->from){
	    from_pat = pattern_to_config(pat->patgrp->from);
	    from_not = pat->patgrp->from->not;
	}

	if(pat->patgrp->sender){
	    sender_pat = pattern_to_config(pat->patgrp->sender);
	    sender_not = pat->patgrp->sender->not;
	}

	if(pat->patgrp->cc){
	    cc_pat = pattern_to_config(pat->patgrp->cc);
	    cc_not = pat->patgrp->cc->not;
	}

	if(pat->patgrp->recip){
	    recip_pat = pattern_to_config(pat->patgrp->recip);
	    recip_not = pat->patgrp->recip->not;
	}

	if(pat->patgrp->partic){
	    partic_pat = pattern_to_config(pat->patgrp->partic);
	    partic_not = pat->patgrp->partic->not;
	}

	if(pat->patgrp->news){
	    news_pat = pattern_to_config(pat->patgrp->news);
	    news_not = pat->patgrp->news->not;
	}

	if(pat->patgrp->subj){
	    subj_pat = pattern_to_config(pat->patgrp->subj);
	    subj_not = pat->patgrp->subj->not;
	}

	if(pat->patgrp->alltext){
	    alltext_pat = pattern_to_config(pat->patgrp->alltext);
	    alltext_not = pat->patgrp->alltext->not;
	}

	if(pat->patgrp->bodytext){
	    bodytext_pat = pattern_to_config(pat->patgrp->bodytext);
	    bodytext_not = pat->patgrp->bodytext->not;
	}

	if(pat->patgrp->keyword){
	    keyword_pat = pattern_to_config(pat->patgrp->keyword);
	    keyword_not = pat->patgrp->keyword->not;
	}

	if(pat->patgrp->charsets){
	    charset_pat = pattern_to_config(pat->patgrp->charsets);
	    charset_not = pat->patgrp->charsets->not;
	}

	if(pat->patgrp->arbhdr){
	    ARBHDR_S *a;
	    char     *p1 = NULL, *p2 = NULL, *p3 = NULL, *p4 = NULL;
	    int       len = 0;

	    /* This is brute force dumb, but who cares? */
	    for(a = pat->patgrp->arbhdr; a; a = a->next){
		if(a->field && a->field[0]){
		    p1 = pattern_to_string(a->p);
		    p1 = p1 ? p1 : cpystr("");
		    l = strlen(a->field)+strlen(p1)+1;
		    p2 = (char *) fs_get((l+1) * sizeof(char));
		    snprintf(p2, l+1, "%s=%s", a->field, p1);
		    p3 = add_pat_escapes(p2);
		    l = strlen(p3)+6;
		    p4 = (char *) fs_get((l+1) * sizeof(char));
		    snprintf(p4, l+1, "/%s%sARB%s",
			    (a->p && a->p->not) ? "!" : "",
			    a->isemptyval ? "E" : "", p3);
		    len += strlen(p4);

		    if(p1)
		      fs_give((void **)&p1);
		    if(p2)
		      fs_give((void **)&p2);
		    if(p3)
		      fs_give((void **)&p3);
		    if(p4)
		      fs_give((void **)&p4);
		}
	    }

	    p = arb_pat = (char *)fs_get((len + 1) * sizeof(char));

	    for(a = pat->patgrp->arbhdr; a; a = a->next){
		if(a->field && a->field[0]){
		    p1 = pattern_to_string(a->p);
		    p1 = p1 ? p1 : cpystr("");
		    l = strlen(a->field)+strlen(p1)+1;
		    p2 = (char *) fs_get((l+1) * sizeof(char));
		    snprintf(p2, l+1, "%s=%s", a->field, p1);
		    p3 = add_pat_escapes(p2);
		    l = strlen(p3)+6;
		    p4 = (char *) fs_get((l+1) * sizeof(char));
		    snprintf(p4, l+1, "/%s%sARB%s",
			    (a->p && a->p->not) ? "!" : "",
			    a->isemptyval ? "E" : "", p3);
		    sstrncpy(&p, p4, len+1-(p-arb_pat));

		    if(p1)
		      fs_give((void **)&p1);
		    if(p2)
		      fs_give((void **)&p2);
		    if(p3)
		      fs_give((void **)&p3);
		    if(p4)
		      fs_give((void **)&p4);
		}
	    }

	    arb_pat[len] = '\0';
	}

	if(pat->patgrp->age_uses_sentdate)
	  sentdate = cpystr("/SENTDATE=1");

	if(pat->patgrp->do_score){
	    p = stringform_of_intvl(pat->patgrp->score);
	    if(p){
		scorei_pat = add_pat_escapes(p);
		fs_give((void **)&p);
	    }
	}

	if(pat->patgrp->do_age){
	    p = stringform_of_intvl(pat->patgrp->age);
	    if(p){
		age_pat = add_pat_escapes(p);
		fs_give((void **)&p);
	    }
	}

	if(pat->patgrp->do_size){
	    p = stringform_of_intvl(pat->patgrp->size);
	    if(p){
		size_pat = add_pat_escapes(p);
		fs_give((void **)&p);
	    }
	}

	if(pat->patgrp->category_cmd && pat->patgrp->category_cmd[0]){
	    size_t sz;
	    char **l, *q;

	    /* concatenate into string with commas first */
	    sz = 0;
	    for(l = pat->patgrp->category_cmd; l[0] && l[0][0]; l++)
	      sz += strlen(l[0]) + 1;

	    if(sz){
		char *p;
		int   first_one = 1;

		q = (char *)fs_get(sz);
		memset(q, 0, sz);
		p = q;
		for(l = pat->patgrp->category_cmd; l[0] && l[0][0]; l++){
		    if(!first_one)
		      sstrncpy(&p, ",", sz-(p-q));

		    first_one = 0;
		    sstrncpy(&p, l[0], sz-(p-q));
		}

		q[sz-1] = '\0';

		category_cmd = add_pat_escapes(q);
		fs_give((void **)&q);
	    }
	}

	if(pat->patgrp->do_cat){
	    p = stringform_of_intvl(pat->patgrp->cat);
	    if(p){
		category_pat = add_pat_escapes(p);
		fs_give((void **)&p);
	    }
	}

	if(pat->patgrp->cat_lim != -1L){
	    category_lim = (char *) fs_get(20 * sizeof(char));
	    snprintf(category_lim, 20, "%ld", pat->patgrp->cat_lim);
	}

	if((f = pat_fldr_types(pat->patgrp->fldr_type)) != NULL)
	  fldr_type_pat = f->shortname;

	if(pat->patgrp->folder)
	  fldr_pat = pattern_to_config(pat->patgrp->folder);

	if((f = inabook_fldr_types(pat->patgrp->inabook)) != NULL
	   && f->value != IAB_DEFL)
	  afrom_type_pat = f->shortname;

	if(pat->patgrp->abooks)
	  abooks_pat = pattern_to_config(pat->patgrp->abooks);

	if(pat->patgrp->stat_new != PAT_STAT_EITHER &&
	   (f = role_status_types(pat->patgrp->stat_new)) != NULL)
	  stat_new_val = f->shortname;

	if(pat->patgrp->stat_rec != PAT_STAT_EITHER &&
	   (f = role_status_types(pat->patgrp->stat_rec)) != NULL)
	  stat_rec_val = f->shortname;

	if(pat->patgrp->stat_del != PAT_STAT_EITHER &&
	   (f = role_status_types(pat->patgrp->stat_del)) != NULL)
	  stat_del_val = f->shortname;

	if(pat->patgrp->stat_ans != PAT_STAT_EITHER &&
	   (f = role_status_types(pat->patgrp->stat_ans)) != NULL)
	  stat_ans_val = f->shortname;

	if(pat->patgrp->stat_imp != PAT_STAT_EITHER &&
	   (f = role_status_types(pat->patgrp->stat_imp)) != NULL)
	  stat_imp_val = f->shortname;

	if(pat->patgrp->stat_8bitsubj != PAT_STAT_EITHER &&
	   (f = role_status_types(pat->patgrp->stat_8bitsubj)) != NULL)
	  stat_8bit_val = f->shortname;

	if(pat->patgrp->stat_bom != PAT_STAT_EITHER &&
	   (f = role_status_types(pat->patgrp->stat_bom)) != NULL)
	  stat_bom_val = f->shortname;

	if(pat->patgrp->stat_boy != PAT_STAT_EITHER &&
	   (f = role_status_types(pat->patgrp->stat_boy)) != NULL)
	  stat_boy_val = f->shortname;
    }

    if(pat->action){
	action = pat->action;

	if(action->is_a_score){
	    if(action->scoreval != 0L &&
	       action->scoreval >= SCORE_MIN && action->scoreval <= SCORE_MAX){
		score_act = (char *) fs_get(5 * sizeof(char));
		snprintf(score_act, 5, "%ld", pat->action->scoreval);
	    }

	    if(action->scorevalhdrtok)
	      hdrtok_act = hdrtok_to_config(action->scorevalhdrtok);
	}

	if(action->is_a_role){
	    if(action->inherit_nick)
	      inherit_nick = add_pat_escapes(action->inherit_nick);
	    if(action->fcc)
	      fcc_act = add_pat_escapes(action->fcc);
	    if(action->litsig)
	      litsig_act = add_pat_escapes(action->litsig);
	    if(action->sig)
	      sig_act = add_pat_escapes(action->sig);
	    if(action->template)
	      templ_act = add_pat_escapes(action->template);

	    if(action->cstm){
		size_t sz;
		char **l, *q;

		/* concatenate into string with commas first */
		sz = 0;
		for(l = action->cstm; l[0] && l[0][0]; l++)
		  sz += strlen(l[0]) + 1;

		if(sz){
		    char *p;
		    int   first_one = 1;

		    q = (char *)fs_get(sz);
		    memset(q, 0, sz);
		    p = q;
		    for(l = action->cstm; l[0] && l[0][0]; l++){
			if((!struncmp(l[0], "from", 4) &&
			   (l[0][4] == ':' || l[0][4] == '\0')) ||
			   (!struncmp(l[0], "reply-to", 8) &&
			   (l[0][8] == ':' || l[0][8] == '\0')))
			  continue;

			if(!first_one)
			  sstrncpy(&p, ",", sz-(p-q));

		        first_one = 0;
			sstrncpy(&p, l[0], sz-(p-q));
		    }

		    q[sz-1] = '\0';

		    cstm_act = add_pat_escapes(q);
		    fs_give((void **)&q);
		}
	    }

	    if(action->smtp){
		size_t sz;
		char **l, *q;

		/* concatenate into string with commas first */
		sz = 0;
		for(l = action->smtp; l[0] && l[0][0]; l++)
		  sz += strlen(l[0]) + 1;

		if(sz){
		    char *p;
		    int   first_one = 1;

		    q = (char *)fs_get(sz);
		    memset(q, 0, sz);
		    p = q;
		    for(l = action->smtp; l[0] && l[0][0]; l++){
			if(!first_one)
			  sstrncpy(&p, ",", sz-(p-q));

		        first_one = 0;
			sstrncpy(&p, l[0], sz-(p-q));
		    }

		    q[sz-1] = '\0';

		    smtp_act = add_pat_escapes(q);
		    fs_give((void **)&q);
		}
	    }

	    if(action->nntp){
		size_t sz;
		char **l, *q;

		/* concatenate into string with commas first */
		sz = 0;
		for(l = action->nntp; l[0] && l[0][0]; l++)
		  sz += strlen(l[0]) + 1;

		if(sz){
		    char *p;
		    int   first_one = 1;

		    q = (char *)fs_get(sz);
		    memset(q, 0, sz);
		    p = q;
		    for(l = action->nntp; l[0] && l[0][0]; l++){
			if(!first_one)
			  sstrncpy(&p, ",", sz-(p-q));

		        first_one = 0;
			sstrncpy(&p, l[0], sz-(p-q));
		    }

		    q[sz-1] = '\0';

		    nntp_act = add_pat_escapes(q);
		    fs_give((void **)&q);
		}
	    }

	    if((f = role_repl_types(action->repl_type)) != NULL)
	      repl_val = f->shortname;

	    if((f = role_forw_types(action->forw_type)) != NULL)
	      forw_val = f->shortname;
	    
	    if((f = role_comp_types(action->comp_type)) != NULL)
	      comp_val = f->shortname;
	}
	
	if(action->is_a_incol && action->incol){
	    char *ptr, buf[256], *p1, *p2;

	    ptr = buf;
	    memset(buf, 0, sizeof(buf));
	    sstrncpy(&ptr, "/FG=", sizeof(buf)-(ptr-buf));
	    sstrncpy(&ptr, (p1=add_pat_escapes(action->incol->fg)), sizeof(buf)-(ptr-buf));
	    sstrncpy(&ptr, "/BG=", sizeof(buf)-(ptr-buf));
	    sstrncpy(&ptr, (p2=add_pat_escapes(action->incol->bg)), sizeof(buf)-(ptr-buf));
	    buf[sizeof(buf)-1] = '\0';
	    /* the colors will be doubly escaped */
	    incol_act = add_pat_escapes(buf);
	    if(p1)
	      fs_give((void **)&p1);

	    if(p2)
	      fs_give((void **)&p2);
	}

	if(action->is_a_other){
	    char buf[256];

	    if(action->sort_is_set){
		snprintf(buf, sizeof(buf), "%.50s%.50s",
			sort_name(action->sortorder),
			action->revsort ? "/Reverse" : "");
		sort_act = add_pat_escapes(buf);
	    }

	    if(action->index_format)
	      iform_act = add_pat_escapes(action->index_format);

	    if(action->startup_rule != IS_NOTSET &&
	       (f = startup_rules(action->startup_rule)) != NULL)
	      start_act = S_OR_L(f);
	}

	if(action->is_a_role && action->from){
	    char *bufp;
	    size_t len;

	    len = est_size(action->from);
	    bufp = (char *) fs_get(len * sizeof(char));
	    p = addr_string_mult(action->from, bufp, len);
	    if(p){
		from_act = add_pat_escapes(p);
		fs_give((void **)&p);
	    }
	}

	if(action->is_a_role && action->replyto){
	    char *bufp;
	    size_t len;

	    len = est_size(action->replyto);
	    bufp = (char *) fs_get(len * sizeof(char));
	    p = addr_string_mult(action->replyto, bufp, len);
	    if(p){
		replyto_act = add_pat_escapes(p);
		fs_give((void **)&p);
	    }
	}

	if(action->is_a_filter){
	    if(action->folder){
		if((folder_act = pattern_to_config(action->folder)) != NULL){
		    if(action->move_only_if_not_deleted)
		      filt_ifnotdel = cpystr("/NOTDEL=1");
		}
	    }

	    if(action->keyword_set)
	      keyword_set = pattern_to_config(action->keyword_set);

	    if(action->keyword_clr)
	      keyword_clr = pattern_to_config(action->keyword_clr);

	    if(!action->kill)
	      filt_nokill = cpystr("/NOKILL=1");
	    
	    if(action->non_terminating)
	      filt_nonterm = cpystr("/NONTERM=1");
	    
	    if(action->state_setting_bits){
		char buf[256];
		int  dval, nval, ival, aval;

		buf[0] = '\0';
		p = buf;

		convert_statebits_to_vals(action->state_setting_bits,
					  &dval, &aval, &ival, &nval);
		if(dval != ACT_STAT_LEAVE &&
		   (f = msg_state_types(dval)) != NULL)
		  filt_del_val = f->shortname;

		if(aval != ACT_STAT_LEAVE &&
		   (f = msg_state_types(aval)) != NULL)
		  filt_ans_val = f->shortname;

		if(ival != ACT_STAT_LEAVE &&
		   (f = msg_state_types(ival)) != NULL)
		  filt_imp_val = f->shortname;

		if(nval != ACT_STAT_LEAVE &&
		   (f = msg_state_types(nval)) != NULL)
		  filt_new_val = f->shortname;
	    }
	}
    }

    l = strlen(nick ? nick : "Alternate Role") +
	strlen(comment ? comment : "") +
	strlen(to_pat ? to_pat : "") +
	strlen(from_pat ? from_pat : "") +
	strlen(sender_pat ? sender_pat : "") +
	strlen(cc_pat ? cc_pat : "") +
	strlen(recip_pat ? recip_pat : "") +
	strlen(partic_pat ? partic_pat : "") +
	strlen(news_pat ? news_pat : "") +
	strlen(subj_pat ? subj_pat : "") +
	strlen(alltext_pat ? alltext_pat : "") +
	strlen(bodytext_pat ? bodytext_pat : "") +
	strlen(arb_pat ? arb_pat : "") +
	strlen(scorei_pat ? scorei_pat : "") +
	strlen(keyword_pat ? keyword_pat : "") +
	strlen(charset_pat ? charset_pat : "") +
	strlen(age_pat ? age_pat : "") +
	strlen(size_pat ? size_pat : "") +
	strlen(category_cmd ? category_cmd : "") +
	strlen(category_pat ? category_pat : "") +
	strlen(category_lim ? category_lim : "") +
	strlen(fldr_pat ? fldr_pat : "") +
	strlen(abooks_pat ? abooks_pat : "") +
	strlen(sentdate ? sentdate : "") +
	strlen(inherit_nick ? inherit_nick : "") +
	strlen(score_act ? score_act : "") +
	strlen(hdrtok_act ? hdrtok_act : "") +
	strlen(from_act ? from_act : "") +
	strlen(replyto_act ? replyto_act : "") +
	strlen(fcc_act ? fcc_act : "") +
	strlen(litsig_act ? litsig_act : "") +
	strlen(cstm_act ? cstm_act : "") +
	strlen(smtp_act ? smtp_act : "") +
	strlen(nntp_act ? nntp_act : "") +
	strlen(sig_act ? sig_act : "") +
	strlen(incol_act ? incol_act : "") +
	strlen(sort_act ? sort_act : "") +
	strlen(iform_act ? iform_act : "") +
	strlen(start_act ? start_act : "") +
	strlen(filt_ifnotdel ? filt_ifnotdel : "") +
	strlen(filt_nokill ? filt_nokill : "") +
	strlen(filt_nonterm ? filt_nonterm : "") +
	(folder_act ? (strlen(folder_act) + 8) : 0) +
	strlen(keyword_set ? keyword_set : "") +
	strlen(keyword_clr ? keyword_clr : "") +
	strlen(templ_act ? templ_act : "") + 540;
    /*
     * The +540 above is larger than needed but not everything is accounted
     * for with the strlens.
     */
    p = (char *) fs_get(l * sizeof(char));

    q = p;
    sstrncpy(&q, "pattern=\"/NICK=", l-(q-p));

    if(nick){
	sstrncpy(&q, nick, l-(q-p));
	fs_give((void **) &nick);
    }
    else
      sstrncpy(&q, "Alternate Role", l-(q-p));

    if(comment){
	sstrncpy(&q, "/", l-(q-p));
	sstrncpy(&q, "COMM=", l-(q-p));
	sstrncpy(&q, comment, l-(q-p));
	fs_give((void **) &comment);
    }

    if(to_pat){
	sstrncpy(&q, "/", l-(q-p));
	if(to_not)
	  sstrncpy(&q, "!", l-(q-p));

	sstrncpy(&q, "TO=", l-(q-p));
	sstrncpy(&q, to_pat, l-(q-p));
	fs_give((void **) &to_pat);
    }

    if(from_pat){
	sstrncpy(&q, "/", l-(q-p));
	if(from_not)
	  sstrncpy(&q, "!", l-(q-p));

	sstrncpy(&q, "FROM=", l-(q-p));
	sstrncpy(&q, from_pat, l-(q-p));
	fs_give((void **) &from_pat);
    }

    if(sender_pat){
	sstrncpy(&q, "/", l-(q-p));
	if(sender_not)
	  sstrncpy(&q, "!", l-(q-p));

	sstrncpy(&q, "SENDER=", l-(q-p));
	sstrncpy(&q, sender_pat, l-(q-p));
	fs_give((void **) &sender_pat);
    }

    if(cc_pat){
	sstrncpy(&q,"/", l-(q-p));
	if(cc_not)
	  sstrncpy(&q, "!", l-(q-p));

	sstrncpy(&q,"CC=", l-(q-p));
	sstrncpy(&q, cc_pat, l-(q-p));
	fs_give((void **) &cc_pat);
    }

    if(recip_pat){
	sstrncpy(&q, "/", l-(q-p));
	if(recip_not)
	  sstrncpy(&q, "!", l-(q-p));

	sstrncpy(&q, "RECIP=", l-(q-p));
	sstrncpy(&q, recip_pat, l-(q-p));
	fs_give((void **) &recip_pat);
    }

    if(partic_pat){
	sstrncpy(&q, "/", l-(q-p));
	if(partic_not)
	  sstrncpy(&q, "!", l-(q-p));

	sstrncpy(&q, "PARTIC=", l-(q-p));
	sstrncpy(&q, partic_pat, l-(q-p));
	fs_give((void **) &partic_pat);
    }

    if(news_pat){
	sstrncpy(&q, "/", l-(q-p));
	if(news_not)
	  sstrncpy(&q, "!", l-(q-p));

	sstrncpy(&q, "NEWS=", l-(q-p));
	sstrncpy(&q, news_pat, l-(q-p));
	fs_give((void **) &news_pat);
    }

    if(subj_pat){
	sstrncpy(&q, "/", l-(q-p));
	if(subj_not)
	  sstrncpy(&q, "!", l-(q-p));

	sstrncpy(&q, "SUBJ=", l-(q-p));
	sstrncpy(&q, subj_pat, l-(q-p));
	fs_give((void **)&subj_pat);
    }

    if(alltext_pat){
	sstrncpy(&q, "/", l-(q-p));
	if(alltext_not)
	  sstrncpy(&q, "!", l-(q-p));

	sstrncpy(&q, "ALL=", l-(q-p));
	sstrncpy(&q, alltext_pat, l-(q-p));
	fs_give((void **) &alltext_pat);
    }

    if(bodytext_pat){
	sstrncpy(&q, "/", l-(q-p));
	if(bodytext_not)
	  sstrncpy(&q, "!", l-(q-p));

	sstrncpy(&q, "BODY=", l-(q-p));
	sstrncpy(&q, bodytext_pat, l-(q-p));
	fs_give((void **) &bodytext_pat);
    }

    if(keyword_pat){
	sstrncpy(&q, "/", l-(q-p));
	if(keyword_not)
	  sstrncpy(&q, "!", l-(q-p));

	sstrncpy(&q, "KEY=", l-(q-p));
	sstrncpy(&q, keyword_pat, l-(q-p));
	fs_give((void **) &keyword_pat);
    }

    if(charset_pat){
	sstrncpy(&q, "/", l-(q-p));
	if(charset_not)
	  sstrncpy(&q, "!", l-(q-p));

	sstrncpy(&q, "CHAR=", l-(q-p));
	sstrncpy(&q, charset_pat, l-(q-p));
	fs_give((void **) &charset_pat);
    }

    if(arb_pat){
	sstrncpy(&q, arb_pat, l-(q-p));
	fs_give((void **)&arb_pat);
    }

    if(scorei_pat){
	sstrncpy(&q, "/SCOREI=", l-(q-p));
	sstrncpy(&q, scorei_pat, l-(q-p));
	fs_give((void **) &scorei_pat);
    }

    if(age_pat){
	sstrncpy(&q, "/AGE=", l-(q-p));
	sstrncpy(&q, age_pat, l-(q-p));
	fs_give((void **) &age_pat);
    }

    if(size_pat){
	sstrncpy(&q, "/SIZE=", l-(q-p));
	sstrncpy(&q, size_pat, l-(q-p));
	fs_give((void **) &size_pat);
    }

    if(category_cmd){
	sstrncpy(&q, "/CATCMD=", l-(q-p));
	sstrncpy(&q, category_cmd, l-(q-p));
	fs_give((void **) &category_cmd);
    }

    if(category_pat){
	sstrncpy(&q, "/CATVAL=", l-(q-p));
	sstrncpy(&q, category_pat, l-(q-p));
	fs_give((void **) &category_pat);
    }

    if(category_lim){
	sstrncpy(&q, "/CATLIM=", l-(q-p));
	sstrncpy(&q, category_lim, l-(q-p));
	fs_give((void **) &category_lim);
    }

    if(sentdate){
	sstrncpy(&q, sentdate, l-(q-p));
	fs_give((void **) &sentdate);
    }

    if(fldr_type_pat){
	sstrncpy(&q, "/FLDTYPE=", l-(q-p));
	sstrncpy(&q, fldr_type_pat, l-(q-p));
    }

    if(fldr_pat){
	sstrncpy(&q, "/FOLDER=", l-(q-p));
	sstrncpy(&q, fldr_pat, l-(q-p));
	fs_give((void **) &fldr_pat);
    }

    if(afrom_type_pat){
	sstrncpy(&q, "/AFROM=", l-(q-p));
	sstrncpy(&q, afrom_type_pat, l-(q-p));

	/*
	 * Add address types. If it is From or Reply-to
	 * leave this out so it will still work with pine.
	 */
	if((pat->patgrp->inabook & IAB_FROM
	    && pat->patgrp->inabook & IAB_REPLYTO
	    && !(pat->patgrp->inabook & IAB_SENDER)
	    && !(pat->patgrp->inabook & IAB_TO)
	    && !(pat->patgrp->inabook & IAB_CC))
	   ||
	   (!(pat->patgrp->inabook & IAB_FROM)
	    && !(pat->patgrp->inabook & IAB_REPLYTO)
	    && !(pat->patgrp->inabook & IAB_SENDER)
	    && !(pat->patgrp->inabook & IAB_TO)
	    && !(pat->patgrp->inabook & IAB_CC))){
	    ; /* leave it out */
	}
	else{
	    sstrncpy(&q, "/AFROMA=", l-(q-p));
	    if(pat->patgrp->inabook & IAB_FROM)
	      sstrncpy(&q, "F", l-(q-p));

	    if(pat->patgrp->inabook & IAB_REPLYTO)
	      sstrncpy(&q, "R", l-(q-p));

	    if(pat->patgrp->inabook & IAB_SENDER)
	      sstrncpy(&q, "S", l-(q-p));

	    if(pat->patgrp->inabook & IAB_TO)
	      sstrncpy(&q, "T", l-(q-p));

	    if(pat->patgrp->inabook & IAB_CC)
	      sstrncpy(&q, "C", l-(q-p));
	}
    }

    if(abooks_pat){
	sstrncpy(&q, "/ABOOKS=", l-(q-p));
	sstrncpy(&q, abooks_pat, l-(q-p));
	fs_give((void **) &abooks_pat);
    }

    if(stat_new_val){
	sstrncpy(&q, "/STATN=", l-(q-p));
	sstrncpy(&q, stat_new_val, l-(q-p));
    }

    if(stat_rec_val){
	sstrncpy(&q, "/STATR=", l-(q-p));
	sstrncpy(&q, stat_rec_val, l-(q-p));
    }

    if(stat_del_val){
	sstrncpy(&q, "/STATD=", l-(q-p));
	sstrncpy(&q, stat_del_val, l-(q-p));
    }

    if(stat_imp_val){
	sstrncpy(&q, "/STATI=", l-(q-p));
	sstrncpy(&q, stat_imp_val, l-(q-p));
    }

    if(stat_ans_val){
	sstrncpy(&q, "/STATA=", l-(q-p));
	sstrncpy(&q, stat_ans_val, l-(q-p));
    }

    if(stat_8bit_val){
	sstrncpy(&q, "/8BITS=", l-(q-p));
	sstrncpy(&q, stat_8bit_val, l-(q-p));
    }

    if(stat_bom_val){
	sstrncpy(&q, "/BOM=", l-(q-p));
	sstrncpy(&q, stat_bom_val, l-(q-p));
    }

    if(stat_boy_val){
	sstrncpy(&q, "/BOY=", l-(q-p));
	sstrncpy(&q, stat_boy_val, l-(q-p));
    }

    sstrncpy(&q, "\" action=\"", l-(q-p));

    if(inherit_nick && *inherit_nick){
	sstrncpy(&q, "/INICK=", l-(q-p));
	sstrncpy(&q, inherit_nick, l-(q-p));
	fs_give((void **)&inherit_nick);
    }

    if(action){
	if(action->is_a_role)
	  sstrncpy(&q, "/ROLE=1", l-(q-p));

	if(action->is_a_incol)
	  sstrncpy(&q, "/ISINCOL=1", l-(q-p));

	if(action->is_a_srch)
	  sstrncpy(&q, "/ISSRCH=1", l-(q-p));

	if(action->is_a_score)
	  sstrncpy(&q, "/ISSCORE=1", l-(q-p));

	if(action->is_a_filter){
	    /*
	     * Older pine will interpret a filter that has no folder
	     * as a Delete, even if we set it up here to be a Just Set
	     * State filter. Disable the filter for older versions in that
	     * case. If kill is set then Delete is what is supposed to
	     * happen, so that's ok. If folder is set then Move is what is
	     * supposed to happen, so ok.
	     */
	    if(!action->kill && !action->folder)
	      sstrncpy(&q, "/FILTER=2", l-(q-p));
	    else
	      sstrncpy(&q, "/FILTER=1", l-(q-p));
	}

	if(action->is_a_other)
	  sstrncpy(&q, "/OTHER=1", l-(q-p));
    }

    if(score_act){
	sstrncpy(&q, "/SCORE=", l-(q-p));
	sstrncpy(&q, score_act, l-(q-p));
	fs_give((void **)&score_act);
    }

    if(hdrtok_act){
	sstrncpy(&q, "/SCOREHDRTOK=", l-(q-p));
	sstrncpy(&q, hdrtok_act, l-(q-p));
	fs_give((void **)&hdrtok_act);
    }

    if(from_act){
	sstrncpy(&q, "/FROM=", l-(q-p));
	sstrncpy(&q, from_act, l-(q-p));
      fs_give((void **) &from_act);
    }

    if(replyto_act){
	sstrncpy(&q, "/REPL=", l-(q-p));
	sstrncpy(&q, replyto_act, l-(q-p));
	fs_give((void **)&replyto_act);
    }

    if(fcc_act){
	sstrncpy(&q, "/FCC=", l-(q-p));
	sstrncpy(&q, fcc_act, l-(q-p));
	fs_give((void **)&fcc_act);
    }

    if(litsig_act){
	sstrncpy(&q, "/LSIG=", l-(q-p));
	sstrncpy(&q, litsig_act, l-(q-p));
	fs_give((void **)&litsig_act);
    }

    if(sig_act){
	sstrncpy(&q, "/SIG=", l-(q-p));
	sstrncpy(&q, sig_act, l-(q-p));
	fs_give((void **)&sig_act);
    }

    if(templ_act){
	sstrncpy(&q, "/TEMPLATE=", l-(q-p));
	sstrncpy(&q, templ_act, l-(q-p));
	fs_give((void **)&templ_act);
    }

    if(cstm_act){
	sstrncpy(&q, "/CSTM=", l-(q-p));
	sstrncpy(&q, cstm_act, l-(q-p));
	fs_give((void **)&cstm_act);
    }

    if(smtp_act){
	sstrncpy(&q, "/SMTP=", l-(q-p));
	sstrncpy(&q, smtp_act, l-(q-p));
	fs_give((void **)&smtp_act);
    }

    if(nntp_act){
	sstrncpy(&q, "/NNTP=", l-(q-p));
	sstrncpy(&q, nntp_act, l-(q-p));
	fs_give((void **)&nntp_act);
    }

    if(repl_val){
	sstrncpy(&q, "/RTYPE=", l-(q-p));
	sstrncpy(&q, repl_val, l-(q-p));
    }

    if(forw_val){
	sstrncpy(&q, "/FTYPE=", l-(q-p));
	sstrncpy(&q, forw_val, l-(q-p));
    }

    if(comp_val){
	sstrncpy(&q, "/CTYPE=", l-(q-p));
	sstrncpy(&q, comp_val, l-(q-p));
    }

    if(incol_act){
	sstrncpy(&q, "/INCOL=", l-(q-p));
	sstrncpy(&q, incol_act, l-(q-p));
	fs_give((void **)&incol_act);
    }

    if(sort_act){
	sstrncpy(&q, "/SORT=", l-(q-p));
	sstrncpy(&q, sort_act, l-(q-p));
	fs_give((void **)&sort_act);
    }

    if(iform_act){
	sstrncpy(&q, "/IFORM=", l-(q-p));
	sstrncpy(&q, iform_act, l-(q-p));
	fs_give((void **)&iform_act);
    }

    if(start_act){
	sstrncpy(&q, "/START=", l-(q-p));
	sstrncpy(&q, start_act, l-(q-p));
    }

    if(folder_act){
	sstrncpy(&q, "/FOLDER=", l-(q-p));
	sstrncpy(&q, folder_act, l-(q-p));
	fs_give((void **) &folder_act);
    }

    if(filt_ifnotdel){
	sstrncpy(&q, filt_ifnotdel, l-(q-p));
	fs_give((void **) &filt_ifnotdel);
    }

    if(filt_nonterm){
	sstrncpy(&q, filt_nonterm, l-(q-p));
	fs_give((void **) &filt_nonterm);
    }

    if(filt_nokill){
	sstrncpy(&q, filt_nokill, l-(q-p));
	fs_give((void **) &filt_nokill);
    }

    if(filt_new_val){
	sstrncpy(&q, "/STATN=", l-(q-p));
	sstrncpy(&q, filt_new_val, l-(q-p));
    }

    if(filt_del_val){
	sstrncpy(&q, "/STATD=", l-(q-p));
	sstrncpy(&q, filt_del_val, l-(q-p));
    }

    if(filt_imp_val){
	sstrncpy(&q, "/STATI=", l-(q-p));
	sstrncpy(&q, filt_imp_val, l-(q-p));
    }

    if(filt_ans_val){
	sstrncpy(&q, "/STATA=", l-(q-p));
	sstrncpy(&q, filt_ans_val, l-(q-p));
    }

    if(keyword_set){
	sstrncpy(&q, "/KEYSET=", l-(q-p));
	sstrncpy(&q, keyword_set, l-(q-p));
	fs_give((void **) &keyword_set);
    }

    if(keyword_clr){
	sstrncpy(&q, "/KEYCLR=", l-(q-p));
	sstrncpy(&q, keyword_clr, l-(q-p));
	fs_give((void **) &keyword_clr);
    }

    if(q-p < l)
      *q++ = '\"';

    if(q-p < l)
      *q   = '\0';

    p[l-1] = '\0';

    return(p);
}


void
convert_statebits_to_vals(long int bits, int *dval, int *aval, int *ival, int *nval)
{
    if(dval)
      *dval = ACT_STAT_LEAVE;
    if(aval)
      *aval = ACT_STAT_LEAVE;
    if(ival)
      *ival = ACT_STAT_LEAVE;
    if(nval)
      *nval = ACT_STAT_LEAVE;

    if(ival){
	if(bits & F_FLAG)
	  *ival = ACT_STAT_SET;
	else if(bits & F_UNFLAG)
	  *ival = ACT_STAT_CLEAR;
    }

    if(aval){
	if(bits & F_ANS)
	  *aval = ACT_STAT_SET;
	else if(bits & F_UNANS)
	  *aval = ACT_STAT_CLEAR;
    }

    if(dval){
	if(bits & F_DEL)
	  *dval = ACT_STAT_SET;
	else if(bits & F_UNDEL)
	  *dval = ACT_STAT_CLEAR;
    }

    if(nval){
	if(bits & F_UNSEEN)
	  *nval = ACT_STAT_SET;
	else if(bits & F_SEEN)
	  *nval = ACT_STAT_CLEAR;
    }
}

    
/*
 * The "searched" bit will be set for each message which matches.
 *
 * Args:   patgrp -- Pattern to search with
 *         stream --
 *      searchset -- Restrict search to this set
 *        section -- Searching a section of the message, not the whole thing
 *      get_score -- Function to return the score for a message
 *          flags -- Most of these are flags to mail_search_full. However, we
 *                   overload the flags namespace and pass some flags of our
 *                   own in here that we pick off before calling mail_search.
 *                   Danger, danger, don't overlap with flag values defined
 *                   for c-client (that we want to use). Flags that we will
 *                   use here are:
 *                     MP_IN_CCLIENT_CB
 *                       If this is set we are in a callback from c-client
 *                       because some imap data arrived. We don't want to
 *                       call c-client again because it isn't re-entrant safe.
 *                       This is only a problem if we need to get the text of
 *                       a message to do the search, the envelope is cached
 *                       already.
 *                     MP_NOT
 *                       We want a ! of the patgrp in the search.
 *                   We also throw in SE_FREE for free, since we create
 *                   the search program here.
 *
 * Returns:   1 if any message in the searchset matches this pattern
 *            0 if no matches
 *           -1 if couldn't perform search because of no_fetch restriction
 */
int
match_pattern(PATGRP_S *patgrp, MAILSTREAM *stream, SEARCHSET *searchset,
	      char *section, long int (*get_score)(MAILSTREAM *, long int),
	      long int flags)
{
    SEARCHPGM    *pgm;
    SEARCHSET    *s;
    MESSAGECACHE *mc;
    long          i, msgno = 0L;
    int           in_client_callback = 0, not = 0;

    dprint((7, "match_pattern\n"));

    /*
     * Is the current folder the right type and possibly the right specific
     * folder for a match?
     */
    if(!(patgrp && !patgrp->bogus && match_pattern_folder(patgrp, stream)))
      return(0);

    /*
     * NULL searchset means that there is no message to compare against.
     * This is a match if the folder type matches above (that gets
     * us here), and there are no patterns to match against.
     *
     * It is not totally clear what should be done in the case of an empty
     * search set.  If there is search criteria, and someone does something
     * that is not specific to any messages (composing from scratch,
     * forwarding an attachment), then we can't be sure what a user would
     * expect.  The original way was to just use the role, which we'll
     * preserve here.
     */
    if(!searchset)
      return(1);

    /*
     * change by sderr : match_pattern_folder will sometimes
     * accept NULL streams, but if we are not in a folder-type-only
     * match test, we don't
     */
    if(!stream)
      return(0);

    if(flags & MP_IN_CCLIENT_CB){
	in_client_callback++;
	flags &= ~MP_IN_CCLIENT_CB;
    }

    if(flags & MP_NOT){
	not++;
	flags &= ~MP_NOT;
    }

    flags |= SE_FREE;

    if(patgrp->stat_bom != PAT_STAT_EITHER){
	if(patgrp->stat_bom == PAT_STAT_YES){
	    if(!ps_global->beginning_of_month){
		return(0);
	    }
	}
	else if(patgrp->stat_bom == PAT_STAT_NO){
	    if(ps_global->beginning_of_month){
		return(0);
	    }
	}
    }

    if(patgrp->stat_boy != PAT_STAT_EITHER){
	if(patgrp->stat_boy == PAT_STAT_YES){
	    if(!ps_global->beginning_of_year){
		return(0);
	    }
	}
	else if(patgrp->stat_boy == PAT_STAT_NO){
	    if(ps_global->beginning_of_year){
		return(0);
	    }
	}
    }

    if(in_client_callback && is_imap_stream(stream)
       && (patgrp->alltext || patgrp->bodytext))
      return(-1);

    pgm = match_pattern_srchpgm(patgrp, stream, searchset);
    if(not && !(is_imap_stream(stream) && !modern_imap_stream(stream))){
	SEARCHPGM *srchpgm;

	srchpgm = pgm;
	pgm = mail_newsearchpgm();
	pgm->not = mail_newsearchpgmlist();
	pgm->not->pgm = srchpgm;
    }

    if((patgrp->alltext || patgrp->bodytext)
       && (!is_imap_stream(stream) || modern_imap_stream(stream)))
      /*
       * Cache isn't going to work. Search on server.
       * Except that is likely to not work on an old imap server because
       * the OR criteria won't work and we are likely to have some ORs.
       * So turn off the NOSERVER flag (and search on server if remote)
       * unless the server is an old server. It doesn't matter if we
       * turn if off if it's not an imap stream, but we do it anyway.
       */
      flags &= ~SE_NOSERVER;

    if(section){
	/*
	 * Mail_search_full only searches the top-level msg. We want to
	 * search an attached msg instead. First do the stuff
	 * that mail_search_full would have done before calling
	 * mail_search_msg, then call mail_search_msg with a section number.
	 * Mail_search_msg does take a section number even though
	 * mail_search_full doesn't.
	 */

	/*
	 * We'll only ever set section if the searchset is a single message.
	 */
	if(pgm->msgno->next == NULL && pgm->msgno->first == pgm->msgno->last)
	  msgno = pgm->msgno->first;

	for(i = 1L; i <= stream->nmsgs; i++)
	  if((mc = mail_elt(stream, i)) != NULL)
	    mc->searched = NIL;

	if(mail_search_msg(stream,msgno,section,pgm)
	   && msgno > 0L && msgno <= stream->nmsgs
	   && (mc = mail_elt(stream, msgno)))
	  mc->searched = T;

	if(flags & SE_FREE)
	  mail_free_searchpgm(&pgm);
    }
    else{
	/* 
	 * Here we could be checking on the return value to see if
	 * the search was "successful" or not.  It may be the case
	 * that we'd want to stop trying filtering if we got some
	 * sort of error, but for now we would just continue on
	 * to the next filter.
	 */
	pine_mail_search_full(stream, "UTF-8", pgm, flags);
    }

    /* we searched without the not, reverse it */
    if(not && is_imap_stream(stream) && !modern_imap_stream(stream)){
	for(msgno = 1L; msgno < mn_get_total(sp_msgmap(stream)); msgno++)
	  if(stream && msgno && msgno <= stream->nmsgs
	     && (mc=mail_elt(stream,msgno)) && mc->searched)
	    mc->searched = NIL;
	  else
	    mc->searched = T;
    }

    /* check scores */
    if(get_score && scores_are_used(SCOREUSE_GET) && patgrp->do_score){
	char      *savebits;
	SEARCHSET *ss;

	/*
	 * Get_score may call build_header_line recursively (we may
	 * be in build_header_line now) so we have to preserve and
	 * restore the sequence bits.
	 */
	savebits = (char *)fs_get((stream->nmsgs+1) * sizeof(char));

	for(i = 1L; i <= stream->nmsgs; i++){
	    if((mc = mail_elt(stream, i)) != NULL){
		savebits[i] = mc->sequence;
		mc->sequence = 0;
	    }
	}

	/*
	 * Build a searchset which will get all the scores that we
	 * need but not more.
	 */
	for(s = searchset; s; s = s->next)
	  for(msgno = s->first; msgno <= s->last; msgno++)
	    if(msgno > 0L && msgno <= stream->nmsgs
	       && (mc = mail_elt(stream, msgno)) && mc->searched
	       && get_msg_score(stream, msgno) == SCORE_UNDEF)
	      mc->sequence = 1;
	
	if((ss = build_searchset(stream)) != NULL){
	    (void)calculate_some_scores(stream, ss, in_client_callback);
	    mail_free_searchset(&ss);
	}

	/*
	 * Now check the scores versus the score intervals to see if
	 * any of the messages which have matched up to this point can
	 * be tossed because they don't match the score interval.
	 */
	for(s = searchset; s; s = s->next)
	  for(msgno = s->first; msgno <= s->last; msgno++)
	    if(msgno > 0L && msgno <= stream->nmsgs
	       && (mc = mail_elt(stream, msgno)) && mc->searched){
		long score;

		score = (*get_score)(stream, msgno);

		/*
		 * If the score is outside all of the intervals,
		 * turn off the searched bit.
		 * So that means we check each interval and if
		 * it is inside any interval we stop and leave
		 * the bit set. If it is outside we keep checking.
		 */
		if(score != SCORE_UNDEF){
		    INTVL_S *iv;

		    for(iv = patgrp->score; iv; iv = iv->next)
		      if(score >= iv->imin && score <= iv->imax)
			break;
		    
		    if(!iv)
		      mc->searched = NIL;
		}
	    }

	for(i = 1L; i <= stream->nmsgs; i++)
	  if((mc = mail_elt(stream, i)) != NULL)
	    mc->sequence = savebits[i];
    
	fs_give((void **)&savebits);
    }

    /* if there are still matches, check for 8bit subject match */
    if(patgrp->stat_8bitsubj != PAT_STAT_EITHER)
      find_8bitsubj_in_messages(stream, searchset, patgrp->stat_8bitsubj, 1);

    /* if there are still matches, check for charset matches */
    if(patgrp->charsets)
      find_charsets_in_messages(stream, searchset, patgrp, 1);

    /* Still matches, check addrbook */
    if(patgrp->inabook != IAB_EITHER)
      address_in_abook(stream, searchset, patgrp->inabook, patgrp->abooks);

    /* Still matches? Run the categorization command on each msg. */
    if(pith_opt_filter_pattern_cmd)
      (*pith_opt_filter_pattern_cmd)(patgrp->category_cmd, searchset, stream, patgrp->cat_lim, patgrp->cat);

    for(s = searchset; s; s = s->next)
      for(msgno = s->first; msgno > 0L && msgno <= s->last; msgno++)
        if(msgno > 0L && msgno <= stream->nmsgs
	   && (mc = mail_elt(stream, msgno)) && mc->searched)
	  return(1);

    return(0);
}


/*
 * Look through messages in searchset to see if they contain 8bit
 * characters in their subjects. All of the messages in
 * searchset should initially have the searched bit set. Turn off the
 * searched bit where appropriate.
 */
void
find_8bitsubj_in_messages(MAILSTREAM *stream, SEARCHSET *searchset,
			  int stat_8bitsubj, int saveseqbits)
{
    char         *savebits = NULL;
    SEARCHSET    *s, *ss = NULL;
    MESSAGECACHE *mc;
    long          count = 0L;
    unsigned long msgno;

    /*
     * If we are being called while in build_header_line we may
     * call build_header_line recursively. So save and restore the
     * sequence bits.
     */
    if(saveseqbits)
      savebits = (char *) fs_get((stream->nmsgs+1) * sizeof(char));

    for(msgno = 1L; msgno <= stream->nmsgs; msgno++){
	if((mc = mail_elt(stream, msgno)) != NULL){
	    if(savebits)
	      savebits[msgno] = mc->sequence;

	    mc->sequence = 0;
	}
    }

    /*
     * Build a searchset so we can look at all the envelopes
     * we need to look at but only those we need to look at.
     * Everything with the searched bit set is still a
     * possibility, so restrict to that set.
     */

    for(s = searchset; s; s = s->next)
      for(msgno = s->first; msgno <= s->last; msgno++)
	if(msgno > 0L && msgno <= stream->nmsgs
	   && (mc = mail_elt(stream, msgno)) && mc->searched){
	    mc->sequence = 1;
	    count++;
	}

    ss = build_searchset(stream);

    if(count){
	SEARCHSET **sset;

	mail_parameters(NULL, SET_FETCHLOOKAHEADLIMIT, (void *) count);

	/*
	 * This causes the lookahead to fetch precisely
	 * the messages we want (in the searchset) instead
	 * of just fetching the next 20 sequential
	 * messages. If the searching so far has caused
	 * a sparse searchset in a large mailbox, the
	 * difference can be substantial.
	 * This resets automatically after the first fetch.
	 */
	sset = (SEARCHSET **) mail_parameters(stream,
					      GET_FETCHLOOKAHEAD,
					      (void *) stream);
	if(sset)
	  *sset = ss;
    }

    for(s = ss; s; s = s->next){
	for(msgno = s->first; msgno <= s->last; msgno++){
	    ENVELOPE   *e;

	    if(!stream || msgno <= 0L || msgno > stream->nmsgs)
	      continue;

	    e = pine_mail_fetchenvelope(stream, msgno);
	    if(stat_8bitsubj == PAT_STAT_YES){
		if(e && e->subject){
		    char *p;

		    for(p = e->subject; *p; p++)
		      if(*p & 0x80)
			break;

		    if(!*p && msgno > 0L && msgno <= stream->nmsgs
		       && (mc = mail_elt(stream, msgno)))
		      mc->searched = NIL;
		}
		else if(msgno > 0L && msgno <= stream->nmsgs
			&& (mc = mail_elt(stream, msgno)))
		  mc->searched = NIL;
	    }
	    else if(stat_8bitsubj == PAT_STAT_NO){
		if(e && e->subject){
		    char *p;

		    for(p = e->subject; *p; p++)
		      if(*p & 0x80)
			break;

		    if(*p && msgno > 0L && msgno <= stream->nmsgs
		       && (mc = mail_elt(stream, msgno)))
		      mc->searched = NIL;
		}
	    }
	}
    }

    if(savebits){
	for(msgno = 1L; msgno <= stream->nmsgs; msgno++)
	  if((mc = mail_elt(stream, msgno)) != NULL)
	    mc->sequence = savebits[msgno];

	fs_give((void **) &savebits);
    }

    if(ss)
      mail_free_searchset(&ss);
}


/*
 * Look through messages in searchset to see if they contain any of the
 * charsets or scripts listed in charsets pattern. All of the messages in
 * searchset should initially have the searched bit set. Turn off the
 * searched bit where appropriate.
 */
void
find_charsets_in_messages(MAILSTREAM *stream, SEARCHSET *searchset,
			  PATGRP_S *patgrp, int saveseqbits)
{
    char         *savebits = NULL;
    unsigned long msgno;
    long          count = 0L;
    MESSAGECACHE *mc;
    SEARCHSET    *s, *ss;

    if(!stream || !patgrp)
      return;

    /*
     * When we actually want to use charsets, we convert it into a list
     * of charsets instead of the mixed list of scripts and charsets and
     * we eliminate duplicates. This is more efficient when we actually
     * do the lookups and compares.
     */
    if(!patgrp->charsets_list){
	PATTERN_S    *cs;
	const CHARSET *cset;
	STRLIST_S    *sl = NULL, *newsl;
	unsigned long scripts = 0L;
	SCRIPT       *script;

	for(cs = patgrp->charsets; cs; cs = cs->next){
	    /*
	     * Run through the charsets pattern looking for
	     * scripts and set the corresponding script bits.
	     * If it isn't a script, it is a character set.
	     */
	    if(cs->substring && (script = utf8_script(cs->substring)))
	      scripts |= script->script;
	    else{
		/* add it to list as a specific character set */
		newsl = new_strlist(cs->substring);
		if(compare_strlists_for_match(sl, newsl))  /* already in list */
		  free_strlist(&newsl);
		else{
		    newsl->next = sl;
		    sl = newsl;
		}
	    }
	}

	/*
	 * Now scripts has a bit set for each script the user
	 * specified in the charsets pattern. Go through all of
	 * the known charsets and include ones in these scripts.
	 */
	if(scripts){
	    for(cset = utf8_charset(NIL); cset && cset->name; cset++){
		if(cset->script & scripts){

		    /* filter this out of each script, not very useful */
		    if(!strucmp("ISO-2022-JP-2", cset->name)
		       || !strucmp("UTF-7", cset->name)
		       || !strucmp("UTF-8", cset->name))
		      continue;

		    /* add cset->name to the list */
		    newsl = new_strlist(cset->name);
		    if(compare_strlists_for_match(sl, newsl))
		      free_strlist(&newsl);
		    else{
			newsl->next = sl;
			sl = newsl;
		    }
		}
	    }
	}

	patgrp->charsets_list = sl;
    }

    /*
     * This may call build_header_line recursively because we may be in
     * build_header_line now. So we have to preserve and restore the
     * sequence bits since we want to use them here.
     */
    if(saveseqbits)
      savebits = (char *) fs_get((stream->nmsgs+1) * sizeof(char));

    for(msgno = 1L; msgno <= stream->nmsgs; msgno++){
	if((mc = mail_elt(stream, msgno)) != NULL){
	    if(savebits)
	      savebits[msgno] = mc->sequence;

	    mc->sequence = 0;
	}
    }


    /*
     * Build a searchset so we can look at all the bodies
     * we need to look at but only those we need to look at.
     * Everything with the searched bit set is still a
     * possibility, so restrict to that set.
     */

    for(s = searchset; s; s = s->next)
      for(msgno = s->first; msgno <= s->last; msgno++)
	if(msgno > 0L && msgno <= stream->nmsgs
	   && (mc = mail_elt(stream, msgno)) && mc->searched){
	    mc->sequence = 1;
	    count++;
        }

    ss = build_searchset(stream);

    if(count){
	SEARCHSET **sset;

	mail_parameters(NULL, SET_FETCHLOOKAHEADLIMIT, (void *) count);

	/*
	 * This causes the lookahead to fetch precisely
	 * the messages we want (in the searchset) instead
	 * of just fetching the next 20 sequential
	 * messages. If the searching so far has caused
	 * a sparse searchset in a large mailbox, the
	 * difference can be substantial.
	 * This resets automatically after the first fetch.
	 */
	sset = (SEARCHSET **) mail_parameters(stream,
					      GET_FETCHLOOKAHEAD,
					      (void *) stream);
	if(sset)
	  *sset = ss;
    }

    for(s = ss; s; s = s->next){
	for(msgno = s->first; msgno <= s->last; msgno++){

	    if(msgno <= 0L || msgno > stream->nmsgs)
	      continue;

	    if(patgrp->charsets_list
	       && charsets_present_in_msg(stream,msgno,patgrp->charsets_list)){
		if(patgrp->charsets->not){
		    if((mc = mail_elt(stream, msgno)))
		      mc->searched = NIL;
		}
		/* else leave it */
	    }
	    else{		/* charset isn't in message */
		if(!patgrp->charsets->not){
		    if((mc = mail_elt(stream, msgno)))
		      mc->searched = NIL;
		}
		/* else leave it */
	    }
	}
    }

    if(savebits){
	for(msgno = 1L; msgno <= stream->nmsgs; msgno++)
	  if((mc = mail_elt(stream, msgno)) != NULL)
	    mc->sequence = savebits[msgno];

	fs_give((void **) &savebits);
    }

    if(ss)
      mail_free_searchset(&ss);
}


/*
 * Look for any of the charsets in this particular message.
 *
 * Returns 1 if there is a match, 0 otherwise.
 */
int
charsets_present_in_msg(MAILSTREAM *stream, long unsigned int rawmsgno, STRLIST_S *charsets)
{
    BODY       *body = NULL;
    ENVELOPE   *env = NULL;
    STRLIST_S  *msg_charsets = NULL;
    int         ret = 0;

    if(charsets && stream && rawmsgno > 0L && rawmsgno <= stream->nmsgs){
	env = pine_mail_fetchstructure(stream, rawmsgno, &body);
	collect_charsets_from_subj(env, &msg_charsets);
	collect_charsets_from_body(body, &msg_charsets);
	if(msg_charsets){
	    ret = compare_strlists_for_match(msg_charsets, charsets);
	    free_strlist(&msg_charsets);
	}
    }

    return(ret);
}


void
collect_charsets_from_subj(ENVELOPE *env, STRLIST_S **listptr)
{
    STRLIST_S *newsl;
    char      *text, *e;

    if(listptr && env && env->subject){
	/* find encoded word */
	for(text = env->subject; *text; text++){
	    if((*text == '=') && (text[1] == '?') && isalpha(text[2]) &&
	       (e = strchr(text+2,'?'))){
		*e = '\0';			/* tie off charset name */

		newsl = new_strlist(text+2);
		*e = '?';

		if(compare_strlists_for_match(*listptr, newsl))
		  free_strlist(&newsl);
		else{
		    newsl->next = *listptr;
		    *listptr = newsl;
		}
	    }
	}
    }
}


/*
 * Check for any of the charsets in any of the charset params in
 * any of the text parts of the body of a message. Put them in the list
 * pointed to by listptr.
 */
void
collect_charsets_from_body(struct mail_bodystruct *body, STRLIST_S **listptr)
{
    PART      *part;
    char      *cset;

    if(listptr && body){
	switch(body->type){
          case TYPEMULTIPART:
	    for(part = body->nested.part; part; part = part->next)
	      collect_charsets_from_body(&part->body, listptr);

	    break;

          case TYPEMESSAGE:
	    if(!strucmp(body->subtype, "RFC822")){
	        collect_charsets_from_subj(body->nested.msg->env, listptr);
		collect_charsets_from_body(body->nested.msg->body, listptr);
		break;
	    }
	    /* else fall through to text case */

	  case TYPETEXT:
	    cset = parameter_val(body->parameter, "charset");
	    if(cset){
		STRLIST_S *newsl;

		newsl = new_strlist(cset);

		if(compare_strlists_for_match(*listptr, newsl))
		  free_strlist(&newsl);
		else{
		    newsl->next = *listptr;
		    *listptr = newsl;
		}

	        fs_give((void **) &cset);
	    }

	    break;

	  default:			/* non-text terminal mode */
	    break;
	}
    }
}


/*
 * If any of the names in list1 is the same as any of the names in list2
 * then return 1, else return 0. Comparison is case independent.
 */
int
compare_strlists_for_match(STRLIST_S *list1, STRLIST_S *list2)
{
    int        ret = 0;
    STRLIST_S *cs1, *cs2;

    for(cs1 = list1; !ret && cs1; cs1 = cs1->next)
      for(cs2 = list2; !ret && cs2; cs2 = cs2->next)
        if(cs1->name && cs2->name && !strucmp(cs1->name, cs2->name))
	  ret = 1;

    return(ret);
}


int
match_pattern_folder(PATGRP_S *patgrp, MAILSTREAM *stream)
{
    int	       is_news;
    
    /* change by sderr : we match FLDR_ANY even if stream is NULL */
    return((patgrp->fldr_type == FLDR_ANY)
	   || (stream
	       && (((is_news = IS_NEWS(stream))
	            && patgrp->fldr_type == FLDR_NEWS)
	           || (!is_news && patgrp->fldr_type == FLDR_EMAIL)
	           || (patgrp->fldr_type == FLDR_SPECIFIC
		       && match_pattern_folder_specific(patgrp->folder,
						       stream, FOR_PATTERN)))));
}


/* 
 * Returns positive if this stream is open on one of the folders in the
 * folders argument, 0 otherwise.
 *
 * If FOR_PATTERN is set, this interprets simple names as nicknames in
 * the incoming collection, otherwise it treats simple names as being in
 * the primary collection.
 * If FOR_FILT is set, the folder names are detokenized before being used.
 */
int
match_pattern_folder_specific(PATTERN_S *folders, MAILSTREAM *stream, int flags)
{
    PATTERN_S *p;
    int        match = 0;
    char      *patfolder, *free_this = NULL;

    dprint((8, "match_pattern_folder_specific\n"));

    if(!(stream && stream->mailbox && stream->mailbox[0]))
      return(0);

    /*
     * For each of the folders in the pattern, see if we get
     * a match. We're just looking for any match. If none match,
     * we return 0, otherwise we fall through and check the rest
     * of the pattern. The fact that the string is called "substring"
     * is not meaningful. We're just using the convenient pattern
     * structure to store a list of folder names. They aren't
     * substrings of names, they are the whole name.
     */
    for(p = folders; !match && p; p = p->next){
	free_this = NULL;
	if(flags & FOR_FILTER)
	  patfolder = free_this = detoken_src(p->substring, FOR_FILT, NULL,
					      NULL, NULL, NULL);
	else
	  patfolder = p->substring;

	if(patfolder
	   && (!strucmp(patfolder, ps_global->inbox_name)
	       || !strcmp(patfolder, ps_global->VAR_INBOX_PATH))){
	    if(sp_flagged(stream, SP_INBOX))
	      match++;
	}
	else{
	    char      *fname;
	    char      *t, *streamfolder;
	    char       tmp1[MAILTMPLEN], tmp2[MAX(MAILTMPLEN,NETMAXMBX)];
	    CONTEXT_S *cntxt = NULL;

	    if(flags & FOR_PATTERN){
		/*
		 * See if patfolder is a nickname in the incoming collection.
		 * If so, use its real name instead.
		 */
		if(patfolder[0] &&
		   (ps_global->context_list->use & CNTXT_INCMNG) &&
		   (fname = (folder_is_nick(patfolder,
					    FOLDERS(ps_global->context_list),
					    0))))
		  patfolder = fname;
	    }
	    else{
		char *save_ref = NULL;

		/*
		 * If it's an absolute pathname, we treat is as a local file
		 * instead of interpreting it in the primary context.
		 */
		if(!is_absolute_path(patfolder)
		   && !(cntxt = default_save_context(ps_global->context_list)))
		  cntxt = ps_global->context_list;
		
		/*
		 * Because this check is independent of where the user is
		 * in the folder hierarchy and has nothing to do with that,
		 * we want to ignore the reference field built into the
		 * context. Zero it out temporarily here.
		 */
		if(cntxt && cntxt->dir){
		    save_ref = cntxt->dir->ref;
		    cntxt->dir->ref = NULL;
		}

		patfolder = context_apply(tmp1, cntxt, patfolder, sizeof(tmp1));
		if(save_ref)
		  cntxt->dir->ref = save_ref;
	    }

	    switch(patfolder[0]){
	      case '{':
		if(stream->mailbox[0] == '{' &&
		   same_stream(patfolder, stream) &&
		   (streamfolder = strindex(&stream->mailbox[1], '}')) &&
		   (t = strindex(&patfolder[1], '}')) &&
		   (!strcmp(t+1, streamfolder+1) ||
		    (*(t+1) == '\0' && !strcmp("INBOX", streamfolder+1))))
		  match++;

		break;
	      
	      case '#':
	        if(!strcmp(patfolder, stream->mailbox))
		  match++;

		break;

	      default:
		t = (strlen(patfolder) < (MAILTMPLEN/2))
				? mailboxfile(tmp2, patfolder) : NULL;
		if(t && *t && !strcmp(t, stream->mailbox))
		  match++;

		break;
	    }
	}

	if(free_this)
	  fs_give((void **) &free_this);
    }

    return(match);
}


/*
 * generate a search program corresponding to the provided patgrp
 */
SEARCHPGM *
match_pattern_srchpgm(PATGRP_S *patgrp, MAILSTREAM *stream, SEARCHSET *searchset)
{
    SEARCHPGM	 *pgm, *tmppgm;
    SEARCHOR     *or;
    SEARCHSET	**sp;

    pgm = mail_newsearchpgm();

    sp = &pgm->msgno;
    /* copy the searchset */
    while(searchset){
	SEARCHSET *s;

	s = mail_newsearchset();
	s->first = searchset->first;
	s->last  = searchset->last;
	searchset = searchset->next;
	*sp = s;
	sp = &s->next;
    }

    if(!patgrp)
      return(pgm);

    if(patgrp->subj){
	if(patgrp->subj->not)
	  tmppgm = next_not(pgm);
	else
	  tmppgm = pgm;

	set_up_search_pgm("subject", patgrp->subj, tmppgm);
    }

    if(patgrp->cc){
	if(patgrp->cc->not)
	  tmppgm = next_not(pgm);
	else
	  tmppgm = pgm;

	set_up_search_pgm("cc", patgrp->cc, tmppgm);
    }

    if(patgrp->from){
	if(patgrp->from->not)
	  tmppgm = next_not(pgm);
	else
	  tmppgm = pgm;

	set_up_search_pgm("from", patgrp->from, tmppgm);
    }

    if(patgrp->to){
	if(patgrp->to->not)
	  tmppgm = next_not(pgm);
	else
	  tmppgm = pgm;

	set_up_search_pgm("to", patgrp->to, tmppgm);
    }

    if(patgrp->sender){
	if(patgrp->sender->not)
	  tmppgm = next_not(pgm);
	else
	  tmppgm = pgm;

	set_up_search_pgm("sender", patgrp->sender, tmppgm);
    }

    if(patgrp->news){
	if(patgrp->news->not)
	  tmppgm = next_not(pgm);
	else
	  tmppgm = pgm;

	set_up_search_pgm("newsgroups", patgrp->news, tmppgm);
    }

    /* To OR Cc */
    if(patgrp->recip){
	if(patgrp->recip->not)
	  tmppgm = next_not(pgm);
	else
	  tmppgm = pgm;
	
	or = next_or(&tmppgm->or);

	set_up_search_pgm("to", patgrp->recip, or->first);
	set_up_search_pgm("cc", patgrp->recip, or->second);
    }

    /* To OR Cc OR From */
    if(patgrp->partic){
	if(patgrp->partic->not)
	  tmppgm = next_not(pgm);
	else
	  tmppgm = pgm;

	or = next_or(&tmppgm->or);

	set_up_search_pgm("to", patgrp->partic, or->first);

	or->second->or = mail_newsearchor();
	set_up_search_pgm("cc", patgrp->partic, or->second->or->first);
	set_up_search_pgm("from", patgrp->partic, or->second->or->second);
    }

    if(patgrp->arbhdr){
	ARBHDR_S *a;

	for(a = patgrp->arbhdr; a; a = a->next)
	  if(a->field && a->field[0] && a->p){
	      if(a->p->not)
	        tmppgm = next_not(pgm);
	      else
	        tmppgm = pgm;

	      set_up_search_pgm(a->field, a->p, tmppgm);
	  }
    }

    if(patgrp->alltext){
	if(patgrp->alltext->not)
	  tmppgm = next_not(pgm);
	else
	  tmppgm = pgm;

	set_up_search_pgm("alltext", patgrp->alltext, tmppgm);
    }
    
    if(patgrp->bodytext){
	if(patgrp->bodytext->not)
	  tmppgm = next_not(pgm);
	else
	  tmppgm = pgm;

	set_up_search_pgm("bodytext", patgrp->bodytext, tmppgm);
    }

    if(patgrp->keyword){
	PATTERN_S *p_old, *p_new, *new_pattern = NULL, **nextp;
	char      *q;

	if(patgrp->keyword->not)
	  tmppgm = next_not(pgm);
	else
	  tmppgm = pgm;

	/*
	 * The keyword entries may be nicknames instead of the actual
	 * keywords, so those need to be converted to actual keywords.
	 *
	 * If we search for keywords that are not defined for a folder
	 * we may get error messages back that we don't want instead of
	 * just no match. We will build a replacement pattern here which
	 * contains only the defined subset of the keywords.
	 */
	
	nextp = &new_pattern;

	for(p_old = patgrp->keyword; p_old; p_old = p_old->next){
	    q = nick_to_keyword(p_old->substring);
	    if(user_flag_index(stream, q) >= 0){
		p_new = (PATTERN_S *) fs_get(sizeof(*p_new));
		memset(p_new, 0, sizeof(*p_new));
		p_new->substring = cpystr(q);
		*nextp = p_new;
		nextp = &p_new->next;
	    }
	}

	/*
	 * If there are some matching keywords that are defined in
	 * the folder, then we are ok because we will match only if
	 * we match one of those. However, if the list is empty, then
	 * we can't just leave this part of the search program empty.
	 * That would result in a match instead of not a match.
	 * We can fake our way around the problem with NOT. If the
	 * list is empty we want the opposite, so we insert a NOT in
	 * front of an empty program. We may end up with NOT NOT if
	 * this was already NOT'd, but that's ok, too. Alternatively,
	 * we could undo the first NOT instead.
	 */

	if(new_pattern){
	    set_up_search_pgm("keyword", new_pattern, tmppgm);
	    free_pattern(&new_pattern);
	}
	else
	  (void) next_not(tmppgm);	/* add NOT of something that matches,
					   so the NOT thing doesn't match   */
    }

    if(patgrp->do_age && patgrp->age){
	INTVL_S  *iv;
	SEARCHOR *or;

	tmppgm = pgm;

	for(iv = patgrp->age; iv; iv = iv->next){
	    if(iv->next){
		or = next_or(&tmppgm->or);
		set_search_by_age(iv, or->first, patgrp->age_uses_sentdate);
		tmppgm = or->second;
	    }
	    else
	      set_search_by_age(iv, tmppgm, patgrp->age_uses_sentdate);
	}
    }
    
    if(patgrp->do_size && patgrp->size){
	INTVL_S  *iv;
	SEARCHOR *or;

	tmppgm = pgm;

	for(iv = patgrp->size; iv; iv = iv->next){
	    if(iv->next){
		or = next_or(&tmppgm->or);
		set_search_by_size(iv, or->first);
		tmppgm = or->second;
	    }
	    else
	      set_search_by_size(iv, tmppgm);
	}
    }
    
    SETPGMSTATUS(patgrp->stat_new,pgm->unseen,pgm->seen);
    SETPGMSTATUS(patgrp->stat_rec,pgm->recent,pgm->old);
    SETPGMSTATUS(patgrp->stat_del,pgm->deleted,pgm->undeleted);
    SETPGMSTATUS(patgrp->stat_imp,pgm->flagged,pgm->unflagged);
    SETPGMSTATUS(patgrp->stat_ans,pgm->answered,pgm->unanswered);

    return(pgm);
}


SEARCHPGM *
next_not(SEARCHPGM *pgm)
{
    SEARCHPGMLIST *not, **not_ptr;

    if(!pgm)
      return(NULL);

    /* find next unused not slot */
    for(not = pgm->not; not && not->next; not = not->next)
      ;
    
    if(not)
      not_ptr = &not->next;
    else
      not_ptr = &pgm->not;
    
    /* allocate */
    *not_ptr = mail_newsearchpgmlist();

    return((*not_ptr)->pgm);
}


SEARCHOR *
next_or(struct search_or **startingor)
{
    SEARCHOR *or, **or_ptr;

    /* find next unused or slot */
    for(or = (*startingor); or && or->next; or = or->next)
      ;
    
    if(or)
      or_ptr = &or->next;
    else
      or_ptr = startingor;
    
    /* allocate */
    *or_ptr = mail_newsearchor();

    return(*or_ptr);
}


void
set_up_search_pgm(char *field, PATTERN_S *pattern, SEARCHPGM *pgm)
{
    SEARCHOR *or;

    if(field && pattern && pgm){

	/*
	 * To is special because we want to use the ReSent-To header instead
	 * of the To header if it exists.  We set up something like:
	 *
	 * if((resent-to matches pat1 or pat2...)
	 *                  OR
	 *    (<resent-to doesn't exist> AND (to matches pat1 or pat2...)))
	 *
	 *  Some servers (Exchange, apparently) seem to have trouble with
	 *  the search for the empty string to decide if the header exists
	 *  or not. So, we will search for either the empty string OR the
	 *  header with a SPACE in it. Some still have trouble with this
	 *  so we are changing it to be off by default.
	 */
	if(!strucmp(field, "to") && F_ON(F_USE_RESENTTO, ps_global)){
	    or = next_or(&pgm->or);

	    add_type_to_pgm("resent-to", pattern, or->first);

	    /* check for resent-to doesn't exist */
	    or->second->not = mail_newsearchpgmlist();

	    or->second->not->pgm->or = mail_newsearchor();
	    set_srch("resent-to", " ", or->second->not->pgm->or->first);
	    set_srch("resent-to",  "", or->second->not->pgm->or->second);

	    /* now add the real To search to second */
	    add_type_to_pgm(field, pattern, or->second);
	}
	else
	  add_type_to_pgm(field, pattern, pgm);
    }
}


void
add_type_to_pgm(char *field, PATTERN_S *pattern, SEARCHPGM *pgm)
{
    PATTERN_S *p;
    SEARCHOR  *or;
    SEARCHPGM *notpgm, *tpgm;
    int        cnt = 0;

    if(field && pattern && pgm){
	/*
	 * Here is a weird bit of logic. What we want here is simply
	 *       A or B or C or D
	 * for all of the elements of pattern. Ors are a bit complicated.
	 * The list of ORs in the SEARCHPGM structure are ANDed together,
	 * not ORd together. It's for things like
	 *      Subject A or B  AND  From C or D
	 * The Subject part would be one member of the OR list and the From
	 * part would be another member of the OR list. Instead we want
	 * a big OR which may have more than two members (first and second)
	 * but the structure just has two members. So we have to build an
	 * OR tree and we build it by going down one branch of the tree
	 * instead of by balancing the branches.
	 *
	 *            or
	 *           /  \
	 *    first==A   second
	 *                /  \
	 *          first==B  second
	 *                     /  \
	 *               first==C  second==D
	 *
	 * There is an additional problem. Some servers don't like deeply
	 * nested logic in the SEARCH command. The tree above produces a
	 * fairly deeply nested command if the user wants to match on
	 * several different From addresses or Subjects...
	 * We use the tried and true equation
	 *
	 *        (A or B) == !(!A and !B)
	 *
	 * to change the deeply nested OR tree into ANDs which aren't nested.
	 * Right now we're only doing that if the nesting is fairly deep.
	 * We can think of some reasons to do that. First, we know that the
	 * OR thing works, that's what we've been using for a while and the
	 * only problem is the deep nesting. 2nd, it is easier to understand.
	 * 3rd, it looks dumb to use  NOT NOT A   instead of  A.
	 * It is probably dumb to mix the two, but what the heck.
	 * Hubert 2003-04-02
	 */
	for(p = pattern; p; p = p->next)
	  cnt++;

	if(cnt < 10){				/* use ORs if count is low */
	    for(p = pattern; p; p = p->next){
		if(p->next){
		    or = next_or(&pgm->or);

		    set_srch(field, p->substring ? p->substring : "", or->first);
		    pgm = or->second;
		}
		else
		  set_srch(field, p->substring ? p->substring : "", pgm);
	    }
	}
	else{					/* else use ANDs */
	    /* ( A or B or C )  <=>  ! ( !A and !B and !C ) */

	    /* first, NOT of the whole thing */
	    notpgm = next_not(pgm);

	    /* then the not list is ANDed together */
	    for(p = pattern; p; p = p->next){
		tpgm = next_not(notpgm);
		set_srch(field, p->substring ? p->substring : "", tpgm);
	    }
	}
    }
}


void
set_srch(char *field, char *value, SEARCHPGM *pgm)
{
    char        *decoded;
    STRINGLIST **list;

    if(!(field && value && pgm))
      return;

    if(!strucmp(field, "subject"))
      list = &pgm->subject;
    else if(!strucmp(field, "from"))
      list = &pgm->from;
    else if(!strucmp(field, "to"))
      list = &pgm->to;
    else if(!strucmp(field, "cc"))
      list = &pgm->cc;
    else if(!strucmp(field, "sender"))
      list = &pgm->sender;
    else if(!strucmp(field, "reply-to"))
      list = &pgm->reply_to;
    else if(!strucmp(field, "in-reply-to"))
      list = &pgm->in_reply_to;
    else if(!strucmp(field, "message-id"))
      list = &pgm->message_id;
    else if(!strucmp(field, "newsgroups"))
      list = &pgm->newsgroups;
    else if(!strucmp(field, "followup-to"))
      list = &pgm->followup_to;
    else if(!strucmp(field, "alltext"))
      list = &pgm->text;
    else if(!strucmp(field, "bodytext"))
      list = &pgm->body;
    else if(!strucmp(field, "keyword"))
      list = &pgm->keyword;
    else{
	set_srch_hdr(field, value, pgm);
	return;
    }

    if(!list)
      return;

    *list = mail_newstringlist();
    decoded = (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf, SIZEOF_20KBUF, value);

    (*list)->text.data = (unsigned char *)cpystr(decoded);
    (*list)->text.size = strlen(decoded);
}


void
set_srch_hdr(char *field, char *value, SEARCHPGM *pgm)
{
    char *decoded;
    SEARCHHEADER  **hdr;

    if(!(field && value && pgm))
      return;

    hdr = &pgm->header;
    if(!hdr)
      return;

    decoded = (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf,
					     SIZEOF_20KBUF, value);
    while(*hdr && (*hdr)->next)
      *hdr = (*hdr)->next;
      
    if(*hdr)
      (*hdr)->next = mail_newsearchheader(field, decoded);
    else
      *hdr = mail_newsearchheader(field, decoded);
}


void
set_search_by_age(INTVL_S *age, SEARCHPGM *pgm, int age_uses_sentdate)
{
    time_t         now, comparetime;
    struct tm     *tm;
    unsigned short i;

    if(!(age && pgm))
      return;

    now = time(0);

    if(age->imin >= 0L && age->imin == age->imax){
	comparetime = now;
	comparetime -= (age->imin * 86400L);
	tm = localtime(&comparetime);
	if(tm && tm->tm_year >= 70){
	    i = mail_shortdate(tm->tm_year - 70, tm->tm_mon + 1,
			       tm->tm_mday);
	    if(age_uses_sentdate)
	      pgm->senton = i;
	    else
	      pgm->on = i;
	}
    }
    else{
	/*
	 * The 20000's are just protecting against overflows.
	 * That's back past the start of email time, anyway.
	 */
	if(age->imin > 0L && age->imin < 20000L){
	    comparetime = now;
	    comparetime -= ((age->imin - 1L) * 86400L);
	    tm = localtime(&comparetime);
	    if(tm && tm->tm_year >= 70){
		i = mail_shortdate(tm->tm_year - 70, tm->tm_mon + 1,
				   tm->tm_mday);
		if(age_uses_sentdate)
		  pgm->sentbefore = i;
		else
		  pgm->before = i;
	    }
	}

	if(age->imax >= 0L && age->imax < 20000L){
	    comparetime = now;
	    comparetime -= (age->imax * 86400L);
	    tm = localtime(&comparetime);
	    if(tm && tm->tm_year >= 70){
		i = mail_shortdate(tm->tm_year - 70, tm->tm_mon + 1,
				   tm->tm_mday);
		if(age_uses_sentdate)
		  pgm->sentsince = i;
		else
		  pgm->since = i;
	    }
	}
    }
}


void
set_search_by_size(INTVL_S *size, SEARCHPGM *pgm)
{
    if(!(size && pgm))
      return;
    
    /*
     * INTVL_S intervals include the endpoints, pgm larger and smaller
     * do not include the endpoints.
     */
    if(size->imin != INTVL_UNDEF && size->imin > 0L)
      pgm->larger  = size->imin - 1L;

    if(size->imax != INTVL_UNDEF && size->imax >= 0L && size->imax != INTVL_INF)
      pgm->smaller = size->imax + 1L;
}


static char *extra_hdrs;

/*
 * Run through the patterns and note which headers we'll need to ask for
 * which aren't normally asked for and so won't be cached.
 */
void
calc_extra_hdrs(void)
{
    PAT_S    *pat = NULL;
    int       alloced_size;
    long      type = (ROLE_INCOL | ROLE_SCORE);
    ARBHDR_S *a;
    PAT_STATE pstate;
    char     *q, *p = NULL, *hdrs[MLCMD_COUNT + 1], **pp;
    INDEX_COL_S *cdesc;
#define INITIALSIZE 1000

    q = (char *)fs_get((INITIALSIZE+1) * sizeof(char));
    q[0] = '\0';
    alloced_size = INITIALSIZE;
    p = q;

    /*
     * *ALWAYS* make sure Resent-To is in the set of
     * extra headers getting fetched.
     *
     * This is because we *will* reference it when we're
     * building header lines and thus want it fetched with
     * the standard envelope data.  Worse, in the IMAP case
     * we're called back from c-client with the envelope data
     * so we can format and display the index lines as they
     * arrive, so we have to ensure the resent-to field
     * is in the cache so we don't reenter c-client
     * to look for it from the callback.  Yeouch.
     */
    add_eh(&q, &p, "resent-to", &alloced_size);
    add_eh(&q, &p, "resent-date", &alloced_size);
    add_eh(&q, &p, "resent-from", &alloced_size);
    add_eh(&q, &p, "resent-cc", &alloced_size);
    add_eh(&q, &p, "resent-subject", &alloced_size);

    /*
     * Sniff at viewer-hdrs too so we can include them
     * if there are any...
     */
    for(pp = ps_global->VAR_VIEW_HEADERS; pp && *pp; pp++)
      if(non_eh(*pp))
	add_eh(&q, &p, *pp, &alloced_size);

    /*
     * Be sure to ask for List management headers too
     * since we'll offer their use in the message view
     */
    for(pp = rfc2369_hdrs(hdrs); *pp; pp++)
      add_eh(&q, &p, *pp, &alloced_size);

    if(nonempty_patterns(type, &pstate))
      for(pat = first_pattern(&pstate);
	  pat;
	  pat = next_pattern(&pstate)){
	  /*
	   * This section wouldn't be necessary if sender was retreived
	   * from the envelope. But if not, we do need to add it.
	   */
	  if(pat->patgrp && pat->patgrp->sender)
	    add_eh(&q, &p, "sender", &alloced_size);

	  if(pat->patgrp && pat->patgrp->arbhdr)
	    for(a = pat->patgrp->arbhdr; a; a = a->next)
	      if(a->field && a->field[0] && a->p && non_eh(a->field))
		add_eh(&q, &p, a->field, &alloced_size);
      }

    /*
     * Check for use of HEADER or X-Priority in index-format.
     */
    for(cdesc = ps_global->index_disp_format; cdesc->ctype != iNothing; cdesc++){
	if(cdesc->ctype == iHeader && cdesc->hdrtok && cdesc->hdrtok->hdrname
           && cdesc->hdrtok->hdrname[0] && non_eh(cdesc->hdrtok->hdrname))
	  add_eh(&q, &p, cdesc->hdrtok->hdrname, &alloced_size);
	else if(cdesc->ctype == iPrio
		|| cdesc->ctype == iPrioAlpha
		|| cdesc->ctype == iPrioBang)
	  add_eh(&q, &p, PRIORITYNAME, &alloced_size);
    }

    /*
     * Check for use of scorevalhdrtok in scoring patterns.
     */
    type = ROLE_SCORE;
    if(nonempty_patterns(type, &pstate))
      for(pat = first_pattern(&pstate);
	  pat;
	  pat = next_pattern(&pstate)){
	  /*
	   * This section wouldn't be necessary if sender was retreived
	   * from the envelope. But if not, we do need to add it.
	   */
	  if(pat->action && pat->action->scorevalhdrtok
	     && pat->action->scorevalhdrtok->hdrname
	     && pat->action->scorevalhdrtok->hdrname[0]
	     && non_eh(pat->action->scorevalhdrtok->hdrname))
	    add_eh(&q, &p, pat->action->scorevalhdrtok->hdrname, &alloced_size);
      }
    
    set_extra_hdrs(q);
    if(q)
      fs_give((void **)&q);
}


int
non_eh(char *field)
{
    char **t;
    static char *existing[] = {"subject", "from", "to", "cc", "sender",
			       "reply-to", "in-reply-to", "message-id",
			       "path", "newsgroups", "followup-to",
			       "references", NULL};

    /*
     * If it is one of these, we should already have it
     * from the envelope or from the extra headers c-client
     * already adds to the list (hdrheader and hdrtrailer
     * in imap4r1.c, Aug 99, slh).
     */
    for(t = existing; *t; t++)
      if(!strucmp(field, *t))
	return(FALSE);

    return(TRUE);
}


/*
 * Add field to extra headers string if not already there.
 */
void
add_eh(char **start, char **ptr, char *field, int *asize)
{
      char *s;

      /* already there? */
      for(s = *start; (s = srchstr(s, field)) != NULL; s++)
	if(s[strlen(field)] == SPACE || s[strlen(field)] == '\0')
	  return;
    
      /* enough space for it? */
      while(strlen(field) + (*ptr - *start) + 1 > *asize){
	  (*asize) *= 2;
	  fs_resize((void **)start, (*asize)+1);
	  *ptr = *start + strlen(*start);
      }

      if(*ptr > *start)
	sstrncpy(ptr, " ", *asize-(*ptr - *start));

      sstrncpy(ptr, field, *asize-(*ptr - *start));

      (*start)[*asize] = '\0';
}


void
set_extra_hdrs(char *hdrs)
{
    free_extra_hdrs();
    if(hdrs && *hdrs)
      extra_hdrs = cpystr(hdrs);
}


char *
get_extra_hdrs(void)
{
    return(extra_hdrs);
}


void
free_extra_hdrs(void)
{
    if(extra_hdrs)
      fs_give((void **)&extra_hdrs);
}


int
is_ascii_string(char *str)
{
    if(!str)
      return(0);
    
    while(*str && isascii(*str))
      str++;
    
    return(*str == '\0');
}


void
free_patline(PAT_LINE_S **patline)
{
    if(patline && *patline){
	free_patline(&(*patline)->next);
	if((*patline)->filename)
	  fs_give((void **)&(*patline)->filename);
	if((*patline)->filepath)
	  fs_give((void **)&(*patline)->filepath);
	free_pat(&(*patline)->first);
	fs_give((void **)patline);
    }
}


void
free_pat(PAT_S **pat)
{
    if(pat && *pat){
	free_pat(&(*pat)->next);
	free_patgrp(&(*pat)->patgrp);
	free_action(&(*pat)->action);
	if((*pat)->raw)
	  fs_give((void **)&(*pat)->raw);

	fs_give((void **)pat);
    }
}


void
free_patgrp(PATGRP_S **patgrp)
{
    if(patgrp && *patgrp){
	if((*patgrp)->nick)
	  fs_give((void **) &(*patgrp)->nick);

	if((*patgrp)->comment)
	  fs_give((void **) &(*patgrp)->comment);

	if((*patgrp)->category_cmd)
	  free_list_array(&(*patgrp)->category_cmd);

	if((*patgrp)->charsets_list)
	  free_strlist(&(*patgrp)->charsets_list);

	free_pattern(&(*patgrp)->to);
	free_pattern(&(*patgrp)->cc);
	free_pattern(&(*patgrp)->recip);
	free_pattern(&(*patgrp)->partic);
	free_pattern(&(*patgrp)->from);
	free_pattern(&(*patgrp)->sender);
	free_pattern(&(*patgrp)->news);
	free_pattern(&(*patgrp)->subj);
	free_pattern(&(*patgrp)->alltext);
	free_pattern(&(*patgrp)->bodytext);
	free_pattern(&(*patgrp)->keyword);
	free_pattern(&(*patgrp)->charsets);
	free_pattern(&(*patgrp)->folder);
	free_arbhdr(&(*patgrp)->arbhdr);
	free_intvl(&(*patgrp)->score);
	free_intvl(&(*patgrp)->age);
	fs_give((void **) patgrp);
    }
}


void
free_pattern(PATTERN_S **pattern)
{
    if(pattern && *pattern){
	free_pattern(&(*pattern)->next);
	if((*pattern)->substring)
	  fs_give((void **)&(*pattern)->substring);
	fs_give((void **)pattern);
    }
}


void
free_arbhdr(ARBHDR_S **arbhdr)
{
    if(arbhdr && *arbhdr){
	free_arbhdr(&(*arbhdr)->next);
	if((*arbhdr)->field)
	  fs_give((void **)&(*arbhdr)->field);
	free_pattern(&(*arbhdr)->p);
	fs_give((void **)arbhdr);
    }
}


void
free_intvl(INTVL_S **intvl)
{
    if(intvl && *intvl){
	free_intvl(&(*intvl)->next);
	fs_give((void **) intvl);
    }
}


void
free_action(ACTION_S **action)
{
    if(action && *action){
	if((*action)->from)
	  mail_free_address(&(*action)->from);
	if((*action)->replyto)
	  mail_free_address(&(*action)->replyto);
	if((*action)->fcc)
	  fs_give((void **)&(*action)->fcc);
	if((*action)->litsig)
	  fs_give((void **)&(*action)->litsig);
	if((*action)->sig)
	  fs_give((void **)&(*action)->sig);
	if((*action)->template)
	  fs_give((void **)&(*action)->template);
	if((*action)->scorevalhdrtok)
	  free_hdrtok(&(*action)->scorevalhdrtok);
	if((*action)->cstm)
	  free_list_array(&(*action)->cstm);
	if((*action)->smtp)
	  free_list_array(&(*action)->smtp);
	if((*action)->nntp)
	  free_list_array(&(*action)->nntp);
	if((*action)->nick)
	  fs_give((void **)&(*action)->nick);
	if((*action)->inherit_nick)
	  fs_give((void **)&(*action)->inherit_nick);
	if((*action)->incol)
	  free_color_pair(&(*action)->incol);
	if((*action)->folder)
	  free_pattern(&(*action)->folder);
	if((*action)->index_format)
	  fs_give((void **)&(*action)->index_format);
	if((*action)->keyword_set)
	  free_pattern(&(*action)->keyword_set);
	if((*action)->keyword_clr)
	  free_pattern(&(*action)->keyword_clr);

	fs_give((void **)action);
    }
}


/*
 * Returns an allocated copy of the pat.
 *
 * Args   pat -- the source pat
 *
 * Returns a copy of pat.
 */
PAT_S *
copy_pat(PAT_S *pat)
{
    PAT_S *new_pat = NULL;

    if(pat){
	new_pat = (PAT_S *)fs_get(sizeof(*new_pat));
	memset((void *)new_pat, 0, sizeof(*new_pat));

	new_pat->patgrp = copy_patgrp(pat->patgrp);
	new_pat->action = copy_action(pat->action);
    }

    return(new_pat);
}


/*
 * Returns an allocated copy of the patgrp.
 *
 * Args   patgrp -- the source patgrp
 *
 * Returns a copy of patgrp.
 */
PATGRP_S *
copy_patgrp(PATGRP_S *patgrp)
{
    char     *p;
    PATGRP_S *new_patgrp = NULL;

    if(patgrp){
	new_patgrp = (PATGRP_S *)fs_get(sizeof(*new_patgrp));
	memset((void *)new_patgrp, 0, sizeof(*new_patgrp));

	if(patgrp->nick)
	  new_patgrp->nick = cpystr(patgrp->nick);
	
	if(patgrp->comment)
	  new_patgrp->comment = cpystr(patgrp->comment);
	
	if(patgrp->to){
	    p = pattern_to_string(patgrp->to);
	    new_patgrp->to = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->to->not = patgrp->to->not;
	}
	
	if(patgrp->from){
	    p = pattern_to_string(patgrp->from);
	    new_patgrp->from = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->from->not = patgrp->from->not;
	}
	
	if(patgrp->sender){
	    p = pattern_to_string(patgrp->sender);
	    new_patgrp->sender = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->sender->not = patgrp->sender->not;
	}
	
	if(patgrp->cc){
	    p = pattern_to_string(patgrp->cc);
	    new_patgrp->cc = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->cc->not = patgrp->cc->not;
	}
	
	if(patgrp->recip){
	    p = pattern_to_string(patgrp->recip);
	    new_patgrp->recip = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->recip->not = patgrp->recip->not;
	}
	
	if(patgrp->partic){
	    p = pattern_to_string(patgrp->partic);
	    new_patgrp->partic = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->partic->not = patgrp->partic->not;
	}
	
	if(patgrp->news){
	    p = pattern_to_string(patgrp->news);
	    new_patgrp->news = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->news->not = patgrp->news->not;
	}
	
	if(patgrp->subj){
	    p = pattern_to_string(patgrp->subj);
	    new_patgrp->subj = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->subj->not = patgrp->subj->not;
	}
	
	if(patgrp->alltext){
	    p = pattern_to_string(patgrp->alltext);
	    new_patgrp->alltext = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->alltext->not = patgrp->alltext->not;
	}
	
	if(patgrp->bodytext){
	    p = pattern_to_string(patgrp->bodytext);
	    new_patgrp->bodytext = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->bodytext->not = patgrp->bodytext->not;
	}
	
	if(patgrp->keyword){
	    p = pattern_to_string(patgrp->keyword);
	    new_patgrp->keyword = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->keyword->not = patgrp->keyword->not;
	}
	
	if(patgrp->charsets){
	    p = pattern_to_string(patgrp->charsets);
	    new_patgrp->charsets = string_to_pattern(p);
	    fs_give((void **)&p);
	    new_patgrp->charsets->not = patgrp->charsets->not;
	}

	if(patgrp->charsets_list)
	  new_patgrp->charsets_list = copy_strlist(patgrp->charsets_list);
	
	if(patgrp->arbhdr){
	    ARBHDR_S *aa, *a, *new_a;

	    aa = NULL;
	    for(a = patgrp->arbhdr; a; a = a->next){
		new_a = (ARBHDR_S *)fs_get(sizeof(*new_a));
		memset((void *)new_a, 0, sizeof(*new_a));

		if(a->field)
		  new_a->field = cpystr(a->field);

		if(a->p){
		    p = pattern_to_string(a->p);
		    new_a->p = string_to_pattern(p);
		    fs_give((void **)&p);
		    new_a->p->not = a->p->not;
		}

		new_a->isemptyval = a->isemptyval;

		if(aa){
		    aa->next = new_a;
		    aa = aa->next;
		}
		else{
		    new_patgrp->arbhdr = new_a;
		    aa = new_patgrp->arbhdr;
		}
	    }
	}

	new_patgrp->fldr_type = patgrp->fldr_type;

	if(patgrp->folder){
	    p = pattern_to_string(patgrp->folder);
	    new_patgrp->folder = string_to_pattern(p);
	    fs_give((void **)&p);
	}

	new_patgrp->inabook = patgrp->inabook;

	if(patgrp->abooks){
	    p = pattern_to_string(patgrp->abooks);
	    new_patgrp->abooks = string_to_pattern(p);
	    fs_give((void **)&p);
	}

	new_patgrp->do_score  = patgrp->do_score;
	if(patgrp->score){
	    INTVL_S *intvl, *iv, *new_iv;

	    intvl = NULL;
	    for(iv = patgrp->score; iv; iv = iv->next){
		new_iv = (INTVL_S *) fs_get(sizeof(*new_iv));
		memset((void *) new_iv, 0, sizeof(*new_iv));

		new_iv->imin = iv->imin;
		new_iv->imax = iv->imax;

		if(intvl){
		    intvl->next = new_iv;
		    intvl = intvl->next;
		}
		else{
		    new_patgrp->score = new_iv;
		    intvl = new_patgrp->score;
		}
	    }
	}

	new_patgrp->do_age    = patgrp->do_age;
	if(patgrp->age){
	    INTVL_S *intvl, *iv, *new_iv;

	    intvl = NULL;
	    for(iv = patgrp->age; iv; iv = iv->next){
		new_iv = (INTVL_S *) fs_get(sizeof(*new_iv));
		memset((void *) new_iv, 0, sizeof(*new_iv));

		new_iv->imin = iv->imin;
		new_iv->imax = iv->imax;

		if(intvl){
		    intvl->next = new_iv;
		    intvl = intvl->next;
		}
		else{
		    new_patgrp->age = new_iv;
		    intvl = new_patgrp->age;
		}
	    }
	}

	new_patgrp->age_uses_sentdate = patgrp->age_uses_sentdate;

	new_patgrp->do_size    = patgrp->do_size;
	if(patgrp->size){
	    INTVL_S *intvl, *iv, *new_iv;

	    intvl = NULL;
	    for(iv = patgrp->size; iv; iv = iv->next){
		new_iv = (INTVL_S *) fs_get(sizeof(*new_iv));
		memset((void *) new_iv, 0, sizeof(*new_iv));

		new_iv->imin = iv->imin;
		new_iv->imax = iv->imax;

		if(intvl){
		    intvl->next = new_iv;
		    intvl = intvl->next;
		}
		else{
		    new_patgrp->size = new_iv;
		    intvl = new_patgrp->size;
		}
	    }
	}

	new_patgrp->stat_new  = patgrp->stat_new;
	new_patgrp->stat_rec  = patgrp->stat_rec;
	new_patgrp->stat_del  = patgrp->stat_del;
	new_patgrp->stat_imp  = patgrp->stat_imp;
	new_patgrp->stat_ans  = patgrp->stat_ans;

	new_patgrp->stat_8bitsubj = patgrp->stat_8bitsubj;
	new_patgrp->stat_bom  = patgrp->stat_bom;
	new_patgrp->stat_boy  = patgrp->stat_boy;

	new_patgrp->do_cat    = patgrp->do_cat;
	if(patgrp->cat){
	    INTVL_S *intvl, *iv, *new_iv;

	    intvl = NULL;
	    for(iv = patgrp->cat; iv; iv = iv->next){
		new_iv = (INTVL_S *) fs_get(sizeof(*new_iv));
		memset((void *) new_iv, 0, sizeof(*new_iv));

		new_iv->imin = iv->imin;
		new_iv->imax = iv->imax;

		if(intvl){
		    intvl->next = new_iv;
		    intvl = intvl->next;
		}
		else{
		    new_patgrp->cat = new_iv;
		    intvl = new_patgrp->cat;
		}
	    }
	}

	if(patgrp->category_cmd)
	  new_patgrp->category_cmd = copy_list_array(patgrp->category_cmd);
    }

    return(new_patgrp);
}


/*
 * Returns an allocated copy of the action.
 *
 * Args   action -- the source action
 *
 * Returns a copy of action.
 */
ACTION_S *
copy_action(ACTION_S *action)
{
    ACTION_S *newaction = NULL;
    char     *p;

    if(action){
	newaction = (ACTION_S *)fs_get(sizeof(*newaction));
	memset((void *)newaction, 0, sizeof(*newaction));

	newaction->is_a_role   = action->is_a_role;
	newaction->is_a_incol  = action->is_a_incol;
	newaction->is_a_score  = action->is_a_score;
	newaction->is_a_filter = action->is_a_filter;
	newaction->is_a_other  = action->is_a_other;
	newaction->is_a_srch   = action->is_a_srch;
	newaction->repl_type   = action->repl_type;
	newaction->forw_type   = action->forw_type;
	newaction->comp_type   = action->comp_type;
	newaction->scoreval    = action->scoreval;
	newaction->kill        = action->kill;
	newaction->state_setting_bits = action->state_setting_bits;
	newaction->move_only_if_not_deleted = action->move_only_if_not_deleted;
	newaction->non_terminating = action->non_terminating;
	newaction->sort_is_set = action->sort_is_set;
	newaction->sortorder   = action->sortorder;
	newaction->revsort     = action->revsort;
	newaction->startup_rule = action->startup_rule;

	if(action->from)
	  newaction->from = copyaddrlist(action->from);
	if(action->replyto)
	  newaction->replyto = copyaddrlist(action->replyto);
	if(action->cstm)
	  newaction->cstm = copy_list_array(action->cstm);
	if(action->smtp)
	  newaction->smtp = copy_list_array(action->smtp);
	if(action->nntp)
	  newaction->nntp = copy_list_array(action->nntp);
	if(action->fcc)
	  newaction->fcc = cpystr(action->fcc);
	if(action->litsig)
	  newaction->litsig = cpystr(action->litsig);
	if(action->sig)
	  newaction->sig = cpystr(action->sig);
	if(action->template)
	  newaction->template = cpystr(action->template);
	if(action->nick)
	  newaction->nick = cpystr(action->nick);
	if(action->inherit_nick)
	  newaction->inherit_nick = cpystr(action->inherit_nick);
	if(action->incol)
	  newaction->incol = new_color_pair(action->incol->fg,
					    action->incol->bg);
	if(action->scorevalhdrtok){
	    newaction->scorevalhdrtok = new_hdrtok(action->scorevalhdrtok->hdrname);
	    if(action->scorevalhdrtok && action->scorevalhdrtok->fieldseps){
		if(newaction->scorevalhdrtok->fieldseps)
		  fs_give((void **) &newaction->scorevalhdrtok->fieldseps);

		newaction->scorevalhdrtok->fieldseps = cpystr(action->scorevalhdrtok->fieldseps);
	    }
	}
	  
	if(action->folder){
	    p = pattern_to_string(action->folder);
	    newaction->folder = string_to_pattern(p);
	    fs_give((void **) &p);
	}

	if(action->keyword_set){
	    p = pattern_to_string(action->keyword_set);
	    newaction->keyword_set = string_to_pattern(p);
	    fs_give((void **) &p);
	}

	if(action->keyword_clr){
	    p = pattern_to_string(action->keyword_clr);
	    newaction->keyword_clr = string_to_pattern(p);
	    fs_give((void **) &p);
	}

	if(action->index_format)
	  newaction->index_format = cpystr(action->index_format);
    }

    return(newaction);
}


/*
 * Given a role, return an allocated role. If this role inherits from
 * another role, then do the correct inheriting so that the result is
 * the role we want to use. The inheriting that is done is just the set
 * of set- actions. This is for role stuff, no inheriting happens for scores
 * or for colors.
 *
 * Args   role -- The source role
 *
 * Returns a role.
 */
ACTION_S *
combine_inherited_role(ACTION_S *role)
{
    PAT_STATE pstate;
    PAT_S    *pat;

    /*
     * Protect against loops in the role inheritance.
     */
    if(role && role->is_a_role && nonempty_patterns(ROLE_DO_ROLES, &pstate))
      for(pat = first_pattern(&pstate); pat; pat = next_pattern(&pstate))
	if(pat->action){
	    if(pat->action == role)
	      pat->action->been_here_before = 1;
	    else
	      pat->action->been_here_before = 0;
	}

    return(combine_inherited_role_guts(role));
}


ACTION_S *
combine_inherited_role_guts(ACTION_S *role)
{
    ACTION_S *newrole = NULL, *inherit_role = NULL;
    PAT_STATE pstate;

    if(role && role->is_a_role){
	newrole = (ACTION_S *)fs_get(sizeof(*newrole));
	memset((void *)newrole, 0, sizeof(*newrole));

	newrole->repl_type  = role->repl_type;
	newrole->forw_type  = role->forw_type;
	newrole->comp_type  = role->comp_type;
	newrole->is_a_role  = role->is_a_role;

	if(role->inherit_nick && role->inherit_nick[0] &&
	   nonempty_patterns(ROLE_DO_ROLES, &pstate)){
	    PAT_S    *pat;

	    /* find the inherit_nick pattern */
	    for(pat = first_pattern(&pstate);
		pat;
		pat = next_pattern(&pstate)){
		if(pat->patgrp &&
		   pat->patgrp->nick &&
		   !strucmp(role->inherit_nick, pat->patgrp->nick)){
		    /* found it, if it has a role, use it */
		    if(!pat->action->been_here_before){
			pat->action->been_here_before = 1;
			inherit_role = pat->action;
		    }

		    break;
		}
	    }

	    /*
	     * inherit_role might inherit further from other roles.
	     * In any case, we copy it so that we'll consistently have
	     * an allocated copy.
	     */
	    if(inherit_role){
		if(inherit_role->inherit_nick && inherit_role->inherit_nick[0])
		  inherit_role = combine_inherited_role_guts(inherit_role);
		else
		  inherit_role = copy_action(inherit_role);
	    }
	}

	if(role->from)
	  newrole->from = copyaddrlist(role->from);
	else if(inherit_role && inherit_role->from)
	  newrole->from = copyaddrlist(inherit_role->from);

	if(role->replyto)
	  newrole->replyto = copyaddrlist(role->replyto);
	else if(inherit_role && inherit_role->replyto)
	  newrole->replyto = copyaddrlist(inherit_role->replyto);

	if(role->fcc)
	  newrole->fcc = cpystr(role->fcc);
	else if(inherit_role && inherit_role->fcc)
	  newrole->fcc = cpystr(inherit_role->fcc);

	if(role->litsig)
	  newrole->litsig = cpystr(role->litsig);
	else if(inherit_role && inherit_role->litsig)
	  newrole->litsig = cpystr(inherit_role->litsig);

	if(role->sig)
	  newrole->sig = cpystr(role->sig);
	else if(inherit_role && inherit_role->sig)
	  newrole->sig = cpystr(inherit_role->sig);

	if(role->template)
	  newrole->template = cpystr(role->template);
	else if(inherit_role && inherit_role->template)
	  newrole->template = cpystr(inherit_role->template);

	if(role->cstm)
	  newrole->cstm = copy_list_array(role->cstm);
	else if(inherit_role && inherit_role->cstm)
	  newrole->cstm = copy_list_array(inherit_role->cstm);

	if(role->smtp)
	  newrole->smtp = copy_list_array(role->smtp);
	else if(inherit_role && inherit_role->smtp)
	  newrole->smtp = copy_list_array(inherit_role->smtp);

	if(role->nntp)
	  newrole->nntp = copy_list_array(role->nntp);
	else if(inherit_role && inherit_role->nntp)
	  newrole->nntp = copy_list_array(inherit_role->nntp);

	if(role->nick)
	  newrole->nick = cpystr(role->nick);

	if(inherit_role)
	  free_action(&inherit_role);
    }

    return(newrole);
}


void
mail_expunge_prefilter(MAILSTREAM *stream, int flags)
{
    int sfdo_state = 0,		/* Some Filter Depends On or Sets State  */
	sfdo_scores = 0,	/* Some Filter Depends On Scores */
	ssdo_state = 0;		/* Some Score Depends On State   */
    
    if(!stream || !sp_flagged(stream, SP_LOCKED))
      return;

    /*
     * An Expunge causes a re-examination of the filters to
     * see if any state changes have caused new matches.
     */
    
    sfdo_scores = (scores_are_used(SCOREUSE_GET) & SCOREUSE_FILTERS);
    if(sfdo_scores)
      ssdo_state = (scores_are_used(SCOREUSE_GET) & SCOREUSE_STATEDEP);

    if(!(sfdo_scores && ssdo_state))
      sfdo_state = some_filter_depends_on_active_state();


    if(sfdo_state || (sfdo_scores && ssdo_state)){
	if(sfdo_scores && ssdo_state)
	  clear_folder_scores(stream);

	reprocess_filter_patterns(stream, sp_msgmap(stream),
				  (flags & MI_CLOSING) |
				  MI_REFILTERING | MI_STATECHGONLY);
    }
}


/*----------------------------------------------------------------------
  Dispatch messages matching FILTER patterns.

  Args:
	stream -- mail stream serving messages
	msgmap -- sequence to msgno mapping table
	recent -- number of recent messages to check (but really only its
	               nonzeroness is used)

 When we're done, any filtered messages are filtered and the message
 mapping table has any filtered messages removed.
  ---*/
void
process_filter_patterns(MAILSTREAM *stream, MSGNO_S *msgmap, long int recent)
{
    long	  i, n, raw;
    imapuid_t     uid;
    int           we_cancel = 0, any_msgs = 0, any_to_filter = 0;
    int		  exbits, nt = 0, pending_actions = 0, for_debugging = 0;
    int           cleared_index_cache = 0;
    long          rflags = ROLE_DO_FILTER;
    char	 *nick = NULL;
    char          busymsg[80];
    MSGNO_S      *tmpmap = NULL;
    MESSAGECACHE *mc;
    PAT_S        *pat, *nextpat = NULL;
    SEARCHPGM	 *pgm = NULL;
    SEARCHSET	 *srchset = NULL;
    long          flags = (SE_NOPREFETCH|SE_FREE);
    PAT_STATE     pstate;

    dprint((5, "process_filter_patterns(stream=%s, recent=%ld)\n",
	    !stream                             ? "<null>"                   :
	     sp_flagged(stream, SP_INBOX)        ? "inbox"                   :
	      stream->original_mailbox            ? stream->original_mailbox :
	       stream->mailbox                     ? stream->mailbox         :
				                      "?",
	    recent));

    if(!msgmap || !stream)
      return;

    if(!recent)
      sp_set_flags(stream, sp_flags(stream) | SP_FILTERED);

    while(stream && stream->nmsgs && nonempty_patterns(rflags, &pstate)){

	for_debugging++;
	pending_actions = 0;
	nextpat = NULL;

	uid = mail_uid(stream, stream->nmsgs);

	/*
	 * Some of the search stuff won't work on old servers so we
	 * get the data and search locally. Big performance hit.
	 */
	if(is_imap_stream(stream) && !modern_imap_stream(stream))
	  flags |= SE_NOSERVER;

	/*
	 * ignore all previously filtered messages
	 * and, if requested, anything not a recent
	 * arrival...
	 *
	 * Here we're using spare6 (MN_STMP), meaning we'll only
	 * search the ones with spare6 marked, new messages coming 
	 * in will not be considered.  There used to be orig_nmsgs,
	 * which kept track of this, but if a message gets expunged,
	 * then a new message could be lower than orig_nmsgs.
	 */
	for(i = 1; i <= stream->nmsgs; i++)
	  if(msgno_exceptions(stream, i, "0", &exbits, FALSE)){
	      if(exbits & MSG_EX_FILTERED){
		  if((mc = mail_elt(stream, i)) != NULL)
		    mc->spare6 = 0;
	      }
	      else if(!recent || !(exbits & MSG_EX_TESTED)){
		  if((mc = mail_elt(stream, i)) != NULL)
		    mc->spare6 = 1;

		  any_to_filter++;
	      }
	      else if((mc = mail_elt(stream, i)) != NULL)
		mc->spare6 = 0;
	  }
	  else{
	      if((mc = mail_elt(stream, i)) != NULL)
	        mc->spare6 = !recent;

	      any_to_filter += !recent;
	  }
	
	if(!any_to_filter){
	  dprint((5, "No messages need filtering\n"));
	}

	/* Then start searching */
	for(pat = first_pattern(&pstate); any_to_filter && pat; pat = nextpat){
	    nextpat = next_pattern(&pstate);
	    dprint((5,
		"Trying filter \"%s\"\n",
		(pat->patgrp && pat->patgrp->nick)
		    ? pat->patgrp->nick : "?"));
	    if(pat->patgrp && !pat->patgrp->bogus
	       && pat->action && !pat->action->bogus
	       && !trivial_patgrp(pat->patgrp)
	       && match_pattern_folder(pat->patgrp, stream)
	       && !match_pattern_folder_specific(pat->action->folder,
						 stream, FOR_FILTER)){

		/*
		 * We could just keep track of spare6 accurately when
		 * we change the msgno_exceptions flags, but...
		 */
		for(i = 1; i <= stream->nmsgs; i++){
		    if((mc=mail_elt(stream, i)) && mc->spare6){
			if(msgno_exceptions(stream, i, "0", &exbits, FALSE)){
			    if(exbits & MSG_EX_FILTERED)
			      mc->sequence = 0;
			    else if(!recent || !(exbits & MSG_EX_TESTED))
			      mc->sequence = 1;
			    else
			      mc->sequence = 0;
			}
			else 
			  mc->sequence = !recent;
		    }
		    else
		      mc->sequence = 0;
		}

		if(!(srchset = build_searchset(stream))){
		    dprint((5, "Empty searchset\n"));
		    continue;		/* nothing to search, move on */
		}

#ifdef DEBUG
		{SEARCHSET *s;
		  dprint((5, "searchset="));
		  for(s = srchset; s; s = s->next){
		    if(s->first == s->last || s->last == 0L){
		      dprint((5, " %ld", s->first));
		    }
		    else{
		      dprint((5, " %ld-%ld", s->first, s->last));
		    }
		  }
		  dprint((5, "\n"));
		}
#endif
		nick = (pat && pat->patgrp && pat->patgrp->nick
			&& pat->patgrp->nick[0]) ? pat->patgrp->nick : NULL;
		snprintf(busymsg, sizeof(busymsg), _("Processing filter \"%s\""),
			nick ? nick : "?");

		/*
		 * The strange last argument is so that the busy message
		 * won't come out until after a second if the user sets
		 * the feature to quell "filtering done". That's because
		 * they are presumably interested in the filtering actions
		 * themselves more than what is happening, so they'd
		 * rather see the action messages instead of the processing
		 * message. That's my theory anyway.
		 */
		if(F_OFF(F_QUELL_FILTER_MSGS, ps_global))
		  any_msgs = we_cancel = busy_cue(busymsg, NULL,
				   F_ON(F_QUELL_FILTER_DONE_MSG, ps_global)
					? 1 : 0);

		if(pat->patgrp->stat_bom != PAT_STAT_EITHER){
		    if(pat->patgrp->stat_bom == PAT_STAT_YES){
			if(!ps_global->beginning_of_month){
			    dprint((5,
		    "Filter %s wants beginning of month and it isn't bom\n",
				   nick ? nick : "?"));
			    continue;
			}
		    }
		    else if(pat->patgrp->stat_bom == PAT_STAT_NO){
			if(ps_global->beginning_of_month){
			    dprint((5,
		   "Filter %s does not want beginning of month and it is bom\n",
				   nick ? nick : "?"));
			    continue;
			}
		    }
		}

		if(pat->patgrp->stat_boy != PAT_STAT_EITHER){
		    if(pat->patgrp->stat_boy == PAT_STAT_YES){
			if(!ps_global->beginning_of_year){
			    dprint((5,
		    "Filter %s wants beginning of year and it isn't boy\n",
				   nick ? nick : "?"));
			    continue;
			}
		    }
		    else if(pat->patgrp->stat_boy == PAT_STAT_NO){
			if(ps_global->beginning_of_year){
			    dprint((5,
		   "Filter %s does not want beginning of year and it is boy\n",
				   nick ? nick : "?"));
			    continue;
			}
		    }
		}
			
		pgm = match_pattern_srchpgm(pat->patgrp, stream, srchset);

		pine_mail_search_full(stream, "UTF-8", pgm, flags);

		/* check scores */
		if(scores_are_used(SCOREUSE_GET) & SCOREUSE_FILTERS &&
		   pat->patgrp->do_score){
		    SEARCHSET *s, *ss;

		    /*
		     * Build a searchset so we can get all the scores we
		     * need and only the scores we need efficiently.
		     */

		    for(i = 1; i <= stream->nmsgs; i++)
		      if((mc = mail_elt(stream, i)) != NULL)
		        mc->sequence = 0;

		    for(s = srchset; s; s = s->next)
		      for(i = s->first; i <= s->last; i++)
			if(i > 0L && stream && i <= stream->nmsgs
			   && (mc=mail_elt(stream, i)) && mc->searched &&
			   get_msg_score(stream, i) == SCORE_UNDEF)
			  mc->sequence = 1;

		    if((ss = build_searchset(stream)) != NULL){
			(void)calculate_some_scores(stream, ss, 0);
			mail_free_searchset(&ss);
		    }

		    /*
		     * Now check the patterns which have matched so far
		     * to see if their score is in the score interval.
		     */
		    for(s = srchset; s; s = s->next)
		      for(i = s->first; i <= s->last; i++)
			if(i > 0L && stream && i <= stream->nmsgs
			   && (mc=mail_elt(stream, i)) && mc->searched){
			    long score;

			    score = get_msg_score(stream, i);

			    /*
			     * If the score is outside all of the intervals,
			     * turn off the searched bit.
			     * So that means we check each interval and if
			     * it is inside any interval we stop and leave
			     * the bit set. If it is outside we keep checking.
			     */
			    if(score != SCORE_UNDEF){
				INTVL_S *iv;

				for(iv = pat->patgrp->score; iv; iv = iv->next)
				  if(score >= iv->imin && score <= iv->imax)
				    break;
				
				if(!iv)
				  mc->searched = NIL;
			    }
			}
		}

		/* check for 8bit subject match or not */
		if(pat->patgrp->stat_8bitsubj != PAT_STAT_EITHER)
		  find_8bitsubj_in_messages(stream, srchset,
					    pat->patgrp->stat_8bitsubj, 0);

		/* if there are still matches, check for charset matches */
		if(pat->patgrp->charsets)
		  find_charsets_in_messages(stream, srchset, pat->patgrp, 0);

		if(pat->patgrp->inabook != IAB_EITHER)
		  address_in_abook(stream, srchset, pat->patgrp->inabook, pat->patgrp->abooks);

		/* Still matches? Run the categorization command on each msg. */
		if(pith_opt_filter_pattern_cmd)
		  (*pith_opt_filter_pattern_cmd)(pat->patgrp->category_cmd, srchset, stream, pat->patgrp->cat_lim, pat->patgrp->cat);

		if(we_cancel){
		    cancel_busy_cue(-1);
		    we_cancel = 0;
		}

		nt = pat->action->non_terminating;
		pending_actions = MAX(nt, pending_actions);

		/*
		 * Change some state bits.
		 * This used to only happen if kill was not set, but
		 * it can be useful to Delete a message even if killing.
		 * That way, it will show up in another pine that isn't
		 * running the same filter as Deleted, so the user won't
		 * bother looking at it.  Hubert 2004-11-16
		 */
		if(pat->action->state_setting_bits
		   || pat->action->keyword_set
		   || pat->action->keyword_clr){
		    tmpmap = NULL;
		    mn_init(&tmpmap, stream->nmsgs);

		    for(i = 1L, n = 0L; i <= stream->nmsgs; i++)
		      if((mc = mail_elt(stream, i)) && mc->searched
			 && !(msgno_exceptions(stream, i, "0", &exbits, FALSE)
			      && (exbits & MSG_EX_FILTERED))){
			if(!n++){
			    mn_set_cur(tmpmap, i);
			}
			else{
			    mn_add_cur(tmpmap, i);
			}
		      }

		    if(n){
			long flagbits;
			char     **keywords_to_set = NULL,
			         **keywords_to_clr = NULL;
			PATTERN_S *pp;
			int        cnt;

			flagbits = pat->action->state_setting_bits;

			if(pat->action->keyword_set){
			    for(cnt = 0, pp = pat->action->keyword_set;
				pp; pp = pp->next)
			      cnt++;

			    keywords_to_set = (char **) fs_get((cnt+1) *
						    sizeof(*keywords_to_set));
			    memset(keywords_to_set, 0,
				   (cnt+1) * sizeof(*keywords_to_set));
			    for(cnt = 0, pp = pat->action->keyword_set;
				pp; pp = pp->next){
				char *q;

				q = nick_to_keyword(pp->substring);
				if(q && q[0])
				  keywords_to_set[cnt++] = cpystr(q);
			    }
				
			    flagbits |= F_KEYWORD;
			}

			if(pat->action->keyword_clr){
			    for(cnt = 0, pp = pat->action->keyword_clr;
				pp; pp = pp->next)
			      cnt++;

			    keywords_to_clr = (char **) fs_get((cnt+1) *
						    sizeof(*keywords_to_clr));
			    memset(keywords_to_clr, 0,
				   (cnt+1) * sizeof(*keywords_to_clr));
			    for(cnt = 0, pp = pat->action->keyword_clr;
				pp; pp = pp->next){
				char *q;

				q = nick_to_keyword(pp->substring);
				if(q && q[0])
				  keywords_to_clr[cnt++] = cpystr(q);
			    }
				
			    flagbits |= F_UNKEYWORD;
			}

			set_some_flags(stream, tmpmap, flagbits,
				       keywords_to_set, keywords_to_clr, 1,
				       nick);
		    }

		    mn_give(&tmpmap);
		}

		/*
		 * The two halves of the if-else are almost the same and
		 * could probably be combined cleverly. The if clause
		 * is simply setting the MSG_EX_FILTERED bit, and leaving
		 * n set to zero. The msgno_exclude is not done in this case.
		 * The else clause excludes each message (because it is
		 * either filtered into nothing or moved to folder). The
		 * exclude messes with the msgmap and that changes max_msgno,
		 * so the loop control is a little tricky.
		 */
		if(!(pat->action->kill || pat->action->folder)){
		  n = 0L;
		  for(i = 1L; i <= mn_get_total(msgmap); i++)
		    if((raw = mn_m2raw(msgmap, i)) > 0L
		       && stream && raw <= stream->nmsgs
		       && (mc = mail_elt(stream, raw)) && mc->searched){
		        dprint((5,
			    "FILTER matching \"%s\": msg %ld%s\n",
			    nick ? nick : "unnamed",
			    raw, nt ? " (dont stop)" : ""));
		        if(msgno_exceptions(stream, raw, "0", &exbits, FALSE))
			  exbits |= (nt ? MSG_EX_FILTONCE : MSG_EX_FILTERED);
		        else
			  exbits = (nt ? MSG_EX_FILTONCE : MSG_EX_FILTERED);
			
			/*
			 * If this matched an earlier non-terminating rule
			 * we've been keeping track of that so that we can
			 * turn it into a permanent match at the end.
			 * However, now we've matched another rule that is
			 * terminating so we don't have to worry about it
			 * anymore. Turn off the flag.
			 */
			if(!nt && exbits & MSG_EX_FILTONCE)
			  exbits ^= MSG_EX_FILTONCE;
			
			exbits &= ~MSG_EX_STATECHG;

		        msgno_exceptions(stream, raw, "0", &exbits, TRUE);
		    }
		}
		else{
		  for(i = 1L, n = 0L; i <= mn_get_total(msgmap); )
		    if((raw = mn_m2raw(msgmap, i))
		       && raw > 0L && stream && raw <= stream->nmsgs
		       && (mc = mail_elt(stream, raw)) && mc->searched){
		        dprint((5,
			      "FILTER matching \"%s\": msg %ld %s%s\n",
			      nick ? nick : "unnamed",
			      raw, pat->action->folder ? "filed" : "killed",
			      nt ? " (dont stop)" : ""));
			if(nt)
			  i++;
			else{
			    if(!cleared_index_cache
			       && stream == ps_global->mail_stream){
				cleared_index_cache = 1;
				clear_index_cache(stream, 0);
			    }

			    msgno_exclude(stream, msgmap, i, 1);
			    /* 
			     * If this message is new, decrement
			     * new_mail_count. Previously, the caller would
			     * do this by counting MN_EXCLUDE before and after,
			     * but the results weren't accurate in the case
			     * where new messages arrived while filtering,
			     * or the filtered message could have gotten
			     * expunged.
			     */
			    if(msgno_exceptions(stream, raw, "0", &exbits,
						FALSE)
			       && (exbits & MSG_EX_RECENT)){
				long l, ll;

			        l = sp_new_mail_count(stream);
			        ll = sp_recent_since_visited(stream);
				dprint((5, "New message being filtered, decrement new_mail_count: %ld -> %ld\n", l, l-1L));
			        if(l > 0L)
				  sp_set_new_mail_count(stream, l-1L);
			        if(ll > 0L)
				  sp_set_recent_since_visited(stream, ll-1L);
			    }
			}

		        if(msgno_exceptions(stream, raw, "0", &exbits, FALSE))
			  exbits |= (nt ? MSG_EX_FILTONCE : MSG_EX_FILTERED);
		        else
			  exbits = (nt ? MSG_EX_FILTONCE : MSG_EX_FILTERED);
			
			/* set pending exclusion  for later */
			if(nt)
			  exbits |= MSG_EX_PEND_EXLD;

			/*
			 * If this matched an earlier non-terminating rule
			 * we've been keeping track of that so that we can
			 * turn it into a permanent match at the end.
			 * However, now we've matched another rule that is
			 * terminating so we don't have to worry about it
			 * anymore. Turn off the flags.
			 */
			if(!nt && exbits & MSG_EX_FILTONCE){
			    exbits ^= MSG_EX_FILTONCE;

			    /* we've already excluded it, too */
			    if(exbits & MSG_EX_PEND_EXLD)
			      exbits ^= MSG_EX_PEND_EXLD;
			}

			exbits &= ~MSG_EX_STATECHG;

			msgno_exceptions(stream, raw, "0", &exbits, TRUE);
			n++;
		    }
		    else
		      i++;
		}

		if(n && pat->action->folder){
		    PATTERN_S *p;
		    int	       err = 0;

		    tmpmap = NULL;
		    mn_init(&tmpmap, stream->nmsgs);

		    /*
		     * For everything matching msg that hasn't
		     * already been saved somewhere, do it...
		     */
		    for(i = 1L, n = 0L; i <= stream->nmsgs; i++)
		      if((mc = mail_elt(stream, i)) && mc->searched
			 && !(msgno_exceptions(stream, i, "0", &exbits, FALSE)
			      && (exbits & MSG_EX_FILED))){
			if(!n++){
			    mn_set_cur(tmpmap, i);
			}
			else{
			    mn_add_cur(tmpmap, i);
			}
		      }

		    /*
		     * Remove already deleted messages from the tmp
		     * message map.
		     * There is a bug with this. If a filter moves a
		     * message to another folder _and_ sets the deleted
		     * status, then the setting of the deleted status
		     * will already have happened above in set_some_flags.
		     * So if the move_only_if_not_deleted bit is set that
		     * message will never be moved. A workaround for the
		     * user is to not set the move-only-if-not-deleted
		     * option.
		     */
		    if(n && pat->action->move_only_if_not_deleted){
			char         *seq;
			MSGNO_S      *tmpmap2 = NULL;
			long          nn = 0L;
			MESSAGECACHE *mc;

			mn_init(&tmpmap2, stream->nmsgs);

			/*
			 * First, make sure elts are valid for all the
			 * interesting messages.
			 */
			if((seq = invalid_elt_sequence(stream, tmpmap)) != NULL){
			    pine_mail_fetch_flags(stream, seq, NIL);
			    fs_give((void **) &seq);
			}

			for(i = mn_first_cur(tmpmap); i > 0L;
			    i = mn_next_cur(tmpmap)){
			    mc = ((raw = mn_m2raw(tmpmap, i)) > 0L
			          && stream && raw <= stream->nmsgs)
				    ? mail_elt(stream, raw) : NULL;
			    if(mc && !mc->deleted){
				if(!nn++){
				    mn_set_cur(tmpmap2, i);
				}
				else{
				    mn_add_cur(tmpmap2, i);
				}
			    }
			}

			mn_give(&tmpmap);
			tmpmap = tmpmap2;
			n = nn;
		    }

		    if(n){
			for(p = pat->action->folder; p; p = p->next){
			  int dval;
			  int flags_for_save;
			  char *processed_name;

			  /* does this filter set delete bit? ... */
			  convert_statebits_to_vals(pat->action->state_setting_bits, &dval, NULL, NULL, NULL);
			  /* ... if so, tell save not to fix it before copy */
			  flags_for_save = SV_FOR_FILT | SV_INBOXWOCNTXT |
				  (nt ? 0 : SV_DELETE) |
				  ((dval != ACT_STAT_SET) ? SV_FIX_DELS : 0);
			  processed_name = detoken_src(p->substring,
						       FOR_FILT, NULL,
						       NULL, NULL, NULL);
			  if(move_filtered_msgs(stream, tmpmap,
					    (processed_name && *processed_name)
					      ? processed_name : p->substring,
					    flags_for_save, nick)){
			      /*
			       * If we filtered into the current
			       * folder, chuck a ping down the
			       * stream so the user can notice it
			       * before the next new mail check...
			       */
			      if(ps_global->mail_stream
				 && ps_global->mail_stream != stream
				 && match_pattern_folder_specific(
						 pat->action->folder,
						 ps_global->mail_stream,
						 FOR_FILTER)){
				  (void) pine_mail_ping(ps_global->mail_stream);
			      }				
			  }
			  else{
			      err = 1;
			      break;
			  }

			  if(processed_name)
			    fs_give((void **) &processed_name);
			}

			if(!err)
			  for(n = mn_first_cur(tmpmap);
			      n > 0L;
			      n = mn_next_cur(tmpmap)){

			      if(msgno_exceptions(stream, mn_m2raw(tmpmap, n),
						  "0", &exbits, FALSE))
				exbits |= (nt ? MSG_EX_FILEONCE : MSG_EX_FILED);
			      else
				exbits = (nt ? MSG_EX_FILEONCE : MSG_EX_FILED);

			      exbits &= ~MSG_EX_STATECHG;

			      msgno_exceptions(stream, mn_m2raw(tmpmap, n),
					       "0", &exbits, TRUE);
			  }
		    }

		    mn_give(&tmpmap);
		}

		mail_free_searchset(&srchset);
	    }

	    /*
	     * If this is the last rule,
	     * we make sure we delete messages that we delayed deleting
	     * in the save. We delayed so that the deletion wouldn't have
	     * an effect on later rules. We convert any temporary
	     * FILED (FILEONCE) and FILTERED (FILTONCE) flags
	     * (which were set by an earlier non-terminating rule)
	     * to permanent. We also exclude some messages from the view.
	     */
	    if(pending_actions && !nextpat){

		pending_actions = 0;
		tmpmap = NULL;
		mn_init(&tmpmap, stream->nmsgs);

		for(i = 1L, n = 0L; i <= mn_get_total(msgmap); i++){

		    raw = mn_m2raw(msgmap, i);
		    if(msgno_exceptions(stream, raw, "0", &exbits, FALSE)){
			if(exbits & MSG_EX_FILEONCE){
			    if(!n++){
				mn_set_cur(tmpmap, raw);
			    }
			    else{
				mn_add_cur(tmpmap, raw);
			    }
			}
		    }
		}

		if(n)
		  set_some_flags(stream, tmpmap, F_DEL, NULL, NULL, 0, NULL);

		mn_give(&tmpmap);

		for(i = 1L; i <= mn_get_total(msgmap); i++){
		    raw = mn_m2raw(msgmap, i);
		    if(msgno_exceptions(stream, raw, "0", &exbits, FALSE)){
			if(exbits & MSG_EX_PEND_EXLD){
			    if(!cleared_index_cache
			       && stream == ps_global->mail_stream){
				cleared_index_cache = 1;
				clear_index_cache(stream, 0);
			    }

			    msgno_exclude(stream, msgmap, i, 1);
			    if(msgno_exceptions(stream, raw, "0",
						&exbits, FALSE)
			       && (exbits & MSG_EX_RECENT)){
				long l, ll;

				/* 
				 * If this message is new, decrement
				 * new_mail_count.  See the above
				 * call to msgno_exclude.
				 */
				l = sp_new_mail_count(stream);
				ll = sp_recent_since_visited(stream);
				dprint((5, "New message being filtered. Decrement new_mail_count: %ld -> %ld\n", l, l-1L));
				if(l > 0L)
				  sp_set_new_mail_count(stream, l - 1L);
				if(ll > 0L)
				  sp_set_recent_since_visited(stream, ll - 1L);
			    }

			    i--;   /* to compensate for loop's i++ */
			}

			/* get rid of temporary flags */
			if(exbits & (MSG_EX_FILTONCE | MSG_EX_FILEONCE |
			             MSG_EX_PEND_EXLD)){
			    if(exbits & MSG_EX_FILTONCE){
				/* convert to permament */
				exbits ^= MSG_EX_FILTONCE;
				exbits |= MSG_EX_FILTERED;
			    }

			    /* convert to permament */
			    if(exbits & MSG_EX_FILEONCE){
				exbits ^= MSG_EX_FILEONCE;
				exbits |= MSG_EX_FILED;
			    }

			    if(exbits & MSG_EX_PEND_EXLD)
			      exbits ^= MSG_EX_PEND_EXLD;

			    exbits &= ~MSG_EX_STATECHG;

			    msgno_exceptions(stream, raw, "0", &exbits,TRUE);
			}
		    }
		}
	    }
	}

	/* New mail arrival means start over */
	if(mail_uid(stream, stream->nmsgs) == uid)
	  break;
	/* else, go again */

	recent = 1; /* only check recent ones now */
    }

    if(!recent){
	/* clear status change flags */
	for(i = 1; i <= stream->nmsgs; i++){
	    if(msgno_exceptions(stream, i, "0", &exbits, FALSE)){
		if(exbits & MSG_EX_STATECHG){
		    exbits &= ~MSG_EX_STATECHG;
		    msgno_exceptions(stream, i, "0", &exbits, TRUE);
		}
	    }
	}
    }

    /* clear any private "recent" flags and add TESTED flag */
    for(i = 1; i <= stream->nmsgs; i++){
	if(msgno_exceptions(stream, i, "0", &exbits, FALSE)){
	    if(exbits & MSG_EX_RECENT
	       || !(exbits & MSG_EX_TESTED)
	       || (!recent && exbits & MSG_EX_STATECHG)){
		exbits &= ~MSG_EX_RECENT;
		exbits |= MSG_EX_TESTED;
		if(!recent)
		  exbits &= ~MSG_EX_STATECHG;

		msgno_exceptions(stream, i, "0", &exbits, TRUE);
	    }
	}
	else{
	    exbits = MSG_EX_TESTED;
	    msgno_exceptions(stream, i, "0", &exbits, TRUE);
	}

	/* clear any stmp flags just in case */
	if((mc = mail_elt(stream, i)) != NULL)
	  mc->spare6 = 0;
    }

    msgmap->flagged_stmp = 0L;

    if(any_msgs && F_OFF(F_QUELL_FILTER_MSGS, ps_global)
       && F_OFF(F_QUELL_FILTER_DONE_MSG, ps_global)){
	q_status_message(SM_ORDER, 0, 1, _("filtering done"));
	display_message('x');
    }
}


/*
 * Re-check the filters for matches because a change of message state may
 * have changed the results.
 */
void
reprocess_filter_patterns(MAILSTREAM *stream, MSGNO_S *msgmap, int flags)
{
    if(stream){
	long i;
	int  exbits;

	if(msgno_include(stream, msgmap, flags)
	   && stream == ps_global->mail_stream
	   && !(flags & MI_CLOSING)){
	    clear_index_cache(stream, 0);
	    refresh_sort(stream, msgmap, SRT_NON);
	    ps_global->mangled_header = 1;
	}

	/*
	 * Passing 1 in the last argument causes it to only look at the
	 * messages we included above, which should be only the ones we
	 * need to look at.
	 */
	process_filter_patterns(stream, msgmap,
				(flags & MI_STATECHGONLY) ? 1L : 0);

	/* clear status change flags */
	for(i = 1; i <= stream->nmsgs; i++){
	    if(msgno_exceptions(stream, i, "0", &exbits, FALSE)){
		if(exbits & MSG_EX_STATECHG){
		    exbits &= ~MSG_EX_STATECHG;
		    msgno_exceptions(stream, i, "0", &exbits, TRUE);
		}
	    }
	}
    }
}


/*
 * When killing or filtering we don't want to match by mistake. So if
 * a pattern has nothing set except the Current Folder Type (which is always
 * set to something) we'll consider it to be trivial and not a match.
 * match_pattern uses this to determine if there is a match, where it is
 * just triggered on the Current Folder Type.
 */
int
trivial_patgrp(PATGRP_S *patgrp)
{
    int ret = 1;

    if(patgrp){
	if(patgrp->subj || patgrp->cc || patgrp->from || patgrp->to ||
	   patgrp->sender || patgrp->news || patgrp->recip || patgrp->partic ||
	   patgrp->alltext || patgrp->bodytext)
	  ret = 0;

	if(ret && patgrp->do_age)
	  ret = 0;

	if(ret && patgrp->do_size)
	  ret = 0;

	if(ret && patgrp->do_score)
	  ret = 0;

	if(ret && patgrp->category_cmd && patgrp->category_cmd[0])
	  ret = 0;

	if(ret && patgrp_depends_on_state(patgrp))
	  ret = 0;

	if(ret && patgrp->stat_8bitsubj != PAT_STAT_EITHER)
	  ret = 0;

	if(ret && patgrp->charsets)
	  ret = 0;

	if(ret && patgrp->stat_bom != PAT_STAT_EITHER)
	  ret = 0;

	if(ret && patgrp->stat_boy != PAT_STAT_EITHER)
	  ret = 0;

	if(ret && patgrp->inabook != IAB_EITHER)
	  ret = 0;

	if(ret && patgrp->arbhdr){
	    ARBHDR_S *a;

	    for(a = patgrp->arbhdr; a && ret; a = a->next)
	      if(a->field && a->field[0] && a->p)
		ret = 0;
	}
    }

    return(ret);
}


int
some_filter_depends_on_active_state(void)
{
    long          rflags = ROLE_DO_FILTER;
    PAT_S        *pat;
    PAT_STATE     pstate;
    int           ret = 0;

    if(nonempty_patterns(rflags, &pstate)){

	for(pat = first_pattern(&pstate);
	    pat && !ret;
	    pat = next_pattern(&pstate))
	  if(patgrp_depends_on_active_state(pat->patgrp))
	    ret++;
    }

    return(ret);
}


/*----------------------------------------------------------------------
  Move all messages with sequence bit lit to dstfldr
 
  Args: stream -- stream to use
	msgmap -- map of messages to be moved
	dstfldr -- folder to receive moved messages
	flags_for_save

  Returns: nonzero on success or on readonly stream
  ----*/
int
move_filtered_msgs(MAILSTREAM *stream, MSGNO_S *msgmap, char *dstfldr,
		   int flags_for_save, char *nick)
{
    long	  n;
    int           we_cancel = 0, width;
    CONTEXT_S	 *save_context = NULL;
    char	  buf[MAX_SCREEN_COLS+1], sbuf[MAX_SCREEN_COLS+1];
    char         *save_ref = NULL;
#define	FILTMSG_MAX	30

    if(!stream)
      return 0;
    
    if(READONLY_FOLDER(stream)){
	dprint((1,
		"Can't delete messages in readonly folder \"%s\"\n",
		STREAMNAME(stream)));
	q_status_message1(SM_ORDER, 1, 3,
			 _("Can't delete messages in readonly folder \"%s\""),
			 STREAMNAME(stream));
	return 1;
    }

    buf[0] = '\0';

    width = MAX(10, ps_global->ttyo ? ps_global->ttyo->screen_cols : 80);
    snprintf(buf, sizeof(buf), "%.30s%.2sMoving %.10s filtered message%.2s to \"\"",
	    nick ? nick : "", nick ? ": " : "",
	    comatose(mn_total_cur(msgmap)), plural(mn_total_cur(msgmap)));
    /* 2 is for brackets, 5 is for " DONE" in busy alarm */
    width -= (strlen(buf) + 2 + 5);
    snprintf(buf, sizeof(buf), "%.30s%.2sMoving %.10s filtered message%.2s to \"%s\"",
	    nick ? nick : "", nick ? ": " : "",
	    comatose(mn_total_cur(msgmap)), plural(mn_total_cur(msgmap)),
	    short_str(dstfldr, sbuf, sizeof(sbuf), width, FrontDots));

    dprint((5, "%s\n", buf));

    if(F_OFF(F_QUELL_FILTER_MSGS, ps_global))
      we_cancel = busy_cue(buf, NULL, 0);

    if(!is_absolute_path(dstfldr)
       && !(save_context = default_save_context(ps_global->context_list)))
      save_context = ps_global->context_list;

    /*
     * Because this save is happening independent of where the user is
     * in the folder hierarchy and has nothing to do with that, we want
     * to ignore the reference field built into the context. Zero it out
     * temporarily here so it won't affect the results of context_apply
     * in save.
     *
     * This might be a problem elsewhere, as well. The same thing as this
     * is also done in match_pattern_folder_specific, which is also only
     * called from within process_filter_patterns. But there could be
     * others. We could have a separate function, something like
     * copy_default_save_context(), that automatically zeroes out the
     * reference field in the copy. However, some of the uses of
     * default_save_context() require that a pointer into the actual
     * context list is returned, so this would have to be done carefully.
     * Besides, we don't know of any other problems so we'll just change
     * these known cases for now.
     */
    if(save_context && save_context->dir){
	save_ref = save_context->dir->ref;
	save_context->dir->ref = NULL;
    }

    n = save(ps_global, stream, save_context, dstfldr, msgmap, flags_for_save);

    if(save_ref)
      save_context->dir->ref = save_ref;

    if(n != mn_total_cur(msgmap)){
	int   exbits;
	long  x;

	buf[0] = '\0';

	/* Clear "filtered" flags for failed messages */
	for(x = mn_first_cur(msgmap); x > 0L; x = mn_next_cur(msgmap))
	  if(n-- <= 0 && msgno_exceptions(stream, mn_m2raw(msgmap, x),
					  "0", &exbits, FALSE)){
	      exbits &= ~(MSG_EX_FILTONCE | MSG_EX_FILEONCE |
			  MSG_EX_FILTERED | MSG_EX_FILED);
	      msgno_exceptions(stream, mn_m2raw(msgmap, x),
			       "0", &exbits, TRUE);
	  }

	/* then re-incorporate them into folder they belong */
	(void) msgno_include(stream, sp_msgmap(stream), MI_NONE);
	clear_index_cache(stream, 0);
	refresh_sort(stream, sp_msgmap(stream), SRT_NON);
	ps_global->mangled_header = 1;
    }
    else{
	snprintf(buf, sizeof(buf), _("Filtered all %s message to \"%s\""),
		comatose(n), dstfldr);
	dprint((5, "%s\n", buf));
    }

    if(we_cancel)
      cancel_busy_cue(buf[0] ? 0 : -1);

    return(buf[0] != '\0');
}


/*----------------------------------------------------------------------
  Move all messages with sequence bit lit to dstfldr
 
  Args: stream -- stream to use
	msgmap -- which messages to set
	flagbits -- which flags to set or clear
	kw_on  -- keywords to set
	kw_off -- keywords to clear
	verbose -- 1 => busy alarm after 1 second
	           2 => forced busy alarm
  ----*/
void
set_some_flags(MAILSTREAM *stream, MSGNO_S *msgmap, long int flagbits,
	       char **kw_on, char **kw_off, int verbose, char *nick)
{
    long	  count = 0L, flipped_flags;
    int           we_cancel = 0;
    char          buf[150], *seq;

    if(!stream)
      return;
    
    if(READONLY_FOLDER(stream)){
	dprint((1, "Can't set flags in readonly folder \"%s\"\n",
		STREAMNAME(stream)));
	q_status_message1(SM_ORDER, 1, 3,
			 _("Can't set flags in readonly folder \"%s\""),
			 STREAMNAME(stream));
	return;
    }

    /* use this to determine if anything needs to be done */
    flipped_flags = ((flagbits & F_ANS)    ? F_UNANS : 0)       |
		    ((flagbits & F_UNANS)  ? F_ANS : 0)         |
		    ((flagbits & F_FLAG)   ? F_UNFLAG : 0)      |
		    ((flagbits & F_UNFLAG) ? F_FLAG : 0)        |
		    ((flagbits & F_DEL)    ? F_UNDEL : 0)       |
		    ((flagbits & F_UNDEL)  ? F_DEL : 0)         |
		    ((flagbits & F_SEEN)   ? F_UNSEEN : 0)      |
		    ((flagbits & F_UNSEEN) ? F_SEEN : 0)        |
		    ((flagbits & F_KEYWORD) ? F_UNKEYWORD : 0)  |
		    ((flagbits & F_UNKEYWORD) ? F_KEYWORD : 0);
    if((seq = currentf_sequence(stream, msgmap, flipped_flags, &count, 0,
			       kw_off, kw_on)) != NULL){
	char *sets = NULL, *clears = NULL;
	char *ps, *pc, **t;
	size_t clen, slen;

	/* allocate enough space for mail_flags arguments */
	for(slen=100, t = kw_on; t && *t; t++)
	  slen += (strlen(*t) + 1);
	
	sets = (char *) fs_get(slen * sizeof(*sets));

	for(clen=100, t = kw_off; t && *t; t++)
	  clen += (strlen(*t) + 1);
	
	clears = (char *) fs_get(clen * sizeof(*clears));

	sets[0] = clears[0] = '\0';
	ps = sets;
	pc = clears;

	snprintf(buf, sizeof(buf), "%.30s%.2sSetting flags in %.10s message%.10s",
		nick ? nick : "", nick ? ": " : "",
		comatose(count), plural(count));

	if(F_OFF(F_QUELL_FILTER_MSGS, ps_global))
	  we_cancel = busy_cue(buf, NULL, verbose ? 0 : 1);

	/*
	 * What's going on here? If we want to set more than one flag
	 * we can do it with a single roundtrip by combining the arguments
	 * into a single call and separating them with spaces.
	 */
	if(flagbits & F_ANS)
	  sstrncpy(&ps, "\\ANSWERED", slen-(ps-sets));
	if(flagbits & F_FLAG){
	    if(ps > sets)
	      sstrncpy(&ps, " ", slen-(ps-sets));

	    sstrncpy(&ps, "\\FLAGGED", slen-(ps-sets));
	}
	if(flagbits & F_DEL){
	    if(ps > sets)
	      sstrncpy(&ps, " ", slen-(ps-sets));

	    sstrncpy(&ps, "\\DELETED", slen-(ps-sets));
	}
	if(flagbits & F_SEEN){
	    if(ps > sets)
	      sstrncpy(&ps, " ", slen-(ps-sets));

	    sstrncpy(&ps, "\\SEEN", slen-(ps-sets));
	}
	if(flagbits & F_KEYWORD){
	    for(t = kw_on; t && *t; t++){
		int i;

		/*
		 * We may be able to tell that this will fail before
		 * we actually try it.
		 */
		if(stream->kwd_create ||
		   (((i=user_flag_index(stream, *t)) >= 0) && i < NUSERFLAGS)){
		    if(ps > sets)
		      sstrncpy(&ps, " ", slen-(ps-sets));
		    
		    sstrncpy(&ps, *t, slen-(ps-sets));
		}
		else{
		  int some_defined = 0;
		  static int msg_delivered = 0;

		  some_defined = some_user_flags_defined(stream);
		
		  if(msg_delivered++ < 2){
		    char b[200], c[200], *p;
		    int w;

		    if(some_defined){
		      snprintf(b, sizeof(b), "Can't set \"%.30s\". No more keywords in ", keyword_to_nick(*t));
		      w = MIN((ps_global->ttyo ? ps_global->ttyo->screen_cols : 80) - strlen(b) - 1 - 2, sizeof(c)-1);
		      p = short_str(STREAMNAME(stream), c, sizeof(c), w, FrontDots);
		      q_status_message2(SM_ORDER, 3, 3, "%s%s!", b, p);
		    }
		    else{
		      snprintf(b, sizeof(b), "Can't set \"%.30s\". Can't add keywords in ", keyword_to_nick(*t));
		      w = MIN((ps_global->ttyo ? ps_global->ttyo->screen_cols : 80) - strlen(b) - 1 - 2, sizeof(c)-1);
		      p = short_str(STREAMNAME(stream), c, sizeof(c), w, FrontDots);
		      q_status_message2(SM_ORDER, 3, 3, "%s%s!", b, p);
		    }
		  }

		  if(some_defined){
		    dprint((1, "Can't set keyword \"%s\". No more keywords allowed in %s\n", *t, stream->mailbox ? stream->mailbox : "target folder"));
		  }
		  else{
		    dprint((1, "Can't set keyword \"%s\". Can't add keywords in %s\n", *t, stream->mailbox ? stream->mailbox : "target folder"));
		  }
		}
	    }
	}

	/* need a separate call for the clears */
	if(flagbits & F_UNANS)
	  sstrncpy(&pc, "\\ANSWERED", clen-(pc-clears));
	if(flagbits & F_UNFLAG){
	    if(pc > clears)
	      sstrncpy(&pc, " ", clen-(pc-clears));

	    sstrncpy(&pc, "\\FLAGGED", clen-(pc-clears));
	}
	if(flagbits & F_UNDEL){
	    if(pc > clears)
	      sstrncpy(&pc, " ", clen-(pc-clears));

	    sstrncpy(&pc, "\\DELETED", clen-(pc-clears));
	}
	if(flagbits & F_UNSEEN){
	    if(pc > clears)
	      sstrncpy(&pc, " ", clen-(pc-clears));

	    sstrncpy(&pc, "\\SEEN", clen-(pc-clears));
	}
	if(flagbits & F_UNKEYWORD){
	    for(t = kw_off; t && *t; t++){
		if(pc > clears)
		  sstrncpy(&pc, " ", clen-(pc-clears));
		
		sstrncpy(&pc, *t, clen-(pc-clears));
	    }
	}


	if(sets[0])
	  mail_flag(stream, seq, sets, ST_SET);

	if(clears[0])
	  mail_flag(stream, seq, clears, 0L);

	fs_give((void **) &sets);
	fs_give((void **) &clears);
	fs_give((void **) &seq);

	if(we_cancel)
	  cancel_busy_cue(buf[0] ? 0 : -1);
    }
}


/*
 * Delete messages which are marked FILTERED and excluded.
 * Messages which are FILTERED but not excluded are those that have had
 * their state set by a filter pattern, but are to remain in the same
 * folder.
 */
void
delete_filtered_msgs(MAILSTREAM *stream)
{
    int	  exbits;
    long  i;
    char *seq;
    MESSAGECACHE *mc;

    for(i = 1L; i <= stream->nmsgs; i++)
      if(msgno_exceptions(stream, i, "0", &exbits, FALSE)
	 && (exbits & MSG_EX_FILTERED)
	 && get_lflag(stream, NULL, i, MN_EXLD)){
	  if((mc = mail_elt(stream, i)) != NULL)
	    mc->sequence = 1;
      }
      else if((mc = mail_elt(stream, i)) != NULL)
	mc->sequence = 0;

    if((seq = build_sequence(stream, NULL, NULL)) != NULL){
	mail_flag(stream, seq, "\\DELETED", ST_SET | ST_SILENT);
	fs_give((void **) &seq);
    }
}
