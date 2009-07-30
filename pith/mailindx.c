#if !defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: mailindx.c 1266 2009-07-14 18:39:12Z hubert@u.washington.edu $";
#endif

/* ========================================================================
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
#include "../pith/mailindx.h"
#include "../pith/mailview.h"
#include "../pith/flag.h"
#include "../pith/icache.h"
#include "../pith/msgno.h"
#include "../pith/thread.h"
#include "../pith/strlst.h"
#include "../pith/status.h"
#include "../pith/mailcmd.h"
#include "../pith/search.h"
#include "../pith/charset.h"
#include "../pith/reply.h"
#include "../pith/bldaddr.h"
#include "../pith/addrstring.h"
#include "../pith/news.h"
#include "../pith/util.h"
#include "../pith/pattern.h"
#include "../pith/sequence.h"
#include "../pith/color.h"
#include "../pith/stream.h"
#include "../pith/string.h"
#include "../pith/send.h"
#include "../pith/options.h"
#include "../pith/ablookup.h"
#ifdef _WINDOWS
#include "../pico/osdep/mswin.h"
#endif

/*
 * pointers to formatting functions 
 */
ICE_S	       *(*format_index_line)(INDEXDATA_S *);
void		(*setup_header_widths)(MAILSTREAM *);

/*
 * pointer to optional load_overview functionality
 */
void	(*pith_opt_paint_index_hline)(MAILSTREAM *, long, ICE_S *);

/*
 * pointer to hook for saving index format state
 */
void	(*pith_opt_save_index_state)(int);

/*
 * hook to allow caller to insert cue that indicates a condensed
 * thread relationship cue
 */
int	(*pith_opt_condense_thread_cue)(PINETHRD_S *, ICE_S *, char **, size_t *, int, int);
int	(*pith_opt_truncate_sfstr)(void);


/*
 * Internal prototypes
 */
void            setup_for_thread_index_screen(void);
ICE_S	       *format_index_index_line(INDEXDATA_S *);
ICE_S	       *format_thread_index_line(INDEXDATA_S *);
int		set_index_addr(INDEXDATA_S *, char *, ADDRESS *, char *, int, char *);
int             ctype_is_fixed_length(IndexColType);
void		setup_index_header_widths(MAILSTREAM *);
void		setup_thread_header_widths(MAILSTREAM *);
int		parse_index_format(char *, INDEX_COL_S **);
int		index_in_overview(MAILSTREAM *);
ADDRESS	       *fetch_from(INDEXDATA_S *);
ADDRESS	       *fetch_sender(INDEXDATA_S *);
char	       *fetch_newsgroups(INDEXDATA_S *);
char	       *fetch_subject(INDEXDATA_S *);
char	       *fetch_date(INDEXDATA_S *);
long		fetch_size(INDEXDATA_S *);
BODY	       *fetch_body(INDEXDATA_S *);
char           *fetch_firsttext(INDEXDATA_S *idata, int);
char           *fetch_header(INDEXDATA_S *idata, char *hdrname);
void		subj_str(INDEXDATA_S *, char *, size_t, SubjKW, int, ICE_S *);
void		key_str(INDEXDATA_S *, SubjKW, ICE_S *);
void		header_str(INDEXDATA_S *, HEADER_TOK_S *, ICE_S *);
void		prio_str(INDEXDATA_S *, IndexColType, ICE_S *);
void		from_str(IndexColType, INDEXDATA_S *, char *, size_t, ICE_S *);
int             day_of_week(struct date *);
int             day_of_year(struct date *);
unsigned long   ice_hash(ICE_S *);
char           *left_adjust(int);
char           *right_adjust(int);
char           *format_str(int, int);
char           *copy_format_str(int, int, char *, int);
void            set_print_format(IELEM_S *, int, int);
void            set_ielem_widths_in_field(IFIELD_S *);


#define BIGWIDTH 2047


/*----------------------------------------------------------------------
      Initialize the index_disp_format array in ps_global from this
      format string.

   Args: format -- the string containing the format tokens
	 answer -- put the answer here, free first if there was a previous
		    value here
 ----*/
void
init_index_format(char *format, INDEX_COL_S **answer)
{
    char *p;
    int i, w, monabb_width = 0, column = 0;

    /*
     * Record the fact that SCORE appears in some index format. This
     * is a heavy-handed approach. It will stick at 1 if any format ever
     * contains score during this session. This is ok since it will just
     * cause recalculation if wrong and these things rarely change much.
     */
    if(!ps_global->a_format_contains_score && format
       && strstr(format, "SCORE")){
	ps_global->a_format_contains_score = 1;
	/* recalculate need for scores */
	scores_are_used(SCOREUSE_INVALID);
    }

    set_need_format_setup(ps_global->mail_stream);
    /* if custom format is specified, try it, else go with default */
    if(!(format && *format && parse_index_format(format, answer))){
	static INDEX_COL_S answer_default[] = {
	    {iStatus, Fixed, 3},
	    {iMessNo, WeCalculate},
	    {iSDateTime24, WeCalculate},
	    {iFromTo, Percent, 33}, /* percent of rest */
	    {iSizeNarrow, WeCalculate},
	    {iSubjKey, Percent, 67},
	    {iNothing}
	};

	if(*answer)
	  free_index_format(answer);

	*answer = (INDEX_COL_S *)fs_get(sizeof(answer_default));
	memcpy(*answer, answer_default, sizeof(answer_default));
    }

    /*
     * Test to see how long the month abbreviations are.
     */
    for(i = 1; i <= 12; i++){
	p = month_abbrev_locale(i);
	monabb_width = MAX(utf8_width(p), monabb_width);
    }

    monabb_width = MIN(MAX(2, monabb_width), 5);

    /*
     * Fill in req_width's for WeCalculate items.
     */
    for(column = 0; (*answer)[column].ctype != iNothing; column++){

	/* don't use strftime if we're not trying to use the LC_TIME stuff */
	if(F_ON(F_DISABLE_INDEX_LOCALE_DATES, ps_global)){
	    switch((*answer)[column].ctype){
	      case iSDate:
		(*answer)[column].ctype = iS1Date;
		break;
	      case iSDateTime:
		(*answer)[column].ctype = iSDateTimeS1;
		break;
	      case iSDateTime24:
		(*answer)[column].ctype = iSDateTimeS124;
		break;
	      default:
		break;
	    }
	}

	if((*answer)[column].wtype == WeCalculate){
	    switch((*answer)[column].ctype){
	      case iPrio:
	      case iPrioBang:
	      case iAtt:
		(*answer)[column].req_width = 1;
		break;
	      case iYear2Digit:
	      case iDay:
	      case iMon:
	      case iDay2Digit:
	      case iMon2Digit:
	      case iArrow:
	      case iKeyInit:
		(*answer)[column].req_width = 2;
		break;
	      case iStatus:
	      case iMessNo:
	      case iInit:
		(*answer)[column].req_width = 3;
		break;
	      case iYear:
	      case iDayOrdinal:
	      case iSIStatus:
		(*answer)[column].req_width = 4;
		break;
	      case iTime24:
	      case iTimezone:
	      case iSizeNarrow:
	      case iKey:
		(*answer)[column].req_width = 5;
		break;
	      case iFStatus:
	      case iIStatus:
	      case iScore:
		(*answer)[column].req_width = 6;
		break;
	      case iTime12:
	      case iSTime:
	      case iKSize:
	      case iSize:
	      case iPrioAlpha:
		(*answer)[column].req_width = 7;
		break;
	      case iS1Date:
	      case iS2Date:
	      case iS3Date:
	      case iS4Date:
	      case iDateIsoS:
	      case iSizeComma:
		(*answer)[column].req_width = 8;
		break;
	      case iMonAbb:
		(*answer)[column].req_width = monabb_width;
		(*answer)[column].monabb_width = monabb_width;
		break;
	      case iDayOfWeekAbb:
	        {
		    w = 0;

		    /*
		     * Test to see how long it is.
		     */
		    for(i = 0; i < 7; i++){
			p = day_abbrev_locale(i);
			w = MAX(utf8_width(p), w);
		    }

		    (*answer)[column].req_width = MIN(MAX(2, w), 5);
		}
		break;
	      case iDate:
		(*answer)[column].req_width = monabb_width + 3;
		(*answer)[column].monabb_width = monabb_width;
		break;
	      case iMonLong:
	        {
		    w = 0;

		    /*
		     * Test to see how long it is.
		     */
		    for(i = 1; i <= 12; i++){
			p = month_name_locale(i);
			w = MAX(utf8_width(p), w);
		    }

		    (*answer)[column].req_width = MIN(MAX(3, w), 12);
		}
		break;
	      case iDayOfWeek:
	        {
		    w = 0;

		    for(i = 0; i < 7; i++){
			p = day_name_locale(i);
			w = MAX(utf8_width(p), w);
		    }

		    (*answer)[column].req_width = MIN(MAX(3, w), 12);
		}
		break;
	      case iSDate:
	      case iSDateTime:
	      case iSDateTime24:
	        {
		    /*
		     * Format a date to see how long it is.
		     * Make it as least as long as "Yesterday".
		     * We should really use the width of the longest
		     * of the translated yesterdays and friends but...
		     */
		    struct tm tm;
		    char ss[100];

		    memset(&tm, 0, sizeof(tm));
		    tm.tm_year = 106;
		    tm.tm_mon = 11;
		    tm.tm_mday = 31;
		    our_strftime(ss, sizeof(ss), "%x", &tm);
		    (*answer)[column].req_width = MIN(MAX(9, utf8_width(ss)), 20);
		}

		(*answer)[column].monabb_width = monabb_width;
		break;

	      case iDescripSize:
	      case iSDateIsoS:
	      case iSDateS1: case iSDateS2: case iSDateS3: case iSDateS4:
	      case iSDateTimeIsoS:
	      case iSDateTimeS1: case iSDateTimeS2: case iSDateTimeS3: case iSDateTimeS4:
	      case iSDateTimeIsoS24:
	      case iSDateTimeS124: case iSDateTimeS224: case iSDateTimeS324: case iSDateTimeS424:
	        /*
		 * These SDates are 8 wide but they need to be 9 for "Yesterday".
		 */
		(*answer)[column].req_width = 9;
		break;
	      case iDateIso:
	      case iSDateIso: case iSDateTimeIso: case iSDateTimeIso24:
		(*answer)[column].req_width = 10;
		break;
	      case iLDate:
		(*answer)[column].req_width = 12;
		(*answer)[column].monabb_width = monabb_width;
		break;
	      case iRDate:
		(*answer)[column].req_width = 16;
		break;
	      default:
		break;
	    }
	}
    }

    calc_extra_hdrs();
    if(get_extra_hdrs())
      (void) mail_parameters(NULL, SET_IMAPEXTRAHEADERS,
			     (void *) get_extra_hdrs());
}


void
reset_index_format(void)
{
    long rflags = ROLE_DO_OTHER;
    PAT_STATE     pstate;
    PAT_S        *pat;
    int           we_set_it = 0;

    if(ps_global->mail_stream && nonempty_patterns(rflags, &pstate)){
	for(pat = first_pattern(&pstate); pat; pat = next_pattern(&pstate)){
	    if(match_pattern(pat->patgrp, ps_global->mail_stream, NULL,
			     NULL, NULL, SE_NOSERVER|SE_NOPREFETCH))
	      break;
	}

	if(pat && pat->action && !pat->action->bogus
	   && pat->action->index_format){
	    we_set_it++;
	    init_index_format(pat->action->index_format,
			      &ps_global->index_disp_format);
	}
    }

    if(!we_set_it)
      init_index_format(ps_global->VAR_INDEX_FORMAT,
		        &ps_global->index_disp_format);
}


void
free_index_format(INDEX_COL_S **disp_format)
{
    INDEX_COL_S	 *cdesc = NULL;

    if(disp_format && *disp_format){
	for(cdesc = (*disp_format); cdesc->ctype != iNothing; cdesc++)
	  if(cdesc->hdrtok)
	    free_hdrtok(&cdesc->hdrtok);

	fs_give((void **) disp_format);
    }
}


HEADER_TOK_S *
new_hdrtok(char *hdrname)
{
    HEADER_TOK_S *hdrtok;

    hdrtok = (HEADER_TOK_S *) fs_get(sizeof(HEADER_TOK_S));
    memset(hdrtok, 0, sizeof(HEADER_TOK_S));
    hdrtok->hdrname = hdrname ? cpystr(hdrname) : NULL;
    hdrtok->fieldnum = 0;
    hdrtok->adjustment = Left;
    hdrtok->fieldsepcnt = 1;
    hdrtok->fieldseps = cpystr(" ");

    return(hdrtok);
}


void
free_hdrtok(HEADER_TOK_S **hdrtok)
{
    if(hdrtok && *hdrtok){
	if((*hdrtok)->hdrname)
	  fs_give((void **) &(*hdrtok)->hdrname);

	if((*hdrtok)->fieldseps)
	  fs_give((void **) &(*hdrtok)->fieldseps);

	fs_give((void **) hdrtok);
    }
}


