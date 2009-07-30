#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: smime.c 1176 2008-09-29 21:16:42Z hubert@u.washington.edu $";
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
 *  This is based on a contribution from Jonathan Paisley
 *
 *  File:   	    smime.c
 *  Author: 	    paisleyj@dcs.gla.ac.uk
 *  Date:   	    01/2001
 */


#include "../pith/headers.h"

#ifdef SMIME

#include "../pith/osdep/canaccess.h"
#include "../pith/helptext.h"
#include "../pith/store.h"
#include "../pith/status.h"
#include "../pith/detach.h"
#include "../pith/conf.h"
#include "../pith/smkeys.h"
#include "../pith/smime.h"
#include "../pith/mailpart.h"
#include "../pith/reply.h"
#include "../pith/tempfile.h"
#include "../pith/readfile.h"
#include "../pith/remote.h"

#include <openssl/buffer.h>


typedef enum {Public, Private, CACert} WhichCerts;


/* internal prototypes */
static void            forget_private_keys(void);
static int             app_RAND_load_file(const char *file);
static void            openssl_extra_randomness(void);
static int             app_RAND_write_file(const char *file);
static void            smime_init(void);
static const char     *openssl_error_string(void);
static void            create_local_cache(char *base, BODY *b);
static BIO            *raw_part_to_bio(long msgno, const char *section);
static long            rfc822_output_func(void *b, char *string);
static int             load_private_key(PERSONAL_CERT *pcert);
static void            setup_pkcs7_body_for_signature(BODY *b, char *description,
						      char *type, char *filename);
static BIO            *body_to_bio(BODY *body);
static BIO            *bio_from_store(STORE_S *store);
static STORE_S        *get_part_contents(long msgno, const char *section);
static PKCS7         *get_pkcs7_from_part(long msgno, const char *section);
static int            do_signature_verify(PKCS7 *p7, BIO *in, BIO *out);
static int            do_detached_signature_verify(BODY *b, long msgno, char *section);
static PERSONAL_CERT *find_certificate_matching_pkcs7(PKCS7 *p7);
static int            do_decoding(BODY *b, long msgno, const char *section);
static void           free_smime_struct(SMIME_STUFF_S **smime);
static void           setup_storage_locations(void);
static int            copy_dir_to_container(WhichCerts which);
static int            copy_container_to_dir(WhichCerts which);


int  (*pith_opt_smime_get_passphrase)(void);


static X509_STORE   *s_cert_store;

/* State management for randomness functions below */
static int seeded = 0;
static int egdsocket = 0;


/*
 * Forget any cached private keys
 */
static void
forget_private_keys(void)
{
    PERSONAL_CERT *pcert;
    size_t len;
    volatile char *p;
    
    dprint((9, "forget_private_keys()"));
    if(ps_global->smime){
	for(pcert=(PERSONAL_CERT *) ps_global->smime->personal_certs;
	    pcert;
	    pcert=pcert->next){

	    if(pcert->key){
		EVP_PKEY_free(pcert->key);
		pcert->key = NULL;
	    }
	}

	ps_global->smime->entered_passphrase = 0;
	len = sizeof(ps_global->smime->passphrase);
	p = ps_global->smime->passphrase;

	while(len-- > 0)
	  *p++ = '\0';
    }
}


/*
 * taken from openssl/apps/app_rand.c
 */
static int
app_RAND_load_file(const char *file)
{
    char buffer[200];

    if(file == NULL)
      file = RAND_file_name(buffer, sizeof buffer);
    else if(RAND_egd(file) > 0){
	/* we try if the given filename is an EGD socket.
	   if it is, we don't write anything back to the file. */
	egdsocket = 1;
	return 1;
    }

    if(file == NULL || !RAND_load_file(file, -1)){
	if(RAND_status() == 0){
	    dprint((1, "unable to load 'random state'\n"));
	    dprint((1, "This means that the random number generator has not been seeded\n"));
	    dprint((1, "with much random data.\n"));
	}

	return 0;
    }

    seeded = 1;
    return 1;
}


/*
 * copied and fiddled from imap/src/osdep/unix/auth_ssl.c
 */
static void
openssl_extra_randomness(void)
{
#if !defined(WIN32)
    int fd;
    unsigned long i;
    char *tf = NULL;
    char tmp[MAXPATH];
    struct stat sbuf;
				/* if system doesn't have /dev/urandom */
    if(stat ("/dev/urandom", &sbuf)){
      tmp[0] = '0';
      tf = temp_nam(NULL, NULL);
      if(tf){
	strncpy(tmp, tf, sizeof(tmp));
	tmp[sizeof(tmp)-1] = '\0';
	fs_give((void **) &tf);
      }     

      if((fd = open(tmp, O_WRONLY|O_CREAT|O_EXCL, 0600)) < 0)
	i = (unsigned long) tmp;
      else{
	unlink(tmp);		/* don't need the file */
	fstat(fd, &sbuf);	/* get information about the file */
	i = sbuf.st_ino;	/* remember its inode */
	close(fd);		/* or its descriptor */
      }
				/* not great but it'll have to do */
      snprintf(tmp+strlen(tmp), sizeof(tmp)-strlen(tmp), "%.80s%lx%lx%lx",
	       tcp_serverhost (),i,
	       (unsigned long) (time (0) ^ gethostid ()),
	       (unsigned long) getpid ());
      RAND_seed(tmp, strlen(tmp));
    }
#endif
}


/* taken from openssl/apps/app_rand.c */
static int
app_RAND_write_file(const char *file)
{
    char buffer[200];

    if(egdsocket || !seeded)
	/*
	 * If we did not manage to read the seed file,
	 * we should not write a low-entropy seed file back --
	 * it would suppress a crucial warning the next time
	 * we want to use it.
	 */
	return 0;

    if(file == NULL)
      file = RAND_file_name(buffer, sizeof buffer);

    if(file == NULL || !RAND_write_file(file)){
	dprint((1, "unable to write 'random state'\n"));
	return 0;
    }

    return 1;
}


/* Installed as an atexit() handler to save the random data */
void
smime_deinit(void)
{
    dprint((9, "smime_deinit()"));
    app_RAND_write_file(NULL);
    free_smime_struct(&ps_global->smime);
}


/* Initialise openssl stuff if needed */
static void
smime_init(void)
{
    if(F_OFF(F_DONT_DO_SMIME, ps_global) && !(ps_global->smime && ps_global->smime->inited)){

	dprint((9, "smime_init()"));
	if(!ps_global->smime)
	  ps_global->smime = new_smime_struct();

	setup_storage_locations();

	s_cert_store = get_ca_store();

        OpenSSL_add_all_algorithms();
        ERR_load_crypto_strings();

	app_RAND_load_file(NULL);

    	openssl_extra_randomness();
	ps_global->smime->inited = 1;
    }
    
    ERR_clear_error();
}


