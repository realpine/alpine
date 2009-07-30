#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: altedit.c 854 2007-12-07 17:44:43Z hubert@u.washington.edu $";
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
 */


#include <system.h>
#include <general.h>

#include "../../pith/osdep/pipe.h"
#include "../../pith/osdep/forkwait.h"
#include "../../pith/charconv/filesys.h"

#include "../estruct.h"
#include "../mode.h"
#include "../pico.h"
#include "../edef.h"
#include "../efunc.h"
#include "../keydefs.h"
#include "../utf8stub.h"
#include "tty.h"
#include "filesys.h"

#ifdef _WINDOWS
#include "mswin.h"
#endif

#include "altedit.h"
#ifndef _WINDOWS
#include "signals.h"

#ifdef	SIGCHLD
static jmp_buf pico_child_state;
static short   pico_child_jmp_ok, pico_child_done;
#endif /* SIGCHLD */

#endif /* !_WINDOWS */


/* internal prototypes */
#ifndef _WINDOWS
#ifdef	SIGCHLD
RETSIGTYPE child_handler(int);
#endif /* SIGCHLD */
#else /* _WINDOWS */
int		alt_editor_valid(char *, char *, size_t);
int		alt_editor_valid_fp(char *);
#endif /* _WINDOWS */


/*
 * alt_editor - fork off an alternate editor for mail message composition 
 *              if one is configured and passed from pine.  If not, only
 *              ask for the editor if advanced user flag is set, and 
 *              suggest environment's EDITOR value as default.
 */
