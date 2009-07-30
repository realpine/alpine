#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: file.c 1082 2008-06-12 18:39:50Z hubert@u.washington.edu $";
#endif

/*
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
 *
 * Program:	High level file input and output routines
 *
 */

/*
 * The routines in this file
 * handle the reading and writing of
 * disk files. All of details about the
 * reading and writing of the disk are
 * in "fileio.c".
 */
#include	"headers.h"
#include	"../pith/charconv/filesys.h"


void  init_insmsgchar_cbuf(void);
int   insmsgchar(int);
char *file_split(char *, size_t, char *, int);

/*
 * Read a file into the current
 * buffer. This is really easy; all you do it
 * find the name of the file, and call the standard
 * "read a file into the current buffer" code.
 * Bound to "C-X C-R".
 */
int
fileread(int f, int n)
{
        register int    s;
        char fname[NFILEN];
	EML eml;

        if ((s=mlreply_utf8(_("Read file: "), fname, NFILEN, QNORML, NULL)) != TRUE)
                return(s);

	if(gmode&MDSCUR){
	    emlwrite(_("File reading disabled in secure mode"),NULL);
	    return(0);
	}

	if (strlen(fname) == 0) {
	  emlwrite(_("No file name entered"),NULL);
	  return(0);
	}

	if((gmode & MDTREE) && !in_oper_tree(fname)){
	  eml.s = opertree;
	  emlwrite(_("Can't read file from outside of %s"), &eml);
	  return(0);
	}

        return(readin(fname, TRUE, TRUE));
}



static char *inshelptext[] = {
  /* TRANSLATORS: several lines of help text */
  N_("Insert File Help Text"),
  " ",
  N_("        Type in a file name to have it inserted into your editing"),
  N_("        buffer between the line that the cursor is currently on"),
  N_("        and the line directly below it.  You may abort this by "),
  N_("~        typing the ~F~2 (~^~C) key after exiting help."),
  " ",
  N_("End of Insert File Help"),
  " ",
  NULL
};

static char *writehelp[] = {
  /* TRANSLATORS: several lines of help text */
  N_("Write File Help Text"),
  " ",
  N_("        Type in a file name to have it written out, thus saving"),
  N_("        your buffer, to a file.  You can abort this by typing "),
  N_("~        the ~F~2 (~^~C) key after exiting help."),
  " ",
  N_("End of Write File Help"),
  " ",
  " ",
  NULL
};


/*
 * Insert a file into the current
 * buffer. This is really easy; all you do it
 * find the name of the file, and call the standard
 * "insert a file into the current buffer" code.
 * Bound to "C-X C-I".
 */
