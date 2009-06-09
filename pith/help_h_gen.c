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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void preamble(FILE *ofp);
void body(FILE *ifp, FILE *ofp);
void postamble(FILE *ofp);


int
main(int argc, char **argv)
{
    preamble(stdout);
    body(stdin, stdout);
    postamble(stdout);
    exit(0);
}


void
preamble(FILE *ofp)
{
    fprintf(ofp, "\n\t\t/*\n");
    fprintf(ofp, "\t\t * AUTMATICALLY GENERATED FILE!\n");
    fprintf(ofp, "\t\t * DO NOT EDIT!!\n");
    fprintf(ofp, "\t\t * See help_h_gen.c.\n\t\t */\n\n\n");
    fprintf(ofp, "#ifndef PITH_HELPTEXT_INCLUDED\n");
    fprintf(ofp, "#define PITH_HELPTEXT_INCLUDED\n\n\n");
    fprintf(ofp, "#define\tHelpType\tchar **\n");
    fprintf(ofp, "#define\tNO_HELP\t((char **) NULL)\n\n");
    fprintf(ofp, "struct help_texts {\n");
    fprintf(ofp, "    HelpType help_text;\n");
    fprintf(ofp, "    char  *tag;\n};\n\n");
}


void
body(FILE *ifp, FILE *ofp)
{
    char  line[10000];
    char *space = " ";
    char *p;

    while(fgets(line, sizeof(line), ifp) != NULL){
	if(!strncmp(line, "====", 4)){
	    p = strtok(line, space);
	    if(p){
		p = strtok(NULL, space);
		if(p){
		    if(isalpha(*p))
		      fprintf(ofp, "extern char *%s[];\n", p);
		    else{
			fprintf(ofp, "Error: help input line\n  %s\nis bad\n", line);
			exit(-1);
		    }
		}
		else{
		    fprintf(ofp, "Error: help input\n  %scontains ==== without following helpname\n", line);
		    exit(-1);
		}

	    }
	    else{
		fprintf(ofp, "Error: help input\n  %scontains ==== without following space\n", line);
		exit(-1);
	    }
	}
    }
}


void
postamble(FILE *ofp)
{
    fprintf(ofp, "\nextern struct help_texts h_texts[];\n\n\n");
    fprintf(ofp, "#endif /* PITH_HELPTEXT_INCLUDED */\n");
}
