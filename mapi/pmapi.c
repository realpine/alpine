
/*
 * ========================================================================
 * Copyright 2006-2009 University of Washington
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

static	int		xlate_key;

mapi_global_s *ms_global = NULL;
#define SIZEOF_20KBUF (20480)
char         tmp_20k_buf[SIZEOF_20KBUF];

void mm_searched (MAILSTREAM *stream,unsigned long number);
void mm_exists (MAILSTREAM *stream,unsigned long number);
void mm_expunged (MAILSTREAM *stream,unsigned long number);
void mm_flags (MAILSTREAM *stream,unsigned long number);
void mm_notify (MAILSTREAM *stream,char *string,long errflg);
void mm_list (MAILSTREAM *stream,int delimiter,char *name,long attributes);
void mm_lsub (MAILSTREAM *stream,int delimiter,char *name,long attributes);
void mm_status (MAILSTREAM *stream,char *mailbox,MAILSTATUS *status);
void mm_log (char *string,long errflg);
void mm_dlog (char *string);
int fetch_recursively(BODY *body, long msgno, char *prefix, 
		      PART *part, long flags, FLAGS MAPIflags, 
		      sessionlist_s *cs);
LRESULT CALLBACK Login(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void mm_login (NETMBX *mb,char *user,char *pwd,long trial);
void mm_critical (MAILSTREAM *stream);
void mm_nocritical (MAILSTREAM *stream);
long mm_diskerror (MAILSTREAM *stream,long errcode,long serious);
void mm_fatal (char *string);
lpMapiFileDesc new_mapi_file_desc(int arraysize);
void free_mapi_file_desc(lpMapiFileDesc lpmfd, int arraysize);
file_s *new_file_s();
void free_file_s(file_s *fs);
void init_prcvars(mapi_global_s *nmg);
void init_prcfeats(mapi_global_s *nmg);
void expand_env_vars(mapi_global_s *nmg);
void init_fcc_folder(mapi_global_s *nmg);
void init_pmapi_registry_vars(mapi_global_s *nmg);
void init_pmapi_vars(mapi_global_s *nmg);
char *copy_remote_to_local(char *pinerc, sessionlist_s *cs);
int read_pinerc(mapi_global_s *nmg, sessionlist_s *cs, 
		char *prc, char *pineconf, char *pinercex, int depth);
void free_mapi_global(mapi_global_s *nmg);
MAILSTREAM *first_open(sessionlist_s *cs);
int lookup_file_mime_type(char *fn, BODY *body);
int LookupMIMEFileExt(char *val, char *mime_type, DWORD *vallen);
int in_passfile(sessionlist_s *cs);
int set_text_data(BODY *body, char *txt);
int get_suggested_directory(char *dir);
int get_suggested_file_ext(char *file_ext, PART *part, DWORD *file_extlen);
int InitDebug();
int GetPineData();
int UnderlineSpace(char *s);
int GetOutlookVersion();
char *pmapi_generate_message_id();
SENDSTREAM *mapi_smtp_open(sessionlist_s *cs, char **servers, long options);
char *encode_mailto_addr_keyval(lpMapiRecipDesc lpmr);
char *encode_mailto_keyval(char *key, char* val);


void mm_searched (MAILSTREAM *stream,unsigned long number)
{
    if(MSDEBUG){
      fprintf(ms_global->dfd, "IMAP mm_searched: not implemented\r\n");
      _flushall();
    }
}

void mm_exists (MAILSTREAM *stream,unsigned long number)
{
    if(MSDEBUG){
      fprintf(ms_global->dfd, "IMAP mm_exists: not implemented\r\n");
      _flushall();
    }
}

void mm_expunged (MAILSTREAM *stream,unsigned long number)
{
    if(MSDEBUG){
      fprintf(ms_global->dfd, "IMAP mm_expunged: not implemented\r\n");
      _flushall();
    }
}

void mm_flags (MAILSTREAM *stream,unsigned long number)
{
    if(MSDEBUG){
      fprintf(ms_global->dfd, "IMAP mm_flags: number is %d\r\n", number);
      _flushall();
    }
}

void mm_notify (MAILSTREAM *stream,char *string,long errflg)
{
    mapi_global_s *nmg;

    nmg = ms_global;
    if(MSDEBUG){
      fprintf(nmg->dfd, "IMAP mm_notify:%s %s\r\n",
	      (errflg == NIL ? "" : 
	       (errflg == WARN ? " WARN:":
	       (errflg == ERROR ? " ERROR:":
	       (errflg == PARSE ? " PARSE:":" BYE:")))), string);
      _flushall();
    }
}

void mm_list (MAILSTREAM *stream,int delimiter,char *name,long attributes)
{
    if(MSDEBUG){
      fprintf(ms_global->dfd, "IMAP mm_list: not implemented\r\n");
      _flushall();
    }
}

void mm_lsub (MAILSTREAM *stream,int delimiter,char *name,long attributes)
{
    if(MSDEBUG){
      fprintf(ms_global->dfd, "IMAP mm_lsub: not implemented\r\n");
      _flushall();
    }
}

void mm_status (MAILSTREAM *stream,char *mailbox,MAILSTATUS *status)
{
    if(MSDEBUG){
      fprintf(ms_global->dfd, "IMAP mm_status: not implemented\r\n");
      _flushall();
    }
}

void mm_log (char *string,long errflg)
{
    if(MSDEBUG){
      fprintf(ms_global->dfd, "IMAP mm_log:%s %s\r\n",
	      (errflg == NIL ? "" :
	      (errflg == WARN ? " WARN:":
	       (errflg == ERROR ? " ERROR:":
		(errflg == PARSE ? " PARSE:":" BYE:")))), string);
      _flushall();
    }
    if(errflg == ERROR)
      ErrorBox("ERROR: %s", string);
}

void mm_dlog (char *string)
{
    if(MSDEBUG){
      time_t now;
      struct tm *tm_now;

      now = time((time_t *)0);
      tm_now = localtime(&now);

      fprintf(ms_global->dfd, "IMAP %2.2d:%2.2d:%2.2d %d/%d: %s\r\n",
	      tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec,
	      tm_now->tm_mon+1, tm_now->tm_mday, string);
      _flushall();
    }
}

int fetch_recursively(BODY *body, long msgno, char *prefix, PART *part, 
		      long flags, FLAGS MAPIflags, sessionlist_s *cs)
{
    FILE *attfd;
    PART *subpart;
    mapi_global_s *nmg;
    int num = 0, tmplen;
    file_s *tfs, *tfs2;
    unsigned long declen, numwritten;
    void *decf;
    char tmp[64], file_ext[64], filename[1024], dir[1024];
    DWORD file_extlen = 64;
    char *tmpatt;
    unsigned long tmpattlen;
    
    nmg = ms_global;
    tmp[0] = '\0';
    if(body && body->type == TYPEMULTIPART){
      part = body->nested.part;
      fetch_recursively(&part->body, msgno, prefix, part, 
			flags, MAPIflags, cs);
    }
    else{
      do{
	num++;
	sprintf(tmp, "%s%s%d", prefix, (*prefix ? "." : ""), num);
	if(part && part->body.type == TYPEMULTIPART){
	  subpart = part->body.nested.part;
	  fetch_recursively(&subpart->body, msgno, tmp, 
			    subpart, flags, MAPIflags, cs);
	}
	else{
	  tmpatt = mail_fetch_body(cs->open_stream, msgno, 
				   tmp, &tmpattlen, flags);
	  if(strcmp(tmp, "1") == 0){
	    if(((part && part->body.type == TYPETEXT) || 
	       (!part && body->type == TYPETEXT)) && 
	       !(MAPIflags & MAPI_BODY_AS_FILE)){
	      if(cs->lpm->lpszNoteText){
		fs_give((void **)&cs->lpm->lpszNoteText);
		cs->lpm->lpszNoteText = NULL;
	      }
	      cs->lpm->lpszNoteText = mstrdup(tmpatt);
	      tmpatt = NULL;
	    }
	    else
	      cs->lpm->lpszNoteText = mstrdup("");
	    if(MAPIflags & MAPI_SUPPRESS_ATTACH)
	      return 0;
	  }
	  if(tmpatt && part && part->body.encoding == ENCBASE64){
	    decf = rfc822_base64(tmpatt, tmpattlen, &declen);
	    tmpatt = NULL;
	  }
	  else if(tmpatt && part && part->body.encoding == ENCQUOTEDPRINTABLE){
	    decf = rfc822_qprint(tmpatt, tmpattlen, &declen);
	    tmpatt = NULL;
	  }
	  else{
	    if(tmpatt){
	      decf = mstrdup(tmpatt);
	      declen = tmpattlen;
	    }
	    else decf = NULL;
	  }
	  if(decf){
	    dir[0] = '\0';
	    get_suggested_directory(dir);
	    tmplen = strlen(dir);
	    if(dir[tmplen - 1] != '\\'){
	      dir[tmplen] = '\\';
	      dir[tmplen+1] = '\0';
	    }
	    file_ext[0] = '\0';
	    get_suggested_file_ext(file_ext,part, &file_extlen);
	    do{
	      sprintf(filename, "%smapiapp%d%s", dir, nmg->attach_no, 
		      file_ext);
	      nmg->attach_no++;
	    }while (_access(filename, 00) != -1);
	    attfd = fopen(filename, "wb");
	    if(attfd){
	      if(MSDEBUG)
		fprintf(nmg->dfd,"preparing to write attachment to %s\r\n",
			filename);
	      numwritten = fwrite(decf, sizeof(char), declen, attfd);
	      fclose(attfd);
	      fs_give((void **)&decf);
	      tfs = new_file_s();
	      tfs->filename = mstrdup(filename);
	      if(!cs->fs)
		cs->fs = tfs;
	      else{
		for(tfs2 = cs->fs; tfs2->next; tfs2 = tfs2->next);
		tfs2->next = tfs;
	      }
	    }
	    else{
	      if(MSDEBUG)
		fprintf(nmg->dfd,"Failure in opening %s for attachment\r\n", 
			filename);
	    }
	  }
	}
      }while(part && (part = part->next));
    }
    return 0;
}

int fetch_structure_and_attachments(long msgno, long flags, 
				    FLAGS MAPIflags, sessionlist_s *cs)
{
    BODY *body = NULL;
    ENVELOPE *env;
    ADDRESS *addr;
    MESSAGECACHE *elt = NULL;
    mapi_global_s *nmg;
    file_s *tfs;
    int num = 0, restore_seen = 0, file_count = 0, i;
    unsigned long recips;
    char  tmp[1024];  /* don't know how much space we'll need */
    
    nmg = ms_global;
    env = mail_fetch_structure(cs->open_stream, msgno, &body, flags);
    if(env == NULL || body == NULL){
      if(MSDEBUG)
	fprintf(nmg->dfd, "mail_fetch_structure returned %p for env and %p for body\r\n", env, body);
      return MAPI_E_FAILURE;
    }
    if(cs->lpm){
      if(MSDEBUG)
	fprintf(nmg->dfd, "global lpm is set when it SHOULDN'T be! Freeing\r\n");
      if(free_mbuffer(cs->lpm))
	free_MapiMessage(cs->lpm, 1);
      cs->lpm = NULL;
    }
    cs->lpm = new_MapiMessage(1);
    if(env->subject) cs->lpm->lpszSubject = mstrdup(env->subject);
    if(env->date){
      elt = mail_elt(cs->open_stream, msgno);
      mail_parse_date(elt, env->date);
      sprintf(tmp, "%d/%s%d/%s%d %s%d:%s%d",
	      elt->year+BASEYEAR, (elt->month < 10) ? "0": "",
	      elt->month, (elt->day < 10) ? "0":"", elt->day,
	      (elt->hours < 10) ? "0":"", elt->hours,
	      (elt->minutes < 10) ? "0":"", elt->minutes);
      cs->lpm->lpszDateReceived = mstrdup(tmp);
    }
    if(env->from){
      cs->lpm->lpOriginator = new_MapiRecipDesc(1);
      if(env->from->personal)
	cs->lpm->lpOriginator->lpszName = mstrdup(env->from->personal);
      if(env->from->mailbox && env->from->host){
	/* don't know if these could ever be empty */
	sprintf(tmp, "%s@%s", env->from->mailbox, env->from->host);
	cs->lpm->lpOriginator->lpszAddress = mstrdup(tmp);
      }
      cs->lpm->lpOriginator->ulRecipClass = MAPI_ORIG;
    }
    if(env->to || env->cc || env->bcc){ /* should always be true */
      recips = 0;
      if(env->to){
	addr = env->to;
	while(addr){
	  recips++;
	  addr = addr->next;
	}
      }
      if(env->cc){
	addr = env->cc;
	while(addr){
	  recips++;
	  addr = addr->next;
	}
      }
      if(env->bcc){
	addr = env->bcc;
	while(addr){
	  recips++;
	  addr = addr->next;
	}
      }
      cs->lpm->nRecipCount = recips;
      cs->lpm->lpRecips = new_MapiRecipDesc(recips);
      recips = 0;
      if(env->to){
	addr = env->to;
	while(addr){
	  cs->lpm->lpRecips[recips].ulRecipClass = MAPI_TO;
	  if(addr->personal)
	    cs->lpm->lpRecips[recips].lpszName = mstrdup(addr->personal);
	  if(addr->mailbox && addr->host){
	    sprintf(tmp, "%s@%s", addr->mailbox, addr->host);
	    cs->lpm->lpRecips[recips].lpszAddress = mstrdup(tmp);
	  }
	  recips++;
	  addr = addr->next;
	}
      }
      if(env->cc){
	addr = env->cc;
	while(addr){
	  cs->lpm->lpRecips[recips].ulRecipClass = MAPI_CC;
	  if(addr->personal)
	    cs->lpm->lpRecips[recips].lpszName = mstrdup(addr->personal);
	  if(addr->mailbox && addr->host){
	    sprintf(tmp, "%s@%s", addr->mailbox, addr->host);
	    cs->lpm->lpRecips[recips].lpszAddress = mstrdup(tmp);
	  }
	  recips++;
	  addr = addr->next;
	}
      }
      if(env->bcc){
	addr = env->bcc;
	while(addr){
	  cs->lpm->lpRecips[recips].ulRecipClass = MAPI_BCC;
	  if(addr->personal)
	    cs->lpm->lpRecips[recips].lpszName = mstrdup(addr->personal);
	  if(addr->mailbox && addr->host){
	    sprintf(tmp, "%s@%s", addr->mailbox, addr->host);
	    cs->lpm->lpRecips[recips].lpszAddress = mstrdup(tmp);
	  }
	  recips++;
	  addr = addr->next;
	}
      }
    }
    
    if(flags & FT_PEEK){
      /* gotta remember to unflag \Seen if we just want a peek */
      sprintf(tmp, "%d", msgno);
      mail_fetch_flags(cs->open_stream, tmp, NIL);
      elt = mail_elt(cs->open_stream, msgno);
      if(!elt->seen){
	if(MSDEBUG)
	  fprintf(nmg->dfd, "Message has not been seen, and a PEEK is requested\r\n");
	restore_seen = 1;
      }
      else if(MSDEBUG)
	fprintf(nmg->dfd, "Message has already been marked seen\r\n");
    }
    if(!(MAPIflags & MAPI_ENVELOPE_ONLY))
      fetch_recursively(body, msgno, "", NULL, flags, MAPIflags, cs);
    if(cs->fs){
      for(tfs = cs->fs; tfs; tfs = tfs->next)
	file_count++;
      cs->lpm->lpFiles = new_mapi_file_desc(file_count);
      for(i = 0, tfs = cs->fs; i < file_count && tfs; i++, tfs = tfs->next){
	cs->lpm->lpFiles[i].lpszPathName = mstrdup(tfs->filename);
      }
      cs->lpm->nFileCount = file_count;
      free_file_s(cs->fs);
      cs->fs = NULL;
    }
    if(restore_seen){
      elt = mail_elt(cs->open_stream, msgno);
      if(!elt->seen && MSDEBUG)
	fprintf(nmg->dfd, "Fetched body and Message still isn't seen\r\n");
      else if(MSDEBUG)
	fprintf(nmg->dfd, "Message has been seen, clearing flag\r\n");
      if(elt->seen){
	mail_flag(cs->open_stream, tmp, "\\SEEN", NIL);
	elt = mail_elt(cs->open_stream, msgno);
	if(MSDEBUG)
	  fprintf(nmg->dfd, "After calling mail_flag(), elt->seen is %s\r\n",
		  elt->seen ? "SET" : "UNSET");
      }
    }
    return SUCCESS_SUCCESS;
}

