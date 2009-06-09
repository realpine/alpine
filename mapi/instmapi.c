
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

/* installmapi.c : installs the Alpine version of mapi32.dll 
 * into the directory from which this program is run.  The PC-Pine
 * version is pmapi32.dll, and must be in the same directory as the
 * program being run.  Note that we will run into trouble if we want
 * to name our dll mapi32.dll, unless we're putting it in the system
 * directory
 */

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <io.h>
#include <string.h>
#include <direct.h>

#define FIXREG 1

/* this program consists of two parts: 
 * (1) Updating the registry, 
 * most particulary the string of DLLPath in the key
 * HKLM\Software\Clients\PC-Pine.
 * (2) Deciding whether or not to copy pmapi32.dll into the system
 * directory.  Starting with Windows 2000, Internet Explorer 5.0
 * or Outlook 2000, mapi32.dll started to be a stub library, loading
 * whichever mapi32.dll was defined in the registry (see (1)).  We
 * don't want to overwrite mapi32.dll if it is the stub library, but
 * if the stub library isn't installed, then we want to install our 
 * own version of mapi32.dll.  Since the method for detecting the
 * stub library isn't very well documented (as of Feb 17, 2000), we
 * should use a liberal policy of whether or not to copy our mapi
 * into the system directory. Here are the criteria that determine
 * that the stub library is correctly installed:
 *  i) Existence of mapistub.dll in the system directory
 *  ii) Existence of the exported function FixMAPI() in mapi32.dll
 * If either of these two conditions fail, then we'll copy into the
 * system directory (provided we have correct permissions for it).
 *
 * Jeff Franklin
 * University of Washington
 */ 

int check_defaults(int, int);
int check_url_commands(int);

#define CD_MAILER 1
#define CD_NEWS   2

#define CUC_MAILTO 1
#define CUC_NEWS   2
#define CUC_NNTP   3

int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
    char dir[1000], filename[1024], mapifile[1024],
      buffer[3*1024], buffer2[3*1024];
    unsigned long bufflen = 3*1024;
    DWORD dtype;
    int copy = 0, success = 0, silent = 0;
    FARPROC proc;
    HKEY hKey;
    HINSTANCE hDll;
    LPSTR p, pp;

    for(pp = p = lpCmdLine; *(p-1); p++){
	if(*p == ' ' || *p == '\t' || *p == '\0'){
	    if(strncmp("-silent", pp, strlen("-silent")) == 0)
	      silent = 1;
	    if(!*p)
	      break;
	    pp = p + 1;
	}
    } 
   

    /* This is the first part of writing the registry with our values */
    _getcwd(dir, 1000);
    sprintf(filename, "%s%s", dir, dir[strlen(dir)-1] == '\\' ?
	    "pmapi32.dll": "\\pmapi32.dll");
    if(_access(filename, 00) == -1){
      sprintf(buffer, 
	  "pmapi32.dll for PC-Pine not found in %s. Install not completed.", 
	      dir);
      MessageBox(NULL, buffer, "instmapi.exe", 
		 MB_OK|MB_ICONERROR);
      return 0;
    }

    if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Clients\\Mail",
		    0, KEY_READ, &hKey) == ERROR_SUCCESS){
      RegCloseKey(hKey);
      if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Clients\\Mail",
		      0, KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS){
	MessageBox(NULL, 
		   "Cannot install pmapi32.dll. Please make sure that you have permissions to update your registry.",
		   "instmapi.exe", 
		   MB_OK|MB_ICONERROR);
	return 0;
      }
    }
    else {
      MessageBox(NULL, 
		 "Cannot install pmapi32.dll. Mailer information not found in registry.",
		 "instmapi.exe", 
		 MB_OK|MB_ICONERROR);
      return 0;
    }

