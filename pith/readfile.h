/*
 * $Id: readfile.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
 *
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

#ifndef PITH_READFILE_INCLUDED
#define PITH_READFILE_INCLUDED


/*
 * Does the string s begin with the UTF-8 Byte Order Mark?
 */
#define BOM_UTF8(s)	((s) && (s)[0] && (s)[1] && (s)[2]	\
			 && (unsigned char)(s)[0] == 0xef	\
			 && (unsigned char)(s)[1] == 0xbb	\
			 && (unsigned char)(s)[2] == 0xbf)


/*
 * Exported Prototypes
 */
char	*read_file(char *, int);


#endif /* PITH_READFILE_INCLUDED */
