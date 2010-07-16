dnl Process this file with autoconf to produce a configure script.

dnl /* ========================================================================
dnl  * Copyright 2006-2008 University of Washington
dnl  *
dnl  * Licensed under the Apache License, Version 2.0 (the "License");
dnl  * you may not use this file except in compliance with the License.
dnl  * You may obtain a copy of the License at
dnl  *
dnl  *     http://www.apache.org/licenses/LICENSE-2.0
dnl  *
dnl  * ========================================================================
dnl  */

AC_PREREQ([2.57])

AC_REVISION([$Id: configure.ac 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $])

dnl Alpine Version Number is in $srcdir/VERSION
AC_INIT(alpine, m4_normalize(m4_include([VERSION])), [alpine-contact@u.washington.edu])

AC_CONFIG_SRCDIR([include/system.h])
AC_CONFIG_HEADERS([include/config.h])

AM_INIT_AUTOMAKE([foreign nostdinc])

AM_MAINTAINER_MODE

AC_CANONICAL_HOST

AC_LANG(C)

AC_MSG_NOTICE([Configuring for $PACKAGE_STRING ($host))])

# start out with intent to build Web Alpine
WEB_BUILD=web/src/alpined.d

dnl CHECK PROGRAMS

AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_LN_S
AC_PROG_AWK
AC_PROG_RANLIB
AC_PROG_LIBTOOL

AC_PATH_PROG(AR, ar, ar)
AC_PATH_PROG(RM, rm, rm)
AC_PATH_PROG(CP, cp, cp)
AC_PATH_PROG(LN, ln, ln)
AC_PATH_PROG(SED, sed, sed)
AC_PATH_PROG(MAKE, make)

dnl COMPILE-TIME OPTIONS

AM_GNU_GETTEXT_VERSION([0.16.1])
AM_GNU_GETTEXT([external])

dnl enable dmalloc per http://dmalloc.com
dnl NOTE: does not check c-client
AC_MSG_CHECKING([option: dmalloc enabled])
AC_ARG_ENABLE(dmalloc, AC_HELP_STRING([--enable-dmalloc],[Enable dmalloc debugging]))
if test x$enable_dmalloc = "xyes" ; then
  AC_MSG_RESULT([yes])
else
  AC_MSG_RESULT([no])
fi

AC_ARG_WITH(dmalloc-dir,
  AC_HELP_STRING([--with-dmalloc-dir=DIR], [Root of dmalloc lib/include path]),
  [
    if test "x$withval" != "xno" ; then
      enable_dmalloc = "yes"
      CPPFLAGS="$CPPCFLAGS -I${withval}"
      LDFLAGS="$LDFLAGS -L${withval}"
    fi
  ])

if test x$enable_dmalloc = "xyes" ; then
  AC_DEFINE(ENABLE_DMALLOC, 1, [Define enable dmalloc debugging])
fi

localedir="\${datadir}/locale"
AC_ARG_WITH(localedir,
  AC_HELP_STRING([--with-localedir=DIR], [Name of gettext locale directory]),
  [
    case $withval in
      yes)
        ;;
      no)
        ;;
      *)
        localedir=$withval
        ;;
    esac
  ])
AC_SUBST(localedir, "[$localedir]")

# Setup OS-Specific features
case "$host" in
  *darwin*)
    dnl OS X Universal Binary Support
    AC_ARG_ENABLE(osx-universal-binaries,
      AC_HELP_STRING([--enable-osx-universal-binaries],[Produce universal binaries under OS X [[default=no]]]))
    if test "x$enable_osx_universal_binaries" = "xyes" ; then
      if test "x$enable_dependency_tracking" != xno ; then
	AC_MSG_ERROR([--enable-osx-universal-binary requires --disable-dependency-tracking.
Please re-run configure with these options:
    --disable-dependency-tracking --enable-osx-universal-binary])
      fi
      if [test -d /Developer/SDKs/MacOSX10.5.sdk] ; then
        alpine_sysroot=/Developer/SDKs/MacOSX10.5.sdk
      elif [test -d /Developer/SDKs/MacOSX10.4u.sdk] ; then
        alpine_sysroot=/Developer/SDKs/MacOSX10.4u.sdk
      else
	AC_MSG_ERROR([No suitable MacOSX SDK found.  Make sure Xcode tools are installed])
      fi
      ub_cflags="-isysroot $alpine_sysroot -arch ppc -arch i386"
      ub_ldflags="-Wl,-syslibroot,$alpine_sysroot -arch ppc -arch i386"
      AM_CFLAGS="$AM_CFLAGS $ub_cflags"
      AM_LDFLAGS="$AM_LDFLAGS $ub_ldflags"
      alpine_c_client_cflags="$alpine_c_client_cflags $ub_cflags"
      alpine_c_client_ldflags="$alpine_c_client_ldflags $ub_ldflags"
    fi
    ;;
esac

AC_ARG_WITH(include-path,
  AC_HELP_STRING([--with-include-path=PATHS], [Colon-separated list of directories used for include file search]),
  [
    case $withval in
      no)
        ;;
      yes)
        ;;
      *)
	new_cppflags="-I`echo ${withval} | ${SED} 's/:/ -I/g'`"
	CPPFLAGS="$CPPFLAGS ${new_cppflags}"
	alpine_c_client_cflags="$alpine_c_client_cflags ${new_cppflags}"
	;;
    esac
  ])

AC_ARG_WITH(lib-path,
  AC_HELP_STRING([--with-lib-path=PATHS], [Colon-separated list of directories used for library search]),
  [
    case $withval in
      no)
        ;;
      yes)
        ;;
      *)
      	new_ldflags="-L`echo ${withval} | ${SED} 's/:/ -L/g'`"
      	LDFLAGS="$LDFLAGS $new_ldflags"
        alpine_c_client_ldflags="$alpine_c_client_ldflags ${new_ldflags}"
	;;
    esac
  ])

AC_ARG_WITH(pubcookie,
  AC_HELP_STRING([--with-pubcookie], [Include support for UW-Pubcookie Web Authentication]),
  [
    if test "x$withval" != "xno" ; then
      WEB_PUBCOOKIE_BUILD=web/src/pubcookie
    fi
  ])


AC_ARG_WITH(web-bin,
  AC_HELP_STRING([--with-web-bin=PATH], [Directory to hold Web Alpine component binary files]),
  [
    case "$withval" in
      no)
	;;
      yes)
	;;
      *)
	WEB_BINDIR=$withval
	;;
    esac
  ])

dnl disable debug, turned on by default
AC_MSG_CHECKING([option: debugging is enabled])
AC_ARG_ENABLE(debug, AC_HELP_STRING([--disable-debug],[Exclude debug messages from source]))
if test x$enable_debug != "xno" ; then
  AM_CFLAGS="$AM_CFLAGS -g"
  AC_DEFINE([DEBUG], [1], [Compile in debugging])
  AC_DEFINE([DEBUGJOURNAL], [1], [Display debug messages in journal])
  AC_MSG_RESULT([yes])
else
dnl ??? set AM_CFLAGS to optimize ???
  AC_MSG_RESULT([no])
fi

dnl disable optimization, on by default
AC_MSG_CHECKING([option: optimization is enabled])
AC_ARG_ENABLE(optimization, AC_HELP_STRING([--disable-optimization],[Exclude optimizing compiler flags]))
if test x$enable_optimization != "xno" ; then
  AC_MSG_RESULT([yes])
else
  CFLAGS="`echo $AM_CFLAGS | ${SED} 's/-O2//'`"
  alpine_c_client_gccoptlevel="-O0"
  AC_MSG_RESULT([no])
fi

dnl disable mouse support
AC_MSG_CHECKING([option: mouse support enabled])
AC_ARG_ENABLE(mouse, AC_HELP_STRING([--disable-mouse], [Disable mouse support]))
if test x$enable_mouse != "xno" ; then
  AC_DEFINE([MOUSE], [], [Compile in mouse support])
  AC_MSG_RESULT([yes])
else
   AC_MSG_RESULT([no])
fi

