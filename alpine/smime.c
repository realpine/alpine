#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: smime.c 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $";
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


#include "headers.h"

#ifdef SMIME

#include "../pith/charconv/utf8.h"
#include "../pith/status.h"
#include "../pith/store.h"
#include "../pith/conf.h"
#include "../pith/list.h"
#include "radio.h"
#include "keymenu.h"
#include "mailview.h"
#include "conftype.h"
#include "confscroll.h"
#include "setup.h"
#include "smime.h"


/* internal prototypes */
void     format_smime_info(int pass, BODY *body, long msgno, gf_io_t pc);
void     print_separator_line(int percent, int ch, gf_io_t pc);
void     output_cert_info(X509 *cert, gf_io_t pc);
void     output_X509_NAME(X509_NAME *name, gf_io_t pc);
void     side_by_side(STORE_S *left, STORE_S *right, gf_io_t pc);
void     get_fingerprint(X509 *cert, const EVP_MD *type, char *buf, size_t maxLen);
STORE_S *wrap_store(STORE_S *in, int width);
void     smime_config_init_display(struct pine *, CONF_S **, CONF_S **);
void     revert_to_saved_smime_config(struct pine *ps, SAVED_CONFIG_S *vsave);
SAVED_CONFIG_S *save_smime_config_vars(struct pine *ps);
void     free_saved_smime_config(struct pine *ps, SAVED_CONFIG_S **vsavep);
int      smime_helper_tool(struct pine *, int, CONF_S **, unsigned);


/*
 * prompt the user for their passphrase 
 *  (possibly prompting with the email address in s_passphrase_emailaddr)
 */
int
smime_get_passphrase(void)
{
    int rc;
    int	flags;
    char prompt[500];
    HelpType help = NO_HELP;

    assert(ps_global->smime != NULL);
    snprintf(prompt, sizeof(prompt),
            _("Enter passphrase for <%s>: "), (ps_global->smime && ps_global->smime->passphrase_emailaddr) ? ps_global->smime->passphrase_emailaddr : "unknown");

    do {
        flags = OE_PASSWD | OE_DISALLOW_HELP;
        rc =  optionally_enter((char *) ps_global->smime->passphrase,
			       -FOOTER_ROWS(ps_global), 0,
			       sizeof(ps_global->smime->passphrase),
                               prompt, NULL, help, &flags);
    } while (rc!=0 && rc!=1 && rc>0);

    if(rc==0){
	if(ps_global->smime)
	  ps_global->smime->entered_passphrase = 1;
    }

    return rc==0;
}


void
smime_info_screen(struct pine *ps)
{
    long      msgno;
    OtherMenu what;
    int       offset = 0;
    BODY     *body;
    ENVELOPE *env;
    HANDLE_S *handles = NULL;
    SCROLL_S  scrollargs;
    STORE_S  *store = NULL;
    
    ps->prev_screen = smime_info_screen;
    ps->next_screen = SCREEN_FUN_NULL;

    if(mn_total_cur(ps->msgmap) > 1L){
	q_status_message(SM_ORDER | SM_DING, 0, 3,
			 _("Can only view one message's information at a time."));
	return;
    }
    /* else check for existence of smime bits */

    msgno = mn_m2raw(ps->msgmap, mn_get_cur(ps->msgmap));
    
    env = mail_fetch_structure(ps->mail_stream, msgno, &body, 0);
    if(!env || !body){
	q_status_message(SM_ORDER, 0, 3,
			 _("Can't fetch body of message."));
	return;
    }
    
    what = FirstMenu;

    store = so_get(CharStar, NULL, EDIT_ACCESS);

    while(ps->next_screen == SCREEN_FUN_NULL){

    	ClearLine(1);

	so_truncate(store, 0);
	
	view_writec_init(store, &handles, HEADER_ROWS(ps),
			 HEADER_ROWS(ps) + 
			 ps->ttyo->screen_rows - (HEADER_ROWS(ps)
						  + HEADER_ROWS(ps)));

    	gf_puts_uline("Overview", view_writec);
    	gf_puts(NEWLINE, view_writec);

	format_smime_info(1, body, msgno, view_writec);
	gf_puts(NEWLINE, view_writec);
	format_smime_info(2, body, msgno, view_writec);

	view_writec_destroy();

	ps->next_screen = SCREEN_FUN_NULL;

	memset(&scrollargs, 0, sizeof(SCROLL_S));
	scrollargs.text.text	= so_text(store);
	scrollargs.text.src	= CharStar;
	scrollargs.text.desc	= "S/MIME information";
	scrollargs.body_valid = 1;

	if(offset){		/* resize?  preserve paging! */
	    scrollargs.start.on		= Offset;
	    scrollargs.start.loc.offset = offset;
	    offset = 0L;
	}

	scrollargs.bar.title	= "S/MIME INFORMATION";
/*	scrollargs.end_scroll	= view_end_scroll; */
	scrollargs.resize_exit	= 1;
	scrollargs.help.text	= NULL;
	scrollargs.help.title	= "HELP FOR S/MIME INFORMATION VIEW";
	scrollargs.keys.menu	= &smime_info_keymenu;
	scrollargs.keys.what    = what;
	setbitmap(scrollargs.keys.bitmap);

	if(scrolltool(&scrollargs) == MC_RESIZE)
	  offset = scrollargs.start.loc.offset;
    }

    so_give(&store);
}


