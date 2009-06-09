#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: pattern.c 169 2006-10-04 23:26:48Z hubert@u.washington.edu $";
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

#include "../c-client/c-client.h"

#include "../pith/osdep/canaccess.h"
#include "../pith/osdep/color.h"
#include "../pith/osdep/pipe.h"

#include "../pith/charconv/utf8.h"
#include "../pith/charconv/filesys.h"

#include "../pith/status.h"
#include "../pith/pipe.h"
#include "../pith/debug.h"
#include "../pith/detach.h"

#include "pipe.h"
#include "pattern.h"
#include "signal.h"


/* Internal prototypes */
int	test_message_with_cmd(MAILSTREAM *, long, char *, long, int *);


/*
 * pattern_filter_command - filter given message
 */
void
pattern_filter_command(char **cat_cmds, SEARCHSET *srchset, MAILSTREAM *stream, long cat_lim, INTVL_S *cat)
{
    char **l;
    int exitval, i;
    SEARCHSET *s;
    MESSAGECACHE *mc;
    char *cmd = NULL;
    char *just_arg0 = NULL;
    char *cmd_start, *cmd_end;
    int just_one;

    if(!(cat_cmds && cat_cmds[0]))
      return;

    just_one = !(cat_cmds[1]);

    /* find the first command that exists on this host */
    for(l = cat_cmds; l && *l; l++){
	cmd = cpystr(*l);
	removing_quotes(cmd);
	if(cmd){
	    for(cmd_start = cmd;
		*cmd_start && isspace(*cmd_start); cmd_start++)
	      ;
			    
	    for(cmd_end = cmd_start+1;
		*cmd_end && !isspace(*cmd_end); cmd_end++)
	      ;
			    
	    just_arg0 = (char *) fs_get((cmd_end-cmd_start+1)
					* sizeof(char));
	    strncpy(just_arg0, cmd_start, cmd_end - cmd_start);
	    just_arg0[cmd_end - cmd_start] = '\0';
	}

	if(valid_filter_command(&just_arg0))
	  break;
	else{
	    if(just_one){
		if(can_access(just_arg0, ACCESS_EXISTS) != 0)
		  q_status_message1(SM_ORDER, 0, 3,
				    "\"%s\" does not exist",
				    just_arg0);
		else
		  q_status_message1(SM_ORDER, 0, 3,
				    "\"%s\" is not executable",
				    just_arg0);
	    }

	    if(just_arg0)
	      fs_give((void **) &just_arg0);
	    if(cmd)
	      fs_give((void **) &cmd);
	}
    }

    if(!just_arg0 && !just_one)
      q_status_message(SM_ORDER, 0, 3,
		       "None of the category cmds exists and is executable");

    /*
     * If category_cmd isn't executable, it isn't a match.
     */
    if(!just_arg0 || !cmd){
	/*
	 * If we couldn't run the pipe command,
	 * we declare no match
	 */
	for(s = srchset; s; s = s->next)
	  for(i = s->first; i <= s->last; i++)
	    if(i > 0L && stream && i <= stream->nmsgs
	       && (mc=mail_elt(stream, i)) && mc->searched)
	      mc->searched = NIL;
    }
    else
      for(s = srchset; s; s = s->next)
	for(i = s->first; i <= s->last; i++)
	  if(i > 0L && stream && i <= stream->nmsgs
	     && (mc=mail_elt(stream, i)) && mc->searched){

	      /*
	       * If there was an error, or the exitval is out of
	       * range, then it is not a match.
	       * Default range is (0,0),
	       * which is right for matching
	       * bogofilter spam.
	       */
	      if(test_message_with_cmd(stream, i, cmd,
				       cat_lim, &exitval) != 0)
		mc->searched = NIL;

	      /* test exitval */
	      if(mc->searched){
		  INTVL_S *iv;

		  if(cat){
		      for(iv = cat; iv; iv = iv->next)
			if((long) exitval >= iv->imin
			   && (long) exitval <= iv->imax)
			  break;
				    
		      if(!iv)
			mc->searched = NIL;  /* not in any interval */
		  }
		  else{
		      /* default to interval containing only zero */
		      if(exitval != 0)
			mc->searched = NIL;
		  }
	      }
	  }
		    
    if(just_arg0)
      fs_give((void **) &just_arg0);

    if(cmd)
      fs_give((void **) &cmd);
}



/*
 * Returns 0 if ok, -1 if not ok.
 * If ok then exitval contains the exit value of the cmd.
 */
int
test_message_with_cmd(MAILSTREAM *stream, long int msgno, char *cmd,
		      long char_limit,	/* limit testing to this many chars from body */
		      int *exitval)
{
    PIPE_S  *tpipe;
    gf_io_t  pc;
    int      status = 0, flags, err = 0;
    char    *resultfile = NULL, *pipe_err;

    if(cmd && cmd[0]){
	
	flags = PIPE_WRITE | PIPE_NOSHELL | PIPE_STDERR | PIPE_NONEWMAIL;

	dprint((7, "test_message_with_cmd(msgno=%ld cmd=%s)\n",
		msgno, cmd));

	if((tpipe = cmd_pipe_open(cmd, &resultfile, flags, &pc))){

	    prime_raw_pipe_getc(stream, msgno, char_limit, FT_PEEK);
	    gf_filter_init();
	    gf_link_filter(gf_nvtnl_local, NULL);
	    if((pipe_err = gf_pipe(raw_pipe_getc, pc)) != NULL){
		q_status_message1(SM_ORDER|SM_DING, 3, 3,
				  "Internal Error: %.200s", pipe_err);
		err++;
	    }

	    /*
	     * Don't call new_mail in close_system_pipe because we're probably
	     * already here from new_mail and we don't want to get loopy.
	     */
	    status = close_system_pipe(&tpipe, exitval, pipe_callback);

	    /*
	     * This is a place where the command can put its output, which we
	     * are not interested in.
	     */
	    if(resultfile){
		our_unlink(resultfile);
		fs_give((void **) &resultfile);
	    }

	    return((err || status) ? -1 : 0);
	}
    }

    return(-1);
}