dnl enable quotas
AC_MSG_CHECKING([option: quotas enabled])
AC_ARG_ENABLE(quotas, AC_HELP_STRING([--enable-quotas],[Enable disk quota checking on startup]))
if test x$enable_quotas = "xyes" ; then
  AC_DEFINE([USE_QUOTAS], [], [Compile in quota check on startup])
  AC_MSG_RESULT([yes])
else
   AC_MSG_RESULT([no])
fi

AC_MSG_CHECKING([option: From changing enabled])
AC_ARG_ENABLE(from_changing, AC_HELP_STRING([--disable-from-changing],[Disallow users changing From addresss]))
if test x$enable_from_changing != "xno" ; then
  AC_MSG_RESULT([yes])
else
  AC_DEFINE([NEVER_ALLOW_CHANGING_FROM], [], [Disallow users changing their From address])
  AC_MSG_RESULT([no])
fi

dnl enable background posting support
AC_MSG_CHECKING([option: background post enabled])
AC_ARG_ENABLE(background-post, AC_HELP_STRING([--disable-background-post],[Disable background posting]))
if test x$enable_background_post != "xno" ; then
  AC_DEFINE([BACKGROUND_POST], [], [Enable background posting support])
  AC_MSG_RESULT([yes])
else
    AC_MSG_RESULT([no])
fi

dnl enable keyboard locking support
AC_MSG_CHECKING([option: keyboard lock enabled])
AC_ARG_ENABLE(keyboard-lock, AC_HELP_STRING([--disable-keyboard-lock],[Disable keyboard locking]))
if test x$enable_keyboard_lock != "xno" ; then
  AC_DEFINE([KEYBOARD_LOCK], [], [Enable keyboard lock support])
  AC_MSG_RESULT([yes])
else
  AC_MSG_RESULT([no])
fi

dnl enable from encoding support
AC_MSG_CHECKING([option: from encoding enabled])
AC_ARG_ENABLE(from-encoding, AC_HELP_STRING([--enable-from-encoding],[Enable From encoding in sent messages]))
if test x$enable_from_encoding = "xyes" ; then
  AC_DEFINE([ENCODE_FROMS], [], [Enable From address encoding in sent messages])
  AC_MSG_RESULT([yes])
else
  AC_MSG_RESULT([no])
fi

dnl OPTION: name of local submission agent
dnl         Might not be sendmail, but it MUST speak SMTP on stdin/stdout
AC_ARG_WITH(smtp-msa,
  AC_HELP_STRING([--with-smtp-msa=PATH],[Local Mail Submission Agent (sendmail)]),
    [
      case "$withval" in
	no)
	  ;;
	yes)
	  AC_PATH_PROG(SENDMAIL, sendmail, "", $PATH:/usr/sbin:/usr/lib)
	  ;;
	*)
	  SENDMAIL=$withval
	  ;;
      esac
    ],
    [
	  AC_PATH_PROG(SENDMAIL, sendmail, "", $PATH:/usr/sbin:/usr/lib)
    ])
if test -n "$SENDMAIL" ; then
    AC_DEFINE_UNQUOTED([SENDMAIL], "$SENDMAIL", [Local mail submission agent])
fi

dnl OPTION: local msa arguments
smtp_msa_flags="-bs -odb -oem"
AC_ARG_WITH(smtp-msa-flags,
  AC_HELP_STRING([--with-smtp-msa-flags=FLAGS],[MSA flags for SMTP on stdin/stdout (-bs -odb -oem)]),
  [
    if test "x$withval" != "xno" ; then
      smtp_msa_flags=$withval
    fi
  ])
AC_DEFINE_UNQUOTED([SENDMAILFLAGS], "$smtp_msa_flags", [Local MSA flags for SMTP on stdin/stdout])

dnl OPTION: name of local news posting agent and flags
npa="inews"
AC_ARG_WITH(npa,
  AC_HELP_STRING([--with-npa=PATH],[Posting agent when no nntp-servers defined (inews)]),
    [
      case "$withval" in
	no)
	  ;;
	yes)
	  AC_PATH_PROG(NPA_PROG, inews, "", $PATH:/usr/sbin:/usr/lib)
	  ;;
	*)
	  NPA_PROG=$withval
	  ;;
      esac
    ],
    [
	  AC_PATH_PROG(NPA_PROG, inews, "", $PATH:/usr/sbin:/usr/lib)
    ])

npa_flags="-h"
AC_ARG_WITH(npa-flags,
  AC_HELP_STRING([--with-npa-flags=FLAGS],[Flags to allow posting via local agent (-h)]),
  [
    if test "x$withval" != "xno" ; then
      npa_flags=$withval
    fi
  ])
if test -n "$NPA_PROG" ; then
  AC_DEFINE_UNQUOTED([SENDNEWS], "$NPA_PROG $npa_flags", [Posting agent to use when no nntp-servers defined])
fi

dnl OPTION: password changing program
AC_ARG_WITH(password-prog,
  AC_HELP_STRING([--with-password-prog=PATH],[Password change program (/bin/passwd)]),
    [
      case "$withval" in
	no)
	  ;;
	yes)
	  AC_PATH_PROG(PWPROG, passwd, "", $PATH:/usr/sbin:/usr/lib)
	  ;;
	*)
	  AC_PATH_PROG(PWPROG, $withval, "", $PATH:/usr/sbin:/usr/lib)
	  ;;
      esac
    ],
    [
	  AC_PATH_PROG(PWPROG, passwd, "", $PATH:/usr/sbin:/usr/lib)
    ])
if test -n "$PWPROG" ; then
    AC_DEFINE_UNQUOTED([PASSWD_PROG], "$PWPROG", [Program users use to change their password])
fi

dnl OPTION: basic spell checking program
AC_ARG_WITH(simple-spellcheck,
  AC_HELP_STRING([--with-simple-spellcheck=PROG],[Spellcheck program reads stdin, emits misspellings on stdout]),
    [
      if test "x$withval" != "xno" ; then
        SPELLPROG=$withval
      fi
    ],
    [
      AC_CHECK_PROG([SPELLPROG], [hunspell], [hunspell], [])
      if test -z "$SPELLPROG" ; then
        AC_CHECK_PROG([SPELLPROG], [aspell], [aspell], [])
        if test -z "$SPELLPROG" ; then
          AC_CHECK_PROG([SPELLPROG], [ispell], [ispell], [])
          if test -z "$SPELLPROG" ; then
           SPELLPROG="spell"
          fi
        fi
      fi
    ])

if test "x$SPELLPROG" != "xno" ; then
  AC_PATH_PROG(alpine_simple_spellcheck, $SPELLPROG)
  if test -n "$alpine_simple_spellcheck" ; then
    case "$SPELLPROG" in
      hunspell)
        alpine_simple_spellcheck="$alpine_simple_spellcheck -l"
        ;;
      aspell)
        alpine_simple_spellcheck="$alpine_simple_spellcheck --dont-backup --mode=email list"
        ;;
      ispell)
        alpine_simple_spellcheck="$alpine_simple_spellcheck -l"
        ;;
      *)
        ;;
    esac
  fi
fi

dnl OPTION: interactive spell checking program
AC_ARG_WITH(interactive-spellcheck,
  AC_HELP_STRING([--with-interactive-spellcheck=PROG],[Interactive, filewise spell checker]),
    [
      if test "x$withval" != "xno" ; then
        ISPELLPROG=$withval
      fi
    ],
    [
      AC_CHECK_PROG([ISPELLPROG], [hunspell], [hunspell], [])
      if test -z "$ISPELLPROG" ; then
        AC_CHECK_PROG([ISPELLPROG], [aspell], [aspell], [])
        if test -z "$SPELLPROG" ; then
          ISPELLPROG="ispell"
        fi
      fi
    ])

if test "x$ISPELLPROG" != "xno" ; then
  AC_PATH_PROG(alpine_interactive_spellcheck, $ISPELLPROG)
  if test -n "$alpine_interactive_spellcheck" ; then
    case "$ISPELLPROG" in
      aspell)
        alpine_interactive_spellcheck="$alpine_interactive_spellcheck --dont-backup --mode=email check"
        ;;
      *)
        ;;
    esac
  fi
