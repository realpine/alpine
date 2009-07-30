#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: fltrname.c 769 2007-10-24 00:15:40Z hubert@u.washington.edu $";
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

#include "../c-client/mail.h"	/* for MAILSTREAM */
#undef ERROR

#include <system.h>
#include <general.h>

#include "../../pith/osdep/writ_dir.h"
#include "../../pith/osdep/bldpath.h"

#include "fltrname.h"


/*----------------------------------------------------------------------
    Filter file names for strange characters

   Args:  file  -- the file name to check
 
 Result: Returns NULL if file name is OK
         Returns formatted error message if it is not
  ----*/
char *
filter_filename(char *file, int *fatal, int strict)
{
#define ALLOW_WEIRD 1
#ifdef ALLOW_WEIRD
    static char illegal[] = {'\177', '\0'};
#else
#ifdef	_WINDOWS
    static char illegal[] = {'"', '#', '$', '%', '&', /*'/',*/ '(', ')','*',
                          ',', ';', '<', '=', '>', '?', '[', ']',
                          '^', '|', '\177', '\0'};
#else	/* UNIX */
    static char illegal[] = {'"', '#', '$', '%', '&', '\'','(', ')','*',
                          ',', ':', ';', '<', '=', '>', '?', '[', ']',
                          '\\', '^', '|', '\177', '\0'};
#endif	/* UNIX */
#endif
    static char error[100];
    char ill_file[MAXPATH+1], *ill_char, *ptr, e2[20];
    int i;

    for(ptr = file; *ptr == ' '; ptr++) ; /* leading spaces gone */

    while(*ptr && (unsigned char)(*ptr) >= ' ' && strchr(illegal, *ptr) == 0)
      ptr++;

    *fatal = TRUE;
    if(*ptr != '\0') {
        if(*ptr == '\n') {
            ill_char = "<newline>";
        } else if(*ptr == '\r') {
            ill_char = "<carriage return>";
        } else if(*ptr == '\t') {
    	    ill_char = "<tab>";
	    *fatal = FALSE;		/* just whitespace */
        } else if(*ptr < ' ') {
            snprintf(e2, sizeof(e2), "control-%c", *ptr + '@');
            ill_char = e2;
        } else if (*ptr == '\177') {
    	    ill_char = "<del>";
        } else {
    	    e2[0] = *ptr;
    	    e2[1] = '\0';
    	    ill_char = e2;
	    *fatal = FALSE;		/* less offensive? */
        }
	if(!*fatal){
	    strncpy(error, ill_char, sizeof(error)-1);
	    error[sizeof(error)-1] = '\0';
	}
        else if(ptr != file) {
            strncpy(ill_file, file, MIN(ptr-file,sizeof(ill_file)-1));
            ill_file[MIN(ptr-file,sizeof(ill_file)-1)] = '\0';
	    snprintf(error, sizeof(error),
		    "Character \"%s\" after \"%.*s\" not allowed in file name",
		    ill_char, sizeof(error)-50, ill_file);
        } else {
            snprintf(error, sizeof(error),
                    "First character, \"%s\", not allowed in file name",
                    ill_char);
        }
            
        return(error);
    }

    if((i=is_writable_dir(file)) == 0 || i == 1){
	snprintf(error, sizeof(error), "\"%.*s\" is a directory", sizeof(error)-50, file);
        return(error);
    }

    if(strict){
	for(ptr = file; *ptr == ' '; ptr++) ;	/* leading spaces gone */

	if((ptr[0] == '.' && ptr[1] == '.') || filename_parent_ref(ptr)){
	    snprintf(error, sizeof(error), "\"..\" not allowed in filename");
	    return(error);
	}
    }

    return((char *)NULL);
}