void
format_smime_info(int pass, BODY *body, long msgno, gf_io_t pc)
{
    PKCS7 *p7;
    int    i;
    
    if(body->type == TYPEMULTIPART){
    	PART *p;    

        for(p=body->nested.part; p; p=p->next)
          format_smime_info(pass, &p->body, msgno, pc);
    }
    
    p7 = body->sparep;
    if(p7){

    	if(PKCS7_type_is_signed(p7)){
            STACK_OF(X509) *signers;

    	    switch(pass){
	      case 1:
		gf_puts(_("This message was cryptographically signed."), pc);
		gf_puts(NEWLINE, pc);
		break;

	      case 2:
		signers = PKCS7_get0_signers(p7, NULL, 0);

		if(signers){

		    snprintf(tmp_20k_buf, SIZEOF_20KBUF, _("Certificate%s used for signing"),
			     plural(sk_X509_num(signers)));
		    gf_puts_uline(tmp_20k_buf, pc);
		    gf_puts(NEWLINE, pc);
		    print_separator_line(100, '-', pc);

		    for(i=0; i<sk_X509_num(signers); i++){
			X509 *x = sk_X509_value(signers, i);

			if(x){
			    output_cert_info(x, pc);
			    gf_puts(NEWLINE, pc);
			}
		    }
		}

		sk_X509_free(signers);
		break;
	    }
    	
	}
	else if(PKCS7_type_is_enveloped(p7)){
	
    	    switch(pass){
	      case 1:
		gf_puts(_("This message was encrypted."), pc);
		gf_puts(NEWLINE, pc);
		break;

	      case 2:
		if(p7->d.enveloped && p7->d.enveloped->enc_data){
		    X509_ALGOR *alg = p7->d.enveloped->enc_data->algorithm;
		    STACK_OF(PKCS7_RECIP_INFO) *ris = p7->d.enveloped->recipientinfo;
		    int found = 0;

		    gf_puts(_("The algorithm used to encrypt was "), pc);

		    if(alg){
			char *n = (char *) OBJ_nid2sn( OBJ_obj2nid(alg->algorithm));

			gf_puts(n ? n : "<unknown>", pc);

		    }
		    else
		      gf_puts("<unknown>", pc);

		    gf_puts("." NEWLINE NEWLINE, pc);

		    snprintf(tmp_20k_buf, SIZEOF_20KBUF, _("Certificate%s for decrypting"),
			     plural(sk_PKCS7_RECIP_INFO_num(ris)));
		    gf_puts_uline(tmp_20k_buf, pc);
		    gf_puts(NEWLINE, pc);
		    print_separator_line(100, '-', pc);

		    for(i=0; i<sk_PKCS7_RECIP_INFO_num(ris); i++){
			PKCS7_RECIP_INFO *ri;
			PERSONAL_CERT *pcert;

			ri = sk_PKCS7_RECIP_INFO_value(ris, i);
			if(!ri)
			  continue;

			pcert = find_certificate_matching_recip_info(ri);

			if(pcert){
			    if(found){
				print_separator_line(25, '*', pc);
				gf_puts(NEWLINE, pc);
			    }

			    found = 1;

			    output_cert_info(pcert->cert, pc);
			    gf_puts(NEWLINE, pc);

			}
		    }

		    if(!found){
			gf_puts(_("No certificate capable of decrypting could be found."), pc);
			gf_puts(NEWLINE, pc);
			gf_puts(NEWLINE, pc);
		    }
		}

		break;
	    }
	}
    }
}


void
print_separator_line(int percent, int ch, gf_io_t pc)
{
    int i, start, len;
    
    len = ps_global->ttyo->screen_cols * percent / 100;
    start = (ps_global->ttyo->screen_cols - len)/2;
    
    for(i=0; i<start; i++)
      pc(' ');

    for(i=start; i<start+len; i++)
      pc(ch);

    gf_puts(NEWLINE, pc);
}