fi

if test -n "$alpine_interactive_spellcheck" ; then
    AC_DEFINE_UNQUOTED([DF_VAR_SPELLER], "$alpine_interactive_spellcheck", [Interactive, filewise spell checker])
fi

if test -z "$alpine_simple_spellcheck" -a -n "$alpine_interactive_spellcheck" ; then
  alpine_simple_spellcheck=test
fi
AC_DEFINE_UNQUOTED([SPELLER], "$alpine_simple_spellcheck", [Simple spell checker: reads stdin, emits misspellings on stdout])

dnl OPTION: system-pinerc
dnl NOTE: historically we used /lib for the config dir.
dnl       don't ask, it was a long time ago. but, we can't
dnl       change it now without breaking compatibility
case "$prefix" in
  NONE) dpv=/usr/local/lib/pine.conf ;;
  *) dpv=${prefix}/lib/pine.conf ;;
esac
AC_ARG_WITH(system-pinerc,
  AC_HELP_STRING([--with-system-pinerc=VALUE], [System pinerc (/usr/local/lib/pine.conf)]),
    [
      case "$withval" in
	no)
	  ;;
	yes)
	  ;;
	*)
	  dpv=$withval
	  ;;
      esac
    ])
AC_DEFINE_UNQUOTED(SYSTEM_PINERC, "$dpv", [System pinerc])

dnl OPTION: system-fixed-pinerc
dnl NOTE: historically we used /lib for the config dir.
dnl       don't ask, it was a long time ago. but, we can't
dnl       change it now without breaking compatibility
case "$prefix" in
  NONE) dpv=/usr/local/lib/pine.conf.fixed ;;
  *) dpv=${prefix}/lib/pine.conf.fixed ;;
esac
AC_ARG_WITH(system-fixed-pinerc,
  AC_HELP_STRING([--with-system-fixed-pinerc=VALUE], [System fixed pinerc (/usr/local/lib/pine.conf.fixed)]),
    [
      case "$withval" in
	no)
	  ;;
	yes)
	  ;;
	*)
	  dpv=$withval
	  ;;
      esac
    ])
AC_DEFINE_UNQUOTED(SYSTEM_PINERC_FIXED, "$dpv", [System fixed pinerc])

dnl Function to simplify setting alpine/pico defaults
dnl usage: PINEVAR(configure-name, definition-name, definition-value, help)
AC_DEFUN([PINEVAR],
  [
    dpv=$3
    AC_ARG_WITH($1,
      AC_HELP_STRING(--with-$1=VALUE, [$4 ($3)]),
      [
	if test "x$withval" != "xno" ; then
	  dpv=$withval
	fi
      ])
    AC_DEFINE_UNQUOTED($2, "$dpv", [Default configuration value])
  ])

dnl Function to simplify setting pine/pico defaults
dnl usage: PINEVAR_UNQUOTED(configure-name, definition-name, definition-value, help)
AC_DEFUN([PINEVAR_UNQUOTED],
  [
    dpv=$3
    AC_ARG_WITH($1,
      AC_HELP_STRING(--with-$1=VALUE, [$4 ($3)]),
      [
	if test "x$withval" != "xno" ; then
	  dpv=$withval
	fi
      ])
    AC_DEFINE_UNQUOTED($2, $dpv, [Default configuration value])
  ])

PINEVAR(mailcheck-interval, DF_MAILCHECK, 150, [Specify default mail-check-interval])
PINEVAR_UNQUOTED(checkpoint-interval, CHECK_POINT_TIME, 420, [Specify default checkpoint-interval])
PINEVAR_UNQUOTED(checkpoint-frequency, CHECK_POINT_FREQ, 12, [State change count before checkpoint])
PINEVAR_UNQUOTED(display-rows, DEFAULT_LINES_ON_TERMINAL, 24, [Initial rows on display])
PINEVAR_UNQUOTED(display-columns, DEFAULT_COLUMNS_ON_TERMINAL, 80, [Initial columns on display])
PINEVAR_UNQUOTED(max-display-rows, MAX_SCREEN_ROWS, 200, [Maximum display rows])
PINEVAR_UNQUOTED(max-display-columns, MAX_SCREEN_COLS, 500, [Maximum display columns])
PINEVAR(fill-column, DF_FILLCOL, 74, [Default fill column])
PINEVAR_UNQUOTED(max_fill-column, MAX_FILLCOL, 80, [Maximum fill column])
PINEVAR_UNQUOTED(debug-level, DEFAULT_DEBUG, 2, [Specify default debug verbosity level])
PINEVAR_UNQUOTED(debug-files, NUMDEBUGFILES, 4, [Specify number of debug files])
PINEVAR(debug-file, DEBUGFILE, [.pine-debug], [Specify debug file name])
PINEVAR(forwarded-keyword, FORWARDED_FLAG, ["\$Forwarded"], [IMAP (c-client) keyword to store forwarded status])
PINEVAR(display-overlap, DF_OVERLAP, [2], [Lines preserved while paging])
PINEVAR(display-margin, DF_MARGIN, [0], [Lines visible while scrolling])
PINEVAR(default-fcc, DF_DEFAULT_FCC, [sent-mail], [Default sent mail folder])
PINEVAR(default-save-folder, DEFAULT_SAVE, [saved-messages], [Default save folder])
PINEVAR(default-legacy-postponed-folder, POSTPONED_MAIL, [postponed-mail], [Pre Pine 3.90 postponed folder])
PINEVAR(default-postponed-folder, POSTPONED_MSGS, [postponed-msgs], [Default postponed folder])
PINEVAR(default-trash-folder, TRASH_FOLDER, [Trash], [Default Trash folder for Web Alpine])
PINEVAR(default-interrupted-mail, INTERRUPTED_MAIL, [.pine-interrupted-mail], [Default folder for interrupted mail])
PINEVAR(default-dead-letter-folder, DEADLETTER, [dead.letter], [Default dead letter folder])
PINEVAR(default-mail-directory, DF_MAIL_DIRECTORY, [mail], [Default mail directory])
PINEVAR(default-inbox-name, INBOX_NAME, [INBOX], [Default inbox name])
PINEVAR(default-signature-file, DF_SIGNATURE_FILE, [.signature], [Default signature file])
PINEVAR(default-elm-style-save, DF_ELM_STYLE_SAVE, [no], [Default to Elm style save])
PINEVAR(default-header-in-reply, DF_HEADER_IN_REPLY, [no], [Include header in reply])
PINEVAR(default-old-style-reply, DF_OLD_STYLE_REPLY, [no], [Default to old style reply])
PINEVAR(default-use-only-domain-name, DF_USE_ONLY_DOMAIN_NAME, [no], [Default to using only the domain name])
PINEVAR(default-save-by-sender, DF_SAVE_BY_SENDER, [no], [Default to save by sender])
PINEVAR(default-sort-key, DF_SORT_KEY, [arrival], [Default sort key])
PINEVAR(default-addressbook-sort-rule, DF_AB_SORT_RULE, [fullname-with-lists-last], [Default addressbook sort rule])
PINEVAR(default-folder-sort-rule, DF_FLD_SORT_RULE, [alphabetical], [Default folder sort rule])
PINEVAR(default-saved-message-name-rule, DF_SAVED_MSG_NAME_RULE, [default-folder], [Default saved message name rule])
PINEVAR(default-fcc-rule, DF_FCC_RULE, [default-fcc], [Default fcc rule])
PINEVAR(default-standard-printer, DF_STANDARD_PRINTER, [lpr], [Default standard printern])
PINEVAR(default-ansi-printer, ANSI_PRINTER, [attached-to-ansi], [ANSI printer definition])
PINEVAR(default-addressbook, DF_ADDRESSBOOK, [.addressbook], [Default addressbook name])
PINEVAR(default-local-fullname, DF_LOCAL_FULLNAME, ["Local Support"], [Default local support fullname])
PINEVAR(default-local-address, DF_LOCAL_ADDRESS, [postmaster], [Default local support address])
PINEVAR(default-keyboard-lock-count, DF_KBLOCK_PASSWD_COUNT, [1], [Default keyboard lock count])
PINEVAR(default-remote-addressbook-history, DF_REMOTE_ABOOK_HISTORY, [3], [Default address book history count])
PINEVAR(smime-public-cert-directory, DF_PUBLICCERT_DIR, [.alpine-smime/public], [Default Public Cert Directory])
PINEVAR(smime-private-key-directory, DF_PRIVATEKEY_DIR, [.alpine-smime/private], [Default Private Key Directory])
PINEVAR(smime-cacert-directory, DF_CACERT_DIR, [.alpine-smime/ca], [Default Cert Authority Directory])
PINEVAR_UNQUOTED(default-printer, DF_DEFAULT_PRINTER, [ANSI_PRINTER], [Default printer])

