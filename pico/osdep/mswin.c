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

#define WIN31
#define STRICT

#define	termdef	1			/* don't define "term" external */

#include "../headers.h"

#include <stdarg.h>
#include <ddeml.h>

#include "mswin.h"
#include "resource.h"

#include "../../pith/osdep/pipe.h"
#include "../../pith/osdep/canaccess.h"

#include "../../pith/charconv/filesys.h"
#include "../../pith/charconv/utf8.h"

#include "../../pith/filttype.h"
#include "../../pith/osdep/color.h"

#include "mswin_tw.h"

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *			Defines
 *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


#define ICON


/* For debugging, export locals so debugger can see them. */
#ifdef DEBUG
#define LOCAL
#else
#define LOCAL		static
#endif


/*
 * Define which debugging is desired.  Generally only FDEBUG.
 */
#define FDEBUG		/* Standard file debugging. */

#undef SDEBUG		/* Verbose debugging of startup and windows handling*/
#undef CDEBUG		/* Verbose debugging of character input timeing. */

#undef OWN_DEBUG_FILE	/* Define if we want to write to our own debug file,
			 * not pine's. */


/* Max size permitted for the screen. */
#define MAXNROW			200
#define MAXNCOLUMN		NLINE

#define MINNROW			10	/* Minimum screen size */
#define MINNCOLUMN		32

#define MARGINE_LEFT		3
#define MARGINE_TOP		1

#define FRAME_3D_SIZE		1

#define WIN_MIN_X_SIZE		190	/* Minimum window size. */
#define WIN_MIN_Y_SIZE		180

#define WIN_X_BORDER_SIZE	8	/* Space taked by window frame. */
#define WIN_Y_BORDER_SIZE	65

#define FONT_MIN_SIZE		5
#define FONT_MAX_SIZE		21

#define PRINT_TAB_SIZE		8	/* Tab size used by print code. */


#define TB_HEIGHT		32	/* Tool Bar Height. */
#define TB_BUTTONHEIGHT		16	/* Button Height. */
#define TB_BUTTONSPACING	8	/* Space between buttons. */


/* Some string lengths. */
#define MAXLEN_TEMPSTR		256	/* Max size for temp storage. */

#define WIN_POS_STR_MAX_LEN	21	/* Max length for window-position
					 * string. */

#define MENU_ITEM_NAME_LEN	32	/* Menu item name lengths. */


/* Length of keyboard input queue. */
#define CHARACTER_QUEUE_LENGTH	32
#define MOUSE_QUEUE_LENGTH	32

/* Number of resize callback functions we can keep track of. */
#define RESIZE_CALLBACK_ARRAY_SIZE	3

/* Number of bytes held in the write accumulator. */
#define WRITE_ACCUM_SIZE		200

/* Max time that may pass between calls to GetMessage.  See mswin_charavail()
 */
#define GM_MAX_TIME	3000		/* In milliseconds.*/

/* My Timer Message */
#define MY_TIMER_ID	33
/* timeout period in miliseconds. */
#define MY_TIMER_PERIOD (UINT)((IDLE_TIMEOUT + 1)*1000)
/***** We use variable my_timer_period now instead so that we can set
       it differently when in pine and when in regular old pico.
       We're not entirely sure we need it in pico, but we will leave
       it there because we don't understand.
 *****/
#define MY_TIMER_SHORT_PERIOD (UINT)5000  /* used when there is a task in
					     the OnTask list. */
#define MY_TIMER_VERY_SHORT_PERIOD (UINT)500  /* used when SIGALRM and alarm()
						 is set. */
#define MY_TIMER_EXCEEDINGLY_SHORT_PERIOD (UINT)80 /* used when
						 gAllowMouseTracking is set */

#define TIMER_FAIL_MESSAGE TEXT("Failed to get all necessary Windows resources (timers).  Alpine will run, but may not be able to keep the connection to the server alive.  Quitting other applications and restarting Alpine may solve the problem.")

/*
 * Task bar notification Icon id and call back message for it.
 */
#define TASKBAR_ICON_NEWMAIL		1
#define TASKBAR_ICON_MESSAGE		WM_USER+1000


/*
 * Below here are fixed constancs that really should not be changed.
 */

/* Auto Wrap States. */
#define WRAP_OFF	0		/* Never wrap to next line. */
#define WRAP_ON		1		/* Wrap to next line. */
#define WRAP_NO_SCROLL	2		/* Wrap to next line but DON'T scroll
					   screen to do it. */
/* Speicial keys in the Character Queue. */
#define CQ_FLAG_DOWN		0x01
#define CQ_FLAG_EXTENDED	0x02
#define CQ_FLAG_ALT		0x04

/* Special ASCII characters. */
#define ASCII_BEL       0x07
#define ASCII_BS        0x08
#define ASCII_TAB	0x09
#define ASCII_LF        0x0A
#define ASCII_CR        0x0D
#define ASCII_XON       0x11
#define ASCII_XOFF      0x13

/* Character Attributes. */
#define CHAR_ATTR_NORM	0x00		/* Normal. */
#define CHAR_ATTR_REV	0x01		/* Reverse Video. */
#define CHAR_ATTR_BOLD	0x02		/* Reverse Video. */
#define CHAR_ATTR_ULINE	0x04		/* Reverse Video. */
#define CHAR_ATTR_SEL	0x08		/* Selected text. */
#define CHAR_ATTR_NOT   0x80		/* No attributes. */

/*
 * Different applications that we know about.
 */
#define APP_UNKNOWN		0
#define APP_PICO		1
#define APP_PICO_IDENT		TEXT("pico")
#define APP_PINE		2
#define APP_PINE_IDENT		TEXT("pine")

/*
 * Control values for call to AccelCtl.
 */
/*#undef ACCELERATORS*/
#define	ACCELERATORS
#define ACCEL_UNLOAD		0	/* Unload the accelerators. */
#define ACCEL_LOAD		1	/* Load the accelerators. */
#define ACCEL_TOGGLE		2	/* Toggle the accelerators. */

/*
 * flag bits to control which edit menu options can get lit
 */
#define	EM_NONE			0L
#define	EM_CP			0x0001
#define	EM_CP_APPEND		0x0002
#define	EM_FIND			0x0004
#define	EM_PST			0x0008
#define	EM_PST_ABORT		0x0010
#define	EM_CUT			0x0020
#define	EM_SEL_ALL		0x0040

#define	EM_MAX_ACCEL		6

/* Offsets to objects in window extra storage. */
#define GWL_PTTYINFO		0	/* Offset in Window extra storage. */

/*
 *
 */
#define	FONT_CHARSET_FONT	DEFAULT_CHARSET

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *			Typedefs
 *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/* Type that the screen array is made up of. */
#define	CHAR	unsigned char

/* define possible caret shapes */
typedef	enum {
    CaretBlock = 0,
    CaretSmallBlock,
    CaretHorizBar,
    CaretVertBar
} CARETS;

/* Type that the attribute array is made up of. */
typedef struct _character_attribute {
    unsigned  style:8;
    DWORD     rgbFG, rgbBG;
} CharAttrib;

typedef int			(*ResizeCallBackProc)();
typedef	int			(*FileDropCallBackProc)();
typedef short			CORD;

/* NOTE:  There is currently code that assumes that CHAR and CharAttrib
 *	are one byte in size.  All this code is flaged with a preceeding
 *	assert () */

/* Struct that defines command menu entries. */
typedef struct tagMenuItem {
    BOOL	miActive;
    UCS 	miKey;
    short	miIndex;
    char       *miLabel;
} MenuItem;


/* List of child window IDs and previous window procedures. */
typedef struct tagBtnList {
    WORD	wndID;
    WNDPROC	wndProc;
} BtnList;


/* General info. */
typedef struct tagTTYINFO {
    TCHAR	*pScreen;	/* Screen. */
    int         *pCellWidth;    /* how wide to paint the char */
    CharAttrib	*pAttrib;	/* Attributes. */
    TCHAR	writeAccum[WRITE_ACCUM_SIZE];
    int		writeAccumCount;
    CARETS	cCaretStyle;	/* Current caret's style */
    int		scrollRange;	/* Current scroll bar range. */
    long	scrollPos;	/* Current scroll position. */
    long	scrollTo;	/* Possition of last scroll to. */
    HFONT	hTTYFont;
    LOGFONT	lfTTYFont;
    DWORD	rgbFGColor;	/* Normal forground color. */
    DWORD	rgbBGColor;	/* Normal background color. */
    DWORD	rgbRFGColor;	/* Reverse forground color. */
    DWORD	rgbRBGColor;	/* Reverse background color */
    unsigned	screenDirty:1;	/* TRUE if screen needs update. */
    unsigned	eraseScreen:1;	/* TRUE if need to erase whole screen */
    unsigned	fMinimized:1;	/* True when window is minimized. */
    unsigned	fMaximized:1;	/* True when window is maximized. */
    unsigned	fFocused:1;	/* True when we have focus. */
    unsigned	fNewLine:1;	/* Auto LF on CR. */
    unsigned	fMassiveUpdate:1;/* True when in Massive screen update. */
    unsigned	fNewMailIcon:1;	/* True when new mail has arrived. */
    unsigned	fMClosedIcon:1;	/* True when no mailbox is open. */
    unsigned	fCursorOn:1;		/* True when cursor's shown */
    unsigned	fCaretOn:1;		/* True if caret's displayed */
    unsigned	fTrayIcon:1;		/* True if tool tray icon's on */
    ResizeCallBackProc  resizer[RESIZE_CALLBACK_ARRAY_SIZE];
    FileDropCallBackProc dndhandler;
    int		autoWrap;	/* Auto wrap to next line. */
    CharAttrib	curAttrib;	/* Current character attributes. */
    int		actNRow, actNColumn;	/* Actual number of rows and comumns
					 * displayed. */
    CORD	xSize, ySize;		/* Size of screen in pixels */
    CORD	xScroll, yScroll;	/* ?? */
    CORD	xOffset, yOffset;	/* Offset from the left and top of
					 * window contents. */
    CORD	nColumn, nRow;		/* Current position of cursor in
				         * cells. */
    CORD	xChar, yChar;		/* Width of a char in pixels. */
    int		yCurOffset;		/* offset of cursor Y-size */
    CORD	fDesiredSize;		/* TRUE when there is a specific size
					 * the window should be expanded to
					 * after being minimized. */
    CORD	xDesPos, yDesPos;	/* Desired position. */
    CORD	xDesSize, yDesSize;	/* Desired window position. */
    int		curWinMenu;		/* Current window menu. */
    HACCEL	hAccel;			/* Handle to accelorator keys. */
    UINT	fAccel;			/* vector of bound accelerator keys. */
    CORD	toolBarSize;		/* Size of toolbar. */
    BOOL	toolBarTop;		/* Toolbar on top? */
    int		curToolBarID;
    BtnList	*toolBarBtns;
    HWND	hTBWnd;
    HBRUSH	hTBBrush;
    BOOL	menuItemsCurrent;
    BOOL        noScrollUpdate;
    BOOL        scrollRangeChanged;
    long        noSUpdatePage;
    long        noSUpdateRange;
    short	menuItemsIndex;
    MenuItem	menuItems[KS_COUNT];
} TTYINFO, *PTTYINFO ;


#define MAXCLEN (MAX(MAXCOLORLEN,RGBLEN+1))
typedef struct MSWINColor {
    char		*colorName;
    char		*canonicalName;
    COLORREF		colorRef;
} MSWINColor;


typedef	struct {
    char	*name;
    CARETS	 style;
} MSWinCaret_t;


/*
 * Entry in the OnTask list.  This is a list of actions to perform
 * when a task exits.  Currently, the only thing we do is delete
 * files.  if that changes this structure will get more complex.
 *
 * hTask == NULL means "This program" and can be used to arrange for
 * deletion of files when this program exits.
 */
typedef struct ontask {
    struct ontask	*next;
    HTASK		hTask;
    char		path[PATH_MAX+1];
} OnTaskItem;

typedef void (__cdecl *SignalType)(int);

typedef	struct _iconlist {
    HICON  hIcon;
/*    char    path[PATH_MAX]; */
/*    WORD    index;          */
    int	    id;
    short   row;
    struct _iconlist *next;
} IconList;

/*
 * char * array for printing registry settings.
 */
typedef struct MSWR_LINE_BUFFER {
    char **linep; /* store these as utf8, since that's what we have to pass back */
    unsigned long size;
    unsigned long offset;
} MSWR_LINE_BUFFER_S;


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *			Forward function declarations.
 *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

LOCAL void *
MyGetWindowLongPtr(HWND hwnd, int nIndex)
{
    return (void *)(LONG_PTR)GetWindowLongPtr(hwnd, nIndex);
}

LOCAL LONG_PTR
MySetWindowLongPtr(HWND hwnd, int nIndex, void *NewLongPtr)
{
// warning C4244: 'function': conversion from 'LONG_PTR' to 'LONG',
//  possible loss of data
#pragma warning(push)
#pragma warning(disable: 4244)
    return SetWindowLongPtr(hwnd, nIndex, (LONG_PTR)NewLongPtr);
#pragma warning(pop)
}

#ifdef	WIN32
#define	GET_HINST( hWnd )  ((HINSTANCE) MyGetWindowLongPtr( hWnd, GWLP_HINSTANCE ))
#define	GET_ID( hWnd )  (LOWORD(MyGetWindowLongPtr( hWnd, GWLP_ID )))
#else
#define GET_HINST( hWnd )  ((HINSTANCE) GetWindowWord( hWnd, GWW_HINSTANCE ))
#define	GET_ID( hWnd )  ((WORD) GetWindowWord( hWnd, GWW_ID ))
#endif

#define CONSTRAIN(v,min,max)	((v) = (v) < (min) ? (min) : (v) > (max) ? (max) : (v))


/* function prototypes (private) */
int app_main (int argc, char *argv[]);

void		WinExit (void);

LOCAL void	mswin_invalidparameter(const wchar_t *, const wchar_t *,
				       const wchar_t *, unsigned int, uintptr_t);

LOCAL BOOL	InitApplication (HANDLE);
LOCAL HWND	InitInstance (HANDLE, int);
LOCAL void	MakeArgv (HINSTANCE hInstance, LPSTR cmdLine, int *pargc,
				CHAR ***pargv);
LOCAL LRESULT NEAR	CreateTTYInfo (HWND hWnd);
LOCAL BOOL NEAR	DestroyTTYInfo (HWND hWnd);
LOCAL int	ResizeTTYScreen (HWND hWnd, PTTYINFO pTTYInfo,
					int newNRow, int newNColumn);
LOCAL BOOL	ResetTTYFont (HWND, PTTYINFO, LOGFONT *);
LOCAL BOOL	EraseTTY (HWND, HDC);
LOCAL BOOL	PaintTTY (HWND);
LOCAL BOOL	GetMinMaxInfoTTY (HWND hWnd, MINMAXINFO __far *lpmmi);
LOCAL BOOL	AboutToSizeTTY (HWND hWnd, WINDOWPOS *winPos);
LOCAL BOOL	SizeTTY (HWND, int, CORD, CORD);
LOCAL BOOL	SizingTTY (HWND hWnd, int fwSide, LPRECT pRect);
LOCAL void	FrameRect3D(HDC hdc, RECT * pRC, int width, BOOL raised);
LOCAL void	FillRectColor(HDC hDC, RECT * pRC, COLORREF color);


LOCAL BOOL	MoveTTY (HWND hWnd, int xPos, int yPos);
LOCAL void	ScrollTTY (HWND hWnd, int wScrollCode, int nPos, HWND hScroll);
LOCAL void	CaretTTY (HWND hWnd, CARETS cStyle);
LOCAL void	CaretCreateTTY (HWND hWnd);
#ifdef	WIN32
LOCAL void	MouseWheelTTY (HWND hWnd, int xPos, int yPos,
			       int fwKeys, int zDelta);
LOCAL void	MouseWheelMultiplier ();
#endif
BOOL CALLBACK	NoMsgsAreSent (void);
LOCAL BOOL	SetTTYFocus (HWND);
LOCAL BOOL	KillTTYFocus (HWND);
LOCAL BOOL	MoveTTYCursor (HWND);
LOCAL BOOL	ProcessTTYKeyDown (HWND hWnd, TCHAR bOut);
LOCAL BOOL	ProcessTTYCharacter (HWND hWnd, TCHAR bOut);
LOCAL BOOL	ProcessTTYMouse (HWND hWnd, int mevent, int button, CORD xPos,
					CORD yPos, WPARAM keys);
LOCAL BOOL	ProcessTTYFileDrop (HANDLE wParam);
LOCAL void	ProcessTimer (void);
LOCAL BOOL	WriteTTYBlock (HWND, LPTSTR, int);
LOCAL BOOL	WriteTTYText (HWND, LPTSTR, int);
LOCAL BOOL	WriteTTYChar (HWND, TCHAR);

LOCAL VOID	GoModalDialogBoxParam (HINSTANCE, LPTSTR, HWND,
					DLGPROC, LPARAM);
LOCAL BOOL	SelectTTYFont (HWND);
LOCAL void	SetColorAttribute (COLORREF *cf, char *colorName);
LOCAL void	SetReverseColor (void);
LOCAL BOOL	ConvertRGBString (char *colorName, COLORREF *cf);
LOCAL char     *ConvertStringRGB (char *colorName, size_t ncolorName, COLORREF colorRef);
LOCAL BOOL	ScanInt (char *str, int min, int max, int *val);

LOCAL void	TBToggle (HWND);
LOCAL void	TBPosToggle (HWND);
LOCAL void	TBShow (HWND);
LOCAL void	TBHide (HWND);
LOCAL void	TBSwap (HWND, int);
LOCAL unsigned  scrwidth(LPTSTR lpText, int nLength);
LOCAL long      pscreen_offset_from_cord(int row, int col, PTTYINFO pTTYInfo);

LOCAL int       tcsucmp(LPTSTR o, LPTSTR r);
LOCAL int       _print_send_page(void);

/* defined in region.c */
int            copyregion(int f, int n);


#ifdef ACCELERATORS
#ifdef ACCELERATORS_OPT
LOCAL void	AccelCtl (HWND hWnd, int ctl, BOOL saveChange);
#endif
LOCAL void	AccelManage (HWND hWnd, long accels);
LOCAL void	UpdateAccelerators (HWND hWnd);
#endif

LOCAL UINT	UpdateEditAllowed(HWND hWnd);
LOCAL void	PopupConfig(HMENU hMenu, MPopup *members, int *n);
LOCAL MPopup   *PopupId(MPopup *members, int id);
LOCAL BOOL	CopyCutPopup (HWND hWnd, UINT fAllowed);

LOCAL void	SelStart (int nRow, int nColumn);
LOCAL void	SelFinish (int nRow, int nColumn);
LOCAL void	SelClear (void);
LOCAL void	SelTrackXYMouse (int xPos, int yPos);
LOCAL void	SelTrackMouse (int nRow, int nColumn);
LOCAL BOOL	SelAvailable (void);
LOCAL void	SelDoCopy (HANDLE hCB, DWORD lenCB);

LOCAL void	UpdateTrayIcon(DWORD dwMsg, HICON hIcon, LPTSTR tip);

LOCAL void	FlushWriteAccum (void);

LOCAL int       mswin_reg_lptstr(int, int, LPTSTR, size_t);

LOCAL BOOL	MSWRPoke(HKEY hKey, LPTSTR subkey, LPTSTR valstr, LPTSTR data);
LOCAL int	MSWRClear(HKEY hKey, LPTSTR pSubKey);
LOCAL void	MSWRAlpineSet(HKEY hRootKey, LPTSTR subkey, LPTSTR val,
			       int update, LPTSTR data);
LOCAL int       MSWRProtocolSet(HKEY hKey, int type, LPTSTR path_lptstr);
LOCAL void	MSWRAlpineSetHandlers(int update, LPTSTR path);
LOCAL int	MSWRAlpineGet(HKEY hKey, LPTSTR subkey, LPTSTR val,
			       LPTSTR data_lptstr, size_t len);

LOCAL void	MSWIconAddList(int row, int id, HICON hIcon);
LOCAL int	MSWIconPaint(int row, HDC hDC);
LOCAL void	MSWIconFree(IconList **ppIcon);
LOCAL void      MSWRLineBufAdd(MSWR_LINE_BUFFER_S *lpLineBuf, LPTSTR line);
LOCAL int       MSWRDump(HKEY hKey, LPTSTR pSubKey, int keyDepth,
			 MSWR_LINE_BUFFER_S *lpLineBuf);


		/* ... interface routines ... */


void		ProcessMenuItem (HWND hWnd, WPARAM wParam);

void		AlarmDeliver (void);
void		HUPDeliver (void);

void		PrintFontSameAs (HWND hWnd);
void		PrintFontSelect (HWND hWnd);
void		ExtractFontInfo(LOGFONT *pFont,
				LPTSTR fontName, size_t nfontName,
				int *fontSize,
				LPTSTR fontStyle, size_t nfontStyle,
				int ppi,
				LPTSTR fontCharSet, size_t nfontCharSet);

LOCAL void	DidResize (PTTYINFO pTTYInfo);

LOCAL void	UpdateMenu (HWND hWnd);
LOCAL void	EditCut (void);
LOCAL void	EditCopy (void);
LOCAL void	EditCopyAppend (void);
LOCAL void	EditDoCopyData (HANDLE hCB, DWORD lenCB);
LOCAL void	EditPaste (void);
LOCAL void	EditCancelPaste (void);
LOCAL UCS	EditPasteGet (void);
LOCAL BOOL	EditPasteAvailable (void);
LOCAL void	EditSelectAll (void);

LOCAL void	MSWHelpShow (cbstr_t);
LOCAL int	MSWHelpSetMenu (HMENU hmenu);


LOCAL void	SortHandler (int order, int reverse);
LOCAL void	FlagHandler (int order, int reverse);

LOCAL void	MyTimerSet (void);

LOCAL LRESULT	ConfirmExit (void);

LOCAL void	CQInit (void);
LOCAL BOOL	CQAvailable (void);
LOCAL BOOL	CQAdd (UCS c, BOOL fKeyControlDown);
LOCAL BOOL	CQAddUniq (UCS c, BOOL fKeyControlDown);
LOCAL UCS	CQGet ();

LOCAL void	MQInit (void);
LOCAL BOOL	MQAvailable (void);
LOCAL BOOL	MQAdd (int mevent, int button, int nRow, int nColumn,
				int keys, int flags);
LOCAL BOOL	MQGet (MEvent * pmouse);
LOCAL BOOL	MQClear (int flag);

LOCAL UCS mswin_getc (void);

LOCAL DWORD	ExplainSystemErr(void);

LOCAL void	RestoreMouseCursor();

LOCAL BYTE	mswin_string2charsetid(LPTSTR);
LOCAL int   mswin_charsetid2string (LPTSTR fontCharSet,
					size_t nfontCharSet, BYTE lfCharSet);

LOCAL int       mswin_tw_init(MSWIN_TEXTWINDOW *mswin_tw, int id,
                              LPCTSTR title);
LOCAL MSWIN_TEXTWINDOW *mswin_tw_displaytext_lptstr (LPTSTR, LPTSTR, size_t,
                                                     LPTSTR *, MSWIN_TEXTWINDOW *mswin_tw,
                                                     int);
LOCAL void	mswin_tw_print_callback(MSWIN_TEXTWINDOW *mswin_tw);


/* Functions exported to MS Windows. */

LRESULT FAR PASCAL __export PWndProc (HWND, UINT, WPARAM, LPARAM);
BOOL FAR PASCAL __export ToolBarProc (HWND, UINT, WPARAM, LPARAM);
LRESULT FAR PASCAL __export TBBtnProc (HWND, UINT, WPARAM, LPARAM);
BOOL FAR PASCAL __export AboutDlgProc (HWND, UINT, WPARAM, LPARAM);
BOOL FAR PASCAL __export SplashDlgProc (HWND, UINT, WPARAM, LPARAM);

LOCAL HDDEDATA CALLBACK DdeCallback(UINT uType, UINT uFmt, HCONV hconv,
				    HSZ hsz1, HSZ hsz2, HDDEDATA hdata,
				    DWORD dwData1, DWORD dwData2);

/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *			Module globals.
 *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


PTTYINFO		gpTTYInfo;
HWND			ghTTYWnd;
HINSTANCE		ghInstance;
BOOL			gfUseDialogs;
FILE			*mswin_debugfile = NULL;
int			mswin_debug = 0;
TCHAR			gszAppName[45];

LOCAL TCHAR		TempBuf [MAXLEN_TEMPSTR];

LOCAL TCHAR		gszTTYClass[] = TEXT("PineWnd");
LOCAL int		gAppIdent = APP_UNKNOWN;

LOCAL int		gNMW_width;

LOCAL HCURSOR		ghCursorArrow = NULL;
LOCAL HCURSOR		ghCursorBusy = NULL;
LOCAL HCURSOR		ghCursorIBeam = NULL;
LOCAL HCURSOR		ghCursorHand = NULL;
LOCAL HCURSOR		ghCursorCurrent = NULL;

LOCAL HWND		ghSplashWnd = NULL;

LOCAL COLOR_PAIR *the_rev_color, *the_normal_color;

/* Used for Pasting text. */
LOCAL HANDLE		ghPaste = NULL;		/* Handle to Paste data. */
LOCAL TCHAR		*gpPasteNext = NULL;	/* Pointer to current char. */
LOCAL size_t		gPasteBytesRemain = 0;	/* Count of bytes left. */
LOCAL BOOL		gPasteWasCR = FALSE;	/* Previous char was CR. */
LOCAL int		gPasteEnabled = MSWIN_PASTE_DISABLE;
LOCAL getc_t		gCopyCutFunction = NULL;
LOCAL cbarg_t		gScrollCallback = NULL;
LOCAL BOOL		gScrolling = FALSE;	/* Keeps track of when we are
						 * in scroll routine. */
LOCAL cbarg_t		gSortCallback = NULL;
LOCAL cbarg_t		gFlagCallback = NULL;
LOCAL cbarg_t		gHdrCallback = NULL;
LOCAL cbarg_t		gZoomCallback = NULL;
LOCAL cbarg_t		gFkeyCallback = NULL;
LOCAL cbarg_t		gSelectedCallback = NULL;
LOCAL BOOL		gMouseTracking = FALSE;	/* Keeps track of when we are
						 * tracking the mouse. */
LOCAL FARPROC		gWSBlockingProc = NULL;
LOCAL DLGPROC		gToolBarProc = NULL;
LOCAL WNDPROC		gTBBtnProc = NULL;

LOCAL BOOL		gAllowCopy = FALSE;
LOCAL BOOL		gAllowCut = FALSE;

LOCAL BOOL		gAllowMouseTrack = FALSE;/* Upper layer interested in
						 * mouse tracking. */
LOCAL short		gsMWMultiplier;
LOCAL MEvent		gMTEvent;

LOCAL cbstr_t		gHelpGenCallback = NULL;
LOCAL BOOL		gfHelpGenMenu = FALSE;	/* TRUE when help menu
						 * installed. */
LOCAL cbstr_t		gHelpCallback = NULL;
LOCAL BOOL		gfHelpMenu = FALSE;	/* TRUE when help menu
						 * installed. */
LOCAL char		*gpCloseText;

LOCAL DWORD		gGMLastCall = 0;	/* Last time I called
						 * GetMessage. */
LOCAL BOOL		gConfirmExit = FALSE;
LOCAL HICON		ghNormalIcon = NULL;
LOCAL HICON		ghNewMailIcon = NULL;
LOCAL HICON		ghMClosedIcon = NULL;

LOCAL int		gPrintFontSize;
LOCAL TCHAR		gPrintFontName[LF_FACESIZE];
LOCAL TCHAR		gPrintFontStyle[64];
LOCAL TCHAR		gPrintFontCharSet[256];
LOCAL BOOL		gPrintFontSameAs = TRUE;

LOCAL UINT		gTimerCurrentPeriod = 0;

LOCAL cbvoid_t  	gPeriodicCallback = NULL; /* Function to call. */
LOCAL DWORD		gPeriodicCBTimeout = 0;   /* Time of next call. */
LOCAL DWORD		gPeriodicCBTime = 0;	  /* Delay between calls. */

//=========================================================================
// n
//=========================================================================
MSWIN_TEXTWINDOW        gMswinAltWin = {0};
MSWIN_TEXTWINDOW        gMswinNewMailWin = {0};

MSWIN_TEXTWINDOW        gMswinIMAPTelem = {0};
LOCAL cbvoid_t          gIMAPDebugONCallback = NULL;
LOCAL cbvoid_t          gIMAPDebugOFFCallback = NULL;
LOCAL cbvoid_t          gEraseCredsCallback = NULL;
LOCAL cbvoid_t		gViewInWindCallback = NULL;

LOCAL cbvoid_t        	gConfigScreenCallback = NULL;

LOCAL cbarg_t		gMouseTrackCallback = NULL;

/* Currently only implement one SIGNAL so only need single variable. */
LOCAL SignalType	gSignalAlarm = SIG_DFL;
LOCAL DWORD		gAlarmTimeout = 0;	/* Time alarm expires in
						 * seconds
						 * (GetTickCount()/1000) */
LOCAL SignalType	gSignalHUP = SIG_DFL;

LOCAL IconList	       *gIconList = NULL;


/*
 * There is some coordination between these names and the similar names
 * in osdep/unix. The names in the left column are mapped to the colors
 * in the right column when displaying. Note that the last eight colors map
 * to the same thing as the 1st eight colors. This is an attempt to inter-
 * operate with 16-color xterms. When editing a color and writing the color
 * into the config file, we use the canonical_name (middle column). So if
 * the user chooses green from the menu we write color010 in the config
 * file. [This has changed. Now we put the RGB value in the config file
 * instead. We leave this so that we can interpret color010 in the config
 * file from old versions of pine.]
 * We display that as green later. The xterm also displays that as
 * green, because the bright green (color010) on the xterm is what we want
 * to consider to be green, and the other one (color002) is dark green.
 * And dark green sucks.
 * On the PC we only use the first 11 colors in this table when giving the
 * user the set color display, since the last eight are repeats.
 */
LOCAL MSWINColor  MSWINColorTable[] =  {
	"black",	"000,000,000",	RGB(0,0,0),
	"red",		"255,000,000",	RGB(255,0,0),
	"green",	"000,255,000",	RGB(0,255,0),
	"yellow",	"255,255,000",	RGB(255,255,0),
	"blue",		"000,000,255",	RGB(0,0,255),
	"magenta",	"255,000,255",	RGB(255,0,255),
	"cyan",		"000,255,255",	RGB(0,255,255),
	"white",	"255,255,255",	RGB(255,255,255),
	"colorlgr",	"192,192,192",	RGB(192,192,192),	/* lite gray */
	"colormgr",	"128,128,128",	RGB(128,128,128),	/* med. gray */
	"colordgr",	"064,064,064",	RGB(64,64,64),		/* dark gray */
	"color008",	"000,000,000",	RGB(0,0,0),
	"color009",	"255,000,000",	RGB(255,0,0),
	"color010",	"000,255,000",	RGB(0,255,0),
	"color011",	"255,255,000",	RGB(255,255,0),
	"color012",	"000,000,255",	RGB(0,0,255),
	"color013",	"255,000,255",	RGB(255,0,255),
	"color014",	"000,255,255",	RGB(0,255,255),
	"color015",	"255,255,255",	RGB(255,255,255),
	NULL,		NULL,		0
};

#define fullColorTableSize ((sizeof(MSWINColorTable) / sizeof(MSWINColorTable[0])) - 1)
#define visibleColorTableSize  (fullColorTableSize - 8)


LOCAL MSWinCaret_t MSWinCaretTable[] = {
	"Block",	CaretBlock,
	"ShortBlock",	CaretSmallBlock,
	"Underline",	CaretHorizBar,
	"VertBar",	CaretVertBar,
	NULL,		0
};


/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *			Windows Functions.
 *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*---------------------------------------------------------------------------
 *  int PASCAL WinMain( HANDLE hInstance, HANDLE hPrevInstance,
 *                      LPSTR lpszCmdLine, int nCmdShow )
 *
 *  Description:
 *     This is the main window loop!
 *
 *  Parameters:
 *     As documented for all WinMain() functions.
 *
 *--------------------------------------------------------------------------*/
int PASCAL
wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    LPTSTR lpszCmdLine, int nCmdShow )
{
    CHAR		**argv;
    int			argc, i, nosplash = 0;
    char               *cmdline_utf8;

    ghInstance = hInstance;
    if (!hPrevInstance)
	if (!InitApplication( hInstance ))
	    return ( FALSE ) ;

    /*
     * Instead of crashing into Dr. Watson on invalid parameter calls
     * into the C library, just return the error indication that these
     * functions are normally expected to return.
     */
    _set_invalid_parameter_handler (mswin_invalidparameter);

#ifdef OWN_DEBUG_FILE	/* Want to write to seperate memdebug.txt file. */
    mswin_debugfile = fopen ("memdebug.txt", "w");
    fprintf (mswin_debugfile, "Beginning of mswin debug log\n");
    if (mswin_debugfile != NULL) {
	mswin_debug = 4;
	MemDebug (mswin_debug, mswin_debugfile);
	fprintf (mswin_debugfile, "Show window as:  %d\n", nCmdShow);
	fflush (mswin_debugfile);
    }
#endif

    if (NULL == (ghTTYWnd = InitInstance (hInstance, nCmdShow)))
	return (FALSE);

    /* cmdline_utf8 memory is never freed */
    cmdline_utf8 = lptstr_to_utf8(lpszCmdLine);

    MakeArgv (hInstance, cmdline_utf8, &argc, &argv);

    for(i = 0; i < argc; i++)
      if(strcmp((const char *)argv[i], "-nosplash") == 0){
	  nosplash = 1;
	  break;
      }
    /* Create Splash window */
    if(nosplash == 0){
	ghSplashWnd = CreateDialog(hInstance,
				   MAKEINTRESOURCE( SPLASHDLGBOX ),
				   ghTTYWnd, (DLGPROC)SplashDlgProc);
	ShowWindow (ghSplashWnd, SW_SHOWNORMAL);
    }

    atexit (WinExit);

    app_main (argc, (char **)argv);

    return (TRUE);
}


LOCAL void
mswin_invalidparameter(const wchar_t *expression,
		       const wchar_t *function,
		       const wchar_t *file,
		       unsigned int line,
		       uintptr_t reserved)
{
    /* do nothing for now */
}


void
WinExit (void)
{
    MSG			msg;

    if (ghTTYWnd == NULL)
	return;

    UpdateTrayIcon(NIM_DELETE, 0, NULL);

    /* Destroy main window and process remaining events. */
    DestroyWindow (ghTTYWnd);
    while (GetMessage (&msg, NULL, 0, 0)) {
	TranslateMessage (&msg);
	DispatchMessage (&msg);
    }

#ifdef OWN_DEBUG_FILE
    fclose (mswin_debugfile);
#endif
    if (gWSBlockingProc != NULL)
	FreeProcInstance (gWSBlockingProc);

    MemFreeAll ();
    ghTTYWnd = NULL;
}


/*---------------------------------------------------------------------------
 *  BOOL  InitApplication( HANDLE hInstance )
 *
 *  Description:
 *     First time initialization stuff.  This registers information
 *     such as window classes.
 *
 *  Parameters:
 *     HANDLE hInstance
 *        Handle to this instance of the application.
 *
 *--------------------------------------------------------------------------*/
LOCAL BOOL
InitApplication (HANDLE hInstance)
{
    WNDCLASS  wndclass;

    /*
     * Register tty window class.
     */
    wndclass.style =         0;  /* CS_NOCLOSE; */
    wndclass.lpfnWndProc =   PWndProc;
    wndclass.cbClsExtra =    0;
    wndclass.cbWndExtra =    sizeof (LONG);
    wndclass.hInstance =     hInstance ;
    /* In win16 we paint our own icon. */
    wndclass.hIcon =         LoadIcon (hInstance, MAKEINTRESOURCE (ALPINEICON));
    wndclass.hCursor =       LoadCursor (NULL, IDC_ARROW);
    wndclass.hbrBackground = (HBRUSH) (COLOR_WINDOW + 1);
    wndclass.lpszMenuName =  MAKEINTRESOURCE (ALPINEMENU);
    wndclass.lpszClassName = gszTTYClass ;

    return (RegisterClass (&wndclass));
}


/*---------------------------------------------------------------------------
 *  HWND  InitInstance( HANDLE hInstance, int nCmdShow )
 *
 *  Description:
 *     Initializes instance specific information.
 *
 *  Parameters:
 *     HANDLE hInstance
 *        Handle to instance
 *
 *     int nCmdShow
 *        How do we show the window?
 *
/*--------------------------------------------------------------------------*/
LOCAL HWND
InitInstance (HANDLE hInstance, int nCmdShow)
{
    HWND       hTTYWnd;
    TCHAR      appIdent[32];
    SCROLLINFO scrollInfo;

#ifdef SDEBUG
    if (mswin_debug >= 5)
	fprintf (mswin_debugfile, "InitInstance:::  entered, nCmdShow %d\n",
		nCmdShow);
#endif

    LoadString (hInstance, IDS_APPNAME, gszAppName, sizeof(gszAppName) / sizeof(TCHAR));

    /* create the TTY window */
    hTTYWnd = CreateWindow (gszTTYClass, gszAppName,
		       WS_OVERLAPPEDWINDOW | WS_VSCROLL,
		       CW_USEDEFAULT, CW_USEDEFAULT,
		       CW_USEDEFAULT, CW_USEDEFAULT,
		       HWND_DESKTOP, NULL, hInstance, NULL);
	
    scrollInfo.cbSize = sizeof(SCROLLINFO);
    scrollInfo.fMask = SIF_DISABLENOSCROLL | SIF_PAGE | SIF_RANGE | SIF_POS;
    scrollInfo.nMin = 0;
    scrollInfo.nMax = 1;
    scrollInfo.nPage = 1;
    scrollInfo.nPos = 0;
    SetScrollInfo(hTTYWnd, SB_VERT, &scrollInfo, FALSE);
    EnableScrollBar (hTTYWnd, SB_VERT, ESB_DISABLE_BOTH);

    if (NULL == hTTYWnd)
	    return (NULL);

    ghTTYWnd = hTTYWnd;

    ghNormalIcon = LoadIcon (hInstance, MAKEINTRESOURCE (ALPINEICON));
    ghNewMailIcon = LoadIcon (hInstance, MAKEINTRESOURCE (NEWMAILICON));
    ghMClosedIcon = LoadIcon (hInstance, MAKEINTRESOURCE (MCLOSEDICON));

    ghCursorArrow = LoadCursor (NULL, IDC_ARROW);
    ghCursorBusy = LoadCursor (NULL, IDC_WAIT);
    ghCursorIBeam = LoadCursor (NULL, IDC_IBEAM);
#ifdef	IDC_HAND
    ghCursorHand = LoadCursor (NULL, IDC_HAND);
#else
    ghCursorHand = LoadCursor (hInstance, MAKEINTRESOURCE( PICOHAND ));
#endif
    ghCursorCurrent = ghCursorArrow;

    MouseWheelMultiplier();

    CQInit ();
    MQInit ();


    /*
     * Load a resource with the name of the application.  Compare to
     * known applications to determine who we are running under.
     * currently, only differentiation is the WINSOCK blocking hook.
     */
    LoadString (hInstance, IDS_APPIDENT, appIdent, sizeof(appIdent) / sizeof(TCHAR));
    if (_tcscmp (appIdent, APP_PINE_IDENT) == 0) {
	gAppIdent = APP_PINE;
	gWSBlockingProc = MakeProcInstance ( (FARPROC) NoMsgsAreSent,
					      hInstance);
    }
    else if (_tcscmp (appIdent, APP_PICO_IDENT) == 0)
	gAppIdent = APP_PICO;
    else
	gAppIdent = APP_UNKNOWN;


    return (hTTYWnd);
}


/*---------------------------------------------------------------------------
 *  void MakeArgv ()
 *
 *  Description:
 *	Build a standard C argc, argv pointers into the command line string.
 *
 *
 *  Parameters:
 *	cmdLine		- Command line.
 *	*argc		- Count of words.
 *	***argc		- Pointer to Pointer to array of pointers to
 *			  characters.
 *
 *--------------------------------------------------------------------------*/
LOCAL void
MakeArgv (HINSTANCE hInstance, char *cmdLine_utf8, int *pargc, CHAR ***pargv)
{
    CHAR		**argv;
    LPSTR		c;
    BOOL		inWord, inQuote;
    int			wordCount;
#define CMD_PATH_LEN    128
    LPTSTR		modPath_lptstr;
    DWORD		mpLen;


    /* Count words in cmdLine. */
    wordCount = 0;
    inWord = FALSE;
    inQuote = FALSE;
    for (c = cmdLine_utf8; *c != '\0'; ++c) {
	if (inQuote) {
	    if(*c == '"' && (*(c+1) == ' ' || *(c+1) == '\t' || *(c+1) == '\0')){
		inQuote = inWord = FALSE;
	    }
	}
	else {
	    if(inWord && (*c == ' ' || *c == '\t' || *c == '\0')){
		inWord = FALSE;
	    }
	    else if(!inWord && (*c != ' ' && *c != '\t')){
		inWord = TRUE;
		wordCount++;
		if(*c == '"')
		  inQuote = TRUE;
	    }
	}
    }

    ++wordCount;				/* One for program name. */
    argv = (CHAR **) MemAlloc (sizeof (CHAR *) * (wordCount + 1));
    *pargv = argv;
    *pargc = wordCount;

    modPath_lptstr = (LPTSTR) MemAlloc (CMD_PATH_LEN*sizeof(TCHAR));
    mpLen = GetModuleFileName (hInstance, modPath_lptstr, CMD_PATH_LEN);
    if (mpLen > 0) {
	*(modPath_lptstr + mpLen) = '\0';
        *(argv++) = (unsigned char *)lptstr_to_utf8(modPath_lptstr);
    }
    else
      *(argv++) = (unsigned char *)"Alpine/Pico";

    MemFree((void *)modPath_lptstr);

    /* Now break up command line. */
    inWord = FALSE;
    inQuote = FALSE;
    for (c = cmdLine_utf8; *c != '\0'; ++c) {
	if (inQuote) {
	    if(*c == '"' && (*(c+1) == ' ' || *(c+1) == '\t' || *(c+1) == '\0')){
		inQuote = inWord = FALSE;
		*c = '\0';
	    }
	}
	else {
	    if(inWord && (*c == ' ' || *c == '\t' || *c == '\0')){
		*c = '\0';
		inWord = FALSE;
	    }
	    else if(!inWord && (*c != ' ' && *c != '\t')){
		inWord = TRUE;
		if(*c == '"'){
		  inQuote = TRUE;
		  *(argv++) = (unsigned char *)c+1;
		}
		else
		  *(argv++) = (unsigned char *)c;
	    }
	}
    }

    *argv = NULL;			/* tie off argv */
}


/*---------------------------------------------------------------------------
 *  LRESULT FAR PASCAL __export PWndProc( HWND hWnd, UINT uMsg,
 *                                 WPARAM wParam, LPARAM lParam )
 *
 *  Description:
 *     This is the TTY Window Proc.  This handles ALL messages
 *     to the tty window.
 *
 *  Parameters:
 *     As documented for Window procedures.
 *
/*--------------------------------------------------------------------------*/
LRESULT FAR PASCAL __export
PWndProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
#ifdef CDEBUG
    if (mswin_debug > 12) {
        fprintf (mswin_debugfile, "PWndProc::  uMsg = 0x%x\n", uMsg);
	fflush (mswin_debugfile);
    }
#endif
    switch (uMsg) {
    case WM_CREATE:
	 MyTimerSet ();
         return (CreateTTYInfo (hWnd));

    case WM_COMMAND:
	switch ((WORD) wParam) {

        case IDM_OPT_SETFONT:
	    SelectTTYFont (hWnd);
	    break ;
	
	case IDM_OPT_FONTSAMEAS:
	    PrintFontSameAs (hWnd);
	    break;
	
	case IDM_OPT_TOOLBAR:
	    TBToggle (hWnd);
	    break;
	
	case IDM_OPT_TOOLBARPOS:
	    TBPosToggle (hWnd);
	    break;
	
	case IDM_OPT_USEDIALOGS: {
	    PTTYINFO	pTTYInfo;
	
	    gfUseDialogs = !gfUseDialogs;
	    pTTYInfo = (PTTYINFO)(LONG_PTR)MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
	    if (pTTYInfo)
		DidResize (pTTYInfo);
	    break;
            }

	case IDM_OPT_ERASE_CREDENTIALS:
	    if(gEraseCredsCallback)
	      (*gEraseCredsCallback)();
	    break;

	case IDM_MI_VIEWINWIND:
	  if(gViewInWindCallback)
	    (*gViewInWindCallback)();
	  break;

	case IDM_OPT_IMAPTELEM:
	    mswin_tw_init(&gMswinIMAPTelem, (int)LOWORD(wParam),
                TEXT("IMAP Telemetry"));
	    SetFocus (ghTTYWnd);
	    break;

	case IDM_OPT_NEWMAILWIN:
	    mswin_tw_init(&gMswinNewMailWin, (int)LOWORD(wParam),
                TEXT("New Mail View"));
	    SetFocus (ghTTYWnd);
	    break;

#ifdef ACCELERATORS_OPT
	case IDM_OPT_USEACCEL:
	    AccelCtl (hWnd, ACCEL_TOGGLE, TRUE);
	    break;
#endif	
	
	case IDM_OPT_SETPRINTFONT:
	    PrintFontSelect (hWnd);
	    break;

	case IDM_OPT_CARETBLOCK :
	  CaretTTY(hWnd, CaretBlock);
	  break;

	case IDM_OPT_CARETSMALLBLOCK :
	  CaretTTY(hWnd, CaretSmallBlock);
	  break;

	case IDM_OPT_CARETVBAR :
	  CaretTTY(hWnd, CaretVertBar);
	  break;

	case IDM_OPT_CARETHBAR :
	  CaretTTY(hWnd, CaretHorizBar);
	  break;

        case IDM_ABOUT:
            GoModalDialogBoxParam ( GET_HINST( hWnd ),
                                       MAKEINTRESOURCE( ABOUTDLGBOX ),
                                       hWnd,
                                       (DLGPROC)AboutDlgProc, (LPARAM) 0 ) ;
            break;
	
	case IDM_EDIT_CUT:
	    EditCut ();
	    break;
	
	case IDM_EDIT_COPY:
	    EditCopy ();
	    break;
	
	case IDM_EDIT_COPY_APPEND:
	    EditCopyAppend ();
	    break;
	
	case IDM_EDIT_PASTE:
	    EditPaste ();
#ifdef ACCELERATORS
	    UpdateAccelerators (hWnd);
#endif
	    break;
	
	case IDM_EDIT_CANCEL_PASTE:
	    EditCancelPaste ();
#ifdef ACCELERATORS
	    UpdateAccelerators (hWnd);
#endif
	    break;

	case IDM_EDIT_SEL_ALL :
	  EditSelectAll();
	  break;
	
	case IDM_HELP:
	    MSWHelpShow (gHelpCallback);
	    break;

	case IDM_MI_GENERALHELP:
	    MSWHelpShow (gHelpGenCallback);
	    break;

	case IDM_MI_WHEREIS :
	  CQAdd (gpTTYInfo->menuItems[KS_WHEREIS - KS_RANGESTART].miKey, 0);
	  break;

	case IDM_MI_SORTSUBJECT :
	case IDM_MI_SORTARRIVAL :
	case IDM_MI_SORTSIZE :
	case IDM_MI_SORTFROM :
	case IDM_MI_SORTTO :
	case IDM_MI_SORTCC :
	case IDM_MI_SORTDATE :
	case IDM_MI_SORTORDERSUB :
	case IDM_MI_SORTSCORE :
	case IDM_MI_SORTTHREAD :
	  SortHandler((int)(wParam - IDM_MI_SORTSUBJECT), 0);
	  break;
	
	case IDM_MI_SORTREVERSE :
	  SortHandler(-1, 1);
	  break;

	case IDM_MI_FLAGIMPORTANT :
	case IDM_MI_FLAGNEW :
	case IDM_MI_FLAGANSWERED :
	case IDM_MI_FLAGDELETED :
	  FlagHandler((int)(wParam - IDM_MI_FLAGIMPORTANT), 0);
	  break;

	default:
	    /* value falling within the menu item range are handled here. */
	    if (wParam >= KS_RANGESTART && wParam <= KS_RANGEEND){
		ProcessMenuItem (hWnd, wParam);
		break;
	    }
	    break;
        }
	break ;
	
    case WM_VSCROLL:
	ScrollTTY (hWnd, LOWORD(wParam), HIWORD(wParam), (HWND) lParam);
	break;

    case WM_MOUSEWHEEL:
	MouseWheelTTY (hWnd, LOWORD(lParam), HIWORD(lParam),
		       LOWORD(wParam), (short) HIWORD(wParam));
	break;

    case WM_ERASEBKGND:
	if (IsIconic (hWnd))
	    return (DefWindowProc (hWnd, WM_ICONERASEBKGND, wParam, lParam));
	else
	    EraseTTY (hWnd, (HDC) wParam);
	break;
	
    case WM_QUERYDRAGICON:
	return ((LRESULT)ghNormalIcon);

    case WM_PAINT:
	PaintTTY (hWnd);
        break ;
	
    case WM_GETMINMAXINFO:
	GetMinMaxInfoTTY (hWnd, (MINMAXINFO __far *)lParam);
	break;

    case WM_SIZE:
        SizeTTY (hWnd, (int)wParam, HIWORD(lParam), LOWORD(lParam));
	break ;

    case WM_SIZING :
	return(SizingTTY(hWnd, (int)wParam, (LPRECT) lParam));
	
    case WM_MOVE:
/*	MoveTTY (hWnd, (int) LOWORD(lParam), (int) HIWORD(lParam));  */
	break;
	
    case WM_WINDOWPOSCHANGING:
	/* Allows us to adjust new size of window. */
	AboutToSizeTTY (hWnd, (WINDOWPOS FAR *) lParam);
	break;


    case TASKBAR_ICON_MESSAGE:
	/* Notification of a mouse event in the task bar tray.
	 * If they are clicking on it we will restore the window. */
	if (lParam == WM_LBUTTONDOWN){
	    if(gpTTYInfo->fMinimized)
	      ShowWindow (hWnd, SW_RESTORE);
	    else
	      SetForegroundWindow(hWnd);
	}

	break;
		

    /*
     * WM_KEYDOWN is sent for every "key press" and reports on the
     * keyboard key, with out processing shift and control keys.
     * WM_CHAR is a synthetic event, created from KEYDOWN and KEYUP
     * events.  It includes processing or control and shift characters.
     * But does not get generated for extended keys suchs as arrow
     * keys.
     * I'm going to try to use KEYDOWN for processing just extended keys
     * and let CHAR handle the the rest.
     *
     * The only key combo that is special is ^-space.  For that, I'll use
     * WM_KEYDOWN and WM_KEYUP to track the state of the control key.
     */
    case WM_CHAR:
	/*
	 * If the windows cursor is visible and we have keyboard input
	 * then hide the windows cursor
	 */
	mswin_showcursor(FALSE);
        ProcessTTYCharacter (hWnd, (TCHAR)wParam);
        break ;
	
    case WM_KEYDOWN:
      if (ProcessTTYKeyDown (hWnd, (TCHAR) wParam))
	    return (0);

        return( DefWindowProc( hWnd, uMsg, wParam, lParam ) ) ;

    case WM_SYSCHAR:
	if (gFkeyCallback && (*gFkeyCallback)(0, 0)
	    && LOBYTE (wParam) == VK_F10){
	    ProcessTTYCharacter (hWnd, (TCHAR)wParam);
	    return (0);
        }

        return( DefWindowProc( hWnd, uMsg, wParam, lParam ) ) ;
	
    case WM_SYSKEYDOWN:
    /*
	 * lParam specifies the context code. Bit 29 is 1 if the ALT key is down
	 * while the key is pressed.
	 */
	if (!(lParam & (1<<29))
	    && gFkeyCallback && (*gFkeyCallback)(0, 0)
	    && LOBYTE (wParam) == VK_F10
	    && ProcessTTYKeyDown (hWnd, (TCHAR) wParam))
	  return (0);

        return( DefWindowProc( hWnd, uMsg, wParam, lParam ) ) ;
	

    case WM_LBUTTONDOWN:
	ProcessTTYMouse (hWnd, M_EVENT_DOWN, M_BUTTON_LEFT, LOWORD (lParam),
			 HIWORD (lParam), wParam);
	break;

    case WM_LBUTTONUP:
	if (ProcessTTYMouse (hWnd, M_EVENT_UP, M_BUTTON_LEFT, LOWORD (lParam),
			 HIWORD (lParam), wParam))
	    goto callDef;
	break;

    case WM_MBUTTONDOWN:
	ProcessTTYMouse (hWnd, M_EVENT_DOWN, M_BUTTON_MIDDLE, LOWORD (lParam),
		 HIWORD (lParam), wParam);
	break;

    case WM_MBUTTONUP:
	ProcessTTYMouse (hWnd, M_EVENT_UP, M_BUTTON_MIDDLE, LOWORD (lParam),
			 HIWORD (lParam), wParam);
	break;

    case WM_RBUTTONDOWN:
	ProcessTTYMouse (hWnd, M_EVENT_DOWN, M_BUTTON_RIGHT, LOWORD (lParam),
			 HIWORD (lParam), wParam);
	break;

    case WM_RBUTTONUP:
	ProcessTTYMouse (hWnd, M_EVENT_UP, M_BUTTON_RIGHT, LOWORD (lParam),
			 HIWORD (lParam), wParam);
	break;
	
    case WM_MOUSEMOVE:
	ProcessTTYMouse (hWnd, M_EVENT_TRACK, 0, LOWORD (lParam),
			HIWORD (lParam), wParam);
	break;

    case WM_NCMOUSEMOVE:
	mswin_showcursor(TRUE);
	goto callDef;		/* pretend it never happened */

    case WM_SETFOCUS:
        SetTTYFocus (hWnd);
        break;

    case WM_KILLFOCUS:
        KillTTYFocus (hWnd);
        break;
	
    case WM_SETCURSOR:
	/* Set cursor.  If in client, leave as is.  Otherwise, pass to
	 * DefWindow Proc */
	if (LOWORD(lParam) == HTCLIENT) {
	    SetCursor (ghCursorCurrent);
	    return (TRUE);
        }

        return( DefWindowProc( hWnd, uMsg, wParam, lParam ) ) ;

    case WM_INITMENU:
	UpdateMenu (hWnd);
	break;
	
    case WM_TIMER:
	/* Really just used so that we continue to receive messages even while
	 * in background.  Causes mswin_getc() to process message and return
	 * to caller so that it can get some periodic processing in. */
	ProcessTimer ();
	break;
	
    case WM_QUERYENDSESSION:
	/* Returns non-zero if I can exit, otherwize zero, and the end
	 * session operation stops. */
	return ((LRESULT)ConfirmExit ());
	
    case WM_DESTROY:
	KillTimer (hWnd, MY_TIMER_ID);
	DestroyTTYInfo (hWnd);
	PostQuitMessage (0);
	break;


    case WM_DROPFILES:
	if(ProcessTTYFileDrop((HANDLE) wParam) == TRUE)
	  SetForegroundWindow(hWnd);

       break;


    case WM_CLOSE:
	/* If the quit menu is active then insert the quit command
	 * Otherwise, abort. */
	if (gpTTYInfo->menuItems[KS_EXIT - KS_RANGESTART].miActive) {
	    CQAdd (gpTTYInfo->menuItems[KS_EXIT - KS_RANGESTART].miKey, 0);
	}
	else if (gSignalHUP != SIG_DFL && gSignalHUP != SIG_IGN) {
	    if (MessageBox (hWnd,
			    TEXT("Abort PINE/PICO, possibly losing current work?"),
			    gszAppName, MB_OKCANCEL | MB_ICONQUESTION) == IDOK)
		HUPDeliver ();
	}
	break;	


    default:
callDef:	
        return( DefWindowProc( hWnd, uMsg, wParam, lParam ) ) ;
    }
    return 0L ;

} /* end of PWndProc() */


/*---------------------------------------------------------------------------
 *  LRESULT NEAR CreateTTYInfo( HWND hWnd )
 *
 *  Description:
 *     Creates the tty information structure and sets
 *     menu option availability.  Returns -1 if unsuccessful.
 *
 *  Parameters:
 *     HWND  hWnd
 *        Handle to main window.
 *
 *-------------------------------------------------------------------------*/
LOCAL LRESULT NEAR
CreateTTYInfo (HWND hWnd)
{
    HMENU		hMenu;
    PTTYINFO		pTTYInfo;
    LOGFONT		newFont;
    int			i, ppi;
    HDC			hDC;
    HFONT		testFont;
#ifdef SDEBUG
    if (mswin_debug >= 5)
	fprintf (mswin_debugfile, "CreateTTYInfo:::  entered\n");
#endif

    hDC = GetDC (ghTTYWnd);
    ppi = GetDeviceCaps (hDC, LOGPIXELSY);
    ReleaseDC (ghTTYWnd, hDC);

    pTTYInfo = (PTTYINFO) MemAlloc (sizeof (TTYINFO));
    if (pTTYInfo == NULL)
	return ((LRESULT) - 1);
    gpTTYInfo = pTTYInfo;

    /* initialize TTY info structure */
    memset (pTTYInfo, 0, sizeof (TTYINFO));
    /* Shown but not focused. */
    pTTYInfo->cCaretStyle		= CaretBlock;
    pTTYInfo->fMinimized		= FALSE;
    pTTYInfo->fMaximized		= FALSE;
    pTTYInfo->fFocused			= FALSE;
    pTTYInfo->fNewLine			= FALSE;
    pTTYInfo->fMassiveUpdate		= FALSE;
    pTTYInfo->fNewMailIcon		= FALSE;
    pTTYInfo->fMClosedIcon		= FALSE;
    pTTYInfo->autoWrap			= WRAP_NO_SCROLL;
    pTTYInfo->xOffset			= MARGINE_LEFT;
    pTTYInfo->yOffset			= MARGINE_TOP;
    pTTYInfo->fDesiredSize		= FALSE;
    pTTYInfo->fCursorOn			= TRUE;
    pico_nfcolor(NULL);
    pico_nbcolor(NULL);
    pico_rfcolor(NULL);
    pico_rbcolor(NULL);
    pico_set_normal_color();
    pTTYInfo->toolBarTop		= TRUE;
    pTTYInfo->curToolBarID		= IDD_TOOLBAR;

    /* Clear menu item array. */
    pTTYInfo->curWinMenu = ALPINEMENU;
    for (i = 0; i < KS_COUNT; ++i)
	pTTYInfo->menuItems[i].miActive = FALSE;
    pTTYInfo->menuItemsCurrent = FALSE;

    /* Clear resize callback procs. */
    for (i = 0; i < RESIZE_CALLBACK_ARRAY_SIZE; ++i)
	pTTYInfo->resizer[i] = NULL;


    /* clear screen space */
    pTTYInfo->pScreen = NULL;
    pTTYInfo->pCellWidth = NULL;
    pTTYInfo->pAttrib = NULL;

    /* setup default font information */

    newFont.lfHeight =         -MulDiv(12, ppi, 72);
    newFont.lfWidth =          0;
    newFont.lfEscapement =     0;
    newFont.lfOrientation =    0;
    newFont.lfWeight =         0;
    newFont.lfItalic =         0;
    newFont.lfUnderline =      0;
    newFont.lfStrikeOut =      0;
    newFont.lfCharSet =        FONT_CHARSET_FONT;
    newFont.lfOutPrecision =   OUT_DEFAULT_PRECIS;
    newFont.lfClipPrecision =  CLIP_DEFAULT_PRECIS;
    newFont.lfQuality =        DEFAULT_QUALITY;
    newFont.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    _sntprintf(newFont.lfFaceName, LF_FACESIZE, TEXT("%s"), TEXT("Courier New"));
    testFont = CreateFontIndirect(&newFont);
    if(NULL == testFont)
      newFont.lfFaceName[0] =    '\0';
    else
      DeleteObject(testFont);

    /* set TTYInfo handle before any further message processing. */

    MySetWindowLongPtr (hWnd, GWL_PTTYINFO, pTTYInfo);

    /* reset the character information, etc. */

    ResetTTYFont (hWnd, pTTYInfo, &newFont);



    hMenu = GetMenu (hWnd);
    EnableMenuItem (hMenu, IDM_EDIT_CUT, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem (hMenu, IDM_EDIT_COPY, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem (hMenu, IDM_EDIT_COPY_APPEND, MF_BYCOMMAND | MF_GRAYED);
    EnableMenuItem (hMenu, IDM_EDIT_PASTE, MF_BYCOMMAND | MF_GRAYED);
    return ((LRESULT) TRUE);
}


/*---------------------------------------------------------------------------
 *  BOOL NEAR DestroyTTYInfo( HWND hWnd )
 *
 *  Description:
 *     Destroys block associated with TTY window handle.
 *
 *  Parameters:
 *     HWND hWnd
 *        handle to TTY window
 *
 *-------------------------------------------------------------------------*/
LOCAL BOOL NEAR
DestroyTTYInfo (HWND hWnd)
{
    PTTYINFO			pTTYInfo;

    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
    if (pTTYInfo == NULL)
	return (FALSE);

#ifdef	ACCELERATORS
    if(pTTYInfo->hAccel){
	DestroyAcceleratorTable(pTTYInfo->hAccel);
	pTTYInfo->hAccel = NULL;
	pTTYInfo->fAccel = EM_NONE;
    }
#endif

    if(pTTYInfo->hTBWnd != NULL)
      DestroyWindow (pTTYInfo->hTBWnd);

    if(pTTYInfo->hTBBrush != NULL)
      DeleteObject(pTTYInfo->hTBBrush);

    DeleteObject (pTTYInfo->hTTYFont);

    MemFree (pTTYInfo);
    return (TRUE);
}


/*---------------------------------------------------------------------------
 *  void  ResizeTTYScreen( HWND hWnd, PTTYINFO pTTYInfo,
 *					int newNrow, int newNColumn);
 *
 *  Description:
 *		Resize the screen to new size, copying data.
 *
 *  Parameters:
 *     PTTYINFO  pTTYInfo
 *        pointer to TTY info structure
 *		newNCo.umn, newNRow
 *			new size of screen.
 *
/*--------------------------------------------------------------------------*/
LOCAL BOOL
ResizeTTYScreen (HWND hWnd, PTTYINFO pTTYInfo, int newNRow, int newNColumn)
{
    CharAttrib		*pNewAttrib, tmpAttrib, *pSourceAtt, *pDestAtt;
    TCHAR		*pNewScreen, *pSource, *pDest;
    int                 *pNewCW, *pSourceCW, *pDestCW;
    size_t		 len;
    int			 cells;
    int			 r, i;
    extern TERM		 term;


    if (newNColumn < MINNCOLUMN)
	    newNColumn = MINNCOLUMN;
    if (newNRow < MINNROW)
	    newNRow = MINNROW;

#ifdef SDEBUG
    if (mswin_debug >= 5)
	fprintf (mswin_debugfile, "ResizeTTYScreen:::  entered, new row %d, col %d\n",
		newNRow, newNColumn);
#endif

	
    SelClear ();
    cells = newNColumn * newNRow;
    pNewScreen = (TCHAR *)MemAlloc (cells * sizeof (TCHAR));
    if (pNewScreen == NULL)
	return (FALSE);

    pNewCW = (int *)MemAlloc(cells * sizeof(int));
    if(pNewCW == NULL){
	MemFree((void *)pNewScreen);
	return(FALSE);
    }

    pNewAttrib = (CharAttrib *)MemAlloc (cells * sizeof (CharAttrib));
    if (pNewAttrib == NULL) {
	MemFree ((void *)pNewScreen);
	MemFree ((void *)pNewCW);
	return (FALSE);
    }


    /*
     * Clear new screen.
     */

    for(i = 0; i < cells; i++){
	pNewScreen[i] = ' ';
	pNewCW[i] = pTTYInfo->xChar;  /* xChar set yet ? */
    }

    tmpAttrib.style = CHAR_ATTR_NORM;
    tmpAttrib.rgbFG = pTTYInfo->rgbFGColor;
    tmpAttrib.rgbBG = pTTYInfo->rgbBGColor;
    for(r = 0; r < cells; r++)
	pNewAttrib[r] = tmpAttrib;

    /*
     * Copy old screen onto new screen.
     */
    if (pTTYInfo->pScreen != NULL) {

	for (r = 1; r <= newNRow && r <= pTTYInfo->actNRow; ++r) {
	    pSource = pTTYInfo->pScreen + ((pTTYInfo->actNRow - r) *
						    pTTYInfo->actNColumn);
	    pDest = pNewScreen + ((newNRow - r) * newNColumn);
	    len = MIN (newNColumn, pTTYInfo->actNColumn);
	    for(i = 0; i < len; i++)
	      pDest[i] = pSource[i];

	    pSourceCW = pTTYInfo->pCellWidth
	      + ((pTTYInfo->actNRow - r) * pTTYInfo->actNColumn);
	    pDestCW = pNewCW + ((newNRow - r) * newNColumn);
	    memcpy(pDestCW, pSourceCW, len * sizeof(int));

	    pSourceAtt = pTTYInfo->pAttrib
	      + ((pTTYInfo->actNRow - r) * pTTYInfo->actNColumn);
	    pDestAtt = pNewAttrib + ((newNRow - r) * newNColumn);
	    len = MIN (newNColumn, pTTYInfo->actNColumn);
	    memcpy (pDestAtt, pSourceAtt, len * sizeof(CharAttrib));
	}

	pTTYInfo->nColumn = (CORD)MIN (pTTYInfo->nColumn, newNColumn);
	pTTYInfo->nRow = (CORD)MAX (0,
			pTTYInfo->nRow + (newNRow - pTTYInfo->actNRow));
	MemFree (pTTYInfo->pScreen);
	MemFree (pTTYInfo->pCellWidth);
	MemFree (pTTYInfo->pAttrib);
    }
    else {
	pTTYInfo->nColumn = (CORD)MIN (pTTYInfo->nColumn, newNColumn);
	pTTYInfo->nRow = (CORD)MIN (pTTYInfo->nRow, newNRow);
    }

    pTTYInfo->pScreen = pNewScreen;
    pTTYInfo->pCellWidth = pNewCW;
    pTTYInfo->pAttrib = pNewAttrib;
    pTTYInfo->actNColumn = newNColumn;
    pTTYInfo->actNRow = newNRow;


    /* Repaint whole screen. */
    pTTYInfo->screenDirty = TRUE;
    pTTYInfo->eraseScreen = TRUE;
    InvalidateRect (hWnd, NULL, FALSE);



    /* Pico specific. */
    if (term.t_nrow == 0) {
	term.t_nrow = (short)(newNRow - 1);
	term.t_ncol = (short)newNColumn;
    }


    return (TRUE);
}


/*---------------------------------------------------------------------------
 *  BOOL  ResetTTYFont( HWND hWnd, PTTYINFO pTTYInfo, LOGFONT *newFont)
 *
 *  Description:
 *     Resets the TTY character information and causes the
 *     screen to resize to update the scroll information.
 *
 *  Parameters:
 *     PTTYINFO  pTTYInfo
 *        pointer to TTY info structure
 *
/*--------------------------------------------------------------------------*/
LOCAL BOOL
ResetTTYFont (HWND hWnd, PTTYINFO pTTYInfo, LOGFONT *newFont)
{
    HDC			hDC;
    HFONT		hFont;
    TEXTMETRIC		tm;
    int			newNRow;
    int			newNColumn;
    BOOL		newsize;


#ifdef SDEBUG
    if (mswin_debug >= 5)
	fprintf (mswin_debugfile, "ResetTTYFont:::  entered, curent window size X %d, Y %d\n",
		pTTYInfo->xSize, pTTYInfo->ySize);
#endif


    if (NULL == pTTYInfo)
	    return (FALSE);

    SelClear ();

    /*
     * Create new font.
     */
    hFont = CreateFontIndirect (newFont);
    if (hFont == NULL)
	    return (FALSE);
    hDC = GetDC (hWnd);
    SelectObject (hDC, hFont);
    GetTextMetrics (hDC, &tm);
    ReleaseDC (hWnd, hDC);


    /*
     * Replace old font.
     */
    if (NULL != pTTYInfo->hTTYFont)
	    DeleteObject (pTTYInfo->hTTYFont);
    pTTYInfo->hTTYFont = hFont;
    memcpy (&pTTYInfo->lfTTYFont, newFont, sizeof (LOGFONT));


    /* Update the char cell size. */
    pTTYInfo->xChar = (CORD)tm.tmAveCharWidth;
    pTTYInfo->yChar = (CORD)(tm.tmHeight + tm.tmExternalLeading);

    /* Update the current number of rows and cols.  Don't allow
     * either to be less than zero. */
    newNRow = MAX (MINNROW,
	    MIN (MAXNROW,
		(pTTYInfo->ySize - pTTYInfo->toolBarSize - (2 * MARGINE_TOP))/
				pTTYInfo->yChar));
    newNColumn = MAX (MINNCOLUMN,
	    MIN (MAXNCOLUMN, (pTTYInfo->xSize - (2 * pTTYInfo->xOffset))/
				    pTTYInfo->xChar));

    newsize = newNRow != pTTYInfo->actNRow ||
		    newNColumn != pTTYInfo->actNColumn;
    if (newsize)
	    ResizeTTYScreen (hWnd, pTTYInfo, newNRow, newNColumn);

    /* Resize the caret as well. */
    if(pTTYInfo->fCaretOn)
      HideCaret (hWnd);

    DestroyCaret ();
    CaretCreateTTY (hWnd);

    /* Redraw screen and, if the "size" changed, tell the upper layers. */
    pTTYInfo->screenDirty = TRUE;
    pTTYInfo->eraseScreen = TRUE;
    InvalidateRect (hWnd, NULL, FALSE);

    /* Always call the resize functions - even if the screen size
     * has not changed, the font style may have. */
    DidResize (pTTYInfo);

    return (TRUE);
}


/*---------------------------------------------------------------------------
 *  BOOL  EraseTTY (HWND hWnd, HDC hDC)
 *
 *  Description:
 *     Erase the tty background.
 *
 *
 *  Parameters:
 *     HWND hWnd
 *        handle to TTY window (as always)
 *
/*--------------------------------------------------------------------------*/
LOCAL BOOL
EraseTTY (HWND hWnd, HDC hDC)
{
    RECT	erect;
    HBRUSH	hBrush;


    GetClientRect (hWnd, &erect);
    hBrush = CreateSolidBrush (gpTTYInfo->rgbBGColor);
    if (hBrush != NULL) {
	FillRect (hDC, &erect, hBrush);
	DeleteObject (hBrush);
    }
    return (TRUE);
}


/*---------------------------------------------------------------------------
 *  BOOL  PaintTTY( HWND hWnd )
 *
 *  Description:
 *     Paints the rectangle determined by the paint struct of
 *     the DC.
 *
 *  Parameters:
 *     HWND hWnd
 *        handle to TTY window (as always)
 *
/*--------------------------------------------------------------------------*/
LOCAL BOOL
PaintTTY (HWND hWnd)
{
    int          nRow, nCol;		/* Top left corner of update. */
    int		 nEndRow, nEndCol;	/* lower right corner of update. */
    int		 nHorzPos, nVertPos;	/* Position of each text write. */
    int		 col;			/* start col of run of similar attr */
    int		 count;			/* count of run of similar attrib. */
    int		 endCount;		/* How far to count. */
    CharAttrib	*pAttrib;
    HDC          hDC;
    LOGFONT	 tmpFont;
    HFONT        hOrigFont, hOldFont = NULL, hTmpFont;
    PTTYINFO    pTTYInfo;
    PAINTSTRUCT  ps;
    RECT         rect;
    RECT	 erect;
    HBRUSH	 hBrush;
    long	 offset;		/* Offset into pScreen array */
    long         endoffset;		/* Offset of nEndCol in each row array */
    CharAttrib	*pLastAttrib;		/* Attributes of last text write. */
    CharAttrib	*pNewAttrib;		/* Attributes of this text write. */


#ifdef CDEBUG
    if (mswin_debug >= 9)
	fprintf (mswin_debugfile, "PaintTTY:::  entered\n");
#endif


    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
    if (pTTYInfo == NULL)
	return (FALSE);

    if (IsIconic (hWnd))
      return (TRUE);

    hDC = BeginPaint (hWnd, &ps);
    rect = ps.rcPaint;

    hOrigFont = SelectObject (hDC, pTTYInfo->hTTYFont);
    SetTextColor (hDC, pTTYInfo->rgbFGColor);
    SetBkColor (hDC, pTTYInfo->rgbBGColor);
    SetBkMode (hDC, OPAQUE);

    nRow = (rect.top - pTTYInfo->yOffset) / pTTYInfo->yChar;
    CONSTRAIN (nRow, 0, pTTYInfo->actNRow - 1);

    nEndRow = MIN(pTTYInfo->actNRow - 1,
	    ((rect.bottom - pTTYInfo->yOffset - 1) / pTTYInfo->yChar));
    nCol = MIN(pTTYInfo->actNColumn - 1,
	    MAX(0, (rect.left - pTTYInfo->xOffset) / pTTYInfo->xChar));
    nEndCol = MIN(pTTYInfo->actNColumn - 1,
	    ((rect.right - pTTYInfo->xOffset - 1) / pTTYInfo->xChar));

    pLastAttrib = NULL;

    /* Erase screen if necessary. */
    if (pTTYInfo->eraseScreen) {
	erect.top = 0;
	erect.left = 0;
	erect.bottom = pTTYInfo->ySize;
	erect.right = pTTYInfo->xSize;
	hBrush = CreateSolidBrush (pTTYInfo->rgbBGColor);
	if (hBrush != NULL) {
	    FillRect (hDC, &erect, hBrush);
	    DeleteObject (hBrush);
	}
	pTTYInfo->eraseScreen = FALSE;
    }


    /* Paint an inset frame around the text region. */
    if (pTTYInfo->toolBarSize == 0) {
	erect.top = 0;
	erect.bottom = pTTYInfo->ySize;
    }
    else if (pTTYInfo->toolBarTop) {
	erect.top = pTTYInfo->toolBarSize;
	erect.bottom = pTTYInfo->ySize;
    }
    else {
	erect.top = 0;
	erect.bottom = pTTYInfo->ySize - pTTYInfo->toolBarSize;
    }
    erect.left = 0;
    erect.right = pTTYInfo->xSize;
    FrameRect3D (hDC, &erect, FRAME_3D_SIZE, FALSE);

    /* Paint rows of text. */
    for (; nRow <= nEndRow; nRow++)    {
	nVertPos = (nRow * pTTYInfo->yChar) + pTTYInfo->yOffset;
	rect.top = nVertPos;
	rect.bottom = nVertPos + pTTYInfo->yChar;
	
	/* Paint runs of similar attributes. */
	col = nCol;				/* Start at left. */

	if(col == 0 && MSWIconPaint(nRow, hDC))
	  col += 2;

	/*
	 * col is the column on the screen, not the index
	 * into the array.
	 */
	while (col <= nEndCol) {		/* While not past right. */

	    /* Starting with Character at nRow, col, what is its attribute? */

	    /* offset is an index into the array */
	    offset = pscreen_offset_from_cord(nRow, col, pTTYInfo);
	    pNewAttrib = pTTYInfo->pAttrib + offset;

	    hTmpFont = NULL;
	    if (!(pLastAttrib
		  && pNewAttrib->style == pLastAttrib->style
		  && pNewAttrib->rgbFG == pLastAttrib->rgbFG
		  && pNewAttrib->rgbBG == pLastAttrib->rgbBG)) {
		/*
		 * Require new font?
		 */
		if(!pLastAttrib
		   || (pNewAttrib->style & CHAR_ATTR_ULINE)
				 != (pLastAttrib->style & CHAR_ATTR_ULINE)){
		    if(pNewAttrib->style & CHAR_ATTR_ULINE){
			/*
			 * Find suitable attribute font...
			 */
			memcpy (&tmpFont, &pTTYInfo->lfTTYFont,
				sizeof (LOGFONT));

			tmpFont.lfHeight = - pTTYInfo->yChar;
			tmpFont.lfWidth  = - pTTYInfo->xChar;

			tmpFont.lfUnderline = (BYTE)((pNewAttrib->style
							    & CHAR_ATTR_ULINE)
							    == CHAR_ATTR_ULINE);

			hTmpFont = CreateFontIndirect (&tmpFont);

			hOldFont = SelectObject (hDC, hTmpFont);
		    }
		}

		/*
		 * Set new color attributes.  If Reverse or Selected, then
		 * show in reverse colors.  But if neither, or both, then
		 * normal colors.
		 */
		if(pNewAttrib->style & CHAR_ATTR_SEL){
		    SetTextColor (hDC, pNewAttrib->rgbBG);
		    SetBkColor (hDC, pNewAttrib->rgbFG);
		}
		else {
		    if(!(pLastAttrib
			 && pNewAttrib->rgbFG == pLastAttrib->rgbFG)
		       || (pLastAttrib->style & CHAR_ATTR_SEL))
		      SetTextColor (hDC, pNewAttrib->rgbFG);

		    if(!(pLastAttrib
			 && pNewAttrib->rgbBG == pLastAttrib->rgbBG)
		       || (pLastAttrib->style & CHAR_ATTR_SEL))
		      SetBkColor (hDC, pNewAttrib->rgbBG);
		}
	    }
	
	    /* Find run of similar attributes. */
	    count = 1;
	    pAttrib = pTTYInfo->pAttrib + (offset + 1);
	    /* endoffset is an index into the pScreen array */
	    endoffset = pscreen_offset_from_cord(nRow, nEndCol, pTTYInfo);
	    endCount = endoffset - offset;
	    while (count <= endCount
		   && pAttrib->style == pNewAttrib->style
		   && pAttrib->rgbFG == pNewAttrib->rgbFG
		   && pAttrib->rgbBG == pNewAttrib->rgbBG){
		++pAttrib;
		++count;
	    }

	    if(hTmpFont != NULL){
/* BUG: compute new offsets based on hTmpFont font if required */
		nHorzPos = (col * pTTYInfo->xChar) + pTTYInfo->xOffset;
		rect.left = nHorzPos;
		rect.right = nHorzPos + pTTYInfo->xChar * scrwidth(pTTYInfo->pScreen+offset, count);
	    }
	    else{
		/* Paint run of characters from nRow, col to nRow, col + count
		 * rect.top and rect.bottom have already been calculated. */
		nHorzPos = (col * pTTYInfo->xChar) + pTTYInfo->xOffset;
		rect.left = nHorzPos;
		rect.right = nHorzPos + pTTYInfo->xChar * scrwidth(pTTYInfo->pScreen+offset, count);
	    }

	    ExtTextOut (hDC, nHorzPos, nVertPos, ETO_OPAQUE | ETO_CLIPPED,
			&rect, (LPTSTR) (pTTYInfo->pScreen + offset),
			count, (int *)(pTTYInfo->pCellWidth+offset));

	    /* Overstrike bold chars by hand to preserve char cell size */
	    if(pNewAttrib->style & CHAR_ATTR_BOLD){
		int old_mode = GetBkMode(hDC);
		SetBkMode (hDC, TRANSPARENT);
		ExtTextOut (hDC, nHorzPos + 1, nVertPos, 0,
			    &rect, (LPTSTR) (pTTYInfo->pScreen + offset),
			    count, (int *)(pTTYInfo->pCellWidth+offset));
		if(old_mode)
		  SetBkMode (hDC, old_mode);
	    }

	    /* Move pointer to end of this span of characters. */
	    col += MAX(scrwidth(pTTYInfo->pScreen+offset, count), 1);
	    pLastAttrib = pNewAttrib;

	    if(hTmpFont != NULL){
		SelectObject(hDC, hOldFont);
		DeleteObject(hTmpFont);
	    }
	}
    }

    SelectObject (hDC, hOrigFont);
    EndPaint (hWnd, &ps);
    MoveTTYCursor (hWnd);
    pTTYInfo->screenDirty = FALSE;
    return (TRUE);
}


/* FillRectColor
 *
 *
 * Description:
 *		FillRectColor is similar to PatB in toolbar.c
 *
 *		Code based on MFC source code, so presumably efficient.
 *
 */
LOCAL void
FillRectColor(HDC hDC, RECT * pRC, COLORREF color)
{
    SetBkColor(hDC, color);
    ExtTextOut(hDC, 0, 0, ETO_OPAQUE, pRC, NULL, 0, NULL);
}


/** FrameRect3D
 *
 *
 * Inputs:
 *		hdc			- HDC
 *		pRC			- pointer to rectangle
 *		width		- width for frame (usually one)
 *		raised		- TRUE for raised effect, FALSE for sunken effect
 *
 * Outputs:
 *		none
 *
 * Returns:
 *		void
 *
 * Description
 *		Draws a frame with a 3D effect.
 *
 *		If 'raised' is true, the rectangle will look raised (like
 *		a button); otherwise, the rectangle will look sunk.
 *
 */
void
FrameRect3D(HDC hdc, RECT * pRC, int width, BOOL raised)
{
    COLORREF	hilite, shadow;
    RECT		rcTemp;

    shadow		= GetSysColor(COLOR_BTNSHADOW);
    hilite		= GetSysColor(COLOR_BTNHIGHLIGHT);

    rcTemp		= *pRC;

    rcTemp.right	= rcTemp.left + width;
    FillRectColor(hdc, &rcTemp, raised ? hilite : shadow);
    rcTemp.right	= pRC->right;

    rcTemp.bottom	= rcTemp.top + width;
    FillRectColor(hdc, &rcTemp, raised ? hilite : shadow);
    rcTemp.bottom	= pRC->bottom;

    rcTemp.left		= rcTemp.right - width;
    FillRectColor(hdc, &rcTemp, raised ? shadow : hilite);
    rcTemp.left		= pRC->left;

    rcTemp.top		= rcTemp.bottom - width;
    FillRectColor(hdc, &rcTemp, raised ? shadow : hilite);
}


/*---------------------------------------------------------------------------
 *  BOOL  GetMinMaxInfoTTY (HWND hWnd, (MINMAXINFO __far *)lParam)
 *
 *  Description:
 *     Return the min and max size that the window can be.
 *
 *  Parameters:
 *     HWND hWnd
 *        handle to TTY window
 *
 *     MINMAXINFO
 *	  Info structure that Windows would like us to fill.
 *
/*--------------------------------------------------------------------------*/
LOCAL BOOL
GetMinMaxInfoTTY (HWND hWnd, MINMAXINFO __far *lpmmi)
{
    PTTYINFO		pTTYInfo;


#ifdef SDEBUG
    if (mswin_debug >= 5)
	fprintf (mswin_debugfile, "GetMinMaxInfoTTY:::  entered\n");
#endif


    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
    if (pTTYInfo == NULL)
	return (FALSE);

    lpmmi->ptMaxTrackSize.x = lpmmi->ptMaxSize.x = MIN (lpmmi->ptMaxSize.x,
			    pTTYInfo->xChar * MAXNCOLUMN + WIN_X_BORDER_SIZE);
    lpmmi->ptMaxTrackSize.y = lpmmi->ptMaxSize.y = MIN (lpmmi->ptMaxSize.y,
			    pTTYInfo->yChar * MAXNROW + WIN_Y_BORDER_SIZE);

    lpmmi->ptMinTrackSize.x = MAX (WIN_MIN_X_SIZE,
		    pTTYInfo->xChar * MINNCOLUMN + WIN_X_BORDER_SIZE);
    lpmmi->ptMinTrackSize.y = MAX (WIN_MIN_Y_SIZE,
		    pTTYInfo->yChar * MINNROW + WIN_Y_BORDER_SIZE);
    return (TRUE);
}


/*---------------------------------------------------------------------------
 *  BOOL  AboutToSizeTTY (HWND hWnd, WINDOWPOS *winPos)
 *
 *  Description:
 *      Called just before Windows resizes our window.  We can change the
 *	values in 'winPos' to change the new size of the window.
 *
 *	If mswin_setwindow() was called when the window was minimized we
 *	set the new size here.
 *
 *  Parameters:
 *     HWND hWnd
 *        handle to TTY window
 *
 *     WORD wVertSize
 *        new vertical size
 *
 *     WORD wHorzSize
 *        new horizontal size
 *
/*--------------------------------------------------------------------------*/
LOCAL BOOL
AboutToSizeTTY (HWND hWnd, WINDOWPOS *winPos)
{
    PTTYINFO	pTTYInfo;


    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
    if (pTTYInfo == NULL)
	    return ( FALSE );

#ifdef SDEBUG
    if (mswin_debug >= 5)
	fprintf (mswin_debugfile, "AboutToSizeTTY:::  After x%lx, pos %d, %d, size %d, %d, flags x%x\n",
	    winPos->hwndInsertAfter, winPos->x, winPos->y, winPos->cx,
	    winPos->cy, winPos->flags);

#endif

    /*
     * Was the window minimized AND is there a desired new size for it?
     * AND is this a call that specifies a new size and position.
     */
    if (pTTYInfo->fMinimized && pTTYInfo->fDesiredSize &&
	    (winPos->flags & (SWP_NOSIZE | SWP_NOMOVE)) == 0) {
#ifdef SDEBUG
	if (mswin_debug >= 5)
	    fprintf (mswin_debugfile, "AboutToSizeTTY:::  substitue pos (%d, %d), size (%d, %d)\n",
		    pTTYInfo->xDesPos, pTTYInfo->yDesPos,
		    pTTYInfo->xDesSize, pTTYInfo->yDesSize);
#endif
	pTTYInfo->fDesiredSize = FALSE;
	winPos->x = pTTYInfo->xDesPos;
	winPos->y = pTTYInfo->yDesPos;
	winPos->cx = pTTYInfo->xDesSize;
	winPos->cy = pTTYInfo->yDesSize;
    }
    return (TRUE);
}


/*---------------------------------------------------------------------------
 *  BOOL  SizeTTY( HWND hWnd, int fwSizeType, CORD wVertSize,
 *					CORD wHorzSize)
 *
 *  Description:
 *     Sizes TTY and sets up scrolling regions.
 *
 *  Parameters:
 *     HWND hWnd
 *        handle to TTY window
 *
 *     WORD wVertSize
 *        new vertical size
 *
 *     WORD wHorzSize
 *        new horizontal size
 *
/*--------------------------------------------------------------------------*/
LOCAL BOOL
SizeTTY (HWND hWnd, int fwSizeType, CORD wVertSize, CORD wHorzSize)
{
    PTTYINFO	pTTYInfo;
    int		newNColumn;
    int		newNRow;


#ifdef SDEBUG
    if (mswin_debug >= 5)
	fprintf (mswin_debugfile, "SizeTTY:::  entered, sizeType %d, New screen size %d, %d pixels\n",
		fwSizeType, wHorzSize, wVertSize);
#endif

    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
    if (pTTYInfo == NULL)
	    return ( FALSE );


    /*
     * Is the window being minimized or maximized?
     */
    switch (fwSizeType) {
    case SIZE_MINIMIZED:
	pTTYInfo->fMinimized = TRUE;
	pTTYInfo->fMaximized = FALSE;
	return (TRUE);
    case SIZE_MAXIMIZED:
	pTTYInfo->fMinimized = FALSE;
	pTTYInfo->fMaximized = TRUE;
	break;
    default:
        pTTYInfo->fMinimized = pTTYInfo->fMaximized = FALSE;
	break;
    }
	
    pTTYInfo->ySize = (CORD) wVertSize;
    newNRow = MAX(MINNROW, MIN(MAXNROW,
	    (pTTYInfo->ySize - pTTYInfo->toolBarSize - (2 * MARGINE_TOP)) /
				    pTTYInfo->yChar));
    if (pTTYInfo->toolBarTop)		
	pTTYInfo->yOffset = MARGINE_TOP + pTTYInfo->toolBarSize;
    else
	pTTYInfo->yOffset = MARGINE_TOP;
	

    pTTYInfo->xSize = (CORD) wHorzSize;
    newNColumn = MAX(MINNCOLUMN,
		    MIN(MAXNCOLUMN, (pTTYInfo->xSize - (2 * MARGINE_LEFT)) /
		    pTTYInfo->xChar));
    pTTYInfo->xOffset = MARGINE_LEFT;

    if(newNRow == pTTYInfo->actNRow && newNColumn == pTTYInfo->actNColumn)
      return(FALSE);

    ResizeTTYScreen (hWnd, pTTYInfo, newNRow, newNColumn);
    pTTYInfo->screenDirty = TRUE;
    pTTYInfo->eraseScreen = TRUE;
    InvalidateRect (hWnd, NULL, FALSE);

    if (pTTYInfo->hTBWnd) {
	if (pTTYInfo->toolBarTop)
	    /* Position at top of window. */
	    SetWindowPos (pTTYInfo->hTBWnd, HWND_TOP,
		    0, 0,
		    wHorzSize, pTTYInfo->toolBarSize,
		    SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
	else
	    /* Position at bottom of window. */
	    SetWindowPos (pTTYInfo->hTBWnd, HWND_TOP,
		    0, pTTYInfo->ySize - pTTYInfo->toolBarSize,
		    wHorzSize, pTTYInfo->toolBarSize,
		    SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    }


    DidResize (pTTYInfo);

    return (TRUE);
}


/*---------------------------------------------------------------------------
 *  BOOL  SizingTTY( HWND hWnd, int fwSide, LPRECT pRect)
 *
 *  Description:
 *     Snaps the drag rectangle to char width/height boundaries
 *
 *  Parameters:
 *     HWND hWnd
 *        handle to TTY window
 *
 *     WORD fwSide
 *        edge of window being sized
 *
 *     LPRECT
 *        screen coords of drag rectangle in and desired size on return
 *
/*--------------------------------------------------------------------------*/
LOCAL BOOL
SizingTTY (HWND hWnd, int fwSide, LPRECT pRect)
{
    PTTYINFO pTTYInfo;
    int	     newNRow, newNCol, xClient, yClient,
	     xSys, ySys, xDiff, yDiff;

    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
    if (pTTYInfo == NULL)
	return (FALSE);

    xSys  = (2 * GetSystemMetrics(SM_CXSIZEFRAME))
					      + GetSystemMetrics(SM_CXVSCROLL);
    ySys  = (2 * GetSystemMetrics(SM_CYSIZEFRAME))
					      + GetSystemMetrics(SM_CYCAPTION)
					      + GetSystemMetrics(SM_CYMENU);

    newNCol = (((pRect->right - pRect->left) - xSys)
				       - (2 * MARGINE_LEFT)) / pTTYInfo->xChar;
    newNRow = (((pRect->bottom - pRect->top) - ySys) - (2 * MARGINE_TOP)
				   - pTTYInfo->toolBarSize) / pTTYInfo->yChar;

    xClient = (newNCol * pTTYInfo->xChar) + (2 * MARGINE_LEFT);
    yClient = (newNRow * pTTYInfo->yChar) + (2 * MARGINE_TOP)
						      + pTTYInfo->toolBarSize;

    xDiff  = (pRect->left + xClient + xSys) - pRect->right;
    yDiff = (pRect->top + yClient + ySys) - pRect->bottom;

    if(!(xDiff || yDiff))
      return(FALSE);

    switch(fwSide){
      case WMSZ_BOTTOM :	/* Bottom edge */
	pRect->bottom += yDiff;
	break;

      case WMSZ_BOTTOMLEFT :	/*Bottom-left corner */
	pRect->bottom += yDiff;
	pRect->left -= xDiff;
	break;

      case WMSZ_BOTTOMRIGHT :	/* Bottom-right corner */
	pRect->bottom += yDiff;
	pRect->right += xDiff;
	break;

      case WMSZ_LEFT :		/* Left edge */
	pRect->left -= xDiff;
	break;

      case WMSZ_RIGHT :		/* Right edge */
	pRect->right += xDiff;
	break;

      case WMSZ_TOP :		/* Top edge */
	pRect->top -= yDiff;
	break;
	
      case WMSZ_TOPLEFT :	/* Top-left corner */
	pRect->top -= yDiff;
	pRect->left -= xDiff;
	break;

      case WMSZ_TOPRIGHT :	/* Top-right corner */
	pRect->top -= yDiff;
	pRect->right += xDiff;
	break;

      default :
	break;
    }

    if(!(newNRow == pTTYInfo->actNRow && newNCol == pTTYInfo->actNColumn))
      SizeTTY(hWnd, SIZE_RESTORED, (CORD) yClient, (CORD) xClient);

    return(TRUE);
}


/*---------------------------------------------------------------------------
 *  BOOL  MoveTTY (HWND hWnd, int xPos, int yPos)
 *
 *  Description:
 *     Notes the fact that the window has moved.
 *     Only real purpose is so we can tell pine which can the write the
 *     new window position to the 'pinerc' file.
 *
 *  Parameters:
 *     HWND hWnd
 *        handle to TTY window
 *
 *     int xPos, yPos
 *	  New position of the top left corner.
 *
 *
/*--------------------------------------------------------------------------*/
LOCAL BOOL
MoveTTY (HWND hWnd, int xPos, int yPos)
{
#ifdef SDEBUG
    if (mswin_debug >= 5)
	fprintf (mswin_debugfile, "MoveTTY:::  entered\n");
#endif

    DidResize (gpTTYInfo);
    return (TRUE);
}


/*---------------------------------------------------------------------------
 *  void  ScrollTTY ()
 *
 *  Description:
 *      Respond to a scroll message by either calling the scroll
 *      callback or inserting a scroll character into the input
 *      stream.
 *
 *	Scrolling in the TTY window is complicated by the way pine
 *      process events.  Normal windows applications are entirly event
 *      driven.  The top level does nothing but dispatch events.  In
 *      pine, the top level implements the logic.  Events are only
 *      dispatched by the lowest levels.
 *
 *	In normal applications, mouse down in the scroll bar causes
 *	an internal scroll function to be entered.  It tracks the
 *	mouse and issues scroll messages as needed.  If the
 *	application redraws the screen the scroll function also
 *	dispatches the WM_PAINT message to the application.  The
 *	important thing is that this internal scroll function does
 *	not exit until the mouse is released.
 *
 *	We implement two methods for pine's screen managers to deal
 *	with scroll events.  They can receive scroll events as
 *	characters in the normal input stream or they can register a
 *	callback function.
 *
 *	In the "insert a character in the queue" mode, the scroll
 *	event never gets process until the mouse is release.  Auto
 *	repeat scroll events (generated as the mouse is held down)
 *	will cause multiple chars to be inserted in the queue, none
 *	of which will get processed till the mouse is release.  In a
 *	compromise, we allow only one scroll char in the queue,
 *	which prevents makes for a more friendly and controllable
 *	behavior.
 *
 *	In the callback mode, the callback repaints the screen, and
 *	then it calls mswin_flush() which PROCESSES EVENTS!  The
 *	Windows internal scroll function does NOT expect that.  This
 *	behavior can confuses the scroll function, causing it to
 *	miss mouse up events.  We avoid this by setting gScrolling TRUE
 *	when this routine is entered and FALSE when this routine exits
 *	All PeekMessage processors avoid processing any message when
 *	gScrolling is TRUE.
 *
/*--------------------------------------------------------------------------*/
LOCAL void
ScrollTTY (HWND hWnd, int wScrollCode, int nPos, HWND hScroll)
{
    PTTYINFO	pTTYInfo;
    int	cmd = 0;
    long	scroll_pos = 0;
    BOOL	noAction = FALSE;
    BOOL	didScroll;
    FARPROC	prevBlockingProc;
	


    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);

    if (pTTYInfo == NULL || gScrolling)
	return;

    gScrolling = TRUE;
    if (gWSBlockingProc != NULL)
	prevBlockingProc = WSASetBlockingHook (gWSBlockingProc);




    switch (wScrollCode) {
    case SB_BOTTOM:
	cmd = MSWIN_KEY_SCROLLTO;
	scroll_pos = pTTYInfo->scrollTo = 0;
	break;
	
    case SB_TOP:
	cmd = MSWIN_KEY_SCROLLTO;
	scroll_pos = pTTYInfo->scrollTo = pTTYInfo->scrollRange;
	break;
	
    case SB_LINEDOWN:
	cmd = MSWIN_KEY_SCROLLDOWNLINE;
	scroll_pos = 1;
        break;

    case SB_LINEUP:
	cmd = MSWIN_KEY_SCROLLUPLINE;
	scroll_pos = 1;
        break;

    case SB_PAGEDOWN:
	cmd = MSWIN_KEY_SCROLLDOWNPAGE;
	scroll_pos = 1;
        break;

    case SB_PAGEUP:
	cmd = MSWIN_KEY_SCROLLUPPAGE;
	scroll_pos = 1;
        break;
	
    case SB_THUMBTRACK:
    case SB_THUMBPOSITION:
	cmd = MSWIN_KEY_SCROLLTO;
	scroll_pos = pTTYInfo->scrollTo = (long) ((float)nPos);
	break;

    default:
	noAction = TRUE;
	break;
    }


    /*
     * If there is a scroll callback call that.  If there is no scroll
     * callback or the callback says it did not handle the event (returned,
     * FALSE) queue the scroll cmd.
     */
    if (!noAction) {
	SelClear ();
	didScroll = FALSE;
	if (gScrollCallback != NULL) {
	    /* Call scrolling callback.  Set blocking hook to our routine
	     * which prevents messages from being dispatched. */
	    if (gWSBlockingProc != NULL)
		WSASetBlockingHook (gWSBlockingProc);
	    didScroll = gScrollCallback (cmd, scroll_pos);
	    if (gWSBlockingProc != NULL)
		WSAUnhookBlockingHook ();
        }
	/*
	 * If no callback or callback did not do the scrolling operation,
	 * insert a scroll cmd in the input stream.
	 */
	if (!didScroll)
	  CQAddUniq ((UCS)cmd, 0);
    }


    gScrolling = FALSE;
    return;
}


#ifdef	WIN32
/*---------------------------------------------------------------------------
 *  void  MouseWheelTTY ()
 *
 *  Description:
 *      Respond to a WM_MOUSEWHEEL event by calling scroll callback
 *      ala ScrollTTY.
 *
 *
/*--------------------------------------------------------------------------*/
LOCAL void
MouseWheelTTY (HWND hWnd, int xPos, int yPos, int fwKeys, int zDelta)
{
    PTTYINFO	pTTYInfo;
    int	cmd;
    long	scroll_pos;
    FARPROC	prevBlockingProc;
    SCROLLINFO	scrollInfo;
    static int  zDelta_accumulated;
	
    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);

    if (pTTYInfo == NULL || gScrolling)
	return;

    scrollInfo.cbSize = sizeof(SCROLLINFO);
    scrollInfo.fMask = SIF_POS | SIF_RANGE;
    GetScrollInfo(hWnd, SB_VERT, &scrollInfo);
    if((zDelta < 0 && scrollInfo.nPos < scrollInfo.nMin)
       || (zDelta > 0 && scrollInfo.nPos >= scrollInfo.nMax))
      return;

    gScrolling = TRUE;
    if (gWSBlockingProc != NULL)
	prevBlockingProc = WSASetBlockingHook (gWSBlockingProc);

    if(fwKeys == MK_MBUTTON)
       zDelta *= 2;			/* double the effect! */

    if(abs(zDelta += zDelta_accumulated) < WHEEL_DELTA){
	zDelta_accumulated = zDelta;
    }
    else{
	/* Remember any partial increments */
	zDelta_accumulated = zDelta % WHEEL_DELTA;

	scroll_pos = (long)(gsMWMultiplier * abs((zDelta / WHEEL_DELTA)));

	cmd = (zDelta < 0) ? MSWIN_KEY_SCROLLDOWNLINE : MSWIN_KEY_SCROLLUPLINE;

	SelClear ();
	if (gScrollCallback != NULL) {
	    /* Call scrolling callback.  Set blocking hook to our routine
	     * which prevents messages from being dispatched. */
	    if (gWSBlockingProc != NULL)
	      WSASetBlockingHook (gWSBlockingProc);
	    (void) gScrollCallback (cmd, scroll_pos);
	    if (gWSBlockingProc != NULL)
	      WSAUnhookBlockingHook ();
	}
    }

    gScrolling = FALSE;
    return;
}


LOCAL void
MouseWheelMultiplier()
{
    TCHAR lines[8];
    DWORD llen = sizeof(lines)/sizeof(TCHAR);

    /* HKEY_CURRENT_USER\Control Panel\Desktop holds the key */
    gsMWMultiplier = (MSWRPeek(HKEY_CURRENT_USER, TEXT("Control Panel\\Desktop"),
			       TEXT("WheelScrollLines"), lines, &llen) == TRUE)
		       ? (short)_ttoi(lines) : 1;
}
#endif


/*---------------------------------------------------------------------------
 *  void  CaretTTY (HWND hWnd, CARETS cStyle)
 *
 *  Description:
 *     Adjusts the Caret to the user supplied style
 *
 *  Parameters:
 *     HWND hWnd
 *        handle to TTY window
 *
 *     int wStyle
 *	  New style to take on
 *
/*--------------------------------------------------------------------------*/
LOCAL void
CaretTTY (HWND hWnd, CARETS cStyle)
{
    PTTYINFO  pTTYInfo;

    if(pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO)){
	pTTYInfo->cCaretStyle = cStyle;
	CaretCreateTTY (hWnd);
	DidResize (gpTTYInfo);
    }
}


/*---------------------------------------------------------------------------
 *  void  CaretCreateTTY (HWND hWnd, BOOL wPosition)
 *
 *  Description:
 *     Adjusts the Caret to the user supplied style
 *
 *  Parameters:
 *     HWND hWnd
 *        handle to TTY window
 *
 *     BOOL wPosition
 *        whether or not to position it too
 *
/*--------------------------------------------------------------------------*/
LOCAL void
CaretCreateTTY (HWND hWnd)
{
    PTTYINFO  pTTYInfo;

    if(pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO)){
	int n = 0, x, y;

	switch(pTTYInfo->cCaretStyle){
	  case CaretHorizBar :
	    x = pTTYInfo->xChar;
	    y = pTTYInfo->yChar / 5;
	    n = pTTYInfo->yChar - y;
	    break;

	  case CaretVertBar :
	    x = pTTYInfo->xChar / 4;
	    y = pTTYInfo->yChar;
	    break;

	  case CaretSmallBlock :
	    x = pTTYInfo->xChar;
	    y = pTTYInfo->yChar / 2;
	    n = pTTYInfo->yChar - y;
	    break;

	  default :
	    x = pTTYInfo->xChar;
	    y = pTTYInfo->yChar;
	    break;
	}

	CreateCaret (hWnd, NULL, x, y);
	pTTYInfo->yCurOffset = n;

	if(pTTYInfo->fCaretOn){
	    ShowCaret(hWnd);
	    MoveTTYCursor(hWnd);
	}
    }
}


/*
 * This routine is inserted as the winsock blocking hook.  It's main perpos
 * is to NOT dispatch messages.
 */
BOOL CALLBACK __export
NoMsgsAreSent (void)
{
    return (FALSE);
}


/*---------------------------------------------------------------------------
 *  BOOL  SetTTYFocus( HWND hWnd )
 *
 *  Description:
 *     Sets the focus to the TTY window also creates caret.
 *
 *  Parameters:
 *     HWND hWnd
 *        handle to TTY window
 *
/*--------------------------------------------------------------------------*/
LOCAL BOOL
SetTTYFocus (HWND hWnd)
{
    PTTYINFO  pTTYInfo;

#ifdef SDEBUG
    if (mswin_debug >= 5)
	fprintf (mswin_debugfile, "SetTTYFocus:::  entered\n");
#endif

    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
    if (pTTYInfo == NULL)
      return (FALSE);

    mswin_showcursor(TRUE);

    pTTYInfo->fFocused = TRUE;

    CaretCreateTTY (hWnd);

    MoveTTYCursor (hWnd);
    return (TRUE);
}


/*---------------------------------------------------------------------------
 *  BOOL  KillTTYFocus( HWND hWnd )
 *
 *  Description:
 *     Kills TTY focus and destroys the caret.
 *
 *  Parameters:
 *     HWND hWnd
 *        handle to TTY window
 *
/*--------------------------------------------------------------------------*/
LOCAL BOOL
KillTTYFocus (HWND hWnd)
{
    PTTYINFO  pTTYInfo;

#ifdef SDEBUG
    if (mswin_debug >= 5)
	fprintf (mswin_debugfile, "KillTTYFocus:::  entered\n");
#endif
    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
    if (pTTYInfo == NULL)
	    return (FALSE);

    mswin_showcursor(TRUE);

    if(pTTYInfo->fCaretOn)
      HideCaret (hWnd);

    DestroyCaret();

    pTTYInfo->fFocused = FALSE;

    return (TRUE);
}


/*---------------------------------------------------------------------------
 *  BOOL  MoveTTYCursor( HWND hWnd )
 *
 *  Description:
 *     Moves caret to current position.
 *
 *  Parameters:
 *     HWND hWnd
 *        handle to TTY window
 *
/*--------------------------------------------------------------------------*/
LOCAL BOOL
MoveTTYCursor (HWND hWnd)
{
    PTTYINFO  pTTYInfo;

    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
    if (pTTYInfo == NULL)
	    return (FALSE);

    if(pTTYInfo->fCaretOn && !pTTYInfo->fMassiveUpdate) {
	HideCaret (hWnd);
	SetCaretPos ((pTTYInfo->nColumn * pTTYInfo->xChar) + pTTYInfo->xOffset,
		     (pTTYInfo->nRow * pTTYInfo->yChar)
				   + pTTYInfo->yCurOffset + pTTYInfo->yOffset);
	ShowCaret (hWnd);
    }

    return (TRUE);
}


/*---------------------------------------------------------------------------
 *  BOOL  ProcessTTYKeyDown ( HWND hWnd, WORD bOut )
 *
 *  Description:
 *	Called to process MW_KEYDOWN message.  We are only interested in
 *	virtual keys that pico/pine use.  All others get passed on to
 *	the default message handler.  Regular key presses will return
 *	latter as a WM_CHAR message, with SHIFT and CONTROL processing
 *	already done.
 *
 *	We do watch for VK_CONTROL to keep track of it's state such
 *	that we can implement ^_space.
 *
 *  Parameters:
 *     HWND hWnd
 *        handle to TTY window
 *
 *     BYTE key
 *	  Virtual key code.
 *
/*--------------------------------------------------------------------------*/
LOCAL BOOL
ProcessTTYKeyDown (HWND hWnd, TCHAR key)
{
    UCS		myKey;
    BOOL        fKeyControlDown = GetKeyState(VK_CONTROL) < 0;
    BOOL        fKeyAltDown = GetKeyState(VK_MENU) < 0;

    // If the alt key is down, let Windows handle the message. This will
    //  allow the Ctrl+Alt (AltGr) processing to work.
    if(fKeyAltDown)
        return FALSE;

    switch (key) {
    case VK_MENU:
    case VK_CONTROL:
    case VK_SHIFT:
        return FALSE;
	case VK_UP:		myKey = KEY_UP;		break;
	case VK_DOWN:		myKey = KEY_DOWN;		break;
	case VK_RIGHT:
        /* Ctrl-@ is used to advance to the next word. */
        myKey = fKeyControlDown ? '@': KEY_RIGHT;
	  break;
	case VK_LEFT:
        /* Ctrl-left is used to advance to the previous word. */
        myKey = KEY_LEFT;
	  break;
	case VK_HOME:
        /* Ctrl-home is used to advance to the beginning of buffer. */
        myKey = KEY_HOME;
	  break;
	case VK_END:
        /* Ctrl-end is used to advance to the end of buffer. */
        myKey = KEY_END;
	  break;
	case VK_PRIOR:		myKey = KEY_PGUP;	break;
	case VK_NEXT:		myKey = KEY_PGDN;	break;
	case VK_DELETE:		myKey = KEY_DEL;	break;
	case VK_F1:		myKey = F1;		break;
	case VK_F2:		myKey = F2;		break;
	case VK_F3:		myKey = F3;		break;
	case VK_F4:		myKey = F4;		break;
	case VK_F5:		myKey = F5;		break;
	case VK_F6:		myKey = F6;		break;
	case VK_F7:		myKey = F7;		break;
	case VK_F8:		myKey = F8;		break;
	case VK_F9:		myKey = F9;		break;
	case VK_F10:		myKey = F10;		break;
	case VK_F11:		myKey = F11;		break;
	case VK_F12:		myKey = F12;		break;
	
	default:
        if(fKeyControlDown && !(GetKeyState(VK_SHIFT) < 0)) {
            if(key == '6') {
	    /*
	     * Ctrl-^ is used to set and clear the mark in the
	     * composer (pico) On most other systems Ctrl-6 does the
                 * same thing.  Allow that on windows too.
	     */
                myKey = '^';
	    break;
            } else if(key == '2') {
                /* Ctrl-@ is used to advance to the next word. */
                myKey = '@';
	    break;
            }
	}

        return (FALSE);	/* Message NOT handled.*/
    }

    CQAdd (myKey, fKeyControlDown);

    set_time_of_last_input();
				
    return (TRUE);			/* Message handled .*/
}


#ifdef CDEBUG
char *
dtime()
{
    static char timestring[23];
    time_t t;
    struct _timeb timebuffer;

    timestring[0] = '\0';
    t = time((time_t *) 0);
    _ftime(&timebuffer);
    snprintf(timestring, sizeof(timestring), "%.8s.%03ld", ctime(&t)+11, timebuffer.millitm);

    return(timestring);
}
#endif


/*---------------------------------------------------------------------------
 *  BOOL  ProcessTTYCharacter( HWND hWnd, WORD bOut )
 *
 *  Description:
 *		Place the character into a queue.
 *
 *  Parameters:
 *     HWND hWnd
 *        handle to TTY window
 *
 *     BYTE bOut
 *        byte from keyboard
 *
/*--------------------------------------------------------------------------*/
LOCAL BOOL
ProcessTTYCharacter (HWND hWnd, TCHAR bOut)
{
    // Only check for control key being down if the alt key isn't also down.
    //  Windows uses Ctrl+Alt as AltGr.
    BOOL fKeyAltDown = GetKeyState(VK_MENU) < 0;
    BOOL fKeyControlDown = fKeyAltDown ?
        FALSE : (GetKeyState(VK_CONTROL) < 0);

    if(fKeyControlDown) {
        if(bOut == ' ')
            bOut = '@';
        else
            bOut += '@';
    }

    CQAdd ((UCS)bOut, fKeyControlDown);

#ifdef ACCELERATORS
    UpdateAccelerators (hWnd);
#endif

    set_time_of_last_input();

    return (TRUE);		/* Message handled. */
}


/*---------------------------------------------------------------------------
 *  BOOL  ProcessTTYMouse(HWND hWnd, int mevent, int button,
 *			  int xPos, int yPos, WPARAM keys)
 *
 *  Description:
 *	This is the central control for all mouse events.  Every event
 *	gets put into a queue to wait for the upper layer.
 *
 *	The upper's input routine calls checkmouse() which pulls the
 *	mouse event off the input queue.  checkmouse() has a list of
 *	of screen regions.  Some regions correspond to a "menu" item
 *	(text button at bottom of screen).  There is generally one
 *	region for the central region of the screen.
 *
 *	Because pine/pico do not interpret mouse drags, we do that here.
 *	When the user presses the button and drags the mouse across the
 *	screen this select the text in the region defined by the drag.
 *	The operation is local to mswin.c, and can only get what text
 *	is on the screen.
 *
 *	The one exception is that now pico interprets mouse drag events
 *	in the body.  pico signals that it wants to track the mouse
 *	by calling mswin_allowmousetrack().  This will 1) turn off
 *	our mouse tracking and 2) cause mouse movement events to
 *	be put on the mouse queue.
 *
 *
 *  Parameters:
 *     HWND hWnd
 *        handle to TTY window
 *
 *     BYTE bOut
 *        byte from keyboard
 *
/*--------------------------------------------------------------------------*/
LOCAL BOOL
ProcessTTYMouse (HWND hWnd, int mevent, int button,
		 CORD xPos, CORD yPos, WPARAM winkeys)
{
    int		nRow;
    int		nColumn;
    int		keys;

    /*
     * Convert to cell position.
     */
    nColumn = (xPos - gpTTYInfo->xOffset) / gpTTYInfo->xChar;
    if (xPos < gpTTYInfo->xOffset)
	--nColumn;
    nRow = (yPos - gpTTYInfo->yOffset) / gpTTYInfo->yChar;
    if (yPos < gpTTYInfo->yOffset)
	--nRow;

    /*
     * Convert window's keys.
     */
    keys = 0;
    if (winkeys & MK_CONTROL)
	    keys |= M_KEY_CONTROL;
    if (winkeys & MK_SHIFT)
	    keys |= M_KEY_SHIFT;

    /* Adjust the cursor */
    if((unsigned long) mevent != M_EVENT_UP){
	if(gMouseTracking)
	  mswin_setcursor(MSWIN_CURSOR_IBEAM);
	else if(ghCursorCurrent == ghCursorBusy)
	  mswin_setcursor(MSWIN_CURSOR_BUSY);
	else if(mouse_on_key(nRow, nColumn))
	  mswin_setcursor(MSWIN_CURSOR_HAND);
	else if(gMouseTrackCallback)
	  mswin_setcursor((*gMouseTrackCallback)(nColumn, (long) nRow));
	else
	  mswin_setcursor(MSWIN_CURSOR_ARROW);
    }

    /*
     * Tracking event or mouse up/down?
     */
    if ((unsigned long) mevent == M_EVENT_TRACK) {
	/*
	 * Who is doing the tracking?
	 */
	if (gAllowMouseTrack) {
	    /* For tracking, Button info is different. */
	    if (keys & MK_LBUTTON)
		button = M_BUTTON_LEFT;
	    else if (keys & MK_MBUTTON)
		button = M_BUTTON_MIDDLE;
	    else if (keys & MK_RBUTTON)
		button = M_BUTTON_RIGHT;
	    MQAdd (mevent, button, nRow, nColumn, keys,
		    MSWIN_MF_REPLACING);
        }
	else
	    SelTrackMouse (nRow, nColumn);
    }
    else{
	/*
	 * Tracking.  Only start tracking mouse down in the text region
	 * But allow mouse up anywhere.
	 */
	if ( (nRow >= 0 && nRow < gpTTYInfo->actNRow &&
		   nColumn >= 0 && nColumn < gpTTYInfo->actNColumn)
		  || (unsigned long) mevent == M_EVENT_UP) {

	    /*
	     * Mouse tracking.  When the mouse goes down we start
	     * capturing all mouse movement events.  If no one else wants
	     * them we will start defining a text selection.
	     */
	    if ((unsigned long) mevent == M_EVENT_DOWN) {
		gMouseTracking = TRUE;
		SetCapture (ghTTYWnd);
		if (!gAllowMouseTrack && button == M_BUTTON_LEFT)
		    SelStart (nRow, nColumn);
	    }
	    else {
		ReleaseCapture ();
		if (!gAllowMouseTrack && button == M_BUTTON_LEFT)
		    SelFinish (nRow, nColumn);
		gMouseTracking = FALSE;

		/*
		 * If right mouse button, toss pop-up menu offering
		 * cut/copy/paste
		 */
		if(button == M_BUTTON_RIGHT && SelAvailable()){
		    UINT fAllowed = (EM_CP | EM_CP_APPEND);

		    if(gAllowCut)
		      fAllowed |= EM_CUT;

		    if(CopyCutPopup(hWnd, fAllowed) == TRUE)
		      mevent = M_EVENT_TRACK; /* don't add to input queue! */
		}
	    }

	    /*
	     * Insert event into queue.
	     */
	    if((unsigned long) mevent != M_EVENT_TRACK)
	      MQAdd (mevent, button, nRow, nColumn, keys, 0);
        }
    }

    mswin_showcursor(TRUE);	/* make sure it's visible */

#ifdef ACCELERATORS
    UpdateAccelerators (hWnd);
#endif

    set_time_of_last_input();

    return (0);		/* Message handled. */
}


/*---------------------------------------------------------------------------
 *  BOOL  ProcessTimer ()
 *
 *  Description:
 *     Process the periodic timer calls.
 *
 *
 *  Parameters:
 *	None.
 *
/*--------------------------------------------------------------------------*/
LOCAL void
ProcessTimer (void)
{
    /* Time to deliver an alarm signal? */
    if (gAlarmTimeout != 0 && GetTickCount () / 1000 > gAlarmTimeout)
	AlarmDeliver ();

    /* Time to make the periodic callback. */
    if (gPeriodicCallback != NULL &&
	    GetTickCount() / 1000 > gPeriodicCBTimeout) {
	gPeriodicCBTimeout = GetTickCount() / 1000 +
						gPeriodicCBTime;
	gPeriodicCallback ();
    }

    /*
     * If tracking the mouse, insert a fake mouse tracking message
     * At the last know location of the mouse.
     */
    if (gAllowMouseTrack) {
	gMTEvent.event = M_EVENT_TRACK;
	MQAdd (gMTEvent.event, gMTEvent.button, gMTEvent.nRow,
		gMTEvent.nColumn, gMTEvent.keys, MSWIN_MF_REPLACING);
    }
}


/*---------------------------------------------------------------------------
 *  BOOL  WriteTTYBlock( HWND hWnd, LPSTR lpBlock, int nLength )
 *
 *  Description:
 *     Writes block to TTY screen.  Nothing fancy - just
 *     straight TTY.
 *
 *  Parameters:
 *     HWND hWnd
 *        handle to TTY window
 *
 *     LPSTR lpBlock
 *        far pointer to block of data
 *
 *     int nLength
 *        length of block
 *
/*--------------------------------------------------------------------------*/
LOCAL BOOL
WriteTTYBlock (HWND hWnd, LPTSTR lpBlock, int nLength)
{
    int				i, j, width;
    PTTYINFO			pTTYInfo;
    RECT			rect;
    BOOL			fNewLine;
    long			offset;

    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
    if (pTTYInfo == NULL)
	    return (FALSE);

    for (i = 0 ; i < nLength; i++) {
	switch (lpBlock[i]) {
	case ASCII_BEL:
	    /* Bell */
	    MessageBeep (0) ;
	    break ;

	case ASCII_BS:
	    /* Backspace over a whole character */
	    offset = pscreen_offset_from_cord(pTTYInfo->nRow, pTTYInfo->nColumn, pTTYInfo);
	    width = (offset > pTTYInfo->nRow * pTTYInfo->actNColumn)
			    ? scrwidth(pTTYInfo->pScreen+offset-1, 1) : 0;

	    if(pTTYInfo->nColumn > 0)
	      pTTYInfo->nColumn = (CORD)(pTTYInfo->nColumn - width);

	    MoveTTYCursor (hWnd);
	    break;

	case ASCII_CR:
	    /* Carriage return */
	    pTTYInfo->nColumn = 0 ;
	    MoveTTYCursor (hWnd);
	    if (!pTTYInfo->fNewLine)
		    break;

	    /* fall through */

	case ASCII_LF:
	    /* Line feed */
	    if (++pTTYInfo->nRow == pTTYInfo->actNRow) {

		/* Scroll the Screen. */

		/* slide rows 1 - n-1 up to row 0 */
		memmove ((LPTSTR)pTTYInfo->pScreen,
			(LPTSTR) (pTTYInfo->pScreen + pTTYInfo->actNColumn),
			(pTTYInfo->actNRow - 1) * pTTYInfo->actNColumn * sizeof (TCHAR));

		/* initialize new row n-1 */
		for(j = (pTTYInfo->actNRow - 1) * pTTYInfo->actNColumn;
		    j < pTTYInfo->actNColumn; j++)
		  pTTYInfo->pScreen[j] = (TCHAR) ' ';


		/* Scroll the Cell Widths */
		memmove ((int *)pTTYInfo->pCellWidth,
			 (int *) (pTTYInfo->pCellWidth + pTTYInfo->actNColumn),
			(pTTYInfo->actNRow - 1) * pTTYInfo->actNColumn * sizeof (int));		
		for(j = (pTTYInfo->actNRow - 1) * pTTYInfo->actNColumn;
		    j < pTTYInfo->actNColumn; j++)
		  pTTYInfo->pCellWidth[j] = pTTYInfo->xChar;    /* xChar set yet ? */

		/* Scroll the Attributes. */

		/* slide rows 1 - n-1 up to row 0 */
		memmove ((CharAttrib *) pTTYInfo->pAttrib,
			 (CharAttrib *) (pTTYInfo->pAttrib + pTTYInfo->actNColumn),
			 (pTTYInfo->actNRow - 1) * pTTYInfo->actNColumn * sizeof(CharAttrib));

		/* initialize new row n-1 to zero */
		memset ((CharAttrib *) (pTTYInfo->pAttrib +
			    (pTTYInfo->actNRow - 1) * pTTYInfo->actNColumn),
			0, pTTYInfo->actNColumn*sizeof(CharAttrib));

		pTTYInfo->screenDirty = TRUE;
		pTTYInfo->eraseScreen = TRUE;
		InvalidateRect (hWnd, NULL, FALSE);
		--pTTYInfo->nRow;
	    }

	    MoveTTYCursor (hWnd);
	    break;

	default:
	    offset = pscreen_offset_from_cord(pTTYInfo->nRow, pTTYInfo->nColumn, pTTYInfo);
	    pTTYInfo->pScreen[offset] = lpBlock[i];
	    pTTYInfo->pCellWidth[offset] = wcellwidth((UCS)lpBlock[i]) * pTTYInfo->xChar;
	    pTTYInfo->pAttrib[offset] = pTTYInfo->curAttrib;
	    rect.left = (pTTYInfo->nColumn * pTTYInfo->xChar) +
		    pTTYInfo->xOffset;
	    rect.right = rect.left + pTTYInfo->xChar;
	    rect.top = (pTTYInfo->nRow * pTTYInfo->yChar) +
		    pTTYInfo->yOffset;
	    rect.bottom = rect.top + pTTYInfo->yChar;
	    pTTYInfo->screenDirty = TRUE;
	    InvalidateRect (hWnd, &rect, FALSE);

	    /* Line Wrap. */
	    if (pTTYInfo->nColumn < pTTYInfo->actNColumn - 1)
		    pTTYInfo->nColumn++ ;
	    else if (pTTYInfo->autoWrap == WRAP_ON ||
		    (pTTYInfo->autoWrap == WRAP_NO_SCROLL &&
			    pTTYInfo->nRow < pTTYInfo->actNRow - 1)) {
		    fNewLine = pTTYInfo->fNewLine;
		    pTTYInfo->fNewLine = FALSE;
		    WriteTTYBlock (hWnd, TEXT("\r\n"), 2);
		    pTTYInfo->fNewLine = fNewLine;
	    }
	    break;
	}
    }
    return (TRUE);
}


/*---------------------------------------------------------------------------
 *  BOOL  WriteTTYText ( HWND hWnd, LPSTR lpBlock, int nLength )
 *
 *  Description:
 *	Like WriteTTYBlock but optimized for strings that are text only,
 *	no carrage control characters.
 *
 *  Parameters:
 *     HWND hWnd
 *        handle to TTY window
 *
 *     LPSTR lpBlock
 *        far pointer to block of data
 *
 *     int nLength
 *        length of block
 *
/*--------------------------------------------------------------------------*/
LOCAL BOOL
WriteTTYText (HWND hWnd, LPTSTR lpText, int nLength)
{
    int				i;
    PTTYINFO			pTTYInfo;
    RECT			rect;
    long			offset, endOffset;
    long			colEnd;
    long			screenEnd;
    int                         screenwidth;


    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
    if (pTTYInfo == NULL)
      return ( FALSE );


    /* Calculate offset of cursor, end of current column, and end of screen */
    offset = pscreen_offset_from_cord(pTTYInfo->nRow, pTTYInfo->nColumn, pTTYInfo);

    colEnd = (pTTYInfo->nRow + 1) * pTTYInfo->actNColumn;
    screenEnd = pTTYInfo->actNRow * pTTYInfo->actNColumn;


    /* Text is allowed to wrap around to subsequent lines, but not past end
     * of screen */
    endOffset = offset + nLength;
    if (endOffset >= screenEnd) {
	nLength = screenEnd - offset;
	endOffset = offset + nLength - 1;  /* Last cell, not one past last */
    }


    /* Calculate bounding rectangle. */
    if (endOffset <= colEnd) {
	/* Single line. */

	screenwidth = scrwidth(lpText, nLength);

	rect.left = (pTTYInfo->nColumn * pTTYInfo->xChar) + pTTYInfo->xOffset;
	rect.right = rect.left + (pTTYInfo->xChar * screenwidth);
	rect.top = (pTTYInfo->nRow * pTTYInfo->yChar) + pTTYInfo->yOffset;
	rect.bottom = rect.top + pTTYInfo->yChar;
	/* Advance cursor on cur line but not past end. */
	pTTYInfo->nColumn = (CORD)MIN(pTTYInfo->nColumn + screenwidth,
				      pTTYInfo->actNColumn - 1);
    }
    else {
	/* Wraps across multiple lines.  Calculate one rect to cover all
	 * lines. */
	rect.left = 0;
	rect.right = pTTYInfo->xSize;
	rect.top = (pTTYInfo->nRow * pTTYInfo->yChar) + pTTYInfo->yOffset;
	rect.bottom = ((((offset + nLength) / pTTYInfo->actNColumn) + 1) *
			pTTYInfo->yChar) + pTTYInfo->yOffset;
	pTTYInfo->nRow = (CORD)(endOffset / pTTYInfo->actNColumn);
	pTTYInfo->nColumn = (CORD)(endOffset % pTTYInfo->actNColumn);
    }


    /* Apply text and attributes to screen in one smooth motion. */
    for(i = 0; i < nLength; i++)
      (pTTYInfo->pScreen+offset)[i] = lpText[i];
    for(i = 0; i < nLength; i++)
      (pTTYInfo->pCellWidth+offset)[i] = wcellwidth((UCS)lpText[i]) * pTTYInfo->xChar;
    for(i = 0; i < nLength; i++)
      pTTYInfo->pAttrib[offset+i] = pTTYInfo->curAttrib;

    /* Invalidate rectangle */
    pTTYInfo->screenDirty = TRUE;
    InvalidateRect (hWnd, &rect, FALSE);
    return (TRUE);
}


/*---------------------------------------------------------------------------
 *  BOOL  WriteTTYChar (HWND hWnd, char ch)
 *
 *  Description:
 *	Write a single character to the cursor position and advance the
 *	cursor.  Does not handle carage control.
 *
 *  Parameters:
 *     HWND hWnd
 *        handle to TTY window
 *
 *     char ch
 *	  character being written.
 *
/*--------------------------------------------------------------------------*/
LOCAL BOOL
WriteTTYChar (HWND hWnd, TCHAR ch)
{
    PTTYINFO			pTTYInfo;
    RECT			rect;
    long			offset;


    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
    if (pTTYInfo == NULL)
	return (FALSE);

    offset = (pTTYInfo->nRow * pTTYInfo->actNColumn) +
	    pTTYInfo->nColumn;

    *(pTTYInfo->pScreen + offset) = ch;
    pTTYInfo->pCellWidth[offset] = wcellwidth((UCS)ch) * pTTYInfo->xChar;
    pTTYInfo->pAttrib[offset] = pTTYInfo->curAttrib;

    rect.left = (pTTYInfo->nColumn * pTTYInfo->xChar) + pTTYInfo->xOffset;
    rect.right = rect.left + pTTYInfo->xChar;
    rect.top = (pTTYInfo->nRow * pTTYInfo->yChar) + pTTYInfo->yOffset;
    rect.bottom = rect.top + pTTYInfo->yChar;
    pTTYInfo->screenDirty = TRUE;
    InvalidateRect (hWnd, &rect, FALSE);



    /* Line Wrap. */
    if (pTTYInfo->nColumn < pTTYInfo->actNColumn - 1)
	pTTYInfo->nColumn++ ;
    else if ((pTTYInfo->autoWrap == WRAP_ON ||
	      pTTYInfo->autoWrap == WRAP_NO_SCROLL) &&
		    pTTYInfo->nRow < pTTYInfo->actNRow - 1) {
       pTTYInfo->nRow++;
       pTTYInfo->nColumn = 0;
    }
    return (TRUE);
}


/*---------------------------------------------------------------------------
 *  VOID  GoModalDialogBoxParam( HINSTANCE hInstance,
 *                                   LPTSTR lpszTemplate, HWND hWnd,
 *                                   DLGPROC lpDlgProc, LPARAM lParam )
 *
 *  Description:
 *     It is a simple utility function that simply performs the
 *     MPI and invokes the dialog box with a DWORD paramter.
 *
 *  Parameters:
 *     similar to that of DialogBoxParam() with the exception
 *     that the lpDlgProc is not a procedure instance
 *
/*--------------------------------------------------------------------------*/
LOCAL VOID
GoModalDialogBoxParam( HINSTANCE hInstance, LPTSTR lpszTemplate,
                                 HWND hWnd, DLGPROC lpDlgProc, LPARAM lParam )
{
   DLGPROC  lpProcInstance ;

   lpProcInstance = (DLGPROC) MakeProcInstance( (FARPROC) lpDlgProc,
                                                hInstance ) ;
   DialogBoxParam( hInstance, lpszTemplate, hWnd, lpProcInstance, lParam ) ;
   FreeProcInstance( (FARPROC) lpProcInstance ) ;
}


/*---------------------------------------------------------------------------
 *  BOOL FAR PASCAL __export AboutDlgProc( HWND hDlg, UINT uMsg,
 *                                WPARAM wParam, LPARAM lParam )
 *
 *  Description:
 *     Simulates the Windows System Dialog Box.
 *
 *  Parameters:
 *     Same as standard dialog procedures.
 *
/*--------------------------------------------------------------------------*/
BOOL FAR PASCAL __export
AboutDlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
   switch (uMsg) {
      case WM_INITDIALOG:
      {
         TCHAR        szTemp [81];

         /* sets up version number for PINE */
         GetDlgItemText (hDlg, IDD_VERSION, szTemp, sizeof(szTemp)/sizeof(TCHAR));
	 /* szTemp is unicode, mswin_compilation_date etc are cast as %S in mswin.rc */
         _sntprintf (TempBuf, sizeof(TempBuf)/sizeof(TCHAR), szTemp, mswin_specific_winver(),
		   mswin_majorver(), mswin_minorver(),
		   mswin_compilation_remarks(),
		   mswin_compilation_date());
         SetDlgItemText (hDlg, IDD_VERSION, (LPTSTR) TempBuf);

         /* get by-line */
         LoadString (GET_HINST (hDlg), IDS_BYLINE, TempBuf,
                     sizeof(TempBuf) / sizeof(TCHAR));
         SetDlgItemText (hDlg, IDD_BYLINE, TempBuf);

      }
      return ( TRUE ) ;

      case WM_CLOSE:
	EndDialog( hDlg, TRUE ) ;
	return ( TRUE ) ;

      case WM_COMMAND:
	switch((WORD) wParam){
	  case IDD_OK :
            EndDialog( hDlg, TRUE ) ;
            return ( TRUE ) ;

	  default :
	    break;
         }

	break;
   }
   return ( FALSE ) ;

} /* end of AboutDlgProc() */


/*---------------------------------------------------------------------------
 *
 *  Description:
 *     Simulates the Windows System Dialog Box.
 *
 *  Parameters:
 *     Same as standard dialog procedures.
 *
/*--------------------------------------------------------------------------*/
BOOL FAR PASCAL __export
SplashDlgProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static  HBITMAP hbmpSplash;

    switch (uMsg) {
      case WM_INITDIALOG:
	if(hbmpSplash = LoadBitmap(GET_HINST(hDlg),
				   MAKEINTRESOURCE(ALPINESPLASH))){
	    BITMAP stBitmap;
	    int	  cx, cy;

	    cx = GetSystemMetrics(SM_CXSCREEN);
	    cy = GetSystemMetrics(SM_CYSCREEN);
	    GetObject(hbmpSplash, sizeof(BITMAP), &stBitmap);

	    SetWindowPos(hDlg, HWND_TOPMOST,
			 (cx - stBitmap.bmWidth) / 2,
			 (cy - stBitmap.bmHeight) / 2,
			 stBitmap.bmWidth, stBitmap.bmHeight,
			 SWP_NOCOPYBITS /* | SWP_SHOWWINDOW */);
	}

	return(TRUE);

      case WM_CTLCOLORDLG :
	return(TRUE);

      case WM_ERASEBKGND :
        {
	    HDC		 hMemDC;
	    POINT	 stPoint;
	    BITMAP	 stBitmap;
	    HGDIOBJ	 hObject;

	  if((hMemDC = CreateCompatibleDC((HDC) wParam)) != NULL){
	      hObject = SelectObject(hMemDC, hbmpSplash);
	      SetMapMode(hMemDC, GetMapMode((HDC) wParam));

	      GetObject(hbmpSplash, sizeof(BITMAP), &stBitmap);
	      stPoint.x = stBitmap.bmWidth;
	      stPoint.y = stBitmap.bmHeight;
	      DPtoLP((HDC) wParam, &stPoint, 1);

	      BitBlt((HDC) wParam,
		     0, 0, stPoint.x, stPoint.y,
		     hMemDC, 0, 0, SRCCOPY);
	      SelectObject(hMemDC, hObject);
	      DeleteDC(hMemDC) ;
	  }

	  return(TRUE);
      }

      break;

      case WM_CLOSE:
	DestroyWindow(hDlg);
	return(TRUE);

     case WM_DESTROY :
       if(hbmpSplash)
	 DeleteObject(hbmpSplash);

       break;

      case WM_COMMAND:		/* No commands! */
	DestroyWindow(hDlg);
	return(TRUE);
   }

   return(FALSE);

}


#if	0
UINT APIENTRY
SelectTTYFontHook(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch(uMsg){
      case WM_INITDIALOG :
      {
	  /*
	   * Deactivate the Style combo box...
	   */
	  HWND hWnd = GetDlgItem(hDlg, cmb2);
	  EnableWindow(hWnd, FALSE);
	  SetWindowPos(hWnd, NULL, 0, 0, 0, 0, SWP_HIDEWINDOW);
	  return(TRUE);
      }

      break;

      case WM_COMMAND :
	switch ((WORD) wParam) {
	  case psh3 :
	    {
		LOGFONT curfont;

		SendMessage(hDlg, WM_CHOOSEFONT_GETLOGFONT,
			    0, (LPARAM) &curfont);
		ResetTTYFont (ghTTYWnd, gpTTYInfo, &curfont);
		return (TRUE);
	    }

	    break;

	  default :
	    break;
	}

	break;

      default :
	break;
    }

    return(FALSE);
}
#endif


/*---------------------------------------------------------------------------
 *  BOOL  SelectTTYFont( HWND hDlg )
 *
 *  Description:
 *     Selects the current font for the TTY screen.
 *     Uses the Common Dialog ChooseFont() API.
 *
 *  Parameters:
 *     HWND hDlg
 *        handle to settings dialog
 *
/*--------------------------------------------------------------------------*/
LOCAL BOOL
SelectTTYFont (HWND hWnd)
{
    CHOOSEFONT		cfTTYFont;
    LOGFONT		newFont, origFont;
    PTTYINFO		pTTYInfo;

    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
    if (pTTYInfo == NULL)
	return (FALSE);

    memcpy (&newFont, &gpTTYInfo->lfTTYFont, sizeof (LOGFONT));
    memcpy (&origFont, &gpTTYInfo->lfTTYFont, sizeof (LOGFONT));

    cfTTYFont.lStructSize    = sizeof (CHOOSEFONT);
    cfTTYFont.hwndOwner      = hWnd ;
    cfTTYFont.hDC            = NULL ;
    cfTTYFont.lpLogFont      = &newFont;
    cfTTYFont.Flags          = CF_SCREENFONTS | CF_FIXEDPITCHONLY |
	    CF_INITTOLOGFONTSTRUCT |
#if	0
	    CF_FORCEFONTEXIST | CF_LIMITSIZE |
	    CF_ENABLEHOOK | CF_APPLY;
#else
	    CF_FORCEFONTEXIST | CF_LIMITSIZE;
#endif
    cfTTYFont.nSizeMin	     = FONT_MIN_SIZE;
    cfTTYFont.nSizeMax	     = FONT_MAX_SIZE;
    cfTTYFont.lCustData      = (long) 0 ;
#if	0
    cfTTYFont.lpfnHook       = SelectTTYFontHook ;
#else
    cfTTYFont.lpfnHook       = NULL;
#endif
    cfTTYFont.lpTemplateName = NULL ;
    cfTTYFont.hInstance      = GET_HINST (hWnd);



    if (ChooseFont (&cfTTYFont)) {
	ResetTTYFont (hWnd, pTTYInfo, &newFont);
    }
#if	0
    else{
	ResetTTYFont (hWnd, pTTYInfo, &origFont);
    }
#endif

    return (TRUE);
}


/*
 * Set a specific color (forground, background, reverse, normal) to
 * the color specified by name.
 */
LOCAL void
SetColorAttribute (COLORREF *cf, char *colorName)
{
    /* color name not in table.  Try converting RGB string. */
    ConvertRGBString (colorName, cf);

    /* Redraw screen. */
    gpTTYInfo->screenDirty = TRUE;
    gpTTYInfo->eraseScreen = TRUE;
    InvalidateRect (ghTTYWnd, NULL, FALSE);
}


/*
 * Set current color attribute to reverse color
 */
void
SetReverseColor()
{
    FlushWriteAccum ();
    gpTTYInfo->curAttrib.rgbFG = gpTTYInfo->rgbRFGColor;
    gpTTYInfo->curAttrib.rgbBG = gpTTYInfo->rgbRBGColor;
}


/*
 * Convert a string to an integer.
 */
LOCAL BOOL
ScanInt (char *str, int min, int max, int *val)
{
    char	*c;
    int		v;
    int		neg = 1;


    if (str == NULL) return (FALSE);
    if (*str == '\0' || strlen (str) > 9) return (FALSE);

    /* Check for a negative sign. */
    if (*str == '-') {
	neg = -1;
	++str;
    }

    /* Check for all digits. */
    for (c = str; *c != '\0'; ++c) {
	if (!isdigit((unsigned char)*c))
	    return (FALSE);
    }

    /* Convert from ascii to int. */
    v = atoi (str) * neg;

    /* Check constraints. */
    if (v < min || v > max)
	return (FALSE);
    *val = v;
    return (TRUE);
}


/*
 * Convert a RGB string to a color ref.  The string should look like:
 *    rrr,ggg,bbb
 * where rrr, ggg, and bbb are numbers between 0 and 255 that represent
 * red, gree, and blue values.  Must be comma seperated.
 * Returns:
 *	TRUE	- Successfully converted string.
 *	FALSE	- Bad format, 'cf' unchanged.
 */
LOCAL BOOL
ConvertRGBString (char *colorName, COLORREF *cf)
{
    int		i, j, n, rgb[3];
    MSWINColor *ct;

    if(!colorName)
      return(FALSE);

    /* Is the name in the global color table? */
    for(ct = MSWINColorTable; ct->colorName; ct++)
      if(!struncmp(ct->colorName, colorName, (int)strlen(ct->colorName))){
	  *cf = ct->colorRef;
	  return(TRUE);
      }

    /* Not a named color, try RRR,GGG,BBB */
    for(i = 0; i < 3; i++){
	if(i && *colorName++ != ',')
	  return(FALSE);

	for(rgb[i] = 0, j = 0; j < 3; j++)
	  if((n = *colorName++ - '0') < 0 || n > 9)
	    return(FALSE);
	  else
	    rgb[i] = (rgb[i] * 10) + n;
    }

    *cf = RGB (rgb[0], rgb[1], rgb[2]);
    return (TRUE);
}


LOCAL char *
ConvertStringRGB(char *colorName, size_t ncolorName, COLORREF colorRef)
{
    MSWINColor *cf;

    for(cf = MSWINColorTable;
	cf->colorName && cf->colorRef != colorRef;
	cf++)
      ;

    if(cf->colorName){
	strncpy(colorName, cf->colorName, ncolorName);
	colorName[ncolorName-1] = '\0';
    }
    else
      snprintf(colorName, ncolorName, "%.3d,%.3d,%.3d",
	       GetRValue(colorRef), GetGValue(colorRef), GetBValue(colorRef));

    return(colorName);
}


/*
 * Map from integer color value to canonical color name.
 */
char *
colorx(int color)
{
    MSWINColor *ct;
    static char cbuf[RGBLEN+1];

    if(color < fullColorTableSize){
	ct = &MSWINColorTable[color];
	if(ct->canonicalName)
	  return(ct->canonicalName);
    }

    /* not supposed to get here */
    snprintf(cbuf, sizeof(cbuf), "color%03.3d", color);
    return(cbuf);
}


/*
 * Argument is a color name which could be an RGB string, a name like "blue",
 * or a name like "color011".
 *
 * Returns a pointer to the canonical name of the color.
 */
char *
color_to_canonical_name(char *s)
{
    int		i, j, n, rgb[3];
    MSWINColor *ct;
    COLORREF    cr;
    static char cn[RGBLEN+1];

    if(!s)
      return(NULL);

    if(!struncmp(s, MATCH_NORM_COLOR, RGBLEN) || !struncmp(s, MATCH_NONE_COLOR, RGBLEN))
      return(s);

    for(ct = MSWINColorTable; ct->colorName; ct++)
      if(!struncmp(ct->colorName, s, (int)strlen(ct->colorName)))
	break;

    if(ct->colorName)
      return(ct->canonicalName);

    /* maybe it is RGB? */
    for(i = 0; i < 3; i++){
	if(i && *s++ != ',')
	  return("");

	for(rgb[i] = 0, j = 0; j < 3; j++)
	  if((n = *s++ - '0') < 0 || n > 9)
	    return("");
	  else
	    rgb[i] = (rgb[i] * 10) + n;
    }

    cr = RGB(rgb[0], rgb[1], rgb[2]);

    /*
     * Now compare that RGB against the color table RGBs. If it is
     * in the table, return the canonical name, else return the RGB string.
     */
    for(ct = MSWINColorTable; ct->colorName; ct++)
      if(ct->colorRef == cr)
	break;

    if(ct->colorName)
      return(ct->canonicalName);
    else{
	snprintf(cn, sizeof(cn), "%.3d,%.3d,%.3d",
	        GetRValue(cr), GetGValue(cr), GetBValue(cr));
	return(cn);
    }
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *                  Toolbar setup routines.
 *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
LOCAL void
TBToggle (HWND hWnd)
{
    PTTYINFO		pTTYInfo;


    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
    if (pTTYInfo == NULL)
	return;

    if (pTTYInfo->toolBarSize > 0)
	TBHide (hWnd);
    else
	TBShow (hWnd);
}


LOCAL void
TBPosToggle (HWND hWnd)
{
    PTTYINFO		pTTYInfo;


    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
    if (pTTYInfo == NULL)
	return;

    pTTYInfo->toolBarTop = !pTTYInfo->toolBarTop;
    if(pTTYInfo->hTBWnd){
	TBHide (hWnd);
	TBShow (hWnd);
    }
}


LOCAL void
TBShow (HWND hWnd)
{
    PTTYINFO		pTTYInfo;
    RECT		rc;


    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
    if (pTTYInfo == NULL)
	return;


    /*
     * Make sure the tool bar not already shown.
     */
    if (pTTYInfo->toolBarSize > 0)
	return;



    /*
     * Make procinstance for dialog funciton.
     */
    HideCaret (hWnd);
    if (gToolBarProc == NULL)
	gToolBarProc = (DLGPROC) MakeProcInstance( (FARPROC) ToolBarProc,
                                                ghInstance ) ;
    if (gTBBtnProc == NULL)
	gTBBtnProc = (WNDPROC) MakeProcInstance( (FARPROC) TBBtnProc,
						ghInstance ) ;

	
    /*
     * Create the dialog box.
     */
    pTTYInfo->hTBWnd = CreateDialog (ghInstance,
		    MAKEINTRESOURCE (pTTYInfo->curToolBarID),
		    hWnd,
		    gToolBarProc);
    if (pTTYInfo->hTBWnd == NULL) {
	ShowCaret (hWnd);
	return;
    }

    SetFocus (hWnd);


    /*
     * Adjust the window size.
     */
    GetWindowRect (pTTYInfo->hTBWnd, &rc);	/* Get Toolbar size. */
    pTTYInfo->toolBarSize = (CORD)(rc.bottom - rc.top);

    GetClientRect (hWnd, &rc);			/* Get TTY window size. */
    SizeTTY (hWnd, 0, (CORD)rc.bottom, (CORD)rc.right);
    ShowCaret (hWnd);
}


LOCAL void
TBHide (HWND hWnd)
{
    PTTYINFO		pTTYInfo;
    RECT		rc;

    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
    if (pTTYInfo == NULL)
	return;


    if (pTTYInfo->toolBarSize == 0)
	return;

    DestroyWindow (pTTYInfo->hTBWnd);
    pTTYInfo->hTBWnd = NULL;
    if (pTTYInfo->toolBarBtns != NULL)
	MemFree (pTTYInfo->toolBarBtns);
    pTTYInfo->toolBarBtns = NULL;


    /*
     * Adjust the window size.
     */
    pTTYInfo->toolBarSize = 0;
    GetClientRect (hWnd, &rc);
    SizeTTY (hWnd, 0, (CORD)rc.bottom, (CORD)rc.right);
}


LOCAL void
TBSwap (HWND hWnd, int newID)
{
    PTTYINFO		pTTYInfo;
    RECT		rc;

    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
    if (pTTYInfo == NULL)
	return;

    if (pTTYInfo->toolBarSize == 0 || pTTYInfo->curToolBarID == newID)
	return;

    /*
     * Dispose of old tool bar window.
     */
    HideCaret (hWnd);

    DestroyWindow (pTTYInfo->hTBWnd);
    pTTYInfo->hTBWnd = NULL;
    if (pTTYInfo->toolBarBtns != NULL)
	MemFree (pTTYInfo->toolBarBtns);
    pTTYInfo->toolBarBtns = NULL;



    /*
     * Create the new dialog box.
     */
    pTTYInfo->hTBWnd = CreateDialog (ghInstance,
		    MAKEINTRESOURCE (newID),
		    hWnd,
		    gToolBarProc);
    if (pTTYInfo->hTBWnd == NULL) {
	ShowCaret (hWnd);
	return;
    }
    pTTYInfo->curToolBarID = newID;
    SetFocus (hWnd);		/* Return focus to parent. */


    /*
     * Fit new tool bar into old tool bars position.  This assumes that
     * all tool bars are about the same height.
     */
    GetClientRect (hWnd, &rc);			/* Get TTY window size. */
    if (pTTYInfo->toolBarTop)
	/* Position at top of window. */
	SetWindowPos (pTTYInfo->hTBWnd, HWND_TOP,
		0, 0,
		rc.right, pTTYInfo->toolBarSize,
		SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    else
	/* Position at bottom of window. */
	SetWindowPos (pTTYInfo->hTBWnd, HWND_TOP,
		0, pTTYInfo->ySize - pTTYInfo->toolBarSize,
		rc.right, pTTYInfo->toolBarSize,
		SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);

    ShowCaret (hWnd);
}


BOOL FAR PASCAL __export
ToolBarProc (HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    RECT		rc;
    BOOL		ret;
    int			height;
    HBRUSH		hBrush;
    HWND		hCld;
    int			btnCount;
    int			i;
    PTTYINFO		pTTYInfo;


    pTTYInfo = gpTTYInfo;

    ret = FALSE;
    switch (msg) {
	
    case WM_INITDIALOG:
	/* Fit dialog to window. */
	GetWindowRect (hWnd, &rc);
	height = rc.bottom - rc.top;
	GetClientRect (GetParent (hWnd), &rc);
	SetWindowPos (hWnd, HWND_TOP, 0, 0, rc.right, height,
			SWP_NOZORDER | SWP_NOACTIVATE);

	/* Count child windows.*/
	btnCount = 0;
	for (hCld = GetWindow (hWnd, GW_CHILD);
	     hCld;
	     hCld = GetWindow (hCld, GW_HWNDNEXT))
		++btnCount;

	/* Allocate a list of previous child procs. */
	if (pTTYInfo->toolBarBtns != NULL)
	    MemFree (pTTYInfo->toolBarBtns);
	pTTYInfo->toolBarBtns = MemAlloc (sizeof (BtnList) * (btnCount + 1));
	
	/* Subclass all child windows. */
	for (i = 0, hCld = GetWindow (hWnd, GW_CHILD);
	    hCld;
	    ++i, hCld = GetWindow (hCld, GW_HWNDNEXT)) {
	    pTTYInfo->toolBarBtns[i].wndID = GET_ID (hCld);
	    pTTYInfo->toolBarBtns[i].wndProc =
					(WNDPROC)(LONG_PTR)MyGetWindowLongPtr (hCld,
					GWLP_WNDPROC);
	    MySetWindowLongPtr (hCld, GWLP_WNDPROC, (void *)(LONG_PTR)TBBtnProc);
        }
        pTTYInfo->toolBarBtns[i].wndID = 0;
	pTTYInfo->toolBarBtns[i].wndProc = NULL;

	ret = FALSE;
	break;
	

    case WM_COMMAND:
	if (wParam >= KS_RANGESTART && wParam <= KS_RANGEEND){
	    ProcessMenuItem (GetParent (hWnd), wParam);
	    /* Set input focus back to parent. */
	    SetFocus (GetParent (hWnd));
	    ret = TRUE;
	    break;
	}
	break;

#ifdef	WIN32
    case WM_CTLCOLORBTN:
#else
    case WM_CTLCOLOR:
#endif
	if (HIWORD (lParam) == CTLCOLOR_DLG) {
	    if(pTTYInfo->hTBBrush != NULL){
		DeleteObject(pTTYInfo->hTBBrush);
		pTTYInfo->hTBBrush = NULL;
	    }

	    hBrush = CreateSolidBrush (GetSysColor (COLOR_ACTIVEBORDER));
	    return ((BOOL)!!(pTTYInfo->hTBBrush = hBrush));
        }
    }
    return (ret);
}


/*
 * Subclass toolbar button windows.
 *
 * These buttons will automatically return the input focus to
 * the toolbar's parent
 */
LRESULT FAR PASCAL __export
TBBtnProc (HWND hBtn, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    PTTYINFO		pTTYInfo;
    HWND		hPrnt;
    int			i;
    WORD		id;
    LRESULT		ret;
    WNDPROC		wndProc;


    /*
     * Find previous window proc.
     */
    pTTYInfo = gpTTYInfo;
    id = GET_ID (hBtn);
    for (i = 0; pTTYInfo->toolBarBtns[i].wndID != 0; ++i)
	if (pTTYInfo->toolBarBtns[i].wndID == id)
	    goto FoundWindow;
    /* Whoops!  Didn't find window, don't know how to pass message. */
    return (0);


FoundWindow:
    wndProc = pTTYInfo->toolBarBtns[i].wndProc;



    if (uMsg == WM_LBUTTONUP || uMsg == WM_MBUTTONUP || uMsg == WM_RBUTTONUP) {
	/*
	 * On mouse button up restore input focus to IDC_RESPONCE, which
	 * processes keyboard input.
	 */
	ret = CallWindowProc (wndProc, hBtn, uMsg, wParam, lParam);
	hPrnt = GetParent (GetParent (hBtn));
	if (hPrnt)
	    SetFocus (hPrnt);
	return (ret);
    }

    return (CallWindowProc (wndProc, hBtn, uMsg, wParam, lParam));
}


/*
 * return bitmap of allowed Edit Menu items
 */
LOCAL UINT
UpdateEditAllowed(HWND hWnd)
{
    PTTYINFO		pTTYInfo;
    UINT		fAccel = EM_NONE;

    if((pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO)) != NULL){
	if(EditPasteAvailable())
	  fAccel |= (EM_PST | EM_PST_ABORT);
	else if(IsClipboardFormatAvailable (CF_UNICODETEXT) && gPasteEnabled)
	  fAccel |= EM_PST;

	if(SelAvailable()){
	    fAccel |= (EM_CP | EM_CP_APPEND);
	}
	else{
	    if(gAllowCut)
	      fAccel |= EM_CUT;

	    if(gAllowCopy)
	      fAccel |= (EM_CP | EM_CP_APPEND);
	}

	if (pTTYInfo->menuItems[KS_WHEREIS - KS_RANGESTART].miActive)
	  fAccel |= EM_FIND;
    }
    return(fAccel);
}


#ifdef ACCELERATORS
#ifdef ACCELERATORS_OPT
/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *      Accelorator key routines.
 *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
LOCAL void
AccelCtl (HWND hWnd, int ctl, BOOL saveChange)
{
    PTTYINFO		pTTYInfo;
    BOOL		load, changed;


    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
    if (pTTYInfo == NULL)
	return;

    switch (ctl) {
      case ACCEL_LOAD :
	load = TRUE;
	break;
      case ACCEL_UNLOAD :
	load = FALSE;
	break;
      case ACCEL_TOGGLE :
	load = pTTYInfo->hAccel == NULL;
	break;
      default :
	load = FALSE;
	break;
    }

    changed = FALSE;
    if (load && pTTYInfo->hAccel == NULL) {
	/* Load em up. */
	pTTYInfo->hAccel = LoadAccelerators (ghInstance,
					MAKEINTRESOURCE (IDR_ACCEL_PINE));
	changed = TRUE;
    }
    else if(!load && pTTYInfo->hAccel) {
	/* unload em. */
	FreeResource (pTTYInfo->hAccel);
	pTTYInfo->hAccel = NULL;
	changed = TRUE;
    }

    if (changed && saveChange)
	DidResize (pTTYInfo);
}
#endif


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *      Accelorator key routines.
 *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
LOCAL void
AccelManage (HWND hWnd, long accels)
{
    PTTYINFO		pTTYInfo;
    ACCEL		accelarray[EM_MAX_ACCEL];
    int			n;
    static ACCEL am_cp = {
	FVIRTKEY | FCONTROL | FSHIFT | FNOINVERT, 'C', IDM_EDIT_COPY
    };
    static ACCEL am_cp_append = {
	FVIRTKEY | FCONTROL | FALT | FNOINVERT, 'C', IDM_EDIT_COPY_APPEND
    };
    static ACCEL am_find = {
	FVIRTKEY | FCONTROL | FSHIFT | FNOINVERT, 'F', IDM_MI_WHEREIS
    };
    static ACCEL am_pst = {
	FVIRTKEY | FCONTROL | FSHIFT | FNOINVERT, 'V', IDM_EDIT_PASTE
    };
    static ACCEL am_pst_abort = {
	FVIRTKEY | FCONTROL | FALT | FNOINVERT, 'V', IDM_EDIT_CANCEL_PASTE
    };
    static ACCEL am_cut = {
	FVIRTKEY | FCONTROL | FSHIFT | FNOINVERT, 'X', IDM_EDIT_CUT
    };

    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
    if (pTTYInfo == NULL || pTTYInfo->fAccel == (UINT)accels)
	return;

    if(pTTYInfo->hAccel){
	DestroyAcceleratorTable(pTTYInfo->hAccel);
	pTTYInfo->hAccel = NULL;
	pTTYInfo->fAccel = EM_NONE;
    }

    n = 0;

    if(accels & EM_CP)
      accelarray[n++] = am_cp;

    if(accels & EM_CP_APPEND)
      accelarray[n++] = am_cp_append;

    if(accels & EM_FIND)
      accelarray[n++] = am_find;

    if(accels & EM_PST)
      accelarray[n++] = am_pst;

    if(accels & EM_PST_ABORT)
      accelarray[n++] = am_pst_abort;

    if(accels & EM_CUT)
      accelarray[n++] = am_cut;

    /* Load em up. */
    if(n && (pTTYInfo->hAccel = CreateAcceleratorTable(accelarray, n)))
      pTTYInfo->fAccel = accels;
    else
      pTTYInfo->fAccel = EM_NONE;
}


LOCAL void
UpdateAccelerators (HWND hWnd)
{
    AccelManage (hWnd, UpdateEditAllowed(hWnd));
}
#endif


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *                  Mouse Selection routines
 *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

LOCAL BOOL	SelSelected = FALSE;
LOCAL BOOL	SelTracking = FALSE;
LOCAL int	SelAnchorRow;
LOCAL int	SelAnchorCol;
LOCAL int	SelPointerRow;
LOCAL int	SelPointerCol;
typedef struct {
    TCHAR	*pRow;
    int		len;
} CopyRow;


LOCAL void
SelRSet (int oStart, int oEnd)
{
    CharAttrib	*pca;

    for (pca = gpTTYInfo->pAttrib + oStart; oStart < oEnd; ++pca, ++oStart)
      pca->style |= CHAR_ATTR_SEL;
}


LOCAL void
SelRClear (int oStart, int oEnd)
{
    CharAttrib	*pca;

    for (pca = gpTTYInfo->pAttrib + oStart; oStart < oEnd; ++pca, ++oStart)
	pca->style &= ~CHAR_ATTR_SEL;
}


LOCAL void
SelRInvalidate (int oStart, int oEnd)
{
    RECT	rect;
    int		sRow, sCol;
    int		eRow, eCol;

    sRow = oStart / gpTTYInfo->actNColumn;
    sCol = oStart % gpTTYInfo->actNColumn;
    eRow = oEnd   / gpTTYInfo->actNColumn;
    eCol = oEnd   % gpTTYInfo->actNColumn;

    rect.top = (sRow * gpTTYInfo->yChar) + gpTTYInfo->yOffset;
    rect.bottom = ((eRow+1) * gpTTYInfo->yChar) + gpTTYInfo->yOffset;
    if (sRow == eRow) {
	rect.left = (sCol * gpTTYInfo->xChar) + gpTTYInfo->xOffset;
	rect.right = ((eCol+1) * gpTTYInfo->xChar) + gpTTYInfo->xOffset;
    } else {
	rect.left = gpTTYInfo->xOffset;
	rect.right = (gpTTYInfo->actNColumn * gpTTYInfo->xChar) +
			gpTTYInfo->xOffset;
    }
    InvalidateRect (ghTTYWnd, &rect, FALSE);
}


/*
 * Start a mouse selection.
 */
LOCAL void
SelStart (int nRow, int nColumn)
{
    SelClear ();
    SelTracking = TRUE;
    SelSelected = TRUE;
    SelPointerRow = SelAnchorRow = nRow;
    SelPointerCol = SelAnchorCol = nColumn;
    return;
}


/*
 * Finish a mouse selection.
 */
LOCAL void
SelFinish (int nRow, int nColumn)
{
    if (nRow == SelAnchorRow && nColumn == SelAnchorCol) {
	/* Mouse up in same place it went down - no selection. */
	SelClear ();
    }
    else {
	/* Update screen selection and set final position of mouse
	 * then turn of mouse tracking.  Selection remains in effect
	 * until SelClear is called. */
	SelTrackMouse (nRow, nColumn);
	SelTracking = FALSE;
    }
}


LOCAL void
SelClear (void)
{
    int		a, p;
    int		s, e;

    if (!SelSelected)
	return;

    /* Convert the anchor and point coordinates to offsets then
     * order the offsets. */
    a = (SelAnchorRow * gpTTYInfo->actNColumn) + SelAnchorCol;
    p = (SelPointerRow * gpTTYInfo->actNColumn) + SelPointerCol;
    if (a < p) {
	s = a;
	e = p;
    } else {
	s = p;
	e = a;
    }

    /* Clear selected attribute of those cells in range. */
    SelRClear (s, e);
    SelRInvalidate (s, e);
    SelSelected = FALSE;
    SelTracking = FALSE;
}


/*
 * Update the position of the mouse point.
 */
LOCAL void
SelTrackXYMouse (int xPos, int yPos)
{
    int		nRow;
    int		nColumn;

    nColumn = (xPos - gpTTYInfo->xOffset) / gpTTYInfo->xChar;
    nRow = (yPos - gpTTYInfo->yOffset) / gpTTYInfo->yChar;

    SelTrackMouse (nRow, nColumn);
}


/*
 * Update the position of the mouse point.
 */
LOCAL void
SelTrackMouse (int nRow, int nColumn)
{
    int		a, p, n;

    if (!SelTracking)
	return;

    /* Constrain the cel position to be on the screen.  But allow
     * for the Column to be one past the right edge of the screen so
     * the user can select the right most cel of a row. */
    nColumn = MAX(0, nColumn);
    nColumn = MIN(gpTTYInfo->actNColumn, nColumn);
    nRow = MAX(0, nRow);
    nRow = MIN(gpTTYInfo->actNRow-1, nRow);


    /* Convert the anchor, previous mouse position, and new mouse
     * position to offsets. */
    a = (SelAnchorRow * gpTTYInfo->actNColumn) + SelAnchorCol;
    p = (SelPointerRow * gpTTYInfo->actNColumn) + SelPointerCol;
    n = (nRow * gpTTYInfo->actNColumn) + nColumn;

    /* If previous position same as current position, do nothing. */
    if (p == n)
	return;

    /* there are six possible orderings of the points, each with
     * a different action:
     *	order		clear		set		redraw
     *	n p a				n - p		n - p
     *	p n a		p - n				p - n
     *	p a n		p - a		a - n		p - n
     *	a p n				p - n		p - n
     *  a n p		n - p				n - p
     *  n a p		a - p		n - a		n - p
     */
    if (p < a) {
	if (n < a) {
	    if (n < p) {
		SelRSet (n, p);
		SelRInvalidate (n, p);
	    } else {
		SelRClear (p, n);
		SelRInvalidate (p, n);
	    }
	} else {
	    SelRClear (p, a);
	    SelRSet (a, n);
	    SelRInvalidate (p, n);
        }
    } else {
	if (n > a) {
	    if (n > p) {
		SelRSet (p, n);
		SelRInvalidate (p, n);
	    } else {
		SelRClear (n, p);
		SelRInvalidate (n, p);
	    }
	} else {
	    SelRClear (a, p);
	    SelRSet (n, a);
	    SelRInvalidate (n, p);
        }
    }

    /* Set new pointer. */
    SelPointerRow = nRow;
    SelPointerCol = nColumn;
}


LOCAL BOOL
SelAvailable (void)
{
    return (SelSelected);
}


/*
 * Copy screen data to clipboard.  Actually appends data from screen to
 * existing handle so as we can implement a "Copy Append" to append to
 * existing clipboard data.
 *
 * The screen does not have real line terminators.  We decide where the
 * actual screen data ends by scanning the line (row) from end backwards
 * to find the first non-space.
 *
 * I don't know how many bytes of data I'll be appending to the clipboard.
 * So I implemented in two passes.  The first finds all the row starts
 * and length while the second copies the data.
 */
LOCAL void
SelDoCopy (HANDLE hCB, DWORD lenCB)
{
    HANDLE		newCB;		/* Used in reallocation. */
    TCHAR		*pCB;		/* Points to CB data. */	
    TCHAR		*p2;		/* Temp pointer to screen data. */
    int			sRow, eRow;	/* Start and End Rows. */
    int			sCol, eCol;	/* Start and End columns. */
    int			row, c1, c2;	/* temp row and column indexes. */
    int			totalLen;	/* total len of new data. */
    CopyRow		*rowTable, *rp;	/* pointers to table of rows. */
    BOOL		noLastCRLF = FALSE;


    if (OpenClipboard (ghTTYWnd)) {		/* ...and we get the CB. */
      if (EmptyClipboard ()) {		/* ...and clear previous CB.*/


	/* Find the start and end row and column. */
	if ( (SelAnchorRow * gpTTYInfo->actNColumn) + SelAnchorCol <
	     (SelPointerRow * gpTTYInfo->actNColumn) + SelPointerCol) {
	    sRow = SelAnchorRow;
	    sCol = SelAnchorCol;
	    eRow = SelPointerRow;
	    eCol = SelPointerCol;
	}
	else {
	    sRow = SelPointerRow;
	    sCol = SelPointerCol;
	    eRow = SelAnchorRow;
	    eCol = SelAnchorCol;
	}

	/* Allocate a table in which we store info on rows. */
	rowTable = (CopyRow *) MemAlloc (sizeof (CopyRow) * (eRow-sRow+1));
	if (rowTable == NULL)
	    goto Fail1;

	/* Find the start and length of each row. */
	totalLen = 0;
	for (row = sRow, rp = rowTable; row <= eRow; ++row, ++rp) {
	    /* Find beginning and end columns, which depends on if
	     * this is the first or last row in the selection. */
	    c1 = (row == sRow ? sCol : 0);
	    c2 = (row == eRow ? eCol : gpTTYInfo->actNColumn);

	    /* Calculate pointer to beginning of this line. */
	    rp->pRow = gpTTYInfo->pScreen +
		    ((row * gpTTYInfo->actNColumn) + c1);

	    /* Back down from end column to find first non space.
	     * noLastCRLF indicates if it looks like the selection
	     * should include a CRLF on the end of the line.  It
	     * gets set for each line, but only the setting for the
	     * last line in the selection is remembered (which is all
	     * we're interested in). */
	    p2 = gpTTYInfo->pScreen +
		    ((row * gpTTYInfo->actNColumn) + c2);
	    noLastCRLF = TRUE;
	    while (c2 > c1) {
		if (*(p2-1) != (TCHAR)' ')
		    break;
		noLastCRLF = FALSE;
		--c2;
		--p2;
	    }

	    /* Calculate size of line, then increment totalLen plus 2 for
	     * the CRLF which will terminate each line. */
	    rp->len = c2 - c1;
	    totalLen += rp->len + 2;
	}

	/* Reallocate the memory block.  Add one byte for null terminator. */
	newCB = GlobalReAlloc (hCB, (lenCB + totalLen + 1)*sizeof(TCHAR), 0);
	if (newCB == NULL)
	    goto Fail2;
	hCB = newCB;

	pCB = GlobalLock (hCB);
	if (pCB == NULL)
	    goto Fail2;

	/* Append each of the rows, deliminated by a CRLF. */
	pCB += lenCB;
	for (row = sRow, rp = rowTable; row <= eRow; ++row, ++rp) {
	    if (rp->len > 0) {
		memcpy (pCB, rp->pRow, rp->len * sizeof(TCHAR));
		pCB += rp->len;
	    }
	    if (row < eRow || !noLastCRLF) {
		*pCB++ = (TCHAR)ASCII_CR;
		*pCB++ = (TCHAR)ASCII_LF;
	    }
	}
	*pCB = (TCHAR)'\0';			/* Null terminator. */
	MemFree (rowTable);
	GlobalUnlock (hCB);

	/* Attempt to pass the data to the clipboard, then release clipboard
	 * and exit function. */
	if (SetClipboardData (CF_UNICODETEXT, hCB) == NULL)
		      /* Failed!  Free the data. */
		      GlobalFree (hCB);
	CloseClipboard ();
      }
    }
    return;


	/* Error exit. */
Fail2:	MemFree (rowTable);
Fail1:	GlobalFree (hCB);
	CloseClipboard ();
	return;
}



/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *                  Upper Layer Screen routines.
 *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*
 * Flush the write accumulator buffer.
 */
LOCAL void
FlushWriteAccum (void)
{
    if (gpTTYInfo->writeAccumCount > 0) {
	WriteTTYText (ghTTYWnd, gpTTYInfo->writeAccum,
					gpTTYInfo->writeAccumCount);
	gpTTYInfo->writeAccumCount = 0;
    }
}


/*
 * Set window's title
 */
void
mswin_settitle(char *utf8_title)
{
    TCHAR buf[256];
    LPTSTR lptstr_title;

    lptstr_title = utf8_to_lptstr(utf8_title);
    _sntprintf(buf, 256, TEXT("%.*s - Alpine"), 80, lptstr_title);
    fs_give((void **) &lptstr_title);

    SetWindowText(ghTTYWnd, buf);
}


/*
 * Return the application instance.
 */
WINHAND
mswin_gethinstance ()
{
    return ((WINHAND)ghInstance);
}


WINHAND
mswin_gethwnd ()
{
    return ((WINHAND)ghTTYWnd);
}


/*
 * destroy splash screen
 */
void
mswin_killsplash()
{
    if(ghSplashWnd != NULL){
	DestroyWindow(ghSplashWnd);
	ghSplashWnd = NULL;
    }
}


/*
 *	Called to get mouse event.
 */
int
mswin_getmouseevent (MEvent * pMouse)
{
	return (MQGet (pMouse));
}


/*
 * Make a pop-up menu and track it
 */
int
mswin_popup(MPopup *members)
{
    HMENU   hMenu;
    POINT   point;
    MPopup *mp;
    int	    n = 1;

    if(members && (hMenu = CreatePopupMenu())){

	PopupConfig(hMenu, members, &n);
	if(GetCursorPos(&point)
	   && (n = TrackPopupMenu(hMenu,
				  TPM_LEFTALIGN | TPM_TOPALIGN
				    | TPM_RIGHTBUTTON | TPM_LEFTBUTTON
				    | TPM_NONOTIFY | TPM_RETURNCMD,
				  point.x, point.y, 0, ghTTYWnd, NULL))
	   && (mp = PopupId(members, n))){
	    /* Find the member with the internal.id of n */
	    n = mp->internal.id - 1;	/* item's "selectable" index */
	    switch(mp->type){
	      case tQueue :
		CQAdd (mp->data.val, 0);
		break;

	      case tMessage :
		SendMessage(ghTTYWnd, WM_COMMAND, mp->data.msg, 0);
		break;

	      default :
		break;
	    }
	}
	else
	  n = -1;		/* error or nothing selected; same diff */

	DestroyMenu(hMenu);
    }
    else
      n = -1;			/* error */

    return(n);
}


LOCAL void
PopupConfig(HMENU hMenu, MPopup *members, int *n)
{
    MENUITEMINFO  mitem;
    int		  index;
    TCHAR         tcbuf[256];

    for(index = 0; members->type != tTail; members++, index++){
	memset(&mitem, 0, sizeof(MENUITEMINFO));
	mitem.cbSize = sizeof(MENUITEMINFO);
	mitem.fMask  = MIIM_TYPE;

	if(members->type == tSubMenu){
	    mitem.fMask |= MIIM_SUBMENU;
	    mitem.fType  = MFT_STRING;
	    if(mitem.hSubMenu = CreatePopupMenu()){
		/* members->label.string is still just a char * */
		_sntprintf(tcbuf, sizeof(tcbuf)/sizeof(TCHAR),
			   TEXT("%S"), members->label.string);
		mitem.dwTypeData = tcbuf;
		mitem.cch	 = (UINT)_tcslen(tcbuf);
		InsertMenuItem(hMenu, index, TRUE, &mitem);

		PopupConfig(mitem.hSubMenu, members->data.submenu, n);
	    }
	}
	else{
	    switch(members->type){
	      case tSeparator :
		mitem.fType = MFT_SEPARATOR;
		break;

	      default :
		mitem.fMask	 |= MIIM_ID;
		mitem.wID	  = members->internal.id = (*n)++;
		switch(members->label.style){
		  case lChecked :
		    mitem.fMask	 |= MIIM_STATE;
		    mitem.fState  = MFS_CHECKED;
		    break;

		  case lDisabled :
		    mitem.fMask	 |= MIIM_STATE;
		    mitem.fState  = MFS_GRAYED;
		    break;

		  case lNormal :
		  default :
		    break;
		}

		mitem.fType	 = MFT_STRING;
		_sntprintf(tcbuf, sizeof(tcbuf)/sizeof(TCHAR),
			   TEXT("%S"), members->label.string);
		mitem.dwTypeData = tcbuf;
		mitem.cch	 = (UINT)_tcslen(tcbuf);
		break;
	    }

	    InsertMenuItem(hMenu, index, TRUE, &mitem);
	}
    }
}


LOCAL MPopup *
PopupId(MPopup *members, int id)
{
    MPopup *mp;

    for(; members->type != tTail; members++)
      switch(members->type){
	case tSubMenu :
	  if(mp = PopupId(members->data.submenu, id))
	    return(mp);

	  break;

	case tSeparator :
	  break;

	default :
	  if(members->internal.id == id)
	    return(members);

	  break;
      }

    return(NULL);
}


/*
 * Make a pop-up offering Copy/Cut/Paste menu and track it
 */
LOCAL BOOL
CopyCutPopup(HWND hWnd, UINT fAllowed)
{
    MENUITEMINFO  mitem;
    HMENU	  hMenu;
    POINT	  point;
    BOOL	  rv = FALSE;
    int		  n = -1;

    /*
     * If nothing to do just silently return
     */
    if(fAllowed != EM_NONE && (hMenu = CreatePopupMenu())){
	if(fAllowed & EM_CUT){
	    /* Insert a "Copy" option */
	    memset(&mitem, 0, sizeof(MENUITEMINFO));
	    mitem.cbSize = sizeof(MENUITEMINFO);
	    mitem.fMask = MIIM_TYPE | MIIM_ID;
	    mitem.wID = IDM_EDIT_CUT;
	    mitem.fType = MFT_STRING;
	    mitem.dwTypeData = TEXT("Cut");
	    mitem.cch = 3;
	    InsertMenuItem(hMenu, ++n, FALSE, &mitem);
	}

	if(fAllowed & EM_CP){
	    /* Insert a "Copy" option */
	    memset(&mitem, 0, sizeof(MENUITEMINFO));
	    mitem.cbSize = sizeof(MENUITEMINFO);
	    mitem.fMask = MIIM_TYPE | MIIM_ID;
	    mitem.wID = IDM_EDIT_COPY;
	    mitem.fType = MFT_STRING;
	    mitem.dwTypeData = TEXT("Copy");
	    mitem.cch = 4;
	    InsertMenuItem(hMenu, ++n, FALSE, &mitem);
	}

	if(fAllowed & EM_CP_APPEND){
	    /* Insert a "Copy Append" option */
	    memset(&mitem, 0, sizeof(MENUITEMINFO));
	    mitem.cbSize = sizeof(MENUITEMINFO);
	    mitem.fMask = MIIM_TYPE | MIIM_ID;
	    mitem.wID = IDM_EDIT_COPY_APPEND;
	    mitem.fType = MFT_STRING;
	    mitem.dwTypeData = TEXT("Copy Append");
	    mitem.cch = 11;
	    InsertMenuItem(hMenu, ++n, FALSE, &mitem);
	}

	if(fAllowed & EM_PST){
	    /* Insert a "Paste" option */
	    memset(&mitem, 0, sizeof(MENUITEMINFO));
	    mitem.cbSize = sizeof(MENUITEMINFO);
	    mitem.fMask = MIIM_TYPE | MIIM_ID;
	    mitem.wID = IDM_EDIT_PASTE;
	    mitem.fType = MFT_STRING;
	    mitem.dwTypeData = TEXT("Paste");
	    mitem.cch = 5;
	    InsertMenuItem(hMenu, ++n, FALSE, &mitem);
	}

	if((fAllowed & EM_SEL_ALL) && !(fAllowed & (EM_CP | EM_CP_APPEND))){
	    /* Insert a "Select All" option */
	    memset(&mitem, 0, sizeof(MENUITEMINFO));
	    mitem.cbSize = sizeof(MENUITEMINFO);
	    mitem.fMask = MIIM_TYPE | MIIM_ID;
	    mitem.wID = IDM_EDIT_SEL_ALL;
	    mitem.fType = MFT_STRING;
	    mitem.dwTypeData = TEXT("Select &All");
	    mitem.cch = 11;
	    InsertMenuItem(hMenu, ++n, FALSE, &mitem);
	}

	if(n >= 0 && GetCursorPos(&point)){
	    TrackPopupMenu(hMenu,
			   TPM_LEFTALIGN | TPM_TOPALIGN
			   | TPM_RIGHTBUTTON | TPM_LEFTBUTTON,
			   point.x, point.y, 0, hWnd, NULL);
	    rv = TRUE;
	}

	DestroyMenu(hMenu);
    }

    return(rv);
}



/*
 *
 */
void
pico_popup()
{
    MENUITEMINFO  mitem;
    HMENU	  hMenu;
    POINT	  point;
    UINT	  fAllow;
    int		  n = -1;

    /*
     * If nothing to do just silently return
     */
    if(hMenu = CreatePopupMenu()){

	if((fAllow = UpdateEditAllowed(ghTTYWnd)) != EM_NONE){
	    /* Insert a "Cut" option */
	    memset(&mitem, 0, sizeof(MENUITEMINFO));
	    mitem.cbSize = sizeof(MENUITEMINFO);
	    mitem.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
	    mitem.wID = IDM_EDIT_CUT;
	    mitem.fState = (fAllow & EM_CUT) ? MFS_ENABLED : MFS_GRAYED;
	    mitem.fType = MFT_STRING;
	    mitem.dwTypeData = TEXT("Cut");
	    mitem.cch = 3;
	    InsertMenuItem(hMenu, ++n, FALSE, &mitem);

	    /* Insert a "Copy" option */
	    memset(&mitem, 0, sizeof(MENUITEMINFO));
	    mitem.cbSize = sizeof(MENUITEMINFO);
	    mitem.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
	    mitem.wID = IDM_EDIT_COPY;
	    mitem.fState = (fAllow & EM_CP) ? MFS_ENABLED : MFS_GRAYED;
	    mitem.fType = MFT_STRING;
	    mitem.dwTypeData = TEXT("Copy");
	    mitem.cch = 4;
	    InsertMenuItem(hMenu, ++n, FALSE, &mitem);

	    /* Insert a "Copy Append" option */
	    memset(&mitem, 0, sizeof(MENUITEMINFO));
	    mitem.cbSize = sizeof(MENUITEMINFO);
	    mitem.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
	    mitem.wID = IDM_EDIT_COPY_APPEND;
	    mitem.fState = (fAllow & EM_CP_APPEND) ? MFS_ENABLED : MFS_GRAYED;
	    mitem.fType = MFT_STRING;
	    mitem.dwTypeData = TEXT("Copy Append");
	    mitem.cch = 11;
	    InsertMenuItem(hMenu, ++n, FALSE, &mitem);

	    /* Insert a "Paste" option */
	    memset(&mitem, 0, sizeof(MENUITEMINFO));
	    mitem.cbSize = sizeof(MENUITEMINFO);
	    mitem.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
	    mitem.wID = IDM_EDIT_PASTE;
	    mitem.fType = MFT_STRING;
	    mitem.dwTypeData = TEXT("Paste");
	    mitem.fState = (fAllow & EM_PST) ? MFS_ENABLED : MFS_GRAYED;
	    mitem.cch = 5;
	    InsertMenuItem(hMenu, ++n, FALSE, &mitem);
	}

	/* Insert a "Select All" option */
	memset(&mitem, 0, sizeof(MENUITEMINFO));
	mitem.cbSize = sizeof(MENUITEMINFO);
	mitem.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
	mitem.wID = IDM_EDIT_SEL_ALL;
	mitem.fType = MFT_STRING;
	mitem.fState = (fAllow & (EM_CP | EM_CP_APPEND))
			  ? MFS_GRAYED : MFS_ENABLED;
	mitem.dwTypeData = TEXT("Select &All");
	mitem.cch = 11;
	InsertMenuItem(hMenu, ++n, FALSE, &mitem);

	if(n >= 0 && GetCursorPos(&point))
	  TrackPopupMenu(hMenu,
			 TPM_LEFTALIGN | TPM_TOPALIGN
			 | TPM_RIGHTBUTTON | TPM_LEFTBUTTON,
			 point.x, point.y, 0, ghTTYWnd, NULL);

	DestroyMenu(hMenu);
    }
}



/*
 *
 */
void
mswin_paste_popup()
{
    MENUITEMINFO  mitem;
    HMENU	  hMenu;
    POINT	  point;
    UINT	  fAllow;
    int		  n = -1;

    /*
     * If nothing to do just silently return
     */
    if(hMenu = CreatePopupMenu()){

	if((fAllow = UpdateEditAllowed(ghTTYWnd)) != EM_NONE){
	    /* Insert a "Paste" option */
	    memset(&mitem, 0, sizeof(MENUITEMINFO));
	    mitem.cbSize = sizeof(MENUITEMINFO);
	    mitem.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
	    mitem.wID = IDM_EDIT_PASTE;
	    mitem.fType = MFT_STRING;
	    mitem.dwTypeData = TEXT("Paste");
	    mitem.fState = (fAllow & EM_PST) ? MFS_ENABLED : MFS_GRAYED;
	    mitem.cch = 5;
	    InsertMenuItem(hMenu, ++n, FALSE, &mitem);

	    if(n >= 0 && GetCursorPos(&point))
	      TrackPopupMenu(hMenu,
			     TPM_LEFTALIGN | TPM_TOPALIGN
			     | TPM_RIGHTBUTTON | TPM_LEFTBUTTON,
			     point.x, point.y, 0, ghTTYWnd, NULL);
	}

	DestroyMenu(hMenu);
    }
}



void
mswin_keymenu_popup()
{
    HMENU	 hBarMenu, hMenu;
    MENUITEMINFO mitem;
    POINT	 point;
    int		 i, j, n;
    TCHAR	 tcbuf[256];

    /*
     * run thru menubar from left to right and down each list building
     * a popup of all active members.  we run down the menu's rather
     * than thru the menuItems array so we can preserve order...
     */
    if((hMenu = CreatePopupMenu())){
	if(hBarMenu = GetMenu(ghTTYWnd)){
	    /* For each possible index, look for matching registered key  */
	    for(n = 0, i = 1; i <= KS_COUNT; i++)
	      for(j = 0; j < KS_COUNT; j++)
		if(gpTTYInfo->menuItems[j].miIndex == i){
		    if(gpTTYInfo->menuItems[j].miActive == TRUE
		       && gpTTYInfo->menuItems[j].miLabel){
			mitem.cbSize = sizeof(MENUITEMINFO);
			mitem.fMask =  MIIM_ID | MIIM_STATE | MIIM_TYPE;
			mitem.wID = j + KS_RANGESTART;
			mitem.fState = MFS_ENABLED;
			mitem.fType = MFT_STRING;
			/* miLabel is still plain old char *, not utf8 */
			_sntprintf(tcbuf, sizeof(tcbuf)/sizeof(TCHAR),
				   TEXT("%S"), gpTTYInfo->menuItems[j].miLabel);
			mitem.dwTypeData = tcbuf;
			mitem.cch = (UINT)_tcslen(tcbuf);
			InsertMenuItem(hMenu, n++, TRUE, &mitem);

			if(j + KS_RANGESTART == IDM_MI_SCREENHELP
			    && gHelpCallback){
			    mitem.cbSize = sizeof(MENUITEMINFO);
			    mitem.fMask =  MIIM_ID | MIIM_STATE | MIIM_TYPE;
			    mitem.wID = IDM_HELP;
			    mitem.fState = MFS_ENABLED;
			    mitem.fType = MFT_STRING;
			    _sntprintf(tcbuf, sizeof(tcbuf)/sizeof(TCHAR),
				       TEXT("%S"), "Help in New Window");
			    mitem.dwTypeData = tcbuf;
			    mitem.cch = (UINT)_tcslen(tcbuf);
			    InsertMenuItem(hMenu, n++, TRUE, &mitem);
			}
		    }

		    break;
		}

	    if(n > 0 && GetCursorPos(&point)){
		TrackPopupMenu(hMenu,
			       TPM_LEFTALIGN | TPM_TOPALIGN
			       | TPM_RIGHTBUTTON | TPM_LEFTBUTTON,
			       point.x, point.y, 0, ghTTYWnd, NULL);
	    }
	}

	DestroyMenu(hMenu);
    }
}



/*
 *
 */
void
mswin_registericon(int row, int id, char *utf8_file)
{
    LPTSTR    sPathCopy;
    HICON     hIcon;
    WORD      iIcon;
    IconList *pIcon;

    /* Turn this off until it can get tuned */
    return;

    /* Already registered? */
    for(pIcon = gIconList; pIcon; pIcon = pIcon->next)
      if(pIcon->id == id){
	  pIcon->row = (short)row;
	  return;
      }

    sPathCopy = utf8_to_lptstr(utf8_file);

    if(hIcon = ExtractAssociatedIcon(ghInstance, sPathCopy, &iIcon))
      MSWIconAddList(row, id, hIcon);

    fs_give((void **) &sPathCopy);
}


void
mswin_destroyicons()
{
    MSWIconFree(&gIconList);
}


void
MSWIconAddList(int row, int id, HICON hIcon)
{
    IconList **ppIcon;

    for(ppIcon = &gIconList; *ppIcon; ppIcon = &(*ppIcon)->next)
      ;

    *ppIcon = (IconList *) MemAlloc (sizeof (IconList));
    memset(*ppIcon, 0, sizeof(IconList));
    (*ppIcon)->hIcon = hIcon;
    (*ppIcon)->id = id;
    (*ppIcon)->row = (short)row;
}


int
MSWIconPaint(int row, HDC hDC)
{
    IconList *pIcon;
    int	      rv = 0;

    for(pIcon = gIconList; pIcon && pIcon->row != row; pIcon = pIcon->next)
      ;

    if(pIcon){
	/* Invalidate rectange covering singel character. */
	DrawIconEx(hDC, 0, (row * gpTTYInfo->yChar) + gpTTYInfo->yOffset,
		   pIcon->hIcon, 2 * gpTTYInfo->xChar, gpTTYInfo->yChar,
		   0, NULL, DI_NORMAL);

	rv = 1;
    }

    return(rv);
}


void
MSWIconFree(IconList **ppIcon)
{
    if(ppIcon && *ppIcon){
	if((*ppIcon)->next)
	  MSWIconFree(&(*ppIcon)->next);

	DestroyIcon((*ppIcon)->hIcon);
	MemFree (*ppIcon);
	*ppIcon = NULL;
    }
}


/*
 * Set up debugging stuff.
 */
void
mswin_setdebug (int debug, FILE *debugfile)
{
    /* Accept new file only if we don't already have a file. */
    if (mswin_debugfile == 0) {
	mswin_debug = debug;
	mswin_debugfile = debugfile;
	MemDebug (debug, mswin_debugfile);
    }
}


void
mswin_setnewmailwidth (int width)
{
    gNMW_width = width;
    gNMW_width = gNMW_width < 20 ? 20 : gNMW_width;
    gNMW_width = gNMW_width > 170 ? 170 : gNMW_width;
}


/*
 * Event handler to deal with File Drop events
 */
LOCAL BOOL
ProcessTTYFileDrop (HANDLE wDrop)
{
    HDROP hDrop = wDrop;
    POINT pos;
    int   col, row, i, n;
    TCHAR fname[1024];
    char *utf8_fname;

    if(!gpTTYInfo->dndhandler)
      return(FALSE);

    /* translate drop point to character cell */
    DragQueryPoint(hDrop, &pos);
    col = (pos.x - gpTTYInfo->xOffset) / gpTTYInfo->xChar;
    if (pos.x < gpTTYInfo->xOffset)
	--col;
    row = (pos.y - gpTTYInfo->yOffset) / gpTTYInfo->yChar;
    if (pos.y < gpTTYInfo->yOffset)
	--row;

    for(n = DragQueryFile(hDrop, (UINT)-1, NULL, 0), i = 0; i < n; i++){
	DragQueryFile(hDrop, i, fname, 1024);
	utf8_fname = lptstr_to_utf8(fname);
	gpTTYInfo->dndhandler (row, col, utf8_fname);
	fs_give((void **) &utf8_fname);
    }

    DragFinish(hDrop);

    set_time_of_last_input();

    return(TRUE);
}


/*
 * Set a callback to deal with Drag 'N Drop events
 */
int
mswin_setdndcallback (int (*cb)())
{
    if(gpTTYInfo->dndhandler)
      gpTTYInfo->dndhandler = NULL;

    if(cb){
	gpTTYInfo->dndhandler = cb;
	DragAcceptFiles(ghTTYWnd, TRUE);
    }

    return(1);
}


/*
 * Clear previously installed callback to handle Drag 'N Drop
 * events
 */
int
mswin_cleardndcallback ()
{
    gpTTYInfo->dndhandler = NULL;
    DragAcceptFiles(ghTTYWnd, FALSE);
    return(1);
}


/*
 * Set a callback for function 'ch'
 */
int
mswin_setresizecallback (int (*cb)())
{
    int		i;
    int		e;


    /*
     * Look through whole array for this call back function.  Don't
     * insert duplicate.  Also look for empty slot.
     */
    e = -1;
    for (i = 0; i < RESIZE_CALLBACK_ARRAY_SIZE; ++i) {
	if (gpTTYInfo->resizer[i] == cb)
	    return (0);
        if (e == -1 && gpTTYInfo->resizer[i] == NULL)
	    e = i;
    }

    /*
     * Insert in empty slot or return an error.
     */
    if (e != -1) {
	gpTTYInfo->resizer[e] = cb;
	return (0);
    }
    return (-1);
}


/*
 * Clear all instances of the callback function 'cb'
 */
int
mswin_clearresizecallback (int (*cb)())
{
    int		i;
    int		status;

    status = -1;
    for (i = 0; i < RESIZE_CALLBACK_ARRAY_SIZE; ++i) {
	if (gpTTYInfo->resizer[i] == cb) {
	    gpTTYInfo->resizer[i] = NULL;
	    status = 0;
	}
    }
    return (status);
}


void
mswin_beginupdate (void)
{
    gpTTYInfo->fMassiveUpdate = TRUE;
}


void
mswin_endupdate (void)
{
    gpTTYInfo->fMassiveUpdate = FALSE;
    MoveTTYCursor (ghTTYWnd);
}


int
mswin_charsetid2string(LPTSTR fontCharSet, size_t nfontCharSet, BYTE lfCharSet)
{
    TCHAR buf[1024];

    buf[0] = '\0';
    switch(lfCharSet){
      case DEFAULT_CHARSET:
	break;
      case ANSI_CHARSET:
	_tcsncpy(buf, TEXT("ANSI_CHARSET"), sizeof(buf)/sizeof(TCHAR));
	break;
      case OEM_CHARSET:
	_tcsncpy(buf, TEXT("OEM_CHARSET"), sizeof(buf)/sizeof(TCHAR));
	break;
      case BALTIC_CHARSET:
	_tcsncpy(buf, TEXT("BALTIC_CHARSET"), sizeof(buf)/sizeof(TCHAR));
	break;
      case CHINESEBIG5_CHARSET:
	_tcsncpy(buf, TEXT("CHINESE_CHARSET"), sizeof(buf)/sizeof(TCHAR));
	break;
      case EASTEUROPE_CHARSET:
	_tcsncpy(buf, TEXT("EASTEUROPE_CHARSET"), sizeof(buf)/sizeof(TCHAR));
	break;
      case GB2312_CHARSET:
	_tcsncpy(buf, TEXT("GF2312_CHARSET"), sizeof(buf)/sizeof(TCHAR));
	break;
      case GREEK_CHARSET:
	_tcsncpy(buf, TEXT("GREEK_CHARSET"), sizeof(buf)/sizeof(TCHAR));
	break;
      case HANGUL_CHARSET:
	_tcsncpy(buf, TEXT("HANGUL_CHARSET"), sizeof(buf)/sizeof(TCHAR));
	break;
      case MAC_CHARSET:
	_tcsncpy(buf, TEXT("MAC_CHARSET"), sizeof(buf)/sizeof(TCHAR));
	break;
      case RUSSIAN_CHARSET:
	_tcsncpy(buf, TEXT("RUSSIAN_CHARSET"), sizeof(buf)/sizeof(TCHAR));
	break;
      case SHIFTJIS_CHARSET:
	_tcsncpy(buf, TEXT("SHIFTJIS_CHARSET"), sizeof(buf)/sizeof(TCHAR));
	break;
      case SYMBOL_CHARSET:
	_tcsncpy(buf, TEXT("SYMBOL_CHARSET"), sizeof(buf)/sizeof(TCHAR));
	break;
      case TURKISH_CHARSET:
	_tcsncpy(buf, TEXT("TURKISH_CHARSET"), sizeof(buf)/sizeof(TCHAR));
	break;
      case VIETNAMESE_CHARSET:
	_tcsncpy(buf, TEXT("VIETNAMESE_CHARSET"), sizeof(buf)/sizeof(TCHAR));
	break;
      case JOHAB_CHARSET:
	_tcsncpy(buf, TEXT("JOHAB_CHARSET"), sizeof(buf)/sizeof(TCHAR));
	break;
      case ARABIC_CHARSET:
	_tcsncpy(buf, TEXT("ARABIC_CHARSET"), sizeof(buf)/sizeof(TCHAR));
	break;
      case HEBREW_CHARSET:
	_tcsncpy(buf, TEXT("HEBREW_CHARSET"), sizeof(buf)/sizeof(TCHAR));
	break;
      case THAI_CHARSET:
	_tcsncpy(buf, TEXT("THAI_CHARSET"), sizeof(buf)/sizeof(TCHAR));
	break;
      default:
	/* default char set if we can't figure it out */
	break;
    }

    buf[sizeof(buf)/sizeof(TCHAR) - 1] = '\0';

    _tcsncpy(fontCharSet, buf, nfontCharSet);
    fontCharSet[nfontCharSet-1] = '\0';

    return 0;
}


BYTE
mswin_string2charsetid(LPTSTR str)
{
    TCHAR tstr[1024];

    if(!str || (lstrlen(str) >= 1024))
      return DEFAULT_CHARSET;

    if((lstrlen(str) > lstrlen(TEXT("_CHARSET")))
       && (tcsucmp(str + lstrlen(str) - lstrlen(TEXT("_CHARSET")), TEXT("_CHARSET")) == 0)){
	_tcsncpy(tstr, str, 1024);
	tstr[lstrlen(str) - lstrlen(TEXT("_CHARSET"))] = '\0';
	tstr[1024-1] = '\0';
	if(tcsucmp(tstr, TEXT("ANSI")) == 0)
	  return ANSI_CHARSET;
	else if(tcsucmp(tstr, TEXT("DEFAULT")) == 0)
	  return DEFAULT_CHARSET;
	else if(tcsucmp(tstr, TEXT("OEM")) == 0)
	  return OEM_CHARSET;
	else if(tcsucmp(tstr, TEXT("BALTIC")) == 0)
	  return BALTIC_CHARSET;
	else if(tcsucmp(tstr, TEXT("CHINESEBIG5")) == 0)
	  return CHINESEBIG5_CHARSET;
	else if(tcsucmp(tstr, TEXT("EASTEUROPE")) == 0)
	  return EASTEUROPE_CHARSET;
	else if(tcsucmp(tstr, TEXT("GB2312")) == 0)
	  return GB2312_CHARSET;
	else if(tcsucmp(tstr, TEXT("GREEK")) == 0)
	  return GREEK_CHARSET;
	else if(tcsucmp(tstr, TEXT("HANGUL")) == 0)
	  return HANGUL_CHARSET;
	else if(tcsucmp(tstr, TEXT("MAC")) == 0)
	  return MAC_CHARSET;
	else if(tcsucmp(tstr, TEXT("RUSSIAN")) == 0)
	  return RUSSIAN_CHARSET;
	else if(tcsucmp(tstr, TEXT("SHIFTJIS")) == 0)
	  return SHIFTJIS_CHARSET;
	else if(tcsucmp(tstr, TEXT("SYMBOL")) == 0)
	  return SYMBOL_CHARSET;
	else if(tcsucmp(tstr, TEXT("TURKISH")) == 0)
	  return TURKISH_CHARSET;
	else if(tcsucmp(tstr, TEXT("VIETNAMESE")) == 0)
	  return VIETNAMESE_CHARSET;
	else if(tcsucmp(tstr, TEXT("JOHAB")) == 0)
	  return JOHAB_CHARSET;
	else if(tcsucmp(tstr, TEXT("ARABIC")) == 0)
	  return ARABIC_CHARSET;
	else if(tcsucmp(tstr, TEXT("HEBREW")) == 0)
	  return HEBREW_CHARSET;
	else if(tcsucmp(tstr, TEXT("THAI")) == 0)
	  return THAI_CHARSET;
	else
	  return DEFAULT_CHARSET;
    }
    else{
	if(_tstoi(str) > 0)
	  return((BYTE)(_tstoi(str)));
	else
	  return DEFAULT_CHARSET;
    }
}


int
mswin_setwindow(char *fontName_utf8, char *fontSize_utf8, char *fontStyle_utf8,
		char *windowPosition, char *caretStyle, char *fontCharSet_utf8)
{
    LOGFONT		newFont;
    int			newHeight;
    HDC			hDC;
    int			ppi;
    RECT		wndRect, cliRect;
    char		*c;
    char		*n;
    int			i;
    BOOL		toolBar = FALSE;
    BOOL		toolBarTop = TRUE;
    int			wColumns, wRows;
    int			wXPos, wYPos;
    int			wXBorder, wYBorder;
    int			wXSize, wYSize;
    char		wp[WIN_POS_STR_MAX_LEN + 1];
    int			showWin = 1;
    LPTSTR              fontName_lpt = NULL;
    LPTSTR              fontCharSet_lpt;

#ifdef SDEBUG
    if (mswin_debug >= 5)
	fprintf (mswin_debugfile, "mswin_setwindow:::  entered, minimized:  %d\n",
		gpTTYInfo->fMinimized);
#endif

    /* Require a font name to set font info. */
    if(fontName_utf8 != NULL && *fontName_utf8 != '\0' &&
	(fontName_lpt = utf8_to_lptstr(fontName_utf8)) &&
	_tcslen(fontName_lpt) <= LF_FACESIZE - 1){
	
	hDC = GetDC(ghTTYWnd);
	ppi = GetDeviceCaps(hDC, LOGPIXELSY);
	ReleaseDC(ghTTYWnd, hDC);

	/* Default height, then examin the requested fontSize. */
	newFont.lfHeight = -MulDiv(12, ppi, 72);
	if(ScanInt(fontSize_utf8, FONT_MIN_SIZE, FONT_MAX_SIZE, &newHeight)){
	    newHeight = abs(newHeight);
	    newFont.lfHeight = -MulDiv(newHeight, ppi, 72);
        }
	
	/* Default font style, then read requested style. */
	newFont.lfWeight =         0;
	newFont.lfItalic =         0;
	if(fontStyle_utf8 != NULL){
	    _strlwr(fontStyle_utf8);
	    if(strstr(fontStyle_utf8, "bold"))
		newFont.lfWeight = FW_BOLD;
	    if(strstr(fontStyle_utf8, "italic"))
		newFont.lfItalic = 1;
	}
	
	/* Everything else takes the default. */
	newFont.lfWidth =          0;
	newFont.lfEscapement =     0;
	newFont.lfOrientation =    0;
	newFont.lfUnderline =      0;
	newFont.lfStrikeOut =      0;
	fontCharSet_lpt = utf8_to_lptstr(fontCharSet_utf8);
	if(fontCharSet_lpt){
	    newFont.lfCharSet = mswin_string2charsetid(fontCharSet_lpt);
	    fs_give((void **) &fontCharSet_lpt);
	}

	newFont.lfOutPrecision =   OUT_DEFAULT_PRECIS;
	newFont.lfClipPrecision =  CLIP_DEFAULT_PRECIS;
	newFont.lfQuality =        DEFAULT_QUALITY;
	newFont.lfPitchAndFamily = FIXED_PITCH;
	_sntprintf(newFont.lfFaceName, LF_FACESIZE, TEXT("%s"), fontName_lpt);
	ResetTTYFont (ghTTYWnd, gpTTYInfo, &newFont);
    }

    if(fontName_lpt)
      fs_give((void **) &fontName_lpt);

    /*
     * Set window position.  String format is:  CxR+X+Y
     * Where C is the number of columns, R is the number of rows,
     * and X and Y specify the top left corner of the window.
     */
    if(windowPosition != NULL && *windowPosition != '\0'){
	if(strlen(windowPosition) > sizeof(wp)-1)
	  return(0);

	strncpy(wp, windowPosition, sizeof(wp));
	wp[sizeof(wp)-1] = '\0';
	
	/*
	 * Flag characters are at the end of the string.  Strip them
	 * off till we get to a number.
	 */
	i = (int)strlen(wp) - 1;
	while(i > 0 && (*(wp+i) < '0' || *(wp+i) > '9')){
	    if(*(wp+i) == 't' || *(wp+i) == 'b'){
		toolBar = TRUE;
		toolBarTop = (*(wp+i) == 't');
	    }

	    if(*(wp+i) == 'd')
	      gfUseDialogs = TRUE;

#ifdef ACCELERATORS_OPT
	    if(*(wp+i) == 'a')
	        AccelCtl(ghTTYWnd, ACCEL_LOAD, FALSE);
#endif	

	    *(wp+i) = 0;
	    --i;
        }

	if(strcmp(wp, "MIN0") == 0){
	    mswin_killsplash();
	    showWin = 0;
	    ShowWindow(ghTTYWnd, SW_MINIMIZE);
	    if(toolBar){
		gpTTYInfo->toolBarTop = toolBarTop;
		TBShow(ghTTYWnd);
	    }
	}
	else{
	    /* Look for Columns, deliminated by 'x'. */
	    c = strchr (wp, 'x');
	    if (c == NULL)
	      return(0);

	    *c = '\0';
	    if(!ScanInt(wp, -999, 9999, &wColumns))
	      return(0);

	    /* Look for Rows, deliminated by '+'. */
	    n = c + 1;
	    c = strchr(n, '+');
	    if(c == NULL)
	      return (0);

	    *c = '\0';
	    if(!ScanInt(n, -999, 9999, &wRows))
	      return(0);

	    /* Look for X position, deliminated by '+'. */
	    n = c + 1;
	    c = strchr(n, '+');
	    if(c == NULL)
	      return(0);

	    *c = '\0';
	    if(!ScanInt(n, -999, 9999, &wXPos))
	      return(0);

	    /* And get Y position, deliminated by end of string. */
	    n = c + 1;
	    if(!ScanInt(n, -999, 9999, &wYPos))
	      return(0);


	    /* Constrain the window position and size. */
	    if(wXPos < 0)
	      wXPos = 0;

	    if(wYPos < 0)
	      wYPos = 0;

	    GetWindowRect(GetDesktopWindow(), &wndRect);
	    if(wXPos > wndRect.right - 20)
	      wXPos = wndRect.right - 100;

	    if(wYPos > wndRect.bottom - 20)
	      wYPos = wndRect.bottom - 100;

	    /* Get the current window rect and client area. */
	    GetWindowRect(ghTTYWnd, &wndRect);
	    GetClientRect(ghTTYWnd, &cliRect);

	    /* Calculate boarder sizes. */
	    wXBorder = wndRect.right - wndRect.left - cliRect.right;
	    wYBorder = wndRect.bottom - wndRect.top - cliRect.bottom;

	    /* Show toolbar before calculating content size. */
	    if(toolBar){
		gpTTYInfo->toolBarTop = toolBarTop;
		TBShow(ghTTYWnd);
	    }

	    /* Calculate new window pos and size. */
	    wXSize = wXBorder + (wColumns * gpTTYInfo->xChar) +
						    (2 * gpTTYInfo->xOffset);
	    wYSize = wYBorder + (wRows * gpTTYInfo->yChar) +
				    gpTTYInfo->toolBarSize + (2 * MARGINE_TOP);
	    if(!gpTTYInfo->fMinimized)
	      MoveWindow(ghTTYWnd, wXPos, wYPos, wXSize, wYSize, TRUE);
	    else{
		gpTTYInfo->fDesiredSize = TRUE;
		gpTTYInfo->xDesPos = (CORD)wXPos;
		gpTTYInfo->yDesPos = (CORD)wYPos;
		gpTTYInfo->xDesSize = (CORD)wXSize;
		gpTTYInfo->yDesSize = (CORD)wYSize;
	    }
        }
    }

    if(caretStyle != NULL && *caretStyle != '\0')
      for(i = 0; MSWinCaretTable[i].name; i++)
	if(!strucmp(MSWinCaretTable[i].name, caretStyle)){
	    CaretTTY(ghTTYWnd, MSWinCaretTable[i].style);
	    break;
	}

    return(0);
}


int
mswin_showwindow()
{
    mswin_killsplash();
    ShowWindow (ghTTYWnd, SW_SHOWNORMAL);
    UpdateWindow (ghTTYWnd);

    return(0);
}


/*
 * Retreive the current font name, font size, and window position
 * These get stored in the pinerc file and will be passed to
 * mswin_setwindow() when pine starts up.  See pinerc for comments
 * on the format.
 */
int
mswin_getwindow(char *fontName_utf8, size_t nfontName,
		char *fontSize_utf8, size_t nfontSize,
		char *fontStyle_utf8, size_t nfontStyle,
		char *windowPosition, size_t nwindowPosition,
		char *foreColor, size_t nforeColor,
		char *backColor, size_t nbackColor,
		char *caretStyle, size_t ncaretStyle,
		char *fontCharSet_utf8, size_t nfontCharSet)
{
    HDC			hDC;
    int			ppi;
    RECT		wndRect;
    char	       *t;

    if(fontName_utf8 != NULL){
	t = lptstr_to_utf8(gpTTYInfo->lfTTYFont.lfFaceName);
	if(strlen(t) < nfontName)
	  snprintf(fontName_utf8, nfontName, "%s", t);

	fs_give((void **) &t);
    }

    if(fontCharSet_utf8 != NULL){
	LPTSTR lpt;

	lpt = (LPTSTR) fs_get(nfontCharSet * sizeof(TCHAR));
	if(lpt){
	    lpt[0] = '\0';
	    mswin_charsetid2string(lpt, nfontCharSet, gpTTYInfo->lfTTYFont.lfCharSet);
	    t = lptstr_to_utf8(lpt);
	    if(strlen(t) < nfontCharSet)
	      snprintf(fontCharSet_utf8, nfontCharSet, "%s", t);

	    fs_give((void **) &t);

	    fs_give((void **) &lpt);
	}
    }

    if(fontSize_utf8 != NULL){
	hDC = GetDC(ghTTYWnd);
	ppi = GetDeviceCaps(hDC, LOGPIXELSY);
	ReleaseDC(ghTTYWnd, hDC);
	snprintf(fontSize_utf8, nfontSize, "%d", MulDiv(-gpTTYInfo->lfTTYFont.lfHeight, 72, ppi));
    }

    if(fontStyle_utf8 != NULL){
	char		*sep[] = {"", ", "};
	int		iSep = 0;
	
	*fontStyle_utf8 = '\0';
	if(gpTTYInfo->lfTTYFont.lfWeight >= FW_BOLD){
	    strncpy(fontStyle_utf8, "bold", nfontStyle);
	    fontStyle_utf8[nfontStyle-1] = '\0';
	    iSep = 1;
        }

	if(gpTTYInfo->lfTTYFont.lfItalic){
	    strncat(fontStyle_utf8, sep[iSep], nfontStyle-strlen(fontStyle_utf8)-1);
	    fontStyle_utf8[nfontStyle-1] = '\0';
	    strncat(fontStyle_utf8, "italic", nfontStyle-strlen(fontStyle_utf8)-1);
	    fontStyle_utf8[nfontStyle-1] = '\0';
        }
    }

    if(windowPosition != NULL){
	if(gpTTYInfo->fMinimized){
	    strncpy(windowPosition, "MIN0", nwindowPosition);
	    windowPosition[nwindowPosition-1] = '\0';
	}
	else{
	    /*
	     * Get the window position.  Constrain the top left corner
	     * to be on the screen.
	     */
	    GetWindowRect(ghTTYWnd, &wndRect);
	    if(wndRect.left < 0)
	      wndRect.left = 0;

	    if(wndRect.top < 0)
	      wndRect.top = 0;

	    snprintf(windowPosition, nwindowPosition, "%dx%d+%d+%d", gpTTYInfo->actNColumn,
		    gpTTYInfo->actNRow, wndRect.left, wndRect.top);
        }

	if(gpTTYInfo->toolBarSize > 0){
	    strncat(windowPosition, gpTTYInfo->toolBarTop ? "t" : "b",
		    nwindowPosition-strlen(windowPosition)-1);
	    windowPosition[nwindowPosition-1] = '\0';
	}

	if(gfUseDialogs){
	    strncat(windowPosition, "d", nwindowPosition-strlen(windowPosition)-1);
	    windowPosition[nwindowPosition-1] = '\0';
	}

	if(gpTTYInfo->hAccel){
	    strncat(windowPosition, "a", nwindowPosition-strlen(windowPosition)-1);
	    windowPosition[nwindowPosition-1] = '\0';
	}

	if(gpTTYInfo->fMaximized){
	    strncat(windowPosition, "!", nwindowPosition-strlen(windowPosition)-1);
	    windowPosition[nwindowPosition-1] = '\0';
	}
    }

    if(foreColor != NULL)
      ConvertStringRGB(foreColor, nforeColor, gpTTYInfo->rgbFGColor);

    if(backColor != NULL)
      ConvertStringRGB(backColor, nbackColor, gpTTYInfo->rgbBGColor);

    if(caretStyle != NULL){
	int i;

	for(i = 0; MSWinCaretTable[i].name; i++)
	  if(MSWinCaretTable[i].style == gpTTYInfo->cCaretStyle){
	      strncpy(caretStyle, MSWinCaretTable[i].name, ncaretStyle);
	      caretStyle[ncaretStyle-1] = '\0';
	      break;
	  }
    }

    return (0);
}


void
mswin_noscrollupdate(int flag)
{
    gpTTYInfo->noScrollUpdate = (flag != 0);
    if(flag == 0 && gpTTYInfo->scrollRangeChanged){
	mswin_setscrollrange(gpTTYInfo->noSUpdatePage, gpTTYInfo->noSUpdateRange);
	gpTTYInfo->noSUpdatePage = gpTTYInfo->noSUpdateRange = 0;
	gpTTYInfo->scrollRangeChanged = 0;
    }
}


/*
 * Set the scroll range.
 */
void
mswin_setscrollrange (long page, long max)
{
    SCROLLINFO scrollInfo;

    if(gpTTYInfo->noScrollUpdate){
	gpTTYInfo->noSUpdatePage = page;
	gpTTYInfo->noSUpdateRange = max;
	gpTTYInfo->scrollRangeChanged = 1;
	return;
    }
    if (max != gpTTYInfo->scrollRange) {
	scrollInfo.cbSize = sizeof(SCROLLINFO);
	scrollInfo.fMask = SIF_DISABLENOSCROLL | SIF_PAGE | SIF_RANGE;
	scrollInfo.nMin = 0;

	if (max > 0) {
	    scrollInfo.nMax = (int) max;
	    scrollInfo.nPage = page;
	    SetScrollInfo(ghTTYWnd, SB_VERT, &scrollInfo, TRUE);
	    EnableScrollBar (ghTTYWnd, SB_VERT, ESB_ENABLE_BOTH);
	}
	else {
	    max = 0;
	    scrollInfo.cbSize = sizeof(SCROLLINFO);
	    scrollInfo.fMask |= SIF_POS;
	    scrollInfo.nMax = 1;
	    scrollInfo.nPos = 0;
	    SetScrollInfo(ghTTYWnd, SB_VERT, &scrollInfo, TRUE);
	    EnableScrollBar (ghTTYWnd, SB_VERT, ESB_DISABLE_BOTH);
	    gpTTYInfo->scrollPos = 0;
	}
	gpTTYInfo->scrollRange = (int)max;
    }
}


/*
 * Set the current scroll position.
 */
void
mswin_setscrollpos (long pos)
{
    SCROLLINFO scrollInfo;

    if (pos != gpTTYInfo->scrollPos) {
	scrollInfo.cbSize = sizeof(SCROLLINFO);
	scrollInfo.fMask = SIF_PAGE | SIF_RANGE;

	GetScrollInfo(ghTTYWnd, SB_VERT, &scrollInfo);

	scrollInfo.fMask |= SIF_POS;
	scrollInfo.nPos = (int) pos;
	SetScrollInfo(ghTTYWnd, SB_VERT, &scrollInfo, TRUE);

	gpTTYInfo->scrollPos = pos;
    }
}


/*
 * retreive the current scroll postion.
 */
long
mswin_getscrollpos (void)
{
   return ((long)((float)GetScrollPos (ghTTYWnd, SB_VERT)));
}


/*
 * retreive the latest scroll to position.
 */
long
mswin_getscrollto (void)
{
   return (gpTTYInfo->scrollTo);
}

/*
 * install function to deal with LINEDOWN events
 */
void
mswin_setscrollcallback (cbarg_t cbfunc)
{
    gScrollCallback = cbfunc;
}


/*
 * install function to deal with sort menu messages
 */
void
mswin_setsortcallback (cbarg_t cbfunc)
{
    gSortCallback = cbfunc;
}


/*
 * install function to deal with sort menu messages
 */
void
mswin_setflagcallback (cbarg_t cbfunc)
{
    gFlagCallback = cbfunc;
}


void
mswin_set_erasecreds_callback (cbvoid_t cbfunc)
{
    gEraseCredsCallback = cbfunc;
}

/*
 * install function to deal with header mode flipping
 */
void
mswin_sethdrmodecallback (cbarg_t cbfunc)
{
    gHdrCallback = cbfunc;
}


/*
 * install function to deal with view in new window messages
 */
void
mswin_setviewinwindcallback (cbvoid_t cbfunc)
{
    gViewInWindCallback = cbfunc;
}


/*
 * install function to deal with zoom mode flipping
 */
void
mswin_setzoomodecallback (cbarg_t cbfunc)
{
    gZoomCallback = cbfunc;
}


/*
 * install function to deal with function key mode flipping
 */
void
mswin_setfkeymodecallback (cbarg_t cbfunc)
{
    gFkeyCallback = cbfunc;
}


/*
 * install function to deal with apply mode flipping
 */
void
mswin_setselectedcallback (cbarg_t cbfunc)
{
    gSelectedCallback = cbfunc;
}


/*
 * return 1 if new mail window is being used
 */
int
mswin_newmailwinon (void)
{
    return(gMswinNewMailWin.hwnd ? 1 : 0);
}


/*
 */
void
mswin_setdebugoncallback (cbvoid_t cbfunc)
{
    gIMAPDebugONCallback = cbfunc;
}


void
mswin_setdebugoffcallback (cbvoid_t cbfunc)
{
    gIMAPDebugOFFCallback = cbfunc;
}


int
mswin_setconfigcallback (cbvoid_t cffunc)
{
    gConfigScreenCallback = cffunc;
    return(1);
}


/*
 * Set the printer font
 */
void		
mswin_setprintfont(char *fontName, char *fontSize,
		   char *fontStyle, char *fontCharSet)
{
    LPTSTR fn = NULL, fstyle = NULL, fc = NULL;

    if(fontName)
      fn = utf8_to_lptstr(fontName);

    if(fontStyle)
      fstyle = utf8_to_lptstr(fontStyle);

    if(fontCharSet)
      fc = utf8_to_lptstr(fontCharSet);

    /* Require a font name to set font info. */
    if(fn != NULL && *fn != '\0' && lstrlen(fn) <= LF_FACESIZE - 1){
	
	_tcsncpy(gPrintFontName, fn, sizeof(gPrintFontName)/sizeof(TCHAR));
	gPrintFontName[sizeof(gPrintFontName)/sizeof(TCHAR)-1] = 0;
	if(fstyle){
	    _tcsncpy(gPrintFontStyle, fstyle, sizeof(gPrintFontStyle)/sizeof(TCHAR));
	    gPrintFontStyle[sizeof(gPrintFontStyle)/sizeof(TCHAR)-1] = 0;
	    _tcslwr(gPrintFontStyle);	/* Lower case font style. */
	}

	if(fc){
	    _tcsncpy(gPrintFontCharSet, fc, sizeof(gPrintFontCharSet)/sizeof(TCHAR));
	    gPrintFontCharSet[sizeof(gPrintFontCharSet)/sizeof(TCHAR)-1] = 0;
	}

	gPrintFontSize = 12;
	if(ScanInt(fontSize, FONT_MIN_SIZE, FONT_MAX_SIZE, &gPrintFontSize))
	  gPrintFontSize = abs(gPrintFontSize);

	gPrintFontSameAs = FALSE;
    }
    else{
	gPrintFontName[0] = '\0';
	gPrintFontSameAs = TRUE;
    }

    if(fn)
      fs_give((void **) &fn);

    if(fstyle)
      fs_give((void **) &fstyle);

    if(fc)
      fs_give((void **) &fc);
}


void
mswin_getprintfont(char *fontName_utf8, size_t nfontName,
		   char *fontSize_utf8, size_t nfontSize,
		   char *fontStyle_utf8, size_t nfontStyle,
		   char *fontCharSet_utf8, size_t nfontCharSet)
{
    if(gPrintFontName[0] == '\0' || gPrintFontSameAs){
	if(fontName_utf8 != NULL)
	    *fontName_utf8 = '\0';
	if(fontSize_utf8 != NULL)
	    *fontSize_utf8 = '\0';
	if(fontStyle_utf8 != NULL)
	    *fontStyle_utf8 = '\0';
	if(fontCharSet_utf8 != NULL)
	    *fontCharSet_utf8 = '\0';
    }
    else{
	char *u;

	if(fontName_utf8 != NULL){
	    u = lptstr_to_utf8(gPrintFontName);
	    if(u){
		strncpy(fontName_utf8, u, nfontName);
		fontName_utf8[nfontName-1] = 0;
		fs_give((void **) &u);
	    }
	}

	if(fontSize_utf8 != NULL)
	  snprintf(fontSize_utf8, nfontSize, "%d", gPrintFontSize);


	if(fontStyle_utf8 != NULL){
	    u = lptstr_to_utf8(gPrintFontStyle);
	    if(u){
		strncpy(fontStyle_utf8, u, nfontStyle);
		fontStyle_utf8[nfontStyle-1] = 0;
		fs_give((void **) &u);
	    }
	}

	if(fontCharSet_utf8 != NULL){
	    u = lptstr_to_utf8(gPrintFontCharSet);
	    if(u){
		strncpy(fontCharSet_utf8, u, nfontCharSet);
		fontCharSet_utf8[nfontCharSet-1] = 0;
		fs_give((void **) &u);
	    }
	}
    }
}


/*
 * Set the window help text.  Add or delete the Help menu item as needed.
 */
int
mswin_sethelptextcallback(cbstr_t cbfunc)
{
    HMENU		hMenu;

    gHelpCallback = cbfunc;

    hMenu = GetMenu (ghTTYWnd);
    if (hMenu == NULL)
	return (1);

    return(MSWHelpSetMenu (hMenu));

}


/*
 * Set the window help text.  Add or delete the Help menu item as needed.
 */
int
mswin_setgenhelptextcallback(cbstr_t cbfunc)
{
    HMENU		hMenu;

    gHelpGenCallback = cbfunc;

    hMenu = GetMenu (ghTTYWnd);
    if (hMenu == NULL)
	return (1);

    return(MSWHelpSetMenu (hMenu));

}


int
MSWHelpSetMenu(HMENU hMenu)
{
    BOOL		brc;
    int			count;

    /*
     * Find menu and update it.
     */
    count = GetMenuItemCount (hMenu);
    if (count == -1)
	return (1);

    hMenu = GetSubMenu (hMenu, count - 1);
    if (hMenu == NULL)
	return (1);

    /*
     * Insert or delete generic help item
     */
    if (gHelpGenCallback == NULL){
	if (gfHelpGenMenu) {
	    brc = DeleteMenu (hMenu, IDM_MI_GENERALHELP, MF_BYCOMMAND);
	    DrawMenuBar (ghTTYWnd);
	}
	gfHelpGenMenu = FALSE;
    }
    else {
	if (!gfHelpGenMenu) {
	    brc = InsertMenu (hMenu, 0,
			      MF_BYPOSITION | MF_STRING,
			      IDM_MI_GENERALHELP, TEXT("&General Help"));
	    DrawMenuBar (ghTTYWnd);
	}
	gfHelpGenMenu = TRUE;
    }

    /*
     * Insert or delete screen help item
     */
    if (gHelpCallback == NULL){
	if (gfHelpMenu) {
	    brc = DeleteMenu (hMenu, IDM_HELP, MF_BYCOMMAND);
	    DrawMenuBar (ghTTYWnd);
	}
	gfHelpMenu = FALSE;
    }
    else {
	if (!gfHelpMenu) {
	    brc = InsertMenu (hMenu, gHelpGenCallback ? 2 : 1,
			      MF_BYPOSITION | MF_STRING,
			      IDM_HELP, TEXT("Screen Help in &New Window"));
	    DrawMenuBar (ghTTYWnd);
	}
	gfHelpMenu = TRUE;
    }

    return (0);
}


/*
 * Set the text displayed when someone tries to close the application
 * the wrong way.
 */
void
mswin_setclosetext (char *pCloseText)
{
    gpCloseText = pCloseText;
}


/*
 * Called when upper layer is in a busy loop.  Allows us to yeild to
 * Windows and perhaps process some events.  Does not yeild control
 * to other applications.
 */
int
mswin_yield (void)
{
    MSG		msg;
    DWORD	start;
    int		didmsg = FALSE;

    if (gScrolling)
	return (TRUE);

    start = GetTickCount ();
#ifdef CDEBUG
    if (mswin_debug > 16)
	fprintf (mswin_debugfile, "mswin_yeild:: Entered\n");
#endif
    if (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE)) {
	if (gpTTYInfo->hAccel == NULL ||
		!TranslateAccelerator (ghTTYWnd, gpTTYInfo->hAccel, &msg)) {
	    TranslateMessage (&msg);
	    DispatchMessage (&msg);
	    didmsg = TRUE;
	}
    }
#ifdef CDEBUG
    if (mswin_debug > 16)
	fprintf (mswin_debugfile, "mswin_yeild::  Delay %ld msec\n",
		GetTickCount () - start);
#endif
    return (didmsg);
}


/*
 *	Called to see if we can process input.
 *	We can't process input when we are in a scrolling mode.
 */
int
mswin_caninput (void)
{
    return (!gScrolling && !gMouseTracking);
}


/*
 *	Called to see if there is a character available.
 */
int
mswin_charavail (void)
{
    MSG		msg;
    BOOL	ca, pa, ma;

    if (gScrolling)
	return (FALSE);

    RestoreMouseCursor();

    /*
     * If there are no windows messages waiting for this app, GetMessage
     * can take a long time.  So before calling GetMessage check if
     * there is anything I should be doing.  If there is, then only
     * call PeekMessage.
     * BUT!  Don't let too much time elapse between calls to GetMessage
     * or we'll shut out other windows.
     */
    ca = CQAvailable ();
    pa = EditPasteAvailable ();
#ifdef CDEBUG
    start = GetTickCount ();
    if (mswin_debug > 16)
	fprintf (mswin_debugfile, "%s mswin_charavail::  Entered, ca %d, pa %d\n",dtime(),ca,pa);
#endif
    if ((ca || pa) && GetTickCount () < gGMLastCall + GM_MAX_TIME)
        ma = PeekMessage (&msg, NULL, 0, 0, PM_NOYIELD | PM_REMOVE);
    else {
        ma = GetMessage (&msg, NULL, 0, 0);
	gGMLastCall = GetTickCount ();
    }
    if (ma) {
	if (gpTTYInfo->hAccel == NULL ||
		!TranslateAccelerator (ghTTYWnd, gpTTYInfo->hAccel, &msg)) {
	    TranslateMessage (&msg);
	    DispatchMessage (&msg);
	}
    }
#ifdef CDEBUG
    if (mswin_debug > 16)
	fprintf (mswin_debugfile, "%s mswin_charavail::  Delay %ld msec\n",
		dtime(), GetTickCount () - start);
#endif

    return (pa || ca || CQAvailable ());
}


/*
 *	Call to get next character.  Dispatch one message.
 */
UCS
mswin_getc (void)
{
    BOOL	ca, pa, ma;
    MSG		msg;

    if (gScrolling)
	return (NODATA);

    RestoreMouseCursor();

    mswin_flush();


    /*
     * If there are no windows messages waiting for this app, GetMessage
     * can take a long time.  So before calling GetMessage check if
     * there is anything I should be doing.  If there is, then only
     * call PeekMessage.
     */
    ca = CQAvailable ();
    pa = EditPasteAvailable ();
#ifdef CDEBUG
    if (mswin_debug > 16) {
	start = GetTickCount ();
	fprintf (mswin_debugfile, "mswin_getc::  Entered, ca %d pa %d\n", ca, pa);
    }
#endif
    if ((ca || pa) && GetTickCount () < gGMLastCall + GM_MAX_TIME)
        ma = PeekMessage (&msg, NULL, 0, 0, PM_NOYIELD | PM_REMOVE);
    else {
        ma = GetMessage (&msg, NULL, 0, 0);
	gGMLastCall = GetTickCount ();
    }
    if (ma) {
	if (gpTTYInfo->hAccel == NULL ||
		!TranslateAccelerator (ghTTYWnd, gpTTYInfo->hAccel, &msg)) {
	    TranslateMessage (&msg);
	    DispatchMessage (&msg);
	}
    }
#ifdef CDEBUG
    if (mswin_debug > 16)
	fprintf (mswin_debugfile, "mswin_getc::  Delay %ld msec\n",
		GetTickCount () - start);
#endif


    if (pa) {
	SelClear ();
	return (EditPasteGet ());
    }
    if (ca || CQAvailable ()) {
	SelClear();
	return ((UCS)(CQGet ()));
    }

    return (NODATA);
}


/*
 *	Like mswin_getc, but don't yield control. Returns a CTRL key values
 *   where, for example, ctrl+c --> CTRL|'C'.
 */
LOCAL UCS
mswin_getc_fast (void)
{
    RestoreMouseCursor();

    if (EditPasteAvailable ()) {
	SelClear ();
	return (EditPasteGet ());
    }
    if (CQAvailable ()) {
	SelClear ();
	return ((UCS)CQGet ());
    }

    return (NODATA);
}


/*
 * Flush the character input queue.
 */
void
mswin_flush_input(void)
{
    /*
     * GetQueueStatus tells us if there are any input messages in the message
     * queue. If there are, we call mswin_getc which will retrieve the
     * next message (which may or may not be an input message) and process
     * it. We do that until all the input messages are gone.
     */
    CQInit();

    while(GetQueueStatus(QS_INPUT))
      (void) mswin_getc();

    /* And we clear Pine's character input queue, too. */
    CQInit();
}


/*
 * ibmmove - Move cursor to...
 */
int
mswin_move (int row, int column)

{
    FlushWriteAccum ();
    if (row < 0 || row >= gpTTYInfo->actNRow)
	return (-1);
    if (column < 0 || column >= gpTTYInfo->actNColumn)
	return (-1);
    gpTTYInfo->nRow = (CORD)row;
    gpTTYInfo->nColumn = (CORD)column;
    MoveTTYCursor (ghTTYWnd);
    return (0);
}


int
mswin_getpos (int *row, int *column)
{
    FlushWriteAccum ();
    if (row == NULL || column == NULL)
	return (-1);
    *row = gpTTYInfo->nRow;
    *column = gpTTYInfo->nColumn;
    return (0);
}


int
mswin_getscreensize (int *row, int *column)
{
    if (row == NULL || column == NULL)
	return (-1);
    *row = gpTTYInfo->actNRow;
    *column = gpTTYInfo->actNColumn;
#ifdef SDEBUG
    if (mswin_debug >= 5)
	fprintf (mswin_debugfile, "mswin_getscreensize::: reported size %d, %d\n",
		*row, *column);
#endif

    *column = MIN(*column, NLINE-1);

    return (0);
}


void
mswin_minimize()
{
    if (!gpTTYInfo->fMinimized)
      ShowWindow(ghTTYWnd, SW_MINIMIZE);
}
	

int
mswin_showcursor (int show)
{
    int was_shown = gpTTYInfo->fCursorOn;

    if (show) {
	/* if the cursor was not already show, show it now. */
	if (!was_shown) {
	    gpTTYInfo->fCursorOn = TRUE;
	    ShowCursor(TRUE);
        }
    }
    else {
	/* If the cursor is shown, hide it. */
	if (was_shown){
	    gpTTYInfo->fCursorOn = FALSE;
	    ShowCursor(FALSE);
	}
    }

    return (was_shown);
}


int
mswin_showcaret (int show)
{
    int was_shown = gpTTYInfo->fCaretOn;

    if (show) {
	/* if the caret was not already show, show it now. */
	if (!was_shown) {
	    gpTTYInfo->fCaretOn = TRUE;
	    ShowCaret(ghTTYWnd);
        }
    }
    else {
	/* If the caret is shown, hide it. */
	if (was_shown){
	    gpTTYInfo->fCaretOn = FALSE;
	    HideCaret(ghTTYWnd);
	}
    }

    return (was_shown);
}


int
mswin_puts (char *utf8_str)
{
    int			strLen;
    LPTSTR              lptstr_str;

    FlushWriteAccum ();
    if (utf8_str == NULL)
	return (-1);
    if(!(lptstr_str = utf8_to_lptstr(utf8_str)))
      return(-1);
    strLen = (int)_tcslen (lptstr_str);
    if (strLen > 0)
	WriteTTYText (ghTTYWnd, lptstr_str, strLen);

    fs_give((void **) &lptstr_str);
    return (0);
}


int
mswin_puts_n (char *utf8_str, int n)
{
    LPTSTR lptstr_str, lptstr_p;

    FlushWriteAccum ();
    if (utf8_str == NULL)
	return (-1);
    lptstr_str = utf8_to_lptstr(utf8_str);
    if(n < _tcslen(lptstr_str))
      lptstr_str[n] = '\0';
    for (lptstr_p = lptstr_str; n > 0 && *lptstr_p; n--, lptstr_p++)
	;
    if (lptstr_p > lptstr_str)
	WriteTTYText (ghTTYWnd, lptstr_str, (int)(lptstr_p - lptstr_str));

    fs_give((void **) &lptstr_str);
    return (0);
}


int
mswin_putblock (char *utf8_str, int strLen)
{
    LPTSTR lptstr_str;

    FlushWriteAccum ();
    if (utf8_str == NULL)
	return (-1);
    lptstr_str = utf8_to_lptstr(utf8_str);
    if (strLen > 0)
	WriteTTYText (ghTTYWnd, lptstr_str, strLen);
    fs_give((void **) &lptstr_str);
    return (0);
}


/*
 * mswin_putc - put a character at the current position in the
 *	     current colors
 */
int
mswin_putc (UCS ucs)
{
    TCHAR cc = (TCHAR)ucs;
    if (ucs >= (UCS)(' ')) {
	/* Not carriage control. */
	gpTTYInfo->writeAccum[gpTTYInfo->writeAccumCount++] = (TCHAR)ucs;
	if (gpTTYInfo->writeAccumCount == WRITE_ACCUM_SIZE)
		FlushWriteAccum ();
    }
    else {
	/* Carriage control.  Need to flush write accumulator and
	 * write this char. */
	FlushWriteAccum ();
	WriteTTYBlock (ghTTYWnd, &cc, 1);
    }
    return (0);
}


/*
 * ibmoutc - output a single character with the right attributes, but
 *           don't advance the cursor
 */
int
mswin_outc (char c)
{
    RECT	rect;
    long	offset;

    FlushWriteAccum ();

    switch (c) {
    case ASCII_BEL:
	MessageBeep (0);
	break;
	
    case ASCII_BS:
    case ASCII_CR:
    case ASCII_LF:
	/* Do nothing for these screen motion characters. */
	break;
	
	
    default:
	/* Paint character to screen. */
	offset = (gpTTYInfo->nRow * gpTTYInfo->actNColumn) + gpTTYInfo->nColumn;
	gpTTYInfo->pScreen[offset] = c;
	gpTTYInfo->pCellWidth[offset] = wcellwidth((UCS)c) * gpTTYInfo->xChar;
	gpTTYInfo->pAttrib[offset] = gpTTYInfo->curAttrib;
	
	/* Invalidate rectange covering singel character. */
	rect.left = (gpTTYInfo->nColumn * gpTTYInfo->xChar) +
		gpTTYInfo->xOffset;
	rect.right = rect.left + gpTTYInfo->xChar;
	rect.top = (gpTTYInfo->nRow * gpTTYInfo->yChar) +
		gpTTYInfo->yOffset;
	rect.bottom = rect.top + gpTTYInfo->yChar;
	gpTTYInfo->screenDirty = TRUE;
	InvalidateRect (ghTTYWnd, &rect, FALSE);
	break;
    }
    return (0);
}

/*
 * ibmrev - change reverse video state
 */
int
mswin_rev (int state)
{
    int curState = (gpTTYInfo->curAttrib.style & CHAR_ATTR_REV) != 0;

    if (state != curState){
	FlushWriteAccum ();
	if (state) {
	    gpTTYInfo->curAttrib.style |= CHAR_ATTR_REV;
	    SetReverseColor();
	}
	else{
	    gpTTYInfo->curAttrib.style &= ~CHAR_ATTR_REV;
	    pico_set_normal_color();
	}
    }

    return (0);
}
	

/*
 * Get current reverse video state.
 */
int
mswin_getrevstate (void)
{
    return ((gpTTYInfo->curAttrib.style & CHAR_ATTR_REV) != 0);
}


/*
 * ibmrev - change reverse video state
 */
int
mswin_bold (int state)
{
    int curState = (gpTTYInfo->curAttrib.style & CHAR_ATTR_BOLD) != 0;

    if (state != curState){
	FlushWriteAccum ();
	if (state)
	  gpTTYInfo->curAttrib.style |= CHAR_ATTR_BOLD;
	else
	  gpTTYInfo->curAttrib.style &= ~CHAR_ATTR_BOLD;
    }

    return (0);
}
	

/*
 * ibmrev - change reverse video state
 */
int
mswin_uline (int state)
{
    int curState = (gpTTYInfo->curAttrib.style & CHAR_ATTR_ULINE) != 0;

    if (state != curState){
	FlushWriteAccum ();
	if (state)
	  gpTTYInfo->curAttrib.style |= CHAR_ATTR_ULINE;
	else
	  gpTTYInfo->curAttrib.style &= ~CHAR_ATTR_ULINE;
    }

    return (0);
}


/*
 * ibmeeol - erase to the end of the line
 */
int
mswin_eeol (void)
{
    TCHAR	*cStart;
    CharAttrib	*aStart;
    int         *cwStart;
    long	length, i;
    RECT	rect;

    FlushWriteAccum ();

    /* From current position to end of line. */
    length = gpTTYInfo->actNColumn - gpTTYInfo->nColumn;		

    cStart = gpTTYInfo->pScreen + (gpTTYInfo->nRow * gpTTYInfo->actNColumn)
			+ gpTTYInfo->nColumn;
    cwStart = gpTTYInfo->pCellWidth + (gpTTYInfo->nRow * gpTTYInfo->actNColumn)
			+ gpTTYInfo->nColumn;
    for(i = 0; i < length; i++){
	cStart[i] = (TCHAR)(' ');
	cwStart[i] = gpTTYInfo->xChar;
    }

    aStart = gpTTYInfo->pAttrib
		      + (gpTTYInfo->nRow * gpTTYInfo->actNColumn)
		      + gpTTYInfo->nColumn;
    for(; length > 0; length--, aStart++){
	aStart->style = CHAR_ATTR_NORM;
	aStart->rgbFG = gpTTYInfo->curAttrib.rgbFG;
	aStart->rgbBG = gpTTYInfo->curAttrib.rgbBG;
    }

    rect.left = (gpTTYInfo->nColumn * gpTTYInfo->xChar) +
	    gpTTYInfo->xOffset;
    rect.right = gpTTYInfo->xSize;
    rect.top = (gpTTYInfo->nRow * gpTTYInfo->yChar) +
	    gpTTYInfo->yOffset;
    rect.bottom = rect.top + gpTTYInfo->yChar;
    gpTTYInfo->screenDirty = TRUE;
    InvalidateRect (ghTTYWnd, &rect, FALSE);

    return (0);
}


/*
 * ibmeeop - clear from cursor to end of page
 */
int
mswin_eeop (void)
{
    TCHAR	*cStart;
    CharAttrib	*aStart;
    int         *cwStart;
    long	length, i;
    RECT	rect;

    FlushWriteAccum ();
    /* From current position to end of screen. */

    cStart = gpTTYInfo->pScreen + (gpTTYInfo->nRow * gpTTYInfo->actNColumn)
			+ gpTTYInfo->nColumn;
    cwStart = gpTTYInfo->pCellWidth + (gpTTYInfo->nRow * gpTTYInfo->actNColumn)
			+ gpTTYInfo->nColumn;
    length = (long)((gpTTYInfo->pScreen
			      + (gpTTYInfo->actNColumn * gpTTYInfo->actNRow))
			      - cStart);
    for(i = 0; i < length; i ++){
	cStart[i] = (TCHAR)(' ');
	cwStart[i] = gpTTYInfo->xChar;
    }

    aStart = gpTTYInfo->pAttrib
		      + (gpTTYInfo->nRow * gpTTYInfo->actNColumn)
		      + gpTTYInfo->nColumn;

    for(; length-- > 0; aStart++){
	aStart->style = CHAR_ATTR_NORM;
	aStart->rgbFG = gpTTYInfo->curAttrib.rgbFG;
	aStart->rgbBG = gpTTYInfo->curAttrib.rgbBG;
    }

    /* Invalidate a rectangle that includes all of the current line down
     * to the bottom of the window. */
    rect.left = 0;
    rect.right = gpTTYInfo->xSize;
    rect.top = (gpTTYInfo->nRow * gpTTYInfo->yChar) +
	    gpTTYInfo->yOffset;
    rect.bottom = gpTTYInfo->ySize;
    gpTTYInfo->screenDirty = TRUE;
    InvalidateRect (ghTTYWnd, &rect, FALSE);

    return (0);
}


/*
 * ibmbeep - system beep...
 */
int
mswin_beep (void)
{
    MessageBeep (MB_OK);
    return (0);
}


/*
 * pause - wait in function for specified number of seconds.
 */
void
mswin_pause (int seconds)
{
    DWORD	stoptime;

    stoptime = GetTickCount () + (DWORD) seconds * 1000;
    while (stoptime > GetTickCount ())
	mswin_yield ();
}
	

/*
 * ibmflush - Flush output to screen.
 *
 */
int
mswin_flush (void)
{

    /*
     * Flush cached changes, then update the window.
     */
    FlushWriteAccum ();
    UpdateWindow (ghTTYWnd);

    return (0);
}


/*
 * A replacement for fflush
 * relies on #define fflush mswin_fflush
 */
#undef fflush
int
mswin_fflush (FILE *f)
{
    if (f == stdout) {
	mswin_flush();
    }
    else
	fflush (f);

    return(0);
}


/*
 * Set the cursor
 */
void
mswin_setcursor (int newcursor)
{
    HCURSOR hNewCursor;

    switch(newcursor){
      case MSWIN_CURSOR_BUSY :
        hNewCursor = ghCursorBusy;
	mswin_showcursor(TRUE);
	break;

      case MSWIN_CURSOR_IBEAM :
        hNewCursor = ghCursorIBeam;
	break;

      case MSWIN_CURSOR_HAND :
        if(hNewCursor = ghCursorHand)
	  break;
	/* ELSE fall thru to normal cursor */

      case MSWIN_CURSOR_ARROW :
      default :
	hNewCursor = ghCursorArrow;
	break;
    }

    /* If new cursor requested, select it. */
    if (ghCursorCurrent != hNewCursor)
      SetCursor (ghCursorCurrent = hNewCursor);
}


void
RestoreMouseCursor()
{
    if(ghCursorCurrent == ghCursorBusy)
      mswin_setcursor(MSWIN_CURSOR_ARROW);
}


/*
 * Display message in windows dialog box.
 */
void
mswin_messagebox (char *msg_utf8, int err)
{
    LPTSTR msg_lptstr;

    mswin_killsplash();

    msg_lptstr = utf8_to_lptstr(msg_utf8);
    MessageBox (NULL, msg_lptstr, gszAppName,
		MB_OK | ((err) ?  MB_ICONSTOP : MB_ICONINFORMATION));
    fs_give((void **) &msg_lptstr);
}


/*
 * Signals whether or not Paste should be turned on in the
 * menu bar.
 */
void
mswin_allowpaste (int on)
{
    static short	 stackp = 0;
    static unsigned long stack = 0L;

    switch(on){
      case MSWIN_PASTE_DISABLE :
	if(stackp){		/* previous state? */
	    if((stackp -= 2) < 0)
	      stackp = 0;

	    gPasteEnabled = ((stack >> stackp) & 0x03);
	}
	else
	  gPasteEnabled = MSWIN_PASTE_DISABLE;

	break;

      case MSWIN_PASTE_FULL :
      case MSWIN_PASTE_LINE :
	if(gPasteEnabled){	/* current state? */
	    stack |= ((unsigned long) gPasteEnabled << stackp);
	    stackp += 2;
	}

	gPasteEnabled = on;
	break;
    }

#ifdef	ACCELERATORS
    UpdateAccelerators(ghTTYWnd);
#endif
}


/*
 * Signals whether or not Copy/Cut should be turned on in the
 * menu bar.
 */
void
mswin_allowcopy (getc_t copyfunc)
{
    gCopyCutFunction = copyfunc;
    gAllowCopy = (copyfunc != NULL);
#ifdef	ACCELERATORS
    UpdateAccelerators(ghTTYWnd);
#endif
}


/*
 * Signals whether or not Copy/Cut should be turned on in the
 * menu bar.
 */
void
mswin_allowcopycut (getc_t copyfunc)
{
    gCopyCutFunction = copyfunc;
    gAllowCopy = gAllowCut = (copyfunc != NULL);
#ifdef	ACCELERATORS
    UpdateAccelerators(ghTTYWnd);
#endif
}


/*
 * Replace the clipboard's contents with the given string
 */
void
mswin_addclipboard(char *s)
{
    HANDLE  hCB;
    char   *pCB;
    size_t  sSize;

    if(s && (sSize = strlen(s))){
	if (OpenClipboard (ghTTYWnd)) {		/* ...and we get the CB. */
	    if (EmptyClipboard ()
		&& (hCB = GlobalAlloc (GMEM_MOVEABLE, sSize+2))){
		pCB = GlobalLock (hCB);
		memcpy (pCB, s, sSize);
		pCB[sSize] = '\0';		/* tie it off  */

		GlobalUnlock (hCB);

		if (SetClipboardData (CF_TEXT, hCB) == NULL)
		  /* Failed!  Free the data. */
		  GlobalFree (hCB);
	    }

	    CloseClipboard ();
	}
    }
}


/*
 * Signals if the upper layer wants to track the mouse.
 */
void
mswin_allowmousetrack (int b)
{
    gAllowMouseTrack = b;
    if (b)
	SelClear ();
    MyTimerSet ();
}


/*
 * register's callback to warn
 */
void
mswin_mousetrackcallback(cbarg_t cbfunc)
{
    if(!(gMouseTrackCallback = cbfunc))
      mswin_setcursor (MSWIN_CURSOR_ARROW);
}


/*
 * Add text to the new mail icon.
 * Nothing done in win 3.1 (consider changing the program name?)
 * For win95 we add some tool tip text to the tray icon.
 */
void
mswin_newmailtext (char *t_utf8)
{
    LPTSTR t_lptstr;

    /*
     * If we're given text, then blip the icon to let the user know.
     * (NOTE: the new shell also gets an updated tooltip.)
     * Otherwise, we're being asked to resume our normal state...
     */
    if(t_utf8){
	/*
	 * Change the appearance of minimized icon so user knows there's new
	 * mail waiting for them.  On win 3.1 systems we redraw the icon.
	 * on win95 systems we update the icon in the task bar,
	 * and possibly udpate the small icon in the taskbar tool tray.
	 */
	t_lptstr = utf8_to_lptstr(t_utf8);
	UpdateTrayIcon(NIM_MODIFY, ghNewMailIcon, t_lptstr);
	fs_give((void **) &t_lptstr);
	PostMessage(ghTTYWnd,WM_SETICON,ICON_BIG,(LPARAM) ghNewMailIcon);
	PostMessage(ghTTYWnd,WM_SETICON,ICON_SMALL,(LPARAM) ghNewMailIcon);

	gpTTYInfo->fNewMailIcon = TRUE;
    }
    else if(gpTTYInfo->fNewMailIcon) {
	UpdateTrayIcon(NIM_MODIFY, ghNormalIcon, TEXT("Alpine"));
	PostMessage(ghTTYWnd,WM_SETICON,ICON_BIG,(LPARAM) ghNormalIcon);
	PostMessage(ghTTYWnd,WM_SETICON,ICON_SMALL,(LPARAM) ghNormalIcon);

	gpTTYInfo->fNewMailIcon = FALSE;
    }
}


void
mswin_mclosedtext (char *t_utf8)
{
    LPTSTR t_lptstr;

    if(t_utf8 && gpTTYInfo->fMClosedIcon == FALSE){
	/*
	 * Change the appearance of minimized icon so user knows
	 * the mailbox closed.
	 */
	t_lptstr = utf8_to_lptstr(t_utf8);
	UpdateTrayIcon(NIM_MODIFY, ghMClosedIcon, t_lptstr);
	fs_give((void **) &t_lptstr);
	PostMessage(ghTTYWnd,WM_SETICON,ICON_BIG,(LPARAM) ghMClosedIcon);
	PostMessage(ghTTYWnd,WM_SETICON,ICON_SMALL,(LPARAM) ghMClosedIcon);

	gpTTYInfo->fMClosedIcon = TRUE;
    }
    else if(t_utf8 == NULL && gpTTYInfo->fMClosedIcon) {
	/* only go from yellow to green */
	UpdateTrayIcon(NIM_MODIFY, ghNormalIcon, TEXT("Alpine"));
	PostMessage(ghTTYWnd,WM_SETICON,ICON_BIG,(LPARAM) ghNormalIcon);
	PostMessage(ghTTYWnd,WM_SETICON,ICON_SMALL,(LPARAM) ghNormalIcon);

	gpTTYInfo->fMClosedIcon = FALSE;
    }
}

void
mswin_trayicon(int show)
{
    if(show){
	if(!gpTTYInfo->fTrayIcon){
	    UpdateTrayIcon(NIM_ADD, ghNormalIcon, TEXT("Alpine"));
	    gpTTYInfo->fTrayIcon = TRUE;
	}
    }
    else{
	if(gpTTYInfo->fTrayIcon){
	    UpdateTrayIcon(NIM_DELETE, 0, NULL);
	    gpTTYInfo->fTrayIcon = FALSE;
	}
    }
}


void
UpdateTrayIcon(DWORD dwMsg, HICON hIcon, LPTSTR tip)
{
    NOTIFYICONDATA nt;

    nt.cbSize = sizeof (nt);
    nt.hWnd   = ghTTYWnd;
    nt.uID    = TASKBAR_ICON_NEWMAIL;
    switch(dwMsg){
      case NIM_DELETE :
	nt.uFlags = 0;
	break;

      case NIM_ADD :		/* send msg to add icon to tray */
	nt.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	nt.uCallbackMessage = TASKBAR_ICON_MESSAGE;

	if(tip){
	    _tcsncpy (nt.szTip, tip, 63);
	    nt.szTip[63] = '\0';
	}
	else
	  nt.szTip[0] = '\0';
	nt.hIcon = hIcon;
	break;

      case NIM_MODIFY :		/* send msg to modify icon in tray */
	nt.uFlags = NIF_ICON | NIF_TIP;
	if(tip){
	    _tcsncpy (nt.szTip, tip, 63);
	    nt.szTip[63] = '\0';
	}
	else
	  nt.szTip[0] = '\0';
	nt.hIcon = hIcon;
	break;

      default :
	return;
    }

    Shell_NotifyIcon (dwMsg, &nt);
}
	


/*---------------------------------------------------------------------------
 *
 * Client level menu item stuff.
 *
 * These are menu items that activate commands in the "client" program.
 * Generally, the client calls us to tell us which menu items are active
 * and what key stroke they generate.  When such an item is selected it's
 * key stroke is injected into the character queue as if it was typed by
 * the user.
 *
 *-------------------------------------------------------------------------*/

/*
 * Clear active status of all "menu items".
 */
void
mswin_menuitemclear (void)
{
    int			i;
    HWND		hWnd;


    for (i = 0; i < KS_COUNT; ++i) {
	gpTTYInfo->menuItems[i].miActive = FALSE;
	if (gpTTYInfo->toolBarSize > 0) {
	    hWnd = GetDlgItem(gpTTYInfo->hTBWnd, i + KS_RANGESTART);
	    if (hWnd != NULL)
		EnableWindow(hWnd, FALSE);
	}
    }

    gpTTYInfo->menuItemsCurrent = FALSE;
    gpTTYInfo->menuItemsIndex = 1;
    for(i = 0; i < KS_COUNT; i++)
      gpTTYInfo->menuItems[i].miIndex = 0;
}


/*
 * Add an item to the cmdmenu
 */
void
mswin_menuitemadd (UCS key, char *label, int menuitem, int flags)
{
    int			i;
    HWND		hWnd;

    if (menuitem >= KS_RANGESTART && menuitem <= KS_RANGEEND) {
	
	gpTTYInfo->menuItemsCurrent = FALSE;
	
	i = menuitem - KS_RANGESTART;
	gpTTYInfo->menuItems[i].miActive = TRUE;
	gpTTYInfo->menuItems[i].miKey = key;
	if(!gpTTYInfo->menuItems[i].miIndex){
	    gpTTYInfo->menuItems[i].miLabel = label;
	    gpTTYInfo->menuItems[i].miIndex = gpTTYInfo->menuItemsIndex++;
	}

	if (gpTTYInfo->toolBarSize > 0) {
	    hWnd = GetDlgItem(gpTTYInfo->hTBWnd, menuitem);
	    if (hWnd != NULL)
		EnableWindow(hWnd, TRUE);
	}
    }
}


/*
 * Called when a menu command arrives with an unknown ID.  If it is
 * within the range of the additional menu items, insert the
 * corresponding character into the char input queue.
 */
void
ProcessMenuItem (HWND hWnd, WPARAM wParam)
{
    PTTYINFO		pTTYInfo;
    int			i;

    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
    if (pTTYInfo == NULL)
	return;


    if (wParam >= KS_RANGESTART && wParam <= KS_RANGEEND) {
	i = (int)(wParam - KS_RANGESTART);
	if (pTTYInfo->menuItems[i].miActive)
	    CQAdd (pTTYInfo->menuItems[i].miKey, 0);
    }
}


/*
 * Called to set a new menu.
 */
int
mswin_setwindowmenu (int menu)
{
    int		oldmenu;
    HMENU	holdmenu;
    HMENU	hmenu;


    oldmenu = gpTTYInfo->curWinMenu;
    holdmenu = GetMenu (ghTTYWnd);
    if (gpTTYInfo->curWinMenu != menu) {
	
	hmenu = LoadMenu (ghInstance, MAKEINTRESOURCE (menu));
	if (hmenu != NULL) {
	    if (SetMenu (ghTTYWnd, hmenu) != 0) {
	        DestroyMenu (holdmenu);
		gfHelpMenu = gfHelpGenMenu = FALSE;
		gpTTYInfo->curWinMenu = menu;
	    }
	}

	if(menu == MENU_DEFAULT){
	    TBSwap (ghTTYWnd, IDD_TOOLBAR);
	    if(hmenu != NULL)
	      (void) MSWHelpSetMenu(hmenu);
	}
	else{
	    TBSwap (ghTTYWnd, IDD_COMPOSER_TB);
	}
    }
    return (oldmenu);
}



/*---------------------------------------------------------------------------
 *
 * Printing stuff
 *
 *-------------------------------------------------------------------------*/

/*
 * Printing globals
 */
LOCAL HDC	P_PrintDC;	/* Printer device context. */
LOCAL int	P_PageRows;	/* Number of rows we put on a page. */
LOCAL int	P_PageColumns;	/* Number of columns we put on a page. */
LOCAL int	P_RowHeight;	/* Hight of a row in printer pixels. */
LOCAL int	P_CurRow;	/* Current row, starting at zero */
LOCAL int	P_CurCol;	/* Current col, starting at zero. */
LOCAL int	P_TopOffset;	/* Top Margin offset, in pixels. */
LOCAL int	P_LeftOffset;	/* Top Margin offset, in pixels. */
LOCAL HFONT	P_hFont;	/* Handle to printing font. */
LPTSTR		P_LineText;	/* Pointer to line buffer. */




/*
 * Define the margin as number of lines at top and bottom of page.
 * (might be better to define as a percent of verticle page size)
 */
#define VERTICLE_MARGIN		3	/* lines at top and bottom of page. */
#define HORIZONTAL_MARGIN	1	/* margine at left & right in chars */

/*
 * Several errors that can be reported.
 */
#define PE_DIALOG_FAILED	1
#define PE_USER_CANCEL		2
#define PE_CANT_START_DOC	3
#define PE_CANT_FINISH_DOC	4
#define PE_OUT_OF_MEMORY	5
#define PE_GENERAL_ERROR	6
#define PE_OUT_OF_DISK		7
#define PE_PRINTER_NOT_FOUND	8
#define PE_PINE_INTERNAL	9
#define PE_FONT_FAILED		10


LOCAL struct pe_error_message {
	int		error_code;
	char		*error_message;
 } P_ErrorMessages[] = {
	{ PE_DIALOG_FAILED,	"Print Dialog Failed"},
        { PE_USER_CANCEL,	"User canceled" },
        { PE_CANT_START_DOC,	"Can't start document" },
	{ PE_OUT_OF_MEMORY,	"Out of memory" },
        { PE_CANT_FINISH_DOC,	"Can't finish document" },
	{ PE_OUT_OF_MEMORY,	"Out of memory" },
	{ PE_OUT_OF_DISK,	"Out of disk space" },
	{ PE_PRINTER_NOT_FOUND,	"Printer not found" },
	{ PE_PINE_INTERNAL,	"Pine internal error" },
	{ PE_FONT_FAILED,	"Failed to create font" },
	{ 0, NULL }};



/*
 * Send text in the line buffer to the printer.
 * Advance to next page if necessary.
 */
LOCAL int
_print_send_line (void)
{
    int		status;

    status = 0;
    if (P_CurCol > 0)
	TextOut (P_PrintDC, P_LeftOffset,
				P_TopOffset + (P_CurRow * P_RowHeight),
		P_LineText, P_CurCol);
    P_CurCol = 0;
    if (++P_CurRow >= P_PageRows)
	status = _print_send_page ();
	
    return (status);
}



/*
 * Advance one page.
 */
int
_print_send_page (void)
{
    DWORD status;

    if((status = EndPage (P_PrintDC)) > 0){
	P_CurRow = 0;
	if((status = StartPage (P_PrintDC)) > 0){
	    SelectObject (P_PrintDC, P_hFont);
	    return(0);
	}
    }

#ifdef	WIN32
    ExplainSystemErr();
    return(PE_GENERAL_ERROR);
#else
    switch (status) {
    case SP_USERABORT:		return (PE_USER_CANCEL);
    case SP_OUTOFDISK:		return (PE_OUT_OF_DISK);
    case SP_OUTOFMEMORY:	return (PE_OUT_OF_MEMORY);
    default:
    case SP_ERROR:		break;
    }
#endif
    return (PE_GENERAL_ERROR);
}



/*
 * Map errors reported to my own error set.
 */
int
_print_map_dlg_error (DWORD error)
{
    switch (error) {
    case 0:			    return (PE_USER_CANCEL);
    case CDERR_MEMALLOCFAILURE:
    case CDERR_MEMLOCKFAILURE:
				    return (PE_OUT_OF_MEMORY);
    case PDERR_PRINTERNOTFOUND:
    case PDERR_NODEVICES:
				    return (PE_PRINTER_NOT_FOUND);
    case CDERR_STRUCTSIZE:
				    return (PE_PINE_INTERNAL);
    default:
				    return (PE_GENERAL_ERROR);
    }
}
	

/*
 * This is used for converting from UTF-8 to UCS and is
 * global so that mswin_print_ready can initialize it.
 */
static CBUF_S print_cb;

/*
 * Get the printer ready.  Returns ZERO for success, or an error code that
 * can be passed to mswin_print_error() to get a text message.
 */
int
mswin_print_ready(WINHAND hWnd, LPTSTR docDesc)
{
    PRINTDLG		pd;
    DOCINFO		di;
    TEXTMETRIC		tm;
    HDC			hDC;
    int			fontSize;	/* Size in Points. */
    int			ppi;		/* Pixels per inch in device. */
    int			xChar;
    int			status;
    HFONT		oldFont;
    LOGFONT		newFont;


    status = 0;
    P_PrintDC = NULL;

    print_cb.cbufp = print_cb.cbuf;

    /*
     * Show print dialog.
     */
    memset(&pd, 0, sizeof(PRINTDLG));
    pd.lStructSize = sizeof (PRINTDLG);
    pd.hwndOwner = (hWnd ? (HWND) hWnd : ghTTYWnd);
    pd.hDevMode = NULL;
    pd.hDevNames = NULL;
    pd.Flags = PD_ALLPAGES | PD_NOSELECTION | PD_NOPAGENUMS |
	    PD_HIDEPRINTTOFILE | PD_RETURNDC;
    pd.nCopies = 1;
    if(PrintDlg (&pd) == 0)
	return(_print_map_dlg_error (CommDlgExtendedError()));

    /*
     * Returns the device name which we could use to remember what printer
     * they selected.  But instead, we just toss them.
     */
    if (pd.hDevNames)
	GlobalFree (pd.hDevNames);
    if (pd.hDevMode)
	GlobalFree (pd.hDevMode);

    /*
     * Get the device drawing context.
     * (does PringDlg() ever return success but fail to return a DC?)
     */
    if (pd.hDC != NULL)
	P_PrintDC = pd.hDC;
    else {
        status = PE_DIALOG_FAILED;
	goto Done;
    }

    /*
     * Start Document
     */
    memset(&di, 0, sizeof (DOCINFO));
    di.cbSize = sizeof (DOCINFO);
    di.lpszDocName = docDesc;		/* This appears in the print manager*/
    di.lpszOutput = NULL;		/* Could suply a file name to print
					   to. */
    if(StartDoc(P_PrintDC, &di) <= 0) {
	ExplainSystemErr();
	status = PE_CANT_START_DOC;
	DeleteDC (P_PrintDC);
	P_PrintDC = NULL;
	goto Done;
    }

    /*
     * Printer font is either same as window font, or is it's own
     * font.
     */
    if (gPrintFontSameAs) {

	/*
	 * Get the current font size in points, then create a new font
	 * of same size for printer.  Do the calculation using the actual
	 * screen resolution instead of the logical resolution so that
	 * we get pretty close to the same font size on the printer
	 * as we see on the screen.
	 */
	hDC = GetDC (ghTTYWnd);			/* Temp screen DC. */
	ppi = (int) ((float)GetDeviceCaps (hDC, VERTRES) /
		    ((float) GetDeviceCaps (hDC, VERTSIZE) / 25.3636));
#ifdef FDEBUG
	if (mswin_debug >= 8) {
	    fprintf (mswin_debugfile, "mswin_print_ready:  Screen res %d ppi, font height %d pixels\n",
		ppi, -gpTTYInfo->lfTTYFont.lfHeight);
	    fprintf (mswin_debugfile, "                    Screen height %d pixel, %d mm\n",
		    GetDeviceCaps (hDC, VERTRES), GetDeviceCaps (hDC, VERTSIZE));
	}
#endif
	ReleaseDC (ghTTYWnd, hDC);

	/* Convert from screen pixels to points. */
	fontSize = MulDiv (-gpTTYInfo->lfTTYFont.lfHeight, 72, ppi);
	++fontSize;		/* Fudge a little. */


	/* Get printer resolution and convert form points to printer pixels. */
	ppi = GetDeviceCaps (P_PrintDC, LOGPIXELSY);
	newFont.lfHeight =  -MulDiv (fontSize, ppi, 72);
	_tcsncpy(newFont.lfFaceName, gpTTYInfo->lfTTYFont.lfFaceName, LF_FACESIZE);
	newFont.lfFaceName[LF_FACESIZE-1] = 0;
	newFont.lfItalic = gpTTYInfo->lfTTYFont.lfItalic;
	newFont.lfWeight = gpTTYInfo->lfTTYFont.lfWeight;
	newFont.lfCharSet = gpTTYInfo->lfTTYFont.lfCharSet;
	

#ifdef FDEBUG
	if (mswin_debug >= 8) {
	    fprintf (mswin_debugfile, "                    font Size %d points\n",
		    fontSize);
	    fprintf (mswin_debugfile, "                    printer res %d ppi, font height %d pixels\n",
		ppi, -newFont.lfHeight);
	    fprintf (mswin_debugfile, "                    paper height %d pixel, %d mm\n",
		    GetDeviceCaps (P_PrintDC, VERTRES),
		    GetDeviceCaps (P_PrintDC, VERTSIZE));
	}
#endif
    }
    else {
	ppi = GetDeviceCaps (P_PrintDC, LOGPIXELSY);
	newFont.lfHeight =  -MulDiv (gPrintFontSize, ppi, 72);
	_tcsncpy(newFont.lfFaceName, gPrintFontName, LF_FACESIZE);
	newFont.lfFaceName[LF_FACESIZE-1] = 0;
	newFont.lfWeight = 0;
	if(_tcsstr(gPrintFontStyle, TEXT("bold")))
	    newFont.lfWeight = FW_BOLD;

	newFont.lfItalic = 0;
	if(_tcsstr(gPrintFontStyle, TEXT("italic")))
	    newFont.lfItalic = 1;

	newFont.lfCharSet = mswin_string2charsetid(gPrintFontCharSet);
    }

	
    /* Fill out rest of font description and request font. */
    newFont.lfWidth =          0;
    newFont.lfEscapement =     0;
    newFont.lfOrientation =    0;
    newFont.lfUnderline =      0;
    newFont.lfStrikeOut =      0;
    newFont.lfOutPrecision =   OUT_DEFAULT_PRECIS;
    newFont.lfClipPrecision =  CLIP_DEFAULT_PRECIS;
    newFont.lfQuality =        DEFAULT_QUALITY;
    newFont.lfPitchAndFamily = FIXED_PITCH;
    P_hFont = CreateFontIndirect (&newFont);
    if (P_hFont == NULL) {
	status = PE_FONT_FAILED;
	DeleteDC (P_PrintDC);
	goto Done;
    }


    /*
     * Start page.
     * Must select font for each page or it returns to default.
     * Windows seems good about maping selected font to a font that
     * will actually print on the printer.
     */
    StartPage (P_PrintDC);
    oldFont = SelectObject (P_PrintDC, P_hFont);


    /*
     * Find out about the font we got and set up page size and margins.
     * This assumes all pages are the same size - which seems reasonable.
     */
    GetTextMetrics (P_PrintDC, &tm);
    xChar = tm.tmAveCharWidth;			
    P_RowHeight = tm.tmHeight + tm.tmExternalLeading;

    /* HORZRES and VERTRES report size of page in printer pixels. */
    P_PageColumns = GetDeviceCaps (P_PrintDC, HORZRES) / xChar;
    P_PageRows = GetDeviceCaps (P_PrintDC, VERTRES) / P_RowHeight;

    /* We allow a margin at top and bottom measured in text rows. */
    P_PageRows -= VERTICLE_MARGIN * 2;
    P_TopOffset = VERTICLE_MARGIN * P_RowHeight;

    /* And allow for a left and right margine measured in characters. */
    P_PageColumns -= HORIZONTAL_MARGIN * 2;
    P_LeftOffset = HORIZONTAL_MARGIN * xChar;

    P_CurRow = 0;			/* Start counting at row 0. */
    P_CurCol = 0;			/* At character 0. */
    P_LineText = (LPTSTR) MemAlloc((P_PageColumns + 1) * sizeof(TCHAR));
    if(P_LineText == NULL){
	if(EndDoc (P_PrintDC) <= 0)
	  ExplainSystemErr();

	DeleteObject (P_hFont);
	P_hFont = NULL;
	DeleteDC (P_PrintDC);
	P_PrintDC = NULL;
	status = PE_OUT_OF_MEMORY;
	goto Done;
    }


Done:
    return (status);
}


/*
 * Called when printing is done.
 * xxx what happens if there is an error?  Will this get called?
 */
int
mswin_print_done(void)
{
    if(P_PrintDC != NULL){
	if(P_LineText != NULL)
	  MemFree((void *) P_LineText);

	if(EndPage (P_PrintDC) <= 0 || EndDoc (P_PrintDC) <= 0)
	  ExplainSystemErr();

	DeleteObject(P_hFont);
	P_hFont = NULL;
	DeleteDC(P_PrintDC);
	P_PrintDC = NULL;
    }

    return (0);
}


/*
 * Return ponter to a text string that describes the erorr.
 */
char *
mswin_print_error(int error_code)
{
    int i;

    for(i = 0; P_ErrorMessages[i].error_message != NULL; ++i)
      if(P_ErrorMessages[i].error_code == error_code)
	return(P_ErrorMessages[i].error_message);

    return("(Unknown error)");
}


/*
 * Add a single character to the current line.
 * Only handles CRLF carrage control.
 */
int
mswin_print_char(TCHAR c)
{
    switch(c){
      case ASCII_CR:
	return(0);

      case ASCII_LF:
	return(_print_send_line());
	
      case ASCII_TAB:
	do{
	    if(P_CurCol == P_PageColumns)
	      return(_print_send_line());

	    *(P_LineText + P_CurCol++) = ' ';
	}
	while(P_CurCol % PRINT_TAB_SIZE != 0);
	return(0);

      default:
	if(P_CurCol == P_PageColumns){
	    int status;

	    if((status = _print_send_line()) != 0)
	      return(status);
	}

	*(P_LineText + P_CurCol++) = c;
	return(0);
    }
}


int
mswin_print_char_utf8(int c)
{
    UCS ucs;
    TCHAR tc;
    int ret = 0;

    if(utf8_to_ucs4_oneatatime(c, &print_cb, &ucs, NULL)){
	/* bogus conversion ignores UTF-16 */
	tc = (TCHAR) ucs;
	ret = mswin_print_char(tc);
    }

    return(ret);
}


/*
 * Send a string to the printer.
 */
int
mswin_print_text(LPTSTR text)
{
    int status;

    if(text)
      while(*text)
	if((status = mswin_print_char(*(text++))) != 0)
	  return(status);

    return(0);
}


int
mswin_print_text_utf8(char *text_utf8)
{
    LPTSTR text_lpt;
    int ret = 0;

    if(text_utf8){
	text_lpt = utf8_to_lptstr(text_utf8);
	if(text_lpt){
	    ret = mswin_print_text(text_lpt);
	    fs_give((void **) &text_lpt);
	}
    }

    return(ret);
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *        File dialog boxes.
 *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

LOCAL TCHAR gHomeDir[PATH_MAX];
LOCAL TCHAR gLastDir[PATH_MAX];

/*
 * Keep track of the last dir visited.  Most of the time pine just passes us
 * the "home directory", which usually is not where the user wants to start.
 * Assume that the first time we are called we are being passed the home
 * direcory.
 */
static void
FillInitialDir (LPCTSTR *iDir, LPTSTR targDir)
{
    if (_tcslen (gHomeDir) == 0) {
	_tcscpy (gHomeDir, targDir);
	*iDir = targDir;
    }
    else if (_tcscmp (gHomeDir, targDir) == 0 && *gLastDir)
	    *iDir = gLastDir;
	else
	    *iDir = targDir;
}



/*
 * Display a save file dialog box.
 *
 *	dir_utf8	>	directory to start search in
 *			<	directory finished in.
 *	fName_utf8	<	Name of file selected
 *	nMaxDName		length of dir_utf8
 *	nMaxFName		length of fName_utf8.
 *
 *      Possible return values:
 *      0   no file selected
 *      1   file selected
 *     -1   some sort of error
 */
int
mswin_savefile(char *dir_utf8, int nMaxDName, char *fName_utf8, int nMaxFName)
{
    OPENFILENAME	ofn;
    TCHAR               filters[128], moniker[128];
    DWORD		rc, len;
    LPTSTR		p, extlist_lpt;
    LPTSTR		fName_lpt, f, dir_lpt;
    char	       *cp;

    /* convert fName_utf8 to LPTSTR */
    fName_lpt = (LPTSTR) fs_get(nMaxFName * sizeof(TCHAR));
    fName_lpt[0] = '\0';
    if(fName_utf8 && fName_utf8[0]){
	f = utf8_to_lptstr(fName_utf8);
	if(f){
	    _tcsncpy(fName_lpt, f, nMaxFName);
	    fName_lpt[nMaxFName-1] = '\0';
	    fs_give((void **) &f);
	}
    }

    dir_lpt = (LPTSTR) fs_get(nMaxDName * sizeof(TCHAR));
    dir_lpt[0] = '\0';
    if(dir_utf8 && dir_utf8[0]){
	f = utf8_to_lptstr(dir_utf8);
	if(f){
	    _tcsncpy(dir_lpt, f, nMaxDName);
	    dir_lpt[nMaxDName-1] = '\0';
	    fs_give((void **) &f);
	}
    }

    for(extlist_lpt = NULL, p = _tcschr(fName_lpt, '.'); p; p = _tcschr(++p, '.'))
      extlist_lpt = p;

    len = sizeof(moniker)/sizeof(TCHAR);
    if(extlist_lpt && MSWRPeek(HKEY_CLASSES_ROOT, extlist_lpt, NULL, moniker, &len) == TRUE){
	len = sizeof(filters)/sizeof(TCHAR);
	filters[0] = '\0';
	if(MSWRPeek(HKEY_CLASSES_ROOT, moniker, NULL, filters, &len) == TRUE)
	  _sntprintf(filters + _tcslen(filters),
		     sizeof(filters)/sizeof(TCHAR) - _tcslen(filters),
		     TEXT(" (*%s)#*%s#"), extlist_lpt, extlist_lpt);
	else
	  _sntprintf(filters, sizeof(filters)/sizeof(TCHAR),
		     TEXT("%s (*%s)#*%s#"), moniker, extlist_lpt, extlist_lpt);
    }
    else
      filters[0] = '\0';

    _tcsncat(filters, TEXT("Text Files (*.txt)#*.txt#All Files (*.*)#*.*#"),
	     sizeof(filters)/sizeof(TCHAR));
    filters[sizeof(filters)/sizeof(TCHAR) - 1] = '\0';

    for(p = filters; *p != '\0'; ++p)
      if(*p == L'#')
        *p = '\0';

    /* Set up the BIG STRUCTURE. */
    memset (&ofn, 0, sizeof(ofn));
    /*
     * sizeof(OPENFILENAME) used to work but doesn't work now with
     * pre-Windows 2000. This is supposed to be the magic constant to
     * make it work in both cases, according to MSDN.
     */
    ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;
    ofn.hwndOwner = ghTTYWnd;
    ofn.lpstrFilter = filters;
    ofn.lpstrCustomFilter = NULL;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = fName_lpt;
    ofn.nMaxFile = nMaxFName;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    FillInitialDir (&ofn.lpstrInitialDir, dir_lpt);
    ofn.lpstrTitle = TEXT("Save To File");
    ofn.Flags = OFN_NOREADONLYRETURN | OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = TEXT("txt");

    if(GetSaveFileName(&ofn)){
	if(ofn.nFileOffset > nMaxDName-1){
	    if(fName_lpt)
	      fs_give((void **) &fName_lpt);

	    if(dir_lpt)
	      fs_give((void **) &dir_lpt);

	    return(0);
	}

	/* Copy directory name to dir_lpt. */
	_tcsncpy(dir_lpt, fName_lpt, nMaxDName-1);
	dir_lpt[nMaxDName-1] = '\0';
	if(dir_lpt[ofn.nFileOffset-1] == '\\'
	   && !(ofn.nFileOffset == 3 && _istalpha(dir_lpt[0]) && dir_lpt[1] == ':'))
	  dir_lpt[ofn.nFileOffset-1] = '\0';
        else
	  dir_lpt[ofn.nFileOffset] = '\0';

	/* Remember last dir visited. */
	_tcsncpy(gLastDir, dir_lpt, PATH_MAX);
	gLastDir[PATH_MAX-1] = '\0';

	/* convert back to UTF-8 */
	cp = lptstr_to_utf8(dir_lpt);
	if(cp){
	    strncpy(dir_utf8, cp, nMaxDName-1);
	    dir_utf8[nMaxDName-1] = '\0';
	    fs_give((void **) &cp);
	}

	p = fName_lpt + ofn.nFileOffset;

	/* convert fName back to UTF-8 */
	cp = lptstr_to_utf8(p);
	if(cp){
	    strncpy(fName_utf8, cp, nMaxFName-1);
	    fName_utf8[nMaxFName-1] = '\0';
	    fs_give((void **) &cp);
	}

	if(fName_lpt)
	  fs_give((void **) &fName_lpt);

	if(dir_lpt)
	  fs_give((void **) &dir_lpt);

	return(1);
    }
    else{
	if(fName_lpt)
	  fs_give((void **) &fName_lpt);

	if(dir_lpt)
	  fs_give((void **) &dir_lpt);

	rc = CommDlgExtendedError();
	return(rc ? -1 : 0);
    }
}




/*
 * Display an open file dialog box.
 *
 *	dir_utf8	>	directory to start search in
 *			<	directory finished in.
 *	fName_utf8	<	Name of file selected
 *	nMaxDName		length of dir_utf8
 *	nMaxFName		length of fName_utf8.
 *
 *      Possible return values:
 *      0   no file selected
 *      1   file selected
 *     -1   some sort of error
 */
int
mswin_openfile(char *dir_utf8, int nMaxDName, char *fName_utf8,
	       int nMaxFName, char *extlist_utf8)
{
    OPENFILENAME	ofn;
    TCHAR		filters[1024];
    DWORD		rc;
    LPTSTR		p;
    LPTSTR		extlist_lpt = NULL;
    LPTSTR		fName_lpt, f, dir_lpt;
    char               *cp;


    if(extlist_utf8)
      extlist_lpt = utf8_to_lptstr(extlist_utf8);

    /*
     * Set filters array.  (pairs of null terminated strings, terminated
     * by a double null).
     */
    _sntprintf(filters, sizeof(filters)/sizeof(TCHAR),
               TEXT("%s%sAll Files (*.*)#*.*#"),
	       extlist_lpt ? extlist_lpt : TEXT(""), extlist_lpt ? TEXT("#") : TEXT(""));

    if(extlist_lpt)
      fs_give((void **) &extlist_lpt);

    for(p = filters; *p != '\0'; ++p)
      if(*p == L'#')
        *p = '\0';

    /* Set up the BIG STRUCTURE. */
    memset(&ofn, 0, sizeof(ofn));

    /* convert fName_utf8 to LPTSTR */
    fName_lpt = (LPTSTR) fs_get(nMaxFName * sizeof(TCHAR));
    fName_lpt[0] = '\0';
    if(fName_utf8 && fName_utf8[0]){
	f = utf8_to_lptstr(fName_utf8);
	if(f){
	    _tcsncpy(fName_lpt, f, nMaxFName);
	    fName_lpt[nMaxFName-1] = '\0';
	    fs_give((void **) &f);
	}
    }

    dir_lpt = (LPTSTR) fs_get(nMaxDName * sizeof(TCHAR));
    dir_lpt[0] = '\0';
    if(dir_utf8 && dir_utf8[0]){
	f = utf8_to_lptstr(dir_utf8);
	if(f){
	    _tcsncpy(dir_lpt, f, nMaxDName);
	    dir_lpt[nMaxDName-1] = '\0';
	    fs_give((void **) &f);
	}
    }

    /*
     * sizeof(OPENFILENAME) used to work but doesn't work now with
     * pre-Windows 2000. This is supposed to be the magic constant to
     * make it work in both cases, according to MSDN.
     */
    ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;
    ofn.hwndOwner = ghTTYWnd;
    ofn.lpstrFilter = filters;
    ofn.lpstrCustomFilter = NULL;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = fName_lpt;
    ofn.nMaxFile = nMaxFName;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    FillInitialDir(&ofn.lpstrInitialDir, dir_lpt);
    ofn.lpstrTitle = TEXT("Select File");
    ofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = TEXT("txt");

    if(GetOpenFileName(&ofn)){
	if(ofn.nFileOffset > nMaxDName-1){
	    if(fName_lpt)
	      fs_give((void **) &fName_lpt);

	    if(dir_lpt)
	      fs_give((void **) &dir_lpt);

	    return(0);
	}

	/* Copy directory name to dir_lpt. */
	_tcsncpy(dir_lpt, fName_lpt, nMaxDName-1);
	dir_lpt[nMaxDName-1] = '\0';
	if(dir_lpt[ofn.nFileOffset-1] == '\\'
	   && !(ofn.nFileOffset == 3 && _istalpha(dir_lpt[0]) && dir_lpt[1] == ':'))
	  dir_lpt[ofn.nFileOffset-1] = '\0';
        else
	  dir_lpt[ofn.nFileOffset] = '\0';

	/* Remember last dir visited. */
	_tcsncpy(gLastDir, dir_lpt, PATH_MAX);
	gLastDir[PATH_MAX-1] = '\0';

	/* convert back to UTF-8 */
	cp = lptstr_to_utf8(dir_lpt);
	if(cp){
	    strncpy(dir_utf8, cp, nMaxDName-1);
	    dir_utf8[nMaxDName-1] = '\0';
	    fs_give((void **) &cp);
	}
	
	p = fName_lpt + ofn.nFileOffset;

	/* convert fName back to UTF-8 */
	cp = lptstr_to_utf8(p);
	if(cp){
	    strncpy(fName_utf8, cp, nMaxFName-1);
	    fName_utf8[nMaxFName-1] = '\0';
	    fs_give((void **) &cp);
	}

	if(fName_lpt)
	  fs_give((void **) &fName_lpt);

	if(dir_lpt)
	  fs_give((void **) &dir_lpt);

	return(1);
    }
    else{
	if(fName_lpt)
	  fs_give((void **) &fName_lpt);

	if(dir_lpt)
	  fs_give((void **) &dir_lpt);

	rc = CommDlgExtendedError();
	return(rc ? -1 : 0);
    }
}


/*
 * Display an open file dialog box.
 * Allow selection of multiple files.
 *
 *	dir_utf8	>	directory to start search in
 *			<	directory finished in.
 *	fName_utf8	<	Names of files selected
 *	nMaxDName		length of dir_utf8
 *	nMaxFName		length of fName_utf8.
 *
 *      Possible return values:
 *      0   no file selected
 *      1   file selected
 *     -1   some sort of error
 */
int
mswin_multopenfile(char *dir_utf8, int nMaxDName, char *fName_utf8,
		   int nMaxFName, char *extlist_utf8)
{
    OPENFILENAME	ofn;
    TCHAR		filters[1024];
    DWORD		rc;
    LPTSTR		p;
    LPTSTR		extlist_lpt = NULL;
    LPTSTR		fName_lpt, f, dir_lpt;
    char	       *cp, *q;


    if(extlist_utf8)
      extlist_lpt = utf8_to_lptstr(extlist_utf8);

    /*
     * Set filters array.  (pairs of null terminated strings, terminated
     * by a double null).
     */
    _sntprintf(filters, sizeof(filters)/sizeof(TCHAR),
               TEXT("%s%sAll Files (*.*)#*.*#"),
	       extlist_lpt ? extlist_lpt : TEXT(""), extlist_lpt ? TEXT("#") : TEXT(""));

    if(extlist_lpt)
      fs_give((void **) &extlist_lpt);

    for(p = filters; *p != '\0'; ++p)
      if(*p == L'#')
        *p = '\0';

    /* Set up the BIG STRUCTURE. */
    memset (&ofn, 0, sizeof(ofn));

    /* convert fName_utf8 to LPTSTR */
    fName_lpt = (LPTSTR) fs_get(nMaxFName * sizeof(TCHAR));
    memset(fName_lpt, 0, nMaxFName * sizeof(TCHAR));
    if(fName_utf8 && fName_utf8[0]){
	f = utf8_to_lptstr(fName_utf8);
	if(f){
	    _tcsncpy(fName_lpt, f, nMaxFName);
	    fName_lpt[nMaxFName-1] = '\0';
	    fs_give((void **) &f);
	}
    }

    dir_lpt = (LPTSTR) fs_get(nMaxDName * sizeof(TCHAR));
    memset(dir_lpt, 0, nMaxDName * sizeof(TCHAR));
    if(dir_utf8 && dir_utf8[0]){
	f = utf8_to_lptstr(dir_utf8);
	if(f){
	    _tcsncpy(dir_lpt, f, nMaxDName);
	    dir_lpt[nMaxDName-1] = '\0';
	    fs_give((void **) &f);
	}
    }

    /*
     * sizeof(OPENFILENAME) used to work but doesn't work now with
     * pre-Windows 2000. This is supposed to be the magic constant to
     * make it work in both cases, according to MSDN.
     */
    ofn.lStructSize = OPENFILENAME_SIZE_VERSION_400;
    ofn.hwndOwner = ghTTYWnd;
    ofn.lpstrFilter = filters;
    ofn.lpstrCustomFilter = NULL;
    ofn.nFilterIndex = 1;
    ofn.lpstrFile = fName_lpt;
    ofn.nMaxFile = nMaxFName;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    FillInitialDir(&ofn.lpstrInitialDir, dir_lpt);
    ofn.lpstrTitle = TEXT("Select Files");
    ofn.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER |
		OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = TEXT("txt");

    if(GetOpenFileName(&ofn)){
	if(ofn.nFileOffset > nMaxDName-1){
	    if(fName_lpt)
	      fs_give((void **) &fName_lpt);

	    if(dir_lpt)
	      fs_give((void **) &dir_lpt);

	    return(0);
	}

	/* Copy directory name to dir_lpt. */
	_tcsncpy(dir_lpt, fName_lpt, nMaxDName-1);
	dir_lpt[nMaxDName-1] = '\0';
	if(dir_lpt[ofn.nFileOffset-1] == '\\'
	   && !(ofn.nFileOffset == 3 && _istalpha(dir_lpt[0]) && dir_lpt[1] == ':'))
	  dir_lpt[ofn.nFileOffset-1] = '\0';
        else
	  dir_lpt[ofn.nFileOffset] = '\0';

	/* Remember last dir visited. */
	_tcsncpy(gLastDir, dir_lpt, PATH_MAX);
	gLastDir[PATH_MAX-1] = '\0';

	/* convert back to UTF-8 */
	cp = lptstr_to_utf8(dir_lpt);
	if(cp){
	    strncpy(dir_utf8, cp, nMaxDName-1);
	    dir_utf8[nMaxDName-1] = '\0';
	    fs_give((void **) &cp);
	}
	
	/*
	 * The file names are all in the same directory and are separated
	 * by '\0' characters and terminated by double '\0'.
	 * This fact depends on the OFN_EXPLORER bit being set in the flags
	 * above.
	 *
	 * This is complicated because we need to convert all of these file
	 * names to UTF-8.
	 */
	for(q=fName_utf8, p=fName_lpt + ofn.nFileOffset; *p; p += _tcslen(p)+1){
	    cp = lptstr_to_utf8(p);
	    if(cp){
		sstrncpy(&q, cp, (int)(nMaxFName-(q-fName_utf8)));
		if(q-fName_utf8 < nMaxFName){
		    *q++ = '\0';
		    if(q-fName_utf8 < nMaxFName)
		      *q = '\0';		/* the double null if this is the end */
		}
		else
		  fName_utf8[nMaxFName-1] = '\0';

		fs_give((void **) &cp);
	    }
	}

	fName_utf8[nMaxFName-1] = fName_utf8[nMaxFName-2] = '\0';

	if(fName_lpt)
	  fs_give((void **) &fName_lpt);

	if(dir_lpt)
	  fs_give((void **) &dir_lpt);

	return (1);
    }
    else{
	if(fName_lpt)
	  fs_give((void **) &fName_lpt);

	if(dir_lpt)
	  fs_give((void **) &dir_lpt);

	rc = CommDlgExtendedError();
	return(rc ? -1 : 0);
    }
}
	
	
/*---------------------------------------------------------------------------
 */

/*
 * pico_XXcolor() - each function sets a particular attribute
 */
void
pico_nfcolor(char *s)
{
    char cbuf[MAXCLEN];

    if(s){
	SetColorAttribute (&gpTTYInfo->rgbFGColor, s);
	pico_set_nfg_color();

	if(the_normal_color){
	  strncpy(the_normal_color->fg,
	          ConvertStringRGB(cbuf, sizeof(cbuf), gpTTYInfo->rgbFGColor),
		  MAXCOLORLEN+1);
	  the_normal_color->fg[MAXCOLORLEN] = '\0';
	}
    }
    else{
	gpTTYInfo->rgbFGColor = GetSysColor (COLOR_WINDOWTEXT);
	if(the_normal_color)
	  free_color_pair(&the_normal_color);
    }

    // Update all textwindows with the new FG color.
    mswin_tw_setcolor((MSWIN_TEXTWINDOW *)-1,
        gpTTYInfo->rgbFGColor, gpTTYInfo->rgbBGColor);
}


void
pico_nbcolor(char *s)
{
    char cbuf[MAXCLEN];

    if(s){
	SetColorAttribute (&gpTTYInfo->rgbBGColor, s);
	pico_set_nbg_color();

	if(the_normal_color){
	  strncpy(the_normal_color->bg,
	          ConvertStringRGB(cbuf, sizeof(cbuf), gpTTYInfo->rgbBGColor),
		  MAXCOLORLEN+1);
	  the_normal_color->fg[MAXCOLORLEN] = '\0';
	}
    }
    else{
	gpTTYInfo->rgbBGColor = GetSysColor (COLOR_WINDOW);
	if(the_normal_color)
	  free_color_pair(&the_normal_color);
    }

    // Update all textwindows with the new BG color.
    mswin_tw_setcolor((MSWIN_TEXTWINDOW *)-1,
        gpTTYInfo->rgbFGColor, gpTTYInfo->rgbBGColor);
}


void
pico_rfcolor(char *s)
{
    char cbuf[MAXCLEN];

    if(s){
	SetColorAttribute (&gpTTYInfo->rgbRFGColor, s);

	if(the_rev_color){
	  strncpy(the_rev_color->fg,
	          ConvertStringRGB(cbuf, sizeof(cbuf), gpTTYInfo->rgbRFGColor),
		  MAXCOLORLEN+1);
	  the_rev_color->fg[MAXCOLORLEN] = '\0';
	}
    }
    else{
	gpTTYInfo->rgbRFGColor = GetSysColor (COLOR_HIGHLIGHTTEXT);
	if(the_rev_color)
	  free_color_pair(&the_rev_color);
    }
}


void
pico_rbcolor(char *s)
{
    char cbuf[MAXCLEN];

    if(s){
	SetColorAttribute (&gpTTYInfo->rgbRBGColor, s);

	if(the_rev_color){
	  strncpy(the_rev_color->bg,
	          ConvertStringRGB(cbuf, sizeof(cbuf), gpTTYInfo->rgbRBGColor),
		  MAXCOLORLEN+1);
	  the_rev_color->bg[MAXCOLORLEN] = '\0';
	}
    }
    else{
	gpTTYInfo->rgbRBGColor = GetSysColor (COLOR_HIGHLIGHT);
	if(the_rev_color)
	  free_color_pair(&the_rev_color);
    }
}


int
pico_usingcolor()
{
    return(TRUE);
}


int
pico_count_in_color_table()
{
    return(visibleColorTableSize);
}


/*
 * Return a pointer to an rgb string for the input color. The output is 11
 * characters long and looks like rrr,ggg,bbb.
 *
 * Args    colorName -- The color to convert to ascii rgb.
 *
 * Returns  Pointer to a static buffer containing the rgb string.
 */
char *
color_to_asciirgb(char *colorName)
{
    static char  c_to_a_buf[3][RGBLEN+1];
    static int   whichbuf = 0;
    COLORREF	 cf;
    int          l;

    whichbuf = (whichbuf + 1) % 3;

    if(ConvertRGBString(colorName, &cf)){
	snprintf(c_to_a_buf[whichbuf], sizeof(c_to_a_buf[0]), "%.3d,%.3d,%.3d",
		GetRValue(cf), GetGValue(cf), GetBValue(cf));
    }
    else{
	/*
	 * If we didn't find the color it could be that it is the
	 * normal color (MATCH_NORM_COLOR) or the none color
	 * (MATCH_NONE_COLOR). If that is the case, this strncpy thing
	 * will work out correctly because those two strings are
	 * RGBLEN long. Otherwise we're in a bit of trouble. This
	 * most likely means that the user is using the same pinerc on
	 * two terminals, one with more colors than the other. We didn't
	 * find a match because this color isn't present on this terminal.
	 * Since the return value of this function is assumed to be
	 * RGBLEN long, we'd better make it that long.
	 * It still won't work correctly because colors will be screwed up,
	 * but at least the embedded colors in filter.c will get properly
	 * sucked up when they're encountered.
	 */
	strncpy(c_to_a_buf[whichbuf], "xxxxxxxxxxx", RGBLEN);
	l = (int)strlen(colorName);
	strncpy(c_to_a_buf[whichbuf], colorName, (l < RGBLEN) ? l : RGBLEN);
	c_to_a_buf[whichbuf][RGBLEN] = '\0';
    }

    return(c_to_a_buf[whichbuf]);
}


void
pico_set_nfg_color()
{
    FlushWriteAccum ();
    gpTTYInfo->curAttrib.rgbFG = gpTTYInfo->rgbFGColor;
}

void
pico_set_nbg_color()
{
    FlushWriteAccum ();
    gpTTYInfo->curAttrib.rgbBG = gpTTYInfo->rgbBGColor;
}

void
pico_set_normal_color()
{
    pico_set_nfg_color();
    pico_set_nbg_color();
}

COLOR_PAIR *
pico_get_rev_color()
{
    char fgbuf[MAXCLEN], bgbuf[MAXCLEN];

    if(!the_rev_color)
      the_rev_color =
	   new_color_pair(ConvertStringRGB(fgbuf,sizeof(fgbuf),gpTTYInfo->rgbRFGColor),
			  ConvertStringRGB(bgbuf,sizeof(bgbuf),gpTTYInfo->rgbRBGColor));
    return(the_rev_color);
}

COLOR_PAIR *
pico_get_normal_color()
{
    char fgbuf[MAXCLEN], bgbuf[MAXCLEN];

    if(!the_normal_color)
      the_normal_color =
	   new_color_pair(ConvertStringRGB(fgbuf,sizeof(fgbuf),gpTTYInfo->rgbFGColor),
			  ConvertStringRGB(bgbuf,sizeof(bgbuf),gpTTYInfo->rgbBGColor));
    return(the_normal_color);
}

/*
 * Sets color to (fg,bg).
 * Flags == PSC_NONE  No alternate default if fg,bg fails.
 *       == PSC_NORM  Set it to Normal color on failure.
 *       == PSC_REV   Set it to Reverse color on failure.
 *
 * If flag PSC_RET is set, returns an allocated copy of the previous
 * color pair, otherwise returns NULL.
 */
COLOR_PAIR *
pico_set_colors(char *fg, char *bg, int flags)
{
    COLOR_PAIR *cp = NULL;

    if(flags & PSC_RET)
      cp = pico_get_cur_color();

    if(!(fg && bg && pico_set_fg_color(fg) && pico_set_bg_color(bg))){

	if(flags & PSC_NORM)
	  pico_set_normal_color();
	else if(flags & PSC_REV)
	  SetReverseColor();
    }

    return(cp);
}


int
pico_is_good_color(char *colorName)
{
    COLORREF	 cf;

    if(!struncmp(colorName, MATCH_NORM_COLOR, RGBLEN)
       || !struncmp(colorName, MATCH_NONE_COLOR, RGBLEN))
      return(TRUE);

    return(ConvertRGBString(colorName, &cf));
}


int
pico_set_fg_color(char *colorName)
{
    char fgbuf[MAXCLEN];

    FlushWriteAccum ();
    
    if(!struncmp(colorName, MATCH_NORM_COLOR, RGBLEN)){
	ConvertStringRGB(fgbuf,sizeof(fgbuf),gpTTYInfo->rgbFGColor);
	colorName = fgbuf;
    }
    else if(!struncmp(colorName, MATCH_NONE_COLOR, RGBLEN))
      return(TRUE);

    return(ConvertRGBString(colorName, &gpTTYInfo->curAttrib.rgbFG));
}


int
pico_set_bg_color(char *colorName)
{
    char bgbuf[MAXCLEN];

    FlushWriteAccum ();
    
    if(!struncmp(colorName, MATCH_NORM_COLOR, RGBLEN)){
	ConvertStringRGB(bgbuf,sizeof(bgbuf),gpTTYInfo->rgbBGColor);
	colorName = bgbuf;
    }
    else if(!struncmp(colorName, MATCH_NONE_COLOR, RGBLEN))
      return(TRUE);

    return(ConvertRGBString(colorName, &gpTTYInfo->curAttrib.rgbBG));
}


char *
pico_get_last_fg_color()
{
    return(NULL);
}


char *
pico_get_last_bg_color()
{
    return(NULL);
}


unsigned
pico_get_color_options()
{
    return((unsigned)0);
}


void
pico_set_color_options(unsigned int opts)
{
}


COLOR_PAIR *
pico_get_cur_color()
{
    char fgbuf[MAXCLEN], bgbuf[MAXCLEN];

    return(new_color_pair(ConvertStringRGB(fgbuf,sizeof(fgbuf),gpTTYInfo->curAttrib.rgbFG),
			  ConvertStringRGB(bgbuf,sizeof(bgbuf),gpTTYInfo->curAttrib.rgbBG)));
}


char *
mswin_rgbchoice(char *pOldRGB)
{
    CHOOSECOLOR cc;
    static COLORREF custColors[16] = {
	RGB(0,0,0),
	RGB(0,0,255),
	RGB(0,255,0),
	RGB(0,255,255),
	RGB(255,0,0),
	RGB(255,0,255),
	RGB(255,255,0),
	RGB(255,255,255),
	RGB(192,192,192),
	RGB(128,128,128),
	RGB(64,64,64)
    };

    memset(&cc, 0, sizeof(CHOOSECOLOR));

    cc.lStructSize  = sizeof(CHOOSECOLOR);
    cc.hwndOwner    = ghTTYWnd;
    cc.Flags	    = CC_ANYCOLOR;
    cc.lpCustColors = &custColors[0];

    if(pOldRGB){
	int i;

	ConvertRGBString (pOldRGB, &cc.rgbResult);
	cc.Flags |= CC_RGBINIT;

	for(i = 0; i < 11 && custColors[i] != cc.rgbResult; i++)
	  ;

	if(i == 11){
	    custColors[i] = cc.rgbResult;
	    cc.Flags |= CC_FULLOPEN;
	}
	
    }

    if(ChooseColor(&cc)){
	char rgbbuf[MAXCLEN], *p;

	ConvertStringRGB(rgbbuf, sizeof(rgbbuf), cc.rgbResult);
	if(p = MemAlloc(MAXCLEN * sizeof(char))){
	    strncpy(p, rgbbuf, MAXCLEN);
	    p[MAXCLEN-1] = '\0';
	    return(p);
	}
    }

    return(NULL);
}




/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *        Signal and alarm functions
 *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*
 * Provide a rough implementation of the SIGALRM and alarm functions
 */



#if 0
/*
 * Set a new handler for a signal.
 */
void (__cdecl * __cdecl signal (int sig,void (__cdecl *hndlr)(int)))(int)

{
    SignalType	oldValue;

    switch(sig) {
      case SIGALRM :
	oldValue = gSignalAlarm;
	gSignalAlarm = hndlr;
	break;

      case SIGHUP :
	oldValue = gSignalHUP;
	gSignalHUP = hndlr;

      default:
	/* All other's are always ignored. */
	oldValue = SIG_IGN;
	break;
    }

    return (oldValue);
}
#endif


/*
 * Set the alarm expiration time (in seconds)
 */
int
mswin_alarm (int seconds)
{
    int		prevtime;

    prevtime = gAlarmTimeout ? (gAlarmTimeout - (GetTickCount () / 1000)): 0;
    gAlarmTimeout = seconds ? (GetTickCount() / 1000) + seconds : 0;
    MyTimerSet ();
    return (prevtime);
}



/*
 * Deliver and clear the alarm.
 */
void
AlarmDeliver ()
{
    if (gSignalAlarm != SIG_DFL && gSignalAlarm != SIG_IGN) {
	/* Clear AlarmTimeout BEFORE calling handler.  handler may call back
	 * to reset timeout. */
	gAlarmTimeout = 0;
	MyTimerSet ();
	gSignalAlarm (SIGALRM);
    }
}



void
HUPDeliver ()
{
    if (gSignalHUP) {
	gSignalHUP (SIGHUP);
	exit (0);
    }
}




/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *        Printer font selection menu
 *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*
 * Set the print font to be the same as the window font.
 * Toggle setting.
 */
void
PrintFontSameAs (HWND hWnd)
{
    HDC			hDC;
    int			ppi;
    PTTYINFO		pTTYInfo;


    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
    if (pTTYInfo == NULL)
	return;

    if (gPrintFontSameAs) {
	
	/* No longer same as window font.  Use window font as starting point
	 * for new printer font.  User may later modify printer font. */
	hDC = GetDC (hWnd);
	ppi = GetDeviceCaps (hDC, LOGPIXELSY);
	ReleaseDC (ghTTYWnd, hDC);
	ExtractFontInfo(&pTTYInfo->lfTTYFont,
			gPrintFontName, sizeof(gPrintFontName)/sizeof(TCHAR),
			&gPrintFontSize,
			gPrintFontStyle, sizeof(gPrintFontStyle)/sizeof(TCHAR),
			ppi,
			gPrintFontCharSet, sizeof(gPrintFontCharSet)/sizeof(TCHAR));
	gPrintFontSameAs = FALSE;
    }
    else {
	
	/* Set to be same as the printer font.  Destroy printer font info
	 * and set "sameAs" flag to TRUE. */
	gPrintFontName[0] = '\0';
	gPrintFontSameAs = TRUE;
    }
    DidResize (gpTTYInfo);
}





void
PrintFontSelect (HWND hWnd)
{
    CHOOSEFONT		cfTTYFont;
    LOGFONT		newFont;
    DWORD		drc;
    int			ppi;
    HDC			hDC;



    hDC = GetDC (hWnd);
    ppi = GetDeviceCaps (hDC, LOGPIXELSY);
    ReleaseDC (ghTTYWnd, hDC);


    newFont.lfHeight =  -MulDiv (gPrintFontSize, ppi, 72);
    _tcsncpy(newFont.lfFaceName, gPrintFontName, LF_FACESIZE);
    newFont.lfFaceName[LF_FACESIZE-1] = 0;
    newFont.lfWeight = 0;
    if(_tcsstr(gPrintFontStyle, TEXT("bold")))
	newFont.lfWeight = FW_BOLD;

    newFont.lfItalic = 0;
    if(_tcsstr(gPrintFontStyle, TEXT("italic")))
	newFont.lfItalic = 1;

    newFont.lfWidth =          0;
    newFont.lfEscapement =     0;
    newFont.lfOrientation =    0;
    newFont.lfUnderline =      0;
    newFont.lfStrikeOut =      0;
    newFont.lfCharSet =        mswin_string2charsetid(gPrintFontCharSet);
    newFont.lfOutPrecision =   OUT_DEFAULT_PRECIS;
    newFont.lfClipPrecision =  CLIP_DEFAULT_PRECIS;
    newFont.lfQuality =        DEFAULT_QUALITY;
    newFont.lfPitchAndFamily = FIXED_PITCH;


    cfTTYFont.lStructSize    = sizeof (CHOOSEFONT);
    cfTTYFont.hwndOwner      = hWnd ;
    cfTTYFont.hDC            = NULL ;
    cfTTYFont.rgbColors      = 0;
    cfTTYFont.lpLogFont      = &newFont;
    cfTTYFont.Flags          = CF_BOTH | CF_FIXEDPITCHONLY |
	    CF_INITTOLOGFONTSTRUCT | CF_ANSIONLY |
	    CF_FORCEFONTEXIST | CF_LIMITSIZE;
    cfTTYFont.nSizeMin	     = FONT_MIN_SIZE;
    cfTTYFont.nSizeMax	     = FONT_MAX_SIZE;
    cfTTYFont.lCustData      = 0 ;
    cfTTYFont.lpfnHook       = NULL ;
    cfTTYFont.lpTemplateName = NULL ;
    cfTTYFont.hInstance      = GET_HINST (hWnd);


    if (ChooseFont (&cfTTYFont)) {
	ExtractFontInfo(&newFont,
			gPrintFontName, sizeof(gPrintFontName)/sizeof(TCHAR),
			&gPrintFontSize,
			gPrintFontStyle, sizeof(gPrintFontStyle)/sizeof(TCHAR),
			ppi,
			gPrintFontCharSet, sizeof(gPrintFontCharSet)/sizeof(TCHAR));
	DidResize (gpTTYInfo);
    }
    else
	/* So I can see with the debugger. */
	drc = CommDlgExtendedError();
}


void
ExtractFontInfo(LOGFONT *pFont, LPTSTR fontName, size_t nfontName,
		int *fontSize, LPTSTR fontStyle, size_t nfontStyle,
		int ppi, LPTSTR fontCharSet, size_t nfontCharSet)
{
    TCHAR		*sep[] = {TEXT(""), TEXT(", ")};
    int			iSep = 0;


    _tcsncpy(fontName, pFont->lfFaceName, nfontName);
    fontName[nfontName-1] = '\0';

    *fontStyle = '\0';
    if(pFont->lfWeight >= FW_BOLD) {
	_tcsncpy(fontStyle, TEXT("bold"), nfontStyle);
	fontStyle[nfontStyle-1] = '\0';
	iSep = 1;
    }

    if(pFont->lfItalic){
	_tcsncat(fontStyle, sep[iSep], nfontStyle - _tcslen(fontStyle));
	fontStyle[nfontStyle-1] = '\0';

	_tcsncat(fontStyle, TEXT("italic"), nfontStyle - _tcslen(fontStyle));
	fontStyle[nfontStyle-1] = '\0';
    }

    mswin_charsetid2string(fontCharSet, nfontCharSet, pFont->lfCharSet);

    if(fontSize)
      *fontSize = MulDiv(-pFont->lfHeight, 72, ppi);
}


LOCAL void
DidResize (PTTYINFO pTTYInfo)
{
    int			i;

    for (i = 0; i < RESIZE_CALLBACK_ARRAY_SIZE; ++i) {
	if (pTTYInfo->resizer[i] != NULL)
	    pTTYInfo->resizer[i] (pTTYInfo->actNRow, pTTYInfo->actNColumn);
    }
    /*
     * Put a null character into the input queue so that the data input
     * loops stop and return to their callers who will then re-calculate the
     * mouse regions so that the user can click on the new regions of the
     * screen and have the right thing happen.
     */
    CQAdd (NODATA, 0);
}


	

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *        Cut, Copy, and Paste operations
 *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


/*
 * Gets called right before the menu is displayed so we can make
 * any last minute adjustments.
 */
LOCAL void
UpdateMenu (HWND hWnd)
{
    HMENU		hMenu;
    PTTYINFO		pTTYInfo;
    int			i;
#ifdef	ACCELERATORS
    UINT		fAccel = EM_NONE;
#endif

    pTTYInfo = (PTTYINFO) MyGetWindowLongPtr (hWnd, GWL_PTTYINFO);
    if (pTTYInfo == NULL)
	return;

    hMenu = GetMenu (hWnd);
    if (hMenu == NULL)
	return;

    if (ghPaste) {
	/* Currently pasting so disable paste and enable cancel paste. */
	EnableMenuItem (hMenu, IDM_EDIT_PASTE, MF_BYCOMMAND|MF_GRAYED);
	EnableMenuItem (hMenu, IDM_EDIT_CANCEL_PASTE, MF_BYCOMMAND|MF_ENABLED);
#ifdef	ACCELERATORS
	fAccel |= (EM_PST | EM_PST_ABORT);
#endif
    }
    else {
	/*
	 * Not pasting.  If text is available on clipboard and we are
	 * at a place where we can paste, enable past menu option.
	 */
	if (IsClipboardFormatAvailable (CF_UNICODETEXT) && gPasteEnabled){
	    EnableMenuItem (hMenu, IDM_EDIT_PASTE, MF_BYCOMMAND|MF_ENABLED);
#ifdef	ACCELERATORS
	    fAccel |= EM_PST;
#endif
	}
	else
	    EnableMenuItem (hMenu, IDM_EDIT_PASTE, MF_BYCOMMAND|MF_GRAYED);

	EnableMenuItem (hMenu, IDM_EDIT_CANCEL_PASTE, MF_BYCOMMAND|MF_GRAYED);
    }

    if (SelAvailable ()) {
	EnableMenuItem (hMenu, IDM_EDIT_CUT, MF_BYCOMMAND|MF_GRAYED);
	EnableMenuItem (hMenu, IDM_EDIT_COPY, MF_BYCOMMAND|MF_ENABLED);
	EnableMenuItem (hMenu, IDM_EDIT_COPY_APPEND, MF_BYCOMMAND|MF_ENABLED);
#ifdef	ACCELERATORS
	fAccel |= (EM_CP | EM_CP_APPEND);
#endif
    }
    else {
	if (gAllowCut){
	    EnableMenuItem (hMenu, IDM_EDIT_CUT, MF_BYCOMMAND | MF_ENABLED);
#ifdef	ACCELERATORS
	    fAccel |= EM_CUT;
#endif
	}
        else
	    EnableMenuItem (hMenu, IDM_EDIT_CUT, MF_BYCOMMAND | MF_GRAYED);

        if (gAllowCopy) {
	    EnableMenuItem (hMenu, IDM_EDIT_COPY, MF_BYCOMMAND | MF_ENABLED);
	    EnableMenuItem (hMenu, IDM_EDIT_COPY_APPEND,
			    MF_BYCOMMAND | MF_ENABLED);
#ifdef	ACCELERATORS
	    fAccel |= (EM_CP | EM_CP_APPEND);
#endif
	}
        else {
	    EnableMenuItem (hMenu, IDM_EDIT_COPY, MF_BYCOMMAND | MF_GRAYED);
	    EnableMenuItem (hMenu, IDM_EDIT_COPY_APPEND,
				MF_BYCOMMAND | MF_GRAYED);	
	}
    }

    /*
     * Set up Font selection menu
     */
    if (gPrintFontName[0] == '\0') {
	CheckMenuItem (hMenu, IDM_OPT_FONTSAMEAS, MF_BYCOMMAND | MF_CHECKED);
	EnableMenuItem (hMenu, IDM_OPT_SETPRINTFONT,
						MF_BYCOMMAND | MF_GRAYED);
    }
    else {
	CheckMenuItem (hMenu, IDM_OPT_FONTSAMEAS,
						MF_BYCOMMAND | MF_UNCHECKED);
	EnableMenuItem (hMenu, IDM_OPT_SETPRINTFONT,
						MF_BYCOMMAND | MF_ENABLED);
    }

    /*
     * Setup Caret selection menu
     */
    EnableMenuItem (hMenu, IDM_OPT_CARETBLOCK, MF_BYCOMMAND | MF_ENABLED);
    EnableMenuItem (hMenu, IDM_OPT_CARETSMALLBLOCK, MF_BYCOMMAND | MF_ENABLED);
    EnableMenuItem (hMenu, IDM_OPT_CARETHBAR, MF_BYCOMMAND | MF_ENABLED);
    EnableMenuItem (hMenu, IDM_OPT_CARETVBAR, MF_BYCOMMAND | MF_ENABLED);
    CheckMenuRadioItem(hMenu, IDM_OPT_CARETBLOCK, IDM_OPT_CARETVBAR,
		       IDM_OPT_CARETBLOCK + pTTYInfo->cCaretStyle,
		       MF_BYCOMMAND);

    /*
     * Check toolbar menu.
     */
    EnableMenuItem (hMenu, IDM_OPT_TOOLBAR, MF_BYCOMMAND | MF_ENABLED);
    CheckMenuItem (hMenu, IDM_OPT_TOOLBAR, MF_BYCOMMAND |
	    (pTTYInfo->toolBarSize > 0 ? MF_CHECKED : MF_UNCHECKED));
    EnableMenuItem (hMenu, IDM_OPT_TOOLBARPOS, MF_BYCOMMAND | MF_ENABLED);
    CheckMenuItem (hMenu, IDM_OPT_TOOLBARPOS, MF_BYCOMMAND |
	    (pTTYInfo->toolBarTop > 0 ? MF_CHECKED : MF_UNCHECKED));



    /*
     * Check the dialogs menu.
     */
    /* xxx EnableMenuItem (hMenu, IDM_OPT_USEDIALOGS, MF_BYCOMMAND | MF_ENABLED);*/
    CheckMenuItem (hMenu, IDM_OPT_USEDIALOGS, MF_BYCOMMAND |
	    (gfUseDialogs ? MF_CHECKED : MF_UNCHECKED));

    /*
     * Enable the Erase Credentials menu
     */
    EnableMenuItem (hMenu, IDM_OPT_ERASE_CREDENTIALS,
		    MF_BYCOMMAND | (gEraseCredsCallback ? MF_ENABLED : MF_GRAYED));
    
    /*
     * Enable the View in New Window menu item
     */
    EnableMenuItem (hMenu, IDM_MI_VIEWINWIND,
		    MF_BYCOMMAND | (gViewInWindCallback ? MF_ENABLED : MF_GRAYED));
    
#ifdef ACCELERATORS_OPT
    CheckMenuItem (hMenu, IDM_OPT_USEACCEL, MF_BYCOMMAND |
	    (pTTYInfo->hAccel ? MF_CHECKED : MF_UNCHECKED));
#endif

    /*
     * Setup the sort menu...
     */
    if(gSortCallback){
	i = (*gSortCallback)(0, 0);

	/* NOTE: this func's args are dependent on definition order
	 *       in resource.h
	 */
	CheckMenuRadioItem(hMenu, IDM_MI_SORTSUBJECT, IDM_MI_SORTTHREAD,
			   IDM_MI_SORTSUBJECT + (i & 0x00ff), MF_BYCOMMAND);
	CheckMenuItem(hMenu, IDM_MI_SORTREVERSE,
		      MF_BYCOMMAND|((i & 0x0100) ? MF_CHECKED : MF_UNCHECKED));
    }
    else{
	CheckMenuRadioItem(hMenu, IDM_MI_SORTSUBJECT, IDM_MI_SORTTHREAD,
			   IDM_MI_SORTARRIVAL, MF_BYCOMMAND);
	CheckMenuItem(hMenu, IDM_MI_SORTREVERSE, MF_BYCOMMAND | MF_UNCHECKED);
    }

    /*
     * Setup the flag menu...
     */
    if(gFlagCallback){
	int flags = (*gFlagCallback)(0, 0);
	for(i = IDM_MI_FLAGIMPORTANT; i <= IDM_MI_FLAGDELETED; i++)
	  CheckMenuItem(hMenu, i, MF_BYCOMMAND
			| (((flags >> (i - IDM_MI_FLAGIMPORTANT)) & 0x0001)
			     ? MF_CHECKED : MF_UNCHECKED));
    }

    /*
     *
     */
    if(gHdrCallback){
	i = (*gHdrCallback)(0, 0);
	CheckMenuItem(hMenu, IDM_MI_HDRMODE,
		      MF_BYCOMMAND|((i != 0) ? MF_CHECKED : MF_UNCHECKED));
    }

    /*
     *
     */
    if(gZoomCallback){
	i = (*gZoomCallback)(0, 0);
	CheckMenuItem(hMenu, IDM_MI_ZOOM,
		      MF_BYCOMMAND | (i ? MF_CHECKED : MF_UNCHECKED));
    }
    /*
     * Set up command menu.
     */
    if (!pTTYInfo->menuItemsCurrent) {
	for (i = 0; i < KS_COUNT; ++i)
	  if(i + KS_RANGESTART != KS_GENERALHELP)
	    EnableMenuItem (hMenu, i + KS_RANGESTART,
			    MF_BYCOMMAND | ((pTTYInfo->menuItems[i].miActive)
					      ? MF_ENABLED : MF_GRAYED));

	/*
	 * Special command-specific knowledge here
	 */
	for(i = IDM_MI_SORTSUBJECT; i <= IDM_MI_SORTREVERSE; i++)
	  EnableMenuItem (hMenu, i,
		      MF_BYCOMMAND
		       | ((pTTYInfo->menuItems[KS_SORT-KS_RANGESTART].miActive)
			    ? MF_ENABLED : MF_GRAYED));

	for(i = IDM_MI_FLAGIMPORTANT; i <= IDM_MI_FLAGDELETED; i++)
	  EnableMenuItem (hMenu, i,
		  MF_BYCOMMAND
		  | ((pTTYInfo->menuItems[KS_FLAG - KS_RANGESTART].miActive)
		     ? MF_ENABLED : MF_GRAYED));

    }

    /*
     * deal with any callback state dependent enabling
     */
    if(pTTYInfo->menuItems[IDM_MI_APPLY - KS_RANGESTART].miActive)
      EnableMenuItem(hMenu, IDM_MI_APPLY,
		     MF_BYCOMMAND | ((gSelectedCallback
				      && (*gSelectedCallback)(0, 0))
					? MF_ENABLED : MF_GRAYED));

    if(pTTYInfo->menuItems[IDM_MI_ZOOM - KS_RANGESTART].miActive)
      EnableMenuItem(hMenu, IDM_MI_ZOOM,
		     MF_BYCOMMAND | ((gSelectedCallback
				      && (*gSelectedCallback)(0, 0))
					? MF_ENABLED : MF_GRAYED));

#ifdef	ACCELERATORS
    if(pTTYInfo->menuItems[KS_WHEREIS - KS_RANGESTART].miActive)
      fAccel |= EM_FIND;

    AccelManage (hWnd, fAccel);
#endif

    pTTYInfo->menuItemsCurrent = TRUE;
}



/*
 * Cut region to kill buffer.
 */
LOCAL void
EditCut (void)
{
    HANDLE		hCB;

    if(gCopyCutFunction == (getc_t)kremove){
	hCB = GlobalAlloc (GMEM_MOVEABLE, 0);
	if (hCB != NULL) {
	    kdelete();		/* Clear current kill buffer. */
	    killregion (1, 0);	/* Kill Region (and copy to clipboard). */
	    update ();		/* And update the screen */
        }
    }
}



/*
 * This function copies the kill buffer to the window's clip board.
 * (actually, it can copy any buffer for which a copyfunc is provided).
 * Called from ldelete().
 */
void
mswin_killbuftoclip (getc_t copyfunc)
{
    HANDLE		hCB;
    getc_t		oldfunc;

    /* Save old copy function. */
    oldfunc = gCopyCutFunction;
    gCopyCutFunction = copyfunc;

    /* Allocate clip buffer. */
    hCB = GlobalAlloc (GMEM_MOVEABLE, 0);
    if (hCB != NULL) {
	EditDoCopyData (hCB, 0);
    }

    /* restore copy function. */
    gCopyCutFunction = oldfunc;
}
	


/*
 * Copy region to kill buffer.
 */
LOCAL void
EditCopy (void)
{
    HANDLE		hCB;

    if (SelAvailable()) {
	/* This is a copy of the windows selection. */
	hCB = GlobalAlloc (GMEM_MOVEABLE, 0);
	if (hCB != NULL)
	    SelDoCopy (hCB, 0);
    }
    else {
	
	/* Otherwise, it's a Pico/Pine copy. */
	if(gCopyCutFunction == (getc_t)kremove){
	    kdelete();		/* Clear current kill buffer. */
	    copyregion (1, 0);
	}

	hCB = GlobalAlloc (GMEM_MOVEABLE, 0);
	if (hCB != NULL)
	    EditDoCopyData (hCB, 0);
    }
}



/*
 * Called in responce to "Copy Append" menu command, when there is an active
 * Windows selection on the screen.
 */
LOCAL void
EditCopyAppend (void)
{
    HANDLE	hCB;
    HANDLE	hMyCopy;
    TCHAR	*pCB;
    TCHAR	*pMyCopy;
    size_t	cbSize = 0;

    /* Attempt to copy clipboard data to my own handle. */
    hMyCopy = NULL;
    if (OpenClipboard (ghTTYWnd)) {		/* And can get clipboard. */
	hCB = GetClipboardData (CF_UNICODETEXT);
	if (hCB != NULL) {			/* And can get data. */
	    pCB = GlobalLock (hCB);
	    cbSize = _tcslen (pCB);		/* It's a null term string. */
	    hMyCopy = GlobalAlloc (GMEM_MOVEABLE, (cbSize+1)*sizeof(*pCB));
	    if (hMyCopy != NULL) {		/* And can get memory. */
		pMyCopy = GlobalLock (hMyCopy);
		if (pMyCopy != NULL) {
		    memcpy (pMyCopy, pCB, cbSize*sizeof(*pCB));  /* Copy data. */
		    GlobalUnlock (hMyCopy);
		}
		else {
		    GlobalFree (hMyCopy);
		    hMyCopy = NULL;
		}
	    }
	    GlobalUnlock (hCB);
	}					/* GetClipboardData. */
	CloseClipboard ();
    }					/* OpenClipboard. */



    /* Now, if I got a copy, append current selection to that
     * and stuff it back into the clipboard. */
    if (hMyCopy != NULL) {
	if (SelAvailable ()) {
	    SelDoCopy (hMyCopy, (DWORD)cbSize);
	}
	else {
	    if(gCopyCutFunction == (getc_t)kremove) {
		kdelete();		/* Clear current kill buffer. */
		copyregion (1, 0);
	    }
	    EditDoCopyData (hMyCopy, (DWORD)cbSize);
	}
    }
}


/*
 * Copy data from the kill buffer to the clipboard.  Handle LF->CRLF
 * translation if necessary.
 */
LOCAL void
EditDoCopyData (HANDLE hCB, DWORD lenCB)
{
    TCHAR		*pCB;
    TCHAR		*p;
    long		c; /* would be TCHAR but needs -1 retval from callback */
    TCHAR		lastc = (TCHAR)'\0';
    DWORD		cbSize;			/* Allocated size of hCB. */
    DWORD		i;
#define	BUF_INC	4096

    if (gCopyCutFunction != NULL) {		/* If there really is data. */
	if (OpenClipboard (ghTTYWnd)) {		/* ...and we get the CB. */
	    if (EmptyClipboard ()) {		/* ...and clear previous CB.*/
		pCB = GlobalLock (hCB);
		p = pCB + lenCB;
		cbSize = lenCB;
		/* Copy it. (BUG: change int arg) */
		for(i = 0L; (c = (*gCopyCutFunction)((int)i)) != -1; i++){
		    /*
		     * Rather than fix every function that might
		     * get called for character retrieval to supply
		     * CRLF EOLs, let's just fix it here.  The downside
		     * is a much slower copy for large buffers, but
		     * hey, what do they want?
		     */
		    if(lenCB + 2L >= cbSize){
			cbSize += BUF_INC;
			GlobalUnlock (hCB);
			hCB = GlobalReAlloc (hCB, cbSize*sizeof(TCHAR), GMEM_MOVEABLE);
			if (hCB == NULL)
			  return;

			pCB = GlobalLock (hCB);
			p = pCB + lenCB;
		    }

		    if(c == (TCHAR)ASCII_LF && lastc != (TCHAR)ASCII_CR) {
			*p++ = (TCHAR)ASCII_CR;	/* insert CR before LF */
		      lenCB++;
		    }

		    *p++ = lastc = (TCHAR)c;
		    lenCB++;
		}

		/* Only if we got some data. */
		if (lenCB > 0) {
		    *p = (TCHAR)'\0';
		    GlobalUnlock (hCB);

		    if (SetClipboardData (CF_UNICODETEXT, hCB) == NULL)
		      /* Failed!  Free the data. */
		      GlobalFree (hCB);
		}
		else {
		    /* There was no data copied. */
		    GlobalUnlock (hCB);
		    GlobalFree (hCB);
		}
	    }
	    CloseClipboard ();
        }
    }
}


/*
 * Get a handle to the current (text) clipboard and make my own copy.
 * Keep my copy locked because I'll be using it to read bytes from.
 */
LOCAL void
EditPaste (void)
{
    HANDLE	hCB;
    LPTSTR	pCB;
    LPTSTR	pPaste;
    size_t	cbSize;

    if (ghPaste == NULL) {		/* If we are not already pasting. */
	if (OpenClipboard (ghTTYWnd)) {		/* And can get clipboard. */
	    hCB = GetClipboardData (CF_UNICODETEXT);
	    if (hCB != NULL) {			/* And can get data. */
		pCB = GlobalLock (hCB);
		cbSize = _tcslen (pCB);		/* It's a null term string. */
		ghPaste = GlobalAlloc (GMEM_MOVEABLE, (cbSize+1)*sizeof(TCHAR));
		if (ghPaste != NULL) {		/* And can get memory. */
		    gpPasteNext = GlobalLock (ghPaste);
		    memcpy (gpPasteNext, pCB, (cbSize+1)*sizeof(TCHAR));  /* Copy data. */
		    /* Keep ghPaste locked. */

		    /*
		     * If we're paste is enabled but limited to the first
		     * line of the clipboard, prune the paste buffer...
		     */
		    if(gPasteEnabled == MSWIN_PASTE_LINE
		       && (pPaste = _tcschr(gpPasteNext, (TCHAR)ASCII_CR))){
			*pPaste = (TCHAR)'\0';
			cbSize  = _tcslen(gpPasteNext);
		    }

		    /*
		     * If there is a selection (gCopyCutFunction != NULL)
		     * then delete it so that it will be replaced by
		     * the pasted text.
		     */
		    if (gCopyCutFunction != NULL)
			deleteregion (1, 0);

		    gPasteBytesRemain = cbSize;
		    gPasteWasCR = FALSE;
#ifdef FDEBUG
		    if (mswin_debug > 8)
			fprintf (mswin_debugfile, "EditPaste::  Paste %d bytes\n",
				    gPasteBytesRemain);
#endif
		}
		GlobalUnlock (hCB);
	    }
	    CloseClipboard ();
        }
    }
}






/*
 * Cancel an active paste operation.
 */
LOCAL void
EditCancelPaste (void)
{
    if (ghPaste != NULL) {	/* Must be pasting. */
	GlobalUnlock (ghPaste);	/* Then Unlock... */
	GlobalFree (ghPaste);	/* ...and free the paste buffer. */
	ghPaste = NULL;		/* Indicates no paste data. */
	gpPasteNext = NULL;		/* Just being tidy. */
	gPasteBytesRemain = 0;	/* ditto. */
#ifdef FDEBUG
	if (mswin_debug > 8)
	    fprintf (mswin_debugfile, "EditCancelPaste::  Free Paste Data\n");
#endif
    }
}


/*
 * Get the next byte from the paste buffer.  If all bytes have been
 * retreived, free the paste buffer.
 * Map all CRLF sequence to a single CR.
 */
LOCAL UCS
EditPasteGet (void)
{
    UCS  b = NODATA;

    if (ghPaste != NULL) {		/* ghPaste tells if we are pasting. */
	if (gPasteBytesRemain > 0) {	/* Just in case... */
					/* Get one byte and move pointer. */
	    b = (TCHAR) *gpPasteNext++;
	    --gPasteBytesRemain;	/*    one less. */
	    if (gPasteWasCR && b == (TCHAR)ASCII_LF) {
		if (gPasteBytesRemain) {
					/* Skip of LF. */
		    b = (TCHAR) *gpPasteNext++;
		    --gPasteBytesRemain;
	        }
		else
		    b = NODATA;  /* Ignore last LF. */
	    }
	    gPasteWasCR = (b == (TCHAR)ASCII_CR);
#ifdef FDEBUG
	    if (mswin_debug > 8)
		fprintf (mswin_debugfile, "EditPasteGet::  char %c, gPasteWasCR %d, gPasteBytesRemain %d\n",
			b, gPasteWasCR, gPasteBytesRemain);
#endif
        }
	if (gPasteBytesRemain <= 0) {	/* All Done? */
	    GlobalUnlock (ghPaste);	/* Then Unlock... */
	    GlobalFree (ghPaste);	/* ...and free the paste buffer. */
	    ghPaste = NULL;		/* Indicates no paste data. */
	    gpPasteNext = NULL;		/* Just being tidy. */
	    gPasteBytesRemain = 0;	/* ditto. */
#ifdef FDEBUG
	    if (mswin_debug > 8)
		fprintf (mswin_debugfile, "EditPasteGet::  Free Paste Data\n");
#endif
        }
    }

    if(b < ' ') {
        b += '@';
        b |= CTRL;
    }

    return (b);
}


/*
 * Return true if Paste data is available.  If gpPaste != NULL then there
 * is paste data.
 */
LOCAL BOOL
EditPasteAvailable (void)
{
    return (ghPaste != NULL);
}



/*
 * Select everything in the buffer
 */
LOCAL void
EditSelectAll()
{
    if(ComposerEditing){
    }
    else{
	gotobob(0, 1);
	setmark(0, 1);
	gotoeob(0, 1);
	update ();		/* And update the screen */
    }
}


LOCAL void
SortHandler(int order, int reverse)
{
    int old = (*gSortCallback)(0, 0);

    if(order < 0){
	old ^= 0x0100;		/* flip reverse bit */
	(*gSortCallback)(1, old);
    }
    else
      (*gSortCallback)(1, order | (old & 0x0100));
}


LOCAL void
FlagHandler(int index, int args)
{
    if(gFlagCallback)
      (void) (*gFlagCallback)(index + 1, 0L);
}


LOCAL void
MSWHelpShow (cbstr_t fpHelpCallback)
{
    if (fpHelpCallback != NULL){
	char title[256], *help;

	if(help = (*fpHelpCallback) (title))
	  mswin_displaytext (title, help, strlen(help), NULL, NULL, 0);
    }
}





/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *                 Adjust the timer frequency as needed.
 *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

LOCAL void
MyTimerSet (void)
{
    UINT	period;
    /* Decide on period to use. */
    if (gAllowMouseTrack)
	period = MY_TIMER_EXCEEDINGLY_SHORT_PERIOD;
    else
	period = my_timer_period;

    if (period != gTimerCurrentPeriod) {
	if (SetTimer (ghTTYWnd, MY_TIMER_ID, period, NULL) == 0)
		MessageBox (ghTTYWnd, TIMER_FAIL_MESSAGE, NULL,
			     MB_OK | MB_ICONINFORMATION);
        else
	    gTimerCurrentPeriod = period;
    }
}





void
mswin_setperiodiccallback (cbvoid_t periodiccb, long period)
{
    if (periodiccb != NULL && period > 0) {
	gPeriodicCallback = periodiccb;
	gPeriodicCBTime = period;
	gPeriodicCBTimeout = GetTickCount () / 1000 + gPeriodicCBTime;
    }
    else {
	gPeriodicCallback = NULL;
    }
}

/*
 * Structure for variables used by mswin_exec_and_wait which need to be
 *  freed in multiple places.
 */
typedef struct MSWIN_EXEC_DATA {
    HANDLE              infd;
    HANDLE              outfd;
    LPTSTR              lptstr_whatsit;
    LPTSTR              lptstr_command;
    LPTSTR              lptstr_infile;
    LPTSTR              lptstr_outfile;
    MSWIN_TEXTWINDOW    *mswin_tw;
} MSWIN_EXEC_DATA;

LOCAL void
mswin_exec_data_init(MSWIN_EXEC_DATA *exec_data)
{
    memset(exec_data, 0, sizeof(MSWIN_EXEC_DATA));
    exec_data->infd = INVALID_HANDLE_VALUE;
    exec_data->outfd = INVALID_HANDLE_VALUE;
}

LOCAL void
mswin_exec_data_free(MSWIN_EXEC_DATA *exec_data, BOOL delete_outfile)
{
    if(exec_data->infd != INVALID_HANDLE_VALUE)
      CloseHandle(exec_data->infd);

    if(exec_data->outfd != INVALID_HANDLE_VALUE) {
      CloseHandle(exec_data->outfd);
      if(delete_outfile)
        _tunlink(exec_data->lptstr_outfile);
    }

    if(exec_data->lptstr_infile)
      fs_give((void **) &exec_data->lptstr_infile);
    if(exec_data->lptstr_outfile)
      fs_give((void **) &exec_data->lptstr_outfile);
    if(exec_data->lptstr_whatsit)
      fs_give((void **) &exec_data->lptstr_whatsit);
    if(exec_data->lptstr_command)
      fs_give((void **) &exec_data->lptstr_command);

    if(exec_data->mswin_tw) {
        /*
         * Set the out_file is zero. We don't need mswin_tw
         *   to save the file anymore since we're bailing.
         */
        exec_data->mswin_tw->out_file = NULL;

        /*
         * If the window is still open, then set the id to 0 so
         *  mswin_tw_close_callback() will free the memory whenever
         *  the window closes. Otherwise free it now.
         */
        if(exec_data->mswin_tw->hwnd)
            exec_data->mswin_tw->id = 0;
        else
            MemFree(exec_data->mswin_tw);
    }
}

/*
 * Execute command and wait for the child to exit
 *
 * whatsit - description of reason exec being called
 * command - command to run
 * infile - name of file to pass as stdin
 * outfile - name of file to pass as stdout
 * exit_val - where to store return value of the process
 * mswe_flags -
 *   MSWIN_EAW_CAPT_STDERR - capture stderr along with stdout
 *   MSWIN_EAW_CTRL_C_CANCELS - if user presses ctrl-c, detach child
 * Returns: 0, successfully completed program
 *         -1, errors occurred
 *         -2, user chose to stop waiting for program before it finished
 */
int
mswin_exec_and_wait (char *utf8_whatsit, char *utf8_command,
		     char *utf8_infile, char *utf8_outfile,
		     int *exit_val, unsigned mswe_flags)
{
    MEvent		mouse;
    BOOL		brc;
    int			rc;
    TCHAR		waitingFor[256];
    PROCESS_INFORMATION	proc_info;
    DWORD		exit_code;
    MSWIN_EXEC_DATA     exec_data;
#ifdef ALTED_DOT
    BOOL                b_use_mswin_tw;
#endif

    mswin_exec_data_init(&exec_data);

    memset(&proc_info, 0, sizeof(proc_info));

    mswin_flush ();

    exec_data.lptstr_infile = utf8_infile ? utf8_to_lptstr(utf8_infile) : NULL;
    exec_data.lptstr_outfile = utf8_outfile ? utf8_to_lptstr(utf8_outfile) : NULL;

    exec_data.lptstr_command = utf8_to_lptstr(utf8_command);
    exec_data.lptstr_whatsit = utf8_to_lptstr(utf8_whatsit);

#ifdef ALTED_DOT
    /* If the command is '.', then use mswin_tw to open the file. */
    b_use_mswin_tw = utf8_command &&
        utf8_command[0] == '.' && utf8_command[1] == '\0';

    if(b_use_mswin_tw) {

        proc_info.hThread = INVALID_HANDLE_VALUE;
        proc_info.hProcess = INVALID_HANDLE_VALUE;

        exec_data.mswin_tw = mswin_tw_displaytext_lptstr(
            exec_data.lptstr_whatsit, exec_data.lptstr_infile, 4, NULL,
	    exec_data.mswin_tw, MSWIN_DT_FILLFROMFILE);

        if(exec_data.mswin_tw) {
            mswin_set_readonly(exec_data.mswin_tw, FALSE);

            /* Tell mswin_tw to write the edit contents to this file. */
            exec_data.mswin_tw->out_file = exec_data.lptstr_outfile;

            /* Make sure mswin_tw isn't freed behind our back. */
            exec_data.mswin_tw->id = (UINT)-1;
            brc = TRUE;
        }
        else {
            brc = FALSE;
        }
    }
    else
#endif /* ALTED_DOT */
    {
    SECURITY_ATTRIBUTES atts;
    STARTUPINFO         start_info;

    memset(&atts, 0, sizeof(atts));
    memset(&start_info, 0, sizeof(start_info));

    /* set file attributes of temp files*/
    atts.nLength = sizeof(SECURITY_ATTRIBUTES);
    atts.bInheritHandle = TRUE;
    atts.lpSecurityDescriptor = NULL;

    /* open files if asked for */
    if(utf8_infile
           && ((exec_data.infd = CreateFile(exec_data.lptstr_infile,
			      GENERIC_READ, 0, &atts,
			      OPEN_EXISTING, FILE_ATTRIBUTE_READONLY, NULL))
	   == INVALID_HANDLE_VALUE)){

	mswin_exec_data_free(&exec_data, TRUE);
	return(-1);
    }

    if(utf8_outfile
           && ((exec_data.outfd = CreateFile(exec_data.lptstr_outfile,
			       GENERIC_WRITE, 0, &atts,
			       OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL))
	   == INVALID_HANDLE_VALUE)){

	mswin_exec_data_free(&exec_data, TRUE);
	return(-1);
    }

    start_info.dwFlags	= STARTF_FORCEONFEEDBACK | STARTF_USESHOWWINDOW;
    start_info.wShowWindow  = (utf8_infile || utf8_outfile) ? SW_SHOWMINNOACTIVE : SW_SHOWNA;

    /* set up i/o redirection */
    if(utf8_infile)
      start_info.hStdInput = exec_data.infd;
    if(utf8_outfile)
      start_info.hStdOutput = exec_data.outfd;
    if(utf8_outfile && (mswe_flags & MSWIN_EAW_CAPT_STDERR))
      start_info.hStdError = exec_data.outfd;
    if(utf8_infile || utf8_outfile)
      start_info.dwFlags |= STARTF_USESTDHANDLES;

    brc = CreateProcess(NULL, exec_data.lptstr_command, NULL, NULL,
		    (utf8_infile || utf8_outfile) ? TRUE : FALSE,
		 CREATE_NEW_CONSOLE | CREATE_NEW_PROCESS_GROUP,
		    NULL, NULL, &start_info, &proc_info);
    }

    if(brc) {
	_sntprintf(waitingFor, sizeof(waitingFor)/sizeof(TCHAR),
		   TEXT("%s is currently waiting for the %s (%s) to complete.  Click \"Cancel\" to stop waiting, or \"OK\" to continue waiting."),
		 gszAppName, exec_data.lptstr_whatsit, exec_data.lptstr_command);

        if(proc_info.hThread != INVALID_HANDLE_VALUE) {
	    /* Don't need the thread handle, close it now. */
	    CloseHandle (proc_info.hThread);
        }

	/*
	 * Go into holding pattern until the other application terminates
	 * or we are told to stop waiting.
	 */
	while(TRUE){

#ifdef ALTED_DOT
            if(b_use_mswin_tw)
            {
                if(!exec_data.mswin_tw)
                    break;

                exit_code = exec_data.mswin_tw->hwnd && exec_data.mswin_tw->out_file ?
                    STILL_ACTIVE : 0;
            }
            else
#endif /* ALTED_DOT */
            {
                if(GetExitCodeProcess(proc_info.hProcess, &exit_code) == FALSE)
                    break;
            }

	    if(exit_code == STILL_ACTIVE){
		rc = mswin_getc();
		brc = mswin_getmouseevent (&mouse);

		if (rc != NODATA ||
		    (brc && mouse.event == M_EVENT_DOWN)) {
		    if(mswe_flags & MSWIN_EAW_CTRL_C_CANCELS){
			if(rc == (CTRL|'C'))
			  rc = IDCANCEL;
		    }
		    else{
			rc = MessageBox (ghTTYWnd, waitingFor, exec_data.lptstr_whatsit,
					 MB_ICONSTOP | MB_OKCANCEL);
		    }
		    SelClear ();
		    if (rc == IDCANCEL){
			/* terminate message to child ? */
			mswin_exec_data_free(&exec_data, TRUE);
			return (-2);
		    }
		}
	    }
	    else{
                if(proc_info.hProcess != INVALID_HANDLE_VALUE) {
		    /* do something about child's exit status */
		    CloseHandle (proc_info.hProcess);
                }
		if(exit_val)
		  *exit_val = exit_code;
		break;
	    }
	}

	if (gpTTYInfo->fMinimized)
	  ShowWindow (ghTTYWnd, SW_SHOWNORMAL);

        /*
         * If we're using a mswin_tw and we're not capturing the output, we
         *  just bailed immediately. If that's the case, don't bring the main
         *  window up over the top of the textwindow we just brought up.
         */
#ifdef ALTED_DOT
        if(!b_use_mswin_tw || !exec_data.mswin_tw || exec_data.mswin_tw->out_file)
#endif /* ALTED_DOT */
	  BringWindowToTop (ghTTYWnd);

        mswin_exec_data_free(&exec_data, FALSE);
	return (0);
    }
    else{
        mswin_exec_data_free(&exec_data, TRUE);
	return((rc = (int) GetLastError()) ? rc : -1);		/* hack */
    }

    /* NOTREACHED */
    return(-1);
}


int
mswin_shell_exec(char *command_utf8, HINSTANCE *pChildProc)
{
    int		      quoted = 0;
    SHELLEXECUTEINFO  shell_info;
    LPTSTR            command_lpt, free_command_lpt;
    LPTSTR            p, q, parm = NULL;
    TCHAR             buf[1024];

    if(!command_utf8)
      return(-1);

    free_command_lpt = command_lpt = utf8_to_lptstr(command_utf8);
    if(!command_lpt)
      return(-1);

    mswin_flush ();

    /*
     * Pick first arg apart by whitespace until what's to the left
     * is no longer a valid path/file.  Everything else is then an
     * command line arg...
     */
    if(*(p = command_lpt) == '\"'){
	p = ++command_lpt;				/* don't include quote */
	quoted++;
    }

    q = buf;
    while(1)
      if(!quoted && _istspace(*p)){
	  char *buf_utf8;

	  *q = '\0';
	  buf_utf8 = lptstr_to_utf8(buf);
	  if(*buf == '*' || (buf_utf8 && fexist(buf_utf8, "x", (off_t *) NULL) == FIOSUC)){
	      parm = p;
	      if(buf_utf8)
	        fs_give((void **) &buf_utf8);

	      break;
	  }

	  if(buf_utf8)
	    fs_give((void **) &buf_utf8);
      }
      else if(quoted && *p == '\"'){
	  parm = p;
	  break;
      }
      else if(!(*q++ = *p++)){
	  parm = p - 1;
	  break;
      }

    if(*command_lpt && parm){
	do
	  *parm++ = '\0';
	while(*parm && _istspace((unsigned char) *parm));


	/*
	 * HACK -- since star is very unlikely to actually appear
	 * in a command name thats launched via a shell command line,
	 * a leading one indicates special handling.
	 */
	if(command_lpt[0] == '*'){
	    if(!_tcsncmp(command_lpt + 1, TEXT("Shell*"), 8)){
		/* Leave it to ShellExecute magic to "open" the thing */
		command_lpt = parm;
		parm = NULL;
	    }
	}
    }
    else{
      if(free_command_lpt)
        fs_give((void **) &free_command_lpt);

      return(-1);
    }

    memset(&shell_info, 0, sizeof(SHELLEXECUTEINFO));
    shell_info.cbSize = sizeof(SHELLEXECUTEINFO);
    shell_info.fMask = SEE_MASK_DOENVSUBST
		      | SEE_MASK_NOCLOSEPROCESS
		      | SEE_MASK_FLAG_DDEWAIT;
    shell_info.hwnd = ghTTYWnd;
    shell_info.lpFile = command_lpt;
    shell_info.lpParameters = parm;
    shell_info.lpDirectory = NULL;	/* default is current directory */
    shell_info.nShow = SW_SHOWNORMAL;

    ShellExecuteEx(&shell_info);

    if((int)(LONG_PTR)shell_info.hInstApp > 32){
	if(pChildProc)
	  *pChildProc = shell_info.hProcess;

	if(free_command_lpt)
	  fs_give((void **) &free_command_lpt);

	return(0);		/* success! */
    }

    if(free_command_lpt)
      fs_give((void **) &free_command_lpt);

    return(-1);
}


/*
 * Generate an error message for a failed windows exec or loadlibrary.
 */
void
mswin_exec_err_msg(char *what, int status, char *buf, size_t buflen)
{
    switch(status){
      case 2:
      case 3:
	snprintf(buf, buflen, "%s not found.", what);
	break;
	
      case 8:
	snprintf(buf, buflen, "Not enough memory to run %s.", what);
	break;
	
      default:
	snprintf(buf, buflen, "Error %d starting %s.", status, what);
	break;
    }
}

	
int
mswin_set_quit_confirm (int confirm)
{
    gConfirmExit = (confirm != 0);
    return (confirm);
}


/*
 * Called when Windows is in shutting down.  Before actually shutting down
 * Windows goes around to all the applications and asks if it is OK with
 * them to shut down (WM_QUERYENDSESSION).
 * If gConfirmExit is set, ask the user if they want to exit.
 * Returning zero will stop the shutdown, non-zero allows it to proceed.
 */
LOCAL LRESULT
ConfirmExit (void)
{
    TCHAR	msg[256];
    int		rc;

    if(gConfirmExit){
	_sntprintf(msg, sizeof(msg)/sizeof(TCHAR),
		   TEXT("Exiting may cause you to lose work in %s, Exit?"),
		   gszAppName);
	rc = MessageBox (ghTTYWnd, msg, gszAppName, MB_ICONSTOP | MB_OKCANCEL);
	if(rc == IDCANCEL)
	  return(0);
    }

    return(1);
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *                  Registry access functions
 *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*
 * Useful def's
 */
#define	MSWR_KEY_MAX	128
#define	MSWR_VAL_MAX	128
#define	MSWR_CLASS_MAX	128
#define	MSWR_DATA_MAX	1024


#define	MSWR_ROOT	TEXT("Software\\University of Washington\\Alpine\\1.0")
#define	MSWR_CAPABILITIES TEXT("Software\\University of Washington\\Alpine\\1.0\\Capabilities")
#define	MSWR_APPNAME	TEXT("Alpine")
#define MSWR_DLLPATH    TEXT("DLLPath")
#define MSWR_DLLNAME    TEXT("pmapi32.dll")
#define MSWRDBUF 1152


struct mswin_reg_key {
  HKEY    rhk;        /* root key (HKEY_LOCAL_MACHINE, ...) */
  LPTSTR *knames;     /* NULL terminated list of keys */
};

LPTSTR mswin_pine_hklm_regs[] = {
    MSWR_ROOT,
    TEXT("Software\\Clients\\Mail\\Alpine"),
    TEXT("Software\\Clients\\News\\Alpine"),
    TEXT("Software\\Classes\\Alpine.Url.Mailto"),
    TEXT("Software\\Classes\\Alpine.Url.News"),
    TEXT("Software\\Classes\\Alpine.Url.Nntp"),
    TEXT("Software\\Classes\\Alpine.Url.Imap"),
    NULL
};

LPTSTR mswin_pine_hkcu_regs[] = {
    MSWR_ROOT,
    NULL
};

static struct mswin_reg_key mswin_pine_regs[] = {
    {HKEY_LOCAL_MACHINE, mswin_pine_hklm_regs},
    {HKEY_CURRENT_USER, mswin_pine_hkcu_regs},
    {NULL, NULL}
};


/*
 *  data: unitialized buffer, could be null
 */
int
mswin_reg(int op, int tree, char *data_utf8, size_t size)
{
    LPTSTR data_lptstr = NULL;
    int rv;

    if(data_utf8){
	if(size == 0){
	    /* size is zero when op & MSWR_OP_SET */
	    data_lptstr = utf8_to_lptstr(data_utf8);
	}
	else {
	    data_lptstr = (LPTSTR)MemAlloc(size * sizeof(TCHAR));
	    data_lptstr[0] = '\0';
	}
    }

    rv = mswin_reg_lptstr(op, tree, data_lptstr, size);

    if(data_utf8 && data_lptstr){
	char *t_utf8str;
	if(size){
	    t_utf8str = lptstr_to_utf8(data_lptstr);
	    strncpy(data_utf8, t_utf8str, size);
	    data_utf8[size-1] = '\0';
	    MemFree((void *)t_utf8str);
	}
	MemFree((void *)data_lptstr);
    }
    return(rv);
}

int
mswin_reg_lptstr(int op, int tree, LPTSTR data_lptstr, size_t size)
{
    if(op & MSWR_OP_SET){
	switch(tree){
	  case MSWR_PINE_RC :
	    MSWRAlpineSet(HKEY_CURRENT_USER, NULL,
			TEXT("PineRC"), op & MSWR_OP_FORCE, data_lptstr);
	    break;

	  case MSWR_PINE_CONF :
	    MSWRAlpineSet(HKEY_CURRENT_USER, NULL,
			TEXT("PineConf"), op & MSWR_OP_FORCE, data_lptstr);
	    break;

	  case MSWR_PINE_AUX :
	    MSWRAlpineSet(HKEY_CURRENT_USER, NULL,
			TEXT("PineAux"), op & MSWR_OP_FORCE, data_lptstr);
	    break;

	  case MSWR_PINE_DIR :
	    MSWRAlpineSet(HKEY_LOCAL_MACHINE, NULL,
			TEXT("Pinedir"), op & MSWR_OP_FORCE, data_lptstr);
	    MSWRAlpineSetHandlers(op & MSWR_OP_FORCE, data_lptstr);
	    break;

	  case MSWR_PINE_EXE :
	    MSWRAlpineSet(HKEY_LOCAL_MACHINE, NULL,
			TEXT("PineEXE"), op & MSWR_OP_FORCE, data_lptstr);
	    break;

	  case MSWR_PINE_POS :
	    MSWRAlpineSet(HKEY_CURRENT_USER, NULL,
			TEXT("PinePos"), op & MSWR_OP_FORCE, data_lptstr);
	    break;

	  default :
	   break;
	}
    }
    else if(op & MSWR_OP_GET){
	switch(tree){
	  case MSWR_PINE_RC :
	    return(MSWRAlpineGet(HKEY_CURRENT_USER, NULL,
			       TEXT("PineRC"), data_lptstr, size));
	  case MSWR_PINE_CONF :
	    return(MSWRAlpineGet(HKEY_CURRENT_USER, NULL,
			       TEXT("PineConf"), data_lptstr, size));
	  case MSWR_PINE_AUX :
	    return(MSWRAlpineGet(HKEY_CURRENT_USER, NULL,
			       TEXT("PineAux"), data_lptstr, size));			
	  case MSWR_PINE_DIR :
	    return(MSWRAlpineGet(HKEY_LOCAL_MACHINE, NULL,
			       TEXT("Pinedir"), data_lptstr, size));
	  case MSWR_PINE_EXE :
	    return(MSWRAlpineGet(HKEY_LOCAL_MACHINE, NULL,
			       TEXT("PineEXE"), data_lptstr, size));
	  case MSWR_PINE_POS :
	    return(MSWRAlpineGet(HKEY_CURRENT_USER, NULL,
			       TEXT("PinePos"), data_lptstr, size));
	  default :
	   break;
	}
    }
    else if(op & MSWR_OP_BLAST){
        int rv = 0, i, j;

	for(i = 0; mswin_pine_regs[i].rhk; i++){
	    for(j = 0; mswin_pine_regs[i].knames[j]; j++)
	      MSWRClear(mswin_pine_regs[i].rhk, mswin_pine_regs[i].knames[j]);
	}
	if(rv) return -1;
    }
    /* else, ignore unknown op? */

    return(0);
}


LOCAL void
MSWRAlpineSet(HKEY hRootKey, LPTSTR subkey, LPTSTR val, int update, LPTSTR data)
{
    HKEY  hKey;
    TCHAR keybuf[MSWR_KEY_MAX+1];

    _sntprintf(keybuf, MSWR_KEY_MAX+1, TEXT("%s%s%s"), MSWR_ROOT,
	       (subkey && *subkey != '\\') ? TEXT("\\") : TEXT(""),
	       (subkey) ? TEXT("\\") : TEXT(""));

    if(RegCreateKeyEx(hRootKey, keybuf, 0, TEXT("REG_SZ"),
		      REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
		      NULL, &hKey, NULL) == ERROR_SUCCESS){
	if(update || RegQueryValueEx(hKey, val, NULL, NULL,
				      NULL, NULL) != ERROR_SUCCESS)
	  RegSetValueEx(hKey, val, 0, REG_SZ, (LPBYTE)data, (DWORD)(_tcslen(data)+1)*sizeof(TCHAR));

	RegCloseKey(hKey);
    }
}



LOCAL int
MSWRAlpineGet(HKEY hKey, LPTSTR subkey, LPTSTR val, LPTSTR data_lptstr, size_t len)
{
    TCHAR keybuf[MSWR_KEY_MAX+1];
    DWORD dlen = (DWORD)len;

    _sntprintf(keybuf, MSWR_KEY_MAX+1, TEXT("%s%s%s"), MSWR_ROOT,
	    (subkey && *subkey != '\\') ? TEXT("\\") : TEXT(""),
	    (subkey) ? TEXT("\\") : TEXT(""));

    return(MSWRPeek(hKey, keybuf, val, data_lptstr, &dlen) == TRUE);
}



LOCAL void
MSWRAlpineSetHandlers(int update, LPTSTR path_lptstr)
{
    HKEY  hKey, hSubKey;
    DWORD dwDisp;
    BYTE tmp_b[MSWR_DATA_MAX];
    unsigned long tmplen = MSWR_DATA_MAX, tmp_lptstr_tcharlen = MSWR_DATA_MAX/sizeof(TCHAR);
    int	  exists;
    LPTSTR tmp_lptstr = (LPTSTR)tmp_b;

    /* Register as a mail client on this system */
    if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		    MSWR_ROOT, 0, KEY_ALL_ACCESS,
		    &hKey) == ERROR_SUCCESS){
	if(RegCreateKeyEx(HKEY_LOCAL_MACHINE, MSWR_CAPABILITIES, 0, TEXT("REG_SZ"),
			  REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
			  NULL, &hSubKey, &dwDisp) == ERROR_SUCCESS){
	    MSWRPoke(hSubKey, NULL, TEXT("ApplicationDescription"),
		     TEXT("Alpine - A program for sending, receiving, and filing email and news, whether stored locally or accessed over the network via IMAP, POP3, or NNTP. Alpine is the successor to Pine, and also is maintained by the University of Washington."));
	    MSWRPoke(hSubKey, NULL, TEXT("ApplicationName"),
		     TEXT("Alpine"));
	    _sntprintf(tmp_lptstr, tmp_lptstr_tcharlen, TEXT("%salpine.exe,0"), path_lptstr);
	    MSWRPoke(hSubKey, NULL, TEXT("ApplicationIcon"), tmp_lptstr);
	    MSWRPoke(hSubKey, TEXT("UrlAssociations"), TEXT("mailto"), TEXT("Alpine.Url.Mailto"));
	    MSWRPoke(hSubKey, TEXT("UrlAssociations"), TEXT("news"), TEXT("Alpine.Url.News"));
	    MSWRPoke(hSubKey, TEXT("UrlAssociations"), TEXT("nntp"), TEXT("Alpine.Url.Nntp"));
	    MSWRPoke(hSubKey, TEXT("UrlAssociations"), TEXT("imap"), TEXT("Alpine.Url.Imap"));
	    RegCloseKey(hSubKey);
	}
	RegCloseKey(hKey);
	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE, TEXT("Software\\RegisteredApplications"), 0,
			KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS){
	    MSWRPoke(hKey, NULL, TEXT("Alpine"), MSWR_CAPABILITIES);
	    RegCloseKey(hKey);
	}
	if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,
			TEXT("Software\\Classes"), 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS){
	    if(RegCreateKeyEx(hKey, TEXT("Alpine.Url.Mailto"), 0, TEXT("REG_SZ"),
			      REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
			      NULL, &hSubKey, &dwDisp) == ERROR_SUCCESS){
		MSWRProtocolSet(hSubKey, MSWR_SDC_MAIL, path_lptstr);
		RegCloseKey(hSubKey);
	    }
	    if(RegCreateKeyEx(hKey, TEXT("Alpine.Url.Nntp"), 0, TEXT("REG_SZ"),
			      REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
			      NULL, &hSubKey, &dwDisp) == ERROR_SUCCESS){
		MSWRProtocolSet(hSubKey, MSWR_SDC_NNTP, path_lptstr);
		RegCloseKey(hSubKey);
	    }
	    if(RegCreateKeyEx(hKey, TEXT("Alpine.Url.News"), 0, TEXT("REG_SZ"),
			      REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
			      NULL, &hSubKey, &dwDisp) == ERROR_SUCCESS){
		MSWRProtocolSet(hSubKey, MSWR_SDC_NEWS, path_lptstr);
		RegCloseKey(hSubKey);
	    }
	    if(RegCreateKeyEx(hKey, TEXT("Alpine.Url.Imap"), 0, TEXT("REG_SZ"),
			      REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
			      NULL, &hSubKey, &dwDisp) == ERROR_SUCCESS){
		MSWRProtocolSet(hSubKey, MSWR_SDC_IMAP, path_lptstr);
		RegCloseKey(hSubKey);
	    }
	    RegCloseKey(hKey);
	}
    }

    if((exists = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
			      TEXT("SOFTWARE\\Clients\\Mail\\Alpine"),
			      0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
       || RegCreateKeyEx(HKEY_LOCAL_MACHINE,
			 TEXT("SOFTWARE\\Clients\\Mail\\Alpine"),
			 0, TEXT("REG_SZ"),
			 REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
			 NULL, &hKey, &dwDisp) == ERROR_SUCCESS){
	if(update || !exists){
	    DWORD dType;
	    char *tmp_utf8str = NULL;

	    MSWRPoke(hKey, NULL, NULL, MSWR_APPNAME);
	    /* set up MAPI dll stuff */
	    *tmp_b = 0;
	    RegQueryValueEx(hKey, MSWR_DLLPATH, NULL, &dType, tmp_b, &tmplen);
	    tmp_lptstr = (LPTSTR)tmp_b;
	    if(!(*tmp_lptstr)
	       || (can_access(tmp_utf8str = lptstr_to_utf8(tmp_lptstr), ACCESS_EXISTS) != 0)){
	      if(*tmp_lptstr)
		RegDeleteValue(hKey, MSWR_DLLPATH);
	      if(tmp_utf8str)
		MemFree((void *)tmp_utf8str);

	      _sntprintf(tmp_lptstr, tmp_lptstr_tcharlen,
			 TEXT("%s%s"), path_lptstr, MSWR_DLLNAME);

	      if(can_access(tmp_utf8str = lptstr_to_utf8(tmp_lptstr), ACCESS_EXISTS) == 0)
		MSWRPoke(hKey, NULL, MSWR_DLLPATH, tmp_lptstr);
	    }
	    if(tmp_utf8str)
	      MemFree((void *)tmp_utf8str);
	    /* Set "mailto" handler */
	    if(RegCreateKeyEx(hKey, TEXT("Protocols\\Mailto"), 0, TEXT("REG_SZ"),
			      REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
			      NULL, &hSubKey, &dwDisp) == ERROR_SUCCESS){
		MSWRProtocolSet(hSubKey, MSWR_SDC_MAIL, path_lptstr);
		RegCloseKey(hSubKey);
	    }

	    /* Set normal handler */
	    _sntprintf(tmp_lptstr, tmp_lptstr_tcharlen,
		       TEXT("\"%salpine.exe\""), path_lptstr);
	    MSWRPoke(hKey, TEXT("shell\\open\\command"), NULL, tmp_lptstr);
	}

	RegCloseKey(hKey);
    }

    /* Register as a news client on this system */
    if((exists = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
			      TEXT("SOFTWARE\\Clients\\News\\Alpine"),
			      0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
       || RegCreateKeyEx(HKEY_LOCAL_MACHINE,
			 TEXT("SOFTWARE\\Clients\\News\\Alpine"),
			 0, TEXT("REG_SZ"), REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
			 NULL, &hKey, &dwDisp) == ERROR_SUCCESS){
	if(update || !exists){
	    MSWRPoke(hKey, NULL, NULL, MSWR_APPNAME);

	    /* Set "news" handler */
	    if(RegCreateKeyEx(hKey, TEXT("Protocols\\news"), 0, TEXT("REG_SZ"),
			      REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
			      NULL, &hSubKey, &dwDisp) == ERROR_SUCCESS){
		MSWRProtocolSet(hSubKey, MSWR_SDC_NEWS, path_lptstr);
		RegCloseKey(hSubKey);
	    }
	    /* Set "nntp" handler */
	    if(RegCreateKeyEx(hKey, TEXT("Protocols\\nntp"), 0, TEXT("REG_SZ"),
			      REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
			      NULL, &hSubKey, &dwDisp) == ERROR_SUCCESS){
		MSWRProtocolSet(hSubKey, MSWR_SDC_NNTP, path_lptstr);
		RegCloseKey(hSubKey);
	    }

	    /* Set normal handler */
	    _sntprintf(tmp_lptstr, tmp_lptstr_tcharlen,
		      TEXT("\"%salpine.exe\""), path_lptstr);
	    MSWRPoke(hKey, TEXT("shell\\open\\command"), NULL, tmp_lptstr);
	}

	RegCloseKey(hKey);
    }

    /* Register as a IMAP url handler */
    if((exists = RegOpenKeyEx(HKEY_CLASSES_ROOT, TEXT("imap"),
			      0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
       || RegCreateKeyEx(HKEY_CLASSES_ROOT, TEXT("imap"), 0, TEXT("REG_SZ"),
			 REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
			 NULL, &hKey, &dwDisp) == ERROR_SUCCESS){
	if(update || !exists)
	  MSWRProtocolSet(hKey, MSWR_SDC_IMAP, path_lptstr);

	RegCloseKey(hKey);
    }
}



char *
mswin_reg_default_browser(char *url_utf8)
{
    TCHAR scheme[MSWR_KEY_MAX+1], *p;
    LPTSTR url_lptstr;
    char cmdbuf[MSWR_DATA_MAX], *cmd = NULL;

    url_lptstr = utf8_to_lptstr(url_utf8);

    if(url_lptstr && (p = _tcschr(url_lptstr, ':')) && p - url_lptstr < MSWR_KEY_MAX){
	_tcsncpy(scheme, url_lptstr, p - url_lptstr);
	scheme[p-url_lptstr] = '\0';

	if(MSWRShellCanOpen(scheme, cmdbuf, MSWR_DATA_MAX, 0)){
	    size_t len;

	    len = strlen(cmdbuf) + 2;
	    cmd = (char *) fs_get((len+1) * sizeof(char));
	    if(cmd){
		if(strchr(cmdbuf, '*'))
		  snprintf(cmd, len+1, "\"%s\"", cmdbuf);
		else{
		    strncpy(cmd, cmdbuf, len);
		    cmd[len] = '\0';
		}
	    }
	}
    }

    MemFree((void *)url_lptstr);

    return(cmd);
}


int
mswin_is_def_client(int type)
{
    TCHAR buf[MSWR_KEY_MAX+1];
    DWORD buflen = MSWR_KEY_MAX;

    if(type != MSWR_SDC_MAIL && type != MSWR_SDC_NEWS)
      return -1;

    if(MSWRPeek(HKEY_CURRENT_USER,
		type == MSWR_SDC_MAIL ? TEXT("Software\\Clients\\Mail")
		: TEXT("Software\\Clients\\News"), NULL,
		buf, &buflen) && !_tcscmp(buf, TEXT("Alpine")))
      return 1;
    buflen = MSWR_KEY_MAX;
    if(MSWRPeek(HKEY_LOCAL_MACHINE,
		type == MSWR_SDC_MAIL ? TEXT("Software\\Clients\\Mail")
		: TEXT("Software\\Clients\\News"), NULL,
		buf, &buflen) && !_tcscmp(buf, TEXT("Alpine")))
      return 1;
    return 0;
}

int
mswin_set_def_client(int type)
{
    HKEY hKey;
    int successful_set = 0;
    TCHAR path_lptstr[MSWR_DATA_MAX];
    DWORD dwDisp;

    if(type != MSWR_SDC_MAIL && type != MSWR_SDC_NEWS)
      return 1;
    if(RegOpenKeyEx(HKEY_CURRENT_USER,
		    type == MSWR_SDC_MAIL ? TEXT("Software\\Clients\\Mail")
		    : TEXT("Software\\Clients\\News"),
		    0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS){
	successful_set = MSWRPoke(hKey, NULL, NULL, TEXT("Alpine"));
	RegCloseKey(hKey);
    }
    else if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,
			 type == MSWR_SDC_MAIL ? TEXT("Software\\Clients\\Mail")
			 : TEXT("Software\\Clients\\News"),
			 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS){
	successful_set = MSWRPoke(hKey, NULL, NULL, TEXT("Alpine"));
	RegCloseKey(hKey);
    }
    if(successful_set){
	mswin_reg_lptstr(MSWR_OP_GET, MSWR_PINE_DIR, path_lptstr, sizeof(path_lptstr)/sizeof(TCHAR));
	if(type == MSWR_SDC_MAIL){
	    MSWRClear(HKEY_CLASSES_ROOT, TEXT("mailto"));
	    if(RegCreateKeyEx(HKEY_CLASSES_ROOT, TEXT("mailto"), 0, TEXT("REG_SZ"),
			      REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
			      NULL, &hKey, &dwDisp) == ERROR_SUCCESS){
		MSWRProtocolSet(hKey, MSWR_SDC_MAIL, path_lptstr);
		RegCloseKey(hKey);
	    }
	}
	else if(type == MSWR_SDC_NEWS){
	    MSWRClear(HKEY_CLASSES_ROOT, TEXT("news"));
	    if(RegCreateKeyEx(HKEY_CLASSES_ROOT, TEXT("news"), 0, TEXT("REG_SZ"),
			      REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
			      NULL, &hKey, &dwDisp) == ERROR_SUCCESS){
		MSWRProtocolSet(hKey, MSWR_SDC_NEWS, path_lptstr);
		RegCloseKey(hKey);
	    }
	    MSWRClear(HKEY_CLASSES_ROOT, TEXT("nntp"));
	    if(RegCreateKeyEx(HKEY_CLASSES_ROOT, TEXT("nntp"), 0, TEXT("REG_SZ"),
			      REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS,
			      NULL, &hKey, &dwDisp) == ERROR_SUCCESS){
		MSWRProtocolSet(hKey, MSWR_SDC_NNTP, path_lptstr);
		RegCloseKey(hKey);
	    }
	}
    }
    return 0;
}

int
MSWRProtocolSet(HKEY hKey, int type, LPTSTR path_lptstr)
{
    TCHAR tmp_lptstr[MSWR_DATA_MAX];
    BYTE EditFlags[4];
    unsigned long tmp_lptstr_len = MSWR_DATA_MAX;

    if(type != MSWR_SDC_MAIL && type != MSWR_SDC_NEWS
       && type != MSWR_SDC_NNTP && type != MSWR_SDC_IMAP)
      return -1;
    MSWRPoke(hKey, NULL, NULL, type == MSWR_SDC_MAIL
	     ? TEXT("URL:MailTo Protocol")
	     : type == MSWR_SDC_NEWS ? TEXT("URL:News Protocol")
	     : type == MSWR_SDC_NNTP ? TEXT("URL:NNTP Protocol")
	     : TEXT("URL:IMAP Prototcol"));
    MSWRPoke(hKey, NULL, TEXT("URL Protocol"), TEXT(""));

    EditFlags[0] = 0x02;
    EditFlags[1] = EditFlags[2] = EditFlags[3] = 0;

    (void) RegDeleteValue(hKey, TEXT("EditFlags"));
    (void) RegSetValueEx(hKey, TEXT("EditFlags"), 0, REG_BINARY,
			 EditFlags, (DWORD) 4);

    _sntprintf(tmp_lptstr, tmp_lptstr_len,
	       TEXT("%salpine.exe,0"), path_lptstr);
    MSWRPoke(hKey, TEXT("DefaultIcon"), NULL, tmp_lptstr);

    _sntprintf(tmp_lptstr, tmp_lptstr_len,
	      TEXT("\"%salpine.exe\" -url \"%%1\""), path_lptstr);
    MSWRPoke(hKey, TEXT("shell\\open\\command"), NULL, tmp_lptstr);
    return 0;
}


/* cmdbuf can stay char * since it's our string */
BOOL
MSWRShellCanOpen(LPTSTR key, char *cmdbuf, int clen, int allow_noreg)
{
    HKEY   hKey;
    BOOL   rv = FALSE;

    /* See if Shell provides a method to open the thing... */
    if(RegOpenKeyEx(HKEY_CLASSES_ROOT, key,
		    0, KEY_READ, &hKey) == ERROR_SUCCESS){

	if(cmdbuf){
	    strncpy(cmdbuf, "*Shell*", clen);
	    cmdbuf[clen-1] = '\0';
	}

	rv = TRUE;

	RegCloseKey(hKey);
    }
    else if(allow_noreg && cmdbuf){
	strncpy(cmdbuf, "*Shell*", clen);
	cmdbuf[clen-1] = '\0';
	rv = TRUE;
    }

    return(rv);
}


/*
 * Fundamental registry access function that queries for particular values.
 */
LOCAL BOOL
MSWRPeek(HKEY hRootKey, LPTSTR subkey, LPTSTR valstr, LPTSTR data_lptstr, DWORD *dlen)
{
    HKEY   hKey;
    DWORD  dtype, dlen_bytes = (dlen ? *dlen : 0) * sizeof(TCHAR);
    LONG   rv = !ERROR_SUCCESS;

    if(RegOpenKeyEx(hRootKey, subkey, 0, KEY_READ, &hKey) == ERROR_SUCCESS){
	rv = RegQueryValueEx(hKey, valstr, NULL, &dtype, (LPBYTE)data_lptstr, &dlen_bytes);
	if(dlen)
	  (*dlen) = dlen_bytes;
	RegCloseKey(hKey);
    }

    return(rv == ERROR_SUCCESS);
}


/*
 * Fundamental registry access function that sets particular values.
 */
LOCAL BOOL
MSWRPoke(HKEY hKey, LPTSTR subkey, LPTSTR valstr, LPTSTR data_lptstr)
{
    DWORD  dtype, dwDisp, dlen = MSWR_DATA_MAX;
    BYTE   olddata[MSWR_DATA_MAX];
    BOOL   rv = FALSE;

    if(!subkey
       || RegCreateKeyEx(hKey, subkey,
			 0, TEXT("REG_SZ"), REG_OPTION_NON_VOLATILE,
			 KEY_ALL_ACCESS, NULL,
			 &hKey, &dwDisp) == ERROR_SUCCESS){

	if(RegQueryValueEx(hKey, valstr, NULL, &dtype,
			   olddata, &dlen) != ERROR_SUCCESS
	   || _tcscmp((LPTSTR)olddata, data_lptstr)){
	    (void) RegDeleteValue(hKey, valstr);
	    rv = RegSetValueEx(hKey, valstr, 0, REG_SZ,
			       (LPBYTE)data_lptstr,
			       (DWORD)(_tcslen(data_lptstr) + 1)*sizeof(TCHAR)) == ERROR_SUCCESS;
	}

	if(subkey)
	  RegCloseKey(hKey);
    }

    return(rv);
}


LOCAL void
MSWRLineBufAdd(MSWR_LINE_BUFFER_S *lpLineBuf, LPTSTR line)
{
    if(lpLineBuf->offset >= lpLineBuf->size){
	/* this probably won't happen, but just in case */
	lpLineBuf->size *= 2;
	lpLineBuf->linep = (char **)MemRealloc(lpLineBuf->linep,
					      (lpLineBuf->size + 1)*sizeof(char *));
    }

    lpLineBuf->linep[lpLineBuf->offset++] = lptstr_to_utf8(line);
    lpLineBuf->linep[lpLineBuf->offset] = NULL;
}

/*
 *  Dump all of the registry values from a list of keys into an array
 *  of UTF8-formatted strings.
 */
char **
mswin_reg_dump(void)
{
    MSWR_LINE_BUFFER_S lineBuf;
    unsigned long initial_size = 256;
    int i, j;

    lineBuf.linep = (char **)MemAlloc((initial_size+1)*sizeof(char *));
    lineBuf.size = initial_size;
    lineBuf.offset = 0;
    
    MSWRLineBufAdd(&lineBuf, TEXT("Registry values for Alpine:"));
    MSWRLineBufAdd(&lineBuf, TEXT(""));

    for(i = 0; mswin_pine_regs[i].rhk; i++){
	MSWRLineBufAdd(&lineBuf, mswin_pine_regs[i].rhk == HKEY_LOCAL_MACHINE
		       ? TEXT("HKEY_LOCAL_MACHINE") 
		       : TEXT("HKEY_CURRENT_USER"));
	for(j = 0; mswin_pine_regs[i].knames[j]; j++)
	  MSWRDump(mswin_pine_regs[i].rhk,
		   mswin_pine_regs[i].knames[j],
		   1, &lineBuf);
    }

    return(lineBuf.linep);
}


/*
 * Recursive function to crawl a registry hierarchy and print the contents.
 *
 * Returns: 0
 */
LOCAL int
MSWRDump(HKEY hKey, LPTSTR pSubKey, int keyDepth, MSWR_LINE_BUFFER_S *lpLineBuf)
{
    HKEY  hSubKey;
    TCHAR KeyBuf[MSWR_KEY_MAX+1];
    TCHAR ValBuf[MSWR_VAL_MAX+1];
    BYTE  DataBuf[MSWR_DATA_MAX+1];
    DWORD dwKeyIndex, dwKeyLen;
    DWORD dwValIndex, dwValLen, dwDataLen;
    DWORD dwType;
    FILETIME ftKeyTime;
    TCHAR new_buf[1024];
    unsigned int new_buf_len = 1024;
    int i, j, k, tab_width = 4;

    /* open the passed subkey */
    if(RegOpenKeyEx(hKey, pSubKey, 0,
		    KEY_READ, &hSubKey) == ERROR_SUCCESS){
	
	/* print out key name here */
	for(i = 0, k = 0; i < keyDepth % 8; i++)
	  for(j = 0; j < tab_width; j++)
	    new_buf[k++] = ' ';
	_sntprintf(new_buf+k, new_buf_len - k, TEXT("%s"), pSubKey);
	new_buf[new_buf_len - 1] = '\0';
	MSWRLineBufAdd(lpLineBuf, new_buf);

	keyDepth++;

	/* Loop through the string values and print their data */
	for(dwValIndex = 0L, dwValLen = MSWR_VAL_MAX + 1, dwDataLen = MSWR_DATA_MAX + 1;
	    RegEnumValue(hSubKey, dwValIndex, ValBuf, &dwValLen, NULL, &dwType,
			 DataBuf, &dwDataLen) == ERROR_SUCCESS;
	    dwValIndex++, dwValLen = MSWR_VAL_MAX + 1, dwDataLen = MSWR_DATA_MAX + 1){

	    /* print out value here */
	    for(i = 0, k = 0; i < keyDepth % 8; i++)
	      for(j = 0; j < tab_width; j++)
		new_buf[k++] = ' ';
	    _sntprintf(new_buf+k, new_buf_len - k,
		       TEXT("%.*s = %.*s"),
		       dwValLen ? dwValLen : 128,
		       dwValLen ? ValBuf : TEXT("(Default)"),
		       dwType == REG_SZ && dwDataLen ? dwDataLen/sizeof(TCHAR) : 128,
		       (dwType == REG_SZ 
			? (dwDataLen ? (LPTSTR)DataBuf : TEXT("(No data)"))
			: TEXT("(Some non-string data)")));
	    new_buf[new_buf_len - 1] = '\0';
	    MSWRLineBufAdd(lpLineBuf, new_buf);
	}

	/* Loop through the subkeys and recursively print their data */
	for(dwKeyIndex = 0L, dwKeyLen = MSWR_KEY_MAX + 1;
	    RegEnumKeyEx(hSubKey, dwKeyIndex, KeyBuf, &dwKeyLen,
			 NULL, NULL, NULL, &ftKeyTime) == ERROR_SUCCESS;
	    dwKeyIndex++, dwKeyLen = MSWR_KEY_MAX + 1){
	    MSWRDump(hSubKey, KeyBuf, keyDepth, lpLineBuf);
	}
    }
    else {
	/* Couldn't open the key.  Must not be defined. */
	for(i = 0, k = 0; i < keyDepth % 8; i++)
	  for(j = 0; j < tab_width; j++)
	    new_buf[k++] = ' ';
	_sntprintf(new_buf+k, new_buf_len - k, TEXT("%s - Not Defined"), pSubKey);
	new_buf[new_buf_len - 1] = '\0';
	MSWRLineBufAdd(lpLineBuf, new_buf);
    }

    return 0;
}


/*
 * Fundamental registry access function that removes a registry key
 * and all its subkeys, their values and data
 */
LOCAL int
MSWRClear(HKEY hKey, LPTSTR pSubKey)
{
    HKEY	hSubKey;
    TCHAR	KeyBuf[MSWR_KEY_MAX+1];
    DWORD	dwKeyIndex, dwKeyLen;
    FILETIME	ftKeyTime;
    int         rv = 0;

    if(RegOpenKeyEx(hKey, pSubKey, 0,
		    KEY_READ, &hSubKey) == ERROR_SUCCESS){
	RegCloseKey(hSubKey);
	if(RegOpenKeyEx(hKey, pSubKey, 0,
			KEY_ALL_ACCESS, &hSubKey) == ERROR_SUCCESS){
	    for(dwKeyIndex = 0L, dwKeyLen = MSWR_KEY_MAX + 1;
		RegEnumKeyEx(hSubKey, dwKeyIndex, KeyBuf, &dwKeyLen,
			     NULL, NULL, NULL, &ftKeyTime) == ERROR_SUCCESS;
		dwKeyLen = MSWR_KEY_MAX + 1)
	      if(MSWRClear(hSubKey, KeyBuf)!= 0){
		  rv = -1;
		  dwKeyIndex++;
	      }
	    RegCloseKey(hSubKey);
	    if(RegDeleteKey(hKey, pSubKey) != ERROR_SUCCESS || rv)
	      return -1;
	    return 0;
	}
	return -1;
    }
    return 0;
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *                  Text display Windows.
 *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*
 * Show a help message.
 * Help text comes as a null terminated array of pointers to lines of
 * text.  Stuff these into a buffer and pass that to MessageBox.
 */
void
mswin_showhelpmsg(WINHAND wnd, char **helplines)
{
    char      **l;
    char       *helptext_utf8, *p;
    size_t	buflen;
    HWND	hWnd;
    LPTSTR      helptext_lpt;

    hWnd = (HWND) wnd;
    if(hWnd == NULL)
      hWnd = ghTTYWnd;

    buflen = 0;
    for(l = helplines; *l != NULL; ++l)
      buflen += (strlen(*l)+1);

    helptext_utf8 = (char *) fs_get((buflen + 1) * sizeof(char));
    if(helptext_utf8 == NULL)
      return;

    *helptext_utf8 = '\0';

    p = helptext_utf8;
    for(l = helplines; *l != NULL; ++l){
	snprintf(p, buflen+1-(p-helptext_utf8), "%s%s", (p == helptext_utf8) ? "" : " ", *l);
	p += strlen(p);
    }

    helptext_lpt = utf8_to_lptstr(helptext_utf8);

    MessageBox(hWnd, helptext_lpt, TEXT("Help"),
	       MB_APPLMODAL | MB_ICONINFORMATION | MB_OK);

    fs_give((void **) &helptext_utf8);
    fs_give((void **) &helptext_lpt);
}

/*
 * Callback for when new mail or imap telem window gets canned.
 */
LOCAL void
mswin_tw_close_imap_telem_or_new_mail(MSWIN_TEXTWINDOW *mswin_tw)
{
    HMENU hMenu;

    if((mswin_tw->id == IDM_OPT_IMAPTELEM) && gIMAPDebugOFFCallback)
        (*gIMAPDebugOFFCallback)();

    if(hMenu = GetMenu(ghTTYWnd))
        CheckMenuItem (hMenu, mswin_tw->id, MF_BYCOMMAND | MF_UNCHECKED);
}

/*
 * Callback for when new mail or imap telem window gets cleared.
 */
LOCAL void
mswin_tw_clear_imap_telem_or_new_mail(MSWIN_TEXTWINDOW *mswin_tw)
{
    char *tmtxt;
    time_t now = time((time_t *)0);
    LPCTSTR desc = (mswin_tw->id == IDM_OPT_IMAPTELEM) ?
                TEXT("IMAP telemetry recorder") :
                TEXT("New Mail window");

    tmtxt = ctime(&now);

    mswin_tw_printf(mswin_tw, TEXT("%s started at %.*S"),
        desc, MIN(100, strlen(tmtxt)-1), tmtxt);

    if(mswin_tw->id == IDM_OPT_NEWMAILWIN)
    {
        int i;
        int fromlen, subjlen, foldlen, len;
        TCHAR ring2[256];

        foldlen = (int)(.18 * gNMW_width);
        foldlen = foldlen > 5 ? foldlen : 5;
        fromlen = (int)(.28 * gNMW_width);
        subjlen = gNMW_width - 2 - foldlen - fromlen;

        mswin_tw_printf(mswin_tw,
	   TEXT("  %-*s%-*s%-*s"),
	   fromlen, TEXT("From:"),
	   subjlen, TEXT("Subject:"),
	   foldlen, TEXT("Folder:"));

        len = 2 + fromlen + subjlen + foldlen;
        if(len >= ARRAYSIZE(ring2) + 1)
            len = ARRAYSIZE(ring2) - 2;
        for(i = 0; i < len; i++)
            ring2[i] = '-';
        ring2[i] = '\0';
        mswin_tw_puts_lptstr(mswin_tw, ring2);
    }
}

/*
 * Init new mail or imap telem window
 */
LOCAL int
mswin_tw_init(MSWIN_TEXTWINDOW *mswin_tw, int id, LPCTSTR title)
{
    if(mswin_tw->hwnd){
	/* destroy it */
	mswin_tw_close(mswin_tw);
    }
    else{
        HMENU hMenu;

	mswin_tw->id = id;
	mswin_tw->hInstance = ghInstance;
	mswin_tw->print_callback = mswin_tw_print_callback;
	mswin_tw->close_callback = mswin_tw_close_imap_telem_or_new_mail;
	mswin_tw->clear_callback = mswin_tw_clear_imap_telem_or_new_mail;

        // If the rcSize rect is empty, then init it to something resembling
        //   the size of the current Pine window. Otherwise we just re-use
        //   whatever the last position/size was.
        if(IsRectEmpty(&mswin_tw->rcSize))
        {
            RECT cliRect;
            RECT sizeRect;

	    GetClientRect(ghTTYWnd, &cliRect);
	    sizeRect.left = CW_USEDEFAULT;
	    sizeRect.top = CW_USEDEFAULT;
	    sizeRect.right = cliRect.right;
	    sizeRect.bottom = cliRect.bottom;
            mswin_tw->rcSize = sizeRect;
        }

	if(!mswin_tw_create(mswin_tw, title))
            return 1;

	mswin_tw_setfont(mswin_tw, gpTTYInfo->hTTYFont);
        mswin_tw_setcolor(mswin_tw, gpTTYInfo->rgbFGColor, gpTTYInfo->rgbBGColor);

	mswin_tw_clear(mswin_tw);

        if(id == IDM_OPT_IMAPTELEM)
        {
	    if(gIMAPDebugONCallback)
	      (*gIMAPDebugONCallback)();

	    mswin_tw_showwindow(mswin_tw, SW_SHOWNA);
        }
	else if(id == IDM_OPT_NEWMAILWIN){
	    mswin_tw_showwindow(mswin_tw, SW_SHOW);
	}

	if(hMenu = GetMenu(ghTTYWnd))
	    CheckMenuItem (hMenu, mswin_tw->id, MF_BYCOMMAND | MF_CHECKED);
    }

    return(0);
}

/*
 * Display text in a window.
 *
 * Parameters:
 *	title		- Title of window.
 *	pText		- address of text to display.
 *	textLen		- Length of text, in bytes.  Limited to 64K.
 *	pLines		- Array of pointers to lines of text.  Each
 *			  line is a sepreate allocation block.  The
 *			  entry in the array of pointers should be a
 *			  NULL.
 *			
 * The text can be supplied as a buffer (pText and textLen) in which
 * lines are terminated by CRLF (including the last line in buffer).
 * This buffer should be NULL terminated, too.
 * Or it can be supplied as a NULL terminated array of pointers to
 * lines.  Each entry points to a separately allocated memory block
 * containing a null terminated string.
 *
 * If the function succeeds the memory containing the text will be
 * used until the user closes the window, at which point it will be
 * freed.
 *
 * Returns:
 *	mswin_tw  - SUCCESS
 *	NULL      - Failed.
 */
MSWIN_TEXTWINDOW *
mswin_displaytext(char *title_utf8, char *pText_utf8, size_t npText,
                  char **pLines_utf8, MSWIN_TEXTWINDOW *mswin_tw, int flags)
{
    LPTSTR  title_lpt = NULL, pText_lpt = NULL, *pLines_lpt = NULL;
    char  **l;
    size_t  pText_lpt_len = 0;
    int     i, count = 0;

    if(pLines_utf8 != NULL){
	for(count=0, l = pLines_utf8; *l != NULL; ++l)
	  ++count;

	pLines_lpt = (LPTSTR *) fs_get((count + 1) * sizeof(LPTSTR));
	memset(pLines_lpt, 0, (count + 1) * sizeof(LPTSTR));
	for(i=0, l = pLines_utf8; *l != NULL && i < count; ++l, ++i)
	  pLines_lpt[i] = utf8_to_lptstr(*l);

	/*caller expects this to be freed */
	if(!(flags & MSWIN_DT_NODELETE)){
	    for(l = pLines_utf8; *l != NULL; ++l)
	      fs_give((void **) l);

	    fs_give((void **) &pLines_utf8);
	}
    }

    if(pText_utf8 != NULL && npText > 0){
	pText_lpt = utf8_to_lptstr(pText_utf8);
	pText_lpt_len = lstrlen(pText_lpt);

	/*caller expects this to be freed */
	if(!(flags & MSWIN_DT_NODELETE))
	  fs_give((void **) &pText_utf8);
    }

    if(title_utf8 != NULL)
      title_lpt = utf8_to_lptstr(title_utf8);

    mswin_tw = mswin_tw_displaytext_lptstr(title_lpt, pText_lpt, pText_lpt_len,
				           pLines_lpt, mswin_tw, flags);

    if(pLines_lpt != NULL){
	for(i=0; i < count; ++i)
	    if(pLines_lpt[i])
		fs_give((void **) &pLines_lpt[i]);

	    fs_give((void **) &pLines_lpt);
	}

    if(pText_lpt != NULL)
        fs_give((void **) &pText_lpt);

    if(title_lpt != NULL)
      fs_give((void **) &title_lpt);

    return(mswin_tw);
}

/*
 * Callback for when a generic mswin_tw gets killed.
 */
LOCAL void
mswin_tw_close_callback(MSWIN_TEXTWINDOW *mswin_tw)
{
    if(mswin_tw->id != -1)
        MemFree(mswin_tw);
}

/*
 * Create a new mswin_tw window. If the MSWIN_DT_USEALTWINDOW flag is set,
 *      then (re)use gMswinAltWin.
 */
LOCAL MSWIN_TEXTWINDOW *
mswin_tw_displaytext_lptstr (LPTSTR title, LPTSTR pText, size_t textLen, LPTSTR *pLines,
	MSWIN_TEXTWINDOW *mswin_tw, int flags)
{
    if (pText == NULL && pLines == NULL)
	return (NULL);

    /* Was a valid existing window supplied? */
    if(!mswin_tw)
    {
        int ctrl_down = GetKeyState(VK_CONTROL) < 0;

        if((flags & MSWIN_DT_USEALTWINDOW) && !ctrl_down)
        {
            mswin_tw = &gMswinAltWin;
            mswin_tw->id = (UINT)-1;  // Tell mswin_tw_close_callback not
                                      //  to free this buffer.
        }
        else
        {
	    mswin_tw = (MSWIN_TEXTWINDOW *)MemAlloc (sizeof (MSWIN_TEXTWINDOW));
	    if(!mswin_tw)
	        return NULL;

	    memset(mswin_tw, 0, sizeof(MSWIN_TEXTWINDOW));
	    mswin_tw->id = 0;
        }

	mswin_tw->hInstance = ghInstance;
	mswin_tw->print_callback = mswin_tw_print_callback;
	mswin_tw->close_callback = mswin_tw_close_callback;
	mswin_tw->clear_callback = NULL;
    }

    /* Create a new window. */
    if (!mswin_tw->hwnd) {

        if(IsRectEmpty(&mswin_tw->rcSize))
        {
	    RECT sizeRect;
            RECT cliRect;

	    GetClientRect(ghTTYWnd, &cliRect);
	    sizeRect.left = CW_USEDEFAULT;
	    sizeRect.top = CW_USEDEFAULT;
	    sizeRect.right = cliRect.right;
	    sizeRect.bottom = cliRect.bottom;
            mswin_tw->rcSize = sizeRect;
        }

        if(!mswin_tw_create(mswin_tw, title)) {
	    MemFree (mswin_tw);
	    return (NULL);
	}

        mswin_tw_setfont(mswin_tw, gpTTYInfo->hTTYFont);
        mswin_tw_setcolor(mswin_tw, gpTTYInfo->rgbFGColor, gpTTYInfo->rgbBGColor);
    }
    else {
	/* Invalidate whole window, change title, and move to top. */
	SetWindowText (mswin_tw->hwnd, title);
	SetWindowPos (mswin_tw->hwnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
    }

    mswin_tw_clear(mswin_tw);

    /*
     * How was text supplied?
     */
    if (pLines != NULL) {
	LPTSTR *l;

	/* Array of pointers to lines supplied. Count lines. */
	for (l = pLines; *l != NULL; ++l){
	    mswin_tw_puts_lptstr(mswin_tw, *l);
	}
    }
    else {
#ifdef ALTED_DOT
        if(flags & MSWIN_DT_FILLFROMFILE)
            mswin_tw_fill_from_file(mswin_tw, pText);
        else
#endif /* ALTED_DOT */
	/* Pointer to block of text supplied. */
	mswin_tw_puts_lptstr(mswin_tw, pText);
    }

    mswin_tw_setsel(mswin_tw, 0, 0);

    mswin_tw_showwindow(mswin_tw, SW_SHOW);
    return mswin_tw;
}

void
mswin_enableimaptelemetry(int state)
{
    HMENU		hMenu;
    MENUITEMINFO	mitem;
    TCHAR		buf[256];
    int			i;

    hMenu = GetMenu (ghTTYWnd);
    if (hMenu == NULL)
	return;

    /*
     * Make sure hMenu's the right menubar
     */
    mitem.cbSize = sizeof(MENUITEMINFO);
    mitem.fMask = (MIIM_SUBMENU | MIIM_TYPE);
    mitem.fType = MFT_STRING;
    mitem.dwTypeData = buf;
    mitem.cch = 255;
    if(GetMenuItemCount(hMenu) == 7
       && GetMenuItemInfo(hMenu, 5, TRUE, &mitem)){
	if(mitem.fType == MFT_STRING
	   && !_tcscmp(mitem.dwTypeData, TEXT("&Config")))
	  hMenu = mitem.hSubMenu;
	else
	  return;
    }
    else
      return;

    i = GetMenuItemCount(hMenu);
    if(state == TRUE && i < 10){
	mitem.fMask = MIIM_TYPE;
	mitem.fType = MFT_SEPARATOR;
	InsertMenuItem (hMenu, 8, TRUE, &mitem);

	mitem.fMask = (MIIM_TYPE | MIIM_ID);
	mitem.fType = MFT_STRING;
	mitem.wID = IDM_OPT_IMAPTELEM;
	mitem.dwTypeData = TEXT("&IMAP Telemetry");
	mitem.cch = 15;
	InsertMenuItem (hMenu, 9, TRUE, &mitem);

	DrawMenuBar (ghTTYWnd);
    }
    else if(state == FALSE && i == 10){
	DeleteMenu (hMenu, 8, MF_BYPOSITION);
	DeleteMenu (hMenu, IDM_OPT_IMAPTELEM, MF_BYCOMMAND);
	DrawMenuBar (ghTTYWnd);
    }
}

int
mswin_imaptelemetry(char *msg)
{
    if(gMswinIMAPTelem.hwnd){
	LPTSTR msg_lptstr;
	msg_lptstr = utf8_to_lptstr(msg);
	mswin_tw_puts_lptstr(&gMswinIMAPTelem, msg_lptstr);
	fs_give((void **) &msg_lptstr);
	return(1);
    }
    return(0);
}


/*
 * The newmail window has a format proportional to screen width.
 * At this point, we've figured out the sizes of the fields, now
 * we fill that field to it's desired column size.  This used to
 * be a lot eaier when it was one char per column, but in the
 * unicode world it's not that easy.  This code does make the
 * assumption that ASCII characters are 1 column.
 */
LPTSTR
format_newmail_string(LPTSTR orig_lptstr, int format_len)
{
    LPTSTR new_lptstr;
    int i, colLen;

    new_lptstr = (LPTSTR)MemAlloc((format_len+1)*sizeof(TCHAR));

    /*
     * Fill up string till we reach the format_len, we can backtrack
     * if we need elipses.
     */
    for(i = 0, colLen = 0;
	i < format_len && colLen < format_len && orig_lptstr && orig_lptstr[i];
	i++){
	new_lptstr[i] = orig_lptstr[i];
	colLen += wcellwidth(new_lptstr[i]);
    }

    if(colLen > format_len || (colLen == format_len && orig_lptstr[i])){
	/*
	 * If we hit the edge of the format and there's still stuff
	 * to write, go back and add ".."
	 */
	i--;
	if(wcellwidth(new_lptstr[i]) > 1){
	    colLen -= wcellwidth(new_lptstr[i]);
	}
	else{
	    colLen -= wcellwidth(new_lptstr[i]);
	    i--;
	    colLen -= wcellwidth(new_lptstr[i]);
	}
	while(colLen < format_len && i < format_len){
	    new_lptstr[i++] = '.';
	    colLen++;
	}
    }
    else{
	/*
	 * If we've hit the end of the string, add spaces until
	 * we get to the correct length.
	 */
	for(; colLen < format_len && i < format_len; i++, colLen++){
	    new_lptstr[i] = ' ';
	}
    }

    if(i <= format_len)
      new_lptstr[i] = '\0';
    else
      new_lptstr[format_len] = '\0';

    return(new_lptstr);
}

/*
 * We're passed the relevant fields, now format them according to window with
 * and put up for display
 */
int
mswin_newmailwin(int is_us, char *from_utf8, char *subject_utf8, char *folder_utf8)
{
    TCHAR tcbuf[256];
    int foldlen, fromlen, subjlen;
    LPTSTR from_lptstr = NULL, subject_lptstr = NULL, folder_lptstr = NULL;
    LPTSTR from_format, subject_format, folder_format;

    if(!gMswinNewMailWin.hwnd)
        return 0;

    if(from_utf8)
      from_lptstr = utf8_to_lptstr(from_utf8);
    if(subject_utf8)
      subject_lptstr = utf8_to_lptstr(subject_utf8);
    if(folder_utf8)
      folder_lptstr = utf8_to_lptstr(folder_utf8);


    foldlen = (int)(.18 * gNMW_width);
    foldlen = foldlen > 5 ? foldlen : 5;
    fromlen = (int)(.28 * gNMW_width);
    subjlen = gNMW_width - 2 - foldlen - fromlen;


    from_format = format_newmail_string(from_lptstr
					 ? from_lptstr : TEXT(""),
					 fromlen - 1);
    subject_format = format_newmail_string(subject_lptstr
					    ? subject_lptstr : TEXT("(no subject)"),
					    subjlen - 1);
    folder_format = format_newmail_string(folder_lptstr
					   ? folder_lptstr : TEXT("INBOX"),
					   foldlen);

    _sntprintf(tcbuf, 256, TEXT("%c %s %s %s"), is_us ? '+' : ' ',
	       from_format, subject_format, folder_format);

    if(from_lptstr)
      fs_give((void **) &from_lptstr);
    if(subject_lptstr)
      fs_give((void **) &subject_lptstr);
    if(folder_lptstr)
      fs_give((void **) &folder_lptstr);
    MemFree((void *)from_format);
    MemFree((void *)subject_format);
    MemFree((void *)folder_format);

    mswin_tw_puts_lptstr(&gMswinNewMailWin, tcbuf);
    return 1;
}

/*
 * Mouse up, end selection
 */

LOCAL void
mswin_tw_print_callback(MSWIN_TEXTWINDOW *mswin_tw)
{
    LPTSTR              text;
    UINT                text_len;
    int			rc;
#define DESC_LEN	180
    TCHAR		description[DESC_LEN+1];

    GetWindowText(mswin_tw->hwnd, description, DESC_LEN);

    rc = mswin_print_ready((WINHAND)mswin_tw->hwnd, description);
    if (rc != 0) {
	if (rc != PE_USER_CANCEL) {
	    LPTSTR e;

	    e = utf8_to_lptstr(mswin_print_error(rc));
	    if(e){
		_sntprintf(description, DESC_LEN+1, TEXT("Printing failed:  %s"), e);
		fs_give((void **) &e);
	    }

	    MessageBox(mswin_tw->hwnd, description, TEXT("Print Failed"),
                MB_OK | MB_ICONEXCLAMATION);
	}
	return;
    }

    text_len = mswin_tw_gettextlength(mswin_tw);
    text = (LPTSTR) fs_get(text_len * sizeof(TCHAR));
    mswin_tw_gettext(mswin_tw, text, text_len);
    mswin_print_text(text);
    fs_give((void **)&text);

    mswin_print_done();
}
		

/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *                  Character Queue
 *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/


typedef struct {
        BOOL fKeyControlDown;
        UCS	c;   /* Bigger than TCHAR for CTRL and MENU setting */
} CQEntry;

LOCAL CQEntry CQBuffer [CHARACTER_QUEUE_LENGTH];
LOCAL int CQHead;
LOCAL int CQTail;
LOCAL int CQCount;


/*---------------------------------------------------------------------------
 *  BOOL  CQInit ()
 *
 *  Description:
 *		Initialize the Character queue.
 *
 *  Parameters:
 *
 *
/*--------------------------------------------------------------------------*/
LOCAL void
CQInit (void)
{
	CQHead = 0;
	CQTail = 0;
	CQCount = 0;
}


/*---------------------------------------------------------------------------
 *  BOOL  CQAvailable (void)
 *
 *  Description:
 *		Return TRUE if there are characters in the queue.
 *
 *  Parameters:
 *
 *
/*--------------------------------------------------------------------------*/

LOCAL BOOL
CQAvailable (void)
{
	return (CQCount > 0);
}



/*---------------------------------------------------------------------------
 *  BOOL  CQAdd (WORD c, DWORC keyData)
 *
 *  Description:
 *		Add 'c' to the end of the character queue.
 *
 *  Parameters:
 *		return true if successfull.
 *
/*--------------------------------------------------------------------------*/

LOCAL BOOL
CQAdd (UCS c, BOOL fKeyControlDown)
{
	if (CQCount == CHARACTER_QUEUE_LENGTH)
		return (FALSE);
	
	CQBuffer[CQTail].fKeyControlDown = fKeyControlDown;
	CQBuffer[CQTail].c = c;
	CQTail = (CQTail + 1) % CHARACTER_QUEUE_LENGTH;
	++CQCount;
	return (TRUE);
}



/*---------------------------------------------------------------------------
 *  BOOL  CQAddUniq (WORD c, DWORC keyData)
 *
 *  Description:
 *		Add 'c' to the end of the character queue, only if
 *		there is no other 'c' in the queue
 *
 *  Parameters:
 *		return true if successfull.
 *
/*--------------------------------------------------------------------------*/

LOCAL BOOL
CQAddUniq (UCS c, BOOL fKeyControlDown)
{
	int		i;
	int		pos;
	
	if (CQCount == CHARACTER_QUEUE_LENGTH)
		return (FALSE);
	
	pos = CQHead;
	for (i = 0; i < CQCount; ++i) {
	    if (CQBuffer[pos].c == c)
		return (FALSE);
	    pos = (pos + 1) % CHARACTER_QUEUE_LENGTH;
	}
	return (CQAdd (c, fKeyControlDown));
}




/*---------------------------------------------------------------------------
 *  int  CQGet ()
 *
 *  Description:
 *		Return the next byte from the head of the queue.  If there is
 *		no byte available, returns 0, which is indistinquishable from
 *		'\0'.  So it is a good idea to call CQAvailable first.
 *
 *  Parameters:
 *		none.
 *
/*--------------------------------------------------------------------------*/

LOCAL UCS
CQGet ()
{
    UCS	c;

    if (CQCount == 0)
	return (0);

    c = CQBuffer[CQHead].c;

    if(CQBuffer[CQHead].fKeyControlDown)
        c |= CTRL;

    if(c < ' ') {
        c += '@';
        c |= CTRL;
    }

    CQHead = (CQHead + 1) % CHARACTER_QUEUE_LENGTH;
    --CQCount;
    return (c);
}



/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
 *
 *                  Mouse Event Queue
 *
 *++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/



LOCAL MEvent MQBuffer [MOUSE_QUEUE_LENGTH];
LOCAL int MQHead;
LOCAL int MQTail;
LOCAL int MQCount;


/*---------------------------------------------------------------------------
 *  BOOL  MQInit ()
 *
 *  Description:
 *		Initialize the Character queue.
 *
 *  Parameters:
 *
 *
/*--------------------------------------------------------------------------*/
LOCAL void
MQInit (void)
{
	MQHead = 0;
	MQTail = 0;
	MQCount = 0;
}


/*---------------------------------------------------------------------------
 *  BOOL  MQAvailable (void)
 *
 *  Description:
 *		Return TRUE if there are characters in the queue.
 *
 *  Parameters:
 *
 *
/*--------------------------------------------------------------------------*/

LOCAL BOOL
MQAvailable (void)
{
	return (MQCount > 0);
}



/*---------------------------------------------------------------------------
 *  BOOL  MQAdd ()
 *
 *  Description:
 *		Add 'c' to the end of the character queue.
 *
 *  Parameters:
 *		return true if successfull.
 *
/*--------------------------------------------------------------------------*/

LOCAL BOOL
MQAdd (int mevent, int button, int nRow, int nColumn, int keys, int flags)
{
    int		c;
    int		i = 0;
    BOOL	found = FALSE;


    /*
     * Find a queue insertion point.
     */
    if (flags & MSWIN_MF_REPLACING) {
	/* Search for same event on queue. */
	for (   i = MQHead, c = MQCount;
	        c > 0;
		i = (i + 1) % MOUSE_QUEUE_LENGTH, --c) {
	    if (MQBuffer[i].event == mevent) {
		found = TRUE;
		break;
	    }
        }
    }
    if (!found) {
	if (MQCount == MOUSE_QUEUE_LENGTH)
	    return (FALSE);
        i = MQTail;
	MQTail = (MQTail + 1) % MOUSE_QUEUE_LENGTH;
	++MQCount;
    }


    /*
     * Record data.
     */
    MQBuffer[i].event = mevent;
    MQBuffer[i].button = button;
    MQBuffer[i].nRow = nRow;
    MQBuffer[i].nColumn = nColumn;
    MQBuffer[i].keys = keys;
    MQBuffer[i].flags = flags;

    /*
     * Keep record of last mouse position.
     */
    gMTEvent = MQBuffer[i];

    return (TRUE);
}





/*---------------------------------------------------------------------------
 *  BOOL  MQGet ()
 *
 *  Description:
 *		Return the next byte from the head of the queue.  If there is
 *		no byte available, returns 0, which is indistinquishable from
 *		'\0'.  So it is a good idea to call MQAvailable first.
 *
 *  Parameters:
 *		none.
 *
/*--------------------------------------------------------------------------*/

LOCAL BOOL
MQGet (MEvent * pMouse)
{
    if (MQCount == 0)
	return (FALSE);


    *pMouse = MQBuffer[MQHead];
    MQHead = (MQHead + 1) % MOUSE_QUEUE_LENGTH;
    --MQCount;
    return (TRUE);
}






DWORD
ExplainSystemErr()
{
    DWORD status;
    void *lpMsgBuf = NULL;
    status = GetLastError();

    FormatMessage(
	FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
	NULL,
	status,
	MAKELANGID(LANG_NEUTRAL,SUBLANG_DEFAULT),
	(LPTSTR) &lpMsgBuf, 0, NULL);

    if(lpMsgBuf){
	char *msg;

	msg = lptstr_to_utf8(lpMsgBuf);
	if(msg){
	    mswin_messagebox(msg, 1);
	    fs_give((void **) &msg);
	}

	LocalFree(lpMsgBuf);
    }

    return(status);
}


/*
 * Called by mswin to scroll text in window in responce to the scrollbar.
 *
 *  Args: cmd - what type of scroll operation.
 * 	scroll_pos - paramter for operation.
 *			used as position for SCROLL_TO operation.
 *
 *  Returns: TRUE - did the scroll operation.
 *	   FALSE - was not able to do the scroll operation.
 */
int
pico_scroll_callback (int cmd, long scroll_pos)
{
    switch (cmd) {
    case MSWIN_KEY_SCROLLUPLINE:
	scrollupline (0, 1);
	break;

    case MSWIN_KEY_SCROLLDOWNLINE:
        scrolldownline (0, 1);
	break;
		
    case MSWIN_KEY_SCROLLUPPAGE:
	backpage (0, 1);
	break;
	
    case MSWIN_KEY_SCROLLDOWNPAGE:
	forwpage (0, 1);
	break;
	
    case MSWIN_KEY_SCROLLTO:
	scrollto (0, 0);
	break;
    }

    update ();
    return (TRUE);
}

/*
 * sleep the given number of seconds
 */
int
sleep(int t)
{
    time_t out = (time_t)t + time((long *) 0);
    while(out > time((long *) 0))
      ;
    return(0);
}

int
tcsucmp(LPTSTR o, LPTSTR r)
{
    return(o ? (r ? _tcsicmp(o, r) : 1) : (r ? -1 : 0));
}

int
tcsruncmp(LPTSTR o, LPTSTR r, int n)
{
    return(o ? (r ? _tcsnicmp(o, r, n) : 1) : (r ? -1 : 0));
}

int
strucmp(char *o, char *r)
{
    return(o ? (r ? stricmp(o, r) : 1) : (r ? -1 : 0));
}


int
struncmp(char *o, char *r, int n)
{
    return(o ? (r ? strnicmp(o, r, n) : 1) : (r ? -1 : 0));
}


/*
 * Returns screen width of len characters of lpText.
 * The width is in character cells. That is, an ascii "A" has
 * width 1 but a CJK character probably has width 2.
 */
unsigned
scrwidth(LPTSTR lpText, int len)
{
    UCS ubuf[1000];
    unsigned w;
    int i, thislen, offset;

    offset = w = 0;

    while(len > 0){
	thislen = MIN(len, 1000);
	for(i = 0; i < thislen; i++)
	  ubuf[i] = lpText[offset+i];

	w += ucs4_str_width_ptr_to_ptr(&ubuf[0], &ubuf[thislen]);

	offset += thislen;
	len -= thislen;
    }

    return w;
}


/*
 * Returns the index into pScreen given the row,col location
 * on the screen, taking into account characters with widths
 * other than 1 cell.
 */
long
pscreen_offset_from_cord(int row, int col, PTTYINFO pTTYInfo)
{
    int offset_due_to_row, offset_due_to_col, width;

    /*
     * Each row starts at a specific offset into pScreen.
     */
    offset_due_to_row = row * pTTYInfo->actNColumn;

    /*
     * Start with col (all chars single width) and go from there.
     * We need to find the offset that makes the string col wide.
     * Hopefully we won't ever get a rectange that causes us to
     * draw half characters, but in case we do we need to err on the
     * side of too much width instead of not enough. It's a little
     * tricky because we want to include following zero-width
     * characters.
     * fewer characters it would be <= desired width, one more would
     * be greater than desired width, this one is >= desired width.
     */

    /* first go to some offset where width is > col */
    offset_due_to_col = col;
    width = scrwidth(pTTYInfo->pScreen+offset_due_to_row, offset_due_to_col);
    while(width <= col && offset_due_to_col < pTTYInfo->actNColumn-1){
	offset_due_to_col++;
	width = scrwidth(pTTYInfo->pScreen+offset_due_to_row, offset_due_to_col);
    }

    /* Now back up until width is no longer > col */
    while(width > col){
	offset_due_to_col--;
	width = scrwidth(pTTYInfo->pScreen+offset_due_to_row, offset_due_to_col);
    }

    /* But, if we're now less than col, go forward one again. */
    if(width < col && offset_due_to_col < pTTYInfo->actNColumn-1)
      offset_due_to_col++;

    return(offset_due_to_row + offset_due_to_col);
}


/*
 *  Returns:
 *   1 if store pass prompt is set in the registry to on
 *   0 if set to off
 *   -1 if not set to anything
 */
int
mswin_store_pass_prompt(void)
{
    /* 
     * We shouldn't need to prompt anymore, but always return 1 
     * just in case
     */
    return(1);
}
