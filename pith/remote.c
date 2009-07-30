#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: remote.c 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $";
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
     remote.c
     Implements remote IMAP config files (remote config, remote abook).
  ====*/


#include "../pith/headers.h"
#include "../pith/remote.h"
#include "../pith/conf.h"
#include "../pith/imap.h"
#include "../pith/msgno.h"
#include "../pith/mailview.h"
#include "../pith/status.h"
#include "../pith/flag.h"
#include "../pith/tempfile.h"
#include "../pith/adrbklib.h"
#include "../pith/detach.h"
#include "../pith/filter.h"
#include "../pith/stream.h"
#include "../pith/options.h"
#include "../pith/busy.h"
#include "../pith/readfile.h"


/*
 * Internal prototypes
 */
REMDATA_META_S *rd_find_our_metadata(char *, unsigned long *);
int             rd_meta_is_broken(FILE *);
int             rd_add_hdr_msg(REMDATA_S *, char *);
int             rd_store_fake_hdrs(REMDATA_S *, char *, char *, char *);
int             rd_upgrade_cookies(REMDATA_S *, long, int);
int             rd_check_for_suspect_data(REMDATA_S *);


char meta_prefix[] = ".ab";


char *(*pith_opt_rd_metadata_name)(void);


char *
read_remote_pinerc(PINERC_S *prc, ParsePinerc which_vars)
{
    int        try_cache, no_perm_create_pass = 0;
    char      *file = NULL;
    unsigned   flags;


    dprint((7, "read_remote_pinerc \"%s\"\n",
	   prc->name ? prc->name : "?"));

    /*
     * We don't cache the pinerc, we always copy it.
     *
     * Don't store the config in a temporary file, just leave it
     * in memory while using it.
     * It is currently required that NO_PERM_CACHE be set if NO_FILE is set.
     */
    flags = (NO_PERM_CACHE | NO_FILE);

create_the_remote_folder:

    if(no_perm_create_pass){
	if(prc->rd){
	    prc->rd->flags &= ~DO_REMTRIM;
	    rd_close_remdata(&prc->rd);
	}

	/* this will cause the remote folder to be created */
	flags = 0;
    }

    /*
     * We could parse the name here to find what type it is. So far we
     * only have type RemImap.
     */
    prc->rd = rd_create_remote(RemImap, prc->name,
			       REMOTE_PINERC_SUBTYPE,
			       &flags, _("Error: "),
			       _(" Can't fetch remote configuration."));
    if(!prc->rd)
      goto bail_out;
    
    /*
     * On first use we just use a temp file instead of memory (NO_FILE).
     * In other words, for our convenience, we don't turn NO_FILE back on
     * here. Why is that convenient? Because of the stuff that happened in
     * rd_create_remote when flags was set to zero.
     */
    if(no_perm_create_pass)
      prc->rd->flags |= NO_PERM_CACHE;

    try_cache = rd_read_metadata(prc->rd);

    if(prc->rd->access == MaybeRorW){
	if(prc->rd->read_status == 'R' ||
           !(which_vars == ParsePers || which_vars == ParsePersPost)){
	    prc->rd->access = ReadOnly;
	    prc->rd->read_status = 'R';
	}
	else
	  prc->rd->access = ReadWrite;
    }

    if(prc->rd->access != NoExists){

	rd_check_remvalid(prc->rd, 1L);

	/*
	 * If the cached info says it is readonly but
	 * it looks like it's been fixed now, change it to readwrite.
	 */
        if((which_vars == ParsePers || which_vars == ParsePersPost) &&
	   prc->rd->read_status == 'R'){
	    /*
	     * We go to this trouble since readonly pinercs
	     * are likely a mistake. They are usually supposed to be
	     * readwrite so we open it and check if it's been fixed.
	     */
	    rd_check_readonly_access(prc->rd);
	    if(prc->rd->read_status == 'W'){
		prc->rd->access = ReadWrite;
		prc->rd->flags |= REM_OUTOFDATE;
	    }
	    else
	      prc->rd->access = ReadOnly;
	}

	if(prc->rd->flags & REM_OUTOFDATE){
	    if(rd_update_local(prc->rd) != 0){
		if(!no_perm_create_pass && prc->rd->flags & NO_PERM_CACHE
		   && !(prc->rd->flags & USER_SAID_NO)){
		    /*
		     * We don't check for the existence of the remote
		     * folder when this flag is turned on, so we could
		     * fail here because the remote folder doesn't exist.
		     * We try to create it.
		     */
		    no_perm_create_pass++;
		    goto create_the_remote_folder;
		}

		dprint((1,
		       "read_pinerc_remote: rd_update_local failed\n"));
		/*
		 * Don't give up altogether. We still may be
		 * able to use a cached copy.
		 */
	    }
	    else{
		dprint((7,
		       "%s: copied remote to local (%ld)\n",
		       prc->rd->rn ? prc->rd->rn : "?",
		       (long)prc->rd->last_use));
	    }
	}

	if(prc->rd->access == ReadWrite)
	  prc->rd->flags |= DO_REMTRIM;
    }

    /* If we couldn't get to remote folder, try using the cached copy */
    if(prc->rd->access == NoExists || prc->rd->flags & REM_OUTOFDATE){
	if(try_cache){
	    prc->rd->access = ReadOnly;
	    prc->rd->flags |= USE_OLD_CACHE;
	    q_status_message(SM_ORDER, 3, 4,
	     "Can't contact remote config server, using cached copy");
	    dprint((2,
    "Can't open remote pinerc %s, using local cached copy %s readonly\n",
		   prc->rd->rn ? prc->rd->rn : "?",
		   prc->rd->lf ? prc->rd->lf : "?"));
	}
	else{
	    prc->rd->flags &= ~DO_REMTRIM;
	    goto bail_out;
	}
    }

    if(prc->rd->flags & NO_FILE)
      /* copy text, leave sonofile for later use */
      file = cpystr((char *)so_text(prc->rd->sonofile));
    else
      file = read_file(prc->rd->lf, 0);

bail_out:
    if((which_vars == ParsePers || which_vars == ParsePersPost) &&
       (!file || !prc->rd || prc->rd->access != ReadWrite)){
	prc->readonly = 1;
	if(prc == ps_global->prc)
	  ps_global->readonly_pinerc = 1;
    }
    
   return(file);
}


/*
 * Check if the remote data folder exists and create an empty folder
 * if it doesn't exist.
 *
 * Args -        type -- The type of remote storage.
 *        remote_name -- 
 *          type_spec -- Type-specific data.
 *              flags -- 
 *         err_prefix -- Should usually end with a SPACE
 *         err_suffix -- Should usually begin with a SPACE
 *
 * Returns a pointer to a REMDATA_S with access set to either
 * NoExists or MaybeRorW. On success, "so" will point to a storage object.
 */
REMDATA_S *
rd_create_remote(RemType type, char *remote_name, char *type_spec,
		 unsigned int *flags, char *err_prefix, char *err_suffix)
{
    REMDATA_S *rd = NULL;
    CONTEXT_S *dummy_cntxt = NULL;

    dprint((7, "rd_create_remote \"%s\"\n",
	   remote_name ? remote_name : "?"));

    rd = rd_new_remdata(type, remote_name, type_spec);
    if(flags)
      rd->flags = *flags;
    
    switch(rd->type){
      case RemImap:
	if(rd->flags & NO_PERM_CACHE){
	    if(rd->rn && (rd->so = so_get(CharStar, NULL, WRITE_ACCESS))){
		if(rd->flags & NO_FILE){
		    rd->sonofile = so_get(CharStar, NULL, WRITE_ACCESS);
		    if(!rd->sonofile)
		      rd->flags &= ~NO_FILE;
		}

		/*
		 * We're not going to check if it is there in this case,
		 * in order to save ourselves some round trips and
		 * connections. We'll just try to select it and then
		 * recover at that point if it isn't already there.
		 */
		rd->flags |= REM_OUTOFDATE;
		rd->access = MaybeRorW;
	    }

	}
	else{
	    /*
	     * Open_fcc creates the folder if it didn't already exist.
	     */
	    if(rd->rn && (rd->so = open_fcc(rd->rn, &dummy_cntxt, 1,
					    err_prefix, err_suffix)) != NULL){
		/*
		 * We know the folder is there but don't know what access
		 * rights we have until we try to select it, which we don't
		 * want to do unless we have to. So delay evaluating.
		 */
		rd->access = MaybeRorW;
	    }
	}

	break;

      default:
	q_status_message(SM_ORDER, 3,5, "rd_create_remote: type not supported");
	break;
    }

    return(rd);
}


REMDATA_S *
rd_new_remdata(RemType type, char *remote_name, char *type_spec)
{
    REMDATA_S *rd = NULL;

    rd = (REMDATA_S *)fs_get(sizeof(*rd));
    memset((void *)rd, 0, sizeof(*rd));

    rd->type   = type;
    rd->access = NoExists;

    if(remote_name)
      rd->rn = cpystr(remote_name);

    switch(rd->type){
      case RemImap:
	if(type_spec)
	  rd->t.i.special_hdr = cpystr(type_spec);

	break;

      default:
	q_status_message(SM_ORDER, 3,5, "rd_new_remdata: type not supported");
	break;
    }

    return(rd);
}


/*
 * Closes the remote stream and frees.
 */
void
rd_free_remdata(REMDATA_S **rd)
{
    if(rd && *rd){
	rd_close_remote(*rd);

	if((*rd)->rn)
	  fs_give((void **)&(*rd)->rn);

	if((*rd)->lf)
	  fs_give((void **)&(*rd)->lf);

	if((*rd)->so){
	    so_give(&(*rd)->so);
	    (*rd)->so = NULL;
	}

	if((*rd)->sonofile){
	    so_give(&(*rd)->sonofile);
	    (*rd)->sonofile = NULL;
	}

	switch((*rd)->type){
	  case RemImap:
	    if((*rd)->t.i.special_hdr)
	      fs_give((void **)&(*rd)->t.i.special_hdr);

	    if((*rd)->t.i.chk_date)
	      fs_give((void **)&(*rd)->t.i.chk_date);
	    
	    break;

	  default:
	    q_status_message(SM_ORDER, 3, 5,
			     "rd_free_remdata: type not supported");
	    break;
	}

	fs_give((void **)rd);
    }
}


/*
 * Call this when finished with the remdata. This does the REMTRIM if the
 * flag is set, the DEL_FILE if the flag is set. It also closes the stream
 * and frees the rd.
 */
