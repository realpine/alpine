/*
 * $Id: smime.h 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $
 *
 * ========================================================================
 * Copyright 2008 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */

#ifdef SMIME
#ifndef PITH_SMIME_INCLUDED
#define PITH_SMIME_INCLUDED


#include "../pith/state.h"
#include "../pith/send.h"
#include "../pith/filttype.h"
#include "../pith/smkeys.h"

#include <openssl/rand.h>
#include <openssl/err.h>


#define OUR_PKCS7_ENCLOSURE_SUBTYPE "x-pkcs7-enclosure"


/* exported protoypes */
int            is_pkcs7_body(BODY *b);
int            fiddle_smime_message(BODY *b, long msgno);
int            encrypt_outgoing_message(METAENV *header, BODY **bodyP);
void           free_smime_body_sparep(void **sparep);
int            sign_outgoing_message(METAENV *header, BODY **bodyP, int dont_detach);
void           gf_puts_uline(char *txt, gf_io_t pc);
PERSONAL_CERT *find_certificate_matching_recip_info(PKCS7_RECIP_INFO *ri);
void           smime_deinit(void);
SMIME_STUFF_S *new_smime_struct(void);
int            copy_publiccert_dir_to_container(void);
int            copy_publiccert_container_to_dir(void);
int            copy_privatecert_dir_to_container(void);
int            copy_privatecert_container_to_dir(void);
int            copy_cacert_dir_to_container(void);
int            copy_cacert_container_to_dir(void);
#ifdef APPLEKEYCHAIN
int            copy_publiccert_container_to_keychain(void);
int            copy_publiccert_keychain_to_container(void);
#endif /* APPLEKEYCHAIN */


#endif /* PITH_SMIME_INCLUDED */
#endif /* SMIME */
