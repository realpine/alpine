#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: debuging.c 900 2008-01-05 01:13:26Z hubert@u.washington.edu $";
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
#include <system.h>		/* os-dep defs/includes */
#include <general.h>		/* generally useful definitions */

#include "../../c-client/mail.h"	/* for MAILSTREAM and friends */
#include "../../c-client/osdep.h"
#include "../../c-client/rfc822.h"	/* for soutr_t and such */
#include "../../c-client/misc.h"	/* for cpystr proto */
#include "../../c-client/utf8.h"	/* for CHARSET and such*/
#include "../../c-client/imap4r1.h"

#include "../../pith/osdep/canaccess.h"

#include "../../pith/debug.h"
#include "../../pith/osdep/debugtime.h"
#include "../../pith/osdep/color.h"
#include "../../pith/osdep/bldpath.h"
#include "../../pith/osdep/rename.h"
#include "../../pith/osdep/filesize.h"
#include "../../pith/init.h"
#include "../../pith/status.h"
#include "../../pith/sort.h"
#include "../../pith/state.h"
#include "../../pith/msgno.h"
#include "../../pith/conf.h"
#include "../../pith/flag.h"
#include "../../pith/foldertype.h"
#include "../../pith/folder.h"
#include "../../pith/charconv/utf8.h"
#include "../../pith/charconv/filesys.h"
#include "../help.h"

#include "debuging.h"


#ifdef DEBUG

/*
 * globally visible 
 */
FILE *debugfile = NULL;


/*----------------------------------------------------------------------
     Initialize debugging - open the debug log file

  Args: none

 Result: opens the debug logfile for dprints

   Opens the file "~/.pine-debug1. Also maintains .pine-debug[2-4]
   by renaming them each time so the last 4 sessions are saved.
  ----*/
void
init_debug(void)
{
    char nbuf[5];
    char newfname[MAXPATH+1], filename[MAXPATH+1], *dfile = NULL;
    int i, fd;

    if(!(debug || ps_global->debug_imap || ps_global->debug_tcp))
      return;

    for(i = ps_global->debug_nfiles - 1; i > 0; i--){
        build_path(filename, ps_global->home_dir, DEBUGFILE, sizeof(filename));
        strncpy(newfname, filename, sizeof(newfname)-1);
	newfname[sizeof(newfname)-1] = '\0';
	snprintf(nbuf, sizeof(nbuf), "%d", i);
        strncat(filename, nbuf, sizeof(filename)-1-strlen(filename));
	snprintf(nbuf, sizeof(nbuf), "%d", i+1);
        strncat(newfname, nbuf, sizeof(newfname)-1-strlen(newfname));
        (void)rename_file(filename, newfname);
    }

    build_path(filename, ps_global->home_dir, DEBUGFILE, sizeof(filename)-1);
    strncat(filename, "1", sizeof(filename)-1-strlen(filename));
    filename[sizeof(filename)-1] = '\0';

    debugfile = NULL;
    dfile = filename;
    /* 
     * We were doing our_open so _WINDOWS would open it in widechar mode, but
     * that means we have to print in widechar mode.  For now just do open, since
     * we it's a debug file and still readable.  The alternative is copying all
     * debug text to a widechar buffer, which would be a drag.
     */
    if((fd = open(filename, O_TRUNC|O_RDWR|O_CREAT, 0600)) >= 0)
      debugfile = fdopen(fd, "w+");

    if(debugfile != NULL){
	char rev[128];
	time_t now = time((time_t *)0);
	if(ps_global->debug_flush)
	  setvbuf(debugfile, (char *)NULL, _IOLBF, BUFSIZ);

	if(ps_global->debug_nfiles == 0){
	    /*
	     * If no debug files are asked for, make filename a tempfile
	     * to be used for a record should pine later crash...
	     */
	    if(debug < 9 && !ps_global->debug_flush && ps_global->debug_imap<4){
		our_unlink(filename);
		dfile = NULL;
	    }
	}

	dprint((0, "Debug output of the Alpine program (debug=%d debug_imap=%d). Version %s (%s %s)\n%s\n",
	       debug, ps_global->debug_imap,
	       ALPINE_VERSION,
	       SYSTYPE ? SYSTYPE : "?",
	       get_alpine_revision_string(rev, sizeof(rev)),
	       ctime(&now)));

	dprint((0, "Starting after the reading_pinerc calls, the data in this file should\nbe encoded as UTF-8. Before that it will be in the user's native charset.\n"));
	if(dfile && (debug > DEFAULT_DEBUG ||
		     ps_global->debug_imap > 0 ||
		     ps_global->debug_tcp > 0)){
	    snprintf(newfname, sizeof(newfname), "Debug file: %s (level=%d imap=%d)", dfile,
		     debug, ps_global->debug_imap);
	    init_error(ps_global, SM_ORDER, 3, 5, newfname);
	}
    }
}