LRESULT CALLBACK Login(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    mapi_global_s *nmg;

    nmg = ms_global;

    switch(message){
    case WM_INITDIALOG:
      SetDlgItemText(hDlg, IDC_SERVER, nmg->tmpmbxptr);
      if(/*nmg->flags.mapi_logon_ui &&*/ in_passfile(nmg->cs)){
	SetDlgItemText(hDlg, IDC_LOGIN, nmg->cs->dlge.edit1);
	SetDlgItemText(hDlg, IDC_PASSWORD, nmg->cs->dlge.edit2);
      }
      else
	SetDlgItemText(hDlg, IDC_LOGIN, 
		       (*nmg->cs->mb->user) ? nmg->cs->mb->user 
		       : nmg->prcvars[USER_ID]->val.p);
      return TRUE;
    case WM_COMMAND:
      if(LOWORD(wParam) == IDOK){
	nmg->cs->flags.dlg_cancel = 0;
	GetDlgItemText(hDlg, IDC_LOGIN, nmg->cs->dlge.edit1, EDITLEN);
	GetDlgItemText(hDlg, IDC_PASSWORD, nmg->cs->dlge.edit2, EDITLEN);
	EndDialog(hDlg, LOWORD(wParam));
	return TRUE;
      }
      else if(LOWORD(wParam) == IDCANCEL){
	nmg->cs->flags.dlg_cancel = 1;
	EndDialog(hDlg, LOWORD(wParam));
	return TRUE;
      }
      break;
    }
    return FALSE;
}

void mm_login (NETMBX *mb,char *user,char *pwd,long trial)
{
    mapi_global_s *nmg;
    pw_cache_s *tpwc, *dpwc;
    int tmp_set_mbx = 0;

    nmg = ms_global;
    nmg->cs->mb = mb;
    if(!nmg->cs->flags.dlg_cancel){
	for(tpwc = nmg->cs->pwc; tpwc; tpwc = tpwc->next){
	    if(tpwc->validpw && strcmp(tpwc->host, mb->host) == 0)
	      break;
	}
	if(tpwc){
	    strcpy(user, tpwc->user);
	    strcpy(pwd, tpwc->pwd);
	    tpwc->validpw = 0;
	}
	/* else if(!nmg->cs->flags.check_stream) { */
	else if(nmg->cs->flags.mapi_logon_ui || !nmg->pmapi_strict_no_dialog) {
	    if(!nmg->tmpmbxptr){
		tmp_set_mbx = 1;
		nmg->tmpmbxptr = mb->host;
	    }
	    DialogBox(nmg->mhinst, MAKEINTRESOURCE(IDD_DIALOG1), 
		      nmg->cs->mhwnd, (DLGPROC)Login);
	    if(tmp_set_mbx)
	      nmg->tmpmbxptr = NULL;
	    if(!nmg->cs->flags.dlg_cancel){
		strcpy(user, nmg->cs->dlge.edit1);
		strcpy(pwd, nmg->cs->dlge.edit2);
		tpwc = nmg->cs->pwc;
		while(tpwc){
		    if(tpwc->validpw == 0){
			dpwc = tpwc;
			tpwc = tpwc->next;
			if(dpwc == nmg->cs->pwc)
			  nmg->cs->pwc = dpwc->next;
			fs_give((void **)&dpwc);
		    }
		    else
		      tpwc = tpwc->next;
		}
		if(nmg->cs->pwc == NULL){
		    nmg->cs->pwc = (pw_cache_s *)fs_get(sizeof(pw_cache_s));
		    tpwc = nmg->cs->pwc;
		}
		else {
		    for(tpwc = nmg->cs->pwc; tpwc->next; tpwc = tpwc->next);
		    tpwc->next = (pw_cache_s *)fs_get(sizeof(pw_cache_s));
		    tpwc = tpwc->next;
		}
		memset(tpwc, 0, sizeof(pw_cache_s));
		strncpy(tpwc->user, nmg->cs->dlge.edit1, EDITLEN - 1);
		strncpy(tpwc->pwd, nmg->cs->dlge.edit2, EDITLEN - 1);
		strncpy(tpwc->host, mb->host, EDITLEN - 1);
	    }
	}
    }
    nmg->cs->mb = NULL;
}

void mm_critical (MAILSTREAM *stream)
{
    if(MSDEBUG){
      fprintf(ms_global->dfd, "IMAP mm_critical: not implemented\r\n");
      _flushall();
    }
}

void mm_nocritical (MAILSTREAM *stream)
{
    if(MSDEBUG){
      fprintf(ms_global->dfd, "IMAP mm_nocritical: not implemented\r\n");
      _flushall();
    }
}

long mm_diskerror (MAILSTREAM *stream,long errcode,long serious)
{
    if(MSDEBUG){
      fprintf(ms_global->dfd, "IMAP mm_diskerror: not implemented\r\n");
      _flushall();
    }
    return 1;
}

void mm_fatal (char *string)
{
    if(MSDEBUG){
      fprintf(ms_global->dfd, "IMAP mm_fatal: %s\r\n", string);
      _flushall();
    }
}

sessionlist_s *new_sessionlist()
{
    sessionlist_s *cs;

    cs = (sessionlist_s *)fs_get(sizeof(sessionlist_s));
    memset(cs, 0, sizeof(sessionlist_s));
    cs->session_number = ms_global->next_session++;
    
    return cs;
}

sessionlist_s *free_sessionlist_node(sessionlist_s *cs)
{
    sessionlist_s *ts;

    ts = cs->next;

    if(cs->currently_open)
      fs_give((void **)&cs->currently_open);
    if(cs->fs)
      free_file_s(cs->fs);
    if(cs->lpm){
	if(free_mbuffer(cs->lpm))
	  free_MapiMessage(cs->lpm, 1);
	cs->lpm = NULL;
    }
    fs_give((void **)&cs);
    
    return ts;
}

sessionlist_s *get_session(unsigned long num)
{
    mapi_global_s *nmg = ms_global;
    sessionlist_s *ts;

    ts = nmg->sessionlist;
    while(ts && ts->session_number != num) ts = ts->next;
    return ts;
}

/*
void *mm_cache (MAILSTREAM *stream,unsigned long msgno,long op){}
*/
mapi_global_s *new_mapi_global()
{
    mapi_global_s *nmg;
    int i;
    
    nmg = (mapi_global_s *)fs_get(sizeof(mapi_global_s));
    memset(nmg, 0, sizeof(mapi_global_s));
    for(i=0; i < NUMPRCVARS; i++){
	nmg->prcvars[i] = (rc_entry_s *)fs_get(sizeof(rc_entry_s));
	memset(nmg->prcvars[i], 0, sizeof(rc_entry_s));
    }
    for(i=0; i < NUMPRCFEATS; i++){
	nmg->prcfeats[i] = (rc_feat_s *)fs_get(sizeof(rc_feat_s));
	memset(nmg->prcfeats[i], 0, sizeof(rc_feat_s));
    }
    nmg->next_session = 1;

    return nmg;
}

int InitPineSpecific(sessionlist_s *cs)
{
    mapi_global_s *nmg = ms_global;

    if(nmg->inited) return 0;
    init_prcvars(ms_global);
    init_prcfeats(ms_global);
    init_pmapi_registry_vars(ms_global);
    if(read_pinerc(ms_global, cs, ms_global->pinerc,
		   ms_global->pineconf, ms_global->pinercex, 0) == -1) 
      return -1;
    expand_env_vars(ms_global);
    init_fcc_folder(ms_global);
    msprint1("Fcc folder defined: %s", ms_global->fccfolder);
    init_pmapi_vars(ms_global);
    nmg->inited = 1;
    return 1;
}

int
new_mbuffer(void *buf, int arraysize, BufType type)
{
    MBUFFER_LIST_S *tlist, *tlist2;
    mapi_global_s *nmg = ms_global;

    tlist = (MBUFFER_LIST_S *)fs_get(sizeof(MBUFFER_LIST_S));
    memset(tlist, 0, sizeof(MBUFFER_LIST_S));
    tlist->buf       = buf;
    tlist->arraysize = arraysize;
    tlist->type      = type;

    if(!nmg->mapi_bufs)
      nmg->mapi_bufs = tlist;
    else{
	for(tlist2 = nmg->mapi_bufs; tlist2->next; tlist2 = tlist2->next);
	tlist2->next = tlist;
    }

    return(0);
}

int
free_mbuffer(void *buf)
{
    MBUFFER_LIST_S *tlist, *pre_tlist = NULL;
    mapi_global_s *nmg = ms_global;
    sessionlist_s *session;

    for(tlist = nmg->mapi_bufs; tlist && tlist->buf != buf; pre_tlist = tlist, tlist = tlist->next);
    if(!tlist){
	msprint1("ERROR: buf %p not found in list!\r\n", buf);
	return 1;
    }
    if(tlist == nmg->mapi_bufs)
      nmg->mapi_bufs = tlist->next;
    else
      pre_tlist->next = tlist->next;
    switch (tlist->type) {
      case RecipDesc:
	free_MapiRecipDesc(tlist->buf, tlist->arraysize);
	break;
      case Message:
	for(session = nmg->sessionlist; session; session = session->next){
	    if(session->lpm == tlist->buf)
	      session->lpm = NULL;
	}
	free_MapiMessage(tlist->buf, tlist->arraysize);
	break;
    }
    fs_give((void **)&tlist);
    return 0;
}

lpMapiRecipDesc
new_MapiRecipDesc(int arraysize)
{
    lpMapiRecipDesc lpmrd;
    mapi_global_s *nmg = ms_global;

    lpmrd = (MapiRecipDesc *)fs_get(arraysize*sizeof(MapiRecipDesc));
    memset(lpmrd, 0, arraysize*sizeof(MapiRecipDesc));
    new_mbuffer((void *)lpmrd, arraysize, RecipDesc);
    return lpmrd;
}

void
free_MapiRecipDesc(lpMapiRecipDesc buf, int arraysize)
{
    int i;

    for(i = 0; i < arraysize; i++){
	if(buf[i].lpszName)
	  fs_give((void **)&buf[i].lpszName);
	if(buf[i].lpszAddress)
	  fs_give((void **)&buf[i].lpszAddress);
    }
    fs_give((void **)&buf);
}

lpMapiMessage
new_MapiMessage(int arraysize)
{
    lpMapiMessage lpmm;
    mapi_global_s *nmg = ms_global;

    lpmm = (lpMapiMessage)fs_get(arraysize*sizeof(MapiMessage));
    memset(lpmm, 0, arraysize*sizeof(MapiMessage));
    new_mbuffer((void *)lpmm, arraysize, Message);
    return lpmm;
}

