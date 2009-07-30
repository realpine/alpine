#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: mimedesc.c 1142 2008-08-13 17:22:21Z hubert@u.washington.edu $";
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
#include "../pith/mimedesc.h"
#include "../pith/mimetype.h"
#include "../pith/state.h"
#include "../pith/conf.h"
#include "../pith/mailview.h"
#include "../pith/rfc2231.h"
#include "../pith/editorial.h"
#include "../pith/mailpart.h"
#include "../pith/mailcap.h"
#include "../pith/smime.h"


/* internal prototypes */
int         mime_known_text_subtype(char *);
ATTACH_S   *next_attachment(void);
void	    format_mime_size(char *, size_t, BODY *, int);
int	    mime_show(BODY *);


/*
 * Def's to help in sorting out multipart/alternative
 */
#define	SHOW_NONE	0
#define	SHOW_PARTS	1
#define	SHOW_ALL_EXT	2
#define	SHOW_ALL	3

/*
 * Def's to control format_mime_size output
 */
#define	FMS_NONE	0x00
#define	FMS_SPACE	0x01


/*----------------------------------------------------------------------
    Add lines to the attachments structure
    
  Args: body   -- body of the part being described
        prefix -- The prefix for numbering the parts
        num    -- The number of this specific part
        should_show -- Flag indicating which of alternate parts should be shown
	multalt     -- Flag indicating the part is one of the multipart
			alternative parts (so suppress editorial comment)

Result: The ps_global->attachments data structure is filled in. This
is called recursively to descend through all the parts of a message. 
The description strings filled in are malloced and should be freed.

  ----*/
