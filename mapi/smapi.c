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

#include "pmapi.h"

ULONG FAR PASCAL MAPISendMail(
    LHANDLE lhSession,
    ULONG ulUIParam,
    lpMapiMessage lpMessage,
    FLAGS flFlags,
    ULONG ulReserved)
{
    ULONG i;
    int ac = 0, rv = 0, can_suppress_dialog = 0, has_recips = 0;
    char **av, *tmpfree, *url;

    if(MSDEBUG){
      fprintf(ms_global->dfd, "\r\nIn MAPISendMail\r\n");
      fprintf(ms_global->dfd, " MAPI_DIALOG is %s set\r\n", 
	      flFlags & MAPI_DIALOG ? "" : "NOT");
      fprintf(ms_global->dfd, " MAPI_LOGON_UI is %s set\r\n", 
	      flFlags & MAPI_LOGON_UI ? "" : "NOT");
      fprintf(ms_global->dfd, " MAPI_NEW_SESSION is %s set\r\n", 
	      flFlags & MAPI_NEW_SESSION ? "" : "NOT");
      fprintf(ms_global->dfd, " session number is %p\r\n", lhSession);
      _flushall();
    }
    msprint_message_structure(lpMessage);

    url = message_structure_to_mailto_url(lpMessage);

    for(i = 0; i < lpMessage->nRecipCount; i++){
	if((&lpMessage->lpRecips[i])->ulRecipClass != MAPI_ORIG)
	  has_recips = 1;
    }
    if((flFlags & MAPI_DIALOG) && has_recips &&
       (lpMessage->lpszSubject
	|| lpMessage->lpszNoteText
	|| lpMessage->nFileCount)){
	if(ms_global->pmapi_suppress_dialogs == PMSD_YES)
	  can_suppress_dialog = 1;
	else if(ms_global->pmapi_suppress_dialogs == PMSD_PROMPT){
	    if(MessageBox(NULL, "Edit Message Before Sending?", "pmapi32.dll", MB_YESNO|MB_ICONQUESTION) == IDNO)
	      can_suppress_dialog = 1;
	}
    }
    if(can_suppress_dialog ||
       ((flFlags & MAPI_DIALOG) == 0)){
	rv = send_msg_nodlg(lhSession, ulUIParam, lpMessage, flFlags, ulReserved);
	return(rv);
    }

    if (lpMessage == NULL){
      /*        lpMessage->lpFiles == NULL ||
		lpMessage->lpFiles->lpszPathName == NULL) */
      /* this old code checked to see that there were attachments */
      /* I think it should be all right for there not to be attachments */
      
      ErrorBox("MAPISendMail: %s pointer in lpMessage argument", "NULL");
      return 0;
    }

    /*
     * allocate space for spawn's argv array:
     * 	2 for each of the nFileCount attachments
     * +4 for the -p pinerc, and av[0] and trailing NULL
     * +2 for the url
     */
    {
    int avSize = (6+2*lpMessage->nFileCount)*sizeof(*av);
    av = (char **)fs_get(avSize);
    if (av == NULL) {
	ErrorBox("MAPISendMail: fs_get of %d bytes for av failed", (avSize));
	return MAPI_E_FAILURE;
	}
    }

    /*
     * establish a default path to the pine executable which will
     * probably be replaced with a value from the registry
     */
    if(ms_global && ms_global->pineExe){
      av[ac++] = quote(ms_global->pineExe);
      if(!av[0]) {
	ErrorBox("Cannot fs_get for %s","pine");
	return MAPI_E_FAILURE;
      }
    }
    else return MAPI_E_FAILURE;
    if(ms_global->pinerc){
      av[ac++] = mstrdup("-p");
      if(tmpfree = TmpCopy(ms_global->pinerc, IS_PINERC)){
	av[ac++] = quote(tmpfree);
	fs_give((void **)&tmpfree);
      }
      else
	av[ac++] = quote(ms_global->pinerc);
    }
    if(url){
	av[ac++] = mstrdup("-url");
	av[ac++] = url;
    }

    /*
     * make a temporary copy of each attachment in attachDir,
     * add the new filename (suitably quoted) to pine's argument list
     */
    for (i=0; i < lpMessage->nFileCount; ++i) {
      char *oldPath = lpMessage->lpFiles[i].lpszPathName;
      char *newCopy, *tmpfree;
      tmpfree = TmpCopy(oldPath, NOT_PINERC);
      if(tmpfree == NULL)
	return MAPI_E_ATTACHMENT_NOT_FOUND;
      newCopy = quote(tmpfree);
      fs_give((void **)&tmpfree);
      if (newCopy) {
	av[ac++] = mstrdup("-attach_and_delete");
	av[ac++] = newCopy;
      }
      else{
	if(MSDEBUG)
	  fprintf(ms_global->dfd, "TmpCopy returned null\r\n");
      }
      if (MSDEBUG) {
	fprintf(ms_global->dfd,"Attachment %d: old path: %s\r\n--\r\n",
		i,oldPath);
	fprintf(ms_global->dfd,">>: %s\r\n--\r\n",ms_global->attachDir);
	fprintf(ms_global->dfd,">>: tmp path: %s\r\n--\r\n",
		(newCopy?newCopy:"NULL"));
      }
    }
    av[ac++] = NULL;

    if(MSDEBUG) {
	fprintf(ms_global->dfd, "spawning %s (else %s):\r\n", 
		ms_global->pineExe, ms_global->pineExeAlt);
	for (i=0; av[i]; ++i)
	    fprintf(ms_global->dfd, "av[%d] = %s\r\n", i, av[i]);
    }

    _flushall();
    if (_spawnvp(_P_NOWAIT, ms_global->pineExe, av) == -1 &&
	_spawnvp(_P_NOWAIT, ms_global->pineExeAlt, av) == -1)
        {
	ErrorBox("MAPISendMail: _spawnvp of %s failed", ms_global->pineExe);
	if(MSDEBUG) fprintf(ms_global->dfd, "_spawnvp %s and %s failed\r\n", 
			    ms_global->pineExe,ms_global->pineExeAlt);
	return(MAPI_E_FAILURE);
	}

    /*
     * close and free allocated resources
     */
    if (av[0]) fs_give((void **)&av[0]);
    for (i=1; av[i]; i++) {
      if (av[i]) fs_give((void **)&av[i]);
    }
    fs_give((void **)&av);

    return SUCCESS_SUCCESS;
}