static void
setup_storage_locations(void)
{
    int publiccertcontainer = 0, privatekeycontainer = 0, cacertcontainer = 0;
    char path[MAXPATH+1], *contents;

    if(!ps_global->smime)
      return;

#ifdef APPLEKEYCHAIN
    if(F_ON(F_PUBLICCERTS_IN_KEYCHAIN, ps_global)){
	ps_global->smime->publictype = Keychain;
    }
    else{
#endif /* APPLEKEYCHAIN */
    /* Public certificates in a container */
    if(ps_global->VAR_PUBLICCERT_CONTAINER && ps_global->VAR_PUBLICCERT_CONTAINER[0]){

	publiccertcontainer = 1;
	contents = NULL;
	path[0] = '\0';
	if(!signature_path(ps_global->VAR_PUBLICCERT_CONTAINER, path, MAXPATH))
	  publiccertcontainer = 0;

	if(publiccertcontainer && !IS_REMOTE(path)
	   && ps_global->VAR_OPER_DIR
           && !in_dir(ps_global->VAR_OPER_DIR, path)){
	    q_status_message2(SM_ORDER | SM_DING, 3, 4,
			  /* TRANSLATORS: First arg is the directory name, second is
			     the file user wants to read but can't. */
			  _("Can't read file outside %s: %s"),
			  ps_global->VAR_OPER_DIR, path);
	    publiccertcontainer = 0;
	}

	if(publiccertcontainer
	   && (IS_REMOTE(path) || can_access(path, ACCESS_EXISTS) == 0)){
	    if(!(IS_REMOTE(path) && (contents = simple_read_remote_file(path, REMOTE_SMIME_SUBTYPE)))
	       &&
	       !(contents = read_file(path, READ_FROM_LOCALE)))
	      publiccertcontainer = 0;
	}

	if(publiccertcontainer && path[0]){
	    ps_global->smime->publictype = Container;
	    ps_global->smime->publicpath = cpystr(path);

	    if(contents){
		ps_global->smime->publiccontent = contents;
		ps_global->smime->publiccertlist = mem_to_certlist(contents);
	    }
	}
    }

    /* Public certificates in a directory of files */
    if(!publiccertcontainer){
	ps_global->smime->publictype = Directory;

	path[0] = '\0';
	if(!(signature_path(ps_global->VAR_PUBLICCERT_DIR, path, MAXPATH)
	     && !IS_REMOTE(path)))
	  ps_global->smime->publictype = Nada;
	else if(can_access(path, ACCESS_EXISTS)){
	    if(our_mkpath(path, 0700)){
		q_status_message1(SM_ORDER, 3, 3, _("Can't create directory %s"), path);
		ps_global->smime->publictype = Nada;
	    }
	}

	if(ps_global->smime->publictype == Directory)
	  ps_global->smime->publicpath = cpystr(path);
    }

#ifdef APPLEKEYCHAIN
    }
#endif /* APPLEKEYCHAIN */

    /* private keys in a container */
    if(ps_global->VAR_PRIVATEKEY_CONTAINER && ps_global->VAR_PRIVATEKEY_CONTAINER[0]){

	privatekeycontainer = 1;
	contents = NULL;
	path[0] = '\0';
	if(!signature_path(ps_global->VAR_PRIVATEKEY_CONTAINER, path, MAXPATH))
	  privatekeycontainer = 0;

	if(privatekeycontainer && !IS_REMOTE(path)
	   && ps_global->VAR_OPER_DIR
           && !in_dir(ps_global->VAR_OPER_DIR, path)){
	    q_status_message2(SM_ORDER | SM_DING, 3, 4,
			  /* TRANSLATORS: First arg is the directory name, second is
			     the file user wants to read but can't. */
			  _("Can't read file outside %s: %s"),
			  ps_global->VAR_OPER_DIR, path);
	    privatekeycontainer = 0;
	}

	if(privatekeycontainer
	   && (IS_REMOTE(path) || can_access(path, ACCESS_EXISTS) == 0)){
	    if(!(IS_REMOTE(path) && (contents = simple_read_remote_file(path, REMOTE_SMIME_SUBTYPE)))
	       &&
	       !(contents = read_file(path, READ_FROM_LOCALE)))
	      privatekeycontainer = 0;
	}

	if(privatekeycontainer && path[0]){
	    ps_global->smime->privatetype = Container;
	    ps_global->smime->privatepath = cpystr(path);

	    if(contents){
		ps_global->smime->privatecontent = contents;
		ps_global->smime->personal_certs = mem_to_personal_certs(contents);
	    }
	}
    }

    /* private keys in a directory of files */
    if(!privatekeycontainer){
	PERSONAL_CERT *result = NULL;

	ps_global->smime->privatetype = Directory;

	path[0] = '\0';
	if(!(signature_path(ps_global->VAR_PRIVATEKEY_DIR, path, MAXPATH)
	     && !IS_REMOTE(path)))
	  ps_global->smime->privatetype = Nada;
	else if(can_access(path, ACCESS_EXISTS)){
	    if(our_mkpath(path, 0700)){
		q_status_message1(SM_ORDER, 3, 3, _("Can't create directory %s"), path);
		ps_global->smime->privatetype = Nada;
	    }
	}

	if(ps_global->smime->privatetype == Directory){
	    char buf2[MAXPATH];
	    struct dirent *d;
	    DIR *dirp;

	    ps_global->smime->privatepath = cpystr(path);
	    dirp = opendir(path);
	    if(dirp){

		while((d=readdir(dirp)) != NULL){
		    X509 *cert;
		    size_t ll;

		    if((ll=strlen(d->d_name)) && ll > 4 && !strcmp(d->d_name+ll-4, ".key")){

			/* copy file name to temp buffer */
			strncpy(buf2, d->d_name, sizeof(buf2)-1);
			buf2[sizeof(buf2)-1] = '\0';
			/* chop off ".key" trailier */
			buf2[strlen(buf2)-4] = 0;
			/* Look for certificate */
			cert = get_cert_for(buf2);

			if(cert){
			    PERSONAL_CERT *pc;

			    /* create a new PERSONAL_CERT, fill it in */

			    pc = (PERSONAL_CERT *) fs_get(sizeof(*pc));
			    pc->cert = cert;
			    pc->name = cpystr(buf2);

			    /* Try to load the key with an empty password */
			    pc->key = load_key(pc, "");

			    pc->next = result;
			    result = pc;
			}
		    }
		}

		closedir(dirp);
	    }
	}

	ps_global->smime->personal_certs = result;
    }

    /* extra cacerts in a container */
    if(ps_global->VAR_CACERT_CONTAINER && ps_global->VAR_CACERT_CONTAINER[0]){

	cacertcontainer = 1;
	contents = NULL;
	path[0] = '\0';
	if(!signature_path(ps_global->VAR_CACERT_CONTAINER, path, MAXPATH))
	  cacertcontainer = 0;

	if(cacertcontainer && !IS_REMOTE(path)
	   && ps_global->VAR_OPER_DIR
           && !in_dir(ps_global->VAR_OPER_DIR, path)){
	    q_status_message2(SM_ORDER | SM_DING, 3, 4,
			  /* TRANSLATORS: First arg is the directory name, second is
			     the file user wants to read but can't. */
			  _("Can't read file outside %s: %s"),
			  ps_global->VAR_OPER_DIR, path);
	    cacertcontainer = 0;
	}

	if(cacertcontainer
	   && (IS_REMOTE(path) || can_access(path, ACCESS_EXISTS) == 0)){
	    if(!(IS_REMOTE(path) && (contents = simple_read_remote_file(path, REMOTE_SMIME_SUBTYPE)))
	       &&
	       !(contents = read_file(path, READ_FROM_LOCALE)))
	      cacertcontainer = 0;
	}

	if(cacertcontainer && path[0]){
	    ps_global->smime->catype = Container;
	    ps_global->smime->capath = cpystr(path);
	    ps_global->smime->cacontent = contents;
	}
    }

    if(!cacertcontainer){
	ps_global->smime->catype = Directory;

	path[0] = '\0';
	if(!(signature_path(ps_global->VAR_CACERT_DIR, path, MAXPATH)
	     && !IS_REMOTE(path)))
	  ps_global->smime->catype = Nada;
	else if(can_access(path, ACCESS_EXISTS)){
	    if(our_mkpath(path, 0700)){
		q_status_message1(SM_ORDER, 3, 3, _("Can't create directory %s"), path);
		ps_global->smime->catype = Nada;
	    }
	}

	if(ps_global->smime->catype == Directory)
	  ps_global->smime->capath = cpystr(path);
    }
}


int
copy_publiccert_dir_to_container(void)
{
    return(copy_dir_to_container(Public));
}


