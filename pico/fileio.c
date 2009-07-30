#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: fileio.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
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
 *
 * Program:	file reading routines
 *
 */

/*
 * The routines in this file read and write ASCII files from the disk. All of
 * the knowledge about files are here. A better message writing scheme should
 * be used.
 */
#include	"headers.h"
#include	"../pith/charconv/filesys.h"


#if	defined(bsd) || defined(lnx)
extern int errno;
#endif



FIOINFO	g_pico_fio;

/*
 * Open a file for reading.
 */
int
ffropen(char *fn)
{
    int status;

    if ((status = fexist(g_pico_fio.name = fn, "r", (off_t *)NULL)) == FIOSUC){
	g_pico_fio.flags = FIOINFO_READ;
	if((g_pico_fio.fp = our_fopen(g_pico_fio.name, "r")) == NULL)
	  status = FIOFNF;
    }

    return (status);
}


/*
 * Write a line to the already opened file. The "buf" points to the buffer,
 * and the "nbuf" is its length, less the free newline. Return the status.
 * Check only at the newline.
 */
int
ffputline(CELL buf[], int nbuf)
{
    register int    i;
    EML eml;

    for(i = 0; i < nbuf; ++i)
       if(write_a_wide_char((UCS) buf[i].c, g_pico_fio.fp) == EOF)
	 break;

   if(i == nbuf)
     write_a_wide_char((UCS) '\n', g_pico_fio.fp);

    if(ferror(g_pico_fio.fp)){
	eml.s = errstr(errno);
        emlwrite("\007Write error: %s", &eml);
	sleep(5);
        return FIOERR;
    }

    return FIOSUC;
}


/*
 * Read a line from a file, and store the bytes in the supplied buffer. The
 * "nbuf" is the length of the buffer. Complain about long lines and lines
 * at the end of the file that don't have a newline present. Check for I/O
 * errors too. Return status.
 *
 * Translate the line from the user's locale charset to UCS-4.
 */
int
ffgetline(UCS buf[], size_t nbuf, size_t *charsreturned, int msg)
{
    size_t i;
    UCS    ucs;

    if(charsreturned)
      *charsreturned = 0;

    i = 0;

    while((ucs = read_a_wide_char(g_pico_fio.fp, input_cs)) != CCONV_EOF && ucs != '\n'){
	/*
	 * Don't blat the CR should the newline be CRLF and we're
	 * running on a unix system.  NOTE: this takes care of itself
	 * under DOS since the non-binary open turns newlines into '\n'.
	 */
	if(ucs == '\r'){
	    if((ucs = read_a_wide_char(g_pico_fio.fp, input_cs)) == CCONV_EOF || ucs == '\n')
	      break;

	    if(i < nbuf-2)		/* Bare CR. Insert it and go on... */
	      buf[i++] = '\r';		/* else, we're up a creek */
	}

        if(i >= nbuf-2){
	    buf[nbuf - 2] = ucs;	/* store last char read */
	    buf[nbuf - 1] = '\0';	/* and terminate it */
	    if(charsreturned)
	      *charsreturned = nbuf - 1;

	    if(msg)
	      emlwrite("File has long line", NULL);

	    return FIOLNG;
        }

        buf[i++] = ucs;
    }

    if(ucs == CCONV_EOF){
        if(ferror(g_pico_fio.fp)){
            emlwrite("File read error", NULL);
	    if(charsreturned)
	      *charsreturned = i;

	    return FIOERR;
        }

        if(i != 0)
	  emlwrite("File doesn't end with newline.  Adding one.", NULL);
	else{
	    if(charsreturned)
	      *charsreturned = i;
	    return FIOEOF;
	}
    }

    buf[i] = '\0';

    if(charsreturned)
      *charsreturned = i;

    return FIOSUC;
}