ULONG FAR PASCAL MAPILogon(
    ULONG ulUIParam,
    LPTSTR lpszProfileName,
    LPTSTR lpszPassword,
    FLAGS flFlags,
    ULONG ulReserved,      
    LPLHANDLE lplhSession)
{
    mapi_global_s *nmg = ms_global;
    sessionlist_s *cs;
    sessionlist_s *ts;

    if(MSDEBUG){
      fprintf(ms_global->dfd, "\r\nIn MAPILogon\r\n");
      fprintf(ms_global->dfd, " MAPI_FORCE_DOWNLOAD is %s set\r\n", 
	      flFlags & MAPI_FORCE_DOWNLOAD ? "" : "NOT");
      fprintf(ms_global->dfd, " MAPI_NEW_SESSION is %s set\r\n", 
	      flFlags & MAPI_NEW_SESSION ? "" : "NOT");
      fprintf(ms_global->dfd, " MAPI_LOGON_UI is %s set\r\n", 
	      flFlags & MAPI_LOGON_UI ? "" : "NOT");
      fprintf(ms_global->dfd, " MAPI_PASSWORD_UI is %s set\r\n", 
	      flFlags & MAPI_PASSWORD_UI ? "" : "NOT");
      fprintf(ms_global->dfd, " ulUIParam is %p\r\n", ulUIParam); 
      if(lpszProfileName)
	fprintf(ms_global->dfd, " profile name is %s\r\n", lpszProfileName); 
      fprintf(ms_global->dfd, " lplhSession is %p\r\n", lplhSession); 
      if(lplhSession)
	fprintf(ms_global->dfd, " session number is %p\r\n", *lplhSession);
      fclose(ms_global->dfd);
      ms_global->dfd = fopen(ms_global->debugFile, "ab");
      if(!ms_global->dfd){
	ms_global->debug = 0;
	ErrorBox("Problem with debug file %s! Debugging turned off",
		 ms_global->debugFile);
	fs_give((void **)&ms_global->debugFile);
	ms_global->debugFile = NULL;
	
      }
    }
    if(lplhSession == NULL){
      if(MSDEBUG){
	fprintf(nmg->dfd, 
		"lplhSession is a NULL pointer, returning MAPI_E_FAILURE\r\n");
      }
      return MAPI_E_FAILURE;
    }
    if((flFlags & MAPI_NEW_SESSION) || !nmg->sessionlist){
	cs = new_sessionlist();
	*lplhSession = cs->session_number;

	if(nmg->sessionlist == NULL)
	  nmg->sessionlist = cs;
	else{
	    ts = nmg->sessionlist;
	    while(ts->next) ts = ts->next;
	    ts->next = cs;
	}
    }
    else{
	if(!*lplhSession) *lplhSession = nmg->sessionlist->session_number;
	else
	  cs = get_session(*lplhSession);
	if(!cs)
	  return MAPI_E_INVALID_SESSION;
    }
    cs->flags.mapi_logon_ui = ((flFlags & MAPI_LOGON_UI) || (flFlags & MAPI_PASSWORD_UI))
      ? 1 : 0;
    if((flFlags & MAPI_LOGON_UI) || (flFlags & MAPI_PASSWORD_UI))
      if(InitPineSpecific(cs) == -1) return MAPI_E_FAILURE;

    if(MSDEBUG){
	fprintf(nmg->dfd, 
		"lplhSession is returning %p for session handle\r\n", *lplhSession);
	_flushall();
    }
    return SUCCESS_SUCCESS;
}