int
copy_publiccert_container_to_dir(void)
{
    return(copy_container_to_dir(Public));
}


int
copy_privatecert_dir_to_container(void)
{
    return(copy_dir_to_container(Private));
}


int
copy_privatecert_container_to_dir(void)
{
    return(copy_container_to_dir(Private));
}


int
copy_cacert_dir_to_container(void)
{
    return(copy_dir_to_container(CACert));
}


int
copy_cacert_container_to_dir(void)
{
    return(copy_container_to_dir(CACert));
}


/*
 * returns 0 on success, -1 on failure
 */
int
copy_dir_to_container(WhichCerts which)
{
    int ret = 0;
    BIO *bio_out = NULL, *bio_in = NULL;
    char srcpath[MAXPATH+1], dstpath[MAXPATH+1], emailaddr[MAXPATH], file[MAXPATH], line[4096];
    char *tempfile = NULL;
    DIR *dirp;
    struct dirent *d;
    REMDATA_S *rd = NULL;
    char *configdir = NULL;
    char *configpath = NULL;
    char *filesuffix = NULL;

    dprint((9, "copy_dir_to_container(%s)", which==Public ? "Public" : which==Private ? "Private" : which==CACert ? "CACert" : "?"));
    smime_init();

    srcpath[0] = '\0';
    dstpath[0] = '\0';
    file[0] = '\0';
    emailaddr[0] = '\0';

    if(which == Public){
	configdir  = ps_global->VAR_PUBLICCERT_DIR;
	configpath = ps_global->smime->publicpath;
	filesuffix = ".crt";
    }
    else if(which == Private){
	configdir = ps_global->VAR_PRIVATEKEY_DIR;
	configpath = ps_global->smime->privatepath;
	filesuffix = ".key";
    }
    else if(which == CACert){
	configdir = ps_global->VAR_CACERT_DIR;
	configpath = ps_global->smime->capath;
	filesuffix = ".crt";
    }

    if(!(configdir && configdir[0])){
	q_status_message(SM_ORDER, 3, 3, _("Directory not defined"));
	return -1;
    }

    if(!(configpath && configpath[0])){
#ifdef APPLEKEYCHAIN
	if(which == Public && F_ON(F_PUBLICCERTS_IN_KEYCHAIN, ps_global)){
	    q_status_message(SM_ORDER, 3, 3, _("Turn off the Keychain feature above first"));
	    return -1;
	}
#endif /* APPLEKEYCHAIN */
	q_status_message(SM_ORDER, 3, 3, _("Container path is not defined"));
	return -1;
    }

    if(!(filesuffix && strlen(filesuffix) == 4)){
	return -1;
    }


    /*
     * If there is a legit directory to read from set up the
     * container file to write to.
     */
    if(signature_path(configdir, srcpath, MAXPATH) && !IS_REMOTE(srcpath)){

	if(IS_REMOTE(configpath)){
	    rd = rd_create_remote(RemImap, configpath, REMOTE_SMIME_SUBTYPE,
				  NULL, "Error: ",
				  _("Can't access remote smime configuration."));
	    if(!rd)
	      return -1;
	    
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

		    dprint((1, "copy_dir_to_container: rd_update_local failed\n"));
		    rd_close_remdata(&rd);
		    return -1;
		}
	    }
	    else
	      rd_open_remote(rd);

	    if(rd->access != ReadWrite || rd_remote_is_readonly(rd)){
		rd_close_remdata(&rd);
		return -1;
	    }

	    rd->flags |= DO_REMTRIM;

	    strncpy(dstpath, rd->lf, sizeof(dstpath)-1);
	    dstpath[sizeof(dstpath)-1] = '\0';
	}
	else{
	    strncpy(dstpath, configpath, sizeof(dstpath)-1);
	    dstpath[sizeof(dstpath)-1] = '\0';
	}

	/*
	 * dstpath is either the local Container file or the local cache file
	 * for the remote Container file.
	 */
	tempfile = tempfile_in_same_dir(dstpath, "az", NULL);
    }

    /*
     * If there is a legit directory to read from and a tempfile
     * to write to we continue.
     */
    if(tempfile && (bio_out=BIO_new_file(tempfile, "w")) != NULL){

	dirp = opendir(srcpath);
	if(dirp){

	    while((d=readdir(dirp)) && !ret){
		size_t ll;

		if((ll=strlen(d->d_name)) && ll > 4 && !strcmp(d->d_name+ll-4, filesuffix)){

		    /* copy file name to temp buffer */
		    strncpy(emailaddr, d->d_name, sizeof(emailaddr)-1);
		    emailaddr[sizeof(emailaddr)-1] = '\0';
		    /* chop off suffix trailier */
		    emailaddr[strlen(emailaddr)-4] = 0;

		    /*
		     * This is the separator between the contents of
		     * different files.
		     */
		    if(which == CACert){
			if(!((BIO_puts(bio_out, CACERTSTORELEADER) > 0)
			     && (BIO_puts(bio_out, emailaddr) > 0)
			     && (BIO_puts(bio_out, "\n") > 0)))
			  ret = -1;
		    }
		    else{
			if(!((BIO_puts(bio_out, EMAILADDRLEADER) > 0)
			     && (BIO_puts(bio_out, emailaddr) > 0)
			     && (BIO_puts(bio_out, "\n") > 0)))
			  ret = -1;
		    }

		    /* read then write contents of file */
		    build_path(file, srcpath, d->d_name, sizeof(file));
		    if(!(bio_in = BIO_new_file(file, "r")))
		      ret = -1;

		    if(!ret){
			int good_stuff = 0;

			while(BIO_gets(bio_in, line, sizeof(line)) > 0){
			    if(strncmp("-----BEGIN", line, strlen("-----BEGIN")) == 0)
			      good_stuff = 1;

			    if(good_stuff)
			      BIO_puts(bio_out, line);

			    if(strncmp("-----END", line, strlen("-----END")) == 0)
			      good_stuff = 0;
			}
		    }

		    BIO_free(bio_in);
		}
	    }

	    closedir(dirp);
	}

	BIO_free(bio_out);

	if(!ret){
	    if(rename_file(tempfile, dstpath) < 0){
		q_status_message2(SM_ORDER, 3, 3,
		    _("Can't rename %s to %s"), tempfile, dstpath);
		ret = -1;
	    }
	    
	    /* if the container is remote, copy it */
	    if(!ret && IS_REMOTE(configpath)){
		int   e;
		char datebuf[200];

		datebuf[0] = '\0';

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
       _("Copy of smime key to remote folder failed, NOT saved remotely"));
		}
		else{
		    rd_update_metadata(rd, datebuf);
		    rd->read_status = 'W';
		}

		rd_close_remdata(&rd);
	    }
	}
    }

    if(tempfile)
      fs_give((void **) &tempfile);

    return ret;
}


/*
 * returns 0 on success, -1 on failure
 */
