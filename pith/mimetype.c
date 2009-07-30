#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: mimetype.c 955 2008-03-06 23:52:36Z hubert@u.washington.edu $";
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
#include "../pith/mimetype.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/mailcap.h"
#include "../pith/util.h"

/*
 * We've decided not to implement the RFC1524 standard minimum path, because
 * some of us think it is harder to debug a problem when you may be misled
 * into looking at the wrong mailcap entry.  Likewise for MIME.Types files.
 */
#if defined(DOS) || defined(OS2)
#define MT_PATH_SEPARATOR ';'
#define	MT_USER_FILE	  "MIMETYPE"
#define MT_STDPATH        NULL
#else /* !DOS */
#define MT_PATH_SEPARATOR ':'
#define	MT_USER_FILE	  NULL
#define MT_STDPATH         \
		".mime.types:/etc/mime.types:/usr/local/lib/mime.types"
#endif /* !DOS */

#define LINE_BUF_SIZE      2000


/*
 * Types used to pass parameters and operator functions to the
 * mime.types searching routines.
 */
#define MT_MAX_FILE_EXTENSION 3


/*
 * Internal prototypes
 */
int  mt_browse_types_file(MT_OPERATORPROC, MT_MAP_T *, char *);
int  mt_srch_by_type(MT_MAP_T *, FILE *);



/*
 * Exported function that does the work of sniffing the mime.types
 * files and filling in the body pointer if found.  Returns 1 (TRUE) if
 * extension found, and body pointer filled in, 0 (FALSE) otherwise.
 */
int
set_mime_type_by_extension(struct mail_bodystruct *body, char *filename)
{
    MT_MAP_T  e2b;

    if(mt_get_file_ext(filename, &e2b.from.ext)
       && mt_srch_mime_type(mt_srch_by_ext, &e2b)){
	body->type    = e2b.to.mime.type;
	body->subtype = e2b.to.mime.subtype; /* NOTE: subtype was malloc'd */
	return(1);
    }

    return(0);
}


/*
 * Exported function that maps from mime types to file extensions.
 */
int
set_mime_extension_by_type (char *ext, char *mtype)
{
    MT_MAP_T t2e;

    t2e.from.mime_type = mtype;
    t2e.to.ext	       = ext;
    return (mt_srch_mime_type (mt_srch_by_type, &t2e));
}
    



/* 
 * Separate and return a pointer to the first character in the 'filename'
 * character buffer that comes after the rightmost '.' character in the
 * filename. (What I mean is a pointer to the filename - extension).
 *
 * Returns 1 if an extension is found, 0 otherwise.
 */
int
mt_get_file_ext(char *filename, char **extension)
{
    dprint((5, "mt_get_file_ext : filename=\"%s\", ",
	   filename ? filename : "?"));

    for(*extension = NULL; filename && *filename; filename++)
      if(*filename == '.')
	*extension = filename + 1;

    dprint((5, "extension=\"%s\"\n",
	   (extension && *extension) ? *extension : "?"));

    return(*extension ? 1 : 0);
}


/*
 * Build a list of possible mime.type files.  For each one that exists
 * call the mt_operator function.
 * Loop terminates when mt_operator returns non-zero.  
 */
int
mt_srch_mime_type(MT_OPERATORPROC mt_operator, MT_MAP_T *mt_map)
{
    char *s, *pathcopy, *path;
    int   rv = 0;
  
    dprint((5, "- mt_srch_mime_type -\n"));

    pathcopy = mc_conf_path(ps_global->VAR_MIMETYPE_PATH, getenv("MIMETYPES"),
			    MT_USER_FILE, MT_PATH_SEPARATOR, MT_STDPATH);

    path = pathcopy;			/* overloaded "path" */

    dprint((7, "mime_types: path: %s\n", path ? path : "?"));
    while(path){
	if((s = strindex(path, MT_PATH_SEPARATOR)) != NULL)
	  *s++ = '\0';

	if((rv = mt_browse_types_file(mt_operator, mt_map, path)) != 0)
	  break;

	path = s;
    }
  
    if(pathcopy)
      fs_give((void **)&pathcopy);

    if(!rv && mime_os_specific_access()){
	if(mt_operator == mt_srch_by_ext){
	    char buf[256];

	    buf[0] = '\0';
	    if(mime_get_os_mimetype_from_ext(mt_map->from.ext, buf, 256)){
		if((s = strindex(buf, '/')) != NULL){
		    *s++ = '\0';
		    mt_map->to.mime.type = mt_translate_type(buf);
		    mt_map->to.mime.subtype = cpystr(s);
		    rv = 1;
		}
	    }
	}
	else if(mt_operator == mt_srch_by_type){
	    if(mime_get_os_ext_from_mimetype(mt_map->from.mime_type,
				  mt_map->to.ext, 32)){
		/* the 32 comes from var ext[] in display_attachment() */
		if(*(s = mt_map->to.ext) == '.')
		  while((*s = *(s+1)) != '\0')
		    s++;

		rv = 1;
	    }
	}
	else
	  panic("Unhandled mime type search");
    }


    return(rv);
}


