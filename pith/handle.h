/*
 * $Id: handle.h 814 2007-11-14 18:39:28Z hubert@u.washington.edu $
 *
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

#ifndef PITH_HANDLE_INCLUDED
#define PITH_HANDLE_INCLUDED


#include "../pith/context.h"
#include "../pith/msgno.h"
#include "../pith/atttype.h"
#include "../pith/util.h"


typedef struct screen_position_list {
    Pos				  where;
    struct screen_position_list  *next;
} POSLIST_S;


/*
 * Struct to help manage embedded urls (and anythin' else we might embed)
 */
typedef	struct handle_s {
    int		     key;		/* tag number embedded in text */
    enum	     {URL, Attach, Folder, Function, IMG} type;
    unsigned	     force_display:1;	/* Don't ask before launching */
    unsigned	     using_is_used:1;	/* bit below is being used     */
    unsigned	     is_used:1;		/* if not, remove it from list */
    unsigned	     color_unseen:1;	/* we're coloring folders with unseen */
    unsigned	     is_dual_do_open:1;	/* choosing this handle means open */
    union {
	struct {			/* URL corresponding to this handle */
	    char *path,			/* Actual url string */
		 *tool,			/* displaying application */
		 *name;			/* URL's NAME attribute */
	} url;				/* stuff to describe URL handle */
	struct {
	    char *src,			/* src of image (CID: only?) */
		 *alt;			/* image alternate text */
	} img;				/* stuff to describe img */
	ATTACH_S    *attach;		/* Attachment struct for this handle */
	struct {
	    int	       index;		/* folder's place in context's list */
	    CONTEXT_S *context;		/* description of folders */
	} f;				/* stuff to describe Folder handle */
	struct {
	    struct {			/* function and args to pass it */
		MAILSTREAM *stream;
		MSGNO_S	   *msgmap;
		long	    msgno;
	    } args;
	    void	(*f)(MAILSTREAM *, MSGNO_S *, long);
	} func;
    } h;
    POSLIST_S	    *loc;		/* list of places it exists in text */
    struct handle_s *next, *prev;	/* next and previous in the list */
} HANDLE_S ;



/*
 * Function used to dispatch locally handled URL's
 */
typedef	int	(*url_tool_t)(char *);


/* exported protoypes */
HANDLE_S   *get_handle(HANDLE_S *, int);
void	    init_handles(HANDLE_S **);
HANDLE_S   *new_handle(HANDLE_S **);
void        delete_unused_handles(HANDLE_S **);
void	    free_handle(HANDLE_S **);
void	    free_handles(HANDLE_S **);
void	    free_handle_locations(POSLIST_S **);


#endif /* PITH_HANDLE_INCLUDED */
