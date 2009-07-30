#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: dispfilt.c 1012 2008-03-26 00:44:22Z hubert@u.washington.edu $";
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
     dispfilt.c
     Display filter technology ;)

  ====*/

#include "headers.h"

#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/filter.h"
#include "../pith/store.h"
#include "../pith/addrstring.h"
#include "../pith/mimedesc.h"
#include "../pith/list.h"
#include "../pith/detach.h"

#include "mailview.h"
#include "signal.h"
#include "busy.h"
#include "dispfilt.h"


/* internal prototypes */
int	df_valid_test(BODY *, char *);



/*
 * dfilter - pipe the data from the given storage object thru the
 *	     global display filter and into whatever the putchar's
 *	     function points to.
 *
 *	     Input is assumed to be UTF-8.
 *	     That's converted to user's locale and passed to rawcmd.
 *	     That's converted back to UTF-8 and passed through aux_filters.
 */
char *
dfilter(char *rawcmd, STORE_S *input_so, gf_io_t output_pc, FILTLIST_S *aux_filters)
{
    char *status = NULL, *cmd, *resultf = NULL, *tmpfile = NULL;
    int   key = 0;

    if((cmd = expand_filter_tokens(rawcmd,NULL,&tmpfile,&resultf,NULL,&key,NULL)) != NULL){
	suspend_busy_cue();
#ifndef	_WINDOWS
	ps_global->mangled_screen = 1;
	ClearScreen();
	fflush(stdout);
#endif

	/*
	 * If it was requested that the interaction take place via
	 * a tmpfile, fill it with text from our input_so, and let
	 * system_pipe handle the rest.  Session key and tmp file
	 * mode support additions based loosely on a patch 
	 * supplied by Thomas Stroesslin <thomas.stroesslin@epfl.ch>
	 */
	if(tmpfile){
	    PIPE_S	  *filter_pipe;
	    FILE          *fp;
	    gf_io_t	   gc, pc;
	    STORE_S       *tmpf_so;

	    /* write the tmp file */
	    so_seek(input_so, 0L, 0);
	    if((tmpf_so = so_get(FileStar, tmpfile, WRITE_ACCESS|OWNER_ONLY|WRITE_TO_LOCALE)) != NULL){
	        if(key){
		    so_puts(tmpf_so, filter_session_key());
                    so_puts(tmpf_so, NEWLINE);
		}
	        /* copy input to tmp file */
		gf_set_so_readc(&gc, input_so);
		gf_set_so_writec(&pc, tmpf_so);
		gf_filter_init();
		status = gf_pipe(gc, pc);
		gf_clear_so_readc(input_so);
		gf_clear_so_writec(tmpf_so);
		if(so_give(&tmpf_so) != 0 && status == NULL)
		  status = error_description(errno);

		/* prepare the terminal in case the filter uses it */
		if(status == NULL){
		    if((filter_pipe = open_system_pipe(cmd, NULL, NULL,
						      PIPE_USER | PIPE_RESET,
						      0, pipe_callback, NULL)) != NULL){
			if(close_system_pipe(&filter_pipe, NULL, pipe_callback) == 0){
			    /* pull result out of tmp file */
			    if((fp = our_fopen(tmpfile, "rb")) != NULL){
				gf_set_readc(&gc, fp, 0L, FileStar, READ_FROM_LOCALE);
				gf_filter_init();
				if(aux_filters)
				  for( ; aux_filters->filter; aux_filters++)
				    gf_link_filter(aux_filters->filter,
						   aux_filters->data);

				status = gf_pipe(gc, output_pc);
				fclose(fp);
			    }
			    else
			      status = "Can't read result of display filter";
			}
			else
			  status = "Filter command returned error.";
		    }
		    else
		      status = "Can't open pipe for display filter";
		}

		our_unlink(tmpfile);
	    }
	    else
	      status = "Can't open display filter tmp file";
	}
	else if((status = gf_filter(cmd, key ? filter_session_key() : NULL,
				   input_so, output_pc, aux_filters,
				   F_ON(F_DISABLE_TERM_RESET_DISP, ps_global),
				   pipe_callback)) != NULL){
	    unsigned long ch;

	    fprintf(stdout,"\r\n%s  Hit return to continue.", status);
	    fflush(stdout);
	    while((ch = read_char(300)) != ctrl('M') && ch != NO_OP_IDLE)
	      putchar(BELL);
	}

	if(resultf){
	    if(name_file_size(resultf) > 0L)
	      display_output_file(resultf, "Filter", NULL, DOF_BRIEF);

	    fs_give((void **)&resultf);
	}

	resume_busy_cue(0);
#ifndef	_WINDOWS
	ClearScreen();
#endif
	fs_give((void **)&cmd);
    }

    return(status);
}


