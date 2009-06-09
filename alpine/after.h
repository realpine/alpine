/*
 * $Id: after.h 137 2006-09-22 21:34:06Z mikes@u.washington.edu $
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

#ifndef PINE_AFTER_INCLUDED
#define PINE_AFTER_INCLUDED

/*
 * start_after() arguments
 */
typedef struct _after_s {
    int		      delay;		/* call "f" after "delay" 1/100's of sec */
    int		    (*f)(void *);	/* 0 if done, else repeat in ret'd 1/100's */
    void	    (*cf)(void *);	/* called when done to clean up "data" etc */
    void	     *data;		/* hook to pass args and such to "f" and "cf" */
    struct _after_s  *next;		/* next function to pause or repeat */
} AFTER_S;


extern int after_active;


/* exported prototypes */
void	 start_after(AFTER_S *);
void	 stop_after(int);
AFTER_S *new_afterstruct(void);
void     status_message_lock_init(void);
int      status_message_lock(void);
int      status_message_unlock(void);


#endif	/* PINE_AFTER_INCLUDED */