dnl set PASSFILE?
AC_ARG_WITH(passfile,
  AC_HELP_STRING([--with-passfile=FILENAME],[Password cache file (NOT secure, NOT recommended)]),
    [
      case "$withval" in
	no)
	  ;;
	yes)
	  ;;
	*)
	  alpine_PASSFILE=$withval
	  ;;
      esac
    ])

dnl os-specific credential cache?
AC_ARG_WITH(local-password-cache,
 AC_HELP_STRING([--without-local-password-cache],[Disable OS-specific password cache, if supported]),
   [
     alpine_os_credential_cache=$withval
   ])

dnl Particular os-specific credential cache?
AC_ARG_WITH(local-password-cache-method,
 AC_HELP_STRING([--with-local-password-cache-method],[OS-specific credential cache (OSX=APPLEKEYCHAIN, Windows=WINCRED)]),
   [
     alpine_os_credential_cache_method=$withval
   ])

if test -n "$alpine_PASSFILE" ; then
  case $alpine_os_credential_cache in
    no)
      ;;
    *)
      alpine_os_credential_cache="no"
      AC_MSG_NOTICE([--with-passfile definition overrides OS-Specific password caching])
      ;;
  esac
  AC_DEFINE_UNQUOTED([PASSFILE], "$alpine_PASSFILE", [Password cache file (NOT secure. NOT recommended)])
fi

dnl set DF_SSHPATH?
AC_ARG_WITH(default-sshpath,
  AC_HELP_STRING([--with-default-sshpath=FILENAME],[set default value of ssh command path (defining should cause ssh to be preferred to rsh)]),
    [
      case "$withval" in
	no)
	  ;;
	yes)
	  ;;
	*)
	  AC_DEFINE_UNQUOTED([DF_SSHPATH], "$withval", [set default value of ssh command path (defining should cause ssh to be preferred to rsh)])
	  ;;
      esac
    ])

dnl set DF_SSHCMD?
AC_ARG_WITH(default-sshcmd,
  AC_HELP_STRING([--with-default-sshcmd=PERCENT_S_STRING],[set default value of ssh command string (usually "%s %s -l %s exec /etc/r%sd")]),
    [
      case "$withval" in
	no)
	  ;;
	yes)
	  ;;
	*)
	  AC_DEFINE_UNQUOTED([DF_SSHCMD], "$withval", [set default value of ssh command string (usually "%s %s -l %s exec /etc/r%sd")])
	  ;;
      esac
    ])

dnl Include SSL?
dnl Set SSLDIR for c-client make
AC_ARG_WITH(ssl,
  AC_HELP_STRING([--without-ssl],[Disable SSL support (OpenSSL)]),
  [ with_ssl=$withval ])

if test "x$with_ssl" = "xno" ; then
  alpine_SSLTYPE="none"
else
  dnl preload c-client default locations/options
  case $host in
    *-linux-gnu)
      if test -f /etc/fedora-release -o -f /etc/redhat-release -o -f /etc/redhat_version ; then  
        alpine_SSLTYPE="nopwd"
	if test -d /etc/pki/tls ; then  
	  alpine_SSLDIR="/etc/pki/tls"
	else
	  alpine_SSLDIR="/usr/share/ssl"
	fi
      elif test -f /etc/SuSE-release ; then
        alpine_SSLTYPE="nopwd"
        alpine_SSLDIR="/usr/share/ssl"
      elif test -d /etc/osso-af-init ; then
        alpine_SSLTYPE="nopwd"
        alpine_SSLDIR="/usr"
        alpine_SSLCERTS="/usr/share/certs"
      else
        alpine_SSLTYPE="nopwd"
        alpine_SSLDIR="/usr"
        alpine_SSLCERTS="/etc/ssl/certs"
      fi
      ;;
    *-apple-darwin*)
      alpine_SSLTYPE="nopwd"
      alpine_SSLCERTS="/System/Library/OpenSSL/certs"
      ;;
    *-openbsd*)
      alpine_SSLTYPE="nopwd"
      alpine_SSLDIR="/usr"
      alpine_SSLCERTS="/etc/ssl/certs"
      ;;
    *-sco-sysv* | *-sysv*UnixWare | *-sysv*OpenUNIX)
      alpine_SSLTYPE="sco.nopwd"
      alpine_SSLDIR=/usr/local/ssl
      ;;
    *-*-solaris*)
      if test -d /usr/sfw/include/openssl ; then
        alpine_SSLDIR="/usr/sfw"
      elif test -d /opt/csw/include/openssl ; then
        alpine_SSLDIR="/opt/csw"
	if test -d /opt/csw/ssl/certs ; then
	  alpine_SSLCERTS="/opt/csw/ssl/certs"
	fi
      fi
      if test -z "$alpine_SSLCERTS" -a -d /etc/certs ; then
        alpine_SSLCERTS="/etc/certs"
      fi
      ;;
    *)
      alpine_SSLTYPE="nopwd"
      alpine_SSLDIR=/usr/local/ssl
      ;;
  esac

  AC_ARG_WITH(ssl-dir,
    AC_HELP_STRING([--with-ssl-dir=DIR], [Root of SSL lib/include path]),
    [
      if test "x$withval" != "xno" ; then
	alpine_SSLDIR=$withval
      fi
    ])

  AC_ARG_WITH(ssl-certs-dir,
    AC_HELP_STRING([--with-ssl-certs-dir=DIR], [Path to SSL certificate directory]),
    [
      if test "x$withval" != "xno" ; then
	alpine_SSLCERTS=$withval
      fi
    ])

  AC_ARG_WITH(ssl-include-dir,
    AC_HELP_STRING([--with-ssl-include-dir=DIR], [SSL include file path]),
    [
      if test "x$withval" != "xno" ; then
	alpine_SSLINCLUDE=$withval
      fi
    ])

  AC_ARG_WITH(ssl-lib-dir,
    AC_HELP_STRING([--with-ssl-lib-dir=DIR], [SSL library path]),
    [
      if test "x$withval" != "xno" ; then
	alpine_SSLLIB=$withval
      fi
    ])
  dnl setup globals so tests below work
  if test -n "$alpine_SSLINCLUDE" ; then
    CPPCFLAGS="-I$alpine_SSLINCLUDE $CPPFLAGS"
  elif test -n "$alpine_SSLDIR" ; then
    CPPFLAGS="-I${alpine_SSLDIR}/include $CPPFLAGS"
  fi
  if test -n "$alpine_SSLLIB" ; then
    LDFLAGS="-L$alpine_SSLLIB $LDFLAGS"
  elif test -n "$alpine_SSLDIR" ; then
    LDFLAGS="-L${alpine_SSLDIR}/lib $LDFLAGS"
  fi
fi

dnl Include Kerberos?
dnl Set GSSDIR for c-client make
AC_ARG_WITH(krb5,
  AC_HELP_STRING([--without-krb5],[Disable Kerberos support]),
  [ with_krb5=$withval ])

if test "x$with_krb5" = "xno" ; then
  alpine_GSSTYPE="none"
