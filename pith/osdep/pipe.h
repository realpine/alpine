/*
 * $Id: pipe.h 769 2007-10-24 00:15:40Z hubert@u.washington.edu $
 *
 * ========================================================================
 * Copyright 2006 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */

#ifndef PITH_OSDEP_PIPE_INCLUDED
#define PITH_OSDEP_PIPE_INCLUDED

/* Standard I/O File Descriptor Definitions  */
#ifndef	STDIN_FILENO
#define	STDIN_FILENO	0
#endif
#ifndef	STDOUT_FILENO
#define	STDOUT_FILENO	1
#endif
#ifndef	STDERR_FILENO
#define	STDERR_FILENO	2
#endif


/*
 * Flags for the pipe command routines...
 */
#define	PIPE_WRITE	0x0001			/* set up pipe for reading */
#define	PIPE_READ	0x0002			/* set up pipe for reading */
#define	PIPE_NOSHELL	0x0004			/* don't exec in shell     */
#define	PIPE_USER	0x0008			/* user mode		   */
#define	PIPE_STDERR	0x0010			/* stderr to child output  */
#define	PIPE_PROT	0x0020			/* protected mode	   */
#define	PIPE_RESET	0x0040			/* reset terminal mode     */
#define	PIPE_DESC	0x0080			/* no stdio desc wrapping  */
#define	PIPE_SILENT	0x0100			/* no screen clear, etc	   */
#define PIPE_RUNNOW     0x0200			/* don't wait for child (PC-Pine) */
#define PIPE_RAW        0x0400			/* don't convert to locale */
#define PIPE_NONEWMAIL  0x0800			/* don't call new_mail     */

#ifdef _WINDOWS
/*
 * Flags for mswin_exec_and_wait
 */
#define MSWIN_EAW_CAPT_STDERR     0x0001
#define MSWIN_EAW_CTRL_C_CANCELS  0x0002
#endif


/*
 * Reaper flags
 */
#define	PR_NONE		0x0000
#ifdef	WNOHANG
#define	PR_NOHANG	0x0001
#endif

/*
 * open_system_pipe callback so caller can insert code, typically interface
 * stuff right before/after the fork and before/after wait
 */
#define	OSB_PRE_OPEN	0x0001
#define	OSB_POST_OPEN	0x0002
#define	OSB_PRE_CLOSE	0x0004
#define	OSB_POST_CLOSE	0x0008

/*
 * stucture required for the pipe commands...
 */
typedef struct pipe_s {
    pid_t    pid;				/* child's process id       */
    int	     mode,				/* mode flags used to open  */
	     timeout,				/* wait this long for child */
	     old_timeo;				/* previous active alarm    */
    RETSIGTYPE (*hsig)(int),			/* previously installed...  */
	       (*isig)(int),			/* handlers		    */
	       (*qsig)(int),
	       (*alrm)(int),
	       (*chld)(int);
    union {
	FILE *f;
	int   d;
    }	     in;				/* input data handle	    */
    union {
	FILE *f;
	int   d;
    }	     out;				/* output data handle	    */
    char   **argv,				/* any necessary args	    */
	    *args,
	    *tmp;				/* pointer to stuff	    */
#ifdef	_WINDOWS
    char    *infile;                            /* file containing pipe's stdin  */
    char    *outfile;                           /* file containing pipe's stdout */
    char    *command;				/* command to execute */
    int      exit_code;                         /* proc rv if run right away */
    int      deloutfile;                        /* need to rm outfile at close */
#endif
} PIPE_S;


/*
 * Exported Prototypes
 */
PIPE_S	*open_system_pipe(char *, char **, char **, int, int,
			  void (*)(PIPE_S *, int, void *), void (*)(char *));
int	 close_system_pipe(PIPE_S **, int *, void (*)(PIPE_S *, int, void *));
int	 pipe_close_write(PIPE_S *);
pid_t	 process_reap(pid_t, int *, int);


#endif /* PITH_OSDEP_PIPE_INCLUDED */
