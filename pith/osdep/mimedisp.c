#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: mimedisp.c 942 2008-03-04 18:21:33Z hubert@u.washington.edu $";
#endif

/*
 * ========================================================================
 * Copyright 2006-2008 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */

#include <system.h>
#include <general.h>

#include "../../c-client/fs.h"

#ifdef _WINDOWS
#include "../../pico/osdep/mswin.h"
#endif

#include "mimedisp.h"
#include "../charconv/utf8.h"
#ifdef OSX_TARGET
#include <Security/AuthSession.h>
#endif


/*
 * Internal prototypes
 */
#ifdef	_WINDOWS
int	mswin_reg_viewer(LPTSTR mime_type, LPTSTR mime_ext,
			 char *cmd, int clen, int chk);
int	mswin_reg_mime_type(LPTSTR file_ext, LPTSTR mime_type, size_t mime_type_len);
int	mswin_reg_mime_ext(LPTSTR mime_type, LPTSTR file_ext, size_t file_ext_len);

#endif	/* WINDOWS */

#if	OSX_TARGET

int	osx_build_mime_type_cmd(char *, char *, int, int *);
int	osx_build_mime_ext_cmd(char *, char *, int, int *);

#endif /* OSX_TARGET */



/*
 * Determine if there is an OS-specific mechanism for accessing
 * MIME and extension data.  In the general *nix case this is all
 * done through mailcap and mime.types files.
 *
 * Returns: 0 if there is no support (most *nix cases)
 *          1 if there is support (Mac OS X, Windows)
 */
int
mime_os_specific_access(void)
{
#ifdef	_WINDOWS
    return 1;
#elif OSX_TARGET
# ifdef AVAILABLE_MAC_OS_X_VERSION_10_2_AND_LATER
    {
	/* 
	 * if we don't have the WidowSession then we should avoid using
	 * frameworks unless they call themselves daemon-safe
	 */
	SecuritySessionId session_id;
	SessionAttributeBits session_bits;

	if((SessionGetInfo(callerSecuritySession, &session_id, &session_bits)
	    == errSessionSuccess)
	   && session_bits & sessionHasGraphicAccess)
	  return 1;
	else
	  return 0;
    }
# else
    return 0;
# endif
#else
    return 0;
#endif
}


/*
 * Return the command based on either the mimetype or the file
 * extension.  Mime-type always takes precedence.
 *
 *   mime_type - mime-type of the file we're looking at
 *   mime_ext  - file extension given us by the mime data
 *   cmd       - buffer to copy the resulting command into
 *   chk       - whether or not we should check the file extension
 *
 *   Returns: 1 on success, 0 on failure
 */
int
mime_get_os_mimetype_command(char *mime_type, char *mime_ext, char *cmd,
			     int clen, int chk, int *sp_hndlp)
{
#ifdef	_WINDOWS
    int ret;
    LPTSTR mime_type_lpt, mime_ext_lpt;

    mime_type_lpt = utf8_to_lptstr(mime_type);
    mime_ext_lpt = utf8_to_lptstr(mime_ext);

    ret = mswin_reg_viewer(mime_type_lpt, mime_ext_lpt, cmd, clen, chk);

    if(mime_type_lpt)
      fs_give((void **) &mime_type_lpt);

    if(mime_ext_lpt)
      fs_give((void **) &mime_ext_lpt);

    return ret;

#elif	OSX_TARGET

    /* 
     * if we wanted to be more like PC-Pine, we'd try checking
     * the mime-type of mime_ext and seeing if that matches
     * with our mime-type, which is safe for opening
     */
    if(!mime_os_specific_access())
      return(0);

    /* don't want to use Mail or something for a part alpine is good at */
    if(!strucmp(mime_type, "message/rfc822"))
      return(0);

    return(osx_build_mime_type_cmd(mime_type, cmd, clen, sp_hndlp)
	   || (chk && mime_ext && *mime_ext &&
	       osx_build_mime_ext_cmd(mime_ext, cmd, clen, sp_hndlp)));
#else
    return 0;
#endif
}


/*
 * Given a file extension, return the mime-type if there is one
 *
 * Returns: 1 on success, 0 on failure
 */