else
  alpine_GSSTYPE=

  AC_ARG_WITH(krb5-dir,
    AC_HELP_STRING([--with-krb5-dir=DIR], [Root of Kerberos lib/include path]),
    [
      if test "x$withval" != "xno" ; then
	CPPFLAGS="$CPPFLAGS -I${withval}/include"
	LDFLAGS="$LDFLAGS -L${withval}/lib"
      fi
    ])

  AC_ARG_WITH(krb5-include-dir,
    AC_HELP_STRING([--with-krb5-include-dir=DIR], [Kerberos include file path]),
    [
      if test "x$withval" != "xno" ; then
	CPPFLAGS="$CPPFLAGS -I$withval"
      fi
    ])

  AC_ARG_WITH(krb5-lib-dir,
    AC_HELP_STRING([--with-krb5-lib-dir=DIR], [Kerberos library path]),
    [
      if test "x$withval" != "xno" ; then
	LDFLAGS="$LDFLAGS -L$withval"
      fi
    ])
fi

dnl Include LDAP?
AC_ARG_WITH(ldap,
  AC_HELP_STRING([--without-ldap],[Disable LDAP query support]),
  [ with_ldap=$withval ])

if test "x$with_ldap" = "xno" ; then
  alpine_with_ldap=
  AC_MSG_NOTICE([Excluding LDAP Support])
else
  dnl Do stuff to figure out where OpenLDAP is

  alpine_with_ldap=yes
  AC_ARG_WITH(ldap-dir,
    AC_HELP_STRING([--with-ldap-dir=DIR], [Root of LDAP lib/include path]),
    [
      if test "x$withval" != "xno" ; then 
	CPPFLAGS="$CPPFLAGS -I${withval}/include"
	LDFLAGS="$LDFLAGS -L${withval}/lib"
      fi
    ])

  AC_ARG_WITH(ldap-include-dir,
    AC_HELP_STRING([--with-ldap-include-dir=DIR], [Directory containing LDAP include files]),
    [
      if test "x$withval" != "xno" ; then
	CPPFLAGS="$CPPFLAGS -I$withval"
      fi
    ])

  AC_ARG_WITH(ldap-lib-dir,
    AC_HELP_STRING([--with-ldap-lib-dir=DIR], [LDAP library path]),
    [
      if test "x$withval" != "xno" ; then
	LDFLAGS="$LDFLAGS -L$withval"
      fi
    ])
fi

dnl Include SMIME?
AC_ARG_WITH(smime,
  AC_HELP_STRING([--without-smime],[Disable S/MIME]),
  [ with_smime=$withval ])

dnl Include TCL?
AC_ARG_WITH(tcl,
  AC_HELP_STRING([--without-tcl],[Disable TCL, thus Web Alpine support]),
  [ with_tcl=$withval ])

if test "x$with_tcl" = "xno" ; then
  WEB_BUILD=
  AC_MSG_NOTICE([Excluding TCL Support, and thus Web Alpine Support])
else
  AC_ARG_WITH(tcl-lib,
    AC_HELP_STRING([--with-tcl-lib=LIBRARY], [Specific TCL Library, like \"tcl8.4\"]),
    [
      if test "x$withval" != "xno" ; then 
	alpine_TCLLIB=$withval
      fi
    ])
  AC_ARG_WITH(tcl-include,
    AC_HELP_STRING([--with-tcl-include=DIR], [Directory containing TCL include files]),
    [
      if test "x$withval" != "xno" ; then
        CPPFLAGS="$CPPFLAGS -I$withval"
	alpine_TCLINC=$withval
      fi
    ])
fi

AC_ARG_WITH(supplied-regex,
  AC_HELP_STRING([--with-supplied-regex],[Use regex library supplied with alpine]),
  [ alpine_REGEX=$withval ])

AC_ARG_WITH(pthread,
  AC_HELP_STRING([--without-pthread],[Do NOT test for nor build with POSIX thread support]),
  [ with_pthread=$withval ])

AC_ARG_WITH(system-mail-directory,
  AC_HELP_STRING([--with-system-mail-directory=DIR], [Directory where local mail is delivered]),
  [
    if test "x$withval" != "xno" ; then
      alpine_with_local_maildir="$withval"
    fi
  ])

AC_ARG_WITH(c-client-target,
  AC_HELP_STRING([--with-c-client-target=TARGET], [IMAP build target (see imap/Makefile)]),
  [
    if test "x$withval" != "xno" ;then
      alpine_with_c_client_target="$withval"
    fi
  ])


dnl Without IPv6?
AC_ARG_WITH(ipv6,
  AC_HELP_STRING([--without-ipv6],[Disable IPv6, primarily to work around resolver problems]),
  [ with_ipv6=$withval ])

if test "x$with_ipv6" = "xno" ; then
  AC_MSG_NOTICE([Excluding IPv6 Support])
  c_client_specials="${c_client_specials}IP6=4 "
  c_client_ip6="true"
else
  c_client_ip6="touch imap/ip6"
fi


dnl CHECK LIBRARIES

if test x$enable_dmalloc = "xyes" ; then
  if test "x$with_pthread" = "xyes" ; then
    dmalloc_lib=dmallocth
  else
    dmalloc_lib=dmalloc
  fi

  AC_CHECK_LIB($dmalloc_lib,dmalloc_shutdown,,
    [
      AC_ERROR([$dmalloc_lib requested, but -ldmalloc not found])
    ])
fi

dnl which terminal cabability database
AC_CHECK_LIB(tinfo, setupterm,
  [
    alpine_termdata=info
    LIBS="$LIBS -ltinfo"
  ],
  [
    AC_CHECK_LIB(ncurses, setupterm,
      [
        alpine_termdata=info
        LIBS="$LIBS -lncurses"
      ],
      [
        AC_CHECK_LIB(curses, setupterm,
          [
            alpine_termdata=info
            LIBS="$LIBS -lcurses"
          ],
          [
            AC_CHECK_LIB(termlib, tgetent,
              [
                alpine_termdata=cap
                LIBS="$LIBS -ltermlib"
              ],
              [
                AC_CHECK_LIB(termcap, tgetent,
                  [
                    alpine_termdata=cap
                    LIBS="$LIBS -ltermcap"
                  ],
                  [
                    AC_ERROR([Terminfo/termcap not found])
                  ])
              ])
          ])
      ])
  ])
case $alpine_termdata in
  info)
    AC_DEFINE(HAS_TERMINFO, [1], [Define if systems uses terminfo terminal database])
    ;;
  cap)
    AC_DEFINE(HAS_TERMCAP, [1], [Define if systems uses termcap terminal database])
    ;;
esac

dnl provide LDAP support?
if test "$alpine_with_ldap" = "yes" ; then
  alpine_has_ldap=
  AC_CHECK_LIB(lber, ber_alloc,
    [
      LIBS="$LIBS -llber"
    ])
  AC_SEARCH_LIBS(ldap_init,ldap,
    [
      alpine_has_ldap=yes
    ],
    [
      AC_SEARCH_LIBS(ldap_open,ldap,
	[
	  alpine_has_ldap=yes
	])
    ])

  if test "$alpine_has_ldap" = "yes" ; then
    AC_MSG_NOTICE([Including LDAP Support])
    AC_DEFINE([ENABLE_LDAP], [], [Enable LDAP query support])

    dnl we use deprecated functions (ldap_get_values)
    dnl OpenLDAP 2.3.x doesn't define LDAP_DEPRECATED by default like 2.2.x
    AC_MSG_CHECKING([if we should define LDAP_DEPRECATED])
    AC_RUN_IFELSE(
	[AC_LANG_SOURCE([[
#include <stdio.h>
#include <string.h>
#include <ldap.h>
int main(void) {

	if (LDAP_VENDOR_VERSION >= 20300)
		exit(0);

	exit(2);
}
	]])],
	[
		AC_MSG_RESULT(yes)
		AC_DEFINE(LDAP_DEPRECATED, 1,
		[Define if you use OpenLDAP 2.3.x deprecated functions])

	],
	[
		AC_MSG_RESULT(no)
	],
	[
		AC_MSG_WARN([cross compiling: not checking])
	]
    )

  else
    AC_MSG_NOTICE([Cannot find LDAP functions!  Excluding LDAP support.])
  fi
fi

