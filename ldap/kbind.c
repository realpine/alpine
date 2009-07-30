/*
 *  Copyright (c) 1993 Regents of the University of Michigan.
 *  All rights reserved.
 *
 *  kbind.c
 */

#ifndef lint 
static char copyright[] = "@(#) Copyright (c) 1993 Regents of the University of Michigan.\nAll rights reserved.\n";
#endif

#ifdef KERBEROS

#include <stdio.h>
#include <string.h>

#ifdef MACOS
#include <stdlib.h>
#include "macos.h"
#else /* MACOS */
#ifdef DOS
#include "msdos.h"
#endif /* DOS */
#include <krb.h>
#include <stdlib.h>
#if !defined(DOS) && !defined( _WIN32 )
#include <sys/types.h>
#endif /* !DOS && !_WIN32 */
#include <sys/time.h>
#include <sys/socket.h>
#endif /* MACOS */

#include "lber.h"
#include "ldap.h"
#include "ldap-int.h"



/*
 * ldap_kerberos_bind1 - initiate a bind to the ldap server using
 * kerberos authentication.  The dn is supplied.  It is assumed the user
 * already has a valid ticket granting ticket.  The msgid of the
 * request is returned on success (suitable for passing to ldap_result()),
 * -1 is returned if there's trouble.
 *
 * Example:
 *	ldap_kerberos_bind1( ld, "cn=manager, o=university of michigan, c=us" )
 */
int
ldap_kerberos_bind1( LDAP *ld, char *dn )
{
	BerElement	*ber;
	char		*cred;
	int		rc, credlen;
	char		*get_kerberosv4_credentials();
#ifdef STR_TRANSLATION
	int		str_translation_on;
#endif /* STR_TRANSLATION */

	/*
	 * The bind request looks like this:
	 *	BindRequest ::= SEQUENCE {
	 *		version		INTEGER,
	 *		name		DistinguishedName,
	 *		authentication	CHOICE {
	 *			krbv42ldap	[1] OCTET STRING
	 *			krbv42dsa	[2] OCTET STRING
	 *		}
	 *	}
	 * all wrapped up in an LDAPMessage sequence.
	 */

	Debug( LDAP_DEBUG_TRACE, "ldap_kerberos_bind1\n", 0, 0, 0 );

	if ( dn == NULL )
		dn = "";

	if ( (cred = get_kerberosv4_credentials( ld, dn, "ldapserver",
	    &credlen )) == NULL ) {
		return( -1 );	/* ld_errno should already be set */
	}

	/* create a message to send */
	if ( (ber = alloc_ber_with_options( ld )) == NULLBER ) {
		free( cred );
		return( -1 );
	}

#ifdef STR_TRANSLATION
	if (( str_translation_on = (( ber->ber_options &
	    LBER_TRANSLATE_STRINGS ) != 0 ))) {	/* turn translation off */
		ber->ber_options &= ~LBER_TRANSLATE_STRINGS;
	}
#endif /* STR_TRANSLATION */

	/* fill it in */
	rc = ber_printf( ber, "{it{isto}}", ++ld->ld_msgid, LDAP_REQ_BIND,
	    ld->ld_version, dn, LDAP_AUTH_KRBV41, cred, credlen );

#ifdef STR_TRANSLATION
	if ( str_translation_on ) {	/* restore translation */
		ber->ber_options |= LBER_TRANSLATE_STRINGS;
	}
#endif /* STR_TRANSLATION */

	if ( rc == -1 ) {
		free( cred );
		ber_free( ber, 1 );
		ld->ld_errno = LDAP_ENCODING_ERROR;
		return( -1 );
	}

	free( cred );

#ifndef NO_CACHE
	if ( ld->ld_cache != NULL ) {
		ldap_flush_cache( ld );
	}
#endif /* !NO_CACHE */

	/* send the message */
	return ( send_initial_request( ld, LDAP_REQ_BIND, dn, ber ));
}

int
ldap_kerberos_bind1_s( LDAP *ld, char *dn )
{
	int		msgid;
	LDAPMessage	*res;

	Debug( LDAP_DEBUG_TRACE, "ldap_kerberos_bind1_s\n", 0, 0, 0 );

	/* initiate the bind */
	if ( (msgid = ldap_kerberos_bind1( ld, dn )) == -1 )
		return( ld->ld_errno );

	/* wait for a result */
	if ( ldap_result( ld, ld->ld_msgid, 1, (struct timeval *) 0, &res )
	    == -1 ) {
		return( ld->ld_errno );	/* ldap_result sets ld_errno */
	}

	return( ldap_result2error( ld, res, 1 ) );
}