int
insfile(int f, int n)
{
    register int s;
    char	 fname[NLINE], dir[NLINE];
    int		 retval, bye = 0, msg = 0;
    char	 prompt[64], *infile;
    EXTRAKEYS    menu_ins[5];
    EML          eml;
    
    if (curbp->b_mode&MDVIEW) /* don't allow this command if */
      return(rdonly()); /* we are in read only mode */

    fname[0] = dir[0] = '\0';
    while(!bye){
	/* set up keymenu stuff */
	if(!msg){
	    int last_menu = 0;

	    menu_ins[last_menu].name  = "^T";
	    menu_ins[last_menu].key   = (CTRL|'T');
	    menu_ins[last_menu].label = N_("To Files");
	    KS_OSDATASET(&menu_ins[last_menu], KS_NONE);

	    if(Pmaster && Pmaster->msgntext){
		menu_ins[++last_menu].name  = "^W";
		menu_ins[last_menu].key     = (CTRL|'W');
		/* TRANSLATORS: Insert File is a key label for a command
		   that inserts a file into a message being composed.
		   InsertMsg means Insert Message and it inserts a different
		   message into the message being composed. */
		menu_ins[last_menu].label   = msg ? N_("InsertFile") : N_("InsertMsg");
		KS_OSDATASET(&menu_ins[last_menu], KS_NONE);
	    }

#if	!defined(DOS) && !defined(MAC)
	    if(Pmaster && Pmaster->upload){
		menu_ins[++last_menu].name = "^Y";
		menu_ins[last_menu].key    = (CTRL|'Y');
		menu_ins[last_menu].label  = "RcvUpload";
		KS_OSDATASET(&menu_ins[last_menu], KS_NONE);
	    }
#endif	/* !(DOS || MAC) */

	    if(gmode & MDCMPLT){
		menu_ins[++last_menu].name = msg ? "" : "TAB";
		menu_ins[last_menu].key    = (CTRL|'I');
		menu_ins[last_menu].label  = msg ? "" : N_("Complete");
		KS_OSDATASET(&menu_ins[last_menu], KS_NONE);
	    }

	    menu_ins[++last_menu].name = NULL;
	}

	snprintf(prompt, sizeof(prompt), "%s to insert from %s %s: ",
		msg ? "Number of message" : "File",
		(msg || (gmode&MDCURDIR))
		  ? "current"
		  : ((gmode & MDTREE) || opertree[0]) ? opertree : "home",
		msg ? "folder" : "directory");
	s = mlreplyd_utf8(prompt, fname, NLINE, QDEFLT, msg ? NULL : menu_ins);
	/* something to read and it was edited or the default accepted */
        if(fname[0] && (s == TRUE || s == FALSE)){
	    bye++;
	    if(msg){
		init_insmsgchar_cbuf();
		if((*Pmaster->msgntext)(atol(fname), insmsgchar)){
		    eml.s = fname;
		    emlwrite(_("Message %s included"), &eml);
		}
	    }
	    else{
		bye++;
		if(gmode&MDSCUR){
		    emlwrite(_("Can't insert file in restricted mode"),NULL);
		}
		else{
		    if((gmode & MDTREE)
		       && !compresspath(opertree, fname, sizeof(fname))){
			eml.s = opertree;
			emlwrite(
			_("Can't insert file from outside of %s: too many ..'s"), &eml);
		    }
		    else{
			fixpath(fname, sizeof(fname));

			if((gmode & MDTREE) && !in_oper_tree(fname)){
			    eml.s = opertree;
			    emlwrite(_("Can't insert file from outside of %s"), &eml);
			}
			else
			  retval = ifile(fname);
		    }
		}
	    }
	}
	else{
	    switch(s){
	      case (CTRL|'I') :
		{
		    char *fn;

		    dir[0] = '\0';
		    fn = file_split(dir, sizeof(dir), fname, 0);

		    if(!pico_fncomplete(dir, fn, sizeof(fname)-(fn-fname)))
		      (*term.t_beep)();
		}

		break;
	      case (CTRL|'W'):
		msg = !msg;			/* toggle what to insert */
		break;
	      case (CTRL|'T'):
		if(msg){
		    emlwrite(_("Can't select messages yet!"), NULL);
		}
		else{
		    char *fn;

		    fn = file_split(dir, sizeof(dir), fname, 1);
		    if(!isdir(dir, NULL, NULL)){
		      strncpy(dir, (gmode&MDCURDIR)
			     ? (browse_dir[0] ? browse_dir : ".")
			     : ((gmode & MDTREE) || opertree[0])
			     ? opertree
			     : (browse_dir[0] ? browse_dir
				:gethomedir(NULL)), sizeof(dir));
		      dir[sizeof(dir)-1] = '\0';
		    }
		    else{
			if(*fn){
			    int dirlen;

			    dirlen = strlen(dir);
			    if(dirlen && dir[dirlen - 1] != C_FILESEP){
			      strncat(dir, S_FILESEP, sizeof(dir)-strlen(dir)-1);
			      dir[sizeof(dir)-1] = '\0';
			    }

			    strncat(dir, fn, sizeof(dir)-strlen(dir)-1);
			    dir[sizeof(dir)-1] = '\0';
			    if(!isdir(dir, NULL, NULL))
			      dir[MIN(dirlen,sizeof(dir)-1)] = '\0';
			}
		    }

		    fname[0] = '\0';
		    if((s = FileBrowse(dir, sizeof(dir), fname, sizeof(fname), 
				       NULL, 0, FB_READ, NULL)) == 1){
			if(gmode&MDSCUR){
			    emlwrite(_("Can't insert in restricted mode"),
				     NULL);
			    sleep(2);
			}
			else{
			    size_t len;

			    len = strlen(dir)+strlen(S_FILESEP)+strlen(fname);
			    if ((infile = (char *)malloc((len+1)*sizeof(char))) != NULL){
			      strncpy(infile, dir, len);
			      infile[len] = '\0';
			      strncat(infile, S_FILESEP, len+1-1-strlen(infile));
			      infile[len] = '\0';
			      strncat(infile, fname, len+1-1-strlen(infile));
			      infile[len] = '\0';
			      retval = ifile(infile);
			      free((char *) infile);
			    }
			    else {
			      emlwrite("Trouble allocating space for insert!"
				       ,NULL);
			      sleep(3);
			    }
			}
			bye++;
		    }
		    else
		      fname[0] = '\0';

		    pico_refresh(FALSE, 1);
		    if(s != 1){
			update(); 		/* redraw on return */
			continue;
		    }
		}

		break;

#if	!defined(DOS) && !defined(MAC)
	      case (CTRL|'Y') :
		if(Pmaster && Pmaster->upload){
		    char tfname[NLINE];

		    if(gmode&MDSCUR){
			emlwrite(
			      _("\007Restricted mode disallows uploaded command"),
			      NULL);
			return(0);
		    }

		    tfname[0] = '\0';
		    retval = (*Pmaster->upload)(tfname, sizeof(tfname), NULL);

		    pico_refresh(FALSE, 1);
		    update();

		    if(retval){
			retval = ifile(tfname);
			bye++;
		    }
		    else
		      sleep(3);			/* problem, show error! */

		    if(tfname[0])		/* clean up temp file */
		      our_unlink(tfname);
		}
		else
		  (*term.t_beep)();		/* what? */

		break;
#endif	/* !(DOS || MAC) */
	      case HELPCH:
		if(Pmaster){
		    VARS_TO_SAVE *saved_state;

		    saved_state = save_pico_state();
		    (*Pmaster->helper)(msg ? Pmaster->ins_m_help
					   : Pmaster->ins_help,
				       _("Help for Insert File"), 1);
		    if(saved_state){
			restore_pico_state(saved_state);
			free_pico_state(saved_state);
		    }
		}
		else
		  pico_help(inshelptext, _("Help for Insert File"), 1);
	      case (CTRL|'L'):
		pico_refresh(FALSE, 1);
		update();
		continue;
	      default:
		ctrlg(FALSE, 0);
                retval = s;
		bye++;
	    }
        }
    }
    curwp->w_flag |= WFMODE|WFHARD;
    
    return(retval);
}