void
rd_trim_remdata(REMDATA_S **rd)
{
    if(!(rd && *rd))
      return;

    switch((*rd)->type){
      case RemImap:
	/*
	 * Trim the number of saved copies of remote data history.
	 * The first message is a fake message that always
	 * stays there, then come ps_global->remote_abook_history messages
	 * which are each a revision of the data, then comes the active
	 * data.
	 */
	if((*rd)->flags & DO_REMTRIM &&
	   !((*rd)->flags & REM_OUTOFDATE) &&
	   (*rd)->t.i.chk_nmsgs > ps_global->remote_abook_history + 2){

	    /* make sure stream is open */
	    rd_ping_stream(*rd);
	    rd_open_remote(*rd);

	    if(!rd_remote_is_readonly(*rd)){
		if((*rd)->t.i.stream &&
		   (*rd)->t.i.stream->nmsgs >
					ps_global->remote_abook_history + 2){
		    char sequence[20];
		    int  user_deleted = 0;

		    /*
		     * If user manually deleted some, we'd better not delete
		     * any more.
		     */
		    if(count_flagged((*rd)->t.i.stream, F_DEL) == 0L){

			dprint((4, "  rd_trim: trimming remote: mark msgs 2-%ld deleted (%s)\n", (*rd)->t.i.stream->nmsgs - 1 - ps_global->remote_abook_history, (*rd)->rn ? (*rd)->rn : "?"));
			snprintf(sequence, sizeof(sequence), "2:%ld",
		(*rd)->t.i.stream->nmsgs - 1 - ps_global->remote_abook_history);
			mail_flag((*rd)->t.i.stream, sequence,
				  "\\DELETED", ST_SET);
		    }
		    else
		      user_deleted++;

		    mail_expunge((*rd)->t.i.stream);

		    if(!user_deleted)
		      rd_update_metadata(*rd, NULL);
		    /* else
		     *  don't update metafile because user is messing with
		     *  the remote folder manually. We'd better re-read it next
		     *  time.  */
		}
	    }

	    ps_global->noshow_error = 0;
	}

	break;

      default:
	q_status_message(SM_ORDER, 3,5, "rd_trim_remdata: type not supported");
	break;
    }
}


/*
 * All done with this remote data. Trim the folder, close the
 * stream, and free.
 */
void
rd_close_remdata(REMDATA_S **rd)
{
    if(!(rd && *rd))
      return;

    rd_trim_remdata(rd);

    if((*rd)->lf && (*rd)->flags & DEL_FILE)
      our_unlink((*rd)->lf);

    /* this closes the stream and frees memory */
    rd_free_remdata(rd);
}


/*
 * Looks in the metadata file for the cache line corresponding to rd and
 * fills in data in rd.
 *
 * Return value -- 1 if it is likely that the filename we're returning
 *   is the permanent name of the local cache file and it may already have
 *   a cached copy of the data. This is to tell us if it makes sense to use
 *   the cached copy when we are unable to contact the remote server.
 *   0 otherwise.
 */
int
rd_read_metadata(REMDATA_S *rd)
{
    REMDATA_META_S *rab = NULL;
    int             try_cache = 0;

    dprint((7, "rd_read_metadata \"%s\"\n",
	   (rd && rd->rn) ? rd->rn : "?"));

    if(!rd)
      return try_cache;

    if(rd->flags & NO_PERM_CACHE)
      rab = NULL;
    else
      rab = rd_find_our_metadata(rd->rn, &rd->flags);

    if(!rab){
	if(!rd->lf){
	    rd->flags |= (NO_META_UPDATE | REM_OUTOFDATE);
	    if(!(rd->flags & NO_FILE)){
		rd->lf = temp_nam(NULL, "a6");
		rd->flags |= DEL_FILE;
	    }

	    /* display error */
	    if(!(rd->flags & NO_PERM_CACHE))
	      display_message('x');

	    dprint((2, "using temp cache file %s\n",
		   rd->lf ? rd->lf : "<none>"));
	}
    }
    else if(rab->local_cache_file){	/* A-OK, it was in the file already */
	if(!is_absolute_path(rab->local_cache_file)){
	    char  dir[MAXPATH+1], path[MAXPATH+1];
	    char *lc;

	    /*
	     * This should be the normal case. The file is stored as a
	     * filename in the pinerc dir, so that it can be
	     * accessed from the PC or from unix where the pathnames to
	     * get there will be different.
	     */

	    if((lc = last_cmpnt(ps_global->pinerc)) != NULL){
		int to_copy;

		to_copy = (lc - ps_global->pinerc > 1)
			    ? (lc - ps_global->pinerc - 1) : 1;
		strncpy(dir, ps_global->pinerc, MIN(to_copy, sizeof(dir)-1));
		dir[MIN(to_copy, sizeof(dir)-1)] = '\0';
	    }
	    else{
		dir[0] = '.';
		dir[1] = '\0';
	    }

	    build_path(path, dir, rab->local_cache_file, sizeof(path));
	    rd->lf = cpystr(path);
	}
	else{
	    rd->lf = rab->local_cache_file;
	    /* don't free this below, we're using it */
	    rab->local_cache_file = NULL;
	}

	rd->read_status = rab->read_status;

	switch(rd->type){
	  case RemImap:
	    rd->t.i.chk_date    = rab->date;
	    rab->date = NULL;	/* don't free this below, we're using it */

	    dprint((7,
	       "in read_metadata, setting chk_date from metadata to ->%s<-\n",
	       rd->t.i.chk_date ? rd->t.i.chk_date : "?"));
	    rd->t.i.chk_nmsgs   = rab->nmsgs;
	    rd->t.i.uidvalidity = rab->uidvalidity;
	    rd->t.i.uidnext     = rab->uidnext;
	    rd->t.i.uid         = rab->uid;
	    rd->t.i.chk_nmsgs   = rab->nmsgs;
	    dprint((7,
	      "setting uid=%lu uidnext=%lu uidval=%lu read_stat=%c nmsgs=%lu\n",
		 rd->t.i.uid, rd->t.i.uidnext, rd->t.i.uidvalidity,
		 rd->read_status ? rd->read_status : '0',
		 rd->t.i.chk_nmsgs));
	    break;

	  default:
	    q_status_message(SM_ORDER, 3, 5,
			     "rd_read_metadata: type not supported");
	    break;
	}

	if(rd->t.i.chk_nmsgs > 0)
	  try_cache++;	/* cache should be valid if we can't contact server */
    }
    /*
     * The line for this data wasn't in the metadata file yet.
     * Figure out what should go there and put it in.
     */
    else{
	/*
	 * The local_cache_file is where we will store the cached local
	 * copy of the remote data.
	 */
	rab->local_cache_file = tempfile_in_same_dir(ps_global->pinerc,
						     meta_prefix, NULL);
	if(rab->local_cache_file){
	    rd->lf = rab->local_cache_file;
	    rd_write_metadata(rd, 0);
	    rab->local_cache_file = NULL;
	}
	else
	  rd->lf = temp_nam(NULL, "a7");
    }

    if(rab){
	if(rab->local_cache_file)
	  fs_give((void **)&rab->local_cache_file);
	if(rab->date)
	  fs_give((void **)&rab->date);
	fs_give((void **)&rab);
    }

    return(try_cache);
}


/*
 * Write out the contents of the metadata file.
 *
 * Each line should be: folder_name cache_file uidvalidity uidnext uid nmsgs
 *							     read_status date
 *
 * If delete_it is set, then remove the line instead of updating it.
 *
 * We have to be careful with the metadata, because it exists in user's
 * existing metadata files now, and it is oriented towards only RemImap.
 * We would have to change the version number and the format of the lines
 * in the file if we add another type.
 */
void
rd_write_metadata(REMDATA_S *rd, int delete_it)
{
    char *tempfile;
    FILE *fp_old = NULL, *fp_new = NULL;
    char *p = NULL, *pinerc_dir = NULL, *metafile = NULL;
    char *rel_filename, *key;
    char  line[MAILTMPLEN];
    int   fd;

    dprint((7, "rd_write_metadata \"%s\"\n",
	   (rd && rd->rn) ? rd->rn : "?"));

    if(!rd || rd->flags & NO_META_UPDATE)
      return;

    if(rd->type != RemImap){
	q_status_message(SM_ORDER, 3, 5,
			 "rd_write_metadata: type not supported");
	return;
    }

    dprint((9, " - rd_write_metadata: rn=%s lf=%s\n",
	   rd->rn ? rd->rn : "?", rd->lf ? rd->lf : "?"));

    key = rd->rn;

    if(!(pith_opt_rd_metadata_name && (metafile = (*pith_opt_rd_metadata_name)())))
      goto io_err;

    if(!(tempfile = tempfile_in_same_dir(metafile, "a9", &pinerc_dir)))
      goto io_err;

    if((fd = our_open(tempfile, O_TRUNC|O_WRONLY|O_CREAT|O_BINARY, 0600)) >= 0)
      fp_new = fdopen(fd, "w");
    
    if(pinerc_dir && rd->lf && strlen(rd->lf) > strlen(pinerc_dir))
      rel_filename = rd->lf + strlen(pinerc_dir) + 1;
    else
      rel_filename = rd->lf;

    if(pinerc_dir)
      fs_give((void **)&pinerc_dir);

    fp_old = our_fopen(metafile, "rb");

    if(fp_new && fp_old){
	/*
	 * Write the header line.
	 */
	if(fprintf(fp_new, "%s %s Pine Metadata\n",
		   PMAGIC, METAFILE_VERSION_NUM) == EOF)
	  goto io_err;

	while((p = fgets(line, sizeof(line), fp_old)) != NULL){
	    /*
	     * Skip the header line and any lines that don't begin
	     * with a "{".
	     */
	    if(line[0] != '{')
	      continue;

	    /* skip the old cache line for this key */
	    if(strncmp(line, key, strlen(key)) == 0 && line[strlen(key)] == TAB)
	      continue;
	    
	    /* add this line to new version of file */
	    if(fputs(p, fp_new) == EOF)
	      goto io_err;
	}
    }

    /* add the cache line for this key */
    /* Warning: this is type RemImap specific right now! */
    if(!delete_it){
	if(!tempfile ||
	   !fp_old ||
	   !fp_new ||
	   p ||
	   fprintf(fp_new, "%s\t%s\t%lu\t%lu\t%lu\t%lu\t%c\t%s\n",
			    key ? key : "",
			    rel_filename ? rel_filename : "",
			    rd->t.i.uidvalidity, rd->t.i.uidnext, rd->t.i.uid,
			    rd->t.i.chk_nmsgs,
			    rd->read_status ? rd->read_status : '?',
			    rd->t.i.chk_date ? rd->t.i.chk_date : "no-match")
				== EOF)
	  goto io_err;
    }

    if(fclose(fp_new) == EOF){
	fp_new = NULL;
	goto io_err;
    }

    if(fclose(fp_old) == EOF){
	fp_old = NULL;
	goto io_err;
    }

    if(rename_file(tempfile, metafile) < 0)
      goto io_err;

    if(tempfile)
      fs_give((void **)&tempfile);
    
    if(metafile)
      fs_give((void **)&metafile);
    
    return;

io_err:
    dprint((2, "io_err in rd_write_metadata(%s), tempfile=%s: %s\n",
	    metafile ? metafile : "<NULL>", tempfile ? tempfile : "<NULL>",
	    error_description(errno)));
    q_status_message2(SM_ORDER, 3, 5,
		    "Trouble updating metafile %s, continuing (%s)",
		    metafile ? metafile : "<NULL>", error_description(errno));
    if(tempfile){
	our_unlink(tempfile);
	fs_give((void **)&tempfile);
    }
    if(metafile)
      fs_give((void **)&metafile);
    if(fp_old)
      (void)fclose(fp_old);
    if(fp_new)
      (void)fclose(fp_new);
}