void
free_MapiMessage(lpMapiMessage buf, int arraysize)
{
    int i;

    for(i = 0; i < arraysize; i++){
	if(buf[i].lpszSubject)
	  fs_give((void **)&buf[i].lpszSubject);
	if(buf[i].lpszNoteText)
	  fs_give((void **)&buf[i].lpszNoteText);
	if(buf[i].lpszMessageType)
	  fs_give((void **)&buf[i].lpszMessageType);
	if(buf[i].lpszDateReceived)
	  fs_give((void **)&buf[i].lpszDateReceived);
	if(buf[i].lpszConversationID)
	  fs_give((void **)&buf[i].lpszConversationID);
	if(buf[i].lpOriginator){
	    if(free_mbuffer(buf[i].lpOriginator))
	      free_MapiRecipDesc(buf[i].lpOriginator, 1);
	}
	if(buf[i].lpRecips){
	    if(free_mbuffer(buf[i].lpRecips))
	      free_MapiRecipDesc(buf[i].lpRecips, buf[i].nRecipCount);
	}
	if(buf[i].lpFiles)
	  free_mapi_file_desc(buf[i].lpFiles, buf[i].nFileCount);
    }
    fs_give((void **)&buf);
}

lpMapiFileDesc new_mapi_file_desc(int arraysize)
{
    lpMapiFileDesc lpmfd;

    lpmfd = (MapiFileDesc *)fs_get(arraysize * sizeof (MapiFileDesc));
    memset(lpmfd, 0, arraysize * sizeof(MapiFileDesc));
    return lpmfd;
}

void free_mapi_file_desc(lpMapiFileDesc lpmfd, int arraysize)
{
    int i;
    
    if(lpmfd == NULL) return;
    
    for(i = 0; i < arraysize; i++){
      if(lpmfd[i].lpszPathName)
	fs_give((void **)&lpmfd[i].lpszPathName);
      if(lpmfd[i].lpszFileName)
	fs_give((void **)&lpmfd[i].lpszFileName);
      /* NOTE: if lpFileType gets used, free it here */
    }
    fs_give((void **)&lpmfd);
}

file_s *new_file_s()
{
    file_s *tmp_fs;
    
    tmp_fs = (file_s *)fs_get(sizeof(file_s));
    memset(tmp_fs, 0, sizeof(file_s));
    return tmp_fs;
}

void free_file_s(file_s *fs)
{
    if(fs == NULL) return;
    if(fs->next)
      free_file_s(fs->next);
    if(fs->filename)
      fs_give((void **)&fs->filename);
    fs_give((void **)&fs);
}

void
init_prcvars(mapi_global_s *nmg)
{
    int i=0;

    nmg->prcvars[USER_ID]->var = mstrdup("user-id");
    nmg->prcvars[PERSONAL_NAME]->var = mstrdup("personal-name");
    nmg->prcvars[USER_DOMAIN]->var = mstrdup("user-domain");
    nmg->prcvars[SMTP_SERVER]->var = mstrdup("smtp-server");
    nmg->prcvars[SMTP_SERVER]->islist = 1;
    nmg->prcvars[INBOX_PATH]->var = mstrdup("inbox-path");
    nmg->prcvars[FEATURE_LIST]->var = mstrdup("feature-list");
    nmg->prcvars[FEATURE_LIST]->islist = 1;
    nmg->prcvars[CHARACTER_SET]->var = mstrdup("character-set");
    nmg->prcvars[FOLDER_COLLECTIONS]->var = mstrdup("folder-collections");
    nmg->prcvars[FOLDER_COLLECTIONS]->islist = 1;
    nmg->prcvars[PMAPI_SEND_BEHAVIOR]->var = mstrdup("pmapi-send-behavior");
    nmg->prcvars[PMAPI_SEND_BEHAVIOR]->ispmapivar = 1;
    nmg->prcvars[DEFAULT_FCC]->var = mstrdup("default-fcc");
    nmg->prcvars[PMAPI_SUPPRESS_DIALOGS]->var = mstrdup("pmapi-suppress-dialogs");
    nmg->prcvars[PMAPI_SUPPRESS_DIALOGS]->ispmapivar = 1;
    nmg->prcvars[PMAPI_STRICT_NO_DIALOG]->var = mstrdup("pmapi-strict-no-dialog");
    nmg->prcvars[PMAPI_STRICT_NO_DIALOG]->ispmapivar = 1;
}

void
init_prcfeats(mapi_global_s *nmg)
{
    nmg->prcfeats[ENABLE8BIT]->var = mstrdup("enable-8bit-esmtp-negotiation");
}

void init_fcc_folder(mapi_global_s *nmg)
{
    char *fcc, **fc, *desc = NULL, *col = NULL, *tfcc, *p, *p2;
    int i = 0;

    if(!nmg->prcvars[DEFAULT_FCC]->val.p)
      nmg->prcvars[DEFAULT_FCC]->val.p = cpystr("sent-mail");
    fcc = nmg->prcvars[DEFAULT_FCC]->val.p;
    if(!fcc || !(*fcc)) return;
    if(strcmp(fcc, "\"\"") == 0) return;
    if((*fcc == '{') || (isalpha(fcc[0]) && (fcc[1] == ':'))
       || ((fcc[0] == '\\') && (fcc[1] == '\\'))){
	nmg->fccfolder = cpystr(fcc);
	return;
    }
    fc = nmg->prcvars[FOLDER_COLLECTIONS]->val.l;
    if(!fc || !fc[0] || !fc[0][0]) return;
    get_pair(fc[i], &desc, &col, 0, 0);
    if(desc)
      fs_give((void **)&desc);
    if(!col)
      return;
    p = strrchr(col, '[');
    p2 = strrchr(col, ']');
    if((p2 < p) || (!p)){
	if(col)
	  fs_give((void **)&col);
	return;
    }
    tfcc = (char *)fs_get((strlen(col) + strlen(fcc) + 1) * sizeof(char));
    *p = '\0';
    p2++;
    sprintf(tfcc, "%s%s%s", col, fcc, p2);
    nmg->fccfolder = tfcc;
    if(col)
      fs_give((void **)&col);
}

void init_pmapi_registry_vars(mapi_global_s *nmg)
{
    HKEY hKey;
    BYTE keyData[1024];
    DWORD keyDataSize = 1024;
    DWORD keyDataType;
    int i;

    if(RegOpenKeyEx(HKEY_CURRENT_USER,
		    "Software\\University of Washington\\Alpine\\1.0\\PmapiOpts",
		    0,
		    KEY_QUERY_VALUE,
		    &hKey) != ERROR_SUCCESS)
      return;

    for(i = 0; i < NUMPRCVARS; i++){
	if(nmg->prcvars[i]->ispmapivar && nmg->prcvars[i]->islist == 0){
	    keyDataSize = 1024;
	    if((RegQueryValueEx(hKey, nmg->prcvars[i]->var, 0, &keyDataType,
				keyData, &keyDataSize) == ERROR_SUCCESS)
	       && keyDataType == REG_SZ){
		if(nmg->prcvars[i]->val.p)
		  fs_give((void **)&nmg->prcvars[i]->val.p);
		nmg->prcvars[i]->val.p = mstrdup(keyData);
	    }
	}
    }

    RegCloseKey(hKey);
}

void init_pmapi_vars(mapi_global_s *nmg)
{
    char *b;

    if(b = nmg->prcvars[PMAPI_SEND_BEHAVIOR]->val.p){
	if(_stricmp(b, "always-prompt") == 0)
	  nmg->pmapi_send_behavior = PMSB_ALWAYS_PROMPT;
	else if(_stricmp(b, "always-send") == 0)
	  nmg->pmapi_send_behavior = PMSB_ALWAYS_SEND;
	else if(_stricmp(b, "never-send") == 0)
	  nmg->pmapi_send_behavior = PMSB_NEVER_SEND;
    }
    else
      nmg->pmapi_send_behavior = PMSB_ALWAYS_PROMPT;
    if(b = nmg->prcvars[PMAPI_SUPPRESS_DIALOGS]->val.p){
	if(_stricmp(b, "yes") == 0)
	  nmg->pmapi_suppress_dialogs = PMSD_YES;
	else if(_stricmp(b, "prompt") == 0)
	  nmg->pmapi_suppress_dialogs = PMSD_PROMPT;
	else if(_stricmp(b, "no") == 0)
	  nmg->pmapi_suppress_dialogs = PMSD_NO;
    }
    else
      nmg->pmapi_suppress_dialogs = PMSD_NO;
    if(b = nmg->prcvars[PMAPI_STRICT_NO_DIALOG]->val.p){
	if(_stricmp(b, "yes") == 0)
	  nmg->pmapi_strict_no_dialog = 1;
    }
    else
      nmg->pmapi_strict_no_dialog = 0;
}

char *copy_remote_to_local(char *pinerc, sessionlist_s *cs)
{
    mapi_global_s *nmg = ms_global;
    char *tmptext, dir[1024], filename[1024];
    unsigned long tmptextlen, i = 0, numwritten;
    FILE *prcfd;
 
    if(nmg->cs) return NULL;
    if(!(cs->open_stream = mapi_mail_open(cs, NULL, pinerc, 
				     ms_global->debug ? OP_DEBUG : NIL))){
      ErrorBox("Couldn't open %s for reading as remote pinerc", pinerc);
      return NULL;
    }
    nmg->cs = NULL;
    nmg->tmpmbxptr = NULL;
    tmptext = mail_fetch_body(cs->open_stream, cs->open_stream->nmsgs, 
			      "1", &tmptextlen, NIL); 
    dir[0] = '\0';
    get_suggested_directory(dir);
    do{
      sprintf(filename, "%s%smapipinerc%d", dir, 
	      dir[strlen(dir)-1] == '\\' ? "" : "\\", i);
      i++;
    }while (_access(filename, 00) != -1);
    if(prcfd = fopen(filename, "wb")){
      if(MSDEBUG)
	fprintf(ms_global->dfd,"preparing to write pinerc to %s\r\n",
		filename);
      numwritten = fwrite(tmptext, sizeof(char), tmptextlen, prcfd);
      fclose(prcfd);
    }
    else{
      ErrorBox("Couldn't open temp file %s for writing", filename);
      mail_close_full(cs->open_stream, NIL);
      return NULL;
    }
    cs->open_stream =  mail_close_full(cs->open_stream, NIL);
    return(mstrdup(filename));
}

int read_pinerc(mapi_global_s *nmg, sessionlist_s *cs, 
		char *prc, char *pineconf, char *pinercex, int depth)
{
    FILE *prcfd;
    int i, varnum, j, varlen, create_local = 0, k;
    char line[BUFLEN], *local_pinerc, *p;

    if(nmg == NULL) return -1;
    if(MSDEBUG){
      fprintf(nmg->dfd, 
      "read_pinerc called: prc: %s, pineconf: %s, pinercex: %s, depth: %d\r\n",
	      prc ? prc : "NULL", pineconf ? pineconf : "NULL", 
	      pinercex ? pinercex : "NULL", depth);
    }
    if(pineconf){
      if(MSDEBUG)
	fprintf(nmg->dfd, "Recursively calling read_pinerc for pineconf\r\n");
      read_pinerc(nmg, cs, pineconf, NULL, NULL, 1);
    }
    if(prc == NULL){
      ErrorBox("No value found for %s.  Try running Alpine.", "pinerc");
      DEBUG_WRITE("No value found for %s\r\n","pinerc");
      _flushall();
      return -1;
    }
    if(*prc == '{'){
      if(MSDEBUG) 
	fprintf(ms_global->dfd, "REMOTE PINERC: %s\r\n", prc);
      if(!(local_pinerc = copy_remote_to_local(prc, cs))){
	if(MSDEBUG)
	  fprintf(nmg->dfd, "Couldn't copy remote pinerc to local\r\n");
	return -1;
      }
      create_local = 1;
    }
    else{
      if(!(local_pinerc = mstrdup(prc))){
	ErrorBox("Couldn't fs_get for %s","pinerc");
	return -1;
      }
    }
    if(MSDEBUG)
      fprintf(nmg->dfd, "Preparing to open local pinerc %s\r\n", local_pinerc);
    prcfd = fopen(local_pinerc, "r");
    if(prcfd == NULL){
      DEBUG_WRITE("Couldn't open %s\r\n","pinerc");
      _flushall();
      ErrorBox("Couldn't open %s\r\n","pinerc");
      if(local_pinerc)
	fs_give((void **)&local_pinerc);
      return -1;
    }
    DEBUG_WRITE("Opened %s for reading\r\n", "pinerc");
    for(i = 0; i < NUMPRCVARS && nmg->prcvars[i]->var; i++);
    varnum = i;
    while(fgets(line, BUFLEN, prcfd)){
      j = 0;
      while(isspace(line[j])) j++;
      if(line[j] != '#' && line[j] != '\0'){
	for(i = 0; i < varnum; i ++){
	  varlen = strlen(nmg->prcvars[i]->var);
	  if(_strnicmp(nmg->prcvars[i]->var, line+j, varlen)==0){
	    j += varlen;
	    if(line[j] == '='){
	      /* we found a match in the pinerc */
	      j++;
	      if(nmg->prcvars[i]->islist){
		  while(isspace(line[j])) j++;
		  if(line[j] != '\0'){
		      STRLIST_S *strl = NULL, *tl, *tln;

		      if(nmg->prcvars[i]->val.l){
			  for(k = 0; nmg->prcvars[i]->val.l[k]; k++)
			    fs_give((void **)&nmg->prcvars[i]->val.l[k]);
			  fs_give((void **)&nmg->prcvars[i]->val.l);
		      }
		      strl = (STRLIST_S *)fs_get(sizeof(STRLIST_S));
		      memset(strl, 0, sizeof(STRLIST_S));
		      tl = strl;
		      while(line[j]){
			  while(isspace(line[j])) j++;
			  if(p = strchr(line+j, ','))
			    *p = '\0';
			  if(tl != strl || tl->str){
			      tl->next = (STRLIST_S *)fs_get(sizeof(STRLIST_S));
			      tl = tl->next;
			      memset(tl, 0, sizeof(STRLIST_S));
			  }
			  varlen = strlen(line+j);
			  while(isspace((line+j)[varlen - 1])){
			      varlen--;
			      (line+j)[varlen] = '\0';
			  }
			  tl->str = mstrdup(line+j);
			  if(p){
			      j = p - line + 1;
			      while(isspace(line[j])) j++;
			      if(!line[j]){
				  fgets(line, BUFLEN, prcfd);
				  j = 0;
			      }
			  }
			  else
			    break;
		      }
		      for(tl = strl, k = 0; tl; tl = tl->next, k++);
		      nmg->prcvars[i]->val.l = (char **)fs_get((k+1)*sizeof(char *));
		      for(tl = strl, k = 0; tl; tl = tln, k++){
			  nmg->prcvars[i]->val.l[k] = tl->str;
			  tln = tl->next;
			  fs_give((void **)&tl);
		      }
		      nmg->prcvars[i]->val.l[k] = NULL;
		  }
	      }
	      else{
		  while(isspace(line[j])) j++;
		  if(line[j] != '\0'){
		      varlen = strlen(line+j);
		      while(isspace((line+j)[varlen-1])){
			  varlen--;
			  (line+j)[varlen] = '\0';
		      }
		      if(nmg->prcvars[i]->val.p) fs_give((void **)&nmg->prcvars[i]->val.p);
		      nmg->prcvars[i]->val.p = (char *)fs_get((varlen + 1)*sizeof(char));
		      strncpy(nmg->prcvars[i]->val.p, line+j, varlen);
		      nmg->prcvars[i]->val.p[varlen] = '\0';
		  }
	      }
	    }
	  }
	}
      }
    }
    if(!depth){
      if(pinercex){
	if(MSDEBUG)
	  fprintf(nmg->dfd, 
		  "Recursively calling read_pinerc for exceptions\r\n");
	read_pinerc(nmg, cs, pinercex, NULL, NULL, 1);
      }
    }
    _flushall();
    fclose(prcfd);
    for(i = 0; nmg->prcvars[FEATURE_LIST]->val.l
	  && nmg->prcvars[FEATURE_LIST]->val.l[i]; i++){
	for(j = 0; j < NUMPRCFEATS; j++){
	    if(strcmp(nmg->prcfeats[j]->var, nmg->prcvars[FEATURE_LIST]->val.l[i]) == 0)
	      nmg->prcfeats[j]->is_set = 1;
	    else if((strncmp("no-", nmg->prcvars[FEATURE_LIST]->val.l[i], 3) == 0)
		    && (strcmp(nmg->prcfeats[j]->var, 
			       nmg->prcvars[FEATURE_LIST]->val.l[i] + 3) == 0)){
		nmg->prcfeats[j]->is_set = 0;
	    }
	}
    }
    if(create_local && local_pinerc){
      if(MSDEBUG)
	fprintf(nmg->dfd, "Removing %s\r\n", local_pinerc);
      _unlink(local_pinerc);
    }
    if(local_pinerc)
      fs_give((void **)&local_pinerc);
    if(MSDEBUG){
      fprintf(nmg->dfd, "Current pinerc settings:\r\n");
      for(i = 0; i < varnum; i++){
	  fprintf(nmg->dfd, "%s:", nmg->prcvars[i]->var);
	  if(!nmg->prcvars[i]->islist)
	    fprintf(nmg->dfd, " %s\r\n", 
		    nmg->prcvars[i]->val.p ? nmg->prcvars[i]->val.p : " NOT DEFINED");
	  else{
	      if(!nmg->prcvars[i]->val.l)
		fprintf(nmg->dfd, " NOT DEFINED\r\n");
	      else {
		  for(j = 0; nmg->prcvars[i]->val.l[j]; j++)
		    fprintf(nmg->dfd, "\t%s\r\n", 
			    nmg->prcvars[i]->val.l[j]);
	      }
	  }
      }
    }
    return 0;
}

