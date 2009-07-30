#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: arg.c 900 2008-01-05 01:13:26Z hubert@u.washington.edu $";
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

/*======================================================================
      Command line argument parsing functions 

  ====*/

#include "headers.h"

#include "../pith/state.h"
#include "../pith/init.h"
#include "../pith/conf.h"
#include "../pith/list.h"
#include "../pith/util.h"
#include "../pith/help.h"

#include "imap.h"

#include "arg.h"


int  process_debug_str(char *);
void args_add_attach(PATMT **, char *, int);
int  pinerc_cmdline_opt(char *);


/*
 * Name started as to invoke function key mode
 */
#define	ALPINE_FKEY_NAME	"alpinef"

/*
 * Various error and informational strings..
 */
/* TRANSLATORS: This is a list of errors printed when something goes wrong
   early on with the argument list. Be careful not to change literal
   option names mentioned in the strings. */
static char args_err_missing_pinerc[] =	N_("missing argument for option \"-pinerc\" (use - for standard out)");
#if	defined(DOS) || defined(OS2)
static char args_err_missing_aux[] =		N_("missing argument for option \"-aux\"");
#endif
#ifdef	PASSFILE
static char args_err_missing_passfile[] =	N_("missing argument for option \"-passfile\"");
static char args_err_non_abs_passfile[] =	N_("argument to \"-passfile\" should be fully-qualified");
#endif
static char args_err_missing_sort[] =		N_("missing argument for option \"-sort\"");
static char args_err_missing_flag_arg[] =	N_("missing argument for flag \"%c\"");
static char args_err_missing_flag_num[] =	N_("Non numeric argument for flag \"%c\"");
static char args_err_missing_debug_num[] =	N_("Non numeric argument for \"%s\"");
static char args_err_missing_url[] =		N_("missing URL for \"-url\"");
static char args_err_missing_attachment[] =	N_("missing attachment for \"%s\"");
static char args_err_conflict[] =		N_("conflicting action: \"%s\"");
static char args_err_unknown[] =		N_("unknown flag \"%c\"");
static char args_err_I_error[] =		N_("-I argument \"%s\": %s");
static char args_err_d_error[] =		N_("-d argument \"%s\": %s");
static char args_err_internal[] =		"%s";
static char args_err_missing_copyprc[] =	N_("missing argument for option \"-copy_pinerc\"\nUsage: pine -copy_pinerc <local_pinerc> <remote_pinerc>");
static char args_err_missing_copyabook[] =	N_("missing argument for option \"-copy_abook\"\nUsage: pine -copy_abook <local_abook> <remote_abook>");


static char *args_pine_args[] = {
N_("Possible Starting Arguments for Alpine program:"),
"",
N_(" Argument\tMeaning"),
N_(" <addrs>...\tGo directly into composer sending to given address"),
N_("\t\tList multiple addresses with a single space between them."),
N_("\t\tStandard input redirection is allowed with addresses."),
N_("\t\tNote: Places addresses in the \"To\" field only."),
N_(" -attach <file>\tGo directly into composer with given file"),
N_(" -attachlist <file-list>"),
N_(" -attach_and_delete <file>"),
N_("\t\tGo to composer, attach file, delete when finished"),
N_("\t\tNote: Attach options can't be used if -f, -F"),
N_("\t\tadded to Attachment list.  Attachlist must be the last"),
N_("\t\toption on the command line"),
N_(" -bail\t\tExit if pinerc file doesn't already exist"),
#ifdef	DEBUG
N_(" -d n\t\tDebug - set debug level to 'n', or use the following:"),
N_(" -d keywords...\tflush,timestamp,imap=0..4,tcp,numfiles=0..31,verbose=0..9"),
#endif
N_(" -f <folder>\tFolder - give folder name to open"),
N_(" -c <number>\tContext - which context to apply to -f arg"),
N_(" -F <file>\tFile - give file name to open and page through and"),
N_("\t\tforward as email."),
N_(" -h \t\tHelp - give this list of options"),
N_(" -k \t\tKeys - Force use of function keys"),
N_(" -z \t\tSuspend - allow use of ^Z suspension"),
N_(" -r \t\tRestricted - can only send mail to oneself"),
N_(" -sort <sort>\tSort - Specify sort order of folder:"),
N_("\t\t\tarrival, subject, threaded, orderedsubject, date,"),
N_("\t\t\tfrom, size, score, to, cc, /reverse"),
N_(" -i\t\tIndex - Go directly to index, bypassing main menu"),
N_(" -I <keystroke_list>   Initial keystrokes to be executed"),
N_(" -n <number>\tEntry in index to begin on"),
N_(" -o \t\tReadOnly - Open first folder read-only"),
N_(" -conf\t\tConfiguration - Print out fresh global configuration. The"),
N_("\t\tvalues of your global configuration affect all Alpine users"),
N_("\t\ton your system unless they have overridden the values in their"),
N_("\t\tpinerc files."),
N_(" -pinerc <file>\tConfiguration - Put fresh pinerc configuration in <file>"),
N_(" -p <pinerc>\tUse alternate .pinerc file"),
#if	!defined(DOS) && !defined(OS2)
N_(" -P <pine.conf>\tUse alternate pine.conf file"),
#else
N_(" -aux <aux_files_dir>\tUse this with remote pinerc"),
N_(" -P <pine.conf>\tUse pine.conf file for default settings"),
N_(" -nosplash \tDisable the PC-Alpine splash screen"),
#endif

#if defined(APPLEKEYCHAIN) || (WINCRED > 0)
N_(" -erase_stored_passwords\tEliminate any stored passwords"),
#endif

#ifdef	PASSFILE
N_(" -passfile <fully_qualified_filename>\tSet the password file to something other"),
N_("\t\tthan the default"),
#endif	/* PASSFILE */

#ifdef	LOCAL_PASSWD_CACHE
N_(" -nowrite_password_cache\tRead from a password cache if there is one, but"),
N_("\t\t\t\tnever offer to write a password to the cache"),
#endif /* LOCAL_PASSWD_CACHE */

N_(" -x <config>\tUse configuration exceptions in <config>."),
N_("\t\tExceptions are used to override your default pinerc"),
N_("\t\tsettings for a particular platform, can be a local file or"),
N_("\t\ta remote folder."),
N_(" -v \t\tVersion - show version information"),
N_(" -version\tVersion - show version information"),
N_(" -supported\tList supported options"),
N_(" -url <url>\tOpen the given URL"),
N_("\t\tNote: Can't be used if -f, -F"),
N_("\t\tStandard input redirection is not allowed with URLs."),
N_("\t\tFor mailto URLs, 'body='text should be used in place of"),
N_("\t\tinput redirection."),
N_(" -copy_pinerc <local_pinerc> <remote_pinerc>  copy local pinerc to remote"),
N_(" -copy_abook <local_abook> <remote_abook>     copy local addressbook to remote"),
N_(" -convert_sigs -p <pinerc>   convert signatures to literal signatures"),
#if	defined(_WINDOWS)
N_(" -install \tPrompt for some basic setup information"),
N_(" -uninstall \tRemove traces of Alpine from Windows system settings"),
N_(" -registry <cmd>\tWhere cmd is set,noset,clear,clearsilent,dump"),
#endif
" -<option>=<value>   Assign <value> to the pinerc option <option>",
"\t\t     e.g. -signature-file=sig1",
"\t\t     e.g. -color-style=no-color",
"\t\t     e.g. -feature-list=enable-sigdashes",
"\t\t     Note: feature-list is additive.",
"\t\t     You may leave off the \"feature-list=\" part of that,",
"\t\t     e.g. -enable-sigdashes",
NULL
};