void
output_cert_info(X509 *cert, gf_io_t pc)
{
    char    buf[256];
    STORE_S *left,*right;
    gf_io_t spc;
    int len;
        
    left = so_get(CharStar, NULL, EDIT_ACCESS);
    right = so_get(CharStar, NULL, EDIT_ACCESS);
    if(!(left && right))
      return;

    gf_set_so_writec(&spc, left);

    if(!cert->cert_info){
    	gf_puts("Couldn't find certificate info.", spc);
	gf_puts(NEWLINE, spc);
    }
    else{
	gf_puts_uline("Subject (whose certificate it is)", spc);
	gf_puts(NEWLINE, spc);

	output_X509_NAME(cert->cert_info->subject, spc);
	gf_puts(NEWLINE, spc);

	gf_puts_uline("Serial Number", spc);
	gf_puts(NEWLINE, spc);

	snprintf(buf, sizeof(buf), "%ld", ASN1_INTEGER_get(cert->cert_info->serialNumber));
	gf_puts(buf, spc);
	gf_puts(NEWLINE, spc);
	gf_puts(NEWLINE, spc);

	gf_puts_uline("Validity", spc);
	gf_puts(NEWLINE, spc);
    	{
    	    BIO *mb = BIO_new(BIO_s_mem());
	    char iobuf[4096];
	    
	    gf_puts("Not Before: ", spc);

	    (void) BIO_reset(mb);
	    ASN1_UTCTIME_print(mb, cert->cert_info->validity->notBefore);
	    (void) BIO_flush(mb);
	    while((len = BIO_read(mb, iobuf, sizeof(iobuf))) > 0)
	      gf_nputs(iobuf, len, spc);

	    gf_puts(NEWLINE, spc);

	    gf_puts("Not After:  ", spc);

	    (void) BIO_reset(mb);
	    ASN1_UTCTIME_print(mb, cert->cert_info->validity->notAfter);
	    (void) BIO_flush(mb);
	    while((len = BIO_read(mb, iobuf, sizeof(iobuf))) > 0)
	      gf_nputs(iobuf, len, spc);
    	    
	    gf_puts(NEWLINE, spc);
	    gf_puts(NEWLINE, spc);
	    	    
	    BIO_free(mb);
	}
    }

    gf_clear_so_writec(left);

    gf_set_so_writec(&spc, right);

    if(!cert->cert_info){
    	gf_puts(_("Couldn't find certificate info."), spc);
	gf_puts(NEWLINE, spc);
    }
    else{
	gf_puts_uline("Issuer", spc);
	gf_puts(NEWLINE, spc);

	output_X509_NAME(cert->cert_info->issuer, spc);
	gf_puts(NEWLINE, spc);
    }
    
    gf_clear_so_writec(right);
    
    side_by_side(left, right, pc);

    gf_puts_uline("SHA1 Fingerprint", pc);
    gf_puts(NEWLINE, pc);
    get_fingerprint(cert, EVP_sha1(), buf, sizeof(buf));
    gf_puts(buf, pc);
    gf_puts(NEWLINE, pc);

    gf_puts_uline("MD5 Fingerprint", pc);
    gf_puts(NEWLINE, pc);
    get_fingerprint(cert, EVP_md5(), buf, sizeof(buf));
    gf_puts(buf, pc);
    gf_puts(NEWLINE, pc);
    
    so_give(&left);
    so_give(&right);
}


void
output_X509_NAME(X509_NAME *name, gf_io_t pc)
{
    int i, c;
    char buf[256];
    
    c = X509_NAME_entry_count(name);
    
    for(i=c-1; i>=0; i--){
    	X509_NAME_ENTRY *e;
	
    	e = X509_NAME_get_entry(name,i);
	if(!e)
	  continue;
	
    	X509_NAME_get_text_by_OBJ(name, e->object, buf, sizeof(buf));
	
    	gf_puts(buf, pc);
	gf_puts(NEWLINE, pc);    
    }
}


/*
 * Output the contents of the given stores (left and right)
 * to the given gf_io_t.
 * The width of the terminal is inspected and two columns
 * are created to fit the stores into. They are then wrapped
 * and merged.
 */
