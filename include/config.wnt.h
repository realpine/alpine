/* include/config.h.in.  Generated from configure.ac by autoheader.  */

/* Default configuration value */
#define ANSI_PRINTER "attached-to-ansi"

/* Enable background posting support */
#undef BACKGROUND_POST

/* Default configuration value */
#define CHECK_POINT_FREQ 12

/* Default configuration value */
#define CHECK_POINT_TIME 420

/* File name separator as character constant */
#define	C_FILESEP '\\'
/* Avoid OSX Conflict */
/* #undef Comment */

/* Default configuration value */
#define DEADLETTER "deadletr"

/* Compile in debugging */
#define DEBUG

/* Default configuration value */
#define DEBUGFILE "pinedebg.txt"

/* Display debug messages in journal */
#define DEBUGJOURNAL

/* Default configuration value */
#define DEFAULT_COLUMNS_ON_TERMINAL 80

/* Default configuration value */
#define DEFAULT_DEBUG 2

/* Default configuration value */
#define DEFAULT_LINES_ON_TERMINAL 25

/* Default configuration value */
#define DEFAULT_SAVE "savemail"

/* Default configuration value */
#define DF_AB_SORT_RULE "fullname-with-lists-last"

/* Default configuration value */
#define DF_ADDRESSBOOK "addrbook"

/* Default configuration value */
#define DF_DEFAULT_FCC "sentmail"

/* Default configuration value */
#define DF_DEFAULT_PRINTER ANSI_PRINTER

/* Default configuration value */
#define DF_ELM_STYLE_SAVE "no"

/* Default configuration value */
#define DF_FCC_RULE "default-fcc"

/* Default configuration value */
#define DF_FILLCOL "74"

/* Default configuration value */
#define DF_FLD_SORT_RULE "alphabetical"

/* Default configuration value */
#define DF_HEADER_IN_REPLY "no"

/* Default configuration value */
#define DF_KBLOCK_PASSWD_COUNT "1"

/* Default configuration value */
#define DF_LOCAL_ADDRESS "postmaster"

/* Default configuration value */
#define DF_LOCAL_FULLNAME "Local Support"

/* Default configuration value */
#define DF_MAILCHECK "150"

/* Default configuration value */
#define DF_MAIL_DIRECTORY "mail"

/* Default configuration value */
#define DF_MARGIN "0"

/* Default configuration value */
#define DF_OLD_STYLE_REPLY "no"

/* Default configuration value */
#define DF_OVERLAP "2"

#define DF_PINEDIR "\\pine"

/* Default configuration value */
#define DF_REMOTE_ABOOK_HISTORY "3"

/* Default configuration value */
#define DF_SAVED_MSG_NAME_RULE "default-folder"

/* Default configuration value */
#define DF_SAVE_BY_SENDER "no"

/* Default configuration value */
#define DF_SIGNATURE_FILE "pine.sig"

/* Default configuration value */
#define DF_SORT_KEY "arrival"

/* Default configuration value */
#define DF_STANDARD_PRINTER "lpr"

/* Default configuration value */
#define DF_USE_ONLY_DOMAIN_NAME "no"

/* Define enable dmalloc debugging */
/* #undef ENABLE_DMALLOC */

/* Enable LDAP query support - the build command defines this */
/* #define ENABLE_LDAP */

/* Define to 1 if translation of program messages to the user's native
   language is requested. */
/* #define ENABLE_NLS 1 */ /* What should this be? - jpf */

/* Enable From address encoding in sent messages */
/* #undef ENCODE_FROMS */

#define FORWARDED_FLAG "$Forwarded"

/* Avoid OSX Conflict */
/* #undef Fixed */

/* Define to 1 if `TIOCGWINSZ' requires <sys/ioctl.h>. */
/* #undef GWINSZ_IN_SYS_IOCTL */

/* Define if systems uses old BSD-style terminal control */
/* #undef HAS_SGTTY */

/* Define if systems uses termcap terminal database */
/* #undef HAS_TERMCAP */