ULONG FAR PASCAL MAPILogoff (
    LHANDLE lhSession,
    ULONG ulUIParam,
    FLAGS flFlags,
    ULONG ulReserved)
{
    sessionlist_s *cs;

    if(MSDEBUG){
      fprintf(ms_global->dfd, "\r\nIn MAPILogoff\r\n");
      fprintf(ms_global->dfd, " session number is %p\r\n", lhSession);
      _flushall();
    }

    cs = get_session(lhSession);
    if(!cs) return MAPI_E_INVALID_SESSION;
    if(!cs->open_stream) return SUCCESS_SUCCESS;
    cs->open_stream = mail_close_full(cs->open_stream, 0);
    cs->open_stream = NULL;
    if(cs->currently_open){
      fs_give((void **)&cs->currently_open);
      cs->currently_open = NULL;
    }
    cs->dlge.edit1[0] = '\0';
    cs->dlge.edit2[0] = '\0';
    /*    ms_global->flags.passfile_checked = FALSE; */
    return SUCCESS_SUCCESS;
}

ULONG FAR PASCAL MAPIFindNext (
    LHANDLE lhSession,
    ULONG ulUIParam,
    LPSTR lpszMessageType,
    LPSTR lpszSeedMessageID,
    FLAGS flFlags,
    ULONG ulReserved,
    LPSTR lpszMessageID)
{
    mapi_global_s *nmg = ms_global;
    sessionlist_s *cs;
    char tmp[1024], tmpseq[1024];
    int msg_found;
    unsigned long tmp_msgno, i, cur_msg = 0;
    MESSAGECACHE *telt;

    if(MSDEBUG){
      fprintf(nmg->dfd, "\r\nMAPIFindNext Called\r\n");
      fprintf(ms_global->dfd, " MAPI_GUARANTEE_FIFO is %s set\r\n", 
	      flFlags & MAPI_GUARANTEE_FIFO ? "" : "NOT");
      fprintf(ms_global->dfd, " MAPI_LONG_MSGID is %s set\r\n", 
	      flFlags & MAPI_LONG_MSGID ? "" : "NOT");
      fprintf(ms_global->dfd, " MAPI_MAPI_UNREAD_ONLY is %s set\r\n", 
	      flFlags & MAPI_UNREAD_ONLY ? "" : "NOT");
      fprintf(ms_global->dfd, " session number is %p\r\n", lhSession);
      fprintf(ms_global->dfd, " ulUIParam is %d\r\n", ulUIParam);
      fprintf(ms_global->dfd, "message type is %s\r\n", 
	      lpszMessageType ? lpszMessageType : "NOT SET");
      fprintf(ms_global->dfd, "seed message id is %s\r\n", 
	      lpszSeedMessageID ? lpszSeedMessageID : "NOT SET");
      _flushall();
    }

    cs = get_session(lhSession);
    if(!cs){
      if(MSDEBUG)
	fprintf(ms_global->dfd, "Session number %p is invalid\r\n", lhSession);
      return MAPI_E_INVALID_SESSION;
    }

    if(InitPineSpecific(cs) == -1) return MAPI_E_FAILURE;

    if(!check_mailstream(cs))
      return MAPI_E_FAILURE;
    if(lpszSeedMessageID == NULL || *lpszSeedMessageID == '\0')
      cur_msg = 0;
    else{
      cur_msg = convert_to_msgno(lpszSeedMessageID);
    }
    if(flFlags & MAPI_UNREAD_ONLY){
      if(cur_msg + 1 > cs->open_stream->nmsgs)
	return MAPI_E_NO_MESSAGES;
      tmp_msgno = cur_msg+1;
      msg_found = FALSE;
      while(!msg_found && tmp_msgno <= cs->open_stream->nmsgs){
	sprintf(tmp, "%d", tmp_msgno);
	strcpy(tmpseq, tmp);
	if(tmp_msgno+1 <= cs->open_stream->nmsgs){
	  sprintf(tmp,":%d", min(cs->open_stream->nmsgs,tmp_msgno+100));
	  strcat(tmpseq, tmp);
	}
	mail_fetch_flags(cs->open_stream, tmpseq, NIL);
	for(i = tmp_msgno; 
	    i <= (unsigned long)min(cs->open_stream->nmsgs,tmp_msgno+100);
	    i++){
	  telt = mail_elt(cs->open_stream, i);
	  if(telt->seen == 0){
	    msg_found = TRUE;
	    if(MSDEBUG)
	      fprintf(nmg->dfd, "msgno %d is the next UNREAD message after %d",
		      i, cur_msg);
	    break;
	  }
	}
	if(!msg_found){
	  if(i == cs->open_stream->nmsgs){
	    if(MSDEBUG)
	      fprintf(nmg->dfd,"No UNREAD messages found after %d\r\n",
		      cur_msg);
	    return MAPI_E_NO_MESSAGES;
	  }
	  tmp_msgno += 100;
	}
      }
      if(msg_found)
	cur_msg = i;
      else return MAPI_E_NO_MESSAGES;
    }
    else{
      if(cur_msg+1 > cs->open_stream->nmsgs)
	return MAPI_E_NO_MESSAGES;
      cur_msg++;
    }

    sprintf(lpszMessageID,"%d", cur_msg);

    if(MSDEBUG)
      fprintf(nmg->dfd, " Next message found is %d\r\n", cur_msg);
    return SUCCESS_SUCCESS;
}