int
copy_container_to_dir(WhichCerts which)
{
    char  path[MAXPATH+1], file[MAXPATH+1], buf[MAXPATH+1];
    char  iobuf[4096];
    char *contents = NULL;
    char *leader = NULL;
    char *filesuffix = NULL;
    char *configdir = NULL;
    char *configpath = NULL;
    char *tempfile = NULL;
    char *p, *q, *line, *name, *certtext, *save_p;
    int  len;
    BIO *in, *out;

    dprint((9, "copy_container_to_dir(%s)", which==Public ? "Public" : which==Private ? "Private" : which==CACert ? "CACert" : "?"));
    smime_init();

    path[0] = '\0';

    if(which == Public){
	leader = EMAILADDRLEADER;
	contents  = ps_global->smime->publiccontent;
	configdir  = ps_global->VAR_PUBLICCERT_DIR;
	configpath = ps_global->smime->publicpath;
	filesuffix = ".crt";
	if(!(configpath && configpath[0])){
#ifdef APPLEKEYCHAIN
	    if(which == Public && F_ON(F_PUBLICCERTS_IN_KEYCHAIN, ps_global)){
		q_status_message(SM_ORDER, 3, 3, _("Turn off the Keychain feature above first"));
		return -1;
	    }
#endif /* APPLEKEYCHAIN */
	    q_status_message(SM_ORDER, 3, 3, _("Container path is not defined"));
	    return -1;
	}

	fs_give((void **) &ps_global->smime->publicpath);

	path[0] = '\0';
	if(!(signature_path(ps_global->VAR_PUBLICCERT_DIR, path, MAXPATH)
	     && !IS_REMOTE(path))){
	    q_status_message(SM_ORDER, 3, 3, _("Directory is not defined"));
	    return -1;
	}

	if(can_access(path, ACCESS_EXISTS)){
	    if(our_mkpath(path, 0700)){
		q_status_message1(SM_ORDER, 3, 3, _("Can't create directory %s"), path);
		return -1;
	    }
	}

	ps_global->smime->publicpath = cpystr(path);
	configpath = ps_global->smime->publicpath;
    }
    else if(which == Private){
	leader = EMAILADDRLEADER;
	contents  = ps_global->smime->privatecontent;
	configdir = ps_global->VAR_PRIVATEKEY_DIR;
	configpath = ps_global->smime->privatepath;
	filesuffix = ".key";
	if(!(configpath && configpath[0])){
	    q_status_message(SM_ORDER, 3, 3, _("Container path is not defined"));
	    return -1;
	}

	fs_give((void **) &ps_global->smime->privatepath);

	path[0] = '\0';
	if(!(signature_path(ps_global->VAR_PRIVATEKEY_DIR, path, MAXPATH)
	     && !IS_REMOTE(path))){
	    q_status_message(SM_ORDER, 3, 3, _("Directory is not defined"));
	    return -1;
	}

	if(can_access(path, ACCESS_EXISTS)){
	    if(our_mkpath(path, 0700)){
		q_status_message1(SM_ORDER, 3, 3, _("Can't create directory %s"), path);
		return -1;
	    }
	}

	ps_global->smime->privatepath = cpystr(path);
	configpath = ps_global->smime->privatepath;
    }
    else if(which == CACert){
	leader = CACERTSTORELEADER;
	contents  = ps_global->smime->cacontent;
	configdir = ps_global->VAR_CACERT_DIR;
	configpath = ps_global->smime->capath;
	filesuffix = ".crt";
	if(!(configpath && configpath[0])){
	    q_status_message(SM_ORDER, 3, 3, _("Container path is not defined"));
	    return -1;
	}

	fs_give((void **) &ps_global->smime->capath);

	path[0] = '\0';
	if(!(signature_path(ps_global->VAR_CACERT_DIR, path, MAXPATH)
	     && !IS_REMOTE(path))){
	    q_status_message(SM_ORDER, 3, 3, _("Directory is not defined"));
	    return -1;
	}

	if(can_access(path, ACCESS_EXISTS)){
	    if(our_mkpath(path, 0700)){
		q_status_message1(SM_ORDER, 3, 3, _("Can't create directory %s"), path);
		return -1;
	    }
	}

	ps_global->smime->capath = cpystr(path);
	configpath = ps_global->smime->capath;
    }

    if(!(configdir && configdir[0])){
	q_status_message(SM_ORDER, 3, 3, _("Directory not defined"));
	return -1;
    }

    if(!(configpath && configpath[0])){
	q_status_message(SM_ORDER, 3, 3, _("Container path is not defined"));
	return -1;
    }

    if(!(filesuffix && strlen(filesuffix) == 4)){
	return -1;
    }


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

	    if(strncmp(leader, line, strlen(leader)) == 0){
		name = line + strlen(leader);
		certtext = p;
		if(strncmp("-----BEGIN", certtext, strlen("-----BEGIN")) == 0){
		    if((q = strstr(certtext, leader)) != NULL){
			p = q;
		    }
		    else{		/* end of file */
			q = certtext + strlen(certtext);
			p = q;
		    }

		    strncpy(buf, name, sizeof(buf)-5);
		    buf[sizeof(buf)-5] = '\0';
		    strncat(buf, filesuffix, 5);
		    build_path(file, configpath, buf, sizeof(file));

		    in = BIO_new_mem_buf(certtext, q-certtext);
		    if(in){
			tempfile = tempfile_in_same_dir(file, "az", NULL);
			out = NULL;
			if(tempfile)
			  out = BIO_new_file(tempfile, "w");

			if(out){
			    while((len = BIO_read(in, iobuf, sizeof(iobuf))) > 0)
			      BIO_write(out, iobuf, len);

			    BIO_free(out);

			    if(rename_file(tempfile, file) < 0){
				q_status_message2(SM_ORDER, 3, 3,
				                  _("Can't rename %s to %s"),
						  tempfile, file);
				return -1;
			    }

			    fs_give((void **) &tempfile);
			}
			  
			BIO_free(in);
		    }
		}
	    }

	    if(save_p)
	      *save_p = '\n';
	}
    }

    return 0;
}


#ifdef APPLEKEYCHAIN

int
copy_publiccert_container_to_keychain(void)
{
    /* NOT IMPLEMNTED */
    return -1;
}

int
copy_publiccert_keychain_to_container(void)
{
    /* NOT IMPLEMNTED */
    return -1;
}

#endif /* APPLEKEYCHAIN */


/*
 * Get a pointer to a string describing the most recent OpenSSL error.
 * It's statically allocated, so don't change or attempt to free it.
 */
static const char *
openssl_error_string(void)
{
    char	*errs;
    const char	*data = NULL;
    long errn;

    errn = ERR_peek_error_line_data(NULL, NULL, &data, NULL);
    errs = (char*) ERR_reason_error_string(errn);

    if(errs)
      return errs;
    else if(data)
      return data;

    return "unknown error";
}


/* Return true if the body looks like a PKCS7 object */
int
is_pkcs7_body(BODY *body)
{
    int result;

    result = body->type==TYPEAPPLICATION &&
             body->subtype &&
             (strucmp(body->subtype,"pkcs7-mime")==0 ||
              strucmp(body->subtype,"x-pkcs7-mime")==0 ||
	      strucmp(body->subtype,"pkcs7-signature")==0 ||
	      strucmp(body->subtype,"x-pkcs7-signature")==0);

    return result;
}


#ifdef notdef
/*
 * Somewhat useful debug utility to dump the contents of a BIO to a file.
 * Note that a memory BIO will have its contents eliminated after they
 * are read so this will break the next step.
 */
static void
dump_bio_to_file(BIO *in, char *filename)
{
    char iobuf[4096];
    int len;
    BIO *out;
    
    out = BIO_new_file(filename, "w");
    
    if(out){
	if(BIO_method_type(in) != BIO_TYPE_MEM)
    	  BIO_reset(in);
    
	while((len = BIO_read(in, iobuf, sizeof(iobuf))) > 0)
	  BIO_write(out, iobuf, len);

    	BIO_free(out);
    }
	
    BIO_reset(in);
}
#endif


/*
 * Recursively stash a pointer to the decrypted data in our
 * manufactured body.
 */
static void
create_local_cache(char *base, BODY *b)
{
    if(b->type==TYPEMULTIPART){
        PART *p;

#if 0 
        cpytxt(&b->contents.text, base + b->contents.offset, b->size.bytes);
#else
    	/*
    	 * We don't really want to copy the real body contents. It shouldn't be
	 * used, and in the case of a message with attachments, we'll be 
	 * duplicating the files multiple times.
	 */
    	cpytxt(&b->contents.text, "BODY UNAVAILABLE", 16);
#endif

        for(p=b->nested.part; p; p=p->next)
          create_local_cache(base, (BODY*) p);
    }
    else{
        cpytxt(&b->contents.text, base + b->contents.offset, b->size.bytes);
    }
}


