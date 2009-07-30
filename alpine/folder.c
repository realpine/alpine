#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: folder.c 1144 2008-08-14 16:53:34Z hubert@u.washington.edu $";
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
     folder.c

  Screen to display and manage all the users folders

This puts up a list of all the folders in the users mail directory on
the screen spacing it nicely. The arrow keys move from one to another
and the user can delete the folder or select it to change to or copy a
message to. The dispay lets the user scroll up or down a screen full,
or search for a folder name.
 ====*/


#include "headers.h"
#include "folder.h"
#include "keymenu.h"
#include "status.h"
#include "context.h"
#include "mailview.h"
#include "mailindx.h"
#include "mailcmd.h"
#include "titlebar.h"
#include "alpine.h"
#include "send.h"
#include "help.h"
#include "imap.h"
#include "signal.h"
#include "reply.h"
#include "setup.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/folder.h"
#include "../pith/flag.h"
#include "../pith/filter.h"
#include "../pith/msgno.h"
#include "../pith/thread.h"
#include "../pith/util.h"
#include "../pith/stream.h"
#include "../pith/save.h"
#include "../pith/busy.h"
#include "../pith/list.h"


#define	SUBSCRIBE_PMT	\
		       _("Enter newsgroup name (or partial name to get a list): ")
#define	LISTMODE_GRIPE	_("Use \"X\" to mark selections in list mode")
#define	SEL_ALTER_PMT	_("ALTER folder selection : ")
#define	SEL_TEXT_PMT	_("Select by folder Name or Contents ? ")
#define	SEL_PROP_PMT	_("Select by which folder property ? ")
#define DIR_FOLD_PMT \
	    _("Folder and directory of the same name will be deleted.  Continue")


/*
 * folder_list_write
 */
#define	FLW_NONE	0x00
#define	FLW_LUNK	0x01	/* Using handles */
#define	FLW_SLCT	0x02	/* Some folder is selected, may need X to show it */
#define	FLW_LIST	0x04	/* Allow for ListMode for subscribing */
#define	FLW_UNSEEN	0x08	/* Add (unseen) */



/*----------------------------------------------------------------------
   The data needed to redraw the folders screen, including the case where the 
screen changes size in which case it may recalculate the folder_display.
  ----*/


/*
 * Struct managing folder_lister arguments and holding state
 * for various internal methods
 */
typedef struct _folder_screen {
    CONTEXT_S	    *context;		/* current collection		  */
    CONTEXT_S	    *list_cntxt;	/* list mode collection		  */
    MAILSTREAM     **cache_streamp;     /* cached mailstream              */
    char	     first_folder[MAXFOLDER];
    unsigned	     first_dir:1;	/* first_folder is a dir	  */
    unsigned	     combined_view:1;	/* display flat folder list	  */
    unsigned	     no_dirs:1;		/* no dirs in this screen	  */
    unsigned	     no_empty_dirs:1;	/* no empty dirs on this screen	  */
    unsigned	     relative_path:1;	/* return fully-qual'd specs	  */
    unsigned	     save_sel:1;
    unsigned	     force_intro:1;
    unsigned	     agg_ops:1;
    unsigned	     include_unseen_cnt:1;
    struct key_menu *km;		/* key label/command bindings	  */
    struct _func_dispatch {
	int	 (*valid)(FOLDER_S *, struct _folder_screen *);
	struct {
	    HelpType  text;
	    char     *title;
	} help;
	struct {
	    char	 *bar;
	    TitleBarType  style;
	} title;
    } f;
} FSTATE_S;


/*
 * Struct mananging folder_lister metadata as it get's passed
 * in and back up thru scrolltool
 */
typedef struct _folder_proc {
    FSTATE_S   *fs;
    STRLIST_S  *rv;
    unsigned	done:1;			/* done listing folders?... */
    unsigned	all_done:1;		/* ...and will list no more forever */
} FPROC_S;

#define	FPROC(X)	((FPROC_S *)(X)->proc.data.p)


typedef struct _scanarg {
    MAILSTREAM  *stream;	
    int		 newstream;
    CONTEXT_S	*context;
    char	*pattern;
    char	 type;
} SCANARG_S;


typedef struct _statarg {
    MAILSTREAM  *stream;	
    int		 newstream;
    CONTEXT_S	*context;
    long	 flags;
    long	 nmsgs;
    int		 cmp;
} STATARG_S;


/*
 * Internal prototypes
 */
STRLIST_S  *folders_for_subscribe(struct pine *, CONTEXT_S *, char *);
int         folders_for_post(struct pine *, CONTEXT_S **, char *);
int	    folder_selector(struct pine *, FSTATE_S *, char *, CONTEXT_S **);
void	    folder_sublist_context(char *, CONTEXT_S *, CONTEXT_S *, FDIR_S **, int);
CONTEXT_S  *context_screen(CONTEXT_S *, struct key_menu *, int);
int	    exit_collection_add(struct headerentry *, void (*)(void), int, char **);
char	   *cancel_collection_add(void (*)(void));
char	   *cancel_collection_edit(void (*)(void));
char	   *cancel_collection_editing(char *, void (*)(void));
int	    build_namespace(char *, char **, char **, BUILDER_ARG *, int *);
int	    fl_val_gen(FOLDER_S *, FSTATE_S *);
int	    fl_val_writable(FOLDER_S *, FSTATE_S *);
int	    fl_val_subscribe(FOLDER_S *, FSTATE_S *);
STRLIST_S  *folder_lister(struct pine *, FSTATE_S *);
int	    folder_list_text(struct pine *, FPROC_S *, gf_io_t, HANDLE_S **, int);
int         folder_list_write(gf_io_t, HANDLE_S **, CONTEXT_S *, int, char *, int);
int	    folder_list_write_prefix(FOLDER_S *, int, gf_io_t);
int         folder_list_write_middle(FOLDER_S *fp, CONTEXT_S *ctxt, gf_io_t pc, HANDLE_S *);
int	    folder_list_write_suffix(FOLDER_S *, int, gf_io_t);
int         color_monitored_unseen(FOLDER_S *, int);
int	    folder_list_ith(int, CONTEXT_S *);
char	   *folder_list_center_space(char *, int);
HANDLE_S   *folder_list_handle(FSTATE_S *, HANDLE_S *);
int	    folder_processor(int, MSGNO_S *, SCROLL_S *);
int         folder_lister_clickclick(SCROLL_S *);
int	    folder_lister_choice(SCROLL_S *);
int	    folder_lister_finish(SCROLL_S *, CONTEXT_S *, int);
int	    folder_lister_addmanually(SCROLL_S *);
void	    folder_lister_km_manager(SCROLL_S *, int);
void	    folder_lister_km_sel_manager(SCROLL_S *, int);
void	    folder_lister_km_sub_manager(SCROLL_S *, int);
int	    folder_select(struct pine *, CONTEXT_S *, int);
int	    folder_lister_select(FSTATE_S *, CONTEXT_S *, int, int);
int	    folder_lister_parent(FSTATE_S *, CONTEXT_S *, int, int);
char	   *folder_lister_fullname(FSTATE_S *, char *);
void        folder_export(SCROLL_S *);
int         folder_import(SCROLL_S *, char *, size_t);
int	    folder_select_toggle(CONTEXT_S *, int, int (*)(CONTEXT_S *, int));
char       *end_bracket_no_nest(char *);
int	    group_subscription(char *, size_t, CONTEXT_S *);
int	    rename_folder(CONTEXT_S *, int, char *, size_t, MAILSTREAM *);
int         delete_folder(CONTEXT_S *, int, char *, size_t, MAILSTREAM **);
void        print_folders(FPROC_S *);
int	    scan_get_pattern(char *, char *, int);
int	    folder_select_text(struct pine *, CONTEXT_S *, int);
int	    foreach_do_scan(FOLDER_S *, void *);
int	    scan_scan_folder(MAILSTREAM *, CONTEXT_S *, FOLDER_S *, char *);
int	    folder_select_props(struct pine *, CONTEXT_S *, int);
int	    folder_select_count(long *, int *);
int	    foreach_do_stat(FOLDER_S *, void *);
int	    foreach_folder(CONTEXT_S *, int, int (*)(FOLDER_S *, void *), void *);
int	    folder_delimiter(char *);
int	    shuffle_incoming_folders(CONTEXT_S *, int);
int         swap_incoming_folders(int, int, FLIST *);
int         search_folder_list(void *, char *);
char       *get_post_list(char **);
char       *quote_brackets_if_needed(char *);
#ifdef	_WINDOWS
int	    folder_list_popup(SCROLL_S *, int);
int	    folder_list_select_popup(SCROLL_S *, int);
void	    folder_popup_config(FSTATE_S *, struct key_menu *,MPopup *);
#endif


/*----------------------------------------------------------------------
      Front end to folder lister when it's called from the main menu

 Args: ps -- The general pine_state data structure

 Result: runs context and folder listers

  ----*/
void
folder_screen(struct pine *ps)
{
    int		n = 1;
    CONTEXT_S  *cntxt = ps->context_current;
    STRLIST_S  *folders;
    FSTATE_S    fs;
    MAILSTREAM *cache_stream = NULL;

    dprint((1, "=== folder_screen called ====\n"));
    mailcap_free(); /* free resources we won't be using for a while */
    ps->next_screen = SCREEN_FUN_NULL;

    /* Initialize folder state and dispatches */
    memset(&fs, 0, sizeof(FSTATE_S));
    fs.context		= cntxt;
    fs.cache_streamp    = &cache_stream;
    fs.combined_view	= F_ON(F_CMBND_FOLDER_DISP, ps_global) != 0;
    fs.agg_ops		= F_ON(F_ENABLE_AGG_OPS, ps_global) != 0;
    fs.relative_path	= 1;
    fs.include_unseen_cnt = 1;
    fs.f.valid		= fl_val_gen;
    /* TRANSLATORS: The all upper case things are screen titles */
    fs.f.title.bar	= _("FOLDER LIST");
    fs.f.title.style    = FolderName;
    fs.f.help.text	= h_folder_maint;
    fs.f.help.title	= _("HELP FOR FOLDERS");
    fs.km		= &folder_km;

    if(context_isambig(ps->cur_folder)
       && (IS_REMOTE(ps->cur_folder) || !is_absolute_path(ps->cur_folder)
	   || (cntxt && cntxt->context && cntxt->context[0] == '{'))){
	if(strlen(ps_global->cur_folder) < MAXFOLDER - 1){
	    strncpy(fs.first_folder, ps_global->cur_folder, MAXFOLDER);
	    fs.first_folder[MAXFOLDER-1] = '\0';
	}

	/*
	 * If we're asked to start in the folder list of the current
	 * folder and it looks like the current folder is part of the
	 * current context, try to start in the list of folders in the
	 * current context.
	 */
	if(ps->start_in_context || fs.combined_view){
	    char	tmp[MAILTMPLEN], *p, *q;
	    FDIR_S *fp;

	    ps->start_in_context = 0;
	    n = 0;

	    if(!(NEWS_TEST(cntxt) || (cntxt->use & CNTXT_INCMNG))
	       && cntxt->dir->delim
	       && strchr(ps->cur_folder, cntxt->dir->delim)){
		for(p = strchr(q = ps->cur_folder, cntxt->dir->delim);
		    p;
		    p = strchr(q = ++p, cntxt->dir->delim)){
		    strncpy(tmp, q, MIN(p - q, sizeof(tmp)-1));
		    tmp[MIN(p - q, sizeof(tmp)-1)] = '\0';

		    fp = next_folder_dir(cntxt, tmp, FALSE, fs.cache_streamp);

		    fp->desc    = folder_lister_desc(cntxt, fp);

		    /* Insert new directory into list */
		    fp->delim   = cntxt->dir->delim;
		    fp->prev    = cntxt->dir;
		    fp->status |= CNTXT_SUBDIR;
		    cntxt->dir  = fp;
		}
	    }
	}
    }

    while(ps->next_screen == SCREEN_FUN_NULL
	  && ((n++) ? (cntxt = context_screen(cntxt,&c_mgr_km,1)) != NULL :1)){

	fs.context = cntxt;
	if(F_ON(F_ENABLE_INCOMING_CHECKING, ps) && ps->VAR_INCOMING_FOLDERS && ps->VAR_INCOMING_FOLDERS[0])
	  ps->in_folder_screen = 1;

	if((folders = folder_lister(ps, &fs)) != NULL){

	    ps->in_folder_screen = 0;

	    if(ps && ps->ttyo){
		blank_keymenu(ps->ttyo->screen_rows - 2, 0);
		ps->mangled_footer = 1;
	    }

	    if(do_broach_folder((char *) folders->name, 
				fs.context, fs.cache_streamp 
				&& *fs.cache_streamp ? fs.cache_streamp
				: NULL, 0L) == 1){
		reset_context_folders(ps->context_list);
		ps->next_screen = mail_index_screen;
	    }

	    if(fs.cache_streamp)
	      *fs.cache_streamp = NULL;
	    free_strlist(&folders);
	}

	ps->in_folder_screen = 0;
    }

    if(fs.cache_streamp && *fs.cache_streamp)
      pine_mail_close(*fs.cache_streamp);

    ps->prev_screen = folder_screen;
}


/*----------------------------------------------------------------------
      Front end to folder lister when it's called from the main menu

 Args: ps -- The general pine_state data structure

 Result: runs context and folder listers

  ----*/
void
folder_config_screen(struct pine *ps, int edit_exceptions)
{
    CONT_SCR_S css;
    char title[50], htitle[50];

    dprint((1, "=== folder_config_screen called ====\n"));
    mailcap_free(); /* free resources we won't be using for a while */

    if(edit_exceptions){
      snprintf(title, sizeof(title), _("SETUP EXCEPTIONS COLLECTION LIST"));
      snprintf(htitle, sizeof(htitle), _("HELP FOR SETUP EXCEPTIONS COLLECTIONS"));
    }
    else{
      snprintf(title, sizeof(title), _("SETUP COLLECTION LIST"));
      snprintf(htitle, sizeof(htitle), _("HELP FOR SETUP COLLECTIONS"));
    }

    memset(&css, 0, sizeof(CONT_SCR_S));
    css.title	     = title;
  /* TRANSLATORS: Print something1 using something2.
     contexts is something1 */
    css.print_string = _("contexts");
    css.contexts     = &ps_global->context_list;
    css.help.text    = h_collection_maint;
    css.help.title   = htitle;
    css.keymenu	     = &c_cfg_km;
    css.edit	     = 1;

    /*
     * Use conf_scroll_screen to manage display/selection
     * of contexts
     */
    context_config_screen(ps_global, &css, edit_exceptions);
}


/*----------------------------------------------------------------------
 Browse folders for ^T selection from the Goto Prompt
  
 Args:  ps --
	cntxtp -- pointer to addr of context to start in, list, and return
	folder -- pointer to buffer inwhich to return selected folder

 Returns: 1 if we have something valid in cntxtp and folder
	  0 if problem or user cancelled

  ----*/     
int
folders_for_goto(struct pine *ps, CONTEXT_S **cntxtp, char *folder, int sublist)
{
    int	       rv;
    CONTEXT_S  fake_context;
    FDIR_S    *fake_dir = NULL;
    FSTATE_S   fs;

    dprint((1, "=== folders_for_goto called ====\n"));

    /* Initialize folder state and dispatches */
    memset(&fs, 0, sizeof(FSTATE_S));
    fs.context	     = *cntxtp;
    fs.combined_view = !sublist && F_ON(F_CMBND_FOLDER_DISP, ps_global) != 0;
    fs.f.valid	     = fl_val_gen;
    fs.f.title.bar   = _("GOTO: SELECT FOLDER");
    fs.f.title.style = FolderName;
    fs.f.help.text   = h_folder_open;
    fs.f.help.title  = _("HELP FOR OPENING FOLDERS");
    fs.km	     = &folder_sel_km;

    /* If we were provided a string,
     * dummy up a context for a substring match
     */
    if(sublist && *folder && context_isambig(folder)){
	if((*cntxtp)->use & CNTXT_INCMNG){
	    q_status_message(SM_ORDER, 0, 3,
			     _("All folders displayed for Incoming Collection"));
	}
	else{
	    folder_sublist_context(folder, *cntxtp, &fake_context,
				   &fake_dir, sublist);
	    fs.context	     = &fake_context;
	    fs.relative_path = 1;
	    fs.force_intro   = 1;
	    cntxtp	     = &fs.context;
	}
    }

    rv = folder_selector(ps, &fs, folder, cntxtp);

    if(fake_dir)
      free_fdir(&fake_dir, TRUE);

    return(rv);
}


/*----------------------------------------------------------------------
 Browse folders for ^T selection from the Save Prompt
  
 Args:  ps --
	cntxtp -- pointer to addr of context to start in, list, and return
	folder -- pointer to buffer inwhich to return selected folder

 Returns: 1 if we have something valid in cntxtp and folder
	  0 if problem or user cancelled

  ----*/
int
folders_for_save(struct pine *ps, CONTEXT_S **cntxtp, char *folder, int sublist)
{
    int	       rv;
    CONTEXT_S  fake_context;
    FDIR_S    *fake_dir = NULL;
    FSTATE_S   fs;

    dprint((1, "=== folders_for_save called ====\n"));

    /* Initialize folder state and dispatches */
    memset(&fs, 0, sizeof(FSTATE_S));
    fs.context	     = *cntxtp;
    fs.combined_view = F_ON(F_CMBND_FOLDER_DISP, ps_global) != 0;
    fs.f.valid	     = fl_val_gen;
    fs.f.title.bar   = _("SAVE: SELECT FOLDER");
    fs.f.title.style = MessageNumber;
    fs.f.help.text   = h_folder_save;
    fs.f.help.title  = _("HELP FOR SAVING MESSAGES TO FOLDERS");
    fs.km	     = &folder_sela_km;

    /* If we were provided a string,
     * dummy up a context for a substring match
     */
    if(sublist && *folder && context_isambig(folder)){
	if((*cntxtp)->use & CNTXT_INCMNG){
	    q_status_message(SM_ORDER, 0, 3,
			     _("All folders displayed for Incoming Collection"));
	}
	else{
	    folder_sublist_context(folder, *cntxtp, &fake_context,
				   &fake_dir, sublist);
	    fs.context	     = &fake_context;
	    fs.relative_path = 1;
	    fs.force_intro   = 1;
	    cntxtp	     = &fs.context;
	}
    }

    rv = folder_selector(ps, &fs, folder, cntxtp);

    if(fake_dir)
      free_fdir(&fake_dir, TRUE);

    return(rv);
}


/*----------------------------------------------------------------------
 Browse folders for ^T selection from the Subscribe Prompt
  
 Args:  ps --
	cntxtp -- pointer to addr of context to start in, list, and return
	folder -- pointer to buffer inwhich to return selected folder

 Returns: 1 if we have something valid in cntxtp and folder
	  0 if problem or user cancelled

  ----*/
STRLIST_S *
folders_for_subscribe(struct pine *ps, CONTEXT_S *cntxt, char *folder)
{
    STRLIST_S	*folders = NULL;
    FSTATE_S	 fs;
    void       (*redraw)(void);

    dprint((1, "=== folders_for_sub called ====\n"));

    /* Initialize folder state and dispatches */
    memset(&fs, 0, sizeof(FSTATE_S));
    fs.context	     = cntxt;
    fs.f.valid	     = fl_val_subscribe;
    fs.f.title.bar   = _("SUBSCRIBE: SELECT FOLDER");
    fs.f.title.style = FolderName;
    fs.f.help.text   = h_folder_subscribe;
    fs.f.help.title  = _("HELP SELECTING NEWSGROUP TO SUBSCRIBE TO");
    fs.km	     = &folder_sub_km;
    fs.force_intro   = 1;

    fs.context		= cntxt;
    redraw		= ps_global->redrawer;
    folders		= folder_lister(ps, &fs);
    ps_global->redrawer = redraw;
    if(ps_global->redrawer)
      (*ps_global->redrawer)();

    return(folders);
}


/*----------------------------------------------------------------------
 Browse folders for ^T selection for posting
  
 Args:  ps --
	cntxtp -- pointer to addr of context to start in, list, and return
	folder -- pointer to buffer inwhich to return selected folder

 Returns: 1 if we have something valid in cntxtp and folder
	  0 if problem or user cancelled

  ----*/
int
folders_for_post(struct pine *ps, CONTEXT_S **cntxtp, char *folder)
{
    FSTATE_S fs;

    dprint((1, "=== folders_for_post called ====\n"));

    /* Initialize folder state and dispatches */
    memset(&fs, 0, sizeof(FSTATE_S));
    fs.context	     = *cntxtp;
    fs.f.valid	     = fl_val_subscribe;
    fs.f.title.bar   = _("NEWS: SELECT GROUP");
    fs.f.title.style = FolderName;
    fs.f.help.text   = h_folder_postnews;
    fs.f.help.title  = _("HELP FOR SELECTING NEWSGROUP TO POST TO");
    fs.km	     = &folder_post_km;

    return(folder_selector(ps, &fs, folder, cntxtp));
}


int
folder_selector(struct pine *ps, FSTATE_S *fs, char *folder, CONTEXT_S **cntxtp)
{
    int	       rv = 0;
    STRLIST_S *folders;

    do{
	fs->context = *cntxtp;
	if((folders = folder_lister(ps, fs)) != NULL){
	    strncpy(folder, (char *) folders->name, MAILTMPLEN-1);
	    folder[MAILTMPLEN-1] = '\0';
	    free_strlist(&folders);
	    *cntxtp = fs->context;
	    rv++;
	    break;
	}
	else if(!(fs->context
		  && (fs->context->next || fs->context->prev))
		|| fs->combined_view)
	  break;
    }
    while((*cntxtp = context_screen(*cntxtp, &c_sel_km, 0)) != NULL);

    return(rv);
}


void
folder_sublist_context(char *folder, CONTEXT_S *cntxt, CONTEXT_S *new_cntxt, FDIR_S **new_dir, int lev)
{
    char *p, *q, *ref, *wildcard;

    *new_cntxt		   = *cntxt;
    new_cntxt->next = new_cntxt->prev = NULL;
    new_cntxt->dir  = *new_dir = (FDIR_S *) fs_get(sizeof(FDIR_S));
    memset(*new_dir, 0, sizeof(FDIR_S));
    (*new_dir)->status |= CNTXT_NOFIND;
    (*new_dir)->folders	= init_folder_entries();
    if(!((*new_dir)->delim = cntxt->dir->delim)){
	/* simple LIST to acquire delimiter, doh */
	build_folder_list(NULL, new_cntxt, "", NULL,
			  NEWS_TEST(new_cntxt) ? BFL_LSUB : BFL_NONE);
	new_cntxt->dir->status = CNTXT_NOFIND;
    }

    wildcard = NEWS_TEST(new_cntxt) ? "*" : "%";

    if((p = strrindex(folder, (*new_dir)->delim)) != NULL){
	snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%s%s", p + 1, wildcard);
	tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	(*new_dir)->view.internal = cpystr(tmp_20k_buf);
	for(ref = tmp_20k_buf, q = new_cntxt->context;
	    (*ref = *q) != '\0' && !(*q == '%' && *(q+1) == 's');
	    ref++, q++)
	  ;

	for(q = folder; q <= p; q++, ref++)
	  *ref = *q;

	*ref = '\0';
	(*new_dir)->ref = cpystr(tmp_20k_buf);

	(*new_dir)->status |= CNTXT_SUBDIR;
    }
    else{
	snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%s%s%s",
		(lev > 1) ? wildcard : "", folder, wildcard);
	tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	(*new_dir)->view.internal = cpystr(tmp_20k_buf);
	/* leave (*new_dir)->ref == NULL */
    }

    snprintf(tmp_20k_buf, SIZEOF_20KBUF, _("List of folders matching \"%s*\""), folder);
    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
    (*new_dir)->desc = cpystr(tmp_20k_buf);
}


/*----------------------------------------------------------------------
 Browse folders for ^T selection from the composer
  
 Args: error_mess -- pointer to place to return an error message
  
 Returns: result if folder selected, NULL if not
	  Composer expects the result to be alloc'd here 
 
  ----*/     
char *
folders_for_fcc(char **errmsg)
{
    char      *rs = NULL;
    STRLIST_S *folders;
    FSTATE_S   fs;

    dprint((1, "=== folders_for_fcc called ====\n"));

    /* Coming back from composer */
    fix_windsize(ps_global);
    init_sigwinch();

    /* Initialize folder state and dispatches */
    memset(&fs, 0, sizeof(FSTATE_S));
    fs.combined_view = F_ON(F_CMBND_FOLDER_DISP, ps_global) != 0;
    fs.f.valid	     = fl_val_gen;
    fs.f.title.bar   = _("FCC: SELECT FOLDER");
    fs.f.title.style = FolderName;
    fs.f.help.text   = h_folder_fcc;
    fs.f.help.title  = _("HELP FOR SELECTING THE FCC");
    fs.km	     = &folder_sela_km;

    /* start in the default save context */
    fs.context = default_save_context(ps_global->context_list);

    do{
	if((folders = folder_lister(ps_global, &fs)) != NULL){
	    char *name;

	    /* replace nickname with full name */
	    if(!(name = folder_is_nick((char *) folders->name,
				       FOLDERS(fs.context), 0)))
	      name = (char *) folders->name;

	    if(context_isambig(name) && !((fs.context->use) & CNTXT_SAVEDFLT)){
		char path_in_context[MAILTMPLEN];

		context_apply(path_in_context, fs.context, name,
			      sizeof(path_in_context));
		if(!(IS_REMOTE(path_in_context)
		     || is_absolute_path(path_in_context))){
		    /*
		     * Name is relative to the home directory,
		     * so have to add that. Otherwise, the sender will
		     * assume it is in the primary collection since it
		     * will still be ambiguous.
		     */
		    build_path(tmp_20k_buf, ps_global->ui.homedir,
			       path_in_context, SIZEOF_20KBUF);
		    rs = cpystr(tmp_20k_buf);
		}
		else
		  rs = cpystr(path_in_context);
	    }
	    else
	      rs = cpystr(name);

	    free_strlist(&folders);
	    break;
	}
	else if(!(fs.context && (fs.context->next || fs.context->prev))
		|| fs.combined_view)
	  break;
    }
    while((fs.context = context_screen(fs.context, &c_fcc_km, 0)) != NULL);

    return(rs);
}


/*----------------------------------------------------------------------
 Browse folders for ^T selection from the role editor

 Returns: result if folder selected, NULL if not
	  Tesult is alloc'd here 
 
  ----*/     
char *
folder_for_config(int flags)
{
    char      *rs = NULL;
    STRLIST_S *folders;
    FSTATE_S   fs;

    dprint((1, "=== folder_for_config called ====\n"));

    /* Initialize folder state and dispatches */
    memset(&fs, 0, sizeof(FSTATE_S));
    fs.combined_view = F_ON(F_CMBND_FOLDER_DISP, ps_global) != 0;
    fs.f.valid	     = fl_val_gen;
    fs.f.title.bar   = _("SELECT FOLDER");
    fs.f.title.style = FolderName;
    fs.km	     = &folder_sela_km;
    if(flags & FOR_PATTERN){
	fs.f.help.text   = h_folder_pattern_roles;
	fs.f.help.title  = _("HELP FOR SELECTING CURRENT FOLDER");
    }
    else if(flags & FOR_OPTIONSCREEN){
	fs.f.help.text   = h_folder_stayopen_folders;
	fs.f.help.title  = _("HELP FOR SELECTING FOLDER");
    }
    else{
	fs.f.help.text   = h_folder_action_roles;
	fs.f.help.title  = _("HELP FOR SELECTING FOLDER");
    }

    /* start in the current context */
    fs.context = ps_global->context_current;

    do{
	if((folders = folder_lister(ps_global, &fs)) != NULL){
	    char *name = NULL;

	    /* replace nickname with full name */
	    if(!(flags & (FOR_PATTERN | FOR_OPTIONSCREEN)))
	      name = folder_is_nick((char *) folders->name,
				    FOLDERS(fs.context), 0);

	    if(!name)
	      name = (char *) folders->name;

	    if(context_isambig(name) &&
	       !(flags & (FOR_PATTERN | FOR_OPTIONSCREEN) &&
	         folder_is_nick(name, FOLDERS(fs.context), 0))){
		char path_in_context[MAILTMPLEN];

		context_apply(path_in_context, fs.context, name,
			      sizeof(path_in_context));

		/*
		 * We may still have a non-fully-qualified name. In the
		 * action case, that will be interpreted in the primary
		 * collection instead of as a local name.
		 * Qualify that name the same way we
		 * qualify it in match_pattern_folder_specific.
		 */
		if(!(IS_REMOTE(path_in_context) ||
		     path_in_context[0] == '#')){
		    if(strlen(path_in_context) < (MAILTMPLEN/2)){
			char  tmp[MAX(MAILTMPLEN,NETMAXMBX)];
			char *t;

			t = mailboxfile(tmp, path_in_context);
			rs = cpystr(t);
		    }
		    else{	/* the else part should never happen */
			build_path(tmp_20k_buf, ps_global->ui.homedir,
				   path_in_context, SIZEOF_20KBUF);
			rs = cpystr(tmp_20k_buf);
		    }
		}
		else
		  rs = cpystr(path_in_context);
	    }
	    else
	      rs = cpystr(name);

	    free_strlist(&folders);
	    break;
	}
	else if(!(fs.context && (fs.context->next || fs.context->prev))
		|| fs.combined_view)
	  break;
    }
    while((fs.context = context_screen(fs.context, &c_fcc_km, 0)) != NULL);

    return(rs);
}


/*
 * offer screen with list of contexts to select and some sort
 * of descriptions
 */
