/*
 * $Id: indxtype.h 1074 2008-06-04 00:08:43Z hubert@u.washington.edu $
 *
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

#ifndef PITH_INDXTYPE_INCLUDED
#define PITH_INDXTYPE_INCLUDED

#include "../pith/osdep/color.h"

#include "../pith/msgno.h"
#include "../pith/charset.h"


/*
 * Flags for msgline_hidden.
 *
 * MH_NONE means we should only consider messages which are or would be
 *   visible in the index view. That is, messages which are not hidden due
 *   to zooming, not hidden because they are in a collapsed part of a
 *   thread, and not hidden because we are in one thread and they are in
 *   another and the view we are using only shows one thread.
 * MH_THISTHD changes that a little bit. It considers more messages
 *   to be visible. In particular, messages in this thread which are
 *   hidden due to collapsing are considered to be visible instead of hidden.
 *   This is useful if we are viewing a message and hit Next, and want
 *   to see the next message in the thread even if it was in a
 *   collapsed thread. This only makes sense when using the separate
 *   thread index and viewing a thread. "This thread" is the thread that
 *   is being viewed, not the thread that msgno is part of. So it is the
 *   thread that has been marked MN_CHID2.
 * MH_ANYTHD adds more visibility. It doesn't matter if the message is in
 *   the same thread or if it is collapsed or not. If a message is not
 *   hidden due to zooming, then it is not hidden. Notice that ANYTHD
 *   implies THISTHD.
 */
#define MH_NONE		0x0
#define MH_THISTHD	0x1
#define MH_ANYTHD	0x2


typedef enum {iNothing, iStatus, iFStatus, iIStatus, iSIStatus,
	      iDate, iLDate, iS1Date, iS2Date, iS3Date, iS4Date, iSDate,
	      iDateIso, iDateIsoS,
	      iSDateIso, iSDateIsoS, iSDateS1, iSDateS2, iSDateS3, iSDateS4,
	      iSDateTime,
	      iSDateTimeIso, iSDateTimeIsoS, iSDateTimeS1,
	      iSDateTimeS2, iSDateTimeS3, iSDateTimeS4,
	      iSDateTime24,
	      iSDateTimeIso24, iSDateTimeIsoS24, iSDateTimeS124,
	      iSDateTimeS224, iSDateTimeS324, iSDateTimeS424,
	      iRDate, iTimezone,
	      iTime24, iTime12,
	      iCurDate, iCurDateIso, iCurDateIsoS, iCurTime24, iCurTime12,
	      iCurDay, iCurDay2Digit, iCurDayOfWeek, iCurDayOfWeekAbb,
	      iCurMon, iCurMon2Digit, iCurMonLong, iCurMonAbb,
	      iCurYear, iCurYear2Digit,
	      iLstMon, iLstMon2Digit, iLstMonLong, iLstMonAbb,
	      iLstMonYear, iLstMonYear2Digit,
	      iLstYear, iLstYear2Digit,
	      iMessNo, iAtt, iMsgID,
	      iSubject, iSubjKey, iSubjKeyInit,
	      iSubjectText, iSubjKeyText, iSubjKeyInitText,
	      iOpeningText, iOpeningTextNQ,
	      iKey, iKeyInit,
	      iPrefDate, iPrefTime, iPrefDateTime,
	      iCurPrefDate, iCurPrefTime, iCurPrefDateTime,
	      iSize, iSizeComma, iSizeNarrow, iDescripSize,
	      iNewsAndTo, iToAndNews, iNewsAndRecips, iRecipsAndNews,
	      iFromTo, iFromToNotNews, iFrom, iTo, iSender, iCc, iNews, iRecips,
	      iCurNews, iArrow,
	      iMailbox, iAddress, iInit, iCursorPos,
	      iDay2Digit, iMon2Digit, iYear2Digit,
	      iSTime, iKSize,
	      iRoleNick, iNewLine,
	      iHeader, iText,
	      iPrio, iPrioAlpha, iPrioBang,
	      iScore, iDayOfWeekAbb, iDayOfWeek,
	      iDay, iDayOrdinal, iMonAbb, iMonLong, iMon, iYear} IndexColType;

typedef enum {AllAuto, Fixed, Percent, WeCalculate, Special} WidthType;

typedef enum {eText = 0, eKeyWord, eThreadCount, eThreadInfo, eTypeCol} ElemType;

typedef enum {Left, Right} ColAdj;

typedef struct index_parse_tokens {
    char        *name;
    IndexColType ctype;
    int          what_for;
} INDEX_PARSE_T;


/* these are flags for the what_for field in INDEX_PARSE_T */
#define FOR_NOTHING	0x00
#define FOR_INDEX	0x01
#define FOR_REPLY_INTRO	0x02
#define FOR_TEMPLATE	0x04		/* or for signature */
#define FOR_FILT	0x08
#define DELIM_USCORE	0x10
#define DELIM_PAREN	0x20
#define DELIM_COLON	0x40