void expand_env_vars(mapi_global_s *nmg)
{
    int i, j, check_reg = 0, islist;
    DWORD keyDataSize = 1024, keyDataType;
    char *p1, *p2, *p3, keyData[1024], *newstr, **valstrp;
    HKEY hKey;


    if(RegOpenKeyEx(HKEY_CURRENT_USER,
		    "Software\\University of Washington\\Alpine\\1.0\\PmapiOpts\\Env",
		    0,
		    KEY_QUERY_VALUE,
		    &hKey) == ERROR_SUCCESS)
      check_reg = 1;
    for(i = 0; i < NUMPRCVARS; i++){
	islist = nmg->prcvars[i]->islist;
	for(j = 0; islist ? (int)(nmg->prcvars[i]->val.l && nmg->prcvars[i]->val.l[j])
	      : (j < 1); j++){
	    valstrp = islist ? &nmg->prcvars[i]->val.l[j] : &nmg->prcvars[i]->val.p;
	    if(*valstrp == NULL) continue;
	    while((p1 = strstr(*valstrp, "${")) && (p2 = strchr(p1, '}'))){
		msprint1("%s -> ", *valstrp);
		*p2 = '\0';
		p3 = NULL;
		if((p3 = getenv(p1+2)) && *p3)
		  ;
		else if(check_reg && (keyDataSize = 1024)
			&& (RegQueryValueEx(hKey, p1+2, 0, &keyDataType,
					    keyData, &keyDataSize) == ERROR_SUCCESS)
			&& keyDataType == REG_SZ)
		  p3 = keyData;
		newstr = (char *)fs_get(sizeof(char)*(strlen(*valstrp)
						      + strlen(p3 ? p3 : "") + strlen(p2+1) + 1));
		*p1 = '\0';
		strcpy(newstr, *valstrp);
		strcat(newstr, p3 && *p3 ? p3 : "");
		strcat(newstr, p2 + 1);
		fs_give((void **)valstrp);
		*valstrp = newstr;
		msprint1(" %s\r\n", *valstrp);
	    }
	}
    }
    if(check_reg)
      RegCloseKey(hKey);
}

void free_mapi_global(mapi_global_s *nmg)
{
  int i, j;
  sessionlist_s *ts;

  if(nmg->pineExe)
    fs_give((void **)&nmg->pineExe);
  if(nmg->pineExeAlt)
    fs_give((void **)&nmg->pineExeAlt);
  if(nmg->pinerc)
    fs_give((void **)&nmg->pinerc);
  if(nmg->pineconf)
    fs_give((void **)&nmg->pineconf);
  if(nmg->pinercex)
    fs_give((void **)&nmg->pinercex);
  if(nmg->fccfolder)
    fs_give((void **)&nmg->fccfolder);
  if(nmg->attachDir)
    fs_give((void **)&nmg->attachDir);
  for(i = 0; i < NUMPRCFEATS; i++){
      if(nmg->prcfeats[i]->var)
	fs_give((void **)&nmg->prcfeats[i]->var);
      fs_give((void **)&nmg->prcfeats[i]);
  }
  for(i = 0; i < NUMPRCVARS; i++){
    if(nmg->prcvars[i]->var)
      fs_give((void **)&nmg->prcvars[i]->var);
    if(nmg->prcvars[i]->islist){
	if(nmg->prcvars[i]->val.l){
	    for(j = 0; nmg->prcvars[i]->val.l[j]; j++)
	      fs_give((void **)&nmg->prcvars[i]->val.l[j]);
	    fs_give((void **)&nmg->prcvars[i]->val.l);
	}
    }
    else {
	if(nmg->prcvars[i]->val.p)
	  fs_give((void **)&nmg->prcvars[i]->val.p);
    }
    fs_give((void **)&nmg->prcvars[i]);
  }
  for(ts = nmg->sessionlist; ts;){
    if(ts->open_stream)
      ts->open_stream = mail_close_full(ts->open_stream, NIL);
    ts = free_sessionlist_node(ts);
  }
  if(nmg->debugFile)
    fs_give((void **)&nmg->debugFile);
  if(nmg->debug && nmg->dfd)
    fclose(nmg->dfd);
  nmg->debug = FALSE;
  fs_give((void **)&nmg);
}

MAILSTREAM *first_open(sessionlist_s *cs)
{
    mapi_global_s *nmg = ms_global;
  /* cs->mhwnd = (HWND)ulUIParam; 
   *    if(flFlags & MAPI_LOGON_UI)
   *  cs->flags.mapi_logon_ui = TRUE;
   */
  /* if someone is logging in right now, return failure */
    if(nmg->cs) return NULL;  
    if(MSDEBUG)
      fprintf(nmg->dfd, "Opening mailbox for the first time\r\n");
    if(nmg->prcvars[INBOX_PATH]->val.p == NULL){
      ErrorBox("No value set for %s!", "inbox");
      return NULL;
    }
    cs->open_stream = mapi_mail_open(cs, cs->open_stream, nmg->prcvars[INBOX_PATH]->val.p,
				 nmg->debug ? OP_DEBUG : NIL);
    /*    cs->flags.mapi_logon_ui = FALSE; */
    if(cs->open_stream){
      if(cs->currently_open){
	fs_give((void **)&cs->currently_open);
	cs->currently_open = NULL;
      }
      cs->currently_open = mstrdup(nmg->prcvars[INBOX_PATH]->val.p);
      if(nmg->debug)
	mail_debug(cs->open_stream);
      if(MSDEBUG){
	fprintf(nmg->dfd, "returning SUCCESS_SUCCESS\r\n");
	_flushall();
      }
      return cs->open_stream;
    }
    else if(cs->flags.dlg_cancel){
      if(MSDEBUG){
	fprintf(nmg->dfd, "returning MAPI_E_FAILURE\r\n");
	_flushall();
      }
      return NULL;
    }
    else{
      cs->dlge.edit1[0] = '\0';
      cs->dlge.edit2[0] = '\0';
      if(cs->currently_open){
	fs_give((void **)&cs->currently_open);
	cs->currently_open = NULL;
      }
      if(MSDEBUG){
	fprintf(nmg->dfd, "returning MAPI_E_FAILURE\r\n");
	_flushall();
      }
      return NULL;
    }
}

SENDSTREAM *
mapi_smtp_open(sessionlist_s *cs, char **servers, long options)
{
    mapi_global_s *nmg = ms_global;
    SENDSTREAM *newstream;
    pw_cache_s *tpwc, *dpwc;

    nmg->cs = cs;
    nmg->tmpmbxptr = NULL;
    nmg->cs->flags.dlg_cancel = 0;
    newstream = smtp_open(servers, options);
    nmg->cs = NULL;
    nmg->tmpmbxptr = NULL;

    if(newstream){ /* if open stream, valid password */
	for(tpwc = cs->pwc; tpwc; tpwc = tpwc->next)
	  tpwc->validpw = 1;
    }
    else{
	for(tpwc = cs->pwc, dpwc = NULL; tpwc; dpwc = tpwc, tpwc = tpwc->next){
	    if(tpwc->validpw == 0){
		if(tpwc == cs->pwc)
		  cs->pwc = tpwc->next;
		else
		  dpwc->next = tpwc->next;
		fs_give((void **)&tpwc);
	    }
	}
    }
    return(newstream);
}

MAILSTREAM *
mapi_mail_open(sessionlist_s *cs, MAILSTREAM *stream, char *name, long options)
{
    MAILSTREAM *newstream;
    mapi_global_s *nmg = ms_global;
    pw_cache_s *tpwc, *dpwc;

    nmg->cs = cs;
    nmg->tmpmbxptr = name;
    nmg->cs->flags.dlg_cancel = 0;
    newstream = mail_open(stream, name, options);
    nmg->tmpmbxptr = NULL;
    nmg->cs = NULL;

    if(newstream){ /* if open stream, valid password */
	for(tpwc = cs->pwc; tpwc; tpwc = tpwc->next)
	  tpwc->validpw = 1;
    }
    else{
	tpwc = cs->pwc;
	while(tpwc){
	    if(tpwc->validpw == 0){
		dpwc = tpwc;
		tpwc = tpwc->next;
		if(dpwc == cs->pwc)
		  cs->pwc = dpwc->next;
		fs_give((void **)&dpwc);
	    }
	    else
	      tpwc = tpwc->next;
	}
    }

    return (newstream);
}


MAILSTREAM *check_mailstream(sessionlist_s *cs)
{
    mapi_global_s *nmg;
    
    nmg = ms_global;
    
    if(!cs->open_stream){
      return(first_open(cs));
    }
    cs->flags.check_stream = TRUE;
    if(!mail_ping(cs->open_stream)){
      if(nmg->cs) return NULL;
      cs->open_stream = mapi_mail_open(cs, cs->open_stream, 
				  cs->currently_open ? cs->currently_open :
				  nmg->prcvars[INBOX_PATH]->val.p,
				   nmg->debug ? OP_DEBUG : NIL);
      if(!cs->open_stream){
	fs_give((void **)&cs->currently_open);
	cs->currently_open = NULL;
	cs->dlge.edit1[0] = '\0';
	cs->dlge.edit2[0] = '\0';
	cs->flags.check_stream = FALSE;
	return NULL;
      }
    }
    cs->flags.check_stream = FALSE;
    return cs->open_stream;
}

/* pretty much changes a string to an integer,
 * but if it is not a valid message number, then 0 is returned
 */
unsigned long convert_to_msgno(char *msgid)
{
    unsigned long place_holder = 1, msgno = 0;
    int i, len;

    len = strlen(msgid);
    for(i = 0; i < len; i++){
      if(msgid[len-1-i] - '0' < 0 || msgid[len-1-i] - '0' > 9)
	return 0;
      msgno += (msgid[len - 1 - i] - '0')*place_holder;
      place_holder *= 10;
    }
    
    return msgno;
}

/* 
 * Lookup file's mimetype by its file extension
 *  fn - filename
 *  body - body in which to store new type, subtype
 *
 *  A mime type is ALWAYS set
 *
 *  Returns 0 if the file extension was found and mimetype was set accordingly
 *          1 if otherwise
 */
