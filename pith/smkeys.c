#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: smkeys.c 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $";
#endif

/*
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

/*
 *  This is based on a contribution from Jonathan Paisley, see smime.c
 */


#include "../pith/headers.h"

#ifdef SMIME

#include "../pith/status.h"
#include "../pith/conf.h"
#include "../pith/remote.h"
#include "../pith/tempfile.h"
#include "../pith/busy.h"
#include "../pith/osdep/lstcmpnt.h"
#include "smkeys.h"

#ifdef APPLEKEYCHAIN
#include <Security/SecKeychain.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecKeychainSearch.h>
#include <Security/SecCertificate.h>
#endif /* APPLEKEYCHAIN */


/* internal prototypes */
static char     *emailstrclean(char *string);
static int       add_certs_in_dir(X509_LOOKUP *lookup, char *path);
static int       certlist_to_file(char *filename, CertList *certlist);
static int       mem_add_extra_cacerts(char *contents, X509_LOOKUP *lookup);


/*
 * Remove leading whitespace, trailing whitespace and convert 
 * to lowercase. Also remove slash characters
 *
 * Args: s, -- The string to clean
 *
 * Result: the cleaned string
 */
static char *
emailstrclean(char *string)
{
    char *s = string, *sc = NULL, *p = NULL;

    for(; *s; s++){				/* single pass */
	if(!isspace((unsigned char) (*s))){
	    p = NULL;				/* not start of blanks   */
	    if(!sc)				/* first non-blank? */
	      sc = string;			/* start copying */
	}
	else if(!p)				/* it's OK if sc == NULL */
	  p = sc;				/* start of blanks? */

	if(sc && *s!='/' && *s!='\\')		/* if copying, copy */
	  *sc++ = isupper((unsigned char) (*s))
			  ? (unsigned char) tolower((unsigned char) (*s))
			  : (unsigned char) (*s);
    }

    if(p)					/* if ending blanks  */
      *p = '\0';				/* tie off beginning */
    else if(!sc)				/* never saw a non-blank */
      *string = '\0';				/* so tie whole thing off */

    return(string);
}


/*
 * Add a lookup for each "*.crt" file in the given directory.
 */
static int
add_certs_in_dir(X509_LOOKUP *lookup, char *path)
{
    char buf[MAXPATH];
    struct direct *d;
    DIR	*dirp;
    int  ret = 0;

    dirp = opendir(path);
    if(dirp){

        while(!ret && (d=readdir(dirp)) != NULL){
            if(srchrstr(d->d_name, ".crt")){
    	    	build_path(buf, path, d->d_name, sizeof(buf));

    	    	if(!X509_LOOKUP_load_file(lookup, buf, X509_FILETYPE_PEM)){
		    q_status_message1(SM_ORDER, 3, 3, _("Error loading file %s"), buf);
		    ret = -1;
		}
            }

        }

        closedir(dirp);
    }

    return ret;
}


/*
 * Get an X509_STORE. This consists of the system
 * certs directory and any certificates in the user's
 * ~/.alpine-smime/ca directory.
 */
X509_STORE *
get_ca_store(void)
{
    X509_LOOKUP	*lookup;
    X509_STORE *store = NULL;

    dprint((9, "get_ca_store()"));

    if(!(store=X509_STORE_new())){
	dprint((9, "X509_STORE_new() failed"));
	return store;
    }

    if(!(lookup=X509_STORE_add_lookup(store, X509_LOOKUP_file()))){
	dprint((9, "X509_STORE_add_lookup() failed"));
	X509_STORE_free(store);
	return NULL;
    }
    
    if(ps_global->smime && ps_global->smime->catype == Container
       && ps_global->smime->cacontent){

	if(!mem_add_extra_cacerts(ps_global->smime->cacontent, lookup)){
	    X509_STORE_free(store);
	    return NULL;
	}
    }
    else if(ps_global->smime && ps_global->smime->catype == Directory
	    && ps_global->smime->capath){
	if(add_certs_in_dir(lookup, ps_global->smime->capath) < 0){
	    X509_STORE_free(store);
	    return NULL;
	}
    }

    if(!(lookup=X509_STORE_add_lookup(store, X509_LOOKUP_hash_dir()))){
	X509_STORE_free(store);
	return NULL;
    }

#ifdef SMIME_SSLCERTS
    dprint((9, "get_ca_store(): adding cacerts from %s", SMIME_SSLCERTS));
    X509_LOOKUP_add_dir(lookup, SMIME_SSLCERTS, X509_FILETYPE_PEM);
#endif

    return store;
}


