#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: status.c 761 2007-10-23 22:35:18Z hubert@u.washington.edu $";
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

#include "../pith/headers.h"
#include "../pith/status.h"
#include "../pith/state.h"


/*----------------------------------------------------------------------
        Put a message with 1 printf argument on queue for status line
 
    Args: min_t -- minimum time to display message for
          max_t -- minimum time to display message for
          s -- printf style control string
          a -- argument for printf
 
   Result: message queued
  ----*/

/*VARARGS1*/
void
q_status_message1(int flags, int min_t, int max_t, char *s, void *a)
{
    snprintf(tmp_20k_buf, SIZEOF_20KBUF, s, a);
    q_status_message(flags, min_t, max_t, tmp_20k_buf);
}



/*----------------------------------------------------------------------
        Put a message with 2 printf argument on queue for status line

    Args: min_t  -- minimum time to display message for
          max_t  -- maximum time to display message for
          s  -- printf style control string
          a1 -- argument for printf
          a2 -- argument for printf

  Result: message queued
  ---*/

/*VARARGS1*/
void
q_status_message2(int flags, int min_t, int max_t, char *s, void *a1, void *a2)
{
    snprintf(tmp_20k_buf, SIZEOF_20KBUF, s, a1, a2);
    q_status_message(flags, min_t, max_t, tmp_20k_buf);
}



/*----------------------------------------------------------------------
        Put a message with 3 printf argument on queue for status line

    Args: min_t  -- minimum time to display message for
          max_t  -- maximum time to display message for
          s  -- printf style control string
          a1 -- argument for printf
          a2 -- argument for printf
          a3 -- argument for printf

  Result: message queued
  ---*/

/*VARARGS1*/
void
q_status_message3(int flags, int min_t, int max_t, char *s, void *a1, void *a2, void *a3)
{
    snprintf(tmp_20k_buf, SIZEOF_20KBUF, s, a1, a2, a3);
    q_status_message(flags, min_t, max_t, tmp_20k_buf);
}



/*----------------------------------------------------------------------
        Put a message with 4 printf argument on queue for status line


    Args: min_t  -- minimum time to display message for
          max_t  -- maximum time to display message for
          s  -- printf style control string
          a1 -- argument for printf
          a2 -- argument for printf
          a3 -- argument for printf
          a4 -- argument for printf

  Result: message queued
  ----------------------------------------------------------------------*/
/*VARARGS1*/
void
q_status_message4(int flags, int min_t, int max_t, char *s, void *a1, void *a2, void *a3, void *a4)
{
    snprintf(tmp_20k_buf, SIZEOF_20KBUF, s, a1, a2, a3, a4);
    q_status_message(flags, min_t, max_t, tmp_20k_buf);
}


/*VARARGS1*/
void
q_status_message5(int flags, int min_t, int max_t, char *s, void *a1, void *a2, void *a3, void *a4, void *a5)
{
    snprintf(tmp_20k_buf, SIZEOF_20KBUF, s, a1, a2, a3, a4, a5);
    q_status_message(flags, min_t, max_t, tmp_20k_buf);
}


/*VARARGS1*/
void
q_status_message6(int flags, int min_t, int max_t, char *s, void *a1, void *a2, void *a3, void *a4, void *a5, void *a6)
{
    snprintf(tmp_20k_buf, SIZEOF_20KBUF, s, a1, a2, a3, a4, a5, a6);
    q_status_message(flags, min_t, max_t, tmp_20k_buf);
}


/*----------------------------------------------------------------------
        Put a message with 7 printf argument on queue for status line


    Args: min_t  -- minimum time to display message for
          max_t  -- maximum time to display message for
          s  -- printf style control string
          a1 -- argument for printf
          a2 -- argument for printf
          a3 -- argument for printf
          a4 -- argument for printf
          a5 -- argument for printf
          a6 -- argument for printf
          a7 -- argument for printf


  Result: message queued
  ----------------------------------------------------------------------*/
/*VARARGS1*/
void
q_status_message7(int flags, int min_t, int max_t, char *s, void *a1, void *a2, void *a3, void *a4, void *a5, void *a6, void *a7)
{
    snprintf(tmp_20k_buf, SIZEOF_20KBUF, s, a1, a2, a3, a4, a5, a6, a7);
    q_status_message(flags, min_t, max_t, tmp_20k_buf);
}


/*VARARGS1*/
void
q_status_message8(int flags, int min_t, int max_t, char *s, void *a1, void *a2, void *a3, void *a4, void *a5, void *a6, void *a7, void *a8)
{
    snprintf(tmp_20k_buf, SIZEOF_20KBUF, s, a1, a2, a3, a4, a5, a6, a7, a8);
    q_status_message(flags, min_t, max_t, tmp_20k_buf);
}
