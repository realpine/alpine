#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: pipe.c 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $";
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

#include <system.h>

#include "err_desc.h"

#include "canaccess.h"
#include "temp_nam.h"
#include "forkwait.h"
#include "pipe.h"
#include "../charconv/utf8.h"
#include "../charconv/filesys.h"
#include "../debug.h"

#ifdef _WINDOWS
#include "../../pico/osdep/mswin.h"
#endif



/*======================================================================
    pipe
    
    Initiate I/O to and from a process.  These functions used to be
    similar to popen and pclose, but both an incoming stream and an
    output file are provided.
   
 ====*/



/*
 * Global's to helpsignal handler tell us child's status has changed...
 */
static pid_t	child_pid;


/*
 * Internal Protos
 */
void	   zot_pipe(PIPE_S **);
#ifdef	_WINDOWS
int pipe_mswin_exec_wrapper(char *, PIPE_S *, unsigned,
			    void (*)(PIPE_S *, int, void *),
			    void (*)(char *));
#else  /* UNIX */
char	  *pipe_error_msg(char *, char *, char *);
RETSIGTYPE pipe_alarm(int);
#endif /* UNIX */




/*----------------------------------------------------------------------
     Spawn a child process and optionally connect read/write pipes to it

  Args: command -- string to hand the shell
	outfile -- address of pointer containing file to receive output
	errfile -- address of pointer containing file to receive error output
	mode -- mode for type of shell, signal protection etc...
  Returns: pointer to alloc'd PIPE_S on success, NULL otherwise

  The outfile is either NULL, a pointer to a NULL value, or a pointer
  to the requested name for the output file.  In the pointer-to-NULL case
  the caller doesn't care about the name, but wants to see the pipe's
  results so we make one up.  It's up to the caller to make sure the
  free storage containing the name is cleaned up.

  Mode bits serve several purposes.
    PIPE_WRITE tells us we need to open a pipe to write the child's
	stdin.
    PIPE_READ tells us we need to open a pipe to read from the child's
	stdout/stderr.  *NOTE*  Having neither of the above set means 
	we're not setting up any pipes, just forking the child and exec'ing
	the command.  Also, this takes precedence over any named outfile.
    PIPE_STDERR means we're to tie the childs stderr to the same place
	stdout is going.  *NOTE* This only makes sense then if PIPE_READ
	or an outfile is provided.  Also, this takes precedence over any
	named errfile.
    PIPE_RESET means we reset the terminal mode to what it was before
	we started pine and then exec the command. In PC-Pine, _RESET
	was a shortcut for just executing a command.  We'll try to pay
	attention to the above flags to make sure we do the right thing.
    PIPE_PROT means to protect the child from the usual nasty signals
	that might cause premature death.  Otherwise, the default signals are
	set so the child can deal with the nasty signals in its own way.
	NOT USED UNDER WINDOWS
    PIPE_NOSHELL means we're to exec the command without the aid of
	a system shell.  *NOTE* This negates the affect of PIPE_USER.
	NOT USED UNDER WINDOWS
    PIPE_USER means we're to try executing the command in the user's
	shell.  Right now we only look in the environment, but that may get
	more sophisticated later.
	NOT USED UNDER WINDOWS
    PIPE_RUNNOW was added for WINDOWS for the case pipe is called to run
        a shell program (like for url viewing).  This is the only option
	where we don't wait for child termination, and is only obeyed if
	PIPE_WRITE and PIPE_READ aren't set
 ----*/