#define DEFAULT_REPLY_INTRO "default"


typedef struct hdr_token_description {
    char  *hdrname;
    int    fieldnum;
    int    fieldsepcnt;
    ColAdj adjustment;
    char  *fieldseps;
} HEADER_TOK_S;

typedef struct col_description {
    IndexColType  ctype;
    WidthType     wtype;
    int		  req_width;
    int		  width;
    int		  actual_length;
    int		  monabb_width;		/* hack */
    ColAdj	  adjustment;
    HEADER_TOK_S *hdrtok;
} INDEX_COL_S;


/*
 * Ensure that all the data in an index_elem (in the data member)
 * is stored in UTF-8. The writing routines (paint_index_line() for pine)
 * should then assume UTF-8 when using the data. The process of putting
 * data into an index_elem will likely involve a conversion into UTF-8.
 * For example, the Subject or From fields may have data in some arbitrary
 * character set.
 */
typedef struct index_elem {
    struct index_elem *next;
    char              *print_format;
    COLOR_PAIR        *color;		/* color, if any, for this element */
    char              *data;		/* ptr to full text for this element */
    unsigned int       datalen;		/* not counting terminating null */
    ElemType	       type;		/* part of field data represents */
    unsigned           freedata:1;	/* free data when done */
    unsigned           freecolor:1;	/* free color when done */
    unsigned           freeprintf:8;	/* how much alloced for print_format */
    unsigned           wid:16;		/* redundant, width from print_format */
} IELEM_S;


/*
 * 3 is room for '%', '.', and 's'. You need to add one for '\0';
 * The format string looks like:
 *
 *   %13.13s  or %-13.13s
 *
 * The '-' is there if left is set and the 13 is the 'width' in the call.
 */
#define PRINT_FORMAT_LEN(width,left) (3 + ((left) ? 1 : 0) + 2 * ((width) > 999 ? 4 : (width) > 99 ? 3 : (width) > 9 ? 2 : 1))


/*
 * Each field consists of a list of elements. The leftadj bit means we
 * should left adjust the whole thing in the field width, padding on the
 * right with spaces or truncating on the right. We haven't yet fully
 * implemented right adjust, except that it is easy for a single element
 * field. So be careful with multi-element, right-adjusted fields.
 */
typedef struct index_field {
    struct index_field *next;
    IndexColType        ctype;
    unsigned            width:16;	/* width of whole field           */
    unsigned            leftadj:1;	/* left adjust elements in field  */
    IELEM_S            *ielem;		/* list of elements in this field */
} IFIELD_S;


/*
 * If the index_cache_entry has an ifield list, it is assumed that the
 * ifields have had their ielement lists filled in.
 * The widths_done bit is set after the widths and print formats are setup
 * to do the correct width. A width change could be done by unsetting
 * the widths_done bit and then recalculating the widths in the ifields
 * and resetting the print_format strings in the ielems.
 */
typedef struct index_cache_entry {
    IFIELD_S     *ifield;		/* list of fields */
    COLOR_PAIR	 *linecolor;
    unsigned      color_lookup_done:1;	/* efficiency hacks */
    unsigned      widths_done:1;
    unsigned	  to_us:1;
    unsigned	  cc_us:1;
    int           plus;
    unsigned long id;			/* hash value */
    struct index_cache_entry *tice;	/* thread index header line */
} ICE_S;


/*
 * Pieces needed to construct a valid folder index entry, and to
 * control what can be fetched when (due to callbacks and such)
 */
typedef struct index_data {
    MAILSTREAM *stream;
    ADDRESS    *from,			/* always valid */
	       *to,			/* check valid bit, fetch as req'd */
	       *cc,			/* check valid bit, fetch as req'd */
	       *sender;			/* check valid bit, fetch as req'd */
    char       *newsgroups,		/* check valid bit, fetch as req'd */
	       *subject,		/* always valid */
	       *date;			/* always valid */
    long	msgno,			/* tells us what we're looking at */
		rawno,
		size;			/* always valid */
    unsigned	no_fetch:1,		/* lit when we're in a callback */
		bogus:2,		/* lit when there were problems */
		valid_to:1,		/* trust struct's "to" pointer */
		valid_cc:1,		/* trust struct's "cc" pointer */
		valid_sender:1,		/* trust struct's "sender" pointer */
		valid_news:1,		/* trust struct's "news" pointer */
		valid_resent_to:1,	/* trust struct's "resent-to" ptr */
		resent_to_us:1;		/* lit when we know its true */
} INDEXDATA_S;


/*
 * Line_hash can't return LINE_HASH_N, so we use it in a couple places as
 * a special value that we can assign knowing it can't be a real hash value.
 */
#define LINE_HASH_N	0xffffffff


typedef enum {NoKW, KW, KWInit} SubjKW;


/* exported protoypes */


#endif /* PITH_INDXTYPE_INCLUDED */
