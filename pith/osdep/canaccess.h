/*
 * $Id: canaccess.h 761 2007-10-23 22:35:18Z hubert@u.washington.edu $
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

#ifndef PITH_OSDEP_CANACCESS_INCLUDED
#define PITH_OSDEP_CANACCESS_INCLUDED


#define EXECUTE_ACCESS	0x01		/* These five are 	   */
#define WRITE_ACCESS	0x02		/*    for the calls	   */
#define READ_ACCESS	0x04		/*       to access()       */
#define ACCESS_EXISTS	0x00		/*           <etc>         */
#define EDIT_ACCESS	(WRITE_ACCESS + READ_ACCESS)

/*
 * These flags are used in calls to so_get.
 * They're OR'd together with the ACCESS flags above but
 * are otherwise unrelated.
 */ 
#define OWNER_ONLY	0x08		/* open with mode 0600     */
/*
 * If the storage object being written to needs to be converted to
 * the character set of the user's locale, then use this flag. For
 * example, when exporting to a file. Do not use this if the data
 * will eventually go to the display because the data is expected
 * to remain as UTF-8 until it gets to Writechar where it will be
 * converted.
 */
#define WRITE_TO_LOCALE	0x10		/* convert to locale-specific charset */
#define READ_FROM_LOCALE 0x20		/* convert from locale-specific charset */


/*
 * Exported Prototypes
 */
int	can_access(char *, int);
int	can_access_in_path(char *, char *, int);


#endif /* PITH_OSDEP_CANACCESS_INCLUDED */
