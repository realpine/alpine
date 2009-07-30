#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: remote.c 1032 2008-04-11 00:30:04Z hubert@u.washington.edu $";
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

#include "headers.h"
#include "remote.h"
#include "keymenu.h"
#include "mailview.h"
#include "status.h"
#include "radio.h"
#include "../pith/msgno.h"
#include "../pith/filter.h"
#include "../pith/util.h"
#include "../pith/conf.h"
#include "../pith/tempfile.h"
#include "../pith/margin.h"


/*
 * Internal prototypes
 */
int             rd_answer_forge_warning(int, MSGNO_S *, SCROLL_S *);


int
rd_prompt_about_forged_remote_data(int reason, REMDATA_S *rd, char *extra)
{
    char      tmp[2000];
    char     *unknown = "<unknown>";
    int       rv = -1;
    char *foldertype, *foldername, *special;

    foldertype = (rd && rd->t.i.special_hdr && !strucmp(rd->t.i.special_hdr, REMOTE_ABOOK_SUBTYPE)) ? "address book" : (rd && rd->t.i.special_hdr && !strucmp(rd->t.i.special_hdr, REMOTE_PINERC_SUBTYPE)) ? "configuration" : "data";
    foldername = (rd && rd->rn) ? rd->rn : unknown;
    special = (rd && rd->t.i.special_hdr) ? rd->t.i.special_hdr : unknown;

    dprint((1, "rd_check_out_forged_remote_data:\n"));
    dprint((1, " reason=%d\n", reason));
    dprint((1, " folder_type=%s\n", foldertype ? foldertype : "?"));
    dprint((1, " remotename=%s\n\n", foldername ? foldername : "?"));

    if(rd && rd->flags & USER_SAID_NO)
      return rv;

    if(reason == -2){
	dprint((1, "The special header \"%s\" is missing from the last message in the folder.\nThis indicates that something is wrong.\nYou should probably answer \"No\"\nso that you don't use the corrupt data.\nThen you should investigate further.\n", special ? special : "?"));
    }
    else if(reason == -1){
	dprint((1,  "The last message in the folder contains \"Received\" headers.\nThis usually indicates that the message was put there by the mail\ndelivery system. Alpine does not add those Received headers.\nYou should probably answer \"No\" so that you don't use the corrupt data.\nThen you should investigate further.\n"));
    }
    else if(reason == 0){
	dprint((1, "The special header \"%s\" in the last message\nin the folder has an unexpected value (%s)\nafter it. This could indicate that something is wrong.\nThis value should not normally be put there by Alpine.\nHowever, since there are no Received lines in the message we choose to\nbelieve that everything is ok and we will proceed.\n",
		special ? special : "?", (extra && *extra) ? extra : "?"));
    }
    else if(reason == 1){
	dprint((1, "The special header \"%s\" in the last message\nin the folder has an unexpected value (1)\nafter it. It appears that it may have been put there by an Pine\nwith a version number less than 4.50.\nSince there are no Received lines in the message we choose to believe that\nthe header was added by an old Pine and we will proceed.\n",
		special ? special : "?"));
    }
    else if(reason > 1){
	dprint((1, "The special header \"%s\" in the last message\nin the folder has an unexpected value (%s)\nafter it. This is the right sort of value that Alpine would normally put there,\nbut it doesn't match the value from the first message in the folder.\nThis may indicate that something is wrong.\nHowever, since there are no Received lines in the message we choose to\nbelieve that everything is ok and we will proceed.\n",
		special ? special : "?", (extra && *extra) ? extra : "?"));
    }

    if(reason >= 0){
	/*
	 * This check should not really be here. We have a cookie that
	 * has the wrong value so something is possibly wrong.
	 * But we are worried that old pines will put the bad value in
	 * there (which they will) and then the questions will bother
	 * users and mystify them. So we're just going to pretend the user
	 * said Yes in this case, and we'll try to fix the cookie.
	 * We still catch Received lines and use that or the complete absence
	 * of a special header as indicators of trouble.
	 */
	rd->flags |= USER_SAID_YES;
	return(1);
    }

    if(ps_global->ttyo){
	SCROLL_S  sargs;
	STORE_S  *in_store, *out_store;
	gf_io_t   pc, gc;
	HANDLE_S *handles = NULL;
	int       the_answer = 'n';

	if(!(in_store = so_get(CharStar, NULL, EDIT_ACCESS)) ||
	   !(out_store = so_get(CharStar, NULL, EDIT_ACCESS)))
	  goto try_wantto;

	/* TRANSLATORS: the first %s is the folder type, it may be address book or
	   configuration. The second %s is the folder name. Of course, the HTML
	   tags should be left as is. */
	snprintf(tmp, sizeof(tmp), _("<HTML><P>The data in the remote %s folder<P><CENTER>%s</CENTER><P>looks suspicious. The reason for the suspicion is<P><CENTER>"),
		foldertype, foldername);
	tmp[sizeof(tmp)-1] = '\0';
	so_puts(in_store, tmp);

	if(reason == -2){
	    snprintf(tmp, sizeof(tmp), _("header \"%s\" is missing</CENTER><P>The special header \"%s\" is missing from the last message in the folder. This indicates that something is wrong. You should probably answer \"No\" so that you don't use the corrupt data. Then you should investigate further."),
		    special, special);
	    tmp[sizeof(tmp)-1] = '\0';
	    so_puts(in_store, tmp);
	}
	else if(reason == -1){
	    so_puts(in_store, _("\"Received\" headers detected</CENTER><P>The last message in the folder contains \"Received\" headers. This usually indicates that the message was put there by the mail delivery system. Alpine does not add those Received headers. You should probably answer \"No\" so that you don't use the corrupt data. Then you should investigate further."));
	}
	else if(reason == 0){
	    snprintf(tmp, sizeof(tmp), _("Unexpected value for header \"%s\"</CENTER><P>The special header \"%s\" in the last message in the folder has an unexpected value (%s) after it. This probably indicates that something is wrong. This value would not normally be put there by Alpine. You should probably answer \"No\" so that you don't use the corrupt data. Then you should investigate further."),
		    special, special, (extra && *extra) ? extra : "?");
	    tmp[sizeof(tmp)-1] = '\0';
	    so_puts(in_store, tmp);
	}
	else if(reason == 1){
	    snprintf(tmp, sizeof(tmp), _("Unexpected value for header \"%s\"</CENTER><P>The special header \"%s\" in the last message in the folder has an unexpected value (1) after it. It appears that it may have been put there by a Pine with a version number less than 4.50. If you believe that you have changed this data with an older Pine more recently than you've changed it with this version of Alpine, then you can probably safely answer \"Yes\". If you do not understand why this has happened, you should probably answer \"No\" so that you don't use the corrupt data. Then you should investigate further."),
		    special, special);
	    tmp[sizeof(tmp)-1] = '\0';
	    so_puts(in_store, tmp);
	}
	else if(reason > 1){
	    snprintf(tmp, sizeof(tmp), _("Unexpected value for header \"%s\"</CENTER><P>The special header \"%s\" in the last message in the folder has an unexpected value (%s) after it. This is the right sort of value that Alpine would normally put there, but it doesn't match the value from the first message in the folder. This may indicate that something is wrong. Unless you understand why this has happened, you should probably answer \"No\" so that you don't use the corrupt data. Then you should investigate further."),
		    special, special, (extra && *extra) ? extra : "?");
	    tmp[sizeof(tmp)-1] = '\0';
	    so_puts(in_store, tmp);
	}

	so_seek(in_store, 0L, 0);
	init_handles(&handles);
	gf_filter_init();
	gf_link_filter(gf_html2plain,
		       gf_html2plain_opt(NULL,
					 ps_global->ttyo->screen_cols, non_messageview_margin(),
					 &handles, NULL, GFHP_LOCAL_HANDLES));
	gf_set_so_readc(&gc, in_store);
	gf_set_so_writec(&pc, out_store);
	gf_pipe(gc, pc);
	gf_clear_so_writec(out_store);
	gf_clear_so_readc(in_store);

	memset(&sargs, 0, sizeof(SCROLL_S));
	sargs.text.handles  = handles;
	sargs.text.text     = so_text(out_store);
	sargs.text.src      = CharStar;
	sargs.bar.title     = _("REMOTE DATA FORGERY WARNING");
	sargs.proc.tool     = rd_answer_forge_warning;
	sargs.proc.data.p   = (void *)&the_answer;
	sargs.keys.menu     = &forge_keymenu;
	setbitmap(sargs.keys.bitmap);

	scrolltool(&sargs);

	if(the_answer == 'y'){
	    rv = 1;
	    rd->flags |= USER_SAID_YES;
	}
	else if(rd)
	  rd->flags |= USER_SAID_NO;

	ps_global->mangled_screen = 1;
	ps_global->painted_body_on_startup = 0;
	ps_global->painted_footer_on_startup = 0;
	so_give(&in_store);
	so_give(&out_store);
	free_handles(&handles);
    }
    else{
	char *p = tmp;

	snprintf(p, sizeof(tmp), _("\nThe data in the remote %s folder\n\n   %s\n\nlooks suspicious. The reason for the suspicion is\n\n   "),
		foldertype, foldername);
	tmp[sizeof(tmp)-1] = '\0';
	p += strlen(p);

	if(reason == -2){
	    snprintf(p, sizeof(tmp)-(p-tmp), _("header \"%s\" is missing\n\nThe special header \"%s\" is missing from the last message\nin the folder. This indicates that something is wrong.\nYou should probably answer \"No\" so that you don't use the corrupt data.\nThen you should investigate further.\n\n"),
		    special, special);
	    tmp[sizeof(tmp)-1] = '\0';
	}
	else if(reason == -1){
	    snprintf(p, sizeof(tmp)-(p-tmp), _("\"Received\" headers detected\n\nThe last message in the folder contains \"Received\" headers.\nThis usually indicates that the message was put there by the\nmail delivery system. Alpine does not add those Received headers.\nYou should probably answer \"No\" so that you don't use the corrupt data.\nThen you should investigate further.\n\n"));
	    tmp[sizeof(tmp)-1] = '\0';
	}
	else if(reason == 0){
	    snprintf(p, sizeof(tmp)-(p-tmp), _("Unexpected value for header \"%s\"\n\nThe special header \"%s\" in the last message in the folder\nhas an unexpected value (%s) after it. This probably\nindicates that something is wrong. This value would not normally be put\nthere by Alpine. You should probably answer \"No\" so that you don't use\nthe corrupt data. Then you should investigate further.\n\n"),
		    special, special, (extra && *extra) ? extra : "?");
	    tmp[sizeof(tmp)-1] = '\0';
	}
	else if(reason == 1){
	    snprintf(p, sizeof(tmp)-(p-tmp), _("Unexpected value for header \"%s\"\n\nThe special header \"%s\" in the last message in the folder\nhas an unexpected value (1) after it. It appears that it may have been\nput there by a Pine with a version number less than 4.50.\nIf you believe that you have changed this data with an older Pine more\nrecently than you've changed it with this version of Alpine, then you can\nprobably safely answer \"Yes\". If you do not understand why this has\nhappened, you should probably answer \"No\" so that you don't use the\ncorrupt data. Then you should investigate further.\n\n"),
		    special, special);
	    tmp[sizeof(tmp)-1] = '\0';
	}
	else if(reason > 1){
	    snprintf(p, sizeof(tmp)-(p-tmp), _("Unexpected value for header \"%s\"\n\nThe special header \"%s\" in the last message in the folder\nhas an unexpected\nvalue (%s) after it. This is\nthe right sort of value that Alpine would normally put there, but it\ndoesn't match the value from the first message in the folder. This may\nindicate that something is wrong. Unless you understand why this has happened,\nyou should probably answer \"No\" so that you don't use the\ncorrupt data. Then you should investigate further.\n\n"),
		    special, special, (extra && *extra) ? extra : "?");
	    tmp[sizeof(tmp)-1] = '\0';
	}

try_wantto:
	p += strlen(p);
	snprintf(p, sizeof(tmp)-(p-tmp), _("Suspicious data in \"%s\": Continue anyway "),
		(rd && rd->t.i.special_hdr) ? rd->t.i.special_hdr
					    : unknown);
	tmp[sizeof(tmp)-1] = '\0';
	if(want_to(tmp, 'n', 'x', NO_HELP, WT_NORM) == 'y'){
	    rv = 1;
	    rd->flags |= USER_SAID_YES;
	}
	else if(rd)
	  rd->flags |= USER_SAID_NO;
    }

    if(rv < 0)
      q_status_message1(SM_ORDER, 1, 3, _("Can't open remote %s"),
			(rd && rd->rn) ? rd->rn : "<noname>");

    return(rv);
}