EVP_PKEY *
load_key(PERSONAL_CERT *pc, char *pass)
{
    BIO *in;
    EVP_PKEY *key = NULL;
    char buf[MAXPATH], file[MAXPATH];

    if(!(ps_global->smime && pc && pc->name))
      return key;

    if(ps_global->smime->privatetype == Container){
	char *q;
	
	if(pc->keytext && (q = strstr(pc->keytext, "-----END")) != NULL){
	    while(*q && *q != '\n')
	      q++;

	    if(*q == '\n')
	      q++;

	    if((in = BIO_new_mem_buf(pc->keytext, q-pc->keytext)) != NULL){
		key = PEM_read_bio_PrivateKey(in, NULL, NULL, pass);
		BIO_free(in);
	    }
	}
    }
    else if(ps_global->smime->privatetype == Directory){
	/* filename is path/name.key */
	strncpy(buf, pc->name, sizeof(buf)-5);
	buf[sizeof(buf)-5] = '\0';
	strncat(buf, ".key", 5);
	build_path(file, ps_global->smime->privatepath, buf, sizeof(file));

	if(!(in = BIO_new_file(file, "r")))
	  return NULL;
	  
	key = PEM_read_bio_PrivateKey(in, NULL, NULL, pass);
	BIO_free(in);
    }

    return key;
}


#ifdef notdef
static char *
get_x509_name_entry(const char *key, X509_NAME *name)
{
    int i, c, n;
    char    buf[256];
    char	*id;
	
    if(!name)
      return NULL;
 
    c = X509_NAME_entry_count(name);
    
    for(i=0; i<c; i++){
    	X509_NAME_ENTRY *e;
	
    	e = X509_NAME_get_entry(name, i);
    	if(!e)
	  continue;

    	buf[0] = 0;
	id = buf;
		
        n = OBJ_obj2nid(e->object);
        if((n == NID_undef) || ((id=(char*) OBJ_nid2sn(n)) == NULL)){
            i2t_ASN1_OBJECT(buf, sizeof(buf), e->object);
            id = buf;
        }

	if((strucmp(id, "email")==0) || (strucmp(id, "emailAddress")==0)){
	    X509_NAME_get_text_by_OBJ(name, e->object, buf, sizeof(buf)-1);
	    return cpystr(buf);
	}
    }

    return NULL;
}


char *
get_x509_subject_email(X509 *x)
{
    char* result;
    result = get_x509_name_entry("email", X509_get_subject_name(x));
    if( !result ){
	result = get_x509_name_entry("emailAddress", X509_get_subject_name(x));
    }

    return result;
}
#endif /* notdef */

#include <openssl/x509v3.h>
/*
 * This newer version is from Adrian Vogel. It looks for the email
 * address not only in the email address field, but also in an
 * X509v3 extension field, Subject Altenative Name.
 */
char *
get_x509_subject_email(X509 *x)
{
    char *result = NULL;
#if OPENSSL_VERSION_NUMBER >= 0x1000000f /* OpenSSL 1.0.0 */
    STACK_OF(OPENSSL_STRING) *emails;
#else /* OpenSSL 0.x and 1.0.0-dev/beta */
    STACK *emails;
#endif

    emails = X509_get1_email(x);

#if OPENSSL_VERSION_NUMBER >= 0x1000000f /* OpenSSL 1.0.0 */
    if (sk_OPENSSL_STRING_num(emails) > 0) {
	/* take the first one on the stack */
	result = cpystr(sk_OPENSSL_STRING_value(emails, 0));
    }
#else /* OpenSSL 0.x and 1.0.0-dev/beta */
    if (sk_num(emails) > 0) {
	/* take the first one on the stack */
	result = cpystr(sk_value(emails, 0));
    }
#endif

    X509_email_free(emails);
    return result;
}


/*
 * Save the certificate for the given email address in
 * ~/.alpine-smime/public.
 *
 * Should consider the security hazards in making a file with
 * the email address that has come from the certificate.
 *
 * The argument email is destroyed.
 */