void
describe_mime(struct mail_bodystruct *body, char *prefix, int num,
	      int should_show, int multalt, int flags)
{
    PART      *part;
    char       numx[512], string[800], *description;
    int        n, named = 0, can_display_ext;
    ATTACH_S  *a;

    if(!body)
      return;

    if(body->type == TYPEMULTIPART){
	int alt_to_show = 0;

        if(strucmp(body->subtype, "alternative") == 0){
	    int effort, best_effort = SHOW_NONE;

	    /*---- Figure out which alternative part to display ---*/
	    /*
	     * This is kind of complicated because some TEXT types
	     * are more displayable than others.  We don't want to
	     * commit to displaying a text-part alternative that we
	     * don't directly recognize unless that's all there is.
	     */
	    for(part=body->nested.part, n=1; part; part=part->next, n++)
	      if(flags & FM_FORCEPREFPLN
	         || (!(flags & FM_FORCENOPREFPLN)
		     && F_ON(F_PREFER_PLAIN_TEXT, ps_global)
		     && part->body.type == TYPETEXT
		     && (!part->body.subtype
			 || !strucmp(part->body.subtype, "PLAIN")))){
		if((effort = mime_show(&part->body)) != SHOW_ALL_EXT){
		  best_effort = effort;
		  alt_to_show = n;
		  break;
		}
	      }
	      else if((effort = mime_show(&part->body)) >= best_effort
		      && (part->body.type != TYPETEXT || mime_known_text_subtype(part->body.subtype))
		      && effort != SHOW_ALL_EXT){
		best_effort = effort;
		alt_to_show = n;
	      }
	      else if(part->body.type == TYPETEXT && alt_to_show == 0){
		  best_effort = effort;
		  alt_to_show = n;
	      }
	}
	else if(!strucmp(body->subtype, "digest")){
	    memset(a = next_attachment(), 0, sizeof(ATTACH_S));
	    if(*prefix){
		prefix[n = strlen(prefix) - 1] = '\0';
		a->number		       = cpystr(prefix);
		prefix[n] = '.';
	    }
	    else
	      a->number = cpystr("");

	    a->description     = cpystr("Multipart/Digest");
	    a->body	       = body;
	    a->can_display     = MCD_INTERNAL;
	    (a+1)->description = NULL;
	}
#ifdef SMIME
	else if(!strucmp(body->subtype, OUR_PKCS7_ENCLOSURE_SUBTYPE)){
	    memset(a = next_attachment(), 0, sizeof(ATTACH_S));
	    if(*prefix){
		prefix[n = strlen(prefix) - 1] = '\0';
		a->number		       = cpystr(prefix);
		prefix[n] = '.';
	    }
	    else
	      a->number = cpystr("");

	    a->description     = body->description ? cpystr(body->description)
						   : cpystr("");
	    a->body	       = body;
	    a->can_display     = MCD_INTERNAL;
	    (a+1)->description = NULL;
	}
#endif /* SMIME */
	else if(mailcap_can_display(body->type, body->subtype, body, 0)
		|| (can_display_ext 
		    = mailcap_can_display(body->type, body->subtype, body, 1))){
	    memset(a = next_attachment(), 0, sizeof(ATTACH_S));
	    if(*prefix){
		prefix[n = strlen(prefix) - 1] = '\0';
		a->number		       = cpystr(prefix);
		prefix[n] = '.';
	    }
	    else
	      a->number = cpystr("");

	    snprintf(string, sizeof(string), "%s/%s", body_type_names(body->type),
		     body->subtype);
	    string[sizeof(string)-1] = '\0';
	    a->description	   = cpystr(string);
	    a->body		   = body;
	    a->can_display	   = MCD_EXTERNAL;
	    if(can_display_ext)
	      a->can_display      |= MCD_EXT_PROMPT;
	    (a+1)->description	   = NULL;
	}

	for(part=body->nested.part, n=1; part; part=part->next, n++){
	    snprintf(numx, sizeof(numx), "%s%d.", prefix, n);
	    numx[sizeof(numx)-1] = '\0';
	    /*
	     * Last arg to describe_mime here. If we have chosen one part
	     * of a multipart/alternative to display then we suppress
	     * the editorial messages on the other parts.
	     */
	    describe_mime(&(part->body),
			  (part->body.type == TYPEMULTIPART) ? numx : prefix,
			  n, should_show && (n == alt_to_show || !alt_to_show),
			  alt_to_show != 0, flags);
	}
    }
    else{
	char tmp1[MAILTMPLEN], tmp2[MAILTMPLEN];
	size_t ll;

	a = next_attachment();
	format_mime_size(a->size, sizeof(a->size), body, FMS_SPACE);

	a->suppress_editorial = (multalt != 0);

	snprintf(tmp1, sizeof(tmp1), "%s", body->description ? body->description : "");
	tmp1[sizeof(tmp1)-1] = '\0';
	snprintf(tmp2, sizeof(tmp2), "%s", (!body->description && body->type == TYPEMESSAGE && body->encoding <= ENCBINARY && body->subtype && strucmp(body->subtype, "rfc822") == 0 && body->nested.msg->env && body->nested.msg->env->subject) ? body->nested.msg->env->subject : "");
	tmp2[sizeof(tmp2)-1] = '\0';

	description = (body->description)
		        ? (char *) rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf,
							  SIZEOF_20KBUF, tmp1)
			: (body->type == TYPEMESSAGE
			   && body->encoding <= ENCBINARY
			   && body->subtype
			   && strucmp(body->subtype, "rfc822") == 0
			   && body->nested.msg->env
			   && body->nested.msg->env->subject)
			   ? (char *) rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf, SIZEOF_20KBUF, tmp2)
			   : (body->type == TYPEMESSAGE
			      && body->subtype
			      && !strucmp(body->subtype, "delivery-status"))
				? "Delivery Status"
				: NULL;

	description = iutf8ncpy((char *)(tmp_20k_buf+1000), description, 1000);
        snprintf(string, sizeof(string), "%s%s%s%s",
		      type_desc(body->type,body->subtype,body->parameter,
				body->disposition.type ? body->disposition.parameter : NULL, 0),
                (description && description[0]) ? ", \"" : "",
                (description && description[0]) ? description : "",
                (description && description[0]) ? "\"": "");
	string[sizeof(string)-1] =- '\0';
        a->description = cpystr(string);
        a->body        = body;

	if(body->disposition.type){
	    named = strucmp(body->disposition.type, "inline");
	}
	else{
	    char *value;


	    /*
	     * This test remains for backward compatibility
	     */
	    if(body && (value = parameter_val(body->parameter, "name")) != NULL){
		named = strucmp(value, "Message Body");
		fs_give((void **) &value);
	    }
	}

	/*
	 * Make sure we have the tools available to display the
	 * type/subtype, *AND* that we can decode it if needed.
	 * Of course, if it's text, we display it anyway in the
	 * mail_view_screen so put off testing mailcap until we're
	 * explicitly asked to display that segment 'cause it could
	 * be expensive to test...
	 */
	if((body->type == TYPETEXT && !named)
	   || MIME_VCARD(body->type,body->subtype)){
	    a->test_deferred = 1;
	    a->can_display = MCD_INTERNAL;
	}
	else{
	    a->test_deferred = 0;
	    a->can_display = mime_can_display(body->type, body->subtype, body);
	}

	/*
	 * Deferred means we can display it
	 */
	a->shown = ((a->can_display & MCD_INTERNAL)
		    && !MIME_VCARD(body->type,body->subtype)
		    && (!named || multalt
			|| (body->type == TYPETEXT && num == 1
			    && !(*prefix && strcmp(prefix,"1."))))
		    && (body->type != TYPEMESSAGE
			|| (body->type == TYPEMESSAGE
			    && body->encoding <= ENCBINARY))
		    && should_show);
	ll = (strlen(prefix) + 16) * sizeof(char);
	a->number = (char *) fs_get(ll);
        snprintf(a->number, ll, "%s%d",prefix, num);
	a->number[ll-1] = '\0';
        (a+1)->description = NULL;
        if(body->type == TYPEMESSAGE && body->encoding <= ENCBINARY
	   && body->subtype && strucmp(body->subtype, "rfc822") == 0){
	    body = body->nested.msg->body;
	    snprintf(numx, sizeof(numx), "%.*s%d.", sizeof(numx)-20, prefix, num);
	    numx[sizeof(numx)-1] = '\0';
	    describe_mime(body, numx, 1, should_show, 0, flags);
        }
    }
}