PIPE_S *
open_system_pipe(char *command, char **outfile, char **errfile, int mode,
		 int timeout, void (*pipecb_f)(PIPE_S *, int, void *),
		 void (*piperr_f)(char *))
{
    PIPE_S *syspipe = NULL;
#ifdef	_WINDOWS
    int exit_code = 0;
    char cmdbuf[1024];
    unsigned flags = 0;
#else
    char    shellpath[MAXPATH+1], *shell;
    int     p[2], oparentd = -1, ochildd = -1, iparentd = -1, ichildd = -1;
#endif

#ifdef	_WINDOWS
    if(mode & PIPE_STDERR)
      flags |= MSWIN_EAW_CAPT_STDERR;
    /* 
     * It'll be a lot more difficult to support READing and WRITing.
     * This was never supported, and there don't look to be any cases
     * that set both of these flags anymore for win32.
     *
     * errfile could probably be supported pretty easily
     */

    if(errfile){
	if(piperr_f)
	  (*piperr_f)("Pipe arg not yet supported: Error File");

	return(NULL);
    }


    if((mode & PIPE_RUNNOW)
       && !(mode & (PIPE_WRITE | PIPE_READ | PIPE_STDERR))){
	if(mswin_shell_exec(command, NULL) == 0
	   && (syspipe = (PIPE_S *) malloc(sizeof(PIPE_S))) != NULL){
	    memset(syspipe, 0, sizeof(PIPE_S));
	    return(syspipe);
	}

	return(NULL);
    }

    strncpy(cmdbuf, command, sizeof(cmdbuf));
    cmdbuf[sizeof(cmdbuf)-1] = '\0';

    if((syspipe = (PIPE_S *) malloc(sizeof(PIPE_S))) == NULL)
      return(NULL);

    memset(syspipe, 0, sizeof(PIPE_S));
    syspipe->mode = mode;
    if(!outfile){
	syspipe->deloutfile = 1;
	if(mode & PIPE_READ){
	    syspipe->outfile = temp_nam(NULL, "po");
	    our_unlink(syspipe->outfile);
	}
    }
    else{
	if(!*outfile) /* asked for, but not named? */
	  *outfile = temp_nam(NULL, "po");

	our_unlink(*outfile);
	syspipe->outfile = (char *) malloc((strlen(*outfile)+1)*sizeof(char));
	snprintf(syspipe->outfile, strlen(*outfile)+1, "%s", *outfile);
    }

    if(mode & PIPE_WRITE){
	/*
	 * Create tmp file to write, spawn child in close_pipe
	 * after tmp file's written...
	 */
	syspipe->infile = temp_nam(NULL, "pw");
	syspipe->out.f = our_fopen(syspipe->infile, "wb");
	syspipe->command = (char *) malloc((strlen(cmdbuf)+1)*sizeof(char));
	snprintf(syspipe->command, strlen(cmdbuf)+1, "%s", cmdbuf);
	dprint((1, "pipe write: %s", cmdbuf));
    }
    else if(mode & PIPE_READ){
	/* 
	 * Create a tmp file for command result, exec the command
	 * here into temp file, and return file pointer to it...
	 */
	syspipe->command = (char *) malloc((strlen(cmdbuf)+1)*sizeof(char));
	snprintf(syspipe->command, strlen(cmdbuf)+1, "%s", cmdbuf);
	dprint((1, "pipe read: %s", cmdbuf));
	if(pipe_mswin_exec_wrapper("pipe command", syspipe,
				   flags, pipecb_f, piperr_f)){
	    if(syspipe->outfile){
		free((void *) syspipe->outfile);
		syspipe->outfile = NULL;
	    }

	    zot_pipe(&syspipe);
	}
	else{
	  syspipe->in.f = our_fopen(syspipe->outfile, "rb");
	  syspipe->exit_code = exit_code;
	}
    }
    else{
	/* we just run the command taking outfile into account */
	syspipe->command = (char *) malloc((strlen(cmdbuf)+1)*sizeof(char));
	snprintf(syspipe->command, strlen(cmdbuf)+1, "%s", cmdbuf);
	if(pipe_mswin_exec_wrapper("pipe command", syspipe,
				   flags, pipecb_f, piperr_f)){
	    if(syspipe->outfile){
		free((void *) syspipe->outfile);
		syspipe->outfile = NULL;
	    }

	    zot_pipe(&syspipe);
	}
	else
	  syspipe->exit_code = exit_code;
    }

#else /* !_WINDOWS */

    if((syspipe = (PIPE_S *) malloc(sizeof(PIPE_S))) == NULL)
      return(NULL);

    memset(syspipe, 0, sizeof(PIPE_S));

    syspipe->mode = mode;

    /*
     * If we're not using the shell's command parsing smarts, build
     * argv by hand...
     */
    if(mode & PIPE_NOSHELL){
	char   **ap, *p;
	size_t   n;

	/* parse the arguments into argv */
	for(p = command; *p && isspace((unsigned char)(*p)); p++)
	  ;					/* swallow leading ws */

	if(*p){
	    int l = strlen(p);

	    if((syspipe->args = (char *) malloc((l + 1) * sizeof(char))) != NULL){
		strncpy(syspipe->args, p, l);
		syspipe->args[l] = '\0';
	    }
	    else{
		if(piperr_f)
		  (*piperr_f)(pipe_error_msg("<null>", "execute",
					     "Can't allocate command string"));
		zot_pipe(&syspipe);
		return(NULL);
	    }
	}
	else{
	    if(piperr_f)
	      (*piperr_f)(pipe_error_msg("<null>", "execute",
					 "No command name found"));
	    zot_pipe(&syspipe);
	    return(NULL);
	}

	for(p = syspipe->args, n = 2; *p; p++)	/* count the args */
	  if(isspace((unsigned char)(*p))
	     && *(p+1) && !isspace((unsigned char)(*(p+1))))
	    n++;

	if ((syspipe->argv = ap = (char **)malloc(n * sizeof(char *))) == NULL){
	    zot_pipe(&syspipe);
	    return(NULL);
	}

	memset(syspipe->argv, 0, n * sizeof(char *));

	for(p = syspipe->args; *p; ){		/* collect args */
	    while(*p && isspace((unsigned char)(*p)))
	      *p++ = '\0';

	    *ap++ = (*p) ? p : NULL;
	    while(*p && !isspace((unsigned char)(*p)))
	      p++;
	}

	/* make sure argv[0] exists in $PATH */
	if(can_access_in_path(getenv("PATH"), syspipe->argv[0],
			      EXECUTE_ACCESS) < 0){
	    if(piperr_f)
	      (*piperr_f)(pipe_error_msg(syspipe->argv[0], "access",
					 error_description(errno)));
	    zot_pipe(&syspipe);
	    return(NULL);
	}
    }

    /* fill in any output filenames */
    if(!(mode & PIPE_READ)){
	if(outfile && !*outfile)
	  *outfile = temp_nam(NULL, "pine_p"); /* asked for, but not named? */

	if(errfile && !*errfile)
	  *errfile = temp_nam(NULL, "pine_p"); /* ditto */
    }

    /* create pipes */
    if(mode & (PIPE_WRITE | PIPE_READ)){
	if(mode & PIPE_WRITE){
	    pipe(p);				/* alloc pipe to write child */
	    oparentd = p[STDOUT_FILENO];
	    ichildd  = p[STDIN_FILENO];
	}

	if(mode & PIPE_READ){
	    pipe(p);				/* alloc pipe to read child */
	    iparentd = p[STDIN_FILENO];
	    ochildd  = p[STDOUT_FILENO];
	}
    }

    if(pipecb_f)				/* let caller prep display */
      (*pipecb_f)(syspipe, OSB_PRE_OPEN, NULL);


    if((syspipe->pid = vfork()) == 0){
 	/* reset child's handlers in requested fashion... */
	(void)signal(SIGINT,  (mode & PIPE_PROT) ? SIG_IGN : SIG_DFL);
	(void)signal(SIGQUIT, (mode & PIPE_PROT) ? SIG_IGN : SIG_DFL);
	(void)signal(SIGHUP,  (mode & PIPE_PROT) ? SIG_IGN : SIG_DFL);
#ifdef	SIGCHLD
	(void) signal(SIGCHLD,  SIG_DFL);
#endif

	/* if parent isn't reading, and we have a filename to write */
	if(!(mode & PIPE_READ) && outfile){	/* connect output to file */
	    int output = our_creat(*outfile, 0600);
	    dup2(output, STDOUT_FILENO);
	    if(mode & PIPE_STDERR)
	      dup2(output, STDERR_FILENO);
	    else if(errfile)
	      dup2(our_creat(*errfile, 0600), STDERR_FILENO);
	}

	if(mode & PIPE_WRITE){			/* connect process input */
	    close(oparentd);
	    dup2(ichildd, STDIN_FILENO);	/* tie stdin to pipe */
	    close(ichildd);
	}

	if(mode & PIPE_READ){			/* connect process output */
	    close(iparentd);
	    dup2(ochildd, STDOUT_FILENO);	/* tie std{out,err} to pipe */
	    if(mode & PIPE_STDERR)
	      dup2(ochildd, STDERR_FILENO);
	    else if(errfile)
	      dup2(our_creat(*errfile, 0600), STDERR_FILENO);

	    close(ochildd);
	}

	if(mode & PIPE_NOSHELL){
	    execvp(syspipe->argv[0], syspipe->argv);
	}
	else{
	    if(mode & PIPE_USER){
		char *env, *sh;
		if((env = getenv("SHELL")) && (sh = strrchr(env, '/'))){
		    shell = sh + 1;
		    strncpy(shellpath, env, sizeof(shellpath)-1);
		    shellpath[sizeof(shellpath)-1] = '\0';
		}
		else{
		    shell = "csh";
		    strncpy(shellpath, "/bin/csh", sizeof(shellpath)-1);
		    shellpath[sizeof(shellpath)-1] = '\0';
		}
	    }
	    else{
		shell = "sh";
		strncpy(shellpath, "/bin/sh", sizeof(shellpath)-1);
		shellpath[sizeof(shellpath)-1] = '\0';
	    }

	    execl(shellpath, shell, command ? "-c" : (char *)NULL, fname_to_locale(command), (char *)NULL);
	}

	fprintf(stderr, "Can't exec %s\nReason: %s",
		command, error_description(errno));
	_exit(-1);
    }

    if((child_pid = syspipe->pid) > 0){
	syspipe->isig = signal(SIGINT,  SIG_IGN); /* Reset handlers to make */
	syspipe->qsig = signal(SIGQUIT, SIG_IGN); /* sure we don't come to  */
	syspipe->hsig = signal(SIGHUP,  SIG_IGN); /* a premature end...     */
	if((syspipe->timeout = timeout) != 0){
	    syspipe->alrm      = signal(SIGALRM,  pipe_alarm);
	    syspipe->old_timeo = alarm(timeout);
	}

	if(mode & PIPE_WRITE){
	    close(ichildd);
	    if(mode & PIPE_DESC)
	      syspipe->out.d = oparentd;
	    else
	      syspipe->out.f = fdopen(oparentd, "w");
	}

	if(mode & PIPE_READ){
	    close(ochildd);
	    if(mode & PIPE_DESC)
	      syspipe->in.d = iparentd;
	    else
	      syspipe->in.f = fdopen(iparentd, "r");
	}
    }
    else{
	if(mode & (PIPE_WRITE | PIPE_READ)){
	    if(mode & PIPE_WRITE){
		close(oparentd);
		close(ichildd);
	    }

	    if(mode & PIPE_READ){
		close(iparentd);
		close(ochildd);
	    }
	}

	if(pipecb_f)				/* let caller fixup display */
	  (*pipecb_f)(syspipe, OSB_POST_OPEN, NULL);

	if(outfile && *outfile){
	    our_unlink(*outfile);
	    free((void *) *outfile);
	    *outfile = NULL;
	}

	if(errfile && *errfile){
	    our_unlink(*errfile);
	    free((void *) *errfile);
	    *errfile = NULL;
	}

	if(piperr_f)
	  (*piperr_f)(pipe_error_msg(command, "fork",
				     error_description(errno)));
	zot_pipe(&syspipe);
    }

#endif /* UNIX */

    return(syspipe);
}