void
rd_update_metadata(REMDATA_S *rd, char *date)
{
    if(!rd)
      return;

    dprint((9, " - rd_update_metadata: rn=%s lf=%s\n",
	   rd->rn ? rd->rn : "?", rd->lf ? rd->lf : "<none>"));

    switch(rd->type){
      case RemImap:
	if(rd->t.i.stream){
	    ps_global->noshow_error = 1;
	    rd_ping_stream(rd);
	    if(rd->t.i.stream){
		/*
		 * If nmsgs < 2 then something is wrong. Maybe it is just
		 * that we haven't been told about the messages we've
		 * appended ourselves. Try closing and re-opening the stream
		 * to see those.
		 */
		if(rd->t.i.stream->nmsgs < 2 ||
		   (rd->t.i.shouldbe_nmsgs &&
		    (rd->t.i.shouldbe_nmsgs != rd->t.i.stream->nmsgs))){
		    pine_mail_check(rd->t.i.stream);
		    if(rd->t.i.stream->nmsgs < 2 ||
		       (rd->t.i.shouldbe_nmsgs &&
			(rd->t.i.shouldbe_nmsgs != rd->t.i.stream->nmsgs))){
			rd_close_remote(rd);
			rd_open_remote(rd);
		    }
		}

		rd->t.i.chk_nmsgs = rd->t.i.stream ? rd->t.i.stream->nmsgs : 0L;

		/*
		 * If nmsgs < 2 something is wrong.
		 */
		if(rd->t.i.chk_nmsgs < 2){
		    rd->t.i.uidvalidity = 0L;
		    rd->t.i.uid = 0L;
		    rd->t.i.uidnext = 0L;
		}
		else{
		    rd->t.i.uidvalidity = rd->t.i.stream->uid_validity;
		    rd->t.i.uid = mail_uid(rd->t.i.stream,
					   rd->t.i.stream->nmsgs);
		    /*
		     * Uid_last is not always valid. If the last known uid is
		     * greater than uid_last, go with it instead (uid+1).
		     * If our guess is wrong (too low), the penalty is not
		     * harsh. When the uidnexts don't match we open the
		     * mailbox to check the uid of the actual last message.
		     * If it was a false hit then we adjust uidnext so it
		     * will be correct the next time through.
		     */
		    rd->t.i.uidnext = MAX(rd->t.i.stream->uid_last,rd->t.i.uid)
					+ 1;
		}
	    }

	    ps_global->noshow_error = 0;

	    /*
	     * Save the date so that we can check if it changed next time
	     * we go to write.
	     */
	    if(date){
		if(rd->t.i.chk_date)
		  fs_give((void **)&rd->t.i.chk_date);
		
		rd->t.i.chk_date = cpystr(date);
	    }

	    rd_write_metadata(rd, 0);
	}

	rd->t.i.shouldbe_nmsgs = 0;

	break;

      default:
	q_status_message(SM_ORDER, 3, 5,
			 "rd_update_metadata: type not supported");
	break;
    }
}




REMDATA_META_S *
rd_find_our_metadata(char *key, long unsigned int *flags)
{
    char        *p, *q, *metafile = NULL;
    char         line[MAILTMPLEN];
    REMDATA_META_S *rab = NULL;
    FILE        *fp;

    dprint((9, "rd_find_our_metadata \"%s\"\n", key ? key : "?"));

    if(!(key && *key))
      return rab;

    if(!(pith_opt_rd_metadata_name && (metafile = (*pith_opt_rd_metadata_name)()) != NULL))
      return rab;

    /*
     * Open the metadata file and get some information out.
     */
    fp = our_fopen(metafile, "rb");
    if(fp == NULL){
	q_status_message2(SM_ORDER, 3, 5,
		   _("can't open metadata file %s, continuing (%s)"),
		   metafile, error_description(errno));
	dprint((2,
		   "can't open existing metadata file %s: %s\n",
		   metafile ? metafile : "?", error_description(errno)));
	if(flags)
	  (*flags) |= NO_META_UPDATE;

	fs_give((void **)&metafile);

	return rab;
    }

    /*
     * If we make it to this point where we have opened the metadata file
     * we return a structure (possibly empty) instead of just a NULL pointer.
     */
    rab = (REMDATA_META_S *)fs_get(sizeof(*rab));
    memset(rab, 0, sizeof(*rab));

    /*
     * Check for header line. If it isn't there or is incorrect,
     * return with the empty rab. This call also positions the file pointer
     * past the header line.
     */
    if(rd_meta_is_broken(fp)){

	dprint((2,
		   "metadata file is broken, creating new one: %s\n",
		   metafile ? metafile : "?"));

	/* Make it size zero so we won't copy any bad stuff */
	if(fp_file_size(fp) != 0){
	    int fd;

	    (void)fclose(fp);
	    our_unlink(metafile);

	    if((fd = our_open(metafile, O_TRUNC|O_WRONLY|O_CREAT|O_BINARY, 0600)) < 0){
		q_status_message2(SM_ORDER, 3, 5,
		       _("can't create metadata file %s, continuing (%s)"),
			 metafile, error_description(errno));
		dprint((2,
			   "can't create metadata file %s: %s\n",
			   metafile ? metafile : "?",
			   error_description(errno)));
		fs_give((void **)&rab);
		fs_give((void **)&metafile);
		return NULL;
	    }

	    (void)close(fd);
	}
	else
	  (void)fclose(fp);

	fs_give((void **)&metafile);
	return(rab);
    }

    fs_give((void **)&metafile);

    /* Look for our line */
    while((p = fgets(line, sizeof(line), fp)) != NULL)
      if(strncmp(line, key, strlen(key)) == 0 && line[strlen(key)] == TAB)
	break;
    
#define SKIP_TO_TAB(p) do{while(*p && *p != TAB)p++;}while(0)

    /*
     * The line should be TAB-separated with fields:
     * folder_name cache_file uidvalidity uidnext uid nmsgs read_status date
     * This part is highly RemImap-specific right now.
     */
    if(p){				/* Found the line, parse it. */
      SKIP_TO_TAB(p);			/* skip to TAB following folder_name */
      if(*p == TAB){
	q = ++p;			/* q points to cache_file */
	SKIP_TO_TAB(p);			/* skip to TAB following cache_file */
	if(*p == TAB){
	  *p = '\0';
	  rab->local_cache_file = cpystr(q);
	  q = ++p;			/* q points to uidvalidity */
	  SKIP_TO_TAB(p);		/* skip to TAB following uidvalidity */
	  if(*p == TAB){
	    *p = '\0';
	    rab->uidvalidity = strtoul(q,(char **)NULL,10);
	    q = ++p;			/* q points to uidnext */
	    SKIP_TO_TAB(p);		/* skip to TAB following uidnext */
	    if(*p == TAB){
	      *p = '\0';
	      rab->uidnext = strtoul(q,(char **)NULL,10);
	      q = ++p;			/* q points to uid */
	      SKIP_TO_TAB(p);		/* skip to TAB following uid */
	      if(*p == TAB){
	        *p = '\0';
	        rab->uid = strtoul(q,(char **)NULL,10);
	        q = ++p;		/* q points to nmsgs */
	        SKIP_TO_TAB(p);		/* skip to TAB following nmsgs */
	        if(*p == TAB){
	          *p = '\0';
	          rab->nmsgs = strtoul(q,(char **)NULL,10);
	          q = ++p;		/* q points to read_status */
	          SKIP_TO_TAB(p);	/* skip to TAB following read_status */
	          if(*p == TAB){
	            *p = '\0';
	            rab->read_status = *q;  /* just a char, not whole string */
	            q = ++p;		/* q points to date */
		    while(*p && *p != '\n' && *p != '\r')  /* skip to newline */
		      p++;
		
		    *p = '\0';
		    rab->date = cpystr(q);
	          }
		}
	      }
	    }
	  }
	}
      }
    }

    (void)fclose(fp);
    return(rab);
}


/*
 * Returns: -1 if this doesn't look like a metafile or some error,
 *               or if it looks like a non-current metafile.
 *           0 if it looks like a current metafile.
 *
 * A side effect is that the file pointer will be pointing to the second
 * line if 0 is returned.
 */
int
rd_meta_is_broken(FILE *fp)
{
    char buf[MAILTMPLEN];

    if(fp == (FILE *)NULL)
      return -1;

    if(fp_file_size(fp) <
		    (long)(SIZEOF_PMAGIC + SIZEOF_SPACE + SIZEOF_VERSION_NUM))
      return -1;
    
    if(fseek(fp, (long)TO_FIND_HDR_PMAGIC, 0))
      return -1;

    /* check for magic */
    if(fread(buf, sizeof(char), (unsigned)SIZEOF_PMAGIC, fp) != SIZEOF_PMAGIC)
      return -1;

    buf[SIZEOF_PMAGIC] = '\0';
    if(strcmp(buf, PMAGIC) != 0)
      return -1;

    /*
     * If we change to a new version, we may want to add code here to convert
     * or to cleanup after the old version. For example, it might just
     * remove the old cache files here.
     */

    /* check for matching version number */
    if(fseek(fp, (long)TO_FIND_VERSION_NUM, 0))
      return -1;
    
    if(fread(buf, sizeof(char), (unsigned)SIZEOF_VERSION_NUM, fp) !=
       SIZEOF_VERSION_NUM)
      return -1;
    
    buf[SIZEOF_VERSION_NUM] = '\0';
    if(strcmp(buf, METAFILE_VERSION_NUM) != 0)
      return -1;

    /* Position file pointer to second line */
    if(fseek(fp, (long)TO_FIND_HDR_PMAGIC, 0))
      return -1;
    
    if(fgets(buf, sizeof(buf), fp) == NULL)
      return -1;

    return 0;
}


/*
 * Open a data stream to the remote data.
 */
