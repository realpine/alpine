/*-----------------------------------------------------------------------
 $Id: imap.h 82 2006-07-12 23:36:59Z mikes@u.washington.edu $
  -----------------------------------------------------------------------*/

#ifndef _WEB_ALPINE_IMAP_INCLUDED
#define _WEB_ALPINE_IMAP_INCLUDED


#include "../../../pith/imap.h"


/* exported protoypes */
long    alpine_tcptimeout(long, long);
long    alpine_sslcertquery(char *, char *, char *);
void    alpine_sslfailure(char *, char *, unsigned long);
void	alpine_set_passwd(char *, char *, char *, int);
void	alpine_clear_passwd(char *, char *);
int	alpine_have_passwd(char *, char *, int);
char   *alpine_get_user(char *, int);



#endif /* _WEB_ALPINE_IMAP_INCLUDED */
