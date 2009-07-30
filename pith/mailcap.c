#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: mailcap.c 1012 2008-03-26 00:44:22Z hubert@u.washington.edu $";
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

#include "../pith/headers.h"
#include "../pith/mailcap.h"
#include "../pith/init.h"
#include "../pith/conf.h"
#include "../pith/mimetype.h"
#include "../pith/mimedesc.h"
#include "../pith/status.h"
#include "../pith/util.h"
#include "../pith/readfile.h"

/*
 * We've decided not to implement the RFC1524 standard minimum path, because
 * some of us think it is harder to debug a problem when you may be misled
 * into looking at the wrong mailcap entry.  Likewise for MIME.Types files.
 */
#if defined(DOS) || defined(OS2)
#define MC_PATH_SEPARATOR ';'
#define	MC_USER_FILE	  "MAILCAP"
#define MC_STDPATH        NULL
#else /* !DOS */
#define MC_PATH_SEPARATOR ':'
#define	MC_USER_FILE	  NULL
#define MC_STDPATH         \
		".mailcap:/etc/mailcap:/usr/etc/mailcap:/usr/local/etc/mailcap"
#endif /* !DOS */

#ifdef	_WINDOWS
#define	MC_ADD_TMP	" %s"
#else
#define	MC_ADD_TMP	" < %s"
#endif

typedef struct mcap_entry {
    struct mcap_entry *next;
    int                needsterminal;
    char              *contenttype;
    char              *command;
    char              *testcommand;
    char              *label;           /* unused */
    char              *printcommand;    /* unused */
} MailcapEntry;

struct	mailcap_data {
    MailcapEntry *head, **tail;
    STRINGLIST	 *raw;
} MailcapData;

#define	MC_TOKEN_MAX	64


/*
 * Internal prototypes
 */
void          mc_init(void);
void          mc_process_file(char *);
void          mc_parse_file(char *);
int           mc_parse_line(char **, char **);
int           mc_comment(char **);
int           mc_token(char **, char **);
void          mc_build_entry(char **);
int           mc_sane_command(char *);
MailcapEntry *mc_get_command(int, char *, BODY *, int, int *);
int           mc_ctype_match(int, char *, char *);
int           mc_passes_test(MailcapEntry *, int, char *, BODY *);
char         *mc_bld_test_cmd(char *, int, char *, BODY *);
char         *mc_cmd_bldr(char *, int, char *, BODY *, char *, char **);
MailcapEntry *mc_new_entry(void);
void          mc_free_entry(MailcapEntry **);


char *
mc_conf_path(char *def_path, char *env_path, char *user_file, int separator, char *stdpath)
{
    char *path;

    /* We specify MIMETYPES as a path override */
    if(def_path)
      /* there may need to be an override specific to pine */
      path = cpystr(def_path);
    else if(env_path)
      path = cpystr(env_path);
    else{
#if defined(DOS) || defined(OS2)
	char *s;

        /*
	 * This gets interesting.  Since we don't have any standard location
	 * for config/data files, look in the same directory as the PINERC
	 * and the same dir as PINE.EXE.  This is similar to the UNIX
	 * situation with personal config info coming before 
	 * potentially shared config data...
	 */
	if(s = last_cmpnt(ps_global->pinerc)){
	    strncpy(tmp_20k_buf+1000, ps_global->pinerc, MIN(s - ps_global->pinerc,SIZEOF_20KBUF-1000));
	    tmp_20k_buf[1000+MIN(s - ps_global->pinerc,SIZEOF_20KBUF-1000-1)] = '\0';
	}
	else
	  strncpy(tmp_20k_buf+1000, ".\\", SIZEOF_20KBUF-1000);

	/* pinerc directory version of file */
	build_path(tmp_20k_buf+2000, tmp_20k_buf+1000, user_file, SIZEOF_20KBUF-2000);
	tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';

	/* pine.exe directory version of file */
	build_path(tmp_20k_buf+3000, ps_global->pine_dir, user_file, SIZEOF_20KBUF-3000);
	tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';

	/* combine them */
	snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%s%c%s", tmp_20k_buf+2000, separator, tmp_20k_buf+3000);

#else	/* !DOS */
	build_path(tmp_20k_buf, ps_global->home_dir, stdpath, SIZEOF_20KBUF);
#endif	/* !DOS */
	path = cpystr(tmp_20k_buf);
    }

    return(path);
}