int lookup_file_mime_type(char *fn, BODY *body)
{
    char *p, subkey[1024], val[1024];
    DWORD dtype, vallen = 1024;
    HKEY hKey;
    int i, rv = 0;

    if(body->subtype)
      fs_give((void **)&body->subtype);
    if((p = strrchr(fn, '.')) && p[1]){
	sprintf(subkey, "%.1020s", p);
	if(RegOpenKeyEx(HKEY_CLASSES_ROOT, subkey, 0, KEY_READ, &hKey) == ERROR_SUCCESS){
	    if(RegQueryValueEx(hKey, "Content Type", NULL, &dtype, val, &vallen) == ERROR_SUCCESS){
		RegCloseKey(hKey);
		if((p = strrchr(val, '/')) && p[1]){
		    *(p++) = '\0';
		    body->subtype = mstrdup(p);
		    for(i=0; (i <= TYPEMAX) && body_types[i] && _stricmp(val, body_types[i]); i++);
		    if(i > TYPEMAX)
		      i = TYPEOTHER;
		    else if(!body_types[i])
		      body_types[i] = mstrdup(val);
		    body->type = i;
		    return 0;
		}
	    }
	}
    }
    body->type = TYPEAPPLICATION;
    body->subtype = "octet-stream";
    return 1;
}

int LookupMIMEFileExt(char *val, char *mime_type, DWORD *vallen)
{
    HKEY hKey;
    DWORD dtype;
    LONG rv = !ERROR_SUCCESS;
    char subkey[1024];
    
    sprintf(subkey, "MIME\\Database\\Content Type\\%s", mime_type);
    if(RegOpenKeyEx(HKEY_CLASSES_ROOT, subkey, 0, KEY_READ, &hKey) == ERROR_SUCCESS){
      rv = RegQueryValueEx(hKey,"extension",NULL, &dtype, val, vallen);
      RegCloseKey(hKey);
    }
    
    return(rv);
}

/*
 * xlate_out() - xlate_out the given character
 */
char
xlate_out(c)
    char c;
{
    register int  dti;
    register int  xch;

    if((c >= FIRSTCH) && (c <= LASTCH)){
        xch  = c - (dti = xlate_key);
	xch += (xch < FIRSTCH-TABSZ) ? 2*TABSZ : (xch < FIRSTCH) ? TABSZ : 0;
        dti  = (xch - FIRSTCH) + dti;
	dti -= (dti >= 2*TABSZ) ? 2*TABSZ : (dti >= TABSZ) ? TABSZ : 0;
        xlate_key = dti;
        return(xch);
    }
    else
      return(c);
}

/* return TRUE if the pwd is found, FALSE if not */
int in_passfile(sessionlist_s *cs)
{
    mapi_global_s *nmg;
    char *tf, *tp, *ui[4], tmp[1024], *dir;
    int i, j, n;
    FILE *tfd;

    nmg = ms_global;
    
    if(*nmg->pinerc == '{')
      dir = nmg->pineExe;
    else
      dir = nmg->pinerc;

    /*    if(nmg->flags.passfile_checked) return FALSE; */
    if(!(tf = (char *)fs_get(sizeof(char)*(strlen(dir) + strlen("pine.pwd") + 1)))){
      /* nmg->flags.passfile_checked = TRUE; */
      return FALSE;
    }
    strcpy(tf,dir);
    if(tp = strrchr(tf, '\\')){
      tp++;
      strcpy(tp, "pine.pwd");
    }
    else   /* don't know when this will ever happen */
      strcpy(tf, "pine.pwd");
    if(_access(tf, 00) == 0){
      if(MSDEBUG)
	fprintf(nmg->dfd,"found %s for passwords\r\n", tf);
      if(!(tfd = fopen(tf,"r"))){
	fs_give((void **)&tf);
	/*	nmg->flags.passfile_checked = TRUE; */
	return FALSE;
      }
      else{
	for(n = 0; fgets(tmp, 1024, tfd); n++){
	  /*** do any necessary DEcryption here ***/
	  xlate_key = n;
	  for(i = 0; tmp[i]; i++)
	    tmp[i] = xlate_out(tmp[i]);
	  
	  if(i && tmp[i-1] == '\n')
	    tmp[i-1] = '\0';			/* blast '\n' */

	  ui[0] = ui[1] = ui[2] = ui[3] = NULL;
	  for(i = 0, j = 0; tmp[i] && j < 4; j++){
	    for(ui[j] = &tmp[i]; tmp[i] && tmp[i] != '\t'; i++)
	      ;					
	    
	    if(tmp[i])
	      tmp[i++] = '\0';			
	  }

	  if(ui[0] && ui[1] && ui[2]){
	    if(strcmp(ui[2], cs->mb->host) == 0){
	      if((cs->mb->altflag && ui[3] && *ui[3] == '1')
		 || (!cs->mb->altflag && (!ui[3] || (*ui[3] == '0')))){
		if(strcmp(ui[1], *cs->mb->user ? cs->mb->user 
			  : nmg->prcvars[USER_ID]->val.p) == 0){
		  /* winner */
		  strcpy(cs->dlge.edit1, *cs->mb->user ? cs->mb->user
			 : nmg->prcvars[USER_ID]->val.p);
		  strcpy(cs->dlge.edit2, ui[0]);
		  fclose(tfd);
		  fs_give((void **)&tf);
		  /*		  nmg->flags.passfile_checked = TRUE; */
		  return TRUE;
		}
	      }
	    }
	  }
	}
	fclose(tfd);
	fs_give((void **)&tf);
      }
    }
    else{
      fs_give((void **)&tf);
      /*      nmg->flags.passfile_checked = TRUE; */
      return FALSE;
    }
    /*    nmg->flags.passfile_checked = TRUE; */
    return FALSE;
}

int get_suggested_directory(char *dir)
{
    char *tmpdir;
    
    if(tmpdir = getenv("TEMP")){
      strcpy(dir, tmpdir);
      return TRUE;
    }
    else if(tmpdir = getenv("TMP")){
      strcpy(dir, tmpdir);
      return TRUE;
    }
    else if(ms_global && ms_global->attachDir){
      strcpy(dir, ms_global->attachDir);
      return TRUE;
    }
    else{  /* should NEVER get here */
      strcpy(dir, "C:\\");
      return TRUE;
    }
    return FALSE;
}


/* return TRUE if file_ext is modified, FALSE if not */
int get_suggested_file_ext(char *file_ext, PART *part, DWORD *file_extlen)
{
    char mime_type[1024], *tmp_ext;
    int rv = !ERROR_SUCCESS;
    PARAMETER *param;

    if(part->body.subtype){
      sprintf(mime_type, "%s/%s", body_types[part->body.type], part->body.subtype);
      rv = LookupMIMEFileExt(file_ext, mime_type, file_extlen);
    }
    if(rv == ERROR_SUCCESS)
      return TRUE;
    else{
      param = part->body.parameter;
      while(param && (_stricmp("NAME", param->attribute)))
	param = param->next;
      if(!param){
	if(part->body.type == TYPEMESSAGE){
	  /* don't try to recurse through attached messages yet */
	  strcpy(file_ext, ".txt");
	  return TRUE;
	}
      }
      tmp_ext = strrchr(param->value, (int)'.');
      if(!tmp_ext) return FALSE;
      strcpy(file_ext, tmp_ext);
    }
    return TRUE;
}

/* return -1 for failure */
int InitDebug()
{
    char path[1024];

    if(!ms_global){
      if((ms_global = new_mapi_global()) == NULL) return -1;
    }
    /*
     * if debug file exists, turn on debugging mode
     */
    if(ms_global->debug == 1)  /* debug file already initialized, somehow */
      return 1;
    get_suggested_directory(path);
    if(path[strlen(path-1)] != '\\')
      strcat(path, "\\");
    strcat(path, "mapi_debug.txt");
    if(_access(path, 00) == 0){
      ms_global->debug = 1;
    }
    else{
      get_suggested_directory(path);
      if(path[strlen(path-1)] != '\\')
	strcat(path, "\\");
      strcat(path, "mapisend");
      if(_access(path, 00) == 0){
	ms_global->debug = 1;
      }
    }
    
    if(ms_global->debug){
      ms_global->dfd = fopen(path, "wb");
      if(!ms_global->dfd){
	ErrorBox("MAPISendMail: debug off: can't open debug file %.200s",
		 path);
	ms_global->debug = 0;  /* can't open the file, turn off debugging */
      }
      else if(ms_global->debug == 1){
	ms_global->debugFile = (char *)fs_get((1+strlen(path))*sizeof(char));
	strcpy(ms_global->debugFile, path);
      }
    }
     
    if(ms_global->debug && (ms_global->dfd == NULL))
      ms_global->debug = 0;

    return ms_global->debug;
}

int GetPineData()
{
    HKEY pineKey;
    BYTE pineKeyData[1024];
    DWORD pineKeyDataSize;
    DWORD pineKeyDataType;
    char *defPath = "c:\\pine\\pine.exe";
    char *pineExe = strrchr(defPath, '\\')+1;
    char *freepineExe = NULL;
    char *defAttachDir = "c:\\tmp";
    char *penv = NULL;

    /*
     * get name of and path to pine.exe from registry
     */
    if (RegOpenKeyEx(
		     HKEY_LOCAL_MACHINE,
		     "SOFTWARE\\University of Washington\\Alpine\\1.0",
		     0,
		     KEY_QUERY_VALUE,
		     &pineKey) == ERROR_SUCCESS) {
      pineKeyDataSize = sizeof(pineKeyData);
      if (RegQueryValueEx(
			  pineKey,
			  "PineEXE",
			  0,
			  &pineKeyDataType,
			  pineKeyData,
			  &pineKeyDataSize) == ERROR_SUCCESS) {
	freepineExe = (char *)fs_get((pineKeyDataSize + 1) * sizeof(char));
	if ((pineExe = freepineExe) != NULL) {
	  strcpy(pineExe, pineKeyData);
	}
	else {
	  ErrorBox("MAPISendMail: can't fs_get %d bytes for pineExe",
		   pineKeyDataSize);
	  return 0;
	}
	if (MSDEBUG) {
	  fprintf(ms_global->dfd,"pine.exe pineKeyDataSize: %d\r\n", pineKeyDataSize);
	  fprintf(ms_global->dfd,"pine.exe pineKeyData: %s\r\n", pineKeyData);
	}
      }
      pineKeyDataSize = sizeof(pineKeyData);
      if (RegQueryValueEx(
			  pineKey,
			  "pinedir",
			  0,
			  &pineKeyDataType,
			  pineKeyData,
			  &pineKeyDataSize) == ERROR_SUCCESS) {
	ms_global->pineExe = (char *)fs_get(sizeof(char)*(pineKeyDataSize+strlen(pineExe)));
	if (ms_global->pineExe) {
	  strncpy(ms_global->pineExe, pineKeyData, pineKeyDataSize);
	  strcat(ms_global->pineExe, pineExe);
	}
	else {
	  ErrorBox("MAPISendMail: can't fs_get %d bytes for av[0]",
			 pineKeyDataSize);
	  return 0;
	}
	if (MSDEBUG) {
	      fprintf(ms_global->dfd,"pine.exe pineKeyDataSize: %d\r\n", pineKeyDataSize);
	      fprintf(ms_global->dfd,"pine.exe pineKeyData: %s\r\n", pineKeyData);
	}
      }
      RegCloseKey(pineKey);
    }
    if(!ms_global->pineExe){
      ms_global->pineExe = (char *)fs_get((1+strlen(defPath))*sizeof(char));
      if(!ms_global->pineExe){
	ErrorBox("Couldn't fs_get for %s","pineExe");
	return 0;
      }
      else
	strcpy(ms_global->pineExe, defPath);
    }
    
    if(freepineExe)
      ms_global->pineExeAlt = freepineExe;
    else{
      ms_global->pineExeAlt = (char *)fs_get((strlen(strrchr(defPath, '\\')+1)+1)*sizeof(char));
      if(!ms_global->pineExeAlt){
	ErrorBox("Couldn't fs_get for %s","pineExeAlt");
	return 0;
      }
      else
	strcpy(ms_global->pineExeAlt, strrchr(defPath, '\\')+1);
    }
      
    /*
     * get path to pinerc from registry
     */
    if (RegOpenKeyEx(
		     HKEY_CURRENT_USER,
		     "Software\\University of Washington\\Alpine\\1.0",
		     0,
		     KEY_QUERY_VALUE,
		     &pineKey) == ERROR_SUCCESS) {
      pineKeyDataSize = sizeof(pineKeyData);
      if( RegQueryValueEx(
			  pineKey,
			  "PineRC",
			  0,
			  &pineKeyDataType,
			  pineKeyData,
			  &pineKeyDataSize) == ERROR_SUCCESS) {
	if(*pineKeyData != '{' || ms_global->pineExe)
	  ms_global->attachDir  = (char *)fs_get(sizeof(char)*(*pineKeyData == '{' ?
						 pineKeyDataSize + 1 : 
						 strlen(ms_global->pineExe)+1));
	ms_global->pinerc = (char *)fs_get(pineKeyDataSize);
	if(ms_global->attachDir){
	  char *p;
	  if(*pineKeyData != '{'){
	    strncpy(ms_global->attachDir,  pineKeyData, pineKeyDataSize);
	    ms_global->attachDir[pineKeyDataSize] = '\0';
	  }
	  else
	    strcpy(ms_global->attachDir,  ms_global->pineExe);
	  p = strrchr(ms_global->attachDir, '\\');
	  if (p) *p = '\0';
	}
	if(ms_global->pinerc)
	  strncpy(ms_global->pinerc, pineKeyData, pineKeyDataSize);
	else {
	  ErrorBox("MAPISendMail: can't fs_get %d bytes for pinercPath",
		   pineKeyDataSize);
	  return 0;
	}
	if (MSDEBUG) {
	  fprintf(ms_global->dfd, "pinerc pineKeyDataSize: %d\r\n", pineKeyDataSize);
	  fprintf(ms_global->dfd, "pinerc pineKeyData: %s\r\n", pineKeyData);
	  fprintf(ms_global->dfd, "attachDir: %s\r\n", 
		  ms_global->attachDir ? ms_global->attachDir :
		  "NOT YET DEFINED");
	}
      }
      pineKeyDataSize = sizeof(pineKeyData);
      if( RegQueryValueEx(
	  pineKey,
	  "PineConf",
	  0,
	  &pineKeyDataType,
	  pineKeyData,
	  &pineKeyDataSize) == ERROR_SUCCESS){
	  ms_global->pineconf = mstrdup(pineKeyData);
	  msprint1("ms_global->pineconf: %s (due to Registry setting)\r\n", ms_global->pineconf);
      }
      RegCloseKey(pineKey);
    }

    if(ms_global->attachDir == NULL){
      if(ms_global->attachDir = (char *)fs_get((strlen(defAttachDir)+1)*sizeof(char)))
	strcpy(ms_global->attachDir, defAttachDir);
      else
	ErrorBox("Can't find TEMP directory for %s!","attachments");
    }


    if(penv = getenv("PINERC")){
      if(ms_global->pinerc)
	fs_give((void **)&ms_global->pinerc);
      if(ms_global->pinerc = (char *)fs_get((strlen(penv)+1)*sizeof(char)))
	strcpy(ms_global->pinerc, penv);
      else
	ErrorBox("Couldn't fs_get for %s", "pinerc");
    }
    if(penv = getenv("PINECONF")){
      if(ms_global->pineconf)
	fs_give((void **)&ms_global->pineconf);
      if(ms_global->pineconf = (char *)fs_get((strlen(penv)+1)*sizeof(char)))
	strcpy(ms_global->pineconf, penv);
      else
	ErrorBox("Couldn't fs_get for %s", "pineconf");
    }
    else{
	
    }
    if(penv = getenv("PINERCEX")){
      if(ms_global->pinercex)
	fs_give((void **)&ms_global->pinercex);
      if(ms_global->pinercex = mstrdup(penv))
	strcpy(ms_global->pinercex, penv);
      else
	ErrorBox("Couldn't fs_get for %s", "pinercex");
    }
    if(MSDEBUG){
      fprintf(ms_global->dfd,"ms_global->pineExe: %s\r\n",
	      (ms_global->pineExe) ? ms_global->pineExe : "NULL");
      fprintf(ms_global->dfd,"ms_global->pineExeAlt: %s\r\n", 
	      ms_global->pineExeAlt ? ms_global->pineExeAlt : "NULL");
      fprintf(ms_global->dfd,"ms_global->attachDir: %s\r\n", 
	      ms_global->attachDir ? ms_global->attachDir : "NULL");
      fprintf(ms_global->dfd,"ms_global->pinerc: %s\r\n", 
	      ms_global->pinerc ? ms_global->pinerc : "NULL");
      fprintf(ms_global->dfd,"ms_global->pineconf: %s\r\n", 
	      ms_global->pinerc ? ms_global->pineconf : "NULL");
      fprintf(ms_global->dfd,"ms_global->pinercex: %s\r\n", 
	      ms_global->pinerc ? ms_global->pinercex : "NULL");
    }
    return 1;
}