void
save_cert_for(char *email, X509 *cert)
{
    if(!ps_global->smime)
      return;

    dprint((9, "save_cert_for(%s)", email ? email : "?"));
    emailstrclean(email);

    if(ps_global->smime->publictype == Keychain){
#ifdef APPLEKEYCHAIN

	OSStatus rc;
	SecCertificateRef secCertificateRef;
	CSSM_DATA certData;

	memset((void *) &certData, 0, sizeof(certData));
	memset((void *) &secCertificateRef, 0, sizeof(secCertificateRef));

	/* convert OpenSSL X509 cert data to MacOS certData */
	if((certData.Length = i2d_X509(cert, &(certData.Data))) > 0){

	    /*
	     * Put that certData into a SecCertificateRef.
	     * Version 3 should work for versions 1-3.
	     */
	    if(!(rc=SecCertificateCreateFromData(&certData,
					         CSSM_CERT_X_509v3,
						 CSSM_CERT_ENCODING_DER,
						 &secCertificateRef))){

		/* add it to the default keychain */
		if(!(rc=SecCertificateAddToKeychain(secCertificateRef, NULL))){
		    /* ok */
		}
		else if(rc == errSecDuplicateItem){
		    dprint((9, "save_cert_for: certificate for %s already in keychain", email));
		}
		else{
		    dprint((9, "SecCertificateAddToKeychain failed"));
		}
	    }
	    else{
		dprint((9, "SecCertificateCreateFromData failed"));
	    }
	}
	else{
	    dprint((9, "i2d_X509 failed"));
	}

#endif /* APPLEKEYCHAIN */
    }
    else if(ps_global->smime->publictype == Container){
	REMDATA_S *rd = NULL;
	char       path[MAXPATH];
	char      *tempfile = NULL;
	int        err = 0;

	add_to_end_of_certlist(&ps_global->smime->publiccertlist, email, X509_dup(cert));

	if(!ps_global->smime->publicpath)
	  return;

	if(IS_REMOTE(ps_global->smime->publicpath)){
	    rd = rd_create_remote(RemImap, ps_global->smime->publicpath, REMOTE_SMIME_SUBTYPE,
				  NULL, "Error: ",
				  _("Can't access remote smime configuration."));
	    if(!rd)
	      return;
	    
	    (void) rd_read_metadata(rd);

	    if(rd->access == MaybeRorW){
		if(rd->read_status == 'R')
		  rd->access = ReadOnly;
		else
		  rd->access = ReadWrite;
	    }

	    if(rd->access != NoExists){

		rd_check_remvalid(rd, 1L);

		/*
		 * If the cached info says it is readonly but
		 * it looks like it's been fixed now, change it to readwrite.
		 */
		if(rd->read_status == 'R'){
		    rd_check_readonly_access(rd);
		    if(rd->read_status == 'W'){
			rd->access = ReadWrite;
			rd->flags |= REM_OUTOFDATE;
		    }
		    else
		      rd->access = ReadOnly;
		}
	    }

	    if(rd->flags & REM_OUTOFDATE){
		if(rd_update_local(rd) != 0){

		    dprint((1, "save_cert_for: rd_update_local failed\n"));
		    rd_close_remdata(&rd);
		    return;
		}
	    }
	    else
	      rd_open_remote(rd);

	    if(rd->access != ReadWrite || rd_remote_is_readonly(rd)){
		rd_close_remdata(&rd);
		return;
	    }

	    rd->flags |= DO_REMTRIM;

	    strncpy(path, rd->lf, sizeof(path)-1);
	    path[sizeof(path)-1] = '\0';
	}
	else{
	    strncpy(path, ps_global->smime->publicpath, sizeof(path)-1);
	    path[sizeof(path)-1] = '\0';
	}

	tempfile = tempfile_in_same_dir(path, "az", NULL);
	if(tempfile){
	    if(certlist_to_file(tempfile, ps_global->smime->publiccertlist))
	      err++;

	    if(!err){
		if(rename_file(tempfile, path) < 0){
		    q_status_message2(SM_ORDER, 3, 3,
			_("Can't rename %s to %s"), tempfile, path);
		    err++;
		}
	    }

	    if(!err && IS_REMOTE(ps_global->smime->publicpath)){
		int   e, we_cancel;
		char datebuf[200];

		datebuf[0] = '\0';

		we_cancel = busy_cue(_("Copying to remote smime container"), NULL, 1);
		if((e = rd_update_remote(rd, datebuf)) != 0){
		    if(e == -1){
			q_status_message2(SM_ORDER | SM_DING, 3, 5,
			  _("Error opening temporary smime file %s: %s"),
			    rd->lf, error_description(errno));
			dprint((1,
			   "write_remote_smime: error opening temp file %s\n",
			   rd->lf ? rd->lf : "?"));
		    }
		    else{
			q_status_message2(SM_ORDER | SM_DING, 3, 5,
					_("Error copying to %s: %s"),
					rd->rn, error_description(errno));
			dprint((1,
			  "write_remote_smime: error copying from %s to %s\n",
			  rd->lf ? rd->lf : "?", rd->rn ? rd->rn : "?"));
		    }
		    
		    q_status_message(SM_ORDER | SM_DING, 5, 5,
       _("Copy of smime cert to remote folder failed, changes NOT saved remotely"));
		}
		else{
		    rd_update_metadata(rd, datebuf);
		    rd->read_status = 'W';
		}

		rd_close_remdata(&rd);

		if(we_cancel)
		  cancel_busy_cue(-1);
	    }

	    fs_give((void **) &tempfile);
	}
    }
    else if(ps_global->smime->publictype == Directory){
	char    certfilename[MAXPATH];
	BIO    *bio_out;

	build_path(certfilename, ps_global->smime->publicpath,
		   email, sizeof(certfilename));
	strncat(certfilename, ".crt", sizeof(certfilename)-1-strlen(certfilename));
	certfilename[sizeof(certfilename)-1] = 0;

	bio_out = BIO_new_file(certfilename, "w");
	if(bio_out){
	    PEM_write_bio_X509(bio_out, cert);
	    BIO_free(bio_out);
	    q_status_message1(SM_ORDER, 1, 1, _("Saved certificate for <%s>"), email);
	}
	else{
	    q_status_message1(SM_ORDER, 1, 1, _("Couldn't save certificate for <%s>"), email);
	}
    }
}