int
alt_editor(int f, int n)
{
#ifndef _WINDOWS
    char   eb[NLINE];				/* buf holding edit command */
    char   *fn;					/* tmp holder for file name */
    char   result[128];				/* result string */
    char   prmpt[128];
    int	   i, done = 0, ret = 0, rv, trv;
    pid_t  child, pid;
    RETSIGTYPE (*ohup)(int);
    RETSIGTYPE (*oint)(int);
    RETSIGTYPE (*osize)(int);
    int status;
    EML eml;

    eml.s = f ? "speller" : "editor";

    if(gmode&MDSCUR){
	emlwrite("Alternate %s not available in restricted mode", &eml);
	return(-1);
    }

    strncpy(result, "Alternate %s complete.", sizeof(result));
    result[sizeof(result)-1] = '\0';

    if(f){
	if(alt_speller){
	  strncpy(eb, alt_speller, sizeof(eb));
	  eb[sizeof(eb)-1] = '\0';
	}
	else
	  return(-1);
    }
    else if(Pmaster == NULL){
	return(-1);
    }
    else{
	eb[0] = '\0';

	if(Pmaster->alt_ed){
	    char **lp, *wsp, *path, fname[MAXPATH+1];
	    int	   c;

	    for(lp = Pmaster->alt_ed; *lp && **lp; lp++){
		if((wsp = strpbrk(*lp, " \t")) != NULL){
		    c = *wsp;
		    *wsp = '\0';
		}

		if(strchr(*lp, '/')){
		    rv = fexist(*lp, "x", (off_t *)NULL);
		}
		else{
		    if(!(path = getenv("PATH")))
		      path = ":/bin:/usr/bin";

		    rv = ~FIOSUC;
		    while(rv != FIOSUC && *path && pathcat(fname, &path, *lp))
		      rv = fexist(fname, "x", (off_t *)NULL);
		}

		if(wsp)
		  *wsp = c;

		if(rv == FIOSUC){
		    strncpy(eb, *lp, sizeof(eb));
		    eb[sizeof(eb)-1] = '\0';
		    break;
		}
	    }
	}

	if(!eb[0]){
	    if(!(gmode&MDADVN)){
		unknown_command(0);
		return(-1);
	    }

	    if(getenv("EDITOR")){
	      strncpy(eb, (char *)getenv("EDITOR"), sizeof(eb));
	      eb[sizeof(eb)-1] = '\0';
	    }
	    else
	      *eb = '\0';

	    while(!done){
		pid = mlreplyd_utf8("Which alternate editor ? ", eb,
			       sizeof(eb), QDEFLT, NULL);
		switch(pid){
		  case ABORT:
		    curwp->w_flag |= WFMODE;
		    return(-1);
		  case HELPCH:
		    emlwrite("no alternate editor help yet", NULL);

/* take sleep and break out after there's help */
		    sleep(3);
		    break;
		  case (CTRL|'L'):
		    sgarbf = TRUE;
		    update();
		    break;
		  case TRUE:
		  case FALSE:			/* does editor exist ? */
		    if(*eb == '\0'){		/* leave silently? */
			mlerase();
			curwp->w_flag |= WFMODE;
			return(-1);
		    }

		    done++;
		    break;
		    default:
		    break;
		}
	    }
	}
    }

    if((fn=writetmp(1, NULL)) == NULL){		/* get temp file */
	emlwrite("Problem writing temp file for alt editor", NULL);
	return(-1);
    }

    strncat(eb, " ", sizeof(eb)-strlen(eb)-1);
    eb[sizeof(eb)-1] = '\0';
    strncat(eb, fn, sizeof(eb)-strlen(eb)-1);
    eb[sizeof(eb)-1] = '\0';


    for(i = 0; i <= ((Pmaster) ? term.t_nrow : term.t_nrow - 1); i++){
	movecursor(i, 0);
	if(!i){
	    fputs("Invoking alternate ", stdout);
	    fputs(f ? "speller..." : "editor...", stdout);
	}

	peeol();
    }

    (*term.t_flush)();
    if(Pmaster)
      (*Pmaster->tty_fix)(0);
    else
      vttidy();

#ifdef	SIGCHLD
    if(Pmaster){
	/*
	 * The idea here is to keep any mail connections our caller
	 * may have open while our child's out running around...
	 */
	pico_child_done = pico_child_jmp_ok = 0;
	(void) signal(SIGCHLD,  child_handler);
    }
#endif

    if((child = fork()) > 0){		/* wait for the child to finish */
	ohup = signal(SIGHUP, SIG_IGN);	/* ignore signals for now */
	oint = signal(SIGINT, SIG_IGN);
#if	defined(SIGWINCH) && defined(TIOCGWINSZ)
        osize = signal(SIGWINCH, SIG_IGN);
#endif

#ifdef	SIGCHLD
	if(Pmaster){
	    while(!pico_child_done){
		(*Pmaster->newmail)(0, 0);
		if(!pico_child_done){
		    if(setjmp(pico_child_state) == 0){
			pico_child_jmp_ok = 1;
			sleep(600);
		    }
		    else
		      our_sigunblock(SIGCHLD);
		}

		pico_child_jmp_ok = 0;
	    }
	}
#endif

	trv = process_reap(child, &status, PR_NONE);

	signal(SIGHUP, ohup);	/* restore signals */
	signal(SIGINT, oint);
#if	defined(SIGWINCH) && defined(TIOCGWINSZ)
        signal(SIGWINCH, osize);
#endif

	/*
	 * Report child's unnatural or unhappy exit...
	 */
	if(trv > 0 && status == 0){
	  strncpy(result, "Alternate %s done", sizeof(result));
	  result[sizeof(result)-1] = '\0';
	}
	else {
	  snprintf(result, sizeof(result), "Alternate %%s terminated abnormally (%d)",
		  (trv == 0) ? status : -1);
	  if(f)
	    ret = -1;
	  else{
	      snprintf(prmpt, sizeof(prmpt), "Alt editor failed, use file %.20s of size %%ld chars anyway", fn);
	      ret = -2;
	  }
	}
    }
    else if(child == 0){		/* spawn editor */
	signal(SIGHUP, SIG_DFL);	/* let editor handle signals */
	signal(SIGINT, SIG_DFL);
#if	defined(SIGWINCH) && defined(TIOCGWINSZ)
        signal(SIGWINCH, SIG_DFL);
#endif
#ifdef	SIGCHLD
	(void) signal(SIGCHLD,  SIG_DFL);
#endif
	if(execl("/bin/sh", "sh", "-c", fname_to_locale(eb), (char *) NULL) < 0)
	  exit(-1);
    }
    else {				/* error! */
	snprintf(result, sizeof(result), "\007Can't fork %%s: %s", errstr(errno));
	ret = -1;
    }

#ifdef	SIGCHLD
    (void) signal(SIGCHLD,  SIG_DFL);
#endif

    if(Pmaster)
      (*Pmaster->tty_fix)(1);

    /*
     * replace edited text with new text 
     */
    curbp->b_flag &= ~BFCHG;		/* make sure old text gets blasted */

    if(ret == -2){
	off_t filesize;
	char  prompt[128];

	rv = fexist(fn, "r", &filesize);
	if(rv == FIOSUC && filesize > 0){
	    snprintf(prompt, sizeof(prompt), prmpt, (long) filesize);
	    /* clear bottom 3 rows */
	    pclear(term.t_nrow-2, term.t_nrow);
	    i = mlyesno_utf8(prompt, FALSE);
	    if(i == TRUE){
		ret = 0;
		strncpy(result, "OK, alternate %s done", sizeof(result));
		result[sizeof(result)-1] = '\0';
	    }
	    else
	      ret = -1;
	}
	else
	  ret = -1;
    }

    if(ret == 0)
      readin(fn, 0, 0);			/* read new text overwriting old */

    our_unlink(fn);			/* blast temp file */
    curbp->b_flag |= BFCHG;		/* mark dirty for packbuf() */
    ttopen();				/* reset the signals */
    pico_refresh(0, 1);			/* redraw */
    update();
    emlwrite(result, &eml);
    return(ret);
#else /* _WINDOWS */
    char   eb[2 * PATH_MAX];			/* buf holding edit command */
    char  *fn;					/* tmp holder for file name */
    char   errbuf[128];
    char **lp;
    int	   status;
    int	   done;
    int	   rc;


    if(Pmaster == NULL)
      return(-1);

    if(gmode&MDSCUR){
	emlwrite("Alternate editor not available in restricted mode", NULL);
	return(-1);
    }

    eb[0] = '\0';

    if(Pmaster->alt_ed){
	char *p;

	for(lp = Pmaster->alt_ed; *lp && **lp; lp++)
	  if(*lp[0] == '\"'){
	      if(p = strchr(*lp + 1, '\"')){
		  *p = '\0';
		  rc = alt_editor_valid((*lp) + 1, eb, sizeof(eb));
		  *p = '\"';
		  if(rc){
		      strncat(eb, p + 1, sizeof(eb)-strlen(eb)-1);
		      eb[sizeof(eb)-1] = '\0';
		      break;
		  }
	      }
	  }
	  else if(p = strchr(*lp, ' ')){
	      for(rc = 0; !rc; ){
		  if(p)
		    *p = '\0';

		  rc = alt_editor_valid(*lp, eb, sizeof(eb));

		  if(p)
		    *p = ' ';

		  if(rc){
		      if(p){
			  strncat(eb, p, sizeof(eb)-strlen(eb)-1);
			  eb[sizeof(eb)-1] = '\0';
		      }

		      break;
		  }
		  else if(p)
		    p = strchr(p + 1, ' ');
		  else
		    break;
	      }

	      if(rc)
		break;
	  }
	  else if(alt_editor_valid(*lp, eb, sizeof(eb)))
	    break;
    }

    if(!eb[0]){
	if(!(gmode&MDADVN)){
	    unknown_command(0);
	    return(-1);
	}
	
	/* Guess which editor they want. */
	if(getenv("EDITOR")){
	  strncpy(eb, (char *)getenv("EDITOR"), sizeof(eb));
	  eb[sizeof(eb)-1] = '\0';
	}
	else
	  *eb = '\0';
  
	done = FALSE;
	while(!done){
	    rc = mlreplyd_utf8("Which alternate editor ? ", eb,
			       sizeof(eb),QDEFLT,NULL);

	    switch(rc){
	      case ABORT:
		curwp->w_flag |= WFMODE;
		return(-1);
	      case HELPCH:
		emlwrite("no alternate editor help yet", NULL);

/* take sleep and break out after there's help */
		sleep(3);
		break;
	      case (CTRL|'L'):
		sgarbf = TRUE;
		update();
		break;
	      case TRUE:
	      case FALSE:			/* does editor exist ? */
		if(*eb == '\0'){		/* leave silently? */
		    mlerase();
		    curwp->w_flag |= WFMODE;
		    return(-1);
		}

		done++;
		break;
	      default:
		break;
	    }
	}
    }

    if((fn=writetmp(1, NULL)) == NULL){		/* get temp file */
	emlwrite("Problem writing temp file for alt editor", NULL);
	return(-1);
    }

    emlwrite("Waiting for alternate editor to finish...", NULL);

#ifdef ALTED_DOT
    if(eb[0] == '.' && eb[1] == '\0') {
        status = mswin_exec_and_wait ("alternate editor", eb, fn, fn,
    				  NULL, 0);
    }
    else
#endif /* ALTED_DOT */
    {				/* just here in case ALTED_DOT is defined */
    strncat(eb, " ", sizeof(eb)-strlen(eb)-1);
    eb[sizeof(eb)-1] = '\0';
    strncat(eb, fn, sizeof(eb)-strlen(eb)-1);
    eb[sizeof(eb)-1] = '\0';
    
    status = mswin_exec_and_wait ("alternate editor", eb, NULL, NULL,
				  NULL, 0);
    }

    switch (status) {

    case 0:
	/*
	 * Success:  replace edited text with new text 
	 */
	curbp->b_flag &= ~BFCHG;	/* make sure old text gets blasted */
	readin(fn, 0, 0);	        /* read new text overwriting old */
	our_unlink(fn);			/* blast temp file */
	curbp->b_flag |= BFCHG;		/* mark dirty for packbuf() */
	ttopen();			/* reset the signals */
	pico_refresh(0, 1);		/* redraw */
	return(0);

	
	/*
	 * Possible errors.
	 */
    case -1:
	/* Failed to map return from WinExec to a HTASK. */
	emlwrite("Problem finding alternate editor task handle.", NULL);
	return (-1);
	
    case -2:
	/* User decided to abandon the alternate editor.*/
	emlwrite("Alternate editor abandoned.", NULL);
	return (-1);

    default:
	mswin_exec_err_msg ("alternate editor", status, errbuf, sizeof(errbuf));
	emlwrite (errbuf, NULL);
	return (-1);
    }
    return (-1);
#endif /*_WINDOWS */
}



