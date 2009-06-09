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
typedef struct MSWIN_TEXTWINDOW MSWIN_TEXTWINDOW;
typedef void (MSWIN_TW_CALLBACK)(MSWIN_TEXTWINDOW *mswin_tw);

typedef struct MSWIN_TEXTWINDOW
{
    // User should set these fields before calling mswin_tw_create()
    UINT            id;             // Caller can use to determine which
                                    // textwindow this is (Ie: IDM_OPT_NEWMAILWIN)
    HANDLE          hInstance;
    RECT            rcSize;         // Window position

    MSWIN_TW_CALLBACK *print_callback; // Print menu selected callback routine.
    MSWIN_TW_CALLBACK *close_callback; // Callback for when window is closed.
    MSWIN_TW_CALLBACK *clear_callback; // Callback after text is cleared.

    LPCTSTR         out_file;       // Save edit contents on close to this file.
    BOOL            out_file_ret;   // TRUE - out_file written, FALSE - nope.

    // internal fields
    HWND            hwnd;           // hwnd for this mswin textwindow.
    HWND            hwnd_edit;
} MSWIN_TEXTWINDOW;

int mswin_tw_create(MSWIN_TEXTWINDOW *mswin_tw, LPCTSTR title);
void mswin_tw_close(MSWIN_TEXTWINDOW *mswin_tw);

void mswin_tw_showwindow(MSWIN_TEXTWINDOW *mswin_tw, int nCmdShow);

BOOL mswin_tw_fill_from_file(MSWIN_TEXTWINDOW *mswin_tw, LPCTSTR file);
BOOL mswin_tw_write_to_file(MSWIN_TEXTWINDOW *mswin_tw, LPCTSTR file);

void mswin_tw_setfont(MSWIN_TEXTWINDOW *mswin_tw, HFONT hfont);
void mswin_tw_setcolor(MSWIN_TEXTWINDOW *mswin_tw,
    COLORREF TextColor, COLORREF BackColor);

int mswin_tw_puts_lptstr(MSWIN_TEXTWINDOW *mswin_tw, LPTSTR msg);
int mswin_tw_printf(MSWIN_TEXTWINDOW *mswin_tw, LPCTSTR fmt, ...);

UINT mswin_tw_gettext(MSWIN_TEXTWINDOW *mswin_tw, LPTSTR lptstr_ret, int lptstr_len);
UINT mswin_tw_gettextlength(MSWIN_TEXTWINDOW *mswin_tw);

void mswin_tw_setsel(MSWIN_TEXTWINDOW *mswin_tw, LONG min, LONG max);
void mswin_tw_clear(MSWIN_TEXTWINDOW *mswin_tw);

void mswin_set_readonly(MSWIN_TEXTWINDOW *mswin_tw, BOOL read_only);