BOOL APIENTRY DllMain(
    HANDLE hInst,
    DWORD ul_reason_being_called,
    LPVOID lpReserved)
{
    switch(ul_reason_being_called){
    case DLL_THREAD_ATTACH:
      /*      if(ms_global)
       *	return 1;
       */
    case DLL_PROCESS_ATTACH:
      if(!ms_global)
	ms_global = new_mapi_global();
      if(!ms_global) return 0;
      ms_global->attached++;
      ms_global->mhinst = hInst;
      if(InitDebug() == -1){
	ErrorBox("Mapi32.dll could not %s", "initialize");
	return 0;
      }
      if(MSDEBUG && ms_global->attached <= 1){
	time_t now;
	struct tm *tm_now;
	extern char datestamp[], hoststamp[];

	now = time((time_t *)0);
	tm_now = localtime(&now);
	fprintf(ms_global->dfd, "pmapi32.dll for Alpine Version 2.02r\n");
	fprintf(ms_global->dfd, " Build date: %s\r\n", datestamp);
	fprintf(ms_global->dfd,
		" please report all bugs to alpine-contact@u.washington.edu\r\n");
	if(tm_now)
	  fprintf(ms_global->dfd, 
		  "Created: %2.2d:%2.2d:%2.2d %d/%d/%d\r\n",
		  tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec,
		  tm_now->tm_mon+1, tm_now->tm_mday, tm_now->tm_year+1900);

	fprintf(ms_global->dfd, "\r\n\r\n");
      }
      DEBUG_WRITE("%s called. Debug initialized (in DllMain)\r\n",
		  ul_reason_being_called == DLL_PROCESS_ATTACH ?
		  "DLL_PROCESS_ATTACH":"DLL_THREAD_ATTACH");
      GetPineData();
#include "../c-client-dll/linkage.c"
      break;
    case DLL_PROCESS_DETACH:
    case DLL_THREAD_DETACH:
      DEBUG_WRITE("\r\n%s called\r\n", 
		  ul_reason_being_called == DLL_PROCESS_DETACH ? 
		  "DLL_PROCESS_DETACH" : "DLL_THREAD_DETACH");
      ms_global->attached--;
      /*      if(ms_global->open_stream)
       * ms_global->open_stream = mail_close_full(ms_global->open_stream, NIL);
       */
      if(ms_global->attached <= 0 && 
	 ul_reason_being_called == DLL_PROCESS_DETACH){
	if(MSDEBUG)
	  fprintf(ms_global->dfd, 
	   "detaching last thread/process.  freeing mapi global struct\r\n");
	free_mapi_global(ms_global);
      }
      break;
    }
    return 1;
}

static char *V="\r\n@(#) Alpine Simple Mapi Library Ver. 1.3\r\n";

int
UnderlineSpace(char *s)
{
    char *p;
    
    if(p = strrchr(s, '\\'))
      s = p++;

    for(; *s; s++)
      if(*s == ' ')
	*s = '_';
    return 1;
}

/*
 * Given source file name and destination directory, make a binary copy
 * of the file and return the full name of the copy (mangled as necessary
 * to avoid conflicts).  The return value will be a fs_get'd string
 */
char *
TmpCopy(char *srcFile, int is_pinerc)
{
    char *dstName;	/* constructed and fs_get'd full output pathname */
    char *srcTail;	/* last component of source pathname */
    char *srcExt;	/* extension, if any, of srcTail */
    char  dstDir[1024];
    int i, cnt, c, len, spc = 0;
    FILE *sfd, *dfd;

    if (!srcFile) {
	ErrorBox("TmpCopy: srcFile is %s", "NULL");
	return NULL;
	}
    if(is_pinerc){
      len = strlen(srcFile);
      for(i = 0; i < len; i++){
	if(srcFile[i] == ' ') spc = 1;
      }
      if(spc == 0) return mstrdup(srcFile);
    }
      
    get_suggested_directory(dstDir);
    if (!dstDir) {
	ErrorBox("TmpCopy: dstDir is %s", "NULL");
	return NULL;
	}
	
    dstName = (char *)fs_get(sizeof(char)*(strlen(srcFile) + 5 +
			     max(strlen(dstDir), strlen(PINERC_FILE))));

    if (dstName == NULL) {
      ErrorBox("TmpCopy: can't fs_get space %d bytes for dstName",
	       strlen(srcFile)+5+max(strlen(dstDir),strlen(PINERC_FILE)));
      return NULL;
    }

    if(!is_pinerc){
      srcTail = strrchr(srcFile, '\\');
      if (srcTail)
	++srcTail;
      else 
	srcTail = srcFile;
      
      srcExt = strrchr(srcTail, '.');
      
      sfd = fopen(srcFile, "rb");
      if (sfd == NULL) {
	ErrorBox("TmpCopy: can't open %.200s for reading", srcFile);
	fs_give((void **)&dstName);
	return NULL;
      }

      i = sprintf(dstName, "%s%s%s", dstDir, 
		  dstDir[strlen(dstDir)-1] == '\\' ? "" : "\\",
		  srcTail);
      UnderlineSpace(dstName);
      for (cnt = 0; cnt < 1000; ++cnt) {
	int handle = _open(dstName, _O_CREAT|_O_EXCL , _S_IREAD|_S_IWRITE);
	if (handle != -1) {
	  if (_close(handle)) /* this shouldn't be able to happen */
	    ErrorBox("TmpCopy: _close of new %.200s failed", dstName);
	  dfd = fopen(dstName, "wb");
	  if (dfd) break;
	}
	if (srcExt)
	  sprintf(dstName+i-strlen(srcExt), "%03d%s", cnt, srcExt);
	else
	  sprintf(dstName+i, "%03d", cnt);
      }
      if (dfd == NULL) {
	ErrorBox("TmpCopy: can't create anything like %.200s", dstName);
	fclose(sfd);
	fs_give((void **)&dstName);
	return NULL;
      }
    }
    else{  /* is_pinerc */
      i = sprintf(dstName, "%s%s%s", dstDir, 
		  dstDir[strlen(dstDir)-1] == '\\' ? "" : "\\",
		  PINERC_FILE);
      dfd = fopen(dstName, "wb");
      if(!dfd){
	ErrorBox("Couldn't create temp %s for pine", "pinerc");
	fs_give((void **)&dstName);
	return NULL;
      }
      sfd = fopen(srcFile, "rb");
      if (sfd == NULL) {
	ErrorBox("TmpCopy: can't open %.200s for reading", srcFile);
	fclose(dfd);
	fs_give((void **)&dstName);
	return NULL;
      }
    }
    c = fgetc(sfd);
    while(feof(sfd) == 0) {
	putc(c, dfd);
	c = fgetc(sfd);
    }
    if (ferror(dfd)) {
	ErrorBox("TmpCopy: write error on %.200s", dstName);
	fs_give((void **)&dstName);
	fclose(dfd);
	return NULL;
	}
    if (ferror(sfd)) {
	ErrorBox("TmpCopy: read error on %.200s", srcFile);
	fs_give((void **)&dstName);
	fclose(sfd);
	return NULL;
	}
    if (fclose(sfd)) {
	ErrorBox("TmpCopy: fclose error on %.200s", srcFile);
	}
    if (fclose(dfd)) {
	ErrorBox("TmpCopy: fclose error on %.200s", dstName);
	}

    return dstName;
}

int send_documents(char *files, char sep)
{
    int ac, i, tmplen, j;
    char **av, *tmpfiles, *file, *tmpfree;
    mapi_global_s *nmg;

    nmg = ms_global;
    ac = 3;
    tmplen = strlen(files);
    tmpfiles = (char *)fs_get(sizeof(char)*(tmplen + 1));
    strcpy(tmpfiles,files);
    for(i = 0; i <= tmplen; i++){
      if(files[i] == sep || files[i] == '\0')
	ac += 2;
    }
    ac += 2; /* just for safe measure */
    av = (char **)fs_get(ac * sizeof(char *));
    if(nmg->pinerc){
      av[1] = mstrdup("-p");
      /* copy pinerc to temp directory just in case it too 
       * has spaces in its directory
       */
      if(tmpfree = TmpCopy(nmg->pinerc, IS_PINERC)){
	av[2] = quote(tmpfree);
	fs_give((void **)&tmpfree);
      }
      else
	av[2] = quote(nmg->pinerc);
    }
    for(i = 0, j = 3, file = tmpfiles; i <= tmplen; i++){
      if(tmpfiles[i] == sep || i == tmplen){
	tmpfiles[i] = '\0';
	if(i - (file - tmpfiles) > 1){
	  tmpfree = TmpCopy(file, NOT_PINERC);
	  if(tmpfree){
	    av[j++] = mstrdup("-attach_and_delete");
	    av[j++] = quote(tmpfree);
	    fs_give((void **)&tmpfree);
	  }
	}
      }
    }
    av[j] = NULL;
    av[0] = quote(nmg->pineExe);
    if(MSDEBUG){
      fprintf(ms_global->dfd, "spawning %s (else %s):\r\n", 
	      ms_global->pineExe, ms_global->pineExeAlt);
      fprintf(nmg->dfd, " av:\r\n");
      for(i = 0; av[i]; i++)
	fprintf(nmg->dfd, "  av[%d]: %s\r\n", i, av[i]);
    }

    /* clean up quote()'s */
    if (_spawnvp(_P_NOWAIT, ms_global->pineExe, av) == -1 &&
	_spawnvp(_P_NOWAIT, ms_global->pineExeAlt, av) == -1){
      ErrorBox("MAPISendMail: _spawnvp of %s failed", ms_global->pineExe);
      if(MSDEBUG) 
	fprintf(ms_global->dfd, "_spawnvp %s and %s failed\r\n", 
		ms_global->pineExe,ms_global->pineExeAlt);
      return(MAPI_E_FAILURE);
    }
    for(i = 0; av[i]; i++)
      fs_give((void **)&av[i]);
    fs_give((void **)&av);
    return SUCCESS_SUCCESS;
}

