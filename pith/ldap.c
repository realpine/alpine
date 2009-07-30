#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: ldap.c 1204 2009-02-02 19:54:23Z hubert@u.washington.edu $";
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

#include "../pith/headers.h"
#include "../pith/ldap.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/status.h"
#include "../pith/util.h"
#include "../pith/imap.h"
#include "../pith/busy.h"
#include "../pith/signal.h"
#include "../pith/ablookup.h"


/*
 * Until we can think of a better way to do this. If user exits from an
 * ldap address selection screen we want to return -1 so that call_builder
 * will stay on the same line. Might want to use another return value
 * for the builder so that call_builder more fully understands what we're
 * up to.
 */
int wp_exit;
int wp_nobail;


#ifdef	ENABLE_LDAP
/*
 * Hook to allow user input on whether or not to save chosen LDAP result
 */
void (*pith_opt_save_ldap_entry)(struct pine *, LDAP_CHOOSE_S *, int);


/*
 * Internal prototypes
 */
LDAP_SERV_RES_S *ldap_lookup(LDAP_SERV_S *, char *, CUSTOM_FILT_S *, WP_ERR_S *, int);
LDAP_SERV_S     *copy_ldap_serv_info(LDAP_SERV_S *);
int              our_ldap_get_lderrno(LDAP *, char **, char **);
int              our_ldap_set_lderrno(LDAP *, int, char *, char *);
#endif	/* ENABLE_LDAP */


/*
 * This function does white pages lookups.
 *
 * Args  string -- the string to use in the lookup
 *
 * Returns     NULL -- lookup failed
 *          Address -- A single address is returned if lookup was successfull.
 */
ADDRESS *
wp_lookups(char *string, WP_ERR_S *wp_err, int recursing)
{
    ADDRESS *ret_a = NULL;
    char ebuf[200];
#ifdef	ENABLE_LDAP
    LDAP_SERV_RES_S *free_when_done = NULL;
    LDAPLookupStyle style;
    LDAP_CHOOSE_S *winning_e = NULL;
    LDAP_SERV_S *info = NULL;
    static char *fakedomain = "@";
    char *tmp_a_string;
    int   auwe_rv = 0;

    /*
     * Runtime ldap lookup of addrbook entry.
     */
    if(!strncmp(string, RUN_LDAP, LEN_RL)){
	LDAP_SERV_RES_S *head_of_result_list;

	info = break_up_ldap_server(string+LEN_RL);
        head_of_result_list = ldap_lookup(info, "", NULL, wp_err, 1);

	if(head_of_result_list){
	    if(!wp_exit)
	      auwe_rv = ask_user_which_entry(head_of_result_list, string,
					     &winning_e,
					     wp_err,
					     wp_err->wp_err_occurred
					     ? DisplayIfOne : DisplayIfTwo);

	    if(auwe_rv != -5)
	      free_when_done = head_of_result_list;
	}
	else{
	    wp_err->wp_err_occurred = 1;
	    if(wp_err->error){
		q_status_message(SM_ORDER, 3, 5, wp_err->error);
		display_message('x');
		fs_give((void **)&wp_err->error);
	    }

	    /* try using backup email address */
	    if(info && info->mail && *info->mail){
		tmp_a_string = cpystr(info->mail);
		rfc822_parse_adrlist(&ret_a, tmp_a_string, fakedomain);
		fs_give((void **)&tmp_a_string);

		wp_err->error =
		  cpystr(_("Directory lookup failed, using backup email address"));
	    }
	    else{
		/*
		 * Do this so the awful LDAP: ... string won't show up
		 * in the composer. This shouldn't actually happen in
		 * real life, so we're not too concerned about it. If we
		 * were we'd want to recover the nickname we started with
		 * somehow, or something like that.
		 */
		ret_a = mail_newaddr();
		ret_a->mailbox = cpystr("missing-username");
		wp_err->error = cpystr(_("Directory lookup failed, no backup email address available"));
	    }

	    q_status_message(SM_ORDER, 3, 5, wp_err->error);
	    display_message('x');
	}
    }
    else{
	style = F_ON(F_COMPOSE_REJECTS_UNQUAL, ps_global)
			? DisplayIfOne : DisplayIfTwo;
	auwe_rv = ldap_lookup_all(string, as.n_serv, recursing, style, NULL,
				  &winning_e, wp_err, &free_when_done);
    }

    if(winning_e && auwe_rv != -5){
	ret_a = address_from_ldap(winning_e);

	if(pith_opt_save_ldap_entry && ret_a && F_ON(F_ADD_LDAP_TO_ABOOK, ps_global) && !info)
	  (*pith_opt_save_ldap_entry)(ps_global, winning_e, 1);


	fs_give((void **)&winning_e);
    }

    /* Info's only set in the RUN_LDAP case */
    if(info){
	if(ret_a && ret_a->host){
	    ADDRESS *backup = NULL;

	    if(info->mail && *info->mail)
	      rfc822_parse_adrlist(&backup, info->mail, fakedomain);

	    if(!backup || !address_is_same(ret_a, backup)){
		if(wp_err->error){
		    q_status_message(SM_ORDER, 3, 5, wp_err->error);
		    display_message('x');
		    fs_give((void **)&wp_err->error);
		}

		snprintf(ebuf, sizeof(ebuf),
	       _("Warning: current address different from saved address (%s)"),
		   info->mail);
		wp_err->error = cpystr(ebuf);
		q_status_message(SM_ORDER, 3, 5, wp_err->error);
		display_message('x');
	    }

	    if(backup)
	      mail_free_address(&backup);
	}

	free_ldap_server_info(&info);
    }

    if(free_when_done)
      free_ldap_result_list(&free_when_done);
#endif	/* ENABLE_LDAP */

    if(ret_a){
	if(ret_a->mailbox){  /* indicates there was a MAIL attribute */
	    if(!ret_a->host || !ret_a->host[0]){
		if(ret_a->host)
		  fs_give((void **)&ret_a->host);

		ret_a->host = cpystr("missing-hostname");
		wp_err->wp_err_occurred = 1;
		if(wp_err->error)
		  fs_give((void **)&wp_err->error);

		wp_err->error = cpystr(_("Missing hostname in LDAP address"));
		q_status_message(SM_ORDER, 3, 5, wp_err->error);
		display_message('x');
	    }

	    if(!ret_a->mailbox[0]){
		if(ret_a->mailbox)
		  fs_give((void **)&ret_a->mailbox);

		ret_a->mailbox = cpystr("missing-username");
		wp_err->wp_err_occurred = 1;
		if(wp_err->error)
		  fs_give((void **)&wp_err->error);

		wp_err->error = cpystr(_("Missing username in LDAP address"));
		q_status_message(SM_ORDER, 3, 5, wp_err->error);
		display_message('x');
	    }
	}
	else{
	    wp_err->wp_err_occurred = 1;

	    if(wp_err->error)
	      fs_give((void **)&wp_err->error);

	    snprintf(ebuf, sizeof(ebuf), _("No email address available for \"%s\""),
		    (ret_a->personal && *ret_a->personal)
			    ? ret_a->personal
			    : "selected entry");
	    wp_err->error = cpystr(ebuf);
	    q_status_message(SM_ORDER, 3, 5, wp_err->error);
	    display_message('x');
	    mail_free_address(&ret_a);
	    ret_a = NULL;
	}
    }

    return(ret_a);
}