#ifndef _WINDOWS
/*----------------------------------------------------------------------
    Return appropriate error message 

  Args: cmd -- command we were trying to exec
	op -- operation leading up to the exec
	res -- result of that operation

 ----*/
char *
pipe_error_msg(char *cmd, char *op, char *res)
{
    static char ebuf[512];

    snprintf(ebuf, 256, "Pipe can't %.256s \"%.32sb\": %.223s",
	     op ? op : "?", cmd ? cmd : "?", res ? res : "?");

    return(ebuf);
}
#endif /* !_WINDOWS */


/*----------------------------------------------------------------------
    Free resources associated with the given pipe struct

  Args: syspipe -- address of pointer to struct to clean up

 ----*/
void
zot_pipe(PIPE_S **syspipe)
{
    if((*syspipe)->args){
	free((void *) (*syspipe)->args);
	(*syspipe)->args = NULL;
    }

    if((*syspipe)->argv){
	free((void *) (*syspipe)->argv);
	(*syspipe)->argv = NULL;
    }

    if((*syspipe)->tmp){
	free((void *) (*syspipe)->tmp);
	(*syspipe)->tmp = NULL;
    }

#ifdef	_WINDOWS

    if((*syspipe)->outfile){
	free((void *) (*syspipe)->outfile);
	(*syspipe)->outfile = NULL;
    }

    if((*syspipe)->command){
	free((void *) (*syspipe)->command);
	(*syspipe)->command = NULL;
    }

#endif /* _WINDOWS */

    free((void *) *syspipe);
    *syspipe = NULL;
}