CONTEXT_S *
context_screen(CONTEXT_S *start, struct key_menu *km, int edit_config)
{
    /* If a list, let the user tell us what to do */
    if(F_OFF(F_CMBND_FOLDER_DISP, ps_global)
       && ps_global->context_list
       && ps_global->context_list->next){
	CONT_SCR_S css;

	memset(&css, 0, sizeof(CONT_SCR_S));
	css.title	 = _("COLLECTION LIST");
	css.print_string = _("contexts");
	css.start        = start;
	css.contexts	 = &ps_global->context_list;
	css.help.text	 = h_collection_screen;
	css.help.title   = _("HELP FOR COLLECTION LIST");
	css.keymenu	 = km;
	css.edit	 = edit_config;

	/*
	 * Use conf_scroll_screen to manage display/selection
	 * of contexts
	 */
	return(context_select_screen(ps_global, &css, 0));
    }

    return(ps_global->context_list);
}


static struct headerentry headents_templ[]={
  /* TRANSLATORS: these are the headings for setting up a collection of
     folders, PATH is a filesystem path, VIEW is sort of a technical
     term that can be used to restrict the View to fewer folders */
  {"Nickname  : ",  N_("Nickname"),  h_composer_cntxt_nick, 12, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Server    : ",  N_("Server"),  h_composer_cntxt_server, 12, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"Path      : ",  N_("Path"),  h_composer_cntxt_path, 12, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {"View      : ",  N_("View"),  h_composer_cntxt_view, 12, 0, NULL,
   NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE},
  {NULL, NULL, NO_HELP, 0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, KS_NONE}
};
#define	AC_NICK	0
#define	AC_SERV	1
#define	AC_PATH	2
#define	AC_VIEW	3


char *
context_edit_screen(struct pine *ps, char *func, char *def_nick,
		    char *def_serv, char *def_path, char *def_view)
{
    int	       editor_result, i, j;
    char       nickpart[MAILTMPLEN], servpart[MAILTMPLEN], new_cntxt[MAILTMPLEN];
    char       pathpart[MAILTMPLEN], allbutnick[MAILTMPLEN];
    char       tmp[MAILTMPLEN], *nick, *serv, *path, *view,
	      *return_cntxt = NULL, *val, *p, new[MAILTMPLEN];
    char       nickpmt[100], servpmt[100], pathpmt[100], viewpmt[100];
    int        indent;
    PICO       pbf;
    STORE_S   *msgso;
    NETMBX     mb;

    standard_picobuf_setup(&pbf);
    pbf.pine_flags   |= P_NOBODY;
    pbf.exittest      = exit_collection_add;
    pbf.canceltest    = (func && !strucmp(func, "EDIT")) ? cancel_collection_edit
							 : cancel_collection_add;
    snprintf(tmp, sizeof(tmp), _("FOLDER COLLECTION %s"), func);
    tmp[sizeof(tmp)-1] = '\0';
    pbf.pine_anchor   = set_titlebar(tmp, ps_global->mail_stream,
				      ps_global->context_current,
				      ps_global->cur_folder,ps_global->msgmap, 
				      0, FolderName, 0, 0, NULL);

    /* An informational message */
    if((msgso = so_get(PicoText, NULL, EDIT_ACCESS)) != NULL){
	pbf.msgtext = (void *) so_text(msgso);
	so_puts(msgso,
       _("\n   Fill in the fields above to add a Folder Collection to your"));
	so_puts(msgso,
       _("\n   COLLECTION LIST screen."));
	so_puts(msgso,
       _("\n   Use the \"^G\" command to get help specific to each item, and"));
	so_puts(msgso,
       _("\n   use \"^X\" when finished."));
    }


    pbf.headents = (struct headerentry *)fs_get((sizeof(headents_templ)
						  /sizeof(struct headerentry))
						 * sizeof(struct headerentry));
    memset((void *) pbf.headents, 0,
	   (sizeof(headents_templ)/sizeof(struct headerentry))
	   * sizeof(struct headerentry));

    for(i = 0; headents_templ[i].prompt; i++)
      pbf.headents[i] = headents_templ[i];

    indent = utf8_width(_("Nickname")) + 2;

    nick = cpystr(def_nick ? def_nick : "");
    pbf.headents[AC_NICK].realaddr = &nick;
    pbf.headents[AC_NICK].maxlen   = strlen(nick);
    utf8_snprintf(nickpmt, sizeof(nickpmt), "%-*.*w: ", indent, indent, _("Nickname"));
    pbf.headents[AC_NICK].prompt   = nickpmt;
    pbf.headents[AC_NICK].prwid    = indent+2;

    serv = cpystr(def_serv ? def_serv : "");
    pbf.headents[AC_SERV].realaddr = &serv;
    pbf.headents[AC_SERV].maxlen   = strlen(serv);
    utf8_snprintf(servpmt, sizeof(servpmt), "%-*.*w: ", indent, indent, _("Server"));
    pbf.headents[AC_SERV].prompt   = servpmt;
    pbf.headents[AC_SERV].prwid    = indent+2;

    path = cpystr(def_path ? def_path : "");
    pbf.headents[AC_PATH].realaddr = &path;
    pbf.headents[AC_PATH].maxlen   = strlen(path);
    pbf.headents[AC_PATH].bldr_private = (void *) 0;
    utf8_snprintf(pathpmt, sizeof(pathpmt), "%-*.*w: ", indent, indent, _("Path"));
    pbf.headents[AC_PATH].prompt   = pathpmt;
    pbf.headents[AC_PATH].prwid    = indent+2;

    view = cpystr(def_view ? def_view : "");
    pbf.headents[AC_VIEW].realaddr = &view;
    pbf.headents[AC_VIEW].maxlen   = strlen(view);
    utf8_snprintf(viewpmt, sizeof(viewpmt), "%-*.*w: ", indent, indent, _("View"));
    pbf.headents[AC_VIEW].prompt   = viewpmt;
    pbf.headents[AC_VIEW].prwid    = indent+2;

    /*
     * If this is new context, setup to query IMAP server
     * for location of personal namespace.
     */
    if(!(def_nick || def_serv || def_path || def_view)){
	pbf.headents[AC_SERV].builder	      = build_namespace;
	pbf.headents[AC_SERV].affected_entry = &pbf.headents[AC_PATH];
	pbf.headents[AC_SERV].bldr_private   = (void *) 0;
    }

    /* pass to pico and let user change them */
    editor_result = pico(&pbf);

    if(editor_result & COMP_GOTHUP){
	hup_signal();
    }
    else{
	fix_windsize(ps_global);
	init_signals();
    }

    if(editor_result & COMP_CANCEL){
	cmd_cancelled(func);
    }
    else if(editor_result & COMP_EXIT){
	servpart[0] = pathpart[0] = new_cntxt[0] = allbutnick[0] = '\0';
	if(serv && *serv){
	    if(serv[0] == '{'  && serv[strlen(serv)-1] == '}'){
		strncpy(servpart, serv, sizeof(servpart)-1);
		servpart[sizeof(servpart)-1] = '\0';
	    }
	    else
	      snprintf(servpart, sizeof(servpart), "{%s}", serv);

	    if(mail_valid_net_parse(servpart, &mb)){
		if(!struncmp(mb.service, "nntp", 4)
		   && (!path || strncmp(path, "#news.", 6)))
		  strncat(servpart, "#news.", sizeof(servpart)-1-strlen(servpart));
	    }
	    else
	      panic("Unexpected invalid server");
	}
	else
	  servpart[0] = '\0';

	servpart[sizeof(servpart)-1] = '\0';

	new_cntxt[0] = '\0';
	if(nick && *nick){
	    val = quote_if_needed(nick);
	    if(val){
		strncpy(new_cntxt, val, sizeof(new_cntxt)-2);
		new_cntxt[sizeof(new_cntxt)-2] = '\0';
		if(val != nick)
		  fs_give((void **)&val);
	    
		strncat(new_cntxt, " ", sizeof(new_cntxt)-strlen(new_cntxt)-1);
		new_cntxt[sizeof(new_cntxt)-1] = '\0';
	    }
	}

	p = allbutnick;
	sstrncpy(&p, servpart, sizeof(allbutnick)-1-(p-allbutnick));
	allbutnick[sizeof(allbutnick)-1] = '\0';

	if(path){
	    val = quote_brackets_if_needed(path);
	    if(val){
		strncpy(pathpart, val, sizeof(pathpart)-1);
		pathpart[sizeof(pathpart)-1] = '\0';
		if(val != path)
		  fs_give((void **)&val);
	    }

	    if(pbf.headents[AC_PATH].bldr_private != (void *) 0){
		strncat(pathpart, (char *) pbf.headents[AC_PATH].bldr_private,
			sizeof(pathpart)-strlen(pathpart)-1);
		pathpart[sizeof(pathpart)-1] = '\0';
	    }
	}

	sstrncpy(&p, pathpart, sizeof(allbutnick)-1-(p-allbutnick));
	allbutnick[sizeof(allbutnick)-1] = '\0';

	if(view[0] != '[' && sizeof(allbutnick)-1-(p-allbutnick) > 0){
	    *p++ = '[';
	    *p = '\0';
	}

	sstrncpy(&p, view, sizeof(allbutnick)-1-(p-allbutnick));
	allbutnick[sizeof(allbutnick)-1] = '\0';
	if((j=strlen(view)) < 2 || (view[j-1] != ']' &&
	   sizeof(allbutnick)-1-(p-allbutnick) > 0)){
	    *p++ = ']';
	    *p = '\0';
	}

	val = quote_if_needed(allbutnick);
	if(val){
	    strncat(new_cntxt, val, sizeof(new_cntxt)-1-strlen(new_cntxt));
	    new_cntxt[sizeof(new_cntxt)-1] = '\0';

	    if(val != allbutnick)
	      fs_give((void **)&val);
	}

	return_cntxt = cpystr(new_cntxt);
    }

    for(i = 0; headents_templ[i].prompt; i++)
      fs_give((void **) pbf.headents[i].realaddr);

    if(pbf.headents[AC_PATH].bldr_private != (void *) 0)
      fs_give(&pbf.headents[AC_PATH].bldr_private);

    fs_give((void **) &pbf.headents);

    standard_picobuf_teardown(&pbf);

    if(msgso)
      so_give(&msgso);

    return(return_cntxt);
}


/*
 * Doubles up '[' and ']' characters and returns either
 * an allocated string or a pointer to the source string
 * if no quoting is needed.
 */
char *
quote_brackets_if_needed(char *src)
{
    char *step1 = NULL, *step2 = NULL, *ret;

    ret = src;

    if((strpbrk(src, "[]") != NULL)
       && ((step1 = add_escapes(src, "[", '[', "", "")) != NULL)
       && ((step2 = add_escapes(step1, "]", ']', "", ""))))
      ret = step2;

    if(step1)
      fs_give((void **) &step1);

    return(ret);
}


/*
 *  Call back for pico to prompt the user for exit confirmation
 *
 * Returns: either NULL if the user accepts exit, or string containing
 *	 reason why the user declined.
 */      
int
exit_collection_add(struct headerentry *he, void (*redraw_pico)(void), int allow_flowed,
		    char **result)
{
    char     prompt[256], tmp[MAILTMPLEN], tmpnodel[MAILTMPLEN], *server, *path,
	     delim = '\0', *rstr = NULL, *p;
    int	     exists = 0, i;
    void   (*redraw)(void) = ps_global->redrawer;
    NETMBX   mb;

    ps_global->redrawer = redraw_pico;
    fix_windsize(ps_global);

    server = *he[AC_SERV].realaddr;
    removing_trailing_white_space(server);
    removing_leading_white_space(server);

    path = *he[AC_PATH].realaddr;

    if(*server){
	/* No brackets? */
	if(server[0] == '{'  && server[strlen(server)-1] == '}'){
	    strncpy(tmp, server, sizeof(tmp)-1);
	    tmp[sizeof(tmp)-1] = '\0';
	}
	else				/* add them */
	  snprintf(tmp, sizeof(tmp), "{%.*s}", sizeof(tmp)-3, server);

	if(mail_valid_net_parse(tmp, &mb)){ /* news? verify namespace */
	    if(!struncmp(mb.service, "nntp", 4) && strncmp(path, "#news.", 6))
	      strncat(tmp, "#news.", sizeof(tmp)-1-strlen(tmp));
	}
	else
	  rstr = "Invalid Server entry";
    }
    else
      tmp[0] = '\0';

    tmp[sizeof(tmp)-1] = '\0';

    if(!rstr){
	/*
	 * Delimiter acquisition below serves dual purposes.  One's to
	 * get the delimiter so we can make sure it's hanging off the end
	 * of the path so we can test for the directory existence.  The
	 * second's to make sure the server (and any requested service)
	 * name we were given exists.  It should be handled by the folder
	 * existence test futher below, but it doesn't work with news...
	 *
	 * Update. Now we are stripping the delimiter in the tmpnodel version
	 * so that we can pass that to folder_exists. Cyrus does not answer
	 * that the folder exists if we leave the trailing delimiter.
	 * Hubert 2004-12-17
	 */
	strncat(tmp, path, sizeof(tmp)-1-strlen(tmp));
	tmp[sizeof(tmp)-1] = '\0';
	strncpy(tmpnodel, tmp, sizeof(tmpnodel)-1);
	tmpnodel[sizeof(tmpnodel)-1] = '\0';

	if(he[AC_PATH].bldr_private != (void *) 0)
	  fs_give(&he[AC_PATH].bldr_private);

	ps_global->mm_log_error = 0;
	if((delim = folder_delimiter(tmp)) != '\0'){
	    if(*path){
		if(tmp[(i = strlen(tmp)) - 1] == delim)
		  tmpnodel[i-1] = '\0';
		else{
		    tmp[i]   = delim;
		    tmp[i+1] = '\0';
		    he[AC_PATH].bldr_private = (void *) cpystr(&tmp[i]);
		}
	    }
	}
	else if(ps_global->mm_log_error && ps_global->last_error)
	  /* We used to bail, but this was changed with 4.10
	   * as some users wanted to add clctn before the server
	   * was actually around and built.
	   */
	  flush_status_messages(0);	/* mail_create gripes */
	else
	  dprint((1, "exit_col_test: No Server Hierarchy!\n"));
    }

    if(!rstr){
	if(!*tmp
	   || !delim
	   || ((*(p = tmp) == '#'
		|| (*tmp == '{' && (p = strchr(tmp, '}')) && *++p))
	       && !struncmp(p, "#news.", 6))
	   || (*tmp == '{' && (p = strchr(tmp, '}')) && !*++p)){
	    exists = 1;
	}
	else if((i = folder_exists(NULL, tmpnodel)) & FEX_ERROR){
	    if(!(rstr = ps_global->last_error))
	      rstr = _("Problem testing for directory existence");
	}
	else
	  exists = (i & FEX_ISDIR);

	if(exists)
	  snprintf(prompt, sizeof(prompt), _("Exit and save changes"));
	else
	  snprintf(prompt, sizeof(prompt), _("Exit, saving changes and creating Path"));

	if(want_to(prompt, 'y', 0, NO_HELP, WT_NORM) == 'y'){
	    if(!exists && !pine_mail_create(NULL, tmp)){
		flush_status_messages(1);	/* mail_create gripes */
		if(!(rstr = ps_global->last_error))
		  rstr = "";
	    }
	}
	else
	  rstr = _("Use ^C to abandon changes you've made");
    }

    if(result)
      *result = rstr;

    ps_global->redrawer = redraw;
    return((rstr == NULL) ? 0 : 1);
}


char *
cancel_collection_add(void (*redraw_pico)(void))
{
    return(cancel_collection_editing(_("Add"), redraw_pico));
}


char *
cancel_collection_edit(void (*redraw_pico)(void))
{
    return(cancel_collection_editing(_("Edit"), redraw_pico));
}


char *
cancel_collection_editing(char *func, void (*redraw_pico)(void))
{
    char *rstr = NULL;
    void (*redraw)(void) = ps_global->redrawer;
    static char rbuf[20];
    char prompt[256];
#define	CCA_PROMPT	\
		_("Cancel Add (answering \"Yes\" will abandon any changes made) ")

    snprintf(prompt, sizeof(prompt), _("Cancel %s (answering \"Yes\" will abandon any changes made) "), func ? func : "Add");
    snprintf(rbuf, sizeof(rbuf), _("%s Cancelled) "), func ? func : "Add");

    ps_global->redrawer = redraw_pico;
    fix_windsize(ps_global);

    switch(want_to(prompt, 'y', 'x', NO_HELP, WT_NORM)){
      case 'y':
	rstr = rbuf;
	break;

      case 'n':
      case 'x':
	break;
    }

    ps_global->redrawer = redraw;
    return(rstr);
}


int
build_namespace(char *server, char **server_too, char **error, BUILDER_ARG *barg, int *mangled)
{
    char	 *p, *name;
    int		  we_cancel = 0;
    MAILSTREAM	 *stream;
    NAMESPACE  ***namespace;
    size_t        len;

    dprint((5, "- build_namespace - (%s)\n",
	       server ? server : "nul"));

    if(*barg->me){		/* only call this once! */
	if(server_too)
	  *server_too = cpystr(server ? server : "");

	return(0);
    }
    else
      *barg->me = (void *) 1;

    if((p = server) != NULL)		/* empty string? */
      while(*p && isspace((unsigned char) *p))
	p++;

    if(p && *p){
	if(server_too)
	  *server_too = cpystr(p);

	len = strlen(p) + 2;
	name = (char *) fs_get((len + 1) * sizeof(char));
	snprintf(name, len+1, "{%s}", p);
    }
    else{
	if(server_too)
	  *server_too = cpystr("");

	return(0);
    }

    *mangled |= BUILDER_SCREEN_MANGLED;
    fix_windsize(ps_global);
    init_sigwinch();
    clear_cursor_pos();

    we_cancel = busy_cue(_("Fetching default directory"), NULL, 1);

    if((stream = pine_mail_open(NULL, name,
			   OP_HALFOPEN | OP_SILENT | SP_USEPOOL | SP_TEMPUSE,
			       NULL)) != NULL){
	if((namespace = mail_parameters(stream, GET_NAMESPACE, NULL))
	   && *namespace && (*namespace)[0]
	   && (*namespace)[0]->name && (*namespace)[0]->name[0]){
	    if(barg->tptr)
	      fs_give((void **)&barg->tptr);

	    barg->tptr = cpystr((*namespace)[0]->name);
	}

	pine_mail_close(stream);
    }

    if(we_cancel)
      cancel_busy_cue(-1);

    fs_give((void **) &name);

    return(1);
}


int
fl_val_gen (FOLDER_S *f, FSTATE_S *fs)
{
    return(f && FLDR_NAME(f));
}


int
fl_val_writable (FOLDER_S *f, FSTATE_S *fs)
{
    return(1);
}


int
fl_val_subscribe (FOLDER_S *f, FSTATE_S *fs)
{
    if(f->subscribed){
	q_status_message1(SM_ORDER, 0, 4, _("Already subscribed to \"%s\""),
			  FLDR_NAME(f));
	return(0);
    }

    return(1);
}


/*----------------------------------------------------------------------
  Business end of displaying and operating on a collection of folders

  Args:  ps           -- The pine_state data structure
	 fs	      -- Folder screen state structure

  Result: A string list containing folders selected,
	  NULL on Cancel, Error or other problem

  ----*/
STRLIST_S *
folder_lister(struct pine *ps, FSTATE_S *fs)
{
    int		fltrv, we_cancel = 0;
    int         first_time_through = 1;
    HANDLE_S   *handles = NULL;
    STORE_S    *screen_text = NULL;
    FPROC_S	folder_proc_data;
    gf_io_t	pc;

    dprint((1, "\n\n    ---- FOLDER LISTER ----\n"));

    memset(&folder_proc_data, 0, sizeof(FPROC_S));
    folder_proc_data.fs = fs;

    while(!folder_proc_data.done){
	if((screen_text = so_get(CharStar, NULL, EDIT_ACCESS)) != NULL){
	    gf_set_so_writec(&pc, screen_text);
	}
	else{
	    q_status_message(SM_ORDER | SM_DING, 3, 3,
			     "Formatting Error: Can't create space for list");
	    return(NULL);
	}

	we_cancel = busy_cue(_("Fetching folder data"), NULL, 1);
	fltrv = folder_list_text(ps, &folder_proc_data, pc, &handles, 
			    ps->ttyo->screen_cols);
	if(we_cancel)
	  cancel_busy_cue(-1);

	if(fltrv){

	    SCROLL_S	    sargs;
	    struct key_menu km;
	    struct key	    keys[36];

	    memset(&sargs, 0, sizeof(SCROLL_S));
	    sargs.text.text = so_text(screen_text);
	    sargs.text.src  = CharStar;
	    sargs.text.desc = "folder list";
	    if((sargs.text.handles = folder_list_handle(fs, handles)) != NULL)
	      sargs.start.on = Handle;

	    sargs.bar.title    = fs->f.title.bar;
	    sargs.bar.style    = fs->f.title.style;

	    sargs.proc.tool	   = folder_processor;
	    sargs.proc.data.p	   = (void *) &folder_proc_data;

	    sargs.quell_first_view = first_time_through ? 0 : 1;
	    first_time_through = 0;

	    sargs.resize_exit  = 1;
	    sargs.vert_handle  = 1;
	    sargs.srch_handle  = 1;

	    sargs.help.text    = fs->f.help.text;
	    sargs.help.title   = fs->f.help.title;

	    sargs.keys.menu    = &km;
	    km		       = *fs->km;
	    km.keys	       = keys;
	    memcpy(&keys[0], fs->km->keys,
		   (km.how_many * 12) * sizeof(struct key));
	    setbitmap(sargs.keys.bitmap);

	    if(fs->km == &folder_km){
		sargs.keys.each_cmd = folder_lister_km_manager;
#ifdef	_WINDOWS
		sargs.mouse.popup      = folder_list_popup;
#endif
	    }
	    else{
#ifdef	_WINDOWS
		sargs.mouse.popup = folder_list_select_popup;
#endif
		if(fs->km == &folder_sel_km || fs->km == &folder_sela_km)
		  sargs.keys.each_cmd = folder_lister_km_sel_manager;
		else if(fs->km == &folder_sub_km)
		  sargs.keys.each_cmd = folder_lister_km_sub_manager;
	    }

	    sargs.mouse.clickclick = folder_lister_clickclick;

	    switch(scrolltool(&sargs)){
	      case MC_MAIN :		/* just leave */
		folder_proc_data.done = 1;
		break;

	      case MC_RESIZE :		/* loop around rebuilding screen */
		if(sargs.text.handles){
		    FOLDER_S *fp;

		    fp = folder_entry(sargs.text.handles->h.f.index,
				    FOLDERS(sargs.text.handles->h.f.context));
		    if(fp && strlen(FLDR_NAME(fp)) < MAXFOLDER -1){
			strncpy(fs->first_folder, FLDR_NAME(fp), MAXFOLDER);
			fs->first_folder[MAXFOLDER-1] = '\0';
		    }

		    fs->context = sargs.text.handles->h.f.context;
		}

		break;

		/*--------- EXIT menu -----------*/
	      case MC_EXIT :
	      case MC_EXITQUERY :
		fs->list_cntxt = NULL;
		folder_proc_data.done = folder_proc_data.all_done = 1;
		break;

	      default :
		break;
	    }

	    ps->noticed_change_in_unseen = 0;


	    if(F_ON(F_BLANK_KEYMENU,ps))
	      FOOTER_ROWS(ps) = 1;

	    gf_clear_so_writec(screen_text);
	    so_give(&screen_text);
	    free_handles(&handles);
	}
	else
	  folder_proc_data.done = 1;
    }

    reset_context_folders(fs->context);

    if(folder_proc_data.all_done)
      fs->context = NULL;

    if(fs->cache_streamp && *fs->cache_streamp){
	int i;

	/* 
	 * check stream pool to see if currently cached
	 * stream went away
	 */
	for(i = 0; i < ps->s_pool.nstream; i++)
	  if(ps->s_pool.streams[i] == *fs->cache_streamp)
	    break;
	if(i == ps->s_pool.nstream)
	  *fs->cache_streamp = NULL;
    }

    return(folder_proc_data.rv);
}


/*
 * folder_list_text - format collection's contents for display
 */