/*
 * mc_init - Run down the path gathering all the mailcap entries.
 *           Returns with the Mailcap list built.
 */
void
mc_init(void)
{
    char *s,
	 *pathcopy,
	 *path,
	  image_viewer[MAILTMPLEN];
  
    if(MailcapData.raw)		/* already have the file? */
      return;
    else
      MailcapData.tail = &MailcapData.head;

    dprint((5, "- mc_init -\n"));

    pathcopy = mc_conf_path(ps_global->VAR_MAILCAP_PATH, getenv("MAILCAPS"),
			    MC_USER_FILE, MC_PATH_SEPARATOR, MC_STDPATH);

    path = pathcopy;			/* overloaded "path" */

    /*
     * Insert an entry for the image-viewer variable from .pinerc, if present.
     */
    if(ps_global->VAR_IMAGE_VIEWER && *ps_global->VAR_IMAGE_VIEWER){
	MailcapEntry *mc = mc_new_entry();

	snprintf(image_viewer, sizeof(image_viewer), "%s %%s", ps_global->VAR_IMAGE_VIEWER);

	MailcapData.raw		   = mail_newstringlist();
	MailcapData.raw->text.data = (unsigned char *) cpystr(image_viewer);
	mc->command		   = (char *) MailcapData.raw->text.data;
	mc->contenttype		   = "image/*";
	mc->label		   = "Alpine Image Viewer";
	dprint((5, "mailcap: using image-viewer=%s\n", 
		   ps_global->VAR_IMAGE_VIEWER
		     ? ps_global->VAR_IMAGE_VIEWER : "?"));
    }

    dprint((7, "mailcap: path: %s\n", path ? path : "?"));
    while(path){
	s = strindex(path, MC_PATH_SEPARATOR);
	if(s)
	  *s++ = '\0';
	mc_process_file(path);
	path = s;
    }
  
    if(pathcopy)
      fs_give((void **)&pathcopy);

#ifdef DEBUG
    if(debug >= 11){
	MailcapEntry *mc;
	int i = 0;

	dprint((11, "Collected mailcap entries\n"));
	for(mc = MailcapData.head; mc; mc = mc->next){

	    dprint((11, "%d: ", i++));
	    if(mc->label)
	      dprint((11, "%s\n", mc->label ? mc->label : "?"));
	    if(mc->contenttype)
	      dprint((11, "   %s",
		     mc->contenttype ? mc->contenttype : "?"));
	    if(mc->command)
	      dprint((11, "   command: %s\n",
		     mc->command ? mc->command : "?"));
	    if(mc->testcommand)
	      dprint((11, "   testcommand: %s",
		     mc->testcommand ? mc->testcommand : "?"));
	    if(mc->printcommand)
	      dprint((11, "   printcommand: %s",
		     mc->printcommand ? mc->printcommand : "?"));
	    dprint((11, "   needsterminal %d\n", mc->needsterminal));
	}
    }
#endif /* DEBUG */
}


/*
 * Add all the entries from this file onto the Mailcap list.
 */
void
mc_process_file(char *file)
{
    char filebuf[MAXPATH+1], *file_data;

    dprint((5, "mailcap: process_file: %s\n", file ? file : "?"));

    (void)strncpy(filebuf, file, MAXPATH);
    filebuf[MAXPATH] = '\0';
    file = fnexpand(filebuf, sizeof(filebuf));
    dprint((7, "mailcap: processing file: %s\n", file ? file : "?"));
    switch(is_writable_dir(file)){
      case 0: case 1: /* is a directory */
	dprint((1, "mailcap: %s is a directory, should be a file\n",
	    file ? file : "?"));
	return;

      case 2: /* ok */
	break;

      case 3: /* doesn't exist */
	dprint((5, "mailcap: %s doesn't exist\n", file ? file : "?"));
	return;

      default:
	panic("Programmer botch in mc_process_file");
	/*NOTREACHED*/
    }

    if((file_data = read_file(file, READ_FROM_LOCALE)) != NULL){
	STRINGLIST *newsl, **sl;

	/* Create a new container */
	newsl		 = mail_newstringlist();
	newsl->text.data = (unsigned char *) file_data;

	/* figure out where in the list it should go */
	for(sl = &MailcapData.raw; *sl; sl = &((*sl)->next))
	  ;

	*sl = newsl;			/* Add it to the list */

	mc_parse_file(file_data);	/* the process mailcap data */
    }
    else
      dprint((5, "mailcap: %s can't be read\n", file ? file : "?"));
}