/*
 * Returns: 0 if all went well, -1 otherwise
 */
int
pipe_close_write(PIPE_S *syspipe)
{
    int rv = 0;

    if(!syspipe || !syspipe->out.f)
      return -1;

#ifdef	_WINDOWS

    {
	unsigned flags = 0;

	if(syspipe->mode & PIPE_STDERR)
	  flags |= MSWIN_EAW_CAPT_STDERR;

	rv = fclose(syspipe->out.f);
	syspipe->out.f = NULL;
	if(syspipe->mode & PIPE_WRITE){ 
	    /* 
	     * PIPE_WRITE should always be set if we're trying to close
	     * the write end.
	     * PIPE_WRITE can't start process till now,  all the others
	     *  will have already run
	     */
	    if(pipe_mswin_exec_wrapper("pipe command", syspipe,
				       flags, NULL, NULL))
	      /* some horrible error just occurred */
	      rv = -1;
	    else
	      syspipe->in.f = our_fopen(syspipe->outfile, "rb");
	}
	else
	  rv = -1;
    }

#else  /* UNIX */

    rv = fclose(syspipe->out.f) ? -1 : 0;
    syspipe->out.f = NULL;

#endif
    return(rv);
}



/*----------------------------------------------------------------------
    Close pipe previously allocated and wait for child's death

  Args: syspipe -- address of pointer to struct returned by open_system_pipe
        exitval -- return exit status here.

  Returns:
    Two modes of return values for backcompat.
      If exitval == NULL
	 returns exit status of child or -1 if invalid syspipe
      If exitval != NULL
         returns -1 if invalid syspipe or 0 if ok. In that case, exitval
	 of child is returned in exitval
 ----*/