/*
 * expand_filter_tokens - return an alloc'd string with any special tokens
 *			  in the given filter expanded, NULL otherwise.
 */
char *
expand_filter_tokens(char *filter, ENVELOPE *env, char **tmpf, char **resultf,
		     char **mtypef, int *key, int *hdrs)
{
    char   **array, **q;
    char    *bp, *cmd = NULL, *p = NULL,
	    *tfn = NULL, *rfn = NULL, *dfn = NULL, *mfn = NULL,
	    *freeme_tfn = NULL, *freeme_rfn = NULL, *freeme_mfn = NULL;
    int      n = 0;
    size_t   len;

    /*
     * break filter into words delimited by whitespace so that we can
     * look for tokens. First we count how many words.
     */
    if((bp = cpystr(filter)) != NULL)
      p = strtok(bp, " \t");
    
    if(p){
	n++;
	while(strtok(NULL, " \t") != NULL)
	  n++;
    }

    if(!n){
	dprint((1, "Unexpected failure creating sending_filter\n"));
	if(bp)
	  fs_give((void **)&bp);

	return(cmd);
    }

    q = array = (char **) fs_get((n+1) * sizeof(*array));
    memset(array, 0, (n+1) * sizeof(*array));
    /* restore bp and form the array */
    strncpy(bp, filter, strlen(filter)+1);
    if((p = strtok(bp, " \t")) != NULL){
	if(q-array < n+1)
	  *q++ = cpystr(p);

	while((p = strtok(NULL, " \t")) != NULL && (q-array < n+1))
	  *q++ = cpystr(p);
    }

    if(bp)
      fs_give((void **)&bp);

    for(q = array; *q != NULL; q++){
	if(!strcmp(*q, "_RECIPIENTS_")){
	    char *rl = NULL;

	    if(env){
		size_t l;

		char *to_l = addr_list_string(env->to,
					      simple_addr_string, 0),
		     *cc_l = addr_list_string(env->cc,
					      simple_addr_string, 0),
		     *bcc_l = addr_list_string(env->bcc,
					       simple_addr_string, 0);

		l = strlen(to_l) + strlen(cc_l) + strlen(bcc_l) + 2;
		rl = (char *) fs_get((l+1) * sizeof(char));
		snprintf(rl, l+1, "%s %s %s", to_l, cc_l, bcc_l);
		fs_give((void **)&to_l);
		fs_give((void **)&cc_l);
		fs_give((void **)&bcc_l);
		for(to_l = rl; *to_l; to_l++)	/* to_l overloaded! */
		  if(*to_l == ',')			/* space delim'd list */
		    *to_l = ' ';
	    }

	    fs_give((void **)q);
	    *q = rl ? rl : cpystr("");
	}
	else if(!strcmp(*q, "_TMPFILE_")){
	    if(!tfn){
		tfn = temp_nam(NULL, "sf"); 	/* send filter file */
		if(!tfn)
		  dprint((1, "FAILED creat of _TMPFILE_\n"));
	    }

	    if(tmpf)
	      *tmpf = tfn;
	    else
	      freeme_tfn = tfn;

	    fs_give((void **)q);
	    *q = cpystr(tfn ? tfn : "");
	}
	else if(!strcmp(*q, "_RESULTFILE_")){
	    if(!rfn){
		rfn = temp_nam(NULL, "rf");
		/*
		 * We don't create the result file, the user does.
		 * That means we have to remove it after temp_nam creates it.
		 */
		if(rfn)
		  our_unlink(rfn);
		else
		  dprint((1, "FAILED creat of _RESULTFILE_\n"));
	    }

	    if(resultf)
	      *resultf = rfn;
	    else
	      freeme_rfn = rfn;

	    fs_give((void **)q);
	    *q = cpystr(rfn ? rfn : "");
	}
	else if(!strcmp(*q, "_MIMETYPE_")){
	    if(!mfn){
		mfn = temp_nam(NULL, "mt");
		/*
		 * We don't create the mimetype file, the user does.
		 * That means we have to remove it after temp_nam creates it.
		 */
		if(mfn)
		  our_unlink(mfn);
		else
		  dprint((1, "FAILED creat of _MIMETYPE_\n"));
	    }

	    if(mtypef)
	      *mtypef = mfn;
	    else
	      freeme_mfn = mfn;

	    fs_give((void **)q);
	    *q = cpystr(mfn ? mfn : "");
	}
	else if(!strcmp(*q, "_DATAFILE_")){
	    if((dfn = filter_data_file(1)) == NULL)	/* filter data file */
	      dprint((1, "FAILED creat of _DATAFILE_\n"));

	    fs_give((void **)q);
	    *q = cpystr(dfn ? dfn : "");
	}
	else if(!strcmp(*q, "_PREPENDKEY_")){
	    (*q)[0] = '\0';
	    if(key)
	      *key = 1;
	}
	else if(!strcmp(*q, "_INCLUDEALLHDRS_")){
	    (*q)[0] = '\0';
	    if(hdrs)
	      *hdrs = 1;
	}
    }

    /* count up required length */
    for(len = 0, q = array; *q != NULL; q++)
      len += (strlen(*q)+1);
    
    cmd = fs_get((len+1) * sizeof(char));
    cmd[len] = '\0';

    /* cat together all the args */
    p = cmd;
    for(q = array; *q != NULL; q++){
	sstrncpy(&p, *q, len+1-(p-cmd));
	sstrncpy(&p, " ", len+1-(p-cmd));
    }

    cmd[len] = '\0';

    if(freeme_rfn)
      fs_give((void **) &freeme_rfn);

    if(freeme_tfn){			/* this shouldn't happen */
	our_unlink(freeme_tfn);
	fs_give((void **) &freeme_tfn);
    }

    if(freeme_mfn)
      fs_give((void **) &freeme_mfn);
    
    free_list_array(&array);

    return(cmd);
}