ULONG FAR PASCAL MAPIReadMail(
    LHANDLE lhSession,
    ULONG ulUIParam,
    LPSTR lpszMessageID,
    FLAGS flFlags,
    ULONG ulReserved,
    lpMapiMessage FAR *lppMessage)
{
    mapi_global_s *nmg = ms_global;
    unsigned rv;
    unsigned long msgno = 0, flags;
    sessionlist_s *cs;

    if(MSDEBUG){
      fprintf(nmg->dfd, "\r\nIn MAPIReadMail\r\n");
      fprintf(nmg->dfd, "  MAPI_PEEK is %s\r\n", 
	      flFlags & MAPI_PEEK ? "TRUE":"FALSE");
      fprintf(nmg->dfd, "  MAPI_BODY_AS_FILE is %s\r\n", 
	      flFlags & MAPI_BODY_AS_FILE ? "TRUE":"FALSE");
      fprintf(nmg->dfd, "  MAPI_ENVELOPE_ONLY is %s\r\n", 
	      flFlags & MAPI_ENVELOPE_ONLY ? "TRUE":"FALSE");
      fprintf(nmg->dfd, "  MAPI_SUPPRESS_ATTACH is %s\r\n",
	      flFlags & MAPI_SUPPRESS_ATTACH ? "TRUE":"FALSE");
      fprintf(ms_global->dfd, " session number is %p\r\n", lhSession);
      _flushall();
    }

    cs = get_session(lhSession);
    if(!cs){
      if(MSDEBUG)
	fprintf(ms_global->dfd, "Session number %p is invalid\r\n", lhSession);
      return MAPI_E_INVALID_SESSION;
    }

    if(InitPineSpecific(cs) == -1) return MAPI_E_FAILURE;

    if(!check_mailstream(cs))
      return MAPI_E_FAILURE;

    msgno = convert_to_msgno(lpszMessageID);

    if(msgno == 0){
      if(MSDEBUG)
	fprintf(nmg->dfd, "Invalid Message ID: %s\r\n", lpszMessageID);
      return MAPI_E_INVALID_MESSAGE;
    }

    if(MSDEBUG){
      fprintf(nmg->dfd, "lpszMessageID: %s,  converted msgno: %d\r\n",
	      lpszMessageID, msgno);
    }

    if(flFlags & MAPI_PEEK)
      flags = FT_PEEK;
    else flags = NIL;

    rv = fetch_structure_and_attachments(msgno, flags, flFlags, cs);
    if(rv == MAPI_E_FAILURE)
      return rv;
    else if(rv == SUCCESS_SUCCESS){
      *lppMessage = cs->lpm;
      return rv;
    }
    else
      return rv;
}