int
mime_get_os_mimetype_from_ext(char *file_ext, char *mime_type, int mime_type_len)
{
#ifdef	_WINDOWS
    int ret;
    LPTSTR x, file_ext_lpt, mime_type_lpt;

    file_ext_lpt = utf8_to_lptstr(file_ext);

    if(file_ext_lpt){
	if(mime_type){
	    mime_type_lpt = (LPTSTR) fs_get(mime_type_len * sizeof(TCHAR));
	    mime_type_lpt[0] = '\0';
	}
	else
	  mime_type_lpt = NULL;
    }

    ret = mswin_reg_mime_type(file_ext_lpt, mime_type_lpt, (size_t) mime_type_len);

    /* convert answer back to UTF-8 */
    if(ret && mime_type_lpt && mime_type){
	char *u;

	u = lptstr_to_utf8(mime_type_lpt);
	if(u){
	    strncpy(mime_type, u, mime_type_len);
	    mime_type[mime_type_len-1] = '\0';
	    fs_give((void **) &u);
	}
    }

    if(file_ext_lpt)
      fs_give((void **) &file_ext_lpt);

    if(mime_type_lpt)
      fs_give((void **) &mime_type_lpt);

    return ret;

#elif	OSX_TARGET

    if(!mime_os_specific_access())
      return(0);
#ifdef AVAILABLE_MAC_OS_X_VERSION_10_3_AND_LATER
    CFStringRef mime_ref = NULL, type_id_ref = NULL, ext_ref = NULL;
    char buf[1024];

    if(!file_ext || !*file_ext)
      return 0;

    /* This for if we built on OS X >= 10.3 but run on < 10.3 */
    if(&UTTypeCreatePreferredIdentifierForTag == NULL)
      return 0;
    if((ext_ref = CFStringCreateWithCString(NULL, *file_ext == '.' ?
					    file_ext + 1 : file_ext,
				    kCFStringEncodingASCII)) == NULL)
      return 0;
    if((type_id_ref 
	= UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension,
						ext_ref, NULL)) == NULL)
      return 0;

    if((mime_ref = UTTypeCopyPreferredTagWithClass(type_id_ref,
				   kUTTagClassMIMEType)) == NULL)
      return 0;
    if(CFStringGetCString(mime_ref, mime_type,
			   (CFIndex)mime_type_len - 1,
			   kCFStringEncodingASCII) == false)
      return 0;

    mime_type[mime_type_len - 1] = '\0';

    return 1;
#else
    return 0;
#endif /* AVAILABLE_MAC_OS_X_VERSION_10_3_AND_LATER */

#else
    return 0;
#endif /* OSX_TARGET */
}


/*
 * Given a mime-type, return the file extension if there is one
 *
 * Returns: 1 on success, 0 on failure
 */
int
mime_get_os_ext_from_mimetype(char *mime_type, char *file_ext, int file_ext_len)
{
#ifdef	_WINDOWS
    int ret;
    LPTSTR x, mime_type_lpt, file_ext_lpt;

    mime_type_lpt = utf8_to_lptstr(mime_type);

    if(mime_type_lpt){
	if(file_ext){
	    file_ext_lpt = (LPTSTR) fs_get(file_ext_len * sizeof(TCHAR));
	    file_ext_lpt[0] = '\0';
	}
	else
	  file_ext_lpt = NULL;
    }

    ret = mswin_reg_mime_ext(mime_type_lpt, file_ext_lpt, (size_t) file_ext_len);

    /* convert answer back to UTF-8 */
    if(ret && file_ext_lpt && file_ext){
	char *u;

	u = lptstr_to_utf8(file_ext_lpt);
	if(u){
	    strncpy(file_ext, u, file_ext_len);
	    file_ext[file_ext_len-1] = '\0';
	    fs_give((void **) &u);
	}
    }

    if(mime_type_lpt)
      fs_give((void **) &mime_type_lpt);

    if(file_ext_lpt)
      fs_give((void **) &file_ext_lpt);

    return ret;

#elif	OSX_TARGET

    if(!mime_os_specific_access())
      return(0);
#ifdef AVAILABLE_MAC_OS_X_VERSION_10_3_AND_LATER
    CFStringRef mime_ref = NULL, type_id_ref = NULL, ext_ref = NULL;

    if(!mime_type || !*mime_type)
      return 0;
    /* This for if we built on OS X >= 10.3 but run on < 10.3 */
    if(&UTTypeCreatePreferredIdentifierForTag == NULL)
      return 0;
    if((mime_ref = CFStringCreateWithCString(NULL, mime_type,
				    kCFStringEncodingASCII)) == NULL)
      return 0;
    if((type_id_ref 
	= UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType,
						mime_ref, NULL)) == NULL)
      return 0;

    if((ext_ref = UTTypeCopyPreferredTagWithClass(type_id_ref,
				   kUTTagClassFilenameExtension)) == NULL)
      return 0;
    if((CFStringGetCString(ext_ref, file_ext, (CFIndex)file_ext_len - 1,
			   kCFStringEncodingASCII)) == false)
      return 0;

    file_ext[file_ext_len - 1] = '\0';

    return 1;
#else
    return 0;
#endif /* AVAILABLE_MAC_OS_X_VERSION_10_3_AND_LATER */

#else
    return 0;
#endif /* OSX_TARGET */
}



#ifdef	_WINDOWS

/*
 * mswin_reg_viewer - 
 */