dnl provide SSL support?
if test "x$alpine_SSLTYPE" != "xnone" ; then
  AC_SEARCH_LIBS(SSL_library_init,ssl,
    [
      LIBS="$LIBS -lssl"
    ])
  if test "x$alpine_SSLTYPE" = "xnone" ; then
    AC_MSG_NOTICE([OpenSSL libraries NOT found])
  else
    AC_MSG_NOTICE([OpenSSL libraries FOUND])
  fi
fi

dnl provide KRB5 support?
if test "x$alpine_GSSTYPE" != "xnone" ; then
  AC_SEARCH_LIBS(gss_init_sec_context,gss gssapi gssapi_krb5,
    [
      dnl MIT-based?
      AC_CHECK_HEADER([gssapi/gssapi_generic.h],,
	[
	  if test ! -d /usr/kerberos/include ; then
	    alpine_GSSTYPE="none"
	    alpine_gss_none_reason="header files not found"
	  fi
	])
    ],
    [
      alpine_GSSTYPE="none"
      alpine_gss_none_reason="libraries not found"
    ])
  if test -n "$alpine_gss_none_reason" ; then
    AC_MSG_NOTICE([NOT including Kerberos Support: $alpine_gss_none_reason])
  fi
fi

dnl check for tcl libraries for Web Alpine (HACKY)
if test -n "$WEB_BUILD" ; then
  AC_SEARCH_LIBS([Tcl_Eval],[$alpine_TCLLIB tcl8.4 tcl8.3 tcl84 tcl83 tcl],,
    [
      WEB_BUILD=
    ])

  dnl look for header file
  if test -n "$alpine_TCLINC" ; then
    AC_CHECK_HEADER($alpine_TCLINC,,
      [
	WEB_BUILD=
      ])
    if test -z "$WEB_BUILD" ; then
      AC_MSG_NOTICE([Tcl Include file NOT found])
    fi
  else
    AC_CHECK_HEADER(tcl.h,,
      [
	for dir in tcl8.4 tcl8.3 tcl84 tcl83 ; do
	  AC_CHECK_FILE([/usr/include/$dir/tcl.h],
	    [
	      found=yes
	    ])
	  if test "$found" = "yes" ; then
	    CPPFLAGS="$CPPFLAGS -I/usr/include/$dir"
	    break
	  fi
	done
      ])
  fi
fi

dnl Local or supplied regex?
if test x$alpine_REGEX != "xyes" ; then
  AC_SEARCH_LIBS([regcomp],posix regexp regex re,,
    [
      if test x$alpine_REGEX = "xno" ; then
        AC_ERROR([Unable to find system regex library])
      else
	alpine_REGEX=yes
      fi
    ])
fi
if test x$alpine_REGEX != "xyes" ; then
  AC_CHECK_HEADER([regex.h],,
    [
      if test x$alpine_REGEX = "xno" ; then
	AC_ERROR([Unable to find system regex include file])
      else
	alpine_REGEX=yes
      fi
    ])
fi
AC_DEFINE(HAVE_REGEX_H,1,[Regular expression header file exists])
if test x$alpine_REGEX = "xyes" ; then
  CPPFLAGS="$CPPFLAGS -I${top_builddir}/regex"
  LDFLAGS="$LDFLAGS -L${top_builddir}/regex -lregex"
  REGEX_BUILD=regex
fi

if test "x$with_pthread" != "xno" ; then
  AC_MSG_CHECKING([for pthread support])
  ACX_PTHREAD(
    [
      AC_MSG_RESULT([yes])
      case "$target" in
	  *openbsd*)
	    AC_MSG_NOTICE([WARNING: pthread support on OpenBSD is unstable!])
	    AM_CFLAGS="$AM_CFLAGS -pthread"
	    ;;
      esac
      LIBS="$PTHREAD_LIBS $LIBS"
      AM_CFLAGS="$AM_CFLAGS $PTHREAD_CFLAGS"
      CC="$PTHREAD_CC"
      AC_DEFINE([HAVE_PTHREAD], [1], [System has pthread support])
    ],
    [
      AC_MSG_RESULT([no])
    ])

  AC_SEARCH_LIBS([nanosleep], [rt posix4],
    [
      AC_DEFINE([HAVE_NANOSLEEP], [1], [Define if system supports subsecond, non-alarm sleep])
    ])
fi



dnl CHECK HEADERS

AC_HEADER_STDC
AC_HEADER_DIRENT
AC_HEADER_STAT
AC_HEADER_SYS_WAIT
AC_HEADER_TIME
AC_HEADER_TIOCGWINSZ

AC_UNISTD_H

AC_CHECK_HEADERS([errno.h \
		  ctype.h \
		  fcntl.h \
		  signal.h \
		  setjmp.h \
		  memory.h \
		  sys/param.h \
		  sys/socket.h \
		  sys/uio.h \
		  sys/un.h \
		  limits.h \
		  wchar.h \
		  sys/poll.h \
		  stropts.h \
		  netdb.h \
		  syslog.h \
		  sys/syslog.h \
		  locale.h \
		  langinfo.h \
		  utime.h \
		  sys/utime.h \
		  pthread.h \
		  pwd.h \
		  assert.h])

dnl terminal line discipline?
AC_CHECK_HEADER([termios.h],
  AC_DEFINE(HAS_TERMIOS, [1], [Define if systems uses termios terminal control]),
  [
    AC_CHECK_HEADER([termio.h],
      AC_DEFINE(HAS_TERMIO, [1], [Define if systems uses termio terminal control]),
      [
        AC_CHECK_HEADER([sgtty.h],
	  AC_DEFINE(HAS_SGTTY, [1], [Define if systems uses old BSD-style terminal control]),
	[
	  AC_ERROR([Unable to figure out terminal control method])
	])
      ])
  ])


dnl CHECK TYPEDEFS

AC_TYPE_SIGNAL
AC_TYPE_SIZE_T
AC_TYPE_MODE_T
AC_TYPE_PID_T
AC_TYPE_UID_T
AC_STRUCT_TM

AC_CHECK_TYPES([union wait])

AC_CHECK_HEADERS([stdint.h], [uint16=uint16_t], [
  AC_CHECK_HEADERS([inttypes.h], [uint16=uint16_t], [
    AC_CHECK_HEADERS([sys/types.h], [uint16=u_int16_t], [
      AC_CHECK_SIZEOF(unsigned short)
      if test $ac_cv_sizeof_unsigned_short -eq 2 ; then
	uint16="unsigned short"
      else
	AC_CHECK_SIZEOF(unsigned int)
	if $ac_cv_sizeof_unsigned_int -eq 2 ; then
	  uint16="unsigned int"
	else
	  AC_ERROR([Unable to determine 16 bit integer type])
        fi
      fi
    ])
  ])
])
AC_DEFINE_UNQUOTED([UINT16], $uint16, [System defined unsigned 16 bit integer])

AC_CHECK_HEADERS([stdint.h], [uint32=uint32_t], [
  AC_CHECK_HEADERS([inttypes.h], [uint32=uint32_t], [
    AC_CHECK_HEADERS([sys/types.h], [uint32=u_int32_t], [
      AC_CHECK_SIZEOF(unsigned int)
      if test $ac_cv_sizeof_unsigned_int -eq 4 ; then
	uint32="unsigned int"
      else
	AC_CHECK_SIZEOF(unsigned long)
	if $ac_cv_sizeof_unsigned_long -eq 4 ; then
	  uint32="unsigned long"
	else
	  AC_ERROR([Unable to determine 32 bit integer type])
        fi
      fi
    ])
  ])
])
AC_DEFINE_UNQUOTED([UINT32], $uint32, [System defined unsigned 32 bit integer])

AC_CACHE_CHECK(argument pointer type of qsort compare function and base,
ac_cv_func_qsort_argtype,
[AC_TRY_COMPILE([
#if	HAVE_STDLIB_H
#include "stdlib.h"
#endif

extern void *base;
extern sortf(const void *, const void *);
int sortf(a, b)
  const void *a;
  const void *b; { return 0; }
],[
qsort(base, 2, sizeof(char *), sortf);
],
	ac_cv_func_qsort_argtype=void, ac_cv_func_qsort_argtype=char)
])
AC_DEFINE_UNQUOTED([qsort_t], $ac_cv_func_qsort_argtype, [qsort compare function argument type])