ULONG FAR PASCAL MAPIAddress(
    LHANDLE lhSession,
    ULONG ulUIParam,
    LPTSTR lpszCaption,
    ULONG nEditFields,
    LPTSTR lpszLabels,
    ULONG nRecips,
    lpMapiRecipDesc lpRecips,
    FLAGS flFlags,
    ULONG ulReserved,
    LPULONG lpnNewRecips,
    lpMapiRecipDesc FAR * lppNewRecips)
{
    if(MSDEBUG){
      fprintf(ms_global->dfd, "\r\nIn MAPIAddress\r\n");
      fprintf(ms_global->dfd, " MAPI_LOGON_UI is %s set\r\n", 
	      flFlags & MAPI_LOGON_UI ? "" : "NOT");
      fprintf(ms_global->dfd, " MAPI_NEW_SESSION is %s set\r\n", 
	      flFlags & MAPI_NEW_SESSION ? "" : "NOT");
      fprintf(ms_global->dfd, " session number is %p\r\n", lhSession);
      _flushall();
    }
    return MAPI_E_NOT_SUPPORTED;
}

ULONG FAR PASCAL MAPIDeleteMail(
    LHANDLE lhSession,
    ULONG ulUIParam,
    LPTSTR lpszMessageID,
    FLAGS flFlags,
    ULONG ulReserved)
{
    if(MSDEBUG){
      fprintf(ms_global->dfd, "\r\nIn MAPIDeleteMail\r\n");
      fprintf(ms_global->dfd, " session number is %p\r\n", lhSession);
      _flushall();
    }
    return MAPI_E_NOT_SUPPORTED;
}