void
rd_open_remote(REMDATA_S *rd)
{
    long openmode = SP_USEPOOL | SP_TEMPUSE;

    dprint((7, "rd_open_remote \"%s\"\n",
	   (rd && rd->rn) ? rd->rn : "?"));

    if(!rd)
      return;
    
    if(rd_stream_exists(rd)){
	rd->last_use = get_adj_time();
	return;
    }

    switch(rd->type){
      case RemImap:

	if(rd->access == ReadOnly)
	  openmode |= OP_READONLY;

	ps_global->noshow_error = 1;
	rd->t.i.stream = context_open(NULL, NULL, rd->rn, openmode, NULL);
	ps_global->noshow_error = 0;

        /* Don't try to reopen if there was a problem (auth failure, etc.) */
	if(!rd->t.i.stream)  
	  rd->flags |= USER_SAID_NO; /* Caution: overloading USER_SAID_NO */
	
	if(rd->t.i.stream && rd->t.i.stream->halfopen){
	    /* this is a failure */
	    rd_close_remote(rd);
	}

	if(rd->t.i.stream)
	  rd->last_use = get_adj_time();

	break;

      default:
	q_status_message(SM_ORDER, 3, 5, "rd_open_remote: type not supported");
	break;
    }
}


/*
 * Close a data stream to the remote data.
 */
void
rd_close_remote(REMDATA_S *rd)
{
    if(!rd || !rd_stream_exists(rd))
      return;

    switch(rd->type){
      case RemImap:
	ps_global->noshow_error = 1;
	pine_mail_close(rd->t.i.stream);
	rd->t.i.stream = NULL;
	ps_global->noshow_error = 0;
	break;

      default:
	q_status_message(SM_ORDER, 3, 5, "rd_close_remote: type not supported");
	break;
    }
}


int
rd_stream_exists(REMDATA_S *rd)
{
    if(!rd)
      return(0);

    switch(rd->type){
      case RemImap:
	return(rd->t.i.stream ? 1 : 0);

      default:
	q_status_message(SM_ORDER, 3,5, "rd_stream_exists: type not supported");
	break;
    }
    
    return(0);
}


/*
 * Ping the stream and return 1 if it is still alive.
 */
int
rd_ping_stream(REMDATA_S *rd)
{
    int ret = 0;

    dprint((7, "rd_ping_stream \"%s\"\n",
	   (rd && rd->rn) ? rd->rn : "?"));

    if(!rd)
      return(ret);

    if(!rd_stream_exists(rd)){
	switch(rd->type){
	  case RemImap:
	    /*
	     * If this stream is already actually open, officially open
	     * it and use it.
	     */
	    if(sp_stream_get(rd->rn, SP_MATCH)){
		long openflags = SP_USEPOOL | SP_TEMPUSE;

		if(rd->access == ReadOnly)
		  openflags |= OP_READONLY;

		rd->t.i.stream = pine_mail_open(NULL, rd->rn, openflags, NULL);
	    }

	    break;

	  default:
	    break;
	}
    }

    if(!rd_stream_exists(rd))
      return(ret);

    switch(rd->type){
      case RemImap:
	ps_global->noshow_error = 1;
	if(pine_mail_ping(rd->t.i.stream))
	  ret = 1;
	else
	  rd->t.i.stream = NULL;

	ps_global->noshow_error = 0;
	break;

      default:
	q_status_message(SM_ORDER, 3, 5, "rd_ping_stream: type not supported");
	break;
    }
    
    return(ret);
}


/*
 * Change readonly access to readwrite if appropriate. Call this if
 * the remote data ought to be readwrite but it is marked readonly in
 * the metadata.
 */
void
rd_check_readonly_access(REMDATA_S *rd)
{
    if(rd && rd->read_status == 'R'){
	switch(rd->type){
	  case RemImap:
	    rd_open_remote(rd);
	    if(rd->t.i.stream && !rd->t.i.stream->rdonly){
		rd->read_status = 'W';
		rd->access = ReadWrite;
	    }

	    break;

	  default:
	    q_status_message(SM_ORDER, 3, 5,
			     "rd_check_readonly_access: type not supported");
	    break;
	}
    }
}


/*
 * Initialize remote data. The remote data is initialized
 * with no data. On success, the local file will be empty and the remote
 * folder (if type RemImap) will contain a header message and an empty
 * data message.
 * The remote folder already exists before we get here, though it may be empty.
 *
 * Returns -1 on failure
 *          0 on success
 */
int
rd_init_remote(REMDATA_S *rd, int add_only_first_msg)
{
    int  err = 0;
    char date[200];

    dprint((7, "rd_init_remote \"%s\"\n",
	   (rd && rd->rn) ? rd->rn : "?"));

    if(rd && rd->type != RemImap){
	dprint((1, "rd_init_remote: type not supported\n"));
	return -1;
    }

    /*
     * The rest is currently type RemImap-specific. 
     */

    if(!(rd->flags & NO_FILE || rd->lf) ||
       !rd->rn || !rd->so || !rd->t.i.stream ||
       !rd->t.i.special_hdr){
	dprint((1,
	       "rd_init_remote: Unexpected error: %s is NULL\n",
	       !(rd->flags & NO_FILE || rd->lf) ? "localfile" :
		!rd->rn ? "remotename" :
		 !rd->so ? "so" :
		  !rd->t.i.stream ? "stream" :
		   !rd->t.i.special_hdr ? "special_hdr" : "?"));
	return -1;
    }

    /* already init'd */
    if(rd->t.i.stream->nmsgs >= 2 ||
       (rd->t.i.stream->nmsgs >= 1 && add_only_first_msg))
      return err;

    dprint((7, " - rd_init_remote(%s): %s\n",
	   rd->t.i.special_hdr ? rd->t.i.special_hdr : "?",
	   rd->rn ? rd->rn : "?"));
    
    /*
     * We want to protect the user who specifies an actual address book
     * file on the remote system as a remote address book. For example,
     * they might say address-book={host}.addressbook where .addressbook
     * is just a file on host. Remote address books are folders, not
     * files.  We also want to protect the user who specifies an existing
     * mail folder as a remote address book. We do that by requiring the
     * first message in the folder to be our header message.
     */

    if(rd->t.i.stream->rdonly){
	q_status_message1(SM_ORDER | SM_DING, 7, 10,
		 _("Can't initialize folder \"%s\" (write permission)"), rd->rn);
	if(rd->t.i.stream->nmsgs > 0)
	  q_status_message(SM_ORDER | SM_DING, 7, 10,
	   _("Choose a new, unused folder for the remote data"));

	dprint((1,
	       "Can't initialize remote folder \"%s\"\n",
	       rd->rn ? rd->rn : "?"));
	dprint((1,
	       "   No write permission for that remote folder.\n"));
	dprint((1,
		   "   Choose a new unused folder for the remote data.\n"));
	err = -1;
    }

    if(!err){
	if(rd->t.i.stream->nmsgs == 0){
	    int we_cancel;

	    we_cancel = busy_cue(_("Initializing remote data"), NULL, 1);
	    rfc822_date(date);
	    /*
	     * The first message in a remote data folder is a special
	     * header message. It is there as a way to explain what the
	     * folder is to users who open it trying to read it. It is also
	     * used as a consistency check so that we don't use a folder
	     * that was being used for something else as a remote
	     * data folder.
	     */
	    err = rd_add_hdr_msg(rd, date);
	    if(rd->t.i.stream->nmsgs == 0)
	      rd_ping_stream(rd);
	    if(rd->t.i.stream && rd->t.i.stream->nmsgs == 0)
	      pine_mail_check(rd->t.i.stream);

	    if(we_cancel)
	      cancel_busy_cue(-1);
	}
	else{
	    char *eptr = NULL;

	    err = rd_chk_for_hdr_msg(&(rd->t.i.stream), rd, &eptr);
	    if(err){
		q_status_message1(SM_ORDER | SM_DING, 5, 5,
		     _("\"%s\" has invalid format, can't initialize"), rd->rn);

		dprint((1,
		       "Can't initialize remote data \"%s\"\n",
		       rd->rn ? rd->rn : "?"));

		if(eptr){
		    q_status_message1(SM_ORDER, 3, 5, "%s", eptr);
		    dprint((1, "%s\n", eptr ? eptr : "?"));
		    fs_give((void **)&eptr);
		}
	    }
	}
    }

    /*
     * Add the second (empty) message.
     */
    if(!err && !add_only_first_msg){
	char *tempfile = NULL;
	int   fd = -1;

	if(rd->flags & NO_FILE){
	    if(so_truncate(rd->sonofile, 0L) == 0)
	      err = -1;
	}
	else{
	    if(!(tempfile = tempfile_in_same_dir(rd->lf, "a8", NULL))){
		q_status_message1(SM_ORDER | SM_DING, 3, 5,
				  _("Error opening temporary file: %s"),
				  error_description(errno));
		dprint((2, "init_remote: Error opening file: %s\n",
		       error_description(errno)));
		err = -1;
	    }

	    if(!err &&
	       (fd = our_open(tempfile, O_TRUNC|O_WRONLY|O_CREAT|O_BINARY, 0600)) < 0){
		q_status_message2(SM_ORDER | SM_DING, 3, 5,
				  "Error opening temporary file %.200s: %.200s",
				  tempfile, error_description(errno));
		dprint((2,
		       "init_remote: Error opening temporary file: %s: %s\n",
		       tempfile ? tempfile : "?", error_description(errno)));
		our_unlink(tempfile);
		err = -1;
	    }
	    else
	      (void)close(fd);

	    if(!err && rename_file(tempfile, rd->lf) < 0){
		q_status_message2(SM_ORDER | SM_DING, 3, 5,
			      "Error creating cache file %.200s: %.200s",
			      rd->lf, error_description(errno));
		our_unlink(tempfile);
		err = -1;
	    }

	    if(tempfile)
	      fs_give((void **)&tempfile);
	}

	if(!err){
	    err = rd_update_remote(rd, date);
	    if(err){
		q_status_message1(SM_ORDER | SM_DING, 3, 5,
				  _("Error copying to remote folder: %s"),
				  error_description(errno));
		
		q_status_message(SM_ORDER | SM_DING, 5, 5,
				 "Creation of remote data folder failed");
	    }
	}
    }

    if(!err){
	rd_update_metadata(rd, date);
	/* turn off out of date flag */
	rd->flags &= ~REM_OUTOFDATE;
    }

    return(err ? -1 : 0);
}


/*
 * IMAP stream should already be open to a remote folder.
 * Check the first message in the folder to be sure it is the right
 * kind of message, not some message from some other folder.
 *
 * Returns 0 if ok, < 0 if invalid format.
 *
 */