#ifdef	ENABLE_LDAP

/*
 * Goes through all servers looking up string.
 *
 * Args  string -- String to search for
 *          who -- Which servers to look on
 *        style -- Sometimes we want to display no matter what, sometimes
 *                  only if more than one entry, sometimes only if email
 *                  addresses, ...
 *
 *     The possible styles are:
 *      AlwaysDisplayAndMailRequired: This happens when user ^T's from
 *                                    composer to address book screen, and
 *                                    then queries directory server.
 *                     AlwaysDisplay: This happens when user is browsing
 *                                    in address book maintenance screen and
 *                                    then queries directory server.
 *                      DisplayIfOne: These two come from implicit lookups
 *                      DisplayIfTwo: from build_address. If the compose rejects
 *                                    unqualified feature is on we get the
 *                                    DisplayIfOne, otherwise IfTwo.
 *
 *         cust -- Use this custom filter instead of configured filters
 *    winning_e -- Return value
 *       wp_err -- Error handling
 * free_when_done -- Caller needs to free this
 *
 * Returns  -- value returned by ask_user_which_entry
 */
int
ldap_lookup_all(char *string, int who, int recursing, LDAPLookupStyle style,
		CUSTOM_FILT_S *cust, LDAP_CHOOSE_S **winning_e,
		WP_ERR_S *wp_err, LDAP_SERV_RES_S **free_when_done)
{
    int              retval = -1;
    LDAP_SERV_RES_S *head_of_result_list = NULL;

    wp_exit = wp_nobail = 0;
    if(free_when_done)
      *free_when_done = NULL;

    head_of_result_list = ldap_lookup_all_work(string, who, recursing, 
					       cust, wp_err);

    if(!wp_exit)
      retval = ask_user_which_entry(head_of_result_list, string, winning_e,
				    wp_err,
				    (wp_err->wp_err_occurred &&
				     style == DisplayIfTwo) ? DisplayIfOne
							    : style);

    /*
     * Because winning_e probably points into the result list
     * we need to leave the result list alone and have the caller
     * free it after they are done with winning_e.
     */
    if(retval != -5 && free_when_done)
      *free_when_done = head_of_result_list;

    return(retval);
}


/*
 * Goes through all servers looking up string.
 *
 * Args  string -- String to search for
 *          who -- Which servers to look on
 *         cust -- Use this custom filter instead of configured filters
 *       wp_err -- Error handling
 *
 * Returns  -- list of results that needs to be freed by caller
 */
LDAP_SERV_RES_S *
ldap_lookup_all_work(char *string, int who, int recursing, 
		     CUSTOM_FILT_S *cust, WP_ERR_S *wp_err)
{
    int              i;
    LDAP_SERV_RES_S *serv_res;
    LDAP_SERV_RES_S *rr, *head_of_result_list = NULL;

    /* If there is at least one server */
    if(ps_global->VAR_LDAP_SERVERS && ps_global->VAR_LDAP_SERVERS[0] &&
       ps_global->VAR_LDAP_SERVERS[0][0]){
	int how_many_servers;

	for(i = 0; ps_global->VAR_LDAP_SERVERS[i] &&
		   ps_global->VAR_LDAP_SERVERS[i][0]; i++)
	  ;

	how_many_servers = i;

	/* For each server in list */
	for(i = 0; !wp_exit && ps_global->VAR_LDAP_SERVERS[i] &&
			       ps_global->VAR_LDAP_SERVERS[i][0]; i++){
	    LDAP_SERV_S *info;

	    dprint((6, "ldap_lookup_all_work: lookup on server (%.256s)\n",
		    ps_global->VAR_LDAP_SERVERS[i]));
	    info = NULL;
	    if(who == -1 || who == i || who == as.n_serv)
	      info = break_up_ldap_server(ps_global->VAR_LDAP_SERVERS[i]);

	    /*
	     * Who tells us which servers to look on.
	     * Who == -1 means all servers.
	     * Who ==  0 means server[0].
	     * Who ==  1 means server[1].
	     * Who == as.n_serv means query on those with impl set.
	     */
	    if(!(who == -1 || who == i ||
		 (who == as.n_serv && !recursing && info && info->impl) ||
		 (who == as.n_serv && recursing && info && info->rhs))){

		if(info)
		  free_ldap_server_info(&info);
		
		continue;
	    }

	    dprint((6, "ldap_lookup_all_work: ldap_lookup (server: %.20s...)(string: %s)\n",
		    ps_global->VAR_LDAP_SERVERS[i], string));
	    serv_res = ldap_lookup(info, string, cust,
				   wp_err, how_many_servers > 1);
	    if(serv_res){
		/* Add new one to end of list so they come in the right order */
		for(rr = head_of_result_list; rr && rr->next; rr = rr->next)
		  ;
		
		if(rr)
		  rr->next = serv_res;
		else
		  head_of_result_list = serv_res;
	    }

	    if(info)
	      free_ldap_server_info(&info);
	}
    }

    return(head_of_result_list);
}


/*
 * Do an LDAP lookup to the server described in the info argument.
 *
 * Args      info -- LDAP info for server.
 *         string -- String to lookup.
 *           cust -- Possible custom filter description.
 *         wp_err -- We set this is we get a white pages error.
 *  name_in_error -- Caller sets this if they want us to include the server
 *                   name in error messages.
 *
 * Returns  Results of lookup, NULL if lookup failed.
 */