int
folder_list_text(struct pine *ps, FPROC_S *fp, gf_io_t pc, HANDLE_S **handlesp, int cols)
{
    int	       rv = 1, i, j, ftotal, fcount, slot_width, slot_rows,
	       slot_cols, index, findex, width, shown, selected;
    CONTEXT_S *c_list;
    char       lbuf[6*MAX_SCREEN_COLS+1];

    /* disarm this trigger that gets us out of scrolltool */
    ps->noticed_change_in_unseen = 0;

    if(handlesp)
      init_handles(handlesp);

    c_list = fp->fs->context;
    if(fp->fs->combined_view
       && (F_ON(F_CMBND_SUBDIR_DISP, ps_global) || !c_list->dir->prev))
      while(c_list->prev)		/* rewind to start */
	c_list = c_list->prev;

    do{
	ps->user_says_cancel = 0;

	/* If we're displaying folders, fetch the list */
	if((shown = (c_list == fp->fs->context
		     || (c_list->dir->status & CNTXT_NOFIND) == 0
		     || F_ON(F_EXPANDED_FOLDERS, ps_global))) != 0){
	    /*
	     * if select is allowed, flag context so any that are
	     * are remembered even after the list is destroyed
	     */
	    if(fp->fs->agg_ops)
	      c_list->use |= CNTXT_PRESRV;

	    /* Make sure folder list filled in */
	    refresh_folder_list(c_list, fp->fs->no_dirs, FALSE, fp->fs->cache_streamp);
	}

	/* Insert any introductory text here */
	if(c_list->next
	   || c_list->prev
	   || c_list->dir->prev
	   || fp->fs->force_intro){

	    /* Leading horizontal line? */
	    if(fp->fs->combined_view
	       && (F_ON(F_CMBND_SUBDIR_DISP,ps_global)
		   || !c_list->dir->prev)){
		if(c_list->prev)
		  gf_puts("\n", pc);		/* blank line */

		gf_puts(repeat_char(cols, '-'), pc);
		gf_puts("\n", pc);
	    }

	    /* nickname or description */
	    if(F_ON(F_CMBND_FOLDER_DISP, ps_global)
	       && (!c_list->dir->prev
		   || F_ON(F_CMBND_SUBDIR_DISP, ps_global))){
		char buf[6*MAX_SCREEN_COLS + 1];

		if(cols < 40){
		  snprintf(buf, sizeof(buf), "%.*s", cols,
			strsquish(tmp_20k_buf, SIZEOF_20KBUF,
				  (c_list->nickname)
				    ? c_list->nickname
				    : (c_list->label ? c_list->label : ""),
				  cols));
		  gf_puts(folder_list_center_space(buf, cols), pc);
		}
		else{
		  int  wid;

		  snprintf(buf, sizeof(buf), "%s-Collection <",
			NEWS_TEST(c_list) ? "News" : "Folder");
		  wid = utf8_width(buf)+1;
		  wid = MAX(0, cols-wid);
		  snprintf(buf+strlen(buf), sizeof(buf)-strlen(buf), "%.*s>", wid,
			strsquish(tmp_20k_buf, SIZEOF_20KBUF,
				  (c_list->nickname)
				    ? c_list->nickname
				    : (c_list->label ? c_list->label : ""),
				  wid));
		}

		gf_puts(buf, pc);
		gf_puts("\n", pc);
	    }
	    else if(c_list->label){
		if(utf8_width(c_list->label) > ps_global->ttyo->screen_cols)
		  utf8_pad_to_width(lbuf, c_list->label, sizeof(lbuf), ps_global->ttyo->screen_cols, 1);
		else
		  strncpy(lbuf, c_list->label, sizeof(lbuf));

		lbuf[sizeof(lbuf)-1] = '\0';

		gf_puts(folder_list_center_space(lbuf, cols), pc);
		gf_puts(lbuf, pc);
		gf_puts("\n", pc);
	    }

	    if(c_list->comment){
		if(utf8_width(c_list->comment) > ps_global->ttyo->screen_cols)
		  utf8_pad_to_width(lbuf, c_list->comment, sizeof(lbuf), ps_global->ttyo->screen_cols, 1);
		else
		  strncpy(lbuf, c_list->comment, sizeof(lbuf));

		lbuf[sizeof(lbuf)-1] = '\0';

		gf_puts(folder_list_center_space(lbuf, cols), pc);
		gf_puts(lbuf, pc);
		gf_puts("\n", pc);
	    }

	    if(c_list->dir->desc){
		char buf[6*MAX_SCREEN_COLS + 1];

		strncpy(buf, strsquish(tmp_20k_buf,SIZEOF_20KBUF,c_list->dir->desc,cols),
			sizeof(buf)-1);
		buf[sizeof(buf)-1] = '\0';
		gf_puts(folder_list_center_space(buf, cols), pc);
		gf_puts(buf, pc);
		gf_puts("\n", pc);
	    }

	    if(c_list->use & CNTXT_ZOOM){
		snprintf(tmp_20k_buf, SIZEOF_20KBUF, "[ ZOOMED on %d (of %d) %ss ]",
			selected_folders(c_list),
			folder_total(FOLDERS(c_list)),
			(c_list->use & CNTXT_NEWS) ? "Newsgroup" : "Folder");
		tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';

		if(utf8_width(tmp_20k_buf) > ps_global->ttyo->screen_cols)
		  utf8_pad_to_width(lbuf, tmp_20k_buf, sizeof(lbuf), ps_global->ttyo->screen_cols, 1);
		else
		  strncpy(lbuf, tmp_20k_buf, sizeof(lbuf));

		lbuf[sizeof(lbuf)-1] = '\0';

		gf_puts(folder_list_center_space(lbuf, cols), pc);
		gf_puts(lbuf, pc);
		gf_puts("\n", pc);
	    }

	    gf_puts(repeat_char(cols, '-'), pc);
	    gf_puts("\n\n", pc);
	}

	if(shown){
	    /* Run thru list formatting as necessary */
	    if((ftotal = folder_total(FOLDERS(c_list))) != 0){
		/* If previously selected, mark members of new list */
		selected = selected_folders(c_list);

		/* How many screen cells per cell for each folder name? */
		slot_width = 1;
		for(fcount = i = 0; i < ftotal; i++){
		    FOLDER_S *f = folder_entry(i, FOLDERS(c_list));

		    ps->user_says_cancel = 0;

		    if((c_list->use & CNTXT_ZOOM) && !f->selected)
		      continue;

		    fcount++;

		    width = utf8_width(FLDR_NAME(f));
		    if(f->isdir)
		      width += (f->isfolder) ? 3 : 1;

		    if(NEWS_TEST(c_list) && (c_list->use & CNTXT_FINDALL))
		      /* assume listmode so we don't have to reformat */
		      /* if listmode is actually entered */
		      width += 4;    

		    if(selected){
			if(F_OFF(F_SELECTED_SHOWN_BOLD, ps_global))
			  width += 4;		/* " X  " */
		    }
		    else if(c_list == fp->fs->list_cntxt)
		      width += 4;			/* "[X] " */

		    if(fp->fs->include_unseen_cnt
		       && c_list->use & CNTXT_INCMNG
		       && F_ON(F_ENABLE_INCOMING_CHECKING, ps_global)
		       && ps_global->VAR_INCOMING_FOLDERS
		       && ps_global->VAR_INCOMING_FOLDERS[0]){

			update_folder_unseen(f, c_list, UFU_ANNOUNCE, NULL);
			if(F_ON(F_INCOMING_CHECKING_RECENT, ps_global)
			   && f->unseen_valid
			   && (f->new > 0L
			       || F_ON(F_INCOMING_CHECKING_TOTAL, ps_global))){
			    width += (strlen(tose(f->new)) + 3);
			    if(F_ON(F_INCOMING_CHECKING_TOTAL, ps_global))
			      width += (strlen(tose(f->total)) + 1);
			}
			else if(F_OFF(F_INCOMING_CHECKING_RECENT, ps_global)
			   && f->unseen_valid
			   && (f->unseen > 0L
			       || F_ON(F_INCOMING_CHECKING_TOTAL, ps_global))){
			    width += (strlen(tose(f->unseen)) + 3);
			    if(F_ON(F_INCOMING_CHECKING_TOTAL, ps_global))
			      width += (strlen(tose(f->total)) + 1);
			}
			else if(!f->unseen_valid && f->last_unseen_update != LUU_NEVERCHK)
			  width += 4;			/* " (?)" */
		    }

		    if(slot_width < width)
		      slot_width = width;
		}

		if(F_ON(F_SINGLE_FOLDER_LIST, ps_global)){
		    slot_cols = 1;
		    slot_rows = fcount;
		}
		else{
		    /* fit as many columns as possible */
		    slot_cols = 1;
		    while(((slot_cols+1) * slot_width) + slot_cols <= cols)
		      slot_cols++;

		    switch(slot_cols){
		      case 0 :
			slot_cols = 1;
			/* fall through */

		      case 1 :
			slot_rows = fcount;
			break;

		      default :
			/*
			 * Make the slot_width as large as possible.
			 * Slot_width is the width of the column, not counting
			 * the space between columns.
			 */
			while((slot_cols * (slot_width+1)) + slot_cols-1 <= cols)
			  slot_width++;

			slot_rows = (fcount / slot_cols)
					      + ((fcount % slot_cols) ? 1 : 0);
			break;
		    }
		}

		for(i = index = 0; i < slot_rows; i++){
		    if(i)
		      gf_puts("\n", pc);

		    for(j = width = 0; j < slot_cols; j++, index++){
			if(width){
			    gf_puts(repeat_char(slot_width + 1 - width, ' '), pc);
			    width = 0;
			}

			if(F_ON(F_VERTICAL_FOLDER_LIST, ps_global))
			  index = i + (j * slot_rows);

			findex = index;

			if(c_list->use & CNTXT_ZOOM)
			  findex = folder_list_ith(index, c_list);

			if(findex < ftotal){
			    int flags = (handlesp) ? FLW_LUNK : FLW_NONE;

			    if(c_list == fp->fs->list_cntxt)
			      flags |= FLW_LIST;
			    else if(selected)
			      flags |= FLW_SLCT;
			    else if(F_ON(F_SINGLE_FOLDER_LIST, ps_global)
				    || ((c_list->use & CNTXT_FINDALL)
					&& NEWS_TEST(c_list)))
			      gf_puts("    ", pc);

			    if(fp->fs->include_unseen_cnt
			       && c_list->use & CNTXT_INCMNG
			       && F_ON(F_ENABLE_INCOMING_CHECKING, ps_global)
			       && ps_global->VAR_INCOMING_FOLDERS
			       && ps_global->VAR_INCOMING_FOLDERS[0])
			      flags |= FLW_UNSEEN;

			    width = folder_list_write(pc, handlesp, c_list,
						    findex, NULL, flags);
			}
		    }
		}
	    }
	    else if(fp->fs->combined_view
		    && (F_ON(F_CMBND_SUBDIR_DISP, ps_global)
			|| !c_list->dir->prev)){
		char *emptiness = N_("[No Folders in Collection]");

		if(utf8_width(_(emptiness)) > ps_global->ttyo->screen_cols)
		  utf8_pad_to_width(lbuf, _(emptiness), sizeof(lbuf), ps_global->ttyo->screen_cols, 1);
		else
		  strncpy(lbuf, _(emptiness), sizeof(lbuf));

		lbuf[sizeof(lbuf)-1] = '\0';

		gf_puts(folder_list_center_space(lbuf, cols), pc);
		(void) folder_list_write(pc, handlesp, c_list, -1, lbuf,
					(handlesp) ? FLW_LUNK : FLW_NONE);
	    }
	}
	else if(fp->fs->combined_view
		&& (F_ON(F_CMBND_SUBDIR_DISP, ps_global)
		    || !c_list->dir->prev)){
	    char *unexpanded = N_("[Select Here to See Expanded List]");

	    if(utf8_width(_(unexpanded)) > ps_global->ttyo->screen_cols)
	      utf8_pad_to_width(lbuf, _(unexpanded), sizeof(lbuf), ps_global->ttyo->screen_cols, 1);
	    else
	      strncpy(lbuf, _(unexpanded), sizeof(lbuf));

	    lbuf[sizeof(lbuf)-1] = '\0';

	    gf_puts(folder_list_center_space(lbuf, cols), pc);
	    (void) folder_list_write(pc, handlesp, c_list, -1, lbuf,
				    (handlesp) ? FLW_LUNK : FLW_NONE);
	}

	gf_puts("\n", pc);			/* blank line */

    }
    while(fp->fs->combined_view
	  && (F_ON(F_CMBND_SUBDIR_DISP, ps_global) || !c_list->dir->prev)
	  && (c_list = c_list->next));

    return(rv);
}


int
folder_list_write(gf_io_t pc, HANDLE_S **handlesp, CONTEXT_S *ctxt, int fnum, char *alt_name, int flags)
{
    char      buf[256];
    int	      width = 0, lprefix = 0, lmiddle = 0, lsuffix = 0;
    FOLDER_S *fp;
    HANDLE_S *h1 = NULL, *h2 = NULL;

    if(flags & FLW_LUNK){
	h1		 = new_handle(handlesp);
	h1->type	 = Folder;
	h1->h.f.index	 = fnum;
	h1->h.f.context	 = ctxt;
	h1->force_display = 1;

	snprintf(buf, sizeof(buf), "%d", h1->key);
	buf[sizeof(buf)-1] = '\0';
    }

    fp = (fnum < 0) ? NULL : folder_entry(fnum, FOLDERS(ctxt));

    if(flags & FLW_LUNK && h1 && fp && fp->isdir && fp->isfolder){
	h2		 = new_handle(handlesp);
	h2->type	 = Folder;
	h2->h.f.index	 = fnum;
	h2->h.f.context	 = ctxt;
	h2->force_display = 1;

	h1->is_dual_do_open = 1;
    }

    if(h1){
	/* color unseen? */
	if(color_monitored_unseen(fp, flags)){
	    h1->color_unseen = 1;
	    if(h2)
	      h2->color_unseen = 1;
	}
    }

    /* embed handle pointer */
    if(((h1 && h1->color_unseen) ?
		gf_puts(color_embed(ps_global->VAR_INCUNSEEN_FORE_COLOR,
				    ps_global->VAR_INCUNSEEN_BACK_COLOR), pc) : 1)
       && (h1 ? ((*pc)(TAG_EMBED) && (*pc)(TAG_HANDLE)
	     && (*pc)(strlen(buf)) && gf_puts(buf, pc)) : 1)
       && (fp ? ((lprefix = folder_list_write_prefix(fp, flags, pc)) >= 0
		 && (lmiddle = folder_list_write_middle(fp, ctxt, pc, h2)) >= 0
		 && ((lsuffix = folder_list_write_suffix(fp, flags, pc)) >= 0))
	      : (alt_name ? gf_puts(alt_name, pc) : 0))
       && (h1 ? ((*pc)(TAG_EMBED) && (*pc)(TAG_BOLDOFF)
		&& (*pc)(TAG_EMBED) && (*pc)(TAG_INVOFF)) : 1)
       && ((h1 && h1->color_unseen) ?
	       gf_puts(color_embed(ps_global->VAR_NORM_FORE_COLOR,
				   ps_global->VAR_NORM_BACK_COLOR), pc) : 1)){
	if(fp)
	  width = lprefix + lmiddle + lsuffix;
	else if(alt_name)
	  width = utf8_width(alt_name);
    }

    return(width);
}


int
folder_list_write_prefix(FOLDER_S *f, int flags, gf_io_t pc)
{
    int rv = 0;

    if(flags & FLW_SLCT){
	if(F_OFF(F_SELECTED_SHOWN_BOLD, ps_global) || !(flags & FLW_LUNK)){
	    rv = 4;
	    if(f->selected){
		gf_puts(" X  ", pc);
	    }
	    else{
		gf_puts("    ", pc);
	    }
	}
	else
	    rv = ((*pc)(TAG_EMBED) 
		  && (*pc)((f->selected) ? TAG_BOLDON : TAG_BOLDOFF)) ? 0 : -1;
    }
    else if(flags & FLW_LIST){
	rv = 4;
	/* screen width of "SUB " is 4 */
	gf_puts(f->subscribed ? "SUB " : (f->selected ? "[X] " : "[ ] "), pc);
    }

    return(rv);
}


int
folder_list_write_middle(FOLDER_S *fp, CONTEXT_S *ctxt, gf_io_t pc, HANDLE_S *h2)
{
    int rv = -1;
    char buf[256];

    if(h2){
	snprintf(buf, sizeof(buf), "%d", h2->key);
	buf[sizeof(buf)-1] = '\0';
    }

    if(!fp)
      return(rv);

    if(gf_puts(FLDR_NAME(fp), pc)
       && (h2 ? ((*pc)(TAG_EMBED) && (*pc)(TAG_BOLDOFF)		/* tie off handle 1 */
		&& (*pc)(TAG_EMBED) && (*pc)(TAG_INVOFF)) : 1)
       && (h2 ? ((*pc)(TAG_EMBED) && (*pc)(TAG_HANDLE)		/* start handle 2 */
	     && (*pc)(strlen(buf)) && gf_puts(buf, pc)) : 1)
       && ((fp->isdir && fp->isfolder) ? (*pc)('[') : 1)
       && ((fp->isdir) ? (*pc)(ctxt->dir->delim) : 1)
       && ((fp->isdir && fp->isfolder) ? (*pc)(']') : 1)){
	rv = utf8_width(FLDR_NAME(fp));
	if(fp->isdir)
	  rv += (fp->isfolder) ? 3 : 1;
    }
	
    return(rv);
}


int
folder_list_write_suffix(FOLDER_S *f, int flags, gf_io_t pc)
{
    int rv = 0;

    if(flags & FLW_UNSEEN){
	char buf[100];

	buf[0] = '\0';
	if(F_ON(F_INCOMING_CHECKING_RECENT, ps_global)
	   && f->unseen_valid
	   && (f->new > 0L
	       || (F_ON(F_INCOMING_CHECKING_TOTAL, ps_global)
	           && f->total > 0L))){
	    snprintf(buf, sizeof(buf), " (%s%s%s)",
		     tose(f->new),
		     F_ON(F_INCOMING_CHECKING_TOTAL, ps_global) ? "/" : "",
		     F_ON(F_INCOMING_CHECKING_TOTAL, ps_global) ? tose(f->total) : "");
	}
	else if(F_OFF(F_INCOMING_CHECKING_RECENT, ps_global)
	   && f->unseen_valid
	   && (f->unseen > 0L
	       || (F_ON(F_INCOMING_CHECKING_TOTAL, ps_global)
	           && f->total > 0L))){
	    snprintf(buf, sizeof(buf), " (%s%s%s)",
		     tose(f->unseen),
		     F_ON(F_INCOMING_CHECKING_TOTAL, ps_global) ? "/" : "",
		     F_ON(F_INCOMING_CHECKING_TOTAL, ps_global) ? tose(f->total) : "");
	}
	else if(!f->unseen_valid && f->last_unseen_update != LUU_NEVERCHK){
	    snprintf(buf, sizeof(buf), " (?)");
	}

	rv = strlen(buf);
	if(rv)
	  gf_puts(buf, pc);
    }

    return(rv);
}


int
color_monitored_unseen(FOLDER_S *f, int flags)
{
    return((flags & FLW_UNSEEN) && f && f->unseen_valid
	   && ((F_ON(F_INCOMING_CHECKING_RECENT, ps_global) && f->new > 0L)
	       || (F_OFF(F_INCOMING_CHECKING_RECENT, ps_global) && f->unseen > 0L))
           && pico_usingcolor()
	   && pico_is_good_color(ps_global->VAR_INCUNSEEN_FORE_COLOR)
	   && pico_is_good_color(ps_global->VAR_INCUNSEEN_BACK_COLOR)
	   && (colorcmp(ps_global->VAR_INCUNSEEN_FORE_COLOR,
		        ps_global->VAR_NORM_FORE_COLOR)
	       || colorcmp(ps_global->VAR_INCUNSEEN_BACK_COLOR,
			   ps_global->VAR_NORM_BACK_COLOR)));
}


int
folder_list_ith(int n, CONTEXT_S *cntxt)
{
    int	      index, ftotal;
    FOLDER_S *f;

    for(index = 0, ftotal = folder_total(FOLDERS(cntxt));
	index < ftotal
	&& (f = folder_entry(index, FOLDERS(cntxt)))
	&& !(f->selected && !n--);
	index++)
      ;
      
    return(index);
}


char *
folder_list_center_space(char *s, int width)
{
    int l;

    return(((l = utf8_width(s)) < width) ? repeat_char((width - l)/2, ' ') : "");
}


/*
 * folder_list_handle - return pointer in handle list
 *			corresponding to "start"
 */
HANDLE_S *
folder_list_handle(FSTATE_S *fs, HANDLE_S *handles)
{
    char     *p, *name = NULL;
    HANDLE_S *h, *h_found = NULL;
    FOLDER_S *fp;

    if(handles && fs->context){
	if(!(NEWS_TEST(fs->context) || (fs->context->use & CNTXT_INCMNG))
	   && fs->context->dir->delim)
	  for(p = strchr(fs->first_folder, fs->context->dir->delim);
	      p;
	      p = strchr(p, fs->context->dir->delim))
	    name = ++p;

	for(h = handles; h; h = h->next)
	  if(h->h.f.context == fs->context){
	      if(!h_found)		/* match at least given context */
		h_found = h;

	      if(!fs->first_folder[0]
		 || ((fp = folder_entry(h->h.f.index, FOLDERS(h->h.f.context)))
		     && ((fs->first_dir && fp->isdir)
			 || (!fs->first_dir && fp->isfolder))
		     && ((fp->nickname && !strcmp(name ? name : fs->first_folder, fp->nickname))
		         || (fp->name && !strcmp(name ? name : fs->first_folder, fp->name))))){
		  h_found = h;
		  break;
	      }
	  }

	fs->first_folder[0] = '\0';
    }

    return(h_found ? h_found : handles);
}


int
folder_processor(int cmd, MSGNO_S *msgmap, SCROLL_S *sparms)
{
    int	      rv = 0;
    char      tmp_output[MAILTMPLEN];

    switch(cmd){
      case MC_FINISH :
	FPROC(sparms)->done = rv = 1;;
	break;

            /*---------- Select or enter a View ----------*/
      case MC_CHOICE :
	rv = folder_lister_choice(sparms);
	break;

            /*--------- Hidden "To Fldrs" command -----------*/
      case MC_LISTMODE :
	if(sparms->text.handles
	   && folder_total(FOLDERS(sparms->text.handles->h.f.context))){
	    if(!FPROC(sparms)->fs->list_cntxt){
		FPROC(sparms)->fs->list_cntxt
					   = sparms->text.handles->h.f.context;
		rv = scroll_add_listmode(sparms->text.handles->h.f.context,
				 folder_total(FOLDERS(sparms->text.handles->h.f.context))); 
		if(!rv){
		  /* need to set the subscribe key ourselves */
		  sparms->keys.menu->keys[SB_SUB_KEY].name = "S";
		  sparms->keys.menu->keys[SB_SUB_KEY].label = N_("Subscribe");
		  sparms->keys.menu->keys[SB_SUB_KEY].bind.cmd = MC_CHOICE;
		  sparms->keys.menu->keys[SB_SUB_KEY].bind.ch[0] = 's';
		  setbitn(SB_SUB_KEY, sparms->keys.bitmap);
		  ps_global->mangled_screen = 1;
		  
		  sparms->keys.menu->keys[SB_EXIT_KEY].bind.cmd = MC_EXITQUERY;
		}
		q_status_message(SM_ORDER, 0, 1, LISTMODE_GRIPE);
	    }
	    else
	      q_status_message(SM_ORDER, 0, 4, _("Already in List Mode"));
	}
	else
	  q_status_message(SM_ORDER, 0, 4,
			   _("No Folders!  Can't enter List Mode"));

	break;

    
	/*--------- Visit parent directory -----------*/
      case MC_PARENT :
	if(folder_lister_parent(FPROC(sparms)->fs,
				(sparms->text.handles)
				 ? sparms->text.handles->h.f.context
				 : FPROC(sparms)->fs->context,
				(sparms->text.handles)
				 ? sparms->text.handles->h.f.index : -1, 0))
	  rv = 1;		/* leave scrolltool to rebuild screen */
	    
	break;


	/*--------- Open the selected folder -----------*/
      case MC_OPENFLDR :
	if(sparms->text.handles
	   && folder_total(FOLDERS(sparms->text.handles->h.f.context)))
	  rv = folder_lister_finish(sparms, sparms->text.handles->h.f.context,
				    sparms->text.handles->h.f.index);
	else
	  q_status_message(SM_ORDER, 0, 4,
			   _("No Folders!  Nothing to View"));

	break;


	/*--------- Export the selected folder -----------*/
      case MC_EXPORT :
	folder_export(sparms);
	break;


	/*--------- Import the selected folder -----------*/
      case MC_IMPORT :
        {
	    CONTEXT_S *cntxt = (sparms->text.handles)
				 ? sparms->text.handles->h.f.context
				 : FPROC(sparms)->fs->context;
	    char       new_file[2*MAXFOLDER+10];
	    int	       r;

	    new_file[0] = '\0';

	    r = folder_import(sparms, new_file, sizeof(new_file));

	    if(r && (cntxt->use & CNTXT_INCMNG || context_isambig(new_file))){
		rv = 1;			/* rebuild display! */
		FPROC(sparms)->fs->context = cntxt;
		if(strlen(new_file) < MAXFOLDER - 1){
		    strncpy(FPROC(sparms)->fs->first_folder, new_file, MAXFOLDER);
		    FPROC(sparms)->fs->first_folder[MAXFOLDER-1] = '\0';
		}
	    }
	    else
	      ps_global->mangled_footer++;
	}

	break;


	/*--------- Return to the Collections Screen -----------*/
      case MC_COLLECTIONS :
	FPROC(sparms)->done = rv = 1;
	break;


	/*--------- QUIT pine -----------*/
      case MC_QUIT :
	ps_global->next_screen = quit_screen;
	FPROC(sparms)->done = rv = 1;
	break;
	    

            /*--------- Compose -----------*/
      case MC_COMPOSE :
	ps_global->next_screen = compose_screen;
	FPROC(sparms)->done = rv = 1;
	break;
	    

            /*--------- Alt Compose -----------*/
      case MC_ROLE :
	ps_global->next_screen = alt_compose_screen;
	FPROC(sparms)->done = rv = 1;
	break;
	    

            /*--------- Message Index -----------*/
      case MC_INDEX :
	if(THREADING()
	   && sp_viewing_a_thread(ps_global->mail_stream)
	   && unview_thread(ps_global, ps_global->mail_stream, ps_global->msgmap)){
	    ps_global->next_screen = mail_index_screen;
	    ps_global->view_skipped_index = 0;
	    ps_global->mangled_screen = 1;
	}

	ps_global->next_screen = mail_index_screen;
	FPROC(sparms)->done = rv = 1;
	break;


            /*----------------- Add a new folder name -----------*/
      case MC_ADDFLDR :
        {
	    CONTEXT_S *cntxt = (sparms->text.handles)
				 ? sparms->text.handles->h.f.context
				 : FPROC(sparms)->fs->context;
	    char       new_file[2*MAXFOLDER+10];
	    int	       r;

	    if(NEWS_TEST(cntxt))
	      r = group_subscription(new_file, sizeof(new_file), cntxt);
	    else{
	      r = add_new_folder(cntxt, Main, V_INCOMING_FOLDERS, new_file,
				 sizeof(new_file),
				 FPROC(sparms)->fs->cache_streamp
				  ? *FPROC(sparms)->fs->cache_streamp : NULL,
				 NULL);
	      if(ps_global->prc && ps_global->prc->outstanding_pinerc_changes)
	        write_pinerc(ps_global, Main, WRP_NONE);
	    }

	    if(r && (cntxt->use & CNTXT_INCMNG || context_isambig(new_file))){
		rv = 1;			/* rebuild display! */
		FPROC(sparms)->fs->context = cntxt;
		if(strlen(new_file) < MAXFOLDER - 1){
		    strncpy(FPROC(sparms)->fs->first_folder, new_file, MAXFOLDER);
		    FPROC(sparms)->fs->first_folder[MAXFOLDER-1] = '\0';
		}
	    }
	    else
	      ps_global->mangled_footer++;
	}

        break;


            /*------ Type in new folder name, e.g., for save ----*/
      case MC_ADD :
	rv = folder_lister_addmanually(sparms);
        break;


            /*--------------- Rename folder ----------------*/
      case MC_RENAMEFLDR :
	if(sparms->text.handles
	   && folder_total(FOLDERS(sparms->text.handles->h.f.context))){
	    char new_file[MAXFOLDER+1];
	    int  r;

	    r = rename_folder(sparms->text.handles->h.f.context,
			      sparms->text.handles->h.f.index, new_file,
			      sizeof(new_file),
			      FPROC(sparms)->fs->cache_streamp
			      ? *FPROC(sparms)->fs->cache_streamp : NULL);

	    if(r){
		/* repaint, placing cursor on new folder! */
		rv = 1;
		if(context_isambig(new_file)){
		    FPROC(sparms)->fs->context
					   = sparms->text.handles->h.f.context;
		    if(strlen(new_file) < MAXFOLDER - 1){
			strncpy(FPROC(sparms)->fs->first_folder, new_file, MAXFOLDER);
			FPROC(sparms)->fs->first_folder[MAXFOLDER-1] = '\0';
		    }
		}
	    }

	    ps_global->mangled_footer++;
	}
	else
	  q_status_message(SM_ORDER | SM_DING, 0, 4,
			   _("Empty folder collection.  No folder to rename!"));

	break;
		     

            /*-------------- Delete --------------------*/
      case MC_DELETE :
	if(!(sparms->text.handles
		 && folder_total(FOLDERS(sparms->text.handles->h.f.context)))){
	    q_status_message(SM_ORDER | SM_DING, 0, 4,
			     _("Empty folder collection.  No folder to delete!"));
	}
	else{
	    char next_folder[MAILTMPLEN+1];

	    next_folder[0] = '\0';
	    if(delete_folder(sparms->text.handles->h.f.context,
			     sparms->text.handles->h.f.index,
			     next_folder, sizeof(next_folder),
			     FPROC(sparms)->fs->cache_streamp
			     && *FPROC(sparms)->fs->cache_streamp
			     ? FPROC(sparms)->fs->cache_streamp : NULL)){

		/* repaint, placing cursor on adjacent folder! */
		rv = 1;
		if(next_folder[0] && strlen(next_folder) < MAXFOLDER - 1){
		    strncpy(FPROC(sparms)->fs->first_folder, next_folder, MAXFOLDER);
		    FPROC(sparms)->fs->first_folder[MAXFOLDER-1] = '\0';
		}
		else
		  sparms->text.handles->h.f.context->use &= ~CNTXT_ZOOM;
	    }
	    else
	      ps_global->mangled_footer++;
	}

	break;


            /*----------------- Shuffle incoming folder list -----------*/
      case MC_SHUFFLE :
        {
	    CONTEXT_S *cntxt = (sparms->text.handles)
				 ? sparms->text.handles->h.f.context : NULL;

	    if(!(cntxt && cntxt->use & CNTXT_INCMNG))
	      q_status_message(SM_ORDER, 0, 4,
			       _("May only shuffle Incoming-Folders."));
	    else if(folder_total(FOLDERS(cntxt)) == 0)
	      q_status_message(SM_ORDER, 0, 4,
			       _("No folders to shuffle."));
	    else if(folder_total(FOLDERS(cntxt)) < 2)
	      q_status_message(SM_ORDER, 0, 4,
		       _("Shuffle only makes sense with more than one folder."));
	    else{
		if(FPROC(sparms) && FPROC(sparms)->fs &&
		   FPROC(sparms)->fs && sparms->text.handles &&
		   sparms->text.handles->h.f.index >= 0 &&
		   sparms->text.handles->h.f.index <
					       folder_total(FOLDERS(cntxt))){
		    strncpy(FPROC(sparms)->fs->first_folder,
			 FLDR_NAME(folder_entry(sparms->text.handles->h.f.index,
						FOLDERS(cntxt))), MAXFOLDER);
		    FPROC(sparms)->fs->first_folder[MAXFOLDER-1] = '\0';
		}

		rv = shuffle_incoming_folders(cntxt,
					      sparms->text.handles->h.f.index);
	    }
	}

      break;


	/*-------------- Goto Folder Prompt --------------------*/
      case MC_GOTO :
      {
	  int notrealinbox;
	  CONTEXT_S *c = (sparms->text.handles)
			   ? sparms->text.handles->h.f.context
			   : FPROC(sparms)->fs->context;
	  char *new_fold = broach_folder(-FOOTER_ROWS(ps_global), 0, &notrealinbox, &c);

	  if(ps_global && ps_global->ttyo){
	      blank_keymenu(ps_global->ttyo->screen_rows - 2, 0);
	      ps_global->mangled_footer = 1;
	  }

	  if(new_fold && do_broach_folder(new_fold, c, NULL, notrealinbox ? 0L : DB_INBOXWOCNTXT) > 0){
	      ps_global->next_screen = mail_index_screen;
	      FPROC(sparms)->done = rv = 1;
	  }
	  else
	    ps_global->mangled_footer = 1;

	  if((c = ((sparms->text.handles)
		    ? sparms->text.handles->h.f.context
		    : FPROC(sparms)->fs->context))->dir->status & CNTXT_NOFIND)
	    refresh_folder_list(c, FPROC(sparms)->fs->no_dirs, TRUE, FPROC(sparms)->fs->cache_streamp);
      }

      break;


      /*------------- Print list of folders ---------*/
      case MC_PRINTFLDR :
	print_folders(FPROC(sparms));
	ps_global->mangled_footer++;
	break;


	/*----- Select the current folder, or toggle checkbox -----*/
      case MC_SELCUR :
	/*---------- Select set of folders ----------*/
      case MC_SELECT :
	if(sparms->text.handles
	   && folder_total(FOLDERS(sparms->text.handles->h.f.context))){
	    if(cmd == MC_SELCUR){
		rv = folder_select_toggle(sparms->text.handles->h.f.context,
				     sparms->text.handles->h.f.index,
				    (NEWS_TEST(sparms->text.handles->h.f.context) 
				     && FPROC(sparms)->fs->list_cntxt) 
				       ? ng_scroll_edit : folder_select_update);
		   if(!rv) ps_global->mangled_screen = 1;
	    }
	    else
	      switch(folder_select(ps_global,
				   sparms->text.handles->h.f.context,
				   sparms->text.handles->h.f.index)){
		case 1 :
		  rv = 1;		/* rebuild screen */

		case 0 :
		default :
		  ps_global->mangled_screen++;
		  break;
	      }

	    if((sparms->text.handles->h.f.context->use & CNTXT_ZOOM)
	       && !selected_folders(sparms->text.handles->h.f.context)){
		sparms->text.handles->h.f.context->use &= ~CNTXT_ZOOM;
		rv = 1;			/* make sure to redraw */
	    }

	    if(rv){			/* remember where to start */
		FOLDER_S *fp;

		FPROC(sparms)->fs->context = sparms->text.handles->h.f.context;
		if((fp = folder_entry(sparms->text.handles->h.f.index,
				 FOLDERS(sparms->text.handles->h.f.context))) != NULL){
		    if(strlen(FLDR_NAME(fp)) < MAXFOLDER - 1){
			strncpy(FPROC(sparms)->fs->first_folder,  FLDR_NAME(fp), MAXFOLDER);
			FPROC(sparms)->fs->first_folder[MAXFOLDER-1] = '\0';
		    }
		}
	    }
	}
	else
	  q_status_message(SM_ORDER | SM_DING, 0, 4,
			   _("Empty folder collection.  No folder to select!"));

	break;


	/*---------- Display  folders ----------*/
      case MC_ZOOM :
	if(sparms->text.handles
	   && folder_total(FOLDERS(sparms->text.handles->h.f.context))){
	    FOLDER_S *fp;
	    int	      n;

	    if((n = selected_folders(sparms->text.handles->h.f.context)) != 0){
		if(sparms->text.handles->h.f.context->use & CNTXT_ZOOM){
		    sparms->text.handles->h.f.context->use &= ~CNTXT_ZOOM;
		    q_status_message(SM_ORDER, 0, 3,
				     _("Folder List Zoom mode is now off"));
		}
		else{
		    q_status_message1(SM_ORDER, 0, 3,
	     _("In Zoomed list of %s folders. Use \"Z\" to restore regular list"),
				      int2string(n));
		    sparms->text.handles->h.f.context->use |= CNTXT_ZOOM;
		}

		/* exit scrolltool to rebuild screen */
		rv = 1;

		/* Set where to start after it's rebuilt */
		FPROC(sparms)->fs->context = sparms->text.handles->h.f.context;
		FPROC(sparms)->fs->first_folder[0] = '\0';
		if((fp = folder_entry(sparms->text.handles->h.f.index,
				  FOLDERS(sparms->text.handles->h.f.context)))
		   && !((sparms->text.handles->h.f.context->use & CNTXT_ZOOM)
			&& !fp->selected)
		   && strlen(FLDR_NAME(fp)) < MAXFOLDER - 1){
		    strncpy(FPROC(sparms)->fs->first_folder,  FLDR_NAME(fp), MAXFOLDER);
		    FPROC(sparms)->fs->first_folder[MAXFOLDER-1] = '\0';
		}
	    }
	    else
	      q_status_message(SM_ORDER, 0, 3,
			       _("No selected folders to Zoom on"));
	}
	else
	  q_status_message(SM_ORDER, 0, 4, _("No Folders to Zoom on!"));

	break;

	/*----- Ask user to abandon selection before exiting -----*/
      case MC_EXITQUERY :
	if(sparms->text.handles
	   && FOLDERS(sparms->text.handles->h.f.context)){
	  int	   i, folder_n;
	  FOLDER_S *fp;
	  
	  folder_n = folder_total(FOLDERS(sparms->text.handles->h.f.context));
	  /* any selected? */
	  for(i = 0; i < folder_n; i++){
	    fp = folder_entry(i, FOLDERS(sparms->text.handles->h.f.context));
	    if(fp->selected)
	      break;
	  }

	  if(i < folder_n	/* some selections have been made */
	     && want_to(_("Really abandon your selections "),
			'y', 'x', NO_HELP, WT_NORM) != 'y'){
	    break;
	  }
	}
	rv = 1;
	break;

	/*------------New Msg command --------------*/
      case MC_CHK_RECENT:
	/*
	 * Derived from code provided by
	 * Rostislav Neplokh neplokh@andrew.cmu.edu and
	 * Robert Siemborski (rjs3@andrew).
	 */
        if(sparms->text.handles
           && folder_total(FOLDERS(sparms->text.handles->h.f.context))){
	    FOLDER_S *folder;

            folder = folder_entry(sparms->text.handles->h.f.index,
				  FOLDERS(sparms->text.handles->h.f.context));
	    
	    if(!folder){
		strncpy(tmp_output, _("Invalid Folder Name"), sizeof(tmp_output)-1);
		tmp_output[sizeof(tmp_output)-1] = '\0';
	    }
	    else if(folder->isdir && !folder->isfolder){
		snprintf(tmp_output, sizeof(tmp_output), _("\"%s\" is a directory"), folder->name);
	    }
	    else{
		char          mailbox_name[MAXPATH+1];
		unsigned long tot, rec;
		int           we_cancel;

		context_apply(mailbox_name,
			      sparms->text.handles->h.f.context,
			      folder->name, MAXPATH+1);

		we_cancel = busy_cue(NULL, NULL, 1);

		if(get_recent_in_folder(mailbox_name, &rec, NULL, &tot, NULL))
		  snprintf(tmp_output, sizeof(tmp_output),
			   _("%lu total message%s, %lu of them recent"),
			   tot, plural(tot), rec);
		else
		  snprintf(tmp_output, sizeof(tmp_output),
			   _("%s: Trouble checking for recent mail"), folder->name);

		if(we_cancel)
		  cancel_busy_cue(-1);
	    }
        }
	else{
	    strncpy(tmp_output, _("No folder to check! Can't get recent info"),
		    sizeof(tmp_output)-1);
	    tmp_output[sizeof(tmp_output)-1] = '\0';
	}

	q_status_message(SM_ORDER, 0, 3, tmp_output);
        break;

  
            /*--------------- Invalid Command --------------*/
      default: 
	q_status_message1(SM_ORDER, 0, 2, "fix this: cmd = %s", comatose(cmd));
	break;
    }

    return(rv);
}