/*
 * Try to retrieve the certificate for the given email address.
 * The caller should free the cert.
 */
X509 *
get_cert_for(char *email)
{
    char       *path;
    char	certfilename[MAXPATH];
    char    	emailaddr[MAXPATH];
    X509       *cert = NULL;
    BIO	       *in;

    if(!ps_global->smime)
      return cert;

    dprint((9, "get_cert_for(%s)", email ? email : "?"));

    strncpy(emailaddr, email, sizeof(emailaddr)-1);
    emailaddr[sizeof(emailaddr)-1] = 0;
    
    /* clean it up (lowercase, space removal) */
    emailstrclean(emailaddr);

    if(ps_global->smime->publictype == Keychain){
#ifdef APPLEKEYCHAIN

	OSStatus rc;
	SecKeychainItemRef itemRef = nil;
	SecKeychainAttributeList attrList;
	SecKeychainAttribute attrib;
	SecKeychainSearchRef searchRef = nil;
	CSSM_DATA certData;

	/* low-level form of MacOS data */
	memset((void *) &certData, 0, sizeof(certData));
	
	attrList.count = 1;
	attrList.attr = &attrib;

	/* kSecAlias means email address for a certificate */
	attrib.tag    = kSecAlias;
	attrib.data   = emailaddr;
	attrib.length = strlen(attrib.data);

	/* Find the certificate in the default keychain */
	if(!(rc=SecKeychainSearchCreateFromAttributes(NULL,
						       kSecCertificateItemClass,
						       &attrList,
						       &searchRef))){

	    if(!(rc=SecKeychainSearchCopyNext(searchRef, &itemRef))){

		/* extract the data portion of the certificate */
		if(!(rc=SecCertificateGetData((SecCertificateRef) itemRef, &certData))){

		    /*
		     * Convert it from MacOS form to OpenSSL form.
		     * The input is certData from above and the output
		     * is the X509 *cert.
		     */
		    if(!d2i_X509(&cert, &(certData.Data), certData.Length)){
			dprint((9, "d2i_X509 failed"));
		    }
		}
		else{
		    dprint((9, "SecCertificateGetData failed"));
		}
	    }
	    else if(rc == errSecItemNotFound){
		dprint((9, "get_cert_for: Public cert for %s not found", emailaddr));
	    }
	    else{
		dprint((9, "SecKeychainSearchCopyNext failed"));
	    }
	}
	else{
	    dprint((9, "SecKeychainSearchCreateFromAttributes failed"));
	}

	if(searchRef)
	  CFRelease(searchRef);

#endif /* APPLEKEYCHAIN */
    }
    else if(ps_global->smime->publictype == Container){
	if(ps_global->smime->publiccertlist){
	    CertList *cl;

	    for(cl = ps_global->smime->publiccertlist; cl; cl = cl->next){
		if(cl->name && !strucmp(emailaddr, cl->name))
		  break;
	    }

	    if(cl)
	      cert = X509_dup((X509 *) cl->x509_cert);
	}
    }
    else if(ps_global->smime->publictype == Directory){
	path = ps_global->smime->publicpath;
	build_path(certfilename, path, emailaddr, sizeof(certfilename));
	strncat(certfilename, ".crt", sizeof(certfilename)-1-strlen(certfilename));
	certfilename[sizeof(certfilename)-1] = 0;
	    
	if((in = BIO_new_file(certfilename, "r"))!=0){

	    cert = PEM_read_bio_X509(in, NULL, NULL, NULL);

	    if(cert){
		/* could check email addr in cert matches */
	    }

	    BIO_free(in);
	}
    }

    return cert;
}