LDAP_SERV_RES_S *
ldap_lookup(LDAP_SERV_S *info, char *string, CUSTOM_FILT_S *cust,
	    WP_ERR_S *wp_err, int name_in_error)
{
    char     ebuf[900];
    char     buf[900];
    char    *serv, *base, *serv_errstr;
    char    *mailattr, *snattr, *gnattr, *cnattr;
    int      we_cancel = 0, we_turned_on = 0;
    LDAP_SERV_RES_S *serv_res = NULL;
    LDAP *ld;
    long  pwdtrial = 0L;
    int   ld_errnum;
    char *ld_errstr;


    if(!info)
      return(serv_res);

    serv = cpystr((info->serv && *info->serv) ? info->serv : "?");

    if(name_in_error)
      snprintf(ebuf, sizeof(ebuf), " (%s)",
	      (info->nick && *info->nick) ? info->nick : serv);
    else
      ebuf[0] = '\0';

    serv_errstr = cpystr(ebuf);
    base = cpystr(info->base ? info->base : "");

    if(info->port < 0)
      info->port = LDAP_PORT;

    if(info->type < 0)
      info->type = DEF_LDAP_TYPE;

    if(info->srch < 0)
      info->srch = DEF_LDAP_SRCH;
	
    if(info->time < 0)
      info->time = DEF_LDAP_TIME;

    if(info->size < 0)
      info->size = DEF_LDAP_SIZE;

    if(info->scope < 0)
      info->scope = DEF_LDAP_SCOPE;

    mailattr = (info->mailattr && info->mailattr[0]) ? info->mailattr
						     : DEF_LDAP_MAILATTR;
    snattr = (info->snattr && info->snattr[0]) ? info->snattr
						     : DEF_LDAP_SNATTR;
    gnattr = (info->gnattr && info->gnattr[0]) ? info->gnattr
						     : DEF_LDAP_GNATTR;
    cnattr = (info->cnattr && info->cnattr[0]) ? info->cnattr
						     : DEF_LDAP_CNATTR;

    /*
     * We may want to keep ldap handles open, but at least for
     * now, re-open them every time.
     */

    dprint((3, "ldap_lookup(%s,%d)\n", serv ? serv : "?", info->port));

    snprintf(ebuf, sizeof(ebuf), "Searching%s%s%s on %s",
	    (string && *string) ? " for \"" : "",
	    (string && *string) ? string : "",
	    (string && *string) ? "\"" : "",
	    serv);
    we_turned_on = intr_handling_on();		/* this erases keymenu */
    we_cancel = busy_cue(ebuf, NULL, 0);
    if(wp_err->mangled)
      *(wp_err->mangled) = 1;

#ifdef _SOLARIS_SDK
    if(info->tls || info->tlsmust)
      ldapssl_client_init(NULL, NULL);
    if((ld = ldap_init(serv, info->port)) == NULL)
#else
#if (LDAPAPI >= 11)
    if((ld = ldap_init(serv, info->port)) == NULL)
#else
    if((ld = ldap_open(serv, info->port)) == NULL)
#endif
#endif
    {
      /* TRANSLATORS: All of the three args together are an error message */
      snprintf(ebuf, sizeof(ebuf), _("Access to LDAP server failed: %s%s(%s)"),
	      errno ? error_description(errno) : "",
	      errno ? " " : "",
	      serv);
      wp_err->wp_err_occurred = 1;
      if(wp_err->error)
	fs_give((void **)&wp_err->error);

      wp_err->error = cpystr(ebuf);
      if(we_cancel)
        cancel_busy_cue(-1);

      q_status_message(SM_ORDER, 3, 5, wp_err->error);
      display_message('x');
      dprint((2, "%s\n", ebuf));
    }
    else if(!ps_global->intr_pending){
      int proto = 3, tlsmustbail = 0;
      char pwd[NETMAXPASSWD], user[NETMAXUSER];
      char *passwd = NULL;
      char hostbuf[1024];
      NETMBX mb;
#ifndef _WINDOWS
      int rc;
#endif

      memset(&mb, 0, sizeof(mb));

#ifdef _SOLARIS_SDK
      if(info->tls || info->tlsmust)
	rc = ldapssl_install_routines(ld);
#endif

      if(ldap_v3_is_supported(ld) &&
	 our_ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION, &proto) == 0){
	dprint((5, "ldap: using version 3 protocol\n"));
      }

      /*
       * If we don't set RESTART then the select() waiting for the answer
       * in libldap will be interrupted and stopped by our busy_cue.
       */
      our_ldap_set_option(ld, LDAP_OPT_RESTART, LDAP_OPT_ON);

      /*
       * If we need to authenticate, get the password. We are not
       * supporting SASL authentication, just LDAP simple.
       */
      if(info->binddn && info->binddn[0]){
	  char pmt[500];
	  char *space;

	  snprintf(hostbuf, sizeof(hostbuf), "{%s}dummy", info->serv ? info->serv : "?");

	  /*
	   * We don't handle multiple space-delimited hosts well.
	   * We don't know which we're asking for a password for.
	   * We're not connected yet so we can't know.
	   */
	  if((space=strindex(hostbuf, ' ')) != NULL)
	    *space = '\0';

	  mail_valid_net_parse_work(hostbuf, &mb, "ldap");
	  mb.port = info->port;
	  mb.tlsflag = (info->tls || info->tlsmust) ? 1 : 0;

try_password_again:

	  if(mb.tlsflag
	     && (pwdtrial > 0 || 
#ifndef _WINDOWS
#ifdef _SOLARIS_SDK
		 (rc == LDAP_SUCCESS)
#else /* !_SOLARIS_SDK */
		 ((rc=ldap_start_tls_s(ld, NULL, NULL)) == LDAP_SUCCESS)
#endif /* !_SOLARIS_SDK */
#else /* _WINDOWS */
		 0  /* TODO: find a way to do this in Windows */
#endif /* _WINDOWS */
		 ))
	    mb.tlsflag = 1;
	  else
	    mb.tlsflag = 0;

	  if((info->tls || info->tlsmust) && !mb.tlsflag){
	    q_status_message(SM_ORDER, 3, 5, "Not able to start TLS encryption for LDAP server");
	    if(info->tlsmust)
	      tlsmustbail++;
	  }

	  if(!tlsmustbail){
	      snprintf(pmt, sizeof(pmt), "  %s", (info->nick && *info->nick) ? info->nick : serv);
	      mm_login_work(&mb, user, pwd, pwdtrial, pmt, info->binddn);
	      if(pwd && pwd[0])
		passwd = pwd;
	  }
      }


      /*
       * LDAPv2 requires the bind. v3 doesn't require it but we want
       * to tell the server we're v3 if the server supports v3, and if the
       * server doesn't support v3 the bind is required.
       */
      if(tlsmustbail || ldap_simple_bind_s(ld, info->binddn, passwd) != LDAP_SUCCESS){
	wp_err->wp_err_occurred = 1;

	ld_errnum = our_ldap_get_lderrno(ld, NULL, &ld_errstr);

        if(!tlsmustbail && info->binddn && info->binddn[0] && pwdtrial < 2L
	   && ld_errnum == LDAP_INVALID_CREDENTIALS){
	  pwdtrial++;
          q_status_message(SM_ORDER, 3, 5, _("Invalid password"));
	  goto try_password_again;
	}

	snprintf(ebuf, sizeof(ebuf), _("LDAP server failed: %s%s%s%s"),
		ldap_err2string(ld_errnum),
		serv_errstr,
		(ld_errstr && *ld_errstr) ? ": " : "",
		(ld_errstr && *ld_errstr) ? ld_errstr : "");

        if(wp_err->error)
	  fs_give((void **)&wp_err->error);

        if(we_cancel)
          cancel_busy_cue(-1);

	ldap_unbind(ld);
        wp_err->error = cpystr(ebuf);
        q_status_message(SM_ORDER, 3, 5, wp_err->error);
        display_message('x');
	dprint((2, "%s\n", ebuf));
      }
      else if(!ps_global->intr_pending){
	int          srch_res, args, slen, flen;
#define TEMPLATELEN 512
	char         filt_template[TEMPLATELEN + 1];
	char         filt_format[2*TEMPLATELEN + 1];
	char         filter[2*TEMPLATELEN + 1];
	char         scp[2*TEMPLATELEN + 1];
	char        *p, *q;
	LDAPMessage *res = NULL;
	int intr_happened = 0;
	int tl;

	tl = (info->time == 0) ? info->time : info->time + 10;

	our_ldap_set_option(ld, LDAP_OPT_TIMELIMIT, &tl);
	our_ldap_set_option(ld, LDAP_OPT_SIZELIMIT, &info->size);

	/*
	 * If a custom filter has been passed in and it doesn't include a
	 * request to combine it with the configured filter, then replace
	 * any configured filter with the passed in filter.
	 */
	if(cust && cust->filt && !cust->combine){
	    if(info->cust)
	      fs_give((void **)&info->cust);
	    
	    info->cust = cpystr(cust->filt);
	}

	if(info->cust && *info->cust){	/* use custom filter if present */
	    strncpy(filt_template, info->cust, sizeof(filt_template));
	    filt_template[sizeof(filt_template)-1] = '\0';
	}
	else{				/* else use configured filter */
	    switch(info->type){
	      case LDAP_TYPE_SUR:
		snprintf(filt_template, sizeof(filt_template), "(%s=%%s)", snattr);
		break;
	      case LDAP_TYPE_GIVEN:
		snprintf(filt_template, sizeof(filt_template), "(%s=%%s)", gnattr);
		break;
	      case LDAP_TYPE_EMAIL:
		snprintf(filt_template, sizeof(filt_template), "(%s=%%s)", mailattr);
		break;
	      case LDAP_TYPE_CN_EMAIL:
		snprintf(filt_template, sizeof(filt_template), "(|(%s=%%s)(%s=%%s))", cnattr,
			mailattr);
		break;
	      case LDAP_TYPE_SUR_GIVEN:
		snprintf(filt_template, sizeof(filt_template), "(|(%s=%%s)(%s=%%s))",
			snattr, gnattr);
		break;
	      case LDAP_TYPE_SEVERAL:
		snprintf(filt_template, sizeof(filt_template),
			"(|(%s=%%s)(%s=%%s)(%s=%%s)(%s=%%s))",
			cnattr, mailattr, snattr, gnattr);
		break;
	      default:
	      case LDAP_TYPE_CN:
		snprintf(filt_template, sizeof(filt_template), "(%s=%%s)", cnattr);
		break;
	    }
	}

	/* just copy if custom */
	if(info->cust && *info->cust)
	  info->srch = LDAP_SRCH_EQUALS;

	p = filt_template;
	q = filt_format;
	memset((void *)filt_format, 0, sizeof(filt_format));
	args = 0;
	while(*p && (q - filt_format) + 4 < sizeof(filt_format)){
	    if(*p == '%' && *(p+1) == 's'){
		args++;
		switch(info->srch){
		  /* Exact match */
		  case LDAP_SRCH_EQUALS:
		    *q++ = *p++;
		    *q++ = *p++;
		    break;

		  /* Append wildcard after %s */
		  case LDAP_SRCH_BEGINS:
		    *q++ = *p++;
		    *q++ = *p++;
		    *q++ = '*';
		    break;

		  /* Insert wildcard before %s */
		  case LDAP_SRCH_ENDS:
		    *q++ = '*';
		    *q++ = *p++;
		    *q++ = *p++;
		    break;

		  /* Put wildcard before and after %s */
		  default:
		  case LDAP_SRCH_CONTAINS:
		    *q++ = '*';
		    *q++ = *p++;
		    *q++ = *p++;
		    *q++ = '*';
		    break;
		}
	    }
	    else
	      *q++ = *p++;
	}

	if(q - filt_format < sizeof(filt_format))
	  *q = '\0';

	filt_format[sizeof(filt_format)-1] = '\0';

	/*
	 * If combine is lit we put the custom filter and the filt_format
	 * filter and combine them with an &.
	 */
	if(cust && cust->filt && cust->combine){
	    char *combined;
	    size_t l;

	    l = strlen(filt_format) + strlen(cust->filt) + 3;
	    combined = (char *) fs_get((l+1) * sizeof(char));
	    snprintf(combined, l+1, "(&%s%s)", cust->filt, filt_format);
	    strncpy(filt_format, combined, sizeof(filt_format));
	    filt_format[sizeof(filt_format)-1] = '\0';
	    fs_give((void **) &combined);
	}

	/*
	 * Ad hoc attempt to make "Steve Hubert" match
	 * Steven Hubert but not Steven Shubert.
	 * We replace a <SPACE> with * <SPACE> (not * <SPACE> *).
	 */
	memset((void *)scp, 0, sizeof(scp));
	if(info->nosub)
	  strncpy(scp, string, sizeof(scp));
	else{
	    p = string;
	    q = scp;
	    while(*p && (q - scp) + 1 < sizeof(scp)){
		if(*p == SPACE && *(p+1) != SPACE){
		    *q++ = '*';
		    *q++ = *p++;
		}
		else
		  *q++ = *p++;
	    }
	}

	scp[sizeof(scp)-1] = '\0';

	slen = strlen(scp);
	flen = strlen(filt_format);
	/* truncate string if it will overflow filter */
	if(args*slen + flen - 2*args > sizeof(filter)-1)
	  scp[(sizeof(filter)-1 - flen)/args] = '\0';

	/*
	 * Replace %s's with scp.
	 */
	switch(args){
	  case 0:
	    snprintf(filter, sizeof(filter), filt_format);
	    break;
	  case 1:
	    snprintf(filter, sizeof(filter), filt_format, scp);
	    break;
	  case 2:
	    snprintf(filter, sizeof(filter), filt_format, scp, scp);
	    break;
	  case 3:
	    snprintf(filter, sizeof(filter), filt_format, scp, scp, scp);
	    break;
	  case 4:
	    snprintf(filter, sizeof(filter), filt_format, scp, scp, scp, scp);
	    break;
	  case 5:
	    snprintf(filter, sizeof(filter), filt_format, scp, scp, scp, scp, scp);
	    break;
	  case 6:
	    snprintf(filter, sizeof(filter), filt_format, scp, scp, scp, scp, scp, scp);
	    break;
	  case 7:
	    snprintf(filter, sizeof(filter), filt_format, scp, scp, scp, scp, scp, scp, scp);
	    break;
	  case 8:
	    snprintf(filter, sizeof(filter), filt_format, scp, scp, scp, scp, scp, scp, scp,
		    scp);
	    break;
	  case 9:
	    snprintf(filter, sizeof(filter), filt_format, scp, scp, scp, scp, scp, scp, scp,
		    scp, scp);
	    break;
	  case 10:
	  default:
	    snprintf(filter, sizeof(filter), filt_format, scp, scp, scp, scp, scp, scp, scp,
		    scp, scp, scp);
	    break;
	}

	/* replace double *'s with single *'s in filter */
	for(p = q = filter; *p; p++)
	  if(*p != '*' || p == filter || *(p-1) != '*')
	    *q++ = *p;

	*q = '\0';

	(void) removing_double_quotes(base);
	dprint((5, "about to ldap_search(\"%s\", %s)\n",
	       base ? base : "?", filter ? filter : "?"));
        if(ps_global->intr_pending)
	  srch_res = LDAP_PROTOCOL_ERROR;
	else{
	  int msgid;
	  time_t start_time;

	  start_time = time((time_t *)0);

	  dprint((6, "ldap_lookup: calling ldap_search\n"));
	  msgid = ldap_search(ld, base, info->scope, filter, NULL, 0);

	  if(msgid == -1)
	    srch_res = our_ldap_get_lderrno(ld, NULL, NULL);
	  else{
	    int lres;
	    /*
	     * Warning: struct timeval is not portable. However, since it is
	     * part of LDAP api it must be portable to all platforms LDAP
	     * has been ported to.
	     */
	    struct timeval t;

	    t.tv_sec  = 1; t.tv_usec = 0;
	      
	    do {
	      if(ps_global->intr_pending)
		intr_happened = 1;

	      dprint((6, "ldap_result(id=%d): ", msgid));
	      if((lres=ldap_result(ld, msgid, LDAP_MSG_ALL, &t, &res)) == -1){
	        /* error */
		srch_res = our_ldap_get_lderrno(ld, NULL, NULL);
	        dprint((6, "error (-1 returned): ld_errno=%d\n",
			   srch_res));
	      }
	      else if(lres == 0){  /* timeout, no results available */
		if(intr_happened){
		  ldap_abandon(ld, msgid);
		  srch_res = LDAP_PROTOCOL_ERROR;
		  if(our_ldap_get_lderrno(ld, NULL, NULL) == LDAP_SUCCESS)
		    our_ldap_set_lderrno(ld, LDAP_PROTOCOL_ERROR, NULL, NULL);

	          dprint((6, "timeout, intr: srch_res=%d\n",
			     srch_res));
		}
		else if(info->time > 0 &&
			((long)time((time_t *)0) - start_time) > info->time){
		  /* try for partial results */
		  t.tv_sec  = 0; t.tv_usec = 0;
		  lres = ldap_result(ld, msgid, LDAP_MSG_RECEIVED, &t, &res);
		  if(lres > 0 && lres != LDAP_RES_SEARCH_RESULT){
		    srch_res = LDAP_SUCCESS;
		    dprint((6, "partial result: lres=0x%x\n", lres));
		  }
		  else{
		    if(lres == 0)
		      ldap_abandon(ld, msgid);

		    srch_res = LDAP_TIMEOUT;
		    if(our_ldap_get_lderrno(ld, NULL, NULL) == LDAP_SUCCESS)
		      our_ldap_set_lderrno(ld, LDAP_TIMEOUT, NULL, NULL);

	            dprint((6,
			       "timeout, total_time (%d), srch_res=%d\n",
			       info->time, srch_res));
		  }
		}
		else{
	          dprint((6, "timeout\n"));
		}
	      }
	      else{
		srch_res = ldap_result2error(ld, res, 0);
	        dprint((6, "lres=0x%x, srch_res=%d\n", lres,
			   srch_res));
	      }
	    }while(lres == 0 &&
		    !(intr_happened ||
		      (info->time > 0 &&
		       ((long)time((time_t *)0) - start_time) > info->time)));
	  }
	}

	if(intr_happened){
	  wp_exit = 1;
          if(we_cancel)
            cancel_busy_cue(-1);

	  if(wp_err->error)
	    fs_give((void **)&wp_err->error);
	  else{
	    q_status_message(SM_ORDER, 0, 1, "Interrupt");
	    display_message('x');
	    fflush(stdout);
	  }

	  if(res)
	    ldap_msgfree(res);
	  if(ld)
	    ldap_unbind(ld);
	  
	  res = NULL; ld  = NULL;
	}
	else if(srch_res != LDAP_SUCCESS &&
	   srch_res != LDAP_TIMELIMIT_EXCEEDED &&
	   srch_res != LDAP_RESULTS_TOO_LARGE &&
	   srch_res != LDAP_TIMEOUT &&
	   srch_res != LDAP_SIZELIMIT_EXCEEDED){
	  wp_err->wp_err_occurred = 1;

	  ld_errnum = our_ldap_get_lderrno(ld, NULL, &ld_errstr);

	  snprintf(ebuf, sizeof(ebuf), _("LDAP search failed: %s%s%s%s"),
		  ldap_err2string(ld_errnum),
		  serv_errstr,
		  (ld_errstr && *ld_errstr) ? ": " : "",
		  (ld_errstr && *ld_errstr) ? ld_errstr : "");

          if(wp_err->error)
	    fs_give((void **)&wp_err->error);

          wp_err->error = cpystr(ebuf);
          if(we_cancel)
            cancel_busy_cue(-1);

          q_status_message(SM_ORDER, 3, 5, wp_err->error);
          display_message('x');
	  dprint((2, "%s\n", ebuf));
	  if(res)
	    ldap_msgfree(res);
	  if(ld)
	    ldap_unbind(ld);
	  
	  res = NULL; ld  = NULL;
	}
	else{
	  int cnt;

	  cnt = ldap_count_entries(ld, res);

	  if(cnt > 0){

	    if(srch_res == LDAP_TIMELIMIT_EXCEEDED ||
	       srch_res == LDAP_RESULTS_TOO_LARGE ||
	       srch_res == LDAP_TIMEOUT ||
	       srch_res == LDAP_SIZELIMIT_EXCEEDED){
	      wp_err->wp_err_occurred = 1;
	      ld_errnum = our_ldap_get_lderrno(ld, NULL, &ld_errstr);

	      snprintf(ebuf, sizeof(ebuf), _("LDAP partial results: %s%s%s%s"),
		      ldap_err2string(ld_errnum),
		      serv_errstr,
		      (ld_errstr && *ld_errstr) ? ": " : "",
		      (ld_errstr && *ld_errstr) ? ld_errstr : "");
	      dprint((2, "%s\n", ebuf));
	      if(wp_err->error)
		fs_give((void **)&wp_err->error);

	      wp_err->error = cpystr(ebuf);
	      if(we_cancel)
		cancel_busy_cue(-1);

	      q_status_message(SM_ORDER, 3, 5, wp_err->error);
	      display_message('x');
	    }

	    dprint((5, "Matched %d entries on %s\n",
	           cnt, serv ? serv : "?"));

	    serv_res = (LDAP_SERV_RES_S *)fs_get(sizeof(LDAP_SERV_RES_S));
	    memset((void *)serv_res, 0, sizeof(*serv_res));
	    serv_res->ld   = ld;
	    serv_res->res  = res;
	    serv_res->info_used = copy_ldap_serv_info(info);
	    /* Save by reference? */
	    if(info->ref){
		snprintf(buf, sizeof(buf), "%s:%s", serv, comatose(info->port));
		serv_res->serv = cpystr(buf);
	    }
	    else
	      serv_res->serv = NULL;

	    serv_res->next = NULL;
	  }
	  else{
	    if(srch_res == LDAP_TIMELIMIT_EXCEEDED ||
	       srch_res == LDAP_RESULTS_TOO_LARGE ||
	       srch_res == LDAP_TIMEOUT ||
	       srch_res == LDAP_SIZELIMIT_EXCEEDED){
	      wp_err->wp_err_occurred = 1;
	      wp_err->ldap_errno      = srch_res;

	      ld_errnum = our_ldap_get_lderrno(ld, NULL, &ld_errstr);

	      snprintf(ebuf, sizeof(ebuf), _("LDAP search failed: %s%s%s%s"),
		      ldap_err2string(ld_errnum),
		      serv_errstr,
		      (ld_errstr && *ld_errstr) ? ": " : "",
		      (ld_errstr && *ld_errstr) ? ld_errstr : "");

	      if(wp_err->error)
		fs_give((void **)&wp_err->error);

	      wp_err->error = cpystr(ebuf);
	      if(we_cancel)
		cancel_busy_cue(-1);

	      q_status_message(SM_ORDER, 3, 5, wp_err->error);
	      display_message('x');
	      dprint((2, "%s\n", ebuf));
	    }

	    dprint((5, "Matched 0 entries on %s\n",
		   serv ? serv : "?"));
	    if(res)
	      ldap_msgfree(res);
	    if(ld)
	      ldap_unbind(ld);

	    res = NULL; ld  = NULL;
	  }
	}
      }
    }

    if(we_cancel)
      cancel_busy_cue(-1);

    if(we_turned_on)
      intr_handling_off();

    if(serv)
      fs_give((void **)&serv);
    if(base)
      fs_give((void **)&base);
    if(serv_errstr)
      fs_give((void **)&serv_errstr);

    return(serv_res);
}


