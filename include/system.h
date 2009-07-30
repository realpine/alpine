/*----------------------------------------------------------------------
  $Id: system.h 776 2007-10-24 23:14:26Z hubert@u.washington.edu $
  ----------------------------------------------------------------------*/

/* ========================================================================
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

/*
 *  System-Specific includes, macros and prototypes based on config.h definitions
 */


#ifndef _SYSTEM_INCLUDED
#define _SYSTEM_INCLUDED

#ifndef _WINDOWS
#include "config.h"
#else
#include "config.wnt.h"
#endif

#include <stdio.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef	HAVE_CTYPE_H
# include <ctype.h>
#endif

#ifdef STDC_HEADERS
# include <stdlib.h>

# if	HAVE_STRING_H
#  include <string.h>
# endif

# if	HAVE_STRINGS_H
#  include <strings.h>
# endif

# if	HAVE_WCHAR_H
#  include <wchar.h>
# endif

# include <stdarg.h>

#else
void *malloc (size_t);
void *realloc (void *, size_t);
void free (void *);
size_t strlen (const char *);
int strcmp (const char *, const char *);
char *strcpy (char *, const char *);
char *strcat (char *, const char *);
char *strtok (char *, const char *);
# ifndef HAVE_STRCHR
# warn "No strchr(); trying to use index() instead."
#  define strchr index
#  define strrchr rindex
# endif
# ifndef HAVE_STRDUP
#  error "C library missing strdup()!"
# endif
char *strdup (const char *);
char *strstr (const char *, const char *);
# ifdef HAVE_MEMORY_H
#  include <memory.h>
# else
#  ifdef HAVE_MEMCPY
void *memcpy(void *, const void *, size_t);
void *memmove(void *, const void *, size_t);
void *memset(void *, int, size_t);
#  else
#   define memcpy(d, s, n) bcopy ((s), (d), (n))
#   define memmove(d, s, n) bcopy ((s), (d), (n))
#   define memset(d, c, n) bzero ((d), (n))
#  endif
# endif
#endif

/* Set System Collator */
#if	HAVE_STRCOLL
# define collator strcoll
#else
# define collator strucmp
#endif

#define PREREQ_FOR_SYS_TRANSLATION (HAVE_WCHAR_H && HAVE_WCRTOMB && HAVE_WCWIDTH && HAVE_MBSTOWCS && HAVE_LANGINFO_H && defined(CODESET) && !defined(_WINDOWS))

/* System uin32 definition */
#if	HAVE_STDINT_H
# include <stdint.h>
#endif

#if	HAVE_INTTYPES_H
# include <inttypes.h>
#endif

#if	HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#if	HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

/* set MAXPATH based on what's available */

#if	HAVE_LIMITS_H
# include <limits.h>
#endif

#ifdef	PATH_MAX
# define MAXPATH PATH_MAX
#else
# if	HAVE_SYS_PARAM_H
#  include <sys/param.h>
# endif
# if	MAXPATHLEN
#   define MAXPATH MAXPATHLEN
# else
#   define MAXPATH (512)
# endif
#endif

#if	TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if	HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#if	HAVE_UTIME_H
# include <utime.h>
#elif	HAVE_SYS_UTIME_H
# include <sys/utime.h>
#endif

#if	HAVE_ERRNO_H
#include <errno.h>
#endif

#if	HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#if	HAVE_FCNTL_H
#include <fcntl.h>
#endif

#if	defined(O_NONBLOCK)
# define NON_BLOCKING_IO O_NONBLOCK
#elif	defined(FNDELAY)
# define NON_BLOCKING_IO FNDELAY
#endif

#if	HAVE_PWD_H
# include <pwd.h>
#endif

#if	HAVE_SIGNAL_H
# include <signal.h>
#endif

#if	HAVE_SETJMP_H
# include <setjmp.h>
#endif

#if	HAS_TERMIOS
# include <termios.h>
#endif

#if	HAS_TERMIO
# include <termio.h>
#endif

#if	HAS_SGTTY
# include <sgtty.h>
#endif

#if	HAVE_NETDB_H
# include <netdb.h>
#endif

#if	GWINSZ_IN_SYS_IOCTL
# include <sys/ioctl.h>
#endif

#if	HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#if	HAVE_SYS_UIO_H
#include <sys/uio.h>
#endif

#if	HAVE_SYS_UN_H
#include <sys/un.h>
#else
struct	sockaddr_un {
	short	sun_family;		/* AF_UNIX */
	char	sun_path[108];		/* path name (gag) */
};
#endif

#if	HAVE_PTHREAD_H
# include <pthread.h>
#endif

#if	HAVE_ASSERT_H
# include <assert.h>
#endif