/*
 *  Parse the command line args.
 *
 *  Args: pine_state -- The pine_state structure to put results in
 *        argc, argv -- The obvious
 *        addrs      -- Pointer to address list that we set for caller
 *
 * Result: command arguments parsed
 *       possible printing of help for command line
 *       various flags in pine_state set
 *       returns the string name of the first folder to open
 *       addrs is set
 */
void
pine_args(struct pine *pine_state, int argc, char **argv, ARGDATA_S *args)
{
    register int    ac;
    register char **av;
    int   c;
    char *str;
    char *cmd_list            = NULL;
    char *debug_str           = NULL;
    char *sort                = NULL;
    char *pinerc_file         = NULL;
    char *lc		      = NULL;
    int   do_help             = 0;
    int   do_conf             = 0;
    int   usage               = 0;
    int   do_use_fk           = 0;
    int   do_can_suspend      = 0;
    int   do_version          = 0;
    struct variable *vars     = pine_state->vars;

    ac = argc;
    av = argv;
    memset(args, 0, sizeof(ARGDATA_S));
    args->action = aaFolder;

    pine_state->pine_name = (lc = last_cmpnt(argv[0])) ? lc : (lc = argv[0]);
#ifdef	DOS
    snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%.*s", pine_state->pine_name - argv[0], argv[0]);
    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
    pine_state->pine_dir = cpystr(tmp_20k_buf);
#endif

      /* while more arguments with leading - */
Loop: while(--ac > 0)
        if(**++av == '-'){
	  /* while more chars in this argument */
	  while(*++*av){
	      /* check for pinerc options */
	      if(pinerc_cmdline_opt(*av)){
		  goto Loop;  /* done with this arg, go to next */
	      }
	      /* then other multi-char options */
	      else if(strcmp(*av, "conf") == 0){
		  do_conf = 1;
		  goto Loop;  /* done with this arg, go to next */
	      }
	      else if(strcmp(*av, "pinerc") == 0){
		  if(--ac)
		    pinerc_file = *++av;
		  else{
		      display_args_err(_(args_err_missing_pinerc), NULL, 1);
		      ++usage;
		  }

		  goto Loop;
	      }
#if	defined(DOS) || defined(OS2)
	      else if(strcmp(*av, "aux") == 0){
		  if(--ac){
		      if((str = *++av) != NULL)
			pine_state->aux_files_dir = cpystr(str);
		  }
		  else{
		      display_args_err(_(args_err_missing_aux), NULL, 1);
		      ++usage;
		  }

		  goto Loop;
	      }
	      else if(strcmp(*av, "nosplash") == 0)
		goto Loop;   /* already taken care of in WinMain */
#endif

#if defined(APPLEKEYCHAIN) || (WINCRED > 0)
	      else if(strcmp(*av, "erase_stored_passwords") == 0){
#if	(WINCRED > 0)
		  erase_windows_credentials();
#else
		  macos_erase_keychain();
#endif
		  goto Loop;
	      }
#endif /* defined(APPLEKEYCHAIN) || (WINCRED > 0) */

#ifdef	PASSFILE
	      else if(strcmp(*av, "passfile") == 0){
		  if(--ac){
		      if((str = *++av) != NULL){
			  if(!is_absolute_path(str)){
			      display_args_err(_(args_err_non_abs_passfile),
					       NULL, 1);
			      ++usage;
			  }
			  else{
			      if(pine_state->passfile)
				fs_give((void **)&pine_state->passfile);

			      pine_state->passfile = cpystr(str);
			  }
		      }
		  }
		  else{
		      display_args_err(_(args_err_missing_passfile), NULL, 1);
		      ++usage;
		  }

		  goto Loop;
	      }
#endif	/* PASSFILE */

#ifdef  LOCAL_PASSWD_CACHE
	      else if(strcmp(*av, "nowrite_password_cache") == 0){
		  ps_global->nowrite_password_cache = 1;
		  goto Loop;
	      }
#endif  /* LOCAL_PASSWD_CACHE */

	      else if(strcmp(*av, "convert_sigs") == 0){
		  ps_global->convert_sigs = 1;
		  goto Loop;
	      }
	      else if(strcmp(*av, "supported") == 0){
		  ps_global->dump_supported_options = 1;
		  goto Loop;
	      }
	      else if(strcmp(*av, "copy_pinerc") == 0){
		  if(args->action == aaFolder && !args->data.folder){
		      args->action = aaPrcCopy;
		      if(ac > 2){
			ac -= 2;
			args->data.copy.local  = *++av;
			args->data.copy.remote = *++av;
		      }
		      else{
			  display_args_err(_(args_err_missing_copyprc), NULL, 1);
			  ++usage;
		      }
		  }
		  else{
		      snprintf(tmp_20k_buf, SIZEOF_20KBUF, _(args_err_conflict), "-copy_pinerc");
		      tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		      display_args_err(tmp_20k_buf, NULL, 1);
		      ++usage;
		  }

		  goto Loop;
	      }
	      else if(strcmp(*av, "copy_abook") == 0){
		  if(args->action == aaFolder && !args->data.folder){
		      args->action = aaAbookCopy;
		      if(ac > 2){
			ac -= 2;
			args->data.copy.local  = *++av;
			args->data.copy.remote = *++av;
		      }
		      else{
			  display_args_err(_(args_err_missing_copyabook), NULL, 1);
			  ++usage;
		      }
		  }
		  else{
		      snprintf(tmp_20k_buf, SIZEOF_20KBUF, _(args_err_conflict), "-copy_abook");
		      tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		      display_args_err(tmp_20k_buf, NULL, 1);
		      ++usage;
		  }

		  goto Loop;
	      }
	      else if(strcmp(*av, "sort") == 0){
		  if(--ac){
		      sort = *++av;
		      COM_SORT_KEY = cpystr(sort);
		  }
		  else{
		      display_args_err(_(args_err_missing_sort), NULL, 1);
		      ++usage;
		  }

		  goto Loop;
	      }
	      else if(strcmp(*av, "url") == 0){
		  if(args->action == aaFolder && !args->data.folder){
		      args->action = aaURL;
		      if(--ac){
			  args->url = cpystr(*++av);
		      }
		      else{
			  display_args_err(_(args_err_missing_url), NULL, 1);
			  ++usage;
		      }
		  }
		  else{
		      snprintf(tmp_20k_buf, SIZEOF_20KBUF, _(args_err_conflict), "-url");
		      tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		      display_args_err(tmp_20k_buf, NULL, 1);
		      ++usage;
		  }

		  goto Loop;
	      }
	      else if(strcmp(*av, "attach") == 0){
		  if((args->action == aaFolder && !args->data.folder)
		     || args->action == aaMail
		     || args->action == aaURL){
		      if(args->action != aaURL)
			args->action = aaMail;
		      if(--ac){
			  args_add_attach(&args->data.mail.attachlist,
					  *++av, FALSE);
		      }
		      else{
			  snprintf(tmp_20k_buf, SIZEOF_20KBUF, _(args_err_missing_attachment), "-attach");
			  tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
			  display_args_err(tmp_20k_buf, NULL, 1);
			  ++usage;
		      }
		  }
		  else{
		      snprintf(tmp_20k_buf, SIZEOF_20KBUF, _(args_err_conflict), "-attach");
		      tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		      display_args_err(tmp_20k_buf, NULL, 1);
		      ++usage;
		  }

		  goto Loop;
	      }
	      else if(strcmp(*av, "attachlist") == 0){
		  if((args->action == aaFolder && !args->data.folder)
		     || args->action == aaMail
		     || args->action == aaURL){
		      if(args->action != aaURL)
			args->action = aaMail;
		      if(ac - 1){
			  do{
			      if(can_access(*(av+1), READ_ACCESS) == 0){
				  ac--;
				  args_add_attach(&args->data.mail.attachlist,
						  *++av, FALSE);
			      }
			      else
				break;
			  }
			  while(ac);
		      }
		      else{
			  snprintf(tmp_20k_buf, SIZEOF_20KBUF, _(args_err_missing_attachment), "-attachList");
			  tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
			  display_args_err(tmp_20k_buf, NULL, 1);
			  ++usage;
		      }
		  }
		  else{
		      snprintf(tmp_20k_buf, SIZEOF_20KBUF, _(args_err_conflict), "-attachList");
		      tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		      display_args_err(tmp_20k_buf, NULL, 1);
		      ++usage;
		  }

		  goto Loop;
	      }
	      else if(strcmp(*av, "attach_and_delete") == 0){
		  if((args->action == aaFolder && !args->data.folder)
		     || args->action == aaMail
		     || args->action == aaURL){
		      if(args->action != aaURL)
			args->action = aaMail;
		      if(--ac){
			  args_add_attach(&args->data.mail.attachlist,
					  *++av, TRUE);
		      }
		      else{
			  snprintf(tmp_20k_buf, SIZEOF_20KBUF, _(args_err_missing_attachment), "-attach_and_delete");
			  tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
			  display_args_err(tmp_20k_buf, NULL, 1);
			  ++usage;
		      }
		  }
		  else{
		      snprintf(tmp_20k_buf, SIZEOF_20KBUF, _(args_err_conflict), "-attach_and_delete");
		      tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		      display_args_err(tmp_20k_buf, NULL, 1);
		      ++usage;
		  }

		  goto Loop;
	      }
	      else if(strcmp(*av, "bail") == 0){
		  pine_state->exit_if_no_pinerc = 1;
		  goto Loop;
	      }
	      else if(strcmp(*av, "version") == 0){
		  do_version = 1;
		  goto Loop;
	      }
#ifdef	_WINDOWS
	      else if(strcmp(*av, "install") == 0){
		  pine_state->install_flag = 1;
		  pine_state->update_registry = UREG_ALWAYS_SET;
		  goto Loop;
	      }
	      else if(strcmp(*av, "uninstall") == 0){
		  /*
		   * Blast password cache, clear registry settings
		   */
#if	(WINCRED > 0)
		  erase_windows_credentials();
#endif
		  mswin_reg(MSWR_OP_BLAST, MSWR_NULL, NULL, 0);
		  exit(0);
	      }
	      else if(strcmp(*av, "registry") == 0){
		  if(--ac){
		      if(!strucmp(*++av, "set")){
			  pine_state->update_registry = UREG_ALWAYS_SET;
		      }
		      else if(!strucmp(*av, "noset")){
			  pine_state->update_registry = UREG_NEVER_SET;
		      }
		      else if(!strucmp(*av, "clear")){
			  if(!mswin_reg(MSWR_OP_BLAST, MSWR_NULL, NULL, 0))
			    display_args_err(
				       _("Alpine related Registry values removed."),
				       NULL, 0);
			  else
			    display_args_err(
		      _("Not all Alpine related Registry values could be removed"),
				       NULL, 0);
			  exit(0);
		      }
		      else if(!strucmp(*av, "clearsilent")){
			  mswin_reg(MSWR_OP_BLAST, MSWR_NULL, NULL, 0);
			  exit(0);
		      }
		      else if(!strucmp(*av, "dump")){
			  char **pRegistry = mswin_reg_dump();

			  if(pRegistry){
			      display_args_err(NULL, pRegistry, 0);
			      free_list_array(&pRegistry);
			  }
			  exit(0);
		      }
		      else{
			  display_args_err(_("unknown registry command"),
					   NULL, 1);
			  ++usage;
		      }
		  }
		  else{
		      snprintf(tmp_20k_buf, SIZEOF_20KBUF, _(args_err_missing_flag_arg), c);
		      tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		      display_args_err(tmp_20k_buf, NULL, 1);
		      ++usage;
		  }

		  goto Loop;
	      }
#endif
	      /* single char flags */
	      else{
		  switch(c = **av){
		    case 'h':
		      do_help = 1;
		      break;			/* break back to inner-while */
		    case 'k':
		      do_use_fk = 1;
		      break;
		    case 'z':
		      do_can_suspend = 1;
		      break;
		    case 'r':
		      pine_state->restricted = 1;
		      break;
		    case 'o':
		      pine_state->open_readonly_on_startup = 1;
		      break;
		    case 'i':
		      pine_state->start_in_index = 1;
		      break;
		    case 'v':
		      do_version = 1;
		      break;			/* break back to inner-while */
		      /* these take arguments */
		    case 'f': case 'F': case 'p': case 'I':
		    case 'c': case 'd': case 'P': case 'x':  /* string args */
		    case 'n':				     /* integer args */
		      if(*++*av)
			str = *av;
		      else if(--ac)
			str = *++av;
		      else if(c == 'f' || c == 'F')
			str = "";
		      else{
			  snprintf(tmp_20k_buf, SIZEOF_20KBUF, _(args_err_missing_flag_arg), c);
			  tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
			  display_args_err(tmp_20k_buf, NULL, 1);
			  ++usage;
			  goto Loop;
		      }

		      switch(c){
			case 'f':
			  if(args->action == aaFolder && !args->data.folder){
			      args->data.folder = cpystr(str);
			  }
			  else{
			      snprintf(tmp_20k_buf, SIZEOF_20KBUF, _(args_err_conflict), "-f");
			      tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
			      display_args_err(tmp_20k_buf, NULL, 1);
			      usage++;
			  }
			  
			  break;
			case 'F':
			  if(args->action == aaFolder && !args->data.folder){
			      args->action    = aaMore;
			      args->data.file = cpystr(str);
			  }
			  else{
			      snprintf(tmp_20k_buf, SIZEOF_20KBUF, _(args_err_conflict), "-F");
			      tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
			      display_args_err(tmp_20k_buf, NULL, 1);
			      usage++;
			  }

			  break;
			case 'd':
			  debug_str = str;
			  break;
			case 'I':
			  cmd_list = str;
			  break;
			case 'p':
			  if(str){
			      char path[MAXPATH], dir[MAXPATH];
				
			      if(IS_REMOTE(str) || is_absolute_path(str)){
				  strncpy(path, str, sizeof(path)-1);
				  path[sizeof(path)-1] = '\0';
			      }
			      else{
				  getcwd(dir, sizeof(path));
				  build_path(path, dir, str, sizeof(path));
			      }

			      /*
			       * Pinerc used to be the name of the pinerc
			       * file. Now, since the pinerc can be remote,
			       * we've replaced the variable pinerc with the
			       * structure prc. Unfortunately, other parts of
			       * pine rely on the fact that pinerc is the
			       * name of the pinerc _file_, and use the
			       * directory that the pinerc file is located
			       * in for their own purposes. We keep that so
			       * things will keep working.
			       */

			      if(!IS_REMOTE(path)){
				  if(pine_state->pinerc)
				    fs_give((void **)&pine_state->pinerc);

				  pine_state->pinerc = cpystr(path);
			      }

			      /*
			       * Last one wins. This would be the place where
			       * we put multiple pinercs in a list if we
			       * were to allow that.
			       */
			      if(pine_state->prc)
				free_pinerc_s(&pine_state->prc);

			      pine_state->prc = new_pinerc_s(path);
			  }

			  break;
			case 'P':
			  if(str){
			      char path[MAXPATH], dir[MAXPATH];

			      if(IS_REMOTE(str) || is_absolute_path(str)){
				  strncpy(path, str, sizeof(path)-1);
				  path[sizeof(path)-1] = '\0';
			      }
			      else{
				  getcwd(dir, sizeof(path));
				  build_path(path, dir, str, sizeof(path));
			      }

			      if(pine_state->pconf)
				free_pinerc_s(&pine_state->pconf);

			      pine_state->pconf = new_pinerc_s(path);
			  }

			  break;
			case 'x':
			  if(str)
			    pine_state->exceptions = cpystr(str);

			  break;
			case 'c':
			  if(!isdigit((unsigned char)str[0])){
			      snprintf(tmp_20k_buf, SIZEOF_20KBUF,
				      _(args_err_missing_flag_num), c);
			      tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
			      display_args_err(tmp_20k_buf, NULL, 1);
			      ++usage;
			      break;
			  }

			  pine_state->init_context = (short) atoi(str);
			  break;

			case 'n':
			  if(!isdigit((unsigned char)str[0])){
			      snprintf(tmp_20k_buf, SIZEOF_20KBUF,
				      _(args_err_missing_flag_num), c);
			      tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
			      display_args_err(tmp_20k_buf, NULL, 1);
			      ++usage;
			      break;
			  }

			  pine_state->start_entry = atoi(str);
			  if(pine_state->start_entry < 1)
			    pine_state->start_entry = 1;

			  break;
		      }

		      goto Loop;

		    default:
		      snprintf(tmp_20k_buf, SIZEOF_20KBUF, _(args_err_unknown), c);
		      tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		      display_args_err(tmp_20k_buf, NULL, 1);
		      ++usage;
		      break;
		  }
	      }
	  }
      }
      else if(args->action == aaMail
	      || (args->action == aaFolder && !args->data.folder)){
	  STRLIST_S *stp, **slp;

	  args->action = aaMail;

	  stp	    = new_strlist(*av);

	  for(slp = &args->data.mail.addrlist; *slp; slp = &(*slp)->next)
	    ;

	  *slp = stp;
      }
      else{
	  snprintf(tmp_20k_buf, SIZEOF_20KBUF, _(args_err_conflict), *av);
	  tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	  display_args_err(tmp_20k_buf, NULL, 1);
	  usage++;
      }

    if(cmd_list){
	int    commas         = 0;
	char  *p              = cmd_list;
	char  *error          = NULL;

	while(*p++)
	  if(*p == ',')
	    ++commas;

	COM_INIT_CMD_LIST = parse_list(cmd_list, commas+1, 0, &error);
	if(error){
	    snprintf(tmp_20k_buf, SIZEOF_20KBUF, _(args_err_I_error), cmd_list, error);
	    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	    display_args_err(tmp_20k_buf, NULL, 1);
	    exit(-1);
	}
    }

#ifdef DEBUG
    pine_state->debug_nfiles = NUMDEBUGFILES;
#endif
    if(debug_str && process_debug_str(debug_str))
      usage++;

    if(lc && strncmp(lc, ALPINE_FKEY_NAME, sizeof(ALPINE_FKEY_NAME) - 1) == 0)
      do_use_fk = 1;

    if(do_use_fk || do_can_suspend){
	char   list[500];
	int    commas         = 0;
	char  *p              = list;
	char  *error          = NULL;

        list[0] = '\0';

        if(do_use_fk){
	    if(list[0]){
		strncat(list, ",", sizeof(list)-strlen(list)-1);
		list[sizeof(list)-1] = '\0';
	    }

	    strncat(list, "use-function-keys", sizeof(list)-strlen(list)-1);
	    list[sizeof(list)-1] = '\0';
	}

	if(do_can_suspend){
	    if(list[0]){
		strncat(list, ",", sizeof(list)-strlen(list)-1);
		list[sizeof(list)-1] = '\0';
	    }

	    strncat(list, "enable-suspend", sizeof(list)-strlen(list)-1);
	    list[sizeof(list)-1] = '\0';
	}

	while(*p++)
	  if(*p == ',')
	    ++commas;

	pine_state->feat_list_back_compat = parse_list(list,commas+1,0,&error);
	if(error){
	    snprintf(tmp_20k_buf, SIZEOF_20KBUF, args_err_internal, error);
	    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	    display_args_err(tmp_20k_buf, NULL, 1);
	    exit(-1);
	}
    }

    if(((do_conf ? 1 : 0)+(pinerc_file ? 1 : 0)) > 1){
	display_args_err(_("May only have one of -conf and -pinerc"),
			 NULL, 1);
	exit(-1);
    }

    if(do_help || usage)
      args_help(); 

    if(usage)
      exit(-1);

    if(do_version){
	extern char datestamp[], hoststamp[];
	char rev[128];

	snprintf(tmp_20k_buf, SIZEOF_20KBUF, "Alpine %s (%s %s) built %s on %s",
		 ALPINE_VERSION,
		 SYSTYPE ? SYSTYPE : "?",
		 get_alpine_revision_string(rev, sizeof(rev)),
		 datestamp, hoststamp);
	tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
	display_args_err(tmp_20k_buf, NULL, 0);
	exit(0);
    }

    if(do_conf)
      dump_global_conf();

    if(pinerc_file)
      dump_new_pinerc(pinerc_file);

    /*
     * Don't NULL out argv[0] or we might crash in unexpected ways.  In OS X, we were
     * crashing when opening attachments because of this.
     */
    if(ac <= 0 && av != argv)
      *av = NULL;
}