void
side_by_side(STORE_S *left, STORE_S *right, gf_io_t pc)
{
    STORE_S *left_wrapped;
    STORE_S *right_wrapped;
    char    buf_l[256];
    char    buf_r[256];
    char    *b;
    int i;
    int w = ps_global->ttyo->screen_cols/2 - 1;
    
    so_seek(left, 0, 0);
    so_seek(right, 0, 0);
    
    left_wrapped = wrap_store(left, w);
    right_wrapped = wrap_store(right, w);
    
    so_seek(left_wrapped, 0, 0);
    so_seek(right_wrapped, 0, 0);

    for(;;){
    
    	if(!so_fgets(left_wrapped, buf_l, sizeof(buf_l)))
	  break;

    	if(!so_fgets(right_wrapped, buf_r, sizeof(buf_r)))
	  break;
    
	for(i=0, b=buf_l; i<w && *b && *b!='\r' && *b!='\n'; i++,b++){
	    pc(*b);
	    /* reduce accumulated width if an embed tag is discovered */
	    if(*b==TAG_EMBED)
	      i-=2;
	}

	if(buf_r[0]){
    	    while(i<w){
	    	pc(' ');
		i++;
	    }

	    for(i=0, b=buf_r; i<w && *b && *b!='\r' && *b!='\n'; i++,b++)
	      pc(*b);
	}

	gf_puts(NEWLINE, pc);
    }
    
    so_give(&left_wrapped);
    so_give(&right_wrapped);
}


void
get_fingerprint(X509 *cert, const EVP_MD *type, char *buf, size_t maxLen)
{
    unsigned char md[128];
    char    *b;
    unsigned int len, i;

    len = sizeof(md);

    X509_digest(cert, type, md, &len);

    b = buf;
    *b = 0;
    for(i=0; i<len; i++){
    	if(b-buf+3>=maxLen)
	  break;
	
	if(i != 0)
	  *b++ = ':';

	snprintf(b, maxLen - (b-buf), "%02x", md[i]);
    	b+=2;
    }
}


/*
 * Wrap the text in the given store to the given width.
 * A new store is created for the result.
 */
STORE_S *
wrap_store(STORE_S *in, int width)
{
    STORE_S *result;
    void  *ws;
    gf_io_t ipc,opc;
    
    if(width<10)
      width = 10;
    
    result = so_get(CharStar, NULL, EDIT_ACCESS);
    ws = gf_wrap_filter_opt(width, width, NULL, 0, 0);

    gf_filter_init();
    gf_link_filter(gf_wrap, ws);

    gf_set_so_writec(&opc, result);
    gf_set_so_readc(&ipc, in);

    gf_pipe(ipc, opc);
    
    gf_clear_so_readc(in);
    gf_clear_so_writec(result);
    
    return result;
}


void
smime_config_screen(struct pine *ps, int edit_exceptions)
{
    CONF_S	   *ctmp = NULL, *first_line = NULL;
    SAVED_CONFIG_S *vsave;
    OPT_SCREEN_S    screen;
    int             ew, readonly_warning = 0;

    dprint((9, "smime_config_screen()"));
    ps->next_screen = SCREEN_FUN_NULL;

    if(ps->fix_fixed_warning)
      offer_to_fix_pinerc(ps);

    ew = edit_exceptions ? ps_global->ew_for_except_vars : Main;

    if(ps->restricted)
      readonly_warning = 1;
    else{
	PINERC_S *prc = NULL;

	switch(ew){
	  case Main:
	    prc = ps->prc;
	    break;
	  case Post:
	    prc = ps->post_prc;
	    break;
	  default:
	    break;
	}

	readonly_warning = prc ? prc->readonly : 1;
	if(prc && prc->quit_to_edit){
	    quit_to_edit_msg(prc);
	    return;
	}
    }

    smime_config_init_display(ps, &ctmp, &first_line);

    vsave = save_smime_config_vars(ps);

    memset(&screen, 0, sizeof(screen));
    screen.deferred_ro_warning = readonly_warning;
    switch(conf_scroll_screen(ps, &screen, first_line,
			      edit_exceptions ? _("SETUP S/MIME EXCEPTIONS")
					      : _("SETUP S/MIME"),
			      /* TRANSLATORS: Print something1 using something2.
				 configuration is something1 */
			      _("configuration"), 0)){
      case 0:
	break;

      case 1:
	write_pinerc(ps, ew, WRP_NONE);
	break;
    
      case 10:
	revert_to_saved_smime_config(ps, vsave);
	break;
      
      default:
	q_status_message(SM_ORDER, 7, 10,
			 _("conf_scroll_screen bad ret in smime_config"));
	break;
    }

    free_saved_smime_config(ps, &vsave);
    smime_deinit();
}


int
smime_related_var(struct pine *ps, struct variable *var)
{
    return(var == &ps->vars[V_PUBLICCERT_DIR] ||
	   var == &ps->vars[V_PUBLICCERT_CONTAINER] ||
	   var == &ps->vars[V_PRIVATEKEY_DIR] ||
	   var == &ps->vars[V_PRIVATEKEY_CONTAINER] ||
	   var == &ps->vars[V_CACERT_DIR] ||
	   var == &ps->vars[V_CACERT_CONTAINER]);
}