int
close_system_pipe(PIPE_S **syspipe, int *exitval, void (*pipecb_f) (PIPE_S *, int, void *))
{
#ifdef	_WINDOWS
    int rv = 0;
    unsigned flags = 0;

    if(!(syspipe && *syspipe))
      return(-1);

    if((*syspipe)->mode & PIPE_STDERR)
      flags |= MSWIN_EAW_CAPT_STDERR;

    if(((*syspipe)->mode & PIPE_WRITE) && (*syspipe)->out.f){
	fclose((*syspipe)->out.f);
	/*
	 * PIPE_WRITE can't start process till now,  all the others
	 *  will have already run
	 */
	if(pipe_mswin_exec_wrapper("pipe command", (*syspipe),
				   flags, pipecb_f, NULL))
	  /* some horrible error just occurred */
	  rv = -1;
    }
    else if((*syspipe)->mode & PIPE_READ)
      if((*syspipe)->in.f)
	fclose((*syspipe)->in.f);

    if(exitval){
	*exitval = (*syspipe)->exit_code;
	dprint((5, "Closed pipe: exitval=%d\n", *exitval));
    }

    if((*syspipe)->infile)
      our_unlink((*syspipe)->infile);

    if((*syspipe)->outfile && (*syspipe)->deloutfile)
      our_unlink((*syspipe)->outfile);

    if(rv != -1 && !exitval)
      rv = (*syspipe)->exit_code;

    zot_pipe(syspipe);

#ifdef	DEBUG
    if(!exitval){
	dprint((5, "Closed pipe: rv=%d\n", rv));
    }
#endif /* DEBUG */

    return(rv);

#else  /* UNIX */
    int status;

    if(!(syspipe && *syspipe))
      return -1;

    if(((*syspipe)->mode) & PIPE_WRITE){
	if(((*syspipe)->mode) & PIPE_DESC){
	    if((*syspipe)->out.d >= 0)
	      close((*syspipe)->out.d);
	}
	else if((*syspipe)->out.f)
	  fclose((*syspipe)->out.f);
    }

    if(((*syspipe)->mode) & PIPE_READ){
	if(((*syspipe)->mode) & PIPE_DESC){
	    if((*syspipe)->in.d >= 0)
	      close((*syspipe)->in.d);
	}
	else if((*syspipe)->in.f)
	  fclose((*syspipe)->in.f);
    }
    
    if(pipecb_f)
      (*pipecb_f)(*syspipe, OSB_PRE_CLOSE, NULL);

    /* wait on the child */
    (void) process_reap((*syspipe)->pid, &status, PR_NONE);

    /* restore original handlers... */
    (void) signal(SIGINT,  (*syspipe)->isig);
    (void) signal(SIGHUP,  (*syspipe)->hsig);
    (void) signal(SIGQUIT, (*syspipe)->qsig);

    if((*syspipe)->timeout){
	(void)signal(SIGALRM, (*syspipe)->alrm);
	alarm((*syspipe)->old_timeo);
	child_pid = 0;
    }

    if(pipecb_f)
      (*pipecb_f)(*syspipe, OSB_POST_CLOSE, NULL);

    zot_pipe(syspipe);

    if(exitval){
	*exitval = status;
	return 0;
    }
    else{
	return(status);
    }
#endif /* UNIX */
}