int
rd_chk_for_hdr_msg(MAILSTREAM **streamp, REMDATA_S *rd, char **errmsg)
{
    char *fields[3], *values[3];
    char *h;
    int   tried_again = 0;
    int   ret;
    MAILSTREAM *st = NULL;

    fields[0] = rd->t.i.special_hdr;
    fields[1] = "received";
    fields[2] = NULL;

try_again:
    ret = -1;

    if(!streamp || !*streamp){
	dprint((1, "rd_chk_for_hdr_msg: stream is null\n"));
    }
    else if((*streamp)->nmsgs == 0){
	ret = -2;
	dprint((1,
		   "rd_chk_for_hdr_msg: stream has nmsgs=0, try a ping\n"));
	if(!pine_mail_ping(*streamp))
	  *streamp = NULL;

	if(*streamp && (*streamp)->nmsgs == 0){
	    dprint((1,
	       "rd_chk_for_hdr_msg: still looks like nmsgs=0, try a check\n"));
	    pine_mail_check(*streamp);
	}

	if(*streamp && (*streamp)->nmsgs == 0){
	    dprint((1,
	       "rd_chk_for_hdr_msg: still nmsgs=0, try re-opening stream\n"));

	    if(rd_stream_exists(rd))
	      rd_close_remote(rd);

	    rd_open_remote(rd);
	    if(rd_stream_exists(rd))
	      st = rd->t.i.stream;
	}

	if(!st)
	  st = *streamp;

	if(st && st->nmsgs == 0){
	    dprint((1,
		       "rd_chk_for_hdr_msg: can't see header message\n"));
	}
    }
    else
      st = *streamp;

    if(st && st->nmsgs != 0
       && (h=pine_fetchheader_lines(st, 1L, NULL, fields))){
	simple_header_parse(h, fields, values);
	ret = -3;
	if(values[1])
	  ret = -4;
	else if(values[0]){
	    rd->cookie = strtoul(values[0], (char **)NULL, 10);
	    if(rd->cookie == 0)
	      ret = -5;
	    else if(rd->cookie == 1){
		if(rd->flags & COOKIE_ONE_OK || tried_again)
		  ret = 0;
		else
		  ret = -6;
	    }
	    else if(rd->cookie > 1)
	      ret = 0;
	}

	if(values[0])
	  fs_give((void **)&values[0]);

	if(values[1])
	  fs_give((void **)&values[1]);

	fs_give((void **)&h);
    }
    
    
    if(ret && ret != -6 && errmsg){
	*errmsg = (char *)fs_get(500 * sizeof(char));
	(*errmsg)[0] = '\0';
    }

    if(ret == -1){
	/* null stream */
	if(errmsg)
	  snprintf(*errmsg, 500, _("Can't open remote address book \"%s\""), rd->rn);
    }
    else if(ret == -2){
	/* no messages in folder */
	if(errmsg)
	  snprintf(*errmsg, 500,
		  _("Error: no messages in remote address book \"%s\"!"),
		  rd->rn);
    }
    else if(ret == -3){
	/* no cookie */
	if(errmsg)
	  snprintf(*errmsg, 500,
		  "First msg in \"%s\" should have \"%s\" header",
		  rd->rn, rd->t.i.special_hdr);
    }
    else if(ret == -4){
	/* Received header */
	if(errmsg)
	  snprintf(*errmsg, 500,
		  _("Suspicious Received headers in first msg in \"%s\""),
		  rd->rn);
    }
    else if(ret == -5){

	/* cookie is 0 */

	/*
	 * This is a failure and should not happen, but we're not going to
	 * fail on this condition.
	 */
	dprint((1, "Unexpected value in \"%s\" header of \"%s\"\n",
	       rd->t.i.special_hdr ? rd->t.i.special_hdr : "?",
	       rd->rn ? rd->rn : "?"));
	ret = 0;
    }
    else if(ret == -6){
	dprint((1,
		   "rd_chk_for_hdr_msg: cookie is 1, try to upgrade it\n"));

	if(rd_remote_is_readonly(rd)){
	    dprint((1,
		   "rd_chk_for_hdr_msg:  can't upgrade, readonly\n"));
	    ret = 0;			/* stick with 1 */
	}
	else{
	    /* cookie is 1, upgrade it */
	    if(rd_upgrade_cookies(rd, st->nmsgs, 0) == 0){
		/* now check again */
		if(!tried_again){
		    tried_again++;
		    goto try_again;
		}
	    }

	    /*
	     * This is actually a failure but we've decided that this
	     * failure is ok.
	     */
	    ret = 0;
	}
    }

    if(errmsg && *errmsg)
      dprint((1, "rd_chk_for_hdr_msg: %s\n", *errmsg));

    return ret;
}


/*
 * For remote data, this adds the explanatory header
 * message to the remote IMAP folder.
 *
 * Args:    rd -- Remote data handle
 *        date -- The date string to use
 *
 * Result: 0 success
 *        -1 failure
 */