/*----------------------------------------------------------------------
     Try to save the debug file if we crash in a controlled way

  Args: dfile:  pointer to open debug file

 Result: tries to move the appropriate .pine-debugx file to .pine-crash

   Looks through the four .pine-debug files hunting for the one that is
   associated with this pine, and then renames it.
  ----*/
void
save_debug_on_crash(FILE *dfile, int (*keystrokes) (int *, char *, size_t))
{
    char nbuf[5], crashfile[MAXPATH+1], filename[MAXPATH+1];
    int i;
    struct stat dbuf, tbuf;
    time_t now = time((time_t *)0);

    if(!(dfile && fstat(fileno(dfile), &dbuf) == 0))
      return;

    fprintf(dfile, "save_debug_on_crash: Version %s: debug level %d",
	    ALPINE_VERSION, debug);
    fprintf(dfile, "\n                   : %s\n", ctime(&now));

    build_path(crashfile, ps_global->home_dir, ".pine-crash",sizeof(crashfile));

    fprintf(dfile, "Attempting to save debug file to %s\n\n", crashfile);
    fprintf(stderr,
	"\n\n       Attempting to save debug file to %s\n\n", crashfile);

    /* Blat out last n keystrokes */
    if(keystrokes){
	int   cval;
	char  cstr[256];

	fputs("========== Latest Keystrokes =========================\n", dfile);

	while((*keystrokes)(&cval, cstr, sizeof(cstr)) != -1){
	    fprintf(dfile, "\t%s\t(0x%4.4x)\n", cstr, cval);
	}
    }

    fputs("========== Latest Keystrokes End =====================\n\n", dfile);

#ifdef DEBUGJOURNAL
    fputs("========== Append DebugJournal =======================\n", dfile);
#else /* DEBUGJOURNAL */
    fputs("========== Append Journal =======================\n", dfile);
#endif /* DEBUGJOURNAL */
    debugjournal_to_file(dfile);
#ifdef DEBUGJOURNAL
    fputs("========== Append DebugJournal End ===================\n", dfile);
#else /* DEBUGJOURNAL */
    fputs("========== Append Journal End ===================\n", dfile);
#endif /* DEBUGJOURNAL */

    /* look for existing debug file */
    for(i = 1; i <= ps_global->debug_nfiles; i++){
	build_path(filename, ps_global->home_dir, DEBUGFILE, sizeof(filename));
	snprintf(nbuf, sizeof(nbuf), "%d", i);
	strncat(filename, nbuf, sizeof(filename)-1-strlen(filename));
	if(our_stat(filename, &tbuf) != 0)
	  continue;

	/* This must be the current debug file */
	if(tbuf.st_dev == dbuf.st_dev && tbuf.st_ino == dbuf.st_ino){
	    rename_file(filename, crashfile);
	    break;
	}
    }

    /* if current debug file name not found, write it by hand */
    if(i > ps_global->debug_nfiles){
	FILE *cfp;
	char  buf[1025];

	/*
	 * Copy the debug temp file into the 
	 */
	if((cfp = our_fopen(crashfile, "wb")) != NULL){
	    buf[sizeof(buf)-1] = '\0';
	    fseek(dfile, 0L, 0);
	    while(fgets(buf, sizeof(buf)-1, dfile) && fputs(buf, cfp) != EOF)
	      ;

	    fclose(cfp);
	}
    }

    fclose(dfile);
}


/*
 * functions required by pith library
 */
#define CHECK_EVERY_N_TIMES 100
#define MAX_DEBUG_FILE_SIZE 200000L
/*
 * This is just to catch runaway Pines that are looping spewing out
 * debugging (and filling up a file system).  The stop doesn't have to be
 * at all precise, just soon enough to hopefully prevent filling the
 * file system.  If the debugging level is high (9 for now), then we're
 * presumably looking for some problem, so don't truncate.
 */