/*
 * Returns 0 if ok, -1 if error.
 */
int
process_debug_str(char *debug_str)
{
    int    i, usage            = 0;
    int    commas              = 0;
    int    new_style_debug_arg = 0;
    char  *q                   = debug_str;
    char  *error               = NULL;
    char **list, **p;

#ifdef DEBUG
    if(debug_str){
	if(!isdigit((unsigned char)debug_str[0]))
	  new_style_debug_arg++;

	if(new_style_debug_arg){
	    while(*q++)
	      if(*q == ',')
		++commas;

	    list = parse_list(debug_str, commas+1, 0, &error);
	    if(error){
		snprintf(tmp_20k_buf, SIZEOF_20KBUF, _(args_err_d_error), debug_str, error);
		tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		display_args_err(tmp_20k_buf, NULL, 1);
		return(-1);
	    }

	    if(list){
	      for(p = list; *p; p++){
		if(struncmp(*p, "timestamp", 9) == 0){
		    ps_global->debug_timestamp = 1;
		}
		else if(struncmp(*p, "imap", 4) == 0){
		    q = *p + 4;
		    if(!*q || !*(q+1) || *q != '=' ||
		       !isdigit((unsigned char)*(q+1))){
			snprintf(tmp_20k_buf, SIZEOF_20KBUF, _(args_err_missing_debug_num), *p);
			tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
			display_args_err(tmp_20k_buf, NULL, 1);
			usage = -1;
		    }
		    else{
			i = atoi(q+1);
			ps_global->debug_imap = MIN(5,MAX(0,i));
		    }
		}
		else if(struncmp(*p, "flush", 5) == 0){
		    ps_global->debug_flush = 1;
		}
		else if(struncmp(*p, "tcp", 3) == 0){
		    ps_global->debug_tcp = 1;
		}
		else if(struncmp(*p, "verbose", 7) == 0){
		    q = *p + 7;
		    if(!*q || !*(q+1) || *q != '=' ||
		       !isdigit((unsigned char)*(q+1))){
			snprintf(tmp_20k_buf, SIZEOF_20KBUF, _(args_err_missing_debug_num), *p);
			tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
			display_args_err(tmp_20k_buf, NULL, 1);
			usage = -1;
		    }
		    else
		      debug = atoi(q+1);
		}
		else if(struncmp(*p, "numfiles", 8) == 0){
		    q = *p + 8;
		    if(!*q || !*(q+1) || *q != '=' ||
		       !isdigit((unsigned char)*(q+1))){
			snprintf(tmp_20k_buf, SIZEOF_20KBUF, _(args_err_missing_debug_num), *p);
			tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
			display_args_err(tmp_20k_buf, NULL, 1);
			usage = -1;
		    }
		    else{
			i = atoi(q+1);
			ps_global->debug_nfiles = MIN(31,MAX(0,i));
		    }
		}
		else if(struncmp(*p, "malloc", 6) == 0){
		    q = *p + 6;
		    if(!*q || !*(q+1) || *q != '=' ||
		       !isdigit((unsigned char)*(q+1))){
			snprintf(tmp_20k_buf, SIZEOF_20KBUF, _(args_err_missing_debug_num), *p);
			tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
			display_args_err(tmp_20k_buf, NULL, 1);
			usage = -1;
		    }
		    else{
			i = atoi(q+1);
			ps_global->debug_malloc = MIN(63,MAX(0,i));
		    }
		}
#if defined(ENABLE_LDAP) && defined(LDAP_DEBUG)
		else if(struncmp(*p, "ldap", 4) == 0){
		    q = *p + 4;
		    if(!*q || !*(q+1) || *q != '=' ||
		       !isdigit((unsigned char)*(q+1))){
			snprintf(tmp_20k_buf, SIZEOF_20KBUF, _(args_err_missing_debug_num), *p);
			tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
			display_args_err(tmp_20k_buf, NULL, 1);
			usage = -1;
		    }
		    else{
			i = atoi(q+1);
			ldap_debug = i;
		    }
		}
#endif	/* LDAP */
		else{
		    snprintf(tmp_20k_buf, SIZEOF_20KBUF, _("unknown debug keyword \"%s\""), *p);
		    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
		    display_args_err(tmp_20k_buf, NULL, 1);
		    usage = -1;
		}
	      }

	      free_list_array(&list);
	    }
	}
	else{
	    debug = atoi(debug_str);
	    if(debug > 9)
	      ps_global->debug_imap = 5;
	    else if(debug > 7)
	      ps_global->debug_imap = 4;
	    else if(debug > 6)
	      ps_global->debug_imap = 3;
	    else if(debug > 4)
	      ps_global->debug_imap = 2;
	    else if(debug > 2)
	      ps_global->debug_imap = 1;

	    if(debug > 7)
	      ps_global->debug_timestamp = 1;

	    if(debug > 8)
	      ps_global->debug_flush = 1;
	}
    }

    if(!new_style_debug_arg){
#ifdef CSRIMALLOC
	ps_global->debug_malloc =
			(debug <= DEFAULT_DEBUG) ? 1 : (debug < 9) ? 2 : 3;
#else /* !CSRIMALLOC */
#ifdef HAVE_SMALLOC
	if(debug > 8)
	  ps_global->debug_malloc = 2;
#else /* !HAVE_SMALLOC */
#ifdef NXT
	if(debug > 8)
	  ps_global->debug_malloc = 32;
#endif /* NXT */
#endif /* HAVE_SMALLOC */
#endif /* CSRIMALLOC */
    }

#else  /* !DEBUG */

    snprintf(tmp_20k_buf, SIZEOF_20KBUF, _("unknown flag \"d\", debugging not compiled in"));
    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';
    display_args_err(tmp_20k_buf, NULL, 1);
    usage = -1;

#endif /* !DEBUG */

    return(usage);
}


