#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: adrbklib.c 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $";
#endif

/* ========================================================================
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
#include "../pith/adrbklib.h"
#include "../pith/abdlc.h"
#include "../pith/addrbook.h"
#include "../pith/addrstring.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/status.h"
#include "../pith/remote.h"
#include "../pith/tempfile.h"
#include "../pith/bldaddr.h"
#include "../pith/signal.h"
#include "../pith/busy.h"
#include "../pith/util.h"


AddrScrState as;

void (*pith_opt_save_and_restore)(int, SAVE_STATE_S *);


/*
 * We don't want any end of line fixups to occur, so include "b" in DOS modes.
 */
#if defined(DOS) || defined(OS2)
#define	ADRBK_NAME		"addrbook"
#else
#define	ADRBK_NAME		".addressbook"
#endif

#define	OPEN_WRITE_MODE		(O_TRUNC|O_WRONLY|O_CREAT|O_BINARY)


#ifndef MAXPATH
#define MAXPATH 1000    /* Longest file path we can deal with */
#endif

#define TABWIDTH 8
#define INDENTSTR "   "
#define INDENTXTRA " : "
#define INDENT 3  /* length of INDENTSTR */


#define SLOP 3

static int writing; /* so we can give understandable error message */

AdrBk               *edited_abook;

static char empty[]       = "";

jmp_buf jump_over_qsort;


/* internal prototypes */
int            copy_abook_to_tempfile(AdrBk *, char *, size_t);
char          *dir_containing(char *);
char          *get_next_abook_entry(FILE *, int);
void           strip_addr_string(char *, char **, char **);
void           adrbk_check_local_validity(AdrBk *, long);
int            write_single_abook_entry(AdrBk_Entry *, FILE *, int *, int *, int *, int *);
char	      *backcompat_encoding_for_abook(char *, size_t, char *, size_t, char *);
int            percent_abook_saved(void);
void           exp_del_nth(EXPANDED_S *, a_c_arg_t);
void           exp_add_nth(EXPANDED_S *, a_c_arg_t);
int            cmp_ae_by_full_lists_last(const qsort_t *,const qsort_t *);
int            cmp_cntr_by_full_lists_last(const qsort_t *, const qsort_t *);
int            cmp_ae_by_full(const qsort_t *, const qsort_t *);
int            cmp_cntr_by_full(const qsort_t *, const qsort_t *);
int            cmp_ae_by_nick_lists_last(const qsort_t *,const qsort_t *);
int            cmp_cntr_by_nick_lists_last(const qsort_t *, const qsort_t *);
int            cmp_ae_by_nick(const qsort_t *, const qsort_t *);
int            cmp_cntr_by_nick(const qsort_t *, const qsort_t *);
int            cmp_addr(const qsort_t *, const qsort_t *);
void           sort_addr_list(char **);
int            build_abook_datastruct(AdrBk *, char *, size_t);
AdrBk_Entry   *init_ae(AdrBk *, AdrBk_Entry *, char *);
adrbk_cntr_t   count_abook_entries_on_disk(AdrBk *, a_c_arg_t *);
AdrBk_Entry   *adrbk_get_delae(AdrBk *, a_c_arg_t);
void           free_ae_parts(AdrBk_Entry *);
void           add_entry_to_trie(AdrBk_Trie **, char *, a_c_arg_t);
int            build_abook_tries(AdrBk *, char *);
void           repair_abook_tries(AdrBk *);
adrbk_cntr_t   lookup_nickname_in_trie(AdrBk *, char *);
adrbk_cntr_t   lookup_address_in_trie(AdrBk *, char *);
adrbk_cntr_t   lookup_in_abook_trie(AdrBk_Trie *, char *);
void           free_abook_trie(AdrBk_Trie **);
adrbk_cntr_t   re_sort_particular_entry(AdrBk *, a_c_arg_t);
void           move_ab_entry(AdrBk *, a_c_arg_t, a_c_arg_t);
void           insert_ab_entry(AdrBk *, a_c_arg_t, AdrBk_Entry *, int);
void           delete_ab_entry(AdrBk *, a_c_arg_t, int);
void           defvalue_ae(AdrBk_Entry *);


/*
 *  Open, read, and parse an address book.
 *
 * Args: pab      -- the PerAddrBook structure
 *       homedir  -- the user's home directory if specified
 *       warning  -- put "why failed" message to user here
 *                   (provide space of at least 201 chars)
 *
 * If filename is NULL, the default will be used in the homedir
 * passed in.  If homedir is NULL, the current dir will be used.
 * If filename is not NULL and is an absolute path, just the filename
 * will be used.  Otherwise, it will be used relative to the homedir, or
 * to the current dir depending on whether or not homedir is NULL.
 *
 * Expected addressbook file format is:
 *  <nickname>\t<fullname>\t<address_field>\t<fcc>\t<comment>
 *
 * The last two fields (\t<fcc>\t<comment>) are optional.
 *
 * Lines that start with SPACE are continuation lines.  Ends of lines are
 * treated as if they were spaces.  The address field is either a single
 * address or a list of comma-separated addresses inside parentheses.
 *
 * Fields missing from the end of an entry are considered blank.
 *
 * Commas in the address field will cause problems, as will tabs in any
 * field.
 *
 * There may be some deleted entries in the addressbook that don't get
 * used.  They look like regular entries except their nicknames start
 * with the string "#DELETED-YY/MM/DD#".
 */
AdrBk *
adrbk_open(PerAddrBook *pab, char *homedir, char *warning, size_t warninglen, int sort_rule)
{
    char   path[MAXPATH], *filename;
    AdrBk *ab;
    int    abook_validity_was_minusone = 0;


    filename = pab ? pab->filename : NULL;

    dprint((2, "- adrbk_open(%s) -\n", filename ? filename : ""));

    ab = (AdrBk *)fs_get(sizeof(AdrBk));
    memset(ab, 0, sizeof(*ab));

    ab->orig_filename = filename ? cpystr(filename) : NULL;

    if(pab->type & REMOTE_VIA_IMAP){
	int try_cache;

	ab->type           = Imap;

	if(!ab->orig_filename || *(ab->orig_filename) != '{'){
	    dprint((1, "adrbk_open: remote: filename=%s\n",
		   ab->orig_filename ? ab->orig_filename : "NULL"));
	    goto bail_out;
	}

	if(!(ab->rd = rd_new_remdata(RemImap, ab->orig_filename, REMOTE_ABOOK_SUBTYPE))){
	    dprint((1,
		   "adrbk_open: remote: new_remdata failed: %s\n",
		   ab->orig_filename ? ab->orig_filename : "NULL"));
	    goto bail_out;
	}
	
	/* Transfer responsibility for the storage object */
	ab->rd->so = pab->so;
	pab->so = NULL;

	try_cache = rd_read_metadata(ab->rd);

	if(ab->rd->lf)
	  ab->filename = cpystr(ab->rd->lf);

	/* Transfer responsibility for removal of temp file */
	if(ab->rd->flags & DEL_FILE){
	    ab->flags |= DEL_FILE;
	    ab->rd->flags &= ~DEL_FILE;
	}

	if(pab->access == MaybeRorW){
	    if(ab->rd->read_status == 'R')
	      pab->access = ab->rd->access = ReadOnly;
	    else
	      pab->access = ab->rd->access = ReadWrite;
	}
	else if(pab->access == ReadOnly){
	    /*
	     * Pass on readonly-ness from being a global addrbook.
	     * This should cause us to open the remote folder readonly,
	     * avoiding error messages about readonly-ness.
	     */
	    ab->rd->access = ReadOnly;
	}

	/*
	 * The plan is to fetch addrbook data and copy into local file.
	 * Then we open the local copy for reading. We use the IMAP STATUS
	 * command to tell us if we need to update from the remote addrbook.
	 *
	 * If access is NoExists, that probably means we had trouble
	 * opening the remote folder in the adrbk_access routine.
	 * In that case we'll use a cached copy if we have one.
	 */
	if(pab->access != NoExists){

bootstrap_nocheck_policy:
	    if(try_cache && ps_global->remote_abook_validity == -1 &&
	       !abook_validity_was_minusone)
	      abook_validity_was_minusone++;
	    else{
		rd_check_remvalid(ab->rd, 1L);
		abook_validity_was_minusone = 0;
	    }

	    /*
	     * If the cached info on this addrbook says it is readonly but
	     * it looks like it's been fixed now, change it to readwrite.
	     */
	    if(!(pab->type & GLOBAL) && ab->rd->read_status == 'R'){
		/*
		 * We go to this trouble since readonly addrbooks
		 * are likely a mistake. They are usually supposed to be
		 * readwrite. So we open it and check if it's been fixed.
		 */
		rd_check_readonly_access(ab->rd);
		if(ab->rd->read_status == 'W'){
		    pab->access = ab->rd->access = ReadWrite;
		    ab->rd->flags |= REM_OUTOFDATE;
		}
		else
		  pab->access = ab->rd->access = ReadOnly;
	    }
	      
	    if(ab->rd->flags & REM_OUTOFDATE){
		if(rd_update_local(ab->rd) != 0){
		    dprint((1,
			   "adrbk_open: remote: rd_update_local failed\n"));
		    /*
		     * Don't give up altogether. We still may be
		     * able to use a cached copy.
		     */
		}
		else{
		    dprint((7,
			   "%s: copied remote to local (%ld)\n",
			   ab->rd->rn ? ab->rd->rn : "?",
			   (long)ab->rd->last_use));
		}
	    }

	    if(pab->access == ReadWrite)
	      ab->rd->flags |= DO_REMTRIM;
	}

	/* If we couldn't get to remote folder, try using the cached copy */
	if(pab->access == NoExists || ab->rd->flags & REM_OUTOFDATE){
	    if(try_cache){
		pab->access = ab->rd->access = ReadOnly;
		ab->rd->flags |= USE_OLD_CACHE;
		q_status_message(SM_ORDER, 3, 4,
		 _("Can't contact remote address book server, using cached copy"));
		dprint((2,
	"Can't open remote addrbook %s, using local cached copy %s readonly\n",
		       ab->rd->rn ? ab->rd->rn : "?",
		       ab->rd->lf ? ab->rd->lf : "?"));
	    }
	    else
	      goto bail_out;
	}
    }
    else{
	ab->type = Local;

	/*------------ figure out and save name of file to open ---------*/
	if(filename == NULL){
	    if(homedir != NULL){
		build_path(path, homedir, ADRBK_NAME, sizeof(path));
		ab->filename = cpystr(path);
	    }
	    else
	      ab->filename = cpystr(ADRBK_NAME);
	}
	else{
	    if(is_absolute_path(filename)){
		ab->filename = cpystr(filename);
	    }
	    else{
		if(homedir != NULL){
		    build_path(path, homedir, filename, sizeof(path));
		    ab->filename = cpystr(path);
		}
		else
		  ab->filename = cpystr(filename);
	    }
	}
    }

    if(ab->filename && ab->filename[0]){
	char buf[MAXPATH];

	strncpy(buf, ab->filename, sizeof(buf)-4);
	buf[sizeof(buf)-4] = '\0';

	/*
	 * Our_filecopy is used in _WINDOWS to allow
	 * multiple pines to update the address book. The problem is
	 * that if a file is open it can't be deleted, so we need to keep
	 * the main filename closed most of the time.
	 * In Unix, our_filecopy just points to filename.
	 */

#ifdef	_WINDOWS
	/*
	 * If we can't write in the same directory as filename is in, put
	 * the copies in /tmp instead.
	 */
	if(!(ab->our_filecopy = tempfile_in_same_dir(ab->filename, "a3", NULL)))
	  ab->our_filecopy = temp_nam(NULL, "a3");
#endif	/* _WINDOWS */

	/*
	 * We don't need the copies on Unix because we can rename/delete
	 * open files. Turn the feature off by making the copies point to
	 * the originals.
	 */
	if(!ab->our_filecopy)
	  ab->our_filecopy = ab->filename;
    }
    else{
	dprint((1, "adrbk_open: ab->filename is NULL???\n"));
	goto bail_out;
    }


    /*
     * We're going to make our own copy of the address book file so that
     * we won't conflict with other instances of pine trying to change it.
     * In particular, on Windows the address book file cannot be deleted
     * or renamed into if it is open in another process.
     */
    ab->flags |= FILE_OUTOFDATE;
    if(copy_abook_to_tempfile(ab, warning, warninglen) < 0){
	dprint((1, "adrbk_open: copy_file failed\n"));
	if(abook_validity_was_minusone){
	    /*
	     * The file copy failed when it shouldn't have. If the user has
	     * remote_abook_validity == -1 then we'll go back and try to
	     * do the validity check in case that can get us the file we
	     * need to copy. Without the validity check first time we won't
	     * contact the imap server.
	     */
	    dprint((1, "adrbk_open: trying to bootstrap\n"));

	    if(ab->our_filecopy){
		if(ab->our_filecopy != ab->filename){
		    our_unlink(ab->our_filecopy);
		    fs_give((void **)&ab->our_filecopy);
		}
		
		ab->our_filecopy = NULL;
	    }

	    goto bootstrap_nocheck_policy;
	}

	goto bail_out;
    }

    if(!ab->fp)
      goto bail_out;

    ab->sort_rule = sort_rule;
    if(pab->access == ReadOnly)
      ab->sort_rule = AB_SORT_RULE_NONE;
      
    if(ab){
	/* allocate header for expanded lists list */
	ab->exp      = (EXPANDED_S *)fs_get(sizeof(EXPANDED_S));
	/* first real element is NULL */
	ab->exp->next = (EXPANDED_S *)NULL;

	/* allocate header for checked entries list */
	ab->checks       = (EXPANDED_S *)fs_get(sizeof(EXPANDED_S));
	/* first real element is NULL */
	ab->checks->next = (EXPANDED_S *)NULL;

	/* allocate header for selected entries list */
	ab->selects       = (EXPANDED_S *)fs_get(sizeof(EXPANDED_S));
	/* first real element is NULL */
	ab->selects->next = (EXPANDED_S *)NULL;

	return(ab);
    }

bail_out:
    dprint((2, "adrbk_open: bailing: filenames=%s %s %s fp=%s\n",
	   ab->orig_filename ? ab->orig_filename : "NULL",
	   ab->filename ? ab->filename : "NULL",
	   ab->our_filecopy ? ab->our_filecopy : "NULL",
	   ab->fp ? "open" : "NULL"));

    if(ab->rd){
	ab->rd->flags &= ~DO_REMTRIM;
	rd_close_remdata(&ab->rd);
    }

    if(ab->fp)
      (void)fclose(ab->fp);

    if(ab->orig_filename)
      fs_give((void **) &ab->orig_filename);

    if(ab->our_filecopy && ab->our_filecopy != ab->filename){
	our_unlink(ab->our_filecopy);
	fs_give((void **) &ab->our_filecopy);
    }

    if(ab->filename)
      fs_give((void **) &ab->filename);

    if(pab->so){
	so_give(&(pab->so));
	pab->so = NULL;
    }

    fs_give((void **) &ab);

    return NULL;
}


/*
 * Copy the address book file to the temporary session copy. Also copy
 * the hashfile. Any of these files which don't exist will be created.
 *
 * Returns  0 success
 *         -1 failure
 */
int
copy_abook_to_tempfile(AdrBk *ab, char *warning, size_t warninglen)
{
    int    got_it, fd, c,
	   ret = -1,
	   we_cancel = 0;
    FILE  *fp_read = (FILE *)NULL,
	  *fp_write = (FILE *)NULL;
    char  *lc;
    time_t mtime;


    dprint((3, "copy_file(%s) -\n",
	       (ab && ab->filename) ? ab->filename : ""));

    if(!ab || !ab->filename || !ab->filename[0])
      goto get_out;

    if(!(ab->flags & FILE_OUTOFDATE))
      return(0);
    
    /* open filename for reading */
    fp_read = our_fopen(ab->filename, "rb");
    if(fp_read == NULL){

	/*
	 * filename probably doesn't exist so we try to create it
	 */

	/* don't want to create in these cases, should already be there */
	if(ab->type == Imap){
	    if(warning){
		if(ab->type == Imap){
		    snprintf(warning, warninglen,
		    /* TRANSLATORS: A temporary file for the address book can't
		       be opened. */
			        _("Temp addrbook file can't be opened: %s"),
				ab->filename);
		    warning[warninglen-1] = '\0';
		}
		else{
		    strncpy(warning, _("Address book doesn't exist"), warninglen);
		    warning[warninglen-1] = '\0';
		}
	    }

	    goto get_out;
	}

	q_status_message1(SM_INFO, 0, 3,
	                  _("Address book %.200s doesn't exist, creating"),
	                  (lc=last_cmpnt(ab->filename)) ? lc : ab->filename);
	dprint((2, "Address book %s doesn't exist, creating\n",
	       ab->filename ? ab->filename : "?"));

	/*
	 * Just use user's umask for permissions. Mode is "w" so the create
	 * will happen. We close it right after creating and open it in
	 * read mode again later.
	 */
        fp_read = our_fopen(ab->filename, "wb");  /* create */
        if(fp_read == NULL        ||
	   fclose(fp_read) == EOF ||
	   (fp_read = our_fopen(ab->filename, "rb")) == NULL){
            /*--- Create failed, bail out ---*/
	    if(warning){
		strncpy(warning, error_description(errno), warninglen);
		warning[warninglen-1] = '\0';
	    }

	    dprint((2, "create failed: %s\n",
		error_description(errno)));

	    goto get_out;
        }
    }

    /* record new change date of addrbook file */
    ab->last_change_we_know_about = get_adj_name_file_mtime(ab->filename);

    ab->last_local_valid_chk = get_adj_time();


    /* now there is an ab->filename and we have it open for reading */

    got_it = 0;

    /* copy ab->filename to ab->our_filecopy, preserving mtime */
    if(ab->filename != ab->our_filecopy){
	struct stat sbuf;
	struct utimbuf times;
	int valid_stat = 0;

	dprint((7, "Before abook copies\n"));
	if((fd = our_open(ab->our_filecopy, OPEN_WRITE_MODE, 0600)) < 0)
	  goto get_out;
	
	fp_write = fdopen(fd, "wb");
	rewind(fp_read);
	if(fstat(fileno(fp_read), &sbuf)){
	    q_status_message1(SM_INFO, 0, 3,
		 "Error: can't stat addrbook \"%.200s\"",
		 (lc=last_cmpnt(ab->filename)) ? lc : ab->filename);
	    dprint((2, "Error: can't stat addrbook \"%s\"\n",
		   ab->filename ? ab->filename : "?"));
	}
	else{
	    valid_stat++;
	    times.actime  = sbuf.st_atime;
	    times.modtime = sbuf.st_mtime;
	}

	while((c = getc(fp_read)) != EOF)
	  if(putc(c, fp_write) == EOF)
	    goto get_out;
	
	(void)fclose(fp_write);
	fp_write = (FILE *)NULL;
	if(valid_stat && our_utime(ab->our_filecopy, &times)){
	    q_status_message1(SM_INFO, 0, 3,
		 "Error: can't set mtime for \"%.200s\"",
		 (lc=last_cmpnt(ab->filename)) ? lc : ab->our_filecopy);
	    dprint((2, "Error: can't set mtime for \"%s\"\n",
		   ab->our_filecopy ? ab->our_filecopy : "?"));
	}

	(void)fclose(fp_read);
	fp_read = (FILE *)NULL;
	if(!(ab->fp = our_fopen(ab->our_filecopy, "rb")))
	  goto get_out;

	dprint((7, "After abook file copy\n"));
    }
    else{  /* already open to the right file */
	ab->fp = fp_read;
	fp_read = (FILE *)NULL;
    }

    /*
     * Now we've copied filename to our_filecopy.
     * Operate on the copy now. Ab->fp is open readonly on
     * our_filecopy.
     */

    got_it = 0;
    mtime = get_adj_name_file_mtime(ab->our_filecopy);
    we_cancel = busy_cue(NULL, NULL, 1);
    if(build_abook_datastruct(ab, (warning && !*warning) ? warning : NULL, warninglen)){
	if(we_cancel)
	  cancel_busy_cue(-1);

	dprint((2, "failed in build_abook_datastruct\n"));
	goto get_out;
    }

    if(ab->arr
       && build_abook_tries(ab, (warning && !*warning) ? warning : NULL)){
	if(we_cancel)
	  cancel_busy_cue(-1);

	dprint((2, "failed in build_abook_tries\n"));
	goto get_out;
    }

    if(we_cancel)
      cancel_busy_cue(-1);

    ab->flags &= ~FILE_OUTOFDATE;  /* turn off out of date flag */
    ret = 0;

get_out:
    if(fp_read)
      (void)fclose(fp_read);

    if(fp_write)
      (void)fclose(fp_write);

    if(ret < 0 && ab){
	if(ab->our_filecopy && ab->our_filecopy != ab->filename)
	  our_unlink(ab->our_filecopy);
    }

    return(ret);
}


/*
 * Returns an allocated copy of the directory which contains filename.
 */
char *
dir_containing(char *filename)
{
    char  dir[MAXPATH+1];
    char *dirp = NULL;

    if(filename){
	char *lc;

	if((lc = last_cmpnt(filename)) != NULL){
	    int to_copy;

	    to_copy = (lc - filename > 1) ? (lc - filename - 1) : 1;
	    strncpy(dir, filename, MIN(to_copy, sizeof(dir)-1));
	    dir[MIN(to_copy, sizeof(dir)-1)] = '\0';
	}
	else{
	    dir[0] = '.';
	    dir[1] = '\0';
	}

	dirp = cpystr(dir);
    }

    return(dirp);
}


/*
 * Checks whether or not the addrbook is sorted correctly according to
 * the SortType.  Returns 1 if is sorted correctly, 0 otherwise.
 */