int
folder_lister_clickclick(SCROLL_S *sparms)
{
  if(!FPROC(sparms)->fs->list_cntxt)
    return(folder_lister_choice(sparms));
  else
    return(folder_processor(MC_SELCUR, ps_global->msgmap, sparms));
}

int
folder_lister_choice(SCROLL_S *sparms)
{
    int	       rv = 0, empty = 0;
    int	       index = (sparms->text.handles)
			 ? sparms->text.handles->h.f.index : 0;
    CONTEXT_S *cntxt = (sparms->text.handles)
			 ? sparms->text.handles->h.f.context : NULL;

    if(cntxt){
	
	FPROC(sparms)->fs->context = cntxt;

	if(cntxt->dir->status & CNTXT_NOFIND){
	    rv = 1;		/* leave scrolltool to rebuild screen */
	    FPROC(sparms)->fs->context = cntxt;
	    FPROC(sparms)->fs->first_folder[0] = '\0';
	}
	else if(folder_total(FOLDERS(cntxt))){
	    if(folder_lister_select(FPROC(sparms)->fs, cntxt, index,
				    sparms->text.handles ?
				      sparms->text.handles->is_dual_do_open : 0)){
		rv = 1;		/* leave scrolltool to rebuild screen */
	    }
	    else if(FPROC(sparms)->fs->list_cntxt == cntxt){
		int	   n = 0, i, folder_n;
		FOLDER_S  *fp;
		STRLIST_S *sl = NULL, **slp;

		/* Scan folder list for selected items */
		folder_n = folder_total(FOLDERS(cntxt));
		slp = &sl;
		for(i = 0; i < folder_n; i++){
		    fp = folder_entry(i, FOLDERS(cntxt));
		    if(fp->selected){
			n++;
			if((*FPROC(sparms)->fs->f.valid)(fp,
							 FPROC(sparms)->fs)){
			    *slp = new_strlist(NULL);
			    (*slp)->name = folder_lister_fullname(
				FPROC(sparms)->fs, FLDR_NAME(fp));

			    slp = &(*slp)->next;
			}
			else{
			    free_strlist(&sl);
			    break;
			}
		    }
		}

		if((FPROC(sparms)->rv = sl) != NULL)
		  FPROC(sparms)->done = rv = 1;
		else if(!n)
		  q_status_message(SM_ORDER, 0, 1, LISTMODE_GRIPE);
	    }
	    else
	      rv = folder_lister_finish(sparms, cntxt, index);
	}
	else
	  empty++;
    }
    else
      empty++;

    if(empty)
      q_status_message(SM_ORDER | SM_DING, 3, 3, _("Empty folder list!"));

    return(rv);
}


int
folder_lister_finish(SCROLL_S *sparms, CONTEXT_S *cntxt, int index)
{
    FOLDER_S *f = folder_entry(index, FOLDERS(cntxt));
    int	      rv = 0;

    if((*FPROC(sparms)->fs->f.valid)(f, FPROC(sparms)->fs)){
	/*
	 * Package up the selected folder names and return...
	 */
	FPROC(sparms)->fs->context = cntxt;
	FPROC(sparms)->rv = new_strlist(NULL);
	FPROC(sparms)->rv->name = folder_lister_fullname(FPROC(sparms)->fs,
							 FLDR_NAME(f));
	FPROC(sparms)->done = rv = 1;
    }

    return(rv);
}


/*
 * This is so that when you Save and use ^T to go to the folder list, and
 * you're in a directory with no folders, you have a way to add a new
 * folder there. The add actually gets done by the caller. This is just a
 * way to let the user type in the name.
 */
int
folder_lister_addmanually(SCROLL_S *sparms)
{
    int        rc, flags = OE_APPEND_CURRENT, cnt = 0, rv = 0;
    char       addname[MAXFOLDER+1];
    HelpType   help;
    CONTEXT_S *cntxt = (sparms->text.handles)
			 ? sparms->text.handles->h.f.context
			 : FPROC(sparms)->fs->context;

    /*
     * Get the foldername from the user.
     */
    addname[0] = '\0';
    help = NO_HELP;
    while(1){
	rc = optionally_enter(addname, -FOOTER_ROWS(ps_global), 0,
			      sizeof(addname), _("Name of new folder : "),
			      NULL, help, &flags);
	removing_leading_and_trailing_white_space(addname);

	if(rc == 3)
	  help = (help == NO_HELP) ? h_save_addman : NO_HELP;
	else if(rc == 1)
	  return(rv);
	else if(rc == 0){
	    if(F_OFF(F_ENABLE_DOT_FOLDERS,ps_global) && *addname == '.'){
		if(cnt++ <= 0)
                  q_status_message(SM_ORDER,3,3,
		    _("Folder name can't begin with dot"));
		else
		  q_status_message1(SM_ORDER,3,3,
		   _("Config feature \"%s\" enables names beginning with dot"),
		   pretty_feature_name(feature_list_name(F_ENABLE_DOT_FOLDERS), -1));

                display_message(NO_OP_COMMAND);
                continue;
	    }
	    else if(!strucmp(addname, ps_global->inbox_name)){
		q_status_message1(SM_ORDER, 3, 3,
				  _("Can't add folder named %s"),
				  ps_global->inbox_name);
		continue;
	    }

	    break;
	}
    }

    if(*addname){
	FPROC(sparms)->fs->context = cntxt;
	FPROC(sparms)->rv = new_strlist(NULL);
	FPROC(sparms)->rv->name = folder_lister_fullname(FPROC(sparms)->fs,
							 addname);
	FPROC(sparms)->done = rv = 1;
    }

    return(rv);
}


void
folder_lister_km_manager(SCROLL_S *sparms, int handle_hidden)
{
    FOLDER_S *fp;

    /* if we're "in" a sub-directory, offer way out */
    if((sparms->text.handles)
	 ? sparms->text.handles->h.f.context->dir->prev
	 : FPROC(sparms)->fs->context->dir->prev){

	/*
	 * Leave the command characters alone and just change
	 * the labels and the bind.cmd for KM_COL_KEY.
	 * Also, leave KM_MAIN_KEY alone instead of trying to
	 * turn it off in the else clause when it is redundant.
	 */
	sparms->keys.menu->keys[KM_COL_KEY].label      = N_("ParentDir");
	sparms->keys.menu->keys[KM_COL_KEY].bind.cmd   = MC_PARENT;
    }
    else if((FPROC(sparms)->fs->context->next
	     || FPROC(sparms)->fs->context->prev)
	    && !FPROC(sparms)->fs->combined_view){
	sparms->keys.menu->keys[KM_COL_KEY].label      = N_("ClctnList");
	sparms->keys.menu->keys[KM_COL_KEY].bind.cmd   = MC_EXIT;
    }
    else{
	sparms->keys.menu->keys[KM_COL_KEY].label      = N_("Main Menu");
	sparms->keys.menu->keys[KM_COL_KEY].bind.cmd   = MC_MAIN;
    }

    if(F_OFF(F_ENABLE_AGG_OPS,ps_global)){
	clrbitn(KM_ZOOM_KEY, sparms->keys.bitmap);
	clrbitn(KM_SELECT_KEY, sparms->keys.bitmap);
	clrbitn(KM_SELCUR_KEY, sparms->keys.bitmap);
    }

    if(sparms->text.handles
       && (fp = folder_entry(sparms->text.handles->h.f.index,
			     FOLDERS(sparms->text.handles->h.f.context)))){
	if(fp->isdir && !sparms->text.handles->is_dual_do_open){
	    sparms->keys.menu->keys[KM_SEL_KEY].label = "[" N_("View Dir") "]";
	    menu_add_binding(sparms->keys.menu, 'v', MC_CHOICE);
	    menu_add_binding(sparms->keys.menu, ctrl('M'), MC_CHOICE);
	    menu_add_binding(sparms->keys.menu, ctrl('J'), MC_CHOICE);
	    setbitn(KM_SEL_KEY, sparms->keys.bitmap);
	    clrbitn(KM_ALTVIEW_KEY, sparms->keys.bitmap);
	    clrbitn(KM_EXPORT_KEY, sparms->keys.bitmap);
	    clrbitn(KM_IMPORT_KEY, sparms->keys.bitmap);
	}
	else{
	    sparms->keys.menu->keys[KM_SEL_KEY].label = "[" N_("View Fldr") "]";
	    menu_add_binding(sparms->keys.menu, 'v', MC_CHOICE);
	    menu_add_binding(sparms->keys.menu, ctrl('M'), MC_CHOICE);
	    menu_add_binding(sparms->keys.menu, ctrl('J'), MC_CHOICE);
	    setbitn(KM_SEL_KEY, sparms->keys.bitmap);
	    clrbitn(KM_ALTVIEW_KEY, sparms->keys.bitmap);
	    setbitn(KM_EXPORT_KEY, sparms->keys.bitmap);
	    setbitn(KM_IMPORT_KEY, sparms->keys.bitmap);
	}
    }
    else if(FPROC(sparms)->fs->combined_view
	    && sparms->text.handles && sparms->text.handles->h.f.context
	    && !sparms->text.handles->h.f.context->dir->prev){
	sparms->keys.menu->keys[KM_SEL_KEY].label = "[" N_("View Cltn") "]";
	menu_add_binding(sparms->keys.menu, 'v', MC_CHOICE);
	menu_add_binding(sparms->keys.menu, ctrl('M'), MC_CHOICE);
	menu_add_binding(sparms->keys.menu, ctrl('J'), MC_CHOICE);
	setbitn(KM_SEL_KEY, sparms->keys.bitmap);
	clrbitn(KM_ALTVIEW_KEY, sparms->keys.bitmap);
	clrbitn(KM_EXPORT_KEY, sparms->keys.bitmap);
	clrbitn(KM_IMPORT_KEY, sparms->keys.bitmap);
    }
    else{
	clrbitn(KM_SEL_KEY, sparms->keys.bitmap);
	clrbitn(KM_ALTVIEW_KEY, sparms->keys.bitmap);
	clrbitn(KM_EXPORT_KEY, sparms->keys.bitmap);
	clrbitn(KM_IMPORT_KEY, sparms->keys.bitmap);
    }
      
    if((sparms->text.handles &&
        sparms->text.handles->h.f.context &&
        sparms->text.handles->h.f.context->use & CNTXT_INCMNG) ||
       (FPROC(sparms) && FPROC(sparms)->fs &&
        FPROC(sparms)->fs->context &&
        FPROC(sparms)->fs->context->use & CNTXT_INCMNG))
      setbitn(KM_SHUFFLE_KEY, sparms->keys.bitmap);
    else
      clrbitn(KM_SHUFFLE_KEY, sparms->keys.bitmap);

    if(F_ON(F_TAB_CHK_RECENT, ps_global)){
	menu_clear_binding(sparms->keys.menu, TAB);
	menu_init_binding(sparms->keys.menu, TAB, MC_CHK_RECENT, "Tab",
			  /* TRANSLATORS: New Messages */
			  N_("NewMsgs"), KM_RECENT_KEY);
	setbitn(KM_RECENT_KEY, sparms->keys.bitmap);
    }
    else{
	menu_clear_binding(sparms->keys.menu, TAB);
	menu_add_binding(sparms->keys.menu, TAB, MC_NEXT_HANDLE);
	clrbitn(KM_RECENT_KEY, sparms->keys.bitmap);
    }

    /* May have to "undo" what scrolltool "did" */
    if(F_ON(F_ARROW_NAV, ps_global)){
	if(F_ON(F_RELAXED_ARROW_NAV, ps_global)){
	    menu_clear_binding(sparms->keys.menu, KEY_LEFT);
	    menu_add_binding(sparms->keys.menu, KEY_LEFT, MC_PREV_HANDLE);
	    menu_clear_binding(sparms->keys.menu, KEY_RIGHT);
	    menu_add_binding(sparms->keys.menu, KEY_RIGHT, MC_NEXT_HANDLE);
	}
	else{
	    menu_clear_binding(sparms->keys.menu, KEY_UP);
	    menu_add_binding(sparms->keys.menu, KEY_UP, MC_PREV_HANDLE);
	    menu_clear_binding(sparms->keys.menu, KEY_DOWN);
	    menu_add_binding(sparms->keys.menu, KEY_DOWN, MC_NEXT_HANDLE);
	}
    }
}


void
folder_lister_km_sel_manager(SCROLL_S *sparms, int handle_hidden)
{
    FOLDER_S *fp;

    /* if we're "in" a sub-directory, offer way out */
    if((sparms->text.handles)
	 ? sparms->text.handles->h.f.context->dir->prev
	 : FPROC(sparms)->fs->context->dir->prev){
	sparms->keys.menu->keys[FC_COL_KEY].name       = "<";
	/* TRANSLATORS: go to parent directory one level up */
	sparms->keys.menu->keys[FC_COL_KEY].label      = N_("ParentDir");
	sparms->keys.menu->keys[FC_COL_KEY].bind.cmd   = MC_PARENT;
	sparms->keys.menu->keys[FC_COL_KEY].bind.ch[0] = '<';
	sparms->keys.menu->keys[FC_COL_KEY].bind.ch[1] = ',';
	if(F_ON(F_ARROW_NAV,ps_global)){
	  sparms->keys.menu->keys[FC_COL_KEY].bind.nch = 3;
	  sparms->keys.menu->keys[FC_COL_KEY].bind.ch[2] = KEY_LEFT;
	}
	else
	  sparms->keys.menu->keys[FC_COL_KEY].bind.nch   = 2;

	/* ExitSelect in position 1 */
	setbitn(FC_EXIT_KEY, sparms->keys.bitmap);
    }
    else if((FPROC(sparms)->fs->context->next
	     || FPROC(sparms)->fs->context->prev)
	    && !FPROC(sparms)->fs->combined_view){
	sparms->keys.menu->keys[FC_COL_KEY].name       = "<";
	/* TRANSLATORS: go to Collection List */
	sparms->keys.menu->keys[FC_COL_KEY].label      = N_("ClctnList");
	sparms->keys.menu->keys[FC_COL_KEY].bind.cmd   = MC_COLLECTIONS;
	sparms->keys.menu->keys[FC_COL_KEY].bind.ch[0] = '<';
	sparms->keys.menu->keys[FC_COL_KEY].bind.ch[1] = ',';
	if(F_ON(F_ARROW_NAV,ps_global)){
	  sparms->keys.menu->keys[FC_COL_KEY].bind.nch = 3;
	  sparms->keys.menu->keys[FC_COL_KEY].bind.ch[2] = KEY_LEFT;
	}
	else
	  sparms->keys.menu->keys[FC_COL_KEY].bind.nch   = 2;

	/* ExitSelect in position 1 */
	setbitn(FC_EXIT_KEY, sparms->keys.bitmap);
    }
    else if(FPROC(sparms)->fs->combined_view){
	/*
	 * This can't be a menu_init_binding() because we don't want
	 * to remove the ExitSelect command in position FC_EXIT_KEY.
	 * We just turn it off until we need it again.
	 */
	sparms->keys.menu->keys[FC_COL_KEY].name       = "E";
	sparms->keys.menu->keys[FC_COL_KEY].label      = N_("ExitSelect");
	sparms->keys.menu->keys[FC_COL_KEY].bind.cmd   = MC_EXIT;
	sparms->keys.menu->keys[FC_COL_KEY].bind.nch   = 1;
	sparms->keys.menu->keys[FC_COL_KEY].bind.ch[0] = 'e';

	/* turn off ExitSelect in position 1, it's in 2 now */
	clrbitn(FC_EXIT_KEY, sparms->keys.bitmap);
    }

    /* clean up per-entry bindings */
    clrbitn(FC_SEL_KEY, sparms->keys.bitmap);
    clrbitn(FC_ALTSEL_KEY, sparms->keys.bitmap);
    menu_clear_binding(sparms->keys.menu, ctrl('M'));
    menu_clear_binding(sparms->keys.menu, ctrl('J'));
    menu_clear_binding(sparms->keys.menu, '>');
    menu_clear_binding(sparms->keys.menu, '.');
    menu_clear_binding(sparms->keys.menu, 's');
    if(F_ON(F_ARROW_NAV,ps_global))
      menu_clear_binding(sparms->keys.menu, KEY_RIGHT);

    /* and then re-assign them as needed */
    if(sparms->text.handles
       && (fp = folder_entry(sparms->text.handles->h.f.index,
			     FOLDERS(sparms->text.handles->h.f.context)))){
	setbitn(FC_SEL_KEY, sparms->keys.bitmap);
	if(fp->isdir){
	    sparms->keys.menu->keys[FC_SEL_KEY].name = ">";
	    menu_add_binding(sparms->keys.menu, '>', MC_CHOICE);
	    menu_add_binding(sparms->keys.menu, '.', MC_CHOICE);
	    if(F_ON(F_ARROW_NAV,ps_global))
	      menu_add_binding(sparms->keys.menu, KEY_RIGHT, MC_CHOICE);

	    if(fp->isfolder){
		sparms->keys.menu->keys[FC_SEL_KEY].label = N_("View Dir");
		setbitn(FC_ALTSEL_KEY, sparms->keys.bitmap);
		menu_add_binding(sparms->keys.menu, 's', MC_OPENFLDR);
		menu_add_binding(sparms->keys.menu, ctrl('M'), MC_OPENFLDR);
		menu_add_binding(sparms->keys.menu, ctrl('J'), MC_OPENFLDR);
	    }
	    else{
		sparms->keys.menu->keys[FC_SEL_KEY].label = "[" N_("View Dir") "]";
		menu_add_binding(sparms->keys.menu, ctrl('M'), MC_CHOICE);
		menu_add_binding(sparms->keys.menu, ctrl('J'), MC_CHOICE);
	    }
	}
	else{
	    sparms->keys.menu->keys[FC_SEL_KEY].name  = "S";
	    sparms->keys.menu->keys[FC_SEL_KEY].label = "[" N_("Select") "]";

	    menu_add_binding(sparms->keys.menu, 's', MC_CHOICE);
	    menu_add_binding(sparms->keys.menu, ctrl('M'), MC_CHOICE);
	    menu_add_binding(sparms->keys.menu, ctrl('J'), MC_CHOICE);
	}

	if(F_ON(F_ARROW_NAV,ps_global))
	  menu_add_binding(sparms->keys.menu, KEY_RIGHT, MC_CHOICE);
    }
    else if(FPROC(sparms)->fs->combined_view
	    && sparms->text.handles && sparms->text.handles->h.f.context
	    && !sparms->text.handles->h.f.context->dir->prev){
	setbitn(FC_SEL_KEY, sparms->keys.bitmap);
	sparms->keys.menu->keys[FC_SEL_KEY].name = ">";
	sparms->keys.menu->keys[FC_SEL_KEY].label = "[" N_("View Cltn") "]";
	menu_add_binding(sparms->keys.menu, '>', MC_CHOICE);
	menu_add_binding(sparms->keys.menu, '.', MC_CHOICE);
	if(F_ON(F_ARROW_NAV,ps_global))
	  menu_add_binding(sparms->keys.menu, KEY_RIGHT, MC_CHOICE);

	menu_add_binding(sparms->keys.menu, ctrl('M'), MC_CHOICE);
	menu_add_binding(sparms->keys.menu, ctrl('J'), MC_CHOICE);
    }

    /* May have to "undo" what scrolltool "did" */
    if(F_ON(F_ARROW_NAV, ps_global)){
	if(F_ON(F_RELAXED_ARROW_NAV, ps_global)){
	    menu_clear_binding(sparms->keys.menu, KEY_LEFT);
	    menu_add_binding(sparms->keys.menu, KEY_LEFT, MC_PREV_HANDLE);
	    menu_clear_binding(sparms->keys.menu, KEY_RIGHT);
	    menu_add_binding(sparms->keys.menu, KEY_RIGHT, MC_NEXT_HANDLE);
	}
	else{
	    menu_clear_binding(sparms->keys.menu, KEY_UP);
	    menu_add_binding(sparms->keys.menu, KEY_UP, MC_PREV_HANDLE);
	    menu_clear_binding(sparms->keys.menu, KEY_DOWN);
	    menu_add_binding(sparms->keys.menu, KEY_DOWN, MC_NEXT_HANDLE);
	}
    }
}


void
folder_lister_km_sub_manager(SCROLL_S *sparms, int handle_hidden)
{
    /*
     * Folder_processor() also modifies the keymenu.
     */
    if(FPROC(sparms)->fs->list_cntxt){
	clrbitn(SB_LIST_KEY, sparms->keys.bitmap);
	sparms->keys.menu->keys[SB_SEL_KEY].name = "X";
	sparms->keys.menu->keys[SB_SEL_KEY].label = "[" N_("Set/Unset") "]";
	sparms->keys.menu->keys[SB_SEL_KEY].bind.cmd = MC_SELCUR;
	sparms->keys.menu->keys[SB_SEL_KEY].bind.ch[0] = 'x';
    }
    else{
	clrbitn(SB_SUB_KEY, sparms->keys.bitmap);
	sparms->keys.menu->keys[SB_SEL_KEY].name = "S";
	sparms->keys.menu->keys[SB_SEL_KEY].label = "[" N_("Subscribe") "]";
	sparms->keys.menu->keys[SB_SEL_KEY].bind.cmd = MC_CHOICE;
	sparms->keys.menu->keys[SB_SEL_KEY].bind.ch[0] = 's';
    }

    /* May have to "undo" what scrolltool "did" */
    if(F_ON(F_ARROW_NAV, ps_global)){
	if(F_ON(F_RELAXED_ARROW_NAV, ps_global)){
	    menu_clear_binding(sparms->keys.menu, KEY_LEFT);
	    menu_add_binding(sparms->keys.menu, KEY_LEFT, MC_PREV_HANDLE);
	    menu_clear_binding(sparms->keys.menu, KEY_RIGHT);
	    menu_add_binding(sparms->keys.menu, KEY_RIGHT, MC_NEXT_HANDLE);
	}
	else{
	    menu_clear_binding(sparms->keys.menu, KEY_UP);
	    menu_add_binding(sparms->keys.menu, KEY_UP, MC_PREV_HANDLE);
	    menu_clear_binding(sparms->keys.menu, KEY_DOWN);
	    menu_add_binding(sparms->keys.menu, KEY_DOWN, MC_NEXT_HANDLE);
	}
    }
}



