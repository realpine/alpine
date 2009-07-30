/*
 * $Id: smkeys.h 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $
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
#ifndef PITH_SMKEYS_INCLUDED
#define PITH_SMKEYS_INCLUDED


#include "../pith/state.h"
#include "../pith/send.h"

#include <openssl/objects.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pkcs7.h>
#include <openssl/pem.h>
#include <openssl/err.h>
#include <openssl/bio.h>


#define EMAILADDRLEADER "emailAddress="
#define CACERTSTORELEADER "cacert="


typedef struct personal_cert {
    X509    	    	 *cert;
    EVP_PKEY	    	 *key;
    char                 *name;
    char                 *keytext;
    struct personal_cert *next;
} PERSONAL_CERT;


/* exported protoypes */
X509_STORE    *get_ca_store(void);
PERSONAL_CERT *get_personal_certs(char *d);
X509          *get_cert_for(char *email);
void           save_cert_for(char *email, X509 *cert);
char          *get_x509_subject_email(X509 *x);
EVP_PKEY      *load_key(PERSONAL_CERT *pc, char *pass);
CertList      *mem_to_certlist(char *contents);
void           add_to_end_of_certlist(CertList **cl, char *name, X509 *cert);
void           free_certlist(CertList **cl);
PERSONAL_CERT *mem_to_personal_certs(char *contents);
void           free_personal_certs(PERSONAL_CERT **pc);


#endif /* PITH_SMKEYS_INCLUDED */
#endif /* SMIME */