int
adrbk_is_in_sort_order(AdrBk *ab, int be_quiet)
{
    adrbk_cntr_t entry;
    AdrBk_Entry *ae, *ae_prev;
    int (*cmp_func)();
    int we_cancel = 0;

    dprint((9, "- adrbk_is_in_sort_order -\n"));

    if(!ab)
      return 0;

    if(ab->sort_rule == AB_SORT_RULE_NONE || ab->count < 2)
      return 1;
    
    cmp_func = (ab->sort_rule == AB_SORT_RULE_FULL_LISTS) ?
					    cmp_ae_by_full_lists_last :
               (ab->sort_rule == AB_SORT_RULE_FULL) ?
					    cmp_ae_by_full :
               (ab->sort_rule == AB_SORT_RULE_NICK_LISTS) ?
					    cmp_ae_by_nick_lists_last :
            /* (ab->sort_rule == AB_SORT_RULE_NICK) */
					    cmp_ae_by_nick;

    ae_prev = adrbk_get_ae(ab, (a_c_arg_t) 0);

    if(!be_quiet)
      we_cancel = busy_cue(NULL, NULL, 1);

    for(entry = 1, ae = adrbk_get_ae(ab, (a_c_arg_t) entry);
	ae != (AdrBk_Entry *)NULL;
	ae = adrbk_get_ae(ab, (a_c_arg_t) (++entry))){

	    if((*cmp_func)((qsort_t *)&ae_prev, (qsort_t *)&ae) > 0){
		if(we_cancel)
		  cancel_busy_cue(-1);

		dprint((9, "- adrbk_is_in_sort_order : no (entry %ld) -\n", (long) entry));
		return 0;
	    }

	    ae_prev = ae;
    }

    if(we_cancel)
      cancel_busy_cue(-1);

    dprint((9, "- adrbk_is_in_sort_order : yes -\n"));

    return 1;
}


/*
 * Look through the ondisk address book and count the number of entries.
 *
 * Args  ab     -- address book pointer
 *      deleted -- pointer to location to return number of deleted entries
 *      warning -- place to put warning
 *
 * Returns number of non-deleted entries.
 */
adrbk_cntr_t
count_abook_entries_on_disk(AdrBk *ab, a_c_arg_t *deleted)
{
    FILE          *fp_in;
    char          *nickname;
    adrbk_cntr_t   count = 0;
    adrbk_cntr_t   deleted_count = 0;
    int		   rew = 1;
    char           nickbuf[50];

    if(!ab || !ab->fp)
      return -1;

    fp_in = ab->fp;

    while((nickname = get_next_abook_entry(fp_in, rew)) != NULL){
	rew = 0;
	strncpy(nickbuf, nickname, sizeof(nickbuf));
	nickbuf[sizeof(nickbuf)-1] = '\0';
	fs_give((void **) &nickname);
	if(strncmp(nickbuf, DELETED, DELETED_LEN) == 0
	   && isdigit((unsigned char)nickbuf[DELETED_LEN])
	   && isdigit((unsigned char)nickbuf[DELETED_LEN+1])
	   && nickbuf[DELETED_LEN+2] == '/'
	   && isdigit((unsigned char)nickbuf[DELETED_LEN+3])
	   && isdigit((unsigned char)nickbuf[DELETED_LEN+4])
	   && nickbuf[DELETED_LEN+5] == '/'
	   && isdigit((unsigned char)nickbuf[DELETED_LEN+6])
	   && isdigit((unsigned char)nickbuf[DELETED_LEN+7])
	   && nickbuf[DELETED_LEN+8] == '#'){
	    deleted_count++;
	    continue;
	}

	count++;
    }

    if(deleted)
      *deleted = (a_c_arg_t) deleted_count;

    return(count);
}


/*
 * Builds the data structures used with the address book which is
 * simply an array of entries.
 */
int
build_abook_datastruct(AdrBk *ab, char *warning, size_t warninglen)
{
    FILE          *fp_in;
    char          *nickname;
    char          *lc;
    a_c_arg_t      count, deleted;
    adrbk_cntr_t   used = 0, delused = 0;
    int            max_nick = 0,
		   max_full = 0, full_two = 0, full_three = 0,
		   max_fcc = 0, fcc_two = 0, fcc_three = 0,
		   max_addr = 0, addr_two = 0, addr_three = 0,
		   this_nick_width, this_full_width, this_addr_width,
		   this_fcc_width;
    WIDTH_INFO_S  *widths;
    int		   rew = 1, is_deleted;
    AdrBk_Entry   *ae;

    dprint((9, "- build_abook_datastruct -\n"));

    if(!ab || !ab->fp)
      return -1;

    errno = 0;

    fp_in = ab->fp;

    /*
     * If we used a list instead of an array to store the entries we
     * could avoid this pass through the file to count the entries, which
     * we use to allocate the array.
     *
     * Since we use entry_nums a lot to access address book entries it is
     * convenient to have an array for quick access, so we'll probably
     * leave this for now.
     */
    count = count_abook_entries_on_disk(ab, &deleted);

    if(count < 0)
      return -1;

    ab->count       = (adrbk_cntr_t) count;
    ab->del_count   = (adrbk_cntr_t) deleted;

    if(count > 0){
	ab->arr = (AdrBk_Entry *) fs_get(count * sizeof(AdrBk_Entry));
	memset(ab->arr, 0, count * sizeof(AdrBk_Entry));
    }

    if(deleted > 0){
	ab->del = (AdrBk_Entry *) fs_get(deleted * sizeof(AdrBk_Entry));
	memset(ab->del, 0, deleted * sizeof(AdrBk_Entry));
    }

    while((nickname = get_next_abook_entry(fp_in, rew)) != NULL){


	ae = NULL;
	rew = 0;
	is_deleted = 0;

	if(strncmp(nickname, DELETED, DELETED_LEN) == 0
	   && isdigit((unsigned char)nickname[DELETED_LEN])
	   && isdigit((unsigned char)nickname[DELETED_LEN+1])
	   && nickname[DELETED_LEN+2] == '/'
	   && isdigit((unsigned char)nickname[DELETED_LEN+3])
	   && isdigit((unsigned char)nickname[DELETED_LEN+4])
	   && nickname[DELETED_LEN+5] == '/'
	   && isdigit((unsigned char)nickname[DELETED_LEN+6])
	   && isdigit((unsigned char)nickname[DELETED_LEN+7])
	   && nickname[DELETED_LEN+8] == '#'){
	    is_deleted++;
	}

	ALARM_BLIP();
	if(!is_deleted && (long) used > MAX_ADRBK_SIZE){
	    q_status_message2(SM_ORDER | SM_DING, 4, 5,
		    "Max addrbook size is %.200s, %.200s too large, giving up",
			    long2string(MAX_ADRBK_SIZE),
			    (lc=last_cmpnt(ab->filename)) ? lc : ab->filename);
	    dprint((1, "build_ondisk: used=%ld > %s\n",
		   (long) used, long2string(MAX_ADRBK_SIZE)));
	    goto io_err;
	}

	if(is_deleted){
	    if(delused < ab->del_count)
	      ae = &ab->del[delused++];
	}
	else{
	    if(used < ab->count)
	      ae = &ab->arr[used++];
	}

	if(ae)
	  init_ae(ab, ae, nickname);

	fs_give((void **) &nickname);

	if(!ae || is_deleted)
	  continue;

	/*
	 * We're calculating the widths as we read in the data.
	 * We could just go with some default widths to save time.
	 */
        this_nick_width = 0;
	this_full_width = 0;
	this_addr_width = 0;
	this_fcc_width = 0;

	if(ae->nickname)
	  this_nick_width = (int) utf8_width(ae->nickname);

	if(ae->fullname)
	  this_full_width = (int) utf8_width(ae->fullname);

	if(ae->tag == Single){
	    if(ae->addr.addr)
	      this_addr_width = (int) utf8_width(ae->addr.addr);
	}
	else{
	    char **a2;

	    this_addr_width = 0;
	    for(a2 = ae->addr.list; *a2 != NULL; a2++)
	      this_addr_width = MAX(this_addr_width, (int) utf8_width(*a2));
	}

	if(ae->fcc)
	  this_fcc_width = (int) utf8_width(ae->fcc);

	max_nick = MAX(max_nick, this_nick_width);

	if(this_full_width > max_full){
	    full_three = full_two;
	    full_two   = max_full;
	    max_full   = this_full_width;
	}
	else if(this_full_width > full_two){
	    full_three = full_two;
	    full_two   = this_full_width;
	}
	else if(this_full_width > full_three){
	    full_three = this_full_width;
	}

	if(this_addr_width > max_addr){
	    addr_three = addr_two;
	    addr_two   = max_addr;
	    max_addr   = this_addr_width;
	}
	else if(this_addr_width > addr_two){
	    addr_three = addr_two;
	    addr_two   = this_addr_width;
	}
	else if(this_addr_width > addr_three){
	    addr_three = this_addr_width;
	}

	if(this_fcc_width > max_fcc){
	    fcc_three = fcc_two;
	    fcc_two   = max_fcc;
	    max_fcc   = this_fcc_width;
	}
	else if(this_fcc_width > fcc_two){
	    fcc_three = fcc_two;
	    fcc_two   = this_fcc_width;
	}
	else if(this_fcc_width > fcc_three){
	    fcc_three = this_fcc_width;
	}
    }

    widths = &ab->widths;
    widths->max_nickname_width  = MIN(max_nick, 99);
    widths->max_fullname_width  = MIN(max_full, 99);
    widths->max_addrfield_width = MIN(max_addr, 99);
    widths->max_fccfield_width  = MIN(max_fcc, 99);
    widths->third_biggest_fullname_width  = MIN(full_three, 99);
    widths->third_biggest_addrfield_width = MIN(addr_three, 99);
    widths->third_biggest_fccfield_width  = MIN(fcc_three, 99);

    dprint((9, "- build_abook_datastruct done -\n"));
    return 0;

io_err:
    if(warning && errno != 0){
	strncpy(warning, error_description(errno), warninglen);
	warning[warninglen-1] = '\0';
    }

    dprint((1, "build_ondisk: io_err: %s\n",
	   error_description(errno)));

    return -1;
}


/*
 * Builds the trees used for nickname and address lookups.
 */
int
build_abook_tries(AdrBk *ab, char *warning)
{
    adrbk_cntr_t entry_num;
    AdrBk_Entry *ae;
    int we_cancel = 0;

    if(!ab)
      return -1;

    dprint((9, "- build_abook_tries(%s) -\n", ab->filename));


    if(ab->nick_trie)
      free_abook_trie(&ab->nick_trie);

    if(ab->addr_trie)
      free_abook_trie(&ab->addr_trie);

    if(ab->full_trie)
      free_abook_trie(&ab->full_trie);

    if(ab->revfull_trie)
      free_abook_trie(&ab->revfull_trie);

    /*
     * Go through addrbook entries and add each to the tries it
     * belongs in.
     */
    for(entry_num = 0; entry_num < ab->count; entry_num++){
	ae = adrbk_get_ae(ab, (a_c_arg_t) entry_num);
	if(ae){
	    /* every nickname in the nick trie */
	    if(ae->nickname && ae->nickname[0])
	      add_entry_to_trie(&ab->nick_trie, ae->nickname, (a_c_arg_t) entry_num);

	    if(ae->fullname && ae->fullname[0]){
		char *reverse = NULL;
		char *forward = NULL;
		char *comma = NULL;

		/*
		 * We have some fullnames stored as Last, First. Put both in.
		 */
		if(ae->fullname[0] != '"'
		   && (comma=strindex(ae->fullname, ',')) != NULL
		   && comma - ae->fullname > 0){
		    forward = adrbk_formatname(ae->fullname, NULL, NULL);
		    if(forward && forward[0]){
			*comma = '\0';
			reverse = cpystr(ae->fullname);
			*comma = ',';
		    }
		    else{
			if(forward)
			  fs_give((void **) &forward);

			forward = ae->fullname;
		    }
		}
		else
		  forward = ae->fullname;

		if(forward){
		    char *addthis;

		    /*
		     * Make this add not only the full name as is (forward) but
		     * also add the middle name and last name (names after spaces).
		     * so that they'll be found.
		     */
		    for(addthis=forward;
			addthis && (*addthis);
			addthis = strindex(addthis, ' ')){
			while(*addthis == ' ')
			  addthis++;

			if(*addthis)
			  add_entry_to_trie(&ab->full_trie, addthis, (a_c_arg_t) entry_num);
		    }
		}

		if(reverse)
		  add_entry_to_trie(&ab->revfull_trie, reverse, (a_c_arg_t) entry_num);

		if(forward && forward != ae->fullname)
		  fs_give((void **) &forward);
	    }

	    if(ae->tag == Single && ae->addr.addr && ae->addr.addr[0]){
		char     buf[1000];
		char    *tmp_a_string, *simple_addr = NULL;
		ADDRESS *addr = NULL;
		char    *fakedomain = "@";

		/*
		 * Isolate the actual address out of ae.
		 */
		tmp_a_string = cpystr(ae->addr.addr);
		rfc822_parse_adrlist(&addr, tmp_a_string, fakedomain);
		if(tmp_a_string)
		  fs_give((void **) &tmp_a_string);

		if(addr){
		    if(addr->mailbox && addr->host
		       && !(addr->host[0] == '@' && addr->host[1] == '\0'))
		      simple_addr = simple_addr_string(addr, buf, sizeof(buf));

		    /*
		     * If the fullname wasn't set in the addrbook entry there
		     * may still be one that is part of the address. We need
		     * to be careful because we may be in the middle of opening
		     * the address book right now. Don't call something like
		     * our_build_address because it will probably re-open this
		     * same addrbook infinitely.
		     */
		    if(!(ae->fullname && ae->fullname[0])
		       && addr->personal && addr->personal[0])
		      add_entry_to_trie(&ab->full_trie, addr->personal,
					(a_c_arg_t) entry_num);

		    mail_free_address(&addr);
		}

		if(simple_addr)
		  add_entry_to_trie(&ab->addr_trie, simple_addr, (a_c_arg_t) entry_num);
	    }
	}
    }

    dprint((9, "- build_abook_tries done -\n"));

    if(we_cancel)
      cancel_busy_cue(-1);

    return 0;
}


void
add_entry_to_trie(AdrBk_Trie **head, char *str, a_c_arg_t entry_num)
{
    AdrBk_Trie *temp, *trail;
    char *addthis, *p, buf[1000];

    if(!head || !str || !*str)
      return;

    /* add as lower case */

    for(p = str; *p && ((*p & 0x80) || !isupper((unsigned char) *p)); p++)
      ;

    if(*p){
	strncpy(buf, str, sizeof(buf));
	buf[sizeof(buf)-1] = '\0';
	for(p = buf; *p; p++)
	  if(!(*p & 0x80) && isupper((unsigned char) *p))
	    *p = tolower(*p);

	addthis = buf;
    }
    else
      addthis = str;

    temp = trail = (*head);

    /*
     * Find way down the trie, adding missing nodes as we go.
     */
    for(p = addthis; *p;){
	if(temp == NULL){
	    temp = (AdrBk_Trie *) fs_get(sizeof(*temp));
	    memset(temp, 0, sizeof(*temp));
	    temp->value = *p;
	    temp->entrynum = NO_NEXT;

	    if(*head == NULL)
	      *head = temp;
	    else
	      trail->down = temp;
	}
	else{
	    while((temp != NULL) && (temp->value != *p)){
		trail = temp;
		temp = temp->right;
	    }

	    /* wasn't there, add new node */
	    if(temp == NULL){
		temp = (AdrBk_Trie *) fs_get(sizeof(*temp));
		memset(temp, 0, sizeof(*temp));
		temp->value = *p;
		temp->entrynum = NO_NEXT;
		trail->right = temp;
	    }
	}

	if(*(++p)){
	    trail = temp;
	    temp = temp->down;
	}
    }

    /*
     * If entrynum is already filled in there must be an entry with
     * the same nickname earlier in the abook. Use that earlier entry.
     */
    if(temp != NULL && temp->entrynum == NO_NEXT)
      temp->entrynum = (adrbk_cntr_t) entry_num;
}


/*
 * Returns entry_num of first entry with this nickname, else NO_NEXT.
 */
adrbk_cntr_t
lookup_nickname_in_trie(AdrBk *ab, char *nickname)
{
    if(!ab || !nickname || !ab->nick_trie)
      return(-1L);

    return(lookup_in_abook_trie(ab->nick_trie, nickname));
}


/*
 * Returns entry_num of first entry with this address, else NO_NEXT.
 */
adrbk_cntr_t
lookup_address_in_trie(AdrBk *ab, char *address)
{
    dprint((9, "lookup_address_in_trie: %s\n", ab ? (ab->addr_trie ? (address ? address : "?") : "null addr_trie") : "null ab"));
    if(!ab || !address || !ab->addr_trie)
      return(-1L);

    return(lookup_in_abook_trie(ab->addr_trie, address));
}


adrbk_cntr_t
lookup_in_abook_trie(AdrBk_Trie *t, char *str)
{
    char        *p, *lookthisup;
    char         buf[1000];
    adrbk_cntr_t ret = NO_NEXT;

    if(!t || !str)
      return(ret);

    /* make lookup case independent */

    for(p = str; *p && !(*p & 0x80) && islower((unsigned char) *p); p++)
      ;

    if(*p){
	strncpy(buf, str, sizeof(buf));
	buf[sizeof(buf)-1] = '\0';
	for(p = buf; *p; p++)
	  if(!(*p & 0x80) && isupper((unsigned char) *p))
	    *p = tolower(*p);

	lookthisup = buf;
    }
    else
      lookthisup = str;

    p = lookthisup;

    /*
     * We usually return out from inside the loop (unless str == "").
     */
    while(*p){
	/* search for character at this level */
	while(t->value != *p){
	    if(t->right == NULL)
	      return(ret);		/* no match */

	    t = t->right;
	}

	if(*++p == '\0')		/* end of str, a match */
	  return(t->entrynum);

	/* need to go down to match next character */
	if(t->down == NULL)		/* no match */
	  return(ret);

	t = t->down;
    }

    return(ret);
}


void
free_abook_trie(AdrBk_Trie **trie)
{
    if(trie){
	if(*trie){
	    free_abook_trie(&(*trie)->down);
	    free_abook_trie(&(*trie)->right);
	    fs_give((void **) trie);
	}
    }
}


/*
 * Returns pointer to start of next address book entry from disk file.
 * The return will be in raw form from the file with newlines still
 * embedded. Or NULL at end of file.
 *
 * If rew is set, rewind the file and start over at beginning.
 */
char *
get_next_abook_entry(FILE *fp, int rew)
{
    char       *returned_lines = NULL, *p;
    char        line[1024];
    static int  will_be_done_next_time = 0;
    static long next_nickname_offset   = 0L;
    size_t      lsize;
    long        offset, saved_offset;
    long        len = 0L;

#define CHUNKSIZE 500

    lsize = sizeof(line);

    if(rew){
	will_be_done_next_time = 0;
	rewind(fp);
	/* skip leading (bogus) continuation lines */
	do{
	    offset  = ftell(fp);
	    line[0] = '\0';
	    line[lsize-2] = '\0';
	    p = fgets(line, lsize, fp);

	    if(p == NULL)
	      return(NULL);

	    /* line is too long to fit, read the rest and discard */
	    while(line[lsize-2] != '\0' && line[lsize-2] != '\n'
		  && p != NULL){

		/* get next lsize-1 characters, leaving line[0] */
		line[lsize-2] = '\0';
		p = fgets(line+1, lsize-1, fp);
	    }
	}while(line[0] == SPACE);

	/* offset is start of first good line now */
	next_nickname_offset = offset;
    }

    if(will_be_done_next_time)
      return(NULL);

    /* we set this up in rew==1 case or on previous call to this routine */
    offset = next_nickname_offset;

    /*
     * The rest is working on finding the start of the next entry so
     * skip continuation lines
     */
    do{
	next_nickname_offset = ftell(fp);
	line[0] = '\0';
	line[lsize-2] = '\0';
	p = fgets(line, lsize, fp);

	/* line is too long to fit, read the rest and discard */
	while(line[lsize-2] != '\0' && line[lsize-2] != '\n'
	      && p != NULL){

	    /* get next lsize-1 characters, leaving line[0] */
	    line[lsize-2] = '\0';
	    p = fgets(line+1, lsize-1, fp);
	}
    }while(line[0] == SPACE);

    /* next_nickname_offset is start of next entry now */

    if(!line[0])
      will_be_done_next_time = 1;

    len = next_nickname_offset - offset;

    returned_lines = (char *) fs_get((len + 1) * sizeof(char));

    saved_offset = ftell(fp);
    if(fseek(fp, offset, 0)){
	dprint((2, "get_next_ab_entry: trouble fseeking\n"));
	len = 0;
	fs_give((void **) &returned_lines);
    }
    else{
	if(fread(returned_lines, sizeof(char), (unsigned) len, fp) != len){
	    dprint((2, "get_next_ab_entry: trouble freading\n"));
	    len = 0;
	    fs_give((void **) &returned_lines);
	}
    }

    if(fseek(fp, saved_offset, 0)){
	dprint((2, "get_next_ab_entry: trouble fseeking to saved_offset\n"));
	len = 0;
	fs_give((void **) &returned_lines);
    }

    if(returned_lines)
      returned_lines[len] = '\0';

    return(returned_lines);
}


/*
 * Returns a pointer to the start of the mailbox@host part of this
 * address string, and a pointer to the end + 1.  The caller can then
 * replace the end char with \0 and call the hash function, then put
 * back the end char.  Start_addr and end_addr are assumed to be non-null.
 */
void
strip_addr_string(char *addrstr, char **start_addr, char **end_addr)
{
    register char *q;
    int in_quotes  = 0,
        in_comment = 0;
    char prev_char = '\0';

    if(!addrstr || !*addrstr){
	*start_addr = NULL;
	*end_addr = NULL;
	return;
    }

    *start_addr = addrstr;

    for(q = addrstr; *q; q++){
	switch(*q){
	  case '<':
	    if(!in_quotes && !in_comment){
		if(*++q){
		    *start_addr = q;
		    /* skip to > */
		    while(*q && *q != '>')
		      q++;

		    /* found > */
		    if(*q){
			*end_addr = q;
			return;
		    }
		    else
		      q--;
		}
	    }

	    break;

	  case LPAREN:
	    if(!in_quotes && !in_comment)
	      in_comment = 1;
	    break;

	  case RPAREN:
	    if(in_comment && prev_char != BSLASH)
	      in_comment = 0;
	    break;

	  case QUOTE:
	    if(in_quotes && prev_char != BSLASH)
	      in_quotes = 0;
	    else if(!in_quotes && !in_comment)
	      in_quotes = 1;
	    break;

	  default:
	    break;
	}

	prev_char = *q;
    }

    *end_addr = q;
}


/*
 * Fill in the passed in ae pointer by parsing the str that is passed.
 * 
 * Args   ab --
 *        ae -- pointer we want to fill in. The individual members of
 *                the ae struct will be allocated here
 *       str -- the string from the on-disk address book file for this entry
 *
 * Returns a pointer to ae or NULL if there are problems.
 */
