/*
 * $Id: utf8stub.h 764 2007-10-23 23:44:49Z hubert@u.washington.edu $
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
 *
 */

#ifndef	UTF8STUB_H
#define	UTF8STUB_H

void *fs_get(size_t size);
void  fs_resize(void **block,size_t size);
void  fs_give(void **block);
void  fatal(char *s);
int   compare_ulong(unsigned long l1, unsigned long l2);
int   compare_cstring(unsigned char *s1, unsigned char *s2);
int   panicking(void);

#endif	/* UTF8STUB_H */