void
args_add_attach(PATMT **alpp, char *s, int deleted)
{
    PATMT *atmp, **atmpp;

    atmp = (PATMT *) fs_get(sizeof(PATMT));
    memset(atmp, 0, sizeof(PATMT));
    atmp->filename = cpystr(s);

#if	defined(DOS) || defined(OS2)
    (void) removing_quotes(atmp->filename);
#endif

    if(deleted)
      atmp->flags |= A_TMP;

    for(atmpp = alpp; *atmpp; atmpp = &(*atmpp)->next)
      ;

    *atmpp = atmp;
}


/*----------------------------------------------------------------------
    print a few lines of help for command line arguments

  Args:  none

 Result: prints help messages
  ----------------------------------------------------------------------*/
void
args_help(void)
{
    char *pp[2];
#ifndef _WINDOWS
    char **a;
#endif

    pp[1] = NULL;

    /**  print out possible starting arguments... **/

    /*
     * display_args_err expects already translated input
     * so we need to translate a line at a time. Since
     * the 1st and 3rd args are zero it is ok to call it
     * a line at a time.
     *
     * Windows expects the full block of text so we'll pass
     * it as such.
     */
#ifndef _WINDOWS
    for(a=args_pine_args; a && *a; a++){
	pp[0] = _(*a);
	display_args_err(NULL, pp, 0);
    }
#else
    display_args_err(NULL, args_pine_args, 0);
#endif

    exit(1);
}