/*
 * Try to match a file extension against extensions found in the file
 * ``filename'' if that file exists. return 1 if a match
 * was found and 0 in all other cases.
 */
int
mt_browse_types_file(MT_OPERATORPROC mt_operator, MT_MAP_T *mt_map, char *filename)
{
    int   rv = 0;
    FILE *file;

    dprint((7, "mt_browse_types_file(%s)\n", filename ? filename : "?"));
    if((file = our_fopen(filename, "rb")) != NULL){
	rv = (*mt_operator)(mt_map, file);
	fclose(file);
    }
    else{
      dprint((1, "mt_browse: FAILED open(%s) : %s.\n",
		 filename ? filename : "?", error_description(errno)));
    }

    return(rv);
}


/*
 * scan each line of the file. Treat each line as a mime type definition.
 * The first word is a type/subtype specification. All following words
 * are file extensions belonging to that type/subtype. Words are separated
 * bij whitespace characters.
 * If a file extension occurs more than once, then the first definition
 * determines the file type and subtype.
 */
int
mt_srch_by_ext(MT_MAP_T *e2b, FILE *file)
{
    char buffer[LINE_BUF_SIZE];

    /* construct a loop reading the file line by line. Then check each
     * line for a matching definition.
     */
    while(fgets(buffer,LINE_BUF_SIZE,file) != NULL){
	char *typespec;
	char *try_extension;

	if(buffer[0] == '#')
	  continue;		/* comment */

	/* divide the input buffer into words separated by whitespace.
	 * The first words is the type and subtype. All following words
	 * are file extensions.
	 */
	dprint((5, "traverse: buffer=\"%s\"\n", buffer));
	typespec = strtok(buffer," \t");	/* extract type,subtype  */
	if(!typespec)
	  continue;

	dprint((5, "typespec=\"%s\"\n", typespec ? typespec : "?"));
	while((try_extension = strtok(NULL, " \t\n\r")) != NULL){
	    /* compare the extensions, and assign the type if a match
	     * is found.
	     */
	    dprint((5,"traverse: trying ext \"%s\"\n",try_extension));
	    if(strucmp(try_extension, e2b->from.ext) == 0){
		/* split the 'type/subtype' specification */
		char *type, *subtype = NULL;

		type = strtok(typespec,"/");
		if(type)
		  subtype = strtok(NULL,"/");

		dprint((5, "traverse: type=%s, subtype=%s.\n",
			   type ? type : "<null>",
			   subtype ? subtype : "<null>"));
		/* The type is encoded as a small integer. we have to
		 * translate the character string naming the type into
		 * the corresponding number.
		 */
		e2b->to.mime.type    = mt_translate_type(type);
		e2b->to.mime.subtype = cpystr(subtype ? subtype : "x-unknown");
		return 1; /* a match has been found */
	    }
	}
    }

    dprint((5, "traverse: search failed.\n"));
    return 0;
}


/*
 * scan each line of the file. Treat each line as a mime type definition.
 * Here we are looking for a matching type.  When that is found return the
 * first extension that is three chars or less.
 */
int
mt_srch_by_type(MT_MAP_T *t2e, FILE *file)
{
    char buffer[LINE_BUF_SIZE];

    /* construct a loop reading the file line by line. Then check each
     * line for a matching definition.
     */
    while(fgets(buffer,LINE_BUF_SIZE,file) != NULL){
	char *typespec;
	char *try_extension;

	if(buffer[0] == '#')
	  continue;		/* comment */

	/* divide the input buffer into words separated by whitespace.
	 * The first words is the type and subtype. All following words
	 * are file extensions.
	 */
	dprint((5, "traverse: buffer=%s.\n", buffer));
	typespec = strtok(buffer," \t");	/* extract type,subtype  */
	dprint((5, "typespec=%s.\n", typespec ? typespec : "?"));
	if (strucmp (typespec, t2e->from.mime_type) == 0) {
	    while((try_extension = strtok(NULL, " \t\n\r")) != NULL){
		if (strlen (try_extension) <= MT_MAX_FILE_EXTENSION) {
		    strncpy (t2e->to.ext, try_extension, 32);
		    /*
		     * not sure of the 32, so don't write to byte 32
		     * on purpose with
		     *     t2e->to.ext[31] = '\0';
		     * in case that breaks something
		     */

		    return (1);
	        }
	    }
        }
    }

    dprint((5, "traverse: search failed.\n"));
    return 0;
}


/*
 * Translate a character string representing a content type into a short 
 * integer number, according to the coding described in c-client/mail.h
 * List of content types taken from rfc1521, September 1993.
 */
int
mt_translate_type(char *type)
{
    int i;

    for (i=0;(i<=TYPEMAX) && body_types[i] && strucmp(type,body_types[i]);i++)
      ;

    if (i > TYPEMAX)
      i = TYPEOTHER;
    else if (!body_types[i])	/* if empty slot, assign it to this type */
      body_types[i] = cpystr (type);

    return(i);
}