int
mime_known_text_subtype(char *subtype)
{
    char **p;
    static char *known_types[] = {
	"plain",
	"html",
	"enriched",
	"richtext",
	NULL
    };

    if(!(subtype && *subtype))
      return(1);

    for(p = known_types; *p; p++)
      if(!strucmp(subtype, *p))
	return(1);
    return(0);
}


/*
 * Returns attribute value or NULL.
 * Value returned needs to be freed by caller
 */
char *
parameter_val(PARAMETER *param, char *attribute)
{
    if(!(param && attribute && attribute[0]))
      return(NULL);

    return(rfc2231_get_param(param, attribute, NULL, NULL));
}


/*
 * Get sender_filename, the filename set by the sender in the attachment.
 * If a sender_filename buffer is passed in, the answer is copied to it
 * and a pointer to it is returned. If sender_filename is passed in as NULL
 * then an allocated copy of the sender filename is returned instead.
 * If ext_ptr is non-NULL then it is set to point to the extension name.
 * It is not a separate copy, it points into the string sender_filename.
 */
char *
get_filename_parameter(char *sender_filename, size_t sfsize, BODY *body, char **ext_ptr)
{
    char *p = NULL;
    char *decoded_name = NULL;
    char *filename = NULL;
    char  tmp[1000];

    if(!body)
      return(NULL);

    if(sender_filename){
	if(sfsize <= 0)
	  return(NULL);

	sender_filename[0] = '\0';
    }

    /*
     * First check for Content-Disposition's "filename" parameter and
     * if that isn't found for the deprecated Content-Type "name" parameter.
     */
    if((p = parameter_val(body->disposition.parameter, "filename"))
       || (p = parameter_val(body->parameter, "name"))){

	/*
	 * If somebody sent us and incorrectly rfc2047 encoded
	 * parameter value instead of what rfc2231 suggest we
	 * grudglingly try to fix it.
	 */
	if(p[0] == '=' && p[1] == '?')
	  decoded_name = (char *) rfc1522_decode_to_utf8((unsigned char *) tmp,
							 sizeof(tmp), p);

	if(!decoded_name)
	  decoded_name = p;

	filename = last_cmpnt(decoded_name);

	if(!filename)
	  filename = decoded_name;
    }

    if(filename){
	if(sender_filename){
	    strncpy(sender_filename, filename, sfsize-1);
	    sender_filename[sfsize-1] = '\0';
	}
	else
	  sender_filename = cpystr(filename);
    }

    if(p)
      fs_give((void **) &p);

    /* ext_ptr will end up pointing into sender_filename string */
    if(ext_ptr && sender_filename)
      mt_get_file_ext(sender_filename, ext_ptr);

    return(sender_filename);
}


