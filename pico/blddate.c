#include <system.h>
#include <general.h>

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

main(argc, argv)
    int    argc;
    char **argv;
{
    struct tm *t;
    FILE      *outfile=stdout;
    time_t     ltime;

    if(argc > 1 && (outfile = fopen(argv[1], "w")) == NULL){
	/* TRANSLATORS: an error message when trying to create a file */
	fprintf(stderr, _("can't create '%s'\n"), argv[1]);
	exit(1);
    }

    time(&ltime);
    t = localtime(&ltime);
    fprintf(outfile,"char datestamp[]=\"%02d-%s-%d\";\n", t->tm_mday, 
	    (t->tm_mon == 0) ? "Jan" : 
	     (t->tm_mon == 1) ? "Feb" : 
	      (t->tm_mon == 2) ? "Mar" : 
	       (t->tm_mon == 3) ? "Apr" : 
		(t->tm_mon == 4) ? "May" : 
		 (t->tm_mon == 5) ? "Jun" : 
		  (t->tm_mon == 6) ? "Jul" : 
		   (t->tm_mon == 7) ? "Aug" : 
		    (t->tm_mon == 8) ? "Sep" : 
		     (t->tm_mon == 9) ? "Oct" : 
		      (t->tm_mon == 10) ? "Nov" : 
		       (t->tm_mon == 11) ? "Dec" : "UKN",
	    1900 + t->tm_year);

    fprintf(outfile, "char hoststamp[]=\"random-pc\";\n");

    fclose(outfile);

    exit(0);
}