int
rd_answer_forge_warning(int cmd, MSGNO_S *msgmap, SCROLL_S *sparms)
{
    int rv = 1;

    ps_global->next_screen = SCREEN_FUN_NULL;

    switch(cmd){
      case MC_YES :
	*(int *)(sparms->proc.data.p) = 'y';
	break;

      case MC_NO :
	*(int *)(sparms->proc.data.p) = 'n';
	break;

      default:
	panic("Unexpected command in rd_answer_forge_warning");
	break;
    }

    return(rv);
}



char *
rd_metadata_name(void)
{
    char        *p, *q, *metafile;
    char         path[MAXPATH], pinerc_dir[MAXPATH];
    struct variable *vars = ps_global->vars;

    dprint((9, "rd_metadata_name\n"));

    pinerc_dir[0] = '\0';
    if(ps_global->pinerc){
	char *prcn = ps_global->pinerc;
	char *lc;

	if((lc = last_cmpnt(prcn)) != NULL){
	    int to_copy;

	    to_copy = (lc - prcn > 1) ? (lc - prcn - 1) : 1;
	    strncpy(pinerc_dir, prcn, MIN(to_copy, sizeof(pinerc_dir)-1));
	    pinerc_dir[MIN(to_copy, sizeof(pinerc_dir)-1)] = '\0';
	}
	else{
	    pinerc_dir[0] = '.';
	    pinerc_dir[1] = '\0';
	}
    }

    /*
     * If there is no metadata file specified in the pinerc, create a filename.
     */
    if(!(VAR_REMOTE_ABOOK_METADATA && VAR_REMOTE_ABOOK_METADATA[0])){
	if(pinerc_dir[0] && (p = tempfile_in_same_dir(ps_global->pinerc,
						      meta_prefix, NULL))){
	    /* fill in the pinerc variable */
	    q = p + strlen(pinerc_dir) + 1;
	    set_variable(V_REMOTE_ABOOK_METADATA, q, 1, 0, Main);
	    dprint((2, "creating name for metadata file: %s\n",
		   q ? q : "?"));

	    /* something's broken, return NULL rab */
	    if(!VAR_REMOTE_ABOOK_METADATA || !VAR_REMOTE_ABOOK_METADATA[0]){
		our_unlink(p);
		fs_give((void **)&p);
		return(NULL);
	    }

	    fs_give((void **)&p);
	}
	else{
	    q_status_message(SM_ORDER, 3, 5,
		"can't create metadata file in pinerc directory, continuing");
	    return(NULL);
	}
    }

    build_path(path, pinerc_dir ? pinerc_dir : NULL,
	       VAR_REMOTE_ABOOK_METADATA, sizeof(path));
    metafile = path;

    /*
     * If the metadata file doesn't exist, create it.
     */
    if(can_access(metafile, ACCESS_EXISTS) != 0){
	int fd;

	if((fd = our_open(metafile, O_CREAT|O_EXCL|O_WRONLY|O_BINARY, 0600)) < 0){

	    set_variable(V_REMOTE_ABOOK_METADATA, NULL, 1, 0, Main);

	    q_status_message2(SM_ORDER, 3, 5,
		       "can't create cache file %.200s, continuing (%.200s)",
		       metafile, error_description(errno));

	    dprint((2, "can't create metafile %s: %s\n",
		       metafile ? metafile : "?", error_description(errno)));

	    return(NULL);
	}

	dprint((2, "created metadata file: %s\n",
	       metafile ? metafile : "?"));

	(void)close(fd);
    }

    return(cpystr(metafile));;
}