/* Define if systems uses terminfo terminal database */
/* #undef HAS_TERMINFO */

/* Define if systems uses termio terminal control */
/* #undef HAS_TERMIO */

/* Define if systems uses termios terminal control */
/* #undef HAS_TERMIOS */

/* Define to 1 if you have the `bind' function. */
/* #undef HAVE_BIND */

/* Define to 1 if you have the MacOS X function CFLocaleCopyCurrent in the
   CoreFoundation framework. */
/* #undef HAVE_CFLOCALECOPYCURRENT */

/* Define to 1 if you have the MacOS X function CFPreferencesCopyAppValue in
   the CoreFoundation framework. */
/* #undef HAVE_CFPREFERENCESCOPYAPPVALUE */

/* Define to 1 if you have the `chown' function. */
/* #undef HAVE_CHOWN */

/* Define if the GNU dcgettext() function is already present or preinstalled.
   */
/* #undef HAVE_DCGETTEXT */

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_DIRENT_H */

/* Define to 1 if you have the <dlfcn.h> header file. */
/* #define HAVE_DLFCN_H */

/* Define to 1 if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `fork' function. */
/* #define HAVE_FORK */

/* Define to 1 if you have the `fsync' function. */
/* #define HAVE_FSYNC */

/* Define to 1 if you have the `getpwnam' function. */
/* #define HAVE_GETPWNAM */

/* Define to 1 if you have the `getpwuid' function. */
/* #define HAVE_GETPWUID */

/* Define if the GNU gettext() function is already present or preinstalled. */
/* #define HAVE_GETTEXT */

/* Define to 1 if you have the `gettimeofday' function. */
/* #define HAVE_GETTIMEOFDAY */

/* Define to 1 if you have the `getuid' function. */
/* #define HAVE_GETUID */

/* Define if you have the iconv() function. */
/* #define HAVE_ICONV */

/* Define to 1 if you have the <inttypes.h> header file. */
/* #define HAVE_INTTYPES_H */

/* Define to 1 if you have the `dmalloc' library (-ldmalloc). */
/* #define HAVE_LIBDMALLOC */

/* Define to 1 if you have the `tcl' library (-ltcl). */
/* #define HAVE_LIBTCL */

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the `listen' function. */
#define HAVE_LISTEN 1

/* Define to 1 if you have the <locale.h> header file. */
#define HAVE_LOCALE_H 1

/* Define to 1 if you have the `mbstowcs' function. */
#define HAVE_MBSTOWCS 1

/* Define to 1 if you have the `memcpy' function. */
#define HAVE_MEMCPY 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
/* #define HAVE_NDIR_H */

/* Define to 1 if you have the <netdb.h> header file. */
/* #define HAVE_NETDB_H */

/* Define to 1 if you have the <pwd.h> header file. */
/* #define HAVE_PWD_H */

/* Define to 1 if you have the `pclose' function. */
/* #define HAVE_PCLOSE 1 */

/* Define to 1 if you have the `poll' function. */
/* #define HAVE_POLL */

/* Define to 1 if you have the `popen' function. */
/* #define HAVE_POPEN 1 */

/* Define to 1 if you have the `qsort' function. */
#define HAVE_QSORT 1

/* Define to 1 if you have the `read' function. */
#define HAVE_READ 1

/* Define to 1 if you have the `rename' function. */
/* #define HAVE_RENAME  */

/* Define to 1 if you have the `select' function. */
#define HAVE_SELECT 1

/* Define to 1 if you have the `setjmp' function. */
#define HAVE_SETJMP 1

/* Define to 1 if you have the <setjmp.h> header file. */
#define HAVE_SETJMP_H 1

/* Define to 1 if you have the `sigaction' function. */
/* #define HAVE_SIGACTION */

/* Define to 1 if you have the `sigaddset' function. */
/* #define HAVE_SIGADDSET */

/* Define to 1 if you have the `sigemptyset' function. */
/* #define HAVE_SIGEMPTYSET */

/* Define to 1 if you have the `signal' function. */
#define HAVE_SIGNAL 1