/*----------------------------------------------------------------------
  Return a pointer to the next attachment struct
    
  Args: none

  ----*/
ATTACH_S *
next_attachment(void)
{
    ATTACH_S *a;
    int       n;

    for(a = ps_global->atmts; a->description; a++)
      ;

    if((n = a - ps_global->atmts) + 1 >= ps_global->atmts_allocated){
	ps_global->atmts_allocated *= 2;
	fs_resize((void **)&ps_global->atmts,
		  ps_global->atmts_allocated * sizeof(ATTACH_S));
	a = &ps_global->atmts[n];
    }

    return(a);
}



/*----------------------------------------------------------------------
   Zero out the attachments structure and free up storage
  ----*/
void
zero_atmts(ATTACH_S *atmts)
{
    ATTACH_S *a;

    for(a = atmts; a->description != NULL; a++){
	fs_give((void **)&(a->description));
	fs_give((void **)&(a->number));
    }

    atmts->description = NULL;
}


char *
body_type_names(int t)
{
#define TLEN 31
    static char body_type[TLEN + 1];
    char   *p;

    body_type[0] = '\0';
    strncpy(body_type,				/* copy the given type */
	    (t > -1 && t < TYPEMAX && body_types[t])
	      ? body_types[t] : "Other", TLEN);
    body_type[sizeof(body_type)-1] = '\0';

    for(p = body_type + 1; *p; p++)		/* make it presentable */
      if(isascii((unsigned char) (*p)) && isupper((unsigned char) (*p)))
	*p = tolower((unsigned char)(*p));

    return(body_type);				/* present it */
}


/*----------------------------------------------------------------------
  Mapping table use to neatly display charset parameters
 ----*/

static struct set_names {
    char *rfcname,
	 *humanname;
} charset_names[] = {
    {"US-ASCII",		"Plain Text"},
    {"ISO-8859-1",		"Latin 1 (Western Europe)"},
    {"ISO-8859-2",		"Latin 2 (Eastern Europe)"},
    {"ISO-8859-3",		"Latin 3 (Southern Europe)"},
    {"ISO-8859-4",		"Latin 4 (Northern Europe)"},
    {"ISO-8859-5",		"Latin & Cyrillic"},
    {"ISO-8859-6",		"Latin & Arabic"},
    {"ISO-8859-7",		"Latin & Greek"},
    {"ISO-8859-8",		"Latin & Hebrew"},
    {"ISO-8859-9",		"Latin 5 (Turkish)"},
    {"ISO-8859-10",		"Latin 6 (Nordic)"},
    {"ISO-8859-11",		"Latin & Thai"},
    {"ISO-8859-13",		"Latin 7 (Baltic)"},
    {"ISO-8859-14",		"Latin 8 (Celtic)"},
    {"ISO-8859-15",		"Latin 9 (Euro)"},
    {"KOI8-R",			"Latin & Russian"},
    {"KOI8-U",			"Latin & Ukranian"},
    {"VISCII",			"Latin & Vietnamese"},
    {"GB2312",			"Latin & Simplified Chinese"},
    {"BIG5",			"Latin & Traditional Chinese"},
    {"EUC-JP",			"Latin & Japanese"},
    {"Shift-JIS",		"Latin & Japanese"},
    {"Shift_JIS",		"Latin & Japanese"},
    {"EUC-KR",			"Latin & Korean"},
    {"ISO-2022-CN",		"Latin & Chinese"},
    {"ISO-2022-JP",		"Latin & Japanese"},
    {"ISO-2022-KR",		"Latin & Korean"},
    {"UTF-7",			"7-bit encoded Unicode"},
    {"UTF-8",			"Internet-standard Unicode"},
    {"ISO-2022-JP-2",		"Multilingual"},
    {NULL,			NULL}
};


/*----------------------------------------------------------------------
  Return a nicely formatted discription of the type of the part
 ----*/