/*
 * filter_session_key - function to return randomly generated number
 *			representing a key for this session.  The idea is
 *			the display/sending filter could use it to muddle
 *			up any pass phrase or such stored in the
 *			"_DATAFILE_".
 */
char *
filter_session_key(void)
{
    static char *key = NULL;

    if(!key){
	snprintf(tmp_20k_buf, SIZEOF_20KBUF, "%ld", random());
	key = cpystr(tmp_20k_buf);
    }
    
    return(key);
}




/*
 * filter_data_file - function to return filename of scratch file for
 *		      display and sending filters.  This file is created
 *		      the first time it's needed, and persists until pine
 *		      exits.
 */
char *
filter_data_file(int create_it)
{
    static char *fn = NULL;

    if(!fn && create_it)
      fn = temp_nam(NULL, "df");
    
    return(fn);
}


/*
 * df_static_trigger - look thru the display filter list and set the
 *		       filter to any triggers that don't require scanning
 *		       the message segment.
 */
char *
dfilter_trigger(struct mail_bodystruct *body, char *cmdbuf, size_t cmdbuflen)
{
    int    passed = 0;
    char **l, *test, *cmd;

    for(l = ps_global->VAR_DISPLAY_FILTERS ; l && *l && !passed; l++){

	get_pair(*l, &test, &cmd, 1, 1);

	dprint((5, "static trigger: \"%s\" --> \"%s\" and \"%s\"",
		   (l && *l) ? *l : "?",
		   test ? test : "<NULL>", cmd ? cmd : "<NULL>"));

	if((passed = (df_valid_test(body, test) && valid_filter_command(&cmd))) != 0){
	  strncpy(cmdbuf, cmd, cmdbuflen);
	  cmdbuf[cmdbuflen-1] = '\0';
	}

	fs_give((void **) &test);
	fs_give((void **) &cmd);
    }

    return(passed ? cmdbuf : NULL);
}



int
df_valid_test(struct mail_bodystruct *body, char *test)
{
    int passed = 0;

    if(!(passed = !test)){			/* NO test always wins */
	if(!*test){
	    passed++;				/* NULL test also wins! */
	}
	else if(body && !struncmp(test, "_CHARSET(", 9)){
	    char *p = strrindex(test, ')');

	    if(p){
		*p = '\0';			/* tie off user charset */
		if((p = parameter_val(body->parameter,"charset")) != NULL){
		    passed = !strucmp(test + 9, p);
		    fs_give((void **) &p);
		}
		else
		  passed = !strucmp(test + 9, "us-ascii");
	    }
	    else
	      dprint((1,
			 "filter trigger: malformed test: %s\n",
			 test ? test : "?"));
	}
    }

    return(passed);
}