/* 
 * split out the file name from the path.
 * Copy path into dirbuf, return filename,
 * which is a pointer into orig_fname.
 *   is_for_browse - use browse dir if possible
 *      don't want to use it for TAB-completion
 */
char *
file_split(char *dirbuf, size_t dirbuflen, char *orig_fname, int is_for_browse)
{
    char *p, *fn;
    size_t dirlen;

    if(*orig_fname && (p = strrchr(orig_fname, C_FILESEP))){
	fn = p + 1;
	dirlen = p - orig_fname;
	if(p == orig_fname){
	    strncpy(dirbuf, S_FILESEP, dirbuflen);
	    dirbuf[dirbuflen-1] = '\0';
	}
#ifdef	DOS
	else if(orig_fname[0] == C_FILESEP
		|| (isalpha((unsigned char)orig_fname[0])
		    && orig_fname[1] == ':')){
	    if(orig_fname[1] == ':' && p == orig_fname+2)
	      dirlen = fn - orig_fname;

	    dirlen = MIN(dirlen, dirbuflen-1);
	    strncpy(dirbuf, orig_fname, dirlen);
	    dirbuf[dirlen] = '\0';
	}
#else
	else if (orig_fname[0] == C_FILESEP || orig_fname[0] == '~'){
	    dirlen = MIN(dirlen, dirbuflen-1);
	    strncpy(dirbuf, orig_fname, dirlen);
	    dirbuf[dirlen] = '\0';
	}
#endif
	else
	  snprintf(dirbuf, dirbuflen, "%s%c%.*s", 
		  (gmode & MDCURDIR)
		  ? ((is_for_browse && browse_dir[0]) ? browse_dir : ".")
		  : ((gmode & MDTREE) || opertree[0])
		  ? opertree
		  : ((is_for_browse && browse_dir[0])
		     ? browse_dir : gethomedir(NULL)),
		  C_FILESEP, p - orig_fname, orig_fname);
    }
    else{
	fn = orig_fname;
	strncpy(dirbuf, (gmode & MDCURDIR)
	       ? ((is_for_browse && browse_dir[0]) ? browse_dir : ".")
	       : ((gmode & MDTREE) || opertree[0])
	       ? opertree
	       : ((is_for_browse && browse_dir[0])
		  ? browse_dir : gethomedir(NULL)), dirbuflen);
	dirbuf[dirbuflen-1] = '\0';
    }

    return fn;
}