AdrBk_Entry *
init_ae(AdrBk *ab, AdrBk_Entry *a, char *str)
{
    char *p;
    char *addrfield = (char *) NULL;
    char *addrfield_end;
    char *nickname, *fullname, *fcc, *extra;

    if(!ab){
	dprint((2, "init_ae: found trouble: NULL ab\n"));
	return((AdrBk_Entry *) NULL);
    }

    defvalue_ae(a);

    p = str;

    REPLACE_NEWLINES_WITH_SPACE(p);

    nickname = p;
    SKIP_TO_TAB(p);
    if(!*p){
	RM_END_SPACE(nickname, p);
	a->nickname = cpystr(nickname);
    }
    else{
	*p = '\0';
	RM_END_SPACE(nickname, p);
	a->nickname = cpystr(nickname);
	p++;
	SKIP_SPACE(p);
	fullname = p;
	SKIP_TO_TAB(p);
	if(!*p){
	    RM_END_SPACE(fullname, p);
	    a->fullname = cpystr(fullname);
	}
	else{
	    *p = '\0';
	    RM_END_SPACE(fullname, p);
	    a->fullname = cpystr(fullname);
	    p++;
	    SKIP_SPACE(p);
	    addrfield = p;
	    SKIP_TO_TAB(p);
	    if(!*p){
		RM_END_SPACE(addrfield, p);
	    }
	    else{
		*p = '\0';
		RM_END_SPACE(addrfield, p);
		p++;
		SKIP_SPACE(p);
		fcc = p;
		SKIP_TO_TAB(p);
		if(!*p){
		    RM_END_SPACE(fcc, p);
		    a->fcc = cpystr(fcc);
		}
		else{
		    char *src, *dst;

		    *p = '\0';
		    RM_END_SPACE(fcc, p);
		    a->fcc = cpystr(fcc);
		    p++;
		    SKIP_SPACE(p);
		    extra = p;
		    p = extra + strlen(extra);
		    RM_END_SPACE(extra, p);

		    /*
		     * When we wrap long comments we insert an extra colon
		     * in the wrap so we can spot it and take it back out.
		     * Pretty much a hack since we thought of it a long
		     * time after designing it, but it eliminates the limit
		     * on length of comments. Here we are looking for
		     * <SP> <SP> : <SP> or
		     * <SP> <SP> <SP> : <SP> and replacing it with <SP>.
		     * There could have been a single \n or \r\n, so that
		     * is why we check for 2 or 3 spaces before the colon.
		     * (This was another mistake.)
		     */
		    dst = src = extra;
		    while(*src != '\0'){
			if(*src == SPACE && *(src+1) == SPACE &&
			   *(src+2) == ':' && *(src+3) == SPACE){

			    /*
			     * If there was an extra space because of the
			     * CRLF (instead of LF) then we already put
			     * a SP in dst last time through the loop
			     * and don't need to add another.
			     */
			    if(src == extra || *(src-1) != SPACE)
			      *dst++ = *src;

			    src += 4;
			}
			else
			  *dst++ = *src++;
		    }

		    *dst = '\0';
		    a->extra = cpystr(extra);
		}
	    }
	}
    }

    /* decode and convert to UTF-8 if we need to */

    convert_possibly_encoded_str_to_utf8(&a->nickname);
    convert_possibly_encoded_str_to_utf8(&a->fullname);
    convert_possibly_encoded_str_to_utf8(&a->fcc);
    convert_possibly_encoded_str_to_utf8(&a->extra);

    /* parse addrfield */
    if(addrfield){
	if(*addrfield == '('){  /* it's a list */
	    a->tag = List;
	    p = addrfield;
	    addrfield_end = p + strlen(p);

	    /*
	     * Get rid of the parens.
	     * If this isn't true the input file is messed up.
	     */
	    if(p[strlen(p)-1] == ')'){
		char **ll;

		p[strlen(p)-1] = '\0';
		p++;
		a->addr.list = parse_addrlist(p);
		for(ll = a->addr.list; ll && *ll; ll++)
		  convert_possibly_encoded_str_to_utf8(ll);
	    }
	    else{
		/* put back what was there to start with */
		*addrfield_end = ')';
		a->addr.list = (char **)fs_get(sizeof(char *) * 2);
		a->addr.list[0] = cpystr(addrfield);
		a->addr.list[1] = NULL;
		dprint((2, "parsing error reading addressbook: missing right paren: %s\n",
			addrfield ? addrfield : "?"));
	    }
	}
	else{  /* A plain, single address */

	    a->tag       = Single;
	    a->addr.addr = cpystr(addrfield);
	    convert_possibly_encoded_str_to_utf8(&a->addr.addr);
	}
    }
    else{
	/*
	 * If no addrfield, assume an empty Single.
	 */
	a->addr.addr = cpystr("");
	a->tag       = Single;
    }

    return(a);
}


/*
 * Return the size of the address book 
 */
adrbk_cntr_t
adrbk_count(AdrBk *ab)
{
    return(ab ? ab->count : (adrbk_cntr_t) 0);
}


/*
 * Return a pointer to the ae that has index number "entry_num".
 */
AdrBk_Entry *
adrbk_get_ae(AdrBk *ab, a_c_arg_t entry_num)
{
    if(!ab || entry_num >= (a_c_arg_t) ab->count)
      return((AdrBk_Entry *) NULL);

    return(&ab->arr[entry_num]);
}


/*
 * Return a pointer to the deleted ae that has index number "entry_num".
 */
AdrBk_Entry *
adrbk_get_delae(AdrBk *ab, a_c_arg_t entry_num)
{
    if(!ab || entry_num >= (a_c_arg_t) ab->del_count)
      return((AdrBk_Entry *) NULL);

    return(&ab->del[entry_num]);
}


/*
 * Look up an entry in the address book given a nickname
 *
 * Args: ab       -- the address book
 *       nickname -- nickname to match
 *      entry_num -- if matched, return entry_num of match here
 *
 * Result: A pointer to an AdrBk_Entry is returned, or NULL if not found.
 *
 * Lookups usually need to be recursive in case the address
 * book references itself.  This is left to the next level up.
 * adrbk_clearrefs() is provided to clear all the reference tags in
 * the address book for loop detection.
 * When there are duplicates of the same nickname we return the first.
 * This can only happen if addrbook was edited externally.
 */
AdrBk_Entry *
adrbk_lookup_by_nick(AdrBk *ab, char *nickname, adrbk_cntr_t *entry_num)
{
    adrbk_cntr_t num;
    AdrBk_Entry *ae;

    dprint((5, "- adrbk_lookup_by_nick(%s) (in %s) -\n",
	   nickname ? nickname : "?",
	   (ab && ab->filename) ? ab->filename : "?"));

    if(!ab || !nickname || !nickname[0])
      return NULL;


    num = lookup_nickname_in_trie(ab, nickname);

    if(num != NO_NEXT){
	ae = adrbk_get_ae(ab, (a_c_arg_t) num);
	if(entry_num && ae)
	  *entry_num = num;

	return(ae);
    }
    else
      return((AdrBk_Entry *) NULL);
}


/*
 * Look up an entry in the address book given an address
 *
 * Args: ab       -- the address book
 *       address  -- address to match
 *      entry_num -- if matched, return entry_num of match here
 *
 * Result: A pointer to an AdrBk_Entry is returned, or NULL if not found.
 *
 * Note:  When there are multiple occurrences of an address in an addressbook,
 * which there will be if more than one nickname points to same address, then
 * we want this to match the first occurrence so that the fcc you get will
 * be predictable.
 */
AdrBk_Entry *
adrbk_lookup_by_addr(AdrBk *ab, char *address, adrbk_cntr_t *entry_num)
{
    adrbk_cntr_t num;
    AdrBk_Entry *ae;

    dprint((5, "- adrbk_lookup_by_addr(%s) (in %s) -\n",
	   address ? address : "?",
	   (ab && ab->filename) ? ab->filename : "?"));

    if(!ab || !address || !address[0])
      return((AdrBk_Entry *)NULL);

    num = lookup_address_in_trie(ab, address);

    if(num != NO_NEXT){
	ae = adrbk_get_ae(ab, (a_c_arg_t) num);
	if(entry_num && ae)
	  *entry_num = num;

	return(ae);
    }
    else
      return((AdrBk_Entry *)NULL);
}


/*
 * Format a full name.
 *
 * Args: fullname -- full name out of address book for formatting
 *          first -- Return a pointer to first name here.
 *           last -- Return a pointer to last name here.
 *
 * Result:  Returns pointer to name formatted for a mail header. Space is
 *          allocated here and should be freed by caller.
 *
 * We need this because we store full names as Last, First.
 * If the name has no comma, then no change is made.
 * Otherwise the text before the first comma is moved to the end and
 * the comma is deleted.
 *
 * Last and first have to be freed by caller.
 */
char *
adrbk_formatname(char *fullname, char **first, char **last)
{
    char       *comma;
    char       *new_name;

    if(first)
      *first = NULL;
    if(last)
      *last = NULL;

    /*
     * There is an assumption that the fullname is a UTF-8 string.
     */

    if(fullname[0] != '"'  && (comma = strindex(fullname, ',')) != NULL){
	size_t l;
        int last_name_len = comma - fullname;

        comma++;
        while(*comma && isspace((unsigned char)*comma))
	  comma++;

	if(first)
	  *first = cpystr(comma);

	if(last){
	    *last = (char *)fs_get((last_name_len + 1) * sizeof(char));
	    strncpy(*last, fullname, last_name_len); 
	    (*last)[last_name_len] = '\0';
	}

	l = strlen(comma) + 1 + last_name_len;
	new_name = (char *) fs_get((l+1) * sizeof(char));
        strncpy(new_name, comma, l);
	new_name[l] = '\0';
        strncat(new_name, " ", l+1-1-strlen(new_name));
	new_name[l] = '\0';
        strncat(new_name, fullname, MIN(last_name_len,l+1-1-strlen(new_name))); 
	new_name[l] = '\0';
    }
    else
      new_name = cpystr(fullname);

    return(new_name);
}


/*
 * Clear reference flags in preparation for a recursive lookup.
 *
 * For loop detection during address book look up.  This clears all the 
 * referenced flags, then as the lookup proceeds the referenced flags can 
 * be checked and set.
 */
void
adrbk_clearrefs(AdrBk *ab)
{
    adrbk_cntr_t entry_num;
    AdrBk_Entry *ae;

    dprint((9, "- adrbk_clearrefs -\n"));

    if(!ab)
      return;

    for(entry_num = 0; entry_num < ab->count; entry_num++){
	ae = adrbk_get_ae(ab, (a_c_arg_t) entry_num);
	ae->referenced = 0;
    }
}


/*
 *  Allocate a new AdrBk_Entry
 */
AdrBk_Entry *
adrbk_newentry(void)
{
    AdrBk_Entry *ae;

    ae = (AdrBk_Entry *) fs_get(sizeof(AdrBk_Entry));
    defvalue_ae(ae);

    return(ae);
}


/*
 *  Just sets ae values to default.
 *  Parts should be freed before calling this or they will leak.
 */
void
defvalue_ae(AdrBk_Entry *ae)
{
    ae->nickname    = empty;
    ae->fullname    = empty;
    ae->addr.addr   = empty;
    ae->fcc         = empty;
    ae->extra       = empty;
    ae->tag         = NotSet;
    ae->referenced  = 0;
}


AdrBk_Entry *
copy_ae(AdrBk_Entry *src)
{
    AdrBk_Entry *a;

    a = adrbk_newentry();
    a->tag = src->tag;
    a->nickname = cpystr(src->nickname ? src->nickname : "");
    a->fullname = cpystr(src->fullname ? src->fullname : "");
    a->fcc      = cpystr(src->fcc ? src->fcc : "");
    a->extra    = cpystr(src->extra ? src->extra : "");
    if(a->tag == Single)
      a->addr.addr = cpystr(src->addr.addr ? src->addr.addr : "");
    else if(a->tag == List){
	char **p;
	int    i, n;

	/* count list */
	for(p = src->addr.list; p && *p; p++)
	  ;/* do nothing */
	
	if(p == NULL)
	  n = 0;
	else
	  n = p - src->addr.list;

	a->addr.list = (char **)fs_get((n+1) * sizeof(char *));
	for(i = 0; i < n; i++)
	  a->addr.list[i] = cpystr(src->addr.list[i]);
	
	a->addr.list[n] = NULL;
    }

    return(a);
}


/*
 * Add an entry to the address book, or modify an existing entry
 *
 * Args: ab       -- address book to add to
 *  old_entry_num -- the entry we want to modify.  If this is NO_NEXT, then
 *                    we look up the nickname passed in to see if that's the
 *                    entry to modify, else it is a new entry.
 *       nickname -- the nickname for new entry
 *       fullname -- the fullname for new entry
 *       address  -- the address for new entry
 *       fcc      -- the fcc for new entry
 *       extra    -- the extra field for new entry
 *       tag      -- the type of new entry
 *  new_entry_num -- return entry_num of new or modified entry here
 * resort_happened -- means that more than just the current entry changed,
 *                     either something was added or order was changed
 *     enable_intr -- tell adrbk_write to enable interrupt handling
 *        be_quiet -- tell adrbk_write to not do percent done messages
 *        write_it -- only do adrbk_write if this is set
 *
 * Result: return code:  0 all went well
 *                      -2 error writing address book, check errno
 *		        -3 no modification, the tag given didn't match
 *                         existing tag
 *                      -4 tabs are in one of the fields passed in
 *
 * If the nickname exists in the address book already, the operation is
 * considered a modification even if the case does not match exactly,
 * otherwise it is an add.  The entry the operation occurs on is returned
 * in new.  All fields are set to those passed in; that is, passing in NULL
 * even on a modification will set those fields to NULL as opposed to leaving
 * them unchanged.  It is acceptable to pass in the current strings
 * in the entry in the case of modification.  For address lists, the
 * structure passed in is what is used, so the storage has to all have
 * come from fs_get().  If the pointer passed in is the same as
 * the current field, no change is made.
 */
int
adrbk_add(AdrBk *ab, a_c_arg_t old_entry_num, char *nickname, char *fullname,
	  char *address, char *fcc, char *extra, Tag tag, adrbk_cntr_t *new_entry_num,
	  int *resort_happened, int enable_intr, int be_quiet, int write_it)
{
    AdrBk_Entry *a;
    AdrBk_Entry *ae;
    adrbk_cntr_t old_enum;
    adrbk_cntr_t new_enum;
    int (*cmp_func)();
    int retval = 0;
    int need_write = 0;
    int set_mangled = 0;

    dprint((3, "- adrbk_add(%s) -\n", nickname ? nickname : ""));

    if(!ab)
      return -2;

    /* ---- Make sure there are no tabs in the stuff to add ------*/
    if((nickname != NULL && strindex(nickname, TAB) != NULL) ||
       (fullname != NULL && strindex(fullname, TAB) != NULL) ||
       (fcc != NULL && strindex(fcc, TAB) != NULL) ||
       (tag == Single && address != NULL && strindex(address, TAB) != NULL))
        return -4;

    /*
     * Are we adding or updating ?
     *
     * If old_entry_num was passed in, we're updating that.  If nickname
     * already exists, we're updating that entry.  Otherwise, this is an add.
     */
    if((adrbk_cntr_t)old_entry_num != NO_NEXT){
	ae = adrbk_get_ae(ab, old_entry_num);
	if(ae)
	  old_enum = (adrbk_cntr_t)old_entry_num;
    }
    else
      ae = adrbk_lookup_by_nick(ab, nickname, &old_enum);

    if(ae == NULL){  /*----- adding a new entry ----*/

	ae            = adrbk_newentry();
	ae->tag       = tag;
	if(nickname)
	  ae->nickname  = cpystr(nickname);
	if(fullname)
	  ae->fullname  = cpystr(fullname);
	if(fcc)
	  ae->fcc  = cpystr(fcc);
	if(extra)
	  ae->extra  = cpystr(extra);

	if(tag == Single)
          ae->addr.addr = cpystr(address);
	else
	  ae->addr.list = (char **)NULL;

	cmp_func = (ab->sort_rule == AB_SORT_RULE_FULL_LISTS) ?
						cmp_ae_by_full_lists_last :
		   (ab->sort_rule == AB_SORT_RULE_FULL) ?
						cmp_ae_by_full :
		   (ab->sort_rule == AB_SORT_RULE_NICK_LISTS) ?
						cmp_ae_by_nick_lists_last :
		/* (ab->sort_rule == AB_SORT_RULE_NICK) */
						cmp_ae_by_nick;

	if(ab->sort_rule == AB_SORT_RULE_NONE)  /* put it last */
	  new_enum = ab->count;
	else  /* Find slot for it */
	  for(new_enum = 0, a = adrbk_get_ae(ab, (a_c_arg_t) new_enum);
	      a != (AdrBk_Entry *)NULL;
	      a = adrbk_get_ae(ab, (a_c_arg_t) (++new_enum))){
		    if((*cmp_func)((qsort_t *)&a, (qsort_t *)&ae) >= 0)
			break;
	  }

	/* Insert ae before entry new_enum. */
	insert_ab_entry(ab, (a_c_arg_t) new_enum, ae, 0);

	if(F_OFF(F_EXPANDED_DISTLISTS,ps_global))
	  exp_add_nth(ab->exp, (a_c_arg_t)new_enum);

	exp_add_nth(ab->selects, (a_c_arg_t)new_enum);

	/*
	 * insert_ab_entry copies the pointers of ae so the things
	 * being pointed to (nickname, ...) are still in use.
	 * Don't free them but free the ae struct itself.
	 */
	fs_give((void **) &ae);

        /*---- return in pointer if requested -----*/
        if(new_entry_num)
	  *new_entry_num = new_enum;

        if(resort_happened)
	  *resort_happened = 1;

	if(write_it)
	  retval = adrbk_write(ab, (a_c_arg_t) new_enum, new_entry_num, &set_mangled,
			       enable_intr, be_quiet);
    }
    else{
        /*----- Updating an existing entry ----*/

	int fix_tries = 0;

	if(ae->tag != tag)
	  return -3;

        /*
	 * Instead of just freeing and reallocating here we attempt to re-use
	 * the space that was already allocated if possible.
	 */
	if(ae->nickname != nickname
	   && ae->nickname != NULL
	   && nickname != NULL
	   && strcmp(nickname, ae->nickname) != 0){
	    need_write++;
	    fix_tries++;
	    /* can use already alloc'd space */
            if(ae->nickname != NULL && nickname != NULL &&
	       strlen(nickname) <= strlen(ae->nickname)){

                strncpy(ae->nickname, nickname, strlen(ae->nickname)+1);
            }
	    else{
                if(ae->nickname != NULL && ae->nickname != empty)
                  fs_give((void **)&ae->nickname);

                ae->nickname = nickname ? cpystr(nickname) : nickname;
	    }
        }

	if(ae->fullname != fullname
	   && ae->fullname != NULL
	   && fullname != NULL
	   && strcmp(fullname, ae->fullname) != 0){
	    need_write++;
            if(ae->fullname != NULL && fullname != NULL &&
	       strlen(fullname) <= strlen(ae->fullname)){

                strncpy(ae->fullname, fullname, strlen(ae->fullname)+1);
            }
	    else{
                if(ae->fullname != NULL && ae->fullname != empty)
                  fs_give((void **)&ae->fullname);

                ae->fullname = fullname ? cpystr(fullname) : fullname;
	    }
        }

	if(ae->fcc != fcc
	   && ae->fcc != NULL
	   && fcc != NULL
	   && strcmp(fcc, ae->fcc) != 0){
	    need_write++;
            if(ae->fcc != NULL && fcc != NULL &&
	       strlen(fcc) <= strlen(ae->fcc)){

                strncpy(ae->fcc, fcc, strlen(ae->fcc)+1);
            }
	    else{
                if(ae->fcc != NULL && ae->fcc != empty)
                  fs_give((void **)&ae->fcc);

                ae->fcc = fcc ? cpystr(fcc) : fcc;
	    }
        }

	if(ae->extra != extra
	   && ae->extra != NULL
	   && extra != NULL
	   && strcmp(extra, ae->extra) != 0){
	    need_write++;
            if(ae->extra != NULL && extra != NULL &&
	       strlen(extra) <= strlen(ae->extra)){

                strncpy(ae->extra, extra, strlen(ae->extra)+1);
            }
	    else{
                if(ae->extra != NULL && ae->extra != empty)
                  fs_give((void **)&ae->extra);

                ae->extra = extra ? cpystr(extra) : extra;
	    }
        }

	if(tag == Single){
            /*---- Single ----*/
	    if(ae->addr.addr != address
	       && ae->addr.addr != NULL
	       && address != NULL
	       && strcmp(address, ae->addr.addr) != 0){
		need_write++;
		fix_tries++;
		if(ae->addr.addr != NULL && address != NULL &&
		   strlen(address) <= strlen(ae->addr.addr)){

		    strncpy(ae->addr.addr, address, strlen(ae->addr.addr)+1);
		}
		else{
		    if(ae->addr.addr != NULL && ae->addr.addr != empty)
		      fs_give((void **)&ae->addr.addr);

		    ae->addr.addr = address ? cpystr(address) : address;
		}
	    }
	}
	else{
            /*---- List -----*/
            /*
	     * We don't mess with lists here.
	     * The caller has to do it with adrbk_listadd().
	     */
	    ;/* do nothing */
	}


        /*---------- Make sure it's still in order ---------*/

	/*
	 * old_enum is where ae is currently located
	 * put it where it belongs
	 */
	if(need_write){
	    new_enum = re_sort_particular_entry(ab, (a_c_arg_t) old_enum);
	    if(old_enum != new_enum)
	      fix_tries++;
	}
	else
	  new_enum = old_enum;

        /*---- return in pointer if requested -----*/
        if(new_entry_num)
	  *new_entry_num = new_enum;

        if(resort_happened)
	  *resort_happened = (old_enum != new_enum);

	if(fix_tries)
	  repair_abook_tries(ab);

	if(write_it && need_write){
	    int sort_happened = 0;

	    retval = adrbk_write(ab, (a_c_arg_t) new_enum, new_entry_num,
			         &sort_happened, enable_intr, be_quiet);

	    set_mangled = sort_happened;

	    if(new_entry_num)
	      new_enum = (*new_entry_num);

	    if(resort_happened && (sort_happened || (old_enum != new_enum)))
	      *resort_happened = 1;
	}
	else
	  retval = 0;
    }

    if(set_mangled)
      ps_global->mangled_screen = 1;

    return(retval);
}