/*
 * Given a list of entries, present them to user so user may
 * select one.
 *
 * Args  head -- The head of the list of results
 *       orig -- The string the user was searching for
 *     result -- Returned pointer to chosen LDAP_SEARCH_WINNER or NULL
 *     wp_err -- Error handling
 *      style -- 
 *
 * Returns  0   ok
 *         -1   Exit chosen by user
 *         -2   None of matched entries had an email address
 *         -3   No matched entries
 *         -4   Goback to Abook List chosen by user
 *         -5   caller shouldn't free head
 */
int
ask_user_which_entry(LDAP_SERV_RES_S *head, char *orig, LDAP_CHOOSE_S **result,
		     WP_ERR_S *wp_err, LDAPLookupStyle style)
{
    ADDR_CHOOSE_S ac;
    char          t[200];
    int           retval;

    dprint((3, "ask_user_which(style=%s)\n",
	style == AlwaysDisplayAndMailRequired ? "AlwaysDisplayAndMailRequired" :
	  style == AlwaysDisplay ? "AlwaysDisplay" :
	    style == DisplayIfTwo ? "DisplayIfTwo" :
	      style == DisplayForURL ? "DisplayForURL" :
	        style == DisplayIfOne ? "DisplayIfOne" : "?"));

    /*
     * Set up a screen for user to choose one entry.
     */

    if(style == AlwaysDisplay || style == DisplayForURL)
      snprintf(t, sizeof(t), "SEARCH RESULTS INDEX");
    else{
	int len;

	len = strlen(orig);
	snprintf(t, sizeof(t), _("SELECT ONE ADDRESS%s%s%s"),
		(orig && *orig && len < 40) ? " FOR \"" : "",
		(orig && *orig && len < 40) ? orig : "",
		(orig && *orig && len < 40) ? "\"" : "");
    }

    memset(&ac, 0, sizeof(ADDR_CHOOSE_S));
    ac.title = cpystr(t);
    ac.res_head = head;

    retval = ldap_addr_select(ps_global, &ac, result, style, wp_err, orig);

    switch(retval){
      case 0:		/* Ok */
	break;

      case -1:		/* Exit chosen by user */
	wp_exit = 1;
	break;

      case -4:		/* GoBack to AbookList chosen by user */
	break;

      case -5:
	wp_nobail = 1;
	break;

      case -2:
	if(style != AlwaysDisplay){
	    if(wp_err->error)
	      fs_give((void **)&wp_err->error);

	    wp_err->error =
   cpystr(_("None of the names matched on directory server has an email address"));
	    q_status_message(SM_ORDER, 3, 5, wp_err->error);
	    display_message('x');
	}

	break;

      case -3:
	if(style == AlwaysDisplayAndMailRequired){
	    if(wp_err->error)
	      fs_give((void **)&wp_err->error);

	    wp_err->error = cpystr(_("No matches on directory server"));
	    q_status_message(SM_ORDER, 3, 5, wp_err->error);
	    display_message('x');
	}

	break;
    }

    fs_give((void **)&ac.title);

    return(retval);
}