void
smime_config_init_display(struct pine *ps, CONF_S **ctmp, CONF_S **first_line)
{
    char            tmp[200];
    int		    i, ind, ln = 0;
    struct	    variable  *vtmp;
    CONF_S         *ctmpb;
    FEATURE_S      *feature;

    /* find longest variable name */
    for(vtmp = ps->vars; vtmp->name; vtmp++){
	if(!(smime_related_var(ps, vtmp)))
	  continue;

	if((i = utf8_width(pretty_var_name(vtmp->name))) > ln)
	  ln = i;
    }

    for(vtmp = ps->vars; vtmp->name; vtmp++){
	if(!(smime_related_var(ps, vtmp)))
	  continue;

	new_confline(ctmp)->var = vtmp;
	if(first_line && !*first_line)
	  *first_line = *ctmp;

	(*ctmp)->valoffset = ln+3;
	(*ctmp)->keymenu   = &config_text_keymenu;
	(*ctmp)->help      = config_help(vtmp - ps->vars, 0);
	(*ctmp)->tool	   = text_tool;

	utf8_snprintf(tmp, sizeof(tmp), "%-*.100w =", ln, pretty_var_name(vtmp->name));
	tmp[sizeof(tmp)-1] = '\0';

	(*ctmp)->varname  = cpystr(tmp);
	(*ctmp)->varnamep = (*ctmp);
	(*ctmp)->flags    = CF_STARTITEM;
	(*ctmp)->value    = pretty_value(ps, *ctmp);
    }


    vtmp = &ps->vars[V_FEATURE_LIST];

    new_confline(ctmp);
    ctmpb = (*ctmp);
    (*ctmp)->flags		 |= CF_NOSELECT | CF_STARTITEM;
    (*ctmp)->keymenu		  = &config_checkbox_keymenu;
    (*ctmp)->tool		  = NULL;

    /* put a nice delimiter before list */
    new_confline(ctmp)->var = NULL;
    (*ctmp)->varnamep		  = ctmpb;
    (*ctmp)->keymenu		  = &config_checkbox_keymenu;
    (*ctmp)->help		  = NO_HELP;
    (*ctmp)->tool		  = checkbox_tool;
    (*ctmp)->valoffset		  = feature_indent();
    (*ctmp)->flags		 |= CF_NOSELECT;
    (*ctmp)->value = cpystr("Set    Feature Name");

    new_confline(ctmp)->var = NULL;
    (*ctmp)->varnamep		  = ctmpb;
    (*ctmp)->keymenu		  = &config_checkbox_keymenu;
    (*ctmp)->help		  = NO_HELP;
    (*ctmp)->tool		  = checkbox_tool;
    (*ctmp)->valoffset		  = feature_indent();
    (*ctmp)->flags		 |= CF_NOSELECT;
    (*ctmp)->value = cpystr("---  ----------------------");

    ind = feature_list_index(F_DONT_DO_SMIME);
    feature = feature_list(ind);
    new_confline(ctmp)->var 	= vtmp;
    (*ctmp)->varnamep		= ctmpb;
    (*ctmp)->keymenu		= &config_checkbox_keymenu;
    (*ctmp)->help		= config_help(vtmp-ps->vars, feature->id);
    (*ctmp)->tool		= checkbox_tool;
    (*ctmp)->valoffset		= feature_indent();
    (*ctmp)->varmem		= ind;
    (*ctmp)->value		= pretty_value(ps, (*ctmp));

    ind = feature_list_index(F_ENCRYPT_DEFAULT_ON);
    feature = feature_list(ind);
    new_confline(ctmp)->var 	= vtmp;
    (*ctmp)->varnamep		= ctmpb;
    (*ctmp)->keymenu		= &config_checkbox_keymenu;
    (*ctmp)->help		= config_help(vtmp-ps->vars, feature->id);
    (*ctmp)->tool		= checkbox_tool;
    (*ctmp)->valoffset		= feature_indent();
    (*ctmp)->varmem		= ind;
    (*ctmp)->value		= pretty_value(ps, (*ctmp));

    ind = feature_list_index(F_REMEMBER_SMIME_PASSPHRASE);
    feature = feature_list(ind);
    new_confline(ctmp)->var 	= vtmp;
    (*ctmp)->varnamep		= ctmpb;
    (*ctmp)->keymenu		= &config_checkbox_keymenu;
    (*ctmp)->help		= config_help(vtmp-ps->vars, feature->id);
    (*ctmp)->tool		= checkbox_tool;
    (*ctmp)->valoffset		= feature_indent();
    (*ctmp)->varmem		= ind;
    (*ctmp)->value		= pretty_value(ps, (*ctmp));

    ind = feature_list_index(F_SIGN_DEFAULT_ON);
    feature = feature_list(ind);
    new_confline(ctmp)->var 	= vtmp;
    (*ctmp)->varnamep		= ctmpb;
    (*ctmp)->keymenu		= &config_checkbox_keymenu;
    (*ctmp)->help		= config_help(vtmp-ps->vars, feature->id);
    (*ctmp)->tool		= checkbox_tool;
    (*ctmp)->valoffset		= feature_indent();
    (*ctmp)->varmem		= ind;
    (*ctmp)->value		= pretty_value(ps, (*ctmp));

#ifdef APPLEKEYCHAIN
    new_confline(ctmp);
    (*ctmp)->flags |= CF_NOSELECT | CF_B_LINE;

    new_confline(ctmp);
    (*ctmp)->flags |= CF_NOSELECT;
    (*ctmp)->value = cpystr(_("Mac OS X specific features"));

    ind = feature_list_index(F_PUBLICCERTS_IN_KEYCHAIN);
    feature = feature_list(ind);
    new_confline(ctmp)->var 	= vtmp;
    (*ctmp)->varnamep		= ctmpb;
    (*ctmp)->keymenu		= &config_checkbox_keymenu;
    (*ctmp)->help		= config_help(vtmp-ps->vars, feature->id);
    (*ctmp)->tool		= checkbox_tool;
    (*ctmp)->valoffset		= feature_indent();
    (*ctmp)->varmem		= ind;
    (*ctmp)->value		= pretty_value(ps, (*ctmp));
#endif /* APPLEKEYCHAIN */

    new_confline(ctmp);
    (*ctmp)->flags |= CF_NOSELECT | CF_B_LINE;

    new_confline(ctmp);
    (*ctmp)->flags |= CF_NOSELECT;
    (*ctmp)->value = cpystr("---------------------------------------------------------------------------");

    new_confline(ctmp);
    (*ctmp)->flags |= CF_NOSELECT;
    (*ctmp)->value = cpystr(_("Be careful with the following commands, they REPLACE contents in the target"));

    new_confline(ctmp);
    (*ctmp)->flags |= CF_NOSELECT;
    (*ctmp)->value = cpystr("---------------------------------------------------------------------------");

    new_confline(ctmp);
    (*ctmp)->flags |= CF_NOSELECT | CF_B_LINE;

    /* copy public directory to container */
    new_confline(ctmp);
    (*ctmp)->tool           = smime_helper_tool;
    (*ctmp)->keymenu        = &config_smime_helper_keymenu;
    (*ctmp)->help           = h_config_smime_transfer_pub_to_con;
    (*ctmp)->value          = cpystr(_("Transfer public certs FROM directory TO container"));
    (*ctmp)->varmem         = 1;

    /* copy private directory to container */
    new_confline(ctmp);
    (*ctmp)->tool           = smime_helper_tool;
    (*ctmp)->keymenu        = &config_smime_helper_keymenu;
    (*ctmp)->help           = h_config_smime_transfer_priv_to_con;
    (*ctmp)->value          = cpystr(_("Transfer private keys FROM directory TO container"));
    (*ctmp)->varmem         = 3;

    /* copy cacert directory to container */
    new_confline(ctmp);
    (*ctmp)->tool           = smime_helper_tool;
    (*ctmp)->keymenu        = &config_smime_helper_keymenu;
    (*ctmp)->help           = h_config_smime_transfer_cacert_to_con;
    (*ctmp)->value          = cpystr(_("Transfer CA certs FROM directory TO container"));
    (*ctmp)->varmem         = 5;

    new_confline(ctmp)->var = vtmp;
    (*ctmp)->flags |= CF_NOSELECT | CF_B_LINE;

    /* copy public container to directory */
    new_confline(ctmp);
    (*ctmp)->tool           = smime_helper_tool;
    (*ctmp)->keymenu        = &config_smime_helper_keymenu;
    (*ctmp)->help           = h_config_smime_transfer_pub_to_dir;
    (*ctmp)->value          = cpystr(_("Transfer public certs FROM container TO directory"));
    (*ctmp)->varmem         = 2;

    /* copy private container to directory */
    new_confline(ctmp);
    (*ctmp)->tool           = smime_helper_tool;
    (*ctmp)->keymenu        = &config_smime_helper_keymenu;
    (*ctmp)->help           = h_config_smime_transfer_priv_to_dir;
    (*ctmp)->value          = cpystr(_("Transfer private keys FROM container TO directory"));
    (*ctmp)->varmem         = 4;

    /* copy cacert container to directory */
    new_confline(ctmp);
    (*ctmp)->tool           = smime_helper_tool;
    (*ctmp)->keymenu        = &config_smime_helper_keymenu;
    (*ctmp)->help           = h_config_smime_transfer_cacert_to_dir;
    (*ctmp)->value          = cpystr(_("Transfer CA certs FROM container TO directory"));
    (*ctmp)->varmem         = 6;

#ifdef APPLEKEYCHAIN

    new_confline(ctmp)->var = vtmp;
    (*ctmp)->flags |= CF_NOSELECT | CF_B_LINE;

    /* copy public container to keychain */
    new_confline(ctmp);
    (*ctmp)->tool           = smime_helper_tool;
    (*ctmp)->keymenu        = &config_smime_helper_keymenu;
    (*ctmp)->help           = h_config_smime_transfer_pubcon_to_key;
    (*ctmp)->value          = cpystr(_("Transfer public certs FROM container TO keychain"));
    (*ctmp)->varmem         = 7;

    /* copy public keychain to container */
    new_confline(ctmp);
    (*ctmp)->tool           = smime_helper_tool;
    (*ctmp)->keymenu        = &config_smime_helper_keymenu;
    (*ctmp)->help           = h_config_smime_transfer_pubkey_to_con;
    (*ctmp)->value          = cpystr(_("Transfer public certs FROM keychain TO container"));
    (*ctmp)->varmem         = 8;

#endif /* APPLEKEYCHAIN */
}