/*
 * Similar to adrbk_add, but lower cost. No sorting is done, the new entry
 * goes on the end. This won't work if it is an edit instead of an append.
 * The address book is not committed to disk.
 *
 * Args: ab       -- address book to add to
 *       nickname -- the nickname for new entry
 *       fullname -- the fullname for new entry
 *       address  -- the address for new entry
 *       fcc      -- the fcc for new entry
 *       extra    -- the extra field for new entry
 *       tag      -- the type of new entry
 *
 * Result: return code:  0 all went well
 *                      -2 error writing address book, check errno
 *		        -3 no modification, the tag given didn't match
 *                         existing tag
 *                      -4 tabs are in one of the fields passed in
 */
int
adrbk_append(AdrBk *ab, char *nickname, char *fullname, char *address, char *fcc,
	     char *extra, Tag tag, adrbk_cntr_t *new_entry_num)
{
    AdrBk_Entry *ae;

    dprint((3, "- adrbk_append(%s) -\n", nickname ? nickname : ""));

    if(!ab)
      return -2;

    /* ---- Make sure there are no tabs in the stuff to add ------*/
    if((nickname != NULL && strindex(nickname, TAB) != NULL) ||
       (fullname != NULL && strindex(fullname, TAB) != NULL) ||
       (fcc != NULL && strindex(fcc, TAB) != NULL) ||
       (tag == Single && address != NULL && strindex(address, TAB) != NULL))
        return -4;

    ae            = adrbk_newentry();
    ae->tag       = tag;
    if(nickname)
      ae->nickname  = cpystr(nickname);
    if(fullname)
      ae->fullname  = cpystr(fullname);
    if(fcc)
      ae->fcc  = cpystr(fcc);
    if(extra)
      ae->extra  = cpystr(extra);

    if(tag == Single)
      ae->addr.addr = cpystr(address);
    else
      ae->addr.list = (char **)NULL;

    if(new_entry_num)
      *new_entry_num = ab->count;

    insert_ab_entry(ab, (a_c_arg_t) ab->count, ae, 0);

    /*
     * insert_ab_entry copies the pointers of ae so the things
     * being pointed to (nickname, ...) are still in use.
     * Don't free them but free the ae struct itself.
     */
    fs_give((void **) &ae);

    return(0);
}


/*
 * The entire address book is assumed sorted correctly except perhaps for
 * entry number cur.  Put it in the correct place.  Return the new entry
 * number for cur.
 */
adrbk_cntr_t
re_sort_particular_entry(AdrBk *ab, a_c_arg_t cur)
{
    AdrBk_Entry  *ae_cur, *ae_prev, *ae_next, *ae_small_enough, *ae_big_enough;
    long small_enough;
    adrbk_cntr_t big_enough;
    adrbk_cntr_t new_entry_num;
    int (*cmp_func)(const qsort_t *, const qsort_t *);

    dprint((9, "- re_sort -\n"));

    cmp_func = (ab->sort_rule == AB_SORT_RULE_FULL_LISTS) ?
					    cmp_ae_by_full_lists_last :
	       (ab->sort_rule == AB_SORT_RULE_FULL) ?
					    cmp_ae_by_full :
	       (ab->sort_rule == AB_SORT_RULE_NICK_LISTS) ?
					    cmp_ae_by_nick_lists_last :
	    /* (ab->sort_rule == AB_SORT_RULE_NICK) */
					    cmp_ae_by_nick;

    new_entry_num = (adrbk_cntr_t) cur;

    if(ab->sort_rule == AB_SORT_RULE_NONE)
      return(new_entry_num);

    ae_cur = adrbk_get_ae(ab, cur);

    if(cur > 0)
      ae_prev  = adrbk_get_ae(ab, cur - 1);

    if(cur < ab->count -1)
      ae_next  = adrbk_get_ae(ab, cur + 1);

    /*
     * A possible optimization here would be to implement some sort of
     * binary search to find where it goes instead of stepping through the
     * entries one at a time.
     */
    if(cur > 0 &&
       (*cmp_func)((qsort_t *)&ae_cur,(qsort_t *)&ae_prev) < 0){
	/*--- Out of order, needs to be moved up ----*/
	for(small_enough = (long)cur - 2; small_enough >= 0L; small_enough--){
	  ae_small_enough = adrbk_get_ae(ab,(a_c_arg_t) small_enough);
	  if((*cmp_func)((qsort_t *)&ae_cur, (qsort_t *)&ae_small_enough) >= 0)
	    break;
	}
	new_entry_num = (adrbk_cntr_t)(small_enough + 1L);
	move_ab_entry(ab, cur, (a_c_arg_t) new_entry_num);
    }
    else if(cur < ab->count - 1 &&
	(*cmp_func)((qsort_t *)&ae_cur, (qsort_t *)&ae_next) > 0){
	/*---- Out of order needs, to be moved towards end of list ----*/
	for(big_enough = (adrbk_cntr_t)(cur + 2);
	    big_enough < ab->count;
	    big_enough++){
	  ae_big_enough = adrbk_get_ae(ab, (a_c_arg_t) big_enough);
	  if((*cmp_func)((qsort_t *)&ae_cur, (qsort_t *)&ae_big_enough) <= 0)
	    break;
	}
	new_entry_num = big_enough - 1;
	move_ab_entry(ab, cur, (a_c_arg_t) big_enough);
    }

    dprint((9, "- re_sort done -\n"));

    return(new_entry_num);
}


/*
 * Delete an entry from the address book
 *
 * Args: ab        -- the address book
 *       entry_num -- entry to delete
 *    save_deleted -- save deleted as a #DELETED- entry
 *     enable_intr -- tell adrbk_write to enable interrupt handling
 *        be_quiet -- tell adrbk_write to not do percent done messages
 *        write_it -- only do adrbk_write if this is set
 *
 * Result: returns:  0 if all went well
 *                  -1 if there is no such entry
 *                  -2 error writing address book, check errno
 */
int
adrbk_delete(AdrBk *ab, a_c_arg_t entry_num, int save_deleted, int enable_intr,
	     int be_quiet, int write_it)
{
    int retval = 0;
    int set_mangled = 0;

    dprint((3, "- adrbk_delete(%ld) -\n", (long)entry_num));

    if(!ab)
      return -2;

    delete_ab_entry(ab, entry_num, save_deleted);
    if(F_OFF(F_EXPANDED_DISTLISTS,ps_global))
      exp_del_nth(ab->exp, entry_num);

    exp_del_nth(ab->selects, entry_num);

    if(write_it)
      retval = adrbk_write(ab, 0, NULL, &set_mangled, enable_intr, be_quiet);

    if(set_mangled)
      ps_global->mangled_screen = 1;
    
    return(retval);
}


/*
 * Delete an address out of an address list
 *
 * Args: ab    -- the address book
 *   entry_num -- the address list we are deleting from
 *       addr  -- address in above list to be deleted
 *
 * Result: 0: Deletion complete, address book written
 *        -1: Address for deletion not found
 *        -2: Error writing address book. Check errno.
 *
 * The address to be deleted is located by matching the string.
 */
int
adrbk_listdel(AdrBk *ab, a_c_arg_t entry_num, char *addr)
{
    char **p, *to_free;
    AdrBk_Entry *ae;
    int ret;
    int set_mangled = 0;

    dprint((3, "- adrbk_listdel(%ld) -\n", (long) entry_num));

    if(!ab || entry_num >= ab->count)
      return -2;

    if(!addr)
      return -1;

    ae = adrbk_get_ae(ab, entry_num);

    if(ae->tag != List)
      return -1;

    for(p = ae->addr.list; *p; p++) 
      if(strcmp(*p, addr) == 0)
        break;

    if(*p == NULL)
      return -1;

    /* note storage to be freed */
    if(*p != empty)
      to_free = *p;
    else
      to_free = NULL;

    /* slide all the entries below up (including NULL) */
    for(; *p; p++)
      *p = *(p+1);

    if(to_free)
      fs_give((void **) &to_free);

    ret = adrbk_write(ab, 0, NULL, &set_mangled, 1, 0);

    if(set_mangled)
      ps_global->mangled_screen = 1;

    return(ret);
}


/*
 * Delete all addresses out of an address list
 *
 * Args: ab    -- the address book
 *   entry_num -- the address list we are deleting from
 *
 * Result: 0: Deletion complete, address book written
 *        -1: Address for deletion not found
 *        -2: Error writing address book. Check errno.
 */
int
adrbk_listdel_all(AdrBk *ab, a_c_arg_t entry_num)
{
    char **p;
    AdrBk_Entry *ae;

    dprint((3, "- adrbk_listdel_all(%ld) -\n", (long) entry_num));

    if(!ab || entry_num >= ab->count)
      return -2;

    ae = adrbk_get_ae(ab, entry_num);

    if(ae->tag != List)
      return -1;

    /* free old list */
    for(p = ae->addr.list; p && *p; p++) 
      if(*p != empty)
        fs_give((void **)p);
    
    if(ae->addr.list)
      fs_give((void **) &ae->addr.list);

    ae->addr.list = NULL;

    return 0;
}


/*
 * Add a list of addresses to an already existing address list
 *
 * Args: ab        -- the address book
 *       entry_num -- the address list we are adding to
 *       addrs     -- address list to be added
 *     enable_intr -- tell adrbk_write to enable interrupt handling
 *        be_quiet -- tell adrbk_write to not do percent done messages
 *        write_it -- only do adrbk_write if this is set
 *
 * Result: returns 0 : addition made, address book written
 *                -1 : addition to non-list attempted
 *                -2 : error writing address book -- check errno
 */
int
adrbk_nlistadd(AdrBk *ab, a_c_arg_t entry_num, adrbk_cntr_t *new_entry_num,
	       int *resort_happened, char **addrs,
	       int enable_intr, int be_quiet, int write_it)
{
    char **p;
    int    cur_size, size_of_additional_list, new_size;
    int    i, rc = 0;
    int    set_mangled = 0;
    AdrBk_Entry *ae;

    dprint((3, "- adrbk_nlistadd(%ld) -\n", (long) entry_num));

    if(!ab || entry_num >= ab->count)
      return -2;

    ae = adrbk_get_ae(ab, entry_num);

    if(ae->tag != List)
      return -1;

    /* count up size of existing list */    
    for(p = ae->addr.list; p != NULL && *p != NULL; p++)
      ;/* do nothing */

    cur_size = p - ae->addr.list;

    /* count up size of new list */    
    for(p = addrs; p != NULL && *p != NULL; p++)
      ;/* do nothing */

    size_of_additional_list = p - addrs;
    new_size = cur_size + size_of_additional_list;

    /* make room at end of list for it */
    if(cur_size == 0)
      ae->addr.list = (char **) fs_get(sizeof(char *) * (new_size + 1));
    else
      fs_resize((void **) &ae->addr.list, sizeof(char *) * (new_size + 1));

    /* Put new list at the end */
    for(i = cur_size; i < new_size; i++)
      (ae->addr.list)[i] = cpystr(addrs[i - cur_size]);

    (ae->addr.list)[new_size] = NULL;

    /*---- sort it into the correct place ------*/
    if(ab->sort_rule != AB_SORT_RULE_NONE)
      sort_addr_list(ae->addr.list);

    if(write_it)
      rc = adrbk_write(ab, entry_num, new_entry_num, &set_mangled, enable_intr, be_quiet);

    if(set_mangled){
	ps_global->mangled_screen = 1;
	if(resort_happened)
	  *resort_happened = 1;
    }

    return(rc);
}


/*
 * Set the valid variable if we determine that the address book has
 * been changed by something other than us. This means we should update
 * our view of the address book when next possible.
 *
 * Args    ab -- AdrBk handle
 *  do_it_now -- If > 0, check now regardless
 *               If = 0, check if time since last chk more than default
 *               If < 0, check if time since last chk more than -do_it_now
 */
void
adrbk_check_validity(AdrBk *ab, long int do_it_now)
{
    dprint((9, "- adrbk_check_validity(%s) -\n",
	   (ab && ab->filename) ? ab->filename : ""));

    if(!ab || ab->flags & FILE_OUTOFDATE)
      return;

    adrbk_check_local_validity(ab, do_it_now);

    if(ab->type == Imap && !(ab->flags & FILE_OUTOFDATE ||
			     ab->rd->flags & REM_OUTOFDATE))
      rd_check_remvalid(ab->rd, do_it_now);
}


/*
 * Set the valid variable if we determine that the address book has
 * been changed by something other than us. This means we should update
 * our view of the address book when next possible.
 *
 * Args    ab -- AdrBk handle
 *  do_it_now -- If > 0, check now regardless
 *               If = 0, check if time since last chk more than default
 *               If < 0, check if time since last chk more than -do_it_now
 */
void
adrbk_check_local_validity(AdrBk *ab, long int do_it_now)
{
    time_t mtime, chk_interval;

    dprint((9, "- adrbk_check_local_validity(%s) -\n",
	   (ab && ab->filename) ? ab->filename : ""));

    if(!ab)
      return;

    if(do_it_now < 0L){
	chk_interval = -1L * do_it_now;
	do_it_now = 0L;
    }
    else
      chk_interval = FILE_VALID_CHK_INTERVAL;

    if(!do_it_now &&
       get_adj_time() <= ab->last_local_valid_chk + chk_interval)
      return;
    
    ab->last_local_valid_chk = get_adj_time();

    /*
     * Check local file for a modification time change.
     * If this is out of date, don't even bother checking for the remote
     * folder being out of date. That will get fixed when we reopen.
     */
    if(!(ab->flags & FILE_OUTOFDATE) &&
       ab->last_change_we_know_about != (time_t)(-1) &&
       (mtime=get_adj_name_file_mtime(ab->filename)) != (time_t)(-1) &&
       ab->last_change_we_know_about != mtime){

	dprint((2, "adrbk_check_local_validity: addrbook %s has changed\n",
		ab->filename ? ab->filename : "?"));
	ab->flags |= FILE_OUTOFDATE;
    }
}


/*
 * See if we can re-use an existing stream.
 * 
 * [ We don't believe we need this anymore now that the stuff in pine.c ]
 * [ is recycling streams for us, but we haven't thought it through all ]
 * [ the way enough to get rid of this. Hubert 2003-07-09               ]
 *
 * Args  name     -- Name of folder we want a stream for.
 *
 * Returns -- A mail stream suitable for status cmd or append cmd.
 *            NULL if none were available.
 */
MAILSTREAM *
adrbk_handy_stream(char *name)
{
    MAILSTREAM *stat_stream = NULL;
    int         i;

    dprint((9, "- adrbk_handy_stream(%s) -\n", name ? name : "?"));

    stat_stream = sp_stream_get(name, SP_SAME);
    
    /*
     * Look through our imap streams to see if there is one we can use.
     */
    for(i = 0; !stat_stream && i < as.n_addrbk; i++){
	PerAddrBook *pab;

	pab = &as.adrbks[i];

	if(pab->address_book &&
	   pab->address_book->type == Imap &&
	   pab->address_book->rd &&
	   pab->address_book->rd->type == RemImap &&
	   same_stream(name, pab->address_book->rd->t.i.stream)){
	    stat_stream = pab->address_book->rd->t.i.stream;
	    pab->address_book->rd->last_use = get_adj_time();
	    dprint((7,
		   "%s: used other abook stream for status (%ld)\n",
		   pab->address_book->orig_filename
		     ? pab->address_book->orig_filename : "?",
		   (long)pab->address_book->rd->last_use));
	}
    }

    dprint((9, "adrbk_handy_stream: returning %s\n",
	   stat_stream ? "good stream" : "NULL"));

    return(stat_stream);
}


/*
 * Close address book
 *
 * All that is done here is to free the storage, since the address book is 
 * rewritten on every change.
 */
void
adrbk_close(AdrBk *ab)
{
    int we_cancel = 0;

    dprint((4, "- adrbk_close(%s) -\n",
	   (ab && ab->filename) ? ab->filename : ""));

    if(!ab)
      return;

    if(ab->rd)
      rd_close_remdata(&ab->rd);

    if(ab->fp)
      (void)fclose(ab->fp);

    if(ab->our_filecopy && ab->filename != ab->our_filecopy){
	our_unlink(ab->our_filecopy);
	fs_give((void**) &ab->our_filecopy);
    }

    if(ab->exp){
	exp_free(ab->exp);
	fs_give((void **)&ab->exp);  /* free head of list, too */
    }

    if(ab->checks){
	exp_free(ab->checks);
	fs_give((void **)&ab->checks);  /* free head of list, too */
    }

    if(ab->selects){
	exp_free(ab->selects);
	fs_give((void **)&ab->selects);  /* free head of list, too */
    }

    if(ab->arr){
	adrbk_cntr_t entry_num;

	/*
	 * ab->arr is an allocated array. Each element of the array contains
	 * several pointers that point to other allocated stuff, so we
	 * free_ae_parts to get those and then free the array after the loop.
	 */
	for(entry_num = 0; entry_num < ab->count; entry_num++)
	  free_ae_parts(&ab->arr[entry_num]);

	fs_give((void **) &ab->arr);
    }

    if(ab->del){
	adrbk_cntr_t entry_num;

	for(entry_num = 0; entry_num < ab->del_count; entry_num++)
	  free_ae_parts(&ab->del[entry_num]);

	fs_give((void **) &ab->del);
    }

    if(ab->nick_trie)
      free_abook_trie(&ab->nick_trie);

    if(ab->addr_trie)
      free_abook_trie(&ab->addr_trie);

    if(ab->full_trie)
      free_abook_trie(&ab->full_trie);

    if(ab->revfull_trie)
      free_abook_trie(&ab->revfull_trie);

    if(we_cancel)
      cancel_busy_cue(0);

    if(ab->filename){
	if(ab->flags & DEL_FILE)
	  our_unlink(ab->filename);

	fs_give((void**)&ab->filename);
    }

    if(ab->orig_filename)
      fs_give((void**)&ab->orig_filename);

    fs_give((void **) &ab);
}


void
adrbk_partial_close(AdrBk *ab)
{
    dprint((4, "- adrbk_partial_close(%s) -\n",
	   (ab && ab->filename) ? ab->filename : ""));

    exp_free(ab->exp);		/* leaves head of list */
    exp_free(ab->checks);	/* leaves head of list */
}


/*
 * It has been noticed that this stream is dead and it is about to
 * be removed from the stream pool. We may have a pointer to it for one
 * of the remote address books. We have to note that the pointer is stale.
 */
void
note_closed_adrbk_stream(MAILSTREAM *stream)
{
    PerAddrBook *pab;
    int i;

    if(!stream)
      return;

    for(i = 0; i < as.n_addrbk; i++){
	pab = &as.adrbks[i];
	if(pab->address_book
	   && pab->address_book->type == Imap
	   && pab->address_book->rd
	   && pab->address_book->rd->type == RemImap
	   && (stream == pab->address_book->rd->t.i.stream)){
	    dprint((4, "- note_closed_adrbk_stream(%s) -\n",
		   (pab->address_book && pab->address_book->orig_filename)
		       ? pab->address_book->orig_filename : ""));
	    pab->address_book->rd->t.i.stream = NULL;
	}
    }
}


static adrbk_cntr_t tot_for_percent;
static adrbk_cntr_t entry_num_for_percent;

/*
 * Write out the address book.
 *
 * If be_quiet is set, don't turn on busy_cue.
 *
 * If enable_intr_handling is set, turn on and off interrupt handling.
 *
 * Format is as in comment in the adrbk_open routine.  Lines are wrapped
 * to be under 80 characters.  This is called on every change to the
 * address book.  Write is first to a temporary file,
 * which is then renamed to be the real address book so that we won't
 * destroy the real address book in case of something like a full file
 * system.
 *
 * Writing a temp file and then renaming has the bad side affect of
 * destroying links.  It also overrides any read only permissions on
 * the mail file since rename ignores such permissions.  However, we
 * handle readonly-ness in addrbook.c before we call this.
 * We retain the permissions by doing a stat on the old file and a
 * chmod on the new one (with file_attrib_copy).
 *
 * In pre-alpine pine the address book entries were encoded on disk.
 * We would be happy with raw UTF-8 now but in order to preserve some
 * backwards compatibility we encode the entries before writing.
 * We first try to translate to the user's character set and encode
 * in that, else we use UTF-8 but with 1522 encoding.
 *
 * Returns:   0 write was successful
 *           -2 write failed
 *           -5 interrupted
 */