static long
rfc822_output_func(void *b, char *string)
{
    BIO *bio = (BIO *) b;

    return((BIO_puts(bio, string) > 0) ? 1L : 0L);
}


/*
 * Attempt to load the private key for the given PERSONAL_CERT.
 * This sets the appropriate passphrase globals in order to
 * interact with the user correctly.
 */
static int
load_private_key(PERSONAL_CERT *pcert)
{
    if(!pcert->key){
    
    	/* Try empty password by default */
    	char	*password = "";
    
    	if(ps_global->smime
           && (ps_global->smime->need_passphrase
               || ps_global->smime->entered_passphrase)){
	    /* We've already been in here and discovered we need a different password */
	    
	    if(ps_global->smime->entered_passphrase)
    	      password = (char *) ps_global->smime->passphrase;	/* already entered */
	    else
	      return 0;
	}

        ERR_clear_error();

        if(!(pcert->key = load_key(pcert, password))){
            long err = ERR_get_error();

    	    /* Couldn't load key... */

	    if(ps_global->smime && ps_global->smime->entered_passphrase){

    	    	/* The user got the password wrong maybe? */

        	if((ERR_GET_LIB(err)==ERR_LIB_EVP && ERR_GET_REASON(err)==EVP_R_BAD_DECRYPT) ||
                	(ERR_GET_LIB(err)==ERR_LIB_PEM && ERR_GET_REASON(err)==PEM_R_BAD_DECRYPT))
                  q_status_message(SM_ORDER | SM_DING, 4, 4, _("Incorrect passphrase"));
        	else
		  q_status_message1(SM_ORDER, 4, 4, _("Couldn't read key: %s"),(char*)openssl_error_string());
    	    	
		/* This passphrase is no good; forget it */
		ps_global->smime->entered_passphrase = 0;
	    }
	    
	    /* Indicate to the UI that we need re-entry (see mailcmd.c:process_cmd())*/
	    if(ps_global->smime)
	      ps_global->smime->need_passphrase = 1;
	    
	    if(ps_global->smime){
		if(ps_global->smime->passphrase_emailaddr)
		  fs_give((void **) &ps_global->smime->passphrase_emailaddr);

		ps_global->smime->passphrase_emailaddr = get_x509_subject_email(pcert->cert);
	    }

            return 0;
        }
	else{
	    /* This key will be cached, so we won't be called again */
	    if(ps_global->smime){
		ps_global->smime->entered_passphrase = 0;
		ps_global->smime->need_passphrase = 0;
	    }
	}
	
	return 1;
    }
    
    return 0;
}


static void
setup_pkcs7_body_for_signature(BODY *b, char *description, char *type, char *filename)
{
    b->type = TYPEAPPLICATION;
    b->subtype = cpystr(type);
    b->encoding = ENCBINARY;
    b->description = cpystr(description);

    b->disposition.type = cpystr("attachment");
    set_parameter(&b->disposition.parameter, "filename", filename);

    set_parameter(&b->parameter, "name", filename);
}


/*
 * Look for a personal certificate matching the 
 * given address
 */
PERSONAL_CERT *
match_personal_cert_to_email(ADDRESS *a)
{
    PERSONAL_CERT   *pcert = NULL;
    char	buf[MAXPATH];
    char    	*email;

    if(!a || !a->mailbox || !a->host)
      return NULL;
    
    snprintf(buf, sizeof(buf), "%s@%s", a->mailbox, a->host);
    
    if(ps_global->smime){
	for(pcert=(PERSONAL_CERT *) ps_global->smime->personal_certs;
	    pcert;
	    pcert=pcert->next){
	
	    if(!pcert->cert)
	      continue;
	
	    email = get_x509_subject_email(pcert->cert);

	    if(email && strucmp(email,buf)==0){
		fs_give((void**) &email);
		break;
	    }

	    fs_give((void**) &email);
	}
    }
    
    return pcert;
}


/*
 * Look for a personal certificate matching the from
 * (or reply_to? in the given envelope)
 */
PERSONAL_CERT *
match_personal_cert(ENVELOPE *env)
{
    PERSONAL_CERT   *pcert;
    
    pcert = match_personal_cert_to_email(env->reply_to);
    if(!pcert)
      pcert = match_personal_cert_to_email(env->from);
        
    return pcert;
}


/*
 * Flatten the given body into its MIME representation.
 * Return the result in a BIO.
 */
static BIO *
body_to_bio(BODY *body)
{
    BIO *bio = NULL;
    int  len;

    bio = BIO_new(BIO_s_mem());
    if(!bio)
      return NULL;
    
    pine_encode_body(body); /* this attaches random boundary strings to multiparts */
    pine_write_body_header(body, rfc822_output_func, bio);
    pine_rfc822_output_body(body, rfc822_output_func, bio);

    /*
     * Now need to truncate by two characters since the above
     * appends CRLF.
     */
    if((len=BIO_ctrl_pending(bio)) > 1){
	BUF_MEM *biobuf = NULL;

	BIO_get_mem_ptr(bio, &biobuf);
	if(biobuf){
	    BUF_MEM_grow(biobuf, len-2);	/* remove CRLF */
	}
    }

    return bio;
} 


static BIO *
bio_from_store(STORE_S *store)
{
    BIO *ret = NULL;

    if(store && store->src == BioType && store->txt){
	ret = (BIO *) store->txt;
    }

    return(ret);
}


/*
 * Encrypt a message on the way out. Called from call_mailer in send.c
 * The body may be reallocated.
 */
int
encrypt_outgoing_message(METAENV *header, BODY **bodyP)
{
    PKCS7 *p7 = NULL;
    BIO	*in = NULL;
    BIO *out = NULL;
    const EVP_CIPHER *cipher = NULL;
    STACK_OF(X509) *encerts = NULL;
    STORE_S *outs = NULL;
    PINEFIELD	*pf;
    ADDRESS	*a;
    BODY	*body = *bodyP;
    BODY	*newBody = NULL;
    int		 result = 0;

    dprint((9, "encrypt_outgoing_message()"));
    smime_init();

    cipher = EVP_des_cbc();

    encerts = sk_X509_new_null();

    /* Look for a certificate for each of the recipients */
    for(pf = header->local; pf && pf->name; pf = pf->next)
        if(pf->type == Address && pf->rcptto && pf->addr && *pf->addr){
            for(a=*pf->addr; a; a=a->next){
                X509	*cert;
                char	 buf[MAXPATH];

                snprintf(buf, sizeof(buf), "%s@%s", a->mailbox, a->host);

                cert = get_cert_for(buf);
                if(cert)
                  sk_X509_push(encerts,cert);
                else{
                    q_status_message2(SM_ORDER, 1, 1,
                                      _("Unable to find certificate for <%s@%s>"),
				      a->mailbox, a->host);
                    goto end;
                }
            }
        }


    in = body_to_bio(body);

    p7 = PKCS7_encrypt(encerts, in, cipher, 0);

    outs = so_get(BioType, NULL, EDIT_ACCESS);
    out = bio_from_store(outs);

    i2d_PKCS7_bio(out, p7);
    (void) BIO_flush(out);

    so_seek(outs, 0, SEEK_SET);

    newBody = mail_newbody();

    newBody->type = TYPEAPPLICATION;
    newBody->subtype = cpystr("pkcs7-mime");
    newBody->encoding = ENCBINARY;

    newBody->disposition.type = cpystr("attachment");
    set_parameter(&newBody->disposition.parameter, "filename", "smime.p7m");

    newBody->description = cpystr("S/MIME Encrypted Message");
    set_parameter(&newBody->parameter, "smime-type", "enveloped-data");
    set_parameter(&newBody->parameter, "name", "smime.p7m");

    newBody->contents.text.data = (unsigned char *) outs;

    *bodyP = newBody;

    result = 1;

end:

    BIO_free(in);
    PKCS7_free(p7);
    sk_X509_pop_free(encerts, X509_free);

    dprint((9, "encrypt_outgoing_message returns %d", result));
    return result;
}