static CBUF_S insmsgchar_cb;

void
init_insmsgchar_cbuf(void)
{
    insmsgchar_cb.cbuf[0] = '\0'; 
    insmsgchar_cb.cbufp = insmsgchar_cb.cbuf;
    insmsgchar_cb.cbufend = insmsgchar_cb.cbuf;
}


/*
 * This is called with a stream of UTF-8 characters.
 * We change that stream into UCS characters and insert
 * those characters.
 */
int
insmsgchar(int c)
{
    if(c == '\n'){
	UCS *u;

	lnewline();
	for(u = glo_quote_str; u && *u; u++)
	  if(!linsert(1, *u))
	    return(0);
    }
    else if(c != '\r'){			/* ignore CR (likely CR of CRLF) */
	int outchars;
	UCS ucs;

	if((outchars = utf8_to_ucs4_oneatatime(c, &insmsgchar_cb, &ucs, NULL)) != 0)
	  if(!linsert(1, ucs))
	    return(0);
    }

    return(1);
}


/*
 * Read file "fname" into the current
 * buffer, blowing away any text found there. Called
 * by both the read and find commands. Return the final
 * status of the read. Also called by the mainline,
 * to read in a file specified on the command line as
 * an argument.
 */
int
readin(char fname[],		/* name of file to read */
       int lockfl,		/* check for file locks? */
       int rename)		/* don't rename if reading from, say, alt speller */
{
	UCS  line[NLINE], *linep;
	long nline;
	int  s, done, newline;

	curbp->b_linecnt = -1;			/* Must be recalculated */
	if ((s = bclear(curbp)) != TRUE)	/* Might be old.        */
	  return (s);

	if(rename){
	  strncpy(curbp->b_fname, fname, sizeof(curbp->b_fname));
	  curbp->b_fname[sizeof(curbp->b_fname)-1] = '\0';
	}

	if ((s=ffropen(fname)) != FIOSUC){	/* Hard file open.      */
	    if(s == FIOFNF)                     /* File not found.      */
	      emlwrite(_("New file"), NULL);
	    else
	      fioperr(s, fname);
	}
	else{
	    size_t charsread = 0;

	    emlwrite(_("Reading file"), NULL);
	    nline = 0L;
	    done  = newline = 0;
	    while(!done)
	      if((s = ffgetline(line, NLINE, &charsread, 1)) == FIOEOF){
		  char b[200];

		  curbp->b_flag &= ~(BFTEMP|BFCHG);
		  gotobob(FALSE, 1);
		  if(nline > 1)
		    snprintf(b, sizeof(b), _("Read %ld lines"), nline);
		  else
		    snprintf(b, sizeof(b), _("Read 1 line"));

		  emlwrite(b, NULL);

		  break;
	      }
	      else{
		  if(newline){
		      lnewline();
		      newline = 0;
		  }

		  switch(s){
		    case FIOSUC :
		      nline++;
		      newline = 1;

		    case FIOLNG :
		      for(linep = line; charsread-- > 0; linep++)
			linsert(1, *linep);

		      break;

		    default :
		      done++;
		      break;
		  }
	      }

	    ffclose();                              /* Ignore errors.       */
	}

	return(s != FIOERR && s != FIOFNF);		/* true if success */
}


/*
 * Ask for a file name, and write the
 * contents of the current buffer to that file.
 * Update the remembered file name and clear the
 * buffer changed flag. This handling of file names
 * is different from the earlier versions, and
 * is more compatable with Gosling EMACS than
 * with ITS EMACS. Bound to "C-X C-W".
 */