int
do_debug(FILE *debug_fp)
{
    static int counter = CHECK_EVERY_N_TIMES;
    static int ok = 1;
    long filesize;

    if(!debugfile)
      return(0);

    if(debug <= DEFAULT_DEBUG
       && !ps_global->debug_flush
       && !ps_global->debug_tcp
       && !ps_global->debug_timestamp
       && !ps_global->debug_imap
       && ok
       && --counter <= 0){
	if((filesize = fp_file_size(debug_fp)) != -1L)
	  ok = (unsigned long)filesize < (unsigned long)MAX_DEBUG_FILE_SIZE;

	counter = CHECK_EVERY_N_TIMES;
	if(!ok){
	    fprintf(debug_fp, "\n\n --- No more debugging ---\n");
	    fprintf(debug_fp,
		"     (debug file growing too large - over %ld bytes)\n\n",
		MAX_DEBUG_FILE_SIZE);
	    fflush(debug_fp);
	}
    }

    if(ok && ps_global->debug_timestamp)
      fprintf(debug_fp, "\n%s\n", debug_time(0, ps_global->debug_timestamp));

    return(ok);
}

void
output_debug_msg(int dlevel, char *fmt, ...)
{
    va_list args;

    if(debugfile && debug >= dlevel && do_debug(debugfile)){
	int   l;

	va_start(args, fmt);
	vfprintf(debugfile, fmt, args);
	va_end(args);

	if((l = strlen(fmt)) > 2 && fmt[l-1] != '\n')
	  fputc('\n', debugfile);

	if(ps_global->debug_flush)
	  fflush(debugfile);
    }

    if(panicking())
      return;

#ifdef DEBUGJOURNAL

    if(dlevel <= 9 || dlevel <= debug){
	/*
	 * Make this static in order to move it off of
	 * the stack. We were getting "random" crashes
	 * on some systems when this size interacted with
	 * pthread stack size somehow. Taking it off of
	 * the stack ought to fix that without us having to
	 * understand how it all works.
	 */
	static char b[64000];

	va_start(args, fmt);
	vsnprintf(b, sizeof(b), fmt, args);
	va_end(args);

	b[sizeof(b)-1] = '\0';
	add_review_message(b, dlevel);
    }

#endif /* DEBUGJOURNAL */

}


/*----------------------------------------------------------------------
  Dump the whole config enchilada using the given function
   
  Args: ps -- pine struct containing vars to dump
	pc -- function to use to write the config data
	brief -- brief dump, only dump user_vals 
 Result: command line, global, user var's written with given function

 ----*/ 
void
dump_configuration(int brief)
{
    gf_io_t pc;

    if(!do_debug(debugfile))
      return;

    gf_set_writec(&pc, debugfile, 0L, FileStar, 0);

    dump_config(ps_global, pc, brief);
}


void
dump_config(struct pine *ps, gf_io_t pc, int brief)
{
    int	       i;
    char       quotes[3], tmp[MAILTMPLEN];
    register struct variable *vars;
    FEATURE_S *feat;

    quotes[0] = '"'; quotes[1] = '"'; quotes[2] = '\0';

    for(i = (brief ? 2 : 0); i < (brief ? 4 : 6); i++){
	snprintf(tmp, sizeof(tmp), "======= %.20s_val options set",
		(i == 0) ? "Current" :
		 (i == 1) ? "Command_line" :
		  (i == 2) ? "User" :
		   (i == 3) ? "PostloadUser" :
		    (i == 4) ? "Global"
			     : "Fixed");
	gf_puts(tmp, pc);
	if(i > 1){
	    snprintf(tmp, sizeof(tmp), " (%.100s)",
		    (i == 2) ? ((ps->prc) ? ps->prc->name : ".pinerc") :
		    (i == 3) ? ((ps->post_prc) ? ps->post_prc->name : "postload") :
		    (i == 4) ? ((ps->pconf) ? ps->pconf->name
						: SYSTEM_PINERC) :
#if defined(DOS) || defined(OS2)
		    "NO FIXED"
#else
		    ((can_access(SYSTEM_PINERC_FIXED, ACCESS_EXISTS) == 0)
			     ? SYSTEM_PINERC_FIXED : "NO pine.conf.fixed")
#endif
		    );
	    gf_puts(tmp, pc);
	}

	gf_puts(" =======\n", pc);
	for(vars = ps->vars; vars->name; vars++){
	    if(vars->is_list){
		char **t;
		t = (i == 0) ? vars->current_val.l :
		     (i == 1) ? vars->cmdline_val.l :
		      (i == 2) ? vars->main_user_val.l :
		       (i == 3) ? vars->post_user_val.l :
			(i == 4) ? vars->global_val.l
				 : vars->fixed_val.l;
		if(t && *t){
		    snprintf(tmp, sizeof(tmp), " %20.20s : %.*s\n", vars->name,
			    sizeof(tmp)-26, **t ? *t : quotes);
		    gf_puts(tmp, pc);
		    while(++t && *t){
			snprintf(tmp, sizeof(tmp)," %20.20s : %.*s\n","",
				sizeof(tmp)-26, **t ? *t : quotes);
			gf_puts(tmp, pc);
		    }
		}
	    }
	    else{
		char *t;
		t = (i == 0) ? vars->current_val.p :
		     (i == 1) ? vars->cmdline_val.p :
		      (i == 2) ? vars->main_user_val.p :
		       (i == 3) ? vars->post_user_val.p :
		        (i == 4) ? vars->global_val.p
				 : vars->fixed_val.p;
		if(t){
		    snprintf(tmp, sizeof(tmp), " %20.20s : %.*s\n", vars->name,
			    sizeof(tmp)-26, *t ? t : quotes);
		    gf_puts(tmp, pc);
		}
	    }
	}
    }

    if(!brief){
	gf_puts("========== Feature settings ==========\n", pc);
	for(i = 0; (feat = feature_list(i)); i++)
	  if(feat->id != F_OLD_GROWTH){
	      snprintf(tmp, sizeof(tmp),
		      "  %.50s%.100s\n", F_ON(feat->id, ps) ? "   " : "no-",
		      feat->name);
	      gf_puts(tmp, pc);
	  }
    }
}


