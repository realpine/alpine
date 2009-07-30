#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: debugtime.c 770 2007-10-24 00:23:09Z hubert@u.washington.edu $";
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
 */

#include <stdio.h>

#include <system.h>
#include "debugtime.h"

#ifdef DEBUG

/*
 * Returns a pointer to static string for a timestamp.
 *
 * If timestamp is set .subseconds are added if available.
 * If include_date is set the date is appended.
 */
char *
debug_time(int include_date, int include_subseconds)
{
    time_t          t;
    struct tm      *tm_now;
#if     HAVE_GETTIMEOFDAY
    struct timeval  tp;
    struct timezone tzp;
#else
    struct _timeb   timebuffer;
#endif
    static char     timestring[23];
    char            subsecond[8];
    char            datestr[7];

    timestring[0] = '\0';

#if	HAVE_GETTIMEOFDAY
    if(gettimeofday(&tp, &tzp) == 0){
	t = (time_t)tp.tv_sec;
	if(include_date){
	    tm_now = localtime(&t);
 	    snprintf(datestr, sizeof(datestr), " %d/%d", tm_now->tm_mon+1, tm_now->tm_mday);
	}
	else
	  datestr[0] = '\0';

	if(include_subseconds)
 	  snprintf(subsecond, sizeof(subsecond), ".%06ld", tp.tv_usec);
	else
	  subsecond[0] = '\0';

 	snprintf(timestring, sizeof(timestring), "%.8s%.7s%.6s", ctime(&t)+11, subsecond, datestr);
    }
#else /* !HAVE_GETTIMEOFDAY */
    /* Should be _WINDOWS */
    t = time((time_t *)0);
    if(include_date){
	tm_now = localtime(&t);
	snprintf(datestr, sizeof(datestr), " %d/%d", tm_now->tm_mon+1, tm_now->tm_mday);
    }
    else
      datestr[0] = '\0';

    if(include_subseconds){
	_ftime(&timebuffer);
	snprintf(subsecond, sizeof(subsecond), ".%03ld", timebuffer.millitm);
    }
    else
      subsecond[0] = '\0';

    snprintf(timestring, sizeof(timestring), "%.8s%.7s%.6s", ctime(&t)+11, subsecond, datestr);
#endif /* HAVE_GETTIMEOFDAY */

    return(timestring);
}
#endif /* DEBUG */


/*
 * Fills in the passed in structure with the current time.
 *
 * Returns 0 if ok
 *        -1 if can't do it
 */
int
get_time(TIMEVAL_S *our_time_val)
{
#if	HAVE_GETTIMEOFDAY
    struct timeval  tp;
    struct timezone tzp;

    if(gettimeofday(&tp, &tzp) == 0){
	our_time_val->sec  = tp.tv_sec;
	our_time_val->usec = tp.tv_usec;
	return 0;
    }
#else /* !HAVE_GETTIMEOFDAY */
#ifdef	_WINDOWS
    struct _timeb timebuffer;

    _ftime(&timebuffer);
    our_time_val->sec  = (long)timebuffer.time;
    our_time_val->usec = 1000L * (long)timebuffer.millitm;
    return 0;
#endif
#endif

    return -1;
}


/*
 * Returns the difference between the two values, in microseconds.
 * Value returned is first - second.
 */
long
time_diff(TIMEVAL_S *first, TIMEVAL_S *second)
{
    return(1000000L*(first->sec - second->sec) + (first->usec - second->usec));
}