void
mc_parse_file(char *file)
{
    char *tokens[MC_TOKEN_MAX];

    while(mc_parse_line(&file, tokens))
      mc_build_entry(tokens);
}


int
mc_parse_line(char **line, char **tokens)
{
    char **tokenp = tokens;

    while(mc_comment(line))		/* skip comment lines */
      ;

    while(mc_token(tokenp, line))	/* collect ';' delim'd tokens */
      if(++tokenp - tokens >= MC_TOKEN_MAX)
	fatal("Ran out of tokens parsing mailcap file");	/* outch! */

    *++tokenp = NULL;			/* tie off list */
    return(*tokens != NULL);
}


/*
 * Retuns 1 if line is a comment, 0 otherwise
 */
int
mc_comment(char **line)
{
    if(**line == '\n'){		/* blank line is a comment, too */
	(*line)++;
	return(1);
    }

    if(**line == '#'){
	while(**line)			/* !EOF */
	  if(*++(*line) == '\n'){	/* EOL? */
	      (*line)++;
	      break;
	  }

	return(1);
    }

    return(0);
}


/*
 * Retuns 0 if EOL, 1 otherwise
 */
int
mc_token(char **token, char **line)
{
    int   rv = 0;
    char *start, *wsp = NULL;

    *token = NULL;			/* init the slot for this token */

    /* skip leading white space */
    while(**line && isspace((unsigned char) **line))
      (*line)++;

    start = *line;

    /* Then see what's left */
    while(1)
      switch(**line){
	case ';' :			/* End-Of-Token */
	  rv = 1;			/* let caller know more follows */
	case '\n' :			/* EOL */
	  if(wsp)
	    *wsp = '\0';		/* truncate white space? */
	  else
	    *start = '\0';		/* if we have a token, tie it off  */

	  (*line)++;			/* and get ready to parse next one */

	  if(rv == 1){			/* ignore trailing semicolon */
	      while(**line){
		  if(**line == '\n')
		    rv = 0;
		  
		  if(isspace((unsigned char) **line))
		    (*line)++;
		  else
		    break;
	      }
	  }

	case '\0' :			/* EOF */
	  return(rv);

	case '\\' :			/* Quoted char */
	  (*line)++;
#if	defined(DOS) || defined(OS2)
	  /*
	   * RFC 1524 says that backslash is used to quote
	   * the next character, but since backslash is part of pathnames
	   * on DOS we're afraid people will not put double backslashes
	   * in their mailcap files.  Therefore, we violate the RFC by
	   * looking ahead to the next character.  If it looks like it
	   * is just part of a pathname, then we consider a single
	   * backslash to *not* be a quoting character, but a literal
	   * backslash instead.
	   *
	   * SO: 
	   * If next char is any of these, treat the backslash
	   * that preceded it like a regular character.
	   */
	  if(**line && isascii(**line)
	     && (isalnum((unsigned char) **line) || strchr("_+-=~" , **line))){
	      *start++ = '\\';
	      wsp = NULL;
	      break;
	  }
	  else
#endif  /* !DOS */

	   if(**line == '\n'){		/* quoted line break */
	       *start = ' ';
	       (*line)++;		/* just move on */
	       while(isspace((unsigned char) **line))
		 (*line)++;

	       break;
	   }
	   else if(**line == '%')	/* quoted '%' becomes "%%" */
	     *--(*line) = '%';		/* overwrite '\' !! */

	   /* Fall thru and copy/advance pointers*/

	default :
	  if(!*token)
	    *token = start;

	  *start = *(*line)++;
	  wsp    = (isspace((unsigned char) *start) && !wsp) ? start : NULL;
	  start++;
	  break;
      }
}


