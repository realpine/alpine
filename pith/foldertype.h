/*
 * $Id: foldertype.h 768 2007-10-24 00:10:03Z hubert@u.washington.edu $
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

#ifndef PITH_FOLDERTYPE_INCLUDED
#define PITH_FOLDERTYPE_INCLUDED


#include "../pith/conftype.h"
#include "../pith/string.h"


/*
 * Type definitions for folder and context structures.
 */


/*------------------------------
   Used for displaying as well as
   keeping track of folders. 
  ----*/
typedef struct folder {
    unsigned char   name_len;			/* name length		      */
    unsigned	    isfolder:1;			/* is it a folder?	      */
    unsigned	    isdir:1;			/* is it a directory?	      */
		    /* isdual is only set if the user has the separate
		       directory and folder display option set. Otherwise,
		       we can tell it's dual use because both isfolder
		       and isdir will be set. */
    unsigned	    isdual:1;			/* dual use                   */
    unsigned        haschildren:1;              /* dir known to have children */
    unsigned        hasnochildren:1;            /* known not to have children */
    unsigned	    scanned:1;			/* scanned by c-client	      */
    unsigned	    selected:1;			/* selected by user	      */
    unsigned	    subscribed:1;		/* selected by user	      */
    unsigned	    unseen_valid:1;		/* unseen count is valid      */
    unsigned long   varhash;			/* hash of var for incoming   */
    imapuid_t       uidvalidity;		/* only for #move folder      */
    imapuid_t       uidnext;			/* only for #move folder      */
    char	   *nickname;			/* folder's short name        */
    unsigned long   unseen;			/* for monitoring unseen      */
    unsigned long   new;			/* for monitoring unseen      */
    unsigned long   total;			/* for monitoring unseen      */
    time_t          last_unseen_update;		/* see LUU_ constants below   */
    char	    name[1];			/* folder's name              */
} FOLDER_S;


/* special values stored in last_unseen_update */
#define LUU_INIT	((time_t) 0)	/* before first check is done */
#define LUU_NEVERCHK	((time_t) 1)	/* don't check this folder */
#define LUU_NOMORECHK	((time_t) 2)	/* check failed so stop checking */

/* this value is eligible for checking */
#define LUU_YES(luu)	(((luu) != LUU_NEVERCHK) && ((luu) != LUU_NOMORECHK))


/*
 * Folder List Structure - provides for two ways to access and manage
 *                         folder list data.  One as an array of pointers
 *                         to folder structs or
 */
typedef struct folder_list {
    unsigned   used;
    unsigned   allocated;
    FOLDER_S **folders;		/* array of pointers to folder structs */
} FLIST;


/*
 * digested form of context including pointer to the parent
 * level of hierarchy...
 */
typedef struct folder_dir {
    char	   *ref,			/* collection location     */
		   *desc,			/* Optional description	   */
		    delim,			/* dir/file delimiter      */
		    status;			/* folder data's status    */
    struct {
	char	   *user,
		   *internal;
    } view;					/* file's within dir	   */

    FLIST          *folders;			/* folder data             */
    struct folder_dir *prev;			/* parent directory	   */
} FDIR_S;


typedef struct selected_s {
    char	*reference;	                /* location of selected	   */
    STRLIST_S   *folders;			/* list of selected	   */
    unsigned     zoomed:1;			/* zoomed state		   */
    struct selected_s  *sub;
} SELECTED_S;


/*------------------------------
    Stucture to keep track of the various folder collections being
    dealt with.
  ----*/
typedef struct context {
    FDIR_S	    *dir;			/* directory stack	   */
    char	    *context,			/* raw context string	   */
		    *server,			/* server name/parms	   */
		    *nickname,			/* user provided nickname  */
		    *label,			/* Description		   */
		    *comment,			/* Optional comment	   */
		     last_folder[MAXFOLDER+1];	/* last folder used        */
    struct {
	struct variable *v;			/* variable where defined  */
	short		 i;			/* index into config list  */
    } var;

    unsigned short   use,			/* use flags (see below)   */
		     d_line;			/* display line for labels */
    SELECTED_S       selected;
    struct context  *next,			/* next context struct	   */
		    *prev;			/* previous context struct */
} CONTEXT_S;

/*
 * Flags to indicate context (i.e., folder collection) use
 */
#define	CNTXT_PSEUDO	0x0001			/* fake folder entry exists  */
#define	CNTXT_INCMNG	0x0002			/* inbox collection	     */
#define	CNTXT_SAVEDFLT	0x0004			/* default save collection   */
#define	CNTXT_PARTFIND	0x0008			/* partial find done         */
#define	CNTXT_NOFIND	0x0010			/* no find done in context   */
#define	CNTXT_FINDALL   0x0020			/* Do a find_all on context  */
#define	CNTXT_NEWS	0x0040			/* News namespace collection */
#define	CNTXT_SUBDIR	0x0080			/* subdirectory within col'n */
#define	CNTXT_PRESRV	0x0100			/* preserve/restore selected */
#define	CNTXT_ZOOM	0x0200			/* context display narrowed  */
#define	CNTXT_INHERIT	0x1000


/*
 * Macros to help users of above two structures...
 */
#define	NEWS_TEST(c)	((c) && ((c)->use & CNTXT_NEWS))
			 
#define	FOLDERS(c)	((c)->dir->folders)
#define	FLDR_NAME(X)	((X) ? ((X)->nickname ? (X)->nickname : (X)->name) :"")
#define	ALL_FOUND(X)	(((X)->dir->status & CNTXT_NOFIND) == 0 && \
			  ((X)->dir->status & CNTXT_PARTFIND) == 0)


/* exported protoypes */


#endif /* PITH_FOLDERTYPE_INCLUDED */
