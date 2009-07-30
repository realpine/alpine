#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: pilot.c 1184 2008-12-16 23:52:15Z hubert@u.washington.edu $";
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

/*
 * Program:	Main stand-alone Pine File Browser routines
 *
 *
 * BROWSER NOTES:
 *
 * 30 Sep 92 - Stand alone PIne's "Lister of Things" came into being.
 *	       It's built against libpico.a from a command line like:
 *
 *		       cc pilot.c libpico.a -ltermcap -lc -o pilot
 *
 *	       should it become a fleshed out tool, we'll move it into
 *	       the normal build process.
 */

#include	"headers.h"
#include	"../c-client/mail.h"
#include	"../c-client/rfc822.h"
#include	"../pith/osdep/collate.h"
#include	"../pith/charconv/filesys.h"
#include	"../pith/charconv/utf8.h"
#include	"../pith/conf.h"


#define		PILOT_VERSION	"UW PILOT 2.99"


extern char *gethomedir(int *);

char *pilot_args(int, char **, int *);
void  pilot_args_help(void);
void  pilot_display_args_err(char *, char **, int);

char  args_pilot_missing_flag[]  = N_("unknown flag \"%c\"");
char  args_pilot_missing_arg[]   = N_("missing or empty argument to \"%c\" flag");
char  args_pilot_missing_num[]   = N_("non numeric argument for \"%c\" flag");
char  args_pilot_missing_color[] = N_("missing color for \"%s\" flag");
char  args_pilot_missing_charset[] = N_("missing character set for \"%s\" flag");
char  args_pilot_input_charset[] = N_("input character set \"%s\" is unsupported");
char  args_pilot_output_charset[] = N_("output character set \"%s\" is unsupported");

char *args_pilot_args[] = {
/* TRANSLATORS: little help printed out when incorrect arguments are
   given for pilot program. */
N_("Possible Starting Arguments for Pilot file browser:"),
"",
N_("\tArgument\t\tMeaning"),
N_("\t -a \t\tShowDot - show dot files in file browser"),
N_("\t -j \t\tGoto - allow 'Goto' command in file browser"),
N_("\t -g \t\tShow - show cursor in file browser"),
N_("\t -m \t\tMouse - turn on mouse support"),
N_("\t -v \t\tOneColumn - use single column display"),
N_("\t -x \t\tNoKeyhelp - suppress keyhelp"),
N_("\t -q \t\tTermdefWins - termcap or terminfo takes precedence over defaults"),
N_("\t -f \t\tKeys - force use of function keys"),
N_("\t -h \t\tHelp - give this list of options"),
#ifndef	_WINDOWS
N_("\t -dcs <display_character_set> \tdefault uses LANG or LC_CTYPE from environment"),
N_("\t -kcs <keyboard_character_set> \tdefaults to display_character_set"),
N_("\t -syscs\t\tuse system-supplied translation routines"),
#endif	/* ! _WINDOWS */
N_("\t -n[#s] \tMail - notify about new mail every #s seconds, default=180"),
N_("\t -t \t\tShutdown - enable special shutdown mode"),
N_("\t -o <dir>\tOperation - specify the operating directory"),
N_("\t -z \t\tSuspend - allow use of ^Z suspension"),
#ifdef	_WINDOWS
N_("\t -cnf color \tforeground color"),
N_("\t -cnb color \tbackground color"),
N_("\t -crf color \treverse foreground color"),
N_("\t -crb color \treverse background color"),
#endif	/* _WINDOWS */
N_("\t -no_setlocale_collate\tdo not do setlocale(LC_COLLATE)"),
"", 
N_("\t All arguments may be followed by a directory name to start in."),
"",
NULL
};


/*
 * main standalone browser routine
 */