/*
 * ldap_kerberos_bind2 - initiate a bind to the X.500 server using
 * kerberos authentication.  The dn is supplied.  It is assumed the user
 * already has a valid ticket granting ticket.  The msgid of the
 * request is returned on success (suitable for passing to ldap_result()),
 * -1 is returned if there's trouble.
 *
 * Example:
 *	ldap_kerberos_bind2( ld, "cn=manager, o=university of michigan, c=us" )
 */
int
ldap_kerberos_bind2( LDAP *ld, char *dn )
{
	BerElement	*ber;
	char		*cred;
	int		rc, credlen;
	char		*get_kerberosv4_credentials();
#ifdef STR_TRANSLATION
	int		str_translation_on;
#endif /* STR_TRANSLATION */

	Debug( LDAP_DEBUG_TRACE, "ldap_kerberos_bind2\n", 0, 0, 0 );

	if ( dn == NULL )
		dn = "";

	if ( (cred = get_kerberosv4_credentials( ld, dn, "x500dsa", &credlen ))
	    == NULL ) {
		return( -1 );	/* ld_errno should already be set */
	}

	/* create a message to send */
	if ( (ber = alloc_ber_with_options( ld )) == NULLBER ) {
		free( cred );
		return( -1 );
	}

#ifdef STR_TRANSLATION
	if (( str_translation_on = (( ber->ber_options &
	    LBER_TRANSLATE_STRINGS ) != 0 ))) {	/* turn translation off */
		ber->ber_options &= ~LBER_TRANSLATE_STRINGS;
	}
#endif /* STR_TRANSLATION */

	/* fill it in */
	rc = ber_printf( ber, "{it{isto}}", ++ld->ld_msgid, LDAP_REQ_BIND,
	    ld->ld_version, dn, LDAP_AUTH_KRBV42, cred, credlen );


#ifdef STR_TRANSLATION
	if ( str_translation_on ) {	/* restore translation */
		ber->ber_options |= LBER_TRANSLATE_STRINGS;
	}
#endif /* STR_TRANSLATION */

	free( cred );

	if ( rc == -1 ) {
		ber_free( ber, 1 );
		ld->ld_errno = LDAP_ENCODING_ERROR;
		return( -1 );
	}

	/* send the message */
	return ( send_initial_request( ld, LDAP_REQ_BIND, dn, ber ));
}

/* synchronous bind to DSA using kerberos */
int
ldap_kerberos_bind2_s( LDAP *ld, char *dn )
{
	int		msgid;
	LDAPMessage	*res;

	Debug( LDAP_DEBUG_TRACE, "ldap_kerberos_bind2_s\n", 0, 0, 0 );

	/* initiate the bind */
	if ( (msgid = ldap_kerberos_bind2( ld, dn )) == -1 )
		return( ld->ld_errno );

	/* wait for a result */
	if ( ldap_result( ld, ld->ld_msgid, 1, (struct timeval *) 0, &res )
	    == -1 ) {
		return( ld->ld_errno );	/* ldap_result sets ld_errno */
	}

	return( ldap_result2error( ld, res, 1 ) );
}

/* synchronous bind to ldap and DSA using kerberos */
int
ldap_kerberos_bind_s( LDAP *ld, char *dn )
{
	int	err;

	Debug( LDAP_DEBUG_TRACE, "ldap_kerberos_bind_s\n", 0, 0, 0 );

	if ( (err = ldap_kerberos_bind1_s( ld, dn )) != LDAP_SUCCESS )
		return( err );

	return( ldap_kerberos_bind2_s( ld, dn ) );
}


#ifndef AUTHMAN  
#ifdef WINDOWS
#define FreeWindowsLibrary() FreeLibrary(instKrbv4DLL);instKrbv4DLL = (HINSTANCE)NULL;
#else /* WINDOWS */
#define FreeWindowsLibrary() /* do nothing */
#endif /* WINDOWS */
/*
 * get_kerberosv4_credentials - obtain kerberos v4 credentials for ldap.
 * The dn of the entry to which to bind is supplied.  It's assumed the
 * user already has a tgt.
 */