int
adrbk_write(AdrBk *ab, a_c_arg_t current_entry_num, adrbk_cntr_t *new_entry_num,
	    int *sort_happened, int enable_intr_handling, int be_quiet)
{
    FILE                  *ab_stream = NULL;
    AdrBk_Entry           *ae = NULL;
    adrbk_cntr_t           entry_num;
#ifndef	DOS
    void                  (*save_sighup)();
#endif
    int                   max_nick = 0,
			  max_full = 0, full_two = 0, full_three = 0,
			  max_fcc = 0, fcc_two = 0, fcc_three = 0,
			  max_addr = 0, addr_two = 0, addr_three = 0,
			  this_nick_width, this_full_width, this_addr_width,
			  this_fcc_width, fd, i;
    int                   interrupt_happened = 0, we_cancel = 0, we_turned_on = 0;
    char                 *temp_filename = NULL;
    WIDTH_INFO_S         *widths;

    if(!ab)
      return -2;

    dprint((2, "- adrbk_write(\"%s\") - writing %lu entries\n",
	ab->filename ? ab->filename : "", (unsigned long) ab->count));
    
    errno = 0;

    adrbk_check_local_validity(ab, 1L);
    /* verify that file has not been changed by something else */
    if(ab->flags & FILE_OUTOFDATE){
	/* It has changed! */
	q_status_message(SM_ORDER | SM_DING, 5, 15,
	    /* TRANSLATORS: The address book was changed by something else so alpine
	       is not making the change the user wanted to make to avoid damaging
	       the address book. */
	    _("Addrbook changed by another process, aborting our change to avoid damage..."));
	dprint((1, "adrbk_write: addrbook %s changed while we had it open, aborting write\n",
		ab->filename ? ab->filename : "?"));
	longjmp(addrbook_changed_unexpectedly, 1);
	/*NOTREACHED*/
    }

    /*
     * Verify that remote folder has not been
     * changed by something else (not if checked in last 5 seconds).
     *
     * The -5 is so we won't check every time on a series of quick writes.
     */
    rd_check_remvalid(ab->rd, -5L);
    if(ab->type == Imap){
	int  ro;

	/*
	 * We'll eventually need this open to write to remote, so see if
	 * we can open it now.
	 */
	rd_open_remote(ab->rd);

	/*
	 * Did someone else change the remote copy?
	 */
	if((ro=rd_remote_is_readonly(ab->rd)) || ab->rd->flags & REM_OUTOFDATE){
	    if(ro == 1){
		q_status_message(SM_ORDER | SM_DING, 5, 15,
			    _("Can't access remote addrbook, aborting change..."));
		dprint((1,
			"adrbk_write: Can't write to remote addrbook %s, aborting write: open failed\n",
			ab->rd->rn ? ab->rd->rn : "?"));
	    }
	    else if(ro == 2){
		if(!(ab->rd->flags & NO_META_UPDATE)){
		    unsigned long save_chk_nmsgs;

		    /*
		     * Should have some non-type-specific method of doing this.
		     */
		    switch(ab->rd->type){
		      case RemImap:
			save_chk_nmsgs = ab->rd->t.i.chk_nmsgs;
			ab->rd->t.i.chk_nmsgs = 0;/* cause it to be OUTOFDATE */
			rd_write_metadata(ab->rd, 0);
			ab->rd->t.i.chk_nmsgs = save_chk_nmsgs;
			break;

		      default:
			q_status_message(SM_ORDER | SM_DING, 3, 5,
					 "Adrbk_write: Type not supported");
			break;
		    }
		}

		q_status_message(SM_ORDER | SM_DING, 5, 15,
	     _("No write permission for remote addrbook, aborting change..."));
	    }
	    else{
		q_status_message(SM_ORDER | SM_DING, 5, 15,
	     _("Remote addrbook changed, aborting our change to avoid damage..."));
		dprint((1,
			"adrbk_write: remote addrbook %s changed while we had it open, aborting write\n",
			ab->orig_filename ? ab->orig_filename : "?"));
	    }

	    rd_close_remote(ab->rd);

	    longjmp(addrbook_changed_unexpectedly, 1);
	    /*NOTREACHED*/
	}
    }

#ifndef	DOS
    save_sighup = (void (*)())signal(SIGHUP, SIG_IGN);
#endif

    /*
     * If we want to be able to modify the address book, we will
     * need a temp_filename in the same directory as our_filecopy.
     */
    if(!(ab->our_filecopy && ab->our_filecopy[0])
       || !(temp_filename = tempfile_in_same_dir(ab->our_filecopy,"a1",NULL))
       || (fd = our_open(temp_filename, OPEN_WRITE_MODE, 0600)) < 0){
	dprint((1, "adrbk_write(%s): failed opening temp file (%s)\n",
		ab->filename ? ab->filename : "?",
		temp_filename ? temp_filename : "NULL"));
        goto io_error;
    }

    ab_stream = fdopen(fd, "wb");
    if(ab_stream == NULL){
	dprint((1, "adrbk_write(%s): fdopen failed\n", temp_filename ? temp_filename : "?"));
        goto io_error;
    }

    if(adrbk_is_in_sort_order(ab, be_quiet)){
	if(sort_happened)
	  *sort_happened = 0;

	if(new_entry_num)
	  *new_entry_num = current_entry_num;
    }
    else{
	if(sort_happened)
	  *sort_happened = 1;

	(void) adrbk_sort(ab, current_entry_num, new_entry_num, be_quiet);
    }

    /* accept keyboard interrupts */
    if(enable_intr_handling)
      we_turned_on = intr_handling_on();

    if(!be_quiet){
	tot_for_percent = MAX(ab->count, 1);
	entry_num_for_percent = 0;
	we_cancel = busy_cue(_("Saving address book"), percent_abook_saved, 0);
    }

    writing = 1;

    /*
     * If there are any old deleted entries, copy them to new file.
     */
    if(ab->del_count > 0){
	char *nickname;
	long  delindex;

	/*
	 * If there are deleted entries old enough that we no longer want
	 * to save them, remove them from the list.
	 */
	for(delindex = (long) ab->del_count-1;
	    delindex >= 0L && ab->del_count > 0; delindex--){

	    ae = adrbk_get_delae(ab, (a_c_arg_t) delindex);
	    nickname = ae->nickname;

	    if(strncmp(nickname, DELETED, DELETED_LEN) == 0
	       && isdigit((unsigned char)nickname[DELETED_LEN])
	       && isdigit((unsigned char)nickname[DELETED_LEN+1])
	       && nickname[DELETED_LEN+2] == '/'
	       && isdigit((unsigned char)nickname[DELETED_LEN+3])
	       && isdigit((unsigned char)nickname[DELETED_LEN+4])
	       && nickname[DELETED_LEN+5] == '/'
	       && isdigit((unsigned char)nickname[DELETED_LEN+6])
	       && isdigit((unsigned char)nickname[DELETED_LEN+7])
	       && nickname[DELETED_LEN+8] == '#'){
		int year, month, day;
		struct tm *tm_before;
		time_t     now, before;

		now = time((time_t *)0);
		before = now - (time_t)ABOOK_DELETED_EXPIRE_TIME;
		tm_before = localtime(&before);
		tm_before->tm_mon++;

		/*
		 * Check to see if it is older than 100 days.
		 */
		year  = atoi(&nickname[DELETED_LEN]);
		month = atoi(&nickname[DELETED_LEN+3]);
		day   = atoi(&nickname[DELETED_LEN+6]);
		if(year < 95)
		  year += 100;  /* this breaks in year 2095 */
		
		/*
		 * remove it if it is more than 100 days old
		 */
		if(!(year > tm_before->tm_year
		     || (year == tm_before->tm_year
		         && month > tm_before->tm_mon)
		     || (year == tm_before->tm_year
		         && month == tm_before->tm_mon
		         && day >= tm_before->tm_mday))){

		    /* it's old, dump it */
		    free_ae_parts(ae);
		    ab->del_count--;
		    /* patch the array by moving everything below up */
		    if(delindex < ab->del_count){
			memmove(&ab->del[delindex],
			        &ab->del[delindex+1],
				(ab->del_count - delindex) *
							sizeof(AdrBk_Entry));
		    }
		}
	    }
	}

	/* write out what remains */
	if(ab->del_count){
	    for(delindex = 0L; delindex < ab->del_count; delindex++){
		ae = adrbk_get_delae(ab, (a_c_arg_t) delindex);
		if(write_single_abook_entry(ae, ab_stream, NULL, NULL,
					    NULL, NULL) == EOF){
		    dprint((1, "adrbk_write(%s): failed writing deleted entry\n",
			    temp_filename ? temp_filename : "?"));
		    goto io_error;
		}
	    }
	}
	else{
	    /* nothing left, get rid of del array */
	    fs_give((void **) &ab->del);
	}
    }

    if(ab->del_count)
      dprint((4, "  adrbk_write: saving %ld deleted entries\n",
			     (long) ab->del_count));

    for(entry_num = 0; entry_num < ab->count; entry_num++){
	entry_num_for_percent++;

	ALARM_BLIP();

	ae = adrbk_get_ae(ab, (a_c_arg_t) entry_num);

	if(ae == (AdrBk_Entry *) NULL){
	    dprint((1, "adrbk_write(%s): can't find ae while writing addrbook, entry_num = %ld\n",
		    ab->filename ? ab->filename : "?",
		    (long) entry_num));
	    goto io_error;
	}

	/* write to temp file */
	if(write_single_abook_entry(ae, ab_stream, &this_nick_width,
		&this_full_width, &this_addr_width, &this_fcc_width) == EOF){
	    dprint((1, "adrbk_write(%s): failed writing for entry %ld\n",
		    temp_filename ? temp_filename : "?",
		    (long) entry_num));
	    goto io_error;
	}

	/* keep track of widths */
	max_nick = MAX(max_nick, this_nick_width);
	if(this_full_width > max_full){
	    full_three = full_two;
	    full_two   = max_full;
	    max_full   = this_full_width;
	}
	else if(this_full_width > full_two){
	    full_three = full_two;
	    full_two   = this_full_width;
	}
	else if(this_full_width > full_three){
	    full_three = this_full_width;
	}

	if(this_addr_width > max_addr){
	    addr_three = addr_two;
	    addr_two   = max_addr;
	    max_addr   = this_addr_width;
	}
	else if(this_addr_width > addr_two){
	    addr_three = addr_two;
	    addr_two   = this_addr_width;
	}
	else if(this_addr_width > addr_three){
	    addr_three = this_addr_width;
	}

	if(this_fcc_width > max_fcc){
	    fcc_three = fcc_two;
	    fcc_two   = max_fcc;
	    max_fcc   = this_fcc_width;
	}
	else if(this_fcc_width > fcc_two){
	    fcc_three = fcc_two;
	    fcc_two   = this_fcc_width;
	}
	else if(this_fcc_width > fcc_three){
	    fcc_three = this_fcc_width;
	}

	/*
	 * Check to see if we've been interrupted.  We check at the bottom
	 * of the loop so that we can handle an interrupt in the last
	 * iteration.
	 */
	if(enable_intr_handling && ps_global->intr_pending){
	    interrupt_happened++;
	    goto io_error;
	}
    }

    if(we_cancel){
	cancel_busy_cue(-1);
	we_cancel = 0;
    }

    if(enable_intr_handling && we_turned_on){
	intr_handling_off();
	we_turned_on = 0;
    }

    if(fclose(ab_stream) == EOF){
	dprint((1, "adrbk_write: fclose for %s failed\n",
	       temp_filename ? temp_filename : "?"));
	goto io_error;
    }

    ab_stream = (FILE *) NULL;

    file_attrib_copy(temp_filename, ab->our_filecopy);
    if(ab->fp){
	(void) fclose(ab->fp);
	ab->fp = NULL;			/* in case of problems */
    }

    if((i=rename_file(temp_filename, ab->our_filecopy)) < 0){
	dprint((1, "adrbk_write: rename(%s, %s) failed: %s\n",
		   temp_filename ? temp_filename : "?",
		   ab->our_filecopy ? ab->our_filecopy : "?",
		   error_description(errno)));
#ifdef	_WINDOWS
	if(i == -5){
	    q_status_message2(SM_ORDER | SM_DING, 5, 7,
			  _("Can't replace address book %.200sfile \"%.200s\""),
			      (ab->type == Imap) ? "cache " : "",
			      ab->filename);
	    q_status_message(SM_ORDER | SM_DING, 5, 7,
    _("If another Alpine is running, quit that Alpine before updating address book."));
	}
#endif	/* _WINDOWS */
	goto io_error;
    }

    /* reopen fp to new file */
    if(!(ab->fp = our_fopen(ab->our_filecopy, "rb"))){
	dprint((1, "adrbk_write: can't reopen %s\n",
	       ab->our_filecopy ? ab->our_filecopy : "?"));
	goto io_error;
    }

    if(temp_filename){
	our_unlink(temp_filename);
	fs_give((void **) &temp_filename);
    }

    /*
     * Now copy our_filecopy back to filename.
     */
    if(ab->filename != ab->our_filecopy){
	int   c, err = 0;
	char *lc;

	if(!(ab->filename && ab->filename[0])
	   || !(temp_filename = tempfile_in_same_dir(ab->filename,"a1",NULL))
	   || (fd = our_open(temp_filename, OPEN_WRITE_MODE, 0600)) < 0){
	    dprint((1,
		    "adrbk_write(%s): failed opening temp file (%s)\n",
		    ab->filename ? ab->filename : "?",
		    temp_filename ? temp_filename : "NULL"));
	    err++;
	}

	if(!err)
	  ab_stream = fdopen(fd, "wb");
	  
	if(ab_stream == NULL){
	    dprint((1, "adrbk_write(%s): fdopen failed\n",
		    temp_filename ? temp_filename : "?"));
	    err++;
	}

	if(!err){
	    rewind(ab->fp);
	    while((c = getc(ab->fp)) != EOF)
	      if(putc(c, ab_stream) == EOF){
		  err++;
		  break;
	      }
	}

	if(!err && fclose(ab_stream) == EOF)
	  err++;

	if(!err){
#ifdef	_WINDOWS
	    int tries = 3;
#endif

	    file_attrib_copy(temp_filename, ab->filename);

	    /*
	     * This could fail if somebody else has it open, but they should
	     * only have it open for a short time.
	     */
	    while(!err && rename_file(temp_filename, ab->filename) < 0){
#ifdef	_WINDOWS
		if(i == -5){
		    if(--tries <= 0)
		      err++;

		    if(!err){
			q_status_message2(SM_ORDER, 0, 3,
			      _("Replace of \"%.200s\" failed, trying %.200s"),
			      (lc=last_cmpnt(ab->filename)) ? lc : ab->filename,
			      (tries > 1) ? "again" : "one more time");
			display_message('x');
			sleep(3);
		    }
		}
#else	/* UNIX */
		err++;
#endif	/* UNIX */
	    }
	}

	if(err){
	    q_status_message1(SM_ORDER | SM_DING, 5, 5,
	       _("Copy of addrbook to \"%.200s\" failed, changes NOT saved!"),
	       (lc=last_cmpnt(ab->filename)) ? lc : ab->filename);
	    dprint((2, "adrbk_write: failed copying our_filecopy (%s)back to filename (%s): %s, continuing without a net\n",
		   ab->our_filecopy ? ab->our_filecopy : "?",
		   ab->filename ? ab->filename : "?",
		   error_description(errno)));
	}
    }

    widths = &ab->widths;
    widths->max_nickname_width  = MIN(max_nick, 99);
    widths->max_fullname_width  = MIN(max_full, 99);
    widths->max_addrfield_width = MIN(max_addr, 99);
    widths->max_fccfield_width  = MIN(max_fcc, 99);
    widths->third_biggest_fullname_width  = MIN(full_three, 99);
    widths->third_biggest_addrfield_width = MIN(addr_three, 99);
    widths->third_biggest_fccfield_width  = MIN(fcc_three, 99);

    /* record new change date of addrbook file */
    ab->last_change_we_know_about = get_adj_name_file_mtime(ab->filename);
    
#ifndef	DOS
    (void)signal(SIGHUP, save_sighup);
#endif

    /*
     * If this is a remote addressbook, copy the file over.
     * If it fails we warn but continue to operate on the changed,
     * locally cached addressbook file.
     */
    if(ab->type == Imap){
	int   e;
	char datebuf[200];

	datebuf[0] = '\0';
	if(we_cancel)
	  cancel_busy_cue(-1);

	we_cancel = busy_cue(_("Copying to remote addressbook"), NULL, 1);
	/*
	 * We don't want a cookie upgrade to blast our data in rd->lf by
	 * copying back the remote data before doing the upgrade, and then
	 * proceeding to finish with that data. So we tell rd_upgrade_cookie
	 * about that here.
	 */
	ab->rd->flags |= BELIEVE_CACHE;
	if((e = rd_update_remote(ab->rd, datebuf)) != 0){
	    if(e == -1){
		q_status_message2(SM_ORDER | SM_DING, 3, 5,
			_("Error opening temporary addrbook file %.200s: %.200s"),
				ab->rd->lf, error_description(errno));
		dprint((1,
		       "adrbk_write: error opening temp file %s\n",
		       ab->rd->lf ? ab->rd->lf : "?"));
	    }
	    else{
		q_status_message2(SM_ORDER | SM_DING, 3, 5,
				_("Error copying to %.200s: %.200s"),
				ab->rd->rn, error_description(errno));
		dprint((1,
		       "adrbk_write: error copying from %s to %s\n",
		       ab->rd->lf ? ab->rd->lf : "?",
		       ab->rd->rn ? ab->rd->rn : "?"));
	    }
	    
	    q_status_message(SM_ORDER | SM_DING, 5, 5,
       _("Copy of addrbook to remote folder failed, changes NOT saved remotely"));
	}
	else{
	    rd_update_metadata(ab->rd, datebuf);
	    ab->rd->read_status = 'W';
	    dprint((7,
		   "%s: copied local to remote in adrbk_write (%ld)\n",
		   ab->rd->rn ? ab->rd->rn : "?",
		   (long)ab->rd->last_use));
	}

	ab->rd->flags &= ~BELIEVE_CACHE;
    }

    writing = 0;

    if(we_cancel)
      cancel_busy_cue(0);

    if(temp_filename){
	our_unlink(temp_filename);
	fs_give((void **) &temp_filename);
    }

    return 0;


io_error:
    if(we_cancel)
      cancel_busy_cue(-1);

    if(enable_intr_handling && we_turned_on)
      intr_handling_off();

#ifndef	DOS
    (void)signal(SIGHUP, save_sighup);
#endif
    if(interrupt_happened){
	q_status_message(0, 1, 2, _("Interrupt!  Reverting to previous version"));
	display_message('x');
	dprint((1, "adrbk_write(%s): Interrupt\n",
		ab->filename ? ab->filename : "?"));
    }
    else
      dprint((1, "adrbk_write(%s) (%s): some sort of io_error\n",
	      ab->filename ? ab->filename : "?",
	      ab->our_filecopy ? ab->our_filecopy : "?"));

    writing = 0;

    if(ab_stream){
	fclose(ab_stream);
	ab_stream = (FILE *)NULL;
    }

    if(temp_filename){
	our_unlink(temp_filename);
	fs_give((void **) &temp_filename);
    }

    adrbk_partial_close(ab);

    if(interrupt_happened){
	errno = EINTR;  /* for nicer error message */
	return -5;
    }
    else
      return -2;
}


/*
 * Writes one addrbook entry with wrapping.  Fills in widths
 * for display purposes. Returns 0, or EOF on error.
 *
 * Continuation lines always start with spaces.  Tabs are treated as
 * separators, never as whitespace.  When we output tab separators we
 * always put them on the ends of lines, never on the start of a line
 * after a continuation.  That is, there is always something printable
 * after continuation spaces.
 */
int
write_single_abook_entry(AdrBk_Entry *ae, FILE *fp, int *ret_nick_width,
			 int *ret_full_width, int *ret_addr_width, int *ret_fcc_width)
{
    int len = 0;
    int nick_width = 0, full_width = 0, addr_width = 0, fcc_width = 0;
    int tmplen, this_len;
    char *write_this = NULL;

    if(fp == (FILE *) NULL){
	dprint((1, "write_single_abook_entry: fp is NULL\n"));
	return(EOF);
    }

    if(ae->nickname){
	nick_width = utf8_width(ae->nickname);

	write_this = backcompat_encoding_for_abook(tmp_20k_buf, 10000,
			    tmp_20k_buf+10000, SIZEOF_20KBUF-10000, ae->nickname);

	this_len = strlen(write_this ? write_this : "");

	/*
	 * We aren't too concerned with where it wraps.
	 * Long lines are ok as long as they aren't super long.
	 */
	if(len > 100 || (len+this_len > 150 && len > 20)){
	    if(fprintf(fp, "%s%s", NEWLINE, INDENTSTR) == EOF){
		dprint((1,
		    "write_single_abook_entry: fputs ind1 failed\n"));
		return(EOF);
	    }

	    len = this_len + INDENT;
	}

	len += this_len;

	if(fputs(write_this, fp) == EOF){
	    dprint((1,
		"write_single_abook_entry: fputs nick failed\n"));
	    return(EOF);
	}
    }
    else
      nick_width = 0;

    putc(TAB, fp);
    len++;

    if(ae->fullname){
	full_width = utf8_width(ae->fullname);

	write_this = backcompat_encoding_for_abook(tmp_20k_buf, 10000,
			    tmp_20k_buf+10000, SIZEOF_20KBUF-10000, ae->fullname);

	this_len = strlen(write_this ? write_this : "");


	if(len > 100 || (len+this_len > 150 && len > 20)){
	    if(fprintf(fp, "%s%s", NEWLINE, INDENTSTR) == EOF){
		dprint((1,
		    "write_single_abook_entry: fputs ind2 failed\n"));
		return(EOF);
	    }

	    len = this_len + INDENT;
	}

	len += this_len;

	if(fputs(write_this, fp) == EOF){
	    dprint((1,
		"write_single_abook_entry: fputs full failed\n"));
	    return(EOF);
	}
    }
    else
      full_width = 0;

    putc(TAB, fp);
    len++;

    /* special case, make sure empty list has () */
    if(ae->tag == List && ae->addr.list == NULL){
	addr_width = 0;
	putc('(', fp);
	putc(')', fp);
	len += 2;
    }
    else if(ae->addr.addr != NULL || ae->addr.list != NULL){
	if(ae->tag == Single){
	    /*----- Single: just one address ----*/
	    if(ae->addr.addr){
		addr_width = utf8_width(ae->addr.addr);

		write_this = backcompat_encoding_for_abook(tmp_20k_buf, 10000,
				    tmp_20k_buf+10000, SIZEOF_20KBUF-10000, ae->addr.addr);

		this_len = strlen(write_this ? write_this : "");

		if(len > 100 || (len+this_len > 150 && len > 20)){
		    if(fprintf(fp, "%s%s", NEWLINE, INDENTSTR) == EOF){
			dprint((1,
			    "write_single_abook_entry: fputs ind3 failed\n"));
			return(EOF);
		    }

		    len = this_len + INDENT;
		}

		len += this_len;

		if(fputs(write_this, fp) == EOF){
		    dprint((1,
			"write_single_abook_entry: fputs addr failed\n"));
		    return(EOF);
		}
	    }
	    else
	      addr_width = 0;
	}
	else if(ae->tag == List){
	    register char **a2;

	    /*----- List: a distribution list ------*/
	    putc('(', fp);
	    len++;
	    addr_width = 0;
	    for(a2 = ae->addr.list; *a2 != NULL; a2++){
		if(*a2){
		    if(a2 != ae->addr.list){
			putc(',', fp);
			len++;
		    }

		    addr_width = MAX(addr_width, utf8_width(*a2));

		    write_this = backcompat_encoding_for_abook(tmp_20k_buf, 10000,
					tmp_20k_buf+10000, SIZEOF_20KBUF-10000, *a2);

		    this_len = strlen(write_this ? write_this : "");

		    if(len > 100 || (len+this_len > 150 && len > 20)){
			if(fprintf(fp, "%s%s", NEWLINE, INDENTSTR) == EOF){
			    dprint((1,
				"write_single_abook_entry: fputs ind3 failed\n"));
			    return(EOF);
			}

			len = this_len + INDENT;
		    }

		    len += this_len;

		    if(fputs(write_this, fp) == EOF){
			dprint((1,
			    "write_single_abook_entry: fputs addrl failed\n"));
			return(EOF);
		    }
		}
	    }

	    putc(')', fp);
	    len++;
	}
    }

    /* If either fcc or extra exists, output both, otherwise, neither */
    if((ae->fcc && ae->fcc[0]) || (ae->extra && ae->extra[0])){
	putc(TAB, fp);
	len++;

	if(ae->fcc && ae->fcc[0]){
	    fcc_width = utf8_width(ae->fcc);

	    write_this = backcompat_encoding_for_abook(tmp_20k_buf, 10000,
				tmp_20k_buf+10000, SIZEOF_20KBUF-10000, ae->fcc);

	    this_len = strlen(write_this ? write_this : "");

	    if(len > 100 || (len+this_len > 150 && len > 20)){
		if(fprintf(fp, "%s%s", NEWLINE, INDENTSTR) == EOF){
		    dprint((1,
			"write_single_abook_entry: fputs ind4 failed\n"));
		    return(EOF);
		}

		len = this_len + INDENT;
	    }

	    len += this_len;

	    if(fputs(write_this, fp) == EOF){
		dprint((1,
		    "write_single_abook_entry: fputs fcc failed\n"));
		return(EOF);
	    }
	}
	else
	  fcc_width = 0;

	putc(TAB, fp);
	len++;

	if(ae->extra && ae->extra[0]){
	    int space;
	    char *cur, *end;
	    char *extra_copy;

	    write_this = backcompat_encoding_for_abook(tmp_20k_buf, 10000,
				tmp_20k_buf+10000, SIZEOF_20KBUF-10000, ae->extra);

	    /*
	     * Copy ae->extra and replace newlines with spaces.
	     * The reason we do this is because the continuation lines
	     * produced by rfc1522_encode may interact with the
	     * continuation lines produced below in a bad way.
	     * In particular, what can happen is that the 1522 continuation
	     * newline can be followed immediately by a newline produced
	     * below. That breaks the continuation since that is a
	     * line with nothing on it. Just turn 1522 continuations into
	     * spaces, which work fine with 1522_decode.
	     */
	    extra_copy = cpystr(write_this);
	    REPLACE_NEWLINES_WITH_SPACE(extra_copy);
	   
	    tmplen = strlen(extra_copy);

	    if(len > 100 || (len+tmplen > 150 && len > 20)){
		if(fprintf(fp, "%s%s", NEWLINE, INDENTSTR) == EOF){
		    dprint((1,
			"write_single_abook_entry: fprintf indent failed\n"));
		    return(EOF);
		}

		len = INDENT;
	    }

	    space = MAX(70 - len, 5);
	    cur = extra_copy;
	    end = cur + tmplen;
	    while(cur < end){
		if(end-cur > space){
		    int i;

		    /* find first space after spot we want to break */
		    for(i = space; cur+i < end && cur[i] != SPACE; i++) 
		      ;
		    
		    cur[i] = '\0';
		    if(fputs(cur, fp) == EOF){
			dprint((1,
			    "write_single_abook_entry: fputs extra failed\n"));
			return(EOF);
		    }

		    cur += (i+1);

		    if(cur < end){
			if(fprintf(fp, "%s%s", NEWLINE, INDENTXTRA) == EOF){
			    dprint((1,
			"write_single_abook_entry: fprintf indent failed\n"));
			    return(EOF);
			}

			space = 70 - INDENT;
		    }
		}
		else{
		    if(fputs(cur, fp) == EOF){
			dprint((1,
			    "write_single_abook_entry: fputs extra failed\n"));
			return(EOF);
		    }

		    cur = end;
		}
	    }

	    if(extra_copy)
	      fs_give((void **)&extra_copy);
	}
    }
    else
      fcc_width = 0;

    fprintf(fp, "%s", NEWLINE);
    
    if(ret_nick_width)
      *ret_nick_width = nick_width;
    if(ret_full_width)
      *ret_full_width = full_width;
    if(ret_addr_width)
      *ret_addr_width = addr_width;
    if(ret_fcc_width)
      *ret_fcc_width = fcc_width;

    return(0);
}