int
rd_add_hdr_msg(REMDATA_S *rd, char *date)
{
    int           err = 0;

    if(!rd|| rd->type != RemImap || !rd->rn || !rd->so || !rd->t.i.special_hdr){
	dprint((1,
	       "rd_add_hdr_msg: Unexpected error: %s is NULL\n",
	       !rd ? "rd" :
		!rd->rn ? "remotename" :
		 !rd->so ? "so" :
		  !rd->t.i.special_hdr ? "special_hdr" : "?"));
	return -1;
    }

    err = rd_store_fake_hdrs(rd, "Header Message for Remote Data",
			     "plain", date);

    /* Write the dummy message */
    if(!strucmp(rd->t.i.special_hdr, REMOTE_ABOOK_SUBTYPE)){
	if(!err && so_puts(rd->so,
	    "This folder contains a single Alpine addressbook.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "This message is just an explanatory message.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "The last message in the folder is the live addressbook data.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "The rest of the messages contain previous revisions of the addressbook data.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "To restore a previous revision just delete and expunge all of the messages\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "which come after it.\015\012") == 0)
	  err = -1;
    }
    else if(!strucmp(rd->t.i.special_hdr, REMOTE_PINERC_SUBTYPE)){
	if(!err && so_puts(rd->so,
	    "This folder contains a Alpine config file.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "This message is just an explanatory message.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "The last message in the folder is the live config data.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "The rest of the messages contain previous revisions of the data.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "To restore a previous revision just delete and expunge all of the messages\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "which come after it.\015\012") == 0)
	  err = -1;
    }
    else{
	if(!err && so_puts(rd->so,
	    "This folder contains remote Alpine data.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "This message is just an explanatory message.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "The last message in the folder is the live data.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "The rest of the messages contain previous revisions of the data.\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "To restore a previous revision just delete and expunge all of the messages\015\012") == 0)
	  err = -1;
	if(!err && so_puts(rd->so,
	    "which come after it.\015\012") == 0)
	  err = -1;
    }

    /* Take the message from "so" to the remote folder */
    if(!err){
	MAILSTREAM *st;

	if((st = rd->t.i.stream) != NULL)
	  rd->t.i.shouldbe_nmsgs = rd->t.i.stream->nmsgs + 1;
	else
	  st = adrbk_handy_stream(rd->rn);

	err = write_fcc(rd->rn, NULL, rd->so, st, "remote data", NULL) ? 0 : -1;
    }
    
    return err;
}


/*
 * Write some fake header lines into storage object rd->so.
 *
 * Args: rd
 *     subject -- subject to put in header
 *     subtype -- subtype to put in header
 *     date    -- date to put in header
 */
int
rd_store_fake_hdrs(REMDATA_S *rd, char *subject, char *subtype, char *date)
{
    ENVELOPE     *fake_env;
    BODY         *fake_body;
    ADDRESS      *fake_from;
    int           err = 0;
    char          vers[50], *p;
    unsigned long r = 0L;
    RFC822BUFFER rbuf;

    if(!rd|| rd->type != RemImap || !rd->so || !rd->t.i.special_hdr)
      return -1;
    
    fake_env = (ENVELOPE *)fs_get(sizeof(ENVELOPE));
    memset(fake_env, 0, sizeof(ENVELOPE));
    fake_body = (BODY *)fs_get(sizeof(BODY));
    memset(fake_body, 0, sizeof(BODY));
    fake_from = (ADDRESS *)fs_get(sizeof(ADDRESS));
    memset(fake_from, 0, sizeof(ADDRESS));

    fake_env->subject = cpystr(subject);
    fake_env->date = (unsigned char *) cpystr(date);
    fake_from->personal = cpystr("Pine Remote Data");
    fake_from->mailbox = cpystr("nobody");
    fake_from->host = cpystr("nowhere");
    fake_env->from = fake_from;
    fake_body->type = REMOTE_DATA_TYPE;
    fake_body->subtype = cpystr(subtype);
    set_parameter(&fake_body->parameter, "charset", "UTF-8");

    if(rd->cookie > 0)
      r = rd->cookie;

    if(!r){
	int i;

	for(i = 100; i > 0 && r < 1000000; i--)
	  r = random();
	
	if(r < 1000000)
	  r = 1712836L + getpid();
	
	rd->cookie = r;
    }

    snprintf(vers, sizeof(vers), "%ld", r);

    p = tmp_20k_buf;
    *p = '\0';
    rbuf.f   = dummy_soutr;
    rbuf.s   = NULL;
    rbuf.beg = p;
    rbuf.cur = p;
    rbuf.end = p+SIZEOF_20KBUF-1;
    rfc822_output_header_line(&rbuf, rd->t.i.special_hdr, 0L, vers);
    rfc822_output_header(&rbuf, fake_env, fake_body, NULL, 0L);
    *rbuf.cur = '\0';

    mail_free_envelope(&fake_env);
    mail_free_body(&fake_body);

    /* Write the fake headers */
    if(so_puts(rd->so, tmp_20k_buf) == 0)
      err = -1;
    
    return err;
}


/*
 * We have discovered that the data in the remote folder is suspicious.
 * In some cases it is just because it is from an old version of pine.
 * We have decided to update the data so that it won't look suspicious
 * next time.
 *
 * Args -- only_update_last  If set, that means to just add a new last message
 *                           by calling rd_update_remote. Don't create a new
 *                           header message and delete the old header message.
 *         nmsgs             Not used if only_update_last is set
 */
int
rd_upgrade_cookies(REMDATA_S *rd, long int nmsgs, int only_update_last)
{
    char date[200];
    int  err = 0;

    /*
     * We need to copy the data from the last message, add a new header
     * message with a random cookie, add the data back in with the
     * new cookie, and delete the old messages.
     */

    /* get data */
    rd->flags |= COOKIE_ONE_OK;

    /*
     * The local copy may be newer than the remote copy. We don't want to
     * blast the local copy in that case. The BELIEVE_CACHE flag tells us
     * to not do the blasting.
     */
    if(rd->flags & BELIEVE_CACHE)
      rd->flags &= ~BELIEVE_CACHE;
    else{
	dprint((1, "rd_upgrade_cookies:  copy abook data\n"));
	err = rd_update_local(rd);
    }

    if(!err && !only_update_last){
	rd->cookie = 0;		/* causes new cookie to be generated */
	rfc822_date(date);
	dprint((1, "rd_upgrade_cookies:  add new hdr msg to end\n"));
	err = rd_add_hdr_msg(rd, date);
    }

    if(!err){
	dprint((1, "rd_upgrade_cookies:  copy back data\n"));
	err = rd_update_remote(rd, NULL);
    }

    rd->flags &= ~COOKIE_ONE_OK;

    if(!err && !only_update_last){
	char sequence[20];

	/*
	 * We've created a new header message and added a new copy of the
	 * data after it. Only problem is that the new copy will have used
	 * the original header message to get its cookie (== 1) from. We
	 * could have deleted the original messages before the last step
	 * to get it right but that would delete all copies of the data
	 * temporarily. Delete now and then re-update.
	 */
	rd_ping_stream(rd);
	rd_open_remote(rd);
	if(rd->t.i.stream && rd->t.i.stream->nmsgs >= nmsgs+2){
	    snprintf(sequence, sizeof(sequence), "1:%ld", nmsgs);
	    mail_flag(rd->t.i.stream, sequence, "\\DELETED", ST_SET);
	    mail_expunge(rd->t.i.stream);
	    err = rd_update_remote(rd, NULL);
	}
    }

    return(err);
}


/*
 * Copy remote data to local file. If needed, the remote data is initialized
 * with no data. On success, the local file contains the remote data (which
 * means it will be empty if we initialized).
 *
 * Returns != 0 on failure
 *            0 on success
 */
int
rd_update_local(REMDATA_S *rd)
{
    char     *error;
    STORE_S  *store;
    gf_io_t   pc;
    int       i, we_cancel = 0;
    BODY     *body = NULL;
    ENVELOPE *env;
    char     *tempfile = NULL;


    if(!rd || !(rd->flags & NO_FILE || rd->lf) || !rd->rn){
	dprint((1,
	       "rd_update_local: Unexpected error: %s is NULL\n",
	       !rd ? "rd" :
		!(rd->flags & NO_FILE || rd->lf) ? "localfile" :
		 !rd->rn ? "remotename" : "?"));

	return -1;
    }

    dprint((3, " - rd_update_local(%s): %s => %s\n",
	   rd->type == RemImap ? "Imap" : "?", rd->rn ? rd->rn : "?",
	   (rd->flags & NO_FILE) ? "<mem>" : (rd->lf ? rd->lf : "?")));
    
    switch(rd->type){
      case RemImap:
	if(!rd->so || !rd->t.i.special_hdr){
	    dprint((1,
		   "rd_update_local: Unexpected error: %s is NULL\n",
		   !rd->so ? "so" :
		    !rd->t.i.special_hdr ? "special_hdr" : "?"));
	    return -1;
	}

	rd_open_remote(rd);
	if(!rd_stream_exists(rd)){
	    if(rd->flags & NO_PERM_CACHE){
		dprint((1,
	     "rd_update_local: backtrack, remote folder does not exist yet\n"));
	    }
	    else{
		dprint((1,
		       "rd_update_local: Unexpected error: stream is NULL\n"));
	    }

	    return -1;
	}

	if(rd->t.i.stream){
	    char  ebuf[500];
	    char *eptr = NULL;
	    int   chk;

	    /* force ReadOnly */
	    if(rd->t.i.stream->rdonly){
		rd->read_status = 'R';
		rd->access = ReadOnly;
	    }
	    else
	      rd->read_status = 'W';

	    if(rd->t.i.stream->nmsgs < 2)
	      return(rd_init_remote(rd, 0));
	    else if(rd_chk_for_hdr_msg(&(rd->t.i.stream), rd, &eptr)){
		q_status_message1(SM_ORDER | SM_DING, 5, 5,
		     _("Can't initialize \"%s\" (invalid format)"), rd->rn);

		if(eptr){
		    q_status_message1(SM_ORDER, 3, 5, "%s", eptr);
		    dprint((1, "%s\n", eptr));
		    fs_give((void **)&eptr);
		}

		dprint((1,
		       "Can't initialize remote data \"%s\"\n",
		       rd->rn ? rd->rn : "?"));
		return -1;
	    }

	    we_cancel = busy_cue(_("Copying remote data"), NULL, 1);

	    if(rd->flags & NO_FILE){
		store = rd->sonofile;
		so_truncate(store, 0L);
	    }
	    else{
		if(!(tempfile = tempfile_in_same_dir(rd->lf, "a8", NULL))){
		    q_status_message1(SM_ORDER | SM_DING, 3, 5,
				      _("Error opening temporary file: %s"),
				      error_description(errno));
		    dprint((2,
			  "rd_update_local: Error opening temporary file: %s\n",
			  error_description(errno)));
		    return -1;
		}

		/* Copy the data into tempfile */
		if((store = so_get(FileStar, tempfile, WRITE_ACCESS|OWNER_ONLY))
								    == NULL){
		    q_status_message2(SM_ORDER | SM_DING, 3, 5,
				  _("Error opening temporary file %s: %s"),
				      tempfile, error_description(errno));
		    dprint((2,
		      "rd_update_local: Error opening temporary file: %s: %s\n",
		      tempfile ? tempfile : "?",
		      error_description(errno)));
		    our_unlink(tempfile);
		    fs_give((void **)&tempfile);
		    return -1;
		}
	    }

	    /*
	     * Copy from the last message in the folder.
	     */
	    if(!pine_mail_fetchstructure(rd->t.i.stream, rd->t.i.stream->nmsgs,
					 &body)){
		q_status_message(SM_ORDER | SM_DING, 3, 4,
				 _("Can't access remote IMAP data"));
		dprint((2, "Can't access remote IMAP data\n"));
		if(tempfile){
		    our_unlink(tempfile);
		    fs_give((void **)&tempfile);
		}

		if(!(rd->flags & NO_FILE))
		  so_give(&store);

		if(we_cancel)
		  cancel_busy_cue(-1);

		return -1;
	    }

	    if(!body ||
	       body->type != REMOTE_DATA_TYPE ||
	       !body->subtype ||
	       strucmp(body->subtype, rd->t.i.special_hdr)){
		q_status_message(SM_ORDER | SM_DING, 3, 4,
				 _("Remote IMAP folder has wrong contents"));
		dprint((2,
		       "Remote IMAP folder has wrong contents\n"));
		if(tempfile){
		    our_unlink(tempfile);
		    fs_give((void **)&tempfile);
		}

		if(!(rd->flags & NO_FILE))
		  so_give(&store);

		if(we_cancel)
		  cancel_busy_cue(-1);

		return -1;
	    }
	    
	    if(!(env = pine_mail_fetchenvelope(rd->t.i.stream,
					       rd->t.i.stream->nmsgs))){
		q_status_message(SM_ORDER | SM_DING, 3, 4,
				 _("Can't access check date in remote data"));
		dprint((2,
		       "Can't access check date in remote data\n"));
		if(tempfile){
		    our_unlink(tempfile);
		    fs_give((void **)&tempfile);
		}

		if(!(rd->flags & NO_FILE))
		  so_give(&store);

		if(we_cancel)
		  cancel_busy_cue(-1);

		return -1;
	    }

	    if(rd && rd->flags & USER_SAID_YES)
	      chk = 0;
	    else
	      chk = rd_check_for_suspect_data(rd);

	    switch(chk){
	      case -1:		/* suspicious data, user says abort */
		if(tempfile){
		    our_unlink(tempfile);
		    fs_give((void **)&tempfile);
		}

		if(!(rd->flags & NO_FILE))
		  so_give(&store);

		if(we_cancel)
		  cancel_busy_cue(-1);

		return -1;

	      case 1:		/* suspicious data, user says go ahead */
		if(rd_remote_is_readonly(rd)){
		    dprint((1,
			   "rd_update_local:  can't upgrade, readonly\n"));
		}
		else
		  /* attempt to upgrade cookie in last message */
		  (void)rd_upgrade_cookies(rd, 0, 1);

	        break;

	      case 0:		/* all is ok */
	      default:
	        break;
	    }


	    /* store may have been written to at this point, so we'll clear it out  */
	    so_truncate(store, 0L);

	    gf_set_so_writec(&pc, store);

	    error = detach(rd->t.i.stream, rd->t.i.stream->nmsgs, "1", 0L,
			   NULL, pc, NULL, DT_NODFILTER);

	    gf_clear_so_writec(store);


	    if(we_cancel)
	      cancel_busy_cue(-1);

	    if(!(rd->flags & NO_FILE)){
		if(so_give(&store)){
		    snprintf(ebuf, sizeof(ebuf), _("Error writing temp file: %s"),
			    error_description(errno));
		    error = ebuf;
		}
	    }

	    if(error){
		q_status_message1(SM_ORDER | SM_DING, 3, 4,
			      _("%s: Error copying remote IMAP data"), error);
		dprint((2, "rd_update_local: Error copying: %s\n",
		       error ? error : "?"));
		if(tempfile){
		    our_unlink(tempfile);
		    fs_give((void **)&tempfile);
		}

		return -1;
	    }

	    if(tempfile && (i = rename_file(tempfile, rd->lf)) < 0){
#ifdef	_WINDOWS
		if(i == -5){
		    q_status_message2(SM_ORDER | SM_DING, 3, 4,
				    _("Error updating local file: %s: %s"),
				      rd->lf, error_description(errno));
		    q_status_message(SM_ORDER, 3, 4,
				 _("Perhaps another process has the file open?"));
		    dprint((2,
	"Rename_file(%s,%s): %s: returned -5, another process has file open?\n",
			   tempfile ? tempfile : "?",
			   rd->lf ? rd->lf : "?",
			   error_description(errno)));
		}
		else
#endif	/* _WINDOWS */
		{
		q_status_message2(SM_ORDER | SM_DING, 3, 5,
				  _("Error updating cache file %s: %s"),
				  rd->lf, error_description(errno));
		dprint((2,
		       "Error updating cache file %s: rename(%s,%s): %s\n",
		       tempfile ? tempfile : "?",
		       tempfile ? tempfile : "?",
		       rd->lf ? rd->lf : "?",
		       error_description(errno)));
		}

		our_unlink(tempfile);
		fs_give((void **)&tempfile);
		return -1;
	    }

	    dprint((5,
		   "in rd_update_local, setting chk_date to ->%s<-\n",
		   env->date ? (char *)env->date : "?"));
	    rd_update_metadata(rd, (char *) env->date);

	    /* turn off out of date flag */
	    rd->flags &= ~REM_OUTOFDATE;

	    if(tempfile)
	      fs_give((void **)&tempfile);

	    return 0;
	}
	else{
	    q_status_message1(SM_ORDER | SM_DING, 5, 5,
		 _("Can't open remote IMAP folder \"%s\""), rd->rn);
	    dprint((1,
		   "Can't open remote IMAP folder \"%s\"\n",
		   rd->rn ? rd->rn : "?"));
	    rd->access = ReadOnly;
	    return -1;
	}

	break;

      default:
	dprint((1, "rd_update_local: Unsupported type\n"));
	return -1;
    }
}


/*
 * Copy local data to remote folder.
 *
 * Args         lf -- Local file name
 *              rn -- Remote name
 * header_to_check -- Name of indicator header in remote folder
 *       imapstuff -- Structure holding info about connection
 *      returndate -- Pointer to the date that was stored in the remote folder
 *
 * Returns !=0 on failure
 *          0 on success
 */
int
rd_update_remote(REMDATA_S *rd, char *returndate)
{
    STORE_S    *store;
    int         err = 0;
    long        openmode = SP_USEPOOL | SP_TEMPUSE;
    MAILSTREAM *st;
    char       *eptr = NULL;

    if(rd && rd->type != RemImap){
	dprint((1, "rd_update_remote: type not supported\n"));
	return -1;
    }

    if(!rd || !(rd->flags & NO_FILE || rd->lf) || !rd->rn ||
       !rd->so || !rd->t.i.special_hdr){
	dprint((1,
	       "rd_update_remote: Unexpected error: %s is NULL\n",
	       !rd ? "rd" :
	        !(rd->flags & NO_FILE || rd->lf) ? "localfile" :
		 !rd->rn ? "remotename" :
		  !rd->so ? "so" :
		    !rd->t.i.special_hdr ? "special_hdr" : "?"));
	return -1;
    }

    dprint((7, " - rd_update_remote(%s): %s => %s\n",
	   rd->t.i.special_hdr ? rd->t.i.special_hdr : "?",
	   rd->lf ? rd->lf : "<mem>",
	   rd->rn ? rd->rn : "?"));

    if(!(st = rd->t.i.stream))
      st = adrbk_handy_stream(rd->rn);

    if(!st)
      st = rd->t.i.stream = context_open(NULL, NULL, rd->rn, openmode, NULL);

    if(!st){
	q_status_message1(SM_ORDER | SM_DING, 5, 5,
	     _("Can't open \"%s\" for copying"), rd->rn);
	dprint((1,
	  "rd_update_remote: Can't open remote folder \"%s\" for copying\n",
	       rd->rn ? rd->rn : "?"));
	return 1;
    }

    rd->last_use = get_adj_time();
    err = rd_chk_for_hdr_msg(&st, rd, &eptr);
    if(err){
	q_status_message1(SM_ORDER | SM_DING, 5, 5,
			  _("\"%s\" has invalid format"), rd->rn);

	if(eptr){
	    q_status_message1(SM_ORDER, 3, 5, "%s", eptr);
	    dprint((1, "%s\n", eptr));
	    fs_give((void **)&eptr);
	}

	dprint((1,
	   "rd_update_remote: \"%s\" has invalid format\n",
	   rd->rn ? rd->rn : "?"));
	return 1;
    }

    errno = 0;

    /*
     * The data that will be going to the remote folder rn is
     * written into the following storage object and then copied to
     * the remote folder from there.
     */

    if(rd->flags & NO_FILE){
	store = rd->sonofile;
	if(store)
	  so_seek(store, 0L, 0);	/* rewind */
    }
    else
      store = so_get(FileStar, rd->lf, READ_ACCESS);

    if(store != NULL){
	char date[200];
	unsigned char c;
	unsigned char last_c = 0;

	/* Reset the storage object, since we may have already used it. */
	if(so_truncate(rd->so, 0L) == 0)
	  err = 1;

	rfc822_date(date);
	dprint((7,
	       "in rd_update_remote, storing date ->%s<-\n",
	       date ? date : "?"));
	if(!err && rd_store_fake_hdrs(rd, "Pine Remote Data Container",
				      rd->t.i.special_hdr, date))
	  err = 1;
	
	/* save the date for later comparisons */
	if(!err && returndate)
	  strncpy(returndate, date, 100);

	/* Write the data */
	while(!err && so_readc(&c, store)){
	    /*
	     * C-client expects CRLF-terminated lines. Convert them
	     * as we copy into c-client. Read ahead isn't available.
	     * Leave CRLF as is, convert LF to CRLF, leave CR as is.
	     */
	    if(last_c != '\r' && c == '\n'){
		/* Convert \n to CRFL */
		if(so_writec('\r', rd->so) == 0 || so_writec('\n', rd->so) == 0)
		  err = 1;
		
		last_c = 0;
	    }
	    else{
		last_c = c;
		if(so_writec((int) c, rd->so) == 0)
		  err = 1;
	    }
	}

	/*
	 * Take that message from so to the remote folder.
	 * We append to that folder and always
	 * use the final message as the active data.
	 */
	if(!err){
	    MAILSTREAM *st;

	    if((st = rd->t.i.stream) != NULL)
	      rd->t.i.shouldbe_nmsgs = rd->t.i.stream->nmsgs + 1;
	    else
	      st = adrbk_handy_stream(rd->rn);

	    err = write_fcc(rd->rn, NULL, rd->so, st,
			    "remote data", NULL) ? 0 : 1;
	}


	if(!(rd->flags & NO_FILE))
	  so_give(&store);
    }
    else
      err = -1;

    if(err)
	dprint((2, "error in rd_update_remote for %s => %s\n",
	       rd->lf ? rd->lf : "<mem>", rd->rn ? rd->rn : "?"));

    return(err);
}


/*
 * Check to see if the remote data has changed since we cached it.
 *
 * Args    rd -- REMDATA handle
 *  do_it_now -- If > 0, check now regardless
 *               If = 0, check if time since last chk more than default
 *               If < 0, check if time since last chk more than -do_it_now
 */
void
rd_check_remvalid(REMDATA_S *rd, long int do_it_now)
{
    time_t      chk_interval;
    long        openmode = SP_USEPOOL | SP_TEMPUSE;
    MAILSTREAM *stat_stream = NULL;
    int         we_cancel = 0, got_cmsgs = 0;
    unsigned long current_nmsgs;

    dprint((7, "- rd_check_remvalid(%s) -\n",
	   (rd && rd->rn) ? rd->rn : ""));

    if(rd && rd->type != RemImap){
	dprint((1, "rd_check_remvalid: type not supported\n"));
	return;
    }

    if(!rd || rd->flags & REM_OUTOFDATE || rd->flags & USE_OLD_CACHE)
      return;

    if(!rd->t.i.chk_date){
	dprint((2,
	       "rd_check_remvalid: remote data %s changed (chk_date)\n",
	       rd->rn));
	rd->flags |= REM_OUTOFDATE;
	return;
    }

    if(rd->t.i.chk_nmsgs <= 1){
	dprint((2,
	  "rd_check_remvalid: remote data %s changed (chk_nmsgs <= 1)\n",
	       rd->rn ? rd->rn : "?"));
	rd->flags |= REM_OUTOFDATE;
	return;
    }

    if(do_it_now < 0L){
	chk_interval = -1L * do_it_now;
	do_it_now = 0L;
    }
    else
      chk_interval = ps_global->remote_abook_validity * 60L;

    /* too soon to check again */
    if(!do_it_now &&
       (chk_interval == 0L ||
        get_adj_time() <= rd->last_valid_chk + chk_interval))
      return;
    
    if(rd->access == ReadOnly)
      openmode |= OP_READONLY;

    rd->last_valid_chk = get_adj_time();
    mm_status_result.flags  = 0L;

    /* make sure the cache file is still there */
    if(rd->lf && can_access(rd->lf, READ_ACCESS) != 0){
	dprint((2,
	       "rd_check_remvalid: %s: cache file %s disappeared\n",
	       rd->rn ? rd->rn : "?",
	       rd->lf ? rd->lf : "?"));
	rd->flags |= REM_OUTOFDATE;
	return;
    }

    /*
     * If the stream is open we should check there instead of using
     * a STATUS command. Check to see if it is really still alive with the
     * ping. It would be convenient if we could use a status command
     * on the open stream but apparently that won't work everywhere.
     */
    rd_ping_stream(rd);

try_looking_in_stream:

    /*
     * Get the current number of messages in the folder to
     * compare with our saved number of messages.
     */
    current_nmsgs = 0;
    if(rd->t.i.stream){
	dprint((7, "using open remote data stream\n"));
	rd->last_use = get_adj_time();
	current_nmsgs = rd->t.i.stream->nmsgs;
	got_cmsgs++;
    }
    else{

	/*
	 * Try to use the imap status command
	 * to get the information as cheaply as possible.
	 * If NO_STATUSCMD is set we just bypass all this stuff.
	 */

	if(!(rd->flags & NO_STATUSCMD))
	  stat_stream = adrbk_handy_stream(rd->rn);

	/*
	 * If we don't have a stream to use for the status command we
	 * skip it and open the folder instead. Then, next time we want to
	 * check we'll probably be able to use that open stream instead of
	 * opening another one each time for the status command.
	 */
	if(stat_stream){
	    if(!LEVELSTATUS(stat_stream)){
		rd->flags |= NO_STATUSCMD;
		dprint((2,
   "rd_check_remvalid: remote data %s: server doesn't support status\n",
		       rd->rn ? rd->rn : "?"));
	    }
	    else{
		/*
		 * This sure seems like a crock. We have to check to
		 * see if the stream is actually open to the folder
		 * we want to do the status on because c-client can't
		 * do a status on an open folder. In this case, we fake
		 * the status command results ourselves.
		 * If we're so unlucky as to get back a stream that will
		 * work for the status command while we also have another
		 * stream that is rd->rn and we don't pick up on that,
		 * too bad.
		 */
		if(same_stream_and_mailbox(rd->rn, stat_stream)){
		    dprint((7,
			   "rd_check_remvalid: faking status\n"));
		    mm_status_result.flags = SA_MESSAGES | SA_UIDVALIDITY
					     | SA_UIDNEXT;
		    mm_status_result.messages = stat_stream->nmsgs;
		    mm_status_result.uidvalidity = stat_stream->uid_validity;
		    mm_status_result.uidnext = stat_stream->uid_last+1;
		}
		else{

		    dprint((7,
			   "rd_check_remvalid: trying status\n"));
		    ps_global->noshow_error = 1;
		    if(!pine_mail_status(stat_stream, rd->rn,
				    SA_UIDVALIDITY | SA_UIDNEXT | SA_MESSAGES)){
			/* failed, mark it so we won't try again */
			rd->flags |= NO_STATUSCMD;
			dprint((2,
		   "rd_check_remvalid: addrbook %s: status command failed\n",
		   rd->rn ? rd->rn : "?"));
			mm_status_result.flags = 0L;
		    }
		}

		ps_global->noshow_error = 0;
	    }
	}

	/* if we got back a result from the status command, use it */
	if(mm_status_result.flags){
	    dprint((7,
		   "rd_check_remvalid: got status_result 0x%x\n",
		   mm_status_result.flags));
	    if(mm_status_result.flags & SA_MESSAGES){
		current_nmsgs = mm_status_result.messages;
		got_cmsgs++;
	    }
	}
    }

    /*
     * Check current_nmsgs versus what we think it should be.
     * If they're different we know things have changed and we can
     * return now. If they're the same we don't know.
     */
    if(got_cmsgs && current_nmsgs != rd->t.i.chk_nmsgs){
	rd->flags |= REM_OUTOFDATE;
	dprint((2,
	       "rd_check_remvalid: remote data %s changed (current msgs (%ld) != chk_nmsgs (%ld))\n",
	       rd->rn ? rd->rn : "?", current_nmsgs, rd->t.i.chk_nmsgs));
	return;
    }
    
    /*
     * Get the current uidvalidity and uidnext values from the
     * folder to compare with our saved values.
     */
    if(rd->t.i.stream){
	if(rd->t.i.stream->uid_validity == rd->t.i.uidvalidity){
	    /*
	     * Uid is valid so we just have to check whether or not the
	     * uid of the last message has changed or not and return.
	     */
	    if(mail_uid(rd->t.i.stream, rd->t.i.stream->nmsgs) != rd->t.i.uid){
		/* uid has changed so we're out of date */
		rd->flags |= REM_OUTOFDATE;
		dprint((2,
		     "rd_check_remvalid: remote data %s changed (uid)\n",
		     rd->rn ? rd->rn : "?"));
	    }
	    else{
		dprint((7,"rd_check_remvalid: uids match\n"));
	    }

	    return;
	}
	else{
	    /*
	     * If the uidvalidity changed that probably means it can't
	     * be relied on to be meaningful, so don't use it in the future.
	     */
	    rd->flags |= NO_STATUSCMD;
	    dprint((7,
       "rd_check_remvalid: remote data %s uidvalidity changed, don't use uid\n",
		   rd->rn ? rd->rn : "?"));
	}
    }
    else{			/* stream not open, use status results */
	if(mm_status_result.flags & SA_UIDVALIDITY &&
	   mm_status_result.flags & SA_UIDNEXT &&
	   mm_status_result.uidvalidity == rd->t.i.uidvalidity){

	    /*
	     * Uids are valid.
	     */

	    if(mm_status_result.uidnext == rd->t.i.uidnext){
		/*
		 * Uidnext valid and unchanged, so the folder is unchanged.
		 */
		dprint((7, "rd_check_remvalid: uidnexts match\n"));
		return;
	    }
	    else{	/* uidnext changed, folder _may_ have changed */

		dprint((7,
   "rd_check_remvalid: uidnexts don't match, ours=%lu status=%lu\n",
		   rd->t.i.uidnext, mm_status_result.uidnext));

		/*
		 * Since c-client can't handle a status cmd on the selected
		 * mailbox, we may have had to guess at the value of uidnext,
		 * and we don't know for sure that this is a real change.
		 * We need to open the mailbox and find out for sure.
		 */
		we_cancel = busy_cue(NULL, NULL, 1);
		ps_global->noshow_error = 1;
		if((rd->t.i.stream = context_open(NULL, NULL, rd->rn, openmode,
						 NULL)) != NULL){
		    imapuid_t last_msg_uid;

		    if(rd->t.i.stream->rdonly)
		      rd->read_status = 'R';

		    last_msg_uid = mail_uid(rd->t.i.stream,
					    rd->t.i.stream->nmsgs);
		    ps_global->noshow_error = 0;
		    rd->last_use = get_adj_time();
		    dprint((7,
			   "%s: opened to check uid (%ld)\n",
			   rd->rn ? rd->rn : "?", (long)rd->last_use));
		    if(last_msg_uid != rd->t.i.uid){	/* really did change */
			rd->flags |= REM_OUTOFDATE;
			dprint((2,
 "rd_check_remvalid: remote data %s changed, our uid=%lu real uid=%lu\n",
			       rd->rn ? rd->rn : "?",
			       rd->t.i.uid, last_msg_uid));
		    }
		    else{				/* false hit */
			/*
			 * The uid of the last message is the same as we
			 * remembered, so the problem is that our guess
			 * for the nextuid was wrong. It didn't actually
			 * change. Since we know the correct uidnext now we
			 * can reset that guess to the correct value for
			 * next time, avoiding this extra mail_open.
			 */
			dprint((2,
			       "rd_check_remvalid: remote data %s false change: adjusting uidnext from %lu to %lu\n",
			       rd->rn ? rd->rn : "?",
			       rd->t.i.uidnext,
			       mm_status_result.uidnext));
			rd->t.i.uidnext = mm_status_result.uidnext;
			rd_write_metadata(rd, 0);
		    }

		    if(we_cancel)
		      cancel_busy_cue(-1);

		    return;
		}
		else{
		    ps_global->noshow_error = 0;
		    dprint((2,
			   "rd_check_remvalid: couldn't open %s\n",
			   rd->rn ? rd->rn : "?"));
		}
	    }
	}
	else{
	    /*
	     * If the status command doesn't return these or
	     * if the uidvalidity is changing that probably means it can't
	     * be relied on to be meaningful, so don't use it.
	     *
	     * We also come here if we don't have a stat_stream handy to
	     * look up the status. This happens, for example, if our
	     * address book is on a different server from the open mail
	     * folders. In that case, instead of opening a stream,
	     * doing a status command, and closing the stream, we open
	     * the stream and use it to check next time, too.
	     */
	    if(stat_stream){
		rd->flags |= NO_STATUSCMD;
		dprint((7,
		  "rd_check_remvalid: remote data %s don't use status\n",
		   rd->rn ? rd->rn : "?"));
	    }

	    dprint((7,
		   "opening remote data stream for validity check\n"));
	    we_cancel = busy_cue(NULL, NULL, 1);
	    ps_global->noshow_error = 1;
	    rd->t.i.stream = context_open(NULL, NULL, rd->rn, openmode,
					  NULL);
	    ps_global->noshow_error = 0;
	    if(we_cancel)
	      cancel_busy_cue(-1);

	    we_cancel = 0;
	    if(rd->t.i.stream)
	      goto try_looking_in_stream;
	    else{
		dprint((7,
		  "rd_check_remvalid: cannot open remote mailbox\n"));
		return;
	    }
	}
    }

    /*
     * If we got here that means that we still don't know if the folder
     * changed or not. If the stream is open then it must be the case that
     * uidvalidity changed so we can't rely on using uids.  If the stream
     * isn't open, then either the status command didn't work or the
     * uidvalidity changed. In any case, we need to fall back to looking
     * inside the folder at the last message and checking whether or not the
     * Date of the last message is the one we remembered.
     */

    dprint((7,
	   "rd_check_remvalid: falling back to Date check\n"));

    /*
     * Fall back to looking in the folder at the Date header.
     */

    if(!rd->t.i.stream)
      we_cancel = busy_cue(NULL, NULL, 1);

    ps_global->noshow_error = 1;
    if(rd->t.i.stream ||
       (rd->t.i.stream = context_open(NULL,NULL,rd->rn,openmode,NULL))){
	ENVELOPE *env = NULL;

	if(rd->t.i.stream->rdonly)
	  rd->read_status = 'R';

	if(rd->t.i.stream->nmsgs != rd->t.i.chk_nmsgs){
	    rd->flags |= REM_OUTOFDATE;
	    dprint((2,
   "rd_check_remvalid: remote data %s changed (expected nmsgs %ld, got %ld)\n",
		   rd->rn ? rd->rn : "?",
		   rd->t.i.chk_nmsgs, rd->t.i.stream->nmsgs));
	}
	else if(rd->t.i.stream->nmsgs > 1){
	    env = pine_mail_fetchenvelope(rd->t.i.stream,rd->t.i.stream->nmsgs);
	
	    if(!env || (env->date && strucmp((char *) env->date, rd->t.i.chk_date))){
		rd->flags |= REM_OUTOFDATE;
		dprint((2,
		      "rd_check_remvalid: remote data %s changed (%s)\n",
		      rd->rn ? rd->rn : "?", env ? "date" : "not enough msgs"));
	    }
	}

	rd->last_use = get_adj_time();
	if(env)
	  dprint((7,
	         "%s: got envelope to check date (%ld)\n",
	         rd->rn ? rd->rn : "?", (long)rd->last_use));
    }
    /* else, we give up and go with the cache copy */

    ps_global->noshow_error = 0;

    if(we_cancel)
      cancel_busy_cue(-1);

    dprint((7, "rd_check_remvalid: falling off end\n"));
}


/*
 * Returns nonzero if remote data is currently readonly.
 *
 * Returns 0 -- apparently writeable
 *         1 -- stream not open
 *         2 -- stream open but not writeable
 */
int
rd_remote_is_readonly(REMDATA_S *rd)
{
    if(!rd || rd->access == ReadOnly || !rd_stream_exists(rd))
      return(1);
    
    switch(rd->type){
      case RemImap:
	if(rd->t.i.stream->rdonly)
	  return(2);
	
	break;

      default:
	q_status_message(SM_ORDER, 3, 5,
			 "rd_remote_is_readonly: type not supported");
	break;
    }

    return(0);
}


/*
 * Returns  0 if ok
 *         -1 if not ok and user says No
 *          1 if not ok but user says Yes, ok to use it
 */
int
rd_check_for_suspect_data(REMDATA_S *rd)
{
    int           ans = -1;
    char         *fields[3], *values[3], *h;
    unsigned long cookie;

    if(!rd || rd->type != RemImap || !rd->so || !rd->rn || !rd->t.i.special_hdr)
      return -1;

    fields[0] = rd->t.i.special_hdr;
    fields[1] = "received";
    fields[2] = NULL;
    cookie = 0L;
    if((h=pine_fetchheader_lines(rd->t.i.stream, rd->t.i.stream->nmsgs,
				NULL, fields)) != NULL){
	simple_header_parse(h, fields, values);
	if(values[1])				/* Received lines present! */
	  ans = rd_prompt_about_forged_remote_data(-1, rd, NULL);
	else if(values[0]){
	    cookie = strtoul(values[0], (char **)NULL, 10);
	    if(cookie == rd->cookie)		/* all's well */
	      ans = 0;
	    else
	      ans = rd_prompt_about_forged_remote_data(cookie > 1L
							  ? 100 : cookie,
						       rd, values[0]);
	}
	else
	  ans = rd_prompt_about_forged_remote_data(-2, rd, NULL);

	if(values[0])
	  fs_give((void **)&values[0]);

	if(values[1])
	  fs_give((void **)&values[1]);

	fs_give((void **)&h);
    }
    else					/* missing magic header */
      ans = rd_prompt_about_forged_remote_data(-2, rd, NULL);

    return ans;
}
