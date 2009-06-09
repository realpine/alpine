#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: after.c 138 2006-09-22 22:12:03Z mikes@u.washington.edu $";
#endif

/* ========================================================================
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

/*======================================================================
     Implement asynchronous start_after() call
 ====*/


#include <system.h>
#include <general.h> 

#include "../pith/debug.h"
#include "../pith/osdep/err_desc.h"

#include "../pico/utf8stub.h"

#include "after.h"


/* internal state */
int	 after_active;

#if	defined(HAVE_PTHREAD) && defined(HAVE_NANOSLEEP)
static pthread_t after_thread;
static pthread_mutex_t status_message_mutex;
#endif


/* internal prototypes */
#if	defined(HAVE_PTHREAD) && defined(HAVE_NANOSLEEP)
void	*do_after(void *);
#else
void	*cleanup_data;
#endif

void	 cleanup_after(void *);


/*
 * start_after - pause and/or loop calling passed function
 *	         without getting in the way of main thread
 *
 */
void
start_after(AFTER_S *a)
{
    if(a){
#if	defined(HAVE_PTHREAD) && defined(HAVE_NANOSLEEP)
	pthread_attr_t attr;
	int rc;
	size_t stack;

	if(after_active)
	  stop_after(1);

	/* Initialize and set thread detached attribute */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
#if	defined(PTHREAD_STACK_MIN)
	stack = PTHREAD_STACK_MIN + 0x10000;
	pthread_attr_setstacksize(&attr, stack);
#endif

	if((rc = pthread_create(&after_thread, &attr, do_after, (void *) a)) != 0){
	    after_active = 0;
	    dprint((1, "start_after: pthread_create failed %d (%d)", rc, errno));
	}
	else
	  after_active = 1;

	pthread_attr_destroy(&attr);
	dprint((9, "start_after() created %x: done", after_thread));
#else /* !(defined(HAVE_PTHREAD) && defined(HAVE_NANOSLEEP)) */
	/*
	 * Just call the first function
	 */
	if(!a->delay)
	  (void) (*a->f)(a->data);		/* do the thing */

	cleanup_data = (void *) a;
	after_active = 1;
#endif
    }
}


/*
 * stop_after - stop the thread 
 */
void
stop_after(int join)
{
#if	defined(HAVE_PTHREAD) && defined(HAVE_NANOSLEEP)
    int rv;

    dprint((9, "stop_after(join=%d) tid=%x", join, pthread_self()));

    if(after_active){
	if((rv = pthread_cancel(after_thread)) != 0){	/* tell thread to end */
	    dprint((1, "pthread_cancel: %d (%s)\n", rv, error_description(errno)));
	}

	if(join){
	    if((rv = pthread_join(after_thread, NULL)) != 0){ /* wait for it to end */
		dprint((1, "pthread_join: %d (%s)\n", rv, error_description(errno)));
	    }
	}
	else if((rv = pthread_detach(after_thread)) != 0){ /* mark thread for deletion */
	    dprint((1, "pthread_detach: %d (%s)\n", rv, error_description(errno)));
	}
    }

    /* not literally true uless "join" set */
    after_active = 0;

#else	/* !(defined(HAVE_PTHREAD) && defined(HAVE_NANOSLEEP)) */

    cleanup_after((void *) cleanup_data);
    cleanup_data = NULL;
    after_active = 0;

#endif
}


#if	defined(HAVE_PTHREAD) && defined(HAVE_NANOSLEEP)
/*
 * do_after - loop thru list of pause/loop functions 
 */
void *
do_after(void *data)
{
    AFTER_S	    *a;
    struct timespec  ts;
    int		     loop;
    sigset_t	     sigs;

#if defined(SIGCHLD) || defined(SIGWINCH)
    sigemptyset(&sigs);
#if defined(SIGCHLD)
    /* make sure we don't end up with SIGCHLD */
    sigaddset(&sigs, SIGCHLD);
#endif	/* SIGCHLD */
#if defined(SIGCHLD)
    /* or with SIGWINCH */
    sigaddset(&sigs, SIGWINCH);
#endif	/* SIGWINCH */
    pthread_sigmask(SIG_BLOCK, &sigs, NULL);
#endif

    /* prepare for the finish */
    pthread_cleanup_push(cleanup_after, data);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    /* and jump in */
    for(a  = (AFTER_S *) data; a != NULL && a->f != NULL; a = a->next){
	if(a->delay){
	    ts.tv_sec  = a->delay / 100;	/* seconds */
	    ts.tv_nsec = (a->delay % 100) * 10000000;

	    if(nanosleep(&ts, NULL))
	      pthread_exit(NULL);		/* interrupted */
	}

	while(1){
	    /* after waking, make sure we're still wanted */
	    pthread_testcancel();

	    loop = (*a->f)(a->data);		/* do the thing */

	    if(loop > 0){	
		ts.tv_sec  = loop / 100;
		ts.tv_nsec = (loop % 100) * 10000000;

		if(nanosleep(&ts, NULL))
		  pthread_exit(NULL);		/* interrupted */
	    }
	    else
	      break;
	}
    }

    pthread_cleanup_pop(1);
    pthread_exit(NULL);
}

#endif	/* defined(HAVE_PTHREAD) && defined(HAVE_NANOSLEEP) */


/*
 * cleanup_after - give start_after caller opportunity to clean up
 *		   their data, then free up AFTER_S list
 */
void
cleanup_after(void *data)
{
    AFTER_S *a, *an;

#if	defined(HAVE_PTHREAD) && defined(HAVE_NANOSLEEP)
    dprint((9, "cleanup_after() tid=%x", pthread_self()));
#endif

    /* free linked list of AFTER_S's */
    a = (AFTER_S *) data;
    while(a != NULL){
	an = a->next;

	if(a->cf)
	  (*a->cf)(a->data);

	free((void *) a);

	a = an;
    }
}


AFTER_S *
new_afterstruct(void)
{
    AFTER_S *a;

    if((a = (AFTER_S *)malloc(sizeof(AFTER_S))) == NULL){
	fatal("Out of memory");
    }

    memset((void *) a, 0, sizeof(AFTER_S));

    return(a);
}


void
status_message_lock_init(void)
{
#if	defined(HAVE_PTHREAD) && defined(HAVE_NANOSLEEP)
    pthread_mutex_init(&status_message_mutex, NULL);
#endif
}


int
status_message_lock(void)
{
#if	defined(HAVE_PTHREAD) && defined(HAVE_NANOSLEEP)
    return(pthread_mutex_lock(&status_message_mutex));
#else
    return(0);
#endif
}


int
status_message_unlock(void)
{
#if	defined(HAVE_PTHREAD) && defined(HAVE_NANOSLEEP)
    return(pthread_mutex_unlock(&status_message_mutex));
#else
    return(0);
#endif
}