char *
type_desc(int type, char *subtype, PARAMETER *params, PARAMETER *disp_params, int full)
{
    static char  type_d[200];
    int		 i;
    char	*p, *parmval;

    p = type_d;
    sstrncpy(&p, body_type_names(type), sizeof(type_d)-(p-type_d));
    if(full && subtype){
	*p++ = '/';
	sstrncpy(&p, subtype, sizeof(type_d)-(p-type_d));
    }

    type_d[sizeof(type_d)-1] = '\0';

    switch(type){
      case TYPETEXT:
	parmval = parameter_val(params, "charset");

        if(parmval){
	    for(i = 0; charset_names[i].rfcname; i++)
	      if(!strucmp(parmval, charset_names[i].rfcname)){
		  if(!strucmp(parmval, ps_global->display_charmap
			      ? ps_global->display_charmap : "us-ascii")
		     || !strucmp(parmval, "us-ascii"))
		    i = -1;

		  break;
	      }

	    if(i >= 0){			/* charset to write */
		if(charset_names[i].rfcname){
		    sstrncpy(&p, " (charset: ", sizeof(type_d)-(p-type_d));
		    sstrncpy(&p, charset_names[i].rfcname
				  ? charset_names[i].rfcname : "Unknown", sizeof(type_d)-(p-type_d));
		    if(full){
			sstrncpy(&p, " \"", sizeof(type_d)-(p-type_d));
			sstrncpy(&p, charset_names[i].humanname
				? charset_names[i].humanname
				: parmval, sizeof(type_d)-(p-type_d));
			if(sizeof(type_d)-(p-type_d) > 0)
			  *p++ = '\"';
		    }

		    sstrncpy(&p, ")", sizeof(type_d)-(p-type_d));
		}
		else{
		    sstrncpy(&p, " (charset: ", sizeof(type_d)-(p-type_d));
		    sstrncpy(&p, parmval, sizeof(type_d)-(p-type_d));
		    sstrncpy(&p, ")", sizeof(type_d)-(p-type_d));
		}
	    }

	    fs_give((void **) &parmval);
        }

        break;

      case TYPEMESSAGE:
	if(full && subtype && strucmp(subtype, "external-body") == 0)
	  if((parmval = parameter_val(params, "access-type")) != NULL){
	      snprintf(p, sizeof(type_d)-(p-type_d), " (%s%s)", full ? "Access: " : "", parmval);
	      fs_give((void **) &parmval);
	  }

	break;

      default:
        break;
    }

    if(full && type != TYPEMULTIPART && type != TYPEMESSAGE){
	if((parmval = parameter_val(params, "name")) != NULL){
	    snprintf(p, sizeof(type_d)-(p-type_d), " (Name: \"%s\")", parmval);
	    fs_give((void **) &parmval);
	}
	else if((parmval = parameter_val(disp_params, "filename")) != NULL){
	    snprintf(p, sizeof(type_d)-(p-type_d), " (Filename: \"%s\")", parmval);
	    fs_give((void **) &parmval);
	}
    }

    type_d[sizeof(type_d)-1] = '\0';

    return(type_d);
}
     



void
format_mime_size(char *string, size_t stringlen, struct mail_bodystruct *b, int flags)
{
    char tmp[10], *p = NULL;
    char *origstring;


    if(stringlen <= 0)
      return;

    origstring = string;

    if(flags & FMS_SPACE)
      *string++ = ' ';

    switch(b->encoding){
      case ENCBASE64 :
	if(b->type == TYPETEXT){
	  if(flags & FMS_SPACE)
	    *(string-1) = '~';
	  else
	    *string++ = '~';
	}

	strncpy(p = string, byte_string((3 * b->size.bytes) / 4), stringlen-(string-origstring));
	break;

      default :
      case ENCQUOTEDPRINTABLE :
	if(flags & FMS_SPACE)
	  *(string-1) = '~';
	else
	  *string++ = '~';

      case ENC8BIT :
      case ENC7BIT :
	if(b->type == TYPETEXT)
	  /* lines with no CRLF aren't counted, just add one so it makes more sense */
	  snprintf(string, stringlen-(string-origstring), "%s lines", comatose(b->size.lines+1));
	else
	  strncpy(p = string, byte_string(b->size.bytes), stringlen-(string-origstring));

	break;
    }

    origstring[stringlen-1] = '\0';

    if(p){
	for(; *p && (isascii((unsigned char) *p) && (isdigit((unsigned char) *p)
		     || ispunct((unsigned char) *p))); p++)
	  ;

	snprintf(tmp, sizeof(tmp), (flags & FMS_SPACE) ? " %-5.5s" : " %s", p);
	tmp[sizeof(tmp)-1] = '\0';
	strncpy(p, tmp, stringlen-(p-origstring));
    }

    origstring[stringlen-1] = '\0';
}

        