ADDRESS *
address_from_ldap(LDAP_CHOOSE_S *winning_e)
{
    ADDRESS *ret_a = NULL;

    if(winning_e){
	char       *a;
	BerElement *ber;

	ret_a = mail_newaddr();
	for(a = ldap_first_attribute(winning_e->ld, winning_e->selected_entry, &ber);
	    a != NULL;
	    a = ldap_next_attribute(winning_e->ld, winning_e->selected_entry, ber)){
	    int i;
	    char  *p;
	    char **vals;

	    dprint((9, "attribute: %s\n", a ? a : "?"));
	    if(!ret_a->personal &&
	       strcmp(a, winning_e->info_used->cnattr) == 0){
		dprint((9, "Got cnattr:"));
		vals = ldap_get_values(winning_e->ld, winning_e->selected_entry, a);
		for(i = 0; vals[i] != NULL; i++)
		  dprint((9, "       %s\n",
		         vals[i] ? vals[i] : "?"));
	    
		if(vals && vals[0])
		  ret_a->personal = cpystr(vals[0]);

		ldap_value_free(vals);
	    }
	    else if(!ret_a->mailbox &&
		    strcmp(a, winning_e->info_used->mailattr) == 0){
		dprint((9, "Got mailattr:"));
		vals = ldap_get_values(winning_e->ld, winning_e->selected_entry, a);
		for(i = 0; vals[i] != NULL; i++)
		  dprint((9, "         %s\n",
		         vals[i] ? vals[i] : "?"));
		    
		/* use first one */
		if(vals && vals[0]){
		    if((p = strindex(vals[0], '@')) != NULL){
			ret_a->host = cpystr(p+1);
			*p = '\0';
		    }

		    ret_a->mailbox = cpystr(vals[0]);
		}

		ldap_value_free(vals);
	    }

	    our_ldap_memfree(a);
	}
    }

    return(ret_a);
}


