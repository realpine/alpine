#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: pipe.c 761 2007-10-23 22:35:18Z hubert@u.washington.edu $";
#endif
/*
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

#include "../pith/headers.h"
#include "../pith/pipe.h"
#include "../pith/status.h"


/* Internal prototypes */



/*
 * pipe_* functions introduced so out.f, in.f don't need to be used
 * by whatever's setting up the pipe.  
 * Should be similar on unix and windows, but since we're faking piping
 * in windows, closing the write pipe would signify that the child
 * process can start running.  This makes it possible for PIPE_WRITE
 * and PIPE_READ flags to be simultaneously set across all platforms.
 * Unix piping should eventually have NON_BLOCKING_IO stuff rolled up
 * into it.
 */

int
pipe_putc(int c, PIPE_S *syspipe)
{
    if(!syspipe || !syspipe->out.f)
      return -1;

    return(fputc(c, syspipe->out.f));
}

int
pipe_puts(char *str, PIPE_S *syspipe)
{
    if(!syspipe || !syspipe->out.f)
      return -1;

    return(fputs(str, syspipe->out.f));
}

char *
pipe_gets(char *str, int size, PIPE_S *syspipe)
{
    if(!syspipe || !syspipe->in.f)
      return NULL;

    return(fgets(str, size, syspipe->in.f));
}

int
pipe_readc(unsigned char *c, PIPE_S *syspipe)
{
    int rv = 0;

    if(!syspipe || !syspipe->in.f)
      return -1;

    do {
	errno = 0;
	clearerr(syspipe->in.f);
	rv = fread(c, sizeof(unsigned char), (size_t)1, syspipe->in.f);
    } while(!rv && ferror(syspipe->in.f) && errno == EINTR);

    return(rv);
}

int
pipe_writec(int c, PIPE_S *syspipe)
{
    unsigned char ch = (unsigned char)c;
    int rv = 0;

    if(!syspipe || !syspipe->out.f)
      return -1;

    do
      rv = fwrite(&ch, sizeof(unsigned char), (size_t)1, syspipe->out.f);
    while(!rv && ferror(syspipe->out.f) && errno == EINTR);

    return(rv);
}


void
pipe_report_error(char *errormsg)
{
    q_status_message(SM_ORDER, 3, 3, errormsg);
}
