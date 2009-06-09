#ifndef _PICO_OS_INCLUDED
#define _PICO_OS_INCLUDED


/*----------------------------------------------------------------------

   OS dependencies, WIN NT version.  See also the os-wnt.c files.
   The following stuff may need to be changed for a new port, but once
   the port is done it won't change.  Further down in the file are a few
   constants that you may want to configure differently than they
   are configured, but probably not.

 ----*/



/*----------------- Are we ANSI? ---------------------------------------*/
#define ANSI          /* this is an ANSI compiler */

/*------ If our compiler doesn't understand type void ------------------*/
/* #define void char */

/*-------- Standard ANSI functions usually defined in stdlib.h ---------*/
#include	<stdlib.h>
#include	<string.h>
#include	<dos.h>
#include	<direct.h>
#include	<search.h>
#undef	CTRL
#include	<sys/types.h>
#include	<sys/stat.h>

#define	_WIN32_WINNT	WINVER

/*-------- Standard windows defines and then our window module defines. */
#include <windows.h>
#include <limits.h>
#include <commdlg.h>
#include <cderr.h>
#include <winsock.h>
#include <shellapi.h>

#include "mswin.h"

#include <io.h>
#include <sys/stat.h>
#include <winsock.h>
#include <dos.h>
#include <direct.h>
#include <memory.h>
#include <fcntl.h>
#include <sys/timeb.h>


/* Windows only version and resource defines. */
#include "resource.h"


#undef ERROR


/*---- Declare sys_errlist() if not already declared -------------------*/
/* extern char *sys_errlist[]; */



/*----------------- locale.h -------------------------------------------*/
#include <locale.h>  /* To make matching and sorting work right */
#define collator strcoll



/*----------------- time.h ---------------------------------------------*/
#include <time.h>
/* plain time.h isn't enough on some systems */
/* #include <sys/time.h> */ /* For struct timeval usually in time.h */ 



/*--------------- signal.h ---------------------------------------------*/
#include <signal.h>
/* #include <sys/signal.h> */

#define SigType void     /* value returned by sig handlers is void */
/* #define SigType int */   /* value returned by sig handlers is int */

/* #define POSIX_SIGNALS */
/* #define SYSV_SIGNALS */ /* use System-V signal semantics (ttyin.c) */

#define	SIG_PROTO(args)	args



/*-------------- A couple typedef's for integer sizes ------------------*/
typedef unsigned long usign32_t;
typedef unsigned short usign16_t;



/*-------------- qsort argument type -----------------------------------*/
#define QSType void
/* #define QSType char */

#define QcompType const void


/*-------- Is window resizing available? -------------------------------*/
/* #define RESIZING */
	/* Actually, under windows it is, but RESIZING compiles in UNIX 
	 * signals code for determining when the window resized.  Window's
	 * works differently. */




/*-------- If no vfork, use regular fork -------------------------------*/
/* #define vfork fork */



/*---- When no screen size can be discovered this is the size used -----*/
#define DEFAULT_LINES_ON_TERMINAL	(25)
#define DEFAULT_COLUMNS_ON_TERMINAL	(80)
#define NROW	DEFAULT_LINES_ON_TERMINAL
#define NCOL	DEFAULT_COLUMNS_ON_TERMINAL


#define	ftruncate	chsize


/*
 * File name separators, char and string
 */
#define	C_FILESEP	'\\'
#define	S_FILESEP	"\\"


/*
 * What and where the tool that checks spelling is located.  If this is
 * undefined, then the spelling checker is not compiled into pico.
 */
#define	SPELLER


/*
 * Mode passed chmod() to make tmp files exclusively user read/write-able
 */
/*#define	MODE_READONLY	(S_IREAD | S_IWRITE) */


#ifdef	maindef
/*	possible names and paths of help files under different OSs	*/

char *pathname[] = {
	"picorc",
	"pico.hlp",
	"\\usr\\local\\",
	"\\usr\\lib\\",
	""
};

#define	NPNAMES	(sizeof(pathname)/sizeof(char *))


#endif


#include	"mswin.h"
#include	"msmenu.h"

/* memmove() is a built-in for DOS/Windows */
#define bcopy(a,b,s) memmove (b, a, s)

/*
 * Make sys_errlist visible
 */
/* extern char *sys_errlist[]; */
/* extern int   sys_nerr; */


#endif /* _PICO_OS_INCLUDED */