/*
 * Break up the ldap-server string stored in the pinerc into its
 * parts. The structure is allocated here and should be freed by the caller.
 *
 * The original string looks like
 *     <servername>[:port] <SPACE> "/base=<base>/impl=1/..."
 *
 * Args  serv_str -- The original string from the pinerc to parse.
 *
 * Returns A pointer to a structure with filled in answers.
 *
 *  Some of the members have defaults. If port is -1, that means to use
 *  the default LDAP_PORT. If base is NULL, use "". Type and srch have
 *  defaults defined in alpine.h. If cust is non-NULL, it overrides type and
 *  srch.
 */
LDAP_SERV_S *
break_up_ldap_server(char *serv_str)
{
    char *lserv;
    char *q, *p, *tail;
    int   i, only_one = 1;
    LDAP_SERV_S *info = NULL;

    if(!serv_str)
      return(info);

    info = (LDAP_SERV_S *)fs_get(sizeof(LDAP_SERV_S));

    /*
     * Initialize to defaults.
     */
    memset((void *)info, 0, sizeof(*info));
    info->port  = -1;
    info->srch  = -1;
    info->type  = -1;
    info->time  = -1;
    info->size  = -1;
    info->scope = -1;

    /* copy the whole string to work on */
    lserv = cpystr(serv_str);
    if(lserv)
      removing_trailing_white_space(lserv);

    if(!lserv || !*lserv || *lserv == '"'){
	if(lserv)
	  fs_give((void **)&lserv);

	if(info)
	  free_ldap_server_info(&info);

	return(NULL);
    }

    tail = lserv;
    while((tail = strindex(tail, SPACE)) != NULL){
	tail++;
	if(*tail == '"' || *tail == '/'){
	    *(tail-1) = '\0';
	    break;
	}
	else
	  only_one = 0;
    }

    /* tail is the part after server[:port] <SPACE> */
    if(tail && *tail){
	removing_leading_white_space(tail);
	(void)removing_double_quotes(tail);
    }

    /* get the optional port number */
    if(only_one && (q = strindex(lserv, ':')) != NULL){
	int ldapport = -1;

	*q = '\0';
	if((ldapport = atoi(q+1)) >= 0)
	  info->port = ldapport;
    }

    /* use lserv for serv even though it has a few extra bytes alloced */
    info->serv = lserv;
    
    if(tail && *tail){
	/* get the search base */
	if((q = srchstr(tail, "/base=")) != NULL)
	  info->base = remove_backslash_escapes(q+6);

	if((q = srchstr(tail, "/binddn=")) != NULL)
	  info->binddn = remove_backslash_escapes(q+8);

	/* get the implicit parameter */
	if((q = srchstr(tail, "/impl=1")) != NULL)
	  info->impl = 1;

	/* get the rhs parameter */
	if((q = srchstr(tail, "/rhs=1")) != NULL)
	  info->rhs = 1;

	/* get the ref parameter */
	if((q = srchstr(tail, "/ref=1")) != NULL)
	  info->ref = 1;

	/* get the nosub parameter */
	if((q = srchstr(tail, "/nosub=1")) != NULL)
	  info->nosub = 1;

	/* get the tls parameter */
	if((q = srchstr(tail, "/tls=1")) != NULL)
	  info->tls = 1;

	/* get the tlsmust parameter */
	if((q = srchstr(tail, "/tlsm=1")) != NULL)
	  info->tlsmust = 1;

	/* get the search type value */
	if((q = srchstr(tail, "/type=")) != NULL){
	    NAMEVAL_S *v;

	    q += 6;
	    if((p = strindex(q, '/')) != NULL)
	      *p = '\0';
	    
	    for(i = 0; (v = ldap_search_types(i)); i++)
	      if(!strucmp(q, v->name)){
		  info->type = v->value;
		  break;
	      }
	    
	    if(p)
	      *p = '/';
	}

	/* get the search rule value */
	if((q = srchstr(tail, "/srch=")) != NULL){
	    NAMEVAL_S *v;

	    q += 6;
	    if((p = strindex(q, '/')) != NULL)
	      *p = '\0';
	    
	    for(i = 0; (v = ldap_search_rules(i)); i++)
	      if(!strucmp(q, v->name)){
		  info->srch = v->value;
		  break;
	      }
	    
	    if(p)
	      *p = '/';
	}

	/* get the scope */
	if((q = srchstr(tail, "/scope=")) != NULL){
	    NAMEVAL_S *v;

	    q += 7;
	    if((p = strindex(q, '/')) != NULL)
	      *p = '\0';
	    
	    for(i = 0; (v = ldap_search_scope(i)); i++)
	      if(!strucmp(q, v->name)){
		  info->scope = v->value;
		  break;
	      }
	    
	    if(p)
	      *p = '/';
	}

	/* get the time limit */
	if((q = srchstr(tail, "/time=")) != NULL){
	    q += 6;
	    if((p = strindex(q, '/')) != NULL)
	      *p = '\0';
	    
	    /* This one's a number */
	    if(*q){
		char *err;

		err = strtoval(q, &i, 0, 500, 0, tmp_20k_buf, SIZEOF_20KBUF, "ldap timelimit");
		if(err){
		  dprint((1, "%s\n", err ? err : "?"));
		}
		else
		  info->time = i;
	    }

	    if(p)
	      *p = '/';
	}

	/* get the size limit */
	if((q = srchstr(tail, "/size=")) != NULL){
	    q += 6;
	    if((p = strindex(q, '/')) != NULL)
	      *p = '\0';
	    
	    /* This one's a number */
	    if(*q){
		char *err;

		err = strtoval(q, &i, 0, 500, 0, tmp_20k_buf, SIZEOF_20KBUF, "ldap sizelimit");
		if(err){
		  dprint((1, "%s\n", err ? err : "?"));
		}
		else
		  info->size = i;
	    }

	    if(p)
	      *p = '/';
	}

	/* get the custom search filter */
	if((q = srchstr(tail, "/cust=")) != NULL)
	  info->cust = remove_backslash_escapes(q+6);

	/* get the nickname */
	if((q = srchstr(tail, "/nick=")) != NULL)
	  info->nick = remove_backslash_escapes(q+6);

	/* get the mail attribute name */
	if((q = srchstr(tail, "/matr=")) != NULL)
	  info->mailattr = remove_backslash_escapes(q+6);

	/* get the sn attribute name */
	if((q = srchstr(tail, "/satr=")) != NULL)
	  info->snattr = remove_backslash_escapes(q+6);

	/* get the gn attribute name */
	if((q = srchstr(tail, "/gatr=")) != NULL)
	  info->gnattr = remove_backslash_escapes(q+6);

	/* get the cn attribute name */
	if((q = srchstr(tail, "/catr=")) != NULL)
	  info->cnattr = remove_backslash_escapes(q+6);

	/* get the backup mail address */
	if((q = srchstr(tail, "/mail=")) != NULL)
	  info->mail = remove_backslash_escapes(q+6);
    }

    return(info);
}