/*
 * Plonk the contents (mime headers and body) of the given
 * section of a message to a BIO_s_mem BIO object.
 */
static BIO *
raw_part_to_bio(long msgno, const char *section)
{
    unsigned long len;
    char	 *text;
    BIO          *bio;

    bio = BIO_new(BIO_s_mem());

    if(bio){

	(void) BIO_reset(bio);

        /* First grab headers of the chap */
        text = mail_fetch_mime(ps_global->mail_stream, msgno, (char*) section, &len, 0);

        if(text){
	    BIO_write(bio, text, len);

            /** Now grab actual body */
            text = mail_fetch_body (ps_global->mail_stream, msgno, (char*) section, &len, 0);
            if(text){
		BIO_write(bio, text, len);
	    }
	    else{
		BIO_free(bio);
		bio = NULL;
	    }

	}
	else{
	    BIO_free(bio);
	    bio = NULL;
	}
    }

    return bio;
}


/*
    Get (and decode) the body of the given section of msg
 */
static STORE_S*
get_part_contents(long msgno, const char *section)
{
    long len;
    gf_io_t     pc;
    STORE_S *store = NULL;
    char	*err;

    store = so_get(CharStar, NULL, EDIT_ACCESS);
    if(store){
        gf_set_so_writec(&pc,store);

        err = detach(ps_global->mail_stream, msgno, (char*) section, 0L, &len, pc, NULL, 0L);

        gf_clear_so_writec(store);

        so_seek(store, 0, SEEK_SET);

        if(err)
          so_give(&store);
    }

    return store;
}


static PKCS7 *
get_pkcs7_from_part(long msgno,const char *section)
{
    STORE_S *store = NULL;
    PKCS7   *p7 = NULL;
    BIO	   *in = NULL;

    store = get_part_contents(msgno, section);

    if(store){
	if(store->src == CharStar){
	    int len;

	    /*
	     * We're reaching inside the STORE_S structure. We should
	     * probably have a way to get the length, instead.
	     */
	    len = (int) (store->eod - store->dp);
	    in = BIO_new_mem_buf(store->txt, len);
	}
	else{				/* just copy it */
	    unsigned char c;

	    in = BIO_new(BIO_s_mem());
	    (void) BIO_reset(in);

	    so_seek(store, 0L, 0);
	    while(so_readc(&c, store)){
		BIO_write(in, &c, 1);
	    }
	}

        if(in){
/* dump_bio_to_file(in, "/tmp/decoded-signature"); */
            if((p7=d2i_PKCS7_bio(in,NULL)) == NULL){
		/* error */
	    }

	    BIO_free(in);
        }

	so_give(&store);
    }

    return p7;
}


/*
 * Try to verify a signature.
 * 
 * p7  - the pkcs7 object to verify
 * in  - the plain data to verify (NULL if not detached)
 * out - BIO to which to write the opaque data
 */
static int
do_signature_verify(PKCS7 *p7, BIO *in, BIO *out)
{
    STACK_OF(X509) *otherCerts = NULL;
    int	result;
    const char *data;
    long err;
    
#if 0
    dump_bio_to_file(in,"/tmp/verified-data");
#endif
    
    if(!s_cert_store){
	q_status_message(SM_ORDER | SM_DING, 2, 2,
		_("Couldn't verify S/MIME signature: No CA Certs were loaded"));

    	return -1;
    }

    result = PKCS7_verify(p7, otherCerts, s_cert_store, in, out, 0);
    
    if(result){
	q_status_message(SM_ORDER, 1, 1, _("S/MIME signature verified ok"));
    }
    else{
	err = ERR_peek_error_line_data(NULL, NULL, &data, NULL);

	if(out && err==ERR_PACK(ERR_LIB_PKCS7,PKCS7_F_PKCS7_VERIFY,PKCS7_R_CERTIFICATE_VERIFY_ERROR)){

	    /* Retry verification so we can get the plain text */
	    /* Might be better to reimplement PKCS7_verify here? */
	    
	    PKCS7_verify(p7, otherCerts, s_cert_store, in, out, PKCS7_NOVERIFY);
	}

	q_status_message1(SM_ORDER | SM_DING, 3, 3,
		_("Couldn't verify S/MIME signature: %s"), (char*) openssl_error_string());
    }

    /* now try to extract the certificates of any signers */
    {
        STACK_OF(X509)	*signers;
        int	i;

        signers = PKCS7_get0_signers(p7, NULL, 0);

        if(signers)
            for(i=0; i<sk_X509_num(signers); i++){
		char	*email;
                X509	*x = sk_X509_value(signers,i);
		X509	*cert;
		
                if(!x)
                  continue;

    	    	email = get_x509_subject_email(x);
    	    	
		if(email){
		    cert = get_cert_for(email);
		    if(cert)
		      X509_free(cert);
		    else
    	    	      save_cert_for(email, x);

		    fs_give((void**) &email);
    	    	}
            }

        sk_X509_free(signers);
    }

    return result;
}


void
free_smime_body_sparep(void **sparep)
{
    if(sparep && *sparep){
	PKCS7_free((PKCS7 *) (*sparep));
	*sparep = NULL;
    }
}


/*
 * Given a multipart body of type multipart/signed, attempt to verify it.
 * Returns non-zero if the body was changed.
 */
static int
do_detached_signature_verify(BODY *b, long msgno, char *section)
{
    PKCS7   *p7 = NULL;
    BIO	    *in = NULL;
    PART    *p;
    int	     result, modified_the_body = 0;
    char     newSec[100];
    char    *what_we_did;

    dprint((9, "do_detached_signature_verify(msgno=%ld type=%d subtype=%s section=%s)", msgno, b->type, b->subtype ? b->subtype : "NULL", (section && *section) ? section : (section != NULL) ? "Top" : "NULL"));
    smime_init();

    snprintf(newSec, sizeof(newSec), "%s%s1", section ? section : "", (section && *section) ? "." : "");
    in = raw_part_to_bio(msgno, newSec);

    if(in){

	snprintf(newSec, sizeof(newSec), "%s%s2", section ? section : "", (section && *section) ? "." : "");
        p7 = get_pkcs7_from_part(msgno, newSec);

        if(!p7)
          goto end;

    	result = do_signature_verify(p7, in, NULL);

        if(b->subtype)
	  fs_give((void**) &b->subtype);

        b->subtype = cpystr(OUR_PKCS7_ENCLOSURE_SUBTYPE);
        b->encoding = ENC8BIT;

    	if(b->description)
	  fs_give ((void**) &b->description);

   	what_we_did = result ? 	_("This message was cryptographically signed.") :
	    	    	    	_("This message was cryptographically signed but the signature could not be verified.");

	b->description = cpystr(what_we_did);

    	b->sparep = p7;
	p7 = NULL;

    	p = b->nested.part;
	
	/* p is signed plaintext */
	if(p && p->next)
	  mail_free_body_part(&p->next); /* hide the pkcs7 from the viewer */

	BIO_free(in);

	modified_the_body = 1;
    }

end:
    PKCS7_free(p7);

    return modified_the_body;
}