int
main(int argc, char *argv[])
{
    char  bname[NBUFN];		/* buffer name of file to read	*/
    char  filename[NSTRING];
    char  filedir[NSTRING];
    char *dir;
    char *display_charmap = NULL, *dc;
    char *keyboard_charmap = NULL;
    int   use_system = 0;
    char *err = NULL;
    int   setlocale_collate = 1;
 
    set_input_timeout(0);
    Pmaster = NULL;			/* turn OFF composer functionality */
    km_popped = 0;
    opertree[0] = '\0'; opertree[NLINE] = '\0';
    filename[0] ='\0';
    gmode |= MDBRONLY;			/* turn on exclusive browser mode */

    /*
     * Read command line flags before initializing, otherwise, we never
     * know to init for f_keys...
     */
    if((dir = pilot_args(argc, argv, &setlocale_collate)) != NULL){
	strncpy(filedir, dir, sizeof(filedir));
	filedir[sizeof(filedir)-1] = '\0';
	fixpath(filedir, sizeof(filedir));
    }
    else{
      strncpy(filedir, gethomedir(NULL), sizeof(filedir));
      filedir[sizeof(filedir)-1] = '\0';
    }

    set_collation(setlocale_collate, 1);

#ifdef	_WINDOWS
    init_utf8_display(1, NULL);
#else	/* UNIX */

#define cpstr(s) strcpy((char *)fs_get(1+strlen(s)), s)

    if(display_character_set)
      display_charmap = cpstr(display_character_set);
#if   HAVE_LANGINFO_H && defined(CODESET)
    else if((dc = nl_langinfo_codeset_wrapper()) != NULL)
      display_charmap = cpstr(dc);
#endif

    if(!display_charmap)
      display_charmap = cpstr("US-ASCII");

    if(keyboard_character_set)
      keyboard_charmap = cpstr(keyboard_character_set);
    else
      keyboard_charmap = cpstr(display_charmap);

#undef cpstr

    if(use_system_translation){
#if	PREREQ_FOR_SYS_TRANSLATION
	use_system++;
	/* This modifies its arguments */
	if(setup_for_input_output(use_system, &display_charmap, &keyboard_charmap,
				  &input_cs, &err) == -1){
	    fprintf(stderr, "%s\n", err ? err : "trouble with character set");
	    exit(1);
	}
	else if(err){
	    fprintf(stderr, "%s\n", err);
	    fs_give((void **) &err);
	}
#endif
    }

    if(!use_system){
	if(setup_for_input_output(use_system, &display_charmap, &keyboard_charmap,
				  &input_cs, &err) == -1){
	    fprintf(stderr, "%s\n", err ? err : "trouble with character set");
	    exit(1);
	}
	else if(err){
	    fprintf(stderr, "%s\n", err);
	    fs_give((void **) &err);
	}
    }

    if(keyboard_charmap){
	set_locale_charmap(keyboard_charmap);
	free((void *) keyboard_charmap);
    }

    if(display_charmap)
      free((void *) display_charmap);

#endif	/* UNIX */

    if(!vtinit())			/* Displays.            */
      exit(1);

    strncpy(bname, "main", sizeof(bname));		/* default buffer name */
    bname[sizeof(bname)-1] = '\0';
    edinit(bname);			/* Buffers, windows.   */
#if	defined(USE_TERMCAP) || defined(USE_TERMINFO) || defined(VMS)
    if(kbesc == NULL){			/* will arrow keys work ? */
	(*term.t_putchar)('\007');
	emlwrite("Warning: keypad keys may be non-functional", NULL);
    }
#endif	/* USE_TERMCAP/USE_TERMINFO/VMS */

    curbp->b_mode |= gmode;		/* and set default modes*/
    if(get_input_timeout()){
	EML eml;

	eml.s = comatose(get_input_timeout());
	emlwrite(_("Checking for new mail every %s seconds"), &eml);
    }


    set_browser_title(PILOT_VERSION);
    FileBrowse(filedir, sizeof(filedir), filename, sizeof(filename), NULL, 0, 0, NULL);
    wquit(1, 0);
    exit(0);
}


/*
 *  Parse the command line args.
 *
 * Args      ac
 *           av
 *
 * Result: command arguments parsed
 *       possible printing of help for command line
 *       various global flags set
 *       returns the name of directory to start in if specified, else NULL
 */