/*
 * process_reap - manage child demise and return exit status
 *
 *   Args: pid   -- id of process to reap
 *         esp   -- pointer to exist status 
 *         flags -- special reaping considerations
 *
 *   Returns:
 *         < 0 -- if there's a problem
 *         0   -- if no child to reap
 *         > 0 -- process id of the child
 */
pid_t
process_reap(pid_t pid, int *esp, int flags)
{
#ifdef	_WINDOWS

    return 0;

#else /* UNIX */
    WAITSTATUS_T wstatus;
    pid_t	 rv;
    int		 wflags;

#if	HAVE_WAITPID

    wflags = 0;

#ifdef	WNOHANG
    if(flags & PR_NOHANG)
      wflags |= WNOHANG;
#endif

    while (((rv = waitpid(pid, &wstatus, wflags)) < 0) && (errno != ECHILD));

#elif	HAVE_WAIT4

    wflags = 0;

#ifdef	WNOHANG
    if(flags & PR_NOHANG)
      wflags |= WNOHANG;
#endif

    while (((rv = wait4(pid,&wstatus,wflags,NULL)) < 0) && (errno != ECHILD));

#elif	HAVE_WAIT

    while (((rv = wait(&wstatus)) != pid) && ((rv > 0) || (errno != ECHILD)));

#else

    /* BUG: BAIL */
    
#endif

    if(rv > 0)
      *esp = (WIFEXITED(wstatus)) ?  (int) WEXITSTATUS(wstatus) : -1;

    return(rv);
#endif /* UNIX */
}


#ifndef _WINDOWS
RETSIGTYPE
pipe_alarm(int sig)
{
    if(child_pid)
      kill(child_pid, SIGINT);
}
#endif /* !_WINDOWS */


#ifdef	_WINDOWS
/*
 * Wrapper around mswin_exec_and_wait()
 */
int
pipe_mswin_exec_wrapper(char *whatsit, 
			PIPE_S *syspipe, unsigned flags, 
			void (*pipecb_f)(PIPE_S *, int, void *),
			void (*piperr_f)(char *))
{
    int rv;

    flags |= MSWIN_EAW_CTRL_C_CANCELS;

    if(pipecb_f)
      (*pipecb_f)(syspipe, OSB_PRE_OPEN, NULL);

    rv = mswin_exec_and_wait(whatsit, syspipe->command, 
			     syspipe->infile, syspipe->outfile,
			     &syspipe->exit_code, flags);

    if(pipecb_f)
      (*pipecb_f)(syspipe, OSB_POST_OPEN, (void *)rv);

    return rv;
}
#endif