/*----------------------------------------------------------------------
   Determine if we can show all, some or none of the parts of a body

Args: body --- The message body to check

Returns: SHOW_ALL, SHOW_ALL_EXT, SHOW_PART or SHOW_NONE depending on
	 how much of the body can be shown and who can show it.
 ----*/     
int
mime_show(struct mail_bodystruct *body)
{
    int   effort, best_effort;
    PART *p;

    if(!body)
      return(SHOW_NONE);

    switch(body->type) {
      case TYPEMESSAGE:
	if(!strucmp(body->subtype, "rfc822"))
	  return(mime_show(body->nested.msg->body) == SHOW_ALL
		 ? SHOW_ALL: SHOW_PARTS);
	/* else fall thru to default case... */

      default:
	/*
	 * Since we're testing for internal displayability, give the
	 * internal result over an external viewer
	 */
	effort = mime_can_display(body->type, body->subtype, body);
	if(effort == MCD_NONE)
	  return(SHOW_NONE);
	else if(effort & MCD_INTERNAL)
	  return(SHOW_ALL);
	else
	  return(SHOW_ALL_EXT);

      case TYPEMULTIPART:
	best_effort = SHOW_NONE;
        for(p = body->nested.part; p; p = p->next)
	  if((effort = mime_show(&p->body)) > best_effort)
	    best_effort = effort;

	return(best_effort);
    }
}


/*
 * fcc_size_guess
 */
long
fcc_size_guess(struct mail_bodystruct *body)
{
    long  size = 0L;

    if(body){
	if(body->type == TYPEMULTIPART){
	    PART *part;

	    for(part = body->nested.part; part; part = part->next)
	      size += fcc_size_guess(&part->body);
	}
	else{
	    size = body->size.bytes;
	    /*
	     * If it is ENCBINARY we will be base64 encoding it. This
	     * ideally increases the size by a factor of 4/3, but there
	     * is a per-line increase in that because of the CRLFs and
	     * because the number of characters in the line might not
	     * be a factor of 3. So push it up by 3/2 instead. This still
	     * won't catch all the cases. In particular, attachements with
	     * lots of short lines (< 10) will expand by more than that,
	     * but that's ok since this is an optimization. That's why
	     * so_cs_puts uses the 3/2 factor when it does a resize, so
	     * that it won't have to resize linearly until it gets there.
	     */
	    if(body->encoding == ENCBINARY)
	      size = 3*size/2;
	}
    }

    return(size);
}



/*----------------------------------------------------------------------
    Format a strings describing one unshown part of a Mime message

Args: number -- A string with the part number i.e. "3.2.1"
      body   -- The body part
      type   -- 1 - Not shown, but can be
                2 - Not shown, cannot be shown
                3 - Can't print
      width  -- allowed width per line of editorial comment
      pc     -- function used to write the description comment

Result: formatted description written to object ref'd by "pc"
 ----*/