int
smime_helper_tool(struct pine *ps, int cmd, CONF_S **cl, unsigned flags)
{
    int rv = 0;

    switch(cmd){
      case MC_CHOICE:
	switch((*cl)->varmem){
	  case 1:
	    rv = copy_publiccert_dir_to_container();
	    if(rv == 0)
	      q_status_message(SM_ORDER, 1, 3, _("Public certs transferred to container"));
	    else{
		q_status_message(SM_ORDER, 3, 3, _("Problem transferring certs"));
		rv = 0;
	    }

	    break;

	  case 2:
	    rv = copy_publiccert_container_to_dir();
	    if(rv == 0)
	      q_status_message(SM_ORDER, 1, 3, _("Public certs transferred to directory, delete Container config to use"));
	    else{
		q_status_message(SM_ORDER, 3, 3, _("Problem transferring certs"));
		rv = 0;
	    }

	    break;

	  case 3:
	    rv = copy_privatecert_dir_to_container();
	    if(rv == 0)
	      q_status_message(SM_ORDER, 1, 3, _("Private keys transferred to container"));
	    else{
		q_status_message(SM_ORDER, 3, 3, _("Problem transferring certs"));
		rv = 0;
	    }

	    break;

	  case 4:
	    rv = copy_privatecert_container_to_dir();
	    if(rv == 0)
	      q_status_message(SM_ORDER, 1, 3, _("Private keys transferred to directory, delete Container config to use"));
	    else{
		q_status_message(SM_ORDER, 3, 3, _("Problem transferring certs"));
		rv = 0;
	    }

	    break;

	  case 5:
	    rv = copy_cacert_dir_to_container();
	    if(rv == 0)
	      q_status_message(SM_ORDER, 1, 3, _("CA certs transferred to container"));
	    else{
		q_status_message(SM_ORDER, 3, 3, _("Problem transferring certs"));
		rv = 0;
	    }

	    break;

	  case 6:
	    rv = copy_cacert_container_to_dir();
	    if(rv == 0)
	      q_status_message(SM_ORDER, 1, 3, _("CA certs transferred to directory, delete Container config to use"));
	    else{
		q_status_message(SM_ORDER, 3, 3, _("Problem transferring certs"));
		rv = 0;
	    }

	    break;

#ifdef APPLEKEYCHAIN
	  case 7:
	    rv = copy_publiccert_container_to_keychain();
	    if(rv == 0)
	      q_status_message(SM_ORDER, 1, 3, _("Public certs transferred to keychain"));
	    else{
		q_status_message(SM_ORDER, 3, 3, _("Command not implemented yet"));
		rv = 0;
	    }

	    break;

	  case 8:
	    rv = copy_publiccert_keychain_to_container();
	    if(rv == 0)
	      q_status_message(SM_ORDER, 1, 3, _("Public certs transferred to container"));
	    else{
		q_status_message(SM_ORDER, 3, 3, _("Command not implemented yet"));
		rv = 0;
	    }

	    break;
#endif /* APPLEKEYCHAIN */

	  default:
	    rv = -1;
	    break;
	}

	break;

      case MC_EXIT:
	rv = config_exit_cmd(flags);
	break;

      default:
	rv = -1;
	break;
    }

    return rv;
}