int
folder_select(struct pine *ps, CONTEXT_S *context, int cur_index)
{
    int        i, j, n, total, old_tot, diff, 
	       q = 0, rv = 0, narrow = 0;
    HelpType   help = NO_HELP;
    ESCKEY_S  *sel_opts;
    FOLDER_S  *f;
    static ESCKEY_S self_opts2[] = {
	/* TRANSLATORS: keymenu descriptions, select all folders, current folder, select
	   based on folder properties, or select based on text contents in folders */
	{'a', 'a', "A", N_("select All")},
	{'c', 'c', "C", N_("select Cur")},
	{'p', 'p', "P", N_("Properties")},
	{'t', 't', "T", N_("Text")},
	{-1, 0, NULL, NULL},
	{-1, 0, NULL, NULL}
    };
    extern     ESCKEY_S sel_opts1[];
    extern     char *sel_pmt2;
#define N_RECENT 4

    f = folder_entry(cur_index, FOLDERS(context));

    sel_opts = self_opts2;
    if((old_tot = selected_folders(context)) != 0){
	sel_opts1[1].label = f->selected ? N_("unselect Cur") : N_("select Cur");
	sel_opts += 2;			/* disable extra options */
	switch(q = radio_buttons(SEL_ALTER_PMT, -FOOTER_ROWS(ps_global),
				 sel_opts1, 'c', 'x', help, RB_NORM)){
	  case 'f' :			/* flip selection */
	    n = folder_total(FOLDERS(context));
	    for(total = i = 0; i < n; i++)
	      if((f = folder_entry(i, FOLDERS(context))) != NULL)
		f->selected = !f->selected;

	    return(1);			/* repaint */

	  case 'n' :			/* narrow selection */
	    narrow++;
	  case 'b' :			/* broaden selection */
	    q = 0;			/* but don't offer criteria prompt */
	    if(context->use & CNTXT_INCMNG){
		q_status_message1(SM_ORDER | SM_DING, 3, 3,
			 _("Select \"%s\" not supported in Incoming Folders"),
			 narrow ? "Narrow" : "Broaden");
		return(0);
	    }

	    break;

	  case 'c' :			/* Un/Select Current */
	  case 'a' :			/* Unselect All */
	  case 'x' :			/* cancel */
	    break;

	  default :
	    q_status_message(SM_ORDER | SM_DING, 3, 3,
			     _("Unsupported Select option"));
	    return(0);
	}
    }

    if(context->use & CNTXT_INCMNG && F_ON(F_ENABLE_INCOMING_CHECKING, ps_global)){
	if(F_ON(F_INCOMING_CHECKING_RECENT, ps_global)){
	    self_opts2[N_RECENT].ch = 'r';
	    self_opts2[N_RECENT].rval = 'r';
	    self_opts2[N_RECENT].name = "R";
	    self_opts2[N_RECENT].label = N_("Recent");
	}
	else{
	    self_opts2[N_RECENT].ch = 'u';
	    self_opts2[N_RECENT].rval = 'u';
	    self_opts2[N_RECENT].name = "U";
	    self_opts2[N_RECENT].label = N_("Unseen");
	}
    }
    else{
	self_opts2[N_RECENT].ch = -1;
	self_opts2[N_RECENT].rval = 0;
	self_opts2[N_RECENT].name = NULL;
	self_opts2[N_RECENT].label = NULL;
    }

    if(!q)
      q = radio_buttons(sel_pmt2, -FOOTER_ROWS(ps_global),
			sel_opts, 'c', 'x', help, RB_NORM);

    /*
     * NOTE: See note about MESSAGECACHE "searched" bits above!
     */
    switch(q){
      case 'x':				/* cancel */
	cmd_cancelled("Select command");
	return(0);

      case 'c' :			/* toggle current's selected state */
	return(folder_select_toggle(context, cur_index, folder_select_update));

      case 'a' :			/* select/unselect all */
	n = folder_total(FOLDERS(context));
	for(total = i = 0; i < n; i++)
	  folder_entry(i, FOLDERS(context))->selected = old_tot == 0;

	q_status_message4(SM_ORDER, 0, 2,
			  "%s%s folder%s %sselected",
			  old_tot ? "" : "All ",
			  comatose(old_tot ? old_tot : n),
			  plural(old_tot ? old_tot : n), old_tot ? "UN" : "");
	return(1);

      case 't' :			/* Text */
	if(!folder_select_text(ps, context, narrow))
	  rv++;

	break;

      case 'p' :			/* Properties */
	if(!folder_select_props(ps, context, narrow))
	  rv++;

	break;

      case 'r' :
	n = folder_total(FOLDERS(context));
	for(i = 0; i < n; i++){
	    f = folder_entry(i, FOLDERS(context));
	    if(f->unseen_valid && f->new > 0L)
	      f->selected = 1;
	}

	break;

      case 'u' :
	n = folder_total(FOLDERS(context));
	for(i = 0; i < n; i++){
	    f = folder_entry(i, FOLDERS(context));
	    if(f->unseen_valid && f->unseen > 0L)
	      f->selected = 1;
	}

	break;

      default :
	q_status_message(SM_ORDER | SM_DING, 3, 3,
			 _("Unsupported Select option"));
	return(0);
    }

    if(rv)
      return(0);

    /* rectify the scanned vs. selected folders */
    n = folder_total(FOLDERS(context));
    for(total = i = j = 0; i < n; i++)
      if(folder_entry(i, FOLDERS(context))->scanned)
	break;
    
    /*
     * Any matches at all? If not, the selected set remains the same.
     * Note that when Narrowing, only matches in the intersection will
     * show up as scanned. We need to reset i to 0 to erase any already
     * selected messages which aren't in the intersection.
     */
    if(i < n)
      for(i = 0; i < n; i++)
	if((f = folder_entry(i, FOLDERS(context))) != NULL){
	    if(narrow){
		if(f->selected){
		    f->selected = f->scanned;
		    j++;
		}
	    }
	    else if(f->scanned)
	      f->selected = 1;
	}

    if(!(diff = (total = selected_folders(context)) - old_tot)){
	if(narrow)
	  q_status_message4(SM_ORDER, 0, 2,
			  "%s.  %s folder%s remain%s selected.",
			    j ? _("No change resulted")
			      : _("No messages in intersection"),
			    comatose(old_tot), plural(old_tot),
			    (old_tot == 1L) ? "s" : "");
	else if(old_tot && j)
	  q_status_message(SM_ORDER, 0, 2,
		   _("No change resulted.  Matching folders already selected."));
	else
	  q_status_message1(SM_ORDER | SM_DING, 0, 2,
			    "Select failed!  No %sfolders selected.",
			    old_tot ? "additional " : "");
    }
    else if(old_tot){
	snprintf(tmp_20k_buf, SIZEOF_20KBUF,
		"Select matched %d folder%s.  %s %sfolder%s %sselected.",
		(diff > 0) ? diff : old_tot + diff,
		plural((diff > 0) ? diff : old_tot + diff),
		comatose((diff > 0) ? total : -diff),
		(diff > 0) ? "total " : "",
		plural((diff > 0) ? total : -diff),
		(diff > 0) ? "" : "UN");
	tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	q_status_message(SM_ORDER, 0, 2, tmp_20k_buf);
    }
    else
      q_status_message2(SM_ORDER, 0, 2, "Select matched %s folder%s.",
			comatose(diff), plural(diff));

    return(1);
}




int
folder_lister_select(FSTATE_S *fs, CONTEXT_S *context, int index, int is_dual_do_open)
{
    int       rv = 0;
    FDIR_S   *fp;
    FOLDER_S  *f = folder_entry(index, FOLDERS(context));

    /*--- Entering a directory?  ---*/
    if(f->isdir && !is_dual_do_open){
	fp = next_folder_dir(context, f->name, TRUE, fs->cache_streamp);

	/* Provide context in new collection header */
	fp->desc = folder_lister_desc(context, fp);

	/* Insert new directory into list */
	free_folder_list(context);
	fp->prev	  = context->dir;
	fp->status	 |= CNTXT_SUBDIR;
	context->dir  = fp;
	q_status_message2(SM_ORDER, 0, 3, "Now in %sdirectory: %s",
			  folder_total(FOLDERS(context))
			  ? "" : "EMPTY ",  fp->ref);
	rv++;
    }
    else
      rv = folder_lister_parent(fs, context, index, 1);

    return(rv);
}


int
folder_lister_parent(FSTATE_S *fs, CONTEXT_S *context, int index, int force_parent)
{
    int       rv = 0;
    FDIR_S   *fp;

    if(!force_parent && (fp = context->dir->prev)){
	char *s, oldir[MAILTMPLEN];

	folder_select_preserve(context);
	oldir[0] = '\0';
	if((s = strrindex(context->dir->ref, context->dir->delim)) != NULL){
	    *s = '\0';
	    if((s = strrindex(context->dir->ref, context->dir->delim)) != NULL){
		strncpy(oldir, s+1, sizeof(oldir)-1);
		oldir[sizeof(oldir)-1] = '\0';
	    }
	}

	if(*oldir){
	    /* remember current directory for hiliting in new list */
	    fs->context = context;
	    if(strlen(oldir) < MAXFOLDER - 1){
		strncpy(fs->first_folder,  oldir, MAXFOLDER);
		fs->first_folder[MAXFOLDER-1] = '\0';
		fs->first_dir = 1;
	    }
	}

	free_fdir(&context->dir, 0);
	fp->status |= CNTXT_NOFIND;

	context->dir = fp;

	if(fp->status & CNTXT_SUBDIR)
	  q_status_message1(SM_ORDER, 0, 3, _("Now in directory: %s"),
			    strsquish(tmp_20k_buf + 500, SIZEOF_20KBUF-500, fp->ref,
				      ps_global->ttyo->screen_cols - 22));
	else
	  q_status_message(SM_ORDER, 0, 3,
			   _("Returned to collection's top directory"));

	rv++;
    }

    return(rv);
}


char *
folder_lister_fullname(FSTATE_S *fs, char *name)
{
    if(fs->context->dir->status & CNTXT_SUBDIR){
	char tmp[2*MAILTMPLEN], tmp2[2*MAILTMPLEN], *p;

	if(fs->context->dir->ref){
	    snprintf(tmp, sizeof(tmp), "%.*s%.*s",
		  sizeof(tmp)/2,
		  ((fs->relative_path || (fs->context->use & CNTXT_SAVEDFLT))
		   && (p = strstr(fs->context->context, "%s")) && !*(p+2)
		   && !strncmp(fs->context->dir->ref, fs->context->context,
			       p - fs->context->context))
		    ? fs->context->dir->ref + (p - fs->context->context)
		    : fs->context->dir->ref,
		  sizeof(tmp)/2, name);
	    tmp[sizeof(tmp)-1] = '\0';
	}

	/*
	 * If the applied name is still ambiguous (defined
	 * that way by the user (i.e., "mail/[]"), then
	 * we better fix it up...
	 */
	if(context_isambig(tmp)
	   && !fs->relative_path
	   && !(fs->context->use & CNTXT_SAVEDFLT)){
	    /* if it's in the primary collection, the names relative */
	    if(fs->context->dir->ref){
		if(IS_REMOTE(fs->context->context)
		   && (p = strrindex(fs->context->context, '}'))){
		    snprintf(tmp2, sizeof(tmp2), "%.*s%.*s",
			  MIN(p - fs->context->context + 1, sizeof(tmp2)/2),
			  fs->context->context,
			  sizeof(tmp2)/2, tmp);
		    tmp2[sizeof(tmp2)-1] = '\0';
		}
		else
		  build_path(tmp2, ps_global->ui.homedir, tmp, sizeof(tmp2));
	    }
	    else
	      (void) context_apply(tmp2, fs->context, tmp, sizeof(tmp2));

	    return(cpystr(tmp2));
	}

	return(cpystr(tmp));
    }

    return(cpystr(name));
}


/*
 * Export a folder from pine space to user's space. It will still be a regular
 * mail folder but it will be in the user's home directory or something like
 * that.
 */
void
folder_export(SCROLL_S *sparms)
{
    FOLDER_S   *f;
    MAILSTREAM *stream, *ourstream = NULL;
    char        expanded_file[MAILTMPLEN], *p,
		tmp[MAILTMPLEN], *fname, *fullname = NULL,
		filename[MAXPATH+1], full_filename[MAXPATH+1],
		deefault[MAXPATH+1];
    int	        open_inbox = 0, we_cancel = 0, width,
		index = (sparms && sparms->text.handles)
			 ? sparms->text.handles->h.f.index : 0;
    CONTEXT_S  *savecntxt,
	       *cntxt = (sparms && sparms->text.handles)
			 ? sparms->text.handles->h.f.context : NULL;
    static HISTORY_S *history = NULL;

    dprint((4, "\n - folder export -\n"));

    if(cntxt){
	if(folder_total(FOLDERS(cntxt))){
	    f = folder_entry(index, FOLDERS(cntxt));
	    if((*FPROC(sparms)->fs->f.valid)(f, FPROC(sparms)->fs)){
		savecntxt = FPROC(sparms)->fs->context;   /* necessary? */
		FPROC(sparms)->fs->context = cntxt;
		strncpy(deefault, FLDR_NAME(f), sizeof(deefault)-1);
		deefault[sizeof(deefault)-1] = '\0';
		fname = folder_lister_fullname(FPROC(sparms)->fs, FLDR_NAME(f));
		FPROC(sparms)->fs->context = savecntxt;

		/*
		 * We have to allow for INBOX and nicknames in
		 * the incoming collection. Mimic what happens in
		 * do_broach_folder.
		 */
		strncpy(expanded_file, fname, sizeof(expanded_file));
		expanded_file[sizeof(expanded_file)-1] = '\0';

		if(strucmp(fname, ps_global->inbox_name) == 0
		   || strucmp(fname, ps_global->VAR_INBOX_PATH) == 0)
		  open_inbox++;

		if(!open_inbox && cntxt && context_isambig(fname)){
		    if((p=folder_is_nick(fname, FOLDERS(cntxt), 0)) != NULL){
			strncpy(expanded_file, p, sizeof(expanded_file));
			expanded_file[sizeof(expanded_file)-1] = '\0';
		    }
		    else if ((cntxt->use & CNTXT_INCMNG)
			     && (folder_index(fname, cntxt, FI_FOLDER) < 0)
			     && !is_absolute_path(fname)){
			q_status_message1(SM_ORDER, 3, 4,
			    _("Can't find Incoming Folder %s."), fname);
			return;
		    }
		}

		if(open_inbox)
		  fullname = ps_global->VAR_INBOX_PATH;
		else{
		    /*
		     * We don't want to interpret this name in the context
		     * of the reference string, that was already done
		     * above in folder_lister_fullname(), just in the
		     * regular context. We also don't want to lose the
		     * reference string because we will still be in the
		     * subdirectory after this operation completes. So
		     * temporarily zero out the reference.
		     */
		    FDIR_S *tmpdir;

		    tmpdir = (cntxt ? cntxt->dir : NULL);
		    cntxt->dir = NULL;
		    fullname = context_apply(tmp, cntxt, expanded_file,
					     sizeof(tmp));
		    cntxt->dir = tmpdir;
		}

		width = MAX(20,
			   ps_global->ttyo ? ps_global->ttyo->screen_cols : 80);
		stream = sp_stream_get(fullname, SP_MATCH | SP_RO_OK);
		if(!stream && fullname){
		    /*
		     * Just using filename and full_filename as convenient
		     * temporary buffers here.
		     */
		    snprintf(filename, sizeof(filename), "Opening \"%s\"",
			    short_str(fullname, full_filename, sizeof(full_filename),
				      MAX(10, width-17),
				      MidDots));
		    filename[sizeof(filename)-1] = '\0';
		    we_cancel = busy_cue(filename, NULL, 0);
		    stream = pine_mail_open(NULL, fullname,
					    OP_READONLY|SP_USEPOOL|SP_TEMPUSE,
					    NULL);
		    if(we_cancel)
		      cancel_busy_cue(0);

		    ourstream = stream;
		}

		/*
		 * We have a stream for the folder we want to
		 * export.
		 */
		if(stream && stream->nmsgs > 0L){
		    int r = 1;
		    static ESCKEY_S eopts[] = {
			{ctrl('T'), 10, "^T", N_("To Files")},
			{-1, 0, NULL, NULL},
			{-1, 0, NULL, NULL}};

		    if(F_ON(F_ENABLE_TAB_COMPLETE,ps_global)){
			eopts[r].ch    =  ctrl('I');
			eopts[r].rval  = 11;
			eopts[r].name  = "TAB";
			/* TRANSLATORS: Complete is a verb, complete the name of a folder */
			eopts[r].label = _("Complete");
		    }

		    eopts[++r].ch = -1;

		    filename[0] = '\0';
		    full_filename[0] = '\0';

		    r = get_export_filename(ps_global, filename, deefault,
					    full_filename,
					    sizeof(filename)-20, fname, NULL,
					    eopts, NULL,
					    -FOOTER_ROWS(ps_global),
					    GE_IS_EXPORT | GE_NO_APPEND, &history);
		    if(r < 0){
			switch(r){
			  default:
			  case -1:
			    cmd_cancelled("Export folder");
			    break;

			  case -2:
			    q_status_message1(SM_ORDER, 0, 2,
				      _("Can't export to file outside of %s"),
					      ps_global->VAR_OPER_DIR);
			    break;
			}
		    }
		    else{
			int ok;
			char *ff;

			ps_global->mm_log_error = 0;

			/*
			 * Do the creation in standard unix format, so it
			 * is readable by all.
			 */
			rplstr(full_filename, sizeof(full_filename), 0, "#driver.unix/");
			ok = pine_mail_create(NULL, full_filename) != 0L;
			/*
			 * ff points to the original filename, without the
			 * driver prefix. Only mail_create knows how to
			 * handle driver prefixes.
			 */
			ff = full_filename + strlen("#driver.unix/");

			if(!ok){
			    if(!ps_global->mm_log_error)
			      q_status_message(SM_ORDER | SM_DING, 3, 3,
					       _("Error creating file"));
			}
			else{
			    long     l, snmsgs;
			    MSGNO_S *tmpmap = NULL;

			    snmsgs = stream->nmsgs;
			    mn_init(&tmpmap, snmsgs);
			    for(l = 1L; l <= snmsgs; l++)
			      if(l == 1L)
				mn_set_cur(tmpmap, l);
			      else
				mn_add_cur(tmpmap, l);
			    
			    blank_keymenu(ps_global->ttyo->screen_rows-2, 0);
			    we_cancel = busy_cue(_("Copying folder"), NULL, 0);
			    l = save(ps_global, stream, NULL, ff, tmpmap, 0);
			    if(we_cancel)
			      cancel_busy_cue(0);

			    mn_give(&tmpmap);

			    if(l == snmsgs)
			      q_status_message2(SM_ORDER, 0, 3,
					    "Folder %s exported to %s",
					        fname, filename);
			    else{
				q_status_message1(SM_ORDER | SM_DING, 3, 3,
						  _("Error exporting to %s"),
						  filename);
				dprint((2,
		    "Error exporting to %s: expected %ld msgs, saved %ld\n",
		    filename, snmsgs, l));
			    }
			}
		    }
		}
		else if(stream)
		  q_status_message1(SM_ORDER|SM_DING, 3, 3,
				    _("No messages in %s to export"), fname);
		else
		  q_status_message(SM_ORDER|SM_DING, 3, 3,
				   _("Can't open folder for exporting"));

		if(fname)
		  fs_give((void **) &fname);

		if(ourstream)
		  pine_mail_close(ourstream);
	    }
	}
    }
    else
      q_status_message(SM_ORDER | SM_DING, 3, 3, _("Empty folder list!"));
}


/*
 * Import a folder from user's space back to pine space.
 * We're just importing a regular mail folder, and saving all the messages
 * to another folder. It may seem more magical to the user but it isn't.
 * The import folder is a local folder, the new one can be remote or whatever.
 * Args  sparms
 *       add_folder -- return new folder name here
 *       len        -- length of add_folder
 *
 * Returns 1 if we may have to redraw screen, 0 otherwise
 */
int
folder_import(SCROLL_S *sparms, char *add_folder, size_t len)
{
    MAILSTREAM *istream = NULL;
    int         r = 1, rv = 0;
    char        filename[MAXPATH+1], full_filename[MAXPATH+1];
    static HISTORY_S *history = NULL;
    static ESCKEY_S eopts[] = {
	{ctrl('T'), 10, "^T", N_("To Files")},
	{-1, 0, NULL, NULL},
	{-1, 0, NULL, NULL}};

    if(F_ON(F_ENABLE_TAB_COMPLETE,ps_global)){
	eopts[r].ch    =  ctrl('I');
	eopts[r].rval  = 11;
	eopts[r].name  = "TAB";
	eopts[r].label = N_("Complete");
    }

    eopts[++r].ch = -1;

    filename[0] = '\0';
    full_filename[0] = '\0';

    /* get a folder to import */
    r = get_export_filename(ps_global, filename, NULL, full_filename,
			    sizeof(filename)-20, "messages", "IMPORT",
			    eopts, NULL,
			    -FOOTER_ROWS(ps_global), GE_IS_IMPORT, &history);
    if(r < 0){
	switch(r){
	  default:
	  case -1:
	    cmd_cancelled("Import folder");
	    break;

	  case -2:
	    q_status_message1(SM_ORDER, 0, 2,
		      _("Can't import file outside of %s"),
			      ps_global->VAR_OPER_DIR);
	    break;
	}
    }
    else{
	ps_global->mm_log_error = 0;
	if(full_filename && full_filename[0])
	  istream = pine_mail_open(NULL, full_filename,
				   OP_READONLY | SP_TEMPUSE, NULL);

	if(istream && istream->nmsgs > 0L){
	    long       l;
	    int        we_cancel = 0;
	    char       newfolder[MAILTMPLEN], nmsgs[32];
	    MSGNO_S   *tmpmap = NULL;
	    CONTEXT_S *cntxt, *ourcntxt;

	    cntxt = (sparms && sparms->text.handles)
			 ? sparms->text.handles->h.f.context : NULL;
	    ourcntxt = cntxt;
	    newfolder[0] = '\0';
	    snprintf(nmsgs, sizeof(nmsgs), "%s msgs ", comatose(istream->nmsgs));
	    nmsgs[sizeof(nmsgs)-1] = '\0';

	    /*
	     * Select a folder to save the messages to.
	     */
	    if(save_prompt(ps_global, &cntxt, newfolder, sizeof(newfolder),
			   nmsgs, NULL, 0L, NULL, NULL, NULL)){

		if((cntxt == ourcntxt) && newfolder[0]){
		    rv = 1;
		    strncpy(add_folder, newfolder, len-1);
		    add_folder[len-1] = '\0';
		    free_folder_list(cntxt);
		}

		mn_init(&tmpmap, istream->nmsgs);
		for(l = 1; l <= istream->nmsgs; l++)
		  if(l == 1L)
		    mn_set_cur(tmpmap, l);
		  else
		    mn_add_cur(tmpmap, l);
		
		blank_keymenu(ps_global->ttyo->screen_rows-2, 0);
		we_cancel = busy_cue("Importing messages", NULL, 0);
		l = save(ps_global, istream, cntxt, newfolder, tmpmap,
			 SV_INBOXWOCNTXT);
		if(we_cancel)
		  cancel_busy_cue(0);

		mn_give(&tmpmap);

		if(l == istream->nmsgs)
		  q_status_message2(SM_ORDER, 0, 3,
				    "Folder %s imported to %s",
				    full_filename, newfolder);
		else
		  q_status_message1(SM_ORDER | SM_DING, 3, 3,
				    _("Error importing to %s"),
				    newfolder);
	    }
	}
	else if(istream)
	  q_status_message1(SM_ORDER | SM_DING, 3, 3,
			    _("No messages in file %s"),
			    full_filename);
	else if(!ps_global->mm_log_error)
	  q_status_message1(SM_ORDER | SM_DING, 3, 3,
			    _("Can't open file %s for import"), full_filename);
    }

    if(istream)
      pine_mail_close(istream);
    
    return(rv);
}


int
folder_select_toggle(CONTEXT_S *context, int index, int (*func) (CONTEXT_S *, int))
{
    FOLDER_S *f;

    if((f = folder_entry(index, FOLDERS(context))) != NULL){
      f->selected = !f->selected;
      return((*func)(context, index));
    }
    return 1;
}


/*
 *  Find the next '}' that isn't part of a "${"
 *  that appear for environment variables.  We need
 *  this for configuration, because we want to edit
 *  the original pinerc entry, and not the digested
 *  value.
 */
char *
end_bracket_no_nest(char *str)
{
    char *p;

    for(p = str; *p; p++){
	if(*p == '$' && *(p+1) == '{'){
	    while(*p && *p != '}')
	      p++;
	    if(!*p)
	      return NULL;
	}
	else if(*p == '}')
	  return p;
    }
    /* if we get here then there was no trailing '}' */
    return NULL;
}


/*----------------------------------------------------------------------
      Create a new folder

   Args: context    -- context we're adding in
	 which      -- which config file to operate on
	 varnum     -- which varnum to operate on
         add_folder -- return new folder here
	 add_folderlen -- length of add_folder
	 possible_stream -- possible stream for re-use
	 def        -- default value to start out with for add_folder
			(for now, only for inbox-path editing)

 Result: returns nonzero on successful add, 0 otherwise
  ----*/