char *
part_desc(char *number, BODY *body, int type, int width, int flags, gf_io_t pc)
{
    char *t;
    char buftmp[MAILTMPLEN], sizebuf[256];

    if(!gf_puts(NEWLINE, pc))
      return("No space for description");

    format_mime_size(sizebuf, 256, body, FMS_NONE);

    snprintf(buftmp, sizeof(buftmp), "%s", body->description ? body->description : "");
    buftmp[sizeof(buftmp)-1] = '\0';
    snprintf(tmp_20k_buf+10000, SIZEOF_20KBUF-10000, "Part %s, %s%.2048s%s%s %s.",
            number,
            body->description == NULL ? "" : "\"",
            body->description == NULL ? ""
	      : (char *)rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf, 10000, buftmp),
            body->description == NULL ? "" : "\"  ",
            type_desc(body->type, body->subtype, body->parameter, NULL, 1),
	    sizebuf);
    tmp_20k_buf[SIZEOF_20KBUF-1] = '\0';

    iutf8ncpy((char *)tmp_20k_buf, (char *)(tmp_20k_buf+10000), 10000);
    tmp_20k_buf[10000] = '\0';

    t = &tmp_20k_buf[strlen(tmp_20k_buf)];

#ifdef SMIME
    /* if smime and not attempting print */
    if(F_OFF(F_DONT_DO_SMIME, ps_global) && is_pkcs7_body(body) && type != 3){

    	sstrncpy(&t, "\015\012", SIZEOF_20KBUF-(t-tmp_20k_buf));
	
	if(ps_global->smime && ps_global->smime->need_passphrase){
	    sstrncpy(&t,
	    "This part is a PKCS7 S/MIME enclosure. "
	    "You may be able to view it by entering the correct passphrase "
	    "with the \"Decrypt\" command.",
	    SIZEOF_20KBUF-(t-tmp_20k_buf));
	}
	else{
	    sstrncpy(&t,
	    "This part is a PKCS7 S/MIME enclosure. "
	    "Press \"^E\" for more information.",
	    SIZEOF_20KBUF-(t-tmp_20k_buf));
	}
    
    } else 
#endif

    if(type){
	sstrncpy(&t, "\015\012", SIZEOF_20KBUF-(t-tmp_20k_buf));
	switch(type) {
	  case 1:
	    if(MIME_VCARD(body->type,body->subtype))
	      sstrncpy(&t,
	   /* TRANSLATORS: This is the description of an attachment that isn't being
	      shown but that can be viewed or saved. */
	   _("Not Shown. Use the \"V\" command to view or save to address book."), SIZEOF_20KBUF-(t-tmp_20k_buf));
	    else
	      sstrncpy(&t,
	   /* TRANSLATORS: This is the description of an attachment that isn't being
	      shown but that can be viewed or saved. */
	       _("Not Shown. Use the \"V\" command to view or save this part."), SIZEOF_20KBUF-(t-tmp_20k_buf));

	    break;

	  case 2:
	    sstrncpy(&t, "Cannot ", SIZEOF_20KBUF-(t-tmp_20k_buf));
	    if(body->type != TYPEAUDIO && body->type != TYPEVIDEO)
	      sstrncpy(&t, "dis", SIZEOF_20KBUF-(t-tmp_20k_buf));

	    sstrncpy(&t, 
	       "play this part. Press \"V\" then \"S\" to save in a file.", SIZEOF_20KBUF-(t-tmp_20k_buf));
	    break;

	  case 3:
	    sstrncpy(&t, _("Unable to print this part."), SIZEOF_20KBUF-(t-tmp_20k_buf));
	    break;
	}
    }

    if(!(t = format_editorial(tmp_20k_buf, width, flags, NULL, pc))){
	if(!gf_puts(NEWLINE, pc))
	  t = "No space for description";
    }

    return(t);
}


/*----------------------------------------------------------------------
       Can we display this type/subtype?

   Args: type       -- the MIME type to check
         subtype    -- the MIME subtype
         params     -- parameters
	 use_viewer -- tell caller he should run external viewer cmd to view

 Result: Returns:

	 MCD_NONE	if we can't display this type at all
	 MCD_INTERNAL	if we can display it internally
	 MCD_EXTERNAL	if it can be displayed via an external viewer

 ----*/
int
mime_can_display(int type, char *subtype, BODY *body)
{
    return((mailcap_can_display(type, subtype, body, 0)
	      ? MCD_EXTERNAL 
	    : (mailcap_can_display(type, subtype, body, 1) 
	       ? (MCD_EXT_PROMPT | MCD_EXTERNAL) : MCD_NONE))
	   | ((type == TYPETEXT || type == TYPEMESSAGE
	       || MIME_VCARD(type,subtype))
	        ? MCD_INTERNAL : MCD_NONE));
}