char *
backcompat_encoding_for_abook(char *buf1, size_t buf1len, char *buf2,
			      size_t buf2len, char *srcstr)
{
    char *encoded = NULL;
    char *p;
    int its_ascii = 1;


    for(p = srcstr; *p && its_ascii; p++)
      if(*p & 0x80)
	its_ascii = 0;

    /* if it is ascii, go with that */
    if(its_ascii)
      encoded = srcstr;
    else{
	char *trythischarset = NULL;

	/*
	 * If it is possible to translate the UTF-8
	 * string into the user's character set then
	 * do that. For backwards compatibility with
	 * old pines.
	 */
	if(ps_global->keyboard_charmap && ps_global->keyboard_charmap[0])
	  trythischarset = ps_global->keyboard_charmap;
	else if(ps_global->display_charmap && ps_global->display_charmap[0])
	  trythischarset = ps_global->display_charmap;

	if(trythischarset){
	    SIZEDTEXT src, dst;

	    src.data = (unsigned char *) srcstr;
	    src.size = strlen(srcstr);
	    memset(&dst, 0, sizeof(dst));
	    if(utf8_cstext(&src, trythischarset, &dst, 0)){
		if(dst.data){
		    strncpy(buf1, (char *) dst.data, buf1len);
		    buf1[buf1len-1] = '\0';
		    fs_give((void **) &dst.data);
		    encoded = rfc1522_encode(buf2, buf2len,
					     (unsigned char *) buf1, trythischarset);
		    if(encoded)
		      REPLACE_NEWLINES_WITH_SPACE(encoded);
		}
	    }
	}

	if(!encoded){
	    encoded = rfc1522_encode(buf1, buf1len, (unsigned char *) srcstr, "UTF-8");
	    if(encoded)
	      REPLACE_NEWLINES_WITH_SPACE(encoded);
	}
    }

    return(encoded);
}


int
percent_abook_saved(void)
{
    return((int)(((unsigned long)entry_num_for_percent * (unsigned long)100) /
	(unsigned long)tot_for_percent));
}


/*
 * Free memory associated with entry ae.
 *
 * Args:  ae  -- Address book entry to be freed.
 */
void
free_ae(AdrBk_Entry **ae)
{
    if(!ae || !(*ae))
      return;

    free_ae_parts(*ae);
    fs_give((void **) ae);
}


/*
 * Free memory associated with entry ae but not ae itself.
 */
void
free_ae_parts(AdrBk_Entry *ae)
{
    char **p;

    if(!ae)
      return;

    if(ae->nickname && ae->nickname != empty)
      fs_give((void **) &ae->nickname);

    if(ae->fullname && ae->fullname != empty)
      fs_give((void **) &ae->fullname);

    if(ae->tag == Single){
        if(ae->addr.addr && ae->addr.addr != empty)
          fs_give((void **) &ae->addr.addr);
    }
    else if(ae->tag == List){
        if(ae->addr.list){
            for(p = ae->addr.list; *p; p++) 
              if(*p != empty)
	        fs_give((void **) p);

            fs_give((void **) &ae->addr.list);
        }
    }

    if(ae->fcc && ae->fcc != empty)
      fs_give((void **) &ae->fcc);

    if(ae->extra && ae->extra != empty)
      fs_give((void **) &ae->extra);

    defvalue_ae(ae);
}


/*
 * Inserts element new_ae before element put_it_before_this.
 */
void
insert_ab_entry(AdrBk *ab, a_c_arg_t put_it_before_this, AdrBk_Entry *new_ae,
		int use_deleted_list)
{
    adrbk_cntr_t before, old_count, new_count;
    AdrBk_Entry *ae_before, *ae_before_next;

    dprint((7, "- insert_ab_entry(before_this=%ld) -\n", (long) put_it_before_this));

    before = (adrbk_cntr_t) put_it_before_this;

    if(!ab || before == NO_NEXT || (!use_deleted_list && before > ab->count)
       || (use_deleted_list && before > ab->del_count)){
	;
    }
    else{
	/*
	 * add space for new entry to array
	 * slide entries [before ... old_count-1] down one
	 * put new_ae into opened up slot
	 */
	old_count = use_deleted_list ? ab->del_count : ab->count;
	new_count = old_count + 1;
	if(old_count == 0){
	    if(use_deleted_list){
		if(ab->del)		/* shouldn't happen */
		  fs_give((void **) &ab->del);

		/* first entry in new array */
		ab->del = (AdrBk_Entry *) fs_get(new_count * sizeof(AdrBk_Entry));
		defvalue_ae(ab->del);
	    }
	    else{
		if(ab->arr)			/* shouldn't happen */
		  fs_give((void **) &ab->arr);

		/* first entry in new array */
		ab->arr = (AdrBk_Entry *) fs_get(new_count * sizeof(AdrBk_Entry));
		defvalue_ae(ab->arr);
	    }
	}
	else{
	    if(use_deleted_list){
		fs_resize((void **) &ab->del, new_count * sizeof(AdrBk_Entry));
		defvalue_ae(&ab->del[new_count-1]);
	    }
	    else{
		fs_resize((void **) &ab->arr, new_count * sizeof(AdrBk_Entry));
		defvalue_ae(&ab->arr[new_count-1]);
	    }
	}

	if(use_deleted_list){
	    ab->del_count = new_count;

	    ae_before = adrbk_get_delae(ab, (a_c_arg_t) before);
	    if(ae_before){
		if(before < old_count){
		    ae_before_next = adrbk_get_delae(ab, (a_c_arg_t) (before+1));
		    memmove(ae_before_next, ae_before,
			    (old_count-before) * sizeof(AdrBk_Entry));
		}

		memcpy(ae_before, new_ae, sizeof(AdrBk_Entry));
	    }
	}
	else{
	    ab->count = new_count;

	    ae_before = adrbk_get_ae(ab, (a_c_arg_t) before);
	    if(ae_before){
		if(before < old_count){
		    ae_before_next = adrbk_get_ae(ab, (a_c_arg_t) (before+1));
		    memmove(ae_before_next, ae_before,
			    (old_count-before) * sizeof(AdrBk_Entry));
		}

		memcpy(ae_before, new_ae, sizeof(AdrBk_Entry));
	    }
	}

    }

    if(!use_deleted_list)
      repair_abook_tries(ab);
}


/*
 * Moves element move_this_one before element put_it_before_this.
 */
void
move_ab_entry(AdrBk *ab, a_c_arg_t move_this_one, a_c_arg_t put_it_before_this)
{
    adrbk_cntr_t m, before;
    AdrBk_Entry  ae_tmp;
    AdrBk_Entry *ae_m, *ae_m_next, *ae_before, *ae_before_prev, *ae_before_next;

    dprint((7, "- move_ab_entry(move_this=%ld,before_this=%ld) -\n", (long) move_this_one, (long) put_it_before_this));

    m      = (adrbk_cntr_t) move_this_one;
    before = (adrbk_cntr_t) put_it_before_this;

    if(!ab || m == NO_NEXT || before == NO_NEXT
       || m == before || m+1 == before || before > ab->count){
	;
    }
    else if(m+1 < before){
	/*
	 * copy m
	 * slide entries [m+1 ... before-1] up one ("up" means smaller indices)
	 * put m into opened up slot
	 */
	ae_m  = adrbk_get_ae(ab, (a_c_arg_t) m);
	ae_m_next  = adrbk_get_ae(ab, (a_c_arg_t) (m+1));
	ae_before_prev  = adrbk_get_ae(ab, (a_c_arg_t) (before-1));
	if(ae_m && ae_m_next && ae_before_prev){
	    memcpy(&ae_tmp, ae_m, sizeof(ae_tmp));
	    memmove(ae_m, ae_m_next, (before-m-1) * sizeof(ae_tmp));
	    memcpy(ae_before_prev, &ae_tmp, sizeof(ae_tmp));
	}
    }
    else if(m > before){
	/*
	 * copy m
	 * slide entries [before ... m-1] down one
	 * put m into opened up slot
	 */
	ae_m  = adrbk_get_ae(ab, (a_c_arg_t) m);
	ae_before = adrbk_get_ae(ab, (a_c_arg_t) before);
	ae_before_next = adrbk_get_ae(ab, (a_c_arg_t) (before+1));
	if(ae_m && ae_before && ae_before_next){
	    memcpy(&ae_tmp, ae_m, sizeof(ae_tmp));
	    memmove(ae_before_next, ae_before, (m-before) * sizeof(ae_tmp));
	    memcpy(ae_before, &ae_tmp, sizeof(ae_tmp));
	}
    }

    dprint((9, "- move_ab_entry: done -\n"));

    repair_abook_tries(ab);
}


/*
 * Deletes element delete_this_one from in-core data structure.
 * If save_it is set the deleted entry is moved to the deleted_list.
 */
void
delete_ab_entry(AdrBk *ab, a_c_arg_t delete_this_one, int save_it)
{
    adrbk_cntr_t d;
    AdrBk_Entry *ae_deleted, *ae_deleted_next;

    dprint((7, "- delete_ab_entry(delete_this=%ld,save_it=%d) -\n", (long) delete_this_one, save_it));

    d = (adrbk_cntr_t) delete_this_one;

    if(!ab || d == NO_NEXT || d >= ab->count){
	;
    }
    else{
	/*
	 * Move the entry to the deleted_list if asked to.
	 */
	ae_deleted  = adrbk_get_ae(ab, (a_c_arg_t) d);
	if(ae_deleted){
	  if(save_it){
	    char *oldnick, *newnick;
	    size_t len;
	    struct tm *tm_now;
	    time_t now;

	    /*
	     * First prepend the prefix
	     * #DELETED-YY/MM/DD#
	     * to the nickname.
	     */
	    now = time((time_t) 0);
	    tm_now = localtime(&now);

	    oldnick = ae_deleted->nickname;
	    len = strlen(oldnick) + DELETED_LEN + strlen("YY/MM/DD#");

	    newnick = (char *) fs_get((len+1) * sizeof(char));
	    snprintf(newnick, len+1, "%s%02d/%02d/%02d#%s",
		     DELETED, (tm_now->tm_year)%100, tm_now->tm_mon+1,
		     tm_now->tm_mday, oldnick ? oldnick : "");
	    newnick[len] = '\0';

	    if(ae_deleted->nickname && ae_deleted->nickname != empty)
	      fs_give((void **) &ae_deleted->nickname);

	    ae_deleted->nickname = newnick;

	    /*
	     * Now insert this entry in the deleted_list.
	     */
	    insert_ab_entry(ab, (a_c_arg_t) ab->del_count, ae_deleted, 1);
	  }
	  else
	    free_ae_parts(ae_deleted);
	}

	/*
	 * slide entries [deleted+1 ... count-1] up one
	 */
	if(d+1 < ab->count){
	    ae_deleted = adrbk_get_ae(ab, (a_c_arg_t) d);
	    ae_deleted_next = adrbk_get_ae(ab, (a_c_arg_t) (d+1));
	    if(ae_deleted && ae_deleted_next)
	      memmove(ae_deleted, ae_deleted_next, (ab->count-d-1) * sizeof(AdrBk_Entry));
	}

	ab->count--;

	if(ab->count > 0)
	  fs_resize((void **) &ab->arr, ab->count * sizeof(AdrBk_Entry));
	else
	  fs_give((void **) &ab->arr);
    }

    repair_abook_tries(ab);
}


/*
 * We may want to be smarter about this and repair instead of
 * rebuild these.
 */
void
repair_abook_tries(AdrBk *ab)
{
    if(ab->arr){
	AdrBk_Trie *save_nick_trie, *save_addr_trie,
		   *save_full_trie, *save_revfull_trie;

	save_nick_trie = ab->nick_trie;
	ab->nick_trie = NULL;
	save_addr_trie = ab->addr_trie;
	ab->addr_trie = NULL;
	save_full_trie = ab->full_trie;
	ab->full_trie = NULL;
	save_revfull_trie = ab->revfull_trie;
	ab->revfull_trie = NULL;
	if(build_abook_tries(ab, NULL)){
	    dprint((2, "trouble rebuilding tries, restoring\n"));
	    if(ab->nick_trie)
	      free_abook_trie(&ab->nick_trie);

	    /* better than nothing */
	    ab->nick_trie = save_nick_trie;

	    if(ab->addr_trie)
	      free_abook_trie(&ab->addr_trie);

	    ab->addr_trie = save_addr_trie;

	    if(ab->full_trie)
	      free_abook_trie(&ab->full_trie);

	    ab->full_trie = save_full_trie;

	    if(ab->revfull_trie)
	      free_abook_trie(&ab->revfull_trie);

	    ab->revfull_trie = save_revfull_trie;
	}
	else{
	    if(save_nick_trie)
	      free_abook_trie(&save_nick_trie);

	    if(save_addr_trie)
	      free_abook_trie(&save_addr_trie);

	    if(save_full_trie)
	      free_abook_trie(&save_full_trie);

	    if(save_revfull_trie)
	      free_abook_trie(&save_revfull_trie);
	}
    }
}


/*
 * Free the list of distribution lists which have been expanded.
 * Leaves the head of the list alone.
 *
 * Args:  exp_head -- Head of the expanded list.
 */
void
exp_free(EXPANDED_S *exp_head)
{
    EXPANDED_S *e, *the_next_one;

    e = exp_head ? exp_head->next : NULL;

    if(!e)
      return;

    while(e){
	the_next_one = e->next;
	fs_give((void **)&e);
	e = the_next_one;
    }

    exp_head->next = (EXPANDED_S *)NULL;
}


/*
 * Is entry n expanded?
 *
 * Args:  exp_head -- Head of the expanded list.
 *        n        -- The entry num to check
 */
int
exp_is_expanded(EXPANDED_S *exp_head, a_c_arg_t n)
{
    register EXPANDED_S *e;
    adrbk_cntr_t nn;

    nn = (adrbk_cntr_t)n;

    e = exp_head ? exp_head->next : NULL;

    /*
     * The list is kept ordered, so we search until we find it or are
     * past it.
     */
    while(e){
	if(e->ent >= nn)
	  break;
	
	e = e->next;
    }
    
    return(e && e->ent == nn);
}


/*
 * How many entries expanded in this addrbook.
 *
 * Args:  exp_head -- Head of the expanded list.
 */
int
exp_howmany_expanded(EXPANDED_S *exp_head)
{
    register EXPANDED_S *e;
    int                  cnt = 0;

    e = exp_head ? exp_head->next : NULL;

    while(e){
	cnt++;
	e = e->next;
    }
    
    return(cnt);
}


/*
 * Are any entries expanded?
 *
 * Args:  exp_head -- Head of the expanded list.
 */
int
exp_any_expanded(EXPANDED_S *exp_head)
{
    return(exp_head && exp_head->next != NULL);
}


/*
 * Return next entry num in list.
 *
 * Args:  cur -- Current position in the list.
 *
 * Result: Returns the number of the next entry, or NO_NEXT if there is
 *	   no next entry.  As a side effect, the cur pointer is incremented.
 */
adrbk_cntr_t
exp_get_next(EXPANDED_S **cur)
{
    adrbk_cntr_t ret = NO_NEXT;

    if(cur && *cur && (*cur)->next){
	ret  = (*cur)->next->ent;
	*cur = (*cur)->next;
    }

    return(ret);
}


/*
 * Mark entry n as being expanded.
 *
 * Args:  exp_head -- Head of the expanded list.
 *        n        -- The entry num to mark
 */
void
exp_set_expanded(EXPANDED_S *exp_head, a_c_arg_t n)
{
    register EXPANDED_S *e;
    EXPANDED_S *new;
    adrbk_cntr_t nn;

    nn = (adrbk_cntr_t)n;
    if(!exp_head)
      panic("exp_head not set in exp_set_expanded");

    for(e = exp_head; e->next; e = e->next)
      if(e->next->ent >= nn)
	break;
    
    if(e->next && e->next->ent == nn) /* already there */
      return;

    /* add new after e */
    new       = (EXPANDED_S *)fs_get(sizeof(EXPANDED_S));
    new->ent  = nn;
    new->next = e->next;
    e->next   = new;
}


/*
 * Mark entry n as being *not* expanded.
 *
 * Args:  exp_head -- Head of the expanded list.
 *        n        -- The entry num to mark
 */
void
exp_unset_expanded(EXPANDED_S *exp_head, a_c_arg_t n)
{
    register EXPANDED_S *e;
    EXPANDED_S *delete_this_one = NULL;
    adrbk_cntr_t nn;

    nn = (adrbk_cntr_t)n;
    if(!exp_head)
      panic("exp_head not set in exp_unset_expanded");

    for(e = exp_head; e->next; e = e->next)
      if(e->next->ent >= nn)
	break;
    
    if(e->next && e->next->ent == nn){
	delete_this_one = e->next;
	e->next = e->next->next;
    }

    if(delete_this_one)
      fs_give((void **)&delete_this_one);
}


/*
 * Adjust the "expanded" list to correspond to addrbook entry n being
 * deleted.
 *
 * Args:  exp_head -- Head of the expanded list.
 *        n        -- The entry num being deleted
 */
void
exp_del_nth(EXPANDED_S *exp_head, a_c_arg_t n)
{
    register EXPANDED_S *e;
    int delete_when_done = 0;
    adrbk_cntr_t nn;

    nn = (adrbk_cntr_t)n;
    if(!exp_head)
      panic("exp_head not set in exp_del_nth");

    e = exp_head->next;
    while(e && e->ent < nn)
      e = e->next;
    
    if(e){
	if(e->ent == nn){
	    delete_when_done++;
	    e = e->next;
	}

	while(e){
	    e->ent--; /* adjust entry nums */
	    e = e->next;
	}

	if(delete_when_done)
	  exp_unset_expanded(exp_head, n);
    }
}


/*
 * Adjust the "expanded" list to correspond to a new addrbook entry being
 * added between current entries n-1 and n.
 *
 * Args:  exp_head -- Head of the expanded list.
 *        n        -- The entry num being added
 *
 * The new entry is not marked expanded.
 */
void
exp_add_nth(EXPANDED_S *exp_head, a_c_arg_t n)
{
    register EXPANDED_S *e;
    adrbk_cntr_t nn;

    nn = (adrbk_cntr_t)n;
    if(!exp_head)
      panic("exp_head not set in exp_add_nth");

    e = exp_head->next;
    while(e && e->ent < nn)
      e = e->next;
    
    while(e){
	e->ent++; /* adjust entry nums */
	e = e->next;
    }
}


static AdrBk *ab_for_sort;

/*
 * Compare two address book entries.  Args are AdrBk_Entry **'s.
 * Sorts lists after simple addresses and then sorts on Fullname field.
 */
int
cmp_ae_by_full_lists_last(const qsort_t *a, const qsort_t *b)
{
    AdrBk_Entry **x = (AdrBk_Entry **)a,
                **y = (AdrBk_Entry **)b;
    int result;

    if((*x)->tag == List && (*y)->tag == Single)
      result = 1;
    else if((*x)->tag == Single && (*y)->tag == List)
      result = -1;
    else{
	register char *p, *q, *r, *s;

	p = (*x)->fullname;
	if(*p == '"' && *(p+1))
	  p++;

	q = (*y)->fullname;
	if(*q == '"' && *(q+1))
	  q++;

	r = (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf,
					   SIZEOF_20KBUF, p);
	
	s = (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf+10000,
					   SIZEOF_20KBUF-10000, q);

	result = (*pcollator)(r, s);
	if(result == 0)
	  result = (*pcollator)((*x)->nickname, (*y)->nickname);
    }
      
    return(result);
}