/*
 * Compare saved user_val with current user_val to see if it changed.
 * If any have changed, change it back and take the appropriate action.
 */
void
revert_to_saved_smime_config(struct pine *ps, SAVED_CONFIG_S *vsave)
{
    struct variable *vreal;
    SAVED_CONFIG_S  *v;
    int i, n;
    int changed = 0;
    char *pval, **apval, **lval, ***alval;

    v = vsave;
    for(vreal = ps->vars; vreal->name; vreal++,v++){
	if(!(smime_related_var(ps, vreal) || vreal==&ps->vars[V_FEATURE_LIST]))
	  continue;

	if(vreal->is_list){
	    lval  = LVAL(vreal, ew);
	    alval = ALVAL(vreal, ew);

	    if((v->saved_user_val.l && !lval)
	       || (!v->saved_user_val.l && lval))
	      changed++;
	    else if(!v->saved_user_val.l && !lval)
	      ;/* no change, nothing to do */
	    else
	      for(i = 0; v->saved_user_val.l[i] || lval[i]; i++)
		if((v->saved_user_val.l[i]
		      && (!lval[i]
			 || strcmp(v->saved_user_val.l[i], lval[i])))
		   ||
		     (!v->saved_user_val.l[i] && lval[i])){
		    changed++;
		    break;
		}
	    
	    if(changed){
		char  **list;

		if(alval){
		    if(*alval)
		      free_list_array(alval);
		
		    /* copy back the original one */
		    if(v->saved_user_val.l){
			list = v->saved_user_val.l;
			n = 0;
			/* count how many */
			while(list[n])
			  n++;

			*alval = (char **)fs_get((n+1) * sizeof(char *));

			for(i = 0; i < n; i++)
			  (*alval)[i] = cpystr(v->saved_user_val.l[i]);

			(*alval)[n] = NULL;
		    }
		}
	    }
	}
	else{
	    pval  = PVAL(vreal, ew);
	    apval = APVAL(vreal, ew);

	    if((v->saved_user_val.p &&
	        (!pval || strcmp(v->saved_user_val.p, pval))) ||
	       (!v->saved_user_val.p && pval)){
		/* It changed, fix it */
		changed++;
		if(apval){
		    /* free the changed value */
		    if(*apval)
		      fs_give((void **)apval);

		    if(v->saved_user_val.p)
		      *apval = cpystr(v->saved_user_val.p);
		}
	    }
	}

	if(changed){
	    if(vreal == &ps->vars[V_FEATURE_LIST])
	      set_feature_list_current_val(vreal);
	    else
	      set_current_val(vreal, TRUE, FALSE);

	    fix_side_effects(ps, vreal, 1);
	}
    }
}