int
filewrite(int f, int n)
{
        register WINDOW *wp;
        register int    s;
        char            fname[NFILEN];
	char		shows[NLINE], origshows[NLINE], *bufp;
	EXTRAKEYS	menu_write[3];
	EML             eml;

	if(curbp->b_fname[0] != 0){
	  strncpy(fname, curbp->b_fname, sizeof(curbp->b_fname));
	  curbp->b_fname[sizeof(curbp->b_fname)-1] = '\0';
	}
	else
	  fname[0] = '\0';

	menu_write[0].name  = "^T";
	menu_write[0].label = N_("To Files");
	menu_write[0].key   = (CTRL|'T');
	menu_write[1].name  = "TAB";
	menu_write[1].label = N_("Complete");
	menu_write[1].key   = (CTRL|'I');
	menu_write[2].name  = NULL;
	for(;!(gmode & MDTOOL);){
	    /* TRANSLATORS: Asking for name of file to write data into */
	    s = mlreplyd_utf8(_("File Name to write : "), fname, NFILEN,
			 QDEFLT|QFFILE, menu_write);

	    switch(s){
	      case FALSE:
		if(!fname[0]){			/* no file name to write to */
		    ctrlg(FALSE, 0);
		    return(s);
		}
	      case TRUE:
		if((gmode & MDTREE) && !compresspath(opertree, fname, sizeof(fname))){
		    eml.s = opertree;
		    emlwrite(_("Can't write outside of %s: too many ..'s"), &eml);
		    sleep(2);
		    continue;
		}
		else{
		    fixpath(fname, sizeof(fname));	/*  fixup ~ in file name  */
		    if((gmode & MDTREE) && !in_oper_tree(fname)){
			eml.s = opertree;
			emlwrite(_("Can't write outside of %s"), &eml);
			sleep(2);
			continue;
		    }
		}

		break;

	      case (CTRL|'I'):
		{
		    char *fn, *p, dir[NFILEN];
		    int   l = NFILEN;

		    dir[0] = '\0';
		    if(*fname && (p = strrchr(fname, C_FILESEP))){
			fn = p + 1;
			l -= fn - fname;
			if(p == fname){
			  strncpy(dir, S_FILESEP, sizeof(dir));
			  dir[sizeof(dir)-1] = '\0';
			}
#ifdef	DOS
			else if(fname[0] == C_FILESEP
				 || (isalpha((unsigned char)fname[0])
				     && fname[1] == ':'))
#else
			else if (fname[0] == C_FILESEP || fname[0] == '~')
#endif
			{
			    strncpy(dir, fname, MIN(p - fname, sizeof(dir)-1));
			    dir[MIN(p-fname, sizeof(dir)-1)] = '\0';
			}
			else
			  snprintf(dir, sizeof(dir), "%s%c%.*s", 
				  (gmode & MDCURDIR)
				     ? "."
				     : ((gmode & MDTREE) || opertree[0])
					  ? opertree : gethomedir(NULL),
				  C_FILESEP, p - fname, fname);
		    }
		    else{
			fn = fname;
			strncpy(dir, (gmode & MDCURDIR)
				       ? "."
				       : ((gmode & MDTREE) || opertree[0])
					    ? opertree : gethomedir(NULL), sizeof(dir));
			dir[sizeof(dir)-1] = '\0';
		    }

		    if(!pico_fncomplete(dir, fn, sizeof(fname)-(fn-fname)))
		      (*term.t_beep)();
		}

		continue;

	      case (CTRL|'T'):
		/* If we have a file name, break up into path and file name.*/
		*shows = 0;
		if(*fname) {
		    if(isdir(fname, NULL, NULL)) {
			/* fname is a directory. */
			strncpy(shows, fname, sizeof(shows));
			shows[sizeof(shows)-1] = '\0';
			*fname = '\0';
		    }
		    else {
			/* Find right most separator. */
			bufp = strrchr (fname, C_FILESEP);
			if (bufp != NULL) {
			    /* Copy directory part to 'shows', and file
			     * name part to front of 'fname'. */
			    *bufp = '\0';
			    strncpy(shows, fname, sizeof(shows));
			    shows[sizeof(shows)-1] = '\0';
			    strncpy(fname, bufp+1, MIN(strlen(bufp+1)+1, sizeof(fname)));
			    fname[sizeof(fname)-1] = '\0';
			}
		    }
		}

		/* If we did not end up with a valid directory, use home. */
		if (!*shows || !isdir (shows, NULL, NULL)){
		  strncpy(shows, ((gmode & MDTREE) || opertree[0])
			 ? opertree
			 : (browse_dir[0] ? browse_dir
			    : gethomedir(NULL)), sizeof(shows));
		  shows[sizeof(shows)-1] = '\0';
		}

		strncpy(origshows, shows, sizeof(origshows));
		origshows[sizeof(origshows)-1] = '\0';
		if ((s = FileBrowse(shows, sizeof(shows), fname, sizeof(fname), NULL, 0,
				    FB_SAVE, NULL)) == 1) {
		  if (strlen(shows)+strlen(S_FILESEP)+strlen(fname) < NLINE){
		    strncat(shows, S_FILESEP, sizeof(shows)-strlen(shows)-1);
		    shows[sizeof(shows)-1] = '\0';
		    strncat(shows, fname, sizeof(shows)-strlen(shows)-1);
		    shows[sizeof(shows)-1] = '\0';
		    strncpy(fname, shows, sizeof(fname));
		    fname[sizeof(fname)-1] = '\0';
		  }
		  else {
		    emlwrite("Cannot write. File name too long!!",NULL);
		    sleep(3);
		  }
		}
		else if (s == 0 && strcmp(shows, origshows)){
		    strncat(shows, S_FILESEP, sizeof(shows)-strlen(shows)-1);
		    shows[sizeof(shows)-1] = '\0';
		    strncat(shows, fname, sizeof(shows)-strlen(shows)-1);
		    shows[sizeof(shows)-1] = '\0';
		    strncpy(fname, shows, sizeof(fname));
		    fname[sizeof(fname)-1] = '\0';
		}
		else if (s == -1){
		  emlwrite("Cannot write. File name too long!!",NULL);
		  sleep(3);
		}

		pico_refresh(FALSE, 1);
		update();
		if(s == 1)
		  break;
		else
		  continue;

	      case HELPCH:
		pico_help(writehelp, "", 1);
	      case (CTRL|'L'):
		pico_refresh(FALSE, 1);
		update();
		continue;

	      default:
		return(s);
		break;
	    }

	    if(strcmp(fname, curbp->b_fname) == 0)
		break;

	    if((s=fexist(fname, "w", (off_t *)NULL)) == FIOSUC){
						    /*exists overwrite? */

		snprintf(shows, sizeof(shows), _("File \"%s\" exists, OVERWRITE"), fname);
		if((s=mlyesno_utf8(shows, FALSE)) == TRUE)
		  break;
	    }
	    else if(s == FIOFNF){
		break;				/* go write it */
	    }
	    else{				/* some error, can't write */
		fioperr(s, fname);
		return(ABORT);
	    }
	}

	emlwrite("Writing...", NULL);

        if ((s=writeout(fname, 0)) != -1) {
	        if(!(gmode&MDTOOL)){
		    strncpy(curbp->b_fname, fname, sizeof(curbp->b_fname));
		    curbp->b_fname[sizeof(curbp->b_fname)-1] = '\0';
		    curbp->b_flag &= ~BFCHG;

		    wp = wheadp;                    /* Update mode lines.   */
		    while (wp != NULL) {
                        if (wp->w_bufp == curbp)
			    if((Pmaster && s == TRUE) || Pmaster == NULL)
                                wp->w_flag |= WFMODE;
                        wp = wp->w_wndp;
		    }
		}

		if(s > 1){
		    eml.s = comatose(s);
		    emlwrite(_("Wrote %s lines"), &eml);
		}
		else
		  emlwrite(_("Wrote 1 line"), NULL);
        }

        return ((s == -1) ? FALSE : TRUE);
}