/*
 * Compare two address book entries.  Args are adrbk_cntr_t *'s (element #'s).
 * Sorts lists after simple addresses and then sorts on Fullname field.
 */
int
cmp_cntr_by_full_lists_last(const qsort_t *a, const qsort_t *b)
{
    adrbk_cntr_t *x = (adrbk_cntr_t *)a,  /* *x is an element_number */
                 *y = (adrbk_cntr_t *)b;
    AdrBk_Entry  *x_ae,
		 *y_ae;

    if(ps_global->intr_pending)
      longjmp(jump_over_qsort, 1);

    ALARM_BLIP();

    x_ae = adrbk_get_ae(ab_for_sort, (a_c_arg_t)(*x));
    y_ae = adrbk_get_ae(ab_for_sort, (a_c_arg_t)(*y));

    return(cmp_ae_by_full_lists_last((const qsort_t *) &x_ae,
				     (const qsort_t *) &y_ae));
}


/*
 * Compare two address book entries.  Args are AdrBk_Entry **'s.
 * Sorts on Fullname field.
 */
int
cmp_ae_by_full(const qsort_t *a, const qsort_t *b)
{
    AdrBk_Entry **x = (AdrBk_Entry **)a,
                **y = (AdrBk_Entry **)b;
    int result;
    register char *p, *q, *r, *s;

    p = (*x)->fullname;
    if(*p == '"' && *(p+1))
      p++;

    q = (*y)->fullname;
    if(*q == '"' && *(q+1))
      q++;

    r = (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf,
				       SIZEOF_20KBUF, p);
    s = (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf+10000,
				       SIZEOF_20KBUF-10000, q);
    result = (*pcollator)(r, s);
    if(result == 0)
      result = (*pcollator)((*x)->nickname, (*y)->nickname);
      
    return(result);
}


/*
 * Compare two address book entries.  Args are adrbk_cntr_t *'s (element #'s).
 * Sorts on Fullname field.
 */
int
cmp_cntr_by_full(const qsort_t *a, const qsort_t *b)
{
    adrbk_cntr_t *x = (adrbk_cntr_t *)a,  /* *x is an element_number */
                 *y = (adrbk_cntr_t *)b;
    AdrBk_Entry  *x_ae,
		 *y_ae;

    if(ps_global->intr_pending)
      longjmp(jump_over_qsort, 1);

    ALARM_BLIP();

    x_ae = adrbk_get_ae(ab_for_sort, (a_c_arg_t)(*x));
    y_ae = adrbk_get_ae(ab_for_sort, (a_c_arg_t)(*y));

    return(cmp_ae_by_full((const qsort_t *) &x_ae, (const qsort_t *) &y_ae));
}


/*
 * Compare two address book entries.  Args are AdrBk_Entry **'s.
 * Sorts lists after simple addresses and then sorts on Nickname field.
 */
int
cmp_ae_by_nick_lists_last(const qsort_t *a, const qsort_t *b)
{
    AdrBk_Entry **x = (AdrBk_Entry **)a,
                **y = (AdrBk_Entry **)b;
    int result;

    if((*x)->tag == List && (*y)->tag == Single)
      result = 1;
    else if((*x)->tag == Single && (*y)->tag == List)
      result = -1;
    else
      result = (*pcollator)((*x)->nickname, (*y)->nickname);

    return(result);
}


/*
 * Compare two address book entries.  Args are adrbk_cntr_t *'s (element #'s).
 * Sorts lists after simple addresses and then sorts on Nickname field.
 */
int
cmp_cntr_by_nick_lists_last(const qsort_t *a, const qsort_t *b)
{
    adrbk_cntr_t *x = (adrbk_cntr_t *)a,  /* *x is an element_number */
                 *y = (adrbk_cntr_t *)b;
    AdrBk_Entry  *x_ae,
		 *y_ae;

    if(ps_global->intr_pending)
      longjmp(jump_over_qsort, 1);

    ALARM_BLIP();

    x_ae = adrbk_get_ae(ab_for_sort, (a_c_arg_t)(*x));
    y_ae = adrbk_get_ae(ab_for_sort, (a_c_arg_t)(*y));

    return(cmp_ae_by_nick_lists_last((const qsort_t *) &x_ae,
				     (const qsort_t *) &y_ae));
}


/*
 * Compare two address book entries.  Args are AdrBk_Entry **'s.
 * Sorts on Nickname field.
 */
int
cmp_ae_by_nick(const qsort_t *a, const qsort_t *b)
{
    AdrBk_Entry **x = (AdrBk_Entry **)a,
                **y = (AdrBk_Entry **)b;

    return((*pcollator)((*x)->nickname, (*y)->nickname));
}


/*
 * Compare two address book entries.  Args are adrbk_cntr_t *'s (element #'s).
 * Sorts on Nickname field.
 */
int
cmp_cntr_by_nick(const qsort_t *a, const qsort_t *b)
{
    adrbk_cntr_t *x = (adrbk_cntr_t *)a,  /* *x is an element_number */
                 *y = (adrbk_cntr_t *)b;
    AdrBk_Entry  *x_ae,
		 *y_ae;

    if(ps_global->intr_pending)
      longjmp(jump_over_qsort, 1);

    ALARM_BLIP();

    x_ae = adrbk_get_ae(ab_for_sort, (a_c_arg_t)(*x));
    y_ae = adrbk_get_ae(ab_for_sort, (a_c_arg_t)(*y));

    return(cmp_ae_by_nick((const qsort_t *) &x_ae, (const qsort_t *) &y_ae));
}


/*
 * For sorting a simple list of pointers to addresses (skip initial quotes)
 */
int
cmp_addr(const qsort_t *a1, const qsort_t *a2)
{
    char *x = *(char **)a1, *y = *(char **)a2;
    char *r, *s;

    if(x && *x == '"')
      x++;

    if(y && *y == '"')
      y++;
    
    r = (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf,
				       SIZEOF_20KBUF, x);
    s = (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf+10000,
				       SIZEOF_20KBUF-10000, y);
    return((*pcollator)(r, s));
}


/*
 * Sort an array of strings, except skip initial quotes.
 */
void
sort_addr_list(char **list)
{
    register char **p;

    /* find size of list */
    for(p = list; *p != NULL; p++)
      ;/* do nothing */

    qsort((qsort_t *)list, (size_t)(p - list), sizeof(char *), cmp_addr);
}


/*
 * Sort this address book.
 *
 * Args: ab            -- address book to sort
 *   current_entry_num -- see next description
 *   new_entry_num     -- return new entry_num of current_entry_num here
 *
 * Result: return code:  0 all went well
 *                      -2 error writing address book, check errno
 *
 * The sorting strategy is to allocate an array of length ab->count which
 * contains the element numbers 0, 1, ..., ab->count - 1, representing the
 * entries in the addrbook, of course.  Sort the array, then use that
 * result to swap ae's in the in-core addrbook array before writing it out.
 */
int
adrbk_sort(AdrBk *ab, a_c_arg_t current_entry_num, adrbk_cntr_t *new_entry_num, int be_quiet)
{
    adrbk_cntr_t *sort_array, *inv, tmp;
    long i, j, hi, count;
    int skip_the_sort = 0, we_cancel = 0, we_turned_on = 0;
    AdrBk_Entry ae_tmp, *ae_i, *ae_hi;
    EXPANDED_S *e, *e2, *smallest;

    dprint((5, "- adrbk_sort -\n"));

    count = (long) (ab->count);

    if(!ab)
      return -2;

    if(ab->sort_rule == AB_SORT_RULE_NONE)
      return 0;
    
    if(count < 2)
      return 0;

    sort_array = (adrbk_cntr_t *) fs_get(count * sizeof(adrbk_cntr_t));
    inv        = (adrbk_cntr_t *) fs_get(count * sizeof(adrbk_cntr_t));
    
    for(i = 0L; i < count; i++)
      sort_array[i] = (adrbk_cntr_t) i;
	
    ab_for_sort = ab;

    if(setjmp(jump_over_qsort))
      skip_the_sort = 1;

    if(!skip_the_sort){
	we_turned_on = intr_handling_on();
	if(!be_quiet)
	  we_cancel = busy_cue(_("Sorting address book"), NULL, 0);

	qsort((qsort_t *)sort_array,
	    (size_t)count,
	    sizeof(adrbk_cntr_t),
	    (ab->sort_rule == AB_SORT_RULE_FULL_LISTS) ?
						cmp_cntr_by_full_lists_last :
	    (ab->sort_rule == AB_SORT_RULE_FULL) ?
						cmp_cntr_by_full :
	    (ab->sort_rule == AB_SORT_RULE_NICK_LISTS) ?
						cmp_cntr_by_nick_lists_last :
	    /* (ab->sort_rule == AB_SORT_RULE_NICK) */
						cmp_cntr_by_nick);
    }

    dprint((9, "- adrbk_sort: done with first sort -\n"));

    if(we_turned_on)
      intr_handling_off();

    if(skip_the_sort){
	q_status_message(SM_ORDER, 3, 3,
	    _("Address book sort cancelled, using old order for now"));
	goto skip_the_write_too;
    }

    dprint((5, "- adrbk_sort (%s)\n",
	    ab->sort_rule==AB_SORT_RULE_FULL_LISTS ? "FullListsLast" :
	     ab->sort_rule==AB_SORT_RULE_FULL ? "Fullname" :
	      ab->sort_rule==AB_SORT_RULE_NICK_LISTS ? "NickListLast" :
	       ab->sort_rule==AB_SORT_RULE_NICK ? "Nickname" : "unknown"));

    /*
     * Rearrange the in-core array of ae's to be in the new order.
     * We can do that by sorting the inverse into the correct order in parallel.
     */
    for(i = 0L; i < count; i++)
      inv[sort_array[i]] = i;

    if(new_entry_num && (adrbk_cntr_t) current_entry_num >= 0
       && (adrbk_cntr_t) current_entry_num < count){
	*new_entry_num = inv[(adrbk_cntr_t) current_entry_num];
    }

    /*
     * The expanded and selected lists will be wrong now. Correct them.
     * First the expanded list.
     */
    e = ab->exp ? ab->exp->next : NULL;
    while(e){
	if(e->ent >= 0 && e->ent < count)
	  e->ent = inv[e->ent];

	e = e->next;
    }

    /*
     * And sort into ascending order as expected by the exp_ routines.
     */
    e = ab->exp ? ab->exp->next : NULL;
    while(e){
	/* move smallest to e */
	e2 = e;
	smallest = e;
	while(e2){
	    if(e2->ent != NO_NEXT && e2->ent >= 0 && e2->ent < count && e2->ent < smallest->ent)
	      smallest = e2;

	    e2 = e2->next;
	}

	/* swap values in e and smallest */
	if(e != smallest){
	    tmp = e->ent;
	    e->ent = smallest->ent;
	    smallest->ent = tmp;
	}

	e = e->next;
    }

    /*
     * Same thing for the selected list.
     */
    e = ab->selects ? ab->selects->next : NULL;
    while(e){
	if(e->ent >= 0 && e->ent < count)
	  e->ent = inv[e->ent];

	e = e->next;
    }

    e = ab->selects ? ab->selects->next : NULL;
    while(e){
	/* move smallest to e */
	e2 = e;
	smallest = e;
	while(e2){
	    if(e2->ent != NO_NEXT && e2->ent >= 0 && e2->ent < count && e2->ent < smallest->ent)
	      smallest = e2;

	    e2 = e2->next;
	}

	/* swap values in e and smallest */
	if(e != smallest){
	    tmp = e->ent;
	    e->ent = smallest->ent;
	    smallest->ent = tmp;
	}

	e = e->next;
    }

    for(i = 0L; i < count; i++){
	if(i != inv[i]){
	    /* find inv[j] which = i */
	    for(j = i+1; j < count; j++){
		if(i == inv[j]){
		    hi = j;
		    break;
		}
	    }

	    /* swap i and hi */
	    ae_i  = adrbk_get_ae(ab, (a_c_arg_t) i);
	    ae_hi = adrbk_get_ae(ab, (a_c_arg_t) hi);
	    if(ae_i && ae_hi){
		memcpy(&ae_tmp, ae_i, sizeof(ae_tmp));
		memcpy(ae_i, ae_hi, sizeof(ae_tmp));
		memcpy(ae_hi, &ae_tmp, sizeof(ae_tmp));
	    }
	    /* else can't happen */

	    inv[hi] = inv[i];
	}
    }

    if(we_cancel)
      cancel_busy_cue(0);

    repair_abook_tries(ab);

    dprint((9, "- adrbk_sort: done with rearranging -\n"));

skip_the_write_too:
    if(we_cancel)
      cancel_busy_cue(0);

    if(sort_array)
      fs_give((void **) &sort_array);

    if(inv)
      fs_give((void **) &inv);

    return 0;
}


/*
 * Returns 1 if any addrbooks are in Open state, 0 otherwise.
 *
 * This is a test for ostatus == Open, not for whether or not the address book
 * FILE is opened.
 */
int
any_ab_open(void)
{
    int i, ret = 0;

    if(as.initialized)
      for(i = 0; ret == 0 && i < as.n_addrbk; i++)
	if(as.adrbks[i].ostatus == Open)
	  ret++;
    
    return(ret);
}


/*
 * Make sure addrbooks are minimally initialized.
 */
void
init_ab_if_needed(void)
{
    dprint((9, "- init_ab_if_needed -\n"));

    if(!as.initialized)
      (void)init_addrbooks(Closed, 0, 0, 1);
}


/*
 * Sets everything up to get started.
 *
 * Args: want_status      -- The desired OpenStatus for all addrbooks.
 *       reset_to_top     -- Forget about the old location and put cursor
 *                           at top.
 *       open_if_only_one -- If want_status is HalfOpen and there is only
 *                           section to look at, then promote want_status
 *                           to Open.
 *       ro_warning       -- Set ReadOnly warning global
 *
 * Return: 1 if ok, 0 if problem
 */
int
init_addrbooks(OpenStatus want_status, int reset_to_top, int open_if_only_one, int ro_warning)
{
    register PerAddrBook *pab;
    char *q, **t;
    long line;

    dprint((4, "-- init_addrbooks(%s, %d, %d, %d) --\n",
		    want_status==Open ?
				"Open" :
				want_status==HalfOpen ?
					"HalfOpen" :
				   want_status==ThreeQuartOpen ?
					"ThreeQuartOpen" :
					want_status==NoDisplay ?
						"NoDisplay" :
						"Closed",
		    reset_to_top, open_if_only_one, ro_warning));

    as.l_p_page = ps_global->ttyo->screen_rows - FOOTER_ROWS(ps_global)
					       - HEADER_ROWS(ps_global);
    if(as.l_p_page <= 0)
      as.no_op_possbl++;
    else
      as.no_op_possbl = 0;

    as.ro_warning = ro_warning;

    /* already been initialized */
    if(as.initialized){
	int i;

	/*
	 * Special case. If there is only one addressbook we start the
	 * user out with that open.
	 */
	if(want_status == HalfOpen &&
	   ((open_if_only_one && as.n_addrbk == 1 && as.n_serv == 0) ||
	    (F_ON(F_EXPANDED_ADDRBOOKS, ps_global) &&
	     F_ON(F_CMBND_ABOOK_DISP, ps_global))))
	  want_status = Open;

	/* open to correct state */
	for(i = 0; i < as.n_addrbk; i++)
	  init_abook(&as.adrbks[i], want_status);

	if(reset_to_top){
	    warp_to_beginning();
	    as.top_ent     = 0L;
	    line           = first_selectable_line(0L);
	    if(line == NO_LINE)
	      as.cur_row = 0L;
	    else
	      as.cur_row = line;

	    if(as.cur_row >= as.l_p_page)
	      as.top_ent += (as.cur_row - as.l_p_page + 1);

	    as.old_cur_row = as.cur_row;
	}

	dprint((4, "init_addrbooks: already initialized: %d books\n",
				    as.n_addrbk));
        return((as.n_addrbk + as.n_serv) ? 1 : 0);
    }

    as.initialized = 1;

    /* count directory servers */
    as.n_serv = 0;
    as.n_impl = 0;
#ifdef	ENABLE_LDAP
    if(ps_global->VAR_LDAP_SERVERS &&
       ps_global->VAR_LDAP_SERVERS[0] &&
       ps_global->VAR_LDAP_SERVERS[0][0])
	for(t = ps_global->VAR_LDAP_SERVERS; t[0] && t[0][0]; t++){
	    LDAP_SERV_S *info;

	    as.n_serv++;
	    info = break_up_ldap_server(*t);
	    as.n_impl += (info && info->impl) ? 1 : 0;
	    if(info)
	      free_ldap_server_info(&info);
	}
#endif	/* ENABLE_LDAP */

    /* count addressbooks */
    as.how_many_personals = 0;
    if(ps_global->VAR_ADDRESSBOOK &&
       ps_global->VAR_ADDRESSBOOK[0] &&
       ps_global->VAR_ADDRESSBOOK[0][0])
	for(t = ps_global->VAR_ADDRESSBOOK; t[0] && t[0][0]; t++)
	  as.how_many_personals++;

    as.n_addrbk = as.how_many_personals;
    if(ps_global->VAR_GLOB_ADDRBOOK &&
       ps_global->VAR_GLOB_ADDRBOOK[0] &&
       ps_global->VAR_GLOB_ADDRBOOK[0][0])
	for(t = ps_global->VAR_GLOB_ADDRBOOK; t[0] && t[0][0]; t++)
	  as.n_addrbk++;

    if(want_status == HalfOpen &&
       ((open_if_only_one && as.n_addrbk == 1 && as.n_serv == 0) ||
	(F_ON(F_EXPANDED_ADDRBOOKS, ps_global) &&
	 F_ON(F_CMBND_ABOOK_DISP, ps_global))))
      want_status = Open;


    /*
     * allocate array of PerAddrBooks
     * (we don't give this up until we exit Pine, but it's small)
     */
    if(as.n_addrbk){
	as.adrbks = (PerAddrBook *)fs_get(as.n_addrbk * sizeof(PerAddrBook));
	memset((void *)as.adrbks, 0, as.n_addrbk * sizeof(PerAddrBook));

	/* init PerAddrBook data */
	for(as.cur = 0; as.cur < as.n_addrbk; as.cur++){
	    char *nickname = NULL,
		 *filename = NULL;

	    if(as.cur < as.how_many_personals)
	      q = ps_global->VAR_ADDRESSBOOK[as.cur];
	    else
	      q = ps_global->VAR_GLOB_ADDRBOOK[as.cur - as.how_many_personals];

	    pab = &as.adrbks[as.cur];
	    
	    /* Parse entry for optional nickname and filename */
	    get_pair(q, &nickname, &filename, 0, 0);

	    if(nickname && !*nickname)
	      fs_give((void **)&nickname);

	    strncpy(tmp_20k_buf, filename, SIZEOF_20KBUF);
	    fs_give((void **)&filename);

	    filename = tmp_20k_buf;
	    if(nickname == NULL)
	      pab->abnick = cpystr(filename);
	    else
	      pab->abnick = nickname;

	    if(*filename == '~')
	      fnexpand(filename, SIZEOF_20KBUF);

	    if(*filename == '{' || is_absolute_path(filename)){
		pab->filename = cpystr(filename); /* fully qualified */
	    }
	    else{
		char book_path[MAXPATH+1];
		char *lc = last_cmpnt(ps_global->pinerc);

		book_path[0] = '\0';
		if(lc != NULL){
		    strncpy(book_path, ps_global->pinerc,
			    MIN(lc - ps_global->pinerc, sizeof(book_path)-1));
		    book_path[MIN(lc - ps_global->pinerc,
				  sizeof(book_path)-1)] = '\0';
		}

		strncat(book_path, filename,
			sizeof(book_path)-1-strlen(book_path));
		pab->filename = cpystr(book_path);
	    }

	    if(*pab->filename == '{')
	      pab->type |= REMOTE_VIA_IMAP;

	    if(as.cur >= as.how_many_personals)
	      pab->type |= GLOBAL;

	    pab->access = adrbk_access(pab);

	    /* global address books are forced readonly */
	    if(pab->type & GLOBAL && pab->access != NoAccess)
	      pab->access = ReadOnly;

	    pab->ostatus  = TotallyClosed;

	    /*
	     * and remember that the memset above initializes everything
	     * else to 0
	     */

	    init_abook(pab, want_status);
	}
    }

    /*
     * Have to reset_to_top in this case since this is the first open,
     * regardless of the value of the argument, since these values haven't been
     * set before here.
     */
    as.cur         = 0;
    as.top_ent     = 0L;
    warp_to_beginning();
    line           = first_selectable_line(0L);

    if(line == NO_LINE)
      as.cur_row = 0L;
    else
      as.cur_row = line;

    if(as.cur_row >= as.l_p_page){
	as.top_ent += (as.cur_row - as.l_p_page + 1);
	as.cur_row = as.l_p_page - 1;
    }

    as.old_cur_row = as.cur_row;

    return((as.n_addrbk + as.n_serv) ? 1 : 0);
}


/*
 * Something was changed in options screen, so need to start over.
 */
void
addrbook_reset(void)
{
    dprint((4, "- addrbook_reset -\n"));
    completely_done_with_adrbks();
}


/*
 * Sort was changed in options screen. Since we only sort normally
 * when we actually make a change to the address book, we need to
 * go out of our way to sort here.
 */
void
addrbook_redo_sorts(void)
{
    int i;
    PerAddrBook *pab;
    AdrBk *ab;

    dprint((4, "- addrbook_redo_sorts -\n"));

    addrbook_reset();
    init_ab_if_needed();

    for(i = 0; i < as.n_addrbk; i++){
	pab = &as.adrbks[i];
	init_abook(pab, NoDisplay);
	ab = pab->address_book;

	if(!adrbk_is_in_sort_order(ab, 0))
	  adrbk_write(ab, 0, NULL, NULL, 1, 0);
    }

    addrbook_reset();
}


/*
 * Returns type of access allowed on this addrbook.
 */