char *
message_structure_to_mailto_url(lpMapiMessage lpm)
{
    char **keyvals, **keyvalp, *url;
    int keyvallen;
    unsigned long i, url_len = 0;

    if(lpm == NULL)
      return NULL;

    keyvallen = lpm->nRecipCount + 4; /* subject + body + from + recips + NULL */
    keyvals = (char **)fs_get(keyvallen * sizeof(char *));
    keyvalp = keyvals;

    for(i = 0; i < lpm->nRecipCount; i++)
      *keyvalp++ = encode_mailto_addr_keyval(&lpm->lpRecips[i]);
    if(lpm->lpszSubject)
      *keyvalp++ = encode_mailto_keyval("subject", lpm->lpszSubject);
    if(lpm->lpOriginator)
      *keyvalp++ = encode_mailto_addr_keyval(lpm->lpOriginator);
    if(lpm->lpszNoteText)
      *keyvalp++ = encode_mailto_keyval("body", lpm->lpszNoteText);
    *keyvalp = NULL;

    if(*keyvals == NULL){
	fs_give((void **)&keyvals);
	return(NULL);
    }

    url_len = keyvallen + 10; /* mailto url extra chars */
    for(keyvalp = keyvals; *keyvalp; keyvalp++)
      url_len += strlen(*keyvalp);

    url = (char *)fs_get(url_len * sizeof(char));
    sprintf(url, "mailto:?");
    for(keyvalp = keyvals; *keyvalp; keyvalp++){
	strcat(url, *keyvalp);
	if(*(keyvalp+1))
	  strcat(url, "&");
	fs_give((void **)&(*keyvalp));
    }
    fs_give((void **)&keyvals);
    return url;
}

char *
encode_mailto_addr_keyval(lpMapiRecipDesc lpmr)
{
    ADDRESS *adr = NULL;
    char *addr, *retstr;
    int use_quotes = 0;

    adr = mapirecip2address(lpmr);
    addr = (char *)fs_get((size_t)est_size(adr));
    addr[0] = '\0';
    rfc822_write_address(addr, adr);
    mail_free_address(&adr);

    retstr = encode_mailto_keyval(lpmr->ulRecipClass == MAPI_CC ? "cc"
				  : (lpmr->ulRecipClass == MAPI_BCC ? "bcc"
				     : (lpmr->ulRecipClass == MAPI_ORIG ? "from"
					: "to")), 
				  addr);
    fs_give((void **)&addr);
    return(retstr);
}



/*
 * Hex conversion aids from alpine.h
 */
#define HEX_ARRAY	"0123456789ABCDEF"
#define	HEX_CHAR1(C)	HEX_ARRAY[((C) & 0xf0) >> 4]
#define	HEX_CHAR2(C)	HEX_ARRAY[(C) & 0xf]

/* strings.c macros */
#define	C2XPAIR(C, S)	{ \
			    *(S)++ = HEX_CHAR1(C); \
			    *(S)++ = HEX_CHAR2(C); \
			}

#define	RFC1738_SAFE	"$-_.+"			/* "safe" */
#define	RFC1738_EXTRA	"!*'(),"		/* "extra" */

/* adapted from rfc1738_encode_mailto */
char *
encode_mailto_keyval(char *key, char* val)
{
    char *d, *ret = NULL, *v = val;
    
    if(key && val){
	ret = (char *)fs_get(sizeof(char) * (strlen(key) + (3*strlen(val)) + 2));
	strcpy(ret, key);
	d = ret + strlen(key);
	*d++ = '=';
	while(*v){
	    if(isalnum((unsigned char)*v)
	       || strchr(RFC1738_SAFE, *v)
	       || strchr(RFC1738_EXTRA, *v))
	      *d++ = *v++;
	    else{
		*d++ = '%';
		C2XPAIR(*v, d);
		v++;
	    }
	}
	*d = '\0';
    }

    return(ret);
}

unsigned long
send_msg_nodlg(LHANDLE lhSession, ULONG ulUIParam, lpMapiMessage lpMessage, 
	       FLAGS flFlags, ULONG ulReserved)
{
    sessionlist_s *cs;
    int tsession = 0, orig_in_recip = 0;
    unsigned long i, orig_index;
    ADDRESS *tadr = NULL, *tadr2 = NULL;
    ENVELOPE *env = NULL;
    BODY *body = NULL;
    mapi_global_s *nmg = ms_global;
    SENDSTREAM *sending_stream = NULL;
    unsigned long rv;
    time_t now;
    struct tm *tm_now;
    char *p = NULL;

    if(nmg->pmapi_send_behavior == PMSB_NEVER_SEND)
      return MAPI_E_USER_ABORT;
    else if(nmg->pmapi_send_behavior == PMSB_ALWAYS_PROMPT){
	if(MessageBox(NULL, "Really Send Message?", "pmapi32.dll", MB_YESNO|MB_ICONQUESTION) == IDNO)
	  return MAPI_E_USER_ABORT;
    }
    if((flFlags & MAPI_NEW_SESSION) || lhSession == 0){
	cs = new_sessionlist();
	tsession = 1;
    }
    else{
	cs = get_session(lhSession);
	if(!cs)
	  return MAPI_E_INVALID_SESSION;
    }
    cs->flags.mapi_logon_ui = (flFlags & MAPI_LOGON_UI) || (flFlags & MAPI_DIALOG) ? 1 : 0;
    if(InitPineSpecific(cs) == -1){
	rv = MAPI_E_LOGIN_FAILURE;
	goto smn_cleanup;
    }
    msprint("Preparing to Send Message with no dialogs...\r\n");
    /* Make an envelope */
    env = (ENVELOPE *)fs_get(sizeof(ENVELOPE));
    memset(env, 0, sizeof(ENVELOPE));
    if(lpMessage->lpszSubject){
      p = rfc1522_encode(tmp_20k_buf, SIZEOF_20KBUF, lpMessage->lpszSubject,
			 nmg->prcvars[CHARACTER_SET]->val.p);
      env->subject = mstrdup(p);      
      if(MSDEBUG)
	fprintf(ms_global->dfd, " Subject: %s\r\n", env->subject);
    }
    /*
     * Since it is "DateReceived", I think the right thing to do is ignore it,
     * since we're sending, not receiving.
     */
    rfc822_date(tmp_20k_buf);
    env->date = mstrdup(tmp_20k_buf);
    msprint1(" Date: %s\r\n", env->date);
    env->message_id = pmapi_generate_message_id();
    msprint1(" Message-Id: %s\r\n", env->message_id);

    for(i = 0; i < lpMessage->nRecipCount; i++){
	if((&lpMessage->lpRecips[i])->ulRecipClass == MAPI_ORIG){
	    orig_in_recip = 1;
	    orig_index = i;
	}
    }

    if(lpMessage->lpOriginator || orig_in_recip){
	if((env->from = mapirecip2address(lpMessage->lpOriginator
					  ? lpMessage->lpOriginator
					  : (&lpMessage->lpRecips[orig_index])))
	   == NULL){
	    rv = MAPI_E_INVALID_RECIPS;
	    goto smn_cleanup;
	}
	if(MSDEBUG){
	    sprintf(tmp_20k_buf, "%.100s <%.100s@%.100s>", env->from->personal ? env->from->personal
		    : "", env->from->mailbox ? env->from->mailbox : "",
		    env->from->host ? env->from->host : "");
	    msprint1("From: %s\r\n", tmp_20k_buf);
	}
    }
    else if(nmg->prcvars[USER_ID]->val.p && nmg->prcvars[USER_DOMAIN]->val.p){
	/* 
	 * judgment call: I guess we'll try to generate the from header if it's not
	 * given to us
	 */
	env->from = mail_newaddr();
	if(nmg->prcvars[PERSONAL_NAME]->val.p){
	    p = rfc1522_encode(tmp_20k_buf, SIZEOF_20KBUF, nmg->prcvars[PERSONAL_NAME]->val.p,
			       nmg->prcvars[CHARACTER_SET]->val.p);
	  env->from->personal = mstrdup(p);
	}
	env->from->mailbox = mstrdup(nmg->prcvars[USER_ID]->val.p);
	env->from->host = mstrdup(nmg->prcvars[USER_DOMAIN]->val.p);
	if(MSDEBUG){
	    sprintf(tmp_20k_buf, "%.100s <%.100s@%.100s>", env->from->personal ? env->from->personal
		    : "", env->from->mailbox ? env->from->mailbox : "",
		    env->from->host ? env->from->host : "");
	    msprint1("From: %s\r\n", tmp_20k_buf);
	}
    }
    for(i = 0; i < lpMessage->nRecipCount; i++){
	if((tadr = mapirecip2address(&lpMessage->lpRecips[i])) == NULL){
	    rv = MAPI_E_INVALID_RECIPS;
	    goto smn_cleanup;
	}
	switch (lpMessage->lpRecips[i].ulRecipClass) {
	  case MAPI_TO:
	  case MAPI_ORIG:
	    if(!env->to)
	      env->to = tadr;
	    else{
		for(tadr2 = env->to; tadr2->next; tadr2 = tadr2->next);
		tadr2->next = tadr;
	    }
	    msprint(" To: ");
	    break;
	  case MAPI_CC:
	    if(!env->cc)
	      env->cc = tadr;
	    else{
		for(tadr2 = env->cc; tadr2->next; tadr2 = tadr2->next);
		tadr2->next = tadr;
	    }
	    msprint(" Cc: ");
	    break;
	  case MAPI_BCC:
	    if(!env->bcc)
	      env->bcc = tadr;
	    else{
		for(tadr2 = env->bcc; tadr2->next; tadr2 = tadr2->next);
		tadr2->next = tadr;
	    }
	    msprint(" Bcc: ");
	    break;
	  default:
	    rv = MAPI_E_INVALID_RECIPS;
	    goto smn_cleanup;
	    break;
	}
	if(MSDEBUG){
	    sprintf(tmp_20k_buf, "%.100s <%.100s@%.100s>", tadr->personal ? tadr->personal
		    : "", tadr->mailbox ? tadr->mailbox : "",
		    tadr->host ? tadr->host : "");
	    msprint1("%s\r\n", tmp_20k_buf);
	}
    }
    /* Now we have an envelope, let's make us a body */
    if(lpMessage->lpszNoteText  == NULL)
      msprint("Empty Message Text\r\n");
    body = mail_newbody();
    if(lpMessage->nFileCount){
	PART *p;
	struct _stat sbuf;
	unsigned long fsize, n;
	FILE *sfd;
	char c, *fn;

	msprint1("Number of files to be Attached: %d\r\n", (void *)lpMessage->nFileCount);
	body->type = TYPEMULTIPART;
	body->nested.part = mail_newbody_part();
	p = body->nested.part;
	set_text_data(&p->body, lpMessage->lpszNoteText ? lpMessage->lpszNoteText
		      : "");

	for(i = 0; i < lpMessage->nFileCount; i++){
	    p->next = mail_newbody_part();
	    p = p->next;
	    p->body.encoding = ENCBINARY;
	    if(lpMessage->lpFiles[i].lpszPathName == NULL)
	      return(MAPI_E_FAILURE);
	    if(lpMessage->lpFiles[i].lpszFileName == NULL
	       || lpMessage->lpFiles[i].lpszFileName[0] == '\0'){
		fn = strrchr(lpMessage->lpFiles[i].lpszPathName, '\\');
		if(!fn)
		  fn = lpMessage->lpFiles[i].lpszPathName;
		else
		  fn++;
	    }
	    else
	      fn = lpMessage->lpFiles[i].lpszFileName;
	    if(lookup_file_mime_type(fn, &p->body) && (fn == lpMessage->lpFiles[i].lpszFileName))
	      lookup_file_mime_type(lpMessage->lpFiles[i].lpszPathName, &p->body);
	    msprint1(" Attaching file %s;", lpMessage->lpFiles[i].lpszPathName);
	    if(_stat(lpMessage->lpFiles[i].lpszPathName, &sbuf))
	      return(MAPI_E_FAILURE);
	    fsize = sbuf.st_size;
	    if((sfd = fopen(lpMessage->lpFiles[i].lpszPathName, "rb")) == NULL)
	      return(MAPI_E_FAILURE);
	    p->body.contents.text.data = fs_get((fsize+1)*sizeof(char));
	    n = 0;
	    c = fgetc(sfd);
	    while(feof(sfd) == 0){
		p->body.contents.text.data[n++] = c;
		if(n > fsize){
		    fsize += 20000;
		    fs_resize((void **)&p->body.contents.text.data, (fsize+1)*sizeof(char));
		}
		c = fgetc(sfd);
	    }
	    fclose(sfd);
	    p->body.contents.text.data[n] = '\0';
	    p->body.contents.text.size = n;
	    msprint1(" File size: %d\r\n", (void *)n);
	    if(fn){
		p->body.parameter = mail_newbody_parameter();
		p->body.parameter->attribute = mstrdup("name");
		p->body.parameter->value = mstrdup(fn);

		p->body.disposition.type = mstrdup("attachment");
		p->body.disposition.parameter = mail_newbody_parameter();
		p->body.disposition.parameter->attribute = mstrdup("filename");
		p->body.disposition.parameter->value = mstrdup(fn);
	    }
	}
    }
    else {
	set_text_data(body, lpMessage->lpszNoteText ? lpMessage->lpszNoteText
		      : "");
	msprint1(" Message Body size: %d\r\n", (void *)body->contents.text.size);
    }

    if(MSDEBUG){
	now = time((time_t *)0);
	tm_now = localtime(&now);
	fprintf(ms_global->dfd, "%2.2d:%2.2d:%2.2d %d/%d/%d ",
		tm_now->tm_hour, tm_now->tm_min, tm_now->tm_sec,
		tm_now->tm_mon+1, tm_now->tm_mday, tm_now->tm_year+1900);
    }
    if(nmg->prcvars[SMTP_SERVER]->val.l && nmg->prcvars[SMTP_SERVER]->val.l[0]
       && nmg->prcvars[SMTP_SERVER]->val.l[0][0]){
	if(MSDEBUG){
	    fprintf(ms_global->dfd, "Preparing to open SMTP connection (%s ...)\r\n",
		    nmg->prcvars[SMTP_SERVER]->val.l[0]);
	    _flushall();
	}
	sending_stream = mapi_smtp_open(cs, nmg->prcvars[SMTP_SERVER]->val.l,
				   nmg->prcfeats[ENABLE8BIT]->is_set ? SOP_8BITMIME : NIL);
    }
    else {
	rv = MAPI_E_FAILURE;
	if(MSDEBUG){
	    fprintf(ms_global->dfd, "Error! No SMTP server defined!\r\n");
	}
	goto smn_cleanup;
    }
    if(!sending_stream){
	rv = MAPI_E_FAILURE;
	if(MSDEBUG){
	    fprintf(ms_global->dfd, "Couldn't open SMTP connection!\r\n");
	}
	goto smn_cleanup;
    }
    if(!smtp_mail(sending_stream, "MAIL", env, body)){
	if(MSDEBUG){
	    fprintf(ms_global->dfd, "Attempt to Send Failed\r\n");
	}
	rv = MAPI_E_FAILURE;
    }
    else {
	if(MSDEBUG){
	    fprintf(ms_global->dfd, "Message SENT!\r\n");
	}
	if(!nmg->fccfolder)
	  msprint("No fcc defined\r\n");
	else {    /* Now try to write to fcc */
	    STRBUFFER_S *sb;
	    MAILSTREAM *fccstream;
	    STRING msg;

	    msprint1("FCCing to %s\r\n", nmg->fccfolder);
	    sb = (STRBUFFER_S *)fs_get(sizeof(STRBUFFER_S));
	    sb->buf = (char *)fs_get(20000*sizeof(char));
	    sb->cur_bytes = 0;
	    sb->increment = 20000;
	    sb->bufsize = 20000;
	    rfc822_output(tmp_20k_buf, env, body, pmapi_soutr, sb, 1);
	    INIT(&msg, mail_string, (void *)sb->buf, sb->cur_bytes);
	    fccstream = mapi_mail_open(cs, NULL, nmg->fccfolder, ms_global->debug ? OP_DEBUG : NIL);
	    if(fccstream){
		if(mail_append(fccstream, nmg->fccfolder, &msg) == NIL)
		  msprint1("Fcc to %s failed\r\n", nmg->fccfolder);
		else
		  msprint1("Fcc to %s SUCCEEDED\r\n", nmg->fccfolder);
		mail_close(fccstream);
	    }
	    else 
	      msprint1("Open of %s failed, abandoning FCC\r\n", nmg->fccfolder);
	    if(sb->buf)
	      fs_give((void **)&sb->buf);
	    if(sb)
	      fs_give((void **)&sb);
	}
	if((flFlags & MAPI_LOGON_UI) || (flFlags & MAPI_DIALOG))
	  MessageBox(NULL, "Message SENT!\r\n", "pmapi32.dll", MB_OK|MB_ICONINFORMATION);
	rv = SUCCESS_SUCCESS;
    }
    smtp_close(sending_stream);
 smn_cleanup:
    if(env)
      mail_free_envelope(&env);
    if(body)
      mail_free_body(&body);
    if(tsession)
      fs_give((void **)&cs);
    return(rv);
}