dnl check for "struct passwd" ?

AC_FUNC_SELECT_ARGTYPES

AC_FUNC_STRCOLL

dnl CHECK FOR LIBRARY FUNCTIONS

AC_FUNC_FORK
AC_CHECK_FUNCS([strchr \
		memcpy \
		strtol \
		strtoul \
		select \
		poll \
		qsort \
	 	getuid \
		getpwuid \
		getpwnam \
	        gettimeofday \
		tmpfile \
		uname \
		rename \
		read \
		signal \
		setjmp \
		chown \
		wait4 \
		waitpid \
		wait \
		srandom \
		popen \
		pclose \
		fsync \
		truncate \
		listen \
		wcwidth \
		mbstowcs \
		wcrtomb \
		putenv \
		setenv])

AC_SEARCH_LIBS([gethostname],[nsl])

AC_SEARCH_LIBS([socket],[socket],,
  [
    WEB_BUILD=
  ])

AC_SEARCH_LIBS([bind],[bind],,
  [
    WEB_BUILD=
  ])

dnl check for POSIX signal interface
AC_CHECK_FUNCS([sigaction sigemptyset sigaddset sigprocmask],
  [
    AC_DEFINE([POSIX_SIGNALS], [], [Define if system supports POSIX signal interface])
  ],
  [
    AC_CHECK_FUNCS([sigset sigrelse],
      [
	AC_DEFINE([SYSV_SIGNALS], [], [Define if system supports SYSV signal interface])
      ])
  ])

AC_SEARCH_LIBS([syslog], [bsd socket inet],
  [
    AC_DEFINE([HAVE_SYSLOG], [1], [Define if system supplies syslog() logging])
  ])


dnl HOST-OS SPECIFIC DEFINITIONS
dnl Tests and assignments below are mostly to coax the appropriate
dnl build from c-client.  Most of this will go away when c-client
dnl adopts configure
case "$host" in
  *-linux-gnu*|*-k*bsd*-gnu*)
    alpine_path_delim="/"
    alpine_mode_readonly="(0600)"
    if test -f /etc/fedora-release ; then  
      systype="LFD"
      if test -d /etc/pki/tls ; then  
	alpine_c_client_target="lfd"
      else
	alpine_c_client_target="lrh"
      fi
    elif test -f /etc/mandrake-release ; then
      systype="LMD"
      alpine_c_client_target="lmd"
    elif test -f /etc/redhat-release -o -f /etc/redhat_version ; then
      systype="LRH"
      if test -d /etc/pki/tls ; then  
	alpine_c_client_target="lr5"
      else
	alpine_c_client_target="lrh"
      fi
    elif test -f /etc/debian_release -o -f /etc/debian_version ; then
      if test -d /etc/osso-af-init ; then
        systype="LN8"
        alpine_c_client_target="ln8"
      else
        systype="DEB"
        alpine_c_client_target="ldb"
      fi
    elif test -f /etc/SuSE-release ; then
      systype="LSU"
      alpine_c_client_target="lsu"
    else
      systype="LNX"
      AC_CHECK_LIB(pam, pam_start,
        [
          alpine_c_client_target="lnp"
        ],
        [
	  if test -f /etc/shadow ; then
	    alpine_c_client_target="slx"
	  else
	    alpine_c_client_target="lnx"
	  fi
        ])
    fi
    ;;
  *-apple-darwin*)
    systype="OSX"
    alpine_path_delim="/"
    alpine_mode_readonly="(0600)"
    LIBS="$LIBS -framework Carbon -framework ApplicationServices -framework Security"
    AM_CFLAGS="$AM_CFLAGS -Dbsd"
    dnl SEE include/system.h
    AC_DEFINE([OSX_TARGET],[1],[OSX TARGET])
    case "$alpine_os_credential_cache" in
      no)
	;;
      *)
	AC_DEFINE([APPLEKEYCHAIN], [1], [Use Apple OS X key chain for credential caching])
	;;
    esac
    AC_CHECK_LIB(pam, pam_start,
      [
        alpine_c_client_target="oxp"
      ],
      [
        alpine_c_client_target="osx"
      ])
    AC_CHECK_HEADER(/usr/include/security/pam_appl.h,
      [
        alpine_c_client_target="oxs"
      ])
    ;;
  *-*-solaris*)
    if test x$GCC = "xyes" ; then
      systype="GSO"
      alpine_c_client_target="gso"
    else
      systype="SOC"
      alpine_c_client_target="soc"
      AC_DEFINE([__EXTENSIONS__], [1], [Enable extended pthread features on Solaris])
    fi
    alpine_path_delim="/"
    alpine_mode_readonly="(0600)"
    dnl possible other OS's need this.
    dnl Use autoconf 2.61's AC_USE_SYSTEM_EXTENSIONS at some point
    ;;
  *-*-sunos4*)
    systype="SUN"
    alpine_path_delim="/"
    alpine_mode_readonly="(0600)"
    alpine_c_client_target="s40"
    ;;
  *-*-sco3.2v5*)
    if test x$GCC = "xyes" ; then
      systype="GO5"
      alpine_c_client_target="go5"
    else
      systype="SC5"
      alpine_c_client_target="sc5"
    fi
    alpine_path_delim="/"
    alpine_mode_readonly="(0600)"
    dnl possible other OS's need this.
    dnl Use autoconf 2.61's AC_USE_SYSTEM_EXTENSIONS at some point
    ;;
  *-next-*)
    systype="NXT"
    alpine_path_delim="/"
    alpine_mode_readonly="(0600)"
    alpine_c_client_target="nx3"
    ;;
  *-*-netbsd*)
    systype="NEB"
    alpine_path_delim="/"
    alpine_mode_readonly="(0600)"
    alpine_c_client_target="neb"
    ;;
  *-*-dragonfly*)
    systype="DFB"
    alpine_path_delim="/"
    alpine_mode_readonly="(0600)"
    alpine_c_client_target="neb"
    ;;
  *-*-bsdi*)
    systype="BSI"
    alpine_path_delim="/"
    alpine_mode_readonly="(0600)"
    alpine_c_client_target="bsi"
    ;;
  *-*-freebsd*)
    systype="BSF"
    alpine_path_delim="/"
    alpine_mode_readonly="(0600)"
    alpine_c_client_target="bsf"
    LIBS="$LIBS $LIBINTL"
    ;;
  *-*-openbsd*)
    systype="BSO"
    alpine_path_delim="/"
    alpine_mode_readonly="(0600)"
    alpine_c_client_target="bso"
    LIBS="$LIBS $LIBINTL"
    ;;
  *-*-aix5*)
    systype="A52"
    alpine_path_delim="/"
    alpine_mode_readonly="(0600)"
    alpine_c_client_target="a52"
    ;;
  *-*-aix4*)
    systype="A41"
    alpine_path_delim="/"
    alpine_mode_readonly="(0600)"
    alpine_c_client_target="a41"
    ;;
  *-*-aix3*)
    systype="A32"
    alpine_path_delim="/"
    alpine_mode_readonly="(0600)"
    alpine_c_client_target="a32"
    ;;
  *-*UNIX_SV | *-*-sysv5UnixWare7* | *-*OpenUNIX*)
    systype="UW2"
    alpine_path_delim="/"
    alpine_mode_readonly="(0600)"
    alpine_c_client_target="uw2"
    ;;
  *-*-osf5*)
    systype="OSF"
    alpine_path_delim="/"
    alpine_mode_readonly="(0600)"
    alpine_c_client_target="osf"
    ;;
  *-*-cygwin)
    systype="CYG"
    alpine_path_delim="\\"
    alpine_mode_readonly="(S_IREAD | S_IWRITE)"
    alpine_c_client_target="cyg"
    ;;
  windows* | *-*-pw32*)
    systype="WNT"
    alpine_path_delim="\\"
    alpine_mode_readonly="(S_IREAD | S_IWRITE)"
    alpine_c_client_target="wnt"
    AC_DEFINE([_WINDOWS], [1], [Windows is just too different])
    ;;
  *)
    AC_ERROR([Unrecognized system: $host])
    ;;
