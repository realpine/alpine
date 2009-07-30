#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: execview.c 942 2008-03-04 18:21:33Z hubert@u.washington.edu $";
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

#include "../../c-client/mail.h"	/* for MAILSTREAM and friends */
#include "../../c-client/osdep.h"
#include "../../c-client/rfc822.h"	/* for soutr_t and such */
#include "../../c-client/misc.h"	/* for cpystr proto */
#include "../../c-client/utf8.h"	/* for CHARSET and such*/
#include "../../c-client/imap4r1.h"

#include "../../pith/debug.h"

#include "../../pith/osdep/temp_nam.h"
#include "../../pith/osdep/color.h"
#include "../../pith/osdep/mimedisp.h"

#include "../../pith/charconv/utf8.h"
#include "../../pith/charconv/filesys.h"

#include "../pith/mailcap.h"

#include "../status.h"
#include "../radio.h"
#include "../signal.h"
#include "../../pico/estruct.h"
#include "../../pico/pico.h"
#include "../mailview.h"
#include "termin.gen.h"

#ifdef _WINDOWS
#include "../../pico/osdep/mswin.h"
#endif

/* Useful structures */
#if	OSX_TARGET
typedef struct _execview_event_data_s {
    int done;
    ProcessSerialNumber pid;
    int set_pid;
} EXEC_EVENT_DATA_S;
#endif


/* internal prototypes */
#if	OSX_TARGET
pascal OSStatus osx_launch_app_callback(EventHandlerCallRef,
					EventRef, void *);
int		install_app_launch_cb(void *);
void		osx_launch_special_handling(MCAP_CMD_S *, char *);
#endif



/* ----------------------------------------------------------------------
   Execute the given mailcap command

  Args: cmd           -- the command to execute
	image_file    -- the file the data is in
	needsterminal -- does this command want to take over the terminal?
  ----*/