#if	HAVE_LOCALE_H
# include <locale.h>
#endif

#if	HAVE_LANGINFO_H
# include <langinfo.h>
#endif

#if	HAVE_REGEX_H
# include <regex.h>
#endif

#ifdef	ENABLE_NLS

# include <libintl.h>

# define _(String) gettext (String)
# define N_(String) String

#else	/* !ENABLE_NLS */

# define _(String) String
# define N_(String) String

#endif	/* !ENABLE_NLS */


#ifdef  ENABLE_LDAP

# include <lber.h>
# include <ldap.h>

# ifndef LDAPAPI
#  if defined(LDAP_API_VERSION)		/* draft-ietf-ldapext-ldap-c-api-04 */
#   define LDAPAPI LDAP_API_VERSION
#  elif defined(LDAP_OPT_SIZELIMIT)
#   define LDAPAPI 15			/* Netscape SDK */
#  elif defined(LDAP_BEGIN_DECL)
#   define LDAPAPI 11			/* OpenLDAP 1.x */
#  else					/* older version */
#   define LDAPAPI 10			/* Umich */
#  endif

#  ifndef LDAP_OPT_ON
#   define LDAP_OPT_ON ((void *)1)
#  endif
#  ifndef LDAP_OPT_OFF
#   define LDAP_OPT_OFF ((void *)0)
#  endif
#  ifndef LDAP_OPT_SIZELIMIT
#   define LDAP_OPT_SIZELIMIT 1134  /* we're hacking now! */
#  endif
#  ifndef LDAP_OPT_TIMELIMIT
#   define LDAP_OPT_TIMELIMIT 1135
#  endif
#  ifndef LDAP_OPT_PROTOCOL_VERSION
#   define LDAP_OPT_PROTOCOL_VERSION 1136
#  endif

#  ifndef LDAP_MSG_ONE
#   define LDAP_MSG_ONE (0x00)
#   define LDAP_MSG_ALL (0x01)
#   define LDAP_MSG_RECEIVED (0x02)
#  endif
# endif
#endif  /* ENABLE_LDAP */


#if	defined(DOS) || defined(OS2)
# define NEWLINE	"\r\n"		/* local EOL convention...  */
#else
# define NEWLINE	"\n"		/* necessary for gf_* funcs */
#endif

#if	OSX_TARGET
# include <Carbon/Carbon.h>
# include <ApplicationServices/ApplicationServices.h>

/* Avoid OSX Conflicts */
#define Fixed	PineFixed
#define Handle	PineHandle
#define Comment	PineComment
#define	normal	PineNormal
#endif

#if	HAVE_SYSLOG_H
# include <syslog.h>
#elif	HAVE_SYS_SYSLOG_H
# include <sys/syslog.h>
#endif

#ifdef	ENABLE_DMALLOC
# include <dmalloc.h>
#endif

#ifdef _WINDOWS

/*
 * Defining these turns generic data types and function calls
 * into their unicode (wide) versions. Not defining it will
 * break the code, so leave it defined.
 */
#ifndef _UNICODE
# define _UNICODE
#endif
#ifndef UNICODE
# define UNICODE
#endif

#define	_WIN32_WINNT	WINVER

/*-------- Standard windows defines and then our window module defines. */
#include	<dos.h>
#include	<direct.h>
#include	<search.h>
#undef	CTRL
#include <io.h>
#include <windows.h>
#include <commdlg.h>
#include <cderr.h>
#include <winsock.h>
#include <shellapi.h>
#include <sys/timeb.h>
#include <tchar.h>
#include <wchar.h>
#include <assert.h>

#if defined(WINVER) && WINVER >= 0x0501
# define WINCRED 1	/* WINCRED will work */
#else
# define WINCRED 0	/* too old for WINCRED to work */
#endif

#undef ERROR

typedef int mode_t;
typedef int uid_t;
typedef int gid_t;

#undef snprintf
#define snprintf _snprintf
#undef CM_NONE

#undef utimbuf
#define utimbuf _utimbuf

#endif /* _WINDOWS */


#if defined(PASSFILE) && defined(APPLEKEYCHAIN)
#  error "Cant define both PASSFILE and APPLEKEYCHAIN"
#endif
#if defined(PASSFILE) && defined(WINCRED)
#  error "Cant define both PASSFILE and WINCRED"
#endif
#if defined(APPLEKEYCHAIN) && defined(WINCRED)
#  error "Cant define both APPLEKEYCHAIN and WINCRED"
#endif

#if defined(PASSFILE) || defined(APPLEKEYCHAIN) || defined(WINCRED)
# define LOCAL_PASSWD_CACHE
#endif

#endif /* _SYSTEM_INCLUDED */