/*----------------------------------------------------------------------
   write argument error to the display...

  Args:  none

 Result: prints help messages
  ----------------------------------------------------------------------*/
void
display_args_err(char *s, char **a, int err)
{
    char  errstr[256], *errp;
    FILE *fp = err ? stderr : stdout;


    if(err && s){
	snprintf(errp = errstr, sizeof(errstr), "%s: %s", _("Argument Error"), s);
	errstr[sizeof(errstr)-1] = '\0';
    }
    else
      errp = s;

#ifdef	_WINDOWS
    if(errp)
      mswin_messagebox(errp, err);

    if(a && *a){
	os_argsdialog(a);
    }
#else
    if(errp)
      fprintf(fp, "%s\n", errp);

    while(a && *a)
      fprintf(fp, "%s\n", *a++);
#endif
}


/*
 * The argument is an argument from the command line.  We check to see
 * if it is specifying an alternate value for one of the options that is
 * normally set in pinerc.  If so, we return 1 and set the appropriate
 * values in the variables array.
 * The arg can be
 *          varname=value
 *          varname=value1,value2,value3
 *          feature-list=featurename                 these are just a special
 *          feature-list=featurename1,featurename2   case of above
 *          featurename                            This is equivalent to above
 *          no-featurename
 */
int
pinerc_cmdline_opt(char *arg)
{
    struct variable *v;
    char            *p1 = NULL,
                    *value,
		   **oldlvalue = NULL,
                   **lvalue;
    int              i, count;

    if(!arg || !arg[0])
      return 0;

    for(v = ps_global->vars; v->name != NULL; v++){
      if(v->is_used && struncmp(v->name, arg, strlen(v->name)) == 0){
	  p1 = arg + strlen(v->name);

	  /*----- Skip to '=' -----*/
	  while(*p1 && (*p1 == '\t' || *p1 == ' '))
	    p1++;

	  if(*p1 != '='){
	      char buf[MAILTMPLEN];

	      snprintf(buf, sizeof(buf), _("Missing \"=\" after -%s\n"), v->name);
	      buf[sizeof(buf)-1] = '\0';
	      exceptional_exit(buf, -1);
	  }

	  p1++;
          break;
      }
    }

    /* no match, check for a feature name used directly */
    if(v->name == NULL){
	FEATURE_S *feat;
	char      *featname;

	if(struncmp(arg, "no-", 3) == 0)
	  featname = arg+3;
	else
	  featname = arg;

	for(i = 0; (feat = feature_list(i)) != NULL; i++){
	    if(strucmp(featname, feat->name) == 0){
		v = &(ps_global->vars)[V_FEATURE_LIST];
		p1 = arg;
		break;
	    }
	}
    }

    if(!p1)
      return 0;

    if(v->is_obsolete || !v->is_user){
	char buf[MAILTMPLEN];

	if(v->is_obsolete)
	  snprintf(buf, sizeof(buf), _("Option \"%s\" is obsolete\n"), v->name);
	else
	  snprintf(buf, sizeof(buf), _("Option \"%s\" is not user settable\n"), v->name);

	exceptional_exit(buf, -1);
    }

    /* free mem */
    if(v->is_list){
	oldlvalue = v->cmdline_val.l;
	v->cmdline_val.l = NULL;
    }
    else if(v->cmdline_val.p)
      fs_give((void **) &(v->cmdline_val.p));

    /*----- Matched a variable, get its value ----*/
    while(*p1 == ' ' || *p1 == '\t')
      p1++;
    value = p1;

    if(*value == '\0'){
      if(v->is_list){
        v->cmdline_val.l = (char **)fs_get(2 * sizeof(char *));
	/*
	 * we let people leave off the quotes on command line so that
	 * they don't have to mess with shell quoting
	 */
        v->cmdline_val.l[0] = cpystr("");
        v->cmdline_val.l[1] = NULL;
	if(oldlvalue)
	  free_list_array(&oldlvalue);

      }else{
        v->cmdline_val.p = cpystr("");
      }
      return 1;
    }

    /*--value is non-empty--*/
    if(*value == '"' && !v->is_list){
      value++;
      for(p1 = value; *p1 && *p1 != '"'; p1++);
        if(*p1 == '"')
          *p1 = '\0';
        else
          removing_trailing_white_space(value);
    }else{
      removing_trailing_white_space(value);
    }

    if(v->is_list){
      int   was_quoted = 0;
      char *error      = NULL;

      count = 1;
      for(p1=value; *p1; p1++){		/* generous count of list elements */
        if(*p1 == '"')			/* ignore ',' if quoted   */
          was_quoted = (was_quoted) ? 0 : 1;

        if(*p1 == ',' && !was_quoted)
          count++;
      }

      lvalue = parse_list(value, count, 0, &error);
      if(error){
	  char buf[MAILTMPLEN];

	  snprintf(buf, sizeof(buf), "%s in %s = \"%s\"\n", error, v->name, value);
	  buf[sizeof(buf)-1] = '\0';
	  exceptional_exit(buf, -1);
      }
      /*
       * Special case: turn "" strings into empty strings.
       * This allows users to turn off default lists.  For example,
       * if smtp-server is set then a user could override smtp-server
       * with smtp-server="".
       */
      for(i = 0; lvalue[i]; i++)
	if(lvalue[i][0] == '"' &&
	   lvalue[i][1] == '"' &&
	   lvalue[i][2] == '\0')
	     lvalue[i][0] = '\0';
    }

    if(v->is_list){
	if(oldlvalue){
	    char **combinedlvalue;
	    int    j;

	    /* combine old and new cmdline lists */
	    for(count = 0, i = 0; oldlvalue[i]; i++, count++)
	      ;

	    for(i = 0; lvalue && lvalue[i]; i++, count++)
	      ;

	    combinedlvalue = (char **) fs_get((count+1) * sizeof(char *));
	    memset(combinedlvalue, 0, (count+1) * sizeof(*combinedlvalue));

	    for(i = 0, j = 0; oldlvalue[i]; i++, j++)
	      combinedlvalue[j] = cpystr(oldlvalue[i]);

	    for(i = 0; lvalue && lvalue[i]; i++, j++)
	      combinedlvalue[j] = cpystr(lvalue[i]);

	    v->cmdline_val.l = combinedlvalue;

	    fs_give((void **) &oldlvalue);
	    if(lvalue)
	      fs_give((void **) &lvalue);
	}
	else
	  v->cmdline_val.l = lvalue;
    }
    else
      v->cmdline_val.p = cpystr(value);

    return 1;
}