void
free_ldap_server_info(LDAP_SERV_S **info)
{
    if(info && *info){
	if((*info)->serv)
	  fs_give((void **)&(*info)->serv);

	if((*info)->base)
	  fs_give((void **)&(*info)->base);

	if((*info)->cust)
	  fs_give((void **)&(*info)->cust);

	if((*info)->binddn)
	  fs_give((void **)&(*info)->binddn);

	if((*info)->nick)
	  fs_give((void **)&(*info)->nick);

	if((*info)->mail)
	  fs_give((void **)&(*info)->mail);

	if((*info)->mailattr)
	  fs_give((void **)&(*info)->mailattr);

	if((*info)->snattr)
	  fs_give((void **)&(*info)->snattr);

	if((*info)->gnattr)
	  fs_give((void **)&(*info)->gnattr);

	if((*info)->cnattr)
	  fs_give((void **)&(*info)->cnattr);

	fs_give((void **)info);
	*info = NULL;
    }
}


LDAP_SERV_S *
copy_ldap_serv_info(LDAP_SERV_S *src)
{
    LDAP_SERV_S *info = NULL;

    if(src){
	info = (LDAP_SERV_S *) fs_get(sizeof(*info));

	/*
	 * Initialize to defaults.
	 */
	memset((void *)info, 0, sizeof(*info));

	info->serv     = src->serv ? cpystr(src->serv) : NULL;
	info->base     = src->base ? cpystr(src->base) : NULL;
	info->cust     = src->cust ? cpystr(src->cust) : NULL;
	info->binddn   = src->binddn ? cpystr(src->binddn) : NULL;
	info->nick     = src->nick ? cpystr(src->nick) : NULL;
	info->mail     = src->mail ? cpystr(src->mail) : NULL;
	info->mailattr = cpystr((src->mailattr && src->mailattr[0])
			    ? src->mailattr : DEF_LDAP_MAILATTR);
	info->snattr = cpystr((src->snattr && src->snattr[0])
			    ? src->snattr : DEF_LDAP_SNATTR);
	info->gnattr = cpystr((src->gnattr && src->gnattr[0])
			    ? src->gnattr : DEF_LDAP_GNATTR);
	info->cnattr = cpystr((src->cnattr && src->cnattr[0])
			    ? src->cnattr : DEF_LDAP_CNATTR);

	info->port  = (src->port < 0)  ? LDAP_PORT      : src->port;
	info->time  = (src->time < 0)  ? DEF_LDAP_TIME  : src->time;
	info->size  = (src->size < 0)  ? DEF_LDAP_SIZE  : src->size;
	info->type  = (src->type < 0)  ? DEF_LDAP_TYPE  : src->type;
	info->srch  = (src->srch < 0)  ? DEF_LDAP_SRCH  : src->srch;
	info->scope = (src->scope < 0) ? DEF_LDAP_SCOPE : src->scope;
	info->impl  = src->impl;
	info->rhs   = src->rhs;
	info->ref   = src->ref;
	info->nosub = src->nosub;
	info->tls   = src->tls;
    }

    return(info);
}