void
mc_build_entry(char **tokens)
{
    MailcapEntry *mc;

    if(!tokens[0]){
	dprint((5, "mailcap: missing content type!\n"));
	return;
    }
    else if(!tokens[1] || !mc_sane_command(tokens[1])){
	dprint((5, "mailcap: missing/bogus command!\n"));
	return;
    }

    mc		    = mc_new_entry();
    mc->contenttype = *tokens++;
    mc->command     = *tokens++;

    dprint((9, "mailcap: content type: %s\n   command: %s\n",
	       mc->contenttype ? mc->contenttype : "?",
	       mc->command ? mc->command : "?"));

    /* grok options  */
    for( ; *tokens; tokens++){
	char *arg;

	/* legit value? */
	if(!isalnum((unsigned char) **tokens)){
	    dprint((5, "Unknown parameter = \"%s\"", *tokens));
	    continue;
	}

	if((arg = strindex(*tokens, '=')) != NULL){
	    *arg = ' ';
	    while(arg > *tokens && isspace((unsigned char) arg[-1]))
	      arg--;

	    *arg++ = '\0';		/* tie off parm arg */
	    while(*arg && isspace((unsigned char) *arg))
	      arg++;

	    if(!*arg)
	      arg = NULL;
	}

	if(!strucmp(*tokens, "needsterminal")){
	    mc->needsterminal = 1;
	    dprint((9, "mailcap: set needsterminal\n"));
	}
	else if(!strucmp(*tokens, "copiousoutput")){
	    mc->needsterminal = 2;
	    dprint((9, "mailcap: set copiousoutput\n"));
	}
	else if(arg && !strucmp(*tokens, "test")){
	    mc->testcommand = arg;
	    dprint((9, "mailcap: testcommand=%s\n",
		       mc->testcommand ? mc->testcommand : "?"));
	}
	else if(arg && !strucmp(*tokens, "description")){
	    mc->label = arg;
	    dprint((9, "mailcap: label=%s\n",
		   mc->label ? mc->label : "?"));
	}
	else if(arg && !strucmp(*tokens, "print")){
	    mc->printcommand = arg;
	    dprint((9, "mailcap: printcommand=%s\n",
		       mc->printcommand ? mc->printcommand : "?"));
	}
	else if(arg && !strucmp(*tokens, "compose")){
	    /* not used */
	    dprint((9, "mailcap: not using compose=%s\n",
		   arg ? arg : "?"));
	}
	else if(arg && !strucmp(arg, "composetyped")){
	    /* not used */
	    dprint((9, "mailcap: not using composetyped=%s\n",
		   arg ? arg : "?"));
	}
	else if(arg && !strucmp(arg, "textualnewlines")){
	    /* not used */
	    dprint((9,
		       "mailcap: not using texttualnewlines=%s\n",
		       arg ? arg : "?"));
	}
	else if(arg && !strucmp(arg, "edit")){
	    /* not used */
	    dprint((9, "mailcap: not using edit=%s\n",
		   arg ? arg : "?"));
	}
	else if(arg && !strucmp(arg, "x11-bitmap")){
	    /* not used */
	    dprint((9, "mailcap: not using x11-bitmap=%s\n",
		   arg ? arg : "?"));
	}
	else
	  dprint((9, "mailcap: ignoring unknown flag: %s\n",
		     arg ? arg : "?"));
    }
}


/*
 * Tests for mailcap defined command's sanity
 */
int
mc_sane_command(char *command)
{
    /* First, test that a command string actually exists */
    if(command && *command){
#ifdef	LATER
	/*
	 * NOTE: Maybe we'll do this later.  The problem is when the
	 * mailcap's been misconfigured.  We then end up supressing
	 * valuable output when the user actually tries to launch the
	 * spec'd viewer.
	 */

	/* Second, Make sure we can get at it */
	if(can_access_in_path(getenv("PATH"), command,  EXECUTE_ACCESS) >= 0)
#endif
	return(1);
    }

    return(0);			/* failed! */
}


/*
 * Returns the mailcap entry for type/subtype from the successfull
 * mailcap entry, or NULL if none.  Command string still contains % stuff.
 */