char *
get_kerberosv4_credentials( LDAP *ld, char *who, char *service, int *len )
{
	KTEXT_ST	ktxt;
	int		err;
	char		realm[REALM_SZ], *cred, *krbinstance;
#ifdef WINDOWS
	typedef int (_cdecl* pfn_krb_get_tf_realm) (char FAR *,char FAR *);
	typedef int (PASCAL* pfn_krb_mk_req) (KTEXT,LPSTR,LPSTR,LPSTR,long);
	typedef char * (_cdecl* pfn_tkt_string) ();

    	static HINSTANCE instKrbv4DLL = (HINSTANCE)NULL;
    	pfn_krb_get_tf_realm fptr_krb_get_tf_realm = NULL;
    	pfn_krb_mk_req fptr_krb_mk_req = NULL;
    	pfn_tkt_string fptr_tkt_string = NULL;
    	char* p_tkt_string = NULL;
    	KTEXT pKt = &ktxt;
#endif

	Debug( LDAP_DEBUG_TRACE, "get_kerberosv4_credentials\n", 0, 0, 0 );

#ifdef WINDOWS
	/*
	 * The goal is to gracefully survive the absence of krbv4win.dll
	 * and thus wshelper.dll.  User's won't be able to use kerberos, 
	 * but they shouldn't see a message box everytime libldap.dll loads.
	 */
    	if ( !instKrbv4DLL ) {
        	unsigned int prevMode = SetErrorMode( SEM_NOOPENFILEERRORBOX ); // don't whine at user if you can't find it
        	instKrbv4DLL = LoadLibrary("Krbv4win.DLL");
        	SetErrorMode( prevMode );
	
        	if ( instKrbv4DLL < HINSTANCE_ERROR ) { // can't find authlib
            		ld->ld_errno = LDAP_AUTH_UNKNOWN; 
            		return( NULL );
        	}
        
        	fptr_krb_get_tf_realm = (pfn_krb_get_tf_realm)GetProcAddress( instKrbv4DLL, "_krb_get_tf_realm" );
        	fptr_krb_mk_req = (pfn_krb_mk_req)GetProcAddress( instKrbv4DLL, "krb_mk_req" );
        	fptr_tkt_string = (pfn_tkt_string)GetProcAddress( instKrbv4DLL, "_tkt_string" );
	
        	// verify that we found all the routines we need
        	if (!(fptr_krb_mk_req && fptr_krb_get_tf_realm && fptr_tkt_string)) {
        		FreeWindowsLibrary();
            		ld->ld_errno = LDAP_AUTH_UNKNOWN; 
            		return( NULL );
        	}
        
    	}
	p_tkt_string = (fptr_tkt_string)( );
	if ( (err = (fptr_krb_get_tf_realm)( p_tkt_string, realm )) != KSUCCESS ) {
#else /* WINDOWS */
	if ( (err = krb_get_tf_realm( tkt_string(), realm )) != KSUCCESS ) {
#endif /* WINDOWS */
#ifndef NO_USERINTERFACE
		fprintf( stderr, "krb_get_tf_realm failed (%s)\n",
		    krb_err_txt[err] );
#endif /* NO_USERINTERFACE */
		ld->ld_errno = LDAP_INVALID_CREDENTIALS;
       		FreeWindowsLibrary();
		return( NULL );
	}

#ifdef LDAP_REFERRALS
	krbinstance = ld->ld_defconn->lconn_krbinstance;
#else /* LDAP_REFERRALS */
	krbinstance = ld->ld_host;
#endif /* LDAP_REFERRALS */

#ifdef WINDOWS
   if ( !krbinstance ) {	// if we don't know name of service host, no chance for service tickets
       	FreeWindowsLibrary();
        ld->ld_errno = LDAP_LOCAL_ERROR;
        WSASetLastError(WSANO_ADDRESS);
    	return( NULL );
    }
#endif /* WINDOWS */

#ifdef WINDOWS
	if ( (err = (fptr_krb_mk_req)( pKt, service, krbinstance, realm, 0 ))
#else /* WINDOWS */
	if ( (err = krb_mk_req( &ktxt, service, krbinstance, realm, 0 ))
#endif /* WINDOWS */
	    != KSUCCESS ) {
#ifndef NO_USERINTERFACE
		fprintf( stderr, "krb_mk_req failed (%s)\n", krb_err_txt[err] );
#endif /* NO_USERINTERFACE */
		ld->ld_errno = LDAP_INVALID_CREDENTIALS;
       		FreeWindowsLibrary();
		return( NULL );
	}

	if ( ( cred = malloc( ktxt.length )) == NULL ) {
		ld->ld_errno = LDAP_NO_MEMORY;
       		FreeWindowsLibrary();
		return( NULL );
	}

	*len = ktxt.length;
	memcpy( cred, ktxt.dat, ktxt.length );

   	FreeWindowsLibrary();
	return( cred );
}

#endif /* !AUTHMAN */
#endif /* KERBEROS */