PERSONAL_CERT *
find_certificate_matching_recip_info(PKCS7_RECIP_INFO *ri)
{
    PERSONAL_CERT *x = NULL;

    if(ps_global->smime){
	for(x = (PERSONAL_CERT *) ps_global->smime->personal_certs; x; x=x->next){
	    X509 *mine;

	    mine = x->cert;

	    if(!X509_NAME_cmp(ri->issuer_and_serial->issuer,mine->cert_info->issuer) &&
		    !ASN1_INTEGER_cmp(ri->issuer_and_serial->serial,mine->cert_info->serialNumber)){
		break;
	    }
	}
    }
    
    return x;
}


static PERSONAL_CERT *
find_certificate_matching_pkcs7(PKCS7 *p7)
{
    int i;
    STACK_OF(PKCS7_RECIP_INFO) *recips;
    PERSONAL_CERT *x = NULL;

    recips = p7->d.enveloped->recipientinfo;

    for(i=0; i<sk_PKCS7_RECIP_INFO_num(recips); i++){
        PKCS7_RECIP_INFO	*ri;

        ri = sk_PKCS7_RECIP_INFO_value(recips, i);

        if((x=find_certificate_matching_recip_info(ri))!=0){
            break;
        }
    }
    
    return x;
}


/*
 * Try to decode (decrypt or verify a signature) a PKCS7 body
 * Returns non-zero if something was changed.
 */
static int
do_decoding(BODY *b, long msgno, const char *section)
{
    int modified_the_body = 0;
    BIO *out = NULL;
    PKCS7 *p7 = NULL;
    X509 *recip = NULL;
    EVP_PKEY *key = NULL;
    PERSONAL_CERT 	*pcert = NULL;
    char    *what_we_did = "";
    char     null[1];
    char     newSec[100];

    dprint((9, "do_decoding(msgno=%ld type=%d subtype=%s section=%s)", msgno, b->type, b->subtype ? b->subtype : "NULL", (section && *section) ? section : (section != NULL) ? "Top" : "NULL"));
    null[0] = '\0';
    smime_init();

    /*
     *	Extract binary data from part to an in-memory store
     */

    if(b->sparep){		/* already done */
    	p7 = (PKCS7*) b->sparep;
    }
    else{

	snprintf(newSec, sizeof(newSec), "%s%s1", section ? section : "", (section && *section) ? "." : "");
	p7 = get_pkcs7_from_part(msgno, newSec);
	if(!p7){
            q_status_message1(SM_ORDER, 2, 2, "Couldn't load PKCS7 object: %s",
			     (char*) openssl_error_string());
            goto end;
	}

    	/*
    	 * Save the PKCS7 object for later dealings by the user interface.
	 * It will be cleaned up when the body is garbage collected.
	 */
	b->sparep = p7;
    }

    if(PKCS7_type_is_signed(p7)){
    	int 	sigok;
	
	out = BIO_new(BIO_s_mem());
	(void) BIO_reset(out);
	BIO_puts(out, "MIME-Version: 1.0\r\n"); /* needed so rfc822_parse_msg_full believes it's MIME */

    	sigok = do_signature_verify(p7, NULL, out);
    	
	/* shouldn't really duplicate these messages */
   	what_we_did = sigok ? 	_("This message was cryptographically signed.") :
	    	    	    	_("This message was cryptographically signed but the signature could not be verified.");
	
	/* make sure it's null terminated */
	BIO_write(out, null, 1);
    }
    else if(!PKCS7_type_is_enveloped(p7)){
        q_status_message(SM_ORDER, 1, 1, "PKCS7 object not recognised.");
        goto end;
    }
    else{ /* It *is* enveloped */
	int decrypt_result;

	what_we_did = _("This message was encrypted.");

	/* now need to find a cert that can decrypt this */
	pcert = find_certificate_matching_pkcs7(p7);

	if(!pcert){
            q_status_message(SM_ORDER, 3, 3, _("Couldn't find the certificate needed to decrypt."));
            goto end;
	}

	recip = pcert->cert;

	if(!load_private_key(pcert)
	   && ps_global->smime
	   && ps_global->smime->need_passphrase
	   && !ps_global->smime->already_auto_asked){
	    /* Couldn't load key with blank password, ask user */
	    ps_global->smime->already_auto_asked = 1;
	    if(pith_opt_smime_get_passphrase){
	      (*pith_opt_smime_get_passphrase)();
	      load_private_key(pcert);
	    }
	}

	key = pcert->key;
	if(!key)	
    	  goto end;

	out = BIO_new(BIO_s_mem());
	(void) BIO_reset(out);
	BIO_puts(out, "MIME-Version: 1.0\r\n");

	decrypt_result = PKCS7_decrypt(p7, key, recip, out, 0);

	if(F_OFF(F_REMEMBER_SMIME_PASSPHRASE,ps_global))
	  forget_private_keys();

	if(!decrypt_result){
            q_status_message1(SM_ORDER, 1, 1, _("Error decrypting: %s"),
			      (char*) openssl_error_string());
            goto end;
	}

	BIO_write(out, null, 1);
    }

    /*
     * We've now produced a flattened MIME object in BIO out.
     * It needs to be turned back into a BODY.
     */

    if(out){
        BODY	 *body;
        ENVELOPE *env;
        char	 *h = NULL;
        char	 *bstart;
        STRING	  s;
	BUF_MEM  *bptr = NULL;

	BIO_get_mem_ptr(out, &bptr);
	if(bptr)
          h = bptr->data;

        /* look for start of body */
        bstart = strstr(h, "\r\n\r\n");

        if(!bstart){
            q_status_message(SM_ORDER, 3, 3, _("Encrypted data couldn't be parsed."));
     	}
	else{
            bstart += 4; /* skip over CRLF*2 */

            INIT(&s, mail_string, bstart, strlen(bstart));
            rfc822_parse_msg_full(&env, &body, h, bstart-h-2, &s, BADHOST, 0, 0);
            mail_free_envelope(&env); /* Don't care about this */

            /*
             * Now convert original body (application/pkcs7-mime)
             * to a multipart body with one sub-part (the decrypted body).
	     * Note that the sub-part may also be multipart!
             */

            b->type = TYPEMULTIPART;
            if(b->subtype)
	      fs_give((void**) &b->subtype);

    	    /*
    	     * This subtype is used in mailview.c to annotate the display of
	     * encrypted or signed messages. We know for sure then that it's a PKCS7
	     * part because the sparep field is set to the PKCS7 object (see above).
	     */
	    b->subtype = cpystr(OUR_PKCS7_ENCLOSURE_SUBTYPE);
            b->encoding = ENC8BIT;

    	    if(b->description)
	      fs_give((void**) &b->description);

	    b->description = cpystr(what_we_did);

            if(b->disposition.type)
	      fs_give((void **) &b->disposition.type);

            if(b->contents.text.data)
	      fs_give((void **) &b->contents.text.data);

    	    if(b->parameter)
	      mail_free_body_parameter(&b->parameter);

            /* Allocate mem for the sub-part, and copy over the contents of our parsed body */
            b->nested.part = fs_get(sizeof(PART));
            b->nested.part->body = *body;
            b->nested.part->next = NULL;

            fs_give((void**) &body);

            /*
             * IMPORTANT BIT: set the body->contents.text.data elements to contain the decrypted
             * data. Otherwise, it'll try to load it from the original data. Eek.
             */
            create_local_cache(bstart, &b->nested.part->body);

            modified_the_body = 1;
        }
    }

end:
    if(out)
      BIO_free(out);

    return modified_the_body;
}


/*
 * Recursively handle PKCS7 bodies in our message.
 *
 * Returns non-zero if some fiddling was done.
 */