void
exec_mailcap_cmd(MCAP_CMD_S *mc_cmd, char *image_file, int needsterminal)
{
#ifdef _WINDOWS
    STARTUPINFO		start_info;
    PROCESS_INFORMATION	proc_info;
    WINHAND		childProcess;
    int			success = 0;
    char               *cmd;
    LPTSTR              image_file_lpt = NULL;
    LPTSTR              cmd_lpt = NULL;

    /* no special handling yet, but could be used to replace '*' hack */
    if(mc_cmd)
      cmd = mc_cmd->command;
    else
      return;

    dprint((9, "run_viewer: command=%s\n", cmd ? cmd : "?")) ;

    if(image_file)
      image_file_lpt = utf8_to_lptstr(image_file);

    /* Set to READONLY so the viewer can't try to edit it and keep it around */
    if(image_file_lpt)
      SetFileAttributes(image_file_lpt, FILE_ATTRIBUTE_READONLY);

    if(*cmd == '*' || (*cmd == '\"' && *(cmd+1) == '*')){
	/*
	 * It has been asked that there be the ability to do an 
	 * "Open With..." on attachments like you can from the
	 * Windows file browser.  After looking into this, it
	 * seems that the only way to do this would be through
	 * an undocumented hack.  Here, we would pass "openas" as
	 * the verb to mswin_shell_exec (also some changes in
	 * mswin_shell_exec).  Since this is the delicate world
	 * of attachment handling, it seems right not to rely on
	 * a hack.  The interface wouldn't be too clean anyways,
	 * as we would have to download the attachment only to
	 * display the "Open With..." dialog.  Go figure, some
	 * things Microsoft just wants to keep to themselves.
	 */

    /*
     * 2/1/2007. No idea when the above comment was written, but it is
     *  documented now at least. The below two urls describe the "openas" verb:
     *
     * http://blogs.msdn.com/oldnewthing/archive/2004/11/26/270710.aspx
     * http://msdn.microsoft.com/library/default.asp?url=/library/en-us/
     *      shellcc/platform/shell/programmersguide/shell_basics/
     *      shell_basics_extending/context.asp
     */
	success = mswin_shell_exec(cmd, &childProcess) == 0;
    }
    else{
	memset(&proc_info, 0, sizeof(proc_info));
	memset(&start_info, 0, sizeof(start_info));
	start_info.dwFlags	    = STARTF_FORCEONFEEDBACK;
	start_info.wShowWindow  = SW_SHOWNORMAL;

	if(cmd)
	  cmd_lpt = utf8_to_lptstr(cmd);

	if(CreateProcess(NULL, cmd_lpt, NULL, NULL, FALSE,
			 CREATE_NEW_CONSOLE | CREATE_NEW_PROCESS_GROUP,
			 NULL, NULL, &start_info, &proc_info) == TRUE){
	    q_status_message(SM_ORDER, 0, 4, "VIEWER command completed");
	    dprint ((3, "CreatProcess(%s)  Success.\n",
		    cmd ? cmd : "?"));
	    childProcess = proc_info.hProcess;
	    success = 1;
	}

	if(cmd_lpt)
	  fs_give((void **) &cmd_lpt);
    }

    if(!success){
	int rc = (int) GetLastError();
	if(image_file_lpt)
	  SetFileAttributes(image_file_lpt, FILE_ATTRIBUTE_NORMAL);

	our_unlink(image_file);
	q_status_message2(SM_ORDER, 3, 4, "\007Can't start viewer. %s%s.",
			  (rc == 2 || rc == 3) ? "Viewer not found:  " :
			  (rc == 8) ? "Not enough memory" : "Windows error ",
			  (rc == 2 || rc == 3) ? cmd :
			  (rc == 8) ? "" : int2string(rc));
    }

    if(image_file_lpt)
      fs_give((void **) &image_file_lpt);

#elif	OSX_TARGET

    char   *command = NULL,
	   *result_file = NULL,
	   *p;
    char  **r_file_h;
    PIPE_S *syspipe;
    int     mode;

    if(!mc_cmd)
      return;
    if(mc_cmd->special_handling){
	char *rhost;

	if(mime_os_specific_access())
	  osx_launch_special_handling(mc_cmd, image_file);
	else{
	  q_status_message(SM_ORDER, 0, 4, "VIEWER command cancelled");
	  our_unlink(image_file);
	}
    }
    else {
	char *cmd = mc_cmd->command;
	size_t l;

	l = 32 + strlen(cmd) + (2*strlen(image_file));
	p = command = (char *) fs_get((l+1) * sizeof(char));
	if(!needsterminal)  /* put in background if it doesn't need terminal */
	  *p++ = '(';

	snprintf(p, l+1-(p-command), "%s", cmd);
	p += strlen(p);
	if(!needsterminal){
	    if(p-command+2 < l+1){
		*p++ = ')';
		*p++ = ' ';
		*p++ = '&';
	    }
	}

	if(p-command < l+1)
	  *p++ = '\n';

	if(p-command < l+1)
	  *p   = '\0';

	dprint((9, "exec_mailcap_cmd: command=%s\n",
		   command ? command : "?"));

	mode = PIPE_RESET;
	if(needsterminal == 1)
	  r_file_h = NULL;
	else{
	    mode       |= PIPE_WRITE | PIPE_STDERR;
	    result_file = temp_nam(NULL, "pine_cmd");
	    r_file_h    = &result_file;
	}

	if(syspipe = open_system_pipe(command, r_file_h, NULL, mode, 0, pipe_callback, NULL)){
	    close_system_pipe(&syspipe, NULL, pipe_callback);
	    if(needsterminal == 1)
	      q_status_message(SM_ORDER, 0, 4, "VIEWER command completed");
	    else if(needsterminal == 2)
	      display_output_file(result_file, "VIEWER", " command result", 1);
	    else
	      display_output_file(result_file, "VIEWER", " command launched", 1);
	}
	else
	  q_status_message1(SM_ORDER, 3, 4, "Cannot spawn command : %s", cmd);

	fs_give((void **)&command);
	if(result_file)
	  fs_give((void **)&result_file);
    }
#else
    char   *command = NULL,
	   *result_file = NULL,
	   *p, *cmd;
    char  **r_file_h;
    PIPE_S *syspipe;
    int     mode;
    size_t  l;

    /* no os-specific command handling */
    if(mc_cmd)
      cmd = mc_cmd->command;
    else
      return;
    l = 32 + strlen(cmd) + 2*strlen(image_file);
    p = command = (char *)fs_get((l+1) * sizeof(char));
    if(!needsterminal)  /* put in background if it doesn't need terminal */
      *p++ = '(';
    snprintf(p, l+1-(p-command), "%s ; rm -f %s", cmd, image_file);
    command[l] = '\0';
    p += strlen(p);
    if(!needsterminal && (p-command)+5 < l){
	*p++ = ')';
	*p++ = ' ';
	*p++ = '&';
    }

    *p++ = '\n';
    *p   = '\0';

    dprint((9, "exec_mailcap_cmd: command=%s\n",
	   command ? command : "?"));

    mode = PIPE_RESET;
    if(needsterminal == 1)
      r_file_h = NULL;
    else{
	mode       |= PIPE_WRITE | PIPE_STDERR;
	result_file = temp_nam(NULL, "pine_cmd");
	r_file_h    = &result_file;
    }

    if((syspipe = open_system_pipe(command, r_file_h, NULL, mode, 0, pipe_callback, NULL)) != NULL){
	close_system_pipe(&syspipe, NULL, pipe_callback);
	if(needsterminal == 1)
	  q_status_message(SM_ORDER, 0, 4, "VIEWER command completed");
	else if(needsterminal == 2)
	  display_output_file(result_file, "VIEWER", " command result", 1);
	else
	  display_output_file(result_file, "VIEWER", " command launched", 1);
    }
    else
      q_status_message1(SM_ORDER, 3, 4, "Cannot spawn command : %s", cmd);

    fs_give((void **)&command);

    if(result_file)
      fs_give((void **)&result_file);
#endif
}