MailcapEntry *
mc_get_command(int type, char *subtype, BODY *body,
	       int check_extension, int *sp_handlingp)
{
    MailcapEntry *mc;
    char	  tmp_subtype[256], tmp_ext[16], *ext = NULL;

    dprint((5, "- mc_get_command(%s/%s) -\n",
	       body_type_names(type),
	       subtype ? subtype : "?"));

    if(type == TYPETEXT
       && (!subtype || !strucmp(subtype, "plain"))
       && F_ON(F_SHOW_TEXTPLAIN_INT, ps_global))
      return(NULL);

    mc_init();

    if(check_extension){
	char     *fname;
	MT_MAP_T  e2b;

	/*
	 * Special handling for when we're looking at what's likely
	 * binary application data. Look for a file name extension
	 * that we might use to hook a helper app to.
	 *
	 * NOTE: This used to preclude an "app/o-s" mailcap entry
	 *       since this took precedence.  Now that there are
	 *       typically two scans through the check_extension
	 *       mechanism, the mailcap entry now takes precedence.
	 */
	if((fname = get_filename_parameter(NULL, 0, body, &e2b.from.ext)) != NULL
	   && e2b.from.ext && e2b.from.ext[0]){
	    if(strlen(e2b.from.ext) < sizeof(tmp_ext) - 2){
		strncpy(ext = tmp_ext, e2b.from.ext - 1, sizeof(tmp_ext)); /* remember it */
		tmp_ext[sizeof(tmp_ext)-1] = '\0';
		if(mt_srch_mime_type(mt_srch_by_ext, &e2b)){
		    type = e2b.to.mime.type;		/* mapped type */
		    strncpy(subtype = tmp_subtype, e2b.to.mime.subtype,
			    sizeof(tmp_subtype)-1);
		    tmp_subtype[sizeof(tmp_subtype)-1] = '\0';
		    fs_give((void **) &e2b.to.mime.subtype);
		    body = NULL;		/* the params no longer apply */
		}
	    }

	    fs_give((void **) &fname);
	}
	else{
	    if(fname)
	      fs_give((void **) &fname);

	    return(NULL);
	}
    }

    for(mc = MailcapData.head; mc; mc = mc->next)
      if(mc_ctype_match(type, subtype, mc->contenttype)
	 && mc_passes_test(mc, type, subtype, body)){
	  dprint((9, 
		     "mc_get_command: type=%s/%s, command=%s\n", 
		     body_type_names(type),
		     subtype ? subtype : "?",
		     mc->command ? mc->command : "?"));
	  return(mc);
      }

    if(mime_os_specific_access()){
	static MailcapEntry  fake_mc;
	static char	     fake_cmd[1024];
	char		     tmp_mime_type[256];

	memset(&fake_mc, 0, sizeof(MailcapEntry));
	fake_cmd[0] = '\0';
	fake_mc.command = fake_cmd;

	snprintf(tmp_mime_type, sizeof(tmp_mime_type), "%s/%s", body_types[type], subtype);
	if(mime_get_os_mimetype_command(tmp_mime_type, ext, fake_cmd,
					sizeof(fake_cmd), check_extension, sp_handlingp))
	  return(&fake_mc);
    }

    return(NULL);
}


/*
 * Check whether the pattern "pat" matches this type/subtype.
 * Returns 1 if it does, 0 if not.
 */
int
mc_ctype_match(int type, char *subtype, char *pat)
{
    char *type_name = body_type_names(type);
    int   len = strlen(type_name);

    dprint((5, "mc_ctype_match: %s == %s / %s ?\n",
	       pat ? pat : "?",
	       type_name ? type_name : "?",
	       subtype ? subtype : "?"));

    return(!struncmp(type_name, pat, len)
	   && ((pat[len] == '/'
		&& (!pat[len+1] || pat[len+1] == '*'
		    || !strucmp(subtype, &pat[len+1])))
	       || !pat[len]));
}


/*
 * Run the test command for entry mc to see if this entry currently applies to
 * applies to this type/subtype.
 *
 * Returns 1 if it does pass test (exits with status 0), 0 otherwise.
 */
int
mc_passes_test(MailcapEntry *mc, int type, char *subtype, BODY *body)
{
    char *cmd = NULL;
    int   rv;

    dprint((5, "- mc_passes_test -\n"));

    if(mc->testcommand
       && *mc->testcommand
       && !(cmd = mc_bld_test_cmd(mc->testcommand, type, subtype, body)))
      return(FALSE);	/* couldn't be built */
    
    if(!mc->testcommand || !cmd || !*cmd){
	if(cmd)
	  fs_give((void **)&cmd);

	dprint((7, "no test command, so Pass\n"));
	return 1;
    }

    rv = exec_mailcap_test_cmd(cmd);
    dprint((7, "mc_passes_test: \"%s\" %s (rv=%d)\n",
       cmd ? cmd : "?", rv ? "Failed" : "Passed", rv)) ;

    fs_give((void **)&cmd);

    return(!rv);
}