ULONG FAR PASCAL MAPIDetails(
  LHANDLE lhSession,
  ULONG ulUIParam,
  lpMapiRecipDesc lpRecip,
  FLAGS flFlags,
  ULONG ulReserved)
{
    if(MSDEBUG){
      fprintf(ms_global->dfd, "\r\nIn MAPIDetails\r\n");
      fprintf(ms_global->dfd, " MAPI_NO_MODIFY is %s set\r\n", 
	      flFlags & MAPI_AB_NOMODIFY ? "" : "NOT");
      fprintf(ms_global->dfd, " MAPI_LOGON_UI is %s set\r\n", 
	      flFlags & MAPI_LOGON_UI ? "" : "NOT");
      fprintf(ms_global->dfd, " MAPI_NEW_SESSION is %s set\r\n", 
	      flFlags & MAPI_NEW_SESSION ? "" : "NOT");
      fprintf(ms_global->dfd, " session number is %p\r\n", lhSession);
      _flushall();
    }
    return MAPI_E_NOT_SUPPORTED;
}

ULONG FAR PASCAL MAPIFreeBuffer(
  LPVOID pv)
{
    mapi_global_s *nmg = ms_global;

    if(MSDEBUG){
      fprintf(nmg->dfd, "\r\nMAPIFreeBuffer called, buffer is %p\r\n", pv);
      _flushall();
    }
    free_mbuffer(pv);
    return SUCCESS_SUCCESS;
}

ULONG FAR PASCAL MAPIResolveName(
    LHANDLE lhSession,
    ULONG ulUIParam,
    LPTSTR lpszName,
    FLAGS flFlags,
    ULONG ulReserved,
    lpMapiRecipDesc FAR * lppRecip)
{
    mapi_global_s *nmg = ms_global;
    sessionlist_s *cs, *tsession;
    static char *fakedomain = "@";
    ADDRESS *adrlist = NULL;
    char *adrstr, *tadrstr, *tadrstrbuf;

    if(MSDEBUG){
	fprintf(ms_global->dfd, "\r\nIn MAPIResolveName\r\n");
	fprintf(ms_global->dfd, " MAPI_NO_MODIFY is %sset\r\n", 
		flFlags & MAPI_AB_NOMODIFY ? "" : "NOT ");
	fprintf(ms_global->dfd, " MAPI_DIALOG is %sset\r\n", 
		flFlags & MAPI_DIALOG ? "" : "NOT ");
	fprintf(ms_global->dfd, " MAPI_LOGON_UI is %sset\r\n", 
		flFlags & MAPI_LOGON_UI ? "" : "NOT ");
	fprintf(ms_global->dfd, " MAPI_NEW_SESSION is %sset\r\n", 
		flFlags & MAPI_NEW_SESSION ? "" : "NOT ");
	fprintf(ms_global->dfd, " session number is %p\r\n", lhSession);
	fprintf(ms_global->dfd, " String to Resolve is %s\r\n", lpszName);
	_flushall();
    }

    if((flFlags & MAPI_NEW_SESSION) || lhSession == 0)
      tsession = cs = new_sessionlist();
    else {
	cs = get_session(lhSession);
	if(!cs){
	  if(MSDEBUG)
	    fprintf(ms_global->dfd, "Session number %p is invalid\r\n", lhSession);
	  return MAPI_E_INVALID_SESSION;
	}
    }
    cs->flags.mapi_logon_ui = (flFlags & MAPI_LOGON_UI) ? 1 : 0;
    if(InitPineSpecific(cs) == -1) return MAPI_E_FAILURE;
    tadrstrbuf = tadrstr = mstrdup(lpszName);
    removing_leading_and_trailing_white_space(tadrstr);
    if(_strnicmp(tadrstr, "SMTP:", 5) == 0){
	tadrstr += 5;
	if(tadrstr[0] == '(' && tadrstr[strlen(tadrstr) - 1] == ')'){
	    tadrstr[strlen(tadrstr)-1] = '\0';
	    tadrstr++;
	    removing_leading_and_trailing_white_space(tadrstr);
	}
    }
    rfc822_parse_adrlist(&adrlist, tadrstr, 
			 nmg->prcvars[USER_DOMAIN]->val.p ? nmg->prcvars[USER_DOMAIN]->val.p
			 : fakedomain);
    fs_give((void **)&tadrstrbuf);
    if(!adrlist || adrlist->next){   /* I guess there aren't supposed to be multiple addresses, */
	                 /* which is pretty lame */
	mail_free_address(&adrlist);
	if(tsession)
	  fs_give((void **)&tsession);
	msprint("Returning MAPI_E_AMBIGUOUS_RECIPIENT\r\n");
	return MAPI_E_AMBIGUOUS_RECIPIENT;
    }
    if(!adrlist->host || *adrlist->host == '@'){
	mail_free_address(&adrlist);
	if(tsession)
	  fs_give((void **)&tsession);
	msprint("Returning MAPI_E_AMBIGUOUS_RECIPIENT\r\n");
	return MAPI_E_AMBIGUOUS_RECIPIENT;
    }

    (*lppRecip) = new_MapiRecipDesc(1);
    if(adrlist->personal){
	(*lppRecip)->lpszName = mstrdup(adrlist->personal);
	adrlist->personal = NULL;
    }
    adrstr = (char *)fs_get((8 + strlen(adrlist->mailbox) + strlen(adrlist->host)) * sizeof(char));
    sprintf(adrstr, "SMTP:%s@%s", adrlist->mailbox, adrlist->host);
    (*lppRecip)->lpszAddress = adrstr;

    /* The spec says it's a recipient, so set the thing to MAPI_TO */
    (*lppRecip)->ulRecipClass = MAPI_TO;

    mail_free_address(&adrlist);
    msprint1(" Returning with name: %s\r\n", (*lppRecip)->lpszName ? (*lppRecip)->lpszName : "(no name)");
    msprint1("             address: %s\r\n", (*lppRecip)->lpszAddress ? (*lppRecip)->lpszAddress : "(no address)");
    msprint("        ulRecipClass: MAPI_TO\r\n");
    msprint("Returning SUCCESS_SUCCESS\r\n");
    return SUCCESS_SUCCESS;
}