void
free_ldap_result_list(LDAP_SERV_RES_S **r)
{
    if(r && *r){
	free_ldap_result_list(&(*r)->next);
	if((*r)->res)
	  ldap_msgfree((*r)->res);
	if((*r)->ld)
	  ldap_unbind((*r)->ld);
	if((*r)->info_used)
	  free_ldap_server_info(&(*r)->info_used);
	if((*r)->serv)
	  fs_give((void **) &(*r)->serv);

	fs_give((void **) r);
    }
}


/*
 * Mask API differences. 
 */
void
our_ldap_memfree(void *a)
{
#if (LDAPAPI >= 15)
    if(a)
      ldap_memfree(a);
#endif
}


/*
 * Mask API differences.
 */
void
our_ldap_dn_memfree(void *a)
{
#if defined(_WINDOWS)
    if(a)
      ldap_memfree(a);
#else
#if (LDAPAPI >= 15)
    if(a)
      ldap_memfree(a);
#else
    if(a)
      free(a);
#endif
#endif
}


/*
 * More API masking.
 */
int
our_ldap_get_lderrno(LDAP *ld, char **m, char **s)
{
    int ret = 0;

#if (LDAPAPI >= 2000)
    if(ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, (void *)&ret) == 0){
	if(s)
	  ldap_get_option(ld, LDAP_OPT_ERROR_STRING, (void *)s);
    }
#elif (LDAPAPI >= 15)
    ret = ldap_get_lderrno(ld, m, s);
#else
    ret = ld->ld_errno;
    if(s)
      *s = ld->ld_error;
#endif

    return(ret);
}


/*
 * More API masking.
 */
int
our_ldap_set_lderrno(LDAP *ld, int e, char *m, char *s)
{
    int ret;

#if (LDAPAPI >= 2000)
    if(ldap_set_option(ld, LDAP_OPT_ERROR_NUMBER, (void *)&e) == 0)
      ret = LDAP_SUCCESS;
    else
      (void)ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, (void *)&ret);
#elif (LDAPAPI >= 15)
    ret = ldap_set_lderrno(ld, e, m, s);
#else
    /* this is all we care about */
    ld->ld_errno = e;
    ret = LDAP_SUCCESS;
#endif

    return(ret);
}


/*
 * More API masking.
 */
int
our_ldap_set_option(LDAP *ld, int option, void *optdata)
{
    int ret;

#if (LDAPAPI >= 15)
    ret = ldap_set_option(ld, option, optdata);
#else
    switch(option){
      case LDAP_OPT_TIMELIMIT:
	ld->ld_timelimit = *(int *)optdata;
	break;
	
      case LDAP_OPT_SIZELIMIT:
	ld->ld_sizelimit = *(int *)optdata;
	break;
    
      case LDAP_OPT_RESTART:
	if((int)optdata)
	  ld->ld_options |= LDAP_OPT_RESTART;
	else
	  ld->ld_options &= ~LDAP_OPT_RESTART;

	break;
    
      /*
       * Does nothing here. There is only one protocol version supported.
       */
      case LDAP_OPT_PROTOCOL_VERSION:
	ret = -1;
	break;
    
      default:
	panic("LDAP function not implemented");
    }
#endif

    return(ret);
}


/*
 * Returns 1 if we can use LDAP version 3 protocol.
 */
int
ldap_v3_is_supported(LDAP *ld)
{
    return(1);
}

struct tl_table {
    char *ldap_ese;
    char *translated;
};

static struct tl_table ldap_trans_table[]={
    /*
     * TRANSLATORS: This is a list of LDAP attributes with translations to present
     * to the user. For example the attribute mail is Email Address and the attribute
     * cn is Name.
     */
    {"mail",				N_("Email Address")},
#define LDAP_MAIL_ATTR 0
    {"sn",				N_("Surname")},
#define LDAP_SN_ATTR 1
    {"givenName",			N_("Given Name")},
#define LDAP_GN_ATTR 2
    {"cn",				N_("Name")},
#define LDAP_CN_ATTR 3
    {"electronicmail",			N_("Email Address")},
#define LDAP_EMAIL_ATTR 4
    {"o",				N_("Organization")},
    {"ou",				N_("Unit")},
    {"c",				N_("Country")},
    {"st",				N_("State or Province")},
    {"l",				N_("Locality")},
    {"objectClass",			N_("Object Class")},
    {"title",				N_("Title")},
    {"departmentNumber",		N_("Department")},
    {"postalAddress",			N_("Postal Address")},
    {"homePostalAddress",		N_("Home Address")},
    {"mailStop",			N_("Mail Stop")},
    {"telephoneNumber",			N_("Voice Telephone")},
    {"homePhone",			N_("Home Telephone")},
    {"officePhone",			N_("Office Telephone")},
    {"facsimileTelephoneNumber",	N_("FAX Telephone")},
    {"mobile",				N_("Mobile Telephone")},
    {"pager",				N_("Pager")},
    {"roomNumber",			N_("Room Number")},
    {"uid",				N_("User ID")},
    {NULL,				NULL}
};

char *
ldap_translate(char *a, LDAP_SERV_S *info_used)
{
    int i;

    if(info_used){
	if(info_used->mailattr && strucmp(info_used->mailattr, a) == 0)
	  return(_(ldap_trans_table[LDAP_MAIL_ATTR].translated));
	else if(info_used->snattr && strucmp(info_used->snattr, a) == 0)
	  return(_(ldap_trans_table[LDAP_SN_ATTR].translated));
	else if(info_used->gnattr && strucmp(info_used->gnattr, a) == 0)
	  return(_(ldap_trans_table[LDAP_GN_ATTR].translated));
	else if(info_used->cnattr && strucmp(info_used->cnattr, a) == 0)
	  return(_(ldap_trans_table[LDAP_CN_ATTR].translated));
    }

    for(i = 0; ldap_trans_table[i].ldap_ese; i++){
	if(info_used)
	  switch(i){
	    case LDAP_MAIL_ATTR:
	    case LDAP_SN_ATTR:
	    case LDAP_GN_ATTR:
	    case LDAP_CN_ATTR:
	    case LDAP_EMAIL_ATTR:
	      continue;
	  }

	if(strucmp(ldap_trans_table[i].ldap_ese, a) == 0)
	  return(_(ldap_trans_table[i].translated));
    }
    
    return(a);
}


#endif	/* ENABLE_LDAP */