#ifdef FIXREG
    /*
     * I want to take this out next release because it is only intended to fix
     * problems that pine (<=4.43) set in the registry.
     */
    if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Clients\\Mail\\PC-Pine\\Protocols\\Mailto",
		    0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS){
	RegSetValueEx(hKey, "URL Protocol", 0, REG_SZ, "", 1);
	RegCloseKey(hKey);
    }
    if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Clients\\News\\PC-Pine\\Protocols\\news",
		    0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS){
	RegSetValueEx(hKey, "URL Protocol", 0, REG_SZ, "", 1);
	RegCloseKey(hKey);
    }
    if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Clients\\News\\PC-Pine\\Protocols\\nntp",
		    0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS){
	RegSetValueEx(hKey, "URL Protocol", 0, REG_SZ, "", 1);
	RegCloseKey(hKey);
    }
    if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Clients\\News\\PC-Pine\\shell\\open\\command",
		    0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS){
	char *tp;

	buffer[0] = '\0';
	bufflen = 1024*3;
	RegQueryValueEx(hKey, "", 0, &dtype, buffer, &bufflen);
	tp = buffer + strlen(buffer) - strlen(" -url news:%1");
	if((tp - buffer > 0) && (tp - buffer < (int)bufflen)
	   && (strcmp(tp, " -url news:%1") == 0)){
	    *tp = '\0';
	    RegSetValueEx(hKey, "", 0, dtype, buffer, strlen(buffer));
	}
	RegCloseKey(hKey);
    }
#endif

    if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Clients\\Mail\\PC-Pine",
		    0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS){
      if(RegSetValueEx(hKey, "DLLPath", 0, REG_SZ, 
		       filename, strlen(filename) + 1) != ERROR_SUCCESS){
	MessageBox(NULL, 
		   "Could not update registry. Install not completed.",
		   "instmapi.exe", 
		   MB_OK|MB_ICONERROR);
	RegCloseKey(hKey);
	return 0;
      }
      else
	RegCloseKey(hKey);
    }
    else{
      MessageBox(NULL,
		 "No entry found for PC-Pine.  Try running PC-Pine, and then run this program", 
		 "instmapi.exe",
		 MB_OK|MB_ICONERROR);
      return 0;
    }
    if(check_defaults(CD_MAILER, silent))
      MessageBox(NULL,
		 "Default mailer could not be set",
		 "instmapi.exe",
		 MB_OK|MB_ICONERROR);
    if(check_defaults(CD_NEWS, silent))
      MessageBox(NULL,
		 "Default newsreader could not be set",
		 "instmapi.exe",
		 MB_OK|MB_ICONERROR);

    /* This next part determines whether or not we should write our mapi
     * into the system directory
     */
    if(GetSystemDirectory(dir, 1000)){
      sprintf(mapifile, "%s%s", dir, dir[strlen(dir)-1] == '\\' ? 
	      "mapi32.dll" : "\\mapi32.dll");

      hDll = LoadLibrary(mapifile);
      if(hDll){
	if(proc = GetProcAddress(hDll,"GetPCPineVersion")){
	  sprintf(buffer2, "pmapi32.dll exists in %s as mapi32.dll", 
		  dir);
	  MessageBox(NULL, buffer2,
		     "instmapi.exe", 
		     MB_APPLMODAL | MB_ICONINFORMATION | MB_OK);
	  success = 1;
	}
	if(!success){
	  sprintf(buffer, "%s%s", dir, dir[strlen(dir)-1] == '\\' ? 
		  "mapistub.dll" : "\\mapistub.dll");
	  if(_access(buffer, 00) != 0)
	    copy = 1;
	  if(!copy){
	    proc = GetProcAddress(hDll, "FixMAPI");
	    if(!proc){
	      copy = 1;
	    }
	    else 
	      success = 1;
	  }
	}
	FreeLibrary(hDll);
      }
      else
	copy = 1;
      if(copy){
	sprintf(buffer2, "%s%s", dir, dir[strlen(dir)-1] == '\\' ? 
		"mapi32x.dll" : "\\mapi32x.dll");
	CopyFile(mapifile, buffer2, 0);
	if(CopyFile(filename, mapifile, 0)){
	  sprintf(buffer2, "pmapi32.dll has been copied to %s", 
		  mapifile);
	  MessageBox(NULL, buffer2, "instmapi.exe",
		     MB_APPLMODAL | MB_ICONINFORMATION | MB_OK);
	  success = 1;
	}
	else{
	  sprintf(buffer2, "pmapi32.dll could not be copied to %s", 
		  mapifile);
	  MessageBox(NULL, buffer2, "instmapi.exe",
		     MB_APPLMODAL | MB_ICONINFORMATION | MB_OK);
	}
      }   
    }

    if(success && !silent){
      sprintf(buffer2, 
	      "Installation of pmapi32.dll was successfully completed", 
	      buffer);
      MessageBox(NULL, buffer2, "instmapi.exe",
		 MB_APPLMODAL | MB_ICONINFORMATION | MB_OK);
    }
    else if(!success){
      sprintf(buffer2, 
	 "Installation of pmapi32.dll may not have successfully completed",
	      buffer);
      MessageBox(NULL, buffer2, "instmapi.exe",
		 MB_APPLMODAL | MB_ICONINFORMATION | MB_OK);
    }
    return 0;
}