int
mswin_reg_viewer(LPTSTR mime_type, LPTSTR mime_ext, char *cmd, int clen, int chk)
{
    TCHAR  tmp[256];
    LPTSTR ext;

    tmp[0] = '\0';

    /*
     * Everything's based on the file extension.
     * So, sniff at registry's mapping for the given MIME type/subtype
     * and sniff at clues on how to launch it, or 
     * extension mapping and then
     * look for clues that some app will handle it.
     *
     */
    return(((mswin_reg_mime_ext(mime_type, ext = tmp, sizeof(tmp)/sizeof(TCHAR))
	    || ((ext = mime_ext) && *mime_ext
		&& ((mswin_reg_mime_type(mime_ext, tmp, sizeof(tmp)/sizeof(TCHAR))
		     && mime_type && !_tcsicmp(mime_type, tmp))
		    || mime_type && !_tcsicmp(mime_type, TEXT("application/octet-stream")))))
	    && MSWRShellCanOpen(ext, cmd, clen, 0) == TRUE)
	   || (chk && MSWRShellCanOpen(ext, cmd, clen, 1) == TRUE));
}


/*
 * given a file name extension, fill in the provided buf with its
 * corresponding MIME type
 */
int
mswin_reg_mime_type(LPTSTR file_ext, LPTSTR mime_type, size_t mime_type_len)
{
    TCHAR buf[64];
    DWORD len = mime_type_len;

    if(file_ext[0] != '.'){
	*buf = '.';
	_tcsncpy(buf + 1, file_ext, sizeof(buf)/sizeof(TCHAR)-1);
	buf[sizeof(buf)/sizeof(TCHAR)-1] = '\0';
	file_ext = buf;
    }

    return(MSWRPeek(HKEY_CLASSES_ROOT, file_ext, TEXT("content type"),
		    mime_type, &len) == TRUE);
}


/*
 * given a mime_type, fill in the provided buf with its
 * corresponding file name extension
 */
int
mswin_reg_mime_ext(LPTSTR mime_type, LPTSTR file_ext, size_t file_ext_len)
{
    TCHAR   keybuf[128];
    DWORD  len = file_ext_len;

    if(mime_type && _tcslen(mime_type) < 50){
	_sntprintf(keybuf, sizeof(keybuf), TEXT("MIME\\Database\\Content Type\\%s"), mime_type);
	return(MSWRPeek(HKEY_CLASSES_ROOT, keybuf,
			TEXT("extension"), file_ext, &len) == TRUE);
    }

    return FALSE;
}
#endif /* _WINDOWS */



#if	OSX_TARGET
/* returns: 1 if success, 0 if failure */
int
osx_build_mime_type_cmd(mime_type, cmd, cmdlen, sp_hndlp)
    char *mime_type, *cmd;
    int   cmdlen, *sp_hndlp;
{
    int rv = 0;

    if(!mime_os_specific_access())
      return(0);
#ifdef AVAILABLE_MAC_OS_X_VERSION_10_2_AND_LATER
    CFStringRef str_ref = NULL, ret_str_ref = NULL;
    CFURLRef url_ref = NULL;

    if(&LSCopyApplicationForMIMEType == NULL)
      return 0;
    if((str_ref = CFStringCreateWithCString(NULL, mime_type, 
					kCFStringEncodingASCII)) == NULL)
      return 0;
    if(LSCopyApplicationForMIMEType(str_ref, kLSRolesAll, &url_ref)
       != kLSApplicationNotFoundErr){
	if((ret_str_ref = CFURLGetString(url_ref)) == NULL)
	  return 0;
	if(CFStringGetCString(ret_str_ref, cmd, (CFIndex)cmdlen,
			      kCFStringEncodingASCII) == false)
	  return 0;
	if(sp_hndlp)
	  *sp_hndlp = 1;
	rv = 1;
	if(url_ref)
	  CFRelease(url_ref);
    }
#endif /* AVAILABLE_MAC_OS_X_VERSION_10_2_AND_LATER */
    return rv;
}


/* returns: 1 if success, 0 if failure */
int
osx_build_mime_ext_cmd(mime_ext, cmd, cmdlen, sp_hndlp)
    char *mime_ext, *cmd;
    int   cmdlen, *sp_hndlp;
{
    int rv = 0;

    if(!mime_os_specific_access())
      return 0;

#ifdef AVAILABLE_MAC_OS_X_VERSION_10_2_AND_LATER
    CFStringRef str_ref = NULL, ret_str_ref = NULL;
    CFURLRef url_ref = NULL;

    if((str_ref = CFStringCreateWithCString(NULL, (*mime_ext) == '.' 
					    ? mime_ext+1 : mime_ext,
					kCFStringEncodingASCII)) == NULL)
      return 0;
    if(LSGetApplicationForInfo(kLSUnknownType, kLSUnknownCreator,
			       str_ref, kLSRolesAll, NULL, &url_ref) 
       != kLSApplicationNotFoundErr){
	if((ret_str_ref = CFURLGetString(url_ref)) == NULL)
	  return 0;
	if(CFStringGetCString(ret_str_ref, cmd, (CFIndex)cmdlen,
			      kCFStringEncodingASCII) == false)
	  return 0;
	if(sp_hndlp)
	  *sp_hndlp = 1;
	rv = 1;
	if(url_ref)
	  CFRelease(url_ref);
    }
#endif
    return rv;
}
#endif /* OSX_TARGET */
