/*
 * ========================================================================
 * Copyright 2006-2007 University of Washington
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * ========================================================================
 */


#ifndef MSMENU_H
#define MSMENU_H

#include "resource.h"


/*
 * var in pine's key structure we'll use
 */
#define	KS_OSDATAVAR			short menuitem;
#define	KS_OSDATAGET(X)			((X)->menuitem)
#define	KS_OSDATASET(X, Y)		((X)->menuitem = (Y))

/*
 * Menu key definitions.
 * Should be same values as in resouce.h
 * These are all in a range from KS_RANGESTART to KS_RANGEEND
 * with no holes.
 */
#define KS_NONE			0
#define KS_RANGESTART		IDM_MI_VIEW

#define KS_VIEW                     IDM_MI_VIEW
#define KS_EXPUNGE                  IDM_MI_EXPUNGE
#define KS_ZOOM                     IDM_MI_ZOOM
#define KS_SORT                     IDM_MI_SORT
#define KS_HDRMODE                  IDM_MI_HDRMODE
#define KS_MAINMENU                 IDM_MI_MAINMENU
#define KS_FLDRLIST                 IDM_MI_FLDRLIST
#define KS_FLDRINDEX                IDM_MI_FLDRINDEX
#define KS_COMPOSER                 IDM_MI_COMPOSER
#define KS_PREVPAGE                 IDM_MI_PREVPAGE
#define KS_PREVMSG                  IDM_MI_PREVMSG
#define KS_NEXTMSG                  IDM_MI_NEXTMSG
#define KS_ADDRBOOK                 IDM_MI_ADDRBOOK
#define KS_WHEREIS                  IDM_MI_WHEREIS
#define KS_PRINT                    IDM_MI_PRINT
#define KS_REPLY                    IDM_MI_REPLY
#define KS_FORWARD                  IDM_MI_FORWARD
#define KS_BOUNCE                   IDM_MI_BOUNCE
#define KS_DELETE                   IDM_MI_DELETE
#define KS_UNDELETE                 IDM_MI_UNDELETE
#define KS_FLAG                     IDM_MI_FLAG
#define KS_SAVE                     IDM_MI_SAVE
#define KS_EXPORT                   IDM_MI_EXPORT
#define KS_TAKEADDR                 IDM_MI_TAKEADDR
#define KS_SELECT                   IDM_MI_SELECT
#define KS_APPLY                    IDM_MI_APPLY
#define KS_POSTPONE                 IDM_MI_POSTPONE
#define KS_SEND                     IDM_MI_SEND
#define KS_CANCEL                   IDM_MI_CANCEL
#define KS_ATTACH                   IDM_MI_ATTACH
#define KS_TOADDRBOOK               IDM_MI_TOADDRBOOK
#define KS_READFILE                 IDM_MI_READFILE
#define KS_JUSTIFY                  IDM_MI_JUSTIFY
#define KS_ALTEDITOR                IDM_MI_ALTEDITOR
#define KS_GENERALHELP              IDM_MI_GENERALHELP
#define KS_SCREENHELP               IDM_MI_SCREENHELP
#define KS_EXIT                     IDM_MI_EXIT
#define KS_NEXTPAGE                 IDM_MI_NEXTPAGE
#define KS_SAVEFILE                 IDM_MI_SAVEFILE
#define KS_CURPOSITION              IDM_MI_CURPOSITION
#define KS_GOTOFLDR                 IDM_MI_GOTOFLDR
#define KS_JUMPTOMSG                IDM_MI_JUMPTOMSG
#define KS_RICHHDR                  IDM_MI_RICHHDR
#define KS_EXITMODE                 IDM_MI_EXITMODE
#define KS_REVIEW		    IDM_MI_REVIEW
#define KS_KEYMENU		    IDM_MI_KEYMENU
#define KS_SELECTCUR		    IDM_MI_SELECTCUR
#define KS_UNDO			    IDM_MI_UNDO
#define KS_SPELLCHK		    IDM_MI_SPELLCHK

#define KS_RANGEEND		IDM_MI_SPELLCHK

#define KS_COUNT	    ((KS_RANGEEND - KS_RANGESTART) + 1)



#define MENU_DEFAULT	300			/* Default menu for application. */
#define MENU_COMPOSER	301			/* Menu for pine's composer. */

#endif /* MSMENU_H */