#ifndef _WINDOWS
int
pathcat(char *buf, char **path, char *file)
{
    register int n = 0;

    while(**path && **path != ':'){
	if(n++ > MAXPATH)
	  return(FALSE);

	*buf++ = *(*path)++;
    }

    if(n){
	if(n++ > MAXPATH)
	  return(FALSE);

	*buf++ = '/';
    }

    while((*buf = *file++) != '\0'){
	if(n++ > MAXPATH)
	  return(FALSE);

	buf++;
    }

    if(**path == ':')
      (*path)++;

    return(TRUE);
}


#ifdef	SIGCHLD
/*
 * child_handler - handle child status change signal
 */
RETSIGTYPE
child_handler(int sig)
{
    pico_child_done = 1;
    if(pico_child_jmp_ok){
#ifdef	sco
	/*
	 * Villy Kruse <vek@twincom.nl> says:
	 * This probably only affects older unix systems with a "broken" sleep             
	 * function such as AT&T System V rel 3.2 and systems derived from
	 * that version. The problem is that the sleep function on these
	 * versions of unix will set up a signal handler for SIGALRM which
	 * will longjmp into the sleep function when the alarm goes off.
	 * This gives problems if another signal handler longjmps out of the
	 * sleep function, thus making the stack frame for the signal function
	 * invalid, and when the ALARM handler later longjmps back into the
	 * sleep function it does no longer have a valid stack frame.
	 * My sugested fix is to cancel the pending alarm in the SIGCHLD
	 * handler before longjmp'ing. This shouldn't hurt as there
	 * shouldn't be any ALARM pending at this point except possibly from
	 * the sleep call.
	 *
	 *
	 * [Even though it shouldn't hurt, we'll make it sco-specific for now.]
	 *
	 *
	 * The sleep call might have set up a signal handler which would
	 * longjmp back into the sleep code, and that would cause a crash.
	 */
	signal(SIGALRM, SIG_IGN);	/* Cancel signal handeler */
	alarm(0);			/* might longjmp back into sleep */ 
#endif
	longjmp(pico_child_state, 1);
    }
}
#endif	/* SIGCHLD */