/*
 * Save the contents of the current
 * buffer in its associatd file. No nothing
 * if nothing has changed (this may be a bug, not a
 * feature). Error if there is no remembered file
 * name for the buffer. Bound to "C-X C-S". May
 * get called by "C-Z".
 */
int
filesave(int f, int n)
{
        register WINDOW *wp;
        register int    s;
	EML eml;

	if (curbp->b_mode&MDVIEW)	/* don't allow this command if	*/
		return(rdonly());	/* we are in read only mode	*/
        if ((curbp->b_flag&BFCHG) == 0)         /* Return, no changes.  */
                return (TRUE);
        if (curbp->b_fname[0] == 0) {           /* Must have a name.    */
                emlwrite(_("No file name"), NULL);
		sleep(2);
                return (FALSE);
        }

	emlwrite("Writing...", NULL);
        if ((s=writeout(curbp->b_fname, 0)) != -1) {
                curbp->b_flag &= ~BFCHG;
                wp = wheadp;                    /* Update mode lines.   */
                while (wp != NULL) {
                        if (wp->w_bufp == curbp)
			  if(Pmaster == NULL)
                                wp->w_flag |= WFMODE;
                        wp = wp->w_wndp;
                }
		if(s > 1){
		    eml.s = comatose(s);
		    emlwrite(_("Wrote %s lines"), &eml);
		}
		else
		  emlwrite(_("Wrote 1 line"), NULL);
        }
        return (s);
}