/* Define to 1 if you have the <signal.h> header file. */
#define HAVE_SIGNAL_H 1

/* Define to 1 if you have the `sigprocmask' function. */
/* #define HAVE_SIGPROCMASK */

/* Define to 1 if you have the `sigrelse' function. */
/* #define HAVE_SIGRELSE */

/* Define to 1 if you have the `sigset' function. */
/* #define HAVE_SIGSET */

/* Define to 1 if you have the `srandom' function. */
/* #define HAVE_SRANDOM */

/* Define to 1 if you have the <stdint.h> header file. */
/* #define HAVE_STDINT_H */

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strchr' function. */
#define HAVE_STRCHR 1

/* Define to 1 if you have the `strcoll' function and it is properly defined.
   */
#define HAVE_STRCOLL 1

/* Define to 1 if you have the <strings.h> header file. */
/* #define HAVE_STRINGS_H */

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <stropts.h> header file. */
/* #define HAVE_STROPTS_H */

/* Define to 1 if you have the `strtol' function. */
#define HAVE_STRTOL 1

/* Define to 1 if you have the `strtoul' function. */
#define HAVE_STRTOUL 1

/* Define to 1 if you have the <syslog.h> header file. */
/* #define HAVE_SYSLOG_H */

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
/* #define HAVE_SYS_DIR_H */

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
/* #define HAVE_SYS_NDIR_H */

/* Define to 1 if you have the <sys/param.h> header file. */
/* #define HAVE_SYS_PARAM_H */

/* Define to 1 if you have the <sys/poll.h> header file. */
/* #define HAVE_SYS_POLL_H */

/* Define to 1 if you have the <sys/select.h> header file. */
/* #define HAVE_SYS_SELECT_H */

/* Define to 1 if you have the <sys/socket.h> header file. */
/* #define HAVE_SYS_SOCKET_H */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/un.h> header file. */
/* #define HAVE_SYS_UN_H */

#define HAVE_SYS_UTIME_H 1

/* Define to 1 if you have <sys/wait.h> that is POSIX.1 compatible. */
/* #define HAVE_SYS_WAIT_H */

/* Define to 1 if you have the <term.h> header file. */
/* #define HAVE_TERM_H */

/* Define to 1 if you have the `tmpfile' function. */
#define HAVE_TMPFILE 1

/* Define to 1 if you have the `truncate' function. */
/* #define HAVE_TRUNCATE */

/* Define to 1 if you have the `uname' function. */
/* #define HAVE_UNAME */

/* Define to 1 if the system has the type `union wait'. */
/* #define HAVE_UNION_WAIT */

/* Define to 1 if you have the <unistd.h> header file. */
/* #define HAVE_UNISTD_H */

/* Define to 1 if you have the `vfork' function. */
/* #define HAVE_VFORK */

/* Define to 1 if you have the <vfork.h> header file. */
/* #define HAVE_VFORK_H */

/* Define to 1 if you have the `wait' function. */
/* #define HAVE_WAIT */

/* Define to 1 if you have the `wait4' function. */
/* #define HAVE_WAIT4 */

/* Define to 1 if you have the `waitpid' function. */
/* #define HAVE_WAITPID */

/* Define to 1 if you have the <wchar.h> header file. */
#define HAVE_WCHAR_H 1

/* Define to 1 if `fork' works. */
/* #define HAVE_WORKING_FORK */

/* Define to 1 if `vfork' works. */
/* #define HAVE_WORKING_VFORK */

/* Always 1 because we use regex/regex.h */
#define HAVE_REGEX_H 1

/* Avoid OSX Conflict */
/* #undef Handle */

/* Default configuration value */
#define INBOX_NAME "INBOX"

/* Default configuration value */
#define INTERRUPTED_MAIL "intruptd"

/* Path to local inboxes for pico newmail check */
#undef MAILDIR

/* Default configuration value */
#define MAX_FILLCOL 80

/* Default configuration value */
#define MAX_SCREEN_COLS 500

