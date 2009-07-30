/*
 * $Id: addrstring.h 770 2007-10-24 00:23:09Z hubert@u.washington.edu $
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

#ifndef PITH_ADDRSTRING_INCLUDED
#define PITH_ADDRSTRING_INCLUDED


#define RAWFIELD "-RAW-FIELD-"


/* exported protoypes */
char	     *addr_string(ADDRESS *, char *, size_t);
char	     *addr_string_mult(ADDRESS *, char *, size_t);
char	     *simple_addr_string(ADDRESS *, char *, size_t);
char         *simple_mult_addr_string(ADDRESS *, char *, size_t, char *);
char         *encode_fullname_of_addrstring(char *, char *);
char         *decode_fullname_of_addrstring(char *, int);
char         *addr_list_string(ADDRESS *, char *(*)(ADDRESS *, char *, size_t), int);
int           est_size(ADDRESS *);
int           count_addrs(ADDRESS *);
void          a_little_addr_string(ADDRESS *, char *, size_t);


#endif /* PITH_ADDRSTRING_INCLUDED */