/*
 * This function performs the details of file
 * writing. Uses the file management routines in the
 * "fileio.c" package. The number of lines written is
 * displayed. Sadly, it looks inside a LINE; provide
 * a macro for this. Most of the grief is error
 * checking of some sort.
 *
 * If the argument readonly is set, the file is created with user read
 * and write permission only if it doesn't already exist. Note that the
 * word readonly is misleading. It is user r/w permission only.
 *
 * CHANGES: 1 Aug 91: returns number of lines written or -1 on error, MSS
 */
int
writeout(char *fn, int readonly)
{
        register int    s;
        register int    t;
        register LINE   *lp;
        register int    nline;

        if (!((s = ffwopen(fn, readonly)) == FIOSUC && ffelbowroom()))
	  return (-1);				/* Open writes message. */

        lp = lforw(curbp->b_linep);             /* First line.          */
        nline = 0;                              /* Number of lines.     */
        while (lp != curbp->b_linep) {
                if ((s=ffputline(&lp->l_text[0], llength(lp))) != FIOSUC)
                        break;
                ++nline;
                lp = lforw(lp);
        }

	t = ffclose();			/* ffclose complains if error */

	if(s == FIOSUC)
	  s = t;				/* report worst case */

	return ((s == FIOSUC) ? nline : -1);
}


/*
 * writetmp - write a temporary file for message text, mindful of 
 *	      access restrictions and included text.  If n is true, include
 *	      lines that indicated included message text, otw forget them
 *            If dir is non-null, put the temp file in that directory.
 */
char *
writetmp(int n, char *dir)
{
        static   char	fn[NFILEN];
        register int    s;
        register int    t;
        register LINE   *lp;
        register int    nline;

        tmpname(dir, fn);

	/* Open writes message */
        if (!fn[0] || (s=ffwopen(fn, TRUE)) != FIOSUC){
	    if(fn[0])
	      our_unlink(fn);

	    return(NULL);
	}

        lp = lforw(curbp->b_linep);             /* First line.          */
        nline = 0;                              /* Number of lines.     */
        while (lp != curbp->b_linep) {
	    if(n || (!n && lp->l_text[0].c != '>'))
	      if ((s=ffputline(&lp->l_text[0], llength(lp))) != FIOSUC)
		break;

	    ++nline;
	    lp = lforw(lp);
        }

	t = ffclose();			/* ffclose complains if error */

	if(s == FIOSUC)
	  s = t;				/* remember worst case */

	if (s != FIOSUC){                       /* Some sort of error.  */
	    our_unlink(fn);
	    return(NULL);
	}

	return(fn);
}


/*
 * Insert file "fname" into the current
 * buffer, Called by insert file command. Return the final
 * status of the read.
 */