/* Default configuration value */
#define MAX_SCREEN_ROWS 200

/* File mode used to set readonly access */
#undef MODE_READONLY

/* Compile in mouse support */
#define MOUSE
/* Windows has this set */
#define EX_MOUSE

/* Disallow users changing their From address */
/* #undef NEVER_ALLOW_CHANGING_FROM */

/* Disable keyboard lock support */
/* #undef NO_KEYBOARD_LOCK */

/* Default configuration value */
#define NUMDEBUGFILES 4

/* Name of package */
#define PACKAGE "alpine"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "alpine-contact@u.washington.edu"

/* Define to the full name of this package. */
#define PACKAGE_NAME "alpine"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "alpine 2.01"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "alpine"

/* Define to the version of this package. */
#define PACKAGE_VERSION "2.01"

/* Program users use to change their password */
/* #undef PASSWD_PROG */

/* Define if system supports POSIX signal interface */
/* #define POSIX_SIGNALS */

/* Default configuration value */
#define POSTPONED_MAIL "postpone"

/* Default configuration value */
#define POSTPONED_MSGS "postpond"

/* Default Trash folder for Web Alpine */
#define TRASH_FOLDER "Trash"

/* Define as the return type of signal handlers (`int' or `void'). */
#define RETSIGTYPE void

/* Define to the type of arg 1 for `select'. */
#define SELECT_TYPE_ARG1 int

/* Define to the type of args 2, 3 and 4 for `select'. */
#define SELECT_TYPE_ARG234 (fd_set *)

/* Define to the type of arg 5 for `select'. */
#define SELECT_TYPE_ARG5 (struct timeval *)

/* Local mail submission agent */
/* #define SENDMAIL */

/* Local MSA flags for SMTP on stdin/stdout */
/* #define SENDMAILFLAGS */

/* Posting agent to use when no nntp-servers defined */
/* #define SENDNEWS */

/* The size of a `unsigned int', as computed by sizeof. */
/* #define SIZEOF_UNSIGNED_INT */

/* The size of a `unsigned long', as computed by sizeof. */
/* #define SIZEOF_UNSIGNED_LONG */

/* The size of a `unsigned short', as computed by sizeof. */
/* #define SIZEOF_UNSIGNED_SHORT */

/* Program pico uses to check spelling */
#define SPELLER

/* Define to 1 if the `S_IS*' macros in <sys/stat.h> do not work properly. */
/* #define STAT_MACROS_BROKEN */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Default configuration value */
#define SYSTEM_PINERC "pinerc"

/* Default configuration value */
/* #define SYSTEM_PINERC_FIXED */

/* Pine-Centric Host Specifier */
#define SYSTYPE "WNT"

/* Define if system supports SYSV signal interface */
/* #define SYSV_SIGNALS */

/* File name separator as string constant */
#define S_FILESEP "\\"

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
/* #define TIME_WITH_SYS_TIME */

/* Define to 1 if your <sys/time.h> declares `struct tm'. */
/* #define TM_IN_SYS_TIME */

/* System defined unsigned 16 bit integer */
/* #define UINT16 uint16_t */
/* #define UINT16 unsigned int */

/* System defined unsigned 32 bit integer */
/* #define UINT32 uint32_t */
/* #define UINT32 unsigned long */

/* Compile in quota check on startup */
/* #define USE_QUOTAS */

/* Enable internal utf8 handling */
#define UTF8_INTERNAL

/* Version number of package */
#define VERSION "2.01"

/* Windows is just too different */
#ifndef _WINDOWS
#define _WINDOWS
#endif

/* Define to `int' if <sys/types.h> doesn't define. */
/* #define gid_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #define mode_t */

/* Define to `int' if <sys/types.h> does not define. */
#define pid_t int

/* qsort compare function argument type */
#define qsort_t void

/* Define to `unsigned' if <sys/types.h> does not define. */
/* #define size_t */

/* Define to `int' if <sys/types.h> doesn't define. */
/* #define uid_t */

/* Define as `fork' if `vfork' does not work. */
/* #define vfork */