#else /* _WINDOWS */
/*
 * alt_editor_valid -- test the given exe name exists, and if so
 *		       return full name in "cmdbuf".
 */
int
alt_editor_valid(char *exe, char *cmdbuf, size_t ncmdbuf)
{

/****
 **** This isn't the right way to do this. I believe we
 **** should be doing all of this in TCHARs starting in
 **** the alt_editor function instead of trying to convert
 **** to char * here.
 ****/

    if(!exe)
      return(FALSE);

#ifdef ALTED_DOT
    if(exe[0] == '.' && exe[1] == '\0') {
        cmdbuf[0] = '.';
        cmdbuf[1] = '\0';
        return(TRUE);
    }
    else
#endif /* ALTED_DOT */

    if((isalpha((unsigned char) *exe) && *exe == ':') || strchr(exe, '\\')){
	char *path;

	if(alt_editor_valid_fp(path = exe)){
	    if(strchr(path, ' '))
	      snprintf(cmdbuf, ncmdbuf, "\"%s\"", path);
	    else{
		strncpy(cmdbuf, path, ncmdbuf);
		cmdbuf[ncmdbuf-1] = '\0';
	    }

	    return(TRUE);
	}
    }
    else{
	TCHAR  pathbuflpt[PATH_MAX+1];
	LPTSTR name, exelpt;
	LPTSTR cmdbuflpt;
	char  *utf8;

	cmdbuflpt = (LPTSTR) MemAlloc(ncmdbuf * sizeof(TCHAR));
	cmdbuflpt[0] = L'0';

	exelpt = utf8_to_lptstr(exe);

	if(SearchPath(NULL, exelpt, NULL, PATH_MAX+1, pathbuflpt, &name)){

	    if(_tcschr(pathbuflpt, L' '))
	      _stprintf_s(cmdbuflpt, ncmdbuf, TEXT("\"%s\""), pathbuflpt);
	    else
	      _stprintf_s(cmdbuflpt, ncmdbuf, TEXT("%s"), pathbuflpt);

	    if(exelpt)
	      fs_give((void **) &exelpt);

	    utf8 = lptstr_to_utf8(cmdbuflpt);
	    if(utf8){
		strncpy(cmdbuf, utf8, ncmdbuf);
		cmdbuf[ncmdbuf-1] = '\0';
		fs_give((void **) &utf8);
	    }

	    if(cmdbuflpt)
	      MemFree((void *) cmdbuflpt);

	    return(TRUE);
	}

	if(exelpt)
	  fs_give((void **) &exelpt);

	if(cmdbuflpt)
	  MemFree((void *) cmdbuflpt);
    }

    return(FALSE);
}


/*
 * alt_editor_valid_fp -- test the given full path name exists
 */
int
alt_editor_valid_fp(path)
    char *path;
{
    static char *exts[] = {".exe", ".com", ".bat", NULL};
    char	 pathcopy[PATH_MAX + 1], *dot = NULL;
    int		 i, j;

    for(i = 0; pathcopy[i] = path[i]; i++)
      if(path[i] == '.')
	dot = &path[i];

    if(dot && (!strucmp(dot, ".exe")
       || !strucmp(dot, ".com") || !strucmp(dot, ".bat"))){
	if(fexist(path, "x", (off_t *) NULL) == FIOSUC)
	  return(TRUE);
    }
    else{
	for(j = 0; exts[j]; j++){
	    strncpy(&pathcopy[i], exts[j], sizeof(pathcopy)-i);
	    pathcopy[sizeof(pathcopy)-1] = '\0';
	    if(fexist(pathcopy, "x", (off_t *) NULL) == FIOSUC)
	      return(TRUE);
	}
    }

    return(FALSE);
}
#endif