/* ----------------------------------------------------------------------
   Execute the given mailcap test= cmd

  Args: cmd -- command to execute
  Returns exit status
  
  ----*/
int
exec_mailcap_test_cmd(char *cmd)
{
#ifdef _WINDOWS
    return((WinExec(cmd, SW_SHOWMINNOACTIVE) < 32) ? 1 : 0);
#else
    PIPE_S *syspipe;

    return((syspipe = open_system_pipe(cmd, NULL, NULL, PIPE_SILENT, 0,
				       pipe_callback, NULL))
	     ? close_system_pipe(&syspipe, NULL, pipe_callback) : -1);
#endif
}


char *
url_os_specified_browser(char *url)
{
#ifdef _WINDOWS
    return(mswin_reg_default_browser(url));
#elif	OSX_TARGET
    if(mime_os_specific_access()){
	return(cpystr("open"));
    }
#else
    /* do nothing here */
    return(NULL);
#endif
}

/*
 * Return a pretty command, on some OS's we might do something
 * different than just display the command.
 *
 * free_ret - whether or not to free the return value
 */
char *
execview_pretty_command(MCAP_CMD_S *mc_cmd, int *free_ret)
{
    char *str;
    int rv_to_free = 0;

    if(free_ret)
      *free_ret = rv_to_free;

    if(!mc_cmd)
      return NULL;

    str = mc_cmd->command;

#ifdef _WINDOWS
    if(*str == '*' || (*str == '\"' && str[1] == '*')){
	if(!strncmp(str + ((*str == '\"') ? 2 : 1), "DDE*", 4))
	  str = cpystr("via app already running");
	else if(!strncmp(str + ((*str == '\"') ? 2 : 1),"ShellEx*",8))
	  str = cpystr("via Explorer defined app");
	else
	  str = cpystr("via Windows-specific method");

	rv_to_free = 1;
    }
#elif	OSX_TARGET
    if(mc_cmd->special_handling){
	CFStringRef str_ref = NULL, kind_str_ref = NULL;
	CFURLRef url_ref;
	char buf[256];

	if((str_ref = CFStringCreateWithCString(NULL, mc_cmd->command,
					 kCFStringEncodingASCII)) == NULL)
	  return "";

	if((url_ref = CFURLCreateWithString(NULL, str_ref, NULL)) == NULL)
	  return "";

	if(LSCopyDisplayNameForURL(url_ref, &kind_str_ref) != noErr)
	  return "";

	if(CFStringGetCString(kind_str_ref, buf, (CFIndex)255,
			      kCFStringEncodingASCII) == false)
	  return "";

	buf[255] = '\0';
	str = cpystr(buf);
	rv_to_free = 1;
	if(kind_str_ref)
	  CFRelease(kind_str_ref);
    }
#else
    /* always pretty */
#endif

    if(free_ret)
      *free_ret = rv_to_free;

    return(str);
}