int
mailcap_can_display(int type, char *subtype, BODY *body, int check_extension)
{
    dprint((5, "- mailcap_can_display -\n"));

    return(mc_get_command(type, subtype, body,
			  check_extension, NULL) != NULL);
}


MCAP_CMD_S *
mailcap_build_command(int type, char *subtype, BODY *body,
		      char *tmp_file, int *needsterm, int chk_extension)
{
    MailcapEntry *mc;
    char         *command, *err = NULL;
    MCAP_CMD_S   *mc_cmd = NULL;
    int           sp_handling = 0;

    dprint((5, "- mailcap_build_command -\n"));

    mc = mc_get_command(type, subtype, body, chk_extension, &sp_handling);
    if(!mc){
	q_status_message(SM_ORDER, 3, 4, "Error constructing viewer command");
	dprint((1,
		   "mailcap_build_command: no command string for %s/%s\n",
		   body_type_names(type), subtype ? subtype : "?"));
	return((MCAP_CMD_S *)NULL);
    }

    if(needsterm)
      *needsterm = mc->needsterminal;

    if(sp_handling)
      command = cpystr(mc->command);
    else if(!(command = mc_cmd_bldr(mc->command, type, subtype, body, tmp_file, &err)) && err && *err)
      q_status_message(SM_ORDER, 5, 5, err);

    dprint((5, "built command: %s\n", command ? command : "?"));

    if(command){
	mc_cmd = (MCAP_CMD_S *)fs_get(sizeof(MCAP_CMD_S));
	mc_cmd->command = command;
	mc_cmd->special_handling = sp_handling;
    }
    return(mc_cmd);
}


/*
 * mc_bld_test_cmd - build the command to test if the given type flies
 *
 *    mc_cmd_bldr's tmp_file argument is NULL as we're not going to
 *    decode and write each and every MIME segment's data to a temp file
 *    when no test's going to use the data anyway.
 */
char *
mc_bld_test_cmd(char *controlstring, int type, char *subtype, BODY *body)
{
    return(mc_cmd_bldr(controlstring, type, subtype, body, NULL, NULL));
}


/*
 * mc_cmd_bldr - construct a command string to execute
 *
 *    If tmp_file is null, then the contents of the given MIME segment
 *    is not provided.  This is useful for building the "test=" string
 *    as it doesn't operate on the segment's data.
 *
 *    The return value is an alloc'd copy of the command to be executed.
 */