esac

dnl set pico newmail check directory
if test -n "$alpine_with_local_maildir" ; then
    alpine_local_maildir=$alpine_with_local_maildir
elif test -d /var/spool/mail ; then
    alpine_local_maildir="/var/spool/mail"
elif test -d /var/mail ; then
    alpine_local_maildir="/var/mail"
else
    alpine_local_maildir="/usr/spool/mail"
fi

dnl set c-client make target
if test -n "$alpine_with_c_client_target" ; then
    alpine_c_client_target=$alpine_with_c_client_target
fi

AC_DEFINE_UNQUOTED([SYSTYPE], "$systype", [Pine-Centric Host Specifier])
AC_DEFINE_UNQUOTED([C_FILESEP],'$alpine_path_delim',[File name separator as character constant])
AC_DEFINE_UNQUOTED([S_FILESEP],"$alpine_path_delim",[File name separator as string constant])
AC_DEFINE_UNQUOTED([MAILDIR],"$alpine_local_maildir",[Path to local inboxes for pico newmail check])
AC_DEFINE_UNQUOTED([MODE_READONLY], $alpine_mode_readonly, [File mode used to set readonly access])

dnl c-client make particulars
AC_SUBST([C_CLIENT_TARGET], $alpine_c_client_target)
AC_SUBST([C_CLIENT_WITH_IPV6], $c_client_ip6)
if test "x$alpine_SSLTYPE" = "xnone" ; then 
  AC_MSG_NOTICE([* * *  NOT Including SSL Support])
  c_client_specials="${c_client_specials}SSLTYPE=none "
else
  dnl issue any warnings for common OpenSSL issues

  if test -n "$alpine_SSLCERTS" -a -d "$alpine_SSLCERTS" ; then
    certdir="$alpine_SSLCERTS"
  elif test -n "$alpine_SSLDIR" -a -d "${alpine_SSLDIR}/certs" ; then
    certdir="${alpine_SSLDIR}/certs"
  else
    AC_MSG_NOTICE([SSL Problem: certificate directory not found])
  fi

  if test "x$with_smime" != "xno" ; then
    if test -n "$certdir" ; then
      AC_DEFINE([SMIME], [], [Enable S/MIME code])
      AC_DEFINE_UNQUOTED([SMIME_SSLCERTS],"$certdir",[Directory where S/MIME CACerts are located])
    fi
  fi

  if test ! -f ${certdir}/factory.pem ; then
    AC_MSG_NOTICE([* * * SSL file "${certdir}/factory.pem" is missing.])
    AC_MSG_NOTICE([* * *   This might indicate that CA certs did not get properly])
    AC_MSG_NOTICE([* * *   installed.  If you get certificate validation failures])
    AC_MSG_NOTICE([* * *   in Alpine, this might be the reason for them.])
  fi

  if test -z "`ls ${certdir} | $EGREP '^@<:@0-9A-Fa-f@:>@{8}\.@<:@0-9@:>@'`" ; then
    AC_MSG_NOTICE([* * * No 8-hexdigit symlinks in certificate directory "${certdir}".])
    AC_MSG_NOTICE([* * *   This might indicate that CA certs did not get properly])
    AC_MSG_NOTICE([* * *   installed.  If you get certificate validation failures])
    AC_MSG_NOTICE([* * *   in Alpine, this might be the reason for them.])
  fi

  if test -n "$alpine_SSLDIR" ; then
    c_client_specials="${c_client_specials}SSLDIR=$alpine_SSLDIR "
  fi

  if test -n "$alpine_SSLCERTS" ; then
    c_client_specials="${c_client_specials}SSLCERTS=$alpine_SSLCERTS "
  fi

  if test -n "$alpine_SSLINCLUDE" ; then
    c_client_specials="${c_client_specials}SSLINCLUDE=$alpine_SSLINCLUDE "
  fi

  if test -n "$alpine_SSLLIB" ; then
    c_client_specials="${c_client_specials}SSLLIB=$alpine_SSLLIB "
  fi

fi

if test "x$alpine_GSSTYPE" != "xnone" ; then
  c_client_specials="${c_client_specials}EXTRAAUTHENTICATORS=gss "
  AC_MSG_NOTICE([* * * Including Kerberos5 functionality])
fi

if test -n "$CPPFLAGS" ; then
  alpine_c_client_cflags="$alpine_c_client_cflags ${CPPFLAGS}"
fi
if test -n "$CFLAGS" ; then
  alpine_c_client_cflags="$alpine_c_client_cflags ${CFLAGS}"
fi

if test -n "$alpine_c_client_cflags" ; then
  AC_SUBST(C_CLIENT_CFLAGS, EXTRACFLAGS=\"$alpine_c_client_cflags\")
fi

if test -n "$LDFLAGS" ; then
  alpine_c_client_ldflags="$alpine_c_client_ldflags ${LDFLAGS}"
fi
if test -n "$LIBS" ; then
  alpine_c_client_ldflags="$alpine_c_client_ldflags ${LIBS}"
fi

if test -n "$alpine_c_client_ldflags" ; then
  AC_SUBST(C_CLIENT_LDFLAGS, EXTRALDFLAGS=\"$alpine_c_client_ldflags\")
fi

if test -n "$alpine_c_client_gccoptlevel" ; then
  AC_SUBST(C_CLIENT_GCCOPTLEVEL, GCCOPTLEVEL=\"$alpine_c_client_gccoptlevel\")
fi

AC_SUBST([C_CLIENT_SPECIALS], $c_client_specials)

dnl Deal with Web Alpine
if test -z "$WEB_BUILD" ; then
  WEB_PUBCOOKIE_BUILD=
  AC_MSG_NOTICE([* * * TCL libraries could not be found.])
  AC_MSG_NOTICE([* * * WEB ALPINE COMPONENT WILL NOT BE BUILT.])
else
  if test -n "$WEB_PUBCOOKIE_BUILD" ; then
    if test "x$alpine_GSSTYPE" = "xnone" ; then
      WEB_PUBCOOKIE_BUILD=
      AC_MSG_NOTICE([* * * Kerberos5 support not found.])
      AC_MSG_NOTICE([* * * WEB ALPINE PUBCOOKIE COMPONENT WILL NOT BE BUILT.])
    elif test -z "$WEB_BINDIR" ; then
      WEB_PUBCOOKIE_BUILD=
      AC_MSG_NOTICE([* * * Web Alpine binary directory not provided.])
	AC_MSG_ERROR([* * * --with-pubcookie requires --with-web-bin=PATH.
        Please re-run configure with these options:
        --with-pubcookie --with-web-bin=/usr/local/libexec/alpine/bin])
    else
      AC_DEFINE([PUBCOOKIE],[1],[Include support for UW Pubcookie Web Authentication])
      WEB_PUBCOOKIE_LIB=../pubcookie/libauthgssproxy.a
      WEB_PUBCOOKIE_LINK=gssapi_proxy.l
    fi
  fi
fi

AC_SUBST([REGEX_BUILD])

AC_SUBST([WEB_BUILD])
AC_SUBST([WEB_BINDIR])
AC_SUBST([WEB_PUBCOOKIE_BUILD])
AC_SUBST([WEB_PUBCOOKIE_LIB])
AC_SUBST([WEB_PUBCOOKIE_LINK])

AC_SUBST([AM_CFLAGS])
AC_SUBST([AM_LDFLAGS])

AC_OUTPUT(m4/Makefile po/Makefile.in regex/Makefile \
	  pith/osdep/Makefile pith/charconv/Makefile pith/Makefile \
	  pico/osdep/Makefile pico/Makefile \
	  alpine/osdep/Makefile alpine/Makefile \
	  web/src/Makefile web/src/pubcookie/Makefile \
	  web/src/alpined.d/Makefile \
	  Makefile)