PERSONAL_CERT *
mem_to_personal_certs(char *contents)
{
    PERSONAL_CERT *result = NULL;
    char *p, *q, *line, *name, *keytext, *save_p;
    X509 *cert = NULL;

    if(contents && *contents){
	for(p = contents; *p != '\0';){
	    line = p;

	    while(*p && *p != '\n')
	      p++;

	    save_p = NULL;
	    if(*p == '\n'){
		save_p = p;
		*p++ = '\0';
	    }

	    if(strncmp(EMAILADDRLEADER, line, strlen(EMAILADDRLEADER)) == 0){
		name = line + strlen(EMAILADDRLEADER);
		cert = get_cert_for(name);
		keytext = p;

		/* advance p past this record */
		if((q = strstr(keytext, "-----END")) != NULL){
		    while(*q && *q != '\n')
		      q++;

		    if(*q == '\n')
		      q++;

		    p = q;
		}
		else{
		    p = p + strlen(p);
		    q_status_message(SM_ORDER | SM_DING, 3, 3, _("Error in privatekey container, missing END"));
		}

		if(cert){
		    PERSONAL_CERT *pc;

		    pc = (PERSONAL_CERT *) fs_get(sizeof(*pc));
		    pc->cert = cert;
		    pc->name = cpystr(name);
		    pc->keytext = keytext;	/* a pointer into contents */

		    pc->key = load_key(pc, "");

		    pc->next = result;
		    result = pc;
		}
	    }

	    if(save_p)
	      *save_p = '\n';
	}
    }

    return result;
}


CertList *
mem_to_certlist(char *contents)
{
    CertList *ret = NULL;
    char *p, *q, *line, *name, *certtext, *save_p;
    X509 *cert = NULL;
    BIO *in;

    if(contents && *contents){
	for(p = contents; *p != '\0';){
	    line = p;

	    while(*p && *p != '\n')
	      p++;

	    save_p = NULL;
	    if(*p == '\n'){
		save_p = p;
		*p++ = '\0';
	    }

	    if(strncmp(EMAILADDRLEADER, line, strlen(EMAILADDRLEADER)) == 0){
		name = line + strlen(EMAILADDRLEADER);
		cert = NULL;
		certtext = p;
		if(strncmp("-----BEGIN", certtext, strlen("-----BEGIN")) == 0){
		    if((q = strstr(certtext, "-----END")) != NULL){
			while(*q && *q != '\n')
			  q++;

			if(*q == '\n')
			  q++;

			p = q;

			if((in = BIO_new_mem_buf(certtext, q-certtext)) != 0){
			    cert = PEM_read_bio_X509(in, NULL, NULL, NULL);

			    BIO_free(in);
			}
		    }
		}
		else{
		    p = p + strlen(p);
		    q_status_message1(SM_ORDER | SM_DING, 3, 3, _("Error in publiccert container, missing BEGIN, certtext=%s"), certtext);
		}

		if(name && cert){
		    add_to_end_of_certlist(&ret, name, cert);
		}
	    }

	    if(save_p)
	      *save_p = '\n';
	}
    }

    return ret;
}


/*
 * Add the CACert Container contents into the CACert store.
 *
 * Returns > 0 for success, 0 for failure
 */