SAVED_CONFIG_S *
save_smime_config_vars(struct pine *ps)
{
    struct variable *vreal;
    SAVED_CONFIG_S *vsave, *v;

    vsave = (SAVED_CONFIG_S *)fs_get((V_LAST_VAR+1)*sizeof(SAVED_CONFIG_S));
    memset((void *)vsave, 0, (V_LAST_VAR+1)*sizeof(SAVED_CONFIG_S));
    v = vsave;
    for(vreal = ps->vars; vreal->name; vreal++,v++){
	if(!(smime_related_var(ps, vreal) || vreal==&ps->vars[V_FEATURE_LIST]))
	  continue;
	
	if(vreal->is_list){
	    int n, i;
	    char **list;

	    if(LVAL(vreal, ew)){
		/* count how many */
		n = 0;
		list = LVAL(vreal, ew);
		while(list[n])
		  n++;

		v->saved_user_val.l = (char **)fs_get((n+1) * sizeof(char *));
		memset((void *)v->saved_user_val.l, 0, (n+1)*sizeof(char *));
		for(i = 0; i < n; i++)
		  v->saved_user_val.l[i] = cpystr(list[i]);

		v->saved_user_val.l[n] = NULL;
	    }
	}
	else{
	    if(PVAL(vreal, ew))
	      v->saved_user_val.p = cpystr(PVAL(vreal, ew));
	}
    }

    return(vsave);
}


void
free_saved_smime_config(struct pine *ps, SAVED_CONFIG_S **vsavep)
{
    struct variable *vreal;
    SAVED_CONFIG_S  *v;

    if(vsavep && *vsavep){
	for(v = *vsavep, vreal = ps->vars; vreal->name; vreal++,v++){
	    if(!(smime_related_var(ps, vreal)))
	      continue;
	    
	    if(vreal->is_list){  /* free saved_user_val.l */
		if(v && v->saved_user_val.l)
		  free_list_array(&v->saved_user_val.l);
	    }
	    else if(v && v->saved_user_val.p)
	      fs_give((void **)&v->saved_user_val.p);
	}

	fs_give((void **)vsavep);
    }
}

#endif /* SMIME */
