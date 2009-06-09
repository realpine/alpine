#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: mimedisp.c 165 2006-10-04 01:09:47Z jpf@u.washington.edu $";
#endif

/*
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

#include <system.h>
#include <general.h>
#include "mimedisp.h"


/*
 * Internal prototypes
 */
#ifdef	_WINDOWS
int	mswin_reg_viewer(char *mime_type, char *mime_ext,
			 char *cmd, int clen, int chk);
int	mswin_reg_mime_type(char *file_ext, char *mime_type);
int	mswin_reg_mime_ext(char *mime_type, char *file_ext);

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
#elif	defined(MAC_OS_X_VERSION_MIN_REQUIRED) && defined(MAC_OS_X_VERSION_10_3) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_3
    return 1;
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

    return(mswin_reg_viewer(mime_type, mime_ext, cmd, clen, chk));

#elif	OSX_TARGET

    /* 
     * if we wanted to be more like PC-Pine, we'd try checking
     * the mime-type of mime_ext and seeing if that matches
     * with our mime-type, which is safe for opening
     */
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

    return(mswin_reg_mime_type(file_ext, mime_type));

#elif	OSX_TARGET

#if defined(MAC_OS_X_VERSION_MIN_REQUIRED) && defined(MAC_OS_X_VERSION_10_3) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_3
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
#endif /* MAC_OS_X_VERSION_10_3 */

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

    return(mswin_reg_mime_ext(mime_type, file_ext));

#elif	OSX_TARGET

#if defined(MAC_OS_X_VERSION_MIN_REQUIRED) && defined(MAC_OS_X_VERSION_10_3) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_3
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
#endif /* MACOS_X_VERSION_10_3 */

#else
    return 0;
#endif /* OSX_TARGET */
}



#ifdef	_WINDOWS

/*
 * mswin_reg_viewer - 
 */
int
mswin_reg_viewer(char *mime_type, char *mime_ext, char *cmd, int clen, int chk)
{
    char  tmp[64], *ext;

    /*
     * Everything's based on the file extension.
     * So, sniff at registry's mapping for the given MIME type/subtype
     * and sniff at clues on how to launch it, or 
     * extension mapping and then
     * look for clues that some app will handle it.
     *
     */
    return(((mswin_reg_mime_ext(mime_type, ext = tmp)
	    || ((ext = mime_ext) && *mime_ext
		&& ((mswin_reg_mime_type(mime_ext, tmp)
		     && !strcmpi(mime_type, tmp))
		    || !strcmpi(mime_type, "application/octet-stream"))))
	    && MSWRShellCanOpen(ext, cmd, (DWORD) clen, 0) == TRUE)
	   || (chk && MSWRShellCanOpen(ext,cmd, (DWORD) clen, 1) == TRUE));
}


/*
 * given a file name extension, fill in the provided buf with its
 * corresponding MIME type
 */
int
mswin_reg_mime_type(char *file_ext, char *mime_type)
{
    char  buf[64];
    DWORD len = 64;

    if(file_ext[0] != '.'){
	*buf = '.';
	strncpy(buf + 1, file_ext, sizeof(buf)-1);
	buf[sizeof(buf)-1] = '\0';
	file_ext = buf;
    }

    return(MSWRPeek(HKEY_CLASSES_ROOT, file_ext, "content type",
		    mime_type, &len) == TRUE);
}


/*
 * given a mime_type, fill in the provided buf with its
 * corresponding file name extension
 */
int
mswin_reg_mime_ext(char *mime_type, char *file_ext)
{
    char   keybuf[128];
    DWORD  len = 64;

    if(strlen(mime_type) < 50){
	snprintf(keybuf, sizeof(keybuf), "MIME\\Database\\Content Type\\%s", mime_type);

	return(MSWRPeek(HKEY_CLASSES_ROOT, keybuf,
			"extension", file_ext, &len) == TRUE);
    }

    return(FALSE);
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
#if defined(MAC_OS_X_VERSION_MIN_REQUIRED) && defined(MAC_OS_X_VERSION_10_2) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_2
    CFStringRef str_ref = NULL, ret_str_ref = NULL;
    CFURLRef url_ref = NULL;

    if(&LSCopyApplicationForMIMEType == NULL)
      return 0;
    if((str_ref = CFStringCreateWithCString(NULL, mime_type, 
					kCFStringEncodingASCII)) == NULL)
      return 0;
    if(LSCopyApplicationForMIMEType(str_ref, kLSRolesViewer, &url_ref)
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
#endif /* MAC_OS_X_VERSION_10_2 */
    return rv;
}


/* returns: 1 if success, 0 if failure */
int
osx_build_mime_ext_cmd(mime_ext, cmd, cmdlen, sp_hndlp)
    char *mime_ext, *cmd;
    int   cmdlen, *sp_hndlp;
{
    int rv = 0;
#if defined(MAC_OS_X_VERSION_MIN_REQUIRED) && defined(MAC_OS_X_VERSION_10_2) && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_2
    CFStringRef str_ref = NULL, ret_str_ref = NULL;
    CFURLRef url_ref = NULL;

    if((str_ref = CFStringCreateWithCString(NULL, mime_ext,
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