int
mem_add_extra_cacerts(char *contents, X509_LOOKUP *lookup)
{
    char *p, *q, *line, *certtext, *save_p;
    BIO  *in, *out;
    int   len, failed = 0;
    char *tempfile;
    char  iobuf[4096];

    /*
     * The most straight-forward way to do this is to write
     * the container contents to a temp file and then load the
     * contents of the file with X509_LOOKUP_load_file(), like
     * is done in add_certs_in_dir(). What we don't know is if
     * each file should consist of one cacert or if they can all
     * just be jammed together into one file. To be safe, we'll use
     * one file per and do each in a separate operation.
     */

    if(contents && *contents){
	for(p = contents; *p != '\0';){
	    line = p;

	    while(*p && *p != '\n')
	      p++;

	    save_p = NULL;
	    if(*p == '\n'){
		save_p = p;
		*p++ = '\0';
	    }

	    /* look for separator line */
	    if(strncmp(CACERTSTORELEADER, line, strlen(CACERTSTORELEADER)) == 0){
		/* certtext is the content that should go in a file */
		certtext = p;
		if(strncmp("-----BEGIN", certtext, strlen("-----BEGIN")) == 0){
		    if((q = strstr(certtext, CACERTSTORELEADER)) != NULL){
			p = q;
		    }
		    else{		/* end of file */
			q = certtext + strlen(certtext);
			p = q;
		    }

		    in = BIO_new_mem_buf(certtext, q-certtext);
		    if(in){
			tempfile = temp_nam(NULL, "az");
			out = NULL;
			if(tempfile)
			  out = BIO_new_file(tempfile, "w");

			if(out){
			    while((len = BIO_read(in, iobuf, sizeof(iobuf))) > 0)
			      BIO_write(out, iobuf, len);

			    BIO_free(out);
			    if(!X509_LOOKUP_load_file(lookup, tempfile, X509_FILETYPE_PEM))
			      failed++;

			    fs_give((void **) &tempfile);
			}
			  
			BIO_free(in);
		    }
		}
		else{
		    p = p + strlen(p);
		    q_status_message1(SM_ORDER | SM_DING, 3, 3, _("Error in cacert container, missing BEGIN, certtext=%s"), certtext);
		}
	    }
	    else{
		p = p + strlen(p);
                q_status_message1(SM_ORDER | SM_DING, 3, 3, _("Error in cacert container, missing separator, line=%s"), line);
	    }

	    if(save_p)
	      *save_p = '\n';
	}
    }

    return(!failed);
}


int
certlist_to_file(char *filename, CertList *certlist)
{
    CertList *cl;
    BIO      *bio_out = NULL;
    int       ret = -1;

    if(filename && (bio_out=BIO_new_file(filename, "w")) != NULL){
	ret = 0;
	for(cl = certlist; cl; cl = cl->next){
	    if(cl->name && cl->name[0] && cl->x509_cert){
		if(!((BIO_puts(bio_out, EMAILADDRLEADER) > 0)
		     && (BIO_puts(bio_out, cl->name) > 0)
		     && (BIO_puts(bio_out, "\n") > 0)))
		  ret = -1;

		if(!PEM_write_bio_X509(bio_out, (X509 *) cl->x509_cert))
		  ret = -1;
	    }
	}

	BIO_free(bio_out);
    }

    return ret;
}


void
add_to_end_of_certlist(CertList **cl, char *name, X509 *cert)
{
    CertList *new, *clp;

    if(!cl)
      return;

    new = (CertList *) fs_get(sizeof(*new));
    memset((void *) new, 0, sizeof(*new));
    new->x509_cert = cert;
    new->name = name ? cpystr(name) : NULL;

    if(!*cl){
	*cl = new;
    }
    else{
	for(clp = (*cl); clp->next; clp = clp->next)
	  ;

	clp->next = new;
    }
}


void
free_certlist(CertList **cl)
{
    if(cl && *cl){
	free_certlist(&(*cl)->next);
	if((*cl)->name)
	  fs_give((void **) &(*cl)->name);

	if((*cl)->x509_cert)
	  X509_free((X509 *) (*cl)->x509_cert);

	fs_give((void **) cl);
    }
}


void
free_personal_certs(PERSONAL_CERT **pc)
{
    if(pc && *pc){
	free_personal_certs(&(*pc)->next);
	if((*pc)->name)
	  fs_give((void **) &(*pc)->name);
	
	if((*pc)->name)
	  fs_give((void **) &(*pc)->name);

	if((*pc)->cert)
	  X509_free((*pc)->cert);

	if((*pc)->key)
	  EVP_PKEY_free((*pc)->key);

	fs_give((void **) pc);
    }
}

#endif /* SMIME */