int
add_new_folder(CONTEXT_S *context, EditWhich which, int varnum, char *add_folder,
	       size_t add_folderlen, MAILSTREAM *possible_stream, char *def)
{
    char	 tmp[MAX(MAXFOLDER,6*MAX_SCREEN_COLS)+1], nickname[32], 
		*p = NULL, *return_val = NULL, buf[MAILTMPLEN],
		buf2[MAILTMPLEN], def_in_prompt[MAILTMPLEN];
    HelpType     help;
    PINERC_S    *prc = NULL;
    int          i, rc, offset, exists, cnt = 0, isdir = 0;
    int          maildrop = 0, flags = 0, inbox = 0, require_a_subfolder = 0;
    char        *maildropfolder = NULL, *maildroplongname = NULL;
    char        *default_mail_drop_host = NULL,
		*default_mail_drop_folder = NULL,
		*default_dstn_host = NULL,
		*default_dstn_folder = NULL,
		*copydef = NULL,
	        *dstnmbox = NULL;
    char         mdmbox[MAILTMPLEN], ctmp[MAILTMPLEN];
    MAILSTREAM  *create_stream = NULL;
    FOLDER_S    *f;
    static ESCKEY_S add_key[] = {{ctrl('X'),12,"^X", NULL},
				 {-1, 0, NULL, NULL}};

    dprint((4, "\n - add_new_folder - \n"));

    add_folder[0] = '\0';
    nickname[0]   = '\0';
    inbox = (varnum == V_INBOX_PATH);

    if(inbox || context->use & CNTXT_INCMNG){
	char inbox_host[MAXPATH], *beg, *end = NULL;
	int readonly = 0;
	static ESCKEY_S host_key[4];

	if(ps_global->restricted)
	  readonly = 1;
	else{
	    switch(which){
	      case Main:
		prc = ps_global->prc;
		break;
	      case Post:
		prc = ps_global->post_prc;
		break;
	      case None:
		break;
	    }

	    readonly = prc ? prc->readonly : 1;
	}

	if(prc && prc->quit_to_edit){
	    quit_to_edit_msg(prc);
	    return(FALSE);
	}

	if(readonly){
	    q_status_message(SM_ORDER,3,5,
		_("Cancelled: config file not editable"));
	    return(FALSE);
	}

	/*
	 * When adding an Incoming folder we can't supply the inbox host
	 * as a default, because then the user has no way to type just
	 * a plain RETURN to mean "no host, I want a local folder".
	 * So we supply it as a ^X command instead. We could supply it
	 * as the initial value of the string...
	 *
	 * When editing inbox-path we will supply the default value as an
	 * initial value of the string which can be edited. They can edit it
	 * or erase it entirely to mean no host.
	 */

	if(inbox && def){

	    copydef = cpystr(def);
	    (void) removing_double_quotes(copydef);

	    if(check_for_move_mbox(copydef, mdmbox, sizeof(mdmbox), &dstnmbox)){

		/*
		 * Current inbox is using a mail drop. Get the default
		 * host value for the maildrop.
		 */

		if(mdmbox
		   && (mdmbox[0] == '{'
		       || (mdmbox[0] == '*' && mdmbox[1] == '{'))
		   && (end = end_bracket_no_nest(mdmbox+1))
		   && end-mdmbox < add_folderlen){
		    *end = '\0';
		    if(mdmbox[0] == '{')
		      default_mail_drop_host = cpystr(mdmbox+1);
		    else
		      default_mail_drop_host = cpystr(mdmbox+2);
		}

		if(!default_mail_drop_host)
		  default_mail_drop_folder = cpystr(mdmbox);
		else if(end && *(end+1))
		  default_mail_drop_folder = cpystr(end+1);

		end = NULL;
		if(dstnmbox
		   && (*dstnmbox == '{'
		       || (*dstnmbox == '*' && *++dstnmbox == '{'))
		   && (end = end_bracket_no_nest(dstnmbox+1))
		   && end-dstnmbox < add_folderlen){
		    *end = '\0';
		    default_dstn_host = cpystr(dstnmbox+1);
		}

		if(!default_dstn_host)
		  default_dstn_folder = cpystr(dstnmbox);
		else if(end && *(end+1))
		  default_dstn_folder = cpystr(end+1);

		maildrop++;
	    }
	    else{
		end = NULL;
		dstnmbox = copydef;
		if(dstnmbox
		   && (*dstnmbox == '{'
		       || (*dstnmbox == '*' && *++dstnmbox == '{'))
		   && (end = end_bracket_no_nest(dstnmbox+1))
		   && end-dstnmbox < add_folderlen){
		    *end = '\0';
		    default_dstn_host = cpystr(dstnmbox+1);
		}

		if(!default_dstn_host)
		  default_dstn_folder = cpystr(dstnmbox);
		else if(end && *(end+1))
		  default_dstn_folder = cpystr(end+1);
	    }

	    if(copydef)
	      fs_give((void **) &copydef);
	}

get_folder_name:

	i = 0;
	host_key[i].ch      = 0;
	host_key[i].rval    = 0;
	host_key[i].name    = "";
	host_key[i++].label = "";

	inbox_host[0] = '\0';
	if(!inbox && (beg = ps_global->VAR_INBOX_PATH)
	   && (*beg == '{' || (*beg == '*' && *++beg == '{'))
	   && (end = strindex(ps_global->VAR_INBOX_PATH, '}'))){
	    strncpy(inbox_host, beg+1, end - beg);
	    inbox_host[end - beg - 1] = '\0';
	    host_key[i].ch      = ctrl('X');
	    host_key[i].rval    = 12;
	    host_key[i].name    = "^X";
	    host_key[i++].label = N_("Use Inbox Host");
	}
	else{
	    host_key[i].ch      = 0;
	    host_key[i].rval    = 0;
	    host_key[i].name    = "";
	    host_key[i++].label = "";
	}

	if(!maildrop && !maildropfolder){
	    host_key[i].ch      = ctrl('W');
	    host_key[i].rval    = 13;
	    host_key[i].name    = "^W";
	    /* TRANSLATORS: a mail drop is a place where mail is copied to so you
	       can read it. */
	    host_key[i++].label = N_("Use a Mail Drop");
	}
	else if(maildrop){
	    host_key[i].ch      = ctrl('W');
	    host_key[i].rval    = 13;
	    host_key[i].name    = "^W";
	    host_key[i++].label = N_("Do Not use a Mail Drop");
	}

	host_key[i].ch      = -1;
	host_key[i].rval    = 0;
	host_key[i].name    = NULL;
	host_key[i].label = NULL;

	if(maildrop)
	  snprintf(tmp, sizeof(tmp), _("Name of Mail Drop server : "));
	else if(maildropfolder)
	  snprintf(tmp, sizeof(tmp), _("Name of server to contain destination folder : "));
	else if(inbox)
	  snprintf(tmp, sizeof(tmp), _("Name of Inbox server : "));
	else
	  snprintf(tmp, sizeof(tmp), _("Name of server to contain added folder : "));

	tmp[sizeof(tmp)-1] = '\0';

	help = NO_HELP;

	/* set up defaults */
	if(inbox && def){
	    flags = OE_APPEND_CURRENT;
	    if(maildrop && default_mail_drop_host){
		strncpy(add_folder, default_mail_drop_host, add_folderlen);
		add_folder[add_folderlen-1] = '\0';
	    }
	    else if(!maildrop && default_dstn_host){
		strncpy(add_folder, default_dstn_host, add_folderlen);
		add_folder[add_folderlen-1] = '\0';
	    }
	    else
	      add_folder[0] = '\0';
	}
	else{
	    flags = 0;
	    add_folder[0] = '\0';
	}

	while(1){
	    rc = optionally_enter(add_folder, -FOOTER_ROWS(ps_global), 0,
				  add_folderlen, tmp, host_key, help, &flags);
	    removing_leading_and_trailing_white_space(add_folder);

	    /*
	     * User went for the whole enchilada and entered a maildrop
	     * completely without going through the steps.
	     * Split it up as if they did and then skip over
	     * some of the code.
	     */
	    if(check_for_move_mbox(add_folder, mdmbox, sizeof(mdmbox),
				   &dstnmbox)){
		maildrop = 1;
		if(maildropfolder)
		  fs_give((void **) &maildropfolder);

		maildropfolder = cpystr(mdmbox);

		strncpy(add_folder, dstnmbox, add_folderlen);
		add_folder[add_folderlen-1] = '\0';
		offset = 0;
		goto skip_over_folder_input;
	    }

	    /*
	     * Now check to see if they entered a whole c-client
	     * spec that isn't a mail drop.
	     */
	    if(add_folder[0] == '{'
	       && add_folder[1] != '\0'
	       && (p = srchstr(add_folder, "}"))
	       && *(p+1) != '\0'){
		offset = p+1 - add_folder;
		goto skip_over_folder_input;
	    }

	    /*
	     * And check to see if they entered "INBOX".
	     */
	    if(!strucmp(add_folder, ps_global->inbox_name)){
		offset = 0;
		goto skip_over_folder_input;
	    }

	    /* remove surrounding braces */
	    if(add_folder[0] == '{' && add_folder[1] != '\0'){
		char *q;

		q = add_folder + strlen(add_folder) - 1;
		if(*q == '}'){
		    *q = '\0';
		    for(q = add_folder; *q; q++)
		      *q = *(q+1);
		}
	    }

	    if(rc == 3){
		if(maildropfolder && inbox)
		  helper(h_inbox_add_maildrop_destn,
			 _("HELP FOR DESTINATION SERVER "), HLPD_SIMPLE);
		else if(maildropfolder && !inbox)
		  helper(h_incoming_add_maildrop_destn,
			 _("HELP FOR DESTINATION SERVER "), HLPD_SIMPLE);
		else if(maildrop && inbox)
		  helper(h_inbox_add_maildrop, _("HELP FOR MAILDROP NAME "),
		         HLPD_SIMPLE);
		else if(maildrop && !inbox)
		  helper(h_incoming_add_maildrop, _("HELP FOR MAILDROP NAME "),
		         HLPD_SIMPLE);
		else if(inbox)
		  helper(h_incoming_add_inbox, _("HELP FOR INBOX SERVER "),
		         HLPD_SIMPLE);
		else
		  helper(h_incoming_add_folder_host, _("HELP FOR SERVER NAME "),
		         HLPD_SIMPLE);

		ps_global->mangled_screen = 1;
	    }
	    else if(rc == 12){
		strncpy(add_folder, inbox_host, add_folderlen);
		flags |= OE_APPEND_CURRENT;
	    }
	    else if(rc == 13){
		if(maildrop){
		    maildrop = 0;
		    if(maildropfolder)
		      fs_give((void **) &maildropfolder);
		}
		else{
		    maildrop++;
		    if(inbox && def){
			default_mail_drop_host = default_dstn_host;
			default_dstn_host = NULL;
			default_mail_drop_folder = default_dstn_folder;
			default_dstn_folder = NULL;
		    }
		}

		goto get_folder_name;
	    }
	    else if(rc == 1){
		q_status_message(SM_ORDER,0,2,
				 inbox ? _("INBOX change cancelled")
				       : _("Addition of new folder cancelled"));
		if(maildropfolder)
		  fs_give((void **) &maildropfolder);

		if(inbox && def){
		    if(default_mail_drop_host)
		      fs_give((void **) &default_mail_drop_host);

		    if(default_mail_drop_folder)
		      fs_give((void **) &default_mail_drop_folder);

		    if(default_dstn_host)
		      fs_give((void **) &default_dstn_host);

		    if(default_dstn_folder)
		      fs_give((void **) &default_dstn_folder);
		}

		return(FALSE);
	    }
	    else if(rc == 0)
	      break;
	}
    }

    /* set up default folder, if any */
    def_in_prompt[0] = '\0';
    if(inbox && def){
	if(maildrop && default_mail_drop_folder){
	    strncpy(def_in_prompt, default_mail_drop_folder,
		    sizeof(def_in_prompt));
	    def_in_prompt[sizeof(def_in_prompt)-1] = '\0';
	}
	else if(!maildrop && default_dstn_folder){
	    strncpy(def_in_prompt, default_dstn_folder,
		    sizeof(def_in_prompt));
	    def_in_prompt[sizeof(def_in_prompt)-1] = '\0';
	}
    }

    if((offset = strlen(add_folder)) != 0){		/* must be host for incoming */
	int i;
	if(maildrop)
	  snprintf(tmp, sizeof(tmp),
	    "Maildrop folder on \"%s\" to copy mail from%s%s%s : ",
	    short_str(add_folder, buf, sizeof(buf), 15, EndDots),
	    def_in_prompt[0] ? " [" : "",
	    short_str(def_in_prompt, buf2, sizeof(buf2), 20, MidDots),
	    def_in_prompt[0] ? "]" : "");
	else if(maildropfolder)
	  snprintf(tmp, sizeof(tmp),
	    "Folder on \"%s\" to copy mail to%s%s%s : ",
	    short_str(add_folder, buf, sizeof(buf), 20, EndDots),
	    def_in_prompt[0] ? " [" : "",
	    short_str(def_in_prompt, buf2, sizeof(buf2), 20, MidDots),
	    def_in_prompt[0] ? "]" : "");
	else if(inbox)
	  snprintf(tmp, sizeof(tmp),
	    "Folder on \"%s\" to use for INBOX%s%s%s : ",
	    short_str(add_folder, buf, sizeof(buf), 20, EndDots),
	    def_in_prompt[0] ? " [" : "",
	    short_str(def_in_prompt, buf2, sizeof(buf2), 20, MidDots),
	    def_in_prompt[0] ? "]" : "");
	else
	  snprintf(tmp, sizeof(tmp),
	    "Folder on \"%s\" to add%s%s%s : ",
	    short_str(add_folder, buf, sizeof(buf), 25, EndDots),
	    def_in_prompt[0] ? " [" : "",
	    short_str(def_in_prompt, buf2, sizeof(buf2), 20, MidDots),
	    def_in_prompt[0] ? "]" : "");

	tmp[sizeof(tmp)-1] = '\0';

	for(i = offset;i >= 0; i--)
	  add_folder[i+1] = add_folder[i];

	add_folder[0] = '{';
	add_folder[++offset] = '}';
	add_folder[++offset] = '\0';		/* +2, total */
    }
    else{
	if(maildrop)
	  snprintf(tmp, sizeof(tmp),
	    "Folder to copy mail from%s%s%s : ",
	    def_in_prompt[0] ? " [" : "",
	    short_str(def_in_prompt, buf2, sizeof(buf2), 20, MidDots),
	    def_in_prompt[0] ? "]" : "");
	else if(maildropfolder)
	  snprintf(tmp, sizeof(tmp),
	    "Folder to copy mail to%s%s%s : ",
	    def_in_prompt[0] ? " [" : "",
	    short_str(def_in_prompt, buf2, sizeof(buf2), 20, MidDots),
	    def_in_prompt[0] ? "]" : "");
	else if(inbox)
	  snprintf(tmp, sizeof(tmp),
	    "Folder name to use for INBOX%s%s%s : ",
	    def_in_prompt[0] ? " [" : "",
	    short_str(def_in_prompt, buf2, sizeof(buf2), 20, MidDots),
	    def_in_prompt[0] ? "]" : "");
	else
	  snprintf(tmp, sizeof(tmp),
	    "Folder name to add%s%s%s : ",
	    def_in_prompt[0] ? " [" : "",
	    short_str(def_in_prompt, buf2, sizeof(buf2), 20, MidDots),
	    def_in_prompt[0] ? "]" : "");

	tmp[sizeof(tmp)-1] = '\0';
    }

    help = NO_HELP;
    while(1){

	p = NULL;
	if(isdir){
	    add_key[0].label = N_("Create Folder");
	    if(tmp[0] == 'F')
	      rplstr(tmp, sizeof(tmp), 6, N_("Directory"));
	}
	else{
	    add_key[0].label = N_("Create Directory");
	    if(tmp[0] == 'D')
	      rplstr(tmp, sizeof(tmp), 9, N_("Folder"));
	}

	flags = OE_APPEND_CURRENT;
        rc = optionally_enter(&add_folder[offset], -FOOTER_ROWS(ps_global), 0, 
			      add_folderlen - offset, tmp,
			      ((context->dir->delim) && !maildrop)
				  ? add_key : NULL,
			      help, &flags);
	/* use default */
	if(rc == 0 && def_in_prompt[0] && !add_folder[offset]){
	    strncpy(&add_folder[offset], def_in_prompt, add_folderlen-offset);
	    add_folder[add_folderlen-1] = '\0';
	}

	removing_leading_and_trailing_white_space(&add_folder[offset]);

	if(rc == 0 && !(inbox || context->use & CNTXT_INCMNG)
	   && check_for_move_mbox(add_folder, NULL, 0L, NULL)){
	    q_status_message(SM_ORDER, 6, 6,
	  _("#move folders may only be the INBOX or in the Incoming Collection"));
	    display_message(NO_OP_COMMAND);
	    continue;
	}

        if(rc == 0 && add_folder[offset]){
	    if(F_OFF(F_ENABLE_DOT_FOLDERS,ps_global)
	       && add_folder[offset] == '.'){
		if(cnt++ <= 0)
		  q_status_message(SM_ORDER,3,3,
				   _("Folder name can't begin with dot"));
		else
		  q_status_message1(SM_ORDER,3,3,
		   _("Config feature \"%s\" enables names beginning with dot"),
		      pretty_feature_name(feature_list_name(F_ENABLE_DOT_FOLDERS), -1));

                display_message(NO_OP_COMMAND);
                continue;
	    }

	    /* if last char is delim, blast from new folder */
	    for(p = &add_folder[offset]; *p && *(p + 1); p++)
	      ;				/* find last char in folder */

	    if(isdir){
		if(*p && *p != context->dir->delim){
		    *++p   = context->dir->delim;
		    *(p+1) = '\0';
		}

		if(F_ON(F_QUELL_EMPTY_DIRS, ps_global))
		  require_a_subfolder++;
	    }
	    else if(*p == context->dir->delim){
		q_status_message(SM_ORDER|SM_DING, 3, 3,
				 _("Can't have trailing directory delimiters!"));
		display_message('X');
		continue;
	    }

	    break;
	}

	if(rc == 12){
	    isdir = !isdir;		/* toggle directory creation */
	}
        else if(rc == 3){
	    helper(h_incoming_add_folder_name, _("HELP FOR FOLDER NAME "),
		   HLPD_SIMPLE);
	}
	else if(rc == 1 || add_folder[0] == '\0') {
	    q_status_message(SM_ORDER,0,2,
			     inbox ? _("INBOX change cancelled")
				   : _("Addition of new folder cancelled"));
	    if(maildropfolder)
	      fs_give((void **) &maildropfolder);

	    if(inbox && def){
		if(default_mail_drop_host)
		  fs_give((void **) &default_mail_drop_host);

		if(default_mail_drop_folder)
		  fs_give((void **) &default_mail_drop_folder);

		if(default_dstn_host)
		  fs_give((void **) &default_dstn_host);

		if(default_dstn_folder)
		  fs_give((void **) &default_dstn_folder);
	    }

	    return(FALSE);
	}
    }

    if(maildrop && !maildropfolder){
	maildropfolder = cpystr(add_folder);
	maildrop = 0;
	goto get_folder_name;
    }

skip_over_folder_input:

    if(require_a_subfolder){
	/* add subfolder name to directory name */
	offset = strlen(add_folder);
	tmp[0] = '\0';

	if(offset > 0){		/* it had better be */
	    char save_delim;

	    save_delim = add_folder[offset-1];
	    add_folder[offset-1] = '\0';
	
	    snprintf(tmp, sizeof(tmp),
		"Name of subfolder to add in \"%s\" : ",
		short_str(add_folder, buf, sizeof(buf), 15, FrontDots));

	    tmp[sizeof(tmp)-1] = '\0';
	    add_folder[offset-1] = save_delim;
	}

	while(1){
	    flags = OE_APPEND_CURRENT;
	    rc = optionally_enter(&add_folder[offset], -FOOTER_ROWS(ps_global), 0, 
				  add_folderlen - offset, tmp,
				  NULL, NO_HELP, &flags);

	    removing_leading_and_trailing_white_space(&add_folder[offset]);

	    /* use default */
	    if(rc == 0 && !add_folder[offset]){
		q_status_message(SM_ORDER, 4, 4,
		    _("A subfolder name is required, there is no default subfolder name"));
		continue;
	    }

	    if(rc == 0 && add_folder[offset]){
		break;
	    }

	    if(rc == 3){
		helper(h_emptydir_subfolder_name, _("HELP FOR SUBFOLDER NAME "),
		       HLPD_SIMPLE);
	    }
	    else if(rc == 1 || add_folder[0] == '\0') {
		q_status_message(SM_ORDER,0,2, _("Addition of new folder cancelled"));
		return(FALSE);
	    }
	}

	/* the directory is implicit now */
	isdir = 0;
    }

    if(context == ps_global->context_list
       && !(context->dir && context->dir->ref)
       && !strucmp(ps_global->inbox_name, add_folder)){
	q_status_message1(SM_ORDER,3,3,
			  _("Cannot add folder %s in current context"),
			  add_folder);
	return(FALSE);
    }

    create_stream = sp_stream_get(context_apply(ctmp, context, add_folder,
						sizeof(ctmp)),
				  SP_SAME);

    if(!create_stream && possible_stream)
      create_stream = context_same_stream(context, add_folder, possible_stream);

    help = NO_HELP;
    if(!inbox && context->use & CNTXT_INCMNG){
	snprintf(tmp, sizeof(tmp), _("Nickname for folder \"%s\" : "), &add_folder[offset]);
	tmp[sizeof(tmp)-1] = '\0';
	while(1){
	    int flags = OE_APPEND_CURRENT;

	    rc = optionally_enter(nickname, -FOOTER_ROWS(ps_global), 0,
				  sizeof(nickname), tmp, NULL, help, &flags);
	    removing_leading_and_trailing_white_space(nickname);
	    if(rc == 0){
		if(strucmp(ps_global->inbox_name, nickname))
		  break;
		else{
		    q_status_message1(SM_ORDER | SM_DING, 3, 3,
				    _("Nickname cannot be \"%s\""), nickname);
		}
	    }

	    if(rc == 3){
		help = help == NO_HELP
			? h_incoming_add_folder_nickname : NO_HELP;
	    }
	    else if(rc == 1 || (rc != 3 && !*nickname)){
		q_status_message(SM_ORDER,0,2,
				 inbox ? _("INBOX change cancelled")
				       : _("Addition of new folder cancelled"));
		if(maildropfolder)
		  fs_give((void **) &maildropfolder);

		if(inbox && def){
		    if(default_mail_drop_host)
		      fs_give((void **) &default_mail_drop_host);

		    if(default_mail_drop_folder)
		      fs_give((void **) &default_mail_drop_folder);

		    if(default_dstn_host)
		      fs_give((void **) &default_dstn_host);

		    if(default_dstn_folder)
		      fs_give((void **) &default_dstn_folder);
		}

		return(FALSE);
	    }
	}

	/*
	 * Already exist?  First, make sure this name won't collide with
	 * anything else in the list.  Next, quickly test to see if it
	 * the actual mailbox exists so we know any errors from 
	 * context_create() are really bad...
	 */
	for(offset = 0; offset < folder_total(FOLDERS(context)); offset++){
	    f = folder_entry(offset, FOLDERS(context));
	    if(!strucmp(FLDR_NAME(f), nickname[0] ? nickname : add_folder)){
		q_status_message1(SM_ORDER | SM_DING, 0, 3,
				  _("Incoming folder \"%s\" already exists"),
				  nickname[0] ? nickname : add_folder);
		if(maildropfolder)
		  fs_give((void **) &maildropfolder);

		if(inbox && def){
		    if(default_mail_drop_host)
		      fs_give((void **) &default_mail_drop_host);

		    if(default_mail_drop_folder)
		      fs_give((void **) &default_mail_drop_folder);

		    if(default_dstn_host)
		      fs_give((void **) &default_dstn_host);

		    if(default_dstn_folder)
		      fs_give((void **) &default_dstn_folder);
		}

		return(FALSE);
	    }
	}

	ps_global->c_client_error[0] = '\0';
	exists = folder_exists(context, add_folder);
	if(exists == FEX_ERROR){
	    if(ps_global->c_client_error[0] != '\0')
	      q_status_message1(SM_ORDER, 3, 3, "%s",
			        ps_global->c_client_error);
	    else
	      q_status_message1(SM_ORDER, 3, 3, _("Error checking for %s"), add_folder);
	}
    }
    else if(!inbox)
      exists = FEX_NOENT;
    else{
	exists = FEX_ISFILE;
	/*
	 * If inbox is a maildropfolder, try to create the destination
	 * folder. But it shouldn't cause a fatal error.
	 */
	if(maildropfolder && (folder_exists(NULL, add_folder) == FEX_NOENT))
	  context_create(NULL, NULL, add_folder);
    }

    if(exists == FEX_ERROR
       || (exists == FEX_NOENT
	   && !context_create(context, create_stream, add_folder)
	   && !((context->use & CNTXT_INCMNG)
		&& !context_isambig(add_folder)))){
	if(maildropfolder)
	  fs_give((void **) &maildropfolder);

	if(inbox && def){
	    if(default_mail_drop_host)
	      fs_give((void **) &default_mail_drop_host);

	    if(default_mail_drop_folder)
	      fs_give((void **) &default_mail_drop_folder);

	    if(default_dstn_host)
	      fs_give((void **) &default_dstn_host);

	    if(default_dstn_folder)
	      fs_give((void **) &default_dstn_folder);
	}

	return(FALSE);		/* c-client should've reported error */
    }

    if(isdir && p)				/* whack off trailing delim */
      *p = '\0';

    if(inbox || context->use & CNTXT_INCMNG){
	char  **apval;
	char ***alval;

	if(inbox){
	    apval = APVAL(&ps_global->vars[varnum], which);
	    if(apval && *apval)
	      fs_give((void **) apval);
	}
	else{
	    alval = ALVAL(&ps_global->vars[varnum], which);
	    if(!*alval){
		offset = 0;
		*alval = (char **) fs_get(2*sizeof(char *));
	    }
	    else{
		for(offset=0;  (*alval)[offset]; offset++)
		  ;

		fs_resize((void **) alval, (offset + 2) * sizeof(char *));
	    }
	}

	/*
	 * If we're using a Mail Drop we have to assemble the correct
	 * c-client string to do that. Mail drop syntax looks like
	 *
	 *   #move <DELIM> <FromMailbox> <DELIM> <ToMailbox>
	 *
	 * DELIM is any character which does not appear in either of
	 * the mailbox names.
	 *
	 * And then the nickname is still in front of that mess.
	 */
	if(maildropfolder){
	    char *delims = " +-_:!|ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	    char *c;
	    size_t len;

	    len = 5 + 2 + strlen(maildropfolder) + strlen(add_folder);
	    maildroplongname = (char *) fs_get((len+1) * sizeof(char));

	    for(c = delims; *c; c++){
		if(!strindex(maildropfolder, *c) &&
		   !strindex(add_folder, *c))
		  break;
	    }

	    if(*c){
		snprintf(maildroplongname, len+1, "#move%c%s%c%s",
			 *c, maildropfolder, *c, add_folder);
		if(strlen(maildroplongname) < add_folderlen){
		    strncpy(add_folder, maildroplongname, add_folderlen);
		    add_folder[add_folderlen-1] = '\0';
		}
	    }
	    else{
		q_status_message2(SM_ORDER,0,2,
		    "Can't find delimiter for \"#move %s %s\"",
		    maildropfolder, add_folder);
		dprint((2,
		    "Can't find delimiter for \"#move %s %s\"",
		    maildropfolder ? maildropfolder : "?",
		    add_folder ? add_folder : "?"));

		if(maildropfolder)
		  fs_give((void **) &maildropfolder);

		if(inbox && def){
		    if(default_mail_drop_host)
		      fs_give((void **) &default_mail_drop_host);

		    if(default_mail_drop_folder)
		      fs_give((void **) &default_mail_drop_folder);

		    if(default_dstn_host)
		      fs_give((void **) &default_dstn_host);

		    if(default_dstn_folder)
		      fs_give((void **) &default_dstn_folder);
		}

		return(FALSE);
	    }

	    if(maildroplongname)
	      fs_give((void **) &maildroplongname);
	}

	if(inbox)
	  *apval = cpystr(add_folder);
	else{
	    (*alval)[offset]   = put_pair(nickname, add_folder);
	    (*alval)[offset+1] = NULL;
	}

	set_current_val(&ps_global->vars[varnum], TRUE, FALSE);
	if(prc)
	  prc->outstanding_pinerc_changes = 1;

	if(context->use & CNTXT_INCMNG){
	    if(!inbox && add_folder && add_folder[0] && alval && *alval && (*alval)[offset]){
		/*
		 * Instead of re-initing we try to insert the
		 * added folder so that we preserve the last_unseen_update
		 * information.
		 */
		f = new_folder(add_folder, line_hash((*alval)[offset]));
		f->isfolder = 1;
		if(nickname && nickname[0]){
		    f->nickname = cpystr(nickname);
		    f->name_len = strlen(f->nickname);
		}

		if(F_ON(F_ENABLE_INCOMING_CHECKING, ps_global)
		   && !ps_global->VAR_INCCHECKLIST)
		  f->last_unseen_update = LUU_INIT;
		else
		  f->last_unseen_update = LUU_NEVERCHK;

		folder_insert(folder_total(FOLDERS(context)), f, FOLDERS(context));
	    }
	    else
	      /* re-init to make sure we got it right */
	      reinit_incoming_folder_list(ps_global, context);
	}

	if(nickname[0]){
	    strncpy(add_folder, nickname, add_folderlen-1);  /* known by new name */
	    add_folder[add_folderlen-1] = '\0';
	}

	if(!inbox)
	  q_status_message1(SM_ORDER, 0, 3, "Folder \"%s\" created",
			    add_folder);
	return_val = add_folder;
    }
    else if(context_isambig(add_folder)){
	free_folder_list(context);
	q_status_message2(SM_ORDER, 0, 3, "%s \"%s\" created",
			  isdir ? "Directory" : "Folder", add_folder);
	return_val = add_folder;
    }
    else
      q_status_message1(SM_ORDER, 0, 3,
			"Folder \"%s\" created outside current collection",
			add_folder);

    if(maildropfolder)
      fs_give((void **) &maildropfolder);

    if(inbox && def){
	if(default_mail_drop_host)
	  fs_give((void **) &default_mail_drop_host);

	if(default_mail_drop_folder)
	  fs_give((void **) &default_mail_drop_folder);

	if(default_dstn_host)
	  fs_give((void **) &default_dstn_host);

	if(default_dstn_folder)
	  fs_give((void **) &default_dstn_folder);
    }

    return(return_val != NULL);
}


/*----------------------------------------------------------------------
    Subscribe to a news group

   Args: folder -- last folder added
         cntxt  -- The context the subscription is for

 Result: returns the name of the folder subscribed too


This builds a complete context for the entire list of possible news groups. 
It also build a context to find the newly created news groups as 
determined by data kept in .pinerc.  When the find of these new groups is
done the subscribed context is searched and the items marked as new. 
A list of new board is never actually created.

  ----*/
int
group_subscription(char *folder, size_t len, CONTEXT_S *cntxt)
{
    STRLIST_S  *folders = NULL;
    int		rc = 0, last_rc, i, n, flags,
		last_find_partial = 0, we_cancel = 0;
    CONTEXT_S	subscribe_cntxt;
    HelpType	help;
    ESCKEY_S	subscribe_keys[3];

    subscribe_keys[i = 0].ch  = ctrl('T');
    subscribe_keys[i].rval    = 12;
    subscribe_keys[i].name    = "^T";
    subscribe_keys[i++].label = N_("To All Grps");

    if(F_ON(F_ENABLE_TAB_COMPLETE,ps_global)){
	subscribe_keys[i].ch	= ctrl('I');
	subscribe_keys[i].rval  = 11;
	subscribe_keys[i].name  = "TAB";
	subscribe_keys[i++].label = N_("Complete");
    }

    subscribe_keys[i].ch = -1;

    /*---- Build a context to find all news groups -----*/
    
    subscribe_cntxt	      = *cntxt;
    subscribe_cntxt.use      |= CNTXT_FINDALL;
    subscribe_cntxt.use      &= ~(CNTXT_PSEUDO | CNTXT_ZOOM | CNTXT_PRESRV);
    subscribe_cntxt.next      = NULL;
    subscribe_cntxt.prev      = NULL;
    subscribe_cntxt.dir       = new_fdir(cntxt->dir->ref,
					 cntxt->dir->view.internal, '*');
    FOLDERS(&subscribe_cntxt) = init_folder_entries();
    /*
     * Prompt for group name.
     */
    folder[0] = '\0';
    help      = NO_HELP;
    while(1){
	flags = OE_APPEND_CURRENT;
	last_rc = rc;
        rc = optionally_enter(folder, -FOOTER_ROWS(ps_global), 0, len,
			      SUBSCRIBE_PMT, subscribe_keys, help, &flags);
	removing_trailing_white_space(folder);
	removing_leading_white_space(folder);
        if((rc == 0 && folder[0]) || rc == 11 || rc == 12){
	    we_cancel = busy_cue(_("Fetching newsgroup list"), NULL, 1);

	    if(last_find_partial){
		/* clean up any previous find results */
		free_folder_list(&subscribe_cntxt);
		last_find_partial = 0;
	    }

	    if(rc == 11){			/* Tab completion! */
		if(folder_complete_internal(&subscribe_cntxt,
					    folder, len, &n, FC_FORCE_LIST)){
		    continue;
		}
		else{
		    if(!(n && last_rc == 11 && !(flags & OE_USER_MODIFIED))){
			Writechar(BELL, 0);
			continue;
		    }
		}
	    }

	    if(rc == 12){			/* list the whole enchilada */
		build_folder_list(NULL, &subscribe_cntxt, NULL, NULL,BFL_NONE);
	    }
	    else if(strlen(folder)){
		char tmp[MAILTMPLEN];

		snprintf(tmp, sizeof(tmp), "%s%.*s*", (rc == 11) ? "" : "*",
			sizeof(tmp)-3, folder);
		tmp[sizeof(tmp)-1] = '\0';
		build_folder_list(NULL, &subscribe_cntxt, tmp, NULL, BFL_NONE);
		subscribe_cntxt.dir->status &= ~(CNTXT_PARTFIND|CNTXT_NOFIND);
	    }
	    else{
		q_status_message(SM_ORDER, 0, 2,
	       _("No group substring to match! Use ^T to list all news groups."));
		continue;
	    }

	    /*
	     * If we did a partial find on matches, then we faked a full
	     * find which will cause this to just return.
	     */
	    if((i = folder_total(FOLDERS(&subscribe_cntxt))) != 0){
		char *f;

		/*
		 * fake that we've found everything there is to find...
		 */
		subscribe_cntxt.use &= ~(CNTXT_NOFIND|CNTXT_PARTFIND);
		last_find_partial = 1;

		if(i == 1){
		    f = folder_entry(0, FOLDERS(&subscribe_cntxt))->name;
		    if(!strcmp(f, folder)){
			rc = 1;			/* success! */
			break;
		    }
		    else{			/* else complete the group */
			strncpy(folder, f, len-1);
			folder[len-1] = '\0';
			continue;
		    }
		}
		else if(!(flags & OE_USER_MODIFIED)){
		    /*
		     * See if there wasn't an exact match in the lot.
		     */
		    while(i-- > 0){
			f = folder_entry(i,FOLDERS(&subscribe_cntxt))->name;
			if(!strcmp(f, folder))
			  break;
			else
			  f = NULL;
		    }

		    /* if so, then the user picked it from the list the
		     * last time and didn't change it at the prompt.
		     * Must mean they're accepting it...
		     */
		    if(f){
			rc = 1;			/* success! */
			break;
		    }
		}
	    }
	    else{
		if(rc == 12)
		  q_status_message(SM_ORDER | SM_DING, 3, 3,
				   _("No groups to select from!"));
		else
		  q_status_message1(SM_ORDER, 3, 3,
			  _("News group \"%s\" didn't match any existing groups"),
			  folder);
		free_folder_list(&subscribe_cntxt);

		continue;
	    }

	    /*----- Mark groups that are currently subscribed too ------*/
	    /* but first make sure they're found */
	    build_folder_list(NULL, cntxt, "*", NULL, BFL_LSUB);
	    for(i = 0 ; i < folder_total(FOLDERS(&subscribe_cntxt)); i++) {
		FOLDER_S *f = folder_entry(i, FOLDERS(&subscribe_cntxt));

		f->subscribed = search_folder_list(FOLDERS(cntxt),
						   f->name) != 0;
	    }

	    if(we_cancel)
	      cancel_busy_cue(-1);

	    /*----- Call the folder lister to do all the work -----*/
	    folders = folders_for_subscribe(ps_global,
					    &subscribe_cntxt, folder);

	    if(folders){
		/* Multiple newsgroups OR Auto-select */
		if(folders->next || F_ON(F_SELECT_WO_CONFIRM,ps_global))
		  break;

		strncpy(folder, (char *) folders->name, len-1);
		folder[len-1] = '\0';
		free_strlist(&folders);
	    }
	}
        else if(rc == 3){
            help = help == NO_HELP ? h_news_subscribe : NO_HELP;
	}
	else if(rc == 1 || folder[0] == '\0'){
	    rc = -1;
	    break;
	}
    }

    if(rc < 0){
	folder[0] = '\0';		/* make sure not to return partials */
	if(rc == -1)
	  q_status_message(SM_ORDER, 0, 3, _("Subscribe cancelled"));
    }
    else{
	MAILSTREAM *sub_stream;
	int sclose = 0, errors = 0;

	if(folders){		/*------ Actually do the subscription -----*/
	    STRLIST_S *flp;
	    int	       n = 0;

	    /* subscribe one at a time */
	    folder[0] = '\0';
	    /*
	     * Open stream before subscribing so c-client knows what newsrc
	     * to use, along with other side-effects.
	     */
	    if((sub_stream = mail_cmd_stream(&subscribe_cntxt, &sclose)) != NULL){
		for(flp = folders; flp; flp = flp->next){
		    (void) context_apply(tmp_20k_buf, &subscribe_cntxt,
					 (char *) flp->name, SIZEOF_20KBUF);
		    if(mail_subscribe(sub_stream, tmp_20k_buf) == 0L){
			/*
			 * This message may not make it to the screen,
			 * because a c-client message about the failure
			 * will be there.  Probably best not to string
			 * together a whole bunch of errors if there is
			 * something wrong.
			 */
			q_status_message1(errors ?SM_INFO : SM_ORDER,
					  errors ? 0 : 3, 3,
					  _("Error subscribing to \"%s\""),
					  (char *) flp->name);
			errors++;
		    }
		    else{
			n++;
			if(!folder[0]){
			    strncpy(folder, (char *) flp->name, len-1);
			    folder[len-1] = '\0';
			}

			/*---- Update the screen display data structures -----*/
			if(ALL_FOUND(cntxt)){
			    if(cntxt->use & CNTXT_PSEUDO){
				folder_delete(0, FOLDERS(cntxt));
				cntxt->use &= ~CNTXT_PSEUDO;
			    }

			    folder_insert(-1, new_folder((char *) flp->name, 0),
					  FOLDERS(cntxt));
			}
		    }
		}
		if(sclose)
		  pine_mail_close(sub_stream);
	    }
	    else
	      errors++;

	    if(n == 0)
	      q_status_message(SM_ORDER | SM_DING, 3, 5,
			  _("Subscriptions failed, subscribed to no new groups"));
	    else
	      q_status_message3(SM_ORDER | (errors ? SM_DING : 0),
				errors ? 3 : 0,3,
				"Subscribed to %s new groups%s%s",
				comatose((long)n),
				errors ? ", failed on " : "",
				errors ? comatose((long)errors) : "");

	    free_strlist(&folders);
	}
	else{
	    if((sub_stream = mail_cmd_stream(&subscribe_cntxt, &sclose)) != NULL){
		(void) context_apply(tmp_20k_buf, &subscribe_cntxt, folder,
				     SIZEOF_20KBUF);
		if(mail_subscribe(sub_stream, tmp_20k_buf) == 0L){
		    q_status_message1(SM_ORDER | SM_DING, 3, 3,
				      _("Error subscribing to \"%s\""), folder);
		    errors++;
		}
		else if(ALL_FOUND(cntxt)){
		    /*---- Update the screen display data structures -----*/
		    if(cntxt->use & CNTXT_PSEUDO){
			folder_delete(0, FOLDERS(cntxt));
			cntxt->use &= ~CNTXT_PSEUDO;
		    }

		    folder_insert(-1, new_folder(folder, 0), FOLDERS(cntxt));
		}
		if(sclose)
		  pine_mail_close(sub_stream);
	    }
	    else{
		q_status_message1(SM_ORDER | SM_DING, 3, 3,
				  _("Error subscribing to \"%s\""), folder);
		errors++;
	    }
	}

	if(!errors && folder[0])
	  q_status_message1(SM_ORDER, 0, 3, _("Subscribed to \"%s\""), folder);
    }

    free_fdir(&subscribe_cntxt.dir, 1);
    return(folder[0]);
}


