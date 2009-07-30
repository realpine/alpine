/*
 * $Id: bldaddr.h 769 2007-10-24 00:15:40Z hubert@u.washington.edu $
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

#ifndef PITH_BLDADDR_INCLUDED
#define PITH_BLDADDR_INCLUDED


#include "../pith/adrbklib.h"
#include "../pith/state.h"
#include "../pith/ldap.h"
#include "../pith/pattern.h"
#include "../pith/remtype.h"


/*
 * This is the structure that the builders impose on the private data
 * that is pointed to by the bldr_private pointer in each header entry.
 *
 * The bldr_private pointer points to a PrivateTop which consists of two
 * parts, whose purposes are independent:
 *    ******* This part no longer exists ******
 *   encoded -- This is used to preserve the charset information for
 *                addresses in this entry. For example, if the user types
 *                in a nickname which has a charset in it, the encoded
 *                version containing the charset information is saved here
 *                along with a checksum of the text that created it (the
 *                line containing the nickname).
 *         etext -- Pointer to the encoded text.
 *     chksumlen -- Length of string that produced the etext.
 *     chksumval -- Checksum of that string.
 *         The string that produced the etext is just the displayed
 *         value of the entry, not the nickname before it was expanded.
 *         That's so we can check on the next builder call to see if it
 *         was changed or not. Appending or prepending more addresses to
 *         what's there will work, the etext from the old contents will
 *         be combined with the etext from the appended stuff. (The check
 *         is for appending or prepending so appending AND prepending all
 *         at once won't work, charset will be lost.) If the middle of the
 *         text is edited the charset is lost. If text is removed, the
 *         charset is lost.
 *
 *  affector -- This is for entries which affect the contents of other
 *                fields. For example, the Lcc field affects what goes in
 *                the To and Fcc fields, and the To field affects what goes
 *                in the Fcc field.
 *           who -- Who caused this field to be set last.
 *     chksumlen -- Length of string in the "who" field that caused the effect.
 *     chksumval -- Checksum of that string.
 *         The string that is being checksummed is the one that is displayed
 *         in the field doing the affecting. So, for the affector in the
 *         Fcc headerentry, the who might point to either To or Lcc and
 *         then the checksummed string would be either the To or Lcc displayed
 *         string. The purpose of the affector is to remember that the
 *         affected field was set from an address book entry that is no
 *         longer identifiable once it is expanded. For example, if a list
 *         is entered into the Lcc field, then the To field gets the list
 *         fullname and the fcc field gets the fcc entry for the list. If
 *         we move out of the Lcc field and back in and call the builder
 *         again, the list has been expanded and we can't tell (except for
 *         the affector) that the same list is what caused the results.
 *         Same for the To field. A nickname entered there will cause the
 *         fcc of that nickname to go in the Fcc field and the affector
 *         will cause it to stick as long as the To field is only appended to.
 *
 * It may seem a little strange that the PrivateAffector doesn't have a text
 * field. The reason is that the text is actually displayed in that field
 * and so is contained in the entry itself, unlike the PrivateEncoded
 * which has etext which is not displayed and is only used when we go to
 * send the mail.
 */

typedef enum {BP_Unset, BP_To, BP_Lcc} WhoSetUs;

typedef struct private_affector {
    WhoSetUs who;
    int      cksumlen;
    unsigned long cksumval;
} PrivateAffector;

/*
 * This used to have more than one member. We could get rid
 * of this structure now if we wanted to, and just have the
 * PrivateAffector at the top-level, but it's easier alone.
 * Who knows, we may have a need for something else to be added
 * to the structure in the future.
 */
typedef struct private_top {
    PrivateAffector *affector;
} PrivateTop;

/* save_and_restore flags */
#define	SAR_SAVE	1
#define	SAR_RESTORE	2

/* exported prototypes */
int	 our_build_address(BuildTo, char **, char **, char **, void (*)(int, SAVE_STATE_S *));
int	 build_address_internal(BuildTo, char **, char **, char **, int *, char **,
				void (*)(int, SAVE_STATE_S *), int, int *);
ADDRESS	*expand_address(BuildTo, char *, char *, int *, char **, \
			int *, char **, char **, int, int, int *);
ADDRESS	*massage_phrase_addr(char *, char *, char *);
char	*get_fcc(char *);
void	 set_last_fcc(char *);
char	*get_fcc_based_on_to(ADDRESS *);
void	 free_privatetop(PrivateTop **);
void     strip_personal_quotes(ADDRESS *);


#endif /* PITH_BLDADDR_INCLUDED */