int
ifile(char fname[])
{
        UCS  line[NLINE], *linep;
	long nline;
	int  s, done, newline;
	size_t charsread = 0;
	EML eml;

        if ((s=ffropen(fname)) != FIOSUC){      /* Hard file open.      */
	    fioperr(s, fname);
	    return(FALSE);
	}

	gotobol(FALSE, 1);

        curbp->b_flag |= BFCHG;			/* we have changed	*/
	curbp->b_flag &= ~BFTEMP;		/* and are not temporary*/
	curbp->b_linecnt = -1;			/* must be recalculated */

	eml.s = fname;
        emlwrite(_("Inserting %s."), &eml);
	done = newline = 0;
	nline = 0L;
	while(!done)
	  if((s = ffgetline(line, NLINE, &charsread, 1)) == FIOEOF){
	      char b[200];

	      if(llength(curwp->w_dotp) > curwp->w_doto)
		lnewline();
	      else
		forwchar(FALSE, 1);

	      if(nline > 1)
	        snprintf(b, sizeof(b), _("Inserted %ld lines"), nline);
	      else
	        snprintf(b, sizeof(b), _("Inserted 1 line"));

	      emlwrite(b, NULL);
	      break;
	  }
	  else{
	      if(newline){
		  lnewline();
		  newline = 0;
	      }

	      switch(s){
		case FIOSUC :			/* copy line into buf */
		  nline++;
		  newline = 1;

		case FIOLNG :
		  for(linep = line; charsread-- > 0; linep++)
		    linsert(1, *linep);

		  break;

		default :
		  done++;
	      }
	  }

        ffclose();                              /* Ignore errors.       */

	return(s != FIOERR);
}


/*
 * pico_fncomplete - pico's function to complete the given file name
 */
int
pico_fncomplete(char *dirarg, char *fn, size_t fnlen)
{
    char *p, *dlist, tmp[NLINE], dir[NLINE];
    int   n, i, match = -1;
#ifdef	DOS
#define	FILECMP(x, y)	(toupper((unsigned char)(x))\
				== toupper((unsigned char)(y)))
#else
#define	FILECMP(x, y)	((x) == (y))
#endif

    strncpy(dir, dirarg, sizeof(dir));
    dir[sizeof(dir)-1] = '\0';
    pfnexpand(dir, sizeof(dir));
    if(*fn && (dlist = p = getfnames(dir, fn, &n, NULL, 0))){
	memset(tmp, 0, sizeof(tmp));
	while(n--){			/* any names in it */
	    for(i = 0; i < sizeof(tmp)-1 && fn[i] && FILECMP(p[i], fn[i]); i++)
	      ;

	    if(!fn[i]){			/* match or more? */
		if(tmp[0]){
		    for(; p[i] && FILECMP(p[i], tmp[i]); i++)
		      ;

		    match = !p[i] && !tmp[i];
		    tmp[i] = '\0';	/* longest common string */
		}
		else{
		    match = 1;		/* may be it!?! */
		    strncpy(tmp,  p, sizeof(tmp));
		    tmp[sizeof(tmp)-1] = '\0';
		}
	    }

	    p += strlen(p) + 1;
	}

	free(dlist);
    }

    if(match >= 0){
	strncpy(fn, tmp, fnlen);
	fn[fnlen-1] = '\0';
	if(match == 1){
	  if ((strlen(dir)+strlen(S_FILESEP)+strlen(fn)) < sizeof(dir)){
	    strncat(dir, S_FILESEP, sizeof(dir)-strlen(dir)-1);
	    dir[sizeof(dir)-1] = '\0';
	    strncat(dir, fn, sizeof(dir)-strlen(dir)-1);
	    dir[sizeof(dir)-1] = '\0';
	    if(isdir(dir, NULL, NULL)){
		strncat(fn, S_FILESEP, fnlen-strlen(fn)-1);
		fn[fnlen-1] = '\0';
	    }
	  }
	  else{
	    emlwrite("File name too BIG!!",0);
	    sleep(3);
	    *fn = '\0';
	  }

	}
    }

    return(match == 1);
}


/*
 * in_oper_tree - returns true if file "f" does reside in opertree
 */
int
in_oper_tree(char *f)
{
    int end = strlen(opertree);

    return(!strncmp(opertree, f, end)
	    && (opertree[end-1] == '/'
		|| opertree[end-1] == '\\'
		|| f[end] == '\0'
		|| f[end] == '/'
		|| f[end] == '\\'));
}