int
check_defaults(wdef, silent)
    int wdef, silent;
{
    HKEY hKey;
    DWORD dtype;
    char buffer[1024*3];
    unsigned long bufflen = 1024*3;
    int ret;

    if(wdef != CD_MAILER && wdef != CD_NEWS)
      return(1);
    if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, wdef == CD_MAILER 
		    ? "SOFTWARE\\Clients\\Mail" : "SOFTWARE\\Clients\\News",
		    0, KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS)
      return(1);
    bufflen = 1024*3;
    RegQueryValueEx(hKey, "", 0, &dtype, buffer, &bufflen);
    if(strcmp(buffer, "PC-Pine") == 0){
	RegCloseKey(hKey);
	return(0);
    }
    ret = silent ? IDYES
      : MessageBox (NULL, wdef == CD_MAILER
		    ? "Do you want to make PC-Pine your default mailer?"
		    : "Do you want to make PC-Pine your default newsreader?",
		    "instmapi.exe", 
		    MB_APPLMODAL | MB_ICONQUESTION | MB_YESNO);
    if(ret == IDYES){
	strcpy(buffer, "PC-Pine");
	bufflen = strlen(buffer)+1;
	if(RegSetValueEx(hKey, "", 0, 
			 REG_SZ, buffer, bufflen) != ERROR_SUCCESS){
	    RegCloseKey(hKey);
	    return(1);
	}
	RegCloseKey(hKey);
	if(wdef == CD_MAILER){
	  if(check_url_commands(CUC_MAILTO))
	    return(1);
	}
	else if(wdef == CD_NEWS){
	    if(check_url_commands(CUC_NEWS)
	       || check_url_commands(CUC_NNTP))
	      return(1);
	}
    }
    return(0);
}