/*----------------------------------------------------------------------
      Rename folder
  
   Args: new_name   -- buffer to contain new file name
         len        -- length of new_name buffer
         index      -- index of folder in folder list to rename
         context    -- collection of folders making up folder list
	possible_stream -- may be able to use this stream

 Result: returns the new name of the folder, or NULL if nothing happened.

 When either the sent-mail or saved-message folders are renamed, immediately 
create a new one in their place so they always exist. The main loop above also
detects this and makes the rename look like an add of the sent-mail or
saved-messages folder. (This behavior may not be optimal, but it keeps things
consistent.

  ----*/
int
rename_folder(CONTEXT_S *context, int index, char *new_name, size_t len, MAILSTREAM *possible_stream)
{
    char        *folder, prompt[64], *name_p = NULL;
    HelpType     help;
    FOLDER_S	*new_f;
    PINERC_S    *prc = NULL;
    int          rc, ren_cur, cnt = 0, isdir = 0, readonly = 0;
    EditWhich    ew;
    MAILSTREAM  *strm;

    dprint((4, "\n - rename folder -\n"));

    if(NEWS_TEST(context)){
	q_status_message(SM_ORDER | SM_DING, 3, 3,
			 _("Can't rename news groups!"));
	return(0);
    }
    else if(!folder_total(FOLDERS(context))){
	q_status_message(SM_ORDER | SM_DING, 0, 4,
			 _("Empty folder collection.  No folder to rename!"));
	return(0);
    }
    else if((new_f = folder_entry(index, FOLDERS(context)))
	    && context == ps_global->context_list
	    && !(context->dir && context->dir->ref)
	    && !strucmp(FLDR_NAME(new_f), ps_global->inbox_name)) {
        q_status_message1(SM_ORDER | SM_DING, 3, 4,
			  _("Can't change special folder name \"%s\""),
			  ps_global->inbox_name);
        return(0);
    }

    ew = config_containing_inc_fldr(new_f);
    if(ps_global->restricted)
      readonly = 1;
    else{
	switch(ew){
	  case Main:
	    prc = ps_global->prc;
	    break;
	  case Post:
	    prc = ps_global->post_prc;
	    break;
	  case None:
	    break;
	}

	readonly = prc ? prc->readonly : 1;
    }

    if(prc && prc->quit_to_edit && (context->use & CNTXT_INCMNG)){
	quit_to_edit_msg(prc);
	return(0);
    }

    if(readonly && (context->use & CNTXT_INCMNG)){
	q_status_message(SM_ORDER,3,5,
		     _("Rename cancelled: folder not in editable config file"));
	return(0);
    }

    if(context->use & CNTXT_INCMNG){
	if(!(folder = new_f->nickname))
	  folder = "";		/* blank nickname */
    }
    else
      folder = FLDR_NAME(new_f);

    ren_cur = strcmp(folder, ps_global->cur_folder) == 0;

    snprintf(prompt, sizeof(prompt), "%s %s to : ", _("Rename"),
	    (context->use & CNTXT_INCMNG)
	      ? _("nickname")
	      : (isdir = new_f->isdir)
		  ? _("directory") : _("folder"));
    prompt[sizeof(prompt)-1] = '\0';
    help   = NO_HELP;
    strncpy(new_name, folder, len-1);
    new_name[len-1] = '\0';
    while(1) {
	int flags = OE_APPEND_CURRENT;

        rc = optionally_enter(new_name, -FOOTER_ROWS(ps_global), 0,
			      len, prompt, NULL, help, &flags);
        if(rc == 3) {
            help = help == NO_HELP ? h_oe_foldrename : NO_HELP;
            continue;
        }

	removing_leading_and_trailing_white_space(new_name);

        if(rc == 0 && (*new_name || (context->use & CNTXT_INCMNG))) {
	    /* verify characters */
	    if(F_OFF(F_ENABLE_DOT_FOLDERS,ps_global) && *new_name == '.'){
		if(cnt++ <= 0)
                  q_status_message(SM_ORDER,3,3,
		    _("Folder name can't begin with dot"));
		else
		  q_status_message1(SM_ORDER,3,3,
		      _("Config feature \"s\" enables names beginning with dot"),
		      pretty_feature_name(feature_list_name(F_ENABLE_DOT_FOLDERS), -1));

                display_message(NO_OP_COMMAND);
                continue;
	    }

	    if(folder_index(new_name, context, FI_ANY|FI_RENAME) >= 0){
                q_status_message1(SM_ORDER, 3, 3,
				  _("Folder \"%s\" already exists"),
                                  pretty_fn(new_name));
                display_message(NO_OP_COMMAND);
                continue;
            }
	    else if(context == ps_global->context_list
		    && !(context->dir && context->dir->ref)
		    && !strucmp(new_name, ps_global->inbox_name)){
		if(context->use & CNTXT_INCMNG)
                  q_status_message1(SM_ORDER, 3, 3, _("Can't rename incoming folder to %s"),
				    new_name);
		else
                  q_status_message1(SM_ORDER, 3, 3, _("Can't rename folder to %s"),
				    new_name);

                display_message(NO_OP_COMMAND);
                continue;
	    }
        }

        if(rc != 4) /* redraw */
	  break;  /* no redraw */

    }

    if(rc != 1 && isdir){		/* add trailing delim? */
	for(name_p = new_name; *name_p && *(name_p+1) ; name_p++)
	  ;

	if(*name_p == context->dir->delim) /* lop off delim */
	  *name_p = '\0';
    }

    if(rc == 1
       || !(*new_name || (context->use & CNTXT_INCMNG))
       || !strcmp(new_name, folder)){
        q_status_message(SM_ORDER, 0, 2, _("Folder rename cancelled"));
        return(0);
    }

    if(context->use & CNTXT_INCMNG){
	char **new_list, **lp, ***alval;
	int   i;

	alval = ALVAL(&ps_global->vars[V_INCOMING_FOLDERS], ew);
	for(i = 0; (*alval)[i]; i++)
	  ;

	new_list = (char **) fs_get((i + 1) * sizeof(char *));

	for(lp = new_list, i = 0; (*alval)[i]; i++){
	  /* figure out if this is the one we're renaming */
	  expand_variables(tmp_20k_buf, SIZEOF_20KBUF, (*alval)[i], 0);
	  if(new_f->varhash == line_hash(tmp_20k_buf)){
	      char *folder_string = NULL, *nickname = NULL;

	      if(new_f->nickname)
		fs_give((void **) &new_f->nickname);

	      if(*new_name)
		new_f->nickname = cpystr(new_name);

	      /*
	       * Parse folder line for nickname and folder name.
	       * No nickname on line is OK.
	       */
	      get_pair(tmp_20k_buf, &nickname, &folder_string, 0, 0);

	      if(nickname)
		fs_give((void **)&nickname);

	      *lp = put_pair(new_name, folder_string);

	      new_f->varhash = line_hash(*lp++);
	  }
	  else
	    *lp++ = cpystr((*alval)[i]);
	}

	*lp = NULL;

	set_variable_list(V_INCOMING_FOLDERS, new_list, TRUE, ew);
	free_list_array(&new_list);

	return(1);
    }

    /* Can't rename open streams */
    if((strm = context_already_open_stream(context, folder, AOS_NONE))
       || (ren_cur && (strm=ps_global->mail_stream))){
	if(possible_stream == strm)
	  possible_stream = NULL;

	pine_mail_actually_close(strm);
    }

    if(possible_stream
       && !context_same_stream(context, new_name, possible_stream))
      possible_stream = NULL;
      
    if((rc = context_rename(context, possible_stream, folder, new_name)) != 0){
	if(name_p && *name_p == context->dir->delim)
	  *name_p = '\0';		/* blat trailing delim */

	/* insert new name? */
	if(!strindex(new_name, context->dir->delim)){
	    new_f	     = new_folder(new_name, 0);
	    new_f->isdir     = isdir;
	    folder_insert(-1, new_f, FOLDERS(context));
	}

	if(strcmp(ps_global->VAR_DEFAULT_FCC, folder) == 0
	   || strcmp(ps_global->VAR_DEFAULT_SAVE_FOLDER, folder) == 0) {
	    /* renaming sent-mail or saved-messages */
	    if(context_create(context, NULL, folder)){
		q_status_message3(SM_ORDER,0,3,
	      "Folder \"%s\" renamed to \"%s\". New \"%s\" created",
				  folder, new_name,
				  pretty_fn(
				    (strcmp(ps_global->VAR_DEFAULT_SAVE_FOLDER,
					    folder) == 0)
				    ? ps_global->VAR_DEFAULT_SAVE_FOLDER
				    : ps_global->VAR_DEFAULT_FCC));

	    }
	    else{
		q_status_message1(SM_ORDER | SM_DING, 3, 4,
				  "Error creating new \"%s\"", folder);

		dprint((1, "Error creating \"%s\" in %s context\n",
			   folder ? folder : "?",
			   context->context ? context->context : "?"));
	    }
	}
	else
	  q_status_message2(SM_ORDER, 0, 3,
			    "Folder \"%s\" renamed to \"%s\"",
			    pretty_fn(folder), pretty_fn(new_name));

	free_folder_list(context);
    }

    if(ren_cur) {
	if(ps_global && ps_global->ttyo){
	    blank_keymenu(ps_global->ttyo->screen_rows - 2, 0);
	    ps_global->mangled_footer = 1;
	}

	/* No reopen the folder we just had open */
        do_broach_folder(new_name, context, NULL, 0L);
    }

    return(rc);
}


/*----------------------------------------------------------------------
   Confirm and delete a folder

   Args: fs -- folder screen state

 Result: return 0 if not delete, 1 if deleted.
  ----*/
int
delete_folder(CONTEXT_S *context, int index, char *next_folder, size_t len, MAILSTREAM **possible_streamp)
{
    char       *folder, ques_buf[MAX_SCREEN_COLS+1], *target = NULL,
	       *fnamep, buf[1000];
    MAILSTREAM *del_stream = NULL, *sub_stream, *strm = NULL;
    FOLDER_S   *fp;
    EditWhich   ew;
    PINERC_S   *prc = NULL;
    int         ret, unsub_opened = 0, close_opened = 0, blast_folder = 1,
		readonly;

    if(!context){
	cmd_cancelled("Missing context in Delete");
	return(0);
    }

    if(NEWS_TEST(context)){
        folder = folder_entry(index, FOLDERS(context))->name;
        snprintf(ques_buf, sizeof(ques_buf), _("Really unsubscribe from \"%s\""), folder);
	ques_buf[sizeof(ques_buf)-1] = '\0';
    
        ret = want_to(ques_buf, 'n', 'x', NO_HELP, WT_NORM);
        switch(ret) {
          /* ^C */
          case 'x':
            Writechar(BELL, 0);
            /* fall through */
          case 'n':
            return(0);
        }
    
        dprint((2, "deleting folder \"%s\" in context \"%s\"\n",
	       folder ? folder : "?",
	       context->context ? context->context : "?"));

	if((sub_stream = mail_cmd_stream(context, &unsub_opened)) != NULL){
	    (void) context_apply(tmp_20k_buf, context, folder, SIZEOF_20KBUF);
	    if(!mail_unsubscribe(sub_stream, tmp_20k_buf)){
		q_status_message1(SM_ORDER | SM_DING, 3, 3,
				  _("Error unsubscribing from \"%s\""),folder);
		if(unsub_opened)
		  pine_mail_close(sub_stream);
		return(0);
	    }
	    if(unsub_opened)
	      pine_mail_close(sub_stream);
	}

	/*
	 * Fix  up the displayed list
	 */
	folder_delete(index, FOLDERS(context));
	return(1);
    }

    fp = folder_entry(index, FOLDERS(context));
    if(!fp){
	cmd_cancelled("Can't find folder to Delete!");
	return(0);
    }

    if(!((context->use & CNTXT_INCMNG) && fp->name
         && check_for_move_mbox(fp->name, NULL, 0, &target))){
	target = NULL;
    }

    folder = FLDR_NAME(fp);
    dprint((4, "=== delete_folder(%s) ===\n",
	   folder ? folder : "?"));

    ew = config_containing_inc_fldr(fp);
    if(ps_global->restricted)
      readonly = 1;
    else{
	switch(ew){
	  case Main:
	    prc = ps_global->prc;
	    break;
	  case Post:
	    prc = ps_global->post_prc;
	    break;
	  case None:
	    break;
	}

	readonly = prc ? prc->readonly : 1;
    }

    if(prc && prc->quit_to_edit && (context->use & CNTXT_INCMNG)){
	quit_to_edit_msg(prc);
	return(0);
    }

    if(context == ps_global->context_list
       && !(context->dir && context->dir->ref)
       && strucmp(folder, ps_global->inbox_name) == 0){
	q_status_message1(SM_ORDER | SM_DING, 3, 4,
		 _("Can't delete special folder \"%s\"."),
		 ps_global->inbox_name);
	return(0);
    }
    else if(readonly && (context->use & CNTXT_INCMNG)){
	q_status_message(SM_ORDER,3,5,
		     _("Deletion cancelled: folder not in editable config file"));
	return(0);
    }
    else if((fp->name
	     && (strm=context_already_open_stream(context,fp->name,AOS_NONE)))
	    ||
	    (target
	     && (strm=context_already_open_stream(NULL,target,AOS_NONE)))){
	if(strm == ps_global->mail_stream)
	  close_opened++;
    }
    else if(fp->isdir || fp->isdual){	/* NO DELETE if directory isn't EMPTY */
	FDIR_S *fdirp = next_folder_dir(context,folder,TRUE,possible_streamp);

	if(fp->haschildren)
	  ret = 1;
	else if(fp->hasnochildren)
	  ret = 0;
	else{
	    ret = folder_total(fdirp->folders) > 0;
	    free_fdir(&fdirp, 1);
	}

	if(ret){
	    q_status_message2(SM_ORDER | SM_DING, 3, 4,
			  _("Can't delete non-empty directory \"%s\"%s."),
			  folder, (fp->isfolder && fp->isdual) ? " (or folder of same name)" : "");
	    return(0);
	}

	/*
	 * Folder by the same name exist?  If so, server's probably going
	 * to delete it as well.  Punt?
	 */
	if(fp->isdual
	   && (ret = want_to(DIR_FOLD_PMT,'n','x',NO_HELP,WT_NORM)) != 'y'){
	    q_status_message(SM_ORDER,0,3, (ret == 'x') ? _("Delete cancelled") 
			     : _("No folder deleted"));
	    return(0);
	}
    }

    if(context->use & CNTXT_INCMNG){
	static ESCKEY_S delf_opts[] = {
	    {'n', 'n', "N", N_("Nickname only")},
	    {'b', 'b', "B", N_("Both Folder and Nickname")},
	    {-1, 0, NULL, NULL}
	};
#define	DELF_PROMPT	_("DELETE only Nickname or Both nickname and folder? ")

	switch(radio_buttons(DELF_PROMPT, -FOOTER_ROWS(ps_global),
			     delf_opts,'n','x',NO_HELP,RB_NORM)){
	  case 'n' :
	    blast_folder = 0;
	    break;

	  case 'x' :
	    cmd_cancelled("Delete");
	    return(0);

	  default :
	    break;
	}
    }
    else{
	snprintf(ques_buf, sizeof(ques_buf), "DELETE \"%s\"%s", folder, 
		close_opened ? " (the currently open folder)" :
	  (fp->isdir && !(fp->isdual || fp->isfolder
			  || (folder_index(folder, context, FI_FOLDER) >= 0)))
			      ? " (a directory)" : "");
	ques_buf[sizeof(ques_buf)-1] = '\0';

	if((ret = want_to(ques_buf, 'n', 'x', NO_HELP, WT_NORM)) != 'y'){
	    q_status_message(SM_ORDER,0,3, (ret == 'x') ? _("Delete cancelled") 
			     : _("Nothing deleted"));
	    return(0);
	}
    }

    if(blast_folder){
	dprint((2,"deleting \"%s\" (%s) in context \"%s\"\n",
		   fp->name ? fp->name : "?",
		   fp->nickname ? fp->nickname : "",
		   context->context ? context->context : "?"));
	if(strm){
	    /*
	     * Close it, NULL the pointer, and let do_broach_folder fixup
	     * the rest...
	     */
	    pine_mail_actually_close(strm);
	    if(close_opened){
		if(ps_global && ps_global->ttyo){
		    blank_keymenu(ps_global->ttyo->screen_rows - 2, 0);
		    ps_global->mangled_footer = 1;
		}

		ps_global->mangled_header = 1;
		do_broach_folder(ps_global->inbox_name,
				 ps_global->context_list, NULL, DB_INBOXWOCNTXT);
	    }
	}

	/*
	 * Use fp->name since "folder" may be a nickname...
	 */
	if(possible_streamp && *possible_streamp
	   && context_same_stream(context, fp->name, *possible_streamp))
	  del_stream = *possible_streamp;

	fnamep = fp->name;

	if(!context_delete(context, del_stream, fnamep)){
/*
 * BUG: what if sent-mail or saved-messages????
 */
	    q_status_message1(SM_ORDER,3,3,"Delete of \"%s\" Failed!", folder);
	    return(0);
	}
    }

    snprintf(buf, sizeof(buf), "%s\"%s\" deleted.",
	    !blast_folder               ? "Nickname "          :
	     fp->isdual                  ? "Folder/Directory " :
	      (fp->isdir && fp->isfolder) ? "Folder "          :
	       fp->isdir                   ? "Directory "      :
					      "Folder ",
	    folder);
    buf[sizeof(buf)-1] = '\0';

    q_status_message(SM_ORDER, 0, 3, buf);

    if(context->use & CNTXT_INCMNG){
	char **new_list, **lp, ***alval;
	int   i;

	alval = ALVAL(&ps_global->vars[V_INCOMING_FOLDERS], ew);
	for(i = 0; (*alval)[i]; i++)
	  ;

	/*
	 * Make it one too big in case we don't find the match for
	 * some unknown reason.
	 */
	new_list = (char **) fs_get((i + 1) * sizeof(char *));

	/*
	 * Copy while figuring out which one to skip.
	 */
	for(lp = new_list, i = 0; (*alval)[i]; i++){
	    expand_variables(tmp_20k_buf, SIZEOF_20KBUF, (*alval)[i], 0);
	    if(fp->varhash != line_hash(tmp_20k_buf))
	      *lp++ = cpystr((*alval)[i]);
	}

	*lp = NULL;

	set_variable_list(V_INCOMING_FOLDERS, new_list, TRUE, ew);
	free_list_array(&new_list);
    }

    /*
     * Fix up the displayed list.
     */
    folder_delete(index, FOLDERS(context));

    /*
     * Take a guess at what should get hilited next.
     */
    if(index < (ret = folder_total(FOLDERS(context)))
       || ((index = ret - 1) >= 0)){
	if((fp = folder_entry(index, FOLDERS(context)))
	   && strlen(FLDR_NAME(fp)) < len - 1)
	  strncpy(next_folder, FLDR_NAME(fp), len-1);
	  next_folder[len-1] = '\0';
    }

    if(!(context->use & CNTXT_INCMNG)){
	/*
	 * Then cause the list to get rebuild 'cause we may've blasted
	 * some folder that's also a directory or vice versa...
	 */
	free_folder_list(context);
    }

    return(1);
}


/*----------------------------------------------------------------------
      Print the list of folders on paper

   Args: list    --  The current list of folders
         lens    --  The list of lengths of the current folders
         display --  The current folder display structure

 Result: list printed on paper

If the display list was created for 80 columns it is used, otherwise
a new list is created for 80 columns

  ----*/
void
print_folders(FPROC_S *fp)
{
    if(open_printer(_("folder list")) == 0){
	(void) folder_list_text(ps_global, fp, print_char, NULL, 80);

	close_printer();
    }
}
                     

int
scan_get_pattern(char *kind, char *pat, int len)
{
    char prompt[256];
    int  flags;

    pat[0] = '\0';
    snprintf(prompt, sizeof(prompt), _("String in folder %s to match : "), kind);
    prompt[sizeof(prompt)-1] = '\0';

    while(1){
	flags = OE_APPEND_CURRENT | OE_DISALLOW_HELP;
	switch(optionally_enter(pat, -FOOTER_ROWS(ps_global), 0, len,
				prompt, NULL, NO_HELP, &flags)){

	  case 4 :
	    if(ps_global->redrawer)
	      (*ps_global->redrawer)();

	    break;

	  case 0 :
	    if(*pat)
	      return(1);

	  case 1 :
	    cmd_cancelled("Select");

	  default :
	    return(0);
	}
    }
}


int
folder_select_text(struct pine *ps, CONTEXT_S *context, int selected)
{
    char	     pattern[MAILTMPLEN], type = '\0';
    static ESCKEY_S  scan_opts[] = {
	{'n', 'n', "N", N_("Name Select")},
	{'c', 'c', "C", N_("Content Select")},
	{-1, 0, NULL, NULL}
    };

    if(context->use & CNTXT_INCMNG){
	q_status_message(SM_ORDER | SM_DING, 3, 3,
		     _("Select \"Text\" not supported in Incoming Folders"));
	return(0);
    }

    switch(radio_buttons(SEL_TEXT_PMT, -FOOTER_ROWS(ps_global),
			 scan_opts, 'n', 'x', NO_HELP, RB_NORM)){
      case 'n' :				/* Name search */
	if(scan_get_pattern("NAME", pattern, sizeof(pattern)))
	  type = 'n';

	break;

      case 'c' :				/* content search */
	if(scan_get_pattern("CONTENTS", pattern, sizeof(pattern)))
	  type = 'c';

	break;

      case 'x' :
      default :
	break;
    }

    if(type){
	int	  rv;
	char	  tmp[MAILTMPLEN];
	SCANARG_S args;

	memset(&args, 0, sizeof(SCANARG_S));
	args.pattern  = pattern;
	args.type     = type;
	args.context  = context;

	if(type == 'c'){
	    args.stream = sp_stream_get(context_apply(tmp, context,
						      "xxx", sizeof(tmp)),
					SP_SAME);
	    if(!args.stream){
		args.stream = pine_mail_open(NULL, tmp,
				 OP_SILENT|OP_HALFOPEN|SP_USEPOOL|SP_TEMPUSE,
					     NULL);
		args.newstream = (args.stream != NULL);
	    }
	}

	rv = foreach_folder(context, selected,
			    foreach_do_scan, (void *) &args);

	if(args.newstream)
	  pine_mail_close(args.stream);

	if(rv)
	  return(1);
    }

    cmd_cancelled("Select");
    return(0);
}


int
foreach_do_scan(FOLDER_S *f, void *d)
{
    SCANARG_S *sa = (SCANARG_S *) d;

    return((sa->type == 'n' && srchstr(FLDR_NAME(f), sa->pattern))
	   || (sa->type == 'c'
	       && scan_scan_folder(sa->stream, sa->context, f, sa->pattern)));
}


int
scan_scan_folder(MAILSTREAM *stream, CONTEXT_S *context, FOLDER_S *f, char *pattern)
{
    MM_LIST_S  ldata;
    LISTRES_S  response;
    int        we_cancel = 0;
    char      *folder, *ref = NULL, tmp[MAILTMPLEN];

    folder = f->name;
    snprintf(tmp, sizeof(tmp), "Scanning \"%s\"", FLDR_NAME(f));
    tmp[sizeof(tmp)-1] = '\0';
    we_cancel = busy_cue(tmp, NULL, 1);

    mm_list_info	      = &ldata;		/* tie down global reference */
    memset(&ldata, 0, sizeof(MM_LIST_S));
    ldata.filter = mail_list_response;
    memset(ldata.data = &response, 0, sizeof(LISTRES_S));

    /*
     * If no preset reference string, must be at top of context
     */
    if(context && context_isambig(folder) && !(ref = context->dir->ref)){
	char *p;

	if((p = strstr(context->context, "%s")) != NULL){
	    if(!*(p+2)){
		snprintf(tmp, sizeof(tmp), "%.*s", MIN(p - context->context, sizeof(tmp)-1),
			context->context);
		tmp[sizeof(tmp)-1] = '\0';
		ref = tmp;
	    }
	    else{
		snprintf(tmp, sizeof(tmp), context->context, folder);
		tmp[sizeof(tmp)-1] = '\0';
		folder = tmp;
		ref    = "";
	    }
	}
	else
	  ref = context->context;
    }

    mail_scan(stream, ref, folder, pattern);

    if(context && context->dir && response.delim)
      context->dir->delim = response.delim;

    if(we_cancel)
      cancel_busy_cue(-1);

    return(((response.isfile) ? FEX_ISFILE : 0)
	   | ((response.isdir) ? FEX_ISDIR : 0));
}