int
set_text_data(BODY *body, char *txt)
{
    char *p;
    int has_8bit = 0;
    PARAMETER *pm;

    for(p = txt; *p; p++)
      if(*p & 0x80)
	has_8bit++;

    body->contents.text.data = mstrdup(txt);
    body->contents.text.size = strlen(txt);
    if(has_8bit){
	body->encoding = ENC8BIT;
	if(!body->parameter)
	  pm = body->parameter = mail_newbody_parameter();
	else {
	    for(pm = body->parameter; pm->next; pm = pm->next);
	    pm->next = mail_newbody_parameter();
	    pm = pm->next;
	}
	pm->attribute = mstrdup("charset");
	if(ms_global->prcvars[CHARACTER_SET]->val.p)
	  pm->value = mstrdup(ms_global->prcvars[CHARACTER_SET]->val.p);
	else
	  pm->value = mstrdup("X-UNKNOWN");
    }
    return 0;
}

long
pmapi_soutr(STRBUFFER_S *s, char *str)
{
    unsigned long i;

    if(s->cur_bytes >= s->bufsize){
	fs_resize((void **)&s->buf, s->bufsize+ s->increment);
	s->bufsize += s->increment;
    }
    for(i = 0; str[i]; i++){
	s->buf[s->cur_bytes++] = str[i];
	if(s->cur_bytes >= s->bufsize){
	    fs_resize((void **)&s->buf, s->bufsize+ s->increment);
	    s->bufsize += s->increment;
	}
    }
    s->buf[s->cur_bytes] = '\0';
    return T;
}

ADDRESS *
mapirecip2address(lpMapiRecipDesc lpmrd)
{
    ADDRESS *adr = NULL;
    static char *fakedomain = "@", *p;
    mapi_global_s *nmg = ms_global;

    if(!lpmrd->lpszAddress)
      return(NULL);
    rfc822_parse_adrlist(&adr, _strnicmp(lpmrd->lpszAddress, "SMTP:", 5) == 0
			 ? lpmrd->lpszAddress + 5 : lpmrd->lpszAddress,
			 nmg->prcvars[USER_DOMAIN]->val.p
			 ? nmg->prcvars[USER_DOMAIN]->val.p : fakedomain);
    if(!adr)
      return(NULL);
    if(adr->next || adr->error){
	mail_free_address(&adr);
	return(NULL);
    }

    if(lpmrd->lpszName && adr->personal)
      fs_give((void **)&adr->personal);
    if(lpmrd->lpszName){
	p = rfc1522_encode(tmp_20k_buf, SIZEOF_20KBUF, lpmrd->lpszName,
			   nmg->prcvars[CHARACTER_SET]->val.p);
	adr->personal = mstrdup(p);
    }
    return(adr);
}

/*
 * given a fs_get'd string, return a newly fs_get'd quoted copy
 */
char *
quote(char *old)
{
    char *new, *newp, *oldp;
    int newSize = strlen(old)*2+3;

    if (!old) return mstrdup(old);
    if(!strchr(old, ' ')) return mstrdup(old);

    newp = new = (char *)fs_get(sizeof(char)*newSize);	/* worst case */
    if (new == NULL) {
	ErrorBox("quote: fs_get of %d bytes failed", newSize);
	return old;
    }

    *newp++ = '"';
    for (oldp=old; *oldp; ++oldp) {
	switch(*oldp) {
	    case '"':  *newp++ = '\\';		/* fall through */
	    default :  *newp++ = *oldp;
	}
    }
    *newp++ = '"';
    *newp = '\0';
    return(new);
}


int GetPCPineVersion(int *major, int *minor, int *minorminor)
{
    *major = 4;
    *minor = 52;
    *minorminor = 0;

    return 1;
}

int
msprint(char *str)
{
    if(MSDEBUG)
      fprintf(ms_global->dfd, "%s", str);
    _flushall();
    return 0;
}

int
msprint1(char *str, void *arg1)
{
    if(MSDEBUG)
      fprintf(ms_global->dfd, str, arg1);
    _flushall();
    return 0;
}

int 
msprint_message_structure(lpMapiMessage lpm)
{
    unsigned long i;

    if(MSDEBUG){
	msprint1("lpMapiMessage: %p\r\n", lpm);
	if(!lpm)
	  return 1;
	msprint1(" ulReserved: %d\r\n", (void *)lpm->ulReserved);
	msprint1(" lpszSubsect: %s\r\n", lpm->lpszSubject ? lpm->lpszSubject : "(NULL)");
	msprint1(" lpszNoteText size: %d\r\n", lpm->lpszNoteText 
		 ? (void *)strlen(lpm->lpszNoteText) : (void *)0);
	if(lpm->lpszNoteText)
	  msprint1("\tleading text: %.10s\r\n", lpm->lpszNoteText);
	msprint1(" lpszMessageType: %s\r\n", lpm->lpszMessageType ? lpm->lpszMessageType : "(NULL)");
	msprint1(" lpszDateReceived: %s\r\n", lpm->lpszDateReceived ? lpm->lpszDateReceived : "(NULL)");
	msprint1(" lpszConversationID: %s\r\n", lpm->lpszConversationID ? lpm->lpszConversationID : "(NULL)");
	msprint1(" flFlags: %d\r\n", (void *)lpm->flFlags);
	msprint(" Originator:\r\n");
	msprint_recipient_structure(lpm->lpOriginator, 0);
	msprint1(" nRecipCount: %d\r\n", (void *)lpm->nRecipCount);
	for(i = 0; i < lpm->nRecipCount; i++)
	  msprint_recipient_structure(&lpm->lpRecips[i], 1);
	msprint1(" nFileCount: %d\r\n", (void *)lpm->nFileCount);
	for(i = 0; i < lpm->nFileCount; i++)
	  msprint_file_structure(&lpm->lpFiles[i]);
	msprint("\r\n");
    }
    return 0;
}

int
msprint_recipient_structure(lpMapiRecipDesc lmrd, int mapi_orig_is_unexpected)
{
    if(MSDEBUG){
	msprint1(" lpMapiRecipDesc: %p\r\n", (void *)lmrd);
	if(lmrd == NULL)
	  return 1;
	msprint1(" ulReserved: %d\r\n", (void *)lmrd->ulReserved);
	msprint1(" ulRecipClass: %s\r\n", lmrd->ulRecipClass == MAPI_ORIG ? "MAPI_ORIG"
		 : lmrd->ulRecipClass == MAPI_TO ? "MAPI_TO" : lmrd->ulRecipClass == MAPI_CC
		 ? "MAPI_CC" : "MAPI_BCC");
	if(mapi_orig_is_unexpected && lmrd->ulRecipClass == MAPI_ORIG){
	    msprint(" # NOTE: it is seemingly strange behavior that a MAPI client would use\r\n #       MAPI_ORIG instead of the lpOriginator.  This may result in unexpected behavior.\r\n");
	}
	msprint1(" lpszName: %s\r\n", lmrd->lpszName ? lmrd->lpszName : "(NULL)");
	msprint1(" lpszAddress: %s\r\n", lmrd->lpszAddress ? lmrd->lpszAddress : "(NULL)");
	msprint1(" ulEIDSize: %p\r\n", (void *)lmrd->ulEIDSize);
	msprint1(" lpEntryID: %p\r\n", (void *)lmrd->lpEntryID);
    }
    return 0;
}


int
msprint_file_structure(lpMapiFileDesc lmfd)
{
    if(MSDEBUG){
	msprint1(" lpMapiFileDesc: %p\r\n", (void *)lmfd);
	if(lmfd == NULL)
	  return 1;
	msprint1(" ulReserved: %d\r\n", (void *)lmfd->ulReserved);
	msprint1(" flFlags: %d\r\n", (void *)lmfd->flFlags);
	msprint1(" nPosition: %d\r\n", (void *)lmfd->nPosition);
	msprint1(" lpszPathName: %s\r\n", lmfd->lpszPathName ? lmfd->lpszPathName : "(NULL)");
	msprint1(" lpszFileName: %s\r\n", lmfd->lpszFileName ? lmfd->lpszFileName : "(NULL)");
	msprint1(" lpFileType: %p\r\n", (void *)lmfd->lpFileType);
    }
    return 0;
}


char *mstrdup(char *old)
{
    char *tmp;
  
    tmp = fs_get((strlen(old)+1) * sizeof(char));
    strcpy(tmp, old);

    return tmp;
}

int
est_size(a)
    ADDRESS *a;
{
    int cnt = 0;

    for(; a; a = a->next){

	/* two times personal for possible quoting */
	cnt   += 2 * (a->personal  ? strlen(a->personal)  : 0);
	cnt   += (a->mailbox  ? strlen(a->mailbox)  : 0);
	cnt   += (a->adl      ? strlen(a->adl)      : 0);
	cnt   += (a->host     ? strlen(a->host)     : 0);

	/*
	 * add room for:
         *   possible single space between fullname and addr
         *   left and right brackets
         *   @ sign
         *   possible : for route addr
         *   , <space>
	 *
	 * So I really think that adding 7 is enough.  Instead, I'll add 10.
	 */
	cnt   += 10;
    }

    return(max(cnt, 50));  /* just making sure */
}

void ErrorBoxFunc(char *msg)
{
    if(MSDEBUG){
      fprintf(ms_global->dfd,"ErrorBox: %s\r\n", msg);
      fclose(ms_global->dfd);
      ms_global->dfd = fopen(ms_global->debugFile, "ab");
      if(!ms_global->dfd){
	MessageBox(NULL, "debug file problems! Debugging turned off.", 
		   "mapi32.dll", 
		   MB_OK|MB_ICONERROR);  
	ms_global->debug = 0;
	fs_give((void **)&ms_global->debugFile);
	ms_global->debugFile = NULL;
      }
    }
    MessageBox(NULL, msg, "mapi32.dll", MB_OK|MB_ICONERROR);  
}

/*----------------------------------------------------------------------
  This was borrowed from reply.c, and modified
        Generate a unique message id string.

   Args: ps -- The usual pine structure

  Result: Alloc'd unique string is returned

Uniqueness is gaurenteed by using the host name, process id, date to the
second and a single unique character
*----------------------------------------------------------------------*/
char *
pmapi_generate_message_id()
{
    static short osec = 0, cnt = 0;
    char        *id;
    time_t       now;
    struct tm   *now_x;

    now   = time((time_t *)0);
    now_x = localtime(&now);
    id    = (char *)fs_get(128 * sizeof(char));

    if(now_x->tm_sec == osec){
	cnt++;
    }else{
	cnt = 0;
	osec = now_x->tm_sec;
    }
    sprintf(id,"<Pmapi32.%04d%02d%02d%02d%02d%02d%X.%d@%.50s>",
	    (now_x->tm_year) + 1900, now_x->tm_mon + 1,
	    now_x->tm_mday, now_x->tm_hour, now_x->tm_min, now_x->tm_sec, 
	    cnt, getpid(), mylocalhost());

    return(id);
}