static int
do_fiddle_smime_message(BODY *b, long msgno, char *section)
{
    int modified_the_body = 0;

    if(!b)
      return 0;

    dprint((9, "do_fiddle_smime_message(msgno=%ld type=%d subtype=%s section=%s)", msgno, b->type, b->subtype ? b->subtype : "NULL", (section && *section) ? section : (section != NULL) ? "Top" : "NULL"));

    if(is_pkcs7_body(b)){
    
        if(do_decoding(b, msgno, section)){
            /*
             *	b should now be a multipart message:
	     *   fiddle it too in case it's been multiply-encrypted!
             */

            /* fallthru */
            modified_the_body = 1;
        }
    }

    if(b->type==TYPEMULTIPART || MIME_MSG(b->type, b->subtype)){

        PART	*p;
        int		partNum;
        char	newSec[100];

	if(MIME_MULT_SIGNED(b->type, b->subtype)){


            /*
             * Ahah. We have a multipart signed entity.
	     *
	     * Multipart/signed
             *   part 1 (signed thing)
             *   part 2 (the pkcs7 signature)
	     *
	     * We're going to convert that to
	     *
	     * Multipart/OUR_PKCS7_ENCLOSURE_SUBTYPE
             *   part 1 (signed thing)
	     *   part 2 has been freed
	     *
	     * We also extract the signature from part 2 and save it
	     * in the multipart body->sparep, and we add a description
	     * in the multipart body->description.
	     *
	     *
	     * The results of a decrypted message will be similar. It
	     * will be
	     *
	     * Multipart/OUR_PKCS7_ENCLOSURE_SUBTYPE
             *   part 1 (decrypted thing)
             */

            modified_the_body += do_detached_signature_verify(b, msgno, section);
        }
	else if(MIME_MSG(b->type, b->subtype)){
	    modified_the_body += do_fiddle_smime_message(b->nested.msg->body, msgno, section);
	}
	else{

            for(p=b->nested.part,partNum=1; p; p=p->next,partNum++){

                /* Append part number to the section string */

                snprintf(newSec, sizeof(newSec), "%s%s%d", section, *section ? "." : "", partNum);

                modified_the_body += do_fiddle_smime_message(&p->body, msgno, newSec);
            }
        }
    }

    return modified_the_body;
}


/*
 * Fiddle a message in-place by decrypting/verifying S/MIME entities.
 * Returns non-zero if something was changed.
 */
int
fiddle_smime_message(BODY *b, long msgno)
{
    return do_fiddle_smime_message(b, msgno, "");
}


/********************************************************************************/


/*
 *  Output a string in a distinctive style
 */
void
gf_puts_uline(char *txt, gf_io_t pc)
{
    pc(TAG_EMBED); pc(TAG_BOLDON);
    gf_puts(txt, pc);
    pc(TAG_EMBED); pc(TAG_BOLDOFF);
}


/*
 * Sign a message. Called from call_mailer in send.c.
 *
 * This takes the header for the outgoing message as well as a pointer
 * to the current body (which may be reallocated).
 */
int
sign_outgoing_message(METAENV *header, BODY **bodyP, int dont_detach)
{
    STORE_S *outs = NULL;
    BODY    *body = *bodyP;
    BODY    *newBody = NULL;
    PART    *p1 = NULL;
    PART    *p2 = NULL;
    PERSONAL_CERT   *pcert;
    BIO *in = NULL;
    BIO *out = NULL;
    PKCS7   *p7 = NULL;
    int result = 0;
    int flags = dont_detach ? 0 : PKCS7_DETACHED;

    dprint((9, "sign_outgoing_message()"));

    smime_init();

    /* Look for a private key matching the sender address... */
    
    pcert = match_personal_cert(header->env);
    
    if(!pcert){
        q_status_message(SM_ORDER, 3, 3, _("Couldn't find the certificate needed to sign."));
	goto end;
    }
    
    if(!load_private_key(pcert) && ps_global->smime && ps_global->smime->need_passphrase){
    	/* Couldn't load key with blank password, try again */
	if(pith_opt_smime_get_passphrase){
	  (*pith_opt_smime_get_passphrase)();
	  load_private_key(pcert);
	}
    }
    
    if(!pcert->key)
      goto end;
    
    in = body_to_bio(body);

#ifdef notdef
    dump_bio_to_file(in,"/tmp/signed-data");
#endif

    p7 = PKCS7_sign(pcert->cert, pcert->key, NULL, in, flags);

    if(F_OFF(F_REMEMBER_SMIME_PASSPHRASE,ps_global))
      forget_private_keys();

    if(!p7){
        q_status_message(SM_ORDER, 1, 1, _("Error creating signed object."));
	goto end;
    }
   
    outs = so_get(BioType, NULL, EDIT_ACCESS);
    out = bio_from_store(outs);

    i2d_PKCS7_bio(out, p7);
    (void) BIO_flush(out);

    so_seek(outs, 0, SEEK_SET);
    
    if((flags&PKCS7_DETACHED)==0){
   
    	/* the simple case: the signed data is in the pkcs7 object */
    
	newBody = mail_newbody();
    	
	setup_pkcs7_body_for_signature(newBody, "S/MIME Cryptographically Signed Message", "pkcs7-mime", "smime.p7m");

    	newBody->contents.text.data = (unsigned char *) outs;
	*bodyP = newBody;

	result = 1;
    }
    else{
    
	/*
	 * OK.
    	 * We have to create a new body as follows:
	 *
	 * multipart/signed; blah blah blah
	 *      reference to existing body
	 *
	 *	pkcs7 object
	 */

	newBody = mail_newbody();

	newBody->type = TYPEMULTIPART;
	newBody->subtype = cpystr("signed");
	newBody->encoding = ENC7BIT;

	set_parameter(&newBody->parameter, "protocol", "application/pkcs7-signature");
	set_parameter(&newBody->parameter, "micalg", "sha1");

	p1 = mail_newbody_part();
	p2 = mail_newbody_part();

    	/*
    	 * This is nasty. We're just copying the body in here,
	 * but since our newBody is freed at the end of call_mailer,
	 * we mustn't let this body (the original one) be freed twice.
	 */
	p1->body = *body; /* ARRGH. This is special cased at the end of call_mailer */

	p1->next = p2;

	setup_pkcs7_body_for_signature(&p2->body, "S/MIME Cryptographic Signature", "pkcs7-signature", "smime.p7s");
    	p2->body.contents.text.data = (unsigned char *) outs;

    	newBody->nested.part = p1;

	*bodyP = newBody;
	
	result = 1;
    }

end:

    PKCS7_free(p7);
    BIO_free(in);

    dprint((9, "sign_outgoing_message returns %d", result));
    return result;
}


SMIME_STUFF_S *
new_smime_struct(void)
{
    SMIME_STUFF_S *ret = NULL;

    ret = (SMIME_STUFF_S *) fs_get(sizeof(*ret));
    memset((void *) ret, 0, sizeof(*ret));
    ret->publictype = Nada;

    return ret;
}


static void
free_smime_struct(SMIME_STUFF_S **smime)
{
    if(smime && *smime){
	if((*smime)->passphrase_emailaddr)
	  fs_give((void **) &(*smime)->passphrase_emailaddr);

	if((*smime)->publicpath)
	  fs_give((void **) &(*smime)->publicpath);

	if((*smime)->publiccertlist)
	  free_certlist(&(*smime)->publiccertlist);

	if((*smime)->publiccontent)
	  fs_give((void **) &(*smime)->publiccontent);

	if((*smime)->privatepath)
	  fs_give((void **) &(*smime)->privatepath);

	if((*smime)->personal_certs){
	    PERSONAL_CERT *pc;

	    pc = (PERSONAL_CERT *) (*smime)->personal_certs;
	    free_personal_certs(&pc);
	    (*smime)->personal_certs = NULL;
	}

	if((*smime)->privatecontent)
	  fs_give((void **) &(*smime)->privatecontent);

	if((*smime)->capath)
	  fs_give((void **) &(*smime)->capath);

	if((*smime)->cacontent)
	  fs_give((void **) &(*smime)->cacontent);

	fs_give((void **) smime);
    }
}

#endif /* SMIME */