int
folder_select_props(struct pine *ps, CONTEXT_S *context, int selected)
{
    int		       cmp = 0;
    long	       flags = 0L, count;
    static ESCKEY_S    prop_opts[] = {
	{'u', 'u', "U", N_("Unseen msgs")},
	{'n', 'n', "N", N_("New msgs")},
	{'c', 'c', "C", N_("msg Count")},
	{-1, 0, NULL, NULL}
    };

    if(context->use & CNTXT_INCMNG){
	q_status_message(SM_ORDER | SM_DING, 3, 3,
		     _("Select \"Properties\" not supported in Incoming Folders"));
	return(0);
    }

    switch(radio_buttons(SEL_PROP_PMT, -FOOTER_ROWS(ps_global),
			 prop_opts, 'n', 'x', h_folder_prop, RB_NORM)){
      case 'c' :				/* message count */
	if(folder_select_count(&count, &cmp))
	  flags = SA_MESSAGES;

	break;

      case 'n' :				/* folders with new */
	flags = SA_RECENT;
	break;

      case 'u' :				/* folders with unseen */
	flags = SA_UNSEEN;
	break;

      case 'x' :
      default :
	break;
    }

    if(flags){
	int	  rv;
	char	  tmp[MAILTMPLEN];
	STATARG_S args;

	memset(&args, 0, sizeof(STATARG_S));
	args.flags    = flags;
	args.context  = context;
	args.nmsgs    = count;
	args.cmp      = cmp;
 
	args.stream = sp_stream_get(context_apply(tmp, context,
						  "xxx", sizeof(tmp)),
				    SP_SAME);
	if(!args.stream){
	    args.stream = pine_mail_open(NULL, tmp,
				 OP_SILENT|OP_HALFOPEN|SP_USEPOOL|SP_TEMPUSE,
					 NULL);
	    args.newstream = (args.stream != NULL);
	}

	rv = foreach_folder(context, selected,
			    foreach_do_stat, (void *) &args);

	if(args.newstream)
	  pine_mail_close(args.stream);

	if(rv)
	  return(1);
    }

    cmd_cancelled("Select");
    return(0);
}


int
folder_select_count(long int *count, int *cmp)
{
    int		r, flags;
    char	number[32], prompt[128];
    static char *tense[] = {"EQUAL TO", "LESS THAN", "GREATER THAN"};
    static ESCKEY_S sel_num_opt[] = {
	{ctrl('W'), 14, "^W", N_("Toggle Comparison")},
	{-1, 0, NULL, NULL}
    };

    *count = 0L;
    while(1){
	flags = OE_APPEND_CURRENT | OE_DISALLOW_HELP;
	snprintf(number, sizeof(number), "%ld", *count);
	number[sizeof(number)-1] = '\0';
	snprintf(prompt, sizeof(prompt), "Select folders with messages %s : ", tense[*cmp]);
	prompt[sizeof(prompt)-1] = '\0';
	r = optionally_enter(number, -FOOTER_ROWS(ps_global), 0, sizeof(number),
			     prompt, sel_num_opt, NO_HELP, &flags);
	switch (r){
	  case 0 :
	    if(!*number)
	      break;
	    else if((*count = atol(number)) < 0L)
	      q_status_message(SM_ORDER, 3, 3,
			       "Can't have NEGATIVE message count!");
	    else
	      return(1);	/* success */

	  case 3 :		/* help */
	  case 4 :		/* redraw */
	    continue;

	  case 14 :		/* toggle comparison */
	    *cmp = ++(*cmp) % 3;
	    continue;

	  case -1 :		/* cancel */
	  default:
	    break;
	}

	break;
    }

    return(0);			/* return failure */
}


int
foreach_do_stat(FOLDER_S *f, void *d)
{
    STATARG_S *sa = (STATARG_S *) d;
    int	       rv = 0;

    if(ps_global->context_current == sa->context
       && !strcmp(ps_global->cur_folder, FLDR_NAME(f))){
	switch(sa->flags){
	  case SA_MESSAGES :
	    switch(sa->cmp){
	      case 0 :				/* equals */
		if(ps_global->mail_stream->nmsgs == sa->nmsgs)
		  rv = 1;

		break;

	      case 1 :				/* less than */
		if(ps_global->mail_stream->nmsgs < sa->nmsgs)
		  rv = 1;

		break;

	      case 2 :
		if(ps_global->mail_stream->nmsgs > sa->nmsgs)
		  rv = 1;

		break;

	      default :
		break;
	    }

	    break;

	  case SA_RECENT :
	    if(count_flagged(ps_global->mail_stream, F_RECENT))
	      rv = 1;

	    break;

	  case SA_UNSEEN :
	    if(count_flagged(ps_global->mail_stream, F_UNSEEN))
	      rv = 1;

	    break;

	  default :
	    break;
	}
    }
    else{
	int	    we_cancel = 0;
	char	    msg_buf[MAX_BM+1];
	extern MAILSTATUS mm_status_result;

	snprintf(msg_buf, sizeof(msg_buf), "Checking %s for %s", FLDR_NAME(f),
		(sa->flags == SA_UNSEEN)
		    ? "unseen messages"
		    : (sa->flags == SA_MESSAGES) ? "message count"
						 : "recent messages");
	msg_buf[sizeof(msg_buf)-1] = '\0';
	we_cancel = busy_cue(msg_buf, NULL, 0);

	if(!context_status(sa->context, sa->stream, f->name, sa->flags))
	  mm_status_result.flags = 0L;

	if(we_cancel)
	  cancel_busy_cue(0);

	if(sa->flags & mm_status_result.flags)
	  switch(sa->flags){
	    case SA_MESSAGES :
	      switch(sa->cmp){
		case 0 :			/* equals */
		  if(mm_status_result.messages == sa->nmsgs)
		    rv = 1;

		  break;

		case 1 :				/* less than */
		  if(mm_status_result.messages < sa->nmsgs)
		    rv = 1;

		  break;

		case 2 :
		  if(mm_status_result.messages > sa->nmsgs)
		    rv = 1;

		  break;

		default :
		  break;
	      }

	      break;

	    case SA_RECENT :
	      if(mm_status_result.recent)
		rv = 1;

	      break;

	    case SA_UNSEEN :
	      if(mm_status_result.unseen)
		rv = 1;

	      break;

	    default :
	      break;
	  }
    }

    return(rv);
}


int
foreach_folder(CONTEXT_S *context, int selected, int (*test) (FOLDER_S *, void *), void *args)
{
    int		     i, n, rv = 1;
    int              we_turned_on = 0;
    FOLDER_S	    *fp;

    we_turned_on = intr_handling_on();

    for(i = 0, n = folder_total(FOLDERS(context)); i < n; i++){
	if(ps_global->intr_pending){
	    for(; i >= 0; i--)
	      folder_entry(i, FOLDERS(context))->scanned = 0;

	    cmd_cancelled("Select");
	    rv = 0;
	    break;
	}

	fp = folder_entry(i, FOLDERS(context));
	fp->scanned = 0;
	if((!selected || fp->selected) && fp->isfolder && (*test)(fp, args))
	  fp->scanned = 1;
    }

    if(we_turned_on)
      intr_handling_off();

    return(rv);
}


/*----------------------------------------------------------------------
  Return the path delimiter for the given folder on the given server

  Args: folder -- folder type for delimiter

    ----*/
int
folder_delimiter(char *folder)
{
    int rv, we_cancel = 0;

    we_cancel = busy_cue(NULL, NULL, 1);

    rv = get_folder_delimiter(folder);

    if(we_cancel)
      cancel_busy_cue(-1);

    return(rv);
}


/*
 * next_folder - given a current folder in a context, return the next in
 *               the list, or NULL if no more or there's a problem.
 *
 *  Args   streamp -- If set, try to re-use this stream for checking.
 *            next -- Put return value here, return points to this
 *         nextlen -- Length of next
 *         current -- Current folder, so we know where to start looking
 *           cntxt --
 *     find_recent -- Returns the number of recent here. The presence of
 *                     this arg also indicates that we should do the calls
 *                     to figure out whether there is a next interesting folder
 *                     or not.
 *      did_cancel -- Tell caller if user canceled. Only used if find_recent
 *                     is also set. Also, user will only be given the
 *                     opportunity to cancel if this is set. If it isn't
 *                     set, we just plow ahead when there is an error or
 *                     when the folder does not exist.
 */
char *
next_folder(MAILSTREAM **streamp, char *next, size_t nextlen, char *current, CONTEXT_S *cntxt, long int *find_recent, int *did_cancel)
{
    int       index, recent = 0, failed_status = 0, try_fast;
    char      prompt[128];
    FOLDER_S *f = NULL;
    char      tmp[MAILTMPLEN];


    /* note: find_folders may assign "stream" */
    build_folder_list(streamp, cntxt, NULL, NULL,
		      NEWS_TEST(cntxt) ? BFL_LSUB : BFL_NONE);

    try_fast = (F_ON(F_ENABLE_FAST_RECENT, ps_global) &&
	        F_OFF(F_TAB_USES_UNSEEN, ps_global));
    if(find_recent)
      *find_recent = 0L;

    for(index = folder_index(current, cntxt, FI_FOLDER) + 1;
	index > 0
	&& index < folder_total(FOLDERS(cntxt))
	&& (f = folder_entry(index, FOLDERS(cntxt)))
	&& !f->isdir;
	index++)
      if(find_recent){
	  MAILSTREAM *stream = NULL;
	  int         rv, we_cancel = 0, match;
	  char        msg_buf[MAX_BM+1];

	  /* must be a folder and it can't be the current one */
	  if(ps_global->context_current == ps_global->context_list
	     && !strcmp(ps_global->cur_folder, FLDR_NAME(f)))
	    continue;

	  /*
	   * If we already have the folder open, short circuit all this
	   * stuff.
	   */
	  match = 0;
	  if((stream = sp_stream_get(context_apply(tmp, cntxt, f->name,
						   sizeof(tmp)),
				     SP_MATCH)) != NULL
	     || (!IS_REMOTE(tmp) && (stream = already_open_stream(tmp, AOS_NONE)) != NULL)){
	      (void) pine_mail_ping(stream);

	      if(F_ON(F_TAB_USES_UNSEEN, ps_global)){
		  /*
		   * Just fall through and let the status call below handle
		   * the already open stream. If we were doing this the
		   * same as the else case, we would figure out how many
		   * unseen are in this open stream by doing a search.
		   * Instead of repeating that code that is already in
		   * pine_mail_status_full, fall through and note the
		   * special case by lighting the match variable.
		   */
		  match++;
	      }
	      else{
		  *find_recent = sp_recent_since_visited(stream);
		  if(*find_recent){
		      recent++;
		      break;
		  }

		  continue;
	      }
	  }

	  snprintf(msg_buf, sizeof(msg_buf), "Checking %s for %s messages",
		  FLDR_NAME(f), F_ON(F_TAB_USES_UNSEEN, ps_global) ? "unseen" : "recent");
	  msg_buf[sizeof(msg_buf)-1] = '\0';
	  we_cancel = busy_cue(msg_buf, NULL, 0);

	  /* First, get a stream for the test */
	  if(!stream && streamp && *streamp){
	      if(context_same_stream(cntxt, f->name, *streamp)){
		  stream = *streamp;
	      }
	      else{
		  pine_mail_close(*streamp);
		  *streamp = NULL;
	      }
	  }

	  if(!stream){
	      stream = sp_stream_get(context_apply(tmp, cntxt, f->name,
						   sizeof(tmp)),
				     SP_SAME);
	  }

	  /*
	   * If interestingness is indeterminate or we're
	   * told to explicitly, look harder...
	   */

	  /*
	   * We could make this more efficient in the cases where we're
	   * opening a new stream or using streamp by having folder_exists
	   * cache the stream. The change would require a folder_exists()
	   * that caches streams, but most of the time folder_exists just
	   * uses the inbox stream or ps->mail_stream.
	   *
	   * Another thing to consider is that maybe there should be an
	   * option to try to LIST a folder before doing a STATUS (or SELECT).
	   * This isn't done by default for the case where a folder is
	   * SELECTable but not LISTable, but on some servers doing an
	   * RLIST first tells the server that we support mailbox referrals.
	   */
	  if(!try_fast
	     || !((rv = folder_exists(cntxt,f->name))
					     & (FEX_ISMARKED | FEX_UNMARKED))){
	      extern MAILSTATUS mm_status_result;

	      if(try_fast && (rv == 0 || rv & FEX_ERROR)){
		  failed_status = 1;
		  mm_status_result.flags = 0L;
	      }
	      else{
		  if(stream){
		      if(!context_status_full(cntxt, match ? NULL : stream,
					      f->name,
					      F_ON(F_TAB_USES_UNSEEN, ps_global)
					        ? SA_UNSEEN : SA_RECENT,
					      &f->uidvalidity,
					      &f->uidnext)){
			  failed_status = 1;
			  mm_status_result.flags = 0L;
		      }
		  }
		  else{
		      /* so we can re-use the stream */
		      if(!context_status_streamp_full(cntxt, streamp, f->name,
					      F_ON(F_TAB_USES_UNSEEN, ps_global)
					        ? SA_UNSEEN : SA_RECENT,
						      &f->uidvalidity,
						      &f->uidnext)){
			  failed_status = 1;
			  mm_status_result.flags = 0L;
		      }
		  }
	      }

	      if(F_ON(F_TAB_USES_UNSEEN, ps_global)){
		  rv = ((mm_status_result.flags & SA_UNSEEN)
			&& (*find_recent = mm_status_result.unseen))
			 ? FEX_ISMARKED : 0;
	      }
	      else{
		  rv = ((mm_status_result.flags & SA_RECENT)
			&& (*find_recent = mm_status_result.recent))
			 ? FEX_ISMARKED : 0;
	      }

	      /* we don't know how many in this case */
	      if(try_fast)
		*find_recent = 0L;	/* consistency, boy! */
	  }

	  if(we_cancel)
	    cancel_busy_cue(0);

	  if(failed_status && did_cancel){
	    char buf1[6*MAX_SCREEN_COLS+1];
	    int  wid1, wid2;

	    snprintf(prompt, sizeof(prompt), _("Check of folder %s failed. Continue "), FLDR_NAME(f));
	    if(utf8_width(prompt) > MAXPROMPT){
	      snprintf(prompt, sizeof(prompt), _("Check of %s failed. Continue "), FLDR_NAME(f));
	      if((wid1=utf8_width(prompt)) > MAXPROMPT){
		if((wid2=utf8_width(FLDR_NAME(f))) > wid1-MAXPROMPT)
	          snprintf(prompt, sizeof(prompt), _("Check of %s failed. Continue "), strsquish(buf1, sizeof(buf1), FLDR_NAME(f), wid2-(wid1-MAXPROMPT)));
		else
	          snprintf(prompt, sizeof(prompt), _("Check failed. Continue "));
	      }
	    }

	    if(want_to(prompt, 'y', 0, NO_HELP, WT_NORM) == 'n'){
	      *did_cancel = 1;
	      break;
	    }
	    else
	      /* have to reset this lower-level cancel marker */
	      ps_global->user_says_cancel = 0;
	  }

	  failed_status = 0;

	  if(rv & FEX_ISMARKED){
	      recent++;
	      break;
	  }
      }

    if(f && (!find_recent || recent)){
	strncpy(next, FLDR_NAME(f), nextlen);
	next[nextlen-1] = '\0';
    }
    else if(nextlen > 0)
      *next = '\0';

    /* BUG: how can this be made smarter so we cache the list? */
    free_folder_list(cntxt);
    return((*next) ? next : NULL);
}


/*----------------------------------------------------------------------
      Shuffle order of incoming folders
  ----*/
int
shuffle_incoming_folders(CONTEXT_S *context, int index)
{
    int       tot, i, deefault, rv, inheriting = 0;
    int       new_index, index_within_var, new_index_within_var;
    int       readonly = 0;
    char      tmp[200];
    HelpType  help;
    ESCKEY_S  opts[3];
    char   ***alval;
    EditWhich ew;
    FOLDER_S *fp;
    PINERC_S *prc = NULL;

    dprint((4, "shuffle_incoming_folders\n"));

    if(!(context->use & CNTXT_INCMNG) ||
       (tot = folder_total(FOLDERS(context))) < 2 ||
       index < 0 || index >= tot)
      return(0);
    
    if(index == 0){
	q_status_message(SM_ORDER,0,3, _("Cannot shuffle INBOX"));
	return(0);
    }

    fp = folder_entry(index, FOLDERS(context));
    ew = config_containing_inc_fldr(fp);

    if(ps_global->restricted)
      readonly = 1;
    else{
	switch(ew){
	  case Main:
	    prc = ps_global->prc;
	    break;
	  case Post:
	    prc = ps_global->post_prc;
	    break;
	  case None:
	    break;
	}

	readonly = prc ? prc->readonly : 1;
    }

    if(prc && prc->quit_to_edit){
	quit_to_edit_msg(prc);
	return(0);
    }

    if(readonly){
	q_status_message(SM_ORDER,3,5,
	    _("Shuffle cancelled: config file not editable"));
	return(0);
    }

    alval = ALVAL(&ps_global->vars[V_INCOMING_FOLDERS], ew);

    if(!(alval && *alval))
      return(0);
    
    i = 0;
    opts[i].ch      = 'b';
    opts[i].rval    = 'b';
    opts[i].name    = "B";
    opts[i++].label = N_("Back");

    opts[i].ch      = 'f';
    opts[i].rval    = 'f';
    opts[i].name    = "F";
    opts[i++].label = N_("Forward");

    opts[i].ch = -1;
    deefault = 'b';

    /* find where this entry is in the particular config list */
    index_within_var = -1;
    for(i = 0; (*alval)[i]; i++){
	expand_variables(tmp_20k_buf, SIZEOF_20KBUF, (*alval)[i], 0);
	if(i == 0 && !strcmp(tmp_20k_buf, INHERIT))
	  inheriting = 1;
	else if(fp->varhash == line_hash(tmp_20k_buf)){
	    index_within_var = i;
	    break;
	}
    }

    if(index_within_var == -1){			/* didn't find it */
	q_status_message(SM_ORDER,3,5,
	    _("Shuffle cancelled: unexpected trouble shuffling"));
	return(0);
    }

    if(index_within_var == 0 || (inheriting && index_within_var == 1)){
	opts[0].ch = -2;			/* no back */
	deefault = 'f';
    }

    if(!(*alval)[i+1])				/* no forward */
      opts[1].ch = -2;
    
    if(opts[0].ch == -2 && opts[1].ch == -2){
	q_status_message(SM_ORDER, 0, 4,
			 _("Cannot shuffle from one config file to another."));
	return(0);
    }

    snprintf(tmp, sizeof(tmp), "Shuffle \"%s\" %s%s%s ? ",
	    FLDR_NAME(folder_entry(index, FOLDERS(context))),
	    (opts[0].ch != -2) ? "BACK" : "",
	    (opts[0].ch != -2 && opts[1].ch != -2) ? " or " : "",
	    (opts[1].ch != -2) ? "FORWARD" : "");
    tmp[sizeof(tmp)-1] = '\0';
    help = (opts[0].ch == -2) ? h_incoming_shuf_down
			      : (opts[1].ch == -2) ? h_incoming_shuf_up
						   : h_incoming_shuf;

    rv = radio_buttons(tmp, -FOOTER_ROWS(ps_global), opts, deefault, 'x',
		       help, RB_NORM);

    new_index = index;
    new_index_within_var = index_within_var;

    switch(rv){
      case 'x':
	cmd_cancelled("Shuffle");
	return(0);

      case 'b':
	new_index_within_var--;
	new_index--;
	break;

      case 'f':
	new_index_within_var++;
	new_index++;
	break;
    }

    if(swap_incoming_folders(index, new_index, FOLDERS(context))){
	char    *stmp;

	/* swap them in the config variable, too  */
	stmp = (*alval)[index_within_var];
	(*alval)[index_within_var] = (*alval)[new_index_within_var];
	(*alval)[new_index_within_var] = stmp;

	set_current_val(&ps_global->vars[V_INCOMING_FOLDERS], TRUE, FALSE);
	write_pinerc(ps_global, ew, WRP_NONE);

	return(1);
    }
    else
      return(0);
}


int
swap_incoming_folders(int index1, int index2, FLIST *flist)
{
    FOLDER_S *ftmp;

    if(!flist)
      return(0);

    if(index1 == index2)
      return(1);
    
    if(index1 < 0 || index1 >= flist->used){
	dprint((1, "Error in swap_incoming_folders: index1=%d, used=%d\n", index1, flist->used));
	return(0);
    }

    if(index2 < 0 || index2 >= flist->used){
	dprint((1, "Error in swap_incoming_folders: index2=%d, used=%d\n", index2, flist->used));
	return(0);
    }

    ftmp = flist->folders[index1];
    flist->folders[index1] = flist->folders[index2];
    flist->folders[index2] = ftmp;

    return(1);
}


/*----------------------------------------------------------------------
    Find an entry in the folder list by matching names
  ----*/
int
search_folder_list(void *list, char *name)
{
    int i;
    char *n;

    for(i = 0; i < folder_total(list); i++) {
        n = folder_entry(i, list)->name;
        if(strucmp(name, n) == 0)
          return(1); /* Found it */
    }
    return(0);
}


static CONTEXT_S *post_cntxt = NULL;

/*----------------------------------------------------------------------
    Browse list of newsgroups available for posting

  Called from composer when ^T is typed in newsgroups field

Args:    none

Returns: pointer to selected newsgroup, or NULL.
         Selector call in composer expects this to be alloc'd here.

  ----*/
char *
news_group_selector(char **error_mess)
{
    CONTEXT_S *tc;
    char      *post_folder;
    int        rc;
    char      *em;

    /* Coming back from composer */
    fix_windsize(ps_global);
    init_sigwinch();

    post_folder = fs_get((size_t)MAILTMPLEN);

    /*--- build the post_cntxt -----*/
    em = get_post_list(ps_global->VAR_NNTP_SERVER);
    if(em != NULL){
        if(error_mess != NULL)
          *error_mess = cpystr(em);

	cancel_busy_cue(-1);
        return(NULL);
    }

    /*----- Call the browser -------*/
    tc = post_cntxt;
    if((rc = folders_for_post(ps_global, &tc, post_folder)) != 0)
      post_cntxt = tc;

    cancel_busy_cue(-1);

    if(rc <= 0)
      return(NULL);

    return(post_folder);
}


/*----------------------------------------------------------------------
    Get the list of news groups that are possible for posting

Args: post_host -- host name for posting

Returns NULL if list is retrieved, pointer to error message if failed

This is kept in a standards "CONTEXT" for a acouple of reasons. First
it makes it very easy to use the folder browser to display the
newsgroup for selection on ^T from the composer. Second it will allow
the same mechanism to be used for all folder lists on memory tight
systems like DOS. The list is kept for the life of the session because
fetching it is a expensive. 

 ----*/
char *
get_post_list(char **post_host)
{
    char *post_context_string;

    if(!post_host || !post_host[0]) {
        /* BUG should assume inews and get this from active file */
        return(_("Can't post messages, NNTP server needs to be configured"));
    }

    if(!post_cntxt){
	int we_cancel;
	size_t l;

	we_cancel = busy_cue(_("Getting full list of groups for posting"), NULL, 1);

	l = strlen(post_host[0]) + 20;
        post_context_string = (char *) fs_get((l+1) * sizeof(char));
        snprintf(post_context_string, l+1, "{%s/nntp}#news.[]", post_host[0]);
	post_context_string[l] = '\0';
        
        post_cntxt          = new_context(post_context_string, NULL);
        post_cntxt->use    |= CNTXT_FINDALL;
        post_cntxt->dir->status |= CNTXT_NOFIND; 
        post_cntxt->next    = NULL;

        build_folder_list(NULL, post_cntxt, NULL, NULL,
			  NEWS_TEST(post_cntxt) ? BFL_LSUB : BFL_NONE);
	if(we_cancel)
	  cancel_busy_cue(-1);
    }
    return(NULL);
}


#ifdef	_WINDOWS
int
folder_list_popup(sparms, in_handle)
    SCROLL_S *sparms;
    int	      in_handle;
{
    MPopup fldr_popup[20];

    memset(fldr_popup, 0, 20 * sizeof(MPopup));
    fldr_popup[0].type = tTail;
    if(in_handle){
	int	  i, n = 0;
	HANDLE_S *h = get_handle(sparms->text.handles, in_handle);
	FOLDER_S *fp = (h) ? folder_entry(h->h.f.index,
					  FOLDERS(h->h.f.context))
			   : NULL;

	if((i = menu_binding_index(sparms->keys.menu, MC_CHOICE)) >= 0){
	    fldr_popup[n].type	      = tQueue;
	    fldr_popup[n].data.val    = sparms->keys.menu->keys[i].bind.ch[0];
	    fldr_popup[n].label.style = lNormal;
	    fldr_popup[n++].label.string = (fp && fp->isdir)
					     ? "&View Directory"
					     : "&View Folder";
	}

	if((i = menu_binding_index(sparms->keys.menu, MC_SELCUR)) >= 0
	   && bitnset(i, sparms->keys.bitmap)){
	    fldr_popup[n].type	      = tQueue;
	    fldr_popup[n].data.val    = sparms->keys.menu->keys[i].bind.ch[0];
	    fldr_popup[n].label.style = lNormal;
	    fldr_popup[n++].label.string = (fp && fp->isdir)
					     ? "&Select Directory"
					     : "&Select Folder";
	}

	if((i = menu_binding_index(sparms->keys.menu, MC_DELETE)) >= 0
	   && bitnset(i, sparms->keys.bitmap)){
	    fldr_popup[n].type	      = tQueue;
	    fldr_popup[n].data.val    = sparms->keys.menu->keys[i].bind.ch[0];
	    fldr_popup[n].label.style = lNormal;
	    fldr_popup[n++].label.string = "Delete Folder";
	}

	if((i = menu_binding_index(sparms->keys.menu, MC_EXPORT)) >= 0
	   && bitnset(i, sparms->keys.bitmap)){
	    fldr_popup[n].type	      = tQueue;
	    fldr_popup[n].data.val    = sparms->keys.menu->keys[i].bind.ch[0];
	    fldr_popup[n].label.style = lNormal;
	    fldr_popup[n++].label.string = "Export Folder";
	}

	if((i = menu_binding_index(sparms->keys.menu, MC_CHK_RECENT)) >= 0
	   && bitnset(i, sparms->keys.bitmap)){
	    fldr_popup[n].type	      = tQueue;
	    fldr_popup[n].data.val    = sparms->keys.menu->keys[i].bind.ch[0];
	    fldr_popup[n].label.style = lNormal;
	    fldr_popup[n++].label.string = "Check New Messages";
	}

	if(n)
	  fldr_popup[n++].type = tSeparator;

	folder_popup_config(((FPROC_S *)sparms->proc.data.p)->fs,
			    sparms->keys.menu, &fldr_popup[n]);
    }
    else
      folder_popup_config(((FPROC_S *)sparms->proc.data.p)->fs,
			  sparms->keys.menu, fldr_popup);

    if(fldr_popup[0].type != tTail)
      mswin_popup(fldr_popup);

    return(0);
}


int
folder_list_select_popup(sparms, in_handle)
    SCROLL_S *sparms;
    int	      in_handle;
{
    MPopup fldr_popup[20];

    memset(fldr_popup, 0, 20 * sizeof(MPopup));
    fldr_popup[0].type = tTail;
    if(in_handle){
	int	  i, n = 0;
	HANDLE_S *h = get_handle(sparms->text.handles, in_handle);
	FOLDER_S *fp = (h) ? folder_entry(h->h.f.index,FOLDERS(h->h.f.context))
			   : NULL;

	if((i = menu_binding_index(sparms->keys.menu, MC_CHOICE)) >= 0){
	    fldr_popup[n].type	      = tQueue;
	    fldr_popup[n].data.val    = sparms->keys.menu->keys[i].bind.ch[0];
	    fldr_popup[n].label.style = lNormal;
	    fldr_popup[n++].label.string = (fp && fp->isdir)
					     ? "&View Directory"
					     : "&Select";

	    fldr_popup[n++].type = tSeparator;
	}

	folder_popup_config(((FPROC_S *)sparms->proc.data.p)->fs,
			    sparms->keys.menu, &fldr_popup[n]);
    }
    else
      folder_popup_config(((FPROC_S *)sparms->proc.data.p)->fs,
			  sparms->keys.menu, fldr_popup);

    if(fldr_popup[0].type != tTail)
      mswin_popup(fldr_popup);

    return(0);
}


/*
 * Just a little something to simplify assignments
 */
#define	FLDRPOPUP(p, c, s)	{ \
				    (p)->type	      = tQueue; \
				    (p)->data.val     = c; \
				    (p)->label.style  = lNormal; \
				    (p)->label.string = s; \
				}


/*----------------------------------------------------------------------
  Popup Menu configurator
	     
 ----*/
void
folder_popup_config(fs, km, popup)
    FSTATE_S	    *fs;
    struct key_menu *km;
    MPopup	    *popup;
{
    int i;

    if((i = menu_binding_index(km, MC_PARENT)) >= 0){
	FLDRPOPUP(popup, km->keys[i].bind.ch[0], "Parent Directory");
	popup++;
    }

    if(fs->km == &folder_km){
	if((fs->context->next || fs->context->prev) && !fs->combined_view){
	    FLDRPOPUP(popup, '<', "Collection List");
	    popup++;
	}
    }
    else if((i = menu_binding_index(km, MC_COLLECTIONS)) >= 0){
	FLDRPOPUP(popup, km->keys[i].bind.ch[0], "Collection List");
	popup++;
    }

    if((i = menu_binding_index(km, MC_INDEX)) >= 0){
	FLDRPOPUP(popup, km->keys[i].bind.ch[0], "Current Folder Index");
	popup++;
    }

    if((i = menu_binding_index(km, MC_MAIN)) >= 0){
	FLDRPOPUP(popup, km->keys[i].bind.ch[0], "Main Menu");
	popup++;
    }
    
    popup->type = tTail;		/* tie off the array */
}
#endif	/* _WINDOWS */