char *
pilot_args(int ac, char **av, int *setlocale_collate)
{
    int   c, usage = 0;
    char *str;
    char  tmp_1k_buf[1000];     /* tmp buf to contain err msgs  */ 

Loop:
    /* while more arguments with leading - */
    while(--ac > 0 && **++av == '-'){
      /* while more chars in this argument */
      while(*++*av){

	if(strcmp(*av, "no_setlocale_collate") == 0){
	    *setlocale_collate = 0;
	    goto Loop;
	}
#ifndef	_WINDOWS
	else if(strcmp(*av, "syscs") == 0){
	    use_system_translation = !use_system_translation;
	    goto Loop;
	}
#endif	/* ! _WINDOWS */
#ifdef	_WINDOWS
	if(strcmp(*av, "cnf") == 0
	   || strcmp(*av, "cnb") == 0
	   || strcmp(*av, "crf") == 0
	   || strcmp(*av, "crb") == 0){

	    char *cmd = *av; /* save it to use below */

	    if(--ac){
		str = *++av;
		if(cmd[1] == 'n'){
		    if(cmd[2] == 'f')
		      pico_nfcolor(str);
		    else if(cmd[2] == 'b')
		      pico_nbcolor(str);
		}
		else if(cmd[1] == 'r'){
		    if(cmd[2] == 'f')
		      pico_rfcolor(str);
		    else if(cmd[2] == 'b')
		      pico_rbcolor(str);
		}
	    }
	    else{
		snprintf(tmp_1k_buf, sizeof(tmp_1k_buf), _(args_pilot_missing_color), cmd);
		pilot_display_args_err(tmp_1k_buf, NULL, 1);
	        usage++;
	    }

	    goto Loop;
	}
#endif	/* _WINDOWS */
#ifndef	_WINDOWS
	else if(strcmp(*av, "dcs") == 0 || strcmp(*av, "kcs") == 0){
	    char *cmd = *av;

	    if(--ac){
		if(strcmp(*av, "dcs") == 0){
		    display_character_set = *++av;
		    if(!output_charset_is_supported(display_character_set)){
			snprintf(tmp_1k_buf, sizeof(tmp_1k_buf), _(args_pilot_output_charset), display_character_set);
			pilot_display_args_err(tmp_1k_buf, NULL, 1);
			usage++;
		    }
		}
		else{
		    keyboard_character_set = *++av;
		    if(!input_charset_is_supported(keyboard_character_set)){
			snprintf(tmp_1k_buf, sizeof(tmp_1k_buf), _(args_pilot_input_charset), keyboard_character_set);
			pilot_display_args_err(tmp_1k_buf, NULL, 1);
			usage++;
		    }
		}
	    }
	    else{
		snprintf(tmp_1k_buf, sizeof(tmp_1k_buf), _(args_pilot_missing_charset), cmd);
		pilot_display_args_err(tmp_1k_buf, NULL, 1);
	        usage++;
	    }

	    goto Loop;
	}
#endif	/* ! _WINDOWS */

	/*
	 * Single char options.
	 */
	switch(c = **av){
	  /*
	   * These don't take arguments.
	   */
	  case 'a':
	    gmode ^= MDDOTSOK;		/* show dot files */
	    break;
	  case 'f':			/* -f for function key use */
	    gmode ^= MDFKEY;
	    break;
	  case 'j':			/* allow "Goto" in file browser */
	    gmode ^= MDGOTO;
	    break;
	  case 'g':			/* show-cursor in file browser */
	    gmode ^= MDSHOCUR;
	    break;
	  case 'm':			/* turn on mouse support */
	    gmode ^= MDMOUSE;
	    break;
	  case 'v':			/* single column display */
	    gmode ^= MDONECOL;
	    break;			/* break back to inner-while */
	  case 'x':			/* suppress keyhelp */
	    sup_keyhelp = !sup_keyhelp;
	    break;
	  case 'q':			/* -q for termcap takes precedence */
	    gmode ^= MDTCAPWINS;
	    break;
	  case 'z':			/* -z to suspend */
	    gmode ^= MDSSPD;
	    break;
	  case 'h':
	    usage++;
	    break;

	  /*
	   * These do take arguments.
	   */
	  case 'n':			/* -n for new mail notification */
	  case 'o' :			/* operating tree */
	    if(*++*av)
	      str = *av;
	    else if(--ac)
	      str = *++av;
	    else{
	      snprintf(tmp_1k_buf, sizeof(tmp_1k_buf), _(args_pilot_missing_arg), c);
	      pilot_display_args_err(tmp_1k_buf, NULL, 1);
	      usage++;
	      goto Loop;
	    }

	    switch(c){
	      case 'o':
		strncpy(opertree, str, NLINE);
		gmode ^= MDTREE;
		break;

	/* numeric args */
	      case 'n':
		if(!isdigit((unsigned char)str[0])){
		  snprintf(tmp_1k_buf, sizeof(tmp_1k_buf), _(args_pilot_missing_num), c);
		  pilot_display_args_err(tmp_1k_buf, NULL, 1);
		  usage++;
		}

		set_input_timeout(180);
		
		if(set_input_timeout(atoi(str)) < 30)
		  set_input_timeout(180);
		
		break;
	    }

	    goto Loop;

	  default:			/* huh? */
	    snprintf(tmp_1k_buf, sizeof(tmp_1k_buf), _(args_pilot_missing_flag), c);
	    pilot_display_args_err(tmp_1k_buf, NULL, 1);
	    usage++;
	    break;
	}
      }
    }

    if(usage)
      pilot_args_help();

    /* return the directory */
    if(ac > 0)
      return(*av);
    else
      return(NULL);
}


/*----------------------------------------------------------------------
    print a few lines of help for command line arguments

  Args:  none

 Result: prints help messages
  ----------------------------------------------------------------------*/
void
pilot_args_help(void)
{
    char **a;
    char *pp[2];

    pp[1] = NULL;

    /**  print out possible starting arguments... **/

    for(a=args_pilot_args; a && *a; a++){
	pp[0] = _(*a);
	pilot_display_args_err(NULL, pp, 0);
    }

    exit(1);
}


/*----------------------------------------------------------------------
   write argument error to the display...

  Args:  none

 Result: prints help messages
  ----------------------------------------------------------------------*/
void
pilot_display_args_err(char *s, char **a, int err)
{
    char  errstr[256], *errp;
    FILE *fp = err ? stderr : stdout;

    if(err && s)
      snprintf(errp = errstr, sizeof(errstr), _("Argument Error: %.200s"), s);
    else
      errp = s;

    if(errp)
      fprintf(fp, "%s\n", errp);

    while(a && *a)
      fprintf(fp, "%s\n", *a++);
}