char *
mc_cmd_bldr(char *controlstring, int type, char *subtype,
	    BODY *body, char *tmp_file, char **err)
{
    char        *from, *to, *s, *parm;
    int	         prefixed = 0, used_tmp_file = 0;

    dprint((8, "- mc_cmd_bldr -\n"));

    for(from = controlstring, to = tmp_20k_buf; *from; ++from){
	if(prefixed){			/* previous char was % */
	    prefixed = 0;
	    switch(*from){
	      case '%':			/* turned \% into this earlier */
		if(to-tmp_20k_buf < SIZEOF_20KBUF)
		  *to++ = '%';

		break;

	      case 's':			/* insert tmp_file name in cmd */
		if(tmp_file){
		    used_tmp_file = 1;
		    sstrncpy(&to, tmp_file, SIZEOF_20KBUF-(to-tmp_20k_buf));
		}
		else
		  dprint((1,
			     "mc_cmd_bldr: %%s in cmd but not supplied!\n"));

		break;

	      case 't':			/* insert MIME type/subtype */
		/* quote to prevent funny business */
		if(to-tmp_20k_buf < SIZEOF_20KBUF)
		  *to++ = '\'';

		sstrncpy(&to, body_type_names(type), SIZEOF_20KBUF-(to-tmp_20k_buf));

		if(to-tmp_20k_buf < SIZEOF_20KBUF)
		  *to++ = '/';

		sstrncpy(&to, subtype, SIZEOF_20KBUF-(to-tmp_20k_buf));

		if(to-tmp_20k_buf < SIZEOF_20KBUF)
		  *to++ = '\'';

		break;

	      case '{':			/* insert requested MIME param */
		if(F_OFF(F_DO_MAILCAP_PARAM_SUBST, ps_global)){
		    int save;

		    dprint((2, "mc_cmd_bldr: param subs %s\n",
			    from ? from : "?"));
		    if(err){
			if((s = strindex(from, '}')) != NULL){
			  save = *++s;
			  *s = '\0';
			}

			snprintf(tmp_20k_buf, SIZEOF_20KBUF,
			    "Mailcap: see hidden feature %.200s (%%%.200s)",
			    feature_list_name(F_DO_MAILCAP_PARAM_SUBST), from);
			*err = tmp_20k_buf;
			if(s)
			  *s = save;
		    }

		    return(NULL);
		}

		s = strindex(from, '}');
		if(!s){
		    q_status_message1(SM_ORDER, 0, 4,
    "Ignoring ill-formed parameter reference in mailcap file: %.200s", from);
		    break;
		}

		*s = '\0';
		++from;    /* from is the part inside the brackets now */

		parm = parameter_val(body ? body->parameter : NULL, from);

		dprint((9,
			   "mc_cmd_bldr: parameter %s = %s\n", 
			   from ? from : "?", parm ? parm : "(not found)"));

		/*
		 * Quote parameter values for /bin/sh.
		 * Put single quotes around the whole thing but every time
		 * there is an actual single quote put it outside of the
		 * single quotes with a backslash in front of it.  So the
		 * parameter value  fred's car
		 * turns into       'fred'\''s car'
		 */
		if(to-tmp_20k_buf < SIZEOF_20KBUF)
		  *to++ = '\'';  /* opening quote */
		  
		if(parm){
		    char *p;

		    /*
		     * Copy value, but quote single quotes for /bin/sh
		     * Backslash quote is ignored inside single quotes so
		     * have to put those outside of the single quotes.
		     * (The parm+1000 nonsense is to protect against
		     * malicious mail trying to overflow our buffer.)
		     *
		     * TCH - Change 2/8/1999
		     * Also quote the ` to prevent execution of arbitrary code
		     */
		    for(p = parm; *p && p < parm+1000; p++){
			if((*p == '\'') || (*p == '`')){
			    if(to-tmp_20k_buf+4 < SIZEOF_20KBUF){
				*to++ = '\'';  /* closing quote */
				*to++ = '\\';
				*to++ = *p;    /* quoted character */
				*to++ = '\'';  /* opening quote */
			    }
			}
			else if(to-tmp_20k_buf < SIZEOF_20KBUF)
			  *to++ = *p;
		    }

		    fs_give((void **) &parm);
		}

		if(to-tmp_20k_buf < SIZEOF_20KBUF)
		  *to++ = '\'';  /* closing quote for /bin/sh */

		*s = '}'; /* restore */
		from = s;
		break;

		/*
		 * %n and %F are used by metamail to support otherwise
		 * unrecognized multipart Content-Types.  Pine does
		 * not use these since we're only dealing with the individual
		 * parts at this point.
		 */
	      case 'n':
	      case 'F':  
	      default:  
		dprint((9, 
		   "Ignoring %s format code in mailcap file: %%%c\n",
		   (*from == 'n' || *from == 'F') ? "unimplemented"
						  : "unrecognized",
		   *from));
		break;
	    }
	}
	else if(*from == '%')		/* next char is special */
	  prefixed = 1;
	else if(to-tmp_20k_buf < SIZEOF_20KBUF)		/* regular character, just copy */
	  *to++ = *from;
    }

    if(to-tmp_20k_buf < SIZEOF_20KBUF)
      *to = '\0';

    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';

    /*
     * file not specified, redirect to stdin
     */
    if(!used_tmp_file && tmp_file)
      snprintf(to, SIZEOF_20KBUF-(to-tmp_20k_buf), MC_ADD_TMP, tmp_file);

    return(cpystr(tmp_20k_buf));
}


/*
 *
 */
MailcapEntry *
mc_new_entry(void)
{
    MailcapEntry *mc = (MailcapEntry *) fs_get(sizeof(MailcapEntry));
    memset(mc, 0, sizeof(MailcapEntry));
    *MailcapData.tail = mc;
    MailcapData.tail  = &mc->next;
    return(mc);
}


/*
 * Free a list of mailcap entries
 */
void
mc_free_entry(MailcapEntry **mc)
{
    if(mc && *mc){
	mc_free_entry(&(*mc)->next);
	fs_give((void **) mc);
    }
}


void
mailcap_free(void)
{
    mail_free_stringlist(&MailcapData.raw);
    mc_free_entry(&MailcapData.head);
}