/*----------------------------------------------------------------------
  Dump interesting variables from the given pine struct
   
  Args: ps -- pine struct to dump 
	pc -- function to use to write the config data

 Result: various important pine struct data written

 ----*/ 
void
dump_pine_struct(struct pine *ps, gf_io_t pc)
{
    char *p;
    extern char term_name[];
    int i;
    MAILSTREAM *m;
    MSGNO_S *msgmap;

    gf_puts("========== struct pine * ==========\n", pc);
    if(!ps){
	gf_puts("!No struct!\n", pc);
	return;
    }

    gf_puts("ui:\tlogin = ", pc);
    gf_puts((ps->ui.login) ? ps->ui.login : "NULL", pc);
    gf_puts(", full = ", pc);
    gf_puts((ps->ui.fullname) ? ps->ui.fullname : "NULL", pc);
    gf_puts("\n\thome = ", pc);
    gf_puts((ps->ui.homedir) ? ps->ui.homedir : "NULL", pc);

    gf_puts("\nhome_dir=\t", pc);
    gf_puts(ps->home_dir ? ps->home_dir : "NULL", pc);
    gf_puts("\nhostname=\t", pc);
    gf_puts(ps->hostname ? ps->hostname : "NULL", pc);
    gf_puts("\nlocaldom=\t", pc);
    gf_puts(ps->localdomain ? ps->localdomain : "NULL", pc);
    gf_puts("\nuserdom=\t", pc);
    gf_puts(ps->userdomain ? ps->userdomain : "NULL", pc);
    gf_puts("\nmaildom=\t", pc);
    gf_puts(ps->maildomain ? ps->maildomain : "NULL", pc);

    gf_puts("\ncur_cntxt=\t", pc);
    gf_puts((ps->context_current && ps->context_current->context)
	    ? ps->context_current->context : "None", pc);
    gf_puts("\ncur_fldr=\t", pc);
    gf_puts(ps->cur_folder, pc);

    gf_puts("\nnstream=\t", pc);
    gf_puts(long2string((long) ps->s_pool.nstream), pc);

    for(i = 0; i < ps->s_pool.nstream; i++){
	m = ps->s_pool.streams[i];
	gf_puts("\nstream slot ", pc);
	gf_puts(long2string((long) i), pc);
	if(!m){
	    gf_puts(" empty", pc);
	    continue;
	}

	if(ps->mail_stream == m)
	  gf_puts("\nThis is the current mailstream", pc);
	if(sp_flagged(m, SP_INBOX))
	  gf_puts("\nThis is the inbox stream", pc);

	gf_puts("\nactual mbox=\t", pc);
	gf_puts(m->mailbox ? m->mailbox : "no mailbox!", pc);

	gf_puts("\nflags=", pc);
	gf_puts(long2string((long) sp_flags(m)), pc);
	gf_puts("\nnew_mail_count=", pc);
	gf_puts(long2string((long) sp_new_mail_count(m)), pc);
	gf_puts("\nmail_since_cmd=", pc);
	gf_puts(long2string((long) sp_mail_since_cmd(m)), pc);
	gf_puts("\nmail_box_changed=", pc);
	gf_puts(long2string((long) sp_mail_box_changed(m)), pc);
	gf_puts("\nunsorted_newmail=", pc);
	gf_puts(long2string((long) sp_unsorted_newmail(m)), pc);
	gf_puts("\nneed_to_rethread=", pc);
	gf_puts(long2string((long) sp_need_to_rethread(m)), pc);
	gf_puts("\nviewing_a_thread=", pc);
	gf_puts(long2string((long) sp_viewing_a_thread(m)), pc);
	gf_puts("\ndead_stream=", pc);
	gf_puts(long2string((long) sp_dead_stream(m)), pc);
	gf_puts("\nnoticed_dead_stream=", pc);
	gf_puts(long2string((long) sp_noticed_dead_stream(m)), pc);
	gf_puts("\nio_error_on_stream=", pc);
	gf_puts(long2string((long) sp_io_error_on_stream(m)), pc);

	msgmap = sp_msgmap(m);
	if(msgmap){
	    gf_puts("\nmsgmap: tot=", pc);
	    gf_puts(long2string(mn_get_total(msgmap)), pc);
	    gf_puts(", cur=", pc);
	    gf_puts(long2string(mn_get_cur(msgmap)), pc);
	    gf_puts(", del=", pc);
	    gf_puts(long2string(count_flagged(m, F_DEL)),pc);
	    gf_puts(", hid=", pc);
	    gf_puts(long2string(any_lflagged(msgmap, MN_HIDE)), pc);
	    gf_puts(", exld=", pc);
	    gf_puts(long2string(any_lflagged(msgmap, MN_EXLD)), pc);
	    gf_puts(", slct=", pc);
	    gf_puts(long2string(any_lflagged(msgmap, MN_SLCT)), pc);
	    gf_puts(", chid=", pc);
	    gf_puts(long2string(any_lflagged(msgmap, MN_CHID)), pc);
	    gf_puts(", coll=", pc);
	    gf_puts(long2string(any_lflagged(msgmap, MN_COLL)), pc);
	    gf_puts(", usor=", pc);
	    gf_puts(long2string(any_lflagged(msgmap, MN_USOR)), pc);
	    gf_puts(", stmp=", pc);
	    gf_puts(long2string(any_lflagged(msgmap, MN_STMP)), pc);
	    gf_puts(", sort=", pc);
	    if(mn_get_revsort(msgmap))
	      gf_puts("rev-", pc);

	    gf_puts(sort_name(mn_get_sort(msgmap)), pc);
	}
	else
	  gf_puts("\nNo msgmap", pc);
    }


    gf_puts("\nterm ", pc);
#if !defined(DOS) && !defined(OS2)
    gf_puts("type=", pc);
    gf_puts(term_name, pc);
    gf_puts(", ttyname=", pc);
    gf_puts((p = (char *)ttyname(0)) ? p : "NONE", pc);
#endif
    gf_puts(", size=", pc);
    gf_puts(int2string(ps->ttyo->screen_rows), pc);
    gf_puts("x", pc);
    gf_puts(int2string(ps->ttyo->screen_cols), pc);
    gf_puts(", speed=", pc);
    gf_puts((ps->low_speed) ? "slow" : "normal", pc);
    gf_puts("\n", pc);
}


void
dump_contexts(void)
{
    int i = 0;
    CONTEXT_S *c = ps_global->context_list;

    if(!(debugfile && debug > 7 && do_debug(debugfile)))
      return;

    while(debugfile && c != NULL){
	fprintf(debugfile, "\n***** context %s\n", c->context);
	if(c->label)
	  fprintf(debugfile,"LABEL: %s\n", c->label);

	if(c->comment)
	  fprintf(debugfile,"COMMENT: %s\n", c->comment);
	
	for(i = 0; i < folder_total(FOLDERS(c)); i++)
	  fprintf(debugfile, "  %d) %s\n", i + 1, folder_entry(i,FOLDERS(c))->name);
	
	c = c->next;
    }
}

#endif /* DEBUG */