AccessType
adrbk_access(PerAddrBook *pab)
{
    char       fbuf[MAXPATH+1];
    AccessType access = NoExists;
    CONTEXT_S *dummy_cntxt = NULL;

    dprint((9, "- addrbook_access -\n"));

    if(pab && pab->type & REMOTE_VIA_IMAP){
	/*
	 * Open_fcc creates the folder if it didn't already exist.
	 */
	if((pab->so = open_fcc(pab->filename, &dummy_cntxt, 1,
			       "Error: ",
			       " Can't fetch remote addrbook.")) != NULL){
	    /*
	     * We know the folder is there but don't know what access
	     * rights we have until we try to select it, which we don't
	     * want to do unless we have to. So delay evaluating.
	     */
	    access = MaybeRorW;
	}
    }
    else if(pab){	/* local file */
#if defined(NO_LOCAL_ADDRBOOKS)
	/* don't allow any access to local addrbooks */
	access = NoAccess;
#else /* !NO_LOCAL_ADDRBOOKS) */
	build_path(fbuf, is_absolute_path(pab->filename) ? NULL
							 : ps_global->home_dir,
		   pab->filename, sizeof(fbuf));

#if defined(DOS) || defined(OS2)
	/*
	 * Microsoft networking causes some access calls to do a DNS query (!!)
	 * when it is turned on. In particular, if there is a / in the filename
	 * this seems to happen. So, just don't allow it.
	 */
	if(strindex(fbuf, '/') != NULL){
	    dprint((2, "\"/\" not allowed in addrbook name\n"));
	    return NoAccess;
	}
#else /* !DOS */
	/* also prevent backslash in non-DOS addrbook names */
	if(strindex(fbuf, '\\') != NULL){
	    dprint((2, "\"\\\" not allowed in addrbook name\n"));
	    return NoAccess;
	}
#endif /* !DOS */

	if(can_access(fbuf, ACCESS_EXISTS) == 0){
	    if(can_access(fbuf, EDIT_ACCESS) == 0){
		char *dir, *p;

		dir = ".";
		if((p = last_cmpnt(fbuf)) != NULL){
		    *--p = '\0';
		    dir  = *fbuf ? fbuf : "/";
		}

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
		 * Even if we can edit the address book file itself, we aren't
		 * going to be able to change it unless we can also write in
		 * the directory that contains it (because we write into a
		 * temp file and then rename).
		 */
		if(can_access(dir, EDIT_ACCESS) == 0)
		  access = ReadWrite;
		else{
		    access = ReadOnly;
		    q_status_message1(SM_ORDER, 2, 2,
				      "Address book directory (%.200s) is ReadOnly",
				      dir);
		}
	    }
	    else if(can_access(fbuf, READ_ACCESS) == 0)
	      access = ReadOnly;
	    else
	      access = NoAccess;
	}
#endif /* !NO_LOCAL_ADDRBOOKS) */
    }
    
    return(access);
}


/*
 * Trim back remote address books if necessary.
 */
void
trim_remote_adrbks(void)
{
    register PerAddrBook *pab;
    int i;

    dprint((2, "- trim_remote_adrbks -\n"));

    if(!as.initialized)
      return;

    for(i = 0; i < as.n_addrbk; i++){
	pab = &as.adrbks[i];
	if(pab->ostatus != TotallyClosed && pab->address_book
	   && pab->address_book->rd)
	  rd_trim_remdata(&pab->address_book->rd);
    }
}


/*
 * Free and close everything.
 */
void
completely_done_with_adrbks(void)
{
    register PerAddrBook *pab;
    int i;

    dprint((2, "- completely_done_with_adrbks -\n"));

    ab_nesting_level = 0;

    if(!as.initialized)
      return;

    for(i = 0; i < as.n_addrbk; i++)
      init_abook(&as.adrbks[i], TotallyClosed);

    for(i = 0; i < as.n_addrbk; i++){
	pab = &as.adrbks[i];

	if(pab->filename)
	  fs_give((void **)&pab->filename);

	if(pab->abnick)
	  fs_give((void **)&pab->abnick);
    }

    done_with_dlc_cache();

    if(as.adrbks)
      fs_give((void **)&as.adrbks);

    as.n_addrbk    = 0;
    as.initialized = 0;
}


/*
 * Initialize or re-initialize this address book.
 *
 *  Args: pab        -- the PerAddrBook ptr
 *       want_status -- desired OpenStatus for this address book
 */
void
init_abook(PerAddrBook *pab, OpenStatus want_status)
{
    register OpenStatus new_status;

    dprint((4, "- init_abook -\n"));
    dprint((7, "    addrbook nickname = %s filename = %s",
	   pab->abnick ? pab->abnick : "<null>",
	   pab->filename ? pab->filename : "<null>"));
    dprint((7, "  ostatus was %s, want %s\n",
	   pab->ostatus==Open ? "Open" :
	     pab->ostatus==HalfOpen ? "HalfOpen" :
	       pab->ostatus==ThreeQuartOpen ? "ThreeQuartOpen" :
		 pab->ostatus==NoDisplay ? "NoDisplay" :
		   pab->ostatus==Closed ? "Closed" : "TotallyClosed",
	   want_status==Open ? "Open" :
	     want_status==HalfOpen ? "HalfOpen" :
	       want_status==ThreeQuartOpen ? "ThreeQuartOpen" :
		 want_status==NoDisplay ? "NoDisplay" :
		   want_status==Closed ? "Closed" : "TotallyClosed"));

    new_status = want_status;  /* optimistic default */

    if(want_status == TotallyClosed){
	if(pab->address_book != NULL){
	    adrbk_close(pab->address_book);
	    pab->address_book = NULL;
	}

	if(pab->so != NULL){
	    so_give(&pab->so);
	    pab->so = NULL;
	}
    }
    /*
     * If we don't need it, release some addrbook memory by calling
     * adrbk_partial_close().
     */
    else if((want_status == Closed || want_status == HalfOpen) &&
	pab->address_book != NULL){
	adrbk_partial_close(pab->address_book);
    }
    /* If we want the addrbook read in and it hasn't been, do so */
    else if(want_status == Open || want_status == NoDisplay){
      if(pab->address_book == NULL){  /* abook handle is not currently active */
	if(pab->access != NoAccess){
	    char warning[800]; /* place to put a warning */
	    int sort_rule;

	    warning[0] = '\0';
	    if(pab->access == ReadOnly)
	      sort_rule = AB_SORT_RULE_NONE;
	    else
	      sort_rule = ps_global->ab_sort_rule;

	    pab->address_book = adrbk_open(pab,
		   ps_global->home_dir, warning, sizeof(warning), sort_rule);

	    if(pab->address_book == NULL){
		pab->access = NoAccess;
		if(want_status == Open){
		    new_status = HalfOpen;  /* best we can do */
		    q_status_message1(SM_ORDER | SM_DING, *warning?1:3, 4,
				      _("Error opening/creating address book %.200s"),
				      pab->abnick);
		    if(*warning)
			q_status_message2(SM_ORDER, 3, 4, "%.200s: %.200s",
			    as.n_addrbk > 1 ? pab->abnick : "addressbook",
			    warning);
		}
		else
		    new_status  = Closed;

		dprint((1, "Error opening address book %s: %s\n",
			  pab->abnick ? pab->abnick : "?",
			  error_description(errno)));
	    }
	    else{
		if(pab->access == NoExists)
		  pab->access = ReadWrite;

		if(pab->access == ReadWrite){
		    /*
		     * Add forced entries if there are any.  These are
		     * entries that are always supposed to show up in
		     * personal address books.  They're specified in the
		     * global config file.
		     */
		    add_forced_entries(pab->address_book);
		}

		new_status = want_status;
		dprint((2, "Address book %s (%s) opened with %ld items\n",
			pab->abnick ? pab->abnick : "?",
			pab->filename ? pab->filename : "?",
			(long)adrbk_count(pab->address_book)));
		if(*warning){
		    dprint((1,
				 "Addressbook parse error in %s (%s): %s\n",
				 pab->abnick ? pab->abnick : "?",
				 pab->filename ? pab->filename : "?",
				 warning));
		    if(!pab->gave_parse_warnings && want_status == Open){
			pab->gave_parse_warnings++;
			q_status_message2(SM_ORDER, 3, 4, "%.200s: %.200s",
			    as.n_addrbk > 1 ? pab->abnick : "addressbook",
			    warning);
		    }
		}
	    }
	}
	else{
	    if(want_status == Open){
		new_status = HalfOpen;  /* best we can do */
		q_status_message1(SM_ORDER | SM_DING, 3, 4,
		   "Insufficient permissions for opening address book %.200s",
		   pab->abnick);
	    }
	    else
	      new_status = Closed;
	}
      }
      /*
       * File handle was already open but we were in Closed or HalfOpen
       * state. Check to see if someone else has changed the addrbook
       * since we first opened it.
       */
      else if((pab->ostatus == Closed || pab->ostatus == HalfOpen) &&
	      ps_global->remote_abook_validity > 0)
	(void)adrbk_check_and_fix(pab, 1, 0, 0);
    }

    pab->ostatus = new_status;
}


/*
 * Does a validity check on all the open address books.
 *
 * Return -- number of address books with invalid data
 */
int
adrbk_check_all_validity_now(void)
{
    int          i;
    int          something_out_of_date = 0;
    PerAddrBook *pab;

    dprint((7, "- adrbk_check_all_validity_now -\n"));

    if(as.initialized){
	for(i = 0; i < as.n_addrbk; i++){
	    pab = &as.adrbks[i];
	    if(pab->address_book){
		adrbk_check_validity(pab->address_book, 1L);
		if(pab->address_book->flags & FILE_OUTOFDATE ||
		   (pab->address_book->rd &&
		    pab->address_book->rd->flags & REM_OUTOFDATE))
		  something_out_of_date++;
	    }
	}
    }

    return(something_out_of_date);
}


/*
 * Fix out-of-date address book. This only fixes the AdrBk part of the
 * problem, not the pab and display. It closes and reopens the address_book
 * file, clears the cache, reads new data.
 *
 * Arg        pab -- Pointer to the PerAddrBook data for this addrbook
 *           safe -- It is safe to apply the fix
 *       low_freq -- This is a low frequency check (longer between checks)
 *
 * Returns non-zero if addrbook was fixed
 */
int
adrbk_check_and_fix(PerAddrBook *pab, int safe, int low_freq, int check_now)
{
    int  ret = 0;
    long chk_interval;

    if(!as.initialized || !pab)
      return(ret);

    dprint((7, "- adrbk_check_and_fix(%s) -\n",
	   pab->filename ? pab->filename : "?"));

    if(pab->address_book){
	if(check_now)
	  chk_interval = 1L;
	else if(low_freq)
	  chk_interval = -60L * MAX(LOW_FREQ_CHK_INTERVAL,
				    ps_global->remote_abook_validity + 5);
	else
	  chk_interval = 0L;

	adrbk_check_validity(pab->address_book, chk_interval);

	if(pab->address_book->flags & FILE_OUTOFDATE ||
	   (pab->address_book->rd &&
	    pab->address_book->rd->flags & REM_OUTOFDATE &&
	    !(pab->address_book->rd->flags & USER_SAID_NO))){
	    if(safe){
		OpenStatus save_status;
		int        save_rem_abook_valid = 0;

		dprint((2, "adrbk_check_and_fix %s: fixing %s\n",
		       debug_time(0,0),
		       pab->filename ? pab->filename : "?"));
		if(ab_nesting_level > 0){
		    q_status_message3(SM_ORDER, 0, 2,
				     "Resyncing address book%.200s%.200s%.200s",
				     as.n_addrbk > 1 ? " \"" : "",
				     as.n_addrbk > 1 ? pab->abnick : "",
				     as.n_addrbk > 1 ? "\"" : "");
		    display_message('x');
		    fflush(stdout);
		}

		ret++;
		save_status = pab->ostatus;

		/* don't do the trim right now */
		if(pab->address_book->rd)
		  pab->address_book->rd->flags &= ~DO_REMTRIM;

		/*
		 * Have to change this from -1 or we won't actually do
		 * the resync.
		 */
		if((pab->address_book->rd &&
		    pab->address_book->rd->flags & REM_OUTOFDATE) &&
		   ps_global->remote_abook_validity == -1){
		    save_rem_abook_valid = -1;
		    ps_global->remote_abook_validity = 0;
		}

		init_abook(pab, TotallyClosed);

		pab->so = NULL;
		/* this sets up pab->so, so is important */
		pab->access = adrbk_access(pab);

		/*
		 * If we just re-init to HalfOpen... we won't actually
		 * open the address book, which was open before. That
		 * would be fine but it's a little nicer if we can open
		 * it now so that we don't defer the resync until
		 * the next open, which would be a user action for sure.
		 * Right now we may be here during a newmail check
		 * timeout, so this is a good time to do the resync.
		 */
		if(save_status == HalfOpen ||
		   save_status == ThreeQuartOpen ||
		   save_status == Closed)
		  init_abook(pab, NoDisplay);

		init_abook(pab, save_status);

		if(save_rem_abook_valid)
		  ps_global->remote_abook_validity = save_rem_abook_valid;

		if(ab_nesting_level > 0 && pab->ostatus == save_status)
		    q_status_message3(SM_ORDER, 0, 2,
				     "Resynced address book%.200s%.200s%.200s",
				     as.n_addrbk > 1 ? " \"" : "",
				     as.n_addrbk > 1 ? pab->abnick : "",
				     as.n_addrbk > 1 ? "\"" : "");
	    }
	    else
	      dprint((2,
		     "adrbk_check_and_fix: not safe to fix %s\n",
		     pab->filename ? pab->filename : "?"));
	}
    }

    return(ret);
}


static time_t last_check_and_fix_all;
/*
 * Fix out of date address books. This only fixes the AdrBk part of the
 * problem, not the pab and display. It closes and reopens the address_book
 * files, clears the caches, reads new data.
 *
 * Args      safe -- It is safe to apply the fix
 *       low_freq -- This is a low frequency check (longer between checks)
 *
 * Returns non-zero if an addrbook was fixed
 */
int
adrbk_check_and_fix_all(int safe, int low_freq, int check_now)
{
    int          i, ret = 0;
    PerAddrBook *pab;

    if(!as.initialized)
      return(ret);

    dprint((7, "- adrbk_check_and_fix_all -\n"));

    last_check_and_fix_all = get_adj_time();

    for(i = 0; i < as.n_addrbk; i++){
	pab = &as.adrbks[i];
	if(pab->address_book)
	  ret += adrbk_check_and_fix(pab, safe, low_freq, check_now);
    }

    return(ret);
}


/*
 *
 */
void
adrbk_maintenance(void)
{
    static time_t last_time_here = 0;
    time_t        now;
    int           i;
    long          low_freq_interval;

    dprint((9, "- adrbk_maintenance -\n"));

    if(!as.initialized)
      return;
    
    now = get_adj_time();

    if(now < last_time_here + 120)
      return;

    last_time_here = now;

    low_freq_interval = MAX(LOW_FREQ_CHK_INTERVAL,
			    ps_global->remote_abook_validity + 5);

    /* check the time here to make it cheap */
    if(ab_nesting_level == 0 &&
       ps_global->remote_abook_validity > 0 &&
       now > last_check_and_fix_all + low_freq_interval * 60L)
      (void)adrbk_check_and_fix_all(1, 1, 0);

    /* close down idle connections */
    for(i = 0; i < as.n_addrbk; i++){
	PerAddrBook *pab;

	pab = &as.adrbks[i];

	if(pab->address_book &&
	   pab->address_book->type == Imap &&
	   pab->address_book->rd &&
	   rd_stream_exists(pab->address_book->rd)){
	    dprint((7,
		   "adrbk_maint: %s: idle cntr %ld (%ld)\n",
		   pab->address_book->orig_filename
		     ? pab->address_book->orig_filename : "?",
		   (long)(now - pab->address_book->rd->last_use),
		   (long)pab->address_book->rd->last_use));

	    if(now > pab->address_book->rd->last_use + IMAP_IDLE_TIMEOUT){
		dprint((2,
		    "adrbk_maint %s: closing idle (%ld secs) connection: %s\n",
		    debug_time(0,0),
		    (long)(now - pab->address_book->rd->last_use),
		    pab->address_book->orig_filename
		      ? pab->address_book->orig_filename : "?"));
		rd_close_remote(pab->address_book->rd);
	    }
	    else{
		/*
		 * If we aren't going to close it, we ping it instead to
		 * make sure it stays open and doesn't timeout on us.
		 * This shouldn't be necessary unless the server has a
		 * really short timeout. If we got killed for some reason
		 * we set imap.stream to NULL.
		 * Instead of just pinging, we may as well check for
		 * updates, too.
		 */
		if(ab_nesting_level == 0 &&
		   ps_global->remote_abook_validity > 0){
		    time_t save_last_use;

		    /*
		     * We shouldn't count this as a real last_use.
		     */
		    save_last_use = pab->address_book->rd->last_use;
		    (void)adrbk_check_and_fix(pab, 1, 0, 1);
		    pab->address_book->rd->last_use = save_last_use;
		}
		/* just ping it if not safe to fix it */
		else if(!rd_ping_stream(pab->address_book->rd)){
		    dprint((2,
		      "adrbk_maint: %s: abook stream closed unexpectedly: %s\n",
		      debug_time(0,0),
		      pab->address_book->orig_filename
		        ? pab->address_book->orig_filename : "?"));
		}
	    }
	}
    }
}


/*
 * Parses a string of comma-separated addresses or nicknames into an
 * array.
 *
 * Returns an allocated, null-terminated list, or NULL.
 */
char **
parse_addrlist(char *addrfield)
{
#define LISTCHUNK  500   /* Alloc this many addresses for list at a time */
    char **al, **ad;
    char *next_addr, *cur_addr, *p, *q;
    int slots = LISTCHUNK;

    if(!addrfield)
      return((char **)NULL);

    /* allocate first chunk */
    slots = LISTCHUNK;
    al    = (char **)fs_get(sizeof(char *) * (slots+1));
    ad    = al;

    p = addrfield;

    /* skip any leading whitespace */
    for(q = p; *q && *q == SPACE; q++)
      ;/* do nothing */

    next_addr = (*q) ? q : NULL;

    /* Loop adding each address in list to array al */
    for(cur_addr = next_addr; cur_addr; cur_addr = next_addr){

	next_addr = skip_to_next_addr(cur_addr);

	q = cur_addr;
	SKIP_SPACE(q);

	/* allocate more space */
	if((ad-al) >= slots){
	    slots += LISTCHUNK;
	    fs_resize((void **)&al, sizeof(char *) * (slots+1));
	    ad = al + slots - LISTCHUNK;
	}

	if(*q)
	  *ad++ = cpystr(q);
    }

    *ad++ = NULL;

    /* free up any excess we've allocated */
    fs_resize((void **)&al, sizeof(char *) * (ad - al));
    return(al);
}


/*
 * Args  cur -- pointer to the start of the current addr in list.
 *
 * Returns a pointer to the start of the next addr or NULL if there are
 * no more addrs.
 *
 * Side effect: current addr has trailing white space removed
 * and is null terminated.
 */
char *
skip_to_next_addr(char *cur)
{
    register char *p,
		  *q;
    char          *ret_pointer;
    int in_quotes  = 0,
        in_comment = 0;
    char prev_char = '\0';

    /*
     * Find delimiting comma or end.
     * Quoted commas and commented commas don't count.
     */
    for(q = cur; *q; q++){
	switch(*q){
	  case COMMA:
	    if(!in_quotes && !in_comment)
	      goto found_comma;
	    break;

	  case LPAREN:
	    if(!in_quotes && !in_comment)
	      in_comment = 1;
	    break;

	  case RPAREN:
	    if(in_comment && prev_char != BSLASH)
	      in_comment = 0;
	    break;

	  case QUOTE:
	    if(in_quotes && prev_char != BSLASH)
	      in_quotes = 0;
	    else if(!in_quotes && !in_comment)
	      in_quotes = 1;
	    break;

	  default:
	    break;
	}

	prev_char = *q;
    }
    
found_comma:
    if(*q){  /* trailing comma case */
	*q = '\0';
	ret_pointer = q + 1;
    }
    else
      ret_pointer = NULL;  /* no more addrs after cur */

    /* remove trailing white space from cur */
    for(p = q - 1; p >= cur && isspace((unsigned char)*p); p--)
      *p = '\0';
    
    return(ret_pointer);
}


/*
 * Add entries specified by system administrator.  If the nickname already
 * exists, it is not touched.
 */
void
add_forced_entries(AdrBk *abook)
{
    AdrBk_Entry *abe;
    char *nickname, *fullname, *address;
    char *end_of_nick, *end_of_full, **t;


    if(!ps_global->VAR_FORCED_ABOOK_ENTRY ||
       !ps_global->VAR_FORCED_ABOOK_ENTRY[0] ||
       !ps_global->VAR_FORCED_ABOOK_ENTRY[0][0])
	return;

    for(t = ps_global->VAR_FORCED_ABOOK_ENTRY; t[0] && t[0][0]; t++){
	nickname = *t;

	/*
	 * syntax for each element is
	 * nick[whitespace]|[whitespace]Fullname[WS]|[WS]Address
	 */

	/* find end of nickname */
	end_of_nick = nickname;
	while(*end_of_nick
	      && !isspace((unsigned char)*end_of_nick)
	      && *end_of_nick != '|')
	  end_of_nick++;

	/* find the pipe character between nickname and fullname */
	fullname = end_of_nick;
	while(*fullname && *fullname != '|')
	  fullname++;
	
	if(*fullname)
	  fullname++;

	*end_of_nick = '\0';
	abe = adrbk_lookup_by_nick(abook, nickname, NULL);

	if(!abe){  /* If it isn't there, add it */

	    /* skip whitespace before fullname */
	    fullname = skip_white_space(fullname);

	    /* find the pipe character between fullname and address */
	    end_of_full = fullname;
	    while(*end_of_full && *end_of_full != '|')
	      end_of_full++;

	    if(!*end_of_full){
		dprint((2,
		    "missing | in forced-abook-entry \"%s\"\n",
		    nickname ? nickname : "?"));
		continue;
	    }

	    address = end_of_full + 1;

	    /* skip whitespace before address */
	    address = skip_white_space(address);

	    if(*address == '('){
		dprint((2,
		   "no lists allowed in forced-abook-entry \"%s\"\n",
		   address ? address : "?"));
		continue;
	    }

	    /* go back and remove trailing white space from fullname */
	    while(*end_of_full == '|' || isspace((unsigned char)*end_of_full)){
		*end_of_full = '\0';
		end_of_full--;
	    }

	    dprint((2,
	       "Adding forced abook entry \"%s\"\n", nickname ? nickname : ""));

	    (void)adrbk_add(abook,
			   NO_NEXT,
			   nickname,
			   fullname,
			   address,
			   NULL,
			   NULL,
			   Single,
			   (adrbk_cntr_t *)NULL,
			   (int *)NULL,
			   1,
			   0,
			   1);
	}
    }
}