ULONG FAR PASCAL MAPISaveMail(
    LHANDLE lhSession,
    ULONG ulUIParam,
    lpMapiMessage lpMessage,
    FLAGS flFlags,
    ULONG ulReserved,
    LPTSTR lpszMessageID)
{
    if(MSDEBUG){
      fprintf(ms_global->dfd, "\r\nIn MAPISaveMail\r\n");
      fprintf(ms_global->dfd, " MAPI_LOGON_UI is %s set\r\n", 
	      flFlags & MAPI_LOGON_UI ? "" : "NOT");
      fprintf(ms_global->dfd, " MAPI_LONG_MSGID is %s set\r\n", 
	      flFlags & MAPI_LONG_MSGID ? "" : "NOT");
      fprintf(ms_global->dfd, " MAPI_NEW_SESSION is %s set\r\n", 
	      flFlags & MAPI_NEW_SESSION ? "" : "NOT");
      fprintf(ms_global->dfd, " session number is %p\r\n", lhSession);
      _flushall();
    }
    return MAPI_E_NOT_SUPPORTED;
}

ULONG FAR PASCAL MAPISendDocuments(
    ULONG ulUIParam,
    LPTSTR lpszDelimChar,
    LPTSTR lpszFullPaths,
    LPTSTR lpszFileNames,
    ULONG ulReserved)
{
    if(MSDEBUG){
      fprintf(ms_global->dfd, "\r\nIn MAPISendDocuments\r\n");
      fprintf(ms_global->dfd, " lpszDelimChar: %c\r\n", *lpszDelimChar);
      fprintf(ms_global->dfd, " lpszFullPaths: %s\r\n", lpszFullPaths);
      fprintf(ms_global->dfd, " lpszFileNames: %s\r\n", lpszFileNames);
      _flushall();
    }
    
    return send_documents(lpszFullPaths, *lpszDelimChar);
}