int
check_url_commands(wdef)
    int wdef;
{
    HKEY hKey, hKeyCP;
    DWORD dtype;
    char buffer[1024*3];
    unsigned long bufflen = 1024*3;

    if(wdef != CUC_MAILTO && wdef != CUC_NEWS && wdef != CUC_NNTP)
      return(1);
    if(RegOpenKeyEx(HKEY_CLASSES_ROOT, wdef == CUC_MAILTO
		    ? "mailto\\shell\\open\\command"
		    : (wdef == CUC_NEWS ? "news\\shell\\open\\command"
		       : "nntp\\shell\\open\\command"),
		    0, KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS)
      return(1);
    if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, wdef == CUC_MAILTO
		    ? "SOFTWARE\\Clients\\Mail\\PC-Pine\\Protocols\\Mailto\\shell\\open\\command"
		    : (wdef == CUC_NEWS ? "SOFTWARE\\Clients\\News\\PC-Pine\\Protocols\\news\\shell\\open\\command"
		       : "SOFTWARE\\Clients\\News\\PC-Pine\\Protocols\\nntp\\shell\\open\\command"),
		    0, KEY_ALL_ACCESS, &hKeyCP) != ERROR_SUCCESS){
	RegCloseKey(hKey);
	return(1);
    }
    buffer[0] = '\0';
    bufflen = 1024*3;
    if(RegQueryValueEx(hKeyCP, "", 0, &dtype, 
		       buffer, &bufflen) != ERROR_SUCCESS
       || RegSetValueEx(hKey, "", 0, REG_SZ, buffer,
			bufflen) != ERROR_SUCCESS){
	RegCloseKey(hKey);
	RegCloseKey(hKeyCP);
	return(1);
    }
    RegCloseKey(hKey);
    RegCloseKey(hKeyCP);
    if(RegOpenKeyEx(HKEY_CLASSES_ROOT, wdef == CUC_MAILTO
		    ? "mailto\\DefaultIcon"
		    : (wdef == CUC_NEWS ? "news\\DefaultIcon"
		       : "nntp\\DefaultIcon"),
		    0, KEY_ALL_ACCESS, 
		    &hKey) != ERROR_SUCCESS)
      return(1);
    if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, wdef == CUC_MAILTO
		    ? "SOFTWARE\\Clients\\Mail\\PC-Pine\\Protocols\\Mailto\\DefaultIcon"
		    : (wdef == CUC_NEWS ? "SOFTWARE\\Clients\\News\\PC-Pine\\Protocols\\news\\DefaultIcon"
		       : "SOFTWARE\\Clients\\News\\PC-Pine\\Protocols\\nntp\\DefaultIcon"),
		    0, KEY_ALL_ACCESS, &hKeyCP) != ERROR_SUCCESS){
	RegCloseKey(hKey);
	return(1);
    }
    buffer[0] = '\0';
    bufflen = 1024*3;
    if(RegQueryValueEx(hKeyCP, "", 0, &dtype, buffer, &bufflen) != ERROR_SUCCESS 
       || RegSetValueEx(hKey, "", 0, REG_SZ, buffer, bufflen) != ERROR_SUCCESS){
	RegCloseKey(hKey);
	RegCloseKey(hKeyCP);
	return(1);
    }
    RegCloseKey(hKey);
    RegCloseKey(hKeyCP);

    if(RegOpenKeyEx(HKEY_CLASSES_ROOT, wdef == CUC_MAILTO
		    ? "mailto"
		    : (wdef == CUC_NEWS ? "news"
		       : "nntp"),
		    0, KEY_ALL_ACCESS, 
		    &hKey) != ERROR_SUCCESS)
      return(1);
    if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, wdef == CUC_MAILTO
		    ? "SOFTWARE\\Clients\\Mail\\PC-Pine\\Protocols\\Mailto"
		    : (wdef == CUC_NEWS ? "SOFTWARE\\Clients\\News\\PC-Pine\\Protocols\\news"
		       : "SOFTWARE\\Clients\\News\\PC-Pine\\Protocols\\nntp"),
		    0, KEY_ALL_ACCESS, &hKeyCP) != ERROR_SUCCESS){
	RegCloseKey(hKey);
	return(1);
    }
    buffer[0] = '\0';
    bufflen = 1024*3;
    if(RegQueryValueEx(hKeyCP, "URL Protocol", 0, &dtype, buffer, &bufflen) != ERROR_SUCCESS 
       || RegSetValueEx(hKey, "URL Protocol", 0, REG_SZ, buffer, bufflen) != ERROR_SUCCESS){
	RegCloseKey(hKey);
	RegCloseKey(hKeyCP);
	return(1);
    }
    RegCloseKey(hKey);
    RegCloseKey(hKeyCP);

    return(0);
}