/* popular ones first to make it slightly faster */
static INDEX_PARSE_T itokens[] = {
    {"STATUS",		iStatus,	FOR_INDEX},
    {"MSGNO",		iMessNo,	FOR_INDEX},
    {"DATE",		iDate,		FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"FROMORTO",	iFromTo,	FOR_INDEX},
    {"FROMORTONOTNEWS",	iFromToNotNews,	FOR_INDEX},
    {"SIZE",		iSize,		FOR_INDEX},
    {"SIZECOMMA",	iSizeComma,	FOR_INDEX},
    {"SIZENARROW",	iSizeNarrow,	FOR_INDEX},
    {"KSIZE",		iKSize,		FOR_INDEX},
    {"SUBJECT",		iSubject,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"FULLSTATUS",	iFStatus,	FOR_INDEX},
    {"IMAPSTATUS",	iIStatus,	FOR_INDEX},
    {"SHORTIMAPSTATUS",	iSIStatus,	FOR_INDEX},
    {"SUBJKEY",		iSubjKey,	FOR_INDEX},
    {"SUBJKEYINIT",	iSubjKeyInit,	FOR_INDEX},
    {"SUBJECTTEXT",	iSubjectText,	FOR_INDEX},
    {"SUBJKEYTEXT",	iSubjKeyText,	FOR_INDEX},
    {"SUBJKEYINITTEXT", iSubjKeyInitText, FOR_INDEX},
    {"OPENINGTEXT",	iOpeningText,	FOR_INDEX},
    {"OPENINGTEXTNQ",	iOpeningTextNQ,	FOR_INDEX},
    {"KEY",		iKey,		FOR_INDEX},
    {"KEYINIT",		iKeyInit,	FOR_INDEX},
    {"DESCRIPSIZE",	iDescripSize,	FOR_INDEX},
    {"ATT",		iAtt,		FOR_INDEX},
    {"SCORE",		iScore,		FOR_INDEX},
    {"PRIORITY",	iPrio,		FOR_INDEX},
    {"PRIORITYALPHA",	iPrioAlpha,	FOR_INDEX},
    {"PRIORITY!",	iPrioBang,	FOR_INDEX},
    {"LONGDATE",	iLDate,		FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SHORTDATE1",	iS1Date,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SHORTDATE2",	iS2Date,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SHORTDATE3",	iS3Date,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SHORTDATE4",	iS4Date,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"DATEISO",		iDateIso,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SHORTDATEISO",	iDateIsoS,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SMARTDATE",	iSDate,		FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SMARTTIME",	iSTime,		FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SMARTDATEISO",	iSDateIso,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SMARTDATESHORTISO",iSDateIsoS,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SMARTDATES1",	iSDateS1,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SMARTDATES2",	iSDateS2,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SMARTDATES3",	iSDateS3,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SMARTDATES4",	iSDateS4,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SMARTDATETIME",	iSDateTime,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SMARTDATETIMEISO",iSDateTimeIso,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SMARTDATETIMESHORTISO",iSDateTimeIsoS,FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SMARTDATETIMES1",	iSDateTimeS1,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SMARTDATETIMES2",	iSDateTimeS2,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SMARTDATETIMES3",	iSDateTimeS3,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SMARTDATETIMES4",	iSDateTimeS4,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SMARTDATETIME24",		iSDateTime24,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SMARTDATETIMEISO24",	iSDateTimeIso24,FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SMARTDATETIMESHORTISO24",iSDateTimeIsoS24,FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SMARTDATETIMES124",	iSDateTimeS124,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SMARTDATETIMES224",	iSDateTimeS224,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SMARTDATETIMES324",	iSDateTimeS324,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SMARTDATETIMES424",	iSDateTimeS424,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"TIME24",		iTime24,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"TIME12",		iTime12,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"TIMEZONE",	iTimezone,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"MONTHABBREV",	iMonAbb,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"DAYOFWEEKABBREV",	iDayOfWeekAbb,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"DAYOFWEEK",	iDayOfWeek,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"FROM",		iFrom,		FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"TO",		iTo,		FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"SENDER",		iSender,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"CC",		iCc,		FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"RECIPS",		iRecips,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"NEWS",		iNews,		FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"TOANDNEWS",	iToAndNews,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"NEWSANDTO",	iNewsAndTo,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"RECIPSANDNEWS",	iRecipsAndNews,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"NEWSANDRECIPS",	iNewsAndRecips,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"MSGID",		iMsgID,		FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"CURNEWS",		iCurNews,	FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"DAYDATE",		iRDate,		FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"PREFDATE",	iPrefDate,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"PREFTIME",	iPrefTime,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"PREFDATETIME",	iPrefDateTime,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"DAY",		iDay,		FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"DAYORDINAL",	iDayOrdinal,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"DAY2DIGIT",	iDay2Digit,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"MONTHLONG",	iMonLong,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"MONTH",		iMon,		FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"MONTH2DIGIT",	iMon2Digit,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"YEAR",		iYear,		FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"YEAR2DIGIT",	iYear2Digit,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"ADDRESS",		iAddress,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"MAILBOX",		iMailbox,	FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"ROLENICK",       	iRoleNick,	FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"INIT",		iInit,		FOR_INDEX|FOR_REPLY_INTRO|FOR_TEMPLATE},
    {"CURDATE",		iCurDate,	FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"CURDATEISO",	iCurDateIso,	FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"CURDATEISOS",	iCurDateIsoS,	FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"CURTIME24",	iCurTime24,	FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"CURTIME12",	iCurTime12,	FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"CURDAY",		iCurDay,	FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"CURDAY2DIGIT",	iCurDay2Digit,	FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"CURDAYOFWEEK",	iCurDayOfWeek,	FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"CURDAYOFWEEKABBREV", iCurDayOfWeekAbb,
					FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"CURMONTH",	iCurMon,	FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"CURMONTH2DIGIT",	iCurMon2Digit,	FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"CURMONTHLONG",	iCurMonLong,	FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"CURMONTHABBREV",	iCurMonAbb,	FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"CURYEAR",		iCurYear,	FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"CURYEAR2DIGIT",	iCurYear2Digit,	FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"CURPREFDATE",	iCurPrefDate,	FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"CURPREFTIME",	iCurPrefTime,	FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"CURPREFDATETIME",	iCurPrefDateTime,
					FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"LASTMONTH",	iLstMon,	FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"LASTMONTH2DIGIT",	iLstMon2Digit,	FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"LASTMONTHLONG",	iLstMonLong,	FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"LASTMONTHABBREV",	iLstMonAbb,	FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"LASTMONTHYEAR",	iLstMonYear,	FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"LASTMONTHYEAR2DIGIT", iLstMonYear2Digit,
					FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"LASTYEAR",	iLstYear,	FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"LASTYEAR2DIGIT",	iLstYear2Digit,	FOR_REPLY_INTRO|FOR_TEMPLATE|FOR_FILT},
    {"HEADER",		iHeader,	FOR_INDEX},
    {"TEXT",		iText,		FOR_INDEX},
    {"ARROW",		iArrow,		FOR_INDEX},
    {"NEWLINE",		iNewLine,	FOR_REPLY_INTRO},
    {"CURSORPOS",	iCursorPos,	FOR_TEMPLATE},
    {NULL,		iNothing,	FOR_NOTHING}
};

INDEX_PARSE_T *
itoken(int i)
{
    return((i < sizeof(itokens) && itokens[i].name) ? &itokens[i] : NULL);
}


/*
 * Args  txt -- The token being checked begins at the beginning
 *              of txt. The end of the token is delimited by a null, or
 *              white space, or an underscore if DELIM_USCORE is set,
 *              or a left paren if DELIM_PAREN is set.
 *     flags -- Flags contains the what_for value, and DELIM_ values.
 *
 * Returns  A ptr to an INDEX_PARSE_T from itokens above, else NULL.
 */
INDEX_PARSE_T *
itoktype(char *txt, int flags)
{
    INDEX_PARSE_T *pt;
    char           token[100 + 1];
    char          *v, *w;

    /*
     * Separate a copy of the possible token out of txt.
     */
    v = txt;
    w = token;
    while(w < token+sizeof(token)-1 &&
	  *v &&
	  !(!(*v & 0x80) && isspace((unsigned char)*v)) &&
	  !(flags & DELIM_USCORE && *v == '_') &&
	  !(flags & DELIM_PAREN && *v == '(') &&
	  !(flags & DELIM_COLON && *v == ':'))
      *w++ = *v++;
    
    *w = '\0';

    for(pt = itokens; pt->name; pt++)
      if(pt->what_for & flags && !strucmp(pt->name, token))
        return(pt);
    
    return(NULL);
}


int
parse_index_format(char *format_str, INDEX_COL_S **answer)
{
    int            i, column = 0;
    char          *p, *q;
    INDEX_PARSE_T *pt;
    INDEX_COL_S    cdesc[200]; /* plenty of temp storage for answer */

    memset((void *)cdesc, 0, sizeof(cdesc));

    p = format_str;
    while(p && *p && column < 200-1){
	/* skip leading white space for next word */
	p = skip_white_space(p);
	pt = itoktype(p, FOR_INDEX | DELIM_PAREN | DELIM_COLON);
	
	/* ignore unrecognized word */
	if(!pt){
	    for(q = p; *p && !isspace((unsigned char)*p); p++)
	      ;

	    if(*p)
	      *p++ = '\0';

	    dprint((1,
		   "parse_index_format: unrecognized token: %s\n",
		   q ? q : "?"));
	    q_status_message1(SM_ORDER | SM_DING, 0, 3,
			      _("Unrecognized word in index-format: %s"), q);
	    continue;
	}

	cdesc[column].ctype = pt->ctype;

	if(pt->ctype == iHeader || pt->ctype == iText){
	    /*
	     * iHeader field has special syntax.
	     *
	     * HEADER:hdrname(width,fieldnum,field_separators,L_or_R)
	     *
	     * where width is the regular width or percentage width or
	     * left out for default width, fieldnum defaults to 0 for
	     * whole thing, 1 for first field, ...
	     * and field_separators is a list of characters which separate
	     * the fields. The whole parenthesized part is optional. If used
	     * the arguments can be dropped from the right, so
	     *
	     * HEADER:hdrname		or
	     * HEADER:hdrname(10)	or
	     * HEADER:hdrname(10%)	or
	     * HEADER:hdrname(10,2)	or
	     * HEADER:hdrname(,2)	or
	     * HEADER:hdrname(10,2, )	or
	     * HEADER:hdrname(10,2,\,:)	or
	     * HEADER:hdrname(10,2,\,:,R)
	     *
	     * iText field uses the hdrtok field for convenience. It has syntax
	     *
	     * TEXT:text		or
	     * TEXT:text(10)		or
	     * TEXT:text(10%)
	     *
	     * and the literal text goes into the index line. It is also special
	     * because there is no 1 column space after this field.
	     */

	    /* skip over name */
	    p += strlen(pt->name);

	    /* look for header name */
	    if(*p == ':'){
		char *w, hdrname[200];

		hdrname[0] = '\0';
		w = hdrname;
		p++;
		if(*p == '\"'){				/* quoted name */
		    p++;
		    while(w < hdrname + sizeof(hdrname)-1 && *p != '\"'){
			if(*p == '\\')
			  p++;

			*w++ = *p++;
		    }

		    *w = '\0';
		    if(*p == '\"')
		      p++;
		}
		else{
		    while(w < hdrname + sizeof(hdrname)-1 &&
			  !(!(*p & 0x80) && isspace((unsigned char)*p)) &&
			  *p != '(')
		      *w++ = *p++;

		    *w = '\0';
		}

		if(hdrname[0]){
		    cdesc[column].hdrtok = new_hdrtok(hdrname);
		}
		else{
		    if(pt->ctype == iHeader){
			dprint((1, "parse_index_token: HEADER should be followed by :hdrname\n"));
			q_status_message(SM_ORDER | SM_DING, 0, 3, "index token HEADER should be followed by :hdrname");
		    }
		    else{
			dprint((1, "parse_index_token: TEXT should be followed by :text\n"));
			q_status_message(SM_ORDER | SM_DING, 0, 3, "index token TEXT should be followed by :text");
		    }
		}
	    }
	    else{
		if(pt->ctype == iHeader){
		    dprint((1, "parse_index_token: HEADER should be followed by :hdrname, not %s\n", p));
		    q_status_message(SM_ORDER | SM_DING, 0, 3, "index token HEADER should be followed by :hdrname");
		}
		else{
		    dprint((1, "parse_index_token: TEXT should be followed by :text, not %s\n", p));
		    q_status_message(SM_ORDER | SM_DING, 0, 3, "index token TEXT should be followed by :text");
		}

		/* skip over rest of bogus config */
		while(!(!(*p & 0x80) && isspace((unsigned char)*p)) && *p != '(')
		  p++;
	    }
	}
	else{
	    /* skip over name and look for parens */
	    p += strlen(pt->name);
	}

	if(*p == '('){
	    p++;
	    q = p;
	    while(p && *p && isdigit((unsigned char) *p))
	      p++;
	    
	    if(pt->ctype == iHeader){
		/* first argument is width or width percentage, like for others */
		if(p && *p && (*p == ')' || *p == ',')){
		    if(p > q){
			cdesc[column].wtype = Fixed;
			cdesc[column].req_width = atoi(q);
		    }
		    else{
			cdesc[column].wtype = WeCalculate;
			cdesc[column].req_width = 0;
		    }
		}
		else if(p && *p && *p == '%' && p > q){
		    cdesc[column].wtype = Percent;
		    cdesc[column].req_width = atoi(q);
		    p++;
		}
		else{
		    cdesc[column].wtype = WeCalculate;
		    cdesc[column].req_width = 0;
		}

		/* optional 2nd argument is field number, 0 whole thing, 1, 2, ... */
		if(p && *p && *p == ','){
		    p++;
		    /* no space allowed between arguments */
		    if(*p && isdigit((unsigned char) *p)){
			q = p;
			while(*p && isdigit((unsigned char) *p))
			  p++;

			cdesc[column].hdrtok->fieldnum = atoi(q);

			/*
			 * Optional 3rd argument is field separators.
			 * Comma is \, and backslash is \\.
			 */
			if(*p == ','){
			    int j;

			    p++;
			    /* don't use default */
			    if(*p && *p != ')' && *p != ',' && cdesc[column].hdrtok->fieldseps)
			      cdesc[column].hdrtok->fieldseps[0] = '\0';

			    j = 0;
			    if(*p == '\"' && strchr(p+1, '\"')){
				p++;
				while(*p && *p != ')' && *p != '\"' && *p != ','){
				    if(cdesc[column].hdrtok->fieldseps)
				      fs_resize((void **) &cdesc[column].hdrtok->fieldseps, j+2);

				    if(*p == '\\' && *(p+1))
				      p++;

				    if(cdesc[column].hdrtok->fieldseps){
					cdesc[column].hdrtok->fieldseps[j++] = *p++;
					cdesc[column].hdrtok->fieldseps[j] = '\0';
					cdesc[column].hdrtok->fieldsepcnt = j;
				    }
				}

				if(*p == '\"')
				  p++;
			    }
			    else{
				while(*p && *p != ')' && *p != ','){
				    if(cdesc[column].hdrtok->fieldseps)
				      fs_resize((void **) &cdesc[column].hdrtok->fieldseps, j+2);
				    if(*p == '\\' && *(p+1))
				      p++;

				    if(cdesc[column].hdrtok->fieldseps){
					cdesc[column].hdrtok->fieldseps[j++] = *p++;
					cdesc[column].hdrtok->fieldseps[j] = '\0';
					cdesc[column].hdrtok->fieldsepcnt = j;
				    }
				}
			    }

			    /* optional 4th argument, left or right adjust */
			    if(*p == ','){
				p++;
				if(*p == 'L' || *p == 'l')
				  cdesc[column].hdrtok->adjustment = Left;
				else if(*p == 'R' || *p == 'r')
				  cdesc[column].hdrtok->adjustment = Right;
				else{
				    dprint((1, "parse_index_token: HEADER 4th argument should be L or R, not\n", *p ? p : "<null>"));
				    q_status_message(SM_ORDER | SM_DING, 0, 3, "HEADER 4th argument should be L or R");
				}
			    }
			}
		    }
		    else{
			dprint((1, "parse_index_token: HEADER 2nd argument should be field number, not\n", *p ? p : "<null>"));
			q_status_message(SM_ORDER | SM_DING, 0, 3, "HEADER 2nd argument should be field number, a non-negative digit");
		    }
		}
	    }
	    else{
		if(p && *p && *p == ')' && p > q){
		    cdesc[column].wtype = Fixed;
		    cdesc[column].req_width = atoi(q);
		}
		else if(p && *p && *p == '%' && p > q){
		    cdesc[column].wtype = Percent;
		    cdesc[column].req_width = atoi(q);
		}
		else{
		    cdesc[column].wtype = WeCalculate;
		    cdesc[column].req_width = 0;
		}
	    }
	}
	else{
	    /* if they left out width for iText we can figure it out */
	    if(pt->ctype == iText && cdesc[column].hdrtok && cdesc[column].hdrtok->hdrname){
		cdesc[column].wtype = Fixed;
		cdesc[column].req_width = utf8_width(cdesc[column].hdrtok->hdrname);
	    }
	    else{
		cdesc[column].wtype     = WeCalculate;
		cdesc[column].req_width = 0;
	    }
	}

	column++;
	/* skip text at end of word */
	while(p && *p && !isspace((unsigned char)*p))
	  p++;
    }

    /* if, after all that, we didn't find anything recognizable, bitch */
    if(!column){
	dprint((1, "Completely unrecognizable index-format\n"));
	q_status_message(SM_ORDER | SM_DING, 0, 3,
		 _("Configured \"index-format\" unrecognizable. Using default."));
	return(0);
    }

    /* Finish with Nothing column */
    cdesc[column].ctype = iNothing;

    /* free up old answer */
    if(*answer)
      free_index_format(answer);

    /* allocate space for new answer */
    *answer = (INDEX_COL_S *)fs_get((column+1)*sizeof(INDEX_COL_S));
    memset((void *)(*answer), 0, (column+1)*sizeof(INDEX_COL_S));
    /* copy answer to real place */
    for(i = 0; i <= column; i++)
      (*answer)[i] = cdesc[i];

    return(1);
}


/*
 * These types are basically fixed in width.
 * The order is slightly significant. The ones towards the front of the
 * list get space allocated sooner than the ones at the end of the list.
 */
static IndexColType fixed_ctypes[] = {
    iMessNo, iStatus, iFStatus, iIStatus, iSIStatus,
    iDate, iSDate, iSDateTime, iSDateTime24,
    iSTime, iLDate,
    iS1Date, iS2Date, iS3Date, iS4Date, iDateIso, iDateIsoS,
    iSDateIso, iSDateIsoS,
    iSDateS1, iSDateS2, iSDateS3, iSDateS4,
    iSDateTimeIso, iSDateTimeIsoS,
    iSDateTimeS1, iSDateTimeS2, iSDateTimeS3, iSDateTimeS4,
    iSDateTimeIso24, iSDateTimeIsoS24,
    iSDateTimeS124, iSDateTimeS224, iSDateTimeS324, iSDateTimeS424,
    iSize, iSizeComma, iSizeNarrow, iKSize, iDescripSize,
    iPrio, iPrioBang, iPrioAlpha, iInit,
    iAtt, iTime24, iTime12, iTimezone, iMonAbb, iYear, iYear2Digit,
    iDay2Digit, iMon2Digit, iDayOfWeekAbb, iScore, iMonLong, iDayOfWeek
};


int
ctype_is_fixed_length(IndexColType ctype)
{
    int j;

    for(j = 0; ; j++){
	if(j >= sizeof(fixed_ctypes)/sizeof(*fixed_ctypes))
	  break;
    
	if(ctype == fixed_ctypes[j])
	  return 1;
    }

    return 0;
}
    

/*----------------------------------------------------------------------
      Setup the widths of the various columns in the index display
 ----*/
void
setup_index_header_widths(MAILSTREAM *stream)
{
    int		 colspace;	/* for reserving space between columns */
    int		 j, some_to_calculate;
    int		 space_left, screen_width, fix;
    int		 keep_going, tot_pct, was_sl;
    long         max_msgno;
    WidthType	 wtype;
    INDEX_COL_S *cdesc;

    max_msgno = mn_get_total(ps_global->msgmap);

    dprint((8, "=== setup_index_header_widths() ===\n"));

    clear_icache_flags(stream);
    screen_width = ps_global->ttyo->screen_cols;
    space_left	 = screen_width;
    some_to_calculate = 0;
    colspace	 = -1;

    /*
     * Calculate how many fields there are so we know how many spaces
     * between columns to reserve.  Fill in Fixed widths now.  Reserve
     * special case WeCalculate with non-zero req_widths before doing
     * Percent cases below.
     */
    for(cdesc = ps_global->index_disp_format;
	cdesc->ctype != iNothing;
	cdesc++){

	if(cdesc->wtype == Fixed){
	  cdesc->width = cdesc->req_width;
	  if(cdesc->width > 0)
	    colspace++;
	}
	else if(cdesc->wtype == Percent){
	    cdesc->width = 0; /* calculated later */
	    colspace++;
	}
	else{ /* WeCalculate */
	    cdesc->width = cdesc->req_width; /* reserve this for now */
	    some_to_calculate++;
	    colspace++;
	}

	/* no space after iText */
	if(cdesc->ctype == iText)
	  colspace--;

	space_left -= cdesc->width;
    }

    colspace = MAX(colspace, 0);

    space_left -= colspace; /* space between columns */

    ps_global->display_keywords_in_subject = 0;
    ps_global->display_keywordinits_in_subject = 0;

    /*
     * Set the actual lengths for the fixed width fields and set up
     * the left or right adjustment for everything.
     * There should be a case setting actual_length for all of the types
     * in fixed_ctypes.
     */
    for(cdesc = ps_global->index_disp_format;
	cdesc->ctype != iNothing;
	cdesc++){

	wtype = cdesc->wtype;

	if(cdesc->ctype == iSubjKey || cdesc->ctype == iSubjKeyText)
	  ps_global->display_keywords_in_subject = 1;
	else if(cdesc->ctype == iSubjKeyInit || cdesc->ctype == iSubjKeyInitText)
	  ps_global->display_keywordinits_in_subject = 1;

	if(wtype == WeCalculate || wtype == Percent || cdesc->width != 0){

	    switch(cdesc->ctype){
	      case iSDate: case iSDateIso: case iSDateIsoS:
	      case iSDateS1: case iSDateS2: case iSDateS3: case iSDateS4:
	      case iSDateTime: case iSDateTimeIso: case iSDateTimeIsoS:
	      case iSDateTimeS1: case iSDateTimeS2: case iSDateTimeS3: case iSDateTimeS4:
	      case iSDateTime24: case iSDateTimeIso24: case iSDateTimeIsoS24:
	      case iSDateTimeS124: case iSDateTimeS224: case iSDateTimeS324: case iSDateTimeS424:
	      case iSTime:
		set_format_includes_smartdate(stream);
	        break;

	      default:
	        break;
	    }

	    if(ctype_is_fixed_length(cdesc->ctype)){
		switch(cdesc->ctype){
		  case iPrio:
		  case iPrioBang:
		  case iAtt:
		    cdesc->actual_length = 1;
		    cdesc->adjustment = Left;
		    break;

		  case iYear2Digit:
		  case iDay2Digit:
		  case iMon2Digit:
		    cdesc->actual_length = 2;
		    cdesc->adjustment = Left;
		    break;

		  case iArrow:
		    cdesc->actual_length = 2;
		    cdesc->adjustment = Right;
		    break;

		  case iStatus:
		  case iInit:
		    cdesc->actual_length = 3;
		    cdesc->adjustment = Left;
		    break;

		  case iMessNo:
		    set_format_includes_msgno(stream);
		    if(max_msgno < 1000)
		      cdesc->actual_length = 3;
		    else if(max_msgno < 10000)
		      cdesc->actual_length = 4;
		    else if(max_msgno < 100000)
		      cdesc->actual_length = 5;
		    else
		      cdesc->actual_length = 6;

		    cdesc->adjustment = Right;
		    break;

		  case iYear:
		  case iSIStatus:
		    cdesc->actual_length = 4;
		    cdesc->adjustment = Left;
		    break;

		  case iTime24:
		  case iTimezone:
		    cdesc->actual_length = 5;
		    cdesc->adjustment = Left;
		    break;

		  case iSizeNarrow:
		    cdesc->actual_length = 5;
		    cdesc->adjustment = Right;
		    break;

		  case iFStatus:
		  case iIStatus:
		    cdesc->actual_length = 6;
		    cdesc->adjustment = Left;
		    break;

		  case iScore:
		    cdesc->actual_length = 6;
		    cdesc->adjustment = Right;
		    break;

		  case iTime12:
		  case iSize:
		  case iKSize:
		    cdesc->actual_length = 7;
		    cdesc->adjustment = Right;
		    break;

		  case iSTime:
		    cdesc->actual_length = 7;
		    cdesc->adjustment = Left;
		    break;

		  case iPrioAlpha:
		    cdesc->actual_length = 7;
		    cdesc->adjustment = Left;
		    break;


		  case iS1Date:
		  case iS2Date:
		  case iS3Date:
		  case iS4Date:
		  case iDateIsoS:
		    cdesc->actual_length = 8;
		    cdesc->adjustment = Left;
		    break;

		  case iSizeComma:
		    cdesc->actual_length = 8;
		    cdesc->adjustment = Right;
		    break;

		  case iSDate:
		  case iSDateTime:
		  case iSDateTime24:
		  case iMonAbb:
		  case iDayOfWeekAbb:
		  case iDayOfWeek:
		  case iDate:
		  case iMonLong:
		    cdesc->actual_length = cdesc->req_width;
		    cdesc->adjustment = Left;
		    break;

		  case iSDateIsoS:
		  case iSDateS1: case iSDateS2: case iSDateS3: case iSDateS4:
		  case iSDateTimeIsoS:
		  case iSDateTimeIsoS24:
		  case iSDateTimeS1: case iSDateTimeS2: case iSDateTimeS3: case iSDateTimeS4:
		  case iSDateTimeS124: case iSDateTimeS224: case iSDateTimeS324: case iSDateTimeS424:
		  case iSDateIso: case iSDateTimeIso: case iSDateTimeIso24:
		    if(cdesc->ctype == iSDateIso
		       || cdesc->ctype == iSDateTimeIso
		       || cdesc->ctype == iSDateTimeIso24)
		      cdesc->actual_length = 10;
		    else
		      cdesc->actual_length = 9;

		    cdesc->adjustment = Left;
		    break;

		  case iDescripSize:
		    cdesc->actual_length = 9;
		    cdesc->adjustment = Right;
		    break;

		  case iDateIso:
		    cdesc->actual_length = 10;
		    cdesc->adjustment = Left;
		    break;

		  case iLDate:
		    cdesc->actual_length = 12;
		    cdesc->adjustment = Left;
		    break;
		  
		  default:
		    panic("Unhandled fixed case in setup_index_header");
		    break;
		}
	    }
	    else if(cdesc->ctype == iHeader)
	      cdesc->adjustment = cdesc->hdrtok ? cdesc->hdrtok->adjustment : Left;
	    else
	      cdesc->adjustment = Left;
	}
    }

    if(ps_global->display_keywords_in_subject)
      ps_global->display_keywordinits_in_subject = 0;

    /* if have reserved unneeded space for size, give it back */
    for(cdesc = ps_global->index_disp_format;
	cdesc->ctype != iNothing;
	cdesc++)
      if(cdesc->ctype == iSize || cdesc->ctype == iKSize ||
         cdesc->ctype == iSizeNarrow ||
	 cdesc->ctype == iSizeComma || cdesc->ctype == iDescripSize){
	  if(cdesc->actual_length == 0){
	      if((fix=cdesc->width) > 0){ /* had this reserved */
		  cdesc->width = 0;
		  space_left += fix;
	      }

	      space_left++;  /* +1 for space between columns */
	  }
      }

    /*
     * Calculate the field widths that are basically fixed in width.
     * Do them in this order in case we don't have enough space to go around.
     * The set of fixed_ctypes here is the same as the set where we
     * set the actual_lengths above.
     */
    for(j = 0; space_left > 0 && some_to_calculate; j++){

      if(j >= sizeof(fixed_ctypes)/sizeof(*fixed_ctypes))
	break;

      for(cdesc = ps_global->index_disp_format;
	  cdesc->ctype != iNothing && space_left > 0 && some_to_calculate;
	  cdesc++)
	if(cdesc->ctype == fixed_ctypes[j] && cdesc->wtype == WeCalculate){
	    some_to_calculate--;
	    fix = MIN(cdesc->actual_length - cdesc->width, space_left);
	    cdesc->width += fix;
	    space_left -= fix;
	}
    }

    /*
     * Fill in widths for Percent cases.  If there are no more to calculate,
     * use the percentages as relative numbers and use the rest of the space,
     * else treat them as absolute percentages of the original avail screen.
     */
    if(space_left > 0){
      if(some_to_calculate){
	int tot_requested = 0;

	/*
	 * Requests are treated as percent of screen width. See if they
	 * will all fit. If not, trim them back proportionately.
	 */
	for(cdesc = ps_global->index_disp_format;
	    cdesc->ctype != iNothing;
	    cdesc++){
	  if(cdesc->wtype == Percent){
	      /* The 2, 200, and +100 are because we're rounding */
	      fix = ((2*cdesc->req_width *
		      (screen_width-colspace))+100) / 200;
	      tot_requested += fix;
	  }
	}

	if(tot_requested > space_left){
	  int multiplier = (100 * space_left) / tot_requested;

	  for(cdesc = ps_global->index_disp_format;
	      cdesc->ctype != iNothing && space_left > 0;
	      cdesc++){
	    if(cdesc->wtype == Percent){
	        /* The 2, 200, and +100 are because we're rounding */
	        fix = ((2*cdesc->req_width *
		        (screen_width-colspace))+100) / 200;
		fix = (2 * fix * multiplier + 100) / 200;
	        fix = MIN(fix, space_left);
	        cdesc->width += fix;
	        space_left -= fix;
	    }
	  }
	}
	else{
	  for(cdesc = ps_global->index_disp_format;
	      cdesc->ctype != iNothing && space_left > 0;
	      cdesc++){
	    if(cdesc->wtype == Percent){
	        /* The 2, 200, and +100 are because we're rounding */
	        fix = ((2*cdesc->req_width *
		        (screen_width-colspace))+100) / 200;
	        fix = MIN(fix, space_left);
	        cdesc->width += fix;
	        space_left -= fix;
	    }
	  }
	}
      }
      else{
	tot_pct = 0;
	was_sl = space_left;
	/* add up total percentages requested */
	for(cdesc = ps_global->index_disp_format;
	    cdesc->ctype != iNothing;
	    cdesc++)
	  if(cdesc->wtype == Percent)
	    tot_pct += cdesc->req_width;

	/* give relative weight to requests */
	for(cdesc = ps_global->index_disp_format;
	    cdesc->ctype != iNothing && space_left > 0 && tot_pct > 0;
	    cdesc++){
	    if(cdesc->wtype == Percent){
		fix = ((2*cdesc->req_width*was_sl)+tot_pct) / (2*tot_pct);
	        fix = MIN(fix, space_left);
	        cdesc->width += fix;
	        space_left -= fix;
	    }
	}
      }
    }

    /* split up rest, give twice as much to Subject */
    keep_going = 1;
    while(space_left > 0 && keep_going){
      keep_going = 0;
      for(cdesc = ps_global->index_disp_format;
	  cdesc->ctype != iNothing && space_left > 0;
	  cdesc++){
	if(cdesc->wtype == WeCalculate && !ctype_is_fixed_length(cdesc->ctype)){
	  keep_going++;
	  cdesc->width++;
	  space_left--;
	  if(space_left > 0 && (cdesc->ctype == iSubject
				|| cdesc->ctype == iSubjectText
				|| cdesc->ctype == iSubjKey
				|| cdesc->ctype == iSubjKeyText
				|| cdesc->ctype == iSubjKeyInit
				|| cdesc->ctype == iSubjKeyInitText)){
	      cdesc->width++;
	      space_left--;
	  }
	}
      }
    }

    /* if still more, pad out percent's */
    keep_going = 1;
    while(space_left > 0 && keep_going){
      keep_going = 0;
      for(cdesc = ps_global->index_disp_format;
	  cdesc->ctype != iNothing && space_left > 0;
	  cdesc++){
	if(cdesc->wtype == Percent && !ctype_is_fixed_length(cdesc->ctype)){
	  keep_going++;
	  cdesc->width++;
	  space_left--;
	}
      }
    }

    /* if user made Fixed fields too big, give back space */
    keep_going = 1;
    while(space_left < 0 && keep_going){
      keep_going = 0;
      for(cdesc = ps_global->index_disp_format;
	  cdesc->ctype != iNothing && space_left < 0;
	  cdesc++){
	if(cdesc->wtype == Fixed && cdesc->width > 0){
	  keep_going++;
	  cdesc->width--;
	  space_left++;
	}
      }
    }

    if(pith_opt_save_index_state)
      (*pith_opt_save_index_state)(FALSE);
}


void
setup_thread_header_widths(MAILSTREAM *stream)
{
    clear_icache_flags(stream);
    if(pith_opt_save_index_state)
      (*pith_opt_save_index_state)(TRUE);
}


/*
 * load_overview - c-client call back to gather overview data
 *
 * Note: if we never get called, UID represents a hole
 *       if we're passed a zero UID, totally bogus overview data
 *       if we're passed a zero obuf, mostly bogus overview data
 */
void
load_overview(MAILSTREAM *stream, imapuid_t uid, OVERVIEW *obuf, long unsigned int rawno)
{
    if(obuf && rawno >= 1L && stream && rawno <= stream->nmsgs){
	INDEXDATA_S  idata;
	ICE_S       *ice;

	memset(&idata, 0, sizeof(INDEXDATA_S));
	idata.no_fetch = 1;

	/*
	 * Only really load the thing if we've got an NNTP stream
	 * otherwise we're just using mail_fetch_overview to load the
	 * IMAP envelope cache with the specific set of messages
	 * in a single RTT.
	 */
	idata.stream  = stream;
	idata.rawno   = rawno;
	idata.msgno   = mn_raw2m(sp_msgmap(stream), idata.rawno);
	idata.size    = obuf->optional.octets;
	idata.from    = obuf->from;
	idata.date    = obuf->date;
	idata.subject = obuf->subject;

	ice = (*format_index_line)(&idata);
	if(idata.bogus && ice){
	    if(THRD_INDX()){
		if(ice->tice)
		  clear_ice(&ice->tice);
	    }
	    else
	      clear_ice(&ice);
	}
	else if(F_OFF(F_QUELL_NEWS_ENV_CB, ps_global)
		&& (!THRD_INDX() || (ice && ice->tice))
		&& !msgline_hidden(stream, sp_msgmap(stream), idata.msgno, 0)
		&& pith_opt_paint_index_hline){
	    (*pith_opt_paint_index_hline)(stream, idata.msgno, ice);
	}
    }
}


ICE_S *
build_header_work(struct pine *state, MAILSTREAM *stream, MSGNO_S *msgmap,
		  long int msgno, long int top_msgno, int msgcount, int *fetched)
{
    ICE_S        *ice, *ic;
    MESSAGECACHE *mc;
    long          n, i, cnt, rawno, visible, limit = -1L;

    rawno = mn_m2raw(msgmap, msgno);

    /* cache hit? */
    if(THRD_INDX()){
	ice = fetch_ice(stream, rawno);
	if(!ice)
	  return(NULL);

	if(ice->tice && ice->tice->ifield
	   && ice->tice->color_lookup_done && ice->tice->widths_done){
#ifdef DEBUG
	    char buf[MAX_SCREEN_COLS+1];
	    simple_index_line(buf, sizeof(buf), ice->tice, msgno);
#endif
	    dprint((9, "Hitt: Returning %p -> <%s (%d)\n",
		       ice->tice,
		       buf[0] ? buf : "?",
		       buf[0] ? strlen(buf) : 0));
	    return(ice);
	}
    }
    else{
	ice = fetch_ice(stream, rawno);
	if(!ice)
	  return(NULL);

	if(ice->ifield && ice->color_lookup_done && ice->widths_done){
#ifdef DEBUG
	    char buf[MAX_SCREEN_COLS+1];
	    simple_index_line(buf, sizeof(buf), ice, msgno);
#endif
	    dprint((9, "Hit: Returning %p -> <%s (%d)\n",
		       ice,
		       buf[0] ? buf : "?",
		       buf[0] ? strlen(buf) : 0));
	    return(ice);
	}
    }

    /*
     * If we are in THRD_INDX() and the width changed we don't currently
     * have a method of fixing just the widths and print_format strings.
     * Instead, we clear the index cache entry and start over.
     */
    if(THRD_INDX() && ice && ice->tice && ice->tice->ifield
       && !ice->tice->widths_done){
	clear_ice(&ice->tice);
    }

    /*
     * Fetch everything we need to start filling in the index line
     * explicitly via mail_fetch_overview.  On an nntp stream
     * this has the effect of building the index lines in the
     * load_overview callback.  Under IMAP we're either getting
     * the envelope data via the imap_envelope callback or
     * preloading the cache.  Either way, we're getting exactly
     * what we want rather than relying on linear lookahead sort
     * of prefetch...
     */
    if(!(fetched && *fetched) && index_in_overview(stream)
       && ((THRD_INDX() && !(ice->tice && ice->tice->ifield))
           || (!THRD_INDX() && !ice->ifield))){
	char	     *seq;
	int	      count;
	MESSAGECACHE *mc;
	PINETHRD_S   *thrd;

	if(fetched)
	  (*fetched)++;

	/* clear sequence bits */
	for(n = 1L; n <= stream->nmsgs; n++)
	  if((mc = mail_elt(stream, n)) != NULL)
	    mc->sequence = 0;

	/*
	 * Light interesting bits
	 * NOTE: not set above because m2raw's cheaper
	 * than raw2m for every message
	 */

	/*
	 * Unfortunately, it is expensive to calculate visible pages
	 * in thread index if we are zoomed, so we don't try.
	 */
	if(THRD_INDX() && any_lflagged(msgmap, MN_HIDE))
	  visible = msgmap->visible_threads;
	else if(THREADING() && sp_viewing_a_thread(stream)){
	    /*
	     * We know that all visible messages in the thread are marked
	     * with MN_CHID2.
	     */
	    for(visible = 0L, n = top_msgno;
		visible < msgcount && n <= mn_get_total(msgmap);
		n++){

		if(!get_lflag(stream, msgmap, n, MN_CHID2))
		  break;
		
		if(!msgline_hidden(stream, msgmap, n, 0))
		  visible++;
	    }
	    
	}
	else
	  visible = mn_get_total(msgmap)
		      - any_lflagged(msgmap, MN_HIDE|MN_CHID);

	limit = MIN(visible, msgcount);

	if(THRD_INDX()){
	    count = i = 0;

	    /*
	     * First add the msgno we're asking for in case it
	     * isn't visible.
	     */
	    thrd = fetch_thread(stream,	mn_m2raw(msgmap, msgno));
	    if(msgno <= mn_get_total(msgmap)
	       && (!(ic=fetch_ice(stream,thrd->rawno)) || !(ic=ic->tice) || !ic->ifield)){
		count += mark_msgs_in_thread(stream, thrd, msgmap);
	    }

	    thrd = fetch_thread(stream,	mn_m2raw(msgmap, top_msgno));

	    /*
	     * Loop through visible threads, marking them for fetching.
	     * Stop at end of screen or sooner if we run out of visible
	     * threads.
	     */
	    while(thrd){
		n = mn_raw2m(msgmap, thrd->rawno);
		if(n >= msgno
		   && n <= mn_get_total(msgmap)
		   && (!(ic=fetch_ice(stream,thrd->rawno)) || !(ic=ic->tice) || !ic->ifield)){
		    count += mark_msgs_in_thread(stream, thrd, msgmap);
		}

		if(++i >= limit)
		  break;

		/* find next thread which is visible */
		do{
		    if(mn_get_revsort(msgmap) && thrd->prevthd)
		      thrd = fetch_thread(stream, thrd->prevthd);
		    else if(!mn_get_revsort(msgmap) && thrd->nextthd)
		      thrd = fetch_thread(stream, thrd->nextthd);
		    else
		      thrd = NULL;
		} while(thrd
			&& msgline_hidden(stream, msgmap,
					  mn_raw2m(msgmap, thrd->rawno), 0));
	    }
	}
	else{
	    count = i = 0;

	    /*
	     * First add the msgno we're asking for in case it
	     * isn't visible.
	     */
	    if(msgno > 0L && msgno <= mn_get_total(msgmap)
	       && (!(ic=fetch_ice(stream, (rawno=mn_m2raw(msgmap,msgno)))) || !ic->ifield)){
		if((thrd = fetch_thread(stream, rawno)) != NULL){
		    /*
		     * If we're doing a MUTTLIKE display the index line
		     * may depend on the thread parent, and grandparent,
		     * and further back. So just fetch the whole thread
		     * in that case.
		     */
		    if(THREADING()
		       && ps_global->thread_disp_style == THREAD_MUTTLIKE
		       && thrd->top)
		      thrd = fetch_thread(stream, thrd->top);

		    count += mark_msgs_in_thread(stream, thrd, msgmap);
		}
		else if(rawno > 0L && rawno <= stream->nmsgs
			&& (mc = mail_elt(stream,rawno))
			&& !mc->private.msg.env){
		    mc->sequence = 1;
		    count++;
		}
	    }

	    n = top_msgno;
	    while(1){
		if(n >= msgno
		   && n <= mn_get_total(msgmap)
		   && (!(ic=fetch_ice(stream, (rawno=mn_m2raw(msgmap,n)))) || !ic->ifield)){
		    if((thrd = fetch_thread(stream, rawno)) != NULL){
			/*
			 * If we're doing a MUTTLIKE display the index line
			 * may depend on the thread parent, and grandparent,
			 * and further back. So just fetch the whole thread
			 * in that case.
			 */
			if(THREADING()
			   && ps_global->thread_disp_style == THREAD_MUTTLIKE
			   && thrd->top)
		          thrd = fetch_thread(stream, thrd->top);

			count += mark_msgs_in_thread(stream, thrd, msgmap);
		    }
		    else if(rawno > 0L && rawno <= stream->nmsgs
			    && (mc = mail_elt(stream,rawno))
			    && !mc->private.msg.env){
			mc->sequence = 1;
			count++;
		    }
		}

		if(++i >= limit)
		  break;

		/* find next n which is visible */
		while(++n <=  mn_get_total(msgmap)
		      && msgline_hidden(stream, msgmap, n, 0))
		  ;
	    }
	}

	if(count){
	    seq = build_sequence(stream, NULL, NULL);
	    if(seq){
		ps_global->dont_count_flagchanges = 1;
		mail_fetch_overview_sequence(stream, seq,
				    (stream->dtb && stream->dtb->name
				     && !strcmp(stream->dtb->name, "imap"))
				      ? NULL : load_overview);
		ps_global->dont_count_flagchanges = 0;
		fs_give((void **) &seq);
	    }
	}

	/*
	 * reassign ice from the cache as it may've been built
	 * within the overview callback or it may have become stale
	 * in the prior sequence bit setting loop ...
	 */
	rawno = mn_m2raw(msgmap, msgno);
	ice = fetch_ice(stream, rawno);
	if(!ice)
	  return(NULL);
    }

    if((THRD_INDX() && !(ice->tice && ice->tice->ifield))
       || (!THRD_INDX() && !ice->ifield)){
	INDEXDATA_S idata;

	/*
	 * With pre-fetching/callback-formatting done and no success,
	 * fall into formatting the requested line...
	 */
	memset(&idata, 0, sizeof(INDEXDATA_S));
	idata.stream   = stream;
	idata.msgno    = msgno;
	idata.rawno    = mn_m2raw(msgmap, msgno);
	if(stream && idata.rawno > 0L && idata.rawno <= stream->nmsgs
	   && (mc = mail_elt(stream, idata.rawno))){
	    idata.size = mc->rfc822_size;
	    index_data_env(&idata, pine_mail_fetchenvelope(stream,idata.rawno));
	}
	else
	  idata.bogus = 2;

	ice = (*format_index_line)(&idata);
	if(!ice)
	  return(NULL);
    }

    /*
     * If needed, reset the print_format strings so that they add up to
     * the right total width. The reset width functionality isn't implemented
     * for THRD_INDX() so we are just doing a complete rebuild in that
     * case. This is driven by the clear_ice() call in clear_index_cache_ent()
     * so it should never be the case that THRD_INDX() is true and only
     * widths_done needs to be fixed.
     */
    if((!THRD_INDX() && ice->ifield && !ice->widths_done)){
	ICE_S       *working_ice;
	IFIELD_S    *ifield;
	INDEX_COL_S *cdesc;

	if(need_format_setup(stream))
	  setup_header_widths(stream);

	if(THRD_INDX())
	  working_ice = ice ? ice->tice : NULL;
	else
	  working_ice = ice;

	if(working_ice){
	  /*
	   * First fix the ifield widths. The cdescs with nonzero widths
	   * should correspond to the ifields that are defined.
	   */
	  ifield = working_ice->ifield;
	  for(cdesc = ps_global->index_disp_format;
	      cdesc->ctype != iNothing && ifield; cdesc++){
	      if(cdesc->width){
		  if(cdesc->ctype != ifield->ctype){
		    dprint((1, "build_header_work(%ld): cdesc->ctype=%d != ifield->ctype=%d NOT SUPPOSED TO HAPPEN!\n", msgno, (int) cdesc->ctype, (int) ifield->ctype));
		    assert(0);
		  }

		  ifield->width = cdesc->width;
		  ifield = ifield->next;
	      }
	  }

	  /* fix the print_format strings and widths */
	  for(ifield = working_ice->ifield; ifield; ifield = ifield->next)
	    set_ielem_widths_in_field(ifield);

	  working_ice->widths_done = 1;
	}
    }

    if(THRD_INDX() && ice->tice)
      ice->tice->color_lookup_done = 1;

    /*
     * Look for a color for this line (and other lines in the current
     * view). This does a SEARCH for each role which has a color until
     * it finds a match. This will be satisfied by the c-client
     * cache created by the mail_fetch_overview above if it is a header
     * search.
     */
    if(!THRD_INDX() && !ice->color_lookup_done){
	COLOR_PAIR *linecolor;
	SEARCHSET  *ss, *s;
	ICE_S      *ic;
	PAT_STATE  *pstate = NULL;

	if(pico_usingcolor()){
	    if(limit < 0L){
		if(THREADING() && sp_viewing_a_thread(stream)){
		    for(visible = 0L, n = top_msgno;
			visible < msgcount && n <= mn_get_total(msgmap);
			n++){

			if(!get_lflag(stream, msgmap, n, MN_CHID2))
			  break;
			
			if(!msgline_hidden(stream, msgmap, n, 0))
			  visible++;
		    }
		    
		}
		else
		  visible = mn_get_total(msgmap)
			      - any_lflagged(msgmap, MN_HIDE|MN_CHID);

		limit = MIN(visible, msgcount);
	    }
	    /* clear sequence bits */
	    for(n = 1L; n <= stream->nmsgs; n++)
	      if((mc = mail_elt(stream, n)) != NULL)
	        mc->sequence = 0;

	    cnt = i = 0;
	    n = top_msgno;
	    while(1){
		if(n >= msgno
		   && n <= mn_get_total(msgmap)
		   && (!(ic=fetch_ice(stream,(rawno = mn_m2raw(msgmap, n)))) || !ic->color_lookup_done)){

		    if(rawno >= 1L && rawno <= stream->nmsgs
		       && (mc = mail_elt(stream, rawno))){
			mc->sequence = 1;
			cnt++;
		    }
		}

		if(++i >= limit)
		  break;

		/* find next n which is visible */
		while(++n <=  mn_get_total(msgmap)
		      && msgline_hidden(stream, msgmap, n, 0))
		  ;
	    }

	    /*
	     * Why is there a loop here? The first call to get_index_line_color
	     * will return a set of messages which match one of the roles.
	     * Then, we eliminate those messages from the search set and try
	     * again. This time we'd get past that role and into a different
	     * role. Because of that, we hang onto the state and don't reset
	     * to the first_pattern on the second and subsequent times
	     * through the loop, avoiding fruitless match_pattern calls in
	     * get_index_line_color.
	     * Before the first call, pstate should be set to NULL.
	     */
	    while(cnt > 0L){
		ss = build_searchset(stream);
		if(ss){
		    int colormatch;

		    linecolor = NULL;
		    colormatch = get_index_line_color(stream, ss, &pstate,
						      &linecolor);

		    /*
		     * Assign this color to all matched msgno's and
		     * turn off the sequence bit so we won't check
		     * for them again.
		     */
		    if(colormatch){
			for(s = ss; s; s = s->next){
			  for(n = s->first; n <= s->last; n++){
			    if(n >= 1L && n <= stream->nmsgs
			       && (mc = mail_elt(stream, n))
			       && mc->searched){
				cnt--;
				mc->sequence = 0;
				ic = fetch_ice(stream, n);
				if(ic){
				    ic->color_lookup_done = 1;
				    if(linecolor)
				      ic->linecolor = new_color_pair(linecolor->fg,
								     linecolor->bg);
				}
			    }
			  }
			}

			if(linecolor)
			  free_color_pair(&linecolor);
		    }
		    else{
			/* have to mark the rest of the lookups done */
			for(s = ss; s && cnt > 0; s = s->next){
			  for(n = s->first; n <= s->last && cnt > 0; n++){
			    if(n >= 1L && n <= stream->nmsgs
			       && (mc = mail_elt(stream, n))
			       && mc->sequence){
				cnt--;
				ic = fetch_ice(stream, n);
				if(ic)
				  ic->color_lookup_done = 1;
			    }
			  }
			}

			/* just making sure */
			cnt = 0L;
		    }

		    mail_free_searchset(&ss);
		}
		else
		  cnt = 0L;
	    }

	    ice = fetch_ice(stream, mn_m2raw(msgmap, msgno));
	}
	else
	  ice->color_lookup_done = 1;
    }

    return(ice);		/* Return formatted index data */
}


int
day_of_week(struct date *d)
{
    int m, y;

    m = d->month;
    y = d->year;
    if(m <= 2){
	m += 9;
	y--;
    }
    else
      m -= 3;	/* March is month 0 */
    
    return((d->day+2+((7+31*m)/12)+y+(y/4)+(y/400)-(y/100))%7);
}


static int daytab[2][13] = {
    {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
};

int
day_of_year(struct date *d)
{
    int i, leap, doy;

    if(d->year <= 0 || d->month < 1 || d->month > 12)
      return(-1);

    doy = d->day;
    leap = (d->year%4 == 0 && d->year%100 != 0) || d->year%400 == 0;
    for(i = 1; i < d->month; i++)
      doy += daytab[leap][i];
    
    return(doy);
}



/*----------------------------------------------------------------------
   Format a string summarizing the message header for index on screen

   Args: buffer -- buffer to place formatted line
	 idata -- snot it takes to format the line

  Result: returns pointer given buffer IF entry formatted
	  else NULL if there was a problem (but the buffer is
	  still suitable for display)
 ----*/
ICE_S *
format_index_index_line(INDEXDATA_S *idata)
{
    char          str[BIGWIDTH+1], to_us, status, *field,
		 *p, *newsgroups;
    int		  i, collapsed = 0, start, fromfield;
    long	  l, score;
    BODY	 *body = NULL;
    MESSAGECACHE *mc;
    ADDRESS      *addr, *toaddr, *ccaddr, *last_to;
    PINETHRD_S   *thrd = NULL;
    INDEX_COL_S	 *cdesc = NULL;
    ICE_S        *ice, **icep;
    IFIELD_S     *ifield;
    IELEM_S      *ielem;
    struct variable *vars = ps_global->vars;

    dprint((8, "=== format_index_line(msgno=%ld,rawno=%ld) ===\n",
	       idata ? idata->msgno : -1, idata ? idata->rawno : -1));


    ice = fetch_ice(idata->stream, idata->rawno);
    if(!ice)
      return(NULL);

    free_ifield(&ice->ifield);

    /*
     * Operate on a temporary copy of ice. The reason for this
     * is that we may end up causing a pine_mail_fetchenvelope() call
     * (e.g., in to_us_symbol_for_thread()) that causes an mm_flags()
     * and mm_flags may do a clear_ice(), freeing the ice we are working
     * on out from under us. We try to fetch everything we need in
     * build_header_work() but c-client will short-circuit our request
     * if we already got the raw header for some reason. One possible
     * reason is a categorizer command in a filter. In that case
     * we still need a fetch fast to get the rest of the envelope data.
     */
    ice = copy_ice(ice);

    /* is this a collapsed thread index line? */
    if(!idata->bogus && THREADING()){
	thrd = fetch_thread(idata->stream, idata->rawno);
	collapsed = thrd && thrd->next
		    && get_lflag(idata->stream, NULL,
				 idata->rawno, MN_COLL);
    }

    /* calculate contents of the required fields */
    for(cdesc = ps_global->index_disp_format; cdesc->ctype != iNothing; cdesc++)
      if(cdesc->width){
	  memset(str, 0, sizeof(str));
	  ifield        = new_ifield(&ice->ifield);
	  ifield->ctype = cdesc->ctype;
	  ifield->width = cdesc->width;
	  fromfield   = 0;

	  if(idata->bogus){
	      if(cdesc->ctype == iMessNo)
		snprintf(str, sizeof(str), "%*.*s", ifield->width, ifield->width, " ");
	      else if(idata->bogus < 2 && (cdesc->ctype == iSubject
					   || cdesc->ctype == iSubjectText
					   || cdesc->ctype == iSubjKey
					   || cdesc->ctype == iSubjKeyText
					   || cdesc->ctype == iSubjKeyInit
					   || cdesc->ctype == iSubjKeyInitText))
		snprintf(str, sizeof(str), "%s", _("[ No Message Text Available ]"));
	  }
	  else
	    switch(cdesc->ctype){
	      case iStatus:
		to_us = status = ' ';
		if(collapsed){
		    thrd = fetch_thread(idata->stream, idata->rawno);
		    to_us = to_us_symbol_for_thread(idata->stream, thrd, 1);
		    status = status_symbol_for_thread(idata->stream, thrd,
						      cdesc->ctype);
		}
		else{
		    if((mc=mail_elt(idata->stream,idata->rawno)) && mc->flagged)
		      to_us = '*';		/* simple */
		    else if(!IS_NEWS(idata->stream)){
			for(addr = fetch_to(idata); addr; addr = addr->next)
			  if(address_is_us(addr, ps_global)){
			      ice->to_us = 1;
			      if(to_us == ' ')
				to_us = '+';

			      break;
			  }

			if(to_us != '+' && resent_to_us(idata)){
			    ice->to_us = 1;
			    if(to_us == ' ')
			      to_us = '+';
			}

			if(to_us == ' ' && F_ON(F_MARK_FOR_CC,ps_global))
			  for(addr = fetch_cc(idata); addr; addr = addr->next)
			    if(address_is_us(addr, ps_global)){
				ice->cc_us = 1;
				to_us = '-';
				break;
			    }
		    }

		    status = (!idata->stream || !IS_NEWS(idata->stream)
			      || F_ON(F_FAKE_NEW_IN_NEWS, ps_global))
			       ? 'N' : ' ';

		     if(mc->seen)
		       status = ' ';

		     if(user_flag_is_set(idata->stream, idata->rawno, FORWARDED_FLAG))
		       status = 'F';

		     if(mc->answered)
		       status = 'A';

		     if(mc->deleted)
		       status = 'D';
		}

		snprintf(str, sizeof(str), "%c %c", to_us, status);

		ifield->leftadj = 1;
		for(i = 0; i < 3; i++){
		    ielem  = new_ielem(&ifield->ielem);
		    ielem->freedata = 1;
		    ielem->data = (char *) fs_get(2 * sizeof(char));
		    ielem->data[0] = str[i];
		    ielem->data[1] = '\0';
		    ielem->datalen = 1;
		    set_print_format(ielem, 1, ifield->leftadj);
		}

		if(pico_usingcolor()){

		    if(str[0] == '*'){
			if(VAR_IND_IMP_FORE_COLOR && VAR_IND_IMP_BACK_COLOR){
			    ielem = ifield->ielem;
			    ielem->freecolor = 1;
			    ielem->color = new_color_pair(VAR_IND_IMP_FORE_COLOR, VAR_IND_IMP_BACK_COLOR);
			}
		    }
		    else if(str[0] == '+' || str[0] == '-'){
			if(VAR_IND_PLUS_FORE_COLOR && VAR_IND_PLUS_BACK_COLOR){
			    ielem = ifield->ielem;
			    ielem->freecolor = 1;
			    ielem->color = new_color_pair(VAR_IND_PLUS_FORE_COLOR, VAR_IND_PLUS_BACK_COLOR);
			}
		    }

		    if(str[2] == 'D'){
			if(VAR_IND_DEL_FORE_COLOR && VAR_IND_DEL_BACK_COLOR){
			    ielem = ifield->ielem->next->next;
			    ielem->freecolor = 1;
			    ielem->color = new_color_pair(VAR_IND_DEL_FORE_COLOR, VAR_IND_DEL_BACK_COLOR);
			}
		    }
		    else if(str[2] == 'A'){
			if(VAR_IND_ANS_FORE_COLOR && VAR_IND_ANS_BACK_COLOR){
			    ielem = ifield->ielem->next->next;
			    ielem->freecolor = 1;
			    ielem->color = new_color_pair(VAR_IND_ANS_FORE_COLOR, VAR_IND_ANS_BACK_COLOR);
			}
		    }
		    else if(str[2] == 'F'){
			if(VAR_IND_FWD_FORE_COLOR && VAR_IND_FWD_BACK_COLOR){
			    ielem = ifield->ielem->next->next;
			    ielem->freecolor = 1;
			    ielem->color = new_color_pair(VAR_IND_FWD_FORE_COLOR, VAR_IND_FWD_BACK_COLOR);
			}
		    }
		    else if(str[2] == 'N'){
			if(VAR_IND_NEW_FORE_COLOR && VAR_IND_NEW_BACK_COLOR){
			    ielem = ifield->ielem->next->next;
			    ielem->freecolor = 1;
			    ielem->color = new_color_pair(VAR_IND_NEW_FORE_COLOR, VAR_IND_NEW_BACK_COLOR);
			}
		    }
		}

		break;

	      case iFStatus:
	      case iIStatus:
	      case iSIStatus:
	      {
		  char new, answered, deleted, flagged;

		  if(collapsed){
		      thrd = fetch_thread(idata->stream, idata->rawno);
		      to_us = to_us_symbol_for_thread(idata->stream, thrd, 0);
		  }
		  else{
		      to_us = ' ';
		      if(!IS_NEWS(idata->stream)){
			for(addr = fetch_to(idata); addr; addr = addr->next)
			  if(address_is_us(addr, ps_global)){
			      to_us = '+';
			      break;
			  }
		      
			if(to_us == ' ' && resent_to_us(idata))
			  to_us = '+';

			if(to_us == ' ' && F_ON(F_MARK_FOR_CC,ps_global))
			  for(addr = fetch_cc(idata); addr; addr = addr->next)
			    if(address_is_us(addr, ps_global)){
				to_us = '-';
				break;
			    }
		      }
		  }

		  new = answered = deleted = flagged = ' ';

		  if(collapsed){
		      unsigned long save_branch, cnt, tot_in_thrd;

		      /*
		       * Branch is a sibling, not part of the thread, so
		       * don't consider it when displaying this line.
		       */
		      save_branch = thrd->branch;
		      thrd->branch = 0L;

		      tot_in_thrd = count_flags_in_thread(idata->stream, thrd,
							  F_NONE);

		      cnt = count_flags_in_thread(idata->stream, thrd, F_DEL);
		      if(cnt)
			deleted = (cnt == tot_in_thrd) ? 'D' : 'd';

		      cnt = count_flags_in_thread(idata->stream, thrd, F_ANS);
		      if(cnt)
			answered = (cnt == tot_in_thrd) ? 'A' : 'a';

		      /* no lower case *, same thing for some or all */
		      if(count_flags_in_thread(idata->stream, thrd, F_FLAG))
			flagged = '*';

		      new = status_symbol_for_thread(idata->stream, thrd,
						     cdesc->ctype);

		      thrd->branch = save_branch;
		  }
		  else{
		      mc = (idata->rawno > 0L && idata->stream
		            && idata->rawno <= idata->stream->nmsgs)
			    ? mail_elt(idata->stream, idata->rawno) : NULL;
		      if(mc && mc->valid){
			  if(cdesc->ctype == iIStatus || cdesc->ctype == iSIStatus){
			      if(mc->recent)
				new = mc->seen ? 'R' : 'N';
			      else if (!mc->seen)
				new = 'U';
			  }
			  else if(!mc->seen
				  && (!IS_NEWS(idata->stream)
				      || F_ON(F_FAKE_NEW_IN_NEWS, ps_global)))
			    new = 'N';

			  if(mc->answered)
			    answered = 'A';

			  if(mc->deleted)
			    deleted = 'D';

			  if(mc->flagged)
			    flagged = '*';
		      }
		  }

		  snprintf(str, sizeof(str), "%c %c%c%c%c", to_us, flagged, new,
			  answered, deleted);

		  if(cdesc->ctype == iSIStatus)
		    start = 2;
		  else
		    start = 0;

		  ifield->leftadj = 1;
		  for(i = start; i < 6; i++){
		    ielem  = new_ielem(&ifield->ielem);
		    ielem->freedata = 1;
		    ielem->data = (char *) fs_get(2 * sizeof(char));
		    ielem->data[0] = str[i];
		    ielem->data[1] = '\0';
		    ielem->datalen = 1;
		    set_print_format(ielem, 1, ifield->leftadj);
		  }

		  if(pico_usingcolor()){

		      if(str[0] == '+' || str[0] == '-'){
			  if(start == 0
			     && VAR_IND_PLUS_FORE_COLOR
			     && VAR_IND_PLUS_BACK_COLOR){
			      ielem = ifield->ielem;
			      ielem->freecolor = 1;
			      ielem->color = new_color_pair(VAR_IND_PLUS_FORE_COLOR, VAR_IND_PLUS_BACK_COLOR);
			  }
		      }

		      if(str[2] == '*'){
			  if(VAR_IND_IMP_FORE_COLOR && VAR_IND_IMP_BACK_COLOR){
			      if(start == 2)
			        ielem = ifield->ielem;
			      else
			        ielem = ifield->ielem->next->next;

			      ielem->freecolor = 1;
			      ielem->color = new_color_pair(VAR_IND_IMP_FORE_COLOR, VAR_IND_IMP_BACK_COLOR);
			  }
		      }

		      if(str[3] == 'N' || str[3] == 'n'){
			  if(VAR_IND_NEW_FORE_COLOR && VAR_IND_NEW_BACK_COLOR){
			      if(start == 2)
			        ielem = ifield->ielem->next;
			      else
			        ielem = ifield->ielem->next->next->next;

			      ielem->freecolor = 1;
			      ielem->color = new_color_pair(VAR_IND_NEW_FORE_COLOR, VAR_IND_NEW_BACK_COLOR);
			  }
		      }
		      else if(str[3] == 'R' || str[3] == 'r'){
			  if(VAR_IND_REC_FORE_COLOR && VAR_IND_REC_BACK_COLOR){
			      if(start == 2)
			        ielem = ifield->ielem->next;
			      else
			        ielem = ifield->ielem->next->next->next;

			      ielem->freecolor = 1;
			      ielem->color = new_color_pair(VAR_IND_REC_FORE_COLOR, VAR_IND_REC_BACK_COLOR);
			  }
		      }
		      else if(str[3] == 'U' || str[3] == 'u'){
			  if(VAR_IND_UNS_FORE_COLOR && VAR_IND_UNS_BACK_COLOR){
			      if(start == 2)
			        ielem = ifield->ielem->next;
			      else
			        ielem = ifield->ielem->next->next->next;

			      ielem->freecolor = 1;
			      ielem->color = new_color_pair(VAR_IND_UNS_FORE_COLOR, VAR_IND_UNS_BACK_COLOR);
			  }
		      }

		      if(str[4] == 'A' || str[4] == 'a'){
			  if(VAR_IND_ANS_FORE_COLOR && VAR_IND_ANS_BACK_COLOR){
			      if(start == 2)
			        ielem = ifield->ielem->next->next;
			      else
			        ielem = ifield->ielem->next->next->next->next;

			      ielem->freecolor = 1;
			      ielem->color = new_color_pair(VAR_IND_ANS_FORE_COLOR, VAR_IND_ANS_BACK_COLOR);
			  }
		      }

		      if(str[5] == 'D' || str[5] == 'd'){
			  if(VAR_IND_DEL_FORE_COLOR && VAR_IND_DEL_BACK_COLOR){
			      if(start == 2)
			        ielem = ifield->ielem->next->next->next;
			      else
			        ielem = ifield->ielem->next->next->next->next->next;

			      ielem->freecolor = 1;
			      ielem->color = new_color_pair(VAR_IND_DEL_FORE_COLOR, VAR_IND_DEL_BACK_COLOR);
			  }
		      }
		  }
	      }

	      break;

	      case iMessNo:
		/*
		 * This is a special case. The message number is
		 * generated on the fly in the painting routine.
		 * But the data array is allocated here in case it
		 * is useful for the paint routine.
		 */
		snprintf(str, sizeof(str), "%*.*s", ifield->width, ifield->width, " ");
		break;

	      case iArrow:
		snprintf(str, sizeof(str), "%-*.*s", ifield->width, ifield->width, " ");
		if(VAR_IND_ARR_FORE_COLOR && VAR_IND_ARR_BACK_COLOR){
		    ifield->leftadj = 1;
		    ielem  = new_ielem(&ifield->ielem);
		    ielem->freedata = 1;
		    ielem->data = cpystr(str);
		    ielem->datalen = strlen(str);
		    set_print_format(ielem, ifield->width, ifield->leftadj);
		    ielem->freecolor = 1;
		    ielem->color = new_color_pair(VAR_IND_ARR_FORE_COLOR,
						  VAR_IND_ARR_BACK_COLOR);
	        }

		break;

	      case iScore:
		score = get_msg_score(idata->stream, idata->rawno);
		if(score == SCORE_UNDEF){
		    SEARCHSET *ss = NULL;

		    ss = mail_newsearchset();
		    ss->first = ss->last = (unsigned long) idata->rawno;
		    if(ss){
			/*
			 * This looks like it might be expensive to get the
			 * score for each message when needed but it shouldn't
			 * be too bad because we know we have the envelope
			 * data cached. We can't calculate all of the scores
			 * we need for the visible messages right here in
			 * one fell swoop because we don't have the other
			 * envelopes yet. And we can't get the other
			 * envelopes at this point because we may be in
			 * the middle of a c-client callback (pine_imap_env).
			 * (Actually we could, because we know whether or
			 * not we're in the callback because of the no_fetch
			 * parameter.)
			 * We have another problem if the score rules depend
			 * on something other than envelope data. I guess they
			 * only do that if they have an alltext (search the
			 * text of the message) definition. So, we're going
			 * to pass no_fetch to calculate_scores so that it
			 * can return an error if we need the text data but
			 * can't get it because of no_fetch. Setting bogus
			 * will cause us to do the scores calculation later
			 * when we are no longer in the callback.
			 */
			idata->bogus =
			    (calculate_some_scores(idata->stream,
						   ss, idata->no_fetch) == 0)
					? 1 : 0;
			score = get_msg_score(idata->stream, idata->rawno);
			mail_free_searchset(&ss);
		    }
		}

		snprintf(str, sizeof(str), "%ld", score != SCORE_UNDEF ? score : 0L);
		break;

	      case iDate: case iMonAbb: case iLDate:
	      case iSDate: case iSTime:
	      case iS1Date: case iS2Date: case iS3Date: case iS4Date:
	      case iDateIso: case iDateIsoS: case iTime24: case iTime12:
	      case iSDateIsoS: case iSDateIso:
	      case iSDateS1: case iSDateS2: case iSDateS3: case iSDateS4:
	      case iSDateTime:
	      case iSDateTimeIsoS: case iSDateTimeIso:
	      case iSDateTimeS1: case iSDateTimeS2: case iSDateTimeS3: case iSDateTimeS4:
	      case iSDateTime24:
	      case iSDateTimeIsoS24: case iSDateTimeIso24:
	      case iSDateTimeS124: case iSDateTimeS224: case iSDateTimeS324: case iSDateTimeS424:
	      case iTimezone: case iYear: case iYear2Digit:
	      case iRDate: case iDay: case iDay2Digit: case iMon2Digit:
	      case iDayOrdinal: case iMon: case iMonLong:
	      case iDayOfWeekAbb: case iDayOfWeek:
	      case iPrefDate: case iPrefTime: case iPrefDateTime:
		date_str(fetch_date(idata), cdesc->ctype, 0, str, sizeof(str), cdesc->monabb_width);
		break;

	      case iFromTo:
	      case iFromToNotNews:
	      case iFrom:
	      case iAddress:
	      case iMailbox:
		fromfield++;
		from_str(cdesc->ctype, idata, str, sizeof(str), ice);
	        break;

	      case iTo:
		if(((field = ((addr = fetch_to(idata))
			      ? "To"
			      : (addr = fetch_cc(idata))
			      ? "Cc"
			      : NULL))
		    && !set_index_addr(idata, field, addr, NULL, BIGWIDTH, str))
		   || !field)
		  if((newsgroups = fetch_newsgroups(idata)) != NULL)
		    snprintf(str, sizeof(str), "%-.*s", BIGWIDTH, newsgroups);

		break;

	      case iCc:
		set_index_addr(idata, "Cc", fetch_cc(idata), NULL, BIGWIDTH, str);
		break;

	      case iRecips:
		toaddr = fetch_to(idata);
		ccaddr = fetch_cc(idata);
		for(last_to = toaddr;
		    last_to && last_to->next;
		    last_to = last_to->next)
		  ;
		 
		/* point end of to list temporarily at cc list */
		if(last_to)
		  last_to->next = ccaddr;

		set_index_addr(idata, "To", toaddr, NULL, BIGWIDTH, str);

		if(last_to)
		  last_to->next = NULL;

		break;

	      case iSender:
		fromfield++;
		if((addr = fetch_sender(idata)) != NULL)
		  set_index_addr(idata, "Sender", addr, NULL, BIGWIDTH, str);

		break;

	      case iInit:
		{ADDRESS *addr;

		 if((addr = fetch_from(idata)) && addr->personal){
		     char *name, *initials = NULL;

		     name = (char *) rfc1522_decode_to_utf8((unsigned char *)tmp_20k_buf,
							    SIZEOF_20KBUF, addr->personal);
		     if(name == addr->personal){
			 strncpy(tmp_20k_buf, name, SIZEOF_20KBUF-1);
			 tmp_20k_buf[SIZEOF_20KBUF - 1] = '\0';
			 name = (char *) tmp_20k_buf;
		     }

		     if(name && *name){
			 initials = reply_quote_initials(name);
			 snprintf(str, sizeof(str), "%-.*s", BIGWIDTH, initials);
		     }
		 }
		}

	        break;

	      case iSize:
		/* 0 ... 9999 */
		if((l = fetch_size(idata)) < 10*1000L)
		  snprintf(str, sizeof(str), "(%lu)", l);
		/* 10K ... 999K */
		else if(l < 1000L*1000L - 1000L/2){
		    l = l/1000L + (l%1000L >= 1000L/2 ? 1L : 0L);
		    snprintf(str, sizeof(str), "(%luK)", l);
		}
		/* 1.0M ... 99.9M */
		else if(l < 1000L*100L*1000L - 100L*1000L/2){
		    l = l/(100L*1000L) + (l%(100L*1000L) >= (100*1000L/2)
								? 1L : 0L);
		    snprintf(str, sizeof(str), "(%lu.%luM)", l/10L, l % 10L);
		}
		/* 100M ... 2000M */
		else if(l <= 2*1000L*1000L*1000L){
		    l = l/(1000L*1000L) + (l%(1000L*1000L) >= (1000L*1000L/2)
								? 1L : 0L);
		    snprintf(str, sizeof(str), "(%luM)", l);
		}
		else
		  snprintf(str, sizeof(str), "(HUGE!)");

		break;

	      case iSizeComma:
		/* 0 ... 99,999 */
		if((l = fetch_size(idata)) < 100*1000L)
		  snprintf(str, sizeof(str), "(%s)", comatose(l));
		/* 100K ... 9,999K */
		else if(l < 10L*1000L*1000L - 1000L/2){
		    l = l/1000L + (l%1000L >= 1000L/2 ? 1L : 0L);
		    snprintf(str, sizeof(str), "(%sK)", comatose(l));
		}
		/* 10.0M ... 999.9M */
		else if(l < 1000L*1000L*1000L - 100L*1000L/2){
		    l = l/(100L*1000L) + (l%(100L*1000L) >= (100*1000L/2)
								? 1L : 0L);
		    snprintf(str, sizeof(str), "(%lu.%luM)", l/10L, l % 10L);
		}
		/* 1,000M ... 2,000M */
		else if(l <= 2*1000L*1000L*1000L){
		    l = l/(1000L*1000L) + (l%(1000L*1000L) >= (1000L*1000L/2)
								? 1L : 0L);
		    snprintf(str, sizeof(str), "(%sM)", comatose(l));
		}
		else
		  snprintf(str, sizeof(str), "(HUGE!)");

		break;

	      case iSizeNarrow:
		/* 0 ... 999 */
		if((l = fetch_size(idata)) < 1000L)
		  snprintf(str, sizeof(str), "(%lu)", l);
		/* 1K ... 99K */
		else if(l < 100L*1000L - 1000L/2){
		    l = l/1000L + (l%1000L >= 1000L/2 ? 1L : 0L);
		    snprintf(str, sizeof(str), "(%luK)", l);
		}
		/* .1M ... .9M */
		else if(l < 1000L*1000L - 100L*1000L/2){
		    l = l/(100L*1000L) + (l%(100L*1000L) >= 100L*1000L/2
								? 1L : 0L);
		    snprintf(str, sizeof(str), "(.%luM)", l);
		}
		/* 1M ... 99M */
		else if(l < 1000L*100L*1000L - 1000L*1000L/2){
		    l = l/(1000L*1000L) + (l%(1000L*1000L) >= (1000L*1000L/2)
								? 1L : 0L);
		    snprintf(str, sizeof(str), "(%luM)", l);
		}
		/* .1G ... .9G */
		else if(l < 1000L*1000L*1000L - 100L*1000L*1000L/2){
		    l = l/(100L*1000L*1000L) + (l%(100L*1000L*1000L) >=
					    (100L*1000L*1000L/2) ? 1L : 0L);
		    snprintf(str, sizeof(str), "(.%luG)", l);
		}
		/* 1G ... 2G */
		else if(l <= 2*1000L*1000L*1000L){
		    l = l/(1000L*1000L*1000L) + (l%(1000L*1000L*1000L) >=
					    (1000L*1000L*1000L/2) ? 1L : 0L);
		    snprintf(str, sizeof(str), "(%luG)", l);
		}
		else
		  snprintf(str, sizeof(str), "(HUGE!)");

		break;

	      /* From Carl Jacobsen <carl@ucsd.edu> */
	      case iKSize:
		l = fetch_size(idata);
		l = (l / 1024L) + (l % 1024L != 0 ? 1 : 0);

		if(l < 1024L) {				/* 0k .. 1023k */
		  snprintf(str, sizeof(str), "(%luk)", l);

		} else if (l < 100L * 1024L){		/* 1.0M .. 99.9M */
		  snprintf(str, sizeof(str), "(%lu.M)", (l * 10L) / 1024L);
		  if ((p = strchr(str, '.')) != NULL) {
		    p--; p[1] = p[0]; p[0] = '.';  /* swap last digit & . */
		  }
		} else if (l <= 2L * 1024L * 1024L) {	/* 100M .. 2048 */
		  snprintf(str, sizeof(str), "(%luM)", l / 1024L);
		} else {
		  snprintf(str, sizeof(str), "(HUGE!)");
		}

		break;

	      case iDescripSize:
		if((body = fetch_body(idata)) != NULL)
		  switch(body->type){
		    case TYPETEXT:
		    {
		        mc = (idata->rawno > 0L && idata->stream
		              && idata->rawno <= idata->stream->nmsgs)
			      ? mail_elt(idata->stream, idata->rawno) : NULL;
			if(mc && mc->rfc822_size < 6000)
			  snprintf(str, sizeof(str), "(short  )");
			else if(mc && mc->rfc822_size < 25000)
			  snprintf(str, sizeof(str), "(medium )");
			else if(mc && mc->rfc822_size < 100000)
			  snprintf(str, sizeof(str), "(long   )");
			else
			  snprintf(str, sizeof(str), "(huge   )");
		    }

		    break;

		    case TYPEMULTIPART:
		      if(strucmp(body->subtype, "MIXED") == 0){
			  int x;

			  x = body->nested.part
			    ? body->nested.part->body.type
			    : TYPETEXT + 1000;
			  switch(x){
			    case TYPETEXT:
			      if(body->nested.part->body.size.bytes < 6000)
				snprintf(str, sizeof(str), "(short+ )");
			      else if(body->nested.part->body.size.bytes
				      < 25000)
				snprintf(str, sizeof(str), "(medium+)");
			      else if(body->nested.part->body.size.bytes
				      < 100000)
				snprintf(str, sizeof(str), "(long+  )");
			      else
				snprintf(str, sizeof(str), "(huge+  )");
			      break;

			    default:
			      snprintf(str, sizeof(str), "(multi  )");
			      break;
			  }
		      }
		      else if(strucmp(body->subtype, "DIGEST") == 0)
			snprintf(str, sizeof(str), "(digest )");
		      else if(strucmp(body->subtype, "ALTERNATIVE") == 0)
			snprintf(str, sizeof(str), "(mul/alt)");
		      else if(strucmp(body->subtype, "PARALLEL") == 0)
			snprintf(str, sizeof(str), "(mul/par)");
		      else
			snprintf(str, sizeof(str), "(multi  )");

		      break;

		    case TYPEMESSAGE:
		      snprintf(str, sizeof(str), "(message)");
		      break;

		    case TYPEAPPLICATION:
		      snprintf(str, sizeof(str), "(applica)");
		      break;

		    case TYPEAUDIO:
		      snprintf(str, sizeof(str), "(audio  )");
		      break;

		    case TYPEIMAGE:
		      snprintf(str, sizeof(str), "(image  )");
		      break;

		    case TYPEVIDEO:
		      snprintf(str, sizeof(str), "(video  )");
		      break;

		    default:
		      snprintf(str, sizeof(str), "(other  )");
		      break;
		  }

		break;

	      case iAtt:
		str[0] = SPACE;
		str[1] = '\0';
		if((body = fetch_body(idata)) &&
		   body->type == TYPEMULTIPART &&
		   strucmp(body->subtype, "ALTERNATIVE") != 0){
		    PART *part;
		    int   atts = 0;

		    part = body->nested.part;  /* 1st part, don't count */
		    while(part && part->next && atts < 10){
			atts++;
			part = part->next;
		    }

		    if(atts > 9)
		      str[0] = '*';
		    else if(atts > 0)
		      str[0] = '0' + atts;
		}

		break;

	      case iSubject:
		subj_str(idata, str, sizeof(str), NoKW, 0, ice);
		break;

	      case iSubjectText:
		subj_str(idata, str, sizeof(str), NoKW, 1, ice);
		break;

	      case iSubjKey:
		subj_str(idata, str, sizeof(str), KW, 0, ice);
		break;

	      case iSubjKeyText:
		subj_str(idata, str, sizeof(str), KW, 1, ice);
		break;

	      case iSubjKeyInit:
		subj_str(idata, str, sizeof(str), KWInit, 0, ice);
		break;

	      case iSubjKeyInitText:
		subj_str(idata, str, sizeof(str), KWInit, 1, ice);
		break;

	      case iOpeningText:
	      case iOpeningTextNQ:
		if(idata->no_fetch)
		  idata->bogus = 1;
		else{
		    char *first_text;

		    first_text = fetch_firsttext(idata, cdesc->ctype == iOpeningTextNQ);

		    if(first_text){
			strncpy(str, first_text, BIGWIDTH);
			str[BIGWIDTH] = '\0';
			fs_give((void **) &first_text);
		    }
		}

		break;

	      case iKey:
		key_str(idata, KW, ice);
		break;

	      case iKeyInit:
		key_str(idata, KWInit, ice);
		break;

	      case iNews:
		if((newsgroups = fetch_newsgroups(idata)) != NULL){
		    strncpy(str, newsgroups, BIGWIDTH);
		    str[BIGWIDTH] = '\0';
		}

		break;

	      case iNewsAndTo:
		if((newsgroups = fetch_newsgroups(idata)) != NULL)
		  strncpy(str, newsgroups, sizeof(str));

		if((l = strlen(str)) < sizeof(str)){
		    if(sizeof(str) - l < 6)
		      strncpy(str+l, "...", sizeof(str)-l);
		    else{
			if(l > 0){
			    strncpy(str+l, " and ", sizeof(str)-l);
			    set_index_addr(idata, "To", fetch_to(idata),
					   NULL, BIGWIDTH-l-5, str+l+5);
			    if(!str[l+5])
			      str[l] = '\0';
			}
			else
			  set_index_addr(idata, "To", fetch_to(idata),
				         NULL, BIGWIDTH, str);
		    }
		}

		break;

	      case iToAndNews:
		set_index_addr(idata, "To", fetch_to(idata),
			       NULL, BIGWIDTH, str);
		if((l = strlen(str)) < sizeof(str) &&
		   (newsgroups = fetch_newsgroups(idata))){
		    if(sizeof(str) - l < 6)
		      strncpy(str+l, "...", sizeof(str)-l);
		    else{
			if(l > 0)
			  strncpy(str+l, " and ", sizeof(str)-l);

			if(l > 0)
			  strncpy(str+l+5, newsgroups, BIGWIDTH-l-5);
			else
			  strncpy(str, newsgroups, BIGWIDTH);
		    }
		}

		break;

	      case iNewsAndRecips:
		if((newsgroups = fetch_newsgroups(idata)) != NULL)
		  strncpy(str, newsgroups, BIGWIDTH);

		if((l = strlen(str)) < BIGWIDTH){
		    if(BIGWIDTH - l < 6)
		      strncpy(str+l, "...", BIGWIDTH-l);
		    else{
			toaddr = fetch_to(idata);
			ccaddr = fetch_cc(idata);
			for(last_to = toaddr;
			    last_to && last_to->next;
			    last_to = last_to->next)
			  ;
			 
			/* point end of to list temporarily at cc list */
			if(last_to)
			  last_to->next = ccaddr;

			if(l > 0){
			    strncpy(str+l, " and ", sizeof(str)-l);
			    set_index_addr(idata, "To", toaddr,
					   NULL, BIGWIDTH-l-5, str+l+5);
			    if(!str[l+5])
			      str[l] = '\0';
			}
			else
			  set_index_addr(idata, "To", toaddr, NULL, BIGWIDTH, str);

			if(last_to)
			  last_to->next = NULL;
		    }
		}

		break;

	      case iRecipsAndNews:
		toaddr = fetch_to(idata);
		ccaddr = fetch_cc(idata);
		for(last_to = toaddr;
		    last_to && last_to->next;
		    last_to = last_to->next)
		  ;
		 
		/* point end of to list temporarily at cc list */
		if(last_to)
		  last_to->next = ccaddr;

		set_index_addr(idata, "To", toaddr, NULL, BIGWIDTH, str);

		if(last_to)
		  last_to->next = NULL;

		if((l = strlen(str)) < BIGWIDTH &&
		   (newsgroups = fetch_newsgroups(idata))){
		    if(BIGWIDTH - l < 6)
		      strncpy(str+l, "...", BIGWIDTH-l);
		    else{
			if(l > 0)
			  strncpy(str+l, " and ", sizeof(str)-l);

			if(l > 0)
			  strncpy(str+l+5, newsgroups, BIGWIDTH-l-5);
			else
			  strncpy(str, newsgroups, BIGWIDTH);
		    }
		}

		break;

	      case iPrio:
	      case iPrioAlpha:
	      case iPrioBang:
		prio_str(idata, cdesc->ctype, ice);
		break;

	      case iHeader:
		header_str(idata, cdesc->hdrtok, ice);
		break;

	      case iText:
		strncpy(str, (cdesc->hdrtok && cdesc->hdrtok->hdrname) ? cdesc->hdrtok->hdrname : "", sizeof(str));
		str[sizeof(str)-1] = '\0';
		break;

	      default:
		break;
	    }

	  /*
	   * If the element wasn't already filled in above, do it here.
	   */
	  if(!ifield->ielem){
	      ielem  = new_ielem(&ifield->ielem);

	      ielem->freedata = 1;
	      ielem->data = cpystr(str);
	      ielem->datalen = strlen(str);

	      if(fromfield && pico_usingcolor()
		 && ps_global->VAR_IND_FROM_FORE_COLOR
		 && ps_global->VAR_IND_FROM_BACK_COLOR){
		  ielem->type = eTypeCol;
		  ielem->freecolor = 1;
		  ielem->color = new_color_pair(ps_global->VAR_IND_FROM_FORE_COLOR,
						ps_global->VAR_IND_FROM_BACK_COLOR);
		  /*
		   * This space is here so that if the text does
		   * not extend all the way to the end of the field then
		   * we'll switch the color back and paint the rest of the
		   * field in the Normal color or the index line color.
		   */
		  ielem = new_ielem(&ielem);
		  ielem->freedata = 1;
		  ielem->data = cpystr(" ");
		  ielem->datalen = 1;
	      }
	      else if((cdesc->ctype == iOpeningText || cdesc->ctype == iOpeningTextNQ)
		      && pico_usingcolor()
		      && ps_global->VAR_IND_OP_FORE_COLOR
		      && ps_global->VAR_IND_OP_BACK_COLOR){
		  ielem->type = eTypeCol;
		  ielem->freecolor = 1;
		  ielem->color = new_color_pair(ps_global->VAR_IND_OP_FORE_COLOR,
						ps_global->VAR_IND_OP_BACK_COLOR);
		  /*
		   * This space is here so that if the text does
		   * not extend all the way to the end of the field then
		   * we'll switch the color back and paint the rest of the
		   * field in the Normal color or the index line color.
		   */
		  ielem = new_ielem(&ielem);
		  ielem->freedata = 1;
		  ielem->data = cpystr(" ");
		  ielem->datalen = 1;
	      }

	      ifield->leftadj = (cdesc->adjustment == Left) ? 1 : 0;
	      set_ielem_widths_in_field(ifield);
	  }
      }

    ice->widths_done = 1;
    ice->id = ice_hash(ice);

    /*
     * Now we have to put the temporary copy of ice back as the
     * real thing.
     */
    icep = fetch_ice_ptr(idata->stream, idata->rawno);
    if(icep){
	free_ice(icep);	/* free what is already there */
	*icep = ice;
    }

    return(ice);
}


ICE_S *
format_thread_index_line(INDEXDATA_S *idata)
{
    char         *p, buffer[BIGWIDTH+1];
    int           thdlen, space_left, i;
    PINETHRD_S   *thrd = NULL;
    ICE_S        *ice, *tice = NULL, **ticep = NULL;
    IFIELD_S     *ifield;
    IELEM_S      *ielem;
    int         (*save_sfstr_func)(void);
    struct variable *vars = ps_global->vars;

    dprint((8, "=== format_thread_index_line(%ld,%ld) ===\n",
	       idata ? idata->msgno : -1, idata ? idata->rawno : -1));

    space_left = ps_global->ttyo->screen_cols;

    if(ps_global->msgmap->max_thrdno < 1000)
      thdlen = 3;
    else if(ps_global->msgmap->max_thrdno < 10000)
      thdlen = 4;
    else if(ps_global->msgmap->max_thrdno < 100000)
      thdlen = 5;
    else
      thdlen = 6;

    ice = fetch_ice(idata->stream, idata->rawno);

    thrd = fetch_thread(idata->stream, idata->rawno);

    if(!thrd || !ice)			/* can't happen? */
      return(ice);
    
    if(!ice->tice){
	tice = (ICE_S *) fs_get(sizeof(*tice));
	memset(tice, 0, sizeof(*tice));
	ice->tice = tice;
    }

    tice = ice->tice;

    if(!tice)
      return(ice);

    free_ifield(&tice->ifield);

    ticep = &ice->tice;
    tice = copy_ice(tice);

    if(space_left >= 3){
	char to_us, status;

	p = buffer;
	to_us = to_us_symbol_for_thread(idata->stream, thrd, 1);
	status = status_symbol_for_thread(idata->stream, thrd, iStatus);

	if((p-buffer)+3 < sizeof(buffer)){
	    p[0] = to_us;
	    p[1] = ' ';
	    p[2] = status;
	    p[3] = '\0';;
	}

	space_left -= 3;

	ifield = new_ifield(&tice->ifield);
	ifield->ctype = iStatus;
	ifield->width = 3;
	ifield->leftadj = 1;
	for(i = 0; i < 3; i++){
	    ielem  = new_ielem(&ifield->ielem);
	    ielem->freedata = 1;
	    ielem->data = (char *) fs_get(2 * sizeof(char));
	    ielem->data[0] = p[i];
	    ielem->data[1] = '\0';
	    ielem->datalen = 1;
	    set_print_format(ielem, 1, ifield->leftadj);
	}

	if(pico_usingcolor()){
	    if(to_us == '*'
	       && VAR_IND_IMP_FORE_COLOR && VAR_IND_IMP_BACK_COLOR){
		ielem = ifield->ielem;
		ielem->freecolor = 1;
		ielem->color = new_color_pair(VAR_IND_IMP_FORE_COLOR,
					      VAR_IND_IMP_BACK_COLOR);
		if(F_ON(F_COLOR_LINE_IMPORTANT, ps_global))
		  tice->linecolor = new_color_pair(VAR_IND_IMP_FORE_COLOR,
						   VAR_IND_IMP_BACK_COLOR);
	    }
	    else if((to_us == '+' || to_us == '-')
		    && VAR_IND_PLUS_FORE_COLOR && VAR_IND_PLUS_BACK_COLOR){
		ielem = ifield->ielem;
		ielem->freecolor = 1;
		ielem->color = new_color_pair(VAR_IND_PLUS_FORE_COLOR,
					      VAR_IND_PLUS_BACK_COLOR);
	    }

	    if(status == 'D'
	       && VAR_IND_DEL_FORE_COLOR && VAR_IND_DEL_BACK_COLOR){
		ielem = ifield->ielem->next->next;
		ielem->freecolor = 1;
		ielem->color = new_color_pair(VAR_IND_DEL_FORE_COLOR,
					      VAR_IND_DEL_BACK_COLOR);
	    }
	    else if(status == 'N'
		    && VAR_IND_NEW_FORE_COLOR && VAR_IND_NEW_BACK_COLOR){
		ielem = ifield->ielem->next->next;
		ielem->freecolor = 1;
		ielem->color = new_color_pair(VAR_IND_NEW_FORE_COLOR,
					      VAR_IND_NEW_BACK_COLOR);
	    }
	}
    }

    if(space_left >= thdlen+1){
	p = buffer;
	space_left--;

	snprintf(p, sizeof(buffer), "%*.*s", thdlen, thdlen, "");
	space_left -= thdlen;

	ifield = new_ifield(&tice->ifield);
	ifield->ctype = iMessNo;
	ifield->width = thdlen;
	ifield->leftadj = 0;
	ielem  = new_ielem(&ifield->ielem);
	ielem->freedata = 1;
	ielem->data = cpystr(p);
	ielem->datalen = strlen(p);
	set_print_format(ielem, ifield->width, ifield->leftadj);
    }

    if(space_left >= 7){

	p = buffer;
	space_left--;

	date_str(fetch_date(idata), iDate, 0, p, sizeof(buffer), 0);
	if(sizeof(buffer) > 6)
	  p[6] = '\0';

	if(strlen(p) < 6 && (sizeof(buffer)) > 6){
	    char *q;

	    for(q = p + strlen(p); q < p + 6; q++)
	      *q = ' ';
	}

	space_left -= 6;

	ifield = new_ifield(&tice->ifield);
	ifield->ctype = iDate;
	ifield->width = 6;
	ifield->leftadj = 1;
	ielem  = new_ielem(&ifield->ielem);
	ielem->freedata = 1;
	ielem->data = cpystr(p);
	ielem->datalen = ifield->width;
	set_print_format(ielem, ifield->width, ifield->leftadj);
    }


    if(space_left > 3){
	int   from_width, subj_width, bigthread_adjust;
	long  in_thread;
	char  from[BIGWIDTH+1];
	char  tcnt[50];

	space_left--;

	in_thread = count_lflags_in_thread(idata->stream, thrd,
					   ps_global->msgmap, MN_NONE);

	p = buffer;
	if(in_thread == 1 && THRD_AUTO_VIEW())
	  snprintf(tcnt, sizeof(tcnt), "   ");
	else
	  snprintf(tcnt, sizeof(tcnt), "(%ld)", in_thread);

	bigthread_adjust = MAX(0, strlen(tcnt) - 3);
	
	/* third of the rest */
	from_width = MAX((space_left-1)/3 - bigthread_adjust, 1);

	/* the rest */
	subj_width = space_left - from_width - 1;

	if(strlen(tcnt) > subj_width)
	  tcnt[subj_width] = '\0';

	from[0] = '\0';
	save_sfstr_func = pith_opt_truncate_sfstr;
	pith_opt_truncate_sfstr = NULL;
	from_str(iFromTo, idata, from, sizeof(from), tice);
	pith_opt_truncate_sfstr = save_sfstr_func;

	ifield = new_ifield(&tice->ifield);
	ifield->leftadj = 1;
	ielem  = new_ielem(&ifield->ielem);
	ielem->freedata = 1;
	ielem->type = eTypeCol;
	ielem->data = cpystr(from);
	ielem->datalen = strlen(from);
	ifield->width = from_width;
	set_print_format(ielem, ifield->width, ifield->leftadj);
	ifield->ctype = iFrom;
	if(from_width > 0 && pico_usingcolor()
	   && VAR_IND_FROM_FORE_COLOR && VAR_IND_FROM_BACK_COLOR){
	    ielem->freecolor = 1;
	    ielem->color = new_color_pair(VAR_IND_FROM_FORE_COLOR,
					  VAR_IND_FROM_BACK_COLOR);
	}

        ifield = new_ifield(&tice->ifield);
	ifield->leftadj = 0;
	ielem  = new_ielem(&ifield->ielem);
	ielem->freedata = 1;
	ielem->data = cpystr(tcnt);
	ielem->datalen = strlen(tcnt);
	ifield->width = ielem->datalen;
	set_print_format(ielem, ifield->width, ifield->leftadj);
	ifield->ctype = iAtt;	/* not used, except that it isn't special */

	subj_width -= strlen(tcnt);

	if(subj_width > 0)
	  subj_width--;

	if(subj_width > 0){
	    if(idata->bogus){
		if(idata->bogus < 2)
		  snprintf(buffer, sizeof(buffer), "%-.*s", BIGWIDTH,
			  _("[ No Message Text Available ]"));
	    }
	    else{
		buffer[0] = '\0';
		save_sfstr_func = pith_opt_truncate_sfstr;
		pith_opt_truncate_sfstr = NULL;
		subj_str(idata, buffer, sizeof(buffer), NoKW, 0, NULL);
		pith_opt_truncate_sfstr = save_sfstr_func;
	    }

	    ifield = new_ifield(&tice->ifield);
	    ifield->leftadj = 1;
	    ielem  = new_ielem(&ifield->ielem);
	    ielem->freedata = 1;
	    ielem->type = eTypeCol;
	    ielem->data = cpystr(buffer);
	    ielem->datalen = strlen(buffer);
	    ifield->width = subj_width;
	    set_print_format(ielem, ifield->width, ifield->leftadj);
	    ifield->ctype = iSubject;
	    if(pico_usingcolor() && VAR_IND_SUBJ_FORE_COLOR && VAR_IND_SUBJ_BACK_COLOR){
		ielem->freecolor = 1;
		ielem->color = new_color_pair(VAR_IND_SUBJ_FORE_COLOR,
					      VAR_IND_SUBJ_BACK_COLOR);
	    }
	}
    }
    else if(space_left > 1){
	snprintf(p, sizeof(buffer)-(p-buffer), "%-.*s", space_left-1, " ");
	ifield = new_ifield(&tice->ifield);
	ifield->leftadj = 1;
	ielem  = new_ielem(&ifield->ielem);
	ielem->freedata = 1;
	ielem->data = cpystr(p);
	ielem->datalen = strlen(p);
	ifield->width = space_left-1;
	set_print_format(ielem, ifield->width, ifield->leftadj);
	ifield->ctype = iSubject;
    }

    tice->widths_done = 1;
    tice->id = ice_hash(tice);

    if(ticep){
	free_ice(ticep);	/* free what is already there */
	*ticep = tice;
    }

    return(ice);
}


/*
 * Print the fields of ice in buf with a single space between fields.
 *
 * Args    buf    -- place to put the line
 *         ice    -- the data for the line
 *       msgno    -- this is the msgno to be used, blanks if <= 0
 *
 * Returns a pointer to buf.
 */
char *
simple_index_line(char *buf, size_t buflen, ICE_S *ice, long int msgno)
{
    char     *p;
    IFIELD_S *ifield, *previfield = NULL;
    IELEM_S  *ielem;

    if(!buf)
      panic("NULL buf in simple_index_line()");

    if(buflen > 0)
      buf[0] = '\0';

    p = buf;

    if(ice){
      
      for(ifield = ice->ifield; ifield && p-buf < buflen; ifield = ifield->next){

	/* space between fields */
	if(ifield != ice->ifield && !(previfield && previfield->ctype == iText))
	  *p++ = ' ';

	/* message number string is generated on the fly */
	if(ifield->ctype == iMessNo){
	  ielem = ifield->ielem;
	  if(ielem && ielem->datalen >= ifield->width){
	    if(msgno > 0L)
	      snprintf(ielem->data, ielem->datalen+1, "%*.ld", ifield->width, msgno);
	    else
	      snprintf(ielem->data, ielem->datalen+1, "%*.*s", ifield->width, ifield->width, "");
	  }
	}

	for(ielem = ifield->ielem;
	    ielem && ielem->print_format && p-buf < buflen;
	    ielem = ielem->next){
	  char *src;
	  size_t bytes_added;

	  src = ielem->data;
	  bytes_added = utf8_pad_to_width(p, src,
					  buflen-(p-buf) * sizeof(char),
					  ielem->wid, ifield->leftadj);
	  p += bytes_added;
	}

	previfield = ifield;
      }

      if(p-buf < buflen)
        *p = '\0';
    }

    buf[buflen-1] = '\0';

    return(buf);
}


/*
 * Look in current mail_stream for matches for messages in the searchset
 * which match a color rule pattern. Return the color.
 * The searched bit will be set for all of the messages which match the
 * first pattern which has a match.
 *
 * Args     stream -- the mail stream
 *       searchset -- restrict attention to this set of messages
 *          pstate -- The pattern state. On the first call it will be Null.
 *                      Null means start over with a new first_pattern.
 *                      After that it will be pointing to our local PAT_STATE
 *                      so that next_pattern goes to the next one after the
 *                      ones we've already checked.
 *
 * Returns   0 if no match, 1 if a match.
 *           The color that goes with the matched rule in returned_color.
 *           It may be NULL, which indicates default.
 */
int
get_index_line_color(MAILSTREAM *stream, SEARCHSET *searchset,
		     PAT_STATE **pstate, COLOR_PAIR **returned_color)
{
    PAT_S           *pat = NULL;
    long             rflags = ROLE_INCOL;
    COLOR_PAIR      *color = NULL;
    int              match = 0;
    static PAT_STATE localpstate;

    dprint((7, "get_index_line_color\n"));

    if(returned_color)
      *returned_color = NULL;

    if(*pstate)
      pat = next_pattern(*pstate);
    else{
	*pstate = &localpstate;
	if(!nonempty_patterns(rflags, *pstate))
	  *pstate = NULL;

	if(*pstate)
	  pat = first_pattern(*pstate);
    }

    if(*pstate){
    
	/* Go through the possible roles one at a time until we get a match. */
	while(!match && pat){
	    if(match_pattern(pat->patgrp, stream, searchset, NULL,
			     get_msg_score, SE_NOSERVER|SE_NOPREFETCH)){
		if(!pat->action || pat->action->bogus)
		  break;

		match++;
		if(pat->action && pat->action->incol)
		  color = new_color_pair(pat->action->incol->fg,
				         pat->action->incol->bg);
	    }
	    else
	      pat = next_pattern(*pstate);
	}
    }

    if(match && returned_color)
      *returned_color = color;

    return(match);
}


/*
 *
 */
int
index_in_overview(MAILSTREAM *stream)
{
    INDEX_COL_S	 *cdesc = NULL;

    if(!(stream->mailbox && IS_REMOTE(stream->mailbox)))
      return(FALSE);			/* no point! */

    if(stream->dtb && stream->dtb->name && !strcmp(stream->dtb->name, "nntp")){

      if(THRD_INDX())
        return(TRUE);

      for(cdesc = ps_global->index_disp_format;
	  cdesc->ctype != iNothing;
	  cdesc++)
	switch(cdesc->ctype){
	  case iTo:			/* can't be satisfied by XOVER */
	  case iSender:			/* ... or specifically handled */
	  case iDescripSize:		/* ... in news case            */
	  case iAtt:
	    return(FALSE);

	  default :
	    break;
	}
    }

    return(TRUE);
}



/*
 * fetch_from - called to get a the index entry's "From:" field
 */
int
resent_to_us(INDEXDATA_S *idata)
{
    if(!idata->valid_resent_to){
	static char *fields[] = {"Resent-To", NULL};
	char *h;

	if(idata->no_fetch){
	    idata->bogus = 1;	/* don't do this */
	    return(FALSE);
	}

	if((h = pine_fetchheader_lines(idata->stream,idata->rawno,NULL,fields)) != NULL){
	    idata->resent_to_us = parsed_resent_to_us(h);
	    fs_give((void **) &h);
	}

	idata->valid_resent_to = 1;
    }

    return(idata->resent_to_us);
}


int
parsed_resent_to_us(char *h)
{
    char    *p, *q;
    ADDRESS *addr = NULL;
    int	     rv = FALSE;

    if((p = strindex(h, ':')) != NULL){
	for(q = ++p; (q = strpbrk(q, "\015\012")) != NULL; q++)
	  *q = ' ';		/* quash junk */

	rfc822_parse_adrlist(&addr, p, ps_global->maildomain);
	if(addr){
	    rv = address_is_us(addr, ps_global);
	    mail_free_address(&addr);
	}
    }

    return(rv);
}



/*
 * fetch_from - called to get a the index entry's "From:" field
 */
ADDRESS *
fetch_from(INDEXDATA_S *idata)
{
    if(idata->no_fetch)			/* implies from is valid */
      return(idata->from);
    else if(idata->bogus)
      idata->bogus = 2;
    else{
	ENVELOPE *env;

	/* c-client call's just cache access at this point */
	if((env = pine_mail_fetchenvelope(idata->stream, idata->rawno)) != NULL)
	  return(env->from);

	idata->bogus = 1;
    }

    return(NULL);
}


/*
 * fetch_to - called to get a the index entry's "To:" field
 */
ADDRESS *
fetch_to(INDEXDATA_S *idata)
{
    if(idata->no_fetch){		/* check for specific validity */
	if(idata->valid_to)
	  return(idata->to);
	else
	  idata->bogus = 1;		/* can't give 'em what they want */
    }
    else if(idata->bogus){
	idata->bogus = 2;		/* elevate bogosity */
    }
    else{
	ENVELOPE *env;

	/* c-client call's just cache access at this point */
	if((env = pine_mail_fetchenvelope(idata->stream, idata->rawno)) != NULL)
	  return(env->to);

	idata->bogus = 1;
    }

    return(NULL);
}


/*
 * fetch_cc - called to get a the index entry's "Cc:" field
 */
ADDRESS *
fetch_cc(INDEXDATA_S *idata)
{
    if(idata->no_fetch){		/* check for specific validity */
	if(idata->valid_cc)
	  return(idata->cc);
	else
	  idata->bogus = 1;		/* can't give 'em what they want */
    }
    else if(idata->bogus){
	idata->bogus = 2;		/* elevate bogosity */
    }
    else{
	ENVELOPE *env;

	/* c-client call's just cache access at this point */
	if((env = pine_mail_fetchenvelope(idata->stream, idata->rawno)) != NULL)
	  return(env->cc);

	idata->bogus = 1;
    }

    return(NULL);
}



/*
 * fetch_sender - called to get a the index entry's "Sender:" field
 */
ADDRESS *
fetch_sender(INDEXDATA_S *idata)
{
    if(idata->no_fetch){		/* check for specific validity */
	if(idata->valid_sender)
	  return(idata->sender);
	else
	  idata->bogus = 1;		/* can't give 'em what they want */
    }
    else if(idata->bogus){
	idata->bogus = 2;		/* elevate bogosity */
    }
    else{
	ENVELOPE *env;

	/* c-client call's just cache access at this point */
	if((env = pine_mail_fetchenvelope(idata->stream, idata->rawno)) != NULL)
	  return(env->sender);

	idata->bogus = 1;
    }

    return(NULL);
}


/*
 * fetch_newsgroups - called to get a the index entry's "Newsgroups:" field
 */
char *
fetch_newsgroups(INDEXDATA_S *idata)
{
    if(idata->no_fetch){		/* check for specific validity */
	if(idata->valid_news)
	  return(idata->newsgroups);
	else
	  idata->bogus = 1;		/* can't give 'em what they want */
    }
    else if(idata->bogus){
	idata->bogus = 2;		/* elevate bogosity */
    }
    else{
	ENVELOPE *env;

	/* c-client call's just cache access at this point */
	if((env = pine_mail_fetchenvelope(idata->stream, idata->rawno)) != NULL)
	  return(env->newsgroups);

	idata->bogus = 1;
    }

    return(NULL);
}


/*
 * fetch_subject - called to get at the index entry's "Subject:" field
 */
char *
fetch_subject(INDEXDATA_S *idata)
{
    if(idata->no_fetch)			/* implies subject is valid */
      return(idata->subject);
    else if(idata->bogus)
      idata->bogus = 2;
    else{
	ENVELOPE *env;

	/* c-client call's just cache access at this point */
	if((env = pine_mail_fetchenvelope(idata->stream, idata->rawno)) != NULL)
	  return(env->subject);

	idata->bogus = 1;
    }

    return(NULL);
}


/*
 * Return an allocated copy of the first few characters from the body
 * of the message for possible use in the index screen.
 *
 * Maybe we could figure out some way to do aggregate calls to get
 * this info for all the lines in view instead of all the one at a
 * time calls we're doing now.
 */
char *
fetch_firsttext(INDEXDATA_S *idata, int delete_quotes)
{
    ENVELOPE *env;
    BODY *body = NULL;
    char *firsttext = NULL;
    STORE_S *so;
    gf_io_t pc;
    long partial_fetch_len = 0L;
    SEARCHSET *ss, **sset;

try_again:

    /*
     * Prevent wild prefetch, just get the one we're after.
     * Can we get this somehow in the overview call in build_header_work?
     */
    ss = mail_newsearchset();
    ss->first = idata->rawno;
    sset = (SEARCHSET **) mail_parameters(idata->stream,
					  GET_FETCHLOOKAHEAD,
					  (void *) idata->stream);
    if(sset)
      *sset = ss;

    if((env = pine_mail_fetchstructure(idata->stream, idata->rawno, &body)) != NULL){
	if(body){
	    char *subtype = NULL;
	    char *partno;

	    if((body->type == TYPETEXT
		&& (subtype=body->subtype) && ALLOWED_SUBTYPE(subtype))
		    ||
	       (body->type == TYPEMULTIPART && body->nested.part
		&& body->nested.part->body.type == TYPETEXT
		&& (subtype=body->nested.part->body.subtype)
		&& ALLOWED_SUBTYPE(subtype))
		    ||
	       (body->type == TYPEMULTIPART && body->nested.part
		&& body->nested.part->body.type == TYPEMULTIPART
		&& body->nested.part->body.nested.part
		&& body->nested.part->body.nested.part->body.type == TYPETEXT
		&& (subtype=body->nested.part->body.nested.part->body.subtype)
		&& ALLOWED_SUBTYPE(subtype))){

		if((so = so_get(CharStar, NULL, EDIT_ACCESS)) != NULL){
		    char buf[1025], *p;
		    unsigned char c;
		    int success;
		    int one_space_done = 0;

		    if(partial_fetch_len == 0L){
			if(subtype && !strucmp(subtype, "html"))
			  partial_fetch_len = 1024L;
			else if(subtype && !strucmp(subtype, "plain"))
			  partial_fetch_len = delete_quotes ? 128L : 64L;
			else
			  partial_fetch_len = 256L;
		    }

		    if((body->type == TYPETEXT
			&& (subtype=body->subtype) && ALLOWED_SUBTYPE(subtype))
			    ||
		       (body->type == TYPEMULTIPART && body->nested.part
			&& body->nested.part->body.type == TYPETEXT
			&& (subtype=body->nested.part->body.subtype)
			&& ALLOWED_SUBTYPE(subtype)))
		      partno = "1";
		    else
		      partno = "1.1";

		    gf_set_so_writec(&pc, so);
		    success = get_body_part_text(idata->stream, body, idata->rawno,
						 partno, partial_fetch_len, pc,
						 NULL, NULL,
						 GBPT_NOINTR | GBPT_PEEK |
						 (delete_quotes ? GBPT_DELQUOTES : 0));
		    gf_clear_so_writec(so);

		    if(success){
			so_seek(so, 0L, 0);
			p = buf;
			while(p-buf < sizeof(buf)-1 && so_readc(&c, so)){
			    /* delete leading whitespace */
			    if(p == buf && isspace(c))
			      ;
			    /* and include just one space per run of whitespace */
			    else if(isspace(c)){
				if(!one_space_done){
				    *p++ = SPACE;
				    one_space_done++;
				}
			    }
			    else{
				one_space_done = 0;
				*p++ = c;
			    }
			}

			*p = '\0';

			if(p > buf){
			    size_t l;

			    l = strlen(buf);
			    l += 100;
			    firsttext = fs_get((l+1) * sizeof(char));
			    firsttext[0] = '\0';
			    iutf8ncpy(firsttext, buf, l);
			    firsttext[l] = '\0';
			    removing_trailing_white_space(firsttext);
			}
		    }

		    so_give(&so);

		    /* first if means we didn't fetch all of the data */
		    if(!(success > 1 && success < partial_fetch_len)){
			if(partial_fetch_len < 4096L
			   && (!firsttext || utf8_width(firsttext) < 50)){
			    if(firsttext)
			      fs_give((void **) &firsttext);

			    partial_fetch_len = 4096L;
			    goto try_again;
			}
		    }
		}
	    }	
	}
    }

    if(ss)
      mail_free_searchset(&ss);

    return(firsttext);
}


/*
 * fetch_date - called to get at the index entry's "Date:" field
 */
char *
fetch_date(INDEXDATA_S *idata)
{
    if(idata->no_fetch)			/* implies date is valid */
      return(idata->date);
    else if(idata->bogus)
      idata->bogus = 2;
    else{
	ENVELOPE *env;

	/* c-client call's just cache access at this point */
	if((env = pine_mail_fetchenvelope(idata->stream, idata->rawno)) != NULL)
	  return((char *) env->date);

	idata->bogus = 1;
    }

    return(NULL);
}


/*
 * fetch_header - called to get at the index entry's "Hdrname:" field
 */
char *
fetch_header(INDEXDATA_S *idata, char *hdrname)
{
    if(idata->no_fetch)
      idata->bogus = 1;
    else if(idata->bogus)
      idata->bogus = 2;
    else{
	char *h, *p, *q, *decoded, *fields[2];
	size_t retsize, decsize;
	char *ret = NULL;
	unsigned char *decode_buf = NULL;

	fields[0] = hdrname;
	fields[1] = NULL;
	if(hdrname && hdrname[0]
	   && (h = pine_fetchheader_lines(idata->stream, idata->rawno,
					  NULL, fields))){
				      
	    if(strlen(h) < strlen(hdrname) + 1){
		fs_give((void **) &h);
		return(cpystr(""));
	    }

	    /* skip "hdrname:" */
	    for(p = h + strlen(hdrname) + 1;
		*p && isspace((unsigned char)*p); p++)
		;

	    decsize = (4 * strlen(p)) + 1;
	    decode_buf = (unsigned char *) fs_get(decsize * sizeof(unsigned char));
	    decoded = (char *) rfc1522_decode_to_utf8(decode_buf, decsize, p);
	    p = decoded;

	    retsize = strlen(decoded);
	    q = ret = (char *) fs_get((retsize+1) * sizeof(char));

	    *q = '\0';
	    while(q-ret < retsize && *p){
		if(*p == '\015' || *p == '\012')
		  p++;
		else if(*p == '\t'){
		  *q++ = SPACE;
		  p++;
		}
		else
		  *q++ = *p++;
	    }

	    *q = '\0';

	    fs_give((void **) &h);
	    if(decode_buf)
	      fs_give((void **) &decode_buf);

	    return(ret);
	}

	idata->bogus = 1;
    }

    return(NULL);
}


/*
 * fetch_size - called to get at the index entry's "size" field
 */
long
fetch_size(INDEXDATA_S *idata)
{
    if(idata->no_fetch)			/* implies size is valid */
      return(idata->size);
    else if(idata->bogus)
      idata->bogus = 2;
    else{
	MESSAGECACHE *mc;

	if(idata->stream && idata->rawno > 0L
	   && idata->rawno <= idata->stream->nmsgs
	   && (mc = mail_elt(idata->stream, idata->rawno)))
	  return(mc->rfc822_size);

	idata->bogus = 1;
    }

    return(0L);
}


/*
 * fetch_body - called to get a the index entry's body structure
 */
BODY *
fetch_body(INDEXDATA_S *idata)
{
    BODY *body;
    
    if(idata->bogus || idata->no_fetch){
	idata->bogus = 2;
	return(NULL);
    }

    if(pine_mail_fetchstructure(idata->stream, idata->rawno, &body))
      return(body);

    idata->bogus = 1;
    return(NULL);
}


/*
 * s is at least size width+1
 */
int
set_index_addr(INDEXDATA_S	   *idata,
	       char		   *field,
	       struct mail_address *addr,
	       char		   *prefix,
	       int		    width,
	       char		   *s)
{
    ADDRESS *atmp;
    char    *p, *stmp = NULL, *sptr;
    char    *save_personal = NULL;
    int      orig_width;

    s[0] = '\0';

    for(atmp = addr; idata->stream && atmp; atmp = atmp->next)
      if(atmp->host && atmp->host[0] == '.'){
	  char *pref, *h, *fields[2];
	  
	  if(idata->no_fetch){
	      idata->bogus = 1;
	      return(TRUE);
	  }

	  fields[0] = field;
	  fields[1] = NULL;
	  if((h = pine_fetchheader_lines(idata->stream, idata->rawno,
					NULL, fields)) != NULL){
	      if(strlen(h) < strlen(field) + 1){
		  p = h + strlen(h);
	      }
	      else{
		  /* skip "field:" */
		  for(p = h + strlen(field) + 1;
		      *p && isspace((unsigned char)*p); p++)
		    ;
	      }

	      orig_width = width;
	      sptr = stmp = (char *) fs_get((orig_width+1) * sizeof(char));

	      /* add prefix */
	      for(pref = prefix; pref && *pref; pref++)
		if(width){
		    *sptr++ = *pref;
		    width--;
		}
		else
		  break;

	      while(width--)
		if(*p == '\015' || *p == '\012')
		  p++;				/* skip CR LF */
		else if(!*p)
		  *sptr++ = ' ';
		else if(*p == '\t'){
		    *sptr++ = ' ';
		    p++;
		}
		else
		  *sptr++ = *p++;

	      *sptr = '\0';			/* tie off return string */

	      if(stmp){
		  iutf8ncpy(s, stmp, orig_width+1);
		  s[orig_width] = '\0';
		  fs_give((void **) &stmp);
	      }

	      fs_give((void **) &h);
	      return(TRUE);
	  }
	  /* else fall thru and display what c-client gave us */
      }

    if(addr && !addr->next		/* only one address */
       && addr->host			/* not group syntax */
       && addr->personal && addr->personal[0]){	/* there is a personal name */
	char  buftmp[MAILTMPLEN];
	int   l;

	if((l = prefix ? strlen(prefix) : 0) != 0)
	  strncpy(s, prefix, width+1);

	snprintf(buftmp, sizeof(buftmp), "%s", addr->personal);
	p = (char *) rfc1522_decode_to_utf8((unsigned char *) tmp_20k_buf,
					    SIZEOF_20KBUF, buftmp);
	removing_leading_and_trailing_white_space(p);

	iutf8ncpy(s + l, p, width - l);

	s[width] = '\0';
	
	if(*(s+l))
	  return(TRUE);
	else{
	    save_personal = addr->personal;
	    addr->personal = NULL;
	}
    }

    if(addr){
	char *a_string;
	int   l;

	a_string = addr_list_string(addr, NULL, 0);
	if(save_personal)
	  addr->personal = save_personal;

	if((l = prefix ? strlen(prefix) : 0) != 0)
	  strncpy(s, prefix, width+1);

	iutf8ncpy(s + l, a_string, width - l);
	s[width] = '\0';

	fs_give((void **)&a_string);

	return(TRUE);
    }

    if(save_personal)
      addr->personal = save_personal;

    return(FALSE);
}


void
index_data_env(INDEXDATA_S *idata, ENVELOPE *env)
{
    if(!env){
	idata->bogus = 2;
	return;
    }

    idata->from	      = env->from;
    idata->to	      = env->to;
    idata->cc	      = env->cc;
    idata->sender     = env->sender;
    idata->subject    = env->subject;
    idata->date	      = (char *) env->date;
    idata->newsgroups = env->newsgroups;

    idata->valid_to = 1;	/* signal that everythings here */
    idata->valid_cc = 1;
    idata->valid_sender = 1;
    idata->valid_news = 1;
}


/*
 * Put a string representing the date into str. The source date is
 * in the string datesrc. The format to be used is in type.
 * Notice that type is an IndexColType, but really only a subset of
 * IndexColType types are allowed.
 *
 * Args  datesrc -- The source date string
 *          type -- What type of output we want
 *             v -- If set, variable width output is ok. (Oct 9 not Oct  9)
 *           str -- Put the answer here.
 *       str_len -- Length of str
 *  monabb_width -- This is a hack to get dates to line up right. For
 *                  example, in French (but without accents here)
 *                          dec.  21
 *                          fevr. 23
 *                          mars   7
 *                     For this monabb_width would be 5.
 */
void
date_str(char *datesrc, IndexColType type, int v, char *str, size_t str_len,
	 int monabb_width)
{
    char	year4[5],	/* 4 digit year			*/
		yearzero[3],	/* zero padded, 2-digit year	*/
		monzero[3],	/* zero padded, 2-digit month	*/
		mon[3],		/* 1 or 2-digit month, no pad	*/
		dayzero[3],	/* zero padded, 2-digit day	*/
		day[3],		/* 1 or 2-digit day, no pad	*/
		dayord[3],	/* 2-letter ordinal label	*/
		monabb[10],	/* 3-letter month abbrev	*/
				    /* actually maybe not 3 if localized */
		hour24[3],	/* 2-digit, 24 hour clock hour	*/
		hour12[3],	/* 12 hour clock hour, no pad	*/
		minzero[3],	/* zero padded, 2-digit minutes */
		timezone[6];	/* timezone, like -0800 or +... */
    int		hr12;
    int         curtype, lastmonthtype, lastyeartype, preftype;
    int         sdatetimetype, sdatetime24type;
    struct	date d;
#define TODAYSTR N_("Today")

    curtype =       (type == iCurDate ||
	             type == iCurDateIso ||
	             type == iCurDateIsoS ||
	             type == iCurPrefDate ||
	             type == iCurPrefDateTime ||
	             type == iCurPrefTime ||
	             type == iCurTime24 ||
	             type == iCurTime12 ||
	             type == iCurDay ||
	             type == iCurDay2Digit ||
	             type == iCurDayOfWeek ||
	             type == iCurDayOfWeekAbb ||
	             type == iCurMon ||
	             type == iCurMon2Digit ||
	             type == iCurMonLong ||
	             type == iCurMonAbb ||
	             type == iCurYear ||
	             type == iCurYear2Digit);
    lastmonthtype = (type == iLstMon ||
	             type == iLstMon2Digit ||
	             type == iLstMonLong ||
	             type == iLstMonAbb ||
	             type == iLstMonYear ||
	             type == iLstMonYear2Digit);
    lastyeartype =  (type == iLstYear ||
	             type == iLstYear2Digit);
    sdatetimetype = (type == iSDateTime ||
	             type == iSDateTimeIso ||
	             type == iSDateTimeIsoS ||
	             type == iSDateTimeS1 ||
	             type == iSDateTimeS2 ||
	             type == iSDateTimeS3 ||
	             type == iSDateTimeS4 ||
		     type == iSDateTime24 ||
	             type == iSDateTimeIso24 ||
	             type == iSDateTimeIsoS24 ||
	             type == iSDateTimeS124 ||
	             type == iSDateTimeS224 ||
	             type == iSDateTimeS324 ||
	             type == iSDateTimeS424);
    sdatetime24type = (type == iSDateTime24 ||
	             type == iSDateTimeIso24 ||
	             type == iSDateTimeIsoS24 ||
	             type == iSDateTimeS124 ||
	             type == iSDateTimeS224 ||
	             type == iSDateTimeS324 ||
	             type == iSDateTimeS424);
    preftype =      (type == iPrefDate ||
	             type == iPrefDateTime ||
	             type == iPrefTime ||
	             type == iCurPrefDate ||
	             type == iCurPrefDateTime ||
	             type == iCurPrefTime);
    if(str_len > 0)
      str[0] = '\0';

    if(!(datesrc && datesrc[0]) && !(curtype || lastmonthtype || lastyeartype))
      return;

    if(curtype || lastmonthtype || lastyeartype){
	char dbuf[200];

	rfc822_date(dbuf);
	parse_date(dbuf, &d);

	if(lastyeartype)
	  d.year--;
	else if(lastmonthtype){
	    d.month--;
	    if(d.month <= 0){
		d.month = 12;
		d.year--;
	    }
	}
    }
    else{
	parse_date(F_ON(F_DATES_TO_LOCAL,ps_global)
		    ? convert_date_to_local(datesrc) : datesrc, &d);
	if(d.year == -1 || d.month == -1 || d.day == -1){
	    sdatetimetype = 0;
	    sdatetime24type = 0;
	    preftype = 0;
	    switch(type){
	      case iSDate: case iSDateTime: case iSDateTime24:
		type = iS1Date;
		break;
	      
	      case iSDateIso: case iSDateTimeIso: case iSDateTimeIso24:
	      case iPrefDate: case iPrefTime: case iPrefDateTime:
		type = iDateIso;
		break;
	      
	      case iSDateIsoS: case iSDateTimeIsoS: case iSDateTimeIsoS24:
		type = iDateIsoS;
		break;
	      
	      case iSDateS1: case iSDateTimeS1: case iSDateTimeS124:
		type = iS1Date;
		break;
	      
	      case iSDateS2: case iSDateTimeS2: case iSDateTimeS224:
		type = iS1Date;
		break;
	      
	      case iSDateS3: case iSDateTimeS3: case iSDateTimeS324:
		type = iS1Date;
		break;
	      
	      case iSDateS4: case iSDateTimeS4: case iSDateTimeS424:
		type = iS1Date;
		break;
	      
	      default:
		break;
	    }
	}
    }

    /* some special ones to start with */
    if(preftype){
	struct tm tm, *tmptr = NULL;
	time_t now;

	/*
	 * Make sure we get the right one if we're using current time.
	 */
	if(curtype){
	    now = time((time_t *) 0);
	    if(now != (time_t) -1)
	      tmptr = localtime(&now);
	}

	if(!tmptr){
	    memset(&tm, 0, sizeof(tm));
	    tm.tm_year = MIN(MAX(d.year-1900, 0), 2000);
	    tm.tm_mon  = MIN(MAX(d.month-1, 0), 11);
	    tm.tm_mday = MIN(MAX(d.day, 1), 31);
	    tm.tm_hour = MIN(MAX(d.hour, 0), 23);
	    tm.tm_min  = MIN(MAX(d.minute, 0), 59);
	    tmptr = &tm;
	}

	switch(type){
	  case iPrefDate:
	  case iCurPrefDate:
	    our_strftime(str, str_len, "%x", tmptr);
	    break;
	  case iPrefTime:
	  case iCurPrefTime:
	    our_strftime(str, str_len, "%X", tmptr);
	    break;
	  case iPrefDateTime:
	  case iCurPrefDateTime:
	    our_strftime(str, str_len, "%c", tmptr);
	    break;
	  default:
	    assert(0);
	    break;
	}

	return;
    }

    strncpy(monabb, (d.month > 0 && d.month < 13)
		    ? month_abbrev_locale(d.month) : "", sizeof(monabb));
    monabb[sizeof(monabb)-1] = '\0';

    strncpy(mon, (d.month > 0 && d.month < 13)
		    ? int2string(d.month) : "", sizeof(mon));
    mon[sizeof(mon)-1] = '\0';

    strncpy(day, (d.day > 0 && d.day < 32)
		    ? int2string(d.day) : "", sizeof(day));
    day[sizeof(day)-1] = '\0';

    strncpy(dayord,
	   (d.day <= 0 || d.day > 31) ? "" :
	    (d.day == 1 || d.day == 21 || d.day == 31) ? "st" :
	     (d.day == 2 || d.day == 22 ) ? "nd" :
	      (d.day == 3 || d.day == 23 ) ? "rd" : "th", sizeof(dayord));

    dayord[sizeof(dayord)-1] = '\0';

    strncpy(year4, (d.year >= 1000 && d.year < 10000)
		    ? int2string(d.year) : "????", sizeof(year4));
    year4[sizeof(year4)-1] = '\0';

    if(d.year >= 0){
	if((d.year % 100) < 10){
	    yearzero[0] = '0';
	    strncpy(yearzero+1, int2string(d.year % 100), sizeof(yearzero)-1);
	}
	else
	  strncpy(yearzero, int2string(d.year % 100), sizeof(yearzero));
    }
    else
      strncpy(yearzero, "??", sizeof(yearzero));

    yearzero[sizeof(yearzero)-1] = '\0';

    if(d.month > 0 && d.month < 10){
	monzero[0] = '0';
	strncpy(monzero+1, int2string(d.month), sizeof(monzero)-1);
    }
    else if(d.month >= 10 && d.month <= 12)
      strncpy(monzero, int2string(d.month), sizeof(monzero));
    else
      strncpy(monzero, "??", sizeof(monzero));

    monzero[sizeof(monzero)-1] = '\0';

    if(d.day > 0 && d.day < 10){
	dayzero[0] = '0';
	strncpy(dayzero+1, int2string(d.day), sizeof(dayzero)-1);
    }
    else if(d.day >= 10 && d.day <= 31)
      strncpy(dayzero, int2string(d.day), sizeof(dayzero));
    else
      strncpy(dayzero, "??", sizeof(dayzero));

    dayzero[sizeof(dayzero)-1] = '\0';

    hr12 = (d.hour == 0) ? 12 :
	     (d.hour > 12) ? (d.hour - 12) : d.hour;
    hour12[0] = '\0';
    if(hr12 > 0 && hr12 <= 12)
      strncpy(hour12, int2string(hr12), sizeof(hour12));

    hour12[sizeof(hour12)-1] = '\0';

    hour24[0] = '\0';
    if(d.hour >= 0 && d.hour < 10){
	hour24[0] = '0';
	strncpy(hour24+1, int2string(d.hour), sizeof(hour24)-1);
    }
    else if(d.hour >= 10 && d.hour < 24)
      strncpy(hour24, int2string(d.hour), sizeof(hour24));

    hour24[sizeof(hour24)-1] = '\0';

    minzero[0] = '\0';
    if(d.minute >= 0 && d.minute < 10){
	minzero[0] = '0';
	strncpy(minzero+1, int2string(d.minute), sizeof(minzero)-1);
    }
    else if(d.minute >= 10 && d.minute <= 60)
      strncpy(minzero, int2string(d.minute), sizeof(minzero));

    minzero[sizeof(minzero)-1] = '\0';

    if(sizeof(timezone) > 5){
	if(d.hours_off_gmt <= 0){
	    timezone[0] = '-';
	    d.hours_off_gmt *= -1;
	    d.min_off_gmt *= -1;
	}
	else
	  timezone[0] = '+';

	timezone[1] = '\0';
	if(d.hours_off_gmt >= 0 && d.hours_off_gmt < 10){
	    timezone[1] = '0';
	    strncpy(timezone+2, int2string(d.hours_off_gmt), sizeof(timezone)-2);
	}
	else if(d.hours_off_gmt >= 10 && d.hours_off_gmt < 24)
	  strncpy(timezone+1, int2string(d.hours_off_gmt), sizeof(timezone)-1);
	else{
	    timezone[1] = '0';
	    timezone[2] = '0';
	}

	timezone[3] = '\0';
	if(d.min_off_gmt >= 0 && d.min_off_gmt < 10){
	    timezone[3] = '0';
	    strncpy(timezone+4, int2string(d.min_off_gmt), sizeof(timezone)-4);
	}
	else if(d.min_off_gmt >= 10 && d.min_off_gmt <= 60)
	  strncpy(timezone+3, int2string(d.min_off_gmt), sizeof(timezone)-3);
	else{
	    timezone[3] = '0';
	    timezone[4] = '0';
	}

	timezone[5] = '\0';
	timezone[sizeof(timezone)-1] = '\0';
    }

    switch(type){
      case iRDate:
	/* this one is not locale-specific */
	snprintf(str, str_len, "%s%s%s %s %s",
		(d.wkday != -1) ? day_abbrev(d.wkday) : "",
		(d.wkday != -1) ? ", " : "",
		day,
		(d.month > 0 && d.month < 13) ? month_abbrev(d.month) : "",
		year4);
	break;
      case iDayOfWeekAbb:
      case iCurDayOfWeekAbb:
	strncpy(str, (d.wkday >= 0 && d.wkday <= 6) ? day_abbrev_locale(d.wkday) : "", str_len);
	str[str_len-1] = '\0';
	break;
      case iDayOfWeek:
      case iCurDayOfWeek:
	strncpy(str, (d.wkday >= 0 && d.wkday <= 6) ? day_name_locale(d.wkday) : "", str_len);
	str[str_len-1] = '\0';
	break;
      case iYear:
      case iCurYear:
      case iLstYear:
      case iLstMonYear:
	strncpy(str, year4, str_len);
	break;
      case iDay2Digit:
      case iCurDay2Digit:
	strncpy(str, dayzero, str_len);
	break;
      case iMon2Digit:
      case iCurMon2Digit:
      case iLstMon2Digit:
	strncpy(str, monzero, str_len);
	break;
      case iYear2Digit:
      case iCurYear2Digit:
      case iLstYear2Digit:
      case iLstMonYear2Digit:
	strncpy(str, yearzero, str_len);
	break;
      case iTimezone:
	strncpy(str, timezone, str_len);
	break;
      case iDay:
      case iCurDay:
	strncpy(str, day, str_len);
	break;
      case iDayOrdinal:
        snprintf(str, str_len, "%s%s", day, dayord);
	break;
      case iMon:
      case iCurMon:
      case iLstMon:
	if(d.month > 0 && d.month <= 12)
	  strncpy(str, int2string(d.month), str_len);

	break;
      case iMonAbb:
      case iCurMonAbb:
      case iLstMonAbb:
	strncpy(str, monabb, str_len);
	break;
      case iMonLong:
      case iCurMonLong:
      case iLstMonLong:
	strncpy(str, (d.month > 0 && d.month < 13)
			? month_name_locale(d.month) : "", str_len);
	break;
      case iDate:
      case iCurDate:
	if(v)
	  snprintf(str, str_len, "%s%s%s", monabb, (monabb[0] && day[0]) ? " " : "", day);
	else{
	    if(monabb_width > 0)
	      utf8_snprintf(str, str_len, "%-*.*w %2s",
			    monabb_width, monabb_width, monabb, day);
	    else
	      snprintf(str, str_len, "%s %2s", monabb, day);
	}

	break;
      case iLDate:
	if(v)
	  snprintf(str, str_len, "%s%s%s%s%s", monabb,
	          (monabb[0] && day[0]) ? " " : "", day,
	          ((monabb[0] || day[0]) && year4[0]) ? ", " : "",
		  year4);
	else{
	    if(monabb_width > 0)
	      utf8_snprintf(str, str_len, "%-*.*w %2s%c %4s",
			    monabb_width, monabb_width,
			    monabb, day,
			    (monabb[0] && day[0] && year4[0]) ? ',' : ' ', year4);
	    else
	      snprintf(str, str_len, "%s %2s%c %4s", monabb, day,
		       (monabb[0] && day[0] && year4[0]) ? ',' : ' ',
		       year4);
	}

	break;
      case iS1Date:
      case iS2Date:
      case iS3Date:
      case iS4Date:
      case iDateIso:
      case iDateIsoS:
      case iCurDateIso:
      case iCurDateIsoS:
	if(monzero[0] == '?' && dayzero[0] == '?' &&
	   yearzero[0] == '?')
	  snprintf(str, str_len, "%8s", "");
	else{
	    switch(type){
	      case iS1Date:
		snprintf(str, str_len, "%2s/%2s/%2s",
			monzero, dayzero, yearzero);
		break;
	      case iS2Date:
		snprintf(str, str_len, "%2s/%2s/%2s",
			dayzero, monzero, yearzero);
		break;
	      case iS3Date:
		snprintf(str, str_len, "%2s.%2s.%2s",
			dayzero, monzero, yearzero);
		break;
	      case iS4Date:
		snprintf(str, str_len, "%2s.%2s.%2s",
			yearzero, monzero, dayzero);
		break;
	      case iDateIsoS:
	      case iCurDateIsoS:
		snprintf(str, str_len, "%2s-%2s-%2s",
			yearzero, monzero, dayzero);
		break;
	      case iDateIso:
	      case iCurDateIso:
		snprintf(str, str_len, "%4s-%2s-%2s",
			year4, monzero, dayzero);
		break;
	      default:
		break;
	    }
	}

	break;
      case iTime24:
      case iCurTime24:
	snprintf(str, str_len, "%2s%c%2s",
		(hour24[0] && minzero[0]) ? hour24 : "",
		(hour24[0] && minzero[0]) ? ':' : ' ',
		(hour24[0] && minzero[0]) ? minzero : "");
	break;
      case iTime12:
      case iCurTime12:
	snprintf(str, str_len, "%s%c%2s%s",
		(hour12[0] && minzero[0]) ? hour12 : "",
		(hour12[0] && minzero[0]) ? ':' : ' ',
		(hour12[0] && minzero[0]) ? minzero : "",
		(hour12[0] && minzero[0] && d.hour < 12) ? "am" :
		  (hour12[0] && minzero[0] && d.hour >= 12) ? "pm" :
		    "  ");
	break;
      case iSDate: case iSDateIso: case iSDateIsoS:
      case iSDateS1: case iSDateS2: case iSDateS3: case iSDateS4: 
      case iSDateTime: case iSDateTimeIso: case iSDateTimeIsoS:
      case iSDateTimeS1: case iSDateTimeS2: case iSDateTimeS3: case iSDateTimeS4: 
      case iSDateTime24: case iSDateTimeIso24: case iSDateTimeIsoS24:
      case iSDateTimeS124: case iSDateTimeS224: case iSDateTimeS324: case iSDateTimeS424: 
	{ struct date now, last_day;
	  char        dbuf[200];
	  int         msg_day_of_year, now_day_of_year, today;
	  int         diff, ydiff, last_day_of_year;

	  rfc822_date(dbuf);
	  parse_date(dbuf, &now);
	  today = day_of_week(&now) + 7;

	  if(today >= 0+7 && today <= 6+7){
	      now_day_of_year = day_of_year(&now);
	      msg_day_of_year = day_of_year(&d);
	      ydiff = now.year - d.year;

	      if(msg_day_of_year == -1)
		diff = -100;
	      else if(ydiff == 0)
		diff = now_day_of_year - msg_day_of_year;
	      else if(ydiff == 1){
		  last_day = d;
		  last_day.month = 12;
		  last_day.day = 31;
		  last_day_of_year = day_of_year(&last_day);

		  diff = now_day_of_year +
			  (last_day_of_year - msg_day_of_year);
	      }
	      else if(ydiff == -1){
		  last_day = now;
		  last_day.month = 12;
		  last_day.day = 31;
		  last_day_of_year = day_of_year(&last_day);

		  diff = -1 * (msg_day_of_year +
			  (last_day_of_year - now_day_of_year));
	      }
	      else if(ydiff > 1)
		diff = 100;
	      else
		diff = -100;

	      if(diff == 0)
		strncpy(str, _(TODAYSTR), str_len);
	      else if(diff == 1)
		strncpy(str, _("Yesterday"), str_len);
	      else if(diff > 1 && diff < 7)
		snprintf(str, str_len, "%s", day_name_locale((today - diff) % 7));
	      else if(diff == -1)
		strncpy(str, _("Tomorrow"), str_len);
	      else if(diff < -1 && diff > -7)
		snprintf(str, str_len, _("Next %.3s!"),
			 day_name_locale((today - diff) % 7));
	      else if(diff > 0
		      && (ydiff == 0
		          || (ydiff == 1 && 12 + now.month - d.month < 6))){
		  if(v)
		    snprintf(str, str_len, "%s%s%s", monabb,
			     (monabb[0] && day[0]) ? " " : "", day);
		  else{
		      if(monabb_width > 0)
			utf8_snprintf(str, str_len, "%-*.*w %2s",
				      monabb_width, monabb_width, monabb, day);
		      else
			snprintf(str, str_len, "%s %2s", monabb, day);
		  }
	      }
	      else{
		  if(msg_day_of_year == -1 && (type == iSDate || type == iSDateTime))
		    type = iSDateTimeIsoS;

		  switch(type){
		    case iSDate: case iSDateTime: case iSDateTime24:
		      {
			struct tm tm;

			memset(&tm, 0, sizeof(tm));
			tm.tm_year = MIN(MAX(d.year-1900, 0), 2000);
			tm.tm_mon  = MIN(MAX(d.month-1, 0), 11);
			tm.tm_mday = MIN(MAX(d.day, 1), 31);
			tm.tm_hour = MIN(MAX(d.hour, 0), 23);
			tm.tm_min  = MIN(MAX(d.minute, 0), 59);
			our_strftime(str, str_len, "%x", &tm);
		      }

		      break;
		    case iSDateS1: case iSDateTimeS1: case iSDateTimeS124:
		      if(v)
			snprintf(str, str_len, "%s/%s/%s%s", mon, day, yearzero,
				 diff < 0 ? "!" : "");
		      else
			snprintf(str, str_len, "%s%s/%s/%s%s",
				 (mon[0] && mon[1]) ? "" : " ",
				 mon, dayzero, yearzero,
				 diff < 0 ? "!" : "");
		      break;
		    case iSDateS2: case iSDateTimeS2: case iSDateTimeS224:
		      if(v)
			snprintf(str, str_len, "%s/%s/%s%s", day, mon, yearzero,
				 diff < 0 ? "!" : "");
		      else
			snprintf(str, str_len, "%s%s/%s/%s%s",
				 (day[0] && day[1]) ? "" : " ",
				 day, monzero, yearzero,
				 diff < 0 ? "!" : "");
		      break;
		    case iSDateS3: case iSDateTimeS3: case iSDateTimeS324:
		      if(v)
			snprintf(str, str_len, "%s.%s.%s%s", day, mon, yearzero,
				 diff < 0 ? "!" : "");
		      else
			snprintf(str, str_len, "%s%s.%s.%s%s",
				 (day[0] && day[1]) ? "" : " ",
				 day, monzero, yearzero,
				 diff < 0 ? "!" : "");
		      break;
		    case iSDateS4: case iSDateTimeS4: case iSDateTimeS424:
		      if(v)
			snprintf(str, str_len, "%s.%s.%s%s",
				 yearzero, monzero, dayzero,
				 diff < 0 ? "!" : "");
		      else
			snprintf(str, str_len, "%s.%s.%s%s",
				 yearzero, monzero, dayzero,
				 diff < 0 ? "!" : "");
		      break;
		    case iSDateIsoS: case iSDateTimeIsoS: case iSDateTimeIsoS24:
		      snprintf(str, str_len, "%2s-%2s-%2s",
			    yearzero, monzero, dayzero);
		      break;
		    case iSDateIso: case iSDateTimeIso: case iSDateTimeIso24:
		      snprintf(str, str_len, "%4s-%2s-%2s",
			    year4, monzero, dayzero);
		      break;
		    default:
		      break;
		  }
	      }

	  }
	  else{
	      if(v)
		snprintf(str, str_len, "%s%s%s", monabb,
			(monabb[0] && day[0]) ? " " : "", day);
	      else{
		  if(monabb_width > 0)
		    utf8_snprintf(str, str_len, "%-*.*w %2s",
				  monabb_width, monabb_width, monabb, day);
		  else
		    snprintf(str, str_len, "%s %2s", monabb, day);
	      }
	  }
	}

	break;

	default:
	  break;
    }

    str[str_len-1] = '\0';
    
    if(type == iSTime ||
       (sdatetimetype && !strcmp(str, _(TODAYSTR)))){
	struct date now, last_day;
	char        dbuf[200], *Ddd, *ampm;
	int         daydiff;

	str[0] = '\0';
	rfc822_date(dbuf);
	parse_date(dbuf, &now);

	/* Figure out if message date lands in the past week */

	/* (if message dated this month or last month...) */
	if((d.year == now.year && d.month >= now.month - 1) ||
	   (d.year == now.year - 1 && d.month == 12 && now.month == 1)){

	    daydiff = day_of_year(&now) - day_of_year(&d);

	    /*
	     * If msg in end of last year (and we're in first bit of "this"
	     * year), diff will be backwards; fix up by adding number of days
	     * in last year (usually 365, but occasionally 366)...
	     */
	    if(d.year == now.year - 1){
		last_day = d;
		last_day.month = 12;
		last_day.day   = 31;

		daydiff += day_of_year(&last_day);
	    }
	}
	else
	  daydiff = -100;	/* comfortably out of range (of past week) */

	/* Build 2-digit hour and am/pm indicator, used below */

	if(d.hour >= 0 && d.hour < 24){
	    snprintf(hour12, sizeof(hour12), "%02d", (d.hour % 12 == 0) ? 12 : d.hour % 12);
	    ampm = (d.hour < 12) ? "am" : "pm";
	    snprintf(hour24, sizeof(hour24), "%02d", d.hour);
	}
	else{
	    strncpy(hour12, "??", sizeof(hour12));
	    hour12[sizeof(hour12)-1] = '\0';
	    ampm = "__";
	    strncpy(hour24, "??", sizeof(hour24));
	    hour24[sizeof(hour24)-1] = '\0';
	}

	/* Build date/time in str, in format similar to that used by w(1) */

	if(daydiff == 0){		    /* If date is today, "HH:MMap" */
	    if(d.minute >= 0 && d.minute < 60)
	      snprintf(minzero, sizeof(minzero), "%02d", d.minute);
	    else{
	      strncpy(minzero, "??", sizeof(minzero));
	      minzero[sizeof(minzero)-1] = '\0';
	    }

	    snprintf(str, str_len, "%s:%s%s", sdatetime24type ? hour24 : hour12,
		     minzero, sdatetime24type ? "" : ampm);
	}
	else if(daydiff >= 1 && daydiff < 6){ /* If <1wk ago, "DddHHap" */

	    if(d.month >= 1 && d.day >= 1 && d.year >= 0 &&
	       d.month <= 12 && d.day <= 31 && d.year <= 9999)
	      Ddd = day_abbrev_locale(day_of_week(&d));
	    else
	      Ddd = "???";

	    snprintf(str, str_len, "%s%s%s", Ddd, hour12, ampm);
	}
	else{		       /* date is old or future, "ddMmmyy" */
	    strncpy(monabb, (d.month >= 1 && d.month <= 12)
			     ? month_abbrev_locale(d.month) : "???", sizeof(monabb));
	    monabb[sizeof(monabb)-1] = '\0';

	    if(d.day >= 1 && d.day <= 31)
	      snprintf(dayzero, sizeof(dayzero), "%02d", d.day);
	    else{
	      strncpy(dayzero, "??", sizeof(dayzero));
	      dayzero[sizeof(dayzero)-1] = '\0';
	    }

	    if(d.year >= 0 && d.year <= 9999)
	      snprintf(yearzero, sizeof(yearzero), "%02d", d.year % 100);
	    else{
	      strncpy(yearzero, "??", sizeof(yearzero));
	      yearzero[sizeof(yearzero)-1] = '\0';
	    }

	    snprintf(str, str_len, "%s%s%s", dayzero, monabb, yearzero);
	}

	if(str[0] == '0'){	/* leading 0 (date|hour) elided or blanked */
	    if(v)
	      memmove(str, str + 1, strlen(str));
	    else
	      str[0] = ' ';
	}
    }
}


/*
 * Format a string representing the keywords into ice.
 *
 * This needs to be done in UTF-8, which may be tricky since it isn't labelled.
 *
 * Args  idata -- which message?
 *      kwtype -- keywords or kw initials
 *         ice -- index cache entry for message
 */
void
key_str(INDEXDATA_S *idata, SubjKW kwtype, ICE_S *ice)
{
    int           firstone = 1;
    KEYWORD_S    *kw;
    char         *word;
    COLOR_PAIR   *color = NULL;
    SPEC_COLOR_S *sc = ps_global->kw_colors;
    IELEM_S      *ielem = NULL;
    IFIELD_S     *ourifield = NULL;

    if(ice && ice->ifield){
	/* move to last ifield, the one we're working */
	for(ourifield = ice->ifield;
	    ourifield && ourifield->next;
	    ourifield = ourifield->next)
	  ;
    }

    if(!ourifield)
      return;

    if(kwtype == KWInit){
      for(kw = ps_global->keywords; kw; kw = kw->next){
	if(user_flag_is_set(idata->stream, idata->rawno, kw->kw)){
	    word = (kw->nick && kw->nick[0]) ? kw->nick :
		     (kw->kw && kw->kw[0])     ? kw->kw : "";

	    /*
	     * Pick off the first initial. Since word is UTF-8 it may
	     * take more than one byte for the first initial.
	     */

	    if(word && word[0]){
		UCS ucs;
		unsigned long remaining_octets;
		unsigned char *inputp;

		remaining_octets = strlen(word);
		inputp = (unsigned char *) word;
		ucs = (UCS) utf8_get(&inputp, &remaining_octets);
		if(!(ucs & U8G_ERROR || ucs == UBOGON)){
		    ielem = new_ielem(&ourifield->ielem);
		    ielem->freedata = 1;
		    ielem->datalen = (unsigned) (inputp - (unsigned char *) word);
		    ielem->data = (char *) fs_get((ielem->datalen + 1) * sizeof(char));
		    strncpy(ielem->data, word, ielem->datalen);
		    ielem->data[ielem->datalen] = '\0';

		    if(pico_usingcolor()
		       && ((kw->nick && kw->nick[0]
			    && (color=hdr_color(kw->nick,NULL,sc)))
			   || (kw->kw && kw->kw[0]
			       && (color=hdr_color(kw->kw,NULL,sc))))){
			ielem->color = color;
			color = NULL;
		    }
		}
	    }

	    if(color)
	      free_color_pair(&color);
	}
      }
    }
    else if(kwtype == KW){
      for(kw = ps_global->keywords; kw; kw = kw->next){
	if(user_flag_is_set(idata->stream, idata->rawno, kw->kw)){

	    if(!firstone){
		ielem = new_ielem(&ourifield->ielem);
		ielem->freedata = 1;
		ielem->data = cpystr(" ");
		ielem->datalen = 1;
	    }

	    firstone = 0;

	    word = (kw->nick && kw->nick[0]) ? kw->nick :
		     (kw->kw && kw->kw[0])     ? kw->kw : "";

	    if(word[0]){
		ielem = new_ielem(&ourifield->ielem);
		ielem->freedata = 1;
		ielem->data = cpystr(word);
		ielem->datalen = strlen(word);

		if(pico_usingcolor()
		   && ((kw->nick && kw->nick[0]
			&& (color=hdr_color(kw->nick,NULL,sc)))
		       || (kw->kw && kw->kw[0]
			   && (color=hdr_color(kw->kw,NULL,sc))))){
		    ielem->color = color;
		    color = NULL;
		}
	    }

	    if(color)
	      free_color_pair(&color);
	}
      }
    }

    /*
     * If we're coloring some of the fields then add a dummy field
     * at the end that can soak up the rest of the space after the last
     * colored keyword. Otherwise, the last one's color will extend to
     * the end of the field.
     */
    if(pico_usingcolor()){
	ielem = new_ielem(&ourifield->ielem);
	ielem->freedata = 1;
	ielem->data = cpystr(" ");
	ielem->datalen = 1;
    }

    ourifield->leftadj = 1;
    set_ielem_widths_in_field(ourifield);
}


void
prio_str(INDEXDATA_S *idata, IndexColType ctype, ICE_S *ice)
{
    IFIELD_S     *ourifield = NULL;
    IELEM_S      *ielem = NULL;
    char         *hdrval;
    PRIORITY_S   *p;
    int           v;

    if(ice && ice->ifield){
	/* move to last ifield, the one we're working */
	for(ourifield = ice->ifield;
	    ourifield && ourifield->next;
	    ourifield = ourifield->next)
	  ;
    }

    if(!ourifield)
      return;

    hdrval = fetch_header(idata, PRIORITYNAME);

    if(hdrval && hdrval[0] && isdigit(hdrval[0])){
	v = atoi(hdrval);
	if(v >= 1 && v <= 5 && v != 3){

	    ielem = new_ielem(&ourifield->ielem);
	    ielem->freedata = 1;

	    switch(ctype){
	      case iPrio:
		ielem->data = (char *) fs_get(2 * sizeof(char));
		ielem->data[0] = hdrval[0];
		ielem->data[1] = '\0';
		break;

	      case iPrioAlpha:
		for(p = priorities; p && p->desc; p++)
		  if(p->val == v)
		    break;

		if(p && p->desc)
		  ielem->data = cpystr(p->desc);
		  
		break;

	      case iPrioBang:
		ielem->data = (char *) fs_get(2 * sizeof(char));
		ielem->data[0] = '\0';
		switch(v){
		  case 1: case 2:
		    ielem->data[0] = '!';
		    break;

		  case 4: case 5:
		    /*
		     * We could put a Unicode downarrow in here but
		     * we have no way of knowing if the user's font
		     * will have it (I think).
		     */
		    ielem->data[0] = 'v';
		    break;
		}

		ielem->data[1] = '\0';
	        
		break;
	  
	      default:
		panic("Unhandled case in prio_str");
		break;
	    }

	    if(!ielem->data)
	      ielem->data = cpystr("");

	    if(ielem && ielem->data)
	      ielem->datalen = strlen(ielem->data);

	    if((v == 1 || v == 2) && pico_usingcolor()
	       && ps_global->VAR_IND_HIPRI_FORE_COLOR
	       && ps_global->VAR_IND_HIPRI_BACK_COLOR){
		ielem->type = eTypeCol;
		ielem->freecolor = 1;
		ielem->color = new_color_pair(ps_global->VAR_IND_HIPRI_FORE_COLOR, ps_global->VAR_IND_HIPRI_BACK_COLOR);
	    }
	    else if((v == 4 || v == 5) && pico_usingcolor()
	       && ps_global->VAR_IND_LOPRI_FORE_COLOR
	       && ps_global->VAR_IND_LOPRI_BACK_COLOR){
		ielem->type = eTypeCol;
		ielem->freecolor = 1;
		ielem->color = new_color_pair(ps_global->VAR_IND_LOPRI_FORE_COLOR, ps_global->VAR_IND_LOPRI_BACK_COLOR);
	    }

	    ourifield->leftadj = 1;
	    set_ielem_widths_in_field(ourifield);
	}

	fs_give((void **) &hdrval);
    }
}


void
header_str(INDEXDATA_S *idata, HEADER_TOK_S *hdrtok, ICE_S *ice)
{
    IFIELD_S     *ourifield = NULL;
    IELEM_S      *ielem = NULL;
    char         *fieldval = NULL;

    if(ice && ice->ifield){
	/* move to last ifield, the one we're working */
	for(ourifield = ice->ifield;
	    ourifield && ourifield->next;
	    ourifield = ourifield->next)
	  ;
    }

    if(!ourifield)
      return;

    fieldval = get_fieldval(idata, hdrtok);

    if(fieldval){
	ielem = new_ielem(&ourifield->ielem);
	ielem->freedata = 1;
	ielem->data = fieldval;
	ielem->datalen = strlen(fieldval);
	fieldval = NULL;
	ourifield->leftadj = (hdrtok->adjustment == Left) ? 1 : 0;
    }

    set_ielem_widths_in_field(ourifield);
}


char *
get_fieldval(INDEXDATA_S *idata, HEADER_TOK_S *hdrtok)
{
    int           sep, fieldnum;
    char         *hdrval = NULL, *testval;
    char         *fieldval = NULL, *firstval;
    char         *retfieldval = NULL;

    if(!hdrtok)
      return(retfieldval);

    if(hdrtok && hdrtok->hdrname && hdrtok->hdrname[0])
      hdrval = fetch_header(idata, hdrtok ? hdrtok->hdrname : "");

    /* find start of fieldnum'th field */
    fieldval = hdrval;
    for(fieldnum = MAX(hdrtok->fieldnum-1, 0);
	fieldnum > 0 && fieldval && *fieldval; fieldnum--){

	firstval = NULL;
	for(sep = 0; sep < hdrtok->fieldsepcnt; sep++){
	    testval = hdrtok->fieldseps ? strchr(fieldval, hdrtok->fieldseps[sep]) : NULL;
	    if(testval && (!firstval || testval < firstval))
	      firstval = testval;
	}

	fieldval = firstval;
	if(fieldval && *fieldval)
	  fieldval++;
    }

    /* tie off end of field */
    if(fieldval && *fieldval && hdrtok->fieldnum > 0){
	firstval = NULL;
	for(sep = 0; sep < hdrtok->fieldsepcnt; sep++){
	    testval = hdrtok->fieldseps ? strchr(fieldval, hdrtok->fieldseps[sep]) : NULL;
	    if(testval && (!firstval || testval < firstval))
	      firstval = testval;
	}

	if(firstval)
	  *firstval = '\0';
    }

    if(!fieldval)
      fieldval = "";

    retfieldval = cpystr(fieldval);

    if(hdrval)
      fs_give((void **) &hdrval);

    return(retfieldval);
}


long
scorevalfrommsg(MAILSTREAM *stream, MsgNo rawno, HEADER_TOK_S *hdrtok, int no_fetch)
{
    INDEXDATA_S   idata;
    MESSAGECACHE *mc;
    char         *fieldval = NULL;
    long          retval = 0L;

    memset(&idata, 0, sizeof(INDEXDATA_S));
    idata.stream   = stream;
    idata.no_fetch = no_fetch;
    idata.msgno    = mn_raw2m(sp_msgmap(stream), rawno);
    idata.rawno    = rawno;
    if(stream && idata.rawno > 0L && idata.rawno <= stream->nmsgs
       && (mc = mail_elt(stream, idata.rawno))){
	idata.size = mc->rfc822_size;
	index_data_env(&idata, pine_mail_fetchenvelope(stream,idata.rawno));
    }
    else
      idata.bogus = 2;

    fieldval = get_fieldval(&idata, hdrtok);

    if(fieldval){
	retval = atol(fieldval);
	fs_give((void **) &fieldval);
    }

    return(retval);
}


/*
 * Put a string representing the subject into str. Idata tells us which
 * message we are referring to.
 *
 * This means we should ensure that all data ends up being UTF-8 data.
 * That covers the data in ice ielems and str.
 *
 * Args  idata -- which message?
 *         str -- destination buffer
 *     strsize -- size of str buffer
 *      kwtype -- prepend keywords or kw initials before the subject
 *     opening -- add first text from body of message if there's room
 *         ice -- index cache entry for message
 */
void
subj_str(INDEXDATA_S *idata, char *str, size_t strsize, SubjKW kwtype, int opening, ICE_S *ice)
{
    char          *subject, *origsubj, *origstr, *rawsubj, *sptr = NULL;
    char          *p, *border, *q = NULL, *free_subj = NULL;
    char	  *sp;
    size_t         len;
    int            width = -1;
    int            depth = 0, mult = 2;
    int            save;
    int            do_subj = 0, truncated_tree = 0;
    PINETHRD_S    *thd, *thdorig;
    IELEM_S       *ielem = NULL, *subjielem = NULL;
    IFIELD_S      *ourifield = NULL;

    if(strsize <= 0)
      return;

    /*
     * If we need the data at the start of the message and we're in
     * a c-client callback, defer the data lookup until later.
     */
    if(opening && idata->no_fetch){
	idata->bogus = 1;
	return;
    }

    if(ice && ice->ifield){
	/* move to last ifield, the one we're working on */
	for(ourifield = ice->ifield;
	    ourifield && ourifield->next;
	    ourifield = ourifield->next)
	  ;
    }

    str[0] = str[strsize-1] = '\0';
    origstr = str;
    rawsubj = fetch_subject(idata);
    if(!rawsubj)
      rawsubj = "";

    /*
     * Before we do anything else, decode the character set in the subject and
     * work with the result.
     */
    sp = (char *) rfc1522_decode_to_utf8((unsigned char *) tmp_20k_buf,
					 SIZEOF_20KBUF, rawsubj);

    len = strlen(sp);
    len += 100;			/* for possible charset, escaped characters */
    origsubj = fs_get((len+1) * sizeof(unsigned char));
    origsubj[0] = '\0';

    iutf8ncpy(origsubj, sp, len);

    origsubj[len] = '\0';
    removing_trailing_white_space(origsubj);

    /*
     * origsubj is the original subject but it has been decoded. We need
     * to free it at the end of this routine.
     */


    /*
     * prepend_keyword will put the keyword stuff before the subject
     * and split the subject up into its colored parts in subjielem.
     * Subjielem is a local ielem which will have to be fit into the
     * real ifield->ielem later. The print_format strings in subjielem will
     * not be filled in by prepend_keyword because of the fact that we
     * may have to adjust things for threading below.
     * We use subjielem in case we want to insert some threading information
     * at the front of the subject.
     */
    if(kwtype == KW || kwtype == KWInit){
	subject = prepend_keyword_subject(idata->stream, idata->rawno,
					  origsubj, kwtype,
					  ourifield ? &subjielem : NULL,
					  ps_global->VAR_KW_BRACES);
	free_subj = subject;
    }
    else{
	subject = origsubj;
	if(ourifield){
	    subjielem = new_ielem(&subjielem);
	    subjielem->type = eTypeCol;
	    subjielem->freedata = 1;
	    subjielem->data = cpystr(subject);
	    subjielem->datalen = strlen(subject);
	    if(pico_usingcolor()
	       && ps_global->VAR_IND_SUBJ_FORE_COLOR
	       && ps_global->VAR_IND_SUBJ_BACK_COLOR){
		subjielem->freecolor = 1;
		subjielem->color = new_color_pair(ps_global->VAR_IND_SUBJ_FORE_COLOR, ps_global->VAR_IND_SUBJ_BACK_COLOR);
	    }
	}
    }

    /*
     * This space is here so that if the subject does
     * not extend all the way to the end of the field then
     * we'll switch the color back and paint the rest of the
     * field in the Normal color or the index line color.
     */
    if(!opening){
	ielem = new_ielem(&subjielem);
	ielem->freedata = 1;
	ielem->data = cpystr(" ");
	ielem->datalen = 1;
    }

    if(!subject)
      subject = "";

    if(THREADING()
       && (ps_global->thread_disp_style == THREAD_STRUCT
	   || ps_global->thread_disp_style == THREAD_MUTTLIKE
	   || ps_global->thread_disp_style == THREAD_INDENT_SUBJ1
	   || ps_global->thread_disp_style == THREAD_INDENT_SUBJ2)){

	/*
	 * Why do we want to truncate the subject and from strs?
	 * It's so we can put the [5] thread count things in below when
	 * we are threading and the thread structure runs off the right
	 * hand edge of the screen. This routine doesn't know that it
	 * is running off the edge unless it knows the actual width
	 * that we have to draw in.
	 */
	if(pith_opt_truncate_sfstr
	   && (*pith_opt_truncate_sfstr)()
	   && ourifield
	   && ourifield->width > 0)
	  width = ourifield->width;

	if(width < 0)
	  width = strsize-1;

	width = MIN(width, strsize-1);

	/*
	 * We're counting on the fact that this initial part of the
	 * string is ascii and we have one octet per character and
	 * characters are width 1 on the screen.
	 */
	border = str + width;

	thdorig = thd = fetch_thread(idata->stream, idata->rawno);

	if(pith_opt_condense_thread_cue)
	  width = (*pith_opt_condense_thread_cue)(thd, ice, &str, &strsize, width,
						  thd && thd->next
						  && get_lflag(idata->stream,
							       NULL,idata->rawno,
							       MN_COLL));

	/*
	 * width is < available strsize and
	 * border points to something less than or equal
	 * to the end of the buffer.
	 */

	sptr = str;

	if(thd)
	  while(thd->parent &&
		(thd = fetch_thread(idata->stream, thd->parent)))
	    depth++;

	if(depth > 0){
	    if(ps_global->thread_disp_style == THREAD_INDENT_SUBJ1)
	      mult = 1;

	    sptr += (mult*depth);
	    for(thd = thdorig, p = str + mult*depth - mult;
		thd && thd->parent && p >= str;
		thd = fetch_thread(idata->stream, thd->parent), p -= mult){
		if(p + mult >= border && !q){
		    if(width >= 4 && depth < 100){
			snprintf(str, width+1, "%*s[%2d]", width-4, "", depth);
			q = str + width-4;
		    }
		    else if(width >= 5 && depth < 1000){
			snprintf(str, width+1, "%*s[%3d]", width-5, "", depth);
			q = str + width-5;
		    }
		    else{
			snprintf(str, width+1, "%s", repeat_char(width, '.'));
			q = str;
		    }

		    border = q;
		    truncated_tree++;
		}

		if(p < border){
		    p[0] = ' ';
		    if(p + 1 < border)
		      p[1] = ' ';

		    if(ps_global->thread_disp_style == THREAD_STRUCT
		       || ps_global->thread_disp_style == THREAD_MUTTLIKE){
    /*
     * WARNING!
     * There is an unwarranted assumption here that VAR_THREAD_LASTREPLY_CHAR[0]
     * is ascii.
     */
			if(thd == thdorig && !thd->branch)
			  p[0] = ps_global->VAR_THREAD_LASTREPLY_CHAR[0];
			else if(thd == thdorig || thd->branch)
			  p[0] = '|';

			if(p + 1 < border && thd == thdorig)
			  p[1] = '-';
		    }
		}
	    }
	}

	if(sptr && !truncated_tree){
	    /*
	     * Look to see if the subject is the same as the previous
	     * message in the thread, if any. If it is the same, don't
	     * reprint the subject.
	     *
	     * Note that when we're prepending keywords to the subject,
	     * and the user changes a keyword, we do invalidate
	     * the index cache for that message but we don't go to the
	     * trouble of invalidating the index cache for the the child
	     * of that node in the thread, so the MUTT subject line
	     * display for the child may be wrong. That is, it may show
	     * it is the same as this subject even though it no longer
	     * is, or vice versa.
	     */
	    if(ps_global->thread_disp_style == THREAD_MUTTLIKE){
		if(depth == 0)
		  do_subj++;
		else{
		    if(thdorig->parent &&
		       (thd = fetch_thread(idata->stream, thdorig->parent))
		       && thd->rawno){
			char       *this_orig = NULL,
				   *prev_orig = NULL,
				   *free_prev_orig = NULL,
				   *this_prep = NULL,  /* includes prepend */
				   *prev_prep = NULL;
			ENVELOPE   *env;
			mailcache_t mc;
			SORTCACHE  *sc = NULL;

			/* get the stripped subject of previous message */
			mc = (mailcache_t) mail_parameters(NIL, GET_CACHE, NIL);
			if(mc)
			  sc = (*mc)(idata->stream, thd->rawno, CH_SORTCACHE);
			
			if(sc && sc->subject)
			  prev_orig = sc->subject;
			else{
			    char *stripthis;

			    env = pine_mail_fetchenvelope(idata->stream,
							  thd->rawno);
			    stripthis = (env && env->subject)
						    ? env->subject : "";

			    mail_strip_subject(stripthis, &prev_orig);
			    
			    free_prev_orig = prev_orig;
			}

			mail_strip_subject(rawsubj, &this_orig);

			if(kwtype == KW || kwtype == KWInit){
			    prev_prep = prepend_keyword_subject(idata->stream,
								thd->rawno,
								prev_orig,
								kwtype, NULL,
						    ps_global->VAR_KW_BRACES);

			    this_prep = prepend_keyword_subject(idata->stream,
								idata->rawno,
								this_orig,
								kwtype, NULL,
						    ps_global->VAR_KW_BRACES);

			    if((this_prep || prev_prep)
			       && ((this_prep && !prev_prep)
				   || (prev_prep && !this_prep)
				   || strucmp(this_prep, prev_prep)))
			      do_subj++;
			}
			else{
			    if((this_orig || prev_orig)
			       && ((this_orig && !prev_orig)
				   || (prev_orig && !this_orig)
				   || strucmp(this_orig, prev_orig)))
			      do_subj++;
			}

			/*
			 * If some of the thread is zoomed out of view, we
			 * want to display the subject of the first one that
			 * is in view. If any of the parents or grandparents
			 * etc of this message are visible, then we don't
			 * need to worry about it. If all of the parents have
			 * been zoomed away, then this is the first one.
			 *
			 * When you're looking at a particular case where
			 * some of the messages of a thread are selected it
			 * seems like we should look at not only our
			 * direct parents, but the siblings of the parent
			 * too. But that's not really correct, because those
			 * siblings are basically the starts of different
			 * branches, separate from our branch. They could
			 * have their own subjects, for example. This will
			 * give us cases where it looks like we are showing
			 * the subject too much, but it will be correct!
			 *
			 * In zoom_index() we clear_index_cache_ent for
			 * some lines which have subjects which might become
			 * visible when we zoom, and also in set_lflags
			 * where we might change subjects by unselecting
			 * something when zoomed.
			 */
			if(!do_subj){
			    while(thd){
				if(!msgline_hidden(idata->stream,
					     sp_msgmap(idata->stream),
					     mn_raw2m(sp_msgmap(idata->stream),
						      (long) thd->rawno),
					     0)){
				    break;	/* found a visible parent */
				}

				if(thd && thd->parent)
				  thd = fetch_thread(idata->stream,thd->parent);
				else
				  thd = NULL;
			    }

			    if(!thd)		/* none were visible */
			      do_subj++;
			}

			if(this_orig)
			  fs_give((void **) &this_orig);

			if(this_prep)
			  fs_give((void **) &this_prep);

			if(free_prev_orig)
			  fs_give((void **) &free_prev_orig);

			if(prev_prep)
			  fs_give((void **) &prev_prep);
		    }
		    else
		      do_subj++;
		}
	    }
	    else
	      do_subj++;

	    if(do_subj){
		/*
		 * We don't need to worry about truncating to width
		 * here. If we go over the right hand edge it will be
		 * truncated.
		 */
		strsize -= (sptr - str);

		strncpy(sptr, subject, strsize-1);
		sptr[strsize-1] = '\0';
	    }
	    else if(ps_global->thread_disp_style == THREAD_MUTTLIKE){
		strsize -= (sptr - str);

		if(strsize > 0){
		    sptr[0] = '>';
		    sptr++;
		}

		/*
		 * We decided we don't need the subject so we'd better
		 * eliminate subjielem.
		 */
		free_ielem(&subjielem);
	    }
	}
	else
	  free_ielem(&subjielem);	/* no room for actual subject */

	if(ourifield && sptr && sptr > origstr){
	    ielem = new_ielem(&ourifield->ielem);
	    ielem->type = eThreadInfo;
	    ielem->freedata = 1;
	    save = *sptr;
	    *sptr = '\0';
	    ielem->data = cpystr(origstr);
	    ielem->datalen = strlen(origstr);
	    *sptr = save;
	}
    }
    else{
	/*
	 * Not much to do for the non-threading case. Just copy the
	 * subject we have so far into str and truncate it.
	 */
	strncpy(str, subject, strsize-1);
	str[strsize-1] = '\0';
    }

    if(ourifield){
	/*
	 * We need to add subjielem to the end of the ourifield->ielem list.
	 */
	if(subjielem){
	    if(ourifield->ielem){
		for(ielem = ourifield->ielem;
		    ielem && ielem->next; ielem = ielem->next)
		  ;
	        
		ielem->next = subjielem;
	    }
	    else
		ourifield->ielem = subjielem;
	}

	ourifield->leftadj = 1;
    }

    if(opening && ourifield){
	IELEM_S *ftielem = NULL;
	size_t len;
	char *first_text;

	first_text = fetch_firsttext(idata, 0);

	if(first_text){
	    char sep[200];
	    int   seplen;

	    strncpy(sep, ps_global->VAR_OPENING_SEP ? ps_global->VAR_OPENING_SEP : " - ",
		    sizeof(sep));
	    sep[sizeof(sep)-1] = '\0';
	    removing_double_quotes(sep);
	    seplen = strlen(sep);

	    ftielem = new_ielem(&ftielem);
	    ftielem->type = eTypeCol;
	    ftielem->freedata = 1;
	    len = strlen(first_text) + seplen;
	    ftielem->data = (char *) fs_get((len + 1) * sizeof(char));

	    strncpy(ftielem->data, sep, seplen);
	    strncpy(ftielem->data+seplen, first_text, len+1-seplen);
	    ftielem->data[len] = '\0';

	    ftielem->datalen = strlen(ftielem->data);
	    if(first_text)
	      fs_give((void **) &first_text);

	    if(ftielem){
		if(pico_usingcolor()
		   && ps_global->VAR_IND_OP_FORE_COLOR
		   && ps_global->VAR_IND_OP_BACK_COLOR){
		    ftielem->freecolor = 1;
		    ftielem->color = new_color_pair(ps_global->VAR_IND_OP_FORE_COLOR, ps_global->VAR_IND_OP_BACK_COLOR);

		    /*
		     * This space is here so that if the opening text does
		     * not extend all the way to the end of the field then
		     * we'll switch the color back and paint the rest of the
		     * field in the Normal color or the index line color.
		     */
		    ielem = new_ielem(&ftielem);
		    ielem->freedata = 1;
		    ielem->data = cpystr(" ");
		    ielem->datalen = 1;
		}

		if(ourifield->ielem){
		    for(ielem = ourifield->ielem;
			ielem && ielem->next; ielem = ielem->next)
		      ;
		    
		    ielem->next = ftielem;
		}
		else
		    ourifield->ielem = ftielem;
	    }

	    ourifield->leftadj = 1;
	}
    }

    if(ourifield)
      set_ielem_widths_in_field(ourifield);

    if(origsubj)
      fs_give((void **) &origsubj);

    if(free_subj)
      fs_give((void **) &free_subj);
}


/*
 * Returns an allocated string which is the passed in subject with a
 * list of keywords prepended.
 *
 * If kwtype == KW you will end up with
 *
 *     {keyword1 keyword2} subject
 *
 * (actually, keyword nicknames will be used instead of the actual keywords
 *  in the case that the user defined nicknames)
 *
 * If kwtype == KWInit you get
 *
 *     {AB} subject
 *
 * where A is the first letter of the first keyword and B is the first letter
 * of the second defined keyword. No space between them. There could be more
 * than two.
 *
 * If an ielemp is passed in it will be filled out with the data and colors
 * of the pieces of the subject but the print_format strings will not
 * be set.
 */
char *
prepend_keyword_subject(MAILSTREAM *stream, long int rawno, char *subject,
			SubjKW kwtype, IELEM_S **ielemp, char *braces)
{
    char         *p, *next_piece, *retsubj = NULL, *str;
    char         *left_brace = NULL, *right_brace = NULL;
    size_t        len;
    int           some_set = 0, save;
    IELEM_S      *ielem;
    KEYWORD_S    *kw;
    COLOR_PAIR   *color = NULL;
    SPEC_COLOR_S *sc = ps_global->kw_colors;

    if(!subject)
      subject = "";

    if(braces && *braces)
      get_pair(braces, &left_brace, &right_brace, 1, 0);

    len = (left_brace ? strlen(left_brace) : 0) +
            (right_brace ? strlen(right_brace) : 0);

    if(stream && rawno >= 0L && rawno <= stream->nmsgs){
	for(kw = ps_global->keywords; kw; kw = kw->next)
	  if(user_flag_is_set(stream, rawno, kw->kw)){
	      if(kwtype == KW){
		  if(some_set)
		    len++;		/* space between keywords */

		  str = kw->nick ? kw->nick : kw->kw ? kw->kw : "";
		  len += strlen(str);
	      }
	      else if(kwtype == KWInit){
		  str = kw->nick ? kw->nick : kw->kw ? kw->kw : "";
		  /* interested in only the first UTF-8 initial */
		  if(str && str[0]){
		    UCS ucs;
		    unsigned long remaining_octets;
		    unsigned char *inputp;

		    remaining_octets = strlen(str);
		    inputp = (unsigned char *) str;
		    ucs = (UCS) utf8_get(&inputp, &remaining_octets);
		    if(!(ucs & U8G_ERROR || ucs == UBOGON)){
			len += (unsigned) (inputp - (unsigned char *) str);
		    }
		  }
	      }

	      some_set++;
	  }
    }

    if((kwtype == KW || kwtype == KWInit) && some_set){
	len += strlen(subject);		/* subject is already UTF-8 if needed */
	retsubj = (char *) fs_get((len + 1) * sizeof(*retsubj));
	memset(retsubj, 0, (len + 1) * sizeof(*retsubj));
	next_piece = p = retsubj;

	for(kw = ps_global->keywords; kw; kw = kw->next){
	    if(user_flag_is_set(stream, rawno, kw->kw)){
		if(p == retsubj){
		    if(left_brace && len > 0)
		      sstrncpy(&p, left_brace, len);
		}
		else if(kwtype == KW)
		  *p++ = ' ';
		
		if(ielemp && p > next_piece){
		    save = *p;
		    *p = '\0';
		    ielem = new_ielem(ielemp);
		    ielem->freedata = 1;
		    ielem->data = cpystr(next_piece);
		    ielem->datalen = strlen(next_piece);
		    *p = save;
		    next_piece = p;
		}

		str = kw->nick ? kw->nick : kw->kw ? kw->kw : "";

		if(kwtype == KWInit){
		  if(str && str[0]){
		    UCS ucs;
		    unsigned long remaining_octets;
		    unsigned char *inputp;

		    remaining_octets = strlen(str);
		    inputp = (unsigned char *) str;
		    ucs = (UCS) utf8_get(&inputp, &remaining_octets);
		    if(!(ucs & U8G_ERROR || ucs == UBOGON)){
			if(len-(p-retsubj) > 0){
			    sstrncpy(&p, str, MIN(inputp - (unsigned char *) str,len-(p-retsubj)));
			    if(p > next_piece && ielemp && pico_usingcolor()
			       && ((kw->nick && kw->nick[0]
				    && (color=hdr_color(kw->nick,NULL,sc)))
				   || (kw->kw && kw->kw[0]
				       && (color=hdr_color(kw->kw,NULL,sc))))){
				ielem = new_ielem(ielemp);
				ielem->freedata = 1;
				save = *p;
				*p = '\0';
				ielem->data = cpystr(next_piece);
				ielem->datalen = strlen(next_piece);
				ielem->color = color;
				color = NULL;
				*p = save;
				next_piece = p;
			    }
			}
		    }

		    if(color)
		      free_color_pair(&color);
		  }
		}
		else{
		    if(len-(p-retsubj) > 0)
		      sstrncpy(&p, str, len-(p-retsubj));

		    if(p > next_piece && ielemp && pico_usingcolor()
		       && ((kw->nick && kw->nick[0]
			    && (color=hdr_color(kw->nick,NULL,sc)))
			   || (kw->kw && kw->kw[0]
			       && (color=hdr_color(kw->kw,NULL,sc))))){
			ielem = new_ielem(ielemp);
			ielem->freedata = 1;
			save = *p;
			*p = '\0';
			ielem->data = cpystr(next_piece);
			ielem->datalen = strlen(next_piece);
			ielem->color = color;
			color = NULL;
			*p = save;
			next_piece = p;
		    }

		    if(color)
		      free_color_pair(&color);
		}
	    }
	}

	if(len-(p-retsubj) > 0 && right_brace)
	  sstrncpy(&p, right_brace, len-(p-retsubj));

	if(ielemp && p > next_piece){
	    save = *p;
	    *p = '\0';
	    ielem = new_ielem(ielemp);
	    ielem->freedata = 1;
	    ielem->data = cpystr(next_piece);
	    ielem->datalen = strlen(next_piece);
	    *p = save;
	    next_piece = p;
	}

	if(len-(p-retsubj) > 0 && subject)
	  sstrncpy(&p, subject, len-(p-retsubj));

	if(ielemp && p > next_piece){
	    save = *p;
	    *p = '\0';
	    ielem = new_ielem(ielemp);
	    ielem->type = eTypeCol;
	    ielem->freedata = 1;
	    ielem->data = cpystr(next_piece);
	    ielem->datalen = strlen(next_piece);
	    *p = save;
	    next_piece = p;
	    if(pico_usingcolor()
	       && ps_global->VAR_IND_SUBJ_FORE_COLOR
	       && ps_global->VAR_IND_SUBJ_BACK_COLOR){
		ielem->freecolor = 1;
		ielem->color = new_color_pair(ps_global->VAR_IND_SUBJ_FORE_COLOR, ps_global->VAR_IND_SUBJ_BACK_COLOR);
	    }
	}

	retsubj[len] = '\0';		/* just making sure */
    }
    else{
	if(ielemp){
	    ielem = new_ielem(ielemp);
	    ielem->type = eTypeCol;
	    ielem->freedata = 1;
	    ielem->data = cpystr(subject);
	    ielem->datalen = strlen(subject);
	    if(pico_usingcolor()
	       && ps_global->VAR_IND_SUBJ_FORE_COLOR
	       && ps_global->VAR_IND_SUBJ_BACK_COLOR){
		ielem->freecolor = 1;
		ielem->color = new_color_pair(ps_global->VAR_IND_SUBJ_FORE_COLOR, ps_global->VAR_IND_SUBJ_BACK_COLOR);
	    }
	}

	retsubj = cpystr(subject);
    }

    if(braces){
	if(left_brace)
	  fs_give((void **) &left_brace);

	if(right_brace)
	  fs_give((void **) &right_brace);
    }

    return(retsubj);
}


/*
 * This means we should ensure that all data ends up being UTF-8 data.
 * That covers the data in ice ielems and str.
 */
void
from_str(IndexColType ctype, INDEXDATA_S *idata, char *str, size_t strsize, ICE_S *ice)
{
    char       *field, *newsgroups, *border, *p, *fptr = NULL, *q = NULL;
    ADDRESS    *addr;
    int         width = -1;
    int         depth = 0, mult = 2;
    PINETHRD_S *thd, *thdorig;

    if(THREADING()
       && (ps_global->thread_disp_style == THREAD_INDENT_FROM1
           || ps_global->thread_disp_style == THREAD_INDENT_FROM2
           || ps_global->thread_disp_style == THREAD_STRUCT_FROM)){

	if(pith_opt_truncate_sfstr && (*pith_opt_truncate_sfstr)()){
	    IFIELD_S   *ourifield = NULL;

	    if(ice && ice->ifield){
		/* move to last ifield, the one we're working on */
		for(ourifield = ice->ifield;
		    ourifield && ourifield->next;
		    ourifield = ourifield->next)
		  ;
	    }

	    if(ourifield && ourifield->width > 0)
	      width = ourifield->width;
	}

	if(width < 0)
	  width = strsize-1;

	width = MIN(width, strsize-1);

	thdorig = thd = fetch_thread(idata->stream, idata->rawno);
	border = str + width;
	if(pith_opt_condense_thread_cue)
	  width = (*pith_opt_condense_thread_cue)(thd, ice, &str, &strsize, width,
						  thd && thd->next
						  && get_lflag(idata->stream,
							       NULL,idata->rawno,
							       MN_COLL));

	fptr = str;

	if(thd)
	  while(thd->parent && (thd = fetch_thread(idata->stream, thd->parent)))
	    depth++;

	if(depth > 0){
	    if(ps_global->thread_disp_style == THREAD_INDENT_FROM1)
	      mult = 1;

	    fptr += (mult*depth);
	    for(thd = thdorig, p = str + mult*depth - mult;
		thd && thd->parent && p >= str;
		thd = fetch_thread(idata->stream, thd->parent), p -= mult){
		if(p + mult >= border && !q){
		    if(width >= 4 && depth < 100){
			snprintf(str, width+1, "%*s[%2d]", width-4, "", depth);
			q = str + width-4;
		    }
		    else if(width >= 5 && depth < 1000){
			snprintf(str, width+1, "%*s[%3d]", width-5, "", depth);
			q = str + width-5;
		    }
		    else{
			snprintf(str, width+1, "%s", repeat_char(width, '.'));
			q = str;
		    }

		    border = q;
		    fptr = NULL;
		}

		if(p + 1 < border){
		    p[0] = p[1] = ' ';
		    if(ps_global->thread_disp_style == THREAD_STRUCT_FROM){
    /*
     * WARNING!
     * There is an unwarranted assumption here that VAR_THREAD_LASTREPLY_CHAR[0]
     * is ascii.
     */
			if(thd == thdorig && !thd->branch)
			  p[0] = ps_global->VAR_THREAD_LASTREPLY_CHAR[0];
			else if(thd == thdorig || thd->branch)
			  p[0] = '|';

			if(thd == thdorig)
			  p[1] = '-';
		    }
		}
		else if(p < border){
		    p[0] = ' ';
		    if(ps_global->thread_disp_style == THREAD_STRUCT_FROM){
    /*
     * WARNING!
     * There is an unwarranted assumption here that VAR_THREAD_LASTREPLY_CHAR[0]
     * is ascii.
     */
			if(thd == thdorig && !thd->branch)
			  p[0] = ps_global->VAR_THREAD_LASTREPLY_CHAR[0];
			else if(thd == thdorig || thd->branch)
			  p[0] = '|';
		    }
		}
	    }
	}
    }
    else
      fptr = str;

    if(fptr){
	strsize -=  (fptr - str);
	switch(ctype){
	  case iFromTo:
	  case iFromToNotNews:
	    if(!(addr = fetch_from(idata)) || address_is_us(addr, ps_global)){
		if(strsize-1 <= 4){
		    strncpy(fptr, "To: ", strsize-1);
		    fptr[strsize-1] = '\0';
		    break;
		}
		else{
		    if((field = ((addr = fetch_to(idata))
				 ? "To"
				 : (addr = fetch_cc(idata))
				 ? "Cc"
				 : NULL))
		       && set_index_addr(idata, field, addr, "To: ",
					 strsize-1, fptr))
		      break;

		    if(ctype == iFromTo &&
		       (newsgroups = fetch_newsgroups(idata)) &&
		       *newsgroups){
			snprintf(fptr, strsize, "To: %-*.*s", strsize-1-4, strsize-1-4,
				newsgroups);
			break;
		    }

		    /* else fall thru to From: */
		}
	    }
	    /* else fall thru to From: */

	    if(idata->bogus)
	      break;

	  case iFrom:
	    set_index_addr(idata, "From", fetch_from(idata), NULL, strsize-1, fptr);
	    break;

	  case iAddress:
	  case iMailbox:
	    if((addr = fetch_from(idata)) && addr->mailbox && addr->mailbox[0]){
		char *mb = NULL, *hst = NULL, *at = NULL;
		size_t len;

		mb = addr->mailbox;
		if(ctype == iAddress && addr->host && addr->host[0]
		   && addr->host[0] != '.'){
		    at = "@";
		    hst = addr->host;
		}

		len = strlen(mb);
		if(!at || strsize-1 <= len)
		  snprintf(fptr, strsize, "%-*.*s", strsize-1, strsize-1, mb);
		else
		  snprintf(fptr, strsize, "%s@%-*.*s", mb, strsize-1-len-1, strsize-1-len-1, hst);
	    }

	    break;

	  default:
	    break;
	}
    }
}


/*
 * Set up the elements contained in field so that they take up the
 * whole field width. Data is assumed to be UTF-8.
 */
void
set_ielem_widths_in_field(IFIELD_S *ifield)
{
    IELEM_S *ielem = NULL;
    int      datawidth, fmtwidth;

    if(!ifield)
      return;

    fmtwidth = ifield->width;

    for(ielem = ifield->ielem; ielem && fmtwidth > 0; ielem = ielem->next){
	if(!ifield->leftadj && ielem->next){
	    dprint((1, "set_ielem_widths_in_field(%d): right adjust with multiple elements, NOT SUPPOSED TO HAPPEN!\n", (int) ifield->ctype));
	    assert(0);
	}

	datawidth = (int) utf8_width(ielem->data);
	if(datawidth >= fmtwidth || !ielem->next){
	    set_print_format(ielem, fmtwidth, ifield->leftadj);
	    fmtwidth = 0;
	}
	else{
	    set_print_format(ielem, datawidth, ifield->leftadj);
	    fmtwidth -= datawidth;
	}
    }
}


/*
 * Simple hash function from K&R 2nd edition, p. 144.
 *
 * This one is modified to never return 0 so we can use that as a special
 * value. Also, LINE_HASH_N fits in an unsigned long, so it too can be used
 * as a special value that can't be returned by line_hash.
 */
unsigned long
line_hash(char *s)
{
    unsigned long hashval;

    for(hashval = 0; *s != '\0'; s++)
      hashval = *s + 31 * hashval;

    hashval = hashval % LINE_HASH_N;

    if(!hashval)
      hashval++;

    return(hashval);
}


/*
 * Returns nonzero if considered hidden, 0 if not considered hidden.
 */
int
msgline_hidden(MAILSTREAM *stream, MSGNO_S *msgmap, long int msgno, int flags)
{
    int ret;

    if(flags & MH_ANYTHD){
	ret = ((any_lflagged(msgmap, MN_HIDE) > 0)
	       && get_lflag(stream, msgmap, msgno, MN_HIDE));
    }
    else if(flags & MH_THISTHD && THREADING() && sp_viewing_a_thread(stream)){
	ret = (get_lflag(stream, msgmap, msgno, MN_HIDE)
	       || !get_lflag(stream, msgmap, msgno, MN_CHID2));
    }
    else{
	if(THREADING() && sp_viewing_a_thread(stream)){
	    ret = (get_lflag(stream, msgmap, msgno, MN_HIDE)
		   || !get_lflag(stream, msgmap, msgno, MN_CHID2)
		   || get_lflag(stream, msgmap, msgno, MN_CHID));
	}
	else if(THRD_INDX()){
	    /*
	     * If this message is in the collapsed part of a thread,
	     * it's hidden. It must be a top-level of a thread to be
	     * considered visible. Even if it is top-level, it is only
	     * visible if some message in the thread is not hidden.
	     */
	    if(get_lflag(stream, msgmap, msgno, MN_CHID))	/* not top */
	      ret = 1;
	    else{
		unsigned long rawno;
		PINETHRD_S   *thrd = NULL;

		rawno = mn_m2raw(msgmap, msgno);
		if(rawno)
		  thrd = fetch_thread(stream, rawno);

		ret = !thread_has_some_visible(stream, thrd);
	    }
	}
	else{
	    ret = ((any_lflagged(msgmap, MN_HIDE | MN_CHID) > 0)
		   && get_lflag(stream, msgmap, msgno, MN_HIDE | MN_CHID));
	}
    }
    
    dprint((10,
	       "msgline_hidden(%ld): %s\n", msgno, ret ? "HID" : "VIS"));

    return(ret);
}


void
adjust_cur_to_visible(MAILSTREAM *stream, MSGNO_S *msgmap)
{
    long n, cur;
    int  dir;

    cur = mn_get_cur(msgmap);

    /* if current is hidden, adjust */
    if(cur >= 1L && cur <= mn_get_total(msgmap)
       && msgline_hidden(stream, msgmap, cur, 0)){

	dir = mn_get_revsort(msgmap) ? -1 : 1;

	for(n = cur;
	    ((dir == 1 && n >= 1L) || (dir == -1 && n <= mn_get_total(msgmap)))
	    && (n >= 1L && n <= mn_get_total(msgmap))
	    && msgline_hidden(stream, msgmap, n, 0);
	    n -= dir)
	  ;
	
	if(((dir == 1 && n >= 1L) || (dir == -1 && n <= mn_get_total(msgmap)))
	   && (n >= 1L && n <= mn_get_total(msgmap)))
	  mn_reset_cur(msgmap, n);
	else{				/* no visible in that direction */
	    for(n = cur;
		((dir == 1 && n >= 1L) || (dir == -1 && n <= mn_get_total(msgmap)))
		&& (n >= 1L && n <= mn_get_total(msgmap))
		&& msgline_hidden(stream, msgmap, n, 0);
		n += dir)
	      ;

	    if(((dir == 1 && n >= 1L) || (dir == -1 && n <= mn_get_total(msgmap)))
	       && (n >= 1L && n <= mn_get_total(msgmap)))
	      mn_reset_cur(msgmap, n);
	    /* else trouble! */
	}
    }
}


void
setup_for_index_index_screen(void)
{
    format_index_line = format_index_index_line;
    setup_header_widths = setup_index_header_widths;
}


void
setup_for_thread_index_screen(void)
{
    format_index_line = format_thread_index_line;
    setup_header_widths = setup_thread_header_widths;
}


unsigned long
ice_hash(ICE_S *ice)
{
    char buf[MAX_SCREEN_COLS+1];

    buf[0] = '\0';

    if(ice)
      simple_index_line(buf, sizeof(buf), ice, 0L);

    buf[sizeof(buf) - 1] = '\0';

    return(line_hash(buf));
}


char *
left_adjust(int width)
{
    return(format_str(width, 1));
}


char *
right_adjust(int width)
{
    return(format_str(width, 0));
}


/*
 * Returns allocated and filled in format string.
 */
char *
format_str(int width, int left)
{
    char  *format;
    size_t len;

    len = PRINT_FORMAT_LEN(width,left) * sizeof(char);
    format = (char *) fs_get(len + 1);
    copy_format_str(width, left, format, len);
    format[len] = '\0';

    return(format);
}


/*
 * Put the left or right adjusted format string of width width into
 * dest. Dest is of size n+1.
 */
char *
copy_format_str(int width, int left, char *dest, int n)
{
    char  *p;

    p = int2string(width);

    snprintf(dest, n+1, "%%%s%s.%ss", left ? "-" : "", p, p);

    dest[n] = '\0';

    return(dest);
}


/*
 * Sets up the print_format string to be width wide with left or right
 * adjust. Takes care of memory freeing and allocation.
 */
void
set_print_format(IELEM_S *ielem, int width, int leftadj)
{
    if(ielem){
	ielem->wid = width;

	if(ielem->print_format){
	    /* is there enough room? */
	    if(ielem->freeprintf < PRINT_FORMAT_LEN(width,leftadj)+1){
		fs_resize((void **) &ielem->print_format,
			  (PRINT_FORMAT_LEN(width,leftadj)+1) * sizeof(char));
		ielem->freeprintf = (PRINT_FORMAT_LEN(width,leftadj) + 1) * sizeof(char);
	    }

	    copy_format_str(width, leftadj, ielem->print_format,
			    PRINT_FORMAT_LEN(width,leftadj));
	}
	else{
	    ielem->print_format = leftadj ? left_adjust(width)
					  : right_adjust(width);
	    ielem->freeprintf = (PRINT_FORMAT_LEN(width,leftadj) + 1) * sizeof(char);
	}
    }
}