#if	OSX_TARGET
void
osx_launch_special_handling(MCAP_CMD_S *mc_cmd, char *image_file)
{
    CFStringRef str_ref = NULL;
    CFURLRef url_ref = NULL;
    LSLaunchFSRefSpec launch_spec;
    FSRef app_ref, file_ref;
    static EXEC_EVENT_DATA_S event_data;

    install_app_launch_cb((void *)&event_data);
    if((str_ref = CFStringCreateWithCString(NULL, mc_cmd->command,
					kCFStringEncodingASCII)) == NULL)
      return;
    if((url_ref = CFURLCreateWithString(NULL, str_ref, NULL)) == NULL)
      return;
    if(CFURLGetFSRef(url_ref, &app_ref) == false)
      return;
    if(FSPathMakeRef((unsigned char *)image_file,
		     &file_ref, NULL) != noErr)
      return;
    launch_spec.appRef = &app_ref;
    launch_spec.numDocs = 1;
    launch_spec.itemRefs = &file_ref;
    launch_spec.passThruParams = NULL;
    launch_spec.launchFlags = kLSLaunchDontAddToRecents | kLSLaunchNoParams
      | kLSLaunchAsync | kLSLaunchNewInstance;
    /* would want to use this if we ever did true event handling */
    launch_spec.asyncRefCon = 0;

    if(LSOpenFromRefSpec( &launch_spec, NULL) == noErr){
	/*
	 * Here's the strategy:  we want to be able to just launch
	 * the app and then just delete the temp file, but that
	 * doesn't work because the called app needs the temp file
	 * at least until it's finished loading.  Being that there's
	 * no way to tell when the app has finished loading, we wait
	 * until the program has exited, which is the safest thing to
	 * do and is what we do for windows.  Since we haven't totally
	 * embraced event handling at this point, we must do the waiting
	 * synchronously.  We allow for a keystroke to stop waiting, and
	 * just delete the temp file.
	 *   Ideally, we would launch the app, and keep running, checking
	 * the events until the process terminates, and then delete the
	 * temp file.  In this method, we would delete the temp file
	 * at close time if the app was still running.
	 */
	int ch;
	OSStatus rne_rv;
	EventTargetRef target;
	EventRef out_event;
	EventTypeSpec event_types[2] = {
	    {kEventClassApplication, kEventAppTerminated},
	    {kEventClassApplication, kEventAppLaunchNotification}};

	q_status_message(SM_ORDER, 0, 4,
	  "Waiting for program to finish, or press a key to stop waiting...");
	flush_status_messages(1);
	target = GetEventDispatcherTarget();
	event_data.done = 0;
	event_data.set_pid = 0;
	while(!event_data.done){
	    if((rne_rv = ReceiveNextEvent(2, event_types, 1,
					  true, &out_event)) == noErr){
		SendEventToEventTarget(out_event, target);
		ReleaseEvent(out_event);
	    }
	    else if(rne_rv == eventLoopTimedOutErr){
		ch = read_char(1);
		if(ch)
		  event_data.done = 1;
	    }
	    else if(rne_rv == eventLoopQuitErr)
	      event_data.done = 1;
	}
	our_unlink(image_file);
    }
    q_status_message(SM_ORDER, 0, 4, "VIEWER command completed");
}

pascal OSStatus osx_launch_app_callback(EventHandlerCallRef next_h, EventRef event,void *user_data)
{
    EXEC_EVENT_DATA_S *ev_datap = (EXEC_EVENT_DATA_S *)user_data;
    ProcessSerialNumber pid;
    Boolean res = 0;

    static int dont_do_anything_yet = 0;
    switch(GetEventClass(event)){
      case kEventClassKeyboard:
	ev_datap->done = 1;
	break;
      case kEventClassApplication:
	switch(GetEventKind(event)){
	  case kEventAppTerminated:
	    GetEventParameter(event,
			      kEventParamProcessID,
			      typeProcessSerialNumber, NULL,
			      sizeof(pid), NULL,
			      &pid);
	    SameProcess(&ev_datap->pid, &pid, &res);
	    if(res){
		ev_datap->done = 1;
		ev_datap->set_pid = 0;
	    }
	    break;
	  case kEventAppLaunchNotification:
	    /* could check asyncRef too */
	    if(!ev_datap->set_pid){ /* should always be true */
		GetEventParameter(event,
				  kEventParamProcessID,
				  typeProcessSerialNumber, NULL,
				  sizeof(ev_datap->pid), NULL,
				  &(ev_datap->pid));
		ev_datap->set_pid = 1;
	    }
	    break;
	}
	break;
    }
    return(noErr);
}


int
install_app_launch_cb(void *user_data)
{
    static int already_installed = 0;

    if(!already_installed){
	EventHandlerUPP cb_upp;
	EventTypeSpec event_types[2] = {
	    {kEventClassApplication, kEventAppTerminated},
	    {kEventClassApplication, kEventAppLaunchNotification}};

	if((cb_upp = NewEventHandlerUPP(osx_launch_app_callback)) == NULL)
	  return 1;
	InstallApplicationEventHandler(cb_upp, 2, event_types,
				       user_data, NULL);
	already_installed = 1;
    }
    return 0;
}
#endif	/* OSX_TARGET */
