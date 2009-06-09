#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: pipe.c 155 2006-09-29 23:28:46Z hubert@u.washington.edu $";
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


#include "../pith/headers.h"
#include "../pith/pipe.h"
#include "../pith/stream.h"
#include "../pith/status.h"

#include "signal.h"
#include "pipe.h"


/*
 * Support structure and functions to support piping raw message texts...
 */
static struct raw_pipe_data {
    MAILSTREAM   *stream;
    unsigned long msgno, len;
    long	  char_limit, flags;
    char         *cur, *body;
} raw_pipe;


int
raw_pipe_getc(unsigned char *c)
{
    static char *free_this = NULL;

    /*
     * What is this if doing?
     *
     * If((just_starting && unsuccessful_fetch_header)
     *    || (no_chars_available && haven't_fetched_body
     *        && (not_supposed_to_fetch
     *            || (supposed_to_fetch_all && unsuccessful_fetch_text)
     *            || (supposed_to_partial_fetch && unsuccessful_partial_fetch))
     *    || (no_chars_available))
     *   return(0);
     *
     * otherwise, fall through and return next character
     */
    if((!raw_pipe.cur
        && !(raw_pipe.cur = mail_fetch_header(raw_pipe.stream, raw_pipe.msgno,
					      NULL, NULL,
					      &raw_pipe.len,
					      raw_pipe.flags)))
       || ((raw_pipe.len <= 0L) && !raw_pipe.body
           && (raw_pipe.char_limit == 0L
	       || (raw_pipe.char_limit < 0L
	           && !(raw_pipe.cur = raw_pipe.body =
				   pine_mail_fetch_text(raw_pipe.stream,
							raw_pipe.msgno,
							NULL,
							&raw_pipe.len,
							raw_pipe.flags)))
	       || (raw_pipe.char_limit > 0L
	           && !(raw_pipe.cur = raw_pipe.body =
		        pine_mail_partial_fetch_wrapper(raw_pipe.stream,
					   raw_pipe.msgno,
					   NULL,
					   &raw_pipe.len,
					   raw_pipe.flags,
					   (unsigned long) raw_pipe.char_limit,
					   &free_this, 1)))))
       || (raw_pipe.len <= 0L)){

	if(free_this)
	  fs_give((void **) &free_this);

	return(0);
    }

    if(raw_pipe.char_limit > 0L
       && raw_pipe.body
       && raw_pipe.len > raw_pipe.char_limit)
      raw_pipe.len = raw_pipe.char_limit;

    if(raw_pipe.len > 0L){
	*c = (unsigned char) *raw_pipe.cur++;
	raw_pipe.len--;
	return(1);
    }
    else
      return(0);

}


/*
 * Set up for using raw_pipe_getc
 *
 * Args: stream
 *       msgno
 *       char_limit  Set to -1 means whole thing
 *                           0 means headers only
 *                         > 0 means headers plus char_limit body chars
 *       flags -- passed to fetch functions
 */
void
prime_raw_pipe_getc(MAILSTREAM *stream, long int msgno, long int char_limit, long int flags)
{
    raw_pipe.stream     = stream;
    raw_pipe.msgno      = (unsigned long) msgno;
    raw_pipe.char_limit = char_limit;
    raw_pipe.len        = 0L;
    raw_pipe.flags      = flags;
    raw_pipe.cur        = NULL;
    raw_pipe.body       = NULL;
}


/*----------------------------------------------------------------------
  Actually open the pipe used to write piped data down

   Args: 
   Returns: TRUE if success, otherwise FALSE

  ----*/
PIPE_S *
cmd_pipe_open(char *cmd, char **result, int flags, gf_io_t *pc)
{
    char    err[200];
    PIPE_S *pipe;

    if((pipe = open_system_pipe(cmd, result, NULL, flags, 0,
			       pipe_callback, pipe_report_error)) != NULL)
      gf_set_writec(pc, pipe, 0L, PipeStar, (flags & PIPE_RAW) ? 0 : WRITE_TO_LOCALE);
    else{
	/* TRANSLATORS: The argument is the command name being piped to. */
	snprintf(err, sizeof(err), _("Error opening pipe: %s"), cmd ? cmd : "?");
	q_status_message(SM_ORDER | SM_DING, 3, 3, err) ;
    }

    return(pipe);
}
